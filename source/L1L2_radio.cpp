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

#include "L1L2_radio.h"
#include "mbed.h"
#include "global_variables.h"
#include "W5500.h"
#include "SI4463.h"
#include "Eth_IPv4.h"
#include "TDMA.h"
#include "signaling.h"

#include "ext_SRAM2.h"

//Timeout SI4463_prepa_TX_1_call;

static unsigned char trash[130];

static unsigned char data_RX[360];//260
static unsigned char rframe_TX[384];

void FDDup_RX_FIFO_dequeue(void) {
	int rframe_lgth, i;
	unsigned char data_temp;
	unsigned long int timer_snapshot;
	i = 0;
	if (RX_FIFO_last_received > RX_FIFO_RD_point) {
		for (i=0; i<=3; i++) {//4 bytes for timer and RSSI
			data_RX[i] = RX_FIFO_data[RX_FIFO_RD_point & RX_FIFO_mask];
			RX_FIFO_RD_point++;
		}
		data_temp = RX_FIFO_data[RX_FIFO_RD_point & RX_FIFO_mask];
		RX_FIFO_RD_point++;
		rframe_lgth = data_temp + SI4463_offset_size;
		data_RX[4] = data_temp;
		
		for (i=0; i<rframe_lgth; i++) {
			data_RX[i+5] = RX_FIFO_data[RX_FIFO_RD_point & RX_FIFO_mask];
			RX_FIFO_RD_point++;
		}
		W5500_write_TX_buffer(W5500_p1, 4, data_RX, rframe_lgth+5, 0);
		timer_snapshot = GLOBAL_timer.read_us();
		last_rframe_seen = timer_snapshot;
		//debug_counter++;
	}
}

void FDDdown_RX_pckt_treat(unsigned char* in_data, int size) {
	//puts raw RX radio packet from Ethernet to RX buffer
	memcpy (RX_FIFO_data, in_data, size+4);
	RX_FIFO_RD_point = 0;
	RX_FIFO_last_received = size;
	radio_RX_FIFO_dequeue(W5500_p1);
	//debug_counter++;
}

