#include <stdio.h>
#include "stdint.h"
#include "stdbool.h"
#include "stddef.h"
#include <pthread.h>
#include <sys/prctl.h>
#include <sys/time.h>
#include <unistd.h>
#include "app_ipcam_ai.h"
#include "app_ipcam_audio.h"
#include "app_ipcam_comm.h"
#include "cvi_audio.h"
#include "cvi_audio_aac_adp.h"
#include "acodec.h"
#include "app_ipcam_gpio.h"
#include "app_ipcam_rtsp.h"
#include "app_ipcam_ll.h"
/**************************************************************************
 *                              M A C R O S                               *
 **************************************************************************/
#define CVI_AUDIO_BLOCK_MODE -1
#define AI_MODEL_SECOND 3
#define AI_ORDER_PAUSE_TIME_MS (1500*1000)
#define AI_NO_ORDER_PAUSE_TIME_MS (100*1000)
/**************************************************************************
 *                           C O N S T A N T S                            *
 **************************************************************************/
static const char *g_stEnumCryStr[] = {"no_baby_cry", "baby_cry"};
static const char *g_stEnumOrderStr[] = {"无指令", "小晶小晶", "拨打电话", "关闭屏幕"};

/**************************************************************************
 *                          D A T A    T Y P E S                          *
 **************************************************************************/

/**************************************************************************
 *                         G L O B A L    D A T A                         *
 **************************************************************************/
static APP_PARAM_AI_CRY_CFG_S g_stCryCfg;
static APP_PARAM_AI_CRY_CFG_S *g_pstCryCfg = &g_stCryCfg;
static volatile bool g_bCryRunning = CVI_FALSE;
static volatile bool g_bCryPause = CVI_FALSE;
static pthread_t g_CryThreadHandle;
static cvitdl_handle_t g_CryAiHandle = NULL;

/**************************************************************************
 *                 E X T E R N A L    R E F E R E N C E S                 *
 **************************************************************************/

/**************************************************************************
 *               F U N C T I O N    D E C L A R A T I O N S               *
 **************************************************************************/
APP_PARAM_AI_CRY_CFG_S *app_ipcam_Ai_Cry_Param_Get(void)
{
    return g_pstCryCfg;
}

CVI_VOID app_ipcam_Ai_Cry_ProcStatus_Set(CVI_BOOL flag)
{
    g_bCryRunning = flag;
}

CVI_BOOL app_ipcam_Ai_Cry_ProcStatus_Get(void)
{
    return g_bCryRunning;
}

CVI_VOID app_ipcam_Ai_Cry_Pause_Set(CVI_BOOL flag)
{
    g_bCryPause = flag;
}

CVI_BOOL app_ipcam_Ai_Cry_Pause_Get(void)
{
    return g_bCryPause;
}

static CVI_S32 app_ipcam_Ai_Cry_Check_Audio_Param(void)
{
    APP_PARAM_AUDIO_CFG_T *pstAudioCfg = app_ipcam_Audio_Param_Get();

    switch(g_pstCryCfg->application_scene)
    {
        case BABY_CRY:
        {
            if(pstAudioCfg->astAudioCfg.enSamplerate != AUDIO_SAMPLE_RATE_16000)
            {
                APP_PROF_LOG_PRINT(LEVEL_ERROR, "AI Cry model only support 16k samplerate !\n");
                return CVI_FAILURE;
            }
            break;
        }
        case AUDIO_ORDER:
        {
            if(pstAudioCfg->astAudioCfg.enSamplerate != AUDIO_SAMPLE_RATE_8000)
            {
                APP_PROF_LOG_PRINT(LEVEL_ERROR, "AI Audio Order model only support 8k samplerate !\n");
                return CVI_FAILURE;
            }
            break; 
        }
        default:
        {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "Unknown Application Scene!\n");
            return CVI_FAILURE;
        }            
    }

    return CVI_SUCCESS;
}

static CVI_VOID *Thread_Cry_PROC(CVI_VOID *pArgs)
{
    APP_PARAM_AUDIO_CFG_T *pstAudioCfg = app_ipcam_Audio_Param_Get();
    AUDIO_SAMPLE_RATE_E u32SampleRate = pstAudioCfg->astAudioCfg.enSamplerate;

    //baby cry: 3 second, audio order: 2 second
    CVI_U32 second = AI_MODEL_SECOND - g_pstCryCfg->application_scene; 
    prctl(PR_SET_NAME, "Thread_Ai_Cry_Proc", 0, 0, 0);
    APP_PROF_LOG_PRINT(LEVEL_INFO, "AI Cry start running!\n");
    CVI_S32 s32Ret = CVI_SUCCESS;
    CVI_U32 u32BufferSize = u32SampleRate * (CVI_U32)(pstAudioCfg->astAudioCfg.enBitwidth + 1) * second;
    // classify the sound result
    CVI_S32 index = -1;
    // Set audio buffer, 3 or 2 seconds
    CVI_U8 buffer[u32BufferSize];  
    memset(buffer, 0, u32BufferSize);
    VIDEO_FRAME_INFO_S Frame;
    memset(&Frame, 0, sizeof(VIDEO_FRAME_INFO_S));
    Frame.stVFrame.pu8VirAddr[0] = buffer;  // Global buffer
    Frame.stVFrame.u32Height = 1;
    Frame.stVFrame.u32Width = u32BufferSize;

    while (app_ipcam_Ai_Cry_ProcStatus_Get()) {
        if (app_ipcam_Ai_Cry_Pause_Get()) {
            usleep(1000*1000);
            continue;
        }

        s32Ret = app_ipcam_Ai_Cry_Audio_Buffer_Get(buffer, u32BufferSize);  // Get audio frame
        if (s32Ret != CVI_SUCCESS) {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "app_ipcam_Ai_Cry_Audio_Buffer_Get Failed\n");
            usleep(1000*1000);
            continue;
        }

        s32Ret = CVI_TDL_SoundClassification_V2(g_CryAiHandle, &Frame, &index);  // Detect the audio
        if((g_pstCryCfg->application_scene))  //AUDIO ORDER
        {
            if (s32Ret == CVI_SUCCESS)
                printf("esc class: %s\n", g_stEnumOrderStr[index]);

            if(index > 0)
                //If it's key word, sleep more time
                usleep(AI_ORDER_PAUSE_TIME_MS);
            else
                usleep(AI_NO_ORDER_PAUSE_TIME_MS);
        }
        else  //BABY CRY
        {
            if (s32Ret == CVI_SUCCESS) 
                printf("esc class: %s\n", g_stEnumCryStr[index]);
            usleep(AI_NO_ORDER_PAUSE_TIME_MS);
        }

    }
    pthread_exit(NULL);

    return NULL;
}

