///////////////////////////////////////////////////////////////////////////////
//  
//	FILE: 	VfxWeapon.h
//	BY	: 	Mark Nicholson
//	FOR	:	Rockstar North
//	ON	:	01 June 2006
//	WHAT:	
//
///////////////////////////////////////////////////////////////////////////////

#ifndef VFX_WEAPON_H
#define	VFX_WEAPON_H


///////////////////////////////////////////////////////////////////////////////
//  INCLUDES
///////////////////////////////////////////////////////////////////////////////													

// rage
#include "script/thread.h"
#include "vectormath/classes.h"

// framework
#include "fwscene/world/InteriorLocation.h"

// game
#include "control/replay/ReplaySettings.h"
#include "Debug/Debug.h"
#include "Physics/GtaMaterial.h"
#include "Weapons/WeaponEnums.h"


///////////////////////////////////////////////////////////////////////////////
//  FORWARD DECLARATIONS
///////////////////////////////////////////////////////////////////////////////	

namespace rage
{
	class grcTexture;
	class ptxEffectInst;
	class phIntersection;
}

class CEntity;
class CObject;
class CProjectile;
class CVehicle;
class CWeapon;
class CWeaponInfo;

struct VfxCollInfo_s; 


///////////////////////////////////////////////////////////////////////////////
//  DEFINES
///////////////////////////////////////////////////////////////////////////////													

#define	VFXWEAPON_MAX_INFOS					(320)

#define VFXWEAPON_MAX_GROUND_DISTURB_PROBES	(8)

#define	VFXRANGE_WPN_MUZZLE_FLASH			(60.0f)
#define	VFXRANGE_WPN_MUZZLE_FLASH_SQR		(VFXRANGE_WPN_MUZZLE_FLASH*VFXRANGE_WPN_MUZZLE_FLASH)

#define	VFXRANGE_WPN_MUZZLE_FLASH_VEH		(150.0f)
#define	VFXRANGE_WPN_MUZZLE_FLASH_VEH_SQR	(VFXRANGE_WPN_MUZZLE_FLASH_VEH*VFXRANGE_WPN_MUZZLE_FLASH_VEH)

#define	VFXRANGE_WPN_MUZZLE_SMOKE			(10.0f)
#define	VFXRANGE_WPN_MUZZLE_SMOKE_SQR		(VFXRANGE_WPN_MUZZLE_SMOKE*VFXRANGE_WPN_MUZZLE_SMOKE)

#define	VFXRANGE_WPN_GUNSHELL				(7.0f)
#define	VFXRANGE_WPN_GUNSHELL_SQR			(VFXRANGE_WPN_GUNSHELL*VFXRANGE_WPN_GUNSHELL)

#define	VFXRANGE_WPN_GUNSHELL_VEH			(40.0f)
#define	VFXRANGE_WPN_GUNSHELL_VEH_SQR		(VFXRANGE_WPN_GUNSHELL_VEH*VFXRANGE_WPN_GUNSHELL_VEH)

#define	VFXRANGE_WPN_VOLUMETRIC				(50.0f)
#define	VFXRANGE_WPN_VOLUMETRIC_SQR			(VFXRANGE_WPN_VOLUMETRIC*VFXRANGE_WPN_VOLUMETRIC)

#define	VFXRANGE_WPN_VOLUMETRIC_VEHICLE		(150.0f)
#define	VFXRANGE_WPN_VOLUMETRIC_VEHICLE_SQR	(VFXRANGE_WPN_VOLUMETRIC_VEHICLE*VFXRANGE_WPN_VOLUMETRIC_VEHICLE)

#define	VFXRANGE_WPN_BULLET_TRACE			(30.0f)
#define	VFXRANGE_WPN_BULLET_TRACE_SQR		(VFXRANGE_WPN_BULLET_TRACE*VFXRANGE_WPN_BULLET_TRACE)


///////////////////////////////////////////////////////////////////////////////
//  STRUCTURES
///////////////////////////////////////////////////////////////////////////////	

