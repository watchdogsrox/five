//
//	CPlantsMgr - management of plants seeded around camera;
//
//	24/06/2005	-	Andrzej:	- initial conversion from SA code;
//	14/07/2005	-	Andrzej:	- full working port to Rage;
//
//
//
#ifndef __CPLANTSMGR_H__
#define __CPLANTSMGR_H__


//
// PSN: if 1, then SPU renderer is used:
//
#define PSN_PLANTSMGR_SPU_RENDER			(1 && (__PPU || __SPU))
#define PSN_PLANTSMGR_SPU_UPDATE			(1 && (__PPU || __SPU))
#define PLANTSMGR_DATA_EDITOR				(__BANK)

#define PLANTSMGR_MULTI_UPDATE				(1 && (RSG_PC || RSG_DURANGO || RSG_ORBIS))	// NG: Update task split into subtasks
#define PLANTSMGR_MULTI_RENDER				(0 && (RSG_PC || RSG_DURANGO || RSG_ORBIS))	// NG: Render task split into subtasks

#if !__SPU
#include "render_channel.h"
	RAGE_DECLARE_SUBCHANNEL(render, plants)

	#define plantsAssertf(cond,fmt,...)			RAGE_ASSERTF(render_plants,cond,fmt,##__VA_ARGS__)
	#define plantsAssert(cond)					RAGE_ASSERT(render_plants,cond)
	#define plantsVerifyf(cond,fmt,...)			RAGE_VERIFYF(render_plants,cond,fmt,##__VA_ARGS__)
	#define plantsErrorf(fmt,...)				RAGE_ERRORF(render_plants,fmt,##__VA_ARGS__)
	#define plantsWarningf(fmt,...)				RAGE_WARNINGF(render_plants,fmt,##__VA_ARGS__)
	#define plantsDisplayf(fmt,...)				RAGE_DISPLAYF(render_plants,fmt,##__VA_ARGS__)
	#define plantsDebugf1(fmt,...)				RAGE_DEBUGF1(render_plants,fmt,##__VA_ARGS__)
	#define plantsDebugf2(fmt,...)				RAGE_DEBUGF2(render_plants,fmt,##__VA_ARGS__)
	#define plantsDebugf3(fmt,...)				RAGE_DEBUGF3(render_plants,fmt,##__VA_ARGS__)
#endif //!__SPU...

// rage includes:
#include "grcore/effect_config.h"
#include "data/resourceheader.h"			// g_rscVirtualLeafSize
#include "spatialdata/transposedplaneset.h"	// spdTransposedPlaneSet8
#include "system/memory.h"
#include "system/criticalsection.h"
#include "atl/array.h"

// framework headers:

// game includes:
#if !__SPU
#include "fwscene/stores/staticboundsstore.h"	// g_StaticBoundsStore
#include "scene/entity.h"
#endif
#include "scene/RegdRefTypes.h"
#include "tools/SectorToolsParam.h"
#include "renderer/PlantsGrassRendererSwitches.h"

// forward definitions:
namespace rage {
	class Vector2;
	class Vector3;
	class Vector4;
	class Color32;
	class phPolygon;
	class phBound;
	class phBoundGeometry;
	class grcTexture;
	class grmGeometry;
}
class CEntity;
class gtaDrawable;
class CPlantLocTriArray;

#define FURGRASS_TEST_V4					(1 && (RSG_PC || RSG_DURANGO || RSG_ORBIS))	// fur grass LOD v4
#define FURGRASS_MAX_LAYERS					(8)

#define CPLANT_STORE_LOCTRI_NORMAL			(0)	// WIP: store LocTri normal
#define CPLANT_WRITE_GRASS_NORMAL			(0)
#define CPLANT_PRE_MULTIPLY_LIGHTING        (1)
#define CPLANT_BLIT_MIN_GBUFFER             (__PS3) // Bind min number of GBuffer targets when doing the fakeBlit (mutually exclusive with CPLANT_STORE_LOCTRI_NORMAL)
#define CPLANT_USE_OCCLUSION				(0)

#define CPLANT_CLIP_EDGE_VERT				(0)
#define CPLANT_DYNAMIC_CULL_SPHERES			(0)

#define CPLANT_CULLSPHERES_MAX				(8)	// amount of cullspheres supported

//
//
// max poly points in phPolygon (taken from phBound, etc.):
//
#define CPLANT_MAX_POLY_POINTS				(POLY_MAX_VERTICES)

//
// num of models per slot:
//
#if __PS3 || __XENON
#define CPLANT_SLOT_NUM_MODELS				(16)
#else // __PS3 || __XENON
#define CPLANT_SLOT_NUM_MODELS				(32)
#endif // __PS3 || __XENON

//
// num of textures per slot:
//
#define CPLANT_SLOT_NUM_TEXTURES			(32)


#define CPLANT_INCREASE_DIST				(80.0f)


// LOD0:
#define CPLANT_LOD0_ALPHA_CLOSE_DIST		(22.0f)		// LOD0: fade start range
#define CPLANT_LOD0_ALPHA_FAR_DIST			(32.0f)		// LOD0: fade stop range
#define CPLANT_LOD0_FAR_DIST				(32.0f)		// LOD0: where LOD0 is culled

// LOD1:
#define CPLANT_LOD1_CLOSE_DIST				(15.0f)		// LOD1: where LOD1 starts

#define CPLANT_LOD1_ALPHA0_CLOSE_DIST		(15.0f)		// LOD1: alpha0 fade start
#define CPLANT_LOD1_ALPHA0_FAR_DIST			(25.0f)		// LOD1: alpha0 fade stop

#define CPLANT_LOD1_ALPHA1_CLOSE_DIST		(40.0f)		// LOD1: alpha1 fade start
#define CPLANT_LOD1_ALPHA1_FAR_DIST			(55.0f)		// LOD1: alpha1 fade stop

#define CPLANT_LOD1_FAR_DIST				(55.0f)		// LOD1: where LOD1 is culled


// LOD2:
#define CPLANT_LOD2_CLOSE_DIST				(30.0f)		// LOD2: where LOD1 starts

#define CPLANT_LOD2_ALPHA0_CLOSE_DIST		(30.0f)		// LOD2: alpha0 fade start
#define CPLANT_LOD2_ALPHA0_FAR_DIST			(45.0f)		// LOD2: alpha0 fade stop

#define CPLANT_LOD2_ALPHA1_CLOSE_DIST		(65.0f )		// LOD2: alpha1 fade start
#define CPLANT_LOD2_ALPHA1_FAR_DIST			(85.0f )		// LOD2: alpha1 fade stop

