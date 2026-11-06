# PHASE DISTORTION  two connection models, to compare (Rodney)

A: EXPANDER model (§14, adopted for now). B: STANDALONE module model (§13, the comparison build).

| Aspect | A: Expander | B: Standalone module |
|---|---|---|
| Where the transform runs | Inside each Monsoon, one shared primitive | Inside the module (once) |
| Phase path to Monsoon | Source -> Monsoon, ONE hop | Source -> module -> Monsoon, TWO hops |
| Sync between Monsoons | Same hop count for all, distorted or not | Same hop count for all Monsoons (convention: all phase via module) |
| Identity output | Bit-identical to today | Today's output delayed by ONE sample (fixed, not a defect) |
| Parameter storage | Expander store (8 sets) | Module's own controls (8 sets) |
| Shared state | Yes: block-consistency rule needed | No: parameters are local to the module |
| Routing | Each Monsoon: 2 indices (phase, subphase) | Each Monsoon: patch module outputs (channel = jack) |
| Patch rule | None: Monsoons may patch the source directly | MUST: every Monsoon phase comes from the module (not enforced) |
| Monsoon changes | Transform + 2 index params + store reads | Only input/output jacks; no transform |
| Standalone use | Not available | Yes: usable outside the Monsoon network |
| Main risk | Store consistency across readers | A Monsoon patched direct desyncs by one sample |
| Cost | Monsoon CPU per reader | One evaluation per channel, shared |

**What to compare:** sync correctness and patch safety (A is safer), the one-sample latency in B (it's fixed and uniform,
but it's a real offset against any direct path), Monsoon complexity (B is cleaner), and whether you want a standalone
distortion tool (only B gives it).

**Gate note for B:** its identity gate is "today's output delayed by one sample", not "bit-identical to today". Compare
against today's output shifted one sample, so the test is not a false failure.
