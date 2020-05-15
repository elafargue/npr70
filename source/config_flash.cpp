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

#include "config_flash.h"
#include "mbed.h"
#include "global_variables.h"
#include "Eth_IPv4.h"
#include "HMI_telnet.h"
#include "TDMA.h"

static npr_config raw_config_data; // 256 bytes long
static unsigned int config_index;

// Flash storage is organized in 16 banks of 256 bytes
#define FLASH_SETTINGS_SIZE 256
#define FLASH_BANKS      16

unsigned int virt_EEPROM_read(npr_config* out_data) { //reads 256 Bytes of virtual eeprom data
	FlashIAP my_loc_flash;
	uint32_t loc_index_int, highest_index_seen;
	unsigned int loc_address;
	uint32_t magic = 0;
	int i; 
	my_loc_flash.init();
	highest_index_seen = 0;
	for (i=0; i< FLASH_BANKS; i++) {
		my_loc_flash.read(&magic, NFPR_config_addr_begin+(i*FLASH_SETTINGS_SIZE), 4);
		if (magic == CONFIG_MAGIC) { // Valid start of config
			my_loc_flash.read(&loc_index_int, NFPR_config_addr_begin+(i*FLASH_SETTINGS_SIZE) + 4, 4);
			if ( (loc_index_int != 0xFFFFFFFF) && (loc_index_int > highest_index_seen) ) {
				highest_index_seen = loc_index_int;
			}
		}
	}
	if (highest_index_seen != 0) { //valid entry found
		loc_address = NFPR_config_addr_begin + (highest_index_seen % FLASH_BANKS )*FLASH_SETTINGS_SIZE;
		my_loc_flash.read(out_data, loc_address, FLASH_SETTINGS_SIZE);
	}
	my_loc_flash.deinit();
	return highest_index_seen;
}

unsigned int virt_EEPROM_write(npr_config* in_data, unsigned int previous_index) {
	uint32_t new_index;
	unsigned int loc_address;
	FlashIAP my_loc_flash;
	if (previous_index == 0) {
		previous_index = 0xFF; //next index will be 0x100, errase first sector
	}
	new_index = previous_index + 1;
	loc_address = NFPR_config_addr_begin + (new_index % FLASH_BANKS)*256; //previous config 
	my_loc_flash.init();
	if ((new_index & 7) == 0) { //new sector, erase sector
		HMI_printf ("erase sector:%X\r\n", loc_address);
		my_loc_flash.erase(loc_address, 2048);
	}
	// Make sure magic number is correct to detect a valid configuration block
	in_data->magic = CONFIG_MAGIC;
	// writes new index
	in_data->index = new_index;
	my_loc_flash.program(in_data, loc_address, 256);
	HMI_printf("write success\r\n");
	my_loc_flash.deinit();
	return new_index;
}

//void virt_EEPROM_debug_read(void) {
//	int i;
//	unsigned int loc_address;
//	unsigned char loc_data[260];
//	FlashIAP my_loc_flash;
//	my_loc_flash.init();
//	//for (i=0; i<256; i++) {
//	for (i=0; i<64; i++) {
//		//loc_address = NFPR_config_addr_begin + (i & 0xFF)*256;
//		loc_address = NFPR_config_addr_begin + (i & 0x3F)*256;
//		my_loc_flash.read(loc_data, loc_address, 256);
//		HMI_printf("addr:%X data:%02X %02X %02X %02X %02X %02X %02X %02X\r\n", loc_address, loc_data[0], loc_data[1], loc_data[2], loc_data[3], loc_data[4], loc_data[5], loc_data[6], loc_data[7]);
//		wait_ms(10);
//	}
//	my_loc_flash.deinit();
//}

void virt_EEPROM_errase_all(void) {
	FlashIAP my_loc_flash;
	my_loc_flash.init();
	//my_loc_flash.erase(NFPR_config_addr_begin, 0x10000); //previous config 64kB
	my_loc_flash.erase(NFPR_config_addr_begin, 0x4000); //config 16kB
	my_loc_flash.deinit();
}

