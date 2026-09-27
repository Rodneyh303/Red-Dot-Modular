RACK_DIR ?= ../..

FLAGS += -Idep/include


# Link Time Optimization (LTO) can significantly improve performance
# by optimizing across your separate manager/engine files.
FLAGS += -flto -O3
LDFLAGS += -flto

# NOTE: -march=native was REMOVED. It targets the CPU doing the BUILD, which:
#   * is not supported by Clang on Apple Silicon (wants -mcpu=apple-m1), and
#   * on CI produces binaries tuned to whichever runner happened to serve the job,
#     which then crash with SIGILL on older machines.
# CI supplies a portable baseline itself (EXTRA_FLAGS="-march=x86-64-v2" in
# .github/workflows/build.yml). For a LOCAL tuned build, pass it on the command line
# instead of committing it:  make EXTRA_FLAGS="-march=native"
#
# NOTE: -ffast-math was REMOVED. It permits reassociation and assumes no NaN/Inf, which
# breaks two things this plugin depends on: the copula readout's BIT-EXACT reversibility
# (the K-term sum must be summed identically forward and backward — see
# docs/design/SLEW_COPULA_PLAN.md) and PhiInv's endpoint clamping. Do not reinstate.

SOURCES += $(wildcard src/*.cpp)
SOURCES += $(wildcard src/*.c)
SOURCES += $(wildcard src/dsp/engines/*.cpp)
SOURCES += $(wildcard src/dsp/managers/*.cpp)
SOURCES += $(wildcard src/dsp/gates/*.cpp)
DISTRIBUTABLES += res
DISTRIBUTABLES += $(wildcard LICENSE*)
DISTRIBUTABLES += $(wildcard presets)

include $(RACK_DIR)/plugin.mk

# C++17 (overrides Rack's default -std=c++11; lands after it so it wins).
# Required by the fold expression in src/ui/SvgPanelKit.hpp.
CXXFLAGS += -std=c++17

# FIX: Silences the specific diagnostic warning about fold-expressions
CXXFLAGS += -Wno-c++17-extensions