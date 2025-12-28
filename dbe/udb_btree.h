/**
 * @file udb_btree.h
 * @brief B-Tree index implementation for the UDB library
 *
 * This file implements a B-Tree (actually a B+ tree variant) index structure
 * that provides efficient key-based access to records. The implementation
 * supports multiple indexes per file with different key types.
 *
 * ## What is a B-Tree?
 *
 * A B-Tree is a self-balancing tree data structure that maintains sorted data
 * and allows searches, sequential access, insertions, and deletions in
 * logarithmic time. B-Trees are optimized for systems that read/write large
 * blocks of data (like disk-based databases).
 *
 * ## B-Tree vs B+ Tree
 *
 * This implementation is closer to a **B+ Tree**:
 * - Internal nodes contain only keys and child pointers
 * - **Only leaves contain data pointers** (positions in the data file)
 * - Leaves are linked for efficient sequential access
 *
 * ```
 * B-Tree:                          B+ Tree (this implementation):
 *        [30|50]                          [30|50]
 *       /   |   \                        /   |   \
 *   [10|20][40][60|70]               [10|20][40][60|70]
 *      ?data    ?data                   |     |     |
 *                                      [Leaves with data pointers]
 * ```
 *
 * ## File Layout
 *
 * ```
 * ????????????????????????????????????????????????????????????????
 * ?                    MULTI-INDEX HEADER                        ?
 * ?  [checksum][numIndexes]                                      ?
 * ????????????????????????????????????????????????????????????????
 * ?                    INDEX INFO [0]                            ?
 * ?  [checksum][attributes][keyType][keySize][maxItems]...       ?
 * ????????????????????????????????????????????????????????????????
 * ?                    INDEX INFO [1]                            ?
 * ?  ... (one per index)                                         ?
 * ????????????????????????????????????????????????????????????????
 * ?                                                              ?
 * ?                    NODES AND LEAVES                          ?
 * ?              (allocated dynamically)                         ?
 * ?                                                              ?
 * ?  Free nodes are linked: [Free1] -> [Free2] -> -1             ?
 * ?  Free leaves are linked similarly                            ?
 * ?                                                              ?
 * ????????????????????????????????????????????????????????????????
 * ```
 *
 * ## Key Properties
 *
 * For a B-Tree node with keys K?, K?, ..., K? and children C?, C?, ..., C?:
 * - All keys in subtree C? are ? K?
 * - This enables binary search within nodes and logarithmic tree traversal
 *
 * ## Time Complexity
 *
 * | Operation | Average | Worst Case |
 * |-----------|---------|------------|
 * | Search | O(log n) | O(log n) |
 * | Insert | O(log n) | O(log n) |
 * | Delete | O(log n) | O(log n) |
 * | Sequential Next | O(1) | O(1) |
 *
 * ## Space Complexity
 *
 * - Tree height: O(log n) where base is maxItems
 * - File size: O(n) with overhead for node structure
 *
 * ## Usage Example
 *
 * ```cpp
 * #include "udb_btree.h"
 * using namespace udb;
 *
 * // Create index file with 2 indexes
 * MultiIndex index("data.ndx", 2);
 *
 * // Initialize first index for string keys
 * index.setActiveIndex(1);
 * index.initIndex(
 *     KeyType::STRING,              // Key type
 *     50,                           // Key size (max string length)
 *     IndexAttribute::ALLOW_DELETE, // Enable deletion
 *     5,                            // Keys per node (B-Tree order)
 *     100,                          // Pre-allocate 100 nodes
 *     200                           // Pre-allocate 200 leaves
 * );
 *
 * // Add entries
 * index.append("Alice", 1000);  // Key "Alice" points to position 1000
 * index.append("Bob", 2000);
 * index.append("Charlie", 3000);
 *
 * // Find an entry
 * int64_t pos = index.find("Bob");  // Returns 2000
 *
 * // Iterate through all entries
 * char key[51];
 * pos = index.getFirst(key);
 * while (pos >= 0 && !index.isEOF()) {
 *     std::cout << key << " -> " << pos << std::endl;
 *     pos = index.getNext(key);
 * }
 * ```
 *
 * ## Thread Safety
 *
 * Not thread-safe. External synchronization required for concurrent access.
 *
 * @author Digixoil
 * @version 2.0.0 (Modernized for Visual Studio 2025)
 * @date 2025
 *
 * @see doc/btree.md for detailed algorithm documentation
 * @see udb_heap.h for data record storage
 */

