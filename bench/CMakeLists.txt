# Wrapper project: builds llama.cpp from LLAMA_SRC (pristine submodule or patched worktree)
# plus the benchmark tools in this directory, in one build tree.
cmake_minimum_required(VERSION 3.14)
project(grammar_bench C CXX)

set(LLAMA_SRC "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/llama.cpp" CACHE PATH "llama.cpp source tree")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

set(BUILD_SHARED_LIBS     OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_COMMON    ON  CACHE BOOL "" FORCE)
set(LLAMA_BUILD_TESTS     ON  CACHE BOOL "" FORCE)
set(LLAMA_BUILD_TOOLS     ON  CACHE BOOL "" FORCE)
set(LLAMA_BUILD_EXAMPLES  OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_SERVER    OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_APP       OFF CACHE BOOL "" FORCE)
set(LLAMA_OPENSSL         OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_HTML      OFF CACHE BOOL "" FORCE)

add_subdirectory(${LLAMA_SRC} llama.cpp)

add_executable(grammar_replay grammar_replay.cpp)
target_include_directories(grammar_replay PRIVATE ${LLAMA_SRC}/src)
target_link_libraries(grammar_replay PRIVATE llama)

add_executable(grammar_record grammar_record.cpp)
target_include_directories(grammar_record PRIVATE ${LLAMA_SRC}/src)
target_link_libraries(grammar_record PRIVATE llama)

add_executable(schema_to_gbnf schema_to_gbnf.cpp)
target_link_libraries(schema_to_gbnf PRIVATE llama-common)

if (LLAMA_LLGUIDANCE)
    add_executable(llg_replay llg_replay.cpp)
    target_link_libraries(llg_replay PRIVATE llama-common)
endif()

add_executable(grammar_fuzz grammar_fuzz.cpp)
target_include_directories(grammar_fuzz PRIVATE ${LLAMA_SRC}/src)
target_link_libraries(grammar_fuzz PRIVATE llama)
