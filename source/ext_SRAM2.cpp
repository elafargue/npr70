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

#ifdef EXT_SRAM_USAGE

#include "mbed.h"
#include "global_variables.h"
#include "ext_SRAM2.h"

static unsigned char trash[350];
static unsigned short int extSRAM_FIFOs[8][94];// 8 FIFO of pointers to extSRAM
static unsigned char extSRAM_filling[374];// 0 : empty frame slot; 1: occupied frame slot
static int extSRAM_total_filling = 0;
static unsigned char extSRAM_pkt_timer[374];// timer of each frame
static unsigned short int extSRAM_pkt_size[374];// size of each frame
static unsigned char extSRAM_FIFO_index_read[8];//Read pointers of FIFO
static unsigned char extSRAM_FIFO_index_write[8];//write pointers of FIFO
static unsigned char extSRAM_FIFO_filling[8];

void ext_SRAM_init(void) {
	int i;
	int j;
	for (i=0; i<8; i++) {
		extSRAM_FIFO_index_read[i] = 0;
		extSRAM_FIFO_index_write[i] = 0;
		extSRAM_FIFO_filling[i] = 0;
	}
	extSRAM_total_filling = 0;
}

void ext_SRAM_set_mode(ext_SRAM_chip* loc_SPI) {
	static unsigned char command[6] = {0x01, 0x40, 0x00, 0x00};
	loc_SPI->cs->write(0);
	loc_SPI->spi_port->transfer_2 (command, 2, trash, 2);
	loc_SPI->cs->write(1);
}

/*
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
*/

