# Cardputer UI simulator - plain make + g++, no PlatformIO, no SDL needed.
#   make          build build/cardputer_sim (downloads LovyanGFX once)
#   make run      run every scenario in scenarios/, screenshots in build/shots/
#   make clean
LGFX_VERSION ?= 1.2.7
BUILD        ?= build
LGFX_DIR     ?= $(BUILD)/LovyanGFX
CXX          ?= g++
CC           ?= gcc

CXXFLAGS ?= -O1 -g
CXXFLAGS += -std=c++17 -Wall -Wno-unused-function -DLGFX_USE_V1 -DLGFX_LINUX_FB -I$(LGFX_DIR)/src
CFLAGS   ?= -O1
CFLAGS   += -std=gnu17 -I$(LGFX_DIR)/src

CORE_SRC := $(wildcard ../core/*.cpp ../core/screens/*.cpp)
SIM_SRC  := $(wildcard *.cpp)
APP_OBJ  := $(patsubst ../core/%.cpp,$(BUILD)/core/%.o,$(CORE_SRC)) $(patsubst %.cpp,$(BUILD)/sim/%.o,$(SIM_SRC))

LGFX_STAMP := $(LGFX_DIR)/.stamp
LGFX_LIB   := $(BUILD)/liblgfx.a

SCENARIOS := $(wildcard scenarios/*.txt)

all: $(BUILD)/cardputer_sim

$(LGFX_STAMP):
	rm -rf $(LGFX_DIR)
	git clone -q --depth 1 --branch $(LGFX_VERSION) https://github.com/lovyan03/LovyanGFX $(LGFX_DIR)
	touch $@

# LovyanGFX is built once into a static archive (platform-independent parts + framebuffer stubs).
# Its CJK font tables are compiled too because lgfx_fonts.cpp references them all.
$(LGFX_LIB): $(LGFX_STAMP)
	@mkdir -p $(BUILD)/lgfx
	@set -e; for f in $$(find $(LGFX_DIR)/src/lgfx -name '*.cpp' -not -path '*/platforms/*') \
	                $(LGFX_DIR)/src/lgfx/v1/platforms/framebuffer/common.cpp; do \
	  o=$(BUILD)/lgfx/$$(echo $$f | sed 's|/|_|g').o; \
	  $(CXX) $(CXXFLAGS) -w -c $$f -o $$o; done
	@set -e; for f in $$(find $(LGFX_DIR)/src/lgfx -name '*.c'); do \
	  o=$(BUILD)/lgfx/$$(echo $$f | sed 's|/|_|g').o; \
	  $(CC) $(CFLAGS) -w -c $$f -o $$o; done
	ar rcs $@ $(BUILD)/lgfx/*.o

$(BUILD)/core/%.o: ../core/%.cpp $(LGFX_STAMP) $(wildcard ../core/*.h ../core/screens/*.h)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/sim/%.o: %.cpp $(LGFX_STAMP) $(wildcard *.h ../core/*.h ../core/screens/*.h)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/cardputer_sim: $(APP_OBJ) $(LGFX_LIB)
	$(CXX) -o $@ $(APP_OBJ) $(LGFX_LIB) -lpthread -Wl,--gc-sections

TILES_STAMP := $(BUILD)/maptiles/tiles/info.txt
$(TILES_STAMP): ../../../tools/tiles/make_tiles.py
	python3 ../../../tools/tiles/make_tiles.py --center 43.6116 1.3346 --radius-km 1.5 --zooms 13-15 --synthetic --out $(BUILD)/maptiles

run: $(BUILD)/cardputer_sim $(TILES_STAMP)
	@mkdir -p $(BUILD)/shots
	@set -e; for s in $(SCENARIOS); do echo "== $$s"; $(BUILD)/cardputer_sim $$s $(BUILD)/shots; done

clean:
	rm -rf $(BUILD)/core $(BUILD)/sim $(BUILD)/cardputer_sim $(BUILD)/shots

distclean:
	rm -rf $(BUILD)

.PHONY: all run clean distclean
