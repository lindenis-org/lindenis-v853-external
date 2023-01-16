#ifndef AW_DLNA_H
#define AW_DLNA_H

#define AWD_EVENT_ENTRY		1
#define AWD_EVENT_QUIT		2
#define AWD_EVENT_VOLUME	3
#define AWD_EVENT_PAUSE		4
#define AWD_EVENT_START		5

typedef struct AWDlnaS AWDlnaT;


struct AWD_LinstenerS
{
	int (*notify)(int /*event*/, void * /*param*/);
};

#ifdef __cplusplus
extern "C"
{
#endif

AWDlnaT *AWD_Instance(char *device_name, char *uuid, struct AWD_LinstenerS *linstener);

int AWD_Start(AWDlnaT *awd);

int AWD_Stop(AWDlnaT *awd);

int AWD_Destroy(AWDlnaT *awd);

#ifdef __cplusplus
}
#endif

#endif
