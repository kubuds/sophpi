#include <stdio.h>
#include <stdlib.h>
#include <sys/queue.h>
#include <semaphore.h>
#include <signal.h>
#include <linux/cvi_math.h>
#include "app_ipcam_comm.h"
#include "app_ipcam_stitch.h"
#include "cvi_sys.h"
#include "cvi_vb.h"
#include "cvi_buffer.h"
#include "cvi_dwa.h"
#include "cvi_vpss.h"
#include "cvi_stitch.h"

/**************************************************************************
 *                              M A C R O S                               *
 **************************************************************************/
#define DWA_STRIDE_ALIGN 32
#define STITCH_EXT_OP_CLR_VB 0
#define MaxQLength 20

/**************************************************************************
 *                           C O N S T A N T S                            *
 **************************************************************************/

/**************************************************************************
 *                          D A T A    T Y P E S                          *
 **************************************************************************/

typedef struct Node {
    SIMPLEQ_ENTRY(Node) field;
    VIDEO_FRAME_INFO_S stVideoFrame;
} Node;

typedef SIMPLEQ_HEAD(Qhead, Node) Qhead;

/**************************************************************************
 *                         G L O B A L    D A T A                         *
 **************************************************************************/
APP_PARAM_STITCH_CFG_S g_stStitchCfg;
APP_PARAM_STITCH_CFG_S *g_pstStitchCfg = &g_stStitchCfg;

static pthread_t g_Stitch_Src_Thread[STITCH_MAX_SRC_NUM];
static pthread_t *g_Stitch_Thread;
static CVI_BOOL g_Stitch_Running;

Qhead *qHead[STITCH_MAX_SRC_NUM];
int qLength[STITCH_MAX_SRC_NUM];

pthread_cond_t conda[STITCH_MAX_SRC_NUM];
pthread_cond_t condaLength[STITCH_MAX_SRC_NUM];
pthread_mutex_t mutexd[STITCH_MAX_SRC_NUM];
pthread_mutex_t mutexs[STITCH_MAX_SRC_NUM];
pthread_mutex_t mutexqLength[STITCH_MAX_SRC_NUM];

static int threadCnt = 1;

/**************************************************************************
 *                 E X T E R N A L    R E F E R E N C E S                 *
 **************************************************************************/

/**************************************************************************
 *               F U N C T I O N    D E C L A R A T I O N S               *
 **************************************************************************/

APP_PARAM_STITCH_CFG_S *app_ipcam_Stitch_Param_Get(void)
{
    return g_pstStitchCfg;
}

static CVI_S32 app_stitch_Cfg_Wgt_Image(SIZE_S size, enum stitch_wgt_mode wgtmode, char *name,
                      CVI_U64 *u64PhyAddr, CVI_VOID **pVirAddr, CVI_S32 value)
{
    CVI_U32 wgt_len;

    if (!name || strlen(name) == 0) {
        APP_PROF_LOG_PRINT(
            LEVEL_INFO,
            "wgt image file is null, use average value for wgt map.\n");
        wgt_len = size.u32Width * size.u32Height;
        if (wgtmode == STITCH_WGT_UV_SHARE) wgt_len = wgt_len << 1;
        if (CVI_SYS_IonAlloc(u64PhyAddr, pVirAddr, "stitch_wgt_image",
                             wgt_len) != CVI_SUCCESS) {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_SYS_IonAlloc NG.\n");
            return CVI_FAILURE;
        }
        if (*u64PhyAddr == 0 || *pVirAddr == 0) {
            APP_PROF_LOG_PRINT(LEVEL_ERROR,
                               "CVI_SYS_IonAlloc NG. zero phy/vir address\n");
            return CVI_FAILURE;
        }
        APP_PROF_LOG_PRINT(LEVEL_INFO, "wgt_len[%d]\n", wgt_len);
        memset(*pVirAddr, value, wgt_len);
    } else {
        FILE *fp;
        int len;
        fp = fopen(name, "rb");
        if (fp == CVI_NULL) {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "open data file [%s] error\n",
                               name);
            return CVI_FAILURE;
        }

        wgt_len = size.u32Width * size.u32Height;
        if (wgtmode == STITCH_WGT_UV_SHARE) wgt_len = wgt_len << 1;
        if (CVI_SYS_IonAlloc(u64PhyAddr, pVirAddr, "stitch_wgt_image",
                             wgt_len) != CVI_SUCCESS) {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_SYS_IonAlloc NG.\n");
            return CVI_FAILURE;
        }
        if (*u64PhyAddr == 0 || *pVirAddr == 0) {
            APP_PROF_LOG_PRINT(LEVEL_ERROR,
                               "CVI_SYS_IonAlloc NG. zero phy/vir address\n");
            return CVI_FAILURE;
        }

        APP_PROF_LOG_PRINT(LEVEL_INFO, "wgt_len[%d]\n", wgt_len);

        len = fread(*pVirAddr, wgt_len, 1, fp);
        if (len <= 0) {
            APP_PROF_LOG_PRINT(LEVEL_ERROR,
                               "stitch read wgt image fread error, ret(%d)\n",
                               len);
            fclose(fp);
            return CVI_FAILURE;
        }
        fflush(fp);
        fclose(fp);
    }

    return CVI_SUCCESS;
}

