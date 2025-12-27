/**
 * @file udb_file.h
 * @brief Low-level file I/O abstraction for the UDB library
 * 
 * This file provides a platform-independent file I/O layer that abstracts
 * the underlying file system operations. It serves as the foundation for
 * all file-based operations in the UDB library.
 * 
 * @author Digixoil
 * @version 2.0.0 (Modernized for Visual Studio 2025)
 * @date 2025
 */

#ifndef UDB_FILE_H
#define UDB_FILE_H

#include "udb_common.h"
#include <string>
#include <fstream>
#include <filesystem>
#include <mutex>

namespace udb {

/**
 * @brief File open mode flags
 */
enum class FileMode : uint32_t {
    READ = 0x01,            ///< Open for reading
    WRITE = 0x02,           ///< Open for writing
    READ_WRITE = 0x03,      ///< Open for reading and writing
    CREATE = 0x04,          ///< Create file if it doesn't exist
    TRUNCATE = 0x08,        ///< Truncate existing file
    BINARY = 0x10           ///< Binary mode (default)
};

inline FileMode operator|(FileMode a, FileMode b) {
    return static_cast<FileMode>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline bool hasMode(FileMode modes, FileMode flag) {
    return (static_cast<uint32_t>(modes) & static_cast<uint32_t>(flag)) != 0;
}

/**
 * @brief Low-level file I/O class
 * 
 * This class provides basic file operations with error handling and
 * position tracking. It's designed for binary file access with random
 * seek capability.
 * 
 * ## Design Notes
 * 
 * The original code used DOS/POSIX file I/O functions (_open, _read, etc.).
 * This modernized version uses C++ streams for better portability and
 * RAII semantics.
 * 
 * ## Thread Safety
 * 
 * Individual file operations are thread-safe through internal locking.
 * However, sequences of operations (read-modify-write) require external
 * synchronization.
 */
class UDB_API File {
public:
    /**
     * @brief Construct a file object
     * 
     * @param filename Path to the file
     * @param createNew If true, create a new file (truncate if exists)
     * @param mode File open mode flags
     * 
     * @throws FileIOException if file cannot be opened/created
     */
    File(const std::string& filename, bool createNew = false, 
         FileMode mode = FileMode::READ_WRITE | FileMode::BINARY);
    
    /**
     * @brief Destructor - closes the file
     */
    virtual ~File();
    
    // Disable copy
    File(const File&) = delete;
    File& operator=(const File&) = delete;
    
    // Enable move
    File(File&& other) noexcept;
    File& operator=(File&& other) noexcept;
    
    /**
     * @brief Get current file position
     * @return Current position in bytes from start of file
     */
    int64_t position() const;
    
    /**
     * @brief Get file size
     * @return Size of file in bytes
     */
    int64_t size() const;
    
    /**
     * @brief Seek to a position in the file
     * 
     * @param pos Position to seek to
     * @param origin Seek origin (SEEK_SET, SEEK_CUR, SEEK_END)
     * @return New position after seek
     * 
     * @throws FileIOException on seek error
     */
    int64_t seek(int64_t pos, int origin = SEEK_SET);
    
    /**
     * @brief Write data to file
     * 
     * @param buffer Data to write
     * @param size Number of bytes to write
     * @param pos Position to write at (-1 for current position)
     * 
     * @throws FileIOException on write error
     */
    void write(const void* buffer, size_t size, int64_t pos = INVALID_POSITION);
    
    /**
     * @brief Read data from file
     * 
     * @param buffer Buffer to read into
     * @param size Number of bytes to read
     * @param pos Position to read from (-1 for current position)
     * @return Number of bytes actually read
     * 
     * @throws FileIOException on read error
     */
    size_t read(void* buffer, size_t size, int64_t pos = INVALID_POSITION);
    
    /**
     * @brief Flush all buffered data to disk
     */
    void flush();
    
    /**
     * @brief Check if file is open and valid
     */
    bool isOpen() const;
    
    /**
     * @brief Get the filename
     */
    const std::string& filename() const { return m_filename; }
    
    /**
     * @brief Get current error code
     */
    ErrorCode getError() const { return m_errorCode; }
    
    /**
     * @brief Set error code
     */
    void setError(ErrorCode code) { m_errorCode = code; }
    
    /**
     * @brief Clear error state
     */
    void clearError() { m_errorCode = ErrorCode::OK; }
    
    /**
     * @brief Check if there's an error
     */
    bool hasError() const { return m_errorCode != ErrorCode::OK; }

protected:
    /**
     * @brief Close the file (called by destructor)
     */
    virtual void close();

private:
    std::string m_filename;
    mutable std::fstream m_file;
    mutable std::mutex m_mutex;
    ErrorCode m_errorCode = ErrorCode::OK;
    bool m_isOpen = false;
};

} // namespace udb

#endif // UDB_FILE_H
