/* GOOSE user-kernel interfance and APIs
 * Header file for user space
 */

#include <sys/stat.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <string.h>
#include <asm/types.h>
#include <linux/netlink.h>
#include <linux/socket.h>
#include <arpa/inet.h>

#include <semaphore.h>

#include "goose_module.h"
#include "proto_goose.h"

/* Netlink interface:
 * Store socket, caches for interation with kernel.
 * Starts with
 *    nl_if_init(...)
 * and ends with
 *    nl_if_close(...)
 */
struct nl_interface {
	struct iovec iov_in, iov_out;
	struct sockaddr_nl src_addr, dest_addr;	
	struct msghdr msg_in, msg_out;
	int sock_fd;
	sem_t access_in;
	sem_t access_out;	
};

int nl_if_init (struct nl_interface *nl_if);
int nl_if_close (struct nl_interface *nl_if);

/* GOOSE Communication APIs */
int send_raw(struct nl_interface *nl_if, unsigned char *data,
			 unsigned int data_len, unsigned short msg_type);

int send_goose_data(struct nl_interface *nl_if, struct nl_data_header *nl_data_h,
					struct goosehdr *goose_h, unsigned char *apdu,
					unsigned int apdu_len, unsigned short msg_type);

int send_goose_ctrl(struct nl_interface *nl_if, struct nl_ctrl_header *ctrl_info);

int recv_raw(struct nl_interface *nl_if, struct nl_data_header *nl_data_h,
			 struct goosehdr *goose_h, unsigned char *apdu);
