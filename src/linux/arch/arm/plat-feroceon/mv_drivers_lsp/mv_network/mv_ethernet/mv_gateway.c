/*******************************************************************************
Copyright (C) Marvell International Ltd. and its affiliates

This software file (the "File") is owned and distributed by Marvell 
International Ltd. and/or its affiliates ("Marvell") under the following
alternative licensing terms.  Once you have made an election to distribute the
File under one of the following license alternatives, please (i) delete this
introductory statement regarding license alternatives, (ii) delete the two
license alternatives that you have not elected to use and (iii) preserve the
Marvell copyright notice above.


********************************************************************************
Marvell GPL License Option

If you received this File from Marvell, you may opt to use, redistribute and/or 
modify this File in accordance with the terms and conditions of the General 
Public License Version 2, June 1991 (the "GPL License"), a copy of which is 
available along with the File in the license.txt file or by writing to the Free 
Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 or 
on the worldwide web at http://www.gnu.org/licenses/gpl.txt. 

THE FILE IS DISTRIBUTED AS-IS, WITHOUT WARRANTY OF ANY KIND, AND THE IMPLIED 
WARRANTIES OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE ARE EXPRESSLY 
DISCLAIMED.  The GPL License provides additional details about this warranty 
disclaimer.
*******************************************************************************/

#include "mvCommon.h"  /* Should be included before mvSysHwConfig */
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/pci.h>
#include <linux/ip.h>
#include <linux/in.h>
#include <linux/tcp.h>
#include "../../../../../../net/bridge/br_private.h"
#include <net/ip.h>
#include <net/xfrm.h>

#include "mvOs.h"
#include "dbg-trace.h"
#include "mvSysHwConfig.h"
#include "eth/mvEth.h"
#include "eth-phy/mvEthPhy.h"
#include "ctrlEnv/sys/mvSysGbe.h"
#include "msApi.h"

#include "mv_netdev.h"

#define MAX_SWITCHES		2

/* Structure of port mask: */
/* DDDD DDDD        CCCC CCCC        xxxY YYYX XXXX */
/* SW#1 CPU Port    SW#0 CPU Port    Y - SW#1 Port mask    X - SW#0 Port mask */ 
#define SWITCH_PORT_MASK(num)		((num) ? (0x4F) : (0x1F))
#define SWITCH_PORT_MASK_NO_DSL(num)		((num) ? (0x0F) : (0x1F))
#define SWITCH_PORT_OFFS(num)		((num) ? (5) : (0))
#define SWITCH_CPU_PORT_OFFS(num)	((num) ? (24) : (16))
#define SWITCH_CPU_PORT_MASK(num)	( (1<<SWITCH_PORT_CPU[(num)]) << SWITCH_CPU_PORT_OFFS(num) )

/* In switch 1, port 4 should not be configured as it is not connected */
#define SWITCH_UNUSED_PORTS(num)	((num) ? (0x10) : (0x00))
#define SWITCH_UNUSED_PORTS_NO_DSL(num)	((num) ? (0x50) : (0x00))

#define SWITCH_0		0
#define SWITCH_1		1

int SWITCH_PORT_CPU[MAX_SWITCHES];
int SWITCH_PORT_IC[MAX_SWITCHES];	/* Inter-connect port between the two switches */

int SWITCH_PORT_0;
int SWITCH_PORT_1;
int SWITCH_PORT_2;
int SWITCH_PORT_3;
int SWITCH_PORT_4;
int SWITCH_PORT_5;
int SWITCH_PORT_6;
int SWITCH_PORT_7;
int SWITCH_PORT_8;
int SWITCH_PORT_9;

unsigned int        switch_enabled_ports;

#ifdef CONFIG_MV_GTW_LINK_STATUS
static int          switch_irq = -1;
struct timer_list   switch_link_timer;
#endif

#ifdef CONFIG_MV_GTW_IGMP
extern int mv_gtw_igmp_snoop_init(void);
extern int mv_gtw_igmp_snoop_exit(void);
extern int mv_gtw_igmp_snoop_process(struct sk_buff* skb, unsigned char port, unsigned char vlan_dbnum);
#endif 

/* Example: "mv_net_config=4,(eth0,00:99:88:88:99:77,0)(eth1,00:55:44:55:66:77,1:2:3:4),mtu=1500" */
static char *cmdline = NULL;

struct mv_gtw_config    gtw_config;

GT_QD_DEV qddev[MAX_SWITCHES];
static GT_SYS_CONFIG    qd_cfg[MAX_SWITCHES];

static char* mv_gtw_port2lport(int switch_port)
{
    /* Note: the purpose of this function is to map between */
    /* "logical" switch port number (e.g. SWITCH_PORT_0) */
    /* and the board name as printed on the box */

    if(switch_port==SWITCH_PORT_0) return "Internal port#1";
    if(switch_port==SWITCH_PORT_1) return "Internal port#2";
    if(switch_port==SWITCH_PORT_2) return "Internal port#3";
    if(switch_port==SWITCH_PORT_3) return "Internal port#4";
    if(switch_port==SWITCH_PORT_4) return "Internal port#5";
    if(switch_port==SWITCH_PORT_5) return "Internal port#6";
    if(switch_port==SWITCH_PORT_6) return "Internal port#7";
    if(switch_port==SWITCH_PORT_7) return "Internal port#8";

    if(switch_port==SWITCH_PORT_8) return "Internal port#9";
    if(switch_port==SWITCH_PORT_9) return "Internal port#10";

    return "Unknown Port";
} 

/* Local function prototypes */

/* Required to get the configuration string from the Kernel Command Line */
int mv_gtw_cmdline_config(char *s);
__setup("mv_net_config=", mv_gtw_cmdline_config);

int mv_gtw_cmdline_config(char *s)
{
    cmdline = s;
    return 1;
}

static int mv_gtw_check_open_bracket(char **p_net_config)
{
    if (**p_net_config == '(') {
        (*p_net_config)++;
	return 0;
    }
    printk("Syntax error: could not find opening bracket\n");
    return -EINVAL;
}

static int mv_gtw_check_closing_bracket(char **p_net_config)
{
    if (**p_net_config == ')') {
        (*p_net_config)++;
	return 0;
    }
    printk("Syntax error: could not find closing bracket\n");
    return -EINVAL;
}

static int mv_gtw_check_comma(char **p_net_config)
{
    if (**p_net_config == ',') {
        (*p_net_config)++;
	    return 0;
    }
    printk("Syntax error: could not find comma\n");
    return -EINVAL;
}


static int mv_gtw_is_digit(char ch)
{
    if( ((ch >= '0') && (ch <= '9')) ||
	((ch >= 'a') && (ch <= 'f')) ||
	((ch >= 'A') && (ch <= 'F')) )
	    return 0;

    return -1;
}

static int mv_gtw_get_cmdline_mac_addr(char **p_net_config, int idx)
{
    /* the MAC address should look like: 00:99:88:88:99:77 */
    /* that is, 6 two-digit numbers, separated by :        */
    /* 6 times two-digits, plus 5 colons, total: 17 characters */
    const int   exact_len = 17; 
    int         i = 0;
    int         syntax_err = 0;
    char	*p_mac_addr = *p_net_config;

    /* check first 15 characters in groups of 3 characters at a time */
    for (i = 0; i < exact_len-2; i+=3) 
    {
	    if ( (mv_gtw_is_digit(**p_net_config) == 0) &&
	         (mv_gtw_is_digit(*(*p_net_config+1)) == 0) &&
	         ((*(*p_net_config+2)) == ':') )
	    {
	        (*p_net_config) += 3;
	    }
	    else {
	        syntax_err = 1;
	        break;
	    }
    }

    /* two characters remaining, must be two digits */
    if ( (mv_gtw_is_digit(**p_net_config) == 0) &&
         (mv_gtw_is_digit(*(*p_net_config+1)) == 0) )
    {
	    (*p_net_config) += 2;
    }
    else
	    syntax_err = 1;

    if (syntax_err == 0) {
        mvMacStrToHex(p_mac_addr, gtw_config.vlan_cfg[idx].macaddr);
        return 0;
    }
    printk("Syntax error while parsing MAC address from command line\n");
    return -EINVAL;
}

static void mv_gtw_update_curr_port_mask(char digit, unsigned int *curr_port_mask, int add_del_flag)
{
    if (digit == '0') {
	if (add_del_flag)
	    *curr_port_mask |= (1<<SWITCH_PORT_0);
	else
	    *curr_port_mask &= ~(1<<SWITCH_PORT_0);
    }
    if (digit == '1') {
	if (add_del_flag)
	    *curr_port_mask |= (1<<SWITCH_PORT_1);
	else
	    *curr_port_mask &= ~(1<<SWITCH_PORT_1);
    }
    if (digit == '2') {
	if (add_del_flag)
	    *curr_port_mask |= (1<<SWITCH_PORT_2);
	else
	    *curr_port_mask &= ~(1<<SWITCH_PORT_2);
    }
    if (digit == '3') {
	if (add_del_flag)
	    *curr_port_mask |= (1<<SWITCH_PORT_3);
	else
	    *curr_port_mask &= ~(1<<SWITCH_PORT_3);
    }
    if (digit == '4') {
	if (add_del_flag)
	    *curr_port_mask |= (1<<SWITCH_PORT_4);
	else
	    *curr_port_mask &= ~(1<<SWITCH_PORT_4);
    }
    if (digit == '5') {
	if (add_del_flag)
	    *curr_port_mask |= (1<<SWITCH_PORT_5);
	else
	    *curr_port_mask &= ~(1<<SWITCH_PORT_5);
    }
    if (digit == '6') {
	if (add_del_flag)
	    *curr_port_mask |= (1<<SWITCH_PORT_6);
	else
	    *curr_port_mask &= ~(1<<SWITCH_PORT_6);
    }
    if (digit == '7') {
	if (add_del_flag)
	    *curr_port_mask |= (1<<SWITCH_PORT_7);
	else
	    *curr_port_mask &= ~(1<<SWITCH_PORT_7);
    }
    if (digit == '8') {
	if (add_del_flag)
	    *curr_port_mask |= (1<<SWITCH_PORT_8);
	else
	    *curr_port_mask &= ~(1<<SWITCH_PORT_8);
    }
    if (digit == '9') {
	if (add_del_flag)
	    *curr_port_mask |= (1<<SWITCH_PORT_9);
	else
	    *curr_port_mask &= ~(1<<SWITCH_PORT_9);
	}
}

static int mv_gtw_get_port_mask(char **p_net_config, int idx)
{
    /* the port mask should look like this: */
    /* example 1: 0 */
    /* example 2: 1:2:3:4 */
    /* that is, one or more one-digit numbers, separated with : */
    /* we have up to MV_GTW_MAX_NUM_OF_IFS interfaces */

    unsigned int curr_port_mask = 0, i = 0;
    int syntax_err = 0;

    for (i = 0; i < MV_GTW_MAX_NUM_OF_IFS; i++) 
    {
	if (mv_gtw_is_digit(**p_net_config) == 0) 
        {
	    if (*(*p_net_config+1) == ':') 
            {
		mv_gtw_update_curr_port_mask(**p_net_config, &curr_port_mask, 1);
		(*p_net_config) += 2;
	    }
	    else if (*(*p_net_config+1) == ')') 
            {
	        mv_gtw_update_curr_port_mask(**p_net_config, &curr_port_mask, 1);
	        (*p_net_config)++;
	        break;
	    }
	    else {
	        syntax_err = 1;
	        break;
	    }
	}
	else {
	    syntax_err = 1;
	    break;
	}
    }

    if (syntax_err == 0) {
	gtw_config.vlan_cfg[idx].ports_mask = curr_port_mask;
	return 0;
    }
    printk("Syntax error while parsing port mask from command line\n");
    return -EINVAL;
}

static int mv_gtw_get_mtu(char **p_net_config)
{
    /* the mtu value is constructed as follows: */
    /* mtu=value                                */
    unsigned int mtu;
    int syntax_err = 0;

    if(strncmp(*p_net_config,"mtu=",4) == 0)
    {
        *p_net_config += 4;
        mtu = 0;
        while((**p_net_config >= '0') && (**p_net_config <= '9'))
        {       
            mtu = (mtu * 10) + (**p_net_config - '0');
            *p_net_config += 1;
        }
        if(**p_net_config != '\0')
            syntax_err = 1;
    }
    else
    {
        syntax_err = 1;
    }

    if(syntax_err == 0)
    {
        gtw_config.mtu = mtu;
        printk("      o MTU set to %d\n", mtu);
        return 0;
    }

    printk("Syntax error while parsing mtu value from command line\n");
    return -EINVAL;
}

