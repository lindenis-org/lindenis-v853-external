/*
 * @Author: Wink
 * @Date: 2020-04-14 10:01:09
 * @LastEditTime: 2020-04-14 13:56:47
 * @LastEditors: Please set LastEditors
 * @Description: In User Settings Edit
 * @FilePath: /networkd/src/include/log.h
 */
#ifndef NETWORKD_LOG_CONFIG_H
#define NETWORKD_LOG_CONFIG_H

#include <stdio.h>
#include <tina_log.h>

enum LOG_LEVEL_TYPE
{
    LOG_LEVEL_INFO = 1,
    LOG_LEVEL_DEBUG = 2,
    LOG_LEVEL_WARN = 3,
    LOG_LEVEL_FATAL = 4,
};

#define GLOBAL_LOG_LEVEL LOG_LEVEL_INFO

//#define HAVE_SYSLOG_H

#ifdef HAVE_SYSLOG_H
#include <libubox/ulog.h>

#define aw_logi(format, ...) ULOG_INFO("[%s:%s:%d] " format, __FILE__, __func__, __LINE__, ##__VA_ARGS__)
#define aw_logd(format, ...) ULOG_NOTE("[%s:%s:%d] " format, __FILE__, __func__, __LINE__, ##__VA_ARGS__)
#define aw_logw(format, ...) ULOG_WARN("[%s:%s:%d] " format, __FILE__, __func__, __LINE__, ##__VA_ARGS__)
#define aw_log_fatal(format, ...)                                                 \
    do                                                                               \
    {                                                                                \
        ULOG_ERR("[%s:%s:%d] " format, __FILE__, __func__, __LINE__, ##__VA_ARGS__); \
        abort();                                                                     \
    } while (0)
#else
#define aw_logi(format, ...) fprintf(stdout, "INFO[%s:%s:%d] " format, __FILE__, __func__, __LINE__, ##__VA_ARGS__)
#define aw_logd(format, ...) fprintf(stdout, "DEBUG[%s:%s:%d] " format, __FILE__, __func__, __LINE__, ##__VA_ARGS__)
#define aw_logw(format, ...) fprintf(stderr, "WARNING[%s:%s:%d] " format, __FILE__, __func__, __LINE__, ##__VA_ARGS__)
#define aw_log_fatal(format, ...)                                                                   \
    do{fprintf(stderr, "FATAL[%s:%s:%d] " format, __FILE__, __func__, __LINE__, ##__VA_ARGS__);abort();}while (0)
#endif

#define NETWORKD_LOG(level, fmt, arg...) \
    do { \
        if (GLOBAL_LOG_LEVEL <= level) \
        { \
            switch (level) \
            { \
            case LOG_LEVEL_INFO: \
                aw_logi(fmt, ##arg); \
                break; \
            case LOG_LEVEL_DEBUG: \
                aw_logd(fmt, ##arg); \
                break; \
            case LOG_LEVEL_WARN: \
                aw_logw(fmt, ##arg); \
                break; \
            case LOG_LEVEL_FATAL: \
                aw_log_fatal(fmt, ##arg); \
                break; \
            default: \
                break; \
            } \
        }} while (0)

//standard output stream
#define LOG_STDOUT(format, ...) fprintf(stdout, "INFO[%s:%s:%d] " format, __FILE__, __func__, __LINE__, ##__VA_ARGS__)

#if 0
#define LOGI(fmt, ...) NETWORKD_LOG(LOG_LEVEL_INFO, fmt, ##__VA_ARGS__)
#define LOGD(fmt, ...) NETWORKD_LOG(LOG_LEVEL_DEBUG, fmt, ##__VA_ARGS__)
#define LOGW(fmt, ...) NETWORKD_LOG(LOG_LEVEL_WARN, fmt, ##__VA_ARGS__)
#define LOGE(fmt, ...) NETWORKD_LOG(LOG_LEVEL_FATAL, fmt, ##__VA_ARGS__)
#endif

#endif
