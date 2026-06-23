#pragma once
#include <linux/spi/spidev.h>
#include <string>
#include <vector>
#include <functional>
#include <mutex>
#include <queue>
#include <atomic>
#include <thread>

namespace EdgeAI {

enum SPIMode {
    SPI_MODE_0 = SPI_MODE_0,
    SPI_MODE_1 = SPI_MODE_1,
    SPI_MODE_2 = SPI_MODE_2,
    SPI_MODE_3 = SPI_MODE_3
};

enum SPIBitOrder {
    SPI_MSB_FIRST = 0,
    SPI_LSB_FIRST = 1
};

enum SPIBus {
    SPI_BUS_0 = 0,
    SPI_BUS_1 = 1,
    SPI_BUS_2 = 2,
    SPI_BUS_3 = 3,
    SPI_BUS_4 = 4
};

enum SPIChipSelect {
    SPI_CS_0 = 0,
    SPI_CS_1 = 1,
    SPI_CS_2 = 2,
    SPI_CS_3 = 3
};

struct SPIConfig {
    SPIBus bus = SPI_BUS_0;
    SPIChipSelect cs = SPI_CS_0;
    uint32_t speed = 1000000;      // 1MHz default
    uint16_t delay = 0;
    uint8_t bits_per_word = 8;
    SPIMode mode = SPI_MODE_0;
    SPIBitOrder bit_order = SPI_MSB_FIRST;
    bool cs_change = false;
    bool loopback = false;
    bool no_cs = false;
    bool ready = false;
    int timeout_ms = 1000;
};

struct SPITransfer {
    std::vector<uint8_t> tx_buffer;
    std::vector<uint8_t> rx_buffer;
    uint32_t speed = 0;
    uint16_t delay = 0;
    uint8_t bits_per_word = 0;
    bool cs_change = false;
    std::function<void(const std::vector<uint8_t>&)> callback;
    std::function<void(bool)> completion_callback;
};

class SPIDriver {
public:
    SPIDriver(const SPIConfig& config);
    ~SPIDriver();
    
    // Initialization
    bool initialize();
    void close();
    bool isOpen() const;
    bool isReady() const;
    
    // Basic operations
    uint8_t transferByte(uint8_t tx_data);
    std::vector<uint8_t> transfer(const std::vector<uint8_t>& tx_data);
    std::vector<uint8_t> transferMultiple(const std::vector<SPITransfer>& transfers);
    
    // Asynchronous operations
    void transferAsync(const SPITransfer& transfer);
    bool transferAsyncMultiple(const std::vector<SPITransfer>& transfers);
    void waitForCompletion();
    bool isTransferComplete() const;
    size_t getPendingTransfers() const;
    
    // Configuration
    void setSpeed(uint32_t speed);
    void setMode(SPIMode mode);
    void setBitsPerWord(uint8_t bits);
    void setBitOrder(SPIBitOrder order);
    void setCSChange(bool enable);
    void setDelay(uint16_t delay);
    void setTimeout(int ms);
    
    // Status
    uint32_t getSpeed() const;
    SPIMode getMode() const;
    uint8_t getBitsPerWord() const;
    SPIBitOrder getBitOrder() const;
    SPIConfig getConfig() const;
    uint32_t getTransferCount() const;
    uint32_t getErrorCount() const;
    
    // Callbacks
    void setTransferCallback(std::function<void(const std::vector<uint8_t>&)> callback);
    void setErrorCallback(std::function<void(const std::string&)> callback);
    void setCompletionCallback(std::function<void(bool)> callback);
    
    // Utility
    bool testSPI();
    bool runLoopbackTest(int iterations = 100);
    void dumpRegisters(uint8_t start_reg, uint8_t count);
    std::string getStatusString() const;
    
private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

// SPI Device class for specific peripherals
class SPIDevice {
public:
    SPIDevice(SPIDriver& driver, uint8_t cs_pin, SPIMode mode = SPI_MODE_0);
    ~SPIDevice();
    
    // Device operations
    uint8_t readRegister(uint8_t reg);
    void writeRegister(uint8_t reg, uint8_t value);
    uint16_t readRegister16(uint8_t reg);
    void writeRegister16(uint8_t reg, uint16_t value);
    std::vector<uint8_t> readBurst(uint8_t reg, size_t count);
    void writeBurst(uint8_t reg, const std::vector<uint8_t>& data);
    std::vector<uint8_t> readBuffer(uint8_t reg, size_t count);
    void writeBuffer(uint8_t reg, const std::vector<uint8_t>& data);
    
    // Device-specific operations
    bool deviceId(uint8_t& id);
    bool resetDevice();
    bool powerDown();
    bool powerUp();
    
    // Configuration
    void setMode(SPIMode mode);
    void setSpeed(uint32_t speed);
    void setTimeout(int ms);
    
    // Status
    bool isReady() const;
    std::string getDeviceInfo() const;
    
private:
    SPIDriver* driver_;
    uint8_t cs_pin_;
    SPIMode mode_;
    std::mutex device_mutex_;
    uint32_t speed_;
    int timeout_ms_;
};

} // namespace EdgeAI
