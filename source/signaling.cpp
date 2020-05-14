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

#include "signaling.h"
#include "mbed.h"
#include "Eth_IPv4.h"
#include "global_variables.h"
#include "L1L2_radio.h"
#include "DHCP_ARP.h"
#include "TDMA.h"
#include "HMI_telnet.h"

#include "ext_SRAM2.h"

static unsigned char rframe_TX[380];
static unsigned char TX_signal_frame_raw[260]; //300
static int TX_signal_frame_point = 0;

static int connect_state_machine_counter = 0;
static int time_counter_last_ack = 0; 

void signaling_frame_exploitation (unsigned char* unFECdata, int unFECsize, int TA_input) {
	int data_pos = 2;
	unsigned char field_type;
	unsigned char field_length;
	unsigned char local_ID;
	unsigned char* local_callsign;
	unsigned long int local_IP_start;
	unsigned long int local_IP_size;
	unsigned char local_static_alloc;
	unsigned char local_reason;
	unsigned char local_RSSI;
	unsigned short int local_BER;
	short int local_TA;
	//printf("signaling rx\r\n");
	do {
		field_type = unFECdata[data_pos];
		field_length = unFECdata[data_pos+1];
		switch (field_type) {
			case 0x01 : // WHOIS
				local_ID = unFECdata[data_pos + 2];
				local_callsign = unFECdata + data_pos + 3;
				local_callsign[15] = 0;//force null termination
				local_IP_start = IP_char2int (unFECdata + data_pos + 19);
				local_IP_size = IP_char2int (unFECdata + data_pos + 23);
				local_RSSI = unFECdata[data_pos + 27];
				local_BER = unFECdata[data_pos + 28] + (unFECdata[data_pos + 29] << 8); 
				local_TA = unFECdata[data_pos + 30] + (unFECdata[data_pos + 31] << 8); 
				signaling_whois_interpret (local_ID, local_callsign, 
					local_IP_start, local_IP_size,
					local_RSSI, local_BER, local_TA); 
				break; 
			case 0x05 : // request new connection
				local_callsign = unFECdata + data_pos + 2;
				local_callsign[15] = 0;
				local_IP_size = IP_char2int (unFECdata + data_pos + 18);
				local_static_alloc = unFECdata[data_pos + 22];
				if (is_TDMA_master) { 
					signaling_connect_req_process(local_callsign, local_IP_size, local_static_alloc, TA_input); 
				}
				break;
			case 0x06 : // ACK new connection
				local_callsign = unFECdata + data_pos + 3;
				local_callsign[15] = 0;
				if ( (is_TDMA_master == 0) && (strcmp ((char*)local_callsign, CONF_radio_my_callsign) == 0) ) {
					signaling_connect_ACK_process(unFECdata + data_pos + 2);
				}
				break;
			case 0x07 : // NACK new connection
				local_callsign = unFECdata + data_pos + 2;
				local_callsign[15] = 0;
				local_reason = unFECdata[data_pos + 18];
				if ( (is_TDMA_master == 0) && (strcmp ((char*)local_callsign, CONF_radio_my_callsign) == 0) ) {
					signaling_connect_NACK_process(local_reason);
				}
				break;
			case 0x0B : // Request disconnection
				local_ID = unFECdata[data_pos + 2];
				local_callsign = unFECdata + data_pos + 3;
				local_callsign[15] = 0;
				if (is_TDMA_master) {
					signaling_disconnect_req_process (local_ID, local_callsign);
				}
				break;
			case 0x0C : // ACK disconnection
				local_ID = unFECdata[data_pos + 2];
				local_callsign = unFECdata + data_pos + 3;
				local_callsign[15] = 0;
				if ( (is_TDMA_master == 0) && (strcmp ((char*)local_callsign, CONF_radio_my_callsign) == 0) ) {
					signaling_disconnect_ACK_process (local_ID, local_callsign);
				}
				break;
		} 
		data_pos = data_pos + field_length + 2;
		
	} while ( (field_type != 0xFF) && (data_pos < unFECsize ) );
	
}

