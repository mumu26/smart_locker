#include "c_types.h"
#include "ets_sys.h"
#include "osapi.h"
#include "ip_addr.h"
#include "mem.h"
#include "espconn.h"
#include <stc.h>
#include "mbedtls/aes.h"
#include "user_interface.h"
#include "htc.h"

static struct espconn *espconn_ptr = NULL;
static web_frame rx_frame;
static uint8 sw_version[SW_VERSION_LEN]={"20190622.01"};
uint8 sign_in_done = 0;
static uint8 g_company_id[COMPANY_ID_LEN] = {'0', '1'};
static uint8 g_equip_id[EQUIP_ID_LEN] = {"XXX"};
static uint8 g_equip_type[EQUIP_TYPE_LEN] = {"0301A"};
static uint8 g_checksum_code[CHKSUM_CODE_LEN];
uint8 g_tcp_stat = TCP_STAT_DISCONNECTED;
//uint8 g_req_unlock_completion;
extern os_timer_t mcu_wait_timer[MAX_TIMER_NUM];
extern mcu_pkg g_rx_mcu_pkg;

void stc_process_server_cmd()
{
	uint16 cmd;
	uint8 i;
	mcu_pkg htc_reply_cmd;
	
	if (rx_frame.content1 == NULL) {
		os_printf("cmd is null, no need to process\n");
		return;
	}	

	memset(&htc_reply_cmd, 0, sizeof(htc_reply_cmd));

	/* cmd from server should start with '0' */
	if (rx_frame.content1[0] == '0' && rx_frame.content1[1] == '0') {
	switch(rx_frame.content1[2]) {
		//sign in ack
		case '0':			
			if (sign_in_done == 1) {
				sign_in_done = 2;
				os_printf("sign in is completed\n");
			} else {
				os_printf("sign in is invalid\n");
			}

			break;

		//Get IMSI number
		case  '1':
			stc_encap_send(NULL, 0, &rx_frame);
			break;
		
		//Get SW version
		case '2':
			stc_encap_send(sw_version, SW_VERSION_LEN, &rx_frame);
			break;
		
		//Get device status
		case '3':
			stc_encap_send(NULL, 0, &rx_frame);
			break;
			
		//Issue control command, with unlock code
		case '4':
			stc_encap_send(NULL, 0, &rx_frame);
			//unlock:00
			if (rx_frame.content2[0] == '0' && rx_frame.content2[1] == '0') {
				os_timer_disarm(&mcu_wait_timer[REQ_UNLOCK_TIMER]);				
				if(!memcmp(&rx_frame.content2[2], &g_checksum_code, CHKSUM_CODE_LEN)) {
					os_printf("checksum code is correct!\n");
					htc_reply_cmd.data1 = RET_SUCC;
				}
				else {
					os_printf("checksum code not match, in rx is \n");
					htc_reply_cmd.data1 = RET_DATA_ERR;
					for (i=0;i<CHKSUM_CODE_LEN;i++)
						os_printf("%c",rx_frame.content2[2+i]);
					os_printf("\n");
				}
				htc_send_reply(&htc_reply_cmd);
			}
			break;
			
		//Get time to start
		case '5':
			stc_encap_send(NULL, 0, &rx_frame);
			break;
			
		//Change DNS and port
		case '6':
			stc_encap_send(NULL, 0, &rx_frame);
			break;
			
		//Restart GPRS/wifi module
		case '7':
			stc_encap_send(NULL, 0, &rx_frame);
			system_restart();
			break;
			
		//server ack of req unlock
		case '8':
			os_printf("unlock req is recv by server\n");			
			break;
		//Set rend deadline
		case '9':
			stc_encap_send(NULL, 0, &rx_frame);
			break;
		case 'A':
			os_timer_disarm(&mcu_wait_timer[RPT_ALARM_TIMER]);
			htc_send_reply(&htc_reply_cmd);
			break;
		//report lock action
		case 'B':
			os_printf("rpt lock action is recv by server\n");
			os_timer_disarm(&mcu_wait_timer[RPT_ACTION_TIMER]);
			htc_send_reply(&htc_reply_cmd);
			break;
		default:
			stc_encap_send(NULL, 0, &rx_frame);
			os_printf("cmd %c is not defined\n", rx_frame.content1[2]);
			break;
	}
	}
}

