#pragma once
// ── Colonnades Duo (Micro-24) — tuning + scale AUTHORING expander (microtonal Phase 3, Model A) ───
// A THIN subclass of MicroTuningModule with N_DEGREES = 24 (Option B shared base, Rodney). Identical
// logic to Colonnades — the only differences are the degree count (24), one-row layout, and panel.
// One row of 24 (MONSOON_MICRO_SPEC §34: honest to arbitrary 24-tone .scl, no 12+12 split). Same
// one-Micro-per-Monsoon delegation as Colonnades. See plans/colonnades_duo_micro24.md.

#include "MicroTuning.hpp"

namespace ColonnadesDuoIds { static constexpr int N_DEGREES = 24; }

struct ColonnadesDuo : MicroTuningModule {
    ColonnadesDuo() : MicroTuningModule(ColonnadesDuoIds::N_DEGREES) {}
};