void signaling_whois_interpret(unsigned char loc_ID, unsigned char* loc_callsign, 
	unsigned long int loc_IP_start, unsigned long int loc_IP_size, 
	unsigned char RSSI_loc, unsigned short int BER_loc, short int TA_loc) {
	unsigned char IP_start_c[5];
	
	IP_int2char (loc_IP_start, IP_start_c); 
	//printf("WHOIS ID:%i CALL:%s IP_start %i.%i.%i.%i IP_size %i\r\n", loc_ID, (char*)loc_callsign, 
	//	IP_start_c[0], IP_start_c[1], IP_start_c[2], IP_start_c[3], loc_IP_size);
	if (is_TDMA_master == 0) { //only useful for slaves
		if (loc_ID == 0x7F) { //who entry for master
			strcpy (CONF_radio_master_callsign, (char*)loc_callsign);		
		} 
		else if (loc_ID < radio_addr_table_size) {
			CONF_radio_addr_table_date[loc_ID] = GLOBAL_timer.read_us();
			CONF_radio_addr_table_status[loc_ID] = 2; //
			strcpy (CONF_radio_addr_table_callsign[loc_ID], (char*)loc_callsign);
			CONF_radio_addr_table_IP_begin[loc_ID] = loc_IP_start;
			CONF_radio_addr_table_IP_size[loc_ID] = loc_IP_size;
			G_radio_addr_table_RSSI[loc_ID] = RSSI_loc;
			G_radio_addr_table_BER[loc_ID] = BER_loc;
			TDMA_table_TA[loc_ID] = TA_loc;
		}
	}
}

unsigned long int signaling_lookfor_IP_range(unsigned long int req_size) {
	int i, j;
	unsigned long int answer = 0xFFFFFFFF;
	unsigned long int current_tested_pos;
	unsigned long int next_alloc_IP;
	
	current_tested_pos = CONF_radio_IP_start;
	next_alloc_IP = CONF_radio_IP_start + CONF_radio_IP_size;
	for (j=0; j<radio_addr_table_size; j++) {//looking for next allocated IP
		if ( (CONF_radio_addr_table_status[j]) && (CONF_radio_addr_table_IP_begin[j] >= current_tested_pos) && (CONF_radio_addr_table_IP_begin[j] < next_alloc_IP) ) {
			next_alloc_IP = CONF_radio_addr_table_IP_begin[j];
		}
	}
	if ( (next_alloc_IP - current_tested_pos) >= req_size) {
		answer = current_tested_pos; 
	}
	
	for (i=0; i<radio_addr_table_size; i++) {
		if (CONF_radio_addr_table_status[i]) {
			current_tested_pos = CONF_radio_addr_table_IP_begin[i] + CONF_radio_addr_table_IP_size[i];//beginning = end of previous
			if (current_tested_pos < answer) { //only continue search if IP tested is below already found
				next_alloc_IP = CONF_radio_IP_start + CONF_radio_IP_size;
				for (j=0; j<radio_addr_table_size; j++) {//looking for next allocated IP
					if ( (CONF_radio_addr_table_status[j]) && (CONF_radio_addr_table_IP_begin[j] >= current_tested_pos) && (CONF_radio_addr_table_IP_begin[j] < next_alloc_IP) ) {
						next_alloc_IP = CONF_radio_addr_table_IP_begin[j];
					}
				}
				if ( (next_alloc_IP - current_tested_pos) >= req_size) {
					answer = current_tested_pos; 
				}
			}
		}
		
	}
	return answer;
}

