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

#include "mbed.h"
#include "global_variables.h"
#include "TDMA.h"
#include "HMI_telnet.h"
#include "L1L2_radio.h"
 
#ifdef FREQ_BAND_2M
	#include "SI4463_config_20_2m.h"
	#include "SI4463_config_11_2m.h"
	#include "SI4463_config_21_2m.h"
	#include "SI4463_config_12_2m.h"
	#include "SI4463_config_22_2m.h"
	#include "SI4463_config_13_2m.h"
	#include "SI4463_config_23_2m.h"
	#include "SI4463_config_14_2m.h"
	#include "SI4463_config_24_2m.h"
#else 
	//#include "SI4463_config_10.h"
	#include "SI4463_config_20.h"
	//#include "radio_config_Si4463_20.h"
	#include "SI4463_config_11.h"
	//#include "radio_config_Si4463_11.h"
	#include "SI4463_config_21.h"
	//#include "radio_config_Si4463_21.h"
	#include "SI4463_config_12.h"
	#include "SI4463_config_22.h"
	//#include "radio_config_Si4463_22.h"
	#include "SI4463_config_13.h"
	//#include "radio_config_Si4463_13.h"
	#include "SI4463_config_23.h"
	//#include "radio_config_Si4463_23.h"
	#include "SI4463_config_14.h"
	#include "SI4463_config_24.h"
	//#include "radio_config_Si4463_24.h"
#endif 

//Timeout SI4463_prepa_TX_1_call;
Timeout SI4463_prepa_TX_2_call;
Timeout SI4463_1st_TX_call;
Timeout SI4463_RX_timeout_call; //2019_04_05
//Ticker SI4463_temp_check2_call;

//TX_buffer_struct* TX_buff; //data structure currently used to TX

static unsigned char SI_trash[150];
static unsigned char TX_temp_rframe[384];
static unsigned char* TX_frame_to_send;
static unsigned char TX_in_progress = 0;
static unsigned char TX_test_inprogress = 0;
//int TX_frame_pointer;

// Low level functions & drivers
void SI4463_send_command(SI4463_Chip* SI4463, unsigned char* data, int size) {
	unsigned char loc_RX[30];
	SI4463->cs->write(0);
	SI4463->spi->transfer_2(data, size, loc_RX, size);
	SI4463->cs->write(1);
	wait_us(1); 
}
 
int SI4463_CTS_read_answer(SI4463_Chip* SI4463, unsigned char* data, int size, int timeout) {
	unsigned char loc_RX[10];
	unsigned char loc_TX[10];
	int answer;
	int loops = 0;
	loc_TX[0] = 0x44;
	loops = 0;
	SI4463->cs->write(0);
	SI4463->spi->transfer_2(loc_TX, 2, loc_RX, 2);
	while ((loc_RX[1] != 0xFF) && (loops < timeout)) {
		SI4463->cs->write(1);
		wait_us(20);
		SI4463->cs->write(0);
		SI4463->spi->transfer_2(loc_TX, 2, loc_RX, 2);
		loops++;
	}
	if (size > 0) {
		SI4463->spi->transfer_2(loc_TX, size, data, size);
	}
	SI4463->cs->write(1);
	wait_us(1);
	if (loops >= timeout) {
		answer = 0; 
	} else {
		answer =1;
	}
	//printf (" loc answer:%i ", answer);
	return answer;
}

//** Higher level functions ***

// configures using RADIO_CONFIGURATION_DATA_ARRAY

//int SI4463_configure_from_10(SI4463_Chip* SI4463) {
//	unsigned char radio_config_data_10[1400] = RADIO_CONFIGURATION_DATA_ARRAY_10;//22
//	CONF_TDMA_frame_duration = 939000;
//	CONF_TDMA_slot_duration = 53400;
//	CONF_reduced_TDMA_slot_duration = 20400; 
//	CONF_TDMA_slot_margin = 300;
//	CONF_TR_margain = 1300; 
//	CONF_TA_margain = 2000;
//	CONF_preamble_duration_for_decide = 3100; //1090
//	CONF_long_preamble_duration_for_TA = 3833;//1420
//	CONF_byte_duration = 143;
//	CONF_preamble_TX_long = 22;
//	CONF_preamble_TX_short = 16;
//	CONF_Tx_rframe_timeout = 30;
//	CONF_radio_timeout_small = 10000000;//10sec
//	CONF_signaling_period = 1;
//	return SI4463_configure_from_h (SI4463, radio_config_data_10);
//}

int SI4463_configure_from_20(SI4463_Chip* SI4463) {
	unsigned char radio_config_data_20[1250] = RADIO_CONFIGURATION_DATA_ARRAY_20;//22
	//unsigned char radio_config_data_20[1400] = RADIO_CONFIGURATION_DATA_ARRAY;
	CONF_TDMA_frame_duration = 560000;
	CONF_TDMA_slot_duration = 31600;
	CONF_reduced_TDMA_slot_duration = 12700; 
	CONF_TDMA_slot_margin = 300;
	CONF_TR_margain = 1300; 
	CONF_TA_margain = 2000;
	CONF_preamble_duration_for_decide = 2670; //1090
	CONF_long_preamble_duration_for_TA = 3751;//1420
	CONF_byte_duration = 80;
	CONF_preamble_TX_long = 20;
	CONF_preamble_TX_short = 16;
	CONF_Tx_rframe_timeout = 120;//8sec
	CONF_radio_timeout = 60000000;
	CONF_radio_timeout_small = 10000000;//10sec previously 20
	CONF_signaling_period = 2;//4 sec
	return SI4463_configure_from_h (SI4463, radio_config_data_20);
}

int SI4463_configure_from_11(SI4463_Chip* SI4463) {
	unsigned char radio_config_data_11[1250] = RADIO_CONFIGURATION_DATA_ARRAY_11;//22
	//unsigned char radio_config_data_11[1400] = RADIO_CONFIGURATION_DATA_ARRAY;//22
	CONF_TDMA_frame_duration = 537000;
	CONF_TDMA_slot_duration = 30250;
	CONF_reduced_TDMA_slot_duration = 11700; 
	CONF_TDMA_slot_margin = 300;
	CONF_TR_margain = 1300; 
	CONF_TA_margain = 2000;
	CONF_preamble_duration_for_decide = 1650; //1090
	CONF_long_preamble_duration_for_TA = 2473;//2292
	CONF_byte_duration = 80;
	CONF_preamble_TX_long = 25;
	CONF_preamble_TX_short = 16;
	CONF_Tx_rframe_timeout = 120;//8sec
	CONF_radio_timeout = 60000000;
	CONF_radio_timeout_small = 10000000;//10sec previously 20
	CONF_signaling_period = 2;//4sec
	return SI4463_configure_from_h (SI4463, radio_config_data_11);
}

int SI4463_configure_from_21(SI4463_Chip* SI4463) {
	unsigned char radio_config_data_21[1250] = RADIO_CONFIGURATION_DATA_ARRAY_21;//22
	//unsigned char radio_config_data_21[1400] = RADIO_CONFIGURATION_DATA_ARRAY;//22
	CONF_TDMA_frame_duration = 294500;
	CONF_TDMA_slot_duration = 16300;
	CONF_reduced_TDMA_slot_duration = 7050; 
	CONF_TDMA_slot_margin = 300;
	CONF_TR_margain = 1300; 
	CONF_TA_margain = 2000;
	CONF_preamble_duration_for_decide = 1550; //1090
	CONF_long_preamble_duration_for_TA = 2370;//1420
	CONF_byte_duration = 30;
	CONF_preamble_TX_long = 25;
	CONF_preamble_TX_short = 16;
	CONF_Tx_rframe_timeout = 90;//6 sec
	CONF_radio_timeout = 60000000;
	CONF_radio_timeout_small = 5000000;//5sec previously 10sec
	CONF_signaling_period = 1;//2sec
	return SI4463_configure_from_h (SI4463, radio_config_data_21);
}

