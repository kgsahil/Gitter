cmake_minimum_required(VERSION 3.20)

project(gitter LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(GITTER_STRICT "Treat warnings as errors" OFF)
option(GITTER_BUILD_TESTS "Build unit tests" ON)
option(GITTER_ENABLE_COVERAGE "Enable code coverage" OFF)

# GoogleTest via FetchContent (for tests)
if(GITTER_BUILD_TESTS)
  include(FetchContent)
  FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
  )
  FetchContent_MakeAvailable(googletest)
endif()

# Code coverage setup
if(GITTER_ENABLE_COVERAGE)
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    message(STATUS "Enabling code coverage")
    add_compile_options(--coverage -fprofile-arcs -ftest-coverage)
    add_link_options(--coverage)
  else()
    message(WARNING "Code coverage not supported for ${CMAKE_CXX_COMPILER_ID}")
  endif()
endif()

# Platform-specific compile options
if(MSVC)
  # MSVC compiler options
  add_compile_options(/W4 /WX-)
  if(GITTER_STRICT)
    add_compile_options(/WX)  # Treat warnings as errors
  endif()
  # MSVC-specific warnings
  add_compile_options(/wd4996)  # Disable deprecation warnings
else()
  # GCC/Clang compiler options
  add_compile_options(-Wall -Wextra -Wconversion -Wpedantic)
  if(GITTER_STRICT)
    add_compile_options(-Werror)
  endif()
endif()

add_library(gitter_lib
  src/util/Logger.cpp
  src/util/PatternMatcher.cpp
  src/util/HasherFactory.cpp
  src/util/Sha1Hasher.cpp
  src/util/Sha256Hasher.cpp
  src/util/FileMetadata.cpp
  src/cli/CommandFactory.cpp
  src/cli/CommandInvoker.cpp
  src/cli/commands/HelpCommand.cpp
  src/cli/commands/InitCommand.cpp
  src/cli/commands/AddCommand.cpp
  src/cli/commands/CommitCommand.cpp
  src/cli/commands/StatusCommand.cpp
  src/cli/commands/LogCommand.cpp
  src/cli/commands/CheckoutCommand.cpp
  src/cli/commands/RestoreCommand.cpp
  src/cli/commands/CatFileCommand.cpp
  src/cli/commands/ResetCommand.cpp
  src/core/ObjectStore.cpp
  src/core/Index.cpp
  src/core/TreeBuilder.cpp
  src/core/Repository.cpp
)

target_include_directories(gitter_lib PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Find and link zlib
find_package(ZLIB REQUIRED)
target_link_libraries(gitter_lib PUBLIC ZLIB::ZLIB)

# Windows-specific: Enable console subsystem for executables
if(WIN32)
  set_target_properties(gitter PROPERTIES WIN32_EXECUTABLE FALSE)
  if(GITTER_BUILD_TESTS)
    set_target_properties(gitter_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  endif()
endif()

add_executable(gitter src/main.cpp)
target_link_libraries(gitter PRIVATE gitter_lib)

install(TARGETS gitter RUNTIME DESTINATION bin)

# Unit tests
if(GITTER_BUILD_TESTS)
  enable_testing()
  
  # Test executable
  add_executable(gitter_tests
    test/test_main.cpp
    test/test_utils.cpp
    test/commands/test_init.cpp
    test/commands/test_add.cpp
    test/commands/test_commit.cpp
    test/commands/test_status.cpp
    test/commands/test_log.cpp
    test/commands/test_restore.cpp
    test/commands/test_reset.cpp
    test/commands/test_checkout.cpp
    test/core/test_repository.cpp
    test/core/test_index.cpp
    test/core/test_objectstore.cpp
    test/core/test_treebuilder.cpp
    test/util/test_hasher.cpp
    test/util/test_patternmatcher.cpp
    test/integration/test_git_workflow.cpp
  )
  
  target_link_libraries(gitter_tests PRIVATE
    gitter_lib
    GTest::gtest
    GTest::gmock
  )
  
  target_include_directories(gitter_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/test
  )
  
  include(GoogleTest)
  gtest_discover_tests(gitter_tests)
endif()



