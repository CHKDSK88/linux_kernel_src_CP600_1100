/*
 * drivers/char/sofaware_led.c
 */

/* INCLUDE FILE DECLARATIONS  
 */
//#include <linux/config.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/fcntl.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/miscdevice.h>
#include <linux/sched.h>
#include <linux/fs.h>
#include <asm/uaccess.h>

        
#include <linux/sofaware_led.h>
#include <linux/led_ctl.h>

#include <marvell/mvHwFeatures.h>

#define OFF_COLOR                 0
#define GREEN_COLOR               1
#define RED_COLOR                 2
#define ORANGE_COLOR              3

#define SLOW_BLINK_SPEED          1
#define FAST_BLINK_SPEED          7
#define BLINK_ONCE_SPEED          2

#define SHORT_BLINK_CYCLE_NUM     2
#define LONG_BLINK_CYCLE_NUM      4

static gl_led_info_t usb1_led;
static gl_led_info_t usb2_led;
static gl_led_info_t power_led;
static gl_led_info_t alert_led;
static gl_led_info_t wifi_led;
static gl_led_info_t adsl_act_led;
static gl_led_info_t adsl_link_led;
static gl_led_info_t internet_led;

static int test_mode = 0;

        
static int sofaware_led_ioctl(struct inode *inode, struct file *file,
		unsigned int led, unsigned long stat);
static int sofaware_led_open(struct inode *inode, struct file *file);
static int sofaware_led_release(struct inode *inode, struct file *file);

/* test mode LEDs handling.
 */
static unsigned long test_mode_count; 
static void update_test_mode_leds(unsigned long data);
static DEFINE_TIMER(test_mode_timer, update_test_mode_leds, 0, 0);

//extern int sofaware_is_test_mode(void);

struct file_operations stSofaware_led_fops = {
	ioctl   : sofaware_led_ioctl,
	open    : sofaware_led_open,
	release : sofaware_led_release,
};

static struct miscdevice stSofaware_led_miscdev = {
	minor   : SW_LED_MISC_MINOR,
	name    : "sofaware_led",
	fops    : &stSofaware_led_fops,
};    

/*****************************************************
 * sofaware_led_ioctl
 ******************************************************
 */
static int sofaware_led_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	ioc_set_led led_cmd;

	switch (cmd)
	{
		case  IOC_SW_SET_LED:
			if (copy_from_user ((void*)&led_cmd, (void*)arg, sizeof(ioc_set_led))) {
				printk ("ledc_ioctl(%d) - failed to copy_from_user() \n", cmd);
				ret = -EFAULT;
				break;
			}
			ret = set_led_state(led_cmd.led, led_cmd.led_stat) ? -1 : 0;
			break;
		default:	
			ret = -EFAULT;
			break;

	}
	return ret;
}


/*****************************************************
 * sofaware_led_open
 ******************************************************
 */
static int sofaware_led_open(struct inode *inode, struct file *file)
{
	return 0;
}


/*****************************************************
 * sofaware_led_release
 ******************************************************
 */
static int sofaware_led_release(struct inode *inode, struct file *file)
{
	return 0;
}

static void __dummy_led_ctl (int color)
{
	return;
}


