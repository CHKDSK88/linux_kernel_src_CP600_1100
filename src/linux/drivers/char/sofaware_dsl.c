//#include <linux/config.h>
#include <linux/module.h>
#include <linux/fcntl.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/miscdevice.h>
#include <asm/uaccess.h>
#include <linux/sofaware_hw_id.h>
#include <linux/sofaware_dsl.h>
#include <linux/sofaware_hal.h>

#include <linux/fs.h>
#include <linux/jiffies.h>
#include <linux/timer.h>
#include <linux/delay.h>

#include "cp_ledctl.h"
#define IC_SG80_EXTEND1 0x21
extern int octeon_is_sbox4_cameo_board(void);
static long dsl_misc_ioctl_compat(struct file *file, unsigned int unMode, unsigned long ulDataAddr);
static int dsl_misc_ioctl(struct inode *inode, struct file *file,
		unsigned int unMode, unsigned long ulDataAddr);
static int dsl_misc_open(struct inode *inode, struct file *file);
static int dsl_misc_release(struct inode *inode, struct file *file);
//static int dsl_misc_read(struct file *file, char* buf, size_t len, loff_t off);
//static int dsl_misc_write(struct file *file, const char* buf, size_t len, loff_t off);

static struct timer_list dsl_modem_mon_timer;
static volatile long sync_time = 0;
static volatile long status_time = 0;

struct file_operations dsl_misc_fops = {
//	read	: dsl_misc_read,
//	write	: dsl_misc_write,
	ioctl   : dsl_misc_ioctl,
	compat_ioctl   : dsl_misc_ioctl_compat,
	open    : dsl_misc_open,
	release : dsl_misc_release,
};

static struct miscdevice stSofaware_misc_miscdev = {
	minor   : SW_DSL_MISC_MINOR,
	name    : "dsl_misc",
	fops    : &dsl_misc_fops,
};

static long dsl_misc_ioctl_compat(struct file *file, unsigned int unMode, unsigned long ulDataAddr) {
   return dsl_misc_ioctl(NULL, file, unMode, ulDataAddr);
}

int get_dsl_sync_status(void)
{
	int rc = 0;

	if (sync_time)
		rc = ((jiffies-sync_time) > (HZ*3)) ? 1 : 0;

	return rc;
}

int  get_dsl_status(void)
{
	int rc = 0;
	
	if (status_time)
		rc = ((jiffies-status_time) > (HZ*3)) ? 1 : 0;
	
	return rc;
}

int __get_dsl_sync_status(void)
{
//	if ( octeon_is_sbox4_cameo_board() )
	{
//		return (octeon_gpio_value(18) ? 0 : 1);
	//	return (octeon_gpio_value(47) ? 0 : 1);
	}	
	return 1;
}

int __get_dsl_status(void)
{
	return 1;
}
int __dsl_modem_init(void)
{
	//do_spi_init();
	return 0;
}

int __dsl_modem_reset(void)
{

/************************************
extender 1 gpio 0 - DSL power
extender 1 gpio 6 - DSL RST
***************************************/
	extend_gpio_ctl(IC_SG80_EXTEND1, 0,0);
	msleep(1000);
	extend_gpio_ctl(IC_SG80_EXTEND1, 6,0);
	msleep(1000);
	extend_gpio_ctl(IC_SG80_EXTEND1, 0,1);
	msleep(1000);
	extend_gpio_ctl(IC_SG80_EXTEND1,6,1);
	msleep(500); // The DSL board reset time is 143 msec
	return 0;
}
int __dsl_modem_spi_mode(void)
{
//	// enable SPI function in GPIO configuration reg 2
//	GPIO_OR(0x9990,CONF2_RG);

//	/* reset(and hold) DSL board */
//	GPIO_AND(0xfb,DATA4_RG);

	return 0;
}

static void dsl_modem_mon_fn(unsigned long not_used)
{

	if (!sync_time && __get_dsl_sync_status())
		sync_time = jiffies;
	else if(!__get_dsl_sync_status())
		sync_time = 0;

	if (!status_time && __get_dsl_status())
		status_time = jiffies;
	else if(!__get_dsl_status())
		status_time = 0;

	dsl_modem_mon_timer.expires = jiffies+(HZ/2);
	add_timer (&dsl_modem_mon_timer);
	
	return;
}

///*****************************************************
 //* dsl_misc_read
 //******************************************************/
//static int dsl_misc_read(struct file *file, char* buf, size_t len, loff_t off)
//{
//	return 0;
//}
//
///*****************************************************
 //* dsl_misc_write
 //******************************************************/
//static int dsl_misc_write(struct file *file, const char* buf, size_t len, loff_t off)
//{
//	return 0;
//}

#ifdef _SPI_DBG_MODE
static void display_page(dsl_eeprom_page *dsl_pg)
{
	int i;
	
	printk("Page %d: \n    ", dsl_pg->page_num);
	for (i=0; i < SPI_EEPROM_PAGE_SIZE ; i++)
	{
		printk("%02x ", dsl_pg->buf[i]);
		if (i && ((i%35)==0))
			printk("\n    ");
	}
	printk("\n");
}
#endif

int	get_dsl_eeprom_page_size(void)
{
	return 0;//spi_get_page_size();
}

