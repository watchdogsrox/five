///////////////////////////////////////////////////////////////////////////////
//  
//	FILE: 	VfxWater.h
//	BY	: 	Mark Nicholson
//	FOR	:	Rockstar North
//	ON	:	03 Jan 2007
//	WHAT:	
//
///////////////////////////////////////////////////////////////////////////////

#ifndef VFX_WATER_H
#define	VFX_WATER_H


///////////////////////////////////////////////////////////////////////////////
//  INCLUDES
///////////////////////////////////////////////////////////////////////////////													

// rage
#include "grcore/texture.h"
#include "vectormath/classes.h"

// game
#include "Animation/AnimBones.h"
#include "Physics/Floater.h"
#include "Vfx/Systems/VfxPed.h"


///////////////////////////////////////////////////////////////////////////////
//  FORWARD DECLARATIONS
///////////////////////////////////////////////////////////////////////////////	

class CEntity;
class CObject;
class CPed;
class CSubmarine;
class CVehicle;
class CWaterCannon;
class CVfxPedInfo;
class CVfxVehicleInfo;
class CVfxImmediateInterface;

///////////////////////////////////////////////////////////////////////////////
//  DEFINES
///////////////////////////////////////////////////////////////////////////////	

#define	VFXRANGE_WATER_SPLASH_GEN_MAX		(100.0f)
#define	VFXRANGE_WATER_SPLASH_GEN_MAX_SQR	(VFXRANGE_WATER_SPLASH_GEN_MAX*VFXRANGE_WATER_SPLASH_GEN_MAX)

#define	VFXRANGE_WATER_SPLASH_BOAT_MAX		(200.0f)
#define	VFXRANGE_WATER_SPLASH_BOAT_MAX_SQR	(VFXRANGE_WATER_SPLASH_BOAT_MAX*VFXRANGE_WATER_SPLASH_BOAT_MAX)

#define	VFXRANGE_WATER_SPLASH_SUB_MAX		(300.0f)
#define	VFXRANGE_WATER_SPLASH_SUB_MAX_SQR	(VFXRANGE_WATER_SPLASH_BOAT_MAX*VFXRANGE_WATER_SPLASH_BOAT_MAX)

#define	VFXRANGE_WATER_SPLASH_BLADES		(80.0f)
#define	VFXRANGE_WATER_SPLASH_BLADES_SQR	 (VFXRANGE_WATER_SPLASH_BLADES*VFXRANGE_WATER_SPLASH_BLADES)

#define	VFXRANGE_WATER_CANNON_JET			(150.0f)
#define	VFXRANGE_WATER_CANNON_JET_SQR		(VFXRANGE_WATER_CANNON_JET*VFXRANGE_WATER_CANNON_JET)

#define	VFXRANGE_WATER_CANNON_SPRAY			(150.0f)
#define	VFXRANGE_WATER_CANNON_SPRAY_SQR		(VFXRANGE_WATER_CANNON_SPRAY*VFXRANGE_WATER_CANNON_SPRAY)

#define	VFXRANGE_WATER_SUB_DIVE				(50.0f)
#define	VFXRANGE_WATER_SUB_DIVE_SQR			(VFXRANGE_WATER_SUB_DIVE*VFXRANGE_WATER_SUB_DIVE)

#define	VFXRANGE_WATER_SUB_AIR_LEAK			(50.0f)
#define	VFXRANGE_WATER_SUB_AIR_LEAK_SQR		(VFXRANGE_WATER_SUB_AIR_LEAK*VFXRANGE_WATER_SUB_AIR_LEAK)

#define	VFXRANGE_WATER_SUB_CRUSH			(50.0f)
#define	VFXRANGE_WATER_SUB_CRUSH_SQR		(VFXRANGE_WATER_SUB_CRUSH*VFXRANGE_WATER_SUB_CRUSH)


///////////////////////////////////////////////////////////////////////////////
//  ENUMS
///////////////////////////////////////////////////////////////////////////////	

// water cannon ptfx offsets for registered systems that play on the same water cannon
enum PtFxWaterCannonOffsets_e
{
	PTFXOFFSET_WATERCANNON_JET		= 0,
	PTFXOFFSET_WATERCANNON_SPRAY,
};


///////////////////////////////////////////////////////////////////////////////
//  STRUCTURES
///////////////////////////////////////////////////////////////////////////////	

