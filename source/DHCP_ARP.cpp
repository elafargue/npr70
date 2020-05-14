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

#include "DHCP_ARP.h"
#include "mbed.h"
#include "global_variables.h"
#include "Eth_IPv4.h"
#include "W5500.h"
#include "HMI_telnet.h"

// Content : DHCP server (for TDMA clients and point to point config)
// ARP proxy for "bridged ethernet" emulation. 
// ARP resolution (a little bit ugly)
 
#define DHCP_ARP_tab_size 32
#define DHCP_ARP_timeout 360 /*120*/
static unsigned char DHCP_ARP_MAC[DHCP_ARP_tab_size][6];
static unsigned long int DHCP_ARP_IP[DHCP_ARP_tab_size];
static unsigned char DHCP_ARP_status[DHCP_ARP_tab_size];	//		DHCP						ARP
															// 0:	Free						Free
															// 1:	allocation in progress		(unused)
															// 2:	allocated					Valid
															// 3:	prefered but not allocated	Timeout_1
static unsigned int DHCP_ARP_date[DHCP_ARP_tab_size];

int compare_IP(unsigned char* IP1, unsigned char* IP2) {
	int result;
	int i;
	result = 1;
	for (i=0; i<4; i++) {
		if (IP1[i] != IP2[i]) {
			result = 0;
		}
	}
	return result;
}

int compare_MAC(unsigned char* MAC1, unsigned char* MAC2) {
	int result;
	int i;
	result = 1;
	for (i=0; i<6; i++) {
		if (MAC1[i] != MAC2[i]) {
			result = 0;
		}
	}
	return result;
}

// *** DHCP functions ***

void reset_DHCP_table(LAN_conf_T* LAN_config) {
	int i;
	for (i=0; i<DHCP_ARP_tab_size; i++) {
		DHCP_ARP_status[i] = 0;
	}
	
}

void DHCP_release (LAN_conf_T* LAN_config, unsigned char* client_MAC) {
	int i;
	for (i=0; i<DHCP_ARP_tab_size; i++) {
		if ((compare_MAC( DHCP_ARP_MAC[i], client_MAC)) && (DHCP_ARP_status[i]))  {
			DHCP_ARP_status[i] = 0;
		}
	}
	
}

