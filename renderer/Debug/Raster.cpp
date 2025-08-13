// =========================
// renderer/Debug/Raster.cpp
// (c) 2012 RockstarNorth
// =========================

#include "renderer/Debug/Raster.h"

#if RASTER

#include "file/stream.h"
#include "grcore/debugdraw.h"
#include "grcore/viewport.h"
#include "math/vecshift.h" // for BIT64
#include "system/codecheck.h"
#include "system/nelem.h"
#include "vectormath/classes.h"
#include "vector/color32.h"

#include "fwmaths/vectorutil.h"
#include "fwrenderer/renderthread.h"
#include "fwutil/xmacro.h"

#include "renderer/occlusion.h"
#include "renderer/Util/ShaderUtil.h"
#include "renderer/Water.h"

//OPTIMISATIONS_OFF()

/*
TODO --
- change CRasterSpan back to using float x0,x1
- fix coverage raster (i broke this when i ported code from proto1)
- perfect clip should also do perspective-correct z interpolation
*/

/*
NOTE
- floating-point texture coordinates (T) and positions (P) are pixel CORNERS, i.e. [0.5,0.5] is the
  centre of the topleft pixel
- pixels are considered to be inside a triangle if their centre is inside the triangle
- allows one to draw a quad of dimensions [w,h] at origin [x,y] by passing positions [x,y,x+w,y+h]
- to convert from INTEGER pixel location to floating-point coordinate: cast to float and add 0.5f
- to convert a span [a,b] from floating-point coordinates to integer pixel locations:
  x0 = ceilf(a - 0.5f), x1 = ceilf(b - 0.5f) .. then covered pixels (x) will be x0 <= x < x1
*/

class CRasterTri
{
public:
	//    2
	//   / \
	//  /   \
	// 0-----1

	inline CRasterTri() {}
	inline CRasterTri(Vec3V_In p0, Vec3V_In p1, Vec3V_In p2, Mat44V_In mat);

	FASTRETURNCHECK(bool) Setup(bool bTwoSided = false);

	inline ScalarV_Out GetCentreZ(Vec2V_In p) const;
	inline ScalarV_Out GetCentreZ(int i, int j) const; // get z value at centre of pixel [i,j]

	inline ScalarV_Out GetMinCornerZ(Vec2V_In p) const;
	inline ScalarV_Out GetMinCornerZ(int i, int j) const; // get min z value at corners of pixel [i,j]

	inline ScalarV_Out GetMaxCornerZ(Vec2V_In p) const;
	inline ScalarV_Out GetMaxCornerZ(int i, int j) const; // get max z value at corners of pixel [i,j]

	inline bool IsInside(Vec2V_In p) const;
	inline bool IsInside(int i, int j) const;

	Vec3V mV[3]; // vertices
	Vec3V mDZ_Q; // dzdx,dzdy,1/(area*2)
};

class CRasterSpan
{
public:
	int m_i0;
	int m_i1;
	int m_j; // TODO -- spans are consecutive in y, seems silly to store this here
};

class CRasterSpanList
{
public:
	CRasterSpanList() { m_spanCount = 0; }
	CRasterSpanList(const CRasterTri& tri, int w, int h, const CRasterControl& control);

private:
	inline void AddSpan(int i0, int i1, int j);
	inline void AddSpan(float x0, float x1, int j) { AddSpan((int)x0, (int)x1, j); }

	void AddSpans(
		Vec3V_In L0, // top left
		Vec3V_In R0, // top right
		int      j0, // top
		Vec3V_In L1, // bottom left
		Vec3V_In R1, // bottom right
		int      j1);// bottom

	void Rasterise(const CRasterTri& tri);
	void RasteriseCoverage(const CRasterTri& tri);

	enum { MAX_SPANS = 256 }; // this limits the height of the depth/stencil buffers

public:
	int         m_w;
	int         m_h;
	CRasterSpan m_spans[MAX_SPANS];
	int         m_spanCount;
};

// ================================================================================================

inline CRasterTri::CRasterTri(Vec3V_In p0, Vec3V_In p1, Vec3V_In p2, Mat44V_In mat)
{
	mV[0] = TransformProjective(mat, p0);
	mV[1] = TransformProjective(mat, p1);
	mV[2] = TransformProjective(mat, p2);
}

FASTRETURNCHECK(bool) CRasterTri::Setup(bool bTwoSided)
{
	Vec3V p0 = mV[0];
	Vec3V p1 = mV[1];
	Vec3V p2 = mV[2];

	// rotate so that vertex A (p0) is the top -- TODO -- remove branches
	if (IsTrue(p0.GetY() >  p1.GetY()) |
		IsTrue(p0.GetY() >= p2.GetY()))
	{
		{ const Vec3V p3 = p0; p0 = p1; p1 = p2; p2 = p3; } // rotate p0,p1,p2 -> p1,p2,p0

		if (IsTrue(p0.GetY() >  p1.GetY()) ||
			IsTrue(p0.GetY() >= p2.GetY()))
		{
			{ const Vec3V p3 = p0; p0 = p1; p1 = p2; p2 = p3; } // rotate p0,p1,p2 -> p1,p2,p0
		}
	}

	Vec3V av = p0 - p2;
	Vec3V bv = p1 - p2;

	// flip vertices B,C so that triangle is counter-clockwise
	{
		const ScalarV ax1 = av.GetX();
		const ScalarV ay1 = av.GetY();
		const ScalarV bx1 = bv.GetX();
		const ScalarV by1 = bv.GetY();

		if (IsGreaterThanAll(ax1*by1, bx1*ay1))
		{
			if (bTwoSided)
			{
				{ const Vec3V p3 = p1; p1 = p2; p2 = p3; } // swap p1 and p2

				av = p0 - p2;
				bv = p1 - p2;
			}
			else
			{
				return false;
			}
		}
	}

	mV[0] = p0;
	mV[1] = p1;
	mV[2] = p2;

	const ScalarV ax = av.GetX();
	const ScalarV ay = av.GetY();
	const ScalarV az = av.GetZ();

	const ScalarV bx = bv.GetX();
	const ScalarV by = bv.GetY();
	const ScalarV bz = bv.GetZ();

	ScalarV q = SubtractScaled(ay*bx, ax, by); // triangle area*2

	if (IsGreaterThanAll(q, ScalarV(V_ZERO)))
	{
		q = Invert(q);
	}

	const ScalarV dzdx = SubtractScaled(bz*ay, az, by);
	const ScalarV dzdy = SubtractScaled(az*bx, bz, ax);

	mDZ_Q = Vec3V(dzdx, dzdy, ScalarV(V_ONE))*q;

	return true;
}

inline ScalarV_Out CRasterTri::GetCentreZ(Vec2V_In p) const
{
	const Vec2V v = p - mV[2].GetXY();
	const Vec2V dz = mDZ_Q.GetXY();

	return mV[2].GetZ() + Dot(dz, v);
}

inline ScalarV_Out CRasterTri::GetCentreZ(int i, int j) const // get z value at centre of pixel [i,j]
{
	return GetCentreZ(Vec2V((float)i + 0.5f, (float)j + 0.5f));
}

inline ScalarV_Out CRasterTri::GetMinCornerZ(Vec2V_In p) const
{
	const Vec2V v = p - mV[2].GetXY();
	const Vec2V dz = mDZ_Q.GetXY();

	return mV[2].GetZ() + Dot(dz, v) - Dot(Abs(dz), Vec2V(V_HALF));
}

inline ScalarV_Out CRasterTri::GetMinCornerZ(int i, int j) const // get min z value at corners of pixel [i,j]
{
	return GetMinCornerZ(Vec2V((float)i + 0.5f, (float)j + 0.5f));
}

inline ScalarV_Out CRasterTri::GetMaxCornerZ(Vec2V_In p) const
{
	const Vec2V v = p - mV[2].GetXY();
	const Vec2V dz = mDZ_Q.GetXY();

	return mV[2].GetZ() + Dot(dz, v) + Dot(Abs(dz), Vec2V(V_HALF));
}

inline ScalarV_Out CRasterTri::GetMaxCornerZ(int i, int j) const // get max z value at corners of pixel [i,j]
{
	return GetMaxCornerZ(Vec2V((float)i + 0.5f, (float)j + 0.5f));
}

inline bool CRasterTri::IsInside(Vec2V_In p) const
{
	const Vec2V v0 = mV[0].GetXY() - p;
	const Vec2V v1 = mV[1].GetXY() - p;
	const Vec2V v2 = mV[2].GetXY() - p;

	if (IsGreaterThanAll(v0.GetX()*v1.GetY(), v1.GetX()*v0.GetY()) |
		IsGreaterThanAll(v1.GetX()*v2.GetY(), v2.GetX()*v1.GetY()) |
		IsGreaterThanAll(v2.GetX()*v0.GetY(), v0.GetX()*v2.GetY()))
	{
		return false;
	}

	return true;
}

inline bool CRasterTri::IsInside(int i, int j) const
{
	return IsInside(Vec2V((float)i + 0.5f, (float)j + 0.5f));
}

