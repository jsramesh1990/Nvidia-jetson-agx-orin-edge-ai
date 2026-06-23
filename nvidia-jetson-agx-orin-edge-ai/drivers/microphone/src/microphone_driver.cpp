#include "microphone/microphone_driver.h"
#include "microphone/audio_processor.h"
#include <alsa/asoundlib.h>
#include <cmath>
#include <cstring>
#include <algorithm>
#include <chrono>

namespace EdgeAI {

class MicrophoneDriver::Impl {
public:
    Impl(const AudioConfig& config) 
        : config_(config), running_(false), frame_counter_(0), 
          dropped_counter_(0), handle_(nullptr) {
        // Initialize ALSA
        initializeALSA();
    }
    
    ~Impl() {
        stopStream();
        if (handle_) {
            snd_pcm_close(handle_);
            handle_ = nullptr;
        }
    }
    
    bool initialize() {
        if (handle_) {
            return true;
        }
        
        // Open PCM device
        int err = snd_pcm_open(&handle_, config_.device.c_str(), 
                               SND_PCM_STREAM_CAPTURE, 0);
        if (err < 0) {
            setError("Failed to open PCM device: " + std::string(snd_strerror(err)));
            return false;
        }
        
        // Set hardware parameters
        if (!setHardwareParams()) {
            return false;
        }
        
        // Set software parameters
        if (!setSoftwareParams()) {
            return false;
        }
        
        // Prepare device
        err = snd_pcm_prepare(handle_);
        if (err < 0) {
            setError("Failed to prepare PCM device: " + std::string(snd_strerror(err)));
            return false;
        }
        
        return true;
    }
    
    bool startStream() {
        if (running_) {
            return true;
        }
        
        if (!handle_) {
            if (!initialize()) {
                return false;
            }
        }
        
        // Start capture
        int err = snd_pcm_start(handle_);
        if (err < 0) {
            setError("Failed to start capture: " + std::string(snd_strerror(err)));
            return false;
        }
        
        running_ = true;
        
        // Start capture thread if callback is set
        if (frame_callback_ || voice_activity_callback_) {
            capture_thread_ = std::thread(&Impl::captureLoop, this);
        }
        
        return true;
    }
    
    void stopStream() {
        if (!running_) {
            return;
        }
        
        running_ = false;
        
        if (capture_thread_.joinable()) {
            capture_thread_.join();
        }
        
        if (handle_) {
            snd_pcm_drop(handle_);
            snd_pcm_prepare(handle_);
        }
    }
    
    bool isRunning() const {
        return running_;
    }
    
    AudioFrame captureFrame() {
        if (!running_ || !handle_) {
            return AudioFrame();
        }
        
        AudioFrame frame;
        frame.samples.resize(config_.buffer_size);
        frame.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        frame.sample_rate = config_.sample_rate;
        frame.channels = config_.channels;
        frame.frame_id = ++frame_counter_;
        
        // Read samples
        snd_pcm_sframes_t frames_read = snd_pcm_readi(
            handle_, 
            frame.samples.data(), 
            config_.buffer_size / config_.channels
        );
        
        if (frames_read < 0) {
            if (frames_read == -EPIPE) {
                // Overrun, recover
                snd_pcm_prepare(handle_);
                dropped_counter_++;
                return AudioFrame();
            }
            dropped_counter_++;
            return AudioFrame();
        }
        
        // Adjust size if less than expected
        if (frames_read < static_cast<snd_pcm_sframes_t>(config_.buffer_size / config_.channels)) {
            frame.samples.resize(frames_read * config_.channels);
        }
        
        // Calculate audio metrics
        frame.rms = AudioProcessor::calculateRMS(frame.samples);
        frame.peak = AudioProcessor::calculatePeak(frame.samples);
        frame.energy = AudioProcessor::calculateEnergy(frame.samples);
        
        // Update stats
        updateStats(frame);
        
        // Voice activity detection
        if (voice_activity_callback_) {
            bool is_voice = AudioProcessor::detectVoiceActivity(
                frame.samples, config_.sample_rate, -20.0);
            voice_activity_callback_(is_voice);
        }
        
        return frame;
    }
    
    bool captureFrameAsync(std::function<void(const AudioFrame&)> callback) {
        if (!running_) {
            return false;
        }
        
        std::thread([this, callback]() {
            auto frame = captureFrame();
            if (callback && !frame.samples.empty()) {
                callback(frame);
            }
        }).detach();
        
        return true;
    }
    
    std::vector<AudioFrame> captureFrames(int count) {
        std::vector<AudioFrame> frames;
        frames.reserve(count);
        
        for (int i = 0; i < count && running_; i++) {
            auto frame = captureFrame();
            if (!frame.samples.empty()) {
                frames.push_back(frame);
            }
        }
        
        return frames;
    }
    
