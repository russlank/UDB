/**
 * @file udb_heap.h
 * @brief Heap-structured file for dynamic record storage
 *
 * This file implements a heap file structure for storing variable-length
 * records. The heap file maintains a "holes table" that tracks free space
 * from deleted records, enabling space reuse.
 *
 * ## What is a Heap File?
 *
 * A heap file is a file organization where records are stored in no particular
 * order. Unlike sorted files or B-Trees, heap files simply append new records
 * at any available position. This makes insertion O(1) but searching O(n).
 *
 * ## Why Use a Heap File?
 *
 * Heap files are ideal when:
 * - Records are accessed via an index (B-Tree provides the position)
 * - Records have variable lengths
 * - Space reclamation is important
 * - Insert speed matters more than search speed
 *
 * ## Heap File Structure
 *
 * The heap file consists of:
 * 1. **Header Block**: File metadata including pointer to first holes table
 * 2. **Holes Tables**: Linked list of tables tracking free space (holes)
 * 3. **Data Records**: Variable-length records stored at arbitrary positions
 *
 * ### Visual Layout
 *
 * ```
 * ┌────────────────────────────────────────────────────────────────────────┐
 * │                        FILE HEADER                                     │
 * │  [checksum][firstHolesTablePos][holesTableSize]                        │
 * ├────────────────────────────────────────────────────────────────────────┤
 * │  Record 1  │  HOLE (deleted)  │  Record 2  │  Record 3  │ ...          │
 * ├────────────────────────────────────────────────────────────────────────┤
 * │                     HOLES TABLE 1                                      │
 * │  [checksum][numUsed][nextTablePos]                                     │
 * │  [hole: pos=X, size=Y] [hole: pos=A, size=B] ...                       │
 * ├────────────────────────────────────────────────────────────────────────┤
 * │                     HOLES TABLE 2 (if needed)                          │
 * └────────────────────────────────────────────────────────────────────────┘
 * ```
 *
 * ## Space Management
 *
 * When records are deleted, their space is added to the holes table.
 * New records first check the holes table for suitable free space
 * (first-fit algorithm) before appending to the end of the file.
 *
 * ### Allocation Strategy: First-Fit
 *
 * We use first-fit allocation for simplicity:
 * 1. Search holes tables for first hole >= requested size
 * 2. If found and hole is larger, shrink the hole
 * 3. If not found, allocate at end of file
 *
 * **Trade-offs**:
 * - First-fit: Simple, fast for small holes tables
 * - Best-fit: Less fragmentation, slower (must scan all)
 * - Worst-fit: Leaves larger holes, may reduce fragmentation
 *
 * ## Integration with B-Tree Index
 *
 * Typically used with MultiIndex:
 * ```cpp
 * // Store a record
 * int64_t pos = heap.allocateSpace(recordSize);
 * heap.write(record, recordSize, pos);
 * index.append(key, pos);  // Store position in index
 *
 * // Retrieve a record
 * int64_t pos = index.find(key);
 * heap.read(record, recordSize, pos);
 *
 * // Delete a record
 * int64_t pos = index.find(key);
 * index.deleteKey(key);
 * heap.freeSpace(pos, recordSize);  // Important: must know size!
 * ```
 *
 * ## Thread Safety
 *
 * The HeapFile class is **thread-safe** for individual operations.
 * All public methods acquire an internal mutex before accessing state.
 *
 * **Safe patterns:**
 * ```cpp
 * // Thread 1                          // Thread 2
 * auto pos1 = heap.allocateSpace(100); auto pos2 = heap.allocateSpace(200);
 * heap.write(data1, 100, pos1);        heap.write(data2, 200, pos2);
 * ```
 *
 * **Patterns requiring external synchronization:**
 * ```cpp
 * // Allocate-then-write must be synchronized if order matters
 * {
 *     std::lock_guard<RecursiveMutex> lock(heap.getMutex());
 *     int64_t pos = heap.allocateSpace(size);
 *     heap.write(data, size, pos);
 * }
 * ```
 *
 * ## Limitations
 *
 * 1. **No size tracking**: Application must track record sizes
 * 2. **No coalescing**: Adjacent holes are not merged (fragmentation)
 * 3. **No compaction**: Must rebuild file to eliminate fragmentation
 * 4. **No transaction support**: No crash recovery
 *
 * ## Future Improvements
 *
 * - Hole coalescing (merge adjacent holes)
 * - File compaction
 * - Best-fit or buddy-system allocation
 * - Size prefix in records
 *
 * @author Digixoil
 * @version 2.1.0 (Thread Safety Enhancement)
 * @date 2025
 *
 * @see udb_btree.h for the B-Tree index that provides key-to-position mapping
 * @see udb_sync.h for synchronization primitives
 * @see doc/heap-file.md for detailed algorithm documentation
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

    /**
     * @brief Pragma pack ensures structures are stored without padding
     *
     * This is critical for file format compatibility. Without packing,
     * the compiler might add padding bytes between fields, causing:
     * - Different sizes on different compilers/platforms
     * - File format incompatibility
     * - Checksum failures when reading files created by different builds
     */
