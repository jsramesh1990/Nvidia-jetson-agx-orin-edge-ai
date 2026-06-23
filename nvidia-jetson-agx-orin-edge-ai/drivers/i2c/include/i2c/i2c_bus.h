#pragma once
#include "i2c_driver.h"
#include "i2c_device.h"
#include <vector>
#include <map>
#include <memory>
#include <string>
#include <functional>
#include <atomic>
#include <mutex>

namespace EdgeAI {

/**
 * @brief I2C Bus Manager
 * 
 * This class manages multiple I2C devices on the same bus.
 * It handles device registration, scanning, and bus-level operations.
 */
class I2CBus {
public:
    /**
     * @brief Constructor for I2C Bus
     * @param bus_number I2C bus number (0-7)
     */
    I2CBus(int bus_number = 1);
    ~I2CBus();

    // Disable copy
    I2CBus(const I2CBus&) = delete;
    I2CBus& operator=(const I2CBus&) = delete;

    // Enable move
    I2CBus(I2CBus&&) = default;
    I2CBus& operator=(I2CBus&&) = default;

    /**
     * @brief Initialize the I2C bus
     * @return true if successful
     */
    bool initialize();

    /**
     * @brief Close the I2C bus
     */
    void close();

    /**
     * @brief Check if bus is open
     * @return true if open
     */
    bool isOpen() const;

    /**
     * @brief Check if bus is ready
     * @return true if ready
     */
    bool isReady() const;

    /**
     * @brief Register a device on the bus
     * @param address I2C address of the device
     * @param config I2C configuration for the device
     * @return true if registered successfully
     */
    bool registerDevice(uint8_t address, const I2CConfig& config);

    /**
     * @brief Unregister a device from the bus
     * @param address I2C address of the device
     * @return true if unregistered successfully
     */
    bool unregisterDevice(uint8_t address);

    /**
     * @brief Get a registered device
     * @param address I2C address of the device
     * @return Shared pointer to the device, or nullptr if not found
     */
    std::shared_ptr<I2CDevice> getDevice(uint8_t address);

    /**
     * @brief Get all registered devices
     * @return Vector of I2C addresses
     */
    std::vector<uint8_t> getRegisteredDevices() const;

    /**
     * @brief Scan the I2C bus for devices
     * @param found_addresses Vector to store found addresses
     * @return true if devices found
     */
    bool scanBus(std::vector<uint8_t>& found_addresses);

    /**
     * @brief Probe a specific address
     * @param address I2C address to probe
     * @return true if device responds
     */
    bool probeDevice(uint8_t address);

    /**
     * @brief Perform a write operation on the bus
     * @param address I2C device address
     * @param data Data to write
     * @return true if write successful
     */
    bool write(uint8_t address, const std::vector<uint8_t>& data);

    /**
     * @brief Perform a read operation on the bus
     * @param address I2C device address
     * @param count Number of bytes to read
     * @return Vector of bytes read
     */
    std::vector<uint8_t> read(uint8_t address, size_t count);

    /**
     * @brief Perform a combined write-read operation
     * @param address I2C device address
     * @param write_data Data to write
     * @param read_count Number of bytes to read
     * @return Vector of bytes read
     */
    std::vector<uint8_t> writeRead(uint8_t address,
                                   const std::vector<uint8_t>& write_data,
                                   size_t read_count);

    /**
     * @brief Perform an asynchronous operation
     * @param transaction I2C transaction to perform
     * @return true if queued successfully
     */
    bool transactionAsync(const I2CTransaction& transaction);

    /**
     * @brief Check if bus is busy
     * @return true if busy
     */
    bool isBusy() const;

    /**
     * @brief Wait for bus to be free
     * @param timeout_ms Maximum time to wait in milliseconds
     * @return true if bus became free
     */
    bool waitForBus(int timeout_ms = 1000);

    /**
     * @brief Set bus speed
     * @param speed Speed in Hz (100000, 400000, 1000000, 3400000)
     */
    void setBusSpeed(int speed);

    /**
     * @brief Get bus speed
     * @return Speed in Hz
     */
    int getBusSpeed() const;

    /**
     * @brief Set bus timeout
     * @param ms Timeout in milliseconds
     */
    void setTimeout(int ms);

    /**
     * @brief Get bus timeout
     * @return Timeout in milliseconds
     */
    int getTimeout() const;

    /**
     * @brief Set retry count
     * @param retries Number of retries
     */
    void setRetries(int retries);

    /**
     * @brief Get retry count
     * @return Number of retries
     */
    int getRetries() const;

    /**
     * @brief Enable/disable SMBus mode
     * @param enable true to enable SMBus
     */
    void setSMBus(bool enable);

    /**
     * @brief Check if SMBus is enabled
     * @return true if SMBus enabled
     */
    bool isSMBus() const;

    /**
     * @brief Reset the I2C bus
     * @return true if reset successful
     */
    bool resetBus();

    /**
     * @brief Get bus information string
     * @return Bus information
     */
    std::string getBusInfo() const;

    /**
     * @brief Test the I2C bus
     * @return true if test passes
     */
    bool testBus();

    /**
     * @brief Get bus statistics
     */
    struct BusStats {
        size_t total_transactions = 0;
        size_t failed_transactions = 0;
        size_t total_bytes_read = 0;
        size_t total_bytes_written = 0;
        double avg_transaction_time_ms = 0.0;
        size_t active_devices = 0;
        double bus_utilization_percent = 0.0;
        size_t bus_errors = 0;
        size_t timeouts = 0;
        size_t nacks = 0;
    };
    BusStats getStats() const;
    void resetStats();

