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
		src_ip = _strdup(temp.c_str()); // ת��Ϊconst char*�������ַ���
	}
	else {
		std::cerr << "Failed to get srcIp from XML" << std::endl;
	}

	socket_child = root->FirstChildElement("destIp");
	if (socket_child && socket_child->GetText()) {
		std::string temp = trim(socket_child->GetText());
		dest_ip = _strdup(temp.c_str()); // ת��Ϊconst char*�������ַ���
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

	// ��ʼ��Winsock
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
		std::cerr << "WSAStartup failed" << std::endl;
		exit(EXIT_FAILURE);
	}
	//��tcp�ش�ʱ�丳ֵ 1050ms��ʱ��
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

	int optval = 0;//��Ҫ����ͷ��
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



	//��socket��Դip�Ͷ˿ں�
	struct sockaddr_in6 addr = {};
	addr.sin6_family = AF_INET6;
	addr.sin6_port = htons(src_port);
	addr.sin6_scope_id = 12;//������Ҫ�޸�
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

	// IPv6ͷ��
	ipv6_header ipv6_hdr;
	ipv6_hdr.ver_tc_fl = htonl((6 << 28) | (0 << 20) | 0); // �汾6��������0������ǩ0
	ipv6_hdr.payload_len = htons(TCP_HDRLEN + data_len); // ���س���
	//std::cout << "payload_len:" << htons(ipv6_hdr.payload_len) << std::endl;
	ipv6_hdr.next_header = IPPROTO_TCP; // ��һ��ͷ������  IPPROTO_TCP
	ipv6_hdr.hop_limit = 255; // ��������
	inet_pton(AF_INET6, src_ip, &ipv6_hdr.src_addr);
	inet_pton(AF_INET6, dest_ip, &ipv6_hdr.dest_addr);
	//std::cout << "src_ip: " << src_ip << "dest_ip: " << dest_ip << std::endl;
	// TCPͷ��
	tcp_header tcp_hdr;
	tcp_hdr.src_port = htons(src_port);
	tcp_hdr.dest_port = htons(dest_port);
	tcp_hdr.seq_num = htonl(seq_num); // ʹ�ô�������к�
	tcp_hdr.ack_num = htonl(ack_num);
	tcp_hdr.data_offset = (TCP_HDRLEN / 4) << 4;
	tcp_hdr.flags = flags;
	tcp_hdr.window = htons(65535);
	tcp_hdr.checksum = 0;
	tcp_hdr.urgent_pointer = 0;

	// ��̬�����ڴ��Լ���TCPУ���
	size_t pseudo_header_size = sizeof(struct in6_addr) * 2 + sizeof(unsigned long) + sizeof(unsigned short) + TCP_HDRLEN + data_len;
	char* pseudo_header = new char[pseudo_header_size];
	memset(pseudo_header, 0, pseudo_header_size);

	// ���αͷ��
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

	// ����IPv6ͷ��������
	memcpy(packet, &ipv6_hdr, IPV6_HDRLEN);
	// ����TCPͷ��������
	memcpy(packet + IPV6_HDRLEN, &tcp_hdr, TCP_HDRLEN);
	// ���Ƹ������ݵ�����������ݰ�������rtpͷ������Ϣ��
	if (data_len > 0) {
		memcpy(packet + IPV6_HDRLEN + TCP_HDRLEN, data, data_len);
	}

	delete[] pseudo_header;

	struct sockaddr_in6 dest = {};
	memset(&dest, 0, sizeof(dest));
	dest.sin6_family = AF_INET6;
	//dest.sin6_port = htons(dest_port);


	/// ������
	/// ��������ʱ��Ӧ����ѯ·�ɱ����������˭
	const char* next_ip = "fe80::2ecf:67ff:fe29:d2f8";//��ѯ·�ɱ�����
	/// ·����Ϣ


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