int lookfor_free_LAN_IP (LAN_conf_T* LAN_config, unsigned char* client_MAC, unsigned char* requested_IP, unsigned char* proposed_IP, int req_type) {
	//req type : like DHCP. 1:discover 3:request
	int answer=0;
	int i_previous_alloc = 255;
	int match_previous_alloc = 0;
	int i,j;
	unsigned long int proposed_IP_int = 0xFFFFFFFF;
	int new_alloc_entry = 0;
	unsigned char new_status;
	//unsigned long int requested_IP_int;
	unsigned long int req_IP_int; 
	
	req_IP_int = IP_char2int (requested_IP); 
		
	// 1) tests
	// 1.1) lookfor client MAC in DHCP table
	for (i=0; i<DHCP_ARP_tab_size; i++) {
		if ((compare_MAC( DHCP_ARP_MAC[i], client_MAC)) && (DHCP_ARP_status[i]))  {
			i_previous_alloc  = i;
		}
		//printf("MAC_comparison:%i %i\r\n", compare_MAC( LAN_config->DHCP_alloc_MAC[i], client_MAC), i);
	}
	//printf ("previous entry with same MAC: %i\r\n", i_previous_alloc);
	// 1.2)check if matches previous allocation
	if (i_previous_alloc!=255) {
		if (DHCP_ARP_IP[i_previous_alloc] == req_IP_int) {
			match_previous_alloc = 1;
		} else {
			match_previous_alloc = 0;
		}
		if (match_previous_alloc==1) {// matches previous allocation
			// total match with previous allocation
			answer = 1;
			if (req_type==1) {
				DHCP_ARP_status[i_previous_alloc] = 1;
			}
			if (req_type==3) {
				DHCP_ARP_status[i_previous_alloc] = 2;
				DHCP_ARP_date[i_previous_alloc] = GLOBAL_timer.read_us();
			}
		} else {
			//address requested different from previous allocation
			
		}
		//printf ("req matches previous:%i\r\n", match_previous_alloc);
		
	}
	// 1.3) check if requested IP agreed : inside range
	int requ_IP_inside_range;
	if ( (req_IP_int >= LAN_config->DHCP_range_start) && (req_IP_int < (LAN_config->DHCP_range_start + LAN_config->DHCP_range_size)) ) {
		requ_IP_inside_range = 1;
	} else {
		requ_IP_inside_range = 0;
	}
	//printf("requested ip inside range:%i\r\n", requ_IP_inside_range);
	// 1.4) chech if requested IP is free (not allocated to another MAC)
	int req_IP_free = 1;
	for (i=0; i<DHCP_ARP_tab_size; i++) {
		if ( (DHCP_ARP_IP[i] == req_IP_int) && (DHCP_ARP_status[i]==2) && (i != i_previous_alloc) ) {
			req_IP_free = 0;
		}
	}
	// 1.5) synthesis
	int req_IP_agreed;
	if ( (requ_IP_inside_range == 1) && (req_IP_free == 1) ) {
		req_IP_agreed = 1;
	} else {
		req_IP_agreed = 0;
	}
	// 2.1) free previous DHCP entry
	if ( (i_previous_alloc!=255) && (match_previous_alloc==0) && (req_IP_agreed==1) ) {
		DHCP_ARP_status[i_previous_alloc] = 0;
	}
	// 2.2) look for unallocated IP
	unsigned long int IP_tested;
	int OK_loc;
	int found_IP = 0;
	if ( (req_type==1) && (req_IP_agreed==0) && (i_previous_alloc==255) ) {
		for (i = (LAN_config->DHCP_range_size - 1); i>=0; i--) {
			IP_tested = LAN_config->DHCP_range_start + i;
			OK_loc=1;
			for (j=0; j<DHCP_ARP_tab_size; j++) {
				if ( (IP_tested == DHCP_ARP_IP[j]) && (DHCP_ARP_status[j]==2) ) { // a améliorer : trop simple
					OK_loc = 0;
				}
			}
			if (OK_loc == 1) {
				proposed_IP_int = IP_tested;
				found_IP = 1;
			}
		}
		if (found_IP == 1) {
			new_alloc_entry = 1;
			IP_int2char (proposed_IP_int,proposed_IP);
			new_status = 1; 
			answer = 3;
		}
		//printf("case 2 new IP needed \r\n");
	}
	
	// 2.3) anwser with previous allocation 
	if ( (req_type == 1) && (req_IP_agreed==0) && (i_previous_alloc!=255) ) {
		IP_int2char (DHCP_ARP_IP[i_previous_alloc], proposed_IP);
		if (req_type == 1) { new_status = 1; }
		if (req_type == 3) { 
			new_status = 2; 
			DHCP_ARP_date[i_previous_alloc] = GLOBAL_timer.read_us();
		}
		DHCP_ARP_status [i_previous_alloc] = new_status;
		answer = 3;
		//printf("case 3 answer with previous alloc\r\n");
	}
	
	// 2.4) new IP agreed, but do match previous allocation
	if ( (i_previous_alloc!=255) && (match_previous_alloc==0) && (req_IP_agreed==1) ) {
		for (i=0; i<4; i++) {
			proposed_IP [i] = requested_IP[i]; 
		}
		new_alloc_entry = 1; 
		if (req_type == 1) { new_status = 1; }
		if (req_type == 3) { new_status = 2; }
		answer = 1;
		//printf("case 4 new IP ok, do not match previous\r\n");
	}
	
	// 2.5) new IP agreed and matches previous allocation
	if ( (i_previous_alloc!=255) && (match_previous_alloc==1) && (req_IP_agreed==1) ) {
		for (i=0; i<4; i++) {
			proposed_IP [i] = requested_IP[i]; 
		}
		if (req_type == 1) { new_status = 1; }
		if (req_type == 3) { 
			new_status = 2; 
			DHCP_ARP_date[i_previous_alloc] = GLOBAL_timer.read_us();
		}
		DHCP_ARP_status [i_previous_alloc] = new_status;
		answer = 1;
		//printf("case 5 IP requested match previous, and OK\r\n");
	}
	
	// 2.6) new IP agreed, no previous allocation
	if ( (i_previous_alloc==255) && (req_IP_agreed==1) ) {
		for (i=0; i<4; i++) {
			proposed_IP [i] = requested_IP[i]; 
		}
		new_alloc_entry = 1;
		if (req_type == 1) { new_status = 1; }
		if (req_type == 3) { new_status = 2; }
		answer = 1;
		//printf("case 6 IP requested ok, no previous alloc\r\n");
	}

	// new DHCP entry 
	int free_DHCP_slot=255;
	if (new_alloc_entry == 1) {
		for (i=DHCP_ARP_tab_size-1; i>=0; i--) {
			if (DHCP_ARP_status[i]!=2) {
				free_DHCP_slot = i;
			}
			//printf("dhcp inside lookfor:%i stat:%i\r\n", i, LAN_config->DHCP_alloc_status[i]);
		}
		if (free_DHCP_slot!=255) {
			DHCP_ARP_status[free_DHCP_slot] = new_status;
			if (new_status == 2) {DHCP_ARP_date[free_DHCP_slot] = GLOBAL_timer.read_us();}
			DHCP_ARP_IP[free_DHCP_slot] = IP_char2int(proposed_IP);
			for (i=0; i<6; i++) {
				DHCP_ARP_MAC[free_DHCP_slot][i] = client_MAC[i];
			}
			//printf("new DHCP entry OK, index %i status %i \r\n", free_DHCP_slot, new_status);
		}
	}
	return answer;
}

