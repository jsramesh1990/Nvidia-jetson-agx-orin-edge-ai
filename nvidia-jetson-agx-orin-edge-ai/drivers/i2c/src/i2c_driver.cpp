#include "i2c/i2c_driver.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <poll.h>
#include <cstring>
#include <chrono>
#include <thread>

namespace EdgeAI {

class I2CDriver::Impl {
public:
    Impl(const I2CConfig& config) : config_(config), fd_(-1), running_(false) {}
    
    ~Impl() { close(); }
    
    bool initialize() {
        fd_ = open(config_.device.c_str(), O_RDWR);
        if (fd_ < 0) {
            setError("Failed to open I2C device: " + config_.device);
            return false;
        }
        
        // Set speed
        if (ioctl(fd_, I2C_FUNCS, &funcs_) < 0) {
            setError("Failed to get I2C functions");
            close();
            return false;
        }
        
        // Check if SMBus is supported
        if (config_.use_smbus && !(funcs_ & I2C_FUNC_SMBUS_EMUL)) {
            setError("SMBus not supported");
            config_.use_smbus = false;
        }
        
        return true;
    }
    
    void close() {
        if (fd_ >= 0) {
            ::close(fd_);
            fd_ = -1;
        }
    }
    
    bool isOpen() const { return fd_ >= 0; }
    
    bool write(uint8_t address, const std::vector<uint8_t>& data) {
        std::lock_guard<std::mutex> lock(transfer_mutex_);
        if (!isOpen()) {
            setError("I2C device not open");
            return false;
        }
        
        if (!setAddress(address)) return false;
        
        ssize_t n = ::write(fd_, data.data(), data.size());
        if (n != static_cast<ssize_t>(data.size())) {
            setError("I2C write failed");
            return false;
        }
        
        return true;
    }
    
    bool writeByte(uint8_t address, uint8_t byte) {
        std::vector<uint8_t> data = {byte};
        return write(address, data);
    }
    
    bool writeByteData(uint8_t address, uint8_t command, uint8_t data) {
        if (config_.use_smbus && (funcs_ & I2C_FUNC_SMBUS_WRITE_BYTE_DATA)) {
            return smbusWriteByteData(address, command, data);
        }
        
        std::vector<uint8_t> data_vec = {command, data};
        return write(address, data_vec);
    }
    
    bool writeBlockData(uint8_t address, uint8_t command, 
                        const std::vector<uint8_t>& data) {
        if (config_.use_smbus && (funcs_ & I2C_FUNC_SMBUS_WRITE_BLOCK_DATA)) {
            return smbusWriteBlockData(address, command, data);
        }
        
        std::vector<uint8_t> data_vec = {command};
        data_vec.insert(data_vec.end(), data.begin(), data.end());
        return write(address, data_vec);
    }
    
    std::vector<uint8_t> read(uint8_t address, size_t count) {
        std::lock_guard<std::mutex> lock(transfer_mutex_);
        if (!isOpen()) {
            setError("I2C device not open");
            return {};
        }
        
        if (!setAddress(address)) return {};
        
        std::vector<uint8_t> data(count);
        ssize_t n = ::read(fd_, data.data(), count);
        if (n != static_cast<ssize_t>(count)) {
            setError("I2C read failed");
            return {};
        }
        
        return data;
    }
    
    uint8_t readByte(uint8_t address) {
        auto data = read(address, 1);
        return data.empty() ? 0 : data[0];
    }
    
    uint8_t readByteData(uint8_t address, uint8_t command) {
        if (config_.use_smbus && (funcs_ & I2C_FUNC_SMBUS_READ_BYTE_DATA)) {
            return smbusReadByteData(address, command);
        }
        
        if (!write(address, {command})) return 0;
        return readByte(address);
    }
    
    std::vector<uint8_t> readBlockData(uint8_t address, uint8_t command,
                                       size_t count) {
        if (config_.use_smbus && (funcs_ & I2C_FUNC_SMBUS_READ_BLOCK_DATA)) {
            return smbusReadBlockData(address, command, count);
        }
        
        if (!write(address, {command})) return {};
        return read(address, count);
    }
    
