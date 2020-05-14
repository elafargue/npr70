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

static unsigned char raw_config_data[260];
static unsigned int config_index;

unsigned int virt_EEPROM_read(unsigned char* out_data) { //reads 256 Bytes of virtual eeprom data
	FlashIAP my_loc_flash;
	unsigned char loc_index_char[6];
	unsigned int loc_index_int, highest_index_seen;
	unsigned int loc_address;
	int i; 
	my_loc_flash.init();
	highest_index_seen = 0;
	//for (i=0; i<256; i++) {
	for (i=0; i<64; i++) {
		my_loc_flash.read(loc_index_char, NFPR_config_addr_begin+(i*256), 4);
		loc_index_int = (loc_index_char[0] << 24) + (loc_index_char[1] << 16) + (loc_index_char[2] << 8) + loc_index_char[3];
		if ( (loc_index_int != 0xFFFFFFFF) && (loc_index_int > highest_index_seen) ) {
			highest_index_seen = loc_index_int;
		}
	}
	if (highest_index_seen != 0) { //valid entry found
		//loc_address = NFPR_config_addr_begin + (highest_index_seen & 0xFF)*256;
		loc_address = NFPR_config_addr_begin + (highest_index_seen & 0x3F)*256;
		my_loc_flash.read(out_data, loc_address, 256);
	}
	my_loc_flash.deinit();
	return highest_index_seen;
}

unsigned int virt_EEPROM_write(unsigned char* in_data, unsigned int previous_index) {
	unsigned int new_index;
	unsigned int loc_address;
	FlashIAP my_loc_flash;
	if (previous_index == 0) {
		previous_index = 0xFF; //next index will be 0x100, errase first sector
	}
	new_index = previous_index + 1;
	//loc_address = NFPR_config_addr_begin + (new_index & 0xFF)*256; //previous config 64kB
	loc_address = NFPR_config_addr_begin + (new_index & 0x3F)*256; //previous config 
	my_loc_flash.init();
	if ((new_index & 7) == 0) { //new sector, errase sector
		HMI_printf ("errase sector:%X\r\n", loc_address);
		my_loc_flash.erase(loc_address, 2048);
	}
	// writes new index
	in_data[0] = (new_index & 0xFF000000) >> 24;
	in_data[1] = (new_index & 0xFF0000) >> 16;
	in_data[2] = (new_index & 0xFF00) >> 8;
	in_data[3] = new_index & 0xFF;
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
	int i;
	unsigned char default_config[260] = NFPR_default_config;
	config_index = virt_EEPROM_read(raw_config_data);
	if (config_index == 0) { //no previous config found
		for (i=0; i<256; i++) {
			raw_config_data[i] = default_config[i];
		}
		//MAC random 2 LSB values
		raw_config_data[58] = NFPR_random_generator(analog_pin);
		raw_config_data[59] = NFPR_random_generator(analog_pin);
		raw_config_data[5] = raw_config_data[58];//callsign 1st char
		raw_config_data[6] = raw_config_data[59];//callsign 2nd char
		config_index = virt_EEPROM_write (raw_config_data, config_index);//save the MAC
	}
	apply_config_from_raw_string(raw_config_data);
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
		write_config_to_raw_string(raw_config_data);
		config_index = virt_EEPROM_write (raw_config_data, config_index);
	}
	return config_index;
}

