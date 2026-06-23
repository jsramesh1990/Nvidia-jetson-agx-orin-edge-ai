#pragma once
#include "storage_driver.h"
#include <vector>
#include <string>
#include <functional>
#include <map>

namespace EdgeAI {

// Cache management
struct CacheConfig {
    size_t max_size_mb = 1024;
    int max_age_days = 7;
    bool use_lru = true;
    bool auto_cleanup = true;
    std::vector<std::string> exclude_patterns;
};

struct CacheEntry {
    std::string key;
    std::string file_path;
    size_t size_bytes = 0;
    std::chrono::system_clock::time_point last_access;
    std::chrono::system_clock::time_point created;
    int access_count = 0;
    bool is_pinned = false;
};

// Index management
struct IndexConfig {
    bool enable_full_text = false;
    bool enable_metadata = true;
    std::vector<std::string> index_extensions;
    int rebuild_interval_hours = 24;
    bool auto_update = true;
};

struct IndexEntry {
    std::string path;
    std::string name;
    std::string content_preview;
    std::map<std::string, std::string> metadata;
    std::vector<std::string> keywords;
    int64_t size_bytes = 0;
    std::chrono::system_clock::time_point modified;
    float relevance_score = 0.0f;
};

class StorageManager {
public:
    StorageManager(const StorageConfig& config);
    ~StorageManager();
    
    // Initialization
    bool initialize();
    bool isReady() const;
    void shutdown();
    
    // Core storage operations
    bool storeData(const std::string& key, const std::vector<uint8_t>& data);
    bool retrieveData(const std::string& key, std::vector<uint8_t>& data);
    bool deleteData(const std::string& key);
    bool exists(const std::string& key) const;
    std::vector<std::string> listKeys(const std::string& prefix = "") const;
    
    // Structured data
    bool storeJSON(const std::string& key, const nlohmann::json& data);
    bool retrieveJSON(const std::string& key, nlohmann::json& data);
    bool storeYAML(const std::string& key, const std::string& yaml_data);
    bool retrieveYAML(const std::string& key, std::string& yaml_data);
    bool storeXML(const std::string& key, const std::string& xml_data);
    bool retrieveXML(const std::string& key, std::string& xml_data);
    
    // Cache management
    bool addToCache(const std::string& key, const std::vector<uint8_t>& data);
    bool getFromCache(const std::string& key, std::vector<uint8_t>& data);
    bool removeFromCache(const std::string& key);
    void clearCache();
    CacheConfig getCacheConfig() const;
    void setCacheConfig(const CacheConfig& config);
    std::vector<CacheEntry> getCacheStatus() const;
    
    // Index management
    bool indexFile(const std::string& path);
    bool rebuildIndex(const std::string& path = "");
    std::vector<IndexEntry> searchIndex(const std::string& query,
                                        int max_results = 100) const;
    IndexConfig getIndexConfig() const;
    void setIndexConfig(const IndexConfig& config);
    
    // Quota management
    struct QuotaInfo {
        size_t total_bytes = 0;
        size_t used_bytes = 0;
        size_t free_bytes = 0;
        size_t total_files = 0;
        size_t used_files = 0;
        int percent_used = 0;
        std::map<std::string, size_t> usage_by_type;
    };
    QuotaInfo getQuotaInfo() const;
    bool checkQuota(const std::string& key, size_t size) const;
    bool setUserQuota(const std::string& user, size_t max_bytes);
    size_t getUserQuota(const std::string& user) const;
    
    // Data migration
    bool migrateData(const std::string& source, const std::string& destination);
    bool backupData(const std::string& backup_name = "");
    bool restoreData(const std::string& backup_name);
    
    // Monitoring
    struct HealthStatus {
        bool is_healthy = false;
        double uptime_hours = 0.0;
        size_t total_operations = 0;
        double avg_response_time_ms = 0.0;
        size_t cache_hit_rate = 0;
        std::string last_error = "";
        std::chrono::system_clock::time_point last_check;
    };
    HealthStatus getHealthStatus() const;
    bool runHealthCheck();
    
    // Events
    void onStore(const std::string& key, size_t size);
    void onRetrieve(const std::string& key, size_t size);
    void onDelete(const std::string& key);
    void setEventCallback(std::function<void(const std::string&, 
                                            const std::string&)> callback);
    
    // Utility
    std::string getStoragePath() const;
    std::string getCachePath() const;
    std::string getIndexPath() const;
    bool vacuum();
    bool optimize();
    size_t getTotalSize() const;
    size_t getFileCount() const;
    std::string getStatistics() const;
    
private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

} // namespace EdgeAI
