# Canonical claim — the precise technical description (build positioning from this)

The maximal TRUE claim (Rodney). Every term is load-bearing, factual, defensible — not marketing
inflation. Nothing else in the Rack library (arguably in software instruments broadly) is this.

## The full technical claim
A **time-reversible random generator** based on **7 lanes x 16 channels of moving-average Gaussian
copulas**, with **slew and mix controls**, the ability to **construct musically interesting
correlation structures**, and to **modulate both the correlation STRUCTURE and the correlation
AMOUNTS** — plus **LOR (length/offset/rotation) and direction** per lane in Sands.

Unpacked:
- **Time-reversible** — scrub, reverse, bit-exact (Philox counter-addressed spine; AD-style exact
  reverse).
- **7 x 16 moving-average Gaussian copulas** — structured joint-distribution model per lane per voice,
  not scalar randomness.
- **Slew + mix controls** — temporal (slew) and interpolation (mix) shaping of the correlation.
- **Constructible correlation structures** — build meaningful voice relationships (via Change Alley),
  not one global randomness amount.
- **Modulate structure AND amount** — the correlation TOPOLOGY (who relates to whom) and the
  correlation STRENGTH (how much) are each independently modulatable.
- **Global pattern length + per-lane LOR within it + direction (Sands)** — a TWO-LEVEL time structure:
  the global length sets the loop/phrase boundary; per-lane LOR (length/offset/rotation) + direction
  operate INSIDE that frame, so lanes are POLYMETRIC/phased against each other and the global cycle
  (e.g. a length-7 melody lane against a length-5 rhythm lane inside a 16 global), drifting and
  realigning over the global loop — all reversible.

## Three orthogonal axes (the full space)
All TIME-REVERSIBLE:
1. **Time structure** — global pattern length + per-lane LOR + direction (polymetric, phased).
2. **Probability** — the 7x16 copula field (order<->chaos in WHAT happens).
3. **Correlation** — constructible + modulatable structure AND amount (order<->chaos in HOW voices
   relate).
Time-structure is the compositional-time axis; probability is content; correlation is ensemble. Three
independent dimensions of a generative space, each navigable and reversible.

## Two registers — SAME machinery, different audiences
- **Technical audience** (developers, serious synthesists, the knowledgeable community corner): use the
  full claim verbatim. "Time-reversible 7x16 moving-average Gaussian copula with modulatable
  correlation structure and amount" is a credential AND a differentiator — it signals this is built on
  foundations the field doesn't use. LEAD with the rigour here. Manual architecture section, docs,
  dev-facing material.
- **Musician audience** (headline, demo, first thing a browsing musician sees): "copula" is a wall.
  Translate to musical words: "voices that relate to each other in ways you control and can change live
  — from locked-together to independent to interlocking — and you can rewind it." Same instrument,
  musical language. Do NOT water down the technical claim; just don't LEAD a general audience with it.

## Why this matters
dot.modular can make the strong technical claim HONESTLY — most "generative" tools would be
overclaiming to describe themselves this way; dot.modular would be UNDERclaiming not to. State the
precise version prominently SOMEWHERE (for the people who recognise what it means — the early adopters
and advocates who carry it), translated per audience elsewhere. Precision is the credential; the same
rigour that built the instrument should describe it.