#ifndef UDB_BTREE_H
#define UDB_BTREE_H

#include "udb_file.h"
#include <vector>
#include <memory>
#include <functional>
#include <stack>

namespace udb {

    //=============================================================================
    // B-Tree File Structures
    //=============================================================================

    /**
     * @brief Pragma pack ensures structures match file format exactly
     *
     * Critical for cross-platform compatibility. Without packing, compilers
     * may add padding that differs between platforms.
     */
#pragma pack(push, 1)

    /**
     * @brief Multi-index file header
     *
     * Stored at position 0 of every index file. Contains metadata about
     * the file and the number of indexes it contains.
     *
     * ## Size: 3 bytes
     * ```
     * Offset  Size  Field
     * 0       1     checksum
     * 1       2     numIndexes
     * ```
     */
    struct MultiIndexHeader {
        uint8_t checksum;               ///< XOR checksum for integrity verification
        uint16_t numIndexes;            ///< Number of indexes in this file (1 to 65535)
    };

    /**
     * @brief Information about a single index
     *
     * Each index has its own configuration and state. These structures
     * are stored sequentially after the file header.
     *
     * ## Design Note
     *
     * Each index in a file can have different:
     * - Key types and sizes
     * - Node capacities (maxItems)
     * - Attributes (unique, allow delete)
     *
     * This allows a single file to index the same data in multiple ways.
     *
     * ## Size: ~60 bytes (varies by platform for int64_t)
     */
    struct IndexInfo {
        uint8_t checksum;               ///< XOR checksum for integrity
        uint16_t attributes;            ///< Index attributes (IndexAttribute flags)
        uint16_t keyType;               ///< Key type (KeyType enum value)
        uint16_t keySize;               ///< Size of key in bytes
        uint16_t maxItems;              ///< Maximum items per node (B-Tree order)
        int64_t freeCreateNodes;        ///< Number of nodes to pre-create when needed
        int64_t freeCreateLeaves;       ///< Number of leaves to pre-create when needed

        // Runtime state (persisted to disk)
        int64_t freeNode;               ///< Head of free node list (-1 if none)
        int64_t freeLeave;              ///< Head of free leaf list (-1 if none)
        uint16_t numLevels;             ///< Current number of tree levels (height)
        int64_t rootNode;               ///< Position of root node
        int64_t firstLeave;             ///< Position of first (leftmost) leaf
        int64_t lastLeave;              ///< Position of last (rightmost) leaf - EOF marker
    };

    /**
     * @brief Node header structure (internal nodes)
     *
     * Internal nodes contain keys and pointers to children (other nodes or leaves).
     * They do NOT contain data pointers directly.
     *
     * ## Node Layout
     * ```
     * ??????????????????????????????????????????????????????????????
     * ?                     NODE HEADER (19 bytes)                 ?
     * ?  [checksum(1)][numUsed(2)][nextNode(8)][prevNode(8)]       ?
     * ??????????????????????????????????????????????????????????????
     * ?  Item 1: [Key (keySize bytes)][ChildPtr (8 bytes)]         ?
     * ?  Item 2: [Key (keySize bytes)][ChildPtr (8 bytes)]         ?
     * ?  ...                                                       ?
     * ?  Item N: [Key (keySize bytes)][ChildPtr (8 bytes)]         ?
     * ??????????????????????????????????????????????????????????????
     * ```
     *
     * ## Horizontal Linking
     *
     * Nodes at the same level are doubly-linked via nextNode/prevNode.
     * This enables efficient sibling access during split/merge operations
     * and level-order traversal.
     */
    struct NodeHeader {
        uint8_t checksum;               ///< XOR checksum for integrity
        uint16_t numUsed;               ///< Number of items currently in use (1 to maxItems)
        int64_t nextNode;               ///< Next node at same level (for traversal/splitting)
        int64_t prevNode;               ///< Previous node at same level
        // Followed by: [Key0][ChildPtr0][Key1][ChildPtr1]...
    };

