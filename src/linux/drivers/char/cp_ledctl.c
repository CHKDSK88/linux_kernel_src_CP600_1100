#include "cp_ledctl.h"
#include <linux/kernel.h>
#include <linux/timer.h>
#include <linux/jiffies.h>

/* London board uses Marvell gpios */
#define MV_GPP_OUT  	    0    		    /* GPP output  */
#define MV_GPP_IN_INVERT    0xFFFFFFFF              /* Inverted value is got*/
#define MV_GPP_IN_ORIGIN    0                       /* original value is got*/
#define GPP_GROUP(gpp)  gpp/32
#define GPP_ID(gpp)     gpp%32
#define GPP_BIT(gpp)    0x1 << GPP_ID(gpp)

/* externs */

/* mvGppTypeSet - Set PP pin mode (IN or OUT) */
extern MV_STATUS mvGppTypeSet(MV_U32 group, MV_U32 mask, MV_U32 value);

/* mvGppPolaritySet - Set a GPP (IN) Pin list Polarity mode. */
extern MV_STATUS mvGppPolaritySet(MV_U32 group, MV_U32 mask, MV_U32 value);

/* mvGppPolarityGet - Get the Polarity of a GPP Pin */
extern MV_U32  mvGppPolarityGet(MV_U32 group, MV_U32 mask);

/* mvGppValueGet - Get a GPP Pin list value.*/
extern MV_U32 mvGppValueGet(MV_U32 group, MV_U32 mask);

/* mvGppValueSet - Set a GPP Pin list value. */
extern MV_STATUS mvGppValueSet (MV_U32 group, MV_U32 mask, MV_U32 value);

extern MV_STATUS mvGppBlinkEn(MV_U32 group, MV_U32 mask, MV_U32 value);


/*Least significant bit - whether it is on or off
 * Other bits - the mode (blink or ON/OFF)
 * e.g. "10" is "1" for mode blink, and "0" for disabled
 */
unsigned char led_cmds[LEDMOD_CMD_NUM] = {
	(LED_MODE_POWER<<1)|0, //OFF
	(LED_MODE_POWER<<1)|1, //ON
	(LED_MODE_BLINK<<1)|1, //STATIC
	(LED_MODE_BLINK<<1)|0  //BLINK
};

typedef void(*led_state_func_t)(unsigned int, unsigned char); //Function that changes state (on/off and blink)
typedef int(*led_init_func_t)(void); //Prepares the leds
typedef void(*gpio_enable_func_t)(int, int); //Turns the specific LED ON or OFF (no blink)
#define NOT_SUPPORTED 0

typedef struct _led_data_t{
	unsigned char led_id; 			// led_id as known to caller
	unsigned char hw_led_number; 	// matching LED number for use by hardware specific function
	void* extra_params;				// helper functions and any extra data needed
	led_state_func_t cp_set_led_state; // Function that actually changes state
} led_data_t;

#define NO_EXTRA_PARAMS 0

/* 0 is ON; 1 is OFF; wrapper may be needed */
extern void sensor_gpio_ctl(int num, int status);
extern void extend_gpio_ctl(int num, int status);

static void mv_gpio_led_wrapper(unsigned int num, unsigned char state);

static void cp_set_led_state_seattle(unsigned int num, unsigned char state);
static void cp_set_gpio(unsigned int num, unsigned char state);


/* Seattle LEDs are connected to extenders on I2C bus */
static unsigned char cp_known_gpio[] = {
	LEDMOD_USB2_GREEN,
	LEDMOD_USB2_RED,
	LEDMOD_USB1_GREEN,
	LEDMOD_USB1_RED,
	LEDMOD_POWER_GREEN,
	LEDMOD_POWER_RED,
	LEDMOD_ALERT_GREEN,
	LEDMOD_ALERT_RED
};

#define CP_KNOWN_LEDS_SIZE 8
#define CP_KNOWN_GPIO_SIZE ((sizeof(cp_known_gpio))/(sizeof(unsigned char)))

static led_data_t cp_london_leds[CP_KNOWN_GPIO_SIZE] = {
	{LEDMOD_USB2_GREEN, LEDMOD_USB2_GREEN, NO_EXTRA_PARAMS, &mv_gpio_led_wrapper },
	{LEDMOD_USB2_RED, LEDMOD_USB2_RED, NO_EXTRA_PARAMS, &mv_gpio_led_wrapper },
	{LEDMOD_USB1_GREEN, LEDMOD_USB1_GREEN, NO_EXTRA_PARAMS, &mv_gpio_led_wrapper },
	{LEDMOD_USB1_RED, LEDMOD_USB1_RED, NO_EXTRA_PARAMS, &mv_gpio_led_wrapper },
	{LEDMOD_POWER_GREEN, LEDMOD_POWER_GREEN, NO_EXTRA_PARAMS, &mv_gpio_led_wrapper },
	{LEDMOD_POWER_RED, LEDMOD_POWER_RED, NO_EXTRA_PARAMS, &mv_gpio_led_wrapper },
	{LEDMOD_ALERT_GREEN, LEDMOD_ALERT_GREEN, NO_EXTRA_PARAMS, &mv_gpio_led_wrapper },
	{LEDMOD_ALERT_RED, LEDMOD_ALERT_RED, NO_EXTRA_PARAMS, &mv_gpio_led_wrapper },
	{GPIO_ADSL, GPIO_ADSL, NO_EXTRA_PARAMS,NOT_SUPPORTED },
	{GPIO_WIFI, GPIO_WIFI, NO_EXTRA_PARAMS, NOT_SUPPORTED }
};

