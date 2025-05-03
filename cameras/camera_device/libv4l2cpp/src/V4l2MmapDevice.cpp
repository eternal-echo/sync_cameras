/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose.
**
** V4l2MmapDevice.cpp
** 
** V4L2 source using mmap API
**
** -------------------------------------------------------------------------*/

#include <string.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h> 
#include <sys/mman.h>
#include <sys/ioctl.h>

// libv4l2
#include <linux/videodev2.h>

// project
#include "logger.h"
#include "V4l2MmapDevice.h"

/**
 * @brief 构造函数
 * 
 * 初始化缓冲区计数和清空缓冲区数组
 * 
 * @param params 设备参数，包含设备路径等信息
 * @param deviceType 设备类型，如视频捕获、输出等
 */
V4l2MmapDevice::V4l2MmapDevice(const V4L2DeviceParameters & params, v4l2_buf_type deviceType) : V4l2Device(params, deviceType), n_buffers(0) 
{
	// 初始化缓冲区数组为全0
	memset(&m_buffer, 0, sizeof(m_buffer));
}

/**
 * @brief 初始化设备
 * 
 * 首先检查设备是否支持所需功能，然后启动设备
 * 
 * @param mandatoryCapabilities 设备必须支持的能力标志
 * @return true 初始化成功
 * @return false 初始化失败
 */
bool V4l2MmapDevice::init(unsigned int mandatoryCapabilities)
{
	// 调用父类初始化方法进行基础设备初始化
	bool ret = V4l2Device::init(mandatoryCapabilities);
	if (ret)
	{
		// 如果基础初始化成功，则执行内存映射启动流程
		ret = this->start();
	}
	return ret;
}

/**
 * @brief 析构函数
 * 
 * 停止设备并释放所有资源
 */
V4l2MmapDevice::~V4l2MmapDevice()
{
	// 停止设备并清理资源
	this->stop();
}

/**
 * @brief 启动视频流
 * 
 * 请求并映射缓冲区，将缓冲区入队，启动视频流
 * 
 * @return true 启动成功
 * @return false 启动失败
 */
bool V4l2MmapDevice::start() 
{
	LOG(INFO) << "Device " << m_params.m_devName;

	bool success = true;
	struct v4l2_requestbuffers req;
	memset (&req, 0, sizeof(req));
	
	// 请求分配内存映射缓冲区
	req.count               = V4L2MMAP_NBBUFFER; // 请求的缓冲区数量
	req.type                = m_deviceType;      // 缓冲区类型（捕获或输出）
	req.memory              = V4L2_MEMORY_MMAP;  // 使用内存映射方式

	// 向驱动请求分配缓冲区
	if (-1 == ioctl(m_fd, VIDIOC_REQBUFS, &req)) 
	{
		if (EINVAL == errno) 
		{
			// 设备不支持内存映射
			LOG(ERROR) << "Device " << m_params.m_devName << " does not support memory mapping";
			success = false;
		} 
		else 
		{
			perror("VIDIOC_REQBUFS");
			success = false;
		}
	}
	else
	{
		LOG(INFO) << "Device " << m_params.m_devName << " nb buffer:" << req.count;
		
		 // 分配并映射缓冲区
		memset(&m_buffer,0, sizeof(m_buffer));
		for (n_buffers = 0; n_buffers < req.count; ++n_buffers) 
		{
			struct v4l2_buffer buf;
			memset (&buf, 0, sizeof(buf));
			buf.type        = m_deviceType;
			buf.memory      = V4L2_MEMORY_MMAP;
			buf.index       = n_buffers;

			// 查询缓冲区信息（大小和偏移量）
			if (-1 == ioctl(m_fd, VIDIOC_QUERYBUF, &buf))
			{
				perror("VIDIOC_QUERYBUF");
				success = false;
			}
			else
			{
				LOG(INFO) << "Device " << m_params.m_devName << " buffer idx:" << n_buffers << " size:" << buf.length << " offset:" << buf.m.offset;
				
				// 保存缓冲区长度
				m_buffer[n_buffers].length = buf.length;
				if (!m_buffer[n_buffers].length) {
					m_buffer[n_buffers].length = buf.bytesused;
				}
				
				// 将内核空间的缓冲区映射到用户空间
				m_buffer[n_buffers].start = mmap (   NULL /* start anywhere */, 
											m_buffer[n_buffers].length, 
											PROT_READ | PROT_WRITE /* required */, 
											MAP_SHARED /* recommended */, 
											m_fd, 
											buf.m.offset);

				// 检查映射是否成功
				if (MAP_FAILED == m_buffer[n_buffers].start)
				{
					perror("mmap");
					success = false;
				}
			}
		}

		// 将所有缓冲区入队，准备接收/发送数据
		for (unsigned int i = 0; i < n_buffers; ++i) 
		{
			struct v4l2_buffer buf;
			memset (&buf, 0, sizeof(buf));
			buf.type        = m_deviceType;
			buf.memory      = V4L2_MEMORY_MMAP;
			buf.index       = i;

			// 将缓冲区放入驱动队列
			if (-1 == ioctl(m_fd, VIDIOC_QBUF, &buf))
			{
				perror("VIDIOC_QBUF");
				success = false;
			}
		}

		// 启动视频流
		int type = m_deviceType;
		if (-1 == ioctl(m_fd, VIDIOC_STREAMON, &type))
		{
			perror("VIDIOC_STREAMON");
			success = false;
		}
	}
	return success; 
}

/**
 * @brief 停止视频流
 * 
 * 停止设备流，解除内存映射，释放缓冲区
 * 
 * @return true 停止成功
 * @return false 停止失败
 */
bool V4l2MmapDevice::stop() 
{
	LOG(INFO) << "Device " << m_params.m_devName;

	bool success = true;
	
	// 停止视频流
	int type = m_deviceType;
	if (-1 == ioctl(m_fd, VIDIOC_STREAMOFF, &type))
	{
		perror("VIDIOC_STREAMOFF");      
		success = false;
	}

	// 解除所有缓冲区的内存映射
	for (unsigned int i = 0; i < n_buffers; ++i)
	{
		if (-1 == munmap (m_buffer[i].start, m_buffer[i].length))
		{
			perror("munmap");
			success = false;
		}
	}
	
	// 释放所有缓冲区
	struct v4l2_requestbuffers req;
	memset (&req, 0, sizeof(req));
	req.count               = 0; // 请求0个缓冲区表示释放所有缓冲区
	req.type                = m_deviceType;
	req.memory              = V4L2_MEMORY_MMAP;
	
	// 向驱动请求释放缓冲区
	if (-1 == ioctl(m_fd, VIDIOC_REQBUFS, &req)) 
	{
		perror("VIDIOC_REQBUFS");
		success = false;
	}
	
	// 重置缓冲区计数
	n_buffers = 0;
	return success; 
}

/**
 * @brief 从设备读取数据
 * 
 * 从队列中取出一个已填充的缓冲区，复制数据后将缓冲区重新入队
 * 
 * @param buffer 目标缓冲区，用于存储读取的数据
 * @param bufferSize 目标缓冲区的大小
 * @return size_t 实际读取的字节数，0表示无数据，-1表示出错
 */
size_t V4l2MmapDevice::readInternal(char* buffer, size_t bufferSize)
{
	size_t size = 0;
	if (n_buffers > 0)
	{
		struct v4l2_buffer buf;	
		memset (&buf, 0, sizeof(buf));
		buf.type = m_deviceType;
		buf.memory = V4L2_MEMORY_MMAP;

		// 从队列中取出一个已填充的缓冲区
		if (-1 == ioctl(m_fd, VIDIOC_DQBUF, &buf)) 
		{
			if (errno == EAGAIN) {
				// 非阻塞模式下没有数据可读
				size = 0;
			} else {
				perror("VIDIOC_DQBUF");
				size = -1;
			}
		}
		else if (buf.index < n_buffers)
		{
			// 获取数据大小，并确保不超出目标缓冲区大小
			size = buf.bytesused;
			if (size > bufferSize)
			{
				size = bufferSize;
				LOG(WARN) << "Device " << m_params.m_devName << " buffer truncated available:" << bufferSize << " needed:" << buf.bytesused;
			}
			
			// 复制数据到目标缓冲区
			memcpy(buffer, m_buffer[buf.index].start, size);

			// 将处理完的缓冲区重新入队，以便重用
			if (-1 == ioctl(m_fd, VIDIOC_QBUF, &buf))
			{
				perror("VIDIOC_QBUF");
				size = -1;
			}
		}
	}
	return size;
}

/**
 * @brief 向设备写入数据
 * 
 * 从队列中取出一个空缓冲区，填充数据后将缓冲区重新入队
 * 
 * @param buffer 源数据缓冲区
 * @param bufferSize 源数据大小
 * @return size_t 实际写入的字节数，-1表示出错
 */
size_t V4l2MmapDevice::writeInternal(char* buffer, size_t bufferSize)
{
	size_t size = 0;
	if (n_buffers > 0)
	{
		struct v4l2_buffer buf;	
		memset (&buf, 0, sizeof(buf));
		buf.type = m_deviceType;
		buf.memory = V4L2_MEMORY_MMAP;

		// 从队列中取出一个空缓冲区
		if (-1 == ioctl(m_fd, VIDIOC_DQBUF, &buf)) 
		{
			perror("VIDIOC_DQBUF");
			size = -1;
		}
		else if (buf.index < n_buffers)
		{
			// 确定写入大小，确保不超出缓冲区容量
			size = bufferSize;
			if (size > buf.length)
			{
				LOG(WARN) << "Device " << m_params.m_devName << " buffer truncated available:" << buf.length << " needed:" << size;
				size = buf.length;
			}
			
			// 将数据复制到缓冲区
			memcpy(m_buffer[buf.index].start, buffer, size);
			buf.bytesused = size; // 设置已使用的字节数

			// 将填充好的缓冲区重新入队，准备发送
			if (-1 == ioctl(m_fd, VIDIOC_QBUF, &buf))
			{
				perror("VIDIOC_QBUF");
				size = -1;
			}
		}
	}
	return size;
}

/**
 * @brief 开始部分写入操作
 * 
 * 为大数据分段写入准备环境，获取一个缓冲区并锁定
 * 
 * @return true 成功开始部分写入
 * @return false 开始失败或已有部分写入操作进行中
 */
bool V4l2MmapDevice::startPartialWrite()
{
	// 检查是否有可用缓冲区
	if (n_buffers <= 0)
		return false;
	
	// 确保没有其他部分写入操作正在进行
	if (m_partialWriteInProgress)
		return false;
	
	// 清空部分写入缓冲区信息
	memset(&m_partialWriteBuf, 0, sizeof(m_partialWriteBuf));
	m_partialWriteBuf.type = m_deviceType;
	m_partialWriteBuf.memory = V4L2_MEMORY_MMAP;
	
	// 从队列中取出一个空缓冲区
	if (-1 == ioctl(m_fd, VIDIOC_DQBUF, &m_partialWriteBuf))
	{
		perror("VIDIOC_DQBUF");
		return false;
	}
	
	// 初始化已使用字节数为0
	m_partialWriteBuf.bytesused = 0;
	
	// 设置部分写入标志
	m_partialWriteInProgress = true;
	return true;
}

/**
 * @brief 部分写入数据（为了支持那些需要接收数据的V4L2设备，比如虚拟摄像头、硬件编码器等)
 * 
 * 向当前锁定的缓冲区追加数据，用于分段传输大数据
 * 
 * @param buffer 源数据缓冲区
 * @param bufferSize 源数据大小
 * @return size_t 实际追加的字节数
 */
size_t V4l2MmapDevice::writePartialInternal(char* buffer, size_t bufferSize)
{
	size_t new_size = 0;
	size_t size = 0;
	
	// 检查是否有部分写入操作正在进行且有可用缓冲区
	if ((n_buffers > 0) && m_partialWriteInProgress)
	{
		if (m_partialWriteBuf.index < n_buffers)
		{
			// 计算追加数据后的总大小
			new_size = m_partialWriteBuf.bytesused + bufferSize;
			
			// 确保不超出缓冲区容量
			if (new_size > m_partialWriteBuf.length)
			{
				LOG(WARN) << "Device " << m_params.m_devName << " buffer truncated available:" << m_partialWriteBuf.length << " needed:" << new_size;
				new_size = m_partialWriteBuf.length;
			}
			
			// 计算实际可写入的数据量
			size = new_size - m_partialWriteBuf.bytesused;
			
			// 将数据追加到已有数据之后
			memcpy(&((char *)m_buffer[m_partialWriteBuf.index].start)[m_partialWriteBuf.bytesused], buffer, size);

			// 更新已使用字节计数
			m_partialWriteBuf.bytesused += size;
		}
	}
	return size;
}

/**
 * @brief 结束部分写入操作
 * 
 * 将已完成的部分写入缓冲区提交到队列
 * 
 * @return true 成功结束部分写入
 * @return false 结束失败或无部分写入操作进行中
 */
bool V4l2MmapDevice::endPartialWrite()
{
	// 检查是否有部分写入操作正在进行
	if (!m_partialWriteInProgress)
		return false;
	
	// 检查是否有可用缓冲区
	if (n_buffers <= 0)
	{
		// 强制结束部分写入状态，放弃当前操作
		m_partialWriteInProgress = false;
		return true;
	}
	
	// 将填充好的缓冲区重新入队，准备发送
	if (-1 == ioctl(m_fd, VIDIOC_QBUF, &m_partialWriteBuf))
	{
		perror("VIDIOC_QBUF");
		// 强制结束部分写入状态，放弃当前操作
		m_partialWriteInProgress = false;
		return true;
	}
	
	// 重置部分写入标志
	m_partialWriteInProgress = false;
	return true;
}
