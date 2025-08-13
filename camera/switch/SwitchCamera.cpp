//
// camera/switch/SwitchCamera.cpp
// 
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved.
//

#include "camera/switch/SwitchCamera.h"

#include "fwmaths/angle.h"
#include "fwsys/timer.h"

#include "camera/CamInterface.h"
#include "camera/gameplay/GameplayDirector.h"
#include "camera/helpers/ControlHelper.h"
#include "camera/helpers/FrameInterpolator.h"
#include "camera/system/CameraManager.h"
#include "camera/system/CameraMetadata.h"
#include "peds/ped.h"
#include "renderer/PostProcessFX.h"
#include "scene/playerswitch/PlayerSwitchInterface.h"
#include "scene/world/GameWorldHeightMap.h"

CAMERA_OPTIMISATIONS()

INSTANTIATE_RTTI_CLASS(camSwitchCamera,0xDECCDF37)

PF_PAGE(camSwitchCameraPage, "Camera: Switch Camera");

PF_GROUP(camSwitchCameraMetrics);
PF_LINK(camSwitchCameraPage, camSwitchCameraMetrics);

PF_VALUE_FLOAT(switchCameraPhase, camSwitchCameraMetrics);
PF_VALUE_FLOAT(switchCameraBlendLevel, camSwitchCameraMetrics);

#if __BANK
enum eSwitchOrientationOptions
{
	SOO_DEFAULT = 0,
	SOO_USE_START_HEADING,
	SOO_USE_FINAL_HEADING,
	SOO_USE_MAP_NORTH,
	SOO_USE_TRAVEL_DIRECTION_AS_UP,
	SOO_USE_TRAVEL_DIRECTION_AS_RIGHT,
	SOO_ROTATE_ON_ASCENT_ONLY,
	SOO_ROTATE_ON_DESCENT_ONLY,
	SOO_ROTATE_ASCENT_AND_DESCENT,
	SOO_MAX
};

static const char* g_DebugSwitchOrientationOptions[SOO_MAX] =
{
	"DEFAULT",
	"USE INITIAL HEADING",
	"USE FINAL HEADING",
	"USE MAP NORTH",
	"CAMERA UP IS TRAVEL DIRECTION",
	"CAMERA RIGHT IS TRAVEL DIRECTION",
	"ROTATE ON ASCENT ONLY",
	"ROTATE ON DESCENT ONLY",
	"ROTATE DURING ASCENT AND DESCENT"
};

s32 camSwitchCamera::ms_DebugOrientationOption = (s32)SOO_DEFAULT;
#endif // __BANK

camSwitchCamera::camSwitchCamera(const camBaseObjectMetadata& metadata)
: camBaseCamera(metadata)
, m_Metadata(static_cast<const camSwitchCameraMetadata&>(metadata))
, m_StartPosition(g_InvalidPosition)
, m_EndPosition(g_InvalidPosition)
, m_EstablishingShotMetadata(NULL)
, m_State(INACTIVE)
, m_NumAscentCuts(1)
, m_NumDescentCuts(1)
, m_StateDuration(0)
, m_StateStartTime(0)
, m_InterpolateOutDurationToApply(0)
, m_ControlFlags(0)
, m_StartHeading(0.0f)
, m_EndHeading(0.0f)
, m_AscentFloorHeight(0.0f)
, m_DescentFloorHeight(0.0f)
, m_CeilingHeight(0.0f)
, m_InitialFovForPan(m_Metadata.m_Fov)
, m_InitialMotionBlurMaxVelocityScale(0.0f)
, m_IsInitialised(false)
, m_ShouldApplyCutBehaviour(false)
, m_ShouldBlendInCutBehaviour(false)
, m_HasBehaviourFinished(false)
, m_WasSourcePedInVehicle(false)
, m_IsUsingFirstPersonEstablishShot(false)
, m_ShouldReportAsFinishedForFPSCutback(false)
{
}

camSwitchCamera::~camSwitchCamera()
{
	//Ensure any intro gameplay behaviour is stopped.
	//TODO: We would ideally manage the gameplay Switch behaviour within the player Switch manager, rather than here.
	camGameplayDirector* gameplayDirector = camInterface::GetGameplayDirectorSafe();
	if(gameplayDirector)
	{
		gameplayDirector->StopSwitchBehaviour();
	}
}

void camSwitchCamera::Init(const CPed& sourcePed, const CPed& destinationPed, const CPlayerSwitchParams& switchParams)
{
	const fwTransform& destinationPedTransform = destinationPed.GetTransform();

	const bool bStartFromCameraPosRatherThanPed = ((switchParams.m_controlFlags & CPlayerSwitchInterface::CONTROL_FLAG_START_FROM_CAMPOS) != 0);

	m_StartPosition			= bStartFromCameraPosRatherThanPed ?  camInterface::GetPos() : VEC3V_TO_VECTOR3(sourcePed.GetTransform().GetPosition());
	m_StartHeading			= ComputeStartHeading();
	m_EndPosition			= VEC3V_TO_VECTOR3(destinationPedTransform.GetPosition());
	m_EndHeading			= destinationPedTransform.GetHeading();
	m_ControlFlags			= switchParams.m_controlFlags;
	m_AscentFloorHeight		= switchParams.m_fLowestHeightAscent;
	m_DescentFloorHeight	= switchParams.m_fLowestHeightDescent;
	m_CeilingHeight			= switchParams.m_fCeilingHeight;
	m_NumAscentCuts			= cameraVerifyf(switchParams.m_numJumpsAscent > 0, "A Switch must include at least one ascent cut. Defaulting to one") ?
								switchParams.m_numJumpsAscent : 1;
	m_NumDescentCuts		= cameraVerifyf(switchParams.m_numJumpsDescent > 0, "A Switch must include at least one descent cut. Defaulting to one") ?
								switchParams.m_numJumpsDescent : 1;
	m_IsInitialised			= true;

	m_InterpolateOutDurationToApply = 0;

	m_HasBehaviourFinished	= true;

	m_WasSourcePedInVehicle	= sourcePed.GetIsInVehicle();
}

