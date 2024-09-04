#ifndef __APP_DBG_H__
#define __APP_DBG_H__

#include <unistd.h>
#include <inttypes.h>
#include <pthread.h>

/* Make this header file easier to include in C++ code */
#ifdef __cplusplus
extern "C" {
#endif

#define APP_IPCAM_MAX_FILE_LEN 128
#define APP_IPCAM_MAX_STR_LEN 64

#define APP_IPCAM_SUCCESS           ((int)(0))
#define APP_IPCAM_ERR_FAILURE       ((int)(-1001))
#define APP_IPCAM_ERR_NOMEM         ((int)(-1002))
#define APP_IPCAM_ERR_TIMEOUT       ((int)(-1003))
#define APP_IPCAM_ERR_INVALID       ((int)(-1004))


#define APP_CHK_RET(express, name)                                                                              \
    do {                                                                                                        \
        int rc = express;                                                                                       \
        if (rc != CVI_SUCCESS) {                                                                                \
            printf("\033[40;31m%s failed at %s line:%d with %#x!\033[0m\n", name, __FUNCTION__, __LINE__, rc);  \
            return rc;                                                                                          \
        }                                                                                                       \
    } while (0)

#define APP_IPCAM_LOGE(...)   printf(__VA_ARGS__)
#define APP_IPCAM_CHECK_RET(ret, fmt...)                                \
    do {                                                                \
        if (ret != CVI_SUCCESS) {                                       \
            APP_IPCAM_LOGE(fmt);                                        \
            APP_IPCAM_LOGE("fail and return:[%#x]\n", ret);             \
            return ret;                                                 \
        }                                                               \
    } while (0)

#ifndef APP_FUNC_RET_CALLBACK
#define APP_FUNC_RET_CALLBACK(func, onSuccess, notSuccess) {    \
    if (func == CVI_SUCCESS) {                                  \
        onSuccess;                                              \
    } else {                                                    \
        notSuccess;                                             \
    }                                                           \
}
#endif


#ifndef _NULL_POINTER_CHECK_
#define _NULL_POINTER_CHECK_(p, errcode)    \
    do {                                        \
        if (!(p)) {                             \
            printf("pointer[%s] is NULL\n", #p); \
            return errcode;                     \
        }                                       \
    } while (0)
#endif


/*  debug solution  */
/**
 * @brief  :  easy debug for app-development
 * @author :  xulong
 * @since  :  2021-06-17
 * @how to used   :	1. RPrintf("Hello world");
                    2. __FUNC_TRACK__;
                    3. APP_PROF_LOG_PRINT(LEVEL_TRACE, "Hello world argv[0] %s\n", argv[0]);
                    n. ......
* @note1 :  default debug level is LEVEL_INFO
*/
/* level setting */
#define LEVEL_ERROR                 0 /*min print info*/
#define LEVEL_WARN                  1
#define LEVEL_INFO                  2
#define LEVEL_TRACE                 3
#define LEVEL_DEBUG                 4 /*max print info*/

/* color setting */
#define DEBUG_COLOR_NORMAL          "\033[m"
#define DEBUG_COLOR_BLACK           "\033[30m"
#define DEBUG_COLOR_RED             "\033[31m"
#define DEBUG_COLOR_GREEN           "\033[32m"
#define DEBUG_COLOR_YELLOW          "\033[33m"
#define DEBUG_COLOR_BLUE            "\033[34m"
#define DEBUG_COLOR_PURPLE          "\033[35m"
#define DEBUG_COLOR_BKRED           "\033[41;37m"

/* tag setting */
#define STRINGIFY(x, fmt)           #x fmt
#define ADDTRACECTAG(fmt)           STRINGIFY([app][trace][%s %u], fmt)
#define ADDDBGTAG(fmt)              STRINGIFY([app][dbg][%s %u], fmt)
#define ADDTAG(fmt)                 STRINGIFY([app], fmt)
#define ADDWARNTAG(fmt)             STRINGIFY([app][warn][%s %u], fmt)
#define ADDERRTAG(fmt)              STRINGIFY([app][err][%s %u], fmt)

#ifndef DEF_DEBUG_LEVEL
#define DEF_DEBUG_LEVEL            	LEVEL_INFO  //use level
#endif

#define APP_PROF_LOG_PRINT(level, fmt, args...)                                                                     \
        if(level <= DEF_DEBUG_LEVEL) {                                                                              \
            if(level <= LEVEL_WARN) {                                                                               \
                if(level == LEVEL_WARN)                                                                             \
                    printf(DEBUG_COLOR_YELLOW ADDWARNTAG(fmt) DEBUG_COLOR_NORMAL , __FUNCTION__, __LINE__, ##args); \
                else                                                                                                \
                    printf(DEBUG_COLOR_RED ADDERRTAG(fmt) DEBUG_COLOR_NORMAL , __FUNCTION__, __LINE__, ##args);     \
            }                                                                                                       \
            else if(level <= LEVEL_TRACE) {                                                                         \
                if(level == LEVEL_TRACE)                                                                            \
                    printf(DEBUG_COLOR_BLUE ADDWARNTAG(fmt) DEBUG_COLOR_NORMAL , __FUNCTION__, __LINE__, ##args);   \
                else                                                                                                \
                    printf( ADDTAG(fmt) , ##args);                                                                  \
            } else                                                                                                  \
                printf(DEBUG_COLOR_PURPLE ADDDBGTAG(fmt) DEBUG_COLOR_NORMAL , __FUNCTION__, __LINE__, ##args);      \
        }

/* 	END debug API 
*	easy debug for app-development
*/

typedef void *(*pfp_task_entry)(void *param);

typedef struct _RUN_THREAD_PARAM
{
    int bRun_flag;
    pthread_t mRun_PID;
} RUN_THREAD_PARAM;

unsigned int GetCurTimeInMsec(void);
int h264Parse(void *pData, int *ps32ReadLen);
int h265Parse(void *pData, int *ps32ReadLen);
int mjpegParse(void *pData, int *ps32ReadLen, unsigned int *pu32Start);

#ifdef __cplusplus
}
#endif


#endif
