#include "PatternEngine.hpp"

void PatternEngine::reset() {
    rhythmSeedPending = melodySeedPending = false;
    rhythmRollPending = melodyRollPending = false;
    rhythmPendingLast = melodyPendingLast = false;
    rhythmReseedRollPending = melodyReseedRollPending = false;
    rhythmReseedRollFull = melodyReseedRollFull = false;
    rhythmMode = melodyMode = 0;
    rhythmSeedCached = melodySeedCached = false;

    // Initialise the addressable Philox draw engines from the current seed floats
    // (default 0 → a fixed reproducible starting sequence, counter at 0).
    seedRhythmPhilox(rhythmSeedFloat);
    seedMelodyPhilox(melodySeedFloat);

    // strands must not be all-zero or module is silent until dice/phrase
    for (int i = 0; i < 16; ++i) {
        rhythmRandom[i] = 1.0f;     // Mono triggers by default
        variationRandom[i] = 0.5f;  // No variation bias
        legatoRandom[i] = 0.0f;     // No legato/ties
        accentRandom[i] = 1.0f;     // No accents by default (roll < prob)
        melodyRandom[i] = 0.5f;     // Default to middle of weighted sum
        octaveRandom[i] = 0.5f;     // Default to middle of octave range
        
        for (int v = 0; v < 15; v++) {
            polyRandom(v, PL_REST)[i] = 1.0f; // Poly voices trigger by default
            polyRandom(v, PL_ACCENT)[i] = 1.0f; // No accent by default (1.0 < accentProb is false).
                                           // BUG FIX: this seed was missing (rhythm had it, accent
                                           // didn't), so polyAccentRandom stayed 0 → 0<accentProb
                                           // always true → EVERY poly note accented on any nonzero
                                           // accent knob. Mirrors polyAccentSource=1.0 below.
            polyRandom(v, PL_MELODY)[i] = 0.5f;
            polyRandom(v, PL_OCTAVE)[i] = 0.5f;
            
            polyRhythmSource[v][i] = 1.0f;
            polyAccentSource[v][i] = 1.0f;
            polyMelodySource[v][i] = polyRandom(v, PL_MELODY)[i];
            polyOctaveSource[v][i] = polyRandom(v, PL_OCTAVE)[i];
        }
        
        rhythmSource[i] = rhythmRandom[i];
        variationSource[i] = variationRandom[i];
        legatoSource[i] = legatoRandom[i];
        accentSource[i] = accentRandom[i];
        melodySource[i] = melodyRandom[i];
        octaveSource[i] = octaveRandom[i];

        rhythmPattern[i] = true;
    }

    // (Step 4c: removed A/B init -- no stored A/B arrays under the scrub model.)

    rhythmSlewLatched=melodySlewLatched=1.f;
    rhythmSlewApplied=melodySlewApplied=1.f;
    rhythmFirstDraw=melodyFirstDraw=true;
    sandsActive=false;
    // Mirror defaults into slewedDraw too (final == slewed at reset)
    for (int i=0;i<16;i++){
        slewedRhythm[i]=rhythmRandom[i]; slewedVariation[i]=variationRandom[i];
        slewedLegato[i]=legatoRandom[i]; slewedAccent[i]=accentRandom[i];
        slewedMelody[i]=melodyRandom[i]; slewedOctave[i]=octaveRandom[i];
        for (int v=0;v<15;v++){
            slewedPolyRhythm[v][i]=polyRandom(v, PL_REST)[i];
            slewedPolyMelody[v][i]=polyRandom(v, PL_MELODY)[i];
            slewedPolyOctave[v][i]=polyRandom(v, PL_OCTAVE)[i];
        }
    }
}

// ── Core generation ───────────────────────────────────────────────────────

// Pick a DEGREE (0..n-1) weighted by fader values using a provided random value. `n` = active degree
// count (tuning.N): 12 for built-in/Sikit/Micro-12, up to 24 for Micro-24. At n=12 the loop bounds,
// sum, walk, and float-safety return are all identical to the legacy pickSemitone → byte-identical.
int PatternEngine::pickSemitone(const float weights[], int n, float r_val) {
    float sum = 0.f;
    for (int i = 0; i < n; ++i) sum += weights[i];
    if (sum <= 0.0001f) return -1;

    float r = pe_clamp(r_val, 0.f, 1.f - 1e-7f) * sum;
    float acc = 0.f;
    for (int i = 0; i < n; ++i) {
        if (weights[i] <= 0.f) continue;
        acc += weights[i];
        if (r < acc) return i;
    }
    return n - 1;  // float safety (was 11 at n=12)
}

