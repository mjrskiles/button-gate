add_subdirectory(hardware)
add_subdirectory(utility)
add_subdirectory(input)
add_subdirectory(output)
add_subdirectory(state)
add_subdirectory(controller)

# Collect source files
target_sources(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/main.c
    ${CMAKE_CURRENT_SOURCE_DIR}/startup.c
)