void camSwitchCamera::OverrideDestination(const Matrix34& worldMatrix)
{
	if(m_EstablishingShotMetadata)
	{
		//We already have an establishing shot, so we must maintain the associated end position and heading.
		return;
	}

	m_EndPosition	= worldMatrix.d;
	m_EndHeading	= camFrame::ComputeHeadingFromMatrix(worldMatrix);

	//Validate that the descent floor height is still safe for the revised end position.
	//TODO: Remove this once the player Switch manager recomputes the Switch parameters and reapplies them to this camera.
	const float maxWorldHeightAtEndPosition	= CGameWorldHeightMap::GetMaxHeightFromWorldHeightMap(
												m_EndPosition.x - m_Metadata.m_HalfBoxExtentForHeightMapQuery,
												m_EndPosition.y - m_Metadata.m_HalfBoxExtentForHeightMapQuery,
												m_EndPosition.x + m_Metadata.m_HalfBoxExtentForHeightMapQuery,
												m_EndPosition.y + m_Metadata.m_HalfBoxExtentForHeightMapQuery);
	const float minSafeCutHeight			= maxWorldHeightAtEndPosition + m_Metadata.m_MinDistanceAboveHeightMapForFirstCut;
	m_DescentFloorHeight					= Max(m_DescentFloorHeight, minSafeCutHeight);
}

void camSwitchCamera::SetEstablishingShotMetadata(const CPlayerSwitchEstablishingShotMetadata* metadata)
{
	m_EstablishingShotMetadata = metadata;
	if(!m_EstablishingShotMetadata)
	{
		return;
	}

	//Apply any alternate shot specified for first-person on-foot, as required.
	const atHashString firstPersonShotName = m_EstablishingShotMetadata->m_OverriddenShotForFirstPersonOnFoot;
	if(firstPersonShotName.IsNotNull())
	{
		const s32 onFootViewMode = camControlHelper::GetViewModeForContext(camControlHelperMetadataViewMode::ON_FOOT);
		if(onFootViewMode == camControlHelperMetadataViewMode::FIRST_PERSON)
		{
			const CPlayerSwitchEstablishingShotMetadata* firstPersonShotMetadata = CPlayerSwitchEstablishingShot::FindShotMetaData(firstPersonShotName);
			if(cameraVerifyf(firstPersonShotMetadata, "An overridden Switch establishing shot for first-person could not be found (name = %s, hash = %u)",
				(firstPersonShotName.TryGetCStr() ? firstPersonShotName.TryGetCStr() : "Unknown"), firstPersonShotName.GetHash()))
			{
				m_EstablishingShotMetadata = firstPersonShotMetadata;
				m_IsUsingFirstPersonEstablishShot = true; 
			}
		}
	}

	//We need to descend to the establishing shot, so override the end position and heading. 

	m_EndPosition	= m_EstablishingShotMetadata->m_Position;
	m_EndHeading	= m_EstablishingShotMetadata->m_Orientation.z * DtoR;

	//Validate that the descent floor height is still safe for the revised end position.
	//TODO: Remove this once the player Switch manager recomputes the Switch parameters and reapplies them to this camera.
	const float maxWorldHeightAtEndPosition	= CGameWorldHeightMap::GetMaxHeightFromWorldHeightMap(
												m_EndPosition.x - m_Metadata.m_HalfBoxExtentForHeightMapQuery,
												m_EndPosition.y - m_Metadata.m_HalfBoxExtentForHeightMapQuery,
												m_EndPosition.x + m_Metadata.m_HalfBoxExtentForHeightMapQuery,
												m_EndPosition.y + m_Metadata.m_HalfBoxExtentForHeightMapQuery);
	const float minSafeCutHeight			= maxWorldHeightAtEndPosition + m_Metadata.m_MinDistanceAboveHeightMapForFirstCut;
	m_DescentFloorHeight					= Max(m_DescentFloorHeight, minSafeCutHeight);

	//NOTE: We must skip the outro states when interpolating out of the establishing shot.
	if(m_EstablishingShotMetadata->m_InterpolateOutDuration > 0)
	{
		m_ControlFlags |= CPlayerSwitchInterface::CONTROL_FLAG_SKIP_OUTRO;
	}
}

bool camSwitchCamera::PerformIntro()
{
	if(!cameraVerifyf(m_IsInitialised, "A Switch camera has been requested to Start when it has not been initialised"))
	{
		m_State = INACTIVE;

		return false;
	}

	if(m_ControlFlags & CPlayerSwitchInterface::CONTROL_FLAG_SKIP_INTRO)
	{
		m_HasBehaviourFinished = true;
	}
	else
	{
		m_HasBehaviourFinished = false;

		//TODO: We would ideally manage the gameplay Switch behaviour within the player Switch manager, rather than here.
		const s32 switchType = g_PlayerSwitch.GetSwitchType();
		camInterface::GetGameplayDirector().StartSwitchBehaviour(switchType, true);
	}

	m_State = INTRO;

	return true;
}

void camSwitchCamera::PerformAscentCut(u32 cutIndex)
{
	//Ensure any intro gameplay behaviour is stopped.
	//TODO: We would ideally manage the gameplay Switch behaviour within the player Switch manager, rather than here.
	camInterface::GetGameplayDirector().StopSwitchBehaviour();

	const u32 cutIndexToApply	= cameraVerifyf(cutIndex < m_NumAscentCuts, "Invalid ascent cut index (%u, %u cuts)",
									cutIndex, m_NumAscentCuts) ? cutIndex : (m_NumAscentCuts - 1);

	camFrame cutFrame;
	ComputeFrameForAscentCut(cutIndexToApply, cutFrame);

	m_Frame.CloneFrom(cutFrame);

	m_StateDuration				= m_Metadata.m_CutEffectDuration;
	m_StateStartTime			= fwTimer::GetTimeInMilliseconds_NonScaledClipped();
	m_State						= ASCENT_CUT;
	m_ShouldApplyCutBehaviour	= true;
	m_ShouldBlendInCutBehaviour	= false;
	m_HasBehaviourFinished		= false;
}

