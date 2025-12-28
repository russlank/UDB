/**
 * @file udb_common.h
 * @brief Common definitions, types, and utilities for the UDB library
 *
 * This file contains all common definitions, error codes, type definitions,
 * and utility functions used throughout the UDB (Ultra Database) library.
 *
 * ## Purpose
 *
 * This header provides the foundational types and utilities that all other
 * UDB components depend on. By centralizing these definitions, we ensure:
 * - Consistent error handling across the library
 * - Type safety through enum classes
 * - Portable integer types via `<cstdint>`
 * - Reusable utility functions
 *
 * ## Design Decisions
 *
 * ### Error Codes vs Exceptions
 * The library supports both error codes (for performance-critical paths and
 * compatibility) and exceptions (for exceptional conditions like file I/O
 * failures or data corruption). Use `hasError()` for checking recoverable
 * conditions, and catch exceptions for critical failures.
 *
 * ### Key Types
 * Different key types have different comparison semantics:
 * - STRING: Null-terminated, uses strcmp (locale-independent)
 * - INTEGER/LONG_INT: Native machine comparison
 * - BLOCK: Byte-by-byte from first to last (lexicographic)
 * - NUM_BLOCK: Byte-by-byte from last to first (big-endian numeric)
 *
 * ### Checksum Algorithm
 * We use XOR checksums for simplicity and speed. While not cryptographically
 * secure, XOR checksums detect single-bit errors and most multi-bit errors.
 * For production use, consider CRC32 or stronger algorithms.
 *
 * ## Best Practices
 *
 * 1. Always check `hasError()` after operations that might fail
 * 2. Use `IndexAttribute::ALLOW_DELETE` for indexes that need deletion
 * 3. Choose key sizes carefully - larger keys mean fewer keys per node
 * 4. Pre-allocate nodes and leaves to reduce file growth operations
 *
 * @author Digixoil
 * @version 2.1.0 (Modernized for Visual Studio 2025)
 * @date 2025
 *
 * @see udb_file.h for file I/O abstraction
 * @see udb_btree.h for B-Tree index implementation
 * @see udb_heap.h for heap file storage
 */

#ifndef UDB_COMMON_H
#define UDB_COMMON_H

#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <memory>
#include <stdexcept>

//=============================================================================
// Platform-specific definitions
//=============================================================================

/**
 * @brief DLL export/import macros for Windows
 *
 * These macros control symbol visibility when building/using as a DLL.
 * Define UDB_BUILDING_DLL when compiling the library as a DLL.
 * Leave undefined when using as a static library or when linking.
 *
 * @note For static library builds (recommended), these macros are no-ops.
 */
#ifdef _WIN32
#define UDB_EXPORT __declspec(dllexport)
#define UDB_IMPORT __declspec(dllimport)
#ifdef UDB_BUILDING_DLL
#define UDB_API UDB_EXPORT
#else
#define UDB_API UDB_IMPORT
#endif
#else
#define UDB_API
#endif

namespace udb {

    //=============================================================================
    // Error Codes
    //=============================================================================

    /**
     * @brief Error codes used throughout the UDB library
     *
     * These error codes provide fine-grained information about what went wrong.
     * They are stored in the File base class and can be retrieved via getError().
     *
     * ## Usage Pattern
     * ```cpp
     * if (file.hasError()) {
     *     switch (file.getError()) {
     *         case ErrorCode::BAD_DATA:
     *             // Handle corruption
     *             break;
     *         case ErrorCode::READ_ERROR:
     *             // Handle I/O error
     *             break;
     *         // ...
     *     }
     * }
     * ```
     *
     * ## Design Note
     * Error codes are retained for compatibility and for cases where exceptions
     * are too heavyweight. For critical errors, exceptions are also thrown.
     */
    enum class ErrorCode : int {
        OK = 0,                 ///< No error - operation succeeded
        ERROR = 1,              ///< Generic error (avoid using - prefer specific codes)
        READ_ERROR = 2,         ///< File read operation failed
        WRITE_ERROR = 3,        ///< File write operation failed
        SEEK_ERROR = 4,         ///< File seek operation failed
        BAD_DATA = 5,           ///< Data corruption detected (checksum mismatch)
        MEMORY_ERROR = 6,       ///< Memory allocation failed (rare with modern C++)
        POINTER_ERROR = 7,      ///< Invalid pointer passed to function
        BAD_FILE_DATA = 8,      ///< File data is structurally invalid
        BAD_FILE_HANDLE = 9,    ///< Invalid file handle (file not open)
        CREATE_ERROR = 10,      ///< File creation failed
        GET_FILE_SIZE = 11,     ///< Failed to get file size
        OPEN_ERROR = 12,        ///< File open failed
        CLOSE_ERROR = 13,       ///< File close failed (rare)
        GET_FILE_POS = 14,      ///< Failed to get file position
        INIT_ERROR = 15         ///< Initialization error (e.g., invalid parameters)
    };

