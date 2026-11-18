cmake_minimum_required(VERSION 3.0)
project(newfs LANGUAGES C)

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMake")
find_package(FUSE REQUIRED)
add_definitions(-D_FILE_OFFSET_BITS=64)
include_directories(${FUSE_INCLUDE_DIR} include)

# 编译实验包自带驱动，不依赖用户目录里的旧静态库。
add_library(ddriver STATIC ../../driver/user_ddriver/ddriver.c)
add_executable(newfs src/newfs.c)
target_compile_features(newfs PRIVATE c_std_11)
target_compile_options(newfs PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(newfs ${FUSE_LIBRARIES} ddriver)
