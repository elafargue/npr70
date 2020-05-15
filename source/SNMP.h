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

#ifndef SNMP_AGENT
#define SNMP_AGENT

#include "mbed.h"
#include "W5500.h"
#include "global_variables.h"


#define COMMUNITY					"public\0"
#define COMMUNITY_SIZE				(strlen(COMMUNITY))

/**
 * ASN.1 Data types, see rfc1157
 */

#define ASN_BOOLEAN     0x01
#define ASN_INTEGER     0x02
#define ASN_BITSTRING   0x03
#define ASN_OCTETSTRING 0x04
#define ASN_NULL        0x05
#define ASN_OBJECTID    0x06
#define ASN_SEQUENCE    0x10
#define ASN_SET         0x11

#define SNMP_SEQUENCE    0x30
#define SNMP_SEQUENCE_OF SNMP_SEQUENCE

#define ASN_APPLICATION 0x40

#define ASN_IPADDRESS   (ASN_APPLICATION | 0)
#define ASN_COUNTER     (ASN_APPLICATION | 1)
#define ASN_GAUGE       (ASN_APPLICATION | 2)
#define ASN_TIMETICKS   (ASN_APPLICATION | 3)
#define ASN_COUNTER64   (ASN_APPLICATION | 6)
#define ASN_FLOAT       (ASN_APPLICATION | 8)
#define ASN_DOUBLE      (ASN_APPLICATION | 9)
#define ASN_INTEGER64  (ASN_APPLICATION | 10)
#define ASN_UNSIGNED64 (ASN_APPLICATION | 11)

// We are limiting the max size here to save as much RAM as possible, those
// could be extended on a larger system
#define MAX_OID          12
#define MAX_STRING       16
#define MAX_SNMPMSG_LEN 384

#define SNMP_V1 0

// SNMPv1 Commands
#define GET_REQUEST       0xa0
#define GET_NEXT_REQUEST  0xa1
#define GET_RESPONSE      0xa2
#define SET_REQUEST       0xa3

// SNMP Error code
#define SNMP_SUCCESS        0
#define OID_NOT_FOUND      -1
#define TABLE_FULL         -2
#define ILLEGAL_LENGTH     -3
#define INVALID_ENTRY_ID   -4
#define INVALID_DATA_TYPE  -5

#define NO_SUCH_NAME        2
#define BAD_VALUE           3


#define COPY_SEGMENT(x) \
{ \
    request_msg.index += seglen; \
    memcpy(&response_msg.buffer[response_msg.index], &request_msg.buffer[x.start], seglen ); \
    response_msg.index += seglen; \
}

// Macros: SNMPv1 request validation checker
#define VALID_REQUEST(x)			((x == GET_REQUEST) || (x == GET_NEXT_REQUEST) || (x == SET_REQUEST))

struct messageStruct {
	uint8_t buffer[MAX_SNMPMSG_LEN];
	uint16_t len;
	uint16_t index;
};

typedef struct {
	uint16_t start;		/* Absolute Index of the TLV */
	uint16_t len;		/* The L value of the TLV */
	uint16_t vstart;		/* Absolute Index of this TLV's Value */
	uint16_t nstart;		/* Absolute Index of the next TLV */
} tlvStructType;

typedef struct {
	uint8_t oidlen;
	uint8_t oid[MAX_OID];  // WARNING: since this is an array of uint8_t, long OID elements (> 256) have to be put there as multiple encoded bytes
	uint8_t dataType;
	uint8_t dataLen;
	union {
		uint8_t octetstring[MAX_STRING];
		uint32_t intval;
	} u;
	void (*getfunction)(void *, uint8_t *);
	void (*setfunction)(int32_t);
} dataEntryType;


/**
 * Very simplistic implementation of a SNMP agent that will return basic stats
 * on the modem. Read only, community is 'public'
 */
void snmp_init(uint8_t snmp_socket);
void snmp_loop(W5500_chip* W5500);
void snmpGetTemp(void *ptr, uint8_t *len); // Get system temperature callback
void snmpGetLinkStatus(void *ptr, uint8_t *len);
void snmpGetLinkDistance(void *ptr, uint8_t *len);
void snmpGetRSSIUp(void *ptr, uint8_t *len);
void snmpGetRSSIDown(void *ptr, uint8_t *len);
void snmpGetErrUp(void *ptr, uint8_t *len);
void snmpGetErrDown(void *ptr, uint8_t *len);

#endif