#ifndef COLLECTOR_H
#define COLLECTOR_H
#include<map>
#include<string>
#include<queue>
#include<mutex>
#include<atomic>
#include <opencv2/opencv.hpp>
typedef bool SIGNAL;
class RTPCollector
{
public:
	RTPCollector();// 无参构造为调取本机的摄像头
	RTPCollector(const std::string& videoDir); //使用string有参构造，读取本地的视频文件
	~RTPCollector();
	bool capture_video_frame();

private:
	cv::VideoCapture cap;
	int frameWidth = 0;
	int frameHeight = 0;
	int fps = 0;

	uint32_t video_ssrc;

	bool init_capture();
	bool assemble_RTP_frame(unsigned char* data, unsigned int dataLen, unsigned short seq);
	uint32_t getCurrentTimestamp();
	uint32_t generateSSRC();
protected:
	std::queue<std::string> pkgQueue;
	std::mutex pkgQueueLock;
	std::mutex printLock;
	SIGNAL isCompleted;
};

#endif // !COLLECTOR_H
