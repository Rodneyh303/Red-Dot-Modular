#pragma once
// MonsoonChangeAlleyV2 — 16×16 pin-matrix expander
// Rows = consuming voices (0=mono/V1, 1..15=poly V2..V16)
// Columns = source voices (same indexing)
// Two pin types per cell: white=rhythm, red=melody (concentric when both)
// Row-radio: exactly one rhythm pin and one melody pin per row.
// Up to 16 rows may share the same column (fan-in is the musical point).
// Default: identity diagonal (rhythmSrc[v]=v, melodySrc[v]=v).
// Does NOT require Straits — operates at the Philox table level.
// Zero param slots by design (DAW_PARAM_AUDIT.md).

#include <rack.hpp>
#include <cmath>
#include <cstdio>
#include <atomic>
#include "Monsoon.hpp"
#include "ui/VisualExpanderHelpers.hpp"
#include "ui/ModArcOverlay.hpp"
#include "ui/StoreEditAction.hpp"   // pin edits: store-backed, undoable (DAW_PARAM_AUDIT 5b)
#include "dsp/ChangeAlleyTransforms.hpp"   // ca::applyCorrelation (transform apply owned here)
#include "ui/IntertropicalPairing.hpp"     // shared pairing: assignPairIdT / resolveFollowedT<T>
#include "ui/ConnectMark.hpp"              // shared dot.modular connect indicator (same as other panels)
#include "ui/SvgPanelKit.hpp"             // Option B-full: bind ports/params/lights by name from anchors

using namespace rack;
// NOT 'using namespace ChangeAlleyIds' — Monsoon.hpp exposes MonsoonIds with the same
// NUM_PARAMS/NUM_INPUTS/... names, so we qualify explicitly (same rule as the Sands
// managers: 'NOT using namespace ... to avoid ambiguous calls — qualify below').
namespace CA = ChangeAlleyV2Ids;

struct MonsoonChangeAlleyV2 : Module {
    uint8_t rhythmSrc[CA::N_VOICES];
    uint8_t melodySrc[CA::N_VOICES];
    // Q-MIX source-select plane (QMIX_LANE_PARITY §"The blend"): the NEW green plane, a full
    // parity sibling of rhythm(white)/melody(red). Row-radio like the other two: qmixSrc[v] holds
    // the ONE source column voice v consumes for its q-mix PROBABILITY. Downstream of CA the
    // per-voice blend mux reads caQmixSrc[v] to scatter WHICH voice's q-mix each voice thresholds
    // on. Default identity (qmixSrc[v]=v) → the Straits per-voice level reads exactly as before.
    // The PANEL's physical q-mix pin ROW is a later layer (CA_PANEL_THREE_STREAM_LAYOUT.md); this
    // field + its persistence/undo/scatter are wired NOW so the panel layer only adds click+render.
    uint8_t qmixSrc[CA::N_VOICES];

    // ── Shared-CA pairing (CA_SHARED_EXPANDER_BUILD.md) ──────────────────────────────────────
    // Self-assigned lowest-free number (1..N) so a second Monsoon can bind this CA by id, rack-wide
    // (mirrors Intertropical::pairId). Assigned lazily in process() — NOT ctor (getModuleIds re-lock
    // deadlock). Persisted; immutable once set; gaps ok. 0 = not yet assigned.
    int pairId = 0;
    // One-shot latch: the rack-wide getModuleIds() clash-scan below runs EXACTLY ONCE per module
    // lifetime (mirrors Intertropical::pairChecked). Without this the scan ran every sample — an
    // O(rack size) walk at 48 kHz — the CPU spike. Runtime only (not persisted).
    bool pairChecked = false;
    // Owner guard for the shared case: applyPendingTransforms MUTATES this CA's pins, so only ONE
    // Monsoon may call it per block (the first sync() caller = the owner). Reset at the top of
    // process() each block; the ExpanderManager sets it after applying. Runtime only (not persisted).
    bool transformsAppliedThisBlock = false;

    static constexpr const char* CURRENCIES[CA::N_VOICES] = {
        "SGD","MYR","IDR","THB","PHP","VND","MMK","KHR",
        "HKD","CNY","TWD","KRW","JPY","AUD","INR","USD",
    };

    // ── Transforms (§15): the full Temasek control set, in one module ──────────────────
    CA::PendingAction pendingRows[CA::N_ROWS];
    rack::dsp::SchmittTrigger domTrig  [CA::N_ROWS];
    rack::dsp::SchmittTrigger codTrig  [CA::N_ROWS];
    rack::dsp::BooleanTrigger btnTrig  [CA::N_ROWS * 2];
    rack::dsp::SchmittTrigger sBackDom [CA::SIDES * CA::TYPES];
    rack::dsp::SchmittTrigger sBackCod [CA::SIDES * CA::TYPES];
    // Button twins of the back-jacks: domain + codomain reverse buttons (scatterDelta = -1).
    rack::dsp::BooleanTrigger sRevBtnDom [CA::SIDES * CA::TYPES];
    rack::dsp::BooleanTrigger sRevBtnCod [CA::SIDES * CA::TYPES];
    // TRUE-REVERSE (CA_DICE_COUNTER_MODEL): one jack + one button per STREAM (rhythm/melody/q-mix =
    // TYPES = 3), VERB-AGNOSTIC. The committed pin STATE is one whole-matrix array per stream, so a
    // single control per stream restores both Intra/Inter AND domain/codomain (they're baked into the
    // recorded state) across all four verbs. Distinct from Philox dice-reverse (axis-specific,
    // scatterDelta=-1): true-reverse walks the committed pin-state TRAJECTORY backward (phrase-
    // granular). Clocked/performance = modulation-class (no undo push).
    // NOTE: the trajectory-replay ENGINE (a deeper state-history buffer, per the doc's true-reverse
    // proposal + buffer-size section) is a SEPARATE build; here we add the CONTROLS + trigger
    // detection + a per-stream pending request flag. See trueRevRequested[] below.
    rack::dsp::SchmittTrigger  sTrueRevIn  [CA::TYPES];
    rack::dsp::BooleanTrigger  sTrueRevBtn [CA::TYPES];
    // Set when a true-reverse jack/button fires (index = stream/type); drained by the future engine.
    bool trueRevRequested[CA::TYPES] = {};
    // Scatter draw counters: 8 = Intra/Inter x rhythm/melody x domain/codomain (the panel's separate
    // scatter jacks). Each is a SIGNED int64 addressable POSITION in its own domain-separated Philox
    // stream -- the SAME model as the main dice draw counters. Forward jack = counter++, back jack =
    // counter-- (negative allowed; Philox is a keyed bijection). The transform draws rng.at(position)
    // so at(N-1) returns the previous draw EXACTLY -- no reseeding. See CA_DICE_COUNTER_MODEL.md.
    // Sized N_SCATTER = SIDES*SCATTER_TYPES*2 = 12 (was 8): rhythm/melody/q-mix × dom/cod ×
    // intra/inter. SCATTER_TYPES=3 (not TYPES=2) so q-mix gets CA scatter parity without touching
    // the panel row/param geometry. Indexing: ci = (side*SCATTER_TYPES + type)*2 + (dom?0:1),
    // type 0=rhythm 1=melody 2=qmix — kept consistent with corrKey + transform apply below.
    int64_t scatterCounter[CA::N_SCATTER] = {};

    // One Philox KEY per scatter stream (8 = the counters' Intra/Inter x r/m x dom/cod), mirroring
    // the 2 main-dice RNGs. INTERNAL seeding = 8 INDEPENDENT random keys (different per stream, like
    // dice's seed*PhiloxFull). EXTERNAL-seed sharing is TBD (same open question as dice). The scatter
    // draw builds a transient PhiloxRng from corrKey[ci] and reads at(scatterCounter[ci]) -- the
    // counter is the addressable position, so counter-- rewinds exactly. Keys persist so saved
    // patches reproduce future scatters. See CA_DICE_COUNTER_MODEL.md.
    uint64_t corrKey[CA::N_SCATTER] = {};
    void seedCorrKeysInternal() {
        for (int i = 0; i < CA::N_SCATTER; ++i) corrKey[i] = rack::random::u64();
    }
    // Derive all correlation keys from an external seed value (0..10) supplied by the
    // adjacent (owner) Monsoon on its reset+reseed gesture. The SAME seed value that seeds
    // rhythm + melody -> one source, three stream families (rhythm/melody/CA scatter). The
    // per-index offset (STREAM_CA + i) keeps the 8 scatter streams independent. Two CAs fed
    // the same seed derive identical corrKey[] -> identical scatter (cross-instance sharing).
    // See PHILOX_KEY_DERIVATION_AND_CA_SEED.md Finding 2.
    void reseedCorrKeys(float seedValue) {
        for (int i = 0; i < CA::N_SCATTER; ++i)
            corrKey[i] = redDot::seed::deriveKey(seedValue, redDot::seed::STREAM_CA + (uint64_t)i);
    }

