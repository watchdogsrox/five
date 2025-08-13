// File header
#include "Peds/PedMotionData.h"

#include "Debug/DebugScene.h"
#include "fwmaths/angle.h"
#include "Math/angmath.h"

AI_OPTIMISATIONS()
AI_MOTION_OPTIMISATIONS()

////////////////////////////////////////////////////////////////////////////////

const float MOVEBLENDRATIO_REVERSE			= -1.0f;
const float MOVEBLENDRATIO_STILL			= 0.0f;
const float MOVEBLENDRATIO_WALK				= 1.0f;
const float MOVEBLENDRATIO_RUN				= 2.0f;
const float MOVEBLENDRATIO_SPRINT			= 3.0f;
const float ONE_OVER_MOVEBLENDRATIO_SPRINT	= 1.0f / MOVEBLENDRATIO_SPRINT;

const float MBR_WALK_BOUNDARY				= 0.1f;
const float MBR_RUN_BOUNDARY				= 1.5f;
const float MBR_SPRINT_BOUNDARY				= 2.5f;

////////////////////////////////////////////////////////////////////////////////

#if __BANK
const char* CPedMotionData::GetMotionStateString(s32 iMotionStateHash)
{
	switch (iMotionStateHash)
	{
	case CPedMotionStates::MotionState_None : return "MotionState_None";
	case CPedMotionStates::MotionState_Idle : return "MotionState_Idle";
	case CPedMotionStates::MotionState_Walk : return "MotionState_Walk";
	case CPedMotionStates::MotionState_Run : return "MotionState_Run";
	case CPedMotionStates::MotionState_Sprint: return "MotionState_Sprint"; 
	case CPedMotionStates::MotionState_Crouch_Idle : return "MotionState_Crouch_Idle"; 
	case CPedMotionStates::MotionState_Crouch_Walk : return "MotionState_Crouch_Walk";
	case CPedMotionStates::MotionState_Crouch_Run : return "MotionState_Crouch_Run";
	case CPedMotionStates::MotionState_DoNothing : return "MotionState_DoNothing";
	case CPedMotionStates::MotionState_AnimatedVelocity : return "MotionState_AnimatedVelocity";
	case CPedMotionStates::MotionState_InVehicle : return "MotionState_InVehicle";
	case CPedMotionStates::MotionState_Aiming : return "MotionState_Aiming";
	case CPedMotionStates::MotionState_Diving_Idle : return "MotionState_Diving_Idle";
	case CPedMotionStates::MotionState_Diving_Swim : return "MotionState_Diving_Swim";
	case CPedMotionStates::MotionState_Swimming_TreadWater : return "MotionState_Swimming_TreadWater";
	case CPedMotionStates::MotionState_Dead : return "MotionState_Dead";
	case CPedMotionStates::MotionState_Stealth_Idle : return "MotionState_Stealth_Idle";
	case CPedMotionStates::MotionState_Stealth_Walk : return "MotionState_Stealth_Walk";
	case CPedMotionStates::MotionState_Stealth_Run : return "MotionState_Stealth_Run";
	case CPedMotionStates::MotionState_Parachuting : return "MotionState_Parachuting";
	case CPedMotionStates::MotionState_ActionMode_Idle : return "MotionState_ActionMode_Idle";
	case CPedMotionStates::MotionState_ActionMode_Walk : return "MotionState_ActionMode_Walk";
	case CPedMotionStates::MotionState_ActionMode_Run : return "MotionState_ActionMode_Run";
	case CPedMotionStates::MotionState_Jetpack : return "MotionState_Jetpack";
	default: break;
	}
	return "Unknown";
}
#endif // __BANK

CPedMotionData::CPedMotionData()
// We don't reset these variables on Reset, so init them
: m_fCurrentHeading(0.0f)
, m_fCurrentPitch(0.0f)
, m_fBoundPitch(0.0f)
, m_fBoundHeading(0.0f)
, m_vBoundOffset(Vector3::ZeroType)
, m_iCrouchStartTime(0)
, m_iCrouchDuration(-1)
, m_bWasCrouching(false)
, m_bUsingStealth(false)
{
	Reset();
}

