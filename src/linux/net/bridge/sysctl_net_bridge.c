/* -*- linux-c -*-
 * sysctl_net_bridge.c: sysctl interface to net Bridge subsystem.
 *
 * Added /proc/sys/net/bridge directory entry (empty =) ). [MS]
 */

#include <linux/autoconf.h>
#include <linux/mm.h>
#include <linux/sysctl.h>
#include "br_private.h"
#include "br_private_stp.h"

#ifndef CONFIG_SYSCTL
#error This file should not be compiled without CONFIG_SYSCTL defined
#endif

/* From br_fdb.c */
extern int sysctl_max_mac_addresses;

static int min_forwarding[] = {0},	max_forwarding[] = {1};
static int  mac_addresses_min = 0; 
static int min_bpdu_forwarding[] = {0},      max_bpdu_forwarding[] = {1};

ctl_table bridge_table[] = {
	{0}
};

static const ctl_table br_param_table[] = {
	{NET_BRIDGE_FORWARDING, "forwarding",
	 &br_forwarding, sizeof(int), 0644, NULL, NULL,
	 &proc_dointvec_minmax, &sysctl_intvec,
	 &min_forwarding, &max_forwarding},
	{NET_BRIDGE_MAX_MAC_ADDRESSES, "max_mac_addresses",
	 &sysctl_max_mac_addresses, sizeof(int), 0644, NULL, NULL,
	 &proc_dointvec_minmax, &sysctl_intvec,
	 &mac_addresses_min, NULL},
        {NET_BRIDGE_BPDU_FORWARDING, "bpdu_forwarding",
         &br_bpdu_forwarding, sizeof(int), 0644, NULL, NULL,
         &proc_dointvec_minmax, &sysctl_intvec,
         &min_bpdu_forwarding, &max_bpdu_forwarding},
	{0}	/* that's all, folks! */
};

static ctl_table br_dir_table[] = {
	{NET_BRIDGE, "bridge", NULL, 0, 0555, br_param_table},
	{0}
};

static ctl_table br_root_table[] = {
	{CTL_NET, "net", NULL, 0, 0555, br_dir_table},
	{0}
};

static struct ctl_table_header *br_table_header;

void br_register_sysctl(void)
{
	br_table_header = register_sysctl_table(br_root_table);
}

void br_unregister_sysctl(void)
{
	unregister_sysctl_table(br_table_header);
}

