# CI: multi-platform build

`build.yml` builds dot.modular for **Windows / macOS / Linux** on every push to
`main` or `feat/microtonal` (and on PRs, and manually via the Actions tab).
Each platform uploads a `.vcvplugin` artifact you can download from the run page.

This exists to break the "container can't compile the Rack SDK" constraint:
CI compiles in the cloud, so all three platforms get built and tested on every
push instead of Windows-by-hand + Mac/Linux untested.

## TWO THINGS TO VERIFY before it runs green (a human/CC check, post-holiday)

1. **Rack SDK URL/version** (`RACK_SDK_VERSION` in build.yml). Confirm the exact
   latest Rack 2.x SDK version and the zip filename format at
   https://vcvrack.com/downloads . The naming has changed across Rack releases;
   macOS is now a universal `mac-x64+arm64` build. If the URL 404s, this is why.

2. **`-march=native` in the Makefile** (build correctness, not just CI). The
   Makefile has `FLAGS += -flto -O3 -ffast-math -march=native`. `-march=native`
   targets the EXACT CPU of whatever machine compiles it -- fine locally, but a
   **distribution bug**: a binary built on a CI runner (or your machine) can
   crash with "illegal instruction" on a user with an older CPU. The workflow
   overrides it to a portable `-march=x86-64-v2` for CI artifacts, but the
   proper fix is to change the Makefile for release builds (portable baseline,
   or drop -march). Keep -march=native only for local/dev builds if you want it.

## Natural next steps (once the matrix is green)
- **Release job** on git tags (`v1.0.0`): collect all three artifacts into a
  GitHub Release for one-command publishing.
- **Cheaper push check**: full 3-OS matrix on PRs/tags, a single Linux
  compile-check on ordinary pushes, to save runner minutes.