CRasterSpanList::CRasterSpanList(const CRasterTri& tri, int w, int h, const CRasterControl& control)
{
	m_w = w;
	m_h = h;

	m_spanCount = 0;

	if (control.m_useCoverage BANK_ONLY(|| control.m_perfectClip))
	{
		RasteriseCoverage(tri);
	}
	else
	{
		Rasterise(tri);
	}
}

inline void CRasterSpanList::AddSpan(int i0, int i1, int j)
{
	// note that spans aren't necessarily consecutive (unless we're doing coverage rasterisation)
	Assert(m_spanCount == 0 || m_spans[m_spanCount - 1].m_j < j);

	if (AssertVerify(m_spanCount < MAX_SPANS))
	{
		CRasterSpan &s = m_spans[m_spanCount++];

		s.m_i0 = i0;
		s.m_i1 = i1;
		s.m_j  = j;
	}
}

void CRasterSpanList::AddSpans(
	Vec3V_In L0, // top left
	Vec3V_In R0, // top right
	int      j0, // top
	Vec3V_In L1, // bottom left
	Vec3V_In R1, // bottom right
	int      j1) // bottom
{
	//          L0
	//          /
	//         /    R0
	//        /      \
	//       +--------+ - - - - j0
	//      /          \
	//     /            \
	//    /              \
	//   +----------------+ - - j1
	//  /                  \
	// L1                   \
	//                       \
	//                       R1

	if (IsEqualAll(L1.GetY(), L0.GetY()) ||
		IsEqualAll(R1.GetY(), R0.GetY())) // TODO -- check outside this function
	{
		return;
	}

	const float xL0 = L0.GetXf();
	const float yL0 = L0.GetYf();
	const float xR0 = R0.GetXf();
	const float yR0 = R0.GetYf();

	const float dxdL = (L1.GetXf() - xL0)/(L1.GetYf() - yL0); // left gradient
	const float dxdR = (R1.GetXf() - xR0)/(R1.GetYf() - yR0); // right gradient

	for (int j = j0; j < j1; j++)
	{
		if (j >= 0 && j < m_h) // simple scissoring in y
		{
			const float yf = (float)j + 0.5f;
			const float xL = xL0 + (yf - yL0)*dxdL; // TODO -- fix single-pixel errors due to gradient being calculated outside of loop
			const float xR = xR0 + (yf - yR0)*dxdR;

			int i0 = (int)ceilf(xL - 0.5f);
			int i1 = (int)ceilf(xR - 0.5f);

			i0 = Max<int>(0,   i0); // simple scissoring in x
			i1 = Min<int>(m_w, i1);

			if (i0 < i1)
			{
				AddSpan(i0, i1, j);
			}
		}
	}
}

void CRasterSpanList::Rasterise(const CRasterTri& tri)
{
	// rasterise pixel centres (i.e. rendering triangles into a framebuffer)

	if (IsLessThanOrEqualAll(tri.mV[1].GetY(), tri.mV[2].GetY()))
	{
		//     A - - j0
		//    /|
		//   / |   (top)
		//  /  |
		// B---+ - - j1,j2
		//  \  |
		//   \ |  (bottom)
		//    \|
		//     C - - j3

		const int j0 = (int)ceilf(tri.mV[0].GetYf() - 0.5f);
		const int j1 = (int)ceilf(tri.mV[1].GetYf() - 0.5f);
		const int j2 = j1;
		const int j3 = (int)ceilf(tri.mV[2].GetYf() - 0.5f);

		AddSpans(tri.mV[0], tri.mV[0], j0, tri.mV[1], tri.mV[2], j1); // top
		AddSpans(tri.mV[1], tri.mV[0], j2, tri.mV[2], tri.mV[2], j3); // bottom
	}
	else
	{
		// A - - - - j0
		// |\
		// | \     (top)
		// |  \
		// +---C - - j1,j2
		// |  /
		// | /    (bottom)
		// |/
		// B - - - - j3

		const int j0 = (int)ceilf(tri.mV[0].GetYf() - 0.5f);
		const int j1 = (int)ceilf(tri.mV[2].GetYf() - 0.5f);
		const int j2 = j1;
		const int j3 = (int)ceilf(tri.mV[1].GetYf() - 0.5f);

		AddSpans(tri.mV[0], tri.mV[0], j0, tri.mV[1], tri.mV[2], j1); // top
		AddSpans(tri.mV[0], tri.mV[2], j2, tri.mV[1], tri.mV[1], j3); // bottom
	}
}

void CRasterSpanList::RasteriseCoverage(const CRasterTri& tri)
{
	// rasterise pixel "coverage" (i.e. determining which pixels overlap with a triangle)

	float Ax = tri.mV[0].GetXf();
	float Ay = tri.mV[0].GetYf();
	float Bx = tri.mV[1].GetXf();
	float By = tri.mV[1].GetYf();
	float Cx = tri.mV[2].GetXf();
	float Cy = tri.mV[2].GetYf();

	float Ay0 = floorf(Ay);

	float xmin = Min<float>(Ax, Bx, Cx); // triangle bounds
	float xmax = Max<float>(Ax, Bx, Cx);
	float ymax = Max<float>(By, Cy);

	int j = (int)Ay0; // index of first span

	// first check for trivial cases
	{
		const float range = Min<float>(ceilf(xmax) - floorf(xmin), ceilf(ymax) - Ay0);

		if (range == 0.0f) // entire triangle spans 0 rows or columns - zero coverage
		{
			return; // done
		}
		else if (range == 1.0f) // entire triangle spans 1 row or column
		{
			while (Ay0 < ymax)
			{
				AddSpan(xmin, xmax, j);

				j++, Ay0++;
			}

			return; // done
		}
	}

	const bool flip = (By > Cy);

	if (flip)
	{
		float temp;

		// negate x-coords, swap B and C
		Ax   = -Ax;
		temp = -Bx; Bx = -Cx; Cx = temp;
		temp =  By; By =  Cy; Cy = temp;

		// negate and swap xmin,xmax
		temp = -xmin; xmin = -xmax; xmax = temp;
	}

	Assert(By <= Cy);

	const float dAB = (Bx - Ax)/(By - Ay); // divide by zero is possible, but the results will not be used ..
	const float dBC = (Cx - Bx)/(Cy - By);
	const float dCA = (Ax - Cx)/(Ay - Cy);

	const float dAB_pos = Max<float>(0.0f, dAB);
	const float dBC_pos = Max<float>(0.0f, dBC);
	const float dCA_pos = Max<float>(0.0f, dCA);
	const float dAB_neg = Min<float>(0.0f, dAB);
	const float dBC_neg = Min<float>(0.0f, dBC);
	const float dCA_neg = Min<float>(0.0f, dCA);

	float By0      = floorf(By);
	float By1      = ceilf (By);
	float Cy1_sub1 = ceilf (Cy) - 1.0f;

	float y1 = Ay0 + 1.0f; // lower edge of first span

	// TOP SPAN ....... [floorf(Ay),   floorf(Ay)+1]
	// UPPER SPANS .... [floorf(Ay)+1, floorf(By)  ] 
	// MIDDLE SPAN .... [floorf(By),   ceilf (By)  ]
	// LOWER SPANS .... [ceilf (By),   ceilf (Cy)-1]
	// BOTTOM SPAN .... [ceilf (Cy)-1, ceilf (Cy)  ]

	if (By1 == y1) // top half is 1 span [Ay0..By1], will include or touch (A) and (B) but will not touch (C)
	{
		Assert(y1 == (float)(j + 1));

		const float x0 = (y1 - By)*dBC_neg + Min<float>(Ax, Bx);
		const float x1 = (y1 - Ay)*dCA_pos + Ax;

		Assert(xmin <= x0 && x0 < x1 && x1 <= xmax);

		AddSpan(x0, x1, j);
		j++, y1++;

		By1 = By0; // skip middle span
	}
	else if (By1 > y1) // top half is multiple spans ..
	{
		// top span [Ay0..Ay0+1], will include or touch (A) on top but will not touch (B) or (C)
		{
			Assert(y1 == (float)(j + 1));

			const float x0 = (y1 - Ay)*dAB_neg + Ax;
			const float x1 = (y1 - Ay)*dCA_pos + Ax; 

			Assert(xmin <= x0 && x0 < x1 && x1 <= xmax);

			AddSpan(x0, x1, j);
			j++, y1++;
		}

		while (y1 <= By0) // upper spans [Ay0+1..By0], may touch (B) on bottom but will not touch (A) ... may touch (C) if Cy=By is an integer
		{
			Assert(y1 == (float)(j + 1));

			const float x0 = ((y1 - By)*dAB - dAB_pos) + Bx; // must = Bx exactly when y = By and dAB <= 0
			const float x1 = ((y1 - Cy)*dCA - dCA_neg) + Cx; // must = Cx exactly when y = Cy and dCA >= 0

			Assert(xmin <= x0 && x0 < x1 && x1 <= xmax);

			AddSpan(x0, x1, j);
			j++, y1++;
		}
	}

	float y0 = y1 - 1.0f; // now we track the upper edge of the span

	if (By0 == Cy1_sub1) // bottom half is 1 span [By0..Cy1], will include or touch (B) and (C) but will not touch (A)
	{
		Assert(y0 == (float)(j + 0));

		const float x0 = (y0 - By)*dAB_pos + Min<float>(Bx, Cx);
		const float x1 = (y0 - Cy)*dCA_neg + Cx;

		Assert(xmin <= x0 && x0 < x1 && x1 <= xmax);

		AddSpan(x0, x1, j);
		//j++, y0++;
	}
	else if (By0 < Cy1_sub1) // bottom half is multiple spans ..
	{
		if (By0 < By1) // middle span [By0..By1], will include or touch (B) on top or bottom but will not touch (A) or (C) 
		{
			Assert(y0 == (float)(j + 0));
			Assert(y1 == (float)(j + 1));

			Assert(dAB_pos*dBC_neg == 0.0f); // NOTE: dAB_pos and dBC_neg cannot simultaneously be non-zero due to CCW winding

			const float x0  = ((y0 - By)*dAB_pos + (y1 - By)*dBC_neg) + Bx;
			const float x1  = ((y0 - Ay)*dCA     +           dCA_pos) + Ax; // all four of these (x1,x1b,x1c,x1d) are valid ...
		//	const float x1b = ((y1 - Ay)*dCA     -           dCA_neg) + Ax;
		//	const float x1c = ((y0 - Cy)*dCA     +           dCA_pos) + Cx;
		//	const float x1d = ((y1 - Cy)*dCA     -           dCA_neg) + Cx;

			Assert(xmin <= x0 && x0 < x1 && x1 <= xmax);

			AddSpan(x0, x1, j);
			j++, y0++;
		}

		while (y0 < Cy1_sub1) // lower spans [By1..Cy1-1], may touch (B) on top but will not touch (C) ... may touch (A) if Ay=By is an integer
		{
			Assert(y0 == (float)(j + 0));

			const float x0 = ((y0 - By)*dBC + dBC_neg) + Bx; // must = Bx exactly when y = By and dBC >= 0
			const float x1 = ((y0 - Ay)*dCA + dCA_pos) + Ax; // must = Ax exactly when y = Ay and dCA <= 0

			Assert(xmin <= x0 && x0 < x1 && x1 <= xmax);

			AddSpan(x0, x1, j);
			j++, y0++;
		}

		// bottom span [Cy1-1..Cy1], will include or touch (C) on bottom but will not touch (A) or (B)
		{
			Assert(y0 == (float)(j + 0));

			const float x0 = (y0 - Cy)*dBC_pos + Cx; // must = Cx exactly when y = Cy
			const float x1 = (y0 - Cy)*dCA_neg + Cx; // must = Cx exactly when y = Cy

			Assert(xmin <= x0 && x0 < x1 && x1 <= xmax);

			AddSpan(x0, x1, j);
			//j++, y0++;
		}
	}

	if (flip) // negate and swap spans
	{
		for (int k = 0; k < m_spanCount; k++)
		{
			CRasterSpan &s = m_spans[k];
			CRasterSpan temp;
			temp.m_i0 = -s.m_i1;
			temp.m_i1 = -s.m_i0;
			temp.m_j = s.m_j;
			s = temp;
		}
	}
}

