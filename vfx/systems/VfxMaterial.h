///////////////////////////////////////////////////////////////////////////////
//  
//	FILE: 	VfxMaterial.h
//	BY	: 	Mark Nicholson
//	FOR	:	Rockstar North
//	ON	:	01 June 2006
//	WHAT:	Material vs Material effects 
//
///////////////////////////////////////////////////////////////////////////////

#ifndef VFX_MATERIAL_H
#define	VFX_MATERIAL_H


///////////////////////////////////////////////////////////////////////////////
//  INCLUDES
///////////////////////////////////////////////////////////////////////////////													

// rage
#include "math/random.h"
#include "phcore/materialmgr.h"
#include "script/thread.h"

// game
#include "Physics/GtaMaterial.h"
#include "control/replay/ReplaySettings.h"


///////////////////////////////////////////////////////////////////////////////
//  FORWARD DECLARATIONS
///////////////////////////////////////////////////////////////////////////////	

namespace rage
{
	class grcTexture;
	class phCollider;
}

class CEntity;


///////////////////////////////////////////////////////////////////////////////
//  DEFINES
///////////////////////////////////////////////////////////////////////////////													

#define	VFXMATERIAL_MAX_INFOS			(96)

#if __BANK
#define VFXMATERIAL_MAX_DEBUG_SCRAPES	(32)
#endif

#define	VFXRANGE_MTL_SCRAPE_BANG		(25.0f)
#define	VFXRANGE_MTL_SCRAPE_BANG_SQR	(VFXRANGE_MTL_SCRAPE_BANG*VFXRANGE_MTL_SCRAPE_BANG)


///////////////////////////////////////////////////////////////////////////////
//  ENUMS
///////////////////////////////////////////////////////////////////////////////	


///////////////////////////////////////////////////////////////////////////////
//  STRUCTURES
///////////////////////////////////////////////////////////////////////////////	

struct VfxMaterialInfo_s
{
	s32 id;

	// collision threshold info
	float velThreshMin;
	float velThreshMax;
	float impThreshMin;
	float impThreshMax;

	// particle effect info
	atHashWithStringNotFinal ptFxHashName;

	// decal info
	float texWidthMin;
	float texWidthMax;
	float texLengthMultMin;
	float texLengthMultMax;

	float maxOverlayRadius;
	float duplicateRejectDist;

	s32 decalRenderSettingIndex;
	s32 decalRenderSettingCount;

	u8 decalColR;
	u8 decalColG;
	u8 decalColB;
	
	bool ptFxSkipUnderwater;
};


struct VfxMaterialTableData_s
{
	s32 id;
	s32 offset[2];
	s32 count[2];
};

struct VfxCollisionInfo_s
{
	VfxCollisionInfo_s() :
		  pEntityA(NULL)
		, pEntityB(NULL)
		, componentIdA(-1)
		, componentIdB(-1)
		, pColliderA(NULL)
		, pColliderB(NULL)
		, vRelVelocity(V_ZERO)
		, vWorldPosA(V_ZERO)
		, vWorldPosB(V_ZERO)
		, vWorldNormalA(V_ZERO)
		, vWorldNormalB(V_ZERO)
		, mtlIdA(phMaterialMgr::MATERIAL_NOT_FOUND)
		, mtlIdB(phMaterialMgr::MATERIAL_NOT_FOUND)
		, vAccumImpulse(V_ZERO)
		, vColnVel(V_ZERO)
		, vColnDir(V_ZERO)
		, bangMagA(0.f)
		, scrapeMagA(0.f)
		, bangMagB(0.f)
		, scrapeMagB(0.f)
	{}

	// from the manifold
	CEntity* pEntityA;
	CEntity* pEntityB;
	int componentIdA;
	int componentIdB;
	phCollider* pColliderA;
	phCollider* pColliderB;
	Vec3V vRelVelocity;

	// from the contact
	Vec3V vWorldPosA;
	Vec3V vWorldPosB;
	Vec3V vWorldNormalA;
	Vec3V vWorldNormalB;
	phMaterialMgr::Id mtlIdA;
	phMaterialMgr::Id mtlIdB;
	ScalarV vAccumImpulse;

	// calculated internally
	Vec3V vColnVel;
	Vec3V vColnDir;
	float bangMagA;
	float scrapeMagA;
	float bangMagB;
	float scrapeMagB;
};

#if __BANK
struct VfxDebugScrapeInfo_s
{
	Vec3V vPosition;
	Vec3V vNormal;
	CEntity* pEntity;
};
#endif


