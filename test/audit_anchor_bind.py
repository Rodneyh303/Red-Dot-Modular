#!/usr/bin/env python3
"""audit_anchor_bind.py — the anchor-vs-bind guard for SvgPanelKit widgets.

Root-cause guard for the Sands regression class: a generated panel and its widget
are two independent geometry sources. This audit makes them ONE by asserting, at
build/CI time:

  1. every anchor the generator emits has a matching bind in the widget, AND
  2. every bind the widget makes resolves to an anchor in the panel, AND
  3. anchor counts equal the SandsGrid lane constants (where declared).

"A control is missing" thus becomes a build failure, not a hover-discovery.

The audit is CONFIG-DRIVEN (see MODULES below). A module is only checked once it
has been migrated to bind-by-name — unmigrated modules are listed under
UNMIGRATED with a reason, so the file documents the remaining work without
failing the suite prematurely.

Anchor model (from panel_src/dotmod_design.py::kit_shape and the generators):
  <circle id="<kind>_<idx-or-name>" ... fill="none" stroke="none"/>
  inside a <g inkscape:label="components" ...> layer.

Bind model (from src/ui/SvgPanelKit.hpp):
  bindParam / bindInput / bindOutput / bindLight / bindLightParam /
  bindWidget / bindChild  — first string arg is the anchor name.
  Variadic bindParams/bindInputs/... use  prefix + std::to_string(i)  → the
  audit expands prefixed families by matching every anchor with that prefix.

Usage:
  python3 test/audit_anchor_bind.py            # audit all migrated modules
  python3 test/audit_anchor_bind.py sands_mono # filter by config name
Exit non-zero on any mismatch.
"""
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, ".."))


def p(*parts):
    return os.path.join(ROOT, *parts)


# ── Config ───────────────────────────────────────────────────────────────────
# Each migrated module: the widget source(s) and the panel SVG(s) it loadPanel()s.
# Only the DARK svg is parsed for anchors (dark + light are generated identically
# for the components layer). 'counts' asserts anchor-family sizes against the
# SandsGrid constants, catching lane-count divergence (the 6→7 q-mix bug class).
MODULES = {
    "sands_mono": {
        "widget": ["src/MonsoonSandsVisualExpander.cpp"],
        "panel": "res/panels/SandsMonoVisual_48HP.svg",
        # family-prefix -> expected count (int) or SandsGrid symbol name (str)
        "counts": {
            "input_cv_": 21,          # 7 lanes x 3 (LEN/OFF/ROT)
            "param_atten_": 21,
            "param_spr_": 5,          # 5 poly spread lanes
            "input_sprcv_": 5,
            "param_spratten_": 5,
            "param_owner_": 5,
            "param_dir_": 7,          # 7 mono lanes
            "input_dir_mod_": 7,
            "input_deleg_mod_": 5,
            "output_prob_": 7,
        },
    },
    "sands_east": {
        "widget": ["src/StraitsEastSandsVisual.cpp"],
        "panel": "res/panels/StraitsEastSandsVisual_48HP.svg",
        # Migrated to DESCRIPTIVE anchors (matches Mono) — the old numeric param_/input_
        # convention collided (param_4 = SPREAD_Q AND an atten slot via a base-4-vs-5
        # off-by-one). Per-family counts now pin every control.
        "counts": {
            "input_cv_": 20,          # 5 poly lanes × 4 (LEN/OFF/ROT/SPR)
            "param_atten_": 20,
            "param_spr_": 5,          # 5 poly spread bases (editor-lane indexed)
            "input_varlegcv_": 6,     # VAR/LEG × 3 (LEN/OFF/ROT)
            "param_varlegatten_": 6,
            "param_owner_": 7,        # 5 poly (incl QMIX) + VAR/LEG
            "param_dir_": 7,
            "input_dir_mod_": 7,
            "input_deleg_mod_": 7,
            "output_prob_": 5,        # 5 poly prob outs (incl QMIX)
        },
    },
}

# Modules known NOT yet migrated — documented, not audited (no false green either).
UNMIGRATED = {
    "sands_macro": "Stage 3: MIX-IN block anchor-derived; residual mm2px placements pending.",
}


# ── SVG anchor parsing ───────────────────────────────────────────────────────
_COMPONENTS_RE = re.compile(
    r'<g[^>]*inkscape:label="components"[^>]*>(.*?)</g>', re.DOTALL)
_ID_RE = re.compile(r'id="([^"]+)"')


def read_anchors(svg_path):
    """Return the set of anchor ids inside the components layer of an SVG."""
    with open(svg_path, encoding="utf-8") as f:
        text = f.read()
    m = _COMPONENTS_RE.search(text)
    if not m:
        raise RuntimeError(
            f"{os.path.relpath(svg_path, ROOT)}: no <g inkscape:label=\"components\"> "
            f"layer found (generator must emit a kit anchor layer)")
    layer = m.group(1)
    return set(_ID_RE.findall(layer))


# ── Widget bind parsing ──────────────────────────────────────────────────────
# A bind's anchor name is the FIRST STRING ARGUMENT of a bind*() call — either a
# LITERAL ("param_dir_3") or a PREFIX built by concatenation ("param_dir_" + i).
# Only that first string arg is an anchor name; every other quoted string in the
# call (labels, undo-action names) must be IGNORED. This is why we parse each call
# site's argument list and take only its leading string token, rather than
# scanning all quoted strings.
#
#   bindParam/Input/Output/Light/LightParam/Widget/Child/StoreKnob → 1st str = name
#   bindParams/Inputs/Outputs/Lights/ParamsN/InputsN/OutputsN      → 1st str = prefix
# bindStoreKnob's 1st arg is `this`, its FIRST STRING arg is the shape name — the
# "first string token" rule handles that uniformly.
# Capture the bind METHOD NAME so we can tell single-name binders from prefix ones.
_BIND_SITE_RE = re.compile(
    r'\b(bind(?:Param|Input|Output|Light|LightParam|Widget|Child|StoreKnob'
    r'|Params|Inputs|Outputs|Lights|ParamsN|InputsN|OutputsN))\s*'
    r'(?:<[^>]*>)?\s*\(', re.DOTALL)
