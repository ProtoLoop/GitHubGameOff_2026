cmake_minimum_required(VERSION 3.27)
project(mosaic LANGUAGES NONE)

include(cmake/ProvisionDeno.cmake)
mosaic_provision_deno(DENO)

set(MOSAIC_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/dist")
set(MOSAIC_DENO_RUNTIME_CACHE "${CMAKE_CURRENT_BINARY_DIR}/deno-cache")

file(MAKE_DIRECTORY "${MOSAIC_OUTPUT}")
configure_file(index.html "${MOSAIC_OUTPUT}/index.html" COPYONLY)

set(MOSAIC_SOURCES
    src/main.ts src/placement.ts src/document.ts src/history.ts src/project-file.ts src/project-session.ts src/project-http.ts
)

add_custom_command(
    OUTPUT "${MOSAIC_OUTPUT}/app.js"
    COMMAND "${CMAKE_COMMAND}" -E env "DENO_DIR=${MOSAIC_DENO_RUNTIME_CACHE}"
        "${DENO}" check --config "${CMAKE_CURRENT_SOURCE_DIR}/deno.json"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/main.ts"
    COMMAND "${CMAKE_COMMAND}" -E env "DENO_DIR=${MOSAIC_DENO_RUNTIME_CACHE}"
        "${DENO}" lint --config "${CMAKE_CURRENT_SOURCE_DIR}/deno.json"
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
    COMMAND "${CMAKE_COMMAND}" -E env "DENO_DIR=${MOSAIC_DENO_RUNTIME_CACHE}"
        "${DENO}" bundle --platform=browser
        --config "${CMAKE_CURRENT_SOURCE_DIR}/deno.json"
        --output "${MOSAIC_OUTPUT}/app.js"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/main.ts"
    DEPENDS index.html ${MOSAIC_SOURCES} deno.json
    VERBATIM
)

add_custom_target(mosaic ALL
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/../ThirdPartyLicenses.txt" "${MOSAIC_OUTPUT}/ThirdPartyLicenses.txt"
    DEPENDS "${MOSAIC_OUTPUT}/app.js"
    VERBATIM
)

find_package(Python3 REQUIRED COMPONENTS Interpreter)

add_custom_target(check
    COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/server_test.py"
    COMMAND "${CMAKE_COMMAND}" -E env "DENO_DIR=${MOSAIC_DENO_RUNTIME_CACHE}"
        "${DENO}" test --config "${CMAKE_CURRENT_SOURCE_DIR}/deno.json"
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
    VERBATIM
)
