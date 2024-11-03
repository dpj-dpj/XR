#include"Sender.h"
#include "tinyxml2.h"
#include"ipv6tcp.h"
#include<iostream>
#include<cstring>
#include <chrono>

IPv6TCPSender::IPv6TCPSender(const std::string _xml)
	:xml(_xml)
{
	if (!init_param()) {
		std::cerr << "init xml file failed (real-time video)" << std::endl;
		return;
	}
	std::cerr << "success to init xml file" << std::endl;
}

IPv6TCPSender::IPv6TCPSender(const std::string _xml, const std::string _videoDir)
	: xml(_xml), RTPCollector(_videoDir)
{
	if (!init_param()) {
		std::cerr << "init xml file failed(video dir)" << _videoDir << std::endl;
		return;
	}
	std::cerr << "success to init xml file" << std::endl;
}

IPv6TCPSender::~IPv6TCPSender()
{
	WSACleanup();
	close_socket();
	if (src_ip) {
		free((void*)src_ip);
	}
	if (dest_ip) {
		free((void*)dest_ip);
	}
}

bool IPv6TCPSender::init_param() {

	tinyxml2::XMLDocument doc;

	if (doc.LoadFile(xml.c_str()) != 0) {
		std::cerr << "load file failed" << std::endl;
		return false;
	}
	tinyxml2::XMLElement* root = doc.RootElement();

	tinyxml2::XMLElement* socket_child;

	socket_child = root->FirstChildElement("srcIp");
	if (socket_child && socket_child->GetText()) {
		std::string temp = trim(socket_child->GetText());
		src_ip = _strdup(temp.c_str()); // 转换为const char*并复制字符串
	}
	else {
		std::cerr << "Failed to get srcIp from XML" << std::endl;
	}

	socket_child = root->FirstChildElement("destIp");
	if (socket_child && socket_child->GetText()) {
		std::string temp = trim(socket_child->GetText());
		dest_ip = _strdup(temp.c_str()); // 转换为const char*并复制字符串
	}
	else {
		std::cerr << "Failed to get destIp from XML" << std::endl;
	}

	socket_child = root->FirstChildElement("srcPort");
	if (socket_child && socket_child->GetText()) {
		src_port = static_cast<unsigned short>(std::stoi(socket_child->GetText()));
	}
	else {
		std::cerr << "Failed to get srcPort from XML" << std::endl;
	}

	socket_child = root->FirstChildElement("destPort");
	if (socket_child && socket_child->GetText()) {
		dest_port = static_cast<unsigned short>(std::stoi(socket_child->GetText()));
	}
	else {
		std::cerr << "Failed to get destPort from XML" << std::endl;
	}

	// 初始化Winsock
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
		std::cerr << "WSAStartup failed" << std::endl;
		exit(EXIT_FAILURE);
	}
	//给tcp重传时间赋值 1050ms的时间
	timeout = std::chrono::milliseconds(1050);
	initialize_socket();
	return true;
}

std::string IPv6TCPSender::trim(const std::string& str) {
	size_t first = str.find_first_not_of(' ');
	if (first == std::string::npos) return "";
	size_t last = str.find_last_not_of(' ');
	return str.substr(first, last - first + 1);
}

bool IPv6TCPSender::initialize_socket() {
	if ((sockfd = socket(AF_INET6, SOCK_RAW, IPPROTO_RAW)) == INVALID_SOCKET) {
		perror("Socket creation failed");
		return false;
	}

	int optval = 0;//需要加上头部
	if (setsockopt(sockfd, IPPROTO_IPV6, IPV6_HDRINCL, (char*)&optval, sizeof(optval)) < 0)
	{
		int err = WSAGetLastError();
		printf("Error code: %d\n", err);
		std::cerr << "Failed to set socket option" << std::endl;
		return false;
	}
	else
	{
		std::cout << "Success to set socket option no-header." << std::endl;
	}

	optval = 1;
	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (char*)&optval, sizeof(optval)) < 0)
	{
		int err = WSAGetLastError();
		printf("Error code: %d\n", err);
		std::cerr << "Failed to set socket option" << std::endl;
		// close_socket();
		return false;
	}
	else
	{
		std::cout << "Success to set socket option SO_REUSEADDR." << std::endl;
	}



	//绑定socket的源ip和端口号
	struct sockaddr_in6 addr = {};
	addr.sin6_family = AF_INET6;
	addr.sin6_port = htons(src_port);
	addr.sin6_scope_id = 12;//后续需要修改
	if (inet_pton(AF_INET6, src_ip, &addr.sin6_addr) != 1) {
		std::cerr << "Invalid IP address: " << src_ip << std::endl;
		return false;
	}
	std::cout << "Binding to IP: " << src_ip << ", Port: " << src_port << std::endl;
	if (bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
		std::cerr << "Bind failed: " << WSAGetLastError() << std::endl;
		return false;
	}

	return true;
}

