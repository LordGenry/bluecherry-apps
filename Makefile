export topdir = $(CURDIR)
include mk.conf

all clean install: FORCE
	$(SUBDIR) misc
	$(SUBDIR) lib
	$(SUBDIR) server
	$(SUBDIR) utils
	$(SUBDIR) php5 -f Makefile.local

FORCE:
