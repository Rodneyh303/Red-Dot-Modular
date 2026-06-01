"""Clean Monsoon panel + tall Supertree backdrop (Gardens by the Bay).
Authored from widget component positions. Controls legible; trees behind."""
import math, random
S = 75.0 / 25.4  # Rack mm2px: 75 DPI, NOT 96 (was 3.7795 — caused 1.28x panel drift)
def px(mm): return round(mm*S, 2)
W_MM, H_MM = 203.2, 128.5
PW, PH = px(W_MM), px(H_MM)

KNOBS_LG=[(16,22)]; KNOBS_MD=[(42,22),(68,22),(94,22),(120,22)]
KNOB_LABELS=["NOTE VALUE","VARIATION","LEGATO","REST","ACCENT"]
KNOBS_SM=[(148,58),(163,58),(178,58)]; SM_LABELS=["BPM","LEN","OFFSET"]
RING=(162,30,14)
SEMI=[(7.5+i*9.0,59.75) for i in range(12)]
SEMI_LABELS=["C","C#","D","D#","E","F","F#","G","G#","A","A#","B"]
OCT=[(119,59.75),(128,59.75)]; SL_TOP,SLH=45.0,29.5
MODE=(197,12); MODE_LIGHTS=[(192,34+i*8) for i in range(4)]; MODE_LABELS=["A","B","C","D"]
ACTIONS=[(118+i*15,87) for i in range(6)]
ACTION_LABELS=["DICE R","DICE M","LOCK","MUTE","RESET","RUN"]
IN1=[(16,105),(30,105),(48,105),(66,105),(84,105)]; IN1_L=["RUN","RST","SEED","LEN","OFF"]
IN2=[(16,120),(30,120),(48,120),(66,120),(84,120)]; IN2_L=["CLK","G1","G2","CV1","CV2"]
OUT1=[(104,105),(122,105),(140,105),(158,105),(176,105)]; OUT1_L=["GATE","TIE","LEG","T|L","ACC"]
OUT2=[(104,120),(122,120),(140,120),(158,120)]; OUT2_L=["CV","SEED","RUN","RST"]
EXP=[(4,4),(9,4),(14,4)]

def theme(dark):
    if dark: return dict(bg="#16181c",ink="#d8d8dc",dim="#8a8f98",knobwell="#0f1114",
        knobring="#3a4048",accent="#dc2626",teal="#26a69a",gold="#c8960c",
        jackwell="#0b0d10",jackring="#4a5058",slot="#101216",slotline="#2a2f37",
        tree="#2a3038", treecanopy="#323a44")
    return dict(bg="#e8e8ea",ink="#2a2a2e",dim="#888d96",knobwell="#d4d6d9",
        knobring="#a8aeb6",accent="#c0202a",teal="#1a8276",gold="#a07808",
        jackwell="#dadce0",jackring="#9298a0",slot="#d8dade",slotline="#c0c4ca",
        tree="#c4c8ce", treecanopy="#b4bac2")

def supertrees(t):
    """Tall Gardens-by-the-Bay backdrop trees, low opacity, behind controls."""
    random.seed(11)
    trees=[(18,72,13),(55,60,16),(95,78,11),(130,64,14),(165,70,12),(192,58,15)]
    out=[f'<g opacity="0.16">']
    for cx,cyc,rx in trees:
        hb,ht=2.4,1.0
        # trunk
        out.append(f'<polygon points="{px(cx-hb):.1f},{px(128.5):.1f} {px(cx+hb):.1f},{px(128.5):.1f} '
                   f'{px(cx+ht):.1f},{px(cyc):.1f} {px(cx-ht):.1f},{px(cyc):.1f}" fill="{t["tree"]}"/>')
        # canopy
        ry=rx*0.30
        out.append(f'<ellipse cx="{px(cx):.1f}" cy="{px(cyc):.1f}" rx="{px(rx):.1f}" ry="{px(ry):.1f}" fill="{t["treecanopy"]}"/>')
        # fronds
        for k in range(9):
            ang=(k/8-0.5)*math.pi*0.9
            fx=cx+rx*1.1*math.sin(ang); fy=cyc-rx*0.55*math.cos(ang)
            out.append(f'<line x1="{px(cx):.1f}" y1="{px(cyc):.1f}" x2="{px(fx):.1f}" y2="{px(fy):.1f}" '
                       f'stroke="{t["treecanopy"]}" stroke-width="0.7"/>')
    out.append('</g>')
    return "\n".join(out)