static int mv_gtw_get_if_num(char **p_net_config)
{
    char    if_num = **p_net_config;
    printk("mv_gtw_get_if_num");
    if (mv_gtw_is_digit(if_num) == 0) 
    {
		int digit2;
        gtw_config.vlans_num = if_num - '0';
	(*p_net_config) += 1;
		if_num = **p_net_config;
		if (mv_gtw_is_digit(if_num) == 0)
		{
			digit2 = if_num - '0';
			gtw_config.vlans_num = gtw_config.vlans_num * 10 + digit2;
			(*p_net_config) += 1;
		}
		printk("mv_gtw_get_if_num gtw_config.vlans_num=%d",gtw_config.vlans_num);
        return 0;
    }
    printk("Syntax error while parsing number of interfaces from command line\n");
    return -EINVAL;
}

static int mv_gtw_parse_net_config(char* cmdline)
{
    char    *p_net_config = cmdline;
    int     i = 0;
    int     status = 0;

    if (p_net_config == NULL)
	    return -EINVAL;

    /* Parse number of gateway interfaces */
    status = mv_gtw_get_if_num(&p_net_config);
    if (status != 0)
        return status;

    if (gtw_config.vlans_num > MV_GTW_MAX_NUM_OF_IFS)
    {
        printk("Too large number of interfaces (%d) in command line: cut to %d\n",
                gtw_config.vlans_num, MV_GTW_MAX_NUM_OF_IFS);
        gtw_config.vlans_num = MV_GTW_MAX_NUM_OF_IFS;
    }
   
	printk("number of interfaces in command line is %d\n", gtw_config.vlans_num);

    status = mv_gtw_check_comma(&p_net_config);
    if (status != 0)
        return status;

    for (i = 0; (i < MV_GTW_MAX_NUM_OF_IFS) && (*p_net_config != '\0'); i++) 
    {
        status = mv_gtw_check_open_bracket(&p_net_config);
	if (status != 0)
	    break;
	status = mv_gtw_get_cmdline_mac_addr(&p_net_config, i);
	if (status != 0)
	    break;
	status = mv_gtw_check_comma(&p_net_config);
	if (status != 0)
	    break;
	status = mv_gtw_get_port_mask(&p_net_config, i);
	if (status != 0)
	    break;
	status = mv_gtw_check_closing_bracket(&p_net_config);
	if (status != 0)
	    break;

        /* If we have a comma after the closing bracket, then interface */
        /* definition is done.                                          */
        if(*p_net_config == ',')
            break;
    }

    if(*p_net_config != '\0')
    {
        status = mv_gtw_check_comma(&p_net_config);
        if (status == 0)
        {
            status = mv_gtw_get_mtu(&p_net_config);
        }
    }
    else
    {
        gtw_config.mtu = 1500;
        printk("      o Using default MTU %d\n", gtw_config.mtu);
    }
    
    /* at this point, we have parsed up to MV_GTW_MAX_NUM_OF_IFS, and the mtu value */
    /* if the net_config string is not finished yet, then its format is invalid */
    if (*p_net_config != '\0')
    {
        printk("Gateway config string is too long: %s\n", p_net_config);
        status = -EINVAL;
    }
    return status;
}

GT_BOOL gtwReadMiiWrap(GT_QD_DEV* dev, unsigned int portNumber, unsigned int MIIReg, unsigned int* value)
{
    unsigned long   flags;
    unsigned short  tmp;
    MV_STATUS       status;

    spin_lock_irqsave(&mii_lock, flags);

    status = mvEthPhyRegRead(portNumber, MIIReg , &tmp);
    spin_unlock_irqrestore(&mii_lock, flags);
    *value = tmp;

    if (status == MV_OK)
        return GT_TRUE;

    return GT_FALSE;
}

GT_BOOL gtwWriteMiiWrap(GT_QD_DEV* dev, unsigned int portNumber, unsigned int MIIReg, unsigned int data)
{
    unsigned long   flags;
    unsigned short  tmp;
    MV_STATUS       status;

    spin_lock_irqsave(&mii_lock, flags);
    tmp = (unsigned short)data;
    status = mvEthPhyRegWrite(portNumber, MIIReg, tmp);

    spin_unlock_irqrestore(&mii_lock, flags);

    if (status == MV_OK)
        return GT_TRUE;

    return GT_FALSE;
} 

INLINE struct net_device*  mv_gtw_port_to_netdev_map(int switch_num, unsigned int switch_port)
{
    int i;
    struct mv_vlan_cfg  *vlan_cfg;

    for (i = 0; i < mv_net_devs_num; i++) {
        if (mv_net_devs[i] == NULL)
	    break;

        vlan_cfg = MV_NETDEV_VLAN(mv_net_devs[i]);
  	if (vlan_cfg != NULL) {
	    if(vlan_cfg->ports_mask & ((1 << switch_port) << SWITCH_PORT_OFFS(switch_num)))
            {
                return mv_net_devs[i];
            }
        }
    }
    return NULL;
}

static int mv_gtw_set_port_based_vlan(int qd_num, unsigned int ports_mask)
{
    unsigned int p, pl;
    unsigned char cnt;
    GT_LPORT port_list[MAX_SWITCH_PORTS];
    GT_QD_DEV *qd = &qddev[qd_num];

    ETH_DBG(ETH_DBG_GTW, ("mv_gtw_set_port_based_vlan, qd_num=%d, qd_p=%p, ports_mask=0x%X\n", qd_num, qd, ports_mask));

    for(p=0; p<qd->numOfPorts; p++) {
	if( MV_BIT_CHECK(ports_mask, p) && (p != SWITCH_PORT_CPU[qd_num]) && !MV_BIT_CHECK(SWITCH_UNUSED_PORTS(qd_num), p)) {
	    ETH_DBG( ETH_DBG_LOAD, ("port based vlan, port %d: ",p));
	    for(pl=0,cnt=0; pl<qd->numOfPorts; pl++) {
		if( MV_BIT_CHECK(ports_mask, pl) && (pl != p) && !MV_BIT_CHECK(SWITCH_UNUSED_PORTS(qd_num), pl)) {
		    ETH_DBG( ETH_DBG_LOAD, ("%d ",pl));
		    port_list[cnt] = pl;
                    cnt++;
                }
	    }
	    /* we don't want to change the port based VLAN of switch 0 port 5 (the interconnect port) */
	    /* because we already connected it to all other ports in mv_switch_init() and we want to keep */
	    /* it that way. */
	    if ( (qd_num != SWITCH_0) || (p != SWITCH_PORT_IC[qd_num]) ) {
            	if( gvlnSetPortVlanPorts(qd, p, port_list, cnt) != GT_OK) {
	        	printk("gvlnSetPortVlanPorts failed\n");
                	return -1;
            	}
	    	ETH_DBG( ETH_DBG_LOAD, ("\n"));
	    }
        }
	}
    return 0;
}


static int mv_gtw_set_vlan_in_vtu(int qd_num, unsigned short vlan_id, short db_num,  unsigned int ports_mask, int add)
{
    GT_VTU_ENTRY vtu_entry;
    unsigned int p;
    GT_QD_DEV *qd = &qddev[qd_num];

    vtu_entry.vid = vlan_id;

#if 0
	if (vlan_id != 0x100 || vlan_id != 0x200)
	{
		printk("%s: vlan_id=%d, Setting vtu_entry.DBNum = 0\n", __FUNCTION__, vlan_id);
		vtu_entry.DBNum = 0;
	}
	else
	{
	    vtu_entry.DBNum = MV_GTW_VLAN_TO_GROUP(vlan_id); /*Take this by port number!!! not vlan_id */
 	    printk("%s: vlan_id=%d, Setting vtu_entry.DBNum = %d\n", __FUNCTION__, vlan_id, vtu_entry.DBNum);	
	}
#endif /*if 0*/

    vtu_entry.DBNum = db_num;
	
    vtu_entry.vidPriOverride = GT_FALSE;
    vtu_entry.vidPriority = 0;
    vtu_entry.vidExInfo.useVIDFPri = GT_FALSE;
    vtu_entry.vidExInfo.vidFPri = 0;
    vtu_entry.vidExInfo.useVIDQPri = GT_FALSE;
    vtu_entry.vidExInfo.vidQPri = 0;
    vtu_entry.vidExInfo.vidNRateLimit = GT_FALSE;
    ETH_DBG( ETH_DBG_LOAD, ("vtu entry: vid=0x%x, port ", vtu_entry.vid));

    ETH_DBG(ETH_DBG_GTW, ("mv_gtw_set_vlan_in_vtu: qd=%p, db=%d, vlan_id=%d, ports_mask = 0x%X\n", 
	qd, vtu_entry.DBNum, vlan_id, ports_mask));

    for(p=0; p<qd->numOfPorts; p++) {
        if(MV_BIT_CHECK(ports_mask, p)) {
	    ETH_DBG( ETH_DBG_LOAD, ("%d ", p));
	    vtu_entry.vtuData.memberTagP[p] = MEMBER_EGRESS_UNMODIFIED;
	}
	else {
	    vtu_entry.vtuData.memberTagP[p] = NOT_A_MEMBER;
	}
	vtu_entry.vtuData.portStateP[p] = 0;
    }

   if (add)
   {
	    if(gvtuAddEntry(qd, &vtu_entry) != GT_OK) {
	        printk("gvtuAddEntry failed\n");
	        return -1;
	    }
    }
   else
   {
	   if(gvtuDelEntry(qd, &vtu_entry) != GT_OK) {
		   printk("gvtuDelEntry failed\n");
		   return -1;
	   }
   }

    ETH_DBG( ETH_DBG_LOAD, ("\n"));
    return 0;
}

int mv_gtw_set_mac_addr_to_switch(unsigned char *mac_addr, unsigned char db, unsigned int ports_mask, unsigned char op)
{
    GT_ATU_ENTRY mac_entry;
    struct mv_vlan_cfg *nc;
    int i;

    /* validate db with VLAN id */
    nc = &gtw_config.vlan_cfg[db];
    if(MV_GTW_VLAN_TO_GROUP(nc->vlan_grp_id) != db) {
        printk("mv_gtw_set_mac_addr_to_switch (invalid db)\n");
	return -1;
    }

    memset(&mac_entry,0,sizeof(GT_ATU_ENTRY));

    mac_entry.trunkMember = GT_FALSE;
    mac_entry.prio = 0;
    mac_entry.exPrio.useMacFPri = GT_FALSE;
    mac_entry.exPrio.macFPri = 0;
    mac_entry.exPrio.macQPri = 0;
    mac_entry.DBNum = db;
    mac_entry.portVec = ports_mask;
    memcpy(mac_entry.macAddr.arEther,mac_addr,6);

    if(is_multicast_ether_addr(mac_addr))
	mac_entry.entryState.mcEntryState = GT_MC_STATIC; /* do not set multicasts as management */
    else
	mac_entry.entryState.ucEntryState = GT_UC_TO_CPU_STATIC;

    ETH_DBG(ETH_DBG_ALL, ("mv_gateway: db%d port-mask=0x%x, %02x:%02x:%02x:%02x:%02x:%02x ",
	    db, (unsigned int)mac_entry.portVec,
	    mac_entry.macAddr.arEther[0],mac_entry.macAddr.arEther[1],mac_entry.macAddr.arEther[2],
	    mac_entry.macAddr.arEther[3],mac_entry.macAddr.arEther[4],mac_entry.macAddr.arEther[5]));

    for (i = 0; i < MAX_SWITCHES; i++) {

	mac_entry.portVec = ((ports_mask >> SWITCH_PORT_OFFS(i)) & SWITCH_PORT_MASK(i));
	mac_entry.portVec |= ((ports_mask >> SWITCH_CPU_PORT_OFFS(i)) & 0xFF);
	/* must add IC port in case of multicast so second switch gets the frame too */
	if( (i == SWITCH_0) && is_multicast_ether_addr(mac_addr))
		mac_entry.portVec |= (1 << SWITCH_PORT_IC[0]);

	ETH_DBG(ETH_DBG_GTW, ("mv_gtw_set_mac_addr_to_switch: i = %d, ports_mask = 0x%lX\n", i, mac_entry.portVec));
	ETH_DBG(ETH_DBG_GTW, ("%02x:%02x:%02x:%02x:%02x:%02x\n", 
	    mac_entry.macAddr.arEther[0],mac_entry.macAddr.arEther[1],mac_entry.macAddr.arEther[2],
	    mac_entry.macAddr.arEther[3],mac_entry.macAddr.arEther[4],mac_entry.macAddr.arEther[5]));

    	if((op == 0) || (mac_entry.portVec == 0)) {
/*			
		printk("%s: IM HERE 1, %02x:%02x:%02x:%02x:%02x:%02x %x %d %x\n", __FUNCTION__,  
			mac_entry.macAddr.arEther[0],mac_entry.macAddr.arEther[1],mac_entry.macAddr.arEther[2],
		    mac_entry.macAddr.arEther[3],mac_entry.macAddr.arEther[4],mac_entry.macAddr.arEther[5],
			mac_entry.portVec, i, mac_entry.DBNum);
*/
        	if(gfdbDelAtuEntry(&qddev[i], &mac_entry) != GT_OK) {
	    		printk("gfdbDelAtuEntry failed\n");
	    		return -1;
        	}
		ETH_DBG(ETH_DBG_ALL, ("deleted\n"));
    	}
    	else {
/*			
		printk("%s: IM HERE 2, %02x:%02x:%02x:%02x:%02x:%02x %x %d %x\n", __FUNCTION__,  
			mac_entry.macAddr.arEther[0],mac_entry.macAddr.arEther[1],mac_entry.macAddr.arEther[2],
		    mac_entry.macAddr.arEther[3],mac_entry.macAddr.arEther[4],mac_entry.macAddr.arEther[5],
			mac_entry.portVec, i, mac_entry.DBNum);
*/
        	if(gfdbAddMacEntry(&qddev[i], &mac_entry) != GT_OK) {
	    		printk("gfdbAddMacEntry failed\n");
	    		return -1;
        	}
		ETH_DBG(ETH_DBG_ALL, ("added\n"));
			}
		}

    return 0;
}

