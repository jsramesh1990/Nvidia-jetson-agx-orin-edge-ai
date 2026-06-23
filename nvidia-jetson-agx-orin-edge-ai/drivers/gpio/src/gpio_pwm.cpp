#include "gpio/gpio_pwm.h"
#include <fstream>
#include <sstream>
#include <cmath>
#include <chrono>

namespace EdgeAI {

class GPIOPWM::Impl {
public:
    Impl() {
        // Check if PWM is available
        checkPWMAvailability();
    }
    
    ~Impl() {
        cleanup();
    }
    
    bool setupPWM(int pin, int period_ns, int duty_ns) {
        if (!pwm_available_) return false;
        
        std::string pwm_path = getPWMPath(pin);
        if (!createPWMChannel(pin)) return false;
        
        // Set period
        if (!writeToFile(pwm_path + "/period", std::to_string(period_ns))) {
            return false;
        }
        
        // Set duty cycle
        if (!writeToFile(pwm_path + "/duty_cycle", std::to_string(duty_ns))) {
            return false;
        }
        
        // Store configuration
        PWMChannel channel;
        channel.pin = pin;
        channel.period_ns = period_ns;
        channel.duty_ns = duty_ns;
        channel.enabled = false;
        channel.frequency_hz = 1e9 / period_ns;
        channel.duty_cycle = static_cast<double>(duty_ns) / period_ns;
        channels_[pin] = channel;
        
        return true;
    }
    
    bool enablePWM(int pin) {
        auto it = channels_.find(pin);
        if (it == channels_.end()) return false;
        
        std::string pwm_path = getPWMPath(pin);
        if (!writeToFile(pwm_path + "/enable", "1")) {
            return false;
        }
        
        it->second.enabled = true;
        return true;
    }
    
    bool disablePWM(int pin) {
        auto it = channels_.find(pin);
        if (it == channels_.end()) return false;
        
        std::string pwm_path = getPWMPath(pin);
        if (!writeToFile(pwm_path + "/enable", "0")) {
            return false;
        }
        
        it->second.enabled = false;
        return true;
    }
    
    bool setDutyCycle(int pin, int duty_ns) {
        auto it = channels_.find(pin);
        if (it == channels_.end()) return false;
        
        std::string pwm_path = getPWMPath(pin);
        if (!writeToFile(pwm_path + "/duty_cycle", std::to_string(duty_ns))) {
            return false;
        }
        
        it->second.duty_ns = duty_ns;
        it->second.duty_cycle = static_cast<double>(duty_ns) / it->second.period_ns;
        return true;
    }
    
    bool setFrequency(int pin, int frequency_hz) {
        auto it = channels_.find(pin);
        if (it == channels_.end()) return false;
        
        int period_ns = static_cast<int>(1e9 / frequency_hz);
        std::string pwm_path = getPWMPath(pin);
        
        if (!writeToFile(pwm_path + "/period", std::to_string(period_ns))) {
            return false;
        }
        
        it->second.period_ns = period_ns;
        it->second.frequency_hz = frequency_hz;
        return true;
    }
    
    bool setDutyPercentage(int pin, double percentage) {
        auto it = channels_.find(pin);
        if (it == channels_.end()) return false;
        
        int duty_ns = static_cast<int>(it->second.period_ns * percentage / 100.0);
        return setDutyCycle(pin, duty_ns);
    }
    
    bool isEnabled(int pin) const {
        auto it = channels_.find(pin);
        if (it == channels_.end()) return false;
        return it->second.enabled;
    }
    
    PWMChannel getConfig(int pin) const {
        auto it = channels_.find(pin);
        if (it == channels_.end()) return PWMChannel{};
        return it->second;
    }
    
    std::vector<int> getActivePins() const {
        std::vector<int> pins;
        for (const auto& [pin, channel] : channels_) {
            if (channel.enabled) {
                pins.push_back(pin);
            }
        }
        return pins;
    }
    
    void cleanup() {
        // Disable all PWM channels
        for (auto& [pin, channel] : channels_) {
            if (channel.enabled) {
                disablePWM(pin);
            }
        }
        channels_.clear();
    }
    
