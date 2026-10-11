# Third-party vendored code

## sse2neon (src/third_party/sse2neon/)
- Upstream: https://github.com/DLTcollab/sse2neon  (v1.9.1)
- License: MIT — see sse2neon/LICENSE (Copyright (c) 2015-2026 SSE2NEON Contributors)
- Purpose: translates Intel SSE intrinsics to ARM NEON at compile time, so the
  SIMD Philox kernel (written once in SSE) compiles + runs on arm64 (Apple Silicon
  Mac slice). Header-only; integer-only ops -> bit-identical to native SSE.
- Include via src/third_party/sse2neon/simd_compat.hpp (picks native SSE on x86,
  sse2neon on arm64).
