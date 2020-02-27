#include <asm/uaccess.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/netdevice.h> 
#include <linux/inetdevice.h> 
#include <linux/ip.h> 
#include <linux/if_arp.h> 
#include <linux/if_adslctl.h> 
#include <net/rtnetlink.h>
#include <linux/sofaware_led.h>
//#include "cp_ledctl.h"


//#define DEBUG_ADSLCTL
#ifdef DEBUG_ADSLCTL
#define ADSLCTL_DEBUG(fmt, arg...) printk("%s:%d --> " fmt, __FUNCTION__, __LINE__, ## arg)
#else
#define ADSLCTL_DEBUG(fmt, arg...) ; 
#endif

//#define DEBUG_ADSLCTL_PKT
#ifdef DEBUG_ADSLCTL_PKT
#define ADSLCTL_DEBUG_PKT(fmt, arg...) printk("%s:%d --> " fmt, __FUNCTION__, __LINE__, ## arg)
#else
#define ADSLCTL_DEBUG_PKT(fmt, arg...) ;
#endif
	
static int adslctl_device_event(struct notifier_block *, unsigned long, void *);

struct notifier_block adslctl_notifier_block = {
	notifier_call: adslctl_device_event,
};

/*
 * adslctl_open
 */
static int adslctl_open(struct net_device *dev)
{
	ADSLCTL_DEBUG("%s interface up, dev 0x%08x\n", dev->name, dev);
	netif_start_queue (dev);
	//limor - for now put this here, remove when I find where it SHOULD be called
	netif_start_queue (ADSLCTL_DEV_INFO(dev)->real_dev);
	return 0;
}

/*
 * adslctl_stop
 */
static int adslctl_stop(struct net_device *dev)
{
	ADSLCTL_DEBUG("%s interface down, dev 0x%08x\n", dev->name, dev);
	netif_stop_queue(dev);
	return 0;
}

/*
 * adslctl_init
 */
static void adslctl_setup(struct net_device *dev)
{
	ADSLCTL_DEBUG("setup dev 0x%08x \n", dev);
	return;
}

/*
 * adslctl_init
 */
static int adslctl_init(struct net_device *dev)
{
	ADSLCTL_DEBUG("init dev 0x%08x \n", dev);
	return 0;
}
/*
 * adslctl_dev_destruct
 */
void adslctl_dev_destruct(struct net_device *dev)
{
	ADSLCTL_DEBUG("destruct dev 0x%08x \n", dev);

	if (dev->priv) {
		kfree(dev->priv);
		dev->priv = NULL;
	}
}

/*
 * adslctl_dev_hard_start_xmit
 */
static int adslctl_dev_hard_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct net_device_stats *stats;
	
	stats = adslctl_dev_get_stats(dev);
	stats->tx_packets++;
	stats->tx_bytes += skb->len;
	
	skb->dev = ADSLCTL_DEV_INFO(dev)->real_dev;
// limor - no security flag in SG80 !!!
//	/* mark the packet as filtered */
//	skb->security = 1; /* skb->pkt_bridged = SW_PKT_WAS_IN_FW */
//
// too many prints for now - restore later.
	ADSLCTL_DEBUG_PKT("dev 0x%08x, dev->name %s, skb 0x%08x, skb->len %d \n", 
			skb->dev, skb->dev->name, skb, skb->len);

	skb->dev->hard_start_xmit(skb, skb->dev);

	return 0;
}

/*
 * is_ip_in_adslctl_net
 */
static inline int is_ip_in_adslctl_net(u32 ip)
{
	return ( 0xA9FEFD00==(ip&0xfffffffc));
}

/*
 * is_adslctl_frame
 */
