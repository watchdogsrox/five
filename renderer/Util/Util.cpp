// ======================
// renderer/util/util.cpp
// (c) 2010 RockstarNorth
// ======================

#include "math/random.h"
#include "system/memops.h"
#include "system/timer.h"

#include "fwutil/xmacro.h"

#include "renderer/util/util.h"

#if !__NO_OUTPUT

//OPTIMISATIONS_OFF();

namespace rage {

// horrifically slow ..
Vec3V_Out  _MakeRandomVec3();
Vec4V_Out  _MakeRandomVec4();
Mat33V_Out _MakeRandomMat33(bool bOrtho);
Mat34V_Out _MakeRandomMat34(bool bOrtho);

Vec3V_Out _MakeRandomVec3()
{
	Vec3V v;
	v.SetX(-1.0f + 2.0f*(float)rand()/(float)RAND_MAX);
	v.SetY(-1.0f + 2.0f*(float)rand()/(float)RAND_MAX);
	v.SetZ(-1.0f + 2.0f*(float)rand()/(float)RAND_MAX);
	return v;
}

Vec4V_Out _MakeRandomVec4()
{
	Vec4V v;
	v.SetX(-1.0f + 2.0f*(float)rand()/(float)RAND_MAX);
	v.SetY(-1.0f + 2.0f*(float)rand()/(float)RAND_MAX);
	v.SetZ(-1.0f + 2.0f*(float)rand()/(float)RAND_MAX);
	v.SetW(-1.0f + 2.0f*(float)rand()/(float)RAND_MAX);
	return v;
}

Mat33V_Out _MakeRandomMat33(bool bOrtho)
{
	Mat33V m;
	//m.SetIdentity();

	for (int i = 0; i < 100; i++) // shouldn't loop more than a few times ..
	{
		Vec3V mx,my,mz;

		if (bOrtho) // or, could make a random normalised quaternion ..
		{
			mx = _MakeRandomVec3();
			my = _MakeRandomVec3();
			mz = Cross(mx, my);
			my = Cross(mz, mx);

			const float magsq_x = MagSquared(mx).Getf();
			const float magsq_y = MagSquared(my).Getf();
			const float magsq_z = MagSquared(mz).Getf();

			if (Min<float>(magsq_x, magsq_y, magsq_z) < 0.01f)
			{
				continue;
			}

			mx = Normalize(mx);
			my = Normalize(my);
			mz = Normalize(mz);

			m.SetCols(mx, my, mz);
			break;
		}
		else
		{
			mx = _MakeRandomVec3();
			my = _MakeRandomVec3();
			mz = _MakeRandomVec3();

			/*
			const float magsq_x = MagSquared(mx).Getf();
			const float magsq_y = MagSquared(my).Getf();
			const float magsq_z = MagSquared(mz).Getf();

			if (Min<float>(magsq_x, magsq_y, magsq_z) < 0.01f)
			{
				continue;
			}

			const float dotv_xy = Dot(mx, my).Getf();
			const float dotv_yz = Dot(my, mz).Getf();
			const float dotv_zx = Dot(mz, mx).Getf();
			const float skew_xy = 1.0f - (dotv_xy*dotv_xy)/(magsq_x*magsq_y);
			const float skew_yz = 1.0f - (dotv_yz*dotv_yz)/(magsq_y*magsq_z);
			const float skew_zx = 1.0f - (dotv_zx*dotv_zx)/(magsq_z*magsq_x);

			if (Min<float>(skew_xy, skew_yz, skew_zx) < 0.01f)
			{
				continue;
			}
			*/

			const float det = Dot(mx, Cross(my, mz)).Getf();

			if (det*det < 0.01f)
			{
				continue;
			}

			m.SetCols(mx, my, mz);
			break;
		}
	}

	return m;
}

Mat34V_Out _MakeRandomMat34(bool bOrtho)
{
	Mat33V m33 = _MakeRandomMat33(bOrtho);
	Mat34V m34;

	m34.SetCols(
		m33.GetCol0(),
		m33.GetCol1(),
		m33.GetCol2(),
		_MakeRandomVec3()
	);

	return m34;
}

} // namespace rage

// ================================================================================================
// ================================================================================================