// ================================================================================================

#if RASTER_GEOM_BUFFER

class CRasterGeometry
{
public:
	void Init(int maxVerts, int maxIndices = 0);
	void Clear();
	void RegisterPoly(const Vec3V* points, int numPoints);
	int GetNumTris() const;
	void GetTri(Vec3V points[3], int triIdx) const;

	Vec3V* m_verts;
	int    m_vertCount;
	int    m_vertCountMax;
	u16*   m_indices;
	int    m_indexCount;
	int    m_indexCountMax;
};

#endif // RASTER_GEOM_BUFFER

class CRaster
{
public:
	CRaster();

	void Init(const grcViewport* vp, int w, int h, float* depth = NULL, u64* stencil = NULL, int maxOccludeVerts = 0, int maxStencilVerts = 3200);
	void Clear(const grcViewport* vp = NULL);
	template <class RasterFunc> bool RasterisePoly(const Vec3V* points, int numPoints, bool bClipToFrustum BANK_PARAM(CRasterStatistics& stats));
	bool RasteriseOccludePoly(const Vec3V* points, int numPoints, bool bClipToFrustum = true);
	bool RasteriseStencilPoly(const Vec3V* points, int numPoints, bool bClipToFrustum = true);
#if RASTER_GEOM_BUFFER
	void RegisterOccludePoly(const Vec3V* points, int numPoints);
	void RegisterStencilPoly(const Vec3V* points, int numPoints);
	void RasteriseOccludeGeometry();
	void RasteriseStencilGeometry();
#endif // RASTER_GEOM_BUFFER
	void RasteriseOccludeBoxes (BANK_ONLY(float debugDrawOpacity = 0.0f));
	void RasteriseOccludeModels(BANK_ONLY(float debugDrawOpacity = 0.0f));
	void RasteriseWater        (BANK_ONLY(float debugDrawOpacity = 0.0f));
	void RasteriseFinish();
#if __BANK
	bool IsFrozen();
	void DebugDraw(int ox = 0, int oy = 0, int scale = 0) const;
#if RASTER_GEOM_BUFFER
	void DebugDrawGeometry(int ox, int oy, int scale, Color32 occludeEdge, Color32 occludeFill, Color32 stencilEdge, Color32 stencilFill) const;
#endif // RASTER_GEOM_BUFFER
#endif // __BANK

	const grcViewport* m_viewport;
	Mat44V             m_fullCompositeMtx;
	int                m_w;       // width
	int                m_h;       // height
	float*             m_depth;   // linear depth buffer, w*h floats
	u64*               m_stencil; // linear stencil buffer, (w+63)/64 elements per row, h rows
	CRasterControl     m_control;
#if RASTER_GEOM_BUFFER
	CRasterGeometry    m_occludeGeometry;
	CRasterGeometry    m_stencilGeometry;
#endif // RASTER_GEOM_BUFFER
#if __BANK
	CRasterStatistics  m_occludeStats;
	CRasterStatistics  m_stencilStats;
	bool               m_freeze;    // don't update until this flag is cleared (this gets set after perfect clipping, etc.)
	Mat44V             m_freezeMtx; // freeze flag gets cleared when viewport composite mtx changes from this
#endif // __BANK
};

// ================================================================================================

#if RASTER_GEOM_BUFFER

void CRasterGeometry::Init(int maxVerts, int maxIndices)
{
	if (maxVerts > 0 && maxIndices == 0)
	{
		maxIndices = (maxVerts/4)*6; // assume quads
	}

	m_verts         = maxVerts > 0 ? rage_new Vec3V[maxVerts] : NULL;
	m_vertCount     = 0;
	m_vertCountMax  = maxVerts;
	m_indices       = maxIndices > 0 ? rage_new u16[maxIndices] : NULL;
	m_indexCount    = 0;
	m_indexCountMax = maxIndices;
}

void CRasterGeometry::Clear()
{
	m_vertCount  = 0;
	m_indexCount = 0;
}

void CRasterGeometry::RegisterPoly(const Vec3V* points, int numPoints)
{
	if (!AssertVerify(m_vertCount + numPoints <= m_vertCountMax))
	{
		return;
	}

	if (!AssertVerify(m_indexCount + (numPoints - 2)*3 <= m_indexCountMax))
	{
		return;
	}

	const int vi = m_vertCount;

	for (int i = 0; i < numPoints; i++)
	{
		m_verts[m_vertCount++] = points[i];
	}

	for (int i = 2; i < numPoints; i++)
	{
		const Vec3V p0 = points[0];
		const Vec3V p1 = points[i - 1];
		const Vec3V p2 = points[i];

		if (IsEqualAll(p0, p1) | IsEqualAll(p1, p2) | IsEqualAll(p2, p0))
		{
			// triangle is degenerate
		}
		else
		{
			m_indices[m_indexCount++] = (u16)(vi);
			m_indices[m_indexCount++] = (u16)(vi + i - 1);
			m_indices[m_indexCount++] = (u16)(vi + i);
		}
	}
}

int CRasterGeometry::GetNumTris() const
{
	Assert(m_indexCount%3 == 0);
	return m_indexCount/3;
}

void CRasterGeometry::GetTri(Vec3V points[3], int triIdx) const
{
	Assert(triIdx >= 0 && triIdx*3 < m_indexCount);
	const int vi0 = (int)m_indices[triIdx*3 + 0];
	const int vi1 = (int)m_indices[triIdx*3 + 1];
	const int vi2 = (int)m_indices[triIdx*3 + 2];
	Assert(vi0 >= 0 && vi0 < m_vertCount);
	Assert(vi1 >= 0 && vi1 < m_vertCount);
	Assert(vi2 >= 0 && vi2 < m_vertCount);
	points[0] = m_verts[vi0];
	points[1] = m_verts[vi1];
	points[2] = m_verts[vi2];
}

#endif // RASTER_GEOM_BUFFER

CRaster::CRaster()
{
	sysMemSet(this, 0, sizeof(*this));
}

