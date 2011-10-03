DIRS= \
	  build \
	  pkgs \

PACKAGES= $(shell cat *.pkglist)

all: $(DIRS) $(PACKAGES)

../%.xzm: $(PACKAGES) | $(DIRS)
	../../porteus-scripts/dir2xzm build $@

.PHONY: all clean

clean:
	git clean -fdx build

$(DIRS):
	mkdir -p $@

$(PACKAGES): %: pkgs/%.tgz build/var/log/packages/% | $(DIRS)

build/var/log/packages/%: pkgs/%.tgz | $(DIRS)
	ROOT=build /sbin/installpkg $<

pkgs/%: | $(DIRS)
	$(MAKE) -C src ../$@

Makefile: ;

