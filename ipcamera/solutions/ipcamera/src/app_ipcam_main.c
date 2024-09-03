#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

#include "app_ipcam_paramparse.h"
#include "app_ipcam_ircut.h"

#ifdef WEB_SOCKET
#include "app_ipcam_websocket.h"
#include "app_ipcam_netctrl.h"
#endif

#ifdef CVI_UVC_SUPPORT
#include "cvi_uvc.h"
#endif

#ifdef CVI_UAC_SUPPORT
#include "cvi_audio_uac.h"
#endif


/**************************************************************************
 *                              M A C R O S                               *
 **************************************************************************/

/**************************************************************************
 *                           C O N S T A N T S                            *
 **************************************************************************/

/**************************************************************************
 *                          D A T A    T Y P E S                          *
 **************************************************************************/

/**************************************************************************
 *                         G L O B A L    D A T A                         *
 **************************************************************************/
static pthread_t g_pthMisc;
static CVI_BOOL g_bMisc;

/**************************************************************************
 *                 E X T E R N A L    R E F E R E N C E S                 *
 **************************************************************************/

/**************************************************************************
 *               F U N C T I O N    D E C L A R A T I O N S               *
 **************************************************************************/
static int app_ipcam_Exit(void);

static CVI_VOID app_ipcam_ExitSig_handle(CVI_S32 signo)
{
    signal(SIGINT, SIG_IGN);
    signal(SIGTERM, SIG_IGN);
    
    if ((SIGINT == signo) || (SIGTERM == signo)) {
        app_ipcam_Exit();
        APP_PROF_LOG_PRINT(LEVEL_INFO, "ipcam receive a signal(%d) from terminate\n", signo);
    }

    exit(-1);
}

static CVI_VOID app_ipcam_Usr1Sig_handle(CVI_S32 signo)
{    
    if (SIGUSR1 == signo) {
        APP_PROF_LOG_PRINT(LEVEL_INFO, "ipcam receive a signal(%d) from terminate and start trigger a picture\n", signo);
        app_ipcam_JpgCapFlag_Set(CVI_TRUE);
    }
}

static int app_ipcam_Peripheral_Init(void)
{
    /* do peripheral Init here */

    app_ipcam_IRCut_Init();

    return CVI_SUCCESS;
}

/* 
* this thread handle a series of small tasks include
* a. send AI framerate to Web-client
* b. 
*/
static void *ThreadMisc(void *arg)
{
    while (g_bMisc) {
        #ifdef WEB_SOCKET
        app_ipcam_WebSocket_AiFps_Send();
        #endif
        sleep(1);
    }

    return NULL;
}

static int app_ipcam_MiscThread_Init(void)
{
    int s32Ret = CVI_SUCCESS;

    g_bMisc = CVI_TRUE;
    s32Ret = pthread_create(&g_pthMisc, NULL, ThreadMisc, NULL);
    if (s32Ret != 0) {
        printf("pthread_create failed!\n");
        return s32Ret;
    }

    return CVI_SUCCESS;
}

static int app_ipcam_MiscThread_DeInit(void)
{
    g_bMisc = CVI_FALSE;

    if (g_pthMisc) {
        pthread_cancel(g_pthMisc);
        pthread_join(g_pthMisc, NULL);
        g_pthMisc = 0;
    }

    return CVI_SUCCESS;
}