void ICACHE_FLASH_ATTR espconn_connect_cb(void *arg)
{
	int8 conn_stat;
	remot_info *premot = NULL;
	mcu_pkg htc_reply_cmd = {0};
	
    conn_stat = espconn_get_connection_info(espconn_ptr,&premot,0);
	if (premot == NULL)
		os_printf("premot is NULL\n");
	else {
		os_printf("remote info:port is %d,ip is %d.%d.%d.%d\n", 
		premot->remote_port,
		premot->remote_ip[0],
		premot->remote_ip[1],
		premot->remote_ip[2],
		premot->remote_ip[3]);
	    //always sign in when connect to avoid unknown of passive disconnect
		stc_sign_in();
		if (sign_in_done == 0) {
			//stc_sign_in();
			sign_in_done = 1;
		}
	}
	g_tcp_stat = TCP_STAT_CONNECTED;
	os_timer_disarm(&mcu_wait_timer[INIT_TCP_TIMER]);	
	htc_reply_cmd.data1 = RET_SUCC;
	htc_send_reply(&htc_reply_cmd);
}

void ICACHE_FLASH_ATTR espconn_recon_cb(void *arg, sint8 errType)
{
	os_printf("Enter espconn_recon_cb\n");
}

void ICACHE_FLASH_ATTR espconn_discon_cb(void *arg)
{
	g_tcp_stat = TCP_STAT_DISCONNECTED;
	os_printf("Enter espconn_discon_cb\n");
}

void ICACHE_FLASH_ATTR espconn_recv_cb(void *arg, char *pusrdata, unsigned short len)
{
	uint8 data[10];
	uint8 index = 0;
	uint8 i=0;
	os_printf("Enter espconn_recv_cb\n");

	os_memset(&rx_frame, 0, sizeof(web_frame));
	/* '/S/...\r\n', head+tail=5 */
	if (len < 5) {
		os_printf("len is too small %d\n", len);
		return;
	}

	/* Start and End frame sanity check */
	if (pusrdata[0] != DELIMETER 
		|| pusrdata[1] != START 
		|| pusrdata[2] != DELIMETER
		|| pusrdata[len-2] != '\r' 
		|| pusrdata[len-1] != '\n') {
		os_printf("invalid data 0-%c, 1-%c,2-%c, last-1-%c,last-%c\n",
			pusrdata[0],pusrdata[1],pusrdata[2],pusrdata[len-2],pusrdata[len-1]);
		return;
		}

	index += 3;
	rx_frame.company_id = (uint8 *)(pusrdata + index);
	os_printf("frame member:company id: %c%c\n", 
		rx_frame.company_id[0], rx_frame.company_id[1]);

	index += COMPANY_ID_LEN;
	if (pusrdata[index] != DELIMETER) {
		os_printf("data at %d should be a delimeter, but not\n", index);
		return;
	}
	
	index += 1;
	rx_frame.equip_id = (uint8 *)(pusrdata + index);
	os_printf("equip_id is:");
	for(i=0;i<15;i++)
		os_printf("%c", rx_frame.equip_id[i]);

	index += EQUIP_ID_LEN;
	if (pusrdata[index] != DELIMETER) {
		os_printf("data at %d should be a delimeter, but not\n", index);
		return;
	}

	index += 1;
	rx_frame.equip_type = (uint8 *)(pusrdata + index);
	os_printf("\nequip_type is:");
	for(i=0;i<5;i++)
		os_printf("%c", rx_frame.equip_type[i]);

	index += EQUIP_TYPE_LEN;
	if (pusrdata[index] != DELIMETER) {
		os_printf("data at %d should be a delimeter, but not\n", index);
		return;
	}

	index += 1;
	rx_frame.content1 = (uint8 *)(pusrdata + index);
	os_printf("\ncontent1 is:");
		for(i=0;i<3;i++)
			os_printf("%c", rx_frame.content1[i]);

	if (len > FRAME_CONTENT1_LEN) {
		index += CONTENT1_LEN;
		if (pusrdata[index] != DELIMETER) {
			os_printf("\ndata at %d should be a delimeter, but not\n", index);
			return;
		}

		index += 1;
		rx_frame.content2 = (uint8 *)(pusrdata + index);
	}
	os_printf("\nlen is %d\n", len);

	stc_process_server_cmd();
}

