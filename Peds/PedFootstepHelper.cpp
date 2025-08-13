#include "audio/northaudioengine.h"
#include "audio/scriptaudioentity.h"
#include "audioengine/engine.h"
#include "PedFootstepHelper.h"
#include "Peds/Ped.h"
#include "peds/pedintelligence.h"
#include "Physics/archetype.h"
#include "Physics/gtaInst.h"
#include "physics/physics.h"
#include "task/Weapons/Gun/TaskGun.h"
#include "scene/playerswitch/PlayerSwitchInterface.h"
#include "system/timer.h"
#include "fwanimation/animdefines.h"
#include "fwanimation/expressionsets.h"
#include "fwmaths/vectorutil.h"
#include "control/replay/replay.h"
#include "task/Vehicle/TaskEnterVehicle.h"
#include "task/Vehicle/TaskExitVehicle.h"

#include "PhBound/boundcomposite.h"
PED_OPTIMISATIONS()
// Footsteps timers
PF_PAGE(Ped_Audio_Timers,	"audPedAudioEntity");
PF_GROUP(PedAudioEntity);
PF_LINK(Ped_Audio_Timers, PedAudioEntity);
PF_TIMER(UpdateFootSteps,PedAudioEntity);
PF_TIMER(PreUpdateFootstepGTest,PedAudioEntity);
PF_TIMER(UpdateFootstepGTest,PedAudioEntity);
PF_TIMER(ProcessAsyncLineTProbes,PedAudioEntity);
PF_TIMER(IssueAsyncLineTProbes,PedAudioEntity);
PF_TIMER(GetRes,PedAudioEntity);

#if __BANK
f32 CPedFootStepHelper::sm_FUpOffset = 0.f;
f32 CPedFootStepHelper::sm_FFrontOffset = 0.f;
f32 CPedFootStepHelper::sm_RUpOffset = 0.f;
f32 CPedFootStepHelper::sm_RFrontOffset = 0.f;

f32 CPedFootStepHelper::sm_OverriddenMovementFactor = 0.f;
f32 CPedFootStepHelper::sm_OverriddenSlopeAngle = 0.f;

bool CPedFootStepHelper::sm_OverrideSlopeAngle = false;
bool CPedFootStepHelper::sm_ShowSlopeInfo = false;
bool CPedFootStepHelper::sm_OverrideSlopeVariables = false;
bool CPedFootStepHelper::sm_OverrideFOffsets = false;
bool CPedFootStepHelper::sm_OverrideROffsets = false;
bool CPedFootStepHelper::sm_ApplyHeelsOffsets = false;
bool CPedFootStepHelper::sm_ApplyHighHeelsOffsets = false;
bool CPedFootStepHelper::sm_DrawHLODResults = false;
bool CPedFootStepHelper::sm_DrawFeetPosition = false;
bool CPedFootStepHelper::sm_DrawFeetVelocity = false;

bool CPedFootStepHelper::sm_OnlyUpdateFLFoot = false;
bool CPedFootStepHelper::sm_OnlyUpdateFRFoot = false;
bool CPedFootStepHelper::sm_OnlyUpdateRLFoot = false;
bool CPedFootStepHelper::sm_OnlyUpdateRRFoot = false;

bool CPedFootStepHelper::sm_DrawResults = false;
bool CPedFootStepHelper::sm_ForceSyncProbesToAllPeds = false;
bool CPedFootStepHelper::sm_DrawDebugInfo = false;
bool CPedFootStepHelper::sm_ShowPedSpeed = false;

bool CPedFootStepHelper::sm_ShowFootStepTuningInfo = false;
bool CPedFootStepHelper::sm_PauseOnLimits = false;
bool CPedFootStepHelper::sm_PauseOnFootstepEvents = false;
bool CPedFootStepHelper::sm_PauseOnDragEvents = false;
#endif

atRangeArray<pedHLODInfo,NumQuadrants>  CPedFootStepHelper::sm_HLODPeds;
atRangeArray<f32,NUM_SHOETYPES>  CPedFootStepHelper::sm_ShoesFitness;

audCurve CPedFootStepHelper::sm_DistanceToHLODFitness;
audCurve CPedFootStepHelper::sm_PlayerClumsinessToScuffProb;
audCurve CPedFootStepHelper::sm_SlopeAngleToDeltaSpeed;

bank_u32 g_TimeToResetStairsFlagThreshold = 250;
f32 CPedFootStepHelper::sm_DeltaAimSpeed = 1.15f;
f32 CPedFootStepHelper::sm_NonPlayerBoneSpeedScalar = 1.15f;
f32 CPedFootStepHelper::sm_SprintBoneSpeedScalar = 0.65f;
f32 CPedFootStepHelper::sm_MaxBoneSpeedDecrease = 0.05f;
f32 CPedFootStepHelper::sm_MaxBoneVelocityInv = 1.0f / 0.050f;

f32 CPedFootStepHelper::sm_HeelUpOffset = 0.085f;
f32 CPedFootStepHelper::sm_HeelFrontOffset = -0.034f;
f32 CPedFootStepHelper::sm_HighHeelFrontOffset = -0.026f;
f32 CPedFootStepHelper::sm_HighHeelUpOffset = 0.129f;

f32 CPedFootStepHelper::sm_RunFitnessScale = 1.5f;
f32 CPedFootStepHelper::sm_AnimalFitnessScale = 0.5f;
f32 CPedFootStepHelper::sm_StairsDeltaSpeed = 1.5f;
f32 CPedFootStepHelper::sm_UpDistaneToTriggerLift = 0.1f;


f32 CPedFootStepHelper::sm_TriggerScuffThresh = 0.9f;
ScalarV	CPedFootStepHelper::sm_DistanceThresholdSqToUpdateFootsteps = ScalarV(550);
ScalarV	CPedFootStepHelper::sm_DistanceThresholdSqToUpdateFootstepsNetworkPlayer = ScalarV(2500);// 50*50 Note: may need to be larger than the distance you care about, for possibly measuring distance to the camera, and because footstep events don't get generated every frame.
f32	CPedFootStepHelper::sm_FootStepLineTestStartOffset = 0.896f;
f32	CPedFootStepHelper::sm_FootStepLineTestLong = 0.216f;
f32	CPedFootStepHelper::sm_FootStepHeightThreshold = 0.3f;
s32	CPedFootStepHelper::sm_NumIntersections = 1;
f32 CPedFootStepHelper::sm_SlopeDAngleThreshold = 12.5f;
f32 CPedFootStepHelper::sm_SlopeDebrisProb = 0.3f;

u32 CPedFootStepHelper::sm_NumberOfTestResultStored = 0;
u32 CPedFootStepHelper::sm_DeltaTimeToResetLadders = 1000;

bool CPedFootStepHelper::sm_AsyncProbesGroundTest = true;
bool CPedFootStepHelper::sm_SyncProbesGroundTest = false;
bool CPedFootStepHelper::sm_UseProceduralFootsteps = true;
bool CPedFootStepHelper::sm_WasPlayerSwitchActive = false;
bool CPedFootStepHelper::sm_IsWalkingOnPuddle = false;

