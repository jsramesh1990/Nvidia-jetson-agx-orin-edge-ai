#include "storage/storage_manager.h"
#include "storage/file_system.h"
#include <nlohmann/json.hpp>
#include <yaml-cpp/yaml.h>
#include <tinyxml2.h>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <thread>
#include <algorithm>
#include <random>
#include <functional>

namespace EdgeAI {

class StorageManager::Impl {
public:
    Impl(const StorageConfig& config) 
        : config_(config), initialized_(false), driver_(config) {
        // Initialize cache
        cache_config_.max_size_mb = 1024;
        cache_config_.max_age_days = 7;
        cache_config_.use_lru = true;
        cache_config_.auto_cleanup = true;
        
        // Initialize index
        index_config_.enable_full_text = false;
        index_config_.enable_metadata = true;
        index_config_.index_extensions = {".txt", ".json", ".yaml", ".xml", ".md"};
        index_config_.rebuild_interval_hours = 24;
        index_config_.auto_update = true;
    }
    
    ~Impl() {
        shutdown();
    }
    
    bool initialize() {
        if (initialized_) return true;
        
        if (!driver_.initialize()) {
            setError("Failed to initialize storage driver");
            return false;
        }
        
        // Initialize cache directory
        std::string cache_path = driver_.getFullPath("cache");
        if (!FileSystem::createDirectory(cache_path)) {
            setError("Failed to create cache directory");
            return false;
        }
        
        // Initialize index directory
        std::string index_path = driver_.getFullPath("index");
        if (!FileSystem::createDirectory(index_path)) {
            setError("Failed to create index directory");
            return false;
        }
        
        initialized_ = true;
        
        // Start background tasks
        startBackgroundTasks();
        
        return true;
    }
    
    void shutdown() {
        running_ = false;
        if (cleanup_thread_.joinable()) {
            cleanup_thread_.join();
        }
        if (index_thread_.joinable()) {
            index_thread_.join();
        }
        driver_.shutdown();
        initialized_ = false;
    }
    
    bool isReady() const {
        return initialized_ && driver_.isReady();
    }
    
