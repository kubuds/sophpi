#include <stdbool.h>
#include <stdio.h>

#include "linux/cvi_type.h"
#include "dsi_3aml069lp01g.h"
#include "dsi_gm8775c.h"
#include "dsi_hx8394_evb.h"
#include "dsi_hx8399_1080p.h"
#include "dsi_icn9707.h"
#include "dsi_ili9881c.h"
#include "dsi_ili9881d.h"
#include "dsi_jd9366ab.h"
#if !defined(__CV186AH__)
#include "dsi_jd9852.h"
#include "dsi_lt9611.h"
#endif
#include "dsi_nt35521.h"
#include "dsi_ota7290b.h"
#include "dsi_ota7290b_1920.h"
#include "dsi_st7701.h"
#if !defined(__CV186AH__)
#include "dsi_st7796s.h"
#include "bt656_tp2803.h"
#endif
#include "i80_st7789v.h"
#if !defined(__CV186AH__)
#include "dsi_st7785m.h"
#endif

#include "app_ipcam_comm.h"
#include "app_ipcam_mipi_tx.h"
#include "app_ipcam_panel.h"
#include "app_ipcam_vo.h"


CVI_S32 app_ipcam_Panel_FillIntfAttr(APP_PARAM_VO_CFG_T* const pstVoCfg)
{
    _NULL_POINTER_CHECK_(pstVoCfg, CVI_FAILURE);

    switch (pstVoCfg->stVoPubAttr.enIntfType) {

        case VO_INTF_I80:
            #if defined(__CV186AH__)
            pstVoCfg->sti80Cfg = stI80Cfg;
            #else
            pstVoCfg->stVoPubAttr.sti80Cfg = stI80Cfg;
            #endif
            break;
        #if !defined(__CV186AH__)
        case VO_INTF_CVBS:
        case VO_INTF_YPBPR:
        case VO_INTF_VGA:
        case VO_INTF_BT656:
            pstVoCfg->stVoPubAttr.stBtAttr = stTP2803Cfg;
            break;
        #endif
        case VO_INTF_BT1120:
        case VO_INTF_LCD:
        case VO_INTF_LCD_18BIT:
        case VO_INTF_LCD_24BIT:
        case VO_INTF_LCD_30BIT:
        case VO_INTF_HDMI:
            break;

        case VO_INTF_MIPI:
        case VO_INTF_MIPI_SLAVE:
            //no need, MIPI-DSI is setup by mipi-tx
            break;

        default:
            break;
    }

    return CVI_SUCCESS;
}