atRangeArray<groundFootStepTest,MAX_NUMBER_OF_GROUND_FOOTSTEPS_TESTS> CPedFootStepHelper::sm_GroundFootStepTests;
//-------------------------------------------------------------------------------------------------------------------
CPedFootStepHelper::CPedFootStepHelper()
{
	m_HasIkResultsThisFrame = false;
	m_pParentPed = NULL;
	m_WasOnStairs = false;
	m_AlreadyLanded = false;
	m_TimeToResetStairsFlag = 0;
	m_TimeToResetLaddersFlag = 0;
	m_IsWalkingOnSlope = false;
	m_SlopeAngle =  0.f;
	m_PrevHeight = 0.f;
	for(u8 i = 0; i < MAX_NUM_FEET; i ++)
	{
		m_FootHeightState[i] = kHeightDown;
		m_FootMoveState[i] = kMovePlanted;
		m_LastTimeFootPlanted[i] = 0;
		m_GroundPosition[i] = Vec3V(V_ZERO);
		m_GroundNormal[i] = Vec3V(V_ZERO);
		m_FeetOffsets[i].front = 0.f;
		m_FeetOffsets[i].up = 0.f;
	}
	for(u8 i = 0; i < MaxFeetTags - 1; i ++)
	{
		m_FeetInfo[i] = Vec4V(V_ZERO);
	}
	for(u8 i = 0; i < MaxNumberAudDetValues; i ++)
	{
		m_DetectionValuesIndices[i] = 0;
	}
	m_AlreadyLifted.Reset();
	m_AlreadyTriggered.Reset();
	m_ScuffAlreadyTriggered.Reset();
	m_WasClimbingLadder.Reset();
	m_WasInTheAir.Reset();
}
//-------------------------------------------------------------------------------------------------------------------
void CPedFootStepHelper::InitClass()
{
	for (s32 i = 0 ; i < MAX_NUMBER_OF_GROUND_FOOTSTEPS_TESTS; ++i)
	{
		CPedFootStepHelper::sm_GroundFootStepTests[i].numberOfHits = 0; 
		CPedFootStepHelper::sm_GroundFootStepTests[i].lastNumberOfHits = 0; 
		CPedFootStepHelper::sm_GroundFootStepTests[i].height = -5000; 
		CPedFootStepHelper::sm_GroundFootStepTests[i].groundPos = Vec3V(V_ZERO); 
		CPedFootStepHelper::sm_GroundFootStepTests[i].groundNormal = Vec3V(V_ZERO); 
		CPedFootStepHelper::sm_GroundFootStepTests[i].entity = NULL;
		CPedFootStepHelper::sm_GroundFootStepTests[i].component = 0;
		CPedFootStepHelper::sm_GroundFootStepTests[i].materialId = phMaterialMgr::DEFAULT_MATERIAL_ID;

	}
	sysMemSet(&sm_HLODPeds, 0, sizeof(sm_HLODPeds));
	sysMemSet(&sm_ShoesFitness, 0, sizeof(sm_ShoesFitness));
	sm_DistanceToHLODFitness.Init(ATSTRINGHASH("PED_DISTANCE_TO_FITNESS", 0xECD21958));
	sm_PlayerClumsinessToScuffProb.Init(ATSTRINGHASH("PLAYER_CLUMSINESS_TO_SCUFF_PROB", 0x32D04003));
	sm_SlopeAngleToDeltaSpeed.Init(ATSTRINGHASH("SLOPE_ANGLE_TO_DELTA_SPEED", 0x433F362F));
	sm_WasPlayerSwitchActive = false;
}
//-------------------------------------------------------------------------------------------------------------------
void CPedFootStepHelper::SetGroundInfo(u32 legId,Vec3V_In groundPos,Vec3V_In groundNormal)
{
	m_HasIkResultsThisFrame = true;
	m_GroundPosition[legId] = groundPos;
	m_GroundNormal[legId] = groundNormal;
}
//-------------------------------------------------------------------------------------------------------------------
void CPedFootStepHelper::ComputeFootInfo(FeetTags footId,Mat34V_InOut footMtx, Vec3V_InOut footVel,f32 upOffset,f32 frontOffset)
{
	eAnimBoneTag boneTag = BONETAG_ROOT;
	switch(footId)
	{
	case LeftFoot:
		boneTag = BONETAG_L_PH_FOOT;
		break;
	case RightFoot:
		boneTag = BONETAG_R_PH_FOOT;
		break;
	case LeftHand:
		boneTag = BONETAG_L_PH_HAND;
		break;
	case RightHand:
		boneTag = BONETAG_R_PH_HAND;
		break;
	default:
		footMtx = Mat34V(V_IDENTITY);
		footMtx.SetCol3(m_pParentPed->GetTransform().GetPosition());
		return;
	}
	if ( IsAnAnimal())
	{
		// don't apply offsets on animals.
		upOffset = frontOffset = 0.f;
	}
	Vec3V footPos = Vec3V(V_ZERO);
	crSkeleton& skel = *m_pParentPed->GetSkeleton();
	const crSkeletonData& skelData = skel.GetSkeletonData();
	s32 iBoneIndex;
	if (skelData.ConvertBoneIdToIndex((u16)boneTag, iBoneIndex))
	{
		footMtx = skel.GetObjectMtx(iBoneIndex);
		naAssertf(IsFiniteAll(footMtx),"Wrong foot matrix.");
		Mat34V_ConstRef mPedMat = m_pParentPed->GetMatrixRef();
		Matrix34 footMx34 = MAT34V_TO_MATRIX34(footMtx);
		footMx34.Dot(MAT34V_TO_MATRIX34(mPedMat));
		footMtx = MATRIX34_TO_MAT34V(footMx34);
		footPos = footMtx.GetCol3Ref();
		footPos = Subtract(footPos,footMtx.GetCol2() * ScalarV(upOffset));
		footPos = Add(footPos,footMtx.GetCol1() * ScalarV(frontOffset));
	}
	footVel = Subtract(footPos, m_FeetInfo[footId].GetXYZ()) * TIME.GetInvSecondsV();
	footMtx.SetCol3(footPos);

#if __BANK
	if(sm_DrawFeetPosition)
	{
		grcDebugDraw::Axis(footMtx, 1.0f, false, 1);
	}
	if(sm_DrawFeetVelocity)
	{
		grcDebugDraw::Line(footPos, m_FeetInfo[footId].GetXYZ(), Color32(1.0f, 0.0f, 0.0f, 1.0f));
	}
#endif
}
//-------------------------------------------------------------------------------------------------------------------
void CPedFootStepHelper::FootstepGroundTest(Vec3V_InOut groundPosition,Vec3V_InOut groundNormal)
{
	PreUpdateFootstepGroundTest(groundPosition, groundNormal);
	if(!m_HasResultThisFrame)
	{
		// Check if the height of the ped pass our test with some of the results that we already have.
		s32 testIndex = -1 ; 
		const Matrix34 &rootMatrix = MAT34V_TO_MATRIX34(m_pParentPed->GetTransform().GetMatrix()); 
		if(CPedFootStepHelper::sm_NumberOfTestResultStored > 0)
		{
			for(s32 i = 0; i < MAX_NUMBER_OF_GROUND_FOOTSTEPS_TESTS; ++i)
			{			
				if(testIndex == -1 && (fabs(rootMatrix.d.GetZ() - CPedFootStepHelper::sm_GroundFootStepTests[i].height) <= sm_FootStepHeightThreshold))
				{
					CPedFootStepHelper::sm_GroundFootStepTests[i].numberOfHits ++; 
					testIndex = i; 
				}
			}
		}
		//If we found a good solution, use it. 
		if (testIndex != -1)
		{
#if __BANK
			if(sm_DrawDebugInfo)
			{
				Vector3 headPos = m_pParentPed->GetBonePositionCached(BONETAG_HEAD);

				grcDebugDraw::Sphere(headPos + Vector3(0.f,0.f,1.f),0.2f,Color_yellow,true);
			}
#endif
			groundPosition = CPedFootStepHelper::sm_GroundFootStepTests[testIndex].groundPos;
			groundNormal = CPedFootStepHelper::sm_GroundFootStepTests[testIndex].groundNormal;
			if(m_pParentPed->GetPedAudioEntity())
			{
				m_pParentPed->GetPedAudioEntity()->GetFootStepAudio().SetStandingMaterial(CPedFootStepHelper::sm_GroundFootStepTests[testIndex].materialId
					, CPedFootStepHelper::sm_GroundFootStepTests[testIndex].entity,  CPedFootStepHelper::sm_GroundFootStepTests[testIndex].component);
			}
		}
		// We didn't get any good result but we have some of them stored.
		else
		{	
			SyncProbesFromFeetGroundTest(groundPosition, groundNormal);
		}
	}
	m_HasResultThisFrame = false; 
}
//-------------------------------------------------------------------------------------------------------------------
ScalarV_Out CPedFootStepHelper::GetFootVelocity(const FeetTags footId) const
{
	naAssertf(footId < (MaxFeetTags - 1), "boneId id out of bounds of m_BoneLocalNormalisedSpeeds[]");
	return m_FeetInfo[footId].GetW();
}
//-------------------------------------------------------------------------------------------------------------------
Vec3V_Out CPedFootStepHelper::GetHeelPosition(const FeetTags footId) const
{
	naAssertf(footId < (MaxFeetTags - 1), "boneId id out of bounds of m_BoneLocalNormalisedSpeeds[]");
	return m_FeetInfo[footId].GetXYZ();
}
//-------------------------------------------------------------------------------------------------------------------
void CPedFootStepHelper::CalculateFootInfo(FeetTags footId, Mat34V_InOut footMtx, Vec3V_InOut footVel)
{
	// default the foot info 
	footVel = Vec3V(V_ZERO);
	footMtx = Mat34V(V_IDENTITY);
	if(Verifyf(m_pParentPed,"Null ped pointer, can't calculate foot info"))
	{
		f32 upOffset = m_FeetOffsets[footId].up;
		f32 frontOffset = m_FeetOffsets[footId].front;
		// Correct the heels position.
		if(m_pParentPed->GetPedAudioEntity())
		{
			if( m_pParentPed->GetPedAudioEntity()->GetFootStepAudio().IsWearingHeels())
			{
				upOffset = m_FeetOffsets[footId].up + sm_HeelUpOffset;
				frontOffset = m_FeetOffsets[footId].front + sm_HeelFrontOffset;
			}
			else if (  m_pParentPed->GetPedAudioEntity()->GetFootStepAudio().IsWearingHighHeels())
			{
				upOffset = m_FeetOffsets[footId].up + sm_HighHeelUpOffset;
				frontOffset = m_FeetOffsets[footId].front + sm_HighHeelFrontOffset;
			}
		}
#if __BANK 
		if(sm_ApplyHeelsOffsets && (footId == RearLeft || footId == RearRight))
		{
			upOffset = m_FeetOffsets[footId].up + sm_HeelUpOffset;
			frontOffset = m_FeetOffsets[footId].front + sm_HeelFrontOffset;
		}
		else if (sm_ApplyHighHeelsOffsets && (footId == RearLeft || footId == RearRight))
		{
			upOffset = m_FeetOffsets[footId].up + sm_HighHeelUpOffset;
			frontOffset = m_FeetOffsets[footId].front + sm_HighHeelFrontOffset;
		}
		if(sm_OverrideROffsets && (footId == RearLeft || footId == RearRight))
		{
			upOffset = sm_RUpOffset;
			frontOffset = sm_RFrontOffset;
		}
		else if ( sm_OverrideFOffsets && (footId == FrontLeft || footId == FrontRight))
		{
			upOffset = sm_FUpOffset;
			frontOffset = sm_FFrontOffset;
		}
#endif
		ComputeFootInfo(footId,footMtx, footVel, upOffset, frontOffset);
		m_FeetInfo[footId] = Vec4V(Vec::V4PermuteTwo<Vec::X1,Vec::Y1,Vec::Z1,Vec::X2>( footMtx.GetCol3().GetIntrin128(), Mag(footVel).GetIntrin128() ));
	}
}
//-------------------------------------------------------------------------------------------------------------------
void CPedFootStepHelper::UpdateHandPositions()
{
	Mat34V handMtx = Mat34V(V_IDENTITY);
	Vec3V handVel= Vec3V(V_ZERO);
	// default the foot info 
	if(Verifyf(m_pParentPed,"Null ped pointer, can't calculate foot info"))
	{
		f32 upOffset = m_FeetOffsets[LeftHand].up;
		f32 frontOffset = m_FeetOffsets[LeftHand].front;
#if __BANK 
		if ( sm_OverrideFOffsets)
		{
			upOffset = sm_FUpOffset;
			frontOffset = sm_FFrontOffset;
		}
#endif
		ComputeFootInfo(LeftHand,handMtx, handVel, upOffset, frontOffset);
		m_FeetInfo[LeftHand] = Vec4V(Vec::V4PermuteTwo<Vec::X1,Vec::Y1,Vec::Z1,Vec::X2>( handMtx.GetCol3().GetIntrin128(), Mag(handVel).GetIntrin128() ));
		upOffset = m_FeetOffsets[RightHand].up;
		frontOffset = m_FeetOffsets[RightHand].front;
#if __BANK 
		if ( sm_OverrideFOffsets)
		{
			upOffset = sm_FUpOffset;
			frontOffset = sm_FFrontOffset;
		}
#endif
		ComputeFootInfo(RightHand,handMtx, handVel, upOffset, frontOffset);
		m_FeetInfo[RightHand] = Vec4V(Vec::V4PermuteTwo<Vec::X1,Vec::Y1,Vec::Z1,Vec::X2>( handMtx.GetCol3().GetIntrin128(), Mag(handVel).GetIntrin128() ));
	}
}
//-------------------------------------------------------------------------------------------------------------------
void CPedFootStepHelper::GetPositionForPedSound(audFootstepEvent event, Vec3V_InOut pos)
{
	GetPositionForPedSound(GetFeetTagForFootstepEvent(event), pos);
}
//-------------------------------------------------------------------------------------------------------------------
void CPedFootStepHelper::GetPositionForPedSound(FeetTags tag, Vec3V_InOut pos)
{
	// if the ped is culled for rendering then the skeleton isn't posed, and the bone positions never update
	if(m_pParentPed->GetIsVisibleInSomeViewportThisFrame())
	{
		switch(tag)
		{
		case RearLeft:
			pos = m_FeetInfo[LeftFoot].GetXYZ();
			break;
		case FrontLeft:
			pos = m_FeetInfo[LeftHand].GetXYZ();
			break;
		case FrontRight:
			pos = m_FeetInfo[RightHand].GetXYZ();
			break;
		case RearRight:
			pos = m_FeetInfo[RightFoot].GetXYZ();
			break;
		case ExtraMouth:
			pos = m_pParentPed->GetBonePositionCachedV(BONETAG_HEAD);
			break;
		default:
			naAssertf(false,"Bad feet tag %d, defaulting position to ped pos.",tag);
			pos = m_pParentPed->GetTransform().GetPosition();
			break;	
		}
	}
	else
	{
		pos = m_pParentPed->GetTransform().GetPosition();
	}
}
//-------------------------------------------------------------------------------------------------------------------
FeetTags CPedFootStepHelper::GetFeetTagForFootstepEvent(const audFootstepEvent event) const
{
	switch(event)
	{
	case AUD_FOOTSTEP_WALK_L:
	case AUD_FOOTSTEP_SCUFFHARD_L:
	case AUD_FOOTSTEP_SCUFF_L:
	case AUD_FOOTSTEP_LADDER_L:
	case AUD_FOOTSTEP_LADDER_L_UP:
	case AUD_FOOTSTEP_JUMP_LAND_L:
	case AUD_FOOTSTEP_RUN_L:
	case AUD_FOOTSTEP_SPRINT_L:
		return RearLeft;
	case AUD_FOOTSTEP_HAND_L:
	case AUD_FOOTSTEP_LADDER_HAND_L:
		return FrontLeft;
	case AUD_FOOTSTEP_HAND_R:
	case AUD_FOOTSTEP_LADDER_HAND_R:
		return FrontRight;
	default:
		return RearRight;
	}
}
//-------------------------------------------------------------------------------------------------------------------
ScalarV CPedFootStepHelper::GetDistanceThresholdSqToUpdateFootSteps(const CPed& ped)
{
	// It's important in MP that remote players generate noise from far enough away -
	// not for the actual audio, but so that CPlayerInfo::ReportStealthNoise() gets called
	// and the player gets blipped on the radar. So, we use a larger threshold value for that case.

	if(NetworkInterface::IsGameInProgress() && ped.IsAPlayerPed())
	{
		return sm_DistanceThresholdSqToUpdateFootstepsNetworkPlayer;
	}
	else
	{
		return sm_DistanceThresholdSqToUpdateFootsteps;
	}
}
//-------------------------------------------------------------------------------------------------------------------
void CPedFootStepHelper::Init(CPed * pParentPed)
{
	m_pParentPed = pParentPed;
	m_UpDiffWihtGround = 1.f;
	for(u8 i = 0; i < MAX_NUM_FEET; i ++)
	{
		m_FootHeightState[i] = kHeightDown;
		m_FootMoveState[i] = kMovePlanted;
		m_LastTimeFootPlanted[i] = 0;
		m_GroundPosition[i] = Vec3V(V_ZERO);
		m_GroundNormal[i] = Vec3V(V_ZERO);
		m_FeetOffsets[i].front = 0.f;
		m_FeetOffsets[i].up = 0.f;
	}
	for(u8 i = 0; i < MaxFeetTags - 1; i ++)
	{
		m_FeetInfo[i] = Vec4V(V_ZERO);
	}
	for(u8 i = 0; i < MaxNumberAudDetValues; i ++)
	{
		m_DetectionValuesIndices[i] = 0;
	}
	m_AlreadyLifted.Reset();
	m_AlreadyTriggered.Reset();
	m_ScuffAlreadyTriggered.Reset();
	m_WasClimbingLadder.Reset();
	m_WasInTheAir.Reset();

	m_pModPhys = NULL;
	m_HasToInitGroundTest = true;
	m_UpDiffWihtGround = 1.f;
	m_LastTimeOnGround = 0;
	m_TimeInTheAir = 0;
	m_ScriptStepType = AUD_FOOTSTEP_WALK_L;
	m_ScriptDetectionValuesIdx = audDVWalk;

	m_AccCosAngle = 0.f;
	m_PrevCosAngle = 0.f;

	m_LastTimeSoftStepPlayed = 0;
	m_LastEvent = AUD_FOOTSTEP_SOFT_L;

	m_ScriptForceStepType = false;
	m_ScriptForceUpdate = false;
	m_VfxWantsToUpdate = false;
	m_IsAudioHLOD = false;
	m_IsPlayingSwitchScene = false;
	m_SlidingDownALadder = false;
	m_ClimbingLadder = false;
	m_ApplyHeelOffsets = false;
	m_ApplyHighHeelOffsets = false;
	m_PedVelocitySmoother.Init(1.0f,1.0f,0.0,10.f);
}
//-------------------------------------------------------------------------------------------------------------------
void CPedFootStepHelper::SetSlidingDownALadder()
{
	if(m_pModPhys)
	{
		for(u32 i = 0; i < m_pModPhys->NumFeet ; i++)
		{
			m_WasClimbingLadder.Set(i);
			m_FootMoveState[i] = kMoveMoving;
			m_AlreadyTriggered.Clear(i);
			m_AlreadyLifted.Clear(i);
			m_LastTimeFootPlanted[i] = 0;
		}
		m_ClimbingLadder = false;
		m_SlidingDownALadder = true;
	}
} 
//-------------------------------------------------------------------------------------------------------------------
void CPedFootStepHelper::SetClimbingLadder(bool isClimbing)
{
	if(!isClimbing && m_ClimbingLadder)
	{
		if(m_pModPhys)
		{
			for(u32 i = 0; i < m_pModPhys->NumFeet ; i++)
			{
				m_WasClimbingLadder.Set(i);
				m_FootMoveState[i] = kMoveMoving;
				m_AlreadyTriggered.Clear(i);
				m_AlreadyLifted.Clear(i);
			}
		}
	}
	m_ClimbingLadder = isClimbing;
} 
//-------------------------------------------------------------------------------------------------------------------
void CPedFootStepHelper::ResetAirFlags()
{
	for(u32 i = 0; i < (MaxFeetTags -1) ; i++)
	{
		m_WasInTheAir.Clear(i);
	}
}
//-------------------------------------------------------------------------------------------------------------------
void CPedFootStepHelper::UpdateClass()
{
#if __BANK
	if(sm_DrawHLODResults)
	{
		for(u32 i = 0; i<NumQuadrants ; i++)
		{
			if(sm_HLODPeds[i].ped)
			{
				Vector3 headPos = sm_HLODPeds[i].ped->GetBonePositionCached(BONETAG_HEAD);
				grcDebugDraw::Sphere(headPos,0.2f,Color32(0.f,255.f,0.f,128.f),false);
			}
		}
		Vector3 playerPos = VEC3V_TO_VECTOR3(CGameWorld::FindLocalPlayer()->GetTransform().GetPosition()); 
		grcDebugDraw::Line(playerPos,playerPos + 10.f *MAT34V_TO_MATRIX34(g_AudioEngine.GetEnvironment().GetVolumeListenerMatrix()).b,Color_red);
		grcDebugDraw::Line(playerPos,playerPos - 10.f *MAT34V_TO_MATRIX34(g_AudioEngine.GetEnvironment().GetVolumeListenerMatrix()).b,Color_red);
		grcDebugDraw::Line(playerPos,playerPos + 10.f *MAT34V_TO_MATRIX34(g_AudioEngine.GetEnvironment().GetVolumeListenerMatrix()).a,Color_red);
		grcDebugDraw::Line(playerPos,playerPos - 10.f *MAT34V_TO_MATRIX34(g_AudioEngine.GetEnvironment().GetVolumeListenerMatrix()).a,Color_red);
	}
#endif
	// Clean up the array for the next iteration
	for(u32 i = 0; i<NumQuadrants ; i++)
	{
		sm_HLODPeds[i].ped = NULL;
		sm_HLODPeds[i].fitness = 0.f;
	}
	ShoeList *shoeList = audNorthAudioEngine::GetObject<ShoeList>(ATSTRINGHASH("ShoeList",0xCB4E5724));
	if(naVerifyf(shoeList, "Couldn't find autogenerated shoe type list"))
	{
		for (s32 i = 0; i < shoeList->numShoes; ++i)
		{
			ShoeAudioSettings *settings =  audNorthAudioEngine::GetObject<ShoeAudioSettings>(shoeList->Shoe[i].ShoeAudioSettings);
			if(settings)
			{
				sm_ShoesFitness[settings->ShoeType] = settings->ShoeFitness;
			}
		}
	}
	if(g_PlayerSwitch.IsActive())
	{
		sm_WasPlayerSwitchActive = true;
	}
}
//-------------------------------------------------------------------------------------------------------------------
void CPedFootStepHelper::ForceStepType(bool force, const audFootstepEvent stepType, const audDetectionValues detectionValuesIdx)
{
	m_ScriptForceStepType = force;
	m_ScriptStepType = stepType;
	m_ScriptDetectionValuesIdx = detectionValuesIdx;
}
//-------------------------------------------------------------------------------------------------------------------
void CPedFootStepHelper::UpdateAudioLOD()
{	
	m_IsAudioHLOD = false;
	//UpdateUpDiffWithGround();
	const crSkeleton* pSkel = m_pParentPed->GetFragInst()->GetSkeleton();
	if (pSkel == NULL ||  !m_pParentPed->GetIsVisible())
	{
		return;
	}
	if(m_pParentPed->IsLocalPlayer())
	{
		m_IsAudioHLOD = true;
		return;
	}
	// Distance test to check if we have to update the footsteps for the current ped. 
	Vec3V pedPos = m_pParentPed->GetTransform().GetPosition();
	Vec3V dirToPed = Subtract(pedPos ,g_AudioEngine.GetEnvironment().GetVolumeListenerPosition()); 
	ScalarV distanceToPed = MagSquared(dirToPed);
	//If using the sniper avoid the distance check 
	if(IsTrue(distanceToPed > GetDistanceThresholdSqToUpdateFootSteps(*m_pParentPed)))
	{
		return; 
	}
	// At this point the ped is a candidate to be a HLOD, check it.
	dirToPed = Normalize(dirToPed);
	audQuadrants pedQuadrant = CalculatePedQuadrant(dirToPed);
	f32 pedFitness = CalculatePedFitness(distanceToPed.Getf());
	if(!sm_HLODPeds[pedQuadrant].ped || (sm_HLODPeds[pedQuadrant].fitness < pedFitness))
	{
		if(sm_HLODPeds[pedQuadrant].ped)
		{
			//sm_ShoesFitness[m_pParentPed->GetPedAudioEntity()->GetFootStepAudio().GetShoeType()] = m_pParentPed->GetPedAudioEntity()->GetFootStepAudio().GetCachedShoeSettings()->ShoeFitness;
			sm_HLODPeds[pedQuadrant].ped->GetFootstepHelper().SetIsAudioHLOD(false);
		}
		sm_HLODPeds[pedQuadrant].ped = m_pParentPed;
		sm_HLODPeds[pedQuadrant].fitness = pedFitness;
		m_IsAudioHLOD = true;
		if(!IsAnAnimal())
		{
			sm_ShoesFitness[m_pParentPed->GetPedAudioEntity()->GetFootStepAudio().GetShoeType()] = 0.01f;
		}
	}
	//g_Count = 0;
#if __BANK
	if(sm_DrawHLODResults)
	{
		Vector3 headPos = m_pParentPed->GetBonePositionCached(BONETAG_HEAD);
		char txt[64];
		formatf(txt,"Q%u %f",pedQuadrant,pedFitness);
		grcDebugDraw::Text(headPos + Vector3(0.f,0.f,1.f),Color_white,txt);
	}
#endif
}
//-------------------------------------------------------------------------------------------------------------------
audQuadrants CPedFootStepHelper::CalculatePedQuadrant(Vec3V_In dirToPed)
{
	// Quadrant 0 Fdot + Rdot -
	audQuadrants pedQuadrant = Q0;
	ScalarV fDot = Dot(g_AudioEngine.GetEnvironment().GetVolumeListenerMatrix().GetCol1(),dirToPed);
	ScalarV rDot = Dot(g_AudioEngine.GetEnvironment().GetVolumeListenerMatrix().GetCol0(),dirToPed);
	// Quadrant 3 Fdot + Rdot +
	if(IsTrue(fDot >= ScalarV(V_ZERO)) && IsTrue(rDot >= ScalarV(V_ZERO)))
	{
		pedQuadrant = Q3;
	}
	// Quadrant 2 Fdot - Rdot +
	else if (IsTrue(fDot < ScalarV(V_ZERO)) && IsTrue(rDot >= ScalarV(V_ZERO)))
	{
		pedQuadrant = Q2;
	}
	// Quadrant 1 Fdot - Rdot -
	else if (IsTrue(fDot < ScalarV(V_ZERO)) && IsTrue(rDot < ScalarV(V_ZERO)))
	{
		pedQuadrant = Q1;
	}
	return pedQuadrant;
}
//-------------------------------------------------------------------------------------------------------------------
f32 CPedFootStepHelper::CalculatePedFitness(f32 distanceToPed)
{
	//Early out if we don't have an audio entity
	if(!m_pParentPed->GetPedAudioEntity())
	{
		return 0.f;
	}
	// The fitness is based on the shore type and the distance to the camera. 
	f32 fitness = sm_DistanceToHLODFitness.CalculateRescaledValue(0.f,1.f,0.f,GetDistanceThresholdSqToUpdateFootSteps(*m_pParentPed).Getf(),distanceToPed);
	fitness *= sm_ShoesFitness[m_pParentPed->GetPedAudioEntity()->GetFootStepAudio().GetShoeType()];
	fitness *= (m_pParentPed->GetMotionData()->GetIsStill()) ? 0.f : 1.f;
	fitness *= (m_pParentPed->GetMotionData()->GetIsRunning()) ? sm_RunFitnessScale : 1.f;
	fitness *= (IsAnAnimal()) ? sm_AnimalFitnessScale : 1.f;
	//fitness += (m_pParentPed->GetMotionData()->GetIsRunning()) ? sm_RunFitnessScale : 0.f; Adding fitness instead of scaling
	fitness = Clamp(fitness, 0.f,1.f);
	return fitness;
}
//-------------------------------------------------------------------------------------------------------------------
bool CPedFootStepHelper::IsAnAnimal()
{
	return m_pParentPed->GetPedType() == PEDTYPE_ANIMAL;
}
//-------------------------------------------------------------------------------------------------------------------
void CPedFootStepHelper::IssueAsyncLineTestProbes()
{
	const Matrix34 &rootMatrix = MAT34V_TO_MATRIX34(m_pParentPed->GetTransform().GetMatrix()); 
	Vector3 vecStart = rootMatrix.d;
	Vector3 vecEnd = rootMatrix.d;
	vecStart.z -= sm_FootStepLineTestStartOffset;
	vecEnd.z = vecStart.z - sm_FootStepLineTestLong;
	u32 nFlags = ArchetypeFlags::GTA_MAP_TYPE_MOVER | ArchetypeFlags::GTA_VEHICLE_TYPE | ArchetypeFlags::GTA_PED_TYPE | ArchetypeFlags::GTA_OBJECT_TYPE;
	m_CachedLineTest.m_MainLineTest.AddProbe(vecStart,vecEnd,m_pParentPed,nFlags,false);
#if __BANK
	if(sm_DrawDebugInfo)
	{
		grcDebugDraw::Line(vecStart,vecEnd,Color_red,Color_red);
	}
#endif
}
//-------------------------------------------------------------------------------------------------------------------
void CPedFootStepHelper::PreUpdateFootstepGroundTest(Vec3V_InOut groundPosition,Vec3V_InOut groundNormal)
{
	// Send an async probe.
	if(m_HasToInitGroundTest)
	{
		PF_FUNC(PreUpdateFootstepGTest);
		if(m_CachedLineTest.Clear())
		{
			m_HasResultThisFrame = false; 
			m_HasToInitGroundTest = false;
			IssueAsyncLineTestProbes();
		}
	}
	else
	{
		PF_FUNC(UpdateFootstepGTest);
		// Update the active tests.
		UpdateFootstepGroundTest(groundPosition, groundNormal);
	}
}
//-------------------------------------------------------------------------------------------------------------------
bool CPedFootStepHelper::ProcessAsyncLineTestProbes()
{
	PF_FUNC(ProcessAsyncLineTProbes);
	audCachedLos & lineTest = m_CachedLineTest.m_MainLineTest;

	if(lineTest.m_bContainsProbe)
	{
		if(lineTest.m_bHitSomething==true)
		{
			m_CachedLineTest.m_GroundPosition = lineTest.m_vIntersectPos;
			m_CachedLineTest.m_GroundNormal = lineTest.m_vIntersectNormal;
			return true;
		}
	}
	return false;
}
//-------------------------------------------------------------------------------------------------------------------
void CPedFootStepHelper::SyncProbesGroundTest(Vec3V_InOut groundPosition,Vec3V_InOut groundNormal)
{
	phIntersection testIntersections[AUD_NUM_INTERSECTIONS_FOR_PED_PROBES];
	phSegment testSegment;
	const Matrix34 &rootMatrix = MAT34V_TO_MATRIX34(m_pParentPed->GetTransform().GetMatrix()); 
	Vector3 vecStart = rootMatrix.d;
	Vector3 vecEnd = rootMatrix.d;
	vecEnd.z -= PED_HUMAN_GROUNDTOROOTOFFSET  * sm_FootStepLineTestLong;

	testSegment.Set(vecStart, vecEnd);
#if __BANK
	Vector3 headPos = m_pParentPed->GetBonePositionCached(BONETAG_HEAD);
	if(sm_DrawDebugInfo)
	{
		grcDebugDraw::Sphere(headPos,0.4f,Color_red,true);
		grcDebugDraw::Line(vecStart,vecEnd,Color_red,Color_red);
	}
#endif
	u32 nFlags = ArchetypeFlags::GTA_MAP_TYPE_MOVER |	ArchetypeFlags::GTA_VEHICLE_TYPE | ArchetypeFlags::GTA_PED_TYPE | ArchetypeFlags::GTA_OBJECT_TYPE;
	int nNumResults = CPhysics::GetLevel()->TestProbe(testSegment, testIntersections, m_pParentPed->GetCurrentPhysicsInst(), nFlags, TYPE_FLAGS_ALL, phLevelBase::STATE_FLAGS_ALL, AUD_NUM_INTERSECTIONS_FOR_PED_PROBES);

	int nUseIntersection = -1;
	for(int i=0; i<nNumResults; i++)
	{

		if(testIntersections[i].IsAHit())
		{
			CEntity* pHitEntity = CPhysics::GetEntityFromInst(testIntersections[i].GetInstance());
			if(pHitEntity && pHitEntity->IsCollisionEnabled())
			{
				// check we've got the first intersection along the probe
				if(nUseIntersection < 0 || IsTrue(testIntersections[i].GetPosition().GetZ() > testIntersections[nUseIntersection].GetPosition().GetZ()))
				{
					nUseIntersection = i;
				}
			}
		}
		else
			break;
	}
	if(nUseIntersection > -1)
	{
		if(testIntersections[nUseIntersection].GetInstance() && CPhysics::GetEntityFromInst(testIntersections[nUseIntersection].GetInstance())
			&& CPhysics::GetEntityFromInst(testIntersections[nUseIntersection].GetInstance())->GetIsPhysical()
			&& ((CPhysical*)CPhysics::GetEntityFromInst(testIntersections[nUseIntersection].GetInstance()))->GetAttachParent() == m_pParentPed)
		{
			//Assertf(false, "Ped is trying to stand on something he's attached to");
			return;
		}
#if __BANK
		if(sm_DrawDebugInfo)
		{
			grcDebugDraw::Sphere(headPos + Vector3(0.f,0.f,0.5f),0.2f,Color_yellow,true);
		}
#endif
		phMaterialMgr::Id	ProbeHitMaterial=0;
		ProbeHitMaterial = testIntersections[nUseIntersection].GetMaterialId();
		groundPosition = testIntersections[nUseIntersection].GetPosition();
		groundNormal = testIntersections[nUseIntersection].GetNormal();
		if(m_pParentPed->GetPedAudioEntity())
		{
			m_pParentPed->GetPedAudioEntity()->GetFootStepAudio().SetStandingMaterial(ProbeHitMaterial, CPhysics::GetEntityFromInst(testIntersections[nUseIntersection].GetInstance()),  testIntersections[nUseIntersection].GetComponent());
		}
	}	
}

