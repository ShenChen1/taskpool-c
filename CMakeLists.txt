cmake_minimum_required(VERSION 2.8)

project(taskpool)

include_directories("inc")
add_library(${PROJECT_NAME} src/taskpool.c inc/taskpool.h)

add_executable(example test/example.c)
target_link_libraries(example ${PROJECT_NAME})