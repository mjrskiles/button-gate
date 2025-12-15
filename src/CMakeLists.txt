add_subdirectory(hardware)
add_subdirectory(utility)
add_subdirectory(input)
add_subdirectory(output)
add_subdirectory(fsm)
add_subdirectory(core)
add_subdirectory(events)
add_subdirectory(modes)

# Collect source files
target_sources(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/main.c
)