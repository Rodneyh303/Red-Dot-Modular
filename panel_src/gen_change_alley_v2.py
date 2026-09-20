#!/usr/bin/env python3
"""Change Alley V2 -- ONE module: pin matrix + transforms. 48HP.
Grid ~V1 size (99.6mm); the extra width goes to GENEROUS control spacing, not the grid.

Column order per side, OUTER -> INNER:
  J_DOM  J_COD | KNOB1(grain)  KNOB2(leader/step/scatter-dom-back)  J_BACK2(scatter-cod-back)
  | BTN_D  BTN_C | LIGHT
Right side mirrored, jacks to the outside. Labels widget-drawn.
"""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from dotmod_design import px, svg_open, logo_embed, jack, trim

PH_MM  = 128.5
import math

# ── HP DERIVED from the widest row's component count (CA widen pass) ────────────────────────
# The widest row is SCATTER: 5 jacks + 5 buttons + 1 grain dial + 1 light per side. We lay the
# columns at element-appropriate pitch (jacks/dial 8.5mm, buttons clustered 6.0mm), measure the
# control-strip width, keep the 16×16 matrix at its established ~99.6mm, then round UP to whole HP
# and absorb the slack into the gutter. So HP is COMPUTED, not chosen.
MARGIN   = 6.0
JACK_P   = 8.5             # jack / dial column pitch (PJ301M Ø7.8 + breathing)
BTN_P    = 6.0            # button cluster pitch (tighter — buttons are small)
J_HALF   = 4.25           # half a jack, for edge clearance

# Jack/dial group (outer→inner): 5 columns at JACK_P.
J_DOM   = MARGIN + J_HALF                 # fwd domain trig jack
J_COD   = J_DOM  + JACK_P                 # fwd codomain trig jack
KNOB1   = J_COD  + JACK_P                 # grain dial (all verbs)
KNOB2   = KNOB1  + JACK_P                 # leader/step dial OR scatter domain-back jack
J_BACK2 = KNOB2  + JACK_P                 # scatter codomain-back jack
# Button cluster (after a jack→button gap): 5 buttons at BTN_P.
BTN_D   = J_BACK2 + (J_HALF + 3.0)        # fwd domain fire
BTN_C   = BTN_D  + BTN_P                  # fwd codomain fire
REV_D   = BTN_C  + BTN_P                  # Philox reverse domain (ON-ROW now, was jammed)
REV_C   = REV_D  + BTN_P                  # Philox reverse codomain
TRUE_REV_BTN = REV_C + BTN_P             # true-reverse button
# True-reverse jack (jack pitch clearance from the button cluster), then the pending light.
TRUE_REV_IN  = TRUE_REV_BTN + (3.0 + J_HALF)
LIGHT   = TRUE_REV_IN + 5.25
CTRL_W  = LIGHT + 4.0

GRID_W  = 99.6            # matrix kept at its established size (cells 6.23mm × 16)
GUTTER0 = 9.6            # nominal gutter (adjusted after HP rounding)
PW_RAW  = 2 * (CTRL_W + GUTTER0) + GRID_W
HP      = int(math.ceil(PW_RAW / 5.08))
PW_MM   = HP * 5.08
# Absorb the rounding slack into the gutter so the matrix stays 99.6 and columns keep their pitch.
GUTTER  = (PW_MM - 2 * CTRL_W - GRID_W) / 2.0
GRID_X  = CTRL_W + GUTTER
CELL    = GRID_W / 16.0
GRID_Y  = 20.0
GRID_H  = CELL * 16.0

N_VERBS   = 4
N_STREAMS = 3                      # Q5 q-mix: 3rd stream (melody, rhythm, q-mix) -> 12 rows/side
# PLAN A (CA_PANEL_THREE_STREAM_LAYOUT): tighten row pitch to fit 12 rows in 128.5mm.
# Jack well is r=3.9 (Ø7.8); ROW_H=8.0 is the jack-floor pitch (jacks touch at 0.2mm gap).
# GROUP_GAP shrunk 6.8->1.5 (groups barely separate); ROW_TOP 21->14; bottom offset 9->6.
# This is the "try tighter pitch first" attempt; if jacks read too cramped -> Plan B (smaller jack SVG).
ROW_H     = 8.0                   # jack-floor pitch (jacks touch at 0.2mm gap) — kept at the floor.
# GROUP_GAP widened 1.5->3.5: the poly jacks + bottom logo were cut and the logo moved to the top,
# freeing vertical space; spend it on a CLEAR BAND between op-groups so each INTRA/INTER label sits
# clear of the group above (was overlapping). MUST MATCH MonsoonChangeAlleyV2.hpp GROUP_GAP.
GROUP_GAP = 3.5
ROW_TOP   = 11.0                  # first row starts below the top logo/title band. MUST MATCH CTRL_TOP.
BOTTOM_OFFSET = 6.0               # (retained for lastBottom(); bottom cluster itself removed)
LOGO_TOP_Y = 3.0                  # dot.modular wordmark at the TOP (matches Monsoon/West placement)
LOGO_W     = 34.0