    void setSampleRate(int rate) {
        config_.sample_rate = rate;
        if (handle_) {
            setHardwareParams();
        }
    }
    
    void setChannels(int channels) {
        config_.channels = channels;
        if (handle_) {
            setHardwareParams();
        }
    }
    
    void setBufferSize(int size) {
        config_.buffer_size = size;
    }
    
    void setGain(float gain) {
        config_.gain = gain;
    }
    
    void setVolume(int volume) {
        config_.volume = volume;
    }
    
    AudioConfig getConfig() const {
        return config_;
    }
    
    AudioStats getStats() const {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        return stats_;
    }
    
    bool isAudioReady() const {
        return frame_counter_ > 0;
    }
    
    int getAudioLevel() const {
        if (current_db_ <= 0) {
            return 0;
        }
        // Convert dB to percentage (0-100)
        int level = static_cast<int>((current_db_ + 100) / 100.0 * 100);
        return std::max(0, std::min(100, level));
    }
    
    void setFrameCallback(std::function<void(const AudioFrame&)> callback) {
        frame_callback_ = callback;
        if (callback && !capture_thread_.joinable()) {
            capture_thread_ = std::thread(&Impl::captureLoop, this);
        }
    }
    
    void setErrorCallback(std::function<void(const std::string&)> callback) {
        error_callback_ = callback;
    }
    
    void setVoiceActivityCallback(std::function<void(bool)> callback) {
        voice_activity_callback_ = callback;
    }
    
    bool saveRecording(const std::string& path, int duration_seconds) {
        if (!running_) {
            return false;
        }
        
        std::vector<AudioFrame> frames;
        auto start_time = std::chrono::steady_clock::now();
        auto end_time = start_time + std::chrono::seconds(duration_seconds);
        
        while (std::chrono::steady_clock::now() < end_time && running_) {
            auto frame = captureFrame();
            if (!frame.samples.empty()) {
                frames.push_back(frame);
            }
        }
        
        if (frames.empty()) {
            return false;
        }
        
        // Save as WAV file
        return saveWAV(path, frames);
    }
    
    bool playAudio(const std::vector<int16_t>& samples) {
        // Play audio using ALSA
        snd_pcm_t* play_handle = nullptr;
        int err = snd_pcm_open(&play_handle, "default", 
                               SND_PCM_STREAM_PLAYBACK, 0);
        if (err < 0) {
            setError("Failed to open playback device: " + std::string(snd_strerror(err)));
            return false;
        }
        
        // Set parameters
        snd_pcm_hw_params_t* params;
        snd_pcm_hw_params_alloca(&params);
        snd_pcm_hw_params_any(play_handle, params);
        snd_pcm_hw_params_set_access(play_handle, params, 
                                     SND_PCM_ACCESS_RW_INTERLEAVED);
        snd_pcm_hw_params_set_format(play_handle, params, 
                                     SND_PCM_FORMAT_S16_LE);
        snd_pcm_hw_params_set_rate_near(play_handle, params, 
                                        &config_.sample_rate, 0);
        snd_pcm_hw_params_set_channels(play_handle, params, 
                                       config_.channels);
        snd_pcm_hw_params(play_handle, params);
        
        // Write samples
        snd_pcm_sframes_t frames_written = snd_pcm_writei(
            play_handle,
            samples.data(),
            samples.size() / config_.channels
        );
        
        snd_pcm_drain(play_handle);
        snd_pcm_close(play_handle);
        
        return frames_written > 0;
    }
    
    std::string getAudioInfo() const {
        std::stringstream ss;
        ss << "Device: " << config_.device << "\n";
        ss << "Sample Rate: " << config_.sample_rate << " Hz\n";
        ss << "Channels: " << config_.channels << "\n";
        ss << "Bits per sample: " << config_.bits_per_sample << "\n";
        ss << "Buffer Size: " << config_.buffer_size << "\n";
        ss << "Running: " << (running_ ? "Yes" : "No") << "\n";
        ss << "Total Frames: " << frame_counter_ << "\n";
        ss << "Dropped Frames: " << dropped_counter_ << "\n";
        return ss.str();
    }
    
    bool testMicrophone() {
        auto frame = captureFrame();
        return !frame.samples.empty();
    }
    
private:
    AudioConfig config_;
    snd_pcm_t* handle_;
    std::atomic<bool> running_;
    std::atomic<int> frame_counter_;
    std::atomic<int> dropped_counter_;
    std::thread capture_thread_;
    AudioStats stats_;
    std::mutex stats_mutex_;
    std::function<void(const AudioFrame&)> frame_callback_;
    std::function<void(const std::string&)> error_callback_;
    std::function<void(bool)> voice_activity_callback_;
    int current_db_ = 0;
    