void CRaster::Init(const grcViewport* vp, int w, int h, float* depth, u64* stencil, int RASTER_GEOM_BUFFER_ONLY(maxOccludeVerts), int RASTER_GEOM_BUFFER_ONLY(maxStencilVerts))
{
	Assert(m_depth == NULL);
	Assert(m_stencil == NULL);

	const int stencilStride = (w + 63)/64;

	m_viewport         = vp;
	m_fullCompositeMtx = Mat44V(V_IDENTITY);
	m_w                = w;
	m_h                = h;
	m_depth            = depth   ? depth   : rage_aligned_new(RSG_PS3 ? 128 : 16) float[w*h];
	m_stencil          = stencil ? stencil : rage_aligned_new(RSG_PS3 ? 128 : 16) u64[stencilStride*h];

#if RASTER_GEOM_BUFFER
	m_occludeGeometry.Init(maxOccludeVerts);
	m_stencilGeometry.Init(maxStencilVerts);
#endif // RASTER_GEOM_BUFFER

	Clear();
}

void CRaster::Clear(const grcViewport* vp)
{
	if (m_depth == NULL)
	{
		return;
	}

	if (vp)
	{
		m_viewport = vp;
	}

	const Vec4V scale((float)m_w/(float)m_viewport->GetWidth(), (float)m_h/(float)m_viewport->GetHeight(), 1.0f, 1.0f);

	m_fullCompositeMtx = m_viewport->GetFullCompositeMtx();
	m_fullCompositeMtx.GetCol0Ref() *= scale;
	m_fullCompositeMtx.GetCol1Ref() *= scale;
	m_fullCompositeMtx.GetCol2Ref() *= scale;
	m_fullCompositeMtx.GetCol3Ref() *= scale;

#if __BANK
	if (IsFrozen())
	{
		return;
	}
#endif // __BANK

	for (int i = 0; i < m_w*m_h; i++)
	{
		m_depth[i] = FLT_MAX;
	}

	const int stencilStride = (m_w + 63)/64;

	sysMemSet(m_stencil, 0, stencilStride*m_h*sizeof(u64));

#if RASTER_GEOM_BUFFER
	m_occludeGeometry.Clear();
	m_stencilGeometry.Clear();
#endif // RASTER_GEOM_BUFFER
#if __BANK
	sysMemSet(&m_occludeStats, 0, sizeof(m_occludeStats));
	sysMemSet(&m_stencilStats, 0, sizeof(m_stencilStats));
#endif // __BANK
}

template <class RasterFunc> bool CRaster::RasterisePoly(const Vec3V* points, int numPoints, bool bClipToFrustum BANK_PARAM(CRasterStatistics& stats))
{
	if (m_depth == NULL)
	{
		return false;
	}

	const int maxPoints = 8;
	Vec3V clipped[maxPoints + 6];

	if (!AssertVerify(numPoints <= maxPoints))
	{
		return false;
	}

	if (bClipToFrustum)
	{
		numPoints = PolyClipToFrustum(clipped, points, numPoints, m_viewport->GetCompositeMtx());
		points = clipped;
	}

	if (numPoints < 3)
	{
		return false;
	}

	const int stencilStride = (m_w + 63)/64;

	bool bPolyVisible = false;

	for (int i = 2; i < numPoints; i++)
	{
		const Vec3V p0 = points[0];
		const Vec3V p1 = points[i - 1];
		const Vec3V p2 = points[i];

		CRasterTri tri(p0, p1, p2, m_fullCompositeMtx);

		if (m_control.m_useLinearZ)
		{
			tri.mV[0].SetZ(ShaderUtil::GetLinearDepth(m_viewport, tri.mV[0].GetZ()));
			tri.mV[1].SetZ(ShaderUtil::GetLinearDepth(m_viewport, tri.mV[1].GetZ()));
			tri.mV[2].SetZ(ShaderUtil::GetLinearDepth(m_viewport, tri.mV[2].GetZ()));
		}

		if (tri.Setup(true)) // TODO -- don't really need double-sided since we do backface culling earlier
		{
			const CRasterSpanList spans(tri, m_w, m_h, m_control);
#if __BANK
			int i_min = +9999;
			int i_max = -9999;
#endif // __BANK

			for (int k = 0; k < spans.m_spanCount; k++)
			{
				const CRasterSpan& span = spans.m_spans[k];

				const int i0 = span.m_i0;
				const int i1 = span.m_i1;
				const int j  = span.m_j;

				if (!AssertVerify(j >= 0 && j <= m_h)) // check simple scissoring in y
				{
					continue;
				}

				if (!AssertVerify(i0 >= 0 && i1 <= m_w)) // check simple scissoring in x
				{
					continue;
				}

				if (RasterFunc::func(&m_depth[j*m_w], &m_stencil[j*stencilStride], tri, i0, i1, j BANK_PARAM(stats)))
				{
					bPolyVisible = true;
				}
#if __BANK
				stats.m_numPixels += i1 - i0;

				i_min = Min<int>(i0, i_min);
				i_max = Max<int>(i1, i_max);
#endif // __BANK
			}

#if __BANK
			stats.m_numPixelsBounds += Max<int>(0, i_max - i_min)*spans.m_spanCount;
#endif // __BANK
		}
	}

#if __BANK
	if (bPolyVisible)
	{
		stats.m_numPolysVisible++;
	}

	stats.m_numPolys++;
#endif // __BANK

	return bPolyVisible;
}

template <bool bUseCorners> class RasteriseOcclude { public: static bool func(float* depthRow, u64* /*stencilRow*/, const CRasterTri& tri, int i0, int i1, int j BANK_PARAM(CRasterStatistics& stats))
{
	bool bSpanVisible = false;

	for (int i = i0; i < i1; i++)
	{
		const float z = bUseCorners ? tri.GetMaxCornerZ(i, j).Getf() : tri.GetCentreZ(i, j).Getf();

		if (depthRow[i] > z)
		{
			depthRow[i] = z;
			bSpanVisible = true;
#if __BANK
			stats.m_numPixelsPassed++;
#endif // __BANK
		}
	}

	return bSpanVisible;
}};

template <bool bUseCorners> class RasteriseStencil { public: static bool func(float* depthRow, u64* stencilRow, const CRasterTri& tri, int i0, int i1, int j BANK_PARAM(CRasterStatistics& stats))
{
	bool bSpanVisible = false;

	for (int i = i0; i < i1; i++)
	{
		const float z = bUseCorners ? tri.GetMinCornerZ(i, j).Getf() : tri.GetCentreZ(i, j).Getf();

		if (depthRow[i] >= z)
		{
			stencilRow[i/64] |= BIT64(63 - i%64);
			bSpanVisible = true;
#if __BANK
			stats.m_numPixelsPassed++;
#endif // __BANK
		}
	}

	return bSpanVisible;
}};

#if __BANK

class RasteriseOccludePerfectClip { public: static bool func(float* depthRow, u64* /*stencilRow*/, const CRasterTri& tri, int i0, int i1, int j BANK_PARAM(CRasterStatistics& stats))
{
	bool bSpanVisible = false;

	for (int i = i0; i < i1; i++)
	{
		const float x0 = (float)(i + 0);
		const float y0 = (float)(j + 0);
		const float x1 = (float)(i + 1);
		const float y1 = (float)(j + 1);

		int   count = 3;
		Vec3V temp0[7] = {tri.mV[0], tri.mV[1], tri.mV[2]};
		Vec3V temp1[7];

		// clip to four planes
		count = PolyClip(temp1, NELEM(temp1), temp0, count, BuildPlane(Vec3V(x0, y0, 0.0f), +Vec3V(V_X_AXIS_WZERO)));
		count = PolyClip(temp0, NELEM(temp0), temp1, count, BuildPlane(Vec3V(x0, y0, 0.0f), +Vec3V(V_Y_AXIS_WZERO)));
		count = PolyClip(temp1, NELEM(temp1), temp0, count, BuildPlane(Vec3V(x1, y1, 0.0f), -Vec3V(V_X_AXIS_WZERO)));
		count = PolyClip(temp0, NELEM(temp0), temp1, count, BuildPlane(Vec3V(x1, y1, 0.0f), -Vec3V(V_Y_AXIS_WZERO)));

		if (count >= 3)
		{
			Vec3V vmax = temp0[0];

			for (int k = 1; k < count; k++)
			{
				vmax = Max(temp0[k], vmax);
			}

			const float z = vmax.GetZf();

			if (depthRow[i] > z)
			{
				depthRow[i] = z;
				bSpanVisible = true;
#if __BANK
				stats.m_numPixelsPassed++;
#endif // __BANK
			}
		}
	}

	return bSpanVisible;
}};

class RasteriseStencilPerfectClip { public: static bool func(float* depthRow, u64* stencilRow, const CRasterTri& tri, int i0, int i1, int j BANK_PARAM(CRasterStatistics& stats))
{
	bool bSpanVisible = false;

