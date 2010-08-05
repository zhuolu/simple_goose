/*
 * Name        : proto_goose.h
 * Description : GOOSE kernel module 
 * File        : The head file for GOOSE messaging
 * Dev. Plat.  : kernel version 2.6.32, gcc version 4.4.1
 *
 */

#ifndef _IEC61850_PROTO_GOOSE_H
#define _IEC61850_PROTO_GOOSE_H

//#include <linux/netdevice.h>

/* Ethernet Protocol Type for GOOSE */
#define ETH_P_GOOSE 0x88b8

/* In our reliability extention,
 *  reserv1 is used for security type in IEC 62351.
 *  reserv2 is used for sequence number
 *  reserv3 is used for marking sequence type:
 *    TYPE_ACK - ack or TYPE_FRAME - transmssion.
 */
#define GOOSE_EXT_TYPE_ACK 0x01
#define GOOSE_EXT_TYPE_FRAME 0x00

/* GOOSE header */
struct goosehdr {
	unsigned short appid;
	unsigned short len;
	unsigned char reserv1;  /* Signature length for security */
	unsigned char reserv2;  /* Seq number */
	unsigned char reserv3;  /* Seq type */
	unsigned char reserv4;  /* Not used */
};
	
#endif  /* _IEC61850_PROTO_GOOSE_H */
