#include "gpio/gpio_interrupt.h"
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/eventfd.h>
#include <linux/gpio.h>
#include <cstring>
#include <chrono>

namespace EdgeAI {

class GPIOInterrupt::Impl {
public:
    Impl() : running_(false), debounce_ms_(10) {
        // Open GPIO chip
        chip_fd_ = open("/dev/gpiochip0", O_RDWR);
        if (chip_fd_ < 0) {
            // Try alternative GPIO chip
            chip_fd_ = open("/dev/gpiochip1", O_RDWR);
        }
        
        if (chip_fd_ >= 0) {
            // Start event monitoring thread
            running_ = true;
            event_thread_ = std::thread(&Impl::eventLoop, this);
        }
    }
    
    ~Impl() {
        running_ = false;
        if (event_thread_.joinable()) {
            event_thread_.join();
        }
        if (chip_fd_ >= 0) {
            close(chip_fd_);
        }
    }
    
    bool registerInterrupt(int pin, int edge_type,
                          std::function<void(int, int)> callback) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (chip_fd_ < 0) return false;
        
        // Request GPIO line
        struct gpioevent_request req;
        memset(&req, 0, sizeof(req));
        req.lineoffset = pin;
        req.handleflags = GPIOHANDLE_REQUEST_INPUT;
        req.eventflags = edge_type;
        strcpy(req.consumer_label, "gpio_interrupt");
        
        int fd = ioctl(chip_fd_, GPIO_GET_LINEEVENT_IOCTL, &req);
        if (fd < 0) return false;
        
        // Store event data
        GPIOEventData data;
        data.fd = fd;
        data.pin = pin;
        data.callback = callback;
        data.edge_type = edge_type;
        data.last_event_time = 0;
        
        events_[pin] = data;
        
        return true;
    }
    
    bool unregisterInterrupt(int pin) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = events_.find(pin);
        if (it == events_.end()) return false;
        
        close(it->second.fd);
        events_.erase(it);
        return true;
    }
    
    bool waitForInterrupt(int pin, int timeout_ms) {
        auto it = events_.find(pin);
        if (it == events_.end()) return false;
        
        struct pollfd pfd;
        pfd.fd = it->second.fd;
        pfd.events = POLLIN;
        
        int ret = poll(&pfd, 1, timeout_ms);
        return ret > 0;
    }
    
    bool getNextEvent(InterruptEvent& event, int timeout_ms) {
        std::unique_lock<std::mutex> lock(event_queue_mutex_);
        
        if (event_queue_.empty() && timeout_ms >= 0) {
            event_queue_cv_.wait_for(lock, 
                std::chrono::milliseconds(timeout_ms),
                [this]() { return !event_queue_.empty(); });
        }
        
        if (event_queue_.empty()) return false;
        
        event = event_queue_.front();
        event_queue_.pop();
        return true;
    }
    
    size_t getPendingEvents() const {
        std::lock_guard<std::mutex> lock(event_queue_mutex_);
        return event_queue_.size();
    }
    
    void clearEvents() {
        std::lock_guard<std::mutex> lock(event_queue_mutex_);
        while (!event_queue_.empty()) {
            event_queue_.pop();
        }
    }
    
    bool isRegistered(int pin) const {
        std::lock_guard<std::mutex> lock(mutex_);
        return events_.find(pin) != events_.end();
    }
    
    std::vector<int> getRegisteredPins() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<int> pins;
        for (const auto& [pin, data] : events_) {
            pins.push_back(pin);
        }
        return pins;
    }
    
    void setDebounceTime(int ms) {
        debounce_ms_ = ms;
    }
    
private:
    struct GPIOEventData {
        int fd;
        int pin;
        int edge_type;
        uint64_t last_event_time;
        std::function<void(int, int)> callback;
    };
    
    int chip_fd_;
    std::map<int, GPIOEventData> events_;
    mutable std::mutex mutex_;
    std::thread event_thread_;
    std::atomic<bool> running_;
    int debounce_ms_;
    
    mutable std::mutex event_queue_mutex_;
    std::queue<InterruptEvent> event_queue_;
    std::condition_variable event_queue_cv_;
    
    void eventLoop() {
        while (running_) {
            // Collect all FDs
            std::vector<struct pollfd> pfds;
            std::map<int, int> fd_to_pin;
            
            {
                std::lock_guard<std::mutex> lock(mutex_);
                pfds.reserve(events_.size());
                for (const auto& [pin, data] : events_) {
                    struct pollfd pfd;
                    pfd.fd = data.fd;
                    pfd.events = POLLIN;
                    pfds.push_back(pfd);
                    fd_to_pin[data.fd] = pin;
                }
            }
            
            if (pfds.empty()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }
            
            int ret = poll(pfds.data(), pfds.size(), 100);
            if (ret < 0) continue;
            
            for (const auto& pfd : pfds) {
                if (pfd.revents & POLLIN) {
                    int pin = fd_to_pin[pfd.fd];
                    
                    // Read event
                    struct gpioevent_data event_data;
                    int n = read(pfd.fd, &event_data, sizeof(event_data));
                    
                    if (n == sizeof(event_data)) {
                        uint64_t current_time = std::chrono::duration_cast<
                            std::chrono::milliseconds>(
                            std::chrono::system_clock::now().time_since_epoch()
                        ).count();
                        
                        // Debounce check
                        std::lock_guard<std::mutex> lock(mutex_);
                        auto it = events_.find(pin);
                        if (it != events_.end()) {
                            if (current_time - it->second.last_event_time >= debounce_ms_) {
                                it->second.last_event_time = current_time;
                                
                                // Store event
                                InterruptEvent evt;
                                evt.pin = pin;
                                evt.value = event_data.id;
                                evt.timestamp = current_time;
                                evt.edge_type = it->second.edge_type;
                                
                                std::lock_guard<std::mutex> qlock(event_queue_mutex_);
                                event_queue_.push(evt);
                                event_queue_cv_.notify_one();
                                
                                // Call callback
                                if (it->second.callback) {
                                    it->second.callback(pin, event_data.id);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
};

GPIOInterrupt::GPIOInterrupt() : pImpl(std::make_unique<Impl>()) {}
GPIOInterrupt::~GPIOInterrupt() = default;

bool GPIOInterrupt::registerInterrupt(int pin, int edge_type,
                                      std::function<void(int, int)> callback) {
    return pImpl->registerInterrupt(pin, edge_type, callback);
}

bool GPIOInterrupt::unregisterInterrupt(int pin) {
    return pImpl->unregisterInterrupt(pin);
}

bool GPIOInterrupt::waitForInterrupt(int pin, int timeout_ms) {
    return pImpl->waitForInterrupt(pin, timeout_ms);
}

bool GPIOInterrupt::getNextEvent(InterruptEvent& event, int timeout_ms) {
    return pImpl->getNextEvent(event, timeout_ms);
}

size_t GPIOInterrupt::getPendingEvents() const {
    return pImpl->getPendingEvents();
}

void GPIOInterrupt::clearEvents() {
    pImpl->clearEvents();
}

bool GPIOInterrupt::isRegistered(int pin) const {
    return pImpl->isRegistered(pin);
}

std::vector<int> GPIOInterrupt::getRegisteredPins() const {
    return pImpl->getRegisteredPins();
}

void GPIOInterrupt::setDebounceTime(int ms) {
    pImpl->setDebounceTime(ms);
}

} // namespace EdgeAI