static CVI_S32 app_stitch_Set_Wgt_Param(void)
{
    CVI_S32 s32Ret = CVI_SUCCESS;

    //Set wgtAttr
    for (int i = 0; i < (int)(g_pstStitchCfg->srcNum - 1); i++) {
        // Set wgtAttr Width & Height
        g_pstStitchCfg->wgtAttr.size_wgt[i].u32Width = ALIGN(g_pstStitchCfg->srcAttr.ovlap_attr.ovlp_rx[i] - g_pstStitchCfg->srcAttr.ovlap_attr.ovlp_lx[i] + 1, STITCH_ALIGN);
        g_pstStitchCfg->wgtAttr.size_wgt[i].u32Height = g_pstStitchCfg->srcAttr.size[i].u32Height;

        // Set wgtAttr Width & Height
        if (g_pstStitchCfg->wgtAttr.size_wgt[i].u32Width && g_pstStitchCfg->wgtAttr.size_wgt[i].u32Height) {
            s32Ret |= app_stitch_Cfg_Wgt_Image(
                        g_pstStitchCfg->wgtAttr.size_wgt[i],
                        g_pstStitchCfg->opAttr.wgt_mode,
                        g_pstStitchCfg->wgt_alpha_name[i],
                        &g_pstStitchCfg->u64PhyAddr_alpha[i],
                        &g_pstStitchCfg->VirAddr_alpha[i],
                        g_pstStitchCfg->wgt_value_alpha[i]);

            s32Ret |= app_stitch_Cfg_Wgt_Image(
                        g_pstStitchCfg->wgtAttr.size_wgt[i],
                        g_pstStitchCfg->opAttr.wgt_mode,
                        g_pstStitchCfg->wgt_beta_name[i],
                        &g_pstStitchCfg->u64PhyAddr_beta[i],
                        &g_pstStitchCfg->VirAddr_beta[i],
                        g_pstStitchCfg->wgt_value_beta[i]);
        }

        // Set wgtAttr Physical Address
        g_pstStitchCfg->wgtAttr.phy_addr_wgt[i][0] = (__u64) g_pstStitchCfg->u64PhyAddr_alpha[i];
        g_pstStitchCfg->wgtAttr.phy_addr_wgt[i][1] = (__u64) g_pstStitchCfg->u64PhyAddr_beta[i];
    }

    return s32Ret;
}

