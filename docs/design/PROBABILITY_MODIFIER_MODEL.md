
## Open question (revisit): per-term vs end clamping of effective spread

SpreadResolver::effective() currently clamps AFTER EACH additive term (own CV, East CV, Macro send),
because that exactly matches the manager code it replaces — the migration is behaviour-inert by
construction. But step-wise clamping is probably NOT the best behaviour: an intermediate term that
overflows ±1 is pinned before a later term can pull it back, so the order of contributions changes the
result and a large-then-opposite pair loses information (e.g. base 0.9 +own 0.4 −east 0.6 → 0.4 under
step-wise, but 0.7 under a single end-clamp — the end-clamp preserves the intended net). A single
end-clamp is order-independent and more musically faithful. Revisit as a deliberate BEHAVIOUR change
(own commit, not folded into a migration): flip effective() to sum-then-clamp once, update the resolver
test's "STEP-WISE clamp" case, and build-verify the audible difference is acceptable. Low risk mechanically
(one function), but it IS a behaviour change so it needs Rodney's sign-off + a Rack listen.
