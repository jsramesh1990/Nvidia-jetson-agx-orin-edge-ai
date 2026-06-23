#include "gpio/gpio_driver.h"
#include "gpio/gpio_interrupt.h"
#include "gpio/gpio_pwm.h"
#include <fstream>
#include <sstream>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <chrono>
#include <cstring>

namespace EdgeAI {

class GPIODriver::Impl {
public:
    Impl() : interrupt_(std::make_unique<GPIOInterrupt>()),
             pwm_(std::make_unique<GPIOPWM>()) {
        // Initialize GPIO sysfs
        initGPIO();
    }
    
    ~Impl() {
        cleanup();
    }
    
    bool setupPin(int pin, GPIODirection direction, bool active_low) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (!exportPin(pin)) return false;
        
        // Set direction
        if (!setPinDirection(pin, direction)) return false;
        
        // Set active low if needed
        if (active_low) {
            if (!writeToFile(getPinPath(pin, "active_low"), "1")) {
                return false;
            }
        }
        
        // Initialize pin structure
        GPIOPin pin_info;
        pin_info.pin_number = pin;
        pin_info.direction = direction;
        pin_info.value = GPIO_LOW;
        pin_info.exported = true;
        pin_info.is_active_low = active_low;
        pins_[pin] = pin_info;
        
        return true;
    }
    
    bool setupPinWithPull(int pin, GPIODirection direction, 
                          GPIOPull pull, bool active_low) {
        // Setup pin with pull-up/down resistor
        if (!setupPin(pin, direction, active_low)) return false;
        
        // Set pull configuration (requires device tree or specific GPIO chip support)
        // This is a placeholder - actual implementation depends on hardware
        pins_[pin].pull = pull;
        return true;
    }
    
    bool exportPin(int pin) {
        if (isExported(pin)) return true;
        
        std::string export_path = "/sys/class/gpio/export";
        return writeToFile(export_path, std::to_string(pin));
    }
    
    bool unexportPin(int pin) {
        if (!isExported(pin)) return false;
        
        std::string unexport_path = "/sys/class/gpio/unexport";
        if (!writeToFile(unexport_path, std::to_string(pin))) {
            return false;
        }
        
        pins_.erase(pin);
        return true;
    }
    
    bool isExported(int pin) const {
        std::string path = "/sys/class/gpio/gpio" + std::to_string(pin);
        struct stat st;
        return stat(path.c_str(), &st) == 0;
    }
    
    bool setPin(int pin, GPIOValue value) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (!isExported(pin)) return false;
        
        bool is_active_low = (pins_.find(pin) != pins_.end() && pins_[pin].is_active_low);
        int write_value = is_active_low ? (value == GPIO_HIGH ? 0 : 1) : value;
        
        if (!writeToFile(getPinPath(pin, "value"), std::to_string(write_value))) {
            return false;
        }
        
