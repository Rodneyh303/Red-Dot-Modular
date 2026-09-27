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

# ── Deterministic floating point for the copula / draw pipeline ──────────────────────────────
# BELT AND BRACES, NOT A BUG FIX — read the measurement before changing this.
#
# Rack's compile.mk adds -funsafe-math-optimizations to every plugin object, permitting
# reassociation (and FMA contraction). In principle the 64-term weighted sum in the slew/spread
# readout could then be summed in a different ORDER on a different platform or compiler version,
# and a last-bit difference near a lane threshold would flip a gate.
#
# MEASURED (2,000,000 comparisons, forward vs reverse summation order):
#   max |difference| from summation order : 4.996e-16
#   threshold flips                       : 0        (expected rate ~1 in 1e15 comparisons)
# At ~1e4 comparisons per phrase that is about one flipped gate every 1e11 phrases. So the
# reassociation risk is effectively nil, and nothing shipped was ever at risk.
#
# Kept anyway, because: (a) it is already here and costs ~4us; (b) -ffp-contract=off covers a
# DIFFERENT and UNMEASURED mechanism — x86 and ARM contract FMA differently, and that error on a
# 64-term sum may exceed 5e-16; and (c) "the same seed gives the same music, on any machine" is a
# headline claim, and shared demo patches / factory presets are where it becomes visible.
#
# DO NOT extend this plugin-wide — the measurement does not justify it.
# ROUNDING probabilities before the threshold compare does NOT help: it replaces one cliff edge
# with a million, and the wider tolerance is exactly cancelled by the extra boundaries.
#
# These lines must come AFTER `include $(RACK_DIR)/plugin.mk` so the object rule exists; the SDK
# compile line is `$(FLAGS) $(CXXFLAGS) ...`, so a target-specific CXXFLAGS addition lands LAST and
# wins (verified).
#   -fno-unsafe-math-optimizations : the umbrella
#   -fno-associative-math          : explicitly forbid re-ordering the sum
#   -ffp-contract=off              : no FMA contraction (the cross-platform mechanism above)
# Cost, measured: ~35% on the window readout (11.2us -> 15.2us), i.e. ~67us -> ~91us per frame while
# scrub-dragging — under 1% of a 60Hz budget.
DETERMINISTIC_FP := -fno-unsafe-math-optimizations -fno-associative-math -ffp-contract=off

$(BUILD)/src/dsp/engines/PatternEngine.cpp.o: CXXFLAGS += $(DETERMINISTIC_FP)
$(BUILD)/src/Monsoon.cpp.o:                   CXXFLAGS += $(DETERMINISTIC_FP)
