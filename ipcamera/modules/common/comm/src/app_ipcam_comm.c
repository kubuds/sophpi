#include "app_ipcam_comm.h"
#include <sys/time.h>

unsigned int GetCurTimeInMsec(void)
{
    struct timeval tv;

    if (gettimeofday(&tv, NULL) < 0)
    {
        return 0;
    }

    return tv.tv_sec * 1000 + tv.tv_usec/1000;
}

int h264Parse(void *pData, int *ps32ReadLen)
{
    unsigned char *pu8Buf = (unsigned char *)pData;
    int i = 0;
    int tmp = 0;
    int findStart = 0;
    int findEnd = 0;

    if ((pData == NULL) || (ps32ReadLen == NULL) || (*ps32ReadLen <= 0)) {
        return -1;
    }

    for (i = 0; i < *ps32ReadLen - 8; i++) {
        tmp = pu8Buf[i + 3] & 0x1F;
        if (pu8Buf[i] == 0 && pu8Buf[i + 1] == 0 && pu8Buf[i + 2] == 1
                && (((tmp == 0x5 || tmp == 0x1) && ((pu8Buf[i + 4] & 0x80) == 0x80))
                || (tmp == 20 && (pu8Buf[i + 7] & 0x80) == 0x80))) {
            findStart = 1;
            i += 8;
            break;
        }
    }

    for (; i < *ps32ReadLen - 8; i++) {
        tmp = pu8Buf[i + 3] & 0x1F;
        if (pu8Buf[i] == 0 && pu8Buf[i + 1] == 0 && pu8Buf[i + 2] == 1 &&
            (tmp == 15 || tmp == 7 || tmp == 8 || tmp == 6 ||
                ((tmp == 5 || tmp == 1) && ((pu8Buf[i + 4] & 0x80) == 0x80)) ||
                (tmp == 20 && (pu8Buf[i + 7] & 0x80) == 0x80))) {
            findEnd = 1;
            break;
        }
    }

    if ((findStart == 0) || (findEnd == 0)) {
        return -1;
    }

    if (i > 0) {
        *ps32ReadLen = i;
    }

    return 0;
}

int h265Parse(void *pData, int *ps32ReadLen)
{
    unsigned char *pu8Buf = NULL;
    int i = 0;
    int tmp = 0;
    int bNewPic    = 0;
    int findStart = 0;
	int findEnd   = 0;

    if ((pData == NULL) || (ps32ReadLen == NULL) || (*ps32ReadLen <= 0)) {
        return -1;
    }

    pu8Buf = (unsigned char *)pData;

    for (i = 0; i < *ps32ReadLen - 6; i++) {
        tmp = (pu8Buf[i + 3] & 0x7E) >> 1;

        bNewPic = (pu8Buf[i + 0] == 0 && pu8Buf[i + 1] == 0 && pu8Buf[i + 2] == 1 &&
                (tmp <= 21) && ((pu8Buf[i + 5] & 0x80) == 0x80));

        if (bNewPic) {
            findStart = 1;
            i += 6;
            break;
        }
    }

    for (; i < *ps32ReadLen - 6; i++) {
        tmp = (pu8Buf[i + 3] & 0x7E) >> 1;

        bNewPic = (pu8Buf[i + 0] == 0 && pu8Buf[i + 1] == 0 && pu8Buf[i + 2] == 1 &&
                (tmp == 32 || tmp == 33 || tmp == 34 || tmp == 39 || tmp == 40 ||
                ((tmp <= 21) && (pu8Buf[i + 5] & 0x80) == 0x80)));

        if (bNewPic) {
            findEnd = 1;
            break;
        }
    }
    
    if ((findStart == 0) || (findEnd == 0)) {
        return -1;
    }

    if (i > 0) {
        *ps32ReadLen = i;
    }

    return 0;
}

int mjpegParse(void *pData, int *ps32ReadLen, unsigned int *pu32Start)
{
    int findStart = 0;
    int findEnd = 0;
    unsigned char *pu8Buf = (unsigned char *)pData;
    int i = 0;
    int u32Len = 0;

    if ((pData == NULL) || (ps32ReadLen == NULL) || (*ps32ReadLen <= 0) 
        || (pu32Start == NULL)) {
        return -1;
    }

    for (i = 0; i < *ps32ReadLen - 1; i++) {
    	if (pu8Buf[i] == 0xFF && pu8Buf[i + 1] == 0xD8) {
    		*pu32Start = i;
    		findStart = 1;
    		i = i + 2;
    		break;
    	}
    }
    for (; i < *ps32ReadLen - 3; i++) {
    	if ((pu8Buf[i] == 0xFF) && (pu8Buf[i + 1] & 0xF0) == 0xE0) {
    		u32Len = (pu8Buf[i + 2] << 8) + pu8Buf[i + 3];
    		i += 1 + u32Len;
    	} else {
    		break;
    	}
    }
    for (; i < *ps32ReadLen - 1; i++) {
    	if (pu8Buf[i] == 0xFF && pu8Buf[i + 1] == 0xD9) {
    		findEnd = 1;
    		break;
    	}
    }
    *ps32ReadLen = i + 2;

    if ((findStart == 0) || (findEnd == 0)) {
    	return -1;
    }
    return 0;
}