#ifdef CONFIG_MV_GTW_IGMP
int mv_gtw_enable_igmp(void)
{
    unsigned char p;
    int num;

    ETH_DBG( ETH_DBG_LOAD, ("enabling L2 IGMP snooping\n"));

    for (num = 0; num < MAX_SWITCHES; num++) {
    	/* enable IGMP snoop on all ports (except cpu port) */
    	for (p=0; p<qddev[num].numOfPorts; p++) {
		if( (p != SWITCH_PORT_CPU[num]) && !MV_BIT_CHECK(SWITCH_UNUSED_PORTS(num), p) ) {
	    		if(gprtSetIGMPSnoop(&qddev[num], p, GT_TRUE) != GT_OK) {
				printk("gprtSetIGMPSnoop failed\n");
				return -1;
	    		}
		}
    	}
    }
    return -1;
}
#endif /* CONFIG_MV_GTW_IGMP */


int __init mv_gtw_net_setup(int port)
{
    struct mv_vlan_cfg  *nc;
    int                 i = 0;

    SWITCH_PORT_CPU[0] = mvBoardSwitchCpuPortGet(port);
    SWITCH_PORT_CPU[1] = 5; /* hard coded according to board setup */

    SWITCH_PORT_IC[0] = 5; /* hard coded according to board setup */
    SWITCH_PORT_IC[1] = SWITCH_PORT_CPU[1]; /* inter-connect port is considered CPU port in switch #1 */

    SWITCH_PORT_0 = mvBoardSwitchPortGet(port, 0);
    SWITCH_PORT_1 = mvBoardSwitchPortGet(port, 1);
    SWITCH_PORT_2 = mvBoardSwitchPortGet(port, 2);
    SWITCH_PORT_3 = mvBoardSwitchPortGet(port, 3);
    SWITCH_PORT_4 = mvBoardSwitchPortGet(port, 4);
    SWITCH_PORT_5 = mvBoardSwitchPortGet(port, 5);
    SWITCH_PORT_6 = mvBoardSwitchPortGet(port, 6);
    SWITCH_PORT_7 = mvBoardSwitchPortGet(port, 7);
    SWITCH_PORT_8 = mvBoardSwitchPortGet(port, 8);
    SWITCH_PORT_9 = mvBoardSwitchPortGet(port, 9);

#ifdef CONFIG_MV_GTW_LINK_STATUS
    switch_irq = mvBoardLinkStatusIrqGet(port);
    switch_irq = 34;
    if(switch_irq != -1)
        switch_irq += IRQ_GPP_START;
#endif

    /* build the net config table */
    memset(&gtw_config, 0, sizeof(struct mv_gtw_config));

    if(cmdline != NULL) 
    {
	    printk("      o Using command line network interface configuration\n");
	    printk("      command is %s\n", cmdline);
    }
    else
    {
	    printk("      o Using default network configuration, overriding boot MAC address\n");
	    cmdline = CONFIG_MV_GTW_CONFIG;
    }

    if (mv_gtw_parse_net_config(cmdline) < 0) 
    {
	    printk("Error parsing mv_net_config\n");
	    return -EINVAL;
    }           

    /* CPU port should always be enabled */
    switch_enabled_ports = SWITCH_CPU_PORT_MASK(SWITCH_0) | SWITCH_CPU_PORT_MASK(SWITCH_1);

    for(i=0, nc=&gtw_config.vlan_cfg[i]; i<gtw_config.vlans_num; i++, nc++) 
    {
	    /* VLAN ID */
	    nc->vlan_grp_id = MV_GTW_GROUP_VLAN_ID(i);
	    nc->ports_link = 0;
	    nc->header = MV_GTW_GROUP_VLAN_ID(i);

	    /* print info */
	    printk("      o mac_addr %02x:%02x:%02x:%02x:%02x:%02x, VID 0x%03x, port list: ", 
                nc->macaddr[0], nc->macaddr[1], nc->macaddr[2], 
	        nc->macaddr[3], nc->macaddr[4], nc->macaddr[5], nc->vlan_grp_id);

	    if(nc->ports_mask & (1<<SWITCH_PORT_0)) 
                printk("port-0 "); /* LAN 1 */
	    if(nc->ports_mask & (1<<SWITCH_PORT_1)) 
                printk("port-1 "); /* LAN 2 */
	    if(nc->ports_mask & (1<<SWITCH_PORT_2)) 
                printk("port-2 "); /* LAN 3 */
	    if(nc->ports_mask & (1<<SWITCH_PORT_3)) 
                printk("port-3 "); /* LAN 4 */
	    if(nc->ports_mask & (1<<SWITCH_PORT_4)) 
                printk("port-4 "); /* LAN 5 */
	    if(nc->ports_mask & (1<<SWITCH_PORT_5)) 
	        printk("port-5 "); /* LAN 6 */
	    if(nc->ports_mask & (1<<SWITCH_PORT_6)) 
                printk("port-6 "); /* LAN 7 */
	    if(nc->ports_mask & (1<<SWITCH_PORT_7)) 
                printk("port-7 "); /* WAN 1 */
	    if(nc->ports_mask & (1<<SWITCH_PORT_8)) 
	        printk("port-8 ");
	    if(nc->ports_mask & (1<<SWITCH_PORT_9)) 
	        printk("port-9 "); /* DSL */

	    printk("\n");

	    /* collect per-interface port_mask into a global port_mask, used for enabling the Switch ports */
	    switch_enabled_ports |= nc->ports_mask;
    }

    return 0;
}

