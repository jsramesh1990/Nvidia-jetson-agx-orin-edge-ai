#include "storage/storage_driver.h"
#include "storage/file_system.h"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <thread>
#include <sys/statvfs.h>

namespace EdgeAI {

class StorageDriver::Impl {
public:
    Impl(const StorageConfig& config) : config_(config), initialized_(false) {
        // Initialize storage paths
        root_path_ = config_.root_path;
        temp_path_ = config_.temp_path;
        backup_path_ = config_.backup_path;
        log_path_ = config_.log_path;
    }
    
    ~Impl() {
        shutdown();
    }
    
    bool initialize() {
        if (initialized_) return true;
        
        // Create all required directories
        if (!FileSystem::createDirectory(root_path_, true)) {
            setError("Failed to create root directory: " + root_path_);
            return false;
        }
        
        if (!FileSystem::createDirectory(temp_path_, true)) {
            setError("Failed to create temp directory: " + temp_path_);
            return false;
        }
        
        if (!FileSystem::createDirectory(backup_path_, true)) {
            setError("Failed to create backup directory: " + backup_path_);
            return false;
        }
        
        if (!FileSystem::createDirectory(log_path_, true)) {
            setError("Failed to create log directory: " + log_path_);
            return false;
        }
        
        // Check available space
        auto stats = getStorageStats();
        if (stats.free_space_gb < 1.0) {
            setError("Insufficient disk space: " + std::to_string(stats.free_space_gb) + " GB");
            return false;
        }
        
        initialized_ = true;
        
        // Start cleanup thread if auto cleanup is enabled
        if (config_.auto_cleanup) {
            startCleanupThread();
        }
        
        return true;
    }
    
    void shutdown() {
        if (cleanup_thread_.joinable()) {
            running_ = false;
            cleanup_thread_.join();
        }
        initialized_ = false;
    }
    
    bool isReady() const {
        return initialized_;
    }
    
    bool createFile(const std::string& path, const std::vector<uint8_t>& data,
                    bool overwrite) {
        if (!initialized_) return false;
        
        std::string full_path = getFullPath(path);
        
        if (FileSystem::exists(full_path) && !overwrite) {
            return false;
        }
        
        // Check quota
        if (!checkQuota(full_path, data.size())) {
            return false;
        }
        
        std::ofstream file(full_path, std::ios::binary);
        if (!file.is_open()) return false;
        
        file.write(reinterpret_cast<const char*>(data.data()), data.size());
        file.close();
        
        return true;
    }
    
    bool createTextFile(const std::string& path, const std::string& content,
                        bool overwrite) {
        std::vector<uint8_t> data(content.begin(), content.end());
        return createFile(path, data, overwrite);
    }
    
    bool readFile(const std::string& path, std::vector<uint8_t>& data) {
        if (!initialized_) return false;
        
        std::string full_path = getFullPath(path);
        
        if (!FileSystem::exists(full_path) || FileSystem::isDirectory(full_path)) {
            return false;
        }
        
        std::ifstream file(full_path, std::ios::binary);
        if (!file.is_open()) return false;
        
        file.seekg(0, std::ios::end);
        size_t size = file.tellg();
        file.seekg(0, std::ios::beg);
        
        data.resize(size);
        file.read(reinterpret_cast<char*>(data.data()), size);
        file.close();
        
        return true;
    }
    
    bool readTextFile(const std::string& path, std::string& content) {
        std::vector<uint8_t> data;
        if (!readFile(path, data)) return false;
        content = std::string(data.begin(), data.end());
        return true;
    }
    
    bool deleteFile(const std::string& path) {
        if (!initialized_) return false;
        
        std::string full_path = getFullPath(path);
        
        if (!FileSystem::exists(full_path) || FileSystem::isDirectory(full_path)) {
            return false;
        }
        
        return FileSystem::removeFile(full_path);
    }
    
