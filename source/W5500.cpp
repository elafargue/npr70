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

#include "W5500.h"
#include "mbed.h"
#include "global_variables.h"
#include "Eth_IPv4.h"

void W5500_read_long(W5500_chip* SPI_p_loc, unsigned int W5500_addr, unsigned char bloc_addr, unsigned char* RX_data, int RX_size)
{
    unsigned char W5_command[20];
	unsigned char trash[20];
    W5_command[0] = W5500_addr / 256;
    W5_command[1] = W5500_addr & 0xFF;
    W5_command[2] = (bloc_addr * 0x08);
    *(SPI_p_loc->cs)=0;
	RX_data[0]=0;
    
	SPI_p_loc->spi_port->transfer_2 (W5_command, 3, trash, 3);
    
	SPI_p_loc->spi_port->transfer_2 (trash, RX_size, RX_data, RX_size);
    wait_us(1);
    *(SPI_p_loc->cs)=1;
	wait_us(2);
}

void W5500_write_long(W5500_chip* SPI_p_loc, unsigned int W5500_addr, unsigned char bloc_addr, unsigned char* TX_data, int TX_size) {
    unsigned char W5_command[4];
	static unsigned char trash[1600];
	
    W5_command[0] = W5500_addr / 256;
    W5_command[1] = W5500_addr & 0xFF;
    W5_command[2] = (bloc_addr * 0x08) + 4;
	SPI_p_loc->cs->write(0);
	SPI_p_loc->spi_port->transfer_2 (W5_command, 3, trash, 3);
	//wait_us(10);
	
	SPI_p_loc->spi_port->transfer_2 (TX_data, TX_size, trash, 0);
	
    wait_us(1);
	SPI_p_loc->cs->write(1);
	wait_us(2);
}

void W5500_read_short(W5500_chip* SPI_p_loc, unsigned int W5500_addr, unsigned char bloc_addr, unsigned char* RX_data_ext, int RX_size) {
    unsigned char TX_data_loc[20];
    unsigned char RX_data_loc[20];
    int i;
    TX_data_loc[0] = W5500_addr / 256;
    TX_data_loc[1] = W5500_addr & 0xFF;
    TX_data_loc[2] = (bloc_addr * 0x08) ;
	SPI_p_loc->cs->write(0);
	SPI_p_loc->spi_port->transfer_2 (TX_data_loc, RX_size+3, RX_data_loc, RX_size+3);
    for (i=0; i < RX_size; i++) {
        RX_data_ext[i] = RX_data_loc[i+3];
    }
    wait_us(1);
	SPI_p_loc->cs->write(1);
	wait_us(2);
}

void W5500_write_short(W5500_chip* SPI_p_loc, unsigned int W5500_addr, unsigned char bloc_addr, unsigned char* TX_data_ext, int TX_size) {
    unsigned char TX_data_loc[10];
	unsigned char trash[10];
    int i;
    TX_data_loc[0] = W5500_addr / 256;
    TX_data_loc[1] = W5500_addr & 0xFF;
    TX_data_loc[2] = (bloc_addr * 0x08) + 4;
    for (i=0; i < TX_size; i++) {
        TX_data_loc[i+3] = TX_data_ext[i];
    }
	SPI_p_loc->cs->write(0);
	SPI_p_loc->spi_port->transfer_2 (TX_data_loc, TX_size+3, trash, 0);
	wait_us(1);
	SPI_p_loc->cs->write(1);
	wait_us(2);
}

unsigned char W5500_read_byte(W5500_chip* SPI_p_loc, unsigned int W5500_addr, unsigned char bloc_addr) {
    unsigned char TX_data_loc[20];
    unsigned char RX_data_loc[20];
	unsigned char data_out;
    TX_data_loc[0] = W5500_addr / 256;
    TX_data_loc[1] = W5500_addr & 0xFF;
    TX_data_loc[2] = (bloc_addr * 0x08) ;
	SPI_p_loc->cs->write(0);
	SPI_p_loc->spi_port->transfer_2 (TX_data_loc, 4, RX_data_loc, 4); 
    wait_us(1);
	SPI_p_loc->cs->write(1);
    data_out = RX_data_loc[3]; 
	return data_out;
	wait_us(2);
}