#pragma pack(push, 1)

    /**
     * @brief Heap file header structure
     *
     * Stored at the beginning (position 0) of every heap file.
     * This header is read first when opening an existing file
     * to locate the holes table chain.
     *
     * ## Layout (11 bytes total)
     * ```
     * Offset  Size  Field
     * 0       1     checksum
     * 1       8     firstHolesTablePos
     * 9       2     holesTableSize
     * ```
     *
     * ## Checksum Calculation
     * The checksum is calculated over all fields (including checksum=0),
     * then stored in the checksum field. To verify: recalculate over
     * entire structure; result should be 0 if valid.
     */
    struct HeapFileHeader {
        uint8_t checksum;               ///< XOR checksum for integrity verification
        int64_t firstHolesTablePos;     ///< Position of first holes table (-1 if no holes exist)
        uint16_t holesTableSize;        ///< Number of entries per holes table (set at creation)
    };

    /**
     * @brief Record of a single free space hole
     *
     * Each hole record tracks one contiguous region of free space
     * in the file. These are stored in holes tables.
     *
     * ## Size: 16 bytes
     * - position: 8 bytes (file offset where hole starts)
     * - size: 8 bytes (how many bytes are free)
     *
     * ## Example
     * If a 100-byte record at position 500 is deleted:
     * ```
     * HoleRecord { position: 500, size: 100 }
     * ```
     */
    struct HoleRecord {
        int64_t position;               ///< Position of the hole in file (byte offset)
        int64_t size;                   ///< Size of the hole in bytes
    };

    /**
     * @brief Header for a holes table block
     *
     * Holes tables store arrays of HoleRecord entries. When one table
     * fills up, a new table is created and linked via nextTablePos.
     *
     * ## Structure
     * ```
     * ┌──────────────────────────────────────────────┐
     * │ HolesTableHeader (11 bytes)                  │
     * │   checksum (1)                               │
     * │   numUsed (2)                                │
     * │   nextTablePos (8)                           │
     * ├──────────────────────────────────────────────┤
     * │ HoleRecord[0] (16 bytes)                     │
     * │ HoleRecord[1] (16 bytes)                     │
     * │ ...                                          │
     * │ HoleRecord[holesTableSize-1]                 │
     * └──────────────────────────────────────────────┘
     * ```
     *
     * ## Linked List
     * Multiple holes tables form a singly-linked list:
     * ```
     * [Header] -> [Table1] -> [Table2] -> [Table3] -> -1
     *             numUsed=50  numUsed=50  numUsed=23
     *             next=pos2   next=pos3   next=-1
     * ```
     */
    struct HolesTableHeader {
        uint8_t checksum;               ///< XOR checksum for integrity
        uint16_t numUsed;               ///< Number of entries currently in use (0 to holesTableSize)
        int64_t nextTablePos;           ///< Position of next holes table (-1 if this is the last)
        // Followed by HoleRecord[holesTableSize] entries
    };

