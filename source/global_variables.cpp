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

#include "SI4463.h"
#include "W5500.h"
#include "global_variables.h"
#include "ext_SRAM2.h"

SI4463_Chip* G_SI4463;
W5500_chip* W5500_p1;
DigitalInOut* G_FDD_trig_pin;
InterruptIn* G_FDD_trig_IRQ;
DigitalOut* G_PTT_PA_pin;

ext_SRAM_chip* SPI_SRAM_p;

Serial pc(SERIAL_TX, SERIAL_RX);
//DigitalOut* LED_connected_p;

Timeout SI4463_prepa_TX_1_call;

char HMI_out_str[120];

unsigned int RX_FIFO_WR_point = 0;
unsigned int RX_FIFO_RD_point = 0;
unsigned int RX_FIFO_last_received = 0;
unsigned char RX_FIFO_data[0x2000]; //8kB

//unsigned int debug_counter = 0;
unsigned int RX_top_FDD_up_counter = 0;
int RX_Eth_IPv4_counter = 0;
int TX_radio_IPv4_counter = 0;
int RX_radio_IPv4_counter = 0;
unsigned int RSSI_total_stat = 0;
unsigned int RSSI_stat_pkt_nb = 0;


int slave_new_burst_tx_pending = 0;

unsigned char TX_buff_intern_FIFOdata[128][128];
unsigned int TX_buff_intern_WR_pointer=0;
unsigned int TX_buff_intern_RD_pointer=0;
unsigned int TX_buff_intern_last_ready=0;

unsigned char TX_buff_ext_sizes[1024];//1024
unsigned int TX_buff_ext_WR_pointer;
unsigned int TX_buff_ext_RD_pointer;
unsigned int TX_buff_ext_last_ready;

unsigned char TX_TDMA_intern_data[384];

bool is_TDMA_master = false; // Is the modem a master or a client
unsigned char is_telnet_active = 1;
unsigned char is_telnet_routed = 0;
unsigned char CONF_radio_modulation;
unsigned char CONF_radio_frequency;
unsigned char CONF_frequency_band;
unsigned char CONF_radio_network_ID;
unsigned short int CONF_frequency_HD;
short int CONF_freq_shift=0;
unsigned char CONF_channel_TX;
unsigned char CONF_channel_RX;
unsigned char CONF_SI4463_freq_conf_RX[15];
unsigned char CONF_SI4463_freq_conf_TX[15];
unsigned char CONF_transmission_method=0;
unsigned char CONF_master_FDD=0;
unsigned long int CONF_master_down_IP;
unsigned int CONF_TDMA_frame_duration = 65000; // 
unsigned int CONF_TDMA_slot_duration = 3360;
unsigned int CONF_reduced_TDMA_slot_duration = 2360;
unsigned int CONF_TDMA_slot_margin = 300;
unsigned int CONF_TR_margain = 500;
unsigned int CONF_TA_margain = 2000;
unsigned int CONF_preamble_duration_for_decide = 590;
unsigned int CONF_long_preamble_duration_for_TA = 1000;
unsigned int CONF_byte_duration = 8;
unsigned int CONF_additional_preamble = 700;
unsigned long int CONF_radio_timeout = 30000000;
unsigned long int CONF_radio_timeout_small = 10000000;
unsigned int offset_time_TX_slave = 48000; 
unsigned int time_next_TX_slave;
unsigned int time_max_TX_burst = 45000;//47000
int CONF_delay_prepTX1_2_TX = 1030;
unsigned char my_radio_client_ID = 0xFE;
int CONF_Tx_rframe_timeout = 30;//unit 1/65000 th of a second
int CONF_signaling_period = 1;
int is_SRAM_ext=1;
int DEBUG_max_rx_size_w5500=0;

unsigned int TDMA_slave_last_master_top = 0;

Timer GLOBAL_timer;

LAN_conf_T LAN_conf_saved;
LAN_conf_T LAN_conf_applied; 

unsigned char CONF_modem_MAC[6]; 

char CONF_radio_my_callsign[16];
char CONF_radio_master_callsign[16];
unsigned long int CONF_radio_addr_table_IP_begin[radio_addr_table_size];
unsigned long int CONF_radio_addr_table_IP_size[radio_addr_table_size];
char CONF_radio_addr_table_callsign[radio_addr_table_size][16];
char CONF_radio_addr_table_status[radio_addr_table_size];
unsigned int CONF_radio_addr_table_date[radio_addr_table_size];
unsigned long int CONF_radio_IP_start;
unsigned long int CONF_radio_IP_size;
unsigned int my_client_radio_connexion_state;
unsigned int my_client_radio_connexion_date;
unsigned char G_connect_rejection_reason;
int G_temperature_SI4463;
int G_need_temperature_check = 0;
unsigned long int CONF_radio_IP_size_requested;
unsigned char CONF_radio_static_IP_requested;
unsigned char CONF_radio_state_ON_OFF = 0;
unsigned char CONF_radio_default_state_ON_OFF;
unsigned char CONF_radio_PA_PWR = 0x7F;
unsigned char CONF_preamble_TX_long;
unsigned char CONF_preamble_TX_short;
unsigned long int last_rframe_seen = 0;

long int TDMA_table_TA[radio_addr_table_size];

unsigned short int G_downlink_RSSI; 
unsigned short int G_radio_addr_table_RSSI[radio_addr_table_size]; 
unsigned short int G_downlink_BER; 
unsigned short int G_radio_addr_table_BER[radio_addr_table_size]; 
short int G_downlink_TA; 
int G_downlink_bandwidth_temp;
int G_uplink_bandwidth_temp;

int super_debug = 0;

unsigned char parity_bit_elab[128] = {
	0x00,0x80,0x80,0x00,0x80,0x00,0x00,0x80,0x80,0x00,0x00,0x80,0x00,0x80,0x80,0x00,
	0x80,0x00,0x00,0x80,0x00,0x80,0x80,0x00,0x00,0x80,0x80,0x00,0x80,0x00,0x00,0x80,
	0x80,0x00,0x00,0x80,0x00,0x80,0x80,0x00,0x00,0x80,0x80,0x00,0x80,0x00,0x00,0x80,
	0x00,0x80,0x80,0x00,0x80,0x00,0x00,0x80,0x80,0x00,0x00,0x80,0x00,0x80,0x80,0x00,
	0x80,0x00,0x00,0x80,0x00,0x80,0x80,0x00,0x00,0x80,0x80,0x00,0x80,0x00,0x00,0x80,
	0x00,0x80,0x80,0x00,0x80,0x00,0x00,0x80,0x80,0x00,0x00,0x80,0x00,0x80,0x80,0x00,
	0x00,0x80,0x80,0x00,0x80,0x00,0x00,0x80,0x80,0x00,0x00,0x80,0x00,0x80,0x80,0x00,
	0x80,0x00,0x00,0x80,0x00,0x80,0x80,0x00,0x00,0x80,0x80,0x00,0x80,0x00,0x00,0x80
};
unsigned char parity_bit_check[256] = {
	1,0,0,1,0,1,1,0,0,1,1,0,1,0,0,1,0,1,1,0,1,0,0,1,1,0,0,1,0,1,1,0,
	0,1,1,0,1,0,0,1,1,0,0,1,0,1,1,0,1,0,0,1,0,1,1,0,0,1,1,0,1,0,0,1,
	0,1,1,0,1,0,0,1,1,0,0,1,0,1,1,0,1,0,0,1,0,1,1,0,0,1,1,0,1,0,0,1,
	1,0,0,1,0,1,1,0,0,1,1,0,1,0,0,1,0,1,1,0,1,0,0,1,1,0,0,1,0,1,1,0,
	0,1,1,0,1,0,0,1,1,0,0,1,0,1,1,0,1,0,0,1,0,1,1,0,0,1,1,0,1,0,0,1,
	1,0,0,1,0,1,1,0,0,1,1,0,1,0,0,1,0,1,1,0,1,0,0,1,1,0,0,1,0,1,1,0,
	1,0,0,1,0,1,1,0,0,1,1,0,1,0,0,1,0,1,1,0,1,0,0,1,1,0,0,1,0,1,1,0,
	0,1,1,0,1,0,0,1,1,0,0,1,0,1,1,0,1,0,0,1,0,1,1,0,0,1,1,0,1,0,0,1
};