#define CPLANT_LOD2_ALPHA1_CLOSE_DIST_FAR	(65.0f+CPLANT_INCREASE_DIST)	// LOD2: alpha1 fade start
#define CPLANT_LOD2_ALPHA1_FAR_DIST_FAR		(85.0f+CPLANT_INCREASE_DIST)	// LOD2: alpha1 fade stop

#define CPLANT_LOD2_FAR_DIST				(85.0f+CPLANT_INCREASE_DIST)	// LOD2: where LOD1 is culled


#if PLANTS_USE_LOD_SETTINGS
	//
	// radius of test sphere used to detect instances located close enough:
	//
	#define CPLANTMGR_COL_TEST_RADIUS_INITVAL		(80.0f  + CPLANT_INCREASE_DIST)
	#define CPLANTMGR_COL_TEST_RADIUS				(80.0f  + CPLANT_INCREASE_DIST)

	//
	// distance where TriLocs are considered to be used as base for plant sectors:
	//
	#define CPLANT_TRILOC_FAR_DIST					(CPLANT_LOD2_FAR_DIST)
	#define CPLANT_TRILOC_FAR_DIST_SQR				(CPLANT_TRILOC_FAR_DIST*CPLANT_TRILOC_FAR_DIST)

	#define CPLANT_TRILOC_SHORT_FAR_DIST			(CPLANT_TRILOC_FAR_DIST - CPLANT_INCREASE_DIST)
	#define CPLANT_TRILOC_SHORT_FAR_DIST_SQR		(CPLANT_TRILOC_SHORT_FAR_DIST*CPLANT_TRILOC_SHORT_FAR_DIST)

	#define CPLANT_TRILOC_FAR_DIST_INITVAL			(85.0f + CPLANT_INCREASE_DIST)
	#define CPLANT_TRILOC_SHORT_FAR_DIST_INITVAL	(CPLANT_TRILOC_FAR_DIST_INITVAL - CPLANT_INCREASE_DIST)

#elif __BANK
	//
	// radius of test sphere used to detect instances located close enough:
	//
	#define CPLANTMGR_COL_TEST_RADIUS_INITVAL		(80.0f  + CPLANT_INCREASE_DIST)
	#define CPLANTMGR_COL_TEST_RADIUS				(CPlantMgr::ms_bkColTestRadius)

	//
	// distance where TriLocs are considered to be used as base for plant sectors:
	//
	#define CPLANT_TRILOC_FAR_DIST_INITVAL			(85.0f + CPLANT_INCREASE_DIST)
	#if __SPU
		#define CPLANT_TRILOC_FAR_DIST_SQR			(g_jobParams->m_bkTriLocFarDistSqr)
	#else
		#define CPLANT_TRILOC_FAR_DIST				(CPlantMgr::ms_bkTriLocFarDist)
		#define CPLANT_TRILOC_FAR_DIST_SQR			(CPlantMgr::ms_bkTriLocFarDistSqr)
	#endif

	#define CPLANT_TRILOC_SHORT_FAR_DIST_INITVAL	(CPLANT_TRILOC_FAR_DIST_INITVAL - CPLANT_INCREASE_DIST)
	#if __SPU
		#define CPLANT_TRILOC_SHORT_FAR_DIST_SQR	(g_jobParams->m_bkTriLocShortFarDistSqr)
	#else
		#define CPLANT_TRILOC_SHORT_FAR_DIST		(CPlantMgr::ms_bkTriLocShortFarDist)
		#define CPLANT_TRILOC_SHORT_FAR_DIST_SQR	(CPlantMgr::ms_bkTriLocShortFarDistSqr)
	#endif
#else
	//
	// radius of test sphere used to detect instances located close enough:
	//
	#define CPLANTMGR_COL_TEST_RADIUS				(80.0f  + CPLANT_INCREASE_DIST)

	//
	// distance where TriLocs are considered to be used as base for plant sectors:
	//
	#define CPLANT_TRILOC_FAR_DIST					(85.0f + CPLANT_INCREASE_DIST)
	#define CPLANT_TRILOC_FAR_DIST_SQR				(CPLANT_TRILOC_FAR_DIST*CPLANT_TRILOC_FAR_DIST)

	#define CPLANT_TRILOC_SHORT_FAR_DIST			(CPLANT_TRILOC_FAR_DIST - CPLANT_INCREASE_DIST)
	#define CPLANT_TRILOC_SHORT_FAR_DIST_SQR		(CPLANT_TRILOC_SHORT_FAR_DIST*CPLANT_TRILOC_SHORT_FAR_DIST)
#endif //!__BANK...


#define CPLANT_GROUND_SLOPE_ANGLE_MIN				(4)	// ground slope must be more than 4degs for grass to be skewed

	extern bool gbPlantMgrActive;	// JB : I need to be able to turn the plantmgr off when exporting navmesh data

//
//
// number of entities hold in our internal CEntity cache:
//
#define CPLANT_COL_ENTITY_CACHE_SIZE			(40)


//
//
// update cache every 32th frame (must be power-of-2):
//
#define CPLANT_COL_ENTITY_UPDATE_CACHE			(32)


//
//
// amount of parallel lists:
//
#if RSG_PC || RSG_DURANGO || RSG_ORBIS
	#ifdef TERRAIN_BAKE_TOOL
		#define CPLANT_LOC_TRIS_LIST_NUM				(32)
	#else
		#define CPLANT_LOC_TRIS_LIST_NUM				(8)
	#endif // TERRAIN_BAKE_TOOL
#else
	#define CPLANT_LOC_TRIS_LIST_NUM					(4)
#endif

#define PLANTSMGR_MULTI_UPDATE_TASKCOUNT				(CPLANT_LOC_TRIS_LIST_NUM)	// NG: amount of parallel update tasks
#define PLANTSMGR_MULTI_RENDER_TASKCOUNT				(2)	//(4)					// NG: amount of parallel render tasks

//
//
// max number of triangles kept in loc triangle cache:
//
#if RSG_PC || RSG_DURANGO || RSG_ORBIS
	#define CPLANT_MAX_CACHE_LOC_TRIS_NUM			(682*4)
#else
	#define CPLANT_MAX_CACHE_LOC_TRIS_NUM			(682)
#endif

//
// process every 8th TriLoc (must be power-of-2):
//
#define CPLANT_ENTRY_TRILOC_PROCESS_UPDATE		(8)

//
// unique mask value to mark case, when we want all LocTris to be processed:
//
#define CPLANT_ENTRY_TRILOC_PROCESS_ALWAYS		(0x7A7A7A7A)	// changed as it is compared with a signed value

//
// use 2D (rather than 3D) collision calculations for LocTris:
//
#define CPLANT_USE_COLLISION_2D_DIST			(1)

