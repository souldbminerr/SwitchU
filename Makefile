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

# Build variant:
#   default    -> standalone homebrew .nro (QLAUNCHEXT_HOMEBREW, debug UI)
#   MENU=1     -> non-homebrew library-applet exefs (main NSO + main.npdm),
#                 loads its assets from the SD card (no embedded romfs)
#   DAEMON=1   -> the qlaunch ExeFS-override sysmodule (TID 0100000000001000).
#                 A normal libnx binary (int main / __appInit) that links the
#                 prebuilt libstratosphere.a for its ams::fs usage in ecs.cpp.
MENU   ?= 0
DAEMON ?= 0

# Prebuilt Atmosphère library (built by `make -C .../libstratosphere nx_release`).
STRAT_DIR    := $(TOPDIR)/lib/Atmosphere-libs/libstratosphere
STRAT_LIBDIR := $(STRAT_DIR)/lib/nintendo_nx_arm64_armv8a/release
MENU_NPDM_JSON   := $(TOPDIR)/projects/menu/menu.json
DAEMON_NPDM_JSON := $(TOPDIR)/projects/daemon/daemon.json
DATA     :=

ifeq ($(DAEMON),1)
    TARGET   := qlaunch-ext-daemon
    BUILD    := build_daemon
    SOURCES  := projects/daemon/src
    # relative to the project root ($(INCLUDE) prepends $(CURDIR))
    INCLUDES := projects/common/include \
                lib/Atmosphere-libs/libstratosphere/include \
                lib/Atmosphere-libs/libvapours/include
    ROMFS    :=
else
    ifeq ($(MENU),1)
        TARGET := qlaunch-ext-menu
        BUILD  := build_menu
        VARIANT_DEFINES := -DQLAUNCHEXT_MENU
    else
        TARGET := qlaunch-ext
        BUILD  := build
        VARIANT_DEFINES := -DQLAUNCHEXT_HOMEBREW -DQLAUNCHEXT_DEBUG_UI
    endif
    SOURCES  := projects/menu/src \
                projects/menu/src/bluetooth \
                projects/menu/src/core \
                projects/menu/src/debug \
                projects/menu/src/launcher \
                projects/menu/src/settings \
                projects/menu/src/settings/tabs \
                projects/menu/src/sidebar \
                projects/menu/src/themeshop \
                projects/menu/src/widgets \
                lib/nxui/src/core \
                lib/nxui/src/widgets \
                lib/nxui/src/focus \
                lib/vendor/imgui
    INCLUDES := projects/menu/src \
                projects/common/include \
                lib/nxui/include \
                lib/nxui/include/nxui/third_party/stb \
                lib/vendor/include \
                lib/vendor/imgui
    ROMFS    := romfs
endif

#---------------------------------------------------------------------------------
# options for code generation
#---------------------------------------------------------------------------------
ARCH    := -march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -fPIE

ifeq ($(DAEMON),1)
DEFINES := -D__SWITCH__ -DATMOSPHERE -DATMOSPHERE_IS_STRATOSPHERE \
           -DATMOSPHERE_OS_HORIZON -DATMOSPHERE_BOARD_NINTENDO_NX \
           -DATMOSPHERE_ARCH_ARM64 -DATMOSPHERE_ARCH_ARM_V8A -D_GNU_SOURCE
else
DEFINES := -D__SWITCH__ \
           $(VARIANT_DEFINES) \
           -DNXUI_BACKEND_DEKO3D \
           -DQLAUNCHEXT_VERSION=\"$(APP_VERSION)\"
endif

CFLAGS  := -g -Wall -O2 -ffunction-sections -fdata-sections \
           $(ARCH) $(DEFINES) $(INCLUDE)

ifeq ($(DAEMON),1)
CXXFLAGS := $(CFLAGS) -std=gnu++23 -fno-rtti -fexceptions
else
CXXFLAGS := $(CFLAGS) -std=gnu++20 -frtti -fexceptions
endif

ASFLAGS := -g $(ARCH)
LDFLAGS  = -specs=$(DEVKITPRO)/libnx/switch.specs -g $(ARCH) -Wl,-Map,$(notdir $*.map)
ifeq ($(DAEMON),1)
LDFLAGS += -L$(STRAT_LIBDIR)
endif

ifeq ($(DAEMON),1)
LIBS    := -lstratosphere -lz -lnx -lm
else
LIBS    := -ldeko3d \
           -lfmt \
           -lSDL2_mixer -lSDL2_ttf -lSDL2 \
           -lwebpdemux -lwebp \
           -lfreetype -lharfbuzz -lpng -lbz2 \
           -lmodplug -lmpg123 -lopusfile -lopus \
           -lvorbisfile -lvorbis -lvorbisidec -logg -lFLAC \
           -lEGL -lglapi -ldrm_nouveau \
           -lz -lnx -lm
endif

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

.PHONY: $(BUILD) clean distclean all menu daemon libstratosphere sd both

#---------------------------------------------------------------------------------
all: $(BUILD)

$(BUILD):
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile MENU=$(MENU) DAEMON=$(DAEMON)

# Build the non-homebrew library-applet exefs (main + main.npdm) + SD assets.
menu:
	@$(MAKE) --no-print-directory MENU=1

# Build the prebuilt Atmosphère library the daemon links against.
libstratosphere:
	@echo "building libstratosphere (first build can take several minutes) ..."
	@$(MAKE) --no-print-directory -C $(STRAT_DIR) nx_release

# Build the qlaunch ExeFS-override daemon (atmosphere/contents/<tid>/exefs.nsp).
daemon: libstratosphere
	@$(MAKE) --no-print-directory DAEMON=1

