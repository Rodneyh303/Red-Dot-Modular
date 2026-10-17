# CLAUDE.md — auto-loaded context for Claude Code

This is dot.modular / Monsoon: a generative polyphonic sequencer for VCV Rack v2 (C++17),
built on a correlational-randomness engine (a reversible Gaussian copula). The repo's docs
ARE the durable memory; the chat is just the reasoning that produced them.

## READ FIRST
- `docs/design/CHAT_HANDOFF.md` — the primer: what the instrument is, the working model,
  current state, the active feature, and the hard-won principles. Read it before doing
  anything. It routes you to the current-focus design docs.
- When given a task prompt in `docs/design/CC_PROMPT_*.md`, that prompt lists its own specs.

## NON-NEGOTIABLE RULES
- VERIFY AGAINST CODE AND TESTS, never a plausible narrative. A lying abstraction plus
  confident-but-wrong "fixed" claims cost this project weeks. If you fix behaviour, prove it
  with a test that FAILS before and PASSES after. When in doubt, read the code.
- You CANNOT verify Rack/behavioural things (no Rack here). Give yourself TESTED, MECHANICAL
  tasks; say "needs Rack feel-test" for anything you can't run. The human feel-tests in Rack.
- Branch discipline: `lane-expander-refactor` is the live branch (NOT master). Cut feature/fix
  branches off it and open PRs INTO it. Never push straight to master.

## BUILD + TEST
- Unit tests (pure, no Rack): `bash test/run_all.sh` (all) or `bash test/run_all.sh <name>`
  (one). `test/run_all.sh` is the source of truth for compile lines + the TESTS[] registry —
  add new tests there; don't trust per-file "Compile:" header comments.
- Panel geometry: `python3 test/audit_anchor_bind.py` — asserts every generated panel anchor
  has a matching widget bind and the counts match the lane constants. Runs in CI via run_all.
- Full plugin build (needs Rack SDK, usually the human's job): `make RACK_DIR=/path/to/Rack-SDK`.

## NAMING
Modules are named after Singapore geography (Straits, Causeway, Raffles, Colonnades,
Shophouse, Changi, Keppel, Fullerton, Sikit, Monsoon). Lanes: MELODY/OCTAVE/QMIX/REST/
ACCENT/VARIATION/LEGATO.
