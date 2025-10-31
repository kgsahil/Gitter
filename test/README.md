# Gitter Test Suite

Comprehensive unit tests for the Gitter Git-like CLI tool using GoogleTest.

## Building Tests

Tests are built by default when `GITTER_BUILD_TESTS` is enabled (default: ON).

```bash
# Configure build
cmake --preset linux-debug

# Build (including tests)
cmake --build build/linux-debug

# Build only tests
cmake --build build/linux-debug --target gitter_tests
```

## Running Tests

### Using CTest (Recommended)

```bash
# Run all tests
cd build/linux-debug
ctest --output-on-failure

# Run specific test
ctest -R InitCommandTest

# Run with verbose output
ctest --output-on-failure --verbose
```

### Running Test Executable Directly

```bash
./build/linux-debug/gitter_tests

# Run specific test
./build/linux-debug/gitter_tests --gtest_filter=InitCommandTest.*

# List all tests
./build/linux-debug/gitter_tests --gtest_list_tests
```

## Test Structure

```
test/
├── test_main.cpp              # Test entry point
├── test_utils.hpp/cpp          # Test utility functions
├── commands/                   # Command tests
│   ├── test_init.cpp          # InitCommand tests
│   ├── test_add.cpp           # AddCommand tests
│   ├── test_commit.cpp         # CommitCommand tests
│   ├── test_status.cpp        # StatusCommand tests
│   ├── test_log.cpp           # LogCommand tests
│   └── test_restore.cpp        # RestoreCommand tests
├── core/                       # Core component tests
│   ├── test_repository.cpp    # Repository tests
│   ├── test_index.cpp         # Index tests
│   ├── test_objectstore.cpp   # ObjectStore tests
│   └── test_treebuilder.cpp   # TreeBuilder tests
├── util/                       # Utility tests
│   ├── test_hasher.cpp         # Hasher tests
│   └── test_patternmatcher.cpp # PatternMatcher tests
└── integration/                # Integration tests
    └── test_git_workflow.cpp   # Complete workflow tests
```

## Test Coverage

### Unit Tests

**Command Tests**: Each command has comprehensive tests covering:
- **Basic functionality**: Normal usage scenarios
- **Edge cases**: Empty inputs, non-existent files, etc.
- **Pattern matching**: Glob patterns, wildcards
- **Error handling**: Invalid arguments, missing files
- **Combinations**: Multiple files, mixed patterns

**Integration Tests**: Complete Git-like workflows combining multiple commands:
- `BasicWorkflowInitAddCommitStatusLog`: Full workflow validation
- `ModifyFileAfterCommit`: File modification and re-commit
- `StageUnstageRestage`: Add/unstage/re-stage workflow
- `DeleteTrackedFile`: Handling deleted files
- `MultipleCommitsChain`: Commit history and ordering
- `CommitWithoutStaging`: Empty commit handling
- `PatternMatchingAddRestore`: Glob patterns across commands
- `AddDirectoryRecursion`: Recursive directory operations

### Core Component Tests

- **Repository**: Discovery, initialization
- **Index**: Add, update, remove, save/load
- **ObjectStore**: Blob, tree, commit storage
- **TreeBuilder**: Tree construction from index

### Utility Tests

- **Hasher**: SHA-1 and SHA-256 algorithms
- **PatternMatcher**: Glob-to-regex conversion, file matching

## Test Utilities

The `test_utils.hpp` provides helper functions:

- `createTempDir()`: Create temporary test directory
- `removeDir()`: Clean up test directory
- `createFile()`: Create test files
- `initTestRepo()`: Initialize test repository
- `getCwd()`, `setCwd()`: Working directory management

## Writing New Tests

1. Create test file in appropriate directory (`test/commands/`, `test/core/`, or `test/util/`)
2. Include `<gtest/gtest.h>` and relevant headers
3. Use `TEST_F(TestClassName, TestName)` for fixture-based tests
4. Use `TEST(TestName, SubTestName)` for standalone tests
5. Add test file to `CMakeLists.txt` under `gitter_tests` executable

Example:

```cpp
#include <gtest/gtest.h>
#include "cli/commands/YourCommand.hpp"

class YourCommandTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup test environment
    }
    
    void TearDown() override {
        // Cleanup
    }
};

TEST_F(YourCommandTest, BasicFunctionality) {
    YourCommand cmd;
    // Test implementation
    EXPECT_TRUE(condition);
}
```

## Test Best Practices

1. **Isolation**: Each test should be independent
2. **Cleanup**: Always clean up temporary files/directories
3. **Assertions**: Use `ASSERT_*` for fatal errors, `EXPECT_*` for non-fatal
4. **Descriptive names**: Test names should describe what they test
5. **Edge cases**: Test empty inputs, large inputs, special characters
6. **Error paths**: Test error conditions and invalid inputs

## Continuous Integration

Tests should pass on:
- Linux (WSL/Ubuntu)
- CMake 3.20+
- C++20 compatible compiler (GCC 11+, Clang 12+)

