#pragma once
#include <string>
#include <vector>
#include <functional>
#include <atomic>
#include <thread>
#include <mutex>
#include <unordered_map>

namespace EdgeAI {

enum GPIODirection {
    GPIO_IN = 0,
    GPIO_OUT = 1
};

enum GPIOValue {
    GPIO_LOW = 0,
    GPIO_HIGH = 1
};

enum GPIOEdge {
    GPIO_EDGE_NONE = 0,
    GPIO_EDGE_RISING = 1,
    GPIO_EDGE_FALLING = 2,
    GPIO_EDGE_BOTH = 3
};

enum GPIOPull {
    GPIO_PULL_NONE = 0,
    GPIO_PULL_UP = 1,
    GPIO_PULL_DOWN = 2
};

struct GPIOPin {
    int pin_number;
    std::string name;
    GPIODirection direction;
    GPIOValue value;
    GPIOEdge edge;
    GPIOPull pull;
    bool exported;
    bool is_active_low;
    std::function<void(int, GPIOValue)> callback;
};

struct PWMConfig {
    int pin;
    int period_ns = 1000000;  // 1ms default
    int duty_ns = 500000;      // 50% duty cycle
    bool enabled = false;
};

class GPIODriver {
public:
    GPIODriver();
    ~GPIODriver();
    
    // Pin management
    bool setupPin(int pin, GPIODirection direction, 
                  bool active_low = false);
    bool setupPinWithPull(int pin, GPIODirection direction, 
                          GPIOPull pull, bool active_low = false);
    bool exportPin(int pin);
    bool unexportPin(int pin);
    bool isExported(int pin) const;
    
    // GPIO operations
    bool setPin(int pin, GPIOValue value);
    bool getPin(int pin, GPIOValue& value);
    bool togglePin(int pin);
    bool pulsePin(int pin, int duration_ms);
    
    // Interrupt handling
    bool setInterrupt(int pin, GPIOEdge edge, 
                      std::function<void(int, GPIOValue)> callback);
    bool removeInterrupt(int pin);
    bool waitForInterrupt(int pin, int timeout_ms = -1);
    
    // PWM
    bool setupPWM(int pin, int period_ns, int duty_ns);
    bool enablePWM(int pin);
    bool disablePWM(int pin);
    bool setPWMDutyCycle(int pin, int duty_ns);
    bool setPWMFrequency(int pin, int frequency_hz);
    
    // Bulk operations
    bool setPins(const std::vector<int>& pins, GPIOValue value);
    std::vector<GPIOValue> getPins(const std::vector<int>& pins);
    
    // Utility
    std::vector<int> getExportedPins() const;
    std::string getPinInfo(int pin) const;
    bool testPin(int pin, GPIODirection direction);
    void cleanup();
    
    // Callbacks
    void setErrorCallback(std::function<void(const std::string&)> callback);
    void setInterruptCallback(std::function<void(int, GPIOValue)> callback);
    
private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

} // namespace EdgeAI
