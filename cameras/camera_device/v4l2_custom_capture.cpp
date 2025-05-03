#include "v4l2_custom_capture.hpp"
#include "libv4l2cpp/inc/V4l2MmapDevice.h"
#include <iostream>
#include <chrono>
#include <linux/videodev2.h>

/**
 * @brief 创建V4l2CustomCapture实例
 * 
 * @param device_path 设备路径
 * @param width 图像宽度
 * @param height 图像高度
 * @param format 像素格式
 * @param fps 帧率
 * @return V4l2CustomCapture* 捕获对象指针
 */
V4l2CustomCapture* V4l2CustomCapture::create(const std::string& device_path, 
                                           unsigned int width, 
                                           unsigned int height, 
                                           unsigned int format,
                                           unsigned int fps)
{
    try {
        // 创建设备参数，指定使用MMAP IO模式
        V4L2DeviceParameters params(device_path.c_str(), format, width, height, fps);
        params.m_iotype = V4l2Device::IOTYPE_MMAP;
        
        // 创建底层V4L2设备
        V4l2Device* device = new V4l2MmapDevice(params, V4L2_BUF_TYPE_VIDEO_CAPTURE);
        
        // 初始化设备，要求支持视频捕获和流媒体能力
        int caps = V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_STREAMING;
        if (!device->init(caps)) {
            std::cerr << "Failed to initialize V4L2 device: " << device_path << std::endl;
            delete device;
            return nullptr;
        }
        
        // 创建自定义捕获对象
        return new V4l2CustomCapture(device);
    } 
    catch (const std::exception& e) {
        std::cerr << "Exception during V4l2CustomCapture creation: " << e.what() << std::endl;
        return nullptr;
    }
}

/**
 * @brief 构造函数
 * 
 * @param device 已初始化的V4l2Device设备对象
 */
V4l2CustomCapture::V4l2CustomCapture(V4l2Device* device)
    : V4l2Capture(device),
      m_timestamp(0),
      m_use_kernel_timestamp(true)
{
}

/**
 * @brief 析构函数
 */
V4l2CustomCapture::~V4l2CustomCapture()
{
    // 基类会处理设备的释放
}

/**
 * @brief 捕获一帧并生成buffer对象
 * 
 * @return std::shared_ptr<buffer> 帧数据
 */
std::shared_ptr<buffer> V4l2CustomCapture::captureFrame()
{
    struct timeval tv;
    tv.tv_sec = 1;  // 设置1秒超时
    tv.tv_usec = 0;
    
    // 检查设备是否有数据可读
    if (!isReadable(&tv)) {
        return nullptr;
    }
    
    try {
        // 获取一个足够大的buffer，大小基于图像尺寸
        unsigned int width = getWidth();
        unsigned int height = getHeight();
        unsigned int format = getFormat();
        
        // 根据格式计算需要的缓冲区大小，这里用最大可能值
        size_t buffer_size = width * height * 4;  // 保守估计，确保足够大
        
        // 创建帧缓冲区
        auto frame_buffer = std::make_shared<buffer>(buffer_size);
        if (!frame_buffer) {
            return nullptr;
        }
        
        // 从设备读取数据
        size_t bytes_read = read(static_cast<char*>(frame_buffer->data()), buffer_size);
        
        if (bytes_read <= 0) {
            // 读取失败
            return nullptr;
        }
        
        // 调整buffer大小为实际读取的字节数
        frame_buffer->resize(bytes_read);
        
        // 获取当前时间戳
        if (!m_use_kernel_timestamp || !getDevice()->hasCapability(V4L2_CAP_TIMEPERFRAME)) {
            // 使用系统时间戳
            auto now = std::chrono::high_resolution_clock::now();
            m_timestamp = std::chrono::duration_cast<std::chrono::microseconds>(
                now.time_since_epoch()).count();
        }
        // 注意：如果启用了内核时间戳，时间戳会在read方法内部由底层驱动更新
        
        return frame_buffer;
    } 
    catch (const std::exception& e) {
        std::cerr << "Exception during frame capture: " << e.what() << std::endl;
        return nullptr;
    }
}

/**
 * @brief 获取最后一帧的时间戳
 * 
 * @return int64_t 时间戳（微秒）
 */
int64_t V4l2CustomCapture::getTimestamp() const
{
    return m_timestamp;
}

/**
 * @brief 设置是否使用内核时间戳
 * 
 * @param use true表示使用内核时间戳
 */
void V4l2CustomCapture::useKernelTimestamp(bool use)
{
    m_use_kernel_timestamp = use;
}
