// This file is part of "NPR70 modem firmware" software
// (A GMSK data modem for ham radio 430-440MHz, at several hundreds of kbps) 
// Copyright (c) 2017-2018 Guillaume F. F4HDK (amateur radio callsign)
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

#ifndef DHCP_ARP_F4
#define DHCP_ARP_F4

#include "mbed.h"
#include "W5500.h"
#include "global_variables.h"

void reset_DHCP_table(LAN_conf_T* LAN_config);

int lookfor_free_LAN_IP (LAN_conf_T* LAN_config, unsigned char* client_MAC, unsigned char* requested_IP, unsigned char* proposed_IP, int req_type);

void DHCP_server(LAN_conf_T* LAN_config, W5500_chip* W5500);

void DHCP_ARP_print_entries(void);

// *** ARP part *** 
void ARP_proxy (unsigned char* ARP_RX_packet);

void ARP_client_request (unsigned long int IP);

void ARP_client_answer_treatment (unsigned char* ARP_RX_packet, int size);

void ARP_RX_packet_treatment (unsigned char* ARP_RX_packet, int size);

int lookfor_MAC_from_IP (unsigned char* MAC_out, unsigned long int IP_addr);

void DHCP_ARP_periodic_free_table (void);

#endif