# Change Alley reuses Monsoon dice RNG design (single directive)

CA (optional) takes the RNG design and lessons from Monsoon's compulsory dice wholesale. No
bespoke scatter RNG.

- Same generator: Philox4x32-10, shared PhiloxRng, keyed per stream (domain-separated).
- Same counter model: signed int64_t, addressable POSITION -- stream.at(counter), counter++
  forward, counter-- reverse (negative on reverse-scrub), exactly like dice draw counters.
- 2 streams: rhythm + melody (keyed apart like every other Monsoon stream). NOT 8.
- Reverse works the same way as dice reverse (already supported): same mechanism, one pattern.

Supersedes the prior scatter-specific counter design (8 counters / uint64 selector / correlationRng
seed-keying). Build CA reverse by copying the dice pattern, not reinventing it.
