/**
 * @file udb_btree.cpp
 * @brief Implementation of the B-Tree index for the UDB library
 *
 * This file contains the complete implementation of the B-Tree index
 * including all insert, delete, search, and navigation operations.
 *
 * @author Digixoil
 * @version 2.0.0
 */

#include "udb_btree.h"
#include <algorithm>
#include <cstring>

namespace udb {

    //=============================================================================
    // Binary Search Helper
    //=============================================================================

    /**
     * @brief Binary search within a node to find insertion point
     * 
     * Finds the position where key should be inserted or where it exists.
     * For B-tree semantics, we need to find the first key >= search key.
     * 
     * @param node The node to search
     * @param key The key to find
     * @param numItems Number of items in the node
     * @return Pair of (position, exactMatch) where position is 1-based
     */
    inline std::pair<uint16_t, bool> MultiIndex::binarySearchNode(
        const std::vector<uint8_t>& node, const void* key, uint16_t numItems) const
    {
        // Note: This is an internal method - caller must hold lock
        if (numItems == 0) {
            return { 1, false };
        }

        uint16_t left = 1;
        uint16_t right = numItems;
        
        // Binary search to find the first position where nodeKey >= key
        while (left < right) {
            uint16_t mid = left + (right - left) / 2;
            int cmp = compare(key, getNodeKey(node, mid));
            
            if (cmp <= 0) {
                // key <= nodeKey[mid], search in left half (including mid)
                right = mid;
            } else {
                // key > nodeKey[mid], search in right half (excluding mid)
                left = mid + 1;
            }
        }
        
        // At this point, left is the position where key should be inserted
        // Check if it's an exact match
        int finalCmp = compare(key, getNodeKey(node, left));
        
        return { left, finalCmp == 0 };
    }

    //=============================================================================
    // IndexStack Implementation
    //=============================================================================

    void IndexStack::push(int64_t nodePos, uint16_t childNo)
    {
        m_stack.push({ nodePos, childNo });
    }

    bool IndexStack::pop(int64_t& nodePos, uint16_t& childNo)
    {
        if (m_stack.empty()) {
            return false;
        }
        StackItem item = m_stack.top();
        m_stack.pop();
        nodePos = item.nodePos;
        childNo = item.childNo;
        return true;
    }

    bool IndexStack::top(int64_t& nodePos, uint16_t& childNo) const
    {
        if (m_stack.empty()) {
            return false;
        }
        nodePos = m_stack.top().nodePos;
        childNo = m_stack.top().childNo;
        return true;
    }

    //=============================================================================
    // MultiIndex Constructor / Destructor
    //=============================================================================

    MultiIndex::MultiIndex(const std::string& filename, uint16_t numIndexes)
        : File(filename, true)
        , m_indexInfo(numIndexes)
        , m_positions(numIndexes)
    {
        // No lock needed - object is being constructed, not yet shared
        m_currentIndex = 0;
        m_header.numIndexes = numIndexes;

        // Initialize all indexes
        for (uint16_t i = 0; i < numIndexes; ++i) {
            m_indexInfo[i].attributes = 0;
            m_indexInfo[i].keyType = 0;
            m_indexInfo[i].keySize = 0;
            m_indexInfo[i].maxItems = 0;
            m_indexInfo[i].freeCreateNodes = 0;
            m_indexInfo[i].freeCreateLeaves = 0;
            m_indexInfo[i].freeNode = INVALID_POSITION;
            m_indexInfo[i].freeLeave = INVALID_POSITION;
            m_indexInfo[i].numLevels = 0;
            m_indexInfo[i].rootNode = INVALID_POSITION;
            m_indexInfo[i].firstLeave = INVALID_POSITION;
            m_indexInfo[i].lastLeave = INVALID_POSITION;

            m_positions[i].currentLeave = INVALID_POSITION;
            m_positions[i].nextLeave = INVALID_POSITION;
            m_positions[i].prevLeave = INVALID_POSITION;
            m_positions[i].currentDataPos = INVALID_POSITION;
            m_positions[i].state = 0;
        }

        writeHeader();
        writeAllInfo();
    }

    MultiIndex::MultiIndex(const std::string& filename)
        : File(filename, false)
    {
        // No lock needed - object is being constructed, not yet shared
        readHeader();
        m_currentIndex = 0;
        m_indexInfo.resize(m_header.numIndexes);
        m_positions.resize(m_header.numIndexes);

        // Initialize positions
        for (uint16_t i = 0; i < m_header.numIndexes; ++i) {
            m_positions[i].currentLeave = INVALID_POSITION;
            m_positions[i].nextLeave = INVALID_POSITION;
            m_positions[i].prevLeave = INVALID_POSITION;
            m_positions[i].currentDataPos = INVALID_POSITION;
            m_positions[i].state = 0;
        }

        readAllInfo();
    }

    MultiIndex::~MultiIndex()
    {
        // Acquire lock to ensure no other operations are in progress
        LockGuard lock(m_indexMutex);
        try {
            writeHeader();
            writeAllInfo();
        }
        catch (...) {
            // Ignore errors in destructor
        }
    }

    //=============================================================================
    // Header Operations
    //=============================================================================

    void MultiIndex::setHeaderChecksum()
    {
        m_header.checksum = 0;
        m_header.checksum = calculateBlockChecksum(&m_header, sizeof(MultiIndexHeader));
    }

    bool MultiIndex::testHeaderChecksum() const
    {
        return calculateBlockChecksum(&m_header, sizeof(MultiIndexHeader)) == 0;
    }

    void MultiIndex::writeHeader()
    {
        setHeaderChecksum();
        write(&m_header, sizeof(MultiIndexHeader), 0);
    }

    void MultiIndex::readHeader()
    {
        read(&m_header, sizeof(MultiIndexHeader), 0);
        if (!testHeaderChecksum()) {
            setError(ErrorCode::BAD_DATA);
            throw DataCorruptionException("Index file header checksum mismatch");
        }
    }

    void MultiIndex::setInfoChecksum(uint16_t indexNo)
    {
        m_indexInfo[indexNo].checksum = 0;
        m_indexInfo[indexNo].checksum = calculateBlockChecksum(
            &m_indexInfo[indexNo], sizeof(IndexInfo));
    }

    bool MultiIndex::testInfoChecksum(uint16_t indexNo) const
    {
        return calculateBlockChecksum(&m_indexInfo[indexNo], sizeof(IndexInfo)) == 0;
    }

    int64_t MultiIndex::getIndexInfoPos() const
    {
        return sizeof(MultiIndexHeader) + m_currentIndex * sizeof(IndexInfo);
    }

    void MultiIndex::writeInfo()
    {
        setInfoChecksum(m_currentIndex);
        write(&m_indexInfo[m_currentIndex], sizeof(IndexInfo), getIndexInfoPos());
    }

    void MultiIndex::readInfo()
    {
        read(&m_indexInfo[m_currentIndex], sizeof(IndexInfo), getIndexInfoPos());
        if (!testInfoChecksum(m_currentIndex)) {
            setError(ErrorCode::BAD_DATA);
            throw DataCorruptionException("Index info checksum mismatch");
        }
    }

