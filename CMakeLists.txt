cmake_minimum_required(VERSION 3.29)
project(V4L2CaptureExample)

set(CMAKE_CXX_STANDARD 20)

# 设置OpenCV路径（如果你没有使用系统安装的OpenCV，可以指定安装路径）
find_package(OpenCV REQUIRED)

# 设置spdlog路径
find_package(spdlog REQUIRED)

include_directories( ${OpenCV_INCLUDE_DIRS} )

# 添加 libv4l2cpp 子模块的路径
add_subdirectory(external/libv4l2cpp)

add_executable(V4L2CaptureExample main.cpp)

target_link_libraries(V4L2CaptureExample PUBLIC libv4l2cpp ${OpenCV_LIBS} spdlog::spdlog)
