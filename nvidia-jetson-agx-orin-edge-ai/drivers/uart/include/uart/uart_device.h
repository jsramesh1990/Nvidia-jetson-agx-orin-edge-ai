#pragma once
#include "uart_driver.h"
#include <memory>
#include <string>
#include <vector>
#include <functional>

namespace EdgeAI {

/**
 * @brief UART Device class for specific serial peripherals
 * 
 * This class provides high-level access to devices connected via UART.
 * It handles device-specific protocols and communication patterns.
 */
class UARTDevice {
public:
    UARTDevice(UARTDriver& driver);
    ~UARTDevice();

    // Disable copy
    UARTDevice(const UARTDevice&) = delete;
    UARTDevice& operator=(const UARTDevice&) = delete;

    // Enable move
    UARTDevice(UARTDevice&&) = default;
    UARTDevice& operator=(UARTDevice&&) = default;

    /**
     * @brief Initialize the device
     * @return true if successful
     */
    bool initialize();

    /**
     * @brief Check if device is ready
     * @return true if ready
     */
    bool isReady() const;

    /**
     * @brief Send a command and wait for response
     * @param command Command string
     * @param response Response string (output)
     * @param timeout_ms Timeout in milliseconds
     * @return true if successful
     */
    bool sendCommand(const std::string& command, 
                     std::string& response,
                     int timeout_ms = 1000);

    /**
     * @brief Send a command with binary data
     * @param command Command bytes
     * @param data Data to send
     * @param response Response data (output)
     * @param timeout_ms Timeout in milliseconds
     * @return true if successful
     */
    bool sendCommandBinary(const std::vector<uint8_t>& command,
                           const std::vector<uint8_t>& data,
                           std::vector<uint8_t>& response,
                           int timeout_ms = 1000);

    /**
     * @brief Send data and wait for acknowledgement
     * @param data Data to send
     * @param ack Expected acknowledgement
     * @param timeout_ms Timeout in milliseconds
     * @return true if acknowledged
     */
    bool sendWithAck(const std::vector<uint8_t>& data,
                     uint8_t ack = 0x06,
                     int timeout_ms = 1000);

    /**
     * @brief Read until a specific pattern is found
     * @param pattern Pattern to look for
     * @param result Resulting data (output)
     * @param timeout_ms Timeout in milliseconds
     * @return true if pattern found
     */
    bool readUntil(const std::string& pattern,
                   std::string& result,
                   int timeout_ms = 1000);

    /**
     * @brief Read a specific number of bytes
     * @param count Number of bytes to read
     * @param data Data read (output)
     * @param timeout_ms Timeout in milliseconds
     * @return true if successful
     */
    bool readBytes(size_t count,
                   std::vector<uint8_t>& data,
                   int timeout_ms = 1000);

    /**
     * @brief Write with retry on failure
     * @param data Data to write
     * @param retries Number of retries
     * @return true if successful
     */
    bool writeWithRetry(const std::vector<uint8_t>& data, int retries = 3);

    /**
     * @brief Set device specific configuration
     * @param config_name Configuration parameter name
     * @param value Configuration value
     * @return true if successful
     */
    bool setConfig(const std::string& config_name, const std::string& value);

    /**
     * @brief Get device specific configuration
     * @param config_name Configuration parameter name
     * @param value Configuration value (output)
     * @return true if successful
     */
    bool getConfig(const std::string& config_name, std::string& value);

    /**
     * @brief Reset the device
     * @return true if successful
     */
    bool resetDevice();

    /**
     * @brief Get device information
     * @return Device information string
     */
    std::string getDeviceInfo() const;

    /**
     * @brief Set callback for asynchronous data reception
     * @param callback Function called when data is received
     */
    void setDataCallback(std::function<void(const std::vector<uint8_t>&)> callback);

    /**
     * @brief Set callback for errors
     * @param callback Function called on error
     */
    void setErrorCallback(std::function<void(const std::string&)> callback);

    /**
     * @brief Set callback for line breaks
     * @param callback Function called when a line is received
     */
    void setLineBreakCallback(std::function<void(const std::string&)> callback);

