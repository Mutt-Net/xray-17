// xrSkin2W_AVX2.cpp — AVX2 + FMA3 C++ intrinsic skinning.
//
// This file is compiled with /arch:AVX2 (set as a per-file CMake property).
// The dispatch table only calls these functions when CPUID confirms AVX2 at
// runtime, so they are safe to ship in all build variants.
//
// Gains over SSE2:
//   1W:      2 vertices per iteration (256-bit packs 2×128-bit lanes)
//   2W-4W:   FMA3 (fmadd) reduces 2 instructions (mul+add) to 1 for the
//            weighted accumulation — ~2x math throughput per clock.

#pragma warning(disable: 4752) // "found Intel(R) AVX instructions; consider using /arch:AVX"

#include <immintrin.h>

#include "xrCPU_Pipe.h"
#include "SkeletonXVertRender.h"
#include "bone.h"

// ---------------------------------------------------------------------------
// SSE helpers (used for scalar tail and multi-bone functions)
// ---------------------------------------------------------------------------

static __forceinline __m128 fma_transform_tiny(const Fvector& v, const Fmatrix& M)
{
    __m128 px = _mm_set1_ps(v.x);
    __m128 py = _mm_set1_ps(v.y);
    __m128 pz = _mm_set1_ps(v.z);
    __m128 r0 = _mm_loadu_ps(&M._11);
    __m128 r1 = _mm_loadu_ps(&M._21);
    __m128 r2 = _mm_loadu_ps(&M._31);
    __m128 r3 = _mm_loadu_ps(&M._41);
    // FMA: fmadd(pz, r2, fmadd(py, r1, fmadd(px, r0, r3)))
    return _mm_fmadd_ps(pz, r2, _mm_fmadd_ps(py, r1, _mm_fmadd_ps(px, r0, r3)));
}

static __forceinline __m128 fma_transform_dir(const Fvector& v, const Fmatrix& M)
{
    __m128 px = _mm_set1_ps(v.x);
    __m128 py = _mm_set1_ps(v.y);
    __m128 pz = _mm_set1_ps(v.z);
    __m128 r0 = _mm_loadu_ps(&M._11);
    __m128 r1 = _mm_loadu_ps(&M._21);
    __m128 r2 = _mm_loadu_ps(&M._31);
    return _mm_fmadd_ps(pz, r2, _mm_fmadd_ps(py, r1, _mm_mul_ps(px, r0)));
}

static __forceinline void store3(float* dst, __m128 v)
{
    _mm_store_ss(dst,     v);
    _mm_store_ss(dst + 1, _mm_shuffle_ps(v, v, _MM_SHUFFLE(1, 1, 1, 1)));
    _mm_store_ss(dst + 2, _mm_shuffle_ps(v, v, _MM_SHUFFLE(2, 2, 2, 2)));
}

// ---------------------------------------------------------------------------
// AVX2 2-vertex helpers (1W only)
// ---------------------------------------------------------------------------

// Pack rows from two different matrices into a 256-bit register:
//   lower 128 = M0.rowN,  upper 128 = M1.rowN
static __forceinline __m256 avx_pack_row(const Fmatrix& M0, const Fmatrix& M1, const float* r0, const float* r1)
{
    return _mm256_set_m128(_mm_loadu_ps(r1), _mm_loadu_ps(r0));
}

// Broadcast one scalar to both 128-bit lanes of a 256-bit register
static __forceinline __m256 avx_bcast2(float s0, float s1)
{
    return _mm256_set_m128(_mm_set1_ps(s1), _mm_set1_ps(s0));
}

// Store xyz from one 128-bit lane
static __forceinline void avx_store3(float* dst, __m128 v)
{
    _mm_store_ss(dst,     v);
    _mm_store_ss(dst + 1, _mm_shuffle_ps(v, v, _MM_SHUFFLE(1, 1, 1, 1)));
    _mm_store_ss(dst + 2, _mm_shuffle_ps(v, v, _MM_SHUFFLE(2, 2, 2, 2)));
}