// per-vertex scaleXYZ, scaleZ and Density:
#define CPLANT_PV_DEFAULT_SCALEXYZ			(0x08)	// scaleXYZ:0x08 maps to 0.06666 ("positive zero")
#define	CPLANT_PV_DEFAULT_SCALEZ			(0x03)	// scaleZ:	0x03 maps to "no scale Z"
#define CPLANT_PV_DEFAULT_DENSITY			(0x03)	// density:	0x03 maps to "no density"
#define CPLANT_PV_DEFAULT_D_SZ_SXYZ			((CPLANT_PV_DEFAULT_DENSITY<<6) | (CPLANT_PV_DEFAULT_SCALEZ<<4) | CPLANT_PV_DEFAULT_SCALEXYZ)
CompileTimeAssert(CPLANT_PV_DEFAULT_D_SZ_SXYZ==0xf8);

#if CPLANT_DYNAMIC_CULL_SPHERES
	#define PLANTSMGR_MAX_DYNAMIC_CULL_SPHERES					(10)
	typedef atFixedArray<spdSphere, PLANTSMGR_MAX_DYNAMIC_CULL_SPHERES> PlantMgr_DynCullSphereArray;
	#define PLANTSMGR_INVALID_DYNAMIC_CULL_SPHERE_INDEX			(static_cast<u32>(-1))
#endif

#if PSN_PLANTSMGR_SPU_RENDER
	#include "PlantsGrassRendererSPU.h"
#endif


//
//
// available for both SPU and non-SPU rendering:
//
struct grassModel
{
	grassModel()						{ sysMemSet(this, 0x00, sizeof(grassModel)); }

	gtaDrawable		*m_pDrawable;		// higher level drawable, used only for memory management
	grmGeometry		*m_pGeometry;		// plant geometry
	Vector2			m_dimensionLOD2;	// dimensions (w&h) of LOD2 (stored in LOD1)

	Vector4			m_BoundingSphere;	// model bound sphere

#if PSN_PLANTSMGR_SPU_RENDER
	u32				m_IdxOffset;
	u16				m_IdxLocation;		// LOCATION_LOCAL, LOCATION_MAIN
	u16				m_IdxCount;
	u16				m_DrawMode;			// CELL_GCM_PRIMITIVE_TRIANGLES
	u16				m_pad0;
	u32				m_GeometryCmdOffset;// offset to geometry commands	

	#define PSN_SIZEOF_GRASSMODEL		(48)
#endif //PSN_PLANTSMGR_SPU_RENDER...
};


//
// 
// "Plant Location" triangle:
// it's generated from ColModel data;
//
class CPlantLocTri
{
	friend class CPlantMgr;
	friend class CPlantLocTriArray;
#if PLANTSMGR_DATA_EDITOR
	friend class CPlantMgrDataEditor;
#endif

public:
	CPlantLocTri()		{ }
	~CPlantLocTri()		{ }

public:
	CPlantLocTri*		Add(CPlantLocTriArray& triTab, const Vector3& v1, const Vector3& v2, const Vector3& v3, phMaterialMgr::Id nSurfaceType, bool bCreatesPlants, bool bCreatesObjects, CEntity* pParentEntity, u16 nColEntIdx, bool IsFarDrawTri);
	void				Release(CPlantLocTriArray& triTab);

public:
	inline static u8	pv8PackDensityScaleZScaleXYZ(u8 density, u8 scaleZ, u8 scaleXYZ);
	inline static u8	pv8UnpackScaleXYZ(u8 densityAndScale8);
	inline static u8	pv8UnpackScaleZ(u8 densityAndScale8);
	inline static u8	pv8UnpackDensity(u8 densityAndScale8);
	inline static float pv8MapScaleXYZ(u8 scaleXYZ);
	inline static float	pv8MapScaleZ(u8 scaleZ);
	inline static float	pv8MapDensity(u8 density);
	inline static float pv8UnpackAndMapScaleXYZ(u8 densityAndScale8);
	inline static float	pv8UnpackAndMapScaleZ(u8 densityAndScale8);
	inline static float	pv8UnpackAndMapDensity(u8 densityAndScale8);
private:
	static float		ms_pvMapScaleZ[4];
	static float		ms_pvMapDensity[4];

public:
	inline Vector3		GetCenter() const;

	inline Vector3		GetV1() const			{ return m_V1.GetVector3(); }
	inline Vector3		GetV2() const			{ return m_V2.GetVector3(); }
	inline Vector3		GetV3() const			{ return m_V3.GetVector3(); }

	inline float		GetSphereRadius() const	{ return m_V1.w;			}
	inline float		GetTriArea() const		{ return m_V2.w;			}
	inline int			GetSeed() const			{ return m_V3.iw;			}

	float				CalcArea();
	static bool			IsPtInTriangle2D(float x, float y, const Vector3& v1, const Vector3& v2, const Vector3& v3, const Vector3& normal, float* z);
	
private:
	// 3 vertices of triangle
	Vector4				m_V1;	// W = SphereRadius
	Vector4				m_V2;	// W = tri Area
	Vector4				m_V3;	// W = Seed	

public:
	inline void			SetSphereRadius(float v)	{ m_V1.w = v; }
	inline void			SetTriArea(float v)			{ m_V2.w = v; }
	inline void			SetSeed(int v)				{ m_V3.iw= v; }


public:

#if __DEV || SECTOR_TOOLS_EXPORT
	u32					m_nMaxNumPlants;	// debug info only: max number of plants to generate for this TriLoc
#endif

	// local per-vertex variation data: ground color(RGB) + density/scaleZ/ScaleXYZ(A):
	Color32				m_GroundColorV1, m_GroundColorV2, m_GroundColorV3;

	// 64
	phMaterialMgr::Id	m_nSurfaceType;									// copied from CColTriangle;

	Float16Vec4			m_skewAxisAngle;								// ground skewing: xyz = skewAxis, w=skewAngle (if 0 then no skewing)

	// 80
	CEntity				*m_pParentEntity;

private:
	u16					m_NextTri;
	u16					m_PrevTri;
public:

	s8					m_normal[3];		// packed loctri normal

	u8					m_nColEntIdx;		// idx of CPlantColBoundEntry this tri belongs to

	u8					m_nAmbientScale[2];	// natural/artificial AO

