//
// camera/helpers/ControlHelper.cpp
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#include "camera/helpers/ControlHelper.h"

#include "math/vecmath.h"

#include "fwsys/timer.h"
#include "input/input.h"
#include "system/memops.h"

#include "camera/CamInterface.h"
#include "camera/cinematic/CinematicDirector.h"
#include "camera/gameplay/GameplayDirector.h"
#include "camera/helpers/Envelope.h"
#include "camera/system/CameraFactory.h"
#include "frontend/PauseMenu.h"
#include "frontend/ProfileSettings.h"
#include "modelinfo/VehicleModelInfoMods.h"
#include "modelinfo/VehicleModelInfoVariation.h"
#include "system/control.h"
#include "frontend/MobilePhone.h"

#include "control/replay/replay.h"

// CS: Includes for Haxx targeting code
#include "peds/Ped.h"
#include "scene/world/GameWorld.h"

CAMERA_OPTIMISATIONS()

INSTANTIATE_RTTI_CLASS(camControlHelper,0xD026E9A8)

REGISTER_TUNE_GROUP_INT(LOOK_BEHIND_HELD_TIME, 150);


#define CONTROLLER_ORIENTATION_ADD_TO_STICK_INPUT	(RSG_ORBIS)

static const s32	g_DefaultViewMode						= camControlHelperMetadataViewMode::THIRD_PERSON_MEDIUM;
static const s32	g_OriginalMaxControllerAimSensitivity	= 10;	//It is apparently not possible to query this limit from the UI Code at present.
static const s32	g_RevisedMaxControllerAimSensitivity	= 14;	//We are extending the upper limit of aim sensitivity post-launch.
static const s32	g_DefaultControllerAimSensitivity		= 5;	//NOTE: This is the midpoint of the original aim sensitivity range.
static const s32	g_DefaultControllerDeadZone				= 14;	//NOTE: This is the max of the original range.
static const s32	g_OriginalMaxControllerDeadZone			= 14;
static const s32	g_DefaultControllerAcceleration			= 7;	//NOTE: This is the midpoint of original range.
static const s32	g_OriginalMaxControllerAcceleration		= 14;	//NOTE: This is the max of the original range.
static const float	g_MinFovForInsideMpVehicle				= 35.0f;

const float			MOUSE_POWER_EXPONENT					= 1.3f;

EXTERN_PARSER_ENUM(camControlHelperMetadataViewMode__eViewModeContext);
EXTERN_PARSER_ENUM(camControlHelperMetadataViewMode__eViewMode);

Vector2		camControlHelper::ms_OverriddenZoomFactorLimits;
s32*		camControlHelper::ms_ContextViewModes				= NULL;
float		camControlHelper::ms_ZoomFactor						= 1.0f;
bool		camControlHelper::ms_ShouldOverrideZoomFactorLimits	= false;
bool		camControlHelper::ms_MaintainCurrentViewMode		= false;
s32			camControlHelper::ms_LastViewModeContext			= -1;
s32			camControlHelper::ms_LastSetViewMode				= -1;
s32			camControlHelper::ms_ContextToForceFirstPersonCamera= -1;
s32			camControlHelper::ms_ContextToForceThirdPersonCamera= -1;
s32			camControlHelper::ms_ThirdPersonViewModeToForce		= -1;
bool		camControlHelper::ms_FirstPersonCancelledForCinematicCamera = false;

#if __BANK
float		camControlHelper::ms_LookAroundSpeedScalar			= 1.0f; 
bool		camControlHelper::ms_UseOverriddenLookAroundHelper	= false; 
camControlHelperMetadataLookAround camControlHelper::ms_OverriddenLookAroundHelper;

s32			camControlHelper::ms_DebugAimSensitivitySetting			= -1;
s32			camControlHelper::ms_DebugLookAroundSensitivitySetting	= -1;
s32			camControlHelper::ms_DebugRequestedContextIndex					= -1;
s32			camControlHelper::ms_DebugRequestedViewModeForContext			= -1;
bool		camControlHelper::ms_DebugWasViewModeForContextSetThisUpdate	= false;
#endif // __BANK

#if RSG_ORBIS
static BankFloat c_fMotionDeadzone = 1.0f; // degrees;
static BankFloat c_fMotionVerticalScale = 0.00f;
static BankFloat c_fMotionHorizontalScale = 0.00f;
#endif

NOSTRIP_PC_PARAM(mouse_sensitivity_override, "Mouse sensitivity override");

void camControlHelper::InitClass()
{
	ms_OverriddenZoomFactorLimits.Zero();

	if(camControlHelperMetadataViewMode::eViewModeContext_NUM_ENUMS > 0)
	{
		ms_ContextViewModes = rage_new s32[camControlHelperMetadataViewMode::eViewModeContext_NUM_ENUMS];
		if(ms_ContextViewModes)
		{
			//TODO: Store a default view mode for each context in the camera metadata.
			for(s32 i=0; i<camControlHelperMetadataViewMode::eViewModeContext_NUM_ENUMS; i++)
			{
				ms_ContextViewModes[i] = g_DefaultViewMode;
			}

			ms_ContextViewModes[camControlHelperMetadataViewMode::ON_BIKE]		= camControlHelperMetadataViewMode::THIRD_PERSON_NEAR;
			ms_ContextViewModes[camControlHelperMetadataViewMode::IN_AIRCRAFT]	= camControlHelperMetadataViewMode::THIRD_PERSON_FAR;
			ms_ContextViewModes[camControlHelperMetadataViewMode::IN_SUBMARINE]	= camControlHelperMetadataViewMode::THIRD_PERSON_NEAR;
		}
	}
}

void camControlHelper::ShutdownClass()
{
	if(ms_ContextViewModes)
	{
		delete[] ms_ContextViewModes;
		ms_ContextViewModes = NULL;
	}
}

s32 camControlHelper::GetViewModeForContextInternal(s32 contextIndex)
{
	const s32* viewModeForContext	= GetViewModeForContextWithValidation(contextIndex);
	s32 viewMode					= viewModeForContext ? *viewModeForContext : g_DefaultViewMode;
	return viewMode;
}

s32 camControlHelper::GetViewModeForContext(s32 contextIndex)
{
	s32 viewMode = GetViewModeForContextInternal(contextIndex);

#if FPS_MODE_SUPPORTED
	if(ms_ContextToForceFirstPersonCamera != -1 && viewMode != camControlHelperMetadataViewMode::FIRST_PERSON)
	{
		viewMode = camControlHelperMetadataViewMode::FIRST_PERSON;
	}
	if(ms_ContextToForceThirdPersonCamera != -1 && viewMode == camControlHelperMetadataViewMode::FIRST_PERSON)
	{
		viewMode = ms_ThirdPersonViewModeToForce;
	}
#endif

	return viewMode;
}

bool camControlHelper::SetViewModeForContext(s32 contextIndex, s32 viewMode)
{
	if(!cameraVerifyf((viewMode >= 0) && (viewMode <= camControlHelperMetadataViewMode::eViewMode_MAX_VALUE), "Invalid camera view mode (%d)", viewMode))
	{
		return false;
	}

#if __BANK
	ms_DebugWasViewModeForContextSetThisUpdate	= true;
	ms_DebugRequestedContextIndex				= contextIndex;
	ms_DebugRequestedViewModeForContext			= viewMode;
	cameraDebugf2("View-mode update: SetViewModeForContext %s = %s", (contextIndex >= 0 && contextIndex < camControlHelperMetadataViewMode::eViewModeContext_NUM_ENUMS) ? PARSER_ENUM(camControlHelperMetadataViewMode__eViewModeContext).m_Names[contextIndex] : "UNKNOWN", PARSER_ENUM(camControlHelperMetadataViewMode__eViewMode).m_Names[viewMode]);
#endif // __BANK

	s32* viewModeForContext	= GetViewModeForContextWithValidation(contextIndex);
	const bool isValid		= (viewModeForContext != NULL);
	if(isValid)
	{
		*viewModeForContext = viewMode;

	#if FPS_MODE_SUPPORTED
		if(ms_ContextToForceFirstPersonCamera != -1 && viewMode != camControlHelperMetadataViewMode::FIRST_PERSON)
		{
			ms_ContextToForceFirstPersonCamera = -1;
			ms_FirstPersonCancelledForCinematicCamera = (viewMode == camControlHelperMetadataViewMode::CINEMATIC);
		}
		if(ms_ContextToForceThirdPersonCamera != -1)
		{
			if(viewMode == camControlHelperMetadataViewMode::FIRST_PERSON)
				ms_ContextToForceThirdPersonCamera = -1;
			else if(viewMode != camControlHelperMetadataViewMode::CINEMATIC)
				ms_ThirdPersonViewModeToForce = viewMode;
		}
	#endif
	}

	return isValid;
}

s32* camControlHelper::GetViewModeForContextWithValidation(s32 contextIndex)
{
	s32* viewMode = NULL;

	if(cameraVerifyf(ms_ContextViewModes, "No camera view mode contexts have been defined"))
	{
		if(cameraVerifyf((contextIndex >= 0) && (contextIndex <= camControlHelperMetadataViewMode::eViewModeContext_MAX_VALUE),
			"Invalid camera view mode context (%d)", contextIndex))
		{
			viewMode = ms_ContextViewModes + contextIndex;
		}
	}

	return viewMode;
}

void camControlHelper::StoreContextViewModesInMap(atBinaryMap<atHashValue, atHashValue>& map)
{
	for(s32 contextIndex=0; contextIndex<camControlHelperMetadataViewMode::eViewModeContext_NUM_ENUMS; contextIndex++)
	{
		const char* contextName = PARSER_ENUM(camControlHelperMetadataViewMode__eViewModeContext).m_Names[contextIndex];
		if(contextName)
		{
			atHashValue contextNameHash(contextName);

			if(cameraVerifyf(!map.Has(contextNameHash), "Found a duplicate name hash for a camera view mode context (name: %s, hash: %u)",
				contextName, contextNameHash.GetHash()))
			{
				const s32 viewMode			= GetViewModeForContextInternal(contextIndex);
				const char* viewModeName	= PARSER_ENUM(camControlHelperMetadataViewMode__eViewMode).m_Names[viewMode];
				if(viewModeName)
				{
					atHashValue viewModeNameHash(viewModeName);

					map.FastInsert(contextNameHash, viewModeNameHash);
				}
			}
		}
	}

	map.FinishInsertion();
}

void camControlHelper::RestoreContextViewModesFromMap(const atBinaryMap<atHashValue, atHashValue>& map)
{
	atBinaryMap<s32, atHashValue> viewModeMap;

	for(s32 viewModeIndex=0; viewModeIndex<camControlHelperMetadataViewMode::eViewMode_NUM_ENUMS; viewModeIndex++)
	{
		const char* viewModeName = PARSER_ENUM(camControlHelperMetadataViewMode__eViewMode).m_Names[viewModeIndex];
		if(viewModeName)
		{
			atHashValue viewModeNameHash(viewModeName);

			if(cameraVerifyf(!viewModeMap.Has(viewModeNameHash), "Found a duplicate name hash for a camera view mode (name: %s, hash: %u)",
				viewModeName, viewModeNameHash.GetHash()))
			{
				viewModeMap.FastInsert(viewModeNameHash, viewModeIndex);
			}
		}
	}

	viewModeMap.FinishInsertion();

	for(s32 contextIndex=0; contextIndex<camControlHelperMetadataViewMode::eViewModeContext_NUM_ENUMS; contextIndex++)
	{
		const char* contextName = PARSER_ENUM(camControlHelperMetadataViewMode__eViewModeContext).m_Names[contextIndex];
		if(contextName)
		{
			atHashValue contextNameHash(contextName);

			const atHashValue* viewModeNameHash = map.SafeGet(contextNameHash);
			if(viewModeNameHash)
			{
				const s32* viewMode = viewModeMap.SafeGet(*viewModeNameHash);
				if(viewMode)
				{
					ms_ContextViewModes[contextIndex] = *viewMode;
				}
			}
		}
	}
}

//static
u32 camControlHelper::GetBonnetCameraToggleTime()
{
	return (u32)(LOOK_BEHIND_HELD_TIME);
}

void camControlHelper::OverrideZoomFactorLimits(float minZoomFactor, float maxZoomFactor)
{
	const float minZoomFactorToApply = Max(minZoomFactor, 1.0f);
	const float maxZoomFactorToApply = Max(maxZoomFactor, minZoomFactor);

	ms_OverriddenZoomFactorLimits.Set(minZoomFactorToApply, maxZoomFactorToApply);
	ms_ShouldOverrideZoomFactorLimits = true;
}

