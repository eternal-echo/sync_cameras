#include <iostream>
#include <chrono>
#include <thread>
#include <memory>
#include <linux/videodev2.h>
#include <opencv2/opencv.hpp>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <cstring>

#include "../v4l2_camera_device.hpp"
#include "../buffer.hpp"

// 确保目录存在，如果不存在则创建
bool ensure_directory_exists(const std::string& path) {
    try {
        if (!std::filesystem::exists(path)) {
            return std::filesystem::create_directory(path);
        }
        return true;
    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "创建目录时出错: " << e.what() << std::endl;
        return false;
    }
}

// 显示帮助信息
void show_usage(const char* program_name) {
    std::cout << "用法: " << program_name << " [选项] [设备]" << std::endl;
    std::cout << "选项:" << std::endl;
    std::cout << "  -w WIDTH     设置宽度 (默认: 640)" << std::endl;
    std::cout << "  -h HEIGHT    设置高度 (默认: 480)" << std::endl;
    std::cout << "  -f FORMAT    设置格式 (MJPEG 或 YUYV, 默认: MJPEG)" << std::endl;
    std::cout << "  -n           无GUI模式，不显示窗口" << std::endl;
    std::cout << "  -o DIR       指定输出目录 (默认: output)" << std::endl;
    std::cout << "  --help       显示此帮助信息" << std::endl;
    std::cout << "示例:" << std::endl;
    std::cout << "  " << program_name << " -w 1280 -h 720 -f MJPEG /dev/video0" << std::endl;
}

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
    bool no_gui = false;
    std::string output_dir = "output";
    
    // 解析命令行参数
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--help") == 0) {
            show_usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "-w") == 0 && i+1 < argc) {
            width = std::stoi(argv[++i]);
        } else if (strcmp(argv[i], "-h") == 0 && i+1 < argc) {
            height = std::stoi(argv[++i]);
        } else if (strcmp(argv[i], "-f") == 0 && i+1 < argc) {
            std::string fmt = argv[++i];
            if (fmt == "YUYV") format = V4L2_PIX_FMT_YUYV;
        } else if (strcmp(argv[i], "-n") == 0) {
            no_gui = true;
        } else if (strcmp(argv[i], "-o") == 0 && i+1 < argc) {
            output_dir = argv[++i];
        } else if (argv[i][0] != '-') {
            device_path = argv[i];
        }
    }
    
    std::cout << "Starting V4L2 camera example on device: " << device_path 
              << " (" << width << "x" << height << ")" << std::endl;
    
    // 创建output目录
    if (!ensure_directory_exists(output_dir)) {
        std::cerr << "创建输出目录失败!" << std::endl;
        return 1;
    }
    std::cout << "图像将保存到: " << std::filesystem::absolute(output_dir) << std::endl;
    
    // 创建并初始化摄像头
    auto camera = std::make_shared<v4l2_camera_device>(device_path, width, height, format, camera_id);
    if (!camera->initialize()) {
        std::cerr << "Failed to initialize camera!" << std::endl;
        return 1;
    }
    
    // 获取实际的分辨率 (从摄像头设备获取实际值)
    int actual_width = camera->get_width();
    int actual_height = camera->get_height();
    
    std::cout << "实际使用的分辨率: " << actual_width << "x" << actual_height << std::endl;
    
    // 开始捕获
    if (!camera->start_capture()) {
        std::cerr << "Failed to start capture!" << std::endl;
        return 1;
    }
    
    // GUI模式下创建窗口
    if (!no_gui) {
        try {
            cv::namedWindow("Camera Feed", cv::WINDOW_AUTOSIZE);
        } catch (const cv::Exception& e) {
            std::cerr << "警告: 无法创建OpenCV窗口，切换到无GUI模式: " << e.what() << std::endl;
            no_gui = true;
        }
    }
    
    // 捕获循环
    int frames_count = 0;
    auto start_time = std::chrono::steady_clock::now();
    
    if (!no_gui) {
        std::cout << "Press ESC to exit" << std::endl;
    } else {
        std::cout << "无GUI模式运行中，按Ctrl+C退出" << std::endl;
    }
    
    while (true) {
        // 捕获一帧
        auto frame = camera->get_frame();
        if (!frame) {
            std::cerr << "Failed to get frame!" << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }
        
        frames_count++;
        
        // 转换图像
        cv::Mat image = convertToMat(frame, actual_width, actual_height, format);
        if (image.empty()) {
            std::cerr << "无法解码图像" << std::endl;
            continue;
        }
        
        // 计算FPS
        auto now = std::chrono::steady_clock::now();
        double elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time).count() / 1000.0;
        double fps = frames_count / elapsed;
        
        // 显示FPS
        cv::putText(image, "FPS: " + std::to_string(fps), 
                cv::Point(10, 30), cv::FONT_HERSHEY_SIMPLEX, 1.0,
                cv::Scalar(0, 255, 0), 2);
        
        // GUI模式下显示
        if (!no_gui) {
            try {
                cv::imshow("Camera Feed", image);
                
                // 按ESC键退出
                if (cv::waitKey(1) == 27) break;
            } catch (const cv::Exception& e) {
                std::cerr << "显示图像时出错，切换到无GUI模式: " << e.what() << std::endl;
                no_gui = true;
            }
        }
        
        // 保存图像到output目录
        auto time_now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(time_now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            time_now.time_since_epoch()) % 1000;
        
        std::stringstream filename;
        filename << output_dir << "/frame_"
                 << std::put_time(std::localtime(&time_t), "%Y%m%d_%H%M%S_")
                 << std::setfill('0') << std::setw(3) << ms.count()
                 << ".jpg";
        
        cv::imwrite(filename.str(), image);
        std::cout << "已保存: " << filename.str() << " (FPS: " << fps << ")" << std::endl;
        
        // 无GUI模式下限制保存频率
        if (no_gui) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    
    // 停止捕获
    camera->stop_capture();
    if (!no_gui) {
        cv::destroyAllWindows();
    }
    
    std::cout << "程序已退出，共捕获 " << frames_count << " 帧" << std::endl;
    
    return 0;
}
