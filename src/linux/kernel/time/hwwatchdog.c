/**************  HW WATCHDOG ***********************
		Hardware watchdog for SG-80
		==============================
	1. Hardware watchdog
		The Feroceon processor suggests a HW watchdog that may be used to verify S/W sanity.
		The HW watchdog is used by writing to the Feroceon registers.
		In order to arm the HW watchdog - first, the CPU HW watchdog timer register (0x20324) should be set,
		then the CPUWDTimerEn field in CPU Timers Control Register (0x20300) and the WDRstOutEn field 
		in the RSTOUTn Mask Register (0x20108) should be set.
		In order to disarm the HW watchdog, one of the enable field should be cleared.
		
		When the HW watchdog is armed, and the timer interval was set (0- 0xffffffff: up to 20 seconds) - 
		the timer value is decreased by 1 (every clock tick - according to CPU frequency). When the timer value
		reaches 0 - the CPU is reset.
		
		In order not no reset the CPU - the timer interval should be reloaded by S/W before it reaches 0.
		
		There is also an option for a periodic HW watchdog which does not reset the CPU when reaches 0 - it only raises
		an interrupt and the timer interval is reloaded again.
		
	2. Software watchdog for the hardware watchdog
		In order to allow longer HW watchdog period than 20 seconds, a S/W watchdog is needed.
		The S/W watchdog arms the H/W watchdog repeatedly (longer before is expires to overcome
		system overloads) - in order to keep it from reseting the CPU.
		The S/W for H/W watchdog is a kernel thread which is scehduled to run every 1-3 (TBD!!) seconds in order 
		to arm the H/W watchdog.
		
	3. Software watchdog for the hardware watchdog for software upgrade
		In SG-80, the H/W watchdog is used to defend the software upgrade procedure.
		When upgrading a software version - there is a possibility that the image is corrupted or buggy.
		In this case the upgrade procedure may stuck in the middle - leaving the appliance stuck with no communication to the 
		management center - not allowing to revert the upgrade procedure.
		
		The H/W watchdog may be armed at boot time (if S/W download + Linux uptime is small enough) or at at 
		the earliest possible time of the Linux kernel initialization.
		The S/W watchdog is started at the earliest possible time of the Linux kernel initialization.
		
		Using the S/W-H/W watchdog - if the upgrade procedure did not complete - the H/W watchdog will reset the CPU.
		In order to acheive this - the S/W for H/W watchdog is configured to run a configurable time. After this time,
		the S/W watchdog will exit and wil not not rearm the H/W watchdog. Then the H/W watchdog will reset the CPU.
		If the upgrade procedure is completed successfully befored the configurable time - the software watchdog will
		be instructed to disable the H/W watchdog.
		
		The application may also increase the S/W timeout in order to allow configuring the timeout at each stage of the upgrade
*/


#include <linux/sysdev.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/jiffies.h>

#define HWWDOG_SLEEP 3
#define HWWDOG_NOACT 0
#define HWWDOG_START 1
#define HWWDOG_STOP  2
#define HWWDOG_CONT  3
#define HWWDOG_SW_TMOUT 600
#define HWWDOG_HW_TMOUT 15
#define HWWDOG_ONE_SEC 0x11111111 // about 1.5 seconds

int wdog_action = HWWDOG_NOACT;
unsigned int wdog_time = 0;
unsigned int wdog_timeout = 0;
static struct task_struct *s_worker_thread;
extern void mv_config_hwwatchdog (unsigned int time, int start);
extern void mv_read_hwwatchdog (unsigned int *time, int *start);
static DEFINE_SPINLOCK(hwwdog_lock);
static char hwwdog_name[32];
u64 hwwdog_start;
u64 hwwdog_end;

#ifdef CONFIG_SYSFS
/**
 * sysfs_show_hwwatchdof - sysfs interface for hardware watchdog
 * @dev:	unused
 * @buf:	char buffer to be filled with clocksource list
 *
 * Provides sysfs interface for showing hardware watchdog.
 */
static ssize_t
sysfs_show_hwwatchdof(struct sys_device *dev, char *buf)
{
	char *curr = buf;
	unsigned int time;
	int start;
	u64 current_jiffies;
	//use uint32 to divide - compiler does not let u64 division
	unsigned int elapsed, end_tmout;

	current_jiffies = get_jiffies_64();

	spin_lock(&hwwdog_lock);
	mv_read_hwwatchdog (&time, &start);
	/* adjust read values to seconds */
	time = time / HWWDOG_ONE_SEC;
	if (time > 0xf) // check overflow maximum 0xffffffff
		time = 0xf;
	if (start != 0)
		start = 1;
		
	current_jiffies = get_jiffies_64();
	elapsed = current_jiffies - hwwdog_start; //diff is in u64
	elapsed = elapsed / HZ; // div in u32
	
	end_tmout = hwwdog_end - hwwdog_start; //diff in u64
	end_tmout = end_tmout / HZ; // div in u32

	curr += sprintf(curr, "%s HWWDOG:%u, elapsed:%u, end:%u, last timer:%u", 
				(start==1)?"start":"stop", time, elapsed, end_tmout, wdog_timeout );
	
	spin_unlock(&hwwdog_lock);

	curr += sprintf(curr, "\n");

	return curr - buf;
}

