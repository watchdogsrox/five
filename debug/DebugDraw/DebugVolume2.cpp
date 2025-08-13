// ================================
// debug/DebugDraw/DebugVolume2.cpp
// (c) 2011 RockstarNorth
// ================================

#if __BANK

/*
Dear lord, another debug volume system?

CDV is a new system intended to replace my older CDebugVolume system. While the old system has a
lot of functionality, there are several aspects I'm not happy with .. once the new system is
complete we should be able to remove the old system. Currently the old system is used in three
places:

	1. Faceted shadow debugging (which will go away once CSM is ready), both for the locked camera
and to generate the shadow clip plane volume.

	2. A picker-type thingy called Scene Debug Volume which was used for visualising kDOP18's,
which I'd like to revisit in the future but the current implementation is pretty useless.

	3. Cascade shadow debugging, but only for the locked camera.

The new system uses the new vectormath library, and I've tried to keep it as vectorised as
possible while still being maintainable. For example, objects like spheres are constructed via
a function called "LatheInternal" which takes an array of {sin,cos,1} slice vectors and an
array of {r,r,z} stack vectors, which are then multiplied together to produce the mesh vertices.
Another important difference in the new system is that it is not dependent on a "static" debug
volume container and on a rag-dependent global colourscheme. The new system should be fast
enough that geometry can be constructed in-place and rendered immediately instead of storing the
geometry.

Also, the old system used fwDebugDraw exclusively, while the new one can draw through either
fwDebugDraw or grcVertex. I'm not sure if this is important or not, but I wanted the new system
to be accessible through both the update and render threads.

Here are the features that still need to be implemented, some of them are present in the old
system and might be able to be ported over:

	1. Ability to construct a convex volume purely via the intersection of halfspaces - this was
needed for visualising the faceted shadow clip plane volume when I didn't understand how the clip
planes were constructed. It's also a handy "brute force" way to construct a volume if you're too
lazy to specify how the faces are connected. Also I think the old system was able to merge
coincident verts and find edges automatically, which is also great for lazy geometry.

	2. A more elaborate colourstyle scheme where the colour and opacity of the surfaces can depend
on the viewing angle. While both systems support multi-pass drawing where backfaces are drawn
before frontfaces and surfaces before edges (etc.), the angle-dependent colourstyle was nice and
pretty and I'd like to have the option to use it.

	3. Both systems can represent "grid lines" which are cosmetic edges internal to a particular
face/surface. These greatly improve the look of frustum-shaped volumes without having to resort to
constructing (and drawing) large meshes of connected quads and evaluating all the edges between
them .. but the old system's frustum constructor handled these better (the new one doesn't even
construct them). I'd actually like to use these to represent the stack edges for cones as well.

	4. The old system could construct a kDOP18 from a debug volume automatically and add the
kDOP18's geometry to the volume, which was handy. The kDOP18 code could pretty easily be moved
to a separate class and shared, I think ..

	5. I would like the new debug volume system to eventually maintain a list of volumes which can
be sorted back-to-front before rendering. To do this I think I will make a virtual CDVGeomProxy
class which can draw itself, and a CDVGeomGroup class to contain an array of these proxies. This
isn't necessary for things like camera lock volumes but might be important for visualising light
volumes that are scattered throughout the scene.
*/

// ================================================================================================

/*
Rotationally-symmetric shapes are constructed by first calculating two Vec3V arrays

	Vec3V xy1[slices];
	Vec3V rrz[stacks + 1];

Some shapes (Box, BoxProj, Frustum) aren't described this way, mainly because of the way gridlines
are applied.

There are two codepaths to draw shapes using the arrays. CDVGeom::LatheInternal followed by Draw
which constructs vertice, edges, quads, etc. and can be quite slow, but produces very pretty
results because it draws backfacing geometry before frontfacing, and surfaces before edges, with
silhoette edges last. The other codepath is CDVGeom::LatheInternal_DrawImmediate which does not
construct anything but only handles single-colour solid or wireframe geometry.
*/

#include "bank/bank.h"
#include "bank/bkmgr.h"
#include "grcore/debugdraw.h"
#include "grcore/viewport.h"
#include "grcore/stateblock.h"
#include "system/memory.h"
#include "system/param.h"

#include "fwutil/xmacro.h"
#include "fwmaths/vectorutil.h"

#include "debug/BankUtil.h"
#include "debug/DebugDraw/DebugVolume2.h"
#include "debug/DebugDraw/DebugVolume_kDOP18.h"
#include "shaders/ShaderLib.h"

RENDER_OPTIMISATIONS()

// ================================================================================================

class CkDOP18;
class CDVGeom;

class CDVVert
{
public:
	__forceinline CDVVert() {}
	__forceinline CDVVert(Vec3V_In position);

	Vec3V m_position;
};

class CDVSurfIndex
{
public:
	__forceinline CDVSurfIndex() {}
	__forceinline CDVSurfIndex(int quadOrFaceIndex, bool isFaceIndex = false);

	__forceinline bool operator ==(const CDVSurfIndex& rhs) const;
	__forceinline bool operator !=(const CDVSurfIndex& rhs) const;

	s32 m_quadOrFaceIndex : 31; // index into m_quads[] or m_faces[]
	u32 m_isFaceIndex     : 1;  // 0=quad, 1=face
};

class CDVLine
{
public:
	__forceinline CDVLine() {}
	__forceinline CDVLine(Vec3V_In p0, Vec3V_In p1, const CDVSurfIndex& si);

	void Draw(const CDVGeom* geom, const Color32& colour, bool fw, atArray<Vec3V>& verts) const;

	Vec3V        m_points[2];
	CDVSurfIndex m_surfIndex;
};

class CDVEdge
{
public:
	__forceinline CDVEdge() {}
	__forceinline CDVEdge(int vi0, int vi1, const CDVSurfIndex& si0, const CDVSurfIndex& si1);

	void Draw(const CDVGeom* geom, const Color32& colour, bool fw, atArray<Vec3V>& verts) const;

	int          m_vertIndices[2]; // index into m_verts[]
	CDVSurfIndex m_surfIndices[2]; // index into m_quads[] or m_faces[]
};

class CDVSurf
{
public:
	Vec3V m_centroid;
	Vec3V m_normal;
};

class CDVQuad : public CDVSurf // 4-sided face, most common
{
public:
	__forceinline CDVQuad() {}
	__forceinline CDVQuad(int vi0, int vi1, int vi2, int vi3);

	void Draw(const CDVGeom* geom, const Color32& colour, bool fw, atArray<Vec3V>& verts) const;

	int m_vertIndices[4];
};

class CDVFace : public CDVSurf // n-sided face, e.g. endcaps
{
public:
	void Draw(const CDVGeom* geom, const Color32& colour, bool fw, atArray<Vec3V>& verts) const;

	atArray<int> m_vertIndices;
};

class CDVGeom
{
public:
	enum
	{
		MAX_SLICES = 256,
		MAX_STACKS = 256,
	};

	CDVGeom(const CDVColourStyle* colourStyle) : m_colourStyle(colourStyle) {}

	CDVGeom& BuildKDOP18(const CkDOP18& kDOP18, float tolerance);

	void AddVerticesToKDOP18(CkDOP18& kDOP18) const;

	void Draw(Vec3V_In camPos, float opacity, bool fw) const;

private:
	void DrawPassInternal(int pass, const CDVColourStyle& colourStyle, const float* NdotV_quads, const float* NdotV_faces, bool fw) const;

	void Clear();

	CDVGeom& BuildBoxInternal(
		const Vec3V points[8],
		int         stacksX,
		int         stacksY,
		int         stacksZ,
		ScalarV_In  perspective
	);
	CDVGeom& LatheInternal(
		Mat34V_In    basis,
		const Vec3V* xy1, // [slices]
		const Vec3V* rrz, // [stacks + 1]
		int          slices,
		int          stacks,
		bool         faceGrid
	);

	// In some cases it might be desirable to draw shapes in a more immediate mode, i.e. not
	// constructing CDVGeom and then iterating over the arrays but instead just calling grcVertex
	// directly. To re-use the code that constructs the xy1 and rrz arrays, we could just call an
	// immediate version of LatheInternal instead.
	static void LatheInternal_DrawImmediate(
		Mat34V_In      basis,
		const Vec3V*   xy1, // [slices]
		const Vec3V*   rrz, // [stacks + 1]
		int            slices,
		int            stacks,
		int            slicesStep,
		int            stacksStep,
		const Color32& colour,
		float          opacity,
		bool           bSolid
	);

	static void LatheInternal_DrawImmediate(
		Vec3V_In       origin,
		const Vec3V*   xy1, // [slices]
		const Vec3V*   rrz, // [stacks + 1]
		int            slices,
		int            stacks,
		int            slicesStep,
		int            stacksStep,
		const Color32& colour,
		float          opacity,
		bool           bSolid
	);

	// Primitive types where the rrz[] arrays are arranged linearly can be drawn even more
	// efficiently, e.g. cylinders and cones.
	static void LatheInternal_DrawImmediate(
		Mat34V_In      basis,
		const Vec3V*   xy1, // [slices]
		Vec3V_In       rrz0,
		Vec3V_In       rrz1,
		const ScalarV* rrzt, // rrz[j] is constructed as rrz0 + (rrz1 - rrz0)*rrzt[j]
		int            slices,
		int            stacks,
		int            slicesStep,
		int            stacksStep,
		const Color32& colour,
		float          opacity,
		bool           bSolid
	);

	void AddGridLinesToQuad(int quadIndex, int w, int h, float perspective);
	void AddGridLinesToFace(int faceIndex, bool bStep3 = false);

	// connected geometry - brute-force and slow!
	int  AddConnectedVertex(Vec3V_In point);
	bool AddConnectedEdge(int vertIndex0, int vertIndex1, const CDVSurfIndex& si);
	bool AddConnectedFace(const atArray<Vec3V>& points);

	void CalcNormals();

#if __DEV
	int  CheckWinding(const CDVSurfIndex& si, int vertIndex0, int vertIndex1) const;
	bool Check() const;
#endif // __DEV

	const CDVColourStyle* m_colourStyle;

	atArray<CDVVert> m_verts;
	atArray<CDVLine> m_lines;
	atArray<CDVEdge> m_edges;
	atArray<CDVQuad> m_quads;
	atArray<CDVFace> m_faces;

	friend class CDVVert;
	friend class CDVLine;
	friend class CDVEdge;
	friend class CDVQuad;
	friend class CDVFace;

#define DEF_CDVGeomParams(type) friend class CDVGeomParams##type;
FOREACH(DEF_CDVGeomParams)
#undef  DEF_CDVGeomParams
};

__forceinline CDVVert::CDVVert(Vec3V_In position) : m_position(position) {}

__forceinline CDVSurfIndex::CDVSurfIndex(int quadOrFaceIndex, bool isFaceIndex) : m_quadOrFaceIndex(quadOrFaceIndex), m_isFaceIndex(isFaceIndex ? 1 : 0) {}

__forceinline bool CDVSurfIndex::operator ==(const CDVSurfIndex& rhs) const
{
	return m_quadOrFaceIndex == rhs.m_quadOrFaceIndex && m_isFaceIndex == rhs.m_isFaceIndex;
}

__forceinline bool CDVSurfIndex::operator !=(const CDVSurfIndex& rhs) const
{
	return m_quadOrFaceIndex != rhs.m_quadOrFaceIndex || m_isFaceIndex != rhs.m_isFaceIndex;
}

__forceinline CDVLine::CDVLine(Vec3V_In p0, Vec3V_In p1, const CDVSurfIndex& si)
{
	m_points[0] = p0;
	m_points[1] = p1;
	m_surfIndex = si;
}

void CDVLine::Draw(const CDVGeom* geom, const Color32& colour, bool fw, atArray<Vec3V>& verts) const
{
	UNUSED_VAR(geom);

	if (fw)
	{
		grcDebugDraw::Line(
			m_points[0],
			m_points[1],
			colour
		);
	}
	else
	{
		verts.PushAndGrow(m_points[0]);
		verts.PushAndGrow(m_points[1]);
	}
}

__forceinline CDVEdge::CDVEdge(int vi0, int vi1, const CDVSurfIndex& si0, const CDVSurfIndex& si1)
{
	m_vertIndices[0] = vi0;
	m_vertIndices[1] = vi1;
	m_surfIndices[0] = si0;
	m_surfIndices[1] = si1;
}

void CDVEdge::Draw(const CDVGeom* geom, const Color32& colour, bool fw, atArray<Vec3V>& verts) const
{
	if (fw)
	{
		grcDebugDraw::Line(
			geom->m_verts[m_vertIndices[0]].m_position,
			geom->m_verts[m_vertIndices[1]].m_position,
			colour
		);
	}
	else
	{
		verts.PushAndGrow(geom->m_verts[m_vertIndices[0]].m_position);
		verts.PushAndGrow(geom->m_verts[m_vertIndices[1]].m_position);
	}
}

__forceinline CDVQuad::CDVQuad(int vi0, int vi1, int vi2, int vi3)
{
	m_vertIndices[0] = vi0;
	m_vertIndices[1] = vi1;
	m_vertIndices[2] = vi2;
	m_vertIndices[3] = vi3;
}

void CDVQuad::Draw(const CDVGeom* geom, const Color32& colour, bool fw, atArray<Vec3V>& verts) const
{
	if (fw)
	{
		grcDebugDraw::Quad(
			geom->m_verts[m_vertIndices[0]].m_position,
			geom->m_verts[m_vertIndices[1]].m_position,
			geom->m_verts[m_vertIndices[2]].m_position,
			geom->m_verts[m_vertIndices[3]].m_position,
			colour
		);
	}
	else
	{
		verts.PushAndGrow(geom->m_verts[m_vertIndices[0]].m_position);
		verts.PushAndGrow(geom->m_verts[m_vertIndices[1]].m_position);
		verts.PushAndGrow(geom->m_verts[m_vertIndices[2]].m_position);
		verts.PushAndGrow(geom->m_verts[m_vertIndices[3]].m_position);
	}
}

