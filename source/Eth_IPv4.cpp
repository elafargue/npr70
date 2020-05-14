// This file is part of "NPR70 modem firmware" software
// (A GMSK data modem for ham radio 430-440MHz, at several hundreds of kbps) 
// Copyright (c) 2017-2020 Guillaume F. F4HDK (amateur radio callsign)
// 
// "NPR70 modem firmware" is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// "NPR70 modem firmware" is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with "NPR70 modem firmware".  If not, see <http://www.gnu.org/licenses/>

#include "Eth_IPv4.h"
#include "mbed.h"
#include "global_variables.h"
#include "L1L2_radio.h"
#include "DHCP_ARP.h"

static unsigned char temp_Eth_buffer[1600];//temporary buffer waiting for ARP entry
static unsigned long int temp_Eth_buff_IP;
static int temp_Eth_buff_size = 0;

unsigned long int IP_char2int(unsigned char* IP_char) {
	unsigned long int result;
	result = 0x1000000*IP_char[0] + 0x10000*IP_char[1] + 0x100*IP_char[2] + IP_char[3];
	return result;
}

void IP_int2char (unsigned long int IP_int, unsigned char* IP_char) {
	IP_char[0] = (IP_int & 0xFF000000) / 0x1000000;
	IP_char[1] = (IP_int & 0x00FF0000) / 0x10000;
	IP_char[2] = (IP_int & 0x0000FF00) / 0x100;
	IP_char[3] = (IP_int & 0xFF);
} 

//static unsigned char match_RTP_value[2][12] = {// FOR TEST ONLY
//	{0x00, 0x13, 0x3B, 0x73, 0x12, 0xAE},
//	{0x98, 0xDE, 0xD0, 0x01, 0x2D, 0x09} 
//}; //first ping test : MAC filter

//void init_RTP_filter(void) {
//	
//}

int Eth_RX_dequeue (W5500_chip* W5500) {
	int answer=0;
	unsigned char RX_data[1600];//1600
	unsigned int RX_port=0;
	unsigned char RX_proto;
	unsigned long int RX_dest_IP=0;
	//unsigned char* RX_Eth_pckt;
	//static int match_RTP_index[12] = {8, 9, 10, 11, 12, 13}; //first ping tests : TX MAC filter
	
	static int more_to_read = 0;
	
	int RX_size=0;
	int mac_size=0;
	//int match_RTP = 1;
	unsigned int ethertype;
	if (*(W5500->interrupt)==0) {
		W5500_write_byte(W5500, 0x0002, RAW_SOCKET, 0xFF);//ack interrupt
		more_to_read=1;
	}
	if ((more_to_read == 1))  { 
		RX_size = W5500_read_received_size(W5500, RAW_SOCKET); 
		//if (RX_size > DEBUG_max_rx_size_w5500) {//!!!
		//	DEBUG_max_rx_size_w5500 = RX_size;//!!!
		//	printf("max buffer:%i\r\n", DEBUG_max_rx_size_w5500);//!!!
		//}//!!!
		if (RX_size > 0) {
			answer=1;

			mac_size = W5500_read_MAC_pckt(W5500, RAW_SOCKET, RX_data);
			if (RX_size > mac_size) {
				more_to_read = 1;
			} else {
				more_to_read = 0;
			}

			// Check if RTP packet
			RX_Eth_IPv4_counter++;
			//match_RTP = 1;
			//for (i=0; i<=6; i++) {
			//	if (RX_data[match_RTP_index[i]] != match_RTP_value[is_TDMA_master][i]) {
			//		match_RTP = 0;
			//	}
				
			//}
			// FOR FUTURE VIRTUAL CHANNEL 
			//if (match_RTP==1) {
			//	W5500_write_TX_buffer(W5500, RTP_SOCKET, RX_data+44, mac_size-44, 0);
			//} 
			
			ethertype = RX_data[14]*0x100 + RX_data[15];
			
			if (ethertype == 0x0806) { //ARP packet received
				//printf("ARP packet received!\r\n");
				if ((!is_TDMA_master)||(CONF_master_FDD<2)) {
					ARP_RX_packet_treatment (RX_data+2, mac_size-2);
				}
			}
			
			if (ethertype == 0x0800) { // IPv4 packet
				RX_port = 0;
				RX_proto = 0;
				RX_dest_IP = 0;
				if ( (is_TDMA_master) && (CONF_master_FDD == 1) ) {//master down
					//RX_Eth_pckt = RX_data+2;
					RX_port = (RX_data[38] << 8) + RX_data[39];
					RX_proto = RX_data[25];//11 for UDP
					RX_dest_IP = IP_char2int(RX_data+32);
				}
				if ( (RX_proto == 0x11) && (RX_dest_IP == LAN_conf_applied.LAN_modem_IP) && (RX_port == 0x1A3C) ) { // data for FDD down
					//printf("RX_from_Eth\r\n");
					FDDdown_RX_pckt_treat(RX_data+44, mac_size-44);
				} else {
					//printf("RXeth %i\r\n", mac_size-2);
					IPv4_to_radio (RX_data+2, mac_size-2);
					//Eth_pause_frame_TX(10);//!!!
				}
			}
			
						
		}
	}
	return answer;
}