// higher level functions

void NFPR_config_read(AnalogIn* analog_pin) {
	config_index = virt_EEPROM_read(&raw_config_data);
	if (config_index == 0) { //no previous config found
		memcpy(&raw_config_data, &NFPR_default_config, 256);
		//MAC random 2 LSB values
		raw_config_data.mac_ls_bytes = (NFPR_random_generator(analog_pin) << 8) + NFPR_random_generator(analog_pin);
		// raw_config_data[9] = raw_config_data[62];//callsign 1st char
		// raw_config_data[10] = raw_config_data[63];//callsign 2nd char
		config_index = virt_EEPROM_write (&raw_config_data, config_index);//save the MAC
	}
	apply_config_from_raw_string(&raw_config_data);
	if (is_TDMA_master) {
		my_client_radio_connexion_state = 2;
	} else {
		my_client_radio_connexion_state = 1;
		my_radio_client_ID = 0x7E;
	}
}

unsigned char NFPR_random_generator(AnalogIn* analog_pin) {
	unsigned short interm_random;
	unsigned char random_8;
	int i;
	random_8 = 0;
	for (i=0; i<8; i++) {
		interm_random = analog_pin->read_u16();
		interm_random = (interm_random & 0x10)>>4;
		interm_random = (interm_random << i);
		random_8 = random_8 + interm_random;
		wait_ms(4);
	}
	return random_8;
}

unsigned int NFPR_config_save(void) {
	if ( (CONF_radio_my_callsign[0] == 0) || (CONF_radio_my_callsign[2] == 0) ) {
		HMI_printf("ERROR : not yet configured\r\n");		
	} else {
		write_config_to_raw_string(&raw_config_data);
		config_index = virt_EEPROM_write (&raw_config_data, config_index);
	}
	return config_index;
}

void apply_config_from_raw_string(npr_config* data_r) {
	unsigned char modul_temp;
	is_TDMA_master = (data_r->is_master == 1);
	strncpy(CONF_radio_my_callsign, data_r->callsign, 15);
	CONF_radio_my_callsign[15] = 0;
	is_telnet_active = data_r->telnet_last_active;
	modul_temp = (data_r->modulation & 0x3F);
	if ( ((modul_temp>=11)&&(modul_temp<=14)) || ((modul_temp>=20)&&(modul_temp<=24)) ) {
		CONF_radio_modulation = modul_temp;
	} else {
		CONF_radio_modulation = 24;
	}
	CONF_frequency_band = (data_r->modulation & 0xC0) >> 6;
	//printf("freq_band:%X modul:%i\r\n", CONF_frequency_band, modul_temp);
	CONF_radio_frequency = data_r->frequency;
	CONF_radio_network_ID = data_r->radio_network_id;
	
	//specific for clients
	CONF_radio_static_IP_requested = data_r->client_static_IP;
	CONF_radio_IP_size_requested = data_r->client_req_size;
	LAN_conf_saved.DHCP_server_active = data_r->dhcp_active;
	LAN_conf_applied.DHCP_server_active = data_r->dhcp_active;
	
	//specific for master
	LAN_conf_saved.LAN_modem_IP = data_r->modem_IP;
	LAN_conf_applied.LAN_modem_IP = data_r->modem_IP;
	LAN_conf_saved.LAN_subnet_mask = data_r->netmask;
	LAN_conf_applied.LAN_subnet_mask = data_r->netmask;
	CONF_radio_IP_size = data_r->IP_size;
	LAN_conf_saved.LAN_DNS_activ = data_r->dns_active;
	LAN_conf_applied.LAN_DNS_activ = data_r->dns_active;
	LAN_conf_saved.LAN_DNS_value = data_r->dns_value;
	LAN_conf_applied.LAN_DNS_value = data_r->dns_value;
	LAN_conf_saved.LAN_def_route_activ = data_r->def_route_active;
	LAN_conf_applied.LAN_def_route_activ = data_r->def_route_active;
	LAN_conf_saved.LAN_def_route = data_r->default_route;
	LAN_conf_applied.LAN_def_route = data_r->default_route;
	CONF_radio_IP_start = data_r->IP_start;
	is_telnet_routed = data_r->telnet_routed;
	CONF_modem_MAC[0] = 0x4E;//N
	CONF_modem_MAC[1] = 0x46;//F
	CONF_modem_MAC[2] = 0x50;//P
	CONF_modem_MAC[3] = 0x52;//R
	CONF_modem_MAC[4] = data_r->mac_ls_bytes >> 8;
	CONF_modem_MAC[5] = data_r->mac_ls_bytes & 0xff;
	CONF_radio_default_state_ON_OFF = data_r->radio_on_at_start;
	CONF_radio_PA_PWR = data_r->rf_power;
	
	CONF_frequency_HD = data_r->frequency_2;
	if ( (CONF_frequency_HD == 0x0000) || (CONF_frequency_HD > FREQ_MAX_RAW) ) {
		CONF_frequency_HD = CONF_DEF_FREQ; // force to default frequency
	}
	CONF_freq_shift = data_r->shift;
	CONF_transmission_method = data_r->transmission_method;
	CONF_master_FDD = data_r->master_fdd;
	CONF_master_down_IP = data_r->master_fdd_down_IP;
	
	if (LAN_conf_applied.DHCP_server_active == 1) {
		LAN_conf_applied.DHCP_range_start = CONF_radio_IP_start;
		LAN_conf_applied.DHCP_range_size = CONF_radio_IP_size_requested;
		
	}
	if ( (is_TDMA_master) && (CONF_master_FDD == 1) ) { // FDD Master down
		G_FDD_trig_pin->output();
	}
	if ( (is_TDMA_master) && (CONF_master_FDD == 2) ) {// FDD master up
		G_FDD_trig_IRQ->rise(&TDMA_FDD_up_top_measure);
	}
}