void CDVFace::Draw(const CDVGeom* geom, const Color32& colour, bool fw, atArray<Vec3V>& verts) const
{
	if (fw)
	{
		Vec3V p0 = geom->m_verts[m_vertIndices[0]].m_position;
		Vec3V p1 = geom->m_verts[m_vertIndices[1]].m_position;

		for (int i = 2; i < m_vertIndices.size(); i++)
		{
			const Vec3V p2 = geom->m_verts[m_vertIndices[i]].m_position;

			grcDebugDraw::Poly(p0, p1, p2, colour);

			p1 = p2;
		}
	}
	else
	{
		if (verts.size() > 0) // connect faces with degenerate triangles
		{
			if (verts.size() & 1) // keep winding order consistent
			{
				verts.PushAndGrow(verts.back());
			}

			verts.PushAndGrow(verts.back()); // dup last vert
			verts.PushAndGrow(geom->m_verts[m_vertIndices[0]].m_position); // dup next vert
		}

		const int n = m_vertIndices.size();

		int i0 = 0;
		int i1 = 1;

		do
		{
			verts.PushAndGrow(geom->m_verts[m_vertIndices[i0]].m_position);

			if (i1 != i0)
			{
				verts.PushAndGrow(geom->m_verts[m_vertIndices[i1]].m_position);
			}

			i0 -= 1; if (i0 <  0) { i0 += n; }
			i1 += 1; if (i1 >= n) { i1 -= n; }
		}
		while (i0 > i1);
	}
}

// ================================================================================================

CDVColourStyle::CDVColourStyle(const CDVColourStyle& parent, float opacity)
{
	for (int i = 0; i < DVP_COUNT; i++)
	{
		m_colours[i] = parent.m_colours[i];
	}

	if (opacity < 1.0f)
	{
		for (int i = 0; i < DVP_COUNT; i++)
		{
			m_colours[i].SetAlpha((int)(opacity*(float)parent.m_colours[i].GetAlpha()));
		}
	}
}

void CDVColourStyle::InitWidgets(bkBank& bk)
{
	m_colours[DVP_BACK_SURFS      ] = Color32(  0,  0,  0, 24);
	m_colours[DVP_BACK_LINES      ] = Color32(  0,  0,  0, 32);
	m_colours[DVP_BACK_EDGES      ] = Color32(  0,  0, 64, 64);
	m_colours[DVP_FRONT_SURFS     ] = Color32(  0,  0,128, 64);
	m_colours[DVP_FRONT_LINES     ] = Color32(255,255,255, 96);
	m_colours[DVP_FRONT_EDGES     ] = Color32(  0,  0,  0, 96);
	m_colours[DVP_SILHOUETTE_EDGES] = Color32(  0,  0,  0,255);

	bk.AddColor("DVP_BACK_SURFS"      , &m_colours[DVP_BACK_SURFS      ]);
	bk.AddColor("DVP_BACK_LINES"      , &m_colours[DVP_BACK_LINES      ]);
	bk.AddColor("DVP_BACK_EDGES"      , &m_colours[DVP_BACK_EDGES      ]);
	bk.AddColor("DVP_FRONT_SURFS"     , &m_colours[DVP_FRONT_SURFS     ]);
	bk.AddColor("DVP_FRONT_LINES"     , &m_colours[DVP_FRONT_LINES     ]);
	bk.AddColor("DVP_FRONT_EDGES"     , &m_colours[DVP_FRONT_EDGES     ]);
	bk.AddColor("DVP_SILHOUETTE_EDGES", &m_colours[DVP_SILHOUETTE_EDGES]);
}

// ================================================================================================

// just for clarity ..
static __forceinline Vec3V_Out PermuteXXY(Vec3V_In v) { return v.Get<Vec::X,Vec::X,Vec::Y>(); }
static __forceinline Vec3V_Out PermuteYYX(Vec3V_In v) { return v.Get<Vec::Y,Vec::Y,Vec::X>(); }

// ================================================================================================

#define CDV_DRAW_PROXY (1)

template <typename T> class ptr_container
{
public:
	__forceinline ptr_container(const T& data) : m_data(data) {}
	__forceinline const T* get_ptr() const { return &m_data; }
private:
	const T m_data;
};

template <typename T> class ptr_container<T*>
{
public:
	__forceinline ptr_container(const T* data) : m_data(data) {}
	__forceinline const T* get_ptr() const { return m_data; }
private:
	const T* m_data;
};

template <typename T> class ptr_container<T&>
{
public:
	__forceinline ptr_container(const T& data) : m_data(data) {}
	__forceinline const T* get_ptr() const { return &m_data; }
private:
	const T m_data;
};

/*
class foo3
{
public:
	foo3() {}
	foo3(int x_, int y_, int z_) : x(x_), y(y_), z(z_) {}
	void print(const char* prefix) const { printf("%s={%d,%d,%d}\n", prefix, x, y, z); }
	int x,y,z;
};

int main(int argc, const char* argv[])
{
	const foo3  b_data = foo3(1,2,3);
	const foo3  a = foo3(0,0,7);      // a={0,0,7}
	const foo3* b = &b_data;          // b={1,2,3}
	ptr_container<const foo3&> ca(a); // i want this to store a copy of 'a', because 'a' is a reference
	ptr_container<const foo3*> cb(b); // i want this to store a pointer to 'b'
	printf("sizeof(ca) = %d\n", sizeof(ca));
	printf("sizeof(cb) = %d\n", sizeof(cb));
	ca.get_ptr()->print("a");
	cb.get_ptr()->print("b");
	system("pause");
	return 0;
}
*/

#define CDVGeomProxyT_csptr(type) grcDrawProxy_CDVGeomProxyT<CDVGeomParams##type,const CDVColourStyle*>
#define CDVGeomProxyT_csref(type) grcDrawProxy_CDVGeomProxyT<CDVGeomParams##type,const CDVColourStyle&>

template <typename ParamsType, typename CSType> class grcDrawProxy_CDVGeomProxyT : public grcDrawProxy
{
public:
	__forceinline grcDrawProxy_CDVGeomProxyT(CSType colourStyle, const Color32& colour, float opacity, const ParamsType& params, Mat34V_In basis, Vec3V_In camPos)
		: m_colourStyle(colourStyle)
		, m_colour     (colour     )
		, m_opacity    (opacity    )
		, m_params     (params     )
		, m_basis      (basis      )
		, m_camPos     (camPos     )
	{}

private:
	virtual int  Size() const { return sizeof(*this); }
	virtual void Draw() const
	{
		//grcWorldIdentity(); // wtf. can we get rid of this?
		m_params.Draw(m_colourStyle.get_ptr(), m_colour, m_opacity, m_basis, m_camPos, false);
	}

	ptr_container<CSType> m_colourStyle;
	const Color32         m_colour; // used if m_colourStyle is NULL
	const float           m_opacity;
	const ParamsType      m_params;
	const Mat34V          m_basis;
	const Vec3V           m_camPos;
};

void CDVGeomParamsBox::Draw(const CDVColourStyle* colourStyle, const Color32& colour, float opacity, Mat34V_In basis, Vec3V_In camPos, bool fw) const
{
	if (CDV_DRAW_PROXY && fw)
	{
		grcDebugDraw::Proxy(CDVGeomProxyT_csptr(Box)(colourStyle, colour, opacity, *this, basis, camPos));
		return;
	}

	const ScalarV x0 = m_boxMin.GetX();
	const ScalarV y0 = m_boxMin.GetY();
	const ScalarV z0 = m_boxMin.GetZ();
	const ScalarV x1 = m_boxMax.GetX();
	const ScalarV y1 = m_boxMax.GetY();
	const ScalarV z1 = m_boxMax.GetZ();

	const Vec3V points[8] =
	{
		Transform(basis, Vec3V(x0, y0, z1)), // NOTE -- i've swapped z0,z1 so that the box isn't inside-out ..
		Transform(basis, Vec3V(x1, y0, z1)),
		Transform(basis, Vec3V(x0, y1, z1)),
		Transform(basis, Vec3V(x1, y1, z1)),
		Transform(basis, Vec3V(x0, y0, z0)),
		Transform(basis, Vec3V(x1, y0, z0)),
		Transform(basis, Vec3V(x0, y1, z0)),
		Transform(basis, Vec3V(x1, y1, z0)),
	};

	if (colourStyle)
	{
		CDVGeom(colourStyle).BuildBoxInternal(points, m_stacksX, m_stacksY, m_stacksZ, ScalarV(V_ZERO)).Draw(camPos, opacity, fw);
	}
	else
	{		
		// TODO
	}
}

static __forceinline Vec3V_Out TransformProject(Mat44V_In m, Vec3V_In v)
{
	const Vec4V v2 = Multiply(m, Vec4V(v, ScalarV(V_ONE)));
	return v2.GetXYZ()/v2.GetW();
}

void CDVGeomParamsBoxProj::Draw(const CDVColourStyle* colourStyle, const Color32& colour, float opacity, Mat34V_In basis, Vec3V_In camPos, bool fw) const
{
	if (CDV_DRAW_PROXY && fw)
	{
		grcDebugDraw::Proxy(CDVGeomProxyT_csptr(BoxProj)(colourStyle, colour, opacity, *this, basis, camPos));
		return;
	}

	const ScalarV px = m_proj.GetX();
	const ScalarV py = m_proj.GetY();
	const ScalarV pz = m_proj.GetZ();

	const Vec3V q = Vec3V(V_ONE) - m_proj*m_proj;

	const ScalarV qx = q.GetX();
	const ScalarV qy = q.GetY();
	const ScalarV qz = q.GetZ();

	/*
	mx : matrix(
		[1 ,0 ,0 ,px],
		[0 ,qx,0 ,0 ],
		[0 ,0 ,qx,0 ],
		[px,0 ,0 ,1 ]
	);
	my : matrix(
		[qy,0 ,0 ,0 ],
		[0 ,1 ,0 ,py],
		[0 ,0 ,qy,0 ],
		[0 ,py,0 ,1 ]
	);
	mz : matrix(
		[qz,0 ,0 ,0 ],
		[0 ,qz,0 ,0 ],
		[0 ,0 ,1 ,pz],
		[0 ,0 ,pz,1 ]
	);

	mz.my.mx;

	(%o25) matrix([qy*qz,0,0,px*qy*qz],[px*py*qz,qx*qz,0,py*qz],[px*pz,py*pz*qx,qx*qy,pz],[px,py*qx,pz*qx*qy,1])
	*/

	const Mat44V projMat = Mat44V(
		Vec4V(qz*qy   , ScalarV(V_ZERO), ScalarV(V_ZERO), px*qy*qz      ),
		Vec4V(px*py*pz, qx*qz          , ScalarV(V_ZERO), py*qz         ),
		Vec4V(px*pz   , py*pz*qx       , qx*qy          , pz            ),
		Vec4V(px      , py*qx          , pz*qx*qy       , ScalarV(V_ONE))
	);

	const Vec3V centre = (m_boxMax + m_boxMin)*ScalarV(V_HALF);
	const Vec3V extent = (m_boxMax - m_boxMin)*ScalarV(V_HALF);

	const Vec3V points[8] =
	{
		Transform(basis, centre + extent*TransformProject(projMat, Vec3V(-1.0f, -1.0f, +1.0f))),
		Transform(basis, centre + extent*TransformProject(projMat, Vec3V(+1.0f, -1.0f, +1.0f))),
		Transform(basis, centre + extent*TransformProject(projMat, Vec3V(-1.0f, +1.0f, +1.0f))),
		Transform(basis, centre + extent*TransformProject(projMat, Vec3V(+1.0f, +1.0f, +1.0f))),
		Transform(basis, centre + extent*TransformProject(projMat, Vec3V(-1.0f, -1.0f, -1.0f))),
		Transform(basis, centre + extent*TransformProject(projMat, Vec3V(+1.0f, -1.0f, -1.0f))),
		Transform(basis, centre + extent*TransformProject(projMat, Vec3V(-1.0f, +1.0f, -1.0f))),
		Transform(basis, centre + extent*TransformProject(projMat, Vec3V(+1.0f, +1.0f, -1.0f))),
	};

	if (colourStyle)
	{
		CDVGeom(colourStyle).BuildBoxInternal(points, m_stacksX, m_stacksY, m_stacksZ, m_perspective).Draw(camPos, opacity, fw);
	}
	else
	{
		// TODO
	}
}

void CDVGeomParamsFrustum::Draw(const CDVColourStyle* colourStyle, const Color32& colour, float opacity, Mat34V_In basis, Vec3V_In camPos, bool fw) const
{
	if (CDV_DRAW_PROXY && fw)
	{
		grcDebugDraw::Proxy(CDVGeomProxyT_csptr(Frustum)(colourStyle, colour, opacity, *this, basis, camPos));
		return;
	}

	const Vec3V points[8] =
	{
		Transform(basis, Vec3V(-m_tanHFOV, -m_tanVFOV, ScalarV(V_NEGONE))*m_z0),
		Transform(basis, Vec3V(+m_tanHFOV, -m_tanVFOV, ScalarV(V_NEGONE))*m_z0),
		Transform(basis, Vec3V(-m_tanHFOV, +m_tanVFOV, ScalarV(V_NEGONE))*m_z0),
		Transform(basis, Vec3V(+m_tanHFOV, +m_tanVFOV, ScalarV(V_NEGONE))*m_z0),
		Transform(basis, Vec3V(-m_tanHFOV, -m_tanVFOV, ScalarV(V_NEGONE))*m_z1),
		Transform(basis, Vec3V(+m_tanHFOV, -m_tanVFOV, ScalarV(V_NEGONE))*m_z1),
		Transform(basis, Vec3V(-m_tanHFOV, +m_tanVFOV, ScalarV(V_NEGONE))*m_z1),
		Transform(basis, Vec3V(+m_tanHFOV, +m_tanVFOV, ScalarV(V_NEGONE))*m_z1),
	};

	if (colourStyle)
	{
		CDVGeom(colourStyle).BuildBoxInternal(points, m_stacksX, m_stacksY, m_stacksZ, m_perspective).Draw(camPos, opacity, fw);
	}
	else
	{
		// TODO
	}
}

