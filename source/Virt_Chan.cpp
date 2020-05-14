#include "Virt_Chan.h"
#include "mbed.h"

int RTP_gateway (W5500_chip* W5500) {
	static unsigned char RX_data[1600];
	
	static unsigned char IP_addr_1[6]={10,151,0,20};
	static unsigned char eth_peer[8]={0x00,0x13,0x3b,0x73,0x12,0xae};
	int RX_size=0;
	int size_UDP;
	int answer=0;
	RX_size = W5500_read_received_size(W5500, RTP_SOCKET); 
	if (RX_size > 0) {
		answer = 1; 
		size_UDP = W5500_read_UDP_pckt(W5500, RTP_SOCKET, RX_data);
		
		W5500_write_short(W5500, 0x0006, RTP_SOCKET, eth_peer, 6);
		W5500_write_short(W5500, 0x000C, RTP_SOCKET, IP_addr_1, 4);
		W5500_write_TX_buffer(W5500, RTP_SOCKET, RX_data+8, size_UDP-8, 1);
		

	}
	return answer;
	
}

