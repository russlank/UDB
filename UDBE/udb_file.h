/**
 * @file udb_file.h
 * @brief Low-level file I/O abstraction for the UDB library
 *
 * This file provides a platform-independent file I/O layer that abstracts
 * the underlying file system operations. It serves as the foundation for
 * all file-based operations in the UDB library.
 *
 * ## Purpose
 *
 * The File class provides:
 * 1. **Abstraction**: Hides platform-specific file I/O details
 * 2. **RAII**: Automatic resource management (file opened in constructor, closed in destructor)
 * 3. **Thread Safety**: Recursive mutex protection for concurrent access
 * 4. **Error Handling**: Consistent error reporting via codes and exceptions
 *
 * ## Design Decisions
 *
 * ### Why std::fstream?
 * The original DOS code used low-level functions like `_open`, `_read`, `_write`.
 * We chose std::fstream because:
 * - Platform independent (works on Windows, Linux, macOS)
 * - RAII semantics (automatic cleanup)
 * - Buffered I/O (better performance for small operations)
 * - Exception safety
 *
 * ### Thread Safety Model
 *
 * Each file operation acquires a recursive mutex lock. This provides:
 * - **Safety**: No data races on the file handle
 * - **Re-entrancy**: Methods can call other methods without deadlock
 * - **Simplicity**: Easy to understand and maintain
 *
 * **Important**: While individual operations are thread-safe, sequences of
 * operations (read-modify-write) require external synchronization. Use the
 * provided lock accessor for such scenarios:
 *
 * ```cpp
 * {
 *     std::lock_guard<udb::RecursiveMutex> lock(file.getMutex());
 *     // Multiple operations are now atomic
 *     auto data = file.read(...);
 *     // ... modify data ...
 *     file.write(data, ...);
 * }
 * ```
 *
 * ### Position Tracking
 * The file maintains separate read and write positions (via seekg/seekp).
 * We keep them synchronized to avoid confusion, but this adds overhead.
 *
 * ## Usage Example
 *
 * ```cpp
 * // Create a new file
 * File file("data.bin", true);  // true = create new
 * 
 * // Write some data
 * int32_t value = 12345;
 * file.write(&value, sizeof(value), 0);  // Write at position 0
 * 
 * // Read it back
 * int32_t readValue;
 * file.read(&readValue, sizeof(readValue), 0);
 * assert(readValue == value);
 * 
 * // File automatically closed when out of scope
 * ```
 *
 * ## Best Practices
 *
 * 1. **Always check for errors** after operations that might fail
 * 2. **Use RAII**: Let the destructor close the file
 * 3. **Flush important data**: Call flush() after critical writes
 * 4. **Specify positions explicitly**: Avoid relying on current position
 *
 * ## Future Improvements
 *
 * - Memory-mapped file support for large files
 * - Asynchronous I/O for better throughput
 * - File locking for multi-process access
 * - Compression support
 *
 * @author Digixoil
 * @version 2.1.0 (Thread Safety Enhancement)
 * @date 2025
 *
 * @see udb_common.h for error codes and exceptions
 * @see udb_sync.h for synchronization primitives
 * @see udb_btree.h for MultiIndex which inherits from File
 * @see udb_heap.h for HeapFile which inherits from File
 */

#ifndef UDB_FILE_H
#define UDB_FILE_H

#include "udb_common.h"
#include "udb_sync.h"
#include <string>
#include <fstream>
#include <filesystem>

namespace udb {

    /**
     * @brief File open mode flags
     *
     * These flags control how a file is opened. They can be combined
     * using the bitwise OR operator.
     *
     * ## Flag Combinations
     *
     * | Use Case | Flags |
     * |----------|-------|
     * | Read only | READ \| BINARY |
     * | Write only | WRITE \| BINARY |
     * | Read/Write | READ_WRITE \| BINARY |
     * | Create new | CREATE \| TRUNCATE \| READ_WRITE \| BINARY |
     *
     * ## Note on BINARY
     * BINARY mode is always recommended for database files to prevent
     * text-mode transformations (like \n to \r\n on Windows).
     */
    enum class FileMode : uint32_t {
        READ = 0x01,            ///< Open for reading
        WRITE = 0x02,           ///< Open for writing
        READ_WRITE = 0x03,      ///< Open for reading and writing (READ | WRITE)
        CREATE = 0x04,          ///< Create file if it doesn't exist
        TRUNCATE = 0x08,        ///< Truncate existing file to zero length
        BINARY = 0x10           ///< Binary mode (no text transformations)
    };