static int app_ipcam_Exit(void)
{
    APP_CHK_RET(app_ipcam_MiscThread_DeInit(), "MiscThread DeInit");

    #ifdef CVI_UVC_SUPPORT
    app_uvc_exit();
    #endif
    #ifdef CVI_UAC_SUPPORT
    app_uac_exit();
    #endif

    #ifdef RECORD_SUPPORT
    APP_CHK_RET(app_ipcam_Record_UnInit(), "SD Record UnInit");
    #endif

    APP_CHK_RET(app_ipcam_Osdc_DeInit(), "OSDC DeInit");

    #ifdef AI_SUPPORT

    #if defined AUDIO_SUPPORT && defined AI_BABYCRY_SUPPORT
    APP_CHK_RET(app_ipcam_Ai_Cry_Stop(), "AI Cry Stop");
    #endif

    #ifdef CONSUMER_COUNTING_SUPPORT
    APP_CHK_RET(app_ipcam_Ai_Consumer_Counting_Stop(), "Consumer Counting Stop");
    #endif

    #ifdef HAND_DETECT_SUPPORT
    APP_CHK_RET(app_ipcam_Ai_HD_Stop(), "HD Stop");
    #endif

    #ifdef PD_SUPPORT
    APP_CHK_RET(app_ipcam_Ai_PD_Stop(), "PD Stop");
    #endif

    #ifdef FACE_SUPPORT
    APP_CHK_RET(app_ipcam_Ai_FD_Stop(), "FD Stop");
    #endif
    #endif

    #ifdef MD_SUPPORT
    APP_CHK_RET(app_ipcam_MD_Stop(), "MD Stop");
    #endif

    #ifdef AUDIO_SUPPORT
    APP_CHK_RET(app_ipcam_Audio_UnInit(), "Audio UnInit");
    #endif

    #ifdef VDEC
    APP_CHK_RET(app_ipcam_Vdec_Stop(), "VDEC Stop");
    #endif

    #ifdef DISP_FRMBUF
    APP_CHK_RET(app_ipcam_Disp_FrmBuf_Stop(), "DISP FRMBUF Stop");
    #endif
    
    #ifdef VDEC_SOFT
    APP_CHK_RET(app_ipcam_Vdec_Soft_Stop(APP_VDEC_SOFT_ALL), "VDEC SOFT Stop");
    APP_CHK_RET(app_ipcam_Vdec_Soft_DeInit(), "VDEC SOFT DeInit");
    #endif

    #ifdef FRMBUF
    APP_CHK_RET(app_ipcam_FrmBuf_DeInit(), "FrmBuf DeInit");
    #endif

    #ifdef DISPLAY
    APP_CHK_RET(app_ipcam_Display_Exit(), "Display Exit");
    #endif

    #ifdef STITCH_SUPPORT
    APP_CHK_RET(app_ipcam_Stitch_UnInit(), "Stitch UnInit");
    #endif

    #ifdef DWA_SUPPORT
    APP_CHK_RET(app_ipcam_Dwa_DeInit(), "DWA DeInit");
    #endif

    APP_CHK_RET(app_ipcam_Vpss_DeInit(), "VPSS DeInit");
    APP_CHK_RET(app_ipcam_Venc_Stop(APP_VENC_ALL), "VENC Stop");
    APP_CHK_RET(app_ipcam_Vi_DeInit(), "VI DeInit");
    APP_CHK_RET(app_ipcam_Sys_DeInit(), "System DeInit");
#ifdef RTSP_SUPPORT
    APP_CHK_RET(app_ipcam_rtsp_Server_Destroy(), "RTSP Server Destroy");
#endif
    return CVI_SUCCESS;
}

static int app_ipcam_Init(void)
{
    APP_CHK_RET(app_ipcam_Peripheral_Init(), "Init Peripheral");
    APP_CHK_RET(app_ipcam_Sys_Init(), "Init Systerm");
    APP_CHK_RET(app_ipcam_Vi_Init(), "Init VI");
    APP_CHK_RET(app_ipcam_Vpss_Init(), "Init VPSS");
    #ifdef DWA_SUPPORT
    APP_CHK_RET(app_ipcam_Ldc_Init(), "Init LDC");
    APP_CHK_RET(app_ipcam_Dwa_Init(), "Init DWA");
    #endif
    APP_CHK_RET(app_ipcam_Osdc_Init(), "Init Draw Osdc");

    #ifdef STITCH_SUPPORT
    APP_CHK_RET(app_ipcam_Stitch_Init(), "Init Stitch");
    #endif

    #ifdef WEB_SOCKET
    APP_CHK_RET(app_ipcam_NetCtrl_Init(), "Init Net Ctrl");
    APP_CHK_RET(app_ipcam_WebSocket_Init(), "Init Websocket");
    #endif

    APP_CHK_RET(app_ipcam_Venc_Init(APP_VENC_ALL), "Init VENC");

    #ifdef VDEC
    APP_CHK_RET(app_ipcam_Vdec_Init(), "Init VDEC");
    #endif

    #ifdef VDEC_SOFT
    APP_CHK_RET(app_ipcam_Vdec_Soft_Init(), "Init VDEC SOFT");
    #endif

    #ifdef AUDIO_SUPPORT
    APP_CHK_RET(app_ipcam_Audio_Init(), "Init Audio");
    #endif

    #ifdef CVI_UVC_SUPPORT
    app_uvc_init();
    #endif

    #ifdef CVI_UAC_SUPPORT
    app_uac_init();
    #endif

    APP_CHK_RET(app_ipcam_MiscThread_Init(), "Init Misc");

    return CVI_SUCCESS;
}

