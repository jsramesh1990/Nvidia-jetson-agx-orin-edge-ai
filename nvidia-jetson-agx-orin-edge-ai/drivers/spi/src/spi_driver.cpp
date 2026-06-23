#include "spi/spi_driver.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <poll.h>
#include <thread>
#include <chrono>
#include <cstring>
#include <iostream>
#include <atomic>

namespace EdgeAI {

class SPIDriver::Impl {
public:
    Impl(const SPIConfig& config) : config_(config), fd_(-1), running_(false) {}
    
    ~Impl() {
        close();
    }
    
    bool initialize() {
        fd_ = open(config_.device.c_str(), O_RDWR);
        if (fd_ < 0) {
            setError("Failed to open SPI device: " + config_.device);
            return false;
        }
        
        // Configure SPI
        if (!setMode(config_.mode) || 
            !setBitsPerWord(config_.bits_per_word) ||
            !setSpeed(config_.speed) ||
            !setBitOrder(config_.bit_order)) {
            close();
            return false;
        }
        
        // Set CS change
        if (ioctl(fd_, SPI_IOC_WR_CS_CHANGE, &config_.cs_change) < 0) {
            setError("Failed to set CS change");
            close();
            return false;
        }
        
        // Set loopback mode (for testing)
        if (config_.loopback) {
            if (ioctl(fd_, SPI_IOC_WR_LOOP, &config_.loopback) < 0) {
                setError("Failed to set loopback mode");
                close();
                return false;
            }
        }
        
        return true;
    }
    
    void close() {
        if (running_) {
            running_ = false;
            if (transferThread_.joinable()) {
                transferThread_.join();
            }
        }
        if (fd_ >= 0) {
            ::close(fd_);
            fd_ = -1;
        }
    }
    
    bool isOpen() const {
        return fd_ >= 0;
    }
    
    uint8_t transferByte(uint8_t tx_data) {
        std::vector<uint8_t> tx = {tx_data};
        auto rx = transfer(tx);
        return rx.empty() ? 0 : rx[0];
    }
    
    std::vector<uint8_t> transfer(const std::vector<uint8_t>& tx_data) {
        std::lock_guard<std::mutex> lock(transfer_mutex_);
        
        if (!isOpen()) {
            setError("SPI device not open");
            return {};
        }
        
        std::vector<uint8_t> rx_data(tx_data.size());
        
        struct spi_ioc_transfer tr = {
            .tx_buf = (unsigned long)tx_data.data(),
            .rx_buf = (unsigned long)rx_data.data(),
            .len = static_cast<uint32_t>(tx_data.size()),
            .speed_hz = config_.speed,
            .delay_usecs = config_.delay,
            .bits_per_word = config_.bits_per_word,
            .cs_change = config_.cs_change,
            .tx_nbits = 0,
            .rx_nbits = 0,
            .pad = 0
        };
        
        int ret = ioctl(fd_, SPI_IOC_MESSAGE(1), &tr);
        if (ret < 0) {
            setError("SPI transfer failed");
            return {};
        }
        
        if (transfer_callback_) {
            transfer_callback_(rx_data);
        }
        
        return rx_data;
    }
    
    void transferAsync(const SPITransfer& transfer) {
        if (!running_) {
            running_ = true;
            transferThread_ = std::thread(&Impl::asyncTransferLoop, this);
        }
        
        std::lock_guard<std::mutex> lock(queue_mutex_);
        transfer_queue_.push(transfer);
        queue_cv_.notify_one();
    }
    
    bool transferMultiple(const std::vector<SPITransfer>& transfers) {
        std::lock_guard<std::mutex> lock(transfer_mutex_);
        
        if (!isOpen()) {
            setError("SPI device not open");
            return false;
        }
        
        std::vector<spi_ioc_transfer> spi_transfers;
        std::vector<std::vector<uint8_t>> tx_buffers;
        std::vector<std::vector<uint8_t>> rx_buffers;
        
        for (const auto& t : transfers) {
            tx_buffers.push_back(t.tx_buffer);
            rx_buffers.push_back(std::vector<uint8_t>(t.tx_buffer.size()));
            
            spi_ioc_transfer tr = {
                .tx_buf = (unsigned long)tx_buffers.back().data(),
                .rx_buf = (unsigned long)rx_buffers.back().data(),
                .len = static_cast<uint32_t>(t.tx_buffer.size()),
                .speed_hz = t.speed > 0 ? t.speed : config_.speed,
                .delay_usecs = t.delay,
                .bits_per_word = t.bits_per_word > 0 ? t.bits_per_word : config_.bits_per_word,
                .cs_change = t.cs_change,
                .tx_nbits = 0,
                .rx_nbits = 0,
                .pad = 0
            };
            spi_transfers.push_back(tr);
        }
        
        int ret = ioctl(fd_, SPI_IOC_MESSAGE(spi_transfers.size()), 
                        spi_transfers.data());
        
        if (ret < 0) {
            setError("SPI multiple transfer failed");
            return false;
        }
        
        // Call callbacks for each transfer
        for (size_t i = 0; i < transfers.size(); ++i) {
            if (transfers[i].callback) {
                transfers[i].callback(rx_buffers[i]);
            }
        }
        
        return true;
    }
    
