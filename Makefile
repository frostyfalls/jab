include config.mk

PROTO = wlr-layer-shell-unstable-v1-protocol.h xdg-shell-protocol.h
SRC = jab.c buffer.c image-mode.c $(PROTO:.h=.c)
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
	rm -f jab $(OBJ) $(PROTO) $(PROTO:.h=.c)

install:
	install -Dm0755 jab $(DESTDIR)$(PREFIX)/bin

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/jab

.PHONY: all clean install uninstall