void signaling_connect_req_process (unsigned char* client_callsign, unsigned long int req_IP_size, unsigned char req_static_alloc, int TA_input) {
	int loc_ack = 0;
	int i;
	int existing_entry = -1;
	unsigned char client_ID = 0xFF;
	static unsigned char raw_answer[60];
	unsigned char NACK_reason;
	unsigned long int proposed_IP;
	unsigned char previous_status;
	
	// 1) look for existing entry for this callsign
	for (i=0; i<radio_addr_table_size; i++) {
		if (strcmp((char*)client_callsign, CONF_radio_addr_table_callsign[i]) == 0) {
			existing_entry = i;
		}
	}
	//printf ("connect req received\r\n");
	//printf ("found client entry %i\r\n", existing_entry);
	if (existing_entry != -1) {
		previous_status = CONF_radio_addr_table_status[existing_entry];
	}
	// 2) logic with 3 cases
	// 2.1) existing entry matches both callsign and size
	if ( (existing_entry != -1) && (req_IP_size == CONF_radio_addr_table_IP_size[existing_entry]) ) {
		client_ID = existing_entry;
		CONF_radio_addr_table_status[existing_entry] = 1;
		CONF_radio_addr_table_date[existing_entry] = GLOBAL_timer.read_us();
		loc_ack = 1; // Ack
		//printf("existing entry matches\r\n");
	}
	// 2.2) existing entry with different size
	if ( (existing_entry != -1) && (req_IP_size != CONF_radio_addr_table_IP_size[existing_entry]) ) {
		CONF_radio_addr_table_status[existing_entry] = 0; //frees existing entry
		proposed_IP = signaling_lookfor_IP_range (req_IP_size);
		if (proposed_IP != 0xFFFFFFFF) {// IP valid found
			client_ID = existing_entry;
			CONF_radio_addr_table_status[client_ID] = 1;
			CONF_radio_addr_table_IP_begin[client_ID] = proposed_IP;
			CONF_radio_addr_table_IP_size[client_ID] = req_IP_size;
			strcpy (CONF_radio_addr_table_callsign[client_ID], (char*)client_callsign);
			CONF_radio_addr_table_date[client_ID] = GLOBAL_timer.read_us();
			loc_ack = 1;
			//printf("existing entry doesnt match, ok new\r\n");
		} else { //no IP found
			NACK_reason = 0x02;
			loc_ack = 0;
			//printf("existing entry doesnt match, no more IP\r\n");
		}
	}
	if ( (existing_entry != -1) && (previous_status==0) && (loc_ack==1) ) {
		TDMA_init_TA(client_ID, TA_input);
	}
	// 2.3) no previous entry
	if (existing_entry == -1) {
		// lookfor empty entry
		for (i=radio_addr_table_size-1; i>=0; i--) {
			if (CONF_radio_addr_table_status[i] <= 0) {
				client_ID = i;
				//printf("client ID search %i\r\n", client_ID);
			}
		}
		if (client_ID == 0xFF) {
			NACK_reason = 0x03; //max nb of clients reached
			loc_ack = 0;
		} else {
			proposed_IP = signaling_lookfor_IP_range (req_IP_size);
			if (proposed_IP != 0xFFFFFFFF) {// IP valid found
				CONF_radio_addr_table_status[client_ID] = 1;
				CONF_radio_addr_table_IP_begin[client_ID] = proposed_IP;
				CONF_radio_addr_table_IP_size[client_ID] = req_IP_size;
				strcpy (CONF_radio_addr_table_callsign[client_ID], (char*)client_callsign);
				CONF_radio_addr_table_date[client_ID] = GLOBAL_timer.read_us();
				TDMA_init_TA(client_ID, TA_input);
				loc_ack = 1;
				//printf("new, alloc OK\r\n");
			} else { //no IP found
				NACK_reason = 0x02;
				loc_ack = 0;
				//printf("new, no more IP\r\n");
			}
		}
	}
	// 3)send answer to client
	if (loc_ack == 1) { // ACK
		raw_answer[0] = 0x06; //signaling type = connection acknowledge
		raw_answer[1] = 59; //size
		raw_answer[2] = client_ID;
		strcpy ((char*)(raw_answer + 3), (char*)client_callsign);					// Client Callsign
		IP_int2char (CONF_radio_addr_table_IP_begin[client_ID], raw_answer + 19); 	// IP start
		IP_int2char (CONF_radio_addr_table_IP_size[client_ID], raw_answer + 23);	// IP size
		strcpy ((char*)(raw_answer + 27), CONF_radio_my_callsign); 					// Master callsign
		IP_int2char (LAN_conf_applied.LAN_modem_IP, raw_answer + 43); 					// Modem IP
		IP_int2char (LAN_conf_applied.LAN_subnet_mask, raw_answer + 47); 					// IP subnet mask
		raw_answer[51] = LAN_conf_applied.LAN_def_route_activ;							// active default route
		IP_int2char (LAN_conf_applied.LAN_def_route, raw_answer + 52); 					// Default Route value
		raw_answer[56] = LAN_conf_applied.LAN_DNS_activ;									// Active DNS
		IP_int2char (LAN_conf_applied.LAN_DNS_value, raw_answer + 57); 					// DNS value
		signaling_TX_add_entry (raw_answer, 61);
		signaling_frame_push();
		//printf("\r\n ACK sent\r\n\r\n");
	}
	else { // NACK
		raw_answer[0] = 0x07; //signaling type = connection NACK
		raw_answer[1] = 33; //size
		strcpy ((char*)(raw_answer + 2), (char*)client_callsign);
		raw_answer[18] = NACK_reason;
		strcpy ((char*)(raw_answer + 19), CONF_radio_my_callsign); // Master callsign;
		signaling_TX_add_entry (raw_answer, 35);
		signaling_frame_push();
		//printf("\r\n NACK sent\r\n\r\n");
	}
}