	bool				m_bCreatesPlants	: 1;
	bool				m_bCreatesObjects	: 1;
	bool				m_bCreatedObjects	: 1;
	bool				m_bDrawFarTri		: 1;
	bool				m_bCameraDontCull	: 1;
	bool				m_bUnderwater		: 1;
	bool				m_bGroundScale1Vert	: 1;
	bool				m_bRequireAmbScale	: 1;	// requires TC ambient scale resample

#if CPLANT_USE_OCCLUSION
	bool				m_bOccluded			: 1;
	bool				m_bNeedsAABB		: 1;
	u8					m_pad3				: 6;
#elif CPLANT_CLIP_EDGE_VERT
	bool				m_ClipEdge_01		:1;
	bool				m_ClipEdge_12		:1;
	bool				m_ClipEdge_20		:1;
	bool				m_ClipVert_0		:1;
	bool				m_ClipVert_1		:1;
	bool				m_ClipVert_2		:1;
	u8					m_pad3				:2;
#endif

#if __PPU || __SPU
	#define PSN_SIZEOF_CPLANTLOCTRI			(96)
#endif
};

//
//
// wrapper class around {listID:2, idx:14} as one u16:
//
struct CTriHashIdx16
{
private:
	union
	{
		u16		m_u16;
		struct
		{
		#if RSG_PC || RSG_DURANGO || RSG_ORBIS
			u16	m_idx	:13;	// hashed index: 0-8191
			u16	m_listID:3;		// listID: 0-7
		#else
			u16	m_idx	:14;	// hashed index: 0-16383
			u16	m_listID:2;		// listID: 0-3
		#endif
		} h;
	};

public:
	CTriHashIdx16()
	{
		// do nothing
	}

	CTriHashIdx16(u16 n)
	{
		m_u16 = n;
	}

public:
	// hash idx operators:
	inline void Make(u16 listID, u16 idx)
	{
	#if RSG_PC || RSG_DURANGO || RSG_ORBIS
		FastAssert(listID	< 8);		// no more than 8 lists
		FastAssert(idx		< 0x1fff);	// no more than 8191 indices
	#else
		FastAssert(listID	< 4);		// no more than 4 lists
		FastAssert(idx		< 0x3fff);	// no more than 16383 indices
	#endif
		h.m_idx		= idx;
		h.m_listID	= listID;
	}

	inline u16 GetIdx() const
	{
		return h.m_idx;
	}

	inline u16 GetListID() const
	{
		return h.m_listID;
	}

public:
	// u16 operators:
	inline u16 operator=(const u16 n)
	{
		m_u16 = n;
		return(m_u16);
	}

	inline bool operator==(const u16 n)
	{
		return(m_u16==n);
	}

	inline operator u16()
	{
		return m_u16;
	}
};
CompileTimeAssert(sizeof(CTriHashIdx16)==sizeof(u16));


// fur grass render settings picked up from underlying entities:
struct CPlantColBoundEntryFurGrassInfo
{
	grcTexture*				m_furGrassDiffTex;
	grcTexture*				m_furGrassNormalTex;
	grcTexture*				m_furGrassComboH4Tex[4];

	u32						m_pad_u32;
	bool					m_bValid	:1;	// true if pickup fur grass settings contain valid entries
	u8						m_pad_u8a	:7;	
	u8						m_pad_u8[3];
};
CompileTimeAssertSize(CPlantColBoundEntryFurGrassInfo,32,56);	// must be dma'able

//
//
// collision bound entry in internal "entity" cache:
//
class CPlantColBoundEntry
{
	friend class CPlantMgr;
	friend class CPlantColBoundEntryArray;

public:
	CPlantColBoundEntry()	{ }
	~CPlantColBoundEntry()	{ }

public:
#if !__SPU
	CPlantColBoundEntry*	AddEntry(CEntity *pEntity, s32 parentIdx, s32 childIdx);
	void					ReleaseEntry();
#endif

	phInst*					GetPhysInst();
	phBoundGeometry*		GetBound();
#if !__SPU
	const Matrix34*			GetBoundMatrix();

	bool					IsParentValid() const
	{
		if(IsMatBound())
		{
			return g_StaticBoundsStore.GetPtr(strLocalIndex(m_nBParentIndex))!=NULL;
		}
		else
		{
			return (m__pEntity!=NULL) && (m__pEntity->GetCurrentPhysicsInst()!=NULL);
		}
	}
#endif

	bool					IsMatBound() const
	{
		return m_bMaterialBound;
	}

#if PLANTSMGR_MULTI_UPDATE
	void Lock()					{ m_csToken.Lock();				}
	bool TryLock()				{ return m_csToken.TryLock();	}
	void Unlock()				{ m_csToken.Unlock();			}
#else
	void Lock()					{ }
	bool TryLock()				{ }
	void Unlock()				{ }
#endif

private:
	Matrix34				m_BoundMat;				// +64: local copy of above matrix (sometimes phInst, which matrix we use, doesn't exist after some time)
#if FURGRASS_TEST_V4
	CPlantColBoundEntryFurGrassInfo	m_furInfo;
#endif
	atBitSet32				m_BoundMatProps;		// +8:	bitfield to store bCreatesPlants|bCreatesObjects material flags

	RegdEnt					m__pEntity;				// +4: valid only if m_bIsMatBound==false
	s32						m_nBParentIndex;		// +4: composite bound index in g_StaticBoundsStore (valid only if m_bMaterialBound==true)
	s32						m_nBChildIndex;			// +4: child index of above composite bound (valid only if m_bMaterialBound==true)

#if PLANTSMGR_MULTI_UPDATE
	sysCriticalSectionToken	m_csToken;				// entity access token during multi update
	atBitSet32				m_processedTris;		// caching table to mark processed tris, so subsequent tasks don't re-process them
#endif

	CTriHashIdx16*			m_LocTriArray;			// +4: array of size [phBound->GetNumPolygons()], holds pointers to CPlantLocTri for every phPolygon in the phBound;
	u16						m_nNumTris;				// +2: size of above array
	u16						m_nScancode;			// +2: scancode (timestamp) of current frame

	bool					m_bBoundMatIdentity	:1;	// +2: true if BoundMat is identity matrix (no transformation required for LocTris then)
	bool					m_bMaterialBound	:1;	// true=material bound (from g_StaticBoundsStore); false=entity based bound
	ATTR_UNUSED u16			m_pad00				:14;

	u16						m_NextEntry;			// +2
	u16						m_PrevEntry;			// +2
};

//
//
// handy wrapper around array of CPlantColBoundEntry to be accessed with hashed 'addresses'/indices (0=NULL, everything else is hashed index to array):
//
class CPlantColBoundEntryArray
{
public:
	CPlantColBoundEntryArray()	{}
	~CPlantColBoundEntryArray()	{}

public:
	void Init();

	// returns entry pointed by hashed index h:
	inline CPlantColBoundEntry& operator[](u16 h)
	{
		FastAssert((h > 0) && (h < (CPLANT_COL_ENTITY_CACHE_SIZE+1)));
		return m_tab[h-1];
	}
	
