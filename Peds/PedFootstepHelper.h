//
// audio/pedaudioentity.cpp
// 
// Copyright (C) 1999-2006 Rockstar Games.  All Rights Reserved.
// 

#ifndef _PED_FOOTSTEP_HELPER_H_
#define _PED_FOOTSTEP_HELPER_H_

#include "vectormath/vec3v.h"
#include "vectormath/vec4v.h"
#include "vectormath/mat34v.h"
#include "audioengine/curve.h"
#include "audioengine/widgets.h"

#include "audio/asyncprobes.h"
#include "audio/gameobjects.h"
#include "PedDefines.h"

#define BIPED_NUM_FEET 2
#define MAX_NUM_FEET 4
#define MAX_NUMBER_OF_GROUND_FOOTSTEPS_TESTS 5
#define AUD_NUM_INTERSECTIONS_FOR_PED_PROBES (16)
#define AUD_NUM_HLOD_PEDS_PER_QUADRANT 1 

enum audQuadrants {Q0 = 0,Q1,Q2,Q3,NumQuadrants};

enum FeetTags 
{	
	RearLeft,
	RearRight,
	FrontLeft,
	FrontRight,
	ExtraMouth,
	MaxFeetTags,
	
	LeftFoot = RearLeft,
	RightFoot = RearRight,
	LeftHand = FrontLeft,
	RightHand = FrontRight
};
enum eFootHeightState { kHeightDown, kHeightDragging, kHeightUp };
enum eFootMoveState { kMovePlanted, kMoveMoving};
enum audFootstepGOIndex
{
	AUD_FOOTSTEP_GO_WALK = 0,
	AUD_FOOTSTEP_GO_SCUFFHARD,
	AUD_FOOTSTEP_GO_SCUFF,
	AUD_FOOTSTEP_GO_HAND,
	AUD_FOOTSTEP_GO_LADDER,
	AUD_FOOTSTEP_GO_LADDER_LIFT,
	AUD_FOOTSTEP_GO_LADDER_HAND,
	AUD_FOOTSTEP_GO_JUMP_LAND,
	AUD_FOOTSTEP_GO_RUN,
	AUD_FOOTSTEP_GO_SPRINT
};
enum audFootstepEvent
{
	AUD_FOOTSTEP_WALK_L,
	AUD_FOOTSTEP_WALK_R,
	AUD_FOOTSTEP_SCUFFHARD_L,
	AUD_FOOTSTEP_SCUFFHARD_R,
	AUD_FOOTSTEP_SCUFF_L,
	AUD_FOOTSTEP_SCUFF_R,
	AUD_FOOTSTEP_HAND_L,
	AUD_FOOTSTEP_HAND_R,
	AUD_FOOTSTEP_LADDER_L,
	AUD_FOOTSTEP_LADDER_R,
	AUD_FOOTSTEP_LADDER_L_UP,
	AUD_FOOTSTEP_LADDER_R_UP,
	AUD_FOOTSTEP_LADDER_HAND_L,
	AUD_FOOTSTEP_LADDER_HAND_R,
	AUD_FOOTSTEP_JUMP_LAND_L,
	AUD_FOOTSTEP_JUMP_LAND_R,
	AUD_FOOTSTEP_RUN_L,
	AUD_FOOTSTEP_RUN_R,
	AUD_FOOTSTEP_SPRINT_L,
	AUD_FOOTSTEP_SPRINT_R,
	AUD_FOOTSTEP_SOFT_L,
	AUD_FOOTSTEP_SOFT_R,
	AUD_FOOTSTEP_LIFT_L,
	AUD_FOOTSTEP_LIFT_R

};
enum audDetectionValues
{
	audDVStop = 0,
	audDVWalk,
	audDVRun,
	audDVSprint,
	audDVJump,
	audDVStairsStop,
	audDVStairsWalk,
	audDVStairsRun,
	audDVSlopes,
	audDVSlopesRun,
	audDVStealthWalk,
	audDVStealthRun,
	audDVLadder,
	audDVVault,
	MaxNumberAudDetValues,
};
struct groundFootStepTest 
{
	Vec3V groundPos,groundNormal; 
	f32 height;
	phMaterialMgr::Id materialId;
	RegdEnt entity;
	s32 numberOfHits,lastNumberOfHits;
	u16 component;
};