struct VfxWaterSampleData_s 
{
	Vec3V			vTestPos;
	Vec3V			vSurfacePos;

	bool			isInWater;

	float			maxLevel;
	float			prevLevel;
	float			currLevel;

	s32				componentId;
	eAnimBoneTag	boneTag;
};


struct VfxWaterAnimPedData_s
{
	eAnimBoneTag	boneTag;
	eAnimBoneTag	boneTag2;
	float			sampleSize;
	float			rippleLifeScale;
	bool			doFxIn;
	bool			doFxOut;
	bool			doFxWade;
};


// object ptfx offsets for registered systems that play on the same object
enum PtFxObjectOffsets_e
{
	PTFXOFFSET_OBJECT_WADE,
	PTFXOFFSET_OBJECT_WADE_LAST					= PTFXOFFSET_OBJECT_WADE + MAX_WATER_SAMPLES,
	PTFXOFFSET_OBJECT_TRAIL,
	PTFXOFFSET_OBJECT_TRAIL_LAST				= PTFXOFFSET_OBJECT_TRAIL + MAX_WATER_SAMPLES,
};


///////////////////////////////////////////////////////////////////////////////
//  CLASSES
///////////////////////////////////////////////////////////////////////////////

//  CVfxWater  ////////////////////////////////////////////////////////////////
//
//
//
///////////////////////////////////////////////////////////////////////////////

class CVfxWater
{	
	friend class CVfxImmediateInterface;
	///////////////////////////////////
	// FUNCTIONS 
	///////////////////////////////////

	public: ///////////////////////////

		// main interface
		void				Init						(unsigned initMode);
		void				Shutdown					(unsigned shutdownMode);

		void				Update						(float deltaTime);

		void				RemoveScript				(scrThreadId scriptThreadId);

#if __BANK
		void				InitWidgets					();
#endif

		// vfx processing
		void 				ProcessVfxWater				(CEntity* pParent, const VfxWaterSampleData_s* pVfxWaterSampleData, bool hasFakePedSamples, s32 numInSampleLevelArray);

 		// ped splashes
		void				TriggerPtFxSplashPedLod		(CPed* pPed, float downwardSpeed, bool isPlayerVfx);
		void				TriggerPtFxSplashPedEntry	(CPed* pPed, float downwardSpeed, bool isPlayerVfx);

 		// other splashes
 		void				TriggerPtFxSplashHeliBlade	(CEntity* pParent, Vec3V_In vPos);
		static s8			GetEntryWaterSampleIndex	(CVehicle* pVehicle);

 		// boat specific (this needs to go through UpdatePtFxPropeller once the boat class is updated to use generic propellers)
 		void				UpdatePtFxBoatPropeller		(CVehicle* pVehicle, Vec3V_In vPropOffset);
	
		// propeller specific
		void				UpdatePtFxPropeller			(CVehicle* pVehicle, s32 propId, s32 boneIndex, float propSpeed);

		// submarine ptfx 
		void				UpdatePtFxSubmarineDive		(CSubmarine* pSubmarine, float diveEvo);
		void				UpdatePtFxSubmarineAirLeak	(CSubmarine* pSubmarine, float damageEvo);
		void				TriggerPtFxSubmarineCrush	(CSubmarine* pSubmarine);

		// water cannon ptfx
		void				UpdatePtFxWaterCannonJet	(CWaterCannon* pWaterCannon, bool registeredThisFrame, s32 boneIndex, Vec3V_In vOffsetPos, Vec3V_In vVelocity, bool collOccurred, Vec3V_In vCollPosition, Vec3V_In vCollNormal, float vehSpeed);
		void				UpdatePtFxWaterCannonSpray	(CWaterCannon* pWaterCannon, Vec3V_In vPosition, Vec3V_In vNormal, Vec3V_In vDir);

		// accessors
		bool				GetBoatBowPtFxActive		()						{return(m_boatBowPtFxActive);}
		void				SetBoatBowPtFxActive		(bool b)				{m_boatBowPtFxActive=b;}

		// script overrides
		void				OverrideWaterPtFxSplashObjectInName(atHashWithStringNotFinal hashName, scrThreadId scriptThreadId);

		// debug
#if __BANK
		bool				RenderFloaterCalculatedSurfacePositions() {return m_renderFloaterCalculatedSurfacePositions;}
#endif


	private: //////////////////////////

