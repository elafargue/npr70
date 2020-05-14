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

#ifndef SIGNALING_F4
#define SIGNALING_F4

#include "mbed.h"
#define connexion_timeout 10
//multiple of signaling_period 10sec or 20sec or 30sec

void signaling_frame_exploitation (unsigned char* unFECdata, int unFECsize, int TA_input);

void signaling_whois_interpret(unsigned char loc_ID, unsigned char* loc_callsign, 
	unsigned long int loc_IP_start, unsigned long int loc_IP_size, 
	unsigned char RSSI_loc, unsigned short int BER_loc, short int TA_loc);
	
void signaling_print_who(void);

unsigned long int signaling_lookfor_IP_range(unsigned long int req_size);

void signaling_connect_req_process (unsigned char* client_callsign, unsigned long int req_IP_size, unsigned char req_static_alloc, int TA_input);

void signaling_connect_ACK_process(unsigned char* raw_data);

void signaling_connect_NACK_process (unsigned char reason_loc);

void signaling_connect_req_TX(void);

void signaling_disconnect_req_process (unsigned loc_ID, unsigned char* loc_callsign);

void signaling_disconnect_ACK_process (unsigned loc_ID, unsigned char* loc_callsign);

void signaling_disconnect_ACK_TX (unsigned loc_ID, unsigned char* loc_callsign);

void signaling_frame_init();

void signaling_single_whois_TX(unsigned char loc_ID, char* loc_callsign, 
		unsigned long int loc_IP_start, unsigned long int loc_IP_size, 
		unsigned char RSSI_loc, unsigned short int BER_loc, short int TA_loc);
		
void signaling_TX_add_entry(unsigned char* raw_data, int size);
		
void signaling_whois_TX();

void signaling_frame_push();
	
void signaling_periodic_call();
	
#endif