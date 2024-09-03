include $(SRCTREE)/build/rule.mk

TOPTARGETS := all

module-$(CONFIG_MODULE_AI)						+= ai
module-$(CONFIG_MODULE_PERIPHERAL)				+= peripheral
# module-$(CONFIG_MODULE_RECORD)					+= record
module-$(CONFIG_MODULE_MEDIA)					+= media
module-$(CONFIG_MODULE_COMMON)					+= common
module-$(CONFIG_MODULE_DISPLAY)					+= display
module-$(CONFIG_MODULE_ENABLE_FRAMEBUFFER)		+= framebuffer
module-$(CONFIG_MODULE_PROTOCOL)				+= protocol

SUBDIRS += $(module-y)

$(TOPTARGETS): $(SUBDIRS) 

$(SUBDIRS):
	$(MAKE) -C $@ $(MAKECMDGOALS)

clean :

install :

.PHONY: $(TOPTARGETS) $(SUBDIRS)