static CVI_S32 app_stitch_Src_Start(APP_STITCH_SRC_CFG_S *param)
{
    CVI_S32 s32Ret = CVI_SUCCESS;
    VB_BLK blk;

    /*Frame source from CVI_ID_VPSS*/
    if (param->enModId == CVI_ID_VPSS) {
        VPSS_GRP VpssGrp = param->s32DevId;
        VPSS_CHN VpssChn = param->s32ChnId;
        s32Ret = CVI_VPSS_GetChnFrame(VpssGrp, VpssChn, &param->stImgIn, 3000);
        if (s32Ret != CVI_SUCCESS) {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_VPSS_GetChnFrame fail\n");
            goto exit1;
        }
    }

    /*Frame source from CVI_ID_BASE*/
    if (param->enModId == CVI_ID_BASE) {
        (CVI_VOID) param->s32DevId;
        (CVI_VOID) param->s32ChnId;
    }

    if(qLength[param->src_idx] >= MaxQLength) {
        //APP_PROF_LOG_PRINT(LEVEL_INFO, "qLengthL: %d full\n", qLengthL);
        goto exit1;
    }

    /*Insert iNode to tail of the queue*/
    pthread_mutex_lock(&mutexd[param->src_idx]);
    Node *iNode = (Node *)malloc(sizeof(Node));
    iNode->stVideoFrame = param->stImgIn;
    SIMPLEQ_INSERT_TAIL(qHead[param->src_idx], iNode, field);
    pthread_cond_signal(&conda[param->src_idx]);
    pthread_mutex_unlock(&mutexd[param->src_idx]);

    pthread_mutex_lock(&mutexqLength[param->src_idx]);
    qLength[param->src_idx]++;
    pthread_mutex_unlock(&mutexqLength[param->src_idx]);
    return s32Ret;

exit1:
    if (param->stImgIn.stVFrame.u64PhyAddr[0]) {
        blk = CVI_VB_PhysAddr2Handle(param->stImgIn.stVFrame.u64PhyAddr[0]);
        if (blk != VB_INVALID_HANDLE) {
            s32Ret = CVI_VB_ReleaseBlock(blk);
        }
    }

    return s32Ret;
}

static CVI_S32 app_stitch_src_thread(void *arg)
{
    APP_STITCH_SRC_CFG_S *param = (APP_STITCH_SRC_CFG_S *)arg;

    while (g_Stitch_Running) {
        if (0 != app_stitch_Src_Start(param)) {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "Stitch_SrcStart failed!\n");
        }
    }
    return 0;
}

static CVI_S32 app_ipcam_stitch_proc()
{
    CVI_S32 s32Ret = CVI_SUCCESS;
    VB_BLK blk;
    CVI_U64 u64PhyAddr;
    Node *node;

    while (g_Stitch_Running) {

        for (int i = 0; i < (int)g_pstStitchCfg->srcNum; i++){
            /*Send frame to Stitch if ready*/
            if (!g_pstStitchCfg->enBindMode) {
                sem_wait(g_pstStitchCfg->srcParam[i].semv);
            }

            pthread_mutex_lock(&mutexs[i]);
            if (SIMPLEQ_FIRST(qHead[i]) == NULL) {
                pthread_cond_wait(&conda[i], &mutexs[i]);
            }
            s32Ret = CVI_STITCH_SendFrame(
                (STITCH_SRC_IDX) i,
                &SIMPLEQ_FIRST(qHead[i])->stVideoFrame,
                3000);
            u64PhyAddr = SIMPLEQ_FIRST(qHead[i])->stVideoFrame.stVFrame.u64PhyAddr[0];
            node = SIMPLEQ_FIRST(qHead[i]);
            SIMPLEQ_REMOVE_HEAD(qHead[i], field);
            pthread_mutex_unlock(&mutexs[i]);
            free(node);

            pthread_mutex_lock(&mutexqLength[i]);
            qLength[i]--;
            pthread_mutex_unlock(&mutexqLength[i]);

            if (u64PhyAddr) {
                blk = CVI_VB_PhysAddr2Handle(u64PhyAddr);
                if (blk != VB_INVALID_HANDLE) {
                    s32Ret = CVI_VB_ReleaseBlock(blk);
                }
            }
        }
    }
    return s32Ret;
}

