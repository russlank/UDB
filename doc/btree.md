# B-Tree Index Structure and Algorithms

This document provides a detailed pedagogical explanation of the B-Tree indexing system implemented in the UDB library.

## Table of Contents

1. [Introduction to B-Trees](#introduction-to-b-trees)
2. [File Structure](#file-structure)
3. [Node Structure](#node-structure)
4. [Leaf Structure](#leaf-structure)
5. [Key Operations](#key-operations)
6. [Navigation](#navigation)
7. [Memory Management](#memory-management)
8. [Data Integrity](#data-integrity)

---

## Introduction to B-Trees

### What is a B-Tree?

A **B-Tree** (Balanced Tree) is a self-balancing tree data structure that maintains sorted data and allows searches, sequential access, insertions, and deletions in logarithmic time. B-Trees are particularly well-suited for storage systems that read and write large blocks of data, like databases and file systems.

### Why B-Trees?

| Operation | Array | Linked List | Binary Search Tree | B-Tree |
|-----------|-------|-------------|-------------------|--------|
| Search | O(n) or O(log n) | O(n) | O(log n)* | O(log n) |
| Insert | O(n) | O(1) | O(log n)* | O(log n) |
| Delete | O(n) | O(1) | O(log n)* | O(log n) |
| Sequential | O(1) | O(1) | O(log n) | O(1) |

*Binary trees can degrade to O(n) when unbalanced

### B-Tree vs B+ Tree

The UDB implementation is actually closer to a **B+ Tree**:

```
Standard B-Tree:
                    [30,50]
                   /   |   \
              [10,20] [35,40] [60,70,80]
              (data)  (data)   (data)

B+ Tree (UDB Style):
                    [30,50]
                   /   |   \
              [10,20] [35,40] [60,70]
                |        |        |
              Leaves   Leaves   Leaves (contain data pointers)
```

**Key Difference**: In the UDB implementation, only **leaves** contain data pointers. Internal nodes only contain keys and child pointers.

---

## File Structure

### Overview

The index file has the following layout:

```
┌─────────────────────────────────────────────────────────────┐
│                     FILE HEADER                              │
│  - Checksum (1 byte)                                         │
│  - Number of indexes (2 bytes)                               │
├─────────────────────────────────────────────────────────────┤
│                   INDEX INFO [0]                             │
│  - Checksum, KeyType, KeySize, MaxItems, etc.               │
├─────────────────────────────────────────────────────────────┤
│                   INDEX INFO [1]                             │
│  - (same structure)                                          │
├─────────────────────────────────────────────────────────────┤
│                        ...                                   │
├─────────────────────────────────────────────────────────────┤
│                   INDEX INFO [N-1]                           │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│                    NODES AND LEAVES                          │
│              (allocated and freed dynamically)               │
│                                                              │
└─────────────────────────────────────────────────────────────┘
```

### Header Structure

```cpp
struct MultiIndexHeader {
    uint8_t checksum;      // XOR checksum for integrity
    uint16_t numIndexes;   // Number of indexes in this file
};
```

### Index Info Structure

Each index maintains its own metadata:

```cpp
struct IndexInfo {
    uint8_t checksum;           // Data integrity check
    uint16_t attributes;        // UNIQUE, ALLOW_DELETE flags
    uint16_t keyType;           // STRING, INTEGER, etc.
    uint16_t keySize;           // Size of key in bytes
    uint16_t maxItems;          // Max keys per node (order)
    int64_t freeCreateNodes;    // Nodes to pre-allocate
    int64_t freeCreateLeaves;   // Leaves to pre-allocate
    
    // Runtime state
    int64_t freeNode;           // Head of free node list
    int64_t freeLeave;          // Head of free leaf list
    uint16_t numLevels;         // Current tree height
    int64_t rootNode;           // Position of root node
    int64_t firstLeave;         // Leftmost leaf (for iteration)
    int64_t lastLeave;          // Rightmost leaf (EOF marker)
};
```

---

## Node Structure

### Internal Nodes

Internal nodes store keys and pointers to children:

```
┌────────────────────────────────────────────────────────────┐
│                      NODE HEADER                            │
│  - Checksum (1 byte)                                        │
│  - NumUsed (2 bytes)    - Number of items in use            │
│  - NextNode (8 bytes)   - Next node at same level           │
│  - PrevNode (8 bytes)   - Previous node at same level       │
├────────────────────────────────────────────────────────────┤
│ Item 1: [Key1][ChildPtr1]                                   │
│ Item 2: [Key2][ChildPtr2]                                   │
│ Item 3: [Key3][ChildPtr3]                                   │
│ ...                                                         │
│ Item N: [KeyN][ChildPtrN]                                   │
└────────────────────────────────────────────────────────────┘
```

### Key Property

For any node with keys K₁, K₂, ..., Kₙ and child pointers C₁, C₂, ..., Cₙ:

```
All keys in subtree pointed by Cᵢ are ≤ Kᵢ
```

Visual example:

```
         [Dog, Lion, Zebra]
        /      |      \      \
    (≤Dog)  (≤Lion)  (≤Zebra)  (>Zebra - EOF)
```

### Horizontal Linking

Nodes at the same level are linked horizontally:

```
Level 1:  [Root Node]
              |
Level 2:  [N1] ←→ [N2] ←→ [N3] ←→ [N4]
           ↓       ↓       ↓       ↓
Level 3:  [Leaves linked horizontally]
```

This enables efficient level-order traversal and neighbor access during node splitting/merging.

---

## Leaf Structure

### Leaf Layout

Leaves store a single key and point to the actual data:

```
┌────────────────────────────────────────────────────────────┐
│                     LEAF HEADER                             │
│  - Checksum (1 byte)                                        │
│  - NextLeave (8 bytes)  - Next leaf (sequential access)     │
│  - PrevLeave (8 bytes)  - Previous leaf                     │
│  - DataPos (8 bytes)    - Position of actual data record    │
├────────────────────────────────────────────────────────────┤
│                      KEY DATA                               │
│  - The actual key value (keySize bytes)                     │
└────────────────────────────────────────────────────────────┘
```

### Leaf Chain

All leaves are doubly-linked for sequential traversal:

```
[First] ←→ [Leaf2] ←→ [Leaf3] ←→ ... ←→ [LastN] ←→ [EOF Leaf]
  ↓          ↓          ↓                  ↓           ↓
Data1      Data2      Data3             DataN        null
```

The **EOF Leaf** is a special sentinel with a maximum key value, ensuring every search terminates.

### Duplicate Keys

When duplicate keys are allowed, leaves with the same key form a chain:

```
           [K=Apple]
              ↓
[K=Apple,D=100] ←→ [K=Apple,D=200] ←→ [K=Apple,D=300] ←→ [K=Banana,...]
```

The first leaf of each key group is pointed to by the B-Tree nodes.

---

## Key Operations

### Search (Find)

The search algorithm traverses from root to leaf:

```
FindKey(key):
    node = rootNode
    level = numLevels
    
    while level > 0:
        // Find the first key >= search key
        i = 1
        while i <= numItems AND key > node.key[i]:
            i++
        
        // Follow child pointer
        node = node.child[i]
        level--
    
    // Now at leaf level
    if leaf.key == key:
        return leaf.dataPos
    else:
        return NOT_FOUND
```

**Time Complexity**: O(log n) where n is the number of keys

**Visual Example**: Finding "Dog" in a tree:

```
                    [Lion]
                   /      \
           [Cat, Dog]    [Zebra]
              ↓              ↓
    [Leaves: Ant,Cat,Dog] [Leaves: Lion,Zebra,EOF]

1. At root: "Dog" < "Lion" → go left
2. At [Cat,Dog]: "Dog" == "Dog" → found, go to leaf
3. Return leaf's data position
```

### Insert (Append)

Insertion involves finding the correct position and potentially splitting nodes:

```
Insert(key, dataPos):
    1. Find path to insertion point (using stack)
    2. Create new leaf with key and dataPos
    3. Link leaf into the leaf chain
    4. Update tree:
       - If node has space, insert key
       - If node is full, split it
       - Propagate splits up the tree if needed
       - If root splits, create new root
```

**Node Splitting**:

When a node overflows (numItems > maxItems), it splits:

```
Before:                    After:
    [A,B,C,D,E] (full)        [A,B,C] ←→ [D,E]
                                  ↑
                           New parent key: C
```

**Visual Example**: Inserting into a full node:

```
Before inserting "Frog" (maxItems=3):

         [Dog, Lion]                    [Dog, Fox, Lion]
        /     |     \          →       /    |    |    \
   [≤Dog] [≤Lion] [≤EOF]          [≤Dog][≤Fox][≤Lion][≤EOF]
```

If the node was already full, it would split:

```
Full node [Dog, Fox, Lion] + insert "Hippo":

Split into: [Dog, Fox] and [Lion]
Insert "Hippo" into [Lion] → [Hippo, Lion]
Promote "Fox" to parent
```

### Delete

Deletion removes leaves and may require node rebalancing:

```
Delete(key):
    1. Find the key in the tree
    2. Remove all leaves with this key
    3. Update leaf links (prev/next)
    4. Remove key from nodes
    5. Rebalance if needed:
       - Borrow from siblings if possible
       - Merge nodes if too empty
       - Reduce tree height if root becomes empty
```

**Rebalancing Rules**:

When a node becomes too empty (less than maxItems/2):

1. **Try to borrow** from a sibling that has extra keys
2. **Merge** with a sibling if borrowing isn't possible
3. **Propagate** the merge up if needed

```
Before merge:              After merge:
[A,B] ←→ [C]               [A,B,C]
  ↓                           ↓
(children)                (combined children)
```

---

## Navigation

### Sequential Access

The leaf chain enables efficient sequential access:

```cpp
// Forward iteration
pos = getFirst(key);    // Go to leftmost leaf
while (!isEOF()) {
    processKey(key);
    pos = getNext(key); // Follow nextLeave pointer
}

// Backward iteration
pos = getLast(key);     // Go to rightmost leaf (before EOF)
while (!isBOF()) {
    processKey(key);
    pos = getPrev(key); // Follow prevLeave pointer
}
```

### Position Tracking

The cursor maintains current position:

```cpp
struct PositionInfo {
    int64_t currentLeave;   // Current leaf position
    int64_t nextLeave;      // Next leaf position
    int64_t prevLeave;      // Previous leaf position
    int64_t currentDataPos; // Data position of current leaf
    uint16_t state;         // EOF/BOF flags
};
```

---

## Memory Management

### Free Lists

Nodes and leaves are managed using free lists:

```
Free Node List:
[Free1] → [Free2] → [Free3] → ... → -1

When allocating:
1. Take from head of free list
2. If empty, create new batch of nodes

When freeing:
1. Add to head of free list
```

### Pre-allocation

To reduce file growth operations, nodes/leaves are pre-allocated in batches:

```cpp
createNodes(freeCreateNodes);   // Create batch of empty nodes
createLeaves(freeCreateLeaves); // Create batch of empty leaves
```

---

## Data Integrity

### Checksums

Every structure includes a checksum byte:

```cpp
uint8_t calculateBlockChecksum(const void* block, size_t size) {
    uint8_t checksum = 0;
    for (size_t i = 0; i < size; i++) {
        checksum ^= block[i];
    }
    return checksum;
}
```

**Verification**:
- Before writing: Calculate checksum, store in first byte
- After reading: Recalculate checksum; if non-zero, data is corrupt

### Why XOR Checksum?

- Simple and fast
- Detects single-bit errors
- Adding checksum byte makes total XOR = 0 for valid data
- Limitations: Can't detect all multi-bit errors

---

## Practical Considerations

### Choosing maxItems (B-Tree Order)

| maxItems | Pros | Cons |
|----------|------|------|
| Small (3-5) | Less I/O per node | Deeper tree, more I/O for search |
| Large (50-100) | Shallow tree | More I/O per node access |

**Rule of thumb**: Choose based on disk block size. If a node fits in one disk block, I/O is minimized.

### Key Size Considerations

```
Node size = sizeof(header) + maxItems × (keySize + 8)

Example with keySize=50, maxItems=10:
Node size = 19 + 10 × (50 + 8) = 599 bytes
```

### Performance Tips

1. **Pre-allocate generously**: Reduces file extension operations
2. **Use appropriate key types**: STRING comparison is slower than INTEGER
3. **Enable ALLOW_DELETE**: Keeps tree balanced during deletes
4. **Batch operations**: Multiple inserts are more efficient than single inserts

---

## Further Reading

1. Cormen, T.H., et al. "Introduction to Algorithms" - Chapter 18
2. Knuth, D.E. "The Art of Computer Programming, Vol. 3" - Section 6.2.4
3. Comer, D. "The Ubiquitous B-Tree" - Computing Surveys, 1979

---

*This document is part of the UDB library documentation.*