    /**
     * @brief Combine file mode flags using bitwise OR
     */
    inline FileMode operator|(FileMode a, FileMode b) {
        return static_cast<FileMode>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
    }

    /**
     * @brief Check if a specific mode flag is set
     *
     * @param modes Combined mode flags
     * @param flag Specific flag to check
     * @return true if the flag is set
     */
    inline bool hasMode(FileMode modes, FileMode flag) {
        return (static_cast<uint32_t>(modes) & static_cast<uint32_t>(flag)) != 0;
    }

    /**
     * @brief Low-level file I/O class with thread-safe operations
     *
     * This class provides basic file operations with error handling,
     * position tracking, and thread-safe access through a recursive mutex.
     * It's designed for binary file access with random seek capability.
     *
     * ## Thread Safety
     *
     * This class is **thread-safe** for individual operations. Each public
     * method acquires the internal mutex before accessing file state.
     *
     * **Safe patterns:**
     * ```cpp
     * // Different threads can call methods concurrently
     * // Thread 1              // Thread 2
     * file.write(a, 100, 0);   file.write(b, 100, 200);  // Safe
     * file.read(&x, 4, 0);     file.read(&y, 4, 200);    // Safe
     * ```
     *
     * **Patterns requiring external synchronization:**
     * ```cpp
     * // Read-modify-write must be externally synchronized
     * {
     *     std::lock_guard<RecursiveMutex> lock(file.getMutex());
     *     file.read(&header, sizeof(header), 0);
     *     header.count++;
     *     file.write(&header, sizeof(header), 0);
     * }
     * ```
     *
     * ## Inheritance
     *
     * File is the base class for:
     * - MultiIndex: B-Tree index files
     * - HeapFile: Variable-length record storage
     *
     * ### Class Hierarchy
     * ```
     * File (base)
     * ├── MultiIndex (B-Tree index)
     * └── HeapFile (Record storage)
     * ```
     *
     * ## Memory Model
     *
     * The File class owns:
     * - The file handle (m_file)
     * - The filename string (m_filename)
     * - The mutex for thread safety (m_mutex)
     * - Error state (m_errorCode)
     *
     * Derived classes add their own data structures but share the file I/O.
     *
     * ## Exception Safety
     *
     * - **Constructor**: May throw FileIOException if file cannot be opened
     * - **Destructor**: No-throw (catches and ignores errors)
     * - **Operations**: May throw FileIOException on I/O errors
     */
    class UDB_THREAD_SAFE File {
    public:
        /**
         * @brief Construct a file object
         *
         * Opens or creates a file based on the parameters.
         *
         * ## Behavior
         * - If createNew is true: Creates a new file, truncating if it exists
         * - If createNew is false: Opens an existing file (must exist)
         *
         * ## Error Handling
         * Throws FileIOException if:
         * - createNew is false and file doesn't exist
         * - createNew is true and cannot create file
         * - File permissions don't allow requested mode
         *
         * ## Thread Safety
         * Constructor is not thread-safe (object is not yet constructed).
         * Do not share the filename string while constructing.
         *
         * @param filename Path to the file (relative or absolute)
         * @param createNew If true, create a new file (truncate if exists)
         * @param mode File open mode flags (default: READ_WRITE | BINARY)
         *
         * @throws FileIOException if file cannot be opened/created
         *
         * @example
         * ```cpp
         * // Open existing file for read/write
         * File existing("data.bin");
         *
         * // Create new file (overwrites if exists)
         * File newFile("output.bin", true);
         *
         * // Open read-only
         * File readOnly("config.bin", false, FileMode::READ | FileMode::BINARY);
         * ```
         */
        File(const std::string& filename, bool createNew = false,
            FileMode mode = FileMode::READ_WRITE | FileMode::BINARY);

