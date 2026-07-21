
## MRT line colours for the 6 Sands strands (parked — try some other time)

Observation (Rodney, morning commute): the MRT map has 6 lines; Sands has 6 strands.
Map the six official SMRT/LTA line colours onto the six strands as a per-lane identity
palette — deeply Singapore, recognisable, and each colour already carries strong
standalone identity (they were designed to be told apart at a glance on a map).

The six lines and their official colours (for when we try this):
- North South Line (NSL)      — red      #d42e12  (brand-adjacent — careful vs our #d4001a)
- East West Line (EWL)        — green    #009645
- North East Line (NEL)       — purple   #9900aa
- Circle Line (CCL)           — orange   #fa9e0d
- Downtown Line (DTL)         — blue     #005ec4
- Thomson-East Coast (TEL)    — brown    #9d5b25

Strand→line assignment is open (aesthetic, not semantic). One natural cut: the two
"pitch" strands (MELODY, OCTAVE) get the two trunk lines (NSL red / EWL green), the
rhythm family (RHYTHM, ACCENT) get CCL orange / DTL blue, and the articulation pair
(VARIATION, LEGATO) get NEL purple / TEL brown. Revisit against the dark LCD ground —
purple and brown may need lifting for contrast on #0b0c0d.

Caveat: NSL red (#d42e12) sits very close to brand red (#d4001a); if used as a lane
colour it competes with accent/connection red. Either reassign NSL's slot or shift it.
Applies wherever lanes are colour-coded: Lantern lanes, Sands editors, Change Alley
pool tinting.

### Refinement (Rodney): two lane-palette THEMES, and the red-role conflict

Structure it as TWO selectable lane palettes, not a replacement:
- **Pastel** (current) — the existing muted lane hues. Stays the default identity.
- **MRT** — the six line colours as lane identity.
Palette becomes a theme choice (context menu / settings), like dark/light already is.
This keeps the pastel work and makes MRT opt-in.

The red-role conflict (the real design pin): brand red #d4001a currently carries a
FUNCTIONAL role — accent emphasis (Lantern top edge, accented notes) and expander
CONNECTION (the connect dot). In MRT theme the NSL lane wants red too, and it should be
BRAND red (#d4001a), not MRT's #d42e12 — one canonical red in the product. That means a
lane and a functional signal would share the exact colour.

Resolution: keep brand red for the NSL LANE in MRT theme, and move the
ACCENT/CONNECTION functional signal to a different colour SO THE FUNCTION STAYS
LEGIBLE against a red lane. Candidates for the functional accent/connection colour:
- amber/gold #c8960c (already a secondary brand colour; reads as "active/attention")
- bright white — but white is a Change Alley pin (rhythm), so context-dependent
Leaning amber: it's already in the palette, distinct from every MRT line, and "amber =
this is the live/accented/connected thing" is a clean second signal alongside red.

Note this makes the accent/connection colour a per-theme value too: pastel theme can
keep red-as-accent (no lane collision there), MRT theme uses amber-as-accent. So the
functional-signal colour is bound to the lane palette, not global. Decide together when
we do the colour pass.
