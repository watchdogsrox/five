// ==============================
// debug/DebugDraw/DebugVolume2.h
// (c) 2011 RockstarNorth
// ==============================

#ifndef _DEBUG_DEBUGDRAW_DEBUGVOLUME2_H_
#define _DEBUG_DEBUGDRAW_DEBUGVOLUME2_H_

#if __BANK

#include "data/base.h"
#include "atl/array.h"
#include "vector/color32.h"
#include "vectormath/classes.h"

#include "fwutil/xmacro.h"

namespace rage { class grcViewport; }

enum eDVPass
{
	DVP_BACK_SURFS,
	DVP_BACK_LINES,
	DVP_BACK_EDGES,
	DVP_FRONT_SURFS,
	DVP_FRONT_LINES,
	DVP_FRONT_EDGES,
	DVP_SILHOUETTE_EDGES,
	DVP_COUNT
};

class CDVColourStyle
{
public:
	CDVColourStyle() {}
	CDVColourStyle(const CDVColourStyle& parent, float opacity);

	void InitWidgets(bkBank& bk);

	Color32 m_colours[DVP_COUNT]; // for now, just a simple colour per pass
};

#define FOREACH_DEF_CDVGeomParams(DEF) \
	DEF(Box       ) \
	DEF(BoxProj   ) \
	DEF(Frustum   ) \
	DEF(Sphere    ) \
	DEF(Hemisphere) \
	DEF(Cylinder  ) \
	DEF(Cone      ) \
	DEF(Capsule   ) \
	// end.

// forward declare classes
#define DEF_CDVGeomParams(type) class CDVGeomParams##type;
FOREACH(DEF_CDVGeomParams)
#undef  DEF_CDVGeomParams

#if __DEV
class CDVGeomTest;
#endif // __DEV

class CDVGeomParamsBox
{
public:
	CDVGeomParamsBox() {}
	CDVGeomParamsBox(
		Vec3V_In boxMin , // 
		Vec3V_In boxMax , // 
		int      stacksX, // 
		int      stacksY, // 
		int      stacksZ  // 
	)
		: m_boxMin (boxMin )
		, m_boxMax (boxMax )
		, m_stacksX(stacksX)
		, m_stacksY(stacksY)
		, m_stacksZ(stacksZ)
	{}            

	Vec3V m_boxMin ;
	Vec3V m_boxMax ;
	int   m_stacksX;
	int   m_stacksY;
	int   m_stacksZ;

	DEV_ONLY(void InitWidgets(bkBank& bk, CDVGeomTest* test));
	void Draw(const CDVColourStyle* colourStyle, const Color32& colour, float opacity, Mat34V_In basis, Vec3V_In camPos, bool fw) const;
};

class CDVGeomParamsBoxProj
{
public:
	CDVGeomParamsBoxProj() {}
	CDVGeomParamsBoxProj(
		Vec3V_In   boxMin     , // 
		Vec3V_In   boxMax     , // 
		int        stacksX    , // 
		int        stacksY    , // 
		int        stacksZ    , // 
		Vec3V_In   proj       , // 
		ScalarV_In perspective  // 
	)
		: m_boxMin     (boxMin     )
		, m_boxMax     (boxMax     )
		, m_stacksX    (stacksX    )
		, m_stacksY    (stacksY    )
		, m_stacksZ    (stacksZ    )
		, m_proj       (proj       )
		, m_perspective(perspective)
	{}            

	Vec3V   m_boxMin     ;
	Vec3V   m_boxMax     ;
	int     m_stacksX    ;
	int     m_stacksY    ;
	int     m_stacksZ    ;
	Vec3V   m_proj       ;
	ScalarV m_perspective;

	DEV_ONLY(void InitWidgets(bkBank& bk, CDVGeomTest* test));
	void Draw(const CDVColourStyle* colourStyle, const Color32& colour, float opacity, Mat34V_In basis, Vec3V_In camPos, bool fw) const;
};

class CDVGeomParamsFrustum
{
public:
	CDVGeomParamsFrustum() {}
	CDVGeomParamsFrustum(
		ScalarV_In tanHFOV    , // 
		ScalarV_In tanVFOV    , // 
		ScalarV_In z0         , // 
		ScalarV_In z1         , // 
		int        stacksX    , // 
		int        stacksY    , // 
		int        stacksZ    , // 
		ScalarV_In perspective  // 
	)
		: m_tanHFOV    (tanHFOV    )
		, m_tanVFOV    (tanVFOV    )
		, m_z0         (z0         )
		, m_z1         (z1         )
		, m_stacksX    (stacksX    )
		, m_stacksY    (stacksY    )
		, m_stacksZ    (stacksZ    )
		, m_perspective(perspective)
	{}            

	ScalarV m_tanHFOV    ;
	ScalarV m_tanVFOV    ;
	ScalarV m_z0         ;
	ScalarV m_z1         ;
	int     m_stacksX    ;
	int     m_stacksY    ;
	int     m_stacksZ    ;
	ScalarV m_perspective;