/* Seattle LEDs are connected to extenders on I2C bus */
static led_data_t cp_seattle_leds[CP_KNOWN_GPIO_SIZE] = {
	{LEDMOD_USB2_GREEN, 8, (void*)(&sensor_gpio_ctl), &cp_set_led_state_seattle },
	{LEDMOD_USB2_RED, 8, NO_EXTRA_PARAMS, NOT_SUPPORTED },
	{LEDMOD_USB1_GREEN, 13, (void*)(&sensor_gpio_ctl), &cp_set_led_state_seattle },
	{LEDMOD_USB1_RED, 13, NO_EXTRA_PARAMS, NOT_SUPPORTED },
	{LEDMOD_POWER_GREEN, 4, (void*)(&extend_gpio_ctl), &cp_set_led_state_seattle },
	{LEDMOD_POWER_RED, 2, (void*)(&extend_gpio_ctl), &cp_set_led_state_seattle },
	{LEDMOD_ALERT_GREEN, 1, (void*)(&extend_gpio_ctl), &cp_set_led_state_seattle },
	{LEDMOD_ALERT_RED, 3, (void*)(&extend_gpio_ctl), &cp_set_led_state_seattle },
	{GPIO_ADSL, 12 | GPIO_I2C, NO_EXTRA_PARAMS, &sensor_gpio_ctl },
	{GPIO_WIFI, 6 | GPIO_I2C, NO_EXTRA_PARAMS, &sensor_gpio_ctl }
};

static led_data_t* led_ctl_per_flavor[NUM_FLAVORS] = {
	cp_london_leds, /* L30 */
	cp_london_leds, /* L50 */
	cp_seattle_leds /* Seattle */
};

static unsigned int blink_state;

static led_data_t* my_board_leds = 0;

//20 times per second
#define BLINK_DELAY   (HZ/20)

struct timer_list led_timer;
static int led_timer_needs_stop = 0;

static void led_timer_func(unsigned long opq)
{
	//static unsigned int debug_visits=0;
	unsigned int num;
	int hw_num;
	static int is_off = 0;

	is_off = !is_off; // Toggle

	if (blink_state) {
		for (num = 0; num < CP_KNOWN_LEDS_SIZE; ++num) {
			if ( blink_state&(1<<num) ) {
				hw_num = my_board_leds[num].hw_led_number;
				((gpio_enable_func_t)(my_board_leds[num].extra_params))(hw_num, is_off);
			}
		}
	}
	led_timer.expires = jiffies + BLINK_DELAY;
	add_timer(&led_timer);
}

void start_led_blink_timer(){
	init_timer(&led_timer);
	led_timer.function = led_timer_func;
	led_timer.data = 0;
	led_timer.expires = jiffies + BLINK_DELAY;
	add_timer(&led_timer);
	led_timer_needs_stop = 1;
}



/* If LEDs are connected to Marvell MPP use hardware for blinking */
static void mv_gpio_led_wrapper(unsigned int num, unsigned char state) {
	int hw_num;
	if (num<CP_KNOWN_LEDS_SIZE) {
		hw_num = my_board_leds[num].hw_led_number;
		mvGppBlinkEn(GPP_GROUP(hw_num), GPP_BIT(hw_num), (state&(1<<LED_MODE_BLINK))?(GPP_BIT(hw_num)):0);
		mvGppValueSet(GPP_GROUP(hw_num), GPP_BIT(hw_num), (state&(1<<LED_MODE_POWER))?0:(GPP_BIT(hw_num)));
	}
}

/* Handles state change for flavors that do not support hardware blinking */
static void cp_set_led_state_seattle(unsigned int num, unsigned char state) {
	int hw_num;
	
	if ( (num<CP_KNOWN_LEDS_SIZE) && (my_board_leds[num].extra_params) ) {
		hw_num = my_board_leds[num].hw_led_number;
		if ( !(state&(1<<LED_MODE_BLINK)) ) {
			blink_state &= (~(1<<num));
			((gpio_enable_func_t)(my_board_leds[num].extra_params))(hw_num, !(state&(1<<LED_MODE_POWER)));
		} else {
			//Starts enabled
			blink_state |= (1<<num);
			((gpio_enable_func_t)(my_board_leds[num].extra_params))(hw_num, 0);
		}
	}
}


