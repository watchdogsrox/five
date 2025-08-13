// ======================
// renderer/util/util.h
// (c) 2010 RockstarNorth
// ======================

#ifndef _RENDERER_UTIL_UTIL_H_
#define _RENDERER_UTIL_UTIL_H_

#include "vectormath/classes.h"
#include "vector/matrix44.h"
#include "atl/string.h"

#include "fwmaths/vectorutil.h"
#include "math/vecrand.h" // i moved a bunch of shit into rage math ..

void _test_vectormath_stuff(int count);

namespace rage { typedef double f64; } // please put this in the forceincludes

// ================================================================================================

template <typename T, size_t N> __forceinline bool IsOneOf(const T& src, const T (&arr)[N])
{
	for (size_t i = 0; i < N; i++)
	{
		if (src == arr[i])
		{
			return true;
		}
	}

	return false;
}

// ================================================================================================

#if 1 // TODO -- clean this up and move it somewhere

#define _MAT4_DST_PARAMS(m) Vector_4V_InOut m##0, Vector_4V_InOut m##1, Vector_4V_InOut m##2, Vector_4V_InOut m##3
#define _MAT3_DST_PARAMS(m) Vector_4V_InOut m##0, Vector_4V_InOut m##1, Vector_4V_InOut m##2

#define _MAT4_SRC_PARAMS(m) Vector_4V_In m##0, Vector_4V_In m##1, Vector_4V_In m##2, Vector_4V_In_After3Args m##3
#define _MAT3_SRC_PARAMS(m) Vector_4V_In m##0, Vector_4V_In m##1, Vector_4V_In m##2

#define _MAT4_SRC_PARAMS_AFTER1ARGS(m) Vector_4V_In m##0, Vector_4V_In m##1, Vector_4V_In_After3Args m##2, Vector_4V_In_After3Args m##3
#define _MAT3_SRC_PARAMS_AFTER1ARGS(m) Vector_4V_In m##0, Vector_4V_In m##1, Vector_4V_In_After3Args m##2

#define _MAT4_SRC_PARAMS_AFTER2ARGS(m) Vector_4V_In m##0, Vector_4V_In_After3Args m##1, Vector_4V_In_After3Args m##2, Vector_4V_In_After3Args m##3
#define _MAT3_SRC_PARAMS_AFTER2ARGS(m) Vector_4V_In m##0, Vector_4V_In_After3Args m##1, Vector_4V_In_After3Args m##2

#define _MAT4_SRC_PARAMS_AFTER3ARGS(m) Vector_4V_In_After3Args m##0, Vector_4V_In_After3Args m##1, Vector_4V_In_After3Args m##2, Vector_4V_In_After3Args m##3
#define _MAT3_SRC_PARAMS_AFTER3ARGS(m) Vector_4V_In_After3Args m##0, Vector_4V_In_After3Args m##1, Vector_4V_In_After3Args m##2

#define _MAT4_TO_PARAMS(m) m.GetCol0Intrin128(), m.GetCol1Intrin128(), m.GetCol2Intrin128(), m.GetCol3Intrin128()
#define _MAT3_TO_PARAMS(m) m.GetCol0Intrin128(), m.GetCol1Intrin128(), m.GetCol2Intrin128()

namespace rage {
namespace Vec {

__forceinline void          _V4_Transpose_Mat44(_MAT4_DST_PARAMS(d), _MAT4_SRC_PARAMS(m));
__forceinline void          _V4_Transpose_Mat34(_MAT3_DST_PARAMS(d), _MAT4_SRC_PARAMS(m));
__forceinline void          _V4_Transpose_Mat33(_MAT3_DST_PARAMS(d), _MAT3_SRC_PARAMS(m));

__forceinline Vector_4V_Out _V4_Multiply_Mat44_Vec4(_MAT4_SRC_PARAMS(m), Vector_4V_In_After3Args a);
__forceinline Vector_4V_Out _V4_Multiply_Mat34_Vec4(_MAT4_SRC_PARAMS(m), Vector_4V_In_After3Args a);
__forceinline Vector_4V_Out _V4_Multiply_Mat33_Vec3(_MAT3_SRC_PARAMS(m), Vector_4V_In_After3Args a);

__forceinline Vector_4V_Out _V4_Multiply_Vec4_Mat44(Vector_4V_In a, _MAT4_SRC_PARAMS_AFTER1ARGS(m));
__forceinline Vector_4V_Out _V4_Multiply_Vec3_Mat33(Vector_4V_In a, _MAT3_SRC_PARAMS_AFTER1ARGS(m));

__forceinline Vector_4V_Out _V4_InvertTransposeAffine_Mat33(_MAT3_DST_PARAMS(d), _MAT3_SRC_PARAMS(m));

__forceinline Vector_4V_Out _V4_UnTransformAffine_Mat33_Vec3     (_MAT3_SRC_PARAMS(m), Vector_4V_In_After3Args a);
__forceinline Vector_4V_Out _V4_UnTransformAffine_Mat33_Vec3_fast(_MAT3_SRC_PARAMS(m), Vector_4V_In_After3Args a);

} // namespace Vec

enum
{
	_X1 = Vec::X1,
	_Y1 = Vec::Y1,
	_Z1 = Vec::Z1,
	_W1 = Vec::W1,

