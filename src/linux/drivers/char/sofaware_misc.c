#include <linux/sched.h>
#include <linux/module.h>
#include <linux/fcntl.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/miscdevice.h>
#include <asm/uaccess.h>
#include <linux/proc_fs.h>
#include <asm/uaccess.h>
#include <linux/sofaware_misc.h>
#include <linux/mtd/mtd.h>
#include <linux/vmalloc.h>
#include <linux/usb.h>
#include <linux/console.h>

//#include <sofaware_common.h>
#include <linux/mtd/partitions.h>
#include <linux/module.h>
#include <linux/sofaware_hal.h>
#include <asm/io.h>
//#include <sofaware-app-init.h>
//#include <sofaware_crc.h>
//#include <sofaware_firmware.h>


//#include "cvmx.h"
//#include "cvmx-csr-db.h"

extern int vnand_get_bad_block_count(void);

static struct proc_dir_entry* debug_firmware_proc_entry;
static struct proc_dir_entry* usb_modem_supermode_proc_entry;
static struct proc_dir_entry* usb_modem_reset_proc_entry;
static struct proc_dir_entry* usb_modem_reset_port_proc_entry;
static struct proc_dir_entry* usb_modem_lsi_proc_entry;

static int(*__sofaware_get_reset_button_status)(void);
static int(*__sofaware_get_jumper_stat)(int);

static int sofaware_misc_ioctl(struct inode *inode, struct file *file,
		unsigned int unMode, unsigned long ulDataAddr);
static long sofaware_misc_ioctl_compat(struct file *file, unsigned int unMode, unsigned long ulDataAddr);
static int sofaware_misc_open(struct inode *inode, struct file *file);
static int sofaware_misc_release(struct inode *inode, struct file *file);
static int sofaware_get_reset_button_status(void);
static int sofaware_get_jumper_stat(int jp);
#define IC_SG80_EXTEND1 0x21
#define USB1_POWER_EXTENDER 3
#define USB2_POWER_EXTENDER 4
#define EXCARD_POWER_EXTENDER 7
//extern int octeon_is_arrow_board(void);
//extern int octeon_is_sbox4_gigabyte_board(void);
//extern int octeon_is_sbox4_cameo_board(void);
//extern int octeon_is_sbox4_sercom_board(void);
int sofaware_get_dsl_annex(void)
{
	return 1;
}

static void DEFAULT_DEBUG_REPORT_FN(const char* msg, int len)
{
	return;
}

static volatile sw_debug_report_fn_t sw_debug_report_fn = DEFAULT_DEBUG_REPORT_FN;

static volatile unsigned long debug_report_lock;
#define DEBUG_REPORT_LOCK_CALLBACK_BIT 1
#define DEBUG_REPORT_LOCK_BACK_BUFFER_BIT 2


/* compat_ioctl callback is called instead of ioctl callback because a 32 bit process is calling a 64 kernel */
struct file_operations stSofaware_misc_fops = {
ioctl   : sofaware_misc_ioctl,
compat_ioctl   : sofaware_misc_ioctl_compat,
open    : sofaware_misc_open,
release : sofaware_misc_release,
};

static struct miscdevice stSofaware_misc_miscdev = {
minor   : SW_MISC_MINOR,
name    : "sofaware_misc",
fops    : &stSofaware_misc_fops,
};

#define SW_MAX_MTD_READ_SZ              0x100000

#define MIN(x,y)                        ((x) < (y)) ? (x) : (y)

static int check_sofaware_image(struct mtd_info* mtd);
static int check_dsl_image(struct mtd_info* mtd);

/*****************************************************
 * sofaware_misc_ioctl
 ******************************************************/

static long sofaware_misc_ioctl_compat(struct file *file, unsigned int unMode, unsigned long ulDataAddr) {
   return sofaware_misc_ioctl(NULL, file, unMode, ulDataAddr);
}


static void erase_callback(struct erase_info* instr)
{
    //    wake_up((wait_queue_head_t *)instr->priv);
}

