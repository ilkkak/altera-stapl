TOPDIR := $(shell pwd)

VERSION := 0.2

# install directories
PREFIX ?= /usr
BINDIR ?= $(PREFIX)/bin
DESTDIR ?=

ifeq ($(DEB_HOST_ARCH),arm64)
CC = /opt/gcc-linaro-x86_64_aarch64/bin/aarch64-linux-gnu-gcc
SYSROOT = /opt/Linux_for_Tegra/rootfs
else
CC = gcc
SYSROOT = /
endif

ifeq ($(shell test -d .git && echo 1),1)
VERSION := $(shell git describe --abbrev=8 --dirty --always --tags --long)
endif

EXTRA_CFLAGS = -g -Wall -Wno-pointer-sign -DVERSION=\"$(VERSION)\"
LDFLAGS += -lftdi -lusb
INCLUDES = -I/usr/include 
#$(shell pkg-config libusb-1.0 --cflags)

SOURCES := $(wildcard *.c)
OBJECTS := $(SOURCES:.c=.o)

programs = jbi

.PHONY: all install
all: altera-stapl

clean:
	rm -f $(OBJECTS)

%.o: %.c
	$(CC) -c $(INCLUDES) $< $(CFLAGS) $(EXTRA_CFLAGS)

altera-stapl: $(OBJECTS)
	$(CC) --sysroot=$(SYSROOT) -o $@ $^ $(LDFLAGS)

install: altera-stapl
	install -D -m 0755 altera-stapl $(DESTDIR)$(BINDIR)/altera-stapl