    // --- Transform-undo groundwork (item 5) ---------------------------------------------------
    // applyPendingTransforms() runs on the AUDIO thread (control-rate, Monsoon::process), where
    // APP->history->push is ILLEGAL (UI-thread only). So we snapshot the pre-transform pin state
    // into a small single-producer/single-consumer RING here, and the widget's step() (UI thread)
    // drains it and pushes a Rack history action per committed transform. Transform commits are
    // infrequent (phrase boundaries), so 16 slots is ample.
    struct TransformUndoSnapshot {
        uint8_t  beforeR[CA::N_VOICES];
        uint8_t  beforeM[CA::N_VOICES];
        uint8_t  beforeQ[CA::N_VOICES];   // q-mix plane (parity with R/M)
        uint8_t  afterR[CA::N_VOICES];
        uint8_t  afterM[CA::N_VOICES];
        uint8_t  afterQ[CA::N_VOICES];
        int64_t counterBefore[CA::N_SCATTER];
        int64_t counterAfter [CA::N_SCATTER];
    };
    static constexpr int UNDO_RING = 16;
    TransformUndoSnapshot undoRing[UNDO_RING];
    std::atomic<uint32_t> undoHead{0};   // producer (audio) writes, then advances
    std::atomic<uint32_t> undoTail{0};   // consumer (UI) reads, then advances

    MonsoonChangeAlleyV2() {
        config(CA::NUM_PARAMS_TOTAL, CA::NUM_INPUTS, 0, CA::NUM_LIGHTS);
        static const char* VN[CA::N_VERBS] = {"Collapse","Rotate","Reflect","Scatter"};
        static const char* SN[CA::SIDES]   = {"Intra","Inter"};
        static const char* PN[CA::TYPES]   = {"Rhythm","Melody","Q-mix"};
        static const char* GL[] = {"1","2","4","8","16"};
        for (int v = 0; v < CA::N_VERBS; ++v)
          for (int sd = 0; sd < CA::SIDES; ++sd)
            for (int ty = 0; ty < CA::TYPES; ++ty) {
                const int r = CA::rowId(v, sd, ty);
                const std::string nm = std::string(VN[v]) + " " + SN[sd] + " " + PN[ty];
                configSwitch(CA::GRAIN_START + r, 0.f, 4.f, 2.f, nm + " grain",
                             {GL[0],GL[1],GL[2],GL[3],GL[4]});
            }
        for (int r = 0; r < CA::SIDES * CA::TYPES; ++r) {   // one leader/step per side×type
            configParam(CA::LEADER_START + r, 0.f, 15.f, 0.f, "Leader offset")->snapEnabled = true;
            configParam(CA::STEP_START   + r, -7.f, 7.f, 1.f, "Step")->snapEnabled = true;
        }
        for (int i = 0; i < CA::N_ROWS * 2; ++i)
            configButton(CA::BTN_START + i, (i % 2 == 0) ? "Domain trigger" : "Codomain trigger");
        for (int r = 0; r < CA::N_ROWS; ++r) {
            configInput(CA::DOMAIN_TRIG_START   + r, "Domain trigger");
            configInput(CA::CODOMAIN_TRIG_START + r, "Codomain trigger");
        }
        for (int i = 0; i < CA::SIDES * CA::TYPES; ++i) {
            configInput(CA::SCATTER_BACK_DOM_START + i, "Scatter domain back");
            configInput(CA::SCATTER_BACK_COD_START + i, "Scatter codomain back");
            configButton(CA::SCATTER_REV_BTN_START + i,                       "Scatter domain reverse");
            configButton(CA::SCATTER_REV_BTN_START + CA::SIDES*CA::TYPES + i, "Scatter codomain reverse");
        }
        // True-reverse: ONE jack + ONE button per STREAM (verb-agnostic, restores the whole per-
        // stream state trajectory backward). PN[ty] = Rhythm/Melody/Q-mix.
        for (int ty = 0; ty < CA::TYPES; ++ty) {
            configButton(CA::TRUE_REV_BTN_START + ty, std::string("True reverse ") + PN[ty]);
            configInput (CA::TRUE_REV_IN_START  + ty, std::string("True reverse ") + PN[ty] + " trigger");
        }
        // GRAIN_POLY_IN / STEP_POLY_IN removed (CA_PANEL_THREE_STREAM_LAYOUT): didn't scale to
        // the 3rd stream; the per-row grain/leader/step knobs remain the sole value source.
        resetToIdentity();
        seedCorrKeysInternal();   // fresh module: no seed known yet, entropy keys are correct
    }

    static int grainFromKnob(float v) {
        static const int B[5] = {1,2,4,8,16};
        int i = (int)std::lround(v); if (i < 0) i = 0; if (i > 4) i = 4;
        return B[i];
    }

    // Poly CV read with MONO NORMALLING: a 1-channel cable drives ALL channels (the standard
    // Rack idiom -- a mono LFO into a poly mod input modulates everything equally).
    static float polyCV(rack::engine::Input& in, int channel) {
        if (!in.isConnected()) return 0.f;
        return (in.getChannels() <= 1) ? in.getVoltage(0) : in.getVoltage(channel);
    }

    void latchRow(int r, int verb, int side, int type, bool domain) {
        auto& p    = pendingRows[r];
        p.armed    = true;
        p.isDomain = domain;
        p.isInter  = (side == 1);
        // Grain = knob + poly CV (channel = row). No attenuverter (§ Rodney): 16 channels
        // map straight to the 16 grain knobs. CV is added in knob-detent units (0..4).
        // Grain from the per-row knob only (poly-CV mod removed, CA_PANEL_THREE_STREAM_LAYOUT).
        float gv = params[CA::GRAIN_START + r].getValue();
        p.grain    = grainFromKnob(gv);
        if      (verb == CA::V_COLLAPSE) {
            const int li = side*CA::TYPES + type;
            float lv = params[CA::LEADER_START + li].getValue();
            p.leaderOrStep = (int)std::lround(lv);
        }
        else if (verb == CA::V_ROTATE)
            {   const int si = side*CA::TYPES + type;
                float sv = params[CA::STEP_START + si].getValue();
                p.leaderOrStep = (int)std::lround(sv); }
        else
            p.leaderOrStep = 0;
        if (verb == CA::V_SCATTER) p.scatterDelta = 1;
        lights[CA::PENDING_LIGHT_START + r].setBrightness(1.f);
    }

    // Apply all ARMED pending transforms to this module's own pin matrix. Owns the state
    // mutation (rhythmSrc/melodySrc + scatterCounter) -- the manager only decides WHEN to call
    // this (phrase boundary / unlock). Moved out of MonsoonExpanderManager so the module that
    // holds the state also owns its mutation (and, next, its undo snapshot). `active` is the
    // active voice count (numPolyVoices+1, clamped >=1).
    // axisMask (LOCK_SCOPE_MENU §6): which axes may commit THIS call. bit0 = rhythm rows (type==0),
    // bit1 = melody rows (type==1), bit2 = q-mix rows (type==2). Default 0b111 = all (normal unlock/
    // boundary fire). Under a live-under-lock scatter, the manager passes only the opted-live axis, so
    // out-of-axis armed rows stay pending (they commit later at the real unlock/boundary). Preserves
    // the "one commit = one undo" rule per fire: the snapshot brackets exactly the rows applied THIS
    // call. Q-mix bit (0b100) is parity groundwork: the CURRENT panel only produces rhythm/melody rows
    // (type 0/1), so type==2 rows only arrive once the panel layer adds the q-mix pin row — the mask +
    // apply handle it now so no engine change is needed then.
    void applyPendingTransforms(int active, unsigned axisMask = 0b111u) {
        // axis bit for a row's type: 0=rhythm→0b001, 1=melody→0b010, 2=qmix→0b100.
        auto axisBitForType = [](int type) -> unsigned { return 1u << type; };
        // TRUE-REVERSE consume at the PHRASE BOUNDARY (same commit gesture as the verbs): a queued
        // per-stream request commits + clears its pending lamp here. Gated by the SAME axisMask as
        // verbs (bit ty = stream). The trajectory-replay ENGINE is deferred (CA_DICE_COUNTER_MODEL:
        // buffer depth + momentary/toggle open); this consumes the queue + lamp with correct
        // boundary timing so the affordance matches the verbs now. When the engine lands it walks
        // the committed-state trajectory back one entry per consumed request (modulation-class: it
        // must NOT push undo). Runs before the verb early-return so it fires even with no armed rows.
        for (int ty = 0; ty < CA::TYPES; ++ty) {
            if (!trueRevRequested[ty]) continue;
            if (!(axisMask & axisBitForType(ty))) continue;   // out-of-axis: stay queued (like verbs)
            trueRevRequested[ty] = false;
            lights[CA::TRUE_REV_LIGHT_START + ty].setBrightness(0.f);
            // TODO(true-reverse engine): step this stream's committed-state trajectory back by one.
        }
        // Any armed row this call whose AXIS is in the mask?  If none, nothing to snapshot or apply.
        bool any = false;
        for (int row = 0; row < CA::N_ROWS; ++row) {
            if (!pendingRows[row].armed) continue;
            if (axisMask & axisBitForType(row % CA::TYPES)) { any = true; break; }  // panel rows: type = row % TYPES
        }
        if (!any) return;

        // Snapshot BEFORE (whole pin matrix + scatter counters). One phrase-boundary commit = one
        // undo step (mirrors ResetPinsAction: a multi-change gesture is a single snapshot).
        TransformUndoSnapshot snap;
        for (int v = 0; v < CA::N_VOICES; ++v) { snap.beforeR[v] = rhythmSrc[v]; snap.beforeM[v] = melodySrc[v]; snap.beforeQ[v] = qmixSrc[v]; }
        for (int i = 0; i < CA::N_SCATTER; ++i) snap.counterBefore[i] = scatterCounter[i];

        for (int row = 0; row < CA::N_ROWS; ++row) {
            auto& p = pendingRows[row];
            if (!p.armed) continue;
            // Decode (verb,side,type) from row using the current dims — NOT hardcoded 4/2
            // (rowId = verb*SIDES*TYPES + side*TYPES + type; TYPES=3 now).
            const int verb = row / (CA::SIDES * CA::TYPES);
            const int side = (row / CA::TYPES) % CA::SIDES;
            const int type = row % CA::TYPES;   // 0=rhythm 1=melody 2=q-mix (panel 3rd stream)
            // SCOPE (LOCK_SCOPE_MENU §6): only commit rows whose axis is in axisMask. Out-of-axis rows
            // stay armed (NOT applied, NOT cleared) so they fire at the next in-axis/unlock commit.
            if (!(axisMask & axisBitForType(type))) continue;
            // Table + scatter index by type: 0=rhythm 1=melody 2=qmix (parity, kept consistent with the
            // corrKey/scatterCounter ordering). ci uses SCATTER_TYPES so the q-mix streams (type 2) are
            // addressable NOW even though the current panel only fires type 0/1.
            uint8_t* tbl   = (type == 0) ? rhythmSrc : (type == 1) ? melodySrc : qmixSrc;
            const int ci   = (side * CA::SCATTER_TYPES + type) * 2 + (p.isDomain ? 0 : 1);
            if (verb == CA::V_SCATTER)
                scatterCounter[ci] += (int64_t)p.scatterDelta;   // +1 fwd jack, -1 back jack
            dotModular::ca::applyCorrelation(
                verb, p.isDomain, p.isInter,
                tbl, active, p.grain, p.leaderOrStep,
                corrKey[ci], scatterCounter[ci]);   // fixed stream key + addressable position
            p.armed = false;
            lights[CA::PENDING_LIGHT_START + row].setBrightness(0.f);
        }

        // Snapshot AFTER, and publish to the ring for the UI thread to turn into a history action.
        for (int v = 0; v < CA::N_VOICES; ++v) { snap.afterR[v] = rhythmSrc[v]; snap.afterM[v] = melodySrc[v]; snap.afterQ[v] = qmixSrc[v]; }
        for (int i = 0; i < CA::N_SCATTER; ++i) snap.counterAfter[i] = scatterCounter[i];
        const uint32_t h = undoHead.load(std::memory_order_relaxed);
        const uint32_t t = undoTail.load(std::memory_order_acquire);
        if (h - t < (uint32_t)UNDO_RING) {           // drop if UI hasn't drained (never in practice)
            undoRing[h % UNDO_RING] = snap;
            undoHead.store(h + 1, std::memory_order_release);
        }
    }