	// converts entry ptr inside m_tab into hashed index:
	inline u16 GetIdx(CPlantColBoundEntry* e)
	{
		const u32 h = ptrdiff_t_to_int(e - m_tab + 1);
		FastAssert((h > 0) && (h < (CPLANT_COL_ENTITY_CACHE_SIZE+1)));
		return (u16)h;
	}

private:
	CPlantColBoundEntry		m_tab[CPLANT_COL_ENTITY_CACHE_SIZE];

public:
	u16						m_UnusedListHead;
	u16						m_CloseListHead;
};

//
//
// handy wrapper around array of CPlantLocTri to be accessed with hashed 'addresses'/indices (0=NULL, everything else is hashed index to array):
//
class CPlantLocTriArray
{
public:
	CPlantLocTriArray()		{}
	~CPlantLocTriArray()	{}

public:
	void Init(u16 listID);

	// returns entry pointed by hashed index h:
	inline CPlantLocTri& operator[](u16 h)
	{
		FastAssert((h > 0) && (h < (CPLANT_MAX_CACHE_LOC_TRIS_NUM+1)));
		return m_tab[h-1];
	}
	
	// converts entry ptr inside m_tab into hashed index:
	inline u16 GetIdx(CPlantLocTri* e)
	{
		const u32 h = ptrdiff_t_to_int(e - m_tab + 1);
		FastAssert((h > 0) && (h < (CPLANT_MAX_CACHE_LOC_TRIS_NUM+1)));
		return (u16)h;
	}

private:
	CPlantLocTri		m_tab[CPLANT_MAX_CACHE_LOC_TRIS_NUM];

public:
	u16					m_CloseListHead;
	u16					m_UnusedListHead;
	u16					m_listID;	// 0-4

	bool				m_bRequireAmbScale	:1;	// any of tris requires TC ambient scale resample	
	u8					m_pad0				:7;
};
CompileTimeAssert(__64BIT || sizeof(CPlantLocTriArray) <= 64*1024);	// 64KB size limit to stay friendly for 4KB streaming chunk size

//
//
// CPlantMgrBase0: 1st part of data required by the SPU Update process
//
class CPlantMgrBase0
{
public:
	CPlantColBoundEntryArray	m_ColEntCache;
	Vector3						m_CameraPos;
};

class CPlantMgrBase : public CPlantMgrBase0
{
public:
#if __SPU
	CPlantLocTriArray			m_LocTrisTabSpu;	// SPU only
#endif
};
// base size for SPU update stack size
#define SPU_SIZEOF_CPLANTMGRBASE	(sizeof(CPlantLocTriArray)+sizeof(CPlantMgrBase))

struct ProcObjectCreationInfo;
struct ProcTriRemovalInfo;
//
//
//
//
struct spuPlantsMgrUpdateStruct
{
public:
	CPlantMgrBase*			m_pPlantsMgr;
	Vector3					m_camPos;
	Vector4					m_cullSphere[2];
	u32						m_iTriProcessSkipMask;
	s32						m_maxAdd;
	u32						m_addBufSize;
	ProcObjectCreationInfo* m_pAddList;
	s32						m_maxRemove;
	u32						m_removeBufSize;
	ProcTriRemovalInfo*		m_pRemoveList;
	float					m_GroundSlopeAngleMin;
	struct CResultSize
	{
		u32 m_numAdd;
		u32 m_numRemove;
	} ;
	CResultSize*			m_pResultSize;
	CPlantLocTriArray*		m_LocTrisTab[CPLANT_LOC_TRIS_LIST_NUM];
	CPlantLocTriArray*		m_LocTrisRenderTab[CPLANT_LOC_TRIS_LIST_NUM];

#if FURGRASS_TEST_V4
	CPlantColBoundEntryFurGrassInfo*	m_furGrassPickupRenderInfo;
#endif

#if __BANK
	Color32					m_gbDefaultGroundColor;
	float					m_bkTriLocFarDistSqr;
	float					m_bkTriLocShortFarDistSqr;
#endif

	u32						m_IsNetworkGameInProgress			:1;		// g_procObjMan flag
	u32						m_bCullSphereEnabled0				:1;
	u32						m_bCullSphereEnabled1				:1;
#if __BANK
	u32						m_gbForceDefaultGroundColor			:1;
	// g_procObjMan flags:
	u32						m_ignoreMinDist						:1;
	u32						m_forceOneObjPerTri					:1;
	u32						m_ignoreSeeding						:1;
	u32						m_enableSeeding						:1;
	u32						m_disableCollisionObjects			:1;
	u32						m_printSpuJobTimings				:1;
	#if PLANTSMGR_DATA_EDITOR
		u32					m_AllCollisionSelectable			:1;
		u32					m_pad00								:21;
	#else
		u32					m_pad00								:22;
	#endif
#else
	u32						m_pad00								:29;
#endif
};


#define CPLANT_LOC_TRI_SHADOW_CANDIDATES_SIZE					(2048*2)


//
//
//
//
class CPlantMgr : public CPlantMgrBase
{
	friend class CPlantLocTri;
	friend class CPlantColBoundEntry;


public:
	CPlantMgr();
	~CPlantMgr();

public:
	static void	Init(unsigned initMode);
	static void	Shutdown(unsigned shutdownMode);

	bool		CheckProceduralMeta();
	bool		ReloadConfig();

#if __BANK
	static bool	InitWidgets(bkBank& bank);
	static void MarkReloadConfig();
#endif

public:
	bool		UpdateBegin();
	void		UpdateEnd();
	void		UpdateTask(u32 listID, s32 iTriProcessSkipMask);
	void		AsyncMemcpyBegin();
	void		AsyncMemcpyEnd();
	void		RenderTabMemcpy(u32 listID);
#if CPLANT_USE_OCCLUSION
	void		UpdateOcclusion();
#endif

	void		UpdateStr();

	static bool	PreUpdateOnceForNewCameraPos(const Vector3& newCamPos);
	static bool	Render();
	static bool	RenderDecal();
#if __BANK
	static bool	RenderDebugForward();
#endif

	static void	SetForceHDGrass(bool enable);
	static bool	GetForceHDGrass()										{ return(ms_bForceHDGrass);	};

private:
	bool		RenderInternal();
	bool		RenderDecalInternal();

public:
	bool		RenderClosedLists(const u32 bufferID, spdTransposedPlaneSet8 &cullFrustum);

private:
#if __BANK
	bool		RenderDebugStuff();			// This debug render function render to gbuffers after main grass render.
	bool		RenderDebugStuffForward();	// This debug render function render during debug render phase near end of frame.
#endif

#if PLANTS_CAST_SHADOWS
public:
	static bool ShadowRender();
private:
	bool		ShadowRenderInternal();
	void		InitShadowCandidates();
	void		ResetShadowCandidates();
	void		AddShadowCandidate(CPlantLocTri *pLocTri);
	void		RenderShadowCandidates(spdTransposedPlaneSet8 &cullFrustum);
#endif //PLANTS_CAST_SHADOWS


#if FURGRASS_TEST_V4
public:
	bool		FurGrassStoreRenderInfo(CPlantColBoundEntryFurGrassInfo *dest, u32 dmaTag);

private:
	bool		FurGrassLODInitialise();
	bool		FurGrassLODRender(const Vector3& camPos, const Vector3& camFront, const Vector3& playerPos);
#endif //FURGRASS_TEST_V4...

