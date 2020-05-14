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

#ifndef SI4463_F4
#define SI4463_F4

#include "mbed.h"
#include "SI4463.h"

#define SI4463_offset_size 90
#define SI4463_CONF_RX_FIFO_threshold 90 
//64
#define SI4463_CONF_TX_FIFO_threshold 90
#define SI4463_CONF_max_field2_size 345 
//63

//#define SI4463_time_byte 8
//#define SI4463_zero_frame_time 590

struct SI4463_Chip{
    SPI* spi;
    DigitalOut* cs;
	InterruptIn* interrupt;
	int RX_TX_state; //0:nothing 1:RX 2:TX
	DigitalOut* RX_LED;
	DigitalOut* SDN;
};

int SI4463_CTS_read_answer(SI4463_Chip* SI4463, unsigned char* data, int size, int timeout);

int SI4463_configure_from_22(SI4463_Chip* SI4463);
int SI4463_configure_from_13(SI4463_Chip* SI4463);
int SI4463_configure_from_23(SI4463_Chip* SI4463);
int SI4463_configure_from_14(SI4463_Chip* SI4463);
int SI4463_configure_from_24(SI4463_Chip* SI4463);

int SI4463_configure_from_h(SI4463_Chip* SI4463, unsigned char* radio_config_data);

int SI4463_set_power(SI4463_Chip* SI4463);

void SI4463_print_version(SI4463_Chip* SI4463);

void SI4463_FIFO_status(SI4463_Chip* SI4463, int* RX_FIFO_count, int* TX_FIFO_count, int reset);

//void SI4463_set_GPIO(SI4463_Chip* SI4463, unsigned char GPIO_st_2, unsigned char GPIO_st_3);

void SI4463_FIFO_write(SI4463_Chip* SI4463, unsigned char* data, int count);

void SI4463_FIFO_read(SI4463_Chip* SI4463, unsigned char* data, int size); 

void SI4463_change_state (SI4463_Chip* SI4463, unsigned char new_state);

void SI4463_start_RX (SI4463_Chip* SI4463, unsigned char channel);

void SI4463_start_TX (SI4463_Chip* SI4463, unsigned char channel, unsigned int size);
//void SI4463_start_TX (SI4463_Chip* SI4463, unsigned char channel, unsigned int size, unsigned char next_state); 
void SI4463_start_TX_repeat (SI4463_Chip* SI4463, unsigned char channel, unsigned int size);

void SI4463_read_FRR(SI4463_Chip* SI4463, unsigned char* data);

int SI4463_get_state(SI4463_Chip* SI4463);

//void SI4463_init_RX();

void SI4463_clear_IT(SI4463_Chip* SI4463, unsigned char PH_clear, unsigned char modem_clear);

void SI4463_set_TX_preamble_length (SI4463_Chip* SI4463, unsigned char preamble_length_val);

int SI4463_read_temperature(SI4463_Chip* SI4463);

int check_RSSI_without_packet (void);

void SI4463_periodic_temperature_check(SI4463_Chip* SI4463);

void SI4463_periodic_temperature_check_2(void); 

void SI4463_temp_check_init(void);

void SI4463_prepa_TX_1();

void SI4463_prepa_TX_2();

void SI4463_TX_to_RX_transition(void);

void SI4463_RX_timeout (void);

void SI4463_decide_new_TX_or_not(void);

void SI4463_TX_new_frame(unsigned char synchro);

void SI4463_HW_interrupt();

int SI4463_configure_all(void);

void SI4463_radio_start(void);
void RADIO_off(int need_disconnect);
void RADIO_on(int need_disconnect, int need_radio_reconfigure, int HMI_output);
void RADIO_off_if_necessary(int need_disconnect);
void RADIO_restart_if_necessary(int need_disconnect, int need_radio_reconfigure, int HMI_output);
void SI4432_TX_test(unsigned int req_duration);

void SI4463_set_frequency(float freq_base, float freq_step);
void RADIO_compute_freq_params();

#endif