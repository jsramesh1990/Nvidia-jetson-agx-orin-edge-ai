#include "i2c/i2c_bus.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <algorithm>

namespace EdgeAI {

class I2CBus::Impl {
public:
    Impl(int bus_number) : bus_number_(bus_number), initialized_(false) {
        // Create default config for this bus
        I2CConfig config;
        config.device = "/dev/i2c-" + std::to_string(bus_number);
        config.speed = 100000;
        config.use_smbus = true;
        config.timeout_ms = 1000;
        config.retries = 3;
        
        driver_ = std::make_unique<I2CDriver>(config);
    }
    
    ~Impl() {
        close();
    }
    
    bool initialize() {
        if (initialized_) return true;
        
        if (!driver_->initialize()) {
            setError("Failed to initialize I2C bus " + std::to_string(bus_number_));
            return false;
        }
        
        initialized_ = true;
        
        // Start auto-detection if enabled
        if (auto_detect_enabled_) {
            startAutoDetection();
        }
        
        return true;
    }
    
    void close() {
        stopAutoDetection();
        driver_->close();
        initialized_ = false;
    }
    
    bool isOpen() const {
        return driver_->isOpen();
    }
    
    bool isReady() const {
        return initialized_ && driver_->isOpen();
    }
    
    bool registerDevice(uint8_t address, const I2CConfig& config) {
        if (!initialized_) return false;
        
        std::lock_guard<std::mutex> lock(device_mutex_);
        
        // Check if device already registered
        if (devices_.find(address) != devices_.end()) {
            return false;
        }
        
        // Create device with custom config
        auto device = std::make_shared<I2CDevice>(*driver_, address);
        device->setTimeout(config.timeout_ms);
        device->setRetries(config.retries);
        
        devices_[address] = device;
        
        // Notify callback
        if (detection_callback_) {
            detection_callback_(address, true);
        }
        
        return true;
    }
    
    bool unregisterDevice(uint8_t address) {
        std::lock_guard<std::mutex> lock(device_mutex_);
        
        auto it = devices_.find(address);
        if (it == devices_.end()) {
            return false;
        }
        
        // Notify callback
        if (detection_callback_) {
            detection_callback_(address, false);
        }
        
        devices_.erase(it);
        return true;
    }
    
    std::shared_ptr<I2CDevice> getDevice(uint8_t address) {
        std::lock_guard<std::mutex> lock(device_mutex_);
        
        auto it = devices_.find(address);
        if (it == devices_.end()) {
            return nullptr;
        }
        return it->second;
    }
    
    std::vector<uint8_t> getRegisteredDevices() const {
        std::lock_guard<std::mutex> lock(device_mutex_);
        
        std::vector<uint8_t> addresses;
        for (const auto& [addr, device] : devices_) {
            addresses.push_back(addr);
        }
        return addresses;
    }
    
    bool scanBus(std::vector<uint8_t>& found_addresses) {
        if (!initialized_) return false;
        
        // Use driver's scan function
        return driver_->scanBus(found_addresses);
    }
    
    bool probeDevice(uint8_t address) {
        if (!initialized_) return false;
        
        // Try to read 1 byte from the address
        auto data = driver_->read(address, 1);
        return !data.empty() || driver_->isOpen(); // Partial check
    }
    
    bool write(uint8_t address, const std::vector<uint8_t>& data) {
        if (!initialized_) return false;
        
        auto start = std::chrono::steady_clock::now();
        bool success = driver_->write(address, data);
        auto end = std::chrono::steady_clock::now();
        
        double duration = std::chrono::duration<double, std::milli>(end - start).count();
        updateStats(duration, success, data.size(), true);
        
        return success;
    }
    
    std::vector<uint8_t> read(uint8_t address, size_t count) {
        if (!initialized_) return {};
        
        auto start = std::chrono::steady_clock::now();
        auto data = driver_->read(address, count);
        auto end = std::chrono::steady_clock::now();
        
        double duration = std::chrono::duration<double, std::milli>(end - start).count();
        updateStats(duration, !data.empty(), data.size(), false);
        
        return data;
    }
    