    void setSpeed(uint32_t speed) {
        config_.speed = speed;
        if (isOpen()) {
            if (ioctl(fd_, SPI_IOC_WR_MAX_SPEED_HZ, &speed) < 0) {
                setError("Failed to set SPI speed");
            }
        }
    }
    
    void setMode(SPIMode mode) {
        config_.mode = mode;
        if (isOpen()) {
            uint8_t mode_val = static_cast<uint8_t>(mode);
            if (ioctl(fd_, SPI_IOC_WR_MODE, &mode_val) < 0) {
                setError("Failed to set SPI mode");
            }
        }
    }
    
    void setBitsPerWord(uint8_t bits) {
        config_.bits_per_word = bits;
        if (isOpen()) {
            if (ioctl(fd_, SPI_IOC_WR_BITS_PER_WORD, &bits) < 0) {
                setError("Failed to set SPI bits per word");
            }
        }
    }
    
    void setBitOrder(SPIBitOrder order) {
        config_.bit_order = order;
        if (isOpen()) {
            uint8_t lsb = (order == SPI_LSB_FIRST) ? 1 : 0;
            if (ioctl(fd_, SPI_IOC_WR_LSB_FIRST, &lsb) < 0) {
                setError("Failed to set SPI bit order");
            }
        }
    }
    
    void setCSChange(bool enable) {
        config_.cs_change = enable ? 1 : 0;
        if (isOpen()) {
            if (ioctl(fd_, SPI_IOC_WR_CS_CHANGE, &config_.cs_change) < 0) {
                setError("Failed to set CS change");
            }
        }
    }
    
    uint32_t getSpeed() const { return config_.speed; }
    SPIMode getMode() const { return config_.mode; }
    uint8_t getBitsPerWord() const { return config_.bits_per_word; }
    
    bool isTransferComplete() const {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        return transfer_queue_.empty();
    }
    
    void setTransferCallback(std::function<void(const std::vector<uint8_t>&)> callback) {
        transfer_callback_ = callback;
    }
    
    void setErrorCallback(std::function<void(const std::string&)> callback) {
        error_callback_ = callback;
    }
    
private:
    SPIConfig config_;
    int fd_;
    std::mutex transfer_mutex_;
    std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    std::queue<SPITransfer> transfer_queue_;
    std::thread transferThread_;
    std::atomic<bool> running_;
    std::function<void(const std::vector<uint8_t>&)> transfer_callback_;
    std::function<void(const std::string&)> error_callback_;
    
    bool setMode(SPIMode mode) {
        uint8_t mode_val = static_cast<uint8_t>(mode);
        return ioctl(fd_, SPI_IOC_WR_MODE, &mode_val) >= 0;
    }
    
    bool setBitsPerWord(uint8_t bits) {
        return ioctl(fd_, SPI_IOC_WR_BITS_PER_WORD, &bits) >= 0;
    }
    
    bool setSpeed(uint32_t speed) {
        return ioctl(fd_, SPI_IOC_WR_MAX_SPEED_HZ, &speed) >= 0;
    }
    
    bool setBitOrder(SPIBitOrder order) {
        uint8_t lsb = (order == SPI_LSB_FIRST) ? 1 : 0;
        return ioctl(fd_, SPI_IOC_WR_LSB_FIRST, &lsb) >= 0;
    }
    
    void setError(const std::string& error) {
        if (error_callback_) {
            error_callback_(error);
        }
        std::cerr << "SPI Error: " << error << std::endl;
    }
    
