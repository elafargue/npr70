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

#include "HMI_telnet.h"
#include "mbed.h"
#include "global_variables.h"
#include "Eth_IPv4.h"
#include "signaling.h"
#include "config_flash.h"
#include "W5500.h"
#include "SI4463.h"
#include "TDMA.h"
#include "DHCP_ARP.h"
#include "L1L2_radio.h"

static char current_rx_line[100];
//static char HMI_out_str[100];
static int current_rx_line_count = 0;
static int is_telnet_opened = 0;
static int echo_ON = 1; 
static int display_status_ongoing = 0;
static int display_who_ongoing = 0;
static int slow_counter = 0;

static unsigned int telnet_last_activity;

/**
 * Called regularly by the main loop, and manages network events (new connection,
 * data, etc)
 */
int telnet_loop (W5500_chip* W5500) {
	static unsigned char previous_state = 0;
	uint8_t RX_data[100];
	uint8_t TX_data[100];
	uint8_t current_state; // Socket state as returned by the W5500
	unsigned int timer_snapshot;
	char loc_char;
	int RX_size = 0;
	int i, j;
	int result;

	result=0;
	
	current_state = W5500_read_byte(W5500, W5500_Sn_SR, TELNET_SOCKET);
	//printf("state: %x\r\n", current_state);
	if ((current_state == W5500_SOCK_ESTABLISHED) && (previous_state != W5500_SOCK_ESTABLISHED)) {
		W5500_read_long(W5500, W5500_Sn_DIPR0, TELNET_SOCKET, RX_data, 4);
		printf("\r\n\r\nnew telnet connexion from %i.%i.%i.%i\r\nserial inactive...\r\n", RX_data[0], RX_data[1], RX_data[2], RX_data[3]);
		fflush(stdout);
		//TX_data[0] = 0xFF; //IAC
		//TX_data[1] = 0xFB; //WILL FB DO FD
		//TX_data[2] = 1; //echo 1 RTCE 7
		//TX_data[3] = 0;
		TX_data[0] = 0xFF; //IAC
		TX_data[1] = 0xFB; //WILL 
		TX_data[2] = 0x01; //Echo
		//TX_data[3] = 0;
		TX_data[3] = 0xFF; //IAC
		TX_data[4] = 0xFD; //DO
		TX_data[5] = 0x03; //Suppr GA 
		TX_data[6] = 0xFF; //IAC
		TX_data[7] = 0xFB; //WILL
		TX_data[8] = 0x03; //Suppr GA 
		TX_data[9] = 0;
		strcat((char*)TX_data, "NPR modem\r\nready> ");
		W5500_write_TX_buffer (W5500, TELNET_SOCKET, TX_data, 27, 0); //27
		//HMI_printf("ready>");
		is_telnet_opened = 1;
		current_rx_line_count = 0;
		echo_ON = 1;
		display_status_ongoing = 0;
		display_who_ongoing = 0;
		telnet_last_activity = GLOBAL_timer.read_us();
	}
	
	if (current_state == W5500_SOCK_WAIT) { // close wait to close 
		W5500_write_byte(W5500, W5500_Sn_CR, TELNET_SOCKET, 0x10); 
		printf("telnet connexion closed\r\nready> "); 
		fflush(stdout);
		is_telnet_opened = 0;
		current_rx_line_count = 0;
		echo_ON = 1;
		display_status_ongoing = 0;
		display_who_ongoing = 0;
	}
	
	if (current_state == W5500_SOCK_CLOSED) { //closed to open
		W5500_write_byte(W5500, W5500_Sn_CR, TELNET_SOCKET, 0x01); 
		//printf("open \r\n");
		result=1;
	}
	
	if (current_state == W5500_SOCK_INIT) { //opened to listen
		W5500_write_byte(W5500, W5500_Sn_CR, TELNET_SOCKET, 0x02); 
		//printf("listen \r\n");
	}
	previous_state = current_state;
	if (is_telnet_opened) {
		RX_size = W5500_read_received_size(W5500, TELNET_SOCKET); 
		timer_snapshot = GLOBAL_timer.read_us();
		if ((timer_snapshot - telnet_last_activity) > 300000000) { //300000000
			//HMI_printf("Telnet inactivity timeout. Force exit.\r\n");
			W5500_write_byte(W5500_p1, W5500_Sn_CR, TELNET_SOCKET, 0x08); //close TCP
			is_telnet_opened = 0;
			echo_ON = 1;
			display_status_ongoing = 0;
			display_who_ongoing = 0;
			printf("telnet connexion closed\r\nready> "); 
			fflush(stdout);
		}
		//timeout 
	}
	if (RX_size > 0) {
		telnet_last_activity = GLOBAL_timer.read_us();
		result=1;
		W5500_read_RX_buffer(W5500, TELNET_SOCKET, RX_data, RX_size);
		RX_data[RX_size] = 0;
		i = 0;
		j = 0;
		while (i < RX_size) {
			loc_char = (char)RX_data[i];
			//printf("%02X %c\r\n", loc_char, loc_char);
			if ( (loc_char >= 0x20) && (loc_char <= 0x7E) ) {//displayable char
				if ( (current_rx_line_count < 98) && (echo_ON) ) {
					TX_data[j]=RX_data[i];
					i++;
					j++;
					current_rx_line[current_rx_line_count] = loc_char;
					current_rx_line_count++;
				} else {
					i++;
				}
			} 
			else { // special char
				if (loc_char == 0xFF) {//IAC
					if (RX_data[i+1] == 244) {//ctrl+C
						HMI_cancel_current();
					}
					i = i + 3;
				}
				else if ( ( (loc_char == 0x08) || (loc_char == 0x7F) ) && (echo_ON) ) { //backspace
					i++;
					
					if (current_rx_line_count>0) {
						current_rx_line_count--;
						TX_data[j] = 0x08;
						TX_data[j+1] = 0x20;
						TX_data[j+2] = 0x08;
						j=j+3;
					}
				}
				else if ( (loc_char == 0x0D) && (echo_ON) ){ //end of line
					TX_data[j] = 0x0D;
					TX_data[j+1] = 0x0A;
					i++;
					j = j + 2;
					current_rx_line[current_rx_line_count] = 0;//null termination
					current_rx_line_count++;
					W5500_write_TX_buffer (W5500, TELNET_SOCKET, TX_data, j, 0);
					j = 0;
					HMI_line_parse (current_rx_line, current_rx_line_count);
					current_rx_line_count = 0;
				}
				else if (loc_char == 0x03) { //ctrl + C
					HMI_cancel_current();
					//printf("CTRL + C\r\n");
					i++;
				} else {
					i++;
				}
			}
		}
		if (j > 0) {
			W5500_write_TX_buffer (W5500, TELNET_SOCKET, TX_data, j, 0);
		}
		//printf("\r\n");
	}
	return result;
}

