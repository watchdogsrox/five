///////////////////////////////////////////////////////////////////////////////
//  
//	FILE: 	VfxTrail.h
//	BY	: 	Mark Nicholson
//	FOR	:	Rockstar North
//	ON	:	12 July 2006
//	WHAT:	
//
///////////////////////////////////////////////////////////////////////////////

#ifndef VFX_TRAIL_H
#define	VFX_TRAIL_H


///////////////////////////////////////////////////////////////////////////////
//  INCLUDES
///////////////////////////////////////////////////////////////////////////////													

// Rage headers
#include "vectormath/classes.h"

// Framework headers
#include "vfx/vfxlist.h"
#include "fwutil/idgen.h"

// Game headers
#include "Scene/RegdRefTypes.h"
#include "Vfx/Systems/VFxPed.h"


///////////////////////////////////////////////////////////////////////////////
//  FORWARD DECLARATIONS
///////////////////////////////////////////////////////////////////////////////	

namespace rage
{
	class crSkeleton;
	class grcTexture;
}

struct VfxMaterialInfo_s;
struct VfxWheelInfo_s;
class CEntity;


///////////////////////////////////////////////////////////////////////////////
//  DEFINES
///////////////////////////////////////////////////////////////////////////////													

#define	VFXTRAIL_MAX_DEBUG_POINTS		(64)
#define	VFXTRAIL_MAX_GROUPS				(128)
//#define	VFXTRAIL_MAX_INFOS				(64)


///////////////////////////////////////////////////////////////////////////////
//  ENUMS
///////////////////////////////////////////////////////////////////////////////	

// types of trails
enum VfxTrailType_e
{
	VFXTRAIL_TYPE_SKIDMARK				= 0,
	VFXTRAIL_TYPE_SCRAPE,
	VFXTRAIL_TYPE_WADE,
	//VFXTRAIL_TYPE_SKI,
	VFXTRAIL_TYPE_GENERIC,

	// liquid trail must be last in the list
	VFXTRAIL_TYPE_LIQUID,
};

// ped trail offsets
enum VfxPedTrailOffsets_e
{
	VFXTRAIL_PED_OFFSET_WADE			= 0,
	VFXTRAIL_PED_OFFSET_SKI				= VFXPEDLIMBID_NUM	
};


///////////////////////////////////////////////////////////////////////////////
//  STRUCTURES
///////////////////////////////////////////////////////////////////////////////	

typedef struct 
{
	Vec3V					vWorldPos;
	Vec3V					vLocalPos;
	Vec3V					vCollNormal;
	Vec3V					vCollVel;
	Vec3V					vDir;
	Vec3V					vForwardCheck;
	Vec3V					vNormal;

	// calculated internally	
	Vec3V					vPrevProjVerts[4];
	s32						matrixIndex;

	fwUniqueObjId			id;
	VfxTrailType_e			type;

	RegdEnt					regdEnt;
	s32						componentId;

	s32						decalRenderSettingIndex;
	s32						decalRenderSettingCount;
	s32						decalRenderSettingsIndex;			// calculated from the previous 2 values - this is stored for when we need to continue the same texture along the scrape
	float					decalLife;
	float					width;
	Color32					col;
	u8						mtlId;

	// liquid specific
	float					liquidPoolStartSize;
	float					liquidPoolEndSize;
	float					liquidPoolGrowthRate;

	// scrape specific
	VfxMaterialInfo_s*		pVfxMaterialInfo;
	float					scrapeMag;
	float					accumImpulse;
	float					vehicleEvo;
	float					lodRangeScale;
	float					zoom;

	// flags
	bool					hasEntityPtr : 1;
	bool					createLiquidPools : 1;
	bool					dontApplyDecal : 1;

	// debug
#if __BANK
	bool					origHasEntityPtr : 1;
	Vec3V					vOrigWorldPos;
	Vec3V					vOrigLocalPos;
	Mat34V					origEntityMatrix;
#endif

} VfxTrailInfo_t;


///////////////////////////////////////////////////////////////////////////////
//  CLASSES
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  CDebugTrailPoint
///////////////////////////////////////////////////////////////////////////////

#if __BANK
class CDebugTrailPoint : public vfxListItem
{
	///////////////////////////////////
	// VARIABLES 
	///////////////////////////////////		

	public: ///////////////////////////

		VfxTrailInfo_t		m_info;

}; // CDebugTrailPoint
#endif // __BANK


///////////////////////////////////////////////////////////////////////////////
//  CTrailGroup
///////////////////////////////////////////////////////////////////////////////

class CTrailGroup : public vfxListItem
{
	///////////////////////////////////
	// FUNCTIONS 
	///////////////////////////////////

	public: ///////////////////////////

		// main interface
		void				Init					(VfxTrailType_e type, fwUniqueObjId id);
		void				Shutdown				();

		void		 		AddPoint				(VfxTrailInfo_t& vfxTrailInfo, bool isLeadOut=false);