		// vfx processing
		void 				ProcessVfxSplash			(CEntity* pParent, s32 sampleId, const VfxWaterSampleData_s* pVfxWaterSampleData, float fxDistSqr, bool isPlayerVfx);
		void 				ProcessVfxSplashPed			(CPed* pPed, s32 sampleId, const VfxWaterSampleData_s* pVfxWaterSampleData, float fxDistSqr, bool isPlayerVfx);
		void 				ProcessVfxSplashVehicle		(CVehicle* pVehicle, s32 sampleId, const VfxWaterSampleData_s* pVfxWaterSampleData, float fxDistSqr, bool isPlayerVfx);
		void 				ProcessVfxSplashObject		(CObject* pObject, s32 sampleId, const VfxWaterSampleData_s* pVfxWaterSampleData, float fxDistSqr, bool isPlayerVfx);
		
		void 				ProcessVfxBoat				(CVehicle* pVehicle, const VfxWaterSampleData_s* pVfxWaterSampleData);
		void 				ProcessVfxBoatBow			(CVehicle* pVehicle, const CVfxVehicleInfo* pVfxVehicleInfo, const VfxWaterSampleData_s* pVfxWaterSampleData);
		bool 				ProcessVfxBoatBowRow		(CVehicle* pVehicle, const CVfxVehicleInfo* pVfxVehicleInfo, const VfxWaterSampleData_s* pVfxWaterSampleData, s32 row, bool goingForward, bool doBowFx, bool doWakeFx);
		void 				ProcessVfxBoatEntry			(CVehicle* pVehicle, const CVfxVehicleInfo* pVfxVehicleInfo, const VfxWaterSampleData_s* pVfxWaterSampleData);
		void 				ProcessVfxBoatExit			(CVehicle* pVehicle, const CVfxVehicleInfo* pVfxVehicleInfo, const VfxWaterSampleData_s* pVfxWaterSampleData);

		// ped splash ptfx
		void				TriggerPtFxSplashPedIn		(CPed* pPed, const CVfxPedInfo* pVfxPedInfo, const VfxPedSkeletonBoneInfo* pSkeletonBoneInfo, const VfxPedBoneWaterInfo* pBoneWaterInfo, const VfxWaterSampleData_s* pVfxWaterSampleData, s32 boneIndex, Vec3V_In vBoneDir, float boneSpeed, bool isPlayerVfx);
		void				TriggerPtFxSplashPedOut		(CPed* pPed, const CVfxPedInfo* pVfxPedInfo, const VfxPedSkeletonBoneInfo* pSkeletonBoneInfo, s32 boneIndex, Vec3V_In vBoneDir, float boneSpeed);
		void				UpdatePtFxSplashPedWade		(CPed* pPed, const CVfxPedInfo* pVfxPedInfo, const VfxPedSkeletonBoneInfo* pSkeletonBoneInfo, const VfxPedBoneWaterInfo* pBoneWaterInfo, const VfxWaterSampleData_s* pVfxWaterSampleData, Vec3V_In vRiverVel, Vec3V_In vBoneDir, float boneSpeed, s32 ptFxOffset);
		void				UpdatePtFxSplashPedTrail	(CPed* pPed, const CVfxPedInfo* pVfxPedInfo, const VfxPedSkeletonBoneInfo* pSkeletonBoneInfo, const VfxPedBoneWaterInfo* pBoneWaterInfo, const VfxWaterSampleData_s* pVfxWaterSampleData, Vec3V_In vRiverVel, Vec3V_In vBoneDir, float boneSpeed, s32 ptFxOffset);

		// vehicle splash ptfx
		void				TriggerPtFxSplashVehicleIn	(CVehicle* pVehicle, const CVfxVehicleInfo* pVfxVehicleInfo, const VfxWaterSampleData_s* pVfxWaterSampleData, Vec3V_In vLateralDir, float lateralSpeed, float downwardSpeed);
		void				TriggerPtFxSplashVehicleOut	(CVehicle* pVehicle, const CVfxVehicleInfo* pVfxVehicleInfo, const VfxWaterSampleData_s* pVfxWaterSampleData, Vec3V_In vLateralDir, float lateralSpeed, float upwardSpeed);
		void				UpdatePtFxSplashVehicleWade	(CVehicle* pVehicle, const CVfxVehicleInfo* pVfxVehicleInfo, const VfxWaterSampleData_s* pVfxWaterSampleData, Vec3V_In vRiverVel, Vec3V_In vLateralVel, s32 ptFxOffset);
		void				UpdatePtFxSplashVehicleTrail(CVehicle* pVehicle, const CVfxVehicleInfo* pVfxVehicleInfo, const VfxWaterSampleData_s* pVfxWaterSampleData, Vec3V_In vRiverVel, Vec3V_In vLateralVel, s32 ptFxOffset);