void signaling_connect_ACK_process(unsigned char* raw_data) 
{
	unsigned char local_client_ID;
	unsigned long int local_IP_start;
	unsigned long int local_IP_size;
	unsigned long int local_modem_IP;
	unsigned long int local_IP_subnet;
	unsigned char local_default_route_activ;
	unsigned long int local_default_route;
	unsigned char local_DNS_activ;
	unsigned long int local_DNS_value;
	
	int need_LAN_reset = 0;
	//Client ID
	local_client_ID = raw_data[0];
	my_radio_client_ID = local_client_ID;
	TDMA_NULL_frame_init(70);
	// IP Start
	local_IP_start = IP_char2int(raw_data + 17);
	if (local_IP_start != LAN_conf_applied.DHCP_range_start) {need_LAN_reset = 1;}
	LAN_conf_applied.DHCP_range_start = local_IP_start;
	// IP Size
	local_IP_size = IP_char2int(raw_data + 21);
	if (local_IP_size != LAN_conf_applied.DHCP_range_size) {need_LAN_reset = 1;}
	LAN_conf_applied.DHCP_range_size = local_IP_size;
	// Master Callsign
	strcpy(CONF_radio_master_callsign, (char*)(raw_data + 25));
	// Modem IP
	local_modem_IP = IP_char2int(raw_data + 41);
	if (local_modem_IP != LAN_conf_applied.LAN_modem_IP) {need_LAN_reset = 1;}
	LAN_conf_applied.LAN_modem_IP = local_modem_IP;
	// IP subnet mask
	local_IP_subnet = IP_char2int(raw_data + 45);
	if (local_IP_subnet != LAN_conf_applied.LAN_subnet_mask) {need_LAN_reset = 1;}
	LAN_conf_applied.LAN_subnet_mask = local_IP_subnet;
	// Default route active
	local_default_route_activ = raw_data[49];
	if (local_default_route_activ != LAN_conf_applied.LAN_def_route_activ) {need_LAN_reset = 1;}
	LAN_conf_applied.LAN_def_route_activ = local_default_route_activ;
	// Default route value
	local_default_route = IP_char2int(raw_data + 50);
	if (local_default_route != LAN_conf_applied.LAN_def_route) {need_LAN_reset = 1;}
	LAN_conf_applied.LAN_def_route = local_default_route;
	// DNS active
	local_DNS_activ = raw_data[54];
	if (local_DNS_activ != LAN_conf_applied.LAN_DNS_activ) {need_LAN_reset = 1;}
	LAN_conf_applied.LAN_DNS_activ = local_DNS_activ;
	// DNS value
	local_DNS_value = IP_char2int(raw_data + 55);
	if (local_DNS_value != LAN_conf_applied.LAN_DNS_value) {need_LAN_reset = 1;}
	LAN_conf_applied.LAN_DNS_value = local_DNS_value;
	
	if (need_LAN_reset) {
		HMI_force_exit();
		reset_DHCP_table(&LAN_conf_applied);
		W5500_re_configure();
		need_LAN_reset = 0;
	}
	my_client_radio_connexion_state = 2;
	connect_state_machine_counter = 0;
	time_counter_last_ack = 0; 
	//printf("\r\n REQ ACK Received\r\n\r\n");
}