    /**
     * @brief Leaf header structure
     *
     * Leaves are the bottom level of the tree. Each leaf contains ONE key
     * and a pointer to the actual data record. Leaves are doubly-linked
     * for efficient sequential traversal.
     *
     * ## Leaf Layout
     * ```
     * ??????????????????????????????????????????????????????????????
     * ?                    LEAF HEADER (25 bytes)                  ?
     * ?  [checksum(1)][nextLeave(8)][prevLeave(8)][dataPos(8)]     ?
     * ??????????????????????????????????????????????????????????????
     * ?                    KEY DATA (keySize bytes)                ?
     * ??????????????????????????????????????????????????????????????
     * ```
     *
     * ## Leaf Chain
     *
     * All leaves are linked in key order:
     * ```
     * [First] <-> [Leaf2] <-> [Leaf3] <-> ... <-> [EOF Leaf]
     *    ?           ?           ?                    ?
     *  data1       data2       data3              -1 (invalid)
     * ```
     *
     * The EOF Leaf is a sentinel with maximum possible key value,
     * ensuring every search terminates properly.
     */
    struct LeafHeader {
        uint8_t checksum;               ///< XOR checksum for integrity
        int64_t nextLeave;              ///< Next leaf in sequence (for getNext)
        int64_t prevLeave;              ///< Previous leaf in sequence (for getPrev)
        int64_t dataPos;                ///< Position of actual data record in data file
        // Followed by key data (keySize bytes)
    };

    /**
     * @brief Current position state for navigation
     *
     * Tracks the cursor position within the index for sequential access.
     * Each index maintains its own position state.
     *
     * ## State Management
     *
     * After operations:
     * - append(): Position moves to new entry
     * - find(): Position moves to found entry (or nearest)
     * - getFirst(): Position at first entry
     * - getNext(): Position advances to next entry
     * - getPrev(): Position moves to previous entry
     * - delete: Position moves to next entry if possible
     */
    struct PositionInfo {
        uint16_t state;                 ///< State flags (PositionState: BOF, EOF)
        int64_t currentLeave;           ///< Current leaf position
        int64_t nextLeave;              ///< Next leaf position (cached for efficiency)
        int64_t prevLeave;              ///< Previous leaf position (cached)
        int64_t currentDataPos;         ///< Current data position (from leaf)
    };

#pragma pack(pop)

    //=============================================================================
    // Navigation Stack
    //=============================================================================

    /**
     * @brief Stack item for tree traversal
     *
     * During insert and delete operations, we need to remember the path
     * from root to leaf so we can backtrack for node splitting or merging.
     */
    struct StackItem {
        int64_t nodePos;                ///< Node position in file
        uint16_t childNo;               ///< Child number within node (1-based)
    };

    /**
     * @brief Stack for tracking path through B-Tree
     *
     * Used during insert and delete operations to track the path
     * from root to leaf, enabling backtracking for node splitting/merging.
     *
     * ## Usage
     *
     * ```cpp
     * IndexStack stack;
     * int64_t leafPos;
     *
     * // Find path to leaf
     * findPath(key, stack, leafPos);
     *
     * // After insertion, backtrack to handle splits
     * while (!stack.empty()) {
     *     int64_t nodePos;
     *     uint16_t childNo;
     *     stack.pop(nodePos, childNo);
     *     // Process node...
     * }
     * ```
     */
    class IndexStack {
    public:
        /**
         * @brief Push a node onto the stack
         * @param nodePos Position of the node in file
         * @param childNo Which child was followed (1-based)
         */
        void push(int64_t nodePos, uint16_t childNo);

