cmake_minimum_required(VERSION 3.16)

# Find required packages
find_program(AVR_CC avr-gcc REQUIRED)
find_program(AVR_OBJCOPY avr-objcopy REQUIRED)
find_program(AVR_SIZE_TOOL avr-size REQUIRED)
find_program(AVR_OBJDUMP avr-objdump REQUIRED)
find_program(AVR_STRIP avr-strip REQUIRED)
find_program(AVRDUDE avrdude REQUIRED)

option(BUILD_TESTS "Build test suite" OFF)

# Set compilers BEFORE project() command
if(DEFINED BUILD_TESTS AND BUILD_TESTS)
    set(CMAKE_C_COMPILER "/usr/bin/gcc")
    set(CMAKE_ASM_COMPILER "/usr/bin/gcc")
    add_compile_definitions(TEST_BUILD)
else()
    # AVR Compiler flags
    set(CMAKE_C_COMPILER ${AVR_CC})
    set(CMAKE_ASM_COMPILER ${AVR_CC})
endif()

# Enable compile_commands.json generation
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Project name
project(button-gate C)

# Include directories
include_directories(include)

if(DEFINED BUILD_TESTS AND BUILD_TESTS)
    # Test build configuration
    message(STATUS "Building tests")

    # Enable new CMake policies
    # set CMP0077 to NEW to prevent the UNITY_EXTENSION_FIXTURE from being set to OFF
    # by an option command in the Unity CMakeLists.txt
    cmake_policy(SET CMP0077 NEW)
    set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)

    add_compile_options(-Wall -Os)

    # Add Unity test framework
    set(UNITY_EXTENSION_FIXTURE ON)
    add_subdirectory(external/Unity)
    add_subdirectory(test/unit)
else()
    message(STATUS "Building application")

    # AVR settings
    set(MCU attiny85)
    set(F_CPU 1000000UL)  
    set(PROGRAMMER stk500v2)
    set(PROGRAMMER_PORT /dev/ttyACM0)  # Adjust port as needed

    set(CMAKE_C_FLAGS "-mmcu=${MCU} -DF_CPU=${F_CPU} -Os -Wall -Wextra -Werror")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ffunction-sections -fdata-sections -fshort-enums")
    set(CMAKE_EXE_LINKER_FLAGS "-mmcu=${MCU} -Wl,--gc-sections -s")

    # Collect source files recursively
    file(GLOB_RECURSE SOURCES 
        "src/*.c"
        "src/**/*.c"
    )

    # Source files
    add_executable(${PROJECT_NAME} ${SOURCES})

    # Generate hex file
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${AVR_STRIP} --strip-debug ${PROJECT_NAME}
        COMMAND ${AVR_OBJCOPY} -O ihex -R .eeprom ${PROJECT_NAME} ${PROJECT_NAME}.hex
        COMMAND ${AVR_SIZE_TOOL} ${PROJECT_NAME}
    )

    # Flash target
    add_custom_target(flash
        COMMAND ${AVRDUDE} -p ${MCU} -c ${PROGRAMMER} -P ${PROGRAMMER_PORT} -b 115200 
                -U flash:w:${PROJECT_NAME}.hex:i
        DEPENDS ${PROJECT_NAME}
    )

    # Fuse targets (using safe default values)
    add_custom_target(fuses
        COMMAND ${AVRDUDE} -p ${MCU} -c ${PROGRAMMER} -P ${PROGRAMMER_PORT} -b 115200 
                -U lfuse:w:0x62:m -U hfuse:w:0xDF:m
    )

    # Read fuses target
    add_custom_target(read_fuses
        COMMAND ${AVRDUDE} -p ${MCU} -c ${PROGRAMMER} -P ${PROGRAMMER_PORT} -b 115200 
                -U lfuse:r:-:h -U hfuse:r:-:h
    )
endif()
