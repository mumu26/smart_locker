#include "user_interface.h"

uint8 init_tcp_connect();

#define DELIMETER '/'
#define START     'S'

#define COMPANY_ID_LEN	2
#define EQUIP_ID_LEN	15
#define EQUIP_TYPE_LEN	5
#define CONTENT1_LEN	3
#define FRAME_CONTENT1_LEN	33

#define SW_VERSION_LEN 11
#define CHKSUM_CODE_LEN 4

#define TCP_STAT_CONNECTED     0
#define TCP_STAT_DISCONNECTED  1

#define INIT_TCP_TIMEOUT    60000  //60s
#define REQ_UNLOCK_TIMEOUT  60000  //60s
#define RPT_ACTION_TIMEOUT  10000   //10s
#define RPT_ALARM_TIMEOUT   10000   //10s

typedef struct webframe {
	uint8 *company_id;
	uint8 *equip_id;
	uint8 *equip_type;
	uint8 *content1;
	uint8 *content2;
} web_frame;

typedef struct saved_stacfg{
	uint32 cfg_updated; //set this flag if the cfg is updated
	uint8 checksum_code[CHKSUM_CODE_LEN];
} saved_sta_param;

enum timer {
	INIT_TCP_TIMER,
	REQ_UNLOCK_TIMER,
	RPT_ACTION_TIMER,
	RPT_ALARM_TIMER,
	MAX_TIMER_NUM,
};

void stc_encap_send(uint8 *pcontent2, uint16 length2, web_frame *pweb_frame);
void stc_sign_in();
void stc_req_unlock();
void stc_start_unlock_timer();
void gen_rand();
void stc_common_send(uint8 *content1, uint8 *content2, uint8 content2_len);