	_X2 = Vec::X2,
	_Y2 = Vec::Y2,
	_Z2 = Vec::Z2,
	_W2 = Vec::W2,

//	_ZERO = ...,
//	_ANY1 = ...,
//	_ANY2 = ...,
//	_DONTCARE = ...,
};

template <u32 permX, u32 permY, u32 permZ, u32 permW> __forceinline Vec4V_Out Permute(Vec4V_In a);

// ================================================================================================

__forceinline Vec4V_Out _TransformV(Mat44V_In m, Vec4V_In a) { return  Vec4V(           Vec::_V4_Multiply_Mat44_Vec4(_MAT4_TO_PARAMS(m), a.GetIntrin128())); }
__forceinline Vec3V_Out _TransformV(Mat34V_In m, Vec4V_In a) { return  Vec3V(           Vec::_V4_Multiply_Mat34_Vec4(_MAT4_TO_PARAMS(m), a.GetIntrin128())); }
__forceinline Vec3V_Out _TransformP(Mat34V_In m, Vec3V_In a) { return  Vec3V(Vec::V4Add(Vec::_V4_Multiply_Mat33_Vec3(_MAT3_TO_PARAMS(m), a.GetIntrin128()), m.GetCol3Intrin128())); }
__forceinline Vec3V_Out _TransformV(Mat34V_In m, Vec3V_In a) { return  Vec3V(           Vec::_V4_Multiply_Mat33_Vec3(_MAT3_TO_PARAMS(m), a.GetIntrin128())); }
__forceinline Vec3V_Out _TransformV(Mat33V_In m, Vec3V_In a) { return  Vec3V(           Vec::_V4_Multiply_Mat33_Vec3(_MAT3_TO_PARAMS(m), a.GetIntrin128())); }

__forceinline Vec3V_Out _UnTransformOrthoP(Mat34V_In m, Vec3V_In a) { return Vec3V(Vec::_V4_Multiply_Vec3_Mat33(Vec::V4Subtract(a.GetIntrin128(), m.GetCol3Intrin128()), _MAT3_TO_PARAMS(m))); }
__forceinline Vec3V_Out _UnTransformOrthoV(Mat34V_In m, Vec3V_In a) { return Vec3V(Vec::_V4_Multiply_Vec3_Mat33(                a.GetIntrin128(),                        _MAT3_TO_PARAMS(m))); }
__forceinline Vec3V_Out _UnTransformOrthoV(Mat33V_In m, Vec3V_In a) { return Vec3V(Vec::_V4_Multiply_Vec3_Mat33(                a.GetIntrin128(),                        _MAT3_TO_PARAMS(m))); }

__forceinline Vec3V_Out _UnTransformAffineP(Mat34V_In m, Vec3V_In a) { return Vec3V(Vec::_V4_UnTransformAffine_Mat33_Vec3(_MAT3_TO_PARAMS(m), Vec::V4Subtract(a.GetIntrin128(), m.GetCol3Intrin128()))); }
__forceinline Vec3V_Out _UnTransformAffineV(Mat34V_In m, Vec3V_In a) { return Vec3V(Vec::_V4_UnTransformAffine_Mat33_Vec3(_MAT3_TO_PARAMS(m),                 a.GetIntrin128())); }
__forceinline Vec3V_Out _UnTransformAffineV(Mat33V_In m, Vec3V_In a) { return Vec3V(Vec::_V4_UnTransformAffine_Mat33_Vec3(_MAT3_TO_PARAMS(m),                 a.GetIntrin128())); }

__forceinline Vec3V_Out _UnTransformAffineP_fast(Mat34V_In m, Vec3V_In a) { return Vec3V(Vec::_V4_UnTransformAffine_Mat33_Vec3_fast(_MAT3_TO_PARAMS(m), Vec::V4Subtract(a.GetIntrin128(), m.GetCol3Intrin128()))); }
__forceinline Vec3V_Out _UnTransformAffineV_fast(Mat34V_In m, Vec3V_In a) { return Vec3V(Vec::_V4_UnTransformAffine_Mat33_Vec3_fast(_MAT3_TO_PARAMS(m),                 a.GetIntrin128())); }
__forceinline Vec3V_Out _UnTransformAffineV_fast(Mat33V_In m, Vec3V_In a) { return Vec3V(Vec::_V4_UnTransformAffine_Mat33_Vec3_fast(_MAT3_TO_PARAMS(m),                 a.GetIntrin128())); }

// ================================================================================================

namespace Vec {

__forceinline void _V4_Transpose_Mat44(_MAT4_DST_PARAMS(d), _MAT4_SRC_PARAMS(m))
{
	Vector_4V v0,v1,v2,v3;
	v0 = V4MergeXY(m0, m2); // m0x,m2x,m0y,m2y
	v1 = V4MergeXY(m1, m3); // m1x,m3x,m1y,m3y
	v2 = V4MergeZW(m0, m2); // m0z,m2z,m0w,m2w
	v3 = V4MergeZW(m1, m3); // m1z,m3z,m1w,m3w
	d0 = V4MergeXY(v0, v1); // m0x,m1x,m2x,m3x
	d1 = V4MergeZW(v0, v1); // m0y,m1y,m2y,m3y
	d2 = V4MergeXY(v2, v3); // m0z,m1z,m2z,m3z
	d3 = V4MergeZW(v2, v3); // m0w,m1w,m2w,m3w
}

__forceinline void _V4_Transpose_Mat34(_MAT3_DST_PARAMS(d), _MAT4_SRC_PARAMS(m))
{
	Vector_4V v0,v1,v2,v3;
	v0 = V4MergeXY(m0, m2); // m0x,m2x,m0y,m2y
	v1 = V4MergeXY(m1, m3); // m1x,m3x,m1y,m3y
	v2 = V4MergeZW(m0, m2); // m0z,m2z,m0w,m2w
	v3 = V4MergeZW(m1, m3); // m1z,m3z,m1w,m3w
	d0 = V4MergeXY(v0, v1); // m0x,m1x,m2x,m3x
	d1 = V4MergeZW(v0, v1); // m0y,m1y,m2y,m3y
	d2 = V4MergeXY(v2, v3); // m0z,m1z,m2z,m3z
}

__forceinline void _V4_Transpose_Mat33(_MAT3_DST_PARAMS(d), _MAT3_SRC_PARAMS(m))
{
#if !USE_ALTERNATE_3X3_TRANSPOSE // 5 instr. .. w-components will be slightly different
	Vector_4V v0,v2;
	v0 = V4MergeXY(m0, m2); // m0x,m2x,m0y,m2y
	v2 = V4MergeZW(m0, m2); // m0z,m2z,m0w,m2w
	d0 = V4MergeXY(v0, m1); // m0x,m1x,m2x,m1y
	d1 = V4PermuteTwo<Z1,Y2,W1,X1>(v0, m1); // m0y,m1y,m2y,m0x
	d2 = V4PermuteTwo<X1,Z2,Y1,X1>(v2, m1); // m0z,m1z,m2z,m0z
#else // 7 instr.
	Vector_4V v0,v1,v2,v3,m3=m2;
	v0 = V4MergeXY(m0, m2); // m0x,m2x,m0y,m2y
	v1 = V4MergeXY(m1, m3); // m1x,m3x,m1y,m3y
	v2 = V4MergeZW(m0, m2); // m0z,m2z,m0w,m2w
	v3 = V4MergeZW(m1, m3); // m1z,m3z,m1w,m3w
	d0 = V4MergeXY(v0, v1); // m0x,m1x,m2x,m3x
	d1 = V4MergeZW(v0, v1); // m0y,m1y,m2y,m3y
	d2 = V4MergeXY(v2, v3); // m0z,m1z,m2z,m3z
#endif
}

__forceinline Vector_4V_Out _V4_Multiply_Mat44_Vec4(_MAT4_SRC_PARAMS(m), Vector_4V_In_After3Args a)
{
#if __XENON // 15 instr. (faster?)
	Vector_4V t0,t1,t2,t3; _V4_Transpose_Mat44(t0,t1,t2,t3, m0,m1,m2,m3);
	t0 = V4DotV(t0, a);
	t1 = V4DotV(t1, a);
	t2 = V4DotV(t2, a);
	t3 = V4DotV(t3, a);
	t0 = V4MergeXY(t0, t2);
	t1 = V4MergeXY(t1, t3);
	t0 = V4MergeXY(t0, t1);
	return t0;
#else // 9/8 instr.
	Vector_4V t0,t1,t2,t3;
	t0 = V4SplatX(a);
	t1 = V4SplatY(a);
	t2 = V4SplatZ(a);
	t3 = V4SplatW(a);
	#if 1
		t0 = V4Scale(t0, m0);
		t1 = V4Scale(t1, m1);
		t0 = V4AddScaled(t0, t2, m2);
		t1 = V4AddScaled(t1, t3, m3);
		t0 = V4Add(t0, t1);
	#else
		t0 = V4Scale(t0, m0);
		t0 = V4AddScaled(t0, t1, m1);
		t0 = V4AddScaled(t0, t2, m2);
		t1 = V4AddScaled(t0, t3, m3);
	#endif
	return t0;
#endif
}

__forceinline Vector_4V_Out _V4_Multiply_Mat34_Vec4(_MAT4_SRC_PARAMS(m), Vector_4V_In_After3Args a)
{
#if 0 && __XENON // 13 instr.
	Vector_4V t0,t1,t2; _V4_Transpose_Mat34(t0,t1,t2, m0,m1,m2,m3);
	t0 = V4DotV(t0, a);
	t1 = V4DotV(t1, a);
	t2 = V4DotV(t2, a);
	t0 = V4MergeXY(t0, t2);
	t0 = V4MergeXY(t0, t1);
	return t0;
#else // 9/8 instr.
	Vector_4V t0,t1,t2,t3;
	t0 = V4SplatX(a);
	t1 = V4SplatY(a);
	t2 = V4SplatZ(a);
	t3 = V4SplatW(a);
	#if 1
		t0 = V4Scale(t0, m0);
		t1 = V4Scale(t1, m1);
		t0 = V4AddScaled(t0, t2, m2);
		t1 = V4AddScaled(t1, t3, m3);
		t0 = V4Add(t0, t1);
	#else
		t0 = V4Scale(t0, m0);
		t0 = V4AddScaled(t0, t1, m1);
		t0 = V4AddScaled(t0, t2, m2);
		t1 = V4AddScaled(t0, t3, m3);
	#endif
	return t0;
#endif
}

__forceinline Vector_4V_Out _V4_Multiply_Mat33_Vec3(_MAT3_SRC_PARAMS(m), Vector_4V_In_After3Args a)
{
#if __XENON // 10 instr.
	Vector_4V t0,t1,t2; _V4_Transpose_Mat33(t0,t1,t2, m0,m1,m2);
	t0 = V3DotV(t0, a);
	t1 = V3DotV(t1, a);
	t2 = V3DotV(t2, a);
	t0 = V4MergeXY(t0, t2);
	t0 = V4MergeXY(t0, t1);
	return t0;
#else // 6 instr.
	Vector_4V t0,t1,t2;
	t0 = V4SplatX(a);
	t1 = V4SplatY(a);
	t2 = V4SplatZ(a);
	t0 = V4Scale(t0, m0);
	t0 = V4AddScaled(t0, t1, m1);
	t0 = V4AddScaled(t0, t2, m2);
	return t0;
#endif
}

__forceinline Vector_4V_Out _V4_Multiply_Vec4_Mat44(Vector_4V_In a, _MAT4_SRC_PARAMS_AFTER1ARGS(m))
{
#if __XENON // 7 instr.
	Vector_4V t0,t1,t2,t3;
	t0 = V4DotV(m0, a);
	t1 = V4DotV(m1, a);
	t2 = V4DotV(m2, a);
	t3 = V4DotV(m3, a);
	t0 = V4MergeXY(t0, t2);
	t1 = V4MergeXY(t1, t3);
	t0 = V4MergeXY(t0, t1);
	return t0;
#else // 17/16 instr.
	Vector_4V t0,t1,t2,t3; _V4_Transpose_Mat44(t0,t1,t2,t3, m0,m1,m2,m3);
	Vector_4V a0,a1,a2,a3;
	a0 = V4SplatX(a);
	a1 = V4SplatY(a);
	a2 = V4SplatZ(a);
	a3 = V4SplatW(a);
	#if 1
		t0 = V4Scale(t0, a0);
		t1 = V4Scale(t1, a1);
		t0 = V4AddScaled(t0, t2, a2);
		t1 = V4AddScaled(t1, t3, a3);
		t0 = V4Add(t0, t1);
	#else
		t0 = V4Scale(t0, a0);
		t0 = V4AddScaled(t0, t1, a1);
		t0 = V4AddScaled(t0, t2, a2);
		t1 = V4AddScaled(t0, t3, a3);
	#endif
	return t0;
#endif
}

__forceinline Vector_4V_Out _V4_Multiply_Vec3_Mat33(Vector_4V_In a, _MAT3_SRC_PARAMS_AFTER1ARGS(m))
{
#if __XENON // 5 instr.
	Vector_4V t0,t1,t2;
	t0 = V3DotV(m0, a);
	t1 = V3DotV(m1, a);
	t2 = V3DotV(m2, a);
	t0 = V4MergeXY(t0, t2);
	t0 = V4MergeXY(t0, t1);
	return t0;
#else // 13 instr.
	Vector_4V t0,t1,t2; _V4_Transpose_Mat33(t0,t1,t2, m0,m1,m2);
	Vector_4V a0,a1,a2;
	a0 = V4SplatX(a);
	a1 = V4SplatY(a);
	a2 = V4SplatZ(a);
	t0 = V4Scale(t0, a0);
	t0 = V4AddScaled(t0, t1, a1);
	t0 = V4AddScaled(t0, t2, a2);
	return t0;
#endif
}

__forceinline Vector_4V_Out _V4_InvertTransposeAffine_Mat33(_MAT3_DST_PARAMS(d), _MAT3_SRC_PARAMS(m))
{
	d0 = V3Cross(m1, m2); // m1yzw*m2zxy - m2yzw*m1zxy
	d1 = V3Cross(m2, m0); // m2yzw*m0zxy - m0yzw*m2zxy
	d2 = V3Cross(m0, m1); // m0yzw*m1zxy - m1yzw*m0zxy

	return V3DotV(m0, d0);
}

__forceinline Vector_4V_Out _V4_UnTransformAffine_Mat33_Vec3(_MAT3_SRC_PARAMS(m), Vector_4V_In_After3Args a)
{
	Vector_4V c0,c1,c2,det = _V4_InvertTransposeAffine_Mat33(c0,c1,c2, m0,m1,m2);

	const Vector_4V invdet = V4Invert(det); // {c0,c1,c2}*invdet is inverse transpose of {m0,m1,m2}
	const Vector_4V av     = V4Scale(a, invdet);

	return _V4_Multiply_Vec3_Mat33(av, c0,c1,c2);
}

__forceinline Vector_4V_Out _V4_UnTransformAffine_Mat33_Vec3_fast(_MAT3_SRC_PARAMS(m), Vector_4V_In_After3Args a)
{
	Vector_4V c0,c1,c2,det = _V4_InvertTransposeAffine_Mat33(c0,c1,c2, m0,m1,m2);

	const Vector_4V invdet = V4InvertFast(det); // {c0,c1,c2}*invdet is inverse transpose of {m0,m1,m2}
	const Vector_4V av     = V4Scale(a, invdet);

	return _V4_Multiply_Vec3_Mat33(av, c0,c1,c2);
}

} // namespace Vec

// ================================================================================================

template <u32 permX, u32 permY, u32 permZ, u32 permW> __forceinline Vec4V_Out Permute(Vec4V_In a)
{
	return a.Get<permX,permY,permZ,permW>();
}

} // namespace rage