    void asyncTransferLoop() {
        while (running_) {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            queue_cv_.wait_for(lock, std::chrono::milliseconds(100));
            
            while (!transfer_queue_.empty()) {
                auto transfer = transfer_queue_.front();
                transfer_queue_.pop();
                lock.unlock();
                
                // Execute transfer
                auto result = this->transfer(transfer.tx_buffer);
                if (!result.empty() && transfer.callback) {
                    transfer.callback(result);
                }
                
                lock.lock();
            }
        }
    }
};

// SPIDriver implementation
SPIDriver::SPIDriver(const SPIConfig& config) 
    : pImpl(std::make_unique<Impl>(config)) {}

SPIDriver::~SPIDriver() = default;

bool SPIDriver::initialize() { return pImpl->initialize(); }
void SPIDriver::close() { pImpl->close(); }
bool SPIDriver::isOpen() const { return pImpl->isOpen(); }

uint8_t SPIDriver::transferByte(uint8_t tx_data) {
    return pImpl->transferByte(tx_data);
}

std::vector<uint8_t> SPIDriver::transfer(const std::vector<uint8_t>& tx_data) {
    return pImpl->transfer(tx_data);
}

void SPIDriver::transferAsync(const SPITransfer& transfer) {
    pImpl->transferAsync(transfer);
}

bool SPIDriver::transferMultiple(const std::vector<SPITransfer>& transfers) {
    return pImpl->transferMultiple(transfers);
}

void SPIDriver::setSpeed(uint32_t speed) { pImpl->setSpeed(speed); }
void SPIDriver::setMode(SPIMode mode) { pImpl->setMode(mode); }
void SPIDriver::setBitsPerWord(uint8_t bits) { pImpl->setBitsPerWord(bits); }
void SPIDriver::setBitOrder(SPIBitOrder order) { pImpl->setBitOrder(order); }
void SPIDriver::setCSChange(bool enable) { pImpl->setCSChange(enable); }

uint32_t SPIDriver::getSpeed() const { return pImpl->getSpeed(); }
SPIMode SPIDriver::getMode() const { return pImpl->getMode(); }
uint8_t SPIDriver::getBitsPerWord() const { return pImpl->getBitsPerWord(); }
bool SPIDriver::isTransferComplete() const { return pImpl->isTransferComplete(); }

void SPIDriver::setTransferCallback(
    std::function<void(const std::vector<uint8_t>&)> callback) {
    pImpl->setTransferCallback(callback);
}

void SPIDriver::setErrorCallback(std::function<void(const std::string&)> callback) {
    pImpl->setErrorCallback(callback);
}

// SPIDevice implementation
SPIDevice::SPIDevice(SPIDriver& driver, uint8_t cs_pin) 
    : driver_(&driver), cs_pin_(cs_pin) {}

SPIDevice::~SPIDevice() = default;

uint8_t SPIDevice::readRegister(uint8_t reg) {
    std::lock_guard<std::mutex> lock(device_mutex_);
    
    // SPI read: send register address with read bit (0x80)
    std::vector<uint8_t> tx = {static_cast<uint8_t>(reg | 0x80), 0x00};
    auto rx = driver_->transfer(tx);
    return rx.empty() ? 0 : rx[1];
}

void SPIDevice::writeRegister(uint8_t reg, uint8_t value) {
    std::lock_guard<std::mutex> lock(device_mutex_);
    
    // SPI write: send register address and data
    std::vector<uint8_t> tx = {reg, value};
    driver_->transfer(tx);
}

std::vector<uint8_t> SPIDevice::readBurst(uint8_t reg, size_t count) {
    std::lock_guard<std::mutex> lock(device_mutex_);
    
    std::vector<uint8_t> tx(count + 1);
    tx[0] = static_cast<uint8_t>(reg | 0x80);
    std::fill(tx.begin() + 1, tx.end(), 0x00);
    
    auto rx = driver_->transfer(tx);
    if (rx.size() <= 1) return {};
    
    return std::vector<uint8_t>(rx.begin() + 1, rx.end());
}

void SPIDevice::writeBurst(uint8_t reg, const std::vector<uint8_t>& data) {
    std::lock_guard<std::mutex> lock(device_mutex_);
    
    std::vector<uint8_t> tx(1 + data.size());
    tx[0] = reg;
    std::copy(data.begin(), data.end(), tx.begin() + 1);
    
    driver_->transfer(tx);
}

} // namespace EdgeAI
