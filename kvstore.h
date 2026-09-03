#ifndef KVSTORE_H
#define KVSTORE_H

#include <cstdint>
#include <fstream>
#include <mutex>
#include <string>
#include <unordered_map>

// Small educational Bitcask-style key-value store.
// The index stays in memory while values are stored in an append-only log.
class KVStore {
public:
    explicit KVStore(const std::string& path);
    ~KVStore();

    void set(const std::string& key, const std::string& value);
    bool get(const std::string& key, std::string& out);
    void del(const std::string& key);
    void compact();
    uint64_t fileSize();

private:
    void recover();

    struct IndexEntry {
        uint64_t value_offset;
        uint32_t value_size;
    };

    std::string path_;
    std::fstream file_;
    std::unordered_map<std::string, IndexEntry> index_;
    std::mutex mu_;
};

#endif // KVSTORE_H
