/* 
 *  SCSI Spinup specific structures and functions.
 *
 *  Copyright (c) 2009 Marvell,  All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/delay.h>

#include <scsi/scsi.h>
#include <scsi/scsi_cmnd.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_host.h>
#include <scsi/scsi_spinup.h>

#include "scsi_priv.h"
#include "scsi_logging.h"

/*structure: spinup_config=<spinup_max>,<spinup_timout> example: spinup_config=2,6 (two max spinup disks, 6 seconds timeout) */
static char *cmdline = NULL;

/* Required to get the configuration string from the Kernel Command Line */
int spinup_cmdline_config(char *s);
__setup("spinup_config=", spinup_cmdline_config);

int spinup_cmdline_config(char *s)
{
    cmdline = s;
    return 1;
}

static int spinup_timeout = 0;
static int spinup_max = 0;
static int spinup_enabled = 0;

/* maximum current disks spinning-up semaphore */
static struct semaphore scsi_spinup_nr_sem;

int scsi_parse_spinup_max(char *p_config)
{
	/* the max spinup value is constructed as follows: */
	/* <spinup_max>,<spinup_timout>                    */
	unsigned int _spinup_max;
	int syntax_err = 0;

	_spinup_max = 0;
	if (p_config == NULL)
	    return 0;
	while((*p_config >= '0') && (*p_config <= '9'))	{
		_spinup_max = (_spinup_max * 10) + (*p_config - '0');
		p_config++;
	}
	if ((*p_config != ',') || (_spinup_max == 0)) {
		printk("Syntax error while parsing spinup_max value from command line\n");
		syntax_err = 1;
	}
	if ((_spinup_max < 0)||(_spinup_max > 8)) {
		printk("Syntax error while parsing spinup_max value from command line. (value must between 1 to 8\n");
		syntax_err = 1;
	}
	if(syntax_err == 0) {
		printk("  o Maximum Disk Spinup set to: %d.\n", _spinup_max);
		return _spinup_max;
	}
	return 0;
}

int scsi_parse_spinup_timeout(char *p_config)
{
	/* the  spinup timeout value is constructed as follows: */
	/* <spinup_max>,<spinup_timout>                         */
	unsigned int _spinup_timout;
	int syntax_err = 0;

	_spinup_timout = 0;
	if (p_config == NULL)
	    return 0;
	while((*p_config != ',') && (*p_config != '\0'))
		p_config++;
		
	p_config++;
	while((*p_config >= '0') && (*p_config <= '9'))	{
		_spinup_timout = (_spinup_timout * 10) + (*p_config - '0');
		p_config++;
	}
	if ((*p_config != '\0') || (_spinup_timout == 0)) {
		printk("Syntax error while parsing spinup_timout value from command line\n");
		syntax_err = 1;
	}
	if ((_spinup_timout < 0) || (_spinup_timout > 6)) {
		printk("Syntax error while parsing spinup_timout value from command line (spinup_timeout must be between 1 to 6) \n");
		syntax_err = 1;
	}
	if(syntax_err == 0) {
		printk("  o Disk Spinup timeout set to: %d.\n", _spinup_timout);
		return _spinup_timout;
	}
	return 0;
}

/* function convertd timeout value got from the user to jiffies */
int timeout_to_jiffies (int timeout)
{
	unsigned int secs=0;

	switch(timeout) {
		case 0:		//printf("off");
			break;
		case 252:	//printf("21 minutes");
			secs = 21 * 60;
			break;
		case 253:	//printf("vendor-specific");
			break;
		case 254:	//printf("?reserved");
			break;
		case 255:	//printf("21 minutes + 15 seconds");
			secs = 21 * 60 + 15;
			break;
		default:
			if (timeout <= 240) {
				secs = timeout * 5;
			} else if (timeout <= 251) {
				secs = ((timeout - 240) * 30) * 60;
			} else
				printk("illegal value");
			break;
	}
	return msecs_to_jiffies ((secs-1) * 1000);
}


void standby_add_timer(struct scsi_device *sdev, int timeout,
		    void (*complete)(struct scsi_device *))
{
	/*
	 * If the clock was already running for this device, then
	 * first delete the timer.  The timer handling code gets rather
	 * confused if we don't do this.
	 */
	if (sdev->standby_timeout.function)
		del_timer(&sdev->standby_timeout);

	sdev->standby_timeout.data = (unsigned long)sdev;
	sdev->standby_timeout_secs = timeout;

	sdev->standby_timeout.expires = timeout + jiffies ;
	sdev->standby_timeout.function = (void (*)(unsigned long)) complete;

	SCSI_LOG_ERROR_RECOVERY(5, printk("%s: scmd: %p, time:"
					  " %d, (%p)\n", __FUNCTION__,
					  sdev, timeout, complete));

	add_timer(&sdev->standby_timeout);
}


