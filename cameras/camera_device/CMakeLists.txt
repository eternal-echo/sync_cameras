cmake_minimum_required(VERSION 3.10)
project(cameras VERSION 1.0.0 LANGUAGES CXX)

# 基本设置
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置输出目录
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# 查找依赖
find_package(OpenCV REQUIRED)
find_package(Threads REQUIRED)

# 添加 libv4l2cpp 子目录
add_subdirectory(libv4l2cpp)

# 创建 v4l2_camera 库
add_library(v4l2_camera STATIC 
    v4l2_camera_device.cpp
    v4l2_camera_device.hpp
    camera_device.hpp
    buffer.hpp
)

# 设置包含目录
target_include_directories(v4l2_camera
    PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${OpenCV_INCLUDE_DIRS}
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/libv4l2cpp/inc
)

# 链接依赖库
target_link_libraries(v4l2_camera
    PUBLIC
    ${OpenCV_LIBS}
    PRIVATE
    libv4l2cpp
    Threads::Threads
)

# 添加示例子目录
add_subdirectory(examples)