static int mv_switch_init(int port)
{
	unsigned int		i, p;
	unsigned char		cnt;
	GT_LPORT		port_list[MAX_SWITCH_PORTS];
	struct mv_vlan_cfg	*nc;
	GT_JUMBO_MODE		jumbo_mode;
	int			num;
	GT_QD_DEV		*qd_dev = NULL;
	unsigned int		ports_mask;
#ifdef CONFIG_MV_GTW_LINK_STATUS
	GT_DEV_EVENT		gt_event;
#endif /* CONFIG_MV_GTW_LINK_STATUS */

    memset((char*)&qd_cfg,0,sizeof(GT_SYS_CONFIG)*MAX_SWITCHES);
    for (num = 0; num < MAX_SWITCHES; num++) {
	/* init config structure for qd package */
	qd_cfg[num].BSPFunctions.readMii   = gtwReadMiiWrap;
	qd_cfg[num].BSPFunctions.writeMii  = gtwWriteMiiWrap;
	qd_cfg[num].BSPFunctions.semCreate = NULL;
	qd_cfg[num].BSPFunctions.semDelete = NULL;
	qd_cfg[num].BSPFunctions.semTake   = NULL;
	qd_cfg[num].BSPFunctions.semGive   = NULL;
	qd_cfg[num].initPorts = GT_TRUE;
	qd_cfg[num].cpuPortNum = SWITCH_PORT_CPU[num];		
	if (mvBoardSmiScanModeGet(port) == 1) {
		qd_cfg[num].mode.baseAddr = 0;
		qd_cfg[num].mode.scanMode = SMI_MANUAL_MODE;
	}
	else if (mvBoardSmiScanModeGet(port) == 2) {
		qd_cfg[num].mode.baseAddr = mvBoardPhyAddrGet(port)+num;
		qd_cfg[num].mode.scanMode = SMI_MULTI_ADDR_MODE;
	}
	/* load switch sw package */
	if (qdLoadDriver(&qd_cfg[num], &qddev[num]) != GT_OK) {
		printk("qdLoadDriver failed\n");
		return -1;
	}
	qd_dev = &qddev[num];

	ETH_DBG( ETH_DBG_LOAD, ("Device ID     : 0x%x\n",qd_dev->deviceId));
	ETH_DBG( ETH_DBG_LOAD, ("Base Reg Addr : 0x%x\n",qd_dev->baseRegAddr));
	ETH_DBG( ETH_DBG_LOAD, ("No. of Ports  : %d\n",qd_dev->numOfPorts));
	ETH_DBG( ETH_DBG_LOAD, ("CPU Ports     : %d\n",qd_dev->cpuPortNum));

	/* disable all ports */
	for(p=0; p<qd_dev->numOfPorts; p++) {
		if (!MV_BIT_CHECK(SWITCH_UNUSED_PORTS(num), p))
			gstpSetPortState(qd_dev, p, GT_PORT_DISABLE);
	}

	if(gstatsFlushAll(qd_dev) != GT_OK) {
		printk("gstatsFlushAll failed\n");
	}

	/* set device number */
	if(gsysSetDeviceNumber(qd_dev, num) != GT_OK) {
		printk("gsysSetDeviceNumber failed\n");
		return -1;
	}

	if(gpvtInitialize(qd_dev) != GT_OK) {
		printk("gpvtInitialize failed\n");
		return -1;
	}

	/* set all ports not to unmodify the tag on egress */
	for(p=0; p<qd_dev->numOfPorts; p++) {
		if (!MV_BIT_CHECK(SWITCH_UNUSED_PORTS(num), p)) {
			if(gprtSetEgressMode(qd_dev, p, GT_UNMODIFY_EGRESS) != GT_OK) {
				printk("gprtSetEgressMode GT_UNMODIFY_EGRESS failed\n");
				return -1;
			}
		}
	}
	/* set all ports to work in Normal mode, except the CPU port and the */
	/* ports connecting the two switches, which are configured as DSA ports. */
	for(p=0; p<qd_dev->numOfPorts; p++) {
		if (!MV_BIT_CHECK(SWITCH_UNUSED_PORTS(num), p)) {
			if ((p != SWITCH_PORT_CPU[num]) && (p != SWITCH_PORT_IC[num])) {
				if (gprtSetFrameMode(qd_dev, p, GT_FRAME_MODE_NORMAL) != GT_OK) {
					printk("gprtSetFrameMode GT_FRAME_MODE_NORMAL failed\n");
					return -1;
				}
			}
			else {
				if (gprtSetFrameMode(qd_dev, p, GT_FRAME_MODE_DSA) != GT_OK) {
					printk("gprtSetFrameMode GT_FRAME_MODE_DSA failed\n");
					return -1;
				}
			}
		}
	}

	/* do not use Marvell Header mode */
	for(p=0; p<qd_dev->numOfPorts; p++) {
		if (!MV_BIT_CHECK(SWITCH_UNUSED_PORTS(num), p)) {
			if(gprtSetHeaderMode(qd_dev, p, GT_FALSE) != GT_OK) {
				printk("gprtSetHeaderMode GT_FALSE failed\n");
				return -1;
			}
		}
	}

	/* Set 802.1Q Disabled mode for all ports except for CPU and IC ports which are
	   set to fallback */
	for(p=0; p<qd_dev->numOfPorts; p++) {
		if(!MV_BIT_CHECK(SWITCH_UNUSED_PORTS(num), p)) {
			if ((p != SWITCH_PORT_CPU[num]) && (p != SWITCH_PORT_IC[num])) {
				if(gvlnSetPortVlanDot1qMode(qd_dev, p, GT_DISABLE) != GT_OK) {
					printk("gvlnSetPortVlanDot1qMode failed\n");
					return -1;
				}
			} else {
				if(gvlnSetPortVlanDot1qMode(qd_dev, p, GT_FALLBACK) != GT_OK) {
					printk("gvlnSetPortVlanDot1qMode failed\n");
					return -1;
				}
			}
		}
	}
	
	/* Setup jumbo frames mode		*/            	
	if( MV_RX_BUF_SIZE(gtw_config.mtu) <= 1522)
		jumbo_mode = GT_JUMBO_MODE_1522;
	else if( MV_RX_BUF_SIZE(gtw_config.mtu) <= 2048)
		jumbo_mode = GT_JUMBO_MODE_2048;
	else
		jumbo_mode = GT_JUMBO_MODE_10240;
	
	for(p=0; p<qd_dev->numOfPorts; p++) {
		if(!MV_BIT_CHECK(SWITCH_UNUSED_PORTS(num), p)) {
			if(gsysSetJumboMode(qd_dev, p, jumbo_mode) != GT_OK) {
				printk("gsysSetJumboMode %d failed\n",jumbo_mode);
				return -1;
			}
		}
	}

	/* set priorities rules */
	for(p=0; p<qd_dev->numOfPorts; p++) {
		if(!MV_BIT_CHECK(SWITCH_UNUSED_PORTS(num), p)) {
        		/* default port priority to queue zero */
	    		if(gcosSetPortDefaultTc(qd_dev, p, 0) != GT_OK)
	        		printk("gcosSetPortDefaultTc failed (port %d)\n", p);
        
        		/* enable IP TOS Prio */
	    		if(gqosIpPrioMapEn(qd_dev, p, GT_TRUE) != GT_OK)
	        		printk("gqosIpPrioMapEn failed (port %d)\n", p);
	
        		/* set IP QoS */
	    		if(gqosSetPrioMapRule(qd_dev, p, GT_FALSE) != GT_OK)
	        		printk("gqosSetPrioMapRule failed (port %d)\n", p);
        
        		/* disable Vlan QoS Prio */
	    		if(gqosUserPrioMapEn(qd_dev, p, GT_FALSE) != GT_OK)
	        		printk("gqosUserPrioMapEn failed (port %d)\n", p);
        
        		/* Set force flow control to FALSE for all ports */
	    		if(gprtSetForceFc(qd_dev, p, GT_FALSE) != GT_OK)
	        		printk("gprtSetForceFc failed (port %d)\n", p);
		}
    	}

	/* The switch CPU port is not part of the VLAN, but rather connected by tunneling to each */
	/* of the VLAN's ports. Our MAC addr will be added during start operation to the VLAN DB  */
	/* at switch level to forward packets with this DA to CPU port.                           */
	ETH_DBG( ETH_DBG_LOAD, ("Enabling Tunneling on ports: "));
	for(p=0; p<qd_dev->numOfPorts; p++) {
		if( (p != SWITCH_PORT_CPU[num]) && !MV_BIT_CHECK(SWITCH_UNUSED_PORTS(num), p) ) {
			if(gprtSetVlanTunnel(qd_dev, p, GT_TRUE) != GT_OK) {
				printk("gprtSetVlanTunnel failed (port %d)\n", p);
				return -1;
			}
			else {
				ETH_DBG( ETH_DBG_LOAD, ("%d ", p));
			}
		}
	}
	ETH_DBG( ETH_DBG_LOAD, ("\n"));

	/* configure ports (excluding CPU port) for each network interface (VLAN): */
	for(i=0, nc=&gtw_config.vlan_cfg[i]; i<gtw_config.vlans_num; i++,nc++) {
		ETH_DBG( ETH_DBG_LOAD, ("vlan%d configuration (nc->ports_mask = 0x%08x) \n",
					i, nc->ports_mask));
		/* set port's default private vlan id and database number (DB per group): */
		for(p=0; p<qd_dev->numOfPorts; p++) {
			if( MV_BIT_CHECK((nc->ports_mask >> SWITCH_PORT_OFFS(num)) & SWITCH_PORT_MASK(num), p) && 
					(p != SWITCH_PORT_CPU[num]) && !MV_BIT_CHECK(SWITCH_UNUSED_PORTS(num), p) ) {
				if(gvlnSetPortVid(qd_dev, p, nc->vlan_grp_id) != GT_OK ) {
					printk("gvlnSetPortVid failed");
					return -1;
				}
				if(gvlnSetPortVlanDBNum(qd_dev, p, MV_GTW_VLAN_TO_GROUP(nc->vlan_grp_id)) != GT_OK) {
					printk("gvlnSetPortVlanDBNum failed\n");
					return -1;
				}
			}
		}

		ports_mask = ((nc->ports_mask >> SWITCH_PORT_OFFS(num)) & SWITCH_PORT_MASK(num));
 		ports_mask |= (1 << SWITCH_PORT_IC[num]);
		if (num == SWITCH_0)
			ports_mask &= ~(1 << SWITCH_PORT_CPU[num]);
		/* set port's port-based vlan (CPU port is not part of VLAN) */
		if(mv_gtw_set_port_based_vlan(num, ports_mask) != 0) {
			printk("mv_gtw_set_port_based_vlan failed\n");
		}

		/* set vtu with group vlan id (used in tx) */
		if(mv_gtw_set_vlan_in_vtu(num, nc->vlan_grp_id, MV_GTW_VLAN_TO_GROUP(nc->vlan_grp_id),
			( ((nc->ports_mask >> SWITCH_PORT_OFFS(num)) & SWITCH_PORT_MASK(num)) 
				| (1 << SWITCH_PORT_CPU[num]) | (1 << SWITCH_PORT_IC[num]) ), 1) != 0) {
			printk("mv_gtw_set_vlan_in_vtu failed\n");
		}
	}
	
	/* set cpu-port with port-based vlan to all other ports */
	ETH_DBG( ETH_DBG_LOAD, ("cpu port-based vlan:"));
	for(p=0,cnt=0; p<qd_dev->numOfPorts; p++) {
		if(p != SWITCH_PORT_CPU[num] && !MV_BIT_CHECK(SWITCH_UNUSED_PORTS(num), p) ) {
			ETH_DBG( ETH_DBG_LOAD, ("%d ",p));
			port_list[cnt] = p;
			cnt++;
		}
	}
	ETH_DBG( ETH_DBG_LOAD, ("\n"));
	if( gvlnSetPortVlanPorts(qd_dev, SWITCH_PORT_CPU[num], port_list, cnt) != GT_OK) {
		printk("gvlnSetPortVlanPorts failed\n");
		return -1;
	}

	if (num == SWITCH_0) 
	{
		/* set switch #0 inter-connect port with port-based vlan to		*/
		/* other ports in switch #0 which can be part of a cross-switch vlan.	*/
		for(p=0,cnt=0; p<qd_dev->numOfPorts; p++) {
			if (	(p != SWITCH_PORT_IC[num]) && 
				(p != SWITCH_PORT_CPU[num]) && 
				!MV_BIT_CHECK(SWITCH_UNUSED_PORTS(num), p) ) {
				port_list[cnt] = p;
				cnt++;
			}
		}
		if( gvlnSetPortVlanPorts(qd_dev, SWITCH_PORT_IC[num], port_list, cnt) != GT_OK) {
			printk("gvlnSetPortVlanPorts failed\n");
			return -1;
		}
	}

	if(gfdbFlush(qd_dev,GT_FLUSH_ALL) != GT_OK) {
		printk("gfdbFlush failed\n");
	}

#ifdef CONFIG_MV_GTW_LINK_STATUS
	/* Enable Phy Link Status Changed interrupt at Phy level for the all enabled ports */
	for(p=0; p<qd_dev->numOfPorts; p++) {
		if(	MV_BIT_CHECK((switch_enabled_ports >> SWITCH_PORT_OFFS(num)) & SWITCH_PORT_MASK_NO_DSL(num), p) && 
			(p != SWITCH_PORT_CPU[num])) {
			if(gprtPhyIntEnable(qd_dev, p, (GT_LINK_STATUS_CHANGED)) != GT_OK) {
				printk("gprtPhyIntEnable failed port %d\n", p);
			}
		}
	}
	
	gt_event.event = GT_DEV_INT_PHY;
	gt_event.portList = 0;
	gt_event.phyList = SWITCH_PORT_MASK_NO_DSL(num);

	if (switch_irq != -1) {
		if(eventSetDevInt(qd_dev, &gt_event) != GT_OK) {
			printk("eventSetDevInt failed\n");
		}
		if(eventSetActive(qd_dev, GT_DEVICE_INT) != GT_OK) {
			printk("eventSetActive failed\n");
		}
	}
#endif /* CONFIG_MV_GTW_LINK_STATUS */

	/* Configure Ethernet related LEDs, currently according to Switch ID */
	switch (qd_dev->deviceId) {
		case GT_88E6171:
		default:
			break; /* do nothing */
	}

	/* done! enable all Switch ports according to the net config table */
	ETH_DBG( ETH_DBG_LOAD, ("enabling: ports "));
	for(p=0; p<qd_dev->numOfPorts; p++) {
		if (!MV_BIT_CHECK(SWITCH_UNUSED_PORTS(num), p)) {
	        	if(gstpSetPortState(qd_dev, p, GT_PORT_FORWARDING) != GT_OK) {
	        		printk("gstpSetPortState failed\n");
	        	}
		}
	}
	ETH_DBG( ETH_DBG_LOAD, ("\n"));
    }
    
    return 0;
}

int mv_gtw_switch_tos_get(int port, unsigned char tos)
{
    unsigned char   queue;
    int             rc;

    /* should be the same for both switches, so we just return what's configured in switch #0 */
    rc = gcosGetDscp2Tc(&qddev[SWITCH_0], tos>>2, &queue);
    if(rc)
        return -1;

    return (int)queue;
}

int mv_gtw_switch_tos_set(int port, unsigned char tos, int queue)
{
    GT_STATUS status;
    status = gcosSetDscp2Tc(&qddev[SWITCH_0], tos>>2, (unsigned char)queue);
    status |= gcosSetDscp2Tc(&qddev[SWITCH_1], tos>>2, (unsigned char)queue);
    return status;
}

static struct net_device* mv_gtw_main_net_dev_get(void)
{
    int                 i;
    mv_eth_priv         *priv;
    struct net_device   *dev;

    for (i=0; i<mv_net_devs_num; i++) {
        dev = mv_net_devs[i];
        priv = MV_ETH_PRIV(dev);

        if (netif_running(dev) && priv->isGtw)
            return dev;
    }
    return NULL;
}

int mv_gtw_set_mac_addr( struct net_device *dev, void *p )
{
    struct mv_vlan_cfg *vlan_cfg = MV_NETDEV_VLAN(dev);
    struct sockaddr *addr = p;

    if(!is_valid_ether_addr(addr->sa_data))
	    return -EADDRNOTAVAIL;

    /* remove old mac addr from VLAN DB */
    mv_gtw_set_mac_addr_to_switch(dev->dev_addr, MV_GTW_VLAN_TO_GROUP(vlan_cfg->vlan_grp_id), 
				(SWITCH_CPU_PORT_MASK(0) | SWITCH_CPU_PORT_MASK(1)), 0);

    memcpy(dev->dev_addr, addr->sa_data, 6);

    /* add new mac addr to VLAN DB */
    mv_gtw_set_mac_addr_to_switch(dev->dev_addr, MV_GTW_VLAN_TO_GROUP(vlan_cfg->vlan_grp_id), 
				(SWITCH_CPU_PORT_MASK(0) | SWITCH_CPU_PORT_MASK(1)), 1);

    printk("mv_gateway: %s change mac address to %02x:%02x:%02x:%02x:%02x:%02x\n", 
        dev->name, *(dev->dev_addr), *(dev->dev_addr+1), *(dev->dev_addr+2), 
       *(dev->dev_addr+3), *(dev->dev_addr+4), *(dev->dev_addr+5));

    return 0;
}

/*
// The code below is used to record which ports are in
// promiscuous mode in each switch(actually only the second switch).
// This is used later in the decision whether to connect the 
// Inter-Connect port of Switch0 to the CPU port of Switch0(if any of the ports in Switch1 is 
// in promiscuous mode, the Inter-Connect port of Switch0 must be connected to the CPU port 
// of Switch0, so packets from the promiscuous mode ports of Switch1 will reach the CPU
*/
unsigned int g_is_switch_port_promisc[(MAX_SWITCHES-1)];

void set_is_switch_port_promisc(unsigned int num, unsigned int ports_mask, unsigned char val) {
	if (num < 1 || num >= (MAX_SWITCHES)) {
		printk(KERN_ERR "Error: set_is_switch_port_promisc got invalid switch num(%d)\n", num);
		return;
	}
	
	ports_mask &= ~(1 << SWITCH_PORT_CPU[num]);
	ports_mask &= ~(1 << SWITCH_PORT_IC[num]);
	
	if (val) {
		g_is_switch_port_promisc[num - 1] |= ports_mask;
	} else {
		g_is_switch_port_promisc[num - 1] &= (~ports_mask);
	}
/*
	printk("%s: Setting porimic bitmap in %d to %x\n", __FUNCTION__, num, g_is_switch_port_promisc[num - 1]);
*/

}

int is_switch_port_promisc(unsigned int num) {
	int i;

	if (num < 1 || num >= (MAX_SWITCHES)) {
		printk(KERN_ERR "Error: is_switch_port_promisc got invalid switch num(%d)\n", num);
		return 0;
	}
	
	return (g_is_switch_port_promisc[num-1]);
}

static int mv_gtw_create_macs(struct net_device *dev);

