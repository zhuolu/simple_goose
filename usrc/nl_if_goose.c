/* GOOSE user-kernel interfance and APIs */

#include <stdio.h>
#include <stdlib.h>
#include <semaphore.h>

#include "nl_if_goose.h"

/* Netlink interface constructor
 * Allocate memoeries for interaction with kernel.
 * Currently, we only support one process with two
 * way communications in a machine.
 */
int nl_if_init (struct nl_interface *nl_if)
{
	struct nlmsghdr *nlh_in, *nlh_out;
	int ret;

	/* Use Netlink socket with NETLINK_GOOSE */
	nl_if->sock_fd = socket(PF_NETLINK, SOCK_RAW, NETLINK_GOOSE);
	
	memset(&nl_if->msg_in, 0, sizeof(struct msghdr));
	memset(&nl_if->msg_out, 0, sizeof(struct msghdr));	
	memset(&nl_if->src_addr, 0, sizeof(struct sockaddr_nl));
	memset(&nl_if->dest_addr, 0, sizeof(struct sockaddr_nl));
	
	nl_if->src_addr.nl_family = AF_NETLINK;
	nl_if->src_addr.nl_pid = getpid(); /* myself */
	nl_if->src_addr.nl_groups = 0;     /* unicast */

	nl_if->dest_addr.nl_family = AF_NETLINK;
	nl_if->dest_addr.nl_pid = 0;       /* to kernel */
	nl_if->dest_addr.nl_groups = 0;    /* unicast */
	
	ret = bind(nl_if->sock_fd, (struct sockaddr*)&nl_if->src_addr, sizeof(struct sockaddr_nl));

	/* Alloc transmit/receiving buffers */
	nlh_in  = (struct nlmsghdr *) malloc(NLMSG_SPACE(NL_MAX_DATALEN_ACCEPTED));
	nlh_out = (struct nlmsghdr *) malloc(NLMSG_SPACE(NL_MAX_DATALEN_ACCEPTED));	
	
	nl_if->iov_in.iov_base = (void *)nlh_in;
	nl_if->iov_in.iov_len = NLMSG_SPACE(NL_MAX_DATALEN_ACCEPTED);
	nl_if->msg_in.msg_name = (void *)&nl_if->dest_addr;
	nl_if->msg_in.msg_namelen = sizeof(nl_if->dest_addr);
	nl_if->msg_in.msg_iov = &nl_if->iov_in;
	nl_if->msg_in.msg_iovlen = 1;

	nl_if->iov_out.iov_base = (void *)nlh_out;
	nl_if->iov_out.iov_len = NLMSG_SPACE(NL_MAX_DATALEN_ACCEPTED);
	nl_if->msg_out.msg_name = (void *)&nl_if->dest_addr;
	nl_if->msg_out.msg_namelen = sizeof(nl_if->dest_addr);
	nl_if->msg_out.msg_iov = &nl_if->iov_out;
	nl_if->msg_out.msg_iovlen = 1;

	/* Init semaphores */
	sem_init(&nl_if->access_in,  0, 1);
	sem_init(&nl_if->access_out, 0, 1);

	/* Report to the module.
	 * Module needs to know the pid of the receiver.
	 */
	send_raw(nl_if, 0, 0, NL_MSG_REPORT_TO_MODULE);
		
	return ret;
}

/* Netlink interface destructor
 * Garbage clean.
 */
int nl_if_close (struct nl_interface *nl_if)
{
	sem_wait(&nl_if->access_in);
	sem_wait(&nl_if->access_out);
	
	free(nl_if->iov_in.iov_base);
	free(nl_if->iov_out.iov_base);
	close(nl_if->sock_fd);

	sem_post(&nl_if->access_in);
	sem_post(&nl_if->access_out);
	
	return 0;
}

/* The API for raw data communication, independent of GOOSE.
 * Here, we use msg_type in netlink for control information
 * marking.
 */
int send_raw(struct nl_interface *nl_if, unsigned char *data,
			 unsigned int data_len, unsigned short msg_type)
{
	int ret;

	struct nlmsghdr *nlh = (struct nlmsghdr *) nl_if->iov_out.iov_base;

	sem_wait(&nl_if->access_out);

	nlh->nlmsg_type = msg_type;
	nlh->nlmsg_len = data_len;
	nlh->nlmsg_pid = getpid();
	nlh->nlmsg_flags = 0;
	nl_if->iov_out.iov_len = NLMSG_SPACE(data_len);

	memcpy(NLMSG_DATA(nlh), data, data_len);
	ret = sendmsg(nl_if->sock_fd, &nl_if->msg_out, 0);

	sem_post(&nl_if->access_out);
	
	return ret;
}
 
