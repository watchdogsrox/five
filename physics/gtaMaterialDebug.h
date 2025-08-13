///////////////////////////////////////////////////////////////////////////////
//  
//	FILE:	GtaMaterialDebug.h
//	BY	: 	Mark Nicholson
//	FOR	:	Rockstar North
//	ON	:	01 December 2008
//
///////////////////////////////////////////////////////////////////////////////

#ifndef GTA_MATERIAL_DEBUG_H
#define GTA_MATERIAL_DEBUG_H


#if __BANK

///////////////////////////////////////////////////////////////////////////////
//  INCLUDES
///////////////////////////////////////////////////////////////////////////////

// rage
#include "bank/combo.h"
#include "phcore/materialmgr.h"
#include "phbound/primitives.h"

// game
#include "GtaMaterial.h"


///////////////////////////////////////////////////////////////////////////////
//  DEFINES
///////////////////////////////////////////////////////////////////////////////

#define MAX_CULLED_POLY_VERTS	(10240)


class phMaterialGta;

namespace rage
{
	class BvhPrimBox;
	class BvhPrimCapsule;
	class BvhPrimSphere;
	class phBound;
	class phBoundBox;
	class phBoundCapsule;
	class phBoundCylinder;
	class phBoundPolyhedron;
	class phBoundSphere;
	class phPolygon;
	class phInst;
}


///////////////////////////////////////////////////////////////////////////////
//  ENUMERATIOS
///////////////////////////////////////////////////////////////////////////////

enum MtlColourMode
{
	MTLCOLOURMODE_OFF				= 0,
	MTLCOLOURMODE_CONSTANT,
	MTLCOLOURMODE_BOUND,
	MTLCOLOURMODE_ENTITY,
	MTLCOLOURMODE_PACKFILE,
	MTLCOLOURMODE_VFXGROUP,
	MTLCOLOURMODE_GLASS,
	MTLCOLOURMODE_FLAGS,

	MTLCOLOURMODE_NUM,

	MTLCOLOURMODE_DEFAULT1 = MTLCOLOURMODE_ENTITY
};

enum MtlQuickDebugMode
{
	MTLDEBUGMODE_OFF				= 0,
	MTLDEBUGMODE_BOUND_WIREFRAME,
	MTLDEBUGMODE_BOUND_WIREFRAME_MTLNAME,
	MTLDEBUGMODE_BOUND_WIREFRAME_VFXGROUPNAME,
	MTLDEBUGMODE_VFXGROUP_SOLID,
	MTLDEBUGMODE_FLAGS_SOLID,
	MTLDEBUGMODE_VFXGROUP_FLAGS_SOLID,

	MTLDEBUGMODE_NUM
};


///////////////////////////////////////////////////////////////////////////////
//  CLASSES
///////////////////////////////////////////////////////////////////////////////

//  phMaterialDebug  //////////////////////////////////////////////////////////
//
///////////////////////////////////////////////////////////////////////////////

class phMaterialDebug
{
	///////////////////////////////////
	// FUNCTIONS 
	///////////////////////////////////

	public: ///////////////////////////

#ifndef NAVGEN_TOOL
		void				Init						();
		void				Update						();
#if __BANK
		void				InitWidgets					(bkBank& bank);
#endif 
		void				UpdateProcObjComboBox		();
		bool				GetProcObjComboNeedsUpdate	() const			{return m_procObjComboNeedsUpdate;}
#endif 

		bool				GetDisableSeeThrough		() const			{return m_disableSeeThru;}
		bool				GetDisableShootThrough		() const 			{return m_disableShootThru;}
		bool				GetZeroPenetrationResistance() const			{return m_zeroPenetrationResistance;}

		void				UserRenderBound				(phInst* pInst, phBound* pBound, Mat34V_In vMtx, bool isCompositeChild);

static	void				ToggleQAMode				();
static	void				ToggleRenderNonClimbableCollision();
static	void				ToggleRenderCarGlassMaterials();
static	void				ToggleRenderNonCarGlassMaterials();
static	void				ToggleRenderFindCollisionHoles();
static	void				ToggleRenderFindCollisionHolesWeaponMapBounds();
static	void				ToggleRenderFindCollisionHolesMoverMapBounds();

		void				SetRenderPedDensity			(bool b);
		void				SetRenderPavement			(bool b);

	private: //////////////////////////

		void				Render						();

		bool				IsBoundInRange				(phBound* pBound, Mat34V_In vMtx, Vec3V_In vCamPos);
		bool				ShouldRenderBound			(phInst* pInst, phBound* pBound, s32 childId, bool isCompositeChild);
		bool				ShouldRenderPoly			(phMaterialGta* pMtl, phMaterialMgr::Id mtlId);

