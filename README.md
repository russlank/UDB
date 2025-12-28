# UDB - Ultra Database Engine

A lightweight, high-performance B-Tree indexing engine with dynamic heap-structured record storage implementation.

## 📋 Overview

UDB (Ultra Database) is a modernized version of a classic database indexing library, originally developed for DOS/Windows 3.1 and now updated for Visual Studio 2022/2025 and modern C++20.

This project serves as both a practical database library and an **educational resource** for understanding:
- B-Tree and B+ Tree data structures
- File-based indexing algorithms
- Heap file storage management
- Space reclamation techniques

### Key Features

- **B-Tree Indexing**: Efficient key-based access with O(log n) search, insert, and delete operations
- **Multiple Indexes**: Support for multiple indexes per file with different key types
- **Heap File Storage**: Dynamic variable-length record storage with space reclamation
- **Data Integrity**: XOR checksums on all data structures for corruption detection
- **Multiple Key Types**: Strings, integers, blocks, characters, and logical values
- **Sequential Access**: Efficient cursor-based navigation (first, next, previous)
- **Thread Safety**: Recursive mutex protection for concurrent access

## 🧪 Build and Test Status

| Component | Status |
|-----------|--------|
| Build | ✅ Compiles successfully |
| Unit Tests | ✅ **45/45 tests passing (100%)** |
| File I/O | ✅ All tests passing |
| B-Tree Navigation | ✅ Working correctly |
| B-Tree Find/Insert/Delete | ✅ All operations working correctly |
| Thread Safety | ✅ Concurrent read/write tests passing |
| Persistence | ✅ Data persists correctly across file reopen |

### Recent Fixes (v2.1.0)

The following issues have been resolved:

1. **Binary Search Bug**: Fixed `binarySearchNode()` to correctly return position `numItems+1` when the search key is greater than all existing keys in a node. Previously, it incorrectly returned the last position, causing keys to be inserted at wrong positions during node splits.

2. **UNIQUE Attribute Check**: Fixed the UNIQUE constraint check in `append()` which incorrectly used bitwise OR (`|`) instead of AND (`&`), causing all indexes to behave as if they had the UNIQUE attribute.

3. **Node Split Logic**: The node split and balance operations now correctly maintain the B-tree invariants after the binary search fix.

## 🚀 Quick Start

### Building the Project

1. Open `UDBE.sln` in Visual Studio 2022/2025
2. Select your configuration (Debug/Release, x64)
3. Restore NuGet packages (right-click solution → "Restore NuGet Packages")
4. Build the solution (F7 or Build → Build Solution)

### Running Tests

The project uses Google Test for unit testing:

```bash
# Build and run tests
msbuild UDBE.sln /p:Configuration=Debug /p:Platform=x64
.\bin\x64\Debug\UDBETests.exe
```

Expected output:
```
[==========] 45 tests from 2 test cases ran.
[  PASSED  ] 45 tests.
```

### Basic Usage

```cpp
#include "udb.h"
using namespace udb;

// Create a new index file with one index
MultiIndex index("mydata.ndx", 1);

// Initialize the index for string keys
index.setActiveIndex(1);
index.initIndex(
    KeyType::STRING,              // Key type
    50,                           // Key size (bytes)
    IndexAttribute::ALLOW_DELETE, // Attributes
    5,                            // Keys per node (B-Tree order)
    100,                          // Pre-allocate nodes
    100                           // Pre-allocate leaves
);

// Add some records
index.append("Alice", 1000);   // Key "Alice", data position 1000
index.append("Bob", 2000);
index.append("Charlie", 3000);

// Find a record
int64_t pos = index.find("Bob");
if (pos >= 0) {
    std::cout << "Found Bob at position " << pos << std::endl;
}

// Iterate through all records
char key[51];
pos = index.getFirst(key);
while (pos >= 0 && !index.isEOF()) {
    std::cout << "Key: " << key << " Position: " << pos << std::endl;
    pos = index.getNext(key);
}
```

## 📁 Project Structure