        /**
         * @brief Destructor - closes the file
         *
         * The destructor ensures the file is properly closed. Any errors
         * during close are silently ignored to maintain no-throw guarantee.
         *
         * ## RAII Guarantee
         * Files are always closed when the File object goes out of scope,
         * even if an exception is thrown.
         *
         * ## Thread Safety
         * Destructor waits for any in-progress operations to complete
         * before closing the file.
         *
         * @note If you need to handle close errors, call close() explicitly
         *       before the destructor.
         */
        virtual ~File();

        // Disable copy (files are non-copyable resources)
        File(const File&) = delete;
        File& operator=(const File&) = delete;

        /**
         * @brief Move constructor
         *
         * Transfers ownership of the file from another File object.
         * The source object is left in a valid but closed state.
         *
         * ## Thread Safety
         * Not thread-safe. Do not move a File while other threads may be using it.
         *
         * @param other File to move from
         */
        File(File&& other) noexcept;

        /**
         * @brief Move assignment operator
         *
         * Closes current file (if open) and takes ownership from another.
         *
         * ## Thread Safety
         * Not thread-safe. Do not move-assign while other threads may be using
         * either this object or the source object.
         *
         * @param other File to move from
         * @return Reference to this
         */
        File& operator=(File&& other) noexcept;

        /**
         * @brief Get current file position
         *
         * Returns the current read/write position in the file.
         *
         * ## Thread Safety
         * Thread-safe, but the position may change immediately after return
         * if other threads are accessing the file.
         *
         * @return Current position in bytes from start of file
         */
        int64_t position() const;

        /**
         * @brief Get file size
         *
         * Returns the total size of the file in bytes.
         *
         * ## Implementation Note
         * This seeks to end of file to determine size, then restores
         * the original position. This adds overhead but is portable.
         *
         * ## Thread Safety
         * Thread-safe. The size is consistent at the moment of the call,
         * but may change if other threads write to the file.
         *
         * @return Size of file in bytes
         */
        int64_t size() const;

        /**
         * @brief Seek to a position in the file
         *
         * Moves the read/write position to the specified location.
         *
         * ## Origin Values
         * - SEEK_SET (0): Position relative to start of file
         * - SEEK_CUR (1): Position relative to current position
         * - SEEK_END (2): Position relative to end of file
         *
         * ## Thread Safety
         * Thread-safe, but consider that other threads may change the
         * position before you perform the next operation. For atomic
         * seek+read or seek+write, use read() or write() with position.
         *
         * @param pos Position to seek to
         * @param origin Seek origin (SEEK_SET, SEEK_CUR, SEEK_END)
         * @return New position after seek
         *
         * @throws FileIOException on seek error
         *
         * @example
         * ```cpp
         * file.seek(0, SEEK_SET);     // Go to beginning
         * file.seek(0, SEEK_END);     // Go to end
         * file.seek(-10, SEEK_END);   // Go to 10 bytes before end
         * ```
         */
        int64_t seek(int64_t pos, int origin = SEEK_SET);

        /**
         * @brief Write data to file
         *
         * Writes a block of data to the file at the specified position.
         *
         * ## Position Behavior
         * - If pos == INVALID_POSITION (-1): Writes at current position
         * - Otherwise: Seeks to pos before writing
         *
         * ## Error Handling
         * Throws FileIOException if the write fails (disk full, permission denied, etc.)
         *
         * ## Thread Safety
         * Thread-safe. The seek and write are atomic when position is specified.
         *
         * @param buffer Data to write
         * @param size Number of bytes to write
         * @param pos Position to write at (-1 for current position)
         *
         * @throws FileIOException on write error
         *
         * @example
         * ```cpp
         * int32_t header = 0x12345678;
         * file.write(&header, sizeof(header), 0);  // Write at start
         * file.write(data, dataSize);              // Write at current position
         * ```
         */
        void write(const void* buffer, size_t size, int64_t pos = INVALID_POSITION);