int SI4463_configure_from_12(SI4463_Chip* SI4463) {
	unsigned char radio_config_data_12[1250] = RADIO_CONFIGURATION_DATA_ARRAY_12;//22
	CONF_TDMA_frame_duration = 312000;
	CONF_TDMA_slot_duration = 17300;
	CONF_reduced_TDMA_slot_duration = 7150; 
	CONF_TDMA_slot_margin = 300;
	CONF_TR_margain = 1300; 
	CONF_TA_margain = 2000;
	CONF_preamble_duration_for_decide = 1030; //1090
	CONF_long_preamble_duration_for_TA = 1765;//1420
	CONF_byte_duration = 45;
	CONF_preamble_TX_long = 32;
	CONF_preamble_TX_short = 16;
	CONF_Tx_rframe_timeout = 100;//6sec
	CONF_radio_timeout = 60000000;
	CONF_radio_timeout_small = 5000000;//5sec previously 10sec
	CONF_signaling_period = 1;//2sec
	return SI4463_configure_from_h (SI4463, radio_config_data_12);
}

int SI4463_configure_from_22(SI4463_Chip* SI4463) {
	unsigned char radio_config_data_22[1250] = RADIO_CONFIGURATION_DATA_ARRAY_22;//22
	//unsigned char radio_config_data_22[1400] = RADIO_CONFIGURATION_DATA_ARRAY;
	CONF_TDMA_frame_duration = 176000;
	CONF_TDMA_slot_duration = 9480;
	CONF_reduced_TDMA_slot_duration = 4330; 
	CONF_TDMA_slot_margin = 300;
	CONF_TR_margain = 1300; 
	CONF_TA_margain = 2000;
	CONF_preamble_duration_for_decide = 890; //1090
	CONF_long_preamble_duration_for_TA = 1708;//1420
	CONF_byte_duration = 23;
	CONF_preamble_TX_long = 32;
	CONF_preamble_TX_short = 16;
	CONF_Tx_rframe_timeout = 60;//4 sec
	CONF_radio_timeout = 40000000;
	CONF_radio_timeout_small = 5000000;//5sec previously 10sec
	CONF_signaling_period = 1;
	return SI4463_configure_from_h (SI4463, radio_config_data_22);
}

int SI4463_configure_from_13(SI4463_Chip* SI4463) {
	unsigned char radio_config_data_13[1250] = RADIO_CONFIGURATION_DATA_ARRAY_13;
	CONF_TDMA_frame_duration = 197000;
	CONF_TDMA_slot_duration = 10720;
	CONF_reduced_TDMA_slot_duration = 4540; 
	CONF_TDMA_slot_margin = 300;
	CONF_TR_margain = 1300; 
	CONF_TA_margain = 2000;
	CONF_preamble_duration_for_decide = 590; 
	CONF_long_preamble_duration_for_TA = 1400;
	CONF_byte_duration = 27;
	CONF_preamble_TX_long = 42;
	CONF_preamble_TX_short = 16;
	CONF_Tx_rframe_timeout = 60;//4 sec
	CONF_radio_timeout = 40000000;
	CONF_radio_timeout_small = 5000000;//5sec previously 10sec
	CONF_signaling_period = 1;
	return SI4463_configure_from_h (SI4463, radio_config_data_13);
}

int SI4463_configure_from_23(SI4463_Chip* SI4463) {
	unsigned char radio_config_data_23[1250] = RADIO_CONFIGURATION_DATA_ARRAY_23;//23
	//unsigned char radio_config_data_23[1500] = RADIO_CONFIGURATION_DATA_ARRAY;
	CONF_TDMA_frame_duration = 117000;
	CONF_TDMA_slot_duration = 6090;
	CONF_reduced_TDMA_slot_duration = 3000; 
	CONF_TDMA_slot_margin = 300;
	CONF_TR_margain = 1300; 
	CONF_TA_margain = 2000;
	CONF_preamble_duration_for_decide = 570;
	CONF_long_preamble_duration_for_TA = 1365;	
	CONF_byte_duration = 14;//13.3
	CONF_preamble_TX_long = 42;
	CONF_preamble_TX_short = 16;
	CONF_Tx_rframe_timeout = 30;//30!!!
	CONF_radio_timeout = 30000000;
	CONF_radio_timeout_small = 5000000;//5sec previously 10sec
	CONF_signaling_period = 1;
	return SI4463_configure_from_h (SI4463, radio_config_data_23);
}

int SI4463_configure_from_14(SI4463_Chip* SI4463) {
	unsigned char radio_config_data_14[1250] = RADIO_CONFIGURATION_DATA_ARRAY_14;//14
	CONF_TDMA_frame_duration = 130000;
	CONF_TDMA_slot_duration = 6840;
	CONF_reduced_TDMA_slot_duration = 3130; 
	CONF_TDMA_slot_margin = 300;
	CONF_TR_margain = 1300; 
	CONF_TA_margain = 2000;
	CONF_preamble_duration_for_decide = 400; 
	CONF_long_preamble_duration_for_TA = 1206; 
	CONF_byte_duration = 16;
	CONF_preamble_TX_long = 60;
	CONF_preamble_TX_short = 16;
	CONF_Tx_rframe_timeout = 30;
	CONF_radio_timeout = 30000000;
	CONF_radio_timeout_small = 5000000;//5sec previously 10sec
	CONF_signaling_period = 1;
	return SI4463_configure_from_h (SI4463, radio_config_data_14);
}

int SI4463_configure_from_24(SI4463_Chip* SI4463) {
	unsigned char radio_config_data_24[1250] = RADIO_CONFIGURATION_DATA_ARRAY_24;//24
	//unsigned char radio_config_data_24[1500] = RADIO_CONFIGURATION_DATA_ARRAY;//24
	CONF_TDMA_frame_duration = 81300;//81300
	CONF_TDMA_slot_duration = 4060;
	CONF_reduced_TDMA_slot_duration = 2210; 
	CONF_TDMA_slot_margin = 300;
	CONF_TR_margain = 1300; 
	CONF_TA_margain = 2000;
	CONF_preamble_duration_for_decide = 390; 
	CONF_long_preamble_duration_for_TA = 1188; 
	CONF_byte_duration = 8;
	CONF_preamble_TX_long = 60; 
	CONF_preamble_TX_short = 16;
	CONF_Tx_rframe_timeout = 30;
	CONF_radio_timeout = 30000000;
	CONF_radio_timeout_small = 5000000;//5sec previously 10sec
	CONF_signaling_period = 1;
	return SI4463_configure_from_h (SI4463, radio_config_data_24);
}

