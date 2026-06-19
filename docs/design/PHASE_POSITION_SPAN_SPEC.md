
═══════════════════════════════════════════════════════════════════════════════
## 11. Boundary dice-state record (enum, per STREAM) — for cross-boundary reverse

Per crossed phrase boundary, store ONE BoundaryDice value PER STREAM. Dice/draws
are NOT per individual lane — there are TWO draw streams that each own a GROUP of
lanes (confirmed from redrawRhythm / redrawMelody):
    RHYTHM stream  → rhythm, variation, legato, accent   (redrawRhythm, rhythm RNG)
    MELODY stream  → melody, octave                       (redrawMelody, melody RNG)
The two streams are independent: a boundary may roll rhythm but not melody (separate
SeedPending/RollPending/... flag sets per stream in applyPendingSeedsAndRedraw). So
the record is two BoundaryDice values per boundary — one for the rhythm stream
(covering its 4 lanes), one for the melody stream (covering its 2). Reverse reads
them to decide whether (and how) to step each stream's draw-counter back when
re-crossing.

NOTE this also implies the §6 block-allocator has TWO draw-counters (rhythmDrawCtr,
melodyDrawCtr) advancing independently — each stream's counter steps only on a
boundary where THAT stream rolled. base(stream, drawCtr, laneWithinStream).

enum class BoundaryDice : uint8_t {
    Reuse,            // no redraw fired — draw carried over; counter UNCHANGED on cross
    SeedReseed,       // seedPending: reproducible reseed (A=B); counter-addressable
    Roll,             // rollPending: advance RNG, no reseed; counter-addressable
    TrialAudition,    // trialPending: A anchored, fresh B (audition); counter-addressable
    ReseedRoll,       // reseedRollPending: main roll + fresh entropy (float seed path)
    LiveMain,         // mode==1 realtime, A walks; redraws every boundary
    LiveTrial,        // mode==1 realtime sourced from trial dice (A anchored)
    LiveFullEntropy   // mode==1 + seedRngFull: NOT counter-reproducible (reverse-limited)
};

Reverse rule when re-crossing a boundary (un-crossing it going backward):
  - Reuse                         → counter unchanged (no draw happened here).
  - SeedReseed/Roll/TrialAudition/ReseedRoll(float) → counter-addressable: step the
    counter DOWN by 1 and regenerate the prior draw from base(counter, key).
  - LiveMain/LiveTrial (counter-sourced) → same: step down, regenerate.
  - LiveFullEntropy → the prior draw used non-counter internal entropy. Either the
    actual entropy was logged (then restore it), OR reverse is declared limited here:
    freeze/anchor at this point (cannot reconstruct the pre-boundary draw). This is
    the user-accepted limitation: reseed-on-roll with full entropy sacrifices
    reverse-reproducibility. (Mitigation noted by user: a future Mode-E variant could
    DISABLE full-entropy reseed-on-roll so all draws stay counter-addressable.)

Forward play WRITES this record at each boundary (cheap, two enums — one per stream).
Only
reverse READS it. So the forward checkpoint branch can populate it even though
nothing consumes it yet — wiring the producer early means the reverse branch only
adds the consumer.

Maps 1:1 onto applyPendingSeedsAndRedraw's branches (rhythmSeedPending,
rhythmRollPending, rhythmTrialPending, rhythmReseedRollPending, rhythmMode==1 +
rhythmLiveTrial + reseedOnRoll/seedRngFull), so classification at the boundary is a
direct read of the same flags the redraw decision already uses.