    void MultiIndex::writeAllInfo()
    {
        for (uint16_t i = 0; i < m_header.numIndexes; ++i) {
            setInfoChecksum(i);
        }
        write(m_indexInfo.data(), sizeof(IndexInfo) * m_header.numIndexes,
            sizeof(MultiIndexHeader));
    }

    void MultiIndex::readAllInfo()
    {
        read(m_indexInfo.data(), sizeof(IndexInfo) * m_header.numIndexes,
            sizeof(MultiIndexHeader));
        for (uint16_t i = 0; i < m_header.numIndexes; ++i) {
            if (!testInfoChecksum(i)) {
                setError(ErrorCode::BAD_DATA);
                throw DataCorruptionException("Index info checksum mismatch");
            }
        }
    }

    //=============================================================================
    // Index Configuration
    //=============================================================================

    void MultiIndex::initIndex(KeyType keyType, uint16_t keySize, IndexAttribute attributes,
        uint16_t numItems, int64_t freeCreateNodes, int64_t freeCreateLeaves)
    {
        LockGuard lock(m_indexMutex);
        if (hasError()) return;

        m_indexInfo[m_currentIndex].attributes = static_cast<uint16_t>(attributes);
        m_indexInfo[m_currentIndex].keyType = static_cast<uint16_t>(keyType);
        m_indexInfo[m_currentIndex].keySize = keySize;
        m_indexInfo[m_currentIndex].maxItems = numItems;
        m_indexInfo[m_currentIndex].freeCreateNodes = freeCreateNodes;
        m_indexInfo[m_currentIndex].freeCreateLeaves = freeCreateLeaves;
        m_indexInfo[m_currentIndex].freeNode = INVALID_POSITION;
        m_indexInfo[m_currentIndex].freeLeave = INVALID_POSITION;
        m_indexInfo[m_currentIndex].numLevels = 0;
        m_indexInfo[m_currentIndex].rootNode = INVALID_POSITION;

        writeInfo();
        createNodes(freeCreateNodes);
        createLeaves(freeCreateLeaves);
        createFirstNode();
        setNumLevels(1);
    }

    void MultiIndex::setActiveIndex(uint16_t indexNo)
    {
        LockGuard lock(m_indexMutex);
        if (hasError()) return;
        if (indexNo > 0 && indexNo <= m_header.numIndexes) {
            m_currentIndex = indexNo - 1;
        }
        else {
            m_currentIndex = 0;
        }
    }

    uint16_t MultiIndex::getActiveIndex() const
    {
        LockGuard lock(m_indexMutex);
        return m_currentIndex + 1;  // Return 1-based index
    }

    KeyType MultiIndex::getKeyType() const
    {
        LockGuard lock(m_indexMutex);
        if (hasError()) return KeyType::VOID;
        return static_cast<KeyType>(m_indexInfo[m_currentIndex].keyType);
    }

    uint16_t MultiIndex::getKeySize() const
    {
        LockGuard lock(m_indexMutex);
        if (hasError()) return 0;
        return m_indexInfo[m_currentIndex].keySize;
    }

    bool MultiIndex::canDelete() const
    {
        LockGuard lock(m_indexMutex);
        if (hasError()) return false;
        return (m_indexInfo[m_currentIndex].attributes &
            static_cast<uint16_t>(IndexAttribute::ALLOW_DELETE)) != 0;
    }

    bool MultiIndex::isUnique() const
    {
        LockGuard lock(m_indexMutex);
        if (hasError()) return false;
        return (m_indexInfo[m_currentIndex].attributes &
            static_cast<uint16_t>(IndexAttribute::UNIQUE)) != 0;
    }

    void MultiIndex::flushIndex()
    {
        LockGuard lock(m_indexMutex);
        if (hasError()) return;
        writeHeader();
    }

    void MultiIndex::flushFile()
    {
        LockGuard lock(m_indexMutex);
        if (hasError()) return;
        writeAllInfo();
    }

    //=============================================================================
    // Size Calculations
    //=============================================================================

    uint16_t MultiIndex::getMaxItems() const
    {
        return m_indexInfo[m_currentIndex].maxItems;
    }

    uint16_t MultiIndex::getItemSize() const
    {
        // Key size + child pointer (int64_t)
        return m_indexInfo[m_currentIndex].keySize + sizeof(int64_t);
    }

    uint16_t MultiIndex::getNodeSize() const
    {
        return getItemSize() * getMaxItems() + getNodeHeaderSize();
    }

    uint16_t MultiIndex::getLeaveSize() const
    {
        return m_indexInfo[m_currentIndex].keySize + sizeof(LeafHeader);
    }

    //=============================================================================
    // Memory Allocation
    //=============================================================================

    std::vector<uint8_t> MultiIndex::allocateNodeBlock() const
    {
        return std::vector<uint8_t>(getNodeSize(), 0);
    }

    std::vector<uint8_t> MultiIndex::allocateLeaveBlock() const
    {
        return std::vector<uint8_t>(getLeaveSize(), 0);
    }

    std::vector<uint8_t> MultiIndex::allocateKeyBlock() const
    {
        return std::vector<uint8_t>(getKeySize(), 0);
    }

    //=============================================================================
    // Index Property Accessors
    //=============================================================================

    int64_t MultiIndex::getFreeNode() const { return m_indexInfo[m_currentIndex].freeNode; }
    void MultiIndex::setFreeNode(int64_t pos) { m_indexInfo[m_currentIndex].freeNode = pos; }
    int64_t MultiIndex::getFreeLeave() const { return m_indexInfo[m_currentIndex].freeLeave; }
    void MultiIndex::setFreeLeave(int64_t pos) { m_indexInfo[m_currentIndex].freeLeave = pos; }
    int64_t MultiIndex::getRootNode() const { return m_indexInfo[m_currentIndex].rootNode; }
    void MultiIndex::setRootNode(int64_t pos) { m_indexInfo[m_currentIndex].rootNode = pos; }
    int64_t MultiIndex::getFirstLeave() const { return m_indexInfo[m_currentIndex].firstLeave; }
    void MultiIndex::setFirstLeave(int64_t pos) { m_indexInfo[m_currentIndex].firstLeave = pos; }
    int64_t MultiIndex::getLastLeave() const { return m_indexInfo[m_currentIndex].lastLeave; }
    void MultiIndex::setLastLeave(int64_t pos) { m_indexInfo[m_currentIndex].lastLeave = pos; }
    uint16_t MultiIndex::getNumLevels() const { return m_indexInfo[m_currentIndex].numLevels; }
    void MultiIndex::setNumLevels(uint16_t num) { m_indexInfo[m_currentIndex].numLevels = num; }
    uint16_t MultiIndex::incNumLevels() { return ++m_indexInfo[m_currentIndex].numLevels; }
    uint16_t MultiIndex::decNumLevels() { return --m_indexInfo[m_currentIndex].numLevels; }
    int64_t MultiIndex::getFreeCreateNodes() const { return m_indexInfo[m_currentIndex].freeCreateNodes; }
    int64_t MultiIndex::getFreeCreateLeaves() const { return m_indexInfo[m_currentIndex].freeCreateLeaves; }

    //=============================================================================
    // Node Checksum Operations
    //=============================================================================

