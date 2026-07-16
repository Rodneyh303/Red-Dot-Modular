import re, sys

def stops(defs, gid):
    m = re.search(r'id="%s"[^>]*>(.*?)</(?:linear|radial)Gradient>' % gid, defs, re.S)
    out = []
    for off, col, op in re.findall(r'offset="([\d.]+)%"\s+stop-color="(#[0-9a-fA-F]{6})"\s+stop-opacity="([\d.]+)"', m.group(1)):
        out.append((float(off)/100.0, col, float(op)))
    return out

def lerp(a,b,t): return a + (b-a)*t
def sample(st, x):
    if x <= st[0][0]: return st[0][1], st[0][2]
    for i in range(len(st)-1):
        (o0,c0,a0),(o1,c1,a1) = st[i], st[i+1]
        if x <= o1:
            t = 0 if o1==o0 else (x-o0)/(o1-o0)
            return c0, lerp(a0,a1,t)
    return st[-1][1], st[-1][2]

def mix(c1, c2, f):
    r1,g1,b1 = int(c1[1:3],16), int(c1[3:5],16), int(c1[5:7],16)
    r2,g2,b2 = int(c2[1:3],16), int(c2[3:5],16), int(c2[5:7],16)
    return '#%02x%02x%02x' % (round(lerp(r1,r2,f)), round(lerp(g1,g2,f)), round(lerp(b1,b2,f)))

def convert(path):
    s = open(path).read()
    defs = re.search(r'<defs>.*?</defs>', s, re.S).group(0)

    # ── grain: a 0.5px-pitch scanline pattern. Sub-pixel at any real zoom, so it can only
    #    ever read as a flat tint anyway — emit that tint as one solid fill.
    pm = re.search(r'<pattern id="grain"[^>]*>(.*?)</pattern>', defs, re.S).group(1)
    rects = re.findall(r'<rect[^>]*fill="(#[0-9a-fA-F]{6})"(?:[^>]*opacity="([\d.]+)")?[^>]*/>', pm)
    base = rects[0][0]
    line_col = rects[1][0]; line_op = float(rects[1][1] or 1.0)
    ph = float(re.search(r'<pattern id="grain"[^>]*height="([\d.]+)"', defs).group(1))
    lh = float(re.search(r'<rect y="0.25" width="[\d.]+" height="([\d.]+)"', pm).group(1))
    grain = mix(base, line_col, (lh/ph) * line_op)
    s = s.replace('fill="url(#grain)"', 'fill="%s"' % grain)

    # ── skyGrad: vertical fade over a rect -> disjoint horizontal bands (no overlap, so each
    #    band takes the sampled alpha directly).
    m = re.search(r'<rect x="0" y="0" width="([\d.]+)" height="([\d.]+)" fill="url\(#skyGrad\)"/>', s)
    w, h = float(m.group(1)), float(m.group(2))
    st = stops(defs, 'skyGrad'); N = 10
    bands = []
    for i in range(N):
        y0 = h*i/N; bh = h/N
        col, a = sample(st, (i+0.5)/N)
        if a > 0.004:
            bands.append('<rect x="0" y="%.2f" width="%.2f" height="%.2f" fill="%s" opacity="%.3f"/>' % (y0,w,bh,col,a))
    s = s.replace(m.group(0), '<g stroke="none">' + ''.join(bands) + '</g>')

    # ── ringGlow: radial falloff -> NESTED circles. They overlap, so alphas composite:
    #    A(r) = 1 - prod(1-a_i) over every circle enclosing r. Solve each ring's alpha from
    #    the target so the stack lands on the gradient's own curve instead of over-darkening.
    m = re.search(r'<circle cx="([\d.]+)" cy="([\d.]+)" r="([\d.]+)" fill="url\(#ringGlow\)" stroke="none"/>', s)
    cx, cy, R = float(m.group(1)), float(m.group(2)), float(m.group(3))
    st = stops(defs, 'ringGlow'); K = 6
    acc, rings = 1.0, []
    for i in range(K):
        r = R * (1 - i/float(K))
        col, target = sample(st, r/R)
        need = 1.0 - target
        a = 1.0 - (need/acc) if acc > 1e-6 else 0.0
        a = max(0.0, min(1.0, a)); acc *= (1.0 - a)
        if a > 0.004:
            rings.append('<circle cx="%.2f" cy="%.2f" r="%.2f" fill="%s" opacity="%.3f"/>' % (cx,cy,r,col,a))
    s = s.replace(m.group(0), '<g stroke="none">' + ''.join(rings) + '</g>')

    # ── treeGrad: a GROUP fill. Its shapes can't be banded without clipping, so take the
    #    gradient's dominant stop as a flat silhouette tint (the panel-generator house rule).
    st = stops(defs, 'treeGrad')
    col, a = sample(st, 0.75)
    s = s.replace('<g fill="url(#treeGrad)" stroke="none">',
                  '<g fill="%s" opacity="%.2f" stroke="none">' % (col, a))

    s = s.replace(defs, '<!-- defs removed: nanosvg renders no gradients/patterns; every fill is now solid -->')
    open(path,'w').write(s)
    print('%-46s grain=%s  bands=%d  rings=%d  trees=%s@%.2f' % (path.split('/')[-1], grain, len(bands), len(rings), col, a))

for p in sys.argv[1:]: convert(p)
