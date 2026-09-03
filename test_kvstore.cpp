#include "kvstore.h"
#include <cassert>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>

static void clean(const char* path) {
    std::remove(path);
    std::string tmp = std::string(path) + ".compact";
    std::remove(tmp.c_str());
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