#undef _MAT4_DST_PARAMS
#undef _MAT3_DST_PARAMS

#undef _MAT4_SRC_PARAMS
#undef _MAT3_SRC_PARAMS

#undef _MAT4_SRC_PARAMS_AFTER1ARGS
#undef _MAT3_SRC_PARAMS_AFTER1ARGS

#undef _MAT4_SRC_PARAMS_AFTER2ARGS
#undef _MAT3_SRC_PARAMS_AFTER2ARGS

#undef _MAT4_SRC_PARAMS_AFTER3ARGS
#undef _MAT3_SRC_PARAMS_AFTER3ARGS

#undef _MAT4_TO_PARAMS
#undef _MAT3_TO_PARAMS

#endif

// ================================================================================================

// ================================================================================================

// ================================================================================================
// copy of projects/rng .. adapting to rage vectormath
// ===================================================

#if 1 // TODO -- clean this up and move it somewhere

void _test_V4Shift();

void _test_XorShift31SkipAhead(bool bTestPerformance = false);
void _test_XorShift32SkipAhead(bool bTestPerformance = false);
void _test_Pattern();
void _test_StrangeXorShiftPattern();

#endif

// ================================================================================================

// ================================================================================================

#if 1 // TODO -- clean this up and move it somewhere

namespace rage {
namespace Vec {

__forceinline Vector_4V_Out _V4DivideBy2(Vector_4V_In a)
{
	// comment by Luke Hutchinson [luke.hutchinson@teambondi.com]
	// 1. Input must be finite floats (infinity will not stay as infinity)
	// 2. Very small floats eg 0x00c00000 will return a denormal, and an incorrect one at that!
	// The problem is not as bad as it sounds though, assuming non-Java mode is enabled (the
	// default for both PS3 and 360), denormal inputs to any further instructions will all get
	// flushed to zero.

	const Vector_4V z = V4IsEqualV   (a, V4VConstant(V_ZERO));
	const Vector_4V b = V4SubtractInt(a, V4VConstant(V_FLT_MIN)); // subtract 0x00800000 integer
	const Vector_4V c = V4Andc       (b, z);

	return c;
}

__forceinline const Vector_4V _V4VConstant_V_FLT_MIN() // replacement for V4VConstant(V_FLT_MIN)
{
#if UNIQUE_VECTORIZED_TYPE && __XENON
	return __vslw( __vspltisw(1), __vspltisw(-9) );
#elif UNIQUE_VECTORIZED_TYPE && __PS3
	return (Vector_4V)vec_sl( vec_splat_u32(1), vec_splat_u32(-9) );
#elif UNIQUE_VECTORIZED_TYPE && __SPU
	return (Vector_4V)spu_splats( (int)U32_FLT_MIN );
#else
	return V4VConstant<U32_FLT_MIN,U32_FLT_MIN,U32_FLT_MIN,U32_FLT_MIN>();
#endif
}

#if __PPU // optimisation from LAN guys ..

__forceinline unsigned int _V3IsEqualIntAll       (Vector_4V_In a, Vector_4V_In b);
__forceinline unsigned int _V3IsEqualAll          (Vector_4V_In a, Vector_4V_In b);
__forceinline unsigned int _V3IsLessThanAll       (Vector_4V_In a, Vector_4V_In b);
__forceinline unsigned int _V3IsLessThanOrEqualAll(Vector_4V_In a, Vector_4V_In b);

/*
__forceinline unsigned int _V3IsEqualIntAll(Vector_4V_In a, Vector_4V_In b)
{
	const Vector_4V z = (Vector_4V)vec_splat_s8(0);
	return vec_all_eq((Vector_4V_uint)vec_sld(z, a, 12), (Vector_4V_uint)vec_sld(z, b, 12));
}

__forceinline unsigned int _V3IsEqualAll(Vector_4V_In a, Vector_4V_In b)
{
	const Vector_4V z = (Vector_4V)vec_splat_s8(0);
	return vec_all_eq(vec_sld(z, a, 12), vec_sld(z, b, 12));
}

__forceinline unsigned int _V3IsLessThanAll(Vector_4V_In a, Vector_4V_In b)
{
	const Vector_4V g = (Vector_4V)vec_splat_s8(1); // '0x01010101' is something > 0 in floating-point
	const Vector_4V z = (Vector_4V)vec_splat_s8(0);
	return vec_all_lt(vec_sld(z, a, 12), vec_sld(g, b, 12));
}

__forceinline unsigned int _V3IsLessThanOrEqualAll(Vector_4V_In a, Vector_4V_In b)
{
	const Vector_4V z = (Vector_4V)vec_splat_s8(0);
	return vec_all_le(vec_sld(z, a, 12), vec_sld(z, b, 12));
}
*/

__forceinline unsigned int _V3IsEqualIntAll(Vector_4V_In a, Vector_4V_In b)
{
	const Vector_4V z = V4VConstant(V_ZERO);
	return V4IsEqualIntAll(V4ShiftLeftBytesDouble<12>(z, a), V4ShiftLeftBytesDouble<12>(z, b));
}

__forceinline unsigned int _V3IsEqualAll(Vector_4V_In a, Vector_4V_In b)
{
	const Vector_4V z = V4VConstant(V_ZERO);
	return V4IsEqualAll(V4ShiftLeftBytesDouble<12>(z, a), V4ShiftLeftBytesDouble<12>(z, b));
}

__forceinline unsigned int _V3IsLessThanAll(Vector_4V_In a, Vector_4V_In b)
{
	const Vector_4V g = (Vector_4V)vec_splat_s8(1); // '0x01010101' is something > 0 in floating-point
	const Vector_4V z = (Vector_4V)vec_splat_s8(0);
	return V4IsLessThanAll(V4ShiftLeftBytesDouble<12>(z, a), V4ShiftLeftBytesDouble<12>(g, b));
}

__forceinline unsigned int _V3IsLessThanOrEqualAll(Vector_4V_In a, Vector_4V_In b)
{
	const Vector_4V z = V4VConstant(V_ZERO);
	return V4IsLessThanOrEqualAll(V4ShiftLeftBytesDouble<12>(z, a), V4ShiftLeftBytesDouble<12>(z, b));
}

#else

#define _V3IsEqualIntAll        V3IsEqualIntAll
#define _V3IsEqualAll           V3IsEqualAll
#define _V3IsLessThanAll        V3IsLessThanAll
#define _V3IsLessThanOrEqualAll V3IsLessThanOrEqualAll

#endif

} // namespace Vec
} // namespace rage