static void set_port_span(struct net_device* dev, int num, int p, GT_BOOL is_span) {
	int was_span;
	int is_failed = 0;
	GT_U32 db_num;

	if (gprtGetLearnDisable(&qddev[num], p, &was_span) != GT_OK) {
		printk(KERN_ERR "%s: Could not get previous learn mode for port %d on switch %d. Setting anyway", __FUNCTION__,
			p, num);
		is_failed = 1;

	}
	if (is_failed || is_span != was_span) {
		if (gprtSetLearnDisable(&qddev[num], p, is_span) != GT_OK) {
			printk(KERN_ERR "%s: Could not set learn mode for port %d on switch %d.", __FUNCTION__,
				p, num);
		}
	}
	if (is_span && dev) {
		if (gvlnGetPortVlanDBNum(&qddev[num], p, &db_num) == GT_OK) {
			printk("%s: Flushing DBnum %d\n", __FUNCTION__, db_num);
			if (gfdbFlushInDB(&qddev[0], GT_FLUSH_ALL, db_num) != GT_OK) {
				printk("%s: Failed flush get DBNum %d for port %d on switch %d\n", __FUNCTION__, db_num, p, 0);
			}
			if (gfdbFlushInDB(&qddev[1], GT_FLUSH_ALL, db_num) != GT_OK) {
				printk("%s: Failed flush get DBNum %d for port %d on switch %d\n", __FUNCTION__, db_num, p, 1);
			}
			if (dev->flags & IFF_UP) {
				mv_gtw_create_macs(dev);
			}
		} else {
			printk("%s: Failed to get DBNum for port %d on switch %d\n", __FUNCTION__, p, num);
		}
	}

}

static void update_interfaces_for_span() {
	int p,num;
	GT_BOOL is_ic_span = GT_FALSE;

	for (num=0; num < MAX_SWITCHES; num++) {
		for(p=0; p<qddev[num].numOfPorts; p++) {
			if ( (p != SWITCH_PORT_IC[num]) && 
			      p != SWITCH_PORT_CPU[num] &&
				!MV_BIT_CHECK(SWITCH_UNUSED_PORTS(num), p)) {
				struct net_device* dev = mv_gtw_port_to_netdev_map(num, p);

				if (dev) {
					GT_BOOL is_span = br_port_is_flag_set(dev, BR_MONITOR_MODE);

					set_port_span(dev, num, p, is_span);
					if (is_span && num == 1) {
						is_ic_span = GT_TRUE;
					}
				} else {
					//printk(KERN_ERR "%s: could not get device for port %d on switch %d\n", 
							//__FUNCTION__, p, num);
				}

			}
		}
	}


	set_port_span(NULL, 0, SWITCH_PORT_IC[0], is_ic_span);
	set_port_span(NULL, 1, SWITCH_PORT_IC[1], is_ic_span);
}

/*
	This callback is used not just for setting multicast addresses
	but also for setting VLAN table(friends)
*/
void    mv_gtw_set_multicast_list(struct net_device *dev)
{
    struct dev_mc_list *curr_addr = dev->mc_list;
    struct mv_vlan_cfg *vlan_cfg = MV_NETDEV_VLAN(dev);
    int i, num;
    GT_ATU_ENTRY mac_entry;
    GT_BOOL found = GT_FALSE;
    GT_STATUS status;
    unsigned int ports_mask, p;
    unsigned char cnt;
    GT_LPORT port_list[MAX_SWITCH_PORTS];

    update_interfaces_for_span();
	
	
    /*
    // The switches are iterated in reverse order, so the promiscuous bitmask of Switch1
    // is updated before Switch0 is handled
    */
    for (num = MAX_SWITCHES - 1; num >= 0; num--) {
    	if((dev->flags & IFF_PROMISC) || (dev->flags & IFF_ALLMULTI)) {
		/* promiscuous mode - connect the CPU port to the VLAN (port based + 802.1q) */
		/*
		if(dev->flags & IFF_PROMISC)
	    		printk("mv_gateway: setting promiscuous mode\n");
		if(dev->flags & IFF_ALLMULTI)
	    		printk("mv_gateway: setting multicast promiscuous mode\n");
		*/

		ports_mask = ((vlan_cfg->ports_mask >> SWITCH_PORT_OFFS(num)) & SWITCH_PORT_MASK(num));
 		ports_mask |= (1 << SWITCH_PORT_IC[num]);
		ports_mask |= (1 << SWITCH_PORT_CPU[num]); /* promiscuous, so connect CPU port */

		mv_gtw_set_port_based_vlan(num, ports_mask);
		if (num == SWITCH_0) 
		{
			/* set switch #0 inter-connect port with port-based vlan to		*/
			/* other ports including CPU port .					*/
			for(p=0,cnt=0; p<qddev[num].numOfPorts; p++) {
				if ( (p != SWITCH_PORT_IC[num]) && !MV_BIT_CHECK(SWITCH_UNUSED_PORTS(num), p)) {
					port_list[cnt] = p;
					cnt++;
				}
			}
			if( gvlnSetPortVlanPorts(&qddev[num], SWITCH_PORT_IC[num], port_list, cnt) != GT_OK) {
				printk("gvlnSetPortVlanPorts failed\n");
			}
		}
		if (num != SWITCH_0) {
			/*
			// Update switch promiscuous mode port bitmask
			*/
			set_is_switch_port_promisc(num, ports_mask, 1);
		}
			
			
    	}
    	else 
    	{
		/* not in promiscuous or allmulti mode - disconnect the CPU port to the VLAN (port based + 802.1q) */

		ports_mask = ((vlan_cfg->ports_mask >> SWITCH_PORT_OFFS(num)) & SWITCH_PORT_MASK(num));
		ports_mask |= (1 << SWITCH_PORT_IC[num]);
		if (num == SWITCH_0)
			ports_mask &= ~(1 << SWITCH_PORT_CPU[num]);

		mv_gtw_set_port_based_vlan(num, ports_mask);

		if (num != SWITCH_0) {
			/*
			// Update switch promiscuous mode port bitmask
			*/
			set_is_switch_port_promisc(num, ports_mask, 0);
		}
		
		if (num == SWITCH_0) 
		{
			int is_promisc = is_switch_port_promisc(SWITCH_1);
			/* set switch #0 inter-connect port with port-based vlan to		*/
			/* other ports in switch #0 which can be part of a cross-switch vlan.	*/
			/* Also connect it to the CPU port, if any of the ports in Switch1 are  */
			/* in promicuous mode(so packets from thses ports will reach the CPU)   */
			for(p=0,cnt=0; p<qddev[num].numOfPorts; p++) {
				if (	(p != SWITCH_PORT_IC[num]) && 
					(is_promisc || p != SWITCH_PORT_CPU[num]) && 
					!MV_BIT_CHECK(SWITCH_UNUSED_PORTS(num), p) ) {
					port_list[cnt] = p;
					cnt++;
				}
			}
			if( gvlnSetPortVlanPorts(&qddev[num], SWITCH_PORT_IC[num], port_list, cnt) != GT_OK) {
				printk("gvlnSetPortVlanPorts failed\n");
			}
		}
    	}
    }

	if(dev->mc_count && (! ((dev->flags & IFF_PROMISC) || (dev->flags & IFF_ALLMULTI)))) {
			/* accept specific multicasts */
			for(i=0; i<dev->mc_count; i++, curr_addr = curr_addr->next) {
				if (!curr_addr)
					break;
/*				
				printk("%s: IM HERE 1, %02x:%02x:%02x:%02x:%02x:%02x %x %x\n", __FUNCTION__,
					mac_entry.macAddr.arEther[0],mac_entry.macAddr.arEther[1],mac_entry.macAddr.arEther[2],
						mac_entry.macAddr.arEther[3],mac_entry.macAddr.arEther[4],mac_entry.macAddr.arEther[5],
					(SWITCH_CPU_PORT_MASK(0)|SWITCH_CPU_PORT_MASK(1)|(vlan_cfg->ports_mask)), MV_GTW_VLAN_TO_GROUP(vlan_cfg->vlan_grp_id));
*/	
				mv_gtw_set_mac_addr_to_switch(curr_addr->dmi_addr,
				MV_GTW_VLAN_TO_GROUP(vlan_cfg->vlan_grp_id),
				(SWITCH_CPU_PORT_MASK(0)|SWITCH_CPU_PORT_MASK(1)|(vlan_cfg->ports_mask)), 1);
			}
	}
	
}
 

int mv_gtw_change_mtu(struct net_device *dev, int mtu)
{
	GT_JUMBO_MODE       jumbo_mode;
	int i, num;
	mv_eth_priv *priv = MV_ETH_PRIV(dev);

	if (netif_running(dev)) {
		printk("mv_gateway does not support changing MTU for active interface.\n"); 
		return -EPERM;
	}

	/* check mtu - can't change mtu if there is a gateway interface that */
	/* is currently up and has a different mtu */
	for (i = 0; i < mv_net_devs_num; i++) {		
		if (	(MV_ETH_PRIV(mv_net_devs[i]) == priv) && 
			(mv_net_devs[i]->mtu != mtu) && 
			(netif_running(mv_net_devs[i])) ) 
		{		
			printk(KERN_ERR "All gateway devices must have same MTU\n");
			return -EPERM;
		}
	}

	if (dev->mtu != mtu) {
		int old_mtu = dev->mtu;

		/* stop tx/rx activity, mask all interrupts, relese skb in rings,*/
		mv_eth_stop_internals(priv);

		if (mv_eth_change_mtu_internals(dev, mtu) == -1) 
			return -EPERM;

		/* fill rx buffers, start rx/tx activity, set coalescing */
		if(mv_eth_start_internals(priv, dev->mtu) != 0 ) {
			printk( KERN_ERR "%s: start internals failed\n", dev->name );
			return -EPERM;
		}

		printk( KERN_NOTICE "%s: change mtu %d (buffer-size %d) to %d (buffer-size %d)\n",
			dev->name, old_mtu, MV_RX_BUF_SIZE(old_mtu), 
			dev->mtu, MV_RX_BUF_SIZE(dev->mtu) );
	}

	if (gtw_config.mtu != mtu) {				
		/* Setup jumbo frames mode.                         */
		if (MV_RX_BUF_SIZE(mtu) <= 1522)
			jumbo_mode = GT_JUMBO_MODE_1522;
		else if (MV_RX_BUF_SIZE(mtu) <= 2048)
			jumbo_mode = GT_JUMBO_MODE_2048;
		else
			jumbo_mode = GT_JUMBO_MODE_10240;

		for (num = 0; num < MAX_SWITCHES; num++) {
			for(i = 0; i < qddev[num].numOfPorts; i++) {
				if (!MV_BIT_CHECK(SWITCH_UNUSED_PORTS(num), i)) {
					if (gsysSetJumboMode(&qddev[num], i, jumbo_mode) != GT_OK) {
						printk("gsysSetJumboMode %d failed for port %d\n", jumbo_mode, i);
					}
				}
			}
		}
		gtw_config.mtu = mtu;
	}		
	return 0;
} 

#ifdef CONFIG_MV_GTW_LINK_STATUS
static void mv_gtw_update_link_status(int qd_num, struct net_device* dev, unsigned int p)
{
    struct mv_vlan_cfg  *vlan_cfg = MV_NETDEV_VLAN(dev);
    GT_BOOL             link;
    unsigned int        prev_ports_link = 0;

    if(gprtGetLinkState(&qddev[qd_num], p, &link) != GT_OK) {
        printk("gprtGetLinkState failed (port %d)\n", p);
        return;
    }

	//limor - chnage this to reading adsl sync?
	if (strncmp(dev->name, "DSL", 3)==0) {
		printk("mv_gtw_update_link_status: forcing ADSL link to seem UP");	
		link = GT_TRUE;
	}

    prev_ports_link = vlan_cfg->ports_link;
    if (link)
        vlan_cfg->ports_link |= ((1 << p) << SWITCH_PORT_OFFS(qd_num));
    else
	vlan_cfg->ports_link &= ~((1 << p) << SWITCH_PORT_OFFS(qd_num));

    if(vlan_cfg->ports_link == 0) {
        if(prev_ports_link != 0) {
            netif_carrier_off(dev);
            netif_stop_queue(dev);
            printk(KERN_DEBUG "%s: interface down\n", dev->name );
        }
    }
    else if (prev_ports_link == 0) {
        netif_carrier_on(dev);
        netif_wake_queue(dev);          
        printk(KERN_DEBUG "%s: interface up\n", dev->name );
    }	    
}