    void MultiIndex::setNodeChecksum(std::vector<uint8_t>& node) const
    {
        auto* header = reinterpret_cast<NodeHeader*>(node.data());
        header->checksum = 0;
        header->checksum = calculateBlockChecksum(node.data(), getNodeSize());
    }

    bool MultiIndex::testNodeChecksum(const std::vector<uint8_t>& node) const
    {
        return calculateBlockChecksum(node.data(), getNodeSize()) == 0;
    }

    void MultiIndex::setLeaveChecksum(std::vector<uint8_t>& leave) const
    {
        auto* header = reinterpret_cast<LeafHeader*>(leave.data());
        header->checksum = 0;
        header->checksum = calculateBlockChecksum(leave.data(), getLeaveSize());
    }

    bool MultiIndex::testLeaveChecksum(const std::vector<uint8_t>& leave) const
    {
        return calculateBlockChecksum(leave.data(), getLeaveSize()) == 0;
    }

    //=============================================================================
    // Node/Leave I/O
    //=============================================================================

    void MultiIndex::readNode(std::vector<uint8_t>& node, int64_t pos)
    {
        node.resize(getNodeSize());
        read(node.data(), getNodeSize(), pos);
        if (!testNodeChecksum(node)) {
            setError(ErrorCode::BAD_DATA);
            throw DataCorruptionException("Node checksum mismatch");
        }
    }

    void MultiIndex::writeNode(std::vector<uint8_t>& node, int64_t pos)
    {
        setNodeChecksum(node);
        write(node.data(), getNodeSize(), pos);
    }

    int64_t MultiIndex::writeNewNode(std::vector<uint8_t>& node)
    {
        int64_t nodePos = allocateNode();
        writeNode(node, nodePos);
        return nodePos;
    }

    void MultiIndex::readLeave(std::vector<uint8_t>& leave, int64_t pos)
    {
        leave.resize(getLeaveSize());
        read(leave.data(), getLeaveSize(), pos);
        if (!testLeaveChecksum(leave)) {
            setError(ErrorCode::BAD_DATA);
            throw DataCorruptionException("Leave checksum mismatch");
        }
    }

    void MultiIndex::writeLeave(std::vector<uint8_t>& leave, int64_t pos)
    {
        setLeaveChecksum(leave);
        write(leave.data(), getLeaveSize(), pos);
    }

    int64_t MultiIndex::writeNewLeave(std::vector<uint8_t>& leave)
    {
        int64_t leavePos = allocateLeave();
        writeLeave(leave, leavePos);
        return leavePos;
    }

    //=============================================================================
    // Node/Leave Management
    //=============================================================================

    void MultiIndex::createNodes(int64_t numNodes)
    {
        if (numNodes <= 0) return;

        int64_t fileSize = size();
        int64_t firstCreatedNodePos = fileSize;

        auto tempNode = allocateNodeBlock();
        resetNode(tempNode);
        seek(fileSize);

        for (int64_t counter = 1; counter < numNodes; ++counter) {
            fileSize += getNodeSize();
            setNextNode(tempNode, fileSize);
            writeNode(tempNode, INVALID_POSITION);
        }

        setNextNode(tempNode, getFreeNode());
        writeNode(tempNode, INVALID_POSITION);
        setFreeNode(firstCreatedNodePos);
    }

    void MultiIndex::createLeaves(int64_t numLeaves)
    {
        if (numLeaves <= 0) return;

        int64_t fileSize = size();
        int64_t firstCreatedLeavePos = fileSize;

        auto tempLeave = allocateLeaveBlock();
        resetLeave(tempLeave);
        seek(fileSize);

        for (int64_t counter = 1; counter < numLeaves; ++counter) {
            fileSize += getLeaveSize();
            setNextLeave(tempLeave, fileSize);
            writeLeave(tempLeave, INVALID_POSITION);
        }

        setNextLeave(tempLeave, getFreeLeave());
        writeLeave(tempLeave, INVALID_POSITION);
        setFreeLeave(firstCreatedLeavePos);
    }

    int64_t MultiIndex::allocateNode()
    {
        int64_t nodePos = getFreeNode();
        if (nodePos == INVALID_POSITION) {
            createNodes(getFreeCreateNodes());
            nodePos = getFreeNode();
        }

        auto tempNode = allocateNodeBlock();
        readNode(tempNode, nodePos);
        setFreeNode(getNextNode(tempNode));

        return nodePos;
    }

    int64_t MultiIndex::allocateLeave()
    {
        int64_t leavePos = getFreeLeave();
        if (leavePos == INVALID_POSITION) {
            createLeaves(getFreeCreateLeaves());
            leavePos = getFreeLeave();
        }

        auto tempLeave = allocateLeaveBlock();
        readLeave(tempLeave, leavePos);
        setFreeLeave(getNextLeave(tempLeave));

        return leavePos;
    }

    void MultiIndex::freeNode(int64_t nodePos)
    {
        auto tempNode = allocateNodeBlock();
        resetNode(tempNode);
        setNextNode(tempNode, getFreeNode());
        writeNode(tempNode, nodePos);
        setFreeNode(nodePos);
    }

    void MultiIndex::freeLeave(int64_t leavePos)
    {
        auto tempLeave = allocateLeaveBlock();
        resetLeave(tempLeave);
        setNextLeave(tempLeave, getFreeLeave());
        writeLeave(tempLeave, leavePos);
        setFreeLeave(leavePos);
    }

    void MultiIndex::createFirstNode()
    {
        auto node = allocateNodeBlock();
        auto leave = allocateLeaveBlock();

        // Create EOF leaf
        fillEOFKey(getLeaveKey(leave));
        setNextLeave(leave, INVALID_POSITION);
        setPrevLeave(leave, INVALID_POSITION);
        setDataPos(leave, INVALID_POSITION);
        int64_t leavePos = writeNewLeave(leave);
        setFirstLeave(leavePos);
        setLastLeave(leavePos);

        // Create root node pointing to EOF leaf
        resetNode(node);
        setNumItems(node, 1);
        fillEOFKey(getNodeKey(node, 1));
        setChildPos(node, 1, leavePos);
        int64_t nodePos = writeNewNode(node);
        setRootNode(nodePos);
    }

    //=============================================================================
    // Node Access Helpers
    //=============================================================================

    void MultiIndex::resetNode(std::vector<uint8_t>& node)
    {
        std::fill(node.begin(), node.end(), 0);
        setNumItems(node, 0);
        setNextNode(node, INVALID_POSITION);
        setPrevNode(node, INVALID_POSITION);
    }

    void MultiIndex::resetLeave(std::vector<uint8_t>& leave)
    {
        std::fill(leave.begin(), leave.end(), 0);
        setNextLeave(leave, INVALID_POSITION);
        setPrevLeave(leave, INVALID_POSITION);
    }

    uint16_t MultiIndex::getNumItems(const std::vector<uint8_t>& node) const
    {
        return reinterpret_cast<const NodeHeader*>(node.data())->numUsed;
    }

    void MultiIndex::setNumItems(std::vector<uint8_t>& node, uint16_t num)
    {
        reinterpret_cast<NodeHeader*>(node.data())->numUsed = num;
    }