/**
 * Runs the DHCP server routine that is regularly called from the main loop
 */
void DHCP_server(LAN_conf_T* LAN_config, W5500_chip* W5500 ) {
	static unsigned char RX_data[600];//600
	unsigned char client_MAC[7];
	unsigned char session_ID[5];
	unsigned char requested_IP[5]={0,0,0,0,0};
	unsigned char DHCP_server_IP[5]={0,0,0,0,0};
	int RX_size, size_UDP, i;
	unsigned char message_type_client = 0;
	unsigned char message_type_server;
	int index_opt_answer=240;
	static unsigned char DHCP_answer[400];
	unsigned char proposed_IP[4];
	int loc_status;
	
	
	RX_size = W5500_read_received_size(W5500, DHCP_SOCKET); 
	if (RX_size>0) {
		
		size_UDP = W5500_read_UDP_pckt(W5500, DHCP_SOCKET, RX_data, 0);
		if (RX_data[8]==1) { // Valid DHCP request
			
			// *** DHCP request decoding ***
			// client mac address read
			for (i=0; i<6; i++) {
				client_MAC[i] = RX_data[i+36];
				//printf("%x:", client_MAC[i]);
			}
			// session ID (XID)
			for (i=0; i<4; i++) {
				session_ID[i] = RX_data[i+12];
				//printf("%x:", session_ID[i]);
			}
			for (i=0; i<4; i++) {
				requested_IP[i] = RX_data[i+20];
			}
			//printf("\r\n");
			//printf("\r\nDHCP from client RXs:%d UDPs:%d\r\n", RX_size, size_UDP);
			// DHCP option read
			int option_pos;
			unsigned char option_size;
			unsigned char option_type;
			option_pos = 248;
			do {
				option_type = RX_data[option_pos];
				option_size = RX_data[option_pos+1];
				//printf("option:%d\r\n",option_type);
				
				switch (option_type) {
					case 53 : message_type_client = RX_data[option_pos+2]; break;
					case 50 :
						requested_IP[0] = RX_data[option_pos+2];
						requested_IP[1] = RX_data[option_pos+3];
						requested_IP[2] = RX_data[option_pos+4];
						requested_IP[3] = RX_data[option_pos+5];
						break;
					case 54 : 
						DHCP_server_IP[0] = RX_data[option_pos+2];
						DHCP_server_IP[1] = RX_data[option_pos+3];
						DHCP_server_IP[2] = RX_data[option_pos+4];
						DHCP_server_IP[3] = RX_data[option_pos+5];
						break;
				} 
				option_pos = option_pos + option_size + 2;
			} while ((option_type != 255) && (option_pos < size_UDP));
			
			// independant DHCP answer fields
			for (i=0; i<400; i++ )  {
				DHCP_answer[i] = 0;
			}
			DHCP_answer[0] = 0x02;//answer
			DHCP_answer[1] = 0x01;
			DHCP_answer[2] = 0x06;
			DHCP_answer[3] = 0x00;
			for (i=0; i<4; i++) {// session ID
				DHCP_answer[i+4] = session_ID[i];
			}
			for (i=0; i<6; i++) {// client MAC
				DHCP_answer[i+28] = client_MAC[i];
			}
			DHCP_answer[236] = 0x63;//magic cookie
			DHCP_answer[237] = 0x82;
			DHCP_answer[238] = 0x53;
			DHCP_answer[239] = 0x63;
			
			// decision
			loc_status = lookfor_free_LAN_IP (LAN_config, client_MAC, requested_IP, proposed_IP, message_type_client);
			
			if (message_type_client == 1) { // discover -> offer
				
				for (i=0; i<4; i++) { //proposed IP
					DHCP_answer[i+16] = proposed_IP[i];
				}
				
				message_type_server = 2;
				DHCP_answer[index_opt_answer]=53; // message type
				DHCP_answer[index_opt_answer+1]=1;
				DHCP_answer[index_opt_answer+2]=message_type_server;
				index_opt_answer = index_opt_answer +3;
				DHCP_answer[index_opt_answer]=1; // subnet mask
				DHCP_answer[index_opt_answer+1]=4;
				IP_int2char (LAN_config->LAN_subnet_mask, (DHCP_answer + index_opt_answer + 2) );
				index_opt_answer = index_opt_answer +6;
				if (LAN_config->LAN_def_route_activ == 1) {
					DHCP_answer[index_opt_answer]=3; // default route
					DHCP_answer[index_opt_answer+1]=4;

					IP_int2char (LAN_config->LAN_def_route, (DHCP_answer + index_opt_answer + 2) );
					index_opt_answer = index_opt_answer +6;
				}
				if (LAN_config->LAN_DNS_activ == 1) {
					DHCP_answer[index_opt_answer]=6; // DNS server
					DHCP_answer[index_opt_answer+1]=4;

					IP_int2char (LAN_config->LAN_DNS_value, (DHCP_answer + index_opt_answer + 2) );
					index_opt_answer = index_opt_answer +6;
				}
				DHCP_answer[index_opt_answer]=51; //Lease Time
				DHCP_answer[index_opt_answer+1]=4;
				DHCP_answer[index_opt_answer+2]=0x00;
				DHCP_answer[index_opt_answer+3]=0x00;//01
				DHCP_answer[index_opt_answer+4]= (DHCP_ARP_timeout & 0xFF00) >> 8;//0x00
				//DHCP_answer[index_opt_answer+5]=0x3C;//80
				DHCP_answer[index_opt_answer+5]= DHCP_ARP_timeout & 0xFF;//DHCP_ARP_timeout
				index_opt_answer = index_opt_answer +6;
				DHCP_answer[index_opt_answer]=54; // DHCP server IP
				DHCP_answer[index_opt_answer+1]=4;
				IP_int2char (LAN_config->LAN_modem_IP, (DHCP_answer + index_opt_answer + 2) );
				index_opt_answer = index_opt_answer + 6;
				//DHCP_answer[index_opt_answer]=26; // Interface MTU
				//DHCP_answer[index_opt_answer+1]=2;
				//DHCP_answer[index_opt_answer+2]=0x03;//02
				//DHCP_answer[index_opt_answer+3]=0xE8;//40
				//index_opt_answer = index_opt_answer + 4;
				//DHCP_answer[index_opt_answer]=27; // subnets local
				//DHCP_answer[index_opt_answer+1]=1;
				//DHCP_answer[index_opt_answer+2]=0x01;
				//index_opt_answer = index_opt_answer + 3;
				DHCP_answer[index_opt_answer]=255;// end
				index_opt_answer = index_opt_answer + 1;
				W5500_write_TX_buffer(W5500, DHCP_SOCKET, DHCP_answer, index_opt_answer, 1);
			}
			
			if ( (message_type_client == 3) && (loc_status !=0 ) )  { // request -> ack
				for (i=0; i<4; i++) { //proposed IP 
					DHCP_answer[i+16] = proposed_IP[i];
				}
				
				message_type_server = 5;
				DHCP_answer[index_opt_answer]=53; // message type
				DHCP_answer[index_opt_answer+1]=1;
				DHCP_answer[index_opt_answer+2]=message_type_server;
				index_opt_answer = index_opt_answer +3;
				DHCP_answer[index_opt_answer]=1; // subnet mask
				DHCP_answer[index_opt_answer+1]=4;
				IP_int2char (LAN_config->LAN_subnet_mask, (DHCP_answer + index_opt_answer + 2) );
				index_opt_answer = index_opt_answer +6;
				if (LAN_config->LAN_def_route_activ == 1) {
					DHCP_answer[index_opt_answer]=3; // default route
					DHCP_answer[index_opt_answer+1]=4;

					IP_int2char (LAN_config->LAN_def_route, (DHCP_answer + index_opt_answer + 2) );
					index_opt_answer = index_opt_answer +6;
				}
				if (LAN_config->LAN_DNS_activ == 1) {
					DHCP_answer[index_opt_answer]=6; // DNS server
					DHCP_answer[index_opt_answer+1]=4;

					IP_int2char (LAN_config->LAN_DNS_value, (DHCP_answer + index_opt_answer + 2) );
					index_opt_answer = index_opt_answer +6;
				}
				DHCP_answer[index_opt_answer]=51; //Lease Time
				DHCP_answer[index_opt_answer+1]=4;
				DHCP_answer[index_opt_answer+2]=0x00;
				DHCP_answer[index_opt_answer+3]=0x00;//01
				DHCP_answer[index_opt_answer+4]= (DHCP_ARP_timeout & 0xFF00) >> 8;//51
				DHCP_answer[index_opt_answer+5]= DHCP_ARP_timeout & 0xFF;
				index_opt_answer = index_opt_answer +6;
				DHCP_answer[index_opt_answer]=54; // DHCP server IP
				DHCP_answer[index_opt_answer+1]=4;
				IP_int2char (LAN_config->LAN_modem_IP, (DHCP_answer + index_opt_answer + 2) );
				index_opt_answer = index_opt_answer + 6;
				//DHCP_answer[index_opt_answer]=26; // Interface MTU
				//DHCP_answer[index_opt_answer+1]=2;
				//DHCP_answer[index_opt_answer+2]=0x03;//02
				//DHCP_answer[index_opt_answer+3]=0xE8;//40
				//index_opt_answer = index_opt_answer + 4;
				//DHCP_answer[index_opt_answer]=27; // subnets local
				//DHCP_answer[index_opt_answer+1]=1;
				//DHCP_answer[index_opt_answer+2]=0x01;
				//index_opt_answer = index_opt_answer + 3;
				DHCP_answer[index_opt_answer]=255;// end
				index_opt_answer = index_opt_answer + 1;
				W5500_write_TX_buffer(W5500, DHCP_SOCKET, DHCP_answer, index_opt_answer, 1);
			}
			
			if ( (message_type_client == 3) && (loc_status ==0 ) ) { //request -> NAK
				//printf ("NAK NAK\r\n");
				message_type_server = 6; // NAK
				DHCP_answer[index_opt_answer]=53; // message type
				DHCP_answer[index_opt_answer+1]=1;
				DHCP_answer[index_opt_answer+2]=message_type_server;
				index_opt_answer = index_opt_answer +3;
				DHCP_answer[index_opt_answer]=255;// end
				index_opt_answer = index_opt_answer + 1;
				W5500_write_TX_buffer(W5500, DHCP_SOCKET, DHCP_answer, index_opt_answer, 1);
			}
			
			if ( message_type_client == 7 ) { // DHCP RELEASE
				//printf ("DHCP RELEASE\r\n");
				DHCP_release (LAN_config, client_MAC); 
			}	
		}
	}
}

