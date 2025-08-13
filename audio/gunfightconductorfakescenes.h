// 
// audio/gunfightconductorfakescenes.h
// 
// Copyright (C) 1999-2006 Rockstar Games.  All Rights Reserved.
// 

#ifndef AUD_GUNFIGHTCONDUCTORFAKESCENES_H
#define AUD_GUNFIGHTCONDUCTORFAKESCENES_H

#include "audioengine/entity.h"
#include "audioengine/soundset.h"
#include "renderer/HierarchyIds.h"

#include "dynamicmixer.h"
#include "fwsys/fsm.h"
#include "scene/RegdRefTypes.h"
#include "audioentity.h"

#define AUD_MAX_POST_SHOOT_OUT_BUILDINGS 5


class audGunFightConductorFakeScenes : public fwFsm,naAudioEntity
{
private:
	enum audFakedImpactsEntity
	{
		AUD_OBJECT = 0,
		AUD_VEHICLE,
		AUD_FAKED_IMPACTS_HISTORY_SIZE
	};
	// FSM States
	enum 
	{
		State_Idle  = 0,
		State_EmphasizePlayerIntoCoverScene,
		State_EmphasizeRunAwayScene,
		State_EmphasizeOpenSpaceScene,
	};
	enum typeOfFakedBulletImpacts
	{
		Invalid_Impact = -1,
		Single_Impact = 0,
		Burst_Of_Impacts,
		Hard_Impact,
		Bunch_Of_Impacts,
		Max_BulletImpactTypes = 5,
	};

	struct PostShootOutBuidlingInfo
	{
		RegdEnt building;
		u32 bulletHitCount;
		u32 timeSinceLastBulletHit;
	};
public:
	AUDIO_ENTITY_NAME(audGunFightConductorFakeScenes);

	void												Init();
	virtual void										PreUpdateService(u32 timeInMs);
	void												ProcessUpdate();
	void												Reset();

	void												EmphasizePlayerIntoCover();
	void												HandlePlayerExitCover();

	void												PlayFakeRicochets(CEntity *hitEntity,const Vector3 &hitPosition);
	void												PlayPostShootOutVehDebris(CEntity *hitEntity,const Vector3 &hitPosition);
	void												PlayPostShootOutBuildingDebris(CEntity *hitEntity,const Vector3 &hitPosition);

	f32													GetBuildingDistThresholdToPlayPostShootOut() {return sm_BuildDistThresholdToPlayPostShootOut;}
	u32													GetTimeToResetBulletCount() {return sm_TimeToResetBulletCount;}

	bool												IsReady(){return m_StateProcessed;}
	bool												IsFakingBulletImpacts(){ return m_FakeBulletImpactSound != NULL;}
	bool												IsFakingRicochets(){ return m_FakeRicochetsSound != NULL;}
	
	
	BANK_ONLY(static void								AddWidgets(bkBank &bank););
	BANK_ONLY(static bool					 			sm_ForceOpenSpaceScene;);
	BANK_ONLY(static bool								sm_FakeBulletImpacts;);

private:
	virtual fwFsm::Return								UpdateState(const s32 state, const s32 iMessage, const fwFsm::Event event);
	// Handlers
	fwFsm::Return										Idle();
	fwFsm::Return										EmphasizePlayerIntoCoverScene();
	fwFsm::Return										EmphasizeRunAwayScene();
	fwFsm::Return										EmphasizeOpenSpaceScene();

	//Helpers
	void												DeEmphasizeRunAwayScene();
	void												DeEmphasizeOpenSpaceScene();
	void												EmphasizePlayerRunningAway();
	void												TriggerOpenSpaceScene();
	void												FakeVehicleVfx(bool runningAwayScene = false);
	void												FakeVehicleSmashLights(eHierarchyId nLightId);
	void												FakeVehicleSmashWindow(eHierarchyId nWindowId);
	void												FakeVehicleTyreBurst(eHierarchyId nWheelId);
	void												HandleRunAwayScene();
	void												PlayObjectsFakedImpacts(bool useMACSmaterial = false);
	void												PlayVehiclesFakedImpacts();
	void												PlayGroundFakedImpacts();

	void												UpdateFakedVehicleVfx(bool runningAwayScene = false);
	void												UpdatePostShootOutAndRicochets();
	void												UpdateFakeRicochets();
	void												UpdatePostShootOutVehDebris();
	void												UpdatePostShootOutBuildingDebris();
#if __BANK
	void												GunfightConductorFSDebugInfo();
	const char*											GetStateName(s32 iState) const ;
#endif
	audFakedImpactsEntity								AnalizeFakedImpactsHistory();

	CObject*											GetClosestValidObject();
	CollisionMaterialSettings*							GetGroundMaterial() const;
	CollisionMaterialSettings*							GetMaterialBehind() const;
	GunfightConductorIntensitySettings *				GetGunfightConductroIntensitySettings();

	Vector3												CalculateFakeGroundBIPosition();
	f32													CalculateOpenSpaceConfidence();

	s32													GetLightIdx(eHierarchyId nLightId);

