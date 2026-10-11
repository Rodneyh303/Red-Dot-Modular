// test_sse2neon_compat.cpp — confirms simd_compat.hpp includes cleanly and a
// representative Philox-style integer SSE intrinsic works, on x86 (native SSE)
// AND arm64 (SIMDe -> NEON). Not a Philox test — just proves the SIMD compat
// layer compiles and produces the expected integer result on this platform.
#include <cstdint>
#include <cstdio>
#include "../src/third_party/simd/simd_compat.hpp"

int main() {
#if defined(SIMD_HAVE_SSE)
    // _mm_mul_epu32: 32x32->64 unsigned multiply of lanes 0 and 2 — core Philox op.
    __m128i a = _mm_set_epi32(0, 0xD2511F53u, 0, 7u);
    __m128i b = _mm_set_epi32(0, 3u,          0, 0xCD9E8D57u);
    __m128i p = _mm_mul_epu32(a, b);          // {7*0xCD9E8D57, 0xD2511F53*3}
    uint64_t lo = (uint64_t)_mm_extract_epi32(p,1) << 32 | (uint32_t)_mm_extract_epi32(p,0);
    uint64_t hi = (uint64_t)_mm_extract_epi32(p,3) << 32 | (uint32_t)_mm_extract_epi32(p,2);
    uint64_t exp_lo = (uint64_t)7u * 0xCD9E8D57ull;
    uint64_t exp_hi = (uint64_t)0xD2511F53ull * 3ull;
    bool ok = (lo == exp_lo) && (hi == exp_hi);
  #if defined(__aarch64__) || defined(_M_ARM64)
    printf("SIMDe (arm64 -> NEON): _mm_mul_epu32 %s
", ok ? "OK (bit-exact)" : "MISMATCH");
  #else
    printf("native SSE (x86): _mm_mul_epu32 %s
", ok ? "OK (bit-exact)" : "MISMATCH");
  #endif
    return ok ? 0 : 1;
#else
    printf("no SSE path on this arch — scalar fallback (OK)\n");
    return 0;
#endif
}