template <typename T> class Vec__Vector_4V_proxy // shit this is in vecrand.cpp now ..
{
public:
	__forceinline Vec__Vector_4V_proxy() {}
	__forceinline Vec__Vector_4V_proxy(Vec::Vector_4V_In v_) : m_v128(v_) {}

	__forceinline Vec__Vector_4V_proxy(T x, T y)
	{
		CompileTimeAssert(sizeof(Vec::Vector_4V) == 2*sizeof(T));
		m_v[0] = x;
		m_v[1] = y;
	}
	__forceinline Vec__Vector_4V_proxy(T x, T y, T z, T w)
	{
		CompileTimeAssert(sizeof(Vec::Vector_4V) == 4*sizeof(T));
		m_v[0] = x;
		m_v[1] = y;
		m_v[2] = z;
		m_v[3] = w;
	}
	__forceinline Vec__Vector_4V_proxy(T x0, T y0, T z0, T w0, T x1, T y1, T z1, T w1)
	{
		CompileTimeAssert(sizeof(Vec::Vector_4V) == 4*sizeof(T));
		m_v[0] = x0;
		m_v[1] = y0;
		m_v[2] = z0;
		m_v[3] = w0;
		m_v[4] = x1;
		m_v[5] = y1;
		m_v[6] = z1;
		m_v[7] = w1;
	}

	__forceinline operator Vec::Vector_4V_Out() const { return m_v128; }

	__forceinline       T& operator[](int i)       { return m_v[i]; }
	__forceinline const T& operator[](int i) const { return m_v[i]; }

private:
	union
	{
		Vec::Vector_4V m_v128;
		T              m_v[sizeof(Vec::Vector_4V)/sizeof(T)];
	};
};


template <int shift> void _test_V4Shift(u32 ASSERT_ONLY(x0), u32 ASSERT_ONLY(x1), u32 ASSERT_ONLY(x2), u32 ASSERT_ONLY(x3))
{
#if __ASSERT
	using namespace Vec;
	
	const Vec__Vector_4V_proxy<u32> v0(x0, x1, x2, x3);

	// test shift by immediate

	{
		const Vec__Vector_4V_proxy<u32> v1 = V4ShiftLeft          <shift>(v0);
		const Vec__Vector_4V_proxy<u32> v2 = V4ShiftRight         <shift>(v0);
		const Vec__Vector_4V_proxy<s32> v3 = V4ShiftRightAlgebraic<shift>(v0);

		for (int i = 0; i < 4; i++)
		{
			Assertf((u32)v1[i] == (u32)v0[i] << shift, "_test_V4Shift (imm) error! 0x%08x << %d, expected 0x%08x, got 0x%08x", (u32)v0[i], shift, (u32)v0[i] << shift, (u32)v1[i]);
			Assertf((u32)v2[i] == (u32)v0[i] >> shift, "_test_V4Shift (imm) error! 0x%08x >> %d, expected 0x%08x, got 0x%08x", (u32)v0[i], shift, (u32)v0[i] >> shift, (u32)v2[i]);
			Assertf((s32)v3[i] == (s32)v0[i] >> shift, "_test_V4Shift (imm) error! 0x%08x >> %d, expected 0x%08x, got 0x%08x", (s32)v0[i], shift, (s32)v0[i] >> shift, (s32)v3[i]);
		}													                                                                                                                                     
	}

	// test shift by vector
	{
#if __WIN32PC || RSG_DURANGO || RSG_ORBIS // SSE only allows you to shift all elements by the amount in x, and yzw should be zero
		const Vec__Vector_4V_proxy<u32> vs(3,0,0,0);
#else
		const Vec__Vector_4V_proxy<u32> vs(3,0,2,5);
#endif
		const Vec__Vector_4V_proxy<u32> v1 = V4ShiftLeft          (v0, vs);
		const Vec__Vector_4V_proxy<u32> v2 = V4ShiftRight         (v0, vs);
		const Vec__Vector_4V_proxy<s32> v3 = V4ShiftRightAlgebraic(v0, vs);

		for (int i = 0; i < 4; i++)
		{
			Assertf((u32)v1[i] == (u32)v0[i] << vs[i], "_test_V4Shift (vec) error! 0x%08x << %d, expected 0x%08x, got 0x%08x", (u32)v0[i], vs[i], (u32)v0[i] << vs[i], (u32)v1[i]);
			Assertf((u32)v2[i] == (u32)v0[i] >> vs[i], "_test_V4Shift (vec) error! 0x%08x >> %d, expected 0x%08x, got 0x%08x", (u32)v0[i], vs[i], (u32)v0[i] >> vs[i], (u32)v2[i]);
			Assertf((s32)v3[i] == (s32)v0[i] >> vs[i], "_test_V4Shift (vec) error! 0x%08x >> %d, expected 0x%08x, got 0x%08x", (s32)v0[i], vs[i], (s32)v0[i] >> vs[i], (s32)v3[i]);
		}
	}
#endif // __ASSERT
}