    /**
     * @brief Error type categories for filtering
     *
     * These categories allow grouping related errors for logging or handling.
     * They use bit flags so multiple categories can be combined.
     *
     * @note Currently informational; not used for filtering in this version.
     */
    enum class ErrorType : uint16_t {
        INPUT = 0x0001,         ///< Input-related errors (read failures)
        OUTPUT = 0x0002,        ///< Output-related errors (write failures)
        IO = 0x0003,            ///< I/O errors (input or output)
        MEMORY = 0x0004,        ///< Memory-related errors
        LOGICAL = 0x0008,       ///< Logical/programming errors
        BAD_DATA = 0x0010,      ///< Data corruption errors
        ALL = 0xFFFF            ///< All error types
    };

    //=============================================================================
    // Key Type Definitions
    //=============================================================================

    /**
     * @brief Supported key types for B-Tree indexing
     *
     * Each key type has specific comparison semantics that affect how keys
     * are ordered in the B-Tree. Choose the appropriate type based on your
     * data and sorting requirements.
     *
     * ## Comparison Semantics
     *
     * | Type | Comparison Method | Notes |
     * |------|-------------------|-------|
     * | BLOCK | Byte-by-byte, MSB first | Good for hashes, UUIDs |
     * | NUM_BLOCK | Byte-by-byte, LSB first | For big-endian numbers |
     * | INTEGER | Native int16_t comparison | Signed 16-bit |
     * | LONG_INT | Native int32_t comparison | Signed 32-bit |
     * | STRING | strcmp() | Null-terminated, ASCII order |
     * | LOGICAL | false < true | Boolean values |
     * | CHARACTER | ASCII value comparison | Single characters |
     *
     * ## Performance Considerations
     *
     * - INTEGER and LONG_INT are fastest (single comparison)
     * - STRING requires scanning for null terminator
     * - BLOCK/NUM_BLOCK scan all bytes
     *
     * ## Example
     * ```cpp
     * // For string keys
     * index.initIndex(KeyType::STRING, 50, ...);
     *
     * // For integer keys (e.g., user IDs)
     * index.initIndex(KeyType::LONG_INT, 4, ...);
     * ```
     */
    enum class KeyType : uint16_t {
        VOID = 0,               ///< No type (invalid/uninitialized)
        BLOCK = 1,              ///< Raw byte block, compared MSB first
        NUM_BLOCK = 2,          ///< Numeric block, compared LSB first (big-endian)
        INTEGER = 3,            ///< 16-bit signed integer
        LONG_INT = 4,           ///< 32-bit signed integer
        STRING = 5,             ///< Null-terminated string (most common)
        LOGICAL = 6,            ///< Boolean value (1 byte, 0=false)
        CHARACTER = 7           ///< Single ASCII character
    };

    //=============================================================================
    // Index Attributes
    //=============================================================================

    /**
     * @brief Index attribute flags
     *
     * These flags control index behavior. They can be combined using the
     * bitwise OR operator.
     *
     * ## UNIQUE Attribute
     * When set, the index rejects duplicate keys. Attempting to append a
     * duplicate key will fail silently (returns false from append()).
     *
     * ## ALLOW_DELETE Attribute
     * When set, deletion operations perform node rebalancing to maintain
     * optimal tree structure. Without this flag:
     * - Deletions may leave nodes underfilled
     * - Tree may become unbalanced over time
     * - Space is not reclaimed efficiently
     *
     * **Recommendation**: Always use ALLOW_DELETE unless you have a specific
     * reason not to (e.g., append-only workload).
     *
     * ## Example
     * ```cpp
     * // Index with unique keys and deletion support
     * auto attrs = IndexAttribute::UNIQUE | IndexAttribute::ALLOW_DELETE;
     * index.initIndex(KeyType::STRING, 50, attrs, 5, 100, 100);
     * ```
     */
    enum class IndexAttribute : uint16_t {
        NONE = 0,               ///< No special attributes
        UNIQUE = 1,             ///< Keys must be unique (duplicates rejected)
        ALLOW_DELETE = 2        ///< Allow key deletion with node rebalancing
    };

    /**
     * @brief Combine index attributes using bitwise OR
     *
     * This operator allows combining multiple attributes:
     * ```cpp
     * auto attrs = IndexAttribute::UNIQUE | IndexAttribute::ALLOW_DELETE;
     * ```
     *
     * @param a First attribute
     * @param b Second attribute
     * @return Combined attributes
     */
    inline IndexAttribute operator|(IndexAttribute a, IndexAttribute b) {
        return static_cast<IndexAttribute>(
            static_cast<uint16_t>(a) | static_cast<uint16_t>(b)
            );
    }

