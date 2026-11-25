cmake_minimum_required(VERSION 3.27)
project(gameoff2026 LANGUAGES C CXX)

set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "Build configurations" FORCE)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/SDL/CMakeLists.txt")
    message(FATAL_ERROR "SDL is missing. Run: git submodule update --init --recursive")
endif()

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/SDL_mixer/CMakeLists.txt")
    message(FATAL_ERROR "SDL_mixer is missing. Run: git submodule update --init --recursive")
endif()

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/stb/stb_image.h")
    message(FATAL_ERROR "stb is missing. Run: git submodule update --init --recursive")
endif()

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/jsmn/jsmn.h")
    message(FATAL_ERROR "jsmn is missing. Run: git submodule update --init --recursive")
endif()

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/glm/CMakeLists.txt")
    message(FATAL_ERROR "GLM is missing. Run: git submodule update --init --recursive")
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

# Use the same bundled audio decoders on native and browser builds.
set(SDLMIXER_DEPS_SHARED OFF CACHE BOOL "Load codec dependencies dynamically" FORCE)
set(SDLMIXER_VENDORED ON CACHE BOOL "Use vendored codec dependencies" FORCE)
set(SDLMIXER_STRICT ON CACHE BOOL "Require enabled codec dependencies" FORCE)
set(SDLMIXER_WAVE ON CACHE BOOL "Enable WAVE audio" FORCE)
set(SDLMIXER_MP3 ON CACHE BOOL "Enable MP3 audio" FORCE)
set(SDLMIXER_MP3_DRMP3 ON CACHE BOOL "Decode MP3 with bundled dr_mp3" FORCE)
set(SDLMIXER_VORBIS_STB ON CACHE BOOL "Decode Vorbis with bundled stb_vorbis" FORCE)
set(SDLMIXER_MP3_MPG123 OFF CACHE BOOL "Disable mpg123" FORCE)
set(SDLMIXER_VORBIS_VORBISFILE OFF CACHE BOOL "Disable libvorbisfile" FORCE)
set(SDLMIXER_VORBIS_TREMOR OFF CACHE BOOL "Disable Tremor" FORCE)
set(SDLMIXER_FLAC OFF CACHE BOOL "Disable FLAC audio" FORCE)
set(SDLMIXER_OPUS OFF CACHE BOOL "Disable Opus audio" FORCE)
set(SDLMIXER_WAVPACK OFF CACHE BOOL "Disable WavPack audio" FORCE)
set(SDLMIXER_MOD OFF CACHE BOOL "Disable tracker audio" FORCE)
set(SDLMIXER_MIDI OFF CACHE BOOL "Disable MIDI audio" FORCE)
set(SDLMIXER_GME OFF CACHE BOOL "Disable game-music-emu" FORCE)
set(SDLMIXER_AIFF OFF CACHE BOOL "Disable AIFF audio" FORCE)
set(SDLMIXER_AU OFF CACHE BOOL "Disable AU audio" FORCE)
set(SDLMIXER_VOC OFF CACHE BOOL "Disable VOC audio" FORCE)
set(SDLMIXER_TESTS OFF CACHE BOOL "Disable SDL_mixer tests" FORCE)
set(SDLMIXER_EXAMPLES OFF CACHE BOOL "Disable SDL_mixer examples" FORCE)
set(SDLMIXER_INSTALL OFF CACHE BOOL "Disable SDL_mixer installation" FORCE)
set(SDLMIXER_WERROR OFF CACHE BOOL "Disable SDL_mixer warnings as errors" FORCE)
add_subdirectory(thirdparty/SDL_mixer EXCLUDE_FROM_ALL SYSTEM)

set(DAWN_BUILD_MONOLITHIC_LIBRARY STATIC CACHE STRING "Build static Dawn" FORCE)
set(DAWN_BUILD_SAMPLES OFF CACHE BOOL "Disable Dawn samples" FORCE)
set(DAWN_BUILD_TESTS OFF CACHE BOOL "Disable Dawn tests" FORCE)
set(TINT_BUILD_TESTS OFF CACHE BOOL "Disable Tint tests" FORCE)
set(DAWN_USE_GLFW OFF CACHE BOOL "Disable GLFW" FORCE)

