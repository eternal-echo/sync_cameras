#pragma once

#include <string>
#include <memory>
#include <chrono>
#include <mutex>

#include "camera_device.hpp"
#include "libv4l2cpp/inc/V4l2Capture.h"

/**
 * @brief V4L2摄像头设备实现类
 * 
 * 基于libv4l2cpp V4l2Capture实现的摄像头设备
 */
class v4l2_camera_device : public icamera_device {
public:
    /**
     * @brief 构造函数
     * 
     * @param device_path 设备路径，如"/dev/video0"
     * @param width 图像宽度
     * @param height 图像高度
     * @param format 像素格式，如V4L2_PIX_FMT_YUYV
     * @param camera_id 摄像头ID
     */
    v4l2_camera_device(const std::string& device_path,
                       unsigned int width,
                       unsigned int height,
                       unsigned int format,
                       int camera_id);
                       
    /**
     * @brief 析构函数
     */
    ~v4l2_camera_device() override;

    bool initialize() override;
    bool start_capture() override;
    bool stop_capture() override;
    std::shared_ptr<buffer> get_frame() override;
    int64_t get_timestamp() const override;
    int get_camera_id() const override;

    /**
     * @brief 获取实际分辨率的方法
     * 
     * @return 图像宽度
     */
    unsigned int get_width() const;

    /**
     * @brief 获取实际分辨率的方法
     * 
     * @return 图像高度
     */
    unsigned int get_height() const;

    /**
     * @brief 获取实际分辨率的方法
     * 
     * @return 像素格式
     */
    unsigned int get_format() const;

private:
    std::string m_device_path;       // 设备路径
    unsigned int m_width;            // 图像宽度
    unsigned int m_height;           // 图像高度
    unsigned int m_format;           // 像素格式
    int m_camera_id;                 // 摄像头ID
    bool m_is_capturing;             // 是否正在捕获
    int64_t m_timestamp;             // 最后一帧的时间戳
    
    std::unique_ptr<V4l2Capture> m_capture; // V4L2捕获设备
    std::mutex m_mutex;                     // 互斥锁
};