// ---------------------------------------------------------------------------
// 1W — 2 vertices per iteration, scalar tail
// ---------------------------------------------------------------------------

void __stdcall xrSkin1W_AVX2(vertRender* D, vertBoned1W* S, u32 vCount, CBoneInstance* Bones)
{
    u32 i = 0;
    for (; i + 1 < vCount; i += 2, S += 2, D += 2)
    {
        const Fmatrix& M0 = Bones[S[0].matrix].mRenderTransform;
        const Fmatrix& M1 = Bones[S[1].matrix].mRenderTransform;

        // Pack matrix rows: lower 128 = M0, upper 128 = M1
        __m256 r0 = avx_pack_row(M0, M1, &M0._11, &M1._11);
        __m256 r1 = avx_pack_row(M0, M1, &M0._21, &M1._21);
        __m256 r2 = avx_pack_row(M0, M1, &M0._31, &M1._31);
        __m256 r3 = avx_pack_row(M0, M1, &M0._41, &M1._41);

        // Position transform for both vertices via FMA
        __m256 px = avx_bcast2(S[0].P.x, S[1].P.x);
        __m256 py = avx_bcast2(S[0].P.y, S[1].P.y);
        __m256 pz = avx_bcast2(S[0].P.z, S[1].P.z);
        __m256 p  = _mm256_fmadd_ps(pz, r2, _mm256_fmadd_ps(py, r1, _mm256_fmadd_ps(px, r0, r3)));

        // Normal transform for both vertices via FMA (no translation)
        __m256 nx = avx_bcast2(S[0].N.x, S[1].N.x);
        __m256 ny = avx_bcast2(S[0].N.y, S[1].N.y);
        __m256 nz = avx_bcast2(S[0].N.z, S[1].N.z);
        __m256 n  = _mm256_fmadd_ps(nz, r2, _mm256_fmadd_ps(ny, r1, _mm256_mul_ps(nx, r0)));

        // Extract vertex 0 (lower 128) and vertex 1 (upper 128)
        __m128 p0 = _mm256_castps256_ps128(p);
        __m128 p1 = _mm256_extractf128_ps(p, 1);
        __m128 n0 = _mm256_castps256_ps128(n);
        __m128 n1 = _mm256_extractf128_ps(n, 1);

        avx_store3(&D[0].P.x, p0);
        avx_store3(&D[0].N.x, n0);
        D[0].u = S[0].u;
        D[0].v = S[0].v;

        avx_store3(&D[1].P.x, p1);
        avx_store3(&D[1].N.x, n1);
        D[1].u = S[1].u;
        D[1].v = S[1].v;
    }
    // Scalar tail
    for (; i < vCount; i++, S++, D++)
    {
        const Fmatrix& M = Bones[S->matrix].mRenderTransform;
        store3(&D->P.x, fma_transform_tiny(S->P, M));
        store3(&D->N.x, fma_transform_dir(S->N, M));
        D->u = S->u;
        D->v = S->v;
    }
}

// ---------------------------------------------------------------------------
// 2W — FMA for weighted blend
// ---------------------------------------------------------------------------

void __stdcall xrSkin2W_AVX2(vertRender* D, vertBoned2W* S, u32 vCount, CBoneInstance* Bones)
{
    for (u32 i = 0; i < vCount; i++, S++, D++)
    {
        if (S->matrix0 != S->matrix1)
        {
            const Fmatrix& M0 = Bones[S->matrix0].mRenderTransform;
            const Fmatrix& M1 = Bones[S->matrix1].mRenderTransform;

            __m128 w1 = _mm_set1_ps(S->w);
            __m128 w0 = _mm_set1_ps(1.0f - S->w);

            // P = P0*(1-w) + P1*w  via FMA: fmadd(P1, w1, P0*w0)
            __m128 p0 = fma_transform_tiny(S->P, M0);
            __m128 p1 = fma_transform_tiny(S->P, M1);
            store3(&D->P.x, _mm_fmadd_ps(p1, w1, _mm_mul_ps(p0, w0)));

            __m128 n0 = fma_transform_dir(S->N, M0);
            __m128 n1 = fma_transform_dir(S->N, M1);
            store3(&D->N.x, _mm_fmadd_ps(n1, w1, _mm_mul_ps(n0, w0)));
        }
        else
        {
            const Fmatrix& M0 = Bones[S->matrix0].mRenderTransform;
            store3(&D->P.x, fma_transform_tiny(S->P, M0));
            store3(&D->N.x, fma_transform_dir(S->N, M0));
        }
        D->u = S->u;
        D->v = S->v;
    }
}

