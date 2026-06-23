#include "uart/uart_driver.h"
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <poll.h>
#include <cstring>
#include <chrono>

namespace EdgeAI {

class UARTDriver::Impl {
public:
    Impl(const UARTConfig& config) : config_(config), fd_(-1), running_(false) {}
    
    ~Impl() { close(); }
    
    bool initialize() {
        fd_ = open(config_.device.c_str(), O_RDWR | O_NOCTTY | O_NDELAY);
        if (fd_ < 0) {
            setError("Failed to open UART device: " + config_.device);
            return false;
        }
        
        // Configure UART
        if (!configureUART()) {
            close();
            return false;
        }
        
        // Start read thread
        if (config_.canonical) {
            running_ = true;
            readThread_ = std::thread(&Impl::readLoop, this);
        }
        
        return true;
    }
    
    void close() {
        if (running_) {
            running_ = false;
            if (readThread_.joinable()) {
                readThread_.join();
            }
        }
        if (fd_ >= 0) {
            ::close(fd_);
            fd_ = -1;
        }
    }
    
    bool isOpen() const { return fd_ >= 0; }
    
    int readByte() {
        if (!isOpen()) return -1;
        uint8_t byte;
        ssize_t n = ::read(fd_, &byte, 1);
        return n == 1 ? byte : -1;
    }
    
