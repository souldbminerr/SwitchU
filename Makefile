#---------------------------------------------------------------------------------
# qlaunch-ext - plain devkitPro Makefile (homebrew .nro, deko3d backend)
#
# Differences from the stock libnx template:
#   * builds a standalone homebrew .nro (QLAUNCHEXT_HOMEBREW), never a sysmodule .nsp
#   * deko3d rendering backend (the SDL2 GpuDevice/Renderer/Texture files are filtered out)
#   * C++20 with RTTI + exceptions (nxui needs them)
#   * imgui debug overlay enabled (QLAUNCHEXT_DEBUG_UI); imgui core is vendored
#   * espeak-ng and curlpp have been removed from the project, so neither is built
#---------------------------------------------------------------------------------
.SUFFIXES:
#---------------------------------------------------------------------------------

ifeq ($(strip $(DEVKITPRO)),)
$(error "Please set DEVKITPRO in your environment. export DEVKITPRO=<path to>/devkitpro")
endif

TOPDIR ?= $(CURDIR)
include $(DEVKITPRO)/libnx/switch_rules

#---------------------------------------------------------------------------------
# app metadata (baked into the .nacp)
#---------------------------------------------------------------------------------
APP_TITLE   := qlaunch-ext
APP_AUTHOR  := Souldbminer and PoloNX
APP_VERSION := 1.0.0

TARGET   := qlaunch-ext
BUILD    := build
SOURCES  := projects/menu/src \
            projects/menu/src/bluetooth \
            projects/menu/src/core \
            projects/menu/src/debug \
            projects/menu/src/launcher \
            projects/menu/src/settings \
            projects/menu/src/settings/tabs \
            projects/menu/src/sidebar \
            projects/menu/src/themeshop \
            projects/menu/src/tutorial \
            projects/menu/src/widgets \
            lib/nxui/src/core \
            lib/nxui/src/widgets \
            lib/nxui/src/focus \
            lib/vendor/imgui
DATA     :=
INCLUDES := projects/menu/src \
            projects/common/include \
            lib/nxui/include \
            lib/nxui/include/nxui/third_party/stb \
            lib/vendor/include \
            lib/vendor/imgui
ROMFS    := romfs

#---------------------------------------------------------------------------------
# options for code generation
#---------------------------------------------------------------------------------
ARCH    := -march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -fPIE

DEFINES := -D__SWITCH__ \
           -DQLAUNCHEXT_HOMEBREW \
           -DNXUI_BACKEND_DEKO3D \
           -DQLAUNCHEXT_DEBUG_UI \
           -DQLAUNCHEXT_VERSION=\"$(APP_VERSION)\"

CFLAGS  := -g -Wall -O2 -ffunction-sections -fdata-sections \
           $(ARCH) $(DEFINES) $(INCLUDE)

CXXFLAGS := $(CFLAGS) -std=gnu++20 -frtti -fexceptions

ASFLAGS := -g $(ARCH)
LDFLAGS  = -specs=$(DEVKITPRO)/libnx/switch.specs -g $(ARCH) -Wl,-Map,$(notdir $*.map)

LIBS    := -ldeko3d \
           -lSDL2_mixer -lSDL2_ttf -lSDL2 \
           -lwebpdemux -lwebp \
           -lfreetype -lharfbuzz -lpng -lbz2 \
           -lmodplug -lmpg123 -lopusfile -lopus \
           -lvorbisfile -lvorbis -lvorbisidec -logg -lFLAC \
           -lEGL -lglapi -ldrm_nouveau \
           -lz -lnx -lm

#---------------------------------------------------------------------------------
# list of directories containing libraries, this must be the top level containing
# include and lib
#---------------------------------------------------------------------------------
LIBDIRS := $(PORTLIBS) $(LIBNX)

#---------------------------------------------------------------------------------
# no real need to edit anything past this point unless you need to add additional
# rules for different file extensions
#---------------------------------------------------------------------------------
ifneq ($(BUILD),$(notdir $(CURDIR)))
#---------------------------------------------------------------------------------

export OUTPUT   := $(CURDIR)/$(TARGET)
export TOPDIR   := $(CURDIR)

export VPATH    := $(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
                   $(foreach dir,$(DATA),$(CURDIR)/$(dir))

export DEPSDIR  := $(CURDIR)/$(BUILD)

CFILES   := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
SFILES   := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))
BINFILES := $(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.*)))

# Drop the SDL2 rendering backend: this build uses deko3d.
CPPFILES := $(filter-out %_sdl2.cpp,$(CPPFILES))

#---------------------------------------------------------------------------------
# use CXX for linking C++ projects, CC for standard C
#---------------------------------------------------------------------------------
ifeq ($(strip $(CPPFILES)),)
	export LD := $(CC)
else
	export LD := $(CXX)
endif

export OFILES_BIN := $(addsuffix .o,$(BINFILES))
export OFILES_SRC := $(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(SFILES:.s=.o)
export OFILES     := $(OFILES_BIN) $(OFILES_SRC)
export HFILES_BIN := $(addsuffix .h,$(subst .,_,$(BINFILES)))

export INCLUDE := $(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
                  $(foreach dir,$(LIBDIRS),-I$(dir)/include) \
                  -I$(CURDIR)/$(BUILD)

export LIBPATHS := $(foreach dir,$(LIBDIRS),-L$(dir)/lib)

export APP_ICON := $(if $(wildcard $(TOPDIR)/icon.jpg),$(TOPDIR)/icon.jpg,$(LIBNX)/default_icon.jpg)
export NROFLAGS += --icon=$(APP_ICON)
export NROFLAGS += --nacp=$(CURDIR)/$(TARGET).nacp

ifneq ($(ROMFS),)
	export NROFLAGS += --romfsdir=$(CURDIR)/$(ROMFS)
endif

.PHONY: $(BUILD) clean all

#---------------------------------------------------------------------------------
all: $(BUILD)

$(BUILD):
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

#---------------------------------------------------------------------------------
clean:
	@echo clean ...
	@rm -fr $(BUILD) $(TARGET).nro $(TARGET).nacp $(TARGET).elf

#---------------------------------------------------------------------------------
else
.PHONY: all

DEPENDS := $(OFILES:.o=.d)

#---------------------------------------------------------------------------------
# main targets
#---------------------------------------------------------------------------------
all : $(OUTPUT).nro

$(OUTPUT).nro : $(OUTPUT).elf $(OUTPUT).nacp
$(OUTPUT).elf : $(OFILES)

$(OFILES_SRC) : $(HFILES_BIN)

#---------------------------------------------------------------------------------
# you need a rule like this for each extension you use as binary data
#---------------------------------------------------------------------------------
%.bin.o %_bin.h : %.bin
	@echo $(notdir $<)
	@$(bin2o)

-include $(DEPENDS)

#---------------------------------------------------------------------------------------
endif
#---------------------------------------------------------------------------------------
