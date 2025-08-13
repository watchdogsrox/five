// 
// audio/vehicleconductor.h
// 
// Copyright (C) 1999-2006 Rockstar Games.  All Rights Reserved.
// 

#ifndef AUD_VEHICLECONDUCTOR_H
#define AUD_VEHICLECONDUCTOR_H


#include "asyncprobes.h"
#include "conductorintensitymap.h"
#include "conductorsutil.h"
#include "vehicleconductordynamicmixing.h"
#include "vehicleconductorfakescenes.h"
#include "scene/RegdRefTypes.h"

#include "fwsys/fsm.h"

#define NUMBER_OF_TEST 4
class audCachedCarLandingTestResults : public audCachedMultipleTestResults
{
public:
	virtual void ClearAll();
	virtual bool AreAllComplete();
	virtual bool NewTestComplete();
	virtual void GetAllProbeResults();
	audCachedLos m_MainLineTest[NUMBER_OF_TEST];
};

#define AUD_MAX_NUM_VEH_WITH_SIREN 64
class audVehicleConductor  : public fwFsm
{
private:
	enum audSides
	{
		left = 0,
		right,
		NumSides,
	};
	struct vehSsirenInfo{
		RegdVeh pVeh;
		f32 fitness;
		audConductorMap::MapAreas area;
		audSides side;
	};

	typedef atRangeArray<vehSsirenInfo,MaxNumVehConductorsTargets>	Targets;
	typedef atRangeArray<vehSsirenInfo,AUD_MAX_NUM_VEH_WITH_SIREN>	InterestingVehiclesInfo;
	typedef atRangeArray<u32,audConductorMap::MaxNumMapAreas>		MapAreasInfo;
public: 

	void									Init();
	void									ProcessMessage(const ConductorMessageData &messageData);
	void									ProcessSuperConductorOrder(ConductorsOrders order);
	void									ProcessUpdate();
	void									Reset();
	void									UpdateSirensInfo(CVehicle *vehicle);
	void									CleanUpSirensInfo();
	void									GetAvgDirForFakeSiren(Vector3 &sirenPosition);

	 
	audCachedCarLandingTestResults			*GetCachedLineTest()
	{
		return &m_CachedLineTest;
	}
	audVehicleConductorDynamicMixing		&GetDynamicMixingConductor()
	{
		return m_VehicleConductorDM;
	}

	CVehicle*								GetVehWithSirenOn(s32 vehIndex) const;
	CVehicle*								GetTarget(ConductorsTargets target) const;
	EmphasizeIntensity						GetIntensity();
	ConductorsInfo							UpdateInfo();
	u32										GetNumberOfVehWithSirenOn() {return m_NumberOfVehWithSirenOn;}
	u32										GetNumberOfTargets(){return m_NumberOfTargets;}
	bool									IsTarget(const CVehicle *pVeh,s32 &targetIndex);
	bool									IsTarget(const CVehicle *pVeh);
	bool									IsNewTarget(const CVehicle *pVeh);
	bool									PlaySirenWithNoDriver() {return sm_PlaySirenWithNoDriver;}
	bool									PlaySirenWithNoNetworkPlayerDriver() {return sm_PlaySirenWithNoNetworkPlayerDriver;}
	bool									JumpConductorActive() {return m_ShouldUpdateJump;}
	bool									PlayStuntJumpCollision(f32 impactMag);

	static void								InitForDLC();
#if __BANK
	void									VehicleConductorDebugInfo();
	static void								AddWidgets(bkBank &bank);
	static bool								GetShowInfo() {return sm_ShowFSMInfo;}
	static bool								sm_HasToPause;
#endif // __BANK

private:
	struct LandingResults
	{
		f32 fitness; 
		f32 timeToLand;
	};

	virtual fwFsm::Return					UpdateState(const s32 state, const s32 iMessage, const fwFsm::Event event);
	fwFsm::Return							Idle();
	fwFsm::Return 							EmphasizeLowIntensity();
	fwFsm::Return 							EmphasizeMediumIntensity();
	fwFsm::Return 							EmphasizeHighIntensity();

	//Helpers
	void									IssueAsyncLineTestProbes();
	void									ForceTarget(const CVehicle *pVeh);
	void									UpdateTargets();
	void									UpdateSirens();
	void									UpdateClosestArea();
	void									UpdateMediumDistanceArea();
	void									UpdateFarArea();
	void 									UpdateLineTestAsync(ConductorsInfoType &info);
	void									StopStuntJump();
	void									GetStuntJumpBedLoop(audMetadataRef &soundRef, const CVehicle* playerVeh);

	ConductorsInfoType						UpdateInfoJump();
	bool									ProcessAsyncLineTestProbes();
	bool									IsTargetValid(CVehicle *vehicle,audConductorMap::MapAreas area);
	bool									IsCandidateValid(CVehicle *vehicle,f32 &distance,Vector3 &dirToVeh);
	f32 									GetPlayerVehVelocity();
	f32 									GetPlayerVehAirtime();
	f32 									GetPlayerVehUpsideDown();