#endif

// ================================================================================================

// ================================================================================================

// To avoid LHS stalls when generating random vector data, one solution is to buffer the data.
// CBufferedRNG is a template class which maintains a buffer of count OutputType's worth of
// data, filling the buffer on demand. The template requires a RNG class with a few simple
// methods such as RNG::Reset(seed) and RNG::Generate(). Note that RNG::Generate() does not
// have to generate the same size type as CBufferedRNG::Generate() returns, in fact this
// template can be used to buffer random vector data from a scalar generator or random scalar
// data from a vector generator, as well as random float (scalar) data from an integer (scalar)
// generator.

// ================================================================================================

namespace CBufferedRNG_ {

class _FromU32_add1 { public: typedef u32 T; enum { required = 1 }; static __forceinline T ConvertSrc(T value)
{
	// optionally shift right by 9 bits (or 8 bits if value is 31 bits) instead of masking
	const u32 mask = 0x007ffffful;
	const u32 one  = 0x3f800000ul;
	return (value & mask) | one; // binary pattern for float [1..2)
}};
class _FromU64_add1 { public: typedef u64 T; enum { required = 1 }; static __forceinline T ConvertSrc(T value)
{
	const u64 mask = 0x007fffff007fffffull;
	const u64 one  = 0x3f8000003f800000ull;
	return (value & mask) | one; // binary pattern for float [1..2)
}};
class _FromVEC { public: typedef Vec::Vector_4V T; enum { required = 1 }; static __forceinline T ConvertSrc(T value)
{
	// optionally shift right by 9 bits (or 8 bits if value is 31 bits) instead of masking
	const Vec::Vector_4V mask = Vec::V4VConstantSplat<0x007fffff>(); // mantissa mask
	const Vec::Vector_4V one  = Vec::V4VConstantSplat<0x3f800000>(); // 1.0f
	return Vec::V4Subtract(Vec::V4Or(Vec::V4And(value, mask), one), one);
}};
class _ToF32 { public: typedef float T; enum { required = 0 }; static __forceinline T ConvertDst(T value)
{
	return value;
}};
class _ToF32_sub1 { public: typedef float T; enum { required = 1 }; static __forceinline T ConvertDst(T value)
{
	return value - 1.0f;
}};
class _ToVEC { public: typedef Vec::Vector_4V T; enum { required = 0 }; static __forceinline T ConvertDst(T value)
{
	return value;
}};
class _ToVEC_sub1 { public: typedef Vec::Vector_4V T; enum { required = 1 }; static __forceinline T ConvertDst(T value)
{
	return Vec::V4Subtract(value, Vec::V4VConstantSplat<0x3f800000>());
}};

template <typename DstType, typename SrcType> class CConverter
{
public:
	static __forceinline DstType ConvertDst(DstType value) { return value; }
	static __forceinline SrcType ConvertSrc(SrcType value) { return value; }
};

#if 1 // new code uses macro .. keep this

#define DEF_CONVERTER(dstconv,srcconv) \
	template <> class CConverter<typename dstconv::T, typename srcconv::T> \
		: public dstconv \
		, public srcconv \
	{ \
	public: \
		enum { REQUIRE_SRC_CONVERSION = srcconv::required }; \
		enum { REQUIRE_DST_CONVERSION = dstconv::required }; \
	} \
	// end.

DEF_CONVERTER(_ToF32_sub1, _FromU32_add1);
DEF_CONVERTER(_ToF32_sub1, _FromU64_add1);
DEF_CONVERTER(_ToF32     , _FromVEC     );
DEF_CONVERTER(_ToVEC_sub1, _FromU32_add1);
DEF_CONVERTER(_ToVEC_sub1, _FromU64_add1);
DEF_CONVERTER(_ToVEC     , _FromVEC     );

#undef DEF_CONVERTER

#else // old code without macro

template <> class CConverter<float         ,u32           > : public _ToF32_sub1, public _FromU32_add1 { public: enum { REQUIRE_DST_CONVERSION = 1 }; };
template <> class CConverter<float         ,u64           > : public _ToF32_sub1, public _FromU64_add1 { public: enum { REQUIRE_DST_CONVERSION = 1 }; };
template <> class CConverter<float         ,Vec::Vector_4V> : public _ToF32     , public _FromVEC      { public: enum { REQUIRE_DST_CONVERSION = 0 }; };
template <> class CConverter<Vec::Vector_4V,u32           > : public _ToVEC_sub1, public _FromU32_add1 { public: enum { REQUIRE_DST_CONVERSION = 1 }; };
template <> class CConverter<Vec::Vector_4V,u64           > : public _ToVEC_sub1, public _FromU64_add1 { public: enum { REQUIRE_DST_CONVERSION = 1 }; };
template <> class CConverter<Vec::Vector_4V,Vec::Vector_4V> : public _ToVEC     , public _FromVEC      { public: enum { REQUIRE_DST_CONVERSION = 0 }; };

#endif

} // namespace CBufferedRNG_

