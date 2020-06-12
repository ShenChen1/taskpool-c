cmake_minimum_required(VERSION 2.8)

project(taskpool)

include_directories("${PROJECT_SOURCE_DIR}/inc")

add_library(${PROJECT_NAME}
    ${PROJECT_SOURCE_DIR}/src/taskpool.c
)

add_executable(example
    ${PROJECT_SOURCE_DIR}/test/example.c
)

target_link_libraries(example ${PROJECT_NAME})