void CDVGeomParamsSphere::Draw(const CDVColourStyle* colourStyle, const Color32& colour, float opacity, Mat34V_In basis, Vec3V_In camPos, bool fw) const
{
	if (CDV_DRAW_PROXY && fw)
	{
		grcDebugDraw::Proxy(CDVGeomProxyT_csptr(Sphere)(colourStyle, colour, opacity, *this, basis, camPos));
		return;
	}

	const int slicesTess = colourStyle ? 1 : m_slicesTess;
	const int stacksTess = colourStyle ? 1 : m_stacksTess;

	const int slices = m_slices*slicesTess;
	const int stacks = m_stacks*stacksTess;

	Assert(slices >= 3 && slices <= CDVGeom::MAX_SLICES);
	Assert(stacks >= 1 && stacks <= CDVGeom::MAX_STACKS);

	Vec3V xy1[CDVGeom::MAX_SLICES];
	Vec3V rrz[CDVGeom::MAX_STACKS + 1];

	const Vec3V r_negz = Vec3V(Vec2V(m_radius), -m_radius); // {+r,+r,-r}

	// this gives {sin,cos} over range [0..pi]
	CalcSinCosTable(rrz, ScalarV(PI/(float)stacks), stacks + 1);

	for (int j = 0; j <= stacks; j++)
	{
		// permute v <- v.xxy, now we have {sin,sin,cos}*r_negz over range [0..pi]
		// rrz[0] = {0,0,-1}
		// rrz[M] = {0,0,+1}
		rrz[j] = PermuteXXY(rrz[j])*r_negz;
	}

	CalcSinCosTable(xy1, ScalarV(2.0f*PI/(float)slices), slices);

	if (colourStyle)
	{
		CDVGeom(colourStyle).LatheInternal(basis, xy1, rrz, slices, stacks, false).Draw(camPos, opacity, fw);
	}
	else
	{
		CDVGeom::LatheInternal_DrawImmediate(basis, xy1, rrz, slices, stacks, slicesTess, stacksTess, colour, opacity, false);
	}
}

void CDVGeomParamsHemisphere::Draw(const CDVColourStyle* colourStyle, const Color32& colour, float opacity, Mat34V_In basis, Vec3V_In camPos, bool fw) const
{
	if (CDV_DRAW_PROXY && fw)
	{
		grcDebugDraw::Proxy(CDVGeomProxyT_csptr(Hemisphere)(colourStyle, colour, opacity, *this, basis, camPos));
		return;
	}

	const int slicesTess = colourStyle ? 1 : m_slicesTess;
	const int stacksTess = colourStyle ? 1 : m_stacksTess;

	const int slices = m_slices*slicesTess;
	const int stacks = m_stacks*stacksTess;

	Assert(slices >= 3 && slices <= CDVGeom::MAX_SLICES);
	Assert(stacks >= 1 && stacks <= CDVGeom::MAX_STACKS);

	Vec3V xy1[CDVGeom::MAX_SLICES];
	Vec3V rrz[CDVGeom::MAX_STACKS + 1];

	// this gives {sin,cos} over range [0..pi/2]
	CalcSinCosTable(rrz, ScalarV(0.5f*PI/(float)stacks), stacks + 1);

	for (int j = 0; j <= stacks; j++)
	{
		// permute v <- v.yyx, now we have {cos,cos,sin}*r over the range [0..pi/2]
		// rrz[0] = {1,1,0}
		// rrz[M] = {0,0,1}
		rrz[j] = PermuteYYX(rrz[j])*m_radius;
	}

	CalcSinCosTable(xy1, ScalarV(2.0f*PI/(float)slices), slices);

	if (colourStyle)
	{
		CDVGeom(colourStyle).LatheInternal(basis, xy1, rrz, slices, stacks, m_faceGrid).Draw(camPos, opacity, fw);
	}
	else
	{
		CDVGeom::LatheInternal_DrawImmediate(basis, xy1, rrz, slices, stacks, slicesTess, stacksTess, colour, opacity, false);
	}
}

void CDVGeomParamsCylinder::Draw(const CDVColourStyle* colourStyle, const Color32& colour, float opacity, Mat34V_In basis, Vec3V_In camPos, bool fw) const
{
	if (CDV_DRAW_PROXY && fw)
	{
		grcDebugDraw::Proxy(CDVGeomProxyT_csptr(Cylinder)(colourStyle, colour, opacity, *this, basis, camPos));
		return;
	}

	const int slicesTess = colourStyle ? 1 : m_slicesTess;
	const int stacksTess = colourStyle ? 1 : m_stacksTess;

	const int slices = m_slices*slicesTess;
	const int stacks = m_stacks*stacksTess;

	Assert(slices >= 3 && slices <= CDVGeom::MAX_SLICES);
	Assert(stacks >= 1 && stacks <= CDVGeom::MAX_STACKS);

	Vec3V xy1[CDVGeom::MAX_SLICES];
	Vec3V rrz[CDVGeom::MAX_STACKS + 1];

	const Vec3V rrz0 = Vec3V(Vec2V(m_radius0), m_z0);
	const Vec3V rrz1 = Vec3V(Vec2V(m_radius1), m_z1);

	CalcLinearTable(rrz, rrz0, rrz1, ScalarV(1.0f/(float)stacks), stacks + 1);
	CalcSinCosTable(xy1, ScalarV(2.0f*PI/(float)slices), slices);

	if (colourStyle)
	{
		CDVGeom(colourStyle).LatheInternal(basis, xy1, rrz, slices, stacks, m_faceGrid).Draw(camPos, opacity, fw);
	}
	else
	{
		CDVGeom::LatheInternal_DrawImmediate(basis, xy1, rrz, slices, stacks, slicesTess, stacksTess, colour, opacity, false);
	}
}

#define NEW_CONE_PARAMS (1)

#if !NEW_CONE_PARAMS
static __forceinline ScalarV_Out CalcSphericalConeZ1(
	ScalarV_In length,      // length + endcap height
	ScalarV_In tanTheta,    // tan(angle/2)
	ScalarV_In capCurvature // 0 for flat cone, 1 for spherical cone
	)
{
	const float t1 = tanTheta.Getf();
	const float a1 = atanf(t1);
	const float a2 = a1*capCurvature.Getf();

	if (a2 == 0.0f)
	{
		return length;
	}

	const float q2 = 1.0f/tanf(a2);
	const float z1 = 1.0f/(t1*(sqrtf(1.0f + q2*q2) - q2) + 1.0f);

	return length*ScalarV(z1);
}
#endif // !NEW_CONE_PARAMS

void CDVGeomParamsCone::Draw(const CDVColourStyle* colourStyle, const Color32& colour, float opacity, Mat34V_In basis, Vec3V_In camPos, bool fw) const
{
	if (CDV_DRAW_PROXY && fw)
	{
		grcDebugDraw::Proxy(CDVGeomProxyT_csptr(Cone)(colourStyle, colour, opacity, *this, basis, camPos));
		return;
	}

	const int slicesTess = colourStyle ? 1 : m_slicesTess;
	const int stacksTess = colourStyle ? 1 : m_stacksTess;

	const int slices = m_slices*slicesTess;
	const int stacks = m_stacks*stacksTess;

	const bool bFlat = IsTrue(m_capCurvature == ScalarV(V_ZERO));

	const int capStacks = bFlat ? 0 : m_capStacks*stacksTess;

	Assert(slices >= 3 && slices             <= CDVGeom::MAX_SLICES);
	Assert(stacks >= 1 && stacks + capStacks <= CDVGeom::MAX_STACKS);

	Vec3V xy1[CDVGeom::MAX_SLICES];
	Vec3V rrz[CDVGeom::MAX_STACKS + 1];

#if NEW_CONE_PARAMS
	// z1 = r*(sqrt(1 + (c*t)^2) + (1 - c)) / (2 - c*(1 - t^2))
	const ScalarV ct = m_capCurvature*m_tanTheta;
	const ScalarV l0 = Sqrt(AddScaled(ScalarV(V_ONE), ct, ct)) + (ScalarV(V_ONE) - m_capCurvature);
	const ScalarV l1 = SubtractScaled(ScalarV(V_TWO), m_capCurvature, SubtractScaled(ScalarV(V_ONE), m_tanTheta, m_tanTheta));
	const ScalarV z1 = m_radius*(l0/l1);
#else
	const ScalarV z1 = CalcSphericalConeZ1(m_radius, m_tanTheta, m_capCurvature);
#endif

	// construct stack vectors (rrz)
	{
		const Vec3V v = Vec3V(Vec2V(m_tanTheta), ScalarV(V_ONE));

#if !__WIN32PC // for fucksake ..
		if (IsTrue(m_perspective == ScalarV(V_ZERO)))
#endif // !__WIN32PC
		{
			CalcLinearTable(rrz, Vec3V(V_ZERO), v*z1, ScalarV(1.0f/(float)stacks), stacks + 1);
		}
#if !__WIN32PC // for fucksake ..
		else
		{
			// ack, floats!
			const float z1f = z1.Getf();
			const float zpf = m_perspective.Getf();

			for (int j = 0; j <= stacks; j++)
			{
#if __WIN32
#pragma warning(push)
#pragma warning(disable:4723) // warning C4723: potential divide by 0
#endif
				const ScalarV z = ScalarV(PerspectiveInterp(0.0f, z1f, (float)j/(float)stacks, zpf));
#if __WIN32
#pragma warning(pop)
#endif
				rrz[j] = v*z;
			}
		}
#endif // !__WIN32PC

		if (capStacks > 0)
		{
#if NEW_CONE_PARAMS
			const float sphericalOffset = (z1*(ScalarV(V_ONE)/m_capCurvature - ScalarV(V_ONE))).Getf(); // offset from apex to centre of spherical cone
			const float sphericalRadius = sphericalOffset + m_radius.Getf(); // radius of spherical cone
			const float sphericalAngle  = atanf((m_capCurvature*m_tanTheta).Getf());
#else
			const float t1 = m_tanTheta.Getf();
			const float a1 = atanf(t1);
			const float a2 = a1*m_capCurvature.Getf();
			const float q2 = 1.0f/tanf(a2);

			const float sphericalRadius = z1.Getf()*t1*sqrtf(1.0f + q2*q2); // radius of spherical cone
			const float sphericalOffset = z1.Getf()*(t1*q2 - 1.0f); // offset from apex to centre of spherical cone
			const float sphericalAngle  = a2;
#endif

			for (int j = stacks + 1; j <= stacks + capStacks; j++)
			{
				const float t = (float)(j - stacks)/(float)capStacks; // (0..1]
				const float a = sphericalAngle*(1.0f - t);
				const float z = sphericalRadius*cosf(a);
				const float r = sphericalRadius*sinf(a);

				rrz[j] = Vec3V(r, r, z - sphericalOffset);
			}
		}
	}

	CalcSinCosTable(xy1, ScalarV(2.0f*PI/(float)slices), slices);

	if (colourStyle)
	{
		CDVGeom(colourStyle).LatheInternal(basis, xy1, rrz, slices, stacks + capStacks, m_faceGrid && bFlat).Draw(camPos, opacity, fw);
	}
	else
	{
		CDVGeom::LatheInternal_DrawImmediate(basis, xy1, rrz, slices, stacks + capStacks, slicesTess, stacksTess, colour, opacity, false);
	}
}

void CDVGeomParamsCapsule::Draw(const CDVColourStyle* colourStyle, const Color32& colour, float opacity, Mat34V_In basis, Vec3V_In camPos, bool fw) const
{
	if (CDV_DRAW_PROXY && fw)
	{
		grcDebugDraw::Proxy(CDVGeomProxyT_csptr(Capsule)(colourStyle, colour, opacity, *this, basis, camPos));
		return;
	}

	const int slicesTess = colourStyle ? 1 : m_slicesTess;
	const int stacksTess = colourStyle ? 1 : m_stacksTess;

	const int slices = m_slices*slicesTess;
	const int stacks = m_stacks*stacksTess;

	const int capStacks = m_capStacks*stacksTess;

	Assert(slices >= 3 && slices               <= CDVGeom::MAX_SLICES);
	Assert(stacks >= 1 && stacks + capStacks*2 <= CDVGeom::MAX_STACKS);

	Vec3V xy1[CDVGeom::MAX_SLICES];
	Vec3V rrz[CDVGeom::MAX_STACKS + 1];

	// total stacks = capStacks + stacks + capStacks
	// rrz[0                .. capStacks                 ] are cap0
	// rrz[capStacks        .. capStacks+stacks          ] are body
	// rrz[capStacks+stacks .. capStacks+stacks+capStacks] are cap1

	// construct stack vectors (rrz)
	{
		const Vec3V rrz0 = Vec3V(Vec2V(m_radius), m_z0);
		const Vec3V rrz1 = Vec3V(Vec2V(m_radius), m_z1);

		CalcLinearTable(rrz + m_capStacks, rrz0, rrz1, ScalarV(1.0f/(float)stacks), stacks + 1);

		if (capStacks > 0)
		{
			const Vec3V v0 = Vec3V(Vec2V(V_ZERO), m_z0);
			const Vec3V v1 = Vec3V(Vec2V(V_ZERO), m_z1);

			const Vec3V r0 = Vec3V(Vec2V(m_radius), -m_radius); // {+r,+r,-r}
			const Vec3V r1 = Vec3V(m_radius);

			CalcSinCosTable(rrz, ScalarV(0.5f*PI/(float)capStacks), capStacks);

			for (int i = 0; i < capStacks; i++)
			{
				const int j = stacks + capStacks*2 - i;
				const Vec3V v = PermuteXXY(rrz[i]);

				rrz[i] = AddScaled(v0, r0, v);
				rrz[j] = AddScaled(v1, r1, v);
			}
		}
	}

	CalcSinCosTable(xy1, ScalarV(2.0f*PI/(float)slices), slices);

	if (colourStyle)
	{
		CDVGeom(colourStyle).LatheInternal(basis, xy1, rrz, slices, stacks + capStacks*2, false).Draw(camPos, opacity, fw);
	}
	else
	{
		CDVGeom::LatheInternal_DrawImmediate(basis, xy1, rrz, slices, stacks + capStacks*2, slicesTess, stacksTess, colour, opacity, false);
	}
}