float camControlHelper::ComputeOffsetBasedOnAcceleratedInput(float input, float acceleration, float deceleration, float maxSpeed, float& speed,
															 float timeStep, bool shouldResetSpeedOnInputFlip)
{
	//NOTE: Input has already been subjected to a dead-zone to filter out very small values.
	if(shouldResetSpeedOnInputFlip && (input != 0.0f) && !SameSign(input, speed))
	{
		//Kill the speed instantly when the direction of movement reverses, in order to avoid lag due to deceleration.
		speed = 0.0f;
	}

	float absSpeed				= Abs(speed);
	float absMaxSpeed			= Abs(input * maxSpeed);
	if(absMaxSpeed >= absSpeed)
	{
		//Accelerate.
		float speedDelta		= acceleration * timeStep;
		speed					+= Sign(input) * speedDelta;
		speed					= Clamp(speed, -absMaxSpeed, absMaxSpeed); //Don't overshoot the max speed.
	}
	else
	{
		//Decelerate.
		float speedDelta		= deceleration * timeStep;
		float maxAbsSpeedDelta	= absSpeed - absMaxSpeed;
		speedDelta				= Clamp(speedDelta, -maxAbsSpeedDelta, maxAbsSpeedDelta); //Don't overshoot the max speed.
		speed					-= speedDelta * Sign(speed);
	}

	float offset				= speed * timeStep;

	return offset;
}

camControlHelper::camControlHelper(const camBaseObjectMetadata& metadata)
: camBaseObject(metadata)
, m_Metadata(static_cast<const camControlHelperMetadata&>(metadata))
, m_ViewModeBlendEnvelope(NULL)
, m_LookAroundInputEnvelope(NULL)
, m_ViewModeContext(-1)
, m_ViewMode(-1)
, m_LimitFirstPersonAimSensitivityThisUpdate(-1)
, m_UseLeftStickForLookInputThisUpdate(false)
, m_UseBothSticksForLookInputThisUpdate(false)
{
	const camEnvelopeMetadata* envelopeMetadata =
		camFactory::FindObjectMetadata<camEnvelopeMetadata>(m_Metadata.m_ViewModes.m_ViewModeBlendEnvelopeRef.GetHash());
	if(envelopeMetadata)
	{
		m_ViewModeBlendEnvelope = camFactory::CreateObject<camEnvelope>(*envelopeMetadata);

		cameraAssertf(m_ViewModeBlendEnvelope,
			"A camera control helper (name: %s, hash: %u) failed to create a view mode blend envelope (name: %s, hash: %u)",
			GetName(), GetNameHash(), SAFE_CSTRING(envelopeMetadata->m_Name.GetCStr()),
			envelopeMetadata->m_Name.GetHash());
	}

	envelopeMetadata =
		camFactory::FindObjectMetadata<camEnvelopeMetadata>(m_Metadata.m_LookAround.m_InputEnvelopeRef.GetHash());
	if(envelopeMetadata)
	{
 		m_LookAroundInputEnvelope = camFactory::CreateObject<camEnvelope>(*envelopeMetadata);

		cameraAssertf(m_LookAroundInputEnvelope,
			"A camera control helper (name: %s, hash: %u) failed to create a look-around input envelope (name: %s, hash: %u)",
			GetName(), GetNameHash(), SAFE_CSTRING(envelopeMetadata->m_Name.GetCStr()),
			envelopeMetadata->m_Name.GetHash());
	}

	sysMemSet(m_ViewModeSourceBlendLevels, 0, camControlHelperMetadataViewMode::eViewMode_NUM_ENUMS * sizeof(float));
	sysMemSet(m_ViewModeBlendLevels, 0, camControlHelperMetadataViewMode::eViewMode_NUM_ENUMS * sizeof(float));

	Reset();
}

camControlHelper::~camControlHelper()
{
	if(m_ViewModeBlendEnvelope)
	{
		delete m_ViewModeBlendEnvelope;
	}

	if(m_LookAroundInputEnvelope)
	{
		delete m_LookAroundInputEnvelope;
	}
}

const camControlHelperMetadataViewModeSettings* camControlHelper::GetViewModeSettings(s32 mode) const
{
	const camControlHelperMetadataViewModeSettings* settings = NULL;
	if (mode < 0 || mode >= camControlHelperMetadataViewMode::eViewMode_NUM_ENUMS)
	{
		mode = m_ViewMode;
	}

	if(cameraVerifyf((mode >= 0),
		"View mode settings were queried on a camera control helper (name: %s, hash: %u) prior to view mode init",
		GetName(), GetNameHash()))
	{
		settings = &m_Metadata.m_ViewModes.m_Settings[mode];
	}

	return settings;
}

bool camControlHelper::IsViewModeBlending() const
{
	const bool isBlending = m_ViewModeBlendEnvelope && m_ViewModeBlendEnvelope->IsActive();

	return isBlending;
}

float camControlHelper::GetOverallViewModeBlendLevel() const
{
	float blendLevel = 1.0f;

	if(m_ViewModeBlendEnvelope && m_ViewModeBlendEnvelope->IsActive())
	{
		blendLevel = m_ViewModeBlendEnvelope->GetLevel();
	}

	return blendLevel;
}

float camControlHelper::ComputeMaxLookAroundOrientationSpeed() const
{
	const float aimSensitivityScaling = GetAimSensitivityScaling();

	//NOTE: We factor in the aim sensitivity, as this has a bearing on the effective look-around orientation speed.
	float speed	= Max(GetLookAroundMetadata().m_MaxHeadingSpeed * DtoR, GetLookAroundMetadata().m_MaxPitchSpeed * DtoR);
	speed		*= aimSensitivityScaling;

	return speed;
}

bool camControlHelper::IsUsingZoomInput() const
{
	return m_Metadata.m_Zoom.m_ShouldUseZoomInput;
}

float camControlHelper::GetZoomFov() const
{
	const float fov = m_Metadata.m_Zoom.m_MaxFov / ms_ZoomFactor;

	return fov;
}

bool camControlHelper::IsUsingLookBehindInput() const
{
	const bool isUsingLookBehindInput	= m_Metadata.m_ShouldUseLookBehindInput && m_IsLookBehindInputActive &&
											!m_ShouldIgnoreLookBehindInputThisUpdate;
	return isUsingLookBehindInput;
}

void camControlHelper::SetAccurateModeState(bool state)
{
	if(m_Metadata.m_ShouldUseAccurateModeInput)
	{
		m_IsInAccurateMode = state;
	}
}

void camControlHelper::CloneSpeeds(const camControlHelper& sourceControlHelper)
{
	m_LookAroundSpeed		= sourceControlHelper.GetLookAroundSpeed();
	m_ZoomFactorSpeed		= sourceControlHelper.GetZoomFactorSpeed();
}

void camControlHelper::Update(const CEntity& attachParent, bool isEarlyUpdate)
{
	if(m_ViewModeContext < 0)
	{
		InitViewModeContext(attachParent);
	}

	if(m_ViewMode < 0)
	{
		InitViewMode();
	}

	CControl* control = camInterface::GetGameplayDirector().GetActiveControl();
	if(control)
	{
		if(!m_SkipControlHelperThisUpdate)
		{
			UpdateLookBehindControl(*control, isEarlyUpdate);
			UpdateViewModeControl(*control, attachParent);
			UpdateLookAroundControl(*control);
			UpdateZoomControl(*control);
			UpdateAccurateModeControl(*control);
		}
		else
		{
			// Only reset the input, leave all other state as is. (so input acceleration is not modified)
			m_LookAroundHeadingOffset = 0.0f;
			m_LookAroundPitchOffset = 0.0f;
		}
	}
	else
	{
		Reset(); //Clear the state.
	}

	ms_LastViewModeContext = m_ViewModeContext;

	m_UseLeftStickForLookInputThisUpdate = false;
	m_UseBothSticksForLookInputThisUpdate = false;

	m_ShouldForceAimSensitivityThisUpdate = false;
	m_ShouldForceDefaultLookAroundSensitivityThisUpdate = false;

	m_SkipControlHelperThisUpdate = false;

	m_LimitFirstPersonAimSensitivityThisUpdate = -1;
}

void camControlHelper::InitViewModeContext(const CEntity& attachParent)
{
	m_ViewModeContext = m_Metadata.m_ViewModes.m_Context;

	// For turrets, attachParent can be vehicle in the case of cinematic POV turret camera.
	if( (camInterface::GetGameplayDirector().IsUsingVehicleTurret(true) && attachParent.GetIsTypePed())
		FPS_MODE_SUPPORTED_ONLY(|| (camInterface::GetGameplayDirector().IsUsingVehicleTurret(false) && attachParent.GetIsTypeVehicle())) )
	{
		m_ViewModeContext = camControlHelperMetadataViewMode::IN_TURRET;
	}
	else if((m_ViewModeContext == camControlHelperMetadataViewMode::IN_VEHICLE) && attachParent.GetIsTypeVehicle())
	{
		//Assign a vehicle-class-specific view mode context where appropriate for our attach parent.
		const CVehicle& attachVehicle = static_cast<const CVehicle&>(attachParent);
		if(attachVehicle.InheritsFromBike() || attachVehicle.InheritsFromQuadBike() || attachVehicle.InheritsFromAmphibiousQuadBike())
		{
			m_ViewModeContext = camControlHelperMetadataViewMode::ON_BIKE;
		}
		else if(attachVehicle.InheritsFromBoat() || attachVehicle.GetIsJetSki() || attachVehicle.InheritsFromAmphibiousAutomobile() || attachVehicle.InheritsFromAmphibiousQuadBike())
		{
			m_ViewModeContext = camControlHelperMetadataViewMode::IN_BOAT;
		}
		else if(attachVehicle.InheritsFromHeli())
		{
			m_ViewModeContext = camControlHelperMetadataViewMode::IN_HELI;
		}
		else if(attachVehicle.GetIsAircraft() || attachVehicle.InheritsFromBlimp())
		{
			m_ViewModeContext = camControlHelperMetadataViewMode::IN_AIRCRAFT;
		}
		else if(attachVehicle.InheritsFromSubmarine())
		{
			m_ViewModeContext = camControlHelperMetadataViewMode::IN_SUBMARINE;
		}
	}
}

s32 camControlHelper::ComputeViewModeContextForVehicle(u32 ModelHashKey)
{
	s32 viewModeContext = -1; 

	CBaseModelInfo* pModelInfo = CModelInfo::GetBaseModelInfoFromHashKey(ModelHashKey, NULL);
	
	if(pModelInfo && pModelInfo->GetModelType()==MI_TYPE_VEHICLE)
	{
		CVehicleModelInfo* pVehicleModel = static_cast<CVehicleModelInfo*>(pModelInfo); 

		viewModeContext = camControlHelperMetadataViewMode::IN_VEHICLE;

		if(pVehicleModel->GetIsBike() || pVehicleModel->GetIsQuadBike() || pVehicleModel->GetIsAmphibiousQuad())
		{
			viewModeContext = camControlHelperMetadataViewMode::ON_BIKE;
		}
		else if(pVehicleModel->GetIsBoat() || pVehicleModel->GetIsJetSki())
		{
			viewModeContext = camControlHelperMetadataViewMode::IN_BOAT;
		}
		else if(pVehicleModel->GetIsHeli() || pVehicleModel->GetVehicleType() == VEHICLE_TYPE_BLIMP)
		{
			viewModeContext = camControlHelperMetadataViewMode::IN_HELI;
		}
		else if(pVehicleModel->GetIsRotaryAircraft() || pVehicleModel->GetIsPlane()  || pVehicleModel->GetVehicleType() == VEHICLE_TYPE_BLIMP)
		{
			viewModeContext = camControlHelperMetadataViewMode::IN_AIRCRAFT;
		}
		else if(pVehicleModel->GetIsSubmarine())
		{
			viewModeContext = camControlHelperMetadataViewMode::IN_SUBMARINE;
		}
	}
	return viewModeContext;
}

s32 camControlHelper::ComputeViewModeContextForVehicle(const CVehicle& vehicle)
{
	s32 viewModeContext = camControlHelperMetadataViewMode::IN_VEHICLE;

	if(vehicle.InheritsFromBike() || vehicle.InheritsFromQuadBike() || vehicle.InheritsFromAmphibiousQuadBike())
	{
		viewModeContext = camControlHelperMetadataViewMode::ON_BIKE;
	}
	else if(vehicle.InheritsFromBoat() || vehicle.GetIsJetSki() || vehicle.InheritsFromAmphibiousAutomobile() || vehicle.InheritsFromAmphibiousQuadBike())
	{
		viewModeContext = camControlHelperMetadataViewMode::IN_BOAT;
	}
	else if(vehicle.InheritsFromHeli())
	{
		viewModeContext = camControlHelperMetadataViewMode::IN_HELI;
	}
	else if(vehicle.GetIsAircraft() || vehicle.InheritsFromBlimp())
	{
		viewModeContext = camControlHelperMetadataViewMode::IN_AIRCRAFT;
	}
	else if(vehicle.InheritsFromSubmarine())
	{
		viewModeContext = camControlHelperMetadataViewMode::IN_SUBMARINE;
	}

	return viewModeContext;
}

