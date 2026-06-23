#pragma once
#include "i2c_driver.h"
#include <memory>
#include <vector>
#include <cstdint>
#include <string>

namespace EdgeAI {

/**
 * @brief I2C Device class for specific I2C peripherals
 */
class I2CDevice {
public:
    I2CDevice(I2CDriver& driver, uint8_t address);
    ~I2CDevice();

    // Disable copy
    I2CDevice(const I2CDevice&) = delete;
    I2CDevice& operator=(const I2CDevice&) = delete;

    // Enable move
    I2CDevice(I2CDevice&&) = default;
    I2CDevice& operator=(I2CDevice&&) = default;

    /**
     * @brief Read a single 8-bit register
     * @param reg Register address
     * @return Register value (0 on error)
     */
    uint8_t readReg(uint8_t reg);

    /**
     * @brief Write a single 8-bit register
     * @param reg Register address
     * @param value Value to write
     */
    void writeReg(uint8_t reg, uint8_t value);

    /**
     * @brief Read a 16-bit register (two bytes)
     * @param reg Register address
     * @return 16-bit register value (0 on error)
     */
    uint16_t readReg16(uint8_t reg);

    /**
     * @brief Write a 16-bit register
     * @param reg Register address
     * @param value 16-bit value to write
     */
    void writeReg16(uint8_t reg, uint16_t value);

    /**
     * @brief Read a 32-bit register (four bytes)
     * @param reg Register address
     * @return 32-bit register value (0 on error)
     */
    uint32_t readReg32(uint8_t reg);

    /**
     * @brief Write a 32-bit register
     * @param reg Register address
     * @param value 32-bit value to write
     */
    void writeReg32(uint8_t reg, uint32_t value);

    /**
     * @brief Read multiple registers
     * @param reg Starting register address
     * @param count Number of bytes to read
     * @return Vector of bytes read
     */
    std::vector<uint8_t> readRegBlock(uint8_t reg, size_t count);

    /**
     * @brief Write multiple registers
     * @param reg Starting register address
     * @param data Data to write
     */
    void writeRegBlock(uint8_t reg, const std::vector<uint8_t>& data);

    /**
     * @brief Read a single bit from a register
     * @param reg Register address
     * @param bit Bit position (0-7)
     * @return Bit value (0 or 1)
     */
    uint8_t readBit(uint8_t reg, uint8_t bit);

    /**
     * @brief Write a single bit to a register
     * @param reg Register address
     * @param bit Bit position (0-7)
     * @param value Bit value (0 or 1)
     */
    void writeBit(uint8_t reg, uint8_t bit, uint8_t value);

    /**
     * @brief Read multiple bits from a register
     * @param reg Register address
     * @param start Starting bit position
     * @param length Number of bits to read
     * @return Bits value
     */
    uint8_t readBits(uint8_t reg, uint8_t start, uint8_t length);

    /**
     * @brief Write multiple bits to a register
     * @param reg Register address
     * @param start Starting bit position
     * @param length Number of bits to write
     * @param value Bits value
     */
    void writeBits(uint8_t reg, uint8_t start, uint8_t length, uint8_t value);

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
     * @brief Check if device is connected
     * @return true if device is connected
     */
    bool isConnected();

    /**
     * @brief Check if device is ready
     * @return true if device is ready
     */
    bool isReady() const;

    /**
     * @brief Set I2C address
     * @param address New device address
     */
    void setAddress(uint8_t address);

    /**
     * @brief Get current I2C address
     * @return Device address
     */
    uint8_t getAddress() const { return address_; }

    /**
     * @brief Set timeout for operations
     * @param ms Timeout in milliseconds
     */
    void setTimeout(int ms);

    /**
     * @brief Set retry count for operations
     * @param retries Number of retries
     */
    void setRetries(int retries);

    /**
     * @brief Get device information string
     * @return Device information
     */
    std::string getDeviceInfo() const;

    /**
     * @brief Perform a combined write-read transaction
     * @param write_data Data to write
     * @param read_count Number of bytes to read
     * @return Read data
     */
    std::vector<uint8_t> writeRead(const std::vector<uint8_t>& write_data,
                                   size_t read_count);

private:
    class Impl;
    std::unique_ptr<Impl> pImpl;

    uint8_t address_;
    int timeout_ms_;
    int retries_;
};

// Common I2C device types
namespace I2C_DEVICES {

/**
 * @brief MPU6050 6-axis IMU (accelerometer + gyroscope)
 */
class MPU6050 : public I2CDevice {
public:
    MPU6050(I2CDriver& driver, uint8_t address = 0x68);
    
    struct IMUData {
        float accel_x;
        float accel_y;
        float accel_z;
        float gyro_x;
        float gyro_y;
        float gyro_z;
        float temp_celsius;
        float pitch;
        float roll;
        float yaw;
    };
    
    bool initialize();
    IMUData readData();
    void setAccelRange(uint8_t range);
    void setGyroRange(uint8_t range);
    void setFilterBandwidth(uint8_t bandwidth);
    void setSampleRateDivider(uint8_t divider);
};

/**
 * @brief BMP280 Pressure and Temperature sensor
 */
class BMP280 : public I2CDevice {
public:
    BMP280(I2CDriver& driver, uint8_t address = 0x76);
    
    struct SensorData {
        double temperature_celsius;
        double temperature_fahrenheit;
        double pressure_hpa;
        double pressure_mbar;
        double altitude_meters;
        double altitude_feet;
    };
    
