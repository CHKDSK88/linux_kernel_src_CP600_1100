
#ifndef _LINUX_IF_ADSLCTL_H_
#define _LINUX_IF_ADSLCTL_H_

#ifdef __KERNEL__

#include <linux/netdevice.h>

/* found in af_inet.c */
extern int (*adsl_ctl_ioctl_hook)(unsigned long arg);

extern void adslctl_handle_frame(struct sk_buff *skb);


struct adslctl_dev_info {
	struct net_device *real_dev;        /* the underlying device/interface */
	struct net_device_stats dev_stats;  /* Device stats (rx-bytes, tx-pkts, etc...) */
};

#define ADSLCTL_DEV_INFO(x) ((struct adslctl_dev_info *)(x->priv))

/* inline functions */

static inline struct net_device_stats *adslctl_dev_get_stats(struct net_device *dev)
{
	return &(ADSLCTL_DEV_INFO(dev)->dev_stats);
}
#endif /* __KERNEL__ */

#define ADSLCTL_NAME "adslctl"
// limor - test without special handing
//#define ADSLCTL_NAME "eth1"
//#define ADSLCTL_NAME "eth1:1"
//#define ADSLCTL_NAME "limor"
/* ADSLCTL IOCTLs are found in sockios.h */

/* Passed in adslctl_ioctl_args structure to determine behaviour. */
enum adslctl_ioctl_cmds {
	ADD_ADSLCTL_CMD,
	DEL_ADSLCTL_CMD,
};

struct adslctl_ioctl_args {
	int cmd;			/* Should be one of the adslctl_ioctl_cmds enum above. */
	char device[24];
};

#endif /* !(_LINUX_IF_ADSLCTL_H_) */