void Eth_pause_frame_TX(unsigned int time) {
	int i;
	unsigned char pause_frame[70] = {
		0x01, 0x80, 0xC2, 0x00, 0x00, 0x01,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x88, 0x08, 0x00, 0x01, 0x4E, 0x70,    /* 0x17, 0x70 */
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
	for (i=0; i<6; i++) {
		pause_frame[i+6] = CONF_modem_MAC[i];
	}
	W5500_write_TX_buffer(W5500_p1, RAW_SOCKET, pause_frame, 60, 0); 
}

void IPv4_to_radio (unsigned char* RX_Eth_frame, int size) {
	int i;
	unsigned long int dest_IP_addr;
	int MAC_dest_match = 1;
	int is_inside_subnet = 0;
	int is_inside_client_range = 0; 
	unsigned char loc_client_ID;
	int radio_tx_need;
	for (i=0; i<6; i++) { 
		if (RX_Eth_frame[i] != CONF_modem_MAC[i]) {
			MAC_dest_match = 0;
		}
	}
	radio_tx_need = 0;
	if (MAC_dest_match == 1) {//we only take frame with destination = modem_MAC, not broadcast or multicast
		dest_IP_addr = IP_char2int(RX_Eth_frame+30); 
		
		if ( (is_TDMA_master) && (dest_IP_addr != LAN_conf_applied.LAN_modem_IP) ) { // TDMA Master
			if ( (dest_IP_addr >= CONF_radio_IP_start) && (dest_IP_addr < (CONF_radio_IP_start + CONF_radio_IP_size) ) ) {
				loc_client_ID = lookfor_client_ID_from_IP (dest_IP_addr); 
				//printf ("IP %X is client %i\r\n", dest_IP_addr, loc_client_ID);
				if (loc_client_ID < 250) {
					radio_tx_need = 1; 
					
				}
			}
		}
		if ( (!is_TDMA_master) && (dest_IP_addr != LAN_conf_applied.LAN_modem_IP) ) { // TDMA Client
			if ( (dest_IP_addr & LAN_conf_applied.LAN_subnet_mask) == (LAN_conf_applied.LAN_modem_IP & LAN_conf_applied.LAN_subnet_mask) ) {
				is_inside_subnet = 1;
			}
			if ( (dest_IP_addr >= LAN_conf_applied.DHCP_range_start) && (dest_IP_addr < (LAN_conf_applied.DHCP_range_start + LAN_conf_applied.DHCP_range_size)) ) {
				is_inside_client_range = 1;
			}
			//printf ("inside subnet:%i inside_DHCP%i\r\n", is_inside_subnet, is_inside_client_range);
			// inside subnet but outside radio range -> to master
			if ( (is_inside_subnet == 1) && (is_inside_client_range == 0) ) {
				loc_client_ID = my_radio_client_ID;
				radio_tx_need = 1;
			}
			// outside subnet and IP gateway active -> to master
			if ( (is_inside_subnet == 0) && (LAN_conf_applied.LAN_def_route_activ == 1) ) {
				loc_client_ID = my_radio_client_ID;
				radio_tx_need = 1;
			}
		}

		if ( (radio_tx_need) && (my_client_radio_connexion_state == 2) ) {
			segment_and_push(RX_Eth_frame + 14, size - 14, loc_client_ID, 0x02); //0x02 is IPv4 access protocol
			//printf("seg&push %i\r\n", size - 14);
		}
	}
}

void IPv4_from_radio (unsigned char* RX_eth_frame, int RX_size) { //Rx size includes ethernet header
	unsigned long int dest_IP_addr;
	unsigned long int LAN_dest_IP;
	int local_size;
	int dest_MAC_found;
	int is_inside_subnet = 0;
	int is_inside_radio_range = 0;
	unsigned char loc_client_ID;
	int i;
	//dest_IP_addr = IP_char2int(RX_radio_frame+16)
	int eth_TX_need = 0; 
	int radio_tx_need = 0;
	dest_IP_addr = IP_char2int(RX_eth_frame + 30); 
	
	local_size = 0x100*RX_eth_frame[16] + RX_eth_frame[17];
	//printf("size IPv4 : %i\r\n", local_size);
	
	//printf (" IPv4 radio RX\r\n");
	if (is_TDMA_master) { // TDMA Master
		if ( (dest_IP_addr & LAN_conf_applied.LAN_subnet_mask) == (LAN_conf_applied.LAN_modem_IP & LAN_conf_applied.LAN_subnet_mask) ) {
			is_inside_subnet = 1;
		}
		if ( (dest_IP_addr >= CONF_radio_IP_start) && (dest_IP_addr < (CONF_radio_IP_start + CONF_radio_IP_size) ) ) {
			is_inside_radio_range = 1;
		}
		//printf("IPv4 from R: inside subnet:%i inside_radio:%i\r\n", is_inside_subnet, is_inside_radio_range);
		// destinated to IP on LAN 
		if ( (is_inside_subnet == 1) && (is_inside_radio_range == 0) ) {
			eth_TX_need = 1;
			LAN_dest_IP = dest_IP_addr;
		}
		// destinated to gateway (out of subnet)
		if ( (is_inside_subnet == 0) && (LAN_conf_applied.LAN_def_route_activ == 1) ) {
			eth_TX_need = 1;
			LAN_dest_IP = LAN_conf_applied.LAN_def_route;
		}
		//forward to another radio client
		if (is_inside_radio_range) {
			loc_client_ID = lookfor_client_ID_from_IP (dest_IP_addr); 
			if (loc_client_ID < 250) {
				radio_tx_need = 1;
			}
		}
	}
	if (!is_TDMA_master) { // TDMA client
		// Checks if dest IP is inside local range
		if ( (dest_IP_addr >= LAN_conf_applied.DHCP_range_start) && (dest_IP_addr < (LAN_conf_applied.DHCP_range_start + LAN_conf_applied.DHCP_range_size)) ) { 
			eth_TX_need = 1;
			LAN_dest_IP = dest_IP_addr;
		}
	}
	// transmit to Ethernet
	if ( (eth_TX_need) && (local_size < 1510) ) {
		//printf("IPv4 Eth TX\r\n");
		for (i=0; i<6; i++) {
			RX_eth_frame[i+6] = CONF_modem_MAC[i];
		}
		RX_eth_frame[12] = 0x08; // Ethertype IPv4
		RX_eth_frame[13] = 0x00; 
		dest_MAC_found = lookfor_MAC_from_IP (RX_eth_frame, LAN_dest_IP);
		if (dest_MAC_found) {		
			W5500_write_TX_buffer(W5500_p1, RAW_SOCKET, RX_eth_frame, local_size + 14, 0); 
			//RX_radio_IPv4_counter++;
		}
		else {
			temp_Eth_buff_IP = LAN_dest_IP;
			temp_Eth_buff_size = local_size + 14;
			memcpy (temp_Eth_buffer, RX_eth_frame, temp_Eth_buff_size); 
		}
	}
	// transmit to radio
	if (radio_tx_need) {
		segment_and_push(RX_eth_frame + 14, local_size, loc_client_ID, 0x02); //0x02 is IPv4 access protocol
		//TX_radio_IPv4_counter++;
	}
}

void flush_temp_Eth_buffer(unsigned long int loc_IP) {
	if (temp_Eth_buff_size > 0) {
		if (loc_IP == temp_Eth_buff_IP) {
			lookfor_MAC_from_IP (temp_Eth_buffer, loc_IP); //puts MAC inside Eth
			W5500_write_TX_buffer(W5500_p1, RAW_SOCKET, temp_Eth_buffer, temp_Eth_buff_size, 0); 
			//RX_radio_IPv4_counter++;
		}
		temp_Eth_buff_size = 0;
	}
}

unsigned char lookfor_client_ID_from_IP(unsigned long int IP_addr) {
	unsigned char i, i_found;
	unsigned long int last_IP;
	i_found = 250;
	
	for (i=0; i<radio_addr_table_size; i++) {
		last_IP = CONF_radio_addr_table_IP_begin[i] + CONF_radio_addr_table_IP_size[i];
		if ( (IP_addr >= CONF_radio_addr_table_IP_begin[i]) && (IP_addr < last_IP) ) {
			i_found = i;
		}
	}
	return i_found;
}