void _test_V4Shift()
{
	const u32 x0 = 0;
	const u32 x1 = 1;
	const u32 x2 = 12345;
	const u32 x3 = 0x80100107;

	_test_V4Shift<0x00>(x0, x1, x2, x3);
	_test_V4Shift<0x01>(x0, x1, x2, x3);
	_test_V4Shift<0x02>(x0, x1, x2, x3);
	_test_V4Shift<0x03>(x0, x1, x2, x3);
	_test_V4Shift<0x04>(x0, x1, x2, x3);
	_test_V4Shift<0x05>(x0, x1, x2, x3);
	_test_V4Shift<0x06>(x0, x1, x2, x3);
	_test_V4Shift<0x07>(x0, x1, x2, x3);
	_test_V4Shift<0x08>(x0, x1, x2, x3);
	_test_V4Shift<0x09>(x0, x1, x2, x3);
	_test_V4Shift<0x0a>(x0, x1, x2, x3);
	_test_V4Shift<0x0b>(x0, x1, x2, x3);
	_test_V4Shift<0x0c>(x0, x1, x2, x3);
	_test_V4Shift<0x0d>(x0, x1, x2, x3);
	_test_V4Shift<0x0e>(x0, x1, x2, x3);
	_test_V4Shift<0x0f>(x0, x1, x2, x3);
	_test_V4Shift<0x10>(x0, x1, x2, x3);
	_test_V4Shift<0x11>(x0, x1, x2, x3);
	_test_V4Shift<0x12>(x0, x1, x2, x3);
	_test_V4Shift<0x13>(x0, x1, x2, x3);
	_test_V4Shift<0x14>(x0, x1, x2, x3);
	_test_V4Shift<0x15>(x0, x1, x2, x3);
	_test_V4Shift<0x16>(x0, x1, x2, x3);
	_test_V4Shift<0x17>(x0, x1, x2, x3);
	_test_V4Shift<0x18>(x0, x1, x2, x3);
	_test_V4Shift<0x19>(x0, x1, x2, x3);
	_test_V4Shift<0x1a>(x0, x1, x2, x3);
	_test_V4Shift<0x1b>(x0, x1, x2, x3);
	_test_V4Shift<0x1c>(x0, x1, x2, x3);
	_test_V4Shift<0x1d>(x0, x1, x2, x3);
	_test_V4Shift<0x1e>(x0, x1, x2, x3);
	_test_V4Shift<0x1f>(x0, x1, x2, x3);
}

// ================================================================================================

template <typename T> __forceinline float RNG_GetFloat(T (*func)(T), T& z)
{
	z = func(z);

	const u32 x = (u32)(z);
	const u32 i = (x >> 9) | 0x3f800000;

	return (*(const float*)&i) - 1.0f;
}

