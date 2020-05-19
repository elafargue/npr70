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

#include "SNMP.h"
#include "Eth_IPv4.h"

// We are reusing similar data structures as what's done in the WizNet ioLib
// SNMP message structures
// 

// This will reserve about 1k of RAM
struct messageStruct request_msg;
struct messageStruct response_msg;
uint8_t errorStatus, errorIndex;

dataEntryType snmpData[] =
    {
        // System MIB
        // SysDescr Entry
        {8, {0x2b, 6, 1, 2, 1, 1, 1, 0}, ASN_OCTETSTRING, 6, {"NPR-70"}, NULL, NULL},

        // SysUptime Entry
        {8, {0x2b, 6, 1, 2, 1, 1, 3, 0}, ASN_TIMETICKS, 0, {}, snmpGetUptime, NULL},

        // 1.3.6.1.4.1.54539 (Wizkers)
        //               .100 (NPR-70)
        //               .1               // Temperature
        {11, {0x2b, 6, 1, 4, 1, 0x83, 0xaa, 0x0b, 100, 1, 0}, ASN_GAUGE,0, {}, snmpGetTemp, NULL },
        //               .2               // Link status
        {11, {0x2b, 6, 1, 4, 1, 0x83, 0xaa, 0x0b, 100, 2, 0}, ASN_INTEGER,0, {}, snmpGetLinkStatus, NULL },
        //               .3               // Link distance
        {11, {0x2b, 6, 1, 4, 1, 0x83, 0xaa, 0x0b, 100, 3, 0}, ASN_INTEGER,0, {}, snmpGetLinkDistance, NULL },
        //               .4.0             // RSSI Uplink
        {11, {0x2b, 6, 1, 4, 1, 0x83, 0xaa, 0x0b, 100, 4, 0}, ASN_GAUGE,0, {}, snmpGetRSSIUp, NULL },
        //               .5.0             // RSSI Downlink
        {11, {0x2b, 6, 1, 4, 1, 0x83, 0xaa, 0x0b, 100, 5, 0}, ASN_GAUGE,0, {}, snmpGetRSSIDown, NULL },
        //               .6.0             // Error rate Uplink
        {11, {0x2b, 6, 1, 4, 1, 0x83, 0xaa, 0x0b, 100, 6, 0}, ASN_GAUGE,0, {}, snmpGetErrUp, NULL },
        //               .7.0             // Error rate Downlink
        {11, {0x2b, 6, 1, 4, 1, 0x83, 0xaa, 0x0b, 100, 7, 0}, ASN_GAUGE,0, {}, snmpGetErrDown, NULL },

};

const uint8_t maxData = (sizeof(snmpData) / sizeof(dataEntryType));

void hexdump(uint8_t *buffer, uint16_t len)
{
    int i;

    if (len == 0)
    {
        printf("  ZERO LENGTH\r\n");
        return;
    }

    for (i = 0; i < len; i++)
    {
        if ((i % 16) == 0)
        {
            printf("\r\n");
        }
        printf(" %02x", buffer[i]);
    }
    printf("\r\n");
}

void snmpGetUptime(void *ptr, uint8_t *len) {
    time_t seconds = time(NULL);
    *(uint32_t *)ptr = (uint32_t)(seconds*100); // Time in 100th of a second
    *len = 4; 
}

void snmpGetVar(void *ptr, uint8_t *len,  const uint32_t var) {
	*(uint32_t *)ptr = (uint32_t)(var);
	*len = 4;
}

void snmpGetTemp(void *ptr, uint8_t *len)
{
    snmpGetVar(ptr, len, G_temperature_SI4463);
}

void snmpGetLinkStatus(void *ptr, uint8_t *len)
{
    // State can be:
    // 1: "waiting connection"
    // 2: "connected"
    // 3: "connection rejected"
    snmpGetVar(ptr, len, my_client_radio_connexion_state);
}