void radio_RX_FIFO_dequeue (W5500_chip* W5500) {
	static unsigned char ethernet_buffer[radio_addr_table_size][1600]; 
	static int size_received[radio_addr_table_size]; 
	static unsigned char prev_seg_counter[radio_addr_table_size];  
	static unsigned char curr_pkt_counter[radio_addr_table_size];  
	
	unsigned char seg_counter;
	unsigned char pkt_counter;
	unsigned char is_last_seg;
	unsigned char client_addr;
	unsigned char LID;
	unsigned char protocol;
	unsigned char protocol_byte;
	unsigned char client_ID_byte;
	unsigned char segmenter_byte;
	//static unsigned char data_RX[260];
	int TA_local = 100000;
	int RSSI;
	int rframe_lgth;
	int size_w_FEC;
	int size_wo_FEC;
	int segment_size;
	int client_ID_filter;
	unsigned int loc_micro_BER;
	unsigned int frame_timer;
	static int rx_count = 0;
	unsigned char TDMA_byte; 
	unsigned char is_downlink;
	unsigned long int timer_snapshot;
	
	if (RX_FIFO_last_received > RX_FIFO_RD_point) { //something new in RX FIFO
		timer_snapshot = GLOBAL_timer.read_us();
		//printf ("something in RX FIFO\r\n");
		frame_timer = RX_FIFO_data[RX_FIFO_RD_point & RX_FIFO_mask];
		RX_FIFO_RD_point++;
		frame_timer = (0x100 * RX_FIFO_data[RX_FIFO_RD_point & RX_FIFO_mask]) + frame_timer;
		RX_FIFO_RD_point++;
		frame_timer = (0x10000 * RX_FIFO_data[RX_FIFO_RD_point & RX_FIFO_mask]) + frame_timer;
		RX_FIFO_RD_point++;
		RSSI = RX_FIFO_data[RX_FIFO_RD_point & RX_FIFO_mask];
		RX_FIFO_RD_point++;
		rframe_lgth = RX_FIFO_data[RX_FIFO_RD_point & RX_FIFO_mask] + SI4463_offset_size;
		RX_FIFO_RD_point++;
		if (rframe_lgth > SI4463_CONF_max_field2_size) {
			rframe_lgth = SI4463_CONF_max_field2_size;
		}
		TDMA_byte = RX_FIFO_data[RX_FIFO_RD_point & RX_FIFO_mask];
		is_downlink = TDMA_byte & 0x40; 
		RX_FIFO_RD_point++;
		client_ID_byte = RX_FIFO_data[ (RX_FIFO_RD_point ) & RX_FIFO_mask]; // +1
		protocol_byte = RX_FIFO_data[ (RX_FIFO_RD_point + 1) & RX_FIFO_mask]; // +2
		
		size_w_FEC = rframe_lgth - 1; 
		
		if (is_TDMA_master) {
			TA_local = TDMA_TA_measure_single_frame(frame_timer, TDMA_byte, client_ID_byte, size_w_FEC);
			if ( (timer_snapshot-last_rframe_seen) > (CONF_radio_timeout+1000000) ) {
				last_rframe_seen = timer_snapshot;
				wait_ms(20);//lets time for client to switch to RX
				SI4463_prepa_TX_1();
			}
		}
		last_rframe_seen = timer_snapshot;

		// WITH FEC
		size_wo_FEC = FEC_decode(data_RX, size_w_FEC, &loc_micro_BER);
		//if (loc_micro_BER>0) {printf("err micro%i\r\n", loc_micro_BER);}
		radio_save_RSSI_BER (client_ID_byte, is_downlink, RSSI, loc_micro_BER); 
		//printf("RSSI:%i BER:%i\r\n", RSSI, loc_micro_BER);
		protocol = 0;
		if ( size_wo_FEC > 0) { 
			client_addr = data_RX[0] & 0x7F; //inside FEC, non need to use parity bit
			if (is_TDMA_master) {
				LID = client_addr; //client_addr;
			} else {
				LID = 0;//only one RX buffer for clients
			}
			protocol = data_RX[1];
			// client ID filter 
			client_ID_filter = 0;
			if (is_TDMA_master) {
				if ( (is_downlink == 0) && (client_addr < radio_addr_table_size) ) {
					if (CONF_radio_addr_table_status[client_addr]) {
						client_ID_filter = 1;
					}
				}
				if ( (is_downlink == 0) && (client_addr == 0x7E) ) {
					client_ID_filter = 1;
				}
			}
			else if ( ( (client_addr == my_radio_client_ID) || (client_addr == 0x7F) ) && (is_downlink) ) {
				client_ID_filter = 1;
			}
			if (client_ID_filter) {
				/* 
				if ( (protocol == 0x01) && (LID < radio_addr_table_size) ) { //RAW ETHERNET
					segment_size = size_wo_FEC - 3;
					segmenter_byte = data_RX[2];
					pkt_counter = (segmenter_byte & 0xF0) / 0x10; 
					is_last_seg = segmenter_byte & 0x08;
					seg_counter = segmenter_byte & 0x07;
					//printf("RX seg:%02X\r\n", segmenter_byte);
					
					if (seg_counter == 0) {//beginning of a new pkt
						curr_pkt_counter[LID] = pkt_counter;
						memcpy (ethernet_buffer[LID], data_RX+3, segment_size);
						size_received[LID] = segment_size;
						//printf("1st seg\r\n");
					} 
					else if ( (seg_counter == (prev_seg_counter[LID] + 1)) && (pkt_counter == curr_pkt_counter[LID]) ) {
						//folowing segment
						memcpy ( (ethernet_buffer[LID]+size_received[LID]), data_RX+3, segment_size);
						size_received[LID] = size_received[LID] + segment_size;
						//printf("follow seg\r\n");
					} 
					else { //continuity error
						size_received[LID] = 0;
					}
					prev_seg_counter[LID] = seg_counter;
					
					if ( (is_last_seg == 0x08) && (size_received[LID] > 0) ) {
						// Raw Ethernet... Not implemented yet
					}
				} */
				if ( (protocol == 0x02) && (LID < radio_addr_table_size) ) { //IPv4 Access 0x02
					//printf("IPv4 fr received\r\n");
					segment_size = size_wo_FEC - 3;
					segmenter_byte = data_RX[2];
					pkt_counter = (segmenter_byte & 0xF0) / 0x10; 
					is_last_seg = segmenter_byte & 0x08;
					seg_counter = segmenter_byte & 0x07;
					//printf("RX seg:%02X\r\n", segmenter_byte);
					
					if (seg_counter == 0) {//beginning of a new pkt
						curr_pkt_counter[LID] = pkt_counter;
						memcpy (ethernet_buffer[LID] + 14, data_RX+3, segment_size);
						size_received[LID] = segment_size;
						//printf("1st seg\r\n");
					} 
					else if ( (seg_counter == (prev_seg_counter[LID] + 1)) && (pkt_counter == curr_pkt_counter[LID]) ) {
						//folowing segment
						memcpy ( (ethernet_buffer[LID] + size_received[LID] +14), data_RX+3, segment_size);
						size_received[LID] = size_received[LID] + segment_size;
						//printf("follow seg\r\n");
					} 
					else { //continuity error
						size_received[LID] = 0;
					}
					prev_seg_counter[LID] = seg_counter;
					
					if ( (is_last_seg == 0x08) && (size_received[LID] > 0) ) {
						IPv4_from_radio (ethernet_buffer[LID], size_received[LID] + 14);
						G_downlink_bandwidth_temp = G_downlink_bandwidth_temp + size_received[LID];
						RX_radio_IPv4_counter++;
						//printf("pkt full\r\n");
					}
				}
				else if (protocol == 0x1E) {
					//printf("sig fr received\r\n");
					signaling_frame_exploitation (data_RX, size_wo_FEC, TA_local);
				}
				else if ( (protocol == 0x1F) && (is_TDMA_master == 0) ) {
					TDMA_slave_alloc_exploitation (data_RX, size_wo_FEC);
				} 
				else if (protocol == 0x00) {
					
				}
				else {
					if (super_debug) {
						printf("ERR Protocol\r\n");
					}
				}
			}
		} else {
			if (super_debug) {
				printf("FEC error\r\n");
			}
		}
		TDMA_byte_RX_interp (TDMA_byte, client_ID_byte, protocol_byte, frame_timer);
		rx_count++;
	}
}

