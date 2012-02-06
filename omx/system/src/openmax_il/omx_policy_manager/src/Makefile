# Debug Flags:
#     0 - NO DEBUG MESSAGES
#     1 - DEBUG MESSAGES are enabled

OMX_DEBUG ?= 0      # master switch: turn debug on or off

SRC=\
	PolicyManager.c

HSRC=$(wildcard ../inc/*)

OBJ=$(SRC:.c=.o)

include $(OMXROOT)/Master.mk

CFLAGS+=-I../inc -I$(OMXINCLUDEDIR) -I$(BRIDGEINCLUDEDIR) -I$(SYSTEMINCLUDEDIR) -Wall -fpic -pipe -DOMAP_2430 

COMPONENT_SYS=OMXPolicyManager

COMPONENT_TARGET=$(SYSTEMDIR)/$(COMPONENT_SYS)

all install:: $(COMPONENT_TARGET)

omxcore $(COMPONENT_TARGET): $(SYSTEMDIR) $(COMPONENT_SYS) $(OMXLIBDIR) 
	@echo "Installing $(COMPONENT_SYS)"
	cp -f $(COMPONENT_SYS) $(COMPONENT_TARGET)
	cp -f policytable.tbl $(OMXTESTDIR)
ifeq ($(ENABLE_POLICY_DEBUG), 1)
	cp -f debugpolicytable.tbl $(OMXTESTDIR)/policytable.tbl
endif
	

$(COMPONENT_SYS): $(OBJ)
	$(CROSS)gcc $(CFLAGS) -o $(COMPONENT_SYS) $(OBJ) -L$(BRIDGELIBDIR) -lbridge $(LOMXPERFLIB)

$(SRC): $(HSRC)

clean:: 
	rm -f $(COMPONENT_SYS)
	rm -f $(OBJ)
	
distclean:: clean

clobber:: clean
	@echo "Clobbering $(SYSTEMDIR)/$(COMPONENT_SYS)"

	rm -f $(SYSTEMDIR)/$(COMPONENT_SYS)
	rm -f $(OMXTESTDIR)/policytable.tbl 