int serial_term_loop (void) {
	char loc_char;
	
	if (pc.readable()) {
		loc_char = getc(pc);
		
		if (is_telnet_opened == 0) {
			if ( (loc_char >= 0x20) && (loc_char <= 0x7E) ) {//printable char
				if ( (current_rx_line_count < 98) && (echo_ON) ) {
					putc(loc_char, pc);
					current_rx_line[current_rx_line_count] = loc_char;
					current_rx_line_count++;
				}
			}
			else {
				if ( ( (loc_char == 0x08) || (loc_char == 0x7F) ) && (echo_ON) ) {//backspace
					if (current_rx_line_count>0) {
						current_rx_line_count--;
						putc(0x08,pc);
						putc(0x20,pc);
						putc(0x08,pc);
					}
				}
				else if ( (loc_char == 0x0D) && (echo_ON) ) {
					printf("\r\n");
					current_rx_line[current_rx_line_count] = 0;
					current_rx_line_count++;
					HMI_line_parse (current_rx_line, current_rx_line_count);
					current_rx_line_count = 0;
				}
				else if (loc_char == 0x03) {//ctrl + c
					HMI_cancel_current();
				}
			}
		}
		return 1;
	} else {
		return 0;
	}
}

void HMI_line_parse (char* RX_text, int RX_text_count) {
	char* loc_command_str;// [100];
	char* loc_param1_str;
	char* loc_param2_str;
	int command_understood = 0;
	int temp;

	loc_command_str = strtok (RX_text, " ");
	loc_param1_str = strtok (NULL, " ");
	loc_param2_str = strtok (NULL, " ");

	if (loc_command_str) {
		if (strcmp(loc_command_str, "radio") == 0) {
			command_understood = 1;
			if (strcmp(loc_param1_str, "on") == 0) {
				if (CONF_radio_state_ON_OFF == 0) {
					RADIO_on(1, 1, 1);
				}
				HMI_printf("OK\r\nready> ");
			}
			else if (strcmp(loc_param1_str, "off") == 0) {
				RADIO_off(1);
				HMI_printf("OK\r\nready> ");
			}
			else {
				HMI_printf("unknown radio command\r\nready> ");
			}
		}
		
		if (strcmp(loc_command_str, "TX_test") == 0) {
			command_understood = 1;
			HMI_TX_test(loc_param1_str);
		}
		if (strcmp(loc_command_str, "status") == 0) {//display status
			command_understood = 1;
			display_status_ongoing = 1;
			G_downlink_bandwidth_temp = 0;
			G_uplink_bandwidth_temp = 0;
			slow_counter = 0;
			echo_ON = 0;
			HMI_periodic_call();
		}
		if (strcmp(loc_command_str, "display") == 0) {
			command_understood = 1;
			if (strcmp(loc_param1_str, "config") == 0) {//display config
				HMI_display_config();
			}
			else if (strcmp(loc_param1_str, "static") == 0) {//display static alloc
				HMI_display_static();
			}
			else if (strcmp(loc_param1_str, "DHCP_ARP") == 0) {//display DHCP_ARP entries
				DHCP_ARP_print_entries();
			}
			else {
				HMI_printf("unknown display command\r\nready> ");
			}
		}
		if (strcmp(loc_command_str, "set") == 0) {
			HMI_set_command(loc_param1_str, loc_param2_str);
			command_understood = 1;
		}
		if (strcmp(loc_command_str, "who") == 0) {
			//HMI_print_who();
			command_understood = 1;
			display_who_ongoing = 1;
			slow_counter = 0;
			echo_ON = 0;
			HMI_periodic_call();
		}
		if (strcmp(loc_command_str, "reboot") == 0) {
			command_understood = 1;
			HMI_reboot();
		}
		if (strcmp(loc_command_str, "save") == 0) {
			command_understood = 1;

			RADIO_off_if_necessary(0);
			temp = NFPR_config_save();
			RADIO_restart_if_necessary(0, 0, 1);
			HMI_printf("saved index:%i\r\nready> ", temp);
		}
		if (strcmp(loc_command_str, "reset_to_default") == 0) {
			command_understood = 1;
			HMI_printf("clearing saved config...\r\n");
			RADIO_off_if_necessary(0);
			virt_EEPROM_errase_all();
			HMI_printf("Done. Now rebooting...\r\n");
			NVIC_SystemReset();
		}
		if (strcmp(loc_command_str, "version") == 0) {
			command_understood = 1;
			HMI_printf("firmware: %s\r\nfreq band: %s\r\nready> ", FW_VERSION, FREQ_BAND);
		}
		if (strcmp(loc_command_str, "exit") == 0) {
			command_understood = 1;
			HMI_exit();
		}
		if (command_understood == 0) {
			HMI_printf("unknown command\r\nready> ");
		}
	} else {//just a return with nothing
		HMI_printf("ready> ");
	}
}

