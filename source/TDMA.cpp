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

#include "TDMA.h"
#include "mbed.h"
#include "L1L2_radio.h"
#include "global_variables.h"

static unsigned char TDMA_table_uplink_st[radio_addr_table_size];
static int TDMA_table_uplink_usage[radio_addr_table_size];
static int TDMA_table_is_fast[radio_addr_table_size];
//static long int TDMA_table_TA[radio_addr_table_size];
static unsigned int TDMA_table_RX_time[radio_addr_table_size]; 
static unsigned char TDMA_table_up2date[radio_addr_table_size];
static unsigned char TDMA_table_slots[radio_addr_table_size];
static unsigned int TDMA_table_offset[radio_addr_table_size];
static unsigned char TDMA_frame_nb; 
static unsigned char master_allocated_slots;
static unsigned int slave_alloc_RX_age = 2;
static unsigned char my_multiframe_mask;
static unsigned char my_multiframe_ID;
//static unsigned int TDMA_slave_last_master_top = 0;
static unsigned int TDMA_offset_multi_frame;

void TDMA_init_all(void) {
	int i;
	for (i=0; i<radio_addr_table_size; i++) {
		TDMA_table_uplink_st[i] = 0;
		TDMA_table_uplink_usage [i] = 32;
		TDMA_table_is_fast[i] = 1;
	}
}

unsigned char TDMA_byte_elaboration(unsigned char synchro) {
	unsigned char TDMA_byte;
	unsigned long int uplink_buffer_size;
	// unsigned long int uplink_buffer_size_temp;
	if (is_TDMA_master) { // TDMA Master
		TDMA_byte = 0x40;//uplink/downlink bit
		TDMA_byte = TDMA_byte + (TDMA_frame_nb & 0x1F);
		if (synchro) {
			TDMA_frame_nb++;
		}
	} else { // TDMA client
		TDMA_byte = 0x00; //uplink/downlink bit
		// uplink buffer size
		//uplink_buffer_size_temp = (TXPS_FIFO->last_ready - TXPS_FIFO->RD_point); 
		//uplink_buffer_size = uplink_buffer_size_temp / 300; //number of frames
		uplink_buffer_size = compute_TX_buff_size_global();
		//if ( (uplink_buffer_size_temp % 300) > 0) {uplink_buffer_size++;}
		if (uplink_buffer_size > 30) {uplink_buffer_size = 30;}
		TDMA_byte = TDMA_byte + (uplink_buffer_size & 0x1F);
	}
	if (synchro) {
		TDMA_byte = TDMA_byte + 0x20;
	}
	TDMA_byte = TDMA_byte + parity_bit_elab[TDMA_byte & 0x7F]; // parity bit
	return TDMA_byte;
}

short int TDMA_TA_measure_single_frame(unsigned int frame_timer, unsigned char TDMA_byte, unsigned char client_byte, int frame_size_loc) {
	int measured_offset = 0x7FFF;
	int TA_answer = 0x7FFF;
	unsigned char client_ID = 0xF0;
	unsigned char is_downlink;
	
	is_downlink = TDMA_byte & 0x40; 
	
	if ( ((TDMA_byte & 0x20) == 0x20) && (is_downlink == 0) && (parity_bit_check[TDMA_byte]) && (parity_bit_check[client_byte]) ) { //first frame top-synchro
		client_ID = client_byte & 0x7F;
		if (CONF_master_FDD == 1) {
			measured_offset = frame_timer;
		} else {
			measured_offset = frame_timer - ((TDMA_slave_last_master_top & 0xFFFFFF));// + 10*TDMA_table_offset[client_ID]);
		}
		if (frame_size_loc < 114) {
			measured_offset = measured_offset + (114 - frame_size_loc) * 0.85; 
		}
		if ( (client_ID < radio_addr_table_size) && (CONF_radio_addr_table_status[client_ID]) ) {
			TA_answer = measured_offset - (10*TDMA_table_offset[client_ID]);
			if ( (TA_answer > -200) && (TA_answer < 2000) ) {// -1000 .. 5000
				TDMA_table_TA[client_ID] = 0.9*TDMA_table_TA[client_ID] + 1*TA_answer;
			}
			//printf ("id:%i TA_single:%i TA_filt:%i\r\n", client_ID, TA_answer, TDMA_table_TA[client_ID]);
		}
		else if (client_ID == 0x7E) {
			TA_answer = measured_offset - (10*TDMA_offset_multi_frame);
			//printf ("id:%i TAth:%i\r\n", client_ID, TA_answer);
		}
	}
	return (short int)TA_answer;
}

