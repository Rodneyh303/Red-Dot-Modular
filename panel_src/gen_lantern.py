#!/usr/bin/env python3
"""Lantern panel — note-output visualiser (read-only observer).

WIDE display module, sized like the Sands East visual (Monsoon Sands): the
display dominates. 41HP (208.28mm) x 128.5mm (3U), native 75 DPI.

Unlike Sands East (which reserves ~half the width for per-lane CV jacks/attens),
Lantern is read-only — no per-lane controls — so the 16-lane display takes the
FULL width and near-full height. A slim left gutter (inside the display, drawn by
the widget) carries the note-name labels; a slim right strip carries the per-voice
velocity/accent dots. A compact control strip sits along the bottom.

nanosvg-safe: solid fills, no gradients/masks/filters; no <text> for labels (the
widget draws note names). Screws via C++ RedScrew. dot.modular dark/light.
"""
import os

# 41HP, 3U. 1HP = 5.08mm.
W_MM, H_MM = 208.28, 128.5
S = 75 / 25.4
def px(mm): return round(mm * S, 2)

# Display box: near-full width and height. Small margins; header band on top,
# control strip on the bottom. 16 lanes fill the vertical space.
DISP_X, DISP_Y = 6.0, 16.0
DISP_W = W_MM - 12.0            # ~196mm — full width less small margins
DISP_H = 96.0                  # tall — 16 lanes use most of the height
N_LANES = 16
LANE_H = DISP_H / N_LANES       # ~6mm per lane (narrow, as specified)

THEMES = {
    "dark":  dict(bg="#16181c", ink="#d8d8dc", dim="#8a8f98", red="#d4001a",
                  redsoft="#dc2626", gold="#c8960c", line="#3a3a3e",
                  recess="#101216", recessring="#2a2f37", tabband="#202833",
                  well="#0f1114", wellring="#3a4048", lanesep="#20242b"),
    "light": dict(bg="#e8e8ea", ink="#2a2a2e", dim="#888d96", red="#d4001a",
                  redsoft="#c0001a", gold="#a07808", line="#c8ccd2",
                  recess="#d8dade", recessring="#c0c4ca", tabband="#cdd4dc",
                  well="#d4d6d9", wellring="#a8aeb6", lanesep="#c4c8ce"),
}


def lantern_glow(t, cx_mm, cy_mm):
    o = ['<g>']
    cx, cy = px(cx_mm), px(cy_mm)
    o.append(f'<circle cx="{cx:.1f}" cy="{cy:.1f}" r="{px(1.4):.1f}" fill="{t["red"]}" fill-opacity="0.9"/>')
    for i, r in enumerate((3.0, 4.6, 6.2)):
        op = 0.5 - i * 0.14
        o.append(f'<circle cx="{cx:.1f}" cy="{cy:.1f}" r="{px(r):.1f}" fill="none" '
                 f'stroke="{t["red"]}" stroke-width="{px(0.4):.1f}" stroke-opacity="{op:.2f}"/>')
    o.append('</g>')
    return "\n".join(o)


def trim_well(t, x_mm, y_mm, cid, r_mm=5.0):
    x, y = px(x_mm), px(y_mm)
    return (f'<circle id="{cid}" cx="{x:.1f}" cy="{y:.1f}" r="{px(r_mm):.1f}" '
            f'fill="{t["well"]}" stroke="{t["wellring"]}" stroke-width="{px(0.4):.1f}"/>')


def button_well(t, x_mm, y_mm, w_mm, h_mm, cid):
    x, y, w, h = px(x_mm), px(y_mm), px(w_mm), px(h_mm)
    return (f'<rect id="{cid}" x="{x:.1f}" y="{y:.1f}" width="{w:.1f}" height="{h:.1f}" '
            f'rx="{px(1.0):.1f}" fill="{t["well"]}" stroke="{t["wellring"]}" '
            f'stroke-width="{px(0.4):.1f}"/>')