void _test_Pattern()
{
#if 0
	using namespace Vec;
	using namespace rng_util;

	const int sizeX = 1024;
	const int sizeY = 1024;

	CPixel24* image = new CPixel24[sizeX*sizeY];

	// XorShift31
	{
		Vector_4V v = V4VConstant<1,1,1,1>();

		for (int y = 0; y < sizeY; y++)
		{
			for (int x = 0; x < sizeX; x++)
			{
				const float r = V4Rand(v).m128_f32[0];
				const float g = V4Rand(v).m128_f32[1];
				const float b = V4Rand(v).m128_f32[2];

				image[x + y*sizeX].R = (u8)(r*255.0f + 0.5f);
				image[x + y*sizeX].G = (u8)(g*255.0f + 0.5f);
				image[x + y*sizeX].B = (u8)(b*255.0f + 0.5f);
			}
		}

		SaveImage("images\\TestPattern_XorShift31.png", image, sizeX, sizeY, 1, true, false);
	}

	if (0) // XorShift31 - xy
	{
		sysMemSet(image, 0, sizeX*sizeY*sizeof(CPixel24));

		Vector_4V v = V4VConstant<1,1,1,1>();

		for (int y = 0; y < sizeY/4; y++)
		{
			for (int x = 0; x < sizeX/4; x++)
			{
				const int xx = (int)(V4Rand(v).m128_f32[0]*(float)(sizeX) + 0.5f);
				const int yy = (int)(V4Rand(v).m128_f32[0]*(float)(sizeY) + 0.5f);

				image[xx + yy*sizeX].R = 255;
				image[xx + yy*sizeX].G = 255;
				image[xx + yy*sizeX].B = 255;
			}
		}

		SaveImage("images\\TestPattern_XorShift31_xy.png", image, sizeX, sizeY, 1, true, false);
	}

	// XorShift61
	{
		u64 z = (u64)0xDEADC0DE * 0xC0FFEE;

		for (int y = 0; y < sizeY; y++)
		{
			for (int x = 0; x < sizeX; x++)
			{
				const float r = RNG_GetFloat(XorShiftRLR<61,XorShiftRLR61_ARGS>, z);
				const float g = RNG_GetFloat(XorShiftRLR<61,XorShiftRLR61_ARGS>, z);
				const float b = RNG_GetFloat(XorShiftRLR<61,XorShiftRLR61_ARGS>, z);

				image[x + y*sizeX].R = (u8)(r*255.0f + 0.5f);
				image[x + y*sizeX].G = (u8)(g*255.0f + 0.5f);
				image[x + y*sizeX].B = (u8)(b*255.0f + 0.5f);
			}
		}

		SaveImage("images\\TestPattern_XorShift61.png", image, sizeX, sizeY, 1, true, false);
	}

	// MWC
	{
		u64 z = (u64)0xDEADC0DE * 0xC0FFEE;

		for (int y = 0; y < sizeY; y++)
		{
			for (int x = 0; x < sizeX; x++)
			{
				const float r = RNG_GetFloat(MWC, z);
				const float g = RNG_GetFloat(MWC, z);
				const float b = RNG_GetFloat(MWC, z);

				image[x + y*sizeX].R = (u8)(r*255.0f + 0.5f);
				image[x + y*sizeX].G = (u8)(g*255.0f + 0.5f);
				image[x + y*sizeX].B = (u8)(b*255.0f + 0.5f);
			}
		}

		SaveImage("images\\TestPattern_MWC.png", image, sizeX, sizeY, 1, true, false);
	}

	delete[] image;
#endif
}

// ================================================================================================
// weird XorShift patterns .. these only seem to occur with non-prime periods

void _test_StrangeXorShiftPattern()
{
#if 0
	using namespace Vec;
	using namespace rng_util;

	typedef u16 T;

	CPixel24* image = new CPixel24[256*256];

	T* data1 = new T[256*256];
	T* data2 = new T[256*256];

	const int xxx = 257; // also 255 leads to patterns, not as obvious ..

	for (int pass = 0; pass < 2; pass++)
	{
		const int xxx = (pass == 0) ? 257 : 255;

		T x = 1;

		for (int i = 0; i < 256*256; i++) // populate
		{
			data1[i] = x = XorShiftLRL<16,7,9,13>(x);
		}

		sysMemSet(data2, 0, 256*256*sizeof(T));

		for (int j = 0; j < 16; j++)
		{
			for (int i = 0; i < 16; i++)
			{
				for (int jj = 0; jj < 16; jj++)
				{
					for (int ii = 0; ii < 16; ii++)
					{
						int dstIndex = (i*16 + ii + (j*16 + jj)*(256)) & 0xffffu;
						int srcIndex = (i + j*16 + (ii + jj*16)*(xxx)) & 0xffffu; 

						data2[dstIndex] = data1[srcIndex];
					}
				}
			}
		}

		sysMemCpy(data1, data2, 256*256*sizeof(T));

		for (int i = 0; i < 16; i++)
		{
			for (int y = 0; y < 256; y++)
			{
				for (int x = 0; x < 256; x++)
				{
					image[x + y*256].R = (data1[(x&255) + (y&255)*256] & (1<<i)) ? 255 : 0;
					image[x + y*256].G = image[x + y*256].R;
					image[x + y*256].B = image[x + y*256].R;
				}
			}

			SaveImage(CString("images\\TestPattern_XorShift16_7_9_13_diag%03d_bit%02d.png", xxx, i).c_str(), image, 256, 256, 4, true, false);
		}
	}

	delete[] data1;
	delete[] data2;

	delete[] image;
#endif
}

#endif // !__NO_OUTPUT

// ================================================================================================

#if !__FINAL

