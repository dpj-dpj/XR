#pragma once
#ifndef IPV6TCP_H
#define IPV6TCP_H
#include <ws2tcpip.h>
#define IPV6_HDRLEN 40
#define TCP_HDRLEN 20
#define MTU 1500
// 第一次尝试使用IPv6通信，自定义IPv6+TCP协议

// 手动构建IPv6头部结构体、40字节定长
struct ipv6_header {
	unsigned int ver_tc_fl;      // 版本（4bit）、流量类型（8bit）、流标签（20bit）、共32bit
	unsigned short payload_len;  // 有效载荷长度（16bit），给出了数据包中跟在定长的40字节数据报首部后面的字节数量
	unsigned char next_header;   // 下一个首部，标识数据报中的数据字段需要交付给哪个协议（如TCP或UDP）
	unsigned char hop_limit;// 跳限制，每过一台路由器减1
	struct in6_addr src_addr;    // 源地址（128bit）
	struct in6_addr dest_addr;   // 目的地址（128bit）
};

// 手动构建TCP头部结构体
struct tcp_header {
	unsigned short src_port;
	unsigned short dest_port;
	unsigned int seq_num;
	unsigned int ack_num;
	unsigned char data_offset;
	unsigned char flags;
	unsigned short window;
	unsigned short checksum;
	unsigned short urgent_pointer;
};



#endif // !IPV6TCP_H
