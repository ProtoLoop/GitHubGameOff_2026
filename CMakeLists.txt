cmake_minimum_required(VERSION 3.27)
project(gameoff2026 LANGUAGES C CXX)

set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "Build configurations" FORCE)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/SDL/CMakeLists.txt")
    message(FATAL_ERROR "SDL is missing. Run: git submodule update --init --recursive")
endif()

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/stb/stb_image.h")
    message(FATAL_ERROR "stb is missing. Run: git submodule update --init --recursive")
endif()

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/jsmn/jsmn.h")
    message(FATAL_ERROR "jsmn is missing. Run: git submodule update --init --recursive")
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

set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build static libraries" FORCE)
set(DAWN_BUILD_MONOLITHIC_LIBRARY STATIC CACHE STRING "Build static Dawn" FORCE)
set(DAWN_FETCH_DEPENDENCIES ON CACHE BOOL "Fetch Dawn dependencies" FORCE)
set(DAWN_BUILD_SAMPLES OFF CACHE BOOL "Disable Dawn samples" FORCE)
set(DAWN_BUILD_TESTS OFF CACHE BOOL "Disable Dawn tests" FORCE)
set(TINT_BUILD_TESTS OFF CACHE BOOL "Disable Tint tests" FORCE)
set(DAWN_USE_GLFW OFF CACHE BOOL "Disable GLFW" FORCE)

add_subdirectory(thirdparty/dawn EXCLUDE_FROM_ALL SYSTEM)
target_compile_definitions(dawn_utils_config INTERFACE
    "$<$<CONFIG:Debug>:DAWN_ENABLE_ASSERTS>"
)

# Enable analysis after third-party targets and before project targets are created.
option(ENABLE_CLANG_TIDY "Analyze project code with clang-tidy" ON)
if(ENABLE_CLANG_TIDY)
    find_program(CLANG_TIDY_EXE NAMES clang-tidy REQUIRED)
    set(CMAKE_C_CLANG_TIDY "${CLANG_TIDY_EXE}")
    set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY_EXE}")

    if(EMSCRIPTEN)
        foreach(language C CXX)
            list(APPEND CMAKE_${language}_CLANG_TIDY
                "--extra-arg=--target=wasm32-unknown-emscripten"
                "--extra-arg=--sysroot=${GAMEOFF_EM_ROOT}/cache/sysroot"
                "--extra-arg=-isystem${GAMEOFF_EM_ROOT}/cache/sysroot/include/compat"
            )
        endforeach()
    endif()

endif()

set(GAMEOFF_SOURCES
    src/Angle.h
    src/BoundedVector.h
    src/Defer.h
    src/Diag.h
    src/Diag.cpp
    src/FileFetcher.h
    src/FileFetcher.cpp
    src/InlineString.h
    src/InlineVector.h
    src/Input.h
    src/Input.cpp
    src/jsmn.cpp
    src/JsonDocument.h
    src/JsonDocument.cpp
    src/LevelLoader.h
    src/LevelLoader.cpp
    src/main.cpp
    src/Result.h
    src/Scene.h
    src/Scene.cpp
    src/Shortcuts.h
    src/SpriteLib.h
    src/SpriteLib.cpp
    src/SpriteSheet.h
    src/SpriteSheet.cpp
    src/stb_image.cpp
    src/WebGpu.h
    src/WebGpu.cpp
)

add_executable(gameoff2026 ${GAMEOFF_SOURCES})
target_link_libraries(gameoff2026 PRIVATE SDL3::SDL3-static)
if(EMSCRIPTEN)
    target_link_libraries(gameoff2026 PRIVATE emdawnwebgpu_cpp)
else()
    target_link_libraries(gameoff2026 PRIVATE dawn::webgpu_dawn)
endif()
target_include_directories(gameoff2026 SYSTEM PRIVATE thirdparty/stb thirdparty/jsmn)
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

# Keep third-party implementations out of clang-tidy noise.
set_source_files_properties(src/stb_image.cpp PROPERTIES SKIP_LINTING ON)

if(EMSCRIPTEN)
    set_target_properties(gameoff2026 PROPERTIES OUTPUT_NAME index SUFFIX ".html")
    set(GAMEOFF_WEB_SHELL "${CMAKE_CURRENT_SOURCE_DIR}/web/index.html")
    set_property(TARGET gameoff2026 APPEND PROPERTY LINK_DEPENDS "${GAMEOFF_WEB_SHELL}")
    target_link_options(gameoff2026 PRIVATE
        "SHELL:--shell-file \"${GAMEOFF_WEB_SHELL}\""
        "-sALLOW_MEMORY_GROWTH=1"
        "-sFETCH=1"
        "-sSTACK_OVERFLOW_CHECK=2"
    )
endif()
