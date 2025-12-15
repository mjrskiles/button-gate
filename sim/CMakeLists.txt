# Gatekeeper x86 Simulator
# Build with: cmake -DBUILD_SIM=ON ..
#
# Output modes (no external dependencies):
#   ./gatekeeper-sim              - Interactive terminal UI
#   ./gatekeeper-sim --json       - NDJSON output (one JSON object per line)
#   ./gatekeeper-sim --batch      - Plain text events (for CI/scripts)
#   ./gatekeeper-sim --script X   - Run test script
#
# JSON schema: sim/schema/sim_state_v1.json

project(gatekeeper-sim C)

message(STATUS "Building x86 simulator")

# Collect application sources (exclude AVR main.c)
file(GLOB_RECURSE APP_SOURCES
    "${CMAKE_SOURCE_DIR}/src/*.c"
)
# Remove main.c - we use sim_main.c instead
list(FILTER APP_SOURCES EXCLUDE REGEX "main\\.c$")
# Remove real HAL - we use sim_hal.c instead
list(FILTER APP_SOURCES EXCLUDE REGEX "hardware/hal\\.c$")
# Remove AVR neopixel - we use sim_neopixel.c instead
list(FILTER APP_SOURCES EXCLUDE REGEX "output/neopixel\\.c$")

# Simulator sources
set(SIM_SOURCES
    sim_main.c
    sim_hal.c
    sim_neopixel.c
    sim_state.c
    input_source.c
    cv_source.c
    socket_server.c
    command_handler.c
    render/render_terminal.c
    render/render_json.c
    render/render_batch.c
)

add_executable(gatekeeper-sim
    ${SIM_SOURCES}
    ${APP_SOURCES}
)

target_include_directories(gatekeeper-sim PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Define SIM_BUILD and TEST_BUILD (TEST_BUILD reuses test-compatible code paths)
target_compile_definitions(gatekeeper-sim PRIVATE SIM_BUILD TEST_BUILD)

# Link math library (for sinf in cv_source.c)
target_link_libraries(gatekeeper-sim PRIVATE m)

# Compiler options
target_compile_options(gatekeeper-sim PRIVATE
    -Wall
    -Wextra
    -g
    -O0
)

# Optional: Address sanitizer for catching memory bugs
option(SIM_SANITIZERS "Enable address/undefined sanitizers" OFF)
if(SIM_SANITIZERS)
    target_compile_options(gatekeeper-sim PRIVATE -fsanitize=address,undefined)
    target_link_options(gatekeeper-sim PRIVATE -fsanitize=address,undefined)
endif()
