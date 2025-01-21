# 使用 `libv4l2cpp` 连续读取并保存摄像头数据的简易 Demo

在这篇博客中，我们将演示如何利用现成的 `libv4l2cpp` 库，结合 **生产者消费者模型** 和 **多线程** 的方式，连续读取摄像头数据并保存为图像。为了方便后续扩展，我们还使用了 **回调函数** 来解耦数据处理逻辑。这整个项目是一个简易的验证 Demo，旨在帮助大家了解如何利用 V4L2 进行视频捕获，并为更复杂的应用程序打下基础。

## 1. V4L2 的底层原理与相关知识

### 什么是 V4L2？

**V4L2**（Video4Linux2）是 Linux 操作系统下用于视频捕获和输出的标准 API。它是 Linux 中最常用的获取视频流的接口之一，支持从各种设备（如摄像头、视频采集卡）获取视频数据。在 V4L2 中，设备通过视频设备节点（通常是 `/dev/videoX`）进行访问。

V4L2 提供了一个底层的接口，允许用户设置设备参数（如分辨率、帧率等）并读取或写入视频帧。它支持多种数据流格式（如 YUYV、MJPEG、H264 等），并能够在用户空间进行高效的视频流捕获。

### V4L2 的数据捕获模式

V4L2 提供了几种不同的数据捕获模式：
- **内存映射（MMAP）**：将视频缓冲区映射到用户空间，可以高效地访问视频帧。
- **轮询模式（READWRITE）**：通过系统调用直接读取数据，适合实时应用。
- **用户空间缓冲区**：通过将数据缓冲区传递给内核进行管理的方式，适合需要更复杂控制的应用。

在我们的项目中，我们使用了 **MMAP** 模式来实现视频捕获，它是一个高效且常用的捕获模式。

## 2. 项目目标

### 目标概述

- **读取视频数据**：通过 `libv4l2cpp` 库从摄像头设备读取视频数据。
- **保存视频帧**：将捕获的每一帧视频数据保存为 JPEG 图像。
- **多线程处理**：使用生产者消费者模型和多线程技术，分离数据捕获和数据消费过程，以提高效率。
- **回调机制**：通过回调函数解耦数据处理逻辑，为后续扩展提供便利。

### 项目实现原理

1. **V4L2 捕获**：通过 `libv4l2cpp` 库，使用 **MMAP** 模式从摄像头设备读取原始视频帧。
2. **生产者消费者模型**：使用两个线程：一个捕获线程（生产者）和一个消费线程（消费者），实现数据的异步处理。
3. **回调函数**：消费线程通过回调函数处理捕获的帧，以便未来可以灵活扩展（如实时处理、压缩、保存为其他格式等）。

## 3. 使用 `libv4l2cpp` 实现视频捕获

### 如何使用 `libv4l2cpp`

`libv4l2cpp` 是一个现代 C++ 库，它封装了底层的 V4L2 API，使得与 V4L2 的交互更加简便和高效。下面的代码展示了如何通过 `libv4l2cpp` 捕获视频并使用 OpenCV 处理每一帧。

#### 初始化 V4L2 捕获

我们首先需要通过 `V4l2Capture` 类来初始化视频捕获，并设置摄像头设备的相关参数（如视频格式、分辨率、帧率等）。

```cpp
static std::unique_ptr<V4l2Capture> init_v4l2_capture(const std::string& device, int format, int width, int height, int fps, V4l2IoType ioTypeIn) {
    V4L2DeviceParameters param(device.c_str(), format, width, height, fps, ioTypeIn);
    std::unique_ptr<V4l2Capture> video_ctx(V4l2Capture::create(param));

    if (!video_ctx) {
        spdlog::error("Cannot initialize V4L2 capture on device {}", device);
        std::exit(-1);
    } else {
        spdlog::info("V4L2 Capture Initialized for device: {}", device);
    }

    return video_ctx;
}
```

### 捕获线程

