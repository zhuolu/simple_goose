#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "nl_if_goose.h"

/* Subtract the `struct timeval' values X and Y,
   storing the result in *result.
   Return 1 if the difference is negative, otherwise 0.  */     
static int timeval_subtract (struct timeval *result,
							 struct timeval *x, struct timeval *y)
{
	/* Perform the carry for the later subtraction by updating y. */
	if (x->tv_usec < y->tv_usec) {
		int nsec = (y->tv_usec - x->tv_usec) / 1000000 + 1;
		y->tv_usec -= 1000000 * nsec;
		y->tv_sec += nsec;
	}
	if (x->tv_usec - y->tv_usec > 1000000) {
		int nsec = (x->tv_usec - y->tv_usec) / 1000000;
		y->tv_usec += 1000000 * nsec;
		y->tv_sec -= nsec;
	}
     
	/* Compute the time remaining to wait.
	   tv_usec is certainly positive. */
	result->tv_sec = x->tv_sec - y->tv_sec;
	result->tv_usec = x->tv_usec - y->tv_usec;
     
	/* Return 1 if result is negative. */
	return x->tv_sec < y->tv_sec;
}


int main(int argc, char* argv[])
{
	struct nl_interface nl_if;
	const unsigned short my_appid = 18;
	unsigned char apdu [NL_MAX_DATALEN_ACCEPTED];
	struct timeval start_time, end_time, diff_time;
	
	/* Set testing control information */
	struct nl_ctrl_header nl_ctrl_h = {
		.def_dev = "ath0",
		.goose_param = {
			.thresh       = 10,
			.intvl_init   = 1,
			.intvl_max    = 1,
			.intvl_incre  = 1
		}
	};
		
	/* Set testing headers */
	struct nl_data_header nl_data_h = {
		.dev_name = "ath0", /* Use default */
		.daddr= {0x00, 0x1b, 0x2f, 0xe1, 0x95, 0xa8} // wlan1 in aspen
//		.daddr= {0x00, 0x0d, 0x56, 0xb6, 0xb4, 0xee} // eth0 in apsen
	};
	
	struct goosehdr goose_h = {
		.appid = htons(my_appid),
		.reserv1 = 1,
		.reserv2 = 2,
		.reserv3 = 3,
		.reserv4 = 4
	};

	/* APDU */
	unsigned char data[] = "This is a simple testing GOOSE message!";

	/* Initiate netlink interface */
	if (nl_if_init(&nl_if)!=0) {
		printf("Initiating netlink interface fails!\n");
		return EXIT_FAILURE;
	}

	gettimeofday(&start_time, NULL);
	
	/* Transmit goose control information */
	send_goose_ctrl(&nl_if, &nl_ctrl_h);

	/* Transmit APDU */
	send_goose_data(&nl_if, &nl_data_h, &goose_h, data, strlen((char*)data), NL_MSG_DATA_UNICAST);   

	/* Receive response */
	recv_raw(&nl_if, &nl_data_h, &goose_h, apdu);
	
	gettimeofday (&end_time, NULL);
	timeval_subtract(&diff_time, &end_time, &start_time);
	
	printf("Round Trip Time = %ld us\n", diff_time.tv_usec);

	/* Close netlink interface */
	nl_if_close(&nl_if);
	return EXIT_SUCCESS;
}