void TDMA_init_TA(unsigned char client_ID, int TA_input) {
	if ( (TA_input > -200) && (TA_input < 2000) ) {
		TDMA_table_TA[client_ID] = 10*TA_input;
	}
}

void TDMA_top_measure(void) {
	TDMA_slave_last_master_top = GLOBAL_timer.read_us();
	TDMA_slave_last_master_top = TDMA_slave_last_master_top + 0;
}

void TDMA_FDD_up_top_measure(void) {
	TDMA_slave_last_master_top = GLOBAL_timer.read_us();
	TDMA_slave_last_master_top = TDMA_slave_last_master_top + 0;
	RX_top_FDD_up_counter++;
	//printf("t\r\n");
}

void TDMA_byte_RX_interp (unsigned char TDMA_byte, unsigned char client_ID_byte, unsigned char protocol, unsigned int RX_time) {
	// a traiter : top client / top master / frame client / 
	unsigned char client_ID;
	unsigned char uplink_buffer_size;
	unsigned char TDMA_synchro;
	unsigned char is_downlink;
	unsigned int loc_time;
	TDMA_synchro = TDMA_byte & 0x20;
	is_downlink = TDMA_byte & 0x40;
	client_ID = client_ID_byte & 0x7F;
	if (parity_bit_check[TDMA_byte]) { // checks parity bit
	//if(1) {
		if (is_TDMA_master) { // TDMA Master
			if (is_downlink == 0) { //only uplink frames
				uplink_buffer_size = TDMA_byte & 0x1F;
				if ( (client_ID < radio_addr_table_size) && (parity_bit_check[client_ID_byte]) ) {
					TDMA_table_uplink_st[client_ID] = uplink_buffer_size;
					TDMA_table_up2date[client_ID] = 1;
					if (uplink_buffer_size > 1) { // force to fast slots
						TDMA_table_uplink_usage[client_ID] = 32;
						TDMA_table_is_fast[client_ID] = 1;
					}
					//printf("%i upl%i mem%i fast%i\r\n", client_ID, uplink_buffer_size, TDMA_table_uplink_usage[client_ID],TDMA_table_is_fast[client_ID]);
					if (TDMA_synchro) {
						TDMA_table_RX_time[client_ID] = RX_time;
					}
				}
			}
		} else { // TDMA client
			TDMA_frame_nb = TDMA_byte & 0x1F;
			if ( (TDMA_synchro) && (is_downlink) ) { //(slave_alloc_RX_age < 2) ) {
				TDMA_slave_last_master_top = RX_time; 
				//debug_counter ++; 
				if ( (slave_alloc_RX_age < 2) && (CONF_radio_state_ON_OFF) ) {
					if ( (TDMA_frame_nb & my_multiframe_mask) == (my_multiframe_ID & my_multiframe_mask) ) {
						loc_time = GLOBAL_timer.read_us();
						time_next_TX_slave = ((RX_time + offset_time_TX_slave) & 0xFFFFFF );//!!!test TA june 2018 +380
						SI4463_prepa_TX_1_call.attach_us(&SI4463_prepa_TX_1, (time_next_TX_slave - loc_time - CONF_delay_prepTX1_2_TX) &0xFFFFFF  );
						slave_alloc_RX_age++;
						slave_new_burst_tx_pending = 1;
					}
				}
			}
		}
	}
}

