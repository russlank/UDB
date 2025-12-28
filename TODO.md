# UDB Development Roadmap

This document outlines planned improvements, potential enhancements, and areas where contributions are welcome.

## ?? Current Status

**Version**: 2.1.0  
**Test Status**: ? 45/45 tests passing (100%)  
**Last Updated**: 2025

---

## ?? High Priority

### Performance Improvements

- [ ] **Fine-grained locking**
  - Current: Single recursive mutex for all operations
  - Goal: Reader-writer locks to allow concurrent reads
  - Complexity: Medium
  - Impact: Significant performance improvement for read-heavy workloads

- [ ] **Memory-mapped file support**
  - Current: Standard file I/O with explicit read/write calls
  - Goal: Optional mmap support for large files
  - Complexity: Medium
  - Impact: Reduced system call overhead, better OS caching

- [ ] **Buffer pool / page cache**
  - Current: No caching, each operation reads from disk
  - Goal: LRU cache for frequently accessed nodes
  - Complexity: Medium-High
  - Impact: Dramatic performance improvement for repeated operations

### Data Integrity

- [ ] **Write-ahead logging (WAL)**
  - Current: No crash recovery
  - Goal: Transaction log for recovery after crashes
  - Complexity: High
  - Impact: Data durability guarantees

- [ ] **Periodic checkpointing**
  - Current: Data written on close only
  - Goal: Configurable checkpoint intervals
  - Complexity: Low-Medium
  - Impact: Reduced data loss window

---

## ?? Medium Priority

### Heap File Improvements

- [ ] **Hole coalescing**
  - Current: Adjacent free spaces remain separate
  - Goal: Merge adjacent holes to reduce fragmentation
  - Complexity: Medium
  - Impact: Better space utilization

- [ ] **File compaction**
  - Current: No way to reclaim fragmented space
  - Goal: Online or offline compaction utility
  - Complexity: Medium-High
  - Impact: Reduced file sizes over time

- [ ] **Best-fit allocation strategy**
  - Current: First-fit allocation from holes table
  - Goal: Configurable allocation strategies
  - Complexity: Low
  - Impact: Reduced fragmentation

### B-Tree Enhancements

- [ ] **Bulk loading**
  - Current: Single-key insertion only
  - Goal: Efficient loading of pre-sorted data
  - Complexity: Medium
  - Impact: Faster initial data loading

- [ ] **Range queries**
  - Current: Point queries and sequential scan
  - Goal: `findRange(lowKey, highKey)` API
  - Complexity: Low
  - Impact: Better query flexibility

- [ ] **Prefix compression**
  - Current: Full keys stored in every entry
  - Goal: Store common prefixes once per node
  - Complexity: Medium
  - Impact: More keys per node, shallower trees

### API Improvements

- [ ] **Iterator pattern**
  - Current: Manual `getFirst()`/`getNext()` calls
  - Goal: C++ iterator interface for range-based for loops
  - Complexity: Low
  - Impact: More idiomatic C++ usage

- [ ] **Async operations**
  - Current: Synchronous blocking API
  - Goal: `std::future`-based async methods
  - Complexity: Medium
  - Impact: Better integration with async applications

---

## ?? Low Priority / Nice to Have

### Cross-Platform

- [ ] **Big-endian support**
  - Current: Assumes little-endian (x86/x64)
  - Goal: Portable file format
  - Complexity: Low-Medium
  - Impact: Broader platform support

- [ ] **32-bit build support**
  - Current: Tested on x64 only
  - Goal: Verified 32-bit builds
  - Complexity: Low
  - Impact: Embedded systems support

### Testing

- [ ] **Stress tests**
  - Current: Functional tests only
  - Goal: Long-running stress tests with random operations
  - Complexity: Low
  - Impact: Better reliability confidence

- [ ] **Fuzzing harness**
  - Current: No fuzz testing
  - Goal: AFL/libFuzzer integration
  - Complexity: Medium
  - Impact: Security and robustness

- [ ] **Performance benchmarks**
  - Current: No performance metrics
  - Goal: Reproducible benchmarks with baselines
  - Complexity: Low
  - Impact: Performance regression detection