static int __set_led_state(SW_LED sw_led, SW_LED_STAT sw_stat)
{
	gl_led_info_t *led;
	int color = -1;
	int blink_speed = 0;
	int is_tmp_blink = 0;
	int tmp_blink_cycles;

	switch (sw_led)
	{
		case USB1_LED:
			led = &usb1_led;
			break;
		case USB2_LED:
			led = &usb2_led;
			break;
		case POWER_LED:
			led = &power_led;
			break;
		case ALERT_LED:
			led = &alert_led;
			break;
		case INTERNET_LED:
			led = &internet_led;
			break;
		case ADSL_ACT:
			led = &adsl_act_led;
			break;
		case ADSL_LINK:
			led = &adsl_link_led;
			break;
		case WIFI1_LED:
			led = &wifi_led;
			break;

		default:
			printk ("ERROR: unknown LED (%d)\n", sw_led);
			return -1; 
	}
	
	switch (sw_stat)
	{
		case SW_LED_OFF:
			color = OFF_COLOR;
			break;
		case SW_LED_RED:
			color = RED_COLOR;
			break;
		case SW_LED_FAST_BLINKING_RED:
			color = RED_COLOR;
			blink_speed = FAST_BLINK_SPEED;
			break;
		case SW_LED_ATTACK:
			color = ORANGE_COLOR;
			blink_speed = FAST_BLINK_SPEED;
			is_tmp_blink= 1;
			tmp_blink_cycles = SHORT_BLINK_CYCLE_NUM;
			break;
		case SW_LED_VIRUS:
			color = RED_COLOR;
			blink_speed = FAST_BLINK_SPEED;
			is_tmp_blink= 1;
			tmp_blink_cycles = LONG_BLINK_CYCLE_NUM;
			break;
		case SW_LED_SLOW_BLINKING_RED:
			color = RED_COLOR;
			blink_speed = SLOW_BLINK_SPEED;
			break;
		case SW_LED_BLINK_ONCE:
			blink_speed = BLINK_ONCE_SPEED;
			is_tmp_blink= 1;
			tmp_blink_cycles = SHORT_BLINK_CYCLE_NUM;
			break;
		case SW_LED_BLINK_ONCE_LONG:
			blink_speed = BLINK_ONCE_SPEED;
			is_tmp_blink = 1;
			tmp_blink_cycles = LONG_BLINK_CYCLE_NUM;
			break;
		case SW_LED_GREEN:
			color = GREEN_COLOR;
			break;
		case SW_LED_FAST_BLINKING_GREEN:
			color = GREEN_COLOR;
			blink_speed = FAST_BLINK_SPEED;
			break;
		case SW_LED_SLOW_BLINKING_GREEN:
			color = GREEN_COLOR;
			blink_speed = SLOW_BLINK_SPEED;
			break;
		case SW_LED_ORANGE:
			color = ORANGE_COLOR;
			break;
		case SW_LED_FAST_BLINKING_ORANGE:
			color = ORANGE_COLOR;
			blink_speed = FAST_BLINK_SPEED;
			break;
		case SW_LED_SLOW_BLINKING_ORANGE:
			color = ORANGE_COLOR;
			blink_speed = SLOW_BLINK_SPEED;
			break;
		case SW_LED_UNDEFINED:
		default:
			printk ("ERROR: unknown LED stat %d on LED %d\n", sw_stat, sw_led);
			return -1; 
			break;
	}

	if (!is_tmp_blink) {
		__gl_set_led_color(led, color);
		__gl_set_led_blink_speed(led, blink_speed);
	}
	else {
		__gl_set_led_temp_blink(led, blink_speed, tmp_blink_cycles, color);
	}
	return 0;
}

static int __set_led_reason(SW_LED led_id, unsigned int reason,unsigned char turnOn)
{
	gl_led_info_t *led;
	int color = -1;
	int blink_speed = 0;
	int reason_priority = 0;

	switch (led_id)
	{
		case USB1_LED:
			led = &usb1_led;
			break;
		case USB2_LED:
			led = &usb2_led;
			break;
		case POWER_LED:
			led = &power_led;
			break;
		case ALERT_LED:
			led = &alert_led;
			break;
		case INTERNET_LED:
			led = &internet_led;
			break;
		case WIFI1_LED:
			led = &wifi_led;
			break;
		default:
			printk ("ERROR: unknown LED (%d)\n", led_id);
			return -1;
	}
	set_led_state_by_reason(led_id,led,reason,turnOn,&color,&blink_speed);
	printk("__set_led_reason: color=%d,blink_speed=%d",color,blink_speed);


	__gl_set_led_color(led, color);
	__gl_set_led_blink_speed(led, blink_speed);

	return 0;
}
void set_led_state_by_reason(SW_LED led_id,gl_led_info_t *led,unsigned int reason,unsigned char turnOn,int *color,int *blink_speed)
{
	int priority = 0;
	if (turnOn)
		led->reason_bit |= reason;
	else
		led->reason_bit &= (~reason);
	switch (led_id)
	{
		case USB1_LED:
			//getUsb1State(&usb1_led,&color,&blink_speed);
			break;
		case USB2_LED:
			//led = &usb2_led;
			break;
		case POWER_LED:
			//led = &power_led;
			break;
		case ALERT_LED:
			priority = get_alert_priority(&alert_led);
			break;

		default:
			printk ("ERROR: unknown LED (%d)\n", led_id);
			break;
		//	return -1;
	}

	switch (priority)
	{
		case LED_NONE:
			*color = OFF_COLOR;
			break;
		case LED_SLOW_BLINKING_RED:
			*color = RED_COLOR;
			*blink_speed = SLOW_BLINK_SPEED;
			break;
		case LED_FAST_BLINKING_RED:
			*color = RED_COLOR;
			*blink_speed = FAST_BLINK_SPEED;
			break;
	}


}

