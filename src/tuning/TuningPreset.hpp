#pragma once
// ── dotModular::TuningPreset — the .dmtune portable tuning+scale format ──────────────────────────
// A dot.modular-native JSON preset storing the FULL Micro state — ALL per-degree cents AND ALL fader
// weights — so it round-trips LOSSLESSLY back into a Micro (unlike .scl, which is a lossy ordered
// pitch list that drops inactive degrees' tuning). Purpose: a portable personal LIBRARY of tuning+
// scale setups, movable between patches. See docs/design/TUNING_PRESET_FORMAT.md.
//
// Uses Rack's bundled jansson (json_t) — same dependency as dataToJson — so no new library. This is a
// UI-thread facility (file I/O); never call from process().
//
// Schema (version 2 — ENABLED_MASK_BUILD_BRIEF): a .dmtune carries the TUNING (cents) + the SCALE MASK
// (enabled), NOT weight — weight is the user's live fader mix, never in a preset (a scale masks, it
// never moves your faders).
//   { "format":"dotmodular.tuning", "version":2, "n":12,
//     "cents":[0.0, 90.58, ...n], "enabled":[true,false, ...n],
//     "name":"optional", "notes":"optional" }
// v1 migration on load: cents kept; enabled[i] = (v1 weight[i] > 0); v1 weight discarded.

#include <rack.hpp>
#include <string>
#include <vector>
#include <functional>

namespace dotModular {

struct TuningPreset {
    static constexpr int MAXN = 24;

    int   n = 12;
    float cents[MAXN]   = {};
    bool  enabled[MAXN] = {};   // scale mask (v2). Load defaults all-true when absent.
    // MONSOON_SCALE_AUTHORING (D3): a scale-only .dmtune (authored on a 12-TET Monsoon) carries a real
    // 12-TET cents ladder so it's a valid full preset (Colonnades can load it as tuning+mask), plus this
    // UI hint so a loader can LABEL it as scale-only. Purely advisory — the file is fully valid either way.
    bool  scaleOnly = false;
    // TONIC_TRANSPOSE_BUILD_BRIEF: a scale-only .dmtune whose enabled[] mask is ROOT-RELATIVE (tonic
    // normalised to degree 0) and meant to be transposed by the LIVE root control (Monsoon scale-menu
    // root / Shophouse front root), exactly like a built-in scale. When false, the mask is ABSOLUTE
    // (no tonic designated). Microtonal .dmtune (cents-carrying) never sets this — arbitrary tunings
    // don't scale-transpose. Monsoon is 12-TET only.
    bool  transposable = false;
    std::string name;
    std::string notes;