void IPv6TCPSender::close_socket() {
	closesocket(sockfd);
}

unsigned short IPv6TCPSender::checksum(void* vdata, size_t length) {
	char* data = (char*)vdata;
	unsigned long acc = 0xffff;

	for (size_t i = 0; i + 1 < length; i += 2) {
		unsigned short word;
		memcpy(&word, data + i, 2);
		acc += ntohs(word);
		if (acc > 0xffff) {
			acc -= 0xffff;
		}
	}

	if (length & 1) {
		unsigned short word = 0;
		memcpy(&word, data + length - 1, 1);
		acc += ntohs(word);
		if (acc > 0xffff) {
			acc -= 0xffff;
		}
	}

	return htons(~acc);
}

void IPv6TCPSender::send_tcp_segment(unsigned int seq_num, unsigned int ack_num, unsigned char flags, const char* data, size_t data_len) {
	char packet[MTU];
	memset(packet, 0, sizeof(packet));

	// IPv6头部
	ipv6_header ipv6_hdr;
	ipv6_hdr.ver_tc_fl = htonl((6 << 28) | (0 << 20) | 0); // 版本6，流量类0，流标签0
	ipv6_hdr.payload_len = htons(TCP_HDRLEN + data_len); // 负载长度
	//std::cout << "payload_len:" << htons(ipv6_hdr.payload_len) << std::endl;
	ipv6_hdr.next_header = IPPROTO_TCP; // 下一个头部类型  IPPROTO_TCP
	ipv6_hdr.hop_limit = 255; // 跳数限制
	inet_pton(AF_INET6, src_ip, &ipv6_hdr.src_addr);
	inet_pton(AF_INET6, dest_ip, &ipv6_hdr.dest_addr);
	//std::cout << "src_ip: " << src_ip << "dest_ip: " << dest_ip << std::endl;
	// TCP头部
	tcp_header tcp_hdr;
	tcp_hdr.src_port = htons(src_port);
	tcp_hdr.dest_port = htons(dest_port);
	tcp_hdr.seq_num = htonl(seq_num); // 使用传入的序列号
	tcp_hdr.ack_num = htonl(ack_num);
	tcp_hdr.data_offset = (TCP_HDRLEN / 4) << 4;
	tcp_hdr.flags = flags;
	tcp_hdr.window = htons(65535);
	tcp_hdr.checksum = 0;
	tcp_hdr.urgent_pointer = 0;

	// 动态分配内存以计算TCP校验和
	size_t pseudo_header_size = sizeof(struct in6_addr) * 2 + sizeof(unsigned long) + sizeof(unsigned short) + TCP_HDRLEN + data_len;
	char* pseudo_header = new char[pseudo_header_size];
	memset(pseudo_header, 0, pseudo_header_size);

	// 填充伪头部
	memcpy(pseudo_header, &ipv6_hdr.src_addr, sizeof(struct in6_addr));
	memcpy(pseudo_header + sizeof(struct in6_addr), &ipv6_hdr.dest_addr, sizeof(struct in6_addr));
	unsigned long chunk_len_network = htonl(TCP_HDRLEN + data_len);
	memcpy(pseudo_header + sizeof(struct in6_addr) * 2, &chunk_len_network, sizeof(unsigned long));
	unsigned short zeros_and_proto = htons((unsigned short)IPPROTO_TCP);
	memcpy(pseudo_header + sizeof(struct in6_addr) * 2 + sizeof(unsigned long), &zeros_and_proto, sizeof(unsigned short));
	memcpy(pseudo_header + sizeof(struct in6_addr) * 2 + sizeof(unsigned long) + sizeof(unsigned short), &tcp_hdr, TCP_HDRLEN);
	if (data_len > 0) {
		memcpy(pseudo_header + sizeof(struct in6_addr) * 2 + sizeof(unsigned long) + sizeof(unsigned short) + TCP_HDRLEN, data, data_len);
	}

	tcp_hdr.checksum = checksum(pseudo_header, pseudo_header_size);

	// 复制IPv6头部到包中
	memcpy(packet, &ipv6_hdr, IPV6_HDRLEN);
	// 复制TCP头部到包中
	memcpy(packet + IPV6_HDRLEN, &tcp_hdr, TCP_HDRLEN);
	// 复制负载数据到包中这个数据包中是有rtp头部的信息的
	if (data_len > 0) {
		memcpy(packet + IPV6_HDRLEN + TCP_HDRLEN, data, data_len);
	}

	delete[] pseudo_header;

	struct sockaddr_in6 dest = {};
	memset(&dest, 0, sizeof(dest));
	dest.sin6_family = AF_INET6;
	//dest.sin6_port = htons(dest_port);


	/// 查表操作
	/// 真正发的时候，应当查询路由表决定交付给谁
	const char* next_ip = "fe80::2ecf:67ff:fe29:d2f8";//查询路由表所得
	/// 路由信息


	inet_pton(AF_INET6, next_ip, &dest.sin6_addr);
	if (sendto(sockfd, packet, IPV6_HDRLEN + TCP_HDRLEN + data_len, 0, (struct sockaddr*)&dest, sizeof(dest)) == SOCKET_ERROR) {
		std::cerr << "send err: " << WSAGetLastError() << std::endl;
	}
	else {
		printLock.lock();
		std::cout << "Packet sent: seq_num=" << seq_num << ", ack_num=" << ack_num << ", flags=" << static_cast<int>(flags) << std::endl;
		printLock.unlock();
		if (flags & 0x18) { // PSH or SYN
			timers[seq_num] = std::chrono::steady_clock::now();
		}
	}
}