    bool copyFile(const std::string& source, const std::string& destination) {
        if (!initialized_) return false;
        
        std::string full_source = getFullPath(source);
        std::string full_dest = getFullPath(destination);
        
        if (!FileSystem::exists(full_source) || FileSystem::isDirectory(full_source)) {
            return false;
        }
        
        return FileSystem::copy(full_source, full_dest);
    }
    
    bool moveFile(const std::string& source, const std::string& destination) {
        if (!initialized_) return false;
        
        std::string full_source = getFullPath(source);
        std::string full_dest = getFullPath(destination);
        
        if (!FileSystem::exists(full_source) || FileSystem::isDirectory(full_source)) {
            return false;
        }
        
        return FileSystem::move(full_source, full_dest);
    }
    
    bool renameFile(const std::string& old_path, const std::string& new_path) {
        return moveFile(old_path, new_path);
    }
    
    bool fileExists(const std::string& path) const {
        if (!initialized_) return false;
        std::string full_path = getFullPath(path);
        return FileSystem::exists(full_path) && FileSystem::isFile(full_path);
    }
    
    bool isFileLocked(const std::string& path) const {
        std::string full_path = getFullPath(path);
        return FileSystem::isFileLocked(full_path);
    }
    
    bool lockFile(const std::string& path) {
        std::string full_path = getFullPath(path);
        return FileSystem::lockFile(full_path);
    }
    
    bool unlockFile(const std::string& path) {
        std::string full_path = getFullPath(path);
        return FileSystem::unlockFile(full_path);
    }
    
    bool createDirectory(const std::string& path, bool recursive) {
        if (!initialized_) return false;
        std::string full_path = getFullPath(path);
        return FileSystem::createDirectory(full_path, recursive);
    }
    
    bool deleteDirectory(const std::string& path, bool recursive) {
        if (!initialized_) return false;
        std::string full_path = getFullPath(path);
        return FileSystem::removeDirectory(full_path, recursive);
    }
    
    bool directoryExists(const std::string& path) const {
        if (!initialized_) return false;
        std::string full_path = getFullPath(path);
        return FileSystem::exists(full_path) && FileSystem::isDirectory(full_path);
    }
    
    bool isDirectoryEmpty(const std::string& path) const {
        std::string full_path = getFullPath(path);
        if (!FileSystem::exists(full_path) || !FileSystem::isDirectory(full_path)) {
            return false;
        }
        return FileSystem::listAll(full_path).empty();
    }
    
    std::vector<FileInfo> listDirectory(const std::string& path, 
                                        bool recursive,
                                        bool include_hidden) const {
        std::vector<FileInfo> files;
        if (!initialized_) return files;
        
        std::string full_path = getFullPath(path);
        if (!FileSystem::exists(full_path) || !FileSystem::isDirectory(full_path)) {
            return files;
        }
        
        auto entries = FileSystem::listAll(full_path, recursive);
        
        for (const auto& entry : entries) {
            if (!include_hidden && entry[0] == '.') continue;
            
            std::string full_entry_path = full_path + "/" + entry;
            FileInfo info;
            info.path = entry;
            info.name = FileSystem::getBaseName(entry);
            info.extension = FileSystem::getExtension(entry);
            info.size_bytes = FileSystem::getFileSize(full_entry_path);
            info.is_directory = FileSystem::isDirectory(full_entry_path);
            info.is_readonly = !FileSystem::isWritable(full_entry_path);
            info.is_hidden = entry[0] == '.';
            info.is_symlink = FileSystem::isSymlink(full_entry_path);
            info.created = FileSystem::getCreationTime(full_entry_path);
            info.modified = FileSystem::getModificationTime(full_entry_path);
            info.permissions = 0644; // Simplified
            info.mime_type = FileSystem::getMimeType(full_entry_path);
            
            files.push_back(info);
        }
        
        return files;
    }
    
