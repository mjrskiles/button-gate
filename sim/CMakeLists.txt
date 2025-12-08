# Gatekeeper x86 Simulator
# Build with: cmake -DBUILD_SIM=ON ..

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

# Simulator sources
set(SIM_SOURCES
    sim_main.c
    sim_hal.c
    input_source.c
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
