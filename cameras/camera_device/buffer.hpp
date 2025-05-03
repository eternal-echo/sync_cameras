#pragma once

#include <vector>
#include <cstdint>
#include <memory>

/**
 * @brief 通用缓冲区类
 * 
 * 用于存储摄像头捕获的图像数据，并附带时间戳和序列号信息
 */
class buffer {
public:
    /**
     * @brief 默认构造函数
     */
    buffer() : m_timestamp(0), m_sequence(0) {}
    
    /**
     * @brief 构造函数，预分配指定大小的内存
     * 
     * @param size 缓冲区大小（字节）
     * @param timestamp 初始时间戳（可选）
     * @param sequence 初始序列号（可选）
     */
    explicit buffer(size_t size, int64_t timestamp = 0, uint64_t sequence = 0) 
        : m_data(size, 0), m_timestamp(timestamp), m_sequence(sequence) {}
    
    /**
     * @brief 获取数据指针
     * 
     * @return void* 指向内部数据的指针
     */
    void* data() { return m_data.data(); }
    
    /**
     * @brief 获取常量数据指针
     * 
     * @return const void* 指向内部数据的常量指针
     */
    const void* data() const { return m_data.data(); }
    
    /**
     * @brief 获取缓冲区大小
     * 
     * @return size_t 缓冲区大小（字节）
     */
    size_t size() const { return m_data.size(); }
    
    /**
     * @brief 调整缓冲区大小
     * 
     * @param new_size 新的缓冲区大小
     */
    void resize(size_t new_size) { m_data.resize(new_size); }
    
    /**
     * @brief 清空缓冲区
     */
    void clear() { m_data.clear(); }

    // 支持下标访问
    uint8_t& operator[](size_t index) { return m_data[index]; }
    const uint8_t& operator[](size_t index) const { return m_data[index]; }

    // 禁止拷贝，只允许移动
    buffer(const buffer&) = delete;
    buffer& operator=(const buffer&) = delete;
    
    // 允许移动
    buffer(buffer&&) = default;
    buffer& operator=(buffer&&) = default;

    /**
     * @brief 获取时间戳
     * 
     * @return int64_t 时间戳（微秒）
     */
    int64_t timestamp() const { return m_timestamp; }
    
    /**
     * @brief 设置时间戳
     * 
     * @param timestamp 时间戳（微秒）
     */
    void set_timestamp(int64_t timestamp) { m_timestamp = timestamp; }
    
    /**
     * @brief 获取序列号
     * 
     * @return uint64_t 序列号
     */
    uint64_t sequence() const { return m_sequence; }
    
    /**
     * @brief 设置序列号
     * 
     * @param sequence 序列号
     */
    void set_sequence(uint64_t sequence) { m_sequence = sequence; }

protected:
    int64_t m_timestamp;           // 时间戳（微秒）
    uint64_t m_sequence;           // 序列号

private:
    std::vector<uint8_t> m_data;   // 内部数据存储
};