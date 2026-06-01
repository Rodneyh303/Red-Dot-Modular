"""Clean Monsoon panel generator — authored from widget component positions.
Each visual element sized to its actual Rack widget so components sit correctly.
Subtle Singapore motif (skyline + faint rain) that does NOT cover controls."""
import math

S = 3.7795
def px(mm): return round(mm*S, 2)
W_MM, H_MM = 203.2, 128.5
PW, PH = px(W_MM), px(H_MM)

# ── Verified component positions (mm), matching MonsoonWidget.cpp ──
KNOBS_LG = [(16,22)]                                   # NOTE_VALUE — Large (r 13.5mm)
KNOBS_MD = [(42,22),(68,22),(94,22),(120,22)]          # VAR/LEG/REST/ACC — Medium (r 11mm)
KNOB_LABELS = ["NOTE VALUE","VARIATION","LEGATO","REST","ACCENT"]
KNOBS_SM = [(148,58),(163,58),(178,58)]                # BPM/LEN/OFF — Small (r 8mm)
SM_LABELS = ["BPM","LEN","OFFSET"]
RING = (162,30,14)                                     # step ring cx,cy,r
SEMI = [(7.5+i*9.0, 59.75) for i in range(12)]
SEMI_LABELS = ["C","C#","D","D#","E","F","F#","G","G#","A","A#","B"]
OCT = [(119,59.75),(128,59.75)]                        # LO/HI
SL_TOP, SLH = 45.0, 29.5                               # slider travel (from widget)
MODE = (197,12)
MODE_LIGHTS = [(192,34+i*8) for i in range(4)]
MODE_LABELS = ["A","B","C","D"]
ACTIONS = [(118+i*15,87) for i in range(6)]
ACTION_LABELS = ["DICE R","DICE M","LOCK","MUTE","RESET","RUN"]
IN1 = [(16,105),(30,105),(48,105),(66,105),(84,105)]
IN1_L = ["RUN","RST","SEED","LEN","OFF"]
IN2 = [(16,120),(30,120),(48,120),(66,120),(84,120)]
IN2_L = ["CLK","G1","G2","CV1","CV2"]
OUT1 = [(104,105),(122,105),(140,105),(158,105),(176,105)]
OUT1_L = ["GATE","TIE","LEG","T|L","ACC"]
OUT2 = [(104,120),(122,120),(140,120),(158,120)]
OUT2_L = ["CV","SEED","RUN","RST"]
EXP = [(4,4),(9,4),(14,4)]

def theme(dark):
    if dark: return dict(
        bg="#1a1c20", surface="#212429", ink="#d8d8dc", dim="#8a8f98",
        knobwell="#15171a", knobring="#3a4048", accent="#dc2626",
        teal="#26a69a", gold="#c8960c", jackwell="#0e1013", jackring="#4a5058",
        slot="#141619", slotline="#2a2f37", motif="#232831")
    return dict(
        bg="#e8e8ea", surface="#f2f2f3", ink="#2a2a2e", dim="#888d96",
        knobwell="#d4d6d9", knobring="#a8aeb6", accent="#c0202a",
        teal="#1a8276", gold="#a07808", jackwell="#dadce0", jackring="#9298a0",
        slot="#d8dade", slotline="#c0c4ca", motif="#d0d4da")

