MODULES= \
		 tor.xzm \
		 mupdf.xzm \
		 babel.xzm \
		 php.xzm \
		 nginx.xzm \
		 batman.xzm \
		 erlang.xzm \

all: $(MODULES)

.PHONY: all clean $(MODULES)

$(MODULES):
	$(MAKE) -C $(@:%.xzm=%) ../$@

clean:
	rm -f $(MODULES)