    bool rampPWM(int pin, int target_duty_ns, int duration_ms) {
        auto it = channels_.find(pin);
        if (it == channels_.end()) return false;
        
        int start_duty = it->second.duty_ns;
        int steps = 50;
        int step_delay = duration_ms / steps;
        int step_size = (target_duty_ns - start_duty) / steps;
        
        for (int i = 1; i <= steps; i++) {
            int duty = start_duty + (step_size * i);
            if (!setDutyCycle(pin, duty)) return false;
            std::this_thread::sleep_for(std::chrono::milliseconds(step_delay));
        }
        
        return true;
    }
    
    bool sweepPWM(int pin, int start_duty, int end_duty, int duration_ms) {
        auto it = channels_.find(pin);
        if (it == channels_.end()) return false;
        
        if (!setDutyCycle(pin, start_duty)) return false;
        
        int steps = 100;
        int step_delay = duration_ms / steps;
        int step_size = (end_duty - start_duty) / steps;
        
        for (int i = 1; i <= steps; i++) {
            int duty = start_duty + (step_size * i);
            if (!setDutyCycle(pin, duty)) return false;
            std::this_thread::sleep_for(std::chrono::milliseconds(step_delay));
        }
        
        return true;
    }
    
private:
    bool pwm_available_ = false;
    std::map<int, PWMChannel> channels_;
    
    void checkPWMAvailability() {
        // Check if PWM sysfs is available
        std::ifstream file("/sys/class/pwm/pwmchip0/export");
        pwm_available_ = file.good();
    }
    
    bool createPWMChannel(int pin) {
        std::string export_path = "/sys/class/pwm/pwmchip0/export";
        std::string pwm_path = "/sys/class/pwm/pwmchip0/pwm" + std::to_string(pin);
        
        // Check if already exported
        std::ifstream check(pwm_path + "/period");
        if (check.good()) return true;
        
        return writeToFile(export_path, std::to_string(pin));
    }
    
    std::string getPWMPath(int pin) {
        return "/sys/class/pwm/pwmchip0/pwm" + std::to_string(pin);
    }
    
    bool writeToFile(const std::string& path, const std::string& value) {
        std::ofstream file(path);
        if (!file.is_open()) return false;
        file << value;
        return file.good();
    }
};

GPIOPWM::GPIOPWM() : pImpl(std::make_unique<Impl>()) {}
GPIOPWM::~GPIOPWM() = default;

bool GPIOPWM::setupPWM(int pin, int period_ns, int duty_ns) {
    return pImpl->setupPWM(pin, period_ns, duty_ns);
}
bool GPIOPWM::enablePWM(int pin) { return pImpl->enablePWM(pin); }
bool GPIOPWM::disablePWM(int pin) { return pImpl->disablePWM(pin); }
bool GPIOPWM::setDutyCycle(int pin, int duty_ns) {
    return pImpl->setDutyCycle(pin, duty_ns);
}
bool GPIOPWM::setFrequency(int pin, int frequency_hz) {
    return pImpl->setFrequency(pin, frequency_hz);
}
bool GPIOPWM::setDutyPercentage(int pin, double percentage) {
    return pImpl->setDutyPercentage(pin, percentage);
}
bool GPIOPWM::isEnabled(int pin) const { return pImpl->isEnabled(pin); }
PWMChannel GPIOPWM::getConfig(int pin) const { return pImpl->getConfig(pin); }
std::vector<int> GPIOPWM::getActivePins() const { return pImpl->getActivePins(); }
void GPIOPWM::cleanup() { pImpl->cleanup(); }

bool GPIOPWM::rampPWM(int pin, int target_duty_ns, int duration_ms) {
    return pImpl->rampPWM(pin, target_duty_ns, duration_ms);
}
bool GPIOPWM::sweepPWM(int pin, int start_duty, int end_duty, int duration_ms) {
    return pImpl->sweepPWM(pin, start_duty, end_duty, duration_ms);
}

} // namespace EdgeAI