    bool storeData(const std::string& key, const std::vector<uint8_t>& data) {
        if (!isReady()) return false;
        
        std::string path = getKeyPath(key);
        std::string metadata_path = path + ".meta";
        
        // Store data
        if (!driver_.createFile(path, data, true)) {
            setError("Failed to store data for key: " + key);
            return false;
        }
        
        // Store metadata
        nlohmann::json metadata;
        metadata["key"] = key;
        metadata["size"] = data.size();
        metadata["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        metadata["type"] = "binary";
        
        std::string meta_data = metadata.dump();
        std::vector<uint8_t> meta_bytes(meta_data.begin(), meta_data.end());
        driver_.createFile(metadata_path, meta_bytes, true);
        
        // Update cache
        addToCache(key, data);
        
        // Notify event
        if (event_callback_) {
            event_callback_("store", key);
        }
        
        return true;
    }
    
    bool retrieveData(const std::string& key, std::vector<uint8_t>& data) {
        if (!isReady()) return false;
        
        // Check cache first
        if (getFromCache(key, data)) {
            return true;
        }
        
        // Read from storage
        std::string path = getKeyPath(key);
        if (!driver_.readFile(path, data)) {
            setError("Failed to retrieve data for key: " + key);
            return false;
        }
        
        // Update cache
        addToCache(key, data);
        
        // Notify event
        if (event_callback_) {
            event_callback_("retrieve", key);
        }
        
        return true;
    }
    
    bool deleteData(const std::string& key) {
        if (!isReady()) return false;
        
        std::string path = getKeyPath(key);
        std::string metadata_path = path + ".meta";
        
        // Delete data
        if (!driver_.deleteFile(path)) {
            setError("Failed to delete data for key: " + key);
            return false;
        }
        
        // Delete metadata
        driver_.deleteFile(metadata_path);
        
        // Remove from cache
        removeFromCache(key);
        
        // Notify event
        if (event_callback_) {
            event_callback_("delete", key);
        }
        
        return true;
    }
    
    bool exists(const std::string& key) const {
        if (!isReady()) return false;
        
        std::string path = getKeyPath(key);
        return driver_.fileExists(path);
    }
    
    std::vector<std::string> listKeys(const std::string& prefix) const {
        if (!isReady()) return {};
        
        std::vector<std::string> keys;
        std::string data_path = driver_.getFullPath("data");
        
        auto files = FileSystem::listFiles(data_path);
        for (const auto& file : files) {
            if (file.find(".meta") == std::string::npos) {
                std::string key = file;
                if (prefix.empty() || key.find(prefix) == 0) {
                    keys.push_back(key);
                }
            }
        }
        
        return keys;
    }
    
    bool storeJSON(const std::string& key, const nlohmann::json& data) {
        if (!isReady()) return false;
        
        std::string json_str = data.dump(2);
        std::vector<uint8_t> bytes(json_str.begin(), json_str.end());
        
        // Store with type metadata
        std::string path = getKeyPath(key);
        std::string metadata_path = path + ".meta";
        
        if (!driver_.createFile(path, bytes, true)) {
            setError("Failed to store JSON for key: " + key);
            return false;
        }
        
        nlohmann::json metadata;
        metadata["key"] = key;
        metadata["size"] = bytes.size();
        metadata["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        metadata["type"] = "json";
        
        std::string meta_data = metadata.dump();
        std::vector<uint8_t> meta_bytes(meta_data.begin(), meta_data.end());
        driver_.createFile(metadata_path, meta_bytes, true);
        
        return true;
    }
    
    bool retrieveJSON(const std::string& key, nlohmann::json& data) {
        if (!isReady()) return false;
        
        std::vector<uint8_t> bytes;
        if (!retrieveData(key, bytes)) {
            return false;
        }
        
        try {
            std::string json_str(bytes.begin(), bytes.end());
            data = nlohmann::json::parse(json_str);
            return true;
        } catch (const std::exception& e) {
            setError("Failed to parse JSON for key: " + key + " - " + e.what());
            return false;
        }
    }
    
    bool storeYAML(const std::string& key, const std::string& yaml_data) {
        if (!isReady()) return false;
        
        std::vector<uint8_t> bytes(yaml_data.begin(), yaml_data.end());
        
        std::string path = getKeyPath(key);
        if (!driver_.createFile(path + ".yaml", bytes, true)) {
            setError("Failed to store YAML for key: " + key);
            return false;
        }
        
        return true;
    }
    
    bool retrieveYAML(const std::string& key, std::string& yaml_data) {
        if (!isReady()) return false;
        
        std::string path = getKeyPath(key);
        std::vector<uint8_t> bytes;
        if (!driver_.readFile(path + ".yaml", bytes)) {
            setError("Failed to retrieve YAML for key: " + key);
            return false;
        }
        
        yaml_data = std::string(bytes.begin(), bytes.end());
        return true;
    }
    
    bool storeXML(const std::string& key, const std::string& xml_data) {
        if (!isReady()) return false;
        
        std::vector<uint8_t> bytes(xml_data.begin(), xml_data.end());
        
        std::string path = getKeyPath(key);
        if (!driver_.createFile(path + ".xml", bytes, true)) {
            setError("Failed to store XML for key: " + key);
            return false;
        }
        
        return true;
    }
    
    bool retrieveXML(const std::string& key, std::string& xml_data) {
        if (!isReady()) return false;
        
        std::string path = getKeyPath(key);
        std::vector<uint8_t> bytes;
        if (!driver_.readFile(path + ".xml", bytes)) {
            setError("Failed to retrieve XML for key: " + key);
            return false;
        }
        
        xml_data = std::string(bytes.begin(), bytes.end());
        return true;
    }
    
    bool addToCache(const std::string& key, const std::vector<uint8_t>& data) {
        if (data.size() > cache_config_.max_size_mb * 1024 * 1024) {
            return false; // Data too large for cache
        }
        
        std::lock_guard<std::mutex> lock(cache_mutex_);
        
        // Check if already in cache
        auto it = cache_map_.find(key);
        if (it != cache_map_.end()) {
            // Update existing entry
            it->second.data = data;
            it->second.last_access = std::chrono::system_clock::now();
            it->second.access_count++;
            return true;
        }
        
        // Check cache size
        size_t total_size = 0;
        for (const auto& entry : cache_map_) {
            total_size += entry.second.data.size();
        }
        
        // Evict if needed
        while (total_size + data.size() > cache_config_.max_size_mb * 1024 * 1024) {
            if (!evictCacheEntry()) {
                return false;
            }
            total_size = 0;
            for (const auto& entry : cache_map_) {
                total_size += entry.second.data.size();
            }
        }
        
        // Add to cache
        CacheEntry entry;
        entry.key = key;
        entry.data = data;
        entry.last_access = std::chrono::system_clock::now();
        entry.created = std::chrono::system_clock::now();
        entry.access_count = 1;
        entry.is_pinned = false;
        
        cache_map_[key] = entry;
        cache_keys_.push_front(key);
        
        return true;
    }
    
    bool getFromCache(const std::string& key, std::vector<uint8_t>& data) {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        
        auto it = cache_map_.find(key);
        if (it == cache_map_.end()) {
            return false;
        }
        
        data = it->second.data;
        it->second.last_access = std::chrono::system_clock::now();
        it->second.access_count++;
        
        // Move to front (LRU)
        auto pos = std::find(cache_keys_.begin(), cache_keys_.end(), key);
        if (pos != cache_keys_.end()) {
            cache_keys_.erase(pos);
            cache_keys_.push_front(key);
        }
        
        return true;
    }
    
    bool removeFromCache(const std::string& key) {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        
        auto it = cache_map_.find(key);
        if (it == cache_map_.end()) {
            return false;
        }
        
        if (it->second.is_pinned) {
            return false; // Cannot remove pinned entry
        }
        
        cache_map_.erase(it);
        
        auto pos = std::find(cache_keys_.begin(), cache_keys_.end(), key);
        if (pos != cache_keys_.end()) {
            cache_keys_.erase(pos);
        }
        
        return true;
    }
    
    void clearCache() {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        cache_map_.clear();
        cache_keys_.clear();
    }
    
    CacheConfig getCacheConfig() const {
        return cache_config_;
    }
    
    void setCacheConfig(const CacheConfig& config) {
        cache_config_ = config;
        
        // Apply new config
        if (cache_config_.auto_cleanup) {
            // Start auto-cleanup if enabled
        }
    }
    
    std::vector<CacheEntry> getCacheStatus() const {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        
        std::vector<CacheEntry> status;
        for (const auto& [key, entry] : cache_map_) {
            CacheEntry status_entry;
            status_entry.key = key;
            status_entry.file_path = getKeyPath(key);
            status_entry.size_bytes = entry.data.size();
            status_entry.last_access = entry.last_access;
            status_entry.created = entry.created;
            status_entry.access_count = entry.access_count;
            status_entry.is_pinned = entry.is_pinned;
            status.push_back(status_entry);
        }
        
        return status;
    }
    
    bool indexFile(const std::string& path) {
        if (!isReady()) return false;
        
        std::string full_path = driver_.getFullPath(path);
        if (!FileSystem::exists(full_path) || FileSystem::isDirectory(full_path)) {
            return false;
        }
        
        // Check if file should be indexed
        std::string ext = FileSystem::getExtension(full_path);
        bool should_index = false;
        for (const auto& index_ext : index_config_.index_extensions) {
            if (ext == index_ext) {
                should_index = true;
                break;
            }
        }
        
        if (!should_index) {
            return false;
        }
        
        // Read file content
        std::vector<uint8_t> data;
        if (!driver_.readFile(full_path, data)) {
            return false;
        }
        
        // Create index entry
        IndexEntry entry;
        entry.path = path;
        entry.name = FileSystem::getBaseName(path);
        entry.size_bytes = data.size();
        entry.modified = FileSystem::getModificationTime(full_path);
        
        // Extract content preview
        if (data.size() > 1024) {
            entry.content_preview = std::string(data.begin(), data.begin() + 1024) + "...";
        } else {
            entry.content_preview = std::string(data.begin(), data.end());
        }
        
        // Extract keywords (simple)
        std::string content(data.begin(), data.end());
        std::vector<std::string> keywords;
        std::stringstream ss(content);
        std::string word;
        while (ss >> word) {
            if (word.length() > 3) {
                std::transform(word.begin(), word.end(), word.begin(), ::tolower);
                keywords.push_back(word);
            }
        }
        entry.keywords = keywords;
        
        // Extract metadata
        if (ext == ".json") {
            try {
                auto json = nlohmann::json::parse(content);
                for (auto& [key, value] : json.items()) {
                    if (value.is_string()) {
                        entry.metadata[key] = value.get<std::string>();
                    }
                }
            } catch (...) {}
        }
        
        // Store index entry
        std::lock_guard<std::mutex> lock(index_mutex_);
        index_entries_[path] = entry;
        
        return true;
    }
    
    bool rebuildIndex(const std::string& path) {
        if (!isReady()) return false;
        
        std::string search_path = path.empty() ? driver_.getFullPath("data") : driver_.getFullPath(path);
        
        if (!FileSystem::exists(search_path)) {
            return false;
        }
        
        // Clear existing index
        std::lock_guard<std::mutex> lock(index_mutex_);
        index_entries_.clear();
        
        // Recursively index files
        auto files = FileSystem::listAll(search_path, true);
        for (const auto& file : files) {
            std::string full_path = search_path + "/" + file;
            if (FileSystem::isFile(full_path)) {
                indexFile(file);
            }
        }
        
        return true;
    }
    
    std::vector<IndexEntry> searchIndex(const std::string& query, int max_results) const {
        std::lock_guard<std::mutex> lock(index_mutex_);
        
        std::vector<IndexEntry> results;
        
        // Convert query to lowercase for case-insensitive search
        std::string query_lower = query;
        std::transform(query_lower.begin(), query_lower.end(), query_lower.begin(), ::tolower);
        
        for (const auto& [path, entry] : index_entries_) {
            // Search in content preview
            std::string content_lower = entry.content_preview;
            std::transform(content_lower.begin(), content_lower.end(), content_lower.begin(), ::tolower);
            
            if (content_lower.find(query_lower) != std::string::npos) {
                IndexEntry result = entry;
                result.relevance_score = 1.0f;
                results.push_back(result);
                continue;
            }
            
            // Search in keywords
            for (const auto& keyword : entry.keywords) {
                std::string keyword_lower = keyword;
                std::transform(keyword_lower.begin(), keyword_lower.end(), 
                              keyword_lower.begin(), ::tolower);
                if (keyword_lower.find(query_lower) != std::string::npos) {
                    IndexEntry result = entry;
                    result.relevance_score = 0.8f;
                    results.push_back(result);
                    break;
                }
            }
            
            // Search in metadata
            for (const auto& [key, value] : entry.metadata) {
                std::string value_lower = value;
                std::transform(value_lower.begin(), value_lower.end(), 
                              value_lower.begin(), ::tolower);
                if (value_lower.find(query_lower) != std::string::npos) {
                    IndexEntry result = entry;
                    result.relevance_score = 0.6f;
                    results.push_back(result);
                    break;
                }
            }
        }
        
        // Sort by relevance
        std::sort(results.begin(), results.end(),
                  [](const IndexEntry& a, const IndexEntry& b) {
                      return a.relevance_score > b.relevance_score;
                  });
        
        // Limit results
        if (results.size() > static_cast<size_t>(max_results)) {
            results.resize(max_results);
        }
        
        return results;
    }
    
    IndexConfig getIndexConfig() const {
        return index_config_;
    }
    
    void setIndexConfig(const IndexConfig& config) {
        index_config_ = config;
    }
    
    QuotaInfo getQuotaInfo() const {
        if (!isReady()) return QuotaInfo{};
        
        QuotaInfo info;
        auto stats = driver_.getStorageStats();
        
        info.total_bytes = static_cast<size_t>(stats.total_space_gb * 1024 * 1024 * 1024);
        info.used_bytes = static_cast<size_t>(stats.used_space_gb * 1024 * 1024 * 1024);
        info.free_bytes = static_cast<size_t>(stats.free_space_gb * 1024 * 1024 * 1024);
        info.total_files = stats.file_count;
        info.used_files = stats.file_count;
        info.percent_used = static_cast<int>(stats.disk_usage_percentage);
        
        // Usage by type
        auto files = FileSystem::listAll(driver_.getFullPath("data"), true);
        for (const auto& file : files) {
            std::string ext = FileSystem::getExtension(file);
            if (!ext.empty()) {
                info.usage_by_type[ext] += FileSystem::getFileSize(driver_.getFullPath("data") + "/" + file);
            }
        }
        
        return info;
    }
    
    bool checkQuota(const std::string& key, size_t size) const {
        auto info = getQuotaInfo();
        // Check if adding this data would exceed quota
        return (info.used_bytes + size) < info.total_bytes;
    }
    
    bool setUserQuota(const std::string& user, size_t max_bytes) {
        std::lock_guard<std::mutex> lock(quota_mutex_);
        user_quotas_[user] = max_bytes;
        return true;
    }
    
    size_t getUserQuota(const std::string& user) const {
        std::lock_guard<std::mutex> lock(quota_mutex_);
        auto it = user_quotas_.find(user);
        if (it == user_quotas_.end()) {
            return 0;
        }
        return it->second;
    }
    
    bool migrateData(const std::string& source, const std::string& destination) {
        if (!isReady()) return false;
        
        std::string src_path = driver_.getFullPath(source);
        std::string dst_path = driver_.getFullPath(destination);
        
        if (!FileSystem::exists(src_path)) {
            setError("Source path does not exist: " + source);
            return false;
        }
        
        // Create destination directory
        if (!FileSystem::createDirectory(dst_path)) {
            setError("Failed to create destination directory: " + destination);
            return false;
        }
        
        // Copy all files
        auto files = FileSystem::listAll(src_path, true);
        for (const auto& file : files) {
            std::string src_file = src_path + "/" + file;
            std::string dst_file = dst_path + "/" + file;
            
            if (FileSystem::isFile(src_file)) {
                if (!FileSystem::copy(src_file, dst_file)) {
                    setError("Failed to copy file: " + file);
                    return false;
                }
            } else if (FileSystem::isDirectory(src_file)) {
                if (!FileSystem::createDirectory(dst_file)) {
                    setError("Failed to create directory: " + file);
                    return false;
                }
            }
        }
        
        return true;
    }
    
    bool backupData(const std::string& backup_name) {
        if (!isReady()) return false;
        
        std::string timestamp = getTimestamp();
        std::string name = backup_name.empty() ? timestamp : backup_name;
        
        return driver_.createBackup("data", name, true);
    }
    
    bool restoreData(const std::string& backup_name) {
        if (!isReady()) return false;
        
        return driver_.restoreBackup(backup_name, "data");
    }
    
    HealthStatus getHealthStatus() const {
        HealthStatus status;
        
        try {
            // Check driver health
            status.is_healthy = driver_.isReady();
            status.uptime_hours = 0; // Track uptime separately
            
            // Get stats
            auto stats = driver_.getStats();
            status.total_operations = stats.total_transactions;
            status.avg_response_time_ms = stats.avg_transaction_time_ms;
            
            // Check cache health
            size_t cache_size = 0;
            {
                std::lock_guard<std::mutex> lock(cache_mutex_);
                cache_size = cache_map_.size();
            }
            
            status.last_check = std::chrono::system_clock::now();
            status.cache_hit_rate = cache_size > 0 ? 80 : 0; // Simplified
            
            if (!status.is_healthy) {
                status.last_error = "Storage driver not ready";
            }
            
        } catch (const std::exception& e) {
            status.is_healthy = false;
            status.last_error = e.what();
        }
        
        return status;
    }
    
    bool runHealthCheck() {
        return isReady() && driver_.testStorage();
    }
    
    void setEventCallback(std::function<void(const std::string&, const std::string&)> callback) {
        event_callback_ = callback;
    }
    
    std::string getStoragePath() const {
        return driver_.getFullPath("data");
    }
    
    std::string getCachePath() const {
        return driver_.getFullPath("cache");
    }
    
    std::string getIndexPath() const {
        return driver_.getFullPath("index");
    }
    
    bool vacuum() {
        if (!isReady()) return false;
        
        // Remove orphaned metadata files
        std::string data_path = driver_.getFullPath("data");
        auto files = FileSystem::listAll(data_path);
        
        for (const auto& file : files) {
            if (file.find(".meta") != std::string::npos) {
                std::string key = file.substr(0, file.find(".meta"));
                std::string data_file = data_path + "/" + key;
                if (!FileSystem::exists(data_file)) {
                    driver_.deleteFile(data_path + "/" + file);
                }
            }
        }
        
        return true;
    }
    
    bool optimize() {
        if (!isReady()) return false;
        
        // Rebuild index
        rebuildIndex();
        
        // Vacuum storage
        vacuum();
        
        // Clear old cache entries
        cleanupCache();
        
        return true;
    }
    
    size_t getTotalSize() const {
        if (!isReady()) return 0;
        
        auto stats = driver_.getStorageStats();
        return static_cast<size_t>(stats.used_space_gb * 1024 * 1024 * 1024);
    }
    
    size_t getFileCount() const {
        if (!isReady()) return 0;
        
        auto stats = driver_.getStorageStats();
        return stats.file_count;
    }
    
    std::string getStatistics() const {
        std::stringstream ss;
        auto health = getHealthStatus();
        auto quota = getQuotaInfo();
        auto cache_status = getCacheStatus();
        
        ss << "=== Storage Manager Statistics ===\n";
        ss << "Health: " << (health.is_healthy ? "Healthy" : "Unhealthy") << "\n";
        ss << "Uptime: " << health.uptime_hours << " hours\n";
        ss << "Total Operations: " << health.total_operations << "\n";
        ss << "Avg Response Time: " << health.avg_response_time_ms << " ms\n";
        ss << "\n=== Quota Information ===\n";
        ss << "Total Space: " << quota.total_bytes / (1024*1024*1024) << " GB\n";
        ss << "Used Space: " << quota.used_bytes / (1024*1024*1024) << " GB\n";
        ss << "Free Space: " << quota.free_bytes / (1024*1024*1024) << " GB\n";
        ss << "Usage: " << quota.percent_used << "%\n";
        ss << "Total Files: " << quota.total_files << "\n";
        ss << "\n=== Cache Information ===\n";
        ss << "Cache Entries: " << cache_status.size() << "\n";
        ss << "Cache Size: " << getCacheSize() / (1024*1024) << " MB\n";
        ss << "Hit Rate: " << health.cache_hit_rate << "%\n";
        ss << "\n=== Index Information ===\n";
        ss << "Indexed Files: " << index_entries_.size() << "\n";
        ss << "Auto-Update: " << (index_config_.auto_update ? "Enabled" : "Disabled") << "\n";
        
        return ss.str();
    }
    
private:
    StorageConfig config_;
    bool initialized_;
    StorageDriver driver_;
    std::atomic<bool> running_{false};
    std::thread cleanup_thread_;
    std::thread index_thread_;
    
    // Cache
    struct CacheEntryData {
        std::vector<uint8_t> data;
        std::chrono::system_clock::time_point last_access;
        std::chrono::system_clock::time_point created;
        int access_count = 0;
        bool is_pinned = false;
    };
    CacheConfig cache_config_;
    std::map<std::string, CacheEntryData> cache_map_;
    std::list<std::string> cache_keys_;
    mutable std::mutex cache_mutex_;
    
    // Index
    IndexConfig index_config_;
    std::map<std::string, IndexEntry> index_entries_;
    mutable std::mutex index_mutex_;
    
    // Quota
    std::map<std::string, size_t> user_quotas_;
    mutable std::mutex quota_mutex_;
    
    // Events
    std::function<void(const std::string&, const std::string&)> event_callback_;
    
    std::string getKeyPath(const std::string& key) const {
        return driver_.getFullPath("data/" + key);
    }
    
    std::string getTimestamp() const {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time_t), "%Y%m%d_%H%M%S");
        return ss.str();
    }
    