    void* MultiIndex::getNodeKey(std::vector<uint8_t>& node, uint16_t itemNo)
    {
        return node.data() + sizeof(NodeHeader) + (getItemSize() * (itemNo - 1));
    }

    const void* MultiIndex::getNodeKey(const std::vector<uint8_t>& node, uint16_t itemNo) const
    {
        return node.data() + sizeof(NodeHeader) + (getItemSize() * (itemNo - 1));
    }

    void MultiIndex::setNodeKey(std::vector<uint8_t>& node, uint16_t itemNo, const void* key)
    {
        std::memcpy(getNodeKey(node, itemNo), key, getKeySize());
    }

    void* MultiIndex::getLeaveKey(std::vector<uint8_t>& leave)
    {
        return leave.data() + sizeof(LeafHeader);
    }

    const void* MultiIndex::getLeaveKey(const std::vector<uint8_t>& leave) const
    {
        return leave.data() + sizeof(LeafHeader);
    }

    void MultiIndex::setLeaveKey(std::vector<uint8_t>& leave, const void* key)
    {
        std::memcpy(getLeaveKey(leave), key, getKeySize());
    }

    void MultiIndex::fillEOFKey(void* keyBlock) const
    {
        std::memset(keyBlock, 0xFF, getKeySize());

        // Special handling for certain key types
        KeyType kt = getKeyType();
        if (kt == KeyType::STRING) {
            // Ensure null terminator
            static_cast<char*>(keyBlock)[getKeySize() - 1] = 0;
        }
        else if (kt == KeyType::NUM_BLOCK || kt == KeyType::INTEGER) {
            // Clear sign bit for signed comparisons
            static_cast<uint8_t*>(keyBlock)[0] &= 0x7F;
        }
        else if (kt == KeyType::LONG_INT) {
            static_cast<uint8_t*>(keyBlock)[getKeySize() - 1] &= 0x7F;
        }
    }

    int64_t MultiIndex::getChildPos(const std::vector<uint8_t>& node, uint16_t itemNo) const
    {
        const void* ptr = node.data() + sizeof(NodeHeader) +
            (getItemSize() * (itemNo - 1)) + getKeySize();
        return *static_cast<const int64_t*>(ptr);
    }

    void MultiIndex::setChildPos(std::vector<uint8_t>& node, uint16_t itemNo, int64_t pos)
    {
        void* ptr = node.data() + sizeof(NodeHeader) +
            (getItemSize() * (itemNo - 1)) + getKeySize();
        *static_cast<int64_t*>(ptr) = pos;
    }

    int64_t MultiIndex::getNextNode(const std::vector<uint8_t>& node) const
    {
        return reinterpret_cast<const NodeHeader*>(node.data())->nextNode;
    }

    void MultiIndex::setNextNode(std::vector<uint8_t>& node, int64_t pos)
    {
        reinterpret_cast<NodeHeader*>(node.data())->nextNode = pos;
    }

    int64_t MultiIndex::getPrevNode(const std::vector<uint8_t>& node) const
    {
        return reinterpret_cast<const NodeHeader*>(node.data())->prevNode;
    }

    void MultiIndex::setPrevNode(std::vector<uint8_t>& node, int64_t pos)
    {
        reinterpret_cast<NodeHeader*>(node.data())->prevNode = pos;
    }

    int64_t MultiIndex::getNextLeave(const std::vector<uint8_t>& leave) const
    {
        return reinterpret_cast<const LeafHeader*>(leave.data())->nextLeave;
    }

    void MultiIndex::setNextLeave(std::vector<uint8_t>& leave, int64_t pos)
    {
        reinterpret_cast<LeafHeader*>(leave.data())->nextLeave = pos;
    }

    int64_t MultiIndex::getPrevLeave(const std::vector<uint8_t>& leave) const
    {
        return reinterpret_cast<const LeafHeader*>(leave.data())->prevLeave;
    }

    void MultiIndex::setPrevLeave(std::vector<uint8_t>& leave, int64_t pos)
    {
        reinterpret_cast<LeafHeader*>(leave.data())->prevLeave = pos;
    }

    int64_t MultiIndex::getDataPos(const std::vector<uint8_t>& leave) const
    {
        return reinterpret_cast<const LeafHeader*>(leave.data())->dataPos;
    }

    void MultiIndex::setDataPos(std::vector<uint8_t>& leave, int64_t pos)
    {
        reinterpret_cast<LeafHeader*>(leave.data())->dataPos = pos;
    }

    void MultiIndex::insertItem(std::vector<uint8_t>& node, uint16_t itemNo,
        const void* key, int64_t childPos)
    {
        if (itemNo == 0) return;

        uint16_t numItems = getNumItems(node);
        setNumItems(node, numItems + 1);

        uint16_t insertAt;
        if (itemNo >= numItems + 1) {
            insertAt = numItems + 1;
        }
        else {
            insertAt = itemNo;
            // Use memmove for efficient bulk shift
            uint16_t itemSize = getItemSize();
            uint8_t* dest = node.data() + sizeof(NodeHeader) + (itemSize * insertAt);
            uint8_t* src = node.data() + sizeof(NodeHeader) + (itemSize * (insertAt - 1));
            size_t bytesToMove = itemSize * (numItems - insertAt + 1);
            std::memmove(dest, src, bytesToMove);
        }

        setNodeKey(node, insertAt, key);
        setChildPos(node, insertAt, childPos);
    }

    void MultiIndex::deleteItem(std::vector<uint8_t>& node, uint16_t itemNo)
    {
        uint16_t numItems = getNumItems(node);

        if (itemNo < numItems && itemNo > 0) {
            // Use memmove for efficient bulk shift
            uint16_t itemSize = getItemSize();
            uint8_t* dest = node.data() + sizeof(NodeHeader) + (itemSize * (itemNo - 1));
            uint8_t* src = node.data() + sizeof(NodeHeader) + (itemSize * itemNo);
            size_t bytesToMove = itemSize * (numItems - itemNo);
            std::memmove(dest, src, bytesToMove);
        }

        if (numItems > 0) {
            setNumItems(node, numItems - 1);
        }
    }

    //=============================================================================
    // Position Management
    //=============================================================================

    void MultiIndex::resetPosition()
    {
        // Internal method - caller must hold lock
        m_positions[m_currentIndex].currentLeave = INVALID_POSITION;
        m_positions[m_currentIndex].nextLeave = INVALID_POSITION;
        m_positions[m_currentIndex].prevLeave = INVALID_POSITION;
        m_positions[m_currentIndex].currentDataPos = INVALID_POSITION;
        m_positions[m_currentIndex].state = 0;
    }

    void MultiIndex::setPosition(int64_t currentLeave, int64_t nextLeave,
        int64_t prevLeave, int64_t dataPos)
    {
        // Internal method - caller must hold lock
        m_positions[m_currentIndex].currentLeave = currentLeave;
        m_positions[m_currentIndex].nextLeave = nextLeave;
        m_positions[m_currentIndex].prevLeave = prevLeave;
        m_positions[m_currentIndex].currentDataPos = dataPos;

        // Update BOF/EOF flags
        if (prevLeave == INVALID_POSITION || currentLeave == getFirstLeave()) {
            setBOF();
        }
        else {
            resetBOF();
        }

        if (nextLeave == INVALID_POSITION || nextLeave == getLastLeave()) {
            setEOF();
        }
        else {
            resetEOF();
        }
    }

