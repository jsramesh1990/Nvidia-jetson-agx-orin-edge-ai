#include "uart/uart_async.h"
#include <chrono>
#include <iostream>

namespace EdgeAI {

class UARTAsync::Impl {
public:
    Impl(UARTDriver& driver) : driver_(driver), running_(false), max_queue_size_(1024) {}
    
    ~Impl() {
        stop();
    }
    
    bool start() {
        if (running_) return true;
        
        running_ = true;
        worker_thread_ = std::thread(&Impl::workerLoop, this);
        return true;
    }
    
    void stop() {
        if (!running_) return;
        
        running_ = false;
        queue_cv_.notify_all();
        
        if (worker_thread_.joinable()) {
            worker_thread_.join();
        }
        
        // Clear queue
        std::lock_guard<std::mutex> lock(queue_mutex_);
        while (!operation_queue_.empty()) {
            operation_queue_.pop();
        }
    }
    
    bool isRunning() const {
        return running_;
    }
    
    bool readAsync(size_t size, std::function<void(const std::vector<uint8_t>&)> callback) {
        if (!running_) return false;
        
        std::lock_guard<std::mutex> lock(queue_mutex_);
        
        if (operation_queue_.size() >= max_queue_size_) {
            return false;
        }
        
        operation_queue_.push([this, size, callback]() {
            auto start = std::chrono::steady_clock::now();
            bool success = false;
            std::vector<uint8_t> data;
            
            try {
                data = driver_.readBytes(size);
                success = !data.empty();
            } catch (const std::exception& e) {
                if (error_callback_) {
                    error_callback_(std::string("Read error: ") + e.what());
                }
            }
            
            auto end = std::chrono::steady_clock::now();
            double duration = std::chrono::duration<double, std::milli>(end - start).count();
            
            updateStats(duration, true, success);
            
            if (callback && success) {
                callback(data);
            }
            
            if (data_callback_ && success) {
                UARTData uart_data;
                uart_data.data = data;
                uart_data.timestamp = std::chrono::system_clock::now();
                uart_data.bytes_read = data.size();
                uart_data.elapsed_ms = duration;
                data_callback_(uart_data);
            }
        });
        
        queue_cv_.notify_one();
        return true;
    }
    
    bool readUntilAsync(const std::string& delimiter,
                        std::function<void(const std::string&)> callback) {
        if (!running_) return false;
        
        std::lock_guard<std::mutex> lock(queue_mutex_);
        
        if (operation_queue_.size() >= max_queue_size_) {
            return false;
        }
        
        operation_queue_.push([this, delimiter, callback]() {
            auto start = std::chrono::steady_clock::now();
            bool success = false;
            std::string result;
            
            try {
                result = driver_.readString();
                success = !result.empty();
            } catch (const std::exception& e) {
                if (error_callback_) {
                    error_callback_(std::string("Read error: ") + e.what());
                }
            }
            
            auto end = std::chrono::steady_clock::now();
            double duration = std::chrono::duration<double, std::milli>(end - start).count();
            
            updateStats(duration, true, success);
            
            if (callback && success) {
                callback(result);
            }
        });
        
        queue_cv_.notify_one();
        return true;
    }
    
    bool readTimeoutAsync(int timeout_ms,
                          std::function<void(const std::vector<uint8_t>&)> callback) {
        if (!running_) return false;
        
        std::lock_guard<std::mutex> lock(queue_mutex_);
        
        if (operation_queue_.size() >= max_queue_size_) {
            return false;
        }
        
        operation_queue_.push([this, timeout_ms, callback]() {
            auto start = std::chrono::steady_clock::now();
            bool success = false;
            std::vector<uint8_t> data;
            
            try {
                data = driver_.readBytes(1024, timeout_ms);
                success = !data.empty();
            } catch (const std::exception& e) {
                if (error_callback_) {
                    error_callback_(std::string("Read error: ") + e.what());
                }
            }
            
            auto end = std::chrono::steady_clock::now();
            double duration = std::chrono::duration<double, std::milli>(end - start).count();
            
            updateStats(duration, true, success);
            
            if (callback && success) {
                callback(data);
            }
        });
        
        queue_cv_.notify_one();
        return true;
    }
    
    bool readPatternAsync(const std::string& pattern,
                          std::function<void(const std::string&)> callback) {
        // Simplified pattern matching - just read until delimiter
        return readUntilAsync(pattern, callback);
    }
    
    bool writeAsync(const std::vector<uint8_t>& data,
                    std::function<void(bool)> callback) {
        if (!running_) return false;
        
        std::lock_guard<std::mutex> lock(queue_mutex_);
        
        if (operation_queue_.size() >= max_queue_size_) {
            return false;
        }
        
        operation_queue_.push([this, data, callback]() {
            auto start = std::chrono::steady_clock::now();
            bool success = false;
            
            try {
                success = driver_.writeBytes(data);
            } catch (const std::exception& e) {
                if (error_callback_) {
                    error_callback_(std::string("Write error: ") + e.what());
                }
            }
            
            auto end = std::chrono::steady_clock::now();
            double duration = std::chrono::duration<double, std::milli>(end - start).count();
            
            updateStats(duration, false, success);
            
            if (callback) {
                callback(success);
            }
        });
        
        queue_cv_.notify_one();
        return true;
    }
    
