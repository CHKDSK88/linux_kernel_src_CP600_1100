#ifndef __MARVELL_DEV_H__
#define __MARVELL_DEV_H__

#include <linux/ioctl.h>

#define  SW_VLAN_MISC_MINOR		3
#define SW_MAX_NUM_OF_SWITCH_PORTS	8
#define SW_MAX_SUPPORTED_VLANS 256

#define MRV_MAX_SUPPORTED_VLANS	4096
#define MRV_ILLEGAL_VID			4096

typedef enum
{
	kMrvAutoLearnAllowAll,	/* the default - auto learning */
	kMrvNewToCPU,			/* manual learning, forward new MACs to the CPU */
	kMrvBlockNew,			/* manual learning, block new MACs */
	kMrvBlockAll			/* block all incoming packets, don't report */
}eMrvPortState;

typedef struct  
{	int		  mPortIdx;
	eMrvPortState port_state;
	u_int8_t  mOnlyAllowedMAC[6];	/* MAC that's allowed when we're in "kMrvBlockNew" */
	u_int16_t mPortVID;
	int		  mIsMultiple;
}tMrvPortCfg;

typedef struct{
	int num_of_ports;
	tMrvPortCfg ports[SW_MAX_NUM_OF_SWITCH_PORTS];
}tMrvPortsCfg;

typedef struct 
{
	u_int16_t mVid;
	u_int32_t mPortMask;
}tMrvVidPortMask;

typedef struct 
{
	tMrvPortsCfg	mPortCfg;
	tMrvVidPortMask mVlanPortMask[SW_MAX_SUPPORTED_VLANS];
	int mNumOfVids;
}tMrvSwitchVlanCfg;

typedef struct 
{
	tMrvPortsCfg m_ports_cfg;
	int	m_vlan_ports_mask[MRV_MAX_SUPPORTED_VLANS];
}
tMrvCfgDB;

typedef struct{
	u_int32_t	reg_addr;
	u_int32_t	reg_val;
}tRegCmd;

typedef struct{
	u_int8_t  mMAC[6];	
	u_int16_t mVid;
	int		  mPortId;	
}tMacEntry;

#define MRV_MAX_IOCTL_DATA_SIZE	(sizeof(tMrvSwitchVlanCfg))

#define MRV_IOC_MAGIC	'm'

#define IOC_DISABLE_VLAN		_IO(MRV_IOC_MAGIC,1)
#define IOC_SWITCH_SET_PORT		_IOW(MRV_IOC_MAGIC,2,tMrvPortCfg)
#define IOC_SWITCH_SET_PORTS	_IOW(MRV_IOC_MAGIC,3,tMrvSwitchVlanCfg)
#define IOC_SWITCH_INIT_DONE	_IO(MRV_IOC_MAGIC,4)
#define IOC_DBG_GET_REG			_IOR(MRV_IOC_MAGIC,5,tRegCmd)
#define IOC_DBG_SET_REG			_IOW(MRV_IOC_MAGIC,6,tRegCmd)
#define IOC_DELETE_FDB			_IO(MRV_IOC_MAGIC,7)
#define IOC_ADD_VID_TO_PORT		_IOW(MRV_IOC_MAGIC,8,tMrvVidPortMask)		
#define IOC_DEL_VID_FROM_PORT	_IOW(MRV_IOC_MAGIC,9,tMrvVidPortMask)
#define IOC_ADD_MAC_TO_FDB		_IOW(MRV_IOC_MAGIC,10,tMacEntry)
#define IOC_DEL_MAC_FROM_FDB	_IOW(MRV_IOC_MAGIC,11,tMacEntry)
#define IOC_REGISTER_MAC_CALLBACK 	_IOW(MRV_IOC_MAGIC,12,long)


#ifdef __KERNEL__
void mrv_disp_dispatch_ioctl(unsigned int cmd, void *data)__attribute__((weak));
#endif

#endif //__MARVELL_DEV_H__