static int is_adslctl_frame(struct sk_buff *skb)
{

	ADSLCTL_DEBUG_PKT("is_adslctl_frame (%d) - protocol is %d\n", 
			__LINE__, eth_hdr(skb)->h_proto);

	/* proto is not IP or ARP, do nothing */
	switch(htons(eth_hdr(skb)->h_proto))
	{
		case ETH_P_IP:
			{
				struct iphdr *iph = ip_hdr(skb);

				if (unlikely(is_ip_in_adslctl_net(htonl(iph->daddr))) ) {
					ADSLCTL_DEBUG_PKT("IP packet accepted - daddr 0x%08x \n", iph->daddr);
					return 1;
				}
				else
					ADSLCTL_DEBUG_PKT("is_adslctl_frame (%d) - daddr 0x%08x \n", __LINE__, iph->daddr);

			}
			break;
		case ETH_P_ARP:
			{
				struct arphdr *arph = arp_hdr(skb);
				unsigned char *arpptr = (unsigned char *)(arph+1);
				u32 sip,dip;

				ADSLCTL_DEBUG_PKT("ARP packet - arph->ar_hrd %d arph->ar_pro %d\n",
					arph->ar_hrd, arph->ar_pro);

				/* Test only Ethernet/IP ARP packrts */
				if (arph->ar_hrd==htons(ARPHRD_ETHER) && arph->ar_pro==htons(ETH_P_IP)) {

					ADSLCTL_DEBUG_PKT("ARP packet - arph->ar_op %d \n",	arph->ar_op);

					/* Test only these message types */
					if (arph->ar_op == htons(ARPOP_REPLY) || arph->ar_op == htons(ARPOP_REQUEST)) {

						/*
						 *  Extract IP fields
						 */
						arpptr += skb->dev->addr_len;
						memcpy(&sip, arpptr, 4);
						arpptr += 4;
						arpptr += skb->dev->addr_len;
						memcpy(&dip, arpptr, 4);

						if ( unlikely(is_ip_in_adslctl_net(htonl(dip))) ) {				
							ADSLCTL_DEBUG_PKT("ARP packet accepted - saddr 0x%08x, daddr 0x%08x \n", 
									sip, dip);
							return 1;
						}
						else
							ADSLCTL_DEBUG_PKT("ARP packet NOT accepted - saddr 0x%08x, daddr 0x%08x \n", 
									sip, dip);
							
					}
				}
			}
			break;
	}

	return 0;
}

/*
 * adslctl_handle_frame
 */
void adslctl_handle_frame(struct sk_buff *skb)
{
	ADSLCTL_DEBUG_PKT("dev 0x%08x, skb 0x%08x, skb->len %d \n", 
			skb->dev, skb, skb->len);


	if (is_adslctl_frame(skb)) {
		struct net_device_stats *stats;
		
		if ((skb->dev = __dev_get_by_name(ADSLCTL_NAME))==NULL)
			return;

		ADSLCTL_DEBUG_PKT("adslctl_handle_frame (%d) this is an adsl frame!\n", __LINE__);

		stats = adslctl_dev_get_stats(skb->dev);

		stats->rx_packets++;
		stats->rx_bytes += skb->len;
	}
// limor - no leds yet
	else {
		
	ADSLCTL_DEBUG_PKT("adslctl_handle_frame (%d) this is NOT an adsl frame!\n", __LINE__);
	//	set_led_state(SW_DSL_ACT_LED, SW_LED_BLINK_ONCE_SHORT);
		//ledMod_set_led_state(49, 1, ~(0x0)); /* Turn on red power LED */
	  	//SET_LED_STATE( LEDMOD_ADSL_ACT, LEDMOD_CMD_ON, 0);
	}

	return;
}

/*
 * register_adsl_ctl_device
 */
static struct net_device *register_adsl_ctl_device(const char *eth_name)
{
	struct net_device *new_adslctl_dev;
	struct net_device *real_dev;    /* the ethernet device */

	/* find the device relating to eth_name. */
	real_dev = dev_get_by_name(eth_name);
	if (real_dev == NULL) {
		printk ("%s: %s does not exist\n", __FUNCTION__, eth_name);
		goto out_ret_null;
	}
	/* From this point on, all the data structures must remain
	 * consistent.
	 */
	rtnl_lock();