void radio_signalisation_frame_building(void) {
	
}

int FEC_encode2 (unsigned char* data_in, unsigned char* data_out, int size_in) { 
	int size_out;
	int size_single_bloc;
	int size_single_bloc_pl1;
	unsigned char CRC_1; 
	unsigned char CRC_2; 
	unsigned char CRC_3; 
	unsigned char CRC_4; 
	unsigned char data_field_1;
	unsigned char data_field_2;
	unsigned char data_field_3;
	unsigned char data_field_4;
	int i; 
	CRC_1 = 0;
	CRC_2 = 0;
	CRC_3 = 0;
	CRC_4 = 0;
	
	size_single_bloc = size_in/3;
	if (size_in % 3) {
		size_single_bloc++;
	}
	size_single_bloc_pl1 = size_single_bloc + 1;
	size_out = 4 * size_single_bloc_pl1; 
	for (i=0; i<size_single_bloc; i++) {
		data_field_1 = data_in[i];
		data_field_2 = data_in[size_single_bloc + i];
		data_field_3 = data_in[(2*size_single_bloc) + i];
		data_field_4 = data_field_1 ^ data_field_2 ^ data_field_3; 
		CRC_1 = CRC_1 ^ data_field_1; 
		CRC_2 = CRC_2 ^ data_field_2; 
		CRC_3 = CRC_3 ^ data_field_3; 
		CRC_4 = CRC_4 ^ data_field_4; 
		data_out[i] = data_field_1;
		data_out[size_single_bloc_pl1 + i] = data_field_2;
		data_out[(2*size_single_bloc_pl1) + i] = data_field_3;
		data_out[(3*size_single_bloc_pl1) + i] = data_field_4;
	}
	data_out[size_single_bloc] = CRC_1;
	data_out[(2*size_single_bloc) + 1] = CRC_2;
	data_out[(3*size_single_bloc) + 2] = CRC_3;
	data_out[(4*size_single_bloc) + 3] = CRC_4;
	return size_out;
}

 int FEC_decode(unsigned char* data_out, int size_in, unsigned int* micro_BER) {
	int size_out; 
	int size_single_bloc;
	int size_single_bloc_pl1;
	unsigned char CRC_check_1;
	unsigned char CRC_check_2;
	unsigned char CRC_check_3;
	unsigned char CRC_check_4;
	unsigned char data_temp; 
	unsigned int nb_errors;
	int i; 
	size_single_bloc_pl1 = size_in / 4;
	size_single_bloc = size_single_bloc_pl1 - 1;
	
	CRC_check_1 = 0;
	CRC_check_2 = 0;
	CRC_check_3 = 0;
	CRC_check_4 = 0;
	for (i=0; i<size_single_bloc_pl1; i++) {
		CRC_check_1 = CRC_check_1 ^ RX_FIFO_data [(RX_FIFO_RD_point + i) & RX_FIFO_mask];
		CRC_check_2 = CRC_check_2 ^ RX_FIFO_data [(RX_FIFO_RD_point + size_single_bloc_pl1 + i) & RX_FIFO_mask];
		CRC_check_3 = CRC_check_3 ^ RX_FIFO_data [(RX_FIFO_RD_point + (2*size_single_bloc_pl1) + i) & RX_FIFO_mask];
		CRC_check_4 = CRC_check_4 ^ RX_FIFO_data [(RX_FIFO_RD_point + (3*size_single_bloc_pl1) + i) & RX_FIFO_mask];
	}
	
	nb_errors = 0;
	if (CRC_check_1 != 0) { nb_errors++; }// printf("error 1  %X ;", CRC_check_1);}
	if (CRC_check_2 != 0) { nb_errors++; }// printf("error 2  %X ;", CRC_check_2);}
	if (CRC_check_3 != 0) { nb_errors++; }// printf("error 3  %X ;", CRC_check_3);}
	if (CRC_check_4 != 0) { nb_errors++; }// printf("error 4  %X ;", CRC_check_4);}
	//printf("\r\n");
	//if (nb_errors>0) {printf("ERR%i\r\n", nb_errors);}
	
	(*micro_BER) = nb_errors;
	
	if (nb_errors>1) {
		size_out = 0; //unrecoverable error
		//printf("unrecoverable\r\n");
	} else {
		// OK
		size_out = 3 * size_single_bloc; 
		// FIELD 1
		if (CRC_check_1==0) {//field 1 OK
			for (i=0; i<size_single_bloc; i++) {
				data_out[i] = RX_FIFO_data [(RX_FIFO_RD_point + i) & RX_FIFO_mask];
			}
		} else {// field 1 corrupted, reconstruction from field 2, 3 and 4
			for (i=0; i<size_single_bloc; i++) {
				data_temp = RX_FIFO_data [(RX_FIFO_RD_point + size_single_bloc_pl1 + i) & RX_FIFO_mask]; 
				data_temp = data_temp ^ RX_FIFO_data [(RX_FIFO_RD_point + (2*size_single_bloc_pl1) + i) & RX_FIFO_mask];
				data_temp = data_temp ^ RX_FIFO_data [(RX_FIFO_RD_point + (3*size_single_bloc_pl1) + i) & RX_FIFO_mask];
				data_out[i] = data_temp; 
			}
		}
		// FIELD 2
		if (CRC_check_2==0) {//field 2 OK
			for (i=0; i<size_single_bloc; i++) {
				data_out[size_single_bloc + i] = RX_FIFO_data [(RX_FIFO_RD_point + size_single_bloc_pl1 + i) & RX_FIFO_mask];
			}
		} else {// field 2 corrupted, reconstruction from field 1, 3 and 4
			for (i=0; i<size_single_bloc; i++) {
				data_temp = RX_FIFO_data [(RX_FIFO_RD_point + i) & RX_FIFO_mask]; //field 1
				data_temp = data_temp ^ RX_FIFO_data [(RX_FIFO_RD_point + (2*size_single_bloc_pl1) + i) & RX_FIFO_mask]; //field 3
				data_temp = data_temp ^ RX_FIFO_data [(RX_FIFO_RD_point + (3*size_single_bloc_pl1) + i) & RX_FIFO_mask]; //field 4
				data_out[size_single_bloc + i] = data_temp; 
			}
		}
		// FIELD 3
		if (CRC_check_3==0) {//field 3 OK
			for (i=0; i<size_single_bloc; i++) {
				data_out[(2*size_single_bloc) + i] = RX_FIFO_data[(RX_FIFO_RD_point + (2*size_single_bloc_pl1) + i) & RX_FIFO_mask];
			}
		} else {// field 3 corrupted, reconstruction from field 1, 2 and 4
			for (i=0; i<size_single_bloc; i++) {
				data_temp = RX_FIFO_data [(RX_FIFO_RD_point + i) & RX_FIFO_mask]; //field 1
				data_temp = data_temp ^ RX_FIFO_data [(RX_FIFO_RD_point + size_single_bloc_pl1 + i) & RX_FIFO_mask]; //field 2
				data_temp = data_temp ^ RX_FIFO_data [(RX_FIFO_RD_point + (3*size_single_bloc_pl1) + i) & RX_FIFO_mask]; //field 4
				data_out[(2*size_single_bloc) + i] = data_temp; 
			}
		}
	}
	RX_FIFO_RD_point = RX_FIFO_RD_point + size_in; 
	return size_out; 
}