CDVGeom& CDVGeom::BuildKDOP18(const CkDOP18& kDOP18, float tolerance)
{
	Clear();

	CkDOP18Face kDOP18Faces[18];
	const int faceCount = kDOP18.BuildFaces(kDOP18Faces, NULL, tolerance);

	for (int faceIndex = 0; faceIndex < faceCount; faceIndex++)
	{
		atArray<Vec3V> points;

		for (int k = 0; k < kDOP18Faces[faceIndex].m_pointCount; k++)
		{
			points.PushAndGrow(RCC_VEC3V(kDOP18Faces[faceIndex].m_points[k]));
		}

		AddConnectedFace(points);
	}

	CalcNormals();

	return *this;
}

void CDVGeom::AddVerticesToKDOP18(CkDOP18& kDOP18) const
{
	for (int i = 0; i < m_verts.size(); i++)
	{
		kDOP18.AddPoint(RCC_VECTOR3(m_verts[i].m_position));
	}
}

static float GetSurfNdotV(const CDVSurfIndex& si, const float* NdotV_quads, const float* NdotV_faces)
{
	if (si.m_quadOrFaceIndex != INDEX_NONE)
	{
		return si.m_isFaceIndex ? NdotV_faces[si.m_quadOrFaceIndex] : NdotV_quads[si.m_quadOrFaceIndex];
	}

	return 0.0f;
}

void CDVGeom::Draw(Vec3V_In camPos, float opacity, bool fw) const
{
	USE_DEBUG_MEMORY();

	if (opacity > 0.0f)
	{
		const CDVColourStyle colourStyle(*m_colourStyle, opacity); // apply opacity

		float* NdotV_quads = rage_new float[m_quads.size()];
		float* NdotV_faces = rage_new float[m_faces.size()];

		for (int i = 0; i < m_quads.size(); i++)
		{
			NdotV_quads[i] = Dot(m_quads[i].m_normal, Normalize(m_quads[i].m_centroid - camPos)).Getf();
		}

		for (int i = 0; i < m_faces.size(); i++)
		{
			NdotV_faces[i] = Dot(m_faces[i].m_normal, Normalize(m_faces[i].m_centroid - camPos)).Getf();
		}

		for (int pass = 0; pass < DVP_COUNT; pass++)
		{
			DrawPassInternal(pass, colourStyle, NdotV_quads, NdotV_faces, fw);
		}

		delete[] NdotV_quads;
		delete[] NdotV_faces;
	}
}

// could probably clean this up some more ..
void CDVGeom::DrawPassInternal(int pass, const CDVColourStyle& colourStyle, const float* NdotV_quads, const float* NdotV_faces, bool fw) const
{
	const Color32 colour = colourStyle.m_colours[pass];

	if (colour.GetAlpha() == 0)
	{
		return;
	}

	const bool bFrontPass = (pass == DVP_FRONT_SURFS || pass == DVP_FRONT_LINES || pass == DVP_FRONT_EDGES);

	atArray<Vec3V> lineVertArray;
	atArray<Vec3V> quadVertArray;
	atArray<Vec3V> faceVertArray;

	if (pass == DVP_BACK_SURFS || pass == DVP_FRONT_SURFS)
	{
		for (int i = 0; i < m_quads.size(); i++)
		{
			if ((NdotV_quads[i] > 0.0f) == bFrontPass)
			{
				m_quads[i].Draw(this, colour, fw, quadVertArray);
			}
		}

		for (int i = 0; i < m_faces.size(); i++)
		{
			if ((NdotV_faces[i] > 0.0f) == bFrontPass)
			{
				const CDVFace& face = m_faces[i];

				face.Draw(this, colour, fw, faceVertArray);
			}
		}
	}
	else if (pass == DVP_BACK_LINES || pass == DVP_FRONT_LINES)
	{
		for (int i = 0; i < m_lines.size(); i++)
		{
			const CDVLine& line = m_lines[i];
			bool bDraw = false;

			if (line.m_surfIndex.m_quadOrFaceIndex != INDEX_NONE)
			{
				const bool bFacing = (GetSurfNdotV(line.m_surfIndex, NdotV_quads, NdotV_faces) > 0.0f);

				if (bFacing == bFrontPass)
				{
					bDraw = true;
				}
			}
			else // free floating line
			{
				bDraw = bFrontPass;
			}

			if (bDraw)
			{
				line.Draw(this, colour, fw, lineVertArray);
			}
		}
	}
	else // edges
	{
		for (int i = 0; i < m_edges.size(); i++)
		{
			const CDVEdge& edge = m_edges[i];
			bool bDraw = false;

			if (edge.m_surfIndices[0].m_quadOrFaceIndex == INDEX_NONE ||
				edge.m_surfIndices[1].m_quadOrFaceIndex == INDEX_NONE)
			{
				if (pass == DVP_SILHOUETTE_EDGES)
				{
					bDraw = true;
				}
			}
			else
			{
				const bool bFacing0 = (GetSurfNdotV(edge.m_surfIndices[0], NdotV_quads, NdotV_faces) > 0.0f);
				const bool bFacing1 = (GetSurfNdotV(edge.m_surfIndices[1], NdotV_quads, NdotV_faces) > 0.0f);

				if (pass == DVP_SILHOUETTE_EDGES)
				{
					bDraw = (bFacing0 != bFacing1);
				}
				else
				{
					bDraw = (bFacing0 == bFrontPass && bFacing1 == bFrontPass);
				}
			}

			if (bDraw)
			{
				edge.Draw(this, colour, fw, lineVertArray);
			}
		}
	}

	grcColor(colour);

	grcVertexArray(drawTriStrip, faceVertArray);
	grcVertexArray(drawQuads   , quadVertArray);
	grcVertexArray(drawLines   , lineVertArray);
}

void CDVGeom::Clear()
{
	m_verts.clear();
	m_lines.clear();
	m_edges.clear();
	m_quads.clear();
	m_faces.clear();
}

CDVGeom& CDVGeom::BuildBoxInternal(
	const Vec3V points[8],
	int         stacksX,
	int         stacksY,
	int         stacksZ,
	ScalarV_In  perspective
	)
{	
	Clear();

	m_verts.Resize(8);
	m_edges.Resize(12);
	m_quads.Resize(6);

	m_verts[0] = CDVVert(points[0]);
	m_verts[1] = CDVVert(points[1]);
	m_verts[2] = CDVVert(points[2]);
	m_verts[3] = CDVVert(points[3]);
	m_verts[4] = CDVVert(points[4]);
	m_verts[5] = CDVVert(points[5]);
	m_verts[6] = CDVVert(points[6]);
	m_verts[7] = CDVVert(points[7]);

	// 2---------------3
	// |\             /|
	// | \     1     / |
	// |  \         /  |
	// |   6-------7   |
	// |   |       |   |
	// | 2 |   4   | 3 |
	// |   |       |   |
	// |   4-------5   |
	// |  /         \  |
	// | /     0     \ |
	// |/             \|
	// 0---------------1

	m_edges[ 0] = CDVEdge(0, 1, 0, 5);
	m_edges[ 1] = CDVEdge(1, 3, 3, 5);
	m_edges[ 2] = CDVEdge(3, 2, 1, 5);
	m_edges[ 3] = CDVEdge(2, 0, 2, 5);
	m_edges[ 4] = CDVEdge(4, 5, 0, 4);
	m_edges[ 5] = CDVEdge(5, 7, 3, 4);
	m_edges[ 6] = CDVEdge(7, 6, 1, 4);
	m_edges[ 7] = CDVEdge(6, 4, 2, 4);
	m_edges[ 8] = CDVEdge(0, 4, 0, 2);
	m_edges[ 9] = CDVEdge(1, 5, 0, 3);
	m_edges[10] = CDVEdge(3, 7, 3, 1);
	m_edges[11] = CDVEdge(2, 6, 1, 2);

	m_quads[0] = CDVQuad(0, 1, 5, 4);
	m_quads[1] = CDVQuad(3, 2, 6, 7);
	m_quads[2] = CDVQuad(2, 0, 4, 6);
	m_quads[3] = CDVQuad(1, 3, 7, 5);
	m_quads[4] = CDVQuad(4, 5, 7, 6);
	m_quads[5] = CDVQuad(2, 3, 1, 0);

	const float persp = perspective.Getf();

	AddGridLinesToQuad(0, stacksX, stacksZ, persp);
	AddGridLinesToQuad(1, stacksX, stacksZ, persp);
	AddGridLinesToQuad(2, stacksY, stacksZ, persp);
	AddGridLinesToQuad(3, stacksY, stacksZ, persp);
	AddGridLinesToQuad(4, stacksX, stacksY, persp);
	AddGridLinesToQuad(5, stacksX, stacksY, persp);

	CalcNormals();

	return *this;
}

CDVGeom& CDVGeom::LatheInternal(
	Mat34V_In    basis,
	const Vec3V* xy1, // [slices]
	const Vec3V* rrz, // [stacks + 1]
	int          slices,
	int          stacks,
	bool         faceGrid
	)
{
	const int N = slices;
	const int M = stacks;

	Clear();

	/*
	rotationally-symmetric geometry with N slices, M stacks ..
	verts   arranged in a N x (M+1) grid
	quads   arranged in a N x M grid
	h-edges arranged in a N x (M+1) grid
	v-edges arranged in a N x M grid

	e.g.:
	N=6 slices
	M=4 stacks

	quad[i,j] index is i + j*N
	vert indices are ..
		vi00 = (i + 0)   + (j + 0)*N
		vi10 = (i + 1)%N + (j + 0)*N
		vi01 = (i + 0)   + (J + 1)*N
		vi11 = (i + 1)%N + (j + 1)*N

	v-edge[i,j] index is i + j*N
	vert indices are ..
		vi00 = (i + 0) + (j + 0)*N
		vi01 = (i + 0) + (J + 1)*N
	adjacent quad indices are ..
		vi00 = (i        )   + (j + 0)*N
		vin0 = (i - 1 + N)%N + (j + 0)*N

	h-edge[i,j] index is i + j*N
	vert indices are ..
		vi00 = (i + 0)   + (j + 0)*N
		vi10 = (i + 1)%N + (j + 0)*N
	adjacent quad indices are ..
		vi00 = (i + 0) + (j + 0)*N ... unless j == M in which case it's adjacent to the positive endcap face
		vi0n = (i + 0) + (j - 1)*N ... unless j == 0 in which case it's adjacent to the negative endcap face

		24--------25--------26--------27--------28--------29-------[24]
		 |         |         |         |         |         |
		 |         |         |         |         |         |         |
		 |   18    |   19    |   20    |   21    |   22    |   23
		 |         |         |         |         |         |         |
		 |         |         |         |         |         |
		18--------19--------20--------21--------22--------23-------[18]
		 |         |         |         |         |         |
		 |         |         |         |         |         |         |
		 |   12    |   13    |   14    |   15    |   16    |   17
		 |         |         |         |         |         |         |
		 |         |         |         |         |         |
		12--------13--------14--------15--------16--------17-------[12]
		 |         |         |         |         |         |
		 |         |         |         |         |         |         |
		 |    6    |    7    |    8    |    9    |   10    |   11
		 |         |         |         |         |         |         |
		 |         |         |         |         |         |
		 6---------7---------8---------9--------10--------11--------[6]
		 |         |         |         |         |         |
		 |         |         |         |         |         |         |
		 |    0    |    1    |    2    |    3    |    4    |    5
		 |         |         |         |         |         |         |
		 |         |         |         |         |         |
		 0---------1---------2---------3---------4---------5--------[0]
	*/

	CDVSurfIndex endcap0(INDEX_NONE, true);
	CDVSurfIndex endcap1(INDEX_NONE, true);

	const float kMinEndcapRadius = 0.01f;

	if (rrz[0].GetXf() > kMinEndcapRadius)
	{
		endcap0.m_quadOrFaceIndex = m_faces.size();

		CDVFace& face = m_faces.Grow();

		for (int i = 0; i < N; i++)
		{
			face.m_vertIndices.PushAndGrow(N - i - 1);
		}
	}

	if (rrz[M].GetXf() > kMinEndcapRadius)
	{
		endcap1.m_quadOrFaceIndex = m_faces.size();

		CDVFace& face = m_faces.Grow();

		for (int i = 0; i < N; i++)
		{
			face.m_vertIndices.PushAndGrow(M*N + i);
		}
	}

	m_verts.Resize(N*(1*M + 1));
	m_edges.Resize(N*(2*M + 1)); // N*(M+1) h-edges, N*M v-edges
	m_quads.Resize(N*(1*M + 0));

	for (int j = 0; j <= M; j++)
	{
		for (int i = 0; i < N; i++)
		{
			// these could be constructed without * and %, but this is for clarity
			const int vi00 = (i + 0    )   + j*N; // di= 0, dj= 0
			const int vi10 = (i + 1    )%N + j*N; // di=+1, dj= 0
			const int vin0 = (i - 1 + N)%N + j*N; // di=-1, dj= 0
			const int vi01 = vi00 + N;            // di= 0, dj=+1
			const int vi11 = vi10 + N;            // di=+1, dj=+1

			const CDVSurfIndex si0 = (j < M) ? CDVSurfIndex(vi00    , false) : endcap1;
			const CDVSurfIndex si1 = (j > 0) ? CDVSurfIndex(vi00 - N, false) : endcap0;

			m_verts[vi00] = CDVVert(Transform(basis, xy1[i]*rrz[j]));
			m_edges[vi00] = CDVEdge(vi00, vi10, si0, si1); // h-edge

			if (j < M)
			{
				const CDVSurfIndex si2 = CDVSurfIndex(vi00, false);
				const CDVSurfIndex si3 = CDVSurfIndex(vin0, false);

				m_quads[vi00] = CDVQuad(vi00, vi10, vi11, vi01);
				m_edges[vi00 + N*(M + 1)] = CDVEdge(vi00, vi01, si2, si3); // v-edge
			}
		}
	}

	if (faceGrid)
	{
		for (int i = 0; i < m_faces.size(); i++)
		{
			AddGridLinesToFace(i);
		}
	}

	CalcNormals();

	return *this;
}