void camSwitchCamera::ComputeFrameForAscentCut(u32 cutIndex, camFrame& frame) const
{
	float desiredHeading = (m_WasSourcePedInVehicle || (m_ControlFlags & CPlayerSwitchInterface::CONTROL_FLAG_UNKNOWN_DEST)) ? m_StartHeading : m_EndHeading;

#if __BANK
	DebugOverrideCameraHeading(desiredHeading, cutIndex, true);
#endif // __BANK

	ComputeBaseFrame(frame, desiredHeading);

	Vector3 cutPosition;
	ComputeAscentCutPosition(cutIndex, cutPosition);

	frame.SetPosition(cutPosition);

	frame.GetFlags().SetFlag(camFrame::Flag_HasCutPosition);
}

void camSwitchCamera::PerformLookUp()
{
	m_StateDuration			= m_Metadata.m_LookUpDuration;
	m_StateStartTime		= fwTimer::GetTimeInMilliseconds_NonScaledClipped();
	m_State					= LOOK_UP;
	m_HasBehaviourFinished	= false;
}

void camSwitchCamera::PerformLookDown()
{
	m_StateDuration			= m_Metadata.m_LookDownDuration;
	m_StateStartTime		= fwTimer::GetTimeInMilliseconds_NonScaledClipped();
	m_State					= LOOK_DOWN;
	m_HasBehaviourFinished	= false;
}

void camSwitchCamera::PerformPan(u32 duration)
{
	m_StateDuration			= duration;
	m_StateStartTime		= fwTimer::GetTimeInMilliseconds_NonScaledClipped();
	m_InitialFovForPan		= m_Frame.GetFov();
	m_State					= PAN;
	m_HasBehaviourFinished	= false;

	m_InitialMotionBlurMaxVelocityScale = PostFX::GetMotionBlurMaxVelocityScale();
}

void camSwitchCamera::PerformDescentCut(u32 cutIndex)
{
	const u32 cutIndexToApply	= cameraVerifyf(cutIndex < m_NumDescentCuts, "Invalid descent cut index (%u, %u cuts)",
									cutIndex, m_NumDescentCuts) ? cutIndex : (m_NumDescentCuts - 1);

	camFrame cutFrame;
	ComputeFrameForDescentCut(cutIndexToApply, cutFrame);

	m_Frame.CloneFrom(cutFrame);

	m_StateDuration				= m_Metadata.m_CutEffectDuration;
	m_StateStartTime			= fwTimer::GetTimeInMilliseconds_NonScaledClipped();
	m_State						= DESCENT_CUT;
	m_ShouldApplyCutBehaviour	= true;
	//NOTE: We blend in the cut behaviour on the topmost descent cut in order to smooth the transition from the pan.
	m_ShouldBlendInCutBehaviour	= (cutIndex == (m_NumDescentCuts - 1));
	m_HasBehaviourFinished		= false;
}

void camSwitchCamera::ComputeFrameForDescentCut(u32 cutIndex, camFrame& frame) const
{
	float desiredHeading = (m_WasSourcePedInVehicle || (m_ControlFlags & CPlayerSwitchInterface::CONTROL_FLAG_UNKNOWN_DEST)) ? m_StartHeading : m_EndHeading;

#if __BANK
	DebugOverrideCameraHeading(desiredHeading, cutIndex, false);
#endif // __BANK

	ComputeBaseFrame(frame, desiredHeading);

	Vector3 cutPosition;
	ComputeDescentCutPosition(cutIndex, cutPosition);

	frame.SetPosition(cutPosition);

	frame.GetFlags().SetFlag(camFrame::Flag_HasCutPosition);
}

void camSwitchCamera::PerformEstablishingShot()
{
	m_State = ESTABLISHING_SHOT_HOLD;

	if(cameraVerifyf(m_EstablishingShotMetadata, "The Switch camera has no establishing shot metadata with which to compose a frame"))
	{
		camFrame establishingShotFrame;
		ComputeBaseFrameForEstablishingShot(*m_EstablishingShotMetadata, establishingShotFrame);

		Matrix34 startWorldMatrix;
		ComputeWorldMatrixForEstablishingShotStart(*m_EstablishingShotMetadata, startWorldMatrix);

		establishingShotFrame.SetWorldMatrix(startWorldMatrix, false);

		float fov	= establishingShotFrame.GetFov();
		fov			*= m_Metadata.m_MaxFovScalingForEstablishingShotInHold;
		establishingShotFrame.SetFov(fov);

		establishingShotFrame.GetFlags().SetFlag(camFrame::Flag_HasCutPosition);

		m_Frame.CloneFrom(establishingShotFrame);

		m_StateDuration			= m_Metadata.m_EstablishingShotInHoldDuration;
		m_StateStartTime		= fwTimer::GetTimeInMilliseconds_NonScaledClipped();
		m_HasBehaviourFinished	= false;
	}
	else
	{
		m_HasBehaviourFinished	= true;
	}
}

bool camSwitchCamera::ComputeFrameForEstablishingShotStart(camFrame& frame) const
{
	if(!cameraVerifyf(m_EstablishingShotMetadata, "The Switch camera has no establishing shot metadata with which to compose a frame"))
	{
		return false;
	}

	ComputeBaseFrameForEstablishingShot(*m_EstablishingShotMetadata, frame);

	Matrix34 worldMatrix;
	ComputeWorldMatrixForEstablishingShotStart(*m_EstablishingShotMetadata, worldMatrix);

	frame.SetWorldMatrix(worldMatrix, false);

	return true;
}

