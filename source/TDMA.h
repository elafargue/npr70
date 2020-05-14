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

#ifndef TDMA_F4
#define TDMA_F4

void TDMA_init_all(void);

unsigned char TDMA_byte_elaboration(unsigned char synchro);

short int TDMA_TA_measure_single_frame(unsigned int frame_timer, unsigned char TDMA_byte, unsigned char client_ID, int frame_size_loc);

void TDMA_init_TA(unsigned char client_ID, int TA_input);

void TDMA_top_measure(void);

void TDMA_FDD_up_top_measure(void);

void TDMA_byte_RX_interp (unsigned char TDMA_byte, unsigned char client_ID_byte, unsigned char protocol, unsigned int RX_time);

void TDMA_slave_timeout (void);

void TDMA_master_allocation ();

void TDMA_master_allocation_slow ();

void TDMA_NULL_frame_init(int size);

void TDMA_slave_alloc_exploitation(unsigned char* unFECdata, int unFECsize);

#endif