```
UDB/
├── UDBE/                    # Main library project
│   ├── udb.h                # Main include file (include this in your projects)
│   ├── udb_common.h         # Common definitions, types, and utilities
│   ├── udb_file.h/cpp       # File I/O abstraction layer
│   ├── udb_heap.h/cpp       # Heap file management (space allocation)
│   ├── udb_btree.h/cpp      # B-Tree index implementation
│   └── udb_sync.h           # Thread synchronization primitives
├── UDBEApp/                 # Interactive test application
│   └── dbe.cpp              # Command-line test interface
├── UDBETests/               # Unit tests (Google Test)
│   ├── test_btree.cpp       # B-Tree tests (33 tests)
│   ├── test_file.cpp        # File I/O tests (12 tests)
│   ├── test_heap.cpp        # Heap file tests
│   └── test_main.cpp        # Test entry point
├── doc/                     # Documentation
│   ├── btree.md             # B-Tree algorithm explanation
│   ├── heap-file.md         # Heap file structure explanation
│   └── architecture.md      # Architecture overview
├── UDBE.sln                 # Visual Studio solution
├── README.md                # This file
└── TODO.md                  # Future development roadmap
```

### Source File Descriptions

| File | Purpose |
|------|---------|
| `udb.h` | Main header - includes all other headers, provides version info |
| `udb_common.h` | Error codes, key types, constants, utility functions, exceptions |
| `udb_file.h/cpp` | Platform-independent file I/O with thread safety |
| `udb_heap.h/cpp` | Heap file for variable-length record storage with holes management |
| `udb_btree.h/cpp` | B+ Tree index implementation with multi-index support |
| `udb_sync.h` | Thread synchronization primitives (mutexes, locks) |

## 🔧 Configuration Options

### Key Types

| Type | Description | Size | Comparison |
|------|-------------|------|------------|
| `STRING` | Null-terminated string | Variable (up to keySize) | strcmp |
| `INTEGER` | 16-bit signed integer | 2 bytes | Numeric |
| `LONG_INT` | 32-bit signed integer | 4 bytes | Numeric |
| `BLOCK` | Raw byte block | Variable | Byte-by-byte (MSB first) |
| `NUM_BLOCK` | Numeric block (big-endian) | Variable | Byte-by-byte (LSB first) |
| `CHARACTER` | Single character | 1 byte | ASCII |
| `LOGICAL` | Boolean value | 1 byte | false < true |

### Index Attributes

| Attribute | Description |
|-----------|-------------|
| `NONE` | No special attributes |
| `UNIQUE` | Keys must be unique (duplicates rejected) |
| `ALLOW_DELETE` | Enable key deletion with node rebalancing |

**Note**: `ALLOW_DELETE` affects tree structure. When enabled, nodes are balanced during insert/delete operations, resulting in more predictable performance but slightly higher overhead.

## 📚 API Reference

### MultiIndex Class

The main class for B-Tree index operations.

#### Construction

```cpp
// Create new file with N indexes
MultiIndex(const std::string& filename, uint16_t numIndexes);

// Open existing file
explicit MultiIndex(const std::string& filename);
```

#### Index Management

```cpp
void setActiveIndex(uint16_t indexNo);    // Set active index (1-based)
uint16_t getActiveIndex() const;          // Get active index number
uint16_t getNumIndexes() const;           // Get total number of indexes
void initIndex(KeyType keyType,           // Initialize an index
               uint16_t keySize,
               IndexAttribute attributes,
               uint16_t numItems,         // B-Tree order (keys per node)
               int64_t freeCreateNodes,   // Nodes to pre-allocate
               int64_t freeCreateLeaves); // Leaves to pre-allocate
```

#### Key Operations

```cpp
bool append(const void* key, int64_t dataPos);  // Add key-data pair
int64_t find(const void* key);                   // Find key, returns data position
bool deleteKey(const void* key);                 // Delete all entries with key
int64_t deleteCurrent();                         // Delete current entry
```

#### Navigation

```cpp
int64_t getFirst(void* key = nullptr);   // Move to first entry
int64_t getNext(void* key = nullptr);    // Move to next entry
int64_t getPrev(void* key = nullptr);    // Move to previous entry
int64_t getCurrent(void* key = nullptr); // Get current entry
bool isEOF() const;                       // At end of index?
bool isBOF() const;                       // At beginning of index?
```