int SI4463_configure_from_h(SI4463_Chip* SI4463, unsigned char* radio_config_data) {
	
	int i=0;
	int answer_loc;
	int answer;
	int current_command_length;
	answer = 1;
	do {
		current_command_length = radio_config_data[i];
		i++;
		if (current_command_length>0) {
			SI4463_send_command(SI4463, radio_config_data+i, current_command_length);
			answer_loc = SI4463_CTS_read_answer(SI4463, SI_trash, 0, 30000);//15000 30000
			if (answer_loc == 0) {answer = 0;}
		}
		

		i = i + current_command_length;
		//
	} while ( (current_command_length > 0) && (answer == 1) );
	wait_ms(5);
	
	// specific GLOBAL_CONFIG : SEQUENCER_MODE=GUARANTEED and FIFO_MODE=FIFO_129
	unsigned char radio_config_bis[10] = {0x11, 0x00, 0x01, 0x03, 0x10}; 
	SI4463_send_command(SI4463, radio_config_bis, 5);
	answer_loc = SI4463_CTS_read_answer(SI4463, SI_trash, 0, 800);//200
	if (answer_loc == 0) {answer = 0;}
	wait_ms(5);//50
	
	// specific Max size field 2 
	unsigned char radio_config_ter[10] = {0x11, 0x12, 0x02, 0x11, 0x00, 0xFF}; 
	radio_config_ter[4] = (SI4463_CONF_max_field2_size & 0x1F00 )/ 0x100 ;
	radio_config_ter[5] = SI4463_CONF_max_field2_size & 0x00FF;
	SI4463_send_command(SI4463, radio_config_ter, 6);
	answer_loc = SI4463_CTS_read_answer(SI4463, SI_trash, 0, 800);
	if (answer_loc == 0) {answer = 0;}
	wait_ms(5);//50
	
	//specific FIFO Threshold
	unsigned char radio_config_quart[10] = {0x11, 0x12, 0x02, 0x0B, 0x30, 0x64}; 
	radio_config_quart[4] = SI4463_CONF_TX_FIFO_threshold & 0x7F;// ajout 30 mai
	radio_config_quart[5] = SI4463_CONF_RX_FIFO_threshold & 0x7F;
	SI4463_send_command(SI4463, radio_config_quart, 6);
	answer_loc = SI4463_CTS_read_answer(SI4463, SI_trash, 0, 800);
	if (answer_loc == 0) {answer = 0;}
	wait_ms(5);//50
	
	//specific SYNC WORD depending on 'radio network ID'
	unsigned char hash_netID[20] = {0x33, 0x36, 0x39, 0x3c, 0x63, 0x66, 0x69, 0x6c, 0x93, 0x96, 0x99, 0x9c, 0xc3, 0xc6, 0xc9, 0xcc};
	unsigned char radio_config_quint[12] = {0x11, 0x11, 0x01, 0x03, 0xCC}; 
	radio_config_quint[4] = hash_netID[CONF_radio_network_ID];
	SI4463_send_command(SI4463, radio_config_quint, 5);//10
	answer_loc = SI4463_CTS_read_answer(SI4463, SI_trash, 0, 800);
	answer_loc = 1;
	if (answer_loc == 0) {answer = 0;}
	wait_ms(5);//5
	
	//specific power 
	unsigned char radio_config_sixt[10] = {0x11, 0x22, 0x01, 0x01, 0x7F}; 
	radio_config_sixt[4] = CONF_radio_PA_PWR & 0x7F;
	SI4463_send_command(SI4463, radio_config_sixt, 5);
	answer_loc = SI4463_CTS_read_answer(SI4463, SI_trash, 0, 800);
	if (answer_loc == 0) {answer = 0;}
	wait_ms(5);//5
	
	return answer;
}

int SI4463_set_power(SI4463_Chip* SI4463) {
	int answer, answer_loc;
	unsigned char radio_config_sixt[10] = {0x11, 0x22, 0x01, 0x01, 0x7F}; 
	radio_config_sixt[4] = CONF_radio_PA_PWR & 0x7F;
	SI4463_send_command(SI4463, radio_config_sixt, 5);
	answer_loc = SI4463_CTS_read_answer(SI4463, SI_trash, 0, 200);
	if (answer_loc == 0) {answer = 0;}
	wait_ms(5);
	return answer;
}

void SI4463_print_version(SI4463_Chip* SI4463) {
	int i;
	unsigned char command [10];
	unsigned char answer [12];
	command[0] = 0x01;//part info
	SI4463_send_command(SI4463, command, 1);
	SI4463_CTS_read_answer(SI4463, answer, 8, 200);
	HMI_printf("part info: ");
	for (i=0; i<8; i++) {
		HMI_printf("%02X ",answer[i]);
	}
	printf("\r\n");
	
	command[0] = 0x10;//function info
	SI4463_send_command(SI4463, command, 1);
	SI4463_CTS_read_answer(SI4463, answer, 8, 200);
	HMI_printf("function info: ");
	for (i=0; i<6; i++) {
		HMI_printf("%02X ",answer[i]);
	}
	printf("\r\n");
}

void SI4463_FIFO_status(SI4463_Chip* SI4463, int* RX_FIFO_count, int* TX_FIFO_count, int reset) {
	unsigned char command[10];
	unsigned char answer[10];
	command[0] = 0x15;
	if (reset == 1) {
		command[1] = 0x03;
	} else {
		command[1] = 0x00;
	}
	SI4463_send_command (SI4463, command, 2);
	SI4463_CTS_read_answer (SI4463, answer, 2, 5);
	*RX_FIFO_count = answer[0];
	*TX_FIFO_count = answer[1];
}

// a supprimer
void SI4463_FIFO_write(SI4463_Chip* SI4463, unsigned char* data, int size) {
	static unsigned char trash [200];
	unsigned char command[6];
	command[0] = 0x66;
	SI4463->cs->write(0);
	SI4463->spi->transfer_2 (command, 1, trash, 1);
	SI4463->spi->transfer_2 (data, size, trash, size);
	SI4463->cs->write(1);
	wait_us(1);
}

void SI4463_FIFO_read (SI4463_Chip* SI4463, unsigned char* data, int size) {
	unsigned char trash [200]; //static
	unsigned char command[6];
	command[0] = 0x77;
	SI4463->cs->write(0);
	SI4463->spi->transfer_2 (command, 1, trash, 1);
	SI4463->spi->transfer_2 (trash, size, data, size);
	SI4463->cs->write(1);
	wait_us(1);
}
//fin a supprimer

void SI4463_change_state (SI4463_Chip* SI4463, unsigned char new_state) {
	unsigned char command[6];
	command[0] = 0x34;
	command[1] = new_state;
	SI4463_send_command(SI4463, command, 2);
	SI4463_CTS_read_answer (SI4463, command, 0, 30);
}

void SI4463_start_RX (SI4463_Chip* SI4463, unsigned char channel) {
	static unsigned char command[12] = {0x32, 0, 0, 0, 0, 0x08, 0x08, 0x08};
	command[1] = channel;
	SI4463_send_command(SI4463, command, 8);
	SI4463_CTS_read_answer (SI4463, command, 0, 55);
}

void SI4463_start_TX (SI4463_Chip* SI4463, unsigned char channel, unsigned int size) {
	static unsigned char command[12] = {0x31, 0, 0x50, 0, 0, 0, 0};
	
	command[1] = channel; 
	command[3] = (size & 0x1F00) >> 8;
	command[4] = size & 0xFF;
	SI4463_send_command(SI4463, command, 8);
}

void SI4463_read_FRR(SI4463_Chip* SI4463, unsigned char* data) {
	unsigned char command[8];
	unsigned char loc_answer[8];
	int i;
	command[0] = 0x50;
	SI4463->cs->write(0);
	SI4463->spi->transfer_2(command, 5, loc_answer, 5);
	SI4463->cs->write(1);
	wait_us(1);
	for (i=0; i<4; i++) {
		data[i] = loc_answer[i+1];
	}
}

int SI4463_get_state(SI4463_Chip* SI4463) {
	unsigned char command[8];
	unsigned char loc_answer[8];
	command[0] = 0x33;
	SI4463_send_command(SI4463, command, 1);
	SI4463_CTS_read_answer (SI4463, loc_answer, 2, 55);//!!! previous 5 / 2020_02_22
	return loc_answer[0];
}

void SI4463_clear_IT(SI4463_Chip* SI4463, unsigned char PH_clear, unsigned char modem_clear) {
	unsigned char command[8];
	//unsigned char loc_answer[8];
	
	command[0] = 0x20; // GET_INT
	//command[1] = 0x00;
	command[1] = PH_clear;
	command[2] = modem_clear;
	command[3] = 0x00;
	
	SI4463_send_command(SI4463, command, 4);
	//wait_us(20);
	//SI4463_CTS_read_answer (SI4463, loc_answer, 2, 5);
	//return loc_answer[0];
}

void SI4463_set_TX_preamble_length (SI4463_Chip* SI4463, unsigned char preamble_length_val) {
	unsigned char trash [10];
	unsigned char command_preamble_length[10] = {0x11, 0x10, 0x01, 0x00, 0x20};

	command_preamble_length [4] = preamble_length_val;
	SI4463_send_command(G_SI4463, command_preamble_length, 5); 
	wait_us(20); // !!!
	SI4463_CTS_read_answer(G_SI4463, trash, 0, 20); 
}