int standby_delete_timer(struct scsi_device *sdev)
{
	int rtn;

	rtn = del_timer(&sdev->standby_timeout);

	SCSI_LOG_ERROR_RECOVERY(5, printk("%s: scmd: %p,"
					 " rtn: %d\n", __FUNCTION__,
					 sdev, rtn));

	sdev->standby_timeout.data = (unsigned long)NULL;
	sdev->standby_timeout.function = NULL;

	return rtn;
}


void standby_times_out(struct scsi_device *sdev)
{
	unsigned long flags = 0;

	spin_unlock_irqrestore(sdev->host->host_lock, flags);
	sdev->sdev_power_state = SDEV_PW_STANDBY_TIMEOUT_PASSED;
	spin_lock_irqsave(sdev->host->host_lock, flags);
	standby_delete_timer(sdev);
}

void spinup_add_timer(struct scsi_device *sdev, int timeout,
		    void (*complete)(struct scsi_device *))
{
	/*
	 * If the clock was already running for this device, then
	 * first delete the timer.  The timer handling code gets rather
	 * confused if we don't do this.
	 */
	if (sdev->spinup_timeout.function)
		del_timer(&sdev->spinup_timeout);
		
	
	sdev->spinup_timeout.data = (unsigned long)sdev;
	
	sdev->spinup_timeout.expires = jiffies + msecs_to_jiffies (timeout * 1000);
	sdev->spinup_timeout.function = (void (*)(unsigned long)) complete;

	SCSI_LOG_ERROR_RECOVERY(5, printk("%s: scmd: %p, time:"
					  " %d, (%p)\n", __FUNCTION__,
					  sdev, timeout, complete));
	add_timer(&sdev->spinup_timeout);
	
}

int spinup_delete_timer(struct scsi_device *sdev)
{
	int rtn;

	rtn = del_timer(&sdev->spinup_timeout);

	SCSI_LOG_ERROR_RECOVERY(5, printk("%s: scmd: %p,"
					 " rtn: %d\n", __FUNCTION__,
					 sdev, rtn));

	sdev->spinup_timeout.data = (unsigned long)NULL;
	sdev->spinup_timeout.function = NULL;

	return rtn;
}

void spinup_times_out(struct scsi_device *sdev)
{
	scsi_spinup_sem_up();
	spinup_delete_timer(sdev);
}

void scsi_spinup_sem_up(void)
{
	up(&scsi_spinup_nr_sem);
}

void scsi_spinup_sem_down(void)
{
	down(&scsi_spinup_nr_sem);
}

int is_scsi_spinup_enabled(void)
{
	return spinup_enabled;
}

int scsi_spinup_get_timeout()
{
	return spinup_timeout;
}

/* __setup kernel line parsing and setting up the spinup feature */
int __init scsi_spinup_init(void)
{
	printk("SCSI Scattered Spinup:\n");
	spinup_max = scsi_parse_spinup_max(cmdline);
	spinup_timeout = scsi_parse_spinup_timeout(cmdline);
	if ((spinup_max == 0) || (spinup_timeout == 0)){
		spinup_enabled = 0;
		printk("SCSI Scattered Spinup Feature Status: Disabled\n");
	}else{
		spinup_enabled = 1;
		sema_init(&scsi_spinup_nr_sem, spinup_max);
		printk("SCSI Scattered Spinup Feature Status: Enabled\n");
	}
	return 0;
}
int scsi_do_spinup(struct scsi_cmnd *cmd, unsigned long *pflags)
{
	switch (cmd->device->sdev_power_state) {
		case SDEV_PW_STANDBY:
		case SDEV_PW_STANDBY_TIMEOUT_PASSED:
			cmd->device->sdev_power_state = SDEV_PW_SPINNING_UP;
			spin_unlock_irqrestore(cmd->device->host->host_lock, *pflags);

			/* disk will wait here to his turn to spinup */
			if (in_atomic())
				while (down_trylock(&scsi_spinup_nr_sem))
					mdelay(MAX_UDELAY_MS);
			else
				scsi_spinup_sem_down();

			spin_lock_irqsave(cmd->device->host->host_lock, *pflags);
			/* starting timer for the spinup process */
			spinup_add_timer(cmd->device, scsi_spinup_get_timeout(), spinup_times_out);
		break;
		case SDEV_PW_STANDBY_TIMEOUT_WAIT:
			standby_add_timer(cmd->device, cmd->device->standby_timeout_secs, standby_times_out);
		break;
		default:
		break;
	}
	return 0;
}