    /**
     * @brief Enable/disable echo
     * @param enable true to enable echo
     */
    void setEcho(bool enable);

    /**
     * @brief Check if echo is enabled
     * @return true if echo enabled
     */
    bool isEchoEnabled() const;

    /**
     * @brief Set device timeout
     * @param ms Timeout in milliseconds
     */
    void setTimeout(int ms);

    /**
     * @brief Get device timeout
     * @return Timeout in milliseconds
     */
    int getTimeout() const;

    /**
     * @brief Flush input buffer
     * @return true if successful
     */
    bool flushInput();

    /**
     * @brief Flush output buffer
     * @return true if successful
     */
    bool flushOutput();

private:
    class Impl;
    std::unique_ptr<Impl> pImpl;

    UARTDriver& driver_;
    bool initialized_ = false;
    bool echo_enabled_ = false;
    int timeout_ms_ = 1000;
    std::function<void(const std::vector<uint8_t>&)> data_callback_;
    std::function<void(const std::string&)> error_callback_;
    std::function<void(const std::string&)> line_callback_;
};

// Common UART device types
namespace UART_DEVICES {

/**
 * @brief GPS Module (NMEA protocol)
 */
class GPSModule : public UARTDevice {
public:
    GPSModule(UARTDriver& driver);
    
    struct GPSData {
        double latitude = 0.0;
        double longitude = 0.0;
        double altitude = 0.0;
        double speed = 0.0;
        double course = 0.0;
        int satellites = 0;
        double hdop = 0.0;
        std::string time;
        std::string date;
        bool fix_3d = false;
        bool fix_2d = false;
    };
    
    bool initialize() override;
    bool parseNMEA(const std::string& sentence);
    GPSData getLatestData();
    bool isDataValid() const;
    void setUpdateCallback(std::function<void(const GPSData&)> callback);
    
private:
    GPSData latest_data_;
    std::mutex data_mutex_;
    std::function<void(const GPSData&)> update_callback_;
    bool data_valid_ = false;
    
    void parseGPGGA(const std::string& fields);
    void parseGPRMC(const std::string& fields);
    void parseGPGSA(const std::string& fields);
    void parseGPGSV(const std::string& fields);
    void parseGPVTG(const std::string& fields);
    double parseLatLon(const std::string& coord, const std::string& direction);
};

/**
 * @brief Modbus RTU Device
 */
class ModbusRTU : public UARTDevice {
public:
    ModbusRTU(UARTDriver& driver, uint8_t slave_id = 1);
    
    struct ModbusException {
        uint8_t code;
        std::string message;
    };
    
    bool initialize() override;
    bool setSlaveID(uint8_t id);
    uint8_t getSlaveID() const;
    
    // Coils (digital outputs)
    bool readCoils(uint16_t address, uint16_t count, std::vector<bool>& values);
    bool readDiscreteInputs(uint16_t address, uint16_t count, std::vector<bool>& values);
    bool writeSingleCoil(uint16_t address, bool value);
    bool writeMultipleCoils(uint16_t address, const std::vector<bool>& values);
    
    // Registers (analog values)
    bool readHoldingRegisters(uint16_t address, uint16_t count, std::vector<uint16_t>& values);
    bool readInputRegisters(uint16_t address, uint16_t count, std::vector<uint16_t>& values);
    bool writeSingleRegister(uint16_t address, uint16_t value);
    bool writeMultipleRegisters(uint16_t address, const std::vector<uint16_t>& values);
    
    // Custom functions
    bool maskWriteRegister(uint16_t address, uint16_t and_mask, uint16_t or_mask);
    bool readWriteMultipleRegisters(uint16_t read_address, uint16_t read_count,
                                    uint16_t write_address, const std::vector<uint16_t>& write_values);
    
    // Exception handling
    void setExceptionCallback(std::function<void(const ModbusException&)> callback);
    
private:
    uint8_t slave_id_;
    uint16_t transaction_id_ = 0;
    std::function<void(const ModbusException&)> exception_callback_;
    
