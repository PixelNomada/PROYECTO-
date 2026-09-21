.SUFFIXES:

ifeq ($(strip $(DEVKITARM)),)
$(error "Please set DEVKITARM in your environment")
endif

include $(DEVKITARM)/ds_rules

# =========================================================
# CONFIGURACIÓN DSi IA
# =========================================================

TARGET := DSi-IA
BUILD := build
SOURCES := source
INCLUDES := include build
NITRO := data

ARCH := -march=armv5te -mtune=arm946e-s -mthumb

CFLAGS := -g -Wall -O2 -ffunction-sections -fdata-sections $(ARCH)
CFLAGS += $(INCLUDE) -DARM9

CXXFLAGS := $(CFLAGS) -fno-rtti -fno-exceptions

ASFLAGS := -g $(ARCH)

LDFLAGS := -specs=ds_arm9.specs -g $(ARCH) -Wl,-Map,$(notdir $*.map)

# =========================================================
# IMPORTANTE:
# El proyecto usa C++, por lo que el enlazador debe ser
# arm-none-eabi-g++ y NO "ld".
# =========================================================

export LD := $(CXX)

# =========================================================
# LIBRERÍAS Nintendo DS
# =========================================================

LIBS := -lfilesystem -lfat -lnds9
LIBDIRS := $(LIBNDS) $(PORTLIBS)

# =========================================================
# PROYECTO PRINCIPAL
# =========================================================

ifneq ($(BUILD),$(notdir $(CURDIR)))

export OUTPUT := $(CURDIR)/$(TARGET)

export NITRO_FILES := $(CURDIR)/$(NITRO)

export VPATH := $(foreach dir,$(SOURCES),$(CURDIR)/$(dir))

export DEPSDIR := $(CURDIR)/$(BUILD)

CFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))

CPPFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))

SFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))

export OFILES := \
	$(CPPFILES:.cpp=.o) \
	$(CFILES:.c=.o) \
	$(SFILES:.s=.o)

export INCLUDE := \
	$(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
	$(foreach dir,$(LIBDIRS),-I$(dir)/include) \
	-I$(CURDIR)/$(BUILD)

export LIBPATHS := \
	$(foreach dir,$(LIBDIRS),-L$(dir)/lib)

.PHONY: $(BUILD) clean

$(BUILD):
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

clean:
	@rm -fr $(BUILD) $(TARGET).elf $(TARGET).nds $(TARGET).ds.gba

else

# =========================================================
# COMPILACIÓN INTERNA
# =========================================================

DEPENDS := $(OFILES:.o=.d)

$(OUTPUT).nds: $(OUTPUT).elf $(NITRO_FILES)

$(OUTPUT).elf: $(OFILES)

-include $(DEPENDS)

endif
