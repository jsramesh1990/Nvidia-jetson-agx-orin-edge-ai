#include "microphone/audio_processor.h"
#include <cmath>
#include <complex>
#include <numeric>
#include <algorithm>
#include <cstring>

namespace EdgeAI {

// RMS and energy calculations
double AudioProcessor::calculateRMS(const std::vector<int16_t>& samples) {
    if (samples.empty()) return 0.0;
    
    double sum = 0.0;
    for (int16_t sample : samples) {
        double normalized = static_cast<double>(sample) / 32768.0;
        sum += normalized * normalized;
    }
    return sqrt(sum / samples.size());
}

double AudioProcessor::calculatePeak(const std::vector<int16_t>& samples) {
    if (samples.empty()) return 0.0;
    
    double peak = 0.0;
    for (int16_t sample : samples) {
        double normalized = fabs(static_cast<double>(sample) / 32768.0);
        if (normalized > peak) {
            peak = normalized;
        }
    }
    return peak;
}

double AudioProcessor::calculateEnergy(const std::vector<int16_t>& samples) {
    if (samples.empty()) return 0.0;
    
    double sum = 0.0;
    for (int16_t sample : samples) {
        double normalized = static_cast<double>(sample) / 32768.0;
        sum += normalized * normalized;
    }
    return sum / samples.size();
}

double AudioProcessor::calculateDB(const std::vector<int16_t>& samples) {
    double rms = calculateRMS(samples);
    if (rms < 1e-10) return -100.0;
    return 20.0 * log10(rms);
}

// Filtering implementations
std::vector<int16_t> AudioProcessor::applyLowPass(const std::vector<int16_t>& samples,
                                                   int sample_rate, double cutoff_freq) {
    if (samples.empty()) return samples;
    
    // Simple IIR low-pass filter
    double dt = 1.0 / sample_rate;
    double RC = 1.0 / (2.0 * M_PI * cutoff_freq);
    double alpha = dt / (RC + dt);
    
    std::vector<int16_t> filtered;
    filtered.reserve(samples.size());
    
    double prev = 0.0;
    for (int16_t sample : samples) {
        double current = alpha * sample + (1.0 - alpha) * prev;
        filtered.push_back(static_cast<int16_t>(current));
        prev = current;
    }
    
    return filtered;
}

std::vector<int16_t> AudioProcessor::applyHighPass(const std::vector<int16_t>& samples,
                                                    int sample_rate, double cutoff_freq) {
    if (samples.empty()) return samples;
    
    // Simple IIR high-pass filter
    double dt = 1.0 / sample_rate;
    double RC = 1.0 / (2.0 * M_PI * cutoff_freq);
    double alpha = RC / (RC + dt);
    
    std::vector<int16_t> filtered;
    filtered.reserve(samples.size());
    
    double prev = samples[0];
    for (int16_t sample : samples) {
        double current = alpha * (prev + sample - prev);
        filtered.push_back(static_cast<int16_t>(current));
        prev = sample;
    }
    
    return filtered;
}

std::vector<int16_t> AudioProcessor::applyBandPass(const std::vector<int16_t>& samples,
                                                    int sample_rate, double low_cutoff,
                                                    double high_cutoff) {
    auto low_passed = applyLowPass(samples, sample_rate, high_cutoff);
    return applyHighPass(low_passed, sample_rate, low_cutoff);
}

std::vector<int16_t> AudioProcessor::applyNotch(const std::vector<int16_t>& samples,
                                                 int sample_rate, double freq) {
    if (samples.empty()) return samples;
    
    // Notch filter using IIR
    double Q = 1.0; // Quality factor
    double omega = 2.0 * M_PI * freq / sample_rate;
    double sin_omega = sin(omega);
    double cos_omega = cos(omega);
    double alpha = sin_omega / (2.0 * Q);
    
    double b0 = 1.0;
    double b1 = -2.0 * cos_omega;
    double b2 = 1.0;
    double a0 = 1.0 + alpha;
    double a1 = -2.0 * cos_omega;
    double a2 = 1.0 - alpha;
    
    std::vector<int16_t> filtered;
    filtered.reserve(samples.size());
    
    double x1 = 0, x2 = 0, y1 = 0, y2 = 0;
    
    for (int16_t sample : samples) {
        double x = sample;
        double y = (b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2) / a0;
        filtered.push_back(static_cast<int16_t>(y));
        x2 = x1;
        x1 = x;
        y2 = y1;
        y1 = y;
    }
    
    return filtered;
}

// Noise reduction
std::vector<int16_t> AudioProcessor::noiseGate(const std::vector<int16_t>& samples,
                                                double threshold_db) {
    if (samples.empty()) return samples;
    
    double threshold_linear = pow(10.0, threshold_db / 20.0);
    double current_rms = calculateRMS(samples);
    
    if (current_rms < threshold_linear) {
        // Too quiet, mute
        return std::vector<int16_t>(samples.size(), 0);
    }
    
    // Apply soft gating
    double gain = 1.0;
    if (current_rms < threshold_linear * 2.0) {
        gain = (current_rms - threshold_linear) / threshold_linear;
    }
    
    std::vector<int16_t> filtered;
    filtered.reserve(samples.size());
    for (int16_t sample : samples) {
        filtered.push_back(static_cast<int16_t>(sample * gain));
    }
    
    return filtered;
}

std::vector<int16_t> AudioProcessor::spectralSubtraction(const std::vector<int16_t>& samples,
                                                          int sample_rate) {
    // Placeholder for spectral subtraction
    // This would require FFT implementation
    return samples;
}

// Voice activity detection
bool AudioProcessor::detectVoiceActivity(const std::vector<int16_t>& samples,
                                          int sample_rate, double threshold_db) {
    if (samples.empty()) return false;
    
    double rms = calculateRMS(samples);
    double db = 20.0 * log10(rms);
    
    // Check energy above threshold
    if (db < threshold_db) return false;
    
    // Additional features for better detection
    // Check zero-crossing rate
    int zero_crossings = 0;
    for (size_t i = 1; i < samples.size(); i++) {
        if ((samples[i] > 0 && samples[i-1] < 0) ||
            (samples[i] < 0 && samples[i-1] > 0)) {
            zero_crossings++;
        }
    }
    
    double zcr = static_cast<double>(zero_crossings) / samples.size();
    
    // Voice typically has moderate zero-crossing rate
    // Music typically has lower zcr
    if (zcr < 0.01 || zcr > 0.2) return false;
    
    return true;
}

// Speech processing
std::vector<int16_t> AudioProcessor::preEmphasis(const std::vector<int16_t>& samples,
                                                  double coefficient) {
    if (samples.empty()) return samples;
    
    std::vector<int16_t> filtered;
    filtered.reserve(samples.size());
    
    filtered.push_back(samples[0]);
    for (size_t i = 1; i < samples.size(); i++) {
        double val = samples[i] - coefficient * samples[i-1];
        filtered.push_back(static_cast<int16_t>(val));
    }
    
    return filtered;
}

std::vector<int16_t> AudioProcessor::normalizeVolume(const std::vector<int16_t>& samples,
                                                      double target_db) {
    if (samples.empty()) return samples;
    
    double current_rms = calculateRMS(samples);
    if (current_rms < 1e-10) return samples;
    
    double current_db = 20.0 * log10(current_rms);
    double gain = pow(10.0, (target_db - current_db) / 20.0);
    
    std::vector<int16_t> normalized;
    normalized.reserve(samples.size());
    
    for (int16_t sample : samples) {
        double val = sample * gain;
        // Clip to prevent saturation
        if (val > 32767.0) val = 32767.0;
        if (val < -32768.0) val = -32768.0;
        normalized.push_back(static_cast<int16_t>(val));
    }
    
    return normalized;
}

std::vector<int16_t> AudioProcessor::convertToMono(const std::vector<int16_t>& samples,
                                                    int channels) {
    if (channels == 1) return samples;
    
    std::vector<int16_t> mono;
    mono.reserve(samples.size() / channels);
    
    for (size_t i = 0; i < samples.size(); i += channels) {
        int32_t sum = 0;
        for (int c = 0; c < channels; c++) {
            if (i + c < samples.size()) {
                sum += samples[i + c];
            }
        }
        mono.push_back(static_cast<int16_t>(sum / channels));
    }
    
    return mono;
}

// Feature extraction
std::vector<double> AudioProcessor::computeMFCC(const std::vector<int16_t>& samples,
                                                 int sample_rate, int num_coeffs) {
    // Placeholder - would require FFT and Mel filterbank
    std::vector<double> mfcc(num_coeffs, 0.0);
    return mfcc;
}

std::vector<double> AudioProcessor::computeSpectralCentroid(const std::vector<int16_t>& samples,
                                                             int sample_rate) {
    // Placeholder
    std::vector<double> centroid;
    return centroid;
}

std::vector<double> AudioProcessor::computeSpectralRolloff(const std::vector<int16_t>& samples,
                                                            int sample_rate, double ratio) {
    // Placeholder
    std::vector<double> rolloff;
    return rolloff;
}

// Resampling
std::vector<int16_t> AudioProcessor::resample(const std::vector<int16_t>& samples,
                                               int original_rate, int target_rate) {
    if (original_rate == target_rate) return samples;
    
    // Simple linear interpolation
    double ratio = static_cast<double>(target_rate) / original_rate;
    size_t new_size = static_cast<size_t>(samples.size() * ratio);
    
    std::vector<int16_t> resampled;
    resampled.reserve(new_size);
    
    for (size_t i = 0; i < new_size; i++) {
        double position = i / ratio;
        size_t index = static_cast<size_t>(position);
        double frac = position - index;
        
        if (index >= samples.size() - 1) {
            resampled.push_back(samples.back());
        } else {
            double val = samples[index] * (1.0 - frac) + samples[index + 1] * frac;
            resampled.push_back(static_cast<int16_t>(val));
        }
    }
    
    return resampled;
}

// Utility
void AudioProcessor::printAudioInfo(const std::vector<int16_t>& samples, int sample_rate) {
    std::cout << "Audio Info:" << std::endl;
    std::cout << "  Samples: " << samples.size() << std::endl;
    std::cout << "  Duration: " << samples.size() / static_cast<double>(sample_rate) << "s" << std::endl;
    std::cout << "  Sample Rate: " << sample_rate << " Hz" << std::endl;
    std::cout << "  RMS: " << calculateRMS(samples) << std::endl;
    std::cout << "  Peak: " << calculatePeak(samples) << std::endl;
    std::cout << "  Energy: " << calculateEnergy(samples) << std::endl;
    std::cout << "  dB: " << calculateDB(samples) << " dB" << std::endl;
}

} // namespace EdgeAI