        pins_[pin].value = value;
        return true;
    }
    
    bool getPin(int pin, GPIOValue& value) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (!isExported(pin)) return false;
        
        std::string content;
        if (!readFromFile(getPinPath(pin, "value"), content)) {
            return false;
        }
        
        int read_value = std::stoi(content);
        bool is_active_low = (pins_.find(pin) != pins_.end() && pins_[pin].is_active_low);
        value = is_active_low ? (read_value == 0 ? GPIO_HIGH : GPIO_LOW) : 
                                (read_value == 1 ? GPIO_HIGH : GPIO_LOW);
        pins_[pin].value = value;
        return true;
    }
    
    bool togglePin(int pin) {
        GPIOValue current;
        if (!getPin(pin, current)) return false;
        return setPin(pin, current == GPIO_HIGH ? GPIO_LOW : GPIO_HIGH);
    }
    
    bool pulsePin(int pin, int duration_ms) {
        if (!setPin(pin, GPIO_HIGH)) return false;
        std::this_thread::sleep_for(std::chrono::milliseconds(duration_ms));
        return setPin(pin, GPIO_LOW);
    }
    
    bool setInterrupt(int pin, GPIOEdge edge, 
                      std::function<void(int, GPIOValue)> callback) {
        if (!isExported(pin)) return false;
        
        // Map GPIOEdge to interrupt edge
        int edge_type;
        switch (edge) {
            case GPIO_EDGE_RISING: edge_type = GPIOEVENT_REQUEST_RISING_EDGE; break;
            case GPIO_EDGE_FALLING: edge_type = GPIOEVENT_REQUEST_FALLING_EDGE; break;
            case GPIO_EDGE_BOTH: edge_type = GPIOEVENT_REQUEST_BOTH_EDGES; break;
            default: return false;
        }
        
        // Set edge in sysfs
        std::string edge_str;
        switch (edge) {
            case GPIO_EDGE_RISING: edge_str = "rising"; break;
            case GPIO_EDGE_FALLING: edge_str = "falling"; break;
            case GPIO_EDGE_BOTH: edge_str = "both"; break;
            default: return false;
        }
        
        if (!writeToFile(getPinPath(pin, "edge"), edge_str)) {
            return false;
        }
        
        // Register interrupt
        return interrupt_->registerInterrupt(pin, edge_type, callback);
    }
    
    bool removeInterrupt(int pin) {
        return interrupt_->unregisterInterrupt(pin);
    }
    
    bool waitForInterrupt(int pin, int timeout_ms) {
        return interrupt_->waitForInterrupt(pin, timeout_ms);
    }
    
    bool setupPWM(int pin, int period_ns, int duty_ns) {
        return pwm_->setupPWM(pin, period_ns, duty_ns);
    }
    
    bool enablePWM(int pin) {
        return pwm_->enablePWM(pin);
    }
    
    bool disablePWM(int pin) {
        return pwm_->disablePWM(pin);
    }
    
    bool setPWMDutyCycle(int pin, int duty_ns) {
        return pwm_->setDutyCycle(pin, duty_ns);
    }
    
    bool setPWMFrequency(int pin, int frequency_hz) {
        return pwm_->setFrequency(pin, frequency_hz);
    }
    
    bool setPins(const std::vector<int>& pins, GPIOValue value) {
        bool success = true;
        for (int pin : pins) {
            if (!setPin(pin, value)) {
                success = false;
            }
        }
        return success;
    }
    
    std::vector<GPIOValue> getPins(const std::vector<int>& pins) {
        std::vector<GPIOValue> values;
        for (int pin : pins) {
            GPIOValue value;
            if (getPin(pin, value)) {
                values.push_back(value);
            } else {
                values.push_back(GPIO_LOW);
            }
        }
        return values;
    }
    
    std::vector<int> getExportedPins() const {
        std::vector<int> exported;
        DIR* dir = opendir("/sys/class/gpio");
        if (!dir) return exported;
        
        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            std::string name = entry->d_name;
            if (name.find("gpio") == 0 && name.length() > 4) {
                try {
                    int pin = std::stoi(name.substr(4));
                    exported.push_back(pin);
                } catch (...) {
                    // Skip non-numeric names
                }
            }
        }
        closedir(dir);
        return exported;
    }
    
    std::string getPinInfo(int pin) const {
        std::stringstream ss;
        auto it = pins_.find(pin);
        if (it == pins_.end()) {
            return "Pin " + std::to_string(pin) + " not configured";
        }
        
        ss << "Pin: " << pin << "\n";
        ss << "Direction: " << (it->second.direction == GPIO_IN ? "Input" : "Output") << "\n";
        ss << "Value: " << (it->second.value == GPIO_HIGH ? "HIGH" : "LOW") << "\n";
        ss << "Active Low: " << (it->second.is_active_low ? "Yes" : "No") << "\n";
        ss << "Exported: " << (it->second.exported ? "Yes" : "No") << "\n";
        return ss.str();
    }
    
    bool testPin(int pin, GPIODirection direction) {
        if (!setupPin(pin, direction)) return false;
        
        if (direction == GPIO_OUT) {
            // Test output
            if (!setPin(pin, GPIO_HIGH)) return false;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            if (!setPin(pin, GPIO_LOW)) return false;
            return true;
        } else {
            // Test input
            GPIOValue value;
            return getPin(pin, value);
        }
    }
    
    void cleanup() {
        for (auto& [pin, info] : pins_) {
            if (info.exported) {
                unexportPin(pin);
            }
        }
        pins_.clear();
        pwm_->cleanup();
    }
    