    enum class Status { Ok, IoError, ParseError, RejectedByPredicate };
    Status status = Status::Ok;
    std::string errorMessage;
    bool ok() const { return status == Status::Ok; }
};

// Save the given preset to `filePath` as .dmtune JSON. Returns true on success. `acceptFn` is not
// needed on write (the caller owns n). UI thread only.
inline bool saveTuningPreset(const std::string& filePath, const TuningPreset& p) {
    json_t* root = json_object();
    json_object_set_new(root, "format", json_string("dotmodular.tuning"));
    json_object_set_new(root, "version", json_integer(2));
    json_object_set_new(root, "n", json_integer(p.n));
    json_t* jc = json_array();
    json_t* je = json_array();
    for (int i = 0; i < p.n && i < TuningPreset::MAXN; ++i) {
        json_array_append_new(jc, json_real((double)p.cents[i]));
        json_array_append_new(je, json_boolean(p.enabled[i]));   // v2: scale mask, NOT weight
    }
    json_object_set_new(root, "cents", jc);
    json_object_set_new(root, "enabled", je);
    if (p.scaleOnly)      json_object_set_new(root, "scaleOnly", json_boolean(true));
    if (p.transposable)   json_object_set_new(root, "transposable", json_boolean(true));
    if (!p.name.empty())  json_object_set_new(root, "name",  json_string(p.name.c_str()));
    if (!p.notes.empty()) json_object_set_new(root, "notes", json_string(p.notes.c_str()));

    int rc = json_dump_file(root, filePath.c_str(), JSON_INDENT(2) | JSON_REAL_PRECISION(10));
    json_decref(root);
    return rc == 0;
}

// Load a .dmtune from `filePath`. `acceptFn(n)` (optional) lets the caller reject a mismatched degree
// count (e.g. a Micro-12 accepts only n==12). UI thread only.
inline TuningPreset loadTuningPreset(
    const std::string& filePath,
    std::function<bool(int)> acceptFn = nullptr,
    const std::string& rejectMessage = "This tuning preset has an unsupported degree count.") {

    TuningPreset p;
    json_error_t err;
    json_t* root = json_load_file(filePath.c_str(), 0, &err);
    if (!root) {
        p.status = TuningPreset::Status::IoError;
        p.errorMessage = std::string("Could not read .dmtune: ") + err.text;
        return p;
    }

    json_t* jfmt = json_object_get(root, "format");
    if (!jfmt || std::string(json_string_value(jfmt) ? json_string_value(jfmt) : "") != "dotmodular.tuning") {
        p.status = TuningPreset::Status::ParseError;
        p.errorMessage = "Not a dot.modular tuning preset (missing format tag).";
        json_decref(root);
        return p;
    }

    json_t* jn = json_object_get(root, "n");
    p.n = jn ? (int)json_integer_value(jn) : 12;
    if (p.n < 1 || p.n > TuningPreset::MAXN) {
        p.status = TuningPreset::Status::ParseError;
        p.errorMessage = "Tuning preset has an out-of-range degree count.";
        json_decref(root);
        return p;
    }
    if (acceptFn && !acceptFn(p.n)) {
        p.status = TuningPreset::Status::RejectedByPredicate;
        p.errorMessage = rejectMessage;
        json_decref(root);
        return p;
    }

    auto readArr = [&](const char* key, float* dst) {
        json_t* arr = json_object_get(root, key);
        if (!json_is_array(arr)) return;
        for (int i = 0; i < p.n && i < (int)json_array_size(arr); ++i) {
            json_t* v = json_array_get(arr, i);
            if (json_is_number(v)) dst[i] = (float)json_number_value(v);
        }
    };
    readArr("cents", p.cents);
    // enabled mask (v2). Default all-true, then read: v2 reads the enabled[] array; v1 migrates from
    // weight[] (enabled = weight>0), then discards weight (loudness is never carried by a preset).
    for (int i = 0; i < p.n && i < TuningPreset::MAXN; ++i) p.enabled[i] = true;
    json_t* jver = json_object_get(root, "version");
    const int version = jver ? (int)json_integer_value(jver) : 1;
    if (json_t* je = json_object_get(root, "enabled"); json_is_array(je)) {
        for (int i = 0; i < p.n && i < (int)json_array_size(je); ++i)
            p.enabled[i] = json_boolean_value(json_array_get(je, i));
    } else if (version < 2) {
        if (json_t* jw = json_object_get(root, "weight"); json_is_array(jw))
            for (int i = 0; i < p.n && i < (int)json_array_size(jw); ++i) {
                json_t* v = json_array_get(jw, i);
                p.enabled[i] = json_is_number(v) && json_number_value(v) > 0.0;
            }
    }
    if (json_t* jso = json_object_get(root, "scaleOnly")) p.scaleOnly = json_boolean_value(jso);
    if (json_t* jtr = json_object_get(root, "transposable")) p.transposable = json_boolean_value(jtr);
    if (json_t* jm = json_object_get(root, "name"))  if (json_is_string(jm)) p.name  = json_string_value(jm);
    if (json_t* jt = json_object_get(root, "notes")) if (json_is_string(jt)) p.notes = json_string_value(jt);

    json_decref(root);
    p.status = TuningPreset::Status::Ok;
    return p;
}

} // namespace dotModular