//TDMA_slave_timeout : allow 1 more burst if no TOP received from Master
void TDMA_slave_timeout (void) {
	unsigned int loc_time;
	unsigned int master_top_age;
	loc_time = GLOBAL_timer.read_us();
	master_top_age = loc_time - TDMA_slave_last_master_top;
	if ( (master_top_age > (CONF_TDMA_frame_duration + 8000) ) && (master_top_age < (CONF_TDMA_frame_duration + 10000)) ) {
		if ( (slave_alloc_RX_age < 2) && (slave_new_burst_tx_pending == 0) && (CONF_radio_state_ON_OFF) ) {
			TDMA_frame_nb = (TDMA_frame_nb+1) & 0x1F;
			if ( (TDMA_frame_nb & my_multiframe_mask) == (my_multiframe_ID & my_multiframe_mask) ) {
				time_next_TX_slave = (TDMA_slave_last_master_top + CONF_TDMA_frame_duration + offset_time_TX_slave) & 0xFFFFFF;
				SI4463_prepa_TX_1_call.attach_us(&SI4463_prepa_TX_1, (time_next_TX_slave - loc_time - CONF_delay_prepTX1_2_TX) &0xFFFFFF  );
				slave_alloc_RX_age++;
				slave_new_burst_tx_pending = 1;
			}
		}
	}
}