int SI4463_read_temperature(SI4463_Chip* SI4463) {
	int temperature; 
	unsigned char answer_loc [10];
	unsigned char command_temp_read[10] = {0x14, 0x10, 0xA0};
	SI4463_send_command(SI4463, command_temp_read, 3);
	//wait_ms(2);
	wait_us(20);
	if (SI4463_CTS_read_answer(SI4463, answer_loc, 6, 2000) ) {
		temperature = answer_loc[4] * 256 + answer_loc[5];
		temperature = temperature * 0.2195 - 293;
	} else {
		temperature = 0xFFFF;
	}
	return temperature;
}

static int previous_temperature = 300;

void SI4463_periodic_temperature_check(SI4463_Chip* SI4463) {
	// int need_recalibrate;
	int delta_temperature;
	int i;
	
	//RADIO_off_if_necessary(0);
	G_temperature_SI4463 = SI4463_read_temperature (SI4463);
	if (previous_temperature == 300) {
		previous_temperature = G_temperature_SI4463;
	}
	delta_temperature = G_temperature_SI4463 - previous_temperature;
	if ( ( delta_temperature > 14) || (delta_temperature < -14) ) {
	//if (1) {
		//  need_recalibrate = 1;
		i = SI4463_configure_all();
		previous_temperature = G_temperature_SI4463;
		if (i == 0) {//fail to recalibrate
			NVIC_SystemReset();
		}
	} else {
		//  need_recalibrate = 0;
	}
	//RADIO_restart_if_necessary(0, need_recalibrate, 0); //0,need,0
}

static unsigned int RX_size_remaining = 0;

void SI4463_periodic_temperature_check_2(void) {
	unsigned long int timer_snapshot;
	int delta_temperature;
	int i;
	unsigned char trash [10];
	timer_snapshot = GLOBAL_timer.read_us();
	if ( (is_TDMA_master) && (CONF_master_FDD == 2) && (CONF_radio_state_ON_OFF) ) {
		SI4463_prepa_TX_1_call.attach_us(&SI4463_prepa_TX_1, 500000);//simulates TX transition
	}
	else if ( (is_TDMA_master) && (CONF_radio_state_ON_OFF) && ((timer_snapshot-last_rframe_seen) > (CONF_radio_timeout+1000000) ) ) {
		//if (G_need_temperature_check == 1) {
			G_need_temperature_check = 0;
			if (previous_temperature == 300) {
				previous_temperature = G_temperature_SI4463;
			}
			G_temperature_SI4463 = SI4463_read_temperature (G_SI4463);
			delta_temperature = G_temperature_SI4463 - previous_temperature;
			if ( ( delta_temperature > 14) || (delta_temperature < -14) ) {
				previous_temperature = G_temperature_SI4463;
				G_SI4463->RX_TX_state = 0;
				//SI4463_change_state(G_SI4463, 0x03); //switch to ready
				RX_FIFO_WR_point = RX_FIFO_last_received; // rewind WR_pointer to last complete packet
				//SI4463_FIFO_status(G_SI4463, &toto, &toto, 1); //reset FIFO
				//SI4463_clear_IT (G_SI4463, 0, 0);
				i = SI4463_configure_all();
				if (i == 0) {//fail to recalibrate
					NVIC_SystemReset();
				}
				//SI4463_start_RX(G_SI4463, CONF_radio_frequency);
				SI4463_start_RX(G_SI4463, CONF_channel_RX);
				SI4463_clear_IT (G_SI4463, 0, 0);
				SI4463_CTS_read_answer (G_SI4463, trash, 2, 5);// ADDED 2018 08 25
				RX_size_remaining = 0;
				G_SI4463->RX_TX_state = 1; // activate RX HW IRQ
			}
		//}
	}
}

//void SI4463_temp_check_init(void) {
//	SI4463_temp_check2_call.attach(&SI4463_periodic_temperature_check_2, 10);//periodic 10 seconds
//}

void SI4463_FIFO_RX_transfer(unsigned int size) {
	//unsigned char trash[150]; //static

	int size_to_read;

	if ( ( (RX_FIFO_WR_point & RX_FIFO_mask) +  size) > RX_FIFO_mask) { // to big for 1 step
		size_to_read = (RX_FIFO_mask + 1 - (RX_FIFO_WR_point & RX_FIFO_mask) );
		G_SI4463->spi->transfer_2 (SI_trash, size_to_read, RX_FIFO_data + (RX_FIFO_WR_point & RX_FIFO_mask), size_to_read);
		RX_FIFO_WR_point = RX_FIFO_WR_point + size_to_read; // should become ZERO
		//second step
		size_to_read = size - size_to_read; 
		G_SI4463->spi->transfer_2 (SI_trash, size_to_read, RX_FIFO_data + (RX_FIFO_WR_point & RX_FIFO_mask), size_to_read);
		RX_FIFO_WR_point = RX_FIFO_WR_point + size_to_read;
	} else { //1 step is enough
	
		G_SI4463->spi->transfer_2 (SI_trash, size, RX_FIFO_data + (RX_FIFO_WR_point & RX_FIFO_mask), size);
		RX_FIFO_WR_point = RX_FIFO_WR_point + size;
	}

}

void SI4463_FIFO_TX_transfer(unsigned int size) {
	unsigned char command[5];
	G_SI4463->cs->write(0);
	command[0] = 0x66;
	G_SI4463->spi->transfer_2 (command, 1, SI_trash, 1);
	G_SI4463->spi->transfer_2 (TX_frame_to_send, size, SI_trash, size);
	TX_frame_to_send = TX_frame_to_send + size;
	G_SI4463->cs->write(1);
}

void SI4463_RX_HOP(void) {
	//G_SI4463->RX_LED->write(1);
	SI4463_send_command(G_SI4463, CONF_SI4463_freq_conf_RX, 7);
	wait_us(20);
	SI4463_CTS_read_answer (G_SI4463, SI_trash, 0, 50);
	wait_us(20);
	//G_SI4463->RX_LED->write(0);
}

void SI4463_TX_HOP(void) {
	//G_SI4463->RX_LED->write(1);
	SI4463_send_command(G_SI4463, CONF_SI4463_freq_conf_TX, 9);
	wait_us(20);
	SI4463_CTS_read_answer (G_SI4463, SI_trash, 0, 50);
	wait_us(20);
	//G_SI4463->RX_LED->write(0);
}

// High level functions

//static unsigned int RX_size_remaining = 0;
static unsigned int time_STOP_TX_burst;