# Full drag-to-SD-root dist: menu applet + assets + daemon override.
sd: menu daemon

# Build homebrew + non-homebrew menu variants (no daemon).
both: all menu

#---------------------------------------------------------------------------------
clean:
	@echo clean ...
	@rm -fr build build_menu build_daemon dist \
	        qlaunch-ext.nro qlaunch-ext.nacp qlaunch-ext.elf \
	        qlaunch-ext-menu.nso qlaunch-ext-menu.elf qlaunch-ext-menu.nacp \
	        qlaunch-ext-daemon.nso qlaunch-ext-daemon.elf qlaunch-ext-daemon.nacp

# Also remove the compiled libstratosphere (slow to rebuild).
distclean: clean
	@$(MAKE) --no-print-directory -C $(STRAT_DIR) clean 2>/dev/null || true

#---------------------------------------------------------------------------------
else
.PHONY: all dist daemon-pack

DEPENDS := $(OFILES:.o=.d)

#---------------------------------------------------------------------------------
# main targets
#---------------------------------------------------------------------------------
ifeq ($(DAEMON),1)
# Daemon: ELF -> NSO + npdm(daemon.json) -> build_pfs0 -> the qlaunch ExeFS
# override at atmosphere/contents/0100000000001000/exefs.nsp (matches xmake's
# on_install for a format=nsp target with install_contents=true).
DIST            := $(TOPDIR)/dist
DAEMON_TID      := 0100000000001000
DAEMON_CONTENTS := $(DIST)/atmosphere/contents/$(DAEMON_TID)
DAEMON_EXEFS    := $(CURDIR)/exefs

all : daemon-pack

daemon-pack : $(OUTPUT).nso
	@echo "packaging daemon ExeFS override ..."
	@rm -rf $(DAEMON_EXEFS)
	@mkdir -p $(DAEMON_EXEFS) $(DAEMON_CONTENTS)
	@cp $(OUTPUT).nso $(DAEMON_EXEFS)/main
	@npdmtool $(DAEMON_NPDM_JSON) $(DAEMON_EXEFS)/main.npdm
	@build_pfs0 $(DAEMON_EXEFS) $(DAEMON_CONTENTS)/exefs.nsp
	@echo "daemon -> atmosphere/contents/$(DAEMON_TID)/exefs.nsp"

$(OUTPUT).nso : $(OUTPUT).elf
	@elf2nso $< $@
	@echo built ... $(notdir $@)

$(OUTPUT).elf : $(OFILES)
else ifeq ($(MENU),1)
# Non-homebrew menu: replicate the xmake `switch` rule's SD-card install layout
# (toolchain/switch.lua on_install) into a drag-to-SD-root dist/ tree. The menu
# is a library applet the daemon registers as external content (raw ExeFS dir),
# matching menu_launcher.hpp / SwitchMenuApp SD_ASSETS:
#   dist/switch/qlaunch-ext/bin/menu/{main,main.npdm}   (elf2nso + npdmtool)
#   dist/switch/qlaunch-ext/<assets>                    (SD assets)
# The ExeFS override that boots it is built by `make daemon`; use `make sd` for
# the complete dist (menu + assets + daemon).
DIST      := $(TOPDIR)/dist
MENU_EXE  := $(DIST)/switch/qlaunch-ext/bin/menu
ASSET_DIR := $(DIST)/switch/qlaunch-ext
DAEMON_TID    := 0100000000001000
DAEMON_PREBLT := $(TOPDIR)/dist-extra/daemon/exefs.nsp
# Asset subdirs the menu loads from SD (xmake's install list + the textures/
# themes the reskin added; espeak data is unused in this build).
ASSET_SUBDIRS := fonts icons textures themes sounds i18n shaders

all : dist

dist : $(OUTPUT).nso
	@echo "assembling dist/ (drag its contents onto the SD card root) ..."
	@rm -rf $(DIST)/switch/qlaunch-ext
	@mkdir -p $(MENU_EXE)
	@cp $(OUTPUT).nso $(MENU_EXE)/main
	@npdmtool $(MENU_NPDM_JSON) $(MENU_EXE)/main.npdm
	@for d in $(ASSET_SUBDIRS); do \
	    if [ -d "$(TOPDIR)/romfs/$$d" ]; then \
	        mkdir -p "$(ASSET_DIR)/$$d"; \
	        cp -rf "$(TOPDIR)/romfs/$$d/." "$(ASSET_DIR)/$$d/"; \
	    fi; \
	done
	@if [ -f "$(DAEMON_PREBLT)" ]; then \
	    mkdir -p "$(DIST)/atmosphere/contents/$(DAEMON_TID)"; \
	    cp "$(DAEMON_PREBLT)" "$(DIST)/atmosphere/contents/$(DAEMON_TID)/exefs.nsp"; \
	    echo "  + daemon ExeFS override -> atmosphere/contents/$(DAEMON_TID)/exefs.nsp"; \
	elif [ ! -f "$(DIST)/atmosphere/contents/$(DAEMON_TID)/exefs.nsp" ]; then \
	    echo "  ! no daemon override yet — run 'make daemon' (or 'make sd') for a bootable dist."; \
	fi
	@echo "dist ready."

$(OUTPUT).nso : $(OUTPUT).elf
	@elf2nso $< $@
	@echo built ... $(notdir $@)

$(OUTPUT).elf : $(OFILES)
else
all : $(OUTPUT).nro

$(OUTPUT).nro : $(OUTPUT).elf $(OUTPUT).nacp
$(OUTPUT).elf : $(OFILES)
endif

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
