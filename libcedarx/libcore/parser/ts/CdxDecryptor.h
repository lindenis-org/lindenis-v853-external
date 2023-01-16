/*================================================================
*   Copyright (C) 2021 Allwinner Technology Co. Ltd.
*   All rights reserved.
*
*   File:		CdxDecryptor.h
*   Author:		Gan Qiuye(ganqiuye@allwinnertech.com)
*   Date:		2021-02-25  10:22
*   Version:		1.0
*   Description: for now, default for sample-aes.
*                if someday use other encryption algorithm,
*                such as KEY_SAMPLE_SM4, please fix it!
*
================================================================*/
#include <openssl/aes.h>
#include <CdxTypes.h>
#include <CdxParser.h>

typedef struct CdxDecryptorS CdxDecryptorT;

struct CdxDecryptorS
{
    enum KeyType mEncryptedType;
    AES_KEY mAesKey;
    uint8_t mAESInitVec[AES_BLOCK_SIZE];
    cdx_bool bValidKeyInfo;
    //enum PaddingType paddingType;
    void      (*signalNewSampleKey)(CdxDecryptorT * /*pDecryptor*/, SampleEncryptInfoT /*item*/);
    cdx_int32 (*processNal)(CdxDecryptorT*, cdx_uint8* /*nalData*/, cdx_int32 /*nalSize*/);
    void      (*processAAC)(CdxDecryptorT*, cdx_int32 /*adtsHdrSize*/, cdx_uint8* /*data*/, cdx_int32 /*size*/);
    void      (*processAC3)(CdxDecryptorT*, cdx_uint8* /*data*/, cdx_int32 /*size*/);
};


CdxDecryptorT* CreateDecryptor(SampleEncryptInfoT mKeyItem);

cdx_int32 DestroyDecryptor(CdxDecryptorT** pDecryptor);