    void MultiIndex::setPositionFromLeave(int64_t leavePos, const std::vector<uint8_t>& leave)
    {
        // Internal method - caller must hold lock
        setPosition(leavePos, getNextLeave(leave), getPrevLeave(leave), getDataPos(leave));
    }

    void MultiIndex::setEOF()
    {
        // Internal method - caller must hold lock
        m_positions[m_currentIndex].state |= static_cast<uint16_t>(PositionState::END_OF_FILE);
    }

    void MultiIndex::resetEOF()
    {
        // Internal method - caller must hold lock
        m_positions[m_currentIndex].state &= ~static_cast<uint16_t>(PositionState::END_OF_FILE);
    }

    void MultiIndex::setBOF()
    {
        // Internal method - caller must hold lock
        m_positions[m_currentIndex].state |= static_cast<uint16_t>(PositionState::BEGIN_OF_FILE);
    }

    void MultiIndex::resetBOF()
    {
        // Internal method - caller must hold lock
        m_positions[m_currentIndex].state &= ~static_cast<uint16_t>(PositionState::BEGIN_OF_FILE);
    }

    bool MultiIndex::isEOFInternal() const
    {
        // Internal method - caller must hold lock
        if (hasError()) return true;
        return (m_positions[m_currentIndex].state &
            static_cast<uint16_t>(PositionState::END_OF_FILE)) != 0;
    }

    bool MultiIndex::isBOFInternal() const
    {
        // Internal method - caller must hold lock
        if (hasError()) return true;
        return (m_positions[m_currentIndex].state &
            static_cast<uint16_t>(PositionState::BEGIN_OF_FILE)) != 0;
    }

    bool MultiIndex::isEOF() const
    {
        LockGuard lock(m_indexMutex);
        return isEOFInternal();
    }

    bool MultiIndex::isBOF() const
    {
        LockGuard lock(m_indexMutex);
        return isBOFInternal();
    }

    //=============================================================================
    // Key Comparison
    //=============================================================================

    int MultiIndex::compare(const void* key1, const void* key2) const
    {
        if (hasError()) return 0;

        KeyType kt = getKeyType();

        switch (kt) {
        case KeyType::BLOCK: {
            // Use memcmp for faster block comparison
            int result = std::memcmp(key1, key2, getKeySize());
            return (result > 0) ? 1 : ((result < 0) ? -1 : 0);
        }

        case KeyType::NUM_BLOCK: {
            // Compare from most significant byte (big-endian style)
            const uint8_t* p1 = static_cast<const uint8_t*>(key1);
            const uint8_t* p2 = static_cast<const uint8_t*>(key2);
            for (int i = getKeySize() - 1; i >= 0; --i) {
                if (p1[i] != p2[i]) {
                    return (p1[i] > p2[i]) ? 1 : -1;
                }
            }
            return 0;
        }

        case KeyType::INTEGER: {
            int16_t v1 = *static_cast<const int16_t*>(key1);
            int16_t v2 = *static_cast<const int16_t*>(key2);
            return (v1 > v2) ? 1 : ((v1 < v2) ? -1 : 0);
        }

        case KeyType::LONG_INT: {
            int32_t v1 = *static_cast<const int32_t*>(key1);
            int32_t v2 = *static_cast<const int32_t*>(key2);
            return (v1 > v2) ? 1 : ((v1 < v2) ? -1 : 0);
        }

        case KeyType::STRING: {
            int result = std::strcmp(static_cast<const char*>(key1),
                static_cast<const char*>(key2));
            return (result > 0) ? 1 : ((result < 0) ? -1 : 0);
        }

        case KeyType::LOGICAL: {
            bool b1 = *static_cast<const char*>(key1) != 0;
            bool b2 = *static_cast<const char*>(key2) != 0;
            return (b1 && !b2) ? 1 : ((!b1 && b2) ? -1 : 0);
        }

        case KeyType::CHARACTER: {
            unsigned char c1 = *static_cast<const unsigned char*>(key1);
            unsigned char c2 = *static_cast<const unsigned char*>(key2);
            return (c1 > c2) ? 1 : ((c1 < c2) ? -1 : 0);
        }

        default:
            return 0;
        }
    }

    //=============================================================================
    // Navigation
    //=============================================================================

    int64_t MultiIndex::bringLeave(int64_t leavePos, void* key)
    {
        // Internal method - caller must hold lock
        if (leavePos == INVALID_POSITION) {
            return INVALID_POSITION;
        }

        auto leave = allocateLeaveBlock();
        readLeave(leave, leavePos);
        setPositionFromLeave(leavePos, leave);

        if (key != nullptr) {
            std::memcpy(key, getLeaveKey(leave), getKeySize());
        }

        return m_positions[m_currentIndex].currentDataPos;
    }

    int64_t MultiIndex::getFirst(void* key)
    {
        LockGuard lock(m_indexMutex);
        if (hasError()) return INVALID_POSITION;

        if (getFirstLeave() != getLastLeave()) {
            return bringLeave(getFirstLeave(), key);
        }
        return INVALID_POSITION;
    }

    int64_t MultiIndex::getNext(void* key)
    {
        LockGuard lock(m_indexMutex);
        if (hasError()) return INVALID_POSITION;

        if (!isEOFInternal() && m_positions[m_currentIndex].nextLeave != INVALID_POSITION) {
            return bringLeave(m_positions[m_currentIndex].nextLeave, key);
        }
        return INVALID_POSITION;
    }

    int64_t MultiIndex::getPrev(void* key)
    {
        LockGuard lock(m_indexMutex);
        if (hasError()) return INVALID_POSITION;

        if (!isBOFInternal() && m_positions[m_currentIndex].prevLeave != INVALID_POSITION) {
            return bringLeave(m_positions[m_currentIndex].prevLeave, key);
        }
        return INVALID_POSITION;
    }

    int64_t MultiIndex::getCurrent(void* key)
    {
        LockGuard lock(m_indexMutex);
        if (hasError()) return INVALID_POSITION;
        return bringLeave(m_positions[m_currentIndex].currentLeave, key);
    }

    //=============================================================================
    // Search Operations
    //=============================================================================

    bool MultiIndex::findLeave(const void* key, int64_t& leavePos)
    {
        // Internal method - caller must hold lock
        uint16_t levelNo = getNumLevels();
        if (levelNo < 1) {
            leavePos = INVALID_POSITION;
            return false;
        }

        auto node = allocateNodeBlock();
        int64_t nodePos = getRootNode();
        bool result = false;

        // Traverse to bottom level using binary search
        while (levelNo > 1 && nodePos != INVALID_POSITION) {
            readNode(node, nodePos);
            uint16_t numItems = getNumItems(node);
            
            auto [keyNo, exactMatch] = binarySearchNode(node, key, numItems);
            
            if (keyNo <= numItems) {
                nodePos = getChildPos(node, keyNo);
                --levelNo;
            }
            else {
                // Error: no key larger than EOF
                nodePos = INVALID_POSITION;
            }
        }

        // Get leaf from bottom level using binary search
        if (nodePos != INVALID_POSITION) {
            readNode(node, nodePos);
            uint16_t numItems = getNumItems(node);

            auto [keyNo, exactMatch] = binarySearchNode(node, key, numItems);

            if (keyNo <= numItems) {
                leavePos = getChildPos(node, keyNo);
                result = exactMatch;
            }
            else {
                leavePos = INVALID_POSITION;
            }
        }

        return result;
    }

