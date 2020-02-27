

#ifndef __SOFAWARE_MISC_H__
#define __SOFAWARE_MISC_H__


#ifdef __KERNEL__ 
#define SW_MISC_MINOR			     4
#endif


#define IOC_PROBE_JP 			 	 0x10
#define IOC_GET_RST_BUTTON           0x20
#define IOC_RST_DSL_IMAGE 			 0x21
#define IOC_RST_SETTING           0x30
#define IOC_RST_FIRMWARE           0x40
#define IOC_GET_BAD_BLOCK_COUNT               0x60
#define IOC_RST_CTERA_FIRMWARE           0x80
#define IOC_GET_BUS_ADDRESS		     0x40085005
#define IOC_GET_VID_PID				 0x40085006
#define IOC_SET_USB_PORT_POWER		 0x40085007
#define IOC_SET_USB_HUB_POWER		 0x40085008
#define IOC_SET_USB_PORT_DISCONNECT 0x40085009

typedef void (*sw_debug_report_fn_t)(const char*, int);
void sw_set_usb_disconnect(int portNum);	
void sw_set_usb_port_pw(int on);
#ifdef __KERNEL__

extern int sw_get_jp_stat(int jp);
extern int is_debug_firmware(void);
extern int register_debug_print_fn(sw_debug_report_fn_t debug_report_fn);
extern int sofaware_print_debug(const char *msg, int len);

enum {
	W83L786_DIR	 = 1,
	W83L786_GPIO1,
	W83L786_GPIO2,
	W83L786_GPIO3,
	W83L786_GPIO4,
	W83L786_GPIO5,
	W83L786_GPO6,
	W83L786_GPO7,
	W83L786_GPIO8,
	W83L786_GPIO9,
	W83L786_GPIO10,
	W83L786_GPIO11,
	W83L786_GPIO12,
	W83L786_GPIO13,
	W83L786_GPIO14,
};

#endif


#define BIN_DSL_MODEM_MAGIC     0x1234ABCD
#define DSL_MODEM_VESION_LEN    65
//
//typedef struct {
//	unsigned int xsum;							/* check sum include header */
//	unsigned int magic;							/* 0x1234ABCD */
//	unsigned int size;							/* bin image length */
//	unsigned int annex;							/* annex */
//	unsigned char version[DSL_MODEM_VESION_LEN];/* version */
//	unsigned char __reserved[19];   			/* reserved */
//} bin_dsl_modem_hdr_t;
//
#define SW_PART_PRI_DSL_IMAGE_STR 		"Pri_DSL_Image"
#define SW_PART_PRI_DSL_IMAGE_HDR_STR	"Pri_DSL_Image_Hdr"
#define SW_PART_SEC_DSL_IMAGE_STR		"Sec_DSL_Image"

#endif /* #ifndef __SOFAWARE_MISC_H__ */

