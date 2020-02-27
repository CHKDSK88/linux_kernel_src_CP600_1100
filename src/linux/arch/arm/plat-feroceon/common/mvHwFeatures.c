#include <linux/kernel.h>
#include <linux/init.h>


/* Required to get the configuration string from the Kernel Command Line */
int mv_boardFlavor_cmdline(char *s);
int mv_noExtPorts_cmdline(char *s);
__setup("boardFlavor=", mv_boardFlavor_cmdline);
__setup("noExtPorts=", mv_noExtPorts_cmdline);


unsigned int is_seattle_board_cache=0;
unsigned int is_no_ext_ports_cache=0;

int mv_boardFlavor_cmdline(char *s)
{

	if ((s==NULL) || (strncmp(s,"SEATTLE",7))) {
		is_seattle_board_cache=0;
	} else {
		is_seattle_board_cache=1;  
	}
	
	return 1;
}

int mv_noExtPorts_cmdline(char *s)
{
	if ((s==NULL) || (strncmp(s,"1",1))) {
		is_no_ext_ports_cache=0;
	} else {
		is_no_ext_ports_cache=1;  
	}
	
	return 1;
}

int is_seattle_board(void) {
	return is_seattle_board_cache;
}

int is_sd_card_supported(void) {
	return is_seattle_board();
}


int is_rs232_modem_supported(void) {
	return is_seattle_board();
}


int is_no_external_ports(void) {
	return is_no_ext_ports_cache;
}

