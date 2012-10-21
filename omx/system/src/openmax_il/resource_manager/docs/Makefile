
HSRC=$(wildcard *34xx*.doc)
HTARGET=$(HSRC:%=$(OMXDOCDIR)/%)

include $(OMXROOT)/Master.mk

all install:: $(OMXDOCDIR) $(HTARGET) 

$(OMXDOCDIR)/%: % 
	cp -f $< $@


