
#ifndef __SOFAWARE_COMMON_H__
#define __SOFAWARE_COMMON_H__


#define SW_ARROW_BIN_MAGIC_V1		0x33441122
#define SW_ARROW_BIN_MAGIC_V2      	0x33441155
#define SW_CTERA_BIN_MAGIC      	0x44883311

#define SW_VERSION_STR_LEN      	64

/* SofaWare loadable files type */
#define	SW_UNKNOWN_FILE				0
#define	SW_IMAGE_FILE				1
#define	SW_BOOTLOADER_SCRIPT		2
#define	SW_PRESET_CFG_FILE			3
#define	SW_DOT_FIRM_FILE 			4

/* SofaWare images types */
#define BIN_TYPE_KERNEL				1
#define BIN_TYPE_BIN2GO				2
#define BIN_TYPE_ARROW_BOOTLOADER			110
#define BIN_TYPE_SBOX4_GB_BOOTLOADER			111
#define BIN_TYPE_SBOX4_CM_BOOTLOADER			112
#define BIN_TYPE_ARROW_DEBUG_BOOTLOADER		11
#define BIN_TYPE_NAC38_BOOTLOADER			13
#define BIN_TYPE_EBH3100_BOOTLOADER         16
#define BIN_TYPE_CN3010_EVB_HS5_BOOTLOADER  17
#define BIN_TYPE_CTERA			200

/* SofaWare images zip alg */
#define BIN_NOT_ZIPPED 				0
#define BIN_GZIP 					1
#define BIN_BZIP 					2


#define SW_LICENSE_LEN				32
#define SW_ACTIVATION_KEY_LEN		32
#define SW_SERIAL_NUM_LEN			16
#define SW_PART_NUM_LEN				16
#define SW_MAX_FIRMWARE_SIZE		(30 * 1024 * 1024)
#define SW_MAX_FIRMWARE_FNAME		15
#define SW_EXTRA_BLOB_PARAM_LEN     32

#define SW_EXTRA_BLOB_PARAM_NUM     8

typedef enum{
	kInternalCF,
	kExternalCF,
	kBootFromRam,
	kMaxBootDevice
}eBootDevice;

#define MAX_IFINDEX_VAL	512

/* SofaWare image header
 */

/*	if file_type is firmware :
		total_file_len = img_len + signature len
		img_len = run_file_len + length of the romdisk
	otherwise: 
		total_file_len = img_len = run_file_len
*/


typedef struct {
	unsigned int 	crc32;
	unsigned int	magic;
	unsigned int	vendor_mask;
	unsigned short	file_type;
	unsigned short	zip_type;
	unsigned int	run_file_len;
	unsigned int	img_file_len;
	unsigned int	total_file_len; 
	unsigned int	unzipped_run_file_len;
	unsigned int	run_address;
	unsigned short	pad_to_romdisk;
	char 			version_str[SW_VERSION_STR_LEN+1];
	char 			__reserved[63];

} arrow_image_hdr_t;

typedef struct {
	unsigned int xsum;				/* check sum include header */
	unsigned int magic;
	unsigned int f_ver;				/* sofaware firmware version */
	unsigned int version;			/* file version */
	unsigned int f_len;     		/* file length */
	unsigned short type;			/* file type; boot code or kernel */
	unsigned short action;			/* action type apply to this image; "jump to" or  "burn to flash" */
	unsigned int romdisk_offset;	/* offset of romdisk inside image */
	unsigned int reserved_length;	/* offset of next image */
	unsigned int run_address; 		/* address to run in RAM */
	unsigned int do_not_use;        /* is not sumxed */
} bin_hdr_t;

#define BIN_DSL_MODEM_MAGIC     0x1234ABCD
#define DSL_MODEM_VESION_LEN    65
#define BIN_MAGIC 0x550708AA
#define ACT_KERNEL_0	2 /* force writing to kernel 1 */
#define ACT_KERNEL_1	3 /* force writing to kernel 2 */

typedef struct {
	unsigned int xsum;							/* check sum include header */
	unsigned int magic;							/* 0x1234ABCD */
	unsigned int size;							/* bin image length */
	unsigned int annex;							/* annex */
	unsigned char version[DSL_MODEM_VESION_LEN];/* version */
	unsigned char __reserved[19];   			/* reserved */
} bin_dsl_modem_hdr_t;

#define NAND_PAGE_SIZE      (16*1024)
#define NAND_BLOCK_SIZE (128*1024)

//
// NAND partitions
//
#define SW_PART_CTERA_STR                             "CTERA"
#define SW_PART_CTERA_IMAGE_STR                             "CTIMAGE"
#define SW_PART_IMAGE_1_STR                             "SofaWare_Image_1"
#define SW_PART_IMAGE_0_STR                             "SofaWare_Image_0"
#define SW_PART_ROMDISK_STR                             "ROMDISK"
#define SW_PART_CONFIG_FILE_STR                 "Config_File"
#define SW_PART_JFFS_STR                                "SofaWare_JFFS"
#define SW_PART_DSL_SEC_STR                                "Sec_DSL_Image"
#define SW_PART_DSL_PRI_HDR_STR                                "Pri_DSL_Image_Hdr"
#define SW_PART_MMAP_STR                                "SofaWare_MMAP"

#define SW_PART_CONFIG_FILE_SIZE 0x00040000
#define SW_PART_CONFIG_FILE_OFFSET 0x00000000
#define SW_PART_JFFS_SIZE 0x00d40000
#define SW_PART_JFFS_OFFSET 0x00040000
#define SW_PART_DSL_SEC_SIZE 0x00200000
#define SW_PART_DSL_SEC_OFFSET 0x01B60000
#define SW_PART_DSL_PRI_HDR_SIZE 0x00020000
#define SW_PART_DSL_PRI_HDR_OFFSET 0x01d60000
#define SW_PART_IMAGE_1_OFFSET 0x01d80000
#define SW_PART_ROMDISK_OFFSET 0x01d80000
#define SW_PART_IMAGE_0_OFFSET 0x01d80000

#if defined(BUILD_CTERA_INTEGRATION)
#define SW_PART_IMAGE_1_SIZE 0x02E00000
#define SW_PART_ROMDISK_SIZE 0x02E00000
#define SW_PART_IMAGE_0_SIZE 0x02E00000
#define SW_PART_MMAP_SIZE 0x01A00000
#define SW_PART_MMAP_OFFSET 0x06380000
#define SW_PART_CTERA_IMAGE_SIZE   0x01800000
#define SW_PART_CTERA_IMAGE_OFFSET 0x04B80000
#define SW_PART_CTERA_SIZE   (SW_PART_CTERA_IMAGE_SIZE - sizeof(arrow_image_hdr_t))
#define SW_PART_CTERA_OFFSET (SW_PART_CTERA_IMAGE_OFFSET + sizeof(arrow_image_hdr_t))
#else
#define SW_PART_IMAGE_1_SIZE 0x03C00000
#define SW_PART_ROMDISK_SIZE 0x03C00000
#define SW_PART_IMAGE_0_SIZE 0x03C00000
#define SW_PART_MMAP_SIZE 0x02400000
#define SW_PART_MMAP_OFFSET 0x05980000
#endif


#define ANNEX_A    "A"
#define ANNEX_B    "B"
#define ANNEX_C    "C"

#define ANNEX_A_ID    0x10000000
#define ANNEX_B_ID    0x20000000
#define ANNEX_C_ID    0x40000000

#endif /* #ifndef __SOFAWARE_COMMON_H__ */