void DHCP_ARP_print_entries(void) {
	int i;
	unsigned char loc_IP_char[10];
	for (i=0; i<DHCP_ARP_tab_size; i++) {
		if (DHCP_ARP_status[i]!=0) {
			IP_int2char (DHCP_ARP_IP[i], loc_IP_char);
			HMI_printf ("   %i:stat:%i IP:", i, DHCP_ARP_status[i]);
			HMI_printf ("%i.%i.%i.%i MAC:", loc_IP_char[0], loc_IP_char[1], loc_IP_char[2], loc_IP_char[3]);
			HMI_printf("%02X:", DHCP_ARP_MAC[i][0]);
			HMI_printf("%02X:", DHCP_ARP_MAC[i][1]);
			HMI_printf("%02X:", DHCP_ARP_MAC[i][2]);
			HMI_printf("%02X:", DHCP_ARP_MAC[i][3]);
			HMI_printf("%02X:", DHCP_ARP_MAC[i][4]);
			HMI_printf("%02X ", DHCP_ARP_MAC[i][5]);
			HMI_printf("age:%isec\r\n", (GLOBAL_timer.read_us()-DHCP_ARP_date[i])/1000000);
		}
	}
	HMI_printf("\r\nready> ")
}

// *** ARP proxy and ARP resolver ***

