<div align="center">

# bitcask-kv

### A small persistent key-value store written from scratch in C++

An educational append-only storage engine (the **Bitcask** model) with recovery,
log compaction, a multithreaded TCP server, and a simple benchmark.

</div>

---

## What it is

`bitcask-kv` is a learning project that demonstrates how a basic persistent key-value
store can be built without a database library or framework.

Every write is appended to a log, while an in-memory hash index maps each key to the
offset and size of its latest value. `GET` uses the index to locate the value directly.
Deletes are represented by tombstone records.

```text
client ──TCP──▶ server ──▶ KVStore
                         │
                  in-memory index
                         │
                    append-only log
```

## Features

- Append-only binary log
- In-memory `unordered_map` index
- `SET`, `GET`, and `DEL`
- Startup recovery by replaying the log
- Protection against oversized/corrupt record lengths during recovery
- Truncated final-record recovery after an interrupted write
- Manual log compaction
- Multithreaded TCP server using `std::thread`
- Mutex-protected shared storage engine
- Basic request validation and a 1 MB network request limit
- Simple correctness test suite
- Benchmark for writes, reads, and recovery

## On-disk format

```text
[ key_size : 4 bytes ][ value_size : 4 bytes ][ key bytes ][ value bytes ]
```

`value_size = 0xFFFFFFFF` represents a tombstone, so no value bytes follow a delete
record. Keys and values have practical size limits to keep recovery and requests bounded.

## Build and run

Requires a C++17 compiler.

```bash
# Local interactive store
g++ -std=c++17 -O2 main.cpp kvstore.cpp -o kvstore
./kvstore

# Network server
# Windows: add -lws2_32 at the end of the command
g++ -std=c++17 -O2 -pthread server.cpp kvstore.cpp -o server
./server

# Python client (in another terminal)
python client.py

# Correctness tests
g++ -std=c++17 -O2 test_kvstore.cpp kvstore.cpp -o test_kvstore
./test_kvstore

# Benchmark
g++ -std=c++17 -O2 bench.cpp kvstore.cpp -o bench
./bench 50000
```

## Example

```text
> set city Ludhiana
OK
> get city
Ludhiana
> set city Kolkata
OK
> get city
Kolkata
> del city
OK
> get city
(nil)
> compact
compacted: ... bytes -> ... bytes
```

## Project structure

```text
bitcask-kv/
├── kvstore.h          # KVStore interface and index definition
├── kvstore.cpp        # storage engine, recovery, and compaction
├── main.cpp           # local interactive REPL
├── server.cpp         # multithreaded TCP server
├── client.py          # simple network client
├── test_kvstore.cpp   # correctness tests
└── bench.cpp          # benchmark harness
```

## Limitations

This is intentionally a **fresher-level systems project**, not a production database.

- Entire key index stays in memory.
- Single node and single data file.
- Manual compaction.
- No replication, authentication, encryption, or clustering.
- Default persistence uses `flush()` rather than a power-loss durability guarantee.
- The simple text protocol is intended for learning and local experimentation.

## What the project demonstrates

The main learning goals are storage-engine fundamentals, file I/O, binary record formats,
hash indexing, recovery, compaction, socket programming, and basic multithreading.

Future work can include automatic compaction, a read/write lock, TTLs, or an LSM-tree,
but they are intentionally outside the scope of this small project.