void HMI_cancel_current(void) {
	if (echo_ON ==0) {
		echo_ON = 1;
		display_status_ongoing = 0;
		display_who_ongoing = 0;
		HMI_printf("ready> ");
	}
}

int HMI_check_radio_OFF(void) {
	if (CONF_radio_state_ON_OFF == 1) {
		HMI_printf("radio must be off for this command\r\nready> ");
		return 0;
	} else {
		return 1;
	}
}


void HMI_TX_test(char* duration_txt) {
	unsigned int duration;
	if ( HMI_check_radio_OFF() == 1) {
		sscanf (duration_txt, "%i", &duration);
		HMI_printf("reconfiguring radio...\r\n");
		SI4463_configure_all();
		wait_ms(50);
		TDMA_init_all();
		//SI4463_radio_start();
		
		wait_ms(1);
		G_SI4463->RX_TX_state = 0;
		SI4463_clear_IT(G_SI4463, 0, 0);
		wait_ms(10);
		CONF_radio_state_ON_OFF = 1;
		SI4463_TX_to_RX_transition();
		
		wait_ms(10);
		CONF_radio_state_ON_OFF = 0;
		if (!is_TDMA_master) {
			my_client_radio_connexion_state = 1;
			my_radio_client_ID = 0x7E;
		}
		wait_ms(50);
		
		TDMA_NULL_frame_init(70);
		HMI_printf("radio transmit test %i seconds...\r\n", duration);
		duration = duration * 1000; //milliseconds instead of seconds
		
		SI4432_TX_test(duration);
		
		HMI_printf("ready> ");
	}
}

void HMI_reboot(void) {
	if (is_telnet_opened == 1) {
		W5500_write_byte(W5500_p1, 0x0001, TELNET_SOCKET, 0x08);
	}
	//extern "C" void mbed_reset();
	NVIC_SystemReset();
}

void HMI_force_exit(void) {
	unsigned char IP_loc[8];
	if (is_telnet_opened == 1) {
		IP_int2char (LAN_conf_applied.LAN_modem_IP, IP_loc);
		//HMI_printf("\r\n\r\n\r\nNew IP config... force telnet exit.\r\n");
		//HMI_printf("\r\n\r\nNew IP config. Open new telnet session with: %i.%i.%i.%i\r\n\r\n", IP_loc[0], IP_loc[1], IP_loc[2], IP_loc[3]);
		W5500_write_byte(W5500_p1, 0x0001, TELNET_SOCKET, 0x08); //close TCP
		is_telnet_opened = 0;
		echo_ON = 1;
		display_status_ongoing = 0;
		printf("telnet connexion closed\r\nready> "); 
		fflush(stdout);
	}
}

void HMI_exit(void) {
	if (is_telnet_opened == 1) {
		W5500_write_byte(W5500_p1, 0x0001, TELNET_SOCKET, 0x08); //close TCP
		is_telnet_opened = 0;
		echo_ON = 1;
		display_status_ongoing = 0;
		printf("telnet connexion closed\r\nready> "); 
		fflush(stdout);
	} else {
		printf("exit only valid for telnet\r\nready> ");
		fflush(stdout);
	}
}

static char HMI_yes_no[2][4]={'n','o',0,0, 'y','e','s',0};
// static char HMI_trans_modes[2][4]={'I','P',0,0,'E','t','h',0};
static char HMI_master_FDD[3][5]={'n','o',0,0,0,'d','o','w','n',0,'u','p',0,0,0};