	bool												ComputeLastShotTimeAndFakedBulletImpactType(u32 timeSinceLastShot);
	bool												FakeRunningAway();
	bool												FakeVehicleSmashSirens();
	bool												HandleOpenSpaceScene();
	//PURPOSE
	//Supports audDynamicEntitySound queries			
	u32													QuerySoundNameFromObjectAndField(const u32 *objectRefs, u32 numRefs);

	Vector3												m_CandidateHitPosition;
	Vector3												m_PostShootOutVehCandidateHitPosition;
	Vector3												m_PostShootOutBuildingCandidateHitPosition;

	PostShootOutBuidlingInfo							m_PostShootOutBuildingCandidate;
	audSoundSet											m_PostShootOutSounds;

	atRangeArray<PostShootOutBuidlingInfo,AUD_MAX_POST_SHOOT_OUT_BUILDINGS>	m_PostShootOutBuilding;  
	atRangeArray<u32,AUD_FAKED_IMPACTS_HISTORY_SIZE>	m_FakedImpactsHistory;  
	RegdObj												m_ObjectToFake;
	RegdVeh												m_VehicleToFake;
	CollisionMaterialSettings*							m_CurrentFakedCollisionMaterial;
	typeOfFakedBulletImpacts							m_typeOfFakedBulletImpacts;

	audSound											*m_FakeBulletImpactSound;
	audSound											*m_FakeRicochetsSound;
	audSound											*m_PostShootOutVehDebrisSound;
	audSound											*m_PostShootOutBuildingDebrisSound;
	RegdEnt												m_Candidate;
	RegdVeh												m_PostShootOutVehCandidate;
	audCurve 											m_PedsInCombatToFakedBulletImpactsProbability;
	audCurve											m_DistanceToFakeRicochetsPredelay;

	u32													m_TimeSinceLastCandidateHit;
	u32													m_TimeFiring;
	u32													m_TimeNotFiring;
	u32													m_OffScreenTimeOut;
	bool												m_HasToTriggerGoingIntoCover;
	bool												m_FakeObjectWhenOffScreen;
	bool												m_HasToPlayPostShootOutVehDebris;
	bool												m_HasToPlayPostShootOutBuildingDebris;
	bool												m_HasToPlayFakeRicochets;
	bool												m_WasSprinting;
	bool												m_StateProcessed;
	s16													m_LastState; 

	static f32	 										sm_TimeSinceLastFakedBulletImpRunningAway;
	static f32	 										sm_BuildDistThresholdToPlayPostShootOut;
	static f32	 										sm_VehSqdDistanceToPlayFarDebris;
	static f32	 										sm_BuildingSqdDistanceToPlayFarDebris;
	static f32	 										sm_MinDistanceToFakeRicochets;
	static f32	 										sm_OpenSpaceConfidenceThreshold;
	static f32	 										sm_LifeThresholdToFakeRunAwayScene;
	static f32											sm_DtToPlayFakedBulletImp;
	static f32											sm_DtToPlayFakedBulletImpRunningAway;
	static f32											sm_DtToPlayFakedBulletImpOpenSpace;
	static f32											sm_RSphereNearByEntities;
	static f32											sm_MaterialTestLenght;
	static u32											sm_TimeToResetBulletCount;
	static u32	 										sm_MaxTimeToFakeRicochets;
	static u32	 										sm_MinTimeFiringToFakeRicochets;
	static u32	 										sm_BulletHitsToTriggerVehicleDebris;
	static u32	 										sm_BulletHitsToTriggerBuildingDebris;
	static u32	 										sm_MinTimeNotFiringToFakeNearVehDebris;
	static u32	 										sm_MinTimeNotFiringToFakeNearLightVehDebris;
	static u32	 										sm_MinTimeNotFiringToFakeFarVehDebris;
	static u32	 										sm_MinTimeNotFiringToFakeNearBuildingDebris;
	static u32	 										sm_MinTimeNotFiringToFakeFarBuildingDebris;
	static u32	 										sm_TimeToSmashSirens;
	static u32	 										sm_TimeSinceLastSirenSmashed;
	static u32	 										sm_MaxOffScreenTimeOut;
	static s32	 										sm_FakedEntityToPlayProbThreshold;
	static s32	 										sm_CarPriorityOverObjects;

	static bool 										sm_AllowOpenSpaceWhenRunningAway;
	static bool 										sm_ForceRunAwayScene;
	static bool											sm_AllowRunningAwayScene;
#if __BANK
	static bool 										sm_DrawMaterialBehindTest;
	static bool 										sm_DrawNearByDetectionSphere;
	static bool 										sm_DontCheckOffScreen;
	static bool 										sm_DisplayDynamicSoundsPlayed;
	static bool 										sm_DrawResultPosition;
	static bool 										sm_ShowFakeRiccoPos;
	static bool 										sm_ShowPostShootOutPos;
	static s32											sm_FakeBulletImpactType;
	static const char* 									sm_FakeBulletImpactTypes[Max_BulletImpactTypes];
#endif // __BANK
};

#endif