def rowY(v, s): return ROW_TOP + v*(N_STREAMS*ROW_H+GROUP_GAP) + s*ROW_H + ROW_H*0.5
def lastBottom(): return rowY(N_VERBS-1,N_STREAMS-1) + ROW_H*0.5
def lx(x, flip): return (PW_MM - x) if flip else x

def pal(dark):
    b=dict(red="#d4001a", gold="#c8960c")
    if dark:
        return dict(b, body="#18181a", ink="#e8e2d0", dim="#8a8578", frame="#2e2e33",
                    jackwell="#0a0b0c", jackring="#46464c", well="#0f1012", wellring="#3a3a40",
                    edrecess="#101113", edborder="#2e2e33", tabband="#181820",
                    gridln="#26262b", booth="#141416")
    return dict(b, body="#e8e8ea", ink="#2a2a2e", dim="#888d96", frame="#a8aeb6",
                jackwell="#dadce0", jackring="#9298a0", well="#dcdee2", wellring="#a8aeb6",
                edrecess="#d8dade", edborder="#c0c4ca", tabband="#cdd4dc",
                gridln="#c4c8ce", booth="#d0d4da")

def gen(dark):
    t=pal(dark); els=[]; E=els.append
    E(f'<rect width="{px(PW_MM):.1f}" height="{px(PH_MM):.1f}" fill="{t["body"]}"/>')
    E(f'<rect x="{px(GRID_X):.1f}" y="{px(GRID_Y):.1f}" width="{px(GRID_W):.1f}" height="{px(GRID_H):.1f}" fill="{t["well"]}" stroke="{t["edborder"]}" stroke-width="{px(0.4):.2f}"/>')
    for i in range(1,16):
        gx=GRID_X+i*CELL; gy=GRID_Y+i*CELL
        E(f'<line x1="{px(gx):.1f}" y1="{px(GRID_Y):.1f}" x2="{px(gx):.1f}" y2="{px(GRID_Y+GRID_H):.1f}" stroke="{t["gridln"]}" stroke-width="{px(0.2):.2f}"/>')
        E(f'<line x1="{px(GRID_X):.1f}" y1="{px(gy):.1f}" x2="{px(GRID_X+GRID_W):.1f}" y2="{px(gy):.1f}" stroke="{t["gridln"]}" stroke-width="{px(0.2):.2f}"/>')

    def btn(cx, ry):
        E(f'<circle cx="{px(cx):.1f}" cy="{px(ry):.1f}" r="{px(2.6):.1f}" fill="{t["frame"]}" stroke="{t["dim"]}" stroke-width="{px(0.5):.2f}"/>')

    for verb in range(N_VERBS):
        for sub in range(N_STREAMS):
            ry=rowY(verb,sub)
            for side in range(2):
                flip=(side==1)
                E(jack(lx(J_DOM,flip),ry,t)); E(jack(lx(J_COD,flip),ry,t))
                E(trim(lx(KNOB1,flip),ry,t,t["gold"]))
                if verb in (0,1): E(trim(lx(KNOB2,flip),ry,t,t["gold"]))
                elif verb==3:
                    # SCATTER: dom/cod back jacks, then ON-ROW Philox reverse buttons (no longer
                    # jammed above/below), plus the true-reverse button + jack (CA_DICE_COUNTER_MODEL).
                    E(jack(lx(KNOB2,flip),ry,t)); E(jack(lx(J_BACK2,flip),ry,t))
                    btn(lx(REV_D,flip),ry); btn(lx(REV_C,flip),ry)
                    btn(lx(TRUE_REV_BTN,flip),ry)
                    E(jack(lx(TRUE_REV_IN,flip),ry,t))
                # forward dom/cod fire buttons (all verbs)
                btn(lx(BTN_D,flip),ry); btn(lx(BTN_C,flip),ry)
                E(f'<circle cx="{px(lx(LIGHT,flip)):.1f}" cy="{px(ry):.1f}" r="{px(1.3):.1f}" fill="{t["well"]}" stroke="{t["dim"]}" stroke-width="{px(0.3):.2f}"/>')

    # dot.modular wordmark at the TOP (matches Monsoon/West); centred horizontally.
    # (The old bottom cluster — logo + 2 poly jacks — is REMOVED: the poly-mod inputs were
    #  cut per CA_PANEL_THREE_STREAM_LAYOUT, freeing the bottom for the matrix legend.)
    E(logo_embed(dark, (PW_MM - LOGO_W) / 2.0, LOGO_TOP_Y, LOGO_W))

    out=os.path.join(os.path.dirname(__file__),"..","res","panels")
    os.makedirs(out,exist_ok=True)
    th="dark" if dark else "light"
    open(os.path.join(out,f"ChangeAlleyV2_panel_{th}.svg"),"w").write(
        svg_open(px(PW_MM),px(PH_MM))+"\n"+"\n".join(els)+"\n</svg>\n")
    print(f"ChangeAlleyV2 {th}: {HP}HP grid {GRID_W:.1f}mm cell {CELL:.2f}mm ctrl {CTRL_W:.1f}mm/side")

if __name__=="__main__":
    gen(True); gen(False)
