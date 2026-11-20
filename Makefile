# Nextbot - PS4 homebrew (OpenOrbis toolchain)
# Runs inside a Linux environment with clang + lld and OO_PS4_TOOLCHAIN set
# (on Windows use build.cmd, which does this in Docker).
#
#   make            -> eboot.bin, sce_sys/param.sfo, pkg.gp4, pkg-orbis-pub-gen.gp4
#   make pkg        -> also the fake-signed .pkg (PkgTool.Core)
#   make host-test  -> builds and runs the PC test harness (screenshots in build/host)

TITLE       := Nextbot
VERSION     := 01.00
TITLE_ID    := NBOT00001
CONTENT_ID  := IV0000-NBOT00001_00-NEXTBOTPS4000000

LIBS        := -lc -lkernel -lc++ -lSceVideoOut -lScePad -lSceUserService -lSceAudioOut -lSceSystemService

TOOLCHAIN   := $(OO_PS4_TOOLCHAIN)
TOOLS       := $(TOOLCHAIN)/bin/linux
INTDIR      := build

# portable code (also compiled by the PC test harness)
COMMON_SRCS := $(wildcard src/game/*.cpp) $(wildcard src/gfx/*.cpp) \
               src/platform/fs.cpp src/platform/wav.cpp src/platform/mixer.cpp
# PS4-only platform layer
PS4_SRCS    := src/main.cpp src/platform/video.cpp src/platform/audio.cpp src/platform/input.cpp \
               src/platform/user.cpp src/platform/system.cpp
SRCS        := $(COMMON_SRCS) $(PS4_SRCS)
OBJS        := $(patsubst src/%.cpp,$(INTDIR)/%.o,$(SRCS))
HEADERS     := $(wildcard src/*/*.h)
SYSFILES    := sce_sys/about/right.sprx sce_sys/param.sfo sce_sys/icon0.png sce_sys/pic1.png
LIBMODULES  := $(wildcard sce_module/*)

CXX         := clang++
LD          := ld.lld

CXXFLAGS    := --target=x86_64-pc-freebsd12-elf -fPIC -funwind-tables -O2 -std=gnu++17 -Wall -Wno-unused-function \
               -DNEXTBOT_PS4 -isysroot $(TOOLCHAIN) -isystem $(TOOLCHAIN)/include -isystem $(TOOLCHAIN)/include/c++/v1
LDFLAGS     := -m elf_x86_64 -pie --script $(TOOLCHAIN)/link.x --eh-frame-hdr -L$(TOOLCHAIN)/lib $(LIBS) $(TOOLCHAIN)/lib/crt1.o

all: eboot.bin sce_sys/param.sfo pkg.gp4 pkg-orbis-pub-gen.gp4

pkg: $(CONTENT_ID).pkg

$(CONTENT_ID).pkg: pkg.gp4
	$(TOOLS)/PkgTool.Core pkg_build $< .

pkg.gp4: eboot.bin $(SYSFILES) $(LIBMODULES)
	$(TOOLS)/create-gp4 -out $@ --content-id=$(CONTENT_ID) --files "$^"

# orbis-pub-gen adds sce_sys/about itself and refuses a project that contains it;
# the empty "assets" tree that create-gp4 always adds is dropped too.
pkg-orbis-pub-gen.gp4: pkg.gp4
	sed -e '/about/d' -e '/targ_name="assets"/,/^\t\t<\/dir>/d' $< > $@

sce_sys/param.sfo: Makefile
	$(TOOLS)/PkgTool.Core sfo_new $@
	$(TOOLS)/PkgTool.Core sfo_setentry $@ APP_TYPE --type Integer --maxsize 4 --value 1
	$(TOOLS)/PkgTool.Core sfo_setentry $@ APP_VER --type Utf8 --maxsize 8 --value '$(VERSION)'
	$(TOOLS)/PkgTool.Core sfo_setentry $@ ATTRIBUTE --type Integer --maxsize 4 --value 0
	$(TOOLS)/PkgTool.Core sfo_setentry $@ CATEGORY --type Utf8 --maxsize 4 --value 'gd'
	$(TOOLS)/PkgTool.Core sfo_setentry $@ CONTENT_ID --type Utf8 --maxsize 48 --value '$(CONTENT_ID)'
	$(TOOLS)/PkgTool.Core sfo_setentry $@ DOWNLOAD_DATA_SIZE --type Integer --maxsize 4 --value 0
	$(TOOLS)/PkgTool.Core sfo_setentry $@ FORMAT --type Utf8 --maxsize 4 --value 'obs'
	$(TOOLS)/PkgTool.Core sfo_setentry $@ PARENTAL_LEVEL --type Integer --maxsize 4 --value 1
	$(TOOLS)/PkgTool.Core sfo_setentry $@ SYSTEM_VER --type Integer --maxsize 4 --value 0
	$(TOOLS)/PkgTool.Core sfo_setentry $@ TITLE --type Utf8 --maxsize 128 --value '$(TITLE)'
	$(TOOLS)/PkgTool.Core sfo_setentry $@ TITLE_ID --type Utf8 --maxsize 12 --value '$(TITLE_ID)'
	$(TOOLS)/PkgTool.Core sfo_setentry $@ VERSION --type Utf8 --maxsize 8 --value '$(VERSION)'

eboot.bin: $(OBJS)
	$(LD) $(OBJS) -o $(INTDIR)/nextbot.elf $(LDFLAGS)
	$(TOOLS)/create-fself -in=$(INTDIR)/nextbot.elf -out=$(INTDIR)/nextbot.oelf --eboot "eboot.bin" --paid 0x3800000000000011

$(INTDIR)/%.o: src/%.cpp $(HEADERS)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

# ---- PC test harness (Linux, inside the same container) ----
HOST_BIN    := $(INTDIR)/host/nextbot_host
host-test: $(HOST_BIN)
	$(HOST_BIN) $(INTDIR)/host

$(HOST_BIN): $(COMMON_SRCS) tools/host_main.cpp tools/platform_host.cpp $(HEADERS)
	@mkdir -p $(INTDIR)/host
	clang++ -std=gnu++17 -O1 -g -Wall -Wno-unused-function -fsanitize=address,undefined -Isrc \
	    $(COMMON_SRCS) tools/host_main.cpp tools/platform_host.cpp -o $@ -lpthread -lm

clean:
	rm -rf $(INTDIR) eboot.bin pkg.gp4 pkg-orbis-pub-gen.gp4 sce_sys/param.sfo $(CONTENT_ID).pkg

.PHONY: all pkg clean host-test