void SI4463_RX_IT() {
	static unsigned char RX_small[10];
	static unsigned char TX_small[10];
	
	static unsigned char FRR[5];
	static unsigned int RX_timer;
	static unsigned char RSSI;
	int IT_SYNC_detected;
	int IT_FIFO_almost_full;
	int IT_pckt_RX;
	//What has been treated
	int Treated_SYNC_detected;
	int Treated_FIFO_almost_full;
	int Treated_pckt_RX;
	// synthesis
	int Synth_SYNC_detected;
	int Synth_FIFO_almost_full;
	int Synth_pckt_RX;
	
	unsigned int size_to_read; 
	int IT_state;
	int toto;
	unsigned int timer_snapshot;
	int t_rehabilit_interrupt;
	unsigned char clear_IT_PH;
	unsigned char clear_IT_modem;
	
	Treated_SYNC_detected = 0;
	Treated_FIFO_almost_full = 0;
	Treated_pckt_RX = 0;
	
	timer_snapshot = GLOBAL_timer.read_us();
	wait_us(10);//for RSSI propagation (measure at sync detect)
	do {
		SI4463_read_FRR (G_SI4463, FRR);
		IT_SYNC_detected = FRR[1] & 0x01;
		IT_FIFO_almost_full = FRR[0] & 0x01;
		IT_pckt_RX = (FRR[0] & 0x10) / 0x10; 
		Synth_SYNC_detected = IT_SYNC_detected ^ Treated_SYNC_detected; //Xor
		Synth_FIFO_almost_full = IT_FIFO_almost_full ^ Treated_FIFO_almost_full;
		Synth_pckt_RX = IT_pckt_RX ^ Treated_pckt_RX;
		
		toto = *(G_SI4463->interrupt);
		if (Synth_SYNC_detected) {//Sync detected
			RSSI = FRR[2];
			RX_timer = timer_snapshot - CONF_long_preamble_duration_for_TA;
			if ( (is_TDMA_master) && (CONF_master_FDD == 2) ) {//Master UP
				RX_timer = RX_timer - TDMA_slave_last_master_top;
			}
			Treated_SYNC_detected = 1;
			RSSI_total_stat = RSSI_total_stat + RSSI;
			RSSI_stat_pkt_nb++;
			G_SI4463->RX_LED->write(1);
		}
		
		if (Synth_FIFO_almost_full || Synth_pckt_RX) { //FIFO full or RX complete
			if ( (RX_size_remaining == 0) && (Treated_FIFO_almost_full == 0) && (Treated_pckt_RX == 0) ) { //beginning of new packet
				// read first byte of packet : size
				TX_small[0] = 0x77;
				TX_small[1] = toto;
				G_SI4463->cs->write(0);
				G_SI4463->spi->transfer_2 (TX_small, 2, RX_small, 2);
				RX_size_remaining = RX_small[1] + SI4463_offset_size;
				if (RX_size_remaining > SI4463_CONF_max_field2_size ) {
					RX_size_remaining = SI4463_CONF_max_field2_size;
				}
				if (RX_size_remaining > (SI4463_CONF_RX_FIFO_threshold - 1)) { //too big //+10
					size_to_read = SI4463_CONF_RX_FIFO_threshold - 1; //+10
				} else {
					size_to_read = RX_size_remaining;
				}
				if (Synth_pckt_RX) { // force to read all remaining if full packet received
					size_to_read = RX_size_remaining;
					G_SI4463->RX_LED->write(0);
				}

				RX_FIFO_data[RX_FIFO_WR_point & RX_FIFO_mask] = RX_timer & 0xFF; //LSB
				RX_FIFO_WR_point++;
				RX_FIFO_data[RX_FIFO_WR_point & RX_FIFO_mask] = (RX_timer & 0xFF00) / 0x100;
				RX_FIFO_WR_point++;
				RX_FIFO_data[RX_FIFO_WR_point & RX_FIFO_mask] = (RX_timer & 0xFF0000) / 0x10000; //MSB
				RX_FIFO_WR_point++;
				RX_FIFO_data[RX_FIFO_WR_point & RX_FIFO_mask] = RSSI; //RSSI
				RX_FIFO_WR_point++;
				RX_FIFO_data[RX_FIFO_WR_point & RX_FIFO_mask] = RX_small[1]; //Size. Warning, with negative offset
				RX_FIFO_WR_point++;
				//read remaining
				SI4463_FIFO_RX_transfer(size_to_read);
				G_SI4463->cs->write(1);
				wait_us(1);
				RX_size_remaining = RX_size_remaining - size_to_read; 
			} 
			else { // middle or end of a packet
				if (RX_size_remaining > SI4463_CONF_RX_FIFO_threshold ) { //too big  //+10 
					size_to_read = SI4463_CONF_RX_FIFO_threshold ;//+10
				} else {
					size_to_read = RX_size_remaining;
				}
				if (Synth_pckt_RX) { // force to read all remaining if full packet received
					size_to_read = RX_size_remaining;
					G_SI4463->RX_LED->write(0); 
				}
				if (RX_size_remaining > 0) { //avoid useless FIFO reading
					TX_small[0] = 0x77;
					G_SI4463->cs->write(0);
					G_SI4463->spi->transfer_2 (TX_small, 1, RX_small, 1);
					SI4463_FIFO_RX_transfer(size_to_read);
					G_SI4463->cs->write(1);
					wait_us(1);
					RX_size_remaining = RX_size_remaining - size_to_read; 
				}
			}
			if (RX_size_remaining == 0) { //packet is full, 
				RX_FIFO_last_received = RX_FIFO_WR_point;
			}
			Treated_FIFO_almost_full = IT_FIFO_almost_full;
			Treated_pckt_RX = IT_pckt_RX;
		}
		clear_IT_PH = ~(Treated_FIFO_almost_full * 0x01 + Treated_pckt_RX * 0x10);
		clear_IT_modem = ~(Treated_SYNC_detected * 0x01);
		SI4463_clear_IT(G_SI4463, clear_IT_PH, clear_IT_modem);
		// checks if new IT has triggered in the meantime
		if (IT_SYNC_detected) { //removed 2019_06_16
			IT_state = 1; // impossible to have immediate new IT
		} else {
			t_rehabilit_interrupt = 0;
			IT_state = G_SI4463->interrupt->read(); 
			while ( (t_rehabilit_interrupt<18) && (IT_state == 0) ) {
				wait_us(1);
				IT_state = G_SI4463->interrupt->read(); 
				t_rehabilit_interrupt++;
			}
		}
	} while (IT_state == 0);
	
}

//static int already_inside_IT = 0;

static int TX_slot_frame_counter=0;
static int radio_lock_TX_pending = 0;

void SI4463_prepa_TX_1(void) {
	unsigned long int timer_snapshot;
	G_SI4463->RX_LED->write(0); 
	G_PTT_PA_pin->write(1);
	timer_snapshot = GLOBAL_timer.read_us() + 50000;
	//if ( (CONF_radio_state_ON_OFF) && (radio_lock_TX_pending == 0) ) {
	if ( (CONF_radio_state_ON_OFF) ) {
		radio_lock_TX_pending = 1;
		G_SI4463->RX_TX_state = 0; //temporary inhibit actions from HW IRQ
		
		if ( (is_TDMA_master) && (CONF_master_FDD<2) && (CONF_radio_state_ON_OFF) && (((timer_snapshot-last_rframe_seen)&0x7FFFFFFF) < CONF_radio_timeout) ) {
		//if ( (is_TDMA_master) && (CONF_radio_state_ON_OFF) ) {
			SI4463_prepa_TX_1_call.attach_us(&SI4463_prepa_TX_1, CONF_TDMA_frame_duration);//master_TDMA_period
			//TDMA_top_measure();
		}
		SI4463_prepa_TX_2_call.attach_us(&SI4463_prepa_TX_2, 350); //300
		if (is_TDMA_master) {
			TDMA_master_allocation();
		}
	}
}

void SI4463_prepa_TX_2(void) {
	int toto;
	unsigned int loc_time;
	unsigned int loc_time_offset;
	unsigned char trash [10];

	if (CONF_radio_state_ON_OFF) {
		loc_time = GLOBAL_timer.read_us();
		if (is_TDMA_master) {
			loc_time_offset = 530; // 530
		} else {
			// //loc_time_offset = (time_next_TX_slave-loc_time) & 0xFFFFFF;
			loc_time_offset = 530; // 530
		}
		time_STOP_TX_burst = (loc_time + loc_time_offset + time_max_TX_burst);// & 0xFFFFFF; 
		SI4463_1st_TX_call.attach_us(&SI4463_decide_new_TX_or_not, loc_time_offset); 
		
		SI4463_change_state(G_SI4463, 0x05); //switch to TX_TUNE (with CTS)
		RX_FIFO_WR_point = RX_FIFO_last_received; // rewind WR_pointer to last complete packet
		SI4463_FIFO_status(G_SI4463, &toto, &toto, 1); //reset FIFO (including CTS)
		TX_slot_frame_counter = 0;
		SI4463_clear_IT (G_SI4463, 0, 0);//without CTS
		G_SI4463->RX_TX_state = 2; // activate TX HW IRQ
		wait_us(20); 
		SI4463_CTS_read_answer(G_SI4463, trash, 0, 20);
		SI4463_set_TX_preamble_length(G_SI4463, CONF_preamble_TX_long); 
	}
	
}

