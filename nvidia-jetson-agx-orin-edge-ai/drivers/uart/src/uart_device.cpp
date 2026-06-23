#include "uart/uart_device.h"
#include <chrono>
#include <thread>
#include <iostream>
#include <sstream>
#include <iomanip>

namespace EdgeAI {

class UARTDevice::Impl {
public:
    Impl(UARTDriver& driver) : driver_(driver), initialized_(false), 
                               echo_enabled_(false), timeout_ms_(1000) {}
    
    ~Impl() = default;
    
    bool initialize() {
        if (initialized_) return true;
        
        // Check if UART is open
        if (!driver_.isOpen()) {
            setError("UART device not open");
            return false;
        }
        
        // Set up callbacks
        driver_.setDataCallback([this](const UARTData& data) {
            if (data_callback_) {
                data_callback_(data.data);
            }
        });
        
        driver_.setErrorCallback([this](const std::string& error) {
            if (error_callback_) {
                error_callback_(error);
            }
        });
        
        driver_.setLineBreakCallback([this](const std::string& line) {
            if (line_callback_) {
                line_callback_(line);
            }
        });
        
        initialized_ = true;
        return true;
    }
    
    bool isReady() const {
        return initialized_ && driver_.isOpen();
    }
    
    bool sendCommand(const std::string& command, 
                     std::string& response,
                     int timeout_ms) {
        if (!isReady()) return false;
        
        // Send command
        if (!driver_.writeString(command)) {
            setError("Failed to send command");
            return false;
        }
        
        // Read response
        response = driver_.readString(timeout_ms);
        return !response.empty();
    }
    
    bool sendCommandBinary(const std::vector<uint8_t>& command,
                           const std::vector<uint8_t>& data,
                           std::vector<uint8_t>& response,
                           int timeout_ms) {
        if (!isReady()) return false;
        
        // Send command
        if (!driver_.writeBytes(command)) {
            setError("Failed to send command");
            return false;
        }
        
        // Send data
        if (!data.empty()) {
            if (!driver_.writeBytes(data)) {
                setError("Failed to send data");
                return false;
            }
        }
        
        // Read response
        response = driver_.readBytes(1024, timeout_ms);
        return !response.empty();
    }
    
    bool sendWithAck(const std::vector<uint8_t>& data,
                     uint8_t ack,
                     int timeout_ms) {
        if (!isReady()) return false;
        
        // Send data
        if (!driver_.writeBytes(data)) {
            setError("Failed to send data");
            return false;
        }
        
        // Wait for ACK
        int ack_received = driver_.readByte();
        return ack_received == ack;
    }
    
    bool readUntil(const std::string& pattern,
                   std::string& result,
                   int timeout_ms) {
        if (!isReady()) return false;
        
        std::string buffer;
        auto start = std::chrono::steady_clock::now();
        
        while (true) {
            // Check timeout
            if (timeout_ms > 0) {
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - start).count();
                if (elapsed > timeout_ms) {
                    result = buffer;
                    return false;
                }
            }
            
            // Read one byte at a time
            int byte = driver_.readByte();
            if (byte < 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }
            
            buffer += static_cast<char>(byte);
            
            // Check for pattern
            if (buffer.find(pattern) != std::string::npos) {
                result = buffer;
                return true;
            }
        }
    }
    
    bool readBytes(size_t count,
                   std::vector<uint8_t>& data,
                   int timeout_ms) {
        if (!isReady()) return false;
        
        data = driver_.readBytes(count, timeout_ms);
        return data.size() == count;
    }
    