	//
	//
	//
private:
	bool		InitialiseInternal();
	void		ShutdownInternal();
	bool		PreUpdateOnceForNewCameraPosInternal(const Vector3& newCameraPos);

	bool		AllocateStrLocTriTabs();
	bool		FreeStrLocTriTabs();
	bool		UpdateStrLocTriTabs();

	bool		InitialiseTextures();
	bool		UpdateStrTextures();
	bool		ShutdownTextures();

	void		AdvanceCurrentScanCode()						{ m_scanCode++;			}
	u16			GetCurrentScanCode()							{ return(m_scanCode);	}

private:
	void		CalculateWindBending(Vector2 &outBending);
	void		CalculateFakeGrassNormal(float rotationAmount);

private:
	bool					_ColBoundCache_Update(const Vector3& cameraPos, bool bQuickUpdate=FALSE);
	bool					_ColBoundCache_ProcessBound(phBound *pBound, CEntity *pEntity, s32 parentIdx, s32 childIdx, const u16 nCurrentScanCode);
	CPlantColBoundEntry*	_ColBoundCache_FindInCache(CEntity *pEntity, s32 parentIdx, s32 childIdx);
	CPlantColBoundEntry*	_ColBoundCache_Add(CEntity* pEntity, s32 parentIdx, s32 childIdx, bool bCheckCacheFirst=FALSE);
	void					_ColBoundCache_Remove(CEntity *pEntity, s32 parentIdx, s32 childIdx);

	bool					IsBoundGeomPlantFriendly(phBoundGeometry *pBound);

	bool					UpdateAllLocTris(CPlantLocTriArray&	triTab, const Vector3& camPos, s32 iTriProcessSkipMask, u32 *pFurgrassTagPresent);
	bool					_ProcessEntryCollisionData(CPlantLocTriArray& triTab, CPlantColBoundEntry *pEntry, const Vector3& camPos, s32 iTriProcessSkipMask, u32 *pFurgrassTagPresent);
	bool					_CullDistanceCheck(const Vec3V positions[4], Vec3V_In camPos, bool bIsDrawFarTri, bool checkAllCulled);
	bool					_CullSphereCheck(const Vec3V positions[4]);



private:
	// debug/stats functions:
	bool					DbgPrintCPlantMgrInfo(u32 nNumLocTrisDrawn, u32 nNumLocTrisTexSkipped, u32 nNumPlantsLOD0Drawn, u32 nNumPlantsLOD1Drawn, u32 nNumPlantsLOD2Drawn);
	bool					DbgCountCachedBounds(u32 *pCountAll, u32 *pCountColl, u32 *pCountMat);
	bool					DbgCountLocTrisAndPlants(u32 *pCountLocTris, u32 *pCountPlants, bool bCountCreatesPlants, bool bCountCreatesObjects, bool bCountFree);
	void					DbgDrawTriangleArrays(atArray<Vector3> &triangleVertices, atArray<Color32>& triangleColors);
	void					DbgDrawLineArrays(atArray<Vector3> &lineVertices, atArray<Color32>& lineColors);

#if SECTOR_TOOLS_EXPORT
public:
	// Statistics Functions for Performance Stats (DHM 23 Jan 2008)
	// These are similar to the other debug functions but available in Release Build
	void				DbgGetCPlantMgrInfo( u32* nNumLocTrisDrawn, u32* nNumPlantsDrawn );
	void				DbgStatsCountCachedBounds( u32* pCountAll );
	void				DbgStatsCountLocTrisAndPlants(u32* pCountLocTris, u32* pCountPlants );
#endif // SECTOR_TOOLS_EXPORT


//////////////////////////////////////////////////////////////////////////////////////////////////////

#if CPLANT_DYNAMIC_CULL_SPHERES
	//Dynamic culling sphere interface
	public:
		u32 AddDynamicCullSphere(const spdSphere &s);	//Returns cull sphere index which can be used as a handle on success, or PLANTSMGR_INVALID_DYNAMIC_CULL_SPHERE_INDEX on failure.
		void RemoveDynamicCullSphere(u32 handle);
		void ResetDynamicCullSpheres();					//Resets the dynamic cull sphere array. (All culling spheres are off)

		typedef PlantMgr_DynCullSphereArray DynCullSphereArray;
		const DynCullSphereArray &GetCullSphereArray(u32 id) const		{ return m_DynCullSpheresBuffer[id]; }

	private:
		void CopyCullSpheresToUpdateBuffer();

		typedef atRangeArray<spdSphere, PLANTSMGR_MAX_DYNAMIC_CULL_SPHERES> DynCullSphereSourceArray;
		DynCullSphereSourceArray m_DynCullSpheres;
		atRangeArray<DynCullSphereArray, 2> m_DynCullSpheresBuffer;
#endif //CPLANT_DYNAMIC_CULL_SPHERES...

private:
	CPlantColBoundEntry*		MoveColEntToList(u16* ppCurrentList, u16* ppNewList, CPlantColBoundEntry *pEntry);
	CPlantLocTri*				MoveLocTriToList(CPlantLocTriArray& triTab, u16* ppCurrentList, u16* ppNewList, CPlantLocTri *pTri);


public:
//	CPlantLocTriArray			m_LocTrisTab[CPLANT_LOC_TRIS_LIST_NUM];	// [listID]
	CPlantLocTriArray*			m_LocTrisTab[CPLANT_LOC_TRIS_LIST_NUM];	// [listID]
private:
	// RenderThread stuff: Render() is callled from RT and has to have all necessary data double-buffered:
//	CPlantLocTriArray			m_LocTrisRenderTab[2][CPLANT_LOC_TRIS_LIST_NUM];//[RT/UP][listID]
	CPlantLocTriArray*			m_LocTrisRenderTab[2][CPLANT_LOC_TRIS_LIST_NUM];//[RT/UP][listID]
#if PLANTS_CAST_SHADOWS
	// Two buffers, one for plant rendering to write candidates to (GetUpdateRTBufferID()), and one for the shader rendering to read from (GetRenderRTBufferID()).
	u32							m_noOfShadowCandidates[2];
	CPlantLocTri*				m_ShadowCandidates[2][CPLANT_LOC_TRI_SHADOW_CANDIDATES_SIZE];
#endif //PLANTS_CAST_SHADOWS

public:
#if FURGRASS_TEST_V4
	CPlantColBoundEntryFurGrassInfo	m_furGrassPickupRenderInfo[2][CPLANT_COL_ENTITY_CACHE_SIZE] ;
#endif

public:
	// helper functions to pick the right buffer:
	u32						GetUpdateRTBufferID() const	{ return(m_RTbufferID);			}
	u32						GetRenderRTBufferID() const	{ return((m_RTbufferID+1)&0x01);}
	void					SwapRTBuffer()				{ (++m_RTbufferID)&=0x01;		}
private:
	u32						m_RTbufferID;


