
CFLAGS +=
LDFLAGS +=

PLUGINS = $(shell find  -maxdepth 1 -type d | grep -e '^./bmx6_' | sort)

%:
	for d in $(PLUGINS); do $(MAKE) CORE_CFLAGS='$(CORE_CFLAGS)' -C $$d $@ || echo compiling $$d failed; echo;echo; done
        echo 2 flags: $(CFLAGS)