void HMI_display_config(void) {
	unsigned char IP_loc[8];

	HMI_printf("CONFIG:\r\n  callsign: '%s'\r\n  is_master: %s\r\n  MAC: %02X:%02X:%02X:%02X:%02X:%02X\r\n", CONF_radio_my_callsign+2, HMI_yes_no[is_TDMA_master],CONF_modem_MAC[0],CONF_modem_MAC[1],CONF_modem_MAC[2],CONF_modem_MAC[3],CONF_modem_MAC[4],CONF_modem_MAC[5]);
	HMI_printf("  ext_SRAM: %s\r\n", HMI_yes_no[is_SRAM_ext]);
	HMI_printf("  frequency: %.3fMHz\r\n  freq_shift: %.3fMHz\r\n  RF_power: %i\r\n  modulation: %i\r\n", ((float)CONF_frequency_HD/1000)+FREQ_RANGE_MIN, (float)CONF_freq_shift/1000, CONF_radio_PA_PWR, CONF_radio_modulation); 

	HMI_printf("  radio_netw_ID: %i\r\n  radio_on_at_start: %s\r\n", CONF_radio_network_ID, HMI_yes_no[CONF_radio_default_state_ON_OFF]);
	HMI_printf("  telnet active: %s\r\n  telnet routed: %s\r\n", HMI_yes_no[is_telnet_active], HMI_yes_no[is_telnet_routed]);
	IP_int2char (LAN_conf_saved.LAN_modem_IP, IP_loc);
	IP_int2char (LAN_conf_saved.LAN_subnet_mask, IP_loc+4);
	HMI_printf("  modem_IP: %i.%i.%i.%i\r\n  netmask: %i.%i.%i.%i\r\n", IP_loc[0], IP_loc[1],IP_loc[2],IP_loc[3],IP_loc[4],IP_loc[5],IP_loc[6],IP_loc[7]);
	
	if (is_TDMA_master) {
		HMI_printf("  master_FDD: %s\r\n", HMI_master_FDD[CONF_master_FDD]);
	}
	
	
	if ( (is_TDMA_master) && ( CONF_master_FDD < 2 ) && (CONF_transmission_method==0) ) {//Master FDD down (or no FDD)	
		IP_int2char (CONF_radio_IP_start, IP_loc);
		IP_int2char (CONF_radio_IP_start+CONF_radio_IP_size-1, IP_loc+4);
		HMI_printf("  IP_begin: %i.%i.%i.%i\r\n  master_IP_size: %ld (Last IP: %i.%i.%i.%i)\r\n", IP_loc[0], IP_loc[1],IP_loc[2],IP_loc[3],CONF_radio_IP_size, IP_loc[4],IP_loc[5],IP_loc[6],IP_loc[7]);
		IP_int2char (LAN_conf_saved.LAN_def_route, IP_loc);	
		HMI_printf("  def_route_active: %s\r\n  def_route_val: %i.%i.%i.%i\r\n", HMI_yes_no[LAN_conf_saved.LAN_def_route_activ], IP_loc[0],IP_loc[1],IP_loc[2],IP_loc[3]);
		IP_int2char (LAN_conf_saved.LAN_DNS_value, IP_loc);
		HMI_printf("  DNS_active: %s\r\n  DNS_value: %i.%i.%i.%i\r\n", HMI_yes_no[LAN_conf_saved.LAN_DNS_activ], IP_loc[0],IP_loc[1],IP_loc[2],IP_loc[3]);
	}
	if ( (is_TDMA_master) && (CONF_master_FDD == 2) ) {//Master FDD up
		IP_int2char (CONF_master_down_IP, IP_loc);
		HMI_printf("  master_down_IP: %i.%i.%i.%i\r\n",IP_loc[0],IP_loc[1],IP_loc[2],IP_loc[3]);
	}
	if (!is_TDMA_master) {//client
		IP_int2char (CONF_radio_IP_start, IP_loc);
		HMI_printf("  IP_begin: %i.%i.%i.%i\r\n", IP_loc[0], IP_loc[1],IP_loc[2],IP_loc[3]);
		HMI_printf("  client_req_size: %ld\r\n  DHCP_active: %s\r\n", CONF_radio_IP_size_requested, HMI_yes_no[LAN_conf_saved.DHCP_server_active]);
	}
	HMI_printf("ready> ");
}

void HMI_display_static(void) {
	
}

