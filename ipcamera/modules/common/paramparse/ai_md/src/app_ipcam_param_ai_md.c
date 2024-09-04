#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include "minIni.h"
#include "app_ipcam_paramparse.h"


int Load_Param_MD(const char * file)
{
    APP_PROF_LOG_PRINT(LEVEL_INFO, "loading MD config ------------------> start \n");

    float color_t;
    APP_PARAM_MD_CFG_S *Md = app_ipcam_MD_Param_Get();

    Md->bEnable       = ini_getl("md_config", "md_enable", 0, file);
    Md->VpssGrp       = ini_getl("md_config", "vpss_grp", 0, file);
    Md->VpssChn       = ini_getl("md_config", "vpss_chn", 0, file);
    Md->u32GrpWidth   = ini_getl("md_config", "grp_width", 0, file);
    Md->u32GrpHeight  = ini_getl("md_config", "grp_height", 0, file);
    Md->threshold     = ini_getl("md_config", "threshold", 0, file);
    Md->miniArea      = ini_getl("md_config", "miniArea", 0, file);
    Md->u32BgUpPeriod = ini_getl("md_config", "bgUpPeriod", 0, file);

    color_t = ini_getf("md_config", "color_r", 0, file);
    Md->rect_brush.color.r = color_t*255;
    color_t = ini_getf("md_config", "color_g", 0, file);
    Md->rect_brush.color.g = color_t*255;
    color_t = ini_getf("md_config", "color_b", 0, file);
    Md->rect_brush.color.b = color_t*255;

    Md->rect_brush.size = ini_getl("md_config", "color_size", 0, file);

    APP_PROF_LOG_PRINT(LEVEL_INFO, "bEnable=%d Grp=%d Chn=%d GrpW=%d GrpH=%d\n",
        Md->bEnable, Md->VpssGrp, Md->VpssChn, Md->u32GrpWidth, Md->u32GrpHeight);
    APP_PROF_LOG_PRINT(LEVEL_INFO, "threshold=%d miniArea=%d u32BgUpPeriod=%d\n",
        Md->threshold, Md->miniArea, Md->u32BgUpPeriod);
    APP_PROF_LOG_PRINT(LEVEL_INFO, "color r=%f g=%f b=%f size=%d\n",
        Md->rect_brush.color.r, Md->rect_brush.color.g,
        Md->rect_brush.color.b, Md->rect_brush.size);

    APP_PROF_LOG_PRINT(LEVEL_INFO, "loading MD config ------------------> done \n\n");

    return CVI_SUCCESS;
}
