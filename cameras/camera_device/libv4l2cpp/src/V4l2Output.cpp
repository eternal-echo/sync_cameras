/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose.
**
** V4l2Output.cpp
** 
** V4L2 wrapper 
**
** -------------------------------------------------------------------------*/

#include <string.h>

// libv4l2
#include <linux/videodev2.h>

// project
#include "logger.h"

#include "V4l2Output.h"
#include "V4l2MmapDevice.h"
#include "V4l2ReadWriteDevice.h"

/**
 * @brief 创建视频输出接口
 * 
 * 根据参数中指定的IO类型创建相应的V4L2视频输出设备
 * 通常用于创建虚拟摄像头、视频输出设备等
 * 
 * @param param V4L2设备参数，包含IO类型等配置
 * @return V4l2Output* 成功则返回V4l2Output实例，失败返回NULL
 */
V4l2Output* V4l2Output::create(const V4L2DeviceParameters & param)
{
	V4l2Output* videoOutput = NULL;
	V4l2Device* videoDevice = NULL; 
	int caps = V4L2_CAP_VIDEO_OUTPUT;  // 设置基本的视频输出能力标志
	switch (param.m_iotype)
	{
		case IOTYPE_MMAP: 
			// 内存映射IO模式，适合高性能视频输出
			videoDevice = new V4l2MmapDevice(param, V4L2_BUF_TYPE_VIDEO_OUTPUT); 
			caps |= V4L2_CAP_STREAMING;  // 添加流媒体能力标志
		break;
		case IOTYPE_READWRITE:
			// 读写IO模式，实现简单但效率较低
			videoDevice = new V4l2ReadWriteDevice(param, V4L2_BUF_TYPE_VIDEO_OUTPUT); 
			caps |= V4L2_CAP_READWRITE;  // 添加读写能力标志
		break;
	}
	
	// 初始化设备，检查是否支持所需功能，失败则清理并返回NULL
	if (videoDevice &&  !videoDevice->init(caps))
	{
		delete videoDevice;
		videoDevice=NULL; 
	}
	
	// 如果设备创建成功，则创建V4l2Output对象封装底层设备
	if (videoDevice)
	{
		videoOutput = new V4l2Output(videoDevice);
	}	
	return videoOutput;
}

/**
 * @brief 构造函数
 * 
 * 使用提供的V4l2Device初始化输出对象
 * 
 * @param device 已初始化的V4l2Device设备指针
 */
V4l2Output::V4l2Output(V4l2Device* device) : V4l2Access(device)
{
}

/**
 * @brief 析构函数
 * 
 * 释放资源，父类V4l2Access会负责释放设备对象
 */
V4l2Output::~V4l2Output() 
{
}

/**
 * @brief 检查设备是否可写
 * 
 * 使用select系统调用检查设备文件描述符是否可写
 * 常用于非阻塞模式，避免写入操作阻塞
 * 
 * @param tv 超时设置，NULL表示无限等待
 * @return true 设备可以接收数据
 * @return false 设备无法接收数据或发生错误
 */
bool V4l2Output::isWritable(timeval* tv)
{
	int fd = m_device->getFd();  // 获取设备文件描述符
	fd_set fdset;
	FD_ZERO(&fdset);	          // 清空文件描述符集合
	FD_SET(fd, &fdset);         // 将设备文件描述符添加到集合
	// 调用select等待设备可写，返回1表示可写
	return (select(fd+1, NULL, &fdset, NULL, tv) == 1);
}

/**
 * @brief 向设备写入数据
 * 
 * 将数据写入到V4L2设备，例如发送视频帧到虚拟摄像头
 * 
 * @param buffer 包含要写入数据的缓冲区
 * @param bufferSize 要写入的数据大小
 * @return size_t 实际写入的字节数，失败返回0
 */
size_t V4l2Output::write(char* buffer, size_t bufferSize)
{
	// 调用设备内部写入方法发送视频帧数据
	return m_device->writeInternal(buffer, bufferSize);
}

/**
 * @brief 开始部分写入操作
 * 
 * 为分段写入大数据准备缓冲区，适用于处理大型视频帧
 * 例如在发送YUV数据时，可能需要分别处理Y、U、V分量
 * 
 * @return true 成功开始部分写入
 * @return false 部分写入失败或已有写入进行中
 */
bool V4l2Output::startPartialWrite()
{
	// 调用设备内部方法开始部分写入过程，为后续数据追加做准备
	return m_device->startPartialWrite();
}

/**
 * @brief 部分写入数据
 * 
 * 向已锁定的缓冲区追加数据，必须在startPartialWrite之后调用
 * 
 * @param buffer 包含要追加数据的缓冲区
 * @param bufferSize 要追加的数据大小
 * @return size_t 实际追加的字节数
 */
size_t V4l2Output::writePartial(char* buffer, size_t bufferSize)
{
	// 调用设备内部方法追加数据到当前缓冲区
	return m_device->writePartialInternal(buffer, bufferSize);
}

/**
 * @brief 结束部分写入操作
 * 
 * 完成部分写入过程，提交数据到设备
 * 必须在所有writePartial调用结束后调用此函数
 * 
 * @return true 成功结束部分写入并提交数据
 * @return false 提交失败或无部分写入进行中
 */
bool V4l2Output::endPartialWrite()
{
	// 调用设备内部方法结束部分写入，将完整缓冲区提交到设备
	return m_device->endPartialWrite();
}