        /**
         * @brief Pop a node from the stack
         * @param nodePos Output: position of the node
         * @param childNo Output: child number
         * @return true if item was popped, false if stack was empty
         */
        bool pop(int64_t& nodePos, uint16_t& childNo);

        /**
         * @brief Peek at top of stack without removing
         * @param nodePos Output: position of the node
         * @param childNo Output: child number
         * @return true if item exists, false if stack is empty
         */
        bool top(int64_t& nodePos, uint16_t& childNo) const;

        /**
         * @brief Check if stack is empty
         */
        bool empty() const { return m_stack.empty(); }

        /**
         * @brief Clear all items from the stack
         *
         * Uses the swap idiom for guaranteed cleanup:
         * `m_stack = std::stack<StackItem>();`
         */
        void clear() { m_stack = std::stack<StackItem>(); }

    private:
        std::stack<StackItem> m_stack;  ///< Internal storage
    };

    //=============================================================================
    // MultiIndex Class
    //=============================================================================

    /**
     * @brief Multi-index B-Tree file manager
     *
     * This class manages a file containing multiple B-Tree indexes.
     * Each index can have different key types and configurations.
     *
     * ## Features
     *
     * - **Multiple indexes per file**: Index the same data in different ways
     * - **Various key types**: Strings, integers, binary blocks, etc.
     * - **Sequential access**: Efficient cursor-based navigation
     * - **Optional duplicate keys**: Allow or prohibit duplicate keys
     * - **Optional deletion**: Enable node rebalancing on delete
     *
     * ## Multi-Index Use Case
     *
     * Suppose you have employee records with ID and name. You could:
     * ```cpp
     * MultiIndex index("employees.ndx", 2);
     *
     * // Index 1: by ID (integer, unique)
     * index.setActiveIndex(1);
     * index.initIndex(KeyType::LONG_INT, 4,
     *     IndexAttribute::UNIQUE | IndexAttribute::ALLOW_DELETE, ...);
     *
     * // Index 2: by name (string, allows duplicates)
     * index.setActiveIndex(2);
     * index.initIndex(KeyType::STRING, 50,
     *     IndexAttribute::ALLOW_DELETE, ...);
     *
     * // Add employee (to both indexes)
     * int64_t dataPos = heapFile.write(employee);
     * index.setActiveIndex(1);
     * index.append(&employee.id, dataPos);
     * index.setActiveIndex(2);
     * index.append(employee.name, dataPos);
     * ```
     *
     * ## B-Tree Order (maxItems)
     *
     * The maxItems parameter determines how many keys fit in each node.
     * Trade-offs:
     *
     * | maxItems | Tree Height | Node Size | Node Operations |
     * |----------|-------------|-----------|-----------------|
     * | Small (3-5) | Taller | Smaller | Faster per-node |
     * | Large (50+) | Shorter | Larger | More I/O per-node |
     *
     * **Recommendation**: 5-20 for most cases. Larger for read-heavy workloads.
     *
     * ## Pre-allocation
     *
     * The freeCreateNodes and freeCreateLeaves parameters control how many
     * nodes/leaves are pre-allocated when the free list runs empty. This
     * reduces file growth operations but uses more space upfront.
     *
     * **Recommendation**: 50-200 depending on expected data size.
     *
     * ## Thread Safety
     *
     * NOT thread-safe. For concurrent access:
     * - Use external mutex/lock
     * - Use one index object per thread
     * - Use file-level locking (not implemented)
     */
    class MultiIndex : public File {
    public:
        /**
         * @brief Create a new multi-index file
         *
         * Creates a new index file with space for the specified number of indexes.
         * Each index must be initialized separately via initIndex().
         *
         * @param filename Path to the file (created/overwritten)
         * @param numIndexes Number of indexes to create (1-65535)
         *
         * @throws FileIOException if file cannot be created
         *
         * @example
         * ```cpp
         * // Create file with 3 indexes
         * MultiIndex index("data.ndx", 3);
         *
         * // Initialize each index
         * for (int i = 1; i <= 3; i++) {
         *     index.setActiveIndex(i);
         *     index.initIndex(...);
         * }
         * ```
         */
        MultiIndex(const std::string& filename, uint16_t numIndexes);