void SI4463_TX_to_RX_transition(void) {
	int toto;
	unsigned char trash[10];
	
	SI4463_FIFO_status(G_SI4463, &toto, &toto, 1); //reset FIFO
	G_SI4463->RX_TX_state = 0; // temporarly inhibit IRQ
	radio_lock_TX_pending = 0;
	
	if (G_need_temperature_check==1) {
		SI4463_periodic_temperature_check(G_SI4463);//added 2019_05_31
		G_need_temperature_check = 0;
	}
	SI4463_start_RX(G_SI4463, CONF_channel_RX); // with CTS
	SI4463_RX_HOP();// with CTS
	SI4463_clear_IT (G_SI4463, 0, 0);
	SI4463_CTS_read_answer (G_SI4463, trash, 2, 5);// ADDED 2018 08 25
	RX_size_remaining = 0;
	G_PTT_PA_pin->write(0);
	G_SI4463->RX_TX_state = 1; // activate RX HW IRQ
	if ( (!is_TDMA_master) && (CONF_radio_modulation == 24) ) {
		SI4463_RX_timeout_call.attach_us(&SI4463_RX_timeout, 10*CONF_TDMA_frame_duration);
	}
	TX_in_progress = 0;
}

void SI4463_RX_timeout (void) {
	unsigned int timer_snapshot;
	timer_snapshot = GLOBAL_timer.read_us();
	if ( (timer_snapshot - last_rframe_seen) > 9*CONF_TDMA_frame_duration) {
		SI4463_prepa_TX_1();
		last_rframe_seen = timer_snapshot;
	}
}

void Radio_purge_old_frames (void) { 
	int force_loop_exit = 0;
	unsigned int loc_size;
	unsigned char RX_frame_datation;
	unsigned char loc_time_char;
	unsigned int loc_time_int;
	loc_time_int = GLOBAL_timer.read_us();
	loc_time_char = (loc_time_int >> 16) & 0xFF; 
	while (force_loop_exit == 0) {
		if (TX_buff_intern_RD_pointer < TX_buff_intern_last_ready) {//data available in FIFO
			RX_frame_datation = TX_buff_intern_FIFOdata[TX_buff_intern_RD_pointer % 128][0];

			if ( (loc_time_char - RX_frame_datation) > CONF_Tx_rframe_timeout) {//CONF_Tx_rframe_timeout
				loc_size = TX_buff_intern_FIFOdata[TX_buff_intern_RD_pointer % 128][1];
				loc_size = loc_size + 2 + SI4463_offset_size;
				//+2 because timer byte and size byte
				if (loc_size <= 128) {
					TX_buff_intern_RD_pointer = TX_buff_intern_RD_pointer + 1;
				} else if (loc_size <= 256) {
					TX_buff_intern_RD_pointer = TX_buff_intern_RD_pointer + 2;
				} else {
					TX_buff_intern_RD_pointer = TX_buff_intern_RD_pointer + 3;
				}
				//printf("purged!\r\n");
				//printf("date1 %i date2 %i\r\n", loc_time_char, RX_frame_datation); 
			}
			else { //packet is recent
				force_loop_exit = 1;
			}
		} 
		else { //no data available in FIFO
			force_loop_exit = 1;
		}
	}
}

int TX_test_mode = 0;


void SI4463_decide_new_TX_or_not (void) { //decides if new frame must be transmitted, and which frame
	unsigned int loc_size;
	unsigned int loc_time;
	unsigned char TDMA_sync;
	int delta_end_burst; // >0 not enough time

	int OK_send_PS;
	int PS_data_available;
	slave_new_burst_tx_pending = 0;
	OK_send_PS = 0;
	PS_data_available = 0;
	Radio_purge_old_frames();
	if (TX_buff_intern_RD_pointer < TX_buff_intern_last_ready) {//data available in TXPS
		PS_data_available = 1;
		loc_size = TX_intern_FIFO_get_lastfrzize();
	} else {
		loc_size = 120; 
	}
		
	loc_time = GLOBAL_timer.read_us();
	delta_end_burst = loc_time + (loc_size * CONF_byte_duration) + CONF_preamble_duration_for_decide - time_STOP_TX_burst - 50; 
	if (TX_slot_frame_counter == 0) {
		delta_end_burst = delta_end_burst + CONF_additional_preamble;
	}
	
	//printf("%i\r\n", delta_end_burst);
	if (delta_end_burst < 0) { 
		OK_send_PS = 1;
	} 
	
	if (TX_slot_frame_counter == 0) {
		TDMA_sync = 1;
	} else {
		TDMA_sync = 0;
	}
	
	if (CONF_radio_state_ON_OFF==0) {//radio off check for TX test
		if (TX_test_inprogress == 1) {
			TX_frame_to_send = TX_TDMA_intern_data;
			SI4463_TX_new_frame(TDMA_sync); 
		} else {
			SI4463_TX_to_RX_transition();
		}
	} 
	else { //normal operation
		if (is_TDMA_master) {
			if (CONF_master_FDD==2) {
				SI4463_TX_to_RX_transition();//Master FDD up, go immediately to RX
			}
			else if (TX_slot_frame_counter == 0) { // systematically send TDMA signaling frame
				TX_frame_to_send = TX_TDMA_intern_data;
				TDMA_top_measure();
				//G_PTT_PA_pin->write(1);
				if (CONF_master_FDD == 1) {//Master FDD downlink
					G_FDD_trig_pin->write(1);
				}
				SI4463_TX_new_frame(TDMA_sync); 
				SI4463_set_TX_preamble_length(G_SI4463, CONF_preamble_TX_short);
				TX_slot_frame_counter++;
			} 
			else if (PS_data_available && OK_send_PS) {
				TX_intern_FIFO_read (TX_temp_rframe);
				TX_frame_to_send = TX_temp_rframe;
				SI4463_TX_new_frame(TDMA_sync); 
				TX_slot_frame_counter++;
			}
			else {
				TX_slot_frame_counter = 0;
				//G_PTT_PA_pin->write(0);
				if (CONF_master_FDD == 1) {//Master FDD downlink
					G_FDD_trig_pin->write(0);
				}
				SI4463_TX_to_RX_transition();
			}
			
		} else { // SLAVE
			if ( (PS_data_available==0) && (TX_slot_frame_counter==0) && (my_client_radio_connexion_state==2) ) {//send a null frame
				TX_frame_to_send = TX_TDMA_intern_data;
				SI4463_TX_new_frame(TDMA_sync); 
				if (TX_slot_frame_counter == 0) {
					SI4463_set_TX_preamble_length(G_SI4463, CONF_preamble_TX_short);
				}
				TX_slot_frame_counter++;
			} 
			else if ( (PS_data_available) && (OK_send_PS) ) {
				TX_intern_FIFO_read (TX_temp_rframe);
				TX_frame_to_send = TX_temp_rframe;
				SI4463_TX_new_frame(TDMA_sync); 
				if (TX_slot_frame_counter == 0) {
					SI4463_set_TX_preamble_length(G_SI4463, CONF_preamble_TX_short);
				}
				TX_slot_frame_counter++;
			} 
			else {
				TX_slot_frame_counter = 0;
				SI4463_TX_to_RX_transition();
			}
		}
	}
}


static unsigned int TX_size_remaining = 0;

void SI4463_TX_new_frame(unsigned char synchro) {
	//unsigned char timer_coarse;
	unsigned int full_packet_size;
	unsigned int size_to_send;
	unsigned char trash[10];

	TX_in_progress = 1; 
	//prefill TX FIFO with small amount of data	
	TX_frame_to_send++; //1st byte ignored timer coarse
	TX_size_remaining = TX_frame_to_send[0] + 1 + SI4463_offset_size;
	full_packet_size = TX_size_remaining; 
		
	TX_frame_to_send[1] = TDMA_byte_elaboration(synchro);
	SI4463_FIFO_TX_transfer(30);
	TX_size_remaining = TX_size_remaining - 30;
	
	//start TX order
	SI4463_start_TX (G_SI4463, CONF_channel_TX, full_packet_size);

	//FIFO transfer
	if (TX_size_remaining < 95) { //sent in 1 pass //95 
		size_to_send = TX_size_remaining;
	} else {
		size_to_send = 95; //95 
	}
	SI4463_FIFO_TX_transfer(size_to_send);
	
	//CTS
	SI4463_CTS_read_answer (G_SI4463, trash, 0, 55);//TEST previously 5
	TX_size_remaining = TX_size_remaining - size_to_send;
}

