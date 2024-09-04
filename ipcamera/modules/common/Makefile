include $(SRCTREE)/build/rule.mk

TOPTARGETS := all clean install

module-$(CONFIG_MODULE_CJSON)					+= cjson
module-$(CONFIG_MODULE_MININI)					+= minini
module-$(CONFIG_MODULE_LIST)					+= list
module-$(CONFIG_MODULE_COMMON)					+= comm
module-$(CONFIG_MODULE_RINGBUF)					+= ringbuffer
module-$(CONFIG_MODULE_PARAMPARSE)				+= paramparse

SUBDIRS += $(module-y)

$(TOPTARGETS): $(SUBDIRS) 

$(SUBDIRS):
	$(MAKE) -C $@ $(MAKECMDGOALS)

.PHONY: $(TOPTARGETS) $(SUBDIRS)
