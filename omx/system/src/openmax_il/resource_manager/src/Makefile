# Debug Flags:
#     0 - NO DEBUG MESSAGES
#     1 - DEBUG MESSAGES are enabled

OMX_DEBUG ?= 0      # master switch: turn debug on or off

SRC=\
	ResourceManager.c

HSRC=$(wildcard ../inc/*)

OBJ=$(SRC:.c=.o)

include $(OMXROOT)/Master.mk

CFLAGS+=-I../inc -I$(OMXINCLUDEDIR) -I$(BRIDGEINCLUDEDIR) -I$(SYSTEMINCLUDEDIR) -Wall -fpic -pipe

ifeq ($(ENABLE_RMPM_STUB), 1)
    CFLAGS += -D__ENABLE_RMPM_STUB__
endif


COMPONENT_SYS=OMXResourceManager

COMPONENT_TARGET=$(SYSTEMDIR)/$(COMPONENT_SYS)

all install:: $(COMPONENT_TARGET)

omxcore $(COMPONENT_TARGET): $(SYSTEMDIR) $(COMPONENT_SYS) $(OMXLIBDIR) 
	@echo "Installing $(COMPONENT_SYS)"
	cp -f $(COMPONENT_SYS) $(COMPONENT_TARGET)

$(COMPONENT_SYS): $(OBJ)
	$(CROSS)gcc $(CFLAGS) -o $(COMPONENT_SYS) $(OBJ) -L$(BRIDGELIBDIR) -lbridge $(LOMXPERFLIB) -lRAM -lqos

$(SRC): $(HSRC)

clean:: 
	rm -f $(COMPONENT_SYS)
	rm -f $(OBJ)
	
distclean:: clean

clobber:: clean
	rm -f $(SYSTEMDIR)/$(COMPONENT_SYS)