static int mk_jffs_partition(struct mtd_info* mtd)
{
        struct erase_info* e_info;
        wait_queue_head_t waitq;
        DECLARE_WAITQUEUE(wait, current);

        if (mtd == NULL){
                printk("mk_jffs_partition: mtd == NULL !!! \n");
                return 1;
        }

        printk("Format JFFS partition (mtd %p, size 0x%x) ... \n", mtd, mtd->size);

        e_info=kmalloc(sizeof(struct erase_info),GFP_KERNEL);
        if (e_info == NULL){
                printk("ERROR: failed to reset settings \n");
                return -1;
        }

        if (mtd == NULL){
                printk("ERROR: failed to get MTD partition \n");
                return -1;
        }

        init_waitqueue_head(&waitq);
        memset (e_info, 0, sizeof(struct erase_info));
        e_info->mtd = mtd;
        e_info->addr = 0;
        e_info->len = mtd->size;
        e_info->callback = erase_callback;
        e_info->priv = (unsigned long)&waitq;

        if(mtd->erase(mtd, e_info)) {
                printk("ERROR: failed to erase JFFS partition \n");
                kfree (e_info);
                return -1;
        }

        set_current_state(TASK_UNINTERRUPTIBLE);
        add_wait_queue(&waitq, &wait);
        remove_wait_queue(&waitq, &wait);
        set_current_state(TASK_RUNNING);

        printk("Format JFFS partition successfully \n");

        kfree (e_info);
        return 0;
}


static int destroy_image(struct mtd_info* mtd)
{
        struct erase_info* e_info;
        wait_queue_head_t waitq;
        DECLARE_WAITQUEUE(wait, current);

        if (mtd == NULL){
                printk("destroy_image: mtd == NULL !!! \n");
                return 1;
        }

        printk("erase first mtd block of \"%s\" \n", mtd->name);

        e_info=kmalloc(sizeof(struct erase_info), GFP_KERNEL);
        if (e_info == NULL){
                printk("ERROR: failed to kmalloc erase buffer, can't reset firmware \n");
                return -1;
        }

        printk("Destroy image 1 ... \n");

        init_waitqueue_head(&waitq);
        memset (e_info, 0, sizeof(struct erase_info));
        e_info->mtd = mtd;
        e_info->addr = 0x0;
        e_info->len = mtd->erasesize;
        e_info->callback = erase_callback;
        e_info->priv = (unsigned long)&waitq;

        if(mtd->erase(mtd, e_info)) {
                printk("ERROR: failed to erase image (%s) \n", mtd->name);
                kfree (e_info);
                return -1;
        }

        set_current_state(TASK_UNINTERRUPTIBLE);
        add_wait_queue(&waitq, &wait);
        remove_wait_queue(&waitq, &wait);
        set_current_state(TASK_RUNNING);
        printk("Destroyed image (%s) successfully \n", mtd->name);
        kfree (e_info);

        return 0;
}

static int get_sofaware_image_mtd_num(int image_num)
{
        int index = -1;

     /*   if (image_num == 0)
                index = get_mtd_partition_index_by_name(SW_PART_IMAGE_0_STR);
        else if (image_num == 1)
                index = get_mtd_partition_index_by_name(SW_PART_IMAGE_1_STR);
*/
        return (index>=0)?index:(-1);
}

static int get_dsl_image_mtd_num(int image_num)
{
	int index = -1;

/*	if (image_num == 0)
		index = get_mtd_partition_index_by_name(SW_PART_SEC_DSL_IMAGE_STR);
	else if (image_num == 1)
		index = get_mtd_partition_index_by_name(SW_PART_PRI_DSL_IMAGE_HDR_STR);*/

	return (index>=0)?index:(-1);
}


static int reset_sofaware_firmware(void)
{
        int rc = 0;

        printk ("Destroy SofaWare primary image \n");

        if ((rc = check_sofaware_image(get_mtd_device(NULL, get_sofaware_image_mtd_num(0)))) > 0)
                printk ("reset_sofaware_firmware: failed to determine image 0 status, reset anyway !!! \n");

        if (rc >= 0){
                rc = destroy_image( get_mtd_device(NULL, get_sofaware_image_mtd_num(1)));
        }
        else {
                printk("Image 0 is bad, can't destroy image 1 \n");
                rc = -1;
        }

        return rc;
}

static int reset_ctera_firmware(void)
{
        int rc = 0;

        printk ("Destroy CTERA iimage \n");

        rc = destroy_image( get_mtd_device(NULL, get_mtd_partition_index_by_name(SW_PART_CTERA_IMAGE_STR)));

        return rc;
}


static int reset_sofaware_setting(void)
{
        mk_jffs_partition(get_mtd_device(NULL, get_mtd_partition_index_by_name(SW_PART_JFFS_STR)));

        return 0;
}


