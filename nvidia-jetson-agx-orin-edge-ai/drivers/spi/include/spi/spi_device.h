#pragma once
#include "spi_driver.h"
#include <memory>
#include <vector>
#include <cstdint>
#include <string>

namespace EdgeAI {

/**
 * @brief SPI Device class for managing specific SPI peripherals
 * 
 * This class provides high-level access to SPI devices connected to the bus.
 * It handles device-specific operations like register reads/writes,
 * burst transfers, and device management.
 */
class SPIDevice {
public:
    /**
     * @brief Constructor for SPI Device
     * @param driver Reference to the SPI driver
     * @param cs_pin Chip select pin number (0-3)
     * @param mode SPI mode (0-3)
     */
    SPIDevice(SPIDriver& driver, uint8_t cs_pin, SPIMode mode = SPI_MODE_0);
    ~SPIDevice();

    // Disable copy
    SPIDevice(const SPIDevice&) = delete;
    SPIDevice& operator=(const SPIDevice&) = delete;

    // Enable move
    SPIDevice(SPIDevice&&) = default;
    SPIDevice& operator=(SPIDevice&&) = default;

    /**
     * @brief Read a single 8-bit register
     * @param reg Register address
     * @return Register value (0 on error)
     */
    uint8_t readRegister(uint8_t reg);

    /**
     * @brief Write a single 8-bit register
     * @param reg Register address
     * @param value Value to write
     */
    void writeRegister(uint8_t reg, uint8_t value);

    /**
     * @brief Read a 16-bit register (two bytes)
     * @param reg Register address
     * @return 16-bit register value (0 on error)
     */
    uint16_t readRegister16(uint8_t reg);

    /**
     * @brief Write a 16-bit register
     * @param reg Register address
     * @param value 16-bit value to write
     */
    void writeRegister16(uint8_t reg, uint16_t value);

    /**
     * @brief Read a 32-bit register (four bytes)
     * @param reg Register address
     * @return 32-bit register value (0 on error)
     */
    uint32_t readRegister32(uint8_t reg);

    /**
     * @brief Write a 32-bit register
     * @param reg Register address
     * @param value 32-bit value to write
     */
    void writeRegister32(uint8_t reg, uint32_t value);

    /**
     * @brief Read multiple registers in burst mode
     * @param reg Starting register address
     * @param count Number of bytes to read
     * @return Vector of bytes read
     */
    std::vector<uint8_t> readBurst(uint8_t reg, size_t count);

    /**
     * @brief Write multiple registers in burst mode
     * @param reg Starting register address
     * @param data Data to write
     */
    void writeBurst(uint8_t reg, const std::vector<uint8_t>& data);

    /**
     * @brief Read data buffer
     * @param reg Register address
     * @param count Number of bytes to read
     * @return Vector of bytes read
     */
    std::vector<uint8_t> readBuffer(uint8_t reg, size_t count);

    /**
     * @brief Write data buffer
     * @param reg Register address
     * @param data Data to write
     */
    void writeBuffer(uint8_t reg, const std::vector<uint8_t>& data);

    /**
     * @brief Read device ID
     * @param id Reference to store device ID
     * @return true if successful
     */
    bool deviceId(uint8_t& id);

    /**
     * @brief Reset the device
     * @return true if successful
     */
    bool resetDevice();

    /**
     * @brief Put device in power-down mode
     * @return true if successful
     */
    bool powerDown();

    /**
     * @brief Wake device from power-down
     * @return true if successful
     */
    bool powerUp();

    /**
     * @brief Check if device is ready
     * @return true if device is ready
     */
    bool isReady() const;

    /**
     * @brief Set SPI mode for this device
     * @param mode SPI mode (0-3)
     */
    void setMode(SPIMode mode);

    /**
     * @brief Set SPI speed for this device
     * @param speed Speed in Hz
     */
    void setSpeed(uint32_t speed);

    /**
     * @brief Set timeout for operations
     * @param ms Timeout in milliseconds
     */
    void setTimeout(int ms);

    /**
     * @brief Get device information string
     * @return Device information
     */
    std::string getDeviceInfo() const;

    /**
     * @brief Get the chip select pin
     * @return CS pin number
     */
    uint8_t getCSPin() const { return cs_pin_; }

    /**
     * @brief Get current SPI mode
     * @return SPI mode
     */
    SPIMode getMode() const { return mode_; }

    /**
     * @brief Get current SPI speed
     * @return Speed in Hz
     */
    uint32_t getSpeed() const { return speed_; }

    /**
     * @brief Get current timeout
     * @return Timeout in milliseconds
     */
    int getTimeout() const { return timeout_ms_; }

    /**
     * @brief Check if device is connected
     * @return true if device is connected
     */
    bool isConnected();

    /**
     * @brief Perform a generic SPI transfer
     * @param tx_data Data to transmit
     * @return Received data
     */
    std::vector<uint8_t> transfer(const std::vector<uint8_t>& tx_data);