void ICACHE_FLASH_ATTR espconn_send_cb(void *arg)
{
	os_printf("Enter espconn_send_cb\n");
}

int aes_cbc_128_process(int enc_dec, unsigned char *buf, uint8 len)
{
	int ret = 0, i, j;
    unsigned char key[16] = {'1', '2', '3', 't', 'j', 'z', 'n', 'j', 
						   'j', '2', '1', 'h', 'u', '9', '9', '0'};
    //unsigned char buf[64];
    unsigned char iv[16] = {'B', 'B', 'B', 'B', 'B', 'B', 'B', 'B', 
    						'B', 'B', 'B', 'B', 'B', 'B', 'B', 'B'};
	unsigned char prv[16];
	mbedtls_aes_context ctx;
	uint8 new_len;

    mbedtls_aes_init( &ctx );

	if (len % 16)
		new_len = (1 + len/16) * 16;
	else 
		new_len = len;
	/*
     * CBC mode
     */
        //memset( iv , 0, 16 );
        memset( prv, 0, 16 );
        //memset( buf, 0, 16 );

    if( enc_dec == MBEDTLS_AES_DECRYPT ) {
        mbedtls_aes_setkey_dec( &ctx, key, 128 );

        for( j = 0; j < 10000; j++ )
            mbedtls_aes_crypt_cbc( &ctx, enc_dec, new_len, iv, buf, buf );

    } else {
        mbedtls_aes_setkey_enc( &ctx, key, 128);

        for( j = 0; j < 10000; j++ )
        {
            unsigned char tmp[16];

            mbedtls_aes_crypt_cbc( &ctx, enc_dec, new_len, iv, buf, buf );

            memcpy( tmp, prv, 16 );
            memcpy( prv, buf, 16 );
            memcpy( buf, tmp, 16 );
        }
    }
	
    ret = 0;

    mbedtls_aes_free( &ctx );

    return( ret );
}

uint8 init_tcp_connect() {
	uint32 ip = 0;
	uint8 value;
	
	espconn_ptr = (struct espconn *)os_zalloc(sizeof(struct espconn));
	espconn_ptr->type = ESPCONN_TCP;
	espconn_ptr->state = ESPCONN_NONE;
	espconn_ptr->proto.tcp = (esp_tcp *)os_zalloc(sizeof(esp_tcp));
    espconn_ptr->proto.tcp->local_port = espconn_port();
    //espconn_ptr->proto.tcp->remote_port = 5138;
    espconn_ptr->proto.tcp->remote_port = 8080;

	//ip = ipaddr_addr("47.92.213.131");
	ip = ipaddr_addr("192.168.1.8");
    os_memcpy(espconn_ptr->proto.tcp->remote_ip,&ip,sizeof(ip));
    espconn_regist_connectcb(espconn_ptr, espconn_connect_cb);
    espconn_regist_reconcb(espconn_ptr, espconn_recon_cb);
    espconn_regist_disconcb(espconn_ptr, espconn_discon_cb);
    espconn_regist_recvcb(espconn_ptr, espconn_recv_cb);
    espconn_regist_sentcb(espconn_ptr, espconn_send_cb);
  
    espconn_connect(espconn_ptr);

	if (g_tcp_stat == TCP_STAT_CONNECTED)
		value = RET_SUCC;
	else
		value = RET_FAILURE;
	os_printf("connect return %d\n", value);
	return value;
}

