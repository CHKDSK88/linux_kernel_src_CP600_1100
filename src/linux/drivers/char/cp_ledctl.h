#ifndef _LEDCTL_H_
#define _LEDCTL_H_

/* defines */
#define L30_FLAVOR 0
#define L50_FLAVOR 1
#define SEATTLE_FLAVOR 2
#define NUM_FLAVORS 3

enum cp_seattle_gpio_extenders {
		GPIO_CPU = 0,
		GPIO_I2C = 0x100,
		GPIO_EXTENDER = 0x200

};

/* Power, USB and Alert GPIO defines */ 

#define LED_BASE_NUM	37
/* TODO: change led mapping to be continues */
/* LED numbers */
#define LEDMOD_USB2_GREEN	37
#define LEDMOD_USB2_RED    	41
#define LEDMOD_USB1_GREEN	38
#define LEDMOD_USB1_RED		40
#define LEDMOD_POWER_GREEN	49
#define LEDMOD_POWER_RED	46 
#define LEDMOD_ALERT_GREEN 	44
#define LEDMOD_ALERT_RED   	48 
#define LED_LAST_NUM 		49
#define GPIO_ADSL			100
#define GPIO_WIFI			101

#define LED_MODE_POWER 0
#define LED_MODE_BLINK 1
#define NUM_LED_MODES 2

/* LED commands */
#define LEDMOD_CMD_OFF		0
#define LEDMOD_CMD_ON		1
#define LEDMOD_CMD_BLINK	2
#define LEDMOD_CMD_STATIC	3
#define LEDMOD_CMD_LAST		3
#define LEDMOD_CMD_NUM		4

extern unsigned char led_cmds[LEDMOD_CMD_NUM];

#define LED_CMD_MODE(CMD) ((led_cmds[(CMD)])>>1)
#define LED_CMD_IS_ON(CMD) ((led_cmds[(CMD)])&1)

typedef int MV_STATUS;
typedef unsigned int MV_U32;
typedef unsigned short MV_U16;
typedef int MV_32;

int cp_led_do(unsigned char led_id, unsigned char state);

int cp_led_set_flavor(unsigned int board_flavor);

void cp_led_stop();

#endif