		void				RenderBound					(phInst* pInst, phBound* pBound, Mat34V_In vCurrMtx, bool isCompositeChild);
		void				RenderPoly					(const phPolygon& currPoly, phMaterialGta* pMtl, phMaterialMgr::Id mtlId, phBoundPolyhedron* pBoundPoly, Mat34V_In vMtx, Vec3V_In vCamPos);
		void				RenderBVHBoxPrim			(const phPrimBox& bvhBoxPrim, phMaterialGta* pMtl, phMaterialMgr::Id mtlId, phBoundPolyhedron* pBoundPoly, Mat34V_In vMtx, Vec3V_In vCamPos);
		void				RenderBVHSpherePrim			(const phPrimSphere& bvhSpherePrim, phMaterialGta* pMtl, phMaterialMgr::Id mtlId, phBoundPolyhedron* pBoundPoly, Mat34V_In vMtx, Vec3V_In vCamPos);
		void				RenderBVHCapsulePrim		(const phPrimCapsule& bvhCapsulePrim, phMaterialGta* pMtl, phMaterialMgr::Id mtlId, phBoundPolyhedron* pBoundPoly, Mat34V_In vMtx, Vec3V_In vCamPos);
		void				RenderBoundCapsule			(phMaterialGta* pMtl, phMaterialMgr::Id mtlId, phBoundCapsule* pBoundCapsule, Mat34V_In vMtx, Vec3V_In vCamPos);
		void				RenderBoundSphere			(phMaterialGta* pMtl, phMaterialMgr::Id mtlId, phBoundSphere* pBoundSphere, Mat34V_In vMtx, Vec3V_In vCamPos);
		void				RenderBoundCylinder			(phMaterialGta* pMtl, phMaterialMgr::Id mtlId, phBoundCylinder* pBoundCylinder, Mat34V_In vMtx, Vec3V_In vCamPos);
		void				RenderBoundBox				(phMaterialGta* pMtl, phMaterialMgr::Id mtlId, phBoundBox* pBoundSphere, Mat34V_In vMtx, Vec3V_In vCamPos);
		void				RenderBoundingVolumes		(phInst *pInst);

		void				UpdateCurrColourInst		(const phInst *pInst);
		void				UpdateCurrColourBound		(int boundType);
		void				UpdateCurrColourPoly		(const phMaterialMgr::Id mtlId, float nDotL=1.0f);

static	void				ToggleRenderStairsCollision	();
static	void				ToggleRenderOnlyWeaponMapBounds();
static	void				ToggleRenderOnlyMoverMapBounds();



	///////////////////////////////////
	// VARIABLES
	///////////////////////////////////

	private: //////////////////////////

		// widget pointers
		bkCombo*			m_pProcObjCombo;
		bool				m_procObjComboNeedsUpdate;//signifies whether update needs to be called

		// debug
		bool				m_disableSeeThru;
		bool				m_disableShootThru;
		bool				m_zeroPenetrationResistance;
		bool				m_renderNonClimbableCollision;
		int					m_quickDebugMode;

		// settings - how to render
		Color32				m_constantColour;
		Color32				m_currColour;
		Color32				m_currColourLit;
		float				m_vertexShift;
		MtlColourMode		m_colourMode1;
		MtlColourMode		m_colourMode2;
		MtlColourMode		m_currColourMode;
		bool				m_flashPolys;
		bool				m_solidPolys;
		bool				m_litPolys;
		bool				m_renderBackFaces;
		bool				m_printObjectNames;
		u8					m_polyAlpha;
		float				m_polyRange;
		bool				m_polyRangeXY; // calculate range in XY only
		bool				m_polyProjectToScreen;

		// settings - what to render - general
		bool				m_renderAllPolys;
		bool				m_renderBoundingSpheres;
		bool				m_renderBoundingBoxes;

		// settings - what to render - bound options
		bool				m_renderCompositeBounds;
		bool				m_renderBVHBounds;
		bool				m_renderGeometryBounds;
		bool				m_renderBoxBounds;
		bool				m_renderSphereBounds;
		bool				m_renderCapsuleBounds;
		bool				m_renderCylinderBounds;

		bool				m_renderWeaponMapBounds;						// high lod map bounds used for weapons and vfx
		bool				m_renderMoverMapBounds;							// low lod map bounds used for collisions
		bool				m_renderStairSlopeMapBounds;					// stair-slope collisions used on stairs

		// settings - what to render - material options
		bool				m_renderMaterials;
		bool				m_includeMaterialsAbove;
		bool				m_renderCarGlassMaterials;
		bool				m_renderNonCarGlassMaterials; // could make an enum out of these ..
		s32					m_currMaterialId;

		bool				m_renderFxGroups;
		s32					m_currFxGroup;

		bool				m_renderMaterialFlags;
		s32					m_currMaterialFlag;

		// settings - what to render - poly options
		bool				m_renderBVHPolyPrims;
		bool				m_renderBVHSpherePrims;
		bool				m_renderBVHCapsulePrims;
		bool				m_renderBVHBoxPrims;

		bool				m_renderProceduralTags;
		s32					m_currProceduralTag;

		bool				m_renderRoomIds;
		s32					m_currRoomId;

		bool				m_renderPolyFlags;
		s32					m_currPolyFlag;

		bool				m_renderPedDensityInfo;
		bool				m_renderPavementInfo;
		bool				m_renderPedDensityValues;
		bool				m_renderMaterialNames;
		bool				m_renderVfxGroupNames;

		// exclusions
		bool				m_excludeMap;
		bool				m_excludeBuildings;
		bool				m_excludeObjects;
		bool				m_excludeFragObjects;
		bool				m_excludeVehicles;
		bool				m_excludeFragVehicles;
		bool				m_excludePeds;
		bool				m_excludeFragPeds;
		bool				m_excludeNonWeaponTest;

		bool				m_excludeInteriors;
		bool				m_excludeExteriors;

		// 
		phPolygon::Index 	m_culledPolys[MAX_CULLED_POLY_VERTS];

		// optimization bank widgets
		bool				m_renderStairsCollision;
		bool				m_renderOnlyWeaponMapBounds;
		bool				m_renderOnlyMoverMapBounds;

};

#endif // __BANK

#endif // GTA_MATERIAL_DEBUG_H


