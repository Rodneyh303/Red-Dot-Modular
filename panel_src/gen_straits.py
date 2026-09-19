#!/usr/bin/env python3
"""Straits — poly expander panel (34HP), styled as the flowing straits between shores.

The refactored single Straits carries 16 REST + 16 ACCENT + 16 Q-MIX per-voice knobs (voice 1 =
mono/ch0, voices 2..16 = poly) plus five 16ch poly-cable outs (gate/step/sleg/CV/accent). The old
East/West split is gone — instead THREE knob banks sit side by side: REST (muted cool), ACCENT
(vibrant warm) and Q-MIX (purple), the tint itself the literal lane distinction. A field of flowing
contour "wave" lines runs behind each bank (the straits' water), tinted to each side. A voice spine
(1..16) on the far left organises rows; voice 1 (mono) is marked distinctly.

Q-MIX (Task 4 poly): per-voice level blending that voice's CV out between quantised input (0) and
internally-generated notes (1) — the poly twin of the mono Q-mix Level. "Goes where rest and accent
already are": a third bank exactly parallel to rest/accent.

nanosvg-safe (solid fills/strokes, no gradient/mask/text/url).

Kit id markers (widget binds; voice v 0..15, v0 = mono/voice 1):
  param_rest_<0..15>     REST probability knob   (v0 → mono REST_PARAM,   v1..15 → POLY_REST_PARAM_*)
  param_accent_<0..15>   ACCENT probability knob (v0 → mono ACCENT_KNOB,  v1..15 → POLY_ACCENT_PARAM_*)
  param_qmix_<0..15>     Q-MIX level knob        (v0 → mono QMIX_LEVEL_PARAM, v1..15 → POLY_QMIX_PARAM_*)
  output_polygate / output_polystepgate / output_polyslegato / output_polycv / output_polyaccent
  input_quantcv / param_voicecount / light_connect
"""
import math, os, re
HP = 34
W  = HP * 5.08
H  = 128.5
S  = 75 / 25.4
PW, PH = round(W*S, 2), round(H*S, 2)
def px(v): return round(v*S, 2)

THEMES = {
    "dark":  dict(bg="#14171b", red="#d4001a", ink="#f0f0f0",
                  # REST = muted cool; ACCENT = vibrant warm; QMIX = purple
                  rest="#3f7d78", restwave="#2a5a56", restknob="#1a2e2c",
                  acc="#e08a1a", accwave="#8a5410", accknob="#3a2a10",
                  qmix="#8060c0", qmixwave="#4e3a78", qmixknob="#241a3a",
                  spine="#5a6470", spinehi="#8a94a0", spinedot="#4c7ac0",
                  knobface="#2a2e33", knobring="#4a5058", knobtick="#c0c8d0",
                  jackwell="#0c0e11", jackring="#4a4a4a", gold="#c8960c",
                  panelmid="#1a1d21", wave_op=0.5),
    "light": dict(bg="#dcdcdc", red="#d4001a", ink="#1a1a1a",
                  rest="#5a9a94", restwave="#6fa8a2", restknob="#c8ddd9",
                  acc="#c88018", accwave="#d09a48", accknob="#e4d4b8",
                  qmix="#8a6ac8", qmixwave="#a087d0", qmixknob="#d8cceb",
                  spine="#b0b8c0", spinehi="#8a94a0", spinedot="#4c6ab0",
                  knobface="#e8e2d6", knobring="#b0a898", knobtick="#5a5040",
                  jackwell="#e2ddd2", jackring="#b0a898", gold="#b07d00",
                  panelmid="#d0d0d0", wave_op=0.75),
}