// i'm sure i've written this function before ..
static __forceinline Color32 SetColourOpacity(const Color32& colour, float opacity)
{
	Color32 temp = colour;
	temp.SetAlpha((int)(opacity*(float)colour.GetAlpha()));
	return temp;
}

__COMMENT(static) void CDVGeom::LatheInternal_DrawImmediate(
	Mat34V_In      basis,
	const Vec3V*   xy1, // [slices]
	const Vec3V*   rrz, // [stacks + 1]
	int            slices,
	int            stacks,
	int            slicesStep,
	int            stacksStep,
	const Color32& colour,
	float          opacity,
	bool           bSolid
	)
{
	if (IsEqualAll(basis.a(), Vec3V(V_X_AXIS_WZERO)) &
		IsEqualAll(basis.b(), Vec3V(V_Y_AXIS_WZERO)) &
		IsEqualAll(basis.c(), Vec3V(V_Z_AXIS_WZERO)))
	{
		LatheInternal_DrawImmediate(basis.d(), xy1, rrz, slices, stacks, slicesStep, stacksStep, colour, opacity, bSolid);
		return;
	}

	const int N = slices;
	const int M = stacks;

	grcColor(SetColourOpacity(colour, opacity));

	if (bSolid)
	{
		Vec3V ring[MAX_SLICES];

		// init ring
		{
			const Vec3V rrz_0 = rrz[0];

			for (int i = 0; i < N; i++)
			{
				ring[i] = Transform(basis, xy1[i]*rrz_0);
			}
		}

		for (int j = 1; j <= M; j++)
		{
			const Vec3V rrz_j = rrz[j];

			grcBegin(drawTriStrip, (N + 1)*2);

			for (int i = 0; i < N; i++)
			{
				const Vec3V p = Transform(basis, xy1[i]*rrz_j);

				grcVertex3f(ring[i]);
				grcVertex3f(p);

				ring[i] = p;
			}

			grcVertex3f(Transform(basis, xy1[0]*rrz[j - 1])); // close loop
			grcVertex3f(Transform(basis, xy1[0]*rrz_j));

			grcEnd();
		}
	}
	else // wireframe
	{
		for (int i = 0; i < N; i += slicesStep) // draw slices
		{
			const Vec3V xy1_i = xy1[i];

			grcBegin(drawLineStrip, M + 1);

			for (int j = 0; j <= M; j++)
			{
				grcVertex3f(Transform(basis, xy1_i*rrz[j]));
			}

			grcEnd();
		}

		for (int j = 0; j <= M; j += stacksStep) // draw stacks
		{
			const Vec3V rrz_j = rrz[j];

			if (IsTrue(rrz_j.GetX() != ScalarV(V_ZERO)))
			{
				grcBegin(drawLineStrip, N + 1);

				for (int i = 0; i < N; i++)
				{
					grcVertex3f(Transform(basis, xy1[i]*rrz_j));
				}

				grcVertex3f(Transform(basis, xy1[0]*rrz_j)); // close loop

				grcEnd();
			}
		}
	}
}

__COMMENT(static) void CDVGeom::LatheInternal_DrawImmediate(
	Vec3V_In       origin,
	const Vec3V*   xy1, // [slices]
	const Vec3V*   rrz, // [stacks + 1]
	int            slices,
	int            stacks,
	int            slicesStep,
	int            stacksStep,
	const Color32& colour,
	float          opacity,
	bool           bSolid
	)
{
	const int N = slices;
	const int M = stacks;

	grcColor(SetColourOpacity(colour, opacity));

	if (bSolid)
	{
		Vec3V ring[MAX_SLICES];

		// init ring
		{
			const Vec3V rrz_0 = rrz[0];

			for (int i = 0; i < N; i++)
			{
				ring[i] = AddScaled(origin, xy1[i], rrz_0);
			}
		}

		for (int j = 1; j <= M; j++)
		{
			const Vec3V rrz_j = rrz[j];

			grcBegin(drawTriStrip, (N + 1)*2);

			for (int i = 0; i < N; i++)
			{
				const Vec3V p = AddScaled(origin, xy1[i], rrz_j);

				grcVertex3f(ring[i]);
				grcVertex3f(p);

				ring[i] = p;
			}

			grcVertex3f(AddScaled(origin, xy1[0], rrz[j - 1])); // close loop
			grcVertex3f(AddScaled(origin, xy1[0], rrz_j));

			grcEnd();
		}
	}
	else // wireframe
	{
		for (int i = 0; i < N; i += slicesStep) // draw slices
		{
			const Vec3V xy1_i = xy1[i];

			grcBegin(drawLineStrip, M + 1);

			for (int j = 0; j <= M; j++)
			{
				grcVertex3f(AddScaled(origin, xy1_i, rrz[j]));
			}

			grcEnd();
		}

		for (int j = 0; j <= M; j += stacksStep) // draw stacks
		{
			const Vec3V rrz_j = rrz[j];

			if (IsTrue(rrz_j.GetX() != ScalarV(V_ZERO)))
			{
				grcBegin(drawLineStrip, N + 1);

				for (int i = 0; i < N; i++)
				{
					grcVertex3f(AddScaled(origin, xy1[i], rrz_j));
				}

				grcVertex3f(AddScaled(origin, xy1[0], rrz_j)); // close loop

				grcEnd();
			}
		}
	}
}

__COMMENT(static) void CDVGeom::LatheInternal_DrawImmediate(
	Mat34V_In      basis,
	const Vec3V*   xy1, // [slices]
	Vec3V_In       rrz0,
	Vec3V_In       rrz1,
	const ScalarV* rrzt, // rrz[j] is constructed as rrz0 + (rrz1 - rrz0)*rrzt[j]
	int            slices,
	int            stacks,
	int            slicesStep,
	int            stacksStep,
	const Color32& colour,
	float          opacity,
	bool           bSolid
	)
{
	const int N = slices;
	const int M = stacks;

	grcColor(SetColourOpacity(colour, opacity));

	const Vec3V rrz_dt = rrz1 - rrz0;
	const Vec3V rrz_j0 = AddScaled(rrz0, rrz_dt, rrzt[0]); // in case rrzt[0] != 0
	const Vec3V rrz_jM = AddScaled(rrz0, rrz_dt, rrzt[M]); // in case rrzt[M] != 1

	if (bSolid)
	{
		grcBegin(drawTriStrip, (N + 1)*2);

		for (int i = 0; i < N; i++)
		{
			const Vec3V xy1_i = xy1[i];

			grcVertex3f(Transform(basis, xy1_i*rrz_j0));
			grcVertex3f(Transform(basis, xy1_i*rrz_jM));
		}

		grcVertex3f(Transform(basis, xy1[0]*rrz_j0)); // close loop
		grcVertex3f(Transform(basis, xy1[0]*rrz_jM));

		grcEnd();
	}
	else // wireframe
	{
		// draw slices (much more efficient because rrz is linear)
		{
			grcBegin(drawLines, N*2);

			for (int i = 0; i < N; i += slicesStep)
			{
				const Vec3V xy1_i = xy1[i];

				grcVertex3f(Transform(basis, xy1_i*rrz_j0));
				grcVertex3f(Transform(basis, xy1_i*rrz_jM));
			}

			grcEnd();
		}

		for (int j = 0; j <= M; j += stacksStep) // draw stacks
		{
			const Vec3V rrz_j = AddScaled(rrz0, rrz_dt, rrzt[j]);

			if (IsTrue(rrz_j.GetX() != ScalarV(V_ZERO)))
			{
				grcBegin(drawLineStrip, N + 1);

				for (int i = 0; i < N; i++)
				{
					grcVertex3f(Transform(basis, xy1[i]*rrz_j));
				}

				grcVertex3f(Transform(basis, xy1[0]*rrz_j)); // close loop

				grcEnd();
			}
		}		
	}
}

void CDVGeom::AddGridLinesToQuad(int quadIndex, int w, int h, float perspective)
{
	const CDVQuad& quad = m_quads[quadIndex];

	const Vec3V p0 = m_verts[quad.m_vertIndices[0]].m_position;
	const Vec3V p1 = m_verts[quad.m_vertIndices[1]].m_position;
	const Vec3V p2 = m_verts[quad.m_vertIndices[2]].m_position;
	const Vec3V p3 = m_verts[quad.m_vertIndices[3]].m_position;

	// p3----p2
	// |      |
	// |      |
	// |      |
	// p0----p1

	// could transpose this ..
	const float p0p1 = Mag(p0 - p1).Getf();
	const float p1p2 = Mag(p1 - p2).Getf();
	const float p2p3 = Mag(p2 - p3).Getf();
	const float p3p0 = Mag(p3 - p0).Getf();

	if (perspective == 0.0f || Abs<float>(p1p2 - p3p0) <= 0.0001f)
	{
		const ScalarV dtx = ScalarV(1.0f/(float)w);
		ScalarV tx = dtx;

		for (int x = 1; x < w; x++)
		{
			const Vec3V g0 = AddScaled(p0, p1 - p0, tx);
			const Vec3V g1 = AddScaled(p3, p2 - p3, tx);

			tx += dtx;

			m_lines.PushAndGrow(CDVLine(g0, g1, quadIndex));
		}
	}
	else
	{
		for (int x = 1; x < w; x++)
		{
			const ScalarV t = ScalarV((PerspectiveInterp(p3p0, p1p2, (float)x/(float)w, perspective) - p3p0)/(p1p2 - p3p0)); // (0..1)

			const Vec3V g0 = AddScaled(p0, p1 - p0, t);
			const Vec3V g1 = AddScaled(p3, p2 - p3, t);

			m_lines.PushAndGrow(CDVLine(g0, g1, quadIndex));
		}
	}

	if (perspective == 0.0f || Abs<float>(p2p3 - p0p1) <= 0.0001f)
	{
		const ScalarV dty = ScalarV(1.0f/(float)h);
		ScalarV ty = dty;

		for (int y = 1; y < h; y++)
		{
			const Vec3V g0 = AddScaled(p0, p3 - p0, ty);
			const Vec3V g1 = AddScaled(p1, p2 - p1, ty);

			ty += dty;

			m_lines.PushAndGrow(CDVLine(g0, g1, quadIndex));
		}
	}
	else
	{
		for (int y = 1; y < h; y++)
		{
			const ScalarV t = ScalarV((PerspectiveInterp(p0p1, p2p3, (float)y/(float)h, perspective) - p0p1)/(p2p3 - p0p1)); // (0..1)

			const Vec3V g0 = AddScaled(p0, p3 - p0, t);
			const Vec3V g1 = AddScaled(p1, p2 - p1, t);

			m_lines.PushAndGrow(CDVLine(g0, g1, quadIndex));
		}
	}
}

void CDVGeom::AddGridLinesToFace(int faceIndex, bool bStep3)
{
	const CDVFace& face = m_faces[faceIndex];
	const int n = face.m_vertIndices.size();
	const CDVSurfIndex si(faceIndex, true);

	if (n < 8)
	{
		return;
	}

	int i0 = n - 1;
	int i1 = (i0 + (bStep3 ? 3 : 2))%n;

	while (Abs<int>(i0 - i1) > 1)
	{
		const Vec3V g0 = m_verts[face.m_vertIndices[i0]].m_position;
		const Vec3V g1 = m_verts[face.m_vertIndices[i1]].m_position;

		i0 = (i0 + n - 1)%n;
		i1 = (i1     + 1)%n;

		m_lines.PushAndGrow(CDVLine(g0, g1, si));
	}

	i0 = n/4 - 1;
	i1 = (i0 + (bStep3 ? 3 : 2))%n;

	while (Abs<int>(i0 - i1) > 1)
	{
		const Vec3V g0 = m_verts[face.m_vertIndices[i0]].m_position;
		const Vec3V g1 = m_verts[face.m_vertIndices[i1]].m_position;

		i0 = (i0 + n - 1)%n;
		i1 = (i1     + 1)%n;

		m_lines.PushAndGrow(CDVLine(g0, g1, si));
	}
}

// ================================================================================================

int CDVGeom::AddConnectedVertex(Vec3V_In point)
{
	const ScalarV threshold = ScalarV(0.0005f);

	for (int i = 0; i < m_verts.size(); i++)
	{
		const Vec3V av = Abs(point - m_verts[i].m_position);
		const ScalarV m = Max(Max(av.GetX(), av.GetY()), av.GetZ());

		if (IsTrue(m < threshold))
		{
			return i;
		}
	}

	m_verts.PushAndGrow(CDVVert(point));

	return m_verts.size() - 1;
}

bool CDVGeom::AddConnectedEdge(int vertIndex0, int vertIndex1, const CDVSurfIndex& si)
{
	for (int i = 0; i < m_edges.size(); i++)
	{
		CDVEdge& edge = m_edges[i];

		if (edge.m_vertIndices[0] == vertIndex1 &&
			edge.m_vertIndices[1] == vertIndex0 &&
			edge.m_surfIndices[1].m_quadOrFaceIndex == INDEX_NONE)
		{
			edge.m_surfIndices[1] = si;
			return true;
		}
	}

	return false;
}

bool CDVGeom::AddConnectedFace(const atArray<Vec3V>& points)
{
	if (points.size() >= 3)
	{
		CDVSurfIndex si = INDEX_NONE;
		atArray<int> vertIndices;

		// add verts
		{
			vertIndices.Resize(points.size());

			for (int i = 0; i < points.size(); i++)
			{
				vertIndices[i] = AddConnectedVertex(points[i]);
			}
		}

		if (vertIndices.size() == 4) // add quad
		{
			si = CDVSurfIndex(m_quads.size(), false);
			CDVQuad& quad = m_quads.Grow();

			for (int i = 0; i < 4; i++)
			{
				quad.m_vertIndices[i] = vertIndices[i];
			}
		}
		else // add face
		{
			si = CDVSurfIndex(m_faces.size(), true);
			CDVFace& face = m_faces.Grow();

			//face.m_vertIndices.CopyFrom(vertIndices);
			face.m_vertIndices.Resize(vertIndices.size());

			for (int k = 0; k < vertIndices.size(); k++)
			{
				face.m_vertIndices[k] = vertIndices[k];
			}
		}

		// add edges
		{
			int vertIndex0 = vertIndices.back();

			for (int i = 0; i < vertIndices.size(); i++)
			{
				const int vertIndex1 = vertIndices[i];

				if (vertIndex0 != vertIndex1)
				{
					if (!AddConnectedEdge(vertIndex0, vertIndex1, si))
					{
						m_edges.PushAndGrow(CDVEdge(vertIndex0, vertIndex1, si, INDEX_NONE));
					}

					vertIndex0 = vertIndex1;
				}
			}
		}

		return true;
	}

	return false;
}

