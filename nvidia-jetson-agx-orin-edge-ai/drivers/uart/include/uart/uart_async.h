#pragma once
#include "uart_driver.h"
#include <functional>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <thread>

namespace EdgeAI {

/**
 * @brief Asynchronous UART operation manager
 * 
 * This class handles asynchronous read/write operations on UART
 * with callback support and queuing.
 */
class UARTAsync {
public:
    UARTAsync(UARTDriver& driver);
    ~UARTAsync();

    /**
     * @brief Start async operations
     * @return true if started successfully
     */
    bool start();

    /**
     * @brief Stop async operations
     */
    void stop();

    /**
     * @brief Check if async operations are running
     * @return true if running
     */
    bool isRunning() const { return running_; }

    /**
     * @brief Asynchronously read data
     * @param size Number of bytes to read
     * @param callback Function called when data is read
     * @return true if queued successfully
     */
    bool readAsync(size_t size, std::function<void(const std::vector<uint8_t>&)> callback);

    /**
     * @brief Asynchronously read until delimiter
     * @param delimiter Delimiter string
     * @param callback Function called when data is read
     * @return true if queued successfully
     */
    bool readUntilAsync(const std::string& delimiter,
                        std::function<void(const std::string&)> callback);

    /**
     * @brief Asynchronously write data
     * @param data Data to write
     * @param callback Function called when write completes
     * @return true if queued successfully
     */
    bool writeAsync(const std::vector<uint8_t>& data,
                    std::function<void(bool)> callback = nullptr);

    /**
     * @brief Asynchronously write string
     * @param str String to write
     * @param callback Function called when write completes
     * @return true if queued successfully
     */
    bool writeStringAsync(const std::string& str,
                          std::function<void(bool)> callback = nullptr);

    /**
     * @brief Asynchronously write line (with newline)
     * @param line Line to write
     * @param callback Function called when write completes
     * @return true if queued successfully
     */
    bool writeLineAsync(const std::string& line,
                        std::function<void(bool)> callback = nullptr);

    /**
     * @brief Set callback for asynchronous data reception
     * @param callback Function called when data is received
     */
    void setDataCallback(std::function<void(const UARTData&)> callback);

    /**
     * @brief Set callback for errors
     * @param callback Function called on error
     */
    void setErrorCallback(std::function<void(const std::string&)> callback);

    /**
     * @brief Get pending operations count
     * @return Number of pending operations
     */
    size_t getPendingOperations() const;

    /**
     * @brief Clear all pending operations
     */
    void clearPendingOperations();

    /**
     * @brief Set maximum queue size
     * @param size Maximum operations in queue
     */
    void setMaxQueueSize(size_t size);

    /**
     * @brief Get queue statistics
     */
    struct QueueStats {
        size_t total_operations = 0;
        size_t completed_operations = 0;
        size_t failed_operations = 0;
        size_t current_queue_size = 0;
        double avg_read_time_ms = 0.0;
        double avg_write_time_ms = 0.0;
    };
    QueueStats getStats() const;
    void resetStats();

    /**
     * @brief Wait for all pending operations to complete
     * @param timeout_ms Maximum time to wait in milliseconds (-1 for forever)
     * @return true if all completed
     */
    bool waitForCompletion(int timeout_ms = -1);

private:
    class Impl;
    std::unique_ptr<Impl> pImpl;

    UARTDriver& driver_;
    std::atomic<bool> running_{false};
    std::thread worker_thread_;
    mutable std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    std::queue<std::function<void()>> operation_queue_;
    std::function<void(const UARTData&)> data_callback_;
    std::function<void(const std::string&)> error_callback_;
    size_t max_queue_size_ = 1024;

    // Statistics
    struct Stats {
        std::atomic<size_t> total_operations{0};
        std::atomic<size_t> completed_operations{0};
        std::atomic<size_t> failed_operations{0};
        std::atomic<size_t> current_queue_size{0};
        double avg_read_time_ms = 0.0;
        double avg_write_time_ms = 0.0;
        mutable std::mutex stats_mutex;
    };
    Stats stats_;

    void workerLoop();
    void processOperation();
    void updateStats(double duration_ms, bool is_read, bool success);
};

} // namespace EdgeAI