    /**
     * @brief Check if an attribute flag is set
     *
     * @param attrs Combined attributes to check
     * @param flag Specific flag to test for
     * @return true if the flag is set in attrs
     *
     * @example
     * ```cpp
     * if (hasAttribute(attrs, IndexAttribute::UNIQUE)) {
     *     // Handle unique constraint
     * }
     * ```
     */
    inline bool hasAttribute(IndexAttribute attrs, IndexAttribute flag) {
        return (static_cast<uint16_t>(attrs) & static_cast<uint16_t>(flag)) != 0;
    }

    //=============================================================================
    // Position State Flags
    //=============================================================================

    /**
     * @brief Cursor position state flags
     *
     * These flags indicate the current cursor position relative to the index
     * boundaries. They are used by navigation functions (getFirst, getNext, etc.)
     * to determine valid movement directions.
     *
     * ## State Transitions
     * ```
     * Empty index:     BOF | EOF (both set)
     * After getFirst:  BOF set, EOF cleared (unless single item)
     * After getNext:   BOF cleared, EOF set when reaching end
     * After getPrev:   EOF cleared, BOF set when reaching start
     * ```
     *
     * ## Usage
     * ```cpp
     * int64_t pos = index.getFirst(key);
     * while (pos >= 0 && !index.isEOF()) {
     *     // Process key
     *     pos = index.getNext(key);
     * }
     * ```
     */
    enum class PositionState : uint16_t {
        NORMAL = 0,             ///< Normal state (not at boundary)
        END_OF_FILE = 0x0001,   ///< At end of index (no more keys forward)
        BEGIN_OF_FILE = 0x0002  ///< At beginning of index (no more keys backward)
    };

    //=============================================================================
    // Constants
    //=============================================================================

    /**
     * @brief Default number of keys per B-Tree node
     *
     * This determines the "order" of the B-Tree. Higher values mean:
     * - Wider, shallower trees (fewer levels)
     * - Larger nodes (more I/O per node access)
     * - Better cache utilization for sequential keys
     *
     * **Guideline**: Choose based on typical key size and disk block size.
     * Aim for nodes that fit in one or two disk blocks (4KB-8KB).
     */
    constexpr uint16_t DEFAULT_KEYS_PER_NODE = 5;

    /**
     * @brief Default number of keys per leaf block
     *
     * Leaves store individual key-data pointer pairs. This value affects
     * pre-allocation behavior.
     */
    constexpr uint16_t DEFAULT_KEYS_PER_BLOCK = 20;

    /**
     * @brief Invalid file position marker
     *
     * Used to indicate "no position" or "not found" throughout the library.
     * All valid file positions are >= 0, so -1 is a safe sentinel value.
     *
     * @note This matches the traditional Unix convention for invalid positions.
     */
    constexpr int64_t INVALID_POSITION = -1;

    //=============================================================================
    // Utility Functions
    //=============================================================================

    /**
     * @brief Calculate XOR checksum for a memory block
     *
     * This function computes an 8-bit XOR checksum over a block of memory.
     * Used for data integrity verification in file structures.
     *
     * ## Algorithm
     * The checksum is calculated by XORing all bytes together:
     * ```
     * checksum = byte[0] ^ byte[1] ^ byte[2] ^ ... ^ byte[n-1]
     * ```
     *
     * ## Usage Pattern
     * 1. Before writing: Set checksum field to 0, calculate checksum, store it
     * 2. After reading: Calculate checksum including stored value; result should be 0
     *
     * ## Properties
     * - Detects all single-bit errors
     * - Detects most odd numbers of bit errors
     * - Does NOT detect all even numbers of bit errors
     * - Does NOT detect byte reordering
     *
     * ## Why XOR Checksum?
     * - **Pros**: Simple, fast, no dependencies
     * - **Cons**: Weak error detection compared to CRC32
     * - **Use case**: Suitable for detecting accidental corruption, not malicious tampering
     *
     * ## Future Improvement
     * For stronger integrity checking, consider implementing CRC32:
     * ```cpp
     * uint32_t calculateCRC32(const void* block, size_t blockSize);
     * ```
     *
     * @param block Pointer to the memory block
     * @param blockSize Size of the block in bytes
     * @return 8-bit checksum value
     *
     * @example
     * ```cpp
     * // Writing with checksum
     * header.checksum = 0;
     * header.checksum = calculateBlockChecksum(&header, sizeof(header));
     * file.write(&header, sizeof(header), 0);
     *
     * // Verifying checksum
     * file.read(&header, sizeof(header), 0);
     * if (calculateBlockChecksum(&header, sizeof(header)) != 0) {
     *     throw DataCorruptionException("Header corrupted");
     * }
     * ```
     */
    inline uint8_t calculateBlockChecksum(const void* block, size_t blockSize) {
        uint8_t checksum = 0;
        const uint8_t* ptr = static_cast<const uint8_t*>(block);

        for (size_t i = 0; i < blockSize; ++i) {
            checksum ^= ptr[i];
        }

        return checksum;
    }