// ================================================================================================

void CDVGeom::CalcNormals()
{
	for (int i = 0; i < m_quads.size(); i++)
	{
		CDVQuad& quad = m_quads[i];

		const Vec3V p0 = m_verts[quad.m_vertIndices[0]].m_position;
		const Vec3V p1 = m_verts[quad.m_vertIndices[1]].m_position;
		const Vec3V p2 = m_verts[quad.m_vertIndices[2]].m_position;
		const Vec3V p3 = m_verts[quad.m_vertIndices[3]].m_position;

		quad.m_centroid = (p0 + p1 + p2 + p3)*ScalarV(V_QUARTER);
		quad.m_normal   = Normalize(Cross(p1 - p0, p2 - p0) + Cross(p2 - p0, p3 - p0));
	}

	for (int i = 0; i < m_faces.size(); i++)
	{
		CDVFace& face = m_faces[i];

		Vec3V centroid(V_ZERO);

		for (int k = 0; k < face.m_vertIndices.size(); k++)
		{
			centroid += m_verts[face.m_vertIndices[k]].m_position;
		}

		// should be stable, provided faces don't have degenerate verts
		const int vi0 = 0;
		const int vi1 = (1*face.m_vertIndices.size())/3;
		const int vi2 = (2*face.m_vertIndices.size())/3;
		const Vec3V p0 = m_verts[face.m_vertIndices[vi0]].m_position;
		const Vec3V p1 = m_verts[face.m_vertIndices[vi1]].m_position;
		const Vec3V p2 = m_verts[face.m_vertIndices[vi2]].m_position;

		face.m_centroid = centroid*ScalarV(1.0f/(float)face.m_vertIndices.size());
		face.m_normal   = Normalize(Cross(p1 - p0, p2 - p0));
	}
}

#if __DEV

int CDVGeom::CheckWinding(const CDVSurfIndex& si, int vertIndex0, int vertIndex1) const
{
	if (si.m_isFaceIndex && si.m_quadOrFaceIndex >= 0 && si.m_quadOrFaceIndex < m_faces.size())
	{
		const CDVFace& face = m_faces[si.m_quadOrFaceIndex];

		int j = face.m_vertIndices.size();

		for (int k = 0; k < face.m_vertIndices.size(); k++)
		{
			if (face.m_vertIndices[j] == vertIndex0 &&
				face.m_vertIndices[k] == vertIndex1)
			{
				return +1; // positive winding
			}
			else
			if (face.m_vertIndices[j] == vertIndex1 &&
				face.m_vertIndices[k] == vertIndex0)
			{
				return -1; // negative winding
			}

			j = k;
		}
	}
	else if (si.m_isFaceIndex == 0 && si.m_quadOrFaceIndex >= 0 && si.m_quadOrFaceIndex < m_quads.size())
	{
		const CDVQuad& quad = m_quads[si.m_quadOrFaceIndex];

		int j = 3;

		for (int k = 0; k < 4; k++)
		{
			if (quad.m_vertIndices[j] == vertIndex0 &&
				quad.m_vertIndices[k] == vertIndex1)
			{
				return +1; // positive winding
			}
			else
			if (quad.m_vertIndices[j] == vertIndex1 &&
				quad.m_vertIndices[k] == vertIndex0)
			{
				return -1; // negative winding
			}

			j = k;
		}
	}

	return 0;
}

#define CDVGEOM_VERIFY(cond) { if (!AssertVerify(cond)) { bOk = false; } }

bool CDVGeom::Check() const
{
	bool bOk = true;

	for (int i = 0; i < m_lines.size(); i++)
	{
		const CDVLine& line = m_lines[i];

		if (line.m_surfIndex.m_quadOrFaceIndex != INDEX_NONE)
		{
			CDVGEOM_VERIFY(line.m_surfIndex.m_quadOrFaceIndex >= 0);

			if (line.m_surfIndex.m_isFaceIndex)
			{
				CDVGEOM_VERIFY(line.m_surfIndex.m_quadOrFaceIndex < m_faces.size());
			}
			else
			{
				CDVGEOM_VERIFY(line.m_surfIndex.m_quadOrFaceIndex < m_quads.size());
			}
		}
	}

	for (int i = 0; i < m_edges.size(); i++)
	{
		const CDVEdge& edge = m_edges[i];

		// verify vert indices are valid
		CDVGEOM_VERIFY(edge.m_vertIndices[0] >= 0 && edge.m_vertIndices[0] < m_verts.size());
		CDVGEOM_VERIFY(edge.m_vertIndices[1] >= 0 && edge.m_vertIndices[1] < m_verts.size());
		CDVGEOM_VERIFY(edge.m_vertIndices[0] != edge.m_vertIndices[1]);

		if (edge.m_surfIndices[0].m_quadOrFaceIndex != INDEX_NONE &&
			edge.m_surfIndices[1].m_quadOrFaceIndex != INDEX_NONE)
		{
			// verify surf indices are not the same surface
			CDVGEOM_VERIFY(edge.m_surfIndices[0].m_quadOrFaceIndex != edge.m_surfIndices[1].m_quadOrFaceIndex || edge.m_surfIndices[0].m_isFaceIndex != edge.m_surfIndices[1].m_isFaceIndex);
		}

		const int winding0 = CheckWinding(edge.m_surfIndices[0], edge.m_vertIndices[0], edge.m_vertIndices[1]);
		const int winding1 = CheckWinding(edge.m_surfIndices[1], edge.m_vertIndices[0], edge.m_vertIndices[1]);

		// verify surf indices are valid, and edge windings are correct
		CDVGEOM_VERIFY(winding0 != 0);
		CDVGEOM_VERIFY(winding1 != 0);
		CDVGEOM_VERIFY(winding0 != winding1);
	}

	for (int i = 0; i < m_faces.size(); i++)
	{
		const CDVFace& face = m_faces[i];

		CDVGEOM_VERIFY(face.m_vertIndices.size() >= 3);
		
		for (int k = 0; k < face.m_vertIndices.size(); k++)
		{
			CDVGEOM_VERIFY(face.m_vertIndices[k] >= 0 && face.m_vertIndices[k] < m_verts.size());
		}
	}

	return bOk;
}

#undef CDVGEOM_VERIFY
#endif // __DEV

// ================================================================================================

void CDVGeomGroup::AddProxy(CDVGeomProxy* proxy)
{
	m_proxies.PushAndGrow(proxy);
}

void CDVGeomGroup::Sort(Vec3V_In camPos, Vec3V_In camDir)
{
	if (IsEqualAll(camDir, Vec3V(V_ZERO)))
	{
		for (int i = 0; i < m_proxies.size(); i++)
		{
			m_proxies[i]->m_distance = Mag(m_proxies[i]->GetCentroid() - camPos).Getf();
		}
	}
	else // sort by z-distance
	{
		for (int i = 0; i < m_proxies.size(); i++)
		{
			m_proxies[i]->m_distance = Dot(m_proxies[i]->GetCentroid() - camPos, camDir).Getf();
		}
	}

	m_proxies.QSort(0, -1, CDVGeomProxy::CompareFuncPtr);
}

void CDVGeomGroup::Draw(Vec3V_In camPos, float opacity, float distanceMin, float distanceMax, bool fw) const
{
	if (opacity > 0.0f && distanceMin < distanceMax)
	{
		for (int i = 0; i < m_proxies.size(); i++)
		{
			const CDVGeomProxy* proxy = m_proxies[i];

			if (proxy->m_distance <= distanceMax) // should also check that distance is >= -objectRadius somehow
			{
				const float proxyOpacity = opacity*Min<float>(1.0f, (proxy->m_distance - distanceMax)/(distanceMin - distanceMax));

				proxy->Draw(camPos, proxyOpacity, fw);
			}
		}
	}
}

void CDVGeomGroup::Clear()
{
	for (int i = 0; i < m_proxies.size(); i++)
	{
		delete m_proxies[i];
	}

	m_proxies.clear();
}

// ================================================================================================

static Vec3V gCamPos = Vec3V(V_ZERO); // aw fuck.

#if __DEV

enum eDVGTProxyType
{
	DVGTPT_NONE = 0,
#define DEF_CDVGeomParams(type) DVGTPT_##type,
FOREACH(DEF_CDVGeomParams)
#undef  DEF_CDVGeomParams
	DVGTPT_COUNT
};

static const char** GetUIStrings_eDVGTProxyType()
{
	static const char* strings[] =
	{
		STRING(DVGTPT_NONE),
#define DEF_CDVGeomParams(type) STRING(DVGTPT_##type),
FOREACH(DEF_CDVGeomParams)
#undef  DEF_CDVGeomParams
		NULL
	};
	return strings;
}


class CDVGeomTest : public datBase
{
public:
	void Init();
	void InitWidgets();

	void Update(const grcViewport* vp);
	void Clear();

	void CreateFractal1(); // test
	void CreateFractal2(); // test

#define DEF_CDVGeomParams(type) template <bool bAddProxy> CDVGeomProxy* Create##type();
FOREACH(DEF_CDVGeomParams)
#undef  DEF_CDVGeomParams

private:
	Mat34V         m_basisLocked;
	Mat34V         m_basis; // adjusted basis

	float          m_opacity;
	float          m_distanceMin;
	float          m_distanceMax;
	bool           m_sortByCamZ;

	bool           m_lockCamera;
	float          m_yaw;
	float          m_pitch;
	float          m_roll;
	Vec3V          m_localOffset;
	Vec3V          m_worldOffset;
	int            m_realtimeProxyType; // eDVGTProxyType

	bool           m_colourStyleEnabled;
	CDVColourStyle m_colourStyle;

	CDVGeomGroup   m_group; // proxies

#define DEF_CDVGeomParams(type) CDVGeomParams##type m_test##type;
FOREACH(DEF_CDVGeomParams)
#undef  DEF_CDVGeomParams
};

// ==============================================

static CDVGeomTest gDVGeomTest;

void CDVGeomTest::Init()
{
	m_basisLocked        = Mat34V(V_IDENTITY);
	m_basis              = Mat34V(V_IDENTITY);

	m_opacity            = 0.0f;
	m_distanceMin        = 10.0f;
	m_distanceMax        = 50.0f;
	m_sortByCamZ         = true;

	m_lockCamera         = false;
	m_yaw                = 0.0f;
	m_pitch              = 0.0f;
	m_roll               = 0.0f;
	m_localOffset        = Vec3V(V_ZERO);
	m_worldOffset        = Vec3V(V_ZERO);
	m_realtimeProxyType  = DVGTPT_NONE;

	m_colourStyleEnabled = true;

	// these get inited in their own InitWidgets
	sysMemSet(&m_colourStyle, 0, sizeof(m_colourStyle));
	
#define DEF_CDVGeomParams(type) sysMemSet(&m_test##type, 0, sizeof(m_test##type));
FOREACH(DEF_CDVGeomParams)
#undef  DEF_CDVGeomParams
}

PARAM(CDVGeomTest,"CDVGeomTest");

void CDVGeomTest::InitWidgets()
{
	if (PARAM_CDVGeomTest.Get())
	{
		bkBank& bk = BANKMGR.CreateBank("CDVGeomTest");

		Init();

		bk.AddSlider("Opacity"      , &m_opacity    , 0.0f,   1.0f, 1.0f/32.0f);
		bk.AddSlider("Distance Min" , &m_distanceMin, 0.0f, 100.0f, 1.0f/32.0f);
		bk.AddSlider("Distance Max" , &m_distanceMax, 0.0f, 100.0f, 1.0f/32.0f);
		bk.AddToggle("Sort by Cam Z", &m_sortByCamZ);
		bk.AddButton("Clear All"    , datCallback(MFA(CDVGeomTest::Clear), this));

		bk.AddSeparator();

		bk.AddToggle("Lock Camera"        , &m_lockCamera);
		bk.AddAngle ("Yaw"                , &m_yaw              , bkAngleType::DEGREES);
		bk.AddAngle ("Pitch"              , &m_pitch            , bkAngleType::DEGREES);
		bk.AddAngle ("Roll"               , &m_roll             , bkAngleType::DEGREES);
		bk.AddVector("Local Offset"       , &m_localOffset      , -100.0f, 100.0f, 1.0f/32.0f);
		bk.AddVector("World Offset"       , &m_worldOffset      , -100.0f, 100.0f, 1.0f/32.0f);
		bk.AddCombo ("Realtime Proxy Type", &m_realtimeProxyType, DVGTPT_COUNT, GetUIStrings_eDVGTProxyType());

		bk.AddSeparator();

		bk.AddToggle("Colour Style Enabled", &m_colourStyleEnabled);
		bk.PushGroup("Colour Style", false);
		{
			m_colourStyle.InitWidgets(bk);
		}
		bk.PopGroup();

#define DEF_CDVGeomParams(type) bk.PushGroup("Test " STRING(type), false); m_test##type.InitWidgets(bk, this); bk.PopGroup();
FOREACH(DEF_CDVGeomParams)
#undef  DEF_CDVGeomParams

		bk.AddButton("CreateFractal1", datCallback(MFA(CDVGeomTest::CreateFractal1), this));
		bk.AddButton("CreateFractal2", datCallback(MFA(CDVGeomTest::CreateFractal2), this));
	}
}