void ext_SRAM_write(ext_SRAM_chip* loc_SPI, unsigned char* loc_data, unsigned int address, int size) {
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

int extSRAM_testfreespace(int pkt_nb, unsigned char FIFO_index) {
	int freespace_available = 1;
	if ( (extSRAM_FIFO_filling[FIFO_index] + pkt_nb) > 94 ) {
		freespace_available = 0;
	}
	if ( (extSRAM_total_filling + pkt_nb) > 374 ) {
		freespace_available = 0;
	}
	return freespace_available;
}

void extSRAM_push(unsigned char* raw_data, int size, unsigned char FIFO_nb) {
	unsigned short int i;
	unsigned short int free_slot = 0xFFFF;
	for (i=0; i<350; i++) {
		if (extSRAM_filling[i] == 0) {
			free_slot = i;
		}
	}
	if (free_slot != 0xFFFF) {
		extSRAM_filling[free_slot] = 1;
		extSRAM_FIFOs[FIFO_nb][extSRAM_FIFO_index_write[FIFO_nb]] = free_slot;
		extSRAM_FIFO_index_write[FIFO_nb] ++;
		if (extSRAM_FIFO_index_write[FIFO_nb] > 93) {
			extSRAM_FIFO_index_write[FIFO_nb] = 0;
		}
		extSRAM_total_filling ++;
		extSRAM_FIFO_filling[FIFO_nb]++;
		extSRAM_pkt_timer[free_slot] = raw_data[0];
		extSRAM_pkt_size[free_slot] = size;
		ext_SRAM_write (SPI_SRAM_p, raw_data, free_slot*350, size);
	} else {
		printf("ERROR extSRAM push\r\n");
	}
}

void TXPS_FIFO_fill(unsigned char* loc_data, int size) {
	int size_to_write;
	if ( ( (TXPS_FIFO->WR_point & TXPS_FIFO->mask) +  size) > TXPS_FIFO->mask) { // to big for 1 step
		size_to_write = (TXPS_FIFO->mask + 1 - (TXPS_FIFO->WR_point & TXPS_FIFO->mask) );
		//G_SI4463->spi->transfer_2 (TXPS_FIFO->data + (TXPS_FIFO->WR_point & TXPS_FIFO->mask), size_to_write, SI_trash, size_to_write);
		memcpy (TXPS_FIFO->data + (TXPS_FIFO->WR_point & TXPS_FIFO->mask), loc_data, size_to_write); 
		TXPS_FIFO->WR_point = TXPS_FIFO->WR_point + size_to_write; // should become ZERO
		//second step
		size_to_write = size - size_to_write; 
		//G_SI4463->spi->transfer_2 (TXPS_FIFO->data + (TXPS_FIFO->WR_point & TXPS_FIFO->mask), size_to_write, SI_trash, size_to_write);
		memcpy (TXPS_FIFO->data + (TXPS_FIFO->WR_point & TXPS_FIFO->mask), loc_data, size_to_write); 
		TXPS_FIFO->WR_point = TXPS_FIFO->WR_point + size_to_write;
		TXPS_FIFO->last_ready = TXPS_FIFO->WR_point;
	} else { //1 step is enough
		//G_SI4463->spi->transfer_2 (TXPS_FIFO->data + (TXPS_FIFO->WR_point & TXPS_FIFO->mask), size, SI_trash, size);
		memcpy (TXPS_FIFO->data + (TXPS_FIFO->WR_point & TXPS_FIFO->mask), loc_data, size); 
		TXPS_FIFO->WR_point = TXPS_FIFO->WR_point + size;
		TXPS_FIFO->last_ready = TXPS_FIFO->WR_point;
	}
}

/*
void ext_SRAM_periodic_call(void) {
	int intern_FIFO_filling;
	int ext_FIFO_filling;
	intern_FIFO_filling = TX_buff_intern_WR_pointer - TX_buff_intern_RD_pointer;
	ext_FIFO_filling = TX_buff_ext_WR_pointer - TX_buff_ext_RD_pointer;
	while ( (ext_FIFO_filling > 0) && (intern_FIFO_filling <= 9) ) {
		ext_SRAM_read(SPI_SRAM_p, TX_buff_intern_FIFOdata[TX_buff_intern_WR_pointer % 128], TX_buff_ext_RD_pointer*128, 128);
		TX_buff_ext_RD_pointer++;
		TX_buff_intern_WR_pointer++;
		TX_buff_intern_last_ready = TX_buff_intern_WR_pointer;
	}
}
*/

/*
void ext_SRAM_periodic_call() {
	static unsigned char current_buff;
	unsigned char radio_pckt[360];
	unsigned char nb_buffer_treated = 0;
	unsigned char frame_datation;
	unsigned char loc_time_char;
	unsigned int loc_time_int;
	unsigned short int extSRAM_pointer;
	int one_packet_ok = 0;
	int switch_to_next_buff = 0;
	int fifo_full;
	int frame_size;
	
	nb_buffer_treated = 0;
	loc_time_int = GLOBAL_timer.read_us();
	loc_time_char = (loc_time_int >> 16) & 0xFF; 
	//condition a rajouter sur buffer interne TXPS
	if ((TXPS_FIFO->last_ready - TXPS_FIFO->RD_point) < 3740 ) { //16380 - 350 ; 3740
		fifo_full = 0;
	} else {
		fifo_full = 1;
	}
	//printf("FIFO full : %i\r\n", fifo_full);
	while ( (nb_buffer_treated<8) && (one_packet_ok==0) && (fifo_full == 0) ) {//loop over all buffers
		switch_to_next_buff = 0;
		if (extSRAM_FIFO_filling[current_buff] > 0) {//data available in current buffer
			extSRAM_pointer = extSRAM_FIFOs[current_buff][extSRAM_FIFO_index_read[current_buff]];
			frame_datation = extSRAM_pkt_timer[extSRAM_pointer];
			//frame_size = extSRAM_pkt_size[extSRAM_pointer] + 2 + SI4463_offset_size;
			frame_size = extSRAM_pkt_size[extSRAM_pointer];
			if ( (loc_time_char - frame_datation) < CONF_Tx_rframe_timeout) {//frame ok
				//printf("frame OK ext SRAM\r\n");
				ext_SRAM_read (SPI_SRAM_p, radio_pckt, extSRAM_pointer*350, frame_size);
				radio_pckt[1] = frame_size - 2 - SI4463_offset_size;//-2
				TXPS_FIFO_fill (radio_pckt, frame_size);
				switch_to_next_buff = 1;
				one_packet_ok = 1;
			} else {//old frame
				switch_to_next_buff = 0;
			}
			extSRAM_filling[extSRAM_pointer] = 0;
			extSRAM_FIFO_index_read[current_buff]++;
			if (extSRAM_FIFO_index_read[current_buff] > 93) {
				extSRAM_FIFO_index_read[current_buff] = 0;
			}
			extSRAM_FIFO_filling[current_buff]--;
			extSRAM_total_filling--;
			
		}
		else {//no data in the current buffer
			switch_to_next_buff = 1;
		}
		if (switch_to_next_buff == 1) {
			current_buff++;
			if (current_buff>7) {current_buff = 0;}
			nb_buffer_treated++;
		}
	} 
}
*/


void ext_SRAM_test(ext_SRAM_chip* loc_SPI) {
	unsigned char test_table[400] = "bonjour!!";
	unsigned char test_table2[400];
	unsigned long int timer1;
	unsigned long int timer2;
	unsigned int loc_address;
	int i, j;
	printf("\r\nsram test begin\r\n");
	//loc_SPI->cs->write(1);
	for (i=0; i<5000; i++) {
		loc_address = 16 * i;
		//wait_ms(1);
		sprintf ((char*)test_table, "jourbon %i", i);
		ext_SRAM_write(loc_SPI, test_table, loc_address, 20);
		printf("write %i\r\n", i);
	}
	for (i=0; i<5000; i++) {
		loc_address = 16 * i;
		for (j=0; j<30; j++) {
			test_table2[j] = 0;
		}
		//wait_ms(1);
		ext_SRAM_read2(loc_SPI, test_table2, loc_address, 20);
		test_table2[12] = 0;
		printf ("read addr %i content '%s' \r\n", loc_address, test_table2);
	}
	timer1 = GLOBAL_timer.read_us();
	for (i=0; i<10; i++) {
		memcpy(test_table2+i, test_table+i, 350);
	}
	timer2 = GLOBAL_timer.read_us();
	printf("temps memcpy 350 %i\r\n", timer2-timer1);
}

#endif