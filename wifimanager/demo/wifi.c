#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <string.h>

#define UNIX_DOMAIN "/tmp/UNIX_WIFI.domain"
#define SEND_BUF_SIZE    256

static void print_help()
{
	printf("=======================================================================\n");
	printf("*************************  sta mode Options  **************************\n");
	printf("=======================================================================\n");
	printf("wifi -o sta\n");
	printf("\t: open sta mode\n");
	printf("wifi -f\n");
	printf("\t: close sta mode\n");
	printf("wifi -s\n");
	printf("\t: scan wifi\n");
	printf("wifi -c ssid [passwd]\n");
	printf("\t: connect to an encrypted or non-encrypted ap\n");
	printf("wifi -d\n");
	printf("\t: disconnect from ap\n");
	printf("wifi -a [enable/disable]\n");
	printf("\t: Auto reconnect\n");
	printf("wifi -l [all]\n");
	printf("\t: list connected or saved ap information\n");
	printf("wifi -r [ssid/all]\n");
	printf("\t: remove a specified network or all networks\n");
	printf("wifi -p [softap/ble/xconfig/soundwave]\n");
	printf("\t: softap/ble/xconfig/soundwave distribution network\n");
	printf("\n");
	printf("=======================================================================\n");
	printf("*************************  ap mode Options  ***************************\n");
	printf("=======================================================================\n");
	printf("wifi -o ap [ssid] [passwd]\n");
	printf("\t: open ap mode\n");
	printf("\t: if ssid and passwd is not set, start the default configuration: (allwinner-ap Aa123456)\n");
	printf("\t: if only set ssid, start the ap without passwd\n");
	printf("wifi -l\n");
	printf("\t: list current ap mode information\n");
	printf("wifi -f\n");
	printf("\t: close ap mode\n");
	printf("=======================================================================\n");
	printf("***********************  monitor mode Options  ************************\n");
	printf("=======================================================================\n");
	printf("wifi -o monitor\n");
	printf("\t: open monitor mode\n");
	printf("wifi -f\n");
	printf("\t: close monitor mode\n");
	printf("=======================================================================\n");
	printf("***************************  other Options  ***************************\n");
	printf("=======================================================================\n");
	printf("wifi -D [error/warn/info/debug/dump/exce]\n");
	printf("\t: set debug level\n");
	printf("wifi -g\n");
	printf("\t: get system mac addr\n");
	printf("wifi -m [macaddr]\n");
	printf("\t: set system mac addr\n");
	printf("wifi -h\n");
	printf("\t: print help\n");
	printf("=======================================================================\n");
}

int main(int argc, char *argv[])
{
	int connect_fd;
	int ret;
	char snd_buf[SEND_BUF_SIZE];
	static struct sockaddr_un srv_addr;

	if((argc > 5 || argc <= 1) || (strncmp(argv[1], "-", 1)) || (strlen(argv[1]) != 2)){
		print_help();
		return -1;
	}

	memset(snd_buf, 0, SEND_BUF_SIZE);

	int ch = 0;
	for (;;) {
		ch = getopt(argc, argv, "ac:dfgl::m:o:p:r:sD:h");
		if (ch < 0)
			break;
		switch (ch) {
			case 'o':
				if(!strcmp(argv[2], "sta")) {
					if(argc != 3) {
						print_help();
						return -1;
					}
					sprintf(snd_buf, "o sta");
				} else if (!strcmp(argv[2], "ap")) {
					if(argc == 4) {
						sprintf(snd_buf, "o ap %s",argv[3]);
					} else if(argc == 5) {
						sprintf(snd_buf, "o ap %s %s",argv[3], argv[4]);
					} else {
						sprintf(snd_buf, "o ap");
					}
				} else if(!strcmp(argv[2], "monitor")) {
					if(argc != 3) {
						print_help();
						return -1;
					}
					sprintf(snd_buf, "o monitor");
				} else {
					print_help();
					return -1;
				}
				goto send_cmd;
			case 'f':
				if(argc > 2) {
					print_help();
					return -1;
				}
				sprintf(snd_buf, "f");
				goto send_cmd;
			case 's':
				if(argc > 2) {
					print_help();
					return -1;
				}
				sprintf(snd_buf, "s");
				goto send_cmd;
			case 'c':
				if(argc == 3) {
					sprintf(snd_buf, "c %s",argv[2]);
				} else if (argc == 4) {
					sprintf(snd_buf, "c %s %s",argv[2], argv[3]);
				} else {
					print_help();
					return -1;
				}
				goto send_cmd;
			case 'd':
				if(argc > 2) {
					print_help();
					return -1;
				}
				sprintf(snd_buf, "d");
				goto send_cmd;
			case 'a':
				if(argc == 3) {
					sprintf(snd_buf, "a %s", argv[2]);
				} else {
					print_help();
					return -1;
				}
				goto send_cmd;
			case 'l':
				if(argc == 2) {
					sprintf(snd_buf, "l");
				} else if (argc == 3) {
					if(!strcmp(argv[2], "all")) {
						sprintf(snd_buf, "l all");
					} else {
						print_help();
						return -1;
					}
				}
				goto send_cmd;
			case 'r':
				if(argc == 3) {
					if(!strcmp(argv[2], "all")) {
						sprintf(snd_buf, "r all");
					} else {
						sprintf(snd_buf, "r %s", argv[2]);
					}
				} else {
					print_help();
					return -1;
				}
				goto send_cmd;
			case 'g':
				if(argc > 2) {
					print_help();
					return -1;
				}
				sprintf(snd_buf, "g");
				goto send_cmd;
			case 'm':
				if(argc == 3) {
					sprintf(snd_buf, "m %s", argv[2]);
				} else {
					print_help();
					return -1;
				}
				goto send_cmd;
			case 'p':
				if(argc == 3) {
					sprintf(snd_buf, "p %s", argv[2]);
				} else {
					print_help();
					return -1;
				}
				goto send_cmd;
			case 'D':
				if(argc == 3) {
					sprintf(snd_buf, "D %s", argv[2]);
				} else {
					print_help();
					return -1;
				}
				goto send_cmd;
			case '?':
				printf("Unknown option: %c\n",(char)optopt);
			case 'h':
				print_help();
				return -1;
		}
	}

send_cmd:
	//create unix socket
	connect_fd = socket(PF_UNIX, SOCK_STREAM, 0);
	if(connect_fd < 0) {
		perror("Cannot create wifi deamon communication socket\n");
		return -1;
	}
	srv_addr.sun_family = AF_UNIX;
	strcpy(srv_addr.sun_path,UNIX_DOMAIN);

	//connect server
	ret = connect(connect_fd, (struct sockaddr*)&srv_addr, sizeof(srv_addr));
	if(ret == -1) {
		perror("Cannot connect to the wifi deamon server");
		close(connect_fd);
		return ret;
	}

	write(connect_fd, snd_buf, sizeof(snd_buf));
	close(connect_fd);

	return 0;
}