	private: //////////////////////////
	
		// particle systems
		void		 		UpdatePtFxMtlScrape		(VfxTrailInfo_t& vfxTrailInfo);
		void		 		UpdatePtFxLiquidTrail	(VfxTrailInfo_t& vfxTrailInfo, Vec3V_In vPrevTrailPos, bool prevTrailPosValid);

		
	///////////////////////////////////
	// VARIABLES 
	///////////////////////////////////		

	public: ///////////////////////////

		VfxTrailType_e		m_type;
		fwUniqueObjId		m_id;

		Vec3V				m_vCurrDir;

		bool				m_isActive;
		bool				m_registeredThisFrame;
		bool				m_isFirstTrailDecal;

		bool				m_lastTrailInfoValid;
		VfxTrailInfo_t		m_lastTrailInfo;

		u32					m_timeLastRegistered;
		u32					m_timeLastTrailInfoAdded;

		float				m_currLength;
		
#if __BANK
		float				m_debugRenderLife;
		vfxList				m_debugTrailPointList;
#endif

}; // CTrailGroup


///////////////////////////////////////////////////////////////////////////////
//  CVfxTrail
///////////////////////////////////////////////////////////////////////////////

class CVfxTrail
{	
	///////////////////////////////////
	// FRIENDS 
	///////////////////////////////////

	friend class CTrailGroup;
#if GTA_REPLAY
	friend class CPacketMarkSkidMarkFx;
#endif // GTA_REPLAY


	///////////////////////////////////
	// FUNCTIONS 
	///////////////////////////////////

	public: ///////////////////////////

		// main interface
		void				Init						(unsigned initMode);
		void				Shutdown					(unsigned shutdownMode);
		
		void		 		Update						(float deltaTime);

		// registration functions
		void		 		RegisterPoint				(VfxTrailInfo_t& vfxTrailInfo);

		// access
const	VfxTrailInfo_t*		GetLastTrailInfo			(fwUniqueObjId id);

		// debug functions
#if __BANK
		void				InitWidgets					();
		void		 		RenderDebug					();
#endif


	private: //////////////////////////

		// registration functions
		//void				StoreTrailInfo				(VfxTrailInfo_t& vfxTrailInfo);
		//void		 		ProcessStoredTrailInfos		();

		// helper functions
		CTrailGroup*	 	FindActiveTrailGroup		(fwUniqueObjId id);
		//CTrailGroup*	 	FindActiveTrailGroup		(Vec3V_In vPos, Vec3V_In vDir);
		//CTrailGroup*	 	FindActiveTrailGroupMovable	(Vec3V_In vPos, CEntity* pEntity, s32 matrixIndex);

		// pool access
		CTrailGroup*	 	GetTrailGroupFromPool		();
		void		 		ReturnTrailGroupToPool		(CTrailGroup* pTrailGroup);

		// debug functions
#if __BANK
		CDebugTrailPoint*	GetDebugTrailPointFromPool	(vfxList* pDebugListToAddTo);
		void		 		ReturnDebugTrailPointToPool	(CDebugTrailPoint* pDebugTrailPoint, vfxList* pDebugListToRemoveFrom);
#endif

#if __ASSERT
		void				ProcessDataValidity			(Mat34V_In mat, VfxTrailInfo_t& vfxTrailInfo, s32 originId);
#endif


	///////////////////////////////////
	// VARIABLES
	///////////////////////////////////

	private: //////////////////////////

		// trail group variables
		CTrailGroup			m_trailGroups				[VFXTRAIL_MAX_GROUPS];
		vfxList				m_trailGroupPool;
		vfxList				m_trailGroupList;

		// trail info variables
//		u32					m_numStoredTrailInfos;
//		VfxTrailInfo_t		m_storedTrailInfos			[VFXTRAIL_MAX_INFOS];

		// debug variables
#if __BANK	
		s32					m_numTrailGroups;

		float				m_debugRenderLifeSingle;
		float				m_debugRenderLifeMultiple;

		bool				m_debugRenderSkidmarkInfo;
		bool				m_debugRenderScrapeInfo;
		bool				m_debugRenderWadeInfo;
		//bool				m_debugRenderSkiInfo;
		bool				m_debugRenderGenericInfo;
		bool				m_debugRenderLiquidInfo;

		bool				m_disableSkidmarkTrails;
		bool				m_disableScrapeTrails;
		bool				m_disableWadeTrails;
		bool				m_disableGenericTrails;
		bool				m_disableLiquidTrails;

		CDebugTrailPoint	m_debugTrailPoints			[VFXTRAIL_MAX_DEBUG_POINTS];
		vfxList				m_debugTrailPointPool;
#endif

}; // CVfxTrail


///////////////////////////////////////////////////////////////////////////////
//  EXTERNS
///////////////////////////////////////////////////////////////////////////////

extern	CVfxTrail				g_vfxTrail;


#endif // VFX_TRAIL_H



