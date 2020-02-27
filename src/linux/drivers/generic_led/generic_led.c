
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/string.h>
#include <linux/ctype.h>
#include <linux/proc_fs.h>
#include <linux/workqueue.h>
#include <linux/led_ctl.h>
#include <linux/capability.h>
#include <asm/uaccess.h>

#undef DEBUG_GL

#ifdef DEBUG_GL
#define GL_DEBUG(fmt, arg...) printk("%s:%d - " fmt, __FUNCTION__, __LINE__, ## arg)
#else
#define GL_DEBUG(fmt, arg...) ;
#endif
#define IC_SG80_EXTEND1 0x21
#define IC_SG80_EXTEND2 0x20


#define BLINK_FOREVER	           (-1)
#define FAST_BLINK_INTERVAL        (HZ/8)
#define SLOW_BLINK_INTERVAL	       (HZ)

#define PROC_LEDS_NAME             "leds"


static int proc_generic_led_read (char *page, char **start, off_t off,
		        int count, int *eof, void *data);
static int proc_generic_led_write(struct file *file, const char __user *buffer,
		                   unsigned long count, void *data);
static void led_work_fn(struct work_struct *work);

static DEFINE_SPINLOCK(leds_lock);
static LIST_HEAD(leds);
static struct proc_dir_entry *proc_leds;
static struct workqueue_struct *led_wq;  /* main LEDs work queue */

static char* def_led_colors_tbl[LED_COLOR_NAME_LEN] = {"off", "green", "red", "orange", NULL};
extern void extend_gpio_ctl(int addr,int num, int status);
//static char buf[100];
static void set_gpio_state(char *str)
{
	/* parse speed */
	char *ptr;
	int pin_num,extender_no;
	int status;
	printk(" enter set_gpio_state str=%s",str);

	ptr = strstr(str, "extender:");
	ptr += sizeof ("extender:")-1;
	if (*ptr == '1' || *ptr == '2') {
		extender_no = (*ptr)-'0';
	}
	printk(" extender set_gpio_state %c",*ptr);
	ptr = strstr(str, "pin:");
	ptr += sizeof ("pin:")-1;
	if (*ptr >= '0' && *ptr <= '7') {
		pin_num = (*ptr)-'0';
	}
	printk(" pin set_gpio_state %c",*ptr);
	ptr = strstr(str, "status:");
	ptr += sizeof ("status:")-1;
	if (*ptr == '0' || *ptr == '1') {
		status = (*ptr)-'0';
	}
	printk(" status set_gpio_state %c",*ptr);

	printk(" enter set_gpio_state %d %d",pin_num,status);
	if (extender_no == 1)
		extend_gpio_ctl(IC_SG80_EXTEND1,pin_num,status);
	else
		extend_gpio_ctl(IC_SG80_EXTEND2,pin_num,status);



}




void __gl_set_led_color(gl_led_info_t* led, int color)
{

	GL_DEBUG("<%s> set color (color %d)\n", led->name, color);

	if (color == LED_COLOR_UNDEFINED)
		return;

	led->color = color;
	led->led_ctl_fn(color);

	return;
}

void __gl_set_led_blink_speed(gl_led_info_t* led, unsigned int blink_speed)
{
	if (blink_speed > 9)
		blink_speed = 9;

	GL_DEBUG("<%s> set blink (speed: %d)\n", led->name, blink_speed);

	if (!blink_speed) {
		cancel_delayed_work(&led->led_work);
		__gl_set_led_color(led, led->color);
	//	led->blink_speed = blink_speed;
	}
	else {
		led->blink_speed = blink_speed;
		queue_delayed_work (led_wq, &led->led_work, HZ/led->blink_speed);
	}

	return;
}

