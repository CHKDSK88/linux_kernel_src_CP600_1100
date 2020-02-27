/**************************************************
* sofaware_hal.h
***************************************************
*/
#ifndef __SOFAWARE_HAL_H
#define __SOFAWARE_HAL_H

#if (! defined(SBOX4_ON_EVAL)) && (! defined(SBOX4_ON_ARROW))
#include "sofaware-app-init.h"
#endif

struct octeon_eth_mac_desc_t
{
	int eth_dev_num;
	int mac_addr_offset;
	int phy_num;
};

typedef struct {
	int first_quad_lan_address;
	int second_quad_lan_address;
	int first_single_address;
	int second_single_address;
} phy_smi_adresses_t;

phy_smi_adresses_t sofaware_get_smi_addresses(void);

int sofaware_get_num_switch_ports(void);
int sofaware_get_fiber_support(void);
int sofaware_get_sub_model(void);
int sofaware_get_debug_jumper(void);

int sofaware_get_octeon_eth_port_desc(int oct_port_num, struct octeon_eth_mac_desc_t *desc);


int sofaware_is_bin2go_image(void);
void sofaware_get_romdisk_params(const char **dev_name, const char **file_name, unsigned *offset);

#if (! defined(SBOX4_ON_EVAL)) && (! defined(SBOX4_ON_ARROW))
u_int32_t sofaware_get_firmware_total_len(void);
nand_img_offset_t sofaware_get_firmware_img_offset(void);
#endif

const char *sofaware_board_type_string(void);

void octeon_gpio_cfg_output(int bit);
void octeon_gpio_cfg_input(int bit);
void octeon_gpio_set(int bit);
void octeon_gpio_clr(int bit);
int octeon_gpio_value(int bit);

#endif
