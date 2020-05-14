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

#ifndef L1L2_RADIO_F4
#define L1L2_RADIO_F4

#include "mbed.h"
#include "W5500.h"
#include "global_variables.h"
#include "ext_SRAM2.h"

void FDDdown_RX_pckt_treat(unsigned char* in_data, int size);

void FDDup_RX_FIFO_dequeue(void);

void radio_RX_FIFO_dequeue (W5500_chip* W5500);

void radio_signalisation_frame_building(void);

//int FEC_encode (unsigned char* data_in, TX_buffer_struct* FIFO_out, int size_in); 
//#ifdef EXT_SRAM_USAGE
int FEC_encode2 (unsigned char* data_in, unsigned char* data_out, int size_in);
//#endif
int FEC_decode(unsigned char* data_out, int size_in, unsigned int* micro_BER); 

int size_w_FEC_compute (int size_wo_FEC); 

void segment_and_push (unsigned char* data_unsegmented, int total_size, unsigned char client_addr, unsigned char protocol); 

void TX_FIFO_write_global(unsigned char* data, int size);
void TX_intern_FIFO_write(unsigned char* data, int size);
void TX_ext_FIFO_write(unsigned char* data, int size);

int TX_intern_FIFO_get_lastfrzize(void);

int TX_intern_FIFO_read(unsigned char* data);

unsigned int compute_TX_buff_size_global(void);

int TX_FIFO_full_global (int priority);
int TX_FIFO_full_woSRAM (int priority);
int TX_FIFO_full_withSRAM (int priority);

void ext_SRAM_periodic_call(void);

int ext_SRAM_detect(void);

void radio_flush_TX_FIFO(void);

void radio_save_RSSI_BER (unsigned char client_byte, unsigned char is_downlink, unsigned char RSSI_loc, unsigned int micro_BER);

#endif