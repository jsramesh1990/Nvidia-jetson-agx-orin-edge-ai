#include "microphone/voice_detector.h"
#include "microphone/audio_processor.h"
#include <cmath>
#include <algorithm>
#include <numeric>

namespace EdgeAI {

struct VoiceDetector::Impl {
    Impl() : state_(State::SILENT), frame_counter_(0) {
        // Initialize voice detection parameters
        params.silence_threshold_db = -30.0;
        params.voice_threshold_db = -20.0;
        params.min_voice_duration_ms = 300.0;
        params.max_silence_duration_ms = 1000.0;
        params.sample_rate = 16000;
    }
    
    bool detect(const std::vector<int16_t>& samples) {
        frame_counter_++;
        
        // Calculate audio metrics
        double rms = AudioProcessor::calculateRMS(samples);
        double db = 20.0 * log10(rms);
        bool is_voice = AudioProcessor::detectVoiceActivity(samples, params.sample_rate, 
                                                            params.silence_threshold_db);
        
        auto now = std::chrono::steady_clock::now();
        
        switch (state_) {
            case State::SILENT:
                if (is_voice) {
                    voice_start_time_ = now;
                    state_ = State::VOICE_STARTED;
                }
                break;
                
            case State::VOICE_STARTED:
                if (is_voice) {
                    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                        now - voice_start_time_).count();
                    if (duration >= params.min_voice_duration_ms) {
                        state_ = State::SPEAKING;
                        return true;
                    }
                } else {
                    state_ = State::SILENT;
                }
                break;
                
            case State::SPEAKING:
                if (is_voice) {
                    silence_start_time_ = now;
                } else {
                    auto silence_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                        now - silence_start_time_).count();
                    if (silence_duration >= params.max_silence_duration_ms) {
                        state_ = State::SILENT;
                        return false;
                    }
                }
                return true;
        }
        
        return false;
    }
    
    bool isSpeaking() const {
        return state_ == State::SPEAKING || state_ == State::VOICE_STARTED;
    }
    
    void reset() {
        state_ = State::SILENT;
        frame_counter_ = 0;
    }
    
    VoiceDetectionParams getParams() const {
        return params;
    }
    
    void setParams(const VoiceDetectionParams& new_params) {
        params = new_params;
    }
    
private:
    enum class State {
        SILENT,
        VOICE_STARTED,
        SPEAKING
    };
    
    State state_;
    VoiceDetectionParams params;
    std::chrono::steady_clock::time_point voice_start_time_;
    std::chrono::steady_clock::time_point silence_start_time_;
    int frame_counter_;
};

VoiceDetector::VoiceDetector() : pImpl(std::make_unique<Impl>()) {}
VoiceDetector::~VoiceDetector() = default;

bool VoiceDetector::detect(const std::vector<int16_t>& samples) {
    return pImpl->detect(samples);
}

bool VoiceDetector::isSpeaking() const {
    return pImpl->isSpeaking();
}

void VoiceDetector::reset() {
    pImpl->reset();
}

VoiceDetectionParams VoiceDetector::getParams() const {
    return pImpl->getParams();
}

void VoiceDetector::setParams(const VoiceDetectionParams& params) {
    pImpl->setParams(params);
}

} // namespace EdgeAI