    std::vector<uint8_t> writeRead(uint8_t address,
                                   const std::vector<uint8_t>& write_data,
                                   size_t read_count) {
        std::lock_guard<std::mutex> lock(transfer_mutex_);
        if (!isOpen()) {
            setError("I2C device not open");
            return {};
        }
        
        if (!setAddress(address)) return {};
        
        // Write data
        if (!write_data.empty()) {
            ssize_t n = ::write(fd_, write_data.data(), write_data.size());
            if (n != static_cast<ssize_t>(write_data.size())) {
                setError("I2C write failed in combined transaction");
                return {};
            }
        }
        
        // Read data
        if (read_count > 0) {
            std::vector<uint8_t> data(read_count);
            ssize_t n = ::read(fd_, data.data(), read_count);
            if (n != static_cast<ssize_t>(read_count)) {
                setError("I2C read failed in combined transaction");
                return {};
            }
            return data;
        }
        
        return {};
    }
    
    bool readAsync(uint8_t address, size_t count,
                   std::function<void(const std::vector<uint8_t>&)> callback) {
        if (!isOpen()) return false;
        
        std::thread([this, address, count, callback]() {
            auto data = this->read(address, count);
            if (callback) {
                callback(data);
            }
        }).detach();
        
        return true;
    }
    
    bool writeAsync(uint8_t address, const std::vector<uint8_t>& data,
                    std::function<void(bool)> callback) {
        if (!isOpen()) return false;
        
        std::thread([this, address, data, callback]() {
            bool success = this->write(address, data);
            if (callback) {
                callback(success);
            }
        }).detach();
        
        return true;
    }
    
    bool transactionAsync(const I2CTransaction& transaction) {
        if (!isOpen()) return false;
        
        std::thread([this, transaction]() {
            bool success = false;
            std::vector<uint8_t> result;
            
            if (!transaction.write_data.empty() && 
                !transaction.read_data.empty()) {
                // Combined transaction
                result = this->writeRead(transaction.address,
                                         transaction.write_data,
                                         transaction.read_data.size());
                success = !result.empty();
            } else if (!transaction.write_data.empty()) {
                // Write only
                success = this->write(transaction.address, 
                                      transaction.write_data);
            } else if (!transaction.read_data.empty()) {
                // Read only
                result = this->read(transaction.address, 
                                    transaction.read_data.size());
                success = !result.empty();
            }
            
            if (transaction.callback) {
                transaction.callback(success, result);
            }
        }).detach();
        
        return true;
    }
    
    void setSpeed(int speed) {
        config_.speed = speed;
        if (isOpen()) {
            // Speed is usually set in the device tree
            // but we can try to set it via ioctl if supported
        }
    }
    
    void setAddress(uint8_t address) {
        config_.address = address;
    }
    
    void setTenBit(bool enable) {
        config_.ten_bit = enable;
    }
    
    void setTimeout(int ms) {
        config_.timeout_ms = ms;
    }
    
    int getSpeed() const { return config_.speed; }
    uint8_t getAddress() const { return config_.address; }
    bool isTenBit() const { return config_.ten_bit; }
    
    bool scanBus(std::vector<uint8_t>& found_addresses) {
        found_addresses.clear();
        
        for (uint8_t addr = 0x08; addr <= 0x77; addr++) {
            if (addr == 0x00) continue;
            
            // Try to read 1 byte from each address
            if (setAddress(addr)) {
                uint8_t buf;
                ssize_t n = ::read(fd_, &buf, 1);
                if (n >= 0 || errno == ENXIO) {
                    // Device responded
                    found_addresses.push_back(addr);
                }
            }
            
            // Reset address
            setAddress(config_.address);
        }
        
        return !found_addresses.empty();
    }
    
    void setErrorCallback(std::function<void(const std::string&)> callback) {
        error_callback_ = callback;
    }
    
private:
    I2CConfig config_;
    int fd_;
    unsigned long funcs_;
    std::mutex transfer_mutex_;
    std::function<void(const std::string&)> error_callback_;
    
    bool setAddress(uint8_t address) {
        if (ioctl(fd_, I2C_SLAVE_FORCE, address) < 0) {
            setError("Failed to set I2C address");
            return false;
        }
        return true;
    }
    