//作为子函数，不可当作进程
bool IPv6TCPSender::receive_ack() {


	char buffer[MTU];
	struct sockaddr_in6 src_addr;
	int src_addr_len = sizeof(src_addr);

	if (recvfrom(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr*)&src_addr, &src_addr_len) != SOCKET_ERROR) {
		struct tcp_header* tcp_hdr = (struct tcp_header*)(buffer + IPV6_HDRLEN);
		unsigned int received_ack_num = ntohl(tcp_hdr->ack_num); // 获取接收端的ack号
		received_seq_num = ntohl(tcp_hdr->seq_num); // 获取接收端的序列号
		window_size = ntohs(tcp_hdr->window); //更新发送窗口大小(增加拥塞窗口，二者取最小)

		auto it = send_buffer.begin();
		while (it != send_buffer.end()) {
			if (it->first + it->second.size() < received_ack_num) {
				// 确认序列号范围内的数据
				it = send_buffer.erase(it); // 移除已确认的数据
			}
			else {
				++it;
			}
		}

		printLock.lock();
		std::cout << "ACK received: ack_num=" << received_ack_num << ", seq_num=" << received_seq_num << std::endl;
		printLock.unlock();

		timers.erase(received_ack_num); // 移除定时器

		return true;

	}

	return false;
}

void IPv6TCPSender::process_ack_data() {
	while (!isCompleted||!send_buffer.empty())
	{// 未完成传输工作 或者 缓冲区里还有未得到确认的报文
		receive_ack();
	}
}
bool IPv6TCPSender::check_timeout_packet() {

	while (!isCompleted || !timers.empty())
	{

		// 检查超时的包并重传
		for (auto it = timers.begin(); it != timers.end(); ) {
			// 计算时间差并检查超时
			auto now = std::chrono::steady_clock::now();
			if (std::chrono::duration_cast<std::chrono::milliseconds>(now - it->second).count() >= timeout.count()) {
				std::cerr << "Timeout for seq_num=" << it->first << ", retransmitting..." << std::endl;
				retransmit(it->first);
				it->second = now; // 重置定时器
				// 不前进迭代器，因为我们更新了当前元素
			}
			else {
				++it; // 只在没有超时的情况下前进
			}
		}

		//固定时间检查一次
		std::this_thread::sleep_for(std::chrono::milliseconds(2));
	}

	return true;
}