int size_w_FEC_compute (int size_wo_FEC) {
	int size_w_FEC; 
	size_w_FEC = size_wo_FEC/3;
	if ((size_wo_FEC%3) > 0) { size_w_FEC++; }
	size_w_FEC = 4*size_w_FEC + 4;
	return size_w_FEC; 
}

void segment_and_push (unsigned char* data_unsegmented, int total_size, unsigned char client_addr, unsigned char protocol) {
	static unsigned char packet_counter = 0;
	unsigned char segment_counter = 0;
	unsigned char segmenter_byte;
	unsigned char is_last_segment;
	int size_wo_FEC;
	int size_w_FEC;
	int rframe_length;
	int size_remaining;
	int segment_size; 
	int size_to_send;
	unsigned char data_wo_FEC[300];
#ifdef EXT_SRAM_USAGE
	unsigned char radio_pckt[360];
#endif
	int size_sent;
	unsigned int rsize_needed;
	unsigned int timer_snapshot;
	size_remaining = total_size;

	if (size_remaining <63) {
		size_remaining = 63;
	}
	
	segment_counter = 0;
	size_sent = 0;
	rsize_needed = 100 + (total_size * 1.4);
	if (total_size < 1510) {
		if (TX_FIFO_full_global(1) == 0) {
			G_uplink_bandwidth_temp = G_uplink_bandwidth_temp + total_size; 
			TX_radio_IPv4_counter++;
			while (size_remaining > 0) {
				if (size_remaining <= 252) {
					segment_size = size_remaining; 
					is_last_segment = 0x08;
				} else {
					segment_size = 252;
					is_last_segment = 0x00;
				}
				if (segment_size <63) {
					size_to_send = 63;
				} else {
					size_to_send = segment_size;
				}
				timer_snapshot = GLOBAL_timer.read_us();
				rframe_TX[0] = (timer_snapshot >> 16) & 0xFF;
			
				size_wo_FEC = size_to_send + 3; //+ 3 inside FEC header
				size_w_FEC = size_w_FEC_compute (size_wo_FEC);
				rframe_length = size_w_FEC + 1 - SI4463_offset_size;
				if (rframe_length < 0) {rframe_length = 0;}
				rframe_TX[1] = rframe_length;
				rframe_TX[2] = 0x01; //TDMA byte
				
				segmenter_byte = (packet_counter & 0x0F) * 0x10 + is_last_segment + (segment_counter & 0x07);
				data_wo_FEC[0] = client_addr + parity_bit_elab[(client_addr&0x7F)]; //client address
				data_wo_FEC[1] = protocol; //protocol : raw ethernet
				data_wo_FEC[2] = segmenter_byte; // segmenter byte
				memcpy(data_wo_FEC+3, (data_unsegmented + size_sent), size_to_send); 
				size_sent = size_sent + size_to_send;
				size_w_FEC = FEC_encode2(data_wo_FEC, rframe_TX+3, size_wo_FEC); 
				TX_FIFO_write_global(rframe_TX, size_w_FEC+3);//3 for timer & length & TDMA_byte
				size_remaining = size_remaining - segment_size; 
				segment_counter++;
				
			}
			packet_counter++;
		} else {
			//if (super_debug) {
			//	printf ("FULL last_ready:%08X RD_point:%08X\r\n", TXPS_FIFO->last_ready, TXPS_FIFO->RD_point);
			//}
		}
	}
}