/* The API for GOOSE receiving
 * Received data have the following structure according
 * to the kernel module arrangement for netlink frame:
 * ----------------------------------------------
 * | nl_data_header | type(2) | goosehdr | apdu |
 * ----------------------------------------------
 *
 * Return value is APDU length.
 */

int recv_raw(struct nl_interface *nl_if, struct nl_data_header *nl_data_h,
			 struct goosehdr *goose_h, unsigned char *apdu)
{
	unsigned char *nlh = (unsigned char *) nl_if->iov_in.iov_base;
	unsigned short apdu_len;

	sem_wait(&nl_if->access_in);
	
	/* A blocking system call */
	recvmsg(nl_if->sock_fd, &nl_if->msg_in, 0);

	/* Get data, then allocate memory to header and data spaces */
	memcpy(nl_data_h, nlh, sizeof(struct nl_data_header));

	nlh += sizeof(struct nl_data_header) + 2;
	memcpy(goose_h, nlh, sizeof(struct goosehdr));

	goose_h->len = ntohs(goose_h->len);
	goose_h->appid = ntohs(goose_h->appid);

	apdu_len = goose_h->len - sizeof(struct goosehdr);
	
	nlh += sizeof(struct goosehdr);
	memcpy(apdu, nlh, apdu_len);

	sem_post(&nl_if->access_in);

	return apdu_len;
}

/* The API for GOOSE transmission
 * An assembler to combine interface header, goose header,
 * and apdu datam, then call send_raw(...).
 * The funcation also computes the length of the goose packet,
 * and writes it to goose header.
 */
int send_goose_data(struct nl_interface *nl_if, struct nl_data_header *nl_data_h,
					struct goosehdr *goose_h, unsigned char *apdu,
					unsigned int apdu_len, unsigned short msg_type)
{
	int ret;
	struct nlmsghdr *nlh = (struct nlmsghdr *) nl_if->iov_out.iov_base;
	unsigned int nl_data_h_len = sizeof(struct nl_data_header);
	unsigned int goose_h_len = sizeof(struct goosehdr);
	unsigned int data_len = apdu_len + goose_h_len + nl_data_h_len;

	/* compute the goose pktlen in header*/
	goose_h->len = htons(apdu_len + goose_h_len);
	
	sem_wait(&nl_if->access_out);

	nlh->nlmsg_type = msg_type;
	nlh->nlmsg_len = data_len;
	nlh->nlmsg_pid = getpid();
	nlh->nlmsg_flags = 0;
	nl_if->iov_out.iov_len = NLMSG_SPACE(data_len);

	memcpy(NLMSG_DATA(nlh), nl_data_h, nl_data_h_len);
	memcpy(NLMSG_DATA(nlh) + nl_data_h_len, goose_h, goose_h_len);
	memcpy(NLMSG_DATA(nlh) + nl_data_h_len + goose_h_len,
		   apdu, apdu_len);		
	
	ret = sendmsg(nl_if->sock_fd, &nl_if->msg_out, 0);

	sem_post(&nl_if->access_out);
	
	return ret;
}

/* Well, this is an old version with lower efficiency */
int send_goose_data_old(struct nl_interface *nl_if, struct nl_data_header *nl_data_h,
						struct goosehdr *goose_h, unsigned char *apdu,
						unsigned int apdu_len, unsigned short msg_type)
{
	int ret;
	unsigned int goose_len = apdu_len + sizeof(struct goosehdr);
	unsigned int raw_data_len = goose_len + sizeof(struct nl_data_header);

	/* raw_data includes all data needed to transmit to kernel,
	 * including data interface header + goose hdr + apdu */
	unsigned char *raw_data = malloc(raw_data_len);

	/* compute the goose pktlen in header*/
	goose_h->len = htons(goose_len);

	memcpy(raw_data,
		   nl_data_h, sizeof(struct nl_data_header));

	memcpy(raw_data + sizeof(struct nl_data_header),
		   goose_h, sizeof(struct goosehdr));

	memcpy(raw_data + sizeof(struct nl_data_header) + sizeof (struct goosehdr),
		   apdu, apdu_len);

	ret = send_raw(nl_if, raw_data, raw_data_len, msg_type);
	
	free(raw_data);

	return ret;
}

/* The API for sending GOOSE transmission control information
 */

inline int send_goose_ctrl(struct nl_interface *nl_if, struct nl_ctrl_header *ctrl_info)
{
	return send_raw(nl_if, (unsigned char*) ctrl_info,
					sizeof(struct nl_ctrl_header), NL_MSG_CTRL);
	
}
