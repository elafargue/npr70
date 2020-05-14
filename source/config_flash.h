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

#ifndef CONFIG_FLASH_F4
#define CONFIG_FLASH_F4

#include "mbed.h"

//#define NFPR_config_addr_begin 0x08030000
#define NFPR_config_addr_begin 0x0803C000

#define NFPR_default_config { \
	0,0,0,0, /*		index*/\
	0, /*			is_master*/\ 
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /*callsign*/\ 
	1, /*			telnet_active*/\
	24, /*			modulation*/\
	175, /*			frequency (175 = 437MHz)*/\
	0, /*			radio_netw_ID*/\
	0, /*			client_static_IP*/\
	0,0,0,1, /*		client_req_size*/\
	1, /*			DHCP server active*/\
	192,168,0,253, /*	modem_IP*/\
	255,255,255,0, /*netmask*/\
	0,0,0,32, /*	IP_size*/\
	1, /*			DNS_active*/\
	9,9,9,9, /*		DNS_value*/\
	1, /*			def_route_active*/\
	192,168,0,1, /*	def_route_val*/\
	192,168,0,65, /*IP_begin*/\
	1, /*			telnet_routed*/\
	0,0, /*			MAC 2 LS bytes*/\
	0, /*			radio_on_at_start*/\
	127, /*			RF_power*/\
	0,0, /*			checksum*/\
	66, 104, /*		frequency MSB LSB 437.000*/\
	00, 00,  /*		frequency shift MSB LSB 0*/\
	0, /*			transmission method 0=IP 1=Ethernet*/\
	0, /*			master_FDD 0=no 1=down 2=up*/\
	192,168,0,252,/*master_FDD_down_IP*/\
	0,0,0,0,0,0,0,0,0,0,0,0,0,0, /*static client 0 24 entries*/\
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, \
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, \
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, \
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, \
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, \
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, \
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 /*static client 7*/\
}

unsigned int virt_EEPROM_read(unsigned char* data);

unsigned char NFPR_random_generator(AnalogIn* analog_pin);

unsigned int virt_EEPROM_write(unsigned char* in_data, unsigned int previous_index);

void virt_EEPROM_errase_all(void);

void virt_EEPROM_debug_read(void);

void apply_config_from_raw_string(unsigned char* data_r);

void write_config_to_raw_string (unsigned char* data_r);

void NFPR_config_read(AnalogIn* analog_pin);

unsigned int NFPR_config_save(void);

#endif