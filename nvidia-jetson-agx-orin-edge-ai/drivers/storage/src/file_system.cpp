#include "storage/file_system.h"
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <dirent.h>
#include <unistd.h>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <openssl/md5.h>
#include <openssl/sha.h>

namespace EdgeAI {

// Static utility methods
bool FileSystem::exists(const std::string& path) {
    struct stat st;
    return stat(path.c_str(), &st) == 0;
}

bool FileSystem::isDirectory(const std::string& path) {
    struct stat st;
    if (stat(path.c_str(), &st) != 0) return false;
    return S_ISDIR(st.st_mode);
}

bool FileSystem::isFile(const std::string& path) {
    struct stat st;
    if (stat(path.c_str(), &st) != 0) return false;
    return S_ISREG(st.st_mode);
}

bool FileSystem::isSymlink(const std::string& path) {
    struct stat st;
    if (lstat(path.c_str(), &st) != 0) return false;
    return S_ISLNK(st.st_mode);
}

bool FileSystem::isReadable(const std::string& path) {
    return access(path.c_str(), R_OK) == 0;
}

bool FileSystem::isWritable(const std::string& path) {
    return access(path.c_str(), W_OK) == 0;
}

bool FileSystem::isExecutable(const std::string& path) {
    return access(path.c_str(), X_OK) == 0;
}

bool FileSystem::createDirectory(const std::string& path, bool recursive) {
    if (exists(path)) return isDirectory(path);
    
    if (recursive) {
        std::string parent = getDirectoryName(path);
        if (!parent.empty() && !exists(parent)) {
            if (!createDirectory(parent, true)) return false;
        }
    }
    
    return mkdir(path.c_str(), 0755) == 0;
}

bool FileSystem::removeDirectory(const std::string& path, bool recursive) {
    if (!exists(path) || !isDirectory(path)) return false;
    
    if (recursive) {
        // Remove all contents first
        auto files = listAll(path);
        for (const auto& file : files) {
            std::string full_path = path + "/" + file;
            if (isDirectory(full_path)) {
                if (!removeDirectory(full_path, true)) return false;
            } else {
                if (!removeFile(full_path)) return false;
            }
        }
    }
    
    return rmdir(path.c_str()) == 0;
}

bool FileSystem::removeFile(const std::string& path) {
    if (!exists(path) || isDirectory(path)) return false;
    return unlink(path.c_str()) == 0;
}

bool FileSystem::copy(const std::string& source, const std::string& destination) {
    if (!exists(source)) return false;
    
    std::ifstream src(source, std::ios::binary);
    std::ofstream dst(destination, std::ios::binary);
    
    if (!src.is_open() || !dst.is_open()) return false;
    
    dst << src.rdbuf();
    return dst.good();
}

bool FileSystem::move(const std::string& source, const std::string& destination) {
    if (!exists(source)) return false;
    return rename(source.c_str(), destination.c_str()) == 0;
}

bool FileSystem::rename(const std::string& old_path, const std::string& new_path) {
    return ::rename(old_path.c_str(), new_path.c_str()) == 0;
}

std::vector<std::string> FileSystem::listFiles(const std::string& path,
                                               const std::string& pattern,
                                               bool recursive) {
    std::vector<std::string> files;
    auto all = listAll(path, recursive);
    
    for (const auto& name : all) {
        std::string full_path = path + "/" + name;
        if (isFile(full_path) && (pattern == "*" || name.find(pattern) != std::string::npos)) {
            files.push_back(name);
        }
    }
    
    return files;
}

std::vector<std::string> FileSystem::listDirectories(const std::string& path,
                                                     const std::string& pattern,
                                                     bool recursive) {
    std::vector<std::string> dirs;
    auto all = listAll(path, recursive);
    
    for (const auto& name : all) {
        std::string full_path = path + "/" + name;
        if (isDirectory(full_path) && (pattern == "*" || name.find(pattern) != std::string::npos)) {
            dirs.push_back(name);
        }
    }
    
    return dirs;
}

std::vector<std::string> FileSystem::listAll(const std::string& path, bool recursive) {
    std::vector<std::string> entries;
    DIR* dir = opendir(path.c_str());
    
    if (!dir) return entries;
    
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        std::string name = entry->d_name;
        if (name == "." || name == "..") continue;
        
        entries.push_back(name);
        
        if (recursive) {
            std::string full_path = path + "/" + name;
            if (isDirectory(full_path)) {
                auto sub_entries = listAll(full_path, true);
                for (const auto& sub : sub_entries) {
                    entries.push_back(name + "/" + sub);
                }
            }
        }
    }
    
