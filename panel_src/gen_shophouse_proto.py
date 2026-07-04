#!/usr/bin/env python3
# gen_shophouse_proto.py — PROTOTYPE (not a real panel yet)
# One Shophouse "front" whose 12 shutters are arranged like a piano octave (7 white + 5 black),
# lit = in-scale, dim = closed/out-of-scale, root shutter accented in Singapore red. Renders a
# single front and a row of 4 (the fixed-4 scale list) so we can look at legibility before
# committing. NanoSVG-safe (rects/lines/circles/polys + opacity; no mask/gradient/filter).
import sys, os
sys.path.insert(0, os.path.dirname(__file__))
from dotmod_design import px, theme, svg_open

# Semitone → is it a "black key"? (C C# D D# E F F# G G# A A# B) = 0..11
BLACK = {1, 3, 6, 8, 10}
# White-key ordinal positions (0..6) for C D E F G A B, and which white key each black sits after.
WHITE_ORDER = [0, 2, 4, 5, 7, 9, 11]                 # semitone of each white key, left→right
BLACK_AFTER = {1: 0, 3: 1, 6: 3, 8: 4, 10: 5}         # black semitone → white-ordinal it follows

def scale_mask(root, intervals):
    m = 0
    for iv in intervals:
        m |= 1 << ((root + iv) % 12)
    return m

# One shophouse front. (x,y) top-left in mm; w,h the front's footprint.
def shophouse_front(x, y, w, h, t, root, mask, label):
    o = []
    R = lambda a,b,c,d,**k: rect(a,b,c,d,**k)
    # ── facade body ──
    o.append(f'<rect x="{px(x)}" y="{px(y)}" width="{px(w)}" height="{px(h)}" '
             f'rx="{px(0.8)}" fill="{t["group"]}" stroke="{t["groupline"]}" stroke-width="1.2"/>')
    # ── pitched roof (Peranakan gable hint) ──
    ridge = y - 3.0
    o.append(f'<polygon points="{px(x-1)},{px(y+0.5)} {px(x+w/2)},{px(ridge)} {px(x+w+1)},{px(y+0.5)}" '
             f'fill="{t["accent"]}" opacity="0.85"/>')
    o.append(f'<polygon points="{px(x-1)},{px(y+0.5)} {px(x+w/2)},{px(ridge)} {px(x+w+1)},{px(y+0.5)}" '
             f'fill="none" stroke="{t["groupline"]}" stroke-width="0.8"/>')

    # ── the 12 shutters, piano-keyboard arranged ──
    # upper-floor window band
    pad = 1.6
    bx, by = x + pad, y + 2.6
    bw, bh = w - 2*pad, h * 0.46
    o.append(f'<rect x="{px(bx)}" y="{px(by)}" width="{px(bw)}" height="{px(bh)}" '
             f'fill="{t["edrecess"]}" stroke="{t["edborder"]}" stroke-width="0.8"/>')
    nW = 7
    wgap = 0.5
    ww = (bw - (nW-1)*wgap) / nW           # white shutter width
    wh = bh - 1.6                          # white shutter height (full band)
    wy = by + 0.8
    def shutter(sx, sy, sw, sh, semi, black):
        lit = (mask >> semi) & 1
        is_root = (semi == root)
        if is_root and lit:
            fill, strk = t["accent"], t["ink"]          # root = Singapore red
        elif lit:
            fill, strk = t["teal"], t["ink"]            # in-scale = teal (open shutter)
        else:
            fill, strk = (t["well"] if not black else t["jackwell"]), t["wellring"]  # closed
        op = 1.0 if lit else 0.5
        louvers = ""
        # louvered shutter texture: two subtle slats (kept minimal for clarity)
        for k in range(2):
            ly = sy + sh*(0.36 + 0.28*k)
            louvers += (f'<line x1="{px(sx+0.6)}" y1="{px(ly)}" x2="{px(sx+sw-0.6)}" y2="{px(ly)}" '
                        f'stroke="{t["ink"]}" stroke-width="0.35" opacity="{0.3*op:.2f}"/>')
        return (f'<rect x="{px(sx)}" y="{px(sy)}" width="{px(sw)}" height="{px(sh)}" rx="{px(0.3)}" '
                f'fill="{fill}" stroke="{strk}" stroke-width="0.6" opacity="{op:.2f}"/>' + louvers)
    # white shutters (full height, lower layer)
    wx_of = {}
    for i, semi in enumerate(WHITE_ORDER):
        sx = bx + 0.8 + i*(ww+wgap)
        wx_of[i] = sx
        o.append(shutter(sx, wy, ww, wh, semi, False))
    # black shutters — narrower, in the UPPER ~55% only, centred on the gap between whites
    bwd = ww*0.58
    bhh = wh*0.55
    for semi, after in BLACK_AFTER.items():
        # centre on the boundary between white 'after' and the next white
        gap_centre = wx_of[after] + ww + wgap*0.5
        sx = gap_centre - bwd*0.5
        o.append(shutter(sx, wy, bwd, bhh, semi, True))

    # ── five-foot-way arch (ground floor colonnade) ──
    ay = y + h - h*0.30
    aw = w - 2*pad
    o.append(f'<path d="M {px(bx)} {px(y+h-1)} '
             f'L {px(bx)} {px(ay+2)} '
             f'Q {px(bx+aw/2)} {px(ay-2.5)} {px(bx+aw)} {px(ay+2)} '
             f'L {px(bx+aw)} {px(y+h-1)} Z" '
             f'fill="{t["edrecess"]}" stroke="{t["gold"]}" stroke-width="1.0" opacity="0.9"/>')
    # slot label (which list slot)
    o.append(f'<text x="{px(x+w/2)}" y="{px(y+h-1.2)}" text-anchor="middle" '
             f'font-family="sans-serif" font-size="{px(2.6)}" fill="{t["dim"]}">{label}</text>')
    return "".join(o)

def rect(x,y,w,h,**k): pass

def render(dark, fname):
    t = theme(dark)
    # one front ~ 16mm wide; a row of 4 across a 20HP-ish portrait width
    PW, PH = px(78), px(64)
    out = [svg_open(PW, PH)]
    out.append(f'<rect width="{PW}" height="{PH}" fill="{t["bg"]}"/>')
    out.append(f'<text x="{px(4)}" y="{px(6)}" font-family="sans-serif" font-size="{px(3)}" '
               f'fill="{t["ink"]}">shophouse — scale-list fronts (keyboard shutters)</text>')
    # four fronts, four scales (C maj, A min=A aeolian, E dorian, G mixolydian) for variety
    MAJ = [0,2,4,5,7,9,11]; DOR=[0,2,3,5,7,9,10]; MIX=[0,2,4,5,7,9,10]; AEO=[0,2,3,5,7,8,10]
    slots = [(0,MAJ,"C maj / slot 1"),(9,AEO,"A min / 2"),(2,DOR,"D dor / 3"),(7,MIX,"G mix / 4")]
    fx = 3.0; fy = 12.0; fw = 17.0; fh = 44.0; gap = 1.8
    for i,(root,iv,lab) in enumerate(slots):
        out.append(shophouse_front(fx + i*(fw+gap), fy, fw, fh, t, root, scale_mask(root,iv), lab))
    out.append("</svg>")
    open(fname,"w").write("".join(out))
    print("wrote", fname)

if __name__ == "__main__":
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    render(True,  os.path.join(root, "panel_src/proto_shophouse_dark.svg"))
    render(False, os.path.join(root, "panel_src/proto_shophouse_light.svg"))
