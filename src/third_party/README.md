# Third-party / SIMD compat

## src/third_party/simd/simd_compat.hpp
Thin wrapper that includes the Rack SDK's bundled **SIMDe** (`<simde/x86/sse4.2.h>`)
and defines `SIMD_HAVE_SSE`. SIMDe provides the full SSE intrinsic set (native
pass-through on x86, NEON emulation on arm64), so the SIMD Philox kernel — written
once in SSE `_mm_*` intrinsics — compiles and runs on both x86 and arm64.

We do NOT vendor sse2neon: it conflicts with Rack's already-included SIMDe (both
define `__m64` etc.). SIMDe ships with the Rack SDK (dep/include/simde/) and Rack
itself includes it (include/simd/common.hpp), so there is no added dependency.
