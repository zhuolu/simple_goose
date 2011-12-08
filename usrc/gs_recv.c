#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

#include "nl_if_goose.h"

int main(int argc, char* argv[])
{
	struct nl_interface nl_if;
	struct nl_data_header nl_data_h;
	struct goosehdr goose_h;
	unsigned char apdu[NL_MAX_DATALEN_ACCEPTED];
	int apdu_len = 0;

	/* Initiate netlink interface */
	if (nl_if_init(&nl_if)!=0) {
		printf("Initiating netlink interface fails!\n");
		return EXIT_FAILURE;
	}

	while (1) {
		int i;
		
		/* Receive GOOSE message */
		apdu_len = recv_raw(&nl_if, &nl_data_h, &goose_h, apdu);

		/* Report goose packet info */
		printf("GOOSE packet info:\n");
		printf("Device: %s, Direction: "
			   "%x:%x:%x:%x:%x:%x ==> %x:%x:%x:%x:%x:%x \n",
			   nl_data_h.dev_name,
			   nl_data_h.saddr[0],nl_data_h.saddr[1],
			   nl_data_h.saddr[2],nl_data_h.saddr[3],
			   nl_data_h.saddr[4],nl_data_h.saddr[5],
			   nl_data_h.daddr[0],nl_data_h.daddr[1],
			   nl_data_h.daddr[2],nl_data_h.daddr[3],
			   nl_data_h.daddr[4],nl_data_h.daddr[5]);
		printf("Header info: "
			   "appid = %d, len = %d\n",
			   goose_h.appid, goose_h.len);
		
		printf("APDU: ");
		for (i = 0; i < apdu_len; i++)
			printf("%c", apdu[i]);
		printf("\n");
			
		/* Reply the same message */
		memcpy(nl_data_h.daddr, nl_data_h.saddr, 6);
		send_goose_data(&nl_if, &nl_data_h, &goose_h, apdu, apdu_len, NL_MSG_DATA_UNICAST);
	}

	/* Close netlink interface */
	nl_if_close(&nl_if);
	return EXIT_SUCCESS;
}
