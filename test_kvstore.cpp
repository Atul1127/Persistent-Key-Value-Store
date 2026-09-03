#include "kvstore.h"
#include <cassert>
#include <cstdio>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>

static void clean(const char* path) {
    std::remove(path);
    std::string tmp = std::string(path) + ".compact";
    std::remove(tmp.c_str());
}

static void writeUint32(std::ofstream& out, uint32_t value) {
    char buf[4];
    buf[0] = static_cast<char>(value & 0xFF);
    buf[1] = static_cast<char>((value >> 8) & 0xFF);
    buf[2] = static_cast<char>((value >> 16) & 0xFF);
    buf[3] = static_cast<char>((value >> 24) & 0xFF);
    out.write(buf, 4);
}

int main() {
    const char* path = "test_kvstore.db";
    clean(path);

    // Basic set/get and overwrite.
    {
        KVStore store(path);
        std::string out;
        assert(!store.get("missing", out));
        store.set("name", "Atul");
        assert(store.get("name", out) && out == "Atul");
        store.set("name", "KV");
        assert(store.get("name", out) && out == "KV");

        // Delete removes the key from the live index.
        store.del("name");
        assert(!store.get("name", out));

        // Empty values are valid.
        store.set("empty", "");
        assert(store.get("empty", out) && out.empty());
    }

    // Recovery rebuilds the index from disk.
    {
        KVStore store(path);
        std::string out;
        assert(!store.get("name", out));
        assert(store.get("empty", out) && out.empty());
        store.set("a", "1");
        store.set("b", "2");
    }

    // A truncated final record is discarded during recovery.
    uint64_t valid_size;
    {
        KVStore store(path);
        valid_size = store.fileSize();
    }
    {
        std::ofstream out(path, std::ios::binary | std::ios::app);
        assert(out);
        writeUint32(out, 3);     // key size
        writeUint32(out, 100);   // value size, but only part of it is written
        out.write("bad", 3);
        out.write("partial", 7);
    }
    {
        KVStore store(path);
        std::string out;
        assert(store.get("a", out) && out == "1");
        assert(store.get("b", out) && out == "2");
        assert(!store.get("bad", out));
        assert(store.fileSize() == valid_size);
    }

    // Compaction preserves live data and removes obsolete records.
    uint64_t before;
    {
        KVStore store(path);
        store.set("a", "new-value");
        store.del("b");
        before = store.fileSize();
        store.compact();
        assert(store.fileSize() < before);

        std::string out;
        assert(store.get("a", out) && out == "new-value");
        assert(store.get("empty", out) && out.empty());
        assert(!store.get("b", out));
    }

    // Compacted file is recoverable too.
    {
        KVStore store(path);
        std::string out;
        assert(store.get("a", out) && out == "new-value");
        assert(store.get("empty", out) && out.empty());
        assert(!store.get("b", out));
    }

    clean(path);
    std::cout << "All KVStore tests passed.\n";
    return 0;
}