CVI_S32 app_ipcam_Panel_PanelDesc_Get(const PANEL_TYPE_E* const penPanelType, PANEL_DESC_T* const pstPanelDesc)
{
    _NULL_POINTER_CHECK_(penPanelType, CVI_FAILURE);
    _NULL_POINTER_CHECK_(pstPanelDesc, CVI_FAILURE);

    switch(*penPanelType) {
        case PANEL_DSI_3AML069LP01G:
            pstPanelDesc->pchPanelName = "3AML069LP01G-600x1024";
            pstPanelDesc->pstDevCfg = &dev_cfg_3AML069LP01G_600x1024;
            pstPanelDesc->pstHsTimingCfg = &hs_timing_cfg_3AML069LP01G_600x1024;
            pstPanelDesc->pstDsiInitCmds = dsi_init_cmds_3AML069LP01G_600x1024;
            pstPanelDesc->s32DsiInitCmdsSize = ARRAY_SIZE(dsi_init_cmds_3AML069LP01G_600x1024);
            break;

        case PANEL_DSI_GM8775C:
            pstPanelDesc->pchPanelName = "GM8775C";
            pstPanelDesc->pstDevCfg = &dev_cfg_gm8775c;
            pstPanelDesc->pstHsTimingCfg = &hs_timing_cfg_gm8775c;
            pstPanelDesc->pstDsiInitCmds = dsi_init_cmds_gm8775c;
            pstPanelDesc->s32DsiInitCmdsSize = ARRAY_SIZE(dsi_init_cmds_gm8775c);
            break;

        case PANEL_DSI_HX8399_1080P:
            pstPanelDesc->pchPanelName = "HX8399_1080x1920";
            pstPanelDesc->pstDevCfg = &dev_cfg_hx8399_1080x1920;
            pstPanelDesc->pstHsTimingCfg = &hs_timing_cfg_hx8399_1080x1920;
            pstPanelDesc->pstDsiInitCmds = dsi_init_cmds_hx8399_1080x1920;
            pstPanelDesc->s32DsiInitCmdsSize = ARRAY_SIZE(dsi_init_cmds_hx8399_1080x1920);
            break;

        case PANEL_DSI_ICN9707:
            pstPanelDesc->pchPanelName = "ICN9707-480x1920";
            pstPanelDesc->pstDevCfg = &dev_cfg_icn9707_480x1920;
            pstPanelDesc->pstHsTimingCfg = &hs_timing_cfg_icn9707_480x1920;
            pstPanelDesc->pstDsiInitCmds = dsi_init_cmds_icn9707_480x1920;
            pstPanelDesc->s32DsiInitCmdsSize = ARRAY_SIZE(dsi_init_cmds_icn9707_480x1920);
            break;

        case PANEL_DSI_ILI9881C:
            pstPanelDesc->pchPanelName = "ILI9881C-720x1280";
            pstPanelDesc->pstDevCfg = &dev_cfg_ili9881c_720x1280;
            pstPanelDesc->pstHsTimingCfg = &hs_timing_cfg_ili9881c_720x1280;
            pstPanelDesc->pstDsiInitCmds = dsi_init_cmds_ili9881c_720x1280;
            pstPanelDesc->s32DsiInitCmdsSize = ARRAY_SIZE(dsi_init_cmds_ili9881c_720x1280);
            break;

        case PANEL_DSI_ILI9881D:
            pstPanelDesc->pchPanelName = "ILI9881D-720x1280";
            pstPanelDesc->pstDevCfg = &dev_cfg_ili9881d_720x1280;
            pstPanelDesc->pstHsTimingCfg = &hs_timing_cfg_ili9881d_720x1280;
            pstPanelDesc->pstDsiInitCmds = dsi_init_cmds_ili9881d_720x1280;
            pstPanelDesc->s32DsiInitCmdsSize = ARRAY_SIZE(dsi_init_cmds_ili9881d_720x1280);
            break;

        case PANEL_DSI_JD9366AB:
            pstPanelDesc->pchPanelName = "JD9366AB-800x1280";
            pstPanelDesc->pstDevCfg = &dev_cfg_jd9366ab_800x1280;
            pstPanelDesc->pstHsTimingCfg = &hs_timing_cfg_jd9366ab_800x1280;
            pstPanelDesc->pstDsiInitCmds = dsi_init_cmds_jd9366ab_800x1280;
            pstPanelDesc->s32DsiInitCmdsSize = ARRAY_SIZE(dsi_init_cmds_jd9366ab_800x1280);
            break;
        #if !defined(__CV186AH__)
        case PANEL_DSI_JD9852:
            pstPanelDesc->pchPanelName = "JD9852-240x320";
            pstPanelDesc->pstDevCfg = &dev_cfg_JD9852_240x320;
            pstPanelDesc->pstHsTimingCfg = &hs_timing_cfg_JD9852_240x320;
            pstPanelDesc->pstDsiInitCmds = dsi_init_cmds_JD9852_320x480;
            pstPanelDesc->s32DsiInitCmdsSize = ARRAY_SIZE(dsi_init_cmds_JD9852_320x480);
            break;

        case PANEL_DSI_LT9611_1024x768_60:
            pstPanelDesc->pchPanelName = "LT9611-1024x768_60";
            pstPanelDesc->pstDevCfg = &dev_cfg_lt9611_1024x768_60Hz;
            pstPanelDesc->pstHsTimingCfg = &hs_timing_cfg_lt9611;
            pstPanelDesc->pstDsiInitCmds = NULL;
            pstPanelDesc->s32DsiInitCmdsSize = 0;
            break;

        case PANEL_DSI_LT9611_1280x720_60:
            pstPanelDesc->pchPanelName = "LT9611-1280x720_60";
            pstPanelDesc->pstDevCfg = &dev_cfg_lt9611_1280x720_60Hz;
            pstPanelDesc->pstHsTimingCfg = &hs_timing_cfg_lt9611;
            pstPanelDesc->pstDsiInitCmds = NULL;
            pstPanelDesc->s32DsiInitCmdsSize = 0;
            break;

        case PANEL_DSI_LT9611_1280x1024_60:
            pstPanelDesc->pchPanelName = "LT9611-1280x1024_60";
            pstPanelDesc->pstDevCfg = &dev_cfg_lt9611_1280x1024_60Hz;
            pstPanelDesc->pstHsTimingCfg = &hs_timing_cfg_lt9611;
            pstPanelDesc->pstDsiInitCmds = NULL;
            pstPanelDesc->s32DsiInitCmdsSize = 0;
            break;

        case PANEL_DSI_LT9611_1600x1200_60:
            pstPanelDesc->pchPanelName = "LT9611-1600x1200_60";
            pstPanelDesc->pstDevCfg = &dev_cfg_lt9611_1600x1200_60Hz;
            pstPanelDesc->pstHsTimingCfg = &hs_timing_cfg_lt9611;
            pstPanelDesc->pstDsiInitCmds = NULL;
            pstPanelDesc->s32DsiInitCmdsSize = 0;
            break;

        case PANEL_DSI_LT9611_1920x1080_30:
            pstPanelDesc->pchPanelName = "LT9611-1920x1080_30";
            pstPanelDesc->pstDevCfg = &dev_cfg_lt9611_1920x1080_30Hz;
            pstPanelDesc->pstHsTimingCfg = &hs_timing_cfg_lt9611;
            pstPanelDesc->pstDsiInitCmds = NULL;
            pstPanelDesc->s32DsiInitCmdsSize = 0;
            break;

        case PANEL_DSI_LT9611_1920x1080_60:
            pstPanelDesc->pchPanelName = "LT9611-1920x1080_60";
            pstPanelDesc->pstDevCfg = &dev_cfg_lt9611_1920x1080_60Hz;
            pstPanelDesc->pstHsTimingCfg = &hs_timing_cfg_lt9611;
            pstPanelDesc->pstDsiInitCmds = NULL;
            pstPanelDesc->s32DsiInitCmdsSize = 0;
            break;
        #endif
        case PANEL_DSI_NT35521:
            pstPanelDesc->pchPanelName = "NT35521-800x1280";
            pstPanelDesc->pstDevCfg = &dev_cfg_nt35521_800x1280;
            pstPanelDesc->pstHsTimingCfg = &hs_timing_cfg_nt35521_800x1280;
            pstPanelDesc->pstDsiInitCmds = dsi_init_cmds_nt35521_800x1280;
            pstPanelDesc->s32DsiInitCmdsSize = ARRAY_SIZE(dsi_init_cmds_nt35521_800x1280);
            break;

        case PANEL_DSI_OTA7290B:
            pstPanelDesc->pchPanelName = "OTA7290B-320x1280";
            pstPanelDesc->pstDevCfg = &dev_cfg_ota7290b_320x1280;
            pstPanelDesc->pstHsTimingCfg = &hs_timing_cfg_ota7290b_320x1280;
            pstPanelDesc->pstDsiInitCmds = dsi_init_cmds_ota7290b_320x1280;
            pstPanelDesc->s32DsiInitCmdsSize = ARRAY_SIZE(dsi_init_cmds_ota7290b_320x1280);
            break;

        case PANEL_DSI_OTA7290B_1920:
            pstPanelDesc->pchPanelName = "OTA7290B-440x1920";
            pstPanelDesc->pstDevCfg = &dev_cfg_ota7290b_440x1920;
            pstPanelDesc->pstHsTimingCfg = &hs_timing_cfg_ota7290b_440x1920;
            pstPanelDesc->pstDsiInitCmds = dsi_init_cmds_ota7290b_440x1920;
            pstPanelDesc->s32DsiInitCmdsSize = ARRAY_SIZE(dsi_init_cmds_ota7290b_440x1920);
            break;

        case PANEL_DSI_ST7701:
            pstPanelDesc->pchPanelName = "ST7701-480x800";
            pstPanelDesc->pstDevCfg = &dev_cfg_st7701_480x800;
            pstPanelDesc->pstHsTimingCfg = &hs_timing_cfg_st7701_480x800;
            pstPanelDesc->pstDsiInitCmds = dsi_init_cmds_st7701_480x800;
            pstPanelDesc->s32DsiInitCmdsSize = ARRAY_SIZE(dsi_init_cmds_st7701_480x800);
            break;
        #if !defined(__CV186AH__)
        case PANEL_DSI_ST7785M:
            pstPanelDesc->pchPanelName = "ST7785M-240x320";
            pstPanelDesc->pstDevCfg = &dev_cfg_st7785m_240x320;
            pstPanelDesc->pstHsTimingCfg = &hs_timing_cfg_st7785m_240x320;
            pstPanelDesc->pstDsiInitCmds = dsi_init_cmds_st7785m_240x320;
            pstPanelDesc->s32DsiInitCmdsSize = ARRAY_SIZE(dsi_init_cmds_st7785m_240x320);
            break;

        case PANEL_DSI_ST7796S:
            pstPanelDesc->pchPanelName = "ST7796S-320x480";
            pstPanelDesc->pstDevCfg = &dev_cfg_ST7796S_320x480;
            pstPanelDesc->pstHsTimingCfg = &hs_timing_cfg_ST7796S_320x480;
            pstPanelDesc->pstDsiInitCmds = dsi_init_cmds_ST7796S_320x480;
            pstPanelDesc->s32DsiInitCmdsSize = ARRAY_SIZE(dsi_init_cmds_ST7796S_320x480);
            break;
        #endif
        case PANEL_DSI_HX8394_EVB:
        #if !defined(__CV186AH__)
        case PANEL_BT656_TP2803:
        #endif
        case PANEL_I80_ST7789V:
        default:
            pstPanelDesc->pchPanelName = "HX8394-720x1280";
            pstPanelDesc->pstDevCfg = &dev_cfg_hx8394_720x1280;
            pstPanelDesc->pstHsTimingCfg = &hs_timing_cfg_hx8394_720x1280;
            pstPanelDesc->pstDsiInitCmds = dsi_init_cmds_hx8394_720x1280;
            pstPanelDesc->s32DsiInitCmdsSize = ARRAY_SIZE(dsi_init_cmds_hx8394_720x1280);
            break;
    }

    return CVI_SUCCESS;
}
