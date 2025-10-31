cmake_minimum_required(VERSION 3.20)

project(gitter LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(GITTER_STRICT "Treat warnings as errors" OFF)

add_compile_options(-Wall -Wextra -Wconversion -Wpedantic)
if(GITTER_STRICT)
  add_compile_options(-Werror)
endif()

add_library(gitter_lib
  src/util/Logger.cpp
  src/util/PatternMatcher.cpp
  src/util/Hasher.cpp
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
  src/core/ObjectStore.cpp
  src/core/Index.cpp
  src/core/Repository.cpp
)

target_include_directories(gitter_lib PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)

add_executable(gitter src/main.cpp)
target_link_libraries(gitter PRIVATE gitter_lib)

install(TARGETS gitter RUNTIME DESTINATION bin)



