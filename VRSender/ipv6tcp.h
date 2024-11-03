#pragma once
#ifndef IPV6TCP_H
#define IPV6TCP_H
#include <ws2tcpip.h>
#define IPV6_HDRLEN 40
#define TCP_HDRLEN 20
#define MTU 1500
// ��һ�γ���ʹ��IPv6ͨ�ţ��Զ���IPv6+TCPЭ��

// �ֶ�����IPv6ͷ���ṹ�塢40�ֽڶ���
struct ipv6_header {
	unsigned int ver_tc_fl;      // �汾��4bit�����������ͣ�8bit��������ǩ��20bit������32bit
	unsigned short payload_len;  // ��Ч�غɳ��ȣ�16bit�������������ݰ��и��ڶ�����40�ֽ����ݱ��ײ�������ֽ�����
	unsigned char next_header;   // ��һ���ײ�����ʶ���ݱ��е������ֶ���Ҫ�������ĸ�Э�飨��TCP��UDP��
	unsigned char hop_limit;// �����ƣ�ÿ��һ̨·������1
	struct in6_addr src_addr;    // Դ��ַ��128bit��
	struct in6_addr dest_addr;   // Ŀ�ĵ�ַ��128bit��
};

// �ֶ�����TCPͷ���ṹ��
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
