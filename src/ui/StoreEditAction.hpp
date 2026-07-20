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
//       float oldV = read(store);                        // BEFORE the edit
//       redDot::applyAndPushStoreEdit<Monsoon>(mon, "change lane owner",
//           [voice, lane](Monsoon& m, float v) { m.editor.owner[voice][lane] = (int)v; },
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
// VALUES are carried as float; integer cells (owners, directions) round-trip exactly —
// cast inside the setter. Equal old/new never records (no zero-size undo steps).

#include <rack.hpp>
#include <functional>
#include <string>
#include <utility>

namespace redDot {

template <typename TModule>
struct StoreEditAction : rack::history::Action {
    int64_t moduleId;
    std::function<void(TModule&, float)> setter;
    float oldValue;
    float newValue;

    StoreEditAction(std::string label, int64_t moduleId_,
                    std::function<void(TModule&, float)> setter_,
                    float oldValue_, float newValue_)
        : moduleId(moduleId_), setter(std::move(setter_)),
          oldValue(oldValue_), newValue(newValue_) {
        name = std::move(label);   // shows in Edit menu as "Undo <name>"
    }

    void apply(float v) {
        // Re-resolve every time: the module may have been deleted (or deleted and
        // undo-restored, in which case Rack re-creates it under the SAME id — this is
        // why an id outlives a pointer).
        auto* m = dynamic_cast<TModule*>(APP->engine->getModule(moduleId));
        if (m && setter) setter(*m, v);
    }
    void undo() override { apply(oldValue); }
    void redo() override { apply(newValue); }
};

// Record an ALREADY-APPLIED edit. No-ops on null module or unchanged value.
template <typename TModule>
inline void pushStoreEdit(TModule* module, std::string label,
                          std::function<void(TModule&, float)> setter,
                          float oldValue, float newValue) {
    if (!module) return;
    if (oldValue == newValue) return;
    APP->history->push(new StoreEditAction<TModule>(
        std::move(label), module->id, std::move(setter), oldValue, newValue));
}

// Apply the edit AND record it — the one-call form for click-style edits.
template <typename TModule>
inline void applyAndPushStoreEdit(TModule* module, std::string label,
                                  std::function<void(TModule&, float)> setter,
                                  float oldValue, float newValue) {
    if (!module) return;
    if (oldValue == newValue) return;
    setter(*module, newValue);
    pushStoreEdit<TModule>(module, std::move(label), std::move(setter), oldValue, newValue);
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
