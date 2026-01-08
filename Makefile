.POSIX:

VERSION = 0.1.0

PKG_CONFIG = pkg-config
WAYLAND_PROTOCOLS = /usr/share/wayland-protocols
WAYLAND_SCANNER = wayland-scanner

PKGS = pixman-1 tllist wayland-client
INCS = `$(PKG_CONFIG) --cflags $(PKGS)`
LIBS = `$(PKG_CONFIG) --libs $(PKGS)`

CPPFLAGS = -D_POSIX_C_SOURCE=200809L -DVERSION=\"$(VERSION)\"
CFLAGS = -std=c99 -pedantic -Wall -Wno-deprecated-declarations -O3 $(INCS) $(CPPFLAGS)
LDFLAGS = $(LIBS)

PROTO = wlr-layer-shell-unstable-v1-protocol.h xdg-shell-protocol.h
SRC = jab.c buffer.c $(PROTO:.h=.c)
OBJ = $(SRC:.c=.o)

all: jab

$(OBJ): $(PROTO)

jab: $(OBJ)

wlr-layer-shell-unstable-v1-protocol.h:
	$(WAYLAND_SCANNER) client-header wlr-layer-shell-unstable-v1.xml $@

wlr-layer-shell-unstable-v1-protocol.c:
	$(WAYLAND_SCANNER) private-code wlr-layer-shell-unstable-v1.xml $@

xdg-shell-protocol.h:
	$(WAYLAND_SCANNER) client-header $(WAYLAND_PROTOCOLS)/stable/xdg-shell/xdg-shell.xml $@

xdg-shell-protocol.c:
	$(WAYLAND_SCANNER) private-code $(WAYLAND_PROTOCOLS)/stable/xdg-shell/xdg-shell.xml $@

clean:
	$(RM) jab $(OBJ) $(PROTO) $(PROTO:.h=.c)

.PHONY: all clean