void __gl_set_led_temp_blink(gl_led_info_t* led, unsigned int blink_speed, int cycles, int color)
{
	unsigned long flags;
	int already_tmp_blink = 0;

	if (blink_speed > 9)
		blink_speed = 9;
	if (blink_speed == 0)
		blink_speed = 1;

	GL_DEBUG("<%s> set tmp blink (speed: %d, cycles: %d, color %d)\n", 
			led->name, blink_speed, cycles, color);

    spin_lock_irqsave(&led->lock, flags);

	already_tmp_blink = led->tmp_blink_speed;

	led->tmp_blink_speed = blink_speed;
	led->tmp_blink_cycles = cycles*2;

	if (color==LED_COLOR_UNDEFINED && led->color==LED_COLOR_OFF)
		led->tmp_color = 1;
	else if (color==LED_COLOR_UNDEFINED)
		led->tmp_color = led->color;
	else 
		led->tmp_color = color;

	if (already_tmp_blink)
		queue_delayed_work (led_wq, &led->led_work, HZ/led->tmp_blink_speed);

    spin_unlock_irqrestore(&led->lock, flags);

	return;
}


int register_led(gl_led_info_t* led, const char* name, gl_led_ctl_fn_t led_ctl_fn)
{
	unsigned long flags;

	memset (led, 0, sizeof(gl_led_info_t));

	led->led_ctl_fn = led_ctl_fn;
	led->color = LED_COLOR_OFF;
	led->led_colors_tbl = def_led_colors_tbl;
	INIT_LIST_HEAD(&led->entry);
	INIT_DELAYED_WORK(&led->led_work, led_work_fn);
	spin_lock_init(&led->lock);
	strncpy (led->name, name, LED_NAME_LEN);

    spin_lock_irqsave(&leds_lock, flags);
    list_add_tail(&led->entry, &leds);
    spin_unlock_irqrestore(&leds_lock, flags);

	led->proc = create_proc_entry(name, 0644, proc_leds);
	if (led->proc == NULL) {
		printk("ERROR: failed to create /proc/%s/%s\n",
				PROC_LEDS_NAME, name);
		return -ENOMEM;
	}

	led->proc->read_proc  = proc_generic_led_read;
	led->proc->write_proc = proc_generic_led_write;
	led->proc->owner 	  = THIS_MODULE;
	led->proc->data       = led;

	led->led_ctl_fn(0);

	return 0;
}


int unregister_led(gl_led_info_t* led)
{
	unsigned long flags;
	cancel_delayed_work(&led->led_work);
	flush_workqueue(led_wq);
	remove_proc_entry(led->name, proc_leds);

    spin_lock_irqsave(&leds_lock, flags);
    list_del(&led->entry);
    spin_unlock_irqrestore(&leds_lock, flags);

	return 0;
}

#if 0
/**
 * convert LED color name to number
 *
 */
static int get_color_id_by_name (const char* color_name, char** colors_tbl)
{
	int color_id;
	 
	for (color_id = 0 ; colors_tbl[color_id] ; color_id++) 
	{
		if (strcmp(color_name, colors_tbl[color_id]) == 0) {
			break;
		}
	}

	return colors_tbl[color_id] ?color_id:(-1);
}
#endif 

/**
 * convert LED color number to color name
 *
 */
static const char *get_color_name_by_id (int color_id, char** colors_tbl)
{
	int i;

	for (i = 0 ; colors_tbl[i] ; i++);

	return (color_id<=i)?colors_tbl[color_id]:"unknown";
}

/**
 * LED work handler 
 *
 */
static void led_work_fn(struct work_struct *work)
{
	unsigned long flags;
	gl_led_info_t *led =
		container_of(work, struct __gl_led_info_t, led_work.work);

    spin_lock_irqsave(&led->lock, flags);

	led->onoff = !led->onoff;

	if (led->tmp_blink_cycles) {
		/* temporary blink */
		led->tmp_blink_cycles--;
		led->led_ctl_fn(led->onoff?led->tmp_color:0);
		queue_delayed_work (led_wq, &led->led_work, HZ/led->tmp_blink_speed);
		spin_unlock_irqrestore(&led->lock, flags);
		return; 
	}

	if (led->blink_speed) {
		/* blink LED */
		led->led_ctl_fn(led->onoff?led->color:0);
		queue_delayed_work (led_wq, &led->led_work, HZ/led->blink_speed);
	}
	else {
		/* stop blink, revert the LED color */
		led->led_ctl_fn(led->color);
	}

    spin_unlock_irqrestore(&led->lock, flags);
	return;
}	

/**
 * This function is called with the /proc LED file is read
 *
 */
