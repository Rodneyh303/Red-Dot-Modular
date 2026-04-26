"""
Generate individual VCV Rack 2 SVG component assets for Ultra Pro v2
Each file is centred on origin (0,0) at the component's hotspot,
exactly as Rack expects for knobs, ports, LEDs and buttons.
Sliders use top-left origin.
"""
import math, os

OUT = "/home/claude/components"
os.makedirs(OUT, exist_ok=True)

# ─── Colour palette ──────────────────────────────────────────────────────────
BG      = "#1e1e1e"
DARK    = "#141414"
MID     = "#2a2a2a"
LIGHT   = "#3a3a3a"
RIM     = "#484848"
HILITE  = "#606060"
TEXT    = "#cccccc"
DIM     = "#777777"
ACCENT  = "#cc2222"
GOLD    = "#c8922a"
G_LED   = "#22dd55"
R_LED   = "#ff3322"
B_LED   = "#3399ff"
Y_LED   = "#ffaa00"
W_LED   = "#eeeedd"

def svg_file(name, w, h, content, centre=True):
    """Write one SVG component. centre=True means viewBox is centred on (0,0)."""
    if centre:
        vb = f"{-w/2:.3f} {-h/2:.3f} {w:.3f} {h:.3f}"
    else:
        vb = f"0 0 {w:.3f} {h:.3f}"

    lines = [
        '<?xml version="1.0" encoding="UTF-8"?>',
        f'<svg xmlns="http://www.w3.org/2000/svg"',
        f'     xmlns:xlink="http://www.w3.org/1999/xlink"',
        f'     width="{w}mm" height="{h}mm"',
        f'     viewBox="{vb}">',
    ]
    lines += content
    lines.append('</svg>')
    path = f"{OUT}/{name}.svg"
    with open(path, 'w') as f:
        f.write('\n'.join(lines))
    print(f"  ✓ {name}.svg  ({w}×{h}mm)")
    return path

# ─── Shared defs block (gradients, filters) ──────────────────────────────────
def defs_knob(id_suffix=""):
    return [
        '<defs>',
        f'  <radialGradient id="kBody{id_suffix}" cx="36%" cy="30%" r="64%">',
        f'    <stop offset="0%"   stop-color="#686868"/>',
        f'    <stop offset="42%"  stop-color="#323232"/>',
        f'    <stop offset="100%" stop-color="#0c0c0c"/>',
        f'  </radialGradient>',
        f'  <radialGradient id="kRim{id_suffix}" cx="50%" cy="50%" r="50%">',
        f'    <stop offset="72%"  stop-color="transparent"/>',
        f'    <stop offset="100%" stop-color="#555" stop-opacity="0.6"/>',
        f'  </radialGradient>',
        f'  <radialGradient id="kInner{id_suffix}" cx="50%" cy="50%" r="50%">',
        f'    <stop offset="0%"   stop-color="#3a3a3a"/>',
        f'    <stop offset="100%" stop-color="#1a1a1a"/>',
        f'  </radialGradient>',
        f'  <filter id="kShadow{id_suffix}" x="-20%" y="-20%" width="140%" height="140%">',
        f'    <feDropShadow dx="0" dy="0.6" stdDeviation="1.0"',
        f'                  flood-color="#000" flood-opacity="0.6"/>',
        f'  </filter>',
        '</defs>',
    ]

def defs_glow(color, id_name="glow"):
    return [
        f'  <filter id="{id_name}" x="-80%" y="-80%" width="260%" height="260%">',
        f'    <feColorMatrix type="matrix"',
        f'      values="0 0 0 0 {int(color[1:3],16)/255:.3f}',
        f'              0 0 0 0 {int(color[3:5],16)/255:.3f}',
        f'              0 0 0 0 {int(color[5:7],16)/255:.3f}',
        f'              0 0 0 18 -7" result="col"/>',
        f'    <feGaussianBlur in="col" stdDeviation="1.5" result="blur"/>',
        f'    <feMerge>',
        f'      <feMergeNode in="blur"/>',
        f'      <feMergeNode in="SourceGraphic"/>',
        f'    </feMerge>',
        f'  </filter>',
    ]