bool camSwitchCamera::ComputeFrameForEstablishingShotEnd(camFrame& frame) const
{
	if(!cameraVerifyf(m_EstablishingShotMetadata, "The Switch camera has no establishing shot metadata with which to compose a frame"))
	{
		return false;
	}

	ComputeBaseFrameForEstablishingShot(*m_EstablishingShotMetadata, frame);

	Matrix34 worldMatrix;
	ComputeWorldMatrixForEstablishingShotEnd(*m_EstablishingShotMetadata, worldMatrix);

	frame.SetWorldMatrix(worldMatrix, false);

	return true;
}

void camSwitchCamera::PerformOutroHold()
{
	if(m_ControlFlags & CPlayerSwitchInterface::CONTROL_FLAG_SKIP_OUTRO)
	{
		if(m_IsUsingFirstPersonEstablishShot)
		{
			camManager::TriggerFirstPersonFlashEffect(camManager::CAM_PUSH_IN_FX_NEUTRAL, 0, "Switch Fall back");
			m_HasBehaviourFinished = false; 
			m_StateStartTime	= fwTimer::GetTimeInMilliseconds_NonScaledClipped();
		}
		else
		{
			m_HasBehaviourFinished = true;
		}
	}
	else
	{
		if(m_IsUsingFirstPersonEstablishShot)
		{
			camManager::TriggerFirstPersonFlashEffect(camManager::CAM_PUSH_IN_FX_NEUTRAL, 0, "Switch Fall back");
			m_HasBehaviourFinished = false; 
			m_StateStartTime	= fwTimer::GetTimeInMilliseconds_NonScaledClipped();
		}
		else
		{
			//Ensure that any shake is cleaned up immediately.
			StopShaking();

			m_HasBehaviourFinished = false;

			//TODO: We would ideally manage the gameplay Switch behaviour within the player Switch manager, rather than here.
			camGameplayDirector& gameplayDirector	= camInterface::GetGameplayDirector();
			const s32 switchType					= g_PlayerSwitch.GetSwitchType();
			gameplayDirector.StartSwitchBehaviour(switchType, false);
			gameplayDirector.SetSwitchBehaviourPauseState(true);

			m_StateDuration		= m_Metadata.m_OutroHoldDuration;
			m_StateStartTime	= fwTimer::GetTimeInMilliseconds_NonScaledClipped();
		}
	}

	m_State = OUTRO_HOLD;
}

void camSwitchCamera::PerformOutroSwoop()
{
	//Ensure that any shake is cleaned up immediately.
	StopShaking();

	if(m_ControlFlags & CPlayerSwitchInterface::CONTROL_FLAG_SKIP_OUTRO)
	{
		m_HasBehaviourFinished = true;
	}
	else
	{
		m_HasBehaviourFinished = false;

		//TODO: We would ideally manage the gameplay Switch behaviour within the player Switch manager, rather than here.
		camInterface::GetGameplayDirector().SetSwitchBehaviourPauseState(false);
	}

	m_State = OUTRO_SWOOP;
}

bool camSwitchCamera::Update()
{
	bool hasSucceeded = false;

	switch(m_State)
	{
	case INACTIVE:
		break;

	case INTRO:
		hasSucceeded = UpdateIntro();
		break;

	case ASCENT_CUT:
		hasSucceeded = UpdateAscentCut();
		break;

	case LOOK_UP:
		hasSucceeded = UpdateLookUp();
		break;

	case LOOK_DOWN:
		hasSucceeded = UpdateLookDown();
		break;

	case PAN:
		hasSucceeded = UpdatePan();
		break;

	case DESCENT_CUT:
		hasSucceeded = UpdateDescentCut();
		break;

	case ESTABLISHING_SHOT_HOLD:
		hasSucceeded = UpdateEstablishingShotHold();
		break;

	case ESTABLISHING_SHOT:
		hasSucceeded = UpdateEstablishingShot();
		break;

	case OUTRO_HOLD:
		hasSucceeded = UpdateOutroHold();
		break;

	case OUTRO_SWOOP:
		hasSucceeded = UpdateOutroSwoop();
		break;
	}

	if(m_ShouldReportAsFinishedForFPSCutback)
	{
		camInterface::GetGameplayDirector().SetSwitchCameraHasTerminated(true);
	}

	return hasSucceeded;
}

bool camSwitchCamera::UpdateIntro()
{
	//TODO: We would ideally manage the gameplay Switch behaviour within the player Switch manager, rather than here.
	m_HasBehaviourFinished = camInterface::GetGameplayDirector().IsSwitchBehaviourFinished();

	//Don't render during the intro, as this is handled by the gameplay director.
	return false;
}

bool camSwitchCamera::UpdateAscentCut()
{
	//Report completion immediately, as we do not need to wait for the dynamic cut effect.
	m_HasBehaviourFinished = true;

	if(m_ShouldApplyCutBehaviour)
	{
		const float phase		= ComputeStatePhase(m_StateDuration);
		const float initialFov	= m_Metadata.m_MaxFovScalingForCutEffect * m_Metadata.m_Fov;
		const float blendLevel	= m_ShouldBlendInCutBehaviour ? SlowInOut(phase) : SlowOut(phase);
		const float fovToApply	= Lerp(blendLevel, initialFov, m_Metadata.m_Fov);

		m_Frame.SetFov(fovToApply);

		PF_SET(switchCameraPhase, phase);
		PF_SET(switchCameraBlendLevel, blendLevel);
	}

	return true;
}

