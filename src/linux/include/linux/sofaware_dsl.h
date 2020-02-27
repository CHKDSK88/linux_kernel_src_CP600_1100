/**************************************************
* sofaware_dsl.h
***************************************************
*/

#ifndef __SOFAWARE_DSL_H__
#define __SOFAWARE_DSL_H__

#define SW_DSL_MISC_MINOR		6

#define IOC_DSL_RESET               0x01 
#define IOC_DSL_SPI_MODE            0x02
#define IOC_DSL_GET_SYNC_STATUS		0x04
#define IOC_DSL_GET_STATUS			0x05
#define IOC_DSL_ERASE_EEPROM    	0x06
#define IOC_DSL_WRITE_EEPROM_PAGE	0x07
#define IOC_DSL_READ_EEPROM_PAGE  	0x08
#define IOC_DSL_GET_EEPROM_PAGE_SIZE 0x09

#define SPI_EEPROM_PAGE_SIZE 528
#define SPI_EEPROM_MAX_PAGES 4096
// the allowed SPI write size is 2Mb for Atmel as well as EON/Spanson
//(might actually be 4Mb but we do not need the addition so we ignore that option)
// following calculation will always give this 2Mb result 
// which will be true even for EON flash that has more but smaller "pages"
#define SPI_EEPROM_MAX_SIZE SPI_EEPROM_PAGE_SIZE*SPI_EEPROM_MAX_PAGES

typedef struct dsl_eeprom_page {
	unsigned int page_num;
	unsigned char buf[SPI_EEPROM_PAGE_SIZE];
} dsl_eeprom_page;

#endif /* #ifndef __SOFAWARE_DSL_H__ */

