include $(SRCTREE)/build/rule.mk

TOPTARGETS := all clean install

module-$(CONFIG_MODULE_MEDIA_SYS)						+= sys
module-$(CONFIG_MODULE_MEDIA_VI)						+= vi
module-$(CONFIG_MODULE_MEDIA_VPSS)						+= vpss
module-$(CONFIG_MODULE_MEDIA_DWA)						+= dwa
module-$(CONFIG_MODULE_MEDIA_VENC)						+= venc
module-$(CONFIG_MODULE_MEDIA_OSD)						+= osd
module-$(CONFIG_MODULE_MEDIA_VO)						+= vo
module-$(CONFIG_MODULE_MEDIA_DEC)						+= vdec
module-$(CONFIG_MODULE_MEDIA_DECSOFT)					+= vdecsoft
module-$(CONFIG_MODULE_MEDIA_AUDIO)						+= audio
module-$(CONFIG_MODULE_MEDIA_STITCH)					+= stitch

SUBDIRS += $(module-y)

$(TOPTARGETS): $(SUBDIRS) 

$(SUBDIRS):
	$(MAKE) -C $@ $(MAKECMDGOALS)

.PHONY: $(TOPTARGETS) $(SUBDIRS)
