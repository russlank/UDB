/**
 * @file udb.h
 * @brief Main include file for the UDB (Ultra Database) library
 *
 * Include this single header to access all UDB functionality.
 *
 * ## UDB Library Overview
 *
 * UDB (Ultra Database) is a lightweight database engine providing:
 * - **B-Tree Indexing**: Fast key-based access with multiple key types
 * - **Heap File Storage**: Dynamic variable-length record storage
 * - **Multi-Index Support**: Multiple indexes per file
 *
 * ## Quick Start
 *
 * ```cpp
 * #include "udb.h"
 * using namespace udb;
 *
 * // Create an index file with one index
 * MultiIndex index("mydata.ndx", 1);
 * index.setActiveIndex(1);
 * index.initIndex(KeyType::STRING, 50, IndexAttribute::ALLOW_DELETE, 5, 100, 100);
 *
 * // Add some data
 * index.append("Alice", 1000);
 * index.append("Bob", 2000);
 * index.append("Charlie", 3000);
 *
 * // Find data
 * int64_t pos = index.find("Bob");
 * if (pos >= 0) {
 *     std::cout << "Found Bob at position " << pos << std::endl;
 * }
 *
 * // Iterate through all keys
 * pos = index.getFirst(nullptr);
 * while (pos >= 0 && !index.isEOF()) {
 *     char key[51];
 *     pos = index.getCurrent(key);
 *     std::cout << "Key: " << key << " at " << pos << std::endl;
 *     pos = index.getNext(nullptr);
 * }
 * ```
 *
 * @author Digixoil
 * @version 2.0.0 (Modernized for Visual Studio 2025)
 * @date 2025
 */

#ifndef UDB_H
#define UDB_H

 // Core components
#include "udb_common.h"
#include "udb_file.h"
#include "udb_heap.h"
#include "udb_btree.h"

/**
 * @namespace udb
 * @brief Ultra Database library namespace
 *
 * All UDB classes, functions, and types are contained within this namespace.
 */
namespace udb {

    /**
     * @brief Library version information
     */
    struct Version {
        static constexpr int MAJOR = 2;
        static constexpr int MINOR = 0;
        static constexpr int PATCH = 0;
        static constexpr const char* STRING = "2.0.0";
        static constexpr const char* NAME = "UDB - Ultra Database Engine";
    };

    /**
     * @brief Get library version string
     * @return Version string (e.g., "2.0.0")
     */
    inline const char* getVersion() {
        return Version::STRING;
    }

    /**
     * @brief Get library name
     * @return Library name string
     */
    inline const char* getLibraryName() {
        return Version::NAME;
    }

} // namespace udb

#endif // UDB_H