void camControlHelper::InitViewMode()
{
	s32 viewMode = GetViewModeForContextInternal(m_ViewModeContext);

	ValidateViewMode(viewMode);

	m_ViewMode = viewMode;
	m_ViewModeBlendLevels[m_ViewMode] = 1.0f;

#if FPS_MODE_SUPPORTED
	// Only update the context to override first person camera for if the view mode context is changing.
	if( ms_MaintainCurrentViewMode )
	{
		if( ms_LastViewModeContext != m_ViewModeContext )
		{
			bool bLastViewModeWasFirstPerson = ms_LastSetViewMode == (s32)camControlHelperMetadataViewMode::FIRST_PERSON ||
												ms_ContextToForceFirstPersonCamera != -1 ||
												(ms_FirstPersonCancelledForCinematicCamera && m_ViewMode != (s32)camControlHelperMetadataViewMode::CINEMATIC) ||
												(ms_LastSetViewMode == -1 && camInterface::IsRenderingFirstPersonCamera());

			if(m_ViewMode != camControlHelperMetadataViewMode::FIRST_PERSON && bLastViewModeWasFirstPerson)
			{
				const bool isModeValid = ComputeIsViewModeValid( (s32)camControlHelperMetadataViewMode::FIRST_PERSON );
				if(isModeValid)
				{
					ms_ContextToForceFirstPersonCamera = m_ViewModeContext;
					ms_ContextToForceThirdPersonCamera = -1;
					m_ViewMode = camControlHelperMetadataViewMode::FIRST_PERSON;
				}
			}
			else if(m_ViewMode == camControlHelperMetadataViewMode::FIRST_PERSON && !bLastViewModeWasFirstPerson)
			{
				s32 lastViewMode = ms_LastSetViewMode;
				if(ms_LastSetViewMode < 0)
					lastViewMode = (ms_LastViewModeContext != -1) ? GetViewModeForContextInternal(ms_LastViewModeContext) : g_DefaultViewMode;

				if(lastViewMode == camControlHelperMetadataViewMode::FIRST_PERSON)
				{
					lastViewMode = g_DefaultViewMode;
				}

				if(!(lastViewMode == camControlHelperMetadataViewMode::CINEMATIC && (m_ViewModeContext >= camControlHelperMetadataViewMode::IN_VEHICLE && m_ViewModeContext <= camControlHelperMetadataViewMode::IN_TURRET)))
				{
					const bool isModeValid = ComputeIsViewModeValid( (s32)lastViewMode);
					if(isModeValid)
					{
						ms_ContextToForceFirstPersonCamera = -1;
						ms_ContextToForceThirdPersonCamera = m_ViewModeContext;
						ms_ThirdPersonViewModeToForce = lastViewMode;
						m_ViewMode = lastViewMode;
					}
					else
					{
						ms_ContextToForceFirstPersonCamera = -1;
						ms_ContextToForceThirdPersonCamera = m_ViewModeContext;
						ms_ThirdPersonViewModeToForce = g_DefaultViewMode;
						m_ViewMode = g_DefaultViewMode;
					}
				}
			}

			// Even if we don't need to set it, treat as if we are overriding view mode so we know that a context is being overridden.
			if(ms_ContextToForceFirstPersonCamera == -1 && ms_ContextToForceThirdPersonCamera != -1 && m_ViewMode == camControlHelperMetadataViewMode::FIRST_PERSON)
			{
				ms_ContextToForceFirstPersonCamera = m_ViewModeContext;
			}
			else if(ms_ContextToForceFirstPersonCamera == -1 && ms_ContextToForceThirdPersonCamera == -1 && m_ViewMode != camControlHelperMetadataViewMode::FIRST_PERSON)
			{
				ms_ContextToForceThirdPersonCamera = m_ViewModeContext;
				ms_ThirdPersonViewModeToForce = m_ViewMode;
			}
		}
		else
		{
			// Context did not change, ensure that the proper view mode is setup.
			// Have to check if any context is overridden because of how animated camera in vehicles are handled.
			if(ms_ContextToForceFirstPersonCamera != -1 && m_ViewMode != camControlHelperMetadataViewMode::FIRST_PERSON)
			{
				const bool isModeValid = ComputeIsViewModeValid( (s32)camControlHelperMetadataViewMode::FIRST_PERSON );
				if(isModeValid)
				{
					m_ViewMode = camControlHelperMetadataViewMode::FIRST_PERSON;
					ms_ContextToForceFirstPersonCamera = m_ViewModeContext;
				}
				else
				{
					ms_ContextToForceFirstPersonCamera = -1;
				}
			}

			if(ms_ContextToForceThirdPersonCamera != -1 && m_ViewMode == camControlHelperMetadataViewMode::FIRST_PERSON)
			{
				s32 viewModeToUse = GetViewModeForContextInternal(m_ViewModeContext);
				if(viewModeToUse == (s32)camControlHelperMetadataViewMode::FIRST_PERSON)
					viewModeToUse = g_DefaultViewMode;
				const bool isModeValid = ComputeIsViewModeValid( viewModeToUse );
				if(isModeValid)
				{
					m_ViewMode = viewModeToUse;
					ms_ContextToForceThirdPersonCamera = m_ViewModeContext;
				}
				else
				{
					ms_ContextToForceThirdPersonCamera = -1;
				}
			}
		}

		ms_FirstPersonCancelledForCinematicCamera &= (viewMode == camControlHelperMetadataViewMode::CINEMATIC);
	}
	else
	{
		ms_ContextToForceFirstPersonCamera = -1;
		ms_ContextToForceThirdPersonCamera = -1;
		ms_FirstPersonCancelledForCinematicCamera = false;
	}
#endif
}

void camControlHelper::UpdateLookBehindControl(const CControl& control, bool isEarlyUpdate)
{
	m_WasLookingBehind = m_IsLookingBehind;

	const bool isUsingLookBehindInput = IsUsingLookBehindInput();
	if(isUsingLookBehindInput)
	{
		const bool bIsInFlyingVehicle = (m_ViewModeContext == camControlHelperMetadataViewMode::IN_AIRCRAFT) ||
										(m_ViewModeContext == camControlHelperMetadataViewMode::IN_HELI);

		const bool bIsInVehicle =		(m_ViewModeContext == camControlHelperMetadataViewMode::IN_VEHICLE) ||
										(m_ViewModeContext == camControlHelperMetadataViewMode::ON_BIKE) ||
										(m_ViewModeContext == camControlHelperMetadataViewMode::IN_BOAT) ||
										(m_ViewModeContext == camControlHelperMetadataViewMode::IN_SUBMARINE) ||
										bIsInFlyingVehicle;

		bool bIsFlyingVehicleArmed = false;
		if(bIsInFlyingVehicle)
		{
			const CPed* pPlayer = CGameWorld::FindLocalPlayer();
			if(pPlayer && pPlayer->GetIsDrivingVehicle() && !pPlayer->GetIsDeadOrDying())
			{
				const CWeaponInfo *pCurrentWeaponInfo = pPlayer->GetEquippedWeaponInfo();
				bIsFlyingVehicleArmed = pCurrentWeaponInfo && pCurrentWeaponInfo->GetIsVehicleWeapon();
			}
		}

		const ioValue& lookBehindControl = bIsInVehicle ? control.GetVehicleLookBehind() : control.GetPedLookBehind();

        const bool bIsOnFoot = m_ViewModeContext == camControlHelperMetadataViewMode::ON_FOOT;
        const bool bIsInMultiplayer = NetworkInterface::IsGameInProgress();
		const bool bIsSpecialAbilityRequested = control.GetPedSpecialAbility().IsDown();
        const bool bPreventLookBehindForSpecialAbility = bIsOnFoot && bIsInMultiplayer && bIsSpecialAbilityRequested;

		if( !bPreventLookBehindForSpecialAbility && 
            ((!bIsFlyingVehicleArmed && lookBehindControl.IsDown() && lookBehindControl.WasDown()) ||
			(bIsFlyingVehicleArmed && lookBehindControl.HistoryHeldDown(LOOK_BEHIND_HELD_TIME))) )
		{
			m_IsLookingBehind = true;
			m_TimeReleasedLookBehindInput = 0;
		}
		else
		{
			if(m_IsLookingBehind)
			{
				if(m_TimeReleasedLookBehindInput == 0)
				{
					m_TimeReleasedLookBehindInput = fwTimer::GetTimeInMilliseconds();	
				}

				if(fwTimer::GetTimeInMilliseconds() >= (m_TimeReleasedLookBehindInput + m_Metadata.m_LookBehindOutroTimeMS) )
				{
					m_IsLookingBehind = false;
				}
			}
		}
	}

	if (!isEarlyUpdate)
	{
		m_ShouldIgnoreLookBehindInputThisUpdate = false;
	}
}

bool camControlHelper::WillBeLookingBehindThisUpdate(const CControl& control) const
{
	const bool isUsingLookBehindInput = IsUsingLookBehindInput();
	if (!isUsingLookBehindInput)
	{
		return m_IsLookingBehind;
	}

	bool isLookingBehind = m_IsLookingBehind;

	const bool bIsInFlyingVehicle = (m_ViewModeContext == camControlHelperMetadataViewMode::IN_AIRCRAFT) ||
									(m_ViewModeContext == camControlHelperMetadataViewMode::IN_HELI);

	const bool bIsInVehicle =		(m_ViewModeContext == camControlHelperMetadataViewMode::IN_VEHICLE) ||
									(m_ViewModeContext == camControlHelperMetadataViewMode::ON_BIKE) ||
									(m_ViewModeContext == camControlHelperMetadataViewMode::IN_BOAT) ||
									(m_ViewModeContext == camControlHelperMetadataViewMode::IN_SUBMARINE) ||
									bIsInFlyingVehicle;

	bool bIsFlyingVehicleArmed = false;
	if(bIsInFlyingVehicle)
	{
		const CPed* pPlayer = CGameWorld::FindLocalPlayer();
		if(pPlayer && pPlayer->GetIsDrivingVehicle() && !pPlayer->GetIsDeadOrDying())
		{
			const CWeaponInfo *pCurrentWeaponInfo = pPlayer->GetEquippedWeaponInfo();
			bIsFlyingVehicleArmed = pCurrentWeaponInfo && pCurrentWeaponInfo->GetIsVehicleWeapon();
		}
	}

	const ioValue &lookBehindControl = bIsInVehicle ? control.GetVehicleLookBehind() : control.GetPedLookBehind();
	if( (!bIsFlyingVehicleArmed && lookBehindControl.IsDown() && lookBehindControl.WasDown()) ||
		( bIsFlyingVehicleArmed && lookBehindControl.HistoryHeldDown(LOOK_BEHIND_HELD_TIME)) )
	{
		isLookingBehind = true;
	}
	else if(isLookingBehind)
	{
		u32 timeReleasedLookBehindInput = m_TimeReleasedLookBehindInput;
		if(timeReleasedLookBehindInput == 0)
		{
			timeReleasedLookBehindInput = fwTimer::GetTimeInMilliseconds();	
		}

		if(fwTimer::GetTimeInMilliseconds() >= (timeReleasedLookBehindInput + m_Metadata.m_LookBehindOutroTimeMS) )
		{
			isLookingBehind = false;
		}
	}

	return isLookingBehind;
}

void camControlHelper::UpdateViewModeControl(const CControl& control, const CEntity& FPS_MODE_SUPPORTED_ONLY(attachParent))
{
	const s32 initialViewMode = m_ViewMode;

#if FPS_MODE_SUPPORTED
	// TODO: rename PREF_FPS_PERSISTANT_VIEW? maybe someday...
	const bool bAllowIndependentViewModes = (CPauseMenu::GetMenuPreference( PREF_FPS_PERSISTANT_VIEW ) == TRUE);
	if(!bAllowIndependentViewModes)
	{
		ms_MaintainCurrentViewMode = true;
		if( m_ViewModeContext == camControlHelperMetadataViewMode::IN_TURRET &&
			m_ViewModeContext != m_Metadata.m_ViewModes.m_Context )
		{
			if( (camInterface::GetGameplayDirector().IsUsingVehicleTurret(true) && attachParent.GetIsTypePed()) ||
				(camInterface::GetGameplayDirector().IsUsingVehicleTurret(false) && attachParent.GetIsTypeVehicle()) )
			{
			}
			else
			{
				// If we are using turret context, but player is no longer in turret seat, update view mode context.
				InitViewModeContext(attachParent);
			}
		}
	}
	else if(bAllowIndependentViewModes)
	{
		ms_MaintainCurrentViewMode = false;
		ms_ContextToForceFirstPersonCamera = -1;
		ms_ContextToForceThirdPersonCamera = -1;
		ms_FirstPersonCancelledForCinematicCamera = false;
	}
#endif

	const bool shouldUpdateViewMode = ComputeShouldUpdateViewMode();
	//m_IsOverridingViewModeForSpectatorModeThisUpdate allow the cover cam to drive a special selection of the spectator cam.
	if(shouldUpdateViewMode || m_IsOverridingViewModeForSpectatorModeThisUpdate)
	{
		s32* viewModeForContext = GetViewModeForContextWithValidation(m_ViewModeContext);
		if(!viewModeForContext)
		{
			cameraDebugf3("View-mode update: bypassing (invalid view mode for context: %s)", (m_ViewModeContext >= 0 && m_ViewModeContext < camControlHelperMetadataViewMode::eViewModeContext_NUM_ENUMS) ? PARSER_ENUM(camControlHelperMetadataViewMode__eViewModeContext).m_Names[m_ViewModeContext] : "UNKNOWN");
		}
		else
		{
			if(m_ShouldIgnoreViewModeInputThisUpdate)
			{
				cameraDebugf3("View-mode update: bypassing (m_ShouldIgnoreViewModeInputThisUpdate)");
			}

			s32 desiredViewMode					= *viewModeForContext;
		#if FPS_MODE_SUPPORTED
			if(ms_ContextToForceFirstPersonCamera == m_ViewModeContext || ms_ContextToForceThirdPersonCamera == m_ViewModeContext)
			{
				// When forcing first person camera, m_ViewMode can be different than the one set for the current context.
				desiredViewMode					= m_ViewMode;
			}
		#endif

			bool bIgnoreInput = false;
#if FPS_MODE_SUPPORTED
			CPed* pLocalPlayerPed = CGameWorld::FindLocalPlayer();

			bool bCameraDisabled = pLocalPlayerPed && pLocalPlayerPed->GetPlayerResetFlag(CPlayerResetFlags::PRF_DISABLE_CAMERA_VIEW_MODE_CYCLE);
			if(bCameraDisabled)
			{
				m_LastTimeViewModeControlDisabled = fwTimer::GetTimeInMilliseconds();
			}

			static dev_u32 s_ViewModeDisabledTimeMs = 300;
			if((m_LastTimeViewModeControlDisabled != 0) && 
				fwTimer::GetTimeInMilliseconds() < (m_LastTimeViewModeControlDisabled + s_ViewModeDisabledTimeMs))
			{
				bIgnoreInput = true;
			}
#endif // FPS_MODE_SUPPORTED

			//NOTE: We now use a short-press/tap view mode input in both SP and MP. This was required only to support the long-press/hold input used by the
			//'interaction menu' in MP at launch.
			bool isNextCameraModePressed	=	!bIgnoreInput && !m_ShouldIgnoreViewModeInputThisUpdate && control.GetNextCameraMode().IsEnabled() && control.GetNextCameraMode().IsReleased()
											&&	!control.GetNextCameraMode().IsReleasedAfterHistoryHeldDown(m_Metadata.m_MaxDurationForMultiplayerViewModeActivation)
											WIN32PC_ONLY(&& !(control.GetFrontendSelect().IsDown() && control.GetFrontendRB().IsDown()) && !CLiveManager::IsSystemUiShowing()); // the global SCUI controller hotkey on PC is Back+RB

			if(isNextCameraModePressed)
			{
				cameraDebugf3("View-mode update: INPUT_NEXT_CAMERA is pressed");
				if(m_IsOverridingViewModeForSpectatorModeThisUpdate)
				{
					if(desiredViewMode == camControlHelperMetadataViewMode::CINEMATIC)
					{
						desiredViewMode = camControlHelperMetadataViewMode::THIRD_PERSON_MEDIUM; 
					}
					else
					{
						desiredViewMode = camControlHelperMetadataViewMode::CINEMATIC; 
					}
				}
				else
				{
					bool bSelectNextViewMode = true;
				#if FPS_MODE_SUPPORTED
					if(m_Metadata.m_ViewModes.m_ShouldToggleViewModeBetweenThirdAndFirstPerson && camInterface::GetGameplayDirector().IsFirstPersonModeEnabled())
					{
						desiredViewMode = (desiredViewMode == camControlHelperMetadataViewMode::FIRST_PERSON) ? camControlHelperMetadataViewMode::THIRD_PERSON_MEDIUM : camControlHelperMetadataViewMode::FIRST_PERSON;
						bSelectNextViewMode = false;
					}
				#endif // FPS_MODE_SUPPORTED

					if (bSelectNextViewMode)
					{
						desiredViewMode = (desiredViewMode + 1) % camControlHelperMetadataViewMode::eViewMode_NUM_ENUMS;
					}

				#if FPS_MODE_SUPPORTED
					TUNE_GROUP_BOOL(FIRST_PERSON_TUNE, ENABLE_PREVENT_NARRATIVE_SWITCH, true);
					if (ENABLE_PREVENT_NARRATIVE_SWITCH)
					{
						TUNE_GROUP_BOOL(FIRST_PERSON_TUNE, FORCE_DISABLE_SWITCHING_NARRATIVE_CAMERA_MODE, false);
						if (FORCE_DISABLE_SWITCHING_NARRATIVE_CAMERA_MODE || (pLocalPlayerPed && pLocalPlayerPed->GetPlayerResetFlag(CPlayerResetFlags::PRF_DISABLE_CAMERA_VIEW_MODE_CYCLE)))
						{
							// Keep current view mode
							desiredViewMode = initialViewMode;
							isNextCameraModePressed = false;
						}
					}
				#endif // FPS_MODE_SUPPORTED
				}
			}

			//NOTE: We must validate the view mode, even in the absence of control input, as we might have picked up an incompatible mode externally.
			ValidateViewMode(desiredViewMode);

#if FPS_MODE_SUPPORTED
			if (isNextCameraModePressed)
			{
				bool b;
				if(desiredViewMode == camControlHelperMetadataViewMode::FIRST_PERSON && pLocalPlayerPed && (pLocalPlayerPed->m_nFlags.bIsOnFire || !camInterface::GetGameplayDirector().ComputeIsFirstPersonModeAllowed(pLocalPlayerPed, false, b)))
				{
					// If changing modes while player is on fire, skip first person.
					desiredViewMode = (desiredViewMode + 1) % camControlHelperMetadataViewMode::eViewMode_NUM_ENUMS;
					ValidateViewMode(desiredViewMode);
				}

				// B*2092381: Don't set the "PRF_CAMERA_VIEW_MODE_SWITCHED_TO_OR_FROM_FIRST_PERSON" flag if we're toggling view modes whilst looking behind as we skip these transitions.
				bool bSetSwitchedToOrFromFPSFlag = true;

				if(IsLookingBehind() )
				{
					if(desiredViewMode == camControlHelperMetadataViewMode::FIRST_PERSON)
					{
						//skip first person.
						desiredViewMode = (desiredViewMode + 1) % camControlHelperMetadataViewMode::eViewMode_NUM_ENUMS;
						bSetSwitchedToOrFromFPSFlag = false;
					}
					else if(initialViewMode == camControlHelperMetadataViewMode::FIRST_PERSON)
					{
						desiredViewMode = initialViewMode;
						isNextCameraModePressed = false;
						bSetSwitchedToOrFromFPSFlag = false;
					}
				}

				if ((desiredViewMode == camControlHelperMetadataViewMode::FIRST_PERSON || initialViewMode == camControlHelperMetadataViewMode::FIRST_PERSON) && bSetSwitchedToOrFromFPSFlag)
				{
					if(pLocalPlayerPed->GetPlayerInfo() && CPlayerInfo::IsFirstPersonModeSupportedForPed(*pLocalPlayerPed))
					{
						pLocalPlayerPed->GetPlayerInfo()->SetSwitchedToOrFromFirstPerson(desiredViewMode == camControlHelperMetadataViewMode::FIRST_PERSON);
					}
				}

			}

			// When changing view modes, treat as if we are overriding view mode so we know that a context is being overridden.
			if(ms_MaintainCurrentViewMode)
			{
				if( isNextCameraModePressed || 
					(ms_ContextToForceFirstPersonCamera == -1 && ms_ContextToForceThirdPersonCamera == -1) )
				{
					if(desiredViewMode == camControlHelperMetadataViewMode::FIRST_PERSON)
					{
						ms_ContextToForceFirstPersonCamera = m_ViewModeContext;
						ms_ContextToForceThirdPersonCamera = -1;
						ms_FirstPersonCancelledForCinematicCamera = false;
					}
					else if(desiredViewMode != camControlHelperMetadataViewMode::CINEMATIC)
					{
						ms_ContextToForceFirstPersonCamera = -1;   
						ms_ContextToForceThirdPersonCamera = m_ViewModeContext;
						ms_ThirdPersonViewModeToForce = desiredViewMode;
						ms_FirstPersonCancelledForCinematicCamera = false;
					}
				}
			}
#endif // FPS_MODE_SUPPORTED

			m_ViewMode = ms_LastSetViewMode = desiredViewMode;
		#if FPS_MODE_SUPPORTED
			// If we are overriding view mode, do not overwrite the context value.
			if( isNextCameraModePressed || 
				(ms_ContextToForceFirstPersonCamera != m_ViewModeContext && ms_ContextToForceThirdPersonCamera != m_ViewModeContext) )
		#endif
			{
				*viewModeForContext = desiredViewMode;
			}
		}
	}

	//Update view mode blend levels.

	if(m_IsLookingBehind != m_WasLookingBehind)
	{
		m_ShouldSkipViewModeBlendThisUpdate = true;
	}

	const bool hasChangedViewMode = (m_ViewMode != initialViewMode);

	float viewModeDestinationBlendLevels[camControlHelperMetadataViewMode::eViewMode_NUM_ENUMS];
	sysMemSet(viewModeDestinationBlendLevels, 0, camControlHelperMetadataViewMode::eViewMode_NUM_ENUMS * sizeof(float));
	viewModeDestinationBlendLevels[m_ViewMode] = 1.0f;

	if(m_ViewModeBlendEnvelope)
	{
		if(m_ShouldSkipViewModeBlendThisUpdate)
		{
			sysMemCpy(m_ViewModeBlendLevels, viewModeDestinationBlendLevels, camControlHelperMetadataViewMode::eViewMode_NUM_ENUMS * sizeof(float));

			m_ViewModeBlendEnvelope->Stop();
		}
		else 
		{
			if(hasChangedViewMode)
			{
				sysMemCpy(m_ViewModeSourceBlendLevels, m_ViewModeBlendLevels, camControlHelperMetadataViewMode::eViewMode_NUM_ENUMS * sizeof(float));

				m_ViewModeBlendEnvelope->Start();
			}

			if(m_ViewModeBlendEnvelope->IsActive())
			{
				//NOTE: We maintain full blend after the envelope has released.
				const float envelopeLevel	= m_ViewModeBlendEnvelope->Update();
				const bool isEnvelopeActive	= m_ViewModeBlendEnvelope->IsActive();
				const float blendLevel		= isEnvelopeActive ? envelopeLevel : 1.0f;

				for(int viewModeIndex=0; viewModeIndex<camControlHelperMetadataViewMode::eViewMode_NUM_ENUMS; viewModeIndex++)
				{
					m_ViewModeBlendLevels[viewModeIndex] = Lerp(blendLevel, m_ViewModeSourceBlendLevels[viewModeIndex], viewModeDestinationBlendLevels[viewModeIndex]);
				}
			}
		}
	}
	else if(hasChangedViewMode)
	{
		sysMemCpy(m_ViewModeBlendLevels, viewModeDestinationBlendLevels, camControlHelperMetadataViewMode::eViewMode_NUM_ENUMS * sizeof(float));
	}

	m_ShouldUpdateViewModeThisUpdate		= false;
	m_ShouldIgnoreViewModeInputThisUpdate	= false;
	m_ShouldSkipViewModeBlendThisUpdate		= false;
	m_IsOverridingViewModeForSpectatorModeThisUpdate = false; 
}

bool camControlHelper::ComputeShouldUpdateViewMode() const
{
	if(!m_Metadata.m_ViewModes.m_ShouldUseViewModeInput || !m_ShouldUpdateViewModeThisUpdate)
	{
		if(!m_Metadata.m_ViewModes.m_ShouldUseViewModeInput)
		{
			cameraDebugf3("View-mode update: bypassing (!m_ShouldUseViewModeInput)");
		}
		else
		{
			cameraDebugf3("View-mode update: bypassing (!m_ShouldUpdateViewModeThisUpdate)");
		}

		return false;
	}

	if((m_ViewModeContext == camControlHelperMetadataViewMode::IN_VEHICLE) ||
		(m_ViewModeContext == camControlHelperMetadataViewMode::ON_BIKE) ||
		(m_ViewModeContext == camControlHelperMetadataViewMode::IN_BOAT) ||
		(m_ViewModeContext == camControlHelperMetadataViewMode::IN_AIRCRAFT) ||
		(m_ViewModeContext == camControlHelperMetadataViewMode::IN_SUBMARINE) ||
		(m_ViewModeContext == camControlHelperMetadataViewMode::IN_HELI) ||
		(m_ViewModeContext == camControlHelperMetadataViewMode::IN_TURRET))
	{
		//NOTE: We only update the in-vehicle view modes when the player is fully inside the vehicle.
		camGameplayDirector& gameplayDirector	= camInterface::GetGameplayDirector();
		const CVehicle* followVehicle			= gameplayDirector.GetFollowVehicle();
		const s32 vehicleEntryExitState			= gameplayDirector.GetVehicleEntryExitState();
		if(!followVehicle || (vehicleEntryExitState != camGameplayDirector::INSIDE_VEHICLE))
		{
			cameraDebugf3("View-mode update: bypassing (in-vehicle context, but not in a vehicle)");
			return false;
		}
	}

	//NOTE: We bypass the view mode update if the cinematic director is rendering irrespective of the current view mode, to avoid confusing
	//the user.
	const camCinematicDirector& cinematicDirector	= camInterface::GetCinematicDirector();
	const bool isCinematicDirectorRendering			= cinematicDirector.IsRendering();
	if(isCinematicDirectorRendering)
	{
		const camBaseCinematicContext* activeCinematicContext = cinematicDirector.GetCurrentContext();
		if(activeCinematicContext && activeCinematicContext->IsValid(true, false))
		{
			cameraDebugf3("View-mode update: bypassing (cinematic director is rendering)");
			return false;
		}
	}

	return true;
}

void camControlHelper::ValidateViewMode(s32& viewMode) const
{
	for(s32 i=0; i<camControlHelperMetadataViewMode::eViewMode_NUM_ENUMS; i++) 
	{
		const bool isModeValid = ComputeIsViewModeValid(viewMode);
		if(isModeValid)
		{
			return;
		}

		viewMode = (viewMode + 1) % camControlHelperMetadataViewMode::eViewMode_NUM_ENUMS;
	}

	//There are no valid view modes, so fall-back to the default mode.
	viewMode = g_DefaultViewMode;
}

bool camControlHelper::ComputeIsViewModeValid(s32 viewMode) const
{
	//Ensure that the on-foot view mode context always supports cinematic view mode in spectator mode. The follow parachute control helper was not
	//flagged as supporting the cinematic view mode and it is not possible to modify the camera metadata for the Online title update.
	if(NetworkInterface::IsInSpectatorMode() && (m_ViewModeContext == camControlHelperMetadataViewMode::ON_FOOT) &&
		(viewMode == camControlHelperMetadataViewMode::CINEMATIC))
	{
		return true;
	}

	bool isValid = m_Metadata.m_ViewModes.m_Flags.IsSet(viewMode);
#if FPS_MODE_SUPPORTED
	if( camInterface::GetGameplayDirector().IsFirstPersonModeEnabled() && viewMode == camControlHelperMetadataViewMode::FIRST_PERSON )
	{
		// For now, force first person mode as valid for all control helpers, if enabled.
		isValid = true;
	}
#endif

	if(isValid)
	{
		switch(m_ViewModeContext)
		{
			case camControlHelperMetadataViewMode::IN_VEHICLE:
			case camControlHelperMetadataViewMode::ON_BIKE:
			case camControlHelperMetadataViewMode::IN_BOAT:
			case camControlHelperMetadataViewMode::IN_AIRCRAFT:
			case camControlHelperMetadataViewMode::IN_SUBMARINE:
			case camControlHelperMetadataViewMode::IN_HELI:
			case camControlHelperMetadataViewMode::IN_TURRET:
				{
					//If we have a follow vehicle, check that it supports this view mode.
					const CVehicle* followVehicle					= camInterface::GetGameplayDirector().GetFollowVehicle();
					const CVehicleModelInfo* followVehicleModelInfo = followVehicle ? followVehicle->GetVehicleModelInfo() : NULL;

					switch(viewMode)
					{
					case camControlHelperMetadataViewMode::FIRST_PERSON:
						{
							const u32 cameraHash								= followVehicleModelInfo ? followVehicleModelInfo->GetBonnetCameraNameHash() : 0;
							const camCinematicMountedCameraMetadata* metadata	= camFactory::FindObjectMetadata<camCinematicMountedCameraMetadata>(cameraHash);

							isValid = (metadata != NULL);
							//NOTE: We no longer check for the bonnet mod, as the cinematic mounted camera is a POV from within the vehicle
						}
						break;

					case camControlHelperMetadataViewMode::CINEMATIC:
							isValid = followVehicleModelInfo && followVehicleModelInfo->ShouldUseCinematicViewMode() && NetworkInterface::IsInSpectatorMode();
						break;

					default:
						break;
					}
				}
				break;

			case camControlHelperMetadataViewMode::ON_FOOT:
				{
					switch(viewMode)
					{
					case camControlHelperMetadataViewMode::CINEMATIC:
						{
							isValid = NetworkInterface::IsInSpectatorMode();
						}
						break;

				#if FPS_MODE_SUPPORTED
					case camControlHelperMetadataViewMode::FIRST_PERSON:
						{
							isValid = camInterface::GetGameplayDirector().IsFirstPersonModeEnabled();
						}
						break;
				#endif

					default:
						break; 
					}

				}
				break;

			default:
				break;
		}
	}

	return isValid;
}

const camControlHelperMetadataLookAround& camControlHelper::GetLookAroundMetadata() const
{
#if __BANK
	if(ms_UseOverriddenLookAroundHelper)
	{
		return ms_OverriddenLookAroundHelper; 
	}
	else
#endif
	{
		return m_Metadata.m_LookAround; 
	}
}

void camControlHelper::UpdateLookAroundControl(const CControl& control)
{
	float lookAroundInputMag;
	float lookAroundHeadingInput;
	float lookAroundPitchInput;
	bool  usingAccelerometers = false;

#if RSG_ORBIS
	m_WasPlayerAiming = m_WasPlayerAiming && !m_ShouldIgnoreLookAroundInputThisUpdate;
#endif // RSG_ORBIS

	m_IsLookAroundInputPresent = ComputeLookAroundInput(control, lookAroundInputMag, lookAroundHeadingInput, lookAroundPitchInput, usingAccelerometers);

	CPed* pPlayer = CGameWorld::FindLocalPlayer();
#if RSG_ORBIS
	////const bool isPlayerAiming		= control.GetPedTargetIsDown();
	bool isPlayerAiming = false;
	if(pPlayer && pPlayer->GetPlayerInfo())
	{
		isPlayerAiming = pPlayer->GetPlayerInfo()->IsAiming();
	}
	m_WasPlayerAiming = isPlayerAiming;
#endif // RSG_ORBIS

	const float timeStep = GetLookAroundMetadata().m_ShouldUseGameTime ? fwTimer::GetTimeStep() : fwTimer::GetCamTimeStep();

#if RSG_PC
	// We only consider the last known source as keyboard/mouse when ped look LR is enabled. The reason for this is that when
	// it is not, we want the camera to follow the vehicle. We are not too worried about UD.
	// NOTE: treat steam controller as mouse.
	m_WasUsingKeyboardMouseLastInput = control.UseRelativeLook() && control.GetPedLookLeftRight().IsEnabled();

	if ( control.IsRelativeLookSource(control.GetPedLookLeftRight().GetSource()) ||
		 control.IsRelativeLookSource(control.GetPedLookUpDown().GetSource()) )
	{
		// PC only: No acceleration or aiming assistance when using the mouse.
		UpdateLookAroundInputEnvelope(m_IsLookAroundInputPresent);

		const bool bAllowMouseAcceleration = (CPauseMenu::GetMenuPreference( PREF_MOUSE_ACCELERATION ) == TRUE);		// aka Fine Aiming Control
		if(bAllowMouseAcceleration && lookAroundInputMag >= SMALL_FLOAT && lookAroundInputMag < 1.0f)
		{
			float scaledLookAroundInputMag = rage::Powf(lookAroundInputMag, MOUSE_POWER_EXPONENT);
			lookAroundHeadingInput *= scaledLookAroundInputMag/lookAroundInputMag;
			lookAroundPitchInput   *= scaledLookAroundInputMag/lookAroundInputMag;
		}

		m_NormalisedLookAroundHeadingInput	= lookAroundHeadingInput;
		m_NormalisedLookAroundPitchInput	= lookAroundPitchInput;

		if (pPlayer && pPlayer->GetMotionData()->GetCurrentMoveBlendRatio().Mag() > MOVEBLENDRATIO_STILL)
		{
			m_NormalisedLookAroundHeadingInput += (pPlayer->GetMotionData()->GetSteerBias() * timeStep);
		}

		// Treat mouse input as angle/frame.
		// We then scale by radians/second and then by 1/60 seconds/frame to give us the target radians/frame with a mouse input of 1.0
		const float c_fInputScale = (1.0f/60.0f);

		float fMouseSensitivityOverride = 0.0f;
		PARAM_mouse_sensitivity_override.Get(fMouseSensitivityOverride);

	#if __BANK
		TUNE_GROUP_FLOAT(MOUSE_TUNE, MOUSE_SENSITIVITY_OVERRIDE, -1.0f, -1.0f, 5000.0f, 1.0f);
		if(MOUSE_SENSITIVITY_OVERRIDE > 0.0f)
		{
			fMouseSensitivityOverride = MOUSE_SENSITIVITY_OVERRIDE;
		}
	#endif

		TUNE_GROUP_FLOAT(MOUSE_TUNE, MAX_MOUSE_SENSITIVITY_OVERRIDE, 2500.0f, 0.0f, 5000.0f, 1.0f);
		fMouseSensitivityOverride = Min(fMouseSensitivityOverride, MAX_MOUSE_SENSITIVITY_OVERRIDE);

		const float fMouseSensitivity = ( (fMouseSensitivityOverride > 0.0f) ? fMouseSensitivityOverride : (float)CPauseMenu::GetMenuPreference(PREF_MOUSE_ON_FOOT_SCALE)) / (float)MAX_MOUSE_SCALE;
		const float fLookSensitivity  = control.WasSteamControllerLastKnownSource() ? 1.0f : fMouseSensitivity;
		const float fMaxHeadingSpeedPerSecond = Lerp(fLookSensitivity, m_Metadata.m_LookAround.m_MouseMaxHeadingSpeedMin, m_Metadata.m_LookAround.m_MouseMaxHeadingSpeedMax) * DtoR * c_fInputScale;
		const float fMaxPitchSpeedPerSecond   = Lerp(fLookSensitivity, m_Metadata.m_LookAround.m_MouseMaxPitchSpeedMin,   m_Metadata.m_LookAround.m_MouseMaxPitchSpeedMax)   * DtoR * c_fInputScale;

		m_LookAroundHeadingOffset = m_NormalisedLookAroundHeadingInput * fMaxHeadingSpeedPerSecond;
		m_LookAroundPitchOffset   = m_NormalisedLookAroundPitchInput   * fMaxPitchSpeedPerSecond;

		TUNE_GROUP_FLOAT(MOUSE_TUNE, MAX_HEADING_TURN_RATE_PER_UPDATE, 180.0f, 45.0f, 360.0f, 1.0f);
		TUNE_GROUP_FLOAT(MOUSE_TUNE, MAX_PITCH_TURN_RATE_PER_UPDATE, 90.0f, 20.0f, 180.0f, 1.0f);

		m_LookAroundHeadingOffset = Clamp(m_LookAroundHeadingOffset, -MAX_HEADING_TURN_RATE_PER_UPDATE*DtoR, MAX_HEADING_TURN_RATE_PER_UPDATE*DtoR);
		m_LookAroundPitchOffset   = Clamp(m_LookAroundPitchOffset,   -MAX_PITCH_TURN_RATE_PER_UPDATE*DtoR,   MAX_PITCH_TURN_RATE_PER_UPDATE*DtoR);
	}
	else
#endif // RSG_PC
	{
		UpdateLookAroundInputEnvelope(m_IsLookAroundInputPresent);

		//Apply the custom power factor scaling specified in metadata to change the control response across the range of stick input.
		float scaledLookAroundInputMag = rage::Powf(lookAroundInputMag, GetLookAroundMetadata().m_InputMagPowerFactor);

		// CS: Haxx code - sorry
		if(pPlayer)
		{
			pPlayer->GetPlayerInfo()->GetTargeting().GetScaledLookAroundInputMagForCamera(lookAroundInputMag, scaledLookAroundInputMag);
		}

		float c_Deceleration = (!m_UseLeftStickForLookInputThisUpdate || GetLookAroundMetadata().m_LSDeceleration == 0.0f) ?
											GetLookAroundMetadata().m_Deceleration : GetLookAroundMetadata().m_LSDeceleration;
		float c_Acceleration = (!m_UseLeftStickForLookInputThisUpdate || GetLookAroundMetadata().m_LSAcceleration == 0.0f) ?
											GetLookAroundMetadata().m_Acceleration : GetLookAroundMetadata().m_LSAcceleration;

		float fMaxSpeed = 1.0f;
#if FPS_MODE_SUPPORTED
		// Use slower accel/max speed values for FPS underwater swim-sprinting
		if (pPlayer && pPlayer->GetIsFPSSwimming() && m_UseLeftStickForLookInputThisUpdate)
		{
			static dev_float fAccel = 1.0f;
			c_Acceleration = fAccel;
			static dev_float fMaxSpeedTune = 0.33f;
			fMaxSpeed = fMaxSpeedTune;
		}

		if( m_UseLeftStickForLookInputThisUpdate && !m_UseBothSticksForLookInputThisUpdate &&
			(lookAroundHeadingInput == 0.0f || m_LookAroundHeadingInputPreviousUpdate*lookAroundHeadingInput < 0.0f) )
		{
			if ((pPlayer && !pPlayer->GetIsFPSSwimmingUnderwater()) || (pPlayer && pPlayer->GetIsFPSSwimmingUnderwater() && lookAroundPitchInput == 0.0f))
			{
				// Reset speed if input is within the deadzone or if direction changed in one update.
				m_LookAroundSpeed = 0.0f;
			}
		}
		m_LookAroundHeadingInputPreviousUpdate = lookAroundHeadingInput;
#endif

		s32 accelProfileSetting = (CProfileSettings::GetInstance().Exists(CProfileSettings::AIM_ACCELERATION) ?
			CProfileSettings::GetInstance().GetInt(CProfileSettings::AIM_ACCELERATION) : g_DefaultControllerAcceleration);
#if FPS_MODE_SUPPORTED

		//TODO: Determine if using a first person control helper via flag instead of hardcoding by metadata name?
		bool bUseFpsSettings = (
			m_Metadata.m_Name == ATSTRINGHASH("FIRST_PERSON_SHOOTER_NO_AIM_CONTROL_HELPER", 0x488A30D2) ||
			m_Metadata.m_Name == ATSTRINGHASH("DEFAULT_FIRST_PERSON_AIM_CONTROL_HELPER", 0x8BB92CE8)	||
			m_Metadata.m_Name == ATSTRINGHASH("DEFAULT_VEHICLE_POV_CONTROL_HELPER", 0x49f5edae)			||
			m_Metadata.m_Name == ATSTRINGHASH("POV_TURRET_CONTROL_HELPER", 0x3cc86ec0)
		);

		if(bUseFpsSettings)
		{
			accelProfileSetting = (CProfileSettings::GetInstance().Exists(CProfileSettings::FPS_AIM_ACCELERATION) ?
				CProfileSettings::GetInstance().GetInt(CProfileSettings::FPS_AIM_ACCELERATION) : g_DefaultControllerAcceleration);
		}
#endif
		{
			const camControlHelperMetaDataPrecisionAimSettings &precisionAim = GetLookAroundMetadata().m_PrecisionAimSettings;

			float fProfileScale = Clamp((float)accelProfileSetting / (float)g_OriginalMaxControllerAcceleration, 0.0f, 1.0f);
		
			float fAccelScale = rage::Lerp(fProfileScale, precisionAim.m_MinAccelModifier, precisionAim.m_MaxAccelModifier);
			c_Acceleration *= fAccelScale;
			float fDeccelScale = rage::Lerp(fProfileScale, precisionAim.m_MinDeccelModifier, precisionAim.m_MaxDeccelModifier );
			
			//! Note: We never allow this to drop below 1.0f. min value allows us to keep the mid range profile value the same as when we shipped.
			//! e.g. g_DefaultControllerAcceleration / g_OriginalMaxControllerAcceleration == 1.0f.
			fDeccelScale = Max(fDeccelScale, 1.0f);
			c_Deceleration *= fDeccelScale;
		}

		//Apply acceleration and deceleration to stick input.
		const float decelerationToApply = m_LookAroundDecelerationScaling * c_Deceleration;
		float baseLookAroundOffset		= ComputeOffsetBasedOnAcceleratedInput(scaledLookAroundInputMag, c_Acceleration,
			decelerationToApply, fMaxSpeed, m_LookAroundSpeed, timeStep);

		//Scale the base offset in accordance with the aim sensitivity, thereby affecting acceleration/deceleration and maximum speed.
		float aimSensitivityScaling	= GetAimSensitivityScaling();
		baseLookAroundOffset		*= aimSensitivityScaling;

#if __BANK
		baseLookAroundOffset *= ms_LookAroundSpeedScalar; 
#endif
		static const atHashString nightClubControlHelperName(ATSTRINGHASH("NIGHTCLUB_FOLLOW_PED_CONTROL_HELPER", 0xE69EFD48));
		const bool isRunningTheNightClubDanceCamera = m_Metadata.m_Name == nightClubControlHelperName;
		if(lookAroundInputMag > 0.0f)
		{
			//Normalise the look-around heading and pitch inputs, or use the last valid normalised values. This allows us to apply an appropriate deceleration
			//across the two axes.
			m_NormalisedLookAroundHeadingInput	= lookAroundHeadingInput / lookAroundInputMag;
			m_NormalisedLookAroundPitchInput	= lookAroundPitchInput / lookAroundInputMag;
		}
		else if(isRunningTheNightClubDanceCamera)
		{
			//special case for the nightclub dancing camera, we do this to make sure we apply deceleration.
			if(Abs(baseLookAroundOffset) < SMALL_FLOAT)
			{
				m_NormalisedLookAroundHeadingInput	= 0.0f;
				m_NormalisedLookAroundPitchInput	= 0.0f;
			}
		}
		else
		{
			m_NormalisedLookAroundHeadingInput	= 0.0f;
			m_NormalisedLookAroundPitchInput	= 0.0f;
		}

		if (pPlayer && pPlayer->GetMotionData()->GetCurrentMoveBlendRatio().Mag() > MOVEBLENDRATIO_STILL)
		{
			m_NormalisedLookAroundHeadingInput = Clamp(m_NormalisedLookAroundHeadingInput + pPlayer->GetMotionData()->GetSteerBias(), -1.f, 1.f);
		}

		float fMaxHeadingSpeed = GetLookAroundMetadata().m_MaxHeadingSpeed;
		float fMaxPitchSpeed = GetLookAroundMetadata().m_MaxPitchSpeed;

#if FPS_MODE_SUPPORTED
		// If player is FPS swimming, scale the heading/pitch offset down by half (and clamp).
		if (pPlayer && pPlayer->GetIsFPSSwimming())
		{
			static dev_float fMaxHeadingSpeedSwimTune = 75.0f;
			static dev_float fMaxPitchSpeedSwimTune = 65.0f;
			static dev_float fSwimScaler = 0.75f;
			fMaxHeadingSpeed = Clamp(fMaxHeadingSpeed * fSwimScaler, 25.0f, fMaxHeadingSpeedSwimTune);
			fMaxPitchSpeed = Clamp(fMaxPitchSpeed * fSwimScaler, 25.0f, fMaxPitchSpeedSwimTune);
		}
#endif	//FPS_MODE_SUPPORTED

		m_LookAroundHeadingOffset	= m_NormalisedLookAroundHeadingInput * baseLookAroundOffset * fMaxHeadingSpeed * DtoR;
		m_LookAroundPitchOffset		= m_NormalisedLookAroundPitchInput * baseLookAroundOffset * fMaxPitchSpeed * DtoR;
	}

	m_ShouldIgnoreLookAroundInputThisUpdate	= false;
}

//Check if we have look-around input, using a dead-zone to prevent false hits.
bool camControlHelper::ComputeLookAroundInput(const CControl& control, float& inputMag, float& headingInput, float& pitchInput, bool& ORBIS_ONLY(usingAccelerometers)) const
{
	if(m_ShouldIgnoreLookAroundInputThisUpdate)
	{
		headingInput	= 0.0f;
		pitchInput		= 0.0f;
		inputMag		= 0.0f;
	}
	else
	{
		const ioValue& headingControl	= (!m_UseLeftStickForLookInputThisUpdate) ? control.GetPedAimWeaponLeftRight() : control.GetPedWalkLeftRight();
		const ioValue& pitchControl		= (!m_UseLeftStickForLookInputThisUpdate) ? control.GetPedAimWeaponUpDown()    : control.GetPedWalkUpDown();

		CPed* pPlayer = CGameWorld::FindLocalPlayer();
	#if RSG_ORBIS
		////const bool isPlayerAiming		= control.GetPedTargetIsDown();
		bool isPlayerAiming = false;
		if(pPlayer && pPlayer->GetPlayerInfo())
		{
			isPlayerAiming = pPlayer->GetPlayerInfo()->IsAiming();
		}
	#endif // RSG_ORBIS

		//NOTE: Each input is zeroed if the corresponding speed limit is zero and hence the input should be ignored.
		//NOTE: We apply no dead-zoning to the individual heading and pitch axes, but instead apply an optional round dead-zone below.
		ioValue::ReadOptions options	= ioValue::NO_DEAD_ZONE;
		headingInput					= IsNearZero(GetLookAroundMetadata().m_MaxHeadingSpeed) ? 0.0f : -headingControl.GetUnboundNorm(options);
		pitchInput						= IsNearZero(GetLookAroundMetadata().m_MaxPitchSpeed)   ? 0.0f : -pitchControl.GetUnboundNorm(options);

		// Script-requested ability to invert look controls as a MP pickup power
		if (pPlayer && pPlayer->GetPedResetFlag(CPED_RESET_FLAG_InvertLookAroundControls) && !m_UseLeftStickForLookInputThisUpdate)
		{
			headingInput = -headingInput;
			pitchInput = -pitchInput;
		}

		inputMag = rage::Sqrtf((headingInput * headingInput) + (pitchInput * pitchInput));
		float fDeadZone = GetDeadZoneScaling(inputMag);

		ioValue::ReadOptions deadZoneOptions	= ioValue::DEFAULT_OPTIONS;
		deadZoneOptions.m_DeadZone = fDeadZone;

	#if RSG_PC
		TUNE_GROUP_FLOAT(CAM_FPS, c_fLeftStickMouseInputMax, 0.50f, 0.0f, 1.0f, 0.001f);
	#endif
		if( ioValue::RequiresDeadZone(headingControl.GetSource()) )
		{
			// Re-read input with deadzone and clear un-deadzoned input if deadzoned input is zero.
			float deadzonedInput = -headingControl.GetUnboundNorm(deadZoneOptions);
			if(deadzonedInput == 0.0f)
				headingInput = 0.0f;
		}
	#if RSG_PC
		else if(m_UseLeftStickForLookInputThisUpdate)
		{
			headingInput = Clamp(headingInput, -c_fLeftStickMouseInputMax, c_fLeftStickMouseInputMax);
		}
	#endif

		if( ioValue::RequiresDeadZone(pitchControl.GetSource()) )
		{
			// Re-read input with deadzone and clear un-deadzoned input if deadzoned input is zero.
			float deadzonedInput = -pitchControl.GetUnboundNorm(deadZoneOptions);
			if(deadzonedInput == 0.0f)
				pitchInput = 0.0f;
		}
	#if RSG_PC
		else if(m_UseLeftStickForLookInputThisUpdate)
		{
			pitchInput = Clamp(pitchInput, -c_fLeftStickMouseInputMax, c_fLeftStickMouseInputMax);
		}
	#endif

	#if RSG_PC
		// If either axis is using the mouse, disable the dead-zone.
		if ( !ioValue::RequiresDeadZone(headingControl.GetSource()) ||
			 !ioValue::RequiresDeadZone(pitchControl.GetSource()) )
		{
			inputMag = rage::Sqrtf((headingInput * headingInput) + (pitchInput * pitchInput));
		}
		else
	#endif // RSG_PC
		{
		#if RSG_ORBIS
			float motionPitch = 0.0f;
			float motionYaw   = 0.0f;
			if (isPlayerAiming)
			{
				Quaternion controllerOrientation;
				if (const_cast<CControl&>(control).GetOrientation(controllerOrientation))
				{
					// Convert quaternion to euler angles to get pitch and yaw relative to centered orientation.
					Vector3 vecEuler;
					controllerOrientation.ToEulers(vecEuler, eEulerOrderXYZ);

					motionPitch = vecEuler.x;
					motionYaw   = vecEuler.y;
				}
			}
		#endif // RSG_ORBIS

			inputMag = rage::ioAddRoundDeadZone(headingInput, pitchInput, fDeadZone);

		#if FPS_MODE_SUPPORTED
			if(m_UseBothSticksForLookInputThisUpdate && !m_UseLeftStickForLookInputThisUpdate && inputMag == 0.0f)
			{
				const ioValue& headingLeftStick	= control.GetPedWalkLeftRight();
				const ioValue& pitchLeftStick	= control.GetPedWalkUpDown();
				headingInput					= IsNearZero(GetLookAroundMetadata().m_MaxHeadingSpeed) ? 0.0f : -headingLeftStick.GetUnboundNorm(options);
				pitchInput						= IsNearZero(GetLookAroundMetadata().m_MaxPitchSpeed)   ? 0.0f : -pitchLeftStick.GetUnboundNorm(options);

				if( ioValue::RequiresDeadZone(headingLeftStick.GetSource()) )
				{
					// Re-read input with deadzone and clear un-deadzoned input if deadzoned input is zero.
					float deadzonedInput = -headingLeftStick.GetUnboundNorm();
					if(deadzonedInput == 0.0f)
						headingInput = 0.0f;
				}

				if( ioValue::RequiresDeadZone(pitchLeftStick.GetSource()) )
				{
					// Re-read input with deadzone and clear un-deadzoned input if deadzoned input is zero.
					float deadzonedInput = -pitchLeftStick.GetUnboundNorm();
					if(deadzonedInput == 0.0f)
						pitchInput = 0.0f;
				}

				inputMag = rage::ioAddRoundDeadZone(headingInput, pitchInput, ioValue::DEFAULT_DEAD_ZONE_VALUE);
			}
		#endif

		#if FPS_MODE_SUPPORTED
			if(m_UseLeftStickForLookInputThisUpdate && !m_UseBothSticksForLookInputThisUpdate)
			{
				// Apply angular dead zone.
				const float c_fAngleDeadzoneRad = GetLookAroundMetadata().m_LSDeadZoneAngle * DtoR;
				float fStickDirection = Atan2f(headingInput, pitchInput);
				if( Abs(fStickDirection) < c_fAngleDeadzoneRad )
				{
					headingInput = 0.0f;
				}
				else
				{
					// Don't re-scale input for FPS swimming underwater (as we use both heading and pitch)
					if (pPlayer && !pPlayer->GetIsFPSSwimmingUnderwater())
					{
						// Re-scale input
						float boundary = (fStickDirection > 0.0f) ? -c_fAngleDeadzoneRad : c_fAngleDeadzoneRad;
						headingInput = (fStickDirection - boundary) / (PI*0.50f);
						headingInput = Clamp(headingInput, -1.0f, 1.0f);
					}
				}
			}
		#endif // FPS_MODE_SUPPORTED

	#if RSG_ORBIS 
		#if !CONTROLLER_ORIENTATION_ADD_TO_STICK_INPUT
			if (isPlayerAiming && inputMag == 0.0f)
		#else
			if (isPlayerAiming)
		#endif
			{
				{
					// Add deadzone
					////s32 viewMode = GetViewModeForContext(m_ViewModeContext);
					////static float c_fMotionDeadZone = ((viewMode != camControlHelperMetadataViewMode::FIRST_PERSON) ? 0.05f : 0.0f);
					rage::ioAddRoundDeadZone(motionYaw, motionPitch, c_fMotionDeadzone * DtoR);
				}

				if (motionYaw != 0.0f || motionPitch != 0.0f)
				{
				#if !CONTROLLER_ORIENTATION_ADD_TO_STICK_INPUT
					usingAccelerometers = true;

					pitchInput = motionPitch * c_fMotionVerticalScale;
					headingInput = motionYaw * c_fMotionHorizontalScale;
				#else
					// When adding to stick input, treat accelerometer input the same.
					pitchInput += motionPitch * c_fMotionVerticalScale;
					headingInput += motionYaw * c_fMotionHorizontalScale;
				#endif

					// Deadzone already applied for stick input and for controller orientation, so just re-calculate the magnitude.
					Vector3 vTemp(headingInput, pitchInput, 0.0f);
					inputMag = vTemp.Mag();
				}
			}
	#endif // RSG_ORBIS
		}
	}

	bool isInputPresent = (inputMag > 0.0f);

	return isInputPresent;
}

void camControlHelper::UpdateLookAroundInputEnvelope(bool isInputPresent)
{
	if(m_LookAroundInputEnvelope)
	{
		//Apply an envelope to the look-around input presence. This can be used to gradually blend out camera behaviors when look-around input is present.
#if RSG_PC
		if (m_WasUsingKeyboardMouseLastInput)
		{
			if (isInputPresent)
			{
				m_LookAroundInputEnvelope->OverrideAttackDuration(0);
				m_LookAroundInputEnvelope->AutoStart();
			}
		}
		else
#endif // RSG_PC
		{
			m_LookAroundInputEnvelope->AutoStartStop(isInputPresent);
		}
		m_LookAroundInputEnvelopeLevel	= m_LookAroundInputEnvelope->Update();
		//TODO: Push this into the envelope helper.
		m_LookAroundInputEnvelopeLevel	= SlowInOut(m_LookAroundInputEnvelopeLevel);
	}
	else
	{
		//We don't have an envelope, so just use the input state directly.
		m_LookAroundInputEnvelopeLevel	= isInputPresent ? 1.0f : 0.0f;
	}
}

float camControlHelper::GetDeadZoneScaling(float fInputMag) const
{
	float fMaxDeadZone = ioValue::DEFAULT_DEAD_ZONE_VALUE;

	float fDeadZone = fMaxDeadZone;

	s32 profileSetting = (CProfileSettings::GetInstance().Exists(CProfileSettings::AIM_DEADZONE) ?
		CProfileSettings::GetInstance().GetInt(CProfileSettings::AIM_DEADZONE) : g_DefaultControllerDeadZone);

#if FPS_MODE_SUPPORTED
	//TODO: Determine if using a first person control helper via flag instead of hardcoding by metadata name?
	bool bUseFpsSettings = (
		m_Metadata.m_Name == ATSTRINGHASH("FIRST_PERSON_SHOOTER_NO_AIM_CONTROL_HELPER", 0x488A30D2) ||
		m_Metadata.m_Name == ATSTRINGHASH("DEFAULT_FIRST_PERSON_AIM_CONTROL_HELPER", 0x8BB92CE8) ||
		m_Metadata.m_Name == ATSTRINGHASH("DEFAULT_VEHICLE_POV_CONTROL_HELPER", 0x49f5edae)
	);

	if(bUseFpsSettings)
	{
		profileSetting = (CProfileSettings::GetInstance().Exists(CProfileSettings::FPS_AIM_DEADZONE) ?
			CProfileSettings::GetInstance().GetInt(CProfileSettings::FPS_AIM_DEADZONE) : g_DefaultControllerDeadZone);
	}
#endif

	float fScale = Clamp((float)profileSetting / (float)g_OriginalMaxControllerDeadZone, 0.0f, 1.0f);

	float fMinDeadZone = ioValue::DEFAULT_MIN_DEAD_ZONE_VALUE;

	fDeadZone = fMinDeadZone + ((fMaxDeadZone - fMinDeadZone) * fScale);

	fInputMag = Max(fInputMag, m_LookAroundSpeed);

	//! If we aggressively move stick, start to increase deadzone, so that we don't wave about on 1 axis. E.g. moving in horizontal plane only could 
	//! introduce judder into vertical plane, so this avoids that.
	if(fInputMag > GetLookAroundMetadata().m_PrecisionAimSettings.m_InputMagToIncreaseDeadZoneMax)
	{
		fDeadZone = fMaxDeadZone;
	}
	else if(fInputMag > GetLookAroundMetadata().m_PrecisionAimSettings.m_InputMagToIncreaseDeadZoneMin)
	{
		float fScale = (fInputMag - GetLookAroundMetadata().m_PrecisionAimSettings.m_InputMagToIncreaseDeadZoneMin) / ((GetLookAroundMetadata().m_PrecisionAimSettings.m_InputMagToIncreaseDeadZoneMax-GetLookAroundMetadata().m_PrecisionAimSettings.m_InputMagToIncreaseDeadZoneMin));
		fDeadZone = rage::Lerp(fScale, fDeadZone, fMaxDeadZone);
	}

	return Clamp(fDeadZone, fMinDeadZone, fMaxDeadZone);
}

float camControlHelper::GetAimSensitivityScaling() const
{
	s32 aimSensitivity;

#if FPS_MODE_SUPPORTED
	const bool isUsingDefaultVehiclePovControlHelper = m_Metadata.m_Name == ATSTRINGHASH("DEFAULT_VEHICLE_POV_CONTROL_HELPER", 0x49f5edae);

	// TODO: Determine if using a first person control helper via flag instead of hardcoding by metadata name?
	bool bUseFpsSettings = (
		m_Metadata.m_Name == ATSTRINGHASH("FIRST_PERSON_SHOOTER_NO_AIM_CONTROL_HELPER", 0x488A30D2) ||
		m_Metadata.m_Name == ATSTRINGHASH("DEFAULT_FIRST_PERSON_AIM_CONTROL_HELPER", 0x8BB92CE8) 	||
		m_Metadata.m_Name == ATSTRINGHASH("POV_TURRET_CONTROL_HELPER", 0x3cc86ec0) 					||
		isUsingDefaultVehiclePovControlHelper
	);

	const CPed* pPlayer = CGameWorld::FindLocalPlayer();
	bool isPlayerAiming = false; // b*2152871 - we don't use a separate camera for in-vehicle POV aiming, so cannot branch on m_ShouldApplyAimSensitivityPref, instead we must branch on isPlayerAiming
	if(pPlayer && pPlayer->GetPlayerInfo())
	{
		isPlayerAiming = pPlayer->GetPlayerInfo()->IsAiming();
	}		
#endif // FPS_MODE_SUPPORTED

	if((m_Metadata.m_ShouldApplyAimSensitivityPref || m_ShouldForceAimSensitivityThisUpdate) FPS_MODE_SUPPORTED_ONLY( || (isUsingDefaultVehiclePovControlHelper && isPlayerAiming)))
	{
		s32 profileSetting = (CProfileSettings::GetInstance().Exists(CProfileSettings::CONTROLLER_AIM_SENSITIVITY) ?
							CProfileSettings::GetInstance().GetInt(CProfileSettings::CONTROLLER_AIM_SENSITIVITY) : g_DefaultControllerAimSensitivity);
	#if FPS_MODE_SUPPORTED
		if(bUseFpsSettings)
		{
			profileSetting = (CProfileSettings::GetInstance().Exists(CProfileSettings::FPS_AIM_SENSITIVITY) ?
							CProfileSettings::GetInstance().GetInt(CProfileSettings::FPS_AIM_SENSITIVITY) : g_DefaultControllerAimSensitivity);
			if(m_LimitFirstPersonAimSensitivityThisUpdate >= 0)
				profileSetting = Min(profileSetting, m_LimitFirstPersonAimSensitivityThisUpdate);
		}
	#endif
		aimSensitivity	= BANK_ONLY((ms_DebugAimSensitivitySetting >= 0) ? ms_DebugAimSensitivitySetting : ) profileSetting;
	}
	else
	{
		s32 profileSetting = (CProfileSettings::GetInstance().Exists(CProfileSettings::LOOK_AROUND_SENSITIVITY) ?
							CProfileSettings::GetInstance().GetInt(CProfileSettings::LOOK_AROUND_SENSITIVITY) : g_DefaultControllerAimSensitivity);
	#if FPS_MODE_SUPPORTED
		if(bUseFpsSettings)
		{
			profileSetting = (CProfileSettings::GetInstance().Exists(CProfileSettings::FPS_LOOK_SENSITIVITY) ?
							CProfileSettings::GetInstance().GetInt(CProfileSettings::FPS_LOOK_SENSITIVITY) : g_DefaultControllerAimSensitivity);
			if(m_LimitFirstPersonAimSensitivityThisUpdate >= 0)
				profileSetting = Min(profileSetting, m_LimitFirstPersonAimSensitivityThisUpdate);
		}
	#endif
		aimSensitivity	= BANK_ONLY((ms_DebugLookAroundSensitivitySetting >= 0) ? ms_DebugLookAroundSensitivitySetting : )
							m_ShouldForceDefaultLookAroundSensitivityThisUpdate ? g_DefaultControllerAimSensitivity : profileSetting;
	}

	// B*2248229: Clamp sensitivity for turrets so they don't turn really slow at lower sensitivities.
	if (camInterface::GetGameplayDirector().IsUsingVehicleTurret(false))
	{
		TUNE_GROUP_INT(TURRET_TUNE, iTurretMaxSensitivity, 8, 0, 14, 1);
		aimSensitivity = Min(iTurretMaxSensitivity, aimSensitivity);
	}

	//NOTE: We are extending the upper limit of aim sensitivity post-launch, so clamp against the revised limit by default.
	const s32 maxAimSensitivity			= m_ShouldUseOriginalMaxControllerAimSensitivity ? g_OriginalMaxControllerAimSensitivity : g_RevisedMaxControllerAimSensitivity;
	aimSensitivity						= Clamp(aimSensitivity, 0, maxAimSensitivity);
	const float aimSensitivityTValue	= static_cast<float>(aimSensitivity) / static_cast<float>(g_OriginalMaxControllerAimSensitivity);
	const float aimSensitivityScaling	= Lerp(aimSensitivityTValue, m_Metadata.m_AimSensitivityScalingLimits.x, m_Metadata.m_AimSensitivityScalingLimits.y);

	return aimSensitivityScaling;
}

void camControlHelper::UpdateZoomControl(CControl& control)
{
	if(!m_Metadata.m_Zoom.m_ShouldUseZoomInput)
	{
		return;
	}

	float zoomInput;
	ComputeZoomInput(control, zoomInput);

	const float timeStep		= m_Metadata.m_Zoom.m_ShouldUseGameTime ? fwTimer::GetTimeStep() : fwTimer::GetCamTimeStep();
	float zoomFactorOffset		= ComputeOffsetBasedOnAcceleratedInput(zoomInput, m_Metadata.m_Zoom.m_Acceleration, m_Metadata.m_Zoom.m_Deceleration,
									m_Metadata.m_Zoom.m_MaxSpeed, m_ZoomFactorSpeed, timeStep);

	UpdateZoomFactorLimits();

	if(m_Metadata.m_Zoom.m_ShouldUseDiscreteZoomControl)
	{
		//Implement discrete zoom, i.e. max zoom if the zoom-in control input is applied, otherwise min zoom.
		ms_ZoomFactor		= (zoomFactorOffset > 0.0f) ? m_ZoomFactorLimits.y : m_ZoomFactorLimits.x;
	}
	else
	{
		//Scale the offset based upon the current zoom factor. This prevents zoom speed seeming faster towards the low end of the zoom range.
		zoomFactorOffset	*= ms_ZoomFactor;
		ms_ZoomFactor		+= zoomFactorOffset;

		ms_ZoomFactor		= Clamp(ms_ZoomFactor, m_ZoomFactorLimits.x, m_ZoomFactorLimits.y);
	}
}

//Check if we have zoom input, using a dead-zone to prevent false hits.
bool camControlHelper::ComputeZoomInput(CControl& control, float& zoomInput) const
{
#if KEYBOARD_MOUSE_SUPPORT
	if(control.GetPedSniperZoom().GetSource().m_DeviceIndex == ioSource::IOMD_KEYBOARD_MOUSE)
	{
		control.SetInputExclusive(INPUT_SNIPER_ZOOM);
	}
#endif //  KEYBOARD_MOUSE_SUPPORT
	const ioValue& zoomControl	= control.GetPedSniperZoom();

	// Use D-pad to zoom except for cellphone camera.
	if( CPauseMenu::GetMenuPreference(PREF_SNIPER_ZOOM) && !CPhoneMgr::CamGetState()
		KEYBOARD_MOUSE_ONLY(&& control.GetPedSniperZoom().GetSource().m_DeviceIndex != ioSource::IOMD_KEYBOARD_MOUSE) )
	{
		zoomInput = 0.0f;
		if(control.GetPedSniperZoomInSecondary().IsDown())
		{
			zoomInput += 1.0f;
		}

		if(control.GetPedSniperZoomOutSecondary().IsDown())
		{
			zoomInput -= 1.0f;
		}
	}
	else
	{
		zoomInput					= -zoomControl.GetUnboundNorm();
	}

	float inputMag				= Abs(zoomInput);
	bool isInputPresent			= (inputMag > 0.0f);
	if(isInputPresent)
	{
		//Apply the custom power factor scaling specified in metadata to change the control response across the range of stick input.
		inputMag				= rage::Powf(inputMag, m_Metadata.m_Zoom.m_InputMagPowerFactor);
		zoomInput				= Sign(zoomInput) * inputMag;
	}

	return isInputPresent;
}

void camControlHelper::UpdateZoomFactorLimits()
{
	float minZoomFactor			= 1.0f;

	float minFov				= NetworkInterface::IsGameInProgress() ? m_Metadata.m_Zoom.m_MinFovForNetworkPlay : m_Metadata.m_Zoom.m_MinFov;
	minFov						= Clamp(minFov, g_MinFov, g_MaxFov);
	const float maxFov			= Clamp(m_Metadata.m_Zoom.m_MaxFov, g_MinFov, g_MaxFov);

	float maxZoomFactor			= maxFov / minFov;
	maxZoomFactor				= Max(maxZoomFactor, minZoomFactor);

	if(ms_ShouldOverrideZoomFactorLimits)
	{
		//Ensure the overridden limits are within our pre-defined limits.
		const float overriddenMinZoomFactorToApply = Clamp(ms_OverriddenZoomFactorLimits.x, minZoomFactor, maxZoomFactor);
		const float overriddenMaxZoomFactorToApply = Clamp(ms_OverriddenZoomFactorLimits.y, overriddenMinZoomFactorToApply, maxZoomFactor);

		m_ZoomFactorLimits.Set(overriddenMinZoomFactorToApply, overriddenMaxZoomFactorToApply);
	}
	else
	{
		m_ZoomFactorLimits.Set(minZoomFactor, maxZoomFactor);
	}
}

void camControlHelper::UpdateAccurateModeControl(CControl& control)
{
	if(m_Metadata.m_ShouldUseAccurateModeInput && !m_ShouldIgnoreAccurateModeInputThisUpdate)
	{
		// B* 737346: Do not allow accurate mode state to change while camera is interpolating
		// as it changes the source camera's frame and mess up the interpolation.
		const bool bIsInterpolating = camInterface::GetGameplayDirector().IsCameraInterpolating();
		if (!bIsInterpolating)
		{
#if KEYBOARD_MOUSE_SUPPORT
			// B* 2032581: The way the mouse wheel works we have to always toggle regardless of meta data settings. The reason
			// for this is that the wheel cannot be held forwards or backwards. To make this work more like the sniper rifle on
			// the mouse, we will use wheel forward for enter accurate aim and wheel backwards for exit accurate aim. On the
			// keyboard, this will be two different buttons.
			if(control.GetPedAccurateAim().GetSource().m_DeviceIndex == ioSource::IOMD_KEYBOARD_MOUSE)
			{
				const float aimValue = control.GetPedAccurateAim().GetUnboundNorm();

				// Positive is wheel up/forwards.
				if(aimValue > ioValue::BUTTON_DOWN_THRESHOLD)
				{
					m_IsInAccurateMode = false;
					control.SetInputExclusive(INPUT_ACCURATE_AIM);
				}
				else if(aimValue < -ioValue::BUTTON_DOWN_THRESHOLD)
				{
					m_IsInAccurateMode = true;
					control.SetInputExclusive(INPUT_ACCURATE_AIM);
				}
			}
			else
#endif // KEYBOARD_MOUSE_SUPPORT
			if(m_Metadata.m_ShouldToggleAccurateModeInput)
			{
				if(control.GetPedAccurateAim().IsPressed())
				{
					control.SetInputExclusive(INPUT_ACCURATE_AIM);
					m_IsInAccurateMode = !m_IsInAccurateMode;
				}
			}
			else
			{
				m_IsInAccurateMode = control.GetPedAccurateAim().IsDown();
				if(m_IsInAccurateMode)
				{
					control.SetInputExclusive(INPUT_ACCURATE_AIM);
				}
			}
		}
	}

	m_ShouldIgnoreAccurateModeInputThisUpdate = false;
}

void camControlHelper::Reset()
{
	m_LookAroundSpeed						= 0.0f;
	m_NormalisedLookAroundHeadingInput		= 0.0f;
	m_NormalisedLookAroundPitchInput		= 0.0f;
	m_LookAroundHeadingOffset				= 0.0f;
	m_LookAroundPitchOffset					= 0.0f;
	m_LookAroundHeadingInputPreviousUpdate	= 0.0f;

	if(m_LookAroundInputEnvelope)
	{
		m_LookAroundInputEnvelope->Stop();
	}

	m_LookAroundInputEnvelopeLevel			= 0.0f;

	m_LookAroundDecelerationScaling			= 1.0f;

	m_ZoomFactorSpeed						= 0.0f;

	m_TimeReleasedLookBehindInput			= 0;
	m_LastTimeViewModeControlDisabled		= 0;

	//Reset the persistent zoom factor (to help with streaming) and check that it is within the valid range for this helper.
	ms_ZoomFactor							= 1.0f;
	UpdateZoomFactorLimits();
	ms_ZoomFactor							= Clamp(ms_ZoomFactor, m_ZoomFactorLimits.x, m_ZoomFactorLimits.y);

	m_ShouldUpdateViewModeThisUpdate		= false;
	m_ShouldIgnoreViewModeInputThisUpdate	= false;
	m_ShouldSkipViewModeBlendThisUpdate		= false;
	m_ShouldIgnoreLookAroundInputThisUpdate	= false;
	m_IsLookBehindInputActive				= true;
	m_ShouldIgnoreLookBehindInputThisUpdate = false;

	m_IsLookAroundInputPresent				= false;

	m_IsLookingBehind						= false;
	m_WasLookingBehind						= false;

	m_ShouldIgnoreAccurateModeInputThisUpdate			= false;	
	m_IsInAccurateMode									= false;
	m_IsOverridingViewModeForSpectatorModeThisUpdate	= false; 
	m_ShouldUseOriginalMaxControllerAimSensitivity		= false;

	m_ShouldForceAimSensitivityThisUpdate				= false;
	m_ShouldForceDefaultLookAroundSensitivityThisUpdate	= false;
	m_SkipControlHelperThisUpdate						= false;

#if RSG_ORBIS
	m_WasPlayerAiming						= false;
#endif // RSG_ORBIS
#if RSG_PC
	m_WasUsingKeyboardMouseLastInput		= false;
#endif // RSG_PC

}

#if __BANK
void camControlHelper::AddWidgets(bkBank& bank)
{
	bank.PushGroup("Control helper");
	{
		bank.AddSlider("Overridden aim sensitivity", &ms_DebugAimSensitivitySetting, -1, g_RevisedMaxControllerAimSensitivity, 1);
		bank.AddSlider("Overridden look-around (non-aim) sensitivity", &ms_DebugLookAroundSensitivitySetting, -1, g_RevisedMaxControllerAimSensitivity, 1);
	#if RSG_ORBIS
		bank.AddSlider("Controller Motion Deadzone", &c_fMotionDeadzone, 0.0f, 45.0f, 1.0f);
		bank.AddSlider("Controller Motion vertical input scale", &c_fMotionVerticalScale, 0.0f, 5.0f, 0.5f);
		bank.AddSlider("Controller Motion horizontal input scale", &c_fMotionHorizontalScale, 0.0f, 5.0f, 0.5f);
	#endif
	}
	bank.PopGroup(); //Control helper

	INSTANTIATE_TUNE_GROUP_INT(FIRST_PERSON_TUNE, LOOK_BEHIND_HELD_TIME, 0, 2000, 50);
}

bool camControlHelper::DebugHadSetViewModeForContext(s32& contextIndex, s32& viewModeForContext)
{
	contextIndex		= ms_DebugRequestedContextIndex;
	viewModeForContext	= ms_DebugRequestedViewModeForContext;

	return ms_DebugWasViewModeForContextSetThisUpdate;
}

void camControlHelper::DebugResetHadSetViewModeForContext()
{
	ms_DebugWasViewModeForContextSetThisUpdate	= false;
	ms_DebugRequestedContextIndex				= -1;
	ms_DebugRequestedViewModeForContext			= -1;
}

#endif // __BANK
