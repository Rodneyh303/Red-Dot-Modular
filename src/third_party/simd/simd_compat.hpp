#pragma once
// simd_compat.hpp — one include for SSE intrinsics across x86 and arm64.
//
// Uses the Rack SDK's bundled SIMDe (dep/include/simde/x86/sse4.2.h), which Rack
// itself already includes (include/simd/common.hpp). SIMDe provides the full SSE
// intrinsic set (through SSE4.2, incl. SSE4.1's _mm_mullo_epi32 etc.): native
// pass-through on x86, NEON emulation on arm64. So the SAME SSE source compiles
// and runs on both archs.
//
// We do NOT vendor sse2neon — it conflicts with Rack's SIMDe (both define __m64
// etc. -> "typedef redefinition"). SIMDe is the SDK's chosen SSE->NEON shim; use it.
//
// Philox uses INTEGER SSE ops only (mul/xor/add/shift/shuffle/blend), which are
// bit-exact across native-SSE and SIMDe-NEON -> the arm64 path is bit-identical.
//
// Usage: include this, then use _mm_* intrinsics. Guard the kernel with
// #if defined(SIMD_HAVE_SSE).

#include <simde/x86/sse4.2.h>   // full SSE..SSE4.2, native on x86 / NEON on arm64
#define SIMD_HAVE_SSE 1
