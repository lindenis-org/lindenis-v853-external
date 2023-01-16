#ifndef INI_CONFIG_H
#define INI_CONFIG_H

#define LINE_CONTENT_MAX_LEN  256

#define AWCAST_CONFIG_FILE      "/usr/local/etc/awcast.cfg"
#define AWCAST_DEVICE_NAME      "device_name"
#define AWCAST_MIRACAST_ONLY    "miracast_only"

int read_string_value(const char *key, char *value,const char *file);
int read_int_value(const char *key, int *value,const char *file);
int write_string_value(const char *key, char *value,const char *file);
int write_int_value(const char *key, int value,const char *file);

#endif //INI_CONFIG_H
