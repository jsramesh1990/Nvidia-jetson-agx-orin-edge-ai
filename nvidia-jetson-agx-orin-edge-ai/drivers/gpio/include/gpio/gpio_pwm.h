#pragma once
#include <string>
#include <vector>
#include <functional>
#include <atomic>
#include <thread>

namespace EdgeAI {

struct PWMChannel {
    int pin;
    int period_ns;
    int duty_ns;
    bool enabled;
    double frequency_hz;
    double duty_cycle;
};

class GPIOPWM {
public:
    GPIOPWM();
    ~GPIOPWM();
    
    bool setupPWM(int pin, int period_ns, int duty_ns);
    bool enablePWM(int pin);
    bool disablePWM(int pin);
    bool setDutyCycle(int pin, int duty_ns);
    bool setFrequency(int pin, int frequency_hz);
    bool setDutyPercentage(int pin, double percentage);
    bool isEnabled(int pin) const;
    PWMChannel getConfig(int pin) const;
    std::vector<int> getActivePins() const;
    void cleanup();
    
    // Advanced PWM features
    bool rampPWM(int pin, int target_duty_ns, int duration_ms);
    bool sweepPWM(int pin, int start_duty, int end_duty, int duration_ms);
    bool generatePulseTrain(int pin, int pulse_count, int period_ms);
    
private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

} // namespace EdgeAI