# ═══════════════════════════════════════════════════════════════════════════════
# KNOBS
# ═══════════════════════════════════════════════════════════════════════════════

def make_knob(name, dia, n_ticks=11, has_pointer=True, skirt_ratio=0.82):
    R  = dia / 2          # outer knob radius
    RS = R * skirt_ratio  # inner body radius
    W  = dia + 8          # canvas includes tick arc space

    # Tick arc: 270° sweep from -225° to +45°
    def tick_pts(angle_deg, inner_r, outer_r):
        a = math.radians(angle_deg)
        return (inner_r*math.cos(a), inner_r*math.sin(a),
                outer_r*math.cos(a), outer_r*math.sin(a))

    c = defs_knob(name)

    # Tick marks
    start_a = -225
    sweep   = 270
    for i in range(n_ticks):
        frac = i / (n_ticks - 1)
        a    = start_a + frac * sweep
        is_main = (i == 0 or i == n_ticks//2 or i == n_ticks-1)
        ir = R + 1.5
        orr = R + (3.5 if is_main else 2.4)
        x1,y1,x2,y2 = tick_pts(a, ir, orr)
        sw = "0.55" if is_main else "0.38"
        col = "#5a5a5a" if is_main else "#404040"
        c.append(f'<line x1="{x1:.3f}" y1="{y1:.3f}" x2="{x2:.3f}" y2="{y2:.3f}" '
                 f'stroke="{col}" stroke-width="{sw}" stroke-linecap="round"/>')

    # Shadow
    c.append(f'<circle r="{R:.3f}" fill="#000" opacity="0.4" '
             f'filter="url(#kShadow{name})" transform="translate(0,0.8)"/>')

    # Outer collar ring
    c.append(f'<circle r="{R+0.8:.3f}" fill="none" stroke="{MID}" stroke-width="1.4"/>')
    c.append(f'<circle r="{R+0.2:.3f}" fill="none" stroke="{HILITE}" stroke-width="0.3"/>')

    # Main knob body
    c.append(f'<circle r="{R:.3f}" fill="url(#kBody{name})"/>')

    # Grip ring (subtle inset circle)
    c.append(f'<circle r="{RS*0.68:.3f}" fill="none" stroke="#1a1a1a" stroke-width="0.6" opacity="0.7"/>')

    # Rubber grip texture: small dashes around circumference
    n_grips = max(24, int(R * 6))
    for i in range(n_grips):
        a = math.radians(i * 360 / n_grips)
        gr = RS * 0.96
        x1 = gr * math.cos(a);       y1 = gr * math.sin(a)
        x2 = (gr-0.9)*math.cos(a);   y2 = (gr-0.9)*math.sin(a)
        c.append(f'<line x1="{x1:.3f}" y1="{y1:.3f}" x2="{x2:.3f}" y2="{y2:.3f}" '
                 f'stroke="#1c1c1c" stroke-width="0.5" opacity="0.5"/>')

    # Pointer line (12 o'clock)
    if has_pointer:
        px1 = 0;  py1 = -RS * 0.42
        px2 = 0;  py2 = -RS * 0.92
        c.append(f'<line x1="{px1}" y1="{py1:.3f}" x2="{px2}" y2="{py2:.3f}" '
                 f'stroke="#e8e8e8" stroke-width="{max(0.7, R*0.09):.2f}" stroke-linecap="round"/>')
        # Dot at tip
        c.append(f'<circle cx="0" cy="{-RS*0.92:.3f}" r="{max(0.5,R*0.06):.2f}" fill="white" opacity="0.6"/>')

    # Specular glint (top-left)
    gx = -R*0.22;  gy = -R*0.26
    gr_r = R*0.20; gr_ry = R*0.12
    c.append(f'<ellipse cx="{gx:.3f}" cy="{gy:.3f}" rx="{gr_r:.3f}" ry="{gr_ry:.3f}" '
             f'fill="white" opacity="0.11" transform="rotate(-35,{gx:.3f},{gy:.3f})"/>')

    svg_file(name, W, W, c)

print("Generating knobs...")
make_knob("RDM_KnobLarge",  dia=18.0, n_ticks=11)
make_knob("RDM_KnobMedium", dia=13.0, n_ticks=9)
make_knob("RDM_KnobSmall",  dia=9.0,  n_ticks=7)

# ═══════════════════════════════════════════════════════════════════════════════
# PORTS (JACKS)
# ═══════════════════════════════════════════════════════════════════════════════

def make_port(name, dia=8.5, is_output=False):
    R = dia / 2
    W = dia + 4

    c = [
        '<defs>',
        f'  <radialGradient id="jBody" cx="38%" cy="32%" r="60%">',
        f'    <stop offset="0%"   stop-color="#5a5a5a"/>',
        f'    <stop offset="55%"  stop-color="#282828"/>',
        f'    <stop offset="100%" stop-color="#0a0a0a"/>',
        f'  </radialGradient>',
        f'  <radialGradient id="jHole" cx="50%" cy="50%" r="50%">',
        f'    <stop offset="0%"   stop-color="#080808"/>',
        f'    <stop offset="100%" stop-color="#1c1c1c"/>',
        f'  </radialGradient>',
        f'  <filter id="jShadow">',
        f'    <feDropShadow dx="0" dy="0.5" stdDeviation="0.8"',
        f'                  flood-color="#000" flood-opacity="0.7"/>',
        f'  </filter>',
        '</defs>',
    ]

    rim_col = GOLD if is_output else RIM

    # Outer mounting ring / shadow
    c.append(f'<circle r="{R+1:.3f}" fill="#0e0e0e" filter="url(#jShadow)"/>')

    # Mounting nut (hex facets implied by octagon)
    n_sides = 8
    pts = []
    for i in range(n_sides):
        a = math.radians(i * 360/n_sides + 22.5)
        pts.append(f"{(R+0.9)*math.cos(a):.3f},{(R+0.9)*math.sin(a):.3f}")
    c.append(f'<polygon points="{" ".join(pts)}" fill="#181818" stroke="{rim_col}" stroke-width="0.8"/>')

    # Inner threaded barrel
    c.append(f'<circle r="{R:.3f}" fill="url(#jBody)"/>')

    # Thread rings (3 subtle concentric lines)
    for ri in [R*0.90, R*0.78, R*0.66]:
        c.append(f'<circle r="{ri:.3f}" fill="none" stroke="#111" stroke-width="0.4" opacity="0.6"/>')

    # Centre socket hole
    c.append(f'<circle r="{R*0.38:.3f}" fill="url(#jHole)" stroke="#0a0a0a" stroke-width="0.5"/>')
    c.append(f'<circle r="{R*0.18:.3f}" fill="#050505"/>')

    # Glint
    c.append(f'<circle cx="{-R*0.28:.3f}" cy="{-R*0.30:.3f}" r="{R*0.12:.3f}" '
             f'fill="white" opacity="0.09"/>')

    # Output: gold collar stripe
    if is_output:
        c.append(f'<circle r="{R*0.52:.3f}" fill="none" stroke="{GOLD}" stroke-width="0.6" opacity="0.7"/>')

    svg_file(name, W, W, c)

print("Generating ports...")
make_port("RDM_PortInput",  dia=8.5, is_output=False)
make_port("RDM_PortOutput", dia=8.5, is_output=True)

# ═══════════════════════════════════════════════════════════════════════════════
# SLIDERS
# ═══════════════════════════════════════════════════════════════════════════════

def make_slider_track(name, w=6.0, h=36.0):
    """Track/trough — origin top-left per Rack slider convention."""
    c = [
        '<defs>',
        '  <linearGradient id="trk" x1="0" y1="0" x2="1" y2="0">',
        '    <stop offset="0%"   stop-color="#141414"/>',
        '    <stop offset="40%"  stop-color="#1e1e1e"/>',
        '    <stop offset="60%"  stop-color="#1e1e1e"/>',
        '    <stop offset="100%" stop-color="#141414"/>',
        '  </linearGradient>',
        '  <filter id="trkIn">',
        '    <feDropShadow dx="0" dy="0.3" stdDeviation="0.5"',
        '                  flood-color="#000" flood-opacity="0.8"/>',
        '  </filter>',
        '</defs>',
    ]
    # Outer frame
    c.append(f'<rect width="{w}" height="{h}" rx="2.0" '
             f'fill="#111" stroke="#2a2a2a" stroke-width="0.5"/>')
    # Inner track
    px = 0.8; py = 0.8
    c.append(f'<rect x="{px}" y="{py}" width="{w-2*px:.1f}" height="{h-2*py:.1f}" '
             f'rx="1.4" fill="url(#trk)" filter="url(#trkIn)"/>')
    # Centre groove line
    cx = w/2
    c.append(f'<rect x="{cx-0.5:.2f}" y="2" width="1" height="{h-4:.1f}" '
             f'rx="0.5" fill="#0c0c0c" stroke="#252525" stroke-width="0.3"/>')
    # Scale marks (5 evenly spaced)
    for i in range(5):
        my = 3 + i * (h-6)/4
        c.append(f'<line x1="1.5" y1="{my:.2f}" x2="2.5" y2="{my:.2f}" '
                 f'stroke="#333" stroke-width="0.35"/>')
        c.append(f'<line x1="{w-2.5:.2f}" y1="{my:.2f}" x2="{w-1.5:.2f}" y2="{my:.2f}" '
                 f'stroke="#333" stroke-width="0.35"/>')

    svg_file(name, w, h, c, centre=False)

def make_slider_handle(name, w=10.0, h=8.0):
    """Slider thumb — centred on hotspot (0,0)."""
    c = [
        '<defs>',
        '  <linearGradient id="hnd" x1="0" y1="0" x2="0" y2="1">',
        '    <stop offset="0%"   stop-color="#525252"/>',
        '    <stop offset="50%"  stop-color="#3a3a3a"/>',
        '    <stop offset="100%" stop-color="#252525"/>',
        '  </linearGradient>',
        '  <filter id="hShadow">',
        '    <feDropShadow dx="0" dy="0.7" stdDeviation="0.9"',
        '                  flood-color="#000" flood-opacity="0.7"/>',
        '  </filter>',
        '</defs>',
    ]
    x, y = -w/2, -h/2

    # Shadow
    c.append(f'<rect x="{x:.2f}" y="{y+0.6:.2f}" width="{w}" height="{h}" '
             f'rx="2" fill="#000" opacity="0.5" filter="url(#hShadow)"/>')

    # Handle body
    c.append(f'<rect x="{x:.2f}" y="{y:.2f}" width="{w}" height="{h}" '
             f'rx="2" fill="url(#hnd)" stroke="{RIM}" stroke-width="0.55"/>')

    # Top edge highlight
    c.append(f'<rect x="{x+0.5:.2f}" y="{y+0.4:.2f}" width="{w-1}" height="1.2" '
             f'rx="1" fill="white" opacity="0.07"/>')

    # Grip lines (3)
    for i in range(3):
        gy = -1.2 + i * 1.2
        c.append(f'<line x1="{x+1.8:.2f}" y1="{gy:.2f}" x2="{-x-1.8:.2f}" y2="{gy:.2f}" '
                 f'stroke="#232323" stroke-width="0.5" stroke-linecap="round"/>')
        c.append(f'<line x1="{x+1.8:.2f}" y1="{gy+0.3:.2f}" x2="{-x-1.8:.2f}" y2="{gy+0.3:.2f}" '
                 f'stroke="#555" stroke-width="0.2" stroke-linecap="round" opacity="0.5"/>')

    # Side notches
    for side in [-1, 1]:
        nx = side * (w/2 - 0.3)
        c.append(f'<rect x="{nx-0.6:.2f}" y="-1.4" width="0.6" height="2.8" '
                 f'rx="0.3" fill="#1a1a1a" stroke="#2e2e2e" stroke-width="0.2"/>')

    svg_file(name, w, h, c, centre=True)

print("Generating sliders...")
make_slider_track("RDM_SliderTrack",  w=6.0, h=36.0)
make_slider_handle("RDM_SliderHandle", w=10.0, h=8.0)

# ═══════════════════════════════════════════════════════════════════════════════
# LEDs
# ═══════════════════════════════════════════════════════════════════════════════

def make_led(name, dia, color, glow_color=None, shape="circle"):
    """LED indicator — centred on (0,0). Rack overlays colour at runtime;
       SVG shows the 'on' state preview with correct shape and glow."""
    if glow_color is None:
        glow_color = color
    R = dia / 2
    W = dia + 6   # canvas larger to contain glow

    # Parse hex colour to RGB 0-1
    def h2f(hx): return int(hx,16)/255.0
    r = h2f(glow_color[1:3]); g = h2f(glow_color[3:5]); b = h2f(glow_color[5:7])

    c = [
        '<defs>',
        f'  <radialGradient id="ledBody" cx="38%" cy="32%" r="62%">',
        f'    <stop offset="0%"   stop-color="white" stop-opacity="0.6"/>',
        f'    <stop offset="40%"  stop-color="{color}" stop-opacity="0.9"/>',
        f'    <stop offset="100%" stop-color="{color}" stop-opacity="0.7"/>',
        f'  </radialGradient>',
        f'  <radialGradient id="ledGlow" cx="50%" cy="50%" r="50%">',
        f'    <stop offset="0%"   stop-color="{glow_color}" stop-opacity="0.5"/>',
        f'    <stop offset="100%" stop-color="{glow_color}" stop-opacity="0"/>',
        f'  </radialGradient>',
        f'  <filter id="ledBloom" x="-150%" y="-150%" width="400%" height="400%">',
        f'    <feGaussianBlur stdDeviation="{R*0.8:.2f}" result="blur"/>',
        f'    <feComposite in="SourceGraphic" in2="blur" operator="over"/>',
        f'  </filter>',
        '</defs>',
    ]

    # Ambient glow halo
    c.append(f'<circle r="{R*2.2:.3f}" fill="url(#ledGlow)"/>')

    # Bloom layer
    c.append(f'<circle r="{R:.3f}" fill="{glow_color}" opacity="0.4" filter="url(#ledBloom)"/>')

    # LED body
    if shape == "circle":
        c.append(f'<circle r="{R:.3f}" fill="url(#ledBody)"/>')
        # Rim
        c.append(f'<circle r="{R:.3f}" fill="none" stroke="{color}" stroke-width="0.3" opacity="0.5"/>')
        # Specular
        c.append(f'<circle cx="{-R*0.28:.3f}" cy="{-R*0.30:.3f}" r="{R*0.32:.3f}" '
                 f'fill="white" opacity="0.45"/>')
        c.append(f'<circle cx="{-R*0.20:.3f}" cy="{-R*0.22:.3f}" r="{R*0.12:.3f}" '
                 f'fill="white" opacity="0.7"/>')
    elif shape == "square":
        c.append(f'<rect x="{-R:.3f}" y="{-R:.3f}" width="{dia}" height="{dia}" '
                 f'rx="{R*0.3:.2f}" fill="url(#ledBody)"/>')
        c.append(f'<rect x="{-R+0.3:.3f}" y="{-R+0.3:.3f}" width="{dia-0.6:.2f}" height="{dia*0.4:.2f}" '
                 f'rx="{R*0.2:.2f}" fill="white" opacity="0.2"/>')

    svg_file(name, W, W, c)

print("Generating LEDs...")
make_led("RDM_LED_Green",   dia=3.2, color=G_LED)
make_led("RDM_LED_Red",     dia=3.2, color=R_LED)
make_led("RDM_LED_Blue",    dia=3.2, color=B_LED)
make_led("RDM_LED_Yellow",  dia=3.2, color=Y_LED)
make_led("RDM_LED_White",   dia=3.2, color=W_LED)
make_led("RDM_LED_Med_Green",  dia=5.0, color=G_LED)
make_led("RDM_LED_Med_Red",    dia=5.0, color=R_LED)
make_led("RDM_LED_Med_Yellow", dia=5.0, color=Y_LED)
make_led("RDM_LED_Large_Green",dia=9.0, color=G_LED)   # RUN button

# ═══════════════════════════════════════════════════════════════════════════════
# BUTTONS
# ═══════════════════════════════════════════════════════════════════════════════

def make_button(name, w, h, style="normal"):
    """Square/rect momentary button — centred on (0,0)."""
    x, y = -w/2, -h/2
    rx   = min(2.2, w*0.15)

    if style == "normal":
        fill_top = "#3e3e3e"; fill_bot = "#1e1e1e"
        stroke   = "#585858"
    elif style == "dice":
        fill_top = "#cc3333"; fill_bot = "#881111"
        stroke   = "#ff4444"
    elif style == "lock":
        fill_top = "#8a6000"; fill_bot = "#5a3d00"
        stroke   = "#cc8800"
    elif style == "run":
        fill_top = "#1a5a1a"; fill_bot = "#0a2f0a"
        stroke   = "#22aa22"

    c = [
        '<defs>',
        f'  <linearGradient id="btnG" x1="0" y1="0" x2="0" y2="1">',
        f'    <stop offset="0%"   stop-color="{fill_top}"/>',
        f'    <stop offset="100%" stop-color="{fill_bot}"/>',
        f'  </linearGradient>',
        f'  <filter id="btnShadow">',
        f'    <feDropShadow dx="0" dy="0.8" stdDeviation="1.0"',
        f'                  flood-color="#000" flood-opacity="0.65"/>',
        f'  </filter>',
        '</defs>',
    ]

    # Drop shadow
    c.append(f'<rect x="{x:.2f}" y="{y+0.8:.2f}" width="{w}" height="{h}" '
             f'rx="{rx}" fill="#000" opacity="0.4" filter="url(#btnShadow)"/>')

    # Body
    c.append(f'<rect x="{x:.2f}" y="{y:.2f}" width="{w}" height="{h}" '
             f'rx="{rx}" fill="url(#btnG)" stroke="{stroke}" stroke-width="0.55"/>')

    # Top bevel highlight
    c.append(f'<rect x="{x+0.6:.2f}" y="{y+0.5:.2f}" width="{w-1.2:.2f}" height="{h*0.22:.2f}" '
             f'rx="{rx*0.7:.2f}" fill="white" opacity="0.06"/>')

    # Bottom shadow edge
    c.append(f'<rect x="{x+0.6:.2f}" y="{y+h-1.5:.2f}" width="{w-1.2:.2f}" height="1.0" '
             f'rx="{rx*0.5:.2f}" fill="black" opacity="0.25"/>')

    # Style-specific decor
    if style == "dice":
        # Dice pips (2x3 grid offset)
        for dx, dy in [(-3.5,-3.5),(0,-3.5),(3.5,-3.5),(-3.5,0),(0,0),(3.5,0)]:
            c.append(f'<circle cx="{dx:.1f}" cy="{dy:.1f}" r="1.3" fill="white" opacity="0.9"/>')

    elif style == "lock":
        # Padlock icon
        c.append(f'<path d="M -2.5,-1 A 2.5,2.5 0 0,1 2.5,-1 L 2.5,0.5 L -2.5,0.5 Z" '
                 f'fill="none" stroke="{Y_LED}" stroke-width="1.1" stroke-linecap="round"/>')
        c.append(f'<rect x="-3" y="0" width="6" height="4.5" rx="0.8" fill="{Y_LED}"/>')
        c.append(f'<circle cx="0" cy="2" r="1" fill="{fill_bot}"/>')

    elif style == "run":
        # Play triangle
        c.append(f'<polygon points="-2.5,-3.5 -2.5,3.5 4.5,0" fill="{G_LED}" opacity="0.9"/>')

    elif style == "normal":
        # Small indicator dot centre
        c.append(f'<circle cx="0" cy="0" r="{min(w,h)*0.12:.2f}" '
                 f'fill="#444" stroke="#555" stroke-width="0.3"/>')

    svg_file(name, w+6, h+6, c, centre=True)

print("Generating buttons...")
make_button("RDM_BtnSquare",  w=14.0, h=10.0, style="normal")
make_button("RDM_BtnDice",    w=18.0, h=14.0, style="dice")
make_button("RDM_BtnLock",    w=14.0, h=11.0, style="lock")
make_button("RDM_BtnRun",     w=14.0, h=11.0, style="run")

# ═══════════════════════════════════════════════════════════════════════════════
# Summary
# ═══════════════════════════════════════════════════════════════════════════════
files = os.listdir(OUT)
print(f"\nDone — {len(files)} component SVGs in {OUT}/")
