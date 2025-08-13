//
// task/taskvault.cpp
//
// Copyright (C) 1999-2010 Rockstar Games.  All Rights Reserved.
//

// File header
#include "Task/Movement/Climbing/TaskVault.h"

// Rage headers
#include "fwanimation/animdirector.h"
#include "math/angmath.h"
#include "fragmentnm/nm_channel.h "

// Game headers
#include "Animation/MovePed.h"
#include "Animation/AnimManager.h"
#include "Debug/DebugScene.h"
#include "Event/Event.h"
#include "Event/EventDamage.h"
#include "Peds/Ped.h"
#include "Peds/PedIntelligence.h"
#include "Physics/Physics.h"
#include "Physics/WorldProbe/shapetestresults.h"
#include "System/ControlMgr.h"
#include "Task/Default/TaskPlayer.h"
#include "Task/Physics/TaskNMBalance.h"
#include "Task/Motion/Locomotion/TaskMotionPed.h"
#include "Task/Movement/TaskFall.h"
#include "Task/Movement/Jumping/TaskJump.h"
#include "Camera/CamInterface.h"
#include "Task/Motion/Locomotion/TaskHumanLocomotion.h"
#include "Task/Motion/Locomotion/TaskInWater.h"
#include "Task/Movement/TaskParachute.h"
#include "Task/Vehicle/TaskCarUtils.h"
#include "Vehicles/vehicle.h"
#include "Ik/IkManager.h"
#include "Ik/solvers/LegSolverProxy.h"
#include "Ik/solvers/LegSolver.h"
#include "Peds/PedIkSettings.h"
#include "camera/gameplay/aim/FirstPersonShooterCamera.h"
#include "camera/gameplay/GameplayDirector.h"

AI_OPTIMISATIONS()
AI_MOVEMENT_OPTIMISATIONS()

const fwMvBooleanId CTaskVault::ms_EndFinishedId("End_Finished",0x647830D);
const fwMvBooleanId CTaskVault::ms_EnterEndVaultId("Enter_EndVault",0xCCCFACD8);
const fwMvBooleanId CTaskVault::ms_EnterFallId("Enter_Fall",0x24A3EB1C);
const fwMvBooleanId CTaskVault::ms_SlideStartedId("Slide_Started",0x288E006E);
const fwMvBooleanId CTaskVault::ms_SlideExitStartedId("Slide_Exit_Started",0x333E6E9D);
const fwMvBooleanId CTaskVault::ms_RunInterrupt("RunInterrupt",0xC8BC90AF);
const fwMvBooleanId CTaskVault::ms_WalkInterrupt("WalkInterrupt",0x4DAF9B66);
const fwMvBooleanId CTaskVault::ms_DisableLegIKId("DisableLegIK",0xA9356413);
const fwMvBooleanId CTaskVault::ms_EnableLegIKId("EnableLegIK",0x864F50D5);
const fwMvBooleanId CTaskVault::ms_EnableArmIKId("EnableArmIK",0x8E9971FB);
const fwMvBooleanId CTaskVault::ms_DisableArmIKId("DisableArmIK",0x97E4B8D3);
const fwMvBooleanId CTaskVault::ms_EnablePelvisFixupId("EnablePelvisFixup",0xD900D9C2);
const fwMvBooleanId CTaskVault::ms_EnableHeadingUpdateId("EnabledHeadingUpdate",0xC64E808B);
const fwMvBooleanId CTaskVault::ms_VaultLandWindowId("VaultLandWindow",0x29d9a218);	
const fwMvBooleanId CTaskVault::ms_LandRightFootId("LandRightFoot",0xd4d6e7a9);	
const fwMvClipId CTaskVault::ms_EndClipId("EndClip",0x34F3F79B);
const fwMvClipId CTaskVault::ms_FallClipId("FallClip",0x51FE37D0);
const fwMvClipId CTaskVault::ms_StartClip1Id("StartClip1",0x16344C29);
const fwMvClipId CTaskVault::ms_StartClip2Id("StartClip2",0x28FB71B7);
const fwMvClipId CTaskVault::ms_SlideClipId("SlideClip",0x6C685A3A);
const fwMvFloatId CTaskVault::ms_DepthId("Depth",0xBCA972D6);
const fwMvFloatId CTaskVault::ms_DistanceId("Distance",0xD6FF9582);
const fwMvFloatId CTaskVault::ms_DropHeightId("DropHeight",0xEB5DD1C1);
const fwMvFloatId CTaskVault::ms_ClearanceHeightId("ClearanceHeight",0xD7057D61);
const fwMvFloatId CTaskVault::ms_ClearanceWidthId("ClearanceWidth",0xD816071D);
const fwMvFloatId CTaskVault::ms_EndPhaseId("EndPhase",0xDB3A39C4);
const fwMvFloatId CTaskVault::ms_FallPhaseId("FallPhase",0x5841DBEB);
const fwMvFloatId CTaskVault::ms_SlidePhaseId("SlidePhase",0x7A739A67);
const fwMvFloatId CTaskVault::ms_HeightId("Height",0x52FDF336);
const fwMvFloatId CTaskVault::ms_SpeedId("Speed",0xF997622B);
const fwMvFloatId CTaskVault::ms_StartPhase1Id("StartPhase1",0x290FEF61);
const fwMvFloatId CTaskVault::ms_StartPhase2Id("StartPhase2",0xF604094A);
const fwMvFloatId CTaskVault::ms_ClimbRunningRateId("RunningRate",0x1B670218);
const fwMvFloatId CTaskVault::ms_ClimbStandRateId("StandRate",0x2B90DCC9);
const fwMvFloatId CTaskVault::ms_ClamberRateId("ClamberRate",0x35CB269D);
const fwMvFloatId CTaskVault::ms_ClimbSlideRateId("SlideRate",0xb869f83b);
const fwMvFloatId CTaskVault::ms_StartWeightId("StartWeight",0x5E9478F1);
const fwMvFloatId CTaskVault::ms_SwimmingId("Swimming",0x5DA2A411);
const fwMvFlagId CTaskVault::ms_AngledClimbId("bAngledClimb",0x9EAAEACC);
const fwMvFlagId CTaskVault::ms_LeftFootVaultFlagId("bLeftFootVault",0xFC144BE0);
const fwMvFlagId CTaskVault::ms_JumpVaultFlagId("bJumpVault",0xD225D02F);
const fwMvFlagId CTaskVault::ms_SlideLeftFlagId("bSlideLeft",0x739A0B21);
const fwMvFlagId CTaskVault::ms_EnabledUpperBodyFlagId("bEnabledUpperBody",0xd58474b4);
const fwMvFlagId CTaskVault::ms_EnabledIKBounceFlagId("bEnabledIKBounce",0xeb43433a);
const fwMvClipSetVarId	CTaskVault::ms_UpperBodyClipSetId("UpperBodyClipSet",0x49bb9318);
const fwMvFilterId CTaskVault::ms_UpperBodyFilterId("UpperBodyFilter",0x6f5d10ab);
const fwMvFlagId	CTaskVault::ms_FirstPersonMode("FirstPersonMode",0x8BB6FFFA);
const fwMvFloatId	CTaskVault::ms_PitchId("Pitch",0x3F4BB8CC);
const fwMvClipSetVarId	CTaskVault::ms_FPSUpperBodyClipSetId("FPSUpperBodyClipSet",0x3B2F2839);
const fwMvClipId CTaskVault::ms_WeaponBlockedClipId(ATSTRINGHASH("aim_med_loop", 0x5bcefe69));

////////////////////////////////////////////////////////////////////////////////

// Statics 
bank_bool CTaskVault::ms_bUseAutoVault            = false;
bank_bool CTaskVault::ms_bUseAutoVaultButtonPress = true;
bank_bool CTaskVault::ms_bUseAutoVaultStepUp	  = true;

const float CTaskVault::ms_fStepUpHeight = 0.8f;

IMPLEMENT_MOVEMENT_TASK_TUNABLES(CTaskVault, 0xce76688f)

CTaskVault::Tunables CTaskVault::sm_Tunables;

//! TO DO - Move to Tunables if necessary.

CTaskVault::tVaultExitCondition CTaskVault::ms_VaultExitConditions[eVEC_Max] = 
{
	//! min height						max height						min depth	step-up depth	clamber depth	min drop	max drop	ver clearance	hor clearance	running jump?	deferTest	disableDelay	no auto vault	flag
	{	0.0f,							CTaskVault::ms_fStepUpHeight,	0.0f,		0.325f,			0.35f,			0.325f,		FLT_MAX,	1.2f,			0.5f,			false,			false,		false,			false,			fwMvFlagId("bVaultLow",0x5594A017)			},		//eVEC_StandLow
	{	0.0f,							CTaskVault::ms_fStepUpHeight,	0.0f,		0.575f,			0.575f,			0.325f,		FLT_MAX,	1.5f,			0.75f,			true,			false,		false,			false,			fwMvFlagId("bVaultLow",0x5594A017)			},		//eVEC_RunningLow
	{	CTaskVault::ms_fStepUpHeight,	FLT_MAX,						0.0f,		0.20f,			0.20f,			0.5f,		FLT_MAX,	0.0f,			0.65f,			false,			false,		false,			false,			fwMvFlagId("bVaultDefault",0xB8F1FB8B)		},		//eVEC_Default
	{	CTaskVault::ms_fStepUpHeight,	FLT_MAX,						0.0f,		0.20f,			0.35f,			0.325f,		0.5f,		0.0f,			0.65f,			false,			false,		false,			false,			fwMvFlagId("bVaultShallow",0x85AFF29F)		},		//eVEC_Shallow
	{	CTaskVault::ms_fStepUpHeight,	FLT_MAX,						0.0f,		0.50f,			0.50f,			0.5f,		FLT_MAX,	0.0f,			0.75f,			false,			false,		false,			false,			fwMvFlagId("bVaultWide",0x4FCB8360)			},		//eVEC_Wide
	{	CTaskVault::ms_fStepUpHeight,	FLT_MAX,						0.0f,		0.70f,			0.70f,			0.5f,		FLT_MAX,	0.0f,			0.75f,			false,			false,		false,			false,			fwMvFlagId("bVaultUberWide",0xA39712DC)		},		//eVEC_UberWide
	{	0.6f,							1.2f,							1.0f,		2.0f,			2.0f,			0.6f,		2.5f,		0.0f,			1.0f,			false,			false,		true,			true,			fwMvFlagId("bVaultSlide",0xC697AB6)			},		//eVEC_Slide
	{	0.0f,							FLT_MAX,						0.0f,		1.2f,			0.6f,			-1.0f,		FLT_MAX,	0.0f,			1.5f,			false,			true,		false,			false,			fwMvFlagId("bClamberToParachute",0xAA06655B)},		//eVEC_Parachute
	{	0.0f,							FLT_MAX,						0.0f,		1.2f,			0.6f,			-1.0f,		FLT_MAX,	0.0f,			1.5f,			false,			true,		false,			false,			fwMvFlagId("bClamberToDive",0xA8EC5F23)		},		//eVEC_Dive
};

CTaskVault::tClimbStandCondition CTaskVault::ms_ClimbStandConditions[eCEC_Max] =
{
	//! min height	min stand depth	min clamber depth		fMaxDrop		fHorizontalClearance	fHorizontalClearanceFirstPerson	flagId
	{ 1.8f,			0.325f,			0.325f,					0.325f,			0.4f,					0.6f,							fwMvFlagId("bCanStand",0x9085E598) },
};
		
bool CTaskVault::tClimbStandCondition::IsSupported(const CClimbHandHoldDetected &handHold, const CPed *pPed)
{
	//! Adjust allowable stand depth for small platforms. 
	float fDepthToTest = handHold.GetHeight() < ms_fStepUpHeight ? fMinStandDepth : fMinClamberDepth;

	float fHorClearanceToTest;
#if FPS_MODE_SUPPORTED
	if(pPed && pPed->IsFirstPersonShooterModeEnabledForPlayer(false, true))
	{
		bool bIsClimbingPBus2 = MI_CAR_PBUS2.IsValid() && handHold.GetPhysical() && handHold.GetPhysical()->GetModelIndex() == MI_CAR_PBUS2;
		fHorClearanceToTest = bIsClimbingPBus2 ? fHorizontalClearance : fHorizontalClearanceFirstPerson;
	}
	else
#endif
	{
		fHorClearanceToTest = fHorizontalClearance;
	}

	if ((handHold.GetVerticalClearance() >= fVerticalClearance) &&
		((handHold.GetDepth() >= fDepthToTest) || ( (handHold.GetHorizontalClearance() > fHorClearanceToTest) && handHold.GetDrop() <= fMaxDrop)))
	{
		return true;
	}
	else
	{
		return false;
	}
}

bool CTaskVault::tVaultExitCondition::IsSupported(const CClimbHandHoldDetected &handHold, const CPed *UNUSED_PARAM(pPed), bool bJumpVault, bool bAutoVault, bool bDoDeferredTest)
{
	//! Adjust allowable stand depth for small platforms. 
	float fDepthToTest = handHold.GetHeight() < ms_fStepUpHeight ? fMaxStandDepth : fMaxClamberDepth;

	//! Ignore deferred test if set.
	if(bDeferTest && !bDoDeferredTest)
	{
		return false;
	}

	//! If this is a jump vault, ignore all vaults that don't support this.
	if(bJumpVaultSupported && !bJumpVault)
	{
		return false;
	}

	if(bDisableAutoVault && bAutoVault)
	{
		return false;
	}

	if( (handHold.GetHeight() > fMinHeight) &&
		(handHold.GetHeight() <= fMaxHeight) &&
		(handHold.GetDepth() >= fMinDepth) && 
		(handHold.GetDepth() <= fDepthToTest) && 
		(handHold.GetDrop() > fMinDrop) && 
		(handHold.GetDrop() < fMaxDrop) && 
		(handHold.GetVerticalClearance() >= fVerticalClearance) &&
		(handHold.GetHorizontalClearance() >= (handHold.GetDepth() + fHorizontalClearance) ))
	{
		return true;
	}

	return false;
}

bool CTaskVault::IsHandholdSupported(const CClimbHandHoldDetected &handHold, const CPed *pPed, bool bJumpVault, bool bAutoVault, bool bDoDeferredTests, bool *pDisableAutoVaultDelay)
{
	bool bValid = false;
	for(int i = 0; i < eCEC_Max; ++i)
	{
		if(ms_ClimbStandConditions[i].IsSupported(handHold, pPed))
		{
			bValid = true;
			break;
		}
	}
	
	for(int i = 0; i < eVEC_Max; ++i)
	{
		if(ms_VaultExitConditions[i].IsSupported(handHold, pPed, bJumpVault, bAutoVault, bDoDeferredTests))
		{
			bValid = true;
			break;
		}
	}

	//! Work out if we need to disable auto vault. This necessitates another loop through list. :(
	if(bValid && pDisableAutoVaultDelay && !(*pDisableAutoVaultDelay))
	{
		for(int i = 0; i < eVEC_Max; ++i)
		{
			if(ms_VaultExitConditions[i].bDisableDelay && 
				ms_VaultExitConditions[i].IsSupported(handHold, pPed, bJumpVault, bAutoVault, bDoDeferredTests))
			{
				*pDisableAutoVaultDelay = true;
			}
		}
	}

	return bValid;
}

bool CTaskVault::IsSlideSupported(const CClimbHandHoldDetected &handHold, const CPed *pPed, bool bJumpVault, bool bAutoVault)
{
	return ms_VaultExitConditions[eVEC_Slide].IsSupported(handHold, pPed, bJumpVault, bAutoVault);
}

////////////////////////////////////////////////////////////////////////////////

CTaskVault::CTaskVault(const CClimbHandHoldDetected& handHold, fwFlags8 iFlags)
: m_vInitialPedPos(Vector3::ZeroType)
, m_vTargetDist(Vector3::ZeroType)
, m_vTargetDistToDropPoint(VEC3_ZERO)
, m_vLastAnimatedVelocity(Vector3::ZeroType)
, m_vCloneStartPosition(Vector3::ZeroType)
, m_vCloneHandHoldPoint(Vector3::ZeroType)
, m_vCloneHandHoldNormal(Vector3::ZeroType)
, m_vTotalAnimatedDisplacement(Vector3::ZeroType)
, m_fCloneClimbAngle(0.0f)
, m_uClonePhysicalComponent(0)
, m_uCloneMinNumDepthTests(0)
, m_pClonePhysical(NULL)
, m_pHandHold(rage_new CClimbHandHoldDetected(handHold))
, m_TargetAttachOrientation(Quaternion::sm_I)
, m_fInitialMBR(0.0f)
, m_fInitialDistance(0.0f)
, m_fClimbRate(1.0f)
, m_fClimbTargetRate(1.0f)
, m_fClimbScaleRate(1.0f)
, m_fRateMultiplier(1.0f)
, m_fClipLastPhase(0.0f)
, m_vClipAdjustStartPhase(0.0f, 0.0f, 0.0f)
, m_vClipAdjustEndPhase(1.0f, 1.0f, 1.0f)
, m_fClipAdjustStartPhase(0.0f)
, m_fClipAdjustEndPhase(1.0f)
, m_fStartScalePhase(0.0f)
, m_fTurnOnCollisionPhase(0.0f)
, m_fDetachPhase(0.0f)
, m_fShowWeaponPhase(0.0f)
, m_eVaultSpeed(VaultSpeed_Stand)
, m_ProcessSignalsNextState(-1)
, m_bNetworkBlendedIn(false)
, m_bSwimmingVault(false)
, m_bLeftFootVault(false)
, m_iFlags(iFlags)
, m_bLegIKDisabled(false)
, m_bArmIKEnabled(false)
, m_bPelvisFixup(true)
, m_bCloneTaskNoLongerRunningOnOwner(false)
, m_bDisableVault(false)
, m_nClimbExitConditions(0)
, m_nVaultExitConditions(0)
, m_bHandholdSlopesLeft(false)
, m_bSlideVault(false)
, m_iCloneUsesVaultExitCondition(0)
, m_bCloneClimbStandCondition(false)
, m_bCloneUseTestDirection(false)
, m_bCloneUsedSlide(false)
, m_bDeferredTestCompleted(false)
, m_bCanUseFPSHeadBound(false)
, m_bHasFPSUpperBodyAnim(false)
, m_pUpperBodyFilter(NULL)
, m_UpperBodyClipSetId(CLIP_SET_ID_INVALID)
, m_UpperBodyFilterId(FILTER_ID_INVALID)
{
	SetInternalTaskType(CTaskTypes::TASK_VAULT);
}

////////////////////////////////////////////////////////////////////////////////

CTaskVault::~CTaskVault()
{
	delete m_pHandHold;
}

#if !__FINAL

////////////////////////////////////////////////////////////////////////////////

void CTaskVault::Debug() const
{
	if(m_moveNetworkHelper.IsNetworkActive())
	{
		const crClip* pClip = GetActiveClip();
		if(pClip)
		{
			taskDebugf1("---Clip Playing: %s: %.2f", pClip->GetName(), GetActivePhase());
		}
	}

#if DEBUG_DRAW	// TODO: Clean up all the DEBUG_DRAW_ONLY calls in this block.
	const CPed *pPed = GetPed();

	// Render the target point
	DEBUG_DRAW_ONLY(grcDebugDraw::Sphere(m_vInitialPedPos + m_vTargetDist, 0.1f, Color_red));
	BANK_ONLY(m_pHandHold->Render(Color_yellow));

	// Render hand positions for IK.
	static dev_float DEBUG_RADIUS = 0.025f;
	static dev_float DEBUG_RADIUS_SMALL = 0.020f;
	s32 pointHelperIdxL = pPed->GetBoneIndexFromBoneTag(BONETAG_L_PH_HAND);
	Mat34V pointHelperMtxL;
	pPed->GetSkeleton()->GetGlobalMtx(pointHelperIdxL, pointHelperMtxL);
	DEBUG_DRAW_ONLY(grcDebugDraw::Sphere(VEC3V_TO_VECTOR3(pointHelperMtxL.GetCol3()), DEBUG_RADIUS, Color_blue));
	DEBUG_DRAW_ONLY(grcDebugDraw::Line(VEC3V_TO_VECTOR3(pointHelperMtxL.GetCol3()), VEC3V_TO_VECTOR3(pointHelperMtxL.GetCol3()) + (ZAXIS * 0.1f), Color_blue, Color_blue));

	s32 pointHelperIdxR = pPed->GetBoneIndexFromBoneTag(BONETAG_R_PH_HAND);
	Mat34V pointHelperMtxR;
	pPed->GetSkeleton()->GetGlobalMtx(pointHelperIdxR, pointHelperMtxR);
	DEBUG_DRAW_ONLY(grcDebugDraw::Sphere(VEC3V_TO_VECTOR3(pointHelperMtxR.GetCol3()), DEBUG_RADIUS, Color_blue));
	DEBUG_DRAW_ONLY(grcDebugDraw::Line(VEC3V_TO_VECTOR3(pointHelperMtxR.GetCol3()), VEC3V_TO_VECTOR3(pointHelperMtxR.GetCol3()) + (ZAXIS * 0.1f), Color_blue, Color_blue));
	
	Vector3 vRHandBonePosition(VEC3_ZERO);
	pPed->GetBonePosition(vRHandBonePosition, BONETAG_R_HAND);
	DEBUG_DRAW_ONLY(grcDebugDraw::Sphere(vRHandBonePosition, DEBUG_RADIUS, Color_red));
	DEBUG_DRAW_ONLY(grcDebugDraw::Line(vRHandBonePosition, vRHandBonePosition + (ZAXIS * 0.1f), Color_red, Color_red));

	Vector3 vLHandBonePosition(VEC3_ZERO);
	pPed->GetBonePosition(vLHandBonePosition, BONETAG_L_HAND);
	DEBUG_DRAW_ONLY(grcDebugDraw::Sphere(vLHandBonePosition, DEBUG_RADIUS, Color_red));
	DEBUG_DRAW_ONLY(grcDebugDraw::Line(vLHandBonePosition, vLHandBonePosition + (ZAXIS * 0.1f), Color_red, Color_red));

	Vector3 vIKHandLeft = GetHandIKPosWS(HandIK_Left);
	DEBUG_DRAW_ONLY(grcDebugDraw::Sphere(vIKHandLeft, DEBUG_RADIUS_SMALL, Color_yellow));
	DEBUG_DRAW_ONLY(grcDebugDraw::Line(vIKHandLeft, vIKHandLeft + (ZAXIS * 0.1f), Color_yellow, Color_yellow));

	Vector3 vIKHandRight = GetHandIKPosWS(HandIK_Right);
	DEBUG_DRAW_ONLY(grcDebugDraw::Sphere(vIKHandRight, DEBUG_RADIUS_SMALL, Color_yellow));
	DEBUG_DRAW_ONLY(grcDebugDraw::Line(vIKHandRight, vIKHandRight + (ZAXIS * 0.1f), Color_yellow, Color_yellow));
#endif

	CTask::Debug();
}

////////////////////////////////////////////////////////////////////////////////

const char * CTaskVault::GetStaticStateName( s32 iState )
{
	static const char* stateNames[] = 
	{
		"Init",
		"Climb",
		"ClamberVault",
		"Falling",
		"ClamberStand",
		"Ragdoll",
		"Slide",
		"ClamberJump",
		"Quit",
	};

	return stateNames[iState];
}

////////////////////////////////////////////////////////////////////////////////

#endif // !__FINAL

Vector3 CTaskVault::GetHandholdPoint() const
{
	Vector3 vHandHold = m_pHandHold->GetHandholdPosition();
	return vHandHold;
}

Vector3 CTaskVault::GetHandholdNormal() const
{
	Vector3 vHandHoldNormal = m_pHandHold->GetHandholdNormal();
	return vHandHoldNormal;
}

