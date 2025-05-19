# virtual_v4l2 虚拟摄像头内核模块

## 功能简介

本模块为一个简单的虚拟V4L2摄像头驱动，支持640x480分辨率、30帧/秒。驱动每33ms输出一帧，帧内容依次为纯红、纯绿、纯蓝三色循环，便于上位机测试多路摄像头同步采集逻辑。

## 编译方法


在本目录下编译内核模块。

注意linux的路径需要指定，若在本机上执行请先跑通网上常见的hello模块的运行，命令不再单独给出直接make就好。

若在tinylab上的qemu运行可以运行这个执行。

```bash
make modules m=virtual_v4l2
```

生成 `virtual_v4l2.ko` 内核模块。

## 加载与卸载

加载模块：
```bash
sudo insmod virtual_v4l2.ko
```

卸载模块：
```bash
sudo rmmod virtual_v4l2
```

## 采集测试

加载后会生成 `/dev/videoX` 设备（X为编号）。可用如下命令采集：

```bash
# 使用v4l2-ctl采集10帧
v4l2-ctl --device=/dev/videoX --stream-count=10 --stream-to=out.raw --stream-mmap=0

# 或用ffmpeg采集
ffmpeg -f v4l2 -framerate 30 -video_size 640x480 -i /dev/videoX out.avi

# 或编译libv4l2cpp库（我的方式，其他的没有验证请注意）
./libv4l2cpptest -f RGB3 -G 640x480x30 -x 5 -r /dev/video0
```

## 适用场景
- 用户态多路摄像头同步采集逻辑验证
- V4L2接口开发与测试

## 注意事项
- 仅支持RGB24格式，分辨率和帧率固定。
- 仅实现了read方式采集，未实现mmap/buffer queue。 