static int reset_dsl_image(void)
{
	int rc = 0;

	printk ("Destroy DSL primary image \n");

    if ((rc = check_dsl_image(get_mtd_device(NULL, get_dsl_image_mtd_num(0)))) > 0)
           printk ("reset_dsl_image: failed to determine image 0 status, reset anyway !!! \n");

    if (rc >= 0){
          rc = destroy_image( get_mtd_device(NULL, get_dsl_image_mtd_num(1)));
    }
    else {
            printk("Image 0 is bad, can't destroy image 1 \n");
            rc = -1;
    }

	return rc;
}
static int sofaware_mtd_read(struct mtd_info *mtd, loff_t from, size_t len,
                size_t *retlen, u_char *buf)
{
        size_t read_count = 0;
        int rc = 0;

        *retlen = 0;

        while (read_count < len) {
                size_t this_read = MIN(len - read_count, SW_MAX_MTD_READ_SZ);
                size_t this_retlen = 0;

                //reset_wd_timer();

                rc = mtd->read(mtd, from + read_count, this_read, &this_retlen, buf + read_count);
                read_count += this_retlen;

                if (rc != 0) {
                        printk ("ERROR: %s() - failed to read mtd 0x%p \n", __FUNCTION__, mtd);
                        break;
                }
        }

        *retlen = read_count;
        return rc;
}

static int check_sofaware_image(struct mtd_info* mtd)
{
        unsigned char* image0_addr = NULL;
        size_t retlen = 0;
        arrow_image_hdr_t bin_hdr;
        int rc;
	size_t image0_size;

        if (mtd == NULL){
                printk("check_sofaware_image: mtd == NULL !!! \n");
                return 1;
        }

        /* read the header only and check it
         */
        sofaware_mtd_read(mtd, 0, sizeof(arrow_image_hdr_t), &retlen, (unsigned char*)(&bin_hdr));
        printk("Check image header ... \n");
        if (sofaware_check_image_header(&bin_hdr) != 0){
                printk("check_sofaware_image: image header 0 is bad \n");
                return -1;
        }

	image0_size = get_image_size_from_header(&bin_hdr);

	printk("check_sofaware_image: Allocating %ld bytes for image0\n", image0_size);

        image0_addr=vmalloc(image0_size);
        if (image0_addr == NULL){
                printk("check_sofaware_image: failed to vmalloc image buffer \n");
                return 1; /* if we can't determine if that image is bad return 1 */
        }

        /* read the all the image and check it
         */
        sofaware_mtd_read(mtd, 0, image0_size, &retlen, image0_addr);
        printk("Check image ... \n");
        rc = sofaware_check_image((unsigned long)image0_addr);

        vfree (image0_addr);
        return rc;
}
static int check_dsl_image(struct mtd_info* mtd)
{
        unsigned char* image0_addr = NULL;
        size_t retlen = 0;
    	bin_dsl_modem_hdr_t dsl_hdr;
        int rc;
	size_t image0_size;

        if (mtd == NULL){
                printk("check_sofaware_image: mtd == NULL !!! \n");
                return 1;
        }

        /* read the header only and check it
         */
        sofaware_mtd_read(mtd, 0, sizeof(bin_dsl_modem_hdr_t), &retlen, (unsigned char*)(&dsl_hdr));
        printk("Check dsl image header ... \n");
       /* if (sofaware_check_dsl_image_header(&dsl_hdr) != 0){
                printk("check_dsl_image: dsl image header 0 is bad \n");
                return -1;
        }*/

	image0_size = 0;//get_image_size_from_dsl_header(&dsl_hdr);

	printk("check_dsl_image: Allocating %ld bytes for image0\n", image0_size);

        image0_addr=vmalloc(image0_size);
        if (image0_addr == NULL){
                printk("check_dsl_image: failed to vmalloc image buffer \n");
                return 1; /* if we can't determine if that image is bad return 1 */
        }

        /* read the all the image and check it
         */
        sofaware_mtd_read(mtd, 0, image0_size, &retlen, image0_addr);
        printk("Check image ... \n");
        rc = 0;//sofaware_check_dsl_image((unsigned long)image0_addr);

        vfree (image0_addr);
        return rc;
}
void sw_set_usb_port_pw(int on)
{
	struct usb_device * usb_dev;
	int result = 0;
	//Cavium Host controller device
	usb_dev = usb_find_device(0x409,0x5a);
	if(usb_dev)
	{
		result = usb_reset_composite_device(usb_dev, NULL);
	}
	else
	{
		printk("%s:%d No device found\n",__FUNCTION__,__LINE__);
	}
}

