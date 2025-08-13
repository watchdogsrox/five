///////////////////////////////////////////////////////////////////////////////
//  
//	FILE: 	VfxVehicle.h
//	BY	: 	Mark Nicholson
//	FOR	:	Rockstar North
//	ON	:	01 June 2006
//	WHAT:	
//
///////////////////////////////////////////////////////////////////////////////

#ifndef VFX_VEHICLE_H
#define	VFX_VEHICLE_H


///////////////////////////////////////////////////////////////////////////////
//  INCLUDES
///////////////////////////////////////////////////////////////////////////////													

// rage
#include "script/thread.h"
#include "vectormath/classes.h"

// game
#include "ModelInfo/ModelInfo.h"
#include "ModelInfo/ModelInfo_Factories.h"
#include "Physics/Floater.h"
#include "Renderer/HierarchyIds.h"
#include "Vehicles/Planes.h"
#include "Vehicles/Vehicle.h"


///////////////////////////////////////////////////////////////////////////////
//  FORWARD DECLARATIONS
///////////////////////////////////////////////////////////////////////////////	

namespace rage
{
}

class CBoat;
class CEntity;
class CPed;
class CPlane;
class CRotaryWingAircraft;


///////////////////////////////////////////////////////////////////////////////
//  DEFINES
///////////////////////////////////////////////////////////////////////////////													

#define	VFXVEHICLE_MAX_DOWNWASH_PROBES		(8)
#define	VFXVEHICLE_MAX_GROUND_DISTURB_PROBES (8)

#define VFXVEHICLE_NUM_ROCKET_BOOSTS		(8)

#define	VFXRANGE_VEH_LIGHT_SMASH			(20.0f)
#define	VFXRANGE_VEH_LIGHT_SMASH_SQR		(VFXRANGE_VEH_LIGHT_SMASH*VFXRANGE_VEH_LIGHT_SMASH)

#define	VFXRANGE_VEH_SLIPSTREAM				(30.0f)
#define	VFXRANGE_VEH_SLIPSTREAM_SQR			(VFXRANGE_VEH_SLIPSTREAM*VFXRANGE_VEH_SLIPSTREAM)

#define	VFXRANGE_VEH_RESPRAY				(60.0f)
#define	VFXRANGE_VEH_RESPRAY_SQR			(VFXRANGE_VEH_RESPRAY*VFXRANGE_VEH_RESPRAY)

#define	VFXRANGE_VEH_HELI_ROTOR_DESTROY		(70.0f)
#define	VFXRANGE_VEH_HELI_ROTOR_DESTROY_SQR	(VFXRANGE_VEH_HELI_ROTOR_DESTROY*VFXRANGE_VEH_HELI_ROTOR_DESTROY)

#define	VFXRANGE_VEH_PLANE_PROP_DESTROY		(100.0f)
#define	VFXRANGE_VEH_PLANE_PROP_DESTROY_SQR	(VFXRANGE_VEH_PLANE_PROP_DESTROY*VFXRANGE_VEH_PLANE_PROP_DESTROY)

#define	VFXRANGE_VEH_TRAIN_SPARKS			(40.0f)
#define	VFXRANGE_VEH_TRAIN_SPARKS_SQR		(VFXRANGE_VEH_TRAIN_SPARKS*VFXRANGE_VEH_TRAIN_SPARKS)

#define	VFXRANGE_VEH_FLAMING_PART			(40.0f)
#define	VFXRANGE_VEH_FLAMING_PART_SQR		(VFXRANGE_VEH_FLAMING_PART*VFXRANGE_VEH_FLAMING_PART)

#define	VFXRANGE_VEH_NITROUS				(50.0f)
#define	VFXRANGE_VEH_NITROUS_SQR			(VFXRANGE_VEH_NITROUS*VFXRANGE_VEH_NITROUS)

#define	VFXRANGE_VEH_EJECTOR_SEAT			(70.0f)
#define	VFXRANGE_VEH_EJECTOR_SEAT_SQR		(VFXRANGE_VEH_EJECTOR_SEAT*VFXRANGE_VEH_EJECTOR_SEAT)

#define	VFXRANGE_VEH_JUMP					(70.0f)
#define	VFXRANGE_VEH_JUMP_SQR				(VFXRANGE_VEH_JUMP*VFXRANGE_VEH_JUMP)

