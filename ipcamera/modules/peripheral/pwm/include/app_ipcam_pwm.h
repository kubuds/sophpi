#ifndef __APP_IPCAM_PWM_H__
#define __APP_IPCAM_PWM_H__

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif /* End of #ifdef __cplusplus */

typedef struct APP_PARAM_PWM_CFG_T {
    CVI_U32 grp;
    CVI_U32 chn;
    CVI_U32 enabled;      // PWM是否使能，0表示未使能，1表示使能
    CVI_U32 period;       // PWM周期（单位为纳秒）
    CVI_U32 duty_cycle;   // PWM占空比（单位为纳秒）
}APP_PARAM_PWM_CFG_S;

APP_PARAM_PWM_CFG_S *app_ipcam_Pwm_Param_Get(void);
int app_ipcam_Pwm_Export(int grp, int chn);
int app_ipcam_Pwm_UnExport(int grp, int chn);
int app_ipcam_Pwm_Param_Set(int grp, int chn, int period, int duty_cycle);
int app_ipcam_Pwm_Enable(int grp, int chn);
int app_ipcam_Pwm_Disable(int grp, int chn);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* End of #ifdef __cplusplus */

#endif