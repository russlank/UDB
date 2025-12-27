# Heap File Structure and Algorithms

This document provides a detailed explanation of the heap file storage system implemented in the UDB library. The heap file provides dynamic, variable-length record storage with efficient space reuse.

## Table of Contents

1. [Introduction](#introduction)
2. [File Structure](#file-structure)
3. [Holes Tables](#holes-tables)
4. [Space Allocation](#space-allocation)
5. [Space Deallocation](#space-deallocation)
6. [Integration with B-Tree Index](#integration-with-b-tree-index)
7. [Performance Considerations](#performance-considerations)

---

## Introduction

### What is a Heap File?

A **heap file** is a simple file organization where records are stored in no particular order. New records are appended to the end of the file, and deleted records leave "holes" that can be reused.

### Why Heap Files?

| Characteristic | Heap File | Sequential File | Indexed File |
|---------------|-----------|-----------------|--------------|
| Insert Speed | O(1)* | O(n) | O(log n) |
| Search Speed | O(n) | O(n) or O(log n) | O(log n) |
| Delete Speed | O(1)* | O(n) | O(log n) |
| Space Reuse | Yes | Difficult | N/A |
| Variable Length | Yes | Complex | Via heap |

*With position known (from index)

### Use Case in UDB

The B-Tree index stores **positions** of records, not the records themselves. The heap file stores the actual data:

```
┌──────────────┐         ┌──────────────┐
│  B-Tree      │ ──────> │  Heap File   │
│  Index       │ dataPos │  (Records)   │
│  [Key→Pos]   │         │              │
└──────────────┘         └──────────────┘
```

---

## File Structure

### Overview

```
┌─────────────────────────────────────────────────────────────┐
│                     FILE HEADER                             │
│  - Checksum (1 byte)                                        │
│  - FirstHolesTablePos (8 bytes)                             │
│  - HolesTableSize (2 bytes)                                 │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│                   DATA RECORDS                              │
│              (Variable length)                              │
│                                                             │
│  ┌─────────┐ ┌─────────────┐ ┌────────┐ ┌─────────────┐     │
│  │Record 1 │ │   Record 2  │ │ HOLE   │ │  Record 4   │     │
│  │ 50 bytes│ │   120 bytes │ │75 bytes│ │   90 bytes  │     │
│  └─────────┘ └─────────────┘ └────────┘ └─────────────┘     │
│                                                             │
├─────────────────────────────────────────────────────────────┤
│                   HOLES TABLE 1                             │
│  - Checksum, NumUsed, NextTablePos                          │
│  - HoleRecord[0]: pos=125, size=75                          │
│  - HoleRecord[1]: (empty)                                   │
│  - ...                                                      │
├─────────────────────────────────────────────────────────────┤
│                   HOLES TABLE 2 (if needed)                 │
│  - (same structure)                                         │
└─────────────────────────────────────────────────────────────┘
```

### Header Structure

```cpp
struct HeapFileHeader {
    uint8_t checksum;           // XOR checksum
    int64_t firstHolesTablePos; // Position of first holes table
                                // -1 if no holes exist
    uint16_t holesTableSize;    // Entries per holes table
};
```

### Design Decisions

1. **Variable-length records**: No padding, exact size storage
2. **Holes tracking**: Linked list of holes tables
3. **Header at position 0**: Fast access to file metadata
4. **Checksums everywhere**: Data integrity verification

---

## Holes Tables

### Purpose

When records are deleted, their space becomes available for reuse. The **holes tables** track these free spaces.

### Structure

```cpp
struct HolesTableHeader {
    uint8_t checksum;       // Data integrity
    uint16_t numUsed;       // Number of entries in use
    int64_t nextTablePos;   // Next holes table (-1 if last)
    // Followed by HoleRecord array
};

struct HoleRecord {
    int64_t position;       // Where the hole starts
    int64_t size;           // Size of the hole
};
```

### Visual Representation

```
Holes Table (holesTableSize = 5):
┌──────────────────────────────────────────┐
│ checksum: 0x5A                           │
│ numUsed: 3                               │
│ nextTablePos: -1                         │
├──────────────────────────────────────────┤
│ [0] pos=1000, size=256                   │  ← Used
│ [1] pos=5000, size=128                   │  ← Used
│ [2] pos=8500, size=512                   │  ← Used
│ [3] pos=0, size=0                        │  ← Empty
│ [4] pos=0, size=0                        │  ← Empty
└──────────────────────────────────────────┘
```

### Linked Holes Tables

When one table fills up, a new one is created:

```
┌────────────────┐     ┌────────────────┐     ┌────────────────┐
│  Holes Table 1 │────>│  Holes Table 2 │────>│  Holes Table 3 │
│  (full)        │     │  (full)        │     │  (has space)   │
│  numUsed=100   │     │  numUsed=100   │     │  numUsed=45    │
│  next=pos2     │     │  next=pos3     │     │  next=-1       │
└────────────────┘     └────────────────┘     └────────────────┘
```

---

## Space Allocation

### Algorithm

```
AllocateSpace(size):
    1. Search holes tables for suitable hole
    2. If found:
       a. If hole.size == size:
          - Remove hole from table
          - Return hole.position
       b. If hole.size > size:
          - Shrink hole: hole.position += size, hole.size -= size
          - Return original hole.position
    3. If not found:
       - Return current file size (append position)
       - File will grow when data is written
```

### Finding a Suitable Hole

**First-Fit Strategy**: Find the first hole that's large enough

```cpp
std::optional<int64_t> findSuitableHole(size_t size) {
    int64_t tablePos = firstHolesTablePos;
    
    while (tablePos != -1) {
        // Read holes table
        for (each entry in table) {
            if (entry.size >= size) {
                // Found suitable hole
                return entry.position;
            }
        }
        tablePos = nextTablePos;
    }
    
    return std::nullopt; // No suitable hole
}
```

### Example Allocation

```
File state:
  Holes: [(pos=100, size=50), (pos=500, size=200), (pos=900, size=80)]

Request: Allocate 120 bytes

Step 1: Check hole at pos=100, size=50: Too small
Step 2: Check hole at pos=500, size=200: Big enough!
Step 3: Shrink hole: pos=620, size=80

Result:
  - Allocated at position 500
  - Remaining holes: [(pos=100, size=50), (pos=620, size=80), (pos=900, size=80)]
```

### Visual Diagram

```
Before allocation of 120 bytes:
┌─────────┬─────────────────┬──────────────────────┬───────────┬─────────┐
│  Data   │     HOLE        │         Data         │   HOLE    │  Data   │
│  0-99   │   100-149       │       150-499        │ 500-699   │ 700-899 │
│         │   (50 bytes)    │                      │(200 bytes)│         │
└─────────┴─────────────────┴──────────────────────┴───────────┴─────────┘

After allocation of 120 bytes (from hole at 500):
┌─────────┬─────────────────┬──────────────────────┬──────────┬───────────┬─────────┐
│  Data   │     HOLE        │         Data         │NEW RECORD│  HOLE     │  Data   │
│  0-99   │   100-149       │       150-499        │ 500-619  │ 620-699   │ 700-899 │
│         │   (50 bytes)    │                      │(120 bytes)│(80 bytes)│         │
└─────────┴─────────────────┴──────────────────────┴──────────┴───────────┴─────────┘
```

---

## Space Deallocation

### Algorithm

```
FreeSpace(position, size):
    1. Search holes tables for table with space
    2. If found:
       - Add new hole entry
    3. If not found:
       - Create new holes table
       - Add hole to new table
       - Link new table into chain
```

### Adding a Hole

```cpp
void addHole(int64_t position, size_t size) {
    int64_t tablePos = firstHolesTablePos;
    int64_t prevTablePos = -1;
    
    // Find table with space
    while (tablePos != -1) {
        if (numUsed < holesTableSize) {
            // Add hole here
            setHoleEntry(numUsed, position, size);
            numUsed++;
            return;
        }
        prevTablePos = tablePos;
        tablePos = nextTablePos;
    }
    
    // Need new table
    createNewHolesTable();
    addHoleToNewTable(position, size);
    linkToChain(prevTablePos);
}
```

### Example Deallocation

```
File state before delete:
┌──────────┬──────────┬──────────┬──────────┐
│ Record A │ Record B │ Record C │ Record D │
│  0-99    │ 100-199  │ 200-349  │ 350-499  │
└──────────┴──────────┴──────────┴──────────┘
Holes: []

Delete Record B (100 bytes at position 100):
┌──────────┬──────────┬──────────┬──────────┐
│ Record A │  HOLE    │ Record C │ Record D │
│  0-99    │ 100-199  │ 200-349  │ 350-499  │
└──────────┴──────────┴──────────┴──────────┘
Holes: [(pos=100, size=100)]

Delete Record C (150 bytes at position 200):
┌──────────┬──────────┬──────────┬──────────┐
│ Record A │  HOLE    │   HOLE   │ Record D │
│  0-99    │ 100-199  │ 200-349  │ 350-499  │
└──────────┴──────────┴──────────┴──────────┘
Holes: [(pos=100, size=100), (pos=200, size=150)]
```

---

## Integration with B-Tree Index

### Typical Usage Pattern

```cpp
// Store a record
int64_t recordPos = heapFile.allocateSpace(recordSize);
heapFile.write(recordData, recordSize, recordPos);
index.append(key, recordPos);

// Retrieve a record
int64_t recordPos = index.find(key);
heapFile.read(buffer, recordSize, recordPos);

// Delete a record
int64_t recordPos = index.find(key);
index.deleteKey(key);
heapFile.freeSpace(recordPos, recordSize);
```

### Coordination

```
┌─────────────────┐          ┌─────────────────┐
│   B-Tree        │          │   Heap File     │
│   Index         │          │                 │
│                 │          │                 │
│  Key: "Alice"   │────────> │  [Record at     │
│  DataPos: 500   │          │   position 500] │
│                 │          │                 │
│  Key: "Bob"     │────────> │  [Record at     │
│  DataPos: 1200  │          │   position 1200 │
│                 │          │                 │
└─────────────────┘          └─────────────────┘
```

### Important: Record Size Tracking

The heap file doesn't track record sizes. The application must:
1. Store size within the record, or
2. Use fixed-size records, or
3. Maintain size externally

```cpp
// Option 1: Size prefix
struct RecordOnDisk {
    uint32_t size;
    uint8_t data[];
};

// Option 2: Fixed size with type indicator
struct FixedRecord {
    uint8_t type;
    uint8_t data[255];
};
```

---

## Performance Considerations

### Fragmentation

Over time, allocations and deallocations can cause fragmentation:

```
Fragmented file:
┌───┬────┬───┬─────┬───┬───────┬───┬───┐
│ D │HOLE│ D │HOLE │ D │ HOLE  │ D │ D │
│50 │ 30 │80 │ 15  │40 │  200  │60 │90 │
└───┴────┴───┴─────┴───┴───────┴───┴───┘

Problem: Many small holes that can't be used
```

### Mitigation Strategies

1. **Coalescing**: Merge adjacent holes (not implemented in basic version)
2. **Compaction**: Rewrite file to eliminate holes (expensive)
3. **Best-fit allocation**: Find smallest suitable hole

### Coalescing Algorithm (Enhancement)

```cpp
void coalesceHoles() {
    // Sort holes by position
    sortHolesByPosition();
    
    // Merge adjacent
    for (int i = 0; i < numHoles - 1; ) {
        if (holes[i].position + holes[i].size == holes[i+1].position) {
            // Adjacent - merge
            holes[i].size += holes[i+1].size;
            removeHole(i+1);
        } else {
            i++;
        }
    }
}
```

### Space vs Speed Trade-off

| holesTableSize | Pros | Cons |
|----------------|------|------|
| Small (10-20) | Less memory | More tables, slower search |
| Large (100+) | Faster search | More memory per table |

**Recommendation**: 50-100 entries per table is a good balance.

### When to Compact

Consider compaction when:
- Free space exceeds 30-40% of file size
- Many small holes exist
- Application has idle time

---

## Advanced Topics

### Transaction Safety (Not Implemented)

For crash recovery, you would need:
1. Write-ahead logging (WAL)
2. Checkpoints
3. Recovery procedures

### Concurrent Access (Basic Protection)

The current implementation uses mutex for basic thread safety:
- File operations are serialized
- No reader-writer optimization

For high concurrency, consider:
- Fine-grained locking
- Lock-free algorithms
- MVCC (Multi-Version Concurrency Control)

---

## Code Examples

### Creating a Heap File

```cpp
#include "udb_heap.h"
using namespace udb;

// Create new heap file with 100-entry holes tables
HeapFile heap("data.heap", 100);

// Or open existing
HeapFile existingHeap("data.heap");
```

### Storing Variable-Length Records

```cpp
struct Person {
    char name[50];
    int age;
};

Person person = {"Alice", 30};
int64_t pos = heap.allocateSpace(sizeof(Person));
heap.write(&person, sizeof(Person), pos);

// Store position in index
index.append("Alice", pos);
```

### Retrieving Records

```cpp
int64_t pos = index.find("Alice");
if (pos >= 0) {
    Person person;
    heap.read(&person, sizeof(Person), pos);
    std::cout << person.name << ", age " << person.age << std::endl;
}
```

---

## Summary

The heap file provides:

1. **Simple storage**: Append-anywhere, read-anywhere
2. **Space reuse**: Deleted space tracked and reused
3. **Variable-length**: No record size restrictions
4. **Integrity**: Checksums on all metadata

Combined with the B-Tree index, it forms a complete database storage system.

---

*This document is part of the UDB library documentation.*
