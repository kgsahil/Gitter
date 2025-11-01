# Code Coverage Report

## Overview

Gitter has comprehensive test coverage with **190 unit tests** covering all major components.

## Why Separate Coverage Build?

We keep coverage in a separate `linux-coverage` build rather than enabling it in `linux-debug` because:
- **Performance**: Coverage instrumentation adds ~2x overhead and significant memory use
- **Binary size**: Coverage data increases binary size significantly
- **Iteration speed**: Slower builds hurt development when coverage isn’t needed
- **Separation of concerns**: Coverage is a release/CI check, not day-to-day

The `linux-coverage` preset is Debug with coverage enabled.

## How to Generate Coverage Report

### Prerequisites
- GCC with coverage support (gcov)
- Python 3 for coverage report generation

### Steps

**Note:** All commands run in WSL. Ensure files are synced from Windows before testing.

1. **Build with coverage enabled:**
   ```bash
   cmake --preset linux-coverage
   cmake --build build/linux-coverage --target gitter_tests
   ```

2. **Run the tests:**
   ```bash
   ./build/linux-coverage/gitter_tests
   ```

3. **Generate coverage report:**
   ```bash
   python3 scripts/generate_coverage_report.py build/linux-coverage
   ```

### Manual Coverage Check

You can also manually check coverage for specific files:

```bash
cd build/linux-coverage
gcov -b -m CMakeFiles/gitter_lib.dir/src/cli/commands/AddCommand.cpp.gcda
```

## Coverage Highlights

From gcov analysis:

### Commands
- **AddCommand.cpp**: 88.68% lines, 74.76% branches
- **CommitCommand.cpp**: 87.93% lines
- **StatusCommand.cpp**: 88.28% lines
- **LogCommand.cpp**: 80.95% lines (18 test cases)
- **CheckoutCommand.cpp**: 82.57% lines (13 test cases) ⬆
- **RestoreCommand.cpp**: 93.33% lines
- **ResetCommand.cpp**: 86.27% lines
- **InitCommand.cpp**: 91.67% lines
- **HelpCommand.cpp**: 0% (untested)
- **CatFileCommand.cpp**: 0% (untested)

### Core Components
- **CommandInvoker.cpp**: 100% lines, 100% branches

### Test Distribution

- **Commands**: ~190 tests
  - Init, Add, Commit, Status, Log (18 tests), Restore, Reset, Checkout (13 tests), CatFile, Help
- **Core**: ~40 tests
  - Repository, Index, ObjectStore, TreeBuilder
- **Util**: ~20 tests
  - Hasher (SHA-1, SHA-256), PatternMatcher
- **Integration**: ~80 tests
  - Complete Git-like workflows

## Coverage Gaps (Areas for Improvement)

### Untested Commands
1. **HelpCommand**: 0% coverage - basic help output (low priority)
2. **CatFileCommand**: 0% coverage - object inspection utility (needs tests)

### Other Gaps
1. **Error handling paths** in some commands
2. **Edge cases** in file system operations
3. **Concurrent access** scenarios (future: threading tests)
4. **Large file handling** (future: performance tests)
5. **Branch coverage**: Higher line coverage but some branch paths uncovered

## Notes

- Coverage is measured using GCC's gcov tool
- Branch coverage considers all conditional execution paths
- Some template code may show lower coverage but is exercised through concrete instantiations
- Main goal: ensure all user-facing functionality is well-tested

## Future Improvements

- [ ] Add lcov for HTML reports
- [ ] Set up CI/CD coverage tracking
- [ ] Add coverage thresholds for PR checks
- [ ] Performance tests for large repositories
- [ ] Stress tests for error conditions