// Generate a pitch voltage (1V/oct, 0..5V) and return the semitone.
float PatternEngine::genPitch(int& outSemitone, const PatternInput& in) {
    return genPitchLive(outSemitone, in, unitMelody(), unitMelody());
}

// Generate a pitch voltage using provided random floats for semitone and octave selection.
float PatternEngine::genPitchLive(int& outSemitone, const PatternInput& in, float r_semi, float r_oct) {
    // Degree count from the shared tuning table (12 default; up to 24 for a Micro-24). pickSemitone is
    // N-bounded so the weighted walk only considers active degrees.
    int sem = pickSemitone(in.semiWeights, tuning.N, r_semi);
    outSemitone = (sem < 0) ? 0 : sem;
    if (sem < 0) return 0.f;

    float lo = in.octaveLo, hi = in.octaveHi;
    if (hi < lo) std::swap(lo, hi);
    int oL = (int)std::floor(lo);
    int oH = (int)std::floor(hi);
    // Ensure r_oct roll isn't exactly 1.0 to prevent oOB indexing
    float roll = pe_clamp(r_oct, 0.f, 0.9999f);
    int oct = oL + (int)std::floor(roll * float(oH - oL + 1));

    // Degree -> voltage via the shared TuningTable (Sikit Phase 1). The octave integer and the
    // 12-TET transpose stay unchanged (Rodney's Phase-1 ruling: transpose remains 12-TET semitones);
    // only the WITHIN-OCTAVE degree term is table-driven. While the table is the equal-division
    // 12-TET default, take the EXACT legacy expression so this refactor is bit-identical; the cents
    // path activates only when a Sikit publishes non-default cents.
    float v;
    if (tuning.isDefault12TET) {
        v = float(oct) - 4.f + (sem + in.transpose) / 12.f;   // legacy path — byte-identical
    } else {
        // Custom tuning (Micro): TRANSPOSE IS OCTAVES-ONLY (Rodney). A 12-TET semitone shift (/12)
        // lands OFF the tuning grid; an octave is +1V in ANY tuning, so it's always tuning-native.
        // Monsoon's transpose knob is ±12 semitones → snap to the nearest whole octave and apply as
        // whole volts (±1 octave from this knob; Intertropical's own per-output knobs cover ±2).
        const float octShift = std::round(in.transpose / 12.f);   // -1 / 0 / +1
        v = float(oct) - 4.f + tuning.degreeVolts(sem) + octShift;
    }
    // Allow full bipolar range for 1V/oct standard; clamping to 0V clips octaves 0-3
    return pe_clamp(v, -5.f, 5.f);
}

// Apply variation bias to a note length index.
int PatternEngine::varyNoteIndex(int baseIdx, const PatternInput& in, float r) {
    // Weights are evenly distributed over a window that expands from the base 
    // index toward the shorter (var > 0.5) or longer (var < 0.5) extremes. 
    // At 50% variation, only the base note is played. At extremes, weight 
    // is shared uniformly across the reachable range.

    auto allowed = [&](int idx) -> bool {
        if (idx < 0 || idx >= 8) return false;
        if (idx == 3) return (in.noteVariationMask & 0b001) != 0; // 1/4T
        if (idx == 5) return (in.noteVariationMask & 0b010) != 0; // 1/8T
        if (idx == 7) return (in.noteVariationMask & 0b100) != 0; // 1/32 & 1/32T
        return true;
    };

    float var = in.variationAmount;
    if (std::fabs(var - 0.5f) < 1e-4f) return baseIdx;

    float spread = 2.f * std::fabs(var - 0.5f); // 0..1
    int direction = (var < 0.5f) ? -1 : 1;
    int targetExtreme = (direction == -1) ? 0 : 7;
    float maxDist = (float)std::abs(targetExtreme - baseIdx);
    
    if (maxDist < 0.5f) return baseIdx; 

    float reach = spread * maxDist;
    float weights[8] = {};
    float total = 0.f;

    for (int i = 0; i < 8; ++i) {
        int dist = direction * (i - baseIdx);
        if (dist < 0) continue; // Only consider chosen direction
        
        if (i == baseIdx) {
            weights[i] = 1.0f; // Base note always active
        } else {
            if (!allowed(i)) continue;

            float fDist = (float)dist;
            if (fDist <= reach) {
                weights[i] = 1.0f;
            } else if (fDist < reach + 1.0f) {
                weights[i] = reach - (fDist - 1.0f); // partial weight for the leading edge
            }
        }
        total += weights[i];
    }
    
    if (total <= 1e-6f) return baseIdx;
    
    float acc = 0.f;
    float roll = r * total;
    for (int i = 0; i < 8; ++i) {
        if (weights[i] > 0.f) {
            acc += weights[i];
            if (roll <= acc) return i;
        }
    }
    return baseIdx;
}

