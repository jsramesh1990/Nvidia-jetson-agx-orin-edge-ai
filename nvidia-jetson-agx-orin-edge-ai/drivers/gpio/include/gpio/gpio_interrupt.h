#pragma once
#include <functional>
#include <atomic>
#include <thread>
#include <vector>
#include <mutex>
#include <map>

namespace EdgeAI {

struct InterruptEvent {
    int pin;
    int value;
    uint64_t timestamp;
    int edge_type;
};

class GPIOInterrupt {
public:
    GPIOInterrupt();
    ~GPIOInterrupt();
    
    bool registerInterrupt(int pin, int edge_type, 
                          std::function<void(int, int)> callback);
    bool unregisterInterrupt(int pin);
    bool waitForInterrupt(int pin, int timeout_ms = -1);
    
    // Event queue
    bool getNextEvent(InterruptEvent& event, int timeout_ms = -1);
    size_t getPendingEvents() const;
    void clearEvents();
    
    // Status
    bool isRegistered(int pin) const;
    std::vector<int> getRegisteredPins() const;
    
    // Configuration
    void setDebounceTime(int ms);
    void setInterruptThreadPriority(int priority);
    
private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

} // namespace EdgeAI