// VfxWeaponInfo_s
struct VfxWeaponInfo_s
{
	s32		id;

	s32		decalRenderSettingIndex;
	s32		decalRenderSettingCount;

	s32		stretchRenderSettingIndex;
	s32		stretchRenderSettingCount;
	float	stretchDot;
	float	stretchMult;

	float	decalRange;

	u8		decalColR;
	u8		decalColG;
	u8		decalColB;
	u8		decalColA;

	bool	useExclusiveMtl;

	float	minTexSize;
	float	maxTexSize;
	float	fadeInTime;
	float	minLifeTime;
	float	maxLifeTime;
	float	maxOverlayRadius;
	float	duplicateRejectDist;

	s32		shotgunDecalRenderSettingIndex;
	s32		shotgunDecalRenderSettingCount;
	float	shotgunTexScale;

	u32		ptFxWeaponHashName;
	float	ptFxRange;
	float	ptFxScale;
	float	ptFxLodScale;
};


struct VfxWeaponTableData_e
{
	s32		id;
	s32		offset;
	s32		count;
};

struct VfxWeaponGroundDisturbProbe_s
{
	Vec3V	m_vStartPos;
	Vec3V	m_vEndPos;
	Vec3V	m_vDir;
	RegdWeapon m_pWeapon;

	s8		m_vfxProbeId;
};



///////////////////////////////////////////////////////////////////////////////
//  CLASSES
///////////////////////////////////////////////////////////////////////////////

class CVfxWeaponAsyncProbeManager
{	
	///////////////////////////////////
	// FUNCTIONS 
	///////////////////////////////////

	public: ///////////////////////////

		void Init();
		void Shutdown();
		void Update();

		void TriggerGroundDisturbProbe(Vec3V_In vStartPos, Vec3V_In vEndPos, CWeapon* pWeapon, Vec3V_In vDir);


	private: //////////////////////////


	///////////////////////////////////
	// VARIABLES
	///////////////////////////////////

	private: //////////////////////////

		VfxWeaponGroundDisturbProbe_s m_groundDisturbProbes	[VFXWEAPON_MAX_GROUND_DISTURB_PROBES];

};

class CVfxWeapon
{	
	///////////////////////////////////
	// FUNCTIONS 
	///////////////////////////////////

	public: ///////////////////////////

		// main interface
		void				Init						(unsigned initMode);
		void				Shutdown					(unsigned shutdownMode);
		
		void				Update						();

		void				RemoveScript				(scrThreadId scriptThreadId);

		// data table access
		VfxWeaponInfo_s*	GetInfo						(VfxGroup_e vfxGroup, s32 weaponGroupId, bool isBloody=false);
		int					GetBreakableGlassId			(s32 weaponGroupId);
		
		// 
		void				DoWeaponFiredVfx			(CWeapon* pWeapon);
		void				DoWeaponImpactVfx			(const VfxCollInfo_s& vfxCollInfo, eDamageType damageType, CEntity* pFiringEntity);