int main(int argc, char *argv[])
{
    system("echo /mnt/nfs/core-%e-%p-%t > /proc/sys/kernel/core_pattern");
    APP_CHK_RET(app_ipcam_Opts_Parse(argc, argv), "Parse Optinos");

    signal(SIGINT, app_ipcam_ExitSig_handle);
    signal(SIGTERM, app_ipcam_ExitSig_handle);
    signal(SIGUSR1, app_ipcam_Usr1Sig_handle);

    /* load each moudles parameter from param_config.ini */
    APP_CHK_RET(app_ipcam_Param_Load(), "Load Global Parameter");

    /* init modules include <Peripheral; Sys; VI; VB; OSD; Venc; AI; Audio; etc.> */
    APP_CHK_RET(app_ipcam_Init(), "Init Ipcam App");

    /* create rtsp server */
#ifdef RTSP_SUPPORT
    APP_CHK_RET(app_ipcam_Rtsp_Server_Create(), "Create RTSP Server");
#endif

    /* start video encode */
    APP_CHK_RET(app_ipcam_Venc_Start(APP_VENC_ALL), "Start VENC");

    #ifdef DISPLAY
    APP_CHK_RET(app_ipcam_Display_Init(), "Init Display");
    #endif

    #ifdef FRMBUF
    APP_CHK_RET(app_ipcam_FrmBuf_Init(), "Init FrameBuffer");
    #endif

    /* start video decode */
    #ifdef VDEC
    APP_CHK_RET(app_ipcam_Vdec_Start(), "Start VDEC");
    #endif

    #ifdef VDEC_SOFT
    APP_CHK_RET(app_ipcam_Vdec_Soft_Start(APP_VDEC_SOFT_ALL), "Start VDEC SOFT");
    #endif

    #ifdef DISP_FRMBUF
    APP_CHK_RET(app_ipcam_Disp_FrmBuf_Start(), "Start DISP FRMBUF");
    #endif

    #ifdef MD_SUPPORT
    /* start MD (Motion Detection)*/
    APP_CHK_RET(app_ipcam_MD_Start(), "running MD");
    #endif

    #ifdef PD_SUPPORT
    /* start AI PD (Pedestrian Detection) */
    APP_CHK_RET(app_ipcam_Ai_PD_Start(), "Start AI PD");
    #endif

    #ifdef HAND_DETECT_SUPPORT
    /* start AI HD (Hand Detection)*/
    APP_CHK_RET(app_ipcam_Ai_HD_Start(), "running AI HD");
    #endif

    #ifdef CONSUMER_COUNTING_SUPPORT
    /* start AI COUNT (Consumer Counting)*/
    APP_CHK_RET(app_ipcam_Ai_Consumer_Counting_Start(), "running AI Consumer Counting");
    #endif

    /* start AI FD (Face Detection)*/
    #ifdef FACE_SUPPORT
    #ifdef IR_FACE_SUPPORT
    APP_CHK_RET(app_ipcam_Ai_IR_FD_Start(), "Start AI IR FD");
    #else
    APP_CHK_RET(app_ipcam_Ai_FD_Start(), "Start AI FD");
    #endif
    #endif

    #if defined AUDIO_SUPPORT && defined AI_BABYCRY_SUPPORT
    APP_CHK_RET(app_ipcam_Ai_Cry_Start(), "Start AI Cry");
    #endif

    #ifdef RECORD_SUPPORT
    /* start SD Record */
    APP_CHK_RET(app_ipcam_Record_Recover_Init(), "Init SD Record Recover");
    APP_CHK_RET(app_ipcam_Record_Init(), "Init SD Record");
    #endif

    /* enable receive a command form another progress for test ipcam */
    // APP_CHK_RET(app_ipcam_CmdTask_Create(), "Create CMD Test");

    while (1) {
        sleep(1);
    };

    return CVI_SUCCESS;
}