//-------------------------------------------------------------------------------------------------------------------
void CPedFootStepHelper::SyncProbesFromFeetGroundTest(Vec3V_InOut groundPosition,Vec3V_InOut groundNormal)
{
	phIntersection testIntersections[AUD_NUM_INTERSECTIONS_FOR_PED_PROBES];
	phSegment testSegment;
	const Matrix34 &rootMatrix = MAT34V_TO_MATRIX34(m_pParentPed->GetTransform().GetMatrix()); 
	Vector3 vecStart = rootMatrix.d;
	Vector3 vecEnd = rootMatrix.d;

	vecStart.z -= sm_FootStepLineTestStartOffset;
	vecEnd.z = vecStart.z - sm_FootStepLineTestLong;
	testSegment.Set(vecStart, vecEnd);
#if __BANK
	if(sm_DrawDebugInfo)
	{
		Vector3 headPos = m_pParentPed->GetBonePositionCached(BONETAG_HEAD);


		grcDebugDraw::Sphere(headPos + Vector3(0.f,0.f,0.5f),0.2f,Color_red,true);
		grcDebugDraw::Line(vecStart,vecEnd,Color_red,Color_red);
	}
#endif

	u32 nFlags = ArchetypeFlags::GTA_MAP_TYPE_MOVER | ArchetypeFlags::GTA_VEHICLE_TYPE | ArchetypeFlags::GTA_PED_TYPE | ArchetypeFlags::GTA_OBJECT_TYPE;
	int nNumResults = CPhysics::GetLevel()->TestProbe(testSegment, testIntersections, m_pParentPed->GetCurrentPhysicsInst(), nFlags, TYPE_FLAGS_ALL, phLevelBase::STATE_FLAGS_ALL, sm_NumIntersections);

	int nUseIntersection = -1;
	for(int i=0; i<nNumResults; i++)
	{

		if(testIntersections[i].IsAHit())
		{
			CEntity* pHitEntity = CPhysics::GetEntityFromInst(testIntersections[i].GetInstance());
			if(pHitEntity &&  pHitEntity->IsCollisionEnabled())
			{
				// check we've got the first intersection along the probe
				if(nUseIntersection < 0 || IsTrue(testIntersections[i].GetPosition().GetZ() > testIntersections[nUseIntersection].GetPosition().GetZ()))
				{
					nUseIntersection = i;
				}
			}
		}
		else
			break;
	}
	if(nUseIntersection > -1)
	{
		if(testIntersections[nUseIntersection].GetInstance() && CPhysics::GetEntityFromInst(testIntersections[nUseIntersection].GetInstance())
			&& CPhysics::GetEntityFromInst(testIntersections[nUseIntersection].GetInstance())->GetIsPhysical()
			&& ((CPhysical*)CPhysics::GetEntityFromInst(testIntersections[nUseIntersection].GetInstance()))->GetAttachParent()==m_pParentPed)
		{
			//Assertf(false, "Ped is trying to stand on something he's attached to");
			return;
		}

		phMaterialMgr::Id	ProbeHitMaterial=0;
		ProbeHitMaterial = testIntersections[nUseIntersection].GetMaterialId();
		groundPosition = testIntersections[nUseIntersection].GetPosition();
		groundNormal = testIntersections[nUseIntersection].GetNormal();
		m_pParentPed->GetPedAudioEntity()->GetFootStepAudio().SetStandingMaterial(ProbeHitMaterial, CPhysics::GetEntityFromInst(testIntersections[nUseIntersection].GetInstance()),  testIntersections[nUseIntersection].GetComponent());
		// We have to store the new result by LRU scheme.
		if(CPedFootStepHelper::sm_NumberOfTestResultStored < MAX_NUMBER_OF_GROUND_FOOTSTEPS_TESTS)
		{
			// No override needed.
			for (s32 i = 0; i < MAX_NUMBER_OF_GROUND_FOOTSTEPS_TESTS; ++i )
			{
				if (CPedFootStepHelper::sm_GroundFootStepTests[i].height == -5000)
				{
					CPedFootStepHelper::sm_GroundFootStepTests[i].height = rootMatrix.d.GetZ();
					CPedFootStepHelper::sm_GroundFootStepTests[i].numberOfHits = 1; 
					CPedFootStepHelper::sm_GroundFootStepTests[i].groundPos = testIntersections[nUseIntersection].GetPosition();
					CPedFootStepHelper::sm_GroundFootStepTests[i].groundNormal = testIntersections[nUseIntersection].GetNormal();
					CPedFootStepHelper::sm_NumberOfTestResultStored++;
					break;
				}
			}
		}else
		{
			s32 badCandidateIndex = 0; 
			for (s32 i = 0; i < MAX_NUMBER_OF_GROUND_FOOTSTEPS_TESTS; ++i )
			{
				// Look for a bad candidate so we'll override it if needed.
				if(CPedFootStepHelper::sm_GroundFootStepTests[i].lastNumberOfHits < CPedFootStepHelper::sm_GroundFootStepTests[badCandidateIndex].lastNumberOfHits)
				{
					badCandidateIndex = i;
				}

			}
			CPedFootStepHelper::sm_GroundFootStepTests[badCandidateIndex].height = rootMatrix.d.GetZ();
			CPedFootStepHelper::sm_GroundFootStepTests[badCandidateIndex].numberOfHits = 1; 
			CPedFootStepHelper::sm_GroundFootStepTests[badCandidateIndex].groundPos = testIntersections[nUseIntersection].GetPosition();
			CPedFootStepHelper::sm_GroundFootStepTests[badCandidateIndex].groundNormal = testIntersections[nUseIntersection].GetNormal();
		}	
		Assertf((CPedFootStepHelper::sm_NumberOfTestResultStored<=MAX_NUMBER_OF_GROUND_FOOTSTEPS_TESTS),"Bad number of test on updateFootSteps");
	}	
}
//-------------------------------------------------------------------------------------------------------------------
void CPedFootStepHelper::ProcessFootstepVfx(audFootstepEvent event, bool hindLegs, Mat34V_In footMatrix, const u32 foot, Vec3V_In groundNormal)
{
	if (m_pParentPed->GetPedVfx() == NULL)
	{
		return;
	}
	naAssertf(IsFiniteAll(footMatrix),"Wrong foot matrix.");
	if (event==AUD_FOOTSTEP_WALK_L || 
		event==AUD_FOOTSTEP_SCUFFHARD_L || 
		event==AUD_FOOTSTEP_SCUFF_L ||
		event==AUD_FOOTSTEP_JUMP_LAND_L || 
		event==AUD_FOOTSTEP_RUN_L || 
		event==AUD_FOOTSTEP_SPRINT_L || 
		event==AUD_FOOTSTEP_SOFT_L)
	{
		m_pParentPed->GetPedVfx()->ProcessVfxFootStep(true, hindLegs, footMatrix, foot, groundNormal);
	}
	else if (event==AUD_FOOTSTEP_WALK_R || 
		event==AUD_FOOTSTEP_SCUFFHARD_R || 
		event==AUD_FOOTSTEP_SCUFF_R ||
		event==AUD_FOOTSTEP_JUMP_LAND_R || 
		event==AUD_FOOTSTEP_RUN_R || 
		event==AUD_FOOTSTEP_SPRINT_R || 
		event==AUD_FOOTSTEP_SOFT_R)
	{
		m_pParentPed->GetPedVfx()->ProcessVfxFootStep(false, hindLegs,footMatrix, foot, groundNormal);
	}
}
//-------------------------------------------------------------------------------------------------------------------
void CPedFootStepHelper::InitDetectionValuesIndices()
{
	for (u8 i = 0; i < m_pModPhys->numModes; i++)
	{
		audDetectionValues detectionMode = FindDetectionModeForHash(m_pModPhys->Modes[i].Name);
		m_DetectionValuesIndices[detectionMode] = i;
	}
}
//-------------------------------------------------------------------------------------------------------------------
audDetectionValues CPedFootStepHelper::FindDetectionModeForHash(const u32 modeHash)
{
	const u32 s_Lookuptable[ModelPhysicsParams::MAX_MODES][2] = {
		{ATSTRINGHASH("STOP", 0x930CA7F2), audDVStop},
		{ATSTRINGHASH("WALK", 0x83504C9C), audDVWalk},
		{ATSTRINGHASH("RUN", 0x1109B569), audDVRun},
		{ATSTRINGHASH("SPRINT", 0xBC29E48), audDVSprint},
		{ATSTRINGHASH("JUMP", 0x2E90C5D5), audDVJump},
		{ATSTRINGHASH("STAIRS_STOP", 0xA8EA8DB3), audDVStairsStop},
		{ATSTRINGHASH("STAIRS_WALK", 0x2A29F8E2), audDVStairsWalk},
		{ATSTRINGHASH("STAIRS_RUN", 0xD540F6ED), audDVStairsRun},
		{ATSTRINGHASH("SLOPES", 0x6C7A1EAF), audDVSlopes},
		{ATSTRINGHASH("SLOPES_RUN", 0x4EA5C142), audDVSlopesRun},
		{ATSTRINGHASH("STEALTH_WALK", 0xD028448C), audDVStealthWalk},
		{ATSTRINGHASH("STEALTH_RUN", 0x3224A955), audDVStealthRun},
		{ATSTRINGHASH("LADDER", 0x1A53E8E4), audDVLadder},
		{ATSTRINGHASH("VAULT", 0x4806CCCA), audDVVault}};

		for (u8 i = 0; i < NELEM(s_Lookuptable); i++)
		{
			if (s_Lookuptable[i][0] == modeHash)
			{
				return (audDetectionValues)s_Lookuptable[i][1];
			}
		}
		return audDVWalk;
}
//-------------------------------------------------------------------------------------------------------------------
void CPedFootStepHelper::UpdatePhysicProbes(Vec4V_InOut groundPlane)
{
	// In case the Ik solver has a result this frame, use the ground position from it, otherwise store the ped ground position and normal 
	// in the first element of the array. 
	if(!m_HasIkResultsThisFrame)
	{
		m_GroundPosition[0] = VECTOR3_TO_VEC3V(m_pParentPed->GetGroundPos());
		m_GroundNormal[0] = VECTOR3_TO_VEC3V(m_pParentPed->GetGroundNormal());

		if(sm_WasPlayerSwitchActive || m_ScriptForceUpdate)
		{
			m_IsPlayingSwitchScene = m_pParentPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning( CTaskTypes::TASK_SYNCHRONIZED_SCENE )
				|| m_pParentPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning( CTaskTypes::TASK_SCRIPTED_ANIMATION );
		}
		if(BANK_ONLY(sm_ForceSyncProbesToAllPeds ||) m_IsPlayingSwitchScene || (!m_pParentPed->GetIsStanding() && (m_pParentPed->GetUseExtractedZ() && ! m_pParentPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning( CTaskTypes::TASK_CLIMB_LADDER ))) )
		{
			if(!sm_AsyncProbesGroundTest)
			{
				SyncProbesGroundTest(m_GroundPosition[0],m_GroundNormal[0]);
			}else 
			{
				FootstepGroundTest(m_GroundPosition[0],m_GroundNormal[0]);
			}
#if __BANK
			if(sm_DrawResults)
			{
				grcDebugDraw::Line(m_GroundPosition[0],m_GroundPosition[0] + ScalarV(20) * m_GroundNormal[0],Color_beige,Color_beige);
			}
#endif
		}
		groundPlane = Vec4V(Vec::V4PermuteTwo<Vec::X1,Vec::Y1,Vec::Z1,Vec::X2>( m_GroundNormal[0].GetIntrin128(), 
						(m_GroundNormal[0].GetX() * m_GroundPosition[0].GetX()
						+ m_GroundNormal[0].GetY() * m_GroundPosition[0].GetY() 
						+ m_GroundNormal[0].GetZ() * m_GroundPosition[0].GetZ()).GetIntrin128() ));

	/*	groundPlane.SetXYZ(m_GroundNormal[0]);
		groundPlane.SetW((m_GroundNormal[0].GetX() * m_GroundPosition[0].GetX())
						+ m_GroundNormal[0].GetY() * m_GroundPosition[0].GetY() 
						+ m_GroundNormal[0].GetZ() * m_GroundPosition[0].GetZ()) ;*/
	}
	else
	{
		sm_WasPlayerSwitchActive = false;
	}
}
//-------------------------------------------------------------------------------------------------------------------
void CPedFootStepHelper::ComputeStepInfo( f32 pedVelocity,audFootstepEvent &stepType,u8 &detectionValuesIndex)
{
	BANK_ONLY (char text[256]);
	if(m_pParentPed->GetPedResetFlag(CPED_RESET_FLAG_PedHitWallLastFrame) )
	{
		BANK_ONLY (formatf(text,"SPECIAL_CASE"));
		detectionValuesIndex = m_DetectionValuesIndices[audDVSprint];
		stepType = (audFootstepEvent)(AUD_FOOTSTEP_WALK_L);
	}
	else if( m_ScriptForceStepType && naVerifyf(m_ScriptDetectionValuesIdx < MaxNumberAudDetValues,"Wrong detection value idx coming from script"))
	{
		BANK_ONLY (formatf(text,"SCRIPT"));
		detectionValuesIndex = m_DetectionValuesIndices[m_ScriptDetectionValuesIdx];
		stepType = m_ScriptStepType;
	}
	else 
	{
		if(m_IsWalkingOnSlope && !m_pParentPed->GetPedConfigFlag(CPED_CONFIG_FLAG_StairsDetected))
		{
			pedVelocity -= sm_SlopeAngleToDeltaSpeed.CalculateValue(RtoD * fabs(m_SlopeAngle));
			pedVelocity = Max(pedVelocity, 0.f);
		}
		if(m_pParentPed->IsLocalPlayer() && (m_pParentPed->GetPedConfigFlag(CPED_CONFIG_FLAG_StairsDetected) || m_WasOnStairs))
		{
			if( pedVelocity != 0.f)
			{
				pedVelocity += sm_StairsDeltaSpeed;
			}
		}
		
		bool isVaulting = m_pParentPed->GetPedResetFlag(CPED_RESET_FLAG_IsVaulting);
		if (isVaulting)
		{
			BANK_ONLY (formatf(text,"VAULT"));
			detectionValuesIndex = m_DetectionValuesIndices[audDVVault];
			stepType = (audFootstepEvent)(AUD_FOOTSTEP_RUN_L);
		}
		else if (m_pParentPed->GetPedResetFlag(CPED_RESET_FLAG_InCoverTaskActive))
		{
			BANK_ONLY (formatf(text,"IN COVER USING RUN"));
			detectionValuesIndex = m_DetectionValuesIndices[audDVRun];
			stepType = (audFootstepEvent)(AUD_FOOTSTEP_RUN_L);
		}
		else if(pedVelocity <= m_pModPhys->StopSpeedThreshold && m_pParentPed->GetDeepSurfaceInfo().GetDepth() <= 0.05f) 
		{
			//moving = false;
			BANK_ONLY (formatf(text,"STOP"));
			detectionValuesIndex = m_DetectionValuesIndices[audDVStop];
			m_pParentPed->GetPedAudioEntity()->GetFootStepAudio().ResetLastMoveBlendRatio();
			stepType = (audFootstepEvent)(AUD_FOOTSTEP_SOFT_L);
			if(m_pParentPed->GetPedConfigFlag(CPED_CONFIG_FLAG_StairsDetected) || m_WasOnStairs)
			{
				BANK_ONLY (formatf(text,"STAIRS_STOP"));
				detectionValuesIndex = m_DetectionValuesIndices[audDVStairsStop];
			}
			else if(m_pParentPed->GetMotionData()->GetUsingStealth())
			{
				BANK_ONLY (formatf(text,"STEALTH_STOP using STOP"));
				detectionValuesIndex = m_DetectionValuesIndices[audDVStop];
				//detectionValuesIndex = m_DetectionValuesIndices[audDVStealthWalk];
			}
			else if (m_IsWalkingOnSlope)
			{
				BANK_ONLY (formatf(text,"SLOPE_STOP using SLOP_WALK"));
				detectionValuesIndex = m_DetectionValuesIndices[audDVSlopes];
				stepType = (audFootstepEvent)(AUD_FOOTSTEP_WALK_L);
			}
		}
		else if(pedVelocity <= m_pModPhys->WalkSpeedThreshold) 
		{
			BANK_ONLY (formatf(text,"WALK"));
			detectionValuesIndex = m_DetectionValuesIndices[audDVWalk];
			if(m_pParentPed->GetPedConfigFlag(CPED_CONFIG_FLAG_StairsDetected) || m_WasOnStairs)
			{
				BANK_ONLY (formatf(text,"STAIRS_WALK"));
				detectionValuesIndex = m_DetectionValuesIndices[audDVStairsWalk];
			}
			else if(m_pParentPed->GetMotionData()->GetUsingStealth())
			{
				BANK_ONLY (formatf(text,"STEALTH_WALK using WALK"));
				detectionValuesIndex = m_DetectionValuesIndices[audDVWalk];
				//detectionValuesIndex = m_DetectionValuesIndices[audDVStealthWalk];
			}
			else if (m_pParentPed->GetPedResetFlag(CPED_RESET_FLAG_HasGunTaskWithAimingState))
			{
				BANK_ONLY (formatf(text,"AIM_WALK using RUN"));
				detectionValuesIndex = m_DetectionValuesIndices[audDVRun];
			}
			else if (m_IsWalkingOnSlope)
			{
				BANK_ONLY (formatf(text,"SLOPE_WALK  using SLOPES"));
				detectionValuesIndex = m_DetectionValuesIndices[audDVSlopes];
			}
			stepType = (audFootstepEvent)(AUD_FOOTSTEP_WALK_L);
		}	
		else if(pedVelocity <= m_pModPhys->RunSpeedThreshold) 
		{
			BANK_ONLY (formatf(text,"RUN"));
			detectionValuesIndex = m_DetectionValuesIndices[audDVRun];
			if(m_pParentPed->GetPedConfigFlag(CPED_CONFIG_FLAG_StairsDetected) || m_WasOnStairs)
			{
				BANK_ONLY (formatf(text,"STAIRS_RUN"));
				detectionValuesIndex = m_DetectionValuesIndices[audDVStairsRun];
			}
			else if(m_pParentPed->GetMotionData()->GetUsingStealth())
			{
				BANK_ONLY (formatf(text,"STEALTH_RUN using RUN"));
				detectionValuesIndex = m_DetectionValuesIndices[audDVRun];
				//detectionValuesIndex = m_DetectionValuesIndices[audDVStealthRun];
			}
			else if (m_IsWalkingOnSlope)
			{
				BANK_ONLY (formatf(text,"SLOPE_RUN using SLOPES"));
				detectionValuesIndex = m_DetectionValuesIndices[audDVSlopesRun];
			}
			stepType = (audFootstepEvent)(AUD_FOOTSTEP_RUN_L);
		}
		else
		{
			BANK_ONLY (formatf(text,"SPRINT"));
			detectionValuesIndex = m_DetectionValuesIndices[audDVSprint];
			if(m_pParentPed->GetPedConfigFlag(CPED_CONFIG_FLAG_StairsDetected) || m_WasOnStairs)
			{
				BANK_ONLY (formatf(text,"STAIRS_SPRINT using STAIRS_RUN"));
				detectionValuesIndex = m_DetectionValuesIndices[audDVStairsRun];
			}
			else if(m_pParentPed->GetMotionData()->GetUsingStealth())
			{
				BANK_ONLY (formatf(text,"STEALTH_RUN using RUN"));
				detectionValuesIndex = m_DetectionValuesIndices[audDVRun];
				//detectionValuesIndex = m_DetectionValuesIndices[audDVStealthRun];
			}
			else if (m_IsWalkingOnSlope)
			{
				BANK_ONLY (formatf(text,"SLOPE_RUN using SLOPES"));
				detectionValuesIndex = m_DetectionValuesIndices[audDVSlopesRun];
			}
			stepType = (audFootstepEvent)(AUD_FOOTSTEP_SPRINT_L);
		}
	}

#if __BANK
	if(sm_ShowPedSpeed)
	{
		char txt[512];
		Vector3 headPos = m_pParentPed->GetBonePositionCached(BONETAG_HEAD);
		formatf(txt,"%s -> %f ",text,pedVelocity);
		grcDebugDraw::AddDebugOutput(text);
		//formatf(text,"ped angular vel -> %f ",angularVelocity);
		//grcDebugDraw::AddDebugOutput(text);
		//formatf(text,"angular acceleration -> %f",fabs((angularVelocity - m_LastPedAngVel) / fwTimer::GetTimeStep()));
		//grcDebugDraw::AddDebugOutput(text);
		grcDebugDraw::Text(headPos,Color_white,txt);
		naDisplayf("%s %f",txt,pedVelocity);
	}
#endif
}
//-------------------------------------------------------------------------------------------------------------------
void CPedFootStepHelper::UpdateFootPlanted(const u32 footId BANK_ONLY(,ScalarV_In footSpeed), const f32 upDistanceToGround, const u8 detectionValuesIndex BANK_ONLY(,const audFootstepEvent stepType),Vec3V_In /*downSlopeDirection*/ BANK_ONLY(,Vec3V_In heelPos))
{
#if __BANK
	if(sm_ShowFootStepTuningInfo)
	{	
		naDisplayf("______________________________________________________________________________________________");
		char txt[128];
		formatf(txt,"PLANTED: ");
		naDisplayf("%s", txt);
		formatf(txt,"Foot %d  [footSpeed = %f] [upDistanceToGround = %f] [groundDistanceUpEpsilon = %f]",footId,footSpeed.Getf(),upDistanceToGround,m_pModPhys->Modes[detectionValuesIndex].GroundDistanceUpEpsilon);
		naDisplayf("%s", txt);

	}
#endif
	//Check if the foot is far enough from the groudn to even consider if it's moving
	if(upDistanceToGround > m_pModPhys->Modes[detectionValuesIndex].GroundDistanceUpEpsilon )
	{
#if __BANK
		if(sm_PauseOnLimits )
		{	
			Color32 color = Color_red;
			if(stepType == AUD_FOOTSTEP_SOFT_L || stepType == AUD_FOOTSTEP_SOFT_R)
			{
				color = Color_green;
			}
			else if ( stepType == AUD_FOOTSTEP_JUMP_LAND_L || stepType == AUD_FOOTSTEP_JUMP_LAND_R )
			{
				color = Color_purple;
			}
			grcDebugDraw::Line(heelPos,heelPos + Vec3V(0.5f,0.f,0.f),color,color,50);
			grcDebugDraw::Line(heelPos,heelPos + Vec3V(0.f,0.5f,0.f),color,color,50);
			grcDebugDraw::Line(heelPos,heelPos + Vec3V(0.f,0.f,0.5f),color,color,50);

			fwTimer::StartUserPause();
		}
#endif
		if(m_pParentPed->GetMotionData()->GetUsingStealth())
		{
			f32 scuffProb = sm_PlayerClumsinessToScuffProb.CalculateValue(m_pParentPed->GetPedAudioEntity()->GetFootStepAudio().GetPlayerClumsiness());
			if(audEngineUtil::ResolveProbability(scuffProb))
			{
				m_pParentPed->GetPedAudioEntity()->GetFootStepAudio().AddFootstepEvent((audFootstepEvent)(AUD_FOOTSTEP_SCUFF_L + footId));
				//if(m_IsWalkingOnSlope)
				//{
					//m_pParentPed->GetPedAudioEntity()->GetFootStepAudio().AddSlopeDebrisEvent((audFootstepEvent)(AUD_FOOTSTEP_SCUFF_L + footId),downSlopeDirection,m_SlopeAngle);
				//}
#if __BANK
				if(sm_PauseOnDragEvents)
				{
					naDisplayf("[footSpeed = %f] [downSpeedEpsilon = %f] [upDistanceToGround = %f]", footSpeed.Getf(), m_pModPhys->Modes[detectionValuesIndex].DownSpeedEpsilon, upDistanceToGround);
					grcDebugDraw::Line(heelPos,heelPos + Vec3V(0.5f,0.f,0.f),Color_blue,Color_blue,50);
					grcDebugDraw::Line(heelPos,heelPos + Vec3V(0.f,0.5f,0.f),Color_blue,Color_blue,50);
					grcDebugDraw::Line(heelPos,heelPos + Vec3V(0.f,0.f,0.5f),Color_blue,Color_blue,50);
					fwTimer::StartUserPause();
				}
				else if (sm_ShowFootStepTuningInfo)
				{
					grcDebugDraw::Line(heelPos,heelPos + Vec3V(0.5f,0.f,0.f),Color_blue,Color_blue,50);
					grcDebugDraw::Line(heelPos,heelPos + Vec3V(0.f,0.5f,0.f),Color_blue,Color_blue,50);
					grcDebugDraw::Line(heelPos,heelPos + Vec3V(0.f,0.f,0.5f),Color_blue,Color_blue,50);
				}
#endif
			}
		}
		m_AlreadyLifted.Clear(footId);
		m_AlreadyTriggered.Clear(footId);
		m_FootMoveState[footId] = kMoveMoving;
	}
}
//-------------------------------------------------------------------------------------------------------------------
void CPedFootStepHelper::SetWalkingOnPuddle(bool walkingOnPuddle)
{
	sm_IsWalkingOnPuddle = walkingOnPuddle;
}
//-------------------------------------------------------------------------------------------------------------------
bool CPedFootStepHelper::IsWalkingOnPuddle()
{
	if( m_pParentPed && m_pParentPed->IsLocalPlayer())
	{
		return sm_IsWalkingOnPuddle;
	}
	return false;
}
//-------------------------------------------------------------------------------------------------------------------
void CPedFootStepHelper::UpdateFootMoving(const u32 footId,ScalarV_In footSpeed, const f32 upDistanceToGround, const u8 detectionValuesIndex,audFootstepEvent stepType,Vec3V_In downSlopeDirection,bool hindFoot,const u32 /*foot*/,Mat34V_In footMatrix BANK_ONLY(,Vec3V_In heelPos))
{
#if GTA_REPLAY
	if(CReplayMgr::IsEditModeActive())
	{
		return;
	}
#endif

#if __BANK
	if(sm_ShowFootStepTuningInfo )
	{	
		naDisplayf("______________________________________________________________________________________________");
		char txt[128];
		formatf(txt,"MOVING: ");
		naDisplayf("%s", txt);
		formatf(txt,"Foot %d [footSpeed %f] [downSpeedEpsilon %f] [upDistanceToGround = %f] [groundDistanceDownEpsilon = %f]",footId,footSpeed.Getf(),m_pModPhys->Modes[detectionValuesIndex].DownSpeedEpsilon,upDistanceToGround,m_pModPhys->Modes[detectionValuesIndex].GroundDistanceDownEpsilon);
		naDisplayf("%s", txt);

	}
#endif
	if (m_AlreadyLifted.IsClear(footId) && (upDistanceToGround > sm_UpDistaneToTriggerLift) && m_pParentPed->GetDeepSurfaceInfo().GetDepth() > 0.f)
	{
		m_AlreadyLifted.Set(footId);
		m_pParentPed->GetPedAudioEntity()->GetFootStepAudio().AddFootstepEvent((audFootstepEvent)(AUD_FOOTSTEP_LIFT_L + (footId % 2)));
	}
	bool applyDeltaFix = false; 
	if( m_pParentPed->IsLocalPlayer() && footId == 1 && detectionValuesIndex == audDVStairsRun && m_pParentPed->GetPedConfigFlag(CPED_CONFIG_FLAG_StairsDetected) && m_PrevHeight < m_pParentPed->GetTransform().GetPosition().GetZf())
	{
		if( audNorthAudioEngine::GetGtaEnvironment() && audNorthAudioEngine::GetGtaEnvironment()->HasNavMeshInfoBeenUpdated() && audNorthAudioEngine::GetGtaEnvironment()->GetListenerInteriorInstance() && audNorthAudioEngine::GetGtaEnvironment()->GetListenerInteriorInstance()->GetBaseModelInfo()
			&& (audNorthAudioEngine::GetGtaEnvironment()->GetListenerInteriorInstance()->GetBaseModelInfo()->GetModelNameHash() == ATSTRINGHASH("v_sweat", 0x5DE36999) 
			|| audNorthAudioEngine::GetGtaEnvironment()->GetListenerInteriorInstance()->GetBaseModelInfo()->GetModelNameHash() == ATSTRINGHASH("v_sweatempty", 0xC404F5CB)))
		{
			applyDeltaFix = true ;
		}
	}
	u32 deltaTimeFix = applyDeltaFix ? 250 : 0;
	if( m_LastTimeFootPlanted[footId] + m_pModPhys->Modes[detectionValuesIndex].TimeToRetrigger + deltaTimeFix < g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0))
	{
		f32 deltaFix =  applyDeltaFix ? 0.25f : 0.f;
		if(upDistanceToGround < m_pModPhys->Modes[detectionValuesIndex].GroundDistanceDownEpsilon + deltaFix )
		{ 
#if __BANK
			if(sm_PauseOnLimits )
			{	
				Color32 color = Color_red;
				if(stepType == AUD_FOOTSTEP_SOFT_L || stepType == AUD_FOOTSTEP_SOFT_R)
				{
					color = Color_green;
				}
				grcDebugDraw::Line(heelPos,heelPos + Vec3V(0.5f,0.f,0.f),color,color,25);
				grcDebugDraw::Line(heelPos,heelPos + Vec3V(0.f,0.5f,0.f),color,color,25);
				grcDebugDraw::Line(heelPos,heelPos + Vec3V(0.f,0.f,0.5f),color,color,25);


				fwTimer::StartUserPause();
			}
#endif
			f32 deltaSpeed = 0.f;
			if(m_SlidingDownALadder)
			{
				deltaSpeed = 5.f;
			}
			if (m_pParentPed->GetPedAudioEntity() && m_pParentPed->GetPedAudioEntity()->GetFootStepAudio().GetIsStandingOnVehicle())
			{
				CVehicle* vehicle = ((CVehicle*) m_pParentPed->GetPedAudioEntity()->GetFootStepAudio().GetLastStandingEntity());
				deltaSpeed = fabs(vehicle->GetVelocity().Mag());
			}
			// Check the footsped for audio.
			if(m_AlreadyTriggered.IsClear(footId) && (footSpeed.Getf() < (m_pModPhys->Modes[detectionValuesIndex].DownSpeedEpsilon + deltaSpeed) ))
			{
				m_AlreadyTriggered.Set(footId);
				if(m_IsPlayingSwitchScene)
				{
					stepType = (audFootstepEvent)(AUD_FOOTSTEP_WALK_L + (footId % 2));
				}
				else if(m_WasClimbingLadder.IsSet(footId))
				{
					stepType = (audFootstepEvent)(AUD_FOOTSTEP_SOFT_L + (footId % 2));
					if(m_SlidingDownALadder)
					{
						stepType = (audFootstepEvent)(AUD_FOOTSTEP_JUMP_LAND_L + (footId % 2));
					}
					m_WasClimbingLadder.Clear(footId);
					if( m_WasClimbingLadder.IsClear(0) && m_WasClimbingLadder.IsClear(1))
					{
						m_SlidingDownALadder = false;
					}
				}
				else if(m_WasInTheAir.IsSet(footId))
				{
					stepType = (audFootstepEvent)(AUD_FOOTSTEP_JUMP_LAND_L + (footId % 2));
					m_WasInTheAir.Clear(footId);
				}
				m_pParentPed->GetPedAudioEntity()->GetFootStepAudio().AddFootstepEvent(stepType);
				if(m_pParentPed->GetPedResetFlag(CPED_RESET_FLAG_InContactWithFoliage) || m_pParentPed->GetPedResetFlag(CPED_RESET_FLAG_InContactWithBIGFoliage))
				{
					m_pParentPed->GetPedAudioEntity()->GetFootStepAudio().AddBushEvent(stepType, m_pParentPed->GetContactedFoliageHash());
				}
				if(!IsAnAnimal())
				{
					if(stepType != AUD_FOOTSTEP_SOFT_L && stepType != AUD_FOOTSTEP_SOFT_R)
					{
						m_pParentPed->GetPedAudioEntity()->GetFootStepAudio().AddClothEvent(stepType);
					}
					m_pParentPed->GetPedAudioEntity()->GetFootStepAudio().AddPetrolCanEvent(stepType);
					m_pParentPed->GetPedAudioEntity()->GetFootStepAudio().AddMolotovEvent(stepType);
					if(m_pParentPed->GetPedAudioEntity()->GetFootStepAudio().ShouldScriptAddSweetenersEvents())
					{
						m_pParentPed->GetPedAudioEntity()->GetFootStepAudio().AddScriptSweetenerEvent(stepType);
					}
				}
				m_pParentPed->GetPedAudioEntity()->GetFootStepAudio().AddWaterEvent(stepType);

				if(m_IsWalkingOnSlope && (stepType != AUD_FOOTSTEP_SOFT_L && stepType != AUD_FOOTSTEP_SOFT_R))
				{
					if(audEngineUtil::ResolveProbability(sm_SlopeDebrisProb))
					{
						m_pParentPed->GetPedAudioEntity()->GetFootStepAudio().AddSlopeDebrisEvent(stepType,downSlopeDirection,m_SlopeAngle);
					}
				}
#if __BANK
				if(sm_PauseOnFootstepEvents)
				{
					naDisplayf("[footSpeed = %f] [downSpeedEpsilon = %f] [upDistanceToGround = %f]",  footSpeed.Getf(), m_pModPhys->Modes[detectionValuesIndex].DownSpeedEpsilon, upDistanceToGround);
					Color32 color = Color_red;
					if(stepType == AUD_FOOTSTEP_SOFT_L || stepType == AUD_FOOTSTEP_SOFT_R)
					{
						color = Color_green;
					}
					else if ( stepType == AUD_FOOTSTEP_JUMP_LAND_L || stepType == AUD_FOOTSTEP_JUMP_LAND_R )
					{
						color = Color_purple;
					}
					grcDebugDraw::Line(heelPos,heelPos + Vec3V(0.5f,0.f,0.f),color,color,25);
					grcDebugDraw::Line(heelPos,heelPos + Vec3V(0.f,0.5f,0.f),color,color,25);
					grcDebugDraw::Line(heelPos,heelPos + Vec3V(0.f,0.f,0.5f),color,color,25);
					fwTimer::StartUserPause();
				}else if (sm_ShowFootStepTuningInfo)
				{
					Color32 color = Color_red;
					if(stepType == AUD_FOOTSTEP_SOFT_L || stepType == AUD_FOOTSTEP_SOFT_R)
					{
						color = Color_green;
					}
					else if ( stepType == AUD_FOOTSTEP_JUMP_LAND_L || stepType == AUD_FOOTSTEP_JUMP_LAND_R )
					{
						color = Color_purple;
					}
					grcDebugDraw::Line(heelPos,heelPos + Vec3V(0.5f,0.f,0.f),color,color,25);
					grcDebugDraw::Line(heelPos,heelPos + Vec3V(0.f,0.5f,0.f),color,color,25);
					grcDebugDraw::Line(heelPos,heelPos + Vec3V(0.f,0.f,0.5f),color,color,25);
				}
#endif
				if(m_HasIkResultsThisFrame)
				{
					ProcessFootstepVfx(stepType,hindFoot,footMatrix,footId,m_GroundNormal[footId]);
				}
				else
				{
					ProcessFootstepVfx(stepType,hindFoot,footMatrix,footId,m_GroundNormal[0]);
				}

				m_FootMoveState[footId] = kMovePlanted;
				m_LastTimeFootPlanted[footId] = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0);
			}
		}
	}
}
//-------------------------------------------------------------------------------------------------------------------
void CPedFootStepHelper::SetSpeedSmootherRates()
{
	u8 index = m_DetectionValuesIndices[audDVStop];
	if(m_pParentPed->GetMotionData()->GetIsWalking())
	{
		index = m_DetectionValuesIndices[audDVWalk];
		if(m_pParentPed->GetPedConfigFlag(CPED_CONFIG_FLAG_StairsDetected) || m_WasOnStairs)
		{
			index = m_DetectionValuesIndices[audDVStairsWalk];
		}
		else if(m_pParentPed->GetMotionData()->GetUsingStealth())
		{
			index = m_DetectionValuesIndices[audDVWalk];
			//index = m_DetectionValuesIndices[audDVStealthWalk];
		}
	}
	else if (m_pParentPed->GetMotionData()->GetIsRunning())
	{
		index = m_DetectionValuesIndices[audDVRun];
		if(m_pParentPed->GetPedConfigFlag(CPED_CONFIG_FLAG_StairsDetected) || m_WasOnStairs)
		{
			index = m_DetectionValuesIndices[audDVStairsRun];
		}
		else if(m_pParentPed->GetMotionData()->GetUsingStealth())
		{
			index = m_DetectionValuesIndices[audDVRun];
			//index = m_DetectionValuesIndices[audDVStairsRun];
			//index = m_DetectionValuesIndices[audDVStealthRun];
		}
	}
	else if (m_pParentPed->GetMotionData()->GetIsSprinting())
	{
		index = m_DetectionValuesIndices[audDVSprint];
		if(m_pParentPed->GetPedConfigFlag(CPED_CONFIG_FLAG_StairsDetected) || m_WasOnStairs)
		{
			index = m_DetectionValuesIndices[audDVStairsRun];
		}
		else if(m_pParentPed->GetMotionData()->GetUsingStealth())
		{
			index = m_DetectionValuesIndices[audDVStealthRun];
		}
	}
	if(naVerifyf( m_pModPhys->numModes >= index,"Wrong footstep detection value index"))
	{
		m_PedVelocitySmoother.SetRates(m_pModPhys->Modes[index].SpeedSmootherIncreaseRate,m_pModPhys->Modes[index].SpeedSmootherDecreaseRate);
	}
}
//-------------------------------------------------------------------------------------------------------------------
void CPedFootStepHelper::UpdateFeetDetection(audFootstepEvent stepType,u8 detectionValuesIndex,Vec3V_In downSlopeDirection)
{
	Vec4V groundPlane;	
	m_IsPlayingSwitchScene = false;
	UpdatePhysicProbes(groundPlane);
	Vec3V heelPos;
	Vec3V boneVel;

	//static const u32 feetTags[MAX_NUM_FEET] = {RearLeft ,RearRight,FrontLeft, FrontRight};
	const u32 numFeet = m_pModPhys->NumFeet; 
	naAssertf(numFeet <= 4, "Creature with more than 4 feet found.  This could lead to audio exceeding the bounds of an array!");
	audFootstepEvent originalStep = stepType;
	for(u32 uFootId = 0; uFootId < numFeet; uFootId++)
	{
		stepType = originalStep;
#if __BANK
		if(!ShouldUpdateFoot(uFootId))
			continue;
#endif
		// Check if hind foot.
		bool hindFoot = false;
		if(m_pModPhys->NumFeet > 2) 
		{
			hindFoot = (uFootId < 2);
		}
		// left or right step.
		stepType = (audFootstepEvent) ((u32)stepType + (uFootId % 2));
		Mat34V footMatrix = Mat34V(V_IDENTITY);
		CalculateFootInfo((FeetTags)uFootId/*FeetTags(feetTags[uFootId])*/, footMatrix, boneVel);
		heelPos = footMatrix.GetCol3Ref();

		ScalarV_ConstRef fFootSpeed = Mag(boneVel);
		eFootHeightState heightState = m_FootHeightState[uFootId];
		if(m_HasIkResultsThisFrame)
		{
			groundPlane = Vec4V(Vec::V4PermuteTwo<Vec::X1,Vec::Y1,Vec::Z1,Vec::X2>( m_GroundNormal[0].GetIntrin128(), 
				(m_GroundNormal[0].GetX() * m_GroundPosition[0].GetX()
				+ m_GroundNormal[0].GetY() * m_GroundPosition[0].GetY() 
				+ m_GroundNormal[0].GetZ() * m_GroundPosition[0].GetZ()).GetIntrin128() ));
		}

		ScalarV distanceToPlane = Subtract(Dot(groundPlane.GetXYZ(),heelPos),groundPlane.GetW());
		const f32 upDistanceToGround = distanceToPlane.Getf();// groundPlane.DistanceToPlane(heelPos);

#if __BANK 
		if(m_IsWalkingOnSlope && sm_ShowSlopeInfo)
		{
			char txt[64]; 
			formatf(txt,"Slope : [Angle %f] ",RtoD * m_SlopeAngle);
			grcDebugDraw::AddDebugOutput(Color_white,txt);
		}
#endif
		if(m_IsPlayingSwitchScene && !m_ScriptForceStepType)
		{
			detectionValuesIndex = m_DetectionValuesIndices[audDVWalk];
		}
		else if(m_WasInTheAir.IsSet(uFootId))
		{
			detectionValuesIndex = m_DetectionValuesIndices[audDVJump];
		}
		else if (m_WasClimbingLadder.IsSet(uFootId))
		{
			detectionValuesIndex = m_DetectionValuesIndices[audDVLadder];
		}
		//	Moving or planted
		eFootMoveState moveState = m_FootMoveState[uFootId];
		switch (moveState)
		{
		case kMovePlanted: 
			{
				UpdateFootPlanted(uFootId BANK_ONLY(,fFootSpeed), upDistanceToGround,detectionValuesIndex BANK_ONLY(,stepType),downSlopeDirection BANK_ONLY(,heelPos));
				break;
			}
		case kMoveMoving:
			{ 
				UpdateFootMoving(uFootId,fFootSpeed, upDistanceToGround,detectionValuesIndex,stepType,downSlopeDirection,hindFoot,/*feetTags[*/uFootId/*]*/,footMatrix BANK_ONLY(,heelPos));
				break;
			} 
		}
		m_FootHeightState[uFootId] = heightState;
		//m_WasMoving = moving;

	} // for(u32 i = 0; i < numBones; i++) 
}
//-------------------------------------------------------------------------------------------------------------------
void CPedFootStepHelper::UpdateFootsteps()
{
	// First of all check if the ped is in the air
	if(m_pParentPed->GetPedConfigFlag( CPED_CONFIG_FLAG_IsInTheAir ))  
	{
		// if the ped is in the air don't bother updating the footsteps
		for(u32 i = 0; i < m_pModPhys->NumFeet ; i++)
		{
			m_WasInTheAir.Set(i);
			m_FootMoveState[i] = kMoveMoving;
			m_AlreadyTriggered.Clear(i);
		}
		m_AlreadyLanded = false;
		m_WasClimbingLadder.Reset();
		m_ClimbingLadder = false;
		m_SlidingDownALadder = false; 
		if(m_pParentPed->GetPedAudioEntity())
		{
			m_pParentPed->GetPedAudioEntity()->GetFootStepAudio().StopLadderSlide();
		}
		return;
	}
	else
	{
		if( !m_AlreadyLanded)
		{
			m_AlreadyLanded = true;
			m_TimeInTheAir =  g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0) - m_LastTimeOnGround;
		}
		m_LastTimeOnGround = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0);
		if(m_pParentPed->GetIsSwimming())
		{
			m_WasInTheAir.Reset();
		}
	}
	if(m_pParentPed->IsLocalPlayer())
	{
		if(m_pParentPed->GetIsArrested() || m_pParentPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_BUSTED))
		{
			m_WasClimbingLadder.Reset();
			m_ClimbingLadder = false;
			m_SlidingDownALadder = false; 
			if(m_pParentPed->GetPedAudioEntity())
			{
				m_pParentPed->GetPedAudioEntity()->GetFootStepAudio().StopLadderSlide();
			}
		}
	}

	const CVehicle* vehiclePedEntering = m_pParentPed->GetVehiclePedEntering();

	if (vehiclePedEntering)
	{
		// url:bugstar:6122431 - Special case, Formula car is super low to the ground and ped feet clip through the bottom when getting in/out, so just suppress footsteps while this is happening
		if (vehiclePedEntering->GetModelNameHash() == ATSTRINGHASH("FORMULA", 0x1446590A) && m_pParentPed->GetPedResetFlag(CPED_RESET_FLAG_IsEnteringOrExitingVehicle))
		{
			bool isPedEnteringOrExitingVehicle = false;

			if (m_pParentPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_ENTER_VEHICLE))
			{
				CTaskEnterVehicle *task = (CTaskEnterVehicle*)m_pParentPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_ENTER_VEHICLE);
				if (task && task->GetState() >= CTaskEnterVehicle::State_EnterSeat)
				{
					isPedEnteringOrExitingVehicle = true;
				}
			}
			else if (m_pParentPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_EXIT_VEHICLE))
			{
				CTaskExitVehicle *task = (CTaskExitVehicle*)m_pParentPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_EXIT_VEHICLE);
				if (task && task->GetState() <= CTaskExitVehicle::State_ExitSeat)
				{
					isPedEnteringOrExitingVehicle = true;
				}
			}

			if (isPedEnteringOrExitingVehicle)
			{
				return;
			}
		}
	}

	if (m_ClimbingLadder)
	{
		m_WasInTheAir.Reset();
		m_TimeToResetLaddersFlag = 0;
		return;
	}
	else
	{
		if(m_TimeToResetLaddersFlag == 0)
		{
			m_TimeToResetLaddersFlag = fwTimer::GetTimeInMilliseconds() + sm_DeltaTimeToResetLadders;
		}
		if(m_TimeToResetLaddersFlag < fwTimer::GetTimeInMilliseconds())
		{
			m_WasClimbingLadder.Reset();
			m_SlidingDownALadder = false;
		}
	}

	//Slopes
	Vec3V downSlopeDirection (V_ONE_WZERO);
	if(m_pParentPed->IsLocalPlayer())
	{
		UpdateSlopeDirection(downSlopeDirection);
	}

	f32 fLocalMoverSpeed = 0.f;
	SetSpeedSmootherRates();
	fLocalMoverSpeed = m_PedVelocitySmoother.CalculateValue(m_pParentPed->GetVelocity().Mag(),audNorthAudioEngine::GetTimeStep());

	audFootstepEvent stepType = AUD_FOOTSTEP_SOFT_L;
	u8 detectionValuesIndex = 0;
	ComputeStepInfo(fLocalMoverSpeed,stepType,detectionValuesIndex);
	if(audNorthAudioEngine::GetMicrophones().IsFirstPerson() && ( stepType == AUD_FOOTSTEP_SOFT_L || stepType == AUD_FOOTSTEP_SOFT_R))
	{
		UpdateSoftStepsFirstPerson();
	}
	else
	{
		UpdateFeetDetection(stepType,detectionValuesIndex,downSlopeDirection);
	}
	m_HasIkResultsThisFrame = false;
	if(m_WasOnStairs && !m_pParentPed->GetPedConfigFlag(CPED_CONFIG_FLAG_StairsDetected))
	{
		m_TimeToResetStairsFlag += fwTimer::GetTimeStepInMilliseconds();
		if(m_TimeToResetStairsFlag >= g_TimeToResetStairsFlagThreshold)
		{
			m_WasOnStairs = false;
		}
	}
	else if(m_pParentPed->GetPedConfigFlag(CPED_CONFIG_FLAG_StairsDetected))
	{
		m_TimeToResetStairsFlag = 0;
		m_WasOnStairs = true;
	}
}
//-------------------------------------------------------------------------------------------------------------------
void CPedFootStepHelper::UpdateSoftStepsFirstPerson()
{
	// We still need to update the feet position to properly place the footstep sounds.
	Mat34V footMatrix = Mat34V(V_IDENTITY);
	Vec3V boneVel = Vec3V(V_ZERO);
	CalculateFootInfo(LeftFoot, footMatrix, boneVel);
	CalculateFootInfo(RightFoot, footMatrix, boneVel);

	const Vec3V north = Vec3V(0.0f, 1.0f, 0.0f);
	const Vec3V front = NormalizeFast(g_AudioEngine.GetEnvironment().GetPanningListenerMatrix().b());
	const f32 cosangle = fabs(Dot(front, north).Getf());
	m_AccCosAngle += fabs(cosangle - m_PrevCosAngle);
	m_PrevCosAngle = cosangle;
	u32 currentTime = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0);
	u32 timeElapsed = currentTime - m_LastTimeSoftStepPlayed;
	if ( m_AccCosAngle >  0.8f && timeElapsed > 500 )
	{
		m_AccCosAngle = 0.f;
		m_LastTimeSoftStepPlayed = currentTime;

		m_pParentPed->GetPedAudioEntity()->GetFootStepAudio().AddFootstepEvent(m_LastEvent);
		if(m_pParentPed->GetPedResetFlag(CPED_RESET_FLAG_InContactWithFoliage) || m_pParentPed->GetPedResetFlag(CPED_RESET_FLAG_InContactWithBIGFoliage))
		{
			m_pParentPed->GetPedAudioEntity()->GetFootStepAudio().AddBushEvent(m_LastEvent, m_pParentPed->GetContactedFoliageHash());
		}
		if(!IsAnAnimal())
		{
			m_pParentPed->GetPedAudioEntity()->GetFootStepAudio().AddPetrolCanEvent(m_LastEvent);
			m_pParentPed->GetPedAudioEntity()->GetFootStepAudio().AddMolotovEvent(m_LastEvent);
		}
		m_pParentPed->GetPedAudioEntity()->GetFootStepAudio().AddWaterEvent(m_LastEvent);
		if(m_LastEvent == AUD_FOOTSTEP_SOFT_L)
		{
			m_LastEvent = AUD_FOOTSTEP_SOFT_R;
		}
		else
		{
			m_LastEvent = AUD_FOOTSTEP_SOFT_L;
		}
	}
}
//-------------------------------------------------------------------------------------------------------------------
void CPedFootStepHelper::UpdateSlopeDirection(Vec3V_InOut downSlopeDirection)
{
	//First of all get the slope angle: 
	f32 mdo = sqrtf( m_pParentPed->GetGroundNormal().x * m_pParentPed->GetGroundNormal().x + m_pParentPed->GetGroundNormal().y * m_pParentPed->GetGroundNormal().y );
	f32 angle = atan2( m_pParentPed->GetGroundNormal().z, mdo );
	m_SlopeAngle = angle;
	if( 90.f -  fabs(RtoD * m_SlopeAngle) < sm_SlopeDAngleThreshold ) 	
	{
		m_IsWalkingOnSlope = false;
	}
	else
	{
		m_IsWalkingOnSlope = true;
	}
	// Negate the angle to get the line downhill
	m_SlopeAngle *= -1.f;
#if __BANK
	if(sm_OverrideSlopeAngle)
	{
		m_SlopeAngle = DtoR * sm_OverriddenSlopeAngle;
	}
#endif	
	// We are walking along a slope, compute a line down the slope plane where to place the sounds.
	//Cross product between up and ground normal to get the direction of rotation.
	Vec3V rotDirection = CrossZAxis(VECTOR3_TO_VEC3V(m_pParentPed->GetGroundNormal()));
	f32 sinAngle = 0.f;
	f32 cosAngle = 0.f;
	rage::cos_and_sin(cosAngle,sinAngle,m_SlopeAngle);
	QuatV rotation(rotDirection * ScalarV(sinAngle), ScalarV(cosAngle));

	downSlopeDirection = Transform(rotation,VECTOR3_TO_VEC3V( m_pParentPed->GetGroundNormal()));
#if __BANK
	if(sm_ShowSlopeInfo)
	{
		grcDebugDraw::Line(m_pParentPed->GetTransform().GetPosition(),m_pParentPed->GetTransform().GetPosition() + VECTOR3_TO_VEC3V(m_pParentPed->GetGroundNormal()),Color_YellowGreen);
		grcDebugDraw::Line(m_pParentPed->GetTransform().GetPosition(),m_pParentPed->GetTransform().GetPosition() + ScalarV(V_TEN) * downSlopeDirection,Color_red);
	}
#endif
}