	// global CullSphere:
public:
	void				EnableCullSphere(u32 i, const Vector4& sphere)	{ Assert(i<CPLANT_CULLSPHERES_MAX); m_CullSphere0[i]=sphere; m_CullSphereEnabled0[i]=true;	}
	void				DisableCullSphere(u32 i)						{ Assert(i<CPLANT_CULLSPHERES_MAX); m_CullSphereEnabled0[i]=false;							}
	bool				IsCullSphereEnabled(u32 i)						{ Assert(i<CPLANT_CULLSPHERES_MAX); return m_CullSphereEnabled0[i];							}
private:
	Vector4				m_CullSphere0[CPLANT_CULLSPHERES_MAX];
	bool				m_CullSphereEnabled0[CPLANT_CULLSPHERES_MAX];

private:
	bool				LoadPlantModel(char *fname, grassModel *pGrassModel);
	void				UnloadPlantModel(grassModel *model);
	bool				LoadPlantModelLOD2(char *fname, grassModel *pGrassModel);
	bool				CreateGeometryLOD2();

private:
	grassModel			m_PlantModelsTab0[CPLANT_SLOT_NUM_MODELS*2] ALIGNED(128);	// LOD0 geometry + LOD1 geometry
	grcTexture*			m_PlantTextureTab0[CPLANT_SLOT_NUM_TEXTURES*2];				// LOD0 texture + LOD1 texture

private:
	static Vector2		ms_AvgWindVector;
	static bool			ms_bForceHDGrass;
	u16					m_scanCode;
	bool				m_bShouldRunAsyncMemcpyJobThisFrame;
	bool				m_bSuppressObjCreation;
	bool				m_bSuppressObjCreationPermanently;

public:
	void				DisableObjCreation()				{ m_bSuppressObjCreationPermanently=true;  }
	void				EnableObjCreation()					{ m_bSuppressObjCreationPermanently=false; }
	bool				ProcObjCreationEnabled()			{ return !m_bSuppressObjCreationPermanently;}	// true=enabled, false=disabled
private:

	bool				m_bEnableAmbScaleScan;
public:
	void				EnableAmbScaleScan()				{ m_bEnableAmbScaleScan=true;	}
	void				DisableAmbScaleScan()				{ m_bEnableAmbScaleScan=false;  }
	bool				IsAmbScaleScanEnabled()				{ return m_bEnableAmbScaleScan; }
private:

#if FURGRASS_TEST_V4
	u32					m_nFurgrassTagPresentTab[CPLANT_LOC_TRIS_LIST_NUM];			// output for every subupdate task
	bool				m_bFurgrassDoRender[2];										// accumulated results from UP
	bool				m_bFurgrassTrisRtPresent[2];								// results from RT's Furgrass::Render()
public:
	inline bool			DoFurgrassRender() const				{ FastAssert(CSystem::IsThisThreadId(SYS_THREAD_RENDER));	return m_bFurgrassDoRender[GetRenderRTBufferID()];};
	inline bool			GetFurgrassTrisRtPresent() const		{ FastAssert(CSystem::IsThisThreadId(SYS_THREAD_UPDATE));	return m_bFurgrassTrisRtPresent[GetUpdateRTBufferID()];};
private:
#endif //FURGRASS_TEST_V4...


#if PLANTSMGR_DATA_EDITOR
public:
	inline void			SetAllCollisionSelectable(bool b)			{ FastAssert(CSystem::IsThisThreadId(SYS_THREAD_UPDATE));		m_bAllCollisionSelectable[GetUpdateRTBufferID()]=b;		};
	inline bool			GetAllCollisionSelectableUT() const		 	{ FastAssert(!CSystem::IsThisThreadId(SYS_THREAD_RENDER));	return m_bAllCollisionSelectable[GetUpdateRTBufferID()];};
	inline bool			GetAllCollisionSelectable() const			{ FastAssert(CSystem::IsThisThreadId(SYS_THREAD_RENDER));	return m_bAllCollisionSelectable[GetRenderRTBufferID()];};
	inline void			ForceRegenTriCaches(bool b)					{ m_bRegenTriCaches=b; };
	inline bool			GetRegenTriCaches() const					{ return(m_bRegenTriCaches); };
private:
	bool				m_bAllCollisionSelectable[2];
	bool				m_bRegenTriCaches;
#endif //PLANTSMGR_DATA_EDITOR...

#if __BANK
	static float		ms_bkColTestRadius;
	static float		ms_bkTriLocFarDist, ms_bkTriLocFarDistSqr;
	static float		ms_bkTriLocShortFarDist, ms_bkTriLocShortFarDistSqr;
	static bool			ms_bkbWindResponseEnabled;
	static float		ms_bkfWindResponseAmount;
	static float		ms_bkfWindResponseVelMin;
	static float		ms_bkfWindResponseVelMax;
	static float		ms_bkfWindResponseVelExp;
#endif //__BANK...
};
extern CPlantMgr gPlantMgr;

// wrapper class for interfacing the plants manager with the game skeleton code
class CPlantMgrWrapper
{
public:
    static void			UpdateBegin();
	static void			UpdateEnd();
#if CPLANT_USE_OCCLUSION
	static void			UpdateOcclusion();
#endif
	static void			UpdateSafeMode();
};

#if PLANTSMGR_DATA_EDITOR
namespace rage {
	class bkData;
	class bkCombo;
}

class CPlantMgrDataEditor
{
public:
	CPlantMgrDataEditor	();

	bool				InitWidgets(bkBank& bank, const char *bankName);
	void				Update();
	void				UpdateSafe();
	void				Render();

