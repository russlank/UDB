/**
 * @file udb_sync.h
 * @brief Thread synchronization primitives for the UDB library
 *
 * This file provides thread-safe synchronization utilities for concurrent
 * access to UDB data structures. It defines mutex types, lock guards, and
 * establishes the library's thread safety model.
 *
 * ## Thread Safety Model
 *
 * The UDB library provides **class-level thread safety**, meaning:
 * - Individual method calls on a single object are atomic
 * - Multiple threads can safely call methods on the SAME object
 * - No external synchronization is required for single operations
 *
 * ## Locking Strategy
 *
 * The library uses **recursive mutexes** because:
 * 1. Public methods may call other public methods internally
 * 2. Callback functions might re-enter the same object
 * 3. Simplifies implementation without risking deadlocks from self-calls
 *
 * ## Usage Patterns
 *
 * ### Safe (Automatic locking):
 * ```cpp
 * // Thread 1                    // Thread 2
 * index.append("A", 100);        index.append("B", 200);  // Safe
 * index.find("A");               index.find("B");         // Safe
 * ```
 *
 * ### Unsafe (Requires external synchronization):
 * ```cpp
 * // Read-modify-write requires external lock
 * std::lock_guard<std::mutex> lock(myMutex);
 * auto pos = index.find("A");
 * if (pos >= 0) {
 *     index.deleteKey("A");  // Atomic with the find
 * }
 * ```
 *
 * ## Performance Considerations
 *
 * - **Mutex overhead**: Small per-operation overhead (~100ns typical)
 * - **Contention**: High contention may cause thread serialization
 * - **Recursive mutex**: Slightly higher overhead than non-recursive
 *
 * ## Future Improvements
 *
 * - Reader-writer locks for read-heavy workloads
 * - Fine-grained page-level locking
 * - Lock-free algorithms for critical paths
 *
 * @author Digixoil
 * @version 2.1.0 (Thread Safety Enhancement)
 * @date 2025
 *
 * @see udb_file.h for base file operations
 * @see udb_btree.h for MultiIndex thread safety
 * @see udb_heap.h for HeapFile thread safety
 */

#ifndef UDB_SYNC_H
#define UDB_SYNC_H

#include <mutex>
#include <shared_mutex>

namespace udb {

    //=============================================================================
    // Mutex Type Aliases
    //=============================================================================

    /**
     * @brief Recursive mutex type for UDB classes
     *
     * We use std::recursive_mutex to allow:
     * - Public methods calling other public methods
     * - Re-entrant callbacks
     * - Nested locking without deadlock
     *
     * ## Example
     * ```cpp
     * class MyClass {
     *     mutable RecursiveMutex m_mutex;
     *
     *     void method1() {
     *         LockGuard lock(m_mutex);
     *         method2();  // Safe - recursive mutex allows this
     *     }
     *
     *     void method2() {
     *         LockGuard lock(m_mutex);
     *         // ...
     *     }
     * };
     * ```
     */
    using RecursiveMutex = std::recursive_mutex;

    /**
     * @brief Read-write mutex for concurrent read access
     *
     * Allows multiple readers OR single writer:
     * - Multiple threads can read simultaneously
     * - Writers have exclusive access
     * - Writers wait for all readers to finish
     *
     * @note Currently unused - reserved for future optimization
     */
    using SharedMutex = std::shared_mutex;

    //=============================================================================
    // Lock Guard Aliases
    //=============================================================================

    /**
     * @brief RAII lock guard for exclusive access
     *
     * Acquires the lock in constructor, releases in destructor.
     * Guarantees the lock is released even if exceptions are thrown.
     *
     * ## Usage
     * ```cpp
     * void safeMethod() {
     *     LockGuard lock(m_mutex);
     *     // Protected code here
     * }  // Lock automatically released
     * ```
     *
     * ## Exception Safety
     * If an exception is thrown while holding the lock, the destructor
     * still runs and releases the lock (RAII guarantee).
     */
    using LockGuard = std::lock_guard<RecursiveMutex>;

