/**
 * @file udb_btree.h
 * @brief B-Tree index implementation for the UDB library
 * 
 * This file implements a B-Tree (actually a B+ tree variant) index structure
 * that provides efficient key-based access to records. The implementation
 * supports multiple indexes per file with different key types.
 * 
 * ## B-Tree Structure
 * 
 * The B-Tree consists of:
 * 1. **Nodes**: Internal nodes containing keys and child pointers
 * 2. **Leaves**: Leaf nodes containing keys and data pointers
 * 
 * Unlike traditional B-Trees, this implementation stores data pointers
 * only in leaves, making it similar to a B+ tree.
 * 
 * ## File Layout
 * 
 * ```
 * [Header][IndexInfo 0][IndexInfo 1]...[Nodes/Leaves]
 * ```
 * 
 * Each index has its own tree structure within the same file.
 * 
 * @author Digixoil
 * @version 2.0.0 (Modernized for Visual Studio 2025)
 * @date 2025
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

#pragma pack(push, 1)

/**
 * @brief Multi-index file header
 */
struct MultiIndexHeader {
    uint8_t checksum;               ///< XOR checksum for integrity
    uint16_t numIndexes;            ///< Number of indexes in file
};

/**
 * @brief Information about a single index
 */
struct IndexInfo {
    uint8_t checksum;               ///< XOR checksum for integrity
    uint16_t attributes;            ///< Index attributes (IndexAttribute flags)
    uint16_t keyType;               ///< Key type (KeyType enum)
    uint16_t keySize;               ///< Size of key in bytes
    uint16_t maxItems;              ///< Maximum items per node (B-Tree order)
    int64_t freeCreateNodes;        ///< Number of nodes to pre-create
    int64_t freeCreateLeaves;       ///< Number of leaves to pre-create
    
    int64_t freeNode;               ///< First free node position
    int64_t freeLeave;              ///< First free leaf position
    uint16_t numLevels;             ///< Current number of tree levels
    int64_t rootNode;               ///< Position of root node
    int64_t firstLeave;             ///< Position of first (leftmost) leaf
    int64_t lastLeave;              ///< Position of last (rightmost) leaf - EOF marker
};

/**
 * @brief Node header structure (internal nodes)
 * 
 * A node contains:
 * - Header (this structure)
 * - Array of (key, childPointer) pairs
 */
struct NodeHeader {
    uint8_t checksum;               ///< XOR checksum for integrity
    uint16_t numUsed;               ///< Number of items currently in use
    int64_t nextNode;               ///< Next node at same level (for traversal)
    int64_t prevNode;               ///< Previous node at same level
    // Followed by: [Key0][ChildPtr0][Key1][ChildPtr1]...
};

/**
 * @brief Leaf header structure
 * 
 * A leaf contains:
 * - Header (this structure)
 * - Key data
 */
struct LeafHeader {
    uint8_t checksum;               ///< XOR checksum for integrity
    int64_t nextLeave;              ///< Next leaf (for sequential access)
    int64_t prevLeave;              ///< Previous leaf
    int64_t dataPos;                ///< Position of actual data record
    // Followed by key data
};

/**
 * @brief Current position state for navigation
 */
struct PositionInfo {
    uint16_t state;                 ///< State flags (PositionState)
    int64_t currentLeave;           ///< Current leaf position
    int64_t nextLeave;              ///< Next leaf position
    int64_t prevLeave;              ///< Previous leaf position
    int64_t currentDataPos;         ///< Current data position
};

#pragma pack(pop)

//=============================================================================
// Navigation Stack
//=============================================================================

/**
 * @brief Stack item for tree traversal
 */
struct StackItem {
    int64_t nodePos;                ///< Node position in file
    uint16_t childNo;               ///< Child number within node
};

/**
 * @brief Stack for tracking path through B-Tree
 * 
 * Used during insert and delete operations to track the path
 * from root to leaf, enabling backtracking for node splitting/merging.
 */
