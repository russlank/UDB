/**
 * @file udb_common.h
 * @brief Common definitions, types, and utilities for the UDB library
 * 
 * This file contains all common definitions, error codes, type definitions,
 * and utility functions used throughout the UDB (Ultra Database) library.
 * 
 * @author Digixoil
 * @version 2.0.0 (Modernized for Visual Studio 2025)
 * @date 2025
 * 
 * @copyright Original code preserved under old-code directory
 */

#ifndef UDB_COMMON_H
#define UDB_COMMON_H

#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <memory>
#include <stdexcept>

// Platform-specific definitions
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
 */
enum class ErrorCode : int {
    OK = 0,                 ///< No error
    ERROR = 1,              ///< Generic error
    READ_ERROR = 2,         ///< File read error
    WRITE_ERROR = 3,        ///< File write error
    SEEK_ERROR = 4,         ///< File seek error
    BAD_DATA = 5,           ///< Data corruption detected (checksum mismatch)
    MEMORY_ERROR = 6,       ///< Memory allocation failed
    POINTER_ERROR = 7,      ///< Invalid pointer
    BAD_FILE_DATA = 8,      ///< File data is invalid
    BAD_FILE_HANDLE = 9,    ///< Invalid file handle
    CREATE_ERROR = 10,      ///< File creation failed
    GET_FILE_SIZE = 11,     ///< Failed to get file size
    OPEN_ERROR = 12,        ///< File open failed
    CLOSE_ERROR = 13,       ///< File close failed
    GET_FILE_POS = 14,      ///< Failed to get file position
    INIT_ERROR = 15         ///< Initialization error
};

/**
 * @brief Error type categories for filtering
 */
enum class ErrorType : uint16_t {
    INPUT = 0x0001,         ///< Input-related errors
    OUTPUT = 0x0002,        ///< Output-related errors
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
 */
enum class KeyType : uint16_t {
    VOID = 0,               ///< No type (invalid)
    BLOCK = 1,              ///< Raw byte block comparison
    NUM_BLOCK = 2,          ///< Numeric block (big-endian comparison)
    INTEGER = 3,            ///< 16-bit integer
    LONG_INT = 4,           ///< 32-bit integer
    STRING = 5,             ///< Null-terminated string
    LOGICAL = 6,            ///< Boolean value
    CHARACTER = 7           ///< Single character
};

//=============================================================================
// Index Attributes
//=============================================================================

/**
 * @brief Index attribute flags
 */
enum class IndexAttribute : uint16_t {
    NONE = 0,               ///< No special attributes
    UNIQUE = 1,             ///< Keys must be unique
    ALLOW_DELETE = 2        ///< Allow key deletion (enables node balancing)
};

/**
 * @brief Combine index attributes using bitwise OR
 */
inline IndexAttribute operator|(IndexAttribute a, IndexAttribute b) {
    return static_cast<IndexAttribute>(
        static_cast<uint16_t>(a) | static_cast<uint16_t>(b)
    );
}

/**
 * @brief Check if an attribute flag is set
 */
inline bool hasAttribute(IndexAttribute attrs, IndexAttribute flag) {
    return (static_cast<uint16_t>(attrs) & static_cast<uint16_t>(flag)) != 0;
}

//=============================================================================
// Position State Flags
//=============================================================================

/**
 * @brief Cursor position state flags
 */
enum class PositionState : uint16_t {
    NORMAL = 0,             ///< Normal state
    END_OF_FILE = 0x0001,   ///< At end of index
    BEGIN_OF_FILE = 0x0002  ///< At beginning of index
};

//=============================================================================
// Constants
//=============================================================================

constexpr uint16_t DEFAULT_KEYS_PER_NODE = 5;    ///< Default keys per B-Tree node
constexpr uint16_t DEFAULT_KEYS_PER_BLOCK = 20;  ///< Default keys per leaf block
constexpr int64_t INVALID_POSITION = -1;         ///< Invalid file position marker

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Calculate XOR checksum for a memory block
 * 
 * This function computes an 8-bit XOR checksum over a block of memory.
 * Used for data integrity verification in file structures.
 * 
 * @param block Pointer to the memory block
 * @param blockSize Size of the block in bytes
 * @return 8-bit checksum value
 * 
 * @note When verifying, if the stored checksum is included in the block,
 *       the result should be 0 for valid data.
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
 */
class UDBException : public std::runtime_error {
public:
    explicit UDBException(ErrorCode code, const std::string& message = "")
        : std::runtime_error(message), m_errorCode(code) {}
    
    ErrorCode errorCode() const noexcept { return m_errorCode; }
    
private:
    ErrorCode m_errorCode;
};

/**
 * @brief File I/O exception
 */
class FileIOException : public UDBException {
public:
    explicit FileIOException(ErrorCode code, const std::string& message = "")
        : UDBException(code, "File I/O error: " + message) {}
};

/**
 * @brief Data corruption exception
 */
class DataCorruptionException : public UDBException {
public:
    explicit DataCorruptionException(const std::string& message = "")
        : UDBException(ErrorCode::BAD_DATA, "Data corruption: " + message) {}
};

/**
 * @brief Memory allocation exception
 */
class MemoryException : public UDBException {
public:
    explicit MemoryException(const std::string& message = "")
        : UDBException(ErrorCode::MEMORY_ERROR, "Memory error: " + message) {}
};

} // namespace udb

#endif // UDB_COMMON_H