void apply_config_from_raw_string(unsigned char* data_r) {
	int i;
	unsigned char modul_temp;
	is_TDMA_master = (data_r[4] == 1);
	for (i=0; i<16; i++) {
		CONF_radio_my_callsign[i] = data_r[5+i];
	}
	CONF_radio_my_callsign[15] = 0;
	is_telnet_active = data_r[21];
	modul_temp = (data_r[22] & 0x3F);
	if ( ((modul_temp>=11)&&(modul_temp<=14)) || ((modul_temp>=20)&&(modul_temp<=24)) ) {
		CONF_radio_modulation = modul_temp;
	} else {
		CONF_radio_modulation = 24;
	}
	CONF_frequency_band = (data_r[22] & 0xC0) >> 6;
	//printf("freq_band:%X modul:%i\r\n", CONF_frequency_band, modul_temp);
	CONF_radio_frequency = data_r[23];
	CONF_radio_network_ID = data_r[24];
	
	//specific for clients
	CONF_radio_static_IP_requested = data_r[25];
	CONF_radio_IP_size_requested = IP_char2int(data_r+26);
	LAN_conf_saved.DHCP_server_active = data_r[30];
	LAN_conf_applied.DHCP_server_active = data_r[30];
	
	//specific for master
	LAN_conf_saved.LAN_modem_IP = IP_char2int(data_r+31);
	LAN_conf_applied.LAN_modem_IP = IP_char2int(data_r+31);
	LAN_conf_saved.LAN_subnet_mask = IP_char2int(data_r+35);
	LAN_conf_applied.LAN_subnet_mask = IP_char2int(data_r+35);
	CONF_radio_IP_size = IP_char2int(data_r+39);
	LAN_conf_saved.LAN_DNS_activ = data_r[43];
	LAN_conf_applied.LAN_DNS_activ = data_r[43];
	LAN_conf_saved.LAN_DNS_value = IP_char2int(data_r+44);
	LAN_conf_applied.LAN_DNS_value = IP_char2int(data_r+44);
	LAN_conf_saved.LAN_def_route_activ = data_r[48];
	LAN_conf_applied.LAN_def_route_activ = data_r[48];
	LAN_conf_saved.LAN_def_route = IP_char2int(data_r+49);
	LAN_conf_applied.LAN_def_route = IP_char2int(data_r+49);
	CONF_radio_IP_start = IP_char2int(data_r+53);
	is_telnet_routed = data_r[57];
	CONF_modem_MAC[0] = 0x4E;//N
	CONF_modem_MAC[1] = 0x46;//F
	CONF_modem_MAC[2] = 0x50;//P
	CONF_modem_MAC[3] = 0x52;//R
	CONF_modem_MAC[4] = data_r[58];
	CONF_modem_MAC[5] = data_r[59];
	CONF_radio_default_state_ON_OFF = data_r[60];
	CONF_radio_PA_PWR = data_r[61];
	
	CONF_frequency_HD = ((data_r[64]) <<8 ) | data_r[65];
	if ( (CONF_frequency_HD == 0x0000) || (CONF_frequency_HD > FREQ_MAX_RAW) ) {
		CONF_frequency_HD = CONF_DEF_FREQ; // force to default frequency
	}
	CONF_freq_shift = ((data_r[66]) <<8) | data_r[67];
	CONF_transmission_method = data_r[68];
	CONF_master_FDD = data_r[69];
	CONF_master_down_IP = IP_char2int(data_r+70);
	
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

void write_config_to_raw_string (unsigned char* data_r) {
	int i;
	data_r[4] = is_TDMA_master;
	for (i=0; i<16; i++) {
		data_r[5+i] = CONF_radio_my_callsign[i];
	}
	data_r[21] = is_telnet_active;
	data_r[22] = ( (CONF_frequency_band << 6) & 0xC0) + (CONF_radio_modulation & 0x3F);
	data_r[23] = CONF_radio_frequency;
	data_r[24] = CONF_radio_network_ID;
	
	//specific for clients
	data_r[25] = CONF_radio_static_IP_requested;
	IP_int2char(CONF_radio_IP_size_requested, data_r+26); 
	data_r[30] = LAN_conf_saved.DHCP_server_active;
	
	//specific for master
	IP_int2char(LAN_conf_saved.LAN_modem_IP, data_r+31);
	IP_int2char(LAN_conf_saved.LAN_subnet_mask, data_r+35);
	IP_int2char(CONF_radio_IP_size, data_r+39);
	data_r[43] = LAN_conf_saved.LAN_DNS_activ;
	IP_int2char(LAN_conf_saved.LAN_DNS_value, data_r+44);
	data_r[48] = LAN_conf_saved.LAN_def_route_activ;
	IP_int2char(LAN_conf_saved.LAN_def_route, data_r+49);
	IP_int2char(CONF_radio_IP_start, data_r+53);
	data_r[57] = is_telnet_routed;
	data_r[58] = CONF_modem_MAC[4];
	data_r[59] = CONF_modem_MAC[5];
	data_r[60] = CONF_radio_default_state_ON_OFF;
	data_r[61] = CONF_radio_PA_PWR;
	
	data_r[64] = (CONF_frequency_HD & 0xFF00)>>8;
	data_r[65] = (CONF_frequency_HD & 0x00FF);
	data_r[66] = (CONF_freq_shift & 0xFF00)>>8;
	data_r[67] = (CONF_freq_shift & 0x00FF);
	data_r[68] = CONF_transmission_method;
	data_r[69] = CONF_master_FDD;
	IP_int2char(CONF_master_down_IP, data_r+70);
}