#define	VFXRANGE_VEH_THRUSTER_JET			(30.0f)
#define	VFXRANGE_VEH_THRUSTER_JET_SQR		(VFXRANGE_VEH_THRUSTER_JET*VFXRANGE_VEH_THRUSTER_JET)

#define	VFXRANGE_VEH_GLIDE_HAZE				(50.0f)
#define	VFXRANGE_VEH_GLIDE_HAZE_SQR			(VFXRANGE_VEH_GLIDE_HAZE*VFXRANGE_VEH_GLIDE_HAZE)

#define	VFXRANGE_VEH_WEAPON_CHARGE			(50.0f)
#define	VFXRANGE_VEH_WEAPON_CHARGE_SQR		(VFXRANGE_VEH_WEAPON_CHARGE*VFXRANGE_VEH_WEAPON_CHARGE)

#define VFXRANGE_VEH_ELECTRIC_RAM_BAR		(30.0f)
#define VFXRANGE_VEH_ELECTRIC_RAM_BAR_SQR	(VFXRANGE_VEH_ELECTRIC_RAM_BAR * VFXRANGE_VEH_ELECTRIC_RAM_BAR)


///////////////////////////////////////////////////////////////////////////////
//  ENUMS
///////////////////////////////////////////////////////////////////////////////	