void SI4463_HW_TX_IT() {
	static unsigned char FRR[5];
	int size_to_write;
	
	int IT_FIFO_almost_empty;
	int IT_pckt_sent;
	int Treated_FIFO_almost_empty;
	int Treated_pckt_sent;
	int Synth_FIFO_almost_empty;
	int Synth_pckt_sent;
	int IT_state;
	int t_rehabilit_interrupt;
	unsigned char clear_IT_PH;
	
	Treated_FIFO_almost_empty = 0;
	Treated_pckt_sent = 0;
	
	do {
		SI4463_read_FRR (G_SI4463, FRR);
		IT_FIFO_almost_empty = (FRR[0] & 0x02 ) /0x02;
		IT_pckt_sent = (FRR[0] & 0x20 ) /0x20;
	
		Synth_FIFO_almost_empty = IT_FIFO_almost_empty ^ Treated_FIFO_almost_empty;
		Synth_pckt_sent = IT_pckt_sent ^ Treated_pckt_sent;
		
		if (Synth_pckt_sent) {  
			TX_in_progress = 0;
			SI4463_decide_new_TX_or_not();
			Treated_pckt_sent = 1;
			Treated_FIFO_almost_empty = IT_FIFO_almost_empty; // trick 
		} 
		Synth_FIFO_almost_empty = IT_FIFO_almost_empty ^ Treated_FIFO_almost_empty;
		if (Synth_FIFO_almost_empty) {
			// FIFO almost empty
			if (TX_size_remaining > 0) {//more data to send on this packet
				if (TX_size_remaining > SI4463_CONF_TX_FIFO_threshold) {//not enough space in FIFO for full packet
					size_to_write = SI4463_CONF_TX_FIFO_threshold;
				} else {
					size_to_write = TX_size_remaining;
				}
				SI4463_FIFO_TX_transfer (size_to_write);
				TX_size_remaining = TX_size_remaining - size_to_write;
			}
			Treated_FIFO_almost_empty = 1; //even if nothing more to send
		}
		
		clear_IT_PH = ~(Treated_FIFO_almost_empty * 0x02 + Treated_pckt_sent * 0x20);
		SI4463_clear_IT(G_SI4463, clear_IT_PH, 0x00);
		//wait_us(25);
		t_rehabilit_interrupt = 0;
		IT_state = G_SI4463->interrupt->read(); 
		while ( (t_rehabilit_interrupt<25) && (IT_state == 0) ) {
			wait_us(1);
			IT_state = G_SI4463->interrupt->read(); 
			t_rehabilit_interrupt++;
		}
	} while (IT_state == 0);

}

void SI4463_HW_interrupt() {
	if ( (G_SI4463->RX_TX_state == 1) && (CONF_radio_state_ON_OFF) ) { //Receive
		SI4463_RX_IT();
	}
	if (G_SI4463->RX_TX_state == 2) { //transmit
		SI4463_HW_TX_IT();
	}
}

int SI4463_configure_all(void) {
	int answer_loc = 0;
	int i = 0;
	G_SI4463->SDN->write(0);
	wait_ms(10);//100
	G_SI4463->SDN->write(1);
	wait_ms(500);//100 20
	G_SI4463->SDN->write(0);
	wait_ms(200);//500 20
	while ( (answer_loc == 0) && (i < 5) ) {
		i++; 
		//if (CONF_radio_modulation == 10) {
		//	answer_loc = SI4463_configure_from_10(G_SI4463);
		//}
		if (CONF_radio_modulation == 20) {
			answer_loc = SI4463_configure_from_20(G_SI4463);
		}
		if (CONF_radio_modulation == 11) {
			answer_loc = SI4463_configure_from_11(G_SI4463);
		}
		if (CONF_radio_modulation == 21) {
			answer_loc = SI4463_configure_from_21(G_SI4463);
		}
		if (CONF_radio_modulation == 12) {
			answer_loc = SI4463_configure_from_12(G_SI4463);
		}
		
		if (CONF_radio_modulation == 22) {
			answer_loc = SI4463_configure_from_22(G_SI4463);
		}
		if (CONF_radio_modulation == 13) {
			answer_loc = SI4463_configure_from_13(G_SI4463);
		}
		if (CONF_radio_modulation == 23) {
			answer_loc = SI4463_configure_from_23(G_SI4463);
		}
		if (CONF_radio_modulation == 14) {
			answer_loc = SI4463_configure_from_14(G_SI4463);
		}
		if (CONF_radio_modulation == 24) {
			answer_loc = SI4463_configure_from_24(G_SI4463);
		}
		RADIO_compute_freq_params();
	}
	return answer_loc;
}

void SI4463_radio_start(void) {
	if (CONF_radio_state_ON_OFF == 0) {
		//SI4463_change_state(G_SI4463, 0x03);//change state to ready
		wait_ms(1);
		G_SI4463->RX_TX_state = 0;
		SI4463_clear_IT(G_SI4463, 0, 0);
		//wait_ms(10);
		SI4463_CTS_read_answer (G_SI4463, SI_trash, 0, 600);
		CONF_radio_state_ON_OFF = 1;
		if ( (is_TDMA_master) && (CONF_master_FDD < 2) ) {
			SI4463_prepa_TX_1();
		} else {
			SI4463_TX_to_RX_transition();
		}
		//TDMA_init_all();
	}
}

void RADIO_on(int need_disconnect, int need_radio_reconfigure, int HMI_output) {
	last_rframe_seen = GLOBAL_timer.read_us();
	wait_ms(50);//10
	if (need_radio_reconfigure == 1) {
		if (HMI_output) { HMI_printf("reconfiguring radio...\r\n"); }
		SI4463_configure_all();
		if (HMI_output) { HMI_printf("reconfiguring done; starting radio\r\n"); }
	}
	if (need_disconnect == 1) {
		if (is_TDMA_master) {
			my_client_radio_connexion_state = 2;
		} else {
			my_client_radio_connexion_state = 1;
			my_radio_client_ID = 0x7E;
		}
	}
	TDMA_NULL_frame_init(70);
	if (need_disconnect == 1) { 
		TDMA_init_all();
	}
	radio_flush_TX_FIFO();// A supprimer!
	SI4463_radio_start();
	//need_radio_reconfigure = 0;
}

void RADIO_off(int need_disconnect) {
	int toto;
	CONF_radio_state_ON_OFF = 0;
	if ( (!is_TDMA_master) && (need_disconnect == 1) ) {
		my_client_radio_connexion_state = 1;
		my_radio_client_ID = 0x7E;
	}
	wait_ms(100);//400
	SI4463_FIFO_status(G_SI4463, &toto, &toto, 1);//tentative 
	G_SI4463->RX_TX_state = 0;//tentative 
	G_SI4463->RX_LED->write(0); 
	wait_us(10);
	SI4463_change_state(G_SI4463, 0x03);//change state to ready
	wait_us(10);
	SI4463_FIFO_status(G_SI4463, &toto, &toto, 1);//tentative 
	wait_us(10);
	SI4463_clear_IT (G_SI4463, 0, 0);//tentative 
	wait_ms(10);
	RX_FIFO_WR_point = RX_FIFO_last_received; //rewind FIFO pointer
	G_PTT_PA_pin->write(0);
	TX_in_progress = 0;
}

static int RADIO_previous_state;

void RADIO_off_if_necessary(int need_disconnect) {
	if (CONF_radio_state_ON_OFF == 1) {
		RADIO_previous_state = 1;
		RADIO_off(need_disconnect);
	} else {
		RADIO_previous_state = 0;
	}
}