    std::vector<FileInfo> findFiles(const std::string& path,
                                    const std::string& pattern,
                                    bool recursive) const {
        std::vector<FileInfo> results;
        auto files = listDirectory(path, recursive);
        
        for (const auto& file : files) {
            if (file.is_directory) continue;
            if (pattern == "*" || file.name.find(pattern) != std::string::npos) {
                results.push_back(file);
            }
        }
        
        return results;
    }
    
    std::unique_ptr<StorageDriver::FileStream> openStream(const std::string& path,
                                                          const std::string& mode) {
        // Implementation for file stream
        return std::unique_ptr<StorageDriver::FileStream>();
    }
    
    bool createBackup(const std::string& source, const std::string& backup_name,
                      bool compress) {
        if (!initialized_) return false;
        
        std::string full_source = getFullPath(source);
        if (!FileSystem::exists(full_source)) return false;
        
        std::string timestamp = getTimestamp();
        std::string name = backup_name.empty() ? timestamp : backup_name;
        std::string backup_dir = backup_path_ + "/" + name;
        
        if (!FileSystem::createDirectory(backup_dir)) {
            return false;
        }
        
        // Copy source to backup
        std::string cmd = "cp -r " + full_source + "/* " + backup_dir + "/";
        if (system(cmd.c_str()) != 0) {
            return false;
        }
        
        // Compress if enabled
        if (compress && config_.compression_enabled) {
            std::string compress_cmd = "tar -czf " + backup_dir + ".tar.gz -C " + backup_dir + " .";
            system(compress_cmd.c_str());
            FileSystem::removeDirectory(backup_dir, true);
        }
        
        return true;
    }
    
    bool restoreBackup(const std::string& backup_name, const std::string& destination) {
        if (!initialized_) return false;
        
        std::string backup_path = backup_path_ + "/" + backup_name;
        std::string dest_path = destination.empty() ? root_path_ : getFullPath(destination);
        
        // Check if backup is compressed
        std::string backup_tar = backup_path + ".tar.gz";
        if (FileSystem::exists(backup_tar)) {
            std::string cmd = "tar -xzf " + backup_tar + " -C " + dest_path;
            return system(cmd.c_str()) == 0;
        }
        
        if (!FileSystem::exists(backup_path) || !FileSystem::isDirectory(backup_path)) {
            return false;
        }
        
        // Copy backup to destination
        std::string cmd = "cp -r " + backup_path + "/* " + dest_path + "/";
        return system(cmd.c_str()) == 0;
    }
    
    bool deleteBackup(const std::string& backup_name) {
        std::string backup_path = backup_path_ + "/" + backup_name;
        
        if (FileSystem::exists(backup_path + ".tar.gz")) {
            return FileSystem::removeFile(backup_path + ".tar.gz");
        }
        
        if (FileSystem::exists(backup_path) && FileSystem::isDirectory(backup_path)) {
            return FileSystem::removeDirectory(backup_path, true);
        }
        
        return false;
    }
    
    std::vector<BackupInfo> listBackups() const {
        std::vector<BackupInfo> backups;
        
        if (!FileSystem::exists(backup_path_)) return backups;
        
        auto files = FileSystem::listAll(backup_path_);
        
        for (const auto& file : files) {
            std::string full_path = backup_path_ + "/" + file;
            
            BackupInfo info;
            info.name = file;
            info.path = full_path;
            
            if (FileSystem::isDirectory(full_path)) {
                info.size_bytes = FileSystem::getDirectorySize(full_path);
                info.file_count = FileSystem::listAll(full_path, true).size();
                info.compressed = false;
            } else if (FileSystem::isFile(full_path) && 
                       FileSystem::getExtension(full_path) == ".tar.gz") {
                info.size_bytes = FileSystem::getFileSize(full_path);
                info.file_count = 0;
                info.compressed = true;
            }
            
            info.created = FileSystem::getCreationTime(full_path);
            info.encrypted = false;
            
            backups.push_back(info);
        }
        
        return backups;
    }
    