    /**
     * @brief RAII unique lock for exclusive access with additional features
     *
     * Similar to LockGuard but supports:
     * - Deferred locking: `UniqueLock lock(mutex, std::defer_lock);`
     * - Try-locking: `lock.try_lock();`
     * - Manual unlock/relock: `lock.unlock(); ... lock.lock();`
     * - Condition variable compatibility
     *
     * ## When to Use
     * - Need to conditionally acquire the lock
     * - Need to release lock before scope ends
     * - Working with condition variables
     *
     * ## Example
     * ```cpp
     * void methodWithOptionalLock(bool needLock) {
     *     UniqueLock lock(m_mutex, std::defer_lock);
     *     if (needLock) {
     *         lock.lock();
     *     }
     *     // ...
     * }
     * ```
     */
    using UniqueLock = std::unique_lock<RecursiveMutex>;

    /**
     * @brief RAII shared lock for read-only access
     *
     * Acquires shared (read) access - multiple threads can hold
     * a shared lock simultaneously.
     *
     * ## Usage
     * ```cpp
     * int readData() {
     *     SharedLock lock(m_sharedMutex);
     *     return m_data;  // Multiple readers allowed
     * }
     *
     * void writeData(int value) {
     *     ExclusiveLock lock(m_sharedMutex);
     *     m_data = value;  // Exclusive access required
     * }
     * ```
     *
     * @note Currently unused - reserved for future read-heavy optimization
     */
    using SharedLock = std::shared_lock<SharedMutex>;

    /**
     * @brief RAII exclusive lock for read-write mutex
     *
     * Acquires exclusive (write) access to a shared mutex.
     * No other readers or writers can access while held.
     */
    using ExclusiveLock = std::unique_lock<SharedMutex>;

    //=============================================================================
    // Thread Safety Documentation Macros
    //=============================================================================

    /**
     * @def UDB_THREAD_SAFE
     * @brief Marks a class or method as thread-safe
     *
     * When applied to a class, indicates all public methods are thread-safe.
     * When applied to a method, indicates that specific method is thread-safe.
     *
     * @note This is a documentation macro with no runtime effect.
     */
    #define UDB_THREAD_SAFE

    /**
     * @def UDB_NOT_THREAD_SAFE
     * @brief Marks a class or method as NOT thread-safe
     *
     * Indicates external synchronization is required for concurrent access.
     *
     * @note This is a documentation macro with no runtime effect.
     */
    #define UDB_NOT_THREAD_SAFE

    /**
     * @def UDB_REQUIRES_LOCK
     * @brief Indicates a method must be called while holding a lock
     *
     * Used for internal/protected methods that assume the caller
     * already holds the appropriate lock.
     *
     * @note This is a documentation macro with no runtime effect.
     */
    #define UDB_REQUIRES_LOCK

    //=============================================================================
    // Scoped Lock Helper
    //=============================================================================

    /**
     * @brief Helper class for conditional locking
     *
     * Allows creating a lock that can be conditionally enabled.
     * Useful when the same code path is used for both locked and
     * unlocked execution.
     *
     * ## Usage
     * ```cpp
     * void processData(bool needLock) {
     *     ConditionalLock lock(m_mutex, needLock);
     *     // Code here is protected only if needLock is true
     * }
     * ```
     */
    class ConditionalLock {
    public:
        /**
         * @brief Construct a conditional lock
         *
         * @param mutex The mutex to potentially lock
         * @param doLock If true, acquire the lock; if false, do nothing
         */
        ConditionalLock(RecursiveMutex& mutex, bool doLock)
            : m_mutex(doLock ? &mutex : nullptr)
        {
            if (m_mutex) {
                m_mutex->lock();
            }
        }

        /**
         * @brief Destructor - releases lock if acquired
         */
        ~ConditionalLock()
        {
            if (m_mutex) {
                m_mutex->unlock();
            }
        }

        // Non-copyable, non-movable
        ConditionalLock(const ConditionalLock&) = delete;
        ConditionalLock& operator=(const ConditionalLock&) = delete;
        ConditionalLock(ConditionalLock&&) = delete;
        ConditionalLock& operator=(ConditionalLock&&) = delete;

        /**
         * @brief Check if the lock is held
         * @return true if the mutex was locked
         */
        bool isLocked() const { return m_mutex != nullptr; }

    private:
        RecursiveMutex* m_mutex;    ///< Pointer to mutex (nullptr if not locking)
    };

} // namespace udb

#endif // UDB_SYNC_H