#define CBufferedRNG_template_decl template <typename OutputType, typename RNG>
#define CBufferedRNG_template_inst CBufferedRNG<OutputType,RNG>

CBufferedRNG_template_decl class CBufferedRNG : public RNG
{
private:
	typedef typename RNG::SeedType SeedType;
	typedef typename RNG::DataType DataType;
	typedef CBufferedRNG_::CConverter<OutputType,DataType> ConverterClass;

public:
	__forceinline CBufferedRNG(SeedType seed, int count);
	__forceinline ~CBufferedRNG();

	__forceinline void Reset(SeedType seed);

	__forceinline       OutputType  Generate(); // grab one vector of random data, fill buffer if necessary
	__forceinline const OutputType* Generate(int count); // returns pointer to count vectors of random data
	__forceinline void              Generate(OutputType* data, int count); // copy vector data to memory

private:
	__forceinline void Fill(); // TODO -- make this non-inlined

	OutputType* m_buffer;
	int         m_index;
	int         m_count;
};

CBufferedRNG_template_decl __forceinline CBufferedRNG_template_inst::CBufferedRNG(SeedType seed, int count) : RNG(seed)
{
	FastAssert(count > 0);
	FastAssert((count*sizeof(OutputType)) % sizeof(DataType) == 0);

	m_buffer = rage_new OutputType[count];
	//m_buffer = (OutputType*)_aligned_malloc(count*sizeof(OutputType), 16);
	m_index  = count;
	m_count  = count;
}