MARGIN   = 5.0
SPINE_W  = 6.0                  # voice spine on the far left
SPINE_CX = MARGIN + SPINE_W/2
GAP      = 3.0                  # gap between the three banks
# Three equal banks fill the remaining width right of the spine.
BANKS_X0 = MARGIN + SPINE_W + 2.0
BANK_W   = (W - BANKS_X0 - MARGIN - 2*GAP) / 3.0
TOP      = 16.0
N_ROWS   = 6                    # 3 cols x 6/6/4 (col-major: voices 1-6, 7-12, 13-16)
COLS     = [6, 6, 4]
ROW_H    = 14.77                # 43.6px -- Compact(tight) arc dia 43.4px kisses, never crosses
KNOB_R   = 4.5                  # painted preview under the Compact body (5.0mm)
GRID_TOP = TOP + 2.0
JACK_Y   = TOP + N_ROWS*ROW_H + 6.5   # 111.1mm; logo band below

def bank_x0(idx):  # left edge of bank idx (0=rest,1=accent,2=qmix)
    return BANKS_X0 + idx*(BANK_W + GAP)

def wave_field(A, t, x0, y0, w, h, colour, n=22):
    """Flowing contour lines (the straits' water) across (x0,y0,w,h). Dense field — many
    fine contours with varied amplitude/phase so it reads as moving water, not a few lines."""
    seg = 40
    for i in range(n):
        yy = y0 + h*i/(n-1)
        pts = []
        amp = 0.8 + (i % 4)*0.55
        phase = i*0.55
        freq = 0.55 + (i % 3)*0.15
        for k in range(seg+1):
            xx = x0 + w*k/seg
            wy = yy + amp*math.sin(k*freq + phase) + 0.35*math.sin(k*1.7 + phase*1.3)
            pts.append(f"{px(xx)},{px(wy)}")
        op = t["wave_op"] * (0.55 + 0.45*(i % 2))   # alternate darker/lighter for depth
        A(f'<polyline points="{" ".join(pts)}" fill="none" stroke="{colour}" '
          f'stroke-width="0.4" stroke-opacity="{op:.2f}"/>')

def knob(A, t, cx, cy, r, face, ring, mono=False):
    A(f'<circle cx="{px(cx)}" cy="{px(cy)}" r="{px(r)}" fill="{face}" '
      f'stroke="{ring}" stroke-width="{px(0.8 if mono else 0.5)}"/>')
    # pointer tick
    A(f'<line x1="{px(cx)}" y1="{px(cy)}" x2="{px(cx)}" y2="{px(cy-r*0.8)}" '
      f'stroke="{t["knobtick"]}" stroke-width="{px(0.5)}"/>')
    if mono:  # voice-1 mono ring accent
        A(f'<circle cx="{px(cx)}" cy="{px(cy)}" r="{px(r+1.0)}" fill="none" '
          f'stroke="{t["red"]}" stroke-width="{px(0.5)}" stroke-opacity="0.8"/>')

# ── dot.modular wordmark embed (nondestructive wordmark crop — strips positioning
#    brackets + blank margins via a translate/scale transform; source logo untouched). ──
_LOGO_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "res", "logo")
def logo_embed(dark, x_mm, y_mm, target_w_mm):
    path = os.path.join(_LOGO_DIR, "dot-modular-logo-dark.svg" if dark else "dot-modular-logo-light.svg")
    s = open(path).read()
    body = s[s.find('<g '):s.rfind('</svg>')]
    body = re.sub(r'<polyline\b[^>]*/>', '', body)   # strip the 2 positioning brackets
    CX, CY, CW = 26.0, 65.0, 657.0                   # wordmark crop (matches *-tight.svg)
    sc = px(target_w_mm) / CW
    tx, ty = px(x_mm), px(y_mm)
    return f'<g transform="translate({tx:.2f},{ty:.2f}) scale({sc:.5f}) translate({-CX:.2f},{-CY:.2f})">{body}</g>'

