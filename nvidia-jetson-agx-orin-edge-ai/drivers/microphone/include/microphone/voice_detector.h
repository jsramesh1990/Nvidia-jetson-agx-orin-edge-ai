#pragma once
#include <vector>
#include <cstdint>
#include <memory>
#include <chrono>

namespace EdgeAI {

struct VoiceDetectionParams {
    double silence_threshold_db = -30.0;      // Threshold for silence detection
    double voice_threshold_db = -20.0;         // Threshold for voice detection
    double min_voice_duration_ms = 300.0;      // Minimum voice duration to trigger
    double max_silence_duration_ms = 1000.0;   // Maximum silence before ending voice
    int sample_rate = 16000;                   // Sample rate in Hz
    int frame_size_ms = 30;                    // Frame size in milliseconds
    bool use_energy_detection = true;          // Use energy-based detection
    bool use_zero_crossing = true;             // Use zero-crossing rate
    bool use_spectral_features = false;        // Use spectral features
    double noise_floor_db = -50.0;             // Noise floor level
    double snr_threshold_db = 15.0;            // SNR threshold for voice detection
};

struct VoiceDetectionResult {
    bool is_voice = false;
    double confidence = 0.0;
    double rms_db = 0.0;
    double snr_db = 0.0;
    double zero_crossing_rate = 0.0;
    double spectral_centroid = 0.0;
    double pitch_hz = 0.0;
    int64_t timestamp = 0;
    int frame_id = 0;
    std::string detected_phrase = "";
};

class VoiceDetector {
public:
    VoiceDetector();
    ~VoiceDetector();
    
    // Main detection
    bool detect(const std::vector<int16_t>& samples);
    bool detectWithResult(const std::vector<int16_t>& samples, 
                          VoiceDetectionResult& result);
    bool isSpeaking() const;
    void reset();
    
    // Configuration
    VoiceDetectionParams getParams() const;
    void setParams(const VoiceDetectionParams& params);
    
    // Advanced features
    void setNoiseProfile(const std::vector<int16_t>& noise_samples);
    bool updateNoiseProfile(const std::vector<int16_t>& samples);
    void clearNoiseProfile();
    
    // Phrase detection
    void setPhraseDetection(bool enable);
    void addPhraseTemplate(const std::string& phrase, 
                           const std::vector<double>& features);
    std::string detectPhrase(const std::vector<int16_t>& samples);
    
    // Statistics
    struct DetectionStats {
        int total_frames = 0;
        int voice_frames = 0;
        int false_positives = 0;
        int false_negatives = 0;
        double avg_confidence = 0.0;
        double detection_rate = 0.0;
    };
    DetectionStats getStats() const;
    void resetStats();
    
    // Callbacks
    void setVoiceStartCallback(std::function<void()> callback);
    void setVoiceEndCallback(std::function<void()> callback);
    void setPhraseDetectedCallback(std::function<void(const std::string&)> callback);
    void setErrorCallback(std::function<void(const std::string&)> callback);
    
    // Utility
    bool isCalibrated() const;
    bool calibrate(int duration_seconds = 3);
    void setDebugMode(bool enabled);
    std::string getStatus() const;
    
private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

} // namespace EdgeAI