    std::vector<uint8_t> writeRead(uint8_t address,
                                   const std::vector<uint8_t>& write_data,
                                   size_t read_count) {
        if (!initialized_) return {};
        
        auto start = std::chrono::steady_clock::now();
        auto data = driver_->writeRead(address, write_data, read_count);
        auto end = std::chrono::steady_clock::now();
        
        double duration = std::chrono::duration<double, std::milli>(end - start).count();
        updateStats(duration, !data.empty(), data.size() + write_data.size(), true);
        
        return data;
    }
    
    bool transactionAsync(const I2CTransaction& transaction) {
        if (!initialized_) return false;
        
        return driver_->transactionAsync(transaction);
    }
    
    bool isBusy() const {
        // Check if driver has pending operations
        return false; // Simplified
    }
    
    bool waitForBus(int timeout_ms) {
        auto start = std::chrono::steady_clock::now();
        while (isBusy()) {
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start).count();
            if (elapsed > timeout_ms) {
                return false;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        return true;
    }
    
    void setBusSpeed(int speed) {
        driver_->setSpeed(speed);
    }
    
    int getBusSpeed() const {
        return driver_->getSpeed();
    }
    
    void setTimeout(int ms) {
        driver_->setTimeout(ms);
    }
    
    int getTimeout() const {
        return driver_->getTimeout();
    }
    
    void setRetries(int retries) {
        driver_->setRetries(retries);
    }
    
    int getRetries() const {
        return driver_->getRetries();
    }
    
    void setSMBus(bool enable) {
        driver_->setSMBus(enable);
    }
    
    bool isSMBus() const {
        return driver_->isSMBus();
    }
    
    bool resetBus() {
        if (!initialized_) return false;
        
        // Perform a bus reset by toggling clock
        // This is platform-specific and may require GPIO control
        // For now, just close and reopen the bus
        close();
        return initialize();
    }
    
    std::string getBusInfo() const {
        std::stringstream ss;
        ss << "I2C Bus " << bus_number_ << " Info:\n";
        ss << "  Open: " << (isOpen() ? "Yes" : "No") << "\n";
        ss << "  Speed: " << getBusSpeed() << " Hz\n";
        ss << "  SMBus: " << (isSMBus() ? "Enabled" : "Disabled") << "\n";
        ss << "  Timeout: " << getTimeout() << " ms\n";
        ss << "  Retries: " << getRetries() << "\n";
        ss << "  Devices: " << devices_.size() << "\n";
        
        auto stats = getStats();
        ss << "  Transactions: " << stats.total_transactions << "\n";
        ss << "  Failed: " << stats.failed_transactions << "\n";
        ss << "  Bytes Read: " << stats.total_bytes_read << "\n";
        ss << "  Bytes Written: " << stats.total_bytes_written << "\n";
        ss << "  Bus Errors: " << stats.bus_errors << "\n";
        ss << "  Timeouts: " << stats.timeouts << "\n";
        ss << "  NACKs: " << stats.nacks << "\n";
        
        return ss.str();
    }
    
    bool testBus() {
        if (!initialized_) return false;
        
        // Test with a known device if available
        std::vector<uint8_t> addresses;
        if (!scanBus(addresses)) {
            return false;
        }
        
        return !addresses.empty();
    }
    
    BusStats getStats() const {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        
        BusStats stats;
        stats.total_transactions = stats_.total_transactions;
        stats.failed_transactions = stats_.failed_transactions;
        stats.total_bytes_read = stats_.total_bytes_read;
        stats.total_bytes_written = stats_.total_bytes_written;
        stats.avg_transaction_time_ms = stats_.avg_transaction_time_ms;
        stats.active_devices = devices_.size();
        stats.bus_errors = stats_.bus_errors;
        stats.timeouts = stats_.timeouts;
        stats.nacks = stats_.nacks;
        
        // Calculate utilization (simplified)
        stats.bus_utilization_percent = 0.0;
        
        return stats;
    }
    
    void resetStats() {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_ = Stats();
    }
    
    void setCompletionCallback(std::function<void(bool, uint8_t)> callback) {
        completion_callback_ = callback;
    }
    
    void setErrorCallback(std::function<void(const std::string&)> callback) {
        error_callback_ = callback;
    }
    
    void setTransactionCallback(std::function<void(uint8_t, bool, size_t)> callback) {
        transaction_callback_ = callback;
    }
    
    void setDeviceDetectionCallback(std::function<void(uint8_t, bool)> callback) {
        detection_callback_ = callback;
    }
    
    void setAutoDetection(bool enable, int interval_ms) {
        auto_detect_enabled_ = enable;
        auto_detect_interval_ms_ = interval_ms;
        
        if (enable) {
            startAutoDetection();
        } else {
            stopAutoDetection();
        }
    }
    
    void dumpRegisters(uint8_t address, uint8_t start_reg, uint8_t count) {
        if (!initialized_) return;
        
        std::cout << "I2C Register Dump for address 0x" << std::hex << (int)address << std::dec << ":\n";
        std::cout << "  " << "Reg  " << "Value  " << "Binary" << std::endl;
        std::cout << "  " << "----- " << "-----  " << "------" << std::endl;
        
        for (uint8_t reg = start_reg; reg < start_reg + count; reg++) {
            uint8_t value = 0;
            try {
                value = driver_->readByteData(address, reg);
            } catch (...) {
                value = 0xFF;
            }
            
            std::cout << "  0x" << std::hex << std::setw(2) << std::setfill('0') 
                      << (int)reg << "  0x" << std::setw(2) 
                      << (int)value << "    ";
            
            // Print binary representation
            for (int bit = 7; bit >= 0; bit--) {
                std::cout << ((value >> bit) & 1);
                if (bit % 4 == 0 && bit > 0) std::cout << " ";
            }
            std::cout << std::dec << std::endl;
        }
    }
    
private:
    int bus_number_;
    bool initialized_;
    std::unique_ptr<I2CDriver> driver_;
    std::map<uint8_t, std::shared_ptr<I2CDevice>> devices_;
    mutable std::mutex device_mutex_;
    mutable std::mutex stats_mutex_;
    
    bool auto_detect_enabled_ = false;
    int auto_detect_interval_ms_ = 10000;
    std::thread detect_thread_;
    std::atomic<bool> running_{false};
    
    std::function<void(bool, uint8_t)> completion_callback_;
    std::function<void(const std::string&)> error_callback_;
    std::function<void(uint8_t, bool, size_t)> transaction_callback_;
    std::function<void(uint8_t, bool)> detection_callback_;
    
    struct Stats {
        std::atomic<size_t> total_transactions{0};
        std::atomic<size_t> failed_transactions{0};
        std::atomic<size_t> total_bytes_read{0};
        std::atomic<size_t> total_bytes_written{0};
        std::atomic<size_t> bus_errors{0};
        std::atomic<size_t> timeouts{0};
        std::atomic<size_t> nacks{0};
        double avg_transaction_time_ms = 0.0;
    };
    Stats stats_;
    
    void setError(const std::string& error) {
        if (error_callback_) {
            error_callback_(error);
        }
        std::cerr << "I2C Bus Error: " << error << std::endl;
    }
    
    void updateStats(double duration_ms, bool success, size_t bytes, bool is_write) {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        
        stats_.total_transactions++;
        if (!success) {
            stats_.failed_transactions++;
        }
        
        if (is_write) {
            stats_.total_bytes_written += bytes;
        } else {
            stats_.total_bytes_read += bytes;
        }
        
        // Update average
        if (stats_.avg_transaction_time_ms == 0.0) {
            stats_.avg_transaction_time_ms = duration_ms;
        } else {
            stats_.avg_transaction_time_ms = (stats_.avg_transaction_time_ms * 0.9) + (duration_ms * 0.1);
        }
        
        if (transaction_callback_) {
            transaction_callback_(0, success, bytes);
        }
    }
    
    void startAutoDetection() {
        if (running_) return;
        
        running_ = true;
        detect_thread_ = std::thread(&Impl::detectionLoop, this);
    }
    
    void stopAutoDetection() {
        running_ = false;
        if (detect_thread_.joinable()) {
            detect_thread_.join();
        }
    }
    
    void detectionLoop() {
        while (running_) {
            std::vector<uint8_t> found;
            if (scanBus(found)) {
                // Check for new devices
                std::lock_guard<std::mutex> lock(device_mutex_);
                for (uint8_t addr : found) {
                    if (devices_.find(addr) == devices_.end()) {
                        if (detection_callback_) {
                            detection_callback_(addr, true);
                        }
                    }
                }
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(auto_detect_interval_ms_));
        }
    }
};

// I2CBus implementation
I2CBus::I2CBus(int bus_number) : bus_number_(bus_number), 
    pImpl(std::make_unique<Impl>(bus_number)) {}

I2CBus::~I2CBus() = default;

bool I2CBus::initialize() { return pImpl->initialize(); }
void I2CBus::close() { pImpl->close(); }
bool I2CBus::isOpen() const { return pImpl->isOpen(); }
bool I2CBus::isReady() const { return pImpl->isReady(); }

bool I2CBus::registerDevice(uint8_t address, const I2CConfig& config) {
    return pImpl->registerDevice(address, config);
}
bool I2CBus::unregisterDevice(uint8_t address) {
    return pImpl->unregisterDevice(address);
}
std::shared_ptr<I2CDevice> I2CBus::getDevice(uint8_t address) {
    return pImpl->getDevice(address);
}
std::vector<uint8_t> I2CBus::getRegisteredDevices() const {
    return pImpl->getRegisteredDevices();
}

bool I2CBus::scanBus(std::vector<uint8_t>& found_addresses) {
    return pImpl->scanBus(found_addresses);
}
bool I2CBus::probeDevice(uint8_t address) {
    return pImpl->probeDevice(address);
}

bool I2CBus::write(uint8_t address, const std::vector<uint8_t>& data) {
    return pImpl->write(address, data);
}
std::vector<uint8_t> I2CBus::read(uint8_t address, size_t count) {
    return pImpl->read(address, count);
}
std::vector<uint8_t> I2CBus::writeRead(uint8_t address,
                                       const std::vector<uint8_t>& write_data,
                                       size_t read_count) {
    return pImpl->writeRead(address, write_data, read_count);
}

bool I2CBus::transactionAsync(const I2CTransaction& transaction) {
    return pImpl->transactionAsync(transaction);
}

bool I2CBus::isBusy() const { return pImpl->isBusy(); }
bool I2CBus::waitForBus(int timeout_ms) { return pImpl->waitForBus(timeout_ms); }

void I2CBus::setBusSpeed(int speed) { pImpl->setBusSpeed(speed); }
int I2CBus::getBusSpeed() const { return pImpl->getBusSpeed(); }
void I2CBus::setTimeout(int ms) { pImpl->setTimeout(ms); }
int I2CBus::getTimeout() const { return pImpl->getTimeout(); }
void I2CBus::setRetries(int retries) { pImpl->setRetries(retries); }
int I2CBus::getRetries() const { return pImpl->getRetries(); }
void I2CBus::setSMBus(bool enable) { pImpl->setSMBus(enable); }
bool I2CBus::isSMBus() const { return pImpl->isSMBus(); }

bool I2CBus::resetBus() { return pImpl->resetBus(); }
std::string I2CBus::getBusInfo() const { return pImpl->getBusInfo(); }
bool I2CBus::testBus() { return pImpl->testBus(); }

I2CBus::BusStats I2CBus::getStats() const { return pImpl->getStats(); }
void I2CBus::resetStats() { pImpl->resetStats(); }

void I2CBus::setCompletionCallback(std::function<void(bool, uint8_t)> callback) {
    pImpl->setCompletionCallback(callback);
}
void I2CBus::setErrorCallback(std::function<void(const std::string&)> callback) {
    pImpl->setErrorCallback(callback);
}
void I2CBus::setTransactionCallback(std::function<void(uint8_t, bool, size_t)> callback) {
    pImpl->setTransactionCallback(callback);
}
void I2CBus::setDeviceDetectionCallback(std::function<void(uint8_t, bool)> callback) {
    pImpl->setDeviceDetectionCallback(callback);
}

void I2CBus::setAutoDetection(bool enable, int interval_ms) {
    pImpl->setAutoDetection(enable, interval_ms);
}

void I2CBus::dumpRegisters(uint8_t address, uint8_t start_reg, uint8_t count) {
    pImpl->dumpRegisters(address, start_reg, count);
}

std::vector<uint8_t> I2CBus::getKnownAddresses() {
    return {
        0x68, 0x69, 0x76, 0x77, 0x48, 0x49, 0x70, 0x3C, 0x3D,
        0x40, 0x29, 0x20, 0x27, 0x44, 0x45, 0x1E, 0x23, 0x6A,
        0x6B, 0x50, 0x57, 0x60, 0x62, 0x13, 0x39, 0x5A, 0x4A
    };
}

std::string I2CBus::getDeviceName(uint8_t address) {
    return I2C_ADDRESSES::getDeviceName(address);
}

} // namespace EdgeAI