def gen(dark):
    t = THEMES["dark" if dark else "light"]
    o = []; A = o.append
    A(f'<svg xmlns="http://www.w3.org/2000/svg" width="{PW}" height="{PH}" viewBox="0 0 {PW} {PH}">')
    A(f'<rect width="{PW}" height="{PH}" fill="{t["bg"]}"/>')
    A(f'<rect x="0" y="0" width="{PW}" height="{px(1.2)}" fill="{t["red"]}"/>')

    bands = [("rest",   t["rest"], t["restwave"], t["restknob"]),
             ("accent", t["acc"],  t["accwave"],  t["accknob"]),
             ("qmix",   t["qmix"], t["qmixwave"], t["qmixknob"])]

    # ── wave fields + tint bands behind each of the three banks ──
    for idx, (kind, tint, wave, _knob) in enumerate(bands):
        x0 = bank_x0(idx)
        wave_field(A, t, x0, TOP, BANK_W, N_ROWS*ROW_H, wave)
        op_fill = 0.06 if kind == "rest" else 0.08
        A(f'<rect x="{px(x0-1)}" y="{px(TOP-4)}" width="{px(BANK_W+2)}" height="{px(N_ROWS*ROW_H+6)}" '
          f'rx="{px(1.5)}" fill="{tint}" fill-opacity="{op_fill}" stroke="{tint}" '
          f'stroke-width="0.3" stroke-opacity="0.45"/>')
        # bank colour marker (label text left implicit / drawn at runtime)
        A(f'<circle cx="{px(x0+BANK_W*0.5)}" cy="{px(TOP-6)}" r="{px(1.4)}" fill="{tint}"/>')

    # ── voice spine 1..16 (far left) ──
    A(f'<line x1="{px(SPINE_CX)}" y1="{px(TOP)}" x2="{px(SPINE_CX)}" y2="{px(TOP+N_ROWS*ROW_H)}" '
      f'stroke="{t["spine"]}" stroke-width="{px(0.6)}"/>')

    # ── knob grid: per bank, 3 columns 6/6/4 = 16, COLUMN-major ──
    # col 0 = voices 1-6 (v0..5, v0 = mono at top-left), col 1 = 7-12, col 2 = 13-16
    # (4-knob col vertically centred: offset one row).
    def bank(kind, x_base, col_face, col_ring, spine_col=False):
        cw = BANK_W/3
        v = 0
        for c, nrows in enumerate(COLS):
            roff = (N_ROWS - nrows) / 2.0
            for r in range(nrows):
                cx = x_base + cw*(c+0.5)
                cy = GRID_TOP + ROW_H*(r+roff+0.5)
                mono = (v == 0)
                knob(A, t, cx, cy, KNOB_R, col_face, col_ring, mono)
                A(f'<circle id="param_{kind}_{v}" cx="{px(cx)}" cy="{px(cy)}" r="0.5" fill="none" stroke="none"/>')
                # voice-number dots on the spine — drawn once (from the REST bank's first column,
                # which aligns row-for-row with all three banks). Marks voices 1..6; mono distinct.
                if spine_col and c == 0:
                    A(f'<circle cx="{px(SPINE_CX)}" cy="{px(cy)}" r="{px(0.7)}" '
                      f'fill="{t["spinedot"] if mono else t["spinehi"]}" fill-opacity="{1.0 if mono else 0.5}"/>')
                v += 1
    bank("rest",   bank_x0(0), t["restknob"], t["rest"], spine_col=True)
    bank("accent", bank_x0(1), t["accknob"],  t["acc"])
    bank("qmix",   bank_x0(2), t["qmixknob"], t["qmix"])

    # ── five poly-cable output jacks along the bottom ──
    # GATE (fused), STEP (un-fused), SLEG (step-legato: articulations inside slurs only),
    # CV, ACCENT. See LEGATO_TIE_MODEL_NOTE.md + STEP_GATE_IMPLEMENTATION.md.
    labels = [("output_polygate",      W*0.42, "GATE"),
              ("output_polystepgate",  W*0.53, "STEP"),
              ("output_polyslegato",   W*0.64, "SLEG"),
              ("output_polycv",        W*0.75, "CV"),
              ("output_polyaccent",    W*0.86, "ACC")]
    for jid, jx, _lab in labels:
        A(f'<circle cx="{px(jx)}" cy="{px(JACK_Y)}" r="{px(3.6)}" fill="{t["jackwell"]}" '
          f'stroke="{t["jackring"]}" stroke-width="0.6"/>')
        A(f'<circle cx="{px(jx)}" cy="{px(JACK_Y)}" r="{px(1.6)}" fill="none" stroke="{t["gold"]}" stroke-width="0.4"/>')
        A(f'<circle id="{jid}" cx="{px(jx)}" cy="{px(JACK_Y)}" r="0.5" fill="none" stroke="none"/>')
    # a wave sweeping under the jacks (the straits continuing)
    wave_field(A, t, MARGIN, JACK_Y+5.5, W-2*MARGIN, 5.5, t["spine"], n=7)

    # ── poly voice-count knob (stepped 1..16, set-and-forget). Sits at the bottom-left,
    #    left of the output strip, so the global "how many voices" reads apart from the
    #    per-voice/per-signal jacks. Slot-style widget bound in the widget. ──
    vc_x, vc_y = MARGIN + 6.0, JACK_Y
    A(f'<circle cx="{px(vc_x)}" cy="{px(vc_y)}" r="{px(3.2)}" fill="{t["knobface"]}" '
      f'stroke="{t["knobring"]}" stroke-width="0.6"/>')
    A(f'<circle id="param_voicecount" cx="{px(vc_x)}" cy="{px(vc_y)}" r="0.5" fill="none" stroke="none"/>')
    A(f'<text x="{px(vc_x)}" y="{px(vc_y+5.8)}" fill="{t["ink"]}" font-family="sans-serif" '
      f'font-size="{px(2.0)}" text-anchor="middle" opacity="0.75">VOICES</text>')

    # ── quantiser CV IN jack (Q2). 16ch poly note-CV source for the quantiser modes (C/D):
    #    ch1 = mono, ch2.. = poly. Sits between the voice-count knob and the output strip, on
    #    the INPUT side (a blue-ringed well distinguishes it from the gold-ringed outputs). ──
    q_x = MARGIN + 13.5
    A(f'<circle cx="{px(q_x)}" cy="{px(JACK_Y)}" r="{px(3.6)}" fill="{t["jackwell"]}" '
      f'stroke="{t["jackring"]}" stroke-width="0.6"/>')
    A(f'<circle cx="{px(q_x)}" cy="{px(JACK_Y)}" r="{px(1.6)}" fill="none" stroke="{t["spinedot"]}" stroke-width="0.5"/>')
    A(f'<circle id="input_quantcv" cx="{px(q_x)}" cy="{px(JACK_Y)}" r="0.5" fill="none" stroke="none"/>')
    A(f'<text x="{px(q_x)}" y="{px(JACK_Y+5.8)}" fill="{t["ink"]}" font-family="sans-serif" '
      f'font-size="{px(2.0)}" text-anchor="middle" opacity="0.75">Q-CV</text>')

    lcx = W - MARGIN - 3
    A(f'<circle cx="{px(lcx)}" cy="{px(JACK_Y)}" r="{px(1.6)}" fill="{t["jackwell"]}" stroke="{t["jackring"]}" stroke-width="0.3"/>')
    A(f'<circle id="light_connect" cx="{px(lcx)}" cy="{px(JACK_Y)}" r="0.5" fill="none" stroke="none"/>')
    # dot.modular wordmark — centred horizontally, lower band (a bit below the Sands panels' y≈113)
    A(logo_embed(dark, (W - 34.0) / 2.0, 120.5, 34.0))
    A('</svg>')
    return "\n".join(o)

def main():
    import os
    out = os.path.join(os.path.dirname(__file__), "..", "res", "panels")
    for dark, name in [(True, "Straits_panel_dark.svg"), (False, "Straits_panel_light.svg")]:
        with open(os.path.join(out, name), "w") as fh:
            fh.write(gen(dark))
        print(f"Straits {'dark' if dark else 'light'}: res/panels/{name}  ({HP}HP, {PW}x{PH}px)")

if __name__ == "__main__":
    main()
