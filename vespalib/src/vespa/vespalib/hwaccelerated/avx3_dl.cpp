// Copyright Vespa.ai. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.

#include "avx3_dl.h"
#include "avxprivate.hpp"
#include "x64_generic.h"
#include <immintrin.h>

namespace vespalib::hwaccelerated {

float
Avx3DlAccelerator::dotProduct(const float* af, const float* bf, size_t sz) const noexcept {
    return avx::dotProductSelectAlignment<float, 64>(af, bf, sz);
}

double
Avx3DlAccelerator::dotProduct(const double* af, const double* bf, size_t sz) const noexcept {
    return avx::dotProductSelectAlignment<double, 64>(af, bf, sz);
}

namespace {

// 4x manually unrolled AVX-512 vectorized popcount kernel. This is essentially a transplant
// of the unrolled popcount Highway kernel in the WIP `vekterli/highway-experiments` branch,
// but with all higher-level abstractions replaced with their AVX-512 intrinsic equivalents,
// and with the unrolling factor lowered from 8 to 4 to reduce the tedium involved.
// This depends on both VPOPCNTDQ and BMI2 instructions, so it can't be used below AVX3_DL levels.
// One of the main speedup sources is the explicit use of parallel, shared-nothing accumulators
// which avoids data dependency stalls and lets a deep CPU pipeline do its thing. Empirically,
// the compiler auto-vectorizer has a hard time doing these sorts of stunts even though it may
// otherwise be able to vectorize a loop.
size_t
avx512_intrinsics_popcount(const uint64_t* a, size_t sz) noexcept {
    size_t i = 0;
    constexpr size_t N = 512/64; // ==> 8 64-bit lanes per load/popcnt
    // 4 independent accumulators of N 64-bit lanes each
    auto a0 = _mm512_setzero_si512();
    auto a1 = _mm512_setzero_si512();
    auto a2 = _mm512_setzero_si512();
    auto a3 = _mm512_setzero_si512();
    // 4-way unrolled main loop. Use all accumulators in parallel.
    // All loads are _unaligned_, to avoid any alignment requirements. This has
    // close to zero extra cost on modern CPUs compared to the aligned versions.
    for (; (i + 4*N) <= sz;) {
        const auto v0 = _mm512_loadu_epi64(a + i);
        const auto p0 = _mm512_popcnt_epi64(v0);
        a0 = _mm512_add_epi64(p0, a0);
        i += N;
        const auto v1 = _mm512_loadu_epi64(a + i);
        const auto p1 = _mm512_popcnt_epi64(v1);
        a1 = _mm512_add_epi64(p1, a1);
        i += N;
        const auto v2 = _mm512_loadu_epi64(a + i);
        const auto p2 = _mm512_popcnt_epi64(v2);
        a2 = _mm512_add_epi64(p2, a2);
        i += N;
        const auto v3 = _mm512_loadu_epi64(a + i);
        const auto p3 = _mm512_popcnt_epi64(v3);
        a3 = _mm512_add_epi64(p3, a3);
        i += N;
    }
    // Boundary case; _at least_ one full vector remains. Use a single accumulator.
    for (; (i + N) <= sz; i += N) {
        const auto v = _mm512_loadu_epi64(a + i);
        const auto p = _mm512_popcnt_epi64(v);
        a0 = _mm512_add_epi64(p, a0);
    }
    // Boundary case; remaining is < a full vector. Use a single accumulator.
    const size_t rem = sz - i; // Must be < 8 by definition
    if (rem != 0) {
        // Inspired by LoadN/FirstN from Highway.
        // BZHI (from BMI2) returns its first argument with all bits >= rem zeroed out.
        // Example: BZHI(1)=1, BZHI(2)=3, BZHI(3)=7, BZHI(4)=15, BZHI(5)=31 and so on.
        // This is a very convenient way to build a lane-wise mask for a subsequent vector
        // load instruction where lanes not part of the mask are implicitly zeroed out.
        const auto load_mask = static_cast<__mmask8>(_bzhi_u32(~uint32_t{0}, rem));
        const auto v = _mm512_maskz_loadu_epi64(load_mask, a + i); // Does not touch memory OOB
        const auto p = _mm512_popcnt_epi64(v);
        a0 = _mm512_add_epi64(p, a0);
    }
    // Pairwise reduce all accumulators down to one, then reduce across its lanes
    a0 = _mm512_add_epi64(a0, a1);
    a2 = _mm512_add_epi64(a2, a3);
    a0 = _mm512_add_epi64(a0, a2);
    return _mm512_reduce_add_epi64(a0);
}

} // anon ns

size_t
Avx3DlAccelerator::populationCount(const uint64_t* a, size_t sz) const noexcept {
    // TODO fallback to X64GenericAccelerator::populationCount for vectors <= 16 elems;
    //  it's too expensive to fire up the AVX-512 pipeline steam engines for short vectors.
    //  But first, use _only_ vectorized code to observe impact on CPU power licenses...!
    return avx512_intrinsics_popcount(a, sz);
}

double
Avx3DlAccelerator::squaredEuclideanDistance(const int8_t* a, const int8_t* b, size_t sz) const noexcept {
    return helper::squaredEuclideanDistance(a, b, sz);
}

double
Avx3DlAccelerator::squaredEuclideanDistance(const float* a, const float* b, size_t sz) const noexcept {
    return avx::euclideanDistanceSelectAlignment<float, 64>(a, b, sz);
}

double
Avx3DlAccelerator::squaredEuclideanDistance(const double* a, const double* b, size_t sz) const noexcept {
    return avx::euclideanDistanceSelectAlignment<double, 64>(a, b, sz);
}

void
Avx3DlAccelerator::and128(size_t offset, const std::vector<std::pair<const void*, bool>>& src, void* dest) const noexcept {
    helper::andChunks<64, 2>(offset, src, dest);
}

void
Avx3DlAccelerator::or128(size_t offset, const std::vector<std::pair<const void*, bool>>& src, void* dest) const noexcept {
    helper::orChunks<64, 2>(offset, src, dest);
}

void
Avx3DlAccelerator::convert_bfloat16_to_float(const uint16_t* src, float* dest, size_t sz) const noexcept {
    helper::convert_bfloat16_to_float(src, dest, sz);
}

int64_t
Avx3DlAccelerator::dotProduct(const int8_t* a, const int8_t* b, size_t sz) const noexcept
{
    return helper::multiplyAdd(a, b, sz);
}

}