int	erase_dsl_eeprom(int pg_num)
{
	//spi_erase();
	return 0;
}

int	write_dsl_eeprom_page(dsl_eeprom_page *dsl_pg)
{
	return 0;//spi_write_page(dsl_pg->page_num, dsl_pg->buf);
}

int	read_dsl_eeprom_page(dsl_eeprom_page *dsl_pg)
{
	return 0;//spi_read(dsl_pg->page_num, dsl_pg->buf);
}

/*****************************************************
 * dsl_misc_ioctl
 ******************************************************/
static int dsl_misc_ioctl(struct inode *inode, struct file *file,
		unsigned int unMode, unsigned long ulDataAddr)
{
	int rc = 0, tmp, page_num;
	unsigned char* pucaData = (unsigned char *)ulDataAddr;
	dsl_eeprom_page dsl_pg;

	switch(unMode) {


		case IOC_DSL_RESET:
			__dsl_modem_reset();
			break;
			
		case IOC_DSL_SPI_MODE:
			__dsl_modem_spi_mode();
			break;

		case IOC_DSL_ERASE_EEPROM:
			// erase ALL and not just one sector - ignore input 
			if (erase_dsl_eeprom(page_num) != 0)
				rc = -EIO;
			break;
			
		case IOC_DSL_GET_EEPROM_PAGE_SIZE:
			tmp = get_dsl_eeprom_page_size();
			if (copy_to_user (pucaData, &tmp, sizeof(int))) {
				printk ("dsl_misc_ioctl() - failed to copy_to_user() \n");
				rc = -EFAULT;
				break;
			}
			break;

		case IOC_DSL_WRITE_EEPROM_PAGE:
			if (copy_from_user ((unsigned char*)&dsl_pg, pucaData, sizeof(dsl_pg))) {
				printk ("%s() - failed to copy_from_user()\n", __FUNCTION__);
				rc = -EFAULT;
				break;
			}

			if (write_dsl_eeprom_page(&dsl_pg) != 0)
			{
				printk("Failed to write eeprom page %d\n", dsl_pg.page_num);
				rc = -EIO;
			}
			break;

		case IOC_DSL_READ_EEPROM_PAGE:
			if (copy_from_user ((unsigned char*)&page_num, pucaData, sizeof(int))) {
				printk ("%s() - failed to copy_from_user()\n", __FUNCTION__);
				rc = -EFAULT;
				break;
			}

			dsl_pg.page_num = page_num;
			if (read_dsl_eeprom_page(&dsl_pg) != 0)
			{
				rc = -EIO;
				break;
			}

#ifdef _SPI_DBG_MODE
			printk("Read page:\n"); 
			display_page(&dsl_pg);
#endif
			
			if (copy_to_user (pucaData, &dsl_pg, sizeof(dsl_pg))) {
				printk ("dsl_misc_ioctl() - failed to copy_to_user() \n");
				rc = -EFAULT;
				break;
			}
			break;

		case IOC_DSL_GET_SYNC_STATUS:
			tmp = get_dsl_sync_status();
			if (copy_to_user (pucaData, &tmp, sizeof(int))) {
				printk ("dsl_misc_ioctl() - failed to copy_to_user() \n");
				rc = -EFAULT;
				break;
			}
			break;
		case IOC_DSL_GET_STATUS:
			tmp = get_dsl_status();
			if (copy_to_user (pucaData, &tmp, sizeof(int))) {
				printk ("dsl_misc_ioctl() - failed to copy_to_user() \n");
				rc = -EFAULT;
				break;
			}
			break;
		default:
			printk ("dsl_misc_ioctl() - unknown ioctl (%d) \n", unMode);
			rc = -1;
			break;
	}

	return rc;
}

/*****************************************************
 * dsl_misc_open
 ******************************************************/
static int dsl_misc_open(struct inode *inode, struct file *file)
{
	return 0;
}


/*****************************************************
 * dsl_misc_release
 ******************************************************/
static int dsl_misc_release(struct inode *inode, struct file *file)
{
	return 0;
}

static int __init dsl_init_module(void)
{
	extern int sofaware_get_dsl_annex(void);

	printk("Initialize DSL driver \n");

	if (!sofaware_get_dsl_annex()) {
		printk("dsl_init_module: hardware does not includ DSL modem \n");
		return -EINVAL;
	}

	__dsl_modem_init();
	__dsl_modem_reset();

	init_timer(&dsl_modem_mon_timer);
	dsl_modem_mon_timer.function = dsl_modem_mon_fn;
	dsl_modem_mon_timer.data = 0;
	dsl_modem_mon_timer.expires = jiffies + (HZ);
	add_timer(&dsl_modem_mon_timer);

	if (misc_register(&stSofaware_misc_miscdev) ) {
		printk("dsl_misc_init: can't register misc device\n");
		return -EINVAL;
	}

	return 0;
}

/**
 * dsl_misc_module() - Q message module cleanup
 *
 */
static void __exit dsl_clean_module(void)
{
	printk ("Cleanup DSL driver \n");

	misc_deregister(&stSofaware_misc_miscdev); 

	del_timer_sync (&dsl_modem_mon_timer);

}

module_init(dsl_init_module);
module_exit(dsl_clean_module);





