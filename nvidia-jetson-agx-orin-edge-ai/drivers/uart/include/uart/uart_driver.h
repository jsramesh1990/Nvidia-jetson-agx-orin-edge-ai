#pragma once
#include <string>
#include <vector>
#include <functional>
#include <atomic>
#include <thread>
#include <queue>
#include <mutex>

namespace EdgeAI {

enum UARTParity {
    UART_PARITY_NONE = 0,
    UART_PARITY_EVEN = 1,
    UART_PARITY_ODD = 2,
    UART_PARITY_MARK = 3,
    UART_PARITY_SPACE = 4
};

enum UARTStopBits {
    UART_STOP_BITS_1 = 0,
    UART_STOP_BITS_1_5 = 1,
    UART_STOP_BITS_2 = 2
};

enum UARTFlowControl {
    UART_FLOW_NONE = 0,
    UART_FLOW_RTS_CTS = 1,
    UART_FLOW_XON_XOFF = 2,
    UART_FLOW_RTS_CTS_XON_XOFF = 3,
    UART_FLOW_DTR_DSR = 4
};

enum UARTDataBits {
    UART_DATA_BITS_5 = 5,
    UART_DATA_BITS_6 = 6,
    UART_DATA_BITS_7 = 7,
    UART_DATA_BITS_8 = 8
};

struct UARTConfig {
    std::string device = "/dev/ttyTHS1";
    int baudrate = 115200;
    UARTDataBits data_bits = UART_DATA_BITS_8;
    UARTParity parity = UART_PARITY_NONE;
    UARTStopBits stop_bits = UART_STOP_BITS_1;
    UARTFlowControl flow_control = UART_FLOW_NONE;
    bool canonical = false;
    int vtime = 0;      // Timeout in deciseconds
    int vmin = 0;       // Minimum characters to read
    int buffer_size = 4096;
    bool echo = false;
    bool raw_mode = true;
    int read_timeout_ms = 1000;
    int write_timeout_ms = 1000;
};

struct UARTData {
    std::vector<uint8_t> data;
    std::chrono::system_clock::time_point timestamp;
    std::string source;
    size_t bytes_read = 0;
    double elapsed_ms = 0.0;
};

struct UARTStats {
    size_t bytes_read = 0;
    size_t bytes_written = 0;
    size_t read_errors = 0;
    size_t write_errors = 0;
    size_t overrun_errors = 0;
    size_t framing_errors = 0;
    size_t parity_errors = 0;
    double avg_read_speed_bps = 0.0;
    double avg_write_speed_bps = 0.0;
    size_t total_read_operations = 0;
    size_t total_write_operations = 0;
};

class UARTDriver {
public:
    UARTDriver(const UARTConfig& config);
    ~UARTDriver();
    
    // Initialization
    bool initialize();
    void close();
    bool isOpen() const;
    bool isReady() const;
    
    // Read operations
    int readByte();
    std::vector<uint8_t> readBytes(size_t count, int timeout_ms = -1);
    std::string readString(int timeout_ms = -1);
    bool readAsync(size_t count);
    bool readUntil(const std::string& delimiter, std::string& result,
                   int timeout_ms = -1);
    
    // Write operations
    bool writeByte(uint8_t data);
    bool writeBytes(const std::vector<uint8_t>& data);
    bool writeString(const std::string& str);
    bool writeAsync(const std::vector<uint8_t>& data,
                    std::function<void(bool)> callback = nullptr);
    bool writeLine(const std::string& line);
    
    // Configuration
    void setBaudrate(int baudrate);
    void setParity(UARTParity parity);
    void setDataBits(UARTDataBits bits);
    void setStopBits(UARTStopBits bits);
    void setFlowControl(UARTFlowControl flow);
    void setTimeout(int vtime, int vmin);
    void setReadTimeout(int ms);
    void setWriteTimeout(int ms);
    void setBufferSize(int size);
    void setEcho(bool enable);
    void setRawMode(bool enable);
    
    // Status
    int getBaudrate() const;
    UARTParity getParity() const;
    UARTDataBits getDataBits() const;
    UARTStopBits getStopBits() const;
    UARTFlowControl getFlowControl() const;
    UARTConfig getConfig() const;
    UARTStats getStats() const;
    int getBytesAvailable() const;
    int getBytesInOutputBuffer() const;
    bool isDataAvailable() const;
    
    // Callbacks
    void setDataCallback(std::function<void(const UARTData&)> callback);
    void setErrorCallback(std::function<void(const std::string&)> callback);
    void setLineBreakCallback(std::function<void(const std::string&)> callback);
    void setByteReceivedCallback(std::function<void(uint8_t)> callback);
    
    // Utility
    bool flushInput();
    bool flushOutput();
    bool flushBoth();
    bool sendBreak(int duration_ms = 100);
    bool testUART();
    bool runLoopbackTest(const std::vector<uint8_t>& test_data);
    std::string getUARTInfo() const;
    void dumpRegisters();
    
private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

} // namespace EdgeAI