        /**
         * @brief Open an existing multi-index file
         *
         * Opens an existing index file and reads all index metadata.
         *
         * @param filename Path to the file (must exist)
         *
         * @throws FileIOException if file cannot be opened
         * @throws DataCorruptionException if checksum verification fails
         *
         * @example
         * ```cpp
         * try {
         *     MultiIndex index("existing.ndx");
         *     std::cout << "File has " << index.getNumIndexes() << " indexes\n";
         * } catch (const DataCorruptionException& e) {
         *     std::cerr << "File corrupted!\n";
         * }
         * ```
         */
        explicit MultiIndex(const std::string& filename);

        /**
         * @brief Destructor - writes all data and closes file
         *
         * Ensures header and index info are written before closing.
         * Errors are caught and ignored to maintain no-throw guarantee.
         */
        ~MultiIndex() override;

        //=========================================================================
        // Index Configuration
        //=========================================================================

        /**
         * @brief Initialize an index
         *
         * Sets up an index with the specified configuration. Must be called
         * once for each index after creating a new file.
         *
         * ## Parameters Explained
         *
         * - **keyType**: How keys are compared (see KeyType enum)
         * - **keySize**: Maximum key size in bytes
         * - **attributes**: UNIQUE, ALLOW_DELETE flags
         * - **numItems**: Keys per node (B-Tree order/fanout)
         * - **freeCreateNodes**: Nodes to pre-allocate at a time
         * - **freeCreateLeaves**: Leaves to pre-allocate at a time
         *
         * ## Key Size Guidelines
         *
         * | Key Type | Recommended Size |
         * |----------|------------------|
         * | INTEGER | 2 (fixed) |
         * | LONG_INT | 4 (fixed) |
         * | STRING | max length + 1 for null |
         * | BLOCK | exact size |
         *
         * @param keyType Type of keys for this index
         * @param keySize Size of keys in bytes
         * @param attributes Index attributes (can combine with |)
         * @param numItems Maximum items per node (B-Tree order)
         * @param freeCreateNodes Number of nodes to pre-allocate
         * @param freeCreateLeaves Number of leaves to pre-allocate
         *
         * @note Only call on newly created files. Re-initializing destroys data!
         */
        void initIndex(KeyType keyType, uint16_t keySize, IndexAttribute attributes,
            uint16_t numItems, int64_t freeCreateNodes, int64_t freeCreateLeaves);

        /**
         * @brief Set the active index for operations
         *
         * All subsequent operations (append, find, delete, navigate) operate
         * on the active index. Index numbers are 1-based.
         *
         * @param indexNo Index number (1 to numIndexes)
         *
         * @example
         * ```cpp
         * index.setActiveIndex(1);  // Work with first index
         * index.append("key", 100);
         *
         * index.setActiveIndex(2);  // Switch to second index
         * index.find("other");
         * ```
         */
        void setActiveIndex(uint16_t indexNo);

        /**
         * @brief Get the active index number
         * @return Active index number (1-based)
         */
        uint16_t getActiveIndex() const { return m_currentIndex + 1; }

        /**
         * @brief Get total number of indexes in file
         */
        uint16_t getNumIndexes() const { return m_header.numIndexes; }

