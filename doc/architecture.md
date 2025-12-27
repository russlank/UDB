# UDB Library Architecture

This document provides a high-level overview of the UDB library's architecture and design decisions.

## Component Overview

```
┌─────────────────────────────────────────────────────────────────┐
│                        Application Layer                         │
│                    (test_main.cpp, your code)                    │
└────────────────────────────────┬────────────────────────────────┘
                                 │
                                 ▼
┌─────────────────────────────────────────────────────────────────┐
│                          udb.h                                   │
│                   (Main include, Version info)                   │
└────────────────────────────────┬────────────────────────────────┘
                                 │
           ┌─────────────────────┼─────────────────────┐
           │                     │                     │
           ▼                     ▼                     ▼
┌───────────────────┐ ┌───────────────────┐ ┌───────────────────┐
│   udb_btree.h     │ │   udb_heap.h      │ │  udb_common.h     │
│   udb_btree.cpp   │ │   udb_heap.cpp    │ │                   │
│                   │ │                   │ │ • Error codes     │
│ • MultiIndex      │ │ • HeapFile        │ │ • Key types       │
│ • B-Tree ops      │ │ • Space mgmt      │ │ • Utilities       │
│ • Navigation      │ │ • Holes tables    │ │ • Exceptions      │
└─────────┬─────────┘ └─────────┬─────────┘ └───────────────────┘
          │                     │
          └──────────┬──────────┘
                     │
                     ▼
        ┌─────────────────────────┐
        │      udb_file.h         │
        │      udb_file.cpp       │
        │                         │
        │  • File I/O abstraction │
        │  • Read/Write/Seek      │
        │  • Thread safety        │
        └─────────────────────────┘
```

## Class Hierarchy

```
udb::File (base class)
├── udb::MultiIndex (B-Tree index file)
└── udb::HeapFile (Variable-length record storage)
```

## Design Principles

### 1. Separation of Concerns

Each component has a single responsibility:
- `File`: Raw file I/O operations
- `MultiIndex`: B-Tree indexing logic
- `HeapFile`: Space management for records
- `udb_common.h`: Shared definitions

### 2. RAII (Resource Acquisition Is Initialization)

All resources are managed automatically:
```cpp
{
    MultiIndex index("data.ndx", 1);  // File opened
    // ... use index ...
}  // File automatically closed
```

### 3. Exception Safety

Operations that can fail throw exceptions:
```cpp
try {
    MultiIndex index("missing.ndx");  // Opens existing
} catch (const FileIOException& e) {
    std::cerr << "Cannot open: " << e.what() << std::endl;
}
```

### 4. Modern C++ Idioms

- `std::vector` for dynamic arrays
- `std::optional` for nullable values  
- `std::unique_ptr` for ownership
- Range-based for loops
- Enum classes for type safety

## Memory Layout

### Stack vs Heap Allocation

```cpp
// Stack allocation (preferred for small objects)
std::vector<uint8_t> node = allocateNodeBlock();

// Automatic cleanup when out of scope
```

### Avoiding Raw Pointers

Original code:
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

## Thread Safety Model

### Current Implementation

Basic thread safety through mutex:
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

### Limitations

- Coarse-grained locking (entire file)
- Not suitable for high concurrency
- No reader-writer optimization

### Future Improvements

For high-performance concurrent access:
- Fine-grained page-level locking
- Read-write locks
- Lock-free data structures

## Error Handling Strategy

### Error Codes (Legacy Support)

```cpp
ErrorCode code = index.getError();
if (code != ErrorCode::OK) {
    // Handle error
}
```

### Exceptions (Preferred)

```cpp
try {
    index.append("key", pos);
} catch (const DataCorruptionException& e) {
    // Handle corruption
}
```

### When to Use Which

| Scenario | Approach |
|----------|----------|
| Critical errors (file I/O) | Exception |
| Recoverable conditions | Error code |
| Data corruption | Exception |
| Not found | Return value (-1) |

## File Format Compatibility

### Versioning

The file format doesn't include a version number (inherited from original).
Consider adding:
```cpp
struct FileHeader {
    uint8_t checksum;
    uint8_t version;      // Add version
    uint16_t numIndexes;
};
```

### Endianness

Current implementation assumes little-endian (x86/x64).
For cross-platform compatibility:
```cpp
int64_t toLittleEndian(int64_t value);
int64_t fromLittleEndian(int64_t value);
```

## Performance Characteristics

### Time Complexity

| Operation | MultiIndex | HeapFile |
|-----------|-----------|----------|
| Find | O(log n) | N/A |
| Insert | O(log n) | O(m)* |
| Delete | O(log n) | O(m)* |
| Sequential | O(1) per item | N/A |

*m = number of holes to search

### Space Complexity

| Component | Space |
|-----------|-------|
| Index file | O(n) where n = number of keys |
| Heap file | O(n + h) where h = fragmentation |
| In-memory | O(k) where k = max node/table size |

## Extension Points

### Custom Key Comparison

```cpp
class CustomIndex : public MultiIndex {
public:
    int compare(const void* key1, const void* key2) const override {
        // Custom comparison logic
    }
};
```

### Custom Storage

Inherit from `File` to implement different storage backends:
- Memory-mapped files
- Network storage
- Encrypted storage

## Testing Strategy

### Unit Tests (Recommended)

Test each component independently:
- File I/O operations
- Checksum calculations
- Node manipulation
- Tree operations

### Integration Tests

Test component interactions:
- Create, populate, search, delete sequence
- Recovery from errors
- Concurrent access patterns

### Stress Tests

Test limits:
- Large numbers of keys
- Very large keys
- Rapid insert/delete cycles

---

*This document is part of the UDB library documentation.*
