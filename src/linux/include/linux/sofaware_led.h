/**************************************************
* sofaware_led.h
***************************************************
*/

#ifndef __SOFAWARE_LED_H__
#define __SOFAWARE_LED_H__

#ifdef __KERNEL__
#define SW_LED_MISC_MINOR 0
#endif 
#define	SEATTLE_EXTENDER1  0x100
#define	SEATTLE_EXTENDER2  0x200
// Seattle GPIO led assignment
#define SEATTLE_GPIO_POWER_GREEN (49)
#define SEATTLE_GPIO_POWER_RED (7)
#define SEATTLE_GPIO_ALERT_GREEN (38)
#define SEATTLE_GPIO_ALERT_RED (2|SEATTLE_EXTENDER1)
#define SEATTLE_GPIO_ADSL_ACT 0
#define SEATTLE_GPIO_ADSL_LINK 0
#define SEATTLE_GPIO_USB2_GREEN (2|SEATTLE_EXTENDER2)
#define SEATTLE_GPIO_USB1_GREEN (0|SEATTLE_EXTENDER2)
#define SEATTLE_GPIO_USB1_RED (4|SEATTLE_EXTENDER2)
#define SEATTLE_GPIO_USB2_RED (5|SEATTLE_EXTENDER2)
#define SEATTLE_GPIO_WIFI_GREEN (7|SEATTLE_EXTENDER2)
#define SEATTLE_GPIO_INTERNET_RED (1|SEATTLE_EXTENDER2)
#define SEATTLE_GPIO_INTERNET_GREEN (3|SEATTLE_EXTENDER2)

#define UNSUPPORTED_LED 0

// Seattle GPIO led assignment
#define LONDON_GPIO_POWER_GREEN 49
#define LONDON_GPIO_POWER_RED 46
#define LONDON_GPIO_ALERT_GREEN 44
#define LONDON_GPIO_ALERT_RED 48
#define LONDON_GPIO_USB1_GREEN 38
#define LONDON_GPIO_USB1_RED 40
#define LONDON_GPIO_USB2_GREEN 37
#define LONDON_GPIO_USB2_RED 41
typedef enum {
	USB1_LED	= 0,
	USB2_LED 	= 1,
	POWER_LED	= 2,
	ALERT_LED	= 3,
	INTERNET_LED= 4,
	WIFI1_LED	= 5,
	WIFI2_LED	= 6,
	ADSL_ACT	= 7,
	ADSL_LINK	= 8,
} SW_LED;

typedef enum {
	LED_BIT_NONE	= 0,
	LED_BIT_NO_MGMT	= 1,
	LED_BIT_WAN_ERR	= 2,
	LED_BIT_CONN_TAB= 4,
	LED_BIT_MEM_LOW	= 8,
	LED_BIT_CPU_HIGH= 16,
} LED_ALERT_REASON;

typedef enum {
	LED_NONE	= 0,
	LED_GREEN	= 1,
	LED_RED	= 2,
	LED_SLOW_BLINKING_GREEN= 3,
	LED_SLOW_BLINKING_RED	= 4,
	LED_FAST_BLINKING_GREEN= 5,
	LED_FAST_BLINKING_RED	= 6,
} LED_ALERT_PRIORITY;
// must be aligned with eSWLedState in \Products\Modem\SWLed.h
typedef enum {
	SW_LED_OFF					= 0,
	SW_LED_RED					= 16,
	SW_LED_FAST_BLINKING_RED	= 26,
	SW_LED_ATTACK				= 27,
	SW_LED_VIRUS				= 28,
	SW_LED_SLOW_BLINKING_RED	= 29,
	SW_LED_BLINK_ONCE			= 30,
	SW_LED_BLINK_ONCE_LONG		= 31,
	SW_LED_GREEN				= 32,
	SW_LED_BLINK_ONCE_SHORT		= 33,
	SW_LED_FAST_BLINKING_GREEN	= 42,
	SW_LED_SLOW_BLINKING_GREEN	= 45,
	SW_LED_ORANGE 				= 50,
	SW_LED_FAST_BLINKING_ORANGE = 51,
	SW_LED_SLOW_BLINKING_ORANGE = 52,
	SW_LED_UNDEFINED			= 100
} SW_LED_STAT;

#define IOC_SW_SET_LED   0x1A 

typedef struct ioc_set_led {
	SW_LED led;
	SW_LED_STAT led_stat;
} ioc_set_led;


#ifdef __KERNEL__

extern int set_led_state(SW_LED, SW_LED_STAT);

extern void sofaware_serial_led_act(void);
extern void sofaware_serial_led_onoff(int on);

extern void sofaware_cf_led_act(void);
extern void sofaware_cf_led_onoff(int on);

extern void sofaware_wlan_led_act(void);
extern void sofaware_wlan_led_onoff(int on);

extern void sofaware_usb_led_act(void);
extern void sofaware_usb_led_onoff(int on);

extern void __system_led_ctl(int color);
extern void __security_led_ctl(int color);
extern void __arrow_security_led_ctl(int color);
extern void __usb_led_ctl(int color);
extern void __uart_led_ctl(int color);
extern void __vpn_led_ctl(int color);
extern void __cf_led_ctl(int color);
extern void __wlan_led_ctl(int color);
extern void __excard_led_ctl(int color);
extern void __dsl_act_led_ctl(int color);

extern void init_leds_arrow(void);
extern void init_leds_sbox4_gigabyte(void);
extern void init_leds_sbox4_cameo(void);


#endif

#endif /* #ifndef __SOFAWARE_LED_H__ */