static int proc_generic_led_read(char *page, char **start, off_t off, 
		int cycles, int *eof, void *data)
{
	gl_led_info_t* led = data;
	unsigned long out_bytes = 0;

	out_bytes += sprintf (page+out_bytes, "color           : %s\n", 
			get_color_name_by_id(led->color, led->led_colors_tbl));
	out_bytes += sprintf (page+out_bytes, "blink speed     : %d\n", 
			led->blink_speed);

	if (led->tmp_blink_cycles) {
		out_bytes += sprintf (page+out_bytes, "tmp blink cycles : %d\n", 
				led->tmp_blink_cycles);
		out_bytes += sprintf (page+out_bytes, "tmp color       : %s\n", 
				get_color_name_by_id(led->tmp_color, led->led_colors_tbl));
		out_bytes += sprintf (page+out_bytes, "tmp blink speed : %d\n", 
				led->tmp_blink_speed);
	}

	*eof = 0;

	return out_bytes;
}

/**
 * This function is called with the LED proc file is written
 *
 */

static int proc_generic_led_write(struct file *file, const char __user *buffer,
		                           unsigned long count, void *data)
{
	gl_led_info_t* led = data;
	char *buf, *s;
	int color, blink_speed;
	int is_tmp_blink = 0;
	int i;


	color = led->color;
	blink_speed = led->blink_speed;

	if (!capable(CAP_SYS_ADMIN))
		return -EACCES;

	if (count >= PAGE_SIZE)
		return -EINVAL;

	s = buf = (char *)__get_free_page(GFP_USER);
	if (!buf)
		return -ENOMEM;

	if (copy_from_user(buf, buffer, count)) {
		free_page((unsigned long)buf);
		return -EFAULT;
	}

	buf[count] = '\0';

	/* parse color */
	for (i = 0 ; led->led_colors_tbl[i] ; i++)
	{
		if (strstr(buf, led->led_colors_tbl[i]) ) {
			color = i;
			break;
		}
	}
	if (strstr(buf, "extender:")) {
			set_gpio_state(buf);
			return 1;
	}

	/* parse speed */
	if (strstr(buf, "speed:")) {
		char *ptr = strstr(buf, "speed:");

		ptr += sizeof ("speed:")-1;

		if (*ptr >= '0' && *ptr <= '9') {
			blink_speed = (*ptr)-'0';
		}
	}

	/* parse is temporary blink */
	if (strstr(buf, "tmp")) {
		is_tmp_blink = 1;
	}

	if (!is_tmp_blink) {
		__gl_set_led_color(led, color);
		__gl_set_led_blink_speed(led, blink_speed);
	}
	else {
		__gl_set_led_temp_blink(led, blink_speed, 4, color);
	}

	free_page((unsigned long)buf);

	return count;
}


/**
 * Module/ driver initialization.
 *
 */
static int __init generic_led_init_module(void)
{

	printk("Generic LED driver initialize\n");

	led_wq = create_singlethread_workqueue("led_wq");
	if (led_wq == NULL) {
		printk ("ERROR: failed to allocate LED work queue\n");
		return -1;
	}

	proc_leds = proc_mkdir(PROC_LEDS_NAME , NULL);
	if (!proc_leds) {
		printk ("ERROR: failed to dreate /proc/%s\n", PROC_LEDS_NAME);
		destroy_workqueue(led_wq);
		return -ENOMEM;
	}
  
	return 0;
}


/**
 * Module / driver shutdown
 *
 */
static void __exit generic_led_cleanup_module(void)
{
	printk("Generic LED driver exit\n");

	destroy_workqueue(led_wq);
	remove_proc_entry(PROC_LEDS_NAME, NULL);
}

MODULE_LICENSE("GPL"); 
MODULE_AUTHOR("Uri Yosef <uriy@sofaware.com>");
MODULE_DESCRIPTION("LEDs control driver.");
module_init(generic_led_init_module);
module_exit(generic_led_cleanup_module);


EXPORT_SYMBOL(register_led);
EXPORT_SYMBOL(unregister_led);
EXPORT_SYMBOL(__gl_set_led_color);
EXPORT_SYMBOL(__gl_set_led_blink_speed);