    bool initialize();
    SensorData readData();
    void setOversampling(uint8_t osrs);
    void setStandbyTime(uint8_t time);
    void setFilter(uint8_t filter);
    double calculateAltitude(double pressure);
};

/**
 * @brief BME280 Environmental sensor (temperature + humidity + pressure)
 */
class BME280 : public I2CDevice {
public:
    BME280(I2CDriver& driver, uint8_t address = 0x76);
    
    struct SensorData {
        double temperature_celsius;
        double humidity_percent;
        double pressure_hpa;
        double dew_point_celsius;
    };
    
    bool initialize();
    SensorData readData();
    void setOversampling(uint8_t osrs_temp, uint8_t osrs_humid, uint8_t osrs_press);
    void setStandbyTime(uint8_t time);
    void setFilter(uint8_t filter);
};

/**
 * @brief ADS1115 16-bit ADC
 */
class ADS1115 : public I2CDevice {
public:
    ADS1115(I2CDriver& driver, uint8_t address = 0x48);
    
    bool initialize();
    int16_t readADC(uint8_t channel);
    int16_t readDifferential(uint8_t p_channel, uint8_t n_channel);
    float readVoltage(uint8_t channel, float gain = 1.0f);
    float readDifferentialVoltage(uint8_t p_channel, uint8_t n_channel, float gain = 1.0f);
    void setGain(float gain);
    void setDataRate(uint8_t rate);
    void setMode(uint8_t mode);
};

/**
 * @brief TCA9548A I2C Multiplexer
 */
class TCA9548A : public I2CDevice {
public:
    TCA9548A(I2CDriver& driver, uint8_t address = 0x70);
    
    bool selectChannel(uint8_t channel);
    bool enableChannel(uint8_t channel);
    bool disableChannel(uint8_t channel);
    bool enableAll();
    bool disableAll();
    uint8_t getSelectedChannels();
};

/**
 * @brief SSD1306 OLED Display
 */
class SSD1306 : public I2CDevice {
public:
    SSD1306(I2CDriver& driver, uint8_t address = 0x3C);
    
    struct DisplayConfig {
        int width = 128;
        int height = 64;
        bool inverted = false;
        uint8_t contrast = 0x7F;
    };
    
    bool initialize();
    void clear();
    void update();
    void setPixel(int x, int y, bool on);
    void drawChar(int x, int y, char c, bool on = true);
    void drawString(int x, int y, const std::string& text);
    void drawLine(int x1, int y1, int x2, int y2, bool on = true);
    void drawRect(int x, int y, int w, int h, bool on = true);
    void drawCircle(int x, int y, int r, bool on = true);
    void setContrast(uint8_t contrast);
    void setInverted(bool inverted);
    void setBrightness(uint8_t brightness);
};

/**
 * @brief PCA9685 16-channel PWM driver
 */
class PCA9685 : public I2CDevice {
public:
    PCA9685(I2CDriver& driver, uint8_t address = 0x40);
    
    bool initialize();
    void setPWMFreq(uint32_t freq);
    void setPWM(uint8_t channel, uint16_t on, uint16_t off);
    void setPWMValue(uint8_t channel, uint16_t value);
    void setAllPWM(uint16_t on, uint16_t off);
    void setAllPWMValue(uint16_t value);
    void setPin(uint8_t channel, bool high);
    void setServoAngle(uint8_t channel, float angle);
    void setServoPulse(uint8_t channel, uint16_t pulse_us);
    void sleep();
    void wake();
    void reset();
};

/**
 * @brief DS3231 Real Time Clock
 */
class DS3231 : public I2CDevice {
public:
    DS3231(I2CDriver& driver, uint8_t address = 0x68);
    
    struct DateTime {
        uint8_t year;      // 0-99
        uint8_t month;     // 1-12
        uint8_t day;       // 1-31
        uint8_t hour;      // 0-23
        uint8_t minute;    // 0-59
        uint8_t second;    // 0-59
        uint8_t weekday;   // 1-7 (Monday=1)
    };
    
    bool initialize();
    bool setDateTime(const DateTime& dt);
    bool getDateTime(DateTime& dt);
    float getTemperature();
    bool setAlarm(uint8_t alarm, const DateTime& dt, bool enable = true);
    bool clearAlarm(uint8_t alarm);
    bool getAlarmStatus(uint8_t alarm);
    bool enableSquareWave(bool enable, uint8_t freq = 0);
};

/**
 * @brief TMP117 High-precision temperature sensor
 */
class TMP117 : public I2CDevice {
public:
    TMP117(I2CDriver& driver, uint8_t address = 0x48);
    
    bool initialize();
    float readTemperature();
    float readTemperatureFahrenheit();
    void setAverageMode(uint8_t mode);
    void setConversionCycleTime(uint8_t time);
    void setAlertMode(uint8_t mode);
    void setAlertLimits(float lower_temp, float upper_temp);
    bool getAlertStatus();
    void clearAlert();
};

/**
 * @brief VL53L1X Time-of-Flight distance sensor
 */
class VL53L1X : public I2CDevice {
public:
    VL53L1X(I2CDriver& driver, uint8_t address = 0x29);
    
    struct RangeData {
        int distance_mm;
        int distance_cm;
        int signal_rate;
        int ambient_rate;
        int status;
    };
    
    bool initialize();
    bool startRanging();
    bool stopRanging();
    bool isDataReady();
    RangeData getRange();
    void setTimingBudget(uint32_t budget_ms);
    void setInterMeasurementPeriod(uint32_t period_ms);
    void setDistanceMode(uint8_t mode);
    void setROI(uint16_t x, uint16_t y, uint8_t size);
};

} // namespace I2C_DEVICES

} // namespace EdgeAI