    bool writeWithRetry(const std::vector<uint8_t>& data, int retries) {
        if (!isReady()) return false;
        
        for (int attempt = 0; attempt < retries; attempt++) {
            if (driver_.writeBytes(data)) {
                return true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        return false;
    }
    
    bool setConfig(const std::string& config_name, const std::string& value) {
        if (config_name == "baudrate") {
            driver_.setBaudrate(std::stoi(value));
            return true;
        } else if (config_name == "parity") {
            if (value == "none") driver_.setParity(UART_PARITY_NONE);
            else if (value == "even") driver_.setParity(UART_PARITY_EVEN);
            else if (value == "odd") driver_.setParity(UART_PARITY_ODD);
            else if (value == "mark") driver_.setParity(UART_PARITY_MARK);
            else if (value == "space") driver_.setParity(UART_PARITY_SPACE);
            else return false;
            return true;
        } else if (config_name == "databits") {
            driver_.setDataBits(static_cast<UARTDataBits>(std::stoi(value)));
            return true;
        } else if (config_name == "stopbits") {
            driver_.setStopBits(static_cast<UARTStopBits>(std::stoi(value)));
            return true;
        } else if (config_name == "flowcontrol") {
            if (value == "none") driver_.setFlowControl(UART_FLOW_NONE);
            else if (value == "rtscts") driver_.setFlowControl(UART_FLOW_RTS_CTS);
            else if (value == "xonxoff") driver_.setFlowControl(UART_FLOW_XON_XOFF);
            else return false;
            return true;
        } else if (config_name == "echo") {
            echo_enabled_ = (value == "true" || value == "1");
            driver_.setEcho(echo_enabled_);
            return true;
        }
        
        return false;
    }
    
    bool getConfig(const std::string& config_name, std::string& value) {
        if (config_name == "baudrate") {
            value = std::to_string(driver_.getBaudrate());
            return true;
        } else if (config_name == "parity") {
            switch (driver_.getParity()) {
                case UART_PARITY_NONE: value = "none"; break;
                case UART_PARITY_EVEN: value = "even"; break;
                case UART_PARITY_ODD: value = "odd"; break;
                case UART_PARITY_MARK: value = "mark"; break;
                case UART_PARITY_SPACE: value = "space"; break;
                default: value = "unknown";
            }
            return true;
        } else if (config_name == "databits") {
            value = std::to_string(driver_.getDataBits());
            return true;
        } else if (config_name == "stopbits") {
            value = std::to_string(static_cast<int>(driver_.getStopBits()));
            return true;
        } else if (config_name == "echo") {
            value = echo_enabled_ ? "true" : "false";
            return true;
        }
        
        return false;
    }
    
    bool resetDevice() {
        if (!isReady()) return false;
        
        // Send break signal
        driver_.sendBreak(100);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // Flush buffers
        driver_.flushInput();
        driver_.flushOutput();
        
        return true;
    }
    
    std::string getDeviceInfo() const {
        std::stringstream ss;
        ss << "UART Device Info:\n";
        ss << "  Baudrate: " << driver_.getBaudrate() << "\n";
        ss << "  Data Bits: " << driver_.getDataBits() << "\n";
        ss << "  Parity: " << static_cast<int>(driver_.getParity()) << "\n";
        ss << "  Stop Bits: " << static_cast<int>(driver_.getStopBits()) << "\n";
        ss << "  Echo: " << (echo_enabled_ ? "Enabled" : "Disabled") << "\n";
        ss << "  Timeout: " << timeout_ms_ << " ms\n";
        ss << "  Initialized: " << (initialized_ ? "Yes" : "No") << "\n";
        return ss.str();
    }
    
    void setDataCallback(std::function<void(const std::vector<uint8_t>&)> callback) {
        data_callback_ = callback;
    }
    
    void setErrorCallback(std::function<void(const std::string&)> callback) {
        error_callback_ = callback;
    }
    
    void setLineBreakCallback(std::function<void(const std::string&)> callback) {
        line_callback_ = callback;
    }
    
    void setEcho(bool enable) {
        echo_enabled_ = enable;
        driver_.setEcho(enable);
    }
    
    bool isEchoEnabled() const {
        return echo_enabled_;
    }
    
    void setTimeout(int ms) {
        timeout_ms_ = ms;
    }
    
    int getTimeout() const {
        return timeout_ms_;
    }
    
    bool flushInput() {
        return driver_.flushInput();
    }
    
    bool flushOutput() {
        return driver_.flushOutput();
    }
    
private:
    UARTDriver& driver_;
    bool initialized_;
    bool echo_enabled_;
    int timeout_ms_;
    std::function<void(const std::vector<uint8_t>&)> data_callback_;
    std::function<void(const std::string&)> error_callback_;
    std::function<void(const std::string&)> line_callback_;
    
    void setError(const std::string& error) {
        if (error_callback_) {
            error_callback_(error);
        }
        std::cerr << "UART Device Error: " << error << std::endl;
    }
};

// UARTDevice implementation
UARTDevice::UARTDevice(UARTDriver& driver) 
    : driver_(driver), pImpl(std::make_unique<Impl>(driver)) {}

UARTDevice::~UARTDevice() = default;

bool UARTDevice::initialize() { return pImpl->initialize(); }
bool UARTDevice::isReady() const { return pImpl->isReady(); }

bool UARTDevice::sendCommand(const std::string& command, 
                             std::string& response,
                             int timeout_ms) {
    return pImpl->sendCommand(command, response, timeout_ms);
}
bool UARTDevice::sendCommandBinary(const std::vector<uint8_t>& command,
                                   const std::vector<uint8_t>& data,
                                   std::vector<uint8_t>& response,
                                   int timeout_ms) {
    return pImpl->sendCommandBinary(command, data, response, timeout_ms);
}
bool UARTDevice::sendWithAck(const std::vector<uint8_t>& data,
                             uint8_t ack,
                             int timeout_ms) {
    return pImpl->sendWithAck(data, ack, timeout_ms);
}
bool UARTDevice::readUntil(const std::string& pattern,
                           std::string& result,
                           int timeout_ms) {
    return pImpl->readUntil(pattern, result, timeout_ms);
}
bool UARTDevice::readBytes(size_t count,
                           std::vector<uint8_t>& data,
                           int timeout_ms) {
    return pImpl->readBytes(count, data, timeout_ms);
}
bool UARTDevice::writeWithRetry(const std::vector<uint8_t>& data, int retries) {
    return pImpl->writeWithRetry(data, retries);
}

bool UARTDevice::setConfig(const std::string& config_name, const std::string& value) {
    return pImpl->setConfig(config_name, value);
}
bool UARTDevice::getConfig(const std::string& config_name, std::string& value) {
    return pImpl->getConfig(config_name, value);
}

bool UARTDevice::resetDevice() { return pImpl->resetDevice(); }
std::string UARTDevice::getDeviceInfo() const { return pImpl->getDeviceInfo(); }

void UARTDevice::setDataCallback(std::function<void(const std::vector<uint8_t>&)> callback) {
    pImpl->setDataCallback(callback);
}
void UARTDevice::setErrorCallback(std::function<void(const std::string&)> callback) {
    pImpl->setErrorCallback(callback);
}
void UARTDevice::setLineBreakCallback(std::function<void(const std::string&)> callback) {
    pImpl->setLineBreakCallback(callback);
}

void UARTDevice::setEcho(bool enable) { pImpl->setEcho(enable); }
bool UARTDevice::isEchoEnabled() const { return pImpl->isEchoEnabled(); }
void UARTDevice::setTimeout(int ms) { pImpl->setTimeout(ms); }
int UARTDevice::getTimeout() const { return pImpl->getTimeout(); }
bool UARTDevice::flushInput() { return pImpl->flushInput(); }
bool UARTDevice::flushOutput() { return pImpl->flushOutput(); }

} // namespace EdgeAI