/*
 * pcontent2: pointer of content2
 * length2: the length of content2
 */
void stc_encap_send(uint8 *pcontent2, uint16 length2, web_frame *pweb_frame)
{
	uint8 *head, *send_frame = NULL;
	uint8 i;
	uint16 length;

	length = FRAME_CONTENT1_LEN + length2;
	
	send_frame = os_malloc(length + 1);
	if (NULL == send_frame)
		os_printf("malloc send_frame fail\n");
	os_memset(send_frame, 0, length);
	head = send_frame;
	*send_frame++ = '/';
	*send_frame++ = 'S';
	
	*send_frame++ = '/';
	os_memcpy(send_frame, pweb_frame->company_id, COMPANY_ID_LEN);
	send_frame += COMPANY_ID_LEN;
	
	*send_frame++ = '/';
	os_memcpy(send_frame, pweb_frame->equip_id, EQUIP_ID_LEN);
	send_frame += EQUIP_ID_LEN;
	
	*send_frame++ = '/';
	os_memcpy(send_frame, pweb_frame->equip_type, EQUIP_TYPE_LEN);
	send_frame += EQUIP_TYPE_LEN;
	
	*send_frame++ = '/';
	*send_frame++ = '8';
	os_memcpy(send_frame, pweb_frame->content1+1, CONTENT1_LEN-1);
	send_frame += CONTENT1_LEN-1;

	if (length2 != 0) {
		*send_frame++ = '/';
		os_memcpy(send_frame, pcontent2, length2);
		send_frame += length2;
	}
	
	*send_frame++ = '\r';
	*send_frame++ = '\n';
	espconn_send(espconn_ptr, head, length);
	os_free(head);
}

void stc_sign_in()
{
	uint8 mac[6], hex_mac[12];
	uint8 i;
	web_frame frame_data;
	uint8 content1[CONTENT1_LEN] = {"800"};

	memset(&frame_data, 0, sizeof(web_frame));
	wifi_get_macaddr(STATION_IF, mac);

	for (i=0;i<6;i++) {
		os_sprintf(&hex_mac[2*i], "%02x", mac[i]);
	}

	for (i=0;i<12;i++) {
		if (hex_mac[i] >= 'a' && hex_mac[i] <= 'f')
			hex_mac[i] -= 32;
		
		os_sprintf(&g_equip_id[3+i], "%c", hex_mac[i]);		
	}
	frame_data.company_id = g_company_id;
	frame_data.equip_id = g_equip_id;
	frame_data.equip_type = g_equip_type;
	frame_data.content1 = content1;
	stc_encap_send(NULL, 0, &frame_data);
}

void stc_common_send(uint8 *content1, uint8 *content2, uint8 content2_len)
{
	web_frame frame_data;

	memset(&frame_data, 0, sizeof(web_frame));
	frame_data.company_id = g_company_id;
	frame_data.equip_id = g_equip_id;
	frame_data.equip_type = g_equip_type;
	frame_data.content1 = content1;
	stc_encap_send(content2, content2_len, &frame_data);
}

void stc_req_unlock()
{
	web_frame frame_data;
	uint8 content1[CONTENT1_LEN] = {"808"};

	memset(&frame_data, 0, sizeof(web_frame));
	frame_data.company_id = g_company_id;
	frame_data.equip_id = g_equip_id;
	frame_data.equip_type = g_equip_type;
	frame_data.content1 = content1;
	stc_encap_send(g_checksum_code, CHKSUM_CODE_LEN, &frame_data);
}



void gen_rand()
{
	int type,i,res;	
	os_printf("random result is ");
	for(i=0;i<CHKSUM_CODE_LEN;i++) {
		srand(system_get_time()); 
		type =rand()&1;
		if (type == 0) {
			srand(system_get_time()); 
			res = 48 + rand() % 10;			
		} else {
			srand(system_get_time());
			res = 65 + rand() % 26;
		}
		g_checksum_code[i] = res;
		os_printf("%c ", res);
	}		
}

void stc_send_with_timer()
{
	
}