struct pedHLODInfo
{
	RegdPed ped;
	f32		fitness;
};

struct feetOffsets
{
	f32	front;
	f32 up;
};
class CPed;

class CPedFootStepHelper 
{
public:

	CPedFootStepHelper();
	void Init (CPed* pParentPed);
	void GetPositionForPedSound(audFootstepEvent event, Vec3V_InOut pos);
	void GetPositionForPedSound(FeetTags tag, Vec3V_InOut pos);
	void Update();
	void UpdatePrivate();
	void UpdateAudioLOD();

	void ForceStepType(bool force, const audFootstepEvent stepType, const audDetectionValues detectionValuesIdx);
	ModelPhysicsParams *GetModelPhysicsParams(){return m_pModPhys;}

	PedTypes GetAudioPedType() { return m_pModPhys ? (PedTypes)m_pModPhys->PedType : HUMAN; }

	Vec3V_Out  GetHeelPosition(const FeetTags footId) const;
	ScalarV_Out GetFootVelocity(const FeetTags footId) const;

	bool WasClimbingALadder() {return m_WasClimbingLadder.IsSet(0) || m_WasClimbingLadder.IsSet(1);};
	bool IsWalkingOnASlope() { return m_IsWalkingOnSlope;};
	bool IsAnAnimal();
	bool IsAudioHLOD() {return m_IsAudioHLOD;};
	void SetVfxWantsToUpdate(bool val) {m_VfxWantsToUpdate = val;}
	FeetTags GetFeetTagForFootstepEvent(const audFootstepEvent event) const;

	void SetClimbingLadder(bool isClimbing);
	void SetSlidingDownALadder();
	void ResetAirFlags();
	void ResetTimeInTheAir(){ m_TimeInTheAir = 0; }
	void ResetFlags() { m_ClimbingLadder = false;}
	void ScriptForceUpdate(bool force) {m_ScriptForceUpdate = force;};

	void SetGroundInfo(u32 legId,Vec3V_In groundPos,Vec3V_In groundNormal);
	static void InitClass();
	static void UpdateClass();
	static void	SetWalkingOnPuddle(bool walkingOnPuddle);
	// Returns true if the player is walking on a puddle, false otherwise. 
	bool	IsWalkingOnPuddle();

	inline u32 GetLastTimeOnGround() const { return m_LastTimeOnGround; }
	inline u32 GetTimeInTheAir() const { return m_TimeInTheAir; }


#if __BANK
	static void AddWidgets(bkBank &bank);
	static f32 OverriddenMovementFactor() {return sm_OverriddenMovementFactor;}
	static bool OnlyUpdateFLFoot(){return sm_OnlyUpdateFLFoot;}
	static bool OnlyUpdateFRFoot(){return sm_OnlyUpdateFRFoot;}
	static bool OnlyUpdateRLFoot(){return sm_OnlyUpdateRLFoot;}
	static bool OnlyUpdateRRFoot(){return sm_OnlyUpdateRRFoot;}
	static bool ShowSlopeInfo(){return sm_ShowSlopeInfo;}
	static bool OverrideSlopeVariables(){return sm_OverrideSlopeVariables;}
#endif

	static f32 GetSlopeDebrisProb()
	{
		return sm_SlopeDebrisProb;
	}

	f32 GetSlopeAngle() const
	{
		return m_SlopeAngle;
	}

	void GetSlopeDirection(Vec3V_InOut downSlopeDirection) const;

	static f32 sm_MaxBoneVelocityInv;

private:

	void InitializeModelPhysics();
	void CalculateFootInfo(FeetTags footId, Mat34V_InOut footMtx, Vec3V_InOut footVel);
	void ComputeFootInfo(FeetTags footId,Mat34V_InOut footMtx, Vec3V_InOut  footVel, f32 upOffset,f32 frontOffset);

	void FootstepGroundTest(Vec3V_InOut groundPosition,Vec3V_InOut groundNormal);

	
	void IssueAsyncLineTestProbes();

	void ProcessFootstepVfx(audFootstepEvent event, bool hindLegs, Mat34V_In footMatrix, const u32 foot, Vec3V_In groundNormal);
	void PreUpdateFootstepGroundTest(Vec3V_InOut groundPosition,Vec3V_InOut groundNormal);
 