void IPv6TCPSender::retransmit(unsigned int seq_num) {
	if (send_buffer.find(seq_num) != send_buffer.end()) {

		sendBufferLock.lock();
		std::string data = send_buffer[seq_num];
		sendBufferLock.unlock();

		send_tcp_segment(seq_num, ack_num, 0x18, data.data(), data.size());
	}
}

void IPv6TCPSender::send_data(const char* data, size_t data_len) {

	send_buffer[global_seq_num] = std::string(data, data_len);//每发一个都会写入缓冲区

	ack_num = received_seq_num + 1;
	send_tcp_segment(global_seq_num, ack_num, 0x18, data, data_len);
	global_seq_num += data_len;// 更新 global_seq_num

	//std::this_thread::sleep_for(std::chrono::milliseconds(1)); // 模拟发送间隔
	//size_t offset = 0;
	//while (offset < data_len) {
	//	// 检查窗口是否已满，满了就得等ACK
	//	while (send_buffer.size() >= window_size) { //满了就得阻塞
	//		if (!receive_ack()) {
	//			return; // 发生错误
	//		}//阻塞等待
	//	}
	//	size_t chunk_size = std::min(static_cast<size_t>(MTU - (IPV6_HDRLEN + TCP_HDRLEN)), data_len - offset);
	//	std::string chunk(data + offset, chunk_size);
	//	//std::cout << "data長度" << chunk.length() << std::endl;
	//	send_buffer[global_seq_num] = chunk;
	//	ack_num = received_seq_num + 1;
	//	send_tcp_segment(global_seq_num, ack_num, 0x18, data, data_len);
	//	global_seq_num += chunk.size();
	//	offset += chunk.size();
	//	std::this_thread::sleep_for(std::chrono::milliseconds(1)); // 模拟发送间隔
	//}

	// 确保所有数据包都已确认
	//while (!send_buffer.empty()) {
	//	if (!receive_ack()) {
	//		return; // 发生错误
	//	}
	//}
}

bool IPv6TCPSender::establish_connection() {
	send_tcp_segment(global_seq_num, 0, 0x02, nullptr, 0); // SYN
	if (!receive_ack()) {
		return false;
	}
	global_seq_num++;
	ack_num = received_seq_num + 1;
	send_tcp_segment(global_seq_num, ack_num, 0x10, nullptr, 0); // ACK

	std::cout << "Connection established" << std::endl;
	return true;
}

bool IPv6TCPSender::terminate_connection() {
	send_tcp_segment(global_seq_num, ack_num, 0x01, nullptr, 0); // FIN
	if (!receive_ack()) {
		return false;
	}
	global_seq_num++;
	ack_num = received_seq_num + 1;
	send_tcp_segment(global_seq_num, ack_num, 0x10, nullptr, 0); // ACK
	std::cout << "Connection terminated" << std::endl;
	return true;
}

void IPv6TCPSender::send_ipv6_tcp_rtp_packet() {
	uint32_t sndCount = 0;//发送次数

	while (true) {
		if (send_buffer.size() >= window_size)//滑动窗口限制发送的速率
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(1)); // 模拟发送间隔
			continue;
		}

		if (!pkgQueue.empty()) {
			pkgQueueLock.lock();
			std::string packet = std::move(pkgQueue.front());
			//std::cout << "packet queue size is " << pkgQueue.size() << std::endl;
			pkgQueue.pop();
			pkgQueueLock.unlock();

			size_t payload_len = packet.length();
			send_data(packet.data(), payload_len);

			sndCount++;//发送次数+1
		}
		else
		{
			//进程通信,采集已经完成视频需要结束
			if (isCompleted)
			{
				printLock.lock();
				std::cout << "sending processing is completed, the number of tcp segment is " << sndCount << std::endl;
				printLock.unlock();
				return;
			}
		}

	}
}

