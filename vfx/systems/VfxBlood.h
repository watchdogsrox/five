///////////////////////////////////////////////////////////////////////////////
//  
//	FILE: 	VfxBlood.h
//	BY	: 	Mark Nicholson
//	FOR	:	Rockstar North
//	ON	:	29 Jan 2007
//	WHAT:	 
//
///////////////////////////////////////////////////////////////////////////////

#ifndef VFX_BLOOD_H
#define	VFX_BLOOD_H


///////////////////////////////////////////////////////////////////////////////
//  INCLUDES
///////////////////////////////////////////////////////////////////////////////													

// rage
#include "script/thread.h"

// game
#include "Control/Replay/ReplaySettings.h"
#include "Debug/Debug.h"
#include "Physics/GtaMaterialManager.h"
#include "Weapons/WeaponEnums.h"


///////////////////////////////////////////////////////////////////////////////
//  FORWARD DECLARATIONS
///////////////////////////////////////////////////////////////////////////////	

struct VfxCollInfo_s; 

class CEntity;
class CObject;
class CPed;
class CTaskTimer;
class CVehicle;
class CVfxPedInfo;
class CWheel;

namespace rage
{
	class grcTexture;
}


///////////////////////////////////////////////////////////////////////////////
//  DEFINES
///////////////////////////////////////////////////////////////////////////////													

#define	VFXBLOOD_MAX_INFOS						(64)
#define	VFXBLOOD_NUM_PED_MATERIALS				(22)

#define	VFXBLOOD_MAX_SPLAT_PROBES				(32)
#define	VFXBLOOD_MAX_POOL_PROBES				(8)

#define	VFXRANGE_BLOOD_FALL_TO_DEATH			(40.0f)
#define	VFXRANGE_BLOOD_FALL_TO_DEATH_SQR		(VFXRANGE_BLOOD_FALL_TO_DEATH*VFXRANGE_BLOOD_FALL_TO_DEATH)

#define	VFXRANGE_BLOOD_WHEEL_SQUASH				(20.0f)
#define	VFXRANGE_BLOOD_WHEEL_SQUASH_SQR			(VFXRANGE_BLOOD_WHEEL_SQUASH*VFXRANGE_BLOOD_WHEEL_SQUASH)

#define	VFXRANGE_BLOOD_MOUTH					(20.0f)
#define	VFXRANGE_BLOOD_MOUTH_SQR				(VFXRANGE_BLOOD_MOUTH*VFXRANGE_BLOOD_MOUTH)

#define	VFXRANGE_BLOOD_MIST						(60.0f)
#define	VFXRANGE_BLOOD_MIST_SQR					(VFXRANGE_BLOOD_MIST*VFXRANGE_BLOOD_MIST)

#define	VFXRANGE_BLOOD_DRIPS					(30.0f)
#define	VFXRANGE_BLOOD_DRIPS_SQR				(VFXRANGE_BLOOD_DRIPS*VFXRANGE_BLOOD_DRIPS)

#define	VFXRANGE_BLOOD_POOL_IN_WATER			(50.0f)
#define	VFXRANGE_BLOOD_POOL_IN_WATER_SQR		(VFXRANGE_BLOOD_POOL_IN_WATER*VFXRANGE_BLOOD_POOL_IN_WATER)

#define	VFXRANGE_BLOOD_SHARK_ATTACK				(50.0f)
#define	VFXRANGE_BLOOD_SHARK_ATTACK_SQR			(VFXRANGE_BLOOD_SHARK_ATTACK*VFXRANGE_BLOOD_SHARK_ATTACK)


///////////////////////////////////////////////////////////////////////////////
//  ENUMS
///////////////////////////////////////////////////////////////////////////////	

enum VfxBloodSpecialWeaponGroups_e
{
	VFXBLOOD_SPECIAL_WEAPON_GROUP_CLOWN			= EWEAPONEFFECTGROUP_MAX,
	VFXBLOOD_SPECIAL_WEAPON_GROUP_ALIEN,

	VFXBLOOD_NUM_WEAPON_GROUPS
};


///////////////////////////////////////////////////////////////////////////////
//  STRUCTURES
///////////////////////////////////////////////////////////////////////////////	

// VfxBloodInfo_s
struct VfxBloodInfo_s
{
	s32		id;

	float	probabilityBlood;
	float	probabilityDamage;

	s32		splatNormRenderSettingIndex;
	s32		splatNormRenderSettingCount;
	s32		splatSoakRenderSettingIndex;
	s32		splatSoakRenderSettingCount;

	s32		sprayNormRenderSettingIndex;
	s32		sprayNormRenderSettingCount;
	s32		spraySoakRenderSettingIndex;
	s32		spraySoakRenderSettingCount;

	s32		mistNormRenderSettingIndex;
	s32		mistNormRenderSettingCount;
	s32		mistSoakRenderSettingIndex;
	s32		mistSoakRenderSettingCount;

	u8		decalColR;
	u8		decalColG;
	u8		decalColB;

	float	sprayDotProductThresh;
	float	mistTValueThresh;