- [ ] **Additional key type tests**
  - Current: STRING and LONG_INT well-tested
  - Goal: Comprehensive tests for all key types
  - Complexity: Low
  - Impact: Better test coverage

### Documentation

- [ ] **API documentation generation**
  - Current: Comments in headers
  - Goal: Doxygen-generated HTML docs
  - Complexity: Low
  - Impact: Better discoverability

- [ ] **Tutorial/walkthrough**
  - Current: Basic examples in README
  - Goal: Step-by-step tutorial with sample application
  - Complexity: Low
  - Impact: Easier onboarding

- [ ] **Performance tuning guide**
  - Current: Default parameters only
  - Goal: Guide for choosing maxItems, pre-allocation, etc.
  - Complexity: Low
  - Impact: Better user experience

---

## ?? Test Coverage Expansion

### Current Test Coverage

| Category | Tests | Status |
|----------|-------|--------|
| B-Tree Construction | 4 | ? |
| B-Tree Append | 4 | ? |
| B-Tree Unique Keys | 3 | ? |
| B-Tree Navigation | 4 | ? |
| B-Tree Delete | 5 | ? |
| B-Tree Multi-Index | 2 | ? |
| B-Tree Thread Safety | 2 | ? |
| B-Tree Edge Cases | 7 | ? |
| B-Tree Persistence | 2 | ? |
| File I/O | 12 | ? |
| **Total** | **45** | **100%** |

### Suggested Additional Tests

- [ ] **Heap file allocation/deallocation cycles**
- [ ] **Mixed insert/delete workloads**
- [ ] **Very large keys (near keySize limit)**
- [ ] **Unicode string keys**
- [ ] **Concurrent read/write mix**
- [ ] **Recovery after simulated crashes**
- [ ] **File corruption detection**
- [ ] **Maximum file size handling**
- [ ] **Index with millions of keys**
- [ ] **All key types (INTEGER, BLOCK, NUM_BLOCK, CHARACTER, LOGICAL)**

---

## ??? Architectural Improvements

### Code Quality

- [ ] **Static analysis integration**
  - Add clang-tidy or PVS-Studio to CI
  
- [ ] **Code coverage reporting**
  - Integrate gcov/llvm-cov for coverage metrics

- [ ] **Continuous integration**
  - GitHub Actions workflow for build/test

### Design Patterns

- [ ] **Strategy pattern for comparison**
  - Replace switch statement with polymorphic comparators

- [ ] **Observer pattern for change notifications**
  - Callbacks when data changes (useful for caching layers)

- [ ] **Builder pattern for index configuration**
  - Fluent API for index creation

---

## ?? Known Issues to Monitor

1. **Recursive mutex overhead**: Current design uses recursive mutex which has higher overhead than non-recursive. Consider refactoring internal methods to avoid re-entrant locking.

2. **Exception safety**: Some operations may leave data structures in inconsistent state if exceptions occur mid-operation. Review for strong exception guarantee.

3. **Integer overflow**: Large file positions use `int64_t` but some intermediate calculations might overflow with very large files.

---

## ?? How to Contribute

1. Pick an item from this list (or propose a new one)
2. Open an issue to discuss the approach
3. Fork the repository
4. Implement changes with tests
5. Ensure all 45+ tests pass
6. Submit a pull request

### Contribution Guidelines

- Follow existing code style (see `.clang-format` if available)
- Add tests for new functionality
- Update documentation as needed
- Keep commits focused and well-described
- Reference issue numbers in commit messages

---

## ?? Version History

### v2.1.0 (Current)
- ? Fixed `binarySearchNode()` returning wrong position for keys > all existing keys
- ? Fixed UNIQUE attribute check using wrong bitwise operator
- ? All 45 unit tests passing

### v2.0.0
- Initial modernization from DOS/Windows 3.1 to C++20
- Added thread safety with recursive mutexes
- Added Google Test unit test suite
- Replaced legacy file I/O with std::fstream

### v1.0.0 (Legacy)
- Original DOS/Windows 3.1 implementation
