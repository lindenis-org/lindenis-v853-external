#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <wifimg.h>
#include <wifi_log.h>
#include <linkd.h>
#include <net/if.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define PORT_DEFAULT 8066

struct soft_ap_resource {
	int fd;
	char ssid[SSID_MAX_LEN];
	char psk[PSK_MAX_LEN];
};

void *_softap_mode_main_loop(void *arg)
{
	WMG_INFO("support softap mode config net\n");
	proto_main_loop_para_t *main_loop_para = (proto_main_loop_para_t *)arg;

	int ret = -1;
	char ssid_buf[SSID_MAX_LEN] = "Aw-wifimg-Test";
	char psk_buf[PSK_MAX_LEN] = "aw123456";
	wifi_ap_config_t ap_config;
	wmg_linkd_result_t linkd_result;

	struct soft_ap_resource resource = {
		.fd = -1,
		.ssid = {0},
		.psk = {0},
	};

	int port = PORT_DEFAULT;
	bool is_receive = false;

	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	WMG_DEBUG("*****port:%d *****\n", port);

	addr.sin_addr.s_addr = htonl(INADDR_ANY);

	int sock;
	if((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0){
		WMG_ERROR("*****socket error!*****\n");
		WMG_ERROR("*****%s *****\n",strerror(errno));
		main_loop_para->result_cb(NULL);
		return NULL;
	}else{
		WMG_INFO("*****socket success.*****\n");
		resource.fd = sock;
	}

	if(bind(sock, (struct sockaddr *)&addr, sizeof(addr))< 0){
		WMG_ERROR("*****bind error!*****\n");
		WMG_ERROR("*****%s *****\n", strerror(errno));
		main_loop_para->result_cb(NULL);
		goto soket_end;
	}else{
		WMG_INFO("bind success.\n");
	}

	//receive usr app ssid information
	char buff[512];
	struct sockaddr_in clientAddr;
	int len = sizeof(clientAddr);
	int n;

	ret = wifi_on(WIFI_AP);
	if (ret == 0) {
		WMG_INFO("Info: wifi on ap mode success\n");
	} else {
		WMG_ERROR("Error: wifi on ap mode failed\n");
		main_loop_para->result_cb(NULL);
		goto soket_end;
	}
	ap_config.ssid = ssid_buf;
	ap_config.psk = psk_buf;
	ap_config.sec = WIFI_SEC_WPA2_PSK;
	ap_config.channel = 6;
	ret = wifi_ap_enable(&ap_config);
	if (ret == 0) {
		WMG_INFO("Info: ap enable success, ssid=%s, psk=%s, sec=%d, channel=%d\n",
		ap_config.ssid, ap_config.psk, ap_config.sec, ap_config.channel);
	} else {
		WMG_ERROR("Error: ap enable failed\n");
		main_loop_para->result_cb(NULL);
		goto soket_end;
	}

	while(1) {
		WMG_INFO("recvfrom...\n");
		n = recvfrom(sock, buff, 511, 0, (struct sockaddr*)&clientAddr, &len);
		WMG_INFO("%s *****\n", buff);
		if(n>0){
			WMG_INFO("recvfrom success.\n");
			break;
		}
	}

	//feedback message to usr app
	char buff_confirm[3];
	strcpy(buff_confirm,"OK");
	while(1) {
		n = sendto(sock, buff_confirm, n, 0, (struct sockaddr *)&clientAddr, sizeof(clientAddr));
		if(n>0){
			WMG_DEBUG("sendto success.\n");
			break;
		}
	}

	//Analyze usr app information
	char *p =NULL;

	int ssid_len = strstr(buff, "::Password::")-buff-8;
	if(ssid_len > SSID_MAX_LEN){
		WMG_ERROR("ssid_len(%d) is longer than ssid_buf(%d)\n", ssid_len, SSID_MAX_LEN);
		main_loop_para->result_cb(NULL);
		goto soket_end;
	}
	if((p = strstr(buff, "::SSID::")) != NULL){
		p += strlen("::SSID::");
		if(*p){
			if(strstr(p, "::Password::") != NULL){
				strncpy(resource.ssid, p, ssid_len);
			}
		}
	}
	WMG_INFO("softap get ssid:%s\n",resource.ssid);

	int password_len = strstr(buff, "::End::") - strstr(buff, "::Password::") - 12;
	if(password_len > PSK_MAX_LEN){
		WMG_ERROR("password_len(%d) is longer than psk_buf(%d)\n", password_len, PSK_MAX_LEN);
		main_loop_para->result_cb(NULL);
		goto soket_end;
	}
	if((p = strstr(buff, "::Password::")) != NULL){
		p += strlen("::Password::");
		if(*p){
			if(strstr(p, "::End::") != NULL){
				strncpy(resource.psk, p, password_len);
			}
		}
	}
	WMG_INFO("softap get psk:%s\n",resource.psk);

	linkd_result.ssid = resource.ssid;
	linkd_result.psk = resource.psk;
	main_loop_para->result_cb(&linkd_result);

soket_end:
	close(resource.fd);
	return NULL;
}
