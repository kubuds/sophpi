include $(SRCTREE)/build/rule.mk

TOPTARGETS := all clean install

module-$(CONFIG_MODULE_DISP_FRMBUF)					+= disp_frmbuf
module-$(CONFIG_MODULE_FRMBUF)						+= frmbuf

SUBDIRS += $(module-y)

$(TOPTARGETS): $(SUBDIRS) 

$(SUBDIRS):
	$(MAKE) -C $@ $(MAKECMDGOALS)

.PHONY: $(TOPTARGETS) $(SUBDIRS)
