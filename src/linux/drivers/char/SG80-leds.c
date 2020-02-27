
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/netdevice.h>
#include <linux/pci.h>
#include <linux/sofaware_led.h>
#include "cp_ledctl.h"
static unsigned short gpio_power_green;
static unsigned short gpio_power_red;
static unsigned short gpio_alert_green;
static unsigned short gpio_alert_red;
static unsigned short gpio_usb1_green;
static unsigned short gpio_usb1_red;
static unsigned short gpio_usb2_green;
static unsigned short gpio_usb2_red;
static unsigned short gpio_internet_red;
static unsigned short gpio_internet_green;
static unsigned short gpio_wifi_green;
static unsigned short gpio_adsl_act_green;
static unsigned short gpio_adsl_link_green;

#define GPP_GROUP(gpp)  gpp/32
#define GPP_ID(gpp)     gpp%32
#define GPP_BIT(gpp)    0x1 << GPP_ID(gpp)

#define IC_SG80_EXTEND1 0x21
#define IC_SG80_EXTEND2 0x20

char boardType[5];
/* mvGppValueSet - Set a GPP Pin list value. */
extern MV_STATUS mvGppValueSet (MV_U32 group, MV_U32 mask, MV_U32 value);

void led_gpio_write(int gpio, unsigned char color)
{
	if (gpio == 0)
		return; //LED is not supported on current HW version
	if (gpio & SEATTLE_EXTENDER1) { // extra bit indicates seattle board I2C
		if (color == 0)	{

			extend_gpio_ctl(IC_SG80_EXTEND1, gpio-SEATTLE_EXTENDER1,1);
		}
		else {
			extend_gpio_ctl(IC_SG80_EXTEND1, gpio-SEATTLE_EXTENDER1,0);
		}
	}
	else if (gpio & SEATTLE_EXTENDER2) {
		if (color == 0)	{
			extend_gpio_ctl(IC_SG80_EXTEND2, gpio-SEATTLE_EXTENDER2,1);
		}
		else {
			extend_gpio_ctl(IC_SG80_EXTEND2, gpio-SEATTLE_EXTENDER2,0);
		}
	}
	else {//CPU
		mvGppBlinkEn(GPP_GROUP(gpio), GPP_BIT(gpio), 0);
		mvGppValueSet(GPP_GROUP(gpio), GPP_BIT(gpio), (color)?0:(GPP_BIT(gpio)));

	}
}
#define MPP0      *((unsigned int *) 0xf1010000)
void init_leds_seattle(void)
{

	MPP0 &= (0xffffffff & (~(0xf << (4*(7 % 8)))));
	gpio_power_green = SEATTLE_GPIO_POWER_GREEN;
	gpio_power_red = SEATTLE_GPIO_POWER_RED;
	gpio_alert_green = SEATTLE_GPIO_ALERT_GREEN;
	gpio_alert_red = SEATTLE_GPIO_ALERT_RED;
	gpio_usb1_green = SEATTLE_GPIO_USB1_GREEN;
	gpio_usb2_green = SEATTLE_GPIO_USB2_GREEN;
	gpio_usb1_red = SEATTLE_GPIO_USB1_RED;
	gpio_usb2_red = SEATTLE_GPIO_USB2_RED;
	gpio_internet_red = SEATTLE_GPIO_INTERNET_RED;
	gpio_internet_green = SEATTLE_GPIO_INTERNET_GREEN;
	gpio_wifi_green = SEATTLE_GPIO_WIFI_GREEN;
	
}

void init_leds_london(void) {
	gpio_power_green = LONDON_GPIO_POWER_GREEN;
	gpio_power_red = LONDON_GPIO_POWER_RED;
	gpio_alert_green = LONDON_GPIO_ALERT_GREEN;
	gpio_alert_red = LONDON_GPIO_ALERT_RED;
	gpio_usb1_green = LONDON_GPIO_USB1_GREEN;
	gpio_usb1_red = LONDON_GPIO_USB1_RED;
	gpio_usb2_green = LONDON_GPIO_USB2_GREEN;
	gpio_usb2_red = LONDON_GPIO_USB2_RED;
	gpio_wifi_green = UNSUPPORTED_LED;
	gpio_adsl_act_green = UNSUPPORTED_LED;
	gpio_adsl_link_green = UNSUPPORTED_LED;
	gpio_internet_red = UNSUPPORTED_LED;
	gpio_internet_green = UNSUPPORTED_LED;
}