/**
 * sysfs_config_hwwatchdog - interface for manually configuring hw watchdog
 * @dev:	unused
 * @buf:	name of override clocksource
 * @count:	length of buffer
 *
 * Takes input from sysfs interface for manually overriding hw watchdog configuration
  */
/** 
	start - echo "start 300" > /sys/devices/system/hwwatchdog/hwwatchdog0/hw_watchdog
    stop  - echo "stop 0" > /sys/devices/system/hwwatchdog/hwwatchdog0/hw_watchdog
    cont -  echo "cont 300" > /sys/devices/system/hwwatchdog/hwwatchdog0/hw_watchdog
    write - echo "write 1 15 " > /sys/devices/system/hwwatchdog/hwwatchdog0/hw_watchdog
**/
int ksw_hwwdog_init(void);
static ssize_t sysfs_config_hwwatchdog(struct sys_device *dev,
					  const char *buf, size_t count)
{
	size_t ret = count;
	unsigned int time;
	u64 current_jiffies;

	/* strings from sysfs write are not 0 terminated! */

	if (count >= sizeof(hwwdog_name))
		return -EINVAL;

	/* strip of \n: */
	if (buf[count-1] == '\n')
		count--;
	
	//init to max HW WDOG time - configurable only by "write" action
	time = HWWDOG_HW_TMOUT;

	spin_lock(&hwwdog_lock);

	if (count > 0)
		memcpy(hwwdog_name, buf, count);
	hwwdog_name[count] = 0;

	if (strncmp(hwwdog_name,"start", 5) == 0)
	{
		if (wdog_action == HWWDOG_NOACT)
		{
			/* get sw watchdog time to wait in sec */
			sscanf(hwwdog_name+6, "%u", &wdog_timeout);
			//let max timeout 6*default = 1hour
			if (wdog_timeout > (HWWDOG_SW_TMOUT*6))
			{
				wdog_timeout = HWWDOG_SW_TMOUT*6;
			}
			wdog_action = HWWDOG_START;
			wdog_time = HWWDOG_ONE_SEC*time;
			hwwdog_start = get_jiffies_64();
			hwwdog_end = hwwdog_start + (wdog_timeout * HZ);
			printk("start HW WDOG %u %u\n", time, wdog_timeout);
			ksw_hwwdog_init();
		}
		else
		{
			printk("HW WDOG: start, HW watchdog already started - stop first\n");
		}
	}

	if (strncmp(hwwdog_name,"cont", 4) == 0)
	{
		/* get sw watchdog time to wait in sec */
		sscanf(hwwdog_name+5, "%u", &wdog_timeout);
		//let max timeout 12*default = 1 hour
		if (wdog_timeout > (HWWDOG_SW_TMOUT*12))
		{
			wdog_timeout = HWWDOG_SW_TMOUT*12;
		}
		wdog_time = HWWDOG_ONE_SEC*time;
		// add current timer to additional timer
		current_jiffies = get_jiffies_64();
		hwwdog_end = current_jiffies + (wdog_timeout * HZ);
		printk("cont HW WDOG %u %u\n", time, wdog_timeout);
	}
	
	if (strncmp(hwwdog_name,"stop", 4) == 0)
	{
		wdog_action = HWWDOG_STOP;
		wdog_time = 0x7fffffff; // default val
	}
	
	if (strncmp(hwwdog_name,"write", 5) == 0)
	{
		unsigned int start = 0;
		/* get hw watchdog time to wait - up to 15 */
		sscanf(hwwdog_name+6, "%u %u", &start, &time);
		if (time > 0xf)
			time = 0xf;
		if (start != 0)
			start = 1;
		//configure H/W directly
		mv_config_hwwatchdog(HWWDOG_ONE_SEC*time, start);
	}

	spin_unlock(&hwwdog_lock);

	return ret;
}

/*
 * Sysfs setup bits:
 */
static SYSDEV_ATTR(hw_watchdog, 0600, sysfs_show_hwwatchdof,
		   sysfs_config_hwwatchdog);

static struct sysdev_class hwwatchdog_sysclass = {
	set_kset_name("hwwatchdog"),
};

static struct sys_device device_hwwatchdog = {
	.id	= 0,
	.cls	= &hwwatchdog_sysclass,
};

static int __init init_hwwatchdog_sysfs(void)
{

	int error = sysdev_class_register(&hwwatchdog_sysclass);

	if (!error)
		error = sysdev_register(&device_hwwatchdog);
	if (!error)
		error = sysdev_create_file(
				&device_hwwatchdog,
				&attr_hw_watchdog);

	return error;
}

device_initcall(init_hwwatchdog_sysfs);
#endif /* CONFIG_SYSFS */

/**
 * ksw_hw_wdog_thread_main - main function for hw watchdog thread
 * @arg:	argument list
 *
 * Acts according to configuration variables to configure HW watchdog HW
  */