    BackupInfo getBackupInfo(const std::string& backup_name) const {
        auto backups = listBackups();
        for (const auto& backup : backups) {
            if (backup.name == backup_name) {
                return backup;
            }
        }
        return BackupInfo();
    }
    
    bool verifyBackup(const std::string& backup_name) const {
        auto info = getBackupInfo(backup_name);
        return info.size_bytes > 0;
    }
    
    bool compressFile(const std::string& path, const std::string& output_path) {
        if (!initialized_) return false;
        
        std::string full_path = getFullPath(path);
        std::string out_path = output_path.empty() ? full_path + ".gz" : getFullPath(output_path);
        
        if (!FileSystem::exists(full_path) || FileSystem::isDirectory(full_path)) {
            return false;
        }
        
        std::string cmd = "gzip -c " + full_path + " > " + out_path;
        return system(cmd.c_str()) == 0;
    }
    
    bool decompressFile(const std::string& path, const std::string& output_path) {
        if (!initialized_) return false;
        
        std::string full_path = getFullPath(path);
        std::string out_path = output_path.empty() ? 
                              full_path.substr(0, full_path.find_last_of('.')) : 
                              getFullPath(output_path);
        
        if (!FileSystem::exists(full_path)) return false;
        
        std::string cmd = "gunzip -c " + full_path + " > " + out_path;
        return system(cmd.c_str()) == 0;
    }
    
    bool compressDirectory(const std::string& path, const std::string& output_path) {
        if (!initialized_) return false;
        
        std::string full_path = getFullPath(path);
        std::string out_path = output_path.empty() ? full_path + ".tar.gz" : getFullPath(output_path);
        
        if (!FileSystem::exists(full_path) || !FileSystem::isDirectory(full_path)) {
            return false;
        }
        
        std::string cmd = "tar -czf " + out_path + " -C " + full_path + " .";
        return system(cmd.c_str()) == 0;
    }
    
    bool decompressDirectory(const std::string& path, const std::string& output_path) {
        if (!initialized_) return false;
        
        std::string full_path = getFullPath(path);
        std::string out_path = output_path.empty() ? 
                              full_path.substr(0, full_path.find_last_of('.')) :
                              getFullPath(output_path);
        
        if (!FileSystem::exists(full_path)) return false;
        
        std::string cmd = "tar -xzf " + full_path + " -C " + out_path;
        return system(cmd.c_str()) == 0;
    }
    
    bool encryptFile(const std::string& path, const std::string& output_path) {
        if (!config_.encryption_enabled) return false;
        // Placeholder - would require OpenSSL
        return false;
    }
    
    bool decryptFile(const std::string& path, const std::string& output_path) {
        if (!config_.encryption_enabled) return false;
        // Placeholder - would require OpenSSL
        return false;
    }
    
    StorageStats getStorageStats() const {
        StorageStats stats;
        struct statvfs vfs;
        
        if (statvfs(root_path_.c_str(), &vfs) != 0) {
            return stats;
        }
        
        stats.total_space_gb = (static_cast<double>(vfs.f_blocks) * vfs.f_frsize) / (1024.0 * 1024.0 * 1024.0);
        stats.free_space_gb = (static_cast<double>(vfs.f_bfree) * vfs.f_frsize) / (1024.0 * 1024.0 * 1024.0);
        stats.used_space_gb = stats.total_space_gb - stats.free_space_gb;
        stats.disk_usage_percentage = (stats.used_space_gb / stats.total_space_gb) * 100.0;
        stats.file_system_type = 0;
        stats.block_size = vfs.f_frsize;
        stats.max_file_size = vfs.f_frsize * vfs.f_blocks;
        
        // Count files
        auto files = FileSystem::listAll(root_path_, true);
        stats.file_count = 0;
        stats.directory_count = 0;
        for (const auto& file : files) {
            std::string full_path = root_path_ + "/" + file;
            if (FileSystem::isDirectory(full_path)) {
                stats.directory_count++;
            } else {
                stats.file_count++;
            }
        }
        
        return stats;
    }
    
