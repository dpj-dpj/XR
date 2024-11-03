#include <iostream>
#include <vector>
#include<thread>
#include"Sender.h"

const std::string XML = "D:\\VRSender\\xml\\IPv6_network_settings.xml";
const std::string VIDEO_DIR = "D:\\VRSender\\video\\1.mp4";


int main() {

    /*RTPCollector Collector("D:\\VRSender\\video\\1.mp4");
	if (Collector.capture_video_frame())
	{
		std::cout << "Main::Success to load video capture " << std::endl;
	}*/

	IPv6TCPSender Sender(XML, VIDEO_DIR);
	if (Sender.capture_video_frame())
	{
		std::cout << "Main::Success to load video capture " << std::endl;
	}

	/*Sender.establish_connection();

	std::thread capture_frame(&IPv6TCPSender::capture_video_frame, &Sender);
	std::thread transfer_segment(&IPv6TCPSender::send_ipv6_tcp_rtp_packet, &Sender);
	std::thread process_ack(&IPv6TCPSender::process_ack_data, &Sender);

	Sender.terminate_connection();*/
}