		// particle systems
		void				TriggerPtFxBulletImpact		(const VfxWeaponInfo_s* pVfxWeaponInfo, CEntity* pEntity, Vec3V_In vPos, Vec3V_In vNormal, Vec3V_In vDirection, bool isExitFx, const CPed* pFiringPed, bool isVehicleMG, fwInteriorLocation hitPtInteriorLocation, bool useHitPtInteriorLocation);
		void				ProcessPtFxMuzzleFlash		(NOT_REPLAY(const) CEntity* pFiringEntity, const CWeapon* pWeapon, CEntity* pMuzzleEntity, s32 muzzleBoneIndex, bool useAlternateFx, Vec3V_In muzzleOffset = Vec3V(V_ZERO));
		void				TriggerPtFxMuzzleFlash		(NOT_REPLAY(const) CEntity* pFiringEntity, const CWeapon* pWeapon, CEntity* pMuzzleEntity, s32 muzzleBoneIndex, bool useAlternateFx, Vec3V_In muzzleOffset, bool isFirstPersonPass);
		void				TriggerPtFxMuzzleFlash_AirDefence (const CWeapon *pWeapon, const Mat34V mMat, const bool useAlternateFx);
		void				ProcessPtFxGunshell			(const CEntity* pFiringEntity, const CWeapon* pWeapon, CEntity* pGunshellEntity, s32 gunshellBoneIndex, s32 weaponIndex);
		void				TriggerPtFxGunshell			(const CEntity* pFiringEntity, const CWeapon* pWeapon, CEntity* pGunshellEntity, s32 gunshellBoneIndex, s32 weaponIndex, bool isFirstPersonPass);
		bool				TriggerPtFxBulletTrace		(const CEntity* pFiringEntity, const CWeapon* pWeapon, const CWeaponInfo* pWeaponInfo, Vec3V_In vStartPos, Vec3V_In vEndPos);
		void		 		TriggerPtFxMtlReactWeapon	(const CWeaponInfo* pWeaponInfo, Vec3V_In vPos);
		void				TriggerPtFxEMP				(CVehicle* pVehicle, float scale=1.0f);

		void				UpdatePtFxMuzzleSmoke		(const CEntity* pFiringEntity, const CWeapon* pWeapon, CEntity* pMuzzleEntity, s32 muzzleBoneIndex, float smokeLevel, Vec3V_In muzzleOffset = Vec3V(V_ZERO));
		void				ProcessPtFxVolumetric		(const CWeapon* pWeapon, CEntity* pMuzzleEntity, s32 muzzleBoneIndex);
		void				UpdatePtFxVolumetric		(const CWeapon* pWeapon, CEntity* pMuzzleEntity, s32 muzzleBoneIndex, bool isFirstPersonPass);
		//void				UpdatePtFxLaserSight		(const CWeapon* pWeapon, Vec3V_In vStartPos, Vec3V_In vEndPos);
		void				UpdatePtFxWeaponGroundDisturb(const CWeapon* pWeapon, Vec3V_In vPos, Vec3V_In vNormal, Vec3V_In vDir, float distEvo, VfxDisturbanceType_e vfxDisturbanceType);

		void				SetIntSmokeOverrideLevel	(float val)				{m_intSmokeOverrideLevel = val;}
		void				SetIntSmokeOverrideHashName	(const char* pName)		{m_intSmokeOverrideHashName = atHashWithStringNotFinal(pName);}	
		void				ResetIntSmokeOverrides		();

		void				SetBulletImpactPtFxUserZoomScale(float val, scrThreadId scriptThreadId);
		void				SetBulletImpactPtFxLodRangeScale(float val, scrThreadId scriptThreadId);
		void				SetBulletImpactDecalRangeScale(float val, scrThreadId scriptThreadId);
		void				SetBulletTraceNoAngleReject(bool val, scrThreadId scriptThreadId);
		bool				GetBulletTraceNoAngleReject()						{return m_bulletTraceNoAngleReject;}

#if __BANK
		void				InitWidgets					();
		float				GetForceBulletTraces		()						{return m_forceBulletTraces;}

		// bullet impact overrides
		bool				GetBulletImpactDecalOverridesActive()				{return m_bulletImpactDecalOverridesActive;}
		float				GetBulletImpactDecalMaxOverlayRadiusOverride()		{return m_bulletImpactDecalOverrideMaxOverlayRadius;}
		float				GetBulletImpactDecalDuplicateRejectDistOverride()	{return m_bulletImpactDecalOverrideDuplicateRejectDist;}

		bool				GetRenderGroundDisturbVfxProbes()					{return m_renderGroundDisturbVfxProbes;}
#endif

		// 
		Vec3V_Out			GetWeaponTintColour			(u8 weaponTintIndex, s32 numBulletsLeft);

	private: //////////////////////////

