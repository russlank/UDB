/**
 * @file udb_heap.cpp
 * @brief Implementation of the HeapFile class for dynamic record storage
 *
 * This file contains the thread-safe implementation of heap file operations
 * including space allocation, deallocation, and holes table management.
 *
 * ## Thread Safety Implementation
 *
 * The HeapFile class uses a two-level locking strategy:
 * 1. **File-level lock (inherited)**: Protects basic file I/O operations
 * 2. **Heap-level lock (m_heapMutex)**: Protects heap-specific state
 *
 * This design allows:
 * - Multiple threads to safely allocate/free space concurrently
 * - Atomic allocation: no two threads receive the same position
 * - Safe concurrent access to holes tables
 *
 * @author Digixoil
 * @version 2.1.0 (Thread Safety Enhancement)
 */

#include "udb_heap.h"
#include <algorithm>

namespace udb {

    //=============================================================================
    // Constructor / Destructor
    //=============================================================================

    HeapFile::HeapFile(const std::string& filename, uint16_t holesTableSize)
        : File(filename, true)
    {
        // No need to lock - object is being constructed
        m_header.firstHolesTablePos = INVALID_POSITION;
        m_header.holesTableSize = holesTableSize;
        writeHeader();
    }

    HeapFile::HeapFile(const std::string& filename)
        : File(filename, false)
    {
        // No need to lock - object is being constructed
        readHeader();
    }

    HeapFile::~HeapFile()
    {
        try {
            LockGuard lock(m_heapMutex);
            writeHeader();
        }
        catch (...) {
            // Ignore errors in destructor
        }
    }

    //=============================================================================
    // Header Operations
    //=============================================================================

    void HeapFile::setHeaderChecksum()
    {
        m_header.checksum = 0;
        m_header.checksum = calculateBlockChecksum(&m_header, sizeof(HeapFileHeader));
    }

    bool HeapFile::testHeaderChecksum() const
    {
        return calculateBlockChecksum(&m_header, sizeof(HeapFileHeader)) == 0;
    }

    void HeapFile::writeHeader()
    {
        // Assumes caller holds the lock
        setHeaderChecksum();
        write(&m_header, sizeof(HeapFileHeader), 0);
    }

    void HeapFile::readHeader()
    {
        // Assumes caller holds the lock (or in constructor)
        read(&m_header, sizeof(HeapFileHeader), 0);
        if (!testHeaderChecksum()) {
            setError(ErrorCode::BAD_DATA);
            throw DataCorruptionException("Heap file header checksum mismatch");
        }
    }

    //=============================================================================
    // Holes Table Operations
    //=============================================================================

    size_t HeapFile::getHolesTableBlockSize() const
    {
        return sizeof(HolesTableHeader) - sizeof(HoleRecord) +
            sizeof(HoleRecord) * m_header.holesTableSize;
    }

    std::vector<uint8_t> HeapFile::allocateHolesTableBlock() const
    {
        return std::vector<uint8_t>(getHolesTableBlockSize(), 0);
    }

    void HeapFile::setHolesTableChecksum(std::vector<uint8_t>& table)
    {
        auto* header = reinterpret_cast<HolesTableHeader*>(table.data());
        header->checksum = 0;
        header->checksum = calculateBlockChecksum(table.data(), getHolesTableBlockSize());
    }

    bool HeapFile::testHolesTableChecksum(const std::vector<uint8_t>& table) const
    {
        return calculateBlockChecksum(table.data(), getHolesTableBlockSize()) == 0;
    }

    void HeapFile::writeHolesTable(const std::vector<uint8_t>& table, int64_t pos)
    {
        // Assumes caller holds the lock
        // Need non-const copy for checksum
        auto tableCopy = table;
        setHolesTableChecksum(tableCopy);
        write(tableCopy.data(), getHolesTableBlockSize(), pos);
    }

    void HeapFile::readHolesTable(std::vector<uint8_t>& table, int64_t pos)
    {
        // Assumes caller holds the lock
        table.resize(getHolesTableBlockSize());
        read(table.data(), getHolesTableBlockSize(), pos);
        if (!testHolesTableChecksum(table)) {
            setError(ErrorCode::BAD_DATA);
            throw DataCorruptionException("Holes table checksum mismatch");
        }
    }

    int64_t HeapFile::writeNewHolesTable(std::vector<uint8_t>& table)
    {
        // Assumes caller holds the lock
        int64_t pos = size();
        setHolesTableNextPos(table, INVALID_POSITION);
        writeHolesTable(table, pos);
        return pos;
    }

    void HeapFile::resetHolesTable(std::vector<uint8_t>& table)
    {
        std::fill(table.begin(), table.end(), 0);
        setHolesTableNumUsed(table, 0);
        setHolesTableNextPos(table, INVALID_POSITION);
    }

    uint16_t HeapFile::getHolesTableNumUsed(const std::vector<uint8_t>& table) const
    {
        const auto* header = reinterpret_cast<const HolesTableHeader*>(table.data());
        return header->numUsed;
    }

    void HeapFile::setHolesTableNumUsed(std::vector<uint8_t>& table, uint16_t num)
    {
        auto* header = reinterpret_cast<HolesTableHeader*>(table.data());
        header->numUsed = num;
    }

    int64_t HeapFile::getHolesTableNextPos(const std::vector<uint8_t>& table) const
    {
        const auto* header = reinterpret_cast<const HolesTableHeader*>(table.data());
        return header->nextTablePos;
    }

    void HeapFile::setHolesTableNextPos(std::vector<uint8_t>& table, int64_t pos)
    {
        auto* header = reinterpret_cast<HolesTableHeader*>(table.data());
        header->nextTablePos = pos;
    }

    HoleRecord HeapFile::getHolesTableItem(const std::vector<uint8_t>& table, uint16_t index) const
    {
        const auto* records = reinterpret_cast<const HoleRecord*>(
            table.data() + sizeof(HolesTableHeader) - sizeof(HoleRecord));
        return records[index];
    }

    void HeapFile::setHolesTableItem(std::vector<uint8_t>& table, uint16_t index,
        int64_t pos, int64_t holeSize)
    {
        auto* records = reinterpret_cast<HoleRecord*>(
            table.data() + sizeof(HolesTableHeader) - sizeof(HoleRecord));
        records[index].position = pos;
        records[index].size = holeSize;
    }

    //=============================================================================
    // Space Management
    //=============================================================================

    std::optional<int64_t> HeapFile::findSuitableHole(size_t size)
    {
        // Assumes caller holds the lock
        int64_t tablePos = m_header.firstHolesTablePos;

        while (tablePos != INVALID_POSITION) {
            auto table = allocateHolesTableBlock();
            readHolesTable(table, tablePos);

            uint16_t numUsed = getHolesTableNumUsed(table);
            for (uint16_t i = 0; i < numUsed; ++i) {
                HoleRecord record = getHolesTableItem(table, i);
                if (record.size >= static_cast<int64_t>(size)) {
                    // Found a suitable hole
                    int64_t holePos = record.position;

                    // If hole is larger than needed, shrink it
                    if (record.size > static_cast<int64_t>(size)) {
                        setHolesTableItem(table, i,
                            record.position + size,
                            record.size - size);
                    }
                    else {
                        // Remove the hole by moving last item here
                        if (i < numUsed - 1) {
                            HoleRecord lastRecord = getHolesTableItem(table, numUsed - 1);
                            setHolesTableItem(table, i, lastRecord.position, lastRecord.size);
                        }
                        setHolesTableNumUsed(table, numUsed - 1);
                    }

                    writeHolesTable(table, tablePos);
                    return holePos;
                }
            }

            tablePos = getHolesTableNextPos(table);
        }

        return std::nullopt;
    }

    void HeapFile::addHole(int64_t position, size_t holeSize)
    {
        // Assumes caller holds the lock
        int64_t tablePos = m_header.firstHolesTablePos;
        int64_t prevTablePos = INVALID_POSITION;

        // Find a table with space
        while (tablePos != INVALID_POSITION) {
            auto table = allocateHolesTableBlock();
            readHolesTable(table, tablePos);

            uint16_t numUsed = getHolesTableNumUsed(table);
            if (numUsed < m_header.holesTableSize) {
                // Add hole to this table
                setHolesTableItem(table, numUsed, position, holeSize);
                setHolesTableNumUsed(table, numUsed + 1);
                writeHolesTable(table, tablePos);
                return;
            }

            prevTablePos = tablePos;
            tablePos = getHolesTableNextPos(table);
        }

        // Need to create a new holes table
        auto newTable = allocateHolesTableBlock();
        resetHolesTable(newTable);
        setHolesTableItem(newTable, 0, position, holeSize);
        setHolesTableNumUsed(newTable, 1);

        int64_t newTablePos = writeNewHolesTable(newTable);

        if (prevTablePos != INVALID_POSITION) {
            // Link from previous table
            auto prevTable = allocateHolesTableBlock();
            readHolesTable(prevTable, prevTablePos);
            setHolesTableNextPos(prevTable, newTablePos);
            writeHolesTable(prevTable, prevTablePos);
        }
        else {
            // This is the first holes table
            m_header.firstHolesTablePos = newTablePos;
            writeHeader();
        }
    }

    int64_t HeapFile::allocateSpace(size_t size)
    {
        // Acquire the heap mutex for the entire allocation operation
        // This ensures atomic allocation - no two threads get the same position
        LockGuard lock(m_heapMutex);

        // First, try to find a suitable hole
        auto hole = findSuitableHole(size);
        if (hole.has_value()) {
            return hole.value();
        }

        // No suitable hole found, allocate at end of file
        return this->size();
    }

    void HeapFile::freeSpace(int64_t position, size_t size)
    {
        // Acquire the heap mutex for the entire free operation
        LockGuard lock(m_heapMutex);
        addHole(position, size);
    }

    void HeapFile::compact()
    {
        // Note: Full compaction is a complex operation that would require
        // coordination with the index to update data pointers.
        // Left as a stub for future implementation.
        throw std::runtime_error("Compact operation not yet implemented");
    }

} // namespace udb