bool CTaskVault::ProcessPhysics(float fTimeStep, int UNUSED_PARAM(nTimeSlice))
{
	// Cache the ped
	CPed* pPed = GetPed();

	// Scale the desired velocity to reach the target
	bool bScaledVelocity = false;
	switch(GetState())
	{
	case State_Climb:
	case State_Slide:
		bScaledVelocity = ScaleDesiredVelocity(fwTimer::GetTimeStep());
		break;
	case State_ClamberStand:
		break;
	default:
		break;
	}

	// Handle if we are attached
	if(GetIsAttachedToPhysical())
	{
		fwAttachmentEntityExtension *extension = pPed->GetAttachmentExtension();
		taskAssert(extension);

		// Get the animated velocity of the ped

		Vector3 vAttachOffset;
		if(bScaledVelocity)
		{
			vAttachOffset = pPed->GetDesiredVelocity();
			vAttachOffset = VEC3V_TO_VECTOR3(pPed->GetTransform().UnTransform3x3(VECTOR3_TO_VEC3V(vAttachOffset)));
		}
		else
		{
			vAttachOffset = pPed->GetAnimatedVelocity();
		}

		Vec3V vPositionalDelta = VECTOR3_TO_VEC3V(vAttachOffset * fTimeStep);

		Vec3V vPosAttachOffset = VECTOR3_TO_VEC3V(extension->GetAttachOffset());
		QuatV qRotAttachOffset = QUATERNION_TO_QUATV(extension->GetAttachQuat());

		vPosAttachOffset += Transform(qRotAttachOffset, vPositionalDelta);

		// Set the attach offset
		extension->SetAttachOffset(RCC_VECTOR3(vPosAttachOffset));

		TUNE_GROUP_FLOAT(VAULTING,fTargetAttachSlerpSpeed,5.0f, 0.0f, 10.0f, 0.01f);

		Matrix34 other;
		m_pHandHold->GetPhysicalMatrix(other);

		// Current Rotation offset
		Matrix34 m = MAT34V_TO_MATRIX34(pPed->GetMatrix());
		m.Dot3x3Transpose(other);
		Quaternion q;
		m.ToQuaternion(q);

		float t = Min(fTargetAttachSlerpSpeed*fwTimer::GetTimeStep(), 1.0f);
		q.SlerpNear(t, m_TargetAttachOrientation);

		q.Normalize();

		// Set the attach quat
		extension->SetAttachQuat(q);
	}

	return true;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskVault::ProcessPostMovement()
{
	m_fClipLastPhase = GetActivePhase();
	m_vLastAnimatedVelocity = GetPed()->GetAnimatedVelocity();
	return true;
}

////////////////////////////////////////////////////////////////////////////////

CTaskVault::FSM_Return CTaskVault::ProcessPreFSM()
{
	// Cache the ped
	CPed* pPed = GetPed();

	pPed->GetPedIntelligence()->SetLastClimbTime(fwTimer::GetTimeInMilliseconds());

	// Reset flags
	pPed->SetPedResetFlag( CPED_RESET_FLAG_IsVaulting, true );
	pPed->SetPedResetFlag( CPED_RESET_FLAG_ForceExplosionCollisions, true );
	pPed->SetPedResetFlag( CPED_RESET_FLAG_DisableDynamicCapsuleRadius, true );
	pPed->SetPedResetFlag( CPED_RESET_FLAG_ProcessPostMovement, true );

	// To make sure we have a better blend when vaulting in first person or aiming
	// and also to get rid of B*2093531, force strafing if the motion state is aiming
	if( pPed->GetPedIntelligence()->FindTaskActiveMotionByType(CTaskTypes::TASK_MOTION_AIMING) != NULL )
	{
		pPed->SetIsStrafing(true);
	}

	// State specific
	switch(GetState())
	{
	case State_Init:
    case State_Climb:

		if(m_moveNetworkHelper.IsNetworkAttached() && (GetTimeInState() >= NORMAL_BLEND_DURATION))
		{
			pPed->ForceMotionStateThisFrame(CPedMotionStates::MotionState_AnimatedVelocity);

			//! Reset sprint values. If we have stick held, we force motion anyway. This just makes sure we don't immediately
			//! sprint from a stop whilst the sprint values dissipate.
			if(pPed->GetPlayerInfo())
			{
				pPed->GetPlayerInfo()->SetPlayerDataSprintControlCounter(0.0f);
			}
		}

		// Disable time-slicing while climbing. The ped scales velocity to hit an exact point and if we time-slice
		// we might not hit it (possibly forcing us into collision).
		pPed->GetPedAiLod().SetForceNoTimesliceIntelligenceUpdate(true);

    // Intentional fall through

	case State_Slide:
    case State_ClamberVault:
	case State_ClamberStand:

		pPed->SetPedResetFlag( CPED_RESET_FLAG_ProcessPhysicsTasks, true );
		pPed->SetPedResetFlag( CPED_RESET_FLAG_ApplyVelocityDirectly, true );

		pPed->SetPedResetFlag( CPED_RESET_FLAG_IsClimbing, true );

		//! If we are doing a non step up, block secondary anims.
		if(!IsStepUp())
		{
			pPed->SetPedResetFlag( CPED_RESET_FLAG_BlockSecondaryAnim, true );
		}

#if FPS_MODE_SUPPORTED
		if (pPed->IsLocalPlayer())
		{
			pPed->SetPlayerResetFlag(CPlayerResetFlags::PRF_DISABLE_CAMERA_VIEW_MODE_CYCLE);
		}
#endif // FPS_MODE_SUPPORTED

	case State_Ragdoll:

		// Leg Ik off.
		if(m_bLegIKDisabled)
		{
			pPed->GetIkManager().SetFlag(PEDIK_LEGS_AND_PELVIS_OFF);
		}
		else
		{
			TUNE_GROUP_FLOAT(VAULTING,fVaultLandLegIKBlend,0.25f, 0.0f, 5.0f, 0.01f)
			TUNE_GROUP_FLOAT(VAULTING,fSlideLandLegIKBlend,0.1f, 0.0f, 5.0f, 0.01f)
			float fLegIKBlendTime = (GetState() == State_ClamberVault && m_bSlideVault) ? fSlideLandLegIKBlend : fVaultLandLegIKBlend;
			pPed->GetIkManager().SetOverrideLegIKBlendTime(fLegIKBlendTime);
		
			if(!m_bPelvisFixup)
			{
				pPed->SetPedResetFlag(CPED_RESET_FLAG_RequiresLegIk, true);
			}
		}

		pPed->SetPedResetFlag(CPED_RESET_FLAG_DisableMotionBaseVelocityOverride, true);
		
		break;
	case State_Falling:
		break;
	default:
		break;
	}

	if(GetState() == State_ClamberStand || GetState() == State_Slide || (GetState() == State_ClamberVault && !m_bSlideVault))
	{
		pPed->ForceMotionStateThisFrame(CPedMotionStates::MotionState_AnimatedVelocity);
	}

	pPed->SetPedResetFlag( CPED_RESET_FLAG_DisableGaitReduction, true );

	// Move blender
	pPed->SetPedResetFlag( CPED_RESET_FLAG_DontChangeMbrInSimpleMoveDoNothing, true );

	// Update the heading
	if(GetState() == State_Climb)
	{
		const float fHeadingDelta = SubtractAngleShorter(pPed->GetDesiredHeading(), pPed->GetCurrentHeading());
		pPed->GetMotionData()->SetExtraHeadingChangeThisFrame(Sign(fHeadingDelta) * Min(Abs(fHeadingDelta), pPed->GetHeadingChangeRate() * fwTimer::GetTimeStep()));
	
		pPed->SetPedResetFlag( CPED_RESET_FLAG_DisableProcessProbes, true );
	}

	UpdateFPSHeadBound();

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskVault::ProcessMoveSignals()
{
	CPed *pPed = GetPed();

	if(m_moveNetworkHelper.IsNetworkActive())
	{
		if(m_moveNetworkHelper.GetBoolean(ms_EnableLegIKId) || (GetState() == State_Climb && GetActivePhase() > 0.9f))
		{
			if(m_bLegIKDisabled)
			{
				m_bLegIKDisabled = false;
			}
		}

		switch(GetState())
		{
		case(State_Init):
			{
				if(GetActiveClip())
				{
					m_ProcessSignalsNextState = State_Climb;
				}
			}
			// Fall through. If ped is being aggressively time-sliced, we may need to miss climb state entirely.
		case(State_Climb):
			{
				if(m_moveNetworkHelper.GetBoolean(ms_DisableLegIKId))
				{
					m_bLegIKDisabled = true;
					
					//! Reset stand data. No longer necessary.
					pPed->ResetStandData();
				}

				bool bDoJump = false;
				if(!pPed->IsNetworkClone() && IsStepUp() && (IsVaultExitFlagSet(eVEC_Parachute) || IsVaultExitFlagSet(eVEC_Dive)) && !m_bDisableVault)
				{
					bDoJump = true;
				}

				if(m_moveNetworkHelper.GetBoolean(ms_EnterEndVaultId))
				{
					m_ProcessSignalsNextState = bDoJump ? State_ClamberJump : State_ClamberStand;
				}
				else if(m_moveNetworkHelper.GetBoolean(ms_EnterFallId) )
				{
					m_ProcessSignalsNextState = bDoJump ? State_ClamberJump : State_ClamberVault;
				}
				else if(m_moveNetworkHelper.GetBoolean(ms_SlideStartedId))
				{
					m_ProcessSignalsNextState = State_Slide;

					// Hide weapon on slides.
					CObject* pWeapon = pPed->GetWeaponManager() ? pPed->GetWeaponManager()->GetEquippedWeaponObject() : NULL;
					if (pWeapon)
					{
						pWeapon->SetIsVisibleForModule(SETISVISIBLE_MODULE_GAMEPLAY, false, true);
					}
				}
			}
			break;
		case(State_Slide):
			{
				if(m_moveNetworkHelper.GetBoolean(ms_SlideExitStartedId))
				{
					//! Note: Go to clamber vault (i.e. to fall), as we want to preserve falling motion
					//! until we are ready to enable land or fall.
					m_ProcessSignalsNextState = State_ClamberVault;
				}
			}
			break;
		case(State_ClamberVault):
		case(State_ClamberStand):
			{
				if(m_moveNetworkHelper.GetBoolean(ms_DisableLegIKId))
				{
					m_bLegIKDisabled = true;
				}

				// Should we turn back on pelvis fix for leg IK?
				if(m_moveNetworkHelper.GetBoolean(ms_EnablePelvisFixupId))
				{
					m_bPelvisFixup = true;
				}

				if(GetState() == State_ClamberStand)
				{
					if(m_moveNetworkHelper.GetBoolean(ms_EndFinishedId))
					{
						m_ProcessSignalsNextState = State_Quit;
					}
				}
			}
			break;
		default:
			break;
		}

		ProcessFPMoveSignals();

		return true;
	}

	return true;
}

////////////////////////////////////////////////////////////////////////////////

CTaskVault::FSM_Return CTaskVault::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin
		FSM_State(State_Init)
			FSM_OnEnter
				return StateInit_OnEnter();
			FSM_OnUpdate
				return StateInit_OnUpdate();
		FSM_State(State_Climb)
			FSM_OnEnter
				return StateClimb_OnEnter();
			FSM_OnUpdate
				return StateClimb_OnUpdate();
		FSM_State(State_ClamberVault)
			FSM_OnEnter
				return StateClamberVault_OnEnter();
			FSM_OnUpdate
				return StateClamberVault_OnUpdate();
		FSM_State(State_Falling)
			FSM_OnEnter
				return StateFalling_OnEnter();
			FSM_OnUpdate
				return StateFalling_OnUpdate();
		FSM_State(State_ClamberStand)
			FSM_OnEnter
				return StateClamberStand_OnEnter();
			FSM_OnUpdate
				return StateClamberStand_OnUpdate();
		FSM_State(State_Ragdoll)
			FSM_OnEnter
				return StateRagdoll_OnEnter();
			FSM_OnUpdate
				return StateRagdoll_OnUpdate();
		FSM_State(State_Slide)
			FSM_OnEnter
				return StateSlide_OnEnter();
			FSM_OnUpdate
				return StateSlide_OnUpdate();
		FSM_State(State_ClamberJump)
			FSM_OnEnter
				return StateClamberJump_OnEnter();
			FSM_OnUpdate
				return StateClamberJump_OnUpdate();
		FSM_State(State_Quit)
			FSM_OnUpdate
				return FSM_Quit;
	FSM_End
}

////////////////////////////////////////////////////////////////////////////////

CTaskVault::FSM_Return CTaskVault::ProcessPostFSM()
{
	CPed *pPed = GetPed();
	if(!m_iFlags.IsFlagSet(VF_RunningJumpVault))
	{
		// Head towards the target rate
		static dev_float RATE_CHANGE = 2.0f;
		float fRateChange = RATE_CHANGE * fwTimer::GetTimeStep();
		m_fClimbRate += Clamp(m_fClimbTargetRate - m_fClimbRate, -fRateChange, fRateChange);
	}

	if(GetState() == State_ClamberStand && 
		m_moveNetworkHelper.GetBoolean(ms_EnableHeadingUpdateId) )
	{
		TUNE_GROUP_FLOAT(VAULTING, fClanberStandHeadingModifier, 3.0f, 0.0f, 10.0f, 0.1f);
		CTaskFall::UpdateHeading(pPed, pPed->GetControlFromPlayer(), fClanberStandHeadingModifier);
	}

	// Move signals
	UpdateMoVE();

	ProcessHandIK(GetPed());

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskVault::CleanUp()
{
	// Cache the ped
	CPed* pPed = GetPed();

	// Enable collision
	pPed->EnableCollision();

	// Enable gravity
	pPed->SetUseExtractedZ(false);

	// Force Detach if we are attached
	pPed->DetachFromParent(DETACH_FLAG_ACTIVATE_PHYSICS);

	ResetRagdollOnCollision(pPed);

	// No longer climbing
	// Restore the previous heading change rate
	pPed->RestoreHeadingChangeRate();

	static dev_float s_BlendTime = pPed->GetMotionData()->GetUsingStealth() ? 0.3f : SLOW_BLEND_DURATION;

	bool bTagSync = pPed->GetMotionData()->GetForcedMotionStateThisFrame() != CPedMotionStates::MotionState_Idle && !GetPed()->GetPedConfigFlag(CPED_CONFIG_FLAG_ForceFPSIKWithUpperBodyAnim);

	// Blend out the move network
	pPed->GetMovePed().ClearTaskNetwork(m_moveNetworkHelper, s_BlendTime, bTagSync ? CMovePed::Task_TagSyncTransition : 0);

	m_SwimClipSetRequestHelper.Release();
	m_bSwimmingVault = false;

	m_UpperBodyClipSetRequestHelper.Release();

	pPed->GetMotionData()->SetGaitReductionBlockTime(); //block gait reduction while we ramp back up

	// Show weapon.
	CObject* pWeapon = pPed->GetWeaponManager() ? pPed->GetWeaponManager()->GetEquippedWeaponObject() : NULL;
	if (pWeapon)
	{
		pWeapon->SetIsVisibleForModule(SETISVISIBLE_MODULE_GAMEPLAY, true, true);
	}

	pPed->GetIkManager().ResetOverrideLegIKBlendTime();

	// Clear any old filter
	if(m_pUpperBodyFilter)
	{
		m_pUpperBodyFilter->Release();
		m_pUpperBodyFilter = NULL;
	}

	//! Inform targeting that we can re-pick a soft lock target (even if we have been holding aim button).
	if (pPed->IsLocalPlayer())
	{
		pPed->GetPlayerInfo()->GetTargeting().ClearSoftAim();
	}

#if FPS_MODE_SUPPORTED
	if(GetPed()->GetPedConfigFlag(CPED_CONFIG_FLAG_ForceFPSIKWithUpperBodyAnim))
	{
		GetPed()->SetPedResetFlag(CPED_RESET_FLAG_ForceSkipFPSAimIntro, true);
		GetPed()->SetPedResetFlag(CPED_RESET_FLAG_AimWeaponReactionRunning, true);
		GetPed()->SetPedConfigFlag(CPED_CONFIG_FLAG_ForceFPSIKWithUpperBodyAnim, false);
	}

	if(GetPed()->IsFirstPersonShooterModeEnabledForPlayer(false))
	{
		GetPed()->SetPedResetFlag(CPED_RESET_FLAG_SkipFPSUnHolsterTransition, true);
	}
#endif // FPS_MODE_SUPPORTED

	// Base class
	CTask::CleanUp();
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskVault::ShouldAbort(const AbortPriority priority, const aiEvent* pEvent)
{
	// Cache the ped
	CPed* pPed = GetPed();

	if(priority == ABORT_PRIORITY_IMMEDIATE || pPed->IsFatallyInjured())
	{
		return CTask::ShouldAbort(priority, pEvent);
	}
	else if(pEvent)
	{
		const CEvent* pGameEvent = static_cast<const CEvent*>(pEvent);

		if(pGameEvent->RequiresAbortForRagdoll() ||
			pGameEvent->GetEventPriority() >= E_PRIORITY_SCRIPT_COMMAND_SP ||
			pGameEvent->GetEventType() == EVENT_ON_FIRE)
		{
			return CTask::ShouldAbort(priority, pEvent);
		}
		else if(pGameEvent->GetEventType() == EVENT_DAMAGE)
		{
			const CEventDamage* pDamageTask = static_cast<const CEventDamage*>(pGameEvent);
			if(pDamageTask->HasKilledPed() || pDamageTask->HasInjuredPed())
			{
				return CTask::ShouldAbort(priority, pEvent);
			}
		}
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

#if __BANK
void CTaskVault::AddWidgets(bkBank& bank)
{
	bank.AddToggle("Use Auto Vault",          &ms_bUseAutoVault);
	bank.AddToggle("Use Auto Vault On Press", &ms_bUseAutoVaultButtonPress);
	bank.AddToggle("Use Auto Vault Step ups", &ms_bUseAutoVaultStepUp);
}

void CTaskVault::DumpToLog()
{
	taskDisplayf("--------------------------------------------------------");
	taskDisplayf("INVALID HANDHOLD DETECTED!!!");
	
	m_pHandHold->DumpToLog();

	if(m_moveNetworkHelper.IsNetworkActive())
	{
		const crClip* pClip = GetActiveClip();
		if(pClip)
		{
			taskDisplayf("Active Clip = %s", pClip->GetName());
		}

		for(int i = 0; i < State_Quit; ++i)
		{
			const crClip* pClip = GetActiveClip(0, i);
			taskDisplayf("Non Active Clip = %s State = %s", pClip ? pClip->GetName() : "None", GetStaticStateName(i));
		}
	}

	const Vector3 vPedPosition = GetPedPositionIncludingAttachment();

	taskDisplayf("m_ProcessSignalsNextState = %d", m_ProcessSignalsNextState);
	taskDisplayf("Ped Name = %s, Clone = %s", GetPed()->GetDebugName(), GetPed()->IsNetworkClone() ? "true" : "false");
	taskDisplayf("m_fInitialMBR = %f", m_fInitialMBR);
	taskDisplayf("m_fInitialDistance = %f", m_fInitialDistance);
	taskDisplayf("Attached = %s", m_pHandHold->GetPhysical() ? "true" : "false");
	taskDisplayf("m_bDisableVault = %s", m_bDisableVault ? "true" : "false");
	taskDisplayf("m_vInitialPedPos = %f:%f:%f", m_vInitialPedPos.x, m_vInitialPedPos.y, m_vInitialPedPos.z);
	taskDisplayf("m_vClonePostion = %f:%f:%f", m_vCloneStartPosition.x, m_vCloneStartPosition.y, m_vCloneStartPosition.z);
	taskDisplayf("Position = %f:%f:%f", vPedPosition.x, vPedPosition.y, vPedPosition.z);
	taskDisplayf("m_bSwimmingVault = %s Ped Is Swimming: %s", m_bSwimmingVault ? "true" : "false", GetPed()->GetIsSwimming() ? "true" : "false");
	taskDisplayf("m_bLeftFootVault = %s", m_bLeftFootVault ? "true" : "false");
	taskDisplayf("m_nClimbExitConditions[eCEC_Default] = %s", IsClimbExitFlagSet(eCEC_Default) ? "true" : "false");
	taskDisplayf("m_nVaultExitConditions[eVEC_StandLow] = %s", IsVaultExitFlagSet(eVEC_StandLow) ? "true" : "false");
	taskDisplayf("m_nVaultExitConditions[eVEC_RunningLow] = %s", IsVaultExitFlagSet(eVEC_RunningLow) ? "true" : "false");
	taskDisplayf("m_nVaultExitConditions[eVEC_Default] = %s", IsVaultExitFlagSet(eVEC_Default) ? "true" : "false");
	taskDisplayf("m_nVaultExitConditions[eVEC_Shallow] = %s", IsVaultExitFlagSet(eVEC_Shallow) ? "true" : "false");
	taskDisplayf("m_nVaultExitConditions[eVEC_Wide] = %s", IsVaultExitFlagSet(eVEC_Wide) ? "true" : "false");
	taskDisplayf("m_nVaultExitConditions[eVEC_UberWide] = %s", IsVaultExitFlagSet(eVEC_UberWide) ? "true" : "false");
	taskDisplayf("m_nVaultExitConditions[eVEC_Slide] = %s", IsVaultExitFlagSet(eVEC_Slide) ? "true" : "false");
	taskDisplayf("m_nVaultExitConditions[eVEC_Parachute] = %s", IsVaultExitFlagSet(eVEC_Parachute) ? "true" : "false");
	taskDisplayf("m_nVaultExitConditions[eVEC_Dive] = %s", IsVaultExitFlagSet(eVEC_Dive) ? "true" : "false");

	taskDisplayf("--------------------------------------------------------");
}

#endif // __BANK

////////////////////////////////////////////////////////////////////////////////

CTaskVault::FSM_Return CTaskVault::StateInit_OnEnter()
{
	// Cache the ped
	CPed* pPed = GetPed();

	// Request move signals.
	RequestProcessMoveSignalCalls();

	if(pPed->IsNetworkClone())
	{
        if(!m_iFlags.IsFlagSet(VF_CloneUseLocalHandhold))
        {
			Matrix34 mOverride;

			Vector3 pedPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
			float  pedHeading = fwAngle::LimitRadianAngleSafe(pPed->GetCurrentHeading());
			float fCloneHeading = rage::Atan2f(-m_vCloneStartDirection.x, m_vCloneStartDirection.y);
			fCloneHeading = fwAngle::LimitRadianAngle(fCloneHeading);

			bool bVehicleOffset = false;

			Vector3 handHoldPosition = m_vCloneHandHoldPoint;

			if(m_pClonePhysical)
			{
				CPhysical *pPhysicalEntity = m_pClonePhysical;

				if(pPhysicalEntity->GetIsTypeVehicle())
				{
					bVehicleOffset = true;

					Matrix34 mMatrix;
					CClimbHandHoldDetected::GetPhysicalMatrix(mMatrix, m_pClonePhysical, m_uClonePhysicalComponent);

					//Check if m_uClonePhysicalComponent gave us a valid matrix here and if not use main body matrix
					if(!mMatrix.IsOrthonormal(0.05f)) 
					{ 
						mMatrix.Set(MAT34V_TO_MATRIX34(m_pClonePhysical->GetMatrix())); 
					}

					mMatrix.Transform3x3(m_vCloneHandHoldNormal);
					mMatrix.Transform3x3(m_vCloneStartDirection);
					mMatrix.Transform(m_vCloneStartPosition);
					mMatrix.Transform(m_vCloneHandHoldPoint);

					handHoldPosition = m_vCloneHandHoldPoint;
					Vector3 vPedPos = m_vCloneStartPosition;
					
					float fOwnerPedHeading = rage::Atan2f(-m_vCloneStartDirection.x, m_vCloneStartDirection.y);
					fOwnerPedHeading = fwAngle::LimitRadianAngle(fOwnerPedHeading);
					
					mOverride.MakeRotateZ(fOwnerPedHeading);
					mOverride.d = vPedPos;				

					if (!pPed->GetUsingRagdoll())
					{
						TUNE_GROUP_FLOAT(VAULTING, fClonePedVehicleVaultPositionTolerance, 0.75f, 0.0f, 5.0f, 0.1f);
						TUNE_GROUP_FLOAT(VAULTING, fClonePedVehicleVaultOrientationTolerance, PI*0.33f, 0.0f, PI, 0.1f);

						float	fHeadingDiff = SubtractAngleShorter(fOwnerPedHeading, pedHeading);

						if(!vPedPos.IsClose(pedPosition, fClonePedVehicleVaultPositionTolerance) ||
							fabs(fHeadingDiff) > fClonePedVehicleVaultOrientationTolerance)
						{
							pPed->SetPosition(vPedPos);
							pPed->SetHeading(fOwnerPedHeading);
						}
					}
				}
			}

			if(!bVehicleOffset)
			{
				TUNE_GROUP_FLOAT(VAULTING, fClonePedVaultPositionTolerance, 2.0f, 0.0f, 5.0f, 0.1f);
				TUNE_GROUP_FLOAT(VAULTING, fClonePedVaultOrientationTolerance, PI*0.33f, 0.0f, PI, 0.1f);

				float	fHeadingDiff = SubtractAngleShorter(fCloneHeading, pedHeading);

				if(!m_vCloneStartPosition.IsClose(pedPosition, fClonePedVaultPositionTolerance) ||
					fabs(fHeadingDiff) > fClonePedVaultOrientationTolerance)
				{
					NetworkInterface::GoStraightToTargetPosition(pPed);
					mOverride = MAT34V_TO_MATRIX34(pPed->GetMatrix());
				}
				else
				{
					mOverride.MakeRotateZ(fCloneHeading);
					mOverride.d = m_vCloneStartPosition;
				}
			}

			pPed->GetPedIntelligence()->GetClimbDetector().ResetFlags();
			pPed->SetPedResetFlag(CPED_RESET_FLAG_SearchingForClimb, true);

			sPedTestParams pedTestParams;
			CTaskPlayerOnFoot::GetClimbDetectorParam(	CTaskPlayerOnFoot::STATE_INVALID,
				pPed, 
				pedTestParams, 
				true, 
				false, 
				false, 
				false);

			taskAssertf(!m_vCloneHandHoldNormal.IsClose(VEC3_ZERO,0.1f),"%s Expect non-zero m_vCloneHandHoldNormal %.2f, %.2f, %.2f",pPed->GetDebugName(),m_vCloneHandHoldNormal.x,m_vCloneHandHoldNormal.y,m_vCloneHandHoldNormal.z );
			taskAssertf(!m_vCloneHandHoldPoint.IsClose(VEC3_ZERO,0.1f),"Expect non-zero m_vCloneHandHoldPoint");

			pedTestParams.bCloneUseTestDirection = m_bCloneUseTestDirection;
			pedTestParams.fOverrideMaxClimbHeight = 100.0f;
			pedTestParams.fOverrideMinClimbHeight = 0.0f;
			pedTestParams.vCloneHandHoldNormal = m_vCloneHandHoldNormal;
			pedTestParams.vCloneHandHoldPoint = handHoldPosition;
			pedTestParams.uCloneMinNumDepthTests = m_uCloneMinNumDepthTests;

			CClimbHandHoldDetected handHoldDetected;
			//Clones don't normal maintain the ped scanner so force a scan now before handhold detect so it can exclude any players
			//that might be in the same position. B*1984753
			pPed->GetPedIntelligence()->GetPedScanner()->ScanForEntitiesInRange(*static_cast<CEntity*>(pPed),true);
			pPed->GetPedIntelligence()->GetClimbDetector().GetCloneVaultHandHold(mOverride, &pedTestParams);
			pPed->GetPedIntelligence()->GetPedScanner()->Clear();

			if(pPed->GetPedIntelligence()->GetClimbDetector().GetDetectedHandHold(handHoldDetected))
			{
				delete m_pHandHold;
				m_pHandHold = rage_new CClimbHandHoldDetected(handHoldDetected);
				m_pHandHold->SetClimbAngle(m_fCloneClimbAngle);
			}
			else
			{
				return FSM_Quit;
			}
		}
	}
	else if ( pPed->GetNetworkObject())
	{
		CNetObjPed* pPedObj = SafeCast(CNetObjPed, pPed->GetNetworkObject());
		
		//Since motion state is updated via "InFrequent" make sure that the remote gets latest as the task state changes
		pPedObj->ForceResendOfAllTaskData();
		pPedObj->ForceResendAllData();

		m_vCloneStartPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());

		m_vCloneStartDirection = VEC3V_TO_VECTOR3(pPed->GetTransform().GetForward());
		m_bCloneUseTestDirection = m_pHandHold->GetCloneUseTestheading();
		m_vCloneHandHoldNormal		= m_pHandHold->GetHandHold().GetNormal();
		m_vCloneHandHoldPoint		= m_pHandHold->GetCloneIntersection();
		m_fCloneClimbAngle			= m_pHandHold->GetClimbAngle();
		m_uClonePhysicalComponent   = m_pHandHold->GetPhysicalComponent();
		m_uCloneMinNumDepthTests	= m_pHandHold->GetMinNumDepthTests();

		AdjustSyncedValuesForVehicleOffset();
	}
	

	// Figure out which foot we need to vault from. Note: Not all climb anims use this. Only some (i.e. running low climbs/vaults) have
	// the required variation.
	CTaskMotionBase* pTask = pPed->GetCurrentMotionTask();
	if (pTask && pTask->GetTaskType() == CTaskTypes::TASK_HUMAN_LOCOMOTION && static_cast<CTaskHumanLocomotion*>(pTask)->UseLeftFootTransition())
	{
		// The last foot down was right, so we need to launch from the left foot
		m_bLeftFootVault = true;
	}
	else
	{
		// The last foot down was left, so we need to launch from the right foot
		m_bLeftFootVault = false;
	}

#if __BANK
	if(!IsHandholdSupported(*m_pHandHold, pPed, m_iFlags.IsFlagSet(VF_RunningJumpVault), m_iFlags.IsFlagSet(VF_AutoVault)))
	{
		DumpToLog();
		taskAssertf(0, "Calling TaskVault usisng Handhold params that are not supported!");
	}
#endif

	if(GetPed()->IsNetworkClone())
	{	//set flags synced from remote
		if(m_bCloneClimbStandCondition)
		{
			SetClimbExitFlagSet(0);
		}
		m_nVaultExitConditions.SetAllFlags(m_iCloneUsesVaultExitCondition);
	}
	else
	{
		//! Do Stand tests.
		for(int i = 0; i < eCEC_Max; ++i)
		{
			if(ms_ClimbStandConditions[i].IsSupported(*m_pHandHold, pPed))
			{
				SetClimbExitFlagSet(i);
			}
		}

		//! Do Vault tests.
		for(int i = 0; i < eVEC_Max; ++i)
		{
			if(ms_VaultExitConditions[i].IsSupported(*m_pHandHold, pPed, m_iFlags.IsFlagSet(VF_RunningJumpVault), m_iFlags.IsFlagSet(VF_AutoVault)))
			{
				SetVaultExitFlagSet(i);
			}
		}
		m_bCloneClimbStandCondition = IsClimbExitFlagSet(0);
		m_iCloneUsesVaultExitCondition = m_nVaultExitConditions.GetAllFlags();
	}

	// Store the initial speed
	m_fInitialMBR = pPed->GetMotionData()->GetCurrentMbrY();

	// network clones use the initial distance returned from the owner of the ped as this affects
	// which animations are used by the task
	if(!pPed->IsNetworkClone())
	{
		// Store the initial distance
		m_fInitialDistance = (GetHandholdPoint() - VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition())).XYMag();
	}

	ProcessDisableVaulting();

	//! Does the ground normal slide left or right? Note: If a vehicle, we want to slide in the facing away from the centre.
	{
		Vector3 vSlopeNormal;
		if(m_pHandHold->GetPhysical() && m_pHandHold->GetPhysical()->GetIsTypeVehicle())
		{
			Vector3 pedPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
			Vector3 vehPosition = VEC3V_TO_VECTOR3(m_pHandHold->GetPhysical()->GetTransform().GetPosition());

			pedPosition.z = 0.0f;
			vehPosition.z = 0.0f;

			vSlopeNormal = pedPosition - vehPosition;
		}		
		else
		{
			vSlopeNormal = m_pHandHold->GetGroundNormalAVG();
			vSlopeNormal.z = 0.0f;
		}

		if(vSlopeNormal.Mag2() > 0.0f)
		{
			vSlopeNormal.NormalizeFast();
			float fSlopeHeading = fwAngle::LimitRadianAngle(rage::Atan2f(vSlopeNormal.x, -vSlopeNormal.y));
			const float fHeadingDelta = SubtractAngleShorter(pPed->GetCurrentHeading(), fSlopeHeading);
			m_bHandholdSlopesLeft = fHeadingDelta > 0.0f;
		}

		bool bUseSlide = IsVaultExitFlagSet(CTaskVault::eVEC_Slide) &&  !m_bDisableVault &&  !pPed->GetIsSwimming() && IsSlideDirectionClear();

		if(GetPed()->IsNetworkClone())
		{
			bUseSlide = m_bCloneUsedSlide;
		}
		else
		{
			m_bCloneUsedSlide = bUseSlide;
		}
		
		//! Clear slide flag if we aren't sliding. 
		if(bUseSlide)
		{
			m_bSlideVault = true;
			m_iFlags.ClearFlag(VF_RunningJumpVault);
		}
		else
		{
			ClearVaultExitFlagSet(CTaskVault::eVEC_Slide);
		}
	}

	if(m_nClimbExitConditions.GetAllFlags() || m_nVaultExitConditions.GetAllFlags())
	{
		// Disable gravity
		pPed->SetUseExtractedZ(true);

		// Hide weapon.
		CObject* pWeapon = pPed->GetWeaponManager() ? pPed->GetWeaponManager()->GetEquippedWeaponObject() : NULL;
		if (pWeapon && !IsStepUp())
		{
			pWeapon->SetIsVisibleForModule(SETISVISIBLE_MODULE_GAMEPLAY, false, true);

			// B*2093531
			// The transition from holding a weapon to climbing doesn't look bad, the 
			// problem is that we delete the weapon on the first frame. Forcing an 
			// animation update will help smooth the transition since we won't see the
			// hands in the "holding gun" position for that first 
			pPed->SetPedResetFlag(CPED_RESET_FLAG_ForcePostCameraAnimUpdate, true);
		}

		// Try to stop the ped moving
		if(!m_iFlags.IsFlagSet(VF_RunningJumpVault))
		{
			pPed->SetVelocity(pPed->GetReferenceFrameVelocity());
			pPed->StopAllMotion();
			pPed->SetDesiredVelocity(pPed->GetReferenceFrameVelocity());
		}

		return FSM_Continue;
	}

	return FSM_Quit;
}

////////////////////////////////////////////////////////////////////////////////

CTaskVault::FSM_Return CTaskVault::StateInit_OnUpdate()
{
	// Cache the ped
	CPed* pPed = GetPed();

    if(!m_iFlags.IsFlagSet(VF_CloneUseLocalHandhold))
    {
		// Bail if we didn't get a valid handhold.
		CClimbHandHoldDetected handHoldDetected;
		if(pPed->IsNetworkClone() && !pPed->GetPedIntelligence()->GetClimbDetector().GetDetectedHandHold(handHoldDetected))
		{
			SetState(State_Quit);
			return FSM_Continue;
		}
    }

	// Bail if we are trying to climb on a fading out entity
	if (pPed->GetNetworkObject() && m_pHandHold->GetPhysical())
	{
		// in MP, break off if the entity being climbed on is fading out. This is to prevent the ped going invisible once the fade completes
		CNetObjPhysical* pClimbingEntityObj = m_pHandHold->GetPhysical()->GetNetworkObject() ? static_cast<CNetObjPhysical*>(m_pHandHold->GetPhysical()->GetNetworkObject()) : NULL;

		if (pClimbingEntityObj && pClimbingEntityObj->IsFadingOut())
		{
			SetState(State_Quit);
			return FSM_Continue;
		}
	}

	//! Make sure swimming clip set is streamed in, before kicking off climb (as we reference this in network).
	bool bClipsReady = false;
	if(pPed->GetIsSwimming())
	{
		CTaskMotionBase* pMotionTask = GetPed()->GetPrimaryMotionTask();
		if (pMotionTask)
		{
			static fwMvClipSetId s_defaultSwimmingSet("move_ped_Swimming",0xB634ECC7);	
			if(m_SwimClipSetRequestHelper.Request(s_defaultSwimmingSet))
			{
				bClipsReady = true;
				m_bSwimmingVault = true;
				m_bPelvisFixup = false;
			}
		}
	}
	else
	{
		bClipsReady = true;
	}

	if(!m_moveNetworkHelper.IsNetworkActive() && bClipsReady)
	{
		m_moveNetworkHelper.RequestNetworkDef(CClipNetworkMoveInfo::ms_NetworkTaskVault);

		if(m_moveNetworkHelper.IsNetworkDefStreamedIn(CClipNetworkMoveInfo::ms_NetworkTaskVault))
		{
			m_moveNetworkHelper.CreateNetworkPlayer(pPed, CClipNetworkMoveInfo::ms_NetworkTaskVault);
			static dev_float s_fBlendInTime = NORMAL_BLEND_DURATION;
			static dev_bool s_bUseTagSync = false;
			pPed->GetMovePed().SetTaskNetwork(m_moveNetworkHelper.GetNetworkPlayer(), s_fBlendInTime, s_bUseTagSync ? CMovePed::Task_TagSyncTransition : 0);
			
			SetUpperBodyClipSet(); 
		}
	}

	if(m_moveNetworkHelper.IsNetworkActive() && m_ProcessSignalsNextState != -1)
	{
		SetState(m_ProcessSignalsNextState);
		m_ProcessSignalsNextState = -1;
	}

#if __BANK
	if(bClipsReady && (GetTimeInState() > 10.0f) )
	{
		DumpToLog();
		taskAssertf(0, "Ped is stuck in TaskVault::Init!");
		SetState(State_Quit);
	}
#endif

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTaskVault::FSM_Return CTaskVault::StateClimb_OnEnter()
{
	// Get the ped
	CPed* pPed = GetPed();

	taskAssertf(m_pHandHold, "CTaskVault::StateClimb_OnEnter - invalid handhold!");
	taskAssertf(pPed, "CTaskVault::StateClimb_OnEnter - invalid ped!");

	// Work out the heading
	float fNewHeading = 0.0f;

	// Notify the ped we are off ground for a bit.
	pPed->SetIsStanding(false);
	pPed->SetPedResetFlag( CPED_RESET_FLAG_DisableProcessProbes, true );

	// Set the desired heading
	if(m_iFlags.IsFlagSet(VF_DontOrientToHandhold))
	{
		const Vector3 vDirection = m_pHandHold->GetAlignDirection();
		fNewHeading = rage::Atan2f(vDirection.x, -vDirection.y);
		fNewHeading = fwAngle::LimitRadianAngle(fNewHeading);
	}
	else
	{
		const Vector3& vNormal = GetHandholdNormal();
		fNewHeading = rage::Atan2f(vNormal.x, -vNormal.y);
		fNewHeading = fwAngle::LimitRadianAngle(fNewHeading);
	}

	pPed->SetDesiredHeading(fNewHeading);

	// If we are climbing on a physical object, attach ourselves to it
	if(m_pHandHold->GetPhysical())
	{
		bool bIgnoreVehicleForAttachment = true;
		if(m_pHandHold->GetPhysical() && m_pHandHold->GetPhysical()->GetIsTypeVehicle())
		{
			const CVehicle *pVehicle = static_cast<const CVehicle*>(m_pHandHold->GetPhysical());
			const CCarDoor* pDoor = CCarEnterExit::GetCollidedDoorFromCollisionComponent(pVehicle, m_pHandHold->GetPhysicalComponent());
			if(pDoor)
			{
				bIgnoreVehicleForAttachment = false;
			}
		}

		bool bCanAttach = true;

		if (pPed->GetNetworkObject())
		{
			// in MP, break off if the entity being climbed on is fading out. This is to prevent the ped going invisible once the fade completes
			CNetObjPhysical* pClimbingEntityObj = m_pHandHold->GetPhysical()->GetNetworkObject() ? static_cast<CNetObjPhysical*>(m_pHandHold->GetPhysical()->GetNetworkObject()) : NULL;

			if (pClimbingEntityObj && pClimbingEntityObj->IsFadingOut())
			{
				bCanAttach = false;
			}
		}

		if (bCanAttach)
		{
			Vector3 vAttachOffset(VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()));

			// Position offset
			s16 nAttachBone = m_pHandHold->GetBoneFromHitComponent();

			Matrix34 boneMat;
			m_pHandHold->GetPhysicalMatrix(boneMat);
			boneMat.UnTransform(vAttachOffset);

			// Target Rotation offset
			Matrix34 m_Target;
			m_Target.MakeRotateZ(fNewHeading);
			m_Target.Dot3x3Transpose(boneMat);
			m_Target.ToQuaternion(m_TargetAttachOrientation);

			// Current Rotation offset
			Matrix34 mPed = MAT34V_TO_MATRIX34(pPed->GetMatrix());
			mPed.Dot3x3Transpose(boneMat);
			Quaternion q;
			mPed.ToQuaternion(q);

			mthAssertf(q.Mag2() >= square(0.999f) && q.Mag2() <= square(1.001f),
				"Invalid quaternion <%f, %f, %f, %f> retrieved from climb system. Name: %s Bone: %d",
				q.x,q.y,q.z,q.w,
				m_pHandHold->GetPhysical()->GetDebugName(),
				nAttachBone);

			// Attach
			u32 flags = ATTACH_STATE_BASIC | 
				ATTACH_FLAG_INITIAL_WARP | 
				ATTACH_FLAG_DONT_NETWORK_SYNC | 
				ATTACH_FLAG_COL_ON | 
				ATTACH_FLAG_AUTODETACH_ON_DEATH | 
				ATTACH_FLAG_AUTODETACH_ON_RAGDOLL;
			pPed->AttachToPhysicalBasic(m_pHandHold->GetPhysical(), nAttachBone, flags, &vAttachOffset, &q);

			if(!pPed->IsNetworkClone())
			{
				SetUpRagdollOnCollision(pPed, bIgnoreVehicleForAttachment ? m_pHandHold->GetPhysical() : NULL, true);
			}
		}
	}
	else
	{
		// Control the rate
		static dev_float HEADING_RATE = 5.0f;
		pPed->SetHeadingChangeRate(HEADING_RATE);
	}

	// Store the initial ped pos
	const Vector3 vPedPosition = GetPedPositionIncludingAttachment();
	m_vInitialPedPos = vPedPosition;

	Vector3 vHandholdPoint = GetHandholdPoint();

	// Set the new desired move distance based on where the ped currently is and where it originally was
	m_vTargetDist   = vHandholdPoint - vPedPosition;
	m_vTargetDist.z += pPed->GetCapsuleInfo()->GetGroundToRootOffset();

	Vector3 vDropPoint = m_pHandHold->GetGroundPositionAtDrop();

	m_vTargetDistToDropPoint = vDropPoint - vPedPosition;
	m_vTargetDistToDropPoint.z += pPed->GetCapsuleInfo()->GetGroundToRootOffset();

	const crClip* pClip = GetActiveClip();

#if __BANK
	if(!pClip)
	{
		DumpToLog();
	}
#endif

	// Reset the m_fClipLastPhase
	if(!CClipEventTags::FindEventPhase(GetActiveClip(), CClipEventTags::Start, m_fClipLastPhase))
	{
		m_fClipLastPhase = 0.0f;
	}

	m_fStartScalePhase = m_fClipLastPhase;

	CalcTotalAnimatedDisplacement();

	if(taskVerifyf(pClip, "pClip is NULL"))
	{
		const crTag* pTag = CClipEventTags::FindFirstEventTag(pClip, CClipEventTags::AdjustAnimVel);
		if(pTag)
		{
			SetDesiredVelocityAdjustmentRange(pClip, pTag->GetStart(), pTag->GetEnd());
		}
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTaskVault::FSM_Return CTaskVault::StateClimb_OnUpdate()
{
	// Get the ped
	CPed* pPed = GetPed();

	// Get the current phase
	float fPhase = GetActivePhase();

	//! Turn off vault if we we prefer to stand?
	ProcessDisableVaulting();

	bool bScalingAtEndPoint = fPhase > m_fClipAdjustEndPhase;
	bool bDoBlendReScale = !m_bNetworkBlendedIn && !m_iFlags.IsFlagSet(VF_RunningJumpVault);

	const Vector3 vPedPosition = GetPedPositionIncludingAttachment();

	// While the network is blending in, we may get some velocity from the animations that are blending out
	// so we compensate by adjusting the velocity scale variables until we are fully blended in
	if( (bDoBlendReScale || bScalingAtEndPoint) && !m_bSlideVault)
	{
		// If m_fClipAdjustStartPhase is less than the current phase, we need to move it along
		Vector3 vHandholdPoint = GetHandholdPoint();

		// Set the new desired move distance based on where the ped currently is and where it originally was
		m_vTargetDist   = vHandholdPoint - vPedPosition;
		m_vTargetDist.z += pPed->GetCapsuleInfo()->GetGroundToRootOffset();

		Vector3 vDropPoint = m_pHandHold->GetGroundPositionAtDrop();

		m_vTargetDistToDropPoint = vDropPoint - vPedPosition;
		m_vTargetDistToDropPoint.z += pPed->GetCapsuleInfo()->GetGroundToRootOffset();

		m_fStartScalePhase = m_fClipLastPhase;

		CalcTotalAnimatedDisplacement();

		if(fPhase > m_fClipLastPhase)
		{
			const crClip* pClip = GetActiveClip();
			if(taskVerifyf(pClip, "pClip is NULL"))
			{
				//! Note: If we hit the end of initial scaling phase - just scale throughout rest of clip.
				float fEndPhase = bScalingAtEndPoint ? 1.0f : Min(fPhase + (m_fClipAdjustEndPhase-m_fClipAdjustStartPhase), 1.0f);
				SetDesiredVelocityAdjustmentRange(pClip, fPhase, fEndPhase);
			}
		}
	}

	if(m_moveNetworkHelper.IsNetworkAttached() && (GetTimeInState() >= NORMAL_BLEND_DURATION))
	{
		// Only disable collision when the move network is fully blended in, so we don't penetrate walls due to extra animated velocity
		// Don't disable when attached.
		if(!m_pHandHold->GetPhysical())
		{
			pPed->DisableCollision();
		}

		// Network blended in
		m_bNetworkBlendedIn = true;
	}

	if(fPhase >= m_fClipAdjustStartPhase && fPhase <= m_fClipAdjustEndPhase)
	{
		// Target rate is the scale rate
		m_fClimbTargetRate = m_fClimbScaleRate;
	}
	else
	{
		// Default the rate to 1.0f
		m_fClimbTargetRate = 1.0f;
	}

	// Disable pelvis fixup when it has blended out.
	if(m_bPelvisFixup)
	{
		if (pPed->GetIKSettings().IsIKFlagDisabled(CBaseIkManager::eIkSolverType(IKManagerSolverTypes::ikSolverTypeLeg)))
		{
			m_bPelvisFixup = false;
		}
		else
		{
			CLegIkSolverProxy* pProxy = static_cast<CLegIkSolverProxy*>(pPed->GetIkManager().GetSolver(IKManagerSolverTypes::ikSolverTypeLeg));
			CLegIkSolver* pLegSolver = pProxy ? static_cast<CLegIkSolver*>(pProxy->GetSolver()) : NULL;

			if (!pLegSolver || (pLegSolver && pLegSolver->GetPelvisBlend() <= 0.0f) )
			{
				m_bPelvisFixup = false;
			}
		}
	}

	//! In auto-vault, test for speeding up the playback of the animations. This is useful for the stand climbs, which
	//! can be quite slow at times.
	static dev_bool s_bDoAutoVaultSpeedAdjustment = false;
	if(s_bDoAutoVaultSpeedAdjustment)
	{
		if(m_iFlags.IsFlagSet(VF_AutoVault))
		{
			CClipEventTags::Key AdjustRate("AdjustRate",0x1EEAABBF);
			CClipEventTags::Key AdjustRateAttrib("Rate",0x7E68C088);

			const crTag* pTag = CClipEventTags::FindFirstEventTag(GetActiveClip(), AdjustRate);
			if(pTag)
			{
				if(fPhase >= pTag->GetStart() && fPhase <= pTag->GetEnd())
				{
					const crPropertyAttributeFloat* pAttrib = 
						static_cast<const crPropertyAttributeFloat*>(pTag->GetProperty().GetAttribute(AdjustRateAttrib.GetHash()));
					if (pAttrib)
					{
						m_fClimbTargetRate = pAttrib->GetFloat(); 
					}
				}
			}
		}
	}

	//! Note: There is no event for this, so just do the test near the end. At the end is too late, as the Move network has already begun the
	//! clamber vault animation.
	static dev_float s_DeferredTestPhase = 0.8f;
	if( (GetActivePhase() > s_DeferredTestPhase) && !m_bDeferredTestCompleted)
	{
		Vector3 vHandholdPoint = GetHandholdPoint();
		Vector3 vPlayerForward = VEC3V_TO_VECTOR3(pPed->GetTransform().GetB());

		// Test for parachute/dive clamber poses. Note: These tests are deferred as we don't want to incur the cost at the same time as the vault tests, so we do them
		// before we climb.
		if(ms_VaultExitConditions[eVEC_Parachute].IsSupported(*m_pHandHold, pPed, m_iFlags.IsFlagSet(VF_RunningJumpVault), m_iFlags.IsFlagSet(VF_AutoVault), true))
		{
			static dev_float s_fDistToTest1 = 1.4f;
			static dev_float s_fDistToTest2 = 2.0f;
			static dev_float s_fTestAngle1 = 10.0f;
			static dev_float s_fTestAngle2 = 10.0f;
			static dev_float s_fZOffset = 0.5f;
			Vector3 vHandHoldOffset = vHandholdPoint;
			vHandHoldOffset.z += s_fZOffset;
			Vec3V vPedPos = VECTOR3_TO_VEC3V(vHandHoldOffset + (vPlayerForward * (m_pHandHold->GetDepth())));
			Vec3V vTestPos1 = VECTOR3_TO_VEC3V(vHandHoldOffset + (vPlayerForward * (m_pHandHold->GetDepth() + s_fDistToTest1)));
			Vec3V vTestPos2 = VECTOR3_TO_VEC3V(vHandHoldOffset + (vPlayerForward * (m_pHandHold->GetDepth() + s_fDistToTest2)));
			if(CTaskParachute::CanPedParachuteFromPosition(*pPed, vTestPos1, false, &vPedPos, s_fTestAngle1) && 
				CTaskParachute::CanPedParachuteFromPosition(*pPed, vTestPos2, true, &vPedPos, s_fTestAngle2) )
			{
				SetVaultExitFlagSet(eVEC_Parachute);
			}
		}

		//! Note: Don't do dive if we are climbing out of water. Quite annoying when tryin to get in a boat ;)
		if(ms_VaultExitConditions[eVEC_Dive].IsSupported(*m_pHandHold, pPed, m_iFlags.IsFlagSet(VF_RunningJumpVault), m_iFlags.IsFlagSet(VF_AutoVault), true) && !m_bSwimmingVault)
		{
			static dev_float s_fDistToTest1 = 1.25f;
			static dev_float s_fDistToTest2 = 3.0f;
			static dev_float s_fZOffset = 0.5f;
			Vector3 vHandHoldOffset = vHandholdPoint;
			vHandHoldOffset.z += s_fZOffset;
			Vec3V vPedPos = VECTOR3_TO_VEC3V(vHandHoldOffset + (vPlayerForward * (m_pHandHold->GetDepth())));
			Vec3V vTestPos1 = VECTOR3_TO_VEC3V(vHandHoldOffset + (vPlayerForward * (m_pHandHold->GetDepth() + s_fDistToTest1)));
			Vec3V vTestPos2 = VECTOR3_TO_VEC3V(vHandHoldOffset + (vPlayerForward * (m_pHandHold->GetDepth() + s_fDistToTest2)));
			if(CTaskMotionSwimming::CanDiveFromPosition(*pPed, vTestPos1, IsVaultExitFlagSet(eVEC_Parachute), &vPedPos, true) && 
				CTaskMotionSwimming::CanDiveFromPosition(*pPed, vTestPos2, IsVaultExitFlagSet(eVEC_Parachute), &vPedPos, true))
			{
				SetVaultExitFlagSet(eVEC_Dive);
				ClearVaultExitFlagSet(eVEC_Parachute);
			}
		}

		m_bDeferredTestCompleted = true;
	}

	// Check for ragdoll
	if(ComputeAttachmentBreakOff())
	{
		// Entering ragdoll
	}
	else if(m_ProcessSignalsNextState != -1)
	{
		m_bPelvisFixup = false;

		SetState(m_ProcessSignalsNextState);
		m_ProcessSignalsNextState = -1;
	}

	//! HACK. Deal with external code warping the ped without clearing the tasks :(
	if(HasPedWarped())
	{
#if __BANK
		taskDisplayf("Ped has been warped during vault task. Not good, so bail!");
		DumpToLog();
#endif
		SetState(State_Quit);
	}

	//! Keep ragdoll on collision going if something splats it. Note: Could do with some kind of system
	//! to prevent this happening.
	if(!pPed->IsNetworkClone() && GetIsAttachedToPhysical() && !GetPed()->GetActivateRagdollOnCollision())
	{
		CTaskVault::SetUpRagdollOnCollision(pPed, m_pHandHold->GetPhysical(), true);
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTaskVault::FSM_Return CTaskVault::StateClamberVault_OnEnter()
{
	//! Now that we know we are vaulting, set special clamber flags.
	if(IsVaultExitFlagSet(eVEC_Parachute))
	{
		m_iFlags.SetFlag(VF_ParachuteClamber);
	}
	else if(IsVaultExitFlagSet(eVEC_Dive))
	{
		m_iFlags.SetFlag(VF_DiveClamber);
	}

	// Store the collision phase
	if(!CClipEventTags::FindEventPhase(GetActiveClip(), CClipEventTags::AdjustAnimVel, m_fTurnOnCollisionPhase))
	{
		m_fTurnOnCollisionPhase = 0.5f;
	}

	// Reset the m_fClipLastPhase
	m_fClipLastPhase = 0.0f;

	GetPed()->SetIsSwimming(false);
	//@@: location CTASKVAULT_STATECLAMBERVAULT_ONENTER
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTaskVault::FSM_Return CTaskVault::StateClamberVault_OnUpdate()
{
	CPed *pPed = GetPed();

	bool bTesClearance = m_iFlags.IsFlagSet(VF_ParachuteClamber) || m_iFlags.IsFlagSet(VF_DiveClamber) ? false : true;

	if(ComputeAttachmentBreakOff(bTesClearance))
	{
		// Entering ragdoll
	}

	// If it's a slide, the falling/land anims are combined into 1, so do fall test here.
	TUNE_GROUP_BOOL(VAULTING,bUseJumpLandsForRunningVault,true);
	if(m_iFlags.IsFlagSet(VF_RunningJumpVault) && bUseJumpLandsForRunningVault)
	{
		if(GetActivePhase() > m_fTurnOnCollisionPhase)
		{
			// Enable collision
			pPed->EnableCollision();

			// Detach if we are attached
			if(GetIsAttachedToPhysical())
			{
				pPed->DetachFromParent(DETACH_FLAG_ACTIVATE_PHYSICS);
				pPed->SetVelocity(pPed->GetDesiredVelocity());
			}
			
			ResetRagdollOnCollision(pPed);
		}

		bool bCanLand = false;
		bool bCanFall= false;

		float fStartPhase = 0.0f;
		float fEndPhase = 0.0f;

		const crClip* pClip = GetActiveClip();
		float fCurrentPhase = GetActivePhase();

		if(CClipEventTags::GetMoveEventStartAndEndPhases(pClip, ms_VaultLandWindowId.GetHash(), fStartPhase, fEndPhase))
		{
			//! Hard code start phase. Anim tag isn't set up correctly. Don't want to change.
			fStartPhase = 0.3f;

			if(fCurrentPhase >= fStartPhase)
			{
				bCanLand = true;
			}

			if(fCurrentPhase >= fEndPhase )
			{
				bCanFall = true;
			}
		}
		else if(GetActivePhase() >= 1.0f)
		{
			bCanFall = true;
		}

		TUNE_GROUP_FLOAT(VAULTING,fRunningStepUpLandThreshold,0.1f, 0.01f, 5.0f, 0.01f);
		if(bCanLand && CTaskJump::IsPedNearGround(GetPed(), fRunningStepUpLandThreshold))
		{
			if(pPed->IsNetworkClone())
			{
				if(GetStateFromNetwork()==State_Falling || m_bCloneTaskNoLongerRunningOnOwner)
				{
					SetState(State_Falling);
				}
				return FSM_Continue;
			}

			SetState(State_ClamberJump);
		}
		else if(bCanFall)
		{
			SetState(State_Falling);
		}
	}

	// Check if we should do ground tests now
	else if( (GetActivePhase() > m_fTurnOnCollisionPhase) || (GetActivePhase() >= 1.0f) )
	{
		if(m_bSlideVault && ComputeIsOnGround())
		{
			SetState(State_ClamberStand);
		}
		else
		{
			SetState(State_Falling);
		}
	}

	//! Keep ragdoll on collision going if something splats it. Note: Could do with some kind of system
	//! to prevent this happening.
	if(!pPed->IsNetworkClone() && GetIsAttachedToPhysical() && !GetPed()->GetActivateRagdollOnCollision())
	{
		CTaskVault::SetUpRagdollOnCollision(pPed, m_pHandHold->GetPhysical(), true);
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTaskVault::FSM_Return CTaskVault::StateFalling_OnEnter()
{
	// Cache the ped
	CPed* pPed = GetPed();

	// Enable collision
	pPed->EnableCollision();

	// Enable gravity
	pPed->SetUseExtractedZ(false);

    // Detach if we are attached
    if(GetIsAttachedToPhysical())
    {
        pPed->DetachFromParent(DETACH_FLAG_ACTIVATE_PHYSICS);
		pPed->SetVelocity(pPed->GetDesiredVelocity());
	}
	
	ResetRagdollOnCollision(pPed);

	if(pPed->IsNetworkClone() && GetSubTask() && GetSubTask()->GetTaskType() == CTaskTypes::TASK_FALL)
	{
		//! Task has been created/cloned already.
	}
	else
	{
		fwFlags32 fallFlags = FF_VaultFall;

		if(m_iFlags.IsFlagSet(VF_ParachuteClamber))
		{
			fallFlags.SetFlag(FF_ForceSkyDive);
		}
		else if(m_iFlags.IsFlagSet(VF_DiveClamber))
		{
			fallFlags.SetFlag(FF_ForceDive);
		}
		else if( (GetPreviousState() == State_ClamberVault) && !IsStepUp())
		{
			fallFlags.SetFlag(FF_LongBlendOnHighFalls);
			fallFlags.SetFlag(FF_UseVaultFall);
		}

		CTaskFall* pNewTask = rage_new CTaskFall(fallFlags);
		SetNewTask(pNewTask);

		if(pPed->IsNetworkClone())
		{
			pNewTask->SetRunningLocally(true);
		}
	}

	m_bLegIKDisabled = false;

	// If in FPS Mode, disable the flag CPED_CONFIG_FLAG_ForceFPSIKWithUpperBodyAnim
	// to avoid weird IK pops when holding a gun
#if FPS_MODE_SUPPORTED
	if( pPed->IsFirstPersonShooterModeEnabledForPlayer(false) )
	{
		pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_ForceFPSIKWithUpperBodyAnim, false);
	}
#endif

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTaskVault::FSM_Return CTaskVault::StateFalling_OnUpdate()
{
	// If we're falling into water then quit the task, because otherwise we'll get stuck.
	CPed *pPed = GetPed();
	taskAssert(pPed);

	//! DMKH. Hack. We don't replicate MBR quick enough when we come out of vault, so the clone ped won't know whether to go into
	//! run or not. Just pass it the replicated running flag in this case.
	if(GetSubTask())
	{
		taskAssert(GetSubTask()->GetTaskType() == CTaskTypes::TASK_FALL);
		CTaskFall *pTaskFall = static_cast<CTaskFall*>(GetSubTask());
		pTaskFall->SetForceRunOnExit(GetVaultSpeed() == VaultSpeed_Run);
		pTaskFall->SetForceWalkOnExit(GetVaultSpeed() == VaultSpeed_Walk);

		if(m_bCloneTaskNoLongerRunningOnOwner && pTaskFall->GetState()==CTaskFall::State_Fall)
		{
			taskAssertf(pPed->IsNetworkClone(),"Only expect m_bCloneTaskNoLongerRunningOnOwner to be true on clones");
			pTaskFall->SetCloneVaultForceLand();
		}

		// Show weapon.		
		TUNE_GROUP_FLOAT(VAULTING, fFallingTimeTillUnHideWeapon, 0.2f, 0.0f, 5.0f, 0.01f);
		if(!pTaskFall->IsDiveFall() && ((GetTimeInState() >= fFallingTimeTillUnHideWeapon) || (pTaskFall->GetState() == CTaskFall::State_Landing) ) && !(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_PARACHUTE) JETPACK_ONLY(|| pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_JETPACK))))
		{
			CObject* pWeapon = pPed->GetWeaponManager() ? pPed->GetWeaponManager()->GetEquippedWeaponObject() : NULL;
			if (pWeapon)
			{
				pWeapon->SetIsVisibleForModule(SETISVISIBLE_MODULE_GAMEPLAY, true, true);
			}
		}
	}

	if (pPed && pPed->GetIsSwimming())
	{
		if (!GetSubTask() || GetSubTask()->MakeAbortable(ABORT_PRIORITY_URGENT, NULL) )
		{
			return FSM_Quit;
		}
	}

	bool bCollidedWithPed = false;
	if(pPed->GetFrameCollisionHistory() && pPed->GetFrameCollisionHistory() && pPed->GetFrameCollisionHistory()->GetFirstPedCollisionRecord())
	{
		CEntity *pEntity = pPed->GetFrameCollisionHistory()->GetFirstPedCollisionRecord()->m_pRegdCollisionEntity;
		if(pEntity && pEntity->GetIsTypePed())
		{
			CPed *pColldingPed = static_cast<CPed*>(pEntity);
			if(!pColldingPed->IsDead())
			{
				bCollidedWithPed = true;
			}
		}
	}

	if (pPed && bCollidedWithPed && !pPed->GetUsingRagdoll() && CTaskNMBehaviour::CanUseRagdoll(pPed, RAGDOLL_TRIGGER_FALL) )
	{
		// If we've fallen onto a ped then transition to rag-doll, otherwise we risk getting stuck doing a balancing act.
		SetState(State_Ragdoll);
		return FSM_Continue;
	}
	else if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		// Fall handles it's own land.
		return FSM_Quit;
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTaskVault::FSM_Return CTaskVault::StateClamberStand_OnEnter()
{
	// Store the collision phase
	if(!CClipEventTags::FindEventPhase(GetActiveClip(), CClipEventTags::AdjustAnimVel, m_fTurnOnCollisionPhase))
	{
		m_fTurnOnCollisionPhase = 0.5f;
	}

	// Store the detach phase.
	CClipEventTags::Key DetachTag("Detach",0xFFC528E4);
	if(!CClipEventTags::FindEventPhase(GetActiveClip(), DetachTag, m_fDetachPhase))
	{
		m_fDetachPhase = 0.0f;
	}

	CClipEventTags::Key ShowWeaponTag("ShowWeapon",0x178DCDFA);
	if(!CClipEventTags::FindEventPhase(GetActiveClip(), ShowWeaponTag, m_fShowWeaponPhase))
	{
		m_fShowWeaponPhase = 0.225f;
	}

	// Reset the m_fClipLastPhase
	m_fClipLastPhase = 0.0f;

	CPedWeaponManager* pWeaponMgr = GetPed()->GetWeaponManager();
	if( pWeaponMgr && GetPed()->IsLocalPlayer() && !IsStepUp() )
	{
		CWeapon* pWeapon = pWeaponMgr->GetEquippedWeapon();
		if (!pWeapon || !pWeapon->GetIsCooking()) //don't destroy cooking projectiles
		{
			// If our gun needs a reload and is single shot, set flag to force a reload once we re-equip it. 
			// ! Unless we're using new ammo caching code; toggled using m_bUseAmmoCaching tunable in player info.
			if (pWeapon && pWeapon->GetNeedsToReload(false) && pWeapon->GetClipSize() == 1 && GetPed()->GetPlayerInfo() && !GetPed()->GetPlayerInfo()->ms_Tunables.m_bUseAmmoCaching)
			{
				pWeaponMgr->SetForceReloadOnEquip(true);
			}
			pWeaponMgr->DestroyEquippedWeaponObject(false);	// Don't allow drop the current weapon, so the player can reequip it after getting out.
		}
	}

	GetPed()->SetIsSwimming(false);

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTaskVault::FSM_Return CTaskVault::StateClamberStand_OnUpdate()
{
	// Cache the ped
	CPed* pPed = GetPed();

	//! Not strictly true, but this enables a bunch of code that enforces ped on ground, plus gives early access to
	//! controls (e.g. duck).
	pPed->SetPedResetFlag( CPED_RESET_FLAG_IsLanding, true );

	//! Not set when vaulting, so set it manually, this allows us to get leg IK correct.
	pPed->SetSlopeNormal(pPed->GetGroundNormal());

	float fActivePhase = GetActivePhase();

	// Should we turn back on collision?
	if(fActivePhase > m_fTurnOnCollisionPhase)
	{
		// Enable collision
		pPed->EnableCollision();

		// Do not re-enable gravity whilst attached. Our attachment update code will use this velocity, which we don't want.
		if(!GetIsAttachedToPhysical())
		{
			// Enable gravity
			pPed->SetUseExtractedZ(false);
		}
	}

	if(fActivePhase > m_fDetachPhase)
	{
		if(GetIsAttachedToPhysical())
		{
			pPed->DetachFromParent(DETACH_FLAG_ACTIVATE_PHYSICS);
		}
		
		ResetRagdollOnCollision(pPed);
	}

	if(fActivePhase > m_fShowWeaponPhase)
	{
		// Show weapon.
		CObject* pWeapon = pPed->GetWeaponManager() ? pPed->GetWeaponManager()->GetEquippedWeaponObject() : NULL;
		if (pWeapon)
		{
			pWeapon->SetIsVisibleForModule(SETISVISIBLE_MODULE_GAMEPLAY, true, true);
		}
	}
	
	// Check for ragdoll
	if(ComputeAttachmentBreakOff(true))
	{
		// Entering ragdoll
	}
	else if(m_ProcessSignalsNextState == State_Quit || ComputeShouldInterrupt() || (CClipEventTags::HasEvent(GetActiveClip(), CClipEventTags::CriticalFrame, m_fClipLastPhase, GetActivePhase())))
	{
		//! If we are trying to aim or fire, re-create weapon.
		if(pPed->IsLocalPlayer() && !IsStepUp())
		{
			CPlayerInfo* pPlayerInfo = pPed->GetPlayerInfo();
			if(pPlayerInfo->IsAiming() || pPlayerInfo->IsFiring())
			{
				CPedWeaponManager* pWeaponManager = pPed->GetWeaponManager();
				if(pWeaponManager && pWeaponManager->GetRequiresWeaponSwitch())
				{
					pWeaponManager->CreateEquippedWeaponObject();
				}
			}
		}

		pPed->SetIsStanding(true);
		TUNE_GROUP_FLOAT(VAULTING, fForceRunStateOnExitVelocityMag, 3.0f, 0.0f, 20.0f, 0.001f);
	
		bool bStealthMode = pPed->GetMotionData()->GetUsingStealth();

#if FPS_MODE_SUPPORTED
		if(pPed->IsFirstPersonShooterModeEnabledForPlayer(false))
		{
			pPed->ForceMotionStateThisFrame(CPedMotionStates::MotionState_Aiming);
		}
		else
#endif
		{
			eVaultSpeed eSpeed = GetVaultSpeed();
			switch(eSpeed)
			{
			case(VaultSpeed_Stand):
				pPed->ForceMotionStateThisFrame(bStealthMode ? CPedMotionStates::MotionState_Stealth_Idle : CPedMotionStates::MotionState_Idle);
				break;
			case(VaultSpeed_Walk):
				pPed->ForceMotionStateThisFrame(bStealthMode ? CPedMotionStates::MotionState_Stealth_Walk : CPedMotionStates::MotionState_Walk);
				break;
			case(VaultSpeed_Run):
				if(pPed->GetVelocity().Mag2()>fForceRunStateOnExitVelocityMag)
				{
					pPed->ForceMotionStateThisFrame(bStealthMode ? CPedMotionStates::MotionState_Stealth_Run : CPedMotionStates::MotionState_Run);
				}
				else
				{
					pPed->ForceMotionStateThisFrame(bStealthMode ? CPedMotionStates::MotionState_Stealth_Walk : CPedMotionStates::MotionState_Walk);
				}
				break;
			}
		}

		if (!pPed->IsNetworkClone() && pPed->GetNetworkObject())
		{
			CNetObjPed* pPedObj = SafeCast(CNetObjPed, pPed->GetNetworkObject());

			//Since motion state is updated via "InFrequent" make sure that the remote gets set at the same time as the task state changes
			pPedObj->ForceResendAllData();
		}

		return FSM_Quit;
	}
	else if(!ComputeIsOnGround() && (!GetIsAttachedToPhysical() || (fActivePhase > m_fDetachPhase)) )
	{
		// If we are not on the ground when landing, have the fall task handle it
		SetState(State_Falling);
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTaskVault::FSM_Return CTaskVault::StateRagdoll_OnEnter()
{
	static dev_u32 MIN_TIME = 250;
	static dev_u32 MAX_TIME = 10000;
	Assert(GetPed());
	nmDebugf2("[%u] Adding nm balance task:%s(%p) task vault.", fwTimer::GetTimeInMilliseconds(), GetPed()->GetModelName(), GetPed());
	aiTask* pTempResponse = rage_new CTaskNMBalance(MIN_TIME, MAX_TIME, NULL, 0, NULL, 0.0f, NULL, CTaskNMBalance::BALANCE_DEFAULT);
	aiTask* pTaskResponse = rage_new CTaskNMControl(MIN_TIME, MAX_TIME, pTempResponse, CTaskNMControl::DO_BLEND_FROM_NM);
	SetNewTask(pTaskResponse);
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTaskVault::FSM_Return CTaskVault::StateSlide_OnEnter()
{
	// Cache the ped
	CPed* pPed = GetPed();

	// Reset Scale vars.
	m_fClipAdjustStartPhase = 0.0f;
	m_fClipAdjustEndPhase = 1.0f;
	m_fClipLastPhase = 0.0f;
	m_fStartScalePhase = 0.0f;
	m_vTargetDist = Vector3::ZeroType;

	m_bSlideVault = true;

	m_bNetworkBlendedIn = false;

	pPed->ResetStandData();

	CalcTotalAnimatedDisplacement();

	taskAssert(GetActiveClip());

	return FSM_Continue;
}

CTaskVault::FSM_Return CTaskVault::StateSlide_OnUpdate()
{
	// Cache the ped
	CPed* pPed = GetPed();

	if(ComputeAttachmentBreakOff(true))
	{
		//! do ragdoll/break off.
	}
	else
	{
		const crClip* pClip = GetActiveClip();
		if(pClip && !m_bNetworkBlendedIn)
		{
			const crTag* pTag = CClipEventTags::FindFirstEventTag(pClip, CClipEventTags::AdjustAnimVel);
			if(pTag)
			{
				float fActivePhase = GetActivePhase();
				float fStartPhase = pTag->GetStart();
				float fEndPhase = pTag->GetEnd();

				if(fActivePhase > pTag->GetStart())
				{
					fStartPhase = fActivePhase;
					fEndPhase = fActivePhase + (pTag->GetEnd() - pTag->GetStart());
					m_fStartScalePhase = fActivePhase;
				}

				//! Test teleporting handhold physical to see how attachment code responds.
				/*static dev_bool s_bTestTeleportAttachPhyical = true;
				if(s_bTestTeleportAttachPhyical)
				{
					Vector3 vPhysicalPosition(VEC3V_TO_VECTOR3(m_pHandHold->GetPhysical()->GetTransform().GetPosition()));
					static Vector3 vOffset(100.0f, 100.0f, 0.0f); 
					vPhysicalPosition += vOffset;
					m_pHandHold->GetPhysical()->Teleport(vPhysicalPosition, m_pHandHold->GetPhysical()->GetTransform().GetHeading(), false, true, true, true);
				}*/

				Vector3 vPedPosition = GetPedPositionIncludingAttachment();
	
				Vector3 vDrop = GetDropPointForSlide();
				m_vTargetDist = vDrop - vPedPosition;
				m_vTargetDist.z += pPed->GetCapsuleInfo()->GetGroundToRootOffset();

				CalcTotalAnimatedDisplacement();

				SetDesiredVelocityAdjustmentRange(pClip, fStartPhase, fEndPhase);
			}

			m_bNetworkBlendedIn = true;
		}

		if(m_ProcessSignalsNextState != -1)
		{
			SetState(m_ProcessSignalsNextState);
			m_ProcessSignalsNextState = -1;
		}
	}

	//! HACK. Deal with external code warping the ped without clearing the tasks :(
	if(HasPedWarped())
	{
#if __BANK
		taskDisplayf("Ped has been warped during vault task. Not good, so bail!");
		DumpToLog();
#endif
		SetState(State_Quit);
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTaskVault::FSM_Return CTaskVault::StateClamberJump_OnEnter()
{
	CPed *pPed = GetPed();

	// Re-Enable collision
	pPed->EnableCollision();

	// Enable gravity
	pPed->SetUseExtractedZ(false);

	// Detach if we are attached
	if(GetIsAttachedToPhysical())
	{
		pPed->DetachFromParent(DETACH_FLAG_ACTIVATE_PHYSICS);
		pPed->SetVelocity(pPed->GetDesiredVelocity());
	}

	ResetRagdollOnCollision(pPed);

	if(pPed->IsNetworkClone() && GetSubTask() && GetSubTask()->GetTaskType() == CTaskTypes::TASK_JUMP)
	{
		//! Task has been created/cloned already.
	}
	else
	{
		if(pPed->IsNetworkClone() && m_moveNetworkHelper.IsNetworkActive()==false)
		{
#if __ASSERT
			DumpToLog();
			Assertf(0,"%s Previous state %s Net State %s. Would have caused B*1990818 assert",
				pPed->GetDebugName(),
				GetStateName(GetPreviousState()),
				GetStateName(GetStateFromNetwork()));
#endif
			return FSM_Quit;
		}
		
		fwFlags32 jumpFlags;
		if(IsVaultExitFlagSet(eVEC_Parachute))
		{
			jumpFlags.SetFlag(JF_ForceParachuteJump);
			jumpFlags.SetFlag(JF_AutoJump);
		}
		else if(IsVaultExitFlagSet(eVEC_Dive))
		{
			jumpFlags.SetFlag(JF_ForceDiveJump);
			jumpFlags.SetFlag(JF_AutoJump);
		}
		else if(m_iFlags.IsFlagSet(VF_RunningJumpVault)) 
		{
			jumpFlags.SetFlag(JF_ForceRunningLand);

			if(m_moveNetworkHelper.GetBoolean(ms_LandRightFootId))
			{
				jumpFlags.SetFlag(JF_ForceRightFootLand);
			}
		}

		CTaskJump* pNewTask = rage_new CTaskJump(jumpFlags);
		SetNewTask(pNewTask);

		if(pPed->IsNetworkClone())
		{
			pNewTask->SetRunningLocally(true);
		}
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTaskVault::FSM_Return CTaskVault::StateClamberJump_OnUpdate()
{
	//! Let network set new state.
	if(GetPed()->IsNetworkClone() && (GetStateFromNetwork() > State_ClamberJump) )
	{
		SetState(GetStateFromNetwork());
	}

	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		return FSM_Quit;
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTaskVault::FSM_Return CTaskVault::StateRagdoll_OnUpdate()
{
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		return FSM_Quit;
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

const crClip* CTaskVault::GetActiveClip(s32 iIndex, s32 iForcedState) const
{
	s32 sStateToTest = iForcedState >= 0 ? iForcedState : GetState();

	switch(sStateToTest)
	{
	case State_Init:
	case State_Climb:
		{
			switch(iIndex)
			{
			case 0:
				return m_moveNetworkHelper.GetClip(ms_StartClip1Id);
			case 1:
				return m_moveNetworkHelper.GetClip(ms_StartClip2Id);
			default:
				taskAssertf(0, "iIndex [%d] not handled", iIndex);
				break;
			}
		}
		break;
	case State_ClamberVault:
		{
			if(m_bSlideVault)
			{
				return m_moveNetworkHelper.GetClip(ms_EndClipId);
			}
			else
			{
				return m_moveNetworkHelper.GetClip(ms_FallClipId);
			}
		}
	case State_ClamberStand:
		{
			return m_moveNetworkHelper.GetClip(ms_EndClipId);
		}
	case State_Slide:
		{
			return m_moveNetworkHelper.GetClip(ms_SlideClipId);
		}
	default:
		break;
	}

	return NULL;
}

////////////////////////////////////////////////////////////////////////////////

float CTaskVault::GetActivePhase(s32 iIndex) const
{
	switch(GetState())
	{
	case State_Climb:
		{
			switch(iIndex)
			{
			case 0:
				return Clamp(m_moveNetworkHelper.GetFloat(ms_StartPhase1Id), 0.0f, 1.0f);
			case 1:
				return Clamp(m_moveNetworkHelper.GetFloat(ms_StartPhase2Id), 0.0f, 1.0f);
			default:
				taskAssertf(0, "iIndex [%d] not handled", iIndex);
				break;
			}
		}
	case State_ClamberVault:
		{
			if(m_bSlideVault)
			{
				return Clamp(m_moveNetworkHelper.GetFloat(ms_EndPhaseId), 0.0f, 1.0f);
			}
			else
			{
				return Clamp(m_moveNetworkHelper.GetFloat(ms_FallPhaseId), 0.0f, 1.0f);
			}
		}
	case State_ClamberStand:
		{
			return Clamp(m_moveNetworkHelper.GetFloat(ms_EndPhaseId), 0.0f, 1.0f);
		}
	case State_Slide:
		{
			return Clamp(m_moveNetworkHelper.GetFloat(ms_SlidePhaseId), 0.0f, 1.0f);
		}
	default:
		break;
	}

	return 0.0f;
}

////////////////////////////////////////////////////////////////////////////////

float CTaskVault::GetActiveWeight() const
{
	switch(GetState())
	{
	case State_Climb:
		return Clamp(m_moveNetworkHelper.GetFloat(ms_StartWeightId), 0.0f, 1.0f);
	default:
		return 0.0f;
	}
}

CTaskVault::eVaultSpeed CTaskVault::GetVaultSpeed() const 
{
	const CPed* pPed = GetPed();
	if(pPed->IsNetworkClone())
	{
		return m_eVaultSpeed;
	}
	else
	{
		bool bTryingToMove = false;
		if(pPed->IsLocalPlayer())
		{	
			float fForwardDot = -1.0f;

			if( (GetState() == State_ClamberStand) || (GetState() == State_Climb) )
			{
				static dev_float s_fRunForwardDot = -0.3f;
				static dev_float s_fWalkForwardDot = -0.3f;

				if(pPed->IsBeingForcedToRun() || pPed->GetPlayerInfo()->ControlButtonSprint(CPlayerInfo::SPRINT_ON_FOOT, true) > 0.0f)
				{
					fForwardDot = s_fRunForwardDot;
				}
				else
				{
					fForwardDot = s_fWalkForwardDot;
				}
			}

			if(IsPushingStickInDirection(pPed,fForwardDot))
			{
				bTryingToMove = true;
			}
		}
		else
		{
			bTryingToMove = true;
		}

		bool bCanRun = false;

		//! Don't run whilst entering a vehicle or stealthing.
		if(!pPed->GetPedResetFlag(CPED_RESET_FLAG_IsEnteringVehicle) && !pPed->GetMotionData()->GetUsingStealth())
		{
			if(m_iFlags.IsFlagSet(VF_RunningJumpVault))
			{
				bCanRun = true;
			}
			else if(pPed->GetPlayerInfo())
			{
				bCanRun = pPed->IsBeingForcedToRun() || pPed->GetPlayerInfo()->ControlButtonSprint(CPlayerInfo::SPRINT_ON_FOOT, true) >= 1.0f || (m_fInitialMBR > MOVEBLENDRATIO_RUN);
			}
			else if(m_iFlags.IsFlagSet(VF_VaultFromCover) || m_iFlags.IsFlagSet(VF_AutoVault) || (m_fInitialMBR > MOVEBLENDRATIO_WALK) )
			{
				bCanRun = true;
			}
		}

		if(bTryingToMove)
		{
			if(bCanRun)
			{
				return VaultSpeed_Run;
			}
			else
			{
				return VaultSpeed_Walk;
			}
		}
	}

	return VaultSpeed_Stand;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskVault::IsPushingStickInDirection(const CPed* pPed, float fTestDot)
{
	const CControl* pControl = pPed->GetControlFromPlayer();
	if(pControl)
	{
		Vector3 vStickInput(pControl->GetPedWalkLeftRight().GetNorm(), -pControl->GetPedWalkUpDown().GetNorm(), 0.0f);

		float fStickMagSq = vStickInput.Mag2();
		if(fStickMagSq > 0.0f)
		{
			float fCamOrient = camInterface::GetHeading();
			vStickInput.RotateZ(fCamOrient);
			vStickInput.NormalizeFast();
			Vector3 vPlayerForward = VEC3V_TO_VECTOR3(pPed->GetTransform().GetB());
			vPlayerForward.z = 0.0f;
			vPlayerForward.NormalizeFast();

			float fDot = vPlayerForward.Dot(vStickInput);
			if(fDot > fTestDot)
			{
				// Only count as running if the control is pushed forward
				if(fStickMagSq > 0.5f )
				{
					return true;
				}
			}
		}
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

const float CTaskVault::GetArcadeAbilityModifier() const
{
	const CPed* pPed = GetPed();
	if (pPed)
	{
		if (!pPed->IsNetworkClone())
		{
			if (pPed->GetPlayerInfo() && pPed->GetPlayerInfo()->GetArcadeAbilityEffects().GetIsActive(AAE_PARKOUR))
			{
				return pPed->GetPlayerInfo()->GetArcadeAbilityEffects().GetModifier(AAE_PARKOUR);
			}
		}
	}
	return 1.0f;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskVault::UpdateMoVE()
{
	if(m_moveNetworkHelper.IsNetworkActive())
	{
		// Set the signals
		m_moveNetworkHelper.SetFloat(ms_HeightId,          m_pHandHold->GetHeight());
		m_moveNetworkHelper.SetFloat(ms_DepthId,           m_pHandHold->GetDepth());
		m_moveNetworkHelper.SetFloat(ms_DropHeightId,      m_pHandHold->GetDrop());
		m_moveNetworkHelper.SetFloat(ms_ClearanceHeightId, m_pHandHold->GetVerticalClearance());
		m_moveNetworkHelper.SetFloat(ms_ClearanceWidthId,  m_pHandHold->GetHorizontalClearance());
		m_moveNetworkHelper.SetFloat(ms_DistanceId,        m_fInitialDistance);

		eVaultSpeed eSpeed = GetVaultSpeed();
		m_eVaultSpeed = eSpeed;

		// Send the speed signal
		switch(eSpeed)
		{
		case(VaultSpeed_Stand):
			m_moveNetworkHelper.SetFloat(ms_SpeedId, 0.0f);
			break;
		case(VaultSpeed_Walk):
			m_moveNetworkHelper.SetFloat(ms_SpeedId, 0.5f);
			break;
		case(VaultSpeed_Run):
			m_moveNetworkHelper.SetFloat(ms_SpeedId, 1.0f);
			break;
		}
		
		float fRunningRate = 1.0f;
		float fStandRate = 1.0f;
		float fClamberRate = 1.0f;

		if(!m_iFlags.IsFlagSet(VF_RunningJumpVault))
		{
			// check for arcade ability modifier
			float fArcadeAbilityModifier = GetArcadeAbilityModifier();

			fRunningRate = m_fClimbRate * m_fRateMultiplier * fArcadeAbilityModifier;
			fStandRate = m_fClimbRate * m_fRateMultiplier * fArcadeAbilityModifier;;
			fClamberRate = m_fRateMultiplier * fArcadeAbilityModifier;;

			CPed* pPed = GetPed();
			if (pPed)
			{
				//! Add in MP modifier.
				if (pPed->IsPlayer() && CAnimSpeedUps::ShouldUseMPAnimRates())
				{
					fStandRate *= CAnimSpeedUps::ms_Tunables.m_MultiplayerClimbStandRateModifier;
					fRunningRate *= CAnimSpeedUps::ms_Tunables.m_MultiplayerClimbRunningRateModifier;
					fClamberRate *= CAnimSpeedUps::ms_Tunables.m_MultiplayerClimbClamberRateModifier;
				}
			}
		}

		bool bPlayerRunning = GetPed()->GetPlayerInfo() && GetPed()->GetPlayerInfo()->ControlButtonSprint(CPlayerInfo::SPRINT_ON_FOOT, true) > 0.1f;

		float fSlideRate = (bPlayerRunning || (m_fInitialMBR > MOVEBLENDRATIO_WALK)) ? 1.0f : sm_Tunables.m_SlideWalkAnimRate;

		// Rate
		m_moveNetworkHelper.SetFloat(ms_ClimbStandRateId, fStandRate);
		m_moveNetworkHelper.SetFloat(ms_ClimbRunningRateId, fRunningRate);
		m_moveNetworkHelper.SetFloat(ms_ClamberRateId, fClamberRate);
		m_moveNetworkHelper.SetFloat(ms_ClimbSlideRateId, fSlideRate);

		// Is Swimming?
		if (m_bSwimmingVault)
		{
			taskAssert(m_SwimClipSetRequestHelper.IsLoaded());
			m_moveNetworkHelper.SetFloat(ms_SwimmingId, 1.0f);
		}
		else
		{
			m_moveNetworkHelper.SetFloat(ms_SwimmingId, 0.0f);
		}

		m_moveNetworkHelper.SetFlag(m_bLeftFootVault, ms_LeftFootVaultFlagId);
		m_moveNetworkHelper.SetFlag(m_iFlags.IsFlagSet(VF_RunningJumpVault), ms_JumpVaultFlagId);
		
		bool bAngled = m_pHandHold->GetClimbAngle() >= sm_Tunables.m_AngledClimbTheshold;
		m_moveNetworkHelper.SetFlag(bAngled, ms_AngledClimbId);

		for(int i = 0; i < eCEC_Max; ++i)
		{
			if(IsClimbExitFlagSet(i))
			{
				m_moveNetworkHelper.SetFlag(true, ms_ClimbStandConditions[i].flagId);
			}
		}

		for(int i = 0; i < eVEC_Max; ++i)
		{
			if(IsVaultExitFlagSet(i))
			{
				m_moveNetworkHelper.SetFlag(m_bDisableVault ? false : true, ms_VaultExitConditions[i].flagId);
			}
		}

		m_moveNetworkHelper.SetFlag(m_bHandholdSlopesLeft, ms_SlideLeftFlagId);
	}
}

////////////////////////////////////////////////////////////////////////////////

Vector3 CTaskVault::GetHandIKPosWS(eHandIK eIK) const
{
	if(m_HandIK[eIK].pRelativePhysical)
	{
		return VEC3V_TO_VECTOR3(m_HandIK[eIK].pRelativePhysical->GetTransform().Transform(VECTOR3_TO_VEC3V(m_HandIK[eIK].vPosition)));
	}
	else
	{
		return m_HandIK[eIK].vPosition;
	}
}

////////////////////////////////////////////////////////////////////////////////

Vector3 CTaskVault::GetHandIKInitialPosWS(eHandIK eIK) const
{
	if(m_HandIK[eIK].pRelativePhysical)
	{
		return VEC3V_TO_VECTOR3(m_HandIK[eIK].pRelativePhysical->GetTransform().Transform(VECTOR3_TO_VEC3V(m_HandIK[eIK].vInitialPosition)));
	}
	else
	{
		return m_HandIK[eIK].vInitialPosition;
	}
}

////////////////////////////////////////////////////////////////////////////////

void CTaskVault::SetHandIKPos(eHandIK eIK, CEntity *pEntity, const Vector3 &vWSPos)
{
	if(m_HandIK[eIK].bSet)
	{
		UpdateHandIKPos(eIK, vWSPos);
	}
	else
	{
		if(pEntity && pEntity->GetIsPhysical())
		{
			CPhysical *pPhysical = static_cast<CPhysical*>(pEntity);
			m_HandIK[eIK].pRelativePhysical = pPhysical;
			m_HandIK[eIK].vPosition = VEC3V_TO_VECTOR3(pPhysical->GetTransform().UnTransform(VECTOR3_TO_VEC3V(vWSPos)));
		}
		else
		{
			m_HandIK[eIK].vPosition = vWSPos;
			m_HandIK[eIK].pRelativePhysical = NULL;
		}

		m_HandIK[eIK].vInitialPosition = m_HandIK[eIK].vPosition;
		m_HandIK[eIK].bSet = true;
		m_HandIK[eIK].nTimeSet = fwTimer::GetTimeInMilliseconds();
	}
}

void CTaskVault::UpdateHandIKPos(eHandIK eIK, const Vector3 &vWSPos)
{ 
	if(m_HandIK[eIK].bSet)
	{
		if(m_HandIK[eIK].pRelativePhysical)
		{
			m_HandIK[eIK].vPosition = VEC3V_TO_VECTOR3(m_HandIK[eIK].pRelativePhysical->GetTransform().UnTransform(VECTOR3_TO_VEC3V(vWSPos)));
		}
		else
		{
			m_HandIK[eIK].vPosition = vWSPos;
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

void CTaskVault::ProcessHandIK(CPed *pPed)
{
	taskAssert(pPed);
	if (!pPed)
	{
		return;
	}

	if (pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsHandCuffed))
	{
		return;
	}

	if(m_moveNetworkHelper.IsNetworkActive())
	{
		//! Note: I could have a window, but this is easier to manage (less anims to fixup).
		if(m_moveNetworkHelper.GetBoolean(ms_EnableArmIKId))
		{
			m_bArmIKEnabled = true;
		}

		if(m_moveNetworkHelper.GetBoolean(ms_DisableArmIKId))
		{
			m_bArmIKEnabled = false;
		}
	}

	if(!m_bArmIKEnabled)
	{
		if( !pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_ForceFPSIKWithUpperBodyAnim) && 
			(GetState() == State_Init || GetState() == State_Climb) && 
			!m_HandIK[HandIK_Left].bSet && 
			!m_HandIK[HandIK_Right].bSet)
		{
			pPed->SetPedResetFlag(CPED_RESET_FLAG_DisableArmSolver, true);
		}
		return;
	}

#if __BANK

	TUNE_GROUP_BOOL(VAULTING,bDebugVaultHandIK,false);

	if(bDebugVaultHandIK)
	{
		static dev_u32 DEBUG_EXPIRY_TIME = 1000;

		if(m_HandIK[HandIK_Left].bSet)
		{
			s32 pointHelperIdxL = pPed->GetBoneIndexFromBoneTag(BONETAG_L_PH_HAND);
			Mat34V pointHelperMtxL;
			pPed->GetSkeleton()->GetGlobalMtx(pointHelperIdxL, pointHelperMtxL);
			grcDebugDraw::Line(VEC3V_TO_VECTOR3(pointHelperMtxL.GetCol3() ), GetHandIKPosWS(HandIK_Left), Color_red, Color_red, DEBUG_EXPIRY_TIME);
		}

		if(m_HandIK[HandIK_Right].bSet)
		{
			s32 pointHelperIdxR = pPed->GetBoneIndexFromBoneTag(BONETAG_R_PH_HAND);
			Mat34V pointHelperMtxR;
			pPed->GetSkeleton()->GetGlobalMtx(pointHelperIdxR, pointHelperMtxR);
			grcDebugDraw::Line(VEC3V_TO_VECTOR3(pointHelperMtxR.GetCol3() ), GetHandIKPosWS(HandIK_Right), Color_red, Color_red, DEBUG_EXPIRY_TIME);
		}
	}
#endif

	UpdateHandIK(HandIK_Left, BONETAG_L_HAND, BONETAG_L_IK_HAND);
	UpdateHandIK(HandIK_Right, BONETAG_R_HAND, BONETAG_R_IK_HAND);
}

////////////////////////////////////////////////////////////////////////////////

void CTaskVault::UpdateHandIK(eHandIK handIndex, int UNUSED_PARAM(nHandBone), int nHelperBone)
{
	CPed *pPed = GetPed();

	float fHandholdHeight = GetHandholdPoint().z;

	TUNE_GROUP_FLOAT(VAULTING, fVaultArmIKBlendIn, 0.2f, 0.0f, 2.0f, 0.05f);
	TUNE_GROUP_FLOAT(VAULTING, fVaultArmIKBlendInSlide, 0.125f, 0.0f, 2.0f, 0.05f);
	TUNE_GROUP_FLOAT(VAULTING, fVaultArmIKBlendOutSlide, 0.125f, 0.0f, 2.0f, 0.05f);
	TUNE_GROUP_FLOAT(VAULTING, fVaultArmIKBlendOut, 0.3333f, 0.0f, 2.0f, 0.05f);
	TUNE_GROUP_FLOAT(VAULTING, nVaultArmAllowIKZUpdateTimeMs, 100, 0, 2000, 1);
	TUNE_GROUP_FLOAT(VAULTING, fHandIKOffsetFlat, 0.0f, 0.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(VAULTING, fHandIKOffsetAtMaxAngle, 0.1f, 0.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(VAULTING, fHandIKOffsetMinAngle, 0.0f, 0.0f, 90.0f, 0.1f);
	TUNE_GROUP_FLOAT(VAULTING, fHandIKOffsetMaxAngle, 45.0f, 0.0f, 90.0f, 0.1f);
	TUNE_GROUP_FLOAT(VAULTING, fHandIKZNormalThreshold, 0.1f, -1.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(VAULTING, fHorIntersectionTestTime, 0.5f, 0.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(VAULTING, fHorIntersectionTestDistance, 0.5f, 0.0f, 1.0f, 0.01f);
	TUNE_GROUP_BOOL(VAULTING, bUseFullReach, true);

	float fIKScale = Clamp( (m_pHandHold->GetHandHoldGroundAngle() - fHandIKOffsetMinAngle)  / (fHandIKOffsetMaxAngle-fHandIKOffsetMinAngle), 0.0f, 1.0f);
	float fHandIKOffset = fHandIKOffsetAtMaxAngle * fIKScale;

	static dev_float ACCEPTABLE_HAND_DRIFT_SQ			= (0.5f * 0.5f);
	static dev_float MINIMUM_HAND_IK_DISTANCE			= (0.25f);
	static dev_float HAND_PROBE_OFFSET_UP				= (0.3f);
	static dev_float HAND_PROBE_OFFSET_DOWN				= (0.3f);
	static dev_float HAND_PROBE_RADIUS					= (0.075f);
	static dev_float HAND_PROBE_RADIUS_HOR				= (0.05f);
	static dev_float HAND_PROBE_UPDATEXY_Z_STEP			= (0.01f);
	static dev_float HAND_PROBE_MAX_Y_DIFF				= (0.2f);
	static dev_float HAND_PROBE_MAX_Y_STEP				= (0.05f);

	Vector3 vHelperBonePosition(VEC3_ZERO);
	pPed->GetBonePosition(vHelperBonePosition, (eAnimBoneTag)nHelperBone);

	bool bDoIKZTest = true;
	if(m_HandIK[handIndex].bSet)
	{
		if ( m_HandIK[handIndex].bSet && (GetHandIKPosWS(handIndex) - vHelperBonePosition).Mag2() > ACCEPTABLE_HAND_DRIFT_SQ)
		{
			m_HandIK[handIndex].bFinished = true;
		}
		else if(GetState() != State_Slide && 
			(fwTimer::GetTimeInMilliseconds() > (m_HandIK[handIndex].nTimeSet + nVaultArmAllowIKZUpdateTimeMs)))
		{
			bDoIKZTest = false;
		}
	}

	if(!m_HandIK[handIndex].bFinished)
	{
		if(m_HandIK[handIndex].bDoTest)
		{
			Vector3 vecStart = vHelperBonePosition;
			Vector3 vecEnd = vHelperBonePosition;

			if(GetState() == State_Slide)
			{	
				vecStart.z += HAND_PROBE_OFFSET_UP;
				vecEnd.z -= HAND_PROBE_OFFSET_DOWN;
			}
			else
			{
				vecStart.z = fHandholdHeight + HAND_PROBE_OFFSET_UP;
				vecEnd.z = fHandholdHeight - HAND_PROBE_OFFSET_DOWN;
			}

			const CEntity* ppExceptions[1] = { NULL };
			s32 iNumExceptions = 1;
			ppExceptions[0] = pPed;
			iNumExceptions = 1;

			WorldProbe::CShapeTestCapsuleDesc capsuleDesc;
			WorldProbe::CShapeTestHitPoint testIntersection;
			WorldProbe::CShapeTestResults probeResults(testIntersection);
			capsuleDesc.SetCapsule(vecStart, vecEnd, HAND_PROBE_RADIUS);
			capsuleDesc.SetResultsStructure(&probeResults);
			capsuleDesc.SetIsDirected(true);
			capsuleDesc.SetDoInitialSphereCheck(true);
			capsuleDesc.SetIncludeFlags(ArchetypeFlags::GTA_MAP_TYPE_MOVER | ArchetypeFlags::GTA_VEHICLE_TYPE | ArchetypeFlags::GTA_OBJECT_TYPE | ArchetypeFlags::GTA_GLASS_TYPE);
			capsuleDesc.SetExcludeEntities(ppExceptions, iNumExceptions, WorldProbe::EIEO_DONT_ADD_VEHICLE_OCCUPANTS);
			capsuleDesc.SetTypeFlags(ArchetypeFlags::GTA_PED_TYPE);
			capsuleDesc.SetExcludeTypeFlags(TYPE_FLAGS_NONE);
			capsuleDesc.SetContext(WorldProbe::LOS_GeneralAI);

			Vector3 vIntersectionPosition;
			bool bHit = WorldProbe::GetShapeTestManager()->SubmitTest(capsuleDesc);
			
			//! If we miss our handhold after setting IK, just set to hand position (but retain z height, so it's still placed appropriately).
			//! This should only happen on thin ledges, so should be fine.
			if( (!bHit || (testIntersection.GetHitNormal().z < fHandIKZNormalThreshold)) && m_HandIK[handIndex].bSet && (GetState() == State_Climb) )
			{
				vIntersectionPosition = vHelperBonePosition;
				vIntersectionPosition.z = GetHandIKPosWS(handIndex).z;
				Vector3 vIKPedSpace = VEC3V_TO_VECTOR3(GetPed()->GetTransform().UnTransform(VECTOR3_TO_VEC3V(GetHandIKPosWS(handIndex))));
				Vector3 vIntersectionPedSpace = VEC3V_TO_VECTOR3(GetPed()->GetTransform().UnTransform(VECTOR3_TO_VEC3V(vIntersectionPosition)));
				
				if( (vIntersectionPedSpace.y - vIKPedSpace.y) > HAND_PROBE_MAX_Y_DIFF)
				{
					//lerp out to meet min intersection.
					vIntersectionPedSpace.y = Min( (vIntersectionPedSpace.y-HAND_PROBE_MAX_Y_DIFF), vIKPedSpace.y + HAND_PROBE_MAX_Y_STEP);
				}
				else
				{
					vIntersectionPedSpace.y = vIKPedSpace.y;
				}
				
				vIntersectionPosition = VEC3V_TO_VECTOR3(GetPed()->GetTransform().Transform(VECTOR3_TO_VEC3V(vIntersectionPedSpace)));
				bHit = true;
			}
			else
			{
				vIntersectionPosition = VEC3V_TO_VECTOR3(testIntersection.GetPosition() );
			}

			if(bHit)
			{
				CEntity* pEntity = CPhysics::GetEntityFromInst(testIntersection.GetInstance());

				//! Do another test for local player only. Get avg point between vertical & horizontal tests.
				//! This prevents IK'ing to back edge.
				u32 nStartTime = m_HandIK[handIndex].bSet ? m_HandIK[handIndex].nTimeSet : fwTimer::GetTimeInMilliseconds();
				float fRatio = ((float)(fwTimer::GetTimeInMilliseconds() - nStartTime)) / ((float)fHorIntersectionTestTime*1000.0f);
				if(GetState() == State_Climb && pPed->IsLocalPlayer() && fRatio < 1.0f)
				{	
					Vector3 vStart = vIntersectionPosition + GetHandholdNormal() * fHorIntersectionTestDistance; 
					capsuleDesc.SetCapsule(vStart, vIntersectionPosition, HAND_PROBE_RADIUS_HOR);
					bool bHit2 = WorldProbe::GetShapeTestManager()->SubmitTest(capsuleDesc);
					if(bHit2)
					{
						Vector3 vIntersectionPosition2 = VEC3V_TO_VECTOR3(testIntersection.GetPosition());

						Vector3 vPedToHit2 = (vIntersectionPosition2 - VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()));
						Vector3 vPedForward = VEC3V_TO_VECTOR3(pPed->GetTransform().GetForward());
						float fHit2Dot = vPedForward.Dot(vPedToHit2);
						
						CEntity* pEntity2 = CPhysics::GetEntityFromInst(testIntersection.GetInstance());

						if(fHit2Dot > 0.0f && (pEntity == pEntity2))
						{
							float fMaxZ = Max(vIntersectionPosition.z, vIntersectionPosition2.z);
							Vector3 vDiff = vIntersectionPosition - vIntersectionPosition2;
							vIntersectionPosition = vIntersectionPosition2 + (vDiff * fRatio);
							vIntersectionPosition.z = fMaxZ;
						}
					}
				}

				float fDistanceHandToIntersection = (vIntersectionPosition - vHelperBonePosition).Mag();
				if ((fDistanceHandToIntersection < MINIMUM_HAND_IK_DISTANCE) )
				{
					Vector3 vIKHand = vIntersectionPosition;
					if(bDoIKZTest)
					{
						SetHandIKPos(handIndex, pEntity, vIKHand);
					}
					else
					{
						//! Don't lock hands in XY. Try and lerp Z to meet.
						float fCurrentIKZ = GetHandIKPosWS(handIndex).z;

						if(vIKHand.z > fCurrentIKZ)
							vIKHand.z = Min(vIKHand.z, (fCurrentIKZ + HAND_PROBE_UPDATEXY_Z_STEP));
						else
							vIKHand.z = Max(vIKHand.z, (fCurrentIKZ - HAND_PROBE_UPDATEXY_Z_STEP));
						
						SetHandIKPos(handIndex, pEntity, vIKHand);
					}
				}
			}
		}

		TUNE_GROUP_BOOL(VAULTING, bUsePointHelper, false);
		TUNE_GROUP_BOOL(VAULTING, bUseIKHelper, true);

		s32 nFlags = AIK_TARGET_WRT_IKHELPER;

		if(bUseFullReach)
		{
			nFlags |= AIK_USE_FULL_REACH;
		}

		if(m_HandIK[handIndex].bSet)
		{
			Vector3 vIKHand = m_HandIK[handIndex].vPosition;
			vIKHand.z += fHandIKOffset;

			float fBlendIn = (GetState() == State_Slide) ? fVaultArmIKBlendInSlide : fVaultArmIKBlendIn;
			float fBlendOut = (GetState() == State_Slide) ? fVaultArmIKBlendOutSlide : fVaultArmIKBlendOut;
			if(m_HandIK[handIndex].pRelativePhysical)
			{
				pPed->GetIkManager().SetRelativeArmIKTarget( handIndex == HandIK_Left ? crIKSolverArms::LEFT_ARM : crIKSolverArms::RIGHT_ARM, 
					m_HandIK[handIndex].pRelativePhysical,
					-1,
					vIKHand,
					nFlags,
					fBlendIn,
					fBlendOut);
			}
			else
			{
				pPed->GetIkManager().SetAbsoluteArmIKTarget(handIndex == HandIK_Left ? crIKSolverArms::LEFT_ARM : crIKSolverArms::RIGHT_ARM, 
					vIKHand, 
					nFlags,
					fBlendIn,
					fBlendOut);
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

void CTaskVault::CalcTotalAnimatedDisplacement()
{
	// Check for a second blended animation
	const crClip* pClip = GetActiveClip(0);
	if(pClip)
	{
		// Get the distance the animation moves over its duration
		Vector3 vAnimTotalDist(fwAnimHelpers::GetMoverTrackDisplacement(*pClip, m_fStartScalePhase, 1.f));

		// Check for a second blended animation
		const crClip* pSecondClip = GetActiveClip(1);
		if(pSecondClip)
		{
			// Get the distance the animation moves over its duration
			Vector3 vSecondAnimTotalDist(fwAnimHelpers::GetMoverTrackDisplacement(*pSecondClip, m_fStartScalePhase, 1.f));

			// Factor this into the anim total dist
			float fBlendWeight = GetActiveWeight();
			vAnimTotalDist = vAnimTotalDist * (1.0f-fBlendWeight) + vSecondAnimTotalDist * fBlendWeight;
		}
	
		m_vTotalAnimatedDisplacement = vAnimTotalDist;
	}
}

////////////////////////////////////////////////////////////////////////////////

Vector3 CTaskVault::GetTotalAnimatedDisplacement(bool bWorldSpace) const
{
	if(bWorldSpace)
	{
		return VEC3V_TO_VECTOR3(GetPed()->GetTransform().Transform3x3(VECTOR3_TO_VEC3V(m_vTotalAnimatedDisplacement)));
	}

	return m_vTotalAnimatedDisplacement;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskVault::SetDesiredVelocityAdjustmentRange(const crClip* ASSERT_ONLY(pClip), float fStart, float fEnd)
{
	m_fClipAdjustStartPhase = fStart;
	m_fClipAdjustEndPhase   = fEnd;

	TUNE_GROUP_FLOAT(VAULTING, fClipAdjustStartPhaseOverride, -1.0f, -1.0f, 1.0f, 0.1f);
	TUNE_GROUP_FLOAT(VAULTING, fClipAdjustEndPhaseOverride, -1.0f, -1.0f, 1.0f, 0.1f);
	if(fClipAdjustStartPhaseOverride >= 0.0f)
		m_fClipAdjustStartPhase = fClipAdjustStartPhaseOverride;
	if(fClipAdjustEndPhaseOverride >= 0.0f)
		m_fClipAdjustEndPhase = fClipAdjustEndPhaseOverride;

	// Start always has to be before end
	taskAssertf(m_fClipAdjustEndPhase >= m_fClipAdjustStartPhase, "End [%.2f] has to come after start [%.2f]. Clip: %s", m_fClipAdjustEndPhase, m_fClipAdjustStartPhase, pClip->GetName());

	for(int i = 0; i < 3; ++i)
	{
		m_vClipAdjustStartPhase[i]=m_fClipAdjustStartPhase;
		m_vClipAdjustEndPhase[i]=m_fClipAdjustEndPhase;
	}

	Vector3 vDistanceDiff(m_vTargetDist - GetTotalAnimatedDisplacement());
	Vector3 vDistanceDiffLocal = VEC3V_TO_VECTOR3(GetPed()->GetTransform().UnTransform3x3(VECTOR3_TO_VEC3V(vDistanceDiff)));
	if(vDistanceDiffLocal.y > 0.0f)
	{	
		if(m_pHandHold->GetClimbAngle() > sm_Tunables.m_MinAngleForScaleVelocityExtension)
		{
			float fScale = (m_pHandHold->GetClimbAngle()-sm_Tunables.m_MinAngleForScaleVelocityExtension)/(sm_Tunables.m_MaxAngleForScaleVelocityExtension-sm_Tunables.m_MinAngleForScaleVelocityExtension);
			fScale = Min(fScale, 1.0f);
			if(m_fClipAdjustEndPhase < sm_Tunables.m_AngledClimbScaleVelocityExtensionMax)
			{
				float fXYEndPhase = m_fClipAdjustEndPhase + ((sm_Tunables.m_AngledClimbScaleVelocityExtensionMax - m_fClipAdjustEndPhase) * fScale );
				m_vClipAdjustEndPhase[0] = fXYEndPhase;
				m_vClipAdjustEndPhase[1] = fXYEndPhase;
			}
		}
	}

	//! Override slide z to get up high
	TUNE_GROUP_FLOAT(VAULTING, fClipAdjustEndPhaseSlideZ, 0.25f, 0.0f, 1.0f, 0.01f);
	if(m_bSlideVault)
	{
		m_vClipAdjustEndPhase[2] = fClipAdjustEndPhaseSlideZ;
	}
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskVault::ScaleDesiredVelocity(float fTimeStep)
{
	TUNE_GROUP_FLOAT(VAULTING, fMaxDepthToScaleBackwards, 0.5f, 0.0f, 10.0f, 0.01f);
	TUNE_GROUP_FLOAT(VAULTING, fMinClearanceToScaleBackwards, 1.75f, 0.0f, 10.0f, 0.01f);

	// Cache the ped
	CPed* pPed = GetPed();

	const crClip* pClip = GetActiveClip(0);
	if(pClip)
	{
		float fPhase = GetActivePhase(0);

		// Work out the difference between the desired distance and animated distance
		Vector3 vDistanceDiff(m_vTargetDist - GetTotalAnimatedDisplacement());
		
		if(m_iFlags.IsFlagSet(VF_RunningJumpVault) && fPhase >= 1.0f)
		{
			Vector3 vDesiredVel = VEC3V_TO_VECTOR3(pPed->GetTransform().Transform3x3(VECTOR3_TO_VEC3V(m_vLastAnimatedVelocity)));
			pPed->SetDesiredVelocity(vDesiredVel);
		}
		else
		{
			//! No XY animated velocity from slide. Just do all the lerp in code so that we don't go through slide point.
			Vector3 vAnimatedVel = pPed->GetAnimatedVelocity();
			if(GetState() == State_Climb)
			{
				if(m_bSlideVault)
				{
					vAnimatedVel.y = 0.0f;
				}
				
				//! Don't use animated vel until we have fully blended. Clones can start anim further forward than anims
				//! are authored for, so can clip through climb object.
				if(!IsStepUp() && GetTimeInState() <= NORMAL_BLEND_DURATION)
				{
					bool bMoveRateOverriden = pPed->GetMotionData()->GetDesiredMoveRateOverride() > 1.0f;
					if(pPed->IsNetworkClone() FPS_MODE_SUPPORTED_ONLY(|| pPed->IsFirstPersonShooterModeEnabledForPlayer(false)) || bMoveRateOverriden)
					{
						vAnimatedVel.y = 0.0f;
					}
				}
			}

			Vector3 vDesiredVel = VEC3V_TO_VECTOR3(pPed->GetTransform().Transform3x3(VECTOR3_TO_VEC3V(vAnimatedVel)));
			
			if(m_iFlags.IsFlagSet(VF_RunningJumpVault) &&  
				m_pHandHold->GetHorizontalClearance() > fMinClearanceToScaleBackwards)
			{
				Vector3 vDistDiffLocal = VEC3V_TO_VECTOR3(pPed->GetTransform().UnTransform3x3(VECTOR3_TO_VEC3V(vDistanceDiff)));
				static dev_float s_fMaxYVel = 0.0f;
				if(vDistDiffLocal.y < s_fMaxYVel)
				{
					//! If we are scaling backwards, just go to drop point instead.
					if(m_pHandHold->GetDepth() < fMaxDepthToScaleBackwards)
					{
						vDistanceDiff = m_vTargetDistToDropPoint - GetTotalAnimatedDisplacement();
					
						if(fPhase < m_vClipAdjustStartPhase[1])
						{
							vAnimatedVel[1] = 0.0f;
							vDistDiffLocal[1] = 0.0f;
							vDesiredVel = VEC3V_TO_VECTOR3(pPed->GetTransform().Transform3x3(VECTOR3_TO_VEC3V(vAnimatedVel)));
							vDistanceDiff = VEC3V_TO_VECTOR3(pPed->GetTransform().Transform3x3(VECTOR3_TO_VEC3V(vDistDiffLocal)));
						}
					}
					else
					{
						vDistDiffLocal[1] = 0.0f;
						vDistanceDiff = VEC3V_TO_VECTOR3(pPed->GetTransform().Transform3x3(VECTOR3_TO_VEC3V(vDistDiffLocal)));
					}
				}
			}

			for(int i = 0; i < 3; i++)
			{
				if( (m_vClipAdjustEndPhase[i] - m_vClipAdjustStartPhase[i]) != 0.f)
				{
					if(fPhase >= m_vClipAdjustStartPhase[i] && (fPhase <= m_vClipAdjustEndPhase[i] || m_fClipLastPhase <= m_vClipAdjustEndPhase[i]))
					{
						// Adjust the velocity based on the current phase and the start/end phases
						float fFrameAdjust = Min(fPhase, m_vClipAdjustEndPhase[i]) - Max(m_fClipLastPhase, m_vClipAdjustStartPhase[i]);

						if(fFrameAdjust > 0.0f) 
						{
							float fClipDuration = pClip->GetDuration();

							fFrameAdjust *= fClipDuration;
							fFrameAdjust /= fTimeStep;

							fFrameAdjust *= 1.0f/(m_vClipAdjustEndPhase[i] - m_vClipAdjustStartPhase[i]);

							vDesiredVel[i] += (vDistanceDiff[i] / fClipDuration) * fFrameAdjust;
						}
					}
				}
			}

			// Apply velocity.
			pPed->SetDesiredVelocity(vDesiredVel);

			return true;
		}
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskVault::ComputeAttachmentBreakOff(bool bTestClearance)
{
	// Get the ped
	CPed* pPed = GetPed();

	//! Check if out attachment has changed. If so, ragdoll. (indicates something like a door being broken off when climbing it).
	if(m_pHandHold->GetPhysical())
	{
		const fwAttachmentEntityExtension *extension = pPed->GetAttachmentExtension();

		if(extension && extension->GetAttachParentForced() && 
			(extension->GetAttachParentForced() != m_pHandHold->GetPhysical()))
		{
			SetState(State_Ragdoll);
			return true;
		}
	}

	// Process if we are attached
	if(GetIsAttachedToPhysical())
	{
		static dev_float PITCHED					= 0.85f;
		static dev_float PITCHED_ENTERING_VEHICLE   = 0.5f;

		float fMaxPitched = pPed->GetPedResetFlag(CPED_RESET_FLAG_IsEnteringVehicle) ? PITCHED_ENTERING_VEHICLE : PITCHED;

		// If we have pitched too much, ragdoll off to safety
		float fDotZ = DotProduct(VEC3V_TO_VECTOR3(pPed->GetTransform().GetC()), ZAXIS);

		bool bBreakOff = (fDotZ < fMaxPitched);

		if (pPed->GetNetworkObject() && m_pHandHold->GetPhysical())
		{
			// in MP, break off if the entity being climbed on is fading out. This is to prevent the ped going invisible once the fade completes
			CNetObjPhysical* pClimbingEntityObj = m_pHandHold->GetPhysical()->GetNetworkObject() ? static_cast<CNetObjPhysical*>(m_pHandHold->GetPhysical()->GetNetworkObject()) : NULL;
		
			if (pClimbingEntityObj && pClimbingEntityObj->IsFadingOut())
			{
				bBreakOff = true;
			}
		}

		if( bBreakOff && CTaskNMBehaviour::CanUseRagdoll(pPed, RAGDOLL_TRIGGER_CLIMB_FAIL))
		{
			//Detach from the parent.
			pPed->DetachFromParent(DETACH_FLAG_ACTIVATE_PHYSICS);
			pPed->SetUseExtractedZ(false);
			ResetRagdollOnCollision(pPed);
			
			//Ensure the ragdoll is valid.
			fragInstNMGta* pRagdollInst = pPed->GetRagdollInst();
			if(pRagdollInst)
			{
				//The initial velocity of the ragdoll will be calculated from the previous and current skeleton bounds.
				//This is a problem when ragdolling off of spinning objects, as the ped will have been attached for a few
				//frames and essentially inherited the object's velocity.  If the object is spinning at a high rate,
				//the ped will be thrown off in a completely ridiculous fashion.
				//This is an attempt to solve the above problem... a good example of this is trying to climb on top of rolling lamp posts / garbage cans.
				
				//The next time the ragdoll is activated, the initial velocity will be scaled to zero.
				pRagdollInst->SetIncomingAnimVelocityScaleReset(0.0f);
									
				//Added a warning per request.
				taskWarningf("Scaling initial ragdoll velocity to zero.");
			}
			
			SetState(State_Ragdoll);
			return true;
		}
	}
	
	//! Compute horizontal clearance if the underlying physical is moving. If we have lost clearance, need to resolve or 
	//! we'll go through geometry.
	if(bTestClearance)
	{
		static dev_float s_fTestRadius = 0.1f;
		static dev_float s_fTestDistance = 1.5f;
		static dev_float s_fExtraDepth = 0.1f;

		Vector3 vStartPos = GetHandholdPoint();
		vStartPos.z = pPed->GetBonePositionCached(BONETAG_SPINE1).z;
		Vector3 vEndPos = vStartPos + (VEC3V_TO_VECTOR3(pPed->GetTransform().GetForward()) * s_fTestDistance);
			
		static const s32 iNumExceptions = 2;
		const CEntity* ppExceptions[iNumExceptions] = { NULL };
		ppExceptions[0] = pPed;
		ppExceptions[1] = m_pHandHold->GetPhysical();

		// Exclude vehicle occupants if large vehicle (8+ seats)
		bool bLargeVehicle = false;
		if(m_pHandHold->GetPhysical() && m_pHandHold->GetPhysical()->GetIsTypeVehicle())
		{
			const CVehicle* pVehicle = static_cast<const CVehicle*>(m_pHandHold->GetPhysical());
			bLargeVehicle = (pVehicle->GetSeatManager()->GetMaxSeats() > 8);
		}

		bool bClimbingDynamicGeometry = GetIsAttachedToPhysical();

		//! If climbing static geometry, we have already tested mover collision, just need to check for fast moving vehicles (like trains!).
		u32 nIncludeFlags = bClimbingDynamicGeometry ? ArchetypeFlags::GTA_ALL_TYPES_MOVER : ArchetypeFlags::GTA_VEHICLE_TYPE;

		WorldProbe::CShapeTestCapsuleDesc capsuleDesc;
		WorldProbe::CShapeTestFixedResults<1> shapeTestResults;
		capsuleDesc.SetResultsStructure(&shapeTestResults);
		capsuleDesc.SetCapsule(vStartPos, vEndPos, s_fTestRadius);
		capsuleDesc.SetIsDirected(true);
		capsuleDesc.SetDoInitialSphereCheck(true);
		capsuleDesc.SetTypeFlags(ArchetypeFlags::GTA_AI_TEST);
		capsuleDesc.SetIncludeFlags(nIncludeFlags);
		capsuleDesc.SetExcludeEntities(ppExceptions, iNumExceptions, bLargeVehicle ? WorldProbe::EIEO_DONT_ADD_VEHICLE_OCCUPANTS : EXCLUDE_ENTITY_OPTIONS_NONE);
		if(WorldProbe::GetShapeTestManager()->SubmitTest(capsuleDesc))
		{
			Vector3 vHitPosition = shapeTestResults[0].GetHitPosition();
			Vector3 vDistance = vHitPosition - vStartPos;
			float fXYMag = vDistance.XYMag();

			//! Note: Check 1st valid vault type, then break out. The vault types are sorted by depth, so we always
			//! perform the least wide vault 1st.
			bool bBreakOut = false;
			for(int i = 0; i < eVEC_Max; ++i)
			{
				if(IsVaultExitFlagSet(i))
				{
					if(fXYMag < (m_pHandHold->GetDepth() + ms_VaultExitConditions[i].fHorizontalClearance + s_fExtraDepth))
					{
						if(bClimbingDynamicGeometry)
						{
							bBreakOut = true;
						}
						else
						{
							//! If climbing on static geometry, test for moving vehicles & ragdoll if we hit.
							if(shapeTestResults[0].GetHitEntity() && shapeTestResults[0].GetHitEntity()->GetIsTypeVehicle())
							{
								if(static_cast<CVehicle*>(shapeTestResults[0].GetHitEntity())->GetVelocity().Mag2() > 1.0f)
								{
									bBreakOut = true;
								}
							}
						}
					}
					break;
				}
			}

			if(bBreakOut)
			{
				//Detach from the parent.
				pPed->DetachFromParent(DETACH_FLAG_ACTIVATE_PHYSICS);
				pPed->SetUseExtractedZ(false);
				ResetRagdollOnCollision(pPed);

				TUNE_GROUP_BOOL(VAULTING, bRagdollOnLosingClearance, false);
				if( (bRagdollOnLosingClearance || !bClimbingDynamicGeometry) && CTaskNMBehaviour::CanUseRagdoll(pPed, RAGDOLL_TRIGGER_CLIMB_FAIL))
				{
					pPed->EnableCollision();
					SetState(State_Ragdoll);
					return true;
				}
			}

#if __BANK
			TUNE_GROUP_BOOL(VAULTING, bRenderAttachmentBreakOff, false);
			if( bRenderAttachmentBreakOff)
			{
				grcDebugDraw::Sphere( vStartPos, s_fTestRadius, Color32(255, 0, 0, 255), false, 1000);
				grcDebugDraw::Sphere( vEndPos, s_fTestRadius, Color32(255, 0, 0, 255), false, 1000);
				grcDebugDraw::Sphere( vHitPosition, 0.1f, Color32(0, 255, 0, 255), true, 1000);
				grcDebugDraw::Line(vStartPos, vEndPos, Color32(255, 0, 0, 255), Color32(255, 0, 0, 255), 1000 );
			}
#endif	//__BANK
		}
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskVault::ComputeShouldInterrupt() const
{
	// Get the ped
	const CPed* pPed = GetPed();

	bool bInterrupt = false;

	//! Breakout using movement breakout for stealth.
	if(GetState() == State_ClamberStand && CanBreakoutToMovementTask() && pPed->GetMotionData()->GetUsingStealth())
	{
		return true;
	}
	else if(pPed->IsNetworkClone() && m_bCloneTaskNoLongerRunningOnOwner)
	{
		bInterrupt = true;
	}
	// Only players & AI following routes can interrupt, we also interrupt as early as possible when entering vehicles using vaulting
	else if(pPed->IsUsingActionMode() || 
		pPed->GetPedResetFlag( CPED_RESET_FLAG_FollowingRoute ) || 
		pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_ENTER_VEHICLE) || 
		pPed->GetMotionData()->GetUsingStealth())
	{
		bInterrupt = true;
	}
	else if(pPed->IsLocalPlayer())
	{
		const CControl* pControl = pPed->GetControlFromPlayer();

		// Interrupt if the control is pushed forward
		if(pControl && (pControl->GetPedWalkLeftRight().GetNorm() != 0.0f || pControl->GetPedWalkUpDown().GetNorm() != 0.0f))
		{
			bInterrupt = true;
		}

#if FPS_MODE_SUPPORTED
		// Interrupt in 1st person on camera movement.
		if(pPed->IsFirstPersonShooterModeEnabledForPlayer(false) && pControl && (pControl->GetPedLookLeftRight().GetNorm() != 0.0f || pControl->GetPedLookUpDown().GetNorm() != 0.0f))
		{
			return true;
		}
#endif
	}
	else
	{
		if(pPed->IsPlayer() && pPed->IsNetworkClone())
		{
			bInterrupt = GetVaultSpeed() != VaultSpeed_Stand;
		}
		else
		{
			Vector2 vDesMbr;
			pPed->GetMotionData()->GetDesiredMoveBlendRatio(vDesMbr);
			if(vDesMbr.Mag2() > 0.01f)
			{
				bInterrupt = true;
			}
		}
	}

	if(m_moveNetworkHelper.GetBoolean(ms_RunInterrupt))
	{
		if(GetVaultSpeed() != VaultSpeed_Run)
			return true;
	}
	else if(m_moveNetworkHelper.GetBoolean(ms_WalkInterrupt))
	{
		if(GetVaultSpeed() == VaultSpeed_Stand)
			return true;
	}

	const crClip* pClip = GetActiveClip();
	if(pClip)
	{
		if(bInterrupt)
		{
			const crClip* pClip = GetActiveClip();
			if(pClip)
			{
				float fInterruptPhase;
				if(CClipEventTags::FindEventPhase(pClip, CClipEventTags::Interruptible, fInterruptPhase))
				{
					if(GetActivePhase() >= fInterruptPhase)
					{
						return true;
					}
				}
			}
		}

		if (pPed->GetPlayerInfo() && (pPed->GetPlayerInfo()->IsHardAiming() || pPed->GetPlayerInfo()->IsFiring()) )
		{
			CClipEventTags::Key AimInterruptTag("AimInterrupt",0x12398EF2);
			float fInterruptPhase;
			if(CClipEventTags::FindEventPhase(pClip, AimInterruptTag, fInterruptPhase))
			{
				if(GetActivePhase() >= fInterruptPhase)
				{
					return true;
				}
			}
		}
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskVault::CanBreakoutToMovementTask() const
{
	const CPed *pPed = GetPed();

	if (pPed->IsLocalPlayer())
	{
		switch(GetState())
		{
		case(State_ClamberStand):
			{
				const crClip* pClip = GetActiveClip();

				//! Note: The aim interrupt is the earliest we allow breakout, so just use this.
				CClipEventTags::Key MovementBreakoutTag("MovementBreakout",0x2ea75fa1);
				float fInterruptPhase;
				if(CClipEventTags::FindEventPhase(pClip, MovementBreakoutTag, fInterruptPhase))
				{
					if(GetActivePhase() >= fInterruptPhase)
					{
						return true;
					}
				}
			}
			break;
		case(State_ClamberJump):
			{
				if(GetSubTask())
				{
					taskAssert(GetSubTask()->GetTaskType() == CTaskTypes::TASK_JUMP);
					const CTaskJump *pTaskJump = static_cast<const CTaskJump*>(GetSubTask());
					return pTaskJump->CanBreakoutToMovementTask();
				}
			}
		case(State_Falling):
			{
				if(GetSubTask())
				{
					taskAssert(GetSubTask()->GetTaskType() == CTaskTypes::TASK_FALL);
					const CTaskFall *pTaskFall = static_cast<const CTaskFall*>(GetSubTask());
					return pTaskFall->CanBreakoutToMovementTask();
				}
			}
		default:
			break;
		}
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskVault::ComputeIsOnGround() const
{
	// Get the ped
	const CPed* pPed = GetPed();

	// Perform a capsule test to detect the ground. 0.15f is the same radius as fall check.
	static dev_float TEST_RADIUS = 0.15f;
	static dev_float TEST_RADIUS_LOW_HOR_CLEARANCE = 0.175f;

	static dev_float TEST_LOW_HOR_CLEARANCE = 0.5f;

	static dev_float TEST_DIST_UNDER = 1.3f;
	static dev_float TEST_DIST_UNDER_STEALTH = 1.4f;
	static dev_float TEST_DIST_UNDER_SLIDE = 1.05f;

	float fTestRadius = m_pHandHold->GetHorizontalClearance() < TEST_LOW_HOR_CLEARANCE ? TEST_RADIUS_LOW_HOR_CLEARANCE : TEST_RADIUS;

	float fTestHeight;
	if(m_bSlideVault)
	{
		fTestHeight = TEST_DIST_UNDER_SLIDE;
	}
	else if(pPed->GetMotionData()->GetUsingStealth())
	{
		fTestHeight = TEST_DIST_UNDER_STEALTH;
	}
	else
	{
		fTestHeight = TEST_DIST_UNDER;
	}

	// Get the test points
	const Vector3 vStartPoint = GetPedPositionIncludingAttachment();
	Vector3 vEndPoint(vStartPoint);
	vEndPoint.z -= fTestHeight;

#if DEBUG_DRAW
	static atHashString FALL_PROBE_1("FallProbe1",0x7FD3E8A8);
	static atHashString FALL_PROBE_2("FallProbe2",0x11E68CCF);
	static atHashString FALL_PROBE_3("FallProbe3",0x84207179);
	static atHashString FALL_PROBE_4("FallProbe4",0x15E014FA);
	ms_debugDraw.AddLine(RCC_VEC3V(vStartPoint), RCC_VEC3V(vEndPoint), Color_green, 1000, FALL_PROBE_1.GetHash());
	ms_debugDraw.AddSphere(RCC_VEC3V(vStartPoint), fTestRadius, Color_green, 1000, FALL_PROBE_2.GetHash());
	ms_debugDraw.AddSphere(RCC_VEC3V(vEndPoint), fTestRadius, Color_green, 1000, FALL_PROBE_3.GetHash());
#endif // DEBUG_DRAW
	
	static const u32  TEST_FLAGS = ArchetypeFlags::GTA_MAP_TYPE_MOVER | 
		ArchetypeFlags::GTA_VEHICLE_TYPE | ArchetypeFlags::GTA_OBJECT_TYPE | 
		ArchetypeFlags::GTA_RAGDOLL_TYPE | ArchetypeFlags::GTA_HORSE_RAGDOLL_TYPE;

	// Do a capsule test
	WorldProbe::CShapeTestFixedResults<> probeResults;
	WorldProbe::CShapeTestCapsuleDesc capsuleDesc;
	capsuleDesc.SetResultsStructure(&probeResults);
	capsuleDesc.SetCapsule(vStartPoint, vEndPoint, fTestRadius);
	capsuleDesc.SetIsDirected(true);
	capsuleDesc.SetDoInitialSphereCheck(true);
	capsuleDesc.SetIncludeFlags(TEST_FLAGS);
	capsuleDesc.SetExcludeEntity(pPed);
	capsuleDesc.SetExcludeTypeFlags(ArchetypeFlags::GTA_PICKUP_TYPE);
	capsuleDesc.SetContext(WorldProbe::LOS_GeneralAI);

	if (WorldProbe::GetShapeTestManager()->SubmitTest(capsuleDesc, WorldProbe::PERFORM_SYNCHRONOUS_TEST))
	{
		for (int i = 0; i < probeResults.GetSize(); i++)
		{
			if (probeResults[i].GetHitDetected())
			{
				bool bValidIntersection = true;
				phInst* pHitInst = probeResults[i].GetHitInst();
				CEntity* pEntity = CPhysics::GetEntityFromInst(pHitInst);
				// We don't consider intersections with small ragdoll components as valid
				if (pEntity != NULL && pEntity->GetIsTypePed())
				{
					const CPed* pPed = static_cast<const CPed*>(pEntity);
					if(pHitInst->GetClassType() != PH_INST_FRAG_PED || pPed->GetDeathState() != DeathState_Dead || !pPed->ShouldBeDead() || 
						static_cast<const fragInstNMGta*>(pHitInst)->GetTypePhysics()->GetChild(probeResults[i].GetHitComponent())->GetUndamagedMass() < CPed::ms_fLargeComponentMass)
					{
						bValidIntersection = false;
					}
				}

				if (bValidIntersection)
				{
					DEBUG_DRAW_ONLY(ms_debugDraw.AddSphere(probeResults[i].GetHitPositionV(), 0.05f, Color_red, 1000, FALL_PROBE_4.GetHash()));
					return true;
				}
			}
		}
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskVault::HasPedWarped() const
{
	const CPed *pPed = GetPed();

	if(!pPed->IsLocalPlayer())
	{
		TUNE_GROUP_FLOAT(VAULTING, fWarpTolerance, 10.0f, 0.0f, 50.0f, 0.1f);
		const Vector3 vPedPosition = GetPedPositionIncludingAttachment();
		if(!GetHandholdPoint().IsClose(vPedPosition, fWarpTolerance) )
		{
			return true;
		}
	}

	return false;
};

////////////////////////////////////////////////////////////////////////////////

bool CTaskVault::GetIsAttachedToPhysical() const
{
	const CPed *pPed = GetPed();

	const fwAttachmentEntityExtension *extension = pPed->GetAttachmentExtension();

	// Handle if we are attached
	if(extension && extension->GetAttachParentForced() && (m_pHandHold->GetPhysical() == extension->GetAttachParentForced()))
	{
		return true;
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskVault::SetUpperBodyClipSet()
{
	const CPed* pPed = GetPed();
	const CWeapon* pWeapon = pPed->GetWeaponManager() ? pPed->GetWeaponManager()->GetEquippedWeapon() : NULL;
	const CWeaponInfo* pWeaponInfo = pWeapon ? pWeapon->GetWeaponInfo() : NULL;

#if FPS_MODE_SUPPORTED
	bool bUseFPSMode = pPed->IsFirstPersonShooterModeEnabledForPlayer(false);
#endif

	if(pWeaponInfo)
	{
		m_UpperBodyClipSetId = pWeaponInfo->GetPedMotionClipSetId(*pPed);
		m_UpperBodyFilterId = pWeaponInfo->GetPedMotionFilterId(*pPed);

		if( (m_UpperBodyClipSetId != CLIP_SET_ID_INVALID) && m_UpperBodyClipSetRequestHelper.Request(m_UpperBodyClipSetId))
		{
			m_moveNetworkHelper.SetFlag(true, ms_EnabledUpperBodyFlagId);

			if(!pWeaponInfo->GetIsProjectile())
			{
				m_moveNetworkHelper.SetFlag(true, ms_EnabledIKBounceFlagId);
			}

#if FPS_MODE_SUPPORTED
			if(!bUseFPSMode)
#endif
			{
				m_moveNetworkHelper.SetClipSet(m_UpperBodyClipSetId, ms_UpperBodyClipSetId);
			}

			if(m_UpperBodyFilterId != FILTER_ID_INVALID)
			{
				m_pUpperBodyFilter = CGtaAnimManager::FindFrameFilter(m_UpperBodyFilterId.GetHash(), GetPed());
				if(taskVerifyf(m_pUpperBodyFilter, "Failed to get filter [%s]", m_UpperBodyFilterId.GetCStr()))	
				{
					m_pUpperBodyFilter->AddRef();
					m_moveNetworkHelper.SetFilter(m_pUpperBodyFilter, ms_UpperBodyFilterId);
				}
			}
		}

#if FPS_MODE_SUPPORTED
		if(bUseFPSMode)
		{
			const fwMvClipSetId appropriateWeaponClipSetId = pWeaponInfo->GetAppropriateWeaponClipSetId(GetPed(), FPS_StreamIdle);

			if(fwAnimManager::GetClipIfExistsBySetId(appropriateWeaponClipSetId, ms_WeaponBlockedClipId))
			{
				m_bHasFPSUpperBodyAnim = true;
				m_moveNetworkHelper.SetClipSet(appropriateWeaponClipSetId, ms_FPSUpperBodyClipSetId);

				if(IsStepUp())
				{
					GetPed()->SetPedConfigFlag(CPED_CONFIG_FLAG_ForceFPSIKWithUpperBodyAnim, true); 
				}
			}
		}
#endif // FPS_MODE_SUPPORTED

		ProcessFPMoveSignals();
	}
}

void CTaskVault::ProcessFPMoveSignals()
{
#if FPS_MODE_SUPPORTED
	CPed* pPed = GetPed();
	bool bFirstPersonEnabled = false;
	if(m_bHasFPSUpperBodyAnim && pPed->IsFirstPersonShooterModeEnabledForPlayer(false, true))
	{
		bFirstPersonEnabled = true;

		float fPitchRatio = 0.5f;
		const camFirstPersonShooterCamera* pCamera = camInterface::GetGameplayDirector().GetFirstPersonShooterCamera();
		if(pCamera)
		{
			fPitchRatio = Clamp(pCamera->GetPitch() / QUARTER_PI, -1.0f, 1.0f);
			fPitchRatio = (fPitchRatio + 1.0f) * 0.5f; 
		}

		m_moveNetworkHelper.SetFloat(ms_PitchId, fPitchRatio);
	}

	m_moveNetworkHelper.SetFlag(bFirstPersonEnabled, ms_FirstPersonMode);

	if(pPed->IsLocalPlayer() && pPed->GetPlayerInfo() && GetPed()->GetPedConfigFlag(CPED_CONFIG_FLAG_ForceFPSIKWithUpperBodyAnim))
	{
		pPed->SetPlayerResetFlag(CPlayerResetFlags::PRF_DISABLE_CAMERA_VIEW_MODE_CYCLE);
	}

#endif // FPS_MODE_SUPPORTED
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskVault::IsStepUp() const
{
	if(m_pHandHold)
		return m_pHandHold->GetHeight() <= ms_fStepUpHeight;

	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskVault::IsSlideDirectionClear() const
{
	static dev_float s_fTestRadius = 0.25f;
	static dev_float s_fTestHeightOffset = 0.4f;
	static dev_float s_fTestRightOffset = 0.25f;
	static dev_float s_fTestRightOffsetLegs = 0.325f;

	static dev_float s_fMaxGroundIntersectionToLine = 0.133f;

#if __BANK
	TUNE_GROUP_BOOL(VAULTING, bRenderSlideTest, false);
#endif	//__BANK

	Vector3 vSlideStart = GetHandholdPoint();
	Vector3 vSlideEnd = GetDropPointForSlide();

	//! Do a line test from start to end. If max ground position lies outside a certain threshold, then can't slide.
	{
		Vector3 vClosest;
		fwGeom::fwLine line(vSlideStart, vSlideEnd );
		line.FindClosestPointOnLine(m_pHandHold->GetMaxGroundPosition(), vClosest);
		Vector3 vDistanceToLine = m_pHandHold->GetMaxGroundPosition() - vClosest;

#if __BANK
		if(bRenderSlideTest)
		{
			grcDebugDraw::Line(vSlideStart, vSlideEnd, Color32(255, 0, 0, 255), Color32(255, 0, 0, 255), 1000 );
			grcDebugDraw::Line(m_pHandHold->GetMaxGroundPosition(), m_pHandHold->GetMaxGroundPosition() + vDistanceToLine, Color32(255, 0, 0, 255), Color32(255, 0, 0, 255), 1000 );
		}
#endif	//__BANK

		if(vDistanceToLine.z > s_fMaxGroundIntersectionToLine)
		{
			return false;
		}
	}

	//! Always allow vehicles - we want to be pretty lax about bonnet sliding.
	if(m_pHandHold->GetPhysical() && m_pHandHold->GetPhysical()->GetIsTypeVehicle())
	{
		return true;
	}

	//! Do collision tests. 2 to the side 1 at drop point.
	{
	Vector3 vRight;
	vRight.Cross(GetHandholdNormal(), ZAXIS);
	vRight.z = 0.0f;

	Vector3 vRightOffset = vRight * s_fTestRightOffset;
	Vector3 vRightOffsetLegs = vRight * s_fTestRightOffsetLegs;

	Vector3 vStartPosCentre = vSlideStart;
	vStartPosCentre.z += s_fTestHeightOffset;
	Vector3 vEndPosCentre = vSlideEnd;
	vEndPosCentre.z += s_fTestHeightOffset;

	Vector3 vStartL = vStartPosCentre;
	Vector3 vEndL = vEndPosCentre;

	Vector3 vStartR = vStartPosCentre;
	Vector3 vEndR = vEndPosCentre;

	if(m_bHandholdSlopesLeft)
	{
		vStartL += vRightOffsetLegs;
		vEndL += vRightOffsetLegs;
		vStartR -= vRightOffset;
		vEndR -= vRightOffset;
	}
	else
	{
		vStartL += vRightOffset;
		vEndL += vRightOffset;
		vStartR -= vRightOffsetLegs;
		vEndR -= vRightOffsetLegs;
	}

	static const s32 iNumExceptions = 2;
	const CEntity* ppExceptions[iNumExceptions] = { NULL };
	ppExceptions[0] = GetPed();
	ppExceptions[1] = m_pHandHold->GetPhysical();

	u32 nIncudeFlags = ArchetypeFlags::GTA_MAP_TYPE_MOVER | ArchetypeFlags::GTA_PED_TYPE | ArchetypeFlags::GTA_OBJECT_TYPE;

	WorldProbe::CShapeTestCapsuleDesc capsuleDesc;
	WorldProbe::CShapeTestFixedResults<1> shapeTestResults;
	capsuleDesc.SetResultsStructure(&shapeTestResults);
	capsuleDesc.SetCapsule(vStartL, vEndL, s_fTestRadius);
	capsuleDesc.SetTypeFlags(ArchetypeFlags::GTA_AI_TEST);
	capsuleDesc.SetIncludeFlags(nIncudeFlags);
	capsuleDesc.SetExcludeEntities(ppExceptions,iNumExceptions);
	
	bool bHitL = WorldProbe::GetShapeTestManager()->SubmitTest(capsuleDesc);
	bool bHitR = false;
	if(!bHitL)
	{
		capsuleDesc.SetCapsule(vStartR, vEndR, s_fTestRadius);
		bHitR = WorldProbe::GetShapeTestManager()->SubmitTest(capsuleDesc);
	}

#if __BANK
	if( bRenderSlideTest)
	{
		grcDebugDraw::Line(vStartL, vEndL, Color32(255, 0, 0, 255), Color32(255, 0, 0, 255), 1000 );
		grcDebugDraw::Sphere( vStartL, s_fTestRadius, Color32(255, 0, 0, 255), false, 1000);
		grcDebugDraw::Sphere( vEndL, s_fTestRadius, Color32(255, 0, 0, 255), false, 1000);
		if(bHitL)
		{
			Vector3 vHitPosition = shapeTestResults[0].GetHitPosition();
			grcDebugDraw::Sphere( vHitPosition, 0.1f, Color32(0, 255, 0, 255), true, 1000);
		}

		grcDebugDraw::Line(vStartR, vEndR, Color32(255, 0, 0, 255), Color32(255, 0, 0, 255), 1000 );
		grcDebugDraw::Sphere( vStartR, s_fTestRadius, Color32(255, 0, 0, 255), false, 1000);
		grcDebugDraw::Sphere( vEndR, s_fTestRadius, Color32(255, 0, 0, 255), false, 1000);
		if(bHitR)
		{
			Vector3 vHitPosition = shapeTestResults[0].GetHitPosition();
			grcDebugDraw::Sphere( vHitPosition, 0.1f, Color32(0, 255, 0, 255), true, 1000);
		}
	}
#endif	//__BANK

	if(!bHitL && !bHitR)
	{
		//! Test if there is a large drop at the end.
		static dev_float s_fDistToTest = 2.5f;
		static dev_float s_fZOffset = 0.5f;

		Vector3 vDirection = m_iFlags.IsFlagSet(VF_DontOrientToHandhold) ? -m_pHandHold->GetAlignDirection() : -GetHandholdNormal();

		Vector3 vSlideToPoint = vSlideEnd;
		Vector3 vTestEnd = vSlideToPoint;

		vSlideToPoint.z += s_fZOffset;

		vTestEnd += vDirection * s_fDistToTest;
		vTestEnd.z -= ms_VaultExitConditions[eVEC_Slide].fMaxDrop;

		capsuleDesc.SetIncludeFlags(ArchetypeFlags::GTA_MAP_TYPE_MOVER | ArchetypeFlags::GTA_OBJECT_TYPE);
		capsuleDesc.SetCapsule(vSlideToPoint, vTestEnd, s_fTestRadius);
		bool bHitGround = WorldProbe::GetShapeTestManager()->SubmitTest(capsuleDesc);
#if __BANK
		if( bRenderSlideTest)
		{
			grcDebugDraw::Line(vSlideToPoint, vTestEnd, bHitGround ? Color32(0, 255, 0, 255) : Color32(255, 0, 0, 255), 1000 );
			grcDebugDraw::Sphere( vSlideToPoint, s_fTestRadius, bHitGround ? Color32(0, 255, 0, 255) : Color32(255, 0, 0, 255), false, 1000);
			grcDebugDraw::Sphere( vTestEnd, s_fTestRadius, bHitGround ? Color32(0, 255, 0, 255) : Color32(255, 0, 0, 255), false, 1000);
		}
#endif	//__BANK

		if(bHitGround)
		{
			return true;
		}
	}
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

Vector3 CTaskVault::GetDropPointForSlide() const
{
	static dev_float s_fExtraSlideDist = 0.1f;

	Vector3 vSlideToPoint = m_pHandHold->GetGroundPositionAtDrop();

	Vector3 vSlideDir = vSlideToPoint - GetPedPositionIncludingAttachment();
	float fSlideDist = vSlideDir.XYMag() + s_fExtraSlideDist;

	Vector3 vDirection;
	if(m_iFlags.IsFlagSet(VF_DontOrientToHandhold))
	{
		vDirection = -m_pHandHold->GetAlignDirection();
	}
	else
	{
		vDirection = -GetHandholdNormal();
	}

	Vector3 vDrop = GetHandholdPoint() + (vDirection * fSlideDist);
	vDrop.z = vSlideToPoint.z;

	return vDrop;
}

////////////////////////////////////////////////////////////////////////////////

Vector3 CTaskVault::GetPedPositionIncludingAttachment() const
{
	Vector3 vPedPosition = VEC3V_TO_VECTOR3(GetPed()->GetTransform().GetPosition());
	
	if(GetIsAttachedToPhysical())
	{
		const fwAttachmentEntityExtension *extension = GetPed()->GetAttachmentExtension();
		if(extension)
		{
			Vector3 vPosAttachOffset = extension->GetAttachOffset();
			Matrix34 boneMat;
			m_pHandHold->GetPhysicalMatrix(boneMat);
			boneMat.Transform(vPosAttachOffset);
			vPedPosition = vPosAttachOffset;
		}
	}

	return vPedPosition;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskVault::ProcessDisableVaulting()
{
	CPed *pPed = GetPed();

	//! Turn off vault if we we prefer to stand?
	if(!m_iFlags.IsFlagSet(VF_DontDisableVaultOver))
	{
		if(!pPed->IsNetworkClone() && 
			(m_nVaultExitConditions > 0) && 
			m_nClimbExitConditions.GetAllFlags() && 
			!pPed->GetPedResetFlag(CPED_RESET_FLAG_IsEnteringVehicle))
		{
			if(m_pHandHold->IsMaxDrop() && !IsVaultExitFlagSet(eVEC_Dive) && !IsVaultExitFlagSet(eVEC_Parachute))
			{
				m_bDisableVault = true;
			}
			else
			{
				if(pPed->IsPlayer())
				{
					m_bDisableVault = !IsPushingStickInDirection(pPed, sm_Tunables.m_DisableVaultForwardDot);
				}
				else
				{
					m_bDisableVault = (GetVaultSpeed() == VaultSpeed_Stand);
				}
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

void CTaskVault::UpdateFPSHeadBound() 
{
	//! Reset every frame.
	m_bCanUseFPSHeadBound = false;

	//! Do a test from ped to head. If something is in the way, then we can't use head bound as it may result in the 
	//! ped being pushed through collision.
#if FPS_MODE_SUPPORTED
	if(GetPed()->IsFirstPersonShooterModeEnabledForPlayer(false) && GetPed()->IsCollisionEnabled())
	{
		Vector3 vStart = VEC3V_TO_VECTOR3(GetPed()->GetTransform().GetPosition());
		Vector3 vEnd;
		GetPed()->GetBonePosition(vEnd, BONETAG_HEAD);
		Vector3 vDirection = vEnd - vStart;
		vDirection.Normalize();
		TUNE_GROUP_FLOAT(CLIMB_TEST, fClimbFPTune, 0.1f, 0.0, 1.0f, 0.01f);
		vEnd += vDirection * fClimbFPTune;

		WorldProbe::CShapeTestProbeDesc probeDesc;
		WorldProbe::CShapeTestHitPoint testIntersection;
		WorldProbe::CShapeTestResults probeResults(testIntersection);
		probeDesc.SetStartAndEnd(vStart, vEnd);
		probeDesc.SetResultsStructure(&probeResults);
		probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_MAP_TYPE_MOVER | ArchetypeFlags::GTA_VEHICLE_TYPE);
		probeDesc.SetExcludeEntity(GetPed());
		probeDesc.SetTypeFlags(ArchetypeFlags::GTA_PED_TYPE);
		probeDesc.SetExcludeTypeFlags(TYPE_FLAGS_NONE);
		probeDesc.SetContext(WorldProbe::LOS_GeneralAI);

		bool bHit = WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc);
		if(bHit)
		{
			m_bCanUseFPSHeadBound = false;
		}
		else
		{
			m_bCanUseFPSHeadBound = true;
		}
	}
#endif
}


REGISTER_TUNE_GROUP_INT(iRagdollOnCollisionVaultPartsMask, 
						0xFFFFFFFF & ~(BIT(RAGDOLL_HAND_LEFT) | BIT(RAGDOLL_HAND_RIGHT) | BIT(RAGDOLL_FOOT_LEFT) | BIT(RAGDOLL_FOOT_RIGHT) | 
						BIT(RAGDOLL_LOWER_ARM_LEFT) | BIT(RAGDOLL_LOWER_ARM_RIGHT) | BIT(RAGDOLL_UPPER_ARM_LEFT) | BIT(RAGDOLL_UPPER_ARM_RIGHT)));

void CTaskVault::SetUpRagdollOnCollision(CPed *pPed, CPhysical *pIgnorePhysical, bool bIgnoreShins)
{
	TUNE_GROUP_FLOAT(VAULTING, fRagdollOnCollisionAllowedPenetrationVault, 0.1f, 0.0f, 100.0f, 0.01f);
	TUNE_GROUP_FLOAT(VAULTING, fRagdollOnCollisionAllowedSlopeVault, -1.0f, -1.0f, 0.0f, 0.01f);

#if __BANK
		if(!hasAddedBankItem_iRagdollOnCollisionVaultPartsMask)
		{
			bkBank* pBank = BANKMGR.FindBank("_TUNE_");
			if(pBank)
			{
				bkGroup* pGroup = GetOrCreateGroup(pBank, "VAULTING");
				pBank->SetCurrentGroup(*pGroup);
				pBank->PushGroup("iRagdollOnCollisionVaultPartsMask", false);
				extern parEnumData parser_RagdollComponent_Data;
				for(int i = 0; i < parser_RagdollComponent_Data.m_NumEnums; i++)
				{
					pBank->AddToggle(parser_RagdollComponent_Data.m_Names[i], reinterpret_cast<u32*>(&iRagdollOnCollisionVaultPartsMask), 1 << i);
				}
				pBank->PopGroup();
				pBank->UnSetCurrentGroup(*pGroup);
				hasAddedBankItem_iRagdollOnCollisionVaultPartsMask = true;
			}
		}
#endif

	int iRagdollPartsMask = iRagdollOnCollisionVaultPartsMask;
	if(bIgnoreShins)
	{
		iRagdollPartsMask = iRagdollPartsMask &~ ( BIT(RAGDOLL_SHIN_LEFT) | BIT(RAGDOLL_SHIN_RIGHT ) );
	}

	fwFlags32 iFlags = FF_ForceHighFallNM;
	CEventGivePedTask event(PED_TASK_PRIORITY_PRIMARY, rage_new CTaskFall(iFlags), false, E_PRIORITY_GIVE_PED_TASK, true);
	pPed->SetActivateRagdollOnCollisionEvent(event);
	pPed->SetActivateRagdollOnCollision(true, true);
	pPed->SetRagdollOnCollisionAllowedPenetration(fRagdollOnCollisionAllowedPenetrationVault);
	pPed->SetRagdollOnCollisionAllowedSlope(fRagdollOnCollisionAllowedSlopeVault);
	pPed->SetRagdollOnCollisionAllowedPartsMask(iRagdollPartsMask);
	pPed->SetRagdollOnCollisionIgnorePhysical(pIgnorePhysical);

	pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_JustLeftVehicleNeedsReset, false);
}

////////////////////////////////////////////////////////////////////////////////

void CTaskVault::ResetRagdollOnCollision(CPed *pPed)
{
	pPed->SetNoCollisionEntity(NULL);
	pPed->SetActivateRagdollOnCollision(false);
	pPed->SetRagdollOnCollisionIgnorePhysical(NULL);
	pPed->ClearActivateRagdollOnCollisionEvent();
}

////////////////////////////////////////////////////////////////////////////////

bank_bool CTaskVault::GetUsingAutoVault()
{
	return ms_bUseAutoVault;
}

////////////////////////////////////////////////////////////////////////////////

bank_bool CTaskVault::GetUsingAutoStepUp()
{
	return ms_bUseAutoVaultStepUp;
};

////////////////////////////////////////////////////////////////////////////////

// Clone task implementation for CTaskVault

////////////////////////////////////////////////////////////////////////////////
void	CTaskVault::AdjustSyncedValuesForVehicleOffset()
{
	if( taskVerifyf(m_pHandHold,"Expect a valid m_pHandHold") && 
		taskVerifyf(GetState()==CTaskVault::State_Init,"Don't expect state %d",GetState()) &&
		taskVerifyf(GetPed(), "expect a valid ped") && 
		taskVerifyf(!GetPed()->IsNetworkClone(),"Expect a local ped") )
	{
		m_pClonePhysical =m_pHandHold->GetPhysical();

		netObject *networkObject = m_pClonePhysical ? NetworkUtils::GetNetworkObjectFromEntity(m_pClonePhysical) : 0;
		s32 entityID = networkObject ? networkObject->GetObjectID() : NETWORK_INVALID_OBJECT_ID;

		if( entityID!= NETWORK_INVALID_OBJECT_ID && 
			m_pClonePhysical && 
			m_pClonePhysical->GetIsTypeVehicle())
		{
			Matrix34 mMatrix;
			CClimbHandHoldDetected::GetPhysicalMatrix(mMatrix, m_pClonePhysical, m_uClonePhysicalComponent);

			mMatrix.UnTransform3x3(m_vCloneHandHoldNormal);
			mMatrix.UnTransform3x3(m_vCloneStartDirection);
			mMatrix.UnTransform(m_vCloneStartPosition);
			mMatrix.UnTransform(m_vCloneHandHoldPoint);
		}
		else
		{
			m_pClonePhysical =NULL;
		}
	}
}

CTaskInfo* CTaskVault::CreateQueriableState() const
{
	float fMultiplier = m_fRateMultiplier * GetArcadeAbilityModifier();
	CTaskInfo* pInfo = NULL;

	pInfo =  rage_new CClonedTaskVaultInfo(GetState(), 
		m_vCloneStartPosition,
		m_vCloneHandHoldPoint,
		m_vCloneHandHoldNormal,
		m_vCloneStartDirection,
		m_pClonePhysical,
		(u8)m_eVaultSpeed,
		m_iFlags,
		m_iCloneUsesVaultExitCondition,
        m_fInitialDistance,
		fMultiplier,
		m_fCloneClimbAngle,
		m_bDisableVault,
		m_bCloneClimbStandCondition,
		m_bCloneUseTestDirection,
		m_bCloneUsedSlide,
		m_uClonePhysicalComponent,
		m_uCloneMinNumDepthTests);

	return pInfo;
}

////////////////////////////////////////////////////////////////////////////////
void CTaskVault::ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo)
{
	taskAssert(pTaskInfo->GetTaskInfoType() == CTaskInfo::INFO_TYPE_VAULT);
	// Call the base implementation - syncs the state
	CClonedTaskVaultInfo *vaultInfo = static_cast<CClonedTaskVaultInfo*>(pTaskInfo);

	m_eVaultSpeed         = static_cast<eVaultSpeed>(vaultInfo->GetVaultSpeed());
	m_iFlags              = vaultInfo->GetFlags();
	m_vCloneStartPosition = vaultInfo->GetStartPos();
	m_vCloneStartDirection  = vaultInfo->GetStartDir();
	m_fCloneClimbAngle	  = vaultInfo->GetHandHoldClimbAngle();
    m_fInitialDistance    = vaultInfo->GetInitialDistance();
	m_fRateMultiplier     = vaultInfo->GetRateMultiplier();
	m_bDisableVault		  = vaultInfo->GetDisableVault();
	m_iCloneUsesVaultExitCondition = vaultInfo->GetExitCondition();
	m_bCloneClimbStandCondition = vaultInfo->GetClimbStandCondition();
	m_bCloneUseTestDirection = vaultInfo->GetCloneUseTestDirection();
	m_bCloneUsedSlide = vaultInfo->GetCloneUsedSlide();
	m_vCloneHandHoldPoint = vaultInfo->GetHandHoldPoint();
	m_vCloneHandHoldNormal = vaultInfo->GetHandHoldNormal();
	m_uClonePhysicalComponent = vaultInfo->GetPhysicalComponent();
	m_uCloneMinNumDepthTests = vaultInfo->GetMinNumDepthTests();
	m_vCloneHandHoldNormal.Normalize();

	if(vaultInfo->GetPhysicalEntity())
	{
		taskAssert(vaultInfo->GetPhysicalEntity()->GetIsPhysical());
		m_pClonePhysical = static_cast<CPhysical*>(vaultInfo->GetPhysicalEntity());
	}

	CTaskFSMClone::ReadQueriableState(pTaskInfo);
}

////////////////////////////////////////////////////////////////////////////////
bool CTaskVault::OverridesNetworkBlender(CPed* pPed)
{
	if(GetSubTask() && GetSubTask()->GetTaskType() == CTaskTypes::TASK_FALL)
	{
		return ((CTaskFSMClone*)GetSubTask())->OverridesNetworkBlender(pPed);
	}

	return true; 
}

////////////////////////////////////////////////////////////////////////////////
bool CTaskVault::OverridesNetworkAttachment(CPed *UNUSED_PARAM(pPed)) 
{
	// if we're parachuting, return false and let TaskParachute decide if we should have something attached...
	if(GetSubTask() && GetSubTask()->GetTaskType() == CTaskTypes::TASK_FALL)
	{
		if(GetSubTask()->GetState() == CTaskFall::State_Parachute)
		{
			return false;
		}
	}

	return true; 
}

////////////////////////////////////////////////////////////////////////////////
CTask::FSM_Return CTaskVault::UpdateClonedFSM	(const s32 iState, const FSM_Event iEvent)
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	if(GetStateFromNetwork() == State_Quit)
	{
		return FSM_Quit;
	}
	else if (iEvent == OnUpdate)
	{
		if(GetTimeInState() > 10.0f) 
		{
			bool parachuting = false;

			if(GetState() == State_Falling)
			{
				if(GetSubTask() && GetSubTask()->GetTaskType() == CTaskTypes::TASK_FALL)
				{
					if(GetSubTask()->GetState() == CTaskFall::State_Parachute)
					{
						// if we're parachuting, don't automatically kill off vault...
						parachuting = true;
					}
				}
			}

			if(!parachuting && ( (GetStateFromNetwork() != iState) || m_bCloneTaskNoLongerRunningOnOwner) )
			{
#if __BANK
				DumpToLog();
#endif
				taskAssertf(0, "%s was stuck in vault state %s, quitting out!  m_moveNetworkHelper %s", 
					GetPed()->GetDebugName(),
					iState>=0?GetStateName(iState):"-1",
					m_moveNetworkHelper.IsNetworkActive()?"Active":"Inactive");

				return FSM_Quit;
			}
		}		
		
		//! Clones wait until owner has told them to go to clamber jump.
		if( (GetState() > State_Init ) && (GetStateFromNetwork() != GetState()) && (GetStateFromNetwork() == State_ClamberJump) )
		{
			SetState(State_ClamberJump);
			return FSM_Continue;
		}

		UpdateClonedSubTasks(pPed, GetState());
	}

	return UpdateFSM( iState, iEvent);
}

////////////////////////////////////////////////////////////////////////////////
void CTaskVault::OnCloneTaskNoLongerRunningOnOwner()
{
	//! DMKH. Let task finish itself. It looks pretty bad when it gets cancelled.
	//SetStateFromNetwork(State_Quit);
	
	m_bCloneTaskNoLongerRunningOnOwner = true;

	CPed *pPed = GetPed();

	Assert(pPed);
	
	if(GetState()==State_Init)
	{
		//If remote has bailed while we are still only in Init then bail clone also
		SetStateFromNetwork(State_Quit);
	}

	//If jump task has started on remote ped then quit
	if(pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_JUMP))
	{
		SetStateFromNetwork(State_Quit);
	}
	//If dropdown task has started on remote ped then quit
	if(pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_DROP_DOWN))
	{
		SetStateFromNetwork(State_Quit);
	}
	//If ped has started melee mid climb
	if(pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_MELEE))
	{
		SetStateFromNetwork(State_Quit);
	}
}

void CTaskVault::UpdateClonedSubTasks(CPed* pPed, int const iState)
{
	CTaskTypes::eTaskType expectedTaskType = CTaskTypes::TASK_INVALID_ID;
	switch(iState)
	{
	case State_Falling:
		expectedTaskType = CTaskTypes::TASK_FALL; 
		break;
	case State_ClamberJump:
		expectedTaskType = CTaskTypes::TASK_JUMP; 
		break;
	default:
		return;
	}

	if(expectedTaskType != CTaskTypes::TASK_INVALID_ID)
	{
		CTask *pCloneSubTask = pPed->GetPedIntelligence()->CreateCloneTaskForTaskType(expectedTaskType);
		if(pCloneSubTask)
		{
			SetNewTask(pCloneSubTask);
		}
	}
}

CTaskFSMClone*	CTaskVault::CreateTaskForClonePed(CPed* UNUSED_PARAM(pPed))
{
	CTaskVault *newTask = NULL;

	//! DMKH. Don't support migration yet.
	/*
	{
		CClimbHandHoldDetected dummyhandHold; //the state will create this for the cloned ped
		const Vector3 vStart(Vector3::ZeroType);
		newTask = rage_new CTaskVault(dummyhandHold, vStart);
	}
	*/

	return newTask;
}

CTaskFSMClone*	CTaskVault::CreateTaskForLocalPed(CPed* pPed)
{
	return CreateTaskForClonePed(pPed);
}

//-------------------------------------------------------------------------
// Task info for Task Vault Info
//-------------------------------------------------------------------------

//-------------------------------------------------------------------------
// Constructor
//-------------------------------------------------------------------------
CClonedTaskVaultInfo::CClonedTaskVaultInfo(s32 state, 
											const Vector3& vPedStartPosition, 
											const Vector3 &vHandHoldPoint,
											const Vector3 &vHandHoldNormal,
											const Vector3 &vPedStartDirection,
											CEntity *pPhysicalEntity,
											u8 iVaultSpeed, 
											u8 iFlags,
											u16 iExitCondition,
                                            float fInitialDistance,
											float fRateMultiplier,
											float fHandHoldClimbAngle,
											bool bDisableVault,
											bool bClimbStandCondition,
											bool bCloneUseTestDirection,
											bool bCloneUsedSlide,
											u16 iPhysicalComponent,
											u8	uMinNumDepthTests)  
: m_vPedStartPosition(vPedStartPosition)
, m_vHandHoldPoint(vHandHoldPoint)
, m_vHandHoldNormal(vHandHoldNormal)
, m_vPedStartDirection(vPedStartDirection)
, m_iVaultSpeed(iVaultSpeed)
, m_iFlags(iFlags)
, m_iVaultExitCondition(iExitCondition)
, m_fInitialDistance(fInitialDistance)
, m_fRateMultiplier(fRateMultiplier)
, m_fHandHoldClimbAngle(fHandHoldClimbAngle)
, m_bDisableVault(bDisableVault)
, m_bPosIsOffset(false)
, m_bClimbStandCondition(bClimbStandCondition) 
, m_bCloneUseTestDirection(bCloneUseTestDirection) 
, m_bCloneUsedSlide(bCloneUsedSlide) 
, m_iPhysicalComponent(iPhysicalComponent)
, m_uMinNumDepthTests(uMinNumDepthTests)
{
	m_pPhysicalEntity.SetEntity(pPhysicalEntity);

	if(pPhysicalEntity && taskVerifyf(m_pPhysicalEntity.GetEntityID()!= NETWORK_INVALID_OBJECT_ID, "m_pPhysicalEntity should have a valid net ID") )
	{
		m_bPosIsOffset = true;
	}
	else
	{
		m_pPhysicalEntity.Invalidate();
	}

	SetStatusFromMainTaskState(state);
}

CClonedTaskVaultInfo::CClonedTaskVaultInfo() 
: m_vPedStartPosition(VEC3_ZERO)
, m_vHandHoldPoint(VEC3_ZERO)
, m_vHandHoldNormal(VEC3_ZERO)
, m_vPedStartDirection(VEC3_ZERO)
, m_pPhysicalEntity(NULL)
, m_iVaultSpeed(0)
, m_iFlags(0)
, m_iVaultExitCondition(CTaskVault::eVEC_Max)
, m_fRateMultiplier(1.0f)
, m_bDisableVault(false)
, m_bPosIsOffset(false)
, m_bClimbStandCondition(false) 
, m_bCloneUseTestDirection(false)
, m_bCloneUsedSlide(false)
, m_iPhysicalComponent(0)
, m_uMinNumDepthTests(0)
{
}


CTaskFSMClone * CClonedTaskVaultInfo::CreateCloneFSMTask()
{
	CTaskVault* pTaskVault = NULL;

	CClimbHandHoldDetected dummyhandHold; //the state will create this for the cloned ped

	pTaskVault = rage_new CTaskVault(dummyhandHold, m_iFlags);

	return pTaskVault;
}