// vehicle ptfx offsets for registered systems that play on the same vehicle
enum PtFxVehicleOffsets_e
{
	PTFXOFFSET_VEHICLE_ENGINE_DAMAGE_PANEL_OPEN					= 0,
	PTFXOFFSET_VEHICLE_ENGINE_DAMAGE_PANEL_SHUT_OR_NO_PANEL,
	PTFXOFFSET_VEHICLE_ENGINE_DAMAGE_PANEL_SHUT_OR_NO_PANEL_2,
	PTFXOFFSET_VEHICLE_OVERTURNED_SMOKE,
	PTFXOFFSET_VEHICLE_FIRE_PETROL_TANK,
	PTFXOFFSET_VEHICLE_FIRE_PETROL_TANK_2,
	PTFXOFFSET_VEHICLE_OIL_LEAK,
	PTFXOFFSET_VEHICLE_PETROL_LEAK,
	PTFXOFFSET_VEHICLE_EXHAUST,
	PTFXOFFSET_VEHICLE_EXHAUST_LAST								= PTFXOFFSET_VEHICLE_EXHAUST + (VEH_LAST_EXHAUST-VEH_FIRST_EXHAUST),
	PTFXOFFSET_VEHICLE_ENGINE_STARTUP,
	PTFXOFFSET_VEHICLE_ENGINE_STARTUP_LAST						= PTFXOFFSET_VEHICLE_ENGINE_STARTUP + (VEH_LAST_EXHAUST-VEH_FIRST_EXHAUST),
	PTFXOFFSET_VEHICLE_MISFIRE,
	PTFXOFFSET_VEHICLE_MISFIRE_LAST								= PTFXOFFSET_VEHICLE_MISFIRE + (VEH_LAST_EXHAUST-VEH_FIRST_EXHAUST),
	PTFXOFFSET_VEHICLE_ROCKET_BOOST,
	PTFXOFFSET_VEHICLE_PLANE_ROCKET_BOOST_LAST					= PTFXOFFSET_VEHICLE_ROCKET_BOOST + VFXVEHICLE_NUM_ROCKET_BOOSTS,
	PTFXOFFSET_VEHICLE_SLIPSTREAM,
	PTFXOFFSET_VEHICLE_SLIPSTREAM_2,
	PTFXOFFSET_VEHICLE_LIGHT_TRAIL,
	PTFXOFFSET_VEHICLE_LIGHT_TRAIL_2,
	PTFXOFFSET_VEHICLE_PLANE_AFTERBURNER,
	PTFXOFFSET_VEHICLE_PLANE_AFTERBURNER_LAST					= PTFXOFFSET_VEHICLE_PLANE_AFTERBURNER + PLANE_NUM_AFTERBURNERS - 1,
	PTFXOFFSET_VEHICLE_PLANE_WINGTIP,
	PTFXOFFSET_VEHICLE_PLANE_WINGTIP_2,
	PTFXOFFSET_VEHICLE_PLANE_DAMAGE_FIRE,
	PTFXOFFSET_VEHICLE_PLANE_DAMAGE_FIRE_LAST					= PTFXOFFSET_VEHICLE_PLANE_DAMAGE_FIRE + CAircraftDamage::NUM_DAMAGE_FLAMES,
	PTFXOFFSET_VEHICLE_PLANE_GROUND_DISTURB,
	PTFXOFFSET_VEHICLE_AIRCRAFT_SECTION_DAMAGE_SMOKE,
	PTFXOFFSET_VEHICLE_AIRCRAFT_SECTION_DAMAGE_SMOKE_LAST		= PTFXOFFSET_VEHICLE_AIRCRAFT_SECTION_DAMAGE_SMOKE + CAircraftDamage::NUM_DAMAGE_SECTIONS,
	PTFXOFFSET_VEHICLE_DOWNWASH,
	PTFXOFFSET_VEHICLE_DOWNWASH_2,
	PTFXOFFSET_VEHICLE_TRAIN_SPARKS,
	PTFXOFFSET_VEHICLE_TRAIN_SPARKS_2,
	PTFXOFFSET_VEHICLE_TRAIN_SPARKS_3,
	PTFXOFFSET_VEHICLE_BOAT_BOW_L,
	PTFXOFFSET_VEHICLE_BOAT_BOW_L_2,
	PTFXOFFSET_VEHICLE_BOAT_BOW_L_3,
	PTFXOFFSET_VEHICLE_BOAT_BOW_L_4,
	PTFXOFFSET_VEHICLE_BOAT_BOW_L_5,
	PTFXOFFSET_VEHICLE_BOAT_BOW_R,
	PTFXOFFSET_VEHICLE_BOAT_BOW_R_2,
	PTFXOFFSET_VEHICLE_BOAT_BOW_R_3,
	PTFXOFFSET_VEHICLE_BOAT_BOW_R_4,
	PTFXOFFSET_VEHICLE_BOAT_BOW_R_5,
	PTFXOFFSET_VEHICLE_BOAT_WASH_L,
	PTFXOFFSET_VEHICLE_BOAT_WASH_R,
	PTFXOFFSET_VEHICLE_PROPELLER,
	PTFXOFFSET_VEHICLE_PROPELLER_LAST							= PTFXOFFSET_VEHICLE_PROPELLER + SUB_NUM_PROPELLERS,
	PTFXOFFSET_VEHICLE_WADE,
	PTFXOFFSET_VEHICLE_WADE_LAST								= PTFXOFFSET_VEHICLE_WADE + MAX_WATER_SAMPLES,
	PTFXOFFSET_VEHICLE_TRAIL,
	PTFXOFFSET_VEHICLE_TRAIL_LAST								= PTFXOFFSET_VEHICLE_TRAIL + MAX_WATER_SAMPLES,
	PTFXOFFSET_VEHICLE_WHEEL_DISP_LOD_FRONT,
	PTFXOFFSET_VEHICLE_WHEEL_DISP_LOD_REAR,
	PTFXOFFSET_VEHICLE_BOAT_WAKE_LOD_FRONT,
	PTFXOFFSET_VEHICLE_BOAT_WAKE_LOD_REAR,
	PTFXOFFSET_VEHICLE_SUBMARINE_DIVE,
	PTFXOFFSET_VEHICLE_SUBMARINE_AIR_LEAK,
	PTFXOFFSET_VEHICLE_PLANE_SMOKE,
	PTFXOFFSET_VEHICLE_THRUSTER,
	PTFXOFFSET_VEHICLE_THRUSTER_2,
	PTFXOFFSET_VEHICLE_GLIDE_HAZE,
	PTFXOFFSET_VEHICLE_EXTRA,
	PTFXOFFSET_VEHICLE_EXTRA_LAST								= PTFXOFFSET_VEHICLE_EXTRA + (VEH_LAST_EXTRA-VEH_EXTRA_1+1),
	PTFXOFFSET_VEHICLE_WEAPON_CHARGE,
	PTFXOFFSET_VEHICLE_ELECTRIC_RAM_BAR,
	PTFXOFFSET_VEHICLE_NUM
};
CompileTimeAssert(PTFXOFFSET_VEHICLE_NUM<sizeof(CVehicle));


///////////////////////////////////////////////////////////////////////////////
//  STRUCTURES
///////////////////////////////////////////////////////////////////////////////	

struct VfxVehicleDownwashProbe_s
{
	Vec3V m_vStartPos;
	Vec3V m_vEndPos;
	Vec3V m_vDir;
	RegdVeh m_regdVeh;
	CVfxVehicleInfo* m_pVfxVehicleInfo;
	float m_downforceEvo;
	u32 m_ptFxId;
	