// Regenerate rhythm pattern (16 steps of bool: true=active, false=rest)
void PatternEngine::redrawRhythm(const PatternInput& in) {
    if (in.locked && !in.diceLiveR) return;   // LOCK_SCOPE_MENU: rhythm dice may draw under lock if opted live

    // B is committed into A FIRST, then a fresh B is drawn, and slew blends A↔B
    // at the roll. A walks forward each roll → groove mutates; low slew = tight
    // variations near the evolving A, slew=1 = full replace (MeloDicer mode).
    // Slew still blends at the roll, so the user auditions candidates against the
    // same anchor A (raise slew to move toward B, lower to fall back to A).
    // First draw (or post-seed): A := B := draw, so effective == draw at any slew.
    const bool first = rhythmFirstDraw;
    rhythmFirstDraw = false;

    // Philox addressable draw bookkeeping. A fresh seed (first) draws chunk 0. Forward
    // → index +1; a reversible stream under backward phase → index -1 (no floor —
    // negative indices are valid reproducible draws). Cursor resets to map unit() calls
    // to this draw's chunk base.
    if (!first) advanceRhythmDraw(rhythmDrawDir());
    beginRhythmDraw();

    // Dice roll = advance the counter (done above). The effective pattern is re-derived from the
    // counter by recomputeEffectiveRhythm (scrub/B2 window) -- no stored A/B walk. (Step 4a removed
    // the old promote-A<-B + step()=a+slew*(unit()-a) population; slew is now the FIR smoothing
    // width in patternRhythmAt, not a roll-time B-shaping step.)
    (void)first;   // first-draw no longer special-cased: infinite-line, patternAt(N) is self-contained
    rhythmSlewApplied = -1.f;       // force recompute of slewedDraw
    recomputeEffectiveRhythm();
    // Cache source from the SLEWED draw (canonical draw; final may be rewritten
    // by Sands later this cycle). UI pattern previews the no-Sands final.
    for (int i = 0; i < 16; ++i) {
        rhythmSource[i]=slewedRhythm[i]; variationSource[i]=slewedVariation[i];
        legatoSource[i]=slewedLegato[i]; accentSource[i]=slewedAccent[i];
        for (int v=0;v<15;v++) polyRhythmSource[v][i]=slewedPolyRhythm[v][i];
        for (int v=0;v<15;v++) polyAccentSource[v][i]=slewedPolyAccent[v][i];
        rhythmPattern[i] = (slewedRhythm[i] >= in.restProb);
    }
}