    void process(const ProcessArgs&) override {
        // Shared-CA pairing: assign a stable pairId ONCE (0 = unassigned, or a clash from a
        // duplicated/pasted module carrying its origin's id). Done here, not the ctor, to avoid the
        // getModuleIds re-lock deadlock; guarded by pairChecked so the rack-wide scan runs exactly
        // once per lifetime — NOT every sample. Mirrors Intertropical.cpp:51-63.
        if (!pairChecked) {
            pairChecked = true;
            bool clash = false;
            if (APP && APP->engine) {
                for (int64_t id : APP->engine->getModuleIds()) {
                    rack::Module* m = APP->engine->getModule(id);
                    if (!m || m == this) continue;
                    if (auto* ca = dynamic_cast<MonsoonChangeAlleyV2*>(m))
                        if (ca->pairId == pairId) { clash = true; break; }
                }
            }
            if (pairId <= 0 || clash) pairId = redDot::assignPairIdT<MonsoonChangeAlleyV2>(this);
        }
        // Owner guard: reset each block; the first ExpanderManager::sync() that applies transforms
        // sets it, so a second (reader) Monsoon skips the mutation. See CA_SHARED_EXPANDER_BUILD §Step4.
        transformsAppliedThisBlock = false;

        for (int v = 0; v < CA::N_VERBS; ++v)
          for (int sd = 0; sd < CA::SIDES; ++sd)
            for (int ty = 0; ty < CA::TYPES; ++ty) {
                const int r = CA::rowId(v, sd, ty);
                if (domTrig[r].process(inputs[CA::DOMAIN_TRIG_START + r].getVoltage(), 0.1f, 1.f))
                    latchRow(r, v, sd, ty, true);
                if (codTrig[r].process(inputs[CA::CODOMAIN_TRIG_START + r].getVoltage(), 0.1f, 1.f))
                    latchRow(r, v, sd, ty, false);
                if (btnTrig[r*2].process(params[CA::BTN_START + r*2].getValue() > 0.5f))
                    latchRow(r, v, sd, ty, true);
                if (btnTrig[r*2+1].process(params[CA::BTN_START + r*2+1].getValue() > 0.5f))
                    latchRow(r, v, sd, ty, false);
            }
        for (int sd = 0; sd < CA::SIDES; ++sd)
          for (int ty = 0; ty < CA::TYPES; ++ty) {
            const int i = sd * CA::TYPES + ty;
            const int r = CA::rowId(CA::V_SCATTER, sd, ty);
            if (sBackDom[i].process(inputs[CA::SCATTER_BACK_DOM_START + i].getVoltage(), 0.1f, 1.f)) {
                latchRow(r, CA::V_SCATTER, sd, ty, true);  pendingRows[r].scatterDelta = -1;
            }
            if (sBackCod[i].process(inputs[CA::SCATTER_BACK_COD_START + i].getVoltage(), 0.1f, 1.f)) {
                latchRow(r, CA::V_SCATTER, sd, ty, false); pendingRows[r].scatterDelta = -1;
            }
            // Reverse BUTTONS: identical action to the back-jacks (scatterDelta = -1).
            if (sRevBtnDom[i].process(params[CA::SCATTER_REV_BTN_START + i].getValue() > 0.5f)) {
                latchRow(r, CA::V_SCATTER, sd, ty, true);  pendingRows[r].scatterDelta = -1;
            }
            if (sRevBtnCod[i].process(params[CA::SCATTER_REV_BTN_START + CA::SIDES*CA::TYPES + i].getValue() > 0.5f)) {
                latchRow(r, CA::V_SCATTER, sd, ty, false); pendingRows[r].scatterDelta = -1;
            }
          }
        // TRUE-REVERSE (CA_DICE_COUNTER_MODEL): one jack + one button PER STREAM (verb-agnostic;
        // NOT per side/dom-cod). Sets a per-stream request flag. The trajectory-replay ENGINE
        // (deeper state-history buffer walked backward, phrase-granular; buffer depth + momentary/
        // toggle are the doc's open design questions) is a SEPARATE build that will DRAIN this flag.
        // Modulation-class: when built, its commit must NOT push undo history (clocked/performance).
        for (int ty = 0; ty < CA::TYPES; ++ty) {
            // Trigger/button ARMS the per-stream true-reverse (queued), lighting its pending lamp —
            // reusing the SAME pending-lamp + phrase-boundary-commit gesture as the other CA verbs.
            // Re-press while queued is a NO-OP re-arm (matches latchRow's idempotent verb re-arm:
            // it re-sets armed=true without cancelling), NOT a cancel.
            bool fired = false;
            if (sTrueRevIn [ty].process(inputs[CA::TRUE_REV_IN_START + ty].getVoltage(), 0.1f, 1.f)) fired = true;
            if (sTrueRevBtn[ty].process(params[CA::TRUE_REV_BTN_START + ty].getValue() > 0.5f))       fired = true;
            if (fired) {
                trueRevRequested[ty] = true;
                lights[CA::TRUE_REV_LIGHT_START + ty].setBrightness(1.f);   // queued (pending) lamp
            }
        }
    }

    // STRUCTURAL reset only: pin matrix -> identity, scatter counters -> 0. Does NOT re-key.
    // Reseeding (changing which Philox stream you draw from) is a SEPARATE gesture, triggered
    // by an explicit reset+reseed from Monsoon (reseedCorrKeys). Conflating them would silently
    // change scatter streams on every matrix reset. See PHILOX_KEY_DERIVATION_AND_CA_SEED.md.
    void resetToIdentity() {
        for (int v = 0; v < CA::N_VOICES; ++v) { rhythmSrc[v] = v; melodySrc[v] = v; qmixSrc[v] = v; }
        for (int i = 0; i < CA::N_SCATTER; ++i) scatterCounter[i] = 0;
        // NO key re-derivation here (moved out — see comment above).
    }


    json_t* dataToJson() override {
        json_t* root = json_object();
        auto save = [&](const char* k, const uint8_t* a) {
            json_t* arr = json_array();
            for (int v = 0; v < CA::N_VOICES; ++v) json_array_append_new(arr, json_integer(a[v]));
            json_object_set_new(root, k, arr);
        };
        save("rhythmSrc", rhythmSrc);
        save("melodySrc", melodySrc);
        save("qmixSrc",   qmixSrc);   // q-mix source-select plane (parity with rhythm/melody)
        json_object_set_new(root, "pairId", json_integer(pairId));   // shared-CA pairing (CA_SHARED_EXPANDER)
        {   json_t* ck = json_array();
            for (int i = 0; i < CA::N_SCATTER; ++i)
                json_array_append_new(ck, json_integer((json_int_t)corrKey[i]));
            json_object_set_new(root, "corrKey", ck);
        }
        return root;
    }