void TDMA_master_allocation () {
	int size_wo_FEC;
	int size_w_FEC;
	int i;
	int allocated_slots;
	int nb_fast_clients;
	unsigned int loc_time_offset;
	long int local_TA; 
	unsigned int downlink_buffer_size;
	// unsigned int downlink_buffer_size_temp;
	static unsigned char TDMA_alloc_frame_raw[150];
	unsigned char rframe_length;
	unsigned char loc_client_needs[radio_addr_table_size];
	unsigned char loc_master_needs;
	unsigned char remaining_needs;
	
	for (i=0; i<radio_addr_table_size; i++) {
		if ( (CONF_radio_addr_table_status[i]) && (TDMA_table_uplink_usage[i] > 0) ) {
			TDMA_table_is_fast[i] = 1;
			TDMA_table_uplink_usage[i]--; 
		}
	}
	//if ((TDMA_frame_nb & 0x7) == 0) { // once every 8 frames 
	if ((TDMA_frame_nb & 0x3) == 0) { // once every 4 frames 
		TDMA_master_allocation_slow();
	}
	// ** TDMA allocation algorithm for fast slots **	
	// 1) Master computes its own downlink buffer size
	//downlink_buffer_size_temp = (TXPS_FIFO->last_ready - TXPS_FIFO->RD_point); 
	//downlink_buffer_size = downlink_buffer_size_temp / 300; //number of frames
	//if ((downlink_buffer_size_temp % 300) > 0) {downlink_buffer_size++;}
	downlink_buffer_size = compute_TX_buff_size_global();
	if (downlink_buffer_size > 30) {downlink_buffer_size = 30;}
	// 2) if no TDMA uplink received from client, lower its need
	for (i=0; i<radio_addr_table_size; i++) {
		if ( (CONF_radio_addr_table_status[i]) && (TDMA_table_up2date[i]==0) && (TDMA_table_uplink_st[i] > 0) ) {
			TDMA_table_uplink_st[i]--; 
		}
		TDMA_table_up2date[i] = 0;
	}
	// 3) init allocation table
	// copy uplink state and allocate 1 slot to each active
	// Master
	loc_master_needs = downlink_buffer_size; // copy
	master_allocated_slots = 1;
	if (loc_master_needs>0) {loc_master_needs--;} // decrement
	allocated_slots = 1; // at least 1 for master
	// Clients
	nb_fast_clients = 0;
	for (i=0; i<radio_addr_table_size; i++) {
		if ( (CONF_radio_addr_table_status[i]) && (TDMA_table_is_fast[i]) ) {
			nb_fast_clients++;
			loc_client_needs[i] = TDMA_table_uplink_st[i]; // copy
			TDMA_table_slots[i] = 1; // allocate 1 slot
			if (loc_client_needs[i]>0) {loc_client_needs[i]--;} // decrement
			allocated_slots++; 
		}
	}
	// 4) 1st allocation pass, round robin
	remaining_needs = 1;
	while ( (allocated_slots < 15) && (remaining_needs > 0) ) {
		// master
		if ( (loc_master_needs > 0) && (allocated_slots < 15) ) {
			master_allocated_slots++;
			loc_master_needs--;
			allocated_slots++;
		}
		if ( (loc_master_needs > 0) && (allocated_slots < 15) && (nb_fast_clients > 1) ) {// master counts 2 times if more than 1 client
			master_allocated_slots++;
			loc_master_needs--;
			allocated_slots++;
		}
		remaining_needs = loc_master_needs;
		for (i=0; i<radio_addr_table_size; i++) {
			if ( (CONF_radio_addr_table_status[i]) && (TDMA_table_is_fast[i]) && (loc_client_needs[i]>0) && (allocated_slots < 15) ) {
				TDMA_table_slots[i]++;
				loc_client_needs[i]--;
				allocated_slots++;
				remaining_needs = remaining_needs + loc_client_needs[i];
			}
		}
	}
	// 5) 2nd allocation pass, round robin of remaining, even without needs
	while (allocated_slots < 15) {
		master_allocated_slots++;
		allocated_slots++;
		for (i=0; i<radio_addr_table_size; i++) {
			if ( (CONF_radio_addr_table_status[i]) && (TDMA_table_is_fast[i]) && (allocated_slots < 15) ) {
				TDMA_table_slots[i]++;
				allocated_slots++;
			}
		}
	}
	// 6) timings construction
	time_max_TX_burst = (CONF_reduced_TDMA_slot_duration + CONF_TDMA_slot_margin) + (master_allocated_slots * CONF_TDMA_slot_duration) + ((master_allocated_slots-1)*CONF_TDMA_slot_margin);// for Master
	loc_time_offset = time_max_TX_burst + CONF_TDMA_slot_margin + CONF_TR_margain + CONF_TA_margain;
	//printf("m %i", time_max_TX_burst);
	for (i=0; i<radio_addr_table_size; i++) {
		if ( (CONF_radio_addr_table_status[i]) && (TDMA_table_is_fast[i]) ) {
			local_TA = TDMA_table_TA[i];
			if ( (local_TA > -2000) && (local_TA < 20000) ) {
				TDMA_table_offset[i] = (loc_time_offset/10) - (local_TA/100); 
			} else {
				TDMA_table_offset[i] = (loc_time_offset/10); 
			}
			loc_time_offset = loc_time_offset + (TDMA_table_slots[i] * (CONF_TDMA_slot_duration + CONF_TDMA_slot_margin) );
			//printf("client %i %i %i ", i, TDMA_table_offset[i], TDMA_table_slots[i]*CONF_TDMA_slot_duration);
		}
	}
	//printf("\r\n");
	//multi frame x4
	TDMA_offset_multi_frame = loc_time_offset / 10;
	for (i=0; i<4; i++) {
		if ( (CONF_radio_addr_table_status[i]) && (TDMA_table_is_fast[i]==0) ) {
			local_TA = TDMA_table_TA[i];
			if ( (local_TA > -2000) && (local_TA < 20000) ) {
				TDMA_table_offset[i] = TDMA_offset_multi_frame - (local_TA/100); 
			} else {
				TDMA_table_offset[i] = TDMA_offset_multi_frame; 
			}
		}
	}
	loc_time_offset = loc_time_offset + CONF_TDMA_slot_duration + CONF_TDMA_slot_margin;
	TDMA_offset_multi_frame = loc_time_offset / 10;
	for (i=4; i<radio_addr_table_size; i++) {
		if ( (CONF_radio_addr_table_status[i]) && (TDMA_table_is_fast[i]==0) ) {
			local_TA = TDMA_table_TA[i];
			if ( (local_TA > -2000) && (local_TA < 20000) ) {
				TDMA_table_offset[i] = TDMA_offset_multi_frame - (local_TA/100); 
			} else {
				TDMA_table_offset[i] = TDMA_offset_multi_frame; 
			}
		}
	}
	
	// ** TDMA allocation frame construction **
	TDMA_alloc_frame_raw[0] = 0xFF; // address = broadcast
	TDMA_alloc_frame_raw[1] = 0x1F; // protocol = TDMA allocation
	size_wo_FEC = 2;
	for (i=0; i<radio_addr_table_size; i++) {
		if (CONF_radio_addr_table_status[i]) { // client "i" is active
			if (TDMA_table_is_fast[i]==1) { // inside fast slot
			//if (1) {
				TDMA_alloc_frame_raw[size_wo_FEC] = i; // client ID
				size_wo_FEC++;
				TDMA_alloc_frame_raw[size_wo_FEC] = TDMA_table_offset[i] & 0xFF; // time offset LSB
				size_wo_FEC++;
				TDMA_alloc_frame_raw[size_wo_FEC] = (TDMA_table_offset[i] & 0xFF00)>>8; // time offset MSB
				size_wo_FEC++;
				TDMA_alloc_frame_raw[size_wo_FEC] = (TDMA_table_slots[i] & 0xF); // TDMA slot length (4xLSb) and power (MSb)
				size_wo_FEC++;
				TDMA_alloc_frame_raw[size_wo_FEC] = 0; //  ID multi frame (4xLSb)
				size_wo_FEC++;
			} else { // inside slow slot
				TDMA_alloc_frame_raw[size_wo_FEC] = i; // client ID
				size_wo_FEC++;
				TDMA_alloc_frame_raw[size_wo_FEC] = TDMA_table_offset[i] & 0xFF; // time offset LSB
				size_wo_FEC++;
				TDMA_alloc_frame_raw[size_wo_FEC] = (TDMA_table_offset[i] & 0xFF00)>>8; // time offset MSB
				size_wo_FEC++;
				TDMA_alloc_frame_raw[size_wo_FEC] = 1; // TDMA slot length (4xLSb) and power (MSb)
				size_wo_FEC++;
				//TDMA_alloc_frame_raw[size_wo_FEC] = 0x30 + (i & 0x0F); //multi frame period (4xMSb) ID multi frame (4xLSb)
				TDMA_alloc_frame_raw[size_wo_FEC] = 0x20 + (i & 0x03); //multi frame period (4xMSb) ID multi frame (4xLSb)
				size_wo_FEC++;
			}
		}
	}
	//Discovery slot multi frame
	TDMA_alloc_frame_raw[size_wo_FEC] = 0x7E; // client ID
	size_wo_FEC++;
	TDMA_alloc_frame_raw[size_wo_FEC] = TDMA_offset_multi_frame & 0xFF; // time offset LSB
	size_wo_FEC++;
	TDMA_alloc_frame_raw[size_wo_FEC] = (TDMA_offset_multi_frame & 0xFF00)>>8; // time offset MSB
	size_wo_FEC++;
	TDMA_alloc_frame_raw[size_wo_FEC] = 1; // TDMA slot length (4xLSb) and power (MSb)
	size_wo_FEC++;
	//TDMA_alloc_frame_raw[size_wo_FEC] = 0x37; //multi frame period=3 (4xMSb); ID multi frame=7 (4xLSb)
	TDMA_alloc_frame_raw[size_wo_FEC] = 0x23; //multi frame period=2 (4xMSb); ID multi frame=7 (4xLSb)
	size_wo_FEC++;
	//END
	TDMA_alloc_frame_raw[size_wo_FEC] = 0xFF; // used to detect end of TDMA frame
	size_wo_FEC++;
	if (size_wo_FEC < 66) {
		size_wo_FEC = 66;
	}
	size_w_FEC = size_w_FEC_compute (size_wo_FEC);
	rframe_length = size_w_FEC + 1 - SI4463_offset_size;

	TX_TDMA_intern_data[0] = 0;//timer coarse, useless
	TX_TDMA_intern_data[1] = rframe_length;
	
	size_w_FEC = FEC_encode2(TDMA_alloc_frame_raw, TX_TDMA_intern_data+3, size_wo_FEC);
}