//unsigned char TX_buff_intern_FIFOdata[42][384];
//unsigned int TX_buff_intern_WR_pointer=0;
//unsigned int TX_buff_intern_RD_pointer=0;
//unsigned int TX_buff_intern_last_ready=0;

void TX_FIFO_write_global(unsigned char* data, int size) {
	if (is_SRAM_ext == 1) {
		TX_ext_FIFO_write(data, size);
	} else {
		TX_intern_FIFO_write(data, size);
	}
}

void TX_intern_FIFO_write(unsigned char* data, int size) {
	if (size <= 128) {
		memcpy (TX_buff_intern_FIFOdata[TX_buff_intern_WR_pointer % 128], data, 128);//size
		TX_buff_intern_WR_pointer++;
	}
	else if (size <= 256) {
		memcpy (TX_buff_intern_FIFOdata[TX_buff_intern_WR_pointer % 128], data, 128);
		TX_buff_intern_WR_pointer++;
		memcpy (TX_buff_intern_FIFOdata[TX_buff_intern_WR_pointer % 128], data+128, 128);//size-128
		TX_buff_intern_WR_pointer++;
	}
	else {
		memcpy (TX_buff_intern_FIFOdata[TX_buff_intern_WR_pointer % 128], data, 128);
		TX_buff_intern_WR_pointer++;
		memcpy (TX_buff_intern_FIFOdata[TX_buff_intern_WR_pointer % 128], data+128, 128);
		TX_buff_intern_WR_pointer++;
		memcpy (TX_buff_intern_FIFOdata[TX_buff_intern_WR_pointer % 128], data+256, 128);//size-256
		TX_buff_intern_WR_pointer++;
	}
	TX_buff_intern_last_ready = TX_buff_intern_WR_pointer;
}