static CVI_S32 app_ipcam_Ai_Cry_Proc_Init(CVI_VOID)
{
    APP_PROF_LOG_PRINT(LEVEL_INFO, "AI Cry init ------------------> start \n");

    CVI_S32 s32Ret = CVI_SUCCESS;

    if (g_CryAiHandle == NULL)
    {
        s32Ret = CVI_TDL_CreateHandle(&g_CryAiHandle);
        if (s32Ret != CVI_SUCCESS)
        {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_TDL_CreateHandle failed with %#x!\n", s32Ret);
            return s32Ret;
        }
    } 
    else
    {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_TDL_CreateHandle has created\n");
        return s32Ret;
    }

    s32Ret = CVI_TDL_SetPerfEvalInterval(g_CryAiHandle, CVI_TDL_SUPPORTED_MODEL_SOUNDCLASSIFICATION_V2, 10);
    if (s32Ret != CVI_SUCCESS)
    {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_TDL_SetPerfEvalInterval failed with %#x!\n", s32Ret);
        return s32Ret;
    }

    s32Ret = CVI_TDL_OpenModel(g_CryAiHandle, CVI_TDL_SUPPORTED_MODEL_SOUNDCLASSIFICATION_V2, g_pstCryCfg->model_path);
    if (s32Ret != CVI_SUCCESS)
    {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_TDL_SetModelPath failed with %#x! maybe reset model path\n", s32Ret);
        return s32Ret;
    }

    APP_PROF_LOG_PRINT(LEVEL_INFO, "AI Cry init ------------------> done \n");

    return CVI_SUCCESS;
}

int app_ipcam_Ai_Cry_Stop(void)
{
    CVI_S32 s32Ret = CVI_SUCCESS;

    if (!g_pstCryCfg->bEnable)
    {
        APP_PROF_LOG_PRINT(LEVEL_WARN, "AI Cry not enable\n");
        return CVI_SUCCESS;
    }

    if (!app_ipcam_Ai_Cry_ProcStatus_Get())
    {
        APP_PROF_LOG_PRINT(LEVEL_WARN, "AI Cry has not running!\n");
        return s32Ret;
    }

    app_ipcam_Ai_Cry_ProcStatus_Set(CVI_FALSE);

    CVI_S32 iTime = GetCurTimeInMsec();

    if (g_CryThreadHandle)
    {
        pthread_join(g_CryThreadHandle, NULL);
        g_CryThreadHandle = 0;
    }

    s32Ret = CVI_TDL_DestroyHandle(g_CryAiHandle);
    if (s32Ret != CVI_SUCCESS)
    {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_TDL_DestroyHandle failed with 0x%x!\n", s32Ret);
        return s32Ret;
    }
    else
    {
        g_CryAiHandle = NULL;
    }

    APP_PROF_LOG_PRINT(LEVEL_INFO, "AI Cry Thread exit takes %u ms\n", (GetCurTimeInMsec() - iTime));

    return CVI_SUCCESS;
}

int app_ipcam_Ai_Cry_Start(void){
    CVI_S32 s32Ret = CVI_SUCCESS;

    if (!g_pstCryCfg->bEnable)
    {
        APP_PROF_LOG_PRINT(LEVEL_WARN, "AI Cry not enable\n");
        return CVI_SUCCESS;
    }

    if (g_bCryRunning)
    {
        APP_PROF_LOG_PRINT(LEVEL_WARN, "AI Cry has started\n");
        return CVI_SUCCESS;
    }

    s32Ret = app_ipcam_Ai_Cry_Check_Audio_Param();
    if (s32Ret != CVI_SUCCESS)
    {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "app_ipcam_Ai_Cry_Check_Audio_Param failed!\n");
        return s32Ret;
    }

    s32Ret = app_ipcam_Ai_Cry_Proc_Init();
    if (s32Ret != CVI_SUCCESS)
    {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "app_ipcam_Ai_Cry_Proc_Init failed!\n");
        return s32Ret;
    }

    app_ipcam_Ai_Cry_ProcStatus_Set(CVI_TRUE);

    s32Ret = pthread_create(&g_CryThreadHandle, NULL, Thread_Cry_PROC, NULL);
    if (s32Ret != CVI_SUCCESS)
    {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "AI Cry_pthread_create failed!\n");
        return s32Ret;
    }

    return s32Ret;
}

CVI_S32 app_ipcam_Ai_Cry_StatusGet(void)
{
    return g_CryAiHandle ? 1 : 0;
}