static int proc_write_usb_modem_reset (struct file* file, const char* buffer, unsigned
	long count, void* data)
{
	char tmp, val;

	if (count<1)
		return -EFAULT;

	if(copy_from_user(&tmp, buffer, 1)) 
		return -EFAULT;

	val = tmp - '0';
	if (val!=0 && val!=1)
	{
		printk("supermode should be set to 0 or 1\n");
		return -EFAULT;
	}

	sw_set_usb_port_pw(val);

	return count;
}

static int proc_write_usb_modem_reset_port (struct file* file, const char* buffer, unsigned
	long count, void* data)
{
	char tmp, val;

	if (count<1)
		return -EFAULT;

	if(copy_from_user(&tmp, buffer, 1)) 
		return -EFAULT;

	val = tmp - '0';
	if (val!=0 && val!=1)
	{
		printk("USB port should be set to 0 or 1\n");
		return -EFAULT;
	}

	sw_set_usb_disconnect(val+1);

	return count;
}

extern int sierra_net_reset_lsi;
static int proc_write_usb_sierra_reset_lsi (struct file* file, const char* buffer, unsigned
	long count, void* data)
{
	char tmp, val;

	if (count<1)
		return -EFAULT;

	if(copy_from_user(&tmp, buffer, 1)) 
		return -EFAULT;

	val = tmp - '0';
	if (val!=0 && val!=1)
	{
		printk("supermode should be set to 0 or 1\n");
		return -EFAULT;
	}

	sierra_net_reset_lsi = (int)val;

	return count;
}

//extern cvmx_bootinfo_t *octeon_bootinfo;
extern int octeon_pci_setup(void);
extern void write_hwmon_gpio(int ctl_name, int val);

static void sw_set_usb_hub_pw(int on)
{
	if (on){
		extend_gpio_ctl(IC_SG80_EXTEND1, USB1_POWER_EXTENDER, 0);
		extend_gpio_ctl(IC_SG80_EXTEND1, USB2_POWER_EXTENDER, 0);
		extend_gpio_ctl(IC_SG80_EXTEND1, EXCARD_POWER_EXTENDER,1); // turn on power to express card
	}
	else {
		extend_gpio_ctl(IC_SG80_EXTEND1, USB1_POWER_EXTENDER, 1); // turn off power for usb1 device
		extend_gpio_ctl(IC_SG80_EXTEND1, USB2_POWER_EXTENDER, 1); // turn off power to usb2 device
		extend_gpio_ctl(IC_SG80_EXTEND1, EXCARD_POWER_EXTENDER,0); // turn on power to express card
	}
}

void sw_set_usb_disconnect(int portNum)
{
	if (portNum == 1){
		extend_gpio_ctl(IC_SG80_EXTEND1, USB1_POWER_EXTENDER, 1);
		mdelay(2000);
		extend_gpio_ctl(IC_SG80_EXTEND1, USB1_POWER_EXTENDER, 0);		
	}
	else if (portNum==2){
		extend_gpio_ctl(IC_SG80_EXTEND1, USB2_POWER_EXTENDER, 1); // turn off power for usb1 device
		mdelay(2000);
		extend_gpio_ctl(IC_SG80_EXTEND1, USB2_POWER_EXTENDER, 0);
	}
}