    /**
     * @brief Set callback for transaction completion
     * @param callback Function called on transaction completion
     */
    void setCompletionCallback(std::function<void(bool, uint8_t)> callback);

    /**
     * @brief Set callback for bus errors
     * @param callback Function called on error
     */
    void setErrorCallback(std::function<void(const std::string&)> callback);

    /**
     * @brief Set callback for bus transactions
     * @param callback Function called for each transaction
     */
    void setTransactionCallback(std::function<void(uint8_t, bool, size_t)> callback);

    /**
     * @brief Set callback for device detection
     * @param callback Function called when device is detected
     */
    void setDeviceDetectionCallback(std::function<void(uint8_t, bool)> callback);

    /**
     * @brief Enable/disable automatic device detection
     * @param enable true to enable
     * @param interval_ms Detection interval in milliseconds
     */
    void setAutoDetection(bool enable, int interval_ms = 10000);

    /**
     * @brief Dump bus registers (debug)
     * @param address I2C device address
     * @param start_reg Starting register
     * @param count Number of registers to dump
     */
    void dumpRegisters(uint8_t address, uint8_t start_reg, uint8_t count);

    /**
     * @brief Get list of known device addresses
     * @return Vector of known addresses
     */
    static std::vector<uint8_t> getKnownAddresses();

    /**
     * @brief Get device name from address
     * @param address I2C address
     * @return Device name string
     */
    static std::string getDeviceName(uint8_t address);

private:
    class Impl;
    std::unique_ptr<Impl> pImpl;

    int bus_number_;
    std::map<uint8_t, std::shared_ptr<I2CDevice>> devices_;
    mutable std::mutex bus_mutex_;
    std::atomic<bool> auto_detect_{false};
    std::thread detect_thread_;
    std::function<void(uint8_t, bool)> detection_callback_;

    // Statistics
    struct Stats {
        std::atomic<size_t> total_transactions{0};
        std::atomic<size_t> failed_transactions{0};
        std::atomic<size_t> total_bytes_read{0};
        std::atomic<size_t> total_bytes_written{0};
        std::atomic<size_t> bus_errors{0};
        std::atomic<size_t> timeouts{0};
        std::atomic<size_t> nacks{0};
        double avg_transaction_time_ms = 0.0;
        mutable std::mutex stats_mutex;
    };
    Stats stats_;

    void detectionLoop();
    void updateStats(double duration_ms, bool success, size_t bytes, bool is_read);
};

// Known I2C device addresses
namespace I2C_ADDRESSES {
    // Common device addresses
    const uint8_t MPU6050         = 0x68;
    const uint8_t MPU6050_ALT     = 0x69;
    const uint8_t BMP280          = 0x76;
    const uint8_t BMP280_ALT      = 0x77;
    const uint8_t BME280          = 0x76;
    const uint8_t BME280_ALT      = 0x77;
    const uint8_t ADS1115         = 0x48;
    const uint8_t ADS1115_ALT     = 0x49;
    const uint8_t TCA9548A        = 0x70;
    const uint8_t SSD1306         = 0x3C;
    const uint8_t SSD1306_ALT     = 0x3D;
    const uint8_t PCA9685         = 0x40;
    const uint8_t DS3231          = 0x68;
    const uint8_t TMP117          = 0x48;
    const uint8_t VL53L1X         = 0x29;
    const uint8_t MCP23017        = 0x20;
    const uint8_t MCP23017_ALT    = 0x27;
    const uint8_t SHT31           = 0x44;
    const uint8_t SHT31_ALT       = 0x45;
    const uint8_t HMC5883L        = 0x1E;
    const uint8_t BH1750          = 0x23;
    const uint8_t LSM9DS0         = 0x6A;
    const uint8_t LSM9DS0_ALT     = 0x6B;
    const uint8_t AT24C256        = 0x50;
    const uint8_t AT24C256_ALT    = 0x57;
    const uint8_t PCF8574         = 0x20;
    const uint8_t PCF8574_ALT     = 0x27;
    const uint8_t MCP4725         = 0x60;
    const uint8_t MCP4725_ALT     = 0x62;
    const uint8_t VCNL4010        = 0x13;
    const uint8_t APDS9960        = 0x39;
    const uint8_t DRV2605         = 0x5A;
    const uint8_t ISL29125        = 0x44;
    const uint8_t MAX44009        = 0x4A;
    const uint8_t SI7021          = 0x40;
    const uint8_t L3GD20          = 0x6A;
    const uint8_t L3GD20_ALT      = 0x6B;

    // Get device name
    inline std::string getDeviceName(uint8_t address) {
        switch (address) {
            case MPU6050: return "MPU6050";
            case BMP280: return "BMP280";
            case BME280: return "BME280";
            case ADS1115: return "ADS1115";
            case TCA9548A: return "TCA9548A";
            case SSD1306: return "SSD1306";
            case PCA9685: return "PCA9685";
            case DS3231: return "DS3231";
            case TMP117: return "TMP117";
            case VL53L1X: return "VL53L1X";
            case MCP23017: return "MCP23017";
            case SHT31: return "SHT31";
            case HMC5883L: return "HMC5883L";
            case BH1750: return "BH1750";
            case LSM9DS0: return "LSM9DS0";
            case AT24C256: return "AT24C256";
            case PCF8574: return "PCF8574";
            case MCP4725: return "MCP4725";
            case VCNL4010: return "VCNL4010";
            case APDS9960: return "APDS9960";
            case DRV2605: return "DRV2605";
            case ISL29125: return "ISL29125";
            case MAX44009: return "MAX44009";
            case SI7021: return "SI7021";
            case L3GD20: return "L3GD20";
            default: return "Unknown";
        }
    }
}

} // namespace EdgeAI
