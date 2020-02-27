/**************************************************
* sofaware_led.h
***************************************************
*/

#ifndef __SOFAWARE_HOSTAPD_H__
#define __SOFAWARE_HOSTAPD_H__

#include <asm/types.h>
#include <linux/netlink.h>
#include <linux/if_ether.h>

#define NETLINK_VFW 18 
#define VFW_GROUP 0 
#define MSG_SIZE NLMSG_SPACE(1024) 

#define MAX_NETWORK_NAME	32
#define MAX_AUTH_USER_NAME	50

typedef enum {
	INVALID_USER	= 0,
	RADIUS_TIMEOUT	= 1,
	SUPP_TIMEOUT	= 2,
	SESSION_TIMEOUT = 3
} e_err_type;

typedef enum {
	START_AUTH			= 0, // msg from kernel to hostapd
	DEAUTHENTICATE_MSG	= 1, // msg from kernel to hostapd
	AUTHENTICATE_STATUS	= 2, // msg from hostapd to kernel
} e_msg_type;

struct netlink_msg_t {
	int			ifindex;
	int			portid;
	char		mac_addr[ETH_ALEN];
	short		is_auth;
	char		group_name[MAX_NETWORK_NAME];
	e_err_type	err;
	char		user_name[MAX_AUTH_USER_NAME+1];
};


#endif /* #ifndef __SOFAWARE_HOSTAPD_H__ */