static int sofaware_misc_ioctl(struct inode *inode, struct file *file,
		unsigned int unMode, unsigned long ulDataAddr)
{
	int rc = 0;
	unsigned char* pucaData = (unsigned char *)ulDataAddr;
	int jp, jp_stat, rst_button, pw, count, tmp;

	switch(unMode) {

	/*	case IOC_PROBE_JP:
			{
				if (copy_from_user ((unsigned char *)&jp, pucaData, sizeof(int))) {
					printk ("sofaware_misc_ioctl(%d) - failed to copy_from_user() \n", unMode);
					rc = -EFAULT;
					break;
				}

				if ((jp_stat = sofaware_get_jumper_stat(jp)) == -1){
					printk ("sofaware_misc_ioctl(%d) - failed get jumper status \n", unMode);
					rc = -EINVAL;
					break;
				}

				if (copy_to_user (pucaData, &jp_stat, sizeof(int))) {
					printk ("sofaware_misc_ioctl(%d) - failed to copy_to_user() \n", unMode);
					rc = -EFAULT;
					break;
				}
				break;
			}

		case IOC_GET_RST_BUTTON:
			{
				rst_button = sofaware_get_reset_button_status();
				if (rst_button) printk("rst button on ");

				if (copy_to_user (pucaData, &rst_button, sizeof(int))) {
					printk ("sofaware_misc_ioctl(%d) - failed to copy_to_user() \n", unMode);
					rc = -EFAULT;
					break;
				}
				break;
			}
		case IOC_RST_SETTING:
			rc = reset_sofaware_setting();
			break;
		case IOC_RST_FIRMWARE:
			rc = reset_sofaware_firmware();
			break;
		case IOC_RST_CTERA_FIRMWARE:
			rc = reset_ctera_firmware();
			break;*/
		case IOC_RST_DSL_IMAGE:
			if (1)
				rc = reset_dsl_image();
			break;
		case IOC_SET_USB_PORT_POWER:
			if (copy_from_user ((int *)&pw, pucaData, sizeof(int))) {
				printk ("sofaware_misc_ioctl(%d) - failed to soft reset copy_from_user() \n", unMode);
				rc = -EFAULT;
				break;
			}
			sw_set_usb_port_pw(pw);
			break;
		case IOC_SET_USB_HUB_POWER:
			if (copy_from_user ((int *)&pw, pucaData, sizeof(int))) {
				printk ("sofaware_misc_ioctl(%d) - failed to copy_from_user() \n", unMode);
				rc = -EFAULT;
				break;
			}
			sw_set_usb_hub_pw(pw);
			break;
		case IOC_SET_USB_PORT_DISCONNECT:
			//IF SG80 - tmp -on/off
			//If SEATTLE - tmp = usb port number
			if (copy_from_user ((int *)&tmp, pucaData, sizeof(int))) {
				printk ("sofaware_misc_ioctl(%d) - failed to copy_from_user() \n", unMode);
				rc = -EFAULT;
				break;
			}	
			if (is_seattle_board())
			{
				sw_set_usb_disconnect(tmp);
			}
			else//SG80
			{
				sw_set_usb_port_pw(tmp);
			}
			break;
		/*case IOC_GET_BAD_BLOCK_COUNT:
			count = vnand_get_bad_block_count() ;

			if (copy_to_user (pucaData, &count, sizeof(int))) {
				printk ("sofaware_misc_ioctl() - failed to copy_to_user() for ioctl %d\n",IOC_GET_BAD_BLOCK_COUNT);
				rc = -EFAULT;
				break;
			}
			break ;*/
		default:
			rc = -1;
			break;
	}

	return rc;
}

/*****************************************************
 * sofaware_misc_open
 ******************************************************/
static int sofaware_misc_open(struct inode *inode, struct file *file)
{
	return 0;
}


/*****************************************************
 * sofaware_misc_release
 ******************************************************/
static int sofaware_misc_release(struct inode *inode, struct file *file)
{
	return 0;
}

static char g_is_usb_supermode = 0;
char is_usb_supermode(void)
{
	return g_is_usb_supermode;
}

static int proc_write_usb_modem_supermode (struct file* file, const char* buffer, unsigned
	long count, void* data)
{
	char tmp, val;

	if (count<1)
		return -EFAULT;

	if(copy_from_user(&tmp, buffer, 1)) 
		return -EFAULT;

	val = tmp - '0';
	if (val!=0 && val!=1)
	{
		printk("supermode should be set to 0 or 1\n");
		return -EFAULT;
	}

	g_is_usb_supermode = val;

	return count;
}

static int proc_read_usb_modem_supermode (char* buf, char **start, off_t offset, 
	int count, int *eof, void* priv_data)
{
	int len;

	if (count < 2)
		return -1;

	len = sprintf(buf, "%d\n", g_is_usb_supermode);

	*eof = 1;
	*start = NULL;

	return len;
}

static int proc_read_gpio (char* buf, char **start, off_t offset, 
	int count, int *eof, void* priv_data)
{
	int len;
/*
	if (count < 2)
		return -1;

	len = sprintf(buf, "%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d\n", 
		octeon_gpio_value(23),octeon_gpio_value(22),octeon_gpio_value(21),octeon_gpio_value(20),
		octeon_gpio_value(19),octeon_gpio_value(18),octeon_gpio_value(17),octeon_gpio_value(16),
		octeon_gpio_value(15),octeon_gpio_value(14),octeon_gpio_value(13),octeon_gpio_value(12),
		octeon_gpio_value(11),octeon_gpio_value(10),octeon_gpio_value(9),octeon_gpio_value(8),
		octeon_gpio_value(7),octeon_gpio_value(6),octeon_gpio_value(5),octeon_gpio_value(4),
		octeon_gpio_value(3),octeon_gpio_value(2),octeon_gpio_value(1),octeon_gpio_value(0)
		);

	*eof = 1;
	*start = NULL;*/

	return len;
}

