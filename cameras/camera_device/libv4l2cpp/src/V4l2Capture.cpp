/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose.
**
** V4l2Capture.cpp
** 
** V4L2 wrapper 
**
** -------------------------------------------------------------------------*/


// libv4l2
#include <linux/videodev2.h>

// project
#include "logger.h"
#include "V4l2Capture.h"
#include "V4l2MmapDevice.h"
#include "V4l2ReadWriteDevice.h"


/**
 * @brief 创建视频捕获接口
 * 
 * 根据参数中指定的IO类型创建相应的V4L2视频捕获设备
 * 
 * @param param V4L2设备参数，包含IO类型等配置
 * @return V4l2Capture* 成功则返回V4l2Capture实例，失败返回NULL
 */
V4l2Capture* V4l2Capture::create(const V4L2DeviceParameters & param)
{
	V4l2Capture* videoCapture = NULL;
	V4l2Device* videoDevice = NULL; 
	int caps = V4L2_CAP_VIDEO_CAPTURE;  // 设置基本的视频捕获能力标志
	switch (param.m_iotype)
	{
		case IOTYPE_MMAP: 
			// 内存映射IO模式，更高效，减少数据复制
			videoDevice = new V4l2MmapDevice(param, V4L2_BUF_TYPE_VIDEO_CAPTURE); 
			caps |= V4L2_CAP_STREAMING;  // 添加流媒体能力标志
		break;
		case IOTYPE_READWRITE:
			// 读写IO模式，实现更简单，但效率较低
			videoDevice = new V4l2ReadWriteDevice(param, V4L2_BUF_TYPE_VIDEO_CAPTURE); 
			caps |= V4L2_CAP_READWRITE;  // 添加读写能力标志
		break;
	}
	
	// 初始化设备，如果失败则清理并返回NULL
	if (videoDevice &&  !videoDevice->init(caps))
	{
		delete videoDevice;
		videoDevice=NULL; 
	}
	
	// 如果设备创建成功，则创建V4l2Capture对象
	if (videoDevice)
	{
		videoCapture = new V4l2Capture(videoDevice);
	}	
	return videoCapture;
}

/**
 * @brief 构造函数
 * 
 * 使用提供的V4l2Device初始化捕获对象
 * 
 * @param device 已初始化的V4l2Device设备指针
 */
V4l2Capture::V4l2Capture(V4l2Device* device) : V4l2Access(device)
{
}

/**
 * @brief 析构函数
 * 
 * 释放资源，父类V4l2Access会负责释放设备对象
 */
V4l2Capture::~V4l2Capture() 
{
}

/**
 * @brief 检查设备是否有数据可读
 * 
 * 使用select系统调用检查设备文件描述符是否可读
 * 
 * @param tv 超时设置，NULL表示无限等待
 * @return true 设备有数据可读
 * @return false 设备无数据可读或发生错误
 */
bool V4l2Capture::isReadable(timeval* tv)
{
	int fd = m_device->getFd();  // 获取设备文件描述符
	fd_set fdset;
	FD_ZERO(&fdset);	          // 清空文件描述符集合
	FD_SET(fd, &fdset);         // 将设备文件描述符添加到集合
	// 调用select等待设备可读，返回1表示可读
	return (select(fd+1, &fdset, NULL, NULL, tv) == 1);
}

/**
 * @brief 从设备读取数据
 * 
 * 封装底层设备的读取操作，将数据读入提供的缓冲区
 * 
 * @param buffer 目标缓冲区指针
 * @param bufferSize 目标缓冲区大小
 * @return size_t 实际读取的字节数，失败返回0
 */
size_t V4l2Capture::read(char* buffer, size_t bufferSize)
{
	// 调用设备内部读取方法获取视频帧数据
	return m_device->readInternal(buffer, bufferSize);
}


