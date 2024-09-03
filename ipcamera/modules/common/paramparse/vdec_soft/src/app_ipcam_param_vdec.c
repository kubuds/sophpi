#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include "minIni.h"
#include "app_ipcam_paramparse.h"


int Load_Param_Vdec_Soft(const char * file)
{
    int i = 0;
    long int chn_num = 0;
    char tmp_section[32] = {0};
    char tmp_buff[PARAM_STRING_LEN] = {0};
    APP_PARAM_VDEC_SOFT_CTX_S * VdecSoft = app_ipcam_Vdec_Soft_Param_Get();

    APP_PROF_LOG_PRINT(LEVEL_INFO, "loading vdec soft config ------------------> start \n");

    chn_num = ini_getl("vdecsoft_config", "chn_num", 0, file);
    VdecSoft->s32VdecChnCnt = chn_num;

    APP_PROF_LOG_PRINT(LEVEL_INFO, "chn_num: %ld\n", chn_num);

    for (i = 0; i < chn_num; i++) {
        memset(tmp_section, 0, sizeof(tmp_section));
        snprintf(tmp_section, sizeof(tmp_section), "vdecsoftchn%d", i);

        VdecSoft->astVdecSoftChnCfg[i].de_type = ini_getl(tmp_section, "de_type", 0, file);

        ini_gets(tmp_section, "decode_filename", " ", tmp_buff, PARAM_STRING_LEN, file);
        strncpy(VdecSoft->astVdecSoftChnCfg[i].decode_file_name, tmp_buff, PARAM_STRING_LEN);

        VdecSoft->astVdecSoftChnCfg[i].u32Width    = ini_getl(tmp_section, "width", 0, file);
        VdecSoft->astVdecSoftChnCfg[i].u32Height   = ini_getl(tmp_section, "height", 0, file);
        VdecSoft->astVdecSoftChnCfg[i].videoFormat = ini_getl(tmp_section, "video_format", 0, file);
        VdecSoft->astVdecSoftChnCfg[i].scaleFormat = ini_getl(tmp_section, "scale_format", 0, file);

        APP_PROF_LOG_PRINT(LEVEL_INFO, "de_type:%d, filename:%s, w:%d, h:%d, vfmt:%d, sfmt:%d.\n"
            , VdecSoft->astVdecSoftChnCfg[i].de_type
            , VdecSoft->astVdecSoftChnCfg[i].decode_file_name
            , VdecSoft->astVdecSoftChnCfg[i].u32Width
            , VdecSoft->astVdecSoftChnCfg[i].u32Height
            , VdecSoft->astVdecSoftChnCfg[i].videoFormat
            , VdecSoft->astVdecSoftChnCfg[i].scaleFormat);
    }

    APP_PROF_LOG_PRINT(LEVEL_INFO, "loading vdec soft config ------------------> done \n\n");

    return CVI_SUCCESS;
}