void ARP_proxy (unsigned char* ARP_req_packet, int size) {
	int answer_needed = 0;
	int i;
	int is_inside_subnet = 0;
	int is_inside_client_range = 0; 
	unsigned long int ARP_client_IP;
	unsigned long int ARP_requested_IP;
	unsigned char ARP_client_MAC[6]; 
		
	unsigned char ARP_answ_packet[50] = {
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,	// dest MAC
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	// source MAC
		0x08, 0x06, 						// Ethtype ARP
		0x00, 0x01,							// HW type Ethernet
		0x08, 0x00, 						// protocol IPv4
		0x06, 								// HW addr size
		0x04, 								// protocol size
		0x00, 0x02	 						// ARP opcode "reply"
	};
	
	ARP_client_IP = IP_char2int (ARP_req_packet+28); //in request : client = sender
	ARP_requested_IP = IP_char2int (ARP_req_packet+38); //in request : server = target
	for (i=0; i<6; i++) {
		ARP_client_MAC[i] = ARP_req_packet[22+i]; // in request : client = sender
	}
	
	// determines if modem should reply or not 
	if (ARP_requested_IP != LAN_conf_applied.LAN_modem_IP) { //only replies to non-modem IP
	
		if (is_TDMA_master) { 
		//TDMA Master (and FDD down) answers to all IP in radio range
			if ( (ARP_requested_IP >= CONF_radio_IP_start) && (ARP_requested_IP < (CONF_radio_IP_start + CONF_radio_IP_size) ) ) {
				answer_needed = 1;
			}
		}
		else { //TDMA Slave answers to IP inside subnet, but which dont belong to it's own range
			if ( (ARP_requested_IP & LAN_conf_applied.LAN_subnet_mask) == (LAN_conf_applied.LAN_modem_IP & LAN_conf_applied.LAN_subnet_mask) ) {
				is_inside_subnet = 1;
			}
			if ( (ARP_requested_IP >= LAN_conf_applied.DHCP_range_start) && (ARP_requested_IP < (LAN_conf_applied.DHCP_range_start + LAN_conf_applied.DHCP_range_size)) ) {
				is_inside_client_range = 1;
			}
			if ( (is_inside_subnet == 1) && (is_inside_client_range == 0) ) {
				answer_needed = 1;
			}
		}
	}
	
	if (answer_needed) {
		for (i=0; i<6; i++) {				// Destination MAC
			ARP_answ_packet[i] = ARP_client_MAC[i];
		}
		for (i=0; i<6; i++) {				// Source MAC
			ARP_answ_packet[i+6] = CONF_modem_MAC[i];
		}
		for (i=0; i<6; i++) {				// ARP Sender MAC
			ARP_answ_packet[i+22] = CONF_modem_MAC[i];
		}
		//IP_int2char (LAN_config.LAN_modem_IP, (ARP_answ_packet + 28) ); // ARP sender IP
		IP_int2char (ARP_requested_IP, (ARP_answ_packet + 28) ); // ARP sender IP
		for (i=0; i<6; i++) {				// ARP Target MAC
			ARP_answ_packet[i+32] = ARP_client_MAC[i];
		}
		IP_int2char (ARP_client_IP, (ARP_answ_packet + 38) ); //ARP target IP
		
		W5500_write_TX_buffer(W5500_p1, RAW_SOCKET, ARP_answ_packet, 42, 0);
	}
}