def gen(dark):
    t=theme(dark); L=[]; A=L.append
    A(f'<svg xmlns="http://www.w3.org/2000/svg" xmlns:inkscape="http://www.inkscape.org/namespaces/inkscape" width="{PW}" height="{PH}" viewBox="0 0 {PW} {PH}">')
    A(f'<g inkscape:label="artwork" inkscape:groupmode="layer">')
    A(f'<rect width="{PW}" height="{PH}" fill="{t["bg"]}"/>')
    # Supertree backdrop (behind everything)
    A(supertrees(t))
    # header accent
    A(f'<line x1="0" y1="{px(11):.1f}" x2="{PW}" y2="{px(11):.1f}" stroke="{t["accent"]}" stroke-width="1" opacity="0.4"/>')
    for yy in [40,78,99]:
        A(f'<line x1="{px(4):.1f}" y1="{px(yy):.1f}" x2="{PW-px(4):.1f}" y2="{px(yy):.1f}" stroke="{t["slotline"]}" stroke-width="0.5" opacity="0.4"/>')
    A('</g>')

    A(f'<g inkscape:label="control-graphics" inkscape:groupmode="layer">')
    def well(x,y,r): A(f'<circle cx="{px(x):.1f}" cy="{px(y):.1f}" r="{px(r):.1f}" fill="{t["knobwell"]}" stroke="{t["knobring"]}" stroke-width="1.2"/>')
    for (x,y) in KNOBS_LG: well(x,y,13.5)
    for (x,y) in KNOBS_MD: well(x,y,11.0)
    for (x,y) in KNOBS_SM: well(x,y,8.0)
    rcx,rcy,rr=RING  # rr=14 = LED radius
    # Guide ring just OUTSIDE the LEDs (r+1.5), and a faint hub well inside
    A(f'<circle cx="{px(rcx):.1f}" cy="{px(rcy):.1f}" r="{px(rr+1.5):.1f}" fill="none" stroke="{t["gold"]}" stroke-width="0.8" opacity="0.6"/>')
    A(f'<circle cx="{px(rcx):.1f}" cy="{px(rcy):.1f}" r="{px(rr-2.5):.1f}" fill="{t["knobwell"]}" opacity="0.4"/>')
    for (x,_) in SEMI:
        A(f'<rect x="{px(x-1.6):.1f}" y="{px(SL_TOP):.1f}" width="{px(3.2):.1f}" height="{px(SLH):.1f}" rx="{px(1.4):.1f}" fill="{t["slot"]}" stroke="{t["slotline"]}" stroke-width="0.5"/>')
    for (x,_),lbl in zip(OCT,["LO","HI"]):
        A(f'<rect x="{px(x-1.6):.1f}" y="{px(SL_TOP):.1f}" width="{px(3.2):.1f}" height="{px(SLH):.1f}" rx="{px(1.4):.1f}" fill="{t["slot"]}" stroke="{t["accent"]}" stroke-width="0.6" opacity="0.8"/>')
    A(f'<circle cx="{px(MODE[0]):.1f}" cy="{px(MODE[1]):.1f}" r="{px(2.4):.1f}" fill="{t["knobwell"]}" stroke="{t["knobring"]}" stroke-width="1"/>')
    for (x,y),lbl in zip(ACTIONS,ACTION_LABELS):
        A(f'<circle cx="{px(x):.1f}" cy="{px(y):.1f}" r="{px(2.2):.1f}" fill="{t["knobwell"]}" stroke="{t["knobring"]}" stroke-width="0.9"/>')
    def jack(x,y,lbl,above):
        A(f'<circle cx="{px(x):.1f}" cy="{px(y):.1f}" r="{px(3.9):.1f}" fill="{t["jackwell"]}" stroke="{t["jackring"]}" stroke-width="1"/>')
        A(f'<circle cx="{px(x):.1f}" cy="{px(y):.1f}" r="{px(1.7):.1f}" fill="{t["slot"]}"/>')
    for (x,y),l in zip(IN1,IN1_L): jack(x,y,l,True)
    for (x,y),l in zip(IN2,IN2_L): jack(x,y,l,False)
    for (x,y),l in zip(OUT1,OUT1_L): jack(x,y,l,True)
    for (x,y),l in zip(OUT2,OUT2_L): jack(x,y,l,False)
    for (x,y) in EXP:
        A(f'<circle cx="{px(x):.1f}" cy="{px(y):.1f}" r="{px(0.9):.1f}" fill="{t["teal"]}" opacity="0.5"/>')
    A('</g>')
    A('</svg>')
    return "\n".join(L)

for dark,name in [(True,"Monsoon_panel_dark_monsoon.svg"),(False,"Monsoon_panel_light_monsoon.svg")]:
    svg=gen(dark)
    with open(f"res/panels/{name}","w") as f: f.write(svg)
    print(f"{name}: {len(svg):,} bytes")
