#!/usr/bin/env python3
"""Sikit — the Tuning Expander panel (8HP). 12 per-degree CENTS knobs that retune Monsoon's
12-TET without touching the scale mask (see docs/design/TUNING_EXPANDER_SPEC.md).

Layout: a 3-column x 4-row grid of degree cells (C..B). Cell 0 (C, the root) is LOCKED to 0
cents — drawn as a static "0" plate with NO knob marker (the widget renders no interactive knob
for it; process() also clamps it). Cells 1..11 each emit a knob marker the widget binds a Trimpot
to, with the note name above and a small cents scale ring drawn around it.

nanosvg-safe: solid fills + strokes + closed arc paths only (no gradients/masks/text-as-path/url).
Note NAMES are drawn as <text> — Rack's nanovg renders panel <text>; the collection's other panels
put live text via widgets, but static note labels as SVG <text> are fine for a fixed 12-name legend.

Kit id markers (geometry single-sourced from the panel):
  param_cents_<i>   knob marker for degree i (1..11); degree 0 has NO marker (locked root)
  light_connect     ConnectMark position
"""
import math

HP = 8
W  = HP * 5.08
H  = 128.5
S  = 75 / 25.4
PW, PH = round(W*S, 2), round(H*S, 2)
def px(v): return round(v*S, 2)

NOTE = ["C","C#","D","D#","E","F","F#","G","G#","A","A#","B"]

THEMES = {
    "dark":  dict(bg="#16181c", red="#d4001a", ink="#f0f0f0", gold="#c8960c",
                  well="#0f1114", ring="#4a4a4a", knob="#2a2e34", knobring="#5a616a",
                  plate="#20242a", platehi="#3a4149", lockwell="#241f14", locktext="#c8960c",
                  sub="#8a94a0"),
    "light": dict(bg="#dcdcdc", red="#d4001a", ink="#1a1a1a", gold="#b07d00",
                  well="#e2ddd2", ring="#b0a898", knob="#c8cdd4", knobring="#9aa2ac",
                  plate="#cdd2d8", platehi="#e4e8ec", lockwell="#e8e0cc", locktext="#b07d00",
                  sub="#5a6470"),
}

# ── Grid geometry (3 columns x 4 rows) ───────────────────────────────────────
COLS, ROWS = 3, 4
MARGIN_X  = 4.6
TOP       = 20.0          # below the wordmark band
BOT_PAD   = 16.0          # room for the connect light at the base
CELL_W    = (W - 2*MARGIN_X) / COLS
GRID_H    = H - TOP - BOT_PAD
CELL_H    = GRID_H / ROWS
KNOB_R    = 3.0

def cell_center(i):
    r = i // COLS
    c = i % COLS
    cx = MARGIN_X + CELL_W*(c + 0.5)
    cy = TOP + CELL_H*(r + 0.5)
    return cx, cy

def gen(dark):
    t = THEMES["dark" if dark else "light"]
    o = []; A = o.append
    A(f'<svg xmlns="http://www.w3.org/2000/svg" width="{PW}" height="{PH}" viewBox="0 0 {PW} {PH}">')
    A(f'<rect width="{PW}" height="{PH}" fill="{t["bg"]}"/>')
    # top brand stripe
    A(f'<rect x="0" y="0" width="{PW}" height="{px(1.2)}" fill="{t["red"]}"/>')
    # Wordmark marker — the widget draws "Sikit" + "cents / degree" here (nanosvg ignores <text>).
    A(f'<circle id="wordmark" cx="{px(W/2)}" cy="{px(12.0)}" r="0.5" fill="none" stroke="none"/>')

    for i in range(12):
        cx, cy = cell_center(i)
        # NOTE: note NAMES + the wordmark are drawn by the WIDGET (nanosvg does not render <text>),
        # so here we emit only geometry (wells/plates/markers). Each cell exposes a cell_<i> marker
        # the widget uses to position that degree's note label; the knob marker is param_cents_<i>.
        A(f'<circle id="cell_{i}" cx="{px(cx)}" cy="{px(cy)}" r="0.5" fill="none" stroke="none"/>')
        if i == 0:
            # LOCKED ROOT: static "0" plate, no knob marker (widget draws the "0"/"root" text).
            A(f'<circle cx="{px(cx)}" cy="{px(cy)}" r="{px(KNOB_R)}" fill="{t["lockwell"]}" '
              f'stroke="{t["ring"]}" stroke-width="0.5"/>')
            continue
        # knob well + a little scale ring, then the marker
        A(f'<circle cx="{px(cx)}" cy="{px(cy)}" r="{px(KNOB_R + 0.6)}" fill="{t["well"]}" '
          f'stroke="{t["ring"]}" stroke-width="0.4"/>')
        A(f'<circle cx="{px(cx)}" cy="{px(cy)}" r="{px(KNOB_R)}" fill="{t["knob"]}" '
          f'stroke="{t["knobring"]}" stroke-width="0.5"/>')
        # default-position tick (pointing up = mid of 0..1200; purely decorative)
        A(f'<line x1="{px(cx)}" y1="{px(cy - KNOB_R + 0.4)}" x2="{px(cx)}" y2="{px(cy - KNOB_R + 1.4)}" '
          f'stroke="{t["gold"]}" stroke-width="0.5"/>')
        # the knob MARKER the widget binds to (invisible)
        A(f'<circle id="param_cents_{i}" cx="{px(cx)}" cy="{px(cy)}" r="0.5" fill="none" stroke="none"/>')

    # (No on-panel loaded-.scl name band — too tight on 8HP; the loaded tuning name lives in the
    # context menu instead.)

    # connect light near the base, centred
    lcx, lcy = W/2, H - BOT_PAD/2
    A(f'<circle cx="{px(lcx)}" cy="{px(lcy)}" r="{px(1.8)}" fill="{t["well"]}" '
      f'stroke="{t["ring"]}" stroke-width="0.3"/>')
    A(f'<circle id="light_connect" cx="{px(lcx)}" cy="{px(lcy)}" r="0.5" fill="none" stroke="none"/>')

    A('</svg>')
    return "\n".join(o)

def main():
    import os
    out = os.path.join(os.path.dirname(__file__), "..", "res", "panels")
    for dark, name in [(True, "Sikit_panel_dark.svg"), (False, "Sikit_panel_light.svg")]:
        with open(os.path.join(out, name), "w") as fh:
            fh.write(gen(dark))
        print(f"Sikit {'dark' if dark else 'light'}: res/panels/{name}  ({HP}HP, {PW}x{PH}px)")

if __name__ == "__main__":
    main()