    bool cleanupTempFiles() {
        return FileSystem::deleteTempFiles(temp_path_);
    }
    
    bool cleanupOldFiles(int days_old) {
        auto files = FileSystem::listAll(root_path_, true);
        auto now = std::chrono::system_clock::now();
        auto cutoff = now - std::chrono::hours(days_old * 24);
        
        bool success = true;
        for (const auto& file : files) {
            std::string full_path = root_path_ + "/" + file;
            auto mtime = FileSystem::getModificationTime(full_path);
            if (mtime < cutoff && !FileSystem::isDirectory(full_path)) {
                if (!FileSystem::removeFile(full_path)) {
                    success = false;
                }
            }
        }
        
        return success;
    }
    
    bool cleanupBySize(size_t max_size_gb) {
        auto stats = getStorageStats();
        if (stats.used_space_gb <= max_size_gb) return true;
        
        // Delete oldest files first
        auto files = FileSystem::listAll(root_path_, true);
        std::vector<std::pair<std::chrono::system_clock::time_point, std::string>> file_list;
        
        for (const auto& file : files) {
            std::string full_path = root_path_ + "/" + file;
            if (!FileSystem::isDirectory(full_path)) {
                file_list.push_back({FileSystem::getModificationTime(full_path), full_path});
            }
        }
        
        std::sort(file_list.begin(), file_list.end());
        
        for (const auto& [mtime, file] : file_list) {
            if (stats.used_space_gb <= max_size_gb) break;
            if (FileSystem::removeFile(file)) {
                stats = getStorageStats();
            }
        }
        
        return true;
    }
    
    bool checkDiskSpace(size_t required_gb) const {
        auto stats = getStorageStats();
        return stats.free_space_gb >= required_gb;
    }
    
    bool defragment() {
        // Placeholder - would require filesystem-specific tools
        return true;
    }
    
    bool sync() {
        return syncfs(open(root_path_.c_str(), O_RDONLY)) == 0;
    }
    
    void setLowSpaceCallback(std::function<void(float)> callback) {
        low_space_callback_ = callback;
    }
    
    void setErrorCallback(std::function<void(const std::string&)> callback) {
        error_callback_ = callback;
    }
    
    void setTransactionCallback(std::function<void(const StorageTransaction&)> callback) {
        transaction_callback_ = callback;
    }
    
    void setProgressCallback(std::function<void(float, const std::string&)> callback) {
        progress_callback_ = callback;
    }
    
    std::string getFullPath(const std::string& relative_path) const {
        if (relative_path.empty()) return root_path_;
        if (relative_path[0] == '/') return relative_path;
        return root_path_ + "/" + relative_path;
    }
    
    std::string getTempPath() const {
        return temp_path_;
    }
    
    std::string getBackupPath() const {
        return backup_path_;
    }
    
    std::string getLogPath() const {
        return log_path_;
    }
    
    std::string generateUniqueFilename(const std::string& prefix,
                                      const std::string& extension) {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << prefix << "_" << time_t << std::setw(4) << std::setfill('0') 
           << std::rand() % 10000 << extension;
        return ss.str();
    }
    
    std::string getFileExtension(const std::string& path) const {
        return FileSystem::getExtension(path);
    }
    
    std::string getFileName(const std::string& path) const {
        return FileSystem::getFileName(path);
    }
    
    std::string getParentDirectory(const std::string& path) const {
        return FileSystem::getDirectoryName(path);
    }
    
    size_t getDirectorySize(const std::string& path) const {
        std::string full_path = getFullPath(path);
        return FileSystem::getDirectorySize(full_path);
    }
    
    bool makeExecutable(const std::string& path) {
        std::string full_path = getFullPath(path);
        return FileSystem::chmod(full_path, 0755);
    }
    
    bool makeWritable(const std::string& path) {
        std::string full_path = getFullPath(path);
        return FileSystem::chmod(full_path, 0644);
    }
    