void snmpGetLinkDistance(void *ptr, uint8_t *len)
{
    // Formula is : 0.15 * value
    snmpGetVar(ptr, len, TDMA_table_TA[my_radio_client_ID]);
}

void snmpGetRSSIUp(void *ptr, uint8_t *len)
{
    // Formula for dBm is: value/2-136
    snmpGetVar(ptr, len, G_radio_addr_table_RSSI[my_radio_client_ID]);
}

void snmpGetRSSIDown(void *ptr, uint8_t *len)
{
    // Formula for dBm is: value/2-136
    snmpGetVar(ptr, len, (G_downlink_RSSI >> 8));
}

void snmpGetErrUp(void *ptr, uint8_t *len)
{
    // Formula for percent is: value / 500
    snmpGetVar(ptr, len, G_radio_addr_table_BER[my_radio_client_ID]);
}

void snmpGetErrDown(void *ptr, uint8_t *len)
{
    // Formula for percent is: value /500
    snmpGetVar(ptr, len, G_downlink_BER);
}

int8_t findEntry(uint8_t *oid, uint8_t len)
{
    uint8_t i;

    for (i = 0; i < maxData; i++)
    {
        if (len == snmpData[i].oidlen)
        {
            if (!memcmp(snmpData[i].oid, oid, len))
                return (i);
        }
    }

    return OID_NOT_FOUND;
}

int8_t getOID(int8_t id, uint8_t *oid, uint8_t *len)
{
    uint8_t j;

    if (!((id >= 0) && (id < maxData)))
        return INVALID_ENTRY_ID;

    *len = snmpData[id].oidlen;

    for (j = 0; j < *len; j++)
    {
        oid[j] = snmpData[id].oid[j];
    }

    return SNMP_SUCCESS;
}

int32_t getValue(uint8_t *vptr, int32_t vlen)
{
    int32_t index = 0;
    int32_t value = 0;

    while (index < vlen)
    {
        if (index != 0)
            value <<= 8;
        value |= vptr[index++];
    }

    return value;
}

int32_t getEntry(int8_t id, uint8_t *dataType, void *ptr, uint8_t *len)
{
    uint8_t *ptr_8;
    int32_t value;

    uint8_t *string;
    uint8_t j;

    if (!((id >= 0) && (id < maxData)))
        return INVALID_ENTRY_ID;

    *dataType = snmpData[id].dataType;

    switch (*dataType)
    {
    case ASN_OCTETSTRING:
    case ASN_OBJECTID:
    {
        string = (uint8_t*) ptr;

        if (snmpData[id].getfunction != NULL)
        {
            snmpData[id].getfunction((void *)&snmpData[id].u.octetstring, &snmpData[id].dataLen);
        }

        if ((*dataType) == ASN_OCTETSTRING)
        {
            snmpData[id].dataLen = (uint8_t)strlen((char const *)&snmpData[id].u.octetstring);
        }

        *len = snmpData[id].dataLen;
        for (j = 0; j < *len; j++)
        {
            string[j] = snmpData[id].u.octetstring[j];
        }
    }
    break;

    case ASN_INTEGER:
    case ASN_TIMETICKS:
    case ASN_COUNTER:
    case ASN_GAUGE:
    {
        if (snmpData[id].getfunction != NULL)
        {
            snmpData[id].getfunction((void *)&snmpData[id].u.intval, &snmpData[id].dataLen);
        }

        if (snmpData[id].dataLen)
            *len = snmpData[id].dataLen;
        else
            *len = sizeof(uint32_t);

        ptr_8 = (uint8_t*) ptr;
        value = snmpData[id].u.intval;

        for (j = 0; j < *len; j++)
        {
            ptr_8[j] = (uint8_t)((value >> ((*len - j - 1) * 8)));
        }
    }
    break;

    default:
        return INVALID_DATA_TYPE;
    }

    return SNMP_SUCCESS;
}