	void SyncProbesFromFeetGroundTest(Vec3V_InOut groundPosition,Vec3V_InOut groundNormal);
	void SyncProbesGroundTest(Vec3V_InOut groundPosition,Vec3V_InOut groundNormal);

	void UpdatePhysicProbes(Vec4V_InOut groundPlane);
	void ComputeStepInfo( f32 pedVelocity,audFootstepEvent &stepType,u8 &detectionValuesIndex);

	void UpdateSoftStepsFirstPerson();
	void UpdateFootPlanted(const u32 footId BANK_ONLY(,ScalarV_In footSpeed), const f32 upDistanceToGround, const u8 detectionValuesIndex BANK_ONLY(,const audFootstepEvent stepType),Vec3V_In downSlopeDirection BANK_ONLY(,Vec3V_In heelPos));
	void UpdateFootMoving(const u32 footId,ScalarV_In footSpeed, const f32 upDistanceToGround, const u8 detectionValuesIndex, audFootstepEvent stepType,Vec3V_In downSlopeDirection,bool hindFoot,const u32 foot,Mat34V_In footMatrix BANK_ONLY(,Vec3V_In heelPos));
	void UpdateFeetDetection(audFootstepEvent stepType,u8 detectionValuesIndex,Vec3V_In downSlopeDirection);
	void InitDetectionValuesIndices();
	void SetSpeedSmootherRates();
	void UpdateFootsteps();
	void UpdateHandPositions();
	void UpdateFootstepGroundTest(Vec3V_InOut groundPosition,Vec3V_InOut groundNormal);
	void UpdateLocalBoneVelocities();
	void UpdateUpDiffWithGround();
	void SetIsAudioHLOD(bool hlod) {m_IsAudioHLOD = hlod;};
	void UpdateSlopeDirection(Vec3V_InOut downSlopeDirection);

	f32 CalculatePedFitness(f32 distanceToPed);
	audQuadrants CalculatePedQuadrant(Vec3V_In dirToPed);


	bool ProcessAsyncLineTestProbes();

	static ScalarV GetDistanceThresholdSqToUpdateFootSteps(const CPed& ped);

	BANK_ONLY(bool ShouldUpdateFoot(const u32 footId));


	audDetectionValues FindDetectionModeForHash(const u32 modeHash);

	atRangeArray<Vec4V,MaxFeetTags - 1>			m_FeetInfo;
	atRangeArray<Vec3V,MAX_NUM_FEET>  			m_GroundPosition;
	atRangeArray<Vec3V,MAX_NUM_FEET>  			m_GroundNormal;
	atRangeArray<eFootHeightState,MAX_NUM_FEET> m_FootHeightState;
	atRangeArray<eFootMoveState,MAX_NUM_FEET>	m_FootMoveState;
	atRangeArray<feetOffsets,MAX_NUM_FEET>  	m_FeetOffsets;
	atRangeArray<u32,MAX_NUM_FEET>  			m_LastTimeFootPlanted;
	atRangeArray<u8,MaxNumberAudDetValues>  	m_DetectionValuesIndices;
	atFixedBitSet<MAX_NUM_FEET>					m_AlreadyTriggered;
	atFixedBitSet<MAX_NUM_FEET>					m_AlreadyLifted;
	atFixedBitSet<MAX_NUM_FEET>					m_ScuffAlreadyTriggered;
	atFixedBitSet<MAX_NUM_FEET>					m_WasInTheAir;
	atFixedBitSet<MAX_NUM_FEET>					m_WasClimbingLadder;
	
	audCachedLosOneResult						m_CachedLineTest;

	RegdPed										m_pParentPed;	   		
	ModelPhysicsParams*							m_pModPhys;


	audSmoother									m_PedVelocitySmoother;

	f32											m_AccCosAngle;
	f32											m_PrevCosAngle;
	f32 										m_UpDiffWihtGround;
	f32 										m_SlopeAngle;
	f32											m_PrevHeight;

	u32											m_TimeToResetLaddersFlag;
	u32											m_TimeToResetStairsFlag;
	u32 										m_LastTimeOnGround; //unpused time from sound manager
	u32 										m_TimeInTheAir;
	u32											m_LastTimeSoftStepPlayed;
	audFootstepEvent							m_LastEvent;

	audFootstepEvent							m_ScriptStepType;
	audDetectionValues							m_ScriptDetectionValuesIdx;