namespace _test_CBufferedRNG {

__forceinline u32 LCG32(u32 x)
{
	return x*1664525 + 1013904223;
}

__forceinline u64 MWC64(u64 x)
{
	const u32 a = (u32)(x);
	const u32 b = (u32)(x >> 32);

	return 0x5CDCFAA7ull*a + b;
}

// http://en.wikipedia.org/wiki/Random_number_generation
// this is much more easily vectorisable as it only uses 16x16->32 bit multiplies
__forceinline u32 MWC64_wiki(u32& z, u32& w)
{
	z = 36969 * (z & 65535) + (z >> 16);
	w = 18000 * (w & 65535) + (w >> 16);
	return (z << 16) + w;
}

#if __PPU // only PPU has mulo and mule intrinsics ..

// http://en.wikipedia.org/wiki/Random_number_generation
// THIS CODE IS UNTESTED
__forceinline Vec::Vector_4V_Out MWC_(Vec::Vector_4V_InOut state) // state is [z0,w0,z1,w1]
{
	//z = 36969 * (z & 65535) + (z >> 16);
	//w = 18000 * (w & 65535) + (w >> 16);
	//return (z << 16) + w;

	const Vec::Vector_4V coefs = Vec::V4VConstant<36969,18000,36969,18000>();

	Vec::Vector_4V temp0,temp1,temp2,temp3,temp4;

	temp0 = state;
	temp1 = (Vec::Vector_4V)vec_mulo((Vec::Vector_4V_ushort)temp0, (Vec::Vector_4V_ushort)coefs);
	temp1 = Vec::V4AddInt(temp1, Vec::V4ShiftRight<16>(temp0)); // [z0,w0,z1,w1]
	temp2 = (Vec::Vector_4V)vec_mule((Vec::Vector_4V_ushort)temp1, (Vec::Vector_4V_ushort)coefs);
	temp2 = Vec::V4AddInt(temp2, Vec::V4ShiftRight<16>(temp1)); // [z2,w2,z3,w3]
	state = temp2;
	temp3 = Vec::V4MergeXY(temp1, temp2); // [z0,z2,w0,w2]
	temp4 = Vec::V4MergeZW(temp1, temp2); // [z1,z3,w0,w3]
	temp1 = Vec::V4MergeXY(temp3, temp4); // [z0,z1,z2,z3]
	temp2 = Vec::V4MergeZW(temp3, temp4); // [w0,w1,w2,w3]
	temp1 = Vec::V4ShiftLeft<16>(temp1);
	temp2 = Vec::V4AddInt(temp2, temp1);
	return temp2;
}

#endif

// ================================================================================================
// XorShift advantages:
// - can compute skip-ahead matrix for any increment
// - operations trivially vectorisable on all platforms
// - really really fast
/*
template <int a, int b, int c> __forceinline u32 XorShiftRLR31(u32 x) // e.g. XorShiftRLR31<XorShiftRLR31_ARGS>
{
	x ^= (x >> a);
	x ^= (x << b); x &= ~0x80000000;
	x ^= (x >> c);

	return x;
}
*/
template <int a, int b, int c> __forceinline u32 rng_util__XorShiftRLR31(u32 x) // shit this is in vecrand.cpp now ..
{
	x ^= (x >> a);
	x ^= (x << b); x &= 0x7fffffff;
	x ^= (x >> c);

	return x;
}

template <int a, int b, int c> __forceinline u32 rng_util__XorShiftLRL32(u32 x) // shit this is in vecrand.cpp now ..
{
	x ^= (x << a);
	x ^= (x >> b);
	x ^= (x << c);

	return x;
}

template <int a, int b, int c> __forceinline Vec::Vector_4V_Out XorShiftRLR31(Vec::Vector_4V_In v)
{
	const Vec::Vector_4V mask = Vec::V4VConstantSplat<0x80000000>();

	Vec::Vector_4V x = v;

	x = Vec::V4Xor (x, Vec::V4ShiftRight<a>(x)); // Mersenne-prime XorShiftRLR (period 2^^31-1)
	x = Vec::V4Xor (x, Vec::V4ShiftLeft <b>(x));
	x = Vec::V4Andc(x, mask);
	x = Vec::V4Xor (x, Vec::V4ShiftRight<c>(x));

	return x;
}

// from http://www.jstatsoft.org/v08/i14/paper
__forceinline u32 _Marsaglia_xor128()
{
	static u32 x = 123456789;
	static u32 y = 362436069;
	static u32 z = 521288629;
	static u32 w = 88675123;

	u32 t;
	t = x ^ (x << 11);
	x = y; y = z; z = w;
	return w = w ^ (w >> 19) ^ (t ^ (t >> 8));
}

__forceinline u32 _Marsaglia_mwc()
{
	static u32 x = 123456789;
	static u32 y = 362436069;
	static u32 z = 77465321;
	static u32 c = 13579;

	u64 t;
	t = 916905990LL*x + c;
	x = y; y = z; c = (t >> 32);
	return z = (t & 0xffffffff);
}

// http://cliodhna.cop.uop.edu/~hetrick/na_faq.html
__forceinline u64 MWC(u64 x)
{
	const u32 a = (u32)(x);
	const u32 b = (u32)(x >> 32);

	return 0x5CDCFAA7ull*a + b;
}

// ================================================================================================
// ==================

class LCG32_generator
{
public:
	typedef u32 DataType;
	typedef u32 SeedType;