	DEV_ONLY(void InitWidgets(bkBank& bk, CDVGeomTest* test));
	void Draw(const CDVColourStyle* colourStyle, const Color32& colour, float opacity, Mat34V_In basis, Vec3V_In camPos, bool fw) const;
};

class CDVGeomParamsSphere
{
public:
	CDVGeomParamsSphere() {}
	CDVGeomParamsSphere(
		ScalarV_In radius    , // 
		int        slices    , // 
		int        stacks    , // 
		int        slicesTess, // 
		int        stacksTess  //
	)
		: m_radius    (radius    )
		, m_slices    (slices    )
		, m_stacks    (stacks    )
		, m_slicesTess(slicesTess)
		, m_stacksTess(stacksTess)
	{}

	ScalarV m_radius    ;
	int     m_slices    ;
	int     m_stacks    ;
	int     m_slicesTess;
	int     m_stacksTess;

	DEV_ONLY(void InitWidgets(bkBank& bk, CDVGeomTest* test));
	void Draw(const CDVColourStyle* colourStyle, const Color32& colour, float opacity, Mat34V_In basis, Vec3V_In camPos, bool fw) const;
};

class CDVGeomParamsHemisphere
{
public:
	CDVGeomParamsHemisphere() {}
	CDVGeomParamsHemisphere(
		ScalarV_In radius    , // 
		int        slices    , // 
		int        stacks    , // 
		int        slicesTess, // 
		int        stacksTess, //
		bool       faceGrid    // 
	)
		: m_radius    (radius    )
		, m_slices    (slices    )
		, m_stacks    (stacks    )
		, m_slicesTess(slicesTess)
		, m_stacksTess(stacksTess)
		, m_faceGrid  (faceGrid  )
	{}

	ScalarV m_radius    ;
	int     m_slices    ;
	int     m_stacks    ;
	int     m_slicesTess;
	int     m_stacksTess;
	bool    m_faceGrid  ;

	DEV_ONLY(void InitWidgets(bkBank& bk, CDVGeomTest* test));
	void Draw(const CDVColourStyle* colourStyle, const Color32& colour, float opacity, Mat34V_In basis, Vec3V_In camPos, bool fw) const;
};

class CDVGeomParamsCylinder
{
public:
	CDVGeomParamsCylinder() {}
	CDVGeomParamsCylinder(
		ScalarV_In radius0   , // 
		ScalarV_In radius1   , // 
		ScalarV_In z0        , // 
		ScalarV_In z1        , // 
		int        slices    , // 
		int        stacks    , // 
		int        slicesTess, // 
		int        stacksTess, //
		bool       faceGrid    // 
	)
		: m_radius0   (radius0   )
		, m_radius1   (radius1   )
		, m_z0        (z0        )
		, m_z1        (z1        )
		, m_slices    (slices    )
		, m_stacks    (stacks    )
		, m_slicesTess(slicesTess)
		, m_stacksTess(stacksTess)
		, m_faceGrid  (faceGrid  )
	{}

	ScalarV m_radius0   ;
	ScalarV m_radius1   ;
	ScalarV m_z0        ;
	ScalarV m_z1        ;
	int     m_slices    ;
	int     m_stacks    ;
	int     m_slicesTess;
	int     m_stacksTess;
	bool    m_faceGrid  ;

	DEV_ONLY(void InitWidgets(bkBank& bk, CDVGeomTest* test));
	void Draw(const CDVColourStyle* colourStyle, const Color32& colour, float opacity, Mat34V_In basis, Vec3V_In camPos, bool fw) const;
};

class CDVGeomParamsCone
{
public:
	CDVGeomParamsCone() {}
	CDVGeomParamsCone(
		ScalarV_In radius      , // along z axis
		ScalarV_In tanTheta    , // tan(angle/2)
		int        slices      , // 
		int        stacks      , // 
		int        slicesTess  , // 
		int        stacksTess  , //
		ScalarV_In capCurvature, // 0 for flat cone, 1 for spherical cone
		int        capStacks   , // 0 for no cap (flat)
		ScalarV_In perspective , // 0 for linear stacks, try something like 0.8
		bool       faceGrid      // 
	)
		: m_radius      (radius      )
		, m_tanTheta    (tanTheta    )
		, m_slices      (slices      )
		, m_stacks      (stacks      )
		, m_slicesTess  (slicesTess  )
		, m_stacksTess  (stacksTess  )
		, m_capCurvature(capCurvature)
		, m_capStacks   (capStacks   )
		, m_perspective (perspective )
		, m_faceGrid    (faceGrid    )
	{}

	ScalarV m_radius      ;
	ScalarV m_tanTheta    ;
	int     m_slices      ;
	int     m_stacks      ;
	int     m_slicesTess  ;
	int     m_stacksTess  ;
	ScalarV m_capCurvature;
	int     m_capStacks   ;
	ScalarV m_perspective ;
    bool    m_faceGrid    ;