		// object splash ptfx
		void				TriggerPtFxSplashObjectIn	(CObject* pObject, const VfxWaterSampleData_s* pVfxWaterSampleData, Vec3V_In vLateralDir, float lateralSpeed, float downwardSpeed);
		void				TriggerPtFxSplashObjectOut	(CObject* pObject, const VfxWaterSampleData_s* pVfxWaterSampleData, Vec3V_In vLateralDir, float lateralSpeed, float upwardSpeed);
		void				UpdatePtFxSplashObjectWade	(CObject* pObject, const VfxWaterSampleData_s* pVfxWaterSampleData, Vec3V_In vRiverVel, Vec3V_In vLateralVel, s32 ptFxOffset);
		void				UpdatePtFxSplashObjectTrail	(CObject* pObject, const VfxWaterSampleData_s* pVfxWaterSampleData, Vec3V_In vRiverVel, Vec3V_In vLateralVel, s32 ptFxOffset);

		// boat ptfx
		void				UpdatePtFxBoatBow			(CVehicle* pVehicle, const CVfxVehicleInfo* pVfxVehicleInfo, s32 registerOffset, const VfxWaterSampleData_s* pVfxWaterSampleData, Vec3V_In vDir, bool goingForward);
		void				UpdatePtFxBoatWash			(CVehicle* pVehicle, const CVfxVehicleInfo* pVfxVehicleInfo, s32 registerOffset, const VfxWaterSampleData_s* pVfxWaterSampleData);
		void				TriggerPtFxBoatEntry		(CVehicle* pVehicle, const CVfxVehicleInfo* pVfxVehicleInfo, Vec3V_In vPos);
		void				TriggerPtFxBoatExit			(CVehicle* pVehicle, const CVfxVehicleInfo* pVfxVehicleInfo, Vec3V_In vPos);

		// debug
#if __BANK
		void				RenderDebugSplashes			(const VfxWaterSampleData_s* pVfxWaterSampleData, bool goneIntoWater, bool comeOutOfWater, bool partiallyInWater, bool fullyInWater);
#endif


	///////////////////////////////////
	// VARIABLES
	///////////////////////////////////

	private: //////////////////////////

		bool				m_boatBowPtFxActive;

		// script overrides
		atHashWithStringNotFinal m_waterPtFxOverrideSplashObjectInName;
		scrThreadId m_waterPtFxOverrideSplashObjectInNameScriptThreadId;

		// debug variables
#if __BANK
		bool				m_disableSplashPedLod;
		bool				m_disableSplashPedEntry;
		bool				m_disableSplashPedIn;
		bool				m_disableSplashPedOut;
		bool				m_disableSplashPedWade;
		bool				m_disableSplashPedTrail;

		bool				m_disableSplashVehicleIn;
		bool				m_disableSplashVehicleOut;
		bool				m_disableSplashVehicleWade;
		bool				m_disableSplashVehicleTrail;

		bool				m_disableSplashObjectIn;
		bool				m_disableSplashObjectOut;
		bool				m_disableSplashObjectWade;
		bool				m_disableSplashObjectTrail;

		bool				m_disableBoatBow;
		bool				m_disableBoatWash;
		bool				m_disableBoatEntry;
		bool				m_disableBoatExit;
		bool				m_disableBoatProp;

		float				m_overrideBoatBowScale;
		float				m_overrideBoatWashScale;
		float				m_overrideBoatEntryScale;
		float				m_overrideBoatExitScale;
		float				m_overrideBoatPropScale;

		bool				m_outputSplashDebug;
		bool				m_renderSplashPedDebug;
		bool				m_renderSplashVehicleDebug;
		bool				m_renderSplashObjectDebug;
		bool				m_renderBoatDebug;
		bool				m_renderDebugAtSurfacePos;
		bool				m_renderFloaterCalculatedSurfacePositions;
#endif

}; // CVfxWater




///////////////////////////////////////////////////////////////////////////////
//  EXTERNS
///////////////////////////////////////////////////////////////////////////////

extern	CVfxWater	g_vfxWater;


#endif // VFX_WATER_H





