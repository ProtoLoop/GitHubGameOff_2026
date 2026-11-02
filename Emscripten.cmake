# Web toolchain entry point. Provision pinned local tools, then configure CMake.
include("${CMAKE_CURRENT_LIST_DIR}/ProvisionEmscripten.cmake")
gameoff_prepare_toolchain()
find_program(GAMEOFF_PYTHON NAMES python3 python REQUIRED NO_CMAKE_FIND_ROOT_PATH)

# Apply the same explicit tool paths during configure and later Ninja invocations.
# This also isolates the build from an activated system/Nix Emscripten environment.
set(gameoff_em_environment
    "EM_CONFIG=${GAMEOFF_EM_ROOT}/.emscripten"
    "EM_LLVM_ROOT=${GAMEOFF_EM_ROOT}/../bin"
    "EM_BINARYEN_ROOT=${GAMEOFF_EM_ROOT}/.."
    "EM_NODE_JS=${GAMEOFF_NODE}"
    "EM_CACHE=${GAMEOFF_EM_ROOT}/cache"
    "EMSDK_PYTHON=${GAMEOFF_PYTHON}"
)
foreach(entry IN LISTS gameoff_em_environment)
    string(FIND "${entry}" "=" separator)
    string(SUBSTRING "${entry}" 0 ${separator} key)
    math(EXPR value_start "${separator} + 1")
    string(SUBSTRING "${entry}" ${value_start} -1 value)
    set(ENV{${key}} "${value}")
endforeach()
set(EMSCRIPTEN_ROOT_PATH "${GAMEOFF_EM_ROOT}")
set(CMAKE_CROSSCOMPILING_EMULATOR "${GAMEOFF_NODE}")
include("${GAMEOFF_EM_ROOT}/cmake/Modules/Platform/Emscripten.cmake")
foreach(language C CXX)
    set(CMAKE_${language}_COMPILER_LAUNCHER "${CMAKE_COMMAND};-E;env;${gameoff_em_environment}")
    set(CMAKE_${language}_LINKER_LAUNCHER "${CMAKE_COMMAND};-E;env;${gameoff_em_environment}")
endforeach()
# Platform initialization loads upstream's file again after this toolchain.
# Apply archiver selection afterward. These wrappers only call the LLVM binaries,
# so invoking those directly also avoids inherited system Python/config settings.
set(gameoff_rules [=[
set(CMAKE_AR "@GAMEOFF_EM_ROOT@/../bin/llvm-ar@GAMEOFF_EXE_SUFFIX@")
set(CMAKE_RANLIB "@GAMEOFF_EM_ROOT@/../bin/llvm-ranlib@GAMEOFF_EXE_SUFFIX@")
set(CMAKE_NM "@GAMEOFF_EM_ROOT@/../bin/llvm-nm@GAMEOFF_EXE_SUFFIX@")
foreach(language C CXX)
    set(CMAKE_${language}_COMPILER_AR "${CMAKE_AR}")
    set(CMAKE_${language}_COMPILER_RANLIB "${CMAKE_RANLIB}")
endforeach()
]=])
set(CMAKE_USER_MAKE_RULES_OVERRIDE "${GAMEOFF_EM_ROOT}/../gameoff-rules.cmake")
file(CONFIGURE OUTPUT "${CMAKE_USER_MAKE_RULES_OVERRIDE}" CONTENT "${gameoff_rules}" @ONLY)
