OPENTTD_BINARY ?= /usr/bin/openttd
HELPERSCRIPT = start_with_controller.sh

all: preload.so $(HELPERSCRIPT)

preload.so: preload.c
	$(CC) -Wall -Wextra -fPIC -shared `pkg-config --libs sdl` `pkg-config --cflags sdl` -ldl -o $@ $^

$(HELPERSCRIPT): preload.so
	echo "#!/bin/bash" >$@
	echo LD_PRELOAD=\"$(shell pwd)/$<\" \"$(OPENTTD_BINARY)\" >>$@
	chmod +x $@

clean:
	$(RM) preload.so $(HELPERSCRIPT)

.PHONY: clean

