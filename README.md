# UDB - Ultra Database Engine

A lightweight, high-performance B-Tree indexing engine with dynamic heap-structured record storage implementation.

## 📋 Overview

UDB (Ultra Database) is a modernized version of a classic database indexing library, originally developed for DOS/Windows 3.1 and now updated for Visual Studio 2022/2025 and modern C++20.

### Key Features

- **B-Tree Indexing**: Efficient key-based access with O(log n) search, insert, and delete operations
- **Multiple Indexes**: Support for multiple indexes per file with different key types
- **Heap File Storage**: Dynamic variable-length record storage with space reclamation
- **Data Integrity**: Checksums on all data structures for corruption detection
- **Multiple Key Types**: Strings, integers, blocks, characters, and logical values
- **Sequential Access**: Efficient cursor-based navigation (first, next, previous)

## 🚀 Quick Start

### Building the Project

1. Open `UDB.sln` in Visual Studio 2022/2025
2. Select your configuration (Debug/Release, x86/x64)
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
UDB/
├── include/             # Header files
│   ├── udb.h            # Main include file
│   ├── udb_common.h     # Common definitions and utilities
│   ├── udb_file.h       # File I/O abstraction
│   ├── udb_heap.h       # Heap file management
│   └── udb_btree.h      # B-Tree index implementation
├── src/                 # Source files
│   ├── udb_file.cpp     # File I/O implementation
│   ├── udb_heap.cpp     # Heap file implementation
│   └── udb_btree.cpp    # B-Tree implementation
├── test/                # Test application
│   ├── test_main.cpp    # Interactive test program
│   └── UDBTest.vcxproj  # Test project file
├── doc/                 # Documentation
│   ├── btree.md         # B-Tree algorithm explanation
│   ├── heap-file.md     # Heap file explanation
│   └── architecture.md  # Architecture overview
├── legacy-code/         # Original legacy code
├── UDB.sln              # Visual Studio solution
├── UDB.vcxproj          # Library project file
└── README.md            # This file
```

## 🔧 Configuration Options

### Key Types

| Type | Description | Size |
|------|-------------|------|
| `STRING` | Null-terminated string | Variable |
| `INTEGER` | 16-bit signed integer | 2 bytes |
| `LONG_INT` | 32-bit signed integer | 4 bytes |
| `BLOCK` | Raw byte comparison | Variable |
| `NUM_BLOCK` | Numeric block (big-endian) | Variable |
| `CHARACTER` | Single character | 1 byte |
| `LOGICAL` | Boolean value | 1 byte |

### Index Attributes

| Attribute | Description |
|-----------|-------------|
| `NONE` | No special attributes |
| `UNIQUE` | Keys must be unique |
| `ALLOW_DELETE` | Enable key deletion with node balancing |

## 📚 API Reference

### MultiIndex Class

#### Construction

```cpp
// Create new file with N indexes
MultiIndex(const std::string& filename, uint16_t numIndexes);

// Open existing file
MultiIndex(const std::string& filename);
```

#### Index Management

```cpp
void setActiveIndex(uint16_t indexNo);    // Set active index (1-based)
uint16_t getActiveIndex() const;          // Get active index number
uint16_t getNumIndexes() const;           // Get total number of indexes
void initIndex(...);                       // Initialize an index
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

## 🧪 Running Tests

The test application provides an interactive interface:

```
> A Hello          # Append key "Hello"
> A World          # Append key "World"
> F Hello          # Find key "Hello"
> L                # List all keys
> +                # Next key
> -                # Previous key
> R                # Remove current
> X                # Exit
```

## 📖 Documentation

Detailed documentation is available in the `doc/` directory:

- [B-Tree Algorithm](doc/btree.md) - How the B-Tree indexing works
- [Heap File Structure](doc/heap-file.md) - How records are stored
- [Architecture Overview](doc/architecture.md) - Library design and components

## 🔄 Migration from Legacy Code

The original DOS/Windows 3.1 code is preserved in the `old-code/` directory. Key changes:

1. **Modern C++20**: Uses modern C++ features, STL containers, and RAII
2. **Platform Independence**: Removed DOS-specific memory functions
3. **Type Safety**: Strong typing with enums and proper integer types
4. **Exception Handling**: Uses exceptions instead of error codes
5. **Thread Safety**: Basic mutex protection for file operations

## 📜 License

This project is provided as-is for educational and reference purposes.

## 🤝 Contributing

Contributions are welcome! Please ensure:
- Code follows the existing style
- All changes compile without warnings
- Tests pass

## 📞 Support

For questions or issues, please open an issue on the project repository.