	for (int i = i0; i < i1; i++)
	{
		const float x0 = (float)(i + 0);
		const float y0 = (float)(j + 0);
		const float x1 = (float)(i + 1);
		const float y1 = (float)(j + 1);

		int   count = 3;
		Vec3V temp0[7] = {tri.mV[0], tri.mV[1], tri.mV[2]};
		Vec3V temp1[7];

		// clip to four planes
		count = PolyClip(temp1, NELEM(temp1), temp0, count, BuildPlane(Vec3V(x0, y0, 0.0f), +Vec3V(V_X_AXIS_WZERO)));
		count = PolyClip(temp0, NELEM(temp0), temp1, count, BuildPlane(Vec3V(x0, y0, 0.0f), +Vec3V(V_Y_AXIS_WZERO)));
		count = PolyClip(temp1, NELEM(temp1), temp0, count, BuildPlane(Vec3V(x1, y1, 0.0f), -Vec3V(V_X_AXIS_WZERO)));
		count = PolyClip(temp0, NELEM(temp0), temp1, count, BuildPlane(Vec3V(x1, y1, 0.0f), -Vec3V(V_Y_AXIS_WZERO)));

		if (count >= 3)
		{
			Vec3V vmin = temp0[0];

			for (int k = 1; k < count; k++)
			{
				vmin = Min(temp0[k], vmin);
			}

			const float z = vmin.GetZf();

			if (depthRow[i] >= z)
			{
				stencilRow[i/64] |= BIT64(63 - i%64);
				bSpanVisible = true;
#if __BANK
				stats.m_numPixelsPassed++;
#endif // __BANK
			}
		}
	}

	return bSpanVisible;
}};

#endif // __BANK

bool CRaster::RasteriseOccludePoly(const Vec3V* points, int numPoints, bool bClipToFrustum)
{
#if __BANK
	if (IsFrozen())
	{
		return false;
	}
	else if (m_control.m_perfectClip)
	{
		return RasterisePoly<RasteriseOccludePerfectClip>(points, numPoints, bClipToFrustum, m_occludeStats);
	}
#endif // __BANK

	const bool bUseCorners = m_control.m_useCorners;

	if (bUseCorners) { return RasterisePoly<RasteriseOcclude<true > >(points, numPoints, bClipToFrustum BANK_PARAM(m_occludeStats)); }
	else             { return RasterisePoly<RasteriseOcclude<false> >(points, numPoints, bClipToFrustum BANK_PARAM(m_occludeStats)); }
}

bool CRaster::RasteriseStencilPoly(const Vec3V* points, int numPoints, bool bClipToFrustum)
{
#if __BANK
	if (IsFrozen())
	{
		return false;
	}
	else if (m_control.m_perfectClip)
	{
		return RasterisePoly<RasteriseStencilPerfectClip>(points, numPoints, bClipToFrustum, m_stencilStats);
	}
#endif // __BANK

	const bool bUseCorners = m_control.m_useCorners;

	if (bUseCorners) { return RasterisePoly<RasteriseStencil<true > >(points, numPoints, bClipToFrustum BANK_PARAM(m_stencilStats)); }
	else             { return RasterisePoly<RasteriseStencil<false> >(points, numPoints, bClipToFrustum BANK_PARAM(m_stencilStats)); }
}

#if RASTER_GEOM_BUFFER

void CRaster::RegisterOccludePoly(const Vec3V* points, int numPoints)
{
	if (m_depth == NULL)
	{
		return;
	}

	m_occludeGeometry.RegisterPoly(points, numPoints);
}

void CRaster::RegisterStencilPoly(const Vec3V* points, int numPoints)
{
	if (m_depth == NULL)
	{
		return;
	}

	m_stencilGeometry.RegisterPoly(points, numPoints);
}

void CRaster::RasteriseOccludeGeometry()
{
	if (m_depth == NULL)
	{
		return;
	}

	int numPixels = 0;

	for (int triIdx = 0; triIdx < m_occludeGeometry.GetNumTris(); triIdx++)
	{
		Vec3V points[3];
		m_occludeGeometry.GetTri(points, triIdx);

		RasteriseOccludePoly(points, NELEM(points), true);
	}
}

void CRaster::RasteriseStencilGeometry()
{
	if (m_depth == NULL)
	{
		return;
	}

	for (int triIdx = 0; triIdx < m_stencilGeometry.GetNumTris(); triIdx++)
	{
		Vec3V points[3];
		m_stencilGeometry.GetTri(points, triIdx);

		RasteriseStencilPoly(points, NELEM(points));
	}
}

#endif // RASTER_GEOM_BUFFER

void CRaster::RasteriseOccludeBoxes(BANK_ONLY(float debugDrawOpacity))
{
	if (m_depth == NULL)
	{
		return;
	}

#if __BANK
	const Color32 debugDrawColour(0, 255, 255, (int)(0.5f + 255.0f*debugDrawOpacity));
	const ScalarV nearZ(m_viewport->GetNearClip() + 0.1f);
#endif // __BANK

	const Vec3V camPos = +m_viewport->GetCameraMtx().GetCol3();
#if __BANK
	const Vec3V camDir = -m_viewport->GetCameraMtx().GetCol2();
#endif // __BANK

	for (int i = 0; i < COcclusion::GetBoxOccluderUsedBucketCount(); i++)
	{
		CBoxOccluderBucket* pBucket = COcclusion::GetBoxOccluderBucket(i);
		Assert(pBucket);

		const CBoxOccluderArray* pArray = pBucket->pArray;
		Assert(pArray);

		for (int j = 0; j < pArray->GetCount(); j++)
		{
			const BoxOccluder& occ = pArray->operator[](j);

			Vec3V verts[8];
			occ.CalculateVerts(verts);

			Vec3V pmin = verts[0];
			Vec3V pmax = pmin;

			for (int j = 1; j < NELEM(verts); j++)
			{
				pmin = Min(verts[j], pmin);
				pmax = Max(verts[j], pmax);
			}

			if (grcViewport::IsAABBVisible(pmin.GetIntrin128(), pmax.GetIntrin128(), m_viewport->GetCullFrustumLRTB()))
			{
				bool bBoxVisible = false;

				const u8 expectedIndices[] = // from boxoccluder.cpp
				{
					2,3,1, 0,2,1, // 2,3,1,0,
					4,5,3, 2,4,3, // 4,5,3,2,
					6,7,5, 4,6,5, // 6,7,5,4,
					0,1,7, 6,0,7, // 0,1,7,6,
					3,5,7, 1,3,7, // 3,5,7,1,
					4,2,0, 6,4,0, // 4,2,0,6,
				};

				const u8 quadIndices[] =
				{
					2,3,1,0,
					4,5,3,2,
					6,7,5,4,
					0,1,7,6,
					3,5,7,1,
					4,2,0,6,
				};

				static bool bChecked = false;

				if (!bChecked) // check once that the triangle indices match what we expect, if not then the quad indices are probably wrong too
				{
					bChecked = true;

					Assert(occ.GetNumOccluderIndices() == NELEM(expectedIndices));
					Assert(memcmp(occ.GetOccluderIndices(), expectedIndices, NELEM(expectedIndices)*sizeof(u8)) == 0);
				}

				for (int j = 0; j < NELEM(quadIndices); j += 4)
				{
					const Vec3V points[] =
					{
						verts[quadIndices[j + 0]],
						verts[quadIndices[j + 1]],
						verts[quadIndices[j + 2]],
						verts[quadIndices[j + 3]],
					};

					if (IsGreaterThanOrEqualAll(Dot(points[0] - camPos, Cross(points[1] - points[0], points[2] - points[0])), ScalarV(V_ZERO)))
					{
						continue; // backfacing
					}
#if __BANK
					if (debugDrawOpacity > 0.0f)
					{
						Vec3V clipped[NELEM(points) + 6];
						const int numClipped = PolyClipToFrustum(clipped, points, NELEM(points), m_viewport->GetCompositeMtx());

						for (int k = 0; k < numClipped; k++)
						{
							clipped[k] = AddScaled(camPos, clipped[k] - camPos, nearZ/Dot(clipped[k] - camPos, camDir));
						}

						for (int k = 0; k < numClipped; k++)
						{
							grcDebugDraw::Line(clipped[k], clipped[(k + 1)%numClipped], debugDrawColour);
						}
					}
					else
#endif // __BANK
					{
						if (RasteriseOccludePoly(points, NELEM(points)))
						{
							bBoxVisible = true;
						}
#if __BANK
						m_occludeStats.m_numPolysScene++;
#endif // __BANK
					}
				}
#if __BANK
				if (debugDrawOpacity == 0.0f)
				{
					if (bBoxVisible)
					{
						m_occludeStats.m_numBoxesVisible++;
					}

					m_occludeStats.m_numBoxes++;
				}
#endif // __BANK
			}

#if __BANK
			if (debugDrawOpacity == 0.0f)
			{
				m_occludeStats.m_numBoxesScene++;
			}
#endif // __BANK
		}
	}
}