    std::vector<uint8_t> readBytes(size_t count, int timeout_ms) {
        if (!isOpen()) return {};
        
        std::vector<uint8_t> buffer(count);
        size_t total_read = 0;
        auto start = std::chrono::steady_clock::now();
        
        while (total_read < count) {
            if (timeout_ms > 0) {
                auto elapsed = std::chrono::steady_clock::now() - start;
                if (std::chrono::duration_cast<std::chrono::milliseconds>(elapsed)
                        .count() > timeout_ms) {
                    break;
                }
            }
            
            ssize_t n = ::read(fd_, buffer.data() + total_read, 
                               count - total_read);
            if (n > 0) {
                total_read += n;
            } else if (n < 0) {
                if (errno != EAGAIN && errno != EWOULDBLOCK) {
                    setError("Read error: " + std::string(strerror(errno)));
                    break;
                }
            }
            
            if (total_read == 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }
        
        buffer.resize(total_read);
        return buffer;
    }
    
    std::string readString(int timeout_ms) {
        auto data = readBytes(1024, timeout_ms);
        return std::string(data.begin(), data.end());
    }
    
    bool readAsync(size_t count) {
        if (!isOpen()) return false;
        
        // Start async read
        std::thread([this, count]() {
            auto data = this->readBytes(count, -1);
            if (!data.empty() && data_callback_) {
                UARTData uart_data;
                uart_data.data = data;
                uart_data.timestamp = std::chrono::system_clock::now();
                uart_data.source = config_.device;
                data_callback_(uart_data);
            }
        }).detach();
        
        return true;
    }
    
    bool writeByte(uint8_t data) {
        if (!isOpen()) return false;
        ssize_t n = ::write(fd_, &data, 1);
        return n == 1;
    }
    
    bool writeBytes(const std::vector<uint8_t>& data) {
        if (!isOpen()) return false;
        ssize_t total_written = 0;
        
        while (total_written < static_cast<ssize_t>(data.size())) {
            ssize_t n = ::write(fd_, data.data() + total_written,
                               data.size() - total_written);
            if (n > 0) {
                total_written += n;
            } else if (n < 0) {
                if (errno != EAGAIN && errno != EWOULDBLOCK) {
                    setError("Write error: " + std::string(strerror(errno)));
                    return false;
                }
            }
            if (total_written == 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }
        return true;
    }
    
    bool writeString(const std::string& str) {
        std::vector<uint8_t> data(str.begin(), str.end());
        return writeBytes(data);
    }
    
    bool writeAsync(const std::vector<uint8_t>& data,
                    std::function<void(bool)> callback) {
        if (!isOpen()) return false;
        
        std::thread([this, data, callback]() {
            bool success = this->writeBytes(data);
            if (callback) {
                callback(success);
            }
        }).detach();
        
        return true;
    }
    
    void setBaudrate(int baudrate) {
        config_.baudrate = baudrate;
        if (isOpen()) {
            configureUART();
        }
    }
    
    void setParity(UARTParity parity) {
        config_.parity = parity;
        if (isOpen()) {
            configureUART();
        }
    }
    
    void setDataBits(int bits) {
        config_.data_bits = bits;
        if (isOpen()) {
            configureUART();
        }
    }
    
    void setStopBits(UARTStopBits bits) {
        config_.stop_bits = bits;
        if (isOpen()) {
            configureUART();
        }
    }
    
    void setFlowControl(UARTFlowControl flow) {
        config_.flow_control = flow;
        if (isOpen()) {
            configureUART();
        }
    }
    
    void setTimeout(int vtime, int vmin) {
        config_.vtime = vtime;
        config_.vmin = vmin;
        if (isOpen()) {
            configureUART();
        }
    }
    
    int getBaudrate() const { return config_.baudrate; }
    UARTParity getParity() const { return config_.parity; }
    int getDataBits() const { return config_.data_bits; }
    UARTStopBits getStopBits() const { return config_.stop_bits; }
    
    int getBytesAvailable() const {
        if (!isOpen()) return 0;
        int bytes;
        ioctl(fd_, FIONREAD, &bytes);
        return bytes;
    }
    
    void setDataCallback(std::function<void(const UARTData&)> callback) {
        data_callback_ = callback;
    }
    
    void setErrorCallback(std::function<void(const std::string&)> callback) {
        error_callback_ = callback;
    }
    
    void setLineBreakCallback(std::function<void(const std::string&)> callback) {
        line_break_callback_ = callback;
    }
    
private:
    UARTConfig config_;
    int fd_;
    std::thread readThread_;
    std::atomic<bool> running_;
    std::function<void(const UARTData&)> data_callback_;
    std::function<void(const std::string&)> error_callback_;
    std::function<void(const std::string&)> line_break_callback_;
    std::string line_buffer_;
    
    bool configureUART() {
        struct termios tty;
        if (tcgetattr(fd_, &tty) != 0) {
            setError("Failed to get UART attributes");
            return false;
        }
        
        // Set baudrate
        speed_t speed;
        switch (config_.baudrate) {
            case 9600: speed = B9600; break;
            case 19200: speed = B19200; break;
            case 38400: speed = B38400; break;
            case 57600: speed = B57600; break;
            case 115200: speed = B115200; break;
            case 230400: speed = B230400; break;
            case 460800: speed = B460800; break;
            case 921600: speed = B921600; break;
            default: speed = B115200; break;
        }
        
        cfsetospeed(&tty, speed);
        cfsetispeed(&tty, speed);
        
        // Data bits
        tty.c_cflag &= ~CSIZE;
        switch (config_.data_bits) {
            case 5: tty.c_cflag |= CS5; break;
            case 6: tty.c_cflag |= CS6; break;
            case 7: tty.c_cflag |= CS7; break;
            case 8: tty.c_cflag |= CS8; break;
            default: tty.c_cflag |= CS8; break;
        }
        
        // Parity
        tty.c_cflag &= ~(PARENB | PARODD);
        switch (config_.parity) {
            case UART_PARITY_EVEN:
                tty.c_cflag |= PARENB;
                break;
            case UART_PARITY_ODD:
                tty.c_cflag |= PARENB | PARODD;
                break;
            case UART_PARITY_MARK:
                tty.c_cflag |= PARENB | PARODD | CMSPAR;
                break;
            case UART_PARITY_SPACE:
                tty.c_cflag |= PARENB | CMSPAR;
                break;
            default:
                break;
        }
        
        // Stop bits
        tty.c_cflag &= ~CSTOPB;
        if (config_.stop_bits == UART_STOP_BITS_2 ||
            config_.stop_bits == UART_STOP_BITS_1_5) {
            tty.c_cflag |= CSTOPB;
        }
        
        // Flow control
        tty.c_cflag &= ~(CRTSCTS | IXON | IXOFF | IXANY);
        switch (config_.flow_control) {
            case UART_FLOW_RTS_CTS:
                tty.c_cflag |= CRTSCTS;
                break;
            case UART_FLOW_XON_XOFF:
                tty.c_iflag |= IXON | IXOFF;
                break;
            case UART_FLOW_RTS_CTS_XON_XOFF:
                tty.c_cflag |= CRTSCTS;
                tty.c_iflag |= IXON | IXOFF;
                break;
            default:
                break;
        }
        
        // Local modes
        tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
        if (config_.canonical) {
            tty.c_lflag |= ICANON;
        }
        
        // Input modes
        tty.c_iflag &= ~(INLCR | ICRNL | IGNCR | ISTRIP);
        
        // Output modes
        tty.c_oflag &= ~(OPOST | ONLCR | OCRNL);
        
        // Control modes
        tty.c_cflag |= CREAD | CLOCAL;
        
        // VMIN and VTIME
        tty.c_cc[VMIN] = config_.vmin;
        tty.c_cc[VTIME] = config_.vtime;
        
        // Apply settings
        if (tcsetattr(fd_, TCSANOW, &tty) != 0) {
            setError("Failed to set UART attributes");
            return false;
        }
        
        return true;
    }
    
    void setError(const std::string& error) {
        if (error_callback_) {
            error_callback_(error);
        }
        std::cerr << "UART Error: " << error << std::endl;
    }
    
    void readLoop() {
        const size_t BUFFER_SIZE = 4096;
        std::vector<uint8_t> buffer(BUFFER_SIZE);
        
        while (running_) {
            ssize_t n = ::read(fd_, buffer.data(), BUFFER_SIZE);
            if (n > 0) {
                std::vector<uint8_t> data(buffer.begin(), buffer.begin() + n);
                
                // Handle line breaks
                if (line_break_callback_) {
                    std::string str(data.begin(), data.end());
                    size_t pos = 0;
                    while ((pos = str.find('\n', pos)) != std::string::npos) {
                        std::string line = str.substr(0, pos);
                        if (!line.empty() && 
                            (line_break_callback_ && line_buffer_.empty())) {
                            line_break_callback_(line);
                        } else if (!line.empty()) {
                            line_buffer_ += line;
                            line_break_callback_(line_buffer_);
                            line_buffer_.clear();
                        }
                        str.erase(0, pos + 1);
                        pos = 0;
                    }
                    if (!str.empty()) {
                        line_buffer_ += str;
                    }
                }
                
                // Call data callback
                if (data_callback_) {
                    UARTData uart_data;
                    uart_data.data = data;
                    uart_data.timestamp = std::chrono::system_clock::now();
                    uart_data.source = config_.device;
                    data_callback_(uart_data);
                }
            } else if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
                setError("Read error: " + std::string(strerror(errno)));
                break;
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
};

// UARTDriver implementation
UARTDriver::UARTDriver(const UARTConfig& config) 
    : pImpl(std::make_unique<Impl>(config)) {}

UARTDriver::~UARTDriver() = default;

bool UARTDriver::initialize() { return pImpl->initialize(); }
void UARTDriver::close() { pImpl->close(); }
bool UARTDriver::isOpen() const { return pImpl->isOpen(); }

int UARTDriver::readByte() { return pImpl->readByte(); }
std::vector<uint8_t> UARTDriver::readBytes(size_t count, int timeout_ms) {
    return pImpl->readBytes(count, timeout_ms);
}
std::string UARTDriver::readString(int timeout_ms) {
    return pImpl->readString(timeout_ms);
}
bool UARTDriver::readAsync(size_t count) { return pImpl->readAsync(count); }

bool UARTDriver::writeByte(uint8_t data) { return pImpl->writeByte(data); }
bool UARTDriver::writeBytes(const std::vector<uint8_t>& data) {
    return pImpl->writeBytes(data);
}
bool UARTDriver::writeString(const std::string& str) {
    return pImpl->writeString(str);
}
bool UARTDriver::writeAsync(const std::vector<uint8_t>& data,
                            std::function<void(bool)> callback) {
    return pImpl->writeAsync(data, callback);
}

void UARTDriver::setBaudrate(int baudrate) { pImpl->setBaudrate(baudrate); }
void UARTDriver::setParity(UARTParity parity) { pImpl->setParity(parity); }
void UARTDriver::setDataBits(int bits) { pImpl->setDataBits(bits); }
void UARTDriver::setStopBits(UARTStopBits bits) { pImpl->setStopBits(bits); }
void UARTDriver::setFlowControl(UARTFlowControl flow) {
    pImpl->setFlowControl(flow);
}
void UARTDriver::setTimeout(int vtime, int vmin) {
    pImpl->setTimeout(vtime, vmin);
}

int UARTDriver::getBaudrate() const { return pImpl->getBaudrate(); }
UARTParity UARTDriver::getParity() const { return pImpl->getParity(); }
int UARTDriver::getDataBits() const { return pImpl->getDataBits(); }
UARTStopBits UARTDriver::getStopBits() const { return pImpl->getStopBits(); }
int UARTDriver::getBytesAvailable() const { return pImpl->getBytesAvailable(); }

void UARTDriver::setDataCallback(std::function<void(const UARTData&)> callback) {
    pImpl->setDataCallback(callback);
}
void UARTDriver::setErrorCallback(std::function<void(const std::string&)> callback) {
    pImpl->setErrorCallback(callback);
}
void UARTDriver::setLineBreakCallback(std::function<void(const std::string&)> callback) {
    pImpl->setLineBreakCallback(callback);
}

} // namespace EdgeAI
