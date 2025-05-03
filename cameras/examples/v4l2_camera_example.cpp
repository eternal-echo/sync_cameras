#include <iostream>
#include <chrono>
#include <thread>
#include <memory>
#include <linux/videodev2.h>
#include <opencv2/opencv.hpp>

#include "../v4l2_camera_device.hpp"
#include "../buffer.hpp"

// 将二进制数据转换为OpenCV格式
cv::Mat convertToMat(const std::shared_ptr<buffer>& frame, int width, int height, unsigned int format)
{
    cv::Mat result;
    
    // 根据不同格式进行转换
    switch (format)
    {
        case V4L2_PIX_FMT_YUYV:
        {
            // YUYV格式
            cv::Mat yuyv(height, width, CV_8UC2, frame->data());
            cv::cvtColor(yuyv, result, cv::COLOR_YUV2BGR_YUYV);
            break;
        }
        case V4L2_PIX_FMT_MJPEG:
        {
            // MJPEG格式
            try {
                result = cv::imdecode(cv::Mat(1, frame->size(), CV_8UC1, frame->data()), cv::IMREAD_COLOR);
            } catch (const cv::Exception& e) {
                std::cerr << "OpenCV error: " << e.what() << std::endl;
            }
            break;
        }
        default:
            std::cout << "Unsupported format: " << format << std::endl;
            break;
    }
    
    return result;
}

int main(int argc, char* argv[])
{
    // 默认参数
    std::string device_path = "/dev/video0";
    int width = 640;
    int height = 480;
    unsigned int format = V4L2_PIX_FMT_MJPEG;
    int camera_id = 0;
    
    // 解析命令行参数
    if (argc > 1) device_path = argv[1];
    if (argc > 2) width = std::stoi(argv[2]);
    if (argc > 3) height = std::stoi(argv[3]);
    if (argc > 4) {
        std::string fmt = argv[4];
        if (fmt == "YUYV") format = V4L2_PIX_FMT_YUYV;
    }
    
    std::cout << "Starting V4L2 camera example on device: " << device_path 
              << " (" << width << "x" << height << ")" << std::endl;
    
    // 创建并初始化摄像头
    auto camera = std::make_shared<v4l2_camera_device>(device_path, width, height, format, camera_id);
    if (!camera->initialize()) {
        std::cerr << "Failed to initialize camera!" << std::endl;
        return 1;
    }
    
    // 开始捕获
    if (!camera->start_capture()) {
        std::cerr << "Failed to start capture!" << std::endl;
        return 1;
    }
    
    // 创建显示窗口
    cv::namedWindow("Camera Feed", cv::WINDOW_AUTOSIZE);
    
    // 捕获循环
    int frames_count = 0;
    auto start_time = std::chrono::steady_clock::now();
    
    std::cout << "Press ESC to exit" << std::endl;
    
    while (true) {
        // 捕获一帧
        auto frame = camera->get_frame();
        if (!frame) {
            std::cerr << "Failed to get frame!" << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }
        
        frames_count++;
        
        // 转换并显示
        cv::Mat image = convertToMat(frame, width, height, format);
        if (!image.empty()) {
            // 计算FPS
            auto now = std::chrono::steady_clock::now();
            double elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time).count() / 1000.0;
            double fps = frames_count / elapsed;
            
            // 显示FPS
            cv::putText(image, "FPS: " + std::to_string(fps), 
                    cv::Point(10, 30), cv::FONT_HERSHEY_SIMPLEX, 1.0,
                    cv::Scalar(0, 255, 0), 2);
            
            cv::imshow("Camera Feed", image);
            
            // 按ESC键退出
            if (cv::waitKey(1) == 27) break;
        }
    }
    
    // 停止捕获
    camera->stop_capture();
    cv::destroyAllWindows();
    
    return 0;
}
