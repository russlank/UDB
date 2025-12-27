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

## 🚀 Quick Start

### Building the Project

1. Open `dbe.sln` in Visual Studio 2022/2025
2. Select your configuration (Debug/Release, x64)
3. Build the solution (F7 or Build → Build Solution)

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
dbe/
├── dbe/                     # Main source directory
│   ├── udb.h                # Main include file (include this in your projects)
│   ├── udb_common.h         # Common definitions, types, and utilities
│   ├── udb_file.h           # File I/O abstraction layer
│   ├── udb_file.cpp         # File I/O implementation
│   ├── udb_heap.h           # Heap file management (space allocation)
│   ├── udb_heap.cpp         # Heap file implementation
│   ├── udb_btree.h          # B-Tree index declarations
│   ├── udb_btree.cpp        # B-Tree index implementation
│   └── dbe.cpp              # Interactive test application
├── doc/                     # Documentation
│   ├── btree.md             # B-Tree algorithm explanation
│   ├── heap-file.md         # Heap file structure explanation
│   └── architecture.md      # Architecture overview
├── dbe.sln                  # Visual Studio solution
├── dbe.vcxproj              # Visual Studio project
├── README.md                # This file
└── .gitignore               # Git ignore rules
```

### Source File Descriptions

| File | Purpose |
|------|---------|
| `udb.h` | Main header - includes all other headers, provides version info |
| `udb_common.h` | Error codes, key types, constants, utility functions, exceptions |
| `udb_file.h/cpp` | Platform-independent file I/O with thread safety |
| `udb_heap.h/cpp` | Heap file for variable-length record storage with holes management |
| `udb_btree.h/cpp` | B+ Tree index implementation with multi-index support |
| `dbe.cpp` | Interactive command-line test application |

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

The test application (`dbe.cpp`) provides an interactive interface:

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

## 🔄 Migration from Legacy Code

This is a modernized version of classic DOS/Windows 3.1 database code. Key changes:

| Aspect | Original (DOS) | Modern (C++20) |
|--------|----------------|----------------|
| Memory | `farmalloc`/`farfree` | `std::vector`, RAII |
| Files | DOS handles (`_open`, `_read`) | `std::fstream` |
| Types | `int`, `long` | `int64_t`, `uint16_t` |
| Errors | Global error codes | Exceptions + error codes |
| Threading | None | `std::mutex` protection |
| Headers | Custom header files | `#pragma once`, include guards |

## ⚠️ Limitations and Known Issues

1. **Not for production use**: This is an educational/reference implementation
2. **No transaction support**: No ACID guarantees, no crash recovery
3. **Basic thread safety**: Coarse-grained locking, not optimized for high concurrency
4. **No compaction**: Heap file fragmentation accumulates over time
5. **Fixed endianness**: Assumes little-endian (x86/x64)

## 🛠️ Future Improvements

Potential enhancements for contributors:

- [ ] Hole coalescing in heap files
- [ ] File compaction
- [ ] Write-ahead logging (WAL) for crash recovery
- [ ] Fine-grained locking for better concurrency
- [ ] Memory-mapped file support
- [ ] Cross-platform endianness handling
- [ ] Unit test suite

## 📜 License

This project is provided as-is for educational and reference purposes.

## 🤝 Contributing

Contributions are welcome! Please ensure:
- Code follows the existing style and documentation patterns
- All changes compile without warnings
- Tests pass (run the test application)
- Documentation is updated as needed

## 📞 Support

For questions or issues, please open an issue on the project repository.