void CPedMotionData::Reset()
{
	m_fDesiredHeading = m_fCurrentHeading;
	m_fHeadingChangeRate = DtoR * 360.0f * 2.5f;
	m_fDesiredPitch = m_fCurrentPitch;
	m_fDesiredBoundPitch = 0.0f;
	m_fDesiredBoundHeading = 0.0f;
	m_vDesiredBoundOffset = Vector3::ZeroType;
	m_fBoundPitch = 0.0f;
	m_fBoundHeading = 0.0f;
	m_vBoundOffset = Vector3::ZeroType;
	m_vCurrentMoveBlendRatio.Zero();
	m_vDesiredMoveBlendRatio.Zero();
	m_fScriptedMaxMoveBlendRatio = MOVEBLENDRATIO_SPRINT;
	m_fScriptedMinMoveBlendRatio = MOVEBLENDRATIO_REVERSE;
	m_fGaitReducedMaxMoveBlendRatio = MOVEBLENDRATIO_SPRINT;
	m_fCurrentTurnVelocity = 0.0f;
	m_fExtraHeadingChangeThisFrame = 0.0f;
	m_fExtraPitchChangeThisFrame = 0.0f;
	m_fIndependentMoverHeadingDelta = 0.0f;
	m_iStateForcedThisFrame = CPedMotionStates::MotionState_None;
	m_bUseExtractedZ = false;
	m_bPlayerHasControlOfPedThisFrame = false;
	m_bPlayerHadControlOfPedLastFrame = false;
	m_bForceSteepSlopeTestThisFrame = false;
	m_bStrafing = false;
	m_bUseRepositionMoveTarget = false;
	m_bFaceLeftInCover = false;
	m_uIndependentMoverTransition = 0;
	m_bDesiredHeadingSetThisFrame = false;
	m_bCombatRoll = false;
	m_iFlyingState = FLYING_INACTIVE;
	m_iFPSState = FPS_IDLE;
	m_iFPSPreviousState = m_iFPSState;
	m_bWasUsingStealthInFPS = false;
	m_bIsUsingStealthInFPS = false;
	m_bWasUsingFPSMode = false;
	m_bUsingFPSMode = false;
	m_bUsingAnalogStickRun = false;
	m_bFPSCheckRunInsteadOfSprint = false;
	m_fCurrentMoveRateOverride = 1.0f;
	m_fDesiredMoveRateOverride = 1.0f;
	m_fCurrentMoveRateInWaterOverride = 1.0f;
	m_fDesiredMoveRateInWaterOverride = 1.0f;
	m_StationaryAimPose.Clear();
	m_AimPoseTransition.Clear();
	m_fGaitReduction = 0;
	m_fGaitReductionHeading = 0;
	m_GaitReductionBlockTime = 0;
	m_iGaitReductionFlags = DEFAULT_GAIT_REDUCTION_FLAGS;
	m_vRepositionMoveTarget.Zero();
	m_overrideDriveByClipSet = CLIP_SET_ID_INVALID;
	m_overrideMotionInCoverClipSet = CLIP_SET_ID_INVALID;
	m_CustomIdleRequestClipSet = CLIP_SET_ID_INVALID;
	m_overrideFallUpperBodyClipSet = CLIP_SET_ID_INVALID;
	m_fSteerBias = 0.0f;
	m_fDesiredSteerBias = 0.0f;
}

void CPedMotionData::SetCurrentMoveBlendRatio(Vector2& vMoveBlendRatio, const float fMoveBlendRatio, const float fLateralMoveBlendRatio, const bool bIgnoreScriptedMBR)
{
	//if(Abs(fLateralMoveBlendRatio) > 0.0f)
	if (m_bStrafing)
	{
		vMoveBlendRatio.x = fLateralMoveBlendRatio;
		vMoveBlendRatio.y = fMoveBlendRatio;
		float mag = vMoveBlendRatio.Mag2();
		if(mag > ( Sign(m_fScriptedMaxMoveBlendRatio) * square(m_fScriptedMaxMoveBlendRatio) ))
		{
			vMoveBlendRatio.Normalize();
			vMoveBlendRatio *= m_fScriptedMaxMoveBlendRatio;
		}
		if(mag < ( Sign(m_fScriptedMinMoveBlendRatio) * square(m_fScriptedMinMoveBlendRatio) ))
		{
			vMoveBlendRatio.Normalize();
			vMoveBlendRatio *= m_fScriptedMinMoveBlendRatio;
		}
	}
	else
	{
		vMoveBlendRatio.x = 0.0f;
		if (!bIgnoreScriptedMBR)
		{
			vMoveBlendRatio.y = Clamp(fMoveBlendRatio, m_fScriptedMinMoveBlendRatio, m_fScriptedMaxMoveBlendRatio);
		}
	}
}

bool CPedMotionData::GetIsDesiredSprinting(const bool bTestRun) const
{
	const Vector2& vDesiredMBR = GetDesiredMoveBlendRatio();
	if(Abs(vDesiredMBR.x) > 0.01f)
	{
		return (!bTestRun) ? GetIsSprinting(vDesiredMBR.Mag()) : !GetIsLessThanRun(vDesiredMBR.Mag());
	}
	else
	{
		return (!bTestRun) ? GetIsSprinting(Abs(vDesiredMBR.y)) : !GetIsLessThanRun(Abs(vDesiredMBR.y));
	}
}

void CPedMotionData::SetCurrentFPSState(int iState)
{
	m_iFPSPreviousState = m_iFPSState;
	m_iFPSState = iState;
}

void CPedMotionData::SetPreviousFPSState(int iState)
{
	m_iFPSPreviousState = iState;
}

void CPedMotionData::SetIsUsingStealthInFPS(bool s)
{
	m_bWasUsingStealthInFPS = m_bIsUsingStealthInFPS;
	m_bIsUsingStealthInFPS = s;
}