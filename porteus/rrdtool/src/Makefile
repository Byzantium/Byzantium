PKGS= $(shell cat ../*.pkglist)

all: $(PKGS:%=../pkgs/%.tgz)

.PHONY: all

../pkgs/%.tgz:
	sh ../../build.sh $(shell echo $(@:../pkgs/%=%) | rev | cut -d - -f 4- | rev)

