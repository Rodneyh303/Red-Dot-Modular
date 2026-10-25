# ARTICULATION: "within an envelope" (containment) vs correlation — a design question (Rodney)

NEEDS MORE THOUGHT/DISCUSSION — note only, not decided. The first time "mono not special" has been
considered MUSICALLY (not just mechanically — indices/decisions/timing). Opened by the MONO_SPECIAL_IN_
LOGIC_AUDIT + the EAST_EXTRA_LANES history.

## LINEAGE (why the current behaviour exists)
Articulation rules originally came from Vermona Melodicer + mex3: "All three voices share NOTE VALUE,
VARIATION and LEGATO; but REST is set individually per voice." → poly voices were SUBTRACTIVE from the
MONO (V1) voice — they articulate WITHIN V1's envelope. This carried into dot.modular; then EAST_EXTRA_
LANES progressively made things per-voice-flexible; eventually FULL equality across all random sources
(every lane). But the "voices articulate within V1's envelope" behaviour was never RE-EXAMINED against
the sophistication we now have (CA + copula correlation). It's a Vermona RESIDUE whose V1-specialness was
never chosen for THIS instrument.

## THE KEY DISTINCTION (the crux)
"Voices articulate within V1's ENVELOPE" (CONTAINMENT/nesting) ≠ "voices CORRELATED to V1" (copula).
- CONTAINMENT (nested/subtractive): V1 defines a gate WINDOW; poly voices can only articulate INSIDE it —
  rest/shorten/subdivide WITHIN V1's note, but CANNOT sound when V1 is silent, CANNOT extend past V1's
  gate. A hard SUPPORT constraint. Lets a voice be rhythmically active (ratchet/subdivide) INSIDE V1's
  held note, guaranteed bounded by it = a specific texture (sustained envelope + inner voices dancing
  within it).
- CORRELATION (copula/CA): each voice draws its OWN rest/legato/length; the DRAWS are statistically
  dependent (tight/loose/anti). A ρ≈1 voice TENDS to rest with V1 but CAN sound when V1 is silent, and
  draws its own gate length. A tendency, not a boundary.
ASYMMETRY (why they're NOT redundant): even ρ=1 correlation makes voices AGREE, it does NOT make a voice
UNABLE to sound outside another's gate, nor let it freely subdivide INSIDE while hard-bounded OUTSIDE.
Containment is a support-nesting constraint; correlation operates on the decisions. CORRELATION CANNOT
REPRODUCE CONTAINMENT. They are COMPLEMENTARY, composable axes.

## THE TWO QUESTIONS (Rodney)
1. Keep / flex / remove "within V1 envelope"?
   LEANING (not decided): KEEP the containment CAPABILITY (unique — correlation can't do it), but STRIP
   the V1-SPECIALNESS (the Vermona residue). Don't remove (loses the sustained-envelope+inner-voices
   texture); don't keep-as-is (V1-hardcoded = the mono-special being eradicated). → DECOUPLE the envelope
   from V1.
2. Allow "within ANOTHER envelope" (CA source, arbitrary)?
   LEANING (not decided): YES — make the ARTICULATION ENVELOPE a routable per-voice/per-group SOURCE:
   within V1 (current, now an OPTION) / within a CA-routed source (→ NESTED VOICE GROUPS: voices 2-4
   within v1's envelope, 5-7 within v5's) / within an external CV or another lane's gate / within NONE
   (independent — the full-equality case we have). Same "stop hardcoding V1, make it a routed input"
   pattern as spread/warp/direction.

## RESOLUTION OF "MONO NOT SPECIAL" (the musical meaning)
"Mono not special" MUSICALLY = V1 is not PRIVILEGED, but CAN be CHOSEN as a reference. Vermona made V1
the MANDATORY envelope; equality makes V1 one SELECTABLE envelope among sources (itself / CA / external /
none). So "mono not special" = "the envelope is ROUTABLE, defaulting to V1 for compatibility", NOT "no
envelopes". Equality DE-PRIVILEGES the envelope into a routing choice; it does not abolish it.

## HOW IT FOLDS INTO THE ARCHITECTURE (if pursued)
- Envelope SOURCE = a routing decision (CA-like, or the resolver's ownership/delegation generalized from
  "whose VALUE" to "whose ENVELOPE").
- CONTAINMENT = a DECISION-MANAGER rule: "this voice's gate decision is BOUNDED by {envelope source}'s
  gate window" — fires at step edge, mode-independent; a decide() input:
  decide(prob, draw, lane, ownership, ENVELOPE_SOURCE) → clamp the voice's gate to the envelope's window.
- V1-default = just the default routing (envelope = V1), preserving current behaviour until re-routed.
- Composes with correlation: a voice can be CONTAINED within V1 AND correlated in its within-envelope
  articulation — nested AND statistically related.
So it's NOT new machinery — it's the envelope-source routed through the decision-manager + CA authorities.

## THEORY CONNECTION (for REACHABLE_STRUCTURES_THEORY)
CONTAINMENT is a dependence relation that is NOT a copula — it is a hard SUPPORT-NESTING constraint on the
gate (copulas describe the DECISION dependence; containment constrains the SUPPORT). So it sits OUTSIDE
the copula framework as a separate structural operator. Worth adding to the reachable-structures map as
"the non-copula dependence: support nesting (containment), composable with the copula decision-dependence".

STATUS: note only, needs discussion. Decide (1) keep+generalize vs remove, (2) which envelope sources,
when the decision-manager + CA work is on the table (its natural vehicle). The containment-vs-correlation
distinction is the thing to hold onto: they are complementary, not substitutes.

────────────────────────────────────────────────────────────────────────────────
## ADVANCING THE DESIGN (Rodney) — CA source as envelope, chaining, gate mode, the control

Following the CA-spread fix (resolver computes / rate manager schedules / convergence-at-step), the
envelope can ride the SAME resolver+CA machinery. Resolved points:

### 1. The RHYTHM CA source IS the natural envelope — but containment is a SEPARATE toggle from spread
The articulation envelope is a RHYTHM concept (gate windows) → route it via the RHYTHM CA source. BUT the
rhythm CA source currently drives CORRELATION (spread-follow-CA). Using it ALSO as the CONTAINMENT envelope
means ONE routing carries TWO relations: "correlated to the source" (spread) AND "contained within the
source's gate" (envelope). They COMPOSE (complementary, established above) but must be SEPARATELY
switchable. → ONE routing (which CA source), TWO independent relations.

### 2. CHAINING gives TRANSITIVE NESTING for free (CA tree = containment hierarchy)
Does it matter if the CA source is identity-mapped or chained? YES, and it's a GIFT:
- IDENTITY source: v2 contained in v3, v3 is its own gate → one level (v2 ⊂ v3).
- CHAINED source: v2 in v3, v3 itself in v1 → containment is a SUPPORT constraint (can't sound outside the
  gate), so v3's ACTUAL gate is already v3∩v1 (it's clamped before v2 reads it). v2 ⊂ v3 ⊂ v1 holds
  TRANSITIVELY BY CONSTRUCTION — each level clamps before the next reads. So deep CA chains = deep nesting
  (inner voices within inner voices within an outer envelope) FOR FREE. The CA ROUTING TREE *is* the
  CONTAINMENT HIERARCHY, composing with the copula correlation on the same rank-1-per-block tree.