static int gpp_init_led(int pin)
{
    int rv;  
    // set output type 
    rv=mvGppTypeSet (GPP_GROUP(pin), GPP_BIT(pin),(MV_GPP_OUT & GPP_BIT(pin)));
    if (rv)
       printk (KERN_INFO "%s:%d: GPIO initialization failed \n",
			       __FUNCTION__,__LINE__);

    // set low polarity 
    rv=mvGppPolaritySet (GPP_GROUP(pin), GPP_BIT(pin),
                             (MV_GPP_IN_INVERT & GPP_BIT(pin)));
    if (rv)
      printk (KERN_INFO "%s:%d: GPIO initialization failed \n",
      				__FUNCTION__,__LINE__);

    return rv;
}


/*****************************************************************************
 *
 * configure_cpu_registers : This routine configures LED GPIO registers  
 *
 * 
 * RETURNS: OK/Error
 */
static int configure_cpu_registers(void) 
{

	int rv=-1; 
	int ret=0;

	if ( (rv=gpp_init_led(LEDMOD_POWER_GREEN) ) < 0)
	{
		printk (KERN_ALERT "Failed to init LEDMOD_POWER_GREEN (%d)\n", 
						LEDMOD_POWER_GREEN);
		ret--;
	}
	
	if ( (rv = gpp_init_led(LEDMOD_POWER_RED) ) < 0)
	{
		printk (KERN_ALERT "%s: Failed to init LEDMOD_POWER_RED (%d)\n", 
						__FUNCTION__, LEDMOD_POWER_RED);
		ret--;
	}

	if ( (rv = gpp_init_led(LEDMOD_ALERT_GREEN) ) < 0)
	{
		printk (KERN_ALERT "%s: Failed to init LEDMOD_ALERT_GREEN (%d)\n", 
						__FUNCTION__, LEDMOD_ALERT_GREEN);
		ret--;
	}

	if ( (rv = gpp_init_led(LEDMOD_ALERT_RED) ) < 0)
	{
		printk (KERN_ALERT "%s: Failed to init LEDMOD_ALERT_RED (%d)\n", 
						__FUNCTION__, LEDMOD_ALERT_RED);
		ret--;
	}

	printk (KERN_DEBUG "%s: Finished initialization: %d\n", __FUNCTION__, ret);

	return ret;

}


static int led_init_seattle() {
	start_led_blink_timer();
	return 0;
}

static led_init_func_t led_initializers_per_flavor[NUM_FLAVORS] = {
	&configure_cpu_registers, /* L30 */
	&configure_cpu_registers, /* L50 */
	&led_init_seattle /* Seattle */
};


/*
 * Initializes the pointers to hardware dependant functions 
 * In the future HW specific init code may be needed.
 */
int cp_led_set_flavor(unsigned int board_flavor) {
	if (board_flavor<NUM_FLAVORS) {
		my_board_leds = led_ctl_per_flavor[board_flavor];
		return led_initializers_per_flavor[board_flavor]();
	}
	else
	{
		return -1;
	}
}

/* This function translates LED ID to index in table
 * and calls the hardware-dependent function
 */
int cp_led_do(unsigned char led_id, unsigned char state) {
	unsigned char i;

	if (!my_board_leds) {
		printk(KERN_ERR "%s: LED callbacks not initialized!\n", __FUNCTION__);
		return -1;
	}
	for (i=0; i< CP_KNOWN_LEDS_SIZE; ++i) {
		if (led_id==cp_seattle_leds[i].led_id) {
			break;
		}
	}
	
	if (i>=CP_KNOWN_LEDS_SIZE) {
		return -1;
		printk(KERN_WARNING  "%s: Invalid LED number: %u\n", __FUNCTION__, (unsigned int)led_id);
	}
	if ( my_board_leds[i].cp_set_led_state) {
		my_board_leds[i].cp_set_led_state(i, state);
	}  else {
		printk(KERN_DEBUG "%s: LED %u is not supported on this hardware.\n", __FUNCTION__, (unsigned int)led_id);
		return -1;
	} 
		
	return 0;
}
static void cp_set_gpio(unsigned int num, unsigned char state)
{
	//call the function depend on cpu/extender/i2cs
	//if (num &)
}

static gpio_ctl(int pin, int status)
{
	//get hw flavour
	if (pin & GPIO_CPU)
		mvGppValueSet(GPP_GROUP(pin), GPP_BIT(pin),status);
	else if (pin & GPIO_I2C)
	{
		pin = pin % GPIO_I2C;
		sensor_gpio_ctl(pin, status);
	}
	else if (pin & GPIO_EXTENDER)
	{
		pin = pin % GPIO_EXTENDER;
		extend_gpio_ctl(pin, status);
	}
}
