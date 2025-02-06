cmake_minimum_required(VERSION 3.16)

# Project name
project(button-gate C)

# AVR settings
set(MCU attiny85)
set(F_CPU 1000000UL)  
set(PROGRAMMER stk500v2)
set(PROGRAMMER_PORT /dev/ttyACM0)  # Adjust port as needed

# Find required packages
find_program(AVR_CC avr-gcc REQUIRED)
find_program(AVR_CXX avr-g++ REQUIRED)
find_program(AVR_OBJCOPY avr-objcopy REQUIRED)
find_program(AVR_SIZE_TOOL avr-size REQUIRED)
find_program(AVR_OBJDUMP avr-objdump REQUIRED)
find_program(AVRDUDE avrdude REQUIRED)

# AVR Compiler flags
set(CMAKE_C_COMPILER ${AVR_CC})
set(CMAKE_ASM_COMPILER ${AVR_CC})

set(CMAKE_C_FLAGS "-mmcu=${MCU} -DF_CPU=${F_CPU} -Os -Wall -Wextra -Werror")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ffunction-sections -fdata-sections -fshort-enums")
set(CMAKE_EXE_LINKER_FLAGS "-mmcu=${MCU} -Wl,--gc-sections")

# Include directories
include_directories(include)

# Source files
add_executable(${PROJECT_NAME} 
    src/main.c
    src/hal.c
    src/startup.c
    src/input/button.c
)

# Generate hex file
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
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