    std::vector<uint8_t> buildRequest(uint8_t function, uint16_t address, 
                                      uint16_t count = 0, const std::vector<uint8_t>& data = {});
    bool parseResponse(const std::vector<uint8_t>& response, std::vector<uint8_t>& data);
    uint16_t crc16(const std::vector<uint8_t>& data);
};

/**
 * @brief AT Command Device (e.g., GSM, WiFi modules)
 */
class ATCommandDevice : public UARTDevice {
public:
    ATCommandDevice(UARTDriver& driver);
    
    struct ATResponse {
        bool success = false;
        std::string response;
        std::vector<std::string> lines;
        int error_code = 0;
        std::string error_message;
    };
    
    bool initialize() override;
    ATResponse sendAT(const std::string& command, const std::string& expected = "OK",
                      int timeout_ms = 1000);
    ATResponse sendATData(const std::string& command, const std::vector<uint8_t>& data,
                          const std::string& expected = "OK", int timeout_ms = 1000);
    bool testAT();
    std::string getIMEI();
    std::string getIMSI();
    std::string getFirmwareVersion();
    bool factoryReset();
    bool setBaudRate(int baudrate);
    bool setEcho(bool enable);
    bool setSMSMode(int mode);
    bool sendSMS(const std::string& number, const std::string& message);
    bool sendSMSBinary(const std::string& number, const std::vector<uint8_t>& data);
    bool readSMS(int index, std::string& sender, std::string& message);
    bool deleteSMS(int index);
    bool listSMS(int mode, std::vector<int>& indices);
    
    // Network operations
    bool connectNetwork();
    bool disconnectNetwork();
    bool getNetworkStatus(std::string& status);
    bool getSignalStrength(int& rssi, int& ber);
    bool getOperatorName(std::string& name);
    bool setAPN(const std::string& apn, const std::string& user = "", 
                const std::string& password = "");
    bool connectGPRS();
    bool disconnectGPRS();
    std::string getIPAddress();
    
    // Callbacks
    void setUnsolicitedCallback(std::function<void(const std::string&)> callback);
    void setSMSCallback(std::function<void(const std::string&, const std::string&)> callback);
    void setNetworkStatusCallback(std::function<void(const std::string&)> callback);
    
private:
    std::function<void(const std::string&)> unsolicited_callback_;
    std::function<void(const std::string&, const std::string&)> sms_callback_;
    std::function<void(const std::string&)> network_callback_;
    
    void parseUnsolicited(const std::string& line);
};

/**
 * @brief Serial Terminal Device
 */
class SerialTerminal : public UARTDevice {
public:
    SerialTerminal(UARTDriver& driver);
    
    bool initialize() override;
    bool sendLine(const std::string& line);
    std::string readLine(int timeout_ms = 1000);
    std::vector<std::string> readLines(int count, int timeout_ms = 1000);
    bool sendBreak(int duration_ms = 100);
    bool setFlowControl(UARTFlowControl flow);
    bool setParity(UARTParity parity);
    bool setDataBits(UARTDataBits bits);
    bool setStopBits(UARTStopBits bits);
    bool setBaudRate(int baudrate);
    
    // Terminal modes
    bool setLineMode(bool enable);
    bool setEchoMode(bool enable);
    bool setRawMode(bool enable);
    bool setLocalEcho(bool enable);
    
    // Terminal features
    bool clearScreen();
    bool moveCursor(int row, int col);
    bool setColor(int foreground, int background = -1);
    bool resetColors();
    bool setTitle(const std::string& title);
    bool sendControlCode(uint8_t code);
    
    // Callbacks
    void setLineCallback(std::function<void(const std::string&)> callback);
    void setCharCallback(std::function<void(uint8_t)> callback);
    void setControlCallback(std::function<void(uint8_t)> callback);
    
private:
    bool line_mode_ = false;
    bool echo_mode_ = false;
    bool raw_mode_ = false;
    bool local_echo_ = false;
    std::function<void(const std::string&)> line_callback_;
    std::function<void(uint8_t)> char_callback_;
    std::function<void(uint8_t)> control_callback_;
    std::string buffer_;
    std::mutex buffer_mutex_;
};

} // namespace UART_DEVICES

} // namespace EdgeAI