    bool smbusWriteByteData(uint8_t address, uint8_t command, uint8_t data) {
        struct i2c_smbus_ioctl_data args;
        uint8_t block[2] = {command, data};
        
        args.read_write = I2C_SMBUS_WRITE;
        args.command = command;
        args.size = I2C_SMBUS_BYTE_DATA;
        args.data = reinterpret_cast<union i2c_smbus_data*>(block);
        
        if (ioctl(fd_, I2C_SMBUS, &args) < 0) {
            setError("SMBus write byte data failed");
            return false;
        }
        return true;
    }
    
    uint8_t smbusReadByteData(uint8_t address, uint8_t command) {
        struct i2c_smbus_ioctl_data args;
        uint8_t data;
        
        args.read_write = I2C_SMBUS_READ;
        args.command = command;
        args.size = I2C_SMBUS_BYTE_DATA;
        args.data = reinterpret_cast<union i2c_smbus_data*>(&data);
        
        if (ioctl(fd_, I2C_SMBUS, &args) < 0) {
            setError("SMBus read byte data failed");
            return 0;
        }
        return data;
    }
    
    bool smbusWriteBlockData(uint8_t address, uint8_t command,
                             const std::vector<uint8_t>& data) {
        struct i2c_smbus_ioctl_data args;
        uint8_t block[I2C_SMBUS_BLOCK_MAX + 1];
        block[0] = data.size();
        memcpy(block + 1, data.data(), std::min(data.size(), 
                                                (size_t)I2C_SMBUS_BLOCK_MAX));
        
        args.read_write = I2C_SMBUS_WRITE;
        args.command = command;
        args.size = I2C_SMBUS_BLOCK_DATA;
        args.data = reinterpret_cast<union i2c_smbus_data*>(block);
        
        if (ioctl(fd_, I2C_SMBUS, &args) < 0) {
            setError("SMBus write block data failed");
            return false;
        }
        return true;
    }
    
    std::vector<uint8_t> smbusReadBlockData(uint8_t address, uint8_t command,
                                            size_t count) {
        struct i2c_smbus_ioctl_data args;
        uint8_t block[I2C_SMBUS_BLOCK_MAX + 1];
        
        args.read_write = I2C_SMBUS_READ;
        args.command = command;
        args.size = I2C_SMBUS_BLOCK_DATA;
        args.data = reinterpret_cast<union i2c_smbus_data*>(block);
        
        if (ioctl(fd_, I2C_SMBUS, &args) < 0) {
            setError("SMBus read block data failed");
            return {};
        }
        
        uint8_t length = std::min(block[0], (uint8_t)count);
        return std::vector<uint8_t>(block + 1, block + 1 + length);
    }
    