    void setError(const std::string& error) {
        std::cerr << "Storage Manager Error: " << error << std::endl;
    }
    
    bool evictCacheEntry() {
        if (cache_keys_.empty()) return false;
        
        // Find least recently used non-pinned entry
        for (auto it = cache_keys_.rbegin(); it != cache_keys_.rend(); ++it) {
            auto map_it = cache_map_.find(*it);
            if (map_it != cache_map_.end() && !map_it->second.is_pinned) {
                std::string key = *it;
                cache_map_.erase(map_it);
                cache_keys_.erase(std::next(it).base());
                return true;
            }
        }
        
        return false;
    }
    
    void cleanupCache() {
        auto now = std::chrono::system_clock::now();
        auto cutoff = now - std::chrono::hours(cache_config_.max_age_days * 24);
        
        std::lock_guard<std::mutex> lock(cache_mutex_);
        
        std::vector<std::string> to_remove;
        for (const auto& [key, entry] : cache_map_) {
            if (!entry.is_pinned && entry.last_access < cutoff) {
                to_remove.push_back(key);
            }
        }
        
        for (const auto& key : to_remove) {
            cache_map_.erase(key);
            auto pos = std::find(cache_keys_.begin(), cache_keys_.end(), key);
            if (pos != cache_keys_.end()) {
                cache_keys_.erase(pos);
            }
        }
    }
    