CBufferedRNG_template_decl __forceinline CBufferedRNG_template_inst::~CBufferedRNG()
{
	delete[] m_buffer;
	//_aligned_free(m_buffer);
}

CBufferedRNG_template_decl __forceinline void CBufferedRNG_template_inst::Reset(SeedType seed)
{
	RNG::Reset(seed);
	m_index = m_count;
}

CBufferedRNG_template_decl __forceinline OutputType CBufferedRNG_template_inst::Generate() // grab one vector of random data, fill buffer if necessary
{
	if (m_index >= m_count)
	{
		Fill();
	}

	return ConverterClass::ConvertDst(m_buffer[m_index++]);
}

#if 0 // this doesn't compile.  What is 'base'?
CBufferedRNG_template_decl __forceinline const OutputType* CBufferedRNG_template_inst::Generate(int count) // returns pointer to count vectors of random data
{
	FastAssert(count <= m_count);

	if (m_index + count > m_count) // note that this may skip vectors at the end of m_buffer ...
	{
		Fill();
	}

	if (ConverterClass::REQUIRE_DST_CONVERSION)
	{
		OutputType* dst0 = &m_buffer[m_index];
		OutputType* dst1 = &m_buffer[m_index + count];

#if !__PS3 // optional loop unrolling (having issues with macro cleverness on new ps3 compiler)
#define DEF_UNROLLCODE(i,args) ARG(0,args)[i] = ARG(1,args)(ARG(0,args)[i])
#define DEF_UNROLL(n,T,ptr,end,code) \
		for (const T* dst##n = &end[-(n)]; ptr <= dst##n; ptr += (n)) \
		{ \
			REP##n##_SEPARATOR_FOREACH_ARGS(UNROLLCODE,;,(ptr,code)); \
		} \
		// end.

		DEF_UNROLL(8,OutputType,dst0,dst1,ConverterClass::ConvertDst);
		DEF_UNROLL(4,OutputType,dst0,dst1,ConverterClass::ConvertDst);
		DEF_UNROLL(2,OutputType,dst0,dst1,ConverterClass::ConvertDst);

#undef DEF_UNROLLCODE
#undef DEF_UNROLL
#endif

		while (dst0 < dst1)
		{
			*(dst0++) = ConverterClass::ConvertDst(*dst0);
		}
	}

	m_index += count;

	return base;
}
#endif

// this function could probably be optimised a bit to reduce branching
CBufferedRNG_template_decl __forceinline void CBufferedRNG_template_inst::Generate(OutputType* data, int count) // copy vector data to memory
{
	while (count > 0)
	{
		const int n = Min<int>(count, m_count - m_index);

		for (int i = 0; i < n; i++) // unroll this?
		{
			*(data++) = ConverterClass::ConvertDst(m_buffer[m_index++]);
		}

		count -= n;

		if (count > 0)
		{
			Fill();
		}
	}
}

CBufferedRNG_template_decl __forceinline void CBufferedRNG_template_inst::Fill()
{
	DataType* dst0 = (DataType*)&m_buffer[0];
	DataType* dst1 = (DataType*)&m_buffer[m_count];

	// note that sizeof(DataType) may be larger or smaller than sizeof(OutputType)
	// i.e. we want to fill the buffer starting at 'dst0' and ending before 'dst1', but
	// there may be more or less than m_count elements

#if !__PS3 // optional loop unrolling
#define DEF_UNROLLCODE(i,args) ARG(0,args)[i] = ARG(1,args)
#define DEF_UNROLL(n,T,ptr,end,code) \
	for (const T* dst##n = &end[-(n)]; ptr <= dst##n; ptr += (n)) \
	{ \
		REP##n##_SEPARATOR_FOREACH_ARGS(UNROLLCODE,;,(ptr,code)); \
	} \
	// end.

	DEF_UNROLL(8,DataType,dst0,dst1,ConverterClass::ConvertSrc(RNG::Generate()));
	DEF_UNROLL(4,DataType,dst0,dst1,ConverterClass::ConvertSrc(RNG::Generate()));
	DEF_UNROLL(2,DataType,dst0,dst1,ConverterClass::ConvertSrc(RNG::Generate()));

#undef DEF_UNROLLCODE
#undef DEF_UNROLL
#endif

	/*
	simpler version which does not use ARG() macros .. however DEF_UNROLL cannot be shared

#if !__PS3 // optional loop unrolling
#define DEF_UNROLLCODE(i) dst0[i] = ConverterClass::ConvertSrc(RNG::Generate())
#define DEF_UNROLL(n,T,ptr,end) \
	for (const DataType* dst##n = &dst1[-(n)]; dst0 <= dst##n; dst0 += (n)) \
	{ \
		REP##n##_SEPARATOR_FOREACH(UNROLLCODE,;); \
	} \
	// end.

	DEF_UNROLL(8);
	DEF_UNROLL(4);
	DEF_UNROLL(2);

#undef DEF_UNROLLCODE
#undef DEF_UNROLL
#endif
	*/

	while (dst0 < dst1)
	{
		*(dst0++) = ConverterClass::ConvertSrc(RNG::Generate());
	}

	m_index = 0;
}

#undef CBufferedRNG_template_decl
#undef CBufferedRNG_template_inst

#if !__FINAL

namespace _test_CBufferedRNG
{
	void test();
}

#endif // !_FINAL

// ================================================================================================

#ifndef SAFE_RELEASE
#define SAFE_RELEASE(x) if ((x) != NULL) { (x)->Release(); (x) = NULL; }
#endif
#ifndef SAFE_DELETE
#define SAFE_DELETE(ptr)  { if (ptr != NULL) { delete ptr;     ptr = NULL; } }
#endif
#ifndef SAFE_DELETE_ARRAY
#define SAFE_DELETE_ARRAY(ptr)  { if (ptr != NULL) { delete[] ptr;     ptr = NULL; } }
#endif

#define ADD_WIDGET( bk,type,var,    ...) bk.Add##type(STRING(var)    , &var, ##__VA_ARGS__)
#define ADD_WIDGET2(bk,type,var,ext,...) bk.Add##type(STRING(var) ext, &var, ##__VA_ARGS__)

// ================================================================================================

__forceinline Vector4 BuildPlane(const Vector3& p, const Vector3& n)
{
	Vector4 plane = n;
	plane.w = -n.Dot(p);
	return plane;
}

__forceinline Vector4 BuildPlane(const Vector3& p0, const Vector3& p1, const Vector3& p2)
{
	Vector3 n;
	n.Cross(p1 - p0, p2 - p0);
	n.Normalize();
	return BuildPlane(p0, n);
}

__forceinline float PlaneDistanceTo(const Vector4& plane, const Vector3& p)
{
	return plane.GetVector3().Dot(p) + plane.w;
}

__forceinline Vector3 PlaneProject(const Vector4& plane, const Vector3& p)
{
	return p - plane.GetVector3()*PlaneDistanceTo(plane, p);
}

__forceinline Vector4 PlaneNormalise(const Vector4& plane)
{
	return plane*(1.0f/plane.GetVector3().Mag());
}

namespace rage {
namespace Vec {

__forceinline Vector_4V_Out _V3FindMinAbsAxis(Vector_4V_In v) // nice vector-friendly implementation
{
	const Vector_4V xyz           = V4Abs               (v);
	const Vector_4V yzx           = V4Permute<Y,Z,X,W>  (xyz);
	const Vector_4V zxy           = V4Permute<Z,X,Y,W>  (xyz);
	const Vector_4V xyz_CmpLT_yzx = V4IsLessThanV       (xyz, yzx);
	const Vector_4V xyz_CmpLE_zxy = V4IsLessThanOrEqualV(xyz, zxy);
	const Vector_4V a             = V4And               (xyz_CmpLT_yzx, xyz_CmpLE_zxy);
	const Vector_4V ax            = V4SplatX            (a);
	const Vector_4V ay            = V4SplatY            (a);
	const Vector_4V az            = V4SplatZ            (a);
	const Vector_4V axy           = V4Or                (ax, ay);
	const Vector_4V axyz          = V4Or                (axy, az); // could use si_orx(a) on SPU
	const Vector_4V mask          = V4Andc              (V4VConstant(V_MASKX), axyz); // 0xffffffff in x component iff a.xyz = 0,0,0
	const Vector_4V b             = V4Or                (a, mask);
	const Vector_4V result        = V4And               (b, V4VConstant(V_ONE));

	return result;
}

} // namespace Vec
} // namespace rage

__forceinline Vec3V_Out FindMinAbsAxis(Vec3V_In v)
{
	return Vec3V(Vec::_V3FindMinAbsAxis(v.GetIntrin128ConstRef()));
}

__forceinline Vector3 FindMinAbsAxis(const Vector3& v)
{
#if 1

	return Vector3(Vec::_V3FindMinAbsAxis(v));

#else // reference implementation, arranged to show vector operations

	const Vector3 xyz = Vector3(Abs<float>(v.x), Abs<float>(v.y), Abs<float>(v.z));
	const Vector3 yzx = Vector3(xyz.y, xyz.z, xyz.x);
	const Vector3 zxy = Vector3(xyz.z, xyz.x, xyz.y);

	const Vector3 xyz_CmpLT_yzx = Vector3(xyz.x <  yzx.x ? 1.0f : 0.0f, xyz.y <  yzx.y ? 1.0f : 0.0f, xyz.z <  yzx.z ? 1.0f : 0.0f);
	const Vector3 xyz_CmpLE_zxy = Vector3(xyz.x <= zxy.x ? 1.0f : 0.0f, xyz.y <= zxy.y ? 1.0f : 0.0f, xyz.z <= zxy.z ? 1.0f : 0.0f);

	Vector3 a = xyz_CmpLT_yzx*xyz_CmpLE_zxy; // vector-AND, mask with 1.0f

	a.x += (a.x == 0 && a.y == 0 && a.z == 0) ? 1.0f : 0.0f;

	return a;

#endif
}

#if 0
__forceinline Vector3 FindMinAbsAxis_REFERENCE(const Vector3& v)
{
	const float x = Abs<float>(v.x);
	const float y = Abs<float>(v.y);
	const float z = Abs<float>(v.z);

	if (x < y && x <= z) return Vector3(1,0,0);
	if (y < z && y <= x) return Vector3(0,1,0);
	if (z < x && z <= y) return Vector3(0,0,1);

	return Vector3(1,0,0);
}

__forceinline void FindMinAbsAxis_TEST()
{
	int numErrors = 0;

	for (float z = -3.0f; z <= 3.0f; z += 1.0f)
	{
		for (float y = -3.0f; y <= 3.0f; y += 1.0f)
		{
			for (float x = -3.0f; x <= 3.0f; x += 1.0f)
			{
				const Vector3 a = FindMinAbsAxis          (Vector3(x,y,z));
				const Vector3 b = FindMinAbsAxis_REFERENCE(Vector3(x,y,z));

				const float a_sum = a.x + a.y + a.z;

				if (a_sum != 1.0f || a.x != b.x || a.y != b.y || a.z != b.z)
				{
					Displayf(
						"v=(%d,%d,%d), FindMinAxis=(%d,%d,%d), REF=(%d,%d,%d)",
						(int)x,
						(int)y,
						(int)z,
						(int)a.x,
						(int)a.y,
						(int)a.z,
						(int)b.x,
						(int)b.y,
						(int)b.z
					);

					numErrors++;
				}
			}
		}
	}

	Displayf("FindMinAbsAxis_TEST: %d errors", numErrors);
}
#endif

#endif // _RENDERER_UTIL_UTIL_H_
