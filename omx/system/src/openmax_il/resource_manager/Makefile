

all::

# pull in the master "inherited" definitions
include $(OMXROOT)/Master.mk

DIRS= \
	resource_activity_monitor \
	src \
	docs \
	inc 
	

EXTRA=\
	docs \
	patterns \
	test


$(BASETARGETS)::
	@$(call traverse_dirs,$(DIRS),$@)