#pragma pack(pop)

    //=============================================================================
    // HeapFile Class
    //=============================================================================

    /**
     * @brief Thread-safe heap-structured file for variable-length record storage
     *
     * The HeapFile class extends the basic File class to provide:
     * - Allocation and deallocation of variable-size records
     * - Free space management through holes tables
     * - Space reuse when records are deleted
     * - Thread-safe operations for concurrent access
     *
     * ## Thread Safety
     *
     * All public methods are thread-safe. The class uses the inherited mutex
     * from File plus an internal mutex for heap-specific state.
     *
     * **Thread-safe operations:**
     * - allocateSpace(): Atomically finds or creates space
     * - freeSpace(): Atomically adds hole to tracking tables
     * - All inherited File operations (read, write, etc.)
     *
     * **Multi-threaded usage example:**
     * ```cpp
     * HeapFile heap("data.heap", 100);
     *
     * // Thread 1
     * int64_t pos1 = heap.allocateSpace(sizeof(Record1));
     * Record1 r1 = {...};
     * heap.write(&r1, sizeof(r1), pos1);
     *
     * // Thread 2 (can run concurrently)
     * int64_t pos2 = heap.allocateSpace(sizeof(Record2));
     * Record2 r2 = {...};
     * heap.write(&r2, sizeof(r2), pos2);
     * ```
     *
     * ## Typical Usage Pattern
     *
     * ```cpp
     * // Create a new heap file with 100-entry holes tables
     * HeapFile heap("data.heap", 100);
     *
     * // Store a record
     * MyRecord record = { ... };
     * int64_t pos = heap.allocateSpace(sizeof(record));
     * heap.write(&record, sizeof(record), pos);
     *
     * // Store position in index for later retrieval
     * index.append(key, pos);
     *
     * // Later, retrieve the record
     * int64_t pos = index.find(key);
     * if (pos >= 0) {
     *     heap.read(&record, sizeof(record), pos);
     * }
     *
     * // Delete the record
     * index.deleteKey(key);
     * heap.freeSpace(pos, sizeof(record));
     * ```
     *
     * ## Important: Record Size Tracking
     *
     * The heap file does NOT track record sizes. The application must
     * either:
     * 1. Use fixed-size records
     * 2. Store size as part of the record
     * 3. Track sizes externally
     *
     * Example with size prefix:
     * ```cpp
     * // Write with size prefix
     * uint32_t size = dataSize;
     * int64_t pos = heap.allocateSpace(sizeof(size) + dataSize);
     * heap.write(&size, sizeof(size), pos);
     * heap.write(data, dataSize, pos + sizeof(size));
     *
     * // Read with size prefix
     * uint32_t size;
     * heap.read(&size, sizeof(size), pos);
     * heap.read(data, size, pos + sizeof(size));
     *
     * // Free with size prefix
     * heap.freeSpace(pos, sizeof(size) + size);
     * ```
     *
     * ## Performance Characteristics
     *
     * | Operation | Time Complexity |
     * |-----------|-----------------|
     * | allocateSpace (with holes) | O(h) where h = total holes |
     * | allocateSpace (no holes) | O(1) - append to end |
     * | freeSpace | O(t) where t = number of holes tables |
     * | read/write | O(1) - direct access |
     *
     * ## Fragmentation
     *
     * Over time, the file may become fragmented with many small holes
     * that cannot satisfy larger allocation requests. Solutions:
     * - Periodic compaction (not implemented)
     * - Hole coalescing (not implemented)
     * - Use fixed-size records when possible
     */
    class UDB_THREAD_SAFE HeapFile : public File {
    public:
        /**
         * @brief Create a new heap file
         *
         * Creates a new heap file, overwriting any existing file with the same name.
         * The holesTableSize parameter determines how many hole records fit in each
         * holes table block.
         *
         * ## Choosing holesTableSize
         *
         * | Size | Pros | Cons |
         * |------|------|------|
         * | Small (10-20) | Less disk space per table | More tables needed, slower search |
         * | Medium (50-100) | Good balance | Reasonable for most use cases |
         * | Large (200+) | Fast search | Wastes space if few holes |
         *
         * **Recommendation**: 100 is a good default for most applications.
         *
         * ## Thread Safety
         * Constructor is not thread-safe (object not yet constructed).
         *
         * @param filename Path to the file
         * @param holesTableSize Number of entries per holes table (default: 100)
         *
         * @throws FileIOException if file cannot be created
         *
         * @example
         * ```cpp
         * // Create with default holes table size
         * HeapFile heap("data.heap", 100);
         *
         * // Create with larger tables for high-delete workloads
         * HeapFile highDeleteHeap("temp.heap", 500);
         * ```
         */
        HeapFile(const std::string& filename, uint16_t holesTableSize);

        /**
         * @brief Open an existing heap file
         *
         * Opens an existing heap file and reads its header to obtain
         * the holes table configuration.
         *
         * ## Thread Safety
         * Constructor is not thread-safe (object not yet constructed).
         *
         * @param filename Path to the file
         *
         * @throws FileIOException if file cannot be opened
         * @throws DataCorruptionException if header checksum fails
         *
         * @example
         * ```cpp
         * try {
         *     HeapFile heap("existing.heap");
         *     // Use the heap file
         * } catch (const DataCorruptionException& e) {
         *     std::cerr << "File corrupted: " << e.what() << std::endl;
         * }
         * ```
         */
        explicit HeapFile(const std::string& filename);

        /**
         * @brief Destructor - writes header and closes file
         *
         * Ensures the header is written before closing to persist
         * any changes to the holes table pointers.
         *
         * ## Thread Safety
         * Destructor waits for in-progress operations to complete.
         */
        ~HeapFile() override;

        /**
         * @brief Allocate space for a record
         *
         * Searches holes tables for suitable free space using first-fit
         * algorithm. If no suitable hole is found, allocates at end of file.
         *
         * ## Algorithm
         *
         * 1. For each holes table:
         *    - For each hole entry:
         *      - If hole.size >= requested size:
         *        - If exact fit: remove hole from table
         *        - If larger: shrink hole (move start, reduce size)
         *        - Return original hole position
         * 2. If no suitable hole: return current file size (append)
         *
         * ## Return Value
         *
         * The returned position is where you should write your data.
         * The file is NOT automatically extended; it grows when you write.
         *
         * ## Thread Safety
         * Thread-safe. Atomic allocation - no two threads will receive
         * the same position.
         *
         * @param size Number of bytes needed
         * @return Position where the record can be written
         *
         * @example
         * ```cpp
         * // Allocate space for a structure
         * int64_t pos = heap.allocateSpace(sizeof(MyRecord));
         *
         * // Write the record at the allocated position
         * MyRecord record = { ... };
         * heap.write(&record, sizeof(record), pos);
         * ```
         *
         * @note The allocation is "optimistic" - it doesn't reserve space.
         *       If you don't write to the position, no space is consumed.
         */
        int64_t allocateSpace(size_t size);

        /**
         * @brief Free space occupied by a record
         *
         * Adds the space to the holes table for future reuse.
         * The space is NOT zeroed or securely erased.
         *
         * ## Important
         *
         * You MUST pass the correct size! If the size is wrong:
         * - Too small: Part of the record space is lost
         * - Too large: May corrupt adjacent records on reuse
         *
         * ## Algorithm
         *
         * 1. Search for a holes table with available space
         * 2. If found: add hole record to that table
         * 3. If not found: create new holes table, add hole
         *
         * ## Thread Safety
         * Thread-safe. Multiple threads can free space concurrently.
         *
         * @param position Start position of the record
         * @param size Size of the record in bytes
         *
         * @example
         * ```cpp
         * // Free a record (must know its size!)
         * heap.freeSpace(recordPos, sizeof(MyRecord));
         *
         * // With size-prefixed records:
         * uint32_t size;
         * heap.read(&size, sizeof(size), recordPos);
         * heap.freeSpace(recordPos, sizeof(size) + size);
         * ```
         *
         * @warning Does NOT check if the position is valid or already free.
         *          Double-freeing will create overlapping holes.
         */
        void freeSpace(int64_t position, size_t size);

        /**
         * @brief Get the holes table size
         *
         * Returns the number of hole records that fit in each holes table.
         * This is set at file creation and cannot be changed.
         *
         * ## Thread Safety
         * Thread-safe. Value is immutable after construction.
         *
         * @return Number of entries per holes table
         */
        uint16_t getHolesTableSize() const { return m_header.holesTableSize; }

        /**
         * @brief Compact the file by removing all holes
         *
         * This is an expensive operation that rewrites the entire file
         * to eliminate fragmentation. Not yet implemented.
         *
         * ## When to Compact
         *
         * Consider compaction when:
         * - Free space exceeds 30-40% of file size
         * - Many small holes exist that can't be reused
         * - File has grown much larger than actual data
         *
         * ## Implementation Notes (for future)
         *
         * 1. Create temporary file
         * 2. Copy all valid records (skip holes)
         * 3. Update all index positions
         * 4. Replace original with compacted file
         *
         * @note Not implemented in this version
         * @throws std::runtime_error always (not implemented)
         */
        void compact();

    private:
        //=========================================================================
        // Header Management
        //=========================================================================

        /**
         * @brief Write the file header to disk
         *
         * Calculates checksum and writes header at position 0.
         *
         * @note Caller must hold the mutex
         */
        UDB_REQUIRES_LOCK void writeHeader();

        /**
         * @brief Read and verify the file header
         *
         * @throws DataCorruptionException if checksum fails
         * @note Caller must hold the mutex
         */
        UDB_REQUIRES_LOCK void readHeader();

        /**
         * @brief Calculate and set header checksum
         * @note Caller must hold the mutex
         */
        UDB_REQUIRES_LOCK void setHeaderChecksum();

        /**
         * @brief Verify header checksum
         * @return true if checksum is valid
         */
        bool testHeaderChecksum() const;

        //=========================================================================
        // Holes Table Management
        //=========================================================================

        /**
         * @brief Get the size of a holes table block in bytes
         * @return Size including header and all hole records
         */
        size_t getHolesTableBlockSize() const;

        /**
         * @brief Allocate memory for a holes table block
         * @return Zero-initialized byte vector of correct size
         */
        std::vector<uint8_t> allocateHolesTableBlock() const;

        /**
         * @brief Write a holes table to disk
         * @param table The holes table data
         * @param pos Position to write at
         * @note Caller must hold the mutex
         */
        UDB_REQUIRES_LOCK void writeHolesTable(const std::vector<uint8_t>& table, int64_t pos);

        /**
         * @brief Read a holes table from disk
         * @param table Output: the holes table data
         * @param pos Position to read from
         * @throws DataCorruptionException if checksum fails
         * @note Caller must hold the mutex
         */
        UDB_REQUIRES_LOCK void readHolesTable(std::vector<uint8_t>& table, int64_t pos);

        /**
         * @brief Write a new holes table at end of file
         * @param table The holes table data
         * @return Position where table was written
         * @note Caller must hold the mutex
         */
        UDB_REQUIRES_LOCK int64_t writeNewHolesTable(std::vector<uint8_t>& table);

        /**
         * @brief Reset a holes table to empty state
         * @param table The holes table to reset
         */
        void resetHolesTable(std::vector<uint8_t>& table);

        /**
         * @brief Calculate and set holes table checksum
         * @param table The holes table to checksum
         */
        void setHolesTableChecksum(std::vector<uint8_t>& table);

        /**
         * @brief Verify holes table checksum
         * @param table The holes table to verify
         * @return true if checksum is valid
         */
        bool testHolesTableChecksum(const std::vector<uint8_t>& table) const;

        //=========================================================================
        // Holes Table Entry Access
        //=========================================================================

        /**
         * @brief Get number of entries in use in a holes table
         */
        uint16_t getHolesTableNumUsed(const std::vector<uint8_t>& table) const;

        /**
         * @brief Set number of entries in use
         */
        void setHolesTableNumUsed(std::vector<uint8_t>& table, uint16_t num);

        /**
         * @brief Get position of next holes table
         */
        int64_t getHolesTableNextPos(const std::vector<uint8_t>& table) const;

        /**
         * @brief Set position of next holes table
         */
        void setHolesTableNextPos(std::vector<uint8_t>& table, int64_t pos);

        /**
         * @brief Get a hole record from the table
         */
        HoleRecord getHolesTableItem(const std::vector<uint8_t>& table, uint16_t index) const;

        /**
         * @brief Set a hole record in the table
         */
        void setHolesTableItem(std::vector<uint8_t>& table, uint16_t index,
            int64_t pos, int64_t size);

        //=========================================================================
        // Allocation Helpers
        //=========================================================================

        /**
         * @brief Find a suitable hole for allocation
         *
         * Uses first-fit algorithm: returns first hole >= requested size.
         * If a larger hole is found, it is shrunk. If exact fit, it is removed.
         *
         * @param size Requested allocation size
         * @return Position if found, std::nullopt if no suitable hole
         * @note Caller must hold the mutex
         */
        UDB_REQUIRES_LOCK std::optional<int64_t> findSuitableHole(size_t size);

        /**
         * @brief Add a hole to the holes table chain
         *
         * Finds a table with space or creates a new one.
         *
         * @param position Start position of the hole
         * @param size Size of the hole
         * @note Caller must hold the mutex
         */
        UDB_REQUIRES_LOCK void addHole(int64_t position, size_t size);

        //=========================================================================
        // Member Data
        //=========================================================================

        HeapFileHeader m_header;            ///< File header (cached in memory)
        mutable RecursiveMutex m_heapMutex; ///< Mutex for heap-specific operations
    };

} // namespace udb

#endif // UDB_HEAP_H