    bool makeReadOnly(const std::string& path) {
        std::string full_path = getFullPath(path);
        return FileSystem::chmod(full_path, 0444);
    }
    
    bool testStorage() {
        // Create test file
        std::vector<uint8_t> test_data(1024, 0xAA);
        std::string test_path = temp_path_ + "/test.dat";
        
        if (!createFile(test_path, test_data, true)) {
            return false;
        }
        
        std::vector<uint8_t> read_data;
        if (!readFile(test_path, read_data)) {
            return false;
        }
        
        if (read_data.size() != test_data.size()) {
            return false;
        }
        
        FileSystem::removeFile(test_path);
        return true;
    }
    
    bool runBenchmark(size_t file_size_mb, int iterations) {
        std::vector<uint8_t> data(file_size_mb * 1024 * 1024, 0x00);
        std::string test_path = temp_path_ + "/benchmark.dat";
        
        auto start = std::chrono::steady_clock::now();
        
        for (int i = 0; i < iterations; i++) {
            if (!createFile(test_path, data, true)) {
                return false;
            }
            
            std::vector<uint8_t> read_data;
            if (!readFile(test_path, read_data)) {
                return false;
            }
            
            FileSystem::removeFile(test_path);
        }
        
        auto end = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        
        if (progress_callback_) {
            progress_callback_(100.0, "Benchmark completed in " + std::to_string(elapsed) + "ms");
        }
        
        return true;
    }
    
    std::string getStorageInfo() const {
        auto stats = getStorageStats();
        std::stringstream ss;
        ss << "Storage Info:\n";
        ss << "  Root Path: " << root_path_ << "\n";
        ss << "  Total Space: " << stats.total_space_gb << " GB\n";
        ss << "  Used Space: " << stats.used_space_gb << " GB\n";
        ss << "  Free Space: " << stats.free_space_gb << " GB\n";
        ss << "  Usage: " << std::fixed << std::setprecision(1) 
           << stats.disk_usage_percentage << "%\n";
        ss << "  Files: " << stats.file_count << "\n";
        ss << "  Directories: " << stats.directory_count << "\n";
        ss << "  Compression: " << (config_.compression_enabled ? "Enabled" : "Disabled") << "\n";
        ss << "  Encryption: " << (config_.encryption_enabled ? "Enabled" : "Disabled") << "\n";
        return ss.str();
    }
    
private:
    StorageConfig config_;
    std::string root_path_;
    std::string temp_path_;
    std::string backup_path_;
    std::string log_path_;
    bool initialized_;
    std::atomic<bool> running_{false};
    std::thread cleanup_thread_;
    std::function<void(float)> low_space_callback_;
    std::function<void(const std::string&)> error_callback_;
    std::function<void(const StorageTransaction&)> transaction_callback_;
    std::function<void(float, const std::string&)> progress_callback_;
    
    void setError(const std::string& error) {
        if (error_callback_) {
            error_callback_(error);
        }
        std::cerr << "Storage Error: " << error << std::endl;
    }
    
    void startCleanupThread() {
        running_ = true;
        cleanup_thread_ = std::thread([this]() {
            while (running_) {
                std::this_thread::sleep_for(std::chrono::hours(24));
                
                // Cleanup old files
                cleanupOldFiles(config_.cleanup_days);
                
                // Cleanup temp files
                cleanupTempFiles();
                
                // Check space
                auto stats = getStorageStats();
                if (stats.disk_usage_percentage > 90.0 && low_space_callback_) {
                    low_space_callback_(stats.disk_usage_percentage);
                }
            }
        });
    }
    
    std::string getTimestamp() const {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time_t), "%Y%m%d_%H%M%S");
        return ss.str();
    }
    
    bool checkQuota(const std::string& path, size_t size) const {
        auto stats = getStorageStats();
        return stats.free_space_gb * 1024 * 1024 * 1024 >= size;
    }
};

