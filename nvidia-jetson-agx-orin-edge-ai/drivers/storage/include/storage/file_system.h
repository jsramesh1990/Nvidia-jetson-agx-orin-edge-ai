#pragma once
#include <string>
#include <vector>
#include <functional>
#include <chrono>

namespace EdgeAI {

class FileSystem {
public:
    // Static utility methods
    static bool exists(const std::string& path);
    static bool isDirectory(const std::string& path);
    static bool isFile(const std::string& path);
    static bool isSymlink(const std::string& path);
    static bool isReadable(const std::string& path);
    static bool isWritable(const std::string& path);
    static bool isExecutable(const std::string& path);
    
    static bool createDirectory(const std::string& path, bool recursive = true);
    static bool removeDirectory(const std::string& path, bool recursive = false);
    static bool removeFile(const std::string& path);
    static bool copy(const std::string& source, const std::string& destination);
    static bool move(const std::string& source, const std::string& destination);
    static bool rename(const std::string& old_path, const std::string& new_path);
    
    static std::vector<std::string> listFiles(const std::string& path,
                                              const std::string& pattern = "*",
                                              bool recursive = false);
    static std::vector<std::string> listDirectories(const std::string& path,
                                                    const std::string& pattern = "*",
                                                    bool recursive = false);
    static std::vector<std::string> listAll(const std::string& path,
                                            bool recursive = false);
    
    static size_t getFileSize(const std::string& path);
    static size_t getDirectorySize(const std::string& path);
    static size_t getFreeSpace(const std::string& path);
    static size_t getTotalSpace(const std::string& path);
    static size_t getUsedSpace(const std::string& path);
    
    static std::chrono::system_clock::time_point getModificationTime(const std::string& path);
    static std::chrono::system_clock::time_point getCreationTime(const std::string& path);
    static std::chrono::system_clock::time_point getAccessTime(const std::string& path);
    
    static std::string getAbsolutePath(const std::string& path);
    static std::string getBaseName(const std::string& path);
    static std::string getDirectoryName(const std::string& path);
    static std::string getExtension(const std::string& path);
    static std::string getFileName(const std::string& path);
    static std::string getParentDirectory(const std::string& path);
    static std::string normalizePath(const std::string& path);
    static std::string joinPath(const std::string& base, const std::string& relative);
    static std::string getRelativePath(const std::string& path, 
                                       const std::string& base_path);
    
    static bool chmod(const std::string& path, int permissions);
    static bool chown(const std::string& path, const std::string& owner);
    static bool chgrp(const std::string& path, const std::string& group);
    static bool touch(const std::string& path);
    static bool truncate(const std::string& path, size_t size);
    
    static std::string getMimeType(const std::string& path);
    static std::string calculateMD5(const std::string& path);
    static std::string calculateSHA1(const std::string& path);
    static std::string calculateSHA256(const std::string& path);
    
    // File watching
    struct FileEvent {
        enum Type {
            CREATED = 0,
            MODIFIED = 1,
            DELETED = 2,
            RENAMED = 3
        };
        Type type;
        std::string path;
        std::string old_path;  // For rename events
        std::chrono::system_clock::time_point timestamp;
    };
    
    static bool watchDirectory(const std::string& path,
                               std::function<void(const FileEvent&)> callback,
                               bool recursive = false);
    static bool stopWatching(const std::string& path);
    static bool isWatching(const std::string& path);
    static std::vector<std::string> getWatchedDirectories();
    
    // File locking
    static bool lockFile(const std::string& path, bool wait = true);
    static bool unlockFile(const std::string& path);
    static bool isFileLocked(const std::string& path);
    static bool tryLockFile(const std::string& path);
    
    // Temporary files
    static std::string createTempFile(const std::string& prefix = "",
                                      const std::string& suffix = "",
                                      const std::string& directory = "");
    static std::string createTempDirectory(const std::string& prefix = "",
                                           const std::string& directory = "");
    static bool deleteTempFiles(const std::string& directory = "",
                                int max_age_seconds = 3600);
    
    // Advanced operations
    static bool sync(const std::string& path);
    static bool flush(const std::string& path);
    static bool compress(const std::string& path, bool recursive = false);
    static bool decompress(const std::string& path, const std::string& destination = "");
    static bool encrypt(const std::string& path, const std::string& key);
    static bool decrypt(const std::string& path, const std::string& key);
    
    // Utility
    static void printFileInfo(const std::string& path);
    static bool verifyIntegrity(const std::string& path, const std::string& checksum);
    static std::vector<std::string> splitPath(const std::string& path);
    static bool isPathAbsolute(const std::string& path);
    static bool isPathRelative(const std::string& path);
    static bool isPathValid(const std::string& path);
    static std::string sanitizePath(const std::string& path);
    
private:
    FileSystem() = delete;  // Static class only
};

} // namespace EdgeAI