int get_alert_priority(gl_led_info_t *alert_led)
{

	if (!alert_led->reason_bit )
	{
		printk("set_led_state_by_reasoncolor alert_led=%d",alert_led);
		return LED_NONE;
	}
	if ((alert_led->reason_bit & LED_BIT_MEM_LOW )||(alert_led->reason_bit & LED_BIT_CPU_HIGH)||(alert_led->reason_bit & LED_BIT_CONN_TAB))
	{
		return LED_FAST_BLINKING_RED;
	}

	if ((alert_led->reason_bit & LED_BIT_NO_MGMT)||(alert_led->reason_bit & LED_BIT_WAN_ERR))
	{
		return LED_SLOW_BLINKING_RED;
	}

	return -1;
}

int set_led_state(SW_LED sw_led, SW_LED_STAT sw_stat)
{
	return __set_led_state(sw_led, sw_stat);
}

int set_led_reason(SW_LED sw_led, unsigned int reason,unsigned char turnOn)
{
	return __set_led_reason(sw_led, reason,turnOn);
}
static void update_test_mode_leds(unsigned long data)
{
/*
	Every 1 hour, change the LED state as below:
		1 hour passed: USB 
		2 hours passed: VPN
		3 hours passed: VPN + USB
		4 hours passed: SEC
		5 hours passed: SEC + USB
		6 hours passed: SEC + VPN
		7 hours passed: SEC + VPN + USB
		8 hours passed: SYS
		
		After 8 hours burn-in, the SYS led should be lit up; otherwise, the DUT should be treated as NG
*/

	//__set_led_state(POWER_LED, SW_LED_OFF);
	//__set_led_state(USB2_LED, SW_LED_OFF);
	//__set_led_state(USB1_LED, SW_LED_OFF);
	//__set_led_state(ALERT_LED, SW_LED_OFF);



	switch(test_mode_count) {
		case 0:
			__set_led_state(POWER_LED, SW_LED_GREEN);
			break;
		case 1:
			__set_led_state(USB1_LED, SW_LED_GREEN);
			break;
		case 2:
		//	__set_led_state(USB2_LED, SW_LED_GREEN);
		//	__set_led_state(USB1_LED, SW_LED_GREEN);
			break;
		case 3:
		//	__set_led_state(ALERT_LED, SW_LED_GREEN);
			break;
		case 4:
		//	__set_led_state(ALERT_LED, SW_LED_GREEN);
		//	__set_led_state(USB2_LED, SW_LED_GREEN);
			break;
		case 5:
		//	 __set_led_state(USB2_LED, SW_LED_GREEN);
			 __set_led_state(USB1_LED, SW_LED_GREEN);
			break;
		case 6:
		//	__set_led_state(ALERT_LED, SW_LED_GREEN);
		//	__set_led_state(USB2_LED, SW_LED_GREEN);
			__set_led_state(USB1_LED, SW_LED_GREEN);
			break;
		case 7:
		default:	
			//__set_led_state(ALERT_LED, SW_LED_GREEN);
			return;
			break;
	}
	
	test_mode_count++;
	test_mode_timer.expires = jiffies + (HZ*60*60);
	add_timer(&test_mode_timer);          

	return;
}	

