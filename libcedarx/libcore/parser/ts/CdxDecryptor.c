/*================================================================
*   Copyright (C) 2021 Allwinner Technology Co. Ltd.
*   All rights reserved.
*
*   File:		CdxDecryptor.c
*   Author:		Gan Qiuye(ganqiuye@allwinnertech.com)
*   Date:		2021-02-25  10:22
*   Version:		1.0
*   Description: for now, default for sample-aes.
*                if someday use other encryption algorithm,
*                such as KEY_SAMPLE_SM4, please fix it!
*
================================================================*/
#define LOG_TAG "tsParser_decryptor"
#include <cdx_log.h>
#include "CdxDecryptor.h"
#include <memory.h>
#include <string.h>
#include <stdlib.h>

#define min(x, y)   ((x) <= (y) ? (x) : (y));

#define VIDEO_CLEAR_LEAD  32

#define AUDIO_CLEAR_LEAD  16

#define SHOW_KEY 0

#if SHOW_KEY
static void aesBlockToStr(uint8_t block[AES_BLOCK_SIZE], char* result) {

    if (block != NULL) {
        sprintf(result, "0x%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X",
            block[0], block[1], block[2], block[3], block[4], block[5], block[6], block[7],
            block[8], block[9], block[10], block[11], block[12], block[13], block[14], block[15]);
    }
    return;
}
#endif

static void CdxSignalNewSampleKey(CdxDecryptorT * pDecryptor, SampleEncryptInfoT item)
{
#if SHOW_KEY
    char keyRet[AES_BLOCK_SIZE*3] ="0";
    char ivRet[AES_BLOCK_SIZE*3]  ="0";
    aesBlockToStr(item.key, keyRet);
    aesBlockToStr(item.iv, ivRet);
    CDX_LOGD("CdxSignalNewSampleKey===type:%d, key:%s, iv:%s", item.key_type, keyRet, ivRet);
#endif
    uint8_t KeyData[AES_BLOCK_SIZE];
    memcpy(KeyData, item.key, AES_BLOCK_SIZE);
    memcpy(pDecryptor->mAESInitVec, item.iv, AES_BLOCK_SIZE);
    pDecryptor->bValidKeyInfo = (AES_set_decrypt_key(KeyData, 8*AES_BLOCK_SIZE/*128*/, &pDecryptor->mAesKey) == 0);
    if(!pDecryptor->bValidKeyInfo)
    {
        CDX_LOGE("signalNewSampleAesKey: failed to set AES decryption key.");
    }
}

static size_t findNextUnescapeIndex(uint8_t *data, size_t offset, size_t limit){
    for (size_t i = offset; i < limit - 2; i++) {
        //TODO: speed
        if (data[i] == 0x00 && data[i + 1] == 0x00 && data[i + 2] == 0x03) {
            return i;
        }
    }
    return limit;
}

// Unescapes data replacing occurrences of [0, 0, 3] with [0, 0] and returns the new size
static size_t unescapeStream(uint8_t *data, size_t limit){
    size_t scratchEscapePositions[1024];
    size_t index = 0;
    size_t position = 0;

    while (position < limit) {
        position = findNextUnescapeIndex(data, position, limit);
        if (position < limit) {
            scratchEscapePositions[index++] = position;
            position += 3;
        }
    }

    size_t scratchEscapeCount = index;
    size_t escapedPosition = 0; // The position being read from.
    size_t unescapedPosition = 0; // The position being written to.
    for (size_t i = 0; i < scratchEscapeCount; i++) {
        size_t nextEscapePosition = scratchEscapePositions[i];
        //TODO: add 2 and get rid of the later = 0 assignments
        size_t copyLength = nextEscapePosition - escapedPosition;
        memmove(data+unescapedPosition, data+escapedPosition, copyLength);
        unescapedPosition += copyLength;
        data[unescapedPosition++] = 0;
        data[unescapedPosition++] = 0;
        escapedPosition += copyLength + 3;
    }

    size_t unescapedLength = limit - scratchEscapeCount;
    size_t remainingLength = unescapedLength - unescapedPosition;
    memmove(data+unescapedPosition, data+escapedPosition, remainingLength);

    return unescapedLength;
}