CVI_S32 app_ipcam_Stitch_UnInit(void)
{
    CVI_S32 s32Ret = CVI_SUCCESS;
    int i = 0, j = 0, k = 0, m = 0, n = 0;
    CVI_U64 u64PhyAddr;
    VB_BLK blk;
    Node *node;
    CVI_BOOL Enable = g_pstStitchCfg->Enable;

    g_pstStitchCfg->Enable = CVI_FALSE;

    if (Enable) {
        g_Stitch_Running = CVI_FALSE;

        /*Uninit Stitch Src Thread*/
        for (j = 0; j < (int)g_pstStitchCfg->srcNum; j++) {
            if (g_Stitch_Src_Thread[j] != 0) {
                for (i = 0; i < threadCnt; i++) {
                    pthread_kill(g_Stitch_Src_Thread[j], SIGUSR1);
                    pthread_join(g_Stitch_Src_Thread[j], NULL);
                    g_Stitch_Src_Thread[j] = 0;
                }
            }
        }

        /*Uninit Stitch Proc Thread*/
        if (g_Stitch_Thread != 0) {
            for (i = 0; i < threadCnt; i++) {
                pthread_kill(g_Stitch_Thread[i], SIGUSR1);
                pthread_join(g_Stitch_Thread[i], NULL);
                g_Stitch_Thread = 0;
            }
            free(g_Stitch_Thread);
        }

        s32Ret = CVI_STITCH_DisableDev();
        if (s32Ret != CVI_SUCCESS) {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_STITCH_DisableDev failed!\n");
            return CVI_FAILURE;
        }

        s32Ret = CVI_STITCH_DeInit();
        if (s32Ret != CVI_SUCCESS) {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_STITCH_DeInit failed!\n");
            return CVI_FAILURE;
        }

        /*Uninit semaphore*/
        for (k = 0; k < (int)g_pstStitchCfg->srcNum; k++) {
            pthread_cond_destroy(&conda[k]);
            pthread_mutex_destroy(&mutexd[k]);
            pthread_mutex_destroy(&mutexs[k]);
            pthread_mutex_destroy(&mutexqLength[k]);

            sem_destroy(g_pstStitchCfg->srcParam[k].semv);
            free(g_pstStitchCfg->srcParam[k].semv);
        }

        /*Uninit qHead[]*/
        for (m = 0; m < (int)g_pstStitchCfg->srcNum; m++){
            while(SIMPLEQ_FIRST(qHead[m]) != NULL) {
                u64PhyAddr = SIMPLEQ_FIRST(qHead[m])->stVideoFrame.stVFrame.u64PhyAddr[0];
                node = SIMPLEQ_FIRST(qHead[m]);
                SIMPLEQ_REMOVE_HEAD(qHead[m], field);
                if (node) {
                    free(node);
                }

                if (u64PhyAddr) {
                    blk = CVI_VB_PhysAddr2Handle(u64PhyAddr);
                    if (blk != VB_INVALID_HANDLE) {
                        s32Ret = CVI_VB_ReleaseBlock(blk);
                    }
                }
            }
        }

        /*Free memory region of Wgt Image */
        for (n = 0; n < (int)(g_pstStitchCfg->srcNum - 1); n++) {
            if (g_pstStitchCfg->u64PhyAddr_alpha[n] && g_pstStitchCfg->VirAddr_alpha[n])
                CVI_SYS_IonFree(g_pstStitchCfg->u64PhyAddr_alpha[n], g_pstStitchCfg->VirAddr_alpha[n]);

            if (g_pstStitchCfg->u64PhyAddr_beta[n] && g_pstStitchCfg->VirAddr_beta[n])
                CVI_SYS_IonFree(g_pstStitchCfg->u64PhyAddr_beta[n], g_pstStitchCfg->VirAddr_beta[n]);
        }
    }

    return s32Ret;
}