	bool 										m_ScriptForceStepType;
	bool 										m_AlreadyLanded;
	bool 										m_WasOnStairs;
	bool 										m_WasMoving;
	bool 										m_IsWalkingOnSlope;
	bool 										m_HasToInitGroundTest;
	bool 										m_HasResultThisFrame;
	bool 										m_HasIkResultsThisFrame;
	bool 										m_ScriptForceUpdate;
	bool 										m_VfxWantsToUpdate;
	bool 										m_IsAudioHLOD;
	bool 										m_IsPlayingSwitchScene;
	bool 										m_SlidingDownALadder;
	bool 										m_ClimbingLadder;
	bool 										m_ApplyHeelOffsets;
	bool 										m_ApplyHighHeelOffsets;

	static atRangeArray<groundFootStepTest,MAX_NUMBER_OF_GROUND_FOOTSTEPS_TESTS> sm_GroundFootStepTests;
	static atRangeArray<pedHLODInfo,NumQuadrants>   sm_HLODPeds; 
	static atRangeArray<f32,NUM_SHOETYPES>			sm_ShoesFitness; 

	static audCurve 								sm_DistanceToHLODFitness;
	static audCurve 								sm_PlayerClumsinessToScuffProb;
	static audCurve 								sm_SlopeAngleToDeltaSpeed;

	static ScalarV 									sm_DistanceThresholdSqToUpdateFootsteps;
	static ScalarV 									sm_DistanceThresholdSqToUpdateFootstepsNetworkPlayer;

	static f32 										sm_SlopeDebrisProb;
	static f32 										sm_SlopeDAngleThreshold;
	static f32 										sm_TriggerScuffThresh;

	static f32 										sm_FootStepLineTestStartOffset;
	static f32 										sm_FootStepLineTestLong;
	static f32 										sm_FootStepHeightThreshold;
	static f32 										sm_NonPlayerBoneSpeedScalar;
	static f32 										sm_DeltaAimSpeed;
	
	static f32 										sm_SprintBoneSpeedScalar;
	static f32 										sm_MaxBoneSpeedDecrease;

	static f32 										sm_HeelUpOffset;
	static f32 										sm_HeelFrontOffset;
	static f32 										sm_HighHeelFrontOffset;
	static f32 										sm_HighHeelUpOffset;

	static f32 										sm_RunFitnessScale;
	static f32 										sm_AnimalFitnessScale;
	static f32 										sm_StairsDeltaSpeed;

	static f32										sm_UpDistaneToTriggerLift;

	static s32 										sm_NumIntersections;

	static u32 										sm_DeltaTimeToResetLadders;
	static u32 										sm_NumberOfTestResultStored;

	static bool										sm_IsWalkingOnPuddle;
	static bool 									sm_AsyncProbesGroundTest;
	static bool 									sm_SyncProbesGroundTest;
	static bool 									sm_UseProceduralFootsteps;
	static bool 									sm_WasPlayerSwitchActive;

#if __BANK

	static f32										sm_FUpOffset;
	static f32										sm_FFrontOffset;
	static f32										sm_RUpOffset;
	static f32										sm_RFrontOffset;

	static f32										sm_OverriddenMovementFactor;
	static f32										sm_OverriddenSlopeAngle;


	static bool										sm_DrawHLODResults;
	static bool										sm_DrawFeetPosition;
	static bool										sm_DrawFeetVelocity;
	static bool										sm_OverrideFOffsets;
	static bool										sm_OverrideROffsets;
	static bool										sm_ApplyHeelsOffsets;
	static bool										sm_ApplyHighHeelsOffsets;

	static bool										sm_OverrideSlopeAngle;
	static bool										sm_OverrideSlopeVariables;
	static bool										sm_ShowSlopeInfo;

	static bool										sm_ForceSyncProbesToAllPeds;
	static bool										sm_DrawDebugInfo;
	static bool										sm_DrawResults;
	static bool										sm_ShowPedSpeed;

	static bool										sm_OnlyUpdateFLFoot;
	static bool										sm_OnlyUpdateFRFoot;
	static bool										sm_OnlyUpdateRLFoot;
	static bool										sm_OnlyUpdateRRFoot;

	static bool										sm_ShowFootStepTuningInfo;

	static bool										sm_PauseOnLimits;
	static bool										sm_PauseOnFootstepEvents;
	static bool										sm_PauseOnDragEvents;

#endif
};

#endif
