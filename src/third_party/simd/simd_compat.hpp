#pragma once
// simd_compat.hpp — one include for SSE intrinsics across x86 and arm64.
//
// Uses the Rack SDK's bundled SIMDe (dep/include/simde/x86/sse4.2.h). SIMDe provides
// the full SSE intrinsic set: native pass-through on x86, NEON emulation on arm64, so
// the SAME SSE source compiles and runs on both archs.
//
// SIMDE_ENABLE_NATIVE_ALIASES makes SIMDe expose the BARE names (__m128i, _mm_*),
// not just its simde-prefixed forms — so code written in plain _mm_* intrinsics
// works unchanged. (On x86 these resolve to native; on arm64, to SIMDe's NEON impls.)
//
// We do NOT vendor sse2neon — it conflicts with Rack's SIMDe (__m64 redefinition).
//
// Philox uses INTEGER SSE ops only (mul/xor/add/shift/shuffle/blend), bit-exact across
// native-SSE and SIMDe-NEON -> the arm64 path is bit-identical.
//
// Usage: include this, then use _mm_* intrinsics. Guard the kernel with
// #if defined(SIMD_HAVE_SSE).

#ifndef SIMDE_ENABLE_NATIVE_ALIASES
#define SIMDE_ENABLE_NATIVE_ALIASES
#endif
#include <simde/x86/sse4.2.h>
#define SIMD_HAVE_SSE 1