        /**
         * @brief Get key type of current index
         */
        KeyType getKeyType() const;

        /**
         * @brief Get key size of current index
         */
        uint16_t getKeySize() const;

        /**
         * @brief Check if delete is allowed on current index
         * @return true if ALLOW_DELETE attribute is set
         */
        bool canDelete() const;

        /**
         * @brief Check if current index requires unique keys
         * @return true if UNIQUE attribute is set
         */
        bool isUnique() const;

        //=========================================================================
        // Key Operations
        //=========================================================================

        /**
         * @brief Append a new key-data pair
         *
         * Inserts a key into the index, pointing to the specified data position.
         *
         * ## Algorithm Overview
         *
         * 1. Find path from root to appropriate leaf position
         * 2. Create new leaf with key and data position
         * 3. Link leaf into the leaf chain
         * 4. Insert key into lowest-level node
         * 5. If node overflows, split it
         * 6. Propagate splits up the tree as needed
         * 7. If root splits, create new root
         *
         * ## Duplicate Keys
         *
         * - If UNIQUE is set: Returns false if key exists
         * - If UNIQUE is not set: Allows multiple entries with same key
         *
         * @param key Pointer to key data (must be keySize bytes)
         * @param dataPos Position of the data record
         * @return true if successful, false if failed (e.g., duplicate in unique index)
         *
         * @example
         * ```cpp
         * // String key
         * index.append("Alice", 1000);
         *
         * // Integer key
         * int32_t id = 42;
         * index.append(&id, 2000);
         * ```
         */
        bool append(const void* key, int64_t dataPos);

        /**
         * @brief Find a key
         *
         * Searches the index for the specified key. If found, positions
         * the cursor at the entry and returns the data position.
         *
         * ## Algorithm
         *
         * 1. Start at root node
         * 2. Binary search within node to find correct child
         * 3. Follow child pointer
         * 4. Repeat until reaching leaf level
         * 5. If leaf key matches, return its data position
         *
         * ## Time Complexity: O(log n)
         *
         * @param key Pointer to key to find
         * @return Data position, or -1 if not found
         *
         * @note Even if key not found, cursor is positioned at nearest entry.
         *       This enables "find nearest" functionality.
         */
        int64_t find(const void* key);

        /**
         * @brief Delete all entries with a key
         *
         * Removes all entries matching the key (for non-unique indexes,
         * there may be multiple).
         *
         * ## Requirements
         *
         * - ALLOW_DELETE attribute must be set
         * - Key must exist in index
         *
         * ## Algorithm Overview
         *
         * 1. Find path to key
         * 2. Remove key from node
         * 3. Delete associated leaves
         * 4. Rebalance tree if needed (borrow from siblings or merge)
         * 5. Reduce tree height if root becomes empty
         *
         * @param key Pointer to key to delete
         * @return true if any entries were deleted
         */
        bool deleteKey(const void* key);

        /**
         * @brief Delete the current entry
         *
         * Deletes the entry at the current cursor position.
         *
         * @return Data position of deleted entry, or -1 if none
         */
        int64_t deleteCurrent();

        //=========================================================================
        // Navigation
        //=========================================================================

        /**
         * @brief Move to first entry
         *
         * Positions cursor at the first (smallest key) entry in the index.
         * The leaf chain makes this O(1) - just follow firstLeave pointer.
         *
         * @param key Buffer to receive the key (optional, can be nullptr)
         * @return Data position, or -1 if empty
         *
         * @example
         * ```cpp
         * char key[51];
         * int64_t pos = index.getFirst(key);
         * if (pos >= 0) {
         *     std::cout << "First key: " << key << std::endl;
         * }
         * ```
         */
        int64_t getFirst(void* key = nullptr);

        /**
         * @brief Move to next entry
         *
         * Advances cursor to the next entry in key order.
         * Uses the leaf's nextLeave pointer for O(1) operation.
         *
         * @param key Buffer to receive the key (optional)
         * @return Data position, or -1 if at end
         */
        int64_t getNext(void* key = nullptr);

