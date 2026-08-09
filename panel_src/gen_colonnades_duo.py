#!/usr/bin/env python3
"""Colonnades Duo (Micro-24) panel — literally gen_colonnades.py with N=24.

COLONNADES_DUO_PANEL_SPEC.md, Option A (single-source): the Duo panel IS the Colonnades generator at
N=24. Same 9.0mm pitch, same FIRST_X, same fader travel, same level ticks, same two-row staggered
cents grid (ROUND 7) — just 24 of everything, so it reads as two Colonnades fader blocks end-to-end
(W = 7.5 + 23*9.0 + 7.5 = 222mm ≈ 43.7HP). NO Duo-specific geometry: importing gen() guarantees any
future Colonnades panel tweak applies to the Duo automatically. Wordmark ("Colonnades Duo") + NOTES
range (1..24) are widget-side, not panel-side.
"""

from gen_colonnades import render

def main():
    render(24, "ColonnadesDuo")

if __name__ == "__main__":
    main()
