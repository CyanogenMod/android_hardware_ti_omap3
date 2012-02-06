
HSRC=ResourceManagerProxyAPI.h 

HTARGET=$(HSRC:%=$(SYSTEMINCLUDEDIR)/%)

include $(OMXROOT)/Master.mk

all install:: $(SYSTEMINCLUDEDIR) $(HTARGET) 

$(SYSTEMINCLUDEDIR)/%: % 
	cp -f $< $@

clobber::
	rm -f $(SYSTEMINCLUDEDIR)/ResourceManagerProxyAPI.h