    void initializeALSA() {
        // ALSA is initialized when opening device
    }
    
    bool setHardwareParams() {
        snd_pcm_hw_params_t* params;
        snd_pcm_hw_params_alloca(&params);
        
        int err = snd_pcm_hw_params_any(handle_, params);
        if (err < 0) {
            setError("Failed to get hardware params: " + std::string(snd_strerror(err)));
            return false;
        }
        
        // Set access type
        err = snd_pcm_hw_params_set_access(handle_, params, 
                                           SND_PCM_ACCESS_RW_INTERLEAVED);
        if (err < 0) {
            setError("Failed to set access: " + std::string(snd_strerror(err)));
            return false;
        }
        
        // Set format
        snd_pcm_format_t format;
        switch (config_.bits_per_sample) {
            case 8: format = SND_PCM_FORMAT_S8; break;
            case 16: format = SND_PCM_FORMAT_S16_LE; break;
            case 24: format = SND_PCM_FORMAT_S24_LE; break;
            case 32: format = SND_PCM_FORMAT_S32_LE; break;
            default: format = SND_PCM_FORMAT_S16_LE; break;
        }
        err = snd_pcm_hw_params_set_format(handle_, params, format);
        if (err < 0) {
            setError("Failed to set format: " + std::string(snd_strerror(err)));
            return false;
        }
        
        // Set sample rate
        unsigned int rate = config_.sample_rate;
        err = snd_pcm_hw_params_set_rate_near(handle_, params, &rate, 0);
        if (err < 0) {
            setError("Failed to set sample rate: " + std::string(snd_strerror(err)));
            return false;
        }
        config_.sample_rate = rate;
        
        // Set channels
        err = snd_pcm_hw_params_set_channels(handle_, params, config_.channels);
        if (err < 0) {
            setError("Failed to set channels: " + std::string(snd_strerror(err)));
            return false;
        }
        
        // Set buffer size
        snd_pcm_uframes_t buffer_size = config_.buffer_size;
        err = snd_pcm_hw_params_set_buffer_size_near(handle_, params, &buffer_size);
        if (err < 0) {
            setError("Failed to set buffer size: " + std::string(snd_strerror(err)));
            return false;
        }
        config_.buffer_size = buffer_size;
        
        // Set period size
        snd_pcm_uframes_t period_size = config_.period_size;
        err = snd_pcm_hw_params_set_period_size_near(handle_, params, &period_size, 0);
        if (err < 0) {
            setError("Failed to set period size: " + std::string(snd_strerror(err)));
            return false;
        }
        config_.period_size = period_size;
        
        // Apply parameters
        err = snd_pcm_hw_params(handle_, params);
        if (err < 0) {
            setError("Failed to apply hardware params: " + std::string(snd_strerror(err)));
            return false;
        }
        
        return true;
    }
    
    bool setSoftwareParams() {
        snd_pcm_sw_params_t* params;
        snd_pcm_sw_params_alloca(&params);
        
        int err = snd_pcm_sw_params_current(handle_, params);
        if (err < 0) {
            setError("Failed to get software params: " + std::string(snd_strerror(err)));
            return false;
        }
        
        // Set start threshold
        err = snd_pcm_sw_params_set_start_threshold(handle_, params, 
                                                    0x7fffffff);
        if (err < 0) {
            setError("Failed to set start threshold: " + std::string(snd_strerror(err)));
            return false;
        }
        
        // Set avail min
        err = snd_pcm_sw_params_set_avail_min(handle_, params, 
                                              config_.period_size);
        if (err < 0) {
            setError("Failed to set avail min: " + std::string(snd_strerror(err)));
            return false;
        }
        
        // Apply parameters
        err = snd_pcm_sw_params(handle_, params);
        if (err < 0) {
            setError("Failed to apply software params: " + std::string(snd_strerror(err)));
            return false;
        }
        
        return true;
    }
    
    void updateStats(const AudioFrame& frame) {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_.total_frames++;
        stats_.avg_rms = (stats_.avg_rms * (stats_.total_frames - 1) + frame.rms) / 
                         stats_.total_frames;
        stats_.avg_peak = (stats_.avg_peak * (stats_.total_frames - 1) + frame.peak) / 
                          stats_.total_frames;
        stats_.avg_energy = (stats_.avg_energy * (stats_.total_frames - 1) + frame.energy) / 
                            stats_.total_frames;
        
        // Calculate current dB
        if (frame.rms > 0) {
            current_db_ = static_cast<int>(20.0 * log10(frame.rms / 32768.0));
        } else {
            current_db_ = -100;
        }
        stats_.current_db = current_db_;
    }
    
