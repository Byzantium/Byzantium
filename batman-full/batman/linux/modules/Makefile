PWD:=$(shell pwd)
ifeq ($(wildcard $(KERNELPATH)),)
KERNELPATH = /lib/modules/$(shell uname -r)/build
# sanity check: does KERNELPATH exist?
ifeq ($(shell cd $(KERNELPATH) && pwd),)
$(error $(KERNELPATH) is missing, please set KERNELPATH)
endif
export KERNELPATH
endif


UNAME:=$(shell uname -r)
LINUX26=2.6
LINUX_VERSION:=$(findstring $(LINUX26),$(UNAME))
REVISION:=	$(shell if [ -d .svn ]; then \
						if which svn > /dev/null; then \
							svn info | grep "Rev:" | sed -e '1p' -n | awk '{print $$4}'; \
						else \
							echo "[unknown]"; \
						fi ; \
					else \
						if [ -d ~/.svk ]; then \
							if which svk > /dev/null; then \
								echo $$(svk info | grep "Mirrored From" | awk '{print $$5}'); \
							else \
								echo "[unknown]"; \
							fi; \
						fi; \
					fi)

include $(PWD)/Makefile.kbuild

ifneq ($(LINUX_VERSION),$(LINUX26))
TARGET:=batgat
INCLUDE:=-I/lib/modules/$(UNAME)/build/include -I/usr/src/kernel-headers-$(UNAME)/include
EXTRA_CFLAGS+=-D__KERNEL__ -DMODULE -O2 -Wall $(INCLUDE)
CC:=gcc
endif

ifeq ($(LINUX_VERSION),$(LINUX26))

all:
	$(MAKE) -C $(KERNELPATH) REVISION=$(REVISION) M=$(PWD) PWD=$(PWD) modules

clean:
	$(MAKE) -C $(KERNELPATH) M=$(PWD) PWD=$(PWD) clean
else

clean:
	rm -f *.o *~
endif