def panel(theme):
    t = THEMES[theme]
    PW, PH = px(W_MM), px(H_MM)
    o = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{PW:.1f}" height="{PH:.1f}" '
         f'viewBox="0 0 {PW:.1f} {PH:.1f}">']
    o.append(f'<rect x="0" y="0" width="{PW:.1f}" height="{PH:.1f}" fill="{t["bg"]}"/>')

    # header band + title glow
    o.append(f'<rect x="0" y="0" width="{PW:.1f}" height="{px(13):.1f}" fill="{t["red"]}" opacity="0.06"/>')
    o.append(f'<line x1="0" y1="{px(13):.1f}" x2="{PW:.1f}" y2="{px(13):.1f}" '
             f'stroke="{t["red"]}" stroke-width="{px(0.5):.1f}" opacity="0.5"/>')
    o.append(lantern_glow(t, cx_mm=W_MM/2, cy_mm=7.0))

    o.append('<g id="components">')

    # ── the wide display recess (widget draws all lane content on top) ──
    #    tab band above it (like Sands East) for the view-mode indicator strip.
    o.append(f'<rect x="{px(DISP_X):.1f}" y="{px(DISP_Y-4):.1f}" width="{px(DISP_W):.1f}" '
             f'height="{px(3.5):.1f}" rx="{px(1):.1f}" fill="{t["tabband"]}" opacity="0.6"/>')
    o.append(f'<rect id="display_lantern" x="{px(DISP_X):.1f}" y="{px(DISP_Y):.1f}" '
             f'width="{px(DISP_W):.1f}" height="{px(DISP_H):.1f}" rx="{px(1.5):.1f}" '
             f'fill="{t["recess"]}" stroke="{t["recessring"]}" stroke-width="1"/>')
    # faint lane separators (16) so the empty panel reads as a 16-lane grid
    for i in range(1, N_LANES):
        ly = px(DISP_Y + i * LANE_H)
        o.append(f'<line x1="{px(DISP_X):.1f}" y1="{ly:.1f}" x2="{px(DISP_X+DISP_W):.1f}" '
                 f'y2="{ly:.1f}" stroke="{t["lanesep"]}" stroke-width="0.5" opacity="0.5"/>')
    # faint bar dividers (4 bars of 4 steps) — vertical, inside a left label gutter
    GUTTER = 16.0  # mm reserved at left for note labels (widget draws them)
    DOTS   = 10.0  # mm at right for velocity/accent dots (widget draws them)
    grid_x0 = DISP_X + GUTTER
    grid_w  = DISP_W - GUTTER - DOTS
    for b in range(1, 4):
        bx = px(grid_x0 + b * grid_w / 4.0)
        o.append(f'<line x1="{bx:.1f}" y1="{px(DISP_Y):.1f}" x2="{bx:.1f}" '
                 f'y2="{px(DISP_Y+DISP_H):.1f}" stroke="{t["lanesep"]}" stroke-width="0.6" opacity="0.6"/>')

    # ── bottom control strip: VIEW (3 buttons) · ZOOM knob · FOLLOW ──
    cy = 118.0
    o.append(button_well(t, 10,  cy-3.5, 20, 7, cid="param_VIEW_NOTES"))
    o.append(button_well(t, 33,  cy-3.5, 20, 7, cid="param_VIEW_VELOCITY"))
    o.append(button_well(t, 56,  cy-3.5, 20, 7, cid="param_VIEW_PROB"))
    o.append(trim_well(t, W_MM/2, cy, cid="param_ZOOM", r_mm=6.5))
    o.append(button_well(t, W_MM-30, cy-4, 20, 8, cid="param_FOLLOW"))

    # connect-mark anchor (footer centre)
    o.append(f'<circle id="light_connect" cx="{px(W_MM/2):.1f}" cy="{px(125.5):.1f}" '
             f'r="0.5" fill="#000000" fill-opacity="0"/>')
    o.append('</g>')
    o.append('</svg>')
    return "\n".join(o)


if __name__ == "__main__":
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    for theme in ("dark", "light"):
        out = os.path.join(root, f"res/panels/Lantern_panel_{theme}.svg")
        open(out, "w").write(panel(theme))
        print(f"wrote {out}")