static int  decryptBlock(uint8_t *buffer, size_t size, const AES_KEY* key,
        uint8_t AESInitVec[AES_BLOCK_SIZE]) {
    if (size == 0) {
        return 0;
    }

    if ((size % AES_BLOCK_SIZE) != 0) {
        CDX_LOGE("decryptBlock: size (%zu) not a multiple of block size", size);
        return -1;
    }

    CDX_LOGV("decryptBlock: %p (%zu)", buffer, size);

    AES_cbc_encrypt(buffer, buffer, size, key, AESInitVec, AES_DECRYPT);

    return 0;
}

static cdx_int32 CdxProcessNal(CdxDecryptorT* pDecryptor, cdx_uint8* nalData, cdx_int32 nalSize)
{
    unsigned nalType = nalData[0] & 0x1f;
    if (!pDecryptor->bValidKeyInfo) {
        CDX_LOGE("processNal[%d]: (%p)/%zu Skipping due to invalid key", nalType, nalData, nalSize);
        return nalSize;
    }

    cdx_bool isEncrypted = (nalSize > VIDEO_CLEAR_LEAD + AES_BLOCK_SIZE);
    CDX_LOGV("processNal[%d]: (%p)/%zu isEncrypted: %d", nalType, nalData, nalSize, isEncrypted);

    if (isEncrypted) {
        // Encrypted NALUs have extra start code emulation prevention that must be
        // stripped out before we can decrypt it.
        size_t newSize = unescapeStream(nalData, nalSize);

        CDX_LOGV("processNal:unescapeStream[%d]: %zu -> %zu", nalType, nalSize, newSize);
        nalSize = newSize;

        //Encrypted_nal_unit () {
        //    nal_unit_type_byte                // 1 byte
        //    unencrypted_leader                // 31 bytes
        //    while (bytes_remaining() > 0) {
        //        if (bytes_remaining() > 16) {
        //            encrypted_block           // 16 bytes
        //        }
        //        unencrypted_block           // MIN(144, bytes_remaining()) bytes
        //    }
        //}

        size_t offset = VIDEO_CLEAR_LEAD;
        size_t remainingBytes = nalSize - VIDEO_CLEAR_LEAD;

        // a copy of initVec as decryptBlock updates it
        unsigned char AESInitVec[AES_BLOCK_SIZE];
        memcpy(AESInitVec, pDecryptor->mAESInitVec, AES_BLOCK_SIZE);

        while (remainingBytes > 0) {
            // encrypted_block: protected block uses 10% skip encryption
            if (remainingBytes > AES_BLOCK_SIZE) {
                uint8_t *encrypted = nalData + offset;
                int ret = decryptBlock(encrypted, AES_BLOCK_SIZE, &pDecryptor->mAesKey, AESInitVec);
                if (ret != 0) {
                    CDX_LOGE("processNal failed with %d", ret);
                    return nalSize; // revisit this
                }

                offset += AES_BLOCK_SIZE;
                remainingBytes -= AES_BLOCK_SIZE;
            }

            // unencrypted_block
            size_t clearBytes = min(remainingBytes, (size_t)(9 * AES_BLOCK_SIZE));

            offset += clearBytes;
            remainingBytes -= clearBytes;
        } // while

    } else { // isEncrypted == false
        CDX_LOGD("processNal[%d]: Unencrypted NALU  (%p)/%zu", nalType, nalData, nalSize);
    }

    return nalSize;
}