	if (__dev_get_by_name(ADSLCTL_NAME)) {
		/* was already registered */
		printk("%s: ALREADY had ADSL ctl device registered\n", __FUNCTION__);
		goto out_unlock;
	}
	
	/* The real device must be up and operating in order to
	 * assosciate a ADSL device with it.
	 */
	if (!(real_dev->flags & IFF_UP)) {
		printk ("%s: %s is down\n", __FUNCTION__, eth_name);
		goto out_unlock;
	}

	new_adslctl_dev = (struct net_device *) alloc_netdev(sizeof(struct adslctl_dev_info), ADSLCTL_NAME, adslctl_setup);

	if (new_adslctl_dev == NULL)
		goto out_unlock;

	/* Set us up to have no queue, as the underlying Hardware device
	 * can do all the queueing we could want.
	 */

	new_adslctl_dev->tx_queue_len = 0;

	/* Gotta set up the fields for the device. */

	/* set up method calls */
	new_adslctl_dev->init = adslctl_init;

	new_adslctl_dev->destructor = adslctl_dev_destruct;
	//new_adslctl_dev->features |= NETIF_F_DYNALLOC;

	new_adslctl_dev->get_stats = adslctl_dev_get_stats;

	/* IFF_BROADCAST|IFF_MULTICAST; ??? */
	new_adslctl_dev->flags = real_dev->flags & (~IFF_UP);
	new_adslctl_dev->mtu = real_dev->mtu;

	/* TODO: maybe just assign it to be ETHERNET? */
	new_adslctl_dev->type = real_dev->type;

	if (new_adslctl_dev->priv == NULL)
		goto out_free_newdev;

	ADSLCTL_DEV_INFO(new_adslctl_dev)->real_dev = real_dev;

	new_adslctl_dev->hard_header_len = real_dev->hard_header_len;

	memcpy(new_adslctl_dev->broadcast, real_dev->broadcast, real_dev->addr_len);
	memcpy(new_adslctl_dev->dev_addr, real_dev->dev_addr, real_dev->addr_len);
	new_adslctl_dev->addr_len = real_dev->addr_len;

	new_adslctl_dev->hard_start_xmit = adslctl_dev_hard_start_xmit;
	new_adslctl_dev->open = adslctl_open;
	new_adslctl_dev->stop = adslctl_stop;
	// limor - no header op in SG80
	//new_adslctl_dev->header_ops = real_dev->header_ops;
	new_adslctl_dev->hard_header = real_dev->hard_header;
	new_adslctl_dev->hard_header_parse = real_dev->hard_header_parse;
	new_adslctl_dev->hard_header_cache = real_dev->hard_header_cache;

	ADSLCTL_DEBUG("real_dev->hard_header_len %d real_dev->hard_header %p\n", real_dev->hard_header_len, real_dev->hard_header);

	/* Calling to real device callbacks with virtual adsl device as a parameter
	   is wrong and caused a memory corruption. Setting to NULL.
	*/
	new_adslctl_dev->set_mac_address = NULL; //real_dev->set_mac_address;
	new_adslctl_dev->set_multicast_list = NULL; // real_dev->set_multicast_list;

	real_dev->priv_flags |= IFF_ADSLCTL_PHY_DEV;

	register_netdevice(new_adslctl_dev);
	
	rtnl_unlock();

	ADSLCTL_DEBUG("dev 0x%08x, real dev 0x%08x (%s)\n", new_adslctl_dev, real_dev, real_dev->name);

	/* NOTE:  We have a reference to the real device,
	 * so hold on to the reference.
	 */
	//MOD_INC_USE_COUNT; /* Add was a success!! */

	return new_adslctl_dev;

out_free_newdev:
	kfree(new_adslctl_dev);

out_unlock:
	dev_put(real_dev);
	rtnl_unlock();

out_ret_null:
	return NULL;
}

/*
 * unregister_adsl_ctl_device
 */