        /**
         * @brief Move to previous entry
         *
         * Moves cursor to the previous entry in key order.
         * Uses the leaf's prevLeave pointer for O(1) operation.
         *
         * @param key Buffer to receive the key (optional)
         * @return Data position, or -1 if at beginning
         */
        int64_t getPrev(void* key = nullptr);

        /**
         * @brief Get current entry
         *
         * Returns data for the current cursor position without moving.
         *
         * @param key Buffer to receive the key (optional)
         * @return Data position, or -1 if no current entry
         */
        int64_t getCurrent(void* key = nullptr);

        /**
         * @brief Check if at end of index
         * @return true if no more entries forward
         */
        bool isEOF() const;

        /**
         * @brief Check if at beginning of index
         * @return true if no more entries backward
         */
        bool isBOF() const;

        //=========================================================================
        // Maintenance
        //=========================================================================

        /**
         * @brief Flush current index info to disk
         */
        void flushIndex();

        /**
         * @brief Flush all index info to disk
         */
        void flushFile();

        /**
         * @brief Compare two keys
         *
         * Virtual to allow derived classes to implement custom comparison.
         * The default implementation handles all KeyType values.
         *
         * @param key1 First key
         * @param key2 Second key
         * @return -1 if key1 < key2, 0 if equal, 1 if key1 > key2
         */
        virtual int compare(const void* key1, const void* key2) const;

    protected:
        //=========================================================================
        // Internal Helper Methods
        //=========================================================================

        // Size calculations
        uint16_t getNodeHeaderSize() const { return sizeof(NodeHeader); }
        uint16_t getMaxItems() const;
        uint16_t getItemSize() const;
        uint16_t getNodeSize() const;
        uint16_t getLeaveSize() const;
        int64_t getIndexInfoPos() const;

        // Memory management
        std::vector<uint8_t> allocateNodeBlock() const;
        std::vector<uint8_t> allocateLeaveBlock() const;
        std::vector<uint8_t> allocateKeyBlock() const;

        // Checksum operations
        void setHeaderChecksum();
        bool testHeaderChecksum() const;
        void setInfoChecksum(uint16_t indexNo);
        bool testInfoChecksum(uint16_t indexNo) const;
        void setNodeChecksum(std::vector<uint8_t>& node) const;
        bool testNodeChecksum(const std::vector<uint8_t>& node) const;
        void setLeaveChecksum(std::vector<uint8_t>& leave) const;
        bool testLeaveChecksum(const std::vector<uint8_t>& leave) const;

        // File I/O
        void writeHeader();
        void readHeader();
        void writeInfo();
        void readInfo();
        void writeAllInfo();
        void readAllInfo();
        void readNode(std::vector<uint8_t>& node, int64_t pos);
        void writeNode(std::vector<uint8_t>& node, int64_t pos);
        int64_t writeNewNode(std::vector<uint8_t>& node);
        void readLeave(std::vector<uint8_t>& leave, int64_t pos);
        void writeLeave(std::vector<uint8_t>& leave, int64_t pos);
        int64_t writeNewLeave(std::vector<uint8_t>& leave);

        // Node/Leave management
        void createNodes(int64_t numNodes);
        void createLeaves(int64_t numLeaves);
        int64_t allocateNode();
        int64_t allocateLeave();
        void freeNode(int64_t nodePos);
        void freeLeave(int64_t leavePos);
        void createFirstNode();