///////////////////////////////////////////////////////////////////////////////
//  CLASSES
///////////////////////////////////////////////////////////////////////////////

class CVfxMaterial
{	
#if GTA_REPLAY
	friend class CPacketPTexMtlBang;
	friend class CPacketPTexMtlScrape;
#endif // GTA_REPLAY

	///////////////////////////////////
	// FUNCTIONS 
	///////////////////////////////////

	public: ///////////////////////////

		// main interface
		void				Init						(unsigned initMode);
		void				Shutdown					(unsigned shutdownMode);

		void				Update						();

		void				RemoveScript				(scrThreadId scriptThreadId);

		// main processing function
		bool				ProcessCollisionVfx			(VfxCollisionInfo_s& vfxColnInfo);
		void				DoMtlBangFx					(CEntity* pEntity, s32 componentId, Vec3V_In vCollPos, Vec3V_In vCollNormal, Vec3V_In vCollVel, phMaterialMgr::Id mtlIdA, phMaterialMgr::Id mtlIdB, float bangMag, float accumImpulse, float lodRangeScale, float vehicleEvo, float zoom);
		void				DoMtlScrapeFx				(CEntity* pEntityA, s32 componentIdA, CEntity* pEntityB, s32 componentIdB, Vec3V_In vCollPos, Vec3V_In vCollNormal, Vec3V_In vCollVel, phMaterialMgr::Id mtlIdA, phMaterialMgr::Id mtlIdB, Vec3V_In vCollDir, float scrapeMag, float accumImpulse, float lodRangeScale, float vehicleEvo, float zoom);

		// data table access
		VfxMaterialInfo_s*	GetBangInfo					(VfxGroup_e vfxGroupA, VfxGroup_e vfxGroupB);
		VfxMaterialInfo_s*	GetScrapeInfo				(VfxGroup_e vfxGroupA, VfxGroup_e vfxGroupB);

		// settings
		void				SetBangScrapePtFxLodRangeScale(float val, scrThreadId scriptThreadId);
		float				GetBangScrapePtFxLodRangeScale()					{return m_bangScrapePtFxLodRangeScale;}

		// debug functions
#if __BANK
		void				InitWidgets					();

		bool				GetColnVfxProcessPerContact	()						{return m_colnVfxProcessPerContact;}
		bool				GetColnVfxDisableTimeSlice1	()						{return m_colnVfxDisableTimeSlice1;}
		bool				GetColnVfxDisableTimeSlice2	()						{return m_colnVfxDisableTimeSlice2;}
		bool				GetColnVfxDisableWheels		()						{return m_colnVfxDisableWheels;}
		bool				GetColnVfxDisableEntityA	()						{return m_colnVfxDisableEntityA;}
		bool				GetColnVfxDisableEntityB	()						{return m_colnVfxDisableEntityB;}
		bool				GetColnVfxRecomputePositions()						{return m_colnVfxRecomputePositions;}
		bool				GetColnVfxOutputDebug		()						{return m_colnVfxOutputDebug;}
		bool				GetColnVfxRenderDebugVectors()						{return m_colnVfxRenderDebugVectors;}
		bool				GetColnVfxRenderDebugImpulses()						{return m_colnVfxRenderDebugImpulses;}

		bool				GetBangVfxAlwaysPassThresholds()					{return m_bangVfxAlwaysPassVelocityThreshold || m_bangVfxAlwaysPassImpulseThreshold;}
		bool				GetScrapeVfxAlwaysPassThresholds()					{return m_scrapeVfxAlwaysPassVelocityThreshold || m_scrapeVfxAlwaysPassImpulseThreshold;}
		bool				GetScrapeDecalsDisabled		()						{return m_scrapeDecalsDisabled;}
		bool				GetScrapeDecalsDisableSingle()						{return m_scrapeDecalsDisableSingle;}
		bool				GetScrapePtFxDisabled		()						{return m_scrapePtFxDisabled;}
		bool				GetScrapePtFxOutputDebug	()						{return m_scrapePtFxOutputDebug;}
		bool				GetScrapePtFxRenderDebug	()						{return m_scrapePtFxRenderDebug;}
		float				GetScrapePtFxRenderDebugSize()						{return m_scrapePtFxRenderDebugSize;}
		int					GetScrapePtFxRenderDebugTimer()						{return m_scrapePtFxRenderDebugTimer;}

		Color32				GetVfxGroupDebugCol			(VfxGroup_e vfxGroup)	{return m_vfxGroupDebugCol[vfxGroup];}
#endif

