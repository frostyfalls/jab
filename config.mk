VERSION = 0.1.0

PREFIX = /usr/local

WAYLAND_PROTOCOLS = /usr/share/wayland-protocols
WAYLAND_SCANNER = wayland-scanner

INCS = -I/usr/include/pixman-1
LIBS = -lpixman-1 -lwayland-client -lm

CPPFLAGS = -D_POSIX_C_SOURCE=200112L -DVERSION=\"$(VERSION)\"
CFLAGS = -std=c99 -Wall -Wno-deprecated-declarations -O2 $(INCS) $(CPPFLAGS)
#CFLAGS = -g -std=c99 -Wall -Wno-deprecated-declarations -O0 $(INCS) $(CPPFLAGS)
LDFLAGS = $(LIBS)

CC = cc