void CRaster::RasteriseOccludeModels(BANK_ONLY(float debugDrawOpacity))
{
	if (m_depth == NULL)
	{
		return;
	}

#if __BANK
	const Color32 debugDrawColour(255, 255, 0, (int)(0.5f + 255.0f*debugDrawOpacity));
	const ScalarV nearZ(m_viewport->GetNearClip() + 0.1f);
#endif // __BANK

	const Vec3V camPos = +m_viewport->GetCameraMtx().GetCol3();
#if __BANK
	const Vec3V camDir = -m_viewport->GetCameraMtx().GetCol2();
#endif // __BANK

	const ScalarV bitPrecision(V_HALF);

	for (int i = 0; i < COcclusion::GetOccludeModelUsedBucketCount(); i++)
	{
		COccludeModelBucket* pBucket = COcclusion::GetOccludeModelBucket(i);
		Assert(pBucket);

		const COccludeModelArray* pArray = pBucket->pArray;
		Assert(pArray);

		for (int j = 0; j < pArray->GetCount(); j++)
		{
			const OccludeModel& occ = pArray->operator[](j);

			if (!occ.IsEmpty() && grcViewport::IsAABBVisible(occ.GetMin().GetIntrin128(), occ.GetMax().GetIntrin128(), m_viewport->GetCullFrustumLRTB()))
			{
				bool bModelVisible = false;

				Vec3V verts[rstFastTiler::MaxIndexedVerts];
				occ.UnPackVertices(verts, rstFastTiler::MaxIndexedVerts, bitPrecision);

				for (int j = 0; j < occ.GetNumIndices(); j += 3)
				{
					const Vec3V points[] =
					{
						verts[occ.GetIndices()[j + 0]],
						verts[occ.GetIndices()[j + 1]],
						verts[occ.GetIndices()[j + 2]],
					};

					if (IsGreaterThanOrEqualAll(Dot(points[0] - camPos, Cross(points[1] - points[0], points[2] - points[0])), ScalarV(V_ZERO)))
					{
						continue; // backfacing
					}
#if __BANK
					if (debugDrawOpacity > 0.0f)
					{
						Vec3V clipped[NELEM(points) + 6];
						const int numClipped = PolyClipToFrustum(clipped, points, NELEM(points), m_viewport->GetCompositeMtx());

						for (int k = 0; k < numClipped; k++)
						{
							clipped[k] = AddScaled(camPos, clipped[k] - camPos, nearZ/Dot(clipped[k] - camPos, camDir));
						}

						for (int k = 0; k < numClipped; k++)
						{
							grcDebugDraw::Line(clipped[k], clipped[(k + 1)%numClipped], debugDrawColour);
						}
					}
					else
#endif // __BANK
					{
						if (RasteriseOccludePoly(points, NELEM(points)))
						{
							bModelVisible = true;
						}
#if __BANK
						m_occludeStats.m_numPolysScene++;
#endif // __BANK
					}
				}
#if __BANK
				if (debugDrawOpacity == 0.0f)
				{
					if (bModelVisible)
					{
						m_occludeStats.m_numModelsVisible++;
					}

					m_occludeStats.m_numModels++;
				}
#endif // __BANK
			}
		}

#if __BANK
		if (debugDrawOpacity == 0.0f)
		{
			m_occludeStats.m_numModelsScene++;
		}
#endif // __BANK
	}
}

void CRaster::RasteriseWater(BANK_ONLY(float debugDrawOpacity))
{
	if (m_depth == NULL)
	{
		return;
	}

#if __BANK
	const Color32 debugDrawColour(0, 0, 255, (int)(0.5f + 255.0f*debugDrawOpacity));
	const ScalarV nearZ(m_viewport->GetNearClip() + 0.1f);
#endif // __BANK

	const Vec3V camPos = +m_viewport->GetCameraMtx().GetCol3();
#if __BANK
	const Vec3V camDir = -m_viewport->GetCameraMtx().GetCol2();
#endif // __BANK

	atArray<Water::CWaterPoly> polys;
	Water::GetWaterPolys(polys);

	for (int i = 0; i < polys.GetCount(); i++)
	{
		const Water::CWaterPoly& poly = polys[i];

		if (IsGreaterThanOrEqualAll(poly.m_points[0].GetZ(), camPos.GetZ()))
		{
			continue; // water poly is above camera, skip it
		}

		Vec3V pmin = poly.m_points[0];
		Vec3V pmax = pmin;

		for (int j = 1; j < poly.m_numPoints; j++)
		{
			pmin = Min(poly.m_points[j], pmin);
			pmax = Max(poly.m_points[j], pmax);
		}

		if (grcViewport::IsAABBVisible(pmin.GetIntrin128(), pmax.GetIntrin128(), m_viewport->GetCullFrustumLRTB()))
		{
#if __BANK
			if (debugDrawOpacity > 0.0f)
			{
				Vec3V clipped[NELEM(poly.m_points) + 6];
				const int numClipped = PolyClipToFrustum(clipped, poly.m_points, poly.m_numPoints, m_viewport->GetCompositeMtx());

				for (int k = 0; k < numClipped; k++)
				{
					clipped[k] = AddScaled(camPos, clipped[k] - camPos, nearZ/Dot(clipped[k] - camPos, camDir));
				}

				for (int k = 0; k < numClipped; k++)
				{
					grcDebugDraw::Line(clipped[k], clipped[(k + 1)%numClipped], debugDrawColour);
				}
			}
			else
#endif // __BANK
			{
				RasteriseStencilPoly(poly.m_points, poly.m_numPoints);
#if __BANK
				m_stencilStats.m_numPolysScene++;
#endif // __BANK
			}
		}
	}
}

void CRaster::RasteriseFinish()
{
#if __BANK
	if (m_control.m_perfectClip)
	{
		m_freeze    = true;
		m_freezeMtx = m_viewport->GetCompositeMtx();
	}
#endif // __BANK
}

#if __BANK

bool CRaster::IsFrozen()
{
	if (!m_control.m_perfectClip)
	{
		m_freeze = false;
	}

	if (m_freeze)
	{
		const Mat44V compMtx = m_viewport->GetCompositeMtx();

		const Vec4V diff0 = Abs(compMtx.GetCol0() - m_freezeMtx.GetCol0());
		const Vec4V diff1 = Abs(compMtx.GetCol1() - m_freezeMtx.GetCol1());
		const Vec4V diff2 = Abs(compMtx.GetCol2() - m_freezeMtx.GetCol2());
		const Vec4V diff3 = Abs(compMtx.GetCol3() - m_freezeMtx.GetCol3());

		if (MaxElement(Max(Max(diff0, diff1), Max(diff2, diff3))).Getf() > 0.01f)
		{
			m_freeze = false;
			m_control.m_perfectClip = false;
		}
	}

	return m_freeze;
}

