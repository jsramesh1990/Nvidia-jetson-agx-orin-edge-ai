#pragma once
#include <string>
#include <vector>
#include <functional>
#include <map>
#include <chrono>

namespace EdgeAI {

struct StorageConfig {
    std::string root_path = "/var/edge_ai/data";
    std::string temp_path = "/tmp/edge_ai";
    std::string backup_path = "/var/backups/edge_ai";
    std::string log_path = "/var/log/edge_ai";
    size_t max_size_gb = 100;
    size_t max_file_size_mb = 100;
    bool auto_cleanup = true;
    bool compression_enabled = true;
    bool encryption_enabled = false;
    std::string encryption_key = "";
    int cleanup_days = 30;
    int max_backup_count = 10;
};

struct FileInfo {
    std::string path;
    std::string name;
    std::string extension;
    size_t size_bytes = 0;
    std::chrono::system_clock::time_point created;
    std::chrono::system_clock::time_point modified;
    bool is_directory = false;
    bool is_readonly = false;
    bool is_hidden = false;
    bool is_symlink = false;
    std::string owner = "";
    std::string group = "";
    int permissions = 0644;
    std::string mime_type = "";
    std::string checksum_md5 = "";
    std::string checksum_sha256 = "";
};

struct StorageStats {
    size_t total_space_gb = 0;
    size_t used_space_gb = 0;
    size_t free_space_gb = 0;
    size_t file_count = 0;
    size_t directory_count = 0;
    float disk_usage_percentage = 0.0f;
    size_t total_bytes_written = 0;
    size_t total_bytes_read = 0;
    int file_system_type = 0;
    size_t block_size = 0;
    size_t max_file_size = 0;
};

struct StorageTransaction {
    enum Type {
        READ = 0,
        WRITE = 1,
        DELETE = 2,
        MOVE = 3,
        COPY = 4
    };
    
    Type type;
    std::string source_path;
    std::string dest_path;
    size_t size = 0;
    int64_t timestamp;
    bool success = false;
    std::string error_message;
    double duration_ms = 0.0;
};

class StorageDriver {
public:
    StorageDriver(const StorageConfig& config);
    ~StorageDriver();
    
    // Initialization
    bool initialize();
    bool isReady() const;
    void shutdown();
    
    // File operations
    bool createFile(const std::string& path, const std::vector<uint8_t>& data,
                    bool overwrite = false);
    bool createTextFile(const std::string& path, const std::string& content,
                        bool overwrite = false);
    bool readFile(const std::string& path, std::vector<uint8_t>& data);
    bool readTextFile(const std::string& path, std::string& content);
    bool deleteFile(const std::string& path);
    bool copyFile(const std::string& source, const std::string& destination);
    bool moveFile(const std::string& source, const std::string& destination);
    bool renameFile(const std::string& old_path, const std::string& new_path);
    bool fileExists(const std::string& path) const;
    bool isFileLocked(const std::string& path) const;
    bool lockFile(const std::string& path);
    bool unlockFile(const std::string& path);
    
    // Directory operations
    bool createDirectory(const std::string& path, bool recursive = true);
    bool deleteDirectory(const std::string& path, bool recursive = false);
    bool directoryExists(const std::string& path) const;
    bool isDirectoryEmpty(const std::string& path) const;
    std::vector<FileInfo> listDirectory(const std::string& path, 
                                        bool recursive = false,
                                        bool include_hidden = false) const;
    std::vector<FileInfo> findFiles(const std::string& path,
                                    const std::string& pattern = "*",
                                    bool recursive = true) const;
    
    // File I/O streaming
    class FileStream {
    public:
        bool open(const std::string& path, const std::string& mode);
        void close();
        bool write(const std::vector<uint8_t>& data);
        bool writeText(const std::string& text);
        bool read(std::vector<uint8_t>& data, size_t count);
        bool readText(std::string& text, size_t count);
        bool seek(size_t position);
        size_t tell() const;
        size_t size() const;
        bool flush();
        bool isOpen() const;
        bool isEndOfFile() const;
    };
    std::unique_ptr<FileStream> openStream(const std::string& path, 
                                           const std::string& mode = "rb");
    
    // Backup and recovery
    struct BackupInfo {
        std::string name;
        std::string path;
        size_t size_bytes = 0;
        size_t file_count = 0;
        std::chrono::system_clock::time_point created;
        bool compressed = false;
        bool encrypted = false;
        std::string checksum = "";
    };
    
    bool createBackup(const std::string& source, 
                      const std::string& backup_name = "",
                      bool compress = true);
    bool restoreBackup(const std::string& backup_name, 
                       const std::string& destination = "");
    bool deleteBackup(const std::string& backup_name);
    std::vector<BackupInfo> listBackups() const;
    BackupInfo getBackupInfo(const std::string& backup_name) const;
    bool verifyBackup(const std::string& backup_name) const;
    
    // Compression
    bool compressFile(const std::string& path, const std::string& output_path = "");
    bool decompressFile(const std::string& path, const std::string& output_path = "");
    bool compressDirectory(const std::string& path, const std::string& output_path = "");
    bool decompressDirectory(const std::string& path, const std::string& output_path = "");
    
    // Encryption (if enabled)
    bool encryptFile(const std::string& path, const std::string& output_path = "");
    bool decryptFile(const std::string& path, const std::string& output_path = "");
    
    // Storage management
    StorageStats getStorageStats() const;
    bool cleanupTempFiles();
    bool cleanupOldFiles(int days_old = 30);
    bool cleanupBySize(size_t max_size_gb);
    bool checkDiskSpace(size_t required_gb) const;
    bool defragment();
    bool sync();
    
    // Monitoring and callbacks
    void setLowSpaceCallback(std::function<void(float)> callback);
    void setErrorCallback(std::function<void(const std::string&)> callback);
    void setTransactionCallback(std::function<void(const StorageTransaction&)> callback);
    void setProgressCallback(std::function<void(float, const std::string&)> callback);
    
    // Utility
    std::string getFullPath(const std::string& relative_path) const;
    std::string getTempPath() const;
    std::string getBackupPath() const;
    std::string getLogPath() const;
    std::string generateUniqueFilename(const std::string& prefix = "",
                                      const std::string& extension = "");
    std::string getFileExtension(const std::string& path) const;
    std::string getFileName(const std::string& path) const;
    std::string getParentDirectory(const std::string& path) const;
    size_t getDirectorySize(const std::string& path) const;
    bool makeExecutable(const std::string& path);
    bool makeWritable(const std::string& path);
    bool makeReadOnly(const std::string& path);
    
    // Testing
    bool testStorage();
    bool runBenchmark(size_t file_size_mb = 100, int iterations = 10);
    std::string getStorageInfo() const;
    
private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

} // namespace EdgeAI