void signaling_connect_NACK_process (unsigned char reason_loc) {
	//printf("NACK reason: %i\r\n", reason_loc);
	G_connect_rejection_reason = reason_loc;
	my_client_radio_connexion_state = 3;
	connect_state_machine_counter = 0;
}

static unsigned char loc_data[40];

void signaling_connect_req_TX(void) {
	//static unsigned char loc_data[30];
	loc_data[0] = 0x05;	// signaling type = connection request
	loc_data[1] = 21; 	// field size
	strcpy ((char*)(loc_data+2), CONF_radio_my_callsign); // callsign
	IP_int2char (CONF_radio_IP_size_requested, loc_data + 18);
	loc_data[22] = CONF_radio_static_IP_requested;
	signaling_TX_add_entry (loc_data, 23);
	signaling_frame_push();
	//printf ("\r\nNEW REQUEST TX\r\n\r\n");
}

void signaling_disconnect_req_process (unsigned loc_ID, unsigned char* loc_callsign) {
	int i;
	int existing_entry = -1;
	for (i=0; i<radio_addr_table_size; i++) {
		if (strcmp((char*)loc_callsign, CONF_radio_addr_table_callsign[i]) == 0) {
			existing_entry = i;
		}
	}
	if (existing_entry != -1) {
		CONF_radio_addr_table_status[existing_entry] = 0;
	}
	signaling_disconnect_ACK_TX(loc_ID, loc_callsign);
	signaling_disconnect_ACK_TX(loc_ID, loc_callsign);//sends 2 times
}

void signaling_disconnect_ACK_process (unsigned loc_ID, unsigned char* loc_callsign) {
	my_client_radio_connexion_state = 5;
	my_radio_client_ID = 0x7E;
	TDMA_NULL_frame_init(70);
}

void signaling_disconnect_req_TX (void) {
	//static unsigned char loc_data[30]; // moved global
	loc_data[0] = 0x0B;	// signaling type = disconnect ACK
	loc_data[1] = 17; 	// field size
	loc_data[2] = my_radio_client_ID;
	strcpy ((char*)(loc_data+3), CONF_radio_my_callsign);
	signaling_TX_add_entry (loc_data, 19);
	signaling_frame_push();
}

void signaling_disconnect_ACK_TX (unsigned loc_ID, unsigned char* loc_callsign) {
	//static unsigned char loc_data[30]; // moved global
	loc_data[0] = 0x0C;	// signaling type = disconnect ACK
	loc_data[1] = 17; 	// field size
	loc_data[2] = loc_ID;
	strcpy ((char*)(loc_data+3), (char*)loc_callsign);
	signaling_TX_add_entry (loc_data, 19);
	signaling_frame_push();
}

// TX part
//void signaling_frame_init(unsigned char* TX_signal_frame_raw) {
void signaling_frame_init(void) {
	if (is_TDMA_master) {
		TX_signal_frame_raw[0] = 0xFF; //broadcast address (plus parity bit)
	} else {
		TX_signal_frame_raw[0] = my_radio_client_ID + parity_bit_elab[(my_radio_client_ID & 0x7F)]; //addr slave
	}
	TX_signal_frame_raw[1] = 0x1E; // protocol = signaling
	TX_signal_frame_point = 2;
}

