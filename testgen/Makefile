DIRS = $(filter-out $(wildcard *.*) Makefile,$(wildcard *))

.PHONY:  all clean

all:
	@for i in $(DIRS) ; do \
		$(MAKE) -C $$i all || { echo "Make: Error (`pwd`)"; exit 1; } ; \
	done

clean:
	@for i in $(DIRS) ; do \
		$(MAKE) -C $$i clean || { echo "Make: Error (`pwd`)"; exit 1; } ; \
	done