bool camSwitchCamera::UpdateLookUp()
{
	const float phase = ComputeStatePhase(m_StateDuration);

	float heading;
	float pitch;
	float roll;
	m_Frame.ComputeHeadingPitchAndRoll(heading, pitch, roll);

	const float phaseToApply	= SlowInOut(phase);
	pitch						= Lerp(phaseToApply, m_Metadata.m_DefaultPitch, m_Metadata.m_DesiredPitchWhenLookingUp);
	pitch						*= DtoR;

	m_Frame.SetWorldMatrixFromHeadingPitchAndRoll(heading, pitch, roll);

	m_HasBehaviourFinished = (phase >= (1.0f - SMALL_FLOAT));

	return true;
}

bool camSwitchCamera::UpdateLookDown()
{
	const float phase = ComputeStatePhase(m_StateDuration);

	float heading;
	float pitch;
	float roll;
	m_Frame.ComputeHeadingPitchAndRoll(heading, pitch, roll);

	const float phaseToApply	= SlowInOut(phase);
	pitch						= Lerp(phaseToApply, m_Metadata.m_DesiredPitchWhenLookingUp, m_Metadata.m_DefaultPitch);
	pitch						*= DtoR;

	m_Frame.SetWorldMatrixFromHeadingPitchAndRoll(heading, pitch, roll);

	m_HasBehaviourFinished = (phase >= (1.0f - SMALL_FLOAT));

	return true;
}

bool camSwitchCamera::UpdatePan()
{
	float desiredHeading = (m_WasSourcePedInVehicle || (m_ControlFlags & CPlayerSwitchInterface::CONTROL_FLAG_UNKNOWN_DEST)) ? m_StartHeading : m_EndHeading;

#if __BANK
	DebugOverrideCameraHeading(desiredHeading, (m_NumAscentCuts - 1), true);
#endif // __BANK

	camFrame panFrame;
	ComputeBaseFrame(panFrame, desiredHeading);

	panFrame.SetMotionBlurStrength(m_Metadata.m_MotionBlurStrengthForPan);

	const float phase		= ComputeStatePhase(m_StateDuration);
	const float blendLevel	= SlowInOut(SlowInOut(phase));

	PF_SET(switchCameraPhase, phase);
	PF_SET(switchCameraBlendLevel, blendLevel);

	Vector3 panStartPosition;
	ComputeAscentCutPosition(m_NumAscentCuts - 1, panStartPosition);

	Vector3 panEndPosition;
	ComputeDescentCutPosition(m_NumDescentCuts - 1, panEndPosition);

	Vector3 panPosition;
	panPosition.Lerp(blendLevel, panStartPosition, panEndPosition);

	panFrame.SetPosition(panPosition);

	//Interpolate to the default FOV across the pan, so that we may apply the full FOV change again at the highest descent position.
	const float fovToApply = Lerp(blendLevel, m_InitialFovForPan, m_Metadata.m_Fov);
	panFrame.SetFov(fovToApply);

	m_Frame.CloneFrom(panFrame);

	if(phase >= (1.0f - SMALL_FLOAT))
	{
		//Restore the initial motion blur settings.
		PostFX::SetMotionBlurMaxVelocityScale(m_InitialMotionBlurMaxVelocityScale);

		m_HasBehaviourFinished = true;
	}
	else
	{
		//Request custom motion blur settings.
		PostFX::SetMotionBlurMaxVelocityScale(m_Metadata.m_MotionBlurMaxVelocityScaleForPan);
	}

	return true;
}

bool camSwitchCamera::UpdateDescentCut()
{
	//Report completion immediately, as we do not need to wait for the dynamic cut effect.
	m_HasBehaviourFinished = true;

	if(m_ShouldApplyCutBehaviour)
	{
		const float phase		= ComputeStatePhase(m_StateDuration);
		const float targetFov	= m_Metadata.m_MaxFovScalingForCutEffect * m_Metadata.m_Fov;
		const float blendLevel	= m_ShouldBlendInCutBehaviour ? SlowInOut(phase) : SlowOut(phase);
		const float fovToApply	= Lerp(blendLevel, m_Metadata.m_Fov, targetFov);

		m_Frame.SetFov(fovToApply);

		PF_SET(switchCameraPhase, phase);
		PF_SET(switchCameraBlendLevel, blendLevel);
	}

	return true;
}

bool camSwitchCamera::UpdateEstablishingShotHold()
{
	if(!cameraVerifyf(m_EstablishingShotMetadata, "The Switch camera has no establishing shot metadata with which to compose a frame"))
	{
		m_HasBehaviourFinished = true;

		return true;
	}

	//Release any existing camera shake.
	StopShaking(false);

	camFrame establishingShotFrame;
	ComputeBaseFrameForEstablishingShot(*m_EstablishingShotMetadata, establishingShotFrame);

	const float phase		= ComputeStatePhase(m_StateDuration);
	const float targetFov	= establishingShotFrame.GetFov();
	const float initialFov	= m_Metadata.m_MaxFovScalingForEstablishingShotInHold * targetFov;
	const float fovToApply	= Lerp(SlowIn(phase), initialFov, targetFov);

	m_Frame.SetFov(fovToApply);

	const bool isHoldFinished = (phase >= (1.0f - SMALL_FLOAT));
	if(isHoldFinished)
	{
		//Transition to the establishing shot state, rather than reporting that the current behaviour has finished.

		//NOTE: We inhibit the shake if we are using a catch-up exit, otherwise we may observe a pop when catch-up is triggered.
		if(!m_EstablishingShotMetadata->m_ShouldIntepolateToCatchUp && m_Metadata.m_EstablishingShotShakeRef)
		{
			Shake(m_Metadata.m_EstablishingShotShakeRef, m_Metadata.m_EstablishingShotShakeAmplitude);
		}

		m_State				= ESTABLISHING_SHOT;
		m_StateDuration		= m_Metadata.m_EstablishingShotSwoopDuration + m_EstablishingShotMetadata->m_HoldDuration +
								m_EstablishingShotMetadata->m_InterpolateOutDuration;
		m_StateStartTime	= fwTimer::GetTimeInMilliseconds_NonScaledClipped();
	}

	return true;
}

