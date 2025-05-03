#include "v4l2_camera_device.hpp"
#include <iostream>
#include <linux/videodev2.h>

/**
 * @brief 构造函数
 */
v4l2_camera_device::v4l2_camera_device(const std::string& device_path,
                                       unsigned int width,
                                       unsigned int height,
                                       unsigned int format,
                                       int camera_id)
    : m_device_path(device_path),
      m_width(width),
      m_height(height),
      m_format(format),
      m_camera_id(camera_id),
      m_is_capturing(false),
      m_timestamp(0),
      m_capture(nullptr)
{
}

/**
 * @brief 析构函数
 */
v4l2_camera_device::~v4l2_camera_device()
{
    // 如果还在捕获，先停止
    if (m_is_capturing) {
        stop_capture();
    }
}

/**
 * @brief 初始化摄像头设备
 */
bool v4l2_camera_device::initialize()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    
    try {
        // 创建V4L2设备参数，指定使用MMAP模式
        V4L2DeviceParameters params(m_device_path.c_str(), m_format, m_width, m_height, 30);
        params.m_iotype = IOTYPE_MMAP; // 使用MMAP模式，更高效
        
        // 创建V4L2捕获设备
        m_capture.reset(V4l2Capture::create(params));
        
        if (!m_capture) {
            std::cerr << "Failed to create V4L2 capture for device: " << m_device_path << std::endl;
            return false;
        }
        
        std::cout << "Device " << m_device_path << " initialized with format: " 
                  << m_capture->getFormat() << " size: " << m_capture->getWidth() 
                  << "x" << m_capture->getHeight() << std::endl;
        return true;
    } 
    catch (const std::exception& e) {
        std::cerr << "Exception during v4l2_camera_device initialization: " << e.what() << std::endl;
        return false;
    }
}

/**
 * @brief 开始捕获
 */
bool v4l2_camera_device::start_capture()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (!m_capture) {
        std::cerr << "Cannot start capture: device not initialized" << std::endl;
        return false;
    }
    
    if (m_is_capturing) {
        // 已经在捕获中
        return true;
    }
    
    // MMAP模式在初始化时已经启动了流，这里只需标记状态
    m_is_capturing = true;
    return true;
}

/**
 * @brief 停止捕获
 */
bool v4l2_camera_device::stop_capture()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (!m_is_capturing) {
        // 已经停止
        return true;
    }
    
    // 标记状态为停止
    m_is_capturing = false;
    return true;
}

/**
 * @brief 获取一帧图像
 */
std::shared_ptr<buffer> v4l2_camera_device::get_frame()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (!m_capture || !m_is_capturing) {
        return nullptr;
    }
    
    try {
        // 检查是否有数据可读
        struct timeval tv;
        tv.tv_sec = 1;  // 1秒超时
        tv.tv_usec = 0;
        
        if (!m_capture->isReadable(&tv)) {
            std::cerr << "Timeout waiting for frame on device " << m_device_path << std::endl;
            return nullptr;
        }
        
        // 获取当前时间戳（简单使用系统时钟）
        auto now = std::chrono::high_resolution_clock::now();
        m_timestamp = std::chrono::duration_cast<std::chrono::microseconds>(
            now.time_since_epoch()).count();
            
        // 估计需要的缓冲区大小
        size_t buffer_size = m_capture->getBufferSize();
        if (buffer_size == 0) {
            std::cerr << "Invalid buffer size for device " << m_device_path << std::endl;
            return nullptr;
        }
        
        // 创建buffer对象
        auto frame = std::make_shared<buffer>(buffer_size);
        if (!frame) {
            std::cerr << "Failed to allocate buffer for frame" << std::endl;
            return nullptr;
        }
        
        // 直接从设备读取数据到buffer
        size_t bytes_read = m_capture->read(static_cast<char*>(frame->data()), buffer_size);
        
        if (bytes_read <= 0) {
            std::cerr << "Failed to read frame from device " << m_device_path << std::endl;
            return nullptr;
        }
        
        // 调整buffer大小为实际读取的字节数
        frame->resize(bytes_read);
        frame->set_timestamp(m_timestamp);
        
        return frame;
    } catch (const std::exception& e) {
        std::cerr << "Exception during frame capture: " << e.what() << std::endl;
        return nullptr;
    }
}

/**
 * @brief 获取时间戳
 */
int64_t v4l2_camera_device::get_timestamp() const
{
    return m_timestamp;
}

/**
 * @brief 获取摄像头ID
 */
int v4l2_camera_device::get_camera_id() const
{
    return m_camera_id;
}

/**
 * @brief 获取实际宽度
 */
unsigned int v4l2_camera_device::get_width() const
{
    if (m_capture) {
        return m_capture->getWidth();
    }
    return m_width;
}

/**
 * @brief 获取实际高度
 */
unsigned int v4l2_camera_device::get_height() const
{
    if (m_capture) {
        return m_capture->getHeight();
    }
    return m_height;
}

/**
 * @brief 获取实际格式
 */
unsigned int v4l2_camera_device::get_format() const
{
    if (m_capture) {
        return m_capture->getFormat();
    }
    return m_format;
}
