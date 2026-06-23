#pragma once
#include <string>
#include <vector>
#include <functional>
#include <mutex>
#include <linux/i2c.h>

namespace EdgeAI {

struct I2CConfig {
    std::string device = "/dev/i2c-1";  // I2C1 on Jetson
    int speed = 100000;  // 100kHz default
    uint8_t address = 0x00;
    bool ten_bit = false;
    bool use_smbus = false;
    int timeout_ms = 1000;
};

struct I2CTransaction {
    uint8_t address;
    std::vector<uint8_t> write_data;
    std::vector<uint8_t> read_data;
    bool use_smbus = false;
    std::function<void(bool, const std::vector<uint8_t>&)> callback;
};

class I2CDriver {
public:
    I2CDriver(const I2CConfig& config);
    ~I2CDriver();
    
    bool initialize();
    void close();
    bool isOpen() const;
    
    // Basic I2C operations
    bool write(uint8_t address, const std::vector<uint8_t>& data);
    bool writeByte(uint8_t address, uint8_t byte);
    bool writeByteData(uint8_t address, uint8_t command, uint8_t data);
    bool writeBlockData(uint8_t address, uint8_t command, 
                        const std::vector<uint8_t>& data);
    
    std::vector<uint8_t> read(uint8_t address, size_t count);
    uint8_t readByte(uint8_t address);
    uint8_t readByteData(uint8_t address, uint8_t command);
    std::vector<uint8_t> readBlockData(uint8_t address, uint8_t command, 
                                       size_t count);
    
    // Combined write/read
    std::vector<uint8_t> writeRead(uint8_t address,
                                   const std::vector<uint8_t>& write_data,
                                   size_t read_count);
    
    // Async operations
    bool readAsync(uint8_t address, size_t count,
                   std::function<void(const std::vector<uint8_t>&)> callback);
    bool writeAsync(uint8_t address, const std::vector<uint8_t>& data,
                    std::function<void(bool)> callback);
    bool transactionAsync(const I2CTransaction& transaction);
    
    // Configuration
    void setSpeed(int speed);
    void setAddress(uint8_t address);
    void setTenBit(bool enable);
    void setTimeout(int ms);
    
    // Status
    int getSpeed() const;
    uint8_t getAddress() const;
    bool isTenBit() const;
    
    // Utility
    bool scanBus(std::vector<uint8_t>& found_addresses);
    
    // Callbacks
    void setErrorCallback(std::function<void(const std::string&)> callback);
    
private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

// I2C Device class for specific peripherals
class I2CDevice {
public:
    I2CDevice(I2CDriver& driver, uint8_t address);
    ~I2CDevice();
    
    // Generic register access
    uint8_t readReg(uint8_t reg);
    void writeReg(uint8_t reg, uint8_t value);
    uint16_t readReg16(uint8_t reg);
    void writeReg16(uint8_t reg, uint16_t value);
    std::vector<uint8_t> readRegBlock(uint8_t reg, size_t count);
    void writeRegBlock(uint8_t reg, const std::vector<uint8_t>& data);
    
    // Device-specific operations for common sensors
    // (can be extended for specific devices)
    int16_t readTwoBytes(uint8_t reg);
    float readFloat(uint8_t reg);
    
private:
    I2CDriver* driver_;
    uint8_t address_;
    std::mutex device_mutex_;
};

} // namespace EdgeAI