// StorageDriver implementation
StorageDriver::StorageDriver(const StorageConfig& config) 
    : pImpl(std::make_unique<Impl>(config)) {}

StorageDriver::~StorageDriver() = default;

bool StorageDriver::initialize() { return pImpl->initialize(); }
bool StorageDriver::isReady() const { return pImpl->isReady(); }
void StorageDriver::shutdown() { pImpl->shutdown(); }

bool StorageDriver::createFile(const std::string& path, const std::vector<uint8_t>& data,
                               bool overwrite) {
    return pImpl->createFile(path, data, overwrite);
}
bool StorageDriver::createTextFile(const std::string& path, const std::string& content,
                                   bool overwrite) {
    return pImpl->createTextFile(path, content, overwrite);
}
bool StorageDriver::readFile(const std::string& path, std::vector<uint8_t>& data) {
    return pImpl->readFile(path, data);
}
bool StorageDriver::readTextFile(const std::string& path, std::string& content) {
    return pImpl->readTextFile(path, content);
}
bool StorageDriver::deleteFile(const std::string& path) {
    return pImpl->deleteFile(path);
}
bool StorageDriver::copyFile(const std::string& source, const std::string& destination) {
    return pImpl->copyFile(source, destination);
}
bool StorageDriver::moveFile(const std::string& source, const std::string& destination) {
    return pImpl->moveFile(source, destination);
}
bool StorageDriver::renameFile(const std::string& old_path, const std::string& new_path) {
    return pImpl->renameFile(old_path, new_path);
}
bool StorageDriver::fileExists(const std::string& path) const {
    return pImpl->fileExists(path);
}
bool StorageDriver::isFileLocked(const std::string& path) const {
    return pImpl->isFileLocked(path);
}
bool StorageDriver::lockFile(const std::string& path) {
    return pImpl->lockFile(path);
}
bool StorageDriver::unlockFile(const std::string& path) {
    return pImpl->unlockFile(path);
}

bool StorageDriver::createDirectory(const std::string& path, bool recursive) {
    return pImpl->createDirectory(path, recursive);
}
bool StorageDriver::deleteDirectory(const std::string& path, bool recursive) {
    return pImpl->deleteDirectory(path, recursive);
}
bool StorageDriver::directoryExists(const std::string& path) const {
    return pImpl->directoryExists(path);
}
bool StorageDriver::isDirectoryEmpty(const std::string& path) const {
    return pImpl->isDirectoryEmpty(path);
}
std::vector<FileInfo> StorageDriver::listDirectory(const std::string& path, 
                                                    bool recursive,
                                                    bool include_hidden) const {
    return pImpl->listDirectory(path, recursive, include_hidden);
}
std::vector<FileInfo> StorageDriver::findFiles(const std::string& path,
                                               const std::string& pattern,
                                               bool recursive) const {
    return pImpl->findFiles(path, pattern, recursive);
}

std::unique_ptr<StorageDriver::FileStream> StorageDriver::openStream(
    const std::string& path, const std::string& mode) {
    return pImpl->openStream(path, mode);
}

bool StorageDriver::createBackup(const std::string& source, const std::string& backup_name,
                                 bool compress) {
    return pImpl->createBackup(source, backup_name, compress);
}
bool StorageDriver::restoreBackup(const std::string& backup_name, 
                                  const std::string& destination) {
    return pImpl->restoreBackup(backup_name, destination);
}
bool StorageDriver::deleteBackup(const std::string& backup_name) {
    return pImpl->deleteBackup(backup_name);
}
std::vector<StorageDriver::BackupInfo> StorageDriver::listBackups() const {
    return pImpl->listBackups();
}
StorageDriver::BackupInfo StorageDriver::getBackupInfo(const std::string& backup_name) const {
    return pImpl->getBackupInfo(backup_name);
}
bool StorageDriver::verifyBackup(const std::string& backup_name) const {
    return pImpl->verifyBackup(backup_name);
}