int8_t setEntry(uint8_t id, void *val, int32_t vlen, uint8_t dataType, int32_t index)
{

    int8_t retStatus = OID_NOT_FOUND;
    int32_t j;

    if (snmpData[id].dataType != dataType)
    {
        errorStatus = BAD_VALUE;
        errorIndex = index;
        return INVALID_DATA_TYPE;
    }

    switch (snmpData[id].dataType)
    {
    case ASN_OCTETSTRING:
    case ASN_OBJECTID:
    {
        uint8_t *string = (uint8_t*) val;
        for (j = 0; j < vlen; j++)
        {
            snmpData[id].u.octetstring[j] = string[j];
        }
        snmpData[id].dataLen = vlen;
    }
        retStatus = SNMP_SUCCESS;
        break;

    case ASN_INTEGER:
    case ASN_TIMETICKS:
    case ASN_COUNTER:
    case ASN_GAUGE:
    {
        snmpData[id].u.intval = getValue((uint8_t *)val, vlen);
        snmpData[id].dataLen = vlen;

        if (snmpData[id].setfunction != NULL)
        {
            snmpData[id].setfunction(snmpData[id].u.intval);
        }
    }
        retStatus = SNMP_SUCCESS;
        break;

    default:
        retStatus = INVALID_DATA_TYPE;
        break;
    }

    return retStatus;
}

void insertRespLen(int32_t reqStart, int32_t respStart, int16_t size)
{
	int32_t indexStart, lenLength;
	uint32_t mask = 0xff;
	int32_t shift = 0;

	if (request_msg.buffer[reqStart+1] & 0x80)
	{
		lenLength = request_msg.buffer[reqStart+1] & 0x7f;
		indexStart = respStart+2;

		while (lenLength--)
		{
			response_msg.buffer[indexStart+lenLength] = 
				(uint8_t)((size & mask) >> shift);
			shift+=8;
			mask <<= shift;
		}
	}
	else
	{
		response_msg.buffer[respStart+1] = (uint8_t)(size & 0xff);
	}
}

uint16_t parseLength(const uint8_t *msg, uint16_t *len)
{
    uint16_t i = 1;

    if (msg[0] & 0x80)
    {
        uint16_t tlen = (msg[0] & 0x7f) - 1;
        *len = msg[i++];
        while (tlen--)
        {
            *len <<= 8;
            *len |= msg[i++];
        }
    }
    else
    {
        *len = msg[0];
    }
    return i;
}

void parseTLV(const uint8_t *msg, int32_t index, tlvStructType *tlv)
{
    int32_t Llen = 0;

    tlv->start = index;
    Llen = parseLength((const uint8_t *)&msg[index + 1], &tlv->len);
    tlv->vstart = index + Llen + 1;

    switch (msg[index])
    {
    case SNMP_SEQUENCE:
    case GET_REQUEST:
    case GET_NEXT_REQUEST:
    case SET_REQUEST:
        tlv->nstart = tlv->vstart;
        break;
    default:
        tlv->nstart = tlv->vstart + tlv->len;
        break;
    }

}