static int ksw_hw_wdog_thread_main(void *arg)
{
	u64 current_jiffies;
	
	int exit_hwwdog_thread = 0;
	//This program will exit without stoping HW wdog after wdog_timeout
	while (exit_hwwdog_thread == 0)
	{
		spin_lock(&hwwdog_lock);
	
		switch (wdog_action)
		{
			// no action
			case HWWDOG_NOACT:
				break;
			//start HW WDOG
			case HWWDOG_START:
				mv_config_hwwatchdog(wdog_time, 1);
				break;
			//continue HW WDOG
			case HWWDOG_CONT:
				//not used - using HWWDOG_START
				//mv_config_hwwatchdog(wdog_time, 1);
				break;
			//stop HW WDOG program and HW WDOG
			case HWWDOG_STOP:
				printk("HWWDOG: stop");
				mv_config_hwwatchdog(0x7fffffff, 0); // set to default val
				wdog_action = HWWDOG_NOACT;
				exit_hwwdog_thread = 2;
				break;
			default:
				printk("HW Wdog bad action\n");
				break;
		}
		// timer for this thread elapsed - exit the thread
		current_jiffies = get_jiffies_64();
		if ( current_jiffies >= hwwdog_end )
			exit_hwwdog_thread = 1;

		spin_unlock(&hwwdog_lock);

		ssleep(HWWDOG_SLEEP);
	}
	if (exit_hwwdog_thread == 1)
	//This happens when wdog_timeout reached
	{
		printk("exit HWWDOG without stopping HWWDOG (timeout) - reboot in up to 20 seconds\n");
	}
	return 0;
}

/**
 * ksw_hwwdog_config - configure HW watchdog from kernel
 * @arg:	argument list
 *
 * Acts according to configuration variables to configure HW watchdog HW
  */
int ksw_hwwdog_config (unsigned int action, unsigned int hw_wdog_time, unsigned int sw_wdog_time)
{
	u64 current_jiffies;
	
	spin_lock(&hwwdog_lock);
	switch (action)
	{
		//start HW WDOG
		case HWWDOG_START:
			if (wdog_action == HWWDOG_NOACT)
			{
				if (hw_wdog_time <= 0xf)
					wdog_time = HWWDOG_ONE_SEC*hw_wdog_time;
				else
					wdog_time = 0xffffffff; // max value
				wdog_action = HWWDOG_START;
				wdog_timeout = sw_wdog_time;
				hwwdog_start = get_jiffies_64();
				hwwdog_end = hwwdog_start + (wdog_timeout * HZ);
				printk("HW WDOG: start %x %u\n", wdog_time, wdog_timeout);
				ksw_hwwdog_init();
			}
			else
			{
				printk("HW WDOG: start, HW watchdog already started - stop first\n");
			}
			break;
		//stop HW WDOG
		case HWWDOG_STOP:
			wdog_action = HWWDOG_STOP;
			wdog_time = 0x7fffffff;
			printk ("HW WDOG: stop\n");
			break;
		//cont HW WDOG
		case HWWDOG_CONT:
			if (hw_wdog_time < 0xf)
				wdog_time = HWWDOG_ONE_SEC*hw_wdog_time;
			else
				wdog_time = 0xffffffff; // max value
			wdog_timeout = sw_wdog_time;
			// add additional timer to current timer
			current_jiffies = get_jiffies_64();
			hwwdog_end = current_jiffies + (wdog_timeout * HZ);
			printk("HW WDOG: cont %u %u\n", wdog_time, wdog_timeout);
			break;
		default:
			printk("HW WDOG: wrong action\n");
			break;
	}
	spin_unlock(&hwwdog_lock);
	return 0;
}

/**
 * ksw_hwwdog_read - read HW watchdog params from HW in kernel
 * @arg:	argument list
 *
 * Call HW watchdog register API read to get HW watchdog params - calle dat kernel
  */
void ksw_hwwdog_read (	unsigned int *time,
	int *start
)
{
	spin_lock(&hwwdog_lock);

	mv_read_hwwatchdog (time, start);

	spin_unlock(&hwwdog_lock);
}

/**
 * ksw_hwwdog_init - Initialize HW watchdog thread
 * @arg:	argument list
 *
 * Create a HW watchdog thread that will manage HW watchdog operation
  */
int ksw_hwwdog_init(void)
{
	spin_lock_init(&hwwdog_lock);

	s_worker_thread = kthread_run(ksw_hw_wdog_thread_main, NULL, "hw_wdog");
	if (IS_ERR(s_worker_thread))
	{
		s_worker_thread = NULL;
		printk ("HW WDOG: thread fail start\n");
		return -1;
	}

	printk ("HW WDOG: thread started\n");
	return 0;
}

void ksw_stop_hwwdog(void)
{
	wdog_action = HWWDOG_NOACT;
	mv_config_hwwatchdog(0x7fffffff, 0);
}

EXPORT_SYMBOL(ksw_stop_hwwdog);

