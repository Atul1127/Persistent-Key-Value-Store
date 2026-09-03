<div align="center">

# bitcask-kv

### A small persistent key-value store written from scratch in C++

An educational append-only storage engine based on the **Bitcask** model, with startup
recovery, log compaction, and a multithreaded TCP server — built without a database
library or framework.

</div>

---

## What it is

`bitcask-kv` is a small key-value database built to understand how storage engines work.
Every write is appended to a log file, while an in-memory hash index maps each key to the
offset and size of its latest value. Reads use the index to jump directly to the value.

```text
   client ──TCP──▶ server ──▶ KVStore
                                 │
              in-memory index:  key ──▶ (file offset, size)
                                 │
              on-disk log:  [..][key3=v][key1=v'][key2=TOMBSTONE][key1=v'']
                              (append-only — newest record for a key wins)
```

## Features

- Append-only log with length-prefixed records
- In-memory `unordered_map` index
- Persistent data across normal restarts
- Recovery that rebuilds the index from the log
- Safe handling of a partially written final record
- Key/value size limits and write-error checking
- Tombstones for deletes
- Manual log compaction
- Multithreaded TCP server
- Simple Python client
- Local interactive CLI
- Basic write/read/recovery benchmark

## Benchmarks

Example results from a Windows machine (MinGW g++ 15.2, `-O2`, 50,000 keys):

| Operation | Throughput | Latency |
|---|---:|---:|
| Write | ~99,000 ops/sec | ~10 µs/op |
| Read | ~241,000 ops/sec | ~4 µs/op |
| Recovery | 50k keys in ~234 ms | — |

These numbers are machine-dependent and are included as an example rather than a
production performance claim. Run the benchmark yourself with different dataset sizes.

```bash
./bench 200000
```

## How it works

### 1. Append-only storage

A `SET` never overwrites an existing value. It appends:

```text
[ key_size : 4 bytes ][ value_size : 4 bytes ][ key ][ value ]
```

The index stores:

```text
key -> (value offset, value size)
```

### 2. Recovery

When the store starts, it scans the log from the beginning and reconstructs the index.
The newest record for a key wins. If the last record is incomplete, the incomplete suffix
is discarded and the valid prefix is retained.

### 3. Delete

`DEL` appends a record whose `value_size` is `0xFFFFFFFF`. This is a tombstone, so the
key is removed from the live in-memory index.

### 4. Compaction

Repeated updates leave old records behind. `COMPACT` copies only currently live values
to a new file and replaces the old file, reducing wasted space.

### 5. Networking

The TCP server accepts commands such as:

```text
SET city Ludhiana
GET city
DEL city
COMPACT
```

Each client is handled by a separate thread, while the KVStore mutex protects the shared
file and index.

## Build & run

Requires a C++17 compiler (g++/MinGW on Windows, g++/clang on Linux/Mac).

```bash
# Local interactive store
g++ -std=c++17 -O2 main.cpp kvstore.cpp -o kvstore
./kvstore

# Network server
# Windows: add -lws2_32 at the end
g++ -std=c++17 -O2 -pthread server.cpp kvstore.cpp -o server
./server

# Python client (another terminal)
python client.py

# Benchmark
g++ -std=c++17 -O2 bench.cpp kvstore.cpp -o bench
./bench 200000
```

The development server listens on `127.0.0.1:6380`, so it is intended for local use.

## Project structure

```text
bitcask-kv/
├── kvstore.h      # KVStore interface and in-memory index
├── kvstore.cpp    # storage engine implementation
├── main.cpp       # local interactive REPL
├── server.cpp     # multithreaded TCP server
├── client.py      # Python TCP client
└── bench.cpp      # benchmark harness
```

## Limitations

This is a **fresher/learning project**, not a production database.

- Entire key index lives in memory
- Single node and single active data file
- Manual compaction
- Simple text protocol
- No authentication or encryption
- No replication or sharding
- No automated test suite yet
- Durability is limited by normal file flushing rather than an explicit `fsync` policy

These limitations are intentional so the project stays small enough to understand while
still demonstrating storage, persistence, recovery, concurrency, networking, and
performance concepts.

## Possible future improvements

- Add unit/integration tests
- Add automatic background compaction
- Add a read/write lock for higher read concurrency
- Add checksums for stronger corruption detection
- Add a more structured network protocol
- Explore an LSM-tree/SSTable design as a separate advanced stage

---

<div align="center">
Built from scratch: append-only storage → recovery → compaction → concurrent TCP server.
</div>