    int64_t MultiIndex::find(const void* key)
    {
        LockGuard lock(m_indexMutex);
        if (hasError()) return INVALID_POSITION;

        int64_t leavePos;
        if (findLeave(key, leavePos)) {
            return bringLeave(leavePos, nullptr);
        }
        else if (leavePos != INVALID_POSITION) {
            bringLeave(leavePos, nullptr);
        }
        return INVALID_POSITION;
    }

    bool MultiIndex::findPath(const void* key, IndexStack& stack, int64_t& lastLevelChild)
    {
        // Internal method - caller must hold lock
        stack.clear();
        int64_t nodePos = getRootNode();

        if (nodePos == INVALID_POSITION) {
            return false;
        }

        uint16_t remainLevels = getNumLevels();
        auto node = allocateNodeBlock();
        bool notFirstLevel = false;

        while (nodePos != INVALID_POSITION) {
            readNode(node, nodePos);
            uint16_t numItems = getNumItems(node);

            if (notFirstLevel || numItems > 1) {
                notFirstLevel = true;
                
                // Use binary search for finding key position
                auto [keyNo, exactMatch] = binarySearchNode(node, key, numItems);

                if (keyNo <= numItems) {
                    --remainLevels;
                    if (remainLevels == 0) {
                        stack.push(nodePos, exactMatch ? keyNo : 0);
                        lastLevelChild = getChildPos(node, keyNo);
                        nodePos = INVALID_POSITION;
                    }
                    else {
                        stack.push(nodePos, keyNo);
                        nodePos = getChildPos(node, keyNo);
                    }
                }
                else {
                    // Error: EOF should be larger than any key
                    nodePos = INVALID_POSITION;
                    lastLevelChild = INVALID_POSITION;
                    stack.clear();
                }
            }
            else {
                if (getNumLevels() > 1) {
                    freeNode(nodePos);
                    nodePos = getChildPos(node, 1);
                    --remainLevels;
                    setRootNode(nodePos);
                    decNumLevels();
                }
                else {
                    --remainLevels;
                    stack.push(nodePos, 0);
                    nodePos = INVALID_POSITION;
                    lastLevelChild = getChildPos(node, 1);
                }
            }
        }

        return (remainLevels == 0 && !stack.empty());
    }

    //=============================================================================
    // Append Operation
    //=============================================================================

    bool MultiIndex::append(const void* newKey, int64_t newDataPos)
    {
        LockGuard lock(m_indexMutex);
        if (hasError()) return false;

        IndexStack stack;
        int64_t nextLeavePos;

        if (!findPath(newKey, stack, nextLeavePos)) {
            return false;
        }

        auto node = allocateNodeBlock();
        auto newLeave = allocateLeaveBlock();
        auto tempLeave = allocateLeaveBlock();

        // Allocate new leaf
        int64_t leavePos = allocateLeave();

        // Set up new leaf
        setNextLeave(newLeave, nextLeavePos);
        readLeave(tempLeave, nextLeavePos);
        int64_t prevLeavePos = getPrevLeave(tempLeave);
        setPrevLeave(newLeave, prevLeavePos);
        setPrevLeave(tempLeave, leavePos);
        writeLeave(tempLeave, nextLeavePos);

        if (prevLeavePos != INVALID_POSITION) {
            readLeave(tempLeave, prevLeavePos);
            setNextLeave(tempLeave, leavePos);
            writeLeave(tempLeave, prevLeavePos);
        }
        else {
            setFirstLeave(leavePos);
        }

        setLeaveKey(newLeave, newKey);
        setDataPos(newLeave, newDataPos);
        writeLeave(newLeave, leavePos);
        setPositionFromLeave(leavePos, newLeave);

        // Update tree
        int64_t nodePos;
        uint16_t keyNo;
        stack.pop(nodePos, keyNo);

        if (keyNo > 0) {
            // Check for UNIQUE constraint
            bool isUniqueIdx = (m_indexInfo[m_currentIndex].attributes |
                static_cast<uint16_t>(IndexAttribute::UNIQUE)) != 0;
            if (isUniqueIdx) {
                // UNIQUE index - key already exists, need to clean up the leaf we just created
                // Remove the leaf from the chain
                readLeave(tempLeave, nextLeavePos);
                setPrevLeave(tempLeave, prevLeavePos);
                writeLeave(tempLeave, nextLeavePos);
                
                if (prevLeavePos != INVALID_POSITION) {
                    readLeave(tempLeave, prevLeavePos);
                    setNextLeave(tempLeave, nextLeavePos);
                    writeLeave(tempLeave, prevLeavePos);
                }
                else {
                    setFirstLeave(nextLeavePos);
                }
                
                // Return the leaf to the free list
                freeLeave(leavePos);
                return false;
            }
            
            // Non-unique index - key already exists, update child pointer to new leaf
            readNode(node, nodePos);
            setChildPos(node, keyNo, leavePos);
            writeNode(node, nodePos);
            return true;
        }

        // New key, need to insert into tree
        if (nodePos == INVALID_POSITION) {
            return true;
        }

        auto newKeyBlock = allocateKeyBlock();
        std::memcpy(newKeyBlock.data(), newKey, getKeySize());
        int64_t newChildPos = leavePos;

        std::vector<uint8_t> changedKeyVal;
        uint16_t changedKeyNo = 0;

        while (nodePos != INVALID_POSITION) {
            std::vector<uint8_t> parentKey;
            std::vector<uint8_t> additionalKey;
            int64_t additionalChildPos = INVALID_POSITION;

            int state = insertKey(nodePos, newKeyBlock.data(), newChildPos,
                changedKeyNo,
                changedKeyVal.empty() ? nullptr : changedKeyVal.data(),
                parentKey, additionalKey, additionalChildPos);

            newKeyBlock.clear();
            changedKeyVal.clear();
            changedKeyNo = 0;

            switch (state) {
            case 1:
                // Done, no parent changes
                return true;

            case 2:
                // Last key modified, update parents
                if (!stack.empty()) {
                    do {
                        stack.pop(nodePos, changedKeyNo);
                        readNode(node, nodePos);
                        setNodeKey(node, changedKeyNo, parentKey.data());
                        writeNode(node, nodePos);
                    } while (changedKeyNo == getNumItems(node) && !stack.empty());
                }
                return true;

            case 3:
                // Node split, need to update parent
                if (!stack.empty()) {
                    stack.pop(nodePos, changedKeyNo);
                    changedKeyVal = std::move(parentKey);
                    newKeyBlock = std::move(additionalKey);
                    newChildPos = additionalChildPos;
                }
                else {
                    // Create new root
                    resetNode(node);
                    setNumItems(node, 2);
                    setNodeKey(node, 1, parentKey.data());
                    setChildPos(node, 1, nodePos);
                    setNodeKey(node, 2, additionalKey.data());
                    setChildPos(node, 2, additionalChildPos);
                    nodePos = writeNewNode(node);
                    setRootNode(nodePos);
                    incNumLevels();
                    return true;
                }
                break;

            default:
                return false;
            }
        }

        return true;
    }

    int MultiIndex::insertKey(int64_t nodePos, const void* newKey, int64_t newChildPos,
        uint16_t changedKeyNo, const void* changedKeyVal,
        std::vector<uint8_t>& parentKey,
        std::vector<uint8_t>& additionalKey,
        int64_t& additionalChildPos)
    {
        auto node = allocateNodeBlock();
        readNode(node, nodePos);

        if (changedKeyNo != 0 && changedKeyVal != nullptr) {
            setNodeKey(node, changedKeyNo, changedKeyVal);
        }

        uint16_t numItems = getNumItems(node);
        
        // Use binary search to find insertion point
        auto [keyNo, exactMatch] = binarySearchNode(node, newKey, numItems);

        if (exactMatch) {
            // Key already exists, update child pointer
            setChildPos(node, keyNo, newChildPos);
            writeNode(node, nodePos);
            return 1;
        }

        if (numItems < getMaxItems()) {
            // Simple insert
            insertItem(node, keyNo, newKey, newChildPos);
            writeNode(node, nodePos);

            if (keyNo <= numItems) {
                return 1;
            }
            else {
                parentKey = allocateKeyBlock();
                std::memcpy(parentKey.data(), newKey, getKeySize());
                return 2;
            }
        }

        // Node is full, need to split
        int64_t nextNodePos = getNextNode(node);

        if (nextNodePos != INVALID_POSITION) {
            auto nextNode = allocateNodeBlock();
            readNode(nextNode, nextNodePos);
            uint16_t nextNumItems = getNumItems(nextNode);

            if (nextNumItems < getMaxItems()) {
                // Can redistribute to next node
                if (keyNo <= numItems) {
                    insertItem(nextNode, 1, getNodeKey(node, numItems),
                        getChildPos(node, numItems));
                    deleteItem(node, numItems);
                    insertItem(node, keyNo, newKey, newChildPos);
                }
                else {
                    insertItem(nextNode, 1, newKey, newChildPos);
                }

                // Balance if delete is allowed
                if (canDelete()) {
                    uint16_t toMove = (getNumItems(node) - getNumItems(nextNode)) / 2;
                    while (toMove > 0) {
                        uint16_t srcNum = getNumItems(node);
                        insertItem(nextNode, 1, getNodeKey(node, srcNum),
                            getChildPos(node, srcNum));
                        deleteItem(node, srcNum);
                        --toMove;
                    }
                }

                parentKey = allocateKeyBlock();
                std::memcpy(parentKey.data(),
                    getNodeKey(node, getNumItems(node)), getKeySize());
                writeNode(node, nodePos);
                writeNode(nextNode, nextNodePos);
                return 2;
            }
        }

        // Create new node
        auto newNode = allocateNodeBlock();
        resetNode(newNode);

        if (keyNo <= numItems) {
            insertItem(newNode, 1, getNodeKey(node, numItems), getChildPos(node, numItems));
            deleteItem(node, numItems);
            insertItem(newNode, keyNo, newKey, newChildPos);
        }
        else {
            insertItem(newNode, 1, newKey, newChildPos);
        }

        // Balance if delete allowed
        if (canDelete()) {
            uint16_t toMove = (getNumItems(node) - 1) / 2;
            while (toMove > 0) {
                uint16_t srcNum = getNumItems(node);
                insertItem(newNode, 1, getNodeKey(node, srcNum), getChildPos(node, srcNum));
                deleteItem(node, srcNum);
                --toMove;
            }
        }

        setNextNode(newNode, nextNodePos);
        setPrevNode(newNode, nodePos);
        int64_t newNodePos = writeNewNode(newNode);
        setNextNode(node, newNodePos);

        if (nextNodePos != INVALID_POSITION) {
            auto nextNode = allocateNodeBlock();
            readNode(nextNode, nextNodePos);
            setPrevNode(nextNode, newNodePos);
            writeNode(nextNode, nextNodePos);
        }

        writeNode(node, nodePos);

        parentKey = allocateKeyBlock();
        std::memcpy(parentKey.data(), getNodeKey(node, getNumItems(node)), getKeySize());
        additionalKey = allocateKeyBlock();
        std::memcpy(additionalKey.data(), getNodeKey(newNode, getNumItems(newNode)), getKeySize());
        additionalChildPos = newNodePos;

        return 3;
    }

    //=============================================================================
    // Delete Operations
    //=============================================================================

    int64_t MultiIndex::modifyLeave(const void* key, int64_t newLeavePos)
    {
        uint16_t levelNo = getNumLevels();
        if (levelNo < 1) {
            return INVALID_POSITION;
        }

        auto node = allocateNodeBlock();
        int64_t nodePos = getRootNode();
        int64_t result = INVALID_POSITION;

        // Traverse using binary search
        while (levelNo > 1 && nodePos != INVALID_POSITION) {
            readNode(node, nodePos);
            uint16_t numItems = getNumItems(node);

            auto [keyNo, exactMatch] = binarySearchNode(node, key, numItems);

            if (keyNo <= numItems) {
                nodePos = getChildPos(node, keyNo);
                --levelNo;
            }
            else {
                nodePos = INVALID_POSITION;
            }
        }

        if (nodePos != INVALID_POSITION) {
            readNode(node, nodePos);
            uint16_t numItems = getNumItems(node);

            auto [keyNo, exactMatch] = binarySearchNode(node, key, numItems);

            if (keyNo <= numItems && exactMatch) {
                result = getChildPos(node, keyNo);
                setChildPos(node, keyNo, newLeavePos);
                writeNode(node, nodePos);
            }
        }

        return result;
    }

    int64_t MultiIndex::deleteKeyFromNodes(const void* deleteKey)
    {
        IndexStack stack;
        int64_t leavePos;

        if (!findPath(deleteKey, stack, leavePos)) {
            return INVALID_POSITION;
        }

        int64_t nodePos;
        uint16_t keyNoToRemove;
        stack.pop(nodePos, keyNoToRemove);

        if (keyNoToRemove == 0) {
            return INVALID_POSITION;
        }

        while (nodePos != INVALID_POSITION) {
            std::vector<uint8_t> parentKey;
            int state = removeKey(nodePos, keyNoToRemove, parentKey);

            switch (state) {
            case 1:
                // Done
                return leavePos;

            case 2:
                // Update parent key
                if (!stack.empty()) {
                    auto node = allocateNodeBlock();
                    uint16_t keyNo;
                    do {
                        stack.pop(nodePos, keyNo);
                        readNode(node, nodePos);
                        setNodeKey(node, keyNo, parentKey.data());
                        writeNode(node, nodePos);
                    } while (keyNo == getNumItems(node) && !stack.empty());
                }
                return leavePos;

            case 3:
                // Node removed, update parent
                if (!stack.empty()) {
                    stack.pop(nodePos, keyNoToRemove);
                }
                else {
                    return leavePos;
                }
                break;

            default:
                return INVALID_POSITION;
            }
        }

        return leavePos;
    }

