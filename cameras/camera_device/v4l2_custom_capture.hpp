#pragma once

#include <string>
#include <memory>
#include <chrono>
#include <vector>
#include "libv4l2cpp/inc/V4l2Capture.h"
#include "buffer.hpp"

/**
 * @brief V4L2 视频捕获的自定义扩展类
 * 
 * 扩展了libv4l2cpp库中的V4l2Capture功能，增加了精确的时间戳管理
 * 和直接创建buffer对象的能力
 */
class V4l2CustomCapture : public V4l2Capture {
public:
    /**
     * @brief 静态创建方法
     * 
     * @param device_path 设备路径
     * @param width 图像宽度
     * @param height 图像高度
     * @param format 像素格式，如V4L2_PIX_FMT_YUYV
     * @param fps 帧率
     * @return V4l2CustomCapture* 捕获对象指针，失败时返回nullptr
     */
    static V4l2CustomCapture* create(const std::string& device_path, 
                                     unsigned int width, 
                                     unsigned int height, 
                                     unsigned int format,
                                     unsigned int fps);

    /**
     * @brief 构造函数
     * 
     * @param device V4l2Device设备对象
     */
    V4l2CustomCapture(V4l2Device* device);
    
    /**
     * @brief 析构函数
     */
    virtual ~V4l2CustomCapture();

    /**
     * @brief 捕获一帧并返回buffer对象
     * 
     * @return std::shared_ptr<buffer> 帧数据，捕获失败返回nullptr
     */
    std::shared_ptr<buffer> captureFrame();
    
    /**
     * @brief 获取最后一帧的时间戳（微秒）
     * 
     * @return int64_t 时间戳
     */
    int64_t getTimestamp() const;
    
    /**
     * @brief 设置是否使用内核时间戳（如果可用）
     * 
     * @param use true表示使用内核时间戳，false表示使用系统时间
     */
    void useKernelTimestamp(bool use);

private:
    int64_t m_timestamp;            // 最后一帧的时间戳（微秒）
    bool m_use_kernel_timestamp;    // 是否使用内核时间戳
};
