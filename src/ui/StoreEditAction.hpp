#pragma once
// StoreEditAction — undo/redo for STORE-BACKED widget edits (the de-param prerequisite).
//
// WHY THIS EXISTS (see docs/design/DAW_PARAM_AUDIT.md §5b): the selected-voice display
// proxies (Macro send disp, East atten/owner/deleg disp, dir disp, ...) are being removed
// from the param pool — their real values live in per-voice STORES on Monsoon (e.g.
// editor.macroSend[256]), not in params. ParamWidget's free undo goes with them. This
// header restores it through Rack's open history API (APP->history->push takes any
// history::Action — the same mechanism cable/module edits use), with one deliberate
// improvement: VOICE-CORRECT undo.
//
// THE VOICE-CORRECTNESS POINT. Proxy undo today is subtly wrong: undoing a proxy change
// after switching voice tabs restores the PARAM, and the store-back then writes the
// restored value into the NEWLY selected voice. A StoreEditAction bakes the target
// indices (voice, lane, item, ...) into its setter AT EDIT TIME, so undo lands on the
// voice that was actually edited, regardless of what the UI has selected since.
//
// OWNERSHIP + APPLY SEMANTICS (Rack conventions, do not deviate):
//   - APP->history->push(action) takes OWNERSHIP of the action.
//   - push() does NOT call redo(). The edit must ALREADY be applied by the caller when
//     it is pushed (exactly how ParamWidget records a finished drag). redo() only runs
//     on an actual redo gesture. Use applyAndPushStoreEdit() when you want apply+record
//     in one call (click-style edits).
//   - The action holds the MODULE ID, not a pointer: the store lives on Monsoon, the
//     widget lives on an expander, and either can be deleted while the action sits in
//     history. undo()/redo() re-resolve through APP->engine->getModule and no-op safely
//     if the module is gone (again matching Rack's own ParamChange behaviour).
//
// WIRING GUIDE
//   Click/step edits (owner cells, dir cells, delegation toggles):
//       bool oldV = read(store);                         // BEFORE the edit
//       redDot::applyAndPushStoreEdit<Monsoon>(mon, "change lane owner",
//           [voice, lane](Monsoon& m, bool v) { m.setOwnerEast(voice, lane, v); },
//           oldV, newV);
//   Drag edits (send cells, atten cells) — coalesce to ONE action per drag, mimicking
//   ParamWidget: live-write the store during the drag, record once at the end:
//       onDragStart:  coal.begin(read(store));
//       onDragMove:   write(store, value);               // live, no history
//       onDragEnd:    coal.commit<Monsoon>(mon, "edit macro send",
//                         [voice,lane,item](Monsoon& m, float v){ m.editor.macroSend[idx(voice,lane,item)] = v; },
//                         read(store));
//   Escape/abort paths call coal.cancel().
//
// VALUES are carried at their NATIVE type (V, deduced from the setter + old/new pair):
// bool for ownership cells, float for float stores. The undo path has NO encode/decode
// step — the action transports exactly what the setter consumes, so no quantization
// threshold can survive in it (boolean-model ruling, Rodney 2026-10-22). Equal old/new
// never records (no zero-size undo steps). A null std::function setter is the CALLER's
// bug to guard (StoreBound checks; click sites always pass a fresh lambda).

#include <rack.hpp>
#include <functional>
#include <string>
#include <utility>

namespace redDot {

template <typename TModule, typename V, typename FSetter>
struct StoreEditAction : rack::history::Action {
    int64_t moduleId;
    FSetter setter;
    V oldValue;
    V newValue;

    StoreEditAction(std::string label, int64_t moduleId_, FSetter setter_,
                    V oldValue_, V newValue_)
        : moduleId(moduleId_), setter(std::move(setter_)),
          oldValue(oldValue_), newValue(newValue_) {
        name = std::move(label);   // shows in Edit menu as "Undo <name>"
    }

    void apply(V v) {
        // Re-resolve every time: the module may have been deleted (or deleted and
        // undo-restored, in which case Rack re-creates it under the SAME id — this is
        // why an id outlives a pointer).
        auto* m = dynamic_cast<TModule*>(APP->engine->getModule(moduleId));
        if (m) setter(*m, v);
    }
    void undo() override { apply(oldValue); }
    void redo() override { apply(newValue); }
};

// Record an ALREADY-APPLIED edit. No-ops on null module or unchanged value.
// V is deduced from the old/new pair, FSetter from the callable; TModule stays the
// only explicit argument at call sites (applyAndPushStoreEdit<Monsoon>(...)).
template <typename TModule, typename V, typename FSetter>
inline void pushStoreEdit(TModule* module, std::string label,
                          FSetter setter, V oldValue, V newValue) {
    if (!module) return;
    if (oldValue == newValue) return;
    APP->history->push(new StoreEditAction<TModule, V, FSetter>(
        std::move(label), module->id, std::move(setter), oldValue, newValue));
}

// Apply the edit AND record it — the one-call form for click-style edits.
template <typename TModule, typename V, typename FSetter>
inline void applyAndPushStoreEdit(TModule* module, std::string label,
                                  FSetter setter, V oldValue, V newValue) {
    if (!module) return;
    if (oldValue == newValue) return;
    setter(*module, newValue);
    pushStoreEdit<TModule, V, FSetter>(module, std::move(label), std::move(setter),
                                       oldValue, newValue);
}

// Drag coalescer: ONE history action per drag gesture, however many live writes the
// drag made (ParamWidget's onDragStart/onDragEnd snapshot pattern, for stores).
struct StoreEditCoalescer {
    float startValue = 0.f;
    bool  armed      = false;

    void begin(float currentValue) { startValue = currentValue; armed = true; }
    void cancel()                  { armed = false; }

    template <typename TModule>
    void commit(TModule* module, std::string label,
                std::function<void(TModule&, float)> setter, float endValue) {
        if (!armed) return;      // commit without begin: no-op (defensive)
        armed = false;
        // The store already holds endValue from the drag's live writes; just record.
        pushStoreEdit<TModule>(module, std::move(label), std::move(setter),
                               startValue, endValue);
    }
};

} // namespace redDot