void CDVGeomTest::Update(const grcViewport* vp)
{
	if (PARAM_CDVGeomTest.Get() && vp)
	{
		gCamPos = vp->GetCameraMtx().d();

		if (!m_lockCamera)
		{
			m_basisLocked = vp->GetCameraMtx();
		}

		m_basis = Mat34V(Mat33V(V_IDENTITY), m_basisLocked.d());

		const Vec3V camPos = +vp->GetCameraMtx().d();
		const Vec3V camDir = -vp->GetCameraMtx().c();

		if (m_opacity > 0.0f && m_group.m_proxies.size() > 0)
		{
			grcDebugDraw::SetDoDebugZTest(true);

			m_group.Sort(camPos, m_sortByCamZ ? camDir : Vec3V(V_ZERO));
			m_group.Draw(camPos, m_opacity, m_distanceMin, m_distanceMax, true);
		}

		// adjust
		{
			if (m_yaw != 0.0f)
			{
				Matrix33 rot;
				rot.MakeRotateZ(-DtoR*m_yaw);
				rot.Transform(RC_VECTOR3(m_basis.GetCol0Ref()));
				rot.Transform(RC_VECTOR3(m_basis.GetCol1Ref()));
				rot.Transform(RC_VECTOR3(m_basis.GetCol2Ref()));
			}

			if (m_pitch != 0.0f)
			{
				Matrix33 rot;
				rot.MakeRotate(VEC3V_TO_VECTOR3(m_basis.a()), DtoR*m_pitch);
				rot.Transform(RC_VECTOR3(m_basis.GetCol0Ref()));
				rot.Transform(RC_VECTOR3(m_basis.GetCol1Ref()));
				rot.Transform(RC_VECTOR3(m_basis.GetCol2Ref()));
			}

			if (m_roll != 0.0f)
			{
				Matrix33 rot;
				rot.MakeRotate(VEC3V_TO_VECTOR3(m_basis.c()), -DtoR*m_roll);
				rot.Transform(RC_VECTOR3(m_basis.GetCol0Ref()));
				rot.Transform(RC_VECTOR3(m_basis.GetCol1Ref()));
				rot.Transform(RC_VECTOR3(m_basis.GetCol2Ref()));
			}

			m_basis.GetCol3Ref() += Transform3x3(m_basis, m_localOffset*Vec3V(1.0f, 1.0f, -1.0f));
			m_basis.GetCol3Ref() += m_worldOffset;
		}

		if (m_opacity > 0.0f) // construct a realtime proxy, draw it (with opacity 0.5), and delete it
		{
			CDVGeomProxy* proxy = NULL;

			switch (m_realtimeProxyType)
			{
			case DVGTPT_NONE: break;
#define DEF_CDVGeomParams(type) case DVGTPT_##type: proxy = Create##type<false>(); break;
FOREACH(DEF_CDVGeomParams)
#undef  DEF_CDVGeomParams
			default: Assertf(0, "CDVGeomTest: unknown realtime proxy type (%d)", m_realtimeProxyType);
			}

			if (proxy)
			{
				grcDebugDraw::SetDoDebugZTest(true);

				proxy->Draw(camPos, m_opacity*0.5f, true);
				delete proxy;
				proxy = NULL;
			}
		}
	}
}

void CDVGeomTest::Clear()
{
	m_group.Clear();
}

template <typename ParamsType> class CDVGeomProxyT : public CDVGeomProxy
{
public:
	explicit CDVGeomProxyT() {}
	virtual ~CDVGeomProxyT() {}
	CDVGeomProxyT(const CDVColourStyle* colourStyle, const Color32& colour, Mat34V_In basis, const ParamsType& params) : CDVGeomProxy(colourStyle, colour, basis), m_params(params) {}

	virtual void Draw(Vec3V_In camPos, float opacity, bool fw) const
	{
		m_params.Draw(m_colourStyle, m_colour, opacity, m_basis, camPos, fw);
	}

	ParamsType m_params;
};

#define DEF_CDVGeomParams(type) \
	template <bool bAddProxy> CDVGeomProxy* CDVGeomTest::Create##type() \
	{ \
		CDVGeomProxy* proxy = rage_new CDVGeomProxyT<CDVGeomParams##type>(m_colourStyleEnabled ? &m_colourStyle : NULL, m_colourStyle.m_colours[DVP_FRONT_EDGES], m_basis, m_test##type); \
		if (bAddProxy) \
		{ \
			m_group.AddProxy(proxy); \
		} \
		return proxy; \
	} \
	// end.
FOREACH(DEF_CDVGeomParams)
#undef  DEF_CDVGeomParams

// Forcing the template instantiation (Orbis build doesn't link without that)
#define DEF_CDVGeomParams(type) \
	template CDVGeomProxy* CDVGeomTest::Create##type<true>();
FOREACH(DEF_CDVGeomParams)
#undef  DEF_CDVGeomParams


// ==============================================
// This section has to follow the CDVGeomTest::Create* implementations

void CDVGeomParamsBox::InitWidgets(bkBank& bk, CDVGeomTest* test)
{
	m_boxMin  = Vec3V(V_NEGFOUR);
	m_boxMax  = Vec3V(V_FOUR);
	m_stacksX = 5;
	m_stacksY = 5;
	m_stacksZ = 5;

	bk.AddVector("m_boxMin" , &m_boxMin , -8.0f, 8.0f, 1.0f/32.0f);
	bk.AddVector("m_boxMax" , &m_boxMax , -8.0f, 8.0f, 1.0f/32.0f);
	bk.AddSlider("m_stacksX", &m_stacksX, 1, 64, 1);
	bk.AddSlider("m_stacksY", &m_stacksY, 1, 64, 1);
	bk.AddSlider("m_stacksZ", &m_stacksZ, 1, 64, 1);

	bk.AddButton("CreateBox", datCallback(MFA(CDVGeomTest::CreateBox<true>), test));
}

void CDVGeomParamsBoxProj::InitWidgets(bkBank& bk, CDVGeomTest* test)
{
	m_boxMin      = Vec3V(V_NEGFOUR);
	m_boxMax      = Vec3V(V_FOUR);
	m_stacksX     = 8;
	m_stacksY     = 8;
	m_stacksZ     = 8;
	m_proj        = Vec3V(V_ZERO);
	m_perspective = ScalarV(V_HALF);

	bk.AddVector("m_boxMin"     , &m_boxMin     , -8.0f, 8.0f, 1.0f/32.0f);
	bk.AddVector("m_boxMax"     , &m_boxMax     , -8.0f, 8.0f, 1.0f/32.0f);
	bk.AddSlider("m_stacksX"    , &m_stacksX    , 1, 64, 1);
	bk.AddSlider("m_stacksY"    , &m_stacksY    , 1, 64, 1);
	bk.AddSlider("m_stacksZ"    , &m_stacksZ    , 1, 64, 1);
	bk.AddVector("m_proj"       , &m_proj       , -1.0f, 1.0f, 1.0f/32.0f);
	bk.AddSlider("m_perspective", &m_perspective, -1.0f, 2.0f, 1.0f/32.0f);

	bk.AddButton("CreateBoxProj", datCallback(MFA(CDVGeomTest::CreateBoxProj<true>), test));
}

void CDVGeomParamsFrustum::InitWidgets(bkBank& bk, CDVGeomTest* test)
{
	m_tanHFOV     = ScalarV(tanf(0.5f*DtoR*80.0f));
	m_tanVFOV     = ScalarV(3.0f/4.0f)*m_tanHFOV;
	m_z0          = ScalarV(V_QUARTER);
	m_z1          = ScalarV(V_EIGHT);
	m_stacksX     = 5;
	m_stacksY     = 5;
	m_stacksZ     = 16;
	m_perspective = ScalarV(V_HALF);

	bk_AddAngle ("m_tanHFOV"    , &m_tanHFOV    , bkAngleType::DEGREES, true, -70.0f, +70.0f);
	bk_AddAngle ("m_tanVFOV"    , &m_tanVFOV    , bkAngleType::DEGREES, true, -70.0f, +70.0f);
	bk.AddSlider("m_z0"         , &m_z0         , 1.0f/32.0f, 8.0f, 1.0f/32.0f);
	bk.AddSlider("m_z1"         , &m_z1         , 1.0f/32.0f, 8.0f, 1.0f/32.0f);
	bk.AddSlider("m_stacksX"    , &m_stacksX    , 1, 64, 1);
	bk.AddSlider("m_stacksY"    , &m_stacksY    , 1, 64, 1);
	bk.AddSlider("m_stacksZ"    , &m_stacksZ    , 1, 64, 1);
	bk.AddSlider("m_perspective", &m_perspective, -1.0f, 2.0f, 1.0f/32.0f);

	bk.AddButton("CreateFrustum", datCallback(MFA(CDVGeomTest::CreateFrustum<true>), test));
}

void CDVGeomParamsSphere::InitWidgets(bkBank& bk, CDVGeomTest* test)
{
	m_radius     = ScalarV(V_FOUR);
	m_slices     = 20;
	m_stacks     = 16;
	m_slicesTess = 1;
	m_stacksTess = 1;

	bk.AddSlider("m_radius"    , &m_radius    , 1.0f/32.0f, 8.0f, 1.0f/32.0f);
	bk.AddSlider("m_slices"    , &m_slices    , 3, 64, 1);
	bk.AddSlider("m_stacks"    , &m_stacks    , 1, 64, 1);
	bk.AddSlider("m_slicesTess", &m_slicesTess, 1,  8, 1);
	bk.AddSlider("m_stacksTess", &m_stacksTess, 1,  8, 1);

	bk.AddButton("CreateSphere", datCallback(MFA(CDVGeomTest::CreateSphere<true>), test));
}

void CDVGeomParamsHemisphere::InitWidgets(bkBank& bk, CDVGeomTest* test)
{
	m_radius     = ScalarV(V_FOUR);
	m_slices     = 20;
	m_stacks     = 8;
	m_slicesTess = 1;
	m_stacksTess = 1;
	m_faceGrid   = true;

	bk.AddSlider("m_radius"    , &m_radius    , 1.0f/32.0f, 8.0f, 1.0f/32.0f);
	bk.AddSlider("m_slices"    , &m_slices    , 3, 64, 1);
	bk.AddSlider("m_stacks"    , &m_stacks    , 1, 64, 1);
	bk.AddSlider("m_slicesTess", &m_slicesTess, 1,  8, 1);
	bk.AddSlider("m_stacksTess", &m_stacksTess, 1,  8, 1);
	bk.AddToggle("m_faceGrid"  , &m_faceGrid);

	bk.AddButton("CreateHemisphere", datCallback(MFA(CDVGeomTest::CreateHemisphere<true>), test));
}

void CDVGeomParamsCylinder::InitWidgets(bkBank& bk, CDVGeomTest* test)
{
	m_radius0    = ScalarV(V_TWO);
	m_radius1    = ScalarV(V_TWO);
	m_z0         = ScalarV(V_NEGTWO);
	m_z1         = ScalarV(V_TWO);
	m_slices     = 20;
	m_stacks     = 8;
	m_slicesTess = 1;
	m_stacksTess = 1;
	m_faceGrid   = true;

	bk.AddSlider("m_radius0"   , &m_radius0   , -1.0f, 8.0f, 1.0f/32.0f);
	bk.AddSlider("m_radius1"   , &m_radius1   , -1.0f, 8.0f, 1.0f/32.0f);
	bk.AddSlider("m_z0"        , &m_z0        , -8.0f, 8.0f, 1.0f/32.0f);
	bk.AddSlider("m_z1"        , &m_z1        , -8.0f, 8.0f, 1.0f/32.0f);
	bk.AddSlider("m_slices"    , &m_slices    , 3, 64, 1);
	bk.AddSlider("m_stacks"    , &m_stacks    , 1, 64, 1);
	bk.AddSlider("m_slicesTess", &m_slicesTess, 1,  8, 1);
	bk.AddSlider("m_stacksTess", &m_stacksTess, 1,  8, 1);
	bk.AddToggle("m_faceGrid"  , &m_faceGrid);

	bk.AddButton("CreateCylinder", datCallback(MFA(CDVGeomTest::CreateCylinder<true>), test));
}

void CDVGeomParamsCone::InitWidgets(bkBank& bk, CDVGeomTest* test)
{
	m_radius       = ScalarV(V_FOUR);
	m_tanTheta     = ScalarV(tanf(0.5f*DtoR*60.0f));
	m_slices       = 20;
	m_stacks       = 12;
	m_slicesTess   = 1;
	m_stacksTess   = 1;
	m_capCurvature = ScalarV(V_ONE);
	m_capStacks    = 8;
	m_perspective  = ScalarV(V_ZERO);
	m_faceGrid     = true;

	bk.AddSlider("m_radius"      , &m_radius      , -8.0f, 8.0f, 1.0f/32.0f);
	bk_AddAngle ("m_tanTheta"    , &m_tanTheta    , bkAngleType::DEGREES, true, -70.0f, +70.0f);
	bk.AddSlider("m_slices"      , &m_slices      , 3, 64, 1);
	bk.AddSlider("m_stacks"      , &m_stacks      , 1, 64, 1);
	bk.AddSlider("m_slicesTess"  , &m_slicesTess  , 1,  8, 1);
	bk.AddSlider("m_stacksTess"  , &m_stacksTess  , 1,  8, 1);
	bk.AddSlider("m_capCurvature", &m_capCurvature, 0.0f, 2.0f, 1.0f/32.0f);
	bk.AddSlider("m_capStacks"   , &m_capStacks   , 0, 64, 1);
	bk.AddSlider("m_perspective" , &m_perspective , 0.0f, 1.0f, 1.0f/32.0f);
	bk.AddToggle("m_faceGrid"    , &m_faceGrid);

	bk.AddButton("CreateCone", datCallback(MFA(CDVGeomTest::CreateCone<true>), test));
}

void CDVGeomParamsCapsule::InitWidgets(bkBank& bk, CDVGeomTest* test)
{
	m_z0         = ScalarV(V_NEGTWO);
	m_z1         = ScalarV(V_TWO);
	m_radius     = ScalarV(V_TWO);
	m_slices     = 20;
	m_stacks     = 8;
	m_slicesTess = 1;
	m_stacksTess = 1;
	m_capStacks  = 8;

	bk.AddSlider("m_z0"        , &m_z0        , -8.0f, 8.0f, 1.0f/32.0f);
	bk.AddSlider("m_z1"        , &m_z1        , -8.0f, 8.0f, 1.0f/32.0f);
	bk.AddSlider("m_radius"    , &m_radius    ,  0.0f, 8.0f, 1.0f/32.0f);
	bk.AddSlider("m_slices"    , &m_slices    , 3, 64, 1);
	bk.AddSlider("m_stacks"    , &m_stacks    , 1, 64, 1);
	bk.AddSlider("m_slicesTess", &m_slicesTess, 1,  8, 1);
	bk.AddSlider("m_stacksTess", &m_stacksTess, 1,  8, 1);
	bk.AddSlider("m_capStacks" , &m_capStacks , 0, 64, 1);

	bk.AddButton("CreateCapsule", datCallback(MFA(CDVGeomTest::CreateCapsule<true>), test));
}

