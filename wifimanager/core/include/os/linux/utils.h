#ifndef __UTILS_H__
#define __UTILS_H__

#include "wmg_common.h"

#if __cplusplus
extern "C" {
#endif

#ifndef PROG_NAME_MAX_LEN
#define PROG_NAME_MAX_LEN	20
#endif
#ifndef CMD_MAX_LEN
#define CMD_MAX_LEN		100
#endif

/**
 * check_process_is_exist - check specified process is exist in system or not
 *
 * @process_name: name of specified process
 * @length: length of process name
 * @return: 1 - exist, 0 - not exist
 */
int check_process_is_exist(const char *process_name, int length);

#if __cplusplus
}
#endif

#endif /*  __UTILS_H__ */