void ARP_client_request (unsigned long int IP_requested) {
	unsigned char ARP_packet[50] = {
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,	// dest MAC
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	// source MAC
		0x08, 0x06, 						// Ethtype ARP
		0x00, 0x01,							// HW type Ethernet
		0x08, 0x00, 						// protocol IPv4
		0x06, 								// HW addr size
		0x04, 								// protocol size
		0x00, 0x01	 						// ARP opcode "request"
	};
	int i;
	for (i=0; i<6; i++) { // source MAC
		ARP_packet[i+6] = CONF_modem_MAC[i];
	}
	for (i=0; i<6; i++) { // ARP sender MAC
		ARP_packet[i+22] = CONF_modem_MAC[i];
	}
	IP_int2char (LAN_conf_applied.LAN_modem_IP, (ARP_packet + 28) ); //ARP sender IP
	for (i=0; i<6; i++) { // target MAC : unknown
		ARP_packet[i+32] = 0;
	}
	IP_int2char (IP_requested, (ARP_packet + 38) ); //ARP Target IP
	W5500_write_TX_buffer(W5500_p1, RAW_SOCKET, ARP_packet, 42, 0);
}

void ARP_client_answer_treatment (unsigned char* ARP_RX_packet, int size) {
	unsigned long int ARP_sender_IP;
	unsigned long int ARP_target_IP;
	unsigned char ARP_sender_MAC[6];
	int entry_already_exists = 0;
	int i_existing_entry = 0;
	int i_free_entry = 255;
	int i;
	
	ARP_sender_IP = IP_char2int (ARP_RX_packet+28);
	ARP_target_IP = IP_char2int (ARP_RX_packet+38);
	for (i=0; i<6; i++) {
		ARP_sender_MAC[i] = ARP_RX_packet[22+i];
	}
	if (ARP_target_IP == LAN_conf_applied.LAN_modem_IP) { //if ARP reply is for us
		//first look for existing entry
		//printf("ARP reply is for us IP %X\r\n", ARP_sender_IP);
		entry_already_exists = 0;
		for (i=0; i<DHCP_ARP_tab_size; i++) {
			if (DHCP_ARP_IP[i] == ARP_sender_IP) {
				entry_already_exists = 1;
				i_existing_entry = i;
				//printf ("matches %i\r\n", i);
			}
		}
		if (entry_already_exists == 1) { // replace existing entry
			//printf("ARP entry already exists %i\r\n", i_existing_entry);
			for (i=0; i<6; i++) {
				DHCP_ARP_MAC[i_existing_entry][i] = ARP_sender_MAC[i];
			}
			DHCP_ARP_status[i_existing_entry] = 2; // valid entry
			DHCP_ARP_date[i_existing_entry] = GLOBAL_timer.read_us();
		} 
		else { // no entry, lookfor empty slot
			for (i=DHCP_ARP_tab_size-1; i>=0; i--) {
				if ( DHCP_ARP_status[i] == 0 ) {
					i_free_entry = i;
				}
			}
			if (i_free_entry < DHCP_ARP_tab_size) {// free entry exists
				for (i=0; i<6; i++) {
					DHCP_ARP_MAC[i_free_entry][i] = ARP_sender_MAC[i];
				}
				DHCP_ARP_status[i_free_entry] = 2; // valid entry
				DHCP_ARP_IP[i_free_entry] = ARP_sender_IP;
				DHCP_ARP_date[i_free_entry] = GLOBAL_timer.read_us();
			}
		}
	}
	flush_temp_Eth_buffer (ARP_sender_IP);
} 

