// icamera_device.h
#pragma once
#include <memory>
#include <cstdint>
#include "buffer.hpp"

// 前向声明
class buffer;

class icamera_device {
public:
    virtual ~icamera_device() = default;
    
    // 初始化设备
    virtual bool initialize() = 0;
    
    // 开始/停止捕获
    virtual bool start_capture() = 0;
    virtual bool stop_capture() = 0;
    
    // 获取一帧数据
    virtual std::shared_ptr<buffer> get_frame() = 0;
    
    // 获取时间戳（微秒级）
    virtual int64_t get_timestamp() const = 0;
    
    // 获取摄像头ID
    virtual int get_camera_id() const = 0;
};