int32_t parseVarBind(int32_t reqType, int32_t index)
{
    int32_t seglen = 0;
    int8_t id;
    tlvStructType name, value;
    int32_t size = 0;

    parseTLV(request_msg.buffer, request_msg.index, &name);

    if (request_msg.buffer[name.start] != ASN_OBJECTID)
        return -1;

    id = findEntry(&request_msg.buffer[name.vstart], name.len);

    if ((reqType == GET_REQUEST) || (reqType == SET_REQUEST))
    {
        seglen = name.nstart - name.start;
        COPY_SEGMENT(name);
        size = seglen;
    }
    else if (reqType == GET_NEXT_REQUEST)
    {
        response_msg.buffer[response_msg.index] = request_msg.buffer[name.start];

        if (++id >= maxData)
        {
            id = OID_NOT_FOUND;
            seglen = name.nstart - name.start;
            COPY_SEGMENT(name);
            size = seglen;
        }
        else
        {
            request_msg.index += name.nstart - name.start;

            getOID(id, &response_msg.buffer[response_msg.index + 2], &response_msg.buffer[response_msg.index + 1]);

            seglen = response_msg.buffer[response_msg.index + 1] + 2;
            response_msg.index += seglen;
            size = seglen;
        }
    }

    parseTLV(request_msg.buffer, request_msg.index, &value);

    if (id != OID_NOT_FOUND)
    {
        uint8_t dataType;
        uint8_t len;

        if ((reqType == GET_REQUEST) || (reqType == GET_NEXT_REQUEST))
        {
            getEntry(id, &dataType, &response_msg.buffer[response_msg.index + 2], &len);

            response_msg.buffer[response_msg.index] = dataType;
            response_msg.buffer[response_msg.index + 1] = len;
            seglen = (2 + len);
            response_msg.index += seglen;

            request_msg.index += (value.nstart - value.start);
        }
        else if (reqType == SET_REQUEST)
        {
            setEntry(id, &request_msg.buffer[value.vstart], value.len, request_msg.buffer[value.start], index);
            seglen = value.nstart - value.start;
            COPY_SEGMENT(value);
        }
    }
    else
    {
        seglen = value.nstart - value.start;
        COPY_SEGMENT(value);

        errorIndex = index;
        errorStatus = NO_SUCH_NAME;
    }

    size += seglen;
    return size;
}

int32_t parseSequence(int32_t reqType, int32_t index)
{
    int32_t seglen;
    tlvStructType seq;
    int32_t size = 0, respLoc;

    parseTLV(request_msg.buffer, request_msg.index, &seq);

    if (request_msg.buffer[seq.start] != SNMP_SEQUENCE)
        return -1;

    seglen = seq.vstart - seq.start;
    respLoc = response_msg.index;
    COPY_SEGMENT(seq);

    size = parseVarBind(reqType, index);
    insertRespLen(seq.start, respLoc, size);
    size += seglen;

    return size;
}

int32_t parseSequenceOf(int32_t reqType)
{
    int32_t seglen;
    tlvStructType seqof;
    int32_t size = 0, respLoc;
    int32_t index = 0;

    parseTLV(request_msg.buffer, request_msg.index, &seqof);

    if (request_msg.buffer[seqof.start] != SNMP_SEQUENCE_OF)
        return -1;

    seglen = seqof.vstart - seqof.start;
    respLoc = response_msg.index;
    COPY_SEGMENT(seqof);

    while (request_msg.index < request_msg.len)
    {
        size += parseSequence(reqType, index++);
    }

    insertRespLen(seqof.start, respLoc, size);

    return size;
}

int32_t parseRequest()
{
    int32_t ret, seglen;
    tlvStructType snmpreq, requestid, errStatus, errIndex;
    int32_t size = 0, respLoc, reqType;

    parseTLV(request_msg.buffer, request_msg.index, &snmpreq);

    reqType = request_msg.buffer[snmpreq.start];

    if (!VALID_REQUEST(reqType))
        return -1;

    seglen = snmpreq.vstart - snmpreq.start;
    respLoc = snmpreq.start;
    size += seglen;
    COPY_SEGMENT(snmpreq);

    response_msg.buffer[snmpreq.start] = GET_RESPONSE;

    // Insert the Request ID
    parseTLV(request_msg.buffer, request_msg.index, &requestid);
    seglen = requestid.nstart - requestid.start;
    size += seglen;
    COPY_SEGMENT(requestid);

    // Insert the error status
    parseTLV(request_msg.buffer, request_msg.index, &errStatus);
    seglen = errStatus.nstart - errStatus.start;
    size += seglen;
    COPY_SEGMENT(errStatus);

    parseTLV(request_msg.buffer, request_msg.index, &errIndex);
    seglen = errIndex.nstart - errIndex.start;
    size += seglen;
    COPY_SEGMENT(errIndex);

    ret = parseSequenceOf(reqType);
    if (ret == -1)
        return -1;
    else
        size += ret;

    insertRespLen(snmpreq.start, respLoc, size);

    if (errorStatus)
    {
        response_msg.buffer[errStatus.vstart] = errorStatus;
        response_msg.buffer[errIndex.vstart] = errorIndex + 1;
    }

    return size;
}

