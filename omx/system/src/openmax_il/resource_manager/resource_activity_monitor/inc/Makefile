
HSRC=Resource_Activity_Monitor.h
HTARGET=$(HSRC:%=$(SYSTEMINCLUDEDIR)/%)

include $(OMXROOT)/Master.mk

all install:: $(SYSTEMINCLUDEDIR) $(HTARGET) 

$(SYSTEMINCLUDEDIR)/%: % 
	cp -f $< $@

clobber::
	rm -f $(SYSTEMINCLUDEDIR)/Resource_Activity_Monitor.h
