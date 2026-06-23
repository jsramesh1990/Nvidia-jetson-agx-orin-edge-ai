#include "i2c/i2c_device.h"
#include <chrono>
#include <thread>
#include <iostream>

namespace EdgeAI {

class I2CDevice::Impl {
public:
    Impl(I2CDriver& driver, uint8_t address) 
        : driver_(&driver), address_(address), timeout_ms_(1000), retries_(3) {}
    
    ~Impl() = default;
    
    uint8_t readReg(uint8_t reg) {
        std::lock_guard<std::mutex> lock(device_mutex_);
        
        for (int attempt = 0; attempt < retries_; attempt++) {
            try {
                return driver_->readByteData(address_, reg);
            } catch (const std::exception& e) {
                if (attempt == retries_ - 1) {
                    std::cerr << "I2C read failed: " << e.what() << std::endl;
                    return 0;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }
        return 0;
    }
    
    void writeReg(uint8_t reg, uint8_t value) {
        std::lock_guard<std::mutex> lock(device_mutex_);
        
        for (int attempt = 0; attempt < retries_; attempt++) {
            try {
                driver_->writeByteData(address_, reg, value);
                return;
            } catch (const std::exception& e) {
                if (attempt == retries_ - 1) {
                    std::cerr << "I2C write failed: " << e.what() << std::endl;
                    throw;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }
    }
    
    uint16_t readReg16(uint8_t reg) {
        auto data = readRegBlock(reg, 2);
        if (data.size() < 2) return 0;
        return (data[0] << 8) | data[1];
    }
    
    void writeReg16(uint8_t reg, uint16_t value) {
        std::vector<uint8_t> data = {
            static_cast<uint8_t>((value >> 8) & 0xFF),
            static_cast<uint8_t>(value & 0xFF)
        };
        writeRegBlock(reg, data);
    }
    
    uint32_t readReg32(uint8_t reg) {
        auto data = readRegBlock(reg, 4);
        if (data.size() < 4) return 0;
        return (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
    }
    
    void writeReg32(uint8_t reg, uint32_t value) {
        std::vector<uint8_t> data = {
            static_cast<uint8_t>((value >> 24) & 0xFF),
            static_cast<uint8_t>((value >> 16) & 0xFF),
            static_cast<uint8_t>((value >> 8) & 0xFF),
            static_cast<uint8_t>(value & 0xFF)
        };
        writeRegBlock(reg, data);
    }
    
    std::vector<uint8_t> readRegBlock(uint8_t reg, size_t count) {
        std::lock_guard<std::mutex> lock(device_mutex_);
        
        for (int attempt = 0; attempt < retries_; attempt++) {
            try {
                return driver_->readBlockData(address_, reg, count);
            } catch (const std::exception& e) {
                if (attempt == retries_ - 1) {
                    std::cerr << "I2C block read failed: " << e.what() << std::endl;
                    return {};
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }
        return {};
    }
    
    void writeRegBlock(uint8_t reg, const std::vector<uint8_t>& data) {
        std::lock_guard<std::mutex> lock(device_mutex_);
        
        for (int attempt = 0; attempt < retries_; attempt++) {
            try {
                driver_->writeBlockData(address_, reg, data);
                return;
            } catch (const std::exception& e) {
                if (attempt == retries_ - 1) {
                    std::cerr << "I2C block write failed: " << e.what() << std::endl;
                    throw;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }
    }
    
    uint8_t readBit(uint8_t reg, uint8_t bit) {
        uint8_t value = readReg(reg);
        return (value >> bit) & 0x01;
    }
    
    void writeBit(uint8_t reg, uint8_t bit, uint8_t value) {
        uint8_t current = readReg(reg);
        if (value) {
            current |= (1 << bit);
        } else {
            current &= ~(1 << bit);
        }
        writeReg(reg, current);
    }
    
    uint8_t readBits(uint8_t reg, uint8_t start, uint8_t length) {
        uint8_t value = readReg(reg);
        uint8_t mask = (1 << length) - 1;
        return (value >> start) & mask;
    }
    
    void writeBits(uint8_t reg, uint8_t start, uint8_t length, uint8_t value) {
        uint8_t current = readReg(reg);
        uint8_t mask = (1 << length) - 1;
        current &= ~(mask << start);
        current |= (value & mask) << start;
        writeReg(reg, current);
    }
    
    bool deviceId(uint8_t& id) {
        // Generic device ID - override for specific devices
        id = readReg(0x00);
        return true;
    }
    
    bool resetDevice() {
        // Generic reset - override for specific devices
        writeReg(0x00, 0x01);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        return true;
    }
    
    bool isConnected() {
        try {
            uint8_t test = readReg(0x00);
            return true;
        } catch (...) {
            return false;
        }
    }
    
    bool isReady() const {
        return isConnected();
    }
    
    void setAddress(uint8_t address) {
        address_ = address;
    }
    
    void setTimeout(int ms) {
        timeout_ms_ = ms;
        driver_->setTimeout(ms);
    }
    
    void setRetries(int retries) {
        retries_ = retries;
    }
    
    std::string getDeviceInfo() const {
        std::stringstream ss;
        ss << "I2C Device Info:\n";
        ss << "  Address: 0x" << std::hex << (int)address_ << std::dec << "\n";
        ss << "  Timeout: " << timeout_ms_ << " ms\n";
        ss << "  Retries: " << retries_ << "\n";
        return ss.str();
    }
    
    std::vector<uint8_t> writeRead(const std::vector<uint8_t>& write_data,
                                   size_t read_count) {
        std::lock_guard<std::mutex> lock(device_mutex_);
        
        for (int attempt = 0; attempt < retries_; attempt++) {
            try {
                return driver_->writeRead(address_, write_data, read_count);
            } catch (const std::exception& e) {
                if (attempt == retries_ - 1) {
                    std::cerr << "I2C write-read failed: " << e.what() << std::endl;
                    return {};
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }
        return {};
    }
    
private:
    I2CDriver* driver_;
    uint8_t address_;
    int timeout_ms_;
    int retries_;
    std::mutex device_mutex_;
};

// I2CDevice implementation
I2CDevice::I2CDevice(I2CDriver& driver, uint8_t address) 
    : pImpl(std::make_unique<Impl>(driver, address)), 
      address_(address), timeout_ms_(1000), retries_(3) {}

I2CDevice::~I2CDevice() = default;

uint8_t I2CDevice::readReg(uint8_t reg) {
    return pImpl->readReg(reg);
}

void I2CDevice::writeReg(uint8_t reg, uint8_t value) {
    pImpl->writeReg(reg, value);
}

uint16_t I2CDevice::readReg16(uint8_t reg) {
    return pImpl->readReg16(reg);
}

void I2CDevice::writeReg16(uint8_t reg, uint16_t value) {
    pImpl->writeReg16(reg, value);
}

uint32_t I2CDevice::readReg32(uint8_t reg) {
    return pImpl->readReg32(reg);
}

void I2CDevice::writeReg32(uint8_t reg, uint32_t value) {
    pImpl->writeReg32(reg, value);
}

std::vector<uint8_t> I2CDevice::readRegBlock(uint8_t reg, size_t count) {
    return pImpl->readRegBlock(reg, count);
}

void I2CDevice::writeRegBlock(uint8_t reg, const std::vector<uint8_t>& data) {
    pImpl->writeRegBlock(reg, data);
}

uint8_t I2CDevice::readBit(uint8_t reg, uint8_t bit) {
    return pImpl->readBit(reg, bit);
}

void I2CDevice::writeBit(uint8_t reg, uint8_t bit, uint8_t value) {
    pImpl->writeBit(reg, bit, value);
}

uint8_t I2CDevice::readBits(uint8_t reg, uint8_t start, uint8_t length) {
    return pImpl->readBits(reg, start, length);
}

void I2CDevice::writeBits(uint8_t reg, uint8_t start, uint8_t length, uint8_t value) {
    pImpl->writeBits(reg, start, length, value);
}

bool I2CDevice::deviceId(uint8_t& id) {
    return pImpl->deviceId(id);
}

bool I2CDevice::resetDevice() {
    return pImpl->resetDevice();
}

bool I2CDevice::isConnected() {
    return pImpl->isConnected();
}

bool I2CDevice::isReady() const {
    return pImpl->isReady();
}

void I2CDevice::setAddress(uint8_t address) {
    address_ = address;
    pImpl->setAddress(address);
}

uint8_t I2CDevice::getAddress() const {
    return address_;
}

void I2CDevice::setTimeout(int ms) {
    timeout_ms_ = ms;
    pImpl->setTimeout(ms);
}

void I2CDevice::setRetries(int retries) {
    retries_ = retries;
    pImpl->setRetries(retries);
}

std::string I2CDevice::getDeviceInfo() const {
    return pImpl->getDeviceInfo();
}

std::vector<uint8_t> I2CDevice::writeRead(const std::vector<uint8_t>& write_data,
                                          size_t read_count) {
    return pImpl->writeRead(write_data, read_count);
}

} // namespace EdgeAI