`capture_function` 线程负责从 V4L2 捕获视频帧，并将每一帧数据放入一个队列中，供消费线程使用。

```cpp
void capture_function(std::unique_ptr<V4l2Capture>& video_ctx, std::queue<std::vector<char>> &frame_queue, 
                    std::mutex &frame_mutex, std::condition_variable &cv, std::atomic<bool> &stop) {
    timeval tv;

    spdlog::info("Starting reading.");

    while (!stop) {
        tv.tv_sec=1;
        tv.tv_usec=0;
        int ret = video_ctx->isReadable(&tv);
        if (ret == 1) {
            std::vector<char> frame(video_ctx->getBufferSize());
            size_t bytesRead = video_ctx->read(frame.data(), frame.size());
            if (bytesRead > 0) {
                frame.resize(bytesRead);
                spdlog::debug("Captured frame size: {} {}", bytesRead, frame.size());
                {
                    std::lock_guard<std::mutex> lock(frame_mutex);
                    frame_queue.push(frame);
                }
                cv.notify_one();
            }
        } else if (ret == -1) {
            spdlog::error("Error reading frame: {}", strerror(errno));
            stop = true;
        }
    }
}
```

### 消费线程与回调函数

消费线程通过回调函数来处理捕获的每一帧。这种设计方式能够确保程序在扩展时，消费者部分可以灵活适配新的功能需求。

```cpp
void consumer_function(std::function<void(std::vector<char>, int)> process_frame, const int stop_count,
                        std::queue<std::vector<char>> &frame_queue, std::mutex &frame_mutex, std::condition_variable &cv, std::atomic<bool> &stop) {
    int frame_count = 0;
    while (!stop && frame_count < stop_count) {
        std::unique_lock<std::mutex> lock(frame_mutex);
        cv.wait(lock, [&frame_queue, &stop] { return !frame_queue.empty() || stop; });
        if (!frame_queue.empty()) {
            std::vector<char> frame = frame_queue.front();
            frame_queue.pop();
            lock.unlock();
            process_frame(frame, frame_count);
        }
        frame_count++;
    }

    if (frame_count >= stop_count) {
        stop = true;
    }
}
```

### 回调函数 - 保存图像

我们使用 OpenCV 处理每一帧，将其保存为 JPEG 图像。

```cpp
std::function<void(std::vector<char>, int)> save_frame = [](std::vector<char> frame, int frame_count) {
    cv::Mat img = cv::imdecode(frame, cv::IMREAD_COLOR);  // Decode as color image
    if (img.empty()) {
        spdlog::error("Failed to decode frame");
        return;
    }

    // Save the frame to disk
    char filename[128];
    if (!std::filesystem::exists("output")) {
        std::filesystem::create_directory("output");
    }
    snprintf(filename, sizeof(filename), "output/frame_%04d.jpg", frame_count);

    if (cv::imwrite(filename, img)) {
        spdlog::info("Frame {} saved as: {}", frame_count, filename);
    } else {
        spdlog::error("Failed to save frame {}.", frame_count);
    }
};
```

## 4. 扩展与未来的改进

### 1. **实时视频处理**：
目前的代码仅仅是将视频帧保存为图像。为了进一步扩展，我们可以在回调函数中添加实时图像处理功能，比如应用图像滤镜、目标检测等。

### 2. **视频录制功能**：
我们可以扩展此功能，将捕获的视频帧保存为一个视频文件。可以使用 OpenCV 的 `cv::VideoWriter` 类将帧保存为视频文件（如 MP4 格式）。

## 5. 总结

本文演示了如何使用 `libv4l2cpp` 库，通过生产者消费者模型和多线程的方式，连续读取摄像头数据并保存图像。我们设计了一个灵活的架构，使用回调函数来解耦数据处理逻辑，为未来的扩展提供了便利。这个 Demo 项目为你深入了解 V4L2 的使用和视频捕获处理提供了一个良好的基础。
