
/**
 * @file
 * Header file for simple executive application initialization.  This defines
 * part of the ABI between the bootloader and the application.
 * $Id: cvmx-app-init.h,v 1.21 2006/06/15 23:12:08 rfranz Exp $
 *
 */

#ifndef __SOFAWARE_APP_INIT_H__
#define __SOFAWARE_APP_INIT_H__

#include <linux/types.h>
#include "sofaware_common.h"

#ifdef	__cplusplus
extern "C" {
#endif

typedef u_int32_t nand_img_offset_t;

typedef struct
{
	u_int32_t     default_ip;
	unsigned char license[SW_LICENSE_LEN];
	unsigned char activation_key[SW_ACTIVATION_KEY_LEN];
	u_int32_t     test_mode;
	u_int32_t     image_crc;
	u_int32_t     kernel_image_size;
	u_int32_t     vendor_mask;
	unsigned char part_num[SW_PART_NUM_LEN];
	unsigned char serial_num[SW_SERIAL_NUM_LEN];
	u_int32_t     sub_model;
	unsigned char bootcode_version;
	unsigned char debug;
	unsigned char user_module_src;   /* 0-none, 1-native, 2-external(CF) */
	unsigned char kernel_module_src; /* 0-none, 1-native, 2-external(CF) */
	u_int32_t	  image_type;
	eBootDevice	  boot_device;
	char		  firmware_fname[SW_MAX_FIRMWARE_FNAME+1];
	u_int32_t	  romdisk_offset;
	char          extra_blob_params[SW_EXTRA_BLOB_PARAM_NUM][SW_EXTRA_BLOB_PARAM_LEN];
	unsigned char image_version[SW_VERSION_STR_LEN+1];
	unsigned char def_lan_ip[16];
	unsigned char def_wan_ip[16];
	unsigned char debug_host_ip[16];
	u_int16_t     debug_host_port;
	char		  __reserved[14];
#if (! defined(SBOX4_ON_EVAL)) && (! defined(SBOX4_ON_ARROW))
	u_int32_t     total_file_len;
	nand_img_offset_t firm_img_offset;
	u_int32_t     sfp_ports;
	u_int32_t     usb_ports;
	u_int32_t     switch_ports;
	u_int32_t     wlan_region;
	u_int32_t     dsl_annex;
#endif
} sofaware_bootinfo_t;

#ifdef	__cplusplus
}
#endif

#endif /* __SOFAWARE_APP_INIT_H__ */
