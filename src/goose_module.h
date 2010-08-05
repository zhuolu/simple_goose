/*
 * Name        : goose_module.h
 * Description : GOOSE kernel module 
 * File        : The head file for GOOSE messaging
 * Dev. Plat.  : kernel version 2.6.32, gcc version 4.4.1
 *
 */

#ifndef _IEC61850_GOOSE_MODULE_H
#define _IEC61850_GOOSE_MODULE_H

/* Netlink interface, the GOOSE module uses 20 */
#define NETLINK_GOOSE            20
#define NL_MAX_DATALEN_ACCEPTED  1000

/* The netlink frame between kernel and user
 *
 * User => Kernel:
 *
 * GOOSE Data:
 * --------------------------------------------
 * | nl_data_header | goose_header | APDU ... |
 * --------------------------------------------
 *
 * GOOSE Control information
 * ------------------
 * | nl_ctrl_header |
 * ------------------
 *
 * Kernel => User:
 *
 * GOOSE Data:
 * ----------------------------------------------------
 * | nl_data_header | 88 b8 | goose_header | APDU ... |
 * ----------------------------------------------------
 *                      ||
 *              (GOOSE protocol type)
 *
 */

/* We use nlmsg_type in struct nlmsghdr to classify
 * message types sent from user space.
 * Usage of all 16 bits:
 *   bit 1 - message is of ctrl type
 *       2 - message should be broadcasted
 *       3 - message should be unicasted
 *       4 - message should be transmitted by
 *           GOOSE enhanced retransmission mechanism
 */
#define NL_MSG_CTRL              0x0001
#define NL_MSG_DATA_BRDCAST      0x0002
#define NL_MSG_DATA_UNICAST      0x0004
#define NL_MSG_DATA_RELB         0x0008
#define NL_MSG_REPORT_TO_MODULE  0xffff

/* User space control header
 * If message type is NL_MSG_CTRL,
 * the send should transmit a nl_ctrl_header.
 */
struct goose_param_t {
	unsigned int thresh;
	unsigned int intvl_init;
	unsigned int intvl_max;
	unsigned int intvl_incre;
};

/* Length of dev_name is 16
 * from linux/if.h
 */
#define IFNAMSIZE 16

struct nl_ctrl_header {
	struct goose_param_t  goose_param;
	char                  def_dev[IFNAMSIZE];
};

/* User space data header
 * If message type is NL_MSG_DATA_XXX,
 * the send should transmit a nl_data_header, followed by data.
 */
struct nl_data_header {
	char dev_name[IFNAMSIZE]; /* network device name, e.g. "eth0" */		
	unsigned char daddr[6];   /* 6-byte destination MAC address, If the
							   * message type is NL_MSG_DATA_BRDCAST, kernel
							   * will ignore daddr. */
	unsigned char saddr[6];
};


/* Maximum number of retransmissions for GOOSE enhanced retransmission*/
#define MAX_GOOSE_TRANS_NUM      32

/* Proc_fs name, buffer size, and defaults */
#define PROC_DNAME                       "goose"
#define PROC_FNAME_DEF_DEV               "def_dev"
#define PROC_FNAME_PKT_SIZE              "pkt_size"
#define PROC_FNAME_TRAN_INTVL            "tran_intvl"
#define PROC_FNAME_DELAY_THRE            "delay_thre"
#define PROC_FNAME_RETRAN_INTVL          "retran_intvl"
#define PROC_FNAME_RETRAN_INCRE          "retran_incre"
#define PROC_FNAME_MAX_RETRAN_INTVL      "max_retran_intvl"

#define PROC_PKT_SIZE_BUFLEN             8
#define PROC_DEF_DEV_BUFLEN              16
#define PROC_TRAN_INTVL_BUFLEN           16
#define PROC_DELAY_THRE_BUFLEN           16
#define PROC_RETRAN_INTVL_BUFLEN         16
#define PROC_RETRAN_INCRE_BUFLEN         16
#define PROC_MAX_RETRAN_INTVL_BUFLEN     16

/* Default values:
 *  size in (byte), time in (ms).
 */
#define DEFBUF_PROC_DEF_DEV    "eth0"
#define DEF_TRAN_INTVL         5000
#define DEF_DELAY_THRE         50
#define DEF_RETRAN_INTVL       10
#define DEF_RETRAN_INCRE       0
#define DEF_MAX_RETRAN_INTVL   10

/* Deamon loop idle time*/
#define DAEMON_LOOP_IDLE_TIME  300

#endif  /* _IEC61850_GOOSE_MODULE_H */