#### Error Handling

```cpp
ErrorCode getError() const;   // Get current error code
bool hasError() const;        // Check if there's an error
void clearError();            // Clear error state
```

### HeapFile Class

For variable-length record storage with space reclamation.

```cpp
// Create new heap file
HeapFile(const std::string& filename, uint16_t holesTableSize);

// Open existing heap file
explicit HeapFile(const std::string& filename);

// Allocate space for a record
int64_t allocateSpace(size_t size);

// Free space occupied by a record
void freeSpace(int64_t position, size_t size);
```

## 🧪 Running the Test Application

The test application (`UDBEApp`) provides an interactive interface:

```
> C                # Create new index file
> A Hello          # Append key "Hello"
> A World          # Append key "World"
> F Hello          # Find key "Hello"
> L                # List all keys
> .                # Go to first key
> +                # Next key
> -                # Previous key
> T                # Show current key
> R                # Remove current
> D Hello          # Delete key "Hello"
> N 1000           # Fill with 1000 sequential keys
> S                # Show statistics
> C 2              # Switch to index 2
> X                # Exit
```

## 📖 Documentation

Detailed documentation is available in the `doc/` directory:

- **[B-Tree Algorithm](doc/btree.md)** - Comprehensive explanation of B-Tree/B+ Tree structure, operations (search, insert, delete), node splitting/merging, and performance characteristics
- **[Heap File Structure](doc/heap-file.md)** - How variable-length records are stored, holes tables, space allocation/deallocation strategies
- **[Architecture Overview](doc/architecture.md)** - Library design, class hierarchy, error handling, thread safety model

## 🎓 Educational Value

This project is designed to be educational. Key learning points:

### B-Tree Concepts
- Why B-Trees are optimal for disk-based storage
- Difference between B-Trees and B+ Trees
- Node splitting and merging algorithms
- How sequential access is optimized via leaf linking

### File Organization
- How to manage variable-length records
- Space reclamation using holes tables
- Checksum-based data integrity verification
- File structure design for efficient I/O

### Software Engineering
- Modern C++ practices (RAII, smart pointers, exceptions)
- Separation of concerns in class design
- How to migrate legacy code to modern standards
- Thread-safe design with recursive mutexes

## 🔄 Migration from Legacy Code

This is a modernized version of classic DOS/Windows 3.1 database code. Key changes:

| Aspect | Original (DOS) | Modern (C++20) |
|--------|----------------|----------------|
| Memory | `farmalloc`/`farfree` | `std::vector`, RAII |
| Files | DOS handles (`_open`, `_read`) | `std::fstream` |
| Types | `int`, `long` | `int64_t`, `uint16_t` |
| Errors | Global error codes | Exceptions + error codes |
| Threading | None | `std::recursive_mutex` protection |
| Headers | Custom header files | `#pragma once`, include guards |
| Tests | Manual testing | Google Test framework |

## ⚠️ Limitations

1. **Not for production use**: This is an educational/reference implementation
2. **No transaction support**: No ACID guarantees, no crash recovery
3. **Basic thread safety**: Coarse-grained locking, not optimized for high concurrency
4. **No compaction**: Heap file fragmentation accumulates over time
5. **Fixed endianness**: Assumes little-endian (x86/x64)

## 🛠️ Completed Features

- [x] Unit test suite (Google Test) - 45 tests
- [x] Thread safety (recursive mutexes)
- [x] B-tree find() after node splits - **FIXED**
- [x] UNIQUE attribute constraint - **FIXED**
- [x] Node split and balance operations - **FIXED**

## 📜 License

This project is provided as-is for educational and reference purposes.

## 🤝 Contributing

Contributions are welcome! Please ensure:
- Code follows the existing style and documentation patterns
- All changes compile without warnings
- All 45 tests pass (run the test suite)
- Documentation is updated as needed

See [TODO.md](TODO.md) for the development roadmap and areas where contributions are welcome.

## 📞 Support

For questions or issues, please open an issue on the project repository.
