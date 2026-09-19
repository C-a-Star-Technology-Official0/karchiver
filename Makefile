# Makefile لـ Karchiver
# حقوق النشر (C) 2026 شركة C.a. Star Technology
# SPDX-License-Identifier: GPL-3.0-or-later

CC       ?= gcc
CFLAGS   ?= -O2 -Wall -Wextra
PREFIX   ?= /usr/local
BINDIR   := $(PREFIX)/bin
MANDIR   := $(PREFIX)/share/man/man1

all: karchiver

karchiver: src/karchiver.c
	$(CC) $(CFLAGS) -o $@ $<

install: karchiver
	install -Dm755 karchiver $(DESTDIR)$(BINDIR)/karchiver
	install -Dm644 man/karchiver.1 $(DESTDIR)$(MANDIR)/karchiver.1

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/karchiver
	rm -f $(DESTDIR)$(MANDIR)/karchiver.1

clean:
	rm -f karchiver

.PHONY: all install uninstall clean

DESKTOPDIR := $(PREFIX)/share/applications
MIMEDIR    := $(PREFIX)/share/mime/packages

install-desktop: data/karchiver.desktop
	install -Dm644 data/karchiver.desktop \
	    $(DESTDIR)$(DESKTOPDIR)/karchiver.desktop
	-update-desktop-database $(DESTDIR)$(DESKTOPDIR) 2>/dev/null || true

install-all: install install-desktop

.PHONY: install-desktop install-all
ICONDIR := $(PREFIX)/share/icons/hicolor/scalable/apps

install-icon: data/icons/hicolor/scalable/apps/karchiver.svg
	install -Dm644 $< $(DESTDIR)$(ICONDIR)/karchiver.svg
	-gtk-update-icon-cache -f -t $(DESTDIR)$(PREFIX)/share/icons/hicolor 2>/dev/null || true

install-all: install install-desktop install-icon

.PHONY: install-icon
