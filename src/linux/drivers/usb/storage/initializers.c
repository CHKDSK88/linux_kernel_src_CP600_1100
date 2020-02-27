/* Special Initializers for certain USB Mass Storage devices
 *
 * Current development and maintenance by:
 *   (c) 1999, 2000 Matthew Dharm (mdharm-usb@one-eyed-alien.net)
 *
 * This driver is based on the 'USB Mass Storage Class' document. This
 * describes in detail the protocol used to communicate with such
 * devices.  Clearly, the designers had SCSI and ATAPI commands in
 * mind when they created this document.  The commands are all very
 * similar to commands in the SCSI-II and ATAPI specifications.
 *
 * It is important to note that in a number of cases this class
 * exhibits class-specific exemptions from the USB specification.
 * Notably the usage of NAK, STALL and ACK differs from the norm, in
 * that they are used to communicate wait, failed and OK on commands.
 *
 * Also, for certain devices, the interrupt endpoint is used to convey
 * status of a command.
 *
 * Please see http://www.one-eyed-alien.net/~mdharm/linux-usb for more
 * information about this driver.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/errno.h>

#include "usb.h"
#include "initializers.h"
#include "debug.h"
#include "transport.h"
#include <linux/delay.h>

/* This places the Shuttle/SCM USB<->SCSI bridge devices in multi-target
 * mode */
int usb_stor_euscsi_init(struct us_data *us)
{
	int result;

	US_DEBUGP("Attempting to init eUSCSI bridge...\n");
	us->iobuf[0] = 0x1;
	result = usb_stor_control_msg(us, us->send_ctrl_pipe,
			0x0C, USB_RECIP_INTERFACE | USB_TYPE_VENDOR,
			0x01, 0x0, us->iobuf, 0x1, 5*HZ);
	US_DEBUGP("-- result is %d\n", result);

	return 0;
}

/* This function is required to activate all four slots on the UCR-61S2B
 * flash reader */
int usb_stor_ucr61s2b_init(struct us_data *us)
{
	struct bulk_cb_wrap *bcb = (struct bulk_cb_wrap*) us->iobuf;
	struct bulk_cs_wrap *bcs = (struct bulk_cs_wrap*) us->iobuf;
	int res;
	unsigned int partial;
	static char init_string[] = "\xec\x0a\x06\x00$PCCHIPS";

	US_DEBUGP("Sending UCR-61S2B initialization packet...\n");

	bcb->Signature = cpu_to_le32(US_BULK_CB_SIGN);
	bcb->Tag = 0;
	bcb->DataTransferLength = cpu_to_le32(0);
	bcb->Flags = bcb->Lun = 0;
	bcb->Length = sizeof(init_string) - 1;
	memset(bcb->CDB, 0, sizeof(bcb->CDB));
	memcpy(bcb->CDB, init_string, sizeof(init_string) - 1);

	res = usb_stor_bulk_transfer_buf(us, us->send_bulk_pipe, bcb,
			US_BULK_CB_WRAP_LEN, &partial);
	if(res)
		return res;

	US_DEBUGP("Getting status packet...\n");
	res = usb_stor_bulk_transfer_buf(us, us->recv_bulk_pipe, bcs,
			US_BULK_CS_WRAP_LEN, &partial);

	return (res ? -1 : 0);
}