void RADIO_restart_if_necessary(int need_disconnect, int need_radio_reconfigure, int HMI_output) {
	if (RADIO_previous_state == 1) {
		RADIO_on(need_disconnect, need_radio_reconfigure, HMI_output);
	}
}

void SI4432_TX_test(unsigned int req_duration) { //duration in ms
	int toto;
	unsigned int timer_begin;
	unsigned int timer_snapshot;
	unsigned int real_duration;
	unsigned char trash[4];
	
	TX_test_inprogress = 1;
	
	TX_frame_to_send = TX_TDMA_intern_data;
	req_duration = req_duration * 1000; //converts ms to microsec
	
	SI4463_FIFO_status(G_SI4463, &toto, &toto, 1); //reset FIFO (including CTS)
	SI4463_clear_IT (G_SI4463, 0, 0);//without CTS
	G_SI4463->RX_TX_state = 2; // activate TX HW IRQ
	wait_us(20); 
	SI4463_CTS_read_answer(G_SI4463, trash, 0, 20);
	
	G_SI4463->RX_TX_state = 2;
	TDMA_NULL_frame_init(230);
	SI4463_TX_new_frame(0);
	timer_begin = GLOBAL_timer.read_us();
	do {
		wait_ms(2);
		timer_snapshot = GLOBAL_timer.read_us();
		real_duration = (timer_snapshot - timer_begin);
	} while (real_duration < (req_duration) ) ;
	TX_test_inprogress = 0;
	wait_ms(200);
	RADIO_off(1);
}

void SI4463_set_frequency(float freq_base, float freq_step) {
	unsigned char radio_config[15] = {0x11, 0x40, 0x06, 0x00};
	unsigned int step_size_temp, FC_int, FC_frac_int;
	float FC_int_float, FC_frac_float;
	
	//FC_int_float = freq_base / 7.5;
	FC_int_float = freq_base * SI4463_NOUTDIV / 60;
	FC_int = (unsigned int)FC_int_float - 1;
	FC_frac_float = (FC_int_float - (float)FC_int) * 524288;
	FC_frac_int = (unsigned int)(FC_frac_float);
	
	//printf ("\r\nfreq_base %f \r\nfreq step %f\r\n", freq_base, freq_step);
	//printf ("\r\nFC_int_float %f\r\nFC_int %i\r\nFC_frac_float %f\r\nFC_frac_int%i\r\n", FC_int_float, FC_int, FC_frac_float, FC_frac_int);
	step_size_temp = (unsigned int)(524288*freq_step/7.5);
	
	radio_config [4] = FC_int & 0xFF; 					//FREQ_CONTROL_INTE
	radio_config [5] = (FC_frac_int & 0xFF0000) >> 16;	//FREQ_CONTROL_FRAC MSB
	radio_config [6] = (FC_frac_int & 0x00FF00) >> 8; 	// ...
	radio_config [7] = (FC_frac_int & 0x0000FF);		//FREQ_CONTROL_FRAC LSB
	radio_config [8] = (step_size_temp & 0xFF00) >> 8;	//FREQ_CONTROL_STEP_SIZE MSB
	radio_config [9] = step_size_temp & 0x00FF;			//FREQ_CONTROL_STEP_SIZE LSB
	//for (i=0; i<10; i++) {
	//	printf("i%i : %02X\r\n", i, radio_config[i]);
	//}
	SI4463_send_command(G_SI4463, radio_config, 10);
	wait_ms(100);// 100us
	SI4463_CTS_read_answer(G_SI4463, SI_trash, 0, 200);
	wait_ms(100);// 100us
}

void RADIO_compute_freq_params() {
	float freq_local, freq_shift_loc;
	float loc_freq_float_RX;
	float loc_freq_float_TX;
	freq_local = FREQ_RANGE_MIN + ((float)CONF_frequency_HD)/1000;//unit MHz
	freq_shift_loc = ((float)CONF_freq_shift)/1000;//unit MHz
	if (is_TDMA_master) {
		loc_freq_float_RX = freq_local + freq_shift_loc;
		loc_freq_float_TX = freq_local;//downlink
		if(CONF_master_FDD == 1) {//artificially disables RX for Master down
			loc_freq_float_RX = freq_local;
		}
	} else {
		loc_freq_float_RX = freq_local;//downlink
		loc_freq_float_TX = freq_local + freq_shift_loc;//uplink
	}
	unsigned int FC_int, FC_frac_int, VCO_CNT_int;
	float FC_int_float, FC_frac_float, VCO_CNT_float;
	
	//TX frequency
	//FC_int_float = loc_freq_float_TX / 7.5;
	FC_int_float = loc_freq_float_TX * SI4463_NOUTDIV / 60;
	FC_int = (unsigned int)FC_int_float - 1;
	FC_frac_float = (FC_int_float - (float)FC_int) * 524288;
	FC_frac_int = (unsigned int)(FC_frac_float);
	//VCO_CNT_float = loc_freq_float_TX * 256/60 + 0.5;
	VCO_CNT_float = (loc_freq_float_TX*32/60) * SI4463_NOUTDIV;
	VCO_CNT_int = (unsigned int)(VCO_CNT_float);
	CONF_SI4463_freq_conf_TX[0] = 0x37;
	CONF_SI4463_freq_conf_TX[1] = FC_int & 0xFF; 			//FREQ_CONTROL_INTE
	CONF_SI4463_freq_conf_TX[2] = (FC_frac_int & 0xFF0000) >> 16;//FREQ_CONTROL_FRAC MSB
	CONF_SI4463_freq_conf_TX[3] = (FC_frac_int & 0x00FF00) >> 8; 	// ...
	CONF_SI4463_freq_conf_TX[4] = (FC_frac_int & 0x0000FF);	//FREQ_CONTROL_FRAC LSB
	CONF_SI4463_freq_conf_TX[5] = (VCO_CNT_int & 0xFF00) >> 8;// VCO CNT MSB
	CONF_SI4463_freq_conf_TX[6] = (VCO_CNT_int & 0x00FF); 		// VCO CNT LSB
	CONF_SI4463_freq_conf_TX[7] = 0x00;// PLL settle time MSB
	CONF_SI4463_freq_conf_TX[8] = 0x64;// PLL settle time LSB (us)
	
	//RX frequency
	//FC_int_float = loc_freq_float_RX / 7.5;
	FC_int_float = loc_freq_float_RX * SI4463_NOUTDIV / 60;
	FC_int = (unsigned int)FC_int_float - 1;
	FC_frac_float = (FC_int_float - (float)FC_int) * 524288;
	FC_frac_int = (unsigned int)(FC_frac_float);
	//VCO_CNT_float = loc_freq_float_RX * 256/60 -2 + 0.5;
	VCO_CNT_float = (loc_freq_float_RX*32/60 - 32/128) * SI4463_NOUTDIV;
	VCO_CNT_int = (unsigned int)(VCO_CNT_float);
	CONF_SI4463_freq_conf_RX[0] = 0x36;
	CONF_SI4463_freq_conf_RX[1] = FC_int & 0xFF; 			//FREQ_CONTROL_INTE
	CONF_SI4463_freq_conf_RX[2] = (FC_frac_int & 0xFF0000) >> 16;//FREQ_CONTROL_FRAC MSB
	CONF_SI4463_freq_conf_RX[3] = (FC_frac_int & 0x00FF00) >> 8; 	// ...
	CONF_SI4463_freq_conf_RX[4] = (FC_frac_int & 0x0000FF);	//FREQ_CONTROL_FRAC LSB
	CONF_SI4463_freq_conf_RX[5] = (VCO_CNT_int & 0xFF00) >> 8;// VCO CNT MSB
	CONF_SI4463_freq_conf_RX[6] = (VCO_CNT_int & 0x00FF); 		// VCO CNT LSB
	
	CONF_channel_TX = 0;
	CONF_channel_RX = 0;
	
	SI4463_set_frequency(loc_freq_float_TX, 10);
}