    //=============================================================================
    // Exception Classes
    //=============================================================================

    /**
     * @brief Base exception class for UDB errors
     *
     * All UDB-specific exceptions derive from this class, which itself
     * derives from std::runtime_error for compatibility with standard
     * exception handling.
     *
     * ## Exception Hierarchy
     * ```
     * std::runtime_error
     * ??? UDBException (base for all UDB exceptions)
     *     ??? FileIOException (file operations)
     *     ??? DataCorruptionException (integrity failures)
     *     ??? MemoryException (allocation failures)
     * ```
     *
     * ## When Exceptions Are Thrown
     * - File cannot be opened/created
     * - Checksum verification fails
     * - I/O operations fail
     * - Memory allocation fails (rare)
     *
     * ## Handling Pattern
     * ```cpp
     * try {
     *     MultiIndex index("data.ndx");
     *     index.append("key", 100);
     * } catch (const DataCorruptionException& e) {
     *     // Handle corruption - file may need repair
     *     std::cerr << "Corruption: " << e.what() << std::endl;
     * } catch (const FileIOException& e) {
     *     // Handle I/O error - check permissions, disk space
     *     std::cerr << "I/O Error: " << e.what() << std::endl;
     * } catch (const UDBException& e) {
     *     // Handle other UDB errors
     *     std::cerr << "UDB Error: " << e.what() << std::endl;
     * }
     * ```
     */
    class UDBException : public std::runtime_error {
    public:
        /**
         * @brief Construct a UDB exception
         *
         * @param code The error code associated with this exception
         * @param message Human-readable error message
         */
        explicit UDBException(ErrorCode code, const std::string& message = "")
            : std::runtime_error(message), m_errorCode(code) {}

        /**
         * @brief Get the error code
         * @return The ErrorCode associated with this exception
         */
        ErrorCode errorCode() const noexcept { return m_errorCode; }

    private:
        ErrorCode m_errorCode;  ///< The specific error that occurred
    };

    /**
     * @brief File I/O exception
     *
     * Thrown when file operations (open, create, read, write, seek) fail.
     * The error code provides specific information about what failed.
     *
     * ## Common Causes
     * - File not found (OPEN_ERROR)
     * - Permission denied (CREATE_ERROR, WRITE_ERROR)
     * - Disk full (WRITE_ERROR)
     * - File locked by another process
     *
     * ## Recovery
     * - Check file path and permissions
     * - Ensure disk has space
     * - Close other applications using the file
     */
    class FileIOException : public UDBException {
    public:
        explicit FileIOException(ErrorCode code, const std::string& message = "")
            : UDBException(code, "File I/O error: " + message) {
        }
    };

    /**
     * @brief Data corruption exception
     *
     * Thrown when checksum verification fails, indicating the file data
     * has been corrupted. This is a serious error that may require:
     * - Restoring from backup
     * - Rebuilding the index
     * - Data recovery procedures
     *
     * ## Common Causes
     * - Incomplete writes (system crash during write)
     * - Disk hardware failure
     * - File modified by external process
     * - Software bugs
     *
     * ## Prevention
     * - Use reliable storage
     * - Implement proper shutdown procedures
     * - Consider write-ahead logging (WAL) for recovery
     */
    class DataCorruptionException : public UDBException {
    public:
        explicit DataCorruptionException(const std::string& message = "")
            : UDBException(ErrorCode::BAD_DATA, "Data corruption: " + message) {
        }
    };

    /**
     * @brief Memory allocation exception
     *
     * Thrown when memory allocation fails. This is rare in modern systems
     * but can occur with very large allocations or memory exhaustion.
     *
     * ## Recovery
     * - Free unused resources
     * - Process data in smaller chunks
     * - Increase available memory
     *
     * @note Most memory allocations in UDB use std::vector which throws
     *       std::bad_alloc. This exception is for explicit allocation failures.
     */
    class MemoryException : public UDBException {
    public:
        explicit MemoryException(const std::string& message = "")
            : UDBException(ErrorCode::MEMORY_ERROR, "Memory error: " + message) {
        }
    };

} // namespace udb

#endif // UDB_COMMON_H
