include $(SRCTREE)/build/rule.mk

TOPTARGETS := all clean install

module-y									+= paramparse_common
module-$(CONFIG_MODULE_MEDIA_SYS)			+= sys
module-$(CONFIG_MODULE_MEDIA_VI)			+= vi
module-$(CONFIG_MODULE_MEDIA_VPSS)			+= vpss
module-$(CONFIG_MODULE_MEDIA_DWA)			+= dwa
module-$(CONFIG_MODULE_MEDIA_VENC)			+= venc
module-$(CONFIG_MODULE_MEDIA_DEC)			+= vdec
module-$(CONFIG_MODULE_MEDIA_DECSOFT)		+= vdec_soft
module-$(CONFIG_MODULE_DISPLAY)				+= display
module-$(CONFIG_MODULE_RTSP)				+= rtsp
module-$(CONFIG_MODULE_MEDIA_OSD)			+= osd
module-$(CONFIG_MODULE_MEDIA_STITCH)		+= stitch
module-$(CONFIG_MODULE_GPIO)				+= gpio
module-$(CONFIG_MODULE_PWM)					+= pwm
module-$(CONFIG_MODULE_FRMBUF)				+= frmbuf
module-$(CONFIG_MODULE_MEDIA_AUDIO)			+= audio
module-$(CONFIG_MODULE_AI)					+= ai
module-$(CONFIG_MODULE_AI_MD)				+= ai_md
module-$(CONFIG_MODULE_AI_PD)				+= ai_pd
module-$(CONFIG_MODULE_AI_HAND_DETECT)		+= ai_hd
module-$(CONFIG_MODULE_AI_FD_FACE)			+= ai_face
module-$(CONFIG_MODULE_AI_IRFAECE)			+= ai_ir_face
module-$(CONFIG_MODULE_AI_BABYCRY)			+= ai_babycry
module-$(CONFIG_MODULE_AI_CONSUMER_COUNTING)+= ai_consumer_counting
# module-$(CONFIG_MODULE_RECORD)				+= record


SUBDIRS += $(module-y)

$(TOPTARGETS): $(SUBDIRS)

$(SUBDIRS):
	$(MAKE) -C $@ $(MAKECMDGOALS)

.PHONY: $(TOPTARGETS) $(SUBDIRS)