    void dataFromJson(json_t* root) override {
        resetToIdentity();
        if (json_t* pj = json_object_get(root, "pairId")) pairId = (int)json_integer_value(pj);  // shared-CA (missing => 0 => reassigned in process())
        auto load = [&](const char* k, uint8_t* a) {
            json_t* arr = json_object_get(root, k);
            if (!arr) return;
            for (int v = 0; v < CA::N_VOICES && v < (int)json_array_size(arr); ++v) {
                json_t* val = json_array_get(arr, v);
                if (json_is_integer(val))
                    a[v] = (uint8_t)math::clamp((int)json_integer_value(val), 0, CA::N_VOICES-1);
            }
        };
        load("rhythmSrc", rhythmSrc);
        load("melodySrc", melodySrc);
        load("qmixSrc",   qmixSrc);   // missing in old patches → resetToIdentity left it identity (qmixSrc[v]=v)
        if (json_t* ck = json_object_get(root, "corrKey")) {
            // NOTE (pre-release, acceptable): bumping the scatter dimension 8→12 (SCATTER_TYPES 2→3)
            // reindexes the melody corrKey/scatterCounter slots, so a patch saved with the OLD 8-stream
            // corrKey will load its first 8 keys into the new 12-slot layout — the melody scatter stream
            // is NOT bit-reproducible across this change. Called out in the commit; no old public patches.
            for (int i = 0; i < CA::N_SCATTER && i < (int)json_array_size(ck); ++i) {
                json_t* v = json_array_get(ck, i);
                if (json_is_integer(v)) corrKey[i] = (uint64_t)json_integer_value(v);
            }
            // Keys beyond the saved count (q-mix streams in an old patch) keep the entropy set by
            // resetToIdentity()→(ctor seed) — but resetToIdentity no longer seeds, so top up any unset.
            for (int i = (int)json_array_size(ck); i < CA::N_SCATTER; ++i)
                corrKey[i] = rack::random::u64();
        } else {
            // Old patch saved before corrKey persistence (or before this fix): resetToIdentity()
            // no longer seeds keys, so give this instance valid entropy keys rather than all-zero.
            seedCorrKeysInternal();
        }
    }

    void onReset(const ResetEvent& e) override { resetToIdentity(); Module::onReset(e); }
};