        /**
         * @brief Read data from file
         *
         * Reads a block of data from the file at the specified position.
         *
         * ## Position Behavior
         * - If pos == INVALID_POSITION (-1): Reads from current position
         * - Otherwise: Seeks to pos before reading
         *
         * ## Return Value
         * Returns the actual number of bytes read, which may be less than
         * requested if end of file is reached.
         *
         * ## Error Handling
         * Throws FileIOException if a read error occurs (not including EOF).
         *
         * ## Thread Safety
         * Thread-safe. The seek and read are atomic when position is specified.
         *
         * @param buffer Buffer to read into (must be at least 'size' bytes)
         * @param size Number of bytes to read
         * @param pos Position to read from (-1 for current position)
         * @return Number of bytes actually read
         *
         * @throws FileIOException on read error
         *
         * @example
         * ```cpp
         * int32_t header;
         * file.read(&header, sizeof(header), 0);  // Read from start
         *
         * char buffer[1024];
         * size_t bytesRead = file.read(buffer, sizeof(buffer));  // May read less
         * ```
         */
        size_t read(void* buffer, size_t size, int64_t pos = INVALID_POSITION);

        /**
         * @brief Flush all buffered data to disk
         *
         * Forces any buffered data to be written to the physical file.
         * Use this after critical writes to ensure data persistence.
         *
         * ## When to Flush
         * - After writing important data structures
         * - Before signaling completion to another process
         * - Periodically during long operations
         *
         * ## Performance Note
         * Flushing is expensive. Don't call after every write.
         *
         * ## Thread Safety
         * Thread-safe.
         */
        void flush();

        /**
         * @brief Check if file is open and valid
         *
         * ## Thread Safety
         * Thread-safe, but state may change after return.
         *
         * @return true if file is open and ready for I/O
         */
        bool isOpen() const;

        /**
         * @brief Get the filename
         *
         * ## Thread Safety
         * Thread-safe. Filename never changes after construction.
         *
         * @return The filename or path passed to the constructor
         */
        const std::string& filename() const { return m_filename; }

        /**
         * @brief Get current error code
         *
         * Returns the error code from the last failed operation.
         * Returns ErrorCode::OK if no error has occurred.
         *
         * ## Thread Safety
         * Thread-safe read, but error state may be changed by other threads.
         *
         * @return Current error code
         */
        ErrorCode getError() const { return m_errorCode; }

        /**
         * @brief Set error code
         *
         * Used internally to record errors. Can also be used by
         * derived classes.
         *
         * ## Thread Safety
         * Thread-safe write.
         *
         * @param code Error code to set
         */
        void setError(ErrorCode code) { m_errorCode = code; }

        /**
         * @brief Clear error state
         *
         * Resets the error code to OK. Use this after handling an error
         * to allow subsequent operations.
         *
         * ## Thread Safety
         * Thread-safe.
         */
        void clearError() { m_errorCode = ErrorCode::OK; }

        /**
         * @brief Check if there's an error
         *
         * Convenience function equivalent to `getError() != ErrorCode::OK`.
         *
         * ## Thread Safety
         * Thread-safe read.
         *
         * @return true if an error has occurred
         */
        bool hasError() const { return m_errorCode != ErrorCode::OK; }

        /**
         * @brief Get the mutex for external synchronization
         *
         * Returns a reference to the internal mutex, allowing callers to
         * perform compound operations atomically.
         *
         * ## Usage
         * ```cpp
         * {
         *     std::lock_guard<RecursiveMutex> lock(file.getMutex());
         *     // All operations here are atomic
         *     file.read(&header, sizeof(header), 0);
         *     header.count++;
         *     file.write(&header, sizeof(header), 0);
         * }
         * ```
         *
         * ## Warning
         * - Do not hold the mutex for extended periods
         * - Do not call external code while holding the mutex
         * - Be careful of deadlocks with other mutexes
         *
         * @return Reference to the internal recursive mutex
         */
        RecursiveMutex& getMutex() const { return m_mutex; }

    protected:
        /**
         * @brief Close the file (called by destructor)
         *
         * Closes the file handle. Safe to call multiple times.
         * Derived classes can override to add cleanup behavior.
         *
         * ## Thread Safety
         * Thread-safe. Acquires the mutex before closing.
         *
         * @note This is virtual so derived classes can add their own
         *       cleanup (e.g., writing final data).
         */
        virtual void close();

    private:
        std::string m_filename;                     ///< Path to the file
        mutable std::fstream m_file;                ///< File stream (mutable for const methods)
        mutable RecursiveMutex m_mutex;             ///< Recursive mutex for thread safety
        ErrorCode m_errorCode = ErrorCode::OK;      ///< Current error state
        bool m_isOpen = false;                      ///< True if file is open
    };

} // namespace udb

#endif // UDB_FILE_H