void __alert_led_ctl(int color)
{
//	printk("__alert_led_ctl\n");
	switch (color) {
		case 0: // off
			led_gpio_write(gpio_alert_green,0);
			led_gpio_write(gpio_alert_red,0);
			break;
		case 1: // green
			led_gpio_write(gpio_alert_red,0);
			led_gpio_write(gpio_alert_green,1);
			break;
		case 2: // red
			led_gpio_write(gpio_alert_green,0);
			led_gpio_write(gpio_alert_red,1);
			break;
		case 3: // orange
			led_gpio_write(gpio_alert_green,1);
			led_gpio_write(gpio_alert_green,1);
			break;
		default:
			break;
	}
}


void __power_led_ctl(int color)
{
//	printk("__power_led_ctl\n");
	switch (color) {
		case 0: /* off */
			led_gpio_write(gpio_power_red,0);
			led_gpio_write(gpio_power_green,0);
			break;
		case 1: /* green */
			led_gpio_write(gpio_power_red,0);
			led_gpio_write(gpio_power_green,1);
			break;
		case 2: /* red */
			led_gpio_write(gpio_power_green,0);
			led_gpio_write(gpio_power_red,1);
			break;
		case 3: /* orange */
			led_gpio_write(gpio_power_green,1);
			led_gpio_write(gpio_power_red,1);
			break;
		default:
			break;
	}
}

void __usb2_led_ctl(int color)
{

	switch (color) {
		case 0: /* off */
			led_gpio_write(gpio_usb2_green,0);
			led_gpio_write(gpio_usb2_red,0);
			break;
		case 1: /* green */
			led_gpio_write(gpio_usb2_red,0);
			led_gpio_write(gpio_usb2_green,1);
			break;
		case 2: /* red */
			led_gpio_write(gpio_usb2_green,0);
			led_gpio_write(gpio_usb2_red,1);
			break;
		case 3: /* orange */
			led_gpio_write(gpio_usb2_green,1);
			led_gpio_write(gpio_usb2_red,1);
			break;
		default:
			break;

	}
}
void __usb1_led_ctl(int color)
{

	switch (color) {
		case 0: /* off */
			led_gpio_write(gpio_usb1_green,0);
			led_gpio_write(gpio_usb1_red,0);
			break;
		case 1: /* green */
			led_gpio_write(gpio_usb1_red,0);
			led_gpio_write(gpio_usb1_green,1);
			break;
		case 2: /* red */
			led_gpio_write(gpio_usb1_green,0);
			led_gpio_write(gpio_usb1_red,1);
			break;
		case 3: /* orange */
			led_gpio_write(gpio_usb1_green,1);
			led_gpio_write(gpio_usb1_red,1);
			break;
		default:
			break;
	}
}

void __internet_led_ctl(int color)
{


	switch (color) {
		case 0: /* off */
			led_gpio_write(gpio_internet_green,0);
			led_gpio_write(gpio_internet_red,0);
			break;
		case 1: /* green */
			led_gpio_write(gpio_internet_red,0);
			led_gpio_write(gpio_internet_green,1);
			break;
		case 2: /* red */
			led_gpio_write(gpio_internet_green,0);
			led_gpio_write(gpio_internet_red,1);
			break;
		case 3: /* orange */
			led_gpio_write(gpio_internet_green,1);
			led_gpio_write(gpio_internet_red,1);
			break;
		default:
			break;
	}
}
void __wifi_led_ctl(int color)
{
//	printk("__wifi_led_ctl\n");
	led_gpio_write(gpio_wifi_green,color);
}
