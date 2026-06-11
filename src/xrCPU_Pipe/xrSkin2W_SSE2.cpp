// xrSkin2W_SSE2.cpp — SSE2 C++ intrinsic skinning (x64-compatible).
//
// For each vertex, uses SSE2 to compute all 3 output components (x,y,z) in
// parallel. The matrix is row-major [_11.._14, _21.._24, ...], so one load
// per row gives a vector that, when dot-multiplied with the broadcast
// component, contributes to Dx, Dy, Dz simultaneously.
//
// Throughput gain over scalar: ~3-4× on the math (fewer dependency chains,
// fewer instructions); memory bandwidth is the practical ceiling.

#include <immintrin.h>

#include "xrCPU_Pipe.h"
#include "SkeletonXVertRender.h"
#include "bone.h"

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Compute v × M (row-vector × 4×4), returns [Dx, Dy, Dz, Dw]
static __forceinline __m128 sse2_transform_tiny(const Fvector& v, const Fmatrix& M)
{
    __m128 px = _mm_set1_ps(v.x);
    __m128 py = _mm_set1_ps(v.y);
    __m128 pz = _mm_set1_ps(v.z);
    __m128 r0 = _mm_loadu_ps(&M._11);  // [_11, _12, _13, _14]
    __m128 r1 = _mm_loadu_ps(&M._21);  // [_21, _22, _23, _24]
    __m128 r2 = _mm_loadu_ps(&M._31);  // [_31, _32, _33, _34]
    __m128 r3 = _mm_loadu_ps(&M._41);  // [_41, _42, _43, _44] (translation)
    // result[i] = v.x*_1i + v.y*_2i + v.z*_3i + _4i = Di
    return _mm_add_ps(_mm_add_ps(_mm_mul_ps(px, r0), _mm_mul_ps(py, r1)),
                      _mm_add_ps(_mm_mul_ps(pz, r2), r3));
}

// Direction transform (no translation term)
static __forceinline __m128 sse2_transform_dir(const Fvector& v, const Fmatrix& M)
{
    __m128 px = _mm_set1_ps(v.x);
    __m128 py = _mm_set1_ps(v.y);
    __m128 pz = _mm_set1_ps(v.z);
    __m128 r0 = _mm_loadu_ps(&M._11);
    __m128 r1 = _mm_loadu_ps(&M._21);
    __m128 r2 = _mm_loadu_ps(&M._31);
    return _mm_add_ps(_mm_add_ps(_mm_mul_ps(px, r0), _mm_mul_ps(py, r1)), _mm_mul_ps(pz, r2));
}

// Store 3 floats from SSE register to non-contiguous memory (xyz packed in dst[0..2])
static __forceinline void sse2_store3(float* dst, __m128 v)
{
    _mm_store_ss(dst,     v);
    _mm_store_ss(dst + 1, _mm_shuffle_ps(v, v, _MM_SHUFFLE(1, 1, 1, 1)));
    _mm_store_ss(dst + 2, _mm_shuffle_ps(v, v, _MM_SHUFFLE(2, 2, 2, 2)));
}

// ---------------------------------------------------------------------------
// 1W — single bone per vertex
// ---------------------------------------------------------------------------

void __stdcall xrSkin1W_SSE2(vertRender* D, vertBoned1W* S, u32 vCount, CBoneInstance* Bones)
{
    for (u32 i = 0; i < vCount; i++, S++, D++)
    {
        const Fmatrix& M = Bones[S->matrix].mRenderTransform;
        sse2_store3(&D->P.x, sse2_transform_tiny(S->P, M));
        sse2_store3(&D->N.x, sse2_transform_dir(S->N, M));
        D->u = S->u;
        D->v = S->v;
    }
}

// ---------------------------------------------------------------------------
// 2W — two bone influences, linearly blended
// ---------------------------------------------------------------------------