- NO CYCLES (Rodney, confirmed): a routing cycle makes containment ill-defined (mutual clamp). The CA tree
  must be ACYCLIC — containment makes cycle-prevention LOAD-BEARING (it already is for correlation).

### 3. GATE MODE IS FINE — the envelope is about DECISIONS, not the timing grid (Rodney corrected)
Earlier (wrong): "gate mode → envelope = V1-only, because V1 sources the gates". CORRECTION: V1 sources
the gate/subgate TIMING GRID (when gate events happen), but EACH VOICE makes its OWN rest/legato/accent
decisions at those gate AND subgate events (eventually with own-knob response — the planned 2nd knobs).
Containment operates on the ARTICULATION DECISIONS (whose rest/legato nests whom), NOT on who generates
the timing grid. So a voice CAN be contained within another's articulation envelope in gate mode (its
rest/legato decisions nested within the source's) even though both articulate on V1's shared grid.
=> FOLLOW-CA ENVELOPE IS VALID IN ALL MODES (clock/phase/gate). The envelope routing is NOT mode-
dependent; only the TIMING-GRID source differs by mode (per-voice gates in clock/phase, V1's grid in gate),
and the grid isn't the envelope. (This is cleaner than the mode-dependent version — no special-casing.)

### 4. THE CONTROL — a SEPARATE articulation-envelope context menu (Rodney)
A new context menu for the ARTICULATION ENVELOPE: NONE / FOLLOW V1 / FOLLOW CA — INDEPENDENT from the
existing SPREAD-target menu (None/Follow-V1/Follow-CA). Same three options, same routing targets, TWO
independent relations:
  SPREAD menu   = CORRELATION (does my probability track the source's).
  ENVELOPE menu = CONTAINMENT (am I nested within the source's gate window).
→ the full 3×3 of containment × correlation, independently chosen. E.g. envelope=Follow-CA + spread=None =
nested but UNcorrelated (contained, independent decisions within); envelope=None + spread=Follow-CA =
correlated but UNcontained (tend to agree, can sound outside). ENVELOPE=NONE (Rodney): no containment, but
STILL full copula correlation + atoms + everything — just not nested gating. That's the full-equality case
(likely the DEFAULT: uncontained unless routed). ENVELOPE=FOLLOW-V1 = the Vermona "within V1" behaviour —
now ONE option among three, not the law. ("Mono not special" = the envelope is a routing CHOICE, default
uncontained; V1 selectable, not mandatory.)

### STATUS
Design substantially advanced; still future work (with the decision-manager/modifier-resolver + the
planned 2nd-knob poly articulation). To build: the envelope as a resolver modifier policy (clamp gate to
{None/V1/CA-source} window), the separate context menu, transitive nesting via the acyclic CA tree, valid
in all modes. The containment-vs-correlation two-axis structure (two menus) is the core.