bool StorageDriver::compressFile(const std::string& path, const std::string& output_path) {
    return pImpl->compressFile(path, output_path);
}
bool StorageDriver::decompressFile(const std::string& path, const std::string& output_path) {
    return pImpl->decompressFile(path, output_path);
}
bool StorageDriver::compressDirectory(const std::string& path, const std::string& output_path) {
    return pImpl->compressDirectory(path, output_path);
}
bool StorageDriver::decompressDirectory(const std::string& path, const std::string& output_path) {
    return pImpl->decompressDirectory(path, output_path);
}

bool StorageDriver::encryptFile(const std::string& path, const std::string& output_path) {
    return pImpl->encryptFile(path, output_path);
}
bool StorageDriver::decryptFile(const std::string& path, const std::string& output_path) {
    return pImpl->decryptFile(path, output_path);
}

StorageStats StorageDriver::getStorageStats() const {
    return pImpl->getStorageStats();
}
bool StorageDriver::cleanupTempFiles() {
    return pImpl->cleanupTempFiles();
}
bool StorageDriver::cleanupOldFiles(int days_old) {
    return pImpl->cleanupOldFiles(days_old);
}
bool StorageDriver::cleanupBySize(size_t max_size_gb) {
    return pImpl->cleanupBySize(max_size_gb);
}
bool StorageDriver::checkDiskSpace(size_t required_gb) const {
    return pImpl->checkDiskSpace(required_gb);
}
bool StorageDriver::defragment() {
    return pImpl->defragment();
}
bool StorageDriver::sync() {
    return pImpl->sync();
}

void StorageDriver::setLowSpaceCallback(std::function<void(float)> callback) {
    pImpl->setLowSpaceCallback(callback);
}
void StorageDriver::setErrorCallback(std::function<void(const std::string&)> callback) {
    pImpl->setErrorCallback(callback);
}
void StorageDriver::setTransactionCallback(std::function<void(const StorageTransaction&)> callback) {
    pImpl->setTransactionCallback(callback);
}
void StorageDriver::setProgressCallback(std::function<void(float, const std::string&)> callback) {
    pImpl->setProgressCallback(callback);
}

std::string StorageDriver::getFullPath(const std::string& relative_path) const {
    return pImpl->getFullPath(relative_path);
}
std::string StorageDriver::getTempPath() const {
    return pImpl->getTempPath();
}
std::string StorageDriver::getBackupPath() const {
    return pImpl->getBackupPath();
}
std::string StorageDriver::getLogPath() const {
    return pImpl->getLogPath();
}
std::string StorageDriver::generateUniqueFilename(const std::string& prefix,
                                                  const std::string& extension) {
    return pImpl->generateUniqueFilename(prefix, extension);
}
std::string StorageDriver::getFileExtension(const std::string& path) const {
    return pImpl->getFileExtension(path);
}
std::string StorageDriver::getFileName(const std::string& path) const {
    return pImpl->getFileName(path);
}
std::string StorageDriver::getParentDirectory(const std::string& path) const {
    return pImpl->getParentDirectory(path);
}
size_t StorageDriver::getDirectorySize(const std::string& path) const {
    return pImpl->getDirectorySize(path);
}
bool StorageDriver::makeExecutable(const std::string& path) {
    return pImpl->makeExecutable(path);
}
bool StorageDriver::makeWritable(const std::string& path) {
    return pImpl->makeWritable(path);
}
bool StorageDriver::makeReadOnly(const std::string& path) {
    return pImpl->makeReadOnly(path);
}

bool StorageDriver::testStorage() {
    return pImpl->testStorage();
}
bool StorageDriver::runBenchmark(size_t file_size_mb, int iterations) {
    return pImpl->runBenchmark(file_size_mb, iterations);
}
std::string StorageDriver::getStorageInfo() const {
    return pImpl->getStorageInfo();
}

} // namespace EdgeAI
