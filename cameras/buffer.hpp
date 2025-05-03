#pragma once

#include <vector>
#include <cstdint>
#include <memory>

/**
 * @brief 通用缓冲区类
 * 
 * 用于存储摄像头捕获的图像数据
 */
class buffer {
public:
    /**
     * @brief 默认构造函数
     */
    buffer() = default;
    
    /**
     * @brief 构造函数，预分配指定大小的内存
     * 
     * @param size 缓冲区大小（字节）
     */
    explicit buffer(size_t size) : m_data(size, 0) {}
    
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

private:
    std::vector<uint8_t> m_data; // 内部数据存储
};