void __stdcall xrSkin2W_SSE2(vertRender* D, vertBoned2W* S, u32 vCount, CBoneInstance* Bones)
{
    for (u32 i = 0; i < vCount; i++, S++, D++)
    {
        if (S->matrix0 != S->matrix1)
        {
            const Fmatrix& M0 = Bones[S->matrix0].mRenderTransform;
            const Fmatrix& M1 = Bones[S->matrix1].mRenderTransform;

            __m128 w1       = _mm_set1_ps(S->w);
            __m128 w0       = _mm_set1_ps(1.0f - S->w);

            // position: P = P0*(1-w) + P1*w
            __m128 p0 = sse2_transform_tiny(S->P, M0);
            __m128 p1 = sse2_transform_tiny(S->P, M1);
            sse2_store3(&D->P.x, _mm_add_ps(_mm_mul_ps(p0, w0), _mm_mul_ps(p1, w1)));

            // normal: N = N0*(1-w) + N1*w
            __m128 n0 = sse2_transform_dir(S->N, M0);
            __m128 n1 = sse2_transform_dir(S->N, M1);
            sse2_store3(&D->N.x, _mm_add_ps(_mm_mul_ps(n0, w0), _mm_mul_ps(n1, w1)));
        }
        else
        {
            const Fmatrix& M0 = Bones[S->matrix0].mRenderTransform;
            sse2_store3(&D->P.x, sse2_transform_tiny(S->P, M0));
            sse2_store3(&D->N.x, sse2_transform_dir(S->N, M0));
        }
        D->u = S->u;
        D->v = S->v;
    }
}

// ---------------------------------------------------------------------------
// 3W — three bone influences with explicit weights
// ---------------------------------------------------------------------------

void __stdcall xrSkin3W_SSE2(vertRender* D, vertBoned3W* S, u32 vCount, CBoneInstance* Bones)
{
    for (u32 i = 0; i < vCount; i++, S++, D++)
    {
        const Fmatrix& M0 = Bones[S->m[0]].mRenderTransform;
        const Fmatrix& M1 = Bones[S->m[1]].mRenderTransform;
        const Fmatrix& M2 = Bones[S->m[2]].mRenderTransform;

        __m128 w0 = _mm_set1_ps(S->w[0]);
        __m128 w1 = _mm_set1_ps(S->w[1]);
        __m128 w2 = _mm_set1_ps(1.0f - S->w[0] - S->w[1]);

        __m128 p = _mm_add_ps(
            _mm_add_ps(_mm_mul_ps(sse2_transform_tiny(S->P, M0), w0),
                       _mm_mul_ps(sse2_transform_tiny(S->P, M1), w1)),
            _mm_mul_ps(sse2_transform_tiny(S->P, M2), w2));
        sse2_store3(&D->P.x, p);

        __m128 n = _mm_add_ps(
            _mm_add_ps(_mm_mul_ps(sse2_transform_dir(S->N, M0), w0),
                       _mm_mul_ps(sse2_transform_dir(S->N, M1), w1)),
            _mm_mul_ps(sse2_transform_dir(S->N, M2), w2));
        sse2_store3(&D->N.x, n);

        D->u = S->u;
        D->v = S->v;
    }
}

// ---------------------------------------------------------------------------
// 4W — four bone influences
// ---------------------------------------------------------------------------

void __stdcall xrSkin4W_SSE2(vertRender* D, vertBoned4W* S, u32 vCount, CBoneInstance* Bones)
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

        __m128 p = _mm_add_ps(
            _mm_add_ps(_mm_mul_ps(sse2_transform_tiny(S->P, M0), w0),
                       _mm_mul_ps(sse2_transform_tiny(S->P, M1), w1)),
            _mm_add_ps(_mm_mul_ps(sse2_transform_tiny(S->P, M2), w2),
                       _mm_mul_ps(sse2_transform_tiny(S->P, M3), w3)));
        sse2_store3(&D->P.x, p);

        __m128 n = _mm_add_ps(
            _mm_add_ps(_mm_mul_ps(sse2_transform_dir(S->N, M0), w0),
                       _mm_mul_ps(sse2_transform_dir(S->N, M1), w1)),
            _mm_add_ps(_mm_mul_ps(sse2_transform_dir(S->N, M2), w2),
                       _mm_mul_ps(sse2_transform_dir(S->N, M3), w3)));
        sse2_store3(&D->N.x, n);

        D->u = S->u;
        D->v = S->v;
    }
}