void HMI_set_command(char* loc_param1, char* loc_param2) {
	int temp;
	unsigned char temp_uchar;
	unsigned long int temp_uint;
	float frequency;
	// unsigned char previous_freq_band;
	char DHCP_warning[50];
	if ((loc_param1) && (loc_param2)) {
		if (strcmp(loc_param1, "callsign") == 0) {
			RADIO_off_if_necessary(1);
			strcpy (CONF_radio_my_callsign+2, loc_param2);
			CONF_radio_my_callsign[0] = CONF_modem_MAC[4];
			CONF_radio_my_callsign[1] = CONF_modem_MAC[5];
			CONF_radio_my_callsign[15] = 0;
			RADIO_restart_if_necessary(1, 0, 1);
			HMI_printf("new callsign '%s'\r\nready> ", CONF_radio_my_callsign+2);
		} 
		else if (strcmp(loc_param1, "is_master") == 0) {
			temp_uchar = HMI_yes_no_2int(loc_param2);
			if ( (temp_uchar==0) || (temp_uchar==1) ) {
				RADIO_off_if_necessary(1);
				is_TDMA_master = (temp_uchar == 1);
				RADIO_restart_if_necessary(1, 0, 1);
				if ( (is_TDMA_master) && (LAN_conf_saved.DHCP_server_active == 1) ) {
					strcpy (DHCP_warning, " (warning, DHCP inhibited in master mode)"); 
				} else {
					strcpy (DHCP_warning, ""); 
				}
				HMI_printf("Master '%s'%s\r\nready> ", loc_param2, DHCP_warning);
			}
		}
		else if (strcmp(loc_param1, "telnet_active") == 0) {
			temp_uchar = HMI_yes_no_2int(loc_param2);
			if ( (temp_uchar==0) || (temp_uchar==1) ) {
				if(is_telnet_opened) { HMI_exit(); }
				is_telnet_active = temp_uchar;
				HMI_printf("telnet active '%s'\r\nready> ", loc_param2);
			}
		}
		else if (strcmp(loc_param1, "telnet_routed") == 0) {
			temp_uchar = HMI_yes_no_2int(loc_param2);
			if ( (temp_uchar==0) || (temp_uchar==1) ) {
				is_telnet_routed = temp_uchar;
				//W5500_re_configure_gateway(W5500_p1);
				HMI_printf("telnet routed '%s'\r\nready> ", loc_param2);
			}
		}
		else if (strcmp(loc_param1, "DNS_active") == 0) {
			temp_uchar = HMI_yes_no_2int(loc_param2);
			if ( (temp_uchar==0) || (temp_uchar==1) ) {
				RADIO_off_if_necessary(1);
				LAN_conf_saved.LAN_DNS_activ = temp_uchar;
				RADIO_restart_if_necessary(1, 0, 1);
				HMI_printf("DNS active '%s'\r\nready> ", loc_param2);
			}
		}
		else if (strcmp(loc_param1, "def_route_active") == 0) {
			temp_uchar = HMI_yes_no_2int(loc_param2);
			if ( (temp_uchar==0) || (temp_uchar==1) ) {
				RADIO_off_if_necessary(1);
				LAN_conf_saved.LAN_def_route_activ = temp_uchar;
				//W5500_re_configure_gateway(W5500_p1);
				RADIO_restart_if_necessary(1, 0, 1);
				HMI_printf("default route active '%s'\r\nready> ", loc_param2);
			}
		}
		else if (strcmp(loc_param1, "master_FDD") == 0) {
			if(strcmp(loc_param2,"no") == 0) {
				CONF_master_FDD = 0;
				RADIO_off_if_necessary(1);
				RADIO_restart_if_necessary(1, 0, 1);
			}
			else if(strcmp(loc_param2,"down") == 0) {
				CONF_master_FDD = 1;
				RADIO_off_if_necessary(1);
				RADIO_restart_if_necessary(1, 0, 1);
			}
			else if(strcmp(loc_param2,"up") == 0) {
				CONF_master_FDD = 2;
				RADIO_off_if_necessary(1);
				RADIO_restart_if_necessary(1, 0, 1);
			}
			else {
				HMI_printf("  wrong value\r\n");
			}
			HMI_printf("ready> ");
		}
		//else if (strcmp(loc_param1, "trans_method") == 0) {
		//	if(strcmp(loc_param2,"IP") == 0) {
		//		CONF_transmission_method = 0;
		//		RADIO_off_if_necessary(1);
		//		RADIO_restart_if_necessary(1, 0, 1);
		//	}
		//	else if(strcmp(loc_param2,"Eth") == 0) {
		//		CONF_transmission_method = 1;
		//		RADIO_off_if_necessary(1);
		//		RADIO_restart_if_necessary(1, 0, 1);
		//	}
		//	else {
		//		HMI_printf("  wrong value\r\n");
		//	}
		//	HMI_printf("ready> ");
		//}
		//else if (strcmp(loc_param1, "client_static_IP") == 0) {
		//	temp_uchar = HMI_yes_no_2int(loc_param2);
		//	if ( (temp_uchar==0) || (temp_uchar==1) ) {
		//		RADIO_off_if_necessary(1);
		//		CONF_radio_static_IP_requested = temp_uchar;
		//		HMI_printf("client static IP '%s'\r\nready> ", loc_param2);
		//		RADIO_restart_if_necessary(1, 0, 1);
		//	}
		//}
		else if (strcmp(loc_param1, "radio_on_at_start") == 0) {
			temp_uchar = HMI_yes_no_2int(loc_param2);
			if ( (temp_uchar==0) || (temp_uchar==1) ) {
				CONF_radio_default_state_ON_OFF = temp_uchar;
				HMI_printf("radio_on_at_start '%s'\r\nready> ", loc_param2);
			}
		}
		else if (strcmp(loc_param1, "DHCP_active") == 0) {
			temp_uchar = HMI_yes_no_2int(loc_param2);
			if ( (temp_uchar==0) || (temp_uchar==1) ) {
				LAN_conf_saved.DHCP_server_active = temp_uchar;
				if ( (is_TDMA_master) && (LAN_conf_saved.DHCP_server_active == 1) ) {
					strcpy (DHCP_warning, " (warning, DHCP inhibited in master mode)"); 
				} else {
					strcpy (DHCP_warning, ""); 
				}
				HMI_printf("DHCP_active: '%s'%s\r\nready> ", loc_param2, DHCP_warning);
			}
		}
		else if (strcmp(loc_param1, "modem_IP") == 0) {
			temp_uint = HMI_str2IP(loc_param2);
			if (temp_uint !=0) {
				RADIO_off_if_necessary(1);
				LAN_conf_saved.LAN_modem_IP = temp_uint;
				//HMI_force_exit();
				//W5500_re_configure();
				RADIO_restart_if_necessary(1, 0, 1);
			}
		}
		else if (strcmp(loc_param1, "netmask") == 0) {
			temp_uint = HMI_str2IP(loc_param2);
			if (temp_uint !=0) {
				RADIO_off_if_necessary(1);
				LAN_conf_saved.LAN_subnet_mask = temp_uint;
				//HMI_force_exit();
				//W5500_re_configure();
				RADIO_restart_if_necessary(1, 0, 1);
			}
		}
		else if (strcmp(loc_param1, "def_route_val") == 0) {
			temp_uint = HMI_str2IP(loc_param2);
			if (temp_uint !=0) {
				RADIO_off_if_necessary(1);
				LAN_conf_saved.LAN_def_route = temp_uint;
				//W5500_re_configure_gateway(W5500_p1);
				RADIO_restart_if_necessary(1, 0, 1);
			}
		}
		else if (strcmp(loc_param1, "DNS_value") == 0) {
			temp_uint = HMI_str2IP(loc_param2);
			if (temp_uint !=0) {
				RADIO_off_if_necessary(1);
				LAN_conf_saved.LAN_DNS_value = temp_uint;
				RADIO_restart_if_necessary(1, 0, 1);
			}
		}
		else if (strcmp(loc_param1, "IP_begin") == 0) {
			temp_uint = HMI_str2IP(loc_param2);
			if (temp_uint !=0) {
				RADIO_off_if_necessary(1);
				CONF_radio_IP_start = temp_uint;
				RADIO_restart_if_necessary(1, 0, 1);
			}
		}
		else if (strcmp(loc_param1, "master_down_IP") == 0) {
			temp_uint = HMI_str2IP(loc_param2);
			if (temp_uint !=0) {
				RADIO_off_if_necessary(1);
				CONF_master_down_IP = temp_uint;
				RADIO_restart_if_necessary(1, 0, 1);
			}
		}
		else if (strcmp(loc_param1, "master_IP_size") == 0) {
			temp = sscanf (loc_param2, "%ld", &temp_uint);
			if ( (temp==1) && (temp_uint!=0) ) {
				RADIO_off_if_necessary(1);
				CONF_radio_IP_size = temp_uint;
				RADIO_restart_if_necessary(1, 0, 1);
				HMI_printf("OK\r\nready> ");
			}
			else {
				HMI_printf("wrong value\r\nready> ");
			}
		}
		else if (strcmp(loc_param1, "client_req_size") == 0) {
			temp = sscanf (loc_param2, "%ld", &temp_uint);
			if ( (temp==1) && (temp_uint!=0) ) {
				RADIO_off_if_necessary(1);
				CONF_radio_IP_size_requested = temp_uint;
				RADIO_restart_if_necessary(1, 0, 1);
				HMI_printf("OK\r\nready> ");
			}
			else {
				HMI_printf("wrong value\r\nready> ");
			}
		}

		else if (strcmp(loc_param1, "frequency") == 0) {
			temp = sscanf (loc_param2, "%f", &frequency);
			if ( (temp == 1) && (frequency<=FREQ_RANGE_MAX) && (frequency>FREQ_RANGE_MIN) ) {
				RADIO_off_if_necessary(0);
				frequency = (frequency - FREQ_RANGE_MIN)*1000 + 0.3; 
				CONF_frequency_HD = (short int)frequency;
				//RADIO_compute_freq_params();//REMOVE TEST
				RADIO_restart_if_necessary(0, 1, 1);
				HMI_printf("OK\r\nready> ");
			} else {
				HMI_printf("wrong freq value\r\nready> ");
			}
		}
		else if (strcmp(loc_param1, "freq_shift") == 0) {
			temp = sscanf (loc_param2, "%f", &frequency);
			if ( (temp == 1) && (frequency<=10) && (frequency>=-10) ) {
				RADIO_off_if_necessary(0);
				frequency = (frequency*1000);
				CONF_freq_shift = (short int)frequency;
				//RADIO_compute_freq_params();//REMOVE TEST
				//if (CONF_frequency_band == previous_freq_band) {
				//	RADIO_restart_if_necessary(0, 0, 1);
				//}else {
				RADIO_restart_if_necessary(0, 1, 1);
				//}
				HMI_printf("OK\r\nready> ");
			} else {
				HMI_printf("wrong freq value\r\nready> ");
			}
		}
		else if (strcmp(loc_param1, "RF_power") == 0) {
			temp_uint = sscanf (loc_param2, "%i", &temp); 
			if ( (temp_uint == 1) && (temp<128) ) {
				RADIO_off_if_necessary(0);
				CONF_radio_PA_PWR = temp;
				SI4463_set_power(G_SI4463);
				RADIO_restart_if_necessary(0, 0, 1);
				HMI_printf("OK\r\nready> ");
			} else {
				HMI_printf("error : max RF_power value 127\r\nready> ");
			}
		}
		else if (strcmp(loc_param1, "modulation") == 0) {
			temp_uint = sscanf (loc_param2, "%i", &temp);
			temp_uchar = temp;
			//if ( (temp_uint == 1) && ((temp_uchar==13)||(temp_uchar==14)||(temp_uchar==22)||(temp_uchar==23)||(temp_uchar==24)) ) {
			if ( (temp_uint == 1) && ( ((temp_uchar>=11)&&(temp_uchar<=14)) || ((temp_uchar>=20)&&(temp_uchar<=24)) ) ) {
				RADIO_off_if_necessary(1);
				CONF_radio_modulation = temp_uchar;
				RADIO_restart_if_necessary(1, 1, 1);
				HMI_printf("OK\r\nready> ");
			} else {
				HMI_printf("wrong modulation value\r\nready> ");
			}
		}
		else if (strcmp(loc_param1, "radio_netw_ID") == 0) {
			temp_uint = sscanf (loc_param2, "%i", &temp);
			temp_uchar = temp;
			if ( (temp_uint == 1) && (temp_uchar <= 15) ) {
				RADIO_off_if_necessary(1);
				CONF_radio_network_ID = temp_uchar;
				RADIO_restart_if_necessary(1, 1, 1);
				HMI_printf("OK\r\nready> ");
			} else {
				HMI_printf("wrong value, 15 max\r\nready> ");
			}
		}
		else {
			HMI_printf("unknown config param\r\nready> ");
		}
	} else {
		HMI_printf("set command requires 2 param\r\nready> ");
	}
}

