import sys
sys.path.insert(0,"panel_src")
from embed_components import emit

# ── EAST: 6 rows × (2 jacks + 2 attens). cvId(r,c)=r*2+c, attenId(r,c)=ATTEN_START+r*2+c
def east():
    ROW_TOP,ROW_BOT,N=14.0,108.0,6
    def rowY(r): return ROW_TOP+(r+0.5)*(ROW_BOT-ROW_TOP)/N
    COL=dict(J1=8.0,J2=18.0,A1=30.0,A2=39.0)
    inputs, params = [], []
    for r in range(6):
        y=rowY(r)
        inputs.append((f"CV_{r}_0", COL['J1'], y))
        inputs.append((f"CV_{r}_1", COL['J2'], y))
        params.append((f"ATTEN_{r}_0", COL['A1'], y))
        params.append((f"ATTEN_{r}_1", COL['A2'], y))
    # 3 spread display trimpots (SPREAD_R/M/O) — at editor right edge, decorative positions
    # (these are params 0-2; placed near top of editor area)
    return params, inputs

# ── MACRO: same 4-col layout, W=132.08
def macro():
    ROW_TOP,ROW_BOT,N=14.0,108.0,6
    def rowY(r): return ROW_TOP+(r+0.5)*(ROW_BOT-ROW_TOP)/N
    COL=dict(J1=8.0,J2=18.0,A1=30.0,A2=39.0)
    inputs, params = [], []
    for r in range(6):
        y=rowY(r)
        inputs.append((f"CV_{r}_0", COL['J1'], y))
        inputs.append((f"CV_{r}_1", COL['J2'], y))
        params.append((f"ATTEN_{r}_0", COL['A1'], y))
        params.append((f"ATTEN_{r}_1", COL['A2'], y))
    return params, inputs

# ── MONO: 6 lanes × (4 jacks + 4 attens). cvId(lane,p)=lane*4+p
def mono():
    ROW_TOP,ROW_BOT,N=14.0,108.0,6
    def laneY(l): return ROW_TOP+(l+0.5)*(ROW_BOT-ROW_TOP)/N
    JACK_X=[6.0,16.0,26.0,36.0]; ATTEN_X=[46.0,55.0,64.0,73.0]
    inputs, params = [], []
    for lane in range(6):
        y=laneY(lane)
        for p in range(4):
            inputs.append((f"CV_{lane}_{p}", JACK_X[p], y))
            params.append((f"ATTEN_{lane}_{p}", ATTEN_X[p], y))
    return params, inputs

jobs = [
    ("East",  east,  ["StraitsEastSandsVisual_36HP.svg","StraitsEastSandsVisual_36HP_light.svg"]),
    ("Macro", macro, ["StraitsSandsMacroVisual_26HP.svg","StraitsSandsMacroVisual_26HP_light.svg"]),
    ("Mono",  mono,  ["SandsMonoVisual_34HP.svg","SandsMonoVisual_34HP_light.svg"]),
]
for name, fn, files in jobs:
    params, inputs = fn()
    for f in files:
        path=f"res/panels/{f}"
        n=emit(path, params, inputs, [], [])
        print(f"{name} {f}: {len(params)} params, {len(inputs)} inputs embedded")