bool camSwitchCamera::UpdateEstablishingShot()
{
	if(!cameraVerifyf(m_EstablishingShotMetadata, "The Switch camera has no establishing shot metadata with which to compose a frame"))
	{
		m_HasBehaviourFinished = true;

		return true;
	}

	camGameplayDirector& gameplayDirector = camInterface::GetGameplayDirector();

	if(m_EstablishingShotMetadata->m_ShouldInhibitFirstPersonOnFoot)
	{
		gameplayDirector.DisableFirstPersonThisUpdate();
	}

	const float statePhase	= ComputeStatePhase(m_StateDuration);
	m_HasBehaviourFinished	= (statePhase >= (1.0f - SMALL_FLOAT));

	const bool shouldInterpolateOut = (m_EstablishingShotMetadata->m_InterpolateOutDuration > 0);

 	const float swoopPhase					= ComputeStatePhase(m_Metadata.m_EstablishingShotSwoopDuration);
	const float swoopOrientationBlendLevel	= shouldInterpolateOut ? SlowOut(SlowInOut(swoopPhase)) : SlowInOut(swoopPhase);
	const float swoopPositionBlendLevel		= shouldInterpolateOut ? SlowOut(SlowOut(swoopPhase)) : SlowOut(swoopPhase);

	camFrame establishingShotFrame;
	ComputeBaseFrameForEstablishingShot(*m_EstablishingShotMetadata, establishingShotFrame);

	Matrix34 startWorldMatrix;
	ComputeWorldMatrixForEstablishingShotStart(*m_EstablishingShotMetadata, startWorldMatrix);
	Matrix34 endWorldMatrix;
	ComputeWorldMatrixForEstablishingShotEnd(*m_EstablishingShotMetadata, endWorldMatrix);

	Matrix34 blendedWorldMatrix;
	camFrame::SlerpOrientation(swoopOrientationBlendLevel, startWorldMatrix, endWorldMatrix, blendedWorldMatrix);

	blendedWorldMatrix.d.Lerp(swoopPositionBlendLevel, startWorldMatrix.d, endWorldMatrix.d);

	establishingShotFrame.SetWorldMatrix(blendedWorldMatrix, false);

	m_Frame.CloneFrom(establishingShotFrame);

	if(shouldInterpolateOut)
	{
		const float swoopAndHoldPhase = ComputeStatePhase(m_Metadata.m_EstablishingShotSwoopDuration + m_EstablishingShotMetadata->m_HoldDuration);
		if(swoopAndHoldPhase > (1.0f - SMALL_FLOAT))
		{
 			if(m_InterpolateOutDurationToApply == 0)
			{
				if(m_EstablishingShotMetadata->m_ShouldIntepolateToCatchUp)
				{
					//Clone the active gameplay frame and override the position, orientation and FOV.
					const camFrame& gameplayFrame = gameplayDirector.GetFrameNoPostEffects();

					camFrame catchUpFrame;
					catchUpFrame.CloneFrom(gameplayFrame);

					Matrix34 catchUpWorldMatrix;
					ComputeWorldMatrixForEstablishingShotCatchUp(*m_EstablishingShotMetadata, catchUpWorldMatrix);

					catchUpFrame.SetWorldMatrix(catchUpWorldMatrix, false);
					catchUpFrame.SetFov(m_EstablishingShotMetadata->m_CatchUpFov);

					gameplayDirector.CatchUpFromFrame(catchUpFrame);
				}
				else
				{
					float desiredRelativeHeading	= 0.0f;
					float desiredRelativePitch		= 0.0f;

					if(m_EstablishingShotMetadata->m_ShouldApplyGameplayCameraOrientation)
					{
						//Apply the custom gameplay camera orientation settings specified for the establishing shot.
						desiredRelativeHeading	= m_EstablishingShotMetadata->m_GameplayRelativeHeading * DtoR;
						desiredRelativePitch	= m_EstablishingShotMetadata->m_GameplayRelativePitch * DtoR;
					}
					else
					{
						//Align the gameplay camera roughly with the establishing shot.
						const CPed* newPed = g_PlayerSwitch.GetLongRangeMgr().GetNewPed();
						if(newPed)
						{
							const CVehicle* vehicle				= newPed->GetVehiclePedInside();
							const Matrix34 attachParentMatrix	= MAT34V_TO_MATRIX34(vehicle ? vehicle->GetMatrix() : newPed->GetMatrix());

							const Vector3 shotToAttachParent	= attachParentMatrix.d - endWorldMatrix.d;
							const float distanceToAttachParent	= shotToAttachParent.Mag();
							if(distanceToAttachParent >= SMALL_FLOAT)
							{
								Vector3 front;
								front.InvScale(shotToAttachParent, distanceToAttachParent);

								attachParentMatrix.UnTransform3x3(front);

								desiredRelativeHeading = camFrame::ComputeHeadingFromFront(front);
							}
						}
					}

					gameplayDirector.SetUseScriptHeading(desiredRelativeHeading);
					gameplayDirector.SetUseScriptPitch(desiredRelativePitch, 1.0f);
				}
			}

			m_InterpolateOutDurationToApply = m_EstablishingShotMetadata->m_InterpolateOutDuration;
		}
	}

	return true;
}