CVI_S32 app_ipcam_Stitch_Init(void)
{
    CVI_S32 s32Ret = CVI_SUCCESS;
    CVI_BOOL Enable = g_pstStitchCfg->Enable;
    int i = 0, j = 0, k = 0;

    if (Enable) {
        APP_PROF_LOG_PRINT(LEVEL_INFO, "stitch init ------------------> start \n");
        g_Stitch_Running = CVI_TRUE;

        g_Stitch_Thread = (pthread_t *)malloc(threadCnt * sizeof(pthread_t));

        for (k = 0; k < (int)g_pstStitchCfg->srcNum; k++) {
            qLength[k] = 0;
            qHead[k] = (Qhead *)malloc(sizeof(Qhead));

            SIMPLEQ_INIT(qHead[k]);
            pthread_cond_init(&conda[k], NULL);
            pthread_mutex_init(&mutexd[k], NULL);
            pthread_mutex_init(&mutexs[k], NULL);
            pthread_mutex_init(&mutexqLength[k], NULL);

            g_pstStitchCfg->srcParam[k].semv = (sem_t *)malloc(sizeof(sem_t));
            sem_init(g_pstStitchCfg->srcParam[k].semv, 0, 1);
        }

        s32Ret = CVI_STITCH_Init();
        if (s32Ret != CVI_SUCCESS) {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_STITCH INIT failed!\n");
            return s32Ret;
        }

        /*Alloc memory region for wgt image*/
        s32Ret = app_stitch_Set_Wgt_Param();
        if (s32Ret != CVI_SUCCESS) {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "app_stitch_Set_Wgt_Param failed!\n");
            return s32Ret;
        }

        s32Ret = CVI_STITCH_Reset();
        if (s32Ret != CVI_SUCCESS) {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_STITCH_Reset failed!\n");
            goto STITCH_EXIT1;
        }

        /*Set Stitch Param -- SetSrcAttr*/
        s32Ret = CVI_STITCH_SetSrcAttr(&g_pstStitchCfg->srcAttr);
        if (s32Ret != CVI_SUCCESS) {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_STITCH_SetSrcAttr failed!\n");
            goto STITCH_EXIT1;
        }

        /*Set Stitch Param -- SetChnAttr*/
        s32Ret = CVI_STITCH_SetChnAttr(&g_pstStitchCfg->chnAttr);
        if (s32Ret != CVI_SUCCESS) {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_STITCH_SetChnAttr failed!\n");
            goto STITCH_EXIT1;
        }

        /*Set Stitch Param -- SetOptAttr*/
        s32Ret = CVI_STITCH_SetOpAttr(&g_pstStitchCfg->opAttr);
        if (s32Ret != CVI_SUCCESS) {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_STITCH_SetOpAttr failed!\n");
            goto STITCH_EXIT1;
        }

        /*Set Stitch Param -- SetWgtAttr*/
        s32Ret = CVI_STITCH_SetWgtAttr(&g_pstStitchCfg->wgtAttr);
        if (s32Ret != CVI_SUCCESS) {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_STITCH_SetWgtAttr failed!\n");
            goto STITCH_EXIT1;
        }

        /*Set Stitch Param -- SetRegX*/
        s32Ret = CVI_STITCH_SetRegX(16);
        if (s32Ret != CVI_SUCCESS) {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_STITCH_SetRegX failed!\n");
            goto STITCH_EXIT1;
        }

        /*After setting param, Enable Device*/
        s32Ret = CVI_STITCH_EnableDev();
        if (s32Ret != CVI_SUCCESS) {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_STITCH_EnableDev failed!\n");
            goto STITCH_EXIT1;
        }

        if (g_pstStitchCfg->bAttachEn) {
            s32Ret = CVI_STITCH_AttachVbPool((VB_POOL)g_pstStitchCfg->u32AttachVbPool);
            if (s32Ret != CVI_SUCCESS) {
                APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_STITCH_AttachVbPool failed!\n");
                goto STITCH_EXIT1;
            }
        }


        for (i = 0; i < threadCnt; i++) {
            /*Init Stitch Src Thread*/
            for (j = 0; j < (int)g_pstStitchCfg->srcNum; j++) {
                s32Ret = pthread_create(&g_Stitch_Src_Thread[j], NULL, (void *)app_stitch_src_thread, &g_pstStitchCfg->srcParam[j]);
                if (s32Ret != CVI_SUCCESS) {
                    APP_PROF_LOG_PRINT(LEVEL_ERROR, "pthread_create src[%d] failed!\n", j);
                    goto STITCH_EXIT2;
                }
            }

            /*Init Stitch Proc Thread*/
            s32Ret = pthread_create(&g_Stitch_Thread[i], NULL, (void *)app_ipcam_stitch_proc, g_pstStitchCfg);
            if (s32Ret != CVI_SUCCESS) {
                APP_PROF_LOG_PRINT(LEVEL_ERROR, "pthread_create stitch failed!\n");
                goto STITCH_EXIT2;
            }
        }

        APP_PROF_LOG_PRINT(LEVEL_INFO, "stitch init ------------------> end \n");
    }

    return s32Ret;

STITCH_EXIT2:
    CVI_STITCH_DisableDev();
STITCH_EXIT1:
    CVI_STITCH_DeInit();

    return s32Ret;
}