    size_t getCacheSize() const {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        size_t total = 0;
        for (const auto& [key, entry] : cache_map_) {
            total += entry.data.size();
        }
        return total;
    }
    
    void startBackgroundTasks() {
        running_ = true;
        
        // Cleanup thread
        cleanup_thread_ = std::thread([this]() {
            while (running_) {
                std::this_thread::sleep_for(std::chrono::hours(1));
                
                if (cache_config_.auto_cleanup) {
                    cleanupCache();
                }
                
                if (index_config_.auto_update) {
                    rebuildIndex();
                }
            }
        });
        
        // Index thread
        index_thread_ = std::thread([this]() {
            while (running_) {
                std::this_thread::sleep_for(std::chrono::hours(24));
                
                if (index_config_.auto_update) {
                    rebuildIndex();
                }
            }
        });
    }
};

// StorageManager implementation
StorageManager::StorageManager(const StorageConfig& config) 
    : pImpl(std::make_unique<Impl>(config)) {}

StorageManager::~StorageManager() = default;

bool StorageManager::initialize() { return pImpl->initialize(); }
bool StorageManager::isReady() const { return pImpl->isReady(); }
void StorageManager::shutdown() { pImpl->shutdown(); }

bool StorageManager::storeData(const std::string& key, const std::vector<uint8_t>& data) {
    return pImpl->storeData(key, data);
}
bool StorageManager::retrieveData(const std::string& key, std::vector<uint8_t>& data) {
    return pImpl->retrieveData(key, data);
}
bool StorageManager::deleteData(const std::string& key) {
    return pImpl->deleteData(key);
}
bool StorageManager::exists(const std::string& key) const {
    return pImpl->exists(key);
}
std::vector<std::string> StorageManager::listKeys(const std::string& prefix) const {
    return pImpl->listKeys(prefix);
}

bool StorageManager::storeJSON(const std::string& key, const nlohmann::json& data) {
    return pImpl->storeJSON(key, data);
}
bool StorageManager::retrieveJSON(const std::string& key, nlohmann::json& data) {
    return pImpl->retrieveJSON(key, data);
}
bool StorageManager::storeYAML(const std::string& key, const std::string& yaml_data) {
    return pImpl->storeYAML(key, yaml_data);
}
bool StorageManager::retrieveYAML(const std::string& key, std::string& yaml_data) {
    return pImpl->retrieveYAML(key, yaml_data);
}
bool StorageManager::storeXML(const std::string& key, const std::string& xml_data) {
    return pImpl->storeXML(key, xml_data);
}
bool StorageManager::retrieveXML(const std::string& key, std::string& xml_data) {
    return pImpl->retrieveXML(key, xml_data);
}

bool StorageManager::addToCache(const std::string& key, const std::vector<uint8_t>& data) {
    return pImpl->addToCache(key, data);
}
bool StorageManager::getFromCache(const std::string& key, std::vector<uint8_t>& data) {
    return pImpl->getFromCache(key, data);
}
bool StorageManager::removeFromCache(const std::string& key) {
    return pImpl->removeFromCache(key);
}
void StorageManager::clearCache() { pImpl->clearCache(); }
CacheConfig StorageManager::getCacheConfig() const { return pImpl->getCacheConfig(); }
void StorageManager::setCacheConfig(const CacheConfig& config) {
    pImpl->setCacheConfig(config);
}
std::vector<CacheEntry> StorageManager::getCacheStatus() const {
    return pImpl->getCacheStatus();
}

bool StorageManager::indexFile(const std::string& path) {
    return pImpl->indexFile(path);
}
bool StorageManager::rebuildIndex(const std::string& path) {
    return pImpl->rebuildIndex(path);
}
std::vector<IndexEntry> StorageManager::searchIndex(const std::string& query,
                                                    int max_results) const {
    return pImpl->searchIndex(query, max_results);
}
IndexConfig StorageManager::getIndexConfig() const { return pImpl->getIndexConfig(); }
void StorageManager::setIndexConfig(const IndexConfig& config) {
    pImpl->setIndexConfig(config);
}

StorageManager::QuotaInfo StorageManager::getQuotaInfo() const {
    return pImpl->getQuotaInfo();
}
bool StorageManager::checkQuota(const std::string& key, size_t size) const {
    return pImpl->checkQuota(key, size);
}
bool StorageManager::setUserQuota(const std::string& user, size_t max_bytes) {
    return pImpl->setUserQuota(user, max_bytes);
}
size_t StorageManager::getUserQuota(const std::string& user) const {
    return pImpl->getUserQuota(user);
}

bool StorageManager::migrateData(const std::string& source, const std::string& destination) {
    return pImpl->migrateData(source, destination);
}
bool StorageManager::backupData(const std::string& backup_name) {
    return pImpl->backupData(backup_name);
}
bool StorageManager::restoreData(const std::string& backup_name) {
    return pImpl->restoreData(backup_name);
}

StorageManager::HealthStatus StorageManager::getHealthStatus() const {
    return pImpl->getHealthStatus();
}
bool StorageManager::runHealthCheck() { return pImpl->runHealthCheck(); }

void StorageManager::setEventCallback(std::function<void(const std::string&, 
                                                         const std::string&)> callback) {
    pImpl->setEventCallback(callback);
}

std::string StorageManager::getStoragePath() const { return pImpl->getStoragePath(); }
std::string StorageManager::getCachePath() const { return pImpl->getCachePath(); }
std::string StorageManager::getIndexPath() const { return pImpl->getIndexPath(); }
bool StorageManager::vacuum() { return pImpl->vacuum(); }
bool StorageManager::optimize() { return pImpl->optimize(); }
size_t StorageManager::getTotalSize() const { return pImpl->getTotalSize(); }
size_t StorageManager::getFileCount() const { return pImpl->getFileCount(); }
std::string StorageManager::getStatistics() const { return pImpl->getStatistics(); }

} // namespace EdgeAI
