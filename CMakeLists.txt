cmake_minimum_required(VERSION 2.8)

project(taskpool)

add_compile_options(-g -Wall -Werror)
add_definitions(-DMODULE_NAME="${PROJECT_NAME}")
include_directories("${PROJECT_SOURCE_DIR}/inc")
include_directories("${PROJECT_SOURCE_DIR}/inc/inner")

add_library(${PROJECT_NAME}
    ${PROJECT_SOURCE_DIR}/src/log.c
    ${PROJECT_SOURCE_DIR}/src/mem.c
    ${PROJECT_SOURCE_DIR}/src/que.c
    ${PROJECT_SOURCE_DIR}/src/task.c
    ${PROJECT_SOURCE_DIR}/src/taskpool.c
)

add_executable(example
    ${PROJECT_SOURCE_DIR}/test/example.c
)

target_link_libraries(example ${PROJECT_NAME} pthread)