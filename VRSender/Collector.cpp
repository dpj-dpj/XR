#include"Collector.h"
#include"rtp.h"
#include <chrono>
#include<winsock2.h>
#include <cstdlib>
#include <ctime>
#include<random>
#include<memory>

#include<iostream>
#pragma comment(lib, "ws2_32.lib")

RTPCollector::RTPCollector()//无参构造
{

	cv::VideoCapture _cap(0);// 0代表默认摄像头 "D:\\VRSender\\video\\1.mp4"
	if (!_cap.isOpened()) {
		std::cerr << "Error: Could not open video." << std::endl;
		return;
	}
	else
	{
		cap = _cap;
		init_capture();
		std::cout << "Success to open real-time video capture!" << std::endl;
	}


}

RTPCollector::RTPCollector(const std::string& videoDir)
{
	// "D:\\VRSender\\video\\1.mp4"
	cv::VideoCapture _cap(videoDir);
	if (!_cap.isOpened()) {
		std::cerr << "Error: Could not open video." << std::endl;
		return;
	}
	else
	{
		cap = _cap;
		init_capture();
		std::cout << "Success to open video capture, videoDir is " << videoDir << std::endl;
	}

}

RTPCollector::~RTPCollector()
{
	cap.release();
	cv::destroyAllWindows();
}

bool RTPCollector::init_capture() {
	frameWidth = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_WIDTH));
	frameHeight = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_HEIGHT));
	fps = static_cast<int>(cap.get(cv::CAP_PROP_FPS));
	video_ssrc = generateSSRC();
	std::cout << "frameWidth is " << frameWidth << std::endl;
	std::cout << "frameHeight is " << frameHeight << std::endl;
	std::cout << "fps is " << fps << std::endl;
	std::cout << "generated SSRC is " << video_ssrc << std::endl;
	isCompleted = false;

	if (frameWidth && frameHeight && fps)
	{
		return true;
	}
	return false;
}


bool RTPCollector::capture_video_frame() {
	//首先随机生成一个此视频流的唯一同步源 用随机数种子

	unsigned short seq = 0;

	cv::Mat frame;

	while (cap.read(frame)) {
		// 读取一帧
		//cap >> frame;
		//bool ret = cap.read(frame);
		if (frame.empty()) {
			break; // 如果没有帧可读，退出循环
		}
		if (0 == seq)//首帧里面加入12字节的头部数据宽、高、帧率
		{

		}
		// 将帧编码为 JPEG 格式并存储到 vector
		std::vector<uchar> buffer;
		cv::imencode(".jpg", frame, buffer);
		printLock.lock();
		std::cout << "frame size is: " << buffer.size() << std::endl;
		printLock.unlock();

		// 将 buffer 转换为 unsigned char*
		unsigned char* data = buffer.data();
		unsigned int dataLen = buffer.size();

		// seq 代表的发送rtp帧的数量，每次执行完加1
		assemble_RTP_frame(data, dataLen, seq++);


		// 解码图像数据
		cv::Mat decoded_frame = cv::imdecode(cv::Mat(buffer), cv::IMREAD_COLOR);
		if (decoded_frame.empty()) {
			std::cerr << "Error: Could not decode image." << std::endl;
			break;
		}

		// 显示解码后的图像
		cv::imshow("VR Video", decoded_frame);

		// 等待 1ms
		if (cv::waitKey(1) >= 0) break;
	}

	printLock.lock();
	std::cout << "Success to load video capture." << std::endl;
	printLock.unlock();
	isCompleted = true; // 输入完成，或发送端结束了发送
	return true;
}

bool RTPCollector::assemble_RTP_frame(unsigned char* data, unsigned int dataLen, unsigned short seq) {
	rtpHeader rtpHdr;
	memset(&rtpHdr, 0, sizeof(rtpHdr));
	/***| 4Bytes empty, not used | 12Bytes rtpHeader|4Bytes dataLen|4B frameWidth|4B frameHeight|4B fps|data……       	|***/
	uchar* packet = (uchar*)malloc(RTP_PACKET_HEADER_SIZE + RTP_HEADER_SIZE + sizeof(uint32_t) + dataLen); //实际的传输数据		
	//std::unique_ptr<rtpPacket> packet(static_cast<rtpPacket*>(std::malloc(RTP_PACKET_HEADER_SIZE + RTP_HEADER_SIZE + dataLen)));
	memset(packet, 0, sizeof(packet));

	rtpHdr.version = RTP_VERSION;
	rtpHdr.payloadType = RTP_PAYLOAD_TYPE_MP4V;
	rtpHdr.seq = htons(seq);
	rtpHdr.timestamp = htonl(getCurrentTimestamp());
	rtpHdr.ssrc = htonl(video_ssrc);

	//首先留着前4B 保留为0，还缺少一个表示整个长度的字段
	memset(packet, 0, RTP_PACKET_HEADER_SIZE);
	memcpy(packet + RTP_PACKET_HEADER_SIZE, &rtpHdr, RTP_HEADER_SIZE);
	memcpy(packet + RTP_PACKET_HEADER_SIZE + RTP_HEADER_SIZE, &dataLen, sizeof(uint32_t));//sizeof(uint32_t)为存储实际数据长度的地方
	memcpy(packet + RTP_PACKET_HEADER_SIZE + RTP_HEADER_SIZE + sizeof(uint32_t), data, dataLen);


	size_t sumLen = RTP_PACKET_HEADER_SIZE + RTP_HEADER_SIZE + sizeof(uint32_t) + dataLen;
	size_t offset = 0;
	size_t RTP_MTU = MTU - IPV6_HEADER_SIZE - TCP_HEADER_SIZE; //为了满足后续的需要，分装成1440B的数据包压入
	const char* pdata = (const char*)packet;
	while (offset + RTP_MTU < sumLen) //非最后一个tcp包
	{
		//std::string substr(pdata + offset, RTP_MTU);
		pkgQueueLock.lock();
		pkgQueue.push(std::string(pdata + offset, RTP_MTU));
		pkgQueueLock.unlock();
		offset += RTP_MTU;
	}
	// 处理最后一个包（pkg <= 1500-40-20 B）
	pkgQueueLock.lock();
	pkgQueue.push(std::string(pdata + offset, sumLen - offset));
	pkgQueueLock.unlock();
	free(packet);
	return true;
}


// 获取时间戳timestamp
uint32_t RTPCollector::getCurrentTimestamp() {
	// 获取当前时间点
	auto now = std::chrono::system_clock::now();

	// 将时间点转换为自纪元以来的毫秒数
	auto duration = now.time_since_epoch();

	// 转换为毫秒
	auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();

	return static_cast<uint32_t>(milliseconds);
}


// 生成随机同步源No.RAND
uint32_t RTPCollector::generateSSRC() {
	// 使用 std::random_device 作为种子
	std::random_device rd; // 随机设备
	std::mt19937 generator(rd()); // Mersenne Twister 伪随机数生成器

	// 生成随机数，范围为 0 到 UINT32_MAX
	std::uniform_int_distribution<uint32_t> distribution(0, UINT32_MAX);
	return distribution(generator);
}