private:
    std::map<int, GPIOPin> pins_;
    std::unique_ptr<GPIOInterrupt> interrupt_;
    std::unique_ptr<GPIOPWM> pwm_;
    mutable std::mutex mutex_;
    
    void initGPIO() {
        // Initialize GPIO subsystem if needed
    }
    
    bool setPinDirection(int pin, GPIODirection direction) {
        std::string direction_str = (direction == GPIO_IN) ? "in" : "out";
        return writeToFile(getPinPath(pin, "direction"), direction_str);
    }
    
    std::string getPinPath(int pin, const std::string& attribute) const {
        return "/sys/class/gpio/gpio" + std::to_string(pin) + "/" + attribute;
    }
    
    bool writeToFile(const std::string& path, const std::string& value) {
        std::ofstream file(path);
        if (!file.is_open()) return false;
        file << value;
        return file.good();
    }
    
    bool readFromFile(const std::string& path, std::string& content) {
        std::ifstream file(path);
        if (!file.is_open()) return false;
        std::getline(file, content);
        return true;
    }
    
    void setError(const std::string& error) {
        if (error_callback_) {
            error_callback_(error);
        }
        std::cerr << "GPIO Error: " << error << std::endl;
    }
};

// GPIODriver implementation
GPIODriver::GPIODriver() : pImpl(std::make_unique<Impl>()) {}
GPIODriver::~GPIODriver() = default;

bool GPIODriver::setupPin(int pin, GPIODirection direction, bool active_low) {
    return pImpl->setupPin(pin, direction, active_low);
}
bool GPIODriver::setupPinWithPull(int pin, GPIODirection direction, 
                                  GPIOPull pull, bool active_low) {
    return pImpl->setupPinWithPull(pin, direction, pull, active_low);
}
bool GPIODriver::exportPin(int pin) { return pImpl->exportPin(pin); }
bool GPIODriver::unexportPin(int pin) { return pImpl->unexportPin(pin); }
bool GPIODriver::isExported(int pin) const { return pImpl->isExported(pin); }

bool GPIODriver::setPin(int pin, GPIOValue value) { return pImpl->setPin(pin, value); }
bool GPIODriver::getPin(int pin, GPIOValue& value) { return pImpl->getPin(pin, value); }
bool GPIODriver::togglePin(int pin) { return pImpl->togglePin(pin); }
bool GPIODriver::pulsePin(int pin, int duration_ms) {
    return pImpl->pulsePin(pin, duration_ms);
}

bool GPIODriver::setInterrupt(int pin, GPIOEdge edge,
                              std::function<void(int, GPIOValue)> callback) {
    return pImpl->setInterrupt(pin, edge, callback);
}
bool GPIODriver::removeInterrupt(int pin) { return pImpl->removeInterrupt(pin); }
bool GPIODriver::waitForInterrupt(int pin, int timeout_ms) {
    return pImpl->waitForInterrupt(pin, timeout_ms);
}

bool GPIODriver::setupPWM(int pin, int period_ns, int duty_ns) {
    return pImpl->setupPWM(pin, period_ns, duty_ns);
}
bool GPIODriver::enablePWM(int pin) { return pImpl->enablePWM(pin); }
bool GPIODriver::disablePWM(int pin) { return pImpl->disablePWM(pin); }
bool GPIODriver::setPWMDutyCycle(int pin, int duty_ns) {
    return pImpl->setPWMDutyCycle(pin, duty_ns);
}
bool GPIODriver::setPWMFrequency(int pin, int frequency_hz) {
    return pImpl->setPWMFrequency(pin, frequency_hz);
}

bool GPIODriver::setPins(const std::vector<int>& pins, GPIOValue value) {
    return pImpl->setPins(pins, value);
}
std::vector<GPIOValue> GPIODriver::getPins(const std::vector<int>& pins) {
    return pImpl->getPins(pins);
}

std::vector<int> GPIODriver::getExportedPins() const {
    return pImpl->getExportedPins();
}
std::string GPIODriver::getPinInfo(int pin) const {
    return pImpl->getPinInfo(pin);
}
bool GPIODriver::testPin(int pin, GPIODirection direction) {
    return pImpl->testPin(pin, direction);
}
void GPIODriver::cleanup() { pImpl->cleanup(); }

} // namespace EdgeAI