//��Ϊ�Ӻ��������ɵ�������
bool IPv6TCPSender::receive_ack() {


	char buffer[MTU];
	struct sockaddr_in6 src_addr;
	int src_addr_len = sizeof(src_addr);

	if (recvfrom(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr*)&src_addr, &src_addr_len) != SOCKET_ERROR) {
		struct tcp_header* tcp_hdr = (struct tcp_header*)(buffer + IPV6_HDRLEN);
		unsigned int received_ack_num = ntohl(tcp_hdr->ack_num); // ��ȡ���ն˵�ack��
		received_seq_num = ntohl(tcp_hdr->seq_num); // ��ȡ���ն˵����к�
		window_size = ntohs(tcp_hdr->window); //���·��ʹ��ڴ�С(����ӵ�����ڣ�����ȡ��С)

		auto it = send_buffer.begin();
		while (it != send_buffer.end()) {
			if (it->first + it->second.size() < received_ack_num) {
				// ȷ�����кŷ�Χ�ڵ�����
				it = send_buffer.erase(it); // �Ƴ���ȷ�ϵ�����
			}
			else {
				++it;
			}
		}

		printLock.lock();
		std::cout << "ACK received: ack_num=" << received_ack_num << ", seq_num=" << received_seq_num << std::endl;
		printLock.unlock();

		timers.erase(received_ack_num); // �Ƴ���ʱ��

		return true;

	}

	return false;
}

void IPv6TCPSender::process_ack_data() {
	while (!isCompleted||!send_buffer.empty())
	{// δ��ɴ��乤�� ���� �������ﻹ��δ�õ�ȷ�ϵı���
		receive_ack();
	}
}
bool IPv6TCPSender::check_timeout_packet() {

	while (!isCompleted || !timers.empty())
	{

		// ��鳬ʱ�İ����ش�
		for (auto it = timers.begin(); it != timers.end(); ) {
			// ����ʱ����鳬ʱ
			auto now = std::chrono::steady_clock::now();
			if (std::chrono::duration_cast<std::chrono::milliseconds>(now - it->second).count() >= timeout.count()) {
				std::cerr << "Timeout for seq_num=" << it->first << ", retransmitting..." << std::endl;
				retransmit(it->first);
				it->second = now; // ���ö�ʱ��
				// ��ǰ������������Ϊ���Ǹ����˵�ǰԪ��
			}
			else {
				++it; // ֻ��û�г�ʱ�������ǰ��
			}
		}

		//�̶�ʱ����һ��
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

	send_buffer[global_seq_num] = std::string(data, data_len);//ÿ��һ������д�뻺����

	ack_num = received_seq_num + 1;
	send_tcp_segment(global_seq_num, ack_num, 0x18, data, data_len);
	global_seq_num += data_len;// ���� global_seq_num

	//std::this_thread::sleep_for(std::chrono::milliseconds(1)); // ģ�ⷢ�ͼ��
	//size_t offset = 0;
	//while (offset < data_len) {
	//	// ��鴰���Ƿ����������˾͵õ�ACK
	//	while (send_buffer.size() >= window_size) { //���˾͵�����
	//		if (!receive_ack()) {
	//			return; // ��������
	//		}//�����ȴ�
	//	}
	//	size_t chunk_size = std::min(static_cast<size_t>(MTU - (IPV6_HDRLEN + TCP_HDRLEN)), data_len - offset);
	//	std::string chunk(data + offset, chunk_size);
	//	//std::cout << "data�L��" << chunk.length() << std::endl;
	//	send_buffer[global_seq_num] = chunk;
	//	ack_num = received_seq_num + 1;
	//	send_tcp_segment(global_seq_num, ack_num, 0x18, data, data_len);
	//	global_seq_num += chunk.size();
	//	offset += chunk.size();
	//	std::this_thread::sleep_for(std::chrono::milliseconds(1)); // ģ�ⷢ�ͼ��
	//}

	// ȷ���������ݰ�����ȷ��
	//while (!send_buffer.empty()) {
	//	if (!receive_ack()) {
	//		return; // ��������
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
	uint32_t sndCount = 0;//���ʹ���

	while (true) {
		if (send_buffer.size() >= window_size)//�����������Ʒ��͵�����
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(1)); // ģ�ⷢ�ͼ��
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

			sndCount++;//���ʹ���+1
		}
		else
		{
			//����ͨ��,�ɼ��Ѿ������Ƶ��Ҫ����
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

