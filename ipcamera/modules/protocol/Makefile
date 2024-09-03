include $(SRCTREE)/build/rule.mk

TOPTARGETS := all clean install

module-$(CONFIG_MODULE_RTSP)					+= rtsp
module-$(CONFIG_MODULE_CVIUAC)					+= cvi_uac
module-$(CONFIG_MODULE_CVIUVC)					+= cvi_uvc
module-$(CONFIG_MODULE_NETWORK)					+= network


SUBDIRS += $(module-y)

$(TOPTARGETS): $(SUBDIRS) 

$(SUBDIRS):
	$(MAKE) -C $@ $(MAKECMDGOALS)

.PHONY: $(TOPTARGETS) $(SUBDIRS)