    /**
     * @brief Perform a generic SPI transfer with custom parameters
     * @param tx_data Data to transmit
     * @param speed Speed in Hz (0 = use device speed)
     * @param delay Delay in microseconds
     * @param bits_per_word Bits per word (0 = use default)
     * @return Received data
     */
    std::vector<uint8_t> transferEx(const std::vector<uint8_t>& tx_data,
                                    uint32_t speed = 0,
                                    uint16_t delay = 0,
                                    uint8_t bits_per_word = 0);

    /**
     * @brief Perform an asynchronous SPI transfer
     * @param transfer Transfer configuration
     * @return true if queued successfully
     */
    bool transferAsync(const SPITransfer& transfer);

private:
    class Impl;
    std::unique_ptr<Impl> pImpl;

    // Device parameters
    uint8_t cs_pin_;
    SPIMode mode_;
    uint32_t speed_;
    int timeout_ms_;
};

// Common SPI device types
namespace SPI_DEVICES {

/**
 * @brief MCP3008 8-channel 10-bit ADC
 */
class MCP3008 : public SPIDevice {
public:
    MCP3008(SPIDriver& driver, uint8_t cs_pin);
    uint16_t readChannel(uint8_t channel);
    std::vector<uint16_t> readAllChannels();
};

/**
 * @brief MCP23S17 16-bit I/O expander
 */
class MCP23S17 : public SPIDevice {
public:
    MCP23S17(SPIDriver& driver, uint8_t cs_pin);
    
    // I/O configuration
    void setDirection(uint8_t port, uint8_t direction);
    void setPullup(uint8_t port, uint8_t pullup);
    void writePort(uint8_t port, uint8_t value);
    uint8_t readPort(uint8_t port);
    void setBit(uint8_t port, uint8_t bit, bool value);
    bool getBit(uint8_t port, uint8_t bit);
};

/**
 * @brief MAX31855 K-type thermocouple amplifier
 */
class MAX31855 : public SPIDevice {
public:
    MAX31855(SPIDriver& driver, uint8_t cs_pin);
    double readTemperature();
    double readInternalTemperature();
    bool isFault();
    uint8_t getFaultCode();
};

/**
 * @brief ADXL345 3-axis accelerometer
 */
class ADXL345 : public SPIDevice {
public:
    ADXL345(SPIDriver& driver, uint8_t cs_pin);
    
    struct AccelData {
        float x;
        float y;
        float z;
    };
    
    bool initialize();
    AccelData readAcceleration();
    void setRange(uint8_t range);
    void setSampleRate(uint8_t rate);
};

/**
 * @brief HMC5883L 3-axis magnetometer
 */
class HMC5883L : public SPIDevice {
public:
    HMC5883L(SPIDriver& driver, uint8_t cs_pin);
    
    struct MagData {
        float x;
        float y;
        float z;
    };
    
    bool initialize();
    MagData readMagnetic();
    void setRange(float range);
    void setSampleRate(uint8_t rate);
};

/**
 * @brief MFRC522 RFID/NFC reader
 */
class MFRC522 : public SPIDevice {
public:
    MFRC522(SPIDriver& driver, uint8_t cs_pin);
    
    struct UID {
        uint8_t bytes[10];
        uint8_t size;
    };
    
    bool initialize();
    bool detectCard();
    bool readUID(UID& uid);
    bool selectCard(const UID& uid);
    bool authenticate(uint8_t block, uint8_t key_type, const uint8_t* key);
    bool readBlock(uint8_t block, uint8_t* data);
    bool writeBlock(uint8_t block, const uint8_t* data);
    bool stopCrypto();
};

/**
 * @brief RFM69HW LoRa transceiver
 */
class RFM69HW : public SPIDevice {
public:
    RFM69HW(SPIDriver& driver, uint8_t cs_pin);
    
    struct Packet {
        std::vector<uint8_t> data;
        int rssi;
        int snr;
        uint8_t lna;
    };
    
    bool initialize();
    bool send(const std::vector<uint8_t>& data);
    bool receive(Packet& packet, int timeout_ms = 1000);
    void setFrequency(uint32_t freq);
    void setPower(uint8_t power);
    void setDataRate(uint32_t rate);
    void setEncryption(const uint8_t* key, size_t length);
};

/**
 * @brief MAX7219 LED matrix driver
 */
class MAX7219 : public SPIDevice {
public:
    MAX7219(SPIDriver& driver, uint8_t cs_pin);
    
    bool initialize();
    void setBrightness(uint8_t brightness);
    void setDecodeMode(uint8_t mode);
    void setDisplayTest(bool enable);
    void setShutdown(bool enable);
    void setDigit(uint8_t digit, uint8_t value);
    void clear();
    void writeRow(uint8_t row, uint8_t value);
    void writeMatrix(const uint8_t data[8][8]);
};

} // namespace SPI_DEVICES

} // namespace EdgeAI
