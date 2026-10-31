NATIVE_CC ?= gcc

ifeq (, $(shell which $(NATIVE_CC) 2>/dev/null))
$(error "Native GCC is missing. Please install it first. If it's path is custom, set it with export NATIVE_CC=<path to native gcc toolchain>")
endif

.PHONY: all clean

all: nrv2e
	@echo > /dev/null

clean:
	@rm -f nrv2e

nrv2e: nrv.c nrv2e.c swd.c
	@$(NATIVE_CC) -g -o $@ nrv.c nrv2e.c swd.c