class IndexStack {
public:
    void push(int64_t nodePos, uint16_t childNo);
    bool pop(int64_t& nodePos, uint16_t& childNo);
    bool top(int64_t& nodePos, uint16_t& childNo) const;
    bool empty() const { return m_stack.empty(); }
    void clear() { m_stack.clear(); }
    
private:
    std::stack<StackItem> m_stack;
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
 * - Multiple indexes per file
 * - Supports various key types (strings, integers, etc.)
 * - Sequential and random access
 * - Optional duplicate keys
 * - Optional key deletion with node balancing
 * 
 * ## Usage Example
 * 
 * ```cpp
 * // Create a new index file with 2 indexes
 * MultiIndex idx("data.ndx", 2);
 * 
 * // Initialize first index for strings
 * idx.setActiveIndex(1);
 * idx.initIndex(KeyType::STRING, 50, IndexAttribute::ALLOW_DELETE, 5, 100, 200);
 * 
 * // Add a key
 * idx.append("Hello", 12345);  // Key "Hello", data at position 12345
 * 
 * // Find a key
 * int64_t dataPos = idx.find("Hello");
 * ```
 * 
 * ## Thread Safety
 * 
 * Not thread-safe. External synchronization required.
 */
class UDB_API MultiIndex : public File {
public:
    /**
     * @brief Create a new multi-index file
     * 
     * @param filename Path to the file
     * @param numIndexes Number of indexes to create
     */
    MultiIndex(const std::string& filename, uint16_t numIndexes);
    
    /**
     * @brief Open an existing multi-index file
     * 
     * @param filename Path to the file
     */
    explicit MultiIndex(const std::string& filename);
    
    /**
     * @brief Destructor - writes all data and closes file
     */
    ~MultiIndex() override;
    
    //=========================================================================
    // Index Configuration
    //=========================================================================
    
    /**
     * @brief Initialize an index
     * 
     * @param keyType Type of keys for this index
     * @param keySize Size of keys in bytes
     * @param attributes Index attributes
     * @param numItems Maximum items per node (B-Tree order)
     * @param freeCreateNodes Number of nodes to pre-allocate
     * @param freeCreateLeaves Number of leaves to pre-allocate
     */
    void initIndex(KeyType keyType, uint16_t keySize, IndexAttribute attributes,
                   uint16_t numItems, int64_t freeCreateNodes, int64_t freeCreateLeaves);
    
    /**
     * @brief Set the active index for operations
     * 
     * @param indexNo Index number (1-based)
     */
    void setActiveIndex(uint16_t indexNo);
    
    /**
     * @brief Get the active index number
     * @return Active index number (1-based)
     */
    uint16_t getActiveIndex() const { return m_currentIndex + 1; }
    
    /**
     * @brief Get total number of indexes
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
     */
    bool canDelete() const;
    
    /**
     * @brief Check if current index requires unique keys
     */
    bool isUnique() const;
    
    //=========================================================================
    // Key Operations
    //=========================================================================
    
    /**
     * @brief Append a new key-data pair
     * 
     * @param key Pointer to key data
     * @param dataPos Position of the data record
     * @return true if successful
     */
    bool append(const void* key, int64_t dataPos);
    
    /**
     * @brief Find a key
     * 
     * @param key Pointer to key to find
     * @return Data position, or -1 if not found
     */
    int64_t find(const void* key);
    
    /**
     * @brief Delete all entries with a key
     * 
     * @param key Pointer to key to delete
     * @return true if any entries were deleted
     */
    bool deleteKey(const void* key);
    
    /**
     * @brief Delete the current entry
     * @return Data position of deleted entry, or -1 if none
     */
    int64_t deleteCurrent();
    
    //=========================================================================
    // Navigation
    //=========================================================================
    
    /**
     * @brief Move to first entry
     * 
     * @param key Buffer to receive the key (optional)
     * @return Data position, or -1 if empty
     */
    int64_t getFirst(void* key = nullptr);
    
    /**
     * @brief Move to next entry
     * 
     * @param key Buffer to receive the key (optional)
     * @return Data position, or -1 if at end
     */
    int64_t getNext(void* key = nullptr);
    
    /**
     * @brief Move to previous entry
     * 
     * @param key Buffer to receive the key (optional)
     * @return Data position, or -1 if at beginning
     */
    int64_t getPrev(void* key = nullptr);
    
    /**
     * @brief Get current entry
     * 
     * @param key Buffer to receive the key (optional)
     * @return Data position, or -1 if none
     */
    int64_t getCurrent(void* key = nullptr);
    
    /**
     * @brief Check if at end of index
     */
    bool isEOF() const;
    
    /**
     * @brief Check if at beginning of index
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
    MultiIndexHeader m_header;
    std::vector<IndexInfo> m_indexInfo;
    std::vector<PositionInfo> m_positions;
    uint16_t m_currentIndex = 0;
    
    // Cached index properties for current index
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