void TDMA_master_allocation_slow () {
	int i;
	for (i=0; i<radio_addr_table_size; i++) {
		if ( (CONF_radio_addr_table_status[i]) && (TDMA_table_uplink_usage[i] == 0) ) {
			TDMA_table_is_fast[i] = 0;
		}
		if (CONF_radio_addr_table_status[i] <= 0) {
			TDMA_table_uplink_st[i] = 0;
			TDMA_table_uplink_usage [i] = 32;
			TDMA_table_is_fast[i] = 1;
		}
	}	
}


void TDMA_NULL_frame_init(int size) {
	unsigned char null_frame[260];
	int size_wo_FEC, size_w_FEC; 
	unsigned char rframe_length;
	null_frame[0] = my_radio_client_ID + parity_bit_elab[my_radio_client_ID]; // address = client
	null_frame[1] = 0x00; // protocol = NULL frame
	//size_wo_FEC = 70;
	size_wo_FEC = size;
	size_w_FEC = size_w_FEC_compute (size_wo_FEC);
	rframe_length = size_w_FEC + 1 - SI4463_offset_size;
	//TX_signaling_TDMA->data[0] = 0; // timer, date for later use
	TX_TDMA_intern_data[0] = 0;
	//TX_signaling_TDMA->data[1] = rframe_length; // length
	//TX_signaling_TDMA->WR_point = 3; 
	TX_TDMA_intern_data[1] = rframe_length;
	//size_w_FEC = FEC_encode(null_frame, TX_signaling_TDMA, size_wo_FEC);
	size_w_FEC = FEC_encode2(null_frame, TX_TDMA_intern_data+3, size_wo_FEC);//+3: timer, size, tdma
	//TX_signaling_TDMA->last_ready = TX_signaling_TDMA->WR_point;
}