// slew: slewedDraw[] = A + slew*(B-A). When no Sands owns the spread stage,
// copy slewedDraw → final (the public arrays the sequencer reads).
void PatternEngine::recomputeEffectiveRhythm() {
    // SCRUB + B2 slew. effective = blend of two B2-smoothed window patterns at the scrub position.
    // s in [0..6] (MIX repurposed; 0..1 scaled to 0..6 until step 5). patternRhythmAt(M,slew) is a
    // geometric MA of raw draws M..M-6 -> correlated walk, pure fn of M (reversible). slew =
    // smoothing width. Detent is widget-only; math reads raw scrub so CV stays smooth.
    const float s = rack::math::clamp(rhythmMixLatched, 0.f, 1.f) * 6.f;
    const int   f    = (int)s;
    const float frac = s - (float)f;
    const int64_t N  = rhythmDrawCtr;
    const float slew = rack::math::clamp(rhythmSlewLatched, 0.f, 1.f);
    RhythmDraw d0, d1;
    patternRhythmAt(N - f,     slew, d0);
    patternRhythmAt(N - f - 1, slew, d1);
    auto bl = [frac](float a, float b){ return a + frac*(b-a); };
    for (int i = 0; i < 16; ++i) {
        slewedRhythm[i]=bl(d0.rhythm[i],d1.rhythm[i]);
        slewedVariation[i]=bl(d0.variation[i],d1.variation[i]);
        slewedLegato[i]=bl(d0.legato[i],d1.legato[i]);
        slewedAccent[i]=bl(d0.accent[i],d1.accent[i]);
        for (int v = 0; v < 15; v++) {
            slewedPolyRhythm[v][i]=bl(d0.polyRhythm[v][i],d1.polyRhythm[v][i]);
            slewedPolyAccent[v][i]=bl(d0.polyAccent[v][i],d1.polyAccent[v][i]);
        }
    }
    if (!sandsActive) {
        for (int i = 0; i < 16; ++i) {
            rhythmRandom[i]=slewedRhythm[i]; variationRandom[i]=slewedVariation[i];
            legatoRandom[i]=slewedLegato[i]; accentRandom[i]=slewedAccent[i];
            for (int v=0;v<15;v++) polyRandom(v, PL_REST)[i]=slewedPolyRhythm[v][i];
            // BUG FIX: poly accent was NOT promoted here (only rhythm was), so in the non-sands
            // path polyAccentRandom never received its slewed random values — it stayed at its
            // init value (0 → all notes accent; or 1.0 after the init-seed fix → no notes
            // accent). This is the real root cause; the init seed only changed which stuck
            // value showed. Mirror the rhythm promotion. (sandsActive path already sets both via
            // SpreadInterp at MonsoonSandsManager 460/463.)
            for (int v=0;v<15;v++) polyRandom(v, PL_ACCENT)[i]=slewedPolyAccent[v][i];
        }
    }
    rhythmMixApplied = rhythmMixLatched; rhythmSlewApplied = slew; rhythmCtrApplied = N;
}

void PatternEngine::recomputeEffectiveMelody() {
    // SCRUB + B2 slew -- mirror of recomputeEffectiveRhythm.
    const float s = rack::math::clamp(melodyMixLatched, 0.f, 1.f) * 6.f;
    const int   f    = (int)s;
    const float frac = s - (float)f;
    const int64_t N  = melodyDrawCtr;
    const float slew = rack::math::clamp(melodySlewLatched, 0.f, 1.f);
    MelodyDraw d0, d1;
    patternMelodyAt(N - f,     slew, d0);
    patternMelodyAt(N - f - 1, slew, d1);
    auto bl = [frac](float a, float b){ return a + frac*(b-a); };
    for (int i = 0; i < 16; ++i) {
        slewedMelody[i]=bl(d0.melody[i],d1.melody[i]);
        slewedOctave[i]=bl(d0.octave[i],d1.octave[i]);
        for (int v = 0; v < 15; v++) {
            slewedPolyMelody[v][i]=bl(d0.polyMelody[v][i],d1.polyMelody[v][i]);
            slewedPolyOctave[v][i]=bl(d0.polyOctave[v][i],d1.polyOctave[v][i]);
        }
    }
    if (!sandsActive) {
        for (int i = 0; i < 16; ++i) {
            melodyRandom[i]=slewedMelody[i]; octaveRandom[i]=slewedOctave[i];
            for (int v=0;v<15;v++){ polyRandom(v, PL_MELODY)[i]=slewedPolyMelody[v][i];
                                    polyRandom(v, PL_OCTAVE)[i]=slewedPolyOctave[v][i]; }
        }
    }
    melodyMixApplied = melodyMixLatched; melodySlewApplied = slew; melodyCtrApplied = N;
}

// Regenerate melody pattern (16 steps of semitone + pitch voltage)
void PatternEngine::redrawMelody(const PatternInput& in) {
    if (in.locked && !in.diceLiveM) return;   // LOCK_SCOPE_MENU: melody dice may draw under lock if opted live
    const bool first = melodyFirstDraw;
    melodyFirstDraw = false;

    // Philox addressable draw bookkeeping (mirror of redrawRhythm).
    if (!first) advanceMelodyDraw(melodyDrawDir());
    beginMelodyDraw();

    // Dice roll = advance the counter (done above); effective pattern re-derived by
    // recomputeEffectiveMelody (scrub/B2). No stored A/B walk. (Step 4a.)
    (void)first;
    melodySlewApplied = -1.f;
    recomputeEffectiveMelody();
    for (int i = 0; i < 16; ++i) {
        melodySource[i]=slewedMelody[i]; octaveSource[i]=slewedOctave[i];
        for (int v=0;v<15;v++){ polyMelodySource[v][i]=slewedPolyMelody[v][i];
                                polyOctaveSource[v][i]=slewedPolyOctave[v][i]; }
        int sem = 0;
        melodyPitchV[i]   = genPitchLive(sem, in, slewedMelody[i], slewedOctave[i]);
        melodySemitone[i] = sem;
    }
}

void PatternEngine::latchMix(float rhythmMix, float melodyMix, float rhythmSlew, float melodySlew,
                             bool applyRhythm, bool applyMelody) {
    // Sample the live SCRUB inputs (control rate). Under the scrub model BOTH the scrub position
    // (MIX repurposed) AND slew (FIR smoothing width) feed the effective pattern, so latch both and
    // recompute a stream when EITHER changed. Recompute is cheap + change-guarded.
    // LOCK_SCOPE_MENU: applyRhythm/applyMelody gate EACH stream independently. A frozen stream (apply
    // false) does NOT re-latch its mix/slew and does NOT recompute — its latched value + effective
    // pattern hold from before the lock. The two streams are fully independent here, so a per-axis
    // A/B scope freeze is exact (no combined coupling). Default true/true = latch both (unlocked).
    if (applyRhythm) {
        rhythmMixLatched  = rhythmMix;
        rhythmSlewLatched = rhythmSlew;
        if (rhythmMixLatched != rhythmMixApplied || rhythmSlewLatched != rhythmSlewApplied)
            recomputeEffectiveRhythm();
    }
    if (applyMelody) {
        melodyMixLatched  = melodyMix;
        melodySlewLatched = melodySlew;
        if (melodyMixLatched != melodyMixApplied || melodySlewLatched != melodySlewApplied)
            recomputeEffectiveMelody();
    }
}

// Updates the rhythm/melody arrays used for UI and LEDs based on the 
// current knob positions and the *existing* random buffers.
void PatternEngine::refreshVisualCache(const PatternInput& in) {
    // No-redraw path (e.g. mix/scrub moved but no roll, or a stream that didn't redraw this cycle).
    // Under the scrub model the effective pattern is a pure function of (counter, scrub position), so
    // re-derive it here too -- otherwise rhythmRandom/melodyRandom keep whatever a PRIOR redraw left
    // and the no-redraw pattern diverges from an equivalent redraw at the same counter. Recomputing
    // makes the two paths converge (a seeded engine that never rolled and one that rolled to the same
    // counter now read identically). Cheap: it's just the window blend.
    // Recompute ONLY when this stream's scrub inputs changed since the last recompute (mix, slew,
    // or counter). Avoids re-deriving the full K-window every ~90Hz refresh when nothing moved.
    {
        const float rSlew = rack::math::clamp(rhythmSlewLatched, 0.f, 1.f);
        if (rhythmMixLatched != rhythmMixApplied || rSlew != rhythmSlewApplied || rhythmDrawCtr != rhythmCtrApplied)
            recomputeEffectiveRhythm();
        const float mSlew = rack::math::clamp(melodySlewLatched, 0.f, 1.f);
        if (melodyMixLatched != melodyMixApplied || mSlew != melodySlewApplied || melodyDrawCtr != melodyCtrApplied)
            recomputeEffectiveMelody();
    }
    for (int i = 0; i < 16; ++i) {
        rhythmPattern[i] = (rhythmRandom[i] >= in.restProb);
        int sem = 0;
        melodyPitchV[i]   = genPitchLive(sem, in, melodyRandom[i], octaveRandom[i]);
        melodySemitone[i] = sem;
    }
}

// Apply any pending seeds, then redraw both patterns.
// Called at phrase boundaries and on reset.
void PatternEngine::applyPendingSeedsAndRedraw(const PatternInput& in) {
    // Whole-module lock freezes everything (seeds, RNG, patterns) — UNLESS a dice stream is opted
    // live under lock (LOCK_SCOPE_MENU §6). Then that stream's ROLL / live-mode reroll may still draw,
    // while seeds/reseed-rolls stay frozen (those are Reseed-scoped). If locked and NEITHER dice
    // stream is live, nothing to do — early-out preserves the pre-menu behaviour exactly.
    if (in.locked && !in.diceLiveR && !in.diceLiveM) return;
    // Per-stream "may this stream draw now?": unlocked, OR this stream's dice bit is opted live.
    const bool drawAllowedR = !in.locked || in.diceLiveR;
    const bool drawAllowedM = !in.locked || in.diceLiveM;

    // ── Dice-undo capture (item 4): a USER ROLL is exactly rhythmRollPending / melodyRollPending
    // at THIS commit. Realtime-mode auto-redraw sets neither (it uses rhythmMode==1); reset/reseed
    // use *SeedPending / *ReseedRollPending — all correctly EXCLUDED per the scope ruling. Capture
    // the roll intent NOW, before the pending flags are cleared below, and record before/after
    // (seedFloat, counter) around each moved stream's redraw. Published by Monsoon::onPhraseBoundary_.
    const bool undoR = rhythmRollPending;
    const bool undoM = melodyRollPending;
    if (undoR || undoM) {
        diceUndoPending.valid  = true;
        diceUndoPending.movedR = undoR;
        diceUndoPending.movedM = undoM;
        diceUndoPending.rSeedBefore = rhythmSeedFloat; diceUndoPending.rCtrBefore = rhythmDrawCtr;
        diceUndoPending.mSeedBefore = melodySeedFloat; diceUndoPending.mCtrBefore = melodyDrawCtr;
    }

    // Redraw if: a seed is pending (reproducible reseed, A=B), a ROLL is pending
    // (advance RNG, no reseed), a TRIAL is pending (audition, A anchored, never
    // reseeds), a RESEED-ROLL is pending (main roll + fresh entropy, keeps A/B
    // morph), or Realtime mode.
    // Under lock (dice-live), only the ROLL and the live-mode per-cycle reroll may draw; seeds and
    // reseed-rolls stay frozen (Reseed control, separately scoped — they won't even be armed under
    // lock, but guard for safety). Unlocked, all terms apply as before. drawAllowedR/M gates the whole
    // stream (false => this stream stays fully frozen, matching the old blanket early-return).
    bool shouldRedrawR = drawAllowedR && (((!in.locked) && (rhythmSeedPending || rhythmReseedRollPending))
                                          || rhythmRollPending || (rhythmMode == 1));
    bool shouldRedrawM = drawAllowedM && (((!in.locked) && (melodySeedPending || melodyReseedRollPending))
                                          || melodyRollPending || (melodyMode == 1));

    if (!in.locked && rhythmSeedPending) {
        rhythmSeedFloat = rhythmSeedPendingFloat;
        seedRhythmPhilox(rhythmSeedFloat);     // mirror seed into Philox (counter→0)
        rhythmSeedPending = false;
        rhythmFirstDraw = true;   // new seed → A=B=draw, reproducible at any slew
    } else if (rhythmReseedRollPending) {
        // Reseed WITHOUT firstDraw — redrawRhythm(promote=true) commits B→A then
        // draws fresh B from the reseeded stream, so A≠B and slew survives.
        if (rhythmReseedRollFull) { seedRhythmPhiloxFull(); }
        else { rhythmSeedFloat = rhythmReseedRollFloat; seedRhythmPhilox(rhythmSeedFloat); }
    }
    // Only CONSUME (clear) the roll flags if this stream was allowed to draw. A frozen stream under a
    // mixed scope (e.g. rhythm live, melody frozen) must KEEP its pending flag so the queued roll fires
    // at unlock — clearing it unconditionally would silently DROP the roll (and blank its dice light).
    if (drawAllowedR) { rhythmRollPending = false; rhythmReseedRollPending = false; }
    if (shouldRedrawR) redrawRhythm(in);
    if (drawAllowedR) rhythmPendingLast = false;   // one-shot: consumed only if this stream drew (held otherwise)

    if (!in.locked && melodySeedPending) {   // seeds stay frozen under lock (Reseed-scoped)
        melodySeedFloat = melodySeedPendingFloat;
        seedMelodyPhilox(melodySeedFloat);     // mirror seed into Philox (counter→0)
        melodySeedPending = false;
        melodyFirstDraw = true;
    } else if (!in.locked && melodyReseedRollPending) {
        if (melodyReseedRollFull) { seedMelodyPhiloxFull(); }
        else { melodySeedFloat = melodyReseedRollFloat; seedMelodyPhilox(melodySeedFloat); }
    }
    // Only consume the melody roll flags if melody was allowed to draw (see rhythm note above).
    if (drawAllowedM) { melodyRollPending = false; melodyReseedRollPending = false; }
    if (shouldRedrawM) redrawMelody(in);
    if (drawAllowedM) melodyPendingLast = false;   // one-shot: consumed only if this stream drew (held otherwise)

    // ── Dice-undo capture (item 4): record the AFTER (seedFloat, counter) now that the roll's
    // redraw has advanced the counter. Only the moved streams matter; the other's before==after.
    if (diceUndoPending.valid) {
        diceUndoPending.rSeedAfter = rhythmSeedFloat; diceUndoPending.rCtrAfter = rhythmDrawCtr;
        diceUndoPending.mSeedAfter = melodySeedFloat; diceUndoPending.mCtrAfter = melodyDrawCtr;
        // diceUndoPending stays valid=true until the owner (Monsoon::onPhraseBoundary_) drains it.
    }

    // Always refresh the cache so the LEDs react to live knob changes in Dice mode
    if (!shouldRedrawR || !shouldRedrawM) refreshVisualCache(in);
}

