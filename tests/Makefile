
TEST_MAKE_SUBDIRS = xv6 host

all:
	@for dir in $(TEST_MAKE_SUBDIRS); do \
		$(MAKE) -C $$dir all; \
	done

clean:
	@for dir in $(TEST_MAKE_SUBDIRS); do \
		$(MAKE) -C $$dir clean; \
	done