    closedir(dir);
    return entries;
}

size_t FileSystem::getFileSize(const std::string& path) {
    struct stat st;
    if (stat(path.c_str(), &st) != 0) return 0;
    return st.st_size;
}

size_t FileSystem::getDirectorySize(const std::string& path) {
    if (!exists(path) || !isDirectory(path)) return 0;
    
    size_t total = 0;
    auto files = listAll(path, true);
    
    for (const auto& file : files) {
        std::string full_path = path + "/" + file;
        if (isFile(full_path)) {
            total += getFileSize(full_path);
        }
    }
    
    return total;
}

size_t FileSystem::getFreeSpace(const std::string& path) {
    struct statvfs vfs;
    if (statvfs(path.c_str(), &vfs) != 0) return 0;
    return vfs.f_bfree * vfs.f_frsize;
}

size_t FileSystem::getTotalSpace(const std::string& path) {
    struct statvfs vfs;
    if (statvfs(path.c_str(), &vfs) != 0) return 0;
    return vfs.f_blocks * vfs.f_frsize;
}

size_t FileSystem::getUsedSpace(const std::string& path) {
    return getTotalSpace(path) - getFreeSpace(path);
}

std::chrono::system_clock::time_point FileSystem::getModificationTime(const std::string& path) {
    struct stat st;
    if (stat(path.c_str(), &st) != 0) {
        return std::chrono::system_clock::time_point();
    }
    return std::chrono::system_clock::from_time_t(st.st_mtime);
}

std::chrono::system_clock::time_point FileSystem::getCreationTime(const std::string& path) {
    struct stat st;
    if (stat(path.c_str(), &st) != 0) {
        return std::chrono::system_clock::time_point();
    }
    return std::chrono::system_clock::from_time_t(st.st_ctime);
}

std::chrono::system_clock::time_point FileSystem::getAccessTime(const std::string& path) {
    struct stat st;
    if (stat(path.c_str(), &st) != 0) {
        return std::chrono::system_clock::time_point();
    }
    return std::chrono::system_clock::from_time_t(st.st_atime);
}

std::string FileSystem::getAbsolutePath(const std::string& path) {
    char* abs_path = realpath(path.c_str(), nullptr);
    if (!abs_path) return path;
    std::string result(abs_path);
    free(abs_path);
    return result;
}

std::string FileSystem::getBaseName(const std::string& path) {
    size_t pos = path.find_last_of("/");
    if (pos == std::string::npos) return path;
    return path.substr(pos + 1);
}

std::string FileSystem::getDirectoryName(const std::string& path) {
    size_t pos = path.find_last_of("/");
    if (pos == std::string::npos) return "";
    if (pos == 0) return "/";
    return path.substr(0, pos);
}

std::string FileSystem::getExtension(const std::string& path) {
    size_t pos = path.find_last_of(".");
    if (pos == std::string::npos || pos == 0) return "";
    return path.substr(pos);
}

std::string FileSystem::getFileName(const std::string& path) {
    std::string basename = getBaseName(path);
    size_t pos = basename.find_last_of(".");
    if (pos == std::string::npos || pos == 0) return basename;
    return basename.substr(0, pos);
}

std::string FileSystem::getParentDirectory(const std::string& path) {
    return getDirectoryName(path);
}

std::string FileSystem::normalizePath(const std::string& path) {
    std::string result = path;
    
    // Remove trailing slashes
    while (!result.empty() && result.back() == '/') {
        result.pop_back();
    }
    
    // Remove duplicate slashes
    size_t pos;
    while ((pos = result.find("//")) != std::string::npos) {
        result.replace(pos, 2, "/");
    }
    
    // Resolve .. and . (simplified)
    std::vector<std::string> parts;
    std::stringstream ss(result);
    std::string part;
    
    while (std::getline(ss, part, '/')) {
        if (part == "." || part.empty()) continue;
        if (part == "..") {
            if (!parts.empty()) parts.pop_back();
        } else {
            parts.push_back(part);
        }
    }
    
    std::string normalized;
    for (size_t i = 0; i < parts.size(); i++) {
        normalized += "/" + parts[i];
    }
    
    return normalized.empty() ? "/" : normalized;
}

std::string FileSystem::joinPath(const std::string& base, const std::string& relative) {
    if (base.empty()) return relative;
    if (relative.empty()) return base;
    
    if (relative[0] == '/') return relative;
    
    std::string result = base;
    if (result.back() != '/') {
        result += "/";
    }
    result += relative;
    
    return normalizePath(result);
}

std::string FileSystem::getRelativePath(const std::string& path, 
                                        const std::string& base_path) {
    std::string abs_path = getAbsolutePath(path);
    std::string abs_base = getAbsolutePath(base_path);
    
    if (abs_path.find(abs_base) != 0) {
        return abs_path;
    }
    
    std::string relative = abs_path.substr(abs_base.length());
    if (!relative.empty() && relative[0] == '/') {
        relative = relative.substr(1);
    }
    
    return relative;
}

bool FileSystem::chmod(const std::string& path, int permissions) {
    return ::chmod(path.c_str(), permissions) == 0;
}

bool FileSystem::chown(const std::string& path, const std::string& owner) {
    // This would require parsing owner string
    // Simplified implementation
    return true;
}

bool FileSystem::chgrp(const std::string& path, const std::string& group) {
    return true;
}

bool FileSystem::touch(const std::string& path) {
    if (exists(path)) {
        // Update modification time
        return utimensat(AT_FDCWD, path.c_str(), nullptr, 0) == 0;
    } else {
        // Create empty file
        std::ofstream file(path);
        return file.is_open();
    }
}

bool FileSystem::truncate(const std::string& path, size_t size) {
    return ::truncate(path.c_str(), size) == 0;
}

std::string FileSystem::getMimeType(const std::string& path) {
    // Simple extension-based MIME detection
    std::string ext = getExtension(path);
    
    if (ext == ".txt") return "text/plain";
    if (ext == ".html" || ext == ".htm") return "text/html";
    if (ext == ".css") return "text/css";
    if (ext == ".js") return "application/javascript";
    if (ext == ".json") return "application/json";
    if (ext == ".xml") return "application/xml";
    if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
    if (ext == ".png") return "image/png";
    if (ext == ".gif") return "image/gif";
    if (ext == ".bmp") return "image/bmp";
    if (ext == ".svg") return "image/svg+xml";
    if (ext == ".mp3") return "audio/mpeg";
    if (ext == ".wav") return "audio/wav";
    if (ext == ".mp4") return "video/mp4";
    if (ext == ".avi") return "video/x-msvideo";
    if (ext == ".mkv") return "video/x-matroska";
    if (ext == ".pdf") return "application/pdf";
    if (ext == ".zip") return "application/zip";
    if (ext == ".tar") return "application/x-tar";
    if (ext == ".gz") return "application/gzip";
    if (ext == ".exe") return "application/x-msdownload";
    if (ext == ".sh") return "application/x-sh";
    if (ext == ".py") return "text/x-python";
    if (ext == ".cpp" || ext == ".cxx") return "text/x-c++";
    if (ext == ".c") return "text/x-c";
    if (ext == ".h") return "text/x-c";
    if (ext == ".hpp") return "text/x-c++";
    
    return "application/octet-stream";
}