unsigned long int HMI_str2IP(char* raw_string) {
	unsigned int IP_char_t[6];
	unsigned char IP_char[6]; 
	unsigned long int answer;
	int i;
	answer = sscanf(raw_string, "%i.%i.%i.%i", IP_char_t, IP_char_t+1, IP_char_t+2, IP_char_t+3);
	for (i=0;i<4; i++) {
		IP_char[i] = IP_char_t[i];
	}
	if (answer == 4) {
		answer = IP_char2int(IP_char);
		HMI_printf("OK\r\nready> ");
	} else {
		HMI_printf("bad IP format\r\nready> ");
		answer = 0;
	}
	return answer;
}

unsigned char HMI_yes_no_2int(char* raw_string) {
	unsigned char answer;
	if (strcmp (raw_string, "yes") == 0) {
		answer = 1;
	} 
	else if (strcmp (raw_string, "no") == 0) {
		answer = 0;
	}
	else {
		HMI_printf("value must be 'yes' or 'no'\r\nready> ");
		answer = -1;
	}
	return answer;
}

void HMI_print_who(void) {
	int i;
	unsigned int loc_age;
	unsigned int timer_snapshot;
	unsigned long int last_IP;
	unsigned char IP_c[6];
	char temp_string[50] = {0x1B,0x5B,0x41,0x1B,0x5B,0x41,0x1B,0x5B,0x41,0x1B, 0x5B, 0x41,0x1B, 0x5B, 0x41,0x1B, 0x5B, 0x41,0x1B, 0x5B, 0x41,0x1B, 0x5B, 0x41,0x1B, 0x5B, 0x41,0x1B, 0x5B, 0x41,0x1B, 0x5B, 0x41,0x00};

	if (slow_counter == 0) { temp_string[0] = 0; }
	HMI_printf ("%s%i Master: ID:127 Callsign:%s\r\n", temp_string, slow_counter, CONF_radio_master_callsign+2);
	IP_int2char (LAN_conf_applied.LAN_modem_IP, IP_c);
	HMI_printf ("ME: Callsign:%s ID:%i modem IP:%i.%i.%i.%i\r\n", CONF_radio_my_callsign+2, my_radio_client_ID, IP_c[0], IP_c[1], IP_c[2], IP_c[3]);
	HMI_printf ("Clients:\r\n");
	timer_snapshot = GLOBAL_timer.read_us();
	for (i=0; i<radio_addr_table_size; i++) {
		
	}
	for (i=0; i<radio_addr_table_size; i++) {
		loc_age = timer_snapshot - CONF_radio_addr_table_date[i];
		loc_age = loc_age / 1000000;
		if (is_TDMA_master) {loc_age = 0;} // master : already timeout in state machine
		//printf ("age:%i ", loc_age);
		if ( (CONF_radio_addr_table_status[i]) && (loc_age < connexion_timeout) ) {
			HMI_printf (" ID:%i Callsign:%s ", i, CONF_radio_addr_table_callsign[i]+2);
			IP_int2char (CONF_radio_addr_table_IP_begin[i], IP_c);
			HMI_printf ("IP start:%i.%i.%i.%i ", IP_c[0], IP_c[1], IP_c[2], IP_c[3]);
			last_IP = CONF_radio_addr_table_IP_begin[i] + CONF_radio_addr_table_IP_size[i] - 1;
			IP_int2char (last_IP, IP_c);
			HMI_printf ("IP end:%i.%i.%i.%i\r\n", IP_c[0], IP_c[1], IP_c[2], IP_c[3]);
		} else {
			HMI_printf ("                                                            \r\n");
		}
		
	}
	HMI_printf("CTRL+c to exit...\r\n");
}

