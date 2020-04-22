ifndef QCONFIG
QCONFIG=qconfig.mk
endif
include $(QCONFIG)

#####################################
# Preset make macros go here
#####################################

EXTRA_SRCVPATH+=$(PROJECT_ROOT)/

EXTRA_INCVPATH+=$(PRODUCT_ROOT)/

# Use same flags from Jamfile to make a shared library
CCFLAGS += 

define PINFO
PINFO DESCRIPTION=JSON_Parser
endef
INSTALLDIR=usr/bin

NAME=json_parser

include $(MKFILES_ROOT)/qtargets.mk

#####################################
# Post-set make macros go here
#####################################