	float	lodRangeHi;
	float	lodRangeLo;

	u8		numProbesHi;
	u8		numProbesLo;

	float	probeVariation;

	float	probeDistA;
	float	probeDistB;
	float	sizeMinAHi;
	float	sizeMaxAHi;
	float	sizeMinALo;
	float	sizeMaxALo;
	float	sizeMinBHi;
	float	sizeMaxBHi;
	float	sizeMinBLo;
	float	sizeMaxBLo;
	float	lifeMin;
	float	lifeMax;
	float	fadeInTime;
	float	growthTime;
	float	growthMultMin;
	float	growthMultMax;

	atHashWithStringNotFinal	ptFxBloodHashName;
	atHashWithStringNotFinal	ptFxBloodBodyArmourHashName;
	atHashWithStringNotFinal	ptFxBloodHeavyArmourHashName;
	atHashWithStringNotFinal	ptFxBloodUnderwaterHashName;
	atHashWithStringNotFinal	ptFxBloodSloMoHashName;
	float	ptFxSizeEvo;
	atHashWithStringNotFinal	pedDamageHashName;
	atHashWithStringNotFinal	pedDamageBodyArmourHashName;
	atHashWithStringNotFinal	pedDamageHeavyArmourHashName;
};

// VfxBloodTableData_s
struct VfxBloodTableData_s
{
	s32		id;
	s32		offset			[2];
	s32		count			[2];
};

struct VfxBloodSplatProbe_s
{
	Vec3V	m_vProbeDir;
	Vec3V	m_vMidPoint;
	const VfxBloodInfo_s* m_pVfxBloodInfo;

	float	m_hiToLoLodRatio;

	s8		m_vfxProbeId;
	bool	m_isProbeA:1;
	bool    m_isFromPlayer:1;
	bool    m_pad0:6;
};

struct VfxBloodPoolProbe_s
{
	RegdPed	m_regdPed;
	float 	m_poolStartSize;
	float 	m_poolEndSize;
	float 	m_poolGrowRate;

	s8		m_vfxProbeId;
};


///////////////////////////////////////////////////////////////////////////////
//  CLASSES
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  CVfxBlood
///////////////////////////////////////////////////////////////////////////////

class CVfxBloodAsyncProbeManager
{	

	///////////////////////////////////
	// FUNCTIONS 
	///////////////////////////////////

	public: ///////////////////////////
		
		void Init();
		void Shutdown();
		void Update();
		void TriggerSplatProbe(Vec3V_In vStartPos, Vec3V_In vEndPos, Vec3V_In vProbeDir, const VfxBloodInfo_s* pVfxBloodInfo, float hiToLoLodRatio, bool isProbeA, bool isFromPlayerPed);
		void TriggerPoolProbe(Vec3V_In vStartPos, Vec3V_In vEndPos, CPed* pPed, float poolStartSize, float poolEndSize, float poolGrowRate);


	private: //////////////////////////



	///////////////////////////////////
	// VARIABLES
	///////////////////////////////////

	private: //////////////////////////

		VfxBloodSplatProbe_s m_splatProbes	[VFXBLOOD_MAX_SPLAT_PROBES];
		VfxBloodPoolProbe_s m_poolProbes	[VFXBLOOD_MAX_POOL_PROBES];

};


class CVfxBlood
{	
	friend class CDecalDebug;
	friend class CVfxBloodAsyncProbeManager;

#if GTA_REPLAY
	friend class CPacketBloodFx;
#endif // GTA_REPLAY

	///////////////////////////////////
	// FUNCTIONS 
	///////////////////////////////////

	public: ///////////////////////////

		// main interface
		void				Init						(unsigned initMode);
		void				Shutdown					(unsigned shutdownMode);

		void				Update						();

		void				PatchData					(const char* pFilename);
		void				UnPatchData					(const char* pFilename);

		void				RemoveScript				(scrThreadId scriptThreadId);

		// main processing function
		void				DoVfxBlood					(const VfxCollInfo_s& vfxCollInfo, const CEntity* pFiringEntity);

		// blood pools
		bool				InitBloodPool				(CPed* pPed, CTaskTimer& timer, float& startSize, float& endSize, float& growRate);
		void				ProcessBloodPool			(CPed* pPed, float poolStartSize, float poolEndSize, float poolGrowRate);

		// particle systems
		void				TriggerPtFxFallToDeath		(CPed* pPed);
		void				TriggerPtFxWheelSquash		(CWheel* pWheel, CVehicle* pVehicle, Vec3V_In vFxPos);
		void				TriggerPtFxBloodSharkAttack	(CPed* pPed);

		void				UpdatePtFxBloodMist			(CPed* pPed);
		void				UpdatePtFxBloodDrips		(CPed* pPed);
		void				UpdatePtFxBloodPoolInWater	(CPed* pPed);

		void				SetBloodVfxDisabled			(bool val, scrThreadId scriptThreadId);
		void				SetBloodPtFxUserZoomScale	(float val, scrThreadId scriptThreadId);
		void				SetOverrideWeaponGroupId	(s32 val, scrThreadId scriptThreadId);