    bool writeStringAsync(const std::string& str,
                          std::function<void(bool)> callback) {
        std::vector<uint8_t> data(str.begin(), str.end());
        return writeAsync(data, callback);
    }
    
    bool writeLineAsync(const std::string& line,
                        std::function<void(bool)> callback) {
        std::string line_with_newline = line + "\n";
        return writeStringAsync(line_with_newline, callback);
    }
    
    bool writeWithAckAsync(const std::vector<uint8_t>& data,
                           uint8_t expected_ack,
                           std::function<void(bool)> callback) {
        if (!running_) return false;
        
        std::lock_guard<std::mutex> lock(queue_mutex_);
        
        if (operation_queue_.size() >= max_queue_size_) {
            return false;
        }
        
        operation_queue_.push([this, data, expected_ack, callback]() {
            bool success = false;
            
            try {
                success = driver_.writeBytes(data);
                if (success) {
                    uint8_t ack = driver_.readByte();
                    success = (ack == expected_ack);
                }
            } catch (const std::exception& e) {
                if (error_callback_) {
                    error_callback_(std::string("Write with ACK error: ") + e.what());
                }
            }
            
            if (callback) {
                callback(success);
            }
        });
        
        queue_cv_.notify_one();
        return true;
    }
    
    bool transactionAsync(const std::vector<uint8_t>& write_data,
                          size_t read_size,
                          std::function<void(const std::vector<uint8_t>&)> callback) {
        if (!running_) return false;
        
        std::lock_guard<std::mutex> lock(queue_mutex_);
        
        if (operation_queue_.size() >= max_queue_size_) {
            return false;
        }
        
        operation_queue_.push([this, write_data, read_size, callback]() {
            auto start = std::chrono::steady_clock::now();
            bool success = false;
            std::vector<uint8_t> response;
            
            try {
                success = driver_.writeBytes(write_data);
                if (success) {
                    response = driver_.readBytes(read_size);
                    success = !response.empty();
                }
            } catch (const std::exception& e) {
                if (error_callback_) {
                    error_callback_(std::string("Transaction error: ") + e.what());
                }
            }
            
            auto end = std::chrono::steady_clock::now();
            double duration = std::chrono::duration<double, std::milli>(end - start).count();
            
            updateStats(duration, true, success);
            
            if (callback && success) {
                callback(response);
            }
        });
        
        queue_cv_.notify_one();
        return true;
    }
    
    void setDataCallback(std::function<void(const UARTData&)> callback) {
        data_callback_ = callback;
    }
    
    void setErrorCallback(std::function<void(const std::string&)> callback) {
        error_callback_ = callback;
    }
    
    size_t getPendingOperations() const {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        return operation_queue_.size();
    }
    
    void clearPendingOperations() {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        while (!operation_queue_.empty()) {
            operation_queue_.pop();
        }
    }
    
    void setMaxQueueSize(size_t size) {
        max_queue_size_ = size;
    }
    
    QueueStats getStats() const {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        QueueStats stats;
        stats.total_operations = stats_.total_operations;
        stats.completed_operations = stats_.completed_operations;
        stats.failed_operations = stats_.failed_operations;
        stats.current_queue_size = getPendingOperations();
        stats.max_queue_size = max_queue_size_;
        stats.avg_read_time_ms = stats_.avg_read_time_ms;
        stats.avg_write_time_ms = stats_.avg_write_time_ms;
        stats.avg_transaction_time_ms = stats_.avg_transaction_time_ms;
        return stats;
    }
    
    void resetStats() {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_ = Stats();
    }
    