void ext_SRAM_write2(ext_SRAM_chip* loc_SPI, unsigned char* loc_data, unsigned int address, int size) {
	//static unsigned char trash[350];
	static unsigned char command[6] = {0x02, 0x00, 0x00, 0x00};
	loc_SPI->cs->write(0);
	command[3] = address & 0xFF;
	command[2] = (address & 0xFF00) >> 8;
	command[1] = (address & 0xFF0000) >> 16;
	loc_SPI->spi_port->transfer_2 (command, 4, trash, 4);
	loc_SPI->spi_port->transfer_2 (loc_data, size, trash, size);
	loc_SPI->cs->write(1);
}

void TX_ext_FIFO_write(unsigned char* data, int size) {
	unsigned int loc_address;
	//printf("size_wr_ext %i\n\r", size);
	if (size <= 128) {
		loc_address = (TX_buff_ext_WR_pointer & 1023)*128;
		ext_SRAM_write2(SPI_SRAM_p, data, loc_address, size);
		TX_buff_ext_sizes[TX_buff_ext_WR_pointer & 1023] = data[1];
		TX_buff_ext_WR_pointer++;
	}
	else if (size <= 256) {
		loc_address = (TX_buff_ext_WR_pointer & 1023)*128;
		ext_SRAM_write2(SPI_SRAM_p, data, loc_address, 128);
		TX_buff_ext_sizes[TX_buff_ext_WR_pointer & 1023] = data[1];
		TX_buff_ext_WR_pointer++;
		loc_address = (TX_buff_ext_WR_pointer & 1023)*128;
		ext_SRAM_write2(SPI_SRAM_p, data+128, loc_address, size-128);
		TX_buff_ext_sizes[TX_buff_ext_WR_pointer & 1023] = 0;
		TX_buff_ext_WR_pointer++;
	}
	else {
		loc_address = (TX_buff_ext_WR_pointer & 1023)*128;
		ext_SRAM_write2(SPI_SRAM_p, data, loc_address, 128);
		TX_buff_ext_sizes[TX_buff_ext_WR_pointer & 1023] = data[1];
		TX_buff_ext_WR_pointer++;
		loc_address = (TX_buff_ext_WR_pointer & 1023)*128;
		ext_SRAM_write2(SPI_SRAM_p, data+128, loc_address, 128);
		TX_buff_ext_sizes[TX_buff_ext_WR_pointer & 1023] = 0;
		TX_buff_ext_WR_pointer++;
		loc_address = (TX_buff_ext_WR_pointer & 1023)*128;
		ext_SRAM_write2(SPI_SRAM_p, data+256, loc_address, size-256);
		TX_buff_ext_sizes[TX_buff_ext_WR_pointer & 1023] = 0;
		TX_buff_ext_WR_pointer++;
	}
	TX_buff_ext_last_ready = TX_buff_ext_WR_pointer;
}

int TX_intern_FIFO_get_lastfrzize(void) {
	int size_loc;
	size_loc = TX_buff_intern_FIFOdata[TX_buff_intern_RD_pointer % 128][1];
	size_loc = size_loc + 1 + SI4463_offset_size;
	return size_loc;
}

