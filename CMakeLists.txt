cmake_minimum_required(VERSION 3.16)
project(cooperative_scheduler VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_library(scheduler STATIC
    src/scheduler.c
)

target_include_directories(scheduler
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_options(scheduler
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
)

add_executable(main_example
    examples/main_example.c
)

target_link_libraries(main_example PRIVATE scheduler)