	s8 m_vfxProbeId;
};

struct VfxVehicleGroundDisturbProbe_s
{
	Vec3V m_vStartPos;
	Vec3V m_vEndPos;
	RegdVeh m_regdVeh;
	CVfxVehicleInfo* m_pVfxVehicleInfo;

	s8 m_vfxProbeId;
};

struct VfxVehicleDummyMtlIdProbe_s
{
	Vec3V m_vStartPos;
	Vec3V m_vEndPos;
	RegdVeh m_regdVeh;

	s8 m_vfxProbeId;
};

struct VfxVehicleRecentlyDamagedByPlayerInfo_s
{
	RegdVeh m_regdVeh;
	u32 m_timeLastDamagedMs;
};

struct VfxVehicleRecentlyCrashedInfo_s
{
	RegdVeh m_regdVeh;
	u32 m_timeLastCrashedMs;
	float m_damageApplied;
	bool m_petrolIgniteTestDone;
};


///////////////////////////////////////////////////////////////////////////////
//  CLASSES
///////////////////////////////////////////////////////////////////////////////

class CVfxVehicleAsyncProbeManager
{	

	///////////////////////////////////
	// FUNCTIONS 
	///////////////////////////////////

	public: ///////////////////////////

		void Init();
		void Shutdown();
		void Update();

		void TriggerDownwashProbe(Vec3V_In vStartPos, Vec3V_In vEndPos, Vec3V_In vDir, CVehicle* pVehicle, CVfxVehicleInfo* pVfxVehicleInfo, float downforceEvo, u32 ptFxId);
		void TriggerGroundDisturbProbe(Vec3V_In vStartPos, Vec3V_In vEndPos, CVehicle* pVehicle, CVfxVehicleInfo* pVfxVehicleInfo);
		void TriggerDummyMtlIdProbe(Vec3V_In vStartPos, Vec3V_In vEndPos, CVehicle* pVehicle);


	private: //////////////////////////



	///////////////////////////////////
	// VARIABLES
	///////////////////////////////////

	private: //////////////////////////

		VfxVehicleDownwashProbe_s m_downwashProbes				[VFXVEHICLE_MAX_DOWNWASH_PROBES];
		VfxVehicleGroundDisturbProbe_s m_groundDisturbProbes	[VFXVEHICLE_MAX_GROUND_DISTURB_PROBES];
		VfxVehicleDummyMtlIdProbe_s m_dummyMtlIdProbe;
		u32	m_dummyMtlIdProbeCurrPoolIdx;						// current index into the vehicle pool - used for updating the dummy vehicle material ids
		u32 m_dummyMtlIdProbeFrameDelay;						// the frame delay between a dummy mtl id probe finishing and the next one starting		
};


class CVfxVehicle
{	
	friend class CVfxVehicleAsyncProbeManager;

	///////////////////////////////////
	// FUNCTIONS 
	///////////////////////////////////

	public: ///////////////////////////

		// main interface
		void				Init						(unsigned initMode);
		void				Shutdown					(unsigned shutdownMode);

		void				Update						();

		void				RemoveScript				(scrThreadId scriptThreadId);

		// processing (engine damage)
		void 				ProcessEngineDamage			(CVehicle* pVehicle);
		void 				ProcessEngineDamageNoPanel	(CVehicle* pVehicle, CVfxVehicleInfo* pVfxVehicleInfo);
		void 				ProcessEngineDamagePanel	(CVehicle* pVehicle, CVfxVehicleInfo* pVfxVehicleInfo);
		void				ProcessWreckedFires			(CVehicle* pVehicle, CPed* pCulprit, s32 numGenerations);

		// processing (disturbance)
		void				ProcessGroundDisturb		(CVehicle* pVehicle);
		void				ProcessPlaneDownwash		(CPlane* pPlane);
		void				ProcessHeliDownwash			(CRotaryWingAircraft* pAircraft);

		// processing (low lod)
		void				ProcessLowLodWheels			(CVehicle* pVehicle);
		void				ProcessLowLodBoatWake		(CBoat* pBoat);

