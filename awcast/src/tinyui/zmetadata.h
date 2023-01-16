#ifndef Z_METADATA_H
#define Z_METADATA_H

typedef struct ZMetadataS ZMetadataT;

#ifdef __cplusplus
extern "C"
{
#endif

ZMetadataT *ZMD_Instance();

int ZMD_Add(ZMetadataT *md, const char *key);

int ZMD_Set(ZMetadataT *md, const char *key, const char *value);

char *ZMD_Get(ZMetadataT *md, const char *key);

int ZMD_Destroy(ZMetadataT *md);

#ifdef __cplusplus
}
#endif

#endif

