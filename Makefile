rwildcard = $(foreach d, $(wildcard $1*), $(filter $(subst *, %, $2), $d) $(call rwildcard, $d/, $2))

ifeq ($(strip $(DEVKITARM)),)
$(error "Please set DEVKITARM in your environment. export DEVKITARM=<path to>devkitARM")
endif

include $(DEVKITARM)/base_rules

################################################################################

IPL_LOAD_ADDR := 0x40008000
LPVERSION_MAJOR := 3
LPVERSION_MINOR := 0
LPVERSION_BUGFX := 6
LPVERSION := \"$(LPVERSION_MAJOR).$(LPVERSION_MINOR).$(LPVERSION_BUGFX)\"

################################################################################

TARGET := TegraExplorer
BUILDDIR := build
OUTPUTDIR := output
SOURCEDIR = source
BDKDIR := bdk
BDKINC := -I./$(BDKDIR)
LOADERDIR := ./loader
LZ77DIR := ./tools/lz
BIN2CDIR := ./tools/bin2c
VPATH = $(dir ./$(SOURCEDIR)/) $(dir $(wildcard ./$(SOURCEDIR)/*/)) $(dir $(wildcard ./$(SOURCEDIR)/*/*/))
VPATH += $(dir $(wildcard ./$(BDKDIR)/)) $(dir $(wildcard ./$(BDKDIR)/*/)) $(dir $(wildcard ./$(BDKDIR)/*/*/))

OBJS =	$(patsubst $(SOURCEDIR)/%.S, $(BUILDDIR)/$(TARGET)/%.o, \
		$(patsubst $(SOURCEDIR)/%.c, $(BUILDDIR)/$(TARGET)/%.o, \
		$(call rwildcard, $(SOURCEDIR), *.S *.c)))
OBJS +=	$(patsubst $(BDKDIR)/%.S, $(BUILDDIR)/$(TARGET)/%.o, \
		$(patsubst $(BDKDIR)/%.c, $(BUILDDIR)/$(TARGET)/%.o, \
		$(call rwildcard, $(BDKDIR), *.S *.c)))

GFX_INC   := '"../$(SOURCEDIR)/gfx/gfx.h"'
FFCFG_INC := '"../$(SOURCEDIR)/libs/fatfs/ffconf.h"'

################################################################################

CUSTOMDEFINES := -DIPL_LOAD_ADDR=$(IPL_LOAD_ADDR)
CUSTOMDEFINES += -DLP_VER_MJ=$(LPVERSION_MAJOR) -DLP_VER_MN=$(LPVERSION_MINOR) -DLP_VER_BF=$(LPVERSION_BUGFX) -DLP_VER=$(LPVERSION)
CUSTOMDEFINES += -DGFX_INC=$(GFX_INC) -DFFCFG_INC=$(FFCFG_INC)

# 0: UART_A, 1: UART_B.
#CUSTOMDEFINES += -DDEBUG_UART_PORT=0

#CUSTOMDEFINES += -DDEBUG

ARCH := -march=armv4t -mtune=arm7tdmi -mthumb -mthumb-interwork
CFLAGS = $(ARCH) -Os -nostdlib -ffunction-sections -fdata-sections -fomit-frame-pointer -fno-inline -std=gnu11 -Wall -Wno-missing-braces $(CUSTOMDEFINES)
LDFLAGS = $(ARCH) -nostartfiles -lgcc -Wl,--nmagic,--gc-sections -Xlinker --defsym=IPL_LOAD_ADDR=$(IPL_LOAD_ADDR)

################################################################################

.PHONY: all clean

all: $(OUTPUTDIR)/$(TARGET)_small.bin
	$(eval BIN_SIZE = $(shell wc -c < $(OUTPUTDIR)/$(TARGET).bin))
	@echo "Payload size is $(BIN_SIZE)"
	$(eval COMPR_BIN_SIZE = $(shell wc -c < $(OUTPUTDIR)/$(TARGET)_small.bin))
	@echo "Compressed Payload size is $(COMPR_BIN_SIZE)"

	@echo "Max size is 126296 Bytes."
	@if [ ${BIN_SIZE} -gt 126296 ]; then echo "\e[1;33mPayload size exceeds limit!\e[0m"; fi
	@if [ ${COMPR_BIN_SIZE} -gt 126296 ]; then echo "\e[1;33mCompressed Payload size exceeds limit!\e[0m"; fi

clean:
	@rm -rf $(BUILDDIR)
	@rm -rf $(OUTPUTDIR)
	@rm -rf $(LOADERDIR)/payload_*.h

$(OUTPUTDIR)/$(TARGET)_small.bin: $(OUTPUTDIR)/$(TARGET).bin
	@$(MAKE) -C $(LZ77DIR)
	@$(LZ77DIR)/lz77 $(OUTPUTDIR)/$(TARGET).bin
	@$(MAKE) -C $(BIN2CDIR)
	@$(BIN2CDIR)/bin2c $(OUTPUTDIR)/$(TARGET).bin.00.lz payload_00 > $(LOADERDIR)/payload_00.h
	@$(BIN2CDIR)/bin2c $(OUTPUTDIR)/$(TARGET).bin.01.lz payload_01 > $(LOADERDIR)/payload_01.h
	@rm -rf $(OUTPUTDIR)/$(TARGET).bin.*.lz

	$(MAKE) -C $(LOADERDIR) PAYLOAD_NAME=$(TARGET)_small

$(OUTPUTDIR)/$(TARGET).bin: $(BUILDDIR)/$(TARGET)/$(TARGET).elf
	@mkdir -p "$(@D)"
	$(OBJCOPY) -S -O binary $< $@

$(BUILDDIR)/$(TARGET)/$(TARGET).elf: $(OBJS)
	$(CC) $(LDFLAGS) -T $(SOURCEDIR)/link.ld $^ -o $@

$(BUILDDIR)/$(TARGET)/%.o: $(SOURCEDIR)/%.c
	@mkdir -p "$(@D)"
	$(CC) $(CFLAGS) $(BDKINC) -c $< -o $@

$(BUILDDIR)/$(TARGET)/%.o: $(SOURCEDIR)/%.S
	@mkdir -p "$(@D)"
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILDDIR)/$(TARGET)/%.o: $(BDKDIR)/%.c
	@mkdir -p "$(@D)"
	$(CC) $(CFLAGS) $(BDKINC) -c $< -o $@

$(BUILDDIR)/$(TARGET)/%.o: $(BDKDIR)/%.S
	@mkdir -p "$(@D)"
	$(CC) $(CFLAGS) -c $< -o $@
	
$(SOURCEDIR)/script/builtin.c: scripts/*.te
	@py te2c.py source/script/builtin scripts
