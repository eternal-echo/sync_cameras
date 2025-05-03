# V4L2摄像头库

## 环境

安装opencv、v4l2-loopback和ffmpeg。
```bash
sudo apt-get install libopencv-dev v4l2loopback-utils ffmpeg
```

使用v4l2loopback创建虚拟摄像头：
```bash
sudo modprobe v4l2loopback
ls /dev/video*
# /dev/video0
sudo chmod 666 /dev/video0
```

## 编译

```bash
cd cameras

# 创建构建目录
mkdir -p build && cd build

# 配置并编译
cmake ..
make

```

# 测试

- 打开另一个终端，使用ffmpeg创建测试视频流：
    ```bash
    ffmpeg -stream_loop -1 -re -i cameras/data/test.mp4 \
        -vcodec mjpeg -f v4l2 /dev/video0
    ```
    -stream_loop -1：无限循环输入文件；
    -re：以实时速率读取；
    -f v4l2：输出为 v4l2 格式；
    /dev/video0：目标为虚拟摄像头。
- 检查视频流：
    ```bash
    v4l2-ctl --device=/dev/video0 --list-formats
    ```
    输出结果如下：
    ```
    ioctl: VIDIOC_ENUM_FMT
        Type: Video Capture

        [0]: 'MJPG' (Motion-JPEG, compressed)
    ```

回到第一个终端，运行示例程序：
```bash
# 运行示例程序
./bin/v4l2_camera_example
```

## 在其他项目中使用

在你的CMakeLists.txt中添加：

```cmake
add_subdirectory(/path/to/cameras)
target_link_libraries(your_target v4l2_camera)
```