int TX_intern_FIFO_read(unsigned char* data) {
	int size_loc;
	size_loc = TX_buff_intern_FIFOdata[TX_buff_intern_RD_pointer % 128][1];//42
	size_loc = size_loc + 1 + SI4463_offset_size;
	size_loc = size_loc + 1;//for timer coarse
	//printf("sizeread: %i\r\n", size_loc);
	if (size_loc <= 128) {
		memcpy(data, TX_buff_intern_FIFOdata[TX_buff_intern_RD_pointer % 128], 128);//size_loc
		TX_buff_intern_RD_pointer++;
	} 
	else if (size_loc<=256) {
		memcpy(data, TX_buff_intern_FIFOdata[TX_buff_intern_RD_pointer % 128], 128);
		TX_buff_intern_RD_pointer++;
		memcpy(data+128, TX_buff_intern_FIFOdata[TX_buff_intern_RD_pointer % 128], 128);//size_loc-128
		TX_buff_intern_RD_pointer++;
	}
	else {
		memcpy(data, TX_buff_intern_FIFOdata[TX_buff_intern_RD_pointer % 128], 128); 
		TX_buff_intern_RD_pointer++;
		memcpy(data+128, TX_buff_intern_FIFOdata[TX_buff_intern_RD_pointer % 128], 128);
		TX_buff_intern_RD_pointer++;
		memcpy(data+256, TX_buff_intern_FIFOdata[TX_buff_intern_RD_pointer % 128], 128);//size_loc-256
		TX_buff_intern_RD_pointer++;
	}
	return size_loc;
}

unsigned int compute_TX_buff_size_global(void) {//for TDMA
	unsigned int size_loc;
	if (is_SRAM_ext == 1) {
		size_loc = (TX_buff_intern_last_ready - TX_buff_intern_RD_pointer);
		size_loc = size_loc + (TX_buff_ext_last_ready - TX_buff_ext_RD_pointer);
		size_loc = size_loc/3;
	} else {
		size_loc = (TX_buff_intern_last_ready - TX_buff_intern_RD_pointer)/3;
	}
	return size_loc;
}

int TX_FIFO_full_global (int priority) {
	if (is_SRAM_ext == 1) {
		return TX_FIFO_full_withSRAM(priority);
	} else {
		return TX_FIFO_full_woSRAM(priority);
	}
}

int TX_FIFO_full_woSRAM (int priority) {
	//priority : 1= normal, 0=high priority (signaling)
	int FIFO_filling;
	int loc_threshold;
	FIFO_filling = TX_buff_intern_WR_pointer - TX_buff_intern_RD_pointer;
	if (priority == 1) {
		loc_threshold = 110;//low priority
	} else {
		loc_threshold = 120;//high priority
	}
	if (FIFO_filling > loc_threshold) {//40
		//printf("FIFO_full!\r\n");
		return 1;
	} else {
		return 0;
	}
}

int TX_FIFO_full_withSRAM (int priority) {
	int FIFO_filling;
	int loc_threshold;
	FIFO_filling = TX_buff_ext_WR_pointer - TX_buff_ext_RD_pointer;
	if (priority == 1) {
		loc_threshold = 950;//low priority
	} else {
		loc_threshold = 1000;//high priority
	}
	if (FIFO_filling > loc_threshold) {//40
		//printf("FIFO_fullext!\r\n");
		return 1;
	} else {
		return 0;
	}
}

void ext_SRAM_read2(ext_SRAM_chip* loc_SPI, unsigned char* loc_data, unsigned int address, int size) {
	static unsigned char command[6] = {0x03, 0x00, 0x00, 0x00};
	loc_SPI->cs->write(0);
	command[3] = address & 0xFF;
	command[2] = (address & 0xFF00) >> 8;
	command[1] = (address & 0xFF0000) >> 16;
	loc_SPI->spi_port->transfer_2 (command, 4, trash, 4);
	loc_SPI->spi_port->transfer_2 (trash, size, loc_data, size);
	loc_SPI->cs->write(1);
}