void W5500_write_byte(W5500_chip* SPI_p_loc, unsigned int W5500_addr, unsigned char bloc_addr, unsigned char data) {
    unsigned char TX_data_loc[10];
	unsigned char trash[10];
    TX_data_loc[0] = W5500_addr / 256;
    TX_data_loc[1] = W5500_addr & 0xFF;
    TX_data_loc[2] = (bloc_addr * 0x08) + 4;
    TX_data_loc[3] = data;
	SPI_p_loc->cs->write(0);
	SPI_p_loc->spi_port->transfer_2 (TX_data_loc, 4, trash, 0);
	wait_us(1);
	SPI_p_loc->cs->write(1);
	wait_us(2);
}


void W5500_Phy_off_2sec(W5500_chip* SPI_p_loc) {
    //unsigned char phy_config[1];
    W5500_write_byte(SPI_p_loc, 0x002E, 0x00, 0x70);
    wait(5);
    W5500_write_byte(SPI_p_loc, 0x002E, 0x00, 0xF8);
}

int W5500_read_received_size(W5500_chip* SPI_p_loc, int sock_nb) {
	int size=9999, previous_size=9999;
	unsigned char data[10];
	do {
		previous_size = size;
		W5500_read_short(SPI_p_loc, 0x0026, (sock_nb*4)+1, data, 2);
		size = data[1] + data[0]*256;
	} while (previous_size != size);
		
	//printf("size:%d\r\n", size);
	return size;
}

int W5500_read_TX_free_size(W5500_chip* SPI_p_loc, int sock_nb) {
	int size=9999, previous_size=9999;
	unsigned char data[10];
	do {
		previous_size = size;
		W5500_read_short(SPI_p_loc, 0x0020, (sock_nb*4)+1, data, 2);
		size = data[1] + data[0]*256;
	} while (previous_size != size);
		
	//printf("size:%d\r\n", size);
	return size;
}

void W5500_read_RX_buffer(W5500_chip* SPI_p_loc, int sock_nb, unsigned char* data, int size) {
	unsigned char read_pointer_raw[10];
	unsigned short read_pointer;
	W5500_read_short(SPI_p_loc, 0x0028, (sock_nb*4)+1, read_pointer_raw, 2);
	read_pointer = read_pointer_raw[1] + read_pointer_raw[0] * 256;
	W5500_read_long(SPI_p_loc, read_pointer, (sock_nb*4)+3, data, size);
	read_pointer = read_pointer + size ;
	read_pointer_raw[0] = read_pointer / 256;
	read_pointer_raw[1] = read_pointer & 0xFF;
	W5500_write_short(SPI_p_loc, 0x0028, (sock_nb*4)+1, read_pointer_raw, 2);
	W5500_write_byte(SPI_p_loc, 0x0001, (sock_nb*4)+1, 0x40);
}

int W5500_read_UDP_pckt (W5500_chip* SPI_p_loc, int sock_nb, unsigned char* data) {
	int size=0;
	unsigned char read_pointer_raw[10];
	unsigned char trash[20];
	unsigned short read_pointer;
	unsigned char W5_command[20];
	W5500_read_short(SPI_p_loc, 0x0028, (sock_nb*4)+1, read_pointer_raw, 2);
	read_pointer = read_pointer_raw[1] + read_pointer_raw[0] * 256;
	// read first 8 bytes
	W5_command[0] = read_pointer_raw[0];
	W5_command[1] = read_pointer_raw[1];
	W5_command[2] = ((sock_nb*4)+3) * 0x08;
	SPI_p_loc->cs->write(0);
	SPI_p_loc->spi_port->transfer_2 (W5_command, 3, trash, 3);
	SPI_p_loc->spi_port->transfer_2 (trash, 8, data, 8);
	size = data [7] + 256 * data[6];
	SPI_p_loc->spi_port->transfer_2 (trash, size, data+8, size);
	
	size = size + 8;
	wait_us(1);
	SPI_p_loc->cs->write(1);
	wait_us(2);
	//printf ("size UDP:%d\r\n", size);
	read_pointer = read_pointer + size ;
		
	read_pointer_raw[0] = read_pointer / 256;
	read_pointer_raw[1] = read_pointer & 0xFF;
	W5500_write_short(SPI_p_loc, 0x0028, (sock_nb*4)+1, read_pointer_raw, 2);
	W5500_write_byte(SPI_p_loc, 0x0001, (sock_nb*4)+1, 0x40);//command receive
	return size; 
}