	__forceinline LCG32_generator(SeedType seed) { Reset(seed); }
	__forceinline void Reset(SeedType seed) { m_state = seed; }

	__forceinline DataType Generate() { m_state = LCG32(m_state); return m_state; }

private:
	u32 m_state;
};

class MWC64_generator
{
public:
	typedef u32 DataType;
	typedef u64 SeedType;

	__forceinline MWC64_generator(SeedType seed) { Reset(seed); }
	__forceinline void Reset(SeedType seed) { m_state = seed; }

	__forceinline DataType Generate() { m_state = MWC64(m_state); return (DataType)m_state; }

private:
	u64 m_state;
};

class MWC64_wiki_generator // http://en.wikipedia.org/wiki/Random_number_generation
{
public:
	typedef u32 DataType;
	typedef u32 SeedType;

	__forceinline MWC64_wiki_generator(SeedType seed) { Reset(seed); }
	__forceinline void Reset(SeedType seed) { m_z = seed; m_w = seed; }

	__forceinline DataType Generate()
	{
		m_z = 36969 * (m_z & 65535) + (m_z >> 16);
		m_w = 18000 * (m_w & 65535) + (m_w >> 16);
		return (m_z << 16) + m_w;
	}

private:
	u32 m_z;
	u32 m_w;
};

class LFSR113_generator // http://www.lomont.org/Math/Papers/2008/Lomont_PRNG_2008.pdf
{
public:
	typedef u32 DataType;
	typedef u32 SeedType;

	__forceinline LFSR113_generator(SeedType seed) { Reset(seed); }
	__forceinline void Reset(SeedType seed) { z1 = seed | 3; z2 = z3 = z4 = 77778; }

	__forceinline DataType Generate()
	{
		u32 temp;

		temp = (((z1 <<  6) ^ z1) >> 13); z1 = (((z1 & 0xFFFFFFFE) << 18) ^ temp);
		temp = (((z2 <<  2) ^ z2) >> 27); z2 = (((z2 & 0xFFFFFFF8) <<  2) ^ temp);
		temp = (((z3 << 13) ^ z3) >> 21); z3 = (((z3 & 0xFFFFFFF0) <<  7) ^ temp);
		temp = (((z4 <<  3) ^ z4) >> 12); z4 = (((z4 & 0xFFFFFF80) << 13) ^ temp);

		return (z1 ^ z2 ^ z3 ^ z4);
	}

private:
	u32 z1,z2,z3,z4; // must satisfy z1 > 1, z2 > 7, z3 > 15, z4 > 127
};

class XorShiftRLR31_generator
{
public:
	typedef u32 DataType;
	typedef u32 SeedType;

	__forceinline XorShiftRLR31_generator(SeedType seed) { Reset(seed); }
	__forceinline void Reset(SeedType seed) { m_state = seed; }

	__forceinline DataType Generate() { m_state = rng_util__XorShiftRLR31<XorShiftRLR31_ARGS>(m_state); return (DataType)m_state; }

private:
	u32 m_state;
};

class XorShiftVector_generator
{
public:
	typedef Vec::Vector_4V DataType;
	typedef u32            SeedType;

	__forceinline XorShiftVector_generator(SeedType seed)
	{
		Reset(seed);
	}

	__forceinline void Reset(SeedType seed)
	{
		const u32 s0 = seed;
		const u32 s1 = rng_util__XorShiftRLR31<XorShiftRLR31_ARGS>(s0);
		const u32 s2 = rng_util__XorShiftRLR31<XorShiftRLR31_ARGS>(s1);
		const u32 s3 = rng_util__XorShiftRLR31<XorShiftRLR31_ARGS>(s2);

		m_state = Vec__Vector_4V_proxy<SeedType>(s0, s1, s2, s3);
	}