std::string FileSystem::calculateMD5(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return "";
    
    MD5_CTX ctx;
    MD5_Init(&ctx);
    
    char buffer[4096];
    while (file.read(buffer, sizeof(buffer))) {
        MD5_Update(&ctx, buffer, file.gcount());
    }
    MD5_Update(&ctx, buffer, file.gcount());
    
    unsigned char digest[MD5_DIGEST_LENGTH];
    MD5_Final(digest, &ctx);
    
    std::stringstream ss;
    for (int i = 0; i < MD5_DIGEST_LENGTH; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)digest[i];
    }
    
    return ss.str();
}

std::string FileSystem::calculateSHA1(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return "";
    
    SHA_CTX ctx;
    SHA1_Init(&ctx);
    
    char buffer[4096];
    while (file.read(buffer, sizeof(buffer))) {
        SHA1_Update(&ctx, buffer, file.gcount());
    }
    SHA1_Update(&ctx, buffer, file.gcount());
    
    unsigned char digest[SHA_DIGEST_LENGTH];
    SHA1_Final(digest, &ctx);
    
    std::stringstream ss;
    for (int i = 0; i < SHA_DIGEST_LENGTH; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)digest[i];
    }
    
    return ss.str();
}

std::string FileSystem::calculateSHA256(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return "";
    
    SHA256_CTX ctx;
    SHA256_Init(&ctx);
    
    char buffer[4096];
    while (file.read(buffer, sizeof(buffer))) {
        SHA256_Update(&ctx, buffer, file.gcount());
    }
    SHA256_Update(&ctx, buffer, file.gcount());
    
    unsigned char digest[SHA256_DIGEST_LENGTH];
    SHA256_Final(digest, &ctx);
    
    std::stringstream ss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)digest[i];
    }
    
    return ss.str();
}

bool FileSystem::watchDirectory(const std::string& path,
                                std::function<void(const FileEvent&)> callback,
                                bool recursive) {
    // Inotify implementation would go here
    // For now, return false (not implemented)
    return false;
}

bool FileSystem::stopWatching(const std::string& path) {
    return false;
}

bool FileSystem::isWatching(const std::string& path) {
    return false;
}

std::vector<std::string> FileSystem::getWatchedDirectories() {
    return {};
}

bool FileSystem::lockFile(const std::string& path, bool wait) {
    // Simple file locking using flock
    int fd = open(path.c_str(), O_RDWR);
    if (fd < 0) return false;
    
    struct flock lock;
    lock.l_type = F_WRLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0;
    
    int ret = fcntl(fd, wait ? F_SETLKW : F_SETLK, &lock);
    close(fd);
    
    return ret == 0;
}

bool FileSystem::unlockFile(const std::string& path) {
    int fd = open(path.c_str(), O_RDWR);
    if (fd < 0) return false;
    
    struct flock lock;
    lock.l_type = F_UNLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0;
    
    int ret = fcntl(fd, F_SETLK, &lock);
    close(fd);
    
    return ret == 0;
}

bool FileSystem::isFileLocked(const std::string& path) {
    int fd = open(path.c_str(), O_RDWR);
    if (fd < 0) return false;
    
    struct flock lock;
    lock.l_type = F_WRLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0;
    
    int ret = fcntl(fd, F_GETLK, &lock);
    close(fd);
    
    return ret == 0 && lock.l_type != F_UNLCK;
}

bool FileSystem::tryLockFile(const std::string& path) {
    return lockFile(path, false);
}

std::string FileSystem::createTempFile(const std::string& prefix,
                                       const std::string& suffix,
                                       const std::string& directory) {
    std::string dir = directory.empty() ? "/tmp" : directory;
    std::string template_path = dir + "/" + prefix + "XXXXXX" + suffix;
    
    char* temp = strdup(template_path.c_str());
    int fd = mkstemps(temp, suffix.length());
    
    if (fd < 0) {
        free(temp);
        return "";
    }
    
    close(fd);
    std::string result(temp);
    free(temp);
    
    return result;
}