int W5500_read_MAC_pckt (W5500_chip* SPI_p_loc, int sock_nb, unsigned char* data) {
	int size=0;
	unsigned char read_pointer_raw[10];
	unsigned char trash[20];
	unsigned short read_pointer;
	unsigned char W5_command[20];
	W5500_read_short(SPI_p_loc, 0x0028, (sock_nb*4)+1, read_pointer_raw, 2);
	read_pointer = read_pointer_raw[1] + read_pointer_raw[0] * 256;
	// read first 8 bytes
	W5_command[0] = read_pointer_raw[0];
	W5_command[1] = read_pointer_raw[1];
	W5_command[2] = ((sock_nb*4)+3) * 0x08;
	SPI_p_loc->cs->write(0);
	SPI_p_loc->spi_port->transfer_2 (W5_command, 3, trash, 3);
	SPI_p_loc->spi_port->transfer_2 (trash, 2, data, 2);
	size = data [1] + 256 * data[0];
	SPI_p_loc->spi_port->transfer_2 (trash, size-2, data+2, size-2);
	
	wait_us(1);
	SPI_p_loc->cs->write(1);
	wait_us(2);
	//printf ("size UDP:%d\r\n", size);
	read_pointer = read_pointer + size ;
		
	read_pointer_raw[0] = read_pointer / 256;
	read_pointer_raw[1] = read_pointer & 0xFF;
	W5500_write_short(SPI_p_loc, 0x0028, (sock_nb*4)+1, read_pointer_raw, 2);
	W5500_write_byte(SPI_p_loc, 0x0001, (sock_nb*4)+1, 0x40);
	return size; 
}

void W5500_write_TX_buffer(W5500_chip* SPI_p_loc, int sock_nb, unsigned char* data, int size, int send_mac) {
	unsigned char write_pointer_raw[10];
	unsigned short write_pointer;
	W5500_read_short(SPI_p_loc, 0x0024, (sock_nb*4)+1, write_pointer_raw, 2);
	write_pointer = write_pointer_raw[1] + write_pointer_raw[0] * 256;
	W5500_write_long(SPI_p_loc, write_pointer, (sock_nb*4)+2, data, size);
	write_pointer = write_pointer + size ;
	write_pointer_raw[0] = write_pointer / 256;
	write_pointer_raw[1] = write_pointer & 0xFF;
	W5500_write_short(SPI_p_loc, 0x0024, (sock_nb*4)+1, write_pointer_raw, 2);
	if (send_mac == 1) {
		W5500_write_byte(SPI_p_loc, 0x0001, (sock_nb*4)+1, 0x21);
	} else {
		W5500_write_byte(SPI_p_loc, 0x0001, (sock_nb*4)+1, 0x20);
	}
}

static int W5500_configured = 0; 	// 0 not yet configured
									// 1 configured
									// 2 waiting reconfigure
									// 3 waiting reboot after reconfigure
void W5500_re_configure (void) {
	W5500_configured = 2; 
	
}

void W5500_re_configure_gateway(W5500_chip* SPI_p_loc) {
	unsigned char data[10];
	if ( (LAN_conf_applied.LAN_def_route_activ) && (is_telnet_routed) && (is_TDMA_master) ) { 
		IP_int2char (LAN_conf_applied.LAN_def_route, data);
		W5500_write_long(SPI_p_loc, 0x0001, 0x00, data, 4); // gateway 
	} else {
		IP_int2char (0x01010101, data);
		W5500_write_long(SPI_p_loc, 0x0001, 0x00, data, 4);
	}	
}

