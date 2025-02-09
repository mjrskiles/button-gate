# Collect test source files
file(GLOB_RECURSE TEST_SOURCES 
    "${CMAKE_CURRENT_SOURCE_DIR}/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/**/*.c"
)

# Add test executable
# If you want to test a new source file, you need to add it to the test executable
add_executable(${PROJECT_NAME}_unit_tests
    ${TEST_SOURCES}
    ${CMAKE_SOURCE_DIR}/src/input/button.c
)

# Add test include directories
target_include_directories(${PROJECT_NAME}_unit_tests PRIVATE 
    ${CMAKE_SOURCE_DIR}/external/unity/src
    ${CMAKE_SOURCE_DIR}/external/unity/extras/fixture/src
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/test/unit
)

target_link_libraries(${PROJECT_NAME}_unit_tests PRIVATE 
    unity::framework
)
