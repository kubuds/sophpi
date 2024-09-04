include $(SRCTREE)/build/rule.mk

TARGET = $(TARGET_OUT_DIR)/$(PROJECT)/$(PROJECT)
SEXT	:= c
SDIR	+= $(CURDIR)/src

MW_LIB := $(MW_PATH)/lib
MW_INC := $(MW_PATH)/include
MW_3RD_LIB := $(MW_PATH)/lib/3rd

INCS-y									+= -I$(CURDIR)/include

SUBDIRS := \
	$(SRCTREE)/modules \

all : $(SUBDIRS) $(TARGET)

$(SUBDIRS) :
	$(MAKE) -C $(SRCTREE)/modules $(MAKECMDGOALS)

include $(SRCTREE)/build/build.mk
include $(SRCTREE)/build/install.mk
.PHONY: $(SUBDIRS)