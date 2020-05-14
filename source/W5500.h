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

#ifndef W5500_F4
#define W5500_F4

#include "mbed.h"

struct W5500_chip{
    SPI* spi_port;
    DigitalOut* cs;
	DigitalIn* interrupt;
	unsigned char sock_interrupt;
};

void W5500_read_long(W5500_chip* SPI_p_loc, unsigned int W5500_addr, unsigned char bloc_addr, unsigned char* RX_data, int RX_size);

void W5500_write_long(W5500_chip* SPI_p_loc, unsigned int W5500_addr, unsigned char bloc_addr, unsigned char* TX_data, int TX_size);

void W5500_read_short(W5500_chip* SPI_p_loc, unsigned int W5500_addr, unsigned char bloc_addr, unsigned char* RX_data_ext, int RX_size);

void W5500_read_char(W5500_chip* SPI_p_loc, unsigned int W5500_addr, unsigned char bloc_addr, char* RX_data_ext, int RX_size);

void W5500_write_short(W5500_chip* SPI_p_loc, unsigned int W5500_addr, unsigned char bloc_addr, unsigned char* TX_data_ext, int TX_size);

unsigned char W5500_read_byte(W5500_chip* SPI_p_loc, unsigned int W5500_addr, unsigned char bloc_addr);

void W5500_write_byte(W5500_chip* SPI_p_loc, unsigned int W5500_addr, unsigned char bloc_addr, unsigned char data);

void W5500_Phy_off_2sec(W5500_chip* SPI_p_loc);

int W5500_read_received_size(W5500_chip* SPI_p_loc, int sock_nb);

int W5500_read_TX_free_size(W5500_chip* SPI_p_loc, int sock_nb);

void W5500_read_RX_buffer(W5500_chip* SPI_p_loc, int sock_nb,  unsigned char* data, int size);

int W5500_read_UDP_pckt (W5500_chip* SPI_p_loc, int sock_nb, unsigned char* data);

int W5500_read_MAC_pckt (W5500_chip* SPI_p_loc, int sock_nb, unsigned char* data);

void W5500_write_TX_buffer(W5500_chip* SPI_p_loc, int sock_nb, unsigned char* data, int size, int send_mac);

void W5500_re_configure (void);

void W5500_re_configure_gateway(W5500_chip* SPI_p_loc);

void W5500_re_configure_periodic_call(W5500_chip* SPI_p_loc);

void W5500_initial_configure(W5500_chip* SPI_p_loc); 

#endif