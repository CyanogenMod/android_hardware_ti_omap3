# Debug Flags:
#     0 - NO DEBUG MESSAGES
#     1 - DEBUG MESSAGES are enabled

OMX_DEBUG ?= 0      # master switch: turn debug on or off

SRC=\
	ResourceManagerProxy.c

HSRC=$(wildcard ../inc/*)

OBJ=$(SRC:.c=.o)

include $(OMXROOT)/Master.mk

CFLAGS+=-I../inc -I$(OMXINCLUDEDIR) -Wall -fpic -pipe

ifeq ($(ENABLE_RMPM_STUB), 1)
    CFLAGS += -D__ENABLE_RMPM_STUB__
endif

COMPONENT_LIB=libOMX_ResourceManagerProxy.so

COMPONENT_TARGET=$(OMXLIBDIR)/$(COMPONENT_LIB)

all install:: $(COMPONENT_TARGET)

omxcore $(COMPONENT_TARGET): $(SYSTEMDIR) $(COMPONENT_LIB) $(OMXLIBDIR) 
	@echo "Installing $(COMPONENT_SYS)"
	cp -f $(COMPONENT_LIB) $(COMPONENT_TARGET)

$(COMPONENT_LIB): $(OBJ)
	$(CROSS)gcc $(CFLAGS) -shared -o $(COMPONENT_LIB) $(OBJ) -L$(BRIDGELIBDIR) -lbridge -lpthread $(LOMXPERFLIB)

$(SRC): $(HSRC)

clean:: 
	rm -f $(COMPONENT_LIB)
	rm -f $(OBJ)
	
distclean:: clean

clobber:: clean
	rm -f $(OMXLIBDIR)/$(COMPONENT_LIB)