void W5500_re_configure_periodic_call(W5500_chip* SPI_p_loc) {
	unsigned char data[10];
	if (W5500_configured == 4) { // reboot
		W5500_write_byte(SPI_p_loc, 0x002E, 0x00, 0xC8);//!!! 0xC8
		W5500_configured = 1; //configured
	}
	if (W5500_configured == 3) { //wait 
		W5500_configured = 4;
	}
	if (W5500_configured == 2) { //reconfigure
		IP_int2char (LAN_conf_applied.LAN_modem_IP, data);
		W5500_write_long(SPI_p_loc, 0x000F, 0x00, data, 4); // modem IP
		IP_int2char (LAN_conf_applied.LAN_subnet_mask, data);
		W5500_write_long(SPI_p_loc, 0x0005, 0x00, data, 4); // net mask
		
		if ( (LAN_conf_applied.LAN_def_route_activ) && (is_telnet_routed) && (is_TDMA_master) ) { 
			IP_int2char (LAN_conf_applied.LAN_def_route, data);
			W5500_write_long(SPI_p_loc, 0x0001, 0x00, data, 4); // gateway 
		} else {
			IP_int2char (0x01010101, data);
			W5500_write_long(SPI_p_loc, 0x0001, 0x00, data, 4);
		}
		W5500_write_byte(SPI_p_loc, 0x002E, 0x00, 0x48); // Phy OFF !!! 0x48
		W5500_configured = 3; //waiting reboot
	}
	
}

