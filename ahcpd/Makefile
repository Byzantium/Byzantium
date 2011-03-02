PREFIX = /usr/local

CDEBUGFLAGS = -Os -g -Wall

DEFINES = $(PLATFORM_DEFINES)

CFLAGS = $(CDEBUGFLAGS) $(DEFINES) $(EXTRA_DEFINES)

SRCS = ahcpd.c monotonic.c transport.c prefix.c configure.c config.c lease.c

OBJS = ahcpd.o monotonic.o transport.o prefix.o configure.o config.o lease.o

LDLIBS = -lrt

ahcpd: $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o ahcpd $(OBJS) $(LDLIBS)

.SUFFIXES: .man .html

.man.html:
	rman -f html $< | \
	sed -e "s|<a href='babeld.8'|<a href=\"../babel/babeld.html\"|" \
            -e "s|<a href='\\(ahcp[-a-z]*\\).8'|<a href=\"\1.html\"|" \
	    -e "s|<a href='[^']*8'>\\(.*(8)\\)</a>|\1|" \
	> $@

ahcpd.html: ahcpd.man

.PHONY: all install.minimal install

all: ahcpd

install.minimal: all
	mkdir -p $(TARGET)$(PREFIX)/bin/
	-rm -f $(TARGET)$(PREFIX)/bin/ahcpd
	cp ahcpd $(TARGET)$(PREFIX)/bin/
	mkdir -p $(TARGET)/etc/ahcp/
	-rm -f $(TARGET)/etc/ahcp/ahcp-config.sh
	cp ahcp-config.sh $(TARGET)/etc/ahcp/
	chmod +x $(TARGET)/etc/ahcp/ahcp-config.sh

install: all install.minimal
	mkdir -p $(TARGET)$(PREFIX)/man/man8/
	cp -f ahcpd.man $(TARGET)$(PREFIX)/man/man8/ahcpd.8

.PHONY: uninstall

uninstall:
	-rm -f $(TARGET)$(PREFIX)/bin/ahcpd
	-rm -f $(TARGET)$(PREFIX)/bin/ahcp-config.sh
	-rm -f $(TARGET)$(PREFIX)/bin/ahcp-dummy-config.sh
	-rm -f $(TARGET)$(PREFIX)/man/man8/ahcpd.8

.PHONY: clean

clean:
	-rm -f ahcpd
	-rm -f *.o *~ core TAGS gmon.out
	-rm -f ahcpd.html
