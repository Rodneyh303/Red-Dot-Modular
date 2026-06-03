"""Clean native-75-DPI panels for Macro (26HP) and Mono (34HP).
Artwork-only: jack wells, atten wells, editor recess. No <text> (nanosvg
ignores it). Positions match the widget mm2px coordinates exactly."""
import math
S = 75.0/25.4
def px(mm): return round(mm*S, 2)
H_MM = 128.5
PH = px(H_MM)

def theme(dark):
    if dark: return dict(bg="#1a1d22", teal="#26a69a", accent="#dc2626",
        jackwell="#0e1013", jackring="#4a5058", well="#15181c", wellring="#3a4048",
        edrecess="#101216", edborder="#2a3038")
    return dict(bg="#e7e8ea", teal="#1a8276", accent="#c0202a",
        jackwell="#dadce0", jackring="#9298a0", well="#d6d8dc", wellring="#a8aeb6",
        edrecess="#dde0e4", edborder="#c0c4ca")

def jack(A,t,x,y):
    A(f'<circle cx="{px(x):.1f}" cy="{px(y):.1f}" r="{px(3.9):.1f}" fill="{t["jackwell"]}" stroke="{t["jackring"]}" stroke-width="1"/>')
    A(f'<circle cx="{px(x):.1f}" cy="{px(y):.1f}" r="{px(1.7):.1f}" fill="{t["edrecess"]}"/>')
def trim(A,t,x,y,col):
    A(f'<circle cx="{px(x):.1f}" cy="{px(y):.1f}" r="{px(3.2):.1f}" fill="{t["well"]}" stroke="{col}" stroke-width="1"/>')

def header(W_MM):
    PW=px(W_MM)
    return (f'<svg xmlns="http://www.w3.org/2000/svg" xmlns:inkscape="http://www.inkscape.org/namespaces/inkscape" '
            f'width="{PW}" height="{PH}" viewBox="0 0 {PW} {PH}">'), PW

# ── MACRO 26HP ──────────────────────────────────────────────────────────
def gen_macro(dark):
    t=theme(dark); L=[]; A=L.append
    W_MM=132.08; ROW_TOP,ROW_BOT,N=14.,108.,6
    def rowY(r): return ROW_TOP+(r+0.5)*(ROW_BOT-ROW_TOP)/N
    COL_J1,COL_J2,COL_A1,COL_A2=6.,14.,23.,32.
    ED_X=39.; ED_W=W_MM-ED_X-4.; ED_Y=16.; ED_H=48.
    hdr,PW=header(W_MM); A(hdr)
    A(f'<g inkscape:label="artwork" inkscape:groupmode="layer">')
    A(f'<rect width="{PW}" height="{PH}" fill="{t["bg"]}"/>')
    A(f'<line x1="0" y1="{px(10):.1f}" x2="{PW}" y2="{px(10):.1f}" stroke="{t["accent"]}" stroke-width="1" opacity="0.4"/>')
    A(f'<rect x="{px(ED_X):.1f}" y="{px(ED_Y):.1f}" width="{px(ED_W):.1f}" height="{px(ED_H):.1f}" rx="{px(1.5):.1f}" fill="{t["edrecess"]}" stroke="{t["edborder"]}" stroke-width="1"/>')
    A('</g>')
    A(f'<g inkscape:label="control-graphics" inkscape:groupmode="layer">')
    for r in range(6):
        y=rowY(r)
        jack(A,t,COL_J1,y); jack(A,t,COL_J2,y)
        trim(A,t,COL_A1,y,t["wellring"]); trim(A,t,COL_A2,y,t["wellring"])
    A('</g></svg>')
    return "\n".join(L)

# ── MONO 40HP ───────────────────────────────────────────────────────────
def gen_mono(dark):
    t=theme(dark); L=[]; A=L.append
    W_MM=203.2; ROW_TOP,ROW_BOT,N=14.,108.,6
    def laneY(l): return ROW_TOP+(l+0.5)*(ROW_BOT-ROW_TOP)/N
    JACK_X=[6.,15.,24.]; ATTEN_X=[34.,43.,52.]
    SPR_BASE_X,SPR_CV_X,SPR_ATTEN_X=62.,71.,80.
    ED_X=88.; ED_W=W_MM-ED_X-4.; ED_Y=16.
    hdr,PW=header(W_MM); A(hdr)
    A(f'<g inkscape:label="artwork" inkscape:groupmode="layer">')
    A(f'<rect width="{PW}" height="{PH}" fill="{t["bg"]}"/>')
    A(f'<line x1="0" y1="{px(10):.1f}" x2="{PW}" y2="{px(10):.1f}" stroke="{t["accent"]}" stroke-width="1" opacity="0.4"/>')
    # subtle spread-column band behind the 3 spread rows (REST/MEL/OCT)
    A(f'<rect x="{px(SPR_BASE_X-5):.1f}" y="{px(laneY(0)-7):.1f}" width="{px(23):.1f}" height="{px(laneY(2)-laneY(0)+14):.1f}" rx="{px(1.5):.1f}" fill="{t["teal"]}" opacity="0.06"/>')
    A(f'<rect x="{px(ED_X):.1f}" y="{px(ED_Y):.1f}" width="{px(ED_W):.1f}" height="{px(ROW_BOT-ED_Y):.1f}" rx="{px(1.5):.1f}" fill="{t["edrecess"]}" stroke="{t["edborder"]}" stroke-width="1"/>')
    A('</g>')
    A(f'<g inkscape:label="control-graphics" inkscape:groupmode="layer">')
    # LOR: 3 jacks + 3 attens, all 6 lanes
    for lane in range(6):
        y=laneY(lane)
        for p in range(3):
            jack(A,t,JACK_X[p],y)
            trim(A,t,ATTEN_X[p],y,t["wellring"])
    # Spread group: base trim + CV jack + atten, REST/MEL/OCT only
    for l in range(3):
        y=laneY(l)
        trim(A,t,SPR_BASE_X,y,t["teal"])       # base spread trimpot
        jack(A,t,SPR_CV_X,y)                    # spread CV
        trim(A,t,SPR_ATTEN_X,y,t["teal"])       # spread atten
    A('</g></svg>')
    return "\n".join(L)

for fn,base in [(gen_macro,"StraitsSandsMacroVisual_26HP"),(gen_mono,"SandsMonoVisual_40HP")]:
    for dark,suf in [(True,""),(False,"_light")]:
        svg=fn(dark)
        name=f"{base}{suf}.svg"
        with open(f"res/panels/{name}","w") as f: f.write(svg)
        print(f"{name}: {len(svg):,} bytes")