# Cache the fetched Dawn revision per build directory to avoid slow dependency
# checks on every configure. Fetch again for a fresh build or a changed revision.
function(add_dawn_with_dependencies_fetched_once_per_revision)
    execute_process(
        COMMAND git rev-parse HEAD
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/dawn"
        OUTPUT_VARIABLE dawn_revision
        OUTPUT_STRIP_TRAILING_WHITESPACE
        COMMAND_ERROR_IS_FATAL ANY
    )

    if("${DAWN_FETCHED_REVISION}" STREQUAL "${dawn_revision}")
        set(DAWN_FETCH_DEPENDENCIES OFF CACHE BOOL "Fetch Dawn dependencies" FORCE)
    else()
        set(DAWN_FETCH_DEPENDENCIES ON CACHE BOOL "Fetch Dawn dependencies" FORCE)
    endif()

    add_subdirectory(thirdparty/dawn EXCLUDE_FROM_ALL SYSTEM)

    set(DAWN_FETCHED_REVISION "${dawn_revision}"
        CACHE INTERNAL "Dawn revision whose dependencies were fetched" FORCE)
endfunction()

add_dawn_with_dependencies_fetched_once_per_revision()
target_compile_definitions(dawn_utils_config INTERFACE
    "$<$<CONFIG:Debug>:DAWN_ENABLE_ASSERTS>"
)

set(GLM_BUILD_LIBRARY OFF CACHE BOOL "Build compiled GLM library" FORCE)
set(GLM_BUILD_TESTS OFF CACHE BOOL "Build GLM tests" FORCE)
set(GLM_BUILD_INSTALL OFF CACHE BOOL "Generate GLM install rules" FORCE)
add_subdirectory(thirdparty/glm EXCLUDE_FROM_ALL SYSTEM)

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
    src/Actors/Caretaker.h
    src/Actors/Caretaker.cpp
    src/Actors/LostSoul.h
    src/Actors/LostSoul.cpp
    src/Angle.h
    src/AudioClip.h
    src/AudioClip.cpp
    src/AudioClipLoader.h
    src/AudioClipLoader.cpp
    src/AudioPlayer.h
    src/AudioPlayer.cpp
    src/BoundedVector.h
    src/CellMap.h
    src/CellMap.cpp
    src/Defer.h
    src/Diag.h
    src/Diag.cpp
    src/EntityId.h
    src/FileFetcher.h
    src/FileFetcher.cpp
    src/Game.h
    src/Game.cpp
    src/Geometry2D.h
    src/InlineString.h
    src/InlineVector.h
    src/Input.h
    src/Input.cpp
    src/jsmn.cpp
    src/JsonDocument.h
    src/JsonDocument.cpp
    src/Level.h
    src/Level.cpp
    src/LevelFetcher.h
    src/LevelFetcher.cpp
    src/LevelLoader.h
    src/LevelLoader.cpp
    src/main.cpp
    src/NameTypes.h
    src/Prop.h
    src/Prop.cpp
    src/Renderer.h
    src/Renderer.cpp
    src/Result.h
    src/Random.h
    src/Random.cpp
    src/ResourceDefs.h
    src/Shortcuts.h
    src/Sprite.h
    src/Sprite.cpp
    src/SpriteSequenceLoader.h
    src/SpriteSequenceLoader.cpp
    src/TextureLoader.h
    src/TextureLoader.cpp
    src/Timer.h
    src/Timer.cpp
    src/Transform2D.h
    src/stb_image.cpp
    src/WebGpu.h
    src/WebGpu.cpp
)

add_executable(gameoff2026 ${GAMEOFF_SOURCES})
target_include_directories(gameoff2026 PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)
target_link_libraries(gameoff2026 PRIVATE SDL3::SDL3-static)
target_link_libraries(gameoff2026 PRIVATE SDL3_mixer::SDL3_mixer-static)
target_link_libraries(gameoff2026 PRIVATE glm::glm-header-only)
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
        "-lembind"
        "-fno-rtti"
        "-sALLOW_MEMORY_GROWTH=1"
        "-sFETCH=1"
        "-sSTACK_OVERFLOW_CHECK=2"
    )
    # Required because we disable RTTI.
    target_compile_definitions(gameoff2026 PRIVATE
        EMSCRIPTEN_HAS_UNBOUND_TYPE_NAMES=0
    )
  endif()