	private: //////////////////////////

		// init helper functions
		void				LoadData					();
		void		 		ProcessLookUpData			();
		void		 		StoreLookUpData				(s32 index, s32 currId, s32 currOffset, s32 currCount);

		// particle systems
		void				TriggerPtFxMtlBang			(const VfxMaterialInfo_s* pVfxMaterialInfo, CEntity* pEntity, Mat34V_In mat, float bangMag, float accumImpulse, float lodRangeScale, float vehicleEvo, float zoom, bool hydraulicsHack);

		// debug functions
#if __BANK
static	void				Reset						();
static	void				StartDebugScrapeRecording	();
static	void				StopDebugScrapeRecording	();
static	void				UpdateDebugScrapeRecording	();
static	void				ApplyDebugScrape			();
#endif


	///////////////////////////////////
	// VARIABLES
	///////////////////////////////////

	private: //////////////////////////

		// file version
		float				m_version;

		// material data table
		VfxMaterialTableData_s m_vfxMaterialTable		[NUM_VFX_GROUPS][NUM_VFX_GROUPS];

		s32					m_numVfxMtlBangInfos;	
		VfxMaterialInfo_s	m_vfxMtlBangInfo			[VFXMATERIAL_MAX_INFOS];
		
		s32					m_numVfxMtlScrapeInfos;	
		VfxMaterialInfo_s	m_vfxMtlScrapeInfo			[VFXMATERIAL_MAX_INFOS];

#if __BANK
		Color32				m_vfxGroupDebugCol			[NUM_VFX_GROUPS];
#endif

		// settings
		scrThreadId			m_bangScrapePtFxLodRangeScaleScriptThreadId;
		float				m_bangScrapePtFxLodRangeScale;

		// debug variables
#if __BANK
		bool				m_bankInitialised;

		bool				m_colnVfxProcessPerContact;
		bool				m_colnVfxDisableTimeSlice1;
		bool				m_colnVfxDisableTimeSlice2;
		bool				m_colnVfxDisableWheels;
		bool				m_colnVfxDisableEntityA;
		bool				m_colnVfxDisableEntityB;
		bool				m_colnVfxRecomputePositions;
		bool				m_colnVfxOutputDebug;
		bool				m_colnVfxRenderDebugVectors;
		bool				m_colnVfxRenderDebugImpulses;

		bool				m_bangVfxAlwaysPassVelocityThreshold;
		bool				m_bangVfxAlwaysPassImpulseThreshold;
		bool				m_bangDecalsDisabled;
		bool				m_bangPtFxDisabled;
		bool				m_bangPtFxOutputDebug;
		bool				m_bangPtFxRenderDebug;
		float				m_bangPtFxRenderDebugSize;
		int					m_bangPtFxRenderDebugTimer;

		bool				m_scrapeVfxAlwaysPassVelocityThreshold;
		bool				m_scrapeVfxAlwaysPassImpulseThreshold;
		bool				m_scrapeDecalsDisabled;
		bool				m_scrapeDecalsDisableSingle;
		//bool				m_scrapeTrailsUseId;
		bool				m_scrapePtFxDisabled;
		bool				m_scrapePtFxOutputDebug;
		bool				m_scrapePtFxRenderDebug;
		float				m_scrapePtFxRenderDebugSize;
		int					m_scrapePtFxRenderDebugTimer;

		static bool			ms_debugScrapeRecording;
		static bool			ms_debugScrapeApplying;
		static int			ms_debugScrapeRecordCount;
		static int			ms_debugScrapeApplyCount;
		static VfxDebugScrapeInfo_s ms_debugScrapeInfos  [VFXMATERIAL_MAX_DEBUG_SCRAPES];
#endif

}; // CVfxMaterial



///////////////////////////////////////////////////////////////////////////////
//  EXTERNS
///////////////////////////////////////////////////////////////////////////////

extern	CVfxMaterial		g_vfxMaterial;

extern	dev_float			VFXMATERIAL_ROTOR_SPAWN_DIST;
extern	dev_float			VFXMATERIAL_ROTOR_SIZE_SCALE;
extern	dev_float			VFXMATERIAL_WHEEL_COLN_DOT_THRESH;
extern	dev_float			VFXMATERIAL_LOD_RANGE_SCALE_VEHICLE;
extern	dev_float			VFXMATERIAL_LOD_RANGE_SCALE_HELI;


#endif // VFX_MATERIAL_H



