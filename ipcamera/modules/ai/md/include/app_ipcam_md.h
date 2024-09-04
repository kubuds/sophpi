#ifndef __APP_IPCAM_MD_H__
#define __APP_IPCAM_MD_H__

#include "linux/cvi_type.h"
#include "linux/cvi_comm_video.h"
#include "cvi_vpss.h"
#include "app_ipcam_comm.h"
#include "app_ipcam_vi.h"
#include "app_ipcam_vpss.h"
#include "app_ipcam_venc.h"
#include "cvi_md.h"


#ifdef __cplusplus
extern "C"
{
#endif

#define SMT_MUTEXAUTOLOCK_MD_INIT(mutex) pthread_mutex_t AUTOLOCK_##mutex = PTHREAD_MUTEX_INITIALIZER;

#define SMT_MutexAutoLock_MD(mutex, lock) __attribute__((cleanup(AutoUnLock_MD))) \
       pthread_mutex_t *lock= &AUTOLOCK_##mutex;\
       pthread_mutex_lock(lock);

  __attribute__ ((always_inline)) inline void AutoUnLock_MD(void *mutex) {
         pthread_mutex_unlock( *(pthread_mutex_t**) mutex);
 }

typedef struct {
  int *p_boxes;
  uint32_t num_boxes;
} cvimd_object_t;

/** @struct cvtdl_service_brush_t
 *  @ingroup core_cviaiservice
 *  @brief Brush structure for bounding box drawing
 *
 */
typedef struct {
  struct {
    float r;
    float g;
    float b;
  } color;
  uint32_t size;
} cvimd_service_brush_t;

typedef struct APP_PARAM_MD_CFG_T {
    CVI_BOOL bEnable;
    VPSS_GRP VpssGrp;
    VPSS_CHN VpssChn;
    CVI_U32 u32GrpWidth;
	CVI_U32 u32GrpHeight;
    CVI_U32 threshold;
    CVI_U32 u32BgUpPeriod;
    CVI_U32 miniArea;
    cvimd_service_brush_t rect_brush;
} APP_PARAM_MD_CFG_S;

/* motion detection function */
APP_PARAM_MD_CFG_S *app_ipcam_MD_Param_Get(void);
CVI_VOID app_ipcam_MD_ProcStatus_Set(CVI_BOOL flag);
CVI_BOOL app_ipcam_MD_ProcStatus_Get(void);
CVI_VOID app_ipcam_MD_Pause_Set(CVI_BOOL flag);
CVI_BOOL app_ipcam_MD_Pause_Get(void);
// int app_ipcam_MD_Rect_Draw(VIDEO_FRAME_INFO_S *pstVencFrame);
int app_ipcam_MD_Start(void);
int app_ipcam_MD_Stop(void);
int app_ipcam_MD_ObjDrawInfo_Get(cvimd_object_t *pstMdObj);
CVI_U32 app_ipcam_MD_ProcFps_Get(void);
CVI_S32 app_ipcam_MD_ProcTime_Get(void);
CVI_VOID app_ipcam_MD_Thresold_Set(CVI_U32 value);
CVI_U32 app_ipcam_MD_Thresold_Get(void);
CVI_S32 app_ipcam_MD_StatusGet(void);
CVI_VOID app_ipcam_Md_obj_Free(cvimd_object_t *pstMdObj);


#ifdef __cplusplus
}
#endif

#endif