static int proc_write_gpio (struct file* file, const char* buffer, unsigned
	long count, void* data)
{
	char tmp[3], val, gpio;

	if (count<1)
		return -EFAULT;

	if(copy_from_user(&tmp[0], buffer, 3)) 
		return -EFAULT;

	if ((tmp[2] != '0') && (tmp[2] != '1'))
	{
		val = (tmp[1] - '0');
		gpio = (tmp[0] - '0');
	}
	else
	{
		val = (tmp[2] - '0');
		gpio = (tmp[0] - '0')*10 + (tmp[1] - '0');
	}

	if (val!=0 && val!=1)
	{
		printk("gpio should be set to 0 or 1\n");
		return -EFAULT;
	}
	if (gpio<0 || gpio>23)
	{
		printk("gpio should be >=0, <=23\n");
		return -EFAULT;
	}
	printk("gpio set %d %d\n",gpio, val);

	//octeon_gpio_cfg_output(gpio);
	if (val)
	{
	//	octeon_gpio_set(gpio);
	}
	else
	{
	//	octeon_gpio_clr(gpio);
	}
	return count;
}

int is_debug_firmware(void)
{
	return 0;//SOFAWARE_DEBUG_FIRMWARE;
}

static int debug_firmware_proc_read (char* buf, char **start, off_t offset,
	int count, int *eof, void* priv_data)
{
	int len;

	len = sprintf(buf, "%s", is_debug_firmware()?"yes":"no");
	*eof = 1;
	*start = NULL;

	return len;
}

#define MAX_DEBUG_PIDS 32768

#if MAX_DEBUG_PIDS < PID_MAX_DEFAULT
#error Current debug pid table size(MAX_DEBUG_PIDS) is too small (max pid: PID_MAX_DEFAULT)
#endif

#define SW_SYSLOG_DEBUG_BUF_SIZE 64000

int is_syslog_debug_pid_set(void);
void set_syslog_debug_pid(void);
void unset_syslog_debug_pid(void);

static char *s_syslog_debug_buf = NULL;
static int s_syslog_debug_back_buf_cur_len = 0;


static void add_msg_syslog_debug_back_buf(const char *buf, int len) {
        if (s_syslog_debug_back_buf_cur_len + len + 1 > SW_SYSLOG_DEBUG_BUF_SIZE) {
                return;
        }

        memcpy(s_syslog_debug_buf + s_syslog_debug_back_buf_cur_len, buf, len);
        s_syslog_debug_buf[s_syslog_debug_back_buf_cur_len + len]='\0';
        s_syslog_debug_back_buf_cur_len += (len + 1);
}


static void flush_syslog_debug_back_buf(void) {
	int i,j;

	if (s_syslog_debug_back_buf_cur_len == 0) {
		return;
	}


        j=0;
        for (i=0; i < s_syslog_debug_back_buf_cur_len; i++) {
                if (s_syslog_debug_buf[i] == '\0' || i - j > 1400) {
                        sw_debug_report_fn(s_syslog_debug_buf + j, i - j + (s_syslog_debug_buf[i] == '\0' ? 0 : 1));
                        j=i+1;
                        if (! oops_in_progress && (!in_atomic())) {
                                yield();
                        }
                }
        }

	s_syslog_debug_back_buf_cur_len = 0;

}

int init_syslog_debug_back_buf(void) {
        s_syslog_debug_buf = kmalloc(SW_SYSLOG_DEBUG_BUF_SIZE, GFP_KERNEL);
        if (s_syslog_debug_buf == NULL) {
                printk("init_syslog_debug_back_buf: Could not allocate %d bytes for s_syslog_debug_buf\n", SW_SYSLOG_DEBUG_BUF_SIZE);
                return -ENOMEM;
        }

	return 0;
}

int register_debug_print_fn(sw_debug_report_fn_t debug_report_fn)
{
	unsigned long flags;

	if (sw_debug_report_fn != DEFAULT_DEBUG_REPORT_FN) {
		printk ("%s(): ERROR - debug report function is already registerd\n", __FUNCTION__);
		return -1;
	}

	local_irq_save(flags);
	sw_debug_report_fn = debug_report_fn;
	local_irq_restore(flags);

	return 0;
}

spinlock_t syslog_debug_pid_lock;
char       *syslog_debug_pid_table = NULL;

