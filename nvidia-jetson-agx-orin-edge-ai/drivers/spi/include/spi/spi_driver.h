#pragma once
#include <linux/spi/spidev.h>
#include <string>
#include <vector>
#include <functional>
#include <mutex>

namespace EdgeAI {

enum SPIMode {
    SPI_MODE_0 = SPI_MODE_0,  // CPOL=0, CPHA=0
    SPI_MODE_1 = SPI_MODE_1,  // CPOL=0, CPHA=1
    SPI_MODE_2 = SPI_MODE_2,  // CPOL=1, CPHA=0
    SPI_MODE_3 = SPI_MODE_3   // CPOL=1, CPHA=1
};

enum SPIBitOrder {
    SPI_MSB_FIRST = 0,
    SPI_LSB_FIRST = 1
};

struct SPIConfig {
    std::string device = "/dev/spidev0.0";
    uint32_t speed = 1000000;      // 1MHz default
    uint16_t delay = 0;
    uint8_t bits_per_word = 8;
    SPIMode mode = SPI_MODE_0;
    SPIBitOrder bit_order = SPI_MSB_FIRST;
    uint16_t cs_change = 0;
    bool loopback = false;
};

struct SPITransfer {
    std::vector<uint8_t> tx_buffer;
    std::vector<uint8_t> rx_buffer;
    uint32_t speed;
    uint16_t delay;
    uint8_t bits_per_word;
    bool cs_change;
    std::function<void(const std::vector<uint8_t>&)> callback;
};

class SPIDriver {
public:
    SPIDriver(const SPIConfig& config);
    ~SPIDriver();
    
    bool initialize();
    void close();
    bool isOpen() const;
    
    // Basic SPI operations
    uint8_t transferByte(uint8_t tx_data);
    std::vector<uint8_t> transfer(const std::vector<uint8_t>& tx_data);
    void transferAsync(const SPITransfer& transfer);
    
    // Multi-transfer operations
    bool transferMultiple(const std::vector<SPITransfer>& transfers);
    
    // Configuration
    void setSpeed(uint32_t speed);
    void setMode(SPIMode mode);
    void setBitsPerWord(uint8_t bits);
    void setBitOrder(SPIBitOrder order);
    void setCSChange(bool enable);
    
    // Status
    uint32_t getSpeed() const;
    SPIMode getMode() const;
    uint8_t getBitsPerWord() const;
    bool isTransferComplete() const;
    
    // Callbacks
    void setTransferCallback(std::function<void(const std::vector<uint8_t>&)> callback);
    void setErrorCallback(std::function<void(const std::string&)> callback);
    
private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

// SPI Device class for specific peripherals
class SPIDevice {
public:
    SPIDevice(SPIDriver& driver, uint8_t cs_pin);
    ~SPIDevice();
    
    // Device-specific operations
    uint8_t readRegister(uint8_t reg);
    void writeRegister(uint8_t reg, uint8_t value);
    std::vector<uint8_t> readBurst(uint8_t reg, size_t count);
    void writeBurst(uint8_t reg, const std::vector<uint8_t>& data);
    
private:
    SPIDriver* driver_;
    uint8_t cs_pin_;
    std::mutex device_mutex_;
};

} // namespace EdgeAI