int16_t parseCommunity()
{
    int32_t seglen;
    tlvStructType community;
    int16_t size = 0;

    parseTLV(request_msg.buffer, request_msg.index, &community);

    if (!((request_msg.buffer[community.start] == ASN_OCTETSTRING) && (community.len == COMMUNITY_SIZE)))
    {
        return -1;
    }

    if (!memcmp(&request_msg.buffer[community.vstart], (int8_t *)COMMUNITY, COMMUNITY_SIZE))
    {
        seglen = community.nstart - community.start;
        size += seglen;
        COPY_SEGMENT(community);
        size += parseRequest();
    }
    else
    {
        return -1;
    }

    return size;
}

int16_t parseVersion()
{
    int16_t size = 0, seglen;
    tlvStructType tlv;

    parseTLV(request_msg.buffer, request_msg.index, &tlv);

    if (!((request_msg.buffer[tlv.start] == ASN_INTEGER) && (request_msg.buffer[tlv.vstart] == SNMP_V1)))
        return -1;

    seglen = tlv.nstart - tlv.start;
    size += seglen;
    COPY_SEGMENT(tlv);
    size = parseCommunity();

    if (size == -1)
        return size;
    else
        return (size + seglen);
}

int8_t parse_snmp()
{
    int16_t size = 0, seglen, respLoc;
    tlvStructType tlv;

    parseTLV(request_msg.buffer, request_msg.index, &tlv);

    if (request_msg.buffer[tlv.start] != SNMP_SEQUENCE)
        return -1;

    seglen = tlv.vstart - tlv.start;
    respLoc = tlv.start; // Save this location to fill in the final response size
    COPY_SEGMENT(tlv);

    size = parseVersion(); // parseVersion then calls community which in turns call the next TLV parsing etc...

    if (size == -1)
        return -1;
    else
        size += seglen;

    insertRespLen(tlv.start, respLoc, size);

    return 0;
}

/**
 * Initialize the SNMP stack
 */
void snmp_init() {
    set_time(0);
}

/**
 * Called at regular intervals and acts if the SNMP socket contains data
 */
void snmp_loop(W5500_chip *W5500)
{
    int8_t ret;

    if (W5500_read_received_size(W5500, SNMP_SOCKET) > 0)
    {
        uint8_t buf[4];
        uint16_t idx = 0;

        request_msg.len = W5500_read_UDP_pckt(W5500, SNMP_SOCKET, request_msg.buffer, MAX_SNMPMSG_LEN);
        //hexdump(request_msg.buffer, request_msg.len);

        // Extract the caller's IP and source port for our reply:
        for (int i=0;i<6;i++) {
            buf[i] = request_msg.buffer[i];
        }
	    W5500_write_long(W5500, W5500_Sn_DIPR0, SNMP_SOCKET, buf, 4); //IP destination
        W5500_write_long(W5500, W5500_Sn_DPORT0, SNMP_SOCKET, buf+4, 2); //port tx

        // Initialize the response
        // We have 8 extra bytes at the beginning we don't want, so we realign the buffer:
        request_msg.len -= 8;
        while (idx < request_msg.len) {
            request_msg.buffer[idx] = request_msg.buffer[idx+8];
            idx++;
        }
        request_msg.index = 0; 
        response_msg.index = 0;
        errorStatus = errorIndex = 0;
        memset(response_msg.buffer, 0x00, MAX_SNMPMSG_LEN);

        ret = parse_snmp();
        if (ret > -1)
        {
            // printf("Sending response to valid query\r\n");
            // hexdump(response_msg.buffer, response_msg.index);
            W5500_write_TX_buffer(W5500, SNMP_SOCKET, response_msg.buffer, response_msg.index, 0);
        } else {
            printf("Failed to parse the SNMP message: %i\r\n", ret);
        }
    }
}