//void signaling_single_whois_TX(unsigned char* TX_signal_frame_raw, unsigned char loc_ID, char* loc_callsign, 
void signaling_single_whois_TX(unsigned char loc_ID, char* loc_callsign, 
		unsigned long int loc_IP_start, unsigned long int loc_IP_size, 
		unsigned char RSSI_loc, unsigned short int BER_loc, short int TA_loc) {
	loc_data[0] = 0x01; //field type = who
	loc_data[1] = 30; // field length previous 30
	loc_data[2] = loc_ID;

	strcpy ((char*)(loc_data+3), loc_callsign);
	loc_data[18] = 0; //null termination of callsign
	IP_int2char (loc_IP_start, loc_data + 19); // IP start
	IP_int2char (loc_IP_size, loc_data + 23); // IP start
	loc_data[27] = RSSI_loc; // RSSI uplink
	loc_data[28] = BER_loc & 0xFF; // error rate uplink (lsB first)
	loc_data[29] = (BER_loc & 0xFF00) >> 8;
	loc_data[30] = TA_loc & 0xFF;
	loc_data[31] = (TA_loc & 0xFF00) >> 8;
	signaling_TX_add_entry (loc_data, 32); //previous 27
}

//void signaling_whois_TX(unsigned char* TX_signal_frame_raw) {
void signaling_whois_TX(void) {
	int i; 
	unsigned int RSSI_loc;
	if (is_TDMA_master) {
		signaling_single_whois_TX (0x7F, CONF_radio_my_callsign, LAN_conf_applied.LAN_modem_IP, 0, 0, 0, 0); // master entry
		for (i=0; i<radio_addr_table_size; i++) { // client entries
			if (CONF_radio_addr_table_status[i]) {
				RSSI_loc = (G_radio_addr_table_RSSI[i] & 0xFF00) >> 8;
				signaling_single_whois_TX (i, CONF_radio_addr_table_callsign[i], 
					CONF_radio_addr_table_IP_begin[i], CONF_radio_addr_table_IP_size[i], 
					RSSI_loc, G_radio_addr_table_BER[i], (short int)(TDMA_table_TA[i] / 10) );
			}
		}
		
	} 
	else if (my_client_radio_connexion_state == 2) { //Slave, only sends if really connected
		signaling_single_whois_TX (my_radio_client_ID, CONF_radio_my_callsign, LAN_conf_applied.DHCP_range_start, LAN_conf_applied.DHCP_range_size, G_downlink_RSSI, G_downlink_BER, 0); // client's own entry
		signaling_single_whois_TX (0x7F, CONF_radio_master_callsign, LAN_conf_applied.LAN_modem_IP, 0, 0, 0, 0); //MASTER entry
	}
}

void signaling_TX_add_entry(unsigned char* raw_data, int size) {
	if (TX_signal_frame_point == 0) {
		signaling_frame_init();
	}
	if ( (TX_signal_frame_point + size) < 248) { //enough space for this entry in current frame
		memcpy ((TX_signal_frame_raw + TX_signal_frame_point), raw_data, size);
		TX_signal_frame_point = TX_signal_frame_point + size;
	} else { // not enough space
		signaling_frame_push(); // send previous entries
		signaling_frame_init(); // initialize new frame
		memcpy ((TX_signal_frame_raw + TX_signal_frame_point), raw_data, size);
		TX_signal_frame_point = TX_signal_frame_point + size;
	}
}