    bool waitForCompletion(int timeout_ms) {
        auto start = std::chrono::steady_clock::now();
        
        while (getPendingOperations() > 0) {
            if (timeout_ms >= 0) {
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - start).count();
                if (elapsed > timeout_ms) {
                    return false;
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        
        return true;
    }
    
    size_t cancelAll() {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        size_t count = operation_queue_.size();
        while (!operation_queue_.empty()) {
            operation_queue_.pop();
        }
        return count;
    }
    
    bool setThreadPriority(int priority) {
        // Platform-specific thread priority setting
        // For now, just return success
        return true;
    }
    
    void setDebugMode(bool enable) {
        debug_mode_ = enable;
    }
    
private:
    UARTDriver& driver_;
    std::atomic<bool> running_;
    std::thread worker_thread_;
    mutable std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    std::queue<std::function<void()>> operation_queue_;
    std::function<void(const UARTData&)> data_callback_;
    std::function<void(const std::string&)> error_callback_;
    size_t max_queue_size_;
    bool debug_mode_ = false;
    
    struct Stats {
        std::atomic<size_t> total_operations{0};
        std::atomic<size_t> completed_operations{0};
        std::atomic<size_t> failed_operations{0};
        double avg_read_time_ms = 0.0;
        double avg_write_time_ms = 0.0;
        double avg_transaction_time_ms = 0.0;
    };
    mutable std::mutex stats_mutex_;
    Stats stats_;
    
    void workerLoop() {
        while (running_) {
            std::function<void()> operation;
            
            {
                std::unique_lock<std::mutex> lock(queue_mutex_);
                queue_cv_.wait_for(lock, std::chrono::milliseconds(100),
                    [this]() { return !operation_queue_.empty() || !running_; });
                
                if (!running_) break;
                
                if (operation_queue_.empty()) continue;
                
                operation = std::move(operation_queue_.front());
                operation_queue_.pop();
            }
            
            if (operation) {
                try {
                    operation();
                    stats_.completed_operations++;
                } catch (const std::exception& e) {
                    stats_.failed_operations++;
                    if (error_callback_) {
                        error_callback_(std::string("Operation failed: ") + e.what());
                    }
                }
                stats_.total_operations++;
            }
        }
    }
    
    void updateStats(double duration_ms, bool is_read, bool success) {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        
        if (is_read) {
            if (stats_.avg_read_time_ms == 0.0) {
                stats_.avg_read_time_ms = duration_ms;
            } else {
                stats_.avg_read_time_ms = (stats_.avg_read_time_ms * 0.9) + (duration_ms * 0.1);
            }
        } else {
            if (stats_.avg_write_time_ms == 0.0) {
                stats_.avg_write_time_ms = duration_ms;
            } else {
                stats_.avg_write_time_ms = (stats_.avg_write_time_ms * 0.9) + (duration_ms * 0.1);
            }
        }
        
        if (!success) {
            stats_.failed_operations++;
        }
        stats_.completed_operations++;
        stats_.total_operations++;
    }
};

// UARTAsync implementation
UARTAsync::UARTAsync(UARTDriver& driver) 
    : driver_(driver), pImpl(std::make_unique<Impl>(driver)) {}

UARTAsync::~UARTAsync() = default;

bool UARTAsync::start() { return pImpl->start(); }
void UARTAsync::stop() { pImpl->stop(); }
bool UARTAsync::isRunning() const { return pImpl->isRunning(); }

bool UARTAsync::readAsync(size_t size, 
                          std::function<void(const std::vector<uint8_t>&)> callback) {
    return pImpl->readAsync(size, callback);
}
bool UARTAsync::readUntilAsync(const std::string& delimiter,
                               std::function<void(const std::string&)> callback) {
    return pImpl->readUntilAsync(delimiter, callback);
}
bool UARTAsync::readTimeoutAsync(int timeout_ms,
                                 std::function<void(const std::vector<uint8_t>&)> callback) {
    return pImpl->readTimeoutAsync(timeout_ms, callback);
}
bool UARTAsync::readPatternAsync(const std::string& pattern,
                                 std::function<void(const std::string&)> callback) {
    return pImpl->readPatternAsync(pattern, callback);
}

bool UARTAsync::writeAsync(const std::vector<uint8_t>& data,
                           std::function<void(bool)> callback) {
    return pImpl->writeAsync(data, callback);
}
bool UARTAsync::writeStringAsync(const std::string& str,
                                 std::function<void(bool)> callback) {
    return pImpl->writeStringAsync(str, callback);
}
bool UARTAsync::writeLineAsync(const std::string& line,
                               std::function<void(bool)> callback) {
    return pImpl->writeLineAsync(line, callback);
}
bool UARTAsync::writeWithAckAsync(const std::vector<uint8_t>& data,
                                  uint8_t expected_ack,
                                  std::function<void(bool)> callback) {
    return pImpl->writeWithAckAsync(data, expected_ack, callback);
}
bool UARTAsync::transactionAsync(const std::vector<uint8_t>& write_data,
                                 size_t read_size,
                                 std::function<void(const std::vector<uint8_t>&)> callback) {
    return pImpl->transactionAsync(write_data, read_size, callback);
}

void UARTAsync::setDataCallback(std::function<void(const UARTData&)> callback) {
    pImpl->setDataCallback(callback);
}
void UARTAsync::setErrorCallback(std::function<void(const std::string&)> callback) {
    pImpl->setErrorCallback(callback);
}

size_t UARTAsync::getPendingOperations() const {
    return pImpl->getPendingOperations();
}
void UARTAsync::clearPendingOperations() {
    pImpl->clearPendingOperations();
}
void UARTAsync::setMaxQueueSize(size_t size) {
    pImpl->setMaxQueueSize(size);
}

UARTAsync::QueueStats UARTAsync::getStats() const {
    return pImpl->getStats();
}
void UARTAsync::resetStats() {
    pImpl->resetStats();
}

bool UARTAsync::waitForCompletion(int timeout_ms) {
    return pImpl->waitForCompletion(timeout_ms);
}
size_t UARTAsync::cancelAll() {
    return pImpl->cancelAll();
}
bool UARTAsync::setThreadPriority(int priority) {
    return pImpl->setThreadPriority(priority);
}
void UARTAsync::setDebugMode(bool enable) {
    pImpl->setDebugMode(enable);
}

} // namespace EdgeAI
