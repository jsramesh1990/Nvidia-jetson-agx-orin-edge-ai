#include "spi/spi_device.h"
#include <cstring>
#include <chrono>
#include <thread>

namespace EdgeAI {

class SPIDevice::Impl {
public:
    Impl(SPIDriver& driver, uint8_t cs_pin, SPIMode mode) 
        : driver_(&driver), cs_pin_(cs_pin), mode_(mode),
          speed_(1000000), timeout_ms_(1000) {
        // Configure device
        configure();
    }
    
    ~Impl() = default;
    
    bool configure() {
        // Set SPI mode for this device
        driver_->setMode(mode_);
        driver_->setSpeed(speed_);
        return true;
    }
    
    uint8_t readRegister(uint8_t reg) {
        std::lock_guard<std::mutex> lock(device_mutex_);
        
        // SPI read: send register address with read bit (0x80)
        std::vector<uint8_t> tx = {static_cast<uint8_t>(reg | 0x80), 0x00};
        auto rx = driver_->transfer(tx);
        return rx.empty() ? 0 : rx[1];
    }
    
    void writeRegister(uint8_t reg, uint8_t value) {
        std::lock_guard<std::mutex> lock(device_mutex_);
        
        // SPI write: send register address and data
        std::vector<uint8_t> tx = {reg, value};
        driver_->transfer(tx);
    }
    
    uint16_t readRegister16(uint8_t reg) {
        std::lock_guard<std::mutex> lock(device_mutex_);
        
        std::vector<uint8_t> tx = {static_cast<uint8_t>(reg | 0x80), 0x00, 0x00};
        auto rx = driver_->transfer(tx);
        if (rx.size() < 3) return 0;
        return (rx[1] << 8) | rx[2];
    }
    
    void writeRegister16(uint8_t reg, uint16_t value) {
        std::lock_guard<std::mutex> lock(device_mutex_);
        
        std::vector<uint8_t> tx = {reg, 
                                   static_cast<uint8_t>((value >> 8) & 0xFF),
                                   static_cast<uint8_t>(value & 0xFF)};
        driver_->transfer(tx);
    }
    
    std::vector<uint8_t> readBurst(uint8_t reg, size_t count) {
        std::lock_guard<std::mutex> lock(device_mutex_);
        
        std::vector<uint8_t> tx(count + 1);
        tx[0] = static_cast<uint8_t>(reg | 0x80);
        std::fill(tx.begin() + 1, tx.end(), 0x00);
        
        auto rx = driver_->transfer(tx);
        if (rx.size() <= 1) return {};
        
        return std::vector<uint8_t>(rx.begin() + 1, rx.end());
    }
    
    void writeBurst(uint8_t reg, const std::vector<uint8_t>& data) {
        std::lock_guard<std::mutex> lock(device_mutex_);
        
        std::vector<uint8_t> tx(1 + data.size());
        tx[0] = reg;
        std::copy(data.begin(), data.end(), tx.begin() + 1);
        
        driver_->transfer(tx);
    }
    
    std::vector<uint8_t> readBuffer(uint8_t reg, size_t count) {
        return readBurst(reg, count);
    }
    
    void writeBuffer(uint8_t reg, const std::vector<uint8_t>& data) {
        writeBurst(reg, data);
    }
    
    bool deviceId(uint8_t& id) {
        // Generic device ID read - override for specific devices
        id = readRegister(0x00);
        return true;
    }
    
    bool resetDevice() {
        // Generic reset - override for specific devices
        writeRegister(0x00, 0x01);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        return true;
    }
    
    bool powerDown() {
        // Generic power down - override for specific devices
        writeRegister(0x01, 0x00);
        return true;
    }
    
    bool powerUp() {
        // Generic power up - override for specific devices
        writeRegister(0x01, 0x01);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        return true;
    }
    
    void setMode(SPIMode mode) {
        mode_ = mode;
        driver_->setMode(mode);
    }
    
    void setSpeed(uint32_t speed) {
        speed_ = speed;
        driver_->setSpeed(speed);
    }
    
    void setTimeout(int ms) {
        timeout_ms_ = ms;
    }
    
    bool isReady() const {
        // Check if device is ready
        try {
            uint8_t id;
            return deviceId(id);
        } catch (...) {
            return false;
        }
    }
    
    std::string getDeviceInfo() const {
        std::stringstream ss;
        ss << "SPI Device Info:\n";
        ss << "  CS Pin: " << (int)cs_pin_ << "\n";
        ss << "  Mode: " << (int)mode_ << "\n";
        ss << "  Speed: " << speed_ << " Hz\n";
        ss << "  Timeout: " << timeout_ms_ << " ms\n";
        return ss.str();
    }
    
private:
    SPIDriver* driver_;
    uint8_t cs_pin_;
    SPIMode mode_;
    uint32_t speed_;
    int timeout_ms_;
    std::mutex device_mutex_;
};

// SPIDevice implementation
SPIDevice::SPIDevice(SPIDriver& driver, uint8_t cs_pin, SPIMode mode) 
    : pImpl(std::make_unique<Impl>(driver, cs_pin, mode)) {}

SPIDevice::~SPIDevice() = default;

uint8_t SPIDevice::readRegister(uint8_t reg) {
    return pImpl->readRegister(reg);
}

void SPIDevice::writeRegister(uint8_t reg, uint8_t value) {
    pImpl->writeRegister(reg, value);
}

uint16_t SPIDevice::readRegister16(uint8_t reg) {
    return pImpl->readRegister16(reg);
}

void SPIDevice::writeRegister16(uint8_t reg, uint16_t value) {
    pImpl->writeRegister16(reg, value);
}

std::vector<uint8_t> SPIDevice::readBurst(uint8_t reg, size_t count) {
    return pImpl->readBurst(reg, count);
}

void SPIDevice::writeBurst(uint8_t reg, const std::vector<uint8_t>& data) {
    pImpl->writeBurst(reg, data);
}

std::vector<uint8_t> SPIDevice::readBuffer(uint8_t reg, size_t count) {
    return pImpl->readBuffer(reg, count);
}

void SPIDevice::writeBuffer(uint8_t reg, const std::vector<uint8_t>& data) {
    pImpl->writeBuffer(reg, data);
}

bool SPIDevice::deviceId(uint8_t& id) {
    return pImpl->deviceId(id);
}

bool SPIDevice::resetDevice() {
    return pImpl->resetDevice();
}

bool SPIDevice::powerDown() {
    return pImpl->powerDown();
}

bool SPIDevice::powerUp() {
    return pImpl->powerUp();
}

void SPIDevice::setMode(SPIMode mode) {
    pImpl->setMode(mode);
}

void SPIDevice::setSpeed(uint32_t speed) {
    pImpl->setSpeed(speed);
}

void SPIDevice::setTimeout(int ms) {
    pImpl->setTimeout(ms);
}

bool SPIDevice::isReady() const {
    return pImpl->isReady();
}

std::string SPIDevice::getDeviceInfo() const {
    return pImpl->getDeviceInfo();
}

} // namespace EdgeAI
