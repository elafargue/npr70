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

#ifndef EXT_SRAM_F4
#define EXT_SRAM_F4

#include "mbed.h"

struct ext_SRAM_chip{
    SPI* spi_port;
    DigitalOut* cs;
};

void ext_SRAM_init(void);

void ext_SRAM_set_mode(ext_SRAM_chip* loc_SPI);

//void ext_SRAM_read2(ext_SRAM_chip* loc_SPI, unsigned char* loc_data, unsigned int address, int size);

void ext_SRAM_write(ext_SRAM_chip* loc_SPI, unsigned char* loc_data, unsigned int address, int size);
	
int extSRAM_testfreespace(int pkt_nb, unsigned char FIFO_index);

void extSRAM_push(unsigned char* raw_data, int size, unsigned char FIFO_nb);

//void ext_SRAM_periodic_call();

void ext_SRAM_test(ext_SRAM_chip* loc_SPI);

#endif