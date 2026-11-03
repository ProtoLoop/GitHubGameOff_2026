cmake_minimum_required(VERSION 3.25)
project(gameoff2026 LANGUAGES C CXX)

set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "Build configurations" FORCE)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/SDL/CMakeLists.txt")
    message(FATAL_ERROR "SDL is missing. Run: git submodule update --init --recursive")
endif()

set(SDL_SHARED OFF CACHE BOOL "Build shared SDL" FORCE)
set(SDL_STATIC ON CACHE BOOL "Build static SDL" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "Build SDL test library" FORCE)
set(SDL_TESTS OFF CACHE BOOL "Build SDL tests" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "Build SDL examples" FORCE)
if(EMSCRIPTEN)
    set(SDL_PTHREADS OFF CACHE BOOL "Use threads in SDL" FORCE)
endif()
add_subdirectory(thirdparty/SDL EXCLUDE_FROM_ALL SYSTEM)

# Enable analysis after third-party targets and before project targets are created.
option(ENABLE_CLANG_TIDY "Analyze project code with clang-tidy" ON)
if(ENABLE_CLANG_TIDY)
    find_program(CLANG_TIDY_EXE NAMES clang-tidy-22 clang-tidy REQUIRED)
    set(CMAKE_C_CLANG_TIDY "${CLANG_TIDY_EXE}")
    set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY_EXE}")
endif()

set(GAMEOFF_SOURCES
    src/Diag.h
    src/Diag.cpp
    src/Input.h
    src/Input.cpp
    src/main.cpp
    src/Result.h
    src/Sprite.h
    src/Sprite.cpp
)

add_executable(gameoff2026 ${GAMEOFF_SOURCES})
target_link_libraries(gameoff2026 PRIVATE SDL3::SDL3-static)
target_include_directories(gameoff2026 SYSTEM PRIVATE thirdparty/stb)
set_target_properties(gameoff2026 PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    COMPILE_WARNING_AS_ERROR YES
)
target_compile_options(gameoff2026 PRIVATE
    $<$<CXX_COMPILER_ID:Clang,AppleClang,GNU>:
        -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow
        -Wformat=2 -Wundef -Wcast-align -Wcast-qual -Wold-style-cast
        -Woverloaded-virtual -Wnon-virtual-dtor -Wnull-dereference
        -fno-exceptions -fno-rtti
    >
)

if(EMSCRIPTEN)
    set_target_properties(gameoff2026 PROPERTIES OUTPUT_NAME index SUFFIX ".html")
    target_link_options(gameoff2026 PRIVATE "-sALLOW_MEMORY_GROWTH=1")
endif()
