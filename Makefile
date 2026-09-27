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
# Rack's own compile.mk adds -funsafe-math-optimizations to EVERY plugin object. That permits
# reassociation (and FMA contraction), so the K-term weighted sum in the slew/spread readout can be
# summed in a different ORDER depending on platform, compiler version or optimisation decisions.
# Within one binary that is still deterministic — so session-level reverse is safe either way — but
# it breaks the stronger promise this plugin is built on: the SAME SEED GIVES THE SAME MUSIC on any
# machine. A patch saved on Windows must replay bit-identically on macOS.
#
# So: re-disable unsafe math for the two translation units that instantiate the copula. These lines
# must come AFTER `include $(RACK_DIR)/plugin.mk` so the object rule exists; the SDK's compile line
# is `$(FLAGS) $(CXXFLAGS) ...`, so a target-specific CXXFLAGS addition lands LAST and wins
# (verified: the later -fno- flag does override the earlier -f).
#
#   -fno-unsafe-math-optimizations  : the umbrella
#   -fno-associative-math           : explicitly forbid re-ordering the sum
#   -ffp-contract=off               : no FMA contraction (x86 and ARM contract differently, which
#                                     would change rounding across platforms)
#
# Cost, measured: ~35% on the window readout (11.2us -> 15.2us per window). At 2 scrub windows x 3
# streams that is ~67us -> ~91us per frame while scrub-dragging — still well under 1% of a 60Hz
# budget. Cheap insurance for the one property the instrument advertises.
DETERMINISTIC_FP := -fno-unsafe-math-optimizations -fno-associative-math -ffp-contract=off

$(BUILD)/src/dsp/engines/PatternEngine.cpp.o: CXXFLAGS += $(DETERMINISTIC_FP)
$(BUILD)/src/Monsoon.cpp.o:                   CXXFLAGS += $(DETERMINISTIC_FP)
