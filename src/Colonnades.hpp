#pragma once
// ── Colonnades (Micro-12) — tuning + scale AUTHORING expander (microtonal Phase 2, Model A) ──────
// A THIN subclass of MicroTuningModule with N_DEGREES = 12 (Option B shared base, Rodney). All the
// logic — root cents-lock, claim/publish cents[]+weight[]+maskAuthored, the widget, the four file
// ops, the context menu — lives in MicroTuning.hpp/.cpp. Colonnades only pins the degree count; its
// sibling ColonnadesDuo pins 24. See plans/colonnades_duo_micro24.md.

#include "MicroTuning.hpp"

namespace ColonnadesIds { static constexpr int N_DEGREES = 12; }

struct Colonnades : MicroTuningModule {
    Colonnades() : MicroTuningModule(ColonnadesIds::N_DEGREES) {}
};
