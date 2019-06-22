#include "c_types.h"
#include "ets_sys.h"
#include "osapi.h"
#include "htc.h"
#include "ip_addr.h"
#include "mem.h"
#include "espconn.h"
#include "mbedtls/aes.h"
#include "stdlib.h"
#include "time.h"
#include "stc.h"

mcu_pkg g_rx_mcu_pkg={0};
extern uint8 g_tcp_stat;
os_timer_t mcu_wait_timer[MAX_TIMER_NUM];

unsigned char get_bcc(unsigned char *src, unsigned char len)
{
	unsigned int bcc = 0;
	unsigned char i;
	for (i = 0; i < len; i++) {
		bcc += src[i];
	}

	return bcc & 0x00FF;
}

/* convert oct number to hex in ascii format
 * input:pointer to the input buffer
 * size: the size of input buffer
 * output:the buffer holding the ascii digit
 * the output size is double of input size
 * e.g.:110=6E  input[0]=110  output[0]='6', output[1]='E'
 */
void convert_int2asciihex(uint8 *input, uint8 size, uint8 *output)
{
	uint8 tmp[100];//max size is 100 digits
	uint8 i;
	
	if (input == NULL || output == NULL)
		return;

	for (i=0;i<size;i++)
		os_sprintf(tmp+2*i, "%02x", *(input+i));

	for (i=0;i<2*size;i++)
		os_sprintf(output+i, "%c", tmp[i]);

	return;
}

uint8 ICACHE_FLASH_ATTR htc_config_entry()
{
	return  init_tcp_connect();
}

void htc_server_timeout()
{
	mcu_pkg reply_cmd;
	memset(&reply_cmd, 0, sizeof(reply_cmd));
	reply_cmd.data1 = RET_FAILURE;
	htc_send_reply(&reply_cmd);
	os_printf("timeout!!!\n");
}

void htc_start_timer(uint8 timer_no, uint32_t timeout)
{
	os_timer_disarm(&mcu_wait_timer[timer_no]);
	os_timer_setfn(&mcu_wait_timer[timer_no], (os_timer_func_t *)htc_server_timeout, NULL);
	os_timer_arm(&mcu_wait_timer[timer_no], timeout, 0);
}

int8 htc_rx_process(uint8 *uart_buf)
{
	unsigned char chksum;
	uint8 ret;
	mcu_pkg *pMcuCmd;
	mcu_pkg reply_cmd;
	uint8 content1[CONTENT1_LEN];
	uint8 content2[8];
	
	if (NULL == uart_buf)
		return -1;

	pMcuCmd = (mcu_pkg *)uart_buf;

	if (pMcuCmd->head != HTT_HEAD) {
		return -1;
	}

	os_memset(&reply_cmd, 0, sizeof(reply_cmd));

	chksum = get_bcc(uart_buf, 6);
	if (chksum != pMcuCmd->chksum) {
		os_printf("MCU cmd chksum is not valid!internal chksum-0x%x,in frame-0x%x\n");
		reply_cmd.data1 = RET_DATA_ERR;
		htc_send_reply(&reply_cmd);
		return FAIL;
	}

	os_memcpy(&g_rx_mcu_pkg, pMcuCmd, sizeof(g_rx_mcu_pkg));
	
	switch (pMcuCmd->cmd) {
		case MCU_CMD_CONFIG:
			os_printf("CMD CONFIG\n");
			htc_config_entry();			
			htc_start_timer(INIT_TCP_TIMER, INIT_TCP_TIMEOUT);
			break;

	   //TBD
		case MCU_CMD_DEL_USER:
			//stc_send("Hello", 6);
			os_printf("CMD DEL USER\n");
			break;
		case MCU_CMD_REMOTE_UNLOCK:
			os_printf("CMD REMOTE UNLOCK\n"); 
			gen_rand();
			stc_req_unlock();
			htc_start_timer(REQ_UNLOCK_TIMER, REQ_UNLOCK_TIMEOUT);
			break;
		//report lock action
		case MCU_CMD_RPT_CMD:
			os_printf("CMD RPT CMD\n");
			strcpy(content1, "80B");
			convert_int2asciihex(&pMcuCmd->func, 4, content2);
			stc_common_send(content1, content2, 8);
			htc_start_timer(RPT_ACTION_TIMER, RPT_ACTION_TIMEOUT);
			break;
		case MCU_CMD_ALARM_RPT:
			os_printf("CMD ALARM RPT\n");
			strcpy(content1, "80A");
			convert_int2asciihex(&pMcuCmd->func, 4, content2);
			stc_common_send(content1, content2, 8);
			htc_start_timer(RPT_ALARM_TIMER, RPT_ALARM_TIMEOUT);
			break;
		case MCU_CMD_LINK_STAT:
			os_printf("CMD LINK STAT\n");
			reply_cmd.data1 = g_tcp_stat;
			htc_send_reply(&reply_cmd);
			break;
		case MCU_CMD_REMOVE_NET:
			os_printf("CMD REMOVE NET\n");
			break;
		default:
			os_printf("Not a valid MCU cmd--0x%x", pMcuCmd->cmd);
	}
	return OK;
}

//func and data field should be filled by caller
void htc_send_reply(mcu_pkg *pMcuCmd)
{
	uint8 len = sizeof(mcu_pkg);
	pMcuCmd->head = HTT_HEAD;
	pMcuCmd->cmd = g_rx_mcu_pkg.cmd;
	pMcuCmd->func = g_rx_mcu_pkg.func;
	pMcuCmd->chksum = get_bcc((uint8 *)pMcuCmd, len - 1);
	//os_printf("cmd func data1 data2 data3--%02x %02x %02x %02x %02x\n",
	//	pMcuCmd->cmd, pMcuCmd->func, pMcuCmd->data1, pMcuCmd->data2, pMcuCmd->data3);
	//os_printf("sending reply to uart\n");
	tx_buff_enq((char *)pMcuCmd,len);
	//os_printf("tx done!\n");
}