		// particle systems (exhausts)
		void				UpdatePtFxExhaust			(CVehicle* pVehicle);
		void				UpdatePtFxNitrous			(CVehicle* pVehicle);
		void				UpdatePtFxEngineStartup		(CVehicle* pVehicle);
		void				UpdatePtFxMisfire			(CVehicle* pVehicle);
		void				UpdatePtFxRocketBoost		(CVehicle* pVehicle);
		void				TriggerPtFxBackfire			(CVehicle* pVehicle, float bangerEvo);

		// particle systems (petrol tanks)
		void 				ProcessPetrolTankDamage		(CVehicle* pVehicle, CPed* pCullprit);

		// particle systems (lights)
		void				UpdatePtFxLightTrail		(CVehicle* pVehicle, const int boneIndex, s32 id);
		void				TriggerPtFxLightSmash		(CVehicle *pVehicle, const int boneId, Vec3V_In vPos, Vec3V_In vNormal);

		// particle systems (aircraft)
		void				UpdatePtFxAircraftSectionDamageSmoke(CVehicle* pVehicle, Vec3V_In vPosLcl, s32 id);

		// particle systems (planes)
		void				UpdatePtFxPlaneAfterburner	(CVehicle* pVehicle, eHierarchyId boneTag, int ptFxId);
		void				UpdatePtFxPlaneWingTips		(CPlane* pPlane);
		void				UpdatePtFxPlaneDamageFire	(CPlane* pPlane, Vec3V_In vPosLcl, float currTime, float maxTime, s32 id);
		void				UpdatePtFxPlaneSmoke		(CVehicle* pVehicle, Color32 colour);	
		void				TriggerPtFxPropellerDestroy	(CPlane* pPlane, s32 boneIndex);

		// particle systems (heli)
		void				TriggerPtFxHeliRotorDestroy	(CRotaryWingAircraft* pAircraft, bool isTailRotor);

		// particle systems (train)
		void				UpdatePtFxTrainWheelSparks	(CVehicle* pVehicle, s32 boneIndex, Vec3V_In vWheelOffset, const float squealEvo, s32 wheelId, bool isLeft);

		// particle systems (weapons)
		void				UpdatePtFxVehicleWeaponCharge(CVehicle* pVehicle, s32 boneIndex, float chargeEvo);

		// particle systems (misc)
		void				UpdatePtFxSlipstream		(CVehicle* pVehicle, float slipstreamEvo);
		void				UpdatePtFxExtra				(CVehicle* pVehicle, const CVfxExtraInfo* pVfxExtraInfo, u32 id);
		void				UpdatePtFxThrusterJet		(CVehicle* pVehicle, s32 boneIndex, s32 fxId, float thrustEvo);
		void				UpdatePtFxGlideHaze			(CVehicle* pVehicle, atHashWithStringNotFinal ptFxHashName);
		void				UpdatePtFxElectricRamBar	(CVehicle* pVehicle);
		void				TriggerPtFxJump				(CVehicle* pVehicle);
		void				TriggerPtFxRespray			(CVehicle* pVehicle, Vec3V_In vFxPos, Vec3V_In vFxDir);
		void				TriggerPtFxVehicleDebris	(CEntity* pEntity);
		void				TriggerPtFxEjectorSeat		(CVehicle* pVehicle);

		// recently damaged by player accessors
		void				SetRecentlyDamagedByPlayer	(CVehicle* pVehicle);
		bool				HasRecentlyBeenDamagedByPlayer(const CVehicle* pVehicle);

		// recently crashed accessors
		void				SetRecentlyCrashed			(CVehicle* pVehicle, float damageApplied);
		bool				HasRecentlyCrashed			(const CVehicle* pVehicle, u32 timeMs, float damage);

		void				SetDownwashPtFxDisabled		(bool val, scrThreadId scriptThreadId);
		void				SetSlipstreamPtFxLodRangeScale(float val, scrThreadId scriptThreadId);


		// debug functions
#if __BANK
		void				InitWidgets					();
		float				GetTrainSquealOverride		()						{return m_trainSquealOverride;}
#endif


	private: //////////////////////////

		// processing
		void				ProcessOverturnedSmoke		(CVehicle* pVehicle, CVfxVehicleInfo* pVfxVehicleInfo);
		void				ProcessOilLeak				(CVehicle* pVehicle, CVfxVehicleInfo* pVfxVehicleInfo);
		void				ProcessPetrolLeak			(CVehicle* pVehicle, CVfxVehicleInfo* pVfxVehicleInfo);
		void				ProcessDownwash				(CVehicle* pVehicle, CVfxVehicleInfo* pVfxVehicleInfo, Vec3V_In vPos, Vec3V_In vDir, float downforceEvo, u32 ptFxId);