		// overridden blood
		bool				IsOverridingWithClownBlood	() {return m_overrideWeaponGroupId==VFXBLOOD_SPECIAL_WEAPON_GROUP_CLOWN;}

#if __BANK
		void				InitWidgets					();
#endif

	private: //////////////////////////

		// init helper functions
		void				LoadData					();
		void				LoadTextures				(s32 txdSlot);
		void		 		ProcessLookUpData			();
		void		 		StoreLookUpData				(s32 index, s32 currId, s32 currOffset, s32 currCount);

		// data table access
		VfxBloodInfo_s*		GetEntryAliveInfo			(phMaterialMgr::Id correctedMtlId, s32 weaponGroupId, s32& infoIndex);
		VfxBloodInfo_s*		GetExitAliveInfo			(phMaterialMgr::Id correctedMtlId, s32 weaponGroupId, s32& infoIndex);
		VfxBloodInfo_s*		GetEntryDeadInfo			(phMaterialMgr::Id correctedMtlId, s32 weaponGroupId, s32& infoIndex);
		VfxBloodInfo_s*		GetExitDeadInfo				(phMaterialMgr::Id correctedMtlId, s32 weaponGroupId, s32& infoIndex);

		// Used by the Replay system to obtain the correct infos
		VfxBloodInfo_s*		GetEntryInfo				(s32 entryIndex)		{	return &m_bloodEntryFxInfo[entryIndex];	}
		VfxBloodInfo_s*		GetExitInfo					(s32 exitIndex)			{	return &m_bloodExitFxInfo[exitIndex];	}

		// camera vfx functions
//		void				ProcessCamBlood				(Vec3V_In vPos, Vec3V_In vDir, bool isShotgunWound);

		// particle systems
		void				TriggerPtFxBlood			(const CVfxPedInfo* pVfxPedInfo, const VfxBloodInfo_s* pVfxBloodInfo, const VfxCollInfo_s& vfxCollInfo, float fxDistSqr, bool isExitWound, bool hitBodyArmour, bool hitHeavyArmour, bool isUnderwater, bool isPlayerPed, const CPed* pFiringPed, bool doPtFx, bool doDamage, bool& producedPtFx, bool& producedDamage);	
		void				TriggerPtFxMouthBlood		(CPed* pPed);

		// projected texture functions
		void				TriggerDecalBlood			(const VfxBloodInfo_s* pVfxBloodInfo, Vec3V_In vPos, Vec3V_In vDir, CEntity* pEntity, float fxDistSqr);

		// debug functions
#if __BANK
static	void				Reset						();
#endif


	///////////////////////////////////
	// VARIABLES
	///////////////////////////////////

	private: //////////////////////////

		// file version
		float				m_version;

		// blood data table
		VfxBloodTableData_s	m_vfxBloodTableSPAlive		[VFXBLOOD_NUM_PED_MATERIALS][VFXBLOOD_NUM_WEAPON_GROUPS];
		VfxBloodTableData_s	m_vfxBloodTableSPDead		[VFXBLOOD_NUM_PED_MATERIALS][VFXBLOOD_NUM_WEAPON_GROUPS];
		VfxBloodTableData_s	m_vfxBloodTableMPAlive		[VFXBLOOD_NUM_PED_MATERIALS][VFXBLOOD_NUM_WEAPON_GROUPS];
		VfxBloodTableData_s	m_vfxBloodTableMPDead		[VFXBLOOD_NUM_PED_MATERIALS][VFXBLOOD_NUM_WEAPON_GROUPS];

		s32					m_numBloodEntryVfxInfos;	
		VfxBloodInfo_s		m_bloodEntryFxInfo			[VFXBLOOD_MAX_INFOS];

		s32					m_numBloodExitVfxInfos;	
		VfxBloodInfo_s		m_bloodExitFxInfo			[VFXBLOOD_MAX_INFOS];

		CVfxBloodAsyncProbeManager m_asyncProbeMgr;

		// blood settings
		scrThreadId			m_bloodVfxDisabledScriptThreadId;
		bool				m_bloodVfxDisabled;

		scrThreadId			m_bloodPtFxUserZoomScaleScriptThreadId;
		float				m_bloodPtFxUserZoomScale;

		scrThreadId			m_overrideWeaponGroupIdScriptThreadId;
		s32					m_overrideWeaponGroupId;

#if __BANK
		bool				m_bankInitialised;

		bool				m_disableEntryVfx;
		bool				m_disableExitVfx;
		bool				m_forceMPData;

		bool				m_renderBloodVfxInfo;
		s32					m_renderBloodVfxInfoTimer;
		bool				m_renderBloodSplatVfxProbes;
		bool				m_renderBloodPoolVfxProbes;
#endif

}; // CVfxBlood



///////////////////////////////////////////////////////////////////////////////
//  EXTERNS
///////////////////////////////////////////////////////////////////////////////

extern	CVfxBlood			g_vfxBlood;

extern	dev_float			VFXBLOOD_POOL_LOD_DIST;


#endif // VFX_BLOOD_H


///////////////////////////////////////////////////////////////////////////////