// ==============================================

static Vec4V_Out CreateFractal1_Sphere(Vec3V_In v)
{
	return Vec4V(v.GetXY(), ScalarV(V_ZERO), ScalarV(V_ONE))/Abs(v.GetZ());
}

static void CreateFractal1_Recursive(atArray<Vec4V>& spheres, Vec3V_In a, Vec3V_In b, Vec3V_In c, Vec3V_In d, ScalarV_In threshold)
{
	// given a,b,c,d mutually tangent, find e mutually tangent to a,b,c
	const Vec3V e = (a + b + c)*ScalarV(V_TWO) - d;
	const ScalarV curvature = Abs(e.GetZ());

	if (IsTrue(curvature <= threshold))
	{
		const Vec4V sphere = CreateFractal1_Sphere(e);

		spheres.PushAndGrow(sphere);

		CreateFractal1_Recursive(spheres, a, b, e, c, threshold);
		CreateFractal1_Recursive(spheres, b, c, e, a, threshold);
		CreateFractal1_Recursive(spheres, c, a, e, b, threshold);
	}
}

static void CreateFractal1_Internal(atArray<Vec4V>& spheres)
{
	const Vec3V d ( 0.0f,  0.0f, -1.0f);
	const Vec3V a ( 0.0f, -1.0f, +2.0f);
	const Vec3V b ( 0.0f, +1.0f, +2.0f);
	const Vec3V c0(-2.0f,  0.0f, +3.0f);
	const Vec3V c1(+2.0f,  0.0f, +3.0f);
	const ScalarV threshold(55.0f);

	spheres.PushAndGrow(CreateFractal1_Sphere(d));
	spheres.PushAndGrow(CreateFractal1_Sphere(a));
	spheres.PushAndGrow(CreateFractal1_Sphere(b));

	CreateFractal1_Recursive(spheres, a, b, d, c0, threshold);
	CreateFractal1_Recursive(spheres, a, b, d, c1, threshold);
}

void CDVGeomTest::CreateFractal1()
{
	const ScalarV radius = m_testHemisphere.m_radius;
	atArray<Vec4V> spheres;

	CreateFractal1_Internal(spheres);

	for (int i = 0; i < spheres.size(); i++)
	{
		typedef CDVGeomProxyT<CDVGeomParamsHemisphere> ProxyType;
		ProxyType* proxy = (ProxyType*)CDVGeomTest::CreateHemisphere<true>();

		proxy->m_basis.GetCol3Ref() += spheres[i].GetXYZ()*radius;
		proxy->m_params.m_radius    *= spheres[i].GetW();
	}
}

void CDVGeomTest::CreateFractal2()
{
	const ScalarV radius = m_testCylinder.m_radius0;
	atArray<Vec4V> spheres;

	CreateFractal1_Internal(spheres);

	for (int i = 0; i < spheres.size(); i++)
	{
		typedef CDVGeomProxyT<CDVGeomParamsCylinder> ProxyType;
		ProxyType* proxy = (ProxyType*)CDVGeomTest::CreateCylinder<true>();

		proxy->m_basis.GetCol3Ref() += spheres[i].GetXYZ()*radius;
		proxy->m_params.m_radius0   *= spheres[i].GetW();
		proxy->m_params.m_radius1   *= spheres[i].GetW();
	}
}

// ==============================================

__COMMENT(static) CDVGeomTestInterface& CDVGeomTestInterface::Get()
{
	static CDVGeomTestInterface sDVGeomTestInterface;
	return sDVGeomTestInterface;
}

void CDVGeomTestInterface::Init()
{
	gDVGeomTest.Init();
}

void CDVGeomTestInterface::InitWidgets()
{
	gDVGeomTest.InitWidgets();
}

void CDVGeomTestInterface::Update(const grcViewport* vp)
{
	gDVGeomTest.Update(vp);
}

#endif // __DEV

// ================================================================================================
// ================================================================================================
// ================================================================================================
// temporary experimentation as an extension of fwDebugDraw ..

namespace gtaDebugDraw {

#define MAX_COLOUR_STYLES 8

static CDVColourStyle gDVColourStyleStack[MAX_COLOUR_STYLES];
static int            gDVColourStyleCount = 0;

void InitColourStyle()
{
	CDVColourStyle* cs = PushColourStyle();

	cs->m_colours[DVP_BACK_SURFS      ] = Color32(  0,  0,  0, 24);
	cs->m_colours[DVP_BACK_LINES      ] = Color32(  0,  0,  0, 32);
	cs->m_colours[DVP_BACK_EDGES      ] = Color32(  0,  0, 64, 64);
	cs->m_colours[DVP_FRONT_SURFS     ] = Color32(  0,  0,128, 64);
	cs->m_colours[DVP_FRONT_LINES     ] = Color32(255,255,255, 96);
	cs->m_colours[DVP_FRONT_EDGES     ] = Color32(  0,  0,  0, 96);
	cs->m_colours[DVP_SILHOUETTE_EDGES] = Color32(  0,  0,  0,255);
}

CDVColourStyle* PushColourStyle()
{
	Assert(gDVColourStyleCount < MAX_COLOUR_STYLES);

	if (gDVColourStyleCount < MAX_COLOUR_STYLES)
	{
		if (gDVColourStyleCount > 0) // copy previous colourstyle
		{
			gDVColourStyleStack[gDVColourStyleCount] = gDVColourStyleStack[gDVColourStyleCount - 1];
		}

		return &gDVColourStyleStack[gDVColourStyleCount++];
	}

	return NULL;
}

void PopColourStyle()
{
	Assert(gDVColourStyleCount > 1);

	if (gDVColourStyleCount > 1)
	{
		gDVColourStyleCount--;
	}
}

class CFixDepthTestProxy : public grcDrawProxy // TEMPORARY!!
{
public:
	CFixDepthTestProxy() {}

	virtual int  Size() const { return sizeof(*this); }
	virtual void Draw() const
	{
		grcStateBlock::SetDepthStencilState(CShaderLib::DSS_TestOnly_LessEqual_Invert);
	}
};

void PrettySphere(Vec4V_In centreAndRadius, int slices, int stacks, float opacity)
{
	if (gDVColourStyleCount == 0) { InitColourStyle(); } grcDebugDraw::Proxy(CFixDepthTestProxy()); // TEMPORARY!!

	grcDebugDraw::Proxy(CDVGeomProxyT_csref(Sphere)( // copies colourstyle from stack
		gDVColourStyleStack[gDVColourStyleCount - 1],
		Color32(0),
		opacity,
		CDVGeomParamsSphere(
			centreAndRadius.GetW(),
			slices,
			stacks,
			1, // slicesTess
			1  // stacksTess
		),
		Mat34V(Mat33V(V_IDENTITY), centreAndRadius.GetXYZ()),
		gCamPos
	));
}

void ColourSphere(Vec4V_In centreAndRadius, int slices, int stacks, const Color32& colour, float opacity)
{
	if (gDVColourStyleCount == 0) { InitColourStyle(); } grcDebugDraw::Proxy(CFixDepthTestProxy()); // TEMPORARY!!

	grcDebugDraw::Proxy(CDVGeomProxyT_csptr(Sphere)( // colourstyle is NULL, so colour will be used
		NULL,
		colour,
		opacity,
		CDVGeomParamsSphere(
			centreAndRadius.GetW(),
			slices,
			stacks,
			1, // slicesTess
			1  // stacksTess
		),
		Mat34V(Mat33V(V_IDENTITY), centreAndRadius.GetXYZ()),
		gCamPos
	));
}

} // namespace gtaDebugDraw

// ================================================================================================

/*
PerspectiveInterp[z0_,z1_,t_,e_] := Block[{zlin,zexp},
	zlin=z0+(z1-z0)*t;
	zexp=z0*((z1/z0)^t);
	zlin+(zexp-zlin)*e
];
DrawGrid[w_,h_,e_] := Block[{p0,p1,p2,p3,d01,d12,d23,d30},
	p0 = {0.2, 0};
	p1 = {0.5, -0.1};
	p2 = {1, 0.6};
	p3 = {0, 0.33};
	d01=Sqrt[(p0-p1).(p0-p1)];
	d12=Sqrt[(p1-p2).(p1-p2)];
	d23=Sqrt[(p2-p3).(p2-p3)];
	d30=Sqrt[(p3-p0).(p3-p0)];
	Graphics[Join[
		Table[{Line[{
			p0+(p1-p0)*(PerspectiveInterp[d30,d12,x/w,e]-d30)/(d12-d30),
			p3+(p2-p3)*(PerspectiveInterp[d30,d12,x/w,e]-d30)/(d12-d30)
		}]},{x,0,w}],
		Table[{Line[{
			p0+(p3-p0)*(PerspectiveInterp[d01,d23,y/h,e]-d01)/(d23-d01),
			p1+(p2-p1)*(PerspectiveInterp[d01,d23,y/h,e]-d01)/(d23-d01)
		}]},{y,0,h}]
	]]
];
DrawGrid[15,15,1] (* <-- 'e' can range from [-1..2] *)

Column[{
	Slider[Dynamic[e$], {-1, 2}],
	DrawGrid[15,15,Dynamic[e$]]
}]

Column[{
	Slider[Dynamic[z0$], {0, 9}],
	Slider[Dynamic[z1$], {0, 9}], 
	Slider[Dynamic[ze$], {0, 1}], 
	Row[{
		Block[{tx = 1/2, z0, z1, ze},
			{z0, z1} = Sort[{Dynamic[z0$], Dynamic[z1$]}];
			ze = Dynamic[ze$];
			Graphics[{
				Line[{{0, 0}, {-tx, 1}*10}],
				Line[{{0, 0}, {+tx, 1}*10}],
				Line[{{-tx, 1}, {+tx, 1}}*z0],
				Line[{{-tx, 1}, {+tx, 1}}*z1],
				Opacity[1/4],
				Line[{{-tx, 1}, {+tx, 1}}*PerspectiveInterp[z0, z1, 1/6, ze]],
				Line[{{-tx, 1}, {+tx, 1}}*PerspectiveInterp[z0, z1, 2/6, ze]],
				Line[{{-tx, 1}, {+tx, 1}}*PerspectiveInterp[z0, z1, 3/6, ze]],
				Line[{{-tx, 1}, {+tx, 1}}*PerspectiveInterp[z0, z1, 4/6, ze]],
				Line[{{-tx, 1}, {+tx, 1}}*PerspectiveInterp[z0, z1, 5/6, ze]]
			}]
		]
	}]
}]
*/

/*
(*utilities*)
MulVec[arr_, vec_] := arr*Table[vec, {Length[arr]}];
Loop[pts_] := Line[Join[pts, {pts[[1]]}]];
UniformAngles[n_, da_, spread_] := Block[{a}, a = Table[0, {n}];
   For[i = 2, i <= n, i++, a[[i]] = a[[i - 1]] + da*Cos[a[[i - 1]]]];
   a*((Pi/2)/a[[n]])^spread];
UniformAngles2[n_, da_, spread_] := 
  Join[-Reverse[UniformAngles[(n + 1)/2, da, spread]], 
   Part[UniformAngles[(n + 1)/2, da, spread], 2 ;; ((n + 1)/2)]];

DrawSphere[slices_, stacks_] := Block[{xy1, rrz},
   xy1 = N[Table[{Sin[2*Pi*t/slices], Cos[2*Pi*t/slices], 1}, {t, 0, slices - 1}]];
   rrz = N[Table[{Sin[Pi*v/stacks], Sin[Pi*v/stacks], Cos[Pi*v/stacks]}, {v, 0, stacks}]];
   Graphics3D[Join[
     Table[Line[MulVec[rrz, xy1[[i]]]], {i, 1, Length[xy1] - 0}],
     Table[Loop[MulVec[xy1, rrz[[i]]]], {i, 2, Length[rrz] - 1}]
     ]]
   ];

DrawSphere[slices_, stacks_] := Block[{a, xy1, rrz},
   (*may need a correction factor of 0.7 .. 0.9 in da*)
   a = N[UniformAngles[stacks/2 + 1, 2*Pi/slices, 0]];
   xy1 = N[Table[{Cos[2*Pi*t/slices], Sin[2*Pi*t/slices], 1}, {t, 0, slices - 1}]];
   rrz = N[Table[{Cos[a[[i + 1]]], Cos[a[[i + 1]]], Sin[a[[i + 1]]]}, {i, 0, stacks/2}]];
   Graphics3D[Join[
     Table[Line[MulVec[rrz, xy1[[i]]]], {i, 1, Length[xy1]}],
     Table[Loop[MulVec[xy1, rrz[[i]]]], {i, 1, Length[rrz]}]
     ]]
   ];
   
DrawSphere[slices_, stacks_] := Block[{a, xy1, rrz},
	a = N[UniformAngles2[stacks + 1, 0.7*2*Pi/slices, 1]];
	xy1 = N[Table[{Cos[2*Pi*t/slices], Sin[2*Pi*t/slices], 1}, {t, 0, slices - 1}]];
	rrz = N[Table[{Cos[a[[i + stacks/2 + 1]]], 
       Cos[a[[i + stacks/2 + 1]]], 
       Sin[a[[i + stacks/2 + 1]]]}, {i, -stacks/2, stacks/2}]];
   Graphics3D[Join[
     Table[Line[MulVec[rrz, xy1[[i]]]], {i, 1, Length[xy1]}], 
     Table[Loop[MulVec[xy1, rrz[[i]]]], {i, 1, Length[rrz]}]]]];
*/

#endif // __BANK
