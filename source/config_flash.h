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

#define NFPR_config_addr_begin 0x0803D000
#define CONFIG_MAGIC 0xbabebabe

// Note: 256 bytes long
typedef struct {
	uint32_t magic;
	uint32_t index;
	uint8_t  is_master;
	char     callsign[16];
	uint8_t  telnet_last_active;
	uint8_t  modulation;
	uint8_t  frequency;
	uint8_t  radio_network_id;
	uint8_t  client_static_IP;
	uint32_t client_req_size;
	uint8_t  dhcp_active;
	uint32_t modem_IP;
	uint32_t netmask;
	uint32_t IP_size;
	uint8_t  dns_active;
	uint32_t dns_value;
	uint8_t  def_route_active;
	uint32_t default_route;
	uint32_t IP_start;
	uint8_t  telnet_routed;
	uint16_t mac_ls_bytes;
	uint8_t  radio_on_at_start;
	uint8_t  rf_power;
	uint16_t checksum;
	uint16_t frequency_2;
	uint16_t shift;
	uint8_t  transmission_method;
	uint8_t  master_fdd;
	uint32_t master_fdd_down_IP;
	uint8_t static_client_entries[178];
} __attribute__((packed)) npr_config; // Ensure the structure is packed to have predictable results in the flash!

#define config_size sizeof(npr_config)

const npr_config NFPR_default_config {
	.magic = CONFIG_MAGIC,
	.index = 0x000000,
	.is_master = 0,
	.callsign = "N0CALL-1       ",
	.telnet_last_active = 1,
	.modulation = 24,
	.frequency = 175,
	.radio_network_id = 0,
	.client_static_IP = 0,
	.client_req_size = 0x00000001,
	.dhcp_active = 1,
	.modem_IP = 0xc0a800fd, // 192.168.0.253
	.netmask = 0xffffff00,
	.IP_size = 0x00000020, // 32
	.dns_active = 1,
	.dns_value = 0x09090909,
	.def_route_active = 1,
	.default_route = 0xc0a800fd, // 192.168.0.1
	.IP_start = 0xc0a80041,  // 192.168.0.65
	.telnet_routed = 1,
	.mac_ls_bytes = 0x0000,
	.radio_on_at_start = 0,
	.rf_power = 127,
	.checksum = 0,
	.frequency_2 = 0x4268,
	.shift = 0x0000,
	.transmission_method = 0,
	.master_fdd = 0,
	.master_fdd_down_IP = 0xc0a800fd, // 192.168.0.252
	.static_client_entries = {0}
};

unsigned int virt_EEPROM_read(npr_config* data);

unsigned char NFPR_random_generator(AnalogIn* analog_pin);

unsigned int virt_EEPROM_write(npr_config* in_data, unsigned int previous_index);

void virt_EEPROM_errase_all(void);

void virt_EEPROM_debug_read(void);

void apply_config_from_raw_string(npr_config* data_r);

void write_config_to_raw_string (npr_config* data_r);

void NFPR_config_read(AnalogIn* analog_pin);

unsigned int NFPR_config_save(void);

#endif