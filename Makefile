RACK_DIR ?= ../..

FLAGS += -Idep/include
# C++17 (overrides Rack's default -std=c++11; lands after it so it wins).
# Required by the fold expression in src/ui/SvgPanelKit.hpp.
CXXFLAGS += -std=c++17

# FIX: Silences the specific diagnostic warning about fold-expressions
CXXFLAGS += -Wno-c++17-extensions

# Link Time Optimization (LTO) can significantly improve performance 
# by optimizing across your separate manager/engine files.
FLAGS += -flto -O3 -ffast-math -march=native
LDFLAGS += -flto

SOURCES += $(wildcard src/*.cpp)
SOURCES += $(wildcard src/*.c)
SOURCES += $(wildcard src/dsp/engines/*.cpp)
SOURCES += $(wildcard src/dsp/managers/*.cpp)
SOURCES += $(wildcard src/dsp/gates/*.cpp)
DISTRIBUTABLES += res
DISTRIBUTABLES += $(wildcard LICENSE*)
DISTRIBUTABLES += $(wildcard presets)

include $(RACK_DIR)/plugin.mk