	static void			cbBankToggleAllowPicking();
	static void			cbBankToggleShowPlantPolys();
	static void			cbBankToggleSelectAllCollision();
	static void			cbBankButtonCopyPlayerPosToSphereCenter();
	static void			cbBankButtonSearchSphere();
	static void			cbBankButtonClearSelEntries();
	static void			cbBankTextEditEntryIdxChanged();
	static void			cbBankButtonTypeNameChanged();
	static void			cbBankButtonSetProcTypeToAll();
	static void			cbBankButtonSaveCurEntry();
	static void			cbBankButtonSaveBatch();
	static void			cbBankToggleFindClosestEntriesFromFile();
	static void			cbBankToggleWriteSelectionToFile();

private:
	int					FindClosestEntry(const Vector3& p0, const Vector3& p1, bool bMultiSelect);
	bool				FindClosestEntries(const Vector3& p0, float radius, bool bKeepSelection = false);
	void				Reset(bool bKeepSelection = false);
	void				ResetEditableEntry();
	void				UpdateEditableEntry();
	void				SaveEditableEntryTo(CPlantLocTri* pLocTri);

	void				AddEditableDataToWidget(bkBank& bank);
	
	bool				ReadInputDataFile();
	bool				WriteOutputDataFile();
	
private:
	atArray<CPlantLocTri*>	m_selectedEntries;

public:
	inline bool				IsAllCollisionSelectable() const					{ return m_bEdAllCollisionSelectable;	}
	inline void				SetRegenTriCache(bool b)							{ m_bEdRegenTriCache=b;					}
	inline bool				GetRegenTriCache() const							{ return m_bEdRegenTriCache;			}

private:
	Vector3					m_searchPos;
	float					m_searchRadius;
	int						m_searchEntriesFound;
	bool					m_bAllowPicking;
	bool					m_bShowPlantPolys;
	bool					m_bEdAllCollisionSelectable;
	bool					m_bEdRegenTriCache;
	bool					m_bSphereSearchPending;
	bool					m_bClearSelectedEntries;
	bool					m_bUpdateCurEditEntry;
	bool					m_bSaveCurEditEntry;
	bool					m_bSetProcTypeToAll;

	bool					m_bMaxUiSphereSearchFromFilePending;
	bool					m_bMaxUiWritingSelectionToFile;
	bool					m_bMaxUiSavingChangesToBatch;

	int						m_editEntryIdx;
	atArray<Vector3>		m_triPositions;

	atArray<const char*>	m_tagNames;
	bkCombo*				m_tagNamesCombo;

	struct EditableData
	{
		Vector3 			m_V[3];
		Color32				m_GroundColorV[3];
		u32 				m_GroundDensity[3];
		u32 				m_GroundScaleZ[3];
		u32 				m_GroundScaleXYZ[3];
		s32					m_nSurfaceType;	
	};
	EditableData			m_editableEntry;
};
extern CPlantMgrDataEditor gPlantMgrEditor;
#endif //PLANTSMGR_DATA_EDITOR...

//
//
//
//
inline Vector3 CPlantLocTri::GetCenter() const
{ 
	static Vector4 one_third(1.0f/3.0f,1.0f/3.0f,1.0f/3.0f,1.0f/3.0f);
	Vector4 r = ( m_V1 + m_V2 + m_V3 ) * one_third;
	return r.GetVector3();
}

//
// packs density, scaleZ and scaleXYZ weights into 8 bits:
// bit layout:
// 0-3: scaleXYZ
// 4-5: scaleZ
// 6-7: density
//
inline u8 CPlantLocTri::pv8PackDensityScaleZScaleXYZ(u8 density, u8 scaleZ, u8 scaleXYZ)
{
	density	&= 0x03;	// 2 bits
	scaleZ	&= 0x03;	// 2 bits
	scaleXYZ&= 0x0f;	// 4 bits
	return (density << 6) | (scaleZ << 4) | (scaleXYZ);
}

// unpacks ScaleXYZ weight <0; 15>:
inline u8 CPlantLocTri::pv8UnpackScaleXYZ(u8 densityAndScale8)
{
	return (densityAndScale8 & 0x0f);
}

// unpacks ScaleZ weight <0; 3>:
inline u8 CPlantLocTri::pv8UnpackScaleZ(u8 densityAndScale8)
{
	return ((densityAndScale8 >> 4) & 0x03);
}

// unpacks Density weight <0; 3>:
inline u8 CPlantLocTri::pv8UnpackDensity(u8 densityAndScale8)
{
	return ((densityAndScale8 >> 6) & 0x03);
}

// maps scaleXYZ weight <0; 15> to <0; 1>:
inline float CPlantLocTri::pv8MapScaleXYZ(u8 scaleXYZ)
{
	scaleXYZ &= 0x0f;
	return float(scaleXYZ)/15.0f;
}

#define CPLANTLOCTRI_PV_MAP_ARRAYS	float CPlantLocTri::ms_pvMapScaleZ[4]	= {1.0f, 2.0f/3.0f, 1.0f/3.0f, 0.0f}; \
									float CPlantLocTri::ms_pvMapDensity[4]	= {1.0f, 2.0f/3.0f, 1.0f/3.0f, 0.0f};

// maps scaleZ weight <0; 3>:
inline float CPlantLocTri::pv8MapScaleZ(u8 scaleZ)
{
	return ms_pvMapScaleZ[scaleZ&0x03];
}

// maps density weight <0; 3>:
inline float CPlantLocTri::pv8MapDensity(u8 density)
{
	return ms_pvMapDensity[density&0x03];
}

// unpack and map combo:
inline float CPlantLocTri::pv8UnpackAndMapScaleXYZ(u8 densityAndScale8)
{
	return pv8MapScaleXYZ( pv8UnpackScaleXYZ(densityAndScale8) );
}

inline float CPlantLocTri::pv8UnpackAndMapScaleZ(u8 densityAndScale8)
{
	return pv8MapScaleZ( pv8UnpackScaleZ(densityAndScale8) );
}

inline float CPlantLocTri::pv8UnpackAndMapDensity(u8 densityAndScale8)
{
	return pv8MapDensity( pv8UnpackDensity(densityAndScale8) );
}

__forceinline void* PlantStrAlloc(size_t size, size_t align)
{
	MEM_USE_USERDATA(MEMUSERDATA_PLANTMGR);
	return strStreamingEngine::GetAllocator().Allocate((size+(g_rscVirtualLeafSize-1))&(~(g_rscVirtualLeafSize-1)), align, MEMTYPE_RESOURCE_VIRTUAL);
}

__forceinline void PlantStrFree(void *ptr)
{
	MEM_USE_USERDATA(MEMUSERDATA_PLANTMGR);
	strStreamingEngine::GetAllocator().Free(ptr);
}

// helper macros to allocate memory from streaming heap
// (str allocator uses leafsize=g_rscVirtualLeafSize (ps3: 4096; 360: 8192):
#define PLANTS_MALLOC_STR(SIZE,ALIGN)	PlantStrAlloc((SIZE), (ALIGN))
#define PLANTS_FREE_STR(P)				PlantStrFree(P)

#endif //__CPLANTSMGR_H__...