	__forceinline Vec::Vector_4V_Out Generate()
	{
		m_state = XorShiftRLR31<XorShiftRLR31_ARGS>(m_state);
		return (DataType)m_state;
	}

private:
	Vec::Vector_4V m_state;
};

static mthRandom g_test_mthRandom;

__forceinline Vec::Vector_4V _test_MWC64_generator_generate(u64& seed)
{
	UNUSED_VAR(seed);
	return g_test_mthRandom.Get4FloatsV().GetIntrin128();

	//const float scale = (float)(1.0/4294967296.0);
	//const float x = scale*(float)(u32)MWC64(seed);
	//const float y = scale*(float)(u32)MWC64(seed);
	//const float z = scale*(float)(u32)MWC64(seed);
	//const float w = scale*(float)(u32)MWC64(seed);
	//Vec::Vector_4V result;
	//Vec::V4Set(result, x, y, z, w);
	//return result;
}

void test()
{
	if (1) // performance test
	{
		for (int i = 0; i < 4; i++)
		{
			CBufferedRNG<Vec::Vector_4V,MWC64_generator> gen1(12345, 256);
			Vec::Vector_4V v = Vec::V4VConstant(V_ZERO);
			u64 seed = 777;
			sysTimer t;

			Displayf("starting tests ..");
			{
				t.Reset();

				for (int i = 0; i < 100000; i++)
				{
					v = Vec::V4Add(v, gen1.Generate()); v = Vec::V4NormalizeFast(v);
					v = Vec::V4Add(v, gen1.Generate()); v = Vec::V4NormalizeFast(v);
					v = Vec::V4Add(v, gen1.Generate()); v = Vec::V4NormalizeFast(v);
					v = Vec::V4Add(v, gen1.Generate()); v = Vec::V4NormalizeFast(v);
				}
			}
			Displayf("test 1 took %f usecs (%f)", t.GetUsTimeAndReset(), Vec::GetX(v));
			{
				t.Reset();

				for (int i = 0; i < 100000; i++)
				{
					v = Vec::V4Add(v, _test_MWC64_generator_generate(seed)); v = Vec::V4NormalizeFast(v);
					v = Vec::V4Add(v, _test_MWC64_generator_generate(seed)); v = Vec::V4NormalizeFast(v);
					v = Vec::V4Add(v, _test_MWC64_generator_generate(seed)); v = Vec::V4NormalizeFast(v);
					v = Vec::V4Add(v, _test_MWC64_generator_generate(seed)); v = Vec::V4NormalizeFast(v);
				}
			}
			Displayf("test 2 took %f usecs (%f)", t.GetUsTimeAndReset(), Vec::GetX(v));
			Displayf("");
		}
	}
	else
	{
		CBufferedRNG<Vec::Vector_4V,LCG32_generator         > bgv1(12345, 4);
		CBufferedRNG<Vec::Vector_4V,MWC64_generator         > bgv2(12345, 4);
		CBufferedRNG<Vec::Vector_4V,MWC64_wiki_generator    > bgv3(12345, 4);
		CBufferedRNG<Vec::Vector_4V,LFSR113_generator       > bgv4(12345, 4);
		CBufferedRNG<Vec::Vector_4V,XorShiftRLR31_generator > bgv5(12345, 4);
		CBufferedRNG<Vec::Vector_4V,XorShiftVector_generator> bgv6(12345, 4);

		CBufferedRNG<float,LCG32_generator         > bgf1(12345, 4);
		CBufferedRNG<float,MWC64_generator         > bgf2(12345, 4);
		CBufferedRNG<u32  ,MWC64_wiki_generator    > bgf3(12345, 4);
		CBufferedRNG<u32  ,LFSR113_generator       > bgf4(12345, 4);
		CBufferedRNG<float,XorShiftRLR31_generator > bgf5(12345, 4);
		CBufferedRNG<float,XorShiftVector_generator> bgf6(12345, 4);

		if (1) for (int i = 0; i < 10; i++)
		{
			const Vec__Vector_4V_proxy<f32> v = bgv6.Generate();

			Displayf("v=%f,%f,%f,%f\n", v[0], v[1], v[2], v[3]);
		}

		if (1) for (int i = 0; i < 10; i++)
		{
			const float v = bgf6.Generate();

			Displayf("v=%f\n", v);
		}

		if (1) for (int i = 0; i < 10; i++)
		{
			const u32 v = bgf4.Generate();

			Displayf("v=0x%08x\n", v);
		}
	}
}

} // namespace _test_CBufferedRNG

