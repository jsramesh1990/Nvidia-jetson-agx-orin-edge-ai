#pragma once
#include <string>
#include <vector>
#include <functional>
#include <atomic>
#include <thread>
#include <queue>
#include <mutex>

namespace EdgeAI {

struct AudioConfig {
    std::string device = "default";
    int sample_rate = 16000;
    int channels = 1;
    int bits_per_sample = 16;
    int buffer_size = 1024;
    int period_size = 256;
    bool use_mmap = true;
};

struct AudioFrame {
    std::vector<int16_t> samples;
    int64_t timestamp;
    int sample_rate;
    int channels;
    int frame_id;
    double rms;
    double peak;
    double energy;
};

struct AudioStats {
    int total_frames = 0;
    int dropped_frames = 0;
    double avg_rms = 0.0;
    double avg_peak = 0.0;
    double avg_energy = 0.0;
    int current_db = 0;
};

class MicrophoneDriver {
public:
    MicrophoneDriver(const AudioConfig& config);
    ~MicrophoneDriver();
    
    bool initialize();
    bool startStream();
    void stopStream();
    bool isRunning() const;
    
    // Audio capture
    AudioFrame captureFrame();
    bool captureFrameAsync(std::function<void(const AudioFrame&)> callback);
    std::vector<AudioFrame> captureFrames(int count);
    
    // Configuration
    void setSampleRate(int rate);
    void setChannels(int channels);
    void setBufferSize(int size);
    void setGain(float gain);
    void setVolume(int volume);
    
    // Status
    AudioConfig getConfig() const;
    AudioStats getStats() const;
    bool isAudioReady() const;
    int getAudioLevel() const;  // dB
    
    // Callbacks
    void setFrameCallback(std::function<void(const AudioFrame&)> callback);
    void setErrorCallback(std::function<void(const std::string&)> callback);
    void setVoiceActivityCallback(std::function<void(bool)> callback);
    
    // Utility
    bool saveRecording(const std::string& path, int duration_seconds);
    bool playAudio(const std::vector<int16_t>& samples);
    std::string getAudioInfo() const;
    bool testMicrophone();
    
private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

} // namespace EdgeAI
