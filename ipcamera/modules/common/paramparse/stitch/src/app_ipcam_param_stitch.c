#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include "minIni.h"
#include <linux/cvi_math.h>
#include "app_ipcam_paramparse.h"

const char *stitch_bind_mode[STITCH_BIND_SEP] = {
    [STITCH_BIND_DISABLE] = "STITCH_BIND_DISABLE",
    [STITCH_BIND_VENC] = "STITCH_BIND_VENC",
};

int Load_Param_Stitch(const char *file)
{
    CVI_S32 ret = 0;
    CVI_U32 i = 0;
    CVI_S32 enum_num = 0;
    char tmp_section[64] = {0};
    char str_name[PARAM_STRING_NAME_LEN] = {0};
    APP_PARAM_STITCH_CFG_S *Stitch = app_ipcam_Stitch_Param_Get();
    const char ** pixel_format = app_ipcam_Param_get_pixel_format();
    const char ** mode_id = app_ipcam_Param_get_mode_id();

    APP_PROF_LOG_PRINT(LEVEL_INFO, "loading STITCH config ------------------> start \n");

    //Load stitch_cfg
    memset(tmp_section, 0, sizeof(tmp_section));
    snprintf(tmp_section, sizeof(tmp_section), "stitch_cfg");
    Stitch->Enable = ini_getl(tmp_section, "Enable", 0, file);
    Stitch->srcNum = ini_getl(tmp_section, "src_num", 0, file);
    APP_PROF_LOG_PRINT(LEVEL_INFO, "Stitch_cfg Enable: %d\n", Stitch->Enable);

    //Load STITCH_SRC_ATTR_S
    memset(tmp_section, 0, sizeof(tmp_section));
    snprintf(tmp_section, sizeof(tmp_section), "stitch_src_attr");
    if (Stitch->srcNum == 2){
        Stitch->srcAttr.way_num = STITCH_2_WAY;
    } else if (Stitch->srcNum == 4) {
        Stitch->srcAttr.way_num = STITCH_4_WAY;
    }

    Stitch->srcAttr.size[0].u32Width = ini_getl(tmp_section, "src0_width", 0, file);
    Stitch->srcAttr.size[0].u32Height = ini_getl(tmp_section, "src0_height", 0, file);
    Stitch->srcAttr.size[1].u32Width = ini_getl(tmp_section, "src1_width", 0, file);
    Stitch->srcAttr.size[1].u32Height = ini_getl(tmp_section, "src1_height", 0, file);
    Stitch->srcAttr.size[2].u32Width = ini_getl(tmp_section, "src2_width", 0, file);
    Stitch->srcAttr.size[2].u32Height = ini_getl(tmp_section, "src2_height", 0, file);
    Stitch->srcAttr.size[3].u32Width = ini_getl(tmp_section, "src3_width", 0, file);
    Stitch->srcAttr.size[3].u32Height = ini_getl(tmp_section, "src3_height", 0, file);

    ini_gets(tmp_section, "pixel_fmt", " ", str_name, PARAM_STRING_NAME_LEN, file);
    ret = app_ipcam_Param_Convert_StrName_to_EnumNum(str_name, pixel_format, PIXEL_FORMAT_MAX, &enum_num);
    if (ret != CVI_SUCCESS) {
        APP_PROF_LOG_PRINT(LEVEL_INFO, "[%s][pixel_fmt] Fail to convert string name [%s] to enum number!\n", tmp_section, str_name);
    } else {
        APP_PROF_LOG_PRINT(LEVEL_INFO, "[%s][pixel_fmt] Convert string name [%s] to enum number [%d].\n", tmp_section, str_name, enum_num);
        Stitch->srcAttr.fmt_in = enum_num;
    }

    //Load STITCH_SRC_ATTR_S.stitch_src_ovlp_attr
    memset(tmp_section, 0, sizeof(tmp_section));
    snprintf(tmp_section, sizeof(tmp_section), "stitch_src_attr.ovlp_attr");
    Stitch->srcAttr.ovlap_attr.ovlp_lx[0] = ini_getl(tmp_section, "ovlp0_lx", 0, file);
    Stitch->srcAttr.ovlap_attr.ovlp_rx[0] = ini_getl(tmp_section, "ovlp0_rx", 0, file);
    Stitch->srcAttr.ovlap_attr.ovlp_lx[1] = ini_getl(tmp_section, "ovlp1_lx", 0, file);
    Stitch->srcAttr.ovlap_attr.ovlp_rx[1] = ini_getl(tmp_section, "ovlp1_rx", 0, file);
    Stitch->srcAttr.ovlap_attr.ovlp_lx[2] = ini_getl(tmp_section, "ovlp2_lx", 0, file);
    Stitch->srcAttr.ovlap_attr.ovlp_rx[2] = ini_getl(tmp_section, "ovlp2_rx", 0, file);

    //Load STITCH_SRC_ATTR_S.stitch_src_bd_attr
    memset(tmp_section, 0, sizeof(tmp_section));
    snprintf(tmp_section, sizeof(tmp_section), "stitch_src_attr.bd_attr");
    Stitch->srcAttr.bd_attr.bd_lx[0] = ini_getl(tmp_section, "bd0_lx", 0, file);
    Stitch->srcAttr.bd_attr.bd_rx[0] = ini_getl(tmp_section, "bd0_rx", 0, file);
    Stitch->srcAttr.bd_attr.bd_lx[1] = ini_getl(tmp_section, "bd1_lx", 0, file);
    Stitch->srcAttr.bd_attr.bd_rx[1] = ini_getl(tmp_section, "bd1_rx", 0, file);
    Stitch->srcAttr.bd_attr.bd_lx[2] = ini_getl(tmp_section, "bd2_lx", 0, file);
    Stitch->srcAttr.bd_attr.bd_rx[2] = ini_getl(tmp_section, "bd2_rx", 0, file);
    Stitch->srcAttr.bd_attr.bd_lx[3] = ini_getl(tmp_section, "bd3_lx", 0, file);
    Stitch->srcAttr.bd_attr.bd_rx[3] = ini_getl(tmp_section, "bd3_rx", 0, file);

    //Load APP_STITCH_SRC_CFG_S
    for (i = 0; i < (CVI_U32)Stitch->srcNum; i++) {
        memset(tmp_section, 0, sizeof(tmp_section));
        snprintf(tmp_section, sizeof(tmp_section), "stitch_src%d_cfg", i);

        Stitch->srcParam[i].src_idx = (STITCH_SRC_IDX) i;

        ini_gets(tmp_section, "src_mod_id", " ", str_name, PARAM_STRING_NAME_LEN, file);
        ret = app_ipcam_Param_Convert_StrName_to_EnumNum(str_name, mode_id, CVI_ID_BUTT, &enum_num);
        if (ret != CVI_SUCCESS) {
            APP_PROF_LOG_PRINT(LEVEL_INFO, "[%s][src_mod_id] Fail to convert string name [%s] to enum number!\n", tmp_section, str_name);
        } else {
            APP_PROF_LOG_PRINT(LEVEL_INFO, "[%s][src_mod_id] Convert string name [%s] to enum number [%d].\n", tmp_section, str_name, enum_num);
            Stitch->srcParam[i].enModId = enum_num;
        }

        Stitch->srcParam[i].s32DevId = ini_getl(tmp_section, "src_dev_id", 0, file);
        Stitch->srcParam[i].s32ChnId = ini_getl(tmp_section, "src_chn_id", 0, file);

        APP_PROF_LOG_PRINT(LEVEL_INFO, "srcParam[%d].src_idx: %d\n", i, Stitch->srcParam[i].src_idx);
        APP_PROF_LOG_PRINT(LEVEL_INFO, "srcParam[%d].s32DevId: %d\n", i, Stitch->srcParam[i].s32DevId);
        APP_PROF_LOG_PRINT(LEVEL_INFO, "srcParam[%d].s32ChnId: %d\n", i, Stitch->srcParam[i].s32ChnId);
    }

    //Load STITCH_OP_ATTR_S
    memset(tmp_section, 0, sizeof(tmp_section));
    snprintf(tmp_section, sizeof(tmp_section), "stitch_op_attr");
    Stitch->opAttr.wgt_mode = ini_getl(tmp_section, "wgt_mode", 0, file);
    int datasrc =  ini_getl(tmp_section, "data_src", 0, file);
    Stitch->opAttr.data_src = datasrc;

    //Load STITCH_WGT_ATTR_S
    memset(tmp_section, 0, sizeof(tmp_section));
    snprintf(tmp_section, sizeof(tmp_section), "stitch_wgt_attr");
    memset(Stitch->u64PhyAddr_alpha, 0, sizeof(Stitch->u64PhyAddr_alpha));
    memset(Stitch->u64PhyAddr_beta, 0, sizeof(Stitch->u64PhyAddr_beta));
    memset(Stitch->VirAddr_alpha, 0, sizeof(Stitch->VirAddr_alpha));
    memset(Stitch->VirAddr_beta, 0, sizeof(Stitch->VirAddr_beta));

    ini_gets(tmp_section, "wgt0_alpha_file", "", Stitch->wgt_alpha_name[0] , sizeof(Stitch->wgt_alpha_name[0]), file);
    ini_gets(tmp_section, "wgt0_beta_file", "", Stitch->wgt_beta_name[0] , sizeof(Stitch->wgt_beta_name[0]), file);
    Stitch->wgt_value_alpha[0] = ini_getl(tmp_section, "wgt0_alpha_value", 0, file);
    Stitch->wgt_value_beta[0] = ini_getl(tmp_section, "wgt0_beta_value", 0, file);

    ini_gets(tmp_section, "wgt1_alpha_file", "", Stitch->wgt_alpha_name[1] , sizeof(Stitch->wgt_alpha_name[1]), file);
    ini_gets(tmp_section, "wgt1_beta_file", "", Stitch->wgt_beta_name[1] , sizeof(Stitch->wgt_beta_name[1]), file);
    Stitch->wgt_value_alpha[1] = ini_getl(tmp_section, "wgt1_alpha_value", 0, file);
    Stitch->wgt_value_beta[1] = ini_getl(tmp_section, "wgt1_beta_value", 0, file);

    ini_gets(tmp_section, "wgt2_alpha_file", "", Stitch->wgt_alpha_name[2] , sizeof(Stitch->wgt_alpha_name[2]), file);
    ini_gets(tmp_section, "wgt2_beta_file", "", Stitch->wgt_beta_name[2] , sizeof(Stitch->wgt_beta_name[2]), file);
    Stitch->wgt_value_alpha[2] = ini_getl(tmp_section, "wgt2_alpha_value", 0, file);
    Stitch->wgt_value_beta[2] = ini_getl(tmp_section, "wgt2_beta_value", 0, file);

    //Load STITCH_CHN_ATTR_S
    memset(tmp_section, 0, sizeof(tmp_section));
    snprintf(tmp_section, sizeof(tmp_section), "stitch_chn_attr");
    Stitch->chnAttr.size.u32Width = ini_getl(tmp_section, "width", 0, file);
    Stitch->chnAttr.size.u32Height = ini_getl(tmp_section, "height", 0, file);
    ini_gets(tmp_section, "chn_pixel_fmt", " ", str_name, PARAM_STRING_NAME_LEN, file);
    ret = app_ipcam_Param_Convert_StrName_to_EnumNum(str_name, pixel_format, PIXEL_FORMAT_MAX, &enum_num);
    if (ret != CVI_SUCCESS) {
        APP_PROF_LOG_PRINT(LEVEL_INFO, "[%s][pixel_fmt] Fail to convert string name [%s] to enum number!\n", tmp_section, str_name);
    } else {
        APP_PROF_LOG_PRINT(LEVEL_INFO, "[%s][pixel_fmt] Convert string name [%s] to enum number [%d].\n", tmp_section, str_name, enum_num);
        Stitch->chnAttr.fmt_out = enum_num;
    }
    Stitch->bAttachEn = ini_getl(tmp_section, "attach_en", 0, file);
    Stitch->u32AttachVbPool = ini_getl(tmp_section, "attach_pool", 0, file);

    ini_gets(tmp_section, "bind_mode", " ", str_name, PARAM_STRING_NAME_LEN, file);
    ret = app_ipcam_Param_Convert_StrName_to_EnumNum(str_name, stitch_bind_mode, STITCH_BIND_SEP, &enum_num);
    if (ret != CVI_SUCCESS) {
        APP_PROF_LOG_PRINT(LEVEL_INFO, "[%s][bind_mode] Fail to convert string name [%s] to enum number!\n", tmp_section, str_name);
    } else {
        APP_PROF_LOG_PRINT(LEVEL_INFO, "[%s][bind_mode] Convert string name [%s] to enum number [%d].\n", tmp_section, str_name, enum_num);
        Stitch->enBindMode = enum_num;
    }
    ini_gets(tmp_section, "dst_mod_id", " ", str_name, PARAM_STRING_NAME_LEN, file);
    ret = app_ipcam_Param_Convert_StrName_to_EnumNum(str_name, mode_id, CVI_ID_BUTT, &enum_num);
    if (ret != CVI_SUCCESS) {
        APP_PROF_LOG_PRINT(LEVEL_INFO, "[%s][dst_mod_id] Fail to convert string name [%s] to enum number!\n", tmp_section, str_name);
    } else {
        APP_PROF_LOG_PRINT(LEVEL_INFO, "[%s][dst_mod_id] Convert string name [%s] to enum number [%d].\n", tmp_section, str_name, enum_num);
        Stitch->astChn[0].enModId = enum_num;
    }

    Stitch->astChn[0].s32DevId = ini_getl(tmp_section, "dst_dev_id", 0, file);
    Stitch->astChn[0].s32ChnId = ini_getl(tmp_section, "dst_chn_id", 0, file);

    APP_PROF_LOG_PRINT(LEVEL_INFO, "StitchAttr:enable: %d\n", Stitch->Enable);
    APP_PROF_LOG_PRINT(LEVEL_INFO, "StitchAttr:srcNum: %d\n", Stitch->srcNum);
    APP_PROF_LOG_PRINT(LEVEL_INFO, "StitchAttr:way_num: %d\n", Stitch->srcAttr.way_num);
    APP_PROF_LOG_PRINT(LEVEL_INFO, "StitchAttr:srcAttr.size[0].u32Width: %d\n", Stitch->srcAttr.size[0].u32Width);
    APP_PROF_LOG_PRINT(LEVEL_INFO, "StitchAttr:srcAttr.size[0].u32Height: %d\n", Stitch->srcAttr.size[0].u32Height);
    APP_PROF_LOG_PRINT(LEVEL_INFO, "StitchAttr:srcAttr.size[1].u32Width: %d\n", Stitch->srcAttr.size[1].u32Width);
    APP_PROF_LOG_PRINT(LEVEL_INFO, "StitchAttr:srcAttr.size[1].u32Height: %d\n", Stitch->srcAttr.size[1].u32Height);
    APP_PROF_LOG_PRINT(LEVEL_INFO, "StitchAttr:srcAttr.size[2].u32Width: %d\n", Stitch->srcAttr.size[2].u32Width );
    APP_PROF_LOG_PRINT(LEVEL_INFO, "StitchAttr:srcAttr.size[2].u32Height: %d\n", Stitch->srcAttr.size[2].u32Height);
    APP_PROF_LOG_PRINT(LEVEL_INFO, "StitchAttr:srcAttr.size[3].u32Width: %d\n", Stitch->srcAttr.size[3].u32Width);
    APP_PROF_LOG_PRINT(LEVEL_INFO, "StitchAttr:srcAttr.size[3].u32Height: %d\n", Stitch->srcAttr.size[3].u32Height);

    APP_PROF_LOG_PRINT(LEVEL_INFO, "StitchAttr:srcAttr.ovlap_attr.ovlp_lx[0]: %d\n", Stitch->srcAttr.ovlap_attr.ovlp_lx[0]);
    APP_PROF_LOG_PRINT(LEVEL_INFO, "StitchAttr:srcAttr.ovlap_attr.ovlp_rx[0]: %d\n", Stitch->srcAttr.ovlap_attr.ovlp_rx[0]);
    APP_PROF_LOG_PRINT(LEVEL_INFO, "StitchAttr:srcAttr.ovlap_attr.ovlp_lx[1]: %d\n", Stitch->srcAttr.ovlap_attr.ovlp_lx[1]);
    APP_PROF_LOG_PRINT(LEVEL_INFO, "StitchAttr:srcAttr.ovlap_attr.ovlp_rx[1]: %d\n", Stitch->srcAttr.ovlap_attr.ovlp_rx[1]);
    APP_PROF_LOG_PRINT(LEVEL_INFO, "StitchAttr:srcAttr.ovlap_attr.ovlp_lx[2]: %d\n", Stitch->srcAttr.ovlap_attr.ovlp_lx[2]);
    APP_PROF_LOG_PRINT(LEVEL_INFO, "StitchAttr:srcAttr.ovlap_attr.ovlp_rx[2]: %d\n", Stitch->srcAttr.ovlap_attr.ovlp_rx[2]);

    APP_PROF_LOG_PRINT(LEVEL_INFO, "StitchAttr:srcAttr.bd_attr.bd_lx[0]: %d\n", Stitch->srcAttr.bd_attr.bd_lx[0]);
    APP_PROF_LOG_PRINT(LEVEL_INFO, "StitchAttr:srcAttr.bd_attr.bd_rx[0]: %d\n", Stitch->srcAttr.bd_attr.bd_rx[0]);
    APP_PROF_LOG_PRINT(LEVEL_INFO, "StitchAttr:srcAttr.bd_attr.bd_lx[1]: %d\n", Stitch->srcAttr.bd_attr.bd_lx[1]);
    APP_PROF_LOG_PRINT(LEVEL_INFO, "StitchAttr:srcAttr.bd_attr.bd_rx[1]: %d\n", Stitch->srcAttr.bd_attr.bd_rx[1]);
    APP_PROF_LOG_PRINT(LEVEL_INFO, "StitchAttr:srcAttr.bd_attr.bd_lx[2]: %d\n", Stitch->srcAttr.bd_attr.bd_lx[2]);
    APP_PROF_LOG_PRINT(LEVEL_INFO, "StitchAttr:srcAttr.bd_attr.bd_rx[2]: %d\n", Stitch->srcAttr.bd_attr.bd_rx[2]);
    APP_PROF_LOG_PRINT(LEVEL_INFO, "StitchAttr:srcAttr.bd_attr.bd_lx[3]: %d\n", Stitch->srcAttr.bd_attr.bd_lx[3]);
    APP_PROF_LOG_PRINT(LEVEL_INFO, "StitchAttr:srcAttr.bd_attr.bd_rx[3]: %d\n", Stitch->srcAttr.bd_attr.bd_rx[3]);

    APP_PROF_LOG_PRINT(LEVEL_INFO, "StitchAttr:srcAttr.fmt_in: %d\n", Stitch->srcAttr.fmt_in);

    APP_PROF_LOG_PRINT(LEVEL_INFO, "StitchAttr:chnAttr.size.u32Width: %d\n", Stitch->chnAttr.size.u32Width);
    APP_PROF_LOG_PRINT(LEVEL_INFO, "StitchAttr:chnAttr.size.u32Height: %d\n",  Stitch->chnAttr.size.u32Height);
    APP_PROF_LOG_PRINT(LEVEL_INFO, "StitchAttr:chnAttr.fmt_out: %d\n", Stitch->chnAttr.fmt_out);
    APP_PROF_LOG_PRINT(LEVEL_INFO, "StitchAttr:bAttachEn: %d\n", Stitch->bAttachEn);
    APP_PROF_LOG_PRINT(LEVEL_INFO, "StitchAttr:u32AttachVbPool: %d\n", Stitch->u32AttachVbPool);
    APP_PROF_LOG_PRINT(LEVEL_INFO, "StitchAttr:enBindMode: %d\n",  Stitch->enBindMode);
    APP_PROF_LOG_PRINT(LEVEL_INFO, "StitchAttr:dst_mod_id: %d\n", Stitch->astChn[0].enModId);
    APP_PROF_LOG_PRINT(LEVEL_INFO, "StitchAttr:dst_dev_id: %d\n", Stitch->astChn[0].s32DevId);
    APP_PROF_LOG_PRINT(LEVEL_INFO, "StitchAttr:dst_chn_id: %d\n", Stitch->astChn[0].s32ChnId);

    APP_PROF_LOG_PRINT(LEVEL_INFO, "StitchAttr:opAttr.size.wgt_mode: %d\n", Stitch->opAttr.wgt_mode);
    APP_PROF_LOG_PRINT(LEVEL_INFO, "StitchAttr:opAttr.data_src: %d\n", Stitch->opAttr.data_src);

    APP_PROF_LOG_PRINT(LEVEL_INFO, "StitchAttr:wgtAttr.alpha0_value: %d\n", Stitch->wgt_value_alpha[0]);
    APP_PROF_LOG_PRINT(LEVEL_INFO, "StitchAttr:wgtAttr.beta0_value: %d\n", Stitch->wgt_value_beta[0]);
    APP_PROF_LOG_PRINT(LEVEL_INFO, "StitchAttr:wgtAttr.alpha1_value: %d\n", Stitch->wgt_value_alpha[1]);
    APP_PROF_LOG_PRINT(LEVEL_INFO, "StitchAttr:wgtAttr.beta1_value: %d\n", Stitch->wgt_value_beta[1]);
    APP_PROF_LOG_PRINT(LEVEL_INFO, "StitchAttr:wgtAttr.alpha2_value: %d\n", Stitch->wgt_value_alpha[2]);
    APP_PROF_LOG_PRINT(LEVEL_INFO, "StitchAttr:wgtAttr.beta2_value: %d\n", Stitch->wgt_value_beta[2]);
    APP_PROF_LOG_PRINT(LEVEL_INFO, "StitchAttr:wgtAttr.alpha0_filename: %s\n", Stitch->wgt_alpha_name[0]);
    APP_PROF_LOG_PRINT(LEVEL_INFO, "StitchAttr:wgtAttr.beta0_filename: %s\n", Stitch->wgt_beta_name[0]);
    APP_PROF_LOG_PRINT(LEVEL_INFO, "StitchAttr:wgtAttr.alpha1_filename: %s\n", Stitch->wgt_alpha_name[1]);
    APP_PROF_LOG_PRINT(LEVEL_INFO, "StitchAttr:wgtAttr.beta1_filename: %s\n", Stitch->wgt_beta_name[1]);
    APP_PROF_LOG_PRINT(LEVEL_INFO, "StitchAttr:wgtAttr.alpha2_filename: %s\n", Stitch->wgt_alpha_name[2]);
    APP_PROF_LOG_PRINT(LEVEL_INFO, "StitchAttr:wgtAttr.beta2_filename: %s\n", Stitch->wgt_beta_name[2]);

    APP_PROF_LOG_PRINT(LEVEL_INFO, "loading STITCH config ------------------> done \n\n");
    return CVI_SUCCESS;
}