void set_syslog_debug_pid(void) {
	unsigned long flags;

        if (syslog_debug_pid_table == NULL || current == NULL || current->pid < 0 || current->pid >= MAX_DEBUG_PIDS) {
                return;
        }
        spin_lock_irqsave(&syslog_debug_pid_lock, flags);
        syslog_debug_pid_table[current->pid >> 3] |= (1 << (current->pid & 7));
        spin_unlock_irqrestore(&syslog_debug_pid_lock, flags);
}

void unset_syslog_debug_pid(void) {
	unsigned long flags;

        if (syslog_debug_pid_table == NULL || current == NULL || current->pid < 0 || current->pid >= MAX_DEBUG_PIDS) {
                return;
        }
        spin_lock_irqsave(&syslog_debug_pid_lock, flags);
        syslog_debug_pid_table[current->pid >> 3] &= (~(1 << (current->pid & 7)));
        spin_unlock_irqrestore(&syslog_debug_pid_lock, flags);
}

int is_syslog_debug_pid_set(void) {
        int ret;
	unsigned long flags;

        if (syslog_debug_pid_table == NULL || current == NULL || current->pid < 0 || current->pid >= MAX_DEBUG_PIDS) {
                return 0;
        }
        spin_lock_irqsave(&syslog_debug_pid_lock, flags);
        ret = (syslog_debug_pid_table[current->pid >> 3] & (1 << (current->pid & 7)));
        spin_unlock_irqrestore(&syslog_debug_pid_lock, flags);

        return ret;
}

inline int lock_syslog_debug_back_buf(void) {
	return test_and_set_bit(DEBUG_REPORT_LOCK_BACK_BUFFER_BIT, &debug_report_lock) == 0;
}

inline void unlock_syslog_debug_back_buf(void) {
	clear_bit(DEBUG_REPORT_LOCK_BACK_BUFFER_BIT, &debug_report_lock);
}

int sofaware_print_debug(const char *msg, int len) 
{
	if (! syslog_debug_pid_table) {
		return 0;
	}

        if (is_syslog_debug_pid_set()) {
                return 0;
        }

	set_syslog_debug_pid();

	if (test_and_set_bit(DEBUG_REPORT_LOCK_CALLBACK_BIT, &debug_report_lock) == 0) {
		if (lock_syslog_debug_back_buf()) {
			flush_syslog_debug_back_buf();
			unlock_syslog_debug_back_buf();
		}
		sw_debug_report_fn(msg, len);

		clear_bit(DEBUG_REPORT_LOCK_CALLBACK_BIT, &debug_report_lock);
	} else {
		if (lock_syslog_debug_back_buf()) {
			add_msg_syslog_debug_back_buf(msg, len);		
			unlock_syslog_debug_back_buf();
		}
	}

	unset_syslog_debug_pid();

	return 0;
}

static void sofaware_print_debug_con(struct console *con, const char *msg, unsigned len) {
	sofaware_print_debug(msg, (int) len);
}

static struct console swsyslog = {
        .name   = "swsyslog",
        .flags  = CON_ENABLED | CON_PRINTBUFFER,
        .write  = sofaware_print_debug_con
};

int sofaware_get_reset_button_status(void)
{
	if (__sofaware_get_reset_button_status)
		return __sofaware_get_reset_button_status();

	return 0;
}

int sofaware_get_jumper_stat(int jp)
{
	if (__sofaware_get_jumper_stat)
		return __sofaware_get_jumper_stat(jp);

	return 0;
}

