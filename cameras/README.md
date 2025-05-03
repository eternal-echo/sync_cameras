# V4L2摄像头库

## 编译

```bash
cd cameras

# 创建构建目录
mkdir -p build && cd build

# 配置并编译
cmake ..
make

# 运行示例程序
./bin/v4l2_camera_example

# 指定设备、分辨率和格式
./bin/v4l2_camera_example /dev/video0 1280 720 MJPEG
```

## 在其他项目中使用

在你的CMakeLists.txt中添加：

```cmake
add_subdirectory(/path/to/cameras)
target_link_libraries(your_target v4l2_camera)
```