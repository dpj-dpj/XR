#ifndef SENDER_H
#define SENDER_H


#include<winsock2.h>
#include<string>
#include<chrono>
#include"rtp.h"
#include"Collector.h"

class IPv6TCPSender :public RTPCollector
{
public:
	IPv6TCPSender(const std::string xml);//Sender有参构造，Collector无参构造
	IPv6TCPSender(const std::string xml, const std::string videoDir); //Sender有参构造，Collector有参构造
	~IPv6TCPSender();

	bool initialize_socket();
	void send_data(const char* data, size_t data_len);
	bool establish_connection();  // 三次握手
	bool terminate_connection();  // 四次挥手
	void send_ipv6_tcp_rtp_packet();
	void process_ack_data();
	bool check_timeout_packet();
	void close_socket();
private:
	const std::string xml;// xml文件路径
	SOCKET sockfd;
	const char* src_ip;
	const char* dest_ip;
	unsigned short src_port;
	unsigned short dest_port;

	unsigned int global_seq_num;
	unsigned int ack_num;
	unsigned int received_seq_num;
	unsigned short window_size;
	//std::map<unsigned int, std::string> send_buffer;
	std::chrono::milliseconds timeout;
	std::map<unsigned int, std::chrono::steady_clock::time_point> timers;
	// std::queue<HologramPacket*> packetQue;
	// 
	std::map<unsigned int, std::string> send_buffer;
	std::mutex sendBufferLock;
	std::string trim(const std::string& str);
	bool init_param();//
	unsigned short checksum(void* vdata, size_t length);
	bool receive_ack();
	void send_tcp_segment(unsigned int seq_num, unsigned int ack_num, unsigned char flags, const char* data, size_t data_len);
	void retransmit(unsigned int seq_num);
};

#endif // !SENDER_H


