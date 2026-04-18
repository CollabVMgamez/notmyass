KERNEL_DIR_DEFAULT := /usr/src/linux-headers-$(shell uname -r)
KERNEL_DIR_FALLBACK := /lib/modules/$(shell uname -r)/build
KERNEL_DIR ?= $(if $(wildcard $(KERNEL_DIR_FALLBACK)), $(KERNEL_DIR_FALLBACK), $(KERNEL_DIR_DEFAULT))
APP_SRCDIR := exe
DRIVER_SRCDIR := sys
PKG_SRCDIR := package
PKG_MUSL_DIR := package-musl

APP_SRC := $(APP_SRCDIR)/myass.c
APP_BIN := $(APP_SRCDIR)/myass
APP_BIN_MUSL := $(APP_SRCDIR)/myass-musl
DRIVER_KO := $(DRIVER_SRCDIR)/myass.ko

CFLAGS ?= -O2 -Wall -Wextra
LDFLAGS ?=
GUI_LDFLAGS := -lX11
MUSL_CC ?= $(shell command -v musl-gcc >/dev/null 2>&1 && echo musl-gcc || echo gcc)
MUSL_CFLAGS ?= -O2 -Wall -Wextra
MUSL_X11_CFLAGS := $(shell pkg-config --cflags x11 2>/dev/null)
MUSL_X11_LDFLAGS := $(shell pkg-config --libs x11 2>/dev/null)
MUSL_HAS_X11 := $(if $(strip $(MUSL_X11_LDFLAGS)),1,0)
MUSL_LDFLAGS_EXTRA ?= -static
MUSL_FORCE_NO_X11_GUI ?= 0

MUSL_GUI_LDFLAGS := $(MUSL_X11_LDFLAGS)
ifeq ($(MUSL_HAS_X11),0)
MUSL_GUI_LDFLAGS :=
endif

.PHONY: all build-driver build-app build-app-musl musl package package-musl clean

all: package

build-driver:
	$(MAKE) -C $(KERNEL_DIR) M=$(CURDIR)/$(DRIVER_SRCDIR) modules

build-app: $(APP_BIN)

$(APP_BIN): $(APP_SRC)
	gcc $(CFLAGS) -o $@ $(APP_SRC) $(GUI_LDFLAGS) $(LDFLAGS)

build-app-musl: $(APP_BIN_MUSL)

$(APP_BIN_MUSL): $(APP_SRC)
	@if ! command -v $(MUSL_CC) >/dev/null 2>&1; then \
		echo "MUSL compiler not found: $(MUSL_CC). Install a musl-capable compiler or set MUSL_CC." >&2; \
		exit 1; \
	fi
	@if [ "$(MUSL_FORCE_NO_X11_GUI)" = "1" ]; then \
		echo "Forcing X11-disabled MUSL build via MUSL_FORCE_NO_X11_GUI=1." >&2; \
		$(MUSL_CC) $(MUSL_CFLAGS) $(MUSL_X11_CFLAGS) -DFORCE_NO_X11_GUI \
			-o $@ $(APP_SRC) $(LDFLAGS) $(MUSL_LDFLAGS_EXTRA); \
		else \
			if [ "$(MUSL_HAS_X11)" != "1" ]; then \
				echo "No X11 development libraries found for pkg-config (x11). Building X11-disabled MUSL binary." >&2; \
				$(MUSL_CC) $(MUSL_CFLAGS) $(MUSL_X11_CFLAGS) -DFORCE_NO_X11_GUI \
					-o $@ $(APP_SRC) $(LDFLAGS) $(MUSL_LDFLAGS_EXTRA); \
		else \
				tmp_src=$$(mktemp /tmp/myass-x11-check-XXXX.c); \
				tmp_bin=$$(mktemp /tmp/myass-x11-check-XXXX); \
				printf '%s\n' '#include <X11/Xlib.h>' 'int main(void) { return XOpenDisplay(NULL) ? 0 : 0; }' > $$tmp_src; \
				if $(MUSL_CC) $(MUSL_CFLAGS) $(MUSL_X11_CFLAGS) $$tmp_src -o $$tmp_bin \
					$(MUSL_X11_LDFLAGS) >/tmp/myass-x11-check.log 2>&1; then \
					echo "X11 headers and libraries detected for MUSL; building GUI binary." >&2; \
					rm -f $$tmp_src $$tmp_bin; \
				$(MUSL_CC) $(MUSL_CFLAGS) $(MUSL_X11_CFLAGS) -o $@ $(APP_SRC) \
					$(MUSL_GUI_LDFLAGS) $(MUSL_LDFLAGS_EXTRA) $(LDFLAGS); \
			else \
				echo "MUSL X11 check failed (see /tmp/myass-x11-check.log). Building CLI-only MUSL binary." >&2; \
				rm -f $$tmp_src $$tmp_bin; \
				$(MUSL_CC) $(MUSL_CFLAGS) $(MUSL_X11_CFLAGS) -DFORCE_NO_X11_GUI \
					-o $@ $(APP_SRC) $(LDFLAGS) $(MUSL_LDFLAGS_EXTRA); \
			fi; \
		fi; \
	fi

package: build-driver build-app
	@mkdir -p $(PKG_SRCDIR)
	cp -f $(APP_BIN) $(PKG_SRCDIR)/
	cp -f $(DRIVER_KO) $(PKG_SRCDIR)/
	cp -f $(DRIVER_SRCDIR)/myass.c $(PKG_SRCDIR)/myass.c
	cp -f $(DRIVER_SRCDIR)/Makefile $(PKG_SRCDIR)/Makefile
	@echo "Created $(PKG_SRCDIR)/myass and $(PKG_SRCDIR)/myass.ko"

musl: build-driver build-app-musl
	@mkdir -p $(PKG_MUSL_DIR)
	cp -f $(APP_BIN_MUSL) $(PKG_MUSL_DIR)/
	cp -f $(DRIVER_KO) $(PKG_MUSL_DIR)/
	cp -f $(DRIVER_SRCDIR)/myass.c $(PKG_MUSL_DIR)/myass.c
	cp -f $(DRIVER_SRCDIR)/Makefile $(PKG_MUSL_DIR)/Makefile
	@echo "Created $(PKG_MUSL_DIR)/myass-musl and $(PKG_MUSL_DIR)/myass.ko"

clean:
	$(MAKE) -C $(KERNEL_DIR) M=$(CURDIR)/$(DRIVER_SRCDIR) clean
	rm -f \
		$(DRIVER_SRCDIR)/myass.o \
		$(DRIVER_SRCDIR)/myass.ko \
		$(DRIVER_SRCDIR)/myass.mod \
		$(DRIVER_SRCDIR)/myass.mod.c \
		$(DRIVER_SRCDIR)/myass.mod.o \
		$(DRIVER_SRCDIR)/myass.mod.symvers \
		$(DRIVER_SRCDIR)/Module.symvers \
		$(DRIVER_SRCDIR)/modules.order \
		$(DRIVER_SRCDIR)/.tmp_versions/*.*
	rm -f $(APP_SRCDIR)/*.o
	rm -f $(APP_BIN) $(APP_BIN_MUSL)
	rm -rf $(PKG_SRCDIR)
	rm -rf $(PKG_MUSL_DIR)
