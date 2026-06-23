#pragma once
#include <vector>
#include <cstdint>
#include <complex>

namespace EdgeAI {

class AudioProcessor {
public:
    // RMS and energy calculations
    static double calculateRMS(const std::vector<int16_t>& samples);
    static double calculatePeak(const std::vector<int16_t>& samples);
    static double calculateEnergy(const std::vector<int16_t>& samples);
    static double calculateDB(const std::vector<int16_t>& samples);
    
    // Filtering
    static std::vector<int16_t> applyLowPass(const std::vector<int16_t>& samples,
                                             int sample_rate, double cutoff_freq);
    static std::vector<int16_t> applyHighPass(const std::vector<int16_t>& samples,
                                              int sample_rate, double cutoff_freq);
    static std::vector<int16_t> applyBandPass(const std::vector<int16_t>& samples,
                                              int sample_rate, double low_cutoff,
                                              double high_cutoff);
    static std::vector<int16_t> applyNotch(const std::vector<int16_t>& samples,
                                           int sample_rate, double freq);
    
    // Noise reduction
    static std::vector<int16_t> noiseGate(const std::vector<int16_t>& samples,
                                          double threshold_db);
    static std::vector<int16_t> spectralSubtraction(const std::vector<int16_t>& samples,
                                                    int sample_rate);
    
    // Voice activity detection
    static bool detectVoiceActivity(const std::vector<int16_t>& samples,
                                    int sample_rate, double threshold_db = -20.0);
    
    // Speech processing
    static std::vector<int16_t> preEmphasis(const std::vector<int16_t>& samples,
                                            double coefficient = 0.97);
    static std::vector<int16_t> normalizeVolume(const std::vector<int16_t>& samples,
                                                double target_db);
    static std::vector<int16_t> convertToMono(const std::vector<int16_t>& samples,
                                              int channels);
    
    // Feature extraction
    static std::vector<double> computeMFCC(const std::vector<int16_t>& samples,
                                           int sample_rate, int num_coeffs = 13);
    static std::vector<double> computeSpectralCentroid(const std::vector<int16_t>& samples,
                                                       int sample_rate);
    static std::vector<double> computeSpectralRolloff(const std::vector<int16_t>& samples,
                                                      int sample_rate, double ratio = 0.85);
    
    // Resampling
    static std::vector<int16_t> resample(const std::vector<int16_t>& samples,
                                         int original_rate, int target_rate);
    
    // Utility
    static void printAudioInfo(const std::vector<int16_t>& samples, int sample_rate);
};

} // namespace EdgeAI