void write_config_to_raw_string (npr_config* data_r) {
	int i;
	data_r->is_master = is_TDMA_master;
	strncpy(data_r->callsign, CONF_radio_my_callsign, 15);
	data_r->telnet_last_active = is_telnet_active;
	data_r->modulation = ( (CONF_frequency_band << 6) & 0xC0) + (CONF_radio_modulation & 0x3F);
	data_r->frequency = CONF_radio_frequency;
	data_r->radio_network_id = CONF_radio_network_ID;
	
	//specific for clients
	data_r->client_static_IP = CONF_radio_static_IP_requested;
	data_r->client_req_size = CONF_radio_IP_size_requested;
	data_r->dhcp_active = LAN_conf_saved.DHCP_server_active;
	
	//specific for master
	data_r->modem_IP = LAN_conf_saved.LAN_modem_IP;
	data_r->netmask = LAN_conf_saved.LAN_subnet_mask;
	data_r->IP_size = CONF_radio_IP_size;
	data_r->dns_active = LAN_conf_saved.LAN_DNS_activ;
	data_r->dns_value = LAN_conf_saved.LAN_DNS_value;
	data_r->def_route_active =LAN_conf_saved.LAN_def_route_activ;
	data_r->default_route = LAN_conf_saved.LAN_def_route;
	data_r->IP_start = CONF_radio_IP_start;
	data_r->telnet_routed =is_telnet_routed;
	data_r->mac_ls_bytes = CONF_modem_MAC[4] * 256 + CONF_modem_MAC[5];
	data_r->radio_on_at_start = CONF_radio_default_state_ON_OFF;
	data_r->rf_power = CONF_radio_PA_PWR;
	
	data_r->frequency_2 = CONF_frequency_HD;
	data_r->shift = CONF_freq_shift;
	data_r->transmission_method = CONF_transmission_method;
	data_r->master_fdd= CONF_master_FDD;
	data_r->master_fdd_down_IP = CONF_master_down_IP;
}