#include "kvstore.h"
#include <ios>
#include <filesystem>
#include <fstream>
#include <stdexcept>

static const uint32_t TOMBSTONE = 0xFFFFFFFF;
static const uint32_t MAX_KEY_SIZE = 1024 * 1024;
static const uint32_t MAX_VALUE_SIZE = 64 * 1024 * 1024;

static void writeUint32(std::ostream& os, uint32_t v) {
    char buf[4];
    buf[0] = static_cast<char>(v & 0xFF);
    buf[1] = static_cast<char>((v >> 8) & 0xFF);
    buf[2] = static_cast<char>((v >> 16) & 0xFF);
    buf[3] = static_cast<char>((v >> 24) & 0xFF);
    os.write(buf, 4);
}

static bool readUint32(std::istream& is, uint32_t& v) {
    char buf[4];
    if (!is.read(buf, 4)) return false;
    v = static_cast<uint32_t>(static_cast<unsigned char>(buf[0])) |
        (static_cast<uint32_t>(static_cast<unsigned char>(buf[1])) << 8) |
        (static_cast<uint32_t>(static_cast<unsigned char>(buf[2])) << 16) |
        (static_cast<uint32_t>(static_cast<unsigned char>(buf[3])) << 24);
    return true;
}

KVStore::KVStore(const std::string& path) : path_(path) {
    file_.open(path_, std::ios::in | std::ios::out | std::ios::binary);
    if (!file_.is_open()) {
        std::ofstream create(path_, std::ios::binary);
        if (!create) throw std::runtime_error("cannot create data file");
        create.close();
        file_.open(path_, std::ios::in | std::ios::out | std::ios::binary);
    }
    if (!file_.is_open()) throw std::runtime_error("cannot open data file");
    recover();
}

void KVStore::recover() {
    file_.clear();
    file_.seekg(0, std::ios::beg);
    std::streamoff last_valid_end = 0;

    while (true) {
        const std::streamoff record_start = file_.tellg();
        if (record_start < 0) break;

        uint32_t key_size, value_size;
        if (!readUint32(file_, key_size)) break;
        if (!readUint32(file_, value_size)) break;
        if (key_size > MAX_KEY_SIZE ||
            (value_size != TOMBSTONE && value_size > MAX_VALUE_SIZE)) break;

        std::string key(key_size, '\0');
        if (key_size > 0 && !file_.read(key.data(), key_size)) break;

        if (value_size == TOMBSTONE) {
            index_.erase(key);
            last_valid_end = file_.tellg();
        } else {
            const std::streamoff value_offset = file_.tellg();
            if (value_offset < 0) break;
            file_.seekg(static_cast<std::streamoff>(value_size), std::ios::cur);
            if (!file_) break;
            index_[key] = IndexEntry{static_cast<uint64_t>(value_offset), value_size};
            last_valid_end = file_.tellg();
        }
    }

    // A crash can leave a partial final record. Keep only the valid prefix.
    file_.clear();
    file_.close();
    std::error_code ec;
    std::filesystem::resize_file(path_, static_cast<uintmax_t>(last_valid_end), ec);
    if (ec) throw std::runtime_error("cannot repair data file: " + ec.message());
    file_.open(path_, std::ios::in | std::ios::out | std::ios::binary);
    if (!file_.is_open()) throw std::runtime_error("cannot reopen data file");
}

KVStore::~KVStore() {
    if (file_.is_open()) file_.close();
}

void KVStore::set(const std::string& key, const std::string& value) {
    if (key.size() > MAX_KEY_SIZE || value.size() > MAX_VALUE_SIZE)
        throw std::invalid_argument("key or value too large");

    std::lock_guard<std::mutex> lock(mu_);
    file_.clear();
    file_.seekp(0, std::ios::end);
    writeUint32(file_, static_cast<uint32_t>(key.size()));
    writeUint32(file_, static_cast<uint32_t>(value.size()));
    file_.write(key.data(), static_cast<std::streamsize>(key.size()));
    const uint64_t value_offset = static_cast<uint64_t>(file_.tellp());
    file_.write(value.data(), static_cast<std::streamsize>(value.size()));
    if (!file_) throw std::runtime_error("write failed");
    file_.flush();
    if (!file_) throw std::runtime_error("flush failed");
    index_[key] = IndexEntry{value_offset, static_cast<uint32_t>(value.size())};
}