void ARP_RX_packet_treatment (unsigned char* ARP_RX_packet, int size) {
	unsigned int ARP_protocol_type;
	unsigned int ARP_opcode;
	unsigned long int ARP_sender_IP;
	unsigned long int ARP_target_IP;
	ARP_protocol_type = ARP_RX_packet[16]*0x100 + ARP_RX_packet[17];
	ARP_opcode = ARP_RX_packet[20]*0x100 + ARP_RX_packet[21];
	if ( (ARP_protocol_type == 0x0800) && (ARP_opcode == 0x0001) ) { // request
		//printf ("ARP request\r\n");
		ARP_sender_IP = IP_char2int (ARP_RX_packet+28);
		ARP_target_IP = IP_char2int (ARP_RX_packet+38);
		if (ARP_sender_IP != ARP_target_IP) {//ignores gratuitous ARP request
			//printf ("real ARP request\r\n");
			ARP_proxy(ARP_RX_packet, size);
		}
		else {
			//printf ("gratuitous!\r\n");
		}
	}
	if ( (ARP_protocol_type == 0x0800) && (ARP_opcode == 0x0002) ) { // reply
		//printf ("ARP reply\r\n");
		if ( (LAN_conf_applied.DHCP_server_active == 0) || (is_TDMA_master) ){//only active if modem is no DHCP server
			ARP_client_answer_treatment (ARP_RX_packet, size);
		}
	}
}

