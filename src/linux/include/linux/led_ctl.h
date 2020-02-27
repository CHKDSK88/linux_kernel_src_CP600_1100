
#ifndef __GENERIC_LED_LED__H_
#define __GENERIC_LED_LED__H_

#define LED_REASON_UNDEFINED    0

#define LED_COLOR_UNDEFINED     (-1)
#define LED_COLOR_OFF           0

#define LED_COLOR_NAME_LEN      16
#define LED_NAME_LEN            16

typedef void(*gl_led_ctl_fn_t)(int);

typedef struct __gl_led_info_t {
	struct list_head   entry;
	char               name[LED_NAME_LEN+1]; /* LED name */
	int                color;                /* LED color */
	int                tmp_color;            /* LED color for temporaty blink */
	unsigned int       blink_speed;          /* blink speed (0 - solid, 1-9) */
	unsigned int       tmp_blink_speed;      /* blink speed for temporaty blink */
	unsigned int       tmp_blink_cycles;     /* tep blink counter */
	int                onoff;                /* current status of the LED (used while blinking) */
	struct delayed_work led_work;
	struct proc_dir_entry *proc;
	gl_led_ctl_fn_t    led_ctl_fn;
	char**             led_colors_tbl;
	spinlock_t         lock;
	unsigned char      reason_bit; /* USR/CPU/MEM */

} gl_led_info_t;

int register_led(gl_led_info_t*, const char* , gl_led_ctl_fn_t);
int unregister_led(gl_led_info_t*);

void __gl_set_led_color(gl_led_info_t* led, int color);
void __gl_set_led_blink_speed(gl_led_info_t* led, unsigned int blink_speed);
void __gl_set_led_temp_blink(gl_led_info_t* led, unsigned int blink_speed, int cycles, int color);


#endif /* __GENERIC_LED_LED__H_ */