// ---------------------------------------------------------------------------
// 3W — FMA for 3-bone weighted accumulation
// ---------------------------------------------------------------------------

void __stdcall xrSkin3W_AVX2(vertRender* D, vertBoned3W* S, u32 vCount, CBoneInstance* Bones)
{
    for (u32 i = 0; i < vCount; i++, S++, D++)
    {
        const Fmatrix& M0 = Bones[S->m[0]].mRenderTransform;
        const Fmatrix& M1 = Bones[S->m[1]].mRenderTransform;
        const Fmatrix& M2 = Bones[S->m[2]].mRenderTransform;

        __m128 w0 = _mm_set1_ps(S->w[0]);
        __m128 w1 = _mm_set1_ps(S->w[1]);
        __m128 w2 = _mm_set1_ps(1.0f - S->w[0] - S->w[1]);

        // FMA chain: fmadd(P2, w2, fmadd(P1, w1, P0*w0))
        __m128 p = _mm_fmadd_ps(fma_transform_tiny(S->P, M2), w2,
                   _mm_fmadd_ps(fma_transform_tiny(S->P, M1), w1,
                                _mm_mul_ps(fma_transform_tiny(S->P, M0), w0)));
        store3(&D->P.x, p);

        __m128 n = _mm_fmadd_ps(fma_transform_dir(S->N, M2), w2,
                   _mm_fmadd_ps(fma_transform_dir(S->N, M1), w1,
                                _mm_mul_ps(fma_transform_dir(S->N, M0), w0)));
        store3(&D->N.x, n);

        D->u = S->u;
        D->v = S->v;
    }
}

// ---------------------------------------------------------------------------
// 4W — FMA for 4-bone weighted accumulation
// ---------------------------------------------------------------------------

void __stdcall xrSkin4W_AVX2(vertRender* D, vertBoned4W* S, u32 vCount, CBoneInstance* Bones)
{
    for (u32 i = 0; i < vCount; i++, S++, D++)
    {
        const Fmatrix& M0 = Bones[S->m[0]].mRenderTransform;
        const Fmatrix& M1 = Bones[S->m[1]].mRenderTransform;
        const Fmatrix& M2 = Bones[S->m[2]].mRenderTransform;
        const Fmatrix& M3 = Bones[S->m[3]].mRenderTransform;

        __m128 w0 = _mm_set1_ps(S->w[0]);
        __m128 w1 = _mm_set1_ps(S->w[1]);
        __m128 w2 = _mm_set1_ps(S->w[2]);
        __m128 w3 = _mm_set1_ps(1.0f - S->w[0] - S->w[1] - S->w[2]);

        __m128 p = _mm_fmadd_ps(fma_transform_tiny(S->P, M3), w3,
                   _mm_fmadd_ps(fma_transform_tiny(S->P, M2), w2,
                   _mm_fmadd_ps(fma_transform_tiny(S->P, M1), w1,
                                _mm_mul_ps(fma_transform_tiny(S->P, M0), w0))));
        store3(&D->P.x, p);

        __m128 n = _mm_fmadd_ps(fma_transform_dir(S->N, M3), w3,
                   _mm_fmadd_ps(fma_transform_dir(S->N, M2), w2,
                   _mm_fmadd_ps(fma_transform_dir(S->N, M1), w1,
                                _mm_mul_ps(fma_transform_dir(S->N, M0), w0))));
        store3(&D->N.x, n);

        D->u = S->u;
        D->v = S->v;
    }
}