static int unregister_adsl_ctl_device(const char *eth_name)
{
	int ret = 0;
	struct net_device *adslctl_dev;

	rtnl_lock();
	
	adslctl_dev = __dev_get_by_name(ADSLCTL_NAME);
	
	if (adslctl_dev != NULL) {
	 	struct net_device *real_dev = ADSLCTL_DEV_INFO(adslctl_dev)->real_dev;

		ADSLCTL_DEBUG("dev 0x%08x, real dev 0x%08x \n", adslctl_dev, real_dev);

		real_dev->priv_flags &= (~IFF_ADSLCTL_PHY_DEV);
		
		unregister_netdevice(adslctl_dev);
		dev_put(real_dev);
		
		//MOD_DEC_USE_COUNT; /* Rem was a success!! */

	} else {
		printk("WARNING: Could not find ADSL control dev.\n");
		ret = -EINVAL;
	}

	rtnl_unlock();
	return ret;
}

/*
 * adslctl_device_event 
 */
static int adslctl_device_event(struct notifier_block *unused, unsigned long event, void *ptr)
{
	struct net_device *dev = (struct net_device *)(ptr);
	struct net_device *adslctl_dev = __dev_get_by_name(ADSLCTL_NAME);
	int flgs;

	if (!adslctl_dev || ADSLCTL_DEV_INFO(adslctl_dev)->real_dev!=dev )
		return NOTIFY_DONE;
	
	ADSLCTL_DEBUG("got event %d on dev name %s \n", event, dev->name);
			
	switch (event) {
		case NETDEV_CHANGEADDR:
		case NETDEV_GOING_DOWN:
		case NETDEV_UNREGISTER:
			/* Ignore for now */
			break;

		case NETDEV_DOWN:

			ADSLCTL_DEBUG("handle down event \n");

			flgs = adslctl_dev->flags;
			if (flgs & IFF_UP)
				dev_change_flags(adslctl_dev, flgs & ~IFF_UP);
			break;

		case NETDEV_UP:

			ADSLCTL_DEBUG("handle up event \n");

			flgs = adslctl_dev->flags;
			if (!(flgs & IFF_UP))
				dev_change_flags(adslctl_dev, flgs | IFF_UP);
			break;
	};

	return NOTIFY_DONE;
}

/*
 * adsl_ctl_ioctl_handler 
 */
int adsl_ctl_ioctl_handler(unsigned long arg)
{
	int err = 0;
	struct adslctl_ioctl_args args;

	/* everything here needs root permissions, except aguably the
	 * hack ioctls for sending packets.  However, I know _I_ don't
	 * want users running that on my network! --BLG
	 */
	if (!capable(CAP_NET_ADMIN))
		return -EPERM;

	if (copy_from_user(&args, (void*)arg,
				sizeof(struct adslctl_ioctl_args)))
		return -EFAULT;

	switch (args.cmd) {
		case ADD_ADSLCTL_CMD:
			if (register_adsl_ctl_device(args.device)) {
				err = 0;
			} else {
				err = -EINVAL;
			}
			break;

		case DEL_ADSLCTL_CMD:
			if (unregister_adsl_ctl_device(args.device) == 0) {
				err = 0;
			} else {
				err = -EINVAL;
			}
			break;
		default:
			/* pass on to underlying device instead?? */
			printk("%s: Unknown ADSL ctl CMD: %x \n",
					__FUNCTION__, args.cmd);
			return -EINVAL;
	};

	return err;
}

/*
 * adslctl_init_module
 */
static int __init adslctl_init_module (void)
{
	printk ("Init ADSL control eth device\n");

	register_netdevice_notifier(&adslctl_notifier_block);
	adsl_ctl_ioctl_hook = adsl_ctl_ioctl_handler;

	return 0;
}

/*
 * adslctl_cleanup 
 */
static void __exit adslctl_cleanup (void)
{
	printk ("Cleaning the ADSL control eth device\n");
	return;
}

module_init (adslctl_init_module);
module_exit (adslctl_cleanup);