def gen(dark):
    t = theme(dark); L=[]; A=L.append
    A(f'<svg xmlns="http://www.w3.org/2000/svg" xmlns:inkscape="http://www.inkscape.org/namespaces/inkscape" width="{PW}" height="{PH}" viewBox="0 0 {PW} {PH}">')

    # ── Background ──
    A(f'<g inkscape:label="artwork" inkscape:groupmode="layer">')
    A(f'<rect width="{PW}" height="{PH}" fill="{t["bg"]}"/>')

    # Subtle skyline motif along the very bottom (doesn't reach jacks at y=120, baseline 128.5)
    # Keep it BELOW y=125mm so it never touches controls
    A(f'<g fill="{t["motif"]}" opacity="0.5">')
    sky_y = px(128.5)
    import random; random.seed(7)
    bx = 0
    while bx < PW:
        bw = px(random.uniform(6,14)); bh = px(random.uniform(2,5))
        A(f'<rect x="{bx:.0f}" y="{sky_y-bh:.0f}" width="{bw:.0f}" height="{bh:.0f}"/>')
        bx += bw + px(random.uniform(1,3))
    A('</g>')

    # Faint header underline accent
    A(f'<line x1="0" y1="{px(11):.1f}" x2="{PW}" y2="{px(11):.1f}" stroke="{t["accent"]}" stroke-width="1" opacity="0.4"/>')

    # Section divider lines (subtle)
    for yy in [40, 78, 99]:
        A(f'<line x1="{px(4):.1f}" y1="{px(yy):.1f}" x2="{PW-px(4):.1f}" y2="{px(yy):.1f}" stroke="{t["slotline"]}" stroke-width="0.5" opacity="0.5"/>')

    A('</g>')  # end artwork layer

    # ── Controls layer (wells/rings sized to widgets) ──
    A(f'<g inkscape:label="control-graphics" inkscape:groupmode="layer">')

    def knob_well(x,y,r_mm):
        A(f'<circle cx="{px(x):.1f}" cy="{px(y):.1f}" r="{px(r_mm):.1f}" fill="{t["knobwell"]}" stroke="{t["knobring"]}" stroke-width="1.2"/>')

    # Large + medium DNA knobs — wells match knob render radius (Large 13.5, Medium 11)
    for (x,y) in KNOBS_LG: knob_well(x,y,13.5)
    for (x,y) in KNOBS_MD: knob_well(x,y,11.0)
    # labels under knobs
    allknobs = KNOBS_LG + KNOBS_MD
    for (x,y),lbl in zip(allknobs, KNOB_LABELS):
        A(f'<text x="{px(x):.1f}" y="{px(y+15.5):.1f}" font-family="sans-serif" font-size="{px(2.6):.1f}" '
          f'fill="{t["ink"]}" text-anchor="middle">{lbl}</text>')

    # Small seq knobs (render r 8mm)
    for (x,y) in KNOBS_SM: knob_well(x,y,8.0)
    for (x,y),lbl in zip(KNOBS_SM, SM_LABELS):
        A(f'<text x="{px(x):.1f}" y="{px(y+11.5):.1f}" font-family="sans-serif" font-size="{px(2.4):.1f}" '
          f'fill="{t["ink"]}" text-anchor="middle">{lbl}</text>')

    # Step ring: outer guide circle (the widget draws the LEDs; we draw a faint ring + glow)
    rcx,rcy,rr = RING
    A(f'<circle cx="{px(rcx):.1f}" cy="{px(rcy):.1f}" r="{px(rr+4):.1f}" fill="none" stroke="{t["gold"]}" stroke-width="0.8" opacity="0.55"/>')
    A(f'<circle cx="{px(rcx):.1f}" cy="{px(rcy):.1f}" r="{px(rr-4):.1f}" fill="{t["knobwell"]}" opacity="0.35"/>')

    # Semitone slider tracks (vertical slots; widget slider travels SL_TOP..SL_TOP+SLH)
    for (x,_) in SEMI:
        A(f'<rect x="{px(x-1.6):.1f}" y="{px(SL_TOP):.1f}" width="{px(3.2):.1f}" height="{px(SLH):.1f}" '
          f'rx="{px(1.4):.1f}" fill="{t["slot"]}" stroke="{t["slotline"]}" stroke-width="0.5"/>')
    for (x,_),lbl in zip(SEMI, SEMI_LABELS):
        A(f'<text x="{px(x):.1f}" y="{px(SL_TOP-1.5):.1f}" font-family="sans-serif" font-size="{px(2.0):.1f}" '
          f'fill="{t["dim"]}" text-anchor="middle">{lbl}</text>')
    # OCT slider tracks
    for (x,_),lbl in zip(OCT,["LO","HI"]):
        A(f'<rect x="{px(x-1.6):.1f}" y="{px(SL_TOP):.1f}" width="{px(3.2):.1f}" height="{px(SLH):.1f}" '
          f'rx="{px(1.4):.1f}" fill="{t["slot"]}" stroke="{t["accent"]}" stroke-width="0.6" opacity="0.8"/>')
        A(f'<text x="{px(x):.1f}" y="{px(SL_TOP-1.5):.1f}" font-family="sans-serif" font-size="{px(2.0):.1f}" '
          f'fill="{t["teal"]}" text-anchor="middle">{lbl}</text>')

    # Mode button well + labels
    A(f'<circle cx="{px(MODE[0]):.1f}" cy="{px(MODE[1]):.1f}" r="{px(2.4):.1f}" fill="{t["knobwell"]}" stroke="{t["knobring"]}" stroke-width="1"/>')
    A(f'<text x="{px(MODE[0]):.1f}" y="{px(MODE[1]-4):.1f}" font-family="sans-serif" font-size="{px(2.4):.1f}" fill="{t["ink"]}" text-anchor="middle">MODE</text>')
    for (x,y),lbl in zip(MODE_LIGHTS, MODE_LABELS):
        A(f'<text x="{px(x-4):.1f}" y="{px(y+1):.1f}" font-family="sans-serif" font-size="{px(2.6):.1f}" fill="{t["dim"]}" text-anchor="end">{lbl}</text>')

    # Action buttons (TL1105 ~ 2.4mm) + labels above
    for (x,y),lbl in zip(ACTIONS, ACTION_LABELS):
        A(f'<circle cx="{px(x):.1f}" cy="{px(y):.1f}" r="{px(2.2):.1f}" fill="{t["knobwell"]}" stroke="{t["knobring"]}" stroke-width="0.9"/>')
        A(f'<text x="{px(x):.1f}" y="{px(y-4):.1f}" font-family="sans-serif" font-size="{px(1.9):.1f}" fill="{t["dim"]}" text-anchor="middle">{lbl}</text>')

    # Jack wells (PJ301M ~ 3.9mm radius) + labels
    def jack(x,y,lbl,labove):
        A(f'<circle cx="{px(x):.1f}" cy="{px(y):.1f}" r="{px(3.9):.1f}" fill="{t["jackwell"]}" stroke="{t["jackring"]}" stroke-width="1"/>')
        A(f'<circle cx="{px(x):.1f}" cy="{px(y):.1f}" r="{px(1.7):.1f}" fill="{t["slot"]}"/>')
        ly = y-5.5 if labove else y+6.5
        A(f'<text x="{px(x):.1f}" y="{px(ly):.1f}" font-family="sans-serif" font-size="{px(1.9):.1f}" fill="{t["dim"]}" text-anchor="middle">{lbl}</text>')
    for (x,y),l in zip(IN1, IN1_L): jack(x,y,l,True)
    for (x,y),l in zip(IN2, IN2_L): jack(x,y,l,False)
    for (x,y),l in zip(OUT1, OUT1_L): jack(x,y,l,True)
    for (x,y),l in zip(OUT2, OUT2_L): jack(x,y,l,False)
    # IN/OUT section headers
    A(f'<text x="{px(50):.1f}" y="{px(101.5):.1f}" font-family="sans-serif" font-size="{px(2.2):.1f}" fill="{t["teal"]}" text-anchor="middle" font-weight="bold">INPUTS</text>')
    A(f'<text x="{px(140):.1f}" y="{px(101.5):.1f}" font-family="sans-serif" font-size="{px(2.2):.1f}" fill="{t["accent"]}" text-anchor="middle" font-weight="bold">OUTPUTS</text>')

    # Expander lights (tiny)
    for (x,y) in EXP:
        A(f'<circle cx="{px(x):.1f}" cy="{px(y):.1f}" r="{px(0.9):.1f}" fill="{t["teal"]}" opacity="0.5"/>')

    A('</g>')  # end control-graphics
    A('</svg>')
    return "\n".join(L)

for dark,name in [(True,"Monsoon_panel_dark_monsoon.svg"),(False,"Monsoon_panel_light_monsoon.svg")]:
    svg=gen(dark)
    with open(f"res/panels/{name}","w") as f: f.write(svg)
    print(f"{name}: {len(svg):,} bytes")