	audConductorMap::MapAreas				CoumputeAndUpdateAreas(CVehicle* vehicle,f32 distance);
	f32										ComputeVehicleFitnessAndSide(f32 distance,Vector3 &dirToVeh,audSides &side);

#if __BANK
	const char*								GetStateName(s32 iState) const ;
#endif

	Vector3											m_AvgDirectionToDistantSirens;
	Vector3											m_LastCheckPoint;
	Vector3 										m_InitialVehVelocity;

	atRangeArray<Vector3,NUMBER_OF_TEST>			m_ParabolPoints;
	atFixedBitSet<2>								m_WheelAlreadyLanded;

	audVehicleConductorDynamicMixing				m_VehicleConductorDM;
	audVehicleConductorFakeScenes					m_VehicleConductorFS;

	audSound*										m_StuntJumpBedLoop;
	audSound*										m_StuntJumpGoodLandingLoop;
	audSound*										m_StuntJumpBadLandingLoop;
	// Car curves
	audCurve 										m_YawDiffToFitness;
	audCurve 										m_PitchDiffToFitness;
	audCurve 										m_UpRightToFitness;
	audCurve 										m_ScaledVelocity;

	audCurve 										m_DistanceToFitness;
	audCurve 										m_DistanceVehVehToFitness;
	audCurve 										m_NumberOfChasingVehToConfidence;
	audCurve 										m_CarsToNumSirensToPlay;

	audSoundSet										m_StuntJumpSoundSet;

	atRangeArray<f32,NumSides>						m_SpatializationFitness;
	InterestingVehiclesInfo							m_VehiclesWithSirenOn;  	

	Targets											m_VehicleTargets;  
	Targets											m_VehicleNewTargets;  

	MapAreasInfo									m_InterestingAreas;

	LandingResults									m_LandingResults;
	audCachedCarLandingTestResults					m_CachedLineTest;
	ConductorsOrders								m_LastOrder;
	f32												m_lastAngVelocity;
	f32												m_DistanceTraveled;

	s32												m_NumberOfSirensToPlay;
	BANK_ONLY(s32									m_SirensBeingPlayed;);
	u32												m_elapsedTime;
	u32												m_NumberOfVehWithSirenOn;

	u32												m_NumberOfTargets;
	u32												m_LastNumVehInFarArea;

	u32												m_VehTimeInAir;
	u32												m_NumberOfNewTargets;
	bool											m_StuntJumpStarted;
	bool											m_ShouldUpdateJump;
	bool											m_ForceStuntJumpTimeToLandToZero;

	static audSoundSet								sm_MPStuntJumpSoundSet;

	static f32										sm_TimeToMuteNonTargets;
	static f32										sm_TimeToUnMuteNonTargets;
	static f32										sm_TimeToMuteVehMidZone;
	static f32										sm_TimeToUnMuteVehMidZone;
	static f32										sm_TimeToMuteVehFarZone;

	static f32										sm_DistanceThreshold;
	static f32										sm_ThresholdTimeToLand;
	static f32										sm_ThresholdTimeToBadLand;
	static f32										sm_ThresholdFitnessToLand; 
	static f32										sm_ThresholdFitnessToBadLand; 
	static f32										sm_JumpConductorImpactThreshold;
	static f32										sm_DistanceTraveledThreshold;

	static f32										sm_DurationOfTest; 
	static f32										sm_LandTimeError; 
	static f32										sm_VehMinFitnessThreshold; 
	static f32										sm_VehMaxFitnessThreshold; 
	static u32										sm_TimeToUpdateTargets; 
	static u32										sm_TimeSinceFakeSirensWasPlayed; 
	static u32										sm_TimeSinceLastUpdate;
	static u32										sm_TimeToStopFakeSirens; 
	static bool										sm_EnableSirens;
	static bool										sm_PlaySirenWithNoDriver;
	static bool										sm_PlaySirenWithNoNetworkPlayerDriver;
#if __BANK
	static bool 									sm_DrawMap;
	static bool 									sm_ShowVehSirenState;
	static bool 									sm_ShowTargets;
	static bool 									sm_ShowNewTargets;
	static bool										sm_ShowFSMInfo;
	static bool										sm_ShowSirensInfo;
	
	static bool										sm_PreImpactSweeteners;
	static bool										sm_DrawJumpResults;
	static bool										sm_ShowJumpInfo;
	static bool										sm_DrawTrajectory; 
	static bool										sm_DrawVelocity; 
	static bool										sm_ShowLandingInfo; 
	static bool										sm_PauseOnLandEvent;
#endif
};
#endif // AUD_VEHICLECONDUCTOR_H