//void signaling_frame_push(unsigned char* TX_signal_frame_raw) {
void signaling_frame_push(void) {
	int rsize_needed;
	int size_w_FEC;
	int size_wo_FEC;
	unsigned char rframe_length;
	unsigned int timer_snapshot;
	
	TX_signal_frame_raw[TX_signal_frame_point] = 0xFF; // END flag
	TX_signal_frame_raw[TX_signal_frame_point + 1] = 0x00; // size 0
	TX_signal_frame_point = TX_signal_frame_point + 2;
	size_wo_FEC = TX_signal_frame_point;
	if (size_wo_FEC < 69) {
		size_wo_FEC = 69;
	}
	rsize_needed = 100 + (size_wo_FEC * 1.4);

	//printf("sig NO ext SRAM\r\n");
	//if ((TXPS_FIFO->last_ready - TXPS_FIFO->RD_point) < (TXPS_FIFO_threshold_sig - rsize_needed) ) { //16380  
	if (TX_FIFO_full_global(0) == 0) {
		timer_snapshot = GLOBAL_timer.read_us();
		//TXPS_FIFO->data[TXPS_FIFO->WR_point & TXPS_FIFO->mask] = (timer_snapshot >> 16) & 0xFF; //timer 
		rframe_TX[0] = (timer_snapshot >> 16) & 0xFF;
		//TXPS_FIFO->WR_point++;
		size_w_FEC = size_w_FEC_compute (size_wo_FEC);
		
		rframe_length = size_w_FEC + 1 - SI4463_offset_size;
		//if (rframe_length < 0) {rframe_length = 0;}
		//TXPS_FIFO->data[TXPS_FIFO->WR_point & TXPS_FIFO->mask] = rframe_length; 
		//TXPS_FIFO->WR_point++;
		rframe_TX[1] = rframe_length;
		//TXPS_FIFO->data[TXPS_FIFO->WR_point & TXPS_FIFO->mask] = 0x00; //TDMA byte
		//TXPS_FIFO->WR_point++;
		rframe_TX[2] = 0x00; //TDMA byte
		//size_w_FEC = FEC_encode(TX_signal_frame_raw, TXPS_FIFO, size_wo_FEC);
		size_w_FEC = FEC_encode2(TX_signal_frame_raw, rframe_TX+3, size_wo_FEC);
		//TX_intern_FIFO_write(rframe_TX, size_w_FEC+3);
		TX_FIFO_write_global(rframe_TX, size_w_FEC+3);
		//TXPS_FIFO->last_ready = TXPS_FIFO->WR_point;
	}
	TX_signal_frame_point = 0;
}

void signaling_periodic_call() { // called every 2 to 6 seconds
	int i;
	unsigned int time_since_last_ack;
	unsigned int timer_snapshot;
	// CLIENT STATE MACHINE
	if (is_TDMA_master == 0) {
		if ( (my_client_radio_connexion_state==1) && (connect_state_machine_counter>2) ) {//waiting for connexion
			signaling_connect_req_TX();
			connect_state_machine_counter = 0;
			timer_snapshot = GLOBAL_timer.read_us();
			if ( (timer_snapshot - last_rframe_seen) > CONF_radio_timeout_small) {
				SI4463_prepa_TX_1_call.attach_us(&SI4463_prepa_TX_1, 500000);
			}
		}
		if ( (my_client_radio_connexion_state==2) && (connect_state_machine_counter>5) ) {//already connected periodic update
			signaling_connect_req_TX();
			// no counter reset, the ack reception does it
		}
		if ( (my_client_radio_connexion_state==3) && (connect_state_machine_counter>15) ) {//rejected, new attempt every 15
			signaling_connect_req_TX();
			connect_state_machine_counter = 0;
		}
		if ( (my_client_radio_connexion_state==2) && (time_counter_last_ack > connexion_timeout) ) {//timeout, no ACK received for long time
			// transition to state 1 "waiting for connection"
			my_client_radio_connexion_state = 1;
			radio_flush_TX_FIFO();
			my_radio_client_ID = 0x7E;
			TDMA_NULL_frame_init(70);
		}
		if ( (my_client_radio_connexion_state==4) && (connect_state_machine_counter>2) ) {//waiting for disconnection
			signaling_disconnect_req_TX();
			connect_state_machine_counter = 0;
		}
		connect_state_machine_counter++;
		time_counter_last_ack++;
	}
	
	// MASTER : timeout management for clients
	if (is_TDMA_master) {
		timer_snapshot = GLOBAL_timer.read_us();
		for (i=0; i< radio_addr_table_size; i++) {
			time_since_last_ack = timer_snapshot - CONF_radio_addr_table_date[i];
			if ( (CONF_radio_addr_table_status[i] == 1) && (time_since_last_ack > (2000000*connexion_timeout*CONF_signaling_period)) ) {
				CONF_radio_addr_table_status[i] = 0; // force disconnect
			}
		}
	}
	
	signaling_whois_TX();
	if (TX_signal_frame_point >0) {
		signaling_frame_push();
		
	}
}