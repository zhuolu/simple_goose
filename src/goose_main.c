
/*
 * Name        : goose_main.c
 * Description : GOOSE kernel module
 * File        : The main module
 * Dev. Plat.  : kernel version 2.6.31-17, gcc version 4.4.1
 *
 */

/* Modified by Timothy Middelkoop <timothy.middelkoop@gmail.com> */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/skbuff.h>
#include <linux/kthread.h>

#include <net/sock.h>
#include <net/netlink.h>

#include "proto_goose.h"
#include "goose_module.h"

/*#define _GOOSE_DEBUG_*/

#ifdef _GOOSE_DEBUG_
#define DEBUG_print(arg, ...) printk ("*GDEBUG: " arg, ## __VA_ARGS__)
#else
#define DEBUG_print(arg, ...)
#endif

/* Module Info */
MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Zhuo Lu, NetWis, NCSU.");
MODULE_DESCRIPTION("GOOSE messaging module");
MODULE_VERSION("0.3.1");

/* Module Parameters */
static short int tran_active = 1;
module_param(tran_active, short, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(tran_active, "GOOSE transmission control: 0 - no transmission");

static short int recv_active = 1;
module_param(recv_active, short, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(recv_active, "GOOSE receiving control: 0 - no receiving");

static unsigned int num_pkt_trans = 0;
module_param(num_pkt_trans, int, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(num_pkt_trans, "Number of GOOSE messages transmitted by Enhanced Transmission");

/* proc file systems */
static struct proc_dir_entry *proc_dir, /* dir */
	*proc_def_dev, *proc_tran_intvl, *proc_delay_thre,
	*proc_retran_intvl, *proc_retran_incre, *proc_max_retran_intvl; /* files */

/* Assign default values to proc files */
static char buf_proc_def_dev [PROC_DEF_DEV_BUFLEN] = DEFBUF_PROC_DEF_DEV;

static unsigned int tran_intvl       = DEF_TRAN_INTVL;       /*  ms  */
static unsigned int delay_thre       = DEF_DELAY_THRE;       /*  ms  */
static unsigned int retran_intvl     = DEF_RETRAN_INTVL;     /*  ms  */
static unsigned int retran_incre     = DEF_RETRAN_INCRE;     /*  ms  */
static unsigned int max_retran_intvl = DEF_MAX_RETRAN_INTVL; /*  ms  */

/* Netlink socket */
static struct sock *nl_sk = NULL;

/* Netlink user pid */
static u32 user_pid = 0;

/* Task pointer to server daemon thread */
static struct task_struct *dmn_task = NULL;

/* Default NIC to transmit */
static struct net_device * def_dev = NULL;

/* GOOSE kernel API */
static int goose_rcv(struct sk_buff *skb, struct net_device *dev,
					 struct packet_type *pt, struct net_device *orin_dev);
static int goose_trans_skb(struct net_device *dev, unsigned char *daddr,
						   struct sk_buff *__skb, int reliablity);
static int goose_enhan_retrans(struct sk_buff *__skb);

/* Define the GOOSE protocol */
static struct packet_type goose_packet_type = {
	.type = ntohs(ETH_P_GOOSE),
	.dev = NULL,
	.func = goose_rcv
};

/************************************************************
 * proc_fs io functions: read and write
 ************************************************************/
#define FS_FUN_READ(fun_name, var_name) static int fun_name(char *page, char **start, off_t off, int count, int *eof, void *data) \
	{																	\
		return sprintf(page, "%u\n",var_name);							\
	}																	\


#define FS_FUN_WRITE(fun_name, max_len, var_name) static ssize_t fun_name(struct file *filp, const char __user *buff, unsigned long len, void *data) \
	{																	\
		char temp_buf[max_len];											\
		if (len > max_len -1)											\
			return -1;													\
		if (copy_from_user(temp_buf, buff, len)>0)						\
			return -1;													\
		sscanf(temp_buf, "%u", &var_name);								\
		return len;														\
	}																	\

FS_FUN_READ(read_tran_intvl, tran_intvl)
FS_FUN_READ(read_retran_intvl, retran_intvl)
FS_FUN_READ(read_retran_incre, retran_incre)
FS_FUN_READ(read_max_retran_intvl, max_retran_intvl)
FS_FUN_READ(read_delay_thre, delay_thre)
FS_FUN_WRITE(write_tran_intvl, PROC_TRAN_INTVL_BUFLEN, tran_intvl)
FS_FUN_WRITE(write_retran_intvl, PROC_RETRAN_INTVL_BUFLEN, retran_intvl)
FS_FUN_WRITE(write_retran_incre, PROC_RETRAN_INCRE_BUFLEN, retran_incre)
FS_FUN_WRITE(write_max_retran_intvl, PROC_MAX_RETRAN_INTVL_BUFLEN, max_retran_intvl)
FS_FUN_WRITE(write_delay_thre, PROC_DELAY_THRE_BUFLEN, delay_thre)


static int read_def_dev(char *page, char **start, off_t off, int count, int *eof, void *data) 
{
	return sprintf(page, "%s\n",buf_proc_def_dev);
}

static ssize_t write_def_dev(struct file *filp, const char __user *buff, unsigned long len, void *data)
{
	if (len > PROC_DEF_DEV_BUFLEN -1)
		return -1;
	if (copy_from_user(buf_proc_def_dev, buff, len)>0)
		return -1;

	sscanf(buf_proc_def_dev, "%s", buf_proc_def_dev);
		
	def_dev = dev_get_by_name(&init_net, buf_proc_def_dev);
	
	if (def_dev == NULL)
		printk("GOOSE: Can not find %s, choose another device.\n", buf_proc_def_dev);
	else
		printk("GOOSE: Change default network device to %s.\n", buf_proc_def_dev);		
	
	return len;
}


/************************************************************
 * proc_fs init: creating and registering
 ************************************************************/
static int proc_fs_init(void)
{
	/* Create /proc/kcache/ ... */
	proc_dir = proc_mkdir(PROC_DNAME, NULL);
	proc_def_dev  = create_proc_entry(PROC_FNAME_DEF_DEV , 0644, proc_dir);
	proc_tran_intvl = create_proc_entry(PROC_FNAME_TRAN_INTVL, 0644, proc_dir);	
	proc_delay_thre = create_proc_entry(PROC_FNAME_DELAY_THRE, 0644, proc_dir);
	proc_retran_intvl = create_proc_entry(PROC_FNAME_RETRAN_INTVL, 0644, proc_dir);
	proc_retran_incre = create_proc_entry(PROC_FNAME_RETRAN_INCRE, 0644, proc_dir);
	proc_max_retran_intvl = create_proc_entry(PROC_FNAME_MAX_RETRAN_INTVL, 0644, proc_dir);
	
	if ((proc_dir == NULL) || (proc_def_dev  == NULL) ||
		(proc_tran_intvl  == NULL) || (proc_delay_thre == NULL) ||
		(proc_retran_intvl == NULL) || (proc_retran_incre == NULL) ||
		(proc_max_retran_intvl == NULL))
		return -1;

	/* read/write interface for transmission interval */
	proc_tran_intvl->read_proc  =  read_tran_intvl;
	proc_tran_intvl->write_proc = write_tran_intvl;

	/* read/write interface for retransmission interval */
	proc_retran_intvl->read_proc  =  read_retran_intvl;
	proc_retran_intvl->write_proc = write_retran_intvl;

	/* read/write interface for retransmission interval increment*/
	proc_retran_incre->read_proc  =  read_retran_incre;
	proc_retran_incre->write_proc = write_retran_incre;

	/* read/write interface for maximum retransmission interval */
	proc_max_retran_intvl->read_proc  =  read_max_retran_intvl;
	proc_max_retran_intvl->write_proc = write_max_retran_intvl;
	
	/* read/write interface for delay threshold */
	proc_delay_thre->read_proc  =  read_delay_thre;
	proc_delay_thre->write_proc = write_delay_thre;

	/* read/write interface for default network interface */
	proc_def_dev->read_proc  =  read_def_dev;
	proc_def_dev->write_proc = write_def_dev;	

	return 0;
}


/************************************************************
 * Netline interface I/O
 ************************************************************/

/*
 * Read data from user space, then pass data to goose_tran
 */

static void nl_goose_read_from_user (struct sk_buff *__skb)
{
	struct sk_buff *skb;
	struct nlmsghdr *nlh;
	struct nl_ctrl_header *nl_ctrl_h;
	struct nl_data_header *nl_data_h;
	struct net_device *trans_dev = NULL;
	unsigned char *data;
	unsigned int data_len;	
	
	skb = skb_get(__skb);

	/* Sanity check */
	if (unlikely(skb->len < NLMSG_SPACE(0)))
		return;

	nlh = nlmsg_hdr(skb);

	/* Message is reporting pid ? */
	if (unlikely(nlh->nlmsg_type == NL_MSG_REPORT_TO_MODULE)) {
		user_pid = nlh->nlmsg_pid;
		printk("GOOSE: registered user_pid = %d \n", user_pid);
		goto read_from_user_return;
	}

	/* Message is ctrl-type? */
	if (unlikely(nlh->nlmsg_type & NL_MSG_CTRL)) {
		nl_ctrl_h = (struct nl_ctrl_header *) (NLMSG_DATA(nlh));
		
		/* Set default device */
		if (nl_ctrl_h->def_dev[0] != 0) {
			def_dev = dev_get_by_name(&init_net, nl_ctrl_h->def_dev);			
			if (unlikely(def_dev == NULL))
				printk("GOOSE: Can not find %s, choose another device.\n",  nl_ctrl_h->def_dev);
			else
				printk("GOOSE: Change default network device to %s.\n",  nl_ctrl_h->def_dev);		   
		}

		/* Set GOOSE parameters */
		delay_thre       = nl_ctrl_h->goose_param.thresh;
		retran_intvl     = nl_ctrl_h->goose_param.intvl_init;
		max_retran_intvl = nl_ctrl_h->goose_param.intvl_max;
		retran_incre     = nl_ctrl_h->goose_param.intvl_incre;
		
		goto read_from_user_return;
	}
	
	/* Then, message should be data */
	nl_data_h = (struct nl_data_header *) NLMSG_DATA(nlh);
	data = (unsigned char *) (NLMSG_DATA(nlh) + sizeof(struct nl_data_header));
	data_len = nlh->nlmsg_len - sizeof(struct nl_data_header);

	/* Obtain transmission device */
	trans_dev = (nl_data_h->dev_name[0] != 0) ?
		dev_get_by_name(&init_net, nl_data_h->dev_name)
		: def_dev;
	
	if (unlikely(trans_dev == NULL))
		goto read_from_user_return;

	/* Should message be broadcasted ? */
	if (unlikely(nlh->nlmsg_type & NL_MSG_DATA_BRDCAST)) {		
		goose_trans_skb(trans_dev, trans_dev->broadcast, skb,
						((nlh->nlmsg_type & NL_MSG_DATA_RELB) != 0));
		goto read_from_user_exit; 
	} 

	/* Then, message should be unicasted */
	goose_trans_skb(trans_dev, nl_data_h->daddr, skb,
					((nlh->nlmsg_type & NL_MSG_DATA_RELB) != 0));
	goto read_from_user_exit;
	
read_from_user_return:	
	kfree_skb(skb);
	
read_from_user_exit:
	return;
}


/*
 * Send data to user space
 */

static inline int nl_goose_send_to_user (struct sk_buff *skb)
{
	netlink_unicast(nl_sk, skb, user_pid, MSG_DONTWAIT);
	return 0;
}

/* Netline interface: creating and registering */
static int netlink_init(void)
{
	nl_sk = netlink_kernel_create(&init_net, NETLINK_GOOSE,
								  0, nl_goose_read_from_user, NULL, THIS_MODULE);
	
	if (!nl_sk) {
		if (nl_sk != NULL)
    		sock_release(nl_sk->sk_socket);
		return -1;
	}

	return 0;

}

/************************************************************
 * GOOSE protocol
 ************************************************************/

/* GOOSE packet handler - recevie */
int goose_rcv(struct sk_buff *skb, struct net_device *dev,
			  struct packet_type *pt, struct net_device *orin_dev)
{
	int ret = -1;
		
	if (unlikely(!recv_active))
		goto goose_rcv_end;

	/* We use existing skb to form a new one:
	 *
	 * skb:
	 * new skb->data                                   old skb->data
	 *   |                                                  |
	 * -----------------------------------------------------------
	 *   | IFNAMESIZ  bytes | daddr(6) | saddr(6) | type(2) | data
	 * -----------------------------------------------------------
	 *   |<-- can form a struct nl_data_header -->|
	 * 
	 */
	skb_push(skb, 14 + IFNAMSIZ);
	
	/* Then, we write the dev_name to nl_data_header->dev_name */
	strcpy(skb->data, dev->name);

	/* Transmit skb to user space */
	ret = nl_goose_send_to_user(skb);	

goose_rcv_end:
	if (unlikely(ret != 0))
		kfree_skb(skb);
	
	return 0;
}

/* GOOSE Enhanced retransmission mechanism.
 */

int goose_enhan_retrans(struct sk_buff *__skb)
{
	struct sk_buff *skb_cp, *skb = __skb;
	unsigned int waiting_time = retran_intvl; /* ms */
	unsigned int total_waiting_time = 0; /* ms */
	unsigned trans_count = 0;
	int ret;

goose_enhan_retrans_redo:
	
	/* Make a skb copy for retransmission */
	skb_cp = skb_copy(skb, GFP_ATOMIC);	
	ret = dev_queue_xmit(skb);
	skb = skb_cp;
	
	if (unlikely(ret != 0))
		goto goose_enhan_retrans_exit;		

    /* Compute the overall delay */
	total_waiting_time += waiting_time;
	waiting_time += retran_incre;
	if (waiting_time > max_retran_intvl)
		waiting_time = max_retran_intvl;

    /* Sleep for a while */
	msleep_interruptible (waiting_time);	

    /* It is necessary to set an upper limit for number of retransmissions */
	if (unlikely((total_waiting_time < delay_thre) && (trans_count++ < MAX_GOOSE_TRANS_NUM)))
		goto goose_enhan_retrans_redo;
		
    /* Retransmission finishes.
	 * Increase the number of packets transmitted by Enhanced Transmission */
	num_pkt_trans ++;
	
goose_enhan_retrans_exit:
	kfree_skb(skb);
	return ret;
}

/* GOOSE transmission function.
 * In order to provide more efficiency, we manipulate the netlink skb
 * to form the new skb to transmit.
 */

int goose_trans_skb(struct net_device *dev, unsigned char *daddr,
					struct sk_buff *__skb, int reliablity)
{
	struct sk_buff *skb = __skb;
	unsigned int skb_pull_len = NLMSG_LENGTH(0) + sizeof(struct nl_data_header);

	if (unlikely((dev == NULL) || (!tran_active)))
		goto goose_trans_skb_fail;
	
	/* We use existing skb to form a new one:
	 *
	 * skb:
	 * old skb->data                      new skb->data
	 *   |                                  |
	 * -----------------------------------------------------------
	 *   | NLMSG_LENGTH(0) | nl_data_header | goose header | APDU
	 * -----------------------------------------------------------
	 *   | <==      skb_pull_len        ==> |
	 * 
	 */		
	skb_pull(skb, skb_pull_len);
	skb_reset_network_header(skb);

	/* But after pulling, if we have still no enough space for link-layer header,
	   we have to reconstruct a new skb */
	if ((skb->head - skb->network_header) < LL_RESERVED_SPACE(dev)) {
		skb = skb_copy_expand(__skb, LL_RESERVED_SPACE(dev), 16, GFP_ATOMIC);
		kfree_skb(__skb);
	}

	/* Specify protocol type and frame information */
	skb->dev = dev;
	skb->protocol = ETH_P_GOOSE;
	skb->pkt_type = PACKET_OUTGOING;
	skb->csum = 0;
	skb->ip_summed = 0;

	/* Set the highest priority */
	skb->priority = 0;
	
	if (unlikely(dev_hard_header(skb, dev, ETH_P_GOOSE, daddr, dev->dev_addr, skb->len) < 0))
		goto goose_trans_skb_fail;

	/* If the message should be transmitted by GOOSE Enhanced Retransmission Mechanism,
	   call goose_enhan_retrans, otherwise transmit it directly.*/
	return reliablity ? goose_enhan_retrans(skb):dev_queue_xmit(skb);
	
goose_trans_skb_fail:
	kfree_skb(skb);
	return -1;
}

/************************************************************
 * GOOSE daemon program.
 ************************************************************/
static int daemon(void* arg)
{	
	dmn_task=current;
	daemonize("GOOSE daemon");
	
	printk("GOOSE: daemon is up.\n");
		
	while (1) {

		/* TODO: add extention here */

		/* Sleep for a while*/
		msleep_interruptible(DAEMON_LOOP_IDLE_TIME);
		
		/* If it recevies a signal, then exit */
		if (unlikely(signal_pending(current)))
			break;
	}


	printk("GOOSE: daemon is down.\n");
	
	dmn_task = NULL;
	return 0;
}

/************************************************************
 * Module init procedure.
 ************************************************************/
static int __init goose_init(void)
{	
	printk("--------------------------------------\n");
	printk("GOOSE: Stand by.\n");

	printk("GOOSE: initiating proc file systems.\n");
	if (proc_fs_init() != 0) {
		printk("GOOSE: Fatal error in initializing proc_fs!\n");
		return -1;
	}
	
	printk("GOOSE: initiating netlink interface.\n");
	if (netlink_init() != 0) {
		printk("GOOSE: Fatal error in initializing netlink!\n");
		return -1;
	}

	/* initialize default dev*/
	def_dev = dev_get_by_name(&init_net, buf_proc_def_dev);
	
	if (def_dev == NULL) {
		printk("GOOSE: Can not find %s, choose another device.\n", buf_proc_def_dev);
	}

	/* register GOOSE protocol */
	dev_add_pack(&goose_packet_type);
	
	/* kernel_thread(daemon, NULL, 0); */

	return 0;
}

/************************************************************
 * Module exiting procedure.
 ************************************************************/
static void __exit goose_exit(void)
{
	tran_active = 0;
	recv_active = 0;
	
	/* Unregister GOOSE protocol */
	dev_remove_pack(&goose_packet_type);
	
	/* Delete all proc_fs */
	if (proc_def_dev  != NULL)
		remove_proc_entry(PROC_FNAME_DEF_DEV,  proc_dir);
	if (proc_tran_intvl != NULL)
		remove_proc_entry(PROC_FNAME_TRAN_INTVL, proc_dir);
	if (proc_delay_thre != NULL)
		remove_proc_entry(PROC_FNAME_DELAY_THRE, proc_dir);
	if (proc_retran_intvl != NULL)
		remove_proc_entry(PROC_FNAME_RETRAN_INTVL, proc_dir);
	if (proc_retran_incre != NULL)
		remove_proc_entry(PROC_FNAME_RETRAN_INCRE, proc_dir);
	if (proc_max_retran_intvl != NULL)
		remove_proc_entry(PROC_FNAME_MAX_RETRAN_INTVL, proc_dir);	
	if (proc_dir != NULL)
		remove_proc_entry(PROC_DNAME, NULL);

	if (nl_sk != NULL)
    	sock_release(nl_sk->sk_socket);
		
	if (dmn_task != NULL)
		send_sig_info(SIGTERM, (struct siginfo *)1, dmn_task);
	
	printk("GOOSE: the module is unloaded.\n");
}

module_init(goose_init);
module_exit(goose_exit);