// ── Widget ───────────────────────────────────────────────────────────────────
struct MonsoonChangeAlleyV2Widget : ModuleWidget,
    dotModular::Compose<MonsoonChangeAlleyV2Widget,
                        dotModular::ShapeQuery, dotModular::Bind, dotModular::Reload> {

    // Geometry -- MUST MATCH gen_change_alley_v2.py. Width DERIVED from the widest (SCATTER) row =
    // 4 jacks + 4 buttons + 1 grain dial + 1 light per side (true-reverse is NOT here — it's a
    // centred per-stream group beneath the matrix). Jacks/dial at 8.5mm pitch, buttons clustered
    // at 6.0mm; matrix kept at 99.6mm. Generator computes HP (now 56) from these; constants below
    // MUST equal the generator's.
    static constexpr float PW_MM   = 59.f * 5.08f;   // 299.72mm (generator-derived; CA widen for the
                                                     // expression pair columns — MUST MATCH the HP the
                                                     // generator prints: 56 → 59 after +1 jack col/side)
    static constexpr float PH_MM   = 128.5f;
    static constexpr float MARGIN  = 6.0f;
    static constexpr float JACK_P  = 8.5f;
    static constexpr float BTN_P   = 6.0f;
    static constexpr float J_HALF  = 4.25f;
    // Jack/dial group (outer→inner), 5 columns at JACK_P, offset inboard past the expression column.
    // MUST MATCH gen_change_alley_v2.py: J_DOM = (MARGIN+J_HALF)+JACK_P; EXPR_X centred in the gutter.
    static constexpr float J_DOM   = (MARGIN + J_HALF) + JACK_P;   // fwd domain trig jack (block start)
    // Correlation EXPRESSION pair column, CENTRED midway between the panel edge and J_DOM (Fix 3).
    static constexpr float EXPR_X  = J_DOM * 0.5f;                 // IN (left) / OUT (right, via lx)
    static constexpr float J_COD   = J_DOM  + JACK_P;          // fwd codomain trig jack
    static constexpr float KNOB1   = J_COD  + JACK_P;          // grain dial (all verbs)
    static constexpr float KNOB2   = KNOB1  + JACK_P;          // leader/step dial OR scatter dom-back jack
    static constexpr float J_BACK2 = KNOB2  + JACK_P;          // scatter cod-back jack
    // Button cluster (after a jack→button gap), 4 buttons at BTN_P — fwd + Philox reverse (ON-ROW):
    static constexpr float BTN_D   = J_BACK2 + (J_HALF + 3.0f);// fwd domain fire
    static constexpr float BTN_C   = BTN_D  + BTN_P;           // fwd codomain fire
    static constexpr float REV_D   = BTN_C  + BTN_P;           // Philox reverse domain
    static constexpr float REV_C   = REV_D  + BTN_P;           // Philox reverse codomain
    static constexpr float LIGHT_X = REV_C + (3.0f + J_HALF);
    static constexpr float CTRL_W  = LIGHT_X + 4.0f;
    static constexpr float GUTTER  = (PW_MM - 2.f*CTRL_W - 99.6f) / 2.f;   // matrix kept 99.6
    static constexpr float MX_MM   = CTRL_W + GUTTER;
    static constexpr float MW_MM   = PW_MM - 2.f * (CTRL_W + GUTTER);
    // True-reverse group (centred beneath the matrix): 3 jack+button pairs, per stream.
    static constexpr float TRUEREV_PAIR_DX  = 9.0f;   // jack↔button within a stream pair (loosened)
    static constexpr float TRUEREV_GROUP_DX = 34.0f;  // centre-to-centre between stream groups (loosened)
    static constexpr float CELL_W  = MW_MM / CA::N_VOICES;
    static constexpr float CELL_H  = CELL_W;
    static constexpr float MY_MM   = 16.0f;   // matrix top: the 1..16 column-number row (drawn at
                                              // MY_MM-1.6) sits level with the COLLAPSE first jack row
                                              // (rowY(0,0)=15). MUST MATCH gen_change_alley_v2.py GRID_Y.
    static constexpr float MH_MM   = CELL_H * CA::N_VOICES;
    // Q5 q-mix: 3 streams (rhythm, melody, q-mix) -> 12 rows/side. PLAN A (CA_PANEL_THREE_STREAM_LAYOUT):
    // tighten row pitch to fit 12 rows in 128.5mm with stock PJ301M jacks. MUST MATCH gen_change_alley_v2.py.
    // Jack well is r=3.9 (Ø7.8); ROW_H=8.0 is the jack-floor pitch (jacks touch at 0.2mm gap).
    static constexpr int   N_STREAMS    = 3;
    static constexpr float CTRL_ROW_H   = 8.0f;
    // GROUP_GAP widened to 4.5: gives each op-group's INTRA/INTER label a real clear band above it
    // (reclaimed room from matrix-up + legend-to-side pays for it). MUST MATCH gen_change_alley_v2.py.
    static constexpr float GROUP_GAP    = 4.5f;
    static constexpr float CTRL_TOP     = 11.0f;  // first row below the top logo/title band. MUST MATCH ROW_TOP.
    static constexpr float BOTTOM_OFFSET = 6.0f;   // gap from last row to the bottom poly-jack cluster
    static float rowY(int verb, int sub) {
        return CTRL_TOP + verb*(N_STREAMS*CTRL_ROW_H + GROUP_GAP) + sub*CTRL_ROW_H + CTRL_ROW_H*0.5f;
    }
    static float lx(float x_mm, bool flip) { return flip ? PW_MM - x_mm : x_mm; }

    static Vec cellCentre(int row, int col) {
        return mm2px(Vec(MX_MM + col * CELL_W + CELL_W * 0.5f,
                         MY_MM + row * CELL_H + CELL_H * 0.5f));
    }

    static float cellRadius() {          // BOTH pin types draw at this size (same, bigger)
        return mm2px(Vec(std::min(CELL_W, CELL_H) * 0.32f, 0)).x;
    }
    static float innerRadius() {         // concentric red-on-white inner: proportionally bigger
        return mm2px(Vec(std::min(CELL_W, CELL_H) * 0.32f * 0.55f, 0)).x;
    }
    static bool hitCell(Vec pos, int row, int col) {
        Vec c = cellCentre(row, col);
        float r = mm2px(Vec(std::min(CELL_W, CELL_H) * 0.5f, 0)).x;
        Vec d = pos - c;
        return d.x*d.x + d.y*d.y < r*r;
    }

    // Theme follows the CONNECTED MONSOON's lightTheme flag — the plugin-wide convention
    // (Raffles/Shophouse do the same). It is NOT Rack's global settings::preferDarkPanels;
    // using that was why Change Alley ignored Monsoon's dark setting.
    std::shared_ptr<rack::window::Svg> panelSvgDark, panelSvgLight;
    int lastThemeLight = -1;

    MonsoonChangeAlleyV2Widget(MonsoonChangeAlleyV2* module) {
        setModule(module);
        std::string dark  = asset::plugin(pluginInstance, "res/panels/ChangeAlleyV2_panel_dark.svg");
        std::string light = asset::plugin(pluginInstance, "res/panels/ChangeAlleyV2_panel_light.svg");
        panelSvgDark  = APP->window->loadSvg(dark);
        panelSvgLight = APP->window->loadSvg(light);
        loadPanel(dark);   // kit: sets the panel AND caches the components-layer anchors for bind-by-name
        // Screws pulled toward the edges to reclaim interior height for the 12 rows/side.
        // ScrewSilver's origin is its TOP-LEFT; the head is ~5.08mm across. y is the corner,
        // so y=2.0 => head spans 2.0..7.1mm (fully on-panel, within the mounting-rail zone);
        // the bottom pair mirrors that at PH-7.1..PH-2.0. This is tighter than the old 5.0mm
        // inset (which wasted ~3mm top and bottom) while staying on-panel and rail-mountable.
        static constexpr float SCREW_INSET_X = 7.5f;
        static constexpr float SCREW_INSET_Y = 2.0f;
        addChild(createWidget<ScrewSilver>(mm2px(Vec(SCREW_INSET_X,         SCREW_INSET_Y))));
        addChild(createWidget<ScrewSilver>(mm2px(Vec(PW_MM - SCREW_INSET_X, SCREW_INSET_Y))));
        addChild(createWidget<ScrewSilver>(mm2px(Vec(SCREW_INSET_X,         PH_MM - 7.1f))));
        addChild(createWidget<ScrewSilver>(mm2px(Vec(PW_MM - SCREW_INSET_X, PH_MM - 7.1f))));

        // Mod arc factory: overlay a red arc on a knob showing where poly CV pushes it.
        // getSetNorm = knob's own value; getModNorm = resolved knob+CV; gated on the
        // Monsoon menu flag modVizChangeAlley (matches every other surface).
        auto* mod = module;   // capture the ctor param explicitly (member `this->module`
                              //   is set by setModule but the local shadows it here)
        auto addArc = [&, mod](rack::app::Knob* knob, int paramId,
                          std::function<float()> resolved) {
            auto* arc = new redDot::ModArcOverlay();
            arc->radius = std::min(knob->box.size.x, knob->box.size.y) * 0.5f + mm2px(0.6f);
            arc->getSetNorm = [mod, paramId]() -> float {
                if (!mod) return 0.f;
                auto* pq = mod->paramQuantities[paramId];
                return pq ? (float)pq->getScaledValue() : 0.f;
            };
            arc->getModNorm = resolved;
            arc->isActive   = [mod]() -> bool {
                Monsoon* mm = mod ? redDot::findMonsoonEitherSide(mod) : nullptr;
                return mm ? mm->modVizChangeAlley : false;
            };
            arc->attachOverKnob(knob, 1.5f);
            addChild(arc);
        };

        // Transform controls: bound BY NAME from the components-layer anchors (Option B-full). The
        // widget no longer computes mm — the generator owns geometry; ids MUST MATCH the anchor names
        // gen_change_alley_v2.py emits (input_domain_{r}, param_grain_{r}, …). Same loop structure as
        // the generator, so the two stay in lockstep. addArc runs inside the knob config lambda.
        for (int verb = 0; verb < CA::N_VERBS; ++verb)
          for (int sub = 0; sub < CA::TYPES; ++sub) {   // 3 streams: rhythm, melody, q-mix
            for (int side = 0; side < 2; ++side) {
                const int r  = CA::rowId(verb, side, sub);
                const int si = side*CA::TYPES + sub;      // scatter-back / leader / step index
                const std::string R = std::to_string(r), SI = std::to_string(si);
                bindInput<PJ301MPort>("input_domain_"   + R, CA::DOMAIN_TRIG_START   + r);
                bindInput<PJ301MPort>("input_codomain_" + R, CA::CODOMAIN_TRIG_START + r);
                bindParam<Trimpot>("param_grain_" + R, CA::GRAIN_START + r,
                    std::function<void(Trimpot*)>([this, mod, r, &addArc](Trimpot* k){
                        if (k->getParamQuantity()) k->getParamQuantity()->snapEnabled = true;
                        addArc(k, CA::GRAIN_START + r, [mod, r]() -> float {
                            if (!mod) return 0.f;
                            float v = mod->params[CA::GRAIN_START + r].getValue();
                            return rack::math::clamp(v / 4.f, 0.f, 1.f);   // 0..4 detents -> 0..1
                        }); }));
                if (verb == CA::V_COLLAPSE) {
                    bindParam<Trimpot>("param_leader_" + SI, CA::LEADER_START + si,
                        std::function<void(Trimpot*)>([this, mod, si, &addArc](Trimpot* k){
                            if (k->getParamQuantity()) k->getParamQuantity()->snapEnabled = true;
                            addArc(k, CA::LEADER_START + si, [mod, si]() -> float {
                                if (!mod) return 0.f;
                                float v = mod->params[CA::LEADER_START + si].getValue();
                                return rack::math::clamp(v / 15.f, 0.f, 1.f);   // leader 0..15
                            }); }));
                } else if (verb == CA::V_ROTATE) {
                    bindParam<Trimpot>("param_step_" + SI, CA::STEP_START + si,
                        std::function<void(Trimpot*)>([this, mod, si, &addArc](Trimpot* k){
                            if (k->getParamQuantity()) k->getParamQuantity()->snapEnabled = true;
                            addArc(k, CA::STEP_START + si, [mod, si]() -> float {
                                if (!mod) return 0.f;
                                float v = mod->params[CA::STEP_START + si].getValue();   // -7..7
                                return rack::math::clamp((v + 7.f) / 14.f, 0.f, 1.f);
                            }); }));
                } else if (verb == CA::V_SCATTER) {
                    bindInput<PJ301MPort>("input_scback_dom_" + SI, CA::SCATTER_BACK_DOM_START + si);
                    bindInput<PJ301MPort>("input_scback_cod_" + SI, CA::SCATTER_BACK_COD_START + si);
                    // Philox reverse BUTTONS — ON-ROW at their own columns (screv_d/screv_c). (True-
                    // reverse is NOT here — it's a centred per-stream group beneath the matrix.)
                    bindParam<TL1105>("param_screv_d_" + SI, CA::SCATTER_REV_BTN_START + si);
                    bindParam<TL1105>("param_screv_c_" + SI, CA::SCATTER_REV_BTN_START + CA::SIDES*CA::TYPES + si);
                }
                bindParam<TL1105>("param_btnD_" + R, CA::BTN_START + r*2);
                bindParam<TL1105>("param_btnC_" + R, CA::BTN_START + r*2 + 1);
                bindLight<SmallLight<RedLight>>("light_pending_" + R, CA::PENDING_LIGHT_START + r);
            }
          }

        // TRUE-REVERSE group: 3 jack+button pairs (rhythm/melody/q-mix), CENTRED beneath the matrix.
        // Verb-agnostic, per-stream (index = type). Bound BY NAME (input/param/light_truerev_{ty}).
        for (int ty = 0; ty < CA::TYPES; ++ty) {
            const std::string TY = std::to_string(ty);
            bindInput<PJ301MPort>("input_truerev_" + TY, CA::TRUE_REV_IN_START + ty);
            bindParam<TL1105>    ("param_truerev_" + TY, CA::TRUE_REV_BTN_START + ty);
            bindLight<SmallLight<RedLight>>("light_truerev_" + TY, CA::TRUE_REV_LIGHT_START + ty);
        }

        // (Bottom-centre ConnectMark REMOVED — replaced by the top-right 8-slot host connect row
        //  (CONNECTION_UI_MODEL §14 / CA_SHARED_EXPANDER_BUILD): slot k = pairId k, filled in
        //  pairColour(k) when connected, primary on a second axis. The 8 slot WELLS are panel art
        //  (light_hostslot_{k} anchors); the filled/primary rendering + primary menu land next.)

        auto* ov = new PinOverlay(module);
        ov->box.pos  = Vec(0, 0);
        ov->box.size = box.size;
        addChild(ov);
    }

    // TransparentWidget, NOT Opaque: an opaque overlay sized to the module box consumed
    // every left-press, leaving nowhere to grab the panel for dragging (and blocked the
    // context menu outside the grid). Transparent passes everything through; we consume
    // ONLY genuine cell hits in onButton.
    struct PinOverlay : widget::TransparentWidget {
        MonsoonChangeAlleyV2* module;
        int hoverRow = -1, hoverCol = -1;   // XILS crosshair target (-1 = none)
        PinOverlay(MonsoonChangeAlleyV2* m) : module(m) {}

        int getPolyCount() const {
            if (!module) return 0;
            auto* mon = redDot::findMonsoonEitherSide(module);
            return mon ? mon->engine.numPolyVoices : 0;
        }

        // A pin that reads as a physical peg: soft drop shadow, flat colour body,
        // Shared stream colours: legend swatches, matrix pins, and the pending-highlight ALL
        // pull from these so the three agree. rhythm=white, melody=red, q-mix=green ("green plane",
        // CA_PANEL_THREE_STREAM_LAYOUT / QMIX_LANE_PARITY). The q-mix pin RENDER + green-pin click
        // are a later layer; the COLOUR is defined here now so the legend + future dots match.
        static NVGcolor pinRhythm() { return nvgRGBf(0.95f,0.95f,0.94f); }
        static NVGcolor pinMelody() { return nvgRGBf(0.83f,0.f,0.10f); }
        static NVGcolor pinQmix()   { return nvgRGBf(0.30f,0.75f,0.35f); }

        // a rim a shade darker, and an offset specular highlight. col = body colour.
        static void drawPin(NVGcontext* vg, float cx, float cy, float r,
                            NVGcolor body, float alpha) {
            // drop shadow
            nvgBeginPath(vg); nvgCircle(vg, cx + r*0.12f, cy + r*0.16f, r);
            nvgFillColor(vg, nvgRGBAf(0,0,0,0.35f*alpha)); nvgFill(vg);
            // body
            nvgBeginPath(vg); nvgCircle(vg, cx, cy, r);
            NVGcolor b = body; b.a *= alpha;
            nvgFillColor(vg, b); nvgFill(vg);
            // rim (slightly darker ring)
            nvgBeginPath(vg); nvgCircle(vg, cx, cy, r);
            nvgStrokeColor(vg, nvgRGBAf(0,0,0,0.30f*alpha)); nvgStrokeWidth(vg, r*0.16f);
            nvgStroke(vg);
            // specular highlight, upper-left
            nvgBeginPath(vg); nvgCircle(vg, cx - r*0.30f, cy - r*0.32f, r*0.30f);
            nvgFillColor(vg, nvgRGBAf(1,1,1,0.55f*alpha)); nvgFill(vg);
        }

        void draw(const DrawArgs& args) override {
            NVGcontext* vg = args.vg;
            int poly = getPolyCount();
            float ro = cellRadius(), ri = innerRadius();

            // ── Labels: drawn HERE because nanosvg ignores SVG <text> (the brand rule
            //    "fonts outlined to paths" exists for panels; for a live widget nvgText
            //    is simpler and theme-aware). Drawn with module==nullptr too, so the
            //    browser preview shows a labelled panel. ──
            {
                std::shared_ptr<Font> font = APP->window->loadFont(
                    asset::system("res/fonts/ShareTechMono-Regular.ttf"));
                if (font) {
                    nvgFontFaceId(vg, font->handle);
                    // Ink follows WHERE the text sits, not just the theme:
                    //   • on the BODY (row/col numbers, transform labels) -> theme ink,
                    //     because the body is light in light theme, dark in dark theme.
                    //   • inside the GRID (tooltip) -> always light; the grid is dark in
                    //     BOTH themes and the tooltip has its own dark backing box.
                    // (The earlier 'only row 1 numbered' bug was dark ink on a dark body;
                    //  the fix is theme-correct ink, not permanently-light ink.)
                    // Same source as the panel swap: the connected Monsoon's flag.
                    Monsoon* themeM = module ? redDot::findMonsoonEitherSide(module) : nullptr;
                    const bool lightBody = themeM && themeM->lightTheme;
                    NVGcolor ink    = lightBody ? nvgRGB(0x2a,0x2a,0x2e) : nvgRGB(0xe8,0xe2,0xd0);
                    NVGcolor inkdim = lightBody ? nvgRGBA(0x88,0x8d,0x96,0xd0)
                                                : nvgRGBA(0x9a,0x95,0x88,0xb0);
                    NVGcolor amber  = lightBody ? nvgRGB(0xa0,0x78,0x08)   // kit light gold
                                                : nvgRGB(0xc8,0x90,0x0c);
                    char num[4];
                    // Voice-number labels only (currency codes dropped — tiny + noisy in-rack).
                    // ShareTechMono, 3.2mm — readable at 100% zoom.
                    for (int col = 0; col < CA::N_VOICES; ++col) {
                        Vec c = cellCentre(0, col);
                        float topY = mm2px(Vec(0, MY_MM)).y;
                        snprintf(num, sizeof(num), "%d", col + 1);
                        nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_BASELINE);
                        nvgFontSize(vg, mm2px(Vec(3.2f,0)).x);
                        nvgFillColor(vg, col == 0 ? amber : ink);
                        nvgText(vg, c.x, topY - mm2px(Vec(0,1.6f)).y, num, NULL);
                    }
                    for (int row = 0; row < CA::N_VOICES; ++row) {
                        Vec c = cellCentre(row, 0);
                        float leftX = mm2px(Vec(MX_MM,0)).x;
                        snprintf(num, sizeof(num), "%d", row + 1);
                        nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
                        nvgFontSize(vg, mm2px(Vec(3.2f,0)).x);
                        nvgFillColor(vg, row == 0 ? amber : ink);
                        nvgText(vg, leftX - mm2px(Vec(1.4f,0)).x, c.y, num, NULL);
                    }
                    // Verb labels BOTH sides: "COLLAPSE INTRA" left, "COLLAPSE INTER" right.
                    // Panel row order is Collapse, Rotate, Reflect, Scatter (matches V_*).
                    //
                    // FIX (2nd widening disturbed these): x is DERIVED FROM THE BLOCK each label
                    // belongs to — the centre of that side's button cluster — NOT from the panel edge
                    // (the old MARGIN / PW_MM-MARGIN pinned INTER to the right edge, so widening pushed
                    // it onto the expression jacks). blockCx is the mm centre of the button columns
                    // (BTN_D..REV_C); lx() mirrors it to each side. Centre-aligned over the block. Now
                    // ANY future width change moves the labels with their blocks automatically.
                    static constexpr const char* TN[4] = {"COLLAPSE","ROTATE","REFLECT","SCATTER"};
                    const float blockCx = (BTN_D + REV_C) * 0.5f;   // button-cluster centre (INTRA frame)
                    nvgFillColor(vg, inkdim);
                    nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_BASELINE);
                    for (int t2 = 0; t2 < 4; ++t2) {
                        // MIDDLE-aligned at the CENTRE of the GROUP_GAP band above this group's
                        // first row — so the label sits squarely in the gap, clear of both the row
                        // above and this group's first row. 2.2mm font fits the 4.5mm band.
                        float gy = mm2px(Vec(0, rowY(t2, 0) - CTRL_ROW_H*0.5f - GROUP_GAP*0.5f)).y;
                        nvgFontSize(vg, mm2px(Vec(2.2f,0)).x);
                        char lbl[24];
                        snprintf(lbl, sizeof(lbl), "%s INTRA", TN[t2]);
                        nvgText(vg, mm2px(Vec(lx(blockCx, false), 0)).x, gy, lbl, NULL);   // left block
                        snprintf(lbl, sizeof(lbl), "%s INTER", TN[t2]);
                        nvgText(vg, mm2px(Vec(lx(blockCx, true),  0)).x, gy, lbl, NULL);   // right block (mirror)
                    }
                    // HORIZONTAL legend (rhythm / melody / q-mix in one row), below the INTRA (left)
                    // control block, NOT overlapping the matrix. Colours from the SHARED accessors so
                    // the legend == matrix pins.
                    {
                        const float lgY = rowY(CA::N_VERBS-1, N_STREAMS-1) + CTRL_ROW_H*0.5f + 4.0f;
                        const float sw  = mm2px(Vec(1.3f,0)).x;
                        nvgFontSize(vg, mm2px(Vec(2.4f,0)).x);
                        nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
                        const float ly = mm2px(Vec(0, lgY)).y;
                        struct Sw { NVGcolor c; const char* t; };
                        const Sw sws[3] = { {pinRhythm(),"rhythm"}, {pinMelody(),"melody"}, {pinQmix(),"q-mix"} };
                        float x = J_COD;   // first swatch under the 2nd jack column (clears the corner screw)
                        for (int i = 0; i < 3; ++i) {
                            const float cx = mm2px(Vec(x, 0)).x;
                            nvgBeginPath(vg); nvgCircle(vg, cx, ly, sw);
                            nvgFillColor(vg, sws[i].c); nvgFill(vg);
                            nvgFillColor(vg, inkdim);
                            nvgText(vg, cx + mm2px(Vec(2.2f,0)).x, ly, sws[i].t, NULL);
                            x += 18.0f;   // horizontal spacing between swatch+label groups
                        }
                    }
                    // TRUE-REVERSE group label + per-stream colour rings (match the legend colours,
                    // so the 3 centred pairs read as rhythm/melody/q-mix). MUST MATCH the bind +
                    // generator geometry (trY / gcx / TRUEREV_*).
                    {
                        const float trY = PH_MM - 5.0f;   // MUST MATCH the bind + generator
                        const float gcx = MX_MM + MW_MM * 0.5f;
                        nvgFontSize(vg, mm2px(Vec(2.6f,0)).x);
                        nvgFillColor(vg, inkdim);
                        nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_BASELINE);
                        nvgText(vg, mm2px(Vec(gcx, 0)).x, mm2px(Vec(0, trY - 6.0f)).y, "TRUE REVERSE", NULL);
                        // Colour rings around the BUTTONS (smaller, right of each pair) — not the jacks.
                        const NVGcolor sc[3] = { pinRhythm(), pinMelody(), pinQmix() };
                        for (int ty = 0; ty < 3; ++ty) {
                            const float cx = gcx + (ty - 1) * TRUEREV_GROUP_DX + TRUEREV_PAIR_DX*0.5f;
                            nvgBeginPath(vg);
                            nvgCircle(vg, mm2px(Vec(cx, 0)).x, mm2px(Vec(0, trY)).y, mm2px(Vec(3.3f,0)).x);
                            NVGcolor rc = sc[ty]; rc.a = 0.7f;
                            nvgStrokeColor(vg, rc); nvgStrokeWidth(vg, mm2px(Vec(0.6f,0)).x); nvgStroke(vg);
                        }
                    }
                    // Title + legend
                    nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_BASELINE);
                    nvgFontSize(vg, mm2px(Vec(3.6f,0)).x);
                    nvgFillColor(vg, ink);
                    nvgText(vg, box.size.x * 0.5f, mm2px(Vec(0,6.0f)).y, "CHANGE ALLEY", NULL);

                    // (Connect indicator is a shared redDot::ConnectMark child widget added in the
                    //  ModuleWidget ctor — bottom, between the legend and TRUE REVERSE. Not drawn here.)
                }
            }
            if (!module) return;

            // Pin colours are inlined below (white=rhythm, red=melody; identity pins at
            // 0.7 alpha, inactive rows at 0.4). Single literals, easy to tune.

            // ── Temasek pending: highlight affected submatrices ──────────────────────
            // Transforms are LOCAL to this module now, so the highlight reads pendingRows
            // directly -- no POD indirection, no header cycle to avoid (that machinery was
            // only for the two-module split).
            if (module) {
                const int active = std::max(1, poly + 1);
                for (int hr = 0; hr < CA::N_ROWS; ++hr) {
                    const auto& h = module->pendingRows[hr];
                    if (!h.armed) continue;
                    const int hType = hr % CA::TYPES;   // 0=rhythm 1=melody 2=q-mix
                    NVGcolor hcol = (hType == 0) ? pinRhythm()
                                  : (hType == 1) ? pinMelody()
                                                 : pinQmix();
                    hcol.a = 0.55f;   // highlight alpha (shared hue, translucent band)
                    const float sw = mm2px(Vec(0.45f,0)).x;
                    const float hw = mm2px(Vec(CELL_W * 0.5f, 0)).x;
                    const float hh = mm2px(Vec(0, CELL_H * 0.5f)).y;
                    // What a transform actually touches:
                    //   DOMAIN   ops partition the ROWS    -> horizontal bands
                    //   CODOMAIN ops partition the SOURCES -> vertical bands
                    // (An earlier version outlined diagonal squares, which is only correct
                    //  near identity: rotateValues takes its block from src[v], the COLUMN,
                    //  and collapseDomain hands row v the value tmp[leader], any column.)
                    // INTRA bands are `grain` wide; INTER bands are the whole pool split
                    // into blocks, drawn heavier because whole blocks move as units.
                    const int b    = std::max(1, h.grain);
                    const float lw = h.isInter ? sw * 1.6f : sw;
                    for (int base = 0; base < active; base += b) {
                        const int last = std::min(base + b, active) - 1;
                        if (last < base) continue;
                        Vec tl, br;
                        if (h.isDomain) {                    // rows: full-width band
                            tl = cellCentre(base, 0);
                            br = cellCentre(last, active - 1);
                        } else {                             // sources: full-height band
                            tl = cellCentre(0, base);
                            br = cellCentre(active - 1, last);
                        }
                        nvgBeginPath(vg);
                        nvgRect(vg, tl.x - hw, tl.y - hh,
                                (br.x + hw) - (tl.x - hw), (br.y + hh) - (tl.y - hh));
                        nvgStrokeColor(vg, hcol); nvgStrokeWidth(vg, lw); nvgStroke(vg);
                    }
                }
            }

            for (int row = 0; row < CA::N_VOICES; ++row) {
                bool active = (row == 0) || (row <= poly);  // row 0=mono always active
                float alpha = active ? 1.f : 0.4f;
                uint8_t rSrc = module->rhythmSrc[row];
                uint8_t mSrc = module->melodySrc[row];
                uint8_t qSrc = module->qmixSrc[row];

                for (int col = 0; col < CA::N_VOICES; ++col) {
                    Vec c = cellCentre(row, col);
                    bool hasR = (rSrc == (uint8_t)col);
                    bool hasM = (mSrc == (uint8_t)col);
                    bool hasQ = (qSrc == (uint8_t)col);
                    bool rIdentity = hasR && (col == row);
                    bool mIdentity = hasM && (col == row);
                    bool qIdentity = hasQ && (col == row);

                    NVGcolor white = pinRhythm();
                    NVGcolor red   = pinMelody();
                    NVGcolor green = pinQmix();
                    // Concentric EMS render: outer white peg (rhythm) -> mid red dot (melody) ->
                    // inner green dot (q-mix). Any subset can be present; a lone plane draws at its
                    // own layer so it's still visible. rq = q-mix centre radius (smaller than ri).
                    const float rq = ri * 0.62f;
                    if (hasR || hasM || hasQ) {
                        // base peg: white if rhythm present, else the outermost present plane's colour
                        if (hasR) {
                            drawPin(vg, c.x, c.y, ro, white, rIdentity ? 0.72f*alpha : alpha);
                        } else if (hasM) {
                            drawPin(vg, c.x, c.y, ro, red, mIdentity ? 0.72f*alpha : alpha);
                        } else { // q-mix only
                            drawPin(vg, c.x, c.y, ro, green, qIdentity ? 0.72f*alpha : alpha);
                        }
                        // mid red dot if melody present AND a rhythm peg is under it
                        if (hasM && hasR) {
                            NVGcolor ic = red; ic.a = (mIdentity ? 0.72f : 1.f) * alpha;
                            nvgBeginPath(vg); nvgCircle(vg, c.x, c.y, ri);
                            nvgFillColor(vg, ic); nvgFill(vg);
                        }
                        // inner green dot if q-mix present AND something is under it (peg is R or M)
                        if (hasQ && (hasR || hasM)) {
                            NVGcolor gc = green; gc.a = (qIdentity ? 0.72f : 1.f) * alpha;
                            nvgBeginPath(vg); nvgCircle(vg, c.x, c.y, rq);
                            nvgFillColor(vg, gc); nvgFill(vg);
                        }
                    } else {
                        // Empty — very faint ghost
                        nvgBeginPath(vg); nvgCircle(vg, c.x, c.y, ro * 0.55f);
                        nvgStrokeColor(vg, nvgRGBAf(0.5f,0.5f,0.5f,0.12f*alpha));
                        nvgStrokeWidth(vg, 0.5f); nvgStroke(vg);
                    }
                }

                // Poly activity bar on right edge
                if (row > 0) {
                    float rx  = mm2px(Vec(MX_MM + MW_MM + 0.8f, 0)).x;
                    float cy  = cellCentre(row, 0).y;
                    float bh  = mm2px(Vec(0, CELL_H * 0.55f)).y;
                    NVGcolor bc = active ? nvgRGBA(0xd4,0x00,0x1a,0xa0) : nvgRGBA(0x30,0x30,0x30,0x60);
                    nvgBeginPath(vg);
                    nvgRect(vg, rx, cy - bh*0.5f, mm2px(Vec(1.2f,0)).x, bh);
                    nvgFillColor(vg, bc); nvgFill(vg);
                }
            }

            // ── XILS-style targeting crosshair + readout on hover ─────────────
            if (hoverRow >= 0 && hoverCol >= 0) {
                Vec c = cellCentre(hoverRow, hoverCol);
                float gx0 = mm2px(Vec(MX_MM, 0)).x, gx1 = mm2px(Vec(MX_MM + MW_MM, 0)).x;
                float gy0 = mm2px(Vec(0, MY_MM)).y, gy1 = mm2px(Vec(0, MY_MM + MH_MM)).y;
                (void)gx1; (void)gy1;
                nvgStrokeColor(vg, nvgRGBAf(1,1,1,0.55f));
                nvgStrokeWidth(vg, 0.8f);
                // XILS-style: guides run from the AXES to the cell only (not full-span)
                nvgBeginPath(vg); nvgMoveTo(vg, gx0, c.y); nvgLineTo(vg, c.x - ro*1.4f, c.y); nvgStroke(vg);
                nvgBeginPath(vg); nvgMoveTo(vg, c.x, gy0); nvgLineTo(vg, c.x, c.y - ro*1.4f); nvgStroke(vg);
                // hovered-cell ring
                nvgBeginPath(vg); nvgCircle(vg, c.x, c.y, ro * 1.15f);
                nvgStrokeColor(vg, nvgRGBAf(1,1,1,0.8f)); nvgStrokeWidth(vg, 0.8f); nvgStroke(vg);
                // readout "row->col" near the cursor (top-left of grid)
                std::shared_ptr<Font> font = APP->window->loadFont(
                    asset::system("res/fonts/ShareTechMono-Regular.ttf"));
                if (font) {
                    // Typed readout: states RHYTHM or MELODY (the gesture that would fire)
                    // per current mouse expectation: plain hover previews rhythm; the melody
                    // half is stated so the mapping reads even before clicking. Both shown.
                    char buf[80];
                    uint8_t rs = module->rhythmSrc[hoverRow], ms = module->melodySrc[hoverRow];
                    uint8_t qs = module->qmixSrc[hoverRow];
                    snprintf(buf, sizeof(buf), "v%d  rhythm<-v%d  melody<-v%d  q-mix<-v%d",
                             hoverRow + 1, rs + 1, ms + 1, qs + 1);
                    nvgFontFaceId(vg, font->handle);
                    nvgFontSize(vg, mm2px(Vec(3.4f,0)).x);          // was 2.6 — readable now
                    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
                    float bounds[4];
                    nvgTextBounds(vg, 0, 0, buf, NULL, bounds);
                    float tw = bounds[2] - bounds[0] + mm2px(Vec(2.f,0)).x;
                    float th = mm2px(Vec(4.6f,0)).x;
                    // near the cursor cell, clamped inside the grid
                    float tx = c.x + ro*1.8f, ty = c.y - th*0.5f;
                    if (tx + tw > gx1) tx = c.x - ro*1.8f - tw;
                    if (ty < gy0) ty = gy0;
                    if (ty + th > gy1) ty = gy1 - th;
                    nvgBeginPath(vg); nvgRect(vg, tx, ty, tw, th);
                    nvgFillColor(vg, nvgRGBAf(0,0,0,0.8f)); nvgFill(vg);
                    nvgFillColor(vg, nvgRGBf(0.95f,0.95f,0.94f));
                    nvgText(vg, tx + mm2px(Vec(1.f,0)).x, ty + th*0.5f, buf, NULL);
                }
            }
        }

        // Track hovered cell for the crosshair; keep events passing through.
        void onHover(const event::Hover& e) override {
            hoverRow = hoverCol = -1;
            for (int row = 0; row < CA::N_VOICES && hoverRow < 0; ++row)
                for (int col = 0; col < CA::N_VOICES; ++col)
                    if (hitCell(e.pos, row, col)) { hoverRow = row; hoverCol = col; break; }
            TransparentWidget::onHover(e);
        }
        void onLeave(const event::Leave& e) override {
            hoverRow = hoverCol = -1;
            TransparentWidget::onLeave(e);
        }

        // Row-radio click: left=rhythm, right/Ctrl=melody, Shift=q-mix (green, either button).
        // Clicking cell (row, col) sets rhythmSrc/melodySrc/qmixSrc[row]=col. Row-radio is
        // automatic: each table's src[row] holds exactly one value — this overwrites it.
        void onButton(const event::Button& e) override {
            if (!module || e.action != GLFW_PRESS) { TransparentWidget::onButton(e); return; }
            // plane: 0=rhythm, 1=melody, 2=q-mix. Shift wins (either mouse button); else
            // right/Ctrl=melody; else left=rhythm.
            int plane = 0;
            if (e.mods & RACK_MOD_SHIFT)                                             plane = 2;
            else if ((e.button == GLFW_MOUSE_BUTTON_RIGHT) || (e.mods & RACK_MOD_CTRL)) plane = 1;
            for (int row = 0; row < CA::N_VOICES; ++row) {
                for (int col = 0; col < CA::N_VOICES; ++col) {
                    if (!hitCell(e.pos, row, col)) continue;
                    // Store-backed + undoable: the pin tables are NOT params (zero DAW slots --
                    // DAW_PARAM_AUDIT), so undo goes through StoreEditAction. The action targets the
                    // module id and bakes (row, plane) into the setter, so undo lands on the row/plane
                    // actually edited. Equal old/new never records.
                    {
                        uint8_t* tbl = (plane == 0) ? module->rhythmSrc
                                     : (plane == 1) ? module->melodySrc : module->qmixSrc;
                        const char* nm = (plane == 0) ? "move rhythm pin"
                                       : (plane == 1) ? "move melody pin" : "move q-mix pin";
                        float oldV = (float)tbl[row];
                        redDot::applyAndPushStoreEdit<MonsoonChangeAlleyV2>(
                            module, nm,
                            [row, plane](MonsoonChangeAlleyV2& m, float v) {
                                uint8_t c = (uint8_t)math::clamp((int)std::lround(v), 0, CA::N_VOICES - 1);
                                uint8_t* t = (plane == 0) ? m.rhythmSrc
                                           : (plane == 1) ? m.melodySrc : m.qmixSrc;
                                t[row] = c;
                            },
                            oldV, (float)col);
                    }
                    e.consume(this);
                    return;
                }
            }
            TransparentWidget::onButton(e);
        }

    };

    // Reset = up to 32 cell changes; one gesture must be ONE undo step, so it gets a
    // whole-table snapshot action rather than 32 StoreEditActions. Same module-id
    // resolution discipline as StoreEditAction (survives deletion; no-ops while gone).
    struct ResetPinsAction : rack::history::Action {
        int64_t moduleId;
        uint8_t oldR[CA::N_VOICES], oldM[CA::N_VOICES], oldQ[CA::N_VOICES];
        ResetPinsAction(MonsoonChangeAlleyV2* m) : moduleId(m->id) {
            name = "reset pins to identity";
            for (int v = 0; v < CA::N_VOICES; ++v) { oldR[v] = m->rhythmSrc[v]; oldM[v] = m->melodySrc[v]; oldQ[v] = m->qmixSrc[v]; }
        }
        MonsoonChangeAlleyV2* resolve() {
            return dynamic_cast<MonsoonChangeAlleyV2*>(APP->engine->getModule(moduleId));
        }
        void undo() override {
            if (auto* m = resolve())
                for (int v = 0; v < CA::N_VOICES; ++v) { m->rhythmSrc[v] = oldR[v]; m->melodySrc[v] = oldM[v]; m->qmixSrc[v] = oldQ[v]; }
        }
        void redo() override {
            if (auto* m = resolve()) m->resetToIdentity();
        }
    };

    // One committed phrase-boundary transform batch = one undo step. Snapshots produced on the
    // audio thread (module->applyPendingTransforms) are drained here (UI thread) into these
    // actions. Same module-id resolution discipline as ResetPinsAction (survives deletion).
    struct TransformUndoAction : rack::history::Action {
        int64_t  moduleId;
        uint8_t  beforeR[CA::N_VOICES], beforeM[CA::N_VOICES], beforeQ[CA::N_VOICES];
        uint8_t  afterR[CA::N_VOICES],  afterM[CA::N_VOICES],  afterQ[CA::N_VOICES];
        int64_t counterBefore[CA::N_SCATTER];
        int64_t counterAfter [CA::N_SCATTER];
        TransformUndoAction() { name = "Change Alley transform"; }
        MonsoonChangeAlleyV2* resolve() {
            return dynamic_cast<MonsoonChangeAlleyV2*>(APP->engine->getModule(moduleId));
        }
        void undo() override {
            if (auto* m = resolve()) {
                for (int v = 0; v < CA::N_VOICES; ++v) { m->rhythmSrc[v] = beforeR[v]; m->melodySrc[v] = beforeM[v]; m->qmixSrc[v] = beforeQ[v]; }
                for (int i = 0; i < CA::N_SCATTER; ++i) m->scatterCounter[i] = counterBefore[i];
            }
        }
        void redo() override {
            if (auto* m = resolve()) {
                for (int v = 0; v < CA::N_VOICES; ++v) { m->rhythmSrc[v] = afterR[v]; m->melodySrc[v] = afterM[v]; m->qmixSrc[v] = afterQ[v]; }
                for (int i = 0; i < CA::N_SCATTER; ++i) m->scatterCounter[i] = counterAfter[i];
            }
        }
    };

    void step() override {
        ModuleWidget::step();
        kitStep();          // kit: dev live-reload poll (Option B-full)
        if (!module) return;

        // Drain the transform-undo ring produced on the audio thread. Each snapshot becomes one
        // Rack history action (UI-thread push, which is required). SPSC: we are the sole consumer.
        if (auto* ca = dynamic_cast<MonsoonChangeAlleyV2*>(module)) {
            uint32_t t = ca->undoTail.load(std::memory_order_relaxed);
            uint32_t h = ca->undoHead.load(std::memory_order_acquire);
            while (t != h) {
                const auto& snap = ca->undoRing[t % MonsoonChangeAlleyV2::UNDO_RING];
                auto* act = new TransformUndoAction();
                act->moduleId = ca->id;
                for (int v = 0; v < CA::N_VOICES; ++v) {
                    act->beforeR[v] = snap.beforeR[v]; act->beforeM[v] = snap.beforeM[v]; act->beforeQ[v] = snap.beforeQ[v];
                    act->afterR[v]  = snap.afterR[v];  act->afterM[v]  = snap.afterM[v];  act->afterQ[v]  = snap.afterQ[v];
                }
                for (int i = 0; i < CA::N_SCATTER; ++i) {
                    act->counterBefore[i] = snap.counterBefore[i];
                    act->counterAfter[i]  = snap.counterAfter[i];
                }
                APP->history->push(act);
                ++t;
            }
            ca->undoTail.store(t, std::memory_order_release);
        }

        Monsoon* m = redDot::findMonsoonEitherSide(module);
        const int wantLight = (m && m->lightTheme) ? 1 : 0;
        if (wantLight != lastThemeLight) {
            lastThemeLight = wantLight;
            for (Widget* child : children) {
                if (auto* sp = dynamic_cast<app::SvgPanel*>(child)) {
                    sp->setBackground(wantLight ? panelSvgLight : panelSvgDark);
                    break;
                }
            }
        }
    }

    void appendContextMenu(Menu* menu) override {
        ModuleWidget::appendContextMenu(menu);
        auto* module = dynamic_cast<MonsoonChangeAlleyV2*>(this->module);
        if (!module) return;
        menu->addChild(new MenuSeparator);
        menu->addChild(createMenuItem("Reset to identity diagonal", "",
            [module]() {
                // Skip the no-op (already identity) so undo history stays clean.
                bool isIdentity = true;
                for (int v = 0; v < CA::N_VOICES; ++v)
                    if (module->rhythmSrc[v] != v || module->melodySrc[v] != v
                        || module->qmixSrc[v] != v) { isIdentity = false; break; }
                if (isIdentity) return;
                auto* act = new ResetPinsAction(module);
                module->resetToIdentity();
                APP->history->push(act);
            }));
    }
};
