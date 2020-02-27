
#include <linux/module.h>
#include <linux/rtc.h>
#include <linux/i2c.h>
#include <linux/bitrev.h>
#include <linux/bcd.h>
#include <linux/slab.h>
#include <marvell/mvHwFeatures.h>

#define IC_SG80_EXTEND1 0x21
#define IC_SG80_EXTEND2 0x20
#define I2C_NOT_INITIALIZED -9999
int was_init=I2C_NOT_INITIALIZED;
static int extender_attach(struct i2c_adapter *adapter);
static int extender_detach(struct i2c_client *client);
int extender_detect_client(struct i2c_adapter *adapter, int address, int kind);

//extern s32 i2c_smbus_write_byte_data(const struct i2c_client *client, u8 command, u8 value);

static struct i2c_driver extender_driver = {
	.driver		= {
		.name	= "extender",
	},
	.attach_adapter		= &extender_attach,
	.detach_client		= &extender_detach,
};

/* store the value */
void i2c_set_clientdata(struct i2c_client *client, void *data);

/* retrieve the value */
void *i2c_get_clientdata(struct i2c_client *client);


struct extender_data {
    struct i2c_client client_extender2;
    struct i2c_client client_extender1;
       
    /* Because the i2c bus is slow, it is often useful to cache the read
       information of a chip for some time (for example, 1 or 2 seconds).
       It depends of course on the device whether this is really worthwhile
       or even sensible. */
    struct mutex update_lock;     /* When we are reading lots of information,
                                     another process should not update the
                                     below information */
    char valid;                   /* != 0 if the following fields are valid. */
    unsigned long last_updated;   /* In jiffies */
	int type;
  };

static unsigned short normal_i2c[] = { IC_SG80_EXTEND2, I2C_CLIENT_END };  
/* Insmod parameters */
I2C_CLIENT_INSMOD;

int extender_read_value(struct i2c_client *client, u8 reg)
{
	return i2c_smbus_read_byte_data(client,reg);
}

int extender_write_value(struct i2c_client *client, u8 reg, u16 value)
{
	return i2c_smbus_write_byte_data(client,reg,value);
}


int extender_detach(struct i2c_client *client)
{
    int err;

    /* Try to detach the client from i2c space */
    if ((err = i2c_detach_client(client)))
      return err;

    kfree(i2c_get_clientdata(client));
    return 0;
}

static int extender_attach(struct i2c_adapter *adapter)
{
	return i2c_probe(adapter,&addr_data,&extender_detect_client);
}
void extender_write_byte(struct i2c_client *client, u8 value)
{
	i2c_smbus_write_byte_data(client,0 ,value);
}

int extender_read_byte(struct i2c_client *client)
{
	return i2c_smbus_read_byte_data(client,0);	
}

struct i2c_client *client_extender1;
struct i2c_client *client_extender2;
int extender_detect_client(struct i2c_adapter *adapter, int address, 
                        int kind)
{
	int err = 0;
	struct extender_data *data;
	
/* Let's see whether this adapter can support what we need.
   Please substitute the things you need here! */
	if (!i2c_check_functionality(adapter,I2C_FUNC_SMBUS_WORD_DATA |
							I2C_FUNC_I2C |
							I2C_FUNC_SMBUS_WRITE_BYTE))
   goto ERROR0;

/* OK. For now, we presume we have a valid client. We now create the
   client structure, even though we cannot fill it completely yet.
   But it allows us to access several i2c functions safely */

	if (!(data = kzalloc(sizeof(struct extender_data), GFP_KERNEL))) {
		err = -ENOMEM;
		goto ERROR0;
	}
	
	client_extender2 = &data->client_extender2;
	client_extender2->addr = address;
	client_extender2->driver = &extender_driver;
	client_extender2->adapter = adapter;
	client_extender1 = &data->client_extender1;
	strlcpy(client_extender2->name, extender_driver.driver.name, I2C_NAME_SIZE);
	strlcpy(client_extender1->name, extender_driver.driver.name, I2C_NAME_SIZE);
	i2c_set_clientdata(client_extender2, data);

	/* Inform the i2c layer */
	if ((err = i2c_attach_client(client_extender2)))
		goto ERROR1;
	/*This chip uses multiple addresses, use dummy devices for them */
	client_extender1 = i2c_new_dummy(client_extender2->adapter,
					IC_SG80_EXTEND1, "extender");
	if ((!client_extender1 ))
		goto ERROR1;
	
/* Now, we do the remaining detection. If no `force' parameter is used. */

	//mutex_init(&data->update_lock); /* Only if you use this field */

/* Any other initializations in data must be done here too. */
	i2c_smbus_write_byte_data(client_extender1,1 ,0);
	i2c_smbus_write_byte_data(client_extender2,1 ,0);

/* This function can write default values to the client registers, if
   needed. */

	was_init = 1;

	return 0;

	/* OK, this is not exactly good programming practice, usually. But it is
	   very code-efficient in this case. */

	ERROR1:
	  kfree(data);	  
	ERROR0:	
	  return err;
}
void set_extender_byte(u8 extender_num, u8 value)
{
	if (was_init != I2C_NOT_INITIALIZED)
	{
		if (extender_num == 2)
			extender_write_byte(client_extender2,value);
		else
			extender_write_byte(client_extender1,value);
	}

}

int get_extender_byte(u8 extender_num)
{
	if (was_init != I2C_NOT_INITIALIZED)
	{
		if (extender_num == 2)
			return extender_read_byte(client_extender2);
		else
			return extender_read_byte(client_extender1);
	}	
	return I2C_NOT_INITIALIZED;

}

void extend_gpio_ctl(unsigned char addr, int gpio_num, int status)
{

	unsigned char reg;
	unsigned char extender_num;
	int rc;
	int i;

	if (is_no_external_ports()) {
		if (addr == IC_SG80_EXTEND1 && gpio_num == 3 && status == 0) {
			printk("%s: USB port1 is disabled by config\n", __FUNCTION__);
			status=1;
		}
		if (addr == IC_SG80_EXTEND1 && gpio_num == 4 && status == 0) {
			printk("%s: USB port2 is disabled by config\n", __FUNCTION__);
			status=1;
		}
		if (addr == IC_SG80_EXTEND1 && gpio_num == 7 && status == 1) {
			printk("%s: Express card is disabled by config\n", __FUNCTION__);
			status=0;
		}
	}

	if (addr == IC_SG80_EXTEND1)
		extender_num=1;
	else
		extender_num=2;

	if((gpio_num>=0) && (gpio_num<=7)){
		for (i=0;i<3;i++)
		{
			rc = get_extender_byte(extender_num);
			if (rc >= 0)
				break;
			else if (rc == I2C_NOT_INITIALIZED)
			{
				printk(KERN_INFO "i2c driver was not initialized yet.\n");
				break;
			}
			else  
				printk(KERN_WARNING "i2c error iteration %d\n",i);		
		}
		if (rc < 0)
	            return;		
		reg = rc;			
		if (status) {
			reg |= (1 << gpio_num);
		} else {
			reg &= ~(1 << gpio_num);
		}
		set_extender_byte(extender_num, reg);
	
    } else {
        printk("can not support this option, please type help.\n");
    }
}
EXPORT_SYMBOL(extend_gpio_ctl);
static int __init extender_init(void)
{
	return i2c_add_driver(&extender_driver);
}

static void __exit extender_exit(void)
{
	i2c_del_driver(&extender_driver);
}

MODULE_AUTHOR("Dafna");
MODULE_DESCRIPTION("Extender driver");
MODULE_LICENSE("GPL");

module_init(extender_init);
module_exit(extender_exit);



