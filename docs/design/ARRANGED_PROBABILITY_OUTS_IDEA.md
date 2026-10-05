# Arranged probability outputs — idea (Intertropical or a companion output module)

STATUS: design idea — RAISED PRIORITY. Additive, optional, HP-contained. It EARNS a core CLAIM.

**Why it matters more than a nice-to-have output:** once Intertropical ROUTES the probabilities (not
just notes/CV), Intertropical is ARRANGING THE RANDOM SOURCE ITSELF. That lets us honestly claim
**random number SOURCE + random number ARRANGER** — with no new feature beyond these arranged
prob-outs. So this isn't just a Burst-patch convenience; it COMPLETES the identity (see CANONICAL_CLAIM).
Almost nothing else arranges the underlying random numbers (anyone can arrange notes). That raises this
from parked-nice-to-have to the-feature-that-earns-a-core-claim.

## The question
Should the probability field (prob-outs) follow Intertropical routing, like CV expression already does?
(CV expression follows the SAME arranger mapping as notes — "expression follows notes, not
independently routable.")

## Resolution — expose BOTH, on different modules (the module boundary IS the answer)
A voice is a bundle (note + CV + probability); the arranger moves the whole bundle. But raw and
arranged probability are both useful, for different things — so expose both, each on the module that
OWNS that stage:
- **Sands prob-outs = RAW generated field**, per-voice as drawn (pre-arrangement). For modulation /
  Burst patches that don't care about arrangement order.
- **Arranged prob-outs = the SAME field routed through the arrangement**, per output slot
  (post-arrangement), ALIGNED with where the notes actually came out. For patches that must land on
  the arranged part (e.g. inverted-rest -> Burst fills on the right voice).

No mode toggle, no "which order is this" confusion — the MODULE tells you which (Sands=raw,
Intertropical/companion=arranged).

## Where it lives: Intertropical, or a companion output module
Intertropical already owns the routing mask (slotOf[v]/slotOutput[slot]), so the arranged outputs are
naturally ITS to emit — the arranged prob-outs likely ride the EXISTING CV routing path (same mask, one
more signal), making it cheap.
- **Part of Intertropical** — natural (owns the mapping), no new module, but adds HP to Intertropical
  (may be tight).
- **Separate companion / expander** — cleaner separation, own HP, but another module. Could be a small
  "arranged outputs" tap that carries ALL arranged signals (arranged CV + arranged probability) — the
  general "here's everything, as arranged" stage.
Lean (HP is binding): if Intertropical has the HP, put it there; else a small companion/expander. The
cleaner framing may be: Intertropical arranges, and an output stage (built-in or companion) taps ANY
arranged signal.

## Why this is the clean answer
Answers "does probability follow the arranger?" without CHOOSING — expose both (raw on Sands, arranged
on Intertropical/companion). Additive, optional, doesn't touch Sands or spend its HP, rides the
existing CV routing mask. NOT on Sands.


## EFFORT — SMALLER than the CV work (Rodney): probabilities already carry the CA mapping
The CV-through-Intertropical work had to pick up the **CA MAPPING** across the three groups (rhythm,
melody, qmix) first — the raw CV didn't carry it, so CV had to be threaded through the CA-group
correlation permutation. **The Sands probability outs ALREADY carry the CA mapping** (that's what makes
them per-voice correlated to begin with). So arranged-prob-outs do NOT need the CA-mapping step CV
needed — they need ONLY the Intertropical ARRANGEMENT routing (slotOf[v]/slotOutput[slot]) applied on
top of the already-CA-mapped probabilities.

So the feature is small: apply Intertropical's EXISTING arrangement mask to signals that are already
ready — not "build routing from scratch", and not the full CA-mapping-plus-routing CV required. The
hard/prior half (CA mapping) is already baked into the Sands prob-outs; only the arranger permutation
remains. Lower effort than parked-as, same claim earned (random source + random arranger).