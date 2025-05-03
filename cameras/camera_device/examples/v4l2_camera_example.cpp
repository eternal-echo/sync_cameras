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
    std::cout << "  -o DIR       指定输出目录 (默认: output)" << std::endl;
    std::cout << "  -i INTERVAL  保存图片的间隔(ms) (默认: 100)" << std::endl;
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
            cv::Mat yuyv(height, width, CV_8UC2, frame->data());
            cv::cvtColor(yuyv, result, cv::COLOR_YUV2BGR_YUYV);
            break;
        }
        case V4L2_PIX_FMT_MJPEG:
        {
            try {
                result = cv::imdecode(cv::Mat(1, frame->size(), CV_8UC1, frame->data()), cv::IMREAD_COLOR);
            } catch (const cv::Exception& e) {
                std::cerr << "OpenCV error: " << e.what() << std::endl;
            }
            break;
        }
        default:
            std::cerr << "不支持的格式: " << format << std::endl;
            break;
    }
    
    return result;
}

// 格式化时间戳为字符串
std::string formatTimestamp(int64_t timestamp_us)
{
    // 将微秒转换为秒和剩余微秒
    auto seconds = timestamp_us / 1000000;
    auto microseconds = timestamp_us % 1000000;
    
    // 转换为日历时间
    auto time_point = std::chrono::system_clock::time_point(
        std::chrono::seconds(seconds) + std::chrono::microseconds(microseconds));
    auto time_t = std::chrono::system_clock::to_time_t(time_point);
    
    // 格式化输出
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y%m%d_%H%M%S_")
       << std::setfill('0') << std::setw(6) << microseconds;
    return ss.str();
}

int main(int argc, char* argv[])
{
    // 默认参数
    std::string device_path = "/dev/video0";
    int width = 640;
    int height = 480;
    unsigned int format = V4L2_PIX_FMT_MJPEG;
    int camera_id = 0;
    std::string output_dir = "output";
    int save_interval = 100; // 保存图片的间隔(ms)
    
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
        } else if (strcmp(argv[i], "-o") == 0 && i+1 < argc) {
            output_dir = argv[++i];
        } else if (strcmp(argv[i], "-i") == 0 && i+1 < argc) {
            save_interval = std::stoi(argv[++i]);
        } else if (argv[i][0] != '-') {
            device_path = argv[i];
        }
    }
    
    std::cout << "启动 V4L2 摄像头示例: " << device_path 
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
        std::cerr << "初始化摄像头失败!" << std::endl;
        return 1;
    }
    
    // 获取实际的分辨率 (从摄像头设备获取实际值)
    int actual_width = camera->get_width();
    int actual_height = camera->get_height();
    
    std::cout << "实际使用的分辨率: " << actual_width << "x" << actual_height << std::endl;
    
    // 开始捕获
    if (!camera->start_capture()) {
        std::cerr << "启动捕获失败!" << std::endl;
        return 1;
    }
    
    std::cout << "开始捕获图像，按 Ctrl+C 退出..." << std::endl;
    
    // 捕获循环
    int frames_count = 0;
    uint64_t sequence = 0;
    
    while (true) {
        // 捕获一帧
        auto frame = camera->get_frame();
        if (!frame) {
            std::cerr << "无法获取帧!" << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }
        
        // 设置序列号
        frame->set_sequence(sequence++);
        frames_count++;
        
        // 转换图像
        cv::Mat image = convertToMat(frame, actual_width, actual_height, format);
        if (image.empty()) {
            std::cerr << "无法解码图像" << std::endl;
            continue;
        }
        
        // 在图像上添加时间戳和序列号信息
        std::string timestamp_text = "TS: " + std::to_string(frame->timestamp()) + 
                                     " μs, Seq: " + std::to_string(frame->sequence());
        cv::putText(image, timestamp_text, 
                    cv::Point(10, 30), cv::FONT_HERSHEY_SIMPLEX, 0.7,
                    cv::Scalar(0, 255, 0), 2);
        
        // 使用帧的时间戳来命名文件
        std::string timestamp_str = formatTimestamp(frame->timestamp());
        std::stringstream filename;
        filename << output_dir << "/frame_" << timestamp_str 
                 << "_seq" << std::setfill('0') << std::setw(6) << frame->sequence()
                 << ".jpg";
        
        cv::imwrite(filename.str(), image);
        std::cout << "已保存: " << filename.str() << std::endl;
        
        // 根据指定的间隔等待
        std::this_thread::sleep_for(std::chrono::milliseconds(save_interval));
    }
    
    // 停止捕获
    camera->stop_capture();
    
    std::cout << "程序已退出，共捕获 " << frames_count << " 帧" << std::endl;
    
    return 0;
}
