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

#ifndef Eth_IPv4_F4
#define Eth_IPv4_F4

#include "mbed.h"
#include "W5500.h"

unsigned long int IP_char2int(unsigned char* IP_char);

void IP_int2char (unsigned long int IP_int, unsigned char* IP_char);

//void init_RTP_filter(void);

//int PS_read_from_ethernet(W5500_chip* W5500);
int Eth_RX_dequeue (W5500_chip* W5500);

void Eth_pause_frame_TX(unsigned int time);

void IPv4_to_radio (unsigned char* RX_Eth_frame, int size);

void IPv4_from_radio (unsigned char* RX_eth_frame, int RX_size);

void flush_temp_Eth_buffer(unsigned long int loc_IP);

unsigned char lookfor_client_ID_from_IP(unsigned long int IP_addr);

#endif