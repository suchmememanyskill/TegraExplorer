ifeq ($(strip $(DEVKITARM)),)
$(error "Please set DEVKITARM in your environment. export DEVKITARM=<path to>devkitARM")
endif

include $(DEVKITARM)/base_rules

################################################################################

LDR_LOAD_ADDR   := 0x40007000
IPL_MAGIC       := 0x54454558 # TEEX
TEVERSION_MAJOR := 5
TEVERSION_MINOR := 0
TEVERSION_BUGFX := 0
TEVERSION       := \"$(TEVERSION_MAJOR).$(TEVERSION_MINOR).$(TEVERSION_BUGFX)\"

################################################################################

TARGET    := loader
BUILDDIR  := ../build/$(TARGET)
OUTPUTDIR := ../output
BDKDIR    := bdk
BDKINC    := -I../$(BDKDIR)

VPATH += $(dir $(wildcard ../$(BDKDIR)/*/)) $(dir $(wildcard ../$(BDKDIR)/*/*/)) $(dir $(wildcard ../$(BDKDIR)/*/*/*/))

# Main and graphics.
OBJS = $(addprefix $(BUILDDIR)/, \
	start.o loader.o nrv2e.o \
)

################################################################################

CUSTOMDEFINES := -DTE_MAGIC=$(IPL_MAGIC)
CUSTOMDEFINES += -DTE_VER_MJ=$(TEVERSION_MAJOR) -DTE_VER_MN=$(TEVERSION_MINOR) -DTE_VER_HF=$(TEVERSION_BUGFX) -DTE_VER=$(TEVERSION)

#TODO: Considering reinstating some of these when pointer warnings have been fixed.
WARNINGS := -Wall -Wsign-compare -Wno-array-bounds -Wno-stringop-overflow

ARCH   := -march=armv4t -mtune=arm7tdmi -mthumb-interwork
CFLAGS  = $(ARCH) -Oz -g -nostdlib -ffunction-sections -fdata-sections -fomit-frame-pointer -std=gnu11 $(WARNINGS) $(CUSTOMDEFINES)
LDFLAGS = $(ARCH) -nostartfiles -lgcc -Wl,--nmagic,--gc-sections -Xlinker --defsym=LDR_LOAD_ADDR=$(LDR_LOAD_ADDR)

################################################################################

.PHONY: all clean

all: $(TARGET).bin $(TOOLSLZ) $(TOOLSB2C)

clean:
	@rm -f payload.h

$(TARGET).bin: $(BUILDDIR)/$(TARGET).elf
	@$(OBJCOPY) -S -O binary $< $(OUTPUTDIR)/$(PAYLOAD_NAME).bin

$(BUILDDIR)/$(TARGET).elf: $(OBJS)
	@$(CC) $(LDFLAGS) -T link.ld $^ -o $@

$(BUILDDIR)/%.o: %.c
	@$(CC) $(CFLAGS) $(BDKINC) -c $< -o $@

$(BUILDDIR)/%.o: %.S
	@$(CC) $(CFLAGS) -c $< -o $@

$(OBJS): $(BUILDDIR)

$(BUILDDIR):
	@mkdir -p "$(BUILDDIR)"
	@mkdir -p "$(OUTPUTDIR)"