void sofaware_usb1_led_act (void)
{
	__gl_set_led_temp_blink(&usb1_led, BLINK_ONCE_SPEED, SHORT_BLINK_CYCLE_NUM, LED_COLOR_UNDEFINED);
}
void sofaware_usb1_led_onoff(int on)
{
	__gl_set_led_color(&usb1_led, on?GREEN_COLOR:OFF_COLOR);
	__gl_set_led_blink_speed(&usb1_led, 0);
}
void sofaware_usb2_led_act (void)
{
	__gl_set_led_temp_blink(&usb2_led, BLINK_ONCE_SPEED, SHORT_BLINK_CYCLE_NUM, LED_COLOR_UNDEFINED);
}
void sofaware_usb2_led_onoff(int on)
{
	__gl_set_led_color(&usb2_led, on?GREEN_COLOR:OFF_COLOR);
	__gl_set_led_blink_speed(&usb2_led, 0);
}
void sofaware_internet_led_onoff(int on)
{
	__gl_set_led_color(&internet_led, on?RED_COLOR:OFF_COLOR);
	__gl_set_led_blink_speed(&internet_led, 0);
}
void sofaware_internet_led_act (void)
{
	__gl_set_led_temp_blink(&internet_led, BLINK_ONCE_SPEED, SHORT_BLINK_CYCLE_NUM, LED_COLOR_UNDEFINED);
}
void sofaware_wifi_led_onoff(int on)
{
	__gl_set_led_color(&wifi_led, on?GREEN_COLOR:OFF_COLOR);
	__gl_set_led_blink_speed(&wifi_led, 0);
}
void sofaware_wifi_led_act (void)
{
	__gl_set_led_temp_blink(&wifi_led, BLINK_ONCE_SPEED, SHORT_BLINK_CYCLE_NUM, LED_COLOR_UNDEFINED);
}
void sofaware_adsl_act_led_onoff(int on)
{
	__gl_set_led_color(&adsl_act_led, on?GREEN_COLOR:OFF_COLOR);
	__gl_set_led_blink_speed(&adsl_act_led, 0);
}
void sofaware_adsl_act_led_act (void)
{
	__gl_set_led_temp_blink(&adsl_act_led, BLINK_ONCE_SPEED, SHORT_BLINK_CYCLE_NUM, LED_COLOR_UNDEFINED);
}
void sofaware_adsl_link_led_onoff(int on)
{
	__gl_set_led_color(&adsl_link_led, on?GREEN_COLOR:OFF_COLOR);
	__gl_set_led_blink_speed(&adsl_link_led, 0);
}
void sofaware_adsl_link_led_act (void)
{
	__gl_set_led_temp_blink(&adsl_link_led, BLINK_ONCE_SPEED, SHORT_BLINK_CYCLE_NUM, LED_COLOR_UNDEFINED);
}

/*****************************************************
 * sofaware_led_init
 ******************************************************
 */
int __init sofaware_led_init(void)
{
	//extern int octeon_is_arrow_board(void);
	if(is_seattle_board())
		init_leds_seattle();
	else //LONDON
		init_leds_london();
	extern void __alert_led_ctl(int color);
	extern void __usb1_led_ctl(int color);
	extern void __usb2_led_ctl(int color);
	extern void __power_led_ctl(int color);
	extern void __internet_led_ctl(int color);
	extern void __wifi_led_ctl(int color);
	extern void __adsl_act_led_ctl(int color);
	extern void __adsl_link_led_ctl(int color);
//	printk("SofaWare LED driver initialize %s\n", sofaware_is_test_mode()?"(test mode)":"");

	register_led(&usb1_led, "usb1", __usb1_led_ctl);
	register_led(&usb2_led, "usb2", __usb2_led_ctl);
	register_led(&alert_led, "alert", __alert_led_ctl);
	register_led(&power_led, "power", __power_led_ctl);
	register_led(&internet_led, "internet", __internet_led_ctl);
	register_led(&wifi_led, "wifi", __wifi_led_ctl);

	if (misc_register(&stSofaware_led_miscdev)) {
		printk("sofaware_led_init: can't register misc device \n");
		return -EINVAL;
	}
	__set_led_state(POWER_LED, SW_LED_GREEN);
	__set_led_state(ALERT_LED, SW_LED_FAST_BLINKING_GREEN);

	if (test_mode) {
		test_mode=0;
		test_mode_timer.expires = jiffies;
		add_timer(&test_mode_timer);          
		__set_led_state(ALERT_LED, SW_LED_FAST_BLINKING_GREEN);
	}

	return 0;
}


/**
 * Module / driver shutdown
 *
 */
void __exit sofaware_led_cleanup(void)
{
	printk("CP LEDs driver exit\n");

	unregister_led(&alert_led);
	unregister_led(&power_led);
	unregister_led(&usb1_led);
	unregister_led(&usb2_led);
	unregister_led(&internet_led);
	unregister_led(&wifi_led);
}

module_init (sofaware_led_init);
module_exit (sofaware_led_cleanup);

EXPORT_SYMBOL(set_led_state);
EXPORT_SYMBOL(set_led_reason);