		// init helper functions
		void				LoadData					();
		void		 		ProcessLookUpData			();
		void		 		StoreLookUpData				(s32 currId, s32 currOffset, s32 currCount);

		//
		void				ProcessSpecialMaterial		(const VfxCollInfo_s& vfxCollInfo);

		// particle systems
		void				UpdatePtFxInteriorSmoke		(Mat34V_In camMat, float fxAlpha, float smokeEvo);
		void				UpdatePtFxInteriorDust		(Mat34V_In camMat, float fxAlpha);

#if __BANK
static	void				Reset						();
static	void				TriggerPlayerVehicleEMP		();
#endif


	///////////////////////////////////
	// VARIABLES
	///////////////////////////////////

	private: //////////////////////////

		// file version
		float				m_version;

		// async probes
		CVfxWeaponAsyncProbeManager m_asyncProbeMgr;

		// weapon data table
		VfxWeaponTableData_e m_vfxWeaponTable			[NUM_VFX_GROUPS][EWEAPONEFFECTGROUP_MAX];
		
		s32					m_numVfxWeaponInfos;		
		VfxWeaponInfo_s		m_vfxWeaponInfos			[VFXWEAPON_MAX_INFOS];

		// breakable glass lookup table
		int					m_vfxBreakableGlass			[EWEAPONEFFECTGROUP_MAX];
		
		// interior smoke overrides
		float				m_intSmokeOverrideLevel;
		atHashWithStringNotFinal m_intSmokeOverrideHashName;

		// bullet impact settings
		scrThreadId			m_bulletImpactPtFxUserZoomScaleScriptThreadId;
		float				m_bulletImpactPtFxUserZoomScale;
		scrThreadId			m_bulletImpactPtFxLodRangeScaleScriptThreadId;
		float				m_bulletImpactPtFxLodRangeScale;
		scrThreadId			m_bulletImpactDecalRangeScaleScriptThreadId;
		float				m_bulletImpactDecalRangeScale;
		scrThreadId			m_bulletTraceNoAngleRejectScriptThreadId;
		bool				m_bulletTraceNoAngleReject;


		// debug variables
#if __BANK
		bool				m_bankInitialised;

		char				m_intSmokeInteriorName		[128];
		char				m_intSmokeRoomName			[128];
		s32					m_intSmokeRoomId;
		float				m_intSmokeTargetLevel;
		char				m_intSmokeOverrideName		[64];
		bool				m_disableInteriorSmoke;			
		bool				m_disableInteriorDust;

		bool				m_forceBulletTraces;
		bool				m_renderDebugBulletTraces;
		bool				m_attachBulletTraces;

		// bullet impact overrides
		bool				m_bulletImpactDecalOverridesActive;
		float				m_bulletImpactDecalOverrideMaxOverlayRadius;
		float				m_bulletImpactDecalOverrideDuplicateRejectDist;

		bool				m_renderGroundDisturbVfxProbes;
#endif

}; // CVfxWeapon



///////////////////////////////////////////////////////////////////////////////
//  EXTERNS
///////////////////////////////////////////////////////////////////////////////

extern	CVfxWeapon			g_vfxWeapon;


extern	dev_float			VFXWEAPON_INTSMOKE_INC_PER_SHOT;
extern	dev_float			VFXWEAPON_INTSMOKE_INC_PER_EXP;
extern	dev_float			VFXWEAPON_INTSMOKE_DEC_PER_UPDATE;
extern	dev_float			VFXWEAPON_INTSMOKE_MAX_LEVEL;
extern	dev_float			VFXWEAPON_INTSMOKE_INTERP_SPEED;
extern	dev_float			VFXWEAPON_INTSMOKE_MIN_GRID_SIZE;

extern	dev_float			VFXWEAPON_TRACE_ANGLE_REJECT_THRESH;
extern	dev_float			VFXWEAPON_TRACE_ANGLE_REJECT_THRESH_FPS;



#endif // VFX_WEAPON_H


///////////////////////////////////////////////////////////////////////////////
