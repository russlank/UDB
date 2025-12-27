# UDB Library Architecture

This document provides a high-level overview of the UDB library's architecture, design decisions, and implementation details.

## Table of Contents

1. [Component Overview](#component-overview)
2. [Class Hierarchy](#class-hierarchy)
3. [File Organization](#file-organization)
4. [Design Principles](#design-principles)
5. [Memory Management](#memory-management)
6. [Thread Safety Model](#thread-safety-model)
7. [Error Handling Strategy](#error-handling-strategy)
8. [File Format Design](#file-format-design)
9. [Performance Characteristics](#performance-characteristics)
10. [Extension Points](#extension-points)
11. [Testing Strategy](#testing-strategy)
12. [Migration from Legacy Code](#migration-from-legacy-code)

---

## Component Overview

The UDB library is organized into distinct layers, each with specific responsibilities:

```
┌─────────────────────────────────────────────────────────────────────┐
│                         Application Layer                           │
│                     (dbe.cpp - Test Application)                    │
└──────────────────────────────┬──────────────────────────────────────┘
                               │
                               ▼
┌─────────────────────────────────────────────────────────────────────┐
│                             udb.h                                   │
│                 (Main include file, Version info)                   │
│         Convenience header that includes all other headers          │
└──────────────────────────────┬──────────────────────────────────────┘
                               │
         ┌─────────────────────┼─────────────────────┐
         │                     │                     │
         ▼                     ▼                     ▼
┌─────────────────┐  ┌─────────────────┐  ┌─────────────────────────┐
│  udb_btree.h    │  │  udb_heap.h     │  │     udb_common.h        │
│  udb_btree.cpp  │  │  udb_heap.cpp   │  │                         │
│                 │  │                 │  │  • Error codes          │
│ • MultiIndex    │  │ • HeapFile      │  │  • Key types            │
│ • IndexStack    │  │ • HolesTable    │  │  • Index attributes     │
│ • B-Tree ops    │  │ • Space mgmt    │  │  • Checksum utility     │
│ • Navigation    │  │ • Allocation    │  │  • Exception classes    │
└────────┬────────┘  └────────┬────────┘  └─────────────────────────┘
         │                    │
         └──────────┬─────────┘
                    │
                    ▼
        ┌───────────────────────────┐
        │       udb_file.h          │
        │       udb_file.cpp        │
        │                           │
        │  • File I/O abstraction   │
        │  • Read/Write/Seek        │
        │  • Position tracking      │
        │  • Thread safety (mutex)  │
        └───────────────────────────┘
```

### Component Responsibilities

| Component | Responsibility | Key Classes |
|-----------|---------------|-------------|
| `udb_common.h` | Shared definitions, utilities | ErrorCode, KeyType, exceptions |
| `udb_file.h/cpp` | Low-level file I/O | File |
| `udb_btree.h/cpp` | B-Tree indexing | MultiIndex, IndexStack |
| `udb_heap.h/cpp` | Record storage | HeapFile |
| `udb.h` | Public API | Version info |
| `dbe.cpp` | Test application | Interactive CLI |

---

## Class Hierarchy

```
udb::File (base class - file I/O abstraction)
│
├── udb::MultiIndex (B-Tree index file management)
│   └── Manages: B-Tree nodes, leaves, multiple indexes
│
└── udb::HeapFile (Variable-length record storage)
    └── Manages: Holes tables, space allocation
```

### Why This Hierarchy?

Both `MultiIndex` and `HeapFile` need file I/O capabilities:
- Read/write at specific positions
- Seek to arbitrary locations
- Track file size
- Handle errors consistently

By inheriting from `File`, they share this functionality without code duplication.

---

## File Organization

```
dbe/
├── dbe/                          # Source files
│   ├── udb.h                     # Main include (includes all others)
│   ├── udb_common.h              # Types, constants, utilities
│   ├── udb_file.h                # File class declaration
│   ├── udb_file.cpp              # File class implementation
│   ├── udb_heap.h                # HeapFile declaration
│   ├── udb_heap.cpp              # HeapFile implementation
│   ├── udb_btree.h               # MultiIndex declaration
│   ├── udb_btree.cpp             # MultiIndex implementation
│   └── dbe.cpp                   # Test application
├── doc/                          # Documentation
│   ├── architecture.md           # This file
│   ├── btree.md                  # B-Tree algorithm details
│   └── heap-file.md              # Heap file algorithm details
├── dbe.sln                       # Visual Studio solution
├── dbe.vcxproj                   # Project file
└── README.md                     # Getting started
```

### Include Dependencies

```
udb.h
├── udb_common.h (no dependencies)
├── udb_file.h
│   └── udb_common.h
├── udb_heap.h
│   └── udb_file.h
└── udb_btree.h
    └── udb_file.h
```

---

## Design Principles

### 1. Separation of Concerns

Each component has a single, well-defined responsibility:

- **File**: Raw file I/O operations
- **MultiIndex**: B-Tree indexing logic (key-to-position mapping)
- **HeapFile**: Space management for variable-length records
- **udb_common.h**: Shared definitions (no logic)

### 2. RAII (Resource Acquisition Is Initialization)

All resources are managed automatically:

```cpp
{
    MultiIndex index("data.ndx", 1);  // File opened
    // ... use index ...
}  // File automatically closed, data flushed
```

Benefits:
- No resource leaks (even if exceptions thrown)
- Clear ownership semantics
- No manual cleanup needed

### 3. Exception Safety

Operations provide basic exception safety:
- No resources leaked on exception
- Objects remain in valid (possibly modified) state

```cpp
try {
    MultiIndex index("missing.ndx");  // Opens existing
} catch (const FileIOException& e) {
    std::cerr << "Cannot open: " << e.what() << std::endl;
}
```

### 4. Modern C++ Idioms

| Pattern | Usage |
|---------|-------|
| `std::vector` | Dynamic arrays (nodes, tables) |
| `std::optional` | Nullable return values |
| `std::unique_ptr` | Owning pointers |
| Enum classes | Type-safe enumerations |
| Range-based for | Iteration |
| `const` correctness | Immutability where appropriate |

### 5. Explicit over Implicit

- No implicit conversions between unrelated types
- Explicit constructor calls where appropriate
- Clear ownership semantics (no raw owning pointers)

---

## Memory Management

### Strategy: Prefer Stack, Use Heap via std::vector

Original code (DOS):
```cpp
void* node = farmalloc(size);
// ... use node ...
farfree(node);  // Easy to forget!
```

Modern code:
```cpp
std::vector<uint8_t> node(size);
// Automatically freed when out of scope
```

### Memory Layout

B-Tree nodes and heap structures are stored as raw byte arrays (`std::vector<uint8_t>`) because:
1. They have fixed layouts defined by packed structs
2. They're read/written directly to/from files
3. `#pragma pack(push, 1)` ensures consistent memory layout

### No Manual Memory Management

The library uses no `new`/`delete` directly. All dynamic memory is managed through:
- `std::vector` for arrays
- `std::string` for text
- `std::fstream` for file handles
- `std::unique_ptr` for owned objects

---

## Thread Safety Model

### Current Implementation

Basic thread safety through coarse-grained locking:

```cpp
class File {
private:
    mutable std::mutex m_mutex;
    
public:
    void write(const void* buffer, size_t size, int64_t pos) {
        std::lock_guard<std::mutex> lock(m_mutex);
        // ... write operation ...
    }
};
```

### What This Provides

- **Single operations are atomic**: No data races on the file handle
- **Simple mental model**: One operation at a time

### What This Does NOT Provide

- **High concurrency**: All operations serialize
- **Compound operation atomicity**: Read-modify-write is not atomic

Example of unsafe pattern:
```cpp
// Thread 1                    // Thread 2
auto value = index.find("A");  auto value = index.find("B");
value++;                       value++;
index.delete("A");             index.delete("B");  // Race!
```

### For High Concurrency (Future Work)

Options to consider:
- Fine-grained page-level locking
- Reader-writer locks (multiple readers, single writer)
- Lock-free data structures
- MVCC (Multi-Version Concurrency Control)

---

## Error Handling Strategy

### Dual Approach: Codes + Exceptions

The library supports both error codes and exceptions:

| Mechanism | Use Case | Example |
|-----------|----------|---------|
| Error codes | Recoverable conditions | `if (index.hasError())` |
| Exceptions | Critical failures | `throw FileIOException(...)` |

### When to Use Each

| Scenario | Approach |
|----------|----------|
| File I/O failure | Exception |
| Data corruption | Exception |
| Key not found | Return value (-1) |
| Index empty | Return value + EOF flag |
| Invalid parameters | Exception or error code |

### Exception Hierarchy

```
std::runtime_error
└── UDBException
    ├── FileIOException (file operations failed)
    ├── DataCorruptionException (checksum mismatch)
    └── MemoryException (allocation failed)
```

### Best Practice

```cpp
try {
    MultiIndex index("data.ndx", 1);
    index.setActiveIndex(1);
    index.initIndex(...);
    
    while (!done) {
        index.append(key, pos);
        if (index.hasError()) {
            // Handle recoverable error
            index.clearError();
        }
    }
} catch (const DataCorruptionException& e) {
    // Critical: file corrupted, need recovery
} catch (const FileIOException& e) {
    // I/O error: check disk, permissions
}
```

---

## File Format Design

### Multi-Index File Format

```
Offset    Size    Description
──────────────────────────────────────────
0         1       Header checksum
1         2       Number of indexes
3         ~60     Index Info [0]
~63       ~60     Index Info [1]
...
[var]     [var]   Nodes and leaves (dynamically allocated)
```

### Checksums

Every structure includes a checksum:
- Calculated as XOR of all bytes (including checksum field = 0)
- Stored in first byte
- Verified on read: recalculate and check for 0

### Packed Structures

`#pragma pack(push, 1)` ensures:
- No padding between fields
- Consistent size across compilers
- Direct file I/O without conversion

### Endianness

Current implementation assumes **little-endian** (x86/x64).

For cross-platform:
```cpp
// Future: byte-swapping functions
int64_t toLittleEndian(int64_t value);
int64_t fromLittleEndian(int64_t value);
```

### Versioning

**Current limitation**: No version number in file format.

Recommended addition:
```cpp
struct FileHeader {
    uint8_t magic[4];     // "UDB\0"
    uint8_t version;      // Format version
    uint8_t checksum;
    uint16_t numIndexes;
};
```

---

## Performance Characteristics

### Time Complexity

| Operation | MultiIndex | HeapFile |
|-----------|-----------|----------|
| Find | O(log n) | N/A |
| Insert | O(log n) | O(h)* |
| Delete | O(log n) | O(h)* |
| Sequential Next | O(1) | N/A |
| Get File Size | O(1) | O(1) |

*h = number of holes to search

### Space Complexity

| Component | Space |
|-----------|-------|
| Index file | O(n) keys × (keySize + pointers) |
| Heap file | O(data) + O(fragmentation) |
| In-memory | O(maxItems × keySize) per node |

### I/O Patterns

- **B-Tree search**: O(log n) random reads
- **Sequential scan**: O(n) sequential reads (via leaf chain)
- **Insert**: O(log n) reads + O(log n) writes (worst case with splits)

### Optimization Opportunities

1. **Node caching**: Keep recently-used nodes in memory
2. **Batch operations**: Commit multiple changes at once
3. **Write buffering**: Delay writes, batch to disk
4. **Compression**: Reduce I/O at cost of CPU

---

## Extension Points

### Custom Key Comparison

Override `compare()` in a derived class:

```cpp
class CaseInsensitiveIndex : public MultiIndex {
public:
    int compare(const void* key1, const void* key2) const override {
        return strcasecmp(
            static_cast<const char*>(key1),
            static_cast<const char*>(key2)
        );
    }
};
```

### Custom Storage Backend

Inherit from `File` for different storage:

```cpp
class MemoryMappedFile : public File {
    // Memory-mapped implementation
};

class EncryptedFile : public File {
    // Encrypted storage
};

class NetworkFile : public File {
    // Remote storage
};
```

### Custom Data Types

Add new KeyType values and implement comparison in `compare()`.

---

## Testing Strategy

### Unit Tests (Recommended)

Test each component in isolation:

```cpp
// Test checksum calculation
void testChecksum() {
    uint8_t data[] = {0x01, 0x02, 0x03};
    assert(calculateBlockChecksum(data, 3) == 0x00);  // 1^2^3=0
}

// Test file operations
void testFileOperations() {
    File file("test.bin", true);
    int32_t value = 12345;
    file.write(&value, sizeof(value), 0);
    
    int32_t readBack;
    file.read(&readBack, sizeof(readBack), 0);
    assert(readBack == value);
}
```

### Integration Tests

Test component interactions:

```cpp
void testIndexWithHeap() {
    MultiIndex index("test.ndx", 1);
    HeapFile heap("test.heap", 100);
    
    // Initialize index
    index.setActiveIndex(1);
    index.initIndex(KeyType::STRING, 50, ...);
    
    // Add records
    for (int i = 0; i < 1000; i++) {
        std::string key = "key" + std::to_string(i);
        int64_t pos = heap.allocateSpace(sizeof(Record));
        heap.write(&record, sizeof(record), pos);
        index.append(key.c_str(), pos);
    }
    
    // Verify all can be found
    for (int i = 0; i < 1000; i++) {
        std::string key = "key" + std::to_string(i);
        assert(index.find(key.c_str()) >= 0);
    }
}
```

### Stress Tests

Test limits and edge cases:

```cpp
void testLargeIndex() {
    MultiIndex index("stress.ndx", 1);
    index.initIndex(KeyType::STRING, 50, ...);
    
    // Insert 1 million keys
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 1000000; i++) {
        index.append(std::to_string(i).c_str(), i);
    }
    auto end = std::chrono::high_resolution_clock::now();
    
    // Report performance
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    std::cout << "Inserted 1M keys in " << ms.count() << " ms\n";
}
```

---

## Migration from Legacy Code

### Original vs Modern

| Aspect | Original (DOS) | Modern (C++20) |
|--------|----------------|----------------|
| Memory | `farmalloc`/`farfree` | `std::vector`, RAII |
| Files | DOS handles (`_open`) | `std::fstream` |
| Types | `int`, `long`, `huge` | `int64_t`, `uint16_t` |
| Errors | Global `errno` | Exceptions + codes |
| Strings | `char[]`, manual | `std::string` |
| Threading | None | `std::mutex` |

### Key Changes Made

1. **Removed segmented memory**: DOS "far" and "huge" pointers → flat pointers
2. **Removed DOS-specific I/O**: `_open`, `_read`, `_lseek` → `std::fstream`
3. **Added type safety**: `enum class` instead of `#define`
4. **Added RAII**: Objects manage their own resources
5. **Added thread safety**: Basic mutex protection
6. **Added exceptions**: For critical errors
7. **Modernized syntax**: `auto`, range-for, nullptr, etc.

### Preserved

- File format (binary compatible)
- B-Tree algorithms
- Holes table structure
- Checksum algorithm

---

*This document is part of the UDB library documentation.*