bool KVStore::get(const std::string& key, std::string& out) {
    std::lock_guard<std::mutex> lock(mu_);
    auto it = index_.find(key);
    if (it == index_.end()) return false;
    const IndexEntry& e = it->second;
    file_.clear();
    file_.seekg(static_cast<std::streamoff>(e.value_offset), std::ios::beg);
    out.resize(e.value_size);
    if (e.value_size > 0 && !file_.read(out.data(), e.value_size)) {
        out.clear();
        return false;
    }
    return true;
}

void KVStore::del(const std::string& key) {
    if (key.size() > MAX_KEY_SIZE) throw std::invalid_argument("key too large");
    std::lock_guard<std::mutex> lock(mu_);
    file_.clear();
    file_.seekp(0, std::ios::end);
    writeUint32(file_, static_cast<uint32_t>(key.size()));
    writeUint32(file_, TOMBSTONE);
    file_.write(key.data(), static_cast<std::streamsize>(key.size()));
    if (!file_) throw std::runtime_error("delete write failed");
    file_.flush();
    if (!file_) throw std::runtime_error("delete flush failed");
    index_.erase(key);
}

uint64_t KVStore::fileSize() {
    std::lock_guard<std::mutex> lock(mu_);
    file_.clear();
    file_.seekg(0, std::ios::end);
    const auto pos = file_.tellg();
    return pos < 0 ? 0 : static_cast<uint64_t>(pos);
}

void KVStore::compact() {
    std::lock_guard<std::mutex> lock(mu_);
    namespace fs = std::filesystem;
    const std::string tmp_path = path_ + ".compact";
    std::unordered_map<std::string, IndexEntry> new_index;

    {
        std::ofstream out(tmp_path, std::ios::binary | std::ios::trunc);
        if (!out) throw std::runtime_error("cannot create compaction file");
        for (const auto& kv : index_) {
            const std::string& key = kv.first;
            const IndexEntry& e = kv.second;
            std::string value(e.value_size, '\0');
            file_.clear();
            file_.seekg(static_cast<std::streamoff>(e.value_offset), std::ios::beg);
            if (e.value_size > 0 && !file_.read(value.data(), e.value_size))
                throw std::runtime_error("compaction read failed");
            writeUint32(out, static_cast<uint32_t>(key.size()));
            writeUint32(out, e.value_size);
            out.write(key.data(), static_cast<std::streamsize>(key.size()));
            const uint64_t new_value_offset = static_cast<uint64_t>(out.tellp());
            out.write(value.data(), static_cast<std::streamsize>(value.size()));
            if (!out) throw std::runtime_error("compaction write failed");
            new_index[key] = IndexEntry{new_value_offset, e.value_size};
        }
        out.flush();
        if (!out) throw std::runtime_error("compaction flush failed");
        out.close();
    }

    file_.close();
    std::error_code ec;
    fs::rename(tmp_path, path_, ec);
    if (ec) {
        // Windows does not replace an existing destination with rename().
        std::error_code remove_ec;
        fs::remove(path_, remove_ec);
        if (remove_ec) {
            file_.open(path_, std::ios::in | std::ios::out | std::ios::binary);
            throw std::runtime_error("compaction replace failed: " + remove_ec.message());
        }
        fs::rename(tmp_path, path_, ec);
    }
    if (ec) {
        file_.open(path_, std::ios::in | std::ios::out | std::ios::binary);
        throw std::runtime_error("compaction rename failed: " + ec.message());
    }
    file_.open(path_, std::ios::in | std::ios::out | std::ios::binary);
    if (!file_.is_open()) throw std::runtime_error("cannot reopen compacted file");
    index_ = std::move(new_index);
}