/* This places the HUAWEI E220 devices in multi-port mode */
int usb_stor_huawei_e220_init(struct us_data *us)
{
	int result;
	result = usb_stor_control_msg(us, us->send_ctrl_pipe,
			USB_REQ_SET_FEATURE,
			USB_TYPE_STANDARD | USB_RECIP_DEVICE,
			0x01, 0x0, NULL, 0x0, 1000);
	US_DEBUGP("Huawei mode set result is %d\n", result);
	return 0;
}
int usb_stor_huawei_e3276_601_init(struct us_data *us)
{
	struct bulk_cb_wrap *bcb = (struct bulk_cb_wrap*) us->iobuf;
	int res;
	unsigned int partial;
	
	static char init_string[] = {0x11,0x06,0x20,0x00,0x00,0x01,0x01,0x00,0x01};
								

	printk("Sending HUAWEI initialization packet usb_stor_huawei_e3276_init...\n");

	bcb->Signature = cpu_to_le32(US_BULK_CB_SIGN);
	bcb->Tag = 0x12345678;
	bcb->DataTransferLength = cpu_to_le32(0);
	bcb->Flags = bcb->Lun = 0;
	bcb->Length = sizeof(init_string);
	memset(bcb->CDB, 0, sizeof(bcb->CDB));
	memcpy(bcb->CDB, init_string, sizeof(init_string));

	res = usb_stor_bulk_transfer_buf(us, us->send_bulk_pipe, bcb,
			US_BULK_CB_WRAP_LEN, &partial);

	printk("Huawei mode set result is %d\n", res);
	return 0;
}
int usb_stor_huawei_e3276_init(struct us_data *us)
{
	struct bulk_cb_wrap *bcb = (struct bulk_cb_wrap*) us->iobuf;
	int res;
	unsigned int partial;
	static char init_string[] = {0x11, 0x06, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00};

	printk("Sending HUAWEI initialization packet usb_stor_huawei_e3276_init...\n");

	bcb->Signature = cpu_to_le32(US_BULK_CB_SIGN);
	bcb->Tag = 0x12345678;
	bcb->DataTransferLength = cpu_to_le32(0);
	bcb->Flags = bcb->Lun = 0;
	bcb->Length = sizeof(init_string);
	memset(bcb->CDB, 0, sizeof(bcb->CDB));
	memcpy(bcb->CDB, init_string, sizeof(init_string));

	res = usb_stor_bulk_transfer_buf(us, us->send_bulk_pipe, bcb,
			US_BULK_CB_WRAP_LEN, &partial);

	printk("Huawei mode set result is %d\n", res);
	return 0;
}

int usb_stor_huawei_e182_init(struct us_data *us)
{
	struct bulk_cb_wrap *bcb = (struct bulk_cb_wrap*) us->iobuf;
	int res;
	unsigned int partial;
	static char init_string[] = {0x11,0x06};

	US_DEBUGP("Sending HUAWEI initialization packet...\n");

	bcb->Signature = cpu_to_le32(US_BULK_CB_SIGN);
	bcb->Tag = 0x12345678;
	bcb->DataTransferLength = cpu_to_le32(0);
	bcb->Flags = bcb->Lun = 0;
	bcb->Length = sizeof(init_string);
	memset(bcb->CDB, 0, sizeof(bcb->CDB));
	memcpy(bcb->CDB, init_string, sizeof(init_string));

	res = usb_stor_bulk_transfer_buf(us, us->send_bulk_pipe, bcb,
			US_BULK_CB_WRAP_LEN, &partial);

	US_DEBUGP("Huawei mode set result is %d\n", res);
	return 0;
}

int usb_stor_photo_cdma_init(struct us_data *us)
{
	struct bulk_cb_wrap *bcb = (struct bulk_cb_wrap*) us->iobuf;
	int res;
	unsigned int partial;
	static char init_string[] = {0xFF,0x05,0xB1,0x12,0xAE,0xE1,0x02};

	US_DEBUGP("Sending PHOTON initialization packet...\n");

	bcb->Signature = cpu_to_le32(US_BULK_CB_SIGN);
	bcb->Tag = 0x12345678;
	bcb->DataTransferLength = cpu_to_le32(0x24);
	bcb->Flags = 0x80;
	bcb->Lun = 0;
	bcb->Length = sizeof(init_string);
	memset(bcb->CDB, 0, sizeof(bcb->CDB));
	memcpy(bcb->CDB, init_string, sizeof(init_string));

	res = usb_stor_bulk_transfer_buf(us, us->send_bulk_pipe, bcb,
			US_BULK_CB_WRAP_LEN, &partial);

	US_DEBUGP("PHOTON mode set result is %d\n", res);
	return 0;
}