void TDMA_slave_alloc_exploitation(unsigned char* unFECdata, int unFECsize) {
	static unsigned char LUT_multif_mask[8] = {0,1,3,7,15,31};
	int i;
	unsigned char loc_client_ID;
	unsigned char loc_TDMA_slot_length;
	// unsigned char loc_power;
	unsigned long int loc_TDMA_offset;
	i=2; //1st byte: client ID, 2nd byte:protocol
	loc_client_ID = unFECdata[2];
	while ( (loc_client_ID != 0xFF) && (i < unFECsize) ) {
		if (loc_client_ID == my_radio_client_ID) {
			loc_TDMA_offset = ( unFECdata[i+1] + (unFECdata[i+2]<<8) )*10;
			offset_time_TX_slave = loc_TDMA_offset; 
			//printf("offset:%i\r\n",loc_TDMA_offset);
			loc_TDMA_slot_length = 0x0F & unFECdata[i+3];
			time_max_TX_burst = (loc_TDMA_slot_length * CONF_TDMA_slot_duration) + ( (loc_TDMA_slot_length-1) *CONF_TDMA_slot_margin );
			my_multiframe_ID = 0x0F & unFECdata[i+4];
			my_multiframe_mask = (0xF0 & unFECdata[i+4]) >> 4;
			my_multiframe_mask = LUT_multif_mask[my_multiframe_mask];
			// loc_power = ( 0xF0 & unFECdata[i+3] ) >> 4;
			//printf("TDMAf:");
			//for (j=0; j< 5; j++) {
			//	printf(" %02X", unFECdata[i+j]);
			//}
			//printf ("\r\n");
			//printf ("offs:%i length:%i multi_ID:%i multi_period:%i\r\n", offset_time_TX_slave, time_max_TX_burst, my_multiframe_ID, my_multiframe_period);
			slave_alloc_RX_age = 0;
		}
		i=i+5;
		loc_client_ID = unFECdata[i];
	}
	
	
}