    void setError(const std::string& error) {
        if (error_callback_) {
            error_callback_(error);
        }
        std::cerr << "I2C Error: " << error << std::endl;
    }
};

// I2CDriver implementation
I2CDriver::I2CDriver(const I2CConfig& config) 
    : pImpl(std::make_unique<Impl>(config)) {}

I2CDriver::~I2CDriver() = default;

bool I2CDriver::initialize() { return pImpl->initialize(); }
void I2CDriver::close() { pImpl->close(); }
bool I2CDriver::isOpen() const { return pImpl->isOpen(); }

bool I2CDriver::write(uint8_t address, const std::vector<uint8_t>& data) {
    return pImpl->write(address, data);
}
bool I2CDriver::writeByte(uint8_t address, uint8_t byte) {
    return pImpl->writeByte(address, byte);
}
bool I2CDriver::writeByteData(uint8_t address, uint8_t command, uint8_t data) {
    return pImpl->writeByteData(address, command, data);
}
bool I2CDriver::writeBlockData(uint8_t address, uint8_t command,
                               const std::vector<uint8_t>& data) {
    return pImpl->writeBlockData(address, command, data);
}

std::vector<uint8_t> I2CDriver::read(uint8_t address, size_t count) {
    return pImpl->read(address, count);
}
uint8_t I2CDriver::readByte(uint8_t address) {
    return pImpl->readByte(address);
}
uint8_t I2CDriver::readByteData(uint8_t address, uint8_t command) {
    return pImpl->readByteData(address, command);
}
std::vector<uint8_t> I2CDriver::readBlockData(uint8_t address, uint8_t command,
                                              size_t count) {
    return pImpl->readBlockData(address, command, count);
}

std::vector<uint8_t> I2CDriver::writeRead(uint8_t address,
                                          const std::vector<uint8_t>& write_data,
                                          size_t read_count) {
    return pImpl->writeRead(address, write_data, read_count);
}

bool I2CDriver::readAsync(uint8_t address, size_t count,
                          std::function<void(const std::vector<uint8_t>&)> callback) {
    return pImpl->readAsync(address, count, callback);
}
bool I2CDriver::writeAsync(uint8_t address, const std::vector<uint8_t>& data,
                           std::function<void(bool)> callback) {
    return pImpl->writeAsync(address, data, callback);
}
bool I2CDriver::transactionAsync(const I2CTransaction& transaction) {
    return pImpl->transactionAsync(transaction);
}

void I2CDriver::setSpeed(int speed) { pImpl->setSpeed(speed); }
void I2CDriver::setAddress(uint8_t address) { pImpl->setAddress(address); }
void I2CDriver::setTenBit(bool enable) { pImpl->setTenBit(enable); }
void I2CDriver::setTimeout(int ms) { pImpl->setTimeout(ms); }

int I2CDriver::getSpeed() const { return pImpl->getSpeed(); }
uint8_t I2CDriver::getAddress() const { return pImpl->getAddress(); }
bool I2CDriver::isTenBit() const { return pImpl->isTenBit(); }

bool I2CDriver::scanBus(std::vector<uint8_t>& found_addresses) {
    return pImpl->scanBus(found_addresses);
}

void I2CDriver::setErrorCallback(std::function<void(const std::string&)> callback) {
    pImpl->setErrorCallback(callback);
}

// I2CDevice implementation
I2CDevice::I2CDevice(I2CDriver& driver, uint8_t address) 
    : driver_(&driver), address_(address) {}

I2CDevice::~I2CDevice() = default;

uint8_t I2CDevice::readReg(uint8_t reg) {
    std::lock_guard<std::mutex> lock(device_mutex_);
    return driver_->readByteData(address_, reg);
}

void I2CDevice::writeReg(uint8_t reg, uint8_t value) {
    std::lock_guard<std::mutex> lock(device_mutex_);
    driver_->writeByteData(address_, reg, value);
}

uint16_t I2CDevice::readReg16(uint8_t reg) {
    std::lock_guard<std::mutex> lock(device_mutex_);
    auto data = driver_->readBlockData(address_, reg, 2);
    if (data.size() < 2) return 0;
    return (data[0] << 8) | data[1];
}

void I2CDevice::writeReg16(uint8_t reg, uint16_t value) {
    std::lock_guard<std::mutex> lock(device_mutex_);
    std::vector<uint8_t> data = {
        static_cast<uint8_t>((value >> 8) & 0xFF),
        static_cast<uint8_t>(value & 0xFF)
    };
    driver_->writeBlockData(address_, reg, data);
}

std::vector<uint8_t> I2CDevice::readRegBlock(uint8_t reg, size_t count) {
    std::lock_guard<std::mutex> lock(device_mutex_);
    return driver_->readBlockData(address_, reg, count);
}

void I2CDevice::writeRegBlock(uint8_t reg, const std::vector<uint8_t>& data) {
    std::lock_guard<std::mutex> lock(device_mutex_);
    driver_->writeBlockData(address_, reg, data);
}

int16_t I2CDevice::readTwoBytes(uint8_t reg) {
    auto data = readRegBlock(reg, 2);
    if (data.size() < 2) return 0;
    return static_cast<int16_t>((data[0] << 8) | data[1]);
}

float I2CDevice::readFloat(uint8_t reg) {
    auto data = readRegBlock(reg, 4);
    if (data.size() < 4) return 0.0f;
    
    union {
        uint32_t u32;
        float f32;
    } converter;
    
    converter.u32 = (data[0] << 24) | (data[1] << 16) | 
                    (data[2] << 8) | data[3];
    return converter.f32;
}

} // namespace EdgeAI