int usb_stor_zte_init2(struct us_data *us)
{
	struct bulk_cb_wrap *bcb = (struct bulk_cb_wrap*) us->iobuf;
		int res;
		unsigned int partial;
		static char init_string[] = {0x1b,0x0, 0x0, 0x0, 0x02, 0x0};

		US_DEBUGP("Sending ZTE initialization packet...\n");

		bcb->Signature = cpu_to_le32(US_BULK_CB_SIGN);
		bcb->Tag = 0x12345678;
		bcb->DataTransferLength = cpu_to_le32(0x0);
		bcb->Flags = 0;
		bcb->Lun = 0;
		bcb->Length = sizeof(init_string);
		memset(bcb->CDB, 0, sizeof(bcb->CDB));
		memcpy(bcb->CDB, init_string, sizeof(init_string));

		res = usb_stor_bulk_transfer_buf(us, us->send_bulk_pipe, bcb,
				US_BULK_CB_WRAP_LEN, &partial);

		US_DEBUGP("ZTE mode set result is %d\n", res);
		return 0;
}
int usb_stor_zte_init(struct us_data *us)
{
	struct bulk_cb_wrap *bcb = (struct bulk_cb_wrap*) us->iobuf;
		int res;
		unsigned int partial;
		static char init_string[] = {0x9F,0x03, 0x0, 0x0, 0x0, 0x0};

		US_DEBUGP("Sending ZTE initialization packet...\n");

		bcb->Signature = cpu_to_le32(US_BULK_CB_SIGN);
		bcb->Tag = 0x12345678;
		bcb->DataTransferLength = cpu_to_le32(0xC0);
		bcb->Flags = 0x80;
		bcb->Lun = 0;
		bcb->Length = sizeof(init_string);
		memset(bcb->CDB, 0, sizeof(bcb->CDB));
		memcpy(bcb->CDB, init_string, sizeof(init_string));

		res = usb_stor_bulk_transfer_buf(us, us->send_bulk_pipe, bcb,
				US_BULK_CB_WRAP_LEN, &partial);

		US_DEBUGP("ZTE mode set result is %d\n", res);
		return 0;
}

int usb_stor_hspa_init(struct us_data *us)
{
	struct bulk_cb_wrap *bcb = (struct bulk_cb_wrap*) us->iobuf;
		int res;
		unsigned int partial;
		static char init_string[] = {0x6,0xf5, 0x4, 0x2, 0x52, 0x70};

		US_DEBUGP("Sending HSPA initialization packet...\n");

		bcb->Signature = cpu_to_le32(US_BULK_CB_SIGN);
		bcb->Tag = 0x48680a88;
		bcb->DataTransferLength = cpu_to_le32(0xC0);
		bcb->Flags = 0x80;
		bcb->Lun = 01;
		bcb->Length = sizeof(init_string);
		memset(bcb->CDB, 0, sizeof(bcb->CDB));
		memcpy(bcb->CDB, init_string, sizeof(init_string));

		res = usb_stor_bulk_transfer_buf(us, us->send_bulk_pipe, bcb,
				US_BULK_CB_WRAP_LEN, &partial);

		US_DEBUGP("HSPA mode set result is %d\n", res);
		return 0;
}

int usb_stor_novatel_mc547_init(struct us_data *us)
{
	struct bulk_cb_wrap *bcb = (struct bulk_cb_wrap*) us->iobuf;
	int res;
	unsigned int partial;
	static char init_string[] = {0x1b, 0x00, 0x00, 0x00, 0x02, 0x00};

	US_DEBUGP("Sending Novatel MC547 initialization packet...\n");

	bcb->Signature = cpu_to_le32(US_BULK_CB_SIGN);
	bcb->Tag = 0x12345678;
	bcb->DataTransferLength = cpu_to_le32(0);
	bcb->Flags = 0x0;
	bcb->Lun = 0;
	bcb->Length = sizeof(init_string);
	memset(bcb->CDB, 0, sizeof(bcb->CDB));
	memcpy(bcb->CDB, init_string, sizeof(init_string));

	res = usb_stor_bulk_transfer_buf(us, us->send_bulk_pipe, bcb,
			US_BULK_CB_WRAP_LEN, &partial);

	US_DEBUGP("Novatel MC547 mode set result is %d\n", res);
	return 0;
}

int hspa_mbd_220HU_init(struct us_data *us)
{
    mdelay(1000);
	struct bulk_cb_wrap *bcb = (struct bulk_cb_wrap*) us->iobuf;
	int res;
	unsigned int partial;
	static char init_string[] = {0x06, 0xf5, 0x04, 0x02, 0x52, 0x70};

	US_DEBUGP("Sending MBD 220HU initialization packet...\n");

	bcb->Signature = cpu_to_le32(US_BULK_CB_SIGN);
	bcb->Tag = 0x12345678;
	bcb->DataTransferLength = cpu_to_le32(0x80);
	bcb->Flags = 0x80;
	bcb->Lun = 0;
	bcb->Length = sizeof(init_string);
	memset(bcb->CDB, 0, sizeof(bcb->CDB));
	memcpy(bcb->CDB, init_string, sizeof(init_string));

	res = usb_stor_bulk_transfer_buf(us, us->send_bulk_pipe, bcb,
			US_BULK_CB_WRAP_LEN, &partial);

	printk("MBD 220HU mode set result is %d\n", res);
	return 0;
}


