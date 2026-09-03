#ifndef KVSTORE_H
#define KVSTORE_H

#include <string>
#include <unordered_map>
#include <fstream>
#include <cstdint>
#include <mutex>

// Stage 1: an append-only key-value store with an in-memory index.
//
// How it works at a glance:
//   - Every SET appends a record to the end of one data file (never overwrites).
//   - An in-memory hash map remembers, for each key, WHERE its latest value
//     lives in that file (a byte offset) and HOW BIG that value is.
//   - GET looks up the offset in the map, jumps to that spot in the file,
//     and reads the bytes back.
//   - DEL appends a "tombstone" record and removes the key from the map.

class KVStore {
public:
    // Opens (or creates) the data file at `path` and gets the store ready.
    explicit KVStore(const std::string& path);

    // Closes the data file cleanly.
    ~KVStore();

    // Store a key -> value. If the key already exists, this just appends a
    // newer record; the old one stays in the file but is no longer pointed to.
    void set(const std::string& key, const std::string& value);

    // Look up a key. Returns true and fills `out` if found; false if missing.
    bool get(const std::string& key, std::string& out);

    // Delete a key. Appends a tombstone and forgets the key.
    void del(const std::string& key);

    // Rewrite the data file keeping only the latest value of each live key,
    // throwing away superseded records and tombstones (Stage 3).
    void compact();

    // Current size of the data file in bytes (handy to see compaction shrink it).
    uint64_t fileSize();

private:
    // Rebuilds the in-memory index by replaying the whole data file on startup.
    // This is what makes data survive restarts and crashes (Stage 2).
    void recover();

    // What the in-memory index stores for each key: where its value is in the
    // file, and how many bytes long it is.
    struct IndexEntry {
        uint64_t value_offset; // byte position in the file where the value starts
        uint32_t value_size;   // length of the value in bytes
    };

    std::string path_;                                  // data file path
    std::fstream file_;                                 // the open data file
    std::unordered_map<std::string, IndexEntry> index_; // key -> location of value
    std::mutex mu_;                                     // protects file_ and index_
                                                        // so multiple client
                                                        // threads can't corrupt them
};

#endif // KVSTORE_H
