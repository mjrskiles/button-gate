add_subdirectory(hardware)
add_subdirectory(input)

# Collect source files
target_sources(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/main.c
    ${CMAKE_CURRENT_SOURCE_DIR}/startup.c
)