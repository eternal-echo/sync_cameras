# V4L2 摄像头示例程序配置

# 查找OpenCV
find_package(OpenCV REQUIRED)
include_directories(${OpenCV_INCLUDE_DIRS})

# 创建 V4L2 摄像头示例程序

# 创建示例程序
add_executable(v4l2_camera_example v4l2_camera_example.cpp)

# 设置包含目录
target_include_directories(v4l2_camera_example
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

# 链接库（直接使用目标名称）
target_link_libraries(v4l2_camera_example
    PRIVATE
    v4l2_camera
    ${OpenCV_LIBS}
)

# 添加执行权限（对于Unix系统）
if(UNIX)
    add_custom_command(TARGET v4l2_camera_example POST_BUILD
        COMMAND chmod +x $<TARGET_FILE:v4l2_camera_example>
        COMMENT "Adding execute permission to v4l2_camera_example"
        VERBATIM
    )
endif()

# 安装示例程序
install(TARGETS v4l2_camera_example
    RUNTIME DESTINATION bin/examples
)