void DHCP_ARP_periodic_free_table () {
	int i;
	unsigned int loc_age;
	for (i=0; i<DHCP_ARP_tab_size; i++) {
		loc_age = GLOBAL_timer.read_us() - DHCP_ARP_date[i];
		if ( (DHCP_ARP_status[i] == 2) && (loc_age > 1000000 * DHCP_ARP_timeout) ) {
			DHCP_ARP_status[i] = 3;
		}
	}
}

// *** generic functions ***
int lookfor_MAC_from_IP (unsigned char* MAC_out, unsigned long int IP_addr) {
	int result = 0; 
	int i;
	int i_found = 300;
	unsigned int age_loc=0;
	if ( (LAN_conf_applied.DHCP_server_active == 1) && (!is_TDMA_master) ) { //resolution for DHCP
		i_found = 300;
		for (i=0; i<DHCP_ARP_tab_size; i++) {
			if ( (DHCP_ARP_IP[i] == IP_addr) && (DHCP_ARP_status[i] == 2) ) {
				i_found = i;
			}
		}
		if (i_found < DHCP_ARP_tab_size) {
			for (i=0; i<6; i++) {
				MAC_out[i] = DHCP_ARP_MAC[i_found][i];
			}
			result = 1;
		}
	}
	else { // resolution for ARP (DHCP not active)
		i_found = 300;
		for (i=0; i<DHCP_ARP_tab_size; i++) {
			if ( (DHCP_ARP_IP[i] == IP_addr) && ( (DHCP_ARP_status[i] == 2) || (DHCP_ARP_status[i] == 3) ) ) {
				i_found = i;
			}
		}
		if (i_found < DHCP_ARP_tab_size) { // existing entry
			age_loc = GLOBAL_timer.read_us() - DHCP_ARP_date[i_found];
			age_loc = age_loc / 1000000;
			if (age_loc < (DHCP_ARP_timeout / 2) ) {
				for (i=0; i<6; i++) {
					MAC_out[i] = DHCP_ARP_MAC[i_found][i];
				}
				result = 1;
			}
			else if (age_loc < DHCP_ARP_timeout) {
				for (i=0; i<6; i++) {
					MAC_out[i] = DHCP_ARP_MAC[i_found][i];
				}
				result = 1;
				ARP_client_request(IP_addr);
			}
			else {
				ARP_client_request(IP_addr); //
				IP_addr = 0; 
				result = 0;
			}
		}
		else {//no entry found, send ARP request
			ARP_client_request(IP_addr); //
			IP_addr = 0; 
			result = 0;
		}
	}
	return result;	// 0 if no entry found yet (most often: ARP resolution not yet answered)
					// 1 if entry present
}