static void mv_gtw_link_status_print(int qd_num, unsigned int port)
{
    char                *duplex_str = NULL, *speed_str = NULL;
    GT_BOOL             link, duplex;
    GT_PORT_SPEED_MODE  speed_mode;
 
    if(gprtGetLinkState(&qddev[qd_num], port, &link) != GT_OK) {
        printk("gprtGetLinkState failed (switch %d, port %d)\n", qd_num, port);
        return;
    }

    if(link) {
        if(gprtGetDuplex(&qddev[qd_num], port, &duplex) != GT_OK) {
	    printk("gprtGetDuplex failed (switch %d, port %d)\n", qd_num, port);
	    duplex_str = "ERR";
	}
        else 
            duplex_str = (duplex) ? "Full" : "Half";

	if(gprtGetSpeedMode(&qddev[qd_num], port, &speed_mode) != GT_OK) {
	    printk("gprtGetSpeedMode failed (switch %d, port %d)\n", qd_num, port);
	    speed_str = "ERR";
	}
	else {
	    if (speed_mode == PORT_SPEED_1000_MBPS)
	        speed_str = "1000Mbps";
	    else if (speed_mode == PORT_SPEED_100_MBPS)
	        speed_str = "100Mbps";
	    else
	        speed_str = "10Mbps";
	}
        printk("%s: Link-up, Duplex-%s, Speed-%s.\n",
               mv_gtw_port2lport(port + SWITCH_PORT_OFFS(qd_num)), duplex_str, speed_str);
    }
    else
    {
        printk("%s: Link-down\n", mv_gtw_port2lport(port + SWITCH_PORT_OFFS(qd_num)));
    }
}

static irqreturn_t mv_gtw_link_interrupt_handler(int irq , void *dev_id)
{
    unsigned short switch_cause, phy_cause, p, switch_ind;
    GT_DEV_INT_STATUS devIntStatus;
    switch_cause = GT_PHY_INTERRUPT;

    if(switch_cause & GT_PHY_INTERRUPT) {
	/* go over all connected switches */
	for (switch_ind = 0; switch_ind < MAX_SWITCHES; switch_ind++) {
	    /* go over all switch ports */
            for(p = 0; p < qddev[switch_ind].numOfPorts; p++) {
		/* if switch port is connected */
                if (MV_BIT_CHECK(SWITCH_PORT_MASK(switch_ind), p)) {
		    if(gprtGetPhyIntStatus(&qddev[switch_ind],p,&phy_cause) == GT_OK) {
		        if(phy_cause & GT_LINK_STATUS_CHANGED) 
                    	{
                            struct net_device* dev = mv_gtw_port_to_netdev_map(switch_ind, p);

                            if( (dev != NULL) && (netif_running(dev)) )
	                        mv_gtw_update_link_status(switch_ind, dev, p);

                            mv_gtw_link_status_print(switch_ind, p);
			}
		    }
		}
	    }
	      /* Clear interrupt */
      		if (geventGetDevIntStatus(&qddev[switch_ind],&devIntStatus) != GT_OK)
          		printk("geventGetDevIntStatus failed\n");

	}
    }

    if (switch_irq == -1 ) {
    	switch_link_timer.expires = jiffies + (HZ); /* 1 second */
    	add_timer(&switch_link_timer);
    }

    return IRQ_HANDLED;
}

static void mv_gtw_link_timer_function(unsigned long data)
{
    mv_gtw_link_interrupt_handler(switch_irq, NULL);
}
#endif /* CONFIG_MV_GTW_LINK_STATUS */


static int mv_gtw_create_macs(struct net_device *dev) {
	struct		mv_vlan_cfg *vlan_cfg = MV_NETDEV_VLAN(dev);

	unsigned char	broadcast[6] = {0xff,0xff,0xff,0xff,0xff,0xff};
	

	/* Add our MAC addr to the VLAN DB at switch level to forward packets with this DA */
	/* to CPU port. The device is always in promisc mode.	   */
	mv_gtw_set_mac_addr_to_switch(dev->dev_addr, MV_GTW_VLAN_TO_GROUP(vlan_cfg->vlan_grp_id), 
			(SWITCH_CPU_PORT_MASK(0) | SWITCH_CPU_PORT_MASK(1)), 1);

	/* We also need to allow L2 broadcasts comming up for this interface */
	mv_gtw_set_mac_addr_to_switch(broadcast, MV_GTW_VLAN_TO_GROUP(vlan_cfg->vlan_grp_id), 
					(vlan_cfg->ports_mask | SWITCH_CPU_PORT_MASK(0) | SWITCH_CPU_PORT_MASK(1)), 1);
	

}

int mv_gtw_recreate_macs( struct net_device *dev ) {
	mv_gtw_create_macs(dev);
	mv_gtw_set_multicast_list(dev);
}

EXPORT_SYMBOL(mv_gtw_recreate_macs);


int mv_gtw_start( struct net_device *dev )
{
    mv_eth_priv		*priv = MV_ETH_PRIV(dev);
    struct		mv_vlan_cfg *vlan_cfg = MV_NETDEV_VLAN(dev);
    extern struct net_device** mv_net_devs;
    extern int mv_net_devs_num;
    int i;
    unsigned short p, switch_ind;

    printk(KERN_DEBUG "mv_gateway: starting %s\n",dev->name);

    /* start upper layer accordingly with ports_map */
#ifdef CONFIG_MV_GTW_LINK_STATUS
	vlan_cfg->ports_link = 0;

	/* go over all connected switches */
	for (switch_ind = 0; switch_ind < MAX_SWITCHES; switch_ind++) {
		/* go over all switch ports */
		for(p = 0; p < qddev[switch_ind].numOfPorts; p++) {
			/* if switch port is connected */
			if ( MV_BIT_CHECK( ((vlan_cfg->ports_mask >> SWITCH_PORT_OFFS(switch_ind)) & SWITCH_PORT_MASK(switch_ind)), p)) {
				mv_gtw_update_link_status(switch_ind, dev, p);
			}
		}
	}
#else
	vlan_cfg->ports_link = vlan_cfg->ports_mask;
#endif

    /* check mtu */
    for (i = 0; i < mv_net_devs_num; i++) {		
	if (MV_ETH_PRIV(mv_net_devs[i]) == priv && mv_net_devs[i]->mtu != dev->mtu)
	{		
		printk(KERN_ERR "All gateway devices must have same MTU\n");
		return -EPERM;
	}
    }

    netif_poll_enable(dev);

	mv_gtw_create_macs(dev);

    if(priv->timer_flag == 0)
    {
        priv->timer.expires = jiffies + ((HZ*CONFIG_MV_ETH_TIMER_PERIOD)/1000); /*ms*/
        add_timer( &(priv->timer) );
        priv->timer_flag = 1;
    }

    if( (priv->net_dev == dev) || !netif_running(priv->net_dev) )
    {
        priv->net_dev = dev;

	/* connect to MAC port interrupt line */
    	if( request_irq( ETH_PORT_IRQ_NUM(priv->port), mv_eth_interrupt_handler, 
            (IRQF_DISABLED | IRQF_SAMPLE_RANDOM), "mv_gateway", priv) ) 
	{
        	printk(KERN_ERR "failed to assign irq%d\n", ETH_PORT_IRQ_NUM(priv->port));
    	}

        /* unmask interrupts */
        mv_eth_unmask_interrupts(priv);
    }
    
    return 0;
}


int mv_gtw_stop( struct net_device *dev )
{
    mv_eth_priv		    *priv = MV_ETH_PRIV(dev);
    struct mv_vlan_cfg	*vlan_cfg = MV_NETDEV_VLAN(dev);
    int num;

    printk(KERN_DEBUG "mv_gateway: stopping %s\n",dev->name);

    /* stop upper layer */
    netif_poll_disable(dev);
    netif_carrier_off(dev);
    netif_stop_queue(dev);

    for (num = 0; num < MAX_SWITCHES; num++) {
    	/* stop switch from forwarding packets from this VLAN toward CPU port */
    	if( gfdbFlushInDB(&qddev[num], GT_FLUSH_ALL, MV_GTW_VLAN_TO_GROUP(vlan_cfg->vlan_grp_id)) != GT_OK) {
        	printk("gfdbFlushInDB failed\n");
    	}
    }

    if(priv->net_dev == dev)
    {
        struct net_device *main_dev = mv_gtw_main_net_dev_get();

        if(main_dev == NULL)
        {
            mv_eth_mask_interrupts(priv);
    	    priv->timer_flag = 0;
            del_timer(&priv->timer);

            free_irq( dev->irq, priv );
        }
        else
        {
            priv->net_dev = main_dev;
        }
    }
    return 0;
}


/*********************************************************** 
 * gtw_init_complete --                                    *
 *   complete all initializations relevant for Gateway.    *
 ***********************************************************/
int  __init mv_gtw_init_complete(mv_eth_priv* priv)
{
    int status = 0;

    status = mv_switch_init(priv->port);
    if (status != 0)
	return status;

    status = mv_eth_start_internals(priv, gtw_config.mtu);
    if (status != 0)
	return status;

    status = mvEthHeaderModeSet(priv->hal_priv, MV_ETH_DISABLE_HEADER_MODE);
    if (status != 0)
	return status;

    status = mvEthDSAModeSet(priv->hal_priv, MV_ETH_DSA_TAG_NON_EXTENDED);
    if (status != 0)
	return status;
    
    /* Mask interrupts */
    mv_eth_mask_interrupts(priv);

#ifdef CONFIG_MV_GTW_IGMP
    /* Initialize the IGMP snooping handler */
    if(mv_gtw_igmp_snoop_init()) {
        printk("failed to init IGMP snooping handler\n");
    }
#endif

#ifdef CONFIG_MV_GTW_LINK_STATUS
    if (switch_irq != -1) {
        if(request_irq(switch_irq, mv_gtw_link_interrupt_handler, 
            (IRQF_DISABLED | IRQF_SAMPLE_RANDOM), "link status", NULL))
	    {
                printk(KERN_ERR "failed to assign irq%d\n", switch_irq);
	    }
        if(request_irq(switch_irq+1, mv_gtw_link_interrupt_handler, 
            (IRQF_DISABLED | IRQF_SAMPLE_RANDOM), "link status", NULL))
	    {
                printk(KERN_ERR "failed to assign irq%d\n", switch_irq);
	    }
    }
    else {
        memset( &switch_link_timer, 0, sizeof(struct timer_list) );
	init_timer(&switch_link_timer);
        switch_link_timer.function = mv_gtw_link_timer_function;
        switch_link_timer.data = -1;
   	switch_link_timer.expires = jiffies + (HZ); /* 1 second */
    	add_timer(&switch_link_timer);
    }
#endif /* CONFIG_MV_GTW_LINK_STATUS */

    return 0;
}

void    mv_gtw_status_print(void)
{
    mv_eth_priv         *priv = gtw_config.priv;
    struct mv_vlan_cfg  *vlan_cfg;
    int                 i;

    printk("Gateway Giga port #%d, priv=%p, vlans_num=%d, mtu=%d, switch_enabled_ports=0x%x\n",
            priv->port, priv, gtw_config.vlans_num, gtw_config.mtu, switch_enabled_ports);

    for(i=0; i<MV_GTW_MAX_NUM_OF_IFS; i++)
    {
        vlan_cfg = &gtw_config.vlan_cfg[i];
        if(vlan_cfg->vlan_grp_id == 0)
            continue;

        printk("%s: vlan_grp_id=0x%x, ports_mask=0x%x, ports_link=0x%x, mv_header=0x%04x\n",
                vlan_cfg->net_dev->name, vlan_cfg->vlan_grp_id, vlan_cfg->ports_mask, 
                vlan_cfg->ports_link, vlan_cfg->header);
    }
}

#if 0
/* map from virtual port number (0..8) to switch number (0..1) and switch port number */
/* Note this is specific to CP board Cut-3 */
int	mv_gtw_map_port_to_switch_port(int port, int *switch_num, int *phys_port)
{
    if ((port < 0) || (port > 8))
	return -1;

    switch(port) {
	case 0: /* LAN1 */
	    *switch_num = 0;
	    *phys_port = 1;
	    return 0;
	case 1: /* LAN2 */
	    *switch_num = 0;
	    *phys_port = 3;
	    return 0;
	case 2: /* LAN3 */
	    *switch_num = 1;
	    *phys_port = 0;
	    return 0;
	case 3: /* LAN4 */
	    *switch_num = 1;
	    *phys_port = 2;
	    return 0;
	case 4: /* LAN5 */
	    *switch_num = 0;
	    *phys_port = 0;
	    return 0;
	case 5: /* LAN6 */
	    *switch_num = 0;
	    *phys_port = 2;
	    return 0;
	case 6: /* LAN7 */
	    *switch_num = 0;
	    *phys_port = 4;
	    return 0;
	case 7: /* LAN8 */
	    *switch_num = 1;
	    *phys_port = 1;
	    return 0;
	case 8: /* WAN1 */
	    *switch_num = 1;
	    *phys_port = 3;
	    return 0;
	default:
	    return -1;
    }
}

#endif