void HMI_print_status(void) {
	static char text_radio_status[3][22] = {
		"waiting connection",
		"connected",
		"connection rejected"
	};
	static char text_reject_reason[2][15] = {
		"IP requested",
		"clients",
	};
	char temp_string[22];
	float loc_downlink_bw;
	float loc_uplink_bw;
	int TA_loc; 
	temp_string[0]=0x1B;
	temp_string[1]=0x5B;
	temp_string[2]=0x41;
	temp_string[3]=0x1B;
	temp_string[4]=0x5B;
	temp_string[5]=0x41;
	temp_string[6]=0x1B;
	temp_string[7]=0x5B;
	temp_string[8]=0x41;
	temp_string[9]=0x1B;
	temp_string[10]=0x5B;
	temp_string[11]=0x41;
	temp_string[12]=0x1B;
	temp_string[13]=0x5B;
	temp_string[14]=0x41;
	temp_string[15]=0x00;
	//temp_string[15]=0x1B;
	//temp_string[16]=0x5B;
	//temp_string[17]=0x41;
	//temp_string[18]=0x00;
	if (slow_counter == 0) { temp_string[0] = 0; }
	if (is_TDMA_master) {
		loc_downlink_bw = G_uplink_bandwidth_temp * 0.004;
		loc_uplink_bw = G_downlink_bandwidth_temp * 0.004;
		TA_loc = 0;
	} else {
		loc_downlink_bw = G_downlink_bandwidth_temp * 0.004;
		loc_uplink_bw =  G_uplink_bandwidth_temp * 0.004;
		TA_loc = TDMA_table_TA[my_radio_client_ID];
	}
	if (CONF_radio_state_ON_OFF == 0) {
		HMI_printf("%s   %i status: radio OFF               \r\n", temp_string, slow_counter);
	}
	else if (my_client_radio_connexion_state == 0x03) {
		HMI_printf("%s   %i status: rejected because too many %s \r\n", temp_string, slow_counter, text_reject_reason[G_connect_rejection_reason-2]);
	} else {
		HMI_printf("%s   %i status: %s TA:%.1fkm Temp:%idegC   \r\n", temp_string, slow_counter, text_radio_status[my_client_radio_connexion_state-1], 0.15*(float)TA_loc, G_temperature_SI4463);
	}
	//HMI_printf("   TX buff filling %i\r\n", (TX_buff_ext_WR_pointer - TX_buff_ext_RD_pointer)*128);//!!!test
	if ( (is_TDMA_master) && (CONF_master_FDD == 2) ) {
		HMI_printf("   RX tops from master FDD down %i\r\n", RX_top_FDD_up_counter);
	} else {
		HMI_printf("   RX_Eth_IPv4 %i ;TX_radio_IPv4 %i ; RX_radio_IPv4 %i   \r\n", RX_Eth_IPv4_counter, TX_radio_IPv4_counter, RX_radio_IPv4_counter);
		//HMI_printf("   RX_Eth_IPv4 %i ;TX_radio_IPv4 %i ; RX_radio_IPv4 %i   \r\n", RX_Eth_IPv4_counter, TX_radio_IPv4_counter, (TX_buff_ext_WR_pointer - TX_buff_ext_RD_pointer)*128);//!!!! debug FIFO filling
	}
	if ( (!is_TDMA_master) && (RSSI_stat_pkt_nb > 0) ) {
		//HMI_printf("RSSI: %i\r\nCTRL+c to exit...\r\n", (RSSI_total_stat / RSSI_stat_pkt_nb) );
		HMI_printf("   DOWNLINK - bandwidth:%.1f RSSI:%.1f ERR:%.2f%%    \r\n", loc_downlink_bw, ((float)G_downlink_RSSI/256/2-136), ((float)G_downlink_BER)/500); // /500
		RSSI_total_stat = 0;
		RSSI_stat_pkt_nb = 0;
	} else {
		HMI_printf("   DOWNLINK - bandwidth: %.1f RSSI:       ERR:      \r\n", loc_downlink_bw);
		
	}
	if ( (!is_TDMA_master) && (my_client_radio_connexion_state == 2) ) {
		HMI_printf("   UPLINK -   bandwidth:%.1f RSSI:%.1f ERR:%.2f%%    \r\nCTRL+c to exit...\r\n", loc_uplink_bw, ((float)G_radio_addr_table_RSSI[my_radio_client_ID]/2-136), ((float)G_radio_addr_table_BER[my_radio_client_ID])/500);
	} else {
		HMI_printf("   UPLINK -   bandwidth:%.1f RSSI:     ERR:      \r\nCTRL+c to exit...\r\n", loc_uplink_bw);
	}
	G_downlink_bandwidth_temp = 0;
	G_uplink_bandwidth_temp = 0;
}

void HMI_periodic_call (void) {
	if (display_status_ongoing) {
		HMI_print_status();
		slow_counter++;
	}
	if (display_who_ongoing) {
		HMI_print_who();
		slow_counter++;
	}
}

void HMI_printf_detail (void) {
	int size;
	if (is_telnet_opened) {
		size = strlen (HMI_out_str);
		W5500_write_TX_buffer (W5500_p1, TELNET_SOCKET, (unsigned char*)HMI_out_str, size, 0);
	}
	else {
		printf("%s", HMI_out_str);
		fflush(stdout);
	}
}
