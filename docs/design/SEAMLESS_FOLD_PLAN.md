# Seamless fold plan: one resizable module, poly-packed jacks

Status: PLAN (2026-11-21). Builds on SEAMLESS_SUBPANEL_IDEA.md and
MODULE_DISCOVERY_AND_VARIABLE_WIDTH.md. Rodney's rulings are marked (R).

## Rulings

- (R) True resizable fold: ONE ModuleWidget grows (`box.size.x`), not abutting modules.
- (R) The OLD modules are NOT deleted. The folded module is a NEW model alongside them,
  so existing patches keep loading unchanged. No cable-rewrite migration is required for
  old patches; any migrator is an optional convenience for later.
- (R) Try the jack reduction by POLY INPUTS. It only works where inputs are mono across
  voices. Sands East's jacks are already poly across voices, so they cannot be packed
  again. Sands Macro's inputs are mono across voices, so they can.

## Port inventory (read from code 2026-11-21)

| Panel | Ports | Notes |
|---|---|---|
| Sands Macro (`StraitsSandsMacroVisual`) | 39 in, 7 out, 0 params | 28 CV (7 lanes x LEN/OFF/ROT/SPR), 7 dir gate-mod, 3 voice-range L/O/R CV + 1 dir gate-mod. Outs: 7 prob CV (MEL OCT QMIX REST ACC VAR LEG). |
| Distribution (`SandsDistributionExpander`) | 56 in, 0 params | 7 lanes x 8 params. Read at channel 0 only (`getVoltage()`). Ids FROZEN: 0..41 = lane*6+p, 42..55 = gap jacks. The `42` in the header comment is stale. |
| Sends (`SandsMacroSendSubpanel`) | 0 | 98 store-backed knobs. |
| Sands East | already poly across voices | NOT packable. Stays as is. |

## Packing proposal (Macro and Distribution only)

Channel index = lane (MEL..LEG, the unified lane order), so one cable carries a column.

- Macro CV: 28 mono -> 4 poly inputs (LEN, OFF, ROT, SPR), 7 channels each.
- Macro dir gate-mod: 7 mono -> 1 poly input, 7 channels.
- Macro voice-range: 3 CV + 1 dir stay mono (4 jacks) or pack to 1 poly with 4 channels. Decide at panel time.
- Distribution mod: 56 mono -> 8 poly inputs (one per param column), 7 channels each.
- Macro outputs (7 prob CV) are already one signal per lane; leave them.

Result: 95 inputs -> about 14 poly inputs. Unpatched channels read 0, same as an unpatched
mono jack today. A poly jack with fewer than 7 channels leaves the remaining lanes at 0.

Cost to name: you lose per-cell cable colour and per-jack tooltips, and a single cable
now drives several lanes. Mono patching of one lane needs a splitter or a poly-merge.
Offer a context-menu option to keep the full mono jack layout in the folded module
(superset port declaration, one visible layout) only if Rodney wants it.

## Rack constraints that shape it

- `config()` runs once per module, so the folded model declares ALL ports for every
  band up front (the repo already does this with the unreachable SG_MOD slots).
- A port with no widget cannot hold a cable. Collapsing a band with cabled jacks must be
  blocked or warned.
- Port ids must stay stable once shipped (Rack persists cables by port index). Assign the
  folded model's ids once, in a frozen enum with a comment, like Distribution's.

## Build order (each step shippable)

1. **Resize spike.** Runtime `box.size.x`, width saved in the patch, neighbour push
   (`setModulePosForce`), undo step, background that scales. Target: Sends band first
   (no ports), on a NEW folded model, old modules untouched.
2. **Seamless draw.** Titled bands keep the subpanel names; no seam.
3. **Distribution band** with the 8 packed poly inputs.
4. **Macro band** with the 4+1(+1) packed inputs and the 7 outputs.
5. **Loadout slots** (SEAMLESS_SUBPANEL_IDEA.md): exclusive vs additive.
6. Optional: a converter that rewrites old cables into the folded model's packed ports.
   Not needed for compatibility, because the old modules stay.

## Open questions

- Is `Distribution` ever read per voice in its East role? Today it reads channel 0 only,
  so packing is safe now. Confirm before freezing ids.
- Keep voice-range jacks mono or pack? Panel decision.
- Does the folded model reuse the existing store (`editor.*`) directly? Expected yes,
  since every control is already store-backed with no host params.
- Scope for East and Straits folds: separate plans. East is excluded from packing.