_FINDNAMED_RE = re.compile(r'\bfindNamed\s*\(\s*"([^"]+)"')
# Explicit prefix binders: their leading string is ALWAYS a prefix (count/pack forms).
_PREFIX_FAMILIES = ("bindParams", "bindInputs", "bindOutputs", "bindLights",
                    "bindParamsN", "bindInputsN", "bindOutputsN")
# Leading-string token of a call's argument text: first "..." possibly followed by +.
_FIRST_STR_RE = re.compile(r'"([^"]*)"\s*(\+?)')


def _args_after(text, open_paren_idx):
    """Return the substring of the balanced (...) beginning at open_paren_idx."""
    depth = 0
    for i in range(open_paren_idx, len(text)):
        c = text[i]
        if c == '(':
            depth += 1
        elif c == ')':
            depth -= 1
            if depth == 0:
                return text[open_paren_idx + 1:i]
    return text[open_paren_idx + 1:]


def read_binds(widget_paths):
    """Return (exact_names, prefixes) the widget binds/consumes by name."""
    exact, prefixes = set(), set()
    for wp in widget_paths:
        with open(p(wp), encoding="utf-8") as f:
            text = f.read()
        # findNamed(...) direct consumption (labels, editor recess, etc.).
        exact.update(_FINDNAMED_RE.findall(text))
        for m in _BIND_SITE_RE.finditer(text):
            method = m.group(1)                  # e.g. "bindInput", "bindInputsN"
            family_is_prefix = method in _PREFIX_FAMILIES
            args = _args_after(text, m.end() - 1)
            fm = _FIRST_STR_RE.search(args)
            if not fm:
                continue                         # no string name → nothing to bind
            name, plus = fm.group(1), fm.group(2)
            if plus == "+" or family_is_prefix:  # concat prefix OR explicit prefix binder
                prefixes.add(name)
            else:
                exact.add(name)
    return exact, prefixes


# ── Audit one module ─────────────────────────────────────────────────────────
def audit(name, cfg):
    errs = []
    svg_path = p(cfg["panel"])
    if not os.path.isfile(svg_path):
        return [f"{name}: panel not found: {cfg['panel']} (run the generator first)"]
    anchors = read_anchors(svg_path)
    exact, prefixes = read_binds(cfg["widget"])

    # Resolve each bind (exact or prefix-family) against the anchor set.
    bound_anchors = set()
    for nm in exact:
        if nm in anchors:
            bound_anchors.add(nm)
        else:
            errs.append(f"{name}: BIND with no anchor — bind(\"{nm}\") "
                        f"resolves to nothing in {cfg['panel']}")
    for pref in prefixes:
        fam = {a for a in anchors if a.startswith(pref)}
        if not fam:
            errs.append(f"{name}: PREFIX bind with no anchors — "
                        f"bind*(\"{pref}\") matched 0 anchors in {cfg['panel']}")
        bound_anchors |= fam

    # Every anchor must be bound (a control the generator drew but the widget
    # never places = the "missing control" bug).
    for a in sorted(anchors):
        if a not in bound_anchors:
            errs.append(f"{name}: ANCHOR with no bind — anchor \"{a}\" in "
                        f"{cfg['panel']} is never bound by the widget")

    # Count assertions (lane-divergence guard).
    for pref, expect in cfg.get("counts", {}).items():
        got = len([a for a in anchors if a.startswith(pref)])
        want = expect if isinstance(expect, int) else resolve_grid_const(expect)
        if got != want:
            errs.append(f"{name}: COUNT mismatch — {got} anchors with prefix "
                        f"\"{pref}\", expected {want}"
                        + (f" ({expect})" if isinstance(expect, str) else ""))
    return errs


# SandsGrid constant lookup (kept minimal; extend as configs reference symbols).
_GRID_VALUES = {
    "MONO_LANES": 7,
    "POLY_LANES": 5,
    "EAST_LANES": 7,
}


def resolve_grid_const(sym):
    if sym not in _GRID_VALUES:
        raise RuntimeError(f"unknown SandsGrid symbol in config: {sym}")
    return _GRID_VALUES[sym]


# ── Main ─────────────────────────────────────────────────────────────────────
def main(argv):
    flt = argv[1] if len(argv) > 1 else ""
    all_errs = []
    ran = 0
    for name, cfg in MODULES.items():
        if flt and flt not in name:
            continue
        ran += 1
        errs = audit(name, cfg)
        if errs:
            all_errs.extend(errs)
            print(f"  \033[31m✗\033[0m  {name}")
            for e in errs:
                print(f"       {e}")
        else:
            n = len(read_anchors(p(cfg["panel"])))
            print(f"  \033[32m✓\033[0m  {name}  ({n} anchors, all bound)")

    if not flt:
        for name, why in UNMIGRATED.items():
            print(f"  \033[33m…\033[0m  {name} (not yet migrated) — {why}")

    print("-----")
    if all_errs:
        print(f"anchor/bind audit: {len(all_errs)} problem(s) across {ran} module(s)")
        return 1
    print(f"anchor/bind audit: all {ran} migrated module(s) consistent")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