// ── Mode switching (dice ↔ realtime) ──────────────────────────────────────
// stepIndex / lastStepIndex passed in+out so the engine can cache/restore them.

void PatternEngine::switchMelodyMode(int& stepIndex, int& lastStepIndex) {
    int prev = melodyMode;
    int next = 1 - prev;
    if (prev == 0 && next == 1) {
        // Entering realtime: cache current state (seed + output + A/B buffers)
        cachedMelodySeedFloat = melodySeedPending
            ? melodySeedPendingFloat : melodySeedFloat;
        melodySeedCached = true;
        for (int i = 0; i < 16; ++i) cachedMelodyPitchV[i] = melodyPitchV[i];
        cachedMelodyStepIndex     = stepIndex;
        cachedMelodyLastStepIndex = lastStepIndex;
        // Scrub model: no A/B snapshot -- (counter, scrub, slew) is the morph position,
        // unchanged by a mode switch; returning to dice recomputes from the counter. (Step 4b.)
    } else if (prev == 1 && next == 0 && melodySeedCached) {
        // Returning to dice: restore the exact A/B buffers (lossless — preserves
        // the slew morph position), NOT a reseed-to-A=B approximation.
        for (int i = 0; i < 16; ++i) melodyPitchV[i] = cachedMelodyPitchV[i];
        stepIndex     = cachedMelodyStepIndex;
        lastStepIndex = cachedMelodyLastStepIndex;
        // Scrub model: recompute effective from the (unchanged) counter -- exact pre-switch pattern.
        recomputeEffectiveMelody();
    }
    melodyMode = next;
}

void PatternEngine::switchRhythmMode(int& stepIndex, int& lastStepIndex) {
    int prev = rhythmMode;
    int next = 1 - prev;
    if (prev == 0 && next == 1) {
        cachedRhythmSeedFloat = rhythmSeedPending
            ? rhythmSeedPendingFloat : rhythmSeedFloat;
        rhythmSeedCached = true;
        for (int i = 0; i < 16; ++i) cachedRhythmPattern[i] = rhythmPattern[i];
        cachedRhythmStepIndex     = stepIndex;
        cachedRhythmLastStepIndex = lastStepIndex;
        // Scrub model: no A/B snapshot -- (counter, scrub, slew) is the morph position,
        // unchanged by a mode switch; returning to dice recomputes from the counter. (Step 4b.)
    } else if (prev == 1 && next == 0 && rhythmSeedCached) {
        for (int i = 0; i < 16; ++i) rhythmPattern[i] = cachedRhythmPattern[i];
        stepIndex     = cachedRhythmStepIndex;
        lastStepIndex = cachedRhythmLastStepIndex;
        // Scrub model: recompute effective from the (unchanged) counter -- exact pre-switch pattern.
        recomputeEffectiveRhythm();
    }
    rhythmMode = next;
}

// ──── Composite Operations ──────────────────────────────────────────────
