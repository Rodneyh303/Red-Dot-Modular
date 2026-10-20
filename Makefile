RACK_DIR ?= ../..

FLAGS += -Idep/include

# The Rack SDK's headers (rack.hpp, helpers.hpp) are THIRD-PARTY: include them as SYSTEM
# headers so THEIR diagnostics don't flood the build. Under C++20 the SDK's own [=]-implicit-
# this captures in helpers.hpp deprecation-warn for every TU, and each such TU then ALSO gets
# GCC's "unrecognized -Wno-vla-extension" note (the SDK passes that clang-only flag; GCC only
# prints the note when the TU produced other diagnostics) — one -isystem kills both families.
# Our own sources keep full -Wall -Wextra. This must come BEFORE `include $(RACK_DIR)/plugin.mk`:
# the SDK's compile.mk later adds plain -I on the same dirs, and the EARLIER -isystem copy wins
# header resolution (search order), so the headers are found as system headers.
FLAGS += -isystem $(RACK_DIR)/include
FLAGS += -isystem $(RACK_DIR)/dep/include


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

# C++20 (confirmed CI-green on lin/mac/win 2026-10 via test/cpp20-probe: full engine+SIMD+simde+libRack link all clean). Overrides Rack SDK default (c++17). Enables concepts (SandsModifierResolver policy contracts) + the SvgPanelKit fold expr.
# Required by the fold expression in src/ui/SvgPanelKit.hpp.
CXXFLAGS += -std=c++20

# FIX: silences the fold-expr c++17-extension warning. Under C++20 fold exprs are standard so this
# MAY be removable now (C++20 move 2026-10) — left in (harmless) pending a CI-green confirmation without it.
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
DETERMINISTIC_FP := -ffp-contract=off   # DECIDED: dropped the reassociation guard (-fno-unsafe-math-optimizations -fno-associative-math) — ~nil measured value (~1 in 1e15), ~most of the cost; validated green on all 3 platforms. Keep -ffp-contract=off: cross-platform FMA determinism, which CI cannot verify per-platform. See docs/design/DETERMINISTIC_FP_COST_NOTE.md.

$(BUILD)/src/dsp/engines/PatternEngine.cpp.o: CXXFLAGS += $(DETERMINISTIC_FP) -mavx
$(BUILD)/src/Monsoon.cpp.o:                   CXXFLAGS += $(DETERMINISTIC_FP)


# NOTE on SIMD: the x86_64 slice gets SSE4.1 from Rack's own -march=nehalem (compile.mk,
# ARCH_X64) — Nehalem includes SSE4.1/4.2 — so NO extra -msse4.1 is needed for Philox.
# -mavx is added TARGET-SPECIFICALLY to PatternEngine.cpp.o above: it enables the AVX1
# 4-wide double (_mm256_mul/add/load/store) path in MovingAverageCopula::sumZ. AVX1 is
# Sandy Bridge (2011+) — the same baseline as Rack's own -march=nehalem targets (Nehalem
# is 2008; AVX is one generation later, but all x86_64 VCV Rack users have AVX). The arm64
# slice uses SIMDe NEON emulation (no x86 flag). AVX-512 path auto-activates if the compiler
# supports -mavx512f (future; not enabled by default).