int UMW190_init(struct us_data *us)
{
    mdelay(1000);
	struct bulk_cb_wrap *bcb = (struct bulk_cb_wrap*) us->iobuf;
	int res;
	unsigned int partial;
	static char init_string[] = {0xff,0x02,0x0,0,0,0,0,0};

	printk("Sending UMW190_init initialization packet...\n");

	bcb->Signature = cpu_to_le32(US_BULK_CB_SIGN);
	bcb->Tag = 0x12345678;
	bcb->DataTransferLength = cpu_to_le32(0x0);
	bcb->Flags = 0;
	bcb->Lun = 0;
	bcb->Length = sizeof(init_string);
	memset(bcb->CDB, 0, sizeof(bcb->CDB));
	memcpy(bcb->CDB, init_string, sizeof(init_string));

	res = usb_stor_bulk_transfer_buf(us, us->send_bulk_pipe, bcb,
			US_BULK_CB_WRAP_LEN, &partial);

	printk(" UMW190_init mode set result is %d\n", res);
	return 0;
}

int usb_stor_tplink_init(struct us_data *us)
{
	struct bulk_cb_wrap *bcb = (struct bulk_cb_wrap*) us->iobuf;
		int res;
		unsigned int partial;
		static char init_string[] = {0x1b,0x0, 0x0, 0x0, 0x02, 0x0};

		US_DEBUGP("Sending TP-LINK initialization packet...\n");

		bcb->Signature = cpu_to_le32(US_BULK_CB_SIGN);
		bcb->Tag = 0x12345678;
		bcb->DataTransferLength = cpu_to_le32(0x0);
		bcb->Flags = 0;
		bcb->Lun = 0;
		bcb->Length = sizeof(init_string);
		memset(bcb->CDB, 0, sizeof(bcb->CDB));
		memcpy(bcb->CDB, init_string, sizeof(init_string));

		res = usb_stor_bulk_transfer_buf(us, us->send_bulk_pipe, bcb,
				US_BULK_CB_WRAP_LEN, &partial);

		US_DEBUGP("TP-LINK mode set result is %d\n", res);
		return 0;
}
int hspa_MC950D_init(struct us_data *us)
{
    mdelay(1000);
	struct bulk_cb_wrap *bcb = (struct bulk_cb_wrap*) us->iobuf;
	int res;
	unsigned int partial;
	static char init_string[] = {0x1b,0x00,0x00,0x00,0x02,0x00};

	printk("Sending MC950D initialization packet...\n");

	bcb->Signature = cpu_to_le32(US_BULK_CB_SIGN);
	bcb->Tag = 0x12345678;
	bcb->DataTransferLength = cpu_to_le32(0x0);
	bcb->Flags = 0;
	bcb->Lun = 0;
	bcb->Length = sizeof(init_string);
	memset(bcb->CDB, 0, sizeof(bcb->CDB));
	memcpy(bcb->CDB, init_string, sizeof(init_string));

	res = usb_stor_bulk_transfer_buf(us, us->send_bulk_pipe, bcb,
			US_BULK_CB_WRAP_LEN, &partial);

	printk(" MC950D mode set result is %d\n", res);
	return 0;
}
int hspa_NCXX_init(struct us_data *us)
{
    mdelay(1000);
	struct bulk_cb_wrap *bcb = (struct bulk_cb_wrap*) us->iobuf;
	int res;
	unsigned int partial;
	static char init_string[] = {0x06,0xf5,0x04,0x02,0x52,0x70};

	printk("Sending NCXX initialization packet...\n");

	bcb->Signature = cpu_to_le32(US_BULK_CB_SIGN);
	bcb->Tag = 0x12345678;
	bcb->DataTransferLength = cpu_to_le32(0x0);
	bcb->Flags = 0;
	bcb->Lun = 0;
	bcb->Length = sizeof(init_string);
	memset(bcb->CDB, 0, sizeof(bcb->CDB));
	memcpy(bcb->CDB, init_string, sizeof(init_string));

	res = usb_stor_bulk_transfer_buf(us, us->send_bulk_pipe, bcb,
			US_BULK_CB_WRAP_LEN, &partial);

	printk(" NCXX mode set result is %d\n", res);
	return 0;
}