bool camSwitchCamera::UpdateOutroHold()
{
	if(m_ControlFlags & CPlayerSwitchInterface::CONTROL_FLAG_SKIP_OUTRO)
	{
		if(m_IsUsingFirstPersonEstablishShot)
		{
			const float statePhase	= ComputeStatePhase(300);
			m_HasBehaviourFinished	= (statePhase >= (1.0f - SMALL_FLOAT));

			camGameplayDirector& gameplayDirector	= camInterface::GetGameplayDirector();

			if(m_EstablishingShotMetadata->m_ShouldInhibitFirstPersonOnFoot && !m_HasBehaviourFinished)
			{
				gameplayDirector.DisableFirstPersonThisUpdate();
			}

			if(m_HasBehaviourFinished && m_IsUsingFirstPersonEstablishShot)
			{
				m_ShouldReportAsFinishedForFPSCutback = true; 
			}
		}
		else
		{
			m_HasBehaviourFinished = true;
		}
		
		//Continue to render the current frame if we're skipping the outro, so that we don't risk seeing a frame of gameplay
		//before cutting to something else.
		return true;
	}
	else
	{
		//Wait the predefined hold time.
		const float phase		= ComputeStatePhase(m_StateDuration);
		m_HasBehaviourFinished	= (phase >= (1.0f - SMALL_FLOAT));

		//Don't render during the outro, as this is handled by another director.
		return false;
	}
}

bool camSwitchCamera::UpdateOutroSwoop()
{
	//TODO: We would ideally manage the gameplay Switch behaviour within the player Switch manager, rather than here.
	m_HasBehaviourFinished = camInterface::GetGameplayDirector().IsSwitchBehaviourFinished();

	//Don't render during the outro, as this is handled by another director.
	return false;
}

float camSwitchCamera::ComputeStartHeading() const
{
	const camFrame& gameplayFrame		= camInterface::GetGameplayDirector().GetFrame();
	const Matrix34& gameplayWorldMatrix	= gameplayFrame.GetWorldMatrix();

	float heading;
	float pitch;
	float roll;
	camFrame::ComputeHeadingPitchAndRollFromMatrix(gameplayWorldMatrix, heading, pitch, roll);

	//Take account of any roll.
	float startHeading	= heading + roll;
	startHeading		= fwAngle::LimitRadianAngle(startHeading);

	return startHeading;
}

void camSwitchCamera::ComputeAscentCutPosition(u32 cutIndex, Vector3& cutPosition) const
{
	cutPosition.Set(m_StartPosition);

	const float phase	= (m_NumAscentCuts > 1) ? (static_cast<float>(cutIndex) / static_cast<float>(m_NumAscentCuts - 1)) : 1.0f;
	cutPosition.z		= Lerp(phase, m_AscentFloorHeight, m_CeilingHeight);
}

void camSwitchCamera::ComputeDescentCutPosition(u32 cutIndex, Vector3& cutPosition) const
{
	cutPosition.Set(m_EndPosition);

	const float phase	= (m_NumDescentCuts > 1) ? (static_cast<float>(cutIndex) / static_cast<float>(m_NumDescentCuts - 1)) : 1.0f;
	cutPosition.z		= Lerp(phase, m_DescentFloorHeight, m_CeilingHeight);
}

float camSwitchCamera::ComputeStatePhase(u32 duration) const
{
	if(duration == 0)
	{
		return 1.0f;
	}

	const u32 currentTime		= fwTimer::GetTimeInMilliseconds_NonScaledClipped();
	const u32 timeSinceStart	= currentTime - m_StateStartTime;
	float phase					= static_cast<float>(timeSinceStart) / static_cast<float>(duration);
	phase						= Clamp(phase, 0.0f, 1.0f);

	return phase;
}

void camSwitchCamera::ComputeBaseFrame(camFrame& frame, float desiredHeading) const
{
	frame.SetWorldMatrixFromHeadingPitchAndRoll(desiredHeading, m_Metadata.m_DefaultPitch * DtoR, 0.0f);
	frame.SetFov(m_Metadata.m_Fov);
	frame.SetNearClip(m_Metadata.m_NearClip);
	frame.SetMotionBlurStrength(m_Metadata.m_MotionBlurStrength);

	frame.GetFlags().SetFlag(camFrame::Flag_ShouldOverrideStreamingFocus);
}

void camSwitchCamera::ComputeBaseFrameForEstablishingShot(const CPlayerSwitchEstablishingShotMetadata& shotMetadata, camFrame& frame) const
{
	frame.SetFov(shotMetadata.m_Fov);
	frame.SetNearClip(m_Metadata.m_NearClip);
// 	frame.SetMotionBlurStrength(m_Metadata.m_MotionBlurStrength);

	frame.GetFlags().SetFlag(camFrame::Flag_ShouldOverrideStreamingFocus);
}

void camSwitchCamera::ComputeWorldMatrixForEstablishingShotStart(const CPlayerSwitchEstablishingShotMetadata& shotMetadata, Matrix34& worldMatrix) const
{
	Vector3 orientation;
	orientation.SetScaled(shotMetadata.m_Orientation, DtoR);

	orientation.x = m_Metadata.m_DefaultPitch * DtoR;

	Vector3 startOffset(m_Metadata.m_EstablishingShotRelativeStartOffset);
	startOffset.RotateZ(orientation.z);

	worldMatrix.FromEulersYXZ(orientation);
	worldMatrix.d = shotMetadata.m_Position + startOffset;
}

void camSwitchCamera::ComputeWorldMatrixForEstablishingShotEnd(const CPlayerSwitchEstablishingShotMetadata& shotMetadata, Matrix34& worldMatrix) const
{
	Vector3 orientation;
	orientation.SetScaled(shotMetadata.m_Orientation, DtoR);

	worldMatrix.FromEulersYXZ(orientation);
	worldMatrix.d = shotMetadata.m_Position;
}

void camSwitchCamera::ComputeWorldMatrixForEstablishingShotCatchUp(const CPlayerSwitchEstablishingShotMetadata& shotMetadata, Matrix34& worldMatrix) const
{
	Vector3 orientation;
	orientation.SetScaled(shotMetadata.m_CatchUpOrientation, DtoR);

	worldMatrix.FromEulersYXZ(orientation);
	worldMatrix.d = shotMetadata.m_CatchUpPosition;
}