		// particle systems (leaks)
		void				UpdatePtFxEngineOilLeak		(CVehicle* pVehicle, CVfxVehicleInfo* pVfxVehicleInfo, float oilDamage);
		void				UpdatePtFxEnginePetrolLeak	(CVehicle* pVehicle, CVfxVehicleInfo* pVfxVehicleInfo, float petrolTankDamage);
		void				UpdatePtFxPetrolSpray		(CVehicle* pVehicle, CVfxVehicleInfo* pVfxVehicleInfo, float petrolTankDamage);

		// particle systems (engine damage)
		void				UpdatePtFxEngineNoPanel		(CVehicle* pVehicle, CVfxVehicleInfo* pVfxVehicleInfo, s32 boneIndex, s32 fxId, float speedEvo, float damageEvo, float fireEvo);
		void				UpdatePtFxEnginePanelOpen	(CVehicle* pVehicle, CVfxVehicleInfo* pVfxVehicleInfo, s32 boneIndex, float speedEvo, float damageEvo, float panelEvo, float fireEvo);
		void				UpdatePtFxEnginePanelShut	(CVehicle* pVehicle, CVfxVehicleInfo* pVfxVehicleInfo, s32 boneIndex, s32 fxId, float speedForwardEvo, float speedBackwardEvo, float damageEvo, float fireEvo);

		void				UpdatePtFxOverturnedSmoke	(CVehicle* pVehicle, CVfxVehicleInfo* pVfxVehicleInfo);

		// particle systems (low lod)
		void				UpdatePtFxLowLodWheels		(CVehicle* pVehicle, VfxWheelInfo_s* pVfxWheelInfo, Vec3V_In vPosLcl, float vehSpeed, bool travellingForwards);
		void				UpdatePtFxLowLodBoatWake	(CBoat* pBoat, CVfxVehicleInfo* pVfxVehicleInfo, Vec3V_In vPosLcl, float vehSpeed, bool travellingForwards);

		// particle systems (misc)
		void				UpdatePtFxVehicleGroundDisturb(CVehicle* pVehicle, CVfxVehicleInfo* pVfxVehicleInfo, Vec3V_In vPos, Vec3V_In vNormal, float distEvo, VfxDisturbanceType_e vfxDisturbanceType);
		void				UpdatePtFxDownwash			(CVehicle* pVehicle, CVfxVehicleInfo* pVfxVehicleInfo, Vec3V_In vPos, Vec3V_In vNormal, float downforceEvo, float distEvo, VfxDisturbanceType_e vfxDisturbanceType, u32 ptFxId);

		void 				CalcDamageAndFireEvos		(CVehicle* pVehicle, float engineHealth, float& damageEvo, float& fireEvo);

		// 
		float				GetEngineDamageHealthThresh	(CVehicle* pVehicle);


	///////////////////////////////////
	// VARIABLES
	///////////////////////////////////

	private: //////////////////////////

		CVfxVehicleAsyncProbeManager m_asyncProbeMgr;

		atArray<VfxVehicleRecentlyDamagedByPlayerInfo_s> m_recentlyDamagedByPlayerInfos;
		atArray<VfxVehicleRecentlyCrashedInfo_s> m_recentlyCrashedInfos;

		scrThreadId			m_downwashPtFxDisabledScriptThreadId;
		bool				m_downwashPtFxDisabled;

		scrThreadId			m_slipstreamPtFxLodRangeScaleScriptThreadId;
		float				m_slipstreamPtFxLodRangeScale;

		// debug variables
#if __BANK
		bool				m_bankInitialised;
		bool				m_renderDownwashVfxProbes;
		bool				m_renderPlaneGroundDisturbVfxProbes;
		bool				m_renderDummyMtlIdVfxProbes;
		float				m_trainSquealOverride;
#endif 

}; // CVfxVehicle


///////////////////////////////////////////////////////////////////////////////
//  EXTERNS
///////////////////////////////////////////////////////////////////////////////

extern	CVfxVehicle			g_vfxVehicle;

extern	dev_float			VFXVEHICLE_RECENTLY_CRASHED_DAMAGE_THRESH;
extern	dev_u32				VFXVEHICLE_NUM_WRECKED_FIRES_FADE_THRESH;


#endif // VFX_VEHICLE_H