	DEV_ONLY(void InitWidgets(bkBank& bk, CDVGeomTest* test));
	void Draw(const CDVColourStyle* colourStyle, const Color32& colour, float opacity, Mat34V_In basis, Vec3V_In camPos, bool fw) const;
};

class CDVGeomParamsCapsule
{
public:
	CDVGeomParamsCapsule() {}
	CDVGeomParamsCapsule(
		ScalarV_In z0        , // 
		ScalarV_In z1        , // 
		ScalarV_In radius    , // 
		int        slices    , // 
		int        stacks    , // 
		int        slicesTess, // 
		int        stacksTess, //
		int        capStacks   // 
	)
		: m_z0        (z0          )
		, m_z1        (z1          )
		, m_radius    (radius      )
		, m_slices    (slices      )
		, m_stacks    (stacks      )
		, m_slicesTess(slicesTess  )
		, m_stacksTess(stacksTess  )
		, m_capStacks (capStacks   )
	{}

	ScalarV m_z0        ;
	ScalarV m_z1        ;
	ScalarV m_radius    ;
	int     m_slices    ;
	int     m_stacks    ;
	int     m_slicesTess;
	int     m_stacksTess;
	int     m_capStacks ;

	DEV_ONLY(void InitWidgets(bkBank& bk, CDVGeomTest* test));
	void Draw(const CDVColourStyle* colourStyle, const Color32& colour, float opacity, Mat34V_In basis, Vec3V_In camPos, bool fw) const;
};

// ================================================================================================

class CDVGeomProxy
{
public:
	explicit CDVGeomProxy() {}
	virtual ~CDVGeomProxy() {}
	CDVGeomProxy(const CDVColourStyle* colourStyle, const Color32& colour, Mat34V_In basis) : m_colourStyle(colourStyle), m_colour(colour), m_basis(basis), m_distance(0.0f) {}

	virtual Vec3V_Out GetCentroid() const { return m_basis.d(); }
	virtual void Draw(Vec3V_In camPos, float opacity, bool fw) const = 0;

	static s32 CompareFunc   (CDVGeomProxy  const* a, CDVGeomProxy  const* b) { return (int)(32.0f*(b->m_distance - a->m_distance)); }
	static s32 CompareFuncPtr(CDVGeomProxy* const* a, CDVGeomProxy* const* b) { return CompareFunc(*a, *b); }

	const CDVColourStyle* m_colourStyle;
	Color32               m_colour; // used if m_colourStyle is NULL
	Mat34V                m_basis;
	float                 m_distance;
};

class CDVGeomGroup
{
public:
	void AddProxy(CDVGeomProxy* proxy);
	void Sort(Vec3V_In camPos, Vec3V_In camDir = Vec3V(V_ZERO));
	void Draw(Vec3V_In camPos, float opacity, float distanceMin, float distanceMax, bool fw) const;
	void Clear();

	atArray<CDVGeomProxy*> m_proxies;
};

// ================================================================================================

#if __DEV

class CDVGeomTestInterface
{
public:
	static CDVGeomTestInterface& Get();

	void Init();
	void InitWidgets();

	void Update(const grcViewport* vp);
};

#endif // __DEV

// ================================================================================================
// ================================================================================================
// ================================================================================================

namespace gtaDebugDraw {

void            InitColourStyle();
CDVColourStyle* PushColourStyle(); // returns copy of previous colourstyle
void            PopColourStyle();

#define CDV_DEFAULT_SLICES 24
#define CDV_DEFAULT_STACKS 18

void PrettySphere(Vec4V_In centreAndRadius, int slices = CDV_DEFAULT_SLICES, int stacks = CDV_DEFAULT_STACKS, float opacity = 1.0f);
void ColourSphere(Vec4V_In centreAndRadius, int slices = CDV_DEFAULT_SLICES, int stacks = CDV_DEFAULT_STACKS, const Color32& colour = Color32(255,0,0,255), float opacity = 1.0f);

inline void PrettySphere(Vec3V_In centre, float radius, int slices = CDV_DEFAULT_SLICES, int stacks = CDV_DEFAULT_STACKS, float opacity = 1.0f)
{
	PrettySphere(Vec4V(centre, ScalarV(radius)), slices, stacks, opacity);
}

inline void ColourSphere(Vec3V_In centre, float radius, int slices = CDV_DEFAULT_SLICES, int stacks = CDV_DEFAULT_STACKS, const Color32& colour = Color32(255,0,0,255), float opacity = 1.0f)
{
	ColourSphere(Vec4V(centre, ScalarV(radius)), slices, stacks, colour, opacity);
}

} // namespace gtaDebugDraw

#endif // __BANK
#endif // _DEBUG_DEBUGDRAW_DEBUGVOLUME2_H_