int     mv_gtw_switch_port_add(struct net_device* dev, int port)
{
    struct mv_vlan_cfg  *vlan_cfg = MV_NETDEV_VLAN(dev);
    int			switch_num, switch_port;
    unsigned int	ports_mask;

    if(vlan_cfg == NULL)
    {
        printk("%s is not connected to switch\n", dev->name);
        return 1;
    }
    if (netif_running(dev) )
    {
        printk("%s must be down to change switch ports map\n", dev->name);
        return 1;
    }

    if(mvPortMap(port, &switch_num, &switch_port) != 0)
    {
        printk("Switch port %d can't be added\n", port);
        return 1;        
    }
    if(MV_BIT_CHECK( ((switch_enabled_ports >> SWITCH_PORT_OFFS(switch_num)) & SWITCH_PORT_MASK(switch_num)), switch_port) )
    {
        ETH_DBG(ETH_DBG_GTW, ("Switch port %d is already enabled\n", port));
        return 0;
    }

    /* Set default VLAN_ID for port */
    if(gvlnSetPortVid(&qddev[switch_num], switch_port, vlan_cfg->vlan_grp_id) != GT_OK ) {
        printk("gvlnSetPortVid failed");
	return -1;
    }
    /* Map port to VLAN DB */
    if(gvlnSetPortVlanDBNum(&qddev[switch_num], switch_port, MV_GTW_VLAN_TO_GROUP(vlan_cfg->vlan_grp_id)) != GT_OK) {
	printk("gvlnSetPortVlanDBNum failed\n");
	return -1;
    }

    /* Update data base */
    vlan_cfg->ports_mask |= ((1 << switch_port) << SWITCH_PORT_OFFS(switch_num));
    switch_enabled_ports |= ((1 << switch_port) << SWITCH_PORT_OFFS(switch_num));
    vlan_cfg->header = vlan_cfg->vlan_grp_id;

    /* Add port to the VLAN (CPU port is not part of VLAN) */
    ports_mask = ((vlan_cfg->ports_mask >> SWITCH_PORT_OFFS(switch_num)) & SWITCH_PORT_MASK(switch_num));
    ports_mask |= (1 << SWITCH_PORT_IC[switch_num]);
    if (switch_num == SWITCH_0)
	ports_mask &= ~(1 << SWITCH_PORT_CPU[switch_num]);

    if(mv_gtw_set_port_based_vlan(switch_num, ports_mask) != 0) {
        printk("mv_gtw_set_port_based_vlan failed\n");
    }

    /* Add port to vtu (used in tx) */
    if(mv_gtw_set_vlan_in_vtu(switch_num, vlan_cfg->vlan_grp_id, MV_GTW_VLAN_TO_GROUP(vlan_cfg->vlan_grp_id),
		(((vlan_cfg->ports_mask >> SWITCH_PORT_OFFS(switch_num)) & SWITCH_PORT_MASK(switch_num)) 
		| (1 << SWITCH_PORT_CPU[switch_num]) | (1 << SWITCH_PORT_IC[switch_num]) ), 1)) {
        printk("mv_gtw_set_vlan_in_vtu failed\n");
    }

    /* Enable port */
    if(gstpSetPortState(&qddev[switch_num], switch_port, GT_PORT_FORWARDING) != GT_OK) {
        printk("gstpSetPortState failed\n");
    }

#ifdef CONFIG_MV_GTW_LINK_STATUS
    /* Enable Phy Link Status Changed interrupt at Phy level for the port */
    if(gprtPhyIntEnable(&qddev[switch_num], switch_port, (GT_LINK_STATUS_CHANGED)) != GT_OK) {
        printk("gprtPhyIntEnable failed port %d\n", switch_port);
    }
#endif /* CONFIG_MV_GTW_LINK_STATUS */

    printk(KERN_DEBUG "%s: Switch port #%d mapped\n", dev->name, port);

    return 0;
}


int     mv_gtw_switch_port_del(struct net_device* dev, int port)
{
    struct mv_vlan_cfg  *vlan_cfg = MV_NETDEV_VLAN(dev);
    int			switch_num, switch_port;
    unsigned int	ports_mask;

    if(vlan_cfg == NULL)
    {
        printk("%s aren't connected to switch\n", dev->name);
        return 1;
    }
    if (netif_running(dev) )
    {
        printk("%s must be down to change switch ports map\n", dev->name);
        return 1;
    }

    if(mvPortMap(port, &switch_num, &switch_port) != 0)
    {
        printk("Switch port %d can't be deleted\n", port);
        return 1;        
    }
    if(!MV_BIT_CHECK( ((switch_enabled_ports >> SWITCH_PORT_OFFS(switch_num)) & SWITCH_PORT_MASK(switch_num)), switch_port) )
    {
        printk(KERN_DEBUG "Switch port %d is disabled\n", port);
        return 1;
    }
    if(!MV_BIT_CHECK( ((vlan_cfg->ports_mask >> SWITCH_PORT_OFFS(switch_num)) & SWITCH_PORT_MASK(switch_num)), switch_port) )
    {
        ETH_DBG(ETH_DBG_GTW, ("Switch port %d is already not mapped on %s\n", port, dev->name));
        return 0;
    }

#ifdef CONFIG_MV_GTW_LINK_STATUS
    /* Disable link change interrupts on unmapped port */
    if(gprtPhyIntEnable(&qddev[switch_num], switch_port, 0) != GT_OK) {
        printk("gprtPhyIntEnable failed on port #%d\n", switch_port);
    }
    vlan_cfg->ports_link &= ~((1 << switch_port) << SWITCH_PORT_OFFS(switch_num));
#endif /* CONFIG_MV_GTW_LINK_STATUS */

    /* Disable unmapped port */
    if(gstpSetPortState(&qddev[switch_num], switch_port, GT_PORT_DISABLE) != GT_OK) {
        printk("gstpSetPortState failed on port #%d\n",  port);
    }
    /* Update data base */
    vlan_cfg->ports_mask &= ~((1 << switch_port) << SWITCH_PORT_OFFS(switch_num));
    switch_enabled_ports &= ~((1 << switch_port) << SWITCH_PORT_OFFS(switch_num));
    vlan_cfg->header = vlan_cfg->vlan_grp_id;
    
    /* Remove port from the VLAN (CPU port is not part of VLAN) */
    ports_mask = ((vlan_cfg->ports_mask >> SWITCH_PORT_OFFS(switch_num)) & SWITCH_PORT_MASK(switch_num));
    ports_mask |= (1 << SWITCH_PORT_IC[switch_num]);
    if (switch_num == SWITCH_0)
	ports_mask &= ~(1 << SWITCH_PORT_CPU[switch_num]);

    if(mv_gtw_set_port_based_vlan(switch_num, ports_mask) != 0) {
	        printk("mv_gtw_set_port_based_vlan failed\n");
    }

    /* Remove port from vtu (used in tx) */
    if(mv_gtw_set_vlan_in_vtu(switch_num, vlan_cfg->vlan_grp_id, MV_GTW_VLAN_TO_GROUP(vlan_cfg->vlan_grp_id),
		(((vlan_cfg->ports_mask >> SWITCH_PORT_OFFS(switch_num)) & SWITCH_PORT_MASK(switch_num)) 
		| (1 << SWITCH_PORT_CPU[switch_num]) | (1 << SWITCH_PORT_IC[switch_num]) ), 1)) {
        printk("mv_gtw_set_vlan_in_vtu failed\n");
    }

    printk(KERN_DEBUG "%s: Switch port #%d unmapped\n", dev->name, port);

    return 0;
}


#define QD_FMT "%10lu %10lu %10lu %10lu %10lu %10lu %10lu\n"
#define QD_CNT(c,f) c[0].f, c[1].f, c[2].f, c[3].f, c[4].f, c[5].f, c[6].f
#define QD_MAX 7
void    mv_gtw_switch_stats(int port)
{
    GT_STATS_COUNTER_SET3 counters[QD_MAX];
    int p, num;

    for (num = 0; num < MAX_SWITCHES; num++) {

    	if(&qddev[num] == NULL) {
        	printk("Switch is not initialized\n");
        	return;
    	}
    	memset(counters, 0, sizeof(GT_STATS_COUNTER_SET3) * QD_MAX);

    	for (p=0; p<QD_MAX; p++)
        	gstatsGetPortAllCounters3(&qddev[num], p, &counters[p]);

	printk("Switch #%d\n", num);

    	printk("PortNum         " QD_FMT,  (GT_U32)0, (GT_U32)1, (GT_U32)2, (GT_U32)3, (GT_U32)4, (GT_U32)5, (GT_U32)6);
    	printk("-----------------------------------------------------------------------------------------------\n");
    	printk("InGoodOctetsLo  " QD_FMT,  QD_CNT(counters,InGoodOctetsLo));
    	printk("InGoodOctetsHi  " QD_FMT,  QD_CNT(counters,InGoodOctetsHi));
    	printk("InBadOctets     " QD_FMT,  QD_CNT(counters,InBadOctets));
    	printk("OutFCSErr       " QD_FMT,  QD_CNT(counters,OutFCSErr));
    	printk("Deferred        " QD_FMT,  QD_CNT(counters,Deferred));
    	printk("InBroadcasts    " QD_FMT,  QD_CNT(counters,InBroadcasts));
    	printk("InMulticasts    " QD_FMT,  QD_CNT(counters,InMulticasts));
    	printk("Octets64        " QD_FMT,  QD_CNT(counters,Octets64));
    	printk("Octets127       " QD_FMT,  QD_CNT(counters,Octets127));
    	printk("Octets255       " QD_FMT,  QD_CNT(counters,Octets255));
    	printk("Octets511       " QD_FMT,  QD_CNT(counters,Octets511));
    	printk("Octets1023      " QD_FMT,  QD_CNT(counters,Octets1023));
    	printk("OctetsMax       " QD_FMT,  QD_CNT(counters,OctetsMax));
    	printk("OutOctetsLo     " QD_FMT,  QD_CNT(counters,OutOctetsLo));
    	printk("OutOctetsHi     " QD_FMT,  QD_CNT(counters,OutOctetsHi));
    	printk("OutUnicasts     " QD_FMT,  QD_CNT(counters,OutOctetsHi));
    	printk("Excessive       " QD_FMT,  QD_CNT(counters,Excessive));
    	printk("OutMulticasts   " QD_FMT,  QD_CNT(counters,OutMulticasts));
    	printk("OutBroadcasts   " QD_FMT,  QD_CNT(counters,OutBroadcasts));
    	printk("Single          " QD_FMT,  QD_CNT(counters,OutBroadcasts));
    	printk("OutPause        " QD_FMT,  QD_CNT(counters,OutPause));
    	printk("InPause         " QD_FMT,  QD_CNT(counters,InPause));
    	printk("Multiple        " QD_FMT,  QD_CNT(counters,InPause));
    	printk("Undersize       " QD_FMT,  QD_CNT(counters,Undersize));
    	printk("Fragments       " QD_FMT,  QD_CNT(counters,Fragments));
    	printk("Oversize        " QD_FMT,  QD_CNT(counters,Oversize));
    	printk("Jabber          " QD_FMT,  QD_CNT(counters,Jabber));
    	printk("InMACRcvErr     " QD_FMT,  QD_CNT(counters,InMACRcvErr));
    	printk("InFCSErr        " QD_FMT,  QD_CNT(counters,InFCSErr));
    	printk("Collisions      " QD_FMT,  QD_CNT(counters,Collisions));
    	printk("Late            " QD_FMT,  QD_CNT(counters,Late));
	printk("\n");

    	gstatsFlushAll(&qddev[num]);
    }
}


struct mv_port_info
{
	int dev_num;			    /* The switch number (0 or 1 in CUT3) */
	int physical_port_num;  /* The physical port number on the internal switch */
};

#define MAX_LOGICAL_SWITCH_PORTS 10 /* Including the DMZ and the DSL port */

static struct mv_port_info mv_port_map[MAX_LOGICAL_SWITCH_PORTS] = 
{
	/* LAN1 */
	{ 0 , 1 },
	
	/* LAN2 */
	{ 0 , 3 },

	/* LAN3 */
	{ 1 , 0 },

	/* LAN4 */
	{ 1 , 2 },

	/* LAN5 */
	{ 0 , 0 },

	/* LAN6 */
	{ 0 , 2 },

	/* LAN7 */
	{ 0 , 4 },

	/* LAN8 */
	{ 1, 1 },

	/* DMZ */
	{ 1 , 3 },

	/* DSL */
	{ 1 , 6 }
};



static int mv_logical_to_phy_port[MAX_SWITCHES][MAX_SWITCH_PORTS] =
{
	/* Switch 0 */
	{  4, 0, 5, 1, 6, -1, -1, -1, -1, -1, -1  },
		
	/* Switch 1 */
	{  2, 7, 3, 8, -1, -1, 9, -1, -1, -1, -1 }
};


	

MV_STATUS mvPortMap(int port_num, int* dev_num, int* physical_port_num)
{
	if (port_num >= 0 && port_num < MAX_LOGICAL_SWITCH_PORTS)
	{
		*dev_num = mv_port_map[port_num].dev_num;
		*physical_port_num = mv_port_map[port_num].physical_port_num;
		return MV_OK;
	}
	
	if (port_num == MAX_LOGICAL_SWITCH_PORTS) /* The WAN port */
	{
		printk("mvPortMap: Error: WAN port is not part of switch\n");
		return MV_ERROR;
	}

	printk("mvPortMap: Wrong port number: %d\n", port_num);
	return MV_ERROR;
}