void ext_SRAM_periodic_call(void) {
	int intern_FIFO_filling;
	int ext_FIFO_filling;
	int i, loc_size;
	int loc_bloc_nb=1;
	unsigned int loc_address;
	unsigned int loc_time_int;
	unsigned char RX_frame_datation;
	unsigned char loc_time_char;
	loc_time_int = GLOBAL_timer.read_us();
	loc_time_char = (loc_time_int >> 16) & 0xFF; 
	intern_FIFO_filling = TX_buff_intern_WR_pointer - TX_buff_intern_RD_pointer;
	ext_FIFO_filling = TX_buff_ext_WR_pointer - TX_buff_ext_RD_pointer;
	while ( (ext_FIFO_filling > 0) && (intern_FIFO_filling <= 9) ) {
		for (i=1; i<=loc_bloc_nb; i++) {
			loc_address = (TX_buff_ext_RD_pointer & 1023) *128;
			ext_SRAM_read2(SPI_SRAM_p, TX_buff_intern_FIFOdata[TX_buff_intern_WR_pointer % 128], loc_address, 128);
			
			if (i==1) {
				RX_frame_datation = TX_buff_intern_FIFOdata[TX_buff_intern_WR_pointer % 128][0];
				loc_size = TX_buff_intern_FIFOdata[TX_buff_intern_WR_pointer % 128][1];
				loc_size = loc_size + SI4463_offset_size + 2;
				//printf("size:%i\r\n", loc_size);
				if (loc_size <= 128) {
					loc_bloc_nb = 1;
				} else if (loc_size <= 256) {
					loc_bloc_nb = 2;
				} else {
					loc_bloc_nb = 3;
				}
			}
			
			//if (TX_buff_ext_sizes[TX_buff_ext_RD_pointer & 1023] > 0) {
			//	TX_buff_intern_FIFOdata[TX_buff_intern_WR_pointer % 128][1] = TX_buff_ext_sizes[TX_buff_ext_RD_pointer & 1023];
			//}
			TX_buff_ext_RD_pointer++;
			//printf("delay %i\r\n", (int)loc_time_char - (int)RX_frame_datation);
			if ( (loc_time_char - RX_frame_datation) < CONF_Tx_rframe_timeout) {
			//if (1) {
				//purge old frames if condition false
				TX_buff_intern_WR_pointer++;
			} 
			//else {
			//	printf ("purged!\r\n");//!!!
			//	printf("date1 %i date2 %i\r\n", loc_time_char, RX_frame_datation); 
			//}
		}
		TX_buff_intern_last_ready = TX_buff_intern_WR_pointer;	
		intern_FIFO_filling = TX_buff_intern_WR_pointer - TX_buff_intern_RD_pointer;
		ext_FIFO_filling = TX_buff_ext_WR_pointer - TX_buff_ext_RD_pointer;
	}
}


//void radio_flush_TX_FIFO(void) {
//	TXPS_FIFO->WR_point = 0;
//	TXPS_FIFO->RD_point = 0;
//	TXPS_FIFO->last_ready = 0;
//}

int ext_SRAM_detect(void) {
	unsigned char data_1[4] = {0x3C, 0x4A, 0xF3, 0x12};
	unsigned char data_2[4] = {0xFF, 0xFF, 0xFF, 0xFF};
	int i; 
	int sram_detected = 1;
	ext_SRAM_write2(SPI_SRAM_p, data_1, 2345, 4);
	ext_SRAM_read2(SPI_SRAM_p, data_2, 2345, 4);
	for (i=0; i<4; i++) {
		if (data_2[i] != data_1[i]) {sram_detected = 0;}
	}
	return sram_detected;
}

void radio_flush_TX_FIFO(void) {
	TX_buff_intern_WR_pointer=0;
	TX_buff_intern_RD_pointer=0;
	TX_buff_intern_last_ready=0;
	TX_buff_ext_WR_pointer=0;
	TX_buff_ext_RD_pointer=0;
	TX_buff_ext_last_ready=0;
}

void radio_save_RSSI_BER (unsigned char client_byte, unsigned char is_downlink, unsigned char RSSI_loc, unsigned int micro_BER) {
	unsigned char client_ID = 0xF0;
	client_ID = client_byte & 0x7F;
	
	if ( (is_TDMA_master) && (is_downlink == 0) && (parity_bit_check[client_byte]) && (client_ID < radio_addr_table_size) ) {
		G_radio_addr_table_RSSI[client_ID] = (26 * RSSI_loc) + (0.9 * G_radio_addr_table_RSSI[client_ID]);
		G_radio_addr_table_BER[client_ID] = (1250 * micro_BER) + (0.9 * G_radio_addr_table_BER[client_ID]);
		
	}
	if ( (is_TDMA_master == 0) && (is_downlink) ) {
		G_downlink_RSSI = (26 * RSSI_loc) + (0.9 * G_downlink_RSSI);
		G_downlink_BER = (1250 * micro_BER) + (0.9 * G_downlink_BER);
	}
	//if (micro_BER>0) {printf("err save%i %i\r\n", micro_BER, G_downlink_BER);}
}