void camSwitchCamera::GetTransition(Vec3V_InOut vPos0, Vec3V_InOut vPos1, float& fFov0, float& fFov1) const
{
	if (g_PlayerSwitch.GetLongRangeMgr().GetState() == CPlayerSwitchMgrLong::STATE_PAN)
	{
		camFrame panFrameStart;
		camFrame panFrameEnd;

		ComputeFrameForAscentCut(m_NumAscentCuts-1, panFrameStart);
		ComputeFrameForDescentCut(m_NumDescentCuts-1, panFrameEnd);

		vPos0 = VECTOR3_TO_VEC3V(panFrameStart.GetPosition());
		vPos1 = VECTOR3_TO_VEC3V(panFrameEnd.GetPosition());

		fFov0 = camInterface::GetFov();
		fFov1 = fFov0;
	}
	else
	{
		vPos0 = VECTOR3_TO_VEC3V(camInterface::GetPos());
		vPos1 = vPos0;

		fFov0 = m_Metadata.m_Fov * m_Metadata.m_MaxFovScalingForCutEffect;
		fFov1 = m_Metadata.m_Fov;
	}
}

void camSwitchCamera::GetTransitionFov(float& fFov0, float& fFov1) const
{
	if (g_PlayerSwitch.GetLongRangeMgr().GetState() == CPlayerSwitchMgrLong::STATE_PAN)
	{
		fFov0 = camInterface::GetFov();
		fFov1 = fFov0;
	}
	else
	{
		fFov0 = m_Metadata.m_Fov * m_Metadata.m_MaxFovScalingForCutEffect;
		fFov1 = m_Metadata.m_Fov;
	}
}

void camSwitchCamera::SetDest(const CPed& destinationPed, const CPlayerSwitchParams& switchParams)
{
	const fwTransform& destinationPedTransform = destinationPed.GetTransform();

	m_EndPosition			= VEC3V_TO_VECTOR3(destinationPedTransform.GetPosition());
	m_EndHeading			= destinationPedTransform.GetHeading();
	m_ControlFlags			= switchParams.m_controlFlags;
	m_DescentFloorHeight	= switchParams.m_fLowestHeightDescent;
	m_NumDescentCuts		= cameraVerifyf(switchParams.m_numJumpsDescent > 0,
								"A Switch must include at least one descent cut. Defaulting to one") ? switchParams.m_numJumpsDescent : 1;
}

#if __BANK
void camSwitchCamera::DebugOverrideCameraHeading(float& desiredHeading, u32 cutIndex, bool ascent) const
{
	switch (ms_DebugOrientationOption)
	{
		case SOO_USE_START_HEADING:
			desiredHeading = m_StartHeading;
			break;

		case SOO_USE_FINAL_HEADING:
			//desiredHeading = (cutIndex != 0 || !ascent) ? m_EndHeading : m_StartHeading;
			desiredHeading = m_EndHeading;
			break;

		case SOO_USE_MAP_NORTH:
			desiredHeading = 0.0f;
			break;

		case SOO_USE_TRAVEL_DIRECTION_AS_UP:
			desiredHeading = DebugComputePanDirectionHeading();
			break;

		case SOO_USE_TRAVEL_DIRECTION_AS_RIGHT:
			desiredHeading = DebugComputePanDirectionHeading();
			desiredHeading += HALF_PI;
			break;

		case SOO_ROTATE_ON_ASCENT_ONLY:
			if (ascent)
			{
				const float phase	= (m_NumAscentCuts > 1) ? (static_cast<float>(cutIndex) /
										static_cast<float>(m_NumAscentCuts - 1)) : 1.0f;
				desiredHeading		= fwAngle::LerpTowards(m_StartHeading, m_EndHeading, phase);
			}
			else
			{
				desiredHeading = m_EndHeading;
			}
			break;

		case SOO_ROTATE_ON_DESCENT_ONLY:
			if (ascent)
			{
				desiredHeading = m_StartHeading;
			}
			else
			{
				const float phase	= (m_NumDescentCuts > 1) ? (1.0f - (static_cast<float>(cutIndex) /
										static_cast<float>(m_NumDescentCuts - 1))) : 1.0f;
				desiredHeading		= fwAngle::LerpTowards(m_StartHeading, m_EndHeading, phase);
			}
			break;

		case SOO_ROTATE_ASCENT_AND_DESCENT:
			if (ascent)
			{
				//Rotate halfway towards the destination heading during ascent.
				const float phase		= (m_NumAscentCuts > 1) ? (static_cast<float>(cutIndex) /
											(static_cast<float>(m_NumAscentCuts - 1) * 2.0f)) : 0.5f;
				desiredHeading			= fwAngle::LerpTowards(m_StartHeading, m_EndHeading, phase);
			}
			else
			{
				//Complete the remaining half of the rotation towards the destination heading during descent.
				const float phase			= (m_NumDescentCuts > 1) ? (1.0f - (static_cast<float>(cutIndex) /
												(static_cast<float>(m_NumDescentCuts - 1) * 2.0f))) : 0.5f;
				desiredHeading				= fwAngle::LerpTowards(m_StartHeading, m_EndHeading, phase);
			}
			break;

		case SOO_DEFAULT:
		default:
			break;
	}
}

float camSwitchCamera::DebugComputePanDirectionHeading() const
{
	Vector3 panDirection = m_EndPosition - m_StartPosition;
	panDirection.NormalizeSafe(YAXIS);

	const float heading = camFrame::ComputeHeadingFromFront(panDirection);

	return heading;
}

void camSwitchCamera::AddWidgets(bkBank& bank)
{
	bank.PushGroup("Switch camera");
	{
		bank.AddCombo("Orientation options", &ms_DebugOrientationOption, SOO_MAX, g_DebugSwitchOrientationOptions);
	}
	bank.PopGroup(); //Switch camera
}
#endif // __BANK