int __init sofaware_misc_init(void)
{

/*	if (octeon_is_arrow_board()) {
		extern int arrow_get_reset_button_status(void);
		extern int arrow_get_jumper_stat(int jp);

		__sofaware_get_reset_button_status = arrow_get_reset_button_status;
		__sofaware_get_jumper_stat = arrow_get_jumper_stat;
	}
	else if ( octeon_is_sbox4_gigabyte_board() )
	{
		extern int gb_sbox4_get_reset_button_status(void);
		extern int gb_sbox4_get_jumper_stat(int jp);

		__sofaware_get_reset_button_status = gb_sbox4_get_reset_button_status;
		__sofaware_get_jumper_stat = gb_sbox4_get_jumper_stat;
	}
	else if ( octeon_is_sbox4_cameo_board() )
	{
		extern int cm_sbox4_get_reset_button_status(void);
		extern int cm_sbox4_get_jumper_stat(int jp);

		__sofaware_get_reset_button_status = cm_sbox4_get_reset_button_status;
		__sofaware_get_jumper_stat = cm_sbox4_get_jumper_stat;
	}
	else if ( octeon_is_sbox4_sercom_board() )
	{
		extern int sm_sbox4_get_reset_button_status(void);
		extern int sm_sbox4_get_jumper_stat(int jp);

		__sofaware_get_reset_button_status = sm_sbox4_get_reset_button_status;
		__sofaware_get_jumper_stat = sm_sbox4_get_jumper_stat;
	}*/

	if (misc_register(&stSofaware_misc_miscdev) ) {
		printk("sofaware_misc_init: can't register misc device\n");
		return -EINVAL;
	}

	debug_firmware_proc_entry = create_proc_entry("is_debug_firmware", 0644, NULL);
	if (debug_firmware_proc_entry){
		debug_firmware_proc_entry->read_proc = debug_firmware_proc_read;
		debug_firmware_proc_entry->write_proc = NULL;
		debug_firmware_proc_entry->owner = THIS_MODULE;
	}
	else
		printk("failed to create /proc/is_debug_firmware \n");

	usb_modem_supermode_proc_entry = create_proc_entry("usb_modem_supermode", 0644, NULL);
	if (usb_modem_supermode_proc_entry ){
		usb_modem_supermode_proc_entry ->read_proc = proc_read_usb_modem_supermode;
		usb_modem_supermode_proc_entry ->write_proc = proc_write_usb_modem_supermode;
		usb_modem_supermode_proc_entry ->owner = THIS_MODULE;
	}
	else
		printk("failed to create /proc/is_usb_modem_supermode \n");
	
	usb_modem_supermode_proc_entry = create_proc_entry("gpio", 0644, NULL);
	if (usb_modem_supermode_proc_entry ){
		usb_modem_supermode_proc_entry ->read_proc = proc_read_gpio;
		usb_modem_supermode_proc_entry ->write_proc = proc_write_gpio;
		usb_modem_supermode_proc_entry ->owner = THIS_MODULE;
	}
	else
		printk("failed to create /proc/gpio\n");
	
	usb_modem_reset_proc_entry = create_proc_entry("usb_modem_reset", 0644, NULL);
	if (usb_modem_reset_proc_entry ){
		usb_modem_reset_proc_entry ->read_proc = NULL;
		usb_modem_reset_proc_entry ->write_proc = proc_write_usb_modem_reset;
		usb_modem_reset_proc_entry ->owner = THIS_MODULE;
	}
	else
		printk("failed to create /proc/usb_modem_reset \n");
	
	usb_modem_reset_port_proc_entry = create_proc_entry("usb_modem_reset_port", 0644, NULL);
	if (usb_modem_reset_port_proc_entry ){
		usb_modem_reset_port_proc_entry ->read_proc = NULL;
		usb_modem_reset_port_proc_entry ->write_proc = proc_write_usb_modem_reset_port;
		usb_modem_reset_port_proc_entry ->owner = THIS_MODULE;
	}
	else
		printk("failed to create /proc/usb_modem_reset_port \n");
	
	usb_modem_lsi_proc_entry = create_proc_entry("usb_sierra_reset_lsi", 0644, NULL);
	if (usb_modem_lsi_proc_entry ){
		usb_modem_lsi_proc_entry ->read_proc = NULL;
		usb_modem_lsi_proc_entry ->write_proc = proc_write_usb_sierra_reset_lsi;
		usb_modem_lsi_proc_entry ->owner = THIS_MODULE;
	}
	else
		printk("failed to create /proc/usb_sierra_reset_lsi \n");
	
	if (is_debug_firmware()) {

		syslog_debug_pid_table = kmalloc(MAX_DEBUG_PIDS >> 3, GFP_KERNEL);
		if (syslog_debug_pid_table == NULL) {
			printk("sofaware_misc_init: Could not create print debug pid table\n");
			return 0;
		} else {
			memset(syslog_debug_pid_table, 0, MAX_DEBUG_PIDS >> 3);
		}

		if (syslog_debug_pid_table) {
			if (init_syslog_debug_back_buf() < 0) {
				kfree(syslog_debug_pid_table);
				syslog_debug_pid_table = NULL;
			} else {
				register_console(&swsyslog);
			}
			
		}
		spin_lock_init(&syslog_debug_pid_lock);

	}

	return 0;
}

module_init(sofaware_misc_init);

EXPORT_SYMBOL(is_usb_supermode);
EXPORT_SYMBOL(register_debug_print_fn);
EXPORT_SYMBOL(sofaware_print_debug);
EXPORT_SYMBOL(is_debug_firmware);