void W5500_initial_configure(W5500_chip* SPI_p_loc) {
    // reset
    W5500_write_byte(SPI_p_loc, 0x0000, 0x00, 0x80);//0x90
	wait_ms(500);
	W5500_write_byte(SPI_p_loc, 0x0000, 0x00, 0x00);//0x10
	//wait_ms(1600);
	//W5500_write_byte(SPI_p_loc, 0x002E, 0x00, 0xC8);
	W5500_write_byte(SPI_p_loc, 0x002E, 0x00, 0x48);//48 for 10MB full duplex / 40 half duplex !!!
	wait_ms(1600);
	W5500_write_byte(SPI_p_loc, 0x002E, 0x00, 0xC8);//4c8 for 10MB full duplex / c0 half duplexc8 !!!
	//W5500_write_byte(SPI_p_loc, 0x002E, 0x00, 0xC8);
	//wait_ms(1600);
    
	//W5500_write_byte(SPI_p_loc, 0x002E, 0x00, 0xC0);
    
    //IP & MAC config
    //unsigned char data[20]={0x00,0x2E,0x00,4,5,6, 10,151,20,254};
	//unsigned char data[20]={0x00,0x2E,0x00,4,5,6, 192,168,0,254};
	unsigned char data[20]={0x00,0x2E,0x00,4,5,6};
	//W5500_write_long(SPI_p_loc, 0x0009, 0x00, data, 6); // modem MAC
	//W5500_write_long(SPI_p_loc, 0x0009, 0x00, LAN_conf_applied.modem_MAC, 6); // modem MAC
	W5500_write_long(SPI_p_loc, 0x0009, 0x00, CONF_modem_MAC, 6); // modem MAC
	IP_int2char (LAN_conf_applied.LAN_modem_IP, data);
	W5500_write_long(SPI_p_loc, 0x000F, 0x00, data, 4); // modem IP
	if ( (is_telnet_routed) && (is_TDMA_master) ) { 
		IP_int2char (LAN_conf_applied.LAN_def_route, data);
		W5500_write_long(SPI_p_loc, 0x0001, 0x00, data, 4); // gateway 
	} else {
		IP_int2char (0, data);
		W5500_write_long(SPI_p_loc, 0x0001, 0x00, data, 4);
	}
	IP_int2char (LAN_conf_applied.LAN_subnet_mask, data);
	W5500_write_long(SPI_p_loc, 0x0005, 0x00, data, 4); // net mask
	
    W5500_write_byte(SPI_p_loc, 0x0018, 0x00, 0x01);//sock interrupt mask 
	
	// Socket Read buffer size
	W5500_write_byte(SPI_p_loc, 0x001E, 0x01, 0x08); //0 macraw 08
	W5500_write_byte(SPI_p_loc, 0x001E, 0x05, 0x02); //1 telnet 02
	W5500_write_byte(SPI_p_loc, 0x001E, 0x09, 0x04); //2 RTP 04
	W5500_write_byte(SPI_p_loc, 0x001E, 0x0D, 0x02); //3 DHCP 02
	W5500_write_byte(SPI_p_loc, 0x001E, 0x11, 0x00); //4 
	W5500_write_byte(SPI_p_loc, 0x001E, 0x15, 0x00); //5 
	W5500_write_byte(SPI_p_loc, 0x001E, 0x19, 0x00); //6 
	W5500_write_byte(SPI_p_loc, 0x001E, 0x1D, 0x00); //7 
	
	// Socket Write buffer size
	W5500_write_byte(SPI_p_loc, 0x001F, 0x01, 0x04); //0 macraw
	W5500_write_byte(SPI_p_loc, 0x001F, 0x05, 0x02); //1 telnet
	W5500_write_byte(SPI_p_loc, 0x001F, 0x09, 0x04); //2 RTP
	W5500_write_byte(SPI_p_loc, 0x001F, 0x0D, 0x02); //3 DHCP
	W5500_write_byte(SPI_p_loc, 0x001F, 0x11, 0x02); //4 UDP_FDD
	W5500_write_byte(SPI_p_loc, 0x001F, 0x15, 0x00); //5 
	W5500_write_byte(SPI_p_loc, 0x001F, 0x19, 0x00); //6 
	W5500_write_byte(SPI_p_loc, 0x001F, 0x1D, 0x00); //7 
	
	// Socket 0 MAC RAW for packet switching
	W5500_write_byte(SPI_p_loc, 0x0000, 0x01, 0x34); //config B4 for classic
	W5500_write_byte(SPI_p_loc, 0x002C, 0x01, 0x04); //Interrupt mask : RECV
	wait_ms(10);
	W5500_write_byte(SPI_p_loc, 0x0001, 0x01, 0x01); //open
	
    // Socket 1 telnet 
    W5500_write_byte(SPI_p_loc, 0x0000, 0x05, 0x01); //config
    wait_ms(10);
    data[0]=0x00; //port 0d23
    data[1]=0x17;
    W5500_write_short(SPI_p_loc, 0x0004, 0x05, data, 2); //port 23 (0x17)
	//W5500_write_byte(SPI_p_loc, 0x002C, 0x05, 0x00); //interrupt mask
	
	// Socket 2 RTP port 1519
	W5500_write_byte(SPI_p_loc, 0x0000, 0x09, 0x42); //config
	wait_ms(10);
	data[0]=0x05; //port 0d1519
    data[1]=0xEF;
	W5500_write_short(SPI_p_loc, 0x0004, 0x09, data, 2); //port rx 1519 
	data[0]=0x05; //port 0d1516
    data[1]=0xEC;
	W5500_write_short(SPI_p_loc, 0x0010, 0x09, data, 2); //port tx 1516
	W5500_write_byte(SPI_p_loc, 0x0001, 0x09, 0x01); // open
	data[0]=10;
	data[1]=151;
	data[2]=0;
	data[3]=60;
	W5500_write_short(SPI_p_loc, 0x000C, 0x09, data, 4); //IP destination 10.151.0.21
	
	// Socket 3 DHCP server
	W5500_write_byte(SPI_p_loc, 0x0000, 0x0D, 0x02); //config
	wait_ms(10);
	data[0]=0x00; //port 0d67
    data[1]=0x43;
	W5500_write_short(SPI_p_loc, 0x0004, 0x0D, data, 2); //port rx 67 
	data[0]=0x00; //port 0d68
    data[1]=0x44;
	W5500_write_short(SPI_p_loc, 0x0010, 0x0D, data, 2); //port tx 68
	W5500_write_byte(SPI_p_loc, 0x0001, 0x0D, 0x01); // open
	data[0]=255;
	data[1]=255;
	data[2]=255;
	data[3]=255;
	data[4]=255;
	data[5]=255;
	W5500_write_short(SPI_p_loc, 0x000C, 0x0D, data, 4); //IP destination 255.255.255.255
	W5500_write_short(SPI_p_loc, 0x0006, 0x0D, data, 6);
	
	// Socket 4 UDP_FDD
	W5500_write_byte(SPI_p_loc, 0x0000, 0x11, 0x42); //config
	wait_ms(10);
	data[0]=0x1A; //port TX 0d6716 = 0x1A3E
    data[1]=0x3E;
	W5500_write_short(SPI_p_loc, 0x0004, 0x11, data, 2);
	data[0]=0x1A; //port RX 0d6718 = 0x1A3C
    data[1]=0x3C;
	W5500_write_short(SPI_p_loc, 0x0010, 0x11, data, 2);
	W5500_write_byte(SPI_p_loc, 0x0001, 0x11, 0x01); // open
	IP_int2char (CONF_master_down_IP, data);
	W5500_write_short(SPI_p_loc, 0x000C, 0x11, data, 4);
	// Socket 5
	
	// Socket 6
	
	// Socket 7
	W5500_configured = 1;
}