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

#ifndef GLOB_VARIAB_F4
#define GLOB_VARIAB_F4
#include "SI4463.h"
#include "W5500.h"

//#define EXT_SRAM_USAGE
//#define FREQ_BAND_2M
//#define NPR_L476

#include "ext_SRAM2.h"
extern ext_SRAM_chip* SPI_SRAM_p;

#ifdef FREQ_BAND_2M
	#define CONF_DEF_FREQ 1000
	#define FREQ_RANGE_MIN 144
	#define FREQ_RANGE_MAX 148
	#define FREQ_MAX_RAW 4000
	#define FREQ_BAND "2m"
	#define SI4463_NOUTDIV 24
#else 
	/*420 - 450MHz*/
	#define CONF_DEF_FREQ 17000
	#define FREQ_RANGE_MIN 420
	#define FREQ_RANGE_MAX 450
	#define FREQ_MAX_RAW 30000
	#define FREQ_BAND "70cm"
	#define SI4463_NOUTDIV 8
#endif

#define FW_VERSION "2020_02_23"

extern SI4463_Chip* G_SI4463;

extern W5500_chip* W5500_p1;

extern DigitalInOut* G_FDD_trig_pin;
extern InterruptIn* G_FDD_trig_IRQ;
extern DigitalOut* G_PTT_PA_pin;

extern Serial pc;

extern Timeout SI4463_prepa_TX_1_call;

extern unsigned char TX_buff_intern_FIFOdata[128][128];
extern unsigned int TX_buff_intern_WR_pointer;
extern unsigned int TX_buff_intern_RD_pointer;
extern unsigned int TX_buff_intern_last_ready;

extern unsigned char TX_buff_ext_sizes[1024];//1024
extern unsigned int TX_buff_ext_WR_pointer;
extern unsigned int TX_buff_ext_RD_pointer;
extern unsigned int TX_buff_ext_last_ready;

extern unsigned char TX_TDMA_intern_data[384];

extern char HMI_out_str[120];

// RX FIFO (RX from radio)
extern unsigned int RX_FIFO_WR_point;
extern unsigned int RX_FIFO_RD_point;
extern unsigned int RX_FIFO_last_received;
//extern unsigned char RX_FIFO_data[0x2000]; //8kB
extern unsigned char RX_FIFO_data[0x2000]; //8kB
#define RX_FIFO_mask 0x1FFF 

#define TXPS_FIFO_mask 0x3FFF
#define TXPS_FIFO_threshold 14000
#define TXPS_FIFO_threshold_sig 16380

//#define CONF_Tx_rframe_timeout 30
//CONF_Tx_rframe_timeout unit 1/65000 th of a second
// *** TDMA ***
extern unsigned char TX_signaling_TDMA_frame[300]; 

extern unsigned char is_TDMA_master;
extern unsigned char is_telnet_active;
extern unsigned char is_telnet_routed;
extern unsigned char CONF_radio_modulation;
extern unsigned char CONF_radio_frequency;
extern unsigned char CONF_frequency_band;
extern unsigned char CONF_radio_network_ID;
extern unsigned short int CONF_frequency_HD;
extern short int CONF_freq_shift;
extern unsigned char CONF_channel_TX;
extern unsigned char CONF_channel_RX;
extern unsigned char CONF_SI4463_freq_conf_RX[15];
extern unsigned char CONF_SI4463_freq_conf_TX[15];
extern unsigned char CONF_transmission_method;
extern unsigned char CONF_master_FDD;
extern unsigned long int CONF_master_down_IP;
extern unsigned int CONF_TDMA_frame_duration;
extern unsigned int CONF_TDMA_slot_duration;
extern unsigned int CONF_reduced_TDMA_slot_duration;
extern unsigned int CONF_TDMA_slot_margin;
extern unsigned int CONF_TR_margain;
extern unsigned int CONF_TA_margain;
extern unsigned int CONF_preamble_duration_for_decide;
extern unsigned int CONF_long_preamble_duration_for_TA;
extern unsigned int CONF_byte_duration;
extern unsigned int CONF_additional_preamble;
extern unsigned long int CONF_radio_timeout;
extern unsigned long int CONF_radio_timeout_small;
extern unsigned int offset_time_TX_slave; 
extern unsigned int time_next_TX_slave;
extern unsigned int time_max_TX_burst;
extern int CONF_delay_prepTX1_2_TX;
extern unsigned char my_radio_client_ID;
extern int CONF_Tx_rframe_timeout;
extern int CONF_signaling_period;
extern int is_SRAM_ext;

extern int DEBUG_max_rx_size_w5500;

extern unsigned int TDMA_slave_last_master_top;

// *** STATISTIC and DEBUG variables ***
//extern unsigned int debug_counter;
extern unsigned int RX_top_FDD_up_counter;
extern int RX_Eth_IPv4_counter;
extern int TX_radio_IPv4_counter;
extern int RX_radio_IPv4_counter;
extern unsigned int RSSI_total_stat;
extern unsigned int RSSI_stat_pkt_nb;

extern int slave_new_burst_tx_pending;

extern Timer GLOBAL_timer;

// *** ARP and DHCP and "routing" ***
struct LAN_conf_T { 
	
	//unsigned char modem_MAC[6]; 
	unsigned long int LAN_modem_IP;
	unsigned long int DHCP_range_start;
	unsigned long int DHCP_range_size; //4
	unsigned long int LAN_subnet_mask;
	unsigned long int LAN_def_route;
	unsigned char LAN_def_route_activ;
	unsigned char LAN_DNS_activ;
	unsigned long int LAN_DNS_value;
	unsigned char DHCP_server_active;
};

extern LAN_conf_T LAN_conf_saved;
extern LAN_conf_T LAN_conf_applied; 

extern unsigned char CONF_modem_MAC[6]; 

#define radio_addr_table_size 7
extern char CONF_radio_my_callsign[16];
extern char CONF_radio_master_callsign[16];
extern unsigned long int CONF_radio_addr_table_IP_begin[radio_addr_table_size];
extern unsigned long int CONF_radio_addr_table_IP_size[radio_addr_table_size];
extern char CONF_radio_addr_table_callsign[radio_addr_table_size][16];
extern char CONF_radio_addr_table_status[radio_addr_table_size];
extern unsigned int CONF_radio_addr_table_date[radio_addr_table_size];
extern unsigned long int CONF_radio_IP_start;
extern unsigned long int CONF_radio_IP_size;
extern unsigned int my_client_radio_connexion_state;
extern unsigned int my_client_radio_connexion_date;
extern unsigned char G_connect_rejection_reason;
extern int G_temperature_SI4463;
extern int G_need_temperature_check;
extern unsigned long int CONF_radio_IP_size_requested;
extern unsigned char CONF_radio_static_IP_requested;
extern unsigned char CONF_radio_state_ON_OFF;
extern unsigned char CONF_radio_default_state_ON_OFF;
extern unsigned char CONF_radio_PA_PWR;
extern unsigned char CONF_preamble_TX_long;
extern unsigned char CONF_preamble_TX_short;
extern unsigned long int last_rframe_seen;

extern long int TDMA_table_TA[radio_addr_table_size];

extern unsigned short int G_downlink_RSSI; 
extern unsigned short int G_radio_addr_table_RSSI[radio_addr_table_size]; 
extern unsigned short int G_downlink_BER; 
extern unsigned short int G_radio_addr_table_BER[radio_addr_table_size]; 
extern short int G_downlink_TA; 
//extern short int G_radio_addr_table_TA[radio_addr_table_size]; 
extern int G_downlink_bandwidth_temp;
extern int G_uplink_bandwidth_temp;

extern int super_debug;

extern unsigned char parity_bit_elab[128];
extern unsigned char parity_bit_check[256];

#endif