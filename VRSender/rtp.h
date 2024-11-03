#pragma once
#ifndef RTP_H
#define RTP_H
#include <stdint.h>

#define RTP_VERSION              3
#define RTP_PAYLOAD_TYPE_MESH   90
#define RTP_PAYLOAD_TYPE_PCM    91
#define RTP_PAYLOAD_TYPE_H264   96
#define RTP_PAYLOAD_TYPE_MP4V   96
#define RTP_PAYLOAD_TYPE_AAC    97


#define RTP_PACKET_HEADER_SIZE  4
#define RTP_HEADER_SIZE         12
#define MTU						1500
#define IPV6_HEADER_SIZE        40
#define TCP_HEADER_SIZE         20
/*
 *
 *    0                   1                   2                   3
 *    7 6 5 4 3 2 1 0|7 6 5 4 3 2 1 0|7 6 5 4 3 2 1 0|7 6 5 4 3 2 1 0
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *   |V=2|P|X|  CC   |M|     PT      |       sequence number         |
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *   |                           timestamp                           |
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *   |           synchronization source (SSRC) identifier            |
 *   +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
 *   |            contributing source (CSRC) identifiers             |
 *   :                             ....                              :
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *
 */
struct rtpHeader {
	/* byte 0 */
	uint8_t csrcLen : 4;
	uint8_t extension : 1;
	uint8_t padding : 1;
	uint8_t version : 2; // 最高2位

	/* byte 1 */
	uint8_t payloadType : 7;
	uint8_t marker : 1;

	/* bytes 2,3 */
	uint16_t seq;

	/* bytes 4-7 */
	uint32_t timestamp;

	/* bytes 8-11 */
	uint32_t ssrc;
};
struct rtpPacket {
	char header[RTP_PACKET_HEADER_SIZE];// 前面4字节是暂时保留的字节
	rtpHeader rtpHdr;
	uint8_t payload[0];// 在分配内存时动态

};

#endif // !RTP_H