static void CdxProcessAAC(CdxDecryptorT* pDecryptor, cdx_int32 adtsHdrSize, cdx_uint8* data, cdx_int32 size)
{
   if (!pDecryptor->bValidKeyInfo) {
        CDX_LOGE("processAAC: (%p)/%zu Skipping due to invalid key", data, size);
        return;
    }

    // ADTS header is included in the size
    if (size < adtsHdrSize) {
        CDX_LOGE("processAAC: size (%zu) < adtsHdrSize (%zu)", size, adtsHdrSize);
        return;
    }
    size_t offset = adtsHdrSize;
    size_t remainingBytes = size - adtsHdrSize;

    cdx_bool isEncrypted = (remainingBytes >= AUDIO_CLEAR_LEAD + AES_BLOCK_SIZE);
    CDX_LOGV("processAAC: header: %zu data: %p(%zu) isEncrypted: %d",
          adtsHdrSize, data, size, isEncrypted);

    // with lead bytes
    if (remainingBytes >= AUDIO_CLEAR_LEAD) {
        offset += AUDIO_CLEAR_LEAD;
        remainingBytes -= AUDIO_CLEAR_LEAD;

        // encrypted_block
        if (remainingBytes >= AES_BLOCK_SIZE) {

            size_t encryptedBytes = (remainingBytes / AES_BLOCK_SIZE) * AES_BLOCK_SIZE;
            unsigned char AESInitVec[AES_BLOCK_SIZE];
            memcpy(AESInitVec, pDecryptor->mAESInitVec, AES_BLOCK_SIZE);
            // decrypting all blocks at once
            uint8_t *encrypted = data + offset;
            int ret = decryptBlock(encrypted, encryptedBytes, &pDecryptor->mAesKey, AESInitVec);
            if (ret != 0) {
                CDX_LOGE("processAAC: decryptBlock failed with %d", ret);
                return;
            }

            offset += encryptedBytes;
            remainingBytes -= encryptedBytes;
        } // encrypted

        // unencrypted_trailer
        size_t clearBytes = remainingBytes;
        if (clearBytes > 0) {
           // CHECK(clearBytes < AES_BLOCK_SIZE);
        }

    } else { // without lead bytes
        CDX_LOGV("processAAC: Unencrypted frame (without lead bytes) size %zu = %zu (hdr) + %zu (rem)",
              size, adtsHdrSize, remainingBytes);
    }
}

static void CdxProcessAC3(CdxDecryptorT* pDecryptor, cdx_uint8* data, cdx_int32 size)
{
   if (!pDecryptor->bValidKeyInfo) {
        CDX_LOGD("processAC3: (%p)/%zu Skipping due to invalid key", data, size);
        return;
    }

    cdx_bool isEncrypted = (size >= AUDIO_CLEAR_LEAD + AES_BLOCK_SIZE);
    CDX_LOGV("processAC3 %p(%zu) isEncrypted: %d", data, size, isEncrypted);

    //Encrypted_AC3_Frame () {
    //    unencrypted_leader                 // 16 bytes
    //    while (bytes_remaining() >= 16) {
    //        encrypted_block                // 16 bytes
    //    }
    //    unencrypted_trailer                // 0-15 bytes
    //}

    if (size >= AUDIO_CLEAR_LEAD) {
        // unencrypted_leader
        size_t offset = AUDIO_CLEAR_LEAD;
        size_t remainingBytes = size - AUDIO_CLEAR_LEAD;

        if (remainingBytes >= AES_BLOCK_SIZE) {

            size_t encryptedBytes = (remainingBytes / AES_BLOCK_SIZE) * AES_BLOCK_SIZE;

            // encrypted_block
            unsigned char AESInitVec[AES_BLOCK_SIZE];
            memcpy(AESInitVec, pDecryptor->mAESInitVec, AES_BLOCK_SIZE);

            // decrypting all blocks at once
            uint8_t *encrypted = data + offset;
            int ret = decryptBlock(encrypted, encryptedBytes, &pDecryptor->mAesKey, AESInitVec);
            if (ret != 0) {
                CDX_LOGE("processAC3: decryptBlock failed with %d", ret);
                return;
            }

            offset += encryptedBytes;
            remainingBytes -= encryptedBytes;
        } // encrypted

        // unencrypted_trailer
        size_t clearBytes = remainingBytes;
        if (clearBytes > 0) {
            //CHECK(clearBytes < AES_BLOCK_SIZE);
        }

    } else {
        CDX_LOGD("processAC3: Unencrypted frame (without lead bytes) size %zu", size);
    }
}

CdxDecryptorT* CreateDecryptor(SampleEncryptInfoT mKeyItem)
{
    CdxDecryptorT* pDecryptor = (CdxDecryptorT *)malloc(sizeof(CdxDecryptorT));
    if(pDecryptor == NULL)
        return NULL;
    memset(pDecryptor, 0, sizeof(CdxDecryptorT));
    CdxSignalNewSampleKey(pDecryptor, mKeyItem);
    pDecryptor->mEncryptedType = mKeyItem.key_type;
    pDecryptor->signalNewSampleKey = CdxSignalNewSampleKey;
    pDecryptor->processNal = CdxProcessNal;
    pDecryptor->processAAC = CdxProcessAAC;
    pDecryptor->processAC3 = CdxProcessAC3;
    return pDecryptor;
}

cdx_int32 DestroyDecryptor(CdxDecryptorT** pDecryptor)
{
    CdxDecryptorT* pTor = *pDecryptor;
    free(pTor);
    return 0;
}