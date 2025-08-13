//
// camera/marketing/MarketingFreeCamera.cpp
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#if __BANK //Marketing tools are only available in BANK builds.

#include "camera/marketing/MarketingFreeCamera.h"

#include "fwdebug/picker.h"
#include "fwscene/stores/mapdatastore.h"

#include "camera/CamInterface.h"
#include "camera/gameplay/GameplayDirector.h"
#include "camera/helpers/ControlHelper.h"
#include "camera/marketing/MarketingDirector.h"
#include "camera/system/CameraFactory.h"
#include "camera/system/CameraMetadata.h"
#include "debug/DebugScene.h"
#include "peds/ped.h"
#include "renderer/PostProcessFX.h"
#include "scene/EntityIterator.h"
#include "scene/streamer/SceneStreamerMgr.h"
#include "system/controlMgr.h"
#include "text/messages.h"

CAMERA_OPTIMISATIONS()

INSTANTIATE_RTTI_CLASS(camMarketingFreeCamera,0x951CD28)

enum eLensOptions
{
	LENS_16_MM = 0,
	LENS_25_MM,
	LENS_35_MM,
	LENS_50_MM,
	LENS_75_MM,
	LENS_120_MM,
	LENS_ZOOM,
	NUM_LENS_OPTIONS
};
static const char* g_LensOptionNames[NUM_LENS_OPTIONS] =
{
	"16mm",
	"25mm",
	"35mm",
	"50mm",
	"75mm",
	"120mm",
	"Zoom"
};
static const float g_LensFocalLengths[NUM_LENS_OPTIONS] =
{
	0.016f,
	0.025f,
	0.035f,
	0.05f,
	0.075f,
	0.12f,
	0.0f
};

enum eFocusModes
{
	FOCUS_MANUALLY = 0,
	FOCUS_NEAREST_ENTITY,
	FOCUS_PICKER_ENTITY,
	FOCUS_DEBUG_ENTITY,
	FOCUS_USING_DEPTH_MEASUREMENT,
	NUM_FOCUS_MODES
};

static const char* g_FocusModeNames[NUM_FOCUS_MODES] =
{
	"MANUAL",
	"NEAREST ENTITY",
	"PICKER ENTITY",
	"DEBUG ENTITY",
	"USE DEPTH MEASUREMENT"
};

static const float g_MinManualFocusDistance		= 0.5f;
static const float g_MaxManualFocusDistance		= 5000.0f;
static const float g_DefaultManualFocusDistance	= 5.0f;

const u32 g_MarketingDofSettingsNameHash = ATSTRINGHASH("DEFAULT_MARKETING_DOF_SETTINGS", 0xb975e147);

s32 camMarketingFreeCamera::ms_FocusMode								= (s32)FOCUS_USING_DEPTH_MEASUREMENT;
s32 camMarketingFreeCamera::ms_LensOption								= (s32)LENS_35_MM;

camDepthOfFieldSettingsMetadata camMarketingFreeCamera::ms_DofSettings;
float camMarketingFreeCamera::ms_FocalPointChangeSpeed					= 0.0f;
float camMarketingFreeCamera::ms_FocalPointMaxSpeed						= 2.0f;
float camMarketingFreeCamera::ms_FocalPointAcceleration					= 50.0f;
float camMarketingFreeCamera::ms_MaxDistanceToTestForEntity				= 100.0f;
float camMarketingFreeCamera::ms_ManualFocusDistance					= g_DefaultManualFocusDistance;
float camMarketingFreeCamera::ms_OverriddenFoV							= g_DefaultFov;
float camMarketingFreeCamera::ms_FocusEntityBoundRadiusScaling			= 1.0f;
bool camMarketingFreeCamera::ms_ShouldKeepFocusEntityBoundsInFocus		= true;


camMarketingFreeCamera::camMarketingFreeCamera(const camBaseObjectMetadata& metadata)
: camBaseCamera(metadata)
, m_Metadata(static_cast<const camMarketingFreeCameraMetadata&>(metadata))
//Initialise only the persistent camera parameters that are not reset on camera activation/deactivation.
, m_LookAroundSpeedScaling(1.0f)
, m_TranslationSpeedScaling(1.0f)
, m_ShouldInvertLook(false)
, m_ShouldSwapPadSticks(false)
, m_ShouldLoadNearbyIpls(false)
{
	Reset();

	const camDepthOfFieldSettingsMetadata* dofSettings = camFactory::FindObjectMetadata<camDepthOfFieldSettingsMetadata>(g_MarketingDofSettingsNameHash);
	if (dofSettings)
	{
		ms_DofSettings = *dofSettings;
	}
}

void camMarketingFreeCamera::InitClass()
{
	const float focalLength = g_LensFocalLengths[ms_LensOption];
	if (focalLength >= SMALL_FLOAT)
	{
		ms_OverriddenFoV = RtoD * 2.0f * atan( (0.5f * g_HeightOf35mmFullFrame) / focalLength );
	}
}

void camMarketingFreeCamera::Reset()
{
	//Clone the current rendered camera frame.
	SetFrame(camInterface::GetFrame());

	if(!g_CameraWorldLimits_AABB.ContainsPoint(VECTOR3_TO_VEC3V(m_Frame.GetPosition())))
	{
		//Fall back to the origin if we cloned a rendered frame with an invalid position.
		m_Frame.SetPosition(VEC3_ZERO);
	}

	m_TranslationVelocity.Zero();
	m_Translation.Zero();
	m_RelativeTranslationVelocity.Zero();
	m_RelativeTranslation.Zero();

	m_LookAroundHeadingSpeed	= 0.0f;
	m_LookAroundPitchSpeed		= 0.0f;
	m_LookAroundHeadingOffset	= 0.0f;
	m_LookAroundPitchOffset		= 0.0f;

	m_AimAcceleration			= m_Metadata.m_LookAroundInputResponse.m_Acceleration;
	m_AimDeceleration			= m_Metadata.m_LookAroundInputResponse.m_Deceleration;
	m_TranslationAcceleration	= m_Metadata.m_TranslationInputResponse.m_Acceleration;
	m_TranslationDeceleration	= m_Metadata.m_TranslationInputResponse.m_Deceleration;

	m_ZoomSpeed					= 0.0f;
	m_ZoomOffset				= 0.0f;
	m_Fov						= m_Metadata.m_ZoomDefaultFov;
	m_DofNearAtDefaultFov		= m_Frame.GetNearInFocusDofPlane();
	m_DofFarAtDefaultFov		= m_Frame.GetFarInFocusDofPlane();
	m_DofNearAtMaxZoom			= m_Frame.GetNearInFocusDofPlane() * 4.0f;
	m_DofFarAtMaxZoom			= m_Frame.GetFarInFocusDofPlane() * 4.0f;
	m_DofNearAtMinZoom			= m_Frame.GetNearInFocusDofPlane() * 0.25f;
	m_DofFarAtMinZoom			= m_Frame.GetFarInFocusDofPlane() * 0.25f;

	m_RollSpeed					= 0.0f;
	m_RollOffset				= 0.0f;
}

bool camMarketingFreeCamera::Update()
{
	UpdateControl();

	ApplyTranslation();
	ApplyZoom();
	ApplyDof();
	ApplyRotation();

	UpdateLighting();

	return true;
}

void camMarketingFreeCamera::UpdateControl()
{
	CPad& pad = CControlMgr::GetDebugPad();

	UpdateLookAroundControl(pad);
	UpdateTranslationControl(pad);
	UpdateZoomControl(pad);
	UpdateRollControl(pad);
	UpdateMiscControl(pad);
}

void camMarketingFreeCamera::UpdateLookAroundControl(CPad& pad)
{
	//Update speed control.
	if(pad.GetShockButtonR() && (pad.DPadUpJustDown() || pad.DPadDownJustDown()))
	{
		m_LookAroundSpeedScaling += pad.DPadUpJustDown() ? m_Metadata.m_SpeedScalingStepSize : -m_Metadata.m_SpeedScalingStepSize;
		m_LookAroundSpeedScaling = Clamp(m_LookAroundSpeedScaling, m_Metadata.m_MinSpeedScaling, m_Metadata.m_MaxSpeedScaling);

		//Display confirmation debug text (if the HUD is enabled.)
		char *string			= TheText.Get("MKT_AIM_SPD");
		const s32 percentage	= (s32)Floorf((m_LookAroundSpeedScaling * 100.0f) + 0.5f);

		CNumberWithinMessage NumberArray[1];
		NumberArray[0].Set(percentage);
		CHelpMessage::SetMessageText(HELP_TEXT_SLOT_STANDARD, string, NumberArray, 1);
	}

	float headingInput;
	float pitchInput;
	ComputeLookAroundInput(pad, headingInput, pitchInput);

	//Apply acceleration and deceleration to stick input.
	const float timeStep		= fwTimer::GetNonPausableCamTimeStep();
	m_LookAroundHeadingOffset	= camControlHelper::ComputeOffsetBasedOnAcceleratedInput(headingInput, m_AimAcceleration * DtoR *
									m_LookAroundSpeedScaling, m_AimDeceleration * DtoR * m_LookAroundSpeedScaling,
									m_Metadata.m_LookAroundInputResponse.m_MaxSpeed * DtoR * m_LookAroundSpeedScaling, m_LookAroundHeadingSpeed, timeStep, false);
	m_LookAroundPitchOffset		= camControlHelper::ComputeOffsetBasedOnAcceleratedInput(pitchInput, m_AimAcceleration * DtoR *
									m_LookAroundSpeedScaling, m_AimDeceleration * DtoR * m_LookAroundSpeedScaling,
									m_Metadata.m_LookAroundInputResponse.m_MaxSpeed * DtoR * m_LookAroundSpeedScaling, m_LookAroundPitchSpeed, timeStep, false);
}

//Check if we have look-around input, using a dead-zone to prevent false hits.
bool camMarketingFreeCamera::ComputeLookAroundInput(CPad& pad, float& headingInput, float& pitchInput) const
{
	const float maxStickValue = 128.0f;
	headingInput	= -(m_ShouldSwapPadSticks ? pad.GetLeftStickX() : pad.GetRightStickX()) / maxStickValue;
	pitchInput		= -(m_ShouldSwapPadSticks ? pad.GetLeftStickY() : pad.GetRightStickY()) / maxStickValue;

	if(m_ShouldInvertLook)
	{
		pitchInput	= -pitchInput;
	}

	const float inputMag		= rage::Sqrtf((headingInput * headingInput) + (pitchInput * pitchInput));
	const bool isInputPresent	= (inputMag >= m_Metadata.m_LookAroundInputResponse.m_MaxInputMagInDeadZone);
	if(isInputPresent)
	{
		//Rescale the input levels taking into account the dead-zone.
		float absRescaledHeadingInput	= RampValueSafe(Abs(headingInput), m_Metadata.m_LookAroundInputResponse.m_MaxInputMagInDeadZone, 1.0f, 0.0f, 1.0f);
		float absRescaledPitchInput		= RampValueSafe(Abs(pitchInput), m_Metadata.m_LookAroundInputResponse.m_MaxInputMagInDeadZone, 1.0f, 0.0f, 1.0f);

		//Apply the custom power factor scaling specified in metadata to change the control response across the range of stick input.
		absRescaledHeadingInput	= rage::Powf(absRescaledHeadingInput, m_Metadata.m_LookAroundInputResponse.m_InputMagPowerFactor);
		absRescaledPitchInput	= rage::Powf(absRescaledPitchInput, m_Metadata.m_LookAroundInputResponse.m_InputMagPowerFactor);

		headingInput	= Sign(headingInput) * absRescaledHeadingInput;
		pitchInput		= Sign(pitchInput) * absRescaledPitchInput;
	}
	else
	{
		//Zero the inputs within the dead-zone to avoid slow drifting.
		headingInput	= 0.0f;
		pitchInput		= 0.0f;
	}

	return isInputPresent;
}

void camMarketingFreeCamera::UpdateTranslationControl(CPad& pad)
{
	//Update speed control.
	if(pad.GetShockButtonL() && (pad.DPadUpJustDown() || pad.DPadDownJustDown()))
	{
		m_TranslationSpeedScaling += pad.DPadUpJustDown() ? m_Metadata.m_SpeedScalingStepSize : -m_Metadata.m_SpeedScalingStepSize;
		m_TranslationSpeedScaling = Clamp(m_TranslationSpeedScaling, m_Metadata.m_MinSpeedScaling, m_Metadata.m_MaxSpeedScaling);

		//Display confirmation debug text (if the HUD is enabled.)
		char *string			= TheText.Get("MKT_MOVE_SPD");
		const s32 percentage	= (s32)Floorf((m_TranslationSpeedScaling * 100.0f) + 0.5f);

		CNumberWithinMessage NumberArray[1];
		NumberArray[0].Set(percentage);
		CHelpMessage::SetMessageText(HELP_TEXT_SLOT_STANDARD, string, NumberArray, 1);
	}

	Vector3 translationInput;
	Vector3 relativeTranslationInput;
	ComputeTranslationInput(pad, translationInput, relativeTranslationInput);

	//Apply acceleration and deceleration to stick input.

	const float acceleration	= m_TranslationAcceleration * m_TranslationSpeedScaling;
	const float deceleration	= m_TranslationDeceleration * m_TranslationSpeedScaling;
	const float maxSpeed		= m_Metadata.m_TranslationInputResponse.m_MaxSpeed * m_TranslationSpeedScaling;
	const float timeStep		= fwTimer::GetNonPausableCamTimeStep();

	m_Translation.x				= camControlHelper::ComputeOffsetBasedOnAcceleratedInput(translationInput.x, acceleration, deceleration,
									maxSpeed, m_TranslationVelocity.x, timeStep, false);
	m_Translation.y				= camControlHelper::ComputeOffsetBasedOnAcceleratedInput(translationInput.y, acceleration, deceleration,
									maxSpeed, m_TranslationVelocity.y, timeStep, false);
	m_Translation.z				= camControlHelper::ComputeOffsetBasedOnAcceleratedInput(translationInput.z, acceleration, deceleration,
									maxSpeed, m_TranslationVelocity.z, timeStep, false);

	m_RelativeTranslation.x		= camControlHelper::ComputeOffsetBasedOnAcceleratedInput(relativeTranslationInput.x, acceleration, deceleration,
									maxSpeed, m_RelativeTranslationVelocity.x, timeStep, false);
	m_RelativeTranslation.y		= camControlHelper::ComputeOffsetBasedOnAcceleratedInput(relativeTranslationInput.y, acceleration, deceleration,
									maxSpeed, m_RelativeTranslationVelocity.y, timeStep, false);
	m_RelativeTranslation.z		= camControlHelper::ComputeOffsetBasedOnAcceleratedInput(relativeTranslationInput.z, acceleration, deceleration,
									maxSpeed, m_RelativeTranslationVelocity.z, timeStep, false);
}

//Check if we have translation input, using a dead-zone to prevent false hits.
void camMarketingFreeCamera::ComputeTranslationInput(CPad& pad, Vector3& translationInput, Vector3& relativeTranslationInput) const
{
	const float maxControlValue	= 128.0f;
	float x						= (m_ShouldSwapPadSticks ? pad.GetRightStickX() : pad.GetLeftStickX()) / maxControlValue;
	float y						= -(m_ShouldSwapPadSticks ? pad.GetRightStickY() : pad.GetLeftStickY()) / maxControlValue;

	const float xyInputMag		= rage::Sqrtf((x * x) + (y * y));
	const bool isXyInputPresent	= (xyInputMag >= m_Metadata.m_TranslationInputResponse.m_MaxInputMagInDeadZone);
	if(isXyInputPresent)
	{
		//Rescale the input levels taking into account the dead-zone.
		float absRescaledX	= RampValueSafe(Abs(x), m_Metadata.m_TranslationInputResponse.m_MaxInputMagInDeadZone, 1.0f, 0.0f, 1.0f);
		float absRescaledY	= RampValueSafe(Abs(y), m_Metadata.m_TranslationInputResponse.m_MaxInputMagInDeadZone, 1.0f, 0.0f, 1.0f);

		//Apply the custom power factor scaling specified in metadata to change the control response across the range of stick input.
		absRescaledX	= rage::Powf(absRescaledX, m_Metadata.m_TranslationInputResponse.m_InputMagPowerFactor);
		absRescaledY	= rage::Powf(absRescaledY, m_Metadata.m_TranslationInputResponse.m_InputMagPowerFactor);

		x	= Sign(x) * absRescaledX;
		y	= Sign(y) * absRescaledY;
	}
	else
	{
		//Zero the inputs within the dead-zone to avoid slow drifting.
		x	= 0.0f;
		y	= 0.0f;
	}

	const float zUpInput			= pad.GetButtonSquare() / maxControlValue;
	const float zDownInput			= pad.GetButtonCross() / maxControlValue;
	const bool isZUpInputPresent	= (zUpInput >= m_Metadata.m_TranslationInputResponse.m_MaxInputMagInDeadZone);
	const bool isZDownInputPresent	= (zDownInput >= m_Metadata.m_TranslationInputResponse.m_MaxInputMagInDeadZone);
	float z							= isZUpInputPresent ? zUpInput : -zDownInput;

	if(isZUpInputPresent || isZDownInputPresent)
	{
		//Rescale the input level taking into account the dead-zone.
		float absRescaledInput = RampValueSafe(Abs(z), m_Metadata.m_TranslationInputResponse.m_MaxInputMagInDeadZone, 1.0f, 0.0f, 1.0f);

		//Apply the custom power factor scaling specified in metadata to change the control response across the range of button input.
		absRescaledInput = rage::Powf(absRescaledInput, m_Metadata.m_TranslationInputResponse.m_InputMagPowerFactor);

		z = Sign(z) * absRescaledInput;
	}
	else
	{
		//Zero the input within the dead-zone to avoid slow drifting.
		z = 0.0f;
	}

	const bool shouldUseRdrControlMapping = camInterface::GetMarketingDirector().ShouldUseRdrControlMapping();
	if(shouldUseRdrControlMapping)
	{
		translationInput.Zero();
		relativeTranslationInput.Set(x, y, z);
	}
	else
	{
		translationInput.Set(x, y, 0.0f);
		relativeTranslationInput.Set(0.0f, z, 0.0f);
	}
}

void camMarketingFreeCamera::UpdateZoomControl(CPad& pad)
{
	float zoomInput;
	ComputeZoomInput(pad, zoomInput);

	//Apply acceleration and deceleration to trigger input.
	const float timeStep	= fwTimer::GetNonPausableCamTimeStep();
	m_ZoomOffset			= camControlHelper::ComputeOffsetBasedOnAcceleratedInput(zoomInput, m_Metadata.m_ZoomInputResponse.m_Acceleration * DtoR,
								m_Metadata.m_ZoomInputResponse.m_Deceleration * DtoR, m_Metadata.m_ZoomInputResponse.m_MaxSpeed * DtoR, m_ZoomSpeed, timeStep, false);
}

//Check if we have zoom input, using a dead-zone to prevent false hits.
bool camMarketingFreeCamera::ComputeZoomInput(CPad& pad, float& zoomInput)
{
	if(pad.GetDebugButtons() & ioPad::START)
	{
		//Do not process input if the start button is held down, as special debug buttons may be pressed in combination with this.
		zoomInput = 0.0f;
		return false; 
	}

	const float maxTriggerValue	= 128.0f;
	const float zoomInInput		= pad.GetLeftShoulder2() / maxTriggerValue;
	const float zoomOutInput	= pad.GetRightShoulder2() / maxTriggerValue;

	const bool isZoomInInputPresent		= (zoomInInput >= m_Metadata.m_ZoomInputResponse.m_MaxInputMagInDeadZone);
	const bool isZoomOutInputPresent	= (zoomOutInput >= m_Metadata.m_ZoomInputResponse.m_MaxInputMagInDeadZone);
	const bool isAnyZoomInputPresent	= (isZoomInInputPresent || isZoomOutInputPresent);
	const bool shouldResetZoom			= (isZoomInInputPresent && isZoomOutInputPresent);

	if (isAnyZoomInputPresent)
	{
		ms_LensOption = LENS_ZOOM;
	}

	if(shouldResetZoom)
	{
		//Reset the FOV to the default setting and zero the input.
		m_Fov		= m_Metadata.m_ZoomDefaultFov;
		zoomInput	= 0.0f;
	}
	else if(isAnyZoomInputPresent)
	{
		zoomInput	= isZoomInInputPresent ? zoomInInput : -zoomOutInput;

		//Rescale the input level taking into account the dead-zone.
		float absRescaledZoomInput = RampValueSafe(Abs(zoomInput), m_Metadata.m_ZoomInputResponse.m_MaxInputMagInDeadZone, 1.0f, 0.0f, 1.0f);

		//Apply the custom power factor scaling specified in metadata to change the control response across the range of trigger input.
		absRescaledZoomInput = rage::Powf(absRescaledZoomInput, m_Metadata.m_ZoomInputResponse.m_InputMagPowerFactor);

		zoomInput	= Sign(zoomInput) * absRescaledZoomInput;
	}
	else
	{
		//Zero the input within the dead-zone to avoid slow drifting.
		zoomInput	= 0.0f;
	}

	return isAnyZoomInputPresent;
}

void camMarketingFreeCamera::UpdateRollControl(CPad& pad)
{
	float rollInput;
	ComputeRollInput(pad, rollInput);

	//Apply acceleration and deceleration to trigger input.
	const float timeStep	= fwTimer::GetNonPausableCamTimeStep();
	m_RollOffset			= camControlHelper::ComputeOffsetBasedOnAcceleratedInput(rollInput, m_Metadata.m_RollInputResponse.m_Acceleration * DtoR,
								m_Metadata.m_RollInputResponse.m_Deceleration * DtoR, m_Metadata.m_RollInputResponse.m_MaxSpeed * DtoR, m_RollSpeed, timeStep, false);
}

//Check if we have roll input, using a dead-zone to prevent false hits.
bool camMarketingFreeCamera::ComputeRollInput(CPad& pad, float& rollInput)
{
	const float maxTriggerValue		= 128.0f;
	const float negativeRollInput	= pad.GetButtonTriangle() ? (pad.GetLeftShoulder1() / maxTriggerValue) : 0.0f;
	const float positiveRollInput	= pad.GetButtonTriangle() ? (pad.GetRightShoulder1() / maxTriggerValue) : 0.0f;

	const bool isNegativeRollInputPresent	= (negativeRollInput >= m_Metadata.m_RollInputResponse.m_MaxInputMagInDeadZone);
	const bool isPositiveRollInputPresent	= (positiveRollInput >= m_Metadata.m_RollInputResponse.m_MaxInputMagInDeadZone);
	const bool isAnyRollInputPresent		= (isNegativeRollInputPresent || isPositiveRollInputPresent);
	m_ShouldResetRoll						= isNegativeRollInputPresent && isPositiveRollInputPresent;

	if(m_ShouldResetRoll)
	{
		//Zero the current input.
		rollInput	= 0.0f;
	}
	else if(isAnyRollInputPresent)
	{
		rollInput	= isNegativeRollInputPresent ? -negativeRollInput : positiveRollInput;

		//Rescale the input level taking into account the dead-zone.
		float absRescaledRollInput = RampValueSafe(Abs(rollInput), m_Metadata.m_RollInputResponse.m_MaxInputMagInDeadZone, 1.0f, 0.0f, 1.0f);

		//Apply the custom power factor scaling specified in metadata to change the control response across the range of trigger input.
		absRescaledRollInput = rage::Powf(absRescaledRollInput, m_Metadata.m_RollInputResponse.m_InputMagPowerFactor);

		rollInput	= Sign(rollInput) * absRescaledRollInput;
	}
	else
	{
		//Zero the input within the dead-zone to avoid slow drifting.
		rollInput	= 0.0f;
	}

	return isAnyRollInputPresent;
}

void camMarketingFreeCamera::UpdateMiscControl(CPad& pad)
{
	if(pad.DPadDownJustDown() && !pad.GetShockButtonL() && !pad.GetShockButtonR() && !(pad.GetDebugButtons() & ioPad::START))
	{
		//Drop follow ped/vehicle.
		TeleportFollowEntity();
	}

	if(pad.GetPressedDebugButtons() & ioPad::RUP)
	{
		//Toggle loading of IPLs around free cam.
		m_ShouldLoadNearbyIpls = !m_ShouldLoadNearbyIpls;

		//Display confirmation debug text (if the HUD is enabled.)
		const char* stringLabel	= m_ShouldLoadNearbyIpls ? "MKT_IPL" : "MKT_NO_IPL";
		char *string			= TheText.Get(stringLabel);
		const s32 percentage	= (s32)Floorf((m_TranslationSpeedScaling * 100.0f) + 0.5f);

		CNumberWithinMessage NumberArray[1];
		NumberArray[0].Set(percentage);
		CHelpMessage::SetMessageText(HELP_TEXT_SLOT_STANDARD, string, NumberArray, 1);
	}
	
	//Dof controls
	if (pad.GetPressedDebugButtons() & ioPad::L1)
	{
		//Cycle lens options
		ms_LensOption = (ms_LensOption + 1) % LENS_ZOOM;
		OnLensChange();
	}

	//NOTE: We must ignore the left and right bumpers when triangle/y is held, as this input combination is used elsewhere
	float focusPointChangeInput = 0.0f;
	if (!pad.GetButtonTriangle() && !(pad.GetDebugButtons() & ioPad::START))
	{
		const bool shouldIncreaseFocusDistance = (pad.GetRightShoulder1() > 0);
		const bool shouldDecreaseFocusDistance = (pad.GetLeftShoulder1() > 0);

		//Allow the user to to switch to manual focus when changing the focus distance when focusing using depth measurement only.
		if ((ms_FocusMode == FOCUS_USING_DEPTH_MEASUREMENT) && (shouldIncreaseFocusDistance || shouldDecreaseFocusDistance))
		{
			ms_FocusMode = FOCUS_MANUALLY;
		}

		if (ms_FocusMode == FOCUS_MANUALLY)
		{
			if (shouldIncreaseFocusDistance && shouldDecreaseFocusDistance)
			{
				//Reset the focal point to a default distance.
				ms_ManualFocusDistance		= g_DefaultManualFocusDistance;
				ms_FocalPointChangeSpeed	= 0.0f;

				return;
			}
			else if (shouldIncreaseFocusDistance)
			{
				//Push focal point out
				focusPointChangeInput = 1.0f;
			}
			else if (shouldDecreaseFocusDistance)
			{
				//Bring focal point closer
				focusPointChangeInput = -1.0f;
			}
		}
	}

	const float timeStep			= fwTimer::GetNonPausableCamTimeStep();
	const float focusDistanceOffset	= camControlHelper::ComputeOffsetBasedOnAcceleratedInput(focusPointChangeInput, ms_FocalPointAcceleration,
										ms_FocalPointAcceleration, ms_FocalPointMaxSpeed, ms_FocalPointChangeSpeed, timeStep, false);

	if ((ms_FocusMode == FOCUS_MANUALLY) && !IsNearZero(focusDistanceOffset))
	{
		ms_ManualFocusDistance += focusDistanceOffset;
		ms_ManualFocusDistance = Clamp(ms_ManualFocusDistance, g_MinManualFocusDistance, g_MaxManualFocusDistance);
	}
}

void camMarketingFreeCamera::ApplyTranslation()
{
	const Matrix34& worldMatrix = m_Frame.GetWorldMatrix();

	Vector3 worldPosition;
	worldMatrix.Transform(m_RelativeTranslation, worldPosition);

	worldPosition += m_Translation;

	//Avoid propagating numerical precision errors that can cause drift on Xbox360.
	if((m_RelativeTranslation.Mag2() >= VERY_SMALL_FLOAT) || (m_Translation.Mag2() >= VERY_SMALL_FLOAT))
	{
		m_Frame.SetPosition(worldPosition);
	}

	if(m_ShouldLoadNearbyIpls)
	{
		INSTANCE_STORE.GetBoxStreamer().Deprecated_AddSecondarySearch(RCC_VEC3V(worldPosition));
	}
}

void camMarketingFreeCamera::ApplyZoom()
{
	if (ms_LensOption != LENS_ZOOM)
	{
		m_Fov				= ms_OverriddenFoV;
	}
	else
	{
		const float maxZoomFactor = m_Metadata.m_ZoomMaxFov / m_Metadata.m_ZoomMinFov;

		//NOTE: We scale the zoom offset based upon the current zoom factor. This prevents zoom speed seeming faster towards the low end of the zoom range.
		float zoomFactor	= m_Metadata.m_ZoomMaxFov / m_Fov;
		m_ZoomOffset		*= zoomFactor;
		zoomFactor			+= m_ZoomOffset;
		zoomFactor			= Clamp(zoomFactor, 1.0f, maxZoomFactor);
		m_Fov				= m_Metadata.m_ZoomMaxFov / zoomFactor;
	}

	m_Frame.SetFov(m_Fov);
}

void camMarketingFreeCamera::ApplyDof()
{
	// calculate the DOF in the camera code because we want it to change based on the fov zoom
	// this code re-purposed from camFrame::ComputeDofFromFovAndClipRange (with this camera's min/max zoom settings)
	float nearDof	= m_DofNearAtDefaultFov;
	float farDof	= m_DofFarAtDefaultFov;

	const float fovDelta = m_Fov - g_DefaultFov;
	if(fovDelta > 0.0f)
	{
		//Zoom out
		const float maxFovDelta = m_Metadata.m_ZoomMaxFov - g_DefaultFov;
		nearDof	+= Min(fovDelta / maxFovDelta, 1.0f) * (m_DofNearAtMinZoom - m_DofNearAtDefaultFov);				//  The DOF is its default plus this change
		farDof	+= Min(fovDelta / maxFovDelta, 1.0f) * (m_DofFarAtMinZoom - m_DofFarAtDefaultFov);					//  The DOF is its default plus this change
	}
	else
	{
		//Zoom in
		const float minFovDelta =  g_DefaultFov - m_Metadata.m_ZoomMinFov;
		nearDof	+= Max(fovDelta / minFovDelta, -1.0f) * (m_DofNearAtDefaultFov - m_DofNearAtMaxZoom);				//  The DOF is its default plus this change
		farDof	+= Max(fovDelta / minFovDelta, -1.0f) * (m_DofFarAtDefaultFov - m_DofFarAtMaxZoom);					//  The DOF is its default plus this change
	}

	m_Frame.SetNearInFocusDofPlane(nearDof);
	m_Frame.SetFarInFocusDofPlane(farDof);
}

void camMarketingFreeCamera::ApplyRotation()
{
	float heading;
	float pitch;
	float roll;
	m_Frame.ComputeHeadingPitchAndRoll(heading, pitch, roll);

	//NOTE: We scale the look-around offsets based upon the current FOV, as this allows the look-around input to be responsive at minimum zoom without
	//being too fast at maximum zoom.
	float fovOrientationScaling = m_Fov / m_Metadata.m_ZoomDefaultFov;

	heading	+= m_LookAroundHeadingOffset * fovOrientationScaling;
	heading	= fwAngle::LimitRadianAngle(heading);

	pitch	+= m_LookAroundPitchOffset * fovOrientationScaling;
	pitch	= Clamp(pitch, -m_Metadata.m_MaxPitch * DtoR, m_Metadata.m_MaxPitch * DtoR);

	if(m_ShouldResetRoll)
	{
		roll = 0.0f;
	}

	//NOTE: We do not need to scale the roll based upon the current FOV, as it is FOV-independent.
	roll	+= m_RollOffset;
	roll	= Clamp(roll, -m_Metadata.m_MaxRoll * DtoR, m_Metadata.m_MaxRoll * DtoR);

	m_Frame.SetWorldMatrixFromHeadingPitchAndRoll(heading, pitch, roll);
}


void camMarketingFreeCamera::UpdateLighting()
{
#if 0
	//Add the debug light that can be attached and detached by the widgets:
 	if(m_IsDebugLightActive)
 	{		
 		if(m_IsDebugLightAttached)
 		{
 			//Move the light with the camera frame.
 			const Vector3 lightDir(0.0f, 1.0f, 0.0f);
 			const Vector3 lightTan(1.0f, 0.0f, 0.0f);
 
 			m_Frame.GetWorldMatrix().Transform3x3(lightDir, m_DebugLightDirection);
 			m_Frame.GetWorldMatrix().Transform3x3(lightTan, m_DebugLightTangent);
 
 			m_DebugLightPosition = m_Frame.GetWorldMatrix().d;
 		}
 
 		//It looks like this light must be added every update and is automatically removed when we no longer add it.
 		fwInteriorLocation outside;
 		CLights::AddSceneLight(LT_SPOT, LT_FLAG_CUTSCENE_LIGHT, m_DebugLightDirection, m_DebugLightTangent, m_DebugLightPosition, m_DebugLightColour, m_DebugLightIntensity, 0, CLights::m_defaultTxdID, m_DebugLightRange/*falloff max*/, m_DebugLightRange * m_DebugLightInnerCone * 0.01f /*inner cone*/, m_DebugLightRange/*outer cone*/, outside, 0);
 	}
#endif
}

void camMarketingFreeCamera::TeleportFollowEntity()
{
	CPed* followPed = const_cast<CPed*>(camInterface::FindFollowPed());
	if(followPed && cameraVerifyf(!followPed->IsNetworkClone(), "camMarketingFreeCamera trying to teleport a network clone"))
	{
		const Vector3& position	= m_Frame.GetPosition();
		const float heading		= m_Frame.ComputeHeading();

		const CVehicle* followVehicle	= camInterface::GetGameplayDirector().GetFollowVehicle();
		const s32 entryExitState		= camInterface::GetGameplayDirector().GetVehicleEntryExitState();
		if(followVehicle && (entryExitState == camGameplayDirector::INSIDE_VEHICLE))
		{
			const_cast<CVehicle*>(followVehicle)->Teleport(position, heading);
		}
		else
		{
			followPed->Teleport(position, heading);
		}

		g_SceneStreamerMgr.LoadScene(position);
	}
}

void camMarketingFreeCamera::ComputeDofOverriddenFocusSettings(Vector3& focusPosition, float& blendLevel) const
{
	camBaseCamera::ComputeDofOverriddenFocusSettings(focusPosition, blendLevel);

	if (ms_FocusMode == FOCUS_USING_DEPTH_MEASUREMENT)
	{
		return;
	}

	if (ms_FocusMode == FOCUS_MANUALLY)
	{
		blendLevel = 1.0f;

		const Vector3& cameraPosition = m_PostEffectFrame.GetPosition();
		const Vector3& cameraFront = m_PostEffectFrame.GetFront();
		focusPosition = cameraPosition + (cameraFront * ms_ManualFocusDistance);
		return;
	}

	const CEntity* focusEntity = ComputeFocusEntity();
	if(!focusEntity)
	{
		blendLevel = 0.0f;
		return;
	}

	blendLevel = 1.0f;

	const fwTransform& lookAtTargetTransform	= focusEntity->GetTransform();
	focusPosition								= VEC3V_TO_VECTOR3(lookAtTargetTransform.GetPosition());

	if(focusEntity->GetIsTypePed())
	{
		//Focus on the ped's head position at the end of the previous update. It would be preferable to focus on the plane of the eyes that will be rendered this update,
		//however the update order of the game can result in a ped's head position being resolved after the camera update, which makes it impractical to query the 'live'
		//eye positions here. Any such queries can give results based upon an un-posed or non-final skeleton.

		Matrix34 focusPedHeadBoneMatrix;
		const CPed* focusPed		= static_cast<const CPed*>(focusEntity);
		const bool isHeadBoneValid	= focusPed->GetBoneMatrixCached(focusPedHeadBoneMatrix, BONETAG_HEAD);
		if(isHeadBoneValid)
		{
			focusPosition.Set(focusPedHeadBoneMatrix.d);
		}
	}
}

const CEntity* camMarketingFreeCamera::ComputeFocusEntity() const
{
	const CEntity* focusEntity;

	switch (ms_FocusMode)
	{
	case FOCUS_NEAREST_ENTITY:
		focusEntity = FindNearestEntityToPosition(m_PostEffectFrame.GetPosition(), ms_MaxDistanceToTestForEntity);
		break;

	case FOCUS_PICKER_ENTITY:
		focusEntity = static_cast<const CEntity*>(g_PickerManager.GetSelectedEntity());
		break;

	case FOCUS_DEBUG_ENTITY:
		focusEntity = CDebugScene::FocusEntities_Get(0);
		break;

	default:
		focusEntity = NULL;
	}

	return focusEntity;
}

void camMarketingFreeCamera::ComputeDofSettings(float overriddenFocusDistance, float overriddenFocusDistanceBlendLevel, camDepthOfFieldSettingsMetadata& settings) const
{
	settings = ms_DofSettings;

	if(overriddenFocusDistanceBlendLevel <= (1.0f - SMALL_FLOAT))
	{
		return;
	}

	if(!ms_ShouldKeepFocusEntityBoundsInFocus)
	{
		return;
	}

	const CEntity* focusEntity = ComputeFocusEntity();
	if(!focusEntity)
	{
		return;
	}

	//Modify the lens aperture to ensure that the bounding sphere of the focus entity remains in acceptable focus.
	//NOTE: No assumption has been made that we are focusing on the bound centre, as this is far from guaranteed.

	Vector3 focusEntityBoundCentre;
	float focusEntityBoundRadius	= focusEntity->GetBoundCentreAndRadius(focusEntityBoundCentre);
	focusEntityBoundRadius			*= ms_FocusEntityBoundRadiusScaling;
	const Vector3& cameraPosition	= m_PostEffectFrame.GetPosition();
	const float distanceToEntity	= cameraPosition.Dist(focusEntityBoundCentre);

	//NOTE: Any focus distance bias is applied much later, so we must take account of this here.
	const float focusDistanceToConsider = overriddenFocusDistance + settings.m_FocusDistanceBias;

	float maxNearInFocusDistanceForEntity		= distanceToEntity - focusEntityBoundRadius;
	maxNearInFocusDistanceForEntity				= Max(maxNearInFocusDistanceForEntity, 0.0f);
	float denominator							= (focusDistanceToConsider - maxNearInFocusDistanceForEntity);
	const float revisedHyperfocalDistanceNear	= (denominator >= SMALL_FLOAT) ? (focusDistanceToConsider * maxNearInFocusDistanceForEntity / denominator) : LARGE_FLOAT;

	const float minFarInFocusDistanceForEntity	= distanceToEntity + focusEntityBoundRadius;
	denominator									= (minFarInFocusDistanceForEntity - focusDistanceToConsider);
	const float revisedHyperfocalDistanceFar	= (denominator >= SMALL_FLOAT) ? (focusDistanceToConsider * minFarInFocusDistanceForEntity / denominator) : LARGE_FLOAT;

	const float revisedHyperfocalDistanceToConsider = Min(revisedHyperfocalDistanceNear, revisedHyperfocalDistanceFar);

	const float fov						= m_PostEffectFrame.GetFov();
	const float focalLengthOfLens		= settings.m_FocalLengthMultiplier * g_HeightOf35mmFullFrame / (2.0f * Tanf(0.5f * fov * DtoR));
	const float desiredFNumberOfLens	= square(focalLengthOfLens) / (revisedHyperfocalDistanceToConsider * g_CircleOfConfusionFor35mm);

	settings.m_FNumberOfLens			= Max(settings.m_FNumberOfLens, desiredFNumberOfLens);
}

CEntity* camMarketingFreeCamera::FindNearestEntityToPosition(const Vector3& position, float maxDistanceToTest) const
{
	float distance2ToNearestEntity	= LARGE_FLOAT;
	CEntity* nearestEntity			= NULL;

	Vec3V vIteratorPos = VECTOR3_TO_VEC3V(position);
	CEntityIterator entityIterator(IteratePeds | IterateVehicles, NULL, &vIteratorPos, maxDistanceToTest);

	CEntity* nextEntity = entityIterator.GetNext();
	while(nextEntity)
	{
		const Vector3 entityPosition	= VEC3V_TO_VECTOR3(nextEntity->GetTransform().GetPosition());
		const float distance2ToEntity	= position.Dist2(entityPosition);
		if(distance2ToEntity < distance2ToNearestEntity)
		{
			distance2ToNearestEntity	= distance2ToEntity;
			nearestEntity				= nextEntity;
		}

		nextEntity = entityIterator.GetNext();
	}

	return nearestEntity;
}

void camMarketingFreeCamera::OnLensChange()
{
	const float focalLength = g_LensFocalLengths[ms_LensOption];
	if (focalLength >= SMALL_FLOAT)
	{
		ms_OverriddenFoV = RtoD * 2.0f * atan( (0.5f * g_HeightOf35mmFullFrame) / focalLength );
	}
}

void camMarketingFreeCamera::AddWidgetsForInstance()
{
	if(!m_WidgetGroup)
	{
		bkBank* bank = BANKMGR.FindBank("Marketing Tools");
		if(bank)
		{
			const char* cameraName = GetName();
			m_WidgetGroup = bank->PushGroup(cameraName, false);
			{
				bank->AddSlider("Position", (Vector3*)&m_Frame.GetPosition(),	-99999.0f, 99999.0f, 0.01f);

				bank->PushGroup("Control", false);
				{
					bank->AddToggle("Invert look",	&m_ShouldInvertLook);
					bank->AddToggle("Swap sticks",	&m_ShouldSwapPadSticks);
				}
				bank->PopGroup(); // Control

				bank->PushGroup("Speed", false);
				{
					bank->AddSlider("Move speed scaling",	&m_TranslationSpeedScaling, m_Metadata.m_MinSpeedScaling, m_Metadata.m_MaxSpeedScaling,
						m_Metadata.m_SpeedScalingStepSize);
					bank->AddSlider("Aim speed scaling",	&m_LookAroundSpeedScaling, m_Metadata.m_MinSpeedScaling, m_Metadata.m_MaxSpeedScaling,
						m_Metadata.m_SpeedScalingStepSize);

					bank->AddSlider("Aim Accel", &m_AimAcceleration, 0.0f, 2000.0f, 0.1f);
					bank->AddSlider("Aim Decel", &m_AimDeceleration, 0.0f, 2000.0f, 0.1f);
					bank->AddSlider("Position Accel", &m_TranslationAcceleration, 0.0f, 2000.0f, 0.1f);
					bank->AddSlider("Position Decel", &m_TranslationDeceleration, 0.0f, 2000.0f, 0.1f);
				}
				bank->PopGroup(); // Speed

				bank->AddToggle("Load camera-local IPLs",	&m_ShouldLoadNearbyIpls);

				bank->PushGroup("Depth of Field", false);
				{
					bank->AddSlider("Dof Near distance at default fov", &m_DofNearAtDefaultFov, 0.0f, 2000.0f, 0.1f);
					bank->AddSlider("Dof Far  distance at default fov", &m_DofFarAtDefaultFov,  0.0f, 3000.0f, 0.5f);
					bank->AddSlider("Dof Near distance at max zoom",    &m_DofNearAtMaxZoom,    0.0f, 2000.0f, 0.1f);
					bank->AddSlider("Dof Far  distance at max zoom",    &m_DofFarAtMaxZoom,     0.0f, 3000.0f, 0.5f);
					bank->AddSlider("Dof Near distance at min zoom",    &m_DofNearAtMinZoom,    0.0f, 2000.0f, 0.1f);
					bank->AddSlider("Dof Far  distance at min zoom",    &m_DofFarAtMinZoom,     0.0f, 3000.0f, 0.5f);
				}
				bank->PopGroup(); // Depth of Field
			}
			bank->PopGroup(); // Camera name
		}
	}
}

void camMarketingFreeCamera::AddWidgets(bkBank &UNUSED_PARAM(bank))
{
	bkBank* bank = BANKMGR.FindBank("Marketing Tools");
	if(bank)
	{
		bank->PushGroup("Depth of Field", false);
		{
			bank->AddCombo("Current lens", &ms_LensOption, NUM_LENS_OPTIONS, g_LensOptionNames, datCallback(CFA(OnLensChange)));
			bank->AddCombo("Focus mode", &ms_FocusMode, NUM_FOCUS_MODES, g_FocusModeNames);
			bank->AddToggle("Should keep focus entity bounds in focus", &ms_ShouldKeepFocusEntityBoundsInFocus);
			bank->AddSlider("Focus entity bound radius scaling", &ms_FocusEntityBoundRadiusScaling, 0.1f, 10.0f, 0.01f);
			bank->AddSlider("Max distance to test for entity", &ms_MaxDistanceToTestForEntity, 0.0f, 1000.0f, 0.1f);
			bank->AddSlider("Focus distance bias", &ms_DofSettings.m_FocusDistanceBias, -100.0f, 100.0f, 0.1f);
			bank->AddSlider("Focus distance change acceleration", &ms_FocalPointAcceleration, 0.0f, 1000.0f, 0.01f);
			bank->AddSlider("Max focus distance change speed", &ms_FocalPointMaxSpeed, 0.0f, 1000.0f, 0.01f);
			bank->AddSlider("Manual focus distance", &ms_ManualFocusDistance, g_MinManualFocusDistance, g_MaxManualFocusDistance, 0.01f);
			bank->AddSlider("Lens f-number", &ms_DofSettings.m_FNumberOfLens, 0.5f, 256.0f, 0.1f);
			bank->AddSlider("Focal length multiplier", &ms_DofSettings.m_FocalLengthMultiplier, 0.1f, 10.0f, 0.01f, datCallback(CFA(OnLensChange)));
			bank->AddToggle("Draw COC Overlay", &PostFX::g_DrawCOCOverlay);
		}
		bank->PopGroup(); // Depth of Field
	}
}

#endif // __BANK