std::string FileSystem::createTempDirectory(const std::string& prefix,
                                            const std::string& directory) {
    std::string dir = directory.empty() ? "/tmp" : directory;
    std::string template_path = dir + "/" + prefix + "XXXXXX";
    
    char* temp = strdup(template_path.c_str());
    char* result = mkdtemp(temp);
    
    if (!result) {
        free(temp);
        return "";
    }
    
    std::string path(result);
    free(temp);
    
    return path;
}

bool FileSystem::deleteTempFiles(const std::string& directory, int max_age_seconds) {
    std::string dir = directory.empty() ? "/tmp" : directory;
    
    auto files = listAll(dir);
    auto now = std::chrono::system_clock::now();
    
    for (const auto& file : files) {
        std::string full_path = dir + "/" + file;
        auto mtime = getModificationTime(full_path);
        auto age = std::chrono::duration_cast<std::chrono::seconds>(now - mtime).count();
        
        if (age > max_age_seconds) {
            if (isFile(full_path)) {
                removeFile(full_path);
            } else if (isDirectory(full_path)) {
                removeDirectory(full_path, true);
            }
        }
    }
    
    return true;
}

bool FileSystem::sync(const std::string& path) {
    return fsync(open(path.c_str(), O_RDONLY)) == 0;
}

bool FileSystem::flush(const std::string& path) {
    return sync(path);
}

bool FileSystem::compress(const std::string& path, bool recursive) {
    // Placeholder - would require zlib
    return false;
}

bool FileSystem::decompress(const std::string& path, const std::string& destination) {
    // Placeholder - would require zlib
    return false;
}

bool FileSystem::encrypt(const std::string& path, const std::string& key) {
    // Placeholder - would require OpenSSL
    return false;
}

bool FileSystem::decrypt(const std::string& path, const std::string& key) {
    // Placeholder - would require OpenSSL
    return false;
}

void FileSystem::printFileInfo(const std::string& path) {
    std::cout << "File: " << path << std::endl;
    std::cout << "  Exists: " << (exists(path) ? "Yes" : "No") << std::endl;
    if (!exists(path)) return;
    
    std::cout << "  Size: " << getFileSize(path) << " bytes" << std::endl;
    std::cout << "  Type: " << (isDirectory(path) ? "Directory" : "File") << std::endl;
    std::cout << "  MIME: " << getMimeType(path) << std::endl;
    std::cout << "  MD5: " << calculateMD5(path) << std::endl;
    
    auto mtime = getModificationTime(path);
    auto time_t = std::chrono::system_clock::to_time_t(mtime);
    std::cout << "  Modified: " << std::ctime(&time_t);
}

bool FileSystem::verifyIntegrity(const std::string& path, const std::string& checksum) {
    std::string calculated = calculateMD5(path);
    return calculated == checksum;
}

std::vector<std::string> FileSystem::splitPath(const std::string& path) {
    std::vector<std::string> parts;
    std::stringstream ss(path);
    std::string part;
    
    while (std::getline(ss, part, '/')) {
        if (!part.empty()) {
            parts.push_back(part);
        }
    }
    
    return parts;
}

bool FileSystem::isPathAbsolute(const std::string& path) {
    return !path.empty() && path[0] == '/';
}

bool FileSystem::isPathRelative(const std::string& path) {
    return !isPathAbsolute(path);
}

bool FileSystem::isPathValid(const std::string& path) {
    // Check for invalid characters
    std::string invalid_chars = "\\:*?\"<>|";
    for (char c : path) {
        if (invalid_chars.find(c) != std::string::npos) {
            return false;
        }
    }
    return true;
}

std::string FileSystem::sanitizePath(const std::string& path) {
    std::string result = path;
    
    // Replace invalid characters
    std::string invalid_chars = "\\:*?\"<>|";
    for (char& c : result) {
        if (invalid_chars.find(c) != std::string::npos) {
            c = '_';
        }
    }
    
    return result;
}

} // namespace EdgeAI
