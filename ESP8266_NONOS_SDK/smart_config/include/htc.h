
#define HTT_HEAD 0xAA

#define RET_SUCC 		0x00
#define RET_DATA_ERR 	0x01
#define RET_FAILURE		0X02

#define MCU_CMD_CONFIG			0x1
#define MCU_CMD_DEL_USER		0x2
#define MCU_CMD_REMOTE_UNLOCK  	0x3
#define MCU_CMD_RPT_CMD			0x4
#define MCU_CMD_ALARM_RPT		0x5
#define MCU_CMD_LINK_STAT		0x6
#define MCU_CMD_REMOVE_NET		0x7

#define CMD_FUNC_UNLOCK			0x1
#define CMD_FUNC_LOCK			0x2
#define CMD_FUNC_INFO			0x3
#define CMD_FUNC_LOW_VALT		0x0
#define CMD_FUNC_FORCE_UNLOCK 	0x1
#define CMD_FUNC_HIJACK			0x2

#define UNLOCK_METHOD_PWD		0
#define UNLOCK_METHOD_KEY 		1
#define UNLOCK_METHOD_FINGER 	2
#define UNLOCK_METHOD_CARD		3
#define UNLOCK_METHOD_REMOTE 	4
#define UNLOCK_METHOD_FACE   	5
#define UNLOCK_METHOD_PALM		6
#define UNLOCK_METHOD_RING		7

#define LINK_STAT_ONLINE	0
#define LINK_STAT_OFFLINE	1
#define LINK_STAT_ERR		2

typedef struct mcu_package {
	uint8 head;
	uint8 cmd;
	uint8 func;
	uint8 data1;
	uint8 data2;
	uint8 data3;
	uint8 chksum;
} mcu_pkg;

unsigned char get_bcc(unsigned char *src, unsigned char len);
int8 htc_rx_process(uint8 *uart_buf);
void htc_send_reply(mcu_pkg *pMcuCmd);


