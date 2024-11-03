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

RTPCollector::RTPCollector()//�޲ι���
{

	cv::VideoCapture _cap(0);// 0����Ĭ������ͷ "D:\\VRSender\\video\\1.mp4"
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
	//�����������һ������Ƶ����Ψһͬ��Դ �����������

	unsigned short seq = 0;

	cv::Mat frame;

	while (cap.read(frame)) {
		// ��ȡһ֡
		//cap >> frame;
		//bool ret = cap.read(frame);
		if (frame.empty()) {
			break; // ���û��֡�ɶ����˳�ѭ��
		}
		if (0 == seq)//��֡�������12�ֽڵ�ͷ�����ݿ��ߡ�֡��
		{

		}
		// ��֡����Ϊ JPEG ��ʽ���洢�� vector
		std::vector<uchar> buffer;
		cv::imencode(".jpg", frame, buffer);
		printLock.lock();
		std::cout << "frame size is: " << buffer.size() << std::endl;
		printLock.unlock();

		// �� buffer ת��Ϊ unsigned char*
		unsigned char* data = buffer.data();
		unsigned int dataLen = buffer.size();

		// seq ����ķ���rtp֡��������ÿ��ִ�����1
		assemble_RTP_frame(data, dataLen, seq++);


		// ����ͼ������
		cv::Mat decoded_frame = cv::imdecode(cv::Mat(buffer), cv::IMREAD_COLOR);
		if (decoded_frame.empty()) {
			std::cerr << "Error: Could not decode image." << std::endl;
			break;
		}

		// ��ʾ������ͼ��
		cv::imshow("VR Video", decoded_frame);

		// �ȴ� 1ms
		if (cv::waitKey(1) >= 0) break;
	}

	printLock.lock();
	std::cout << "Success to load video capture." << std::endl;
	printLock.unlock();
	isCompleted = true; // ������ɣ����Ͷ˽����˷���
	return true;
}

bool RTPCollector::assemble_RTP_frame(unsigned char* data, unsigned int dataLen, unsigned short seq) {
	rtpHeader rtpHdr;
	memset(&rtpHdr, 0, sizeof(rtpHdr));
	/***| 4Bytes empty, not used | 12Bytes rtpHeader|4Bytes dataLen|4B frameWidth|4B frameHeight|4B fps|data����       	|***/
	uchar* packet = (uchar*)malloc(RTP_PACKET_HEADER_SIZE + RTP_HEADER_SIZE + sizeof(uint32_t) + dataLen); //ʵ�ʵĴ�������		
	//std::unique_ptr<rtpPacket> packet(static_cast<rtpPacket*>(std::malloc(RTP_PACKET_HEADER_SIZE + RTP_HEADER_SIZE + dataLen)));
	memset(packet, 0, sizeof(packet));

	rtpHdr.version = RTP_VERSION;
	rtpHdr.payloadType = RTP_PAYLOAD_TYPE_MP4V;
	rtpHdr.seq = htons(seq);
	rtpHdr.timestamp = htonl(getCurrentTimestamp());
	rtpHdr.ssrc = htonl(video_ssrc);

	//��������ǰ4B ����Ϊ0����ȱ��һ����ʾ�������ȵ��ֶ�
	memset(packet, 0, RTP_PACKET_HEADER_SIZE);
	memcpy(packet + RTP_PACKET_HEADER_SIZE, &rtpHdr, RTP_HEADER_SIZE);
	memcpy(packet + RTP_PACKET_HEADER_SIZE + RTP_HEADER_SIZE, &dataLen, sizeof(uint32_t));//sizeof(uint32_t)Ϊ�洢ʵ�����ݳ��ȵĵط�
	memcpy(packet + RTP_PACKET_HEADER_SIZE + RTP_HEADER_SIZE + sizeof(uint32_t), data, dataLen);


	size_t sumLen = RTP_PACKET_HEADER_SIZE + RTP_HEADER_SIZE + sizeof(uint32_t) + dataLen;
	size_t offset = 0;
	size_t RTP_MTU = MTU - IPV6_HEADER_SIZE - TCP_HEADER_SIZE; //Ϊ�������������Ҫ����װ��1440B�����ݰ�ѹ��
	const char* pdata = (const char*)packet;
	while (offset + RTP_MTU < sumLen) //�����һ��tcp��
	{
		//std::string substr(pdata + offset, RTP_MTU);
		pkgQueueLock.lock();
		pkgQueue.push(std::string(pdata + offset, RTP_MTU));
		pkgQueueLock.unlock();
		offset += RTP_MTU;
	}
	// �������һ������pkg <= 1500-40-20 B��
	pkgQueueLock.lock();
	pkgQueue.push(std::string(pdata + offset, sumLen - offset));
	pkgQueueLock.unlock();
	free(packet);
	return true;
}


// ��ȡʱ���timestamp
uint32_t RTPCollector::getCurrentTimestamp() {
	// ��ȡ��ǰʱ���
	auto now = std::chrono::system_clock::now();

	// ��ʱ���ת��Ϊ�Լ�Ԫ�����ĺ�����
	auto duration = now.time_since_epoch();

	// ת��Ϊ����
	auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();

	return static_cast<uint32_t>(milliseconds);
}


// �������ͬ��ԴNo.RAND
uint32_t RTPCollector::generateSSRC() {
	// ʹ�� std::random_device ��Ϊ����
	std::random_device rd; // ����豸
	std::mt19937 generator(rd()); // Mersenne Twister α�����������

	// �������������ΧΪ 0 �� UINT32_MAX
	std::uniform_int_distribution<uint32_t> distribution(0, UINT32_MAX);
	return distribution(generator);
}