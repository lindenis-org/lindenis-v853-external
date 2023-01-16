/*

Copyright (c) 2008-2019 Allwinner Technology Co. Ltd.. All rights reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

     http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <tina_log.h>
#include "ini_config.h"

typedef struct item_t {
    char key[LINE_CONTENT_MAX_LEN];
    char value[LINE_CONTENT_MAX_LEN];
}ITEM;

/*
 *去除字符串右端空格
 */
static char *strtrimr(char *pstr)
{
    int i;
    i = strlen(pstr) - 1;
    while (isspace(pstr[i]) && (i >= 0))
        pstr[i--] = '\0';
    return pstr;
}
/*
 *去除字符串左端空格
 */
static char *strtriml(char *pstr)
{
    int i = 0,j;
    j = strlen(pstr) - 1;
    while (isspace(pstr[i]) && (i <= j))
        i++;
    if (0<i)
        strcpy(pstr, &pstr[i]);
    return pstr;
}
/*
 *去除字符串两端空格
 */
static char *strtrim(char *pstr)
{
    char *p;
    p = strtrimr(pstr);
    return strtriml(p);
}


/*
 *从配置文件的一行读出key或value,返回item指针
 *line--从配置文件读出的一行
 */
static int  get_item_from_line(char *line, ITEM *item)
{
    char p[LINE_CONTENT_MAX_LEN] = {0};
    int len = 0;

    strcpy(p, line);
    len = strlen(strtrim(p));
    if(len <= 0){
        return 1;//空行
    }else if(p[0]=='#'){
        return 2;//注释
    }else if(p[0]==';'){
        return 2;//注释
    }else{
        char *p2 = strchr(p, '=');
        *p2++ = '\0';

        memset(item->key, 0, LINE_CONTENT_MAX_LEN);
        memset(item->value, 0, LINE_CONTENT_MAX_LEN);

        strcpy(item->key,p);
        strcpy(item->value,p2);
    }
    return 0;//查询成功
}

static int file_to_items(const char *file, ITEM *items, int *num)
{
    char line[1024];
    FILE *fp;
    fp = fopen(file,"r");
    if(fp == NULL){
        TLOGE("err: fopen failed");
        return 1;
    }
    int i = 0;
    while(fgets(line, 1023, fp)){
        char *p = strtrim(line);
        int len = strlen(p);
        if(len <= 0){
            continue;
        }else if(p[0]=='#'){
            continue;
        }else if(p[0]==';'){
            continue;
        }else{
            char *p2 = strchr(p, '=');
            /*这里认为只有key没什么意义*/
            if(p2 == NULL)
                continue;
            *p2++ = '\0';

            memset(items[i].key, 0, LINE_CONTENT_MAX_LEN);
            memset(items[i].value, 0, LINE_CONTENT_MAX_LEN);

            strcpy(items[i].key,p);
            strcpy(items[i].value,p2);

            i++;
        }
    }
    (*num) = i;
    fclose(fp);
    return 0;
}

/*
 *读取value
 */
static int read_conf_value(const char *key, char *value,const char *file)
{
    char line[1024];
    FILE *fp;
    int found = -1;

    fp = fopen(file,"r");
    if(fp == NULL){
        TLOGE("err: fopen failed");
        return 1;//文件打开错误
    }
    while (fgets(line, 1023, fp)){
        ITEM item;
        get_item_from_line(line,&item);
        if(!strcmp(item.key,key)){
            strcpy(value,item.value);

            found = 0;
            goto end;
        }
    }

end:
    fclose(fp);

    return found;//成功

}

static int write_conf_value(const char *key, const char *value, const char *file)
{
    char line[1024] = {0};
    char strWrite[LINE_CONTENT_MAX_LEN] = {0};
    FILE *fp = NULL;
    FILE *fp_tmp = NULL;
    char file_tmp[LINE_CONTENT_MAX_LEN] = {0};
    int FoundKey = 0;
    int ret = 0, err = 0;
    int last_line_len = 0;
    int add_break = 0;

    TLOGI("write_conf_value: key=%s, value=%s, file=%s", key, value, file);

    sprintf(file_tmp, "%s_tmp", file);
    sprintf(strWrite, "%s=%s\n", key, value);
    fp = fopen(file, "r");
    if(fp == NULL){
        TLOGE("err: fopen failed");
        return -1;
    }

    fp_tmp = fopen(file_tmp, "a+");
    if(fp_tmp == NULL){
        TLOGE("err: fopen failed");
        fclose(fp);
        return -1;
    }

    while(fgets(line, 1023, fp)){
        ITEM item;

        if(line[strlen(line) -1] != '\n'){
            add_break = 1;
        }

        get_item_from_line(line, &item);
        if(!strcmp(item.key, key)){
            ret = fputs(strWrite, fp_tmp);
            if(ret < 0){
                TLOGE("err: fputs failed");
                err = -1;
                goto end;
            }

            FoundKey = 1;

        }else{
            ret = fputs(line, fp_tmp);
            if(ret < 0){
                TLOGE("err: fputs failed");
                err = -1;
                goto end;
            }
        }
    }

    if(FoundKey == 0){
        //没有找到，写到文件尾部
        fseek(fp_tmp, 0, SEEK_END);
        if(add_break){
            fputs("\n", fp_tmp);
            fseek(fp_tmp, 0, SEEK_END);
        }

        ret = fputs(strWrite, fp_tmp);
        if(ret < 0){
            TLOGE("err: fputs failed");
            err = -1;
        }
    }

end:
    fclose(fp);
    fclose(fp_tmp);

    if(!err){
        char cmd_line[1024] = {0};
        sprintf(cmd_line, "mv %s %s", file_tmp, file);
        system(cmd_line);
    }else{
        remove(file_tmp);
    }

    return err;
}

int read_string_value(const char *key, char *value,const char *file)
{
    return read_conf_value(key, value, file);
}

int read_int_value(const char *key, int *value,const char *file)
{
    int ret = 0;
    char strValue[32] = {0};

    ret = read_conf_value(key, strValue, file);
    if(ret != 0){
        return -1;
    }

    *value = atoi(strValue);

    return 0;
}

int write_string_value(const char *key, char *value,const char *file)
{
    return write_conf_value(key, value, file);
}

int write_int_value(const char *key, int value,const char *file)
{
    int ret = 0;
    char strValue[32] = {0};

    sprintf(strValue, "%-4d", value);
    ret = write_conf_value(key, strValue, file);
    if(ret != 0){
        return -1;
    }

    return 0;
}

