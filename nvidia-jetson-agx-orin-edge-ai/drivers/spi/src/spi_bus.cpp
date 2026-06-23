#include "spi/spi_bus.h"
#include <algorithm>
#include <iostream>

namespace EdgeAI {

class SPIBus::Impl {
public:
    Impl(SPIBus bus) : bus_(bus), initialized_(false) {
        // Create default config for this bus
        SPIConfig config;
        config.bus = bus;
        config.cs = SPI_CS_0;
        config.speed = 1000000;
        config.mode = SPI_MODE_0;
        
        driver_ = std::make_unique<SPIDriver>(config);
    }
    
    ~Impl() {
        close();
    }
    
    bool initialize() {
        if (initialized_) return true;
        
        if (!driver_->initialize()) {
            return false;
        }
        
        initialized_ = true;
        return true;
    }
    
    void close() {
        driver_->close();
        initialized_ = false;
        devices_.clear();
    }
    
    bool isOpen() const {
        return driver_->isOpen();
    }
    
    bool registerDevice(uint8_t cs_pin, const SPIConfig& config) {
        if (!initialized_) return false;
        
        // Check if device already registered
        if (devices_.find(cs_pin) != devices_.end()) {
            return false;
        }
        
        // Create device with custom config
        auto device = std::make_shared<SPIDevice>(*driver_, cs_pin, config.mode);
        device->setSpeed(config.speed);
        
        devices_[cs_pin] = device;
        return true;
    }
    
    bool unregisterDevice(uint8_t cs_pin) {
        auto it = devices_.find(cs_pin);
        if (it == devices_.end()) {
            return false;
        }
        
        devices_.erase(it);
        return true;
    }
    
    std::shared_ptr<SPIDevice> getDevice(uint8_t cs_pin) {
        auto it = devices_.find(cs_pin);
        if (it == devices_.end()) {
            return nullptr;
        }
        return it->second;
    }
    
    std::vector<uint8_t> getRegisteredDevices() const {
        std::vector<uint8_t> pins;
        for (const auto& [pin, device] : devices_) {
            pins.push_back(pin);
        }
        return pins;
    }
    
    bool transfer(uint8_t cs_pin, const std::vector<uint8_t>& tx_data,
                  std::vector<uint8_t>& rx_data) {
        auto device = getDevice(cs_pin);
        if (!device) return false;
        
        rx_data = driver_->transfer(tx_data);
        return !rx_data.empty();
    }
    
    bool transferAsync(uint8_t cs_pin, const SPITransfer& transfer) {
        auto device = getDevice(cs_pin);
        if (!device) return false;
        
        driver_->transferAsync(transfer);
        return true;
    }
    
    bool isBusy() const {
        return !driver_->isTransferComplete();
    }
    
    void setBusSpeed(uint32_t speed) {
        driver_->setSpeed(speed);
    }
    
    void setBusMode(SPIMode mode) {
        driver_->setMode(mode);
    }
    
    uint32_t getBusSpeed() const {
        return driver_->getSpeed();
    }
    
    SPIMode getBusMode() const {
        return driver_->getMode();
    }
    
    std::string getBusInfo() const {
        std::stringstream ss;
        ss << "SPI Bus " << (int)bus_ << " Info:\n";
        ss << "  Open: " << (isOpen() ? "Yes" : "No") << "\n";
        ss << "  Speed: " << getBusSpeed() << " Hz\n";
        ss << "  Mode: " << (int)getBusMode() << "\n";
        ss << "  Devices: " << devices_.size() << "\n";
        return ss.str();
    }
    
    bool testBus() {
        if (!initialized_) return false;
        
        // Run loopback test if available
        return driver_->runLoopbackTest(10);
    }
    
private:
    SPIBus bus_;
    bool initialized_;
    std::unique_ptr<SPIDriver> driver_;
    std::map<uint8_t, std::shared_ptr<SPIDevice>> devices_;
};

// SPIBus implementation
SPIBus::SPIBus(SPIBus bus) : pImpl(std::make_unique<Impl>(bus)) {}
SPIBus::~SPIBus() = default;

bool SPIBus::initialize() { return pImpl->initialize(); }
void SPIBus::close() { pImpl->close(); }
bool SPIBus::isOpen() const { return pImpl->isOpen(); }

bool SPIBus::registerDevice(uint8_t cs_pin, const SPIConfig& config) {
    return pImpl->registerDevice(cs_pin, config);
}
bool SPIBus::unregisterDevice(uint8_t cs_pin) {
    return pImpl->unregisterDevice(cs_pin);
}
std::shared_ptr<SPIDevice> SPIBus::getDevice(uint8_t cs_pin) {
    return pImpl->getDevice(cs_pin);
}
std::vector<uint8_t> SPIBus::getRegisteredDevices() const {
    return pImpl->getRegisteredDevices();
}

bool SPIBus::transfer(uint8_t cs_pin, const std::vector<uint8_t>& tx_data,
                      std::vector<uint8_t>& rx_data) {
    return pImpl->transfer(cs_pin, tx_data, rx_data);
}
bool SPIBus::transferAsync(uint8_t cs_pin, const SPITransfer& transfer) {
    return pImpl->transferAsync(cs_pin, transfer);
}
bool SPIBus::isBusy() const { return pImpl->isBusy(); }

void SPIBus::setBusSpeed(uint32_t speed) { pImpl->setBusSpeed(speed); }
void SPIBus::setBusMode(SPIMode mode) { pImpl->setBusMode(mode); }
uint32_t SPIBus::getBusSpeed() const { return pImpl->getBusSpeed(); }
SPIMode SPIBus::getBusMode() const { return pImpl->getBusMode(); }

std::string SPIBus::getBusInfo() const { return pImpl->getBusInfo(); }
bool SPIBus::testBus() { return pImpl->testBus(); }

} // namespace EdgeAI