#if 0

namespace ScalarVArray_ {

	template <u32 i, u32 i_and_3> __forceinline ScalarV_Out GetInternal(const Vec4V* m_v);

	template <u32 i, u32> __forceinline ScalarV_Out GetInternal<i,0>(const Vec4V* m_v) { CompileTimeAssert((i&3) == 0); return m_v[i/4].GetX(); }
	template <u32 i, u32> __forceinline ScalarV_Out GetInternal<i,1>(const Vec4V* m_v) { CompileTimeAssert((i&3) == 1); return m_v[i/4].GetY(); }
	template <u32 i, u32> __forceinline ScalarV_Out GetInternal<i,2>(const Vec4V* m_v) { CompileTimeAssert((i&3) == 2); return m_v[i/4].GetZ(); }
	template <u32 i, u32> __forceinline ScalarV_Out GetInternal<i,3>(const Vec4V* m_v) { CompileTimeAssert((i&3) == 3); return m_v[i/4].GetW(); }

	template <u32 i, u32 i_and_3> __forceinline void SetInternal(Vec4V* m_v);

	template <u32 i, u32> __forceinline void SetInternal<i,0>(Vec4V* m_v, ScalarV_In value) { CompileTimeAssert((i&3) == 0); m_v[i/4].SetX(value); }
	template <u32 i, u32> __forceinline void SetInternal<i,1>(Vec4V* m_v, ScalarV_In value) { CompileTimeAssert((i&3) == 1); m_v[i/4].SetY(value); }
	template <u32 i, u32> __forceinline void SetInternal<i,2>(Vec4V* m_v, ScalarV_In value) { CompileTimeAssert((i&3) == 2); m_v[i/4].SetZ(value); }
	template <u32 i, u32> __forceinline void SetInternal<i,3>(Vec4V* m_v, ScalarV_In value) { CompileTimeAssert((i&3) == 3); m_v[i/4].SetW(value); }

} // namespace ScalarVArray_

template <u32 n> class ScalarVArray
{
public:
	template <u32 i> __forceinline ScalarV_Out Get() const           { CompileTimeAssert(i <= n); return ScalarVArray_::GetInternal<i,i&3>(m_v); }
	template <u32 i> __forceinline void        Set(ScalarV_In value) { CompileTimeAssert(i <= n);        ScalarVArray_::SetInternal<i,i&3>(m_v, value); }

	template <u32 i> __forceinline Vec4V_Out Get4() const         { CompileTimeAssert(i <= n && (i&3) == 0); return m_v[i/4]; }
	template <u32 i> __forceinline void      Set4(Vec4V_In value) { CompileTimeAssert(i <= n && (i&3) == 0);        m_v[i/4] = value; }

private:
	Vec4V m_v[(n+3)/4];
};

void TestScalarVArray()
{
	const int N = 5;
	ScalarV arr1[N];
	ScalarVArray<N> arr2;

	arr1[0] = ScalarV(1.1f);
	arr1[1] = ScalarV(2.2f);
	arr1[2] = ScalarV(-0.0f);
	arr1[3] = ScalarV(-1.2345f);
	arr1[4] = ScalarV(99999.0f);

	arr2.Set<0>(arr1[0]);
	arr2.Set<1>(arr1[1]);
	arr2.Set<2>(arr1[2]);
	arr2.Set<3>(arr1[3]);
	arr2.Set<4>(arr1[4]);

	Displayf("arr[0]=%f, arr2.Get<0>()=%f", arr1[0].Getf(), arr2.Get<0>().Getf());
	Displayf("arr[1]=%f, arr2.Get<1>()=%f", arr1[1].Getf(), arr2.Get<1>().Getf());
	Displayf("arr[2]=%f, arr2.Get<2>()=%f", arr1[2].Getf(), arr2.Get<2>().Getf());
	Displayf("arr[3]=%f, arr2.Get<3>()=%f", arr1[3].Getf(), arr2.Get<3>().Getf());
	Displayf("arr[4]=%f, arr2.Get<4>()=%f", arr1[4].Getf(), arr2.Get<4>().Getf());
}

#endif

#endif // !__FINAL