void CRaster::DebugDraw(int ox, int oy, int scale) const
{
	if (m_depth == NULL)
	{
		return;
	}

	if (m_control.m_displayOpacity <= 0.0f)
	{
		return;
	}

	if (scale == 0)
	{
		scale = Min<int>(m_viewport->GetWidth()/m_w, m_viewport->GetHeight()/m_h);
	}

	const int opacity = (int)(0.5f + 255.0f*m_control.m_displayOpacity);

	const float scaleX = 1.0f/(m_viewport ? m_viewport->GetWidth() : 1280.0f);
	const float scaleY = 1.0f/(m_viewport ? m_viewport->GetHeight() : 720.0f);

	const int stencilStride = (m_w + 63)/64;

	float minZ = m_control.m_displayLinearZ0;
	float maxZ = m_control.m_displayLinearZ1;

	if (minZ == maxZ)
	{
		minZ = +FLT_MAX;
		maxZ = -FLT_MAX;

		for (int i = 0; i < m_w*m_h; i++)
		{
			if (m_depth[i] < FLT_MAX)
			{
				minZ = Min<float>(m_depth[i], minZ);
				maxZ = Max<float>(m_depth[i], maxZ);
			}
		}
	}
	else if (!m_control.m_useLinearZ)
	{
		minZ = ShaderUtil::GetSampleDepth(m_viewport, ScalarV(minZ)).Getf();
		maxZ = ShaderUtil::GetSampleDepth(m_viewport, ScalarV(maxZ)).Getf();
	}

	const bool bDrawNiceStencilRegion = (opacity < 255);

	if (!m_control.m_displayStencilOnly || !bDrawNiceStencilRegion)
	{
		for (int j = 0; j < m_h; j++)
		{
			const float* depthRow = &m_depth[j*m_w];
			const u64* stencilRow = &m_stencil[j*stencilStride];

			for (int i = 0; i < m_w; i++)
			{
				if (!m_control.m_displayStencilOnly)
				{
					if (depthRow[i] < FLT_MAX)
					{
						const int shade = (int)(0.5f + 255.0f*Clamp<float>((depthRow[i] - maxZ)/(minZ - maxZ), 0.0f, 1.0f));

						const Vec2V p00((float)(ox + (i + 0)*scale)*scaleX, (float)(oy + (j + 0)*scale)*scaleY);
						const Vec2V p10((float)(ox + (i + 1)*scale)*scaleX, (float)(oy + (j + 0)*scale)*scaleY);
						const Vec2V p01((float)(ox + (i + 0)*scale)*scaleX, (float)(oy + (j + 1)*scale)*scaleY);
						const Vec2V p11((float)(ox + (i + 1)*scale)*scaleX, (float)(oy + (j + 1)*scale)*scaleY);

						grcDebugDraw::Quad(p00, p10, p11, p01, Color32(shade, shade, shade, opacity), true, true);
					}
				}

				if (!bDrawNiceStencilRegion) // draw stencil as little dots, kind of lame ..
				{
					if (stencilRow[i/64] & BIT64(63 - i%64))
					{
						const int inset = 2;

						const Vec2V p00((float)(ox + (i + 0)*scale + inset)*scaleX, (float)(oy + (j + 0)*scale + inset)*scaleY);
						const Vec2V p10((float)(ox + (i + 1)*scale - inset)*scaleX, (float)(oy + (j + 0)*scale + inset)*scaleY);
						const Vec2V p01((float)(ox + (i + 0)*scale + inset)*scaleX, (float)(oy + (j + 1)*scale - inset)*scaleY);
						const Vec2V p11((float)(ox + (i + 1)*scale - inset)*scaleX, (float)(oy + (j + 1)*scale - inset)*scaleY);

						grcDebugDraw::Quad(p00, p10, p11, p01, Color32(0, 0, 255, opacity), true, true);
					}
				}
			}
		}
	}

	if (bDrawNiceStencilRegion) // draw stencil region as strips and edges, looks better and produces far less debug primitives
	{
		for (int j = 0; j < m_h; j++)
		{
			const u64* stencilRow = &m_stencil[j*stencilStride];
			int i0 = -1;

			for (int i = 0; i <= m_w; i++)
			{
				if (i < m_w && (stencilRow[i/64] & BIT64(63 - i%64)) != 0)
				{
					if (i0 == -1)
					{
						i0 = i;
					}
				}
				else
				{
					if (i0 != -1)
					{
						const int i1 = i;

						const Vec2V p00((float)(ox + i0*scale)*scaleX, (float)(oy + (j + 0)*scale)*scaleY);
						const Vec2V p10((float)(ox + i1*scale)*scaleX, (float)(oy + (j + 0)*scale)*scaleY);
						const Vec2V p01((float)(ox + i0*scale)*scaleX, (float)(oy + (j + 1)*scale)*scaleY);
						const Vec2V p11((float)(ox + i1*scale)*scaleX, (float)(oy + (j + 1)*scale)*scaleY);

						grcDebugDraw::Quad(p00, p10, p11, p01, Color32(0, 0, 255, opacity/2), true, true);

						i0 = -1;
					}
				}
			}
		}

		for (int j = 1; j < m_h; j++) // draw horizontal edges between rows j-1 and j
		{
			const u64* stencilRow0 = &m_stencil[(j - 1)*stencilStride];
			const u64* stencilRow1 = &m_stencil[(j - 0)*stencilStride];
			int i0 = -1;

			for (int i = 0; i <= m_w; i++)
			{
				const bool bStencil0 = (stencilRow0[Min<int>(i, m_w - 1)/64] & BIT64(63 - Min<int>(i, m_w - 1)%64)) != 0;
				const bool bStencil1 = (stencilRow1[Min<int>(i, m_w - 1)/64] & BIT64(63 - Min<int>(i, m_w - 1)%64)) != 0;

				if (i < m_w && bStencil0 != bStencil1)
				{
					if (i0 == -1)
					{
						i0 = i;
					}
				}
				else
				{
					if (i0 != -1)
					{
						const int i1 = i;

						const Vec2V p00((float)(ox + i0*scale)*scaleX, (float)(oy + j*scale)*scaleY);
						const Vec2V p10((float)(ox + i1*scale)*scaleX, (float)(oy + j*scale)*scaleY);

						grcDebugDraw::Line(p00, p10, Color32(0, 80, 255, opacity));

						i0 = -1;
					}
				}
			}
		}

		for (int i = 1; i < m_w; i++) // draw vertical edges between columns i-1 and i
		{
			int j0 = -1;

			for (int j = 0; j <= m_h; j++)
			{
				const u64* stencilRow = &m_stencil[Min<int>(j, m_h - 1)*stencilStride];

				const bool bStencil0 = (stencilRow[(i - 1)/64] & BIT64(63 - (i - 1)%64)) != 0;
				const bool bStencil1 = (stencilRow[(i - 0)/64] & BIT64(63 - (i - 0)%64)) != 0;

				if (j < m_h && bStencil0 != bStencil1)
				{
					if (j0 == -1)
					{
						j0 = j;
					}
				}
				else
				{
					if (j0 != -1)
					{
						const int j1 = j;

						const Vec2V p00((float)(ox + i*scale)*scaleX, (float)(oy + j0*scale)*scaleY);
						const Vec2V p01((float)(ox + i*scale)*scaleX, (float)(oy + j1*scale)*scaleY);

						grcDebugDraw::Line(p00, p01, Color32(0, 80, 255, opacity));

						j0 = -1;
					}
				}
			}
		}
	}
}

#if RASTER_GEOM_BUFFER

void CRaster::DebugDrawGeometry(int ox, int oy, int scale, Color32 occludeEdge, Color32 occludeFill, Color32 stencilEdge, Color32 stencilFill) const
{
	if (m_depth == NULL)
	{
		return;
	}

	const float scaleX = 1.0f/(m_viewport ? m_viewport->GetWidth() : 1280.0f);
	const float scaleY = 1.0f/(m_viewport ? m_viewport->GetHeight() : 720.0f);

	if (occludeFill.GetAlpha() > 0)
	{
		for (int triIdx = 0; triIdx < m_occludeGeometry.GetNumTris(); triIdx++)
		{
			Vec3V points[3];
			m_occludeGeometry.GetTri(points, triIdx);

			const Vec2V p0 = (Vec2V((float)ox, (float)oy) + points[0].GetXY()*ScalarV((float)scale))*Vec2V(scaleX, scaleY);
			const Vec2V p1 = (Vec2V((float)ox, (float)oy) + points[1].GetXY()*ScalarV((float)scale))*Vec2V(scaleX, scaleY);
			const Vec2V p2 = (Vec2V((float)ox, (float)oy) + points[2].GetXY()*ScalarV((float)scale))*Vec2V(scaleX, scaleY);

			grcDebugDraw::Poly(p0, p1, p2, occludeFill, true, true);
		}
	}

	if (occludeEdge.GetAlpha() > 0)
	{
		for (int triIdx = 0; triIdx < m_occludeGeometry.GetNumTris(); triIdx++)
		{
			Vec3V points[3];
			m_occludeGeometry.GetTri(points, triIdx);

			const Vec2V p0 = (Vec2V((float)ox, (float)oy) + points[0].GetXY()*ScalarV((float)scale))*Vec2V(scaleX, scaleY);
			const Vec2V p1 = (Vec2V((float)ox, (float)oy) + points[1].GetXY()*ScalarV((float)scale))*Vec2V(scaleX, scaleY);
			const Vec2V p2 = (Vec2V((float)ox, (float)oy) + points[2].GetXY()*ScalarV((float)scale))*Vec2V(scaleX, scaleY);

			grcDebugDraw::Line(p0, p1, occludeEdge);
			grcDebugDraw::Line(p1, p2, occludeEdge);
			grcDebugDraw::Line(p2, p0, occludeEdge);
		}
	}

	if (stencilFill.GetAlpha() > 0)
	{
		for (int triIdx = 0; triIdx < m_stencilGeometry.GetNumTris(); triIdx++)
		{
			Vec3V points[3];
			m_stencilGeometry.GetTri(points, triIdx);

			const Vec2V p0 = (Vec2V((float)ox, (float)oy) + points[0].GetXY()*ScalarV((float)scale))*Vec2V(scaleX, scaleY);
			const Vec2V p1 = (Vec2V((float)ox, (float)oy) + points[1].GetXY()*ScalarV((float)scale))*Vec2V(scaleX, scaleY);
			const Vec2V p2 = (Vec2V((float)ox, (float)oy) + points[2].GetXY()*ScalarV((float)scale))*Vec2V(scaleX, scaleY);

			grcDebugDraw::Poly(p0, p1, p2, stencilFill, true, true);
		}
	}

	if (stencilEdge.GetAlpha() > 0)
	{
		for (int triIdx = 0; triIdx < m_stencilGeometry.GetNumTris(); triIdx++)
		{
			Vec3V points[3];
			m_stencilGeometry.GetTri(points, triIdx);

			const Vec2V p0 = (Vec2V((float)ox, (float)oy) + points[0].GetXY()*ScalarV((float)scale))*Vec2V(scaleX, scaleY);
			const Vec2V p1 = (Vec2V((float)ox, (float)oy) + points[1].GetXY()*ScalarV((float)scale))*Vec2V(scaleX, scaleY);
			const Vec2V p2 = (Vec2V((float)ox, (float)oy) + points[2].GetXY()*ScalarV((float)scale))*Vec2V(scaleX, scaleY);

			grcDebugDraw::Line(p0, p1, stencilEdge);
			grcDebugDraw::Line(p1, p2, stencilEdge);
			grcDebugDraw::Line(p2, p0, stencilEdge);

		}
	}
}

#endif // RASTER_GEOM_BUFFER
#endif // __BANK