        // Node access helpers
        void resetNode(std::vector<uint8_t>& node);
        void resetLeave(std::vector<uint8_t>& leave);
        uint16_t getNumItems(const std::vector<uint8_t>& node) const;
        void setNumItems(std::vector<uint8_t>& node, uint16_t num);
        void* getNodeKey(std::vector<uint8_t>& node, uint16_t itemNo);
        const void* getNodeKey(const std::vector<uint8_t>& node, uint16_t itemNo) const;
        void setNodeKey(std::vector<uint8_t>& node, uint16_t itemNo, const void* key);
        void* getLeaveKey(std::vector<uint8_t>& leave);
        const void* getLeaveKey(const std::vector<uint8_t>& leave) const;
        void setLeaveKey(std::vector<uint8_t>& leave, const void* key);
        void fillEOFKey(void* keyBlock) const;

        int64_t getChildPos(const std::vector<uint8_t>& node, uint16_t itemNo) const;
        void setChildPos(std::vector<uint8_t>& node, uint16_t itemNo, int64_t pos);
        int64_t getNextNode(const std::vector<uint8_t>& node) const;
        void setNextNode(std::vector<uint8_t>& node, int64_t pos);
        int64_t getPrevNode(const std::vector<uint8_t>& node) const;
        void setPrevNode(std::vector<uint8_t>& node, int64_t pos);
        int64_t getNextLeave(const std::vector<uint8_t>& leave) const;
        void setNextLeave(std::vector<uint8_t>& leave, int64_t pos);
        int64_t getPrevLeave(const std::vector<uint8_t>& leave) const;
        void setPrevLeave(std::vector<uint8_t>& leave, int64_t pos);
        int64_t getDataPos(const std::vector<uint8_t>& leave) const;
        void setDataPos(std::vector<uint8_t>& leave, int64_t pos);

        void insertItem(std::vector<uint8_t>& node, uint16_t itemNo, const void* key, int64_t childPos);
        void deleteItem(std::vector<uint8_t>& node, uint16_t itemNo);

        // Position management
        void resetPosition();
        void setPosition(int64_t currentLeave, int64_t nextLeave, int64_t prevLeave, int64_t dataPos);
        void setPositionFromLeave(int64_t leavePos, const std::vector<uint8_t>& leave);
        void setEOF();
        void resetEOF();
        void setBOF();
        void resetBOF();

        // Tree algorithms
        bool findPath(const void* key, IndexStack& stack, int64_t& lastLevelChild);
        bool findLeave(const void* key, int64_t& leavePos);
        int64_t modifyLeave(const void* key, int64_t newLeavePos);
        int64_t bringLeave(int64_t leavePos, void* key);
        int64_t deleteKeyFromNodes(const void* deleteKey);

        // Insert/Remove helpers
        int insertKey(int64_t nodePos, const void* newKey, int64_t newChildPos,
            uint16_t changedKeyNo, const void* changedKeyVal,
            std::vector<uint8_t>& parentKey, std::vector<uint8_t>& additionalKey,
            int64_t& additionalChildPos);
        int removeKey(int64_t nodePos, uint16_t removeKeyNo, std::vector<uint8_t>& parentKey);

    private:
        MultiIndexHeader m_header;              ///< File header (cached)
        std::vector<IndexInfo> m_indexInfo;     ///< All index info (cached)
        std::vector<PositionInfo> m_positions;  ///< Cursor positions per index
        uint16_t m_currentIndex = 0;            ///< Active index (0-based internally)

        // Index property accessors
        int64_t getFreeNode() const;
        void setFreeNode(int64_t pos);
        int64_t getFreeLeave() const;
        void setFreeLeave(int64_t pos);
        int64_t getRootNode() const;
        void setRootNode(int64_t pos);
        int64_t getFirstLeave() const;
        void setFirstLeave(int64_t pos);
        int64_t getLastLeave() const;
        void setLastLeave(int64_t pos);
        uint16_t getNumLevels() const;
        void setNumLevels(uint16_t num);
        uint16_t incNumLevels();
        uint16_t decNumLevels();
        int64_t getFreeCreateNodes() const;
        int64_t getFreeCreateLeaves() const;
    };

} // namespace udb

#endif // UDB_BTREE_H