static int mvPortReverseMap(int switch_num, int phy_port_num)
{
	if (switch_num < 0 || switch_num >= MAX_SWITCHES)
	{
		printk("mvPortReverseMap: invalid switch number\n");
		return -1;
	}

	if (phy_port_num < 0 || phy_port_num >= MAX_SWITCH_PORTS)
	{
		printk("mvPortReverseMap: invalid physical port number\n");
		return -1;
	}

	return mv_logical_to_phy_port[switch_num][phy_port_num];
}

short mvNetDeviceGet(int switch_num, int phy_port_num)
{
	int i;
	int port_map = (1 << phy_port_num);

	port_map = (port_map << SWITCH_PORT_OFFS(switch_num));

	ETH_DBG(ETH_DBG_GTW, ("%s: port_map=0x%x\n", __FUNCTION__, port_map));

	for (i = 0 ; i < MV_GTW_MAX_NUM_OF_IFS ; i++)
	{
		if (gtw_config.vlan_cfg[i].ports_mask & port_map) /* If member of VLAN */
		{
			ETH_DBG(ETH_DBG_GTW, ("%s: Returning %d, port_mask=0x%x i=%d\n", __FUNCTION__, gtw_config.vlan_cfg[i].vlan_grp_id, gtw_config.vlan_cfg[i].ports_mask, i));
			return gtw_config.vlan_cfg[i].vlan_grp_id;
		}
	}

	printk("%s: Device not found\n", __FUNCTION__);
	return -1;
}

GT_QD_DEV * getDriverParams(int dev_num)
{ 
	if (dev_num >= MAX_SWITCHES)
	{
		printk("Error: wrong device number provided: %d\n", dev_num);
		return NULL;
	}
	return &qddev[dev_num];
}

MV_STATUS mv_gateway_set_vtu(int port, int tag, int add)
{
	int i = 0;
	int switch_num;
	int phy_port_num;
	unsigned int port_mask = 0;
	short	group_id = 0;

	if (mvPortMap(port, &switch_num, &phy_port_num) < 0)
	{
		printk("%s: Error mapping port.\n", __FUNCTION__);
		return MV_FAIL;
	}

	group_id = mvNetDeviceGet(switch_num, phy_port_num);
	if (group_id < 0)
	{
		printk("%s: Error getting group ID.\n", __FUNCTION__);
		return MV_FAIL;
	}

	for (i = 0 ; i < MAX_SWITCHES ; i++)
	{
		port_mask = ( (1 << SWITCH_PORT_IC[i]) | (1 << SWITCH_PORT_CPU[i])); /* For switch 0, ports 5,6, for switch 1, port 5 */
		if (i == switch_num)
		{
			port_mask |= (1 << phy_port_num);
		}

		if (mv_gtw_set_vlan_in_vtu(i, (unsigned short)tag, MV_GTW_VLAN_TO_GROUP(group_id),  port_mask, add) < 0)
		{
			printk("%s: Error setting vtu.\n", __FUNCTION__);
			return MV_FAIL;
		}
	}
	printk("VLAN tag %d is now %sset for port %d.\n", tag, add ? "" : "un", port);
	return MV_OK;
}

EXPORT_SYMBOL(getDriverParams);
EXPORT_SYMBOL(mvPortMap);
EXPORT_SYMBOL(mvNetDeviceGet);
EXPORT_SYMBOL(mv_gateway_set_vtu);
EXPORT_SYMBOL(mv_gtw_port_to_netdev_map);
EXPORT_SYMBOL(mv_gtw_switch_port_add);
EXPORT_SYMBOL(mv_gtw_switch_port_del);

//
// All kinds of statistics
//
GT_STATUS sampleDisplayVIDTable(GT_QD_DEV *dev)
{
    GT_STATUS status;
	GT_VTU_ENTRY vtuEntry;
	GT_LPORT port;
	int portIndex;

	gtMemSet(&vtuEntry, 0, sizeof(GT_VTU_ENTRY));
	vtuEntry.vid = 0xfff;
	if ((status = gvtuGetEntryFirst(dev, &vtuEntry)) != GT_OK)
	{
		printk( "gvtuGetEntryCount returned fail.\n");
		return status;
	}

	printk( "DBNum:%d, VID:%d \n", vtuEntry.DBNum, vtuEntry.vid);

	for (portIndex = 0; portIndex < dev->numOfPorts; portIndex++) {
		port = portIndex;
		printk( "Tag%d:%d  ", port, vtuEntry.vtuData.memberTagP[port]);
	}

	printk( "\n");

	while (1) {
		if ((status = gvtuGetEntryNext(dev, &vtuEntry)) != GT_OK)
		{
			break;
		}

		printk( "DBNum:%d, VID:%d \n", vtuEntry.DBNum, vtuEntry.vid);

		for (portIndex = 0; portIndex < dev->numOfPorts; portIndex++) {
			port = portIndex;
			printk( "Port%d:%d  ", port, vtuEntry.vtuData.memberTagP[port]);
		}

		printk( "\n");

	}
	return GT_OK;
}

void dsdt_dump_ports_info(GT_QD_DEV *dev)
{
	int i, j;
	GT_U16 vid;
	GT_LPORT mem_ports[MAX_SWITCH_PORTS];
	GT_U8 mem_ports_len;

	for (i = 0; i < dev->numOfPorts; ++i) {
		gvlnGetPortVid(dev, i, &vid);
		gvlnGetPortVlanPorts(dev, i, mem_ports, &mem_ports_len);
		printk( "PORT %d: VID %d\n\t Friend with %d ports: ", i, vid, mem_ports_len);
		for (j = 0; j < mem_ports_len; ++j) {
			printk( "%d ", mem_ports[j]);
		}
		printk( "\n");
	}
}

void mrv_dump_port_regs(GT_QD_DEV *dev)
{
	int i, j;
	GT_U16 data;

	for (i = 0; i < dev->numOfPorts; ++i) {
		printk( "PORT %d: \n", i);
		for (j = 0; j < 32; ++j) {
			gprtGetSwitchReg(dev, i, j, &data);
			printk( "Reg %d: 0x%x\n", j, data);
		}
		printk( "\n");
	}
	printk( "\n\nSwitch global1 registers:\n");
	for (j = 0; j < 32; ++j) {
		gprtGetGlobalReg(dev, j, &data);
		printk( "Reg %d: 0x%x\n", j, data);
	}

	printk( "\n\nSwitch global2 registers:\n");
	for (j = 0; j < 32; ++j) {
		gprtGetGlobal2Reg(dev, j, &data);
		printk( "Reg %d: 0x%x\n", j, data);
	}
	printk("\n\n");
}

void sampleDisplayCounter3(GT_STATS_COUNTER_SET3 *statsCounter)
{
   printk("InGoodOctetsLo  %08i    ", statsCounter->InGoodOctetsLo);
   printk( "InGoodOctetsHi  %08i   \n", statsCounter->InGoodOctetsHi);
   printk( "InBadOctets     %08i    ", statsCounter->InBadOctets);
   printk( "OutFCSErr       %08i   \n", statsCounter->OutFCSErr);
   printk( "InUnicasts      %08i    ", statsCounter->InUnicasts);
   printk( "Deferred        %08i   \n", statsCounter->Deferred);
   printk( "InBroadcasts    %08i    ", statsCounter->InBroadcasts);
   printk( "InMulticasts    %08i   \n", statsCounter->InMulticasts);
   printk( "64Octets        %08i    ", statsCounter->Octets64);
   printk( "127Octets       %08i   \n", statsCounter->Octets127);
   printk( "255Octets       %08i    ", statsCounter->Octets255);
   printk( "511Octets       %08i   \n", statsCounter->Octets511);
   printk( "1023Octets      %08i    ", statsCounter->Octets1023);
   printk( "MaxOctets       %08i   \n", statsCounter->OctetsMax);
   printk( "OutOctetsLo     %08i    ", statsCounter->OutOctetsLo);
   printk( "OutOctetsHi     %08i   \n", statsCounter->OutOctetsHi);
   printk( "OutUnicasts     %08i    ", statsCounter->OutUnicasts);
   printk( "Excessive       %08i   \n", statsCounter->Excessive);
   printk( "OutMulticasts   %08i    ", statsCounter->OutMulticasts);
   printk( "OutBroadcasts   %08i   \n", statsCounter->OutBroadcasts);
   printk( "Single          %08i    ", statsCounter->Single);
   printk( "OutPause        %08i   \n", statsCounter->OutPause);
   printk( "InPause         %08i    ", statsCounter->InPause);
   printk( "Multiple        %08i   \n", statsCounter->Multiple);
   printk( "Undersize       %08i    ", statsCounter->Undersize);
   printk( "Fragments       %08i   \n", statsCounter->Fragments);
   printk( "Oversize        %08i    ", statsCounter->Oversize);
   printk( "Jabber          %08i   \n", statsCounter->Jabber);
   printk( "InMACRcvErr     %08i    ", statsCounter->InMACRcvErr);
   printk( "InFCSErr        %08i   \n", statsCounter->InFCSErr);
   printk( "Collisions      %08i    ", statsCounter->Collisions);
   printk("Late            %08i   \n", statsCounter->Late);
}


/*
 * This sample is for 88E6093, 88E6095, 88E6185, and 88E6065
*/
GT_STATUS sampleGetRMONCounter3(GT_QD_DEV *dev)
{
    GT_STATUS status;
    GT_LPORT port;
    GT_STATS_COUNTER_SET3 statsCounterSet;

    for(port=0; port<dev->numOfPorts; port++)
    {
    	printk("Port %i :\n",port);

        if((status = gstatsGetPortAllCounters3(dev,port,&statsCounterSet)) != GT_OK)
        {
        	printk("gstatsGetPortAllCounters3 returned fail (%#x).\n",status);
            return status;
        }

        sampleDisplayCounter3(&statsCounterSet);

    }

    return GT_OK;
}

GT_STATUS mrv_show_macs(GT_QD_DEV *dev)
{
    GT_STATUS status;
    GT_ATU_ENTRY tmpMacEntry;
    int vid_dbnum;

    printk("ATU List:\n");

    for (vid_dbnum = 0; vid_dbnum < 512; vid_dbnum++)
    {
    	memset(&tmpMacEntry,0,sizeof(GT_ATU_ENTRY));
    	tmpMacEntry.DBNum = vid_dbnum;
    	if ((status = gfdbGetAtuEntryFirst(dev, &tmpMacEntry)) != GT_OK)
    	{
    		continue;
    	}

    	printk("(%02x-%02x-%02x-%02x-%02x-%02x) PortVec %#x, DBNum: %d, EntryState: 0x%x, MCEntryState: 0x%x\n",
    			tmpMacEntry.macAddr.arEther[0],
    			tmpMacEntry.macAddr.arEther[1],
    			tmpMacEntry.macAddr.arEther[2],
    			tmpMacEntry.macAddr.arEther[3],
    			tmpMacEntry.macAddr.arEther[4],
    			tmpMacEntry.macAddr.arEther[5],
    			tmpMacEntry.portVec,
    			tmpMacEntry.DBNum,
    			tmpMacEntry.entryState.ucEntryState,
    			tmpMacEntry.entryState.mcEntryState);

    	while(gfdbGetAtuEntryNext(dev,&tmpMacEntry) == GT_OK)
    	{
    		printk("(%02x-%02x-%02x-%02x-%02x-%02x) PortVec %#x, DBNum: %d, EntryState: 0x%x, MCEntryState: 0x%x\n",
    				tmpMacEntry.macAddr.arEther[0],
    				tmpMacEntry.macAddr.arEther[1],
    				tmpMacEntry.macAddr.arEther[2],
    				tmpMacEntry.macAddr.arEther[3],
    				tmpMacEntry.macAddr.arEther[4],
    				tmpMacEntry.macAddr.arEther[5],
    				tmpMacEntry.portVec,
    				tmpMacEntry.DBNum,
    				tmpMacEntry.entryState.ucEntryState,
    				tmpMacEntry.entryState.mcEntryState);
    	}
    }
    return GT_OK;
}


/*
 * Added for 6171 only 
*/
void dsdt_dump_switch_stats(GT_QD_DEV *dev)
{
	sampleDisplayVIDTable(dev);
	dsdt_dump_ports_info(dev);
	mrv_show_macs(dev);
	mrv_dump_port_regs(dev);
	sampleGetRMONCounter3(dev);
}

void dsdt_dump_all_switch_stats(int i) {
	int num;

	if (i < 0 || i >= MAX_SWITCHES) {
		printk("%s: Illegal switch number: %d\n",__FUNCTION__,  i);
		return;
	}

	if(&qddev[num] == NULL) {
		printk("%s: Switch %d is not initialized\n", i, __FUNCTION__);
		return;
	}

	
	printk("SWITCH STATS %d\n", i);
	printk("--------------------------------\n");
	dsdt_dump_switch_stats(&qddev[i]);
	printk("--------------------------------\n");
	
}

