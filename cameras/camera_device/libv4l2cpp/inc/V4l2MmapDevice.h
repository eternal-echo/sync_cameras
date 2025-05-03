/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose.
**
** V4l2MmapDevice.h
** 
** V4L2 source using mmap API
**
** -------------------------------------------------------------------------*/


#pragma once
 
#include "V4l2Device.h"

// 内存映射缓冲区数量，影响视频流的平滑度和延迟
#define V4L2MMAP_NBBUFFER 10

/**
 * @brief V4L2设备的内存映射实现类
 * 
 * 该类通过内存映射(mmap)方式与V4L2设备交互，相比读写方式有更高的性能
 * 实现了视频数据的高效采集和输出
 */
class V4l2MmapDevice : public V4l2Device
{	
	protected:	
		/**
		 * @brief 将数据写入设备的内部实现
		 * 
		 * @param buffer 包含要写入数据的缓冲区
		 * @param bufferSize 要写入的数据大小
		 * @return size_t 实际写入的字节数，失败返回-1
		 */
		size_t writeInternal(char* buffer, size_t bufferSize);
		
		/**
		 * @brief 开始部分写入操作
		 * 
		 * 为分段写入数据准备缓冲区，用于大量数据分批写入场景
		 * 
		 * @return true 成功获取缓冲区
		 * @return false 获取缓冲区失败或已有写入操作进行中
		 */
		bool   startPartialWrite();
		
		/**
		 * @brief 部分写入数据的内部实现
		 * 
		 * 在startPartialWrite后调用，向已锁定的缓冲区追加数据
		 * 
		 * @param buffer 包含要写入数据的缓冲区
		 * @param bufferSize 要写入的数据大小
		 * @return size_t 实际写入的字节数
		 */
		size_t writePartialInternal(char*, size_t);
		
		/**
		 * @brief 结束部分写入操作
		 * 
		 * 提交已写入的缓冲区数据到设备
		 * 
		 * @return true 成功提交数据
		 * @return false 提交失败或无进行中的写入操作
		 */
		bool   endPartialWrite();
		
		/**
		 * @brief 从设备读取数据的内部实现
		 * 
		 * 通过内存映射方式获取一帧视频数据
		 * 
		 * @param buffer 目标缓冲区，用于存储读取的数据
		 * @param bufferSize 目标缓冲区的大小
		 * @return size_t 实际读取的字节数，失败返回-1
		 */
		size_t readInternal(char* buffer, size_t bufferSize);
			
	public:
		/**
		 * @brief 构造函数
		 * 
		 * @param params V4L2设备参数，包含设备路径等信息
		 * @param deviceType 设备类型，如V4L2_BUF_TYPE_VIDEO_CAPTURE
		 */
		V4l2MmapDevice(const V4L2DeviceParameters & params, v4l2_buf_type deviceType);		
		
		/**
		 * @brief 析构函数
		 * 
		 * 释放所有分配的映射缓冲区资源
		 */
		virtual ~V4l2MmapDevice();

		/**
		 * @brief 初始化设备
		 * 
		 * 检查设备能力并启动设备的内存映射
		 * 
		 * @param mandatoryCapabilities 设备必须支持的能力标志
		 * @return true 初始化成功
		 * @return false 初始化失败
		 */
		virtual bool init(unsigned int mandatoryCapabilities);
		
		/**
		 * @brief 检查设备是否准备就绪
		 * 
		 * 设备就绪的条件：文件描述符有效且缓冲区已初始化
		 * 
		 * @return true 设备已准备好进行I/O操作
		 * @return false 设备未就绪
		 */
		virtual bool isReady() { return ((m_fd != -1) && (n_buffers != 0)); }
		
		/**
		 * @brief 启动视频流
		 * 
		 * 分配并映射缓冲区，将缓冲区入队，启动视频流
		 * 
		 * @return true 启动成功
		 * @return false 启动失败
		 */
		virtual bool start();
		
		/**
		 * @brief 停止视频流
		 * 
		 * 停止视频流，解除内存映射，释放缓冲区
		 * 
		 * @return true 停止成功
		 * @return false 停止失败
		 */
		virtual bool stop();
	
	protected:
		unsigned int  n_buffers;  // 已分配的缓冲区数量
	
		/**
		 * @brief 缓冲区结构，保存映射内存的信息
		 */
		struct buffer 
		{
			void *                  start;  // 映射内存的起始地址
			size_t                  length; // 映射内存的长度
		};
		buffer m_buffer[V4L2MMAP_NBBUFFER]; // 缓冲区数组
};


