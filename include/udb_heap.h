/**
 * @file udb_heap.h
 * @brief Heap-structured file for dynamic record storage
 * 
 * This file implements a heap file structure for storing variable-length
 * records. The heap file maintains a "holes table" that tracks free space
 * from deleted records, enabling space reuse.
 * 
 * ## Heap File Structure
 * 
 * The heap file consists of:
 * 1. **Header Block**: File metadata including pointer to first holes table
 * 2. **Holes Tables**: Linked list of tables tracking free space (holes)
 * 3. **Data Records**: Variable-length records stored sequentially
 * 
 * ## Space Management
 * 
 * When records are deleted, their space is added to the holes table.
 * New records first check the holes table for suitable free space
 * before appending to the end of the file.
 * 
 * @author Digixoil
 * @version 2.0.0 (Modernized for Visual Studio 2025)
 * @date 2025
 */

#ifndef UDB_HEAP_H
#define UDB_HEAP_H

#include "udb_file.h"
#include <vector>
#include <optional>

namespace udb {

//=============================================================================
// Heap File Structures
//=============================================================================

#pragma pack(push, 1)

/**
 * @brief Heap file header structure
 * 
 * Stored at the beginning of every heap file.
 */
struct HeapFileHeader {
    uint8_t checksum;               ///< XOR checksum for integrity
    int64_t firstHolesTablePos;     ///< Position of first holes table (-1 if none)
    uint16_t holesTableSize;        ///< Number of entries per holes table
};

/**
 * @brief Record of a single free space hole
 */
struct HoleRecord {
    int64_t position;               ///< Position of the hole in file
    int64_t size;                   ///< Size of the hole in bytes
};

/**
 * @brief Header for a holes table block
 * 
 * Holes tables are linked together when more space is needed.
 */
struct HolesTableHeader {
    uint8_t checksum;               ///< XOR checksum for integrity
    uint16_t numUsed;               ///< Number of entries currently used
    int64_t nextTablePos;           ///< Position of next holes table (-1 if last)
    // Followed by HoleRecord[holesTableSize] entries
};

#pragma pack(pop)

//=============================================================================
// HeapFile Class
//=============================================================================

/**
 * @brief Heap-structured file for variable-length record storage
 * 
 * The HeapFile class extends the basic File class to provide:
 * - Allocation and deallocation of variable-size records
 * - Free space management through holes tables
 * - Space reuse when records are deleted
 * 
 * ## Usage Example
 * 
 * ```cpp
 * // Create a new heap file
 * HeapFile heap("data.hdb", 100);  // 100 entries per holes table
 * 
 * // Allocate space for a record
 * int64_t pos = heap.allocateSpace(256);
 * heap.write(data, 256, pos);
 * 
 * // Later, free the space
 * heap.freeSpace(pos, 256);
 * ```
 * 
 * ## Thread Safety
 * 
 * Not thread-safe. External synchronization required for concurrent access.
 */
class UDB_API HeapFile : public File {
public:
    /**
     * @brief Create a new heap file
     * 
     * @param filename Path to the file
     * @param holesTableSize Number of entries per holes table (default: 100)
     * 
     * @throws FileIOException if file cannot be created
     */
    HeapFile(const std::string& filename, uint16_t holesTableSize);
    
    /**
     * @brief Open an existing heap file
     * 
     * @param filename Path to the file
     * 
     * @throws FileIOException if file cannot be opened
     * @throws DataCorruptionException if header checksum fails
     */
    explicit HeapFile(const std::string& filename);
    
    /**
     * @brief Destructor - writes header and closes file
     */
    ~HeapFile() override;
    
    /**
     * @brief Allocate space for a record
     * 
     * Searches holes tables for suitable free space. If no suitable
     * hole is found, allocates at end of file.
     * 
     * @param size Number of bytes needed
     * @return Position where the record can be written
     */
    int64_t allocateSpace(size_t size);
    
    /**
     * @brief Free space occupied by a record
     * 
     * Adds the space to the holes table for future reuse.
     * 
     * @param position Start position of the record
     * @param size Size of the record in bytes
     */
    void freeSpace(int64_t position, size_t size);
    
    /**
     * @brief Get the holes table size
     */
    uint16_t getHolesTableSize() const { return m_header.holesTableSize; }
    
    /**
     * @brief Compact the file by removing all holes
     * 
     * This is an expensive operation that rewrites the entire file
     * to eliminate fragmentation.
     * 
     * @note Not implemented in this version
     */
    void compact();

private:
    // Header management
    void writeHeader();
    void readHeader();
    void setHeaderChecksum();
    bool testHeaderChecksum() const;
    
    // Holes table management
    size_t getHolesTableBlockSize() const;
    std::vector<uint8_t> allocateHolesTableBlock() const;
    void writeHolesTable(const std::vector<uint8_t>& table, int64_t pos);
    void readHolesTable(std::vector<uint8_t>& table, int64_t pos);
    int64_t writeNewHolesTable(std::vector<uint8_t>& table);
    void resetHolesTable(std::vector<uint8_t>& table);
    void setHolesTableChecksum(std::vector<uint8_t>& table);
    bool testHolesTableChecksum(const std::vector<uint8_t>& table) const;
    
    // Holes table entry access
    uint16_t getHolesTableNumUsed(const std::vector<uint8_t>& table) const;
    void setHolesTableNumUsed(std::vector<uint8_t>& table, uint16_t num);
    int64_t getHolesTableNextPos(const std::vector<uint8_t>& table) const;
    void setHolesTableNextPos(std::vector<uint8_t>& table, int64_t pos);
    HoleRecord getHolesTableItem(const std::vector<uint8_t>& table, uint16_t index) const;
    void setHolesTableItem(std::vector<uint8_t>& table, uint16_t index, 
                           int64_t pos, int64_t size);
    
    // Find a suitable hole for allocation
    std::optional<int64_t> findSuitableHole(size_t size);
    void addHole(int64_t position, size_t size);
    
    HeapFileHeader m_header;
};

} // namespace udb

#endif // UDB_HEAP_H