    bool saveWAV(const std::string& path, const std::vector<AudioFrame>& frames) {
        if (frames.empty()) {
            return false;
        }
        
        // Calculate total samples
        size_t total_samples = 0;
        for (const auto& frame : frames) {
            total_samples += frame.samples.size();
        }
        
        // WAV header
        struct WAVHeader {
            char riff[4] = {'R', 'I', 'F', 'F'};
            uint32_t file_size;
            char wave[4] = {'W', 'A', 'V', 'E'};
            char fmt[4] = {'f', 'm', 't', ' '};
            uint32_t fmt_size = 16;
            uint16_t audio_format = 1;
            uint16_t num_channels;
            uint32_t sample_rate;
            uint32_t byte_rate;
            uint16_t block_align;
            uint16_t bits_per_sample;
            char data[4] = {'d', 'a', 't', 'a'};
            uint32_t data_size;
        } header;
        
        header.num_channels = config_.channels;
        header.sample_rate = config_.sample_rate;
        header.bits_per_sample = config_.bits_per_sample;
        header.block_align = config_.channels * config_.bits_per_sample / 8;
        header.byte_rate = header.sample_rate * header.block_align;
        header.data_size = total_samples * sizeof(int16_t);
        header.file_size = 36 + header.data_size;
        
        // Write to file
        FILE* file = fopen(path.c_str(), "wb");
        if (!file) {
            return false;
        }
        
        fwrite(&header, sizeof(header), 1, file);
        
        // Write samples
        for (const auto& frame : frames) {
            fwrite(frame.samples.data(), sizeof(int16_t), 
                   frame.samples.size(), file);
        }
        
        fclose(file);
        return true;
    }
    
    void captureLoop() {
        while (running_) {
            auto frame = captureFrame();
            
            if (!frame.samples.empty()) {
                if (frame_callback_) {
                    frame_callback_(frame);
                }
            }
            
            // Don't consume all CPU
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
    
    void setError(const std::string& error) {
        if (error_callback_) {
            error_callback_(error);
        }
        std::cerr << "Microphone Error: " << error << std::endl;
    }
};

// MicrophoneDriver implementation
MicrophoneDriver::MicrophoneDriver(const AudioConfig& config) 
    : pImpl(std::make_unique<Impl>(config)) {}

MicrophoneDriver::~MicrophoneDriver() = default;

bool MicrophoneDriver::initialize() { return pImpl->initialize(); }
bool MicrophoneDriver::startStream() { return pImpl->startStream(); }
void MicrophoneDriver::stopStream() { pImpl->stopStream(); }
bool MicrophoneDriver::isRunning() const { return pImpl->isRunning(); }

AudioFrame MicrophoneDriver::captureFrame() { return pImpl->captureFrame(); }
bool MicrophoneDriver::captureFrameAsync(std::function<void(const AudioFrame&)> callback) {
    return pImpl->captureFrameAsync(callback);
}
std::vector<AudioFrame> MicrophoneDriver::captureFrames(int count) {
    return pImpl->captureFrames(count);
}

void MicrophoneDriver::setSampleRate(int rate) { pImpl->setSampleRate(rate); }
void MicrophoneDriver::setChannels(int channels) { pImpl->setChannels(channels); }
void MicrophoneDriver::setBufferSize(int size) { pImpl->setBufferSize(size); }
void MicrophoneDriver::setGain(float gain) { pImpl->setGain(gain); }
void MicrophoneDriver::setVolume(int volume) { pImpl->setVolume(volume); }

AudioConfig MicrophoneDriver::getConfig() const { return pImpl->getConfig(); }
AudioStats MicrophoneDriver::getStats() const { return pImpl->getStats(); }
bool MicrophoneDriver::isAudioReady() const { return pImpl->isAudioReady(); }
int MicrophoneDriver::getAudioLevel() const { return pImpl->getAudioLevel(); }

void MicrophoneDriver::setFrameCallback(std::function<void(const AudioFrame&)> callback) {
    pImpl->setFrameCallback(callback);
}
void MicrophoneDriver::setErrorCallback(std::function<void(const std::string&)> callback) {
    pImpl->setErrorCallback(callback);
}
void MicrophoneDriver::setVoiceActivityCallback(std::function<void(bool)> callback) {
    pImpl->setVoiceActivityCallback(callback);
}

bool MicrophoneDriver::saveRecording(const std::string& path, int duration_seconds) {
    return pImpl->saveRecording(path, duration_seconds);
}
bool MicrophoneDriver::playAudio(const std::vector<int16_t>& samples) {
    return pImpl->playAudio(samples);
}
std::string MicrophoneDriver::getAudioInfo() const { return pImpl->getAudioInfo(); }
bool MicrophoneDriver::testMicrophone() { return pImpl->testMicrophone(); }

} // namespace EdgeAI