void CPedFootStepHelper::GetSlopeDirection(Vec3V_InOut downSlopeDirection) const
{
	// We are walking along a slope, compute a line down the slope plane where to place the sounds.
	//Cross product between up and ground normal to get the direction of rotation.
	Vec3V rotDirection = CrossZAxis(VECTOR3_TO_VEC3V(m_pParentPed->GetGroundNormal()));
	f32 sinAngle = 0.f;
	f32 cosAngle = 0.f;
	rage::cos_and_sin(cosAngle,sinAngle,m_SlopeAngle);
	QuatV rotation(rotDirection * ScalarV(sinAngle), ScalarV(cosAngle));

	downSlopeDirection = Transform(rotation,VECTOR3_TO_VEC3V( m_pParentPed->GetGroundNormal()));
}
//-------------------------------------------------------------------------------------------------------------------
void CPedFootStepHelper::UpdateFootstepGroundTest(Vec3V_InOut groundPosition,Vec3V_InOut groundNormal)
{

#if __BANK 
	if(sm_DrawDebugInfo)
	{
		const Matrix34 &rootMatrix = MAT34V_TO_MATRIX34(m_pParentPed->GetTransform().GetMatrix()); 
		Vector3 vecStart = rootMatrix.d;
		Vector3 vecEnd = rootMatrix.d;
		vecStart.z -= sm_FootStepLineTestStartOffset;
		vecEnd.z = vecStart.z - sm_FootStepLineTestLong;
		grcDebugDraw::Line(vecStart,vecEnd,Color_red,Color_red);
	}
#endif
	//Collect all the results we can from the CWorldProbeAsync
	{
		PF_FUNC(GetRes);
		m_CachedLineTest.GetProbeResults();
	}
	//Check if we complete a new test and process it.
	//Otherwise wait another frame until a new one is complete.
	if(m_CachedLineTest.IsComplete())
	{
		// Process it
		if (ProcessAsyncLineTestProbes())
		{
#if __BANK
			if(sm_DrawDebugInfo)
			{
				Vector3 headPos = m_pParentPed->GetBonePositionCached(BONETAG_HEAD);

				grcDebugDraw::Sphere(headPos,0.4f,Color_green,true);
			}
#endif
			const Matrix34 &rootMatrix = MAT34V_TO_MATRIX34(m_pParentPed->GetTransform().GetMatrix()); 
			// We have to store the new result by LRU scheme.
			if(CPedFootStepHelper::sm_NumberOfTestResultStored < MAX_NUMBER_OF_GROUND_FOOTSTEPS_TESTS)
			{
				// No override needed.
				for (s32 i = 0; i < MAX_NUMBER_OF_GROUND_FOOTSTEPS_TESTS; ++i )
				{
					if (CPedFootStepHelper::sm_GroundFootStepTests[i].height == -5000)
					{
						CPedFootStepHelper::sm_GroundFootStepTests[i].height = rootMatrix.d.GetZ();
						CPedFootStepHelper::sm_GroundFootStepTests[i].numberOfHits = 1; 
						CPedFootStepHelper::sm_GroundFootStepTests[i].groundPos = VECTOR3_TO_VEC3V(m_CachedLineTest.m_GroundPosition);
						CPedFootStepHelper::sm_GroundFootStepTests[i].groundNormal = VECTOR3_TO_VEC3V(m_CachedLineTest.m_GroundNormal);
						CPedFootStepHelper::sm_GroundFootStepTests[i].entity = CPhysics::GetEntityFromInst(m_CachedLineTest.m_MainLineTest.m_Instance);
						CPedFootStepHelper::sm_GroundFootStepTests[i].component = m_CachedLineTest.m_MainLineTest.m_Component;
						CPedFootStepHelper::sm_GroundFootStepTests[i].materialId = m_CachedLineTest.m_MainLineTest.m_MaterialId;
						CPedFootStepHelper::sm_NumberOfTestResultStored++;
						break;
					}
				}
			}
			else
			{
				s32 badCandidateIndex = 0; 
				for (s32 i = 0; i < MAX_NUMBER_OF_GROUND_FOOTSTEPS_TESTS; ++i )
				{
					// Look for a bad candidate so we'll override it if needed.
					if(CPedFootStepHelper::sm_GroundFootStepTests[i].lastNumberOfHits < CPedFootStepHelper::sm_GroundFootStepTests[badCandidateIndex].lastNumberOfHits)
					{
						badCandidateIndex = i;
					}

				}
				CPedFootStepHelper::sm_GroundFootStepTests[badCandidateIndex].height = rootMatrix.d.GetZ();
				CPedFootStepHelper::sm_GroundFootStepTests[badCandidateIndex].numberOfHits = 1; 
				CPedFootStepHelper::sm_GroundFootStepTests[badCandidateIndex].lastNumberOfHits = 1; 
				CPedFootStepHelper::sm_GroundFootStepTests[badCandidateIndex].groundPos	  = VECTOR3_TO_VEC3V(m_CachedLineTest.m_GroundPosition);
				CPedFootStepHelper::sm_GroundFootStepTests[badCandidateIndex].groundNormal = VECTOR3_TO_VEC3V(m_CachedLineTest.m_GroundNormal);
				CPedFootStepHelper::sm_GroundFootStepTests[badCandidateIndex].entity = CPhysics::GetEntityFromInst(m_CachedLineTest.m_MainLineTest.m_Instance);
				CPedFootStepHelper::sm_GroundFootStepTests[badCandidateIndex].component = m_CachedLineTest.m_MainLineTest.m_Component;
				CPedFootStepHelper::sm_GroundFootStepTests[badCandidateIndex].materialId = m_CachedLineTest.m_MainLineTest.m_MaterialId;			

			}	
			Assertf((CPedFootStepHelper::sm_NumberOfTestResultStored<=MAX_NUMBER_OF_GROUND_FOOTSTEPS_TESTS),"Bad number of test on updateFootSteps");
			groundPosition = VECTOR3_TO_VEC3V(m_CachedLineTest.m_GroundPosition);
			groundNormal  = VECTOR3_TO_VEC3V(m_CachedLineTest.m_GroundNormal);
			if(m_pParentPed->GetPedAudioEntity())
			{
				m_pParentPed->GetPedAudioEntity()->GetFootStepAudio().SetStandingMaterial( m_CachedLineTest.m_MainLineTest.m_MaterialId
					, CPhysics::GetEntityFromInst(m_CachedLineTest.m_MainLineTest.m_Instance),m_CachedLineTest.m_MainLineTest.m_Component);
			}
			m_HasResultThisFrame = true; 
		}
		if(m_CachedLineTest.Clear())
		{
			PF_FUNC(IssueAsyncLineTProbes);
			IssueAsyncLineTestProbes();
		}
	}
}
//-------------------- -----------------------------------------------------------------------------------------------
void CPedFootStepHelper::UpdateUpDiffWithGround(/*const phBoundComposite *pRagdollBoundComp*/)
{
	if(naVerifyf(m_pParentPed, "NULL ped pointer"))
	{
		/*atHashValue hashedPersonality(m_pParentPed->GetPedModelInfo()->GetPersonalitySettings().GetPersonalityNameHash());
		if(hashedPersonality == g_AnimalPed)
		{
			if(pRagdollBoundComp && (pRagdollBoundComp->GetMaxNumBounds() >= RAGDOLL_BUTTOCKS))
			{
				Matrix34 rootMatrix = RCC_MATRIX34(pRagdollBoundComp->GetCurrentMatrix(RAGDOLL_BUTTOCKS));
				const Matrix34& currWorldMatrix = MAT34V_TO_MATRIX34(m_pParentPed->GetRagdollInst()->GetMatrix());
				rootMatrix.Dot(currWorldMatrix);
				m_UpDiffWihtGround = rootMatrix.c.Dot(m_pParentPed->GetGroundNormal());
				m_UpDiffWihtGround = fabs(m_UpDiffWihtGround);
			}
		}
		else*/
		{
			Matrix34 rootMatrix = M34_IDENTITY;
			m_pParentPed->GetBoneMatrix(rootMatrix,BONETAG_ROOT); 
			m_UpDiffWihtGround = rootMatrix.c.Dot(m_pParentPed->GetGroundNormal());
			m_UpDiffWihtGround = fabs(m_UpDiffWihtGround);
		}
	}
}
//-------------------------------------------------------------------------------------------------------------------
void CPedFootStepHelper::Update()
{
	if (!m_pModPhys)
	{
		InitializeModelPhysics();
	}
	UpdatePrivate();
	m_PrevHeight = m_pParentPed->GetTransform().GetPosition().GetZf();
}
//-------------------------------------------------------------------------------------------------------------------
void CPedFootStepHelper::UpdatePrivate()
{
	if(m_pModPhys && (m_pModPhys->NumFeet != 0) && sm_UseProceduralFootsteps && !m_pParentPed->m_nDEflags.bFrozenByInterior
		&& ((!m_pParentPed->GetIsInVehicle() && !audPedAudioEntity::GetBJVehicle()) || (m_pParentPed->GetIsInVehicle() && m_pParentPed->GetPedResetFlag(CPED_RESET_FLAG_IsEnteringOrExitingVehicle))))
	{
		if(IsAudioHLOD() || m_ScriptForceUpdate || m_VfxWantsToUpdate || audNorthAudioEngine::GetMicrophones().IsVisibleBySniper((CEntity*)m_pParentPed))
		{
			//g_Count++;
			//naDisplayf("Frame %u Count %u Ped %u",fwTimer::GetFrameCount(),g_Count,&m_pParentPed);
			if(!m_pParentPed->GetIsSwimming())
			{
				//Vector3 headPos = m_pParentPed->GetBonePositionCached(BONETAG_HEAD);
				//grcDebugDraw::Sphere(headPos,0.2f,Color32(0.f,255.f,0.f,128.f),false);
				UpdateFootsteps();
				// if the ped has only 2 feet, update the position of the hands, the update footstep call is not doing it.
				if(m_pModPhys->NumFeet == 2 && !IsAnAnimal())
				{
					UpdateHandPositions();
				}
			}
			else if(m_WasInTheAir.IsSet(0) || m_WasInTheAir.IsSet(1))
			{
				m_WasInTheAir.Reset();
				if(m_pParentPed->GetPedAudioEntity())
				{
					m_pParentPed->GetPedAudioEntity()->GetFootStepAudio().ShouldTriggerFallInWaterSplash();
				}
			}
			m_IsAudioHLOD = false;
		}
	}
}
//-------------------------------------------------------------------------------------------------------------------
void CPedFootStepHelper::InitializeModelPhysics()
{
	if(m_pParentPed->GetPedModelInfo())
	{
		m_pModPhys = audNorthAudioEngine::GetObject<ModelPhysicsParams>(m_pParentPed->GetPedModelInfo()->GetHashKey());
	}
	if(m_pModPhys == NULL)
	{
		if (!IsAnAnimal())
		{
			m_pModPhys = audNorthAudioEngine::GetObject<ModelPhysicsParams>(ATSTRINGHASH("DEFAULT_MODEL", 0x0d2bc59f5));
		}
		else
		{
			//m_pModPhys = audNorthAudioEngine::GetObject<ModelPhysicsParams>(ATSTRINGHASH("DEFAULT_ANIMAL_MODEL", 0x9AC3F963));
			naAssertf(false, "Missing animal physic parameters for %s",m_pParentPed->GetPedModelInfo()->GetModelName());
		}
	}
	if(naVerifyf(m_pModPhys,"All peds need a modelPhysicsParams object, please bug the audio team."))
	{
		m_pParentPed->GetPedAudioEntity()->GetFootStepAudio().InitModelSpecifics();
		InitDetectionValuesIndices();
		// Set up the feet offsets for the ped 
		m_FeetOffsets[LeftHand].front = m_pParentPed->GetPedModelInfo()->GetFFrontOffset();
		m_FeetOffsets[LeftHand].up = m_pParentPed->GetPedModelInfo()->GetFUpOffset();
		m_FeetOffsets[RightHand].front = m_pParentPed->GetPedModelInfo()->GetFFrontOffset();
		m_FeetOffsets[RightHand].up = m_pParentPed->GetPedModelInfo()->GetFUpOffset();
		m_FeetOffsets[LeftFoot].front = m_pParentPed->GetPedModelInfo()->GetRFrontOffset();
		m_FeetOffsets[LeftFoot].up = m_pParentPed->GetPedModelInfo()->GetRUpOffset();
		m_FeetOffsets[RightFoot].front = m_pParentPed->GetPedModelInfo()->GetRFrontOffset();
		m_FeetOffsets[RightFoot].up = m_pParentPed->GetPedModelInfo()->GetRUpOffset();
	}
}
//-------------------------------------------------------------------------------------------------------------------
#if __BANK
bool CPedFootStepHelper::ShouldUpdateFoot(const u32 footId)
{
	bool shouldUpdate = false;
	if(sm_OnlyUpdateFLFoot)
	{
		if((m_pModPhys->NumFeet == 2 && footId == 0 ) || (m_pModPhys->NumFeet == 4 && footId == 2))
		{
			shouldUpdate = true;
		}
	}
	else if (sm_OnlyUpdateFRFoot)
	{
		if((m_pModPhys->NumFeet == 2 && footId == 1 ) || (m_pModPhys->NumFeet == 4 && footId == 3))
		{
			shouldUpdate = true;
		}
	}
	else if (sm_OnlyUpdateRLFoot)
	{
		if((m_pModPhys->NumFeet == 4 && footId == 0))
		{
			shouldUpdate = true;
		}
	}
	else if (sm_OnlyUpdateRRFoot)
	{
		if((m_pModPhys->NumFeet == 4 && footId == 1))
		{
			shouldUpdate = true;
		}
	}
	else 
	{
		shouldUpdate = true;
	}
	return shouldUpdate;
}
void TogglePause(void)
{
	fwTimer::EndUserPause();
}
//-------------------------------------------------------------------------------------------------------------------
void CPedFootStepHelper::AddWidgets(bkBank &bank)
{
	bank.PushGroup("Peds footstep helper",false);
	bank.PushGroup("Footstep detection",false);
	bank.AddToggle("Draw HLOD peds", &sm_DrawHLODResults);
	bank.AddSlider("Run fitness scale", &sm_RunFitnessScale, 0.f, 5.f, 0.001f);
	bank.AddSlider("Animal fitness scale", &sm_AnimalFitnessScale, 0.f, 5.f, 0.001f);
	bank.AddSlider("Foot lift", &sm_UpDistaneToTriggerLift, 0.f, 5.f, 0.001f);
	bank.PushGroup("Slopes",false);
		bank.AddToggle("Show slope info", &sm_ShowSlopeInfo);
		bank.AddSlider("sm_SlopeDebrisProb", &sm_SlopeDebrisProb, 0.f, 1.f, 0.001f);
		bank.AddToggle("sm_OverrideSlopeAngle", &sm_OverrideSlopeAngle);
		bank.AddSlider("sm_OverriddenSlopeAngle", &sm_OverriddenSlopeAngle, -180.f, 360.f, 0.001f);
		bank.AddSlider("Slope angle (degrees) threshold", &sm_SlopeDAngleThreshold, 0.f, 360.f, 1.f);
		bank.AddToggle("Override slope variables", &sm_OverrideSlopeVariables);
		bank.AddSlider("sm_OverriddenMovementFactor", &sm_OverriddenMovementFactor, 0.f, 1.f, 0.001f);
		bank.AddSlider("sm_DeltaTimeToResetLadders", &sm_DeltaTimeToResetLadders, 0, 5000, 1);
	bank.PopGroup();
		bank.AddToggle("Only Update FL Foot", &sm_OnlyUpdateFLFoot);
		bank.AddToggle("Only Update FR Foot", &sm_OnlyUpdateFRFoot);
		bank.AddToggle("Only Update RL Foot", &sm_OnlyUpdateRLFoot);
		bank.AddToggle("Only Update RR Foot", &sm_OnlyUpdateRRFoot);	
		bank.AddSeparator();
		bank.AddSlider("g_TimeToResetStairsFlagThreshold", &g_TimeToResetStairsFlagThreshold, 0, 3000, 100);
		
		bank.AddToggle("Show footstep tuning info", &sm_ShowFootStepTuningInfo);
		bank.AddSlider("sm_StairsDeltaSpeed", &sm_StairsDeltaSpeed, 0.f, 10.f, 0.001f);
		
		bank.AddToggle("Pause On state limits", &sm_PauseOnLimits);  
		bank.AddToggle("Pause On Footstep Event", &sm_PauseOnFootstepEvents);
		bank.AddToggle("Pause On Drag Event", &sm_PauseOnDragEvents);
		bank.AddSlider("sm_DeltaAimSpeed", &sm_DeltaAimSpeed, 0.f, 2.f, 0.001f);
		
		bank.AddButton("Toggle Pause", datCallback(CFA(TogglePause)));
		bank.AddSeparator();
		bank.AddToggle("Draw debug info", &sm_DrawDebugInfo);
		bank.AddToggle("show ped velocities	", &sm_ShowPedSpeed);
		bank.AddSlider("Trigger scuff threshold", &sm_TriggerScuffThresh, 0.f, 500.f, 1.f);
		bank.PushGroup("Probes tests");
			bank.AddSlider("g_FootStepLineTestStartOffset", &sm_FootStepLineTestStartOffset, 0.f, 2.f, 0.001f);
			bank.AddSlider("g_FootStepLineTestLong", &sm_FootStepLineTestLong, 0.f, 2.f, 0.01f);
			bank.AddSlider("g_FootStepHeightThreshold", &sm_FootStepHeightThreshold, 0.f, 10.f, 0.1f);
			bank.AddToggle("Force all peds to use sync probes", &sm_ForceSyncProbesToAllPeds);
			bank.AddSlider("Number of Intersections", &sm_NumIntersections, 0, 16, 1);
			bank.AddToggle("Sync from rootBone", &sm_SyncProbesGroundTest);
			bank.AddToggle("ASync from footsteps.+ height algorithm", &sm_AsyncProbesGroundTest);
			bank.AddToggle("Draw results asyncTest", &sm_DrawResults);
		bank.PopGroup();
	bank.PopGroup();
	bank.PushGroup("Footstep position calculation",false);
		bank.AddToggle("Draw feet position", &sm_DrawFeetPosition);
		bank.AddToggle("Draw feet velocity", &sm_DrawFeetVelocity);
		bank.AddSeparator();
		bank.AddToggle("Apply Heels Offset",&sm_ApplyHeelsOffsets);
		bank.AddToggle("Apply High-Heels Offset",&sm_ApplyHighHeelsOffsets);
		bank.AddSlider("Heels up offset",&sm_HeelUpOffset,-1.f,1.f,0.001f);
		bank.AddSlider("Heels front offset",&sm_HeelFrontOffset,-1.f,1.f,0.001f);
		bank.AddSlider("High-Heels front offset",&sm_HighHeelFrontOffset,-1.f,1.f,0.001f);
		bank.AddSlider("High-Heels up offset",&sm_HighHeelUpOffset,-1.f,1.f,0.001f);
		bank.AddToggle("Override Front foot offsets",&sm_OverrideFOffsets);
		bank.AddSlider("Front foot up offset",&sm_FUpOffset,-1.f,1.f,0.001f);
		bank.AddSlider("Front foot front offset",&sm_FFrontOffset,-1.f,1.f,0.001f);
		bank.AddToggle("Override Rear foot offsets",&sm_OverrideROffsets);
		bank.AddSlider("Rear foot up offset",&sm_RUpOffset,-1.f,1.f,0.001f);
		bank.AddSlider("Rear foot front offset",&sm_RFrontOffset,-1.f,1.f,0.001f);
		bank.AddSlider("MaxBoneVelocity INV", &sm_MaxBoneVelocityInv, 0.0001f, 100.f, 0.0001f);
		bank.AddSlider("sm_SprintBoneSpeedScalar", &sm_SprintBoneSpeedScalar, 0.f, 100.f, 0.01f);
		bank.AddSlider("sm_NonPlayerBoneSpeedScalar", &sm_NonPlayerBoneSpeedScalar, 0.f, 100.f, 0.01f);
		bank.AddSlider("sm_MaxBoneSpeedDecrease", &sm_MaxBoneSpeedDecrease, 0.f, 10.f, 0.001f);
	bank.PopGroup();
	bank.PopGroup();
}
#endif