    int MultiIndex::removeKey(int64_t nodePos, uint16_t removeKeyNo,
        std::vector<uint8_t>& parentKey)
    {
        int resultState = 1;
        auto node = allocateNodeBlock();
        readNode(node, nodePos);

        uint16_t numItems = getNumItems(node);
        if (removeKeyNo > numItems || removeKeyNo < 1) {
            return 1;
        }

        deleteItem(node, removeKeyNo);
        bool lastChanged = (removeKeyNo == numItems);
        --numItems;

        if (numItems != 0) {
            int64_t nextNodePos = getNextNode(node);

            if (nextNodePos != INVALID_POSITION) {
                auto nextNode = allocateNodeBlock();
                readNode(nextNode, nextNodePos);
                uint16_t nextNumItems = getNumItems(nextNode);

                if (nextNumItems + numItems <= getMaxItems() / 2) {
                    // Merge with next node
                    for (uint16_t i = 1; i <= numItems; ++i) {
                        insertItem(nextNode, i, getNodeKey(node, i), getChildPos(node, i));
                    }

                    int64_t prevNodePos = getPrevNode(node);
                    if (prevNodePos != INVALID_POSITION) {
                        auto prevNode = allocateNodeBlock();
                        readNode(prevNode, prevNodePos);
                        setNextNode(prevNode, nextNodePos);
                        writeNode(prevNode, prevNodePos);
                    }

                    setPrevNode(nextNode, prevNodePos);
                    writeNode(nextNode, nextNodePos);
                    freeNode(nodePos);
                    resultState = 3;
                }
                else if (nextNumItems > numItems + 1) {
                    // Borrow from next node
                    uint16_t toMove = (nextNumItems - numItems) / 2;
                    while (toMove > 0) {
                        ++numItems;
                        insertItem(node, numItems, getNodeKey(nextNode, 1),
                            getChildPos(nextNode, 1));
                        deleteItem(nextNode, 1);
                        --toMove;
                    }
                    lastChanged = true;
                    writeNode(nextNode, nextNodePos);
                    writeNode(node, nodePos);
                }
                else {
                    writeNode(node, nodePos);
                }
            }
            else {
                writeNode(node, nodePos);
            }
        }
        else {
            // Node is now empty
            int64_t nextNodePos = getNextNode(node);
            int64_t prevNodePos = getPrevNode(node);

            if (nextNodePos != INVALID_POSITION) {
                auto nextNode = allocateNodeBlock();
                readNode(nextNode, nextNodePos);
                setPrevNode(nextNode, prevNodePos);
                writeNode(nextNode, nextNodePos);
            }

            if (prevNodePos != INVALID_POSITION) {
                auto prevNode = allocateNodeBlock();
                readNode(prevNode, prevNodePos);
                setNextNode(prevNode, nextNodePos);
                writeNode(prevNode, prevNodePos);
            }

            freeNode(nodePos);
            resultState = 3;
        }

        if (resultState != 3 && lastChanged) {
            parentKey = allocateKeyBlock();
            std::memcpy(parentKey.data(), getNodeKey(node, getNumItems(node)), getKeySize());
            resultState = 2;
        }

        return resultState;
    }

    //=============================================================================
    // Key-Based Delete Operations
    //=============================================================================

    bool MultiIndex::deleteKey(const void* deleteKey)
    {
        LockGuard lock(m_indexMutex);
        if (hasError()) return false;

        int64_t leavePos = deleteKeyFromNodes(deleteKey);
        if (leavePos == INVALID_POSITION) {
            return false;
        }

        auto firstLeave = allocateLeaveBlock();
        auto tempLeave = allocateLeaveBlock();
        int64_t tempLeavePos = leavePos;

        readLeave(tempLeave, tempLeavePos);
        int64_t firstLeavePos = getPrevLeave(tempLeave);

        // Delete all leaves with this key
        do {
            freeLeave(tempLeavePos);
            tempLeavePos = getNextLeave(tempLeave);
            if (tempLeavePos == INVALID_POSITION) break;
            readLeave(tempLeave, tempLeavePos);
        } while (compare(getLeaveKey(tempLeave), deleteKey) == 0);

        // Update links
        setPrevLeave(tempLeave, firstLeavePos);
        writeLeave(tempLeave, tempLeavePos);

        if (firstLeavePos != INVALID_POSITION) {
            readLeave(firstLeave, firstLeavePos);
            setNextLeave(firstLeave, tempLeavePos);
            writeLeave(firstLeave, firstLeavePos);
        }
        else {
            setFirstLeave(tempLeavePos);
        }

        // Update position
        if (tempLeavePos != getLastLeave()) {
            setPositionFromLeave(tempLeavePos, tempLeave);
        }
        else if (firstLeavePos != INVALID_POSITION) {
            setPositionFromLeave(firstLeavePos, firstLeave);
        }
        else {
            resetPosition();
        }

        return true;
    }

    int64_t MultiIndex::deleteCurrent()
    {
        LockGuard lock(m_indexMutex);
        if (hasError()) return INVALID_POSITION;

        int64_t currentLeave = m_positions[m_currentIndex].currentLeave;
        if (currentLeave == INVALID_POSITION || currentLeave == getLastLeave()) {
            return INVALID_POSITION;
        }

        auto deletedLeave = allocateLeaveBlock();
        auto prevLeave = allocateLeaveBlock();
        auto nextLeave = allocateLeaveBlock();

        readLeave(deletedLeave, currentLeave);
        int64_t dataPos = getDataPos(deletedLeave);

        int64_t prevPos = m_positions[m_currentIndex].prevLeave;
        int64_t nextPos = m_positions[m_currentIndex].nextLeave;

        bool prevEqual = false;
        bool nextEqual = false;

        if (prevPos != INVALID_POSITION) {
            readLeave(prevLeave, prevPos);
            setNextLeave(prevLeave, nextPos);
            writeLeave(prevLeave, prevPos);
            prevEqual = (compare(getLeaveKey(prevLeave), getLeaveKey(deletedLeave)) == 0);
        }
        else {
            setFirstLeave(nextPos);
        }

        if (nextPos != INVALID_POSITION) {
            readLeave(nextLeave, nextPos);
            setPrevLeave(nextLeave, prevPos);
            writeLeave(nextLeave, nextPos);
            nextEqual = (compare(getLeaveKey(nextLeave), getLeaveKey(deletedLeave)) == 0);
        }

        freeLeave(currentLeave);

        // Update tree
        if (!prevEqual) {
            if (nextEqual) {
                modifyLeave(getLeaveKey(deletedLeave), nextPos);
            }
            else {
                deleteKeyFromNodes(getLeaveKey(deletedLeave));
            }
        }

        // Update position
        if (nextPos != INVALID_POSITION && nextPos != getLastLeave()) {
            setPositionFromLeave(nextPos, nextLeave);
        }
        else if (prevPos != INVALID_POSITION) {
            setPositionFromLeave(prevPos, prevLeave);
        }
        else {
            resetPosition();
        }

        return dataPos;
    }

} // namespace udb
