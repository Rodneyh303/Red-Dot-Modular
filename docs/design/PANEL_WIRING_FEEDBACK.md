# In-Rack render feedback (Rodney, end of session) — worklist

## BUGS (priority — real wiring defects, not cosmetics)
1. **Straits voice-1 knobs NOT locked to Monsoon mono rest/accent.** param_rest_0/accent_0 were
   bound to REST_PARAM/ACCENT_PARAM but in Rack they don't track. Likely: binding a param that
   the parent module OWNS from a child expander doesn't share state — the expander has its own
   param store. Voice-1 knob must MIRROR/READ the parent's mono value, not bind a local param.
   Fix direction: voice-1 rest/accent on Straits should either (a) read the parent Monsoon param
   live (display-only, no independent knob), or (b) push its value onto the parent. Decide which.
2. **Causeway channel-1 (mono) modulation not reaching Monsoon rest/accent.** MONO_*_MOD_ATT +
   getEffectiveMonoRest/Accent were added, applied at getRestParam/getAccentParam + engine.accentProb.
   But no effect in Rack. Check: (i) is the mono REST prob actually consumed via getRestParam, or a
   different path? (ii) is cachedCausewayPolyExpander populated? (iii) is CV ch0 the right channel?

## PANELS (after bugs)
- **Changi = worst.** Jewel removed by Rodney (was bad). RESTART from the provided concept art:
  make it look like a proper AIRPORT first (gold taxiway/apron linework, control tower, planes,
  runway markings, apron geometry — dense like the concept), THEN add a Changi-specific touch.
  Do NOT re-add the vortex as-is. Airport-first, Changi-second.
- **Straits waves too sparse.** Increase wave line density/count substantially.
- **Causeway + Changi too simplistic.** Add detail. Raster quality is out of reach but we can do
  much better — denser linework, layering, depth cues.
- **Causeway knobs aligning with the truss design is GOOD (like Interchange)** — keep/extend that.

## Direction going forward (Rodney)
Mostly BUG FIX + PANELS from here. Major refactoring / big fixes believed largely DONE.