// ================================================================================================

static CRaster g_raster;

__COMMENT(static) CRasterControl& CRasterInterface::GetControlRef()
{
	return g_raster.m_control;
}

__COMMENT(static) bool CRasterInterface::IsInited()
{
	return g_raster.m_depth != NULL;
}

__COMMENT(static) void CRasterInterface::Init(const grcViewport* vp, int w, int h, int maxOccludeVerts, int maxStencilVerts)
{
	g_raster.Init(vp, w, h, NULL, (u64*)COcclusion::GetHiStencilBuffer(), maxOccludeVerts, maxStencilVerts);
}

__COMMENT(static) void CRasterInterface::Clear(const grcViewport* vp)
{
	g_raster.Clear(vp);
}

__COMMENT(static) void CRasterInterface::RasteriseOccludePoly(const Vec3V* points, int numPoints, bool bClipToFrustum)
{
	g_raster.RasteriseOccludePoly(points, numPoints, bClipToFrustum);
}

__COMMENT(static) void CRasterInterface::RasteriseStencilPoly(const Vec3V* points, int numPoints, bool bClipToFrustum)
{
	g_raster.RasteriseStencilPoly(points, numPoints, bClipToFrustum);
}

#if RASTER_GEOM_BUFFER

__COMMENT(static) void CRasterInterface::RegisterOccludePoly(const Vec3V* points, int numPoints)
{
	g_raster.RegisterOccludePoly(points, numPoints);
}

__COMMENT(static) void CRasterInterface::RegisterStencilPoly(const Vec3V* points, int numPoints)
{
	g_raster.RegisterStencilPoly(points, numPoints);
}

__COMMENT(static) void CRasterInterface::RasteriseOccludeGeometry()
{
	g_raster.RasteriseOccludeGeometry();
}

__COMMENT(static) void CRasterInterface::RasteriseStencilGeometry()
{
	g_raster.RasteriseStencilGeometry();
}

#endif // RASTER_GEOM_BUFFER

#if __BANK
static CRasterStatistics g_occludeStats; // copies so we can hook them directly to rag sliders for display
static CRasterStatistics g_stencilStats;
#endif // __BANK

__COMMENT(static) void CRasterInterface::RasteriseOccludersAndWater(bool bBoxes, bool bMeshes, bool bWater)
{
	if (bBoxes ) { g_raster.RasteriseOccludeBoxes (); }
	if (bMeshes) { g_raster.RasteriseOccludeModels(); }
	if (bWater ) { g_raster.RasteriseWater        (); }

	g_raster.RasteriseFinish();
#if __BANK
	g_occludeStats = g_raster.m_occludeStats;
	g_stencilStats = g_raster.m_stencilStats;
#endif // __BANK
}

__COMMENT(static) int CRasterInterface::GetWidth()
{
	return g_raster.m_w;
}

__COMMENT(static) int CRasterInterface::GetHeight()
{
	return g_raster.m_h;
}

__COMMENT(static) const float* CRasterInterface::GetDepthData()
{
	return g_raster.m_depth;
}

__COMMENT(static) const u64* CRasterInterface::GetStencilData()
{
	return g_raster.m_stencil;
}

#if __BANK

__COMMENT(static) CRasterStatistics& CRasterInterface::GetOccludeStatsRef()
{
	return g_occludeStats;
}

__COMMENT(static) CRasterStatistics& CRasterInterface::GetStencilStatsRef()
{
	return g_stencilStats;
}

__COMMENT(static) void CRasterInterface::PerfectClip()
{
	g_raster.m_control.m_perfectClip = true;
}

__COMMENT(static) void CRasterInterface::DebugDraw()
{
	g_raster.DebugDraw();
}

__COMMENT(static) void CRasterInterface::DebugDrawOccludersAndWater(bool bBoxes, bool bMeshes, bool bWater, float opacity)
{
	if (opacity > 0.0f)
	{
		if (bBoxes ) { g_raster.RasteriseOccludeBoxes (opacity); }
		if (bMeshes) { g_raster.RasteriseOccludeModels(opacity); }
		if (bWater ) { g_raster.RasteriseWater        (opacity); }
	}
}

static fiStream*       g_occluderOBJ = NULL;
static int             g_occluderOBJNumVerts = 0;
static atMap<u32,bool> g_occluderOBJDumped; // keep track of which occluders have been dumped

__COMMENT(static) void CRasterInterface::DumpOccludersToOBJ_Begin(const char* path)
{
	if (g_occluderOBJ == NULL)
	{
		g_occluderOBJ = fiStream::Create(path);
	}
}

__COMMENT(static) void CRasterInterface::DumpOccludersToOBJ_End()
{
	if (g_occluderOBJ)
	{
		for (int i = 0; i < g_occluderOBJNumVerts; i += 3)
		{
			fprintf(g_occluderOBJ, "f %d %d %d\n", 1 + i, 2 + i, 3 + i);
		}

		Displayf("dumped occluder obj file with %d verts", g_occluderOBJNumVerts);

		g_occluderOBJ->Close();
		g_occluderOBJ = NULL;
		g_occluderOBJNumVerts = 0;
		g_occluderOBJDumped.Reset();
	}
}

__COMMENT(static) void CRasterInterface::DumpOccludersToOBJ_Update(bool bWaterOccluders)
{
	if (g_occluderOBJ)
	{
		if (!bWaterOccluders)
		{
			for (int i = 0; i < COcclusion::GetBoxOccluderUsedBucketCount(); i++)
			{
				CBoxOccluderBucket* pBucket = COcclusion::GetBoxOccluderBucket(i);
				Assert(pBucket);

				const CBoxOccluderArray* pArray = pBucket->pArray;
				Assert(pArray);

				for (int j = 0; j < pArray->GetCount(); j++)
				{
					const BoxOccluder& occ = pArray->operator[](j);
					const u32 hash = atDataHash((const char*)&occ, sizeof(occ));

					if (g_occluderOBJDumped.Access(hash) == NULL)
					{
						g_occluderOBJDumped[hash] = true;

						Displayf("dumping box occluder to obj [0x%08x]", hash);

						Vec3V verts[8];
						occ.CalculateVerts(verts);

						for (int j = 0; j < occ.GetNumOccluderIndices(); j += 3)
						{
							const Vec3V points[] =
							{
								verts[occ.GetOccluderIndices()[j + 0]],
								verts[occ.GetOccluderIndices()[j + 1]],
								verts[occ.GetOccluderIndices()[j + 2]],
							};

							fprintf(g_occluderOBJ, "v %f %f %f\n", VEC3V_ARGS(points[0]));
							fprintf(g_occluderOBJ, "v %f %f %f\n", VEC3V_ARGS(points[1]));
							fprintf(g_occluderOBJ, "v %f %f %f\n", VEC3V_ARGS(points[2]));

							g_occluderOBJ->Flush();
							g_occluderOBJNumVerts += 3;
						}
					}
				}
			}
		}

		const ScalarV bitPrecision(V_HALF);

		for (int i = 0; i < COcclusion::GetOccludeModelUsedBucketCount(); i++)
		{
			COccludeModelBucket* pBucket = COcclusion::GetOccludeModelBucket(i);
			Assert(pBucket);

			const COccludeModelArray* pArray = pBucket->pArray;
			Assert(pArray);

			for (int j = 0; j < pArray->GetCount(); j++)
			{
				const OccludeModel& occ = pArray->operator[](j);
				const bool bIsWaterOccluder = (occ.GetFlags() & OccludeModel::FLAG_WATER_ONLY) != 0;

				if (bIsWaterOccluder == bWaterOccluders && !occ.IsEmpty())
				{
					const Vec3V bmin = occ.GetMin();
					const Vec3V bmax = occ.GetMax();

					u32 hash = 0;

					hash = atDataHash((const char*)&bmin, 3*sizeof(float), hash);
					hash = atDataHash((const char*)&bmax, 3*sizeof(float), hash);

					if (g_occluderOBJDumped.Access(hash) == NULL)
					{
						g_occluderOBJDumped[hash] = true;

						Displayf("dumping mesh occluder to obj [0x%08x], %d triangles", hash, occ.GetNumTris());

						Vec3V verts[rstFastTiler::MaxIndexedVerts];
						occ.UnPackVertices(verts, rstFastTiler::MaxIndexedVerts, bitPrecision);

						for (int j = 0; j < occ.GetNumIndices(); j += 3)
						{
							const Vec3V points[] =
							{
								verts[occ.GetIndices()[j + 0]],
								verts[occ.GetIndices()[j + 1]],
								verts[occ.GetIndices()[j + 2]],
							};

							fprintf(g_occluderOBJ, "v %f %f %f\n", VEC3V_ARGS(points[0]));
							fprintf(g_occluderOBJ, "v %f %f %f\n", VEC3V_ARGS(points[1]));
							fprintf(g_occluderOBJ, "v %f %f %f\n", VEC3V_ARGS(points[2]));

							g_occluderOBJ->Flush();
							g_occluderOBJNumVerts += 3;
						}
					}
				}
			}
		}
	}
}

#endif // __BANK

#endif // RASTER
