//
// camera/gameplay/follow/FollowVehicleCamera.cpp
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#include "camera/gameplay/follow/FollowVehicleCamera.h"

#include "camera/CamInterface.h"
#include "camera/gameplay/GameplayDirector.h"
#include "camera/helpers/BaseFrameShaker.h"
#include "camera/helpers/Collision.h"
#include "camera/helpers/ControlHelper.h"
#include "camera/helpers/Envelope.h"
#include "camera/system/CameraFactory.h"
#include "camera/system/CameraManager.h"
#include "camera/system/CameraMetadata.h"
#include "control/record.h"
#include "frontend/PauseMenu.h"
#include "network/NetworkInterface.h"
#include "Network/Cloud/Tunables.h"
#include "peds/ped.h"
#include "profile/group.h"
#include "profile/page.h"
#include "system/control.h"
#include "system/param.h"
#include "vehicleAi/VehicleIntelligence.h"
#include "vehicles/Automobile.h"
#include "vehicles/Heli.h"
#include "vehicles/Planes.h"
#include "vehicles/Submarine.h"
#include "vehicles/VehicleGadgets.h"


CAMERA_OPTIMISATIONS()

INSTANTIATE_RTTI_CLASS(camFollowVehicleCamera,0x704EC53D)

PF_PAGE(camFollowVehicleCameraPage, "Camera: Follow Vehicle Camera");

PF_GROUP(camFollowVehicleCameraMetrics);
PF_LINK(camFollowVehicleCameraPage, camFollowVehicleCameraMetrics);

PF_VALUE_FLOAT(followVehicleCurrentDuckLevel, camFollowVehicleCameraMetrics);
PF_VALUE_FLOAT(followVehicleCachedDuckLevel, camFollowVehicleCameraMetrics);
PF_VALUE_FLOAT(followVehicleBlendedDuckLevel, camFollowVehicleCameraMetrics);

PF_VALUE_FLOAT(verticalFlightModeBlendLevel, camFollowVehicleCameraMetrics);

bank_bool	camFollowVehicleCamera::ms_ShouldCollide = true;
bool		camFollowVehicleCamera::ms_ConsiderMetadataFlagForIgnoringBikeCollision = true;

#if __BANK
bank_bool   camFollowVehicleCamera::ms_ShouldDisablePullAround = false;
#endif // #if __BANK

//Vertical flight mode custom settings
camEnvelopeMetadata g_CustomInAirEnvelope;
const u32			g_CustomInAirEnvelopeAttackDuration		= 3000;
const s32			g_CustomInAirEnvelopeHoldDuration		= -1;
const s32			g_CustomInAirEnvelopeReleaseDuration	= 500;
const eCurveType	g_CustomInAirEnvelopeAttackCurveType	= CURVE_TYPE_SLOW_IN;
const eCurveType	g_CustomInAirEnvelopeReleaseCurveType	= CURVE_TYPE_LINEAR;

const Vector2		g_VFMOrbitPitchLimits										= Vector2(-65.0f, 45.0f);
const bool			g_VFMShouldBlendOutWhenAttachParentIsOnGround				= true;
const bool			g_VFMShouldBlendWithAttachParentMatrixForRelativeOrbitBlend	= false;
const float			g_VFMHeadingPullAroundMaxMoveSpeed							= 0.0f;
const float			g_VFMHeadingPullAroundSpringConstant						= 75.0f;
const float			g_VFMPitchPullAroundMaxMoveSpeed							= 0.0f;
const float			g_VFMPitchPullAroundSpringConstant							= 5.0f;
const bool			g_VFMShouldPullAroundToBasicAttachParentMatrix				= true;
const float			g_VFMMinAttachParentApproachSpeedForPitchLock				= 99999.0f;
const Vector2		g_VFMOrbitDistanceLimitsForBasePosition						= Vector2(0.0f, 99999.0f);

camFollowVehicleCamera::camFollowVehicleCamera(const camBaseObjectMetadata& metadata)
: camFollowCamera(metadata)
, m_Metadata(static_cast<const camFollowVehicleCameraMetadata&>(metadata))
, m_VehicleCustomSettingsMetadata(NULL)
, m_CollisionRootPositionForMostRecentOverheadCollision(VEC3_LARGE_FLOAT)
, m_HandBrakeInputEnvelope(NULL)
, m_DuckUnderOverheadCollisionEnvelope(NULL)
, m_DuckLevelForMostRecentOverheadCollision(0.0f)
, m_DuckUnderOverheadCollisionBlendLevel(0.0f)
, m_WaterLevelForBoatWaterEntrySample(0.0f)
, m_VerticalFlightModeBlendLevel(0.0f)
, m_ShouldForceFirstPersonViewMode(false)
, m_MotionBlurForAttachParentImpulseStartTime(0.0f)
, m_MotionBlurPreviousAttachParentImpuse(0.0f)
, m_MotionBlurPeakAttachParentImpulse(0.0f)
, m_ShouldUseStuntSettings(false)
{
	const camEnvelopeMetadata* envelopeMetadata =
		camFactory::FindObjectMetadata<camEnvelopeMetadata>(m_Metadata.m_HandBrakeSwingSettings.m_HandBrakeInputEnvelopeRef.GetHash());
	if(envelopeMetadata)
	{
		m_HandBrakeInputEnvelope = camFactory::CreateObject<camEnvelope>(*envelopeMetadata);
		if(cameraVerifyf(m_HandBrakeInputEnvelope, "A follow-vehicle camera (name: %s, hash: %u) failed to create an envelope (name: %s, hash: %u)",
			GetName(), GetNameHash(), SAFE_CSTRING(envelopeMetadata->m_Name.GetCStr()),
			envelopeMetadata->m_Name.GetHash()))
		{
			m_HandBrakeInputEnvelope->SetUseGameTime(true);
		}
	}

	envelopeMetadata = camFactory::FindObjectMetadata<camEnvelopeMetadata>(m_Metadata.m_DuckUnderOverheadCollisionSettings.m_EnvelopeRef.GetHash());
	if(envelopeMetadata)
	{
		m_DuckUnderOverheadCollisionEnvelope = camFactory::CreateObject<camEnvelope>(*envelopeMetadata);
		if(cameraVerifyf(m_DuckUnderOverheadCollisionEnvelope,
			"A follow-vehicle camera (name: %s, hash: %u) failed to create an envelope (name: %s, hash: %u)", GetName(), GetNameHash(),
			SAFE_CSTRING(envelopeMetadata->m_Name.GetCStr()), envelopeMetadata->m_Name.GetHash()))
		{
			m_DuckUnderOverheadCollisionEnvelope->SetUseGameTime(true);
		}
	}
}

camFollowVehicleCamera::~camFollowVehicleCamera()
{
	if(m_HandBrakeInputEnvelope)
	{
		delete m_HandBrakeInputEnvelope;
	}

	if(m_DuckUnderOverheadCollisionEnvelope)
	{
		delete m_DuckUnderOverheadCollisionEnvelope;
	}
}

const camVehicleCustomSettingsMetadataDoorAlignmentSettings& camFollowVehicleCamera::GetVehicleEntryExitSettings() const
{
	//Allow vehicle custom settings to override the settings in the camera metadata.
	const camVehicleCustomSettingsMetadataDoorAlignmentSettings* pSettingToConsider;
	if (m_VehicleCustomSettingsMetadata && m_VehicleCustomSettingsMetadata->m_DoorAlignmentSettings.m_ShouldConsiderData)
	{
		pSettingToConsider = &m_VehicleCustomSettingsMetadata->m_DoorAlignmentSettings;
	}
	else
	{
		pSettingToConsider = &m_Metadata.m_DoorAlignmentSettings;
	}

	return *pSettingToConsider;
}

bool camFollowVehicleCamera::Update()
{
	//Determine the attach vehicle.
	const CVehicle* attachVehicle = camInterface::GetGameplayDirector().GetFollowVehicle();
	if(!attachVehicle)
	{
		return false;
	}

	if(m_AttachParent.Get() != attachVehicle)
	{
		//We don't have an attach vehicle or the player was warped into another vehicle, so change the attach vehicle.
		AttachTo(attachVehicle);
	}

	UpdateHighSpeedShake();
	UpdateWaterEntryShake();

	//Temporary means to disable follow-vehicle collision in code (vs RAVE.)
	if(ms_ShouldCollide)
	{
		if(!m_Collision)
		{
			//Create a collision helper if specified in metadata.
			const camCollisionMetadata* collisionMetadata = camFactory::FindObjectMetadata<camCollisionMetadata>(m_Metadata.m_CollisionRef.GetHash());
			if(collisionMetadata)
			{
				m_Collision = camFactory::CreateObject<camCollision>(m_Metadata.m_CollisionRef);
				cameraAssertf(m_Collision, "A follow-vehicle camera (name: %s, hash: %u) failed to create a collision helper (name: %s, hash: %u)",
					GetName(), GetNameHash(), SAFE_CSTRING(m_Metadata.m_CollisionRef.GetCStr()), m_Metadata.m_CollisionRef.GetHash());
			}
		}
	}
	else if(m_Collision)
	{
		delete m_Collision;
		m_Collision = NULL;
	}

	UpdateEnforcedFirstPersonViewMode();

	UpdateVerticalFlightModeBlendLevel();

	const bool hasSucceeded = camFollowCamera::Update();

	UpdateCustomVehicleEntryExitBehaviour();

	UpdateParachuteBehavior();

	return hasSucceeded;
}

void camFollowVehicleCamera::GetAttachParentVelocity(Vector3& velocity) const
{
	//NOTE: m_AttachParent && m_AttachParent->GetIsTypeVehicle() have already been validated.
	const CVehicle* attachVehicle = static_cast<const CVehicle*>(m_AttachParent.Get());

	// Return the vehicle's velocity instead of (m_BaseAttachPosition - m_BaseAttachPositionOnPreviousUpdate) * gameInvTimeStep;
	// This prevents bad velocities from propagating through the camera when script reposition the vehicle.
	velocity = attachVehicle->GetVelocity();

	if(attachVehicle->InheritsFromSubmarine() && attachVehicle->GetModelNameHash() == ATSTRINGHASH("KOSATKA", 0x4FAF0D70))
	{
		const CSubmarine* attachSubmarine = static_cast<const CSubmarine*>(attachVehicle);
		const float propellorSpeed0 = attachSubmarine->GetSubHandling()->GetPropellorSpeed(0);
		const float propellorSpeed1 = attachSubmarine->GetSubHandling()->GetPropellorSpeed(1);
		const float averagePropellorsSpeed = (propellorSpeed0 + propellorSpeed1)*0.5f*(-1.0f);
		velocity = VEC3V_TO_VECTOR3(attachSubmarine->GetMatrix().b()) * averagePropellorsSpeed;
	}
}

void camFollowVehicleCamera::UpdateEnforcedFirstPersonViewMode()
{
	//Force a first-person view mode and ignore all view-mode input if we're attached to a tank that is hanging beneath a Cargobob helicopter.

	m_ShouldForceFirstPersonViewMode = false;

	if(!m_ControlHelper)
	{
		return;
	}

	//NOTE: m_AttachParent && m_AttachParent->GetIsTypeVehicle() have already been validated.
	const CVehicle* attachVehicle	= (CVehicle*)m_AttachParent.Get();
	const bool isTank				= attachVehicle->IsTank();
	if(!isTank)
	{
		return;
	}

	const bool isVehicleAttachedToCargobob = ComputeIsVehicleAttachedToCargobob(*attachVehicle);
	if(!isVehicleAttachedToCargobob)
	{
		return;
	}

	m_ShouldForceFirstPersonViewMode = true;

	m_ControlHelper->IgnoreViewModeInputThisUpdate();
}

bool camFollowVehicleCamera::ComputeIsVehicleAttachedToCargobob(const CVehicle& vehicle) const
{
	TUNE_BOOL(shouldTreatFollowVehicleAsAttachedToCargobobForCamera, false)
	if(shouldTreatFollowVehicleAsAttachedToCargobobForCamera)
	{
		return true;
	}

	const fwEntity* vehicleParent = vehicle.GetAttachParent();
	if(!vehicleParent || !static_cast<const CEntity*>(vehicleParent)->GetIsTypeVehicle())
	{
		return false;
	}

	const CVehicle* vehicleParentVehicle = static_cast<const CVehicle*>(vehicleParent);
	if(!vehicleParentVehicle->InheritsFromHeli() || !static_cast<const CHeli*>(vehicleParentVehicle)->GetIsCargobob())
	{
		return false;
	}

	bool isAttachedToCargobob = false;

	//Loop through gadgets until we found the pick up rope.
	const s32 numVehicleGadgets = vehicleParentVehicle->GetNumberOfVehicleGadgets();
	for(s32 i=0; i<numVehicleGadgets; i++)
	{
		const CVehicleGadget* vehicleGadget = vehicleParentVehicle->GetVehicleGadget(i);
		if(vehicleGadget && (vehicleGadget->GetType() == VGT_PICK_UP_ROPE || vehicleGadget->GetType() == VGT_PICK_UP_ROPE_MAGNET))
		{
			const CVehicleGadgetPickUpRope* pickUpRope	= static_cast<const CVehicleGadgetPickUpRope*>(vehicleGadget);
			isAttachedToCargobob						= (pickUpRope->GetAttachedVehicle() == &vehicle);

			break;
		}
	}

	return isAttachedToCargobob;
}

const CEntity* camFollowVehicleCamera::ComputeIsVehicleAttachedToMagnet(const CVehicle& vehicle) const
{		
	const CEntity* vehicleAttachParentEntity = static_cast<const CEntity*>(vehicle.GetAttachParent());
	if(!(vehicleAttachParentEntity && vehicleAttachParentEntity->GetIsTypeObject()))
	{
		return NULL;
	}	

	if(vehicleAttachParentEntity->GetArchetype()->GetModelNameHash() != ATSTRINGHASH("hei_prop_heist_magnet", 0x7a897074))
	{
		return NULL;
	}

	return vehicleAttachParentEntity;
}

void camFollowVehicleCamera::UpdateVerticalFlightModeBlendLevel()
{
	const bool isVerticalFlightModeAvailable = ComputeIsVerticalFlightModeAvailable();
	if (!isVerticalFlightModeAvailable)
	{
		m_VerticalFlightModeBlendLevel = 0.0f;
		return;
	}

	if (!m_AttachParentInAirEnvelope)
	{
		g_CustomInAirEnvelope.m_AttackDuration = g_CustomInAirEnvelopeAttackDuration;
		g_CustomInAirEnvelope.m_HoldDuration = g_CustomInAirEnvelopeHoldDuration;
		g_CustomInAirEnvelope.m_ReleaseDuration = g_CustomInAirEnvelopeReleaseDuration;
		g_CustomInAirEnvelope.m_AttackCurveType = g_CustomInAirEnvelopeAttackCurveType;
		g_CustomInAirEnvelope.m_ReleaseCurveType = g_CustomInAirEnvelopeReleaseCurveType;

		m_AttachParentInAirEnvelope = camFactory::CreateObject<camEnvelope>(g_CustomInAirEnvelope);
		if(cameraVerifyf(m_AttachParentInAirEnvelope,
			"A follow-vehicle camera (name: %s, hash: %u) failed to create an in air envelope for vertical flight mode)", GetName(), GetNameHash()))
		{
			m_AttachParentInAirEnvelope->SetUseGameTime(true);
		}
	}

	//NOTE: m_AttachParent->GetIsTypeVehicle() and attachVehicle->InheritsFromPlane() have already been validated.
	const CPlane* attachPlane = static_cast<const CPlane*>(m_AttachParent.Get());

	m_VerticalFlightModeBlendLevel	= attachPlane->GetVerticalFlightModeRatio();
	m_VerticalFlightModeBlendLevel	= Clamp(m_VerticalFlightModeBlendLevel, 0.0f, 1.0f);

	PF_SET(verticalFlightModeBlendLevel, m_VerticalFlightModeBlendLevel);
}

bool camFollowVehicleCamera::ComputeIsVerticalFlightModeAvailable() const
{
	if (!m_AttachParent)
	{
		return false;
	}
	if (!m_AttachParent->GetIsTypeVehicle())
	{
		return false;
	}

	const CVehicle* attachVehicle = (CVehicle*)m_AttachParent.Get();
	if(!attachVehicle->InheritsFromPlane())
	{
		return false;
	}

	const CPlane* attachPlane = static_cast<const CPlane*>(attachVehicle);
	const bool isVerticalFlightModeAvailable = attachPlane->GetVerticalFlightModeAvaliable();

	return isVerticalFlightModeAvailable;
}

void camFollowVehicleCamera::UpdateCustomVehicleEntryExitBehaviour()
{
	const camGameplayDirector& gameplayDirector		= camInterface::GetGameplayDirector();
	const s32 vehicleEntryExitState					= gameplayDirector.GetVehicleEntryExitState();
	const s32 vehicleEntryExitStateOnPreviousUpdate	= gameplayDirector.GetVehicleEntryExitStateOnPreviousUpdate();

	if((vehicleEntryExitState == camGameplayDirector::ENTERING_VEHICLE) && (vehicleEntryExitStateOnPreviousUpdate != camGameplayDirector::ENTERING_VEHICLE) && !m_HintHelper)
	{
		//Auto-level the follow pitch on starting to enter a vehicle.

		//NOTE: We use the simple matrix of the attach parent directly, not m_AttachParentMatrix, when overriding a relative orientation. This
		//reduces the scope for the parent matrix to be upside-down in world space, as this can flip the decomposed parent heading.
		//NOTE: m_AttachParent has already been validated.
		const Matrix34 attachParentMatrix			= MAT34V_TO_MATRIX34(m_AttachParent->GetMatrix());
		const float attachParentFrontDotBaseFront	= attachParentMatrix.b.Dot(m_BaseFront);
		const float attachParentPitch				= camFrame::ComputePitchFromMatrix(attachParentMatrix);
		const float relativePitchToApply			= RampValue(attachParentFrontDotBaseFront, -1.0f, 1.0f, -2.0f * attachParentPitch, 0.0f);

		SetRelativePitch(relativePitchToApply, m_Metadata.m_VehicleEntryExitPitchLevelSmoothRate, false);
	}
}

void camFollowVehicleCamera::UpdateHighSpeedShake()
{
	const camFollowVehicleCameraMetadataHighSpeedShakeSettings& settings = m_Metadata.m_HighSpeedShakeSettings;

	const u32 shakeHash = settings.m_ShakeRef.GetHash();
	if(shakeHash == 0)
	{
		//No high-speed shake was specified in metadata.
		return;
	}

	if(ShouldBlockHighSpeedEffects())
	{
		return;
	}

	camBaseFrameShaker* frameShaker = FindFrameShaker(shakeHash);
	if(!frameShaker)
	{
		//Try to kick-off the specified high-speed shake.
		frameShaker = Shake(shakeHash);
		if(!frameShaker)
		{
			return;
		}
	}

	//Consider the forward speed only.
	const Vector3 vehicleFront		= VEC3V_TO_VECTOR3(m_AttachParent->GetTransform().GetB());
	const float forwardSpeed		= DotProduct(m_BaseAttachVelocityToConsider, vehicleFront);
	const float desiredSpeedFactor	= SlowInOut(RampValueSafe(forwardSpeed, settings.m_MinForwardSpeed, settings.m_MaxForwardSpeed, 0.0f, 1.0f));

	float speedFactorToApply		= m_HighSpeedShakeSpring.Update(desiredSpeedFactor, settings.m_SpringConstant);
	speedFactorToApply				= Clamp(speedFactorToApply, 0.0f, 1.0f); //For safety.

	frameShaker->SetAmplitude(speedFactorToApply);
}

void camFollowVehicleCamera::UpdateWaterEntryShake()
{
	const camFollowVehicleCameraMetadataWaterEntryShakeSettings& settings = m_Metadata.m_WaterEntryShakeSettings;

	const u32 shakeHash = settings.m_ShakeRef.GetHash();
	if(shakeHash == 0)
	{
		//No shake was specified in metadata.
		return;
	}

	const float waterLevelForBoatWaterEntrySampleOnPreviousUpdate = m_WaterLevelForBoatWaterEntrySample;

	m_WaterLevelForBoatWaterEntrySample = ComputeWaterLevelForBoatWaterEntrySample();

 	const bool hasBoatEnteredWater = (waterLevelForBoatWaterEntrySampleOnPreviousUpdate < SMALL_FLOAT) &&
 										(m_WaterLevelForBoatWaterEntrySample >= SMALL_FLOAT);
	if(!hasBoatEnteredWater)
	{
		return;
	}

	//Ignore minor impacts on entry.
	if(-m_BaseAttachVelocityToConsider.z < settings.m_DownwardSpeedLimits.x)
	{
		return;
	}

	//Scale the amplitude based upon the downward speed of the boat.
	const float amplitude	= RampValueSafe(-m_BaseAttachVelocityToConsider.z, settings.m_DownwardSpeedLimits.x, settings.m_DownwardSpeedLimits.y,
								settings.m_AmplitudeLimits.x, settings.m_AmplitudeLimits.y);
	if(amplitude >= SMALL_FLOAT)
	{
		Shake(shakeHash, amplitude, settings.m_MaxShakeInstances);
	}
}

float camFollowVehicleCamera::ComputeWaterLevelForBoatWaterEntrySample() const
{
	//NOTE: m_AttachParent && m_AttachParent->GetIsTypeVehicle() have already been validated.
	const CVehicle* attachVehicle	= static_cast<const CVehicle*>(m_AttachParent.Get());
	const bool isBoat				= attachVehicle->InheritsFromBoat();
	if(!isBoat)
	{
		return 0.0f;
	}

	//NOTE: This logic is derived from CVfxWater::ProcessVfxBoatEntry and could/should share common code.

	const CBuoyancyInfo* buoyancyInfo = attachVehicle->m_Buoyancy.GetBuoyancyInfo(attachVehicle);
	if(!buoyancyInfo)
	{
		return 0.0f;
	}

	const int numWaterSampleRows = buoyancyInfo->m_nNumBoatWaterSampleRows;
	if(numWaterSampleRows < 2)
	{
		return 0.0f;
	}

	const int lastRowWaterSampleIndex		= buoyancyInfo->m_nBoatWaterSampleRowIndices[numWaterSampleRows-1];
	const int secondLastRowWaterSampleIndex	= buoyancyInfo->m_nBoatWaterSampleRowIndices[numWaterSampleRows-2];
	const int secondLastRowNumWaterSamples	= lastRowWaterSampleIndex - secondLastRowWaterSampleIndex;
	const int entryWaterSampleIndex			= secondLastRowWaterSampleIndex + ((secondLastRowNumWaterSamples) / 2);

	const float waterLevel					= attachVehicle->m_Buoyancy.GetWaterLevelOnSample(entryWaterSampleIndex);

	return waterLevel;
}

bool camFollowVehicleCamera::ComputeIsAttachParentInAir() const
{
	//NOTE: m_AttachParent && m_AttachParent->GetIsTypeVehicle() have already been validated.
	//NOTE: We need to use a const_cast here, as IsInAir appears to rely on some non-const functions.
	CVehicle* attachVehicle			= const_cast<CVehicle*>(static_cast<const CVehicle*>(m_AttachParent.Get()));
	bool bRecogniseCollision = false;
	CCollisionHistory* pImpulseHistory = attachVehicle->GetFrameCollisionHistory();
	if (pImpulseHistory && (pImpulseHistory->GetFirstBuildingCollisionRecord() || pImpulseHistory->GetFirstObjectCollisionRecord() || pImpulseHistory->GetFirstOtherCollisionRecord()))
	{
		bRecogniseCollision = true;
	}

	const bool isAttachParentInAir	= attachVehicle->IsInAir(false) && !bRecogniseCollision;

	return isAttachParentInAir;
}

float camFollowVehicleCamera::ComputeMinAttachParentApproachSpeedForPitchLock() const
{
	float minAttachParentApproachSpeedForPitchLock = camFollowCamera::ComputeMinAttachParentApproachSpeedForPitchLock();

	minAttachParentApproachSpeedForPitchLock = Lerp(m_VerticalFlightModeBlendLevel, minAttachParentApproachSpeedForPitchLock, g_VFMMinAttachParentApproachSpeedForPitchLock);

	return minAttachParentApproachSpeedForPitchLock;
}

bool camFollowVehicleCamera::ComputeShouldCutToDesiredOrbitDistanceLimits() const
{
	bool shouldCut = camFollowCamera::ComputeShouldCutToDesiredOrbitDistanceLimits();

	if(!shouldCut && m_Metadata.m_ShouldForceCutToOrbitDistanceLimitsForThirdPersonFarViewMode && m_ControlHelper)
	{
		const s32 viewMode = m_ControlHelper->GetViewMode();
		shouldCut = (viewMode == camControlHelperMetadataViewMode::THIRD_PERSON_FAR);
	}

	return shouldCut;
}

void camFollowVehicleCamera::UpdateOrbitPitchLimits()
{
	camFollowCamera::UpdateOrbitPitchLimits();

	//blend the pitch limits for when the vehicle is airborne
	m_OrbitPitchLimits.Lerp(m_AttachParentInAirBlendLevel, m_OrbitPitchLimits, m_Metadata.m_OrbitPitchLimitsWhenAirborne*DtoR);

	//Blend to custom orbit pitch limits for planes with an active vertical flight mode.
	Vector2 customOrbitPitchLimits;
	customOrbitPitchLimits.SetScaled(g_VFMOrbitPitchLimits, DtoR);
	m_OrbitPitchLimits.Lerp(m_VerticalFlightModeBlendLevel, m_OrbitPitchLimits, customOrbitPitchLimits);
}

void camFollowVehicleCamera::ApplyPullAroundOrientationOffsets(float& followHeading, float& followPitch)
{
	if (camInterface::GetGameplayDirector().IsAttachVehicleMiniTank())
	{
		return;
	}

#if __BANK
    if( ms_ShouldDisablePullAround )
    {
        ms_ShouldDisablePullAround = false;
        return;
    }
#endif // #if __BANK

	//Apply the basic pull-around behaviour.
	camFollowCamera::ApplyPullAroundOrientationOffsets(followHeading, followPitch);

	//Apply the special hand-brake swing effect.

	//NOTE: m_AttachParent && m_AttachParent->GetIsTypeVehicle() have already been validated.
	const CVehicle* attachVehicle = (CVehicle*)m_AttachParent.Get();
	if((attachVehicle->GetVehicleType() == VEHICLE_TYPE_CAR || attachVehicle->GetVehicleType() == VEHICLE_TYPE_AMPHIBIOUS_AUTOMOBILE || attachVehicle->GetVehicleType() == VEHICLE_TYPE_SUBMARINECAR) || (attachVehicle->GetVehicleType() == VEHICLE_TYPE_BIKE) || (attachVehicle->GetVehicleType() == VEHICLE_TYPE_QUADBIKE) || (attachVehicle->GetVehicleType() == VEHICLE_TYPE_AMPHIBIOUS_QUADBIKE))
	{
		CControl* control					= camInterface::GetGameplayDirector().GetActiveControl();
		const bool isHandBrakeInputActive	= !(attachVehicle->pHandling->hFlags & HF_NO_HANDBRAKE) &&
												control && attachVehicle->GetIsHandBrakePressed(control);

		float handBrakeInputEnvelopeLevel;
		if(m_HandBrakeInputEnvelope)
		{
			if(isHandBrakeInputActive)
			{
				m_HandBrakeInputEnvelope->AutoStart();
			}

			handBrakeInputEnvelopeLevel = m_HandBrakeInputEnvelope->Update();
		}
		else
		{
			//We don't have an envelope, so just use the input state directly.
			handBrakeInputEnvelopeLevel = isHandBrakeInputActive ? 1.0f : 0.0f;
		}

		//NOTE: The hand-brake swing effect is blended out when there is look-around input, otherwise it would interfere with the look-around.
		//NOTE: m_ControlHelper has already been validated.
		const float lookAroundScaling = 1.0f - m_ControlHelper->GetLookAroundInputEnvelopeLevel();

		//NOTE: The hand-brake swing effect is blended out when the attach vehicle is in the air, to prevent undesirable behaviour when sliding into the air.
		const float inAirScaling = (1.0f - m_AttachParentInAirBlendLevel);

		//Compute a blend level based upon the speed of the vehicle in relation to its right vector, i.e. lateral-skid speed.
		const float lateralSkidSpeed	= DotProduct(VEC3V_TO_VECTOR3(attachVehicle->GetTransform().GetA()), m_BaseAttachVelocityToConsider);
		const float lateralSkidDir		= Sign(lateralSkidSpeed);
		const float absLateralSkidSpeed	= Abs(lateralSkidSpeed);
		const float absSkidSpeedScaling	= RampValueSafe(absLateralSkidSpeed, m_Metadata.m_HandBrakeSwingSettings.m_MinLateralSkidSpeed,
											m_Metadata.m_HandBrakeSwingSettings.m_MaxLateralSkidSpeed, 0.0f, 1.0f);
 
 		const float desiredHandBrakeEffectScaling = handBrakeInputEnvelopeLevel * lookAroundScaling * inAirScaling * lateralSkidDir * absSkidSpeedScaling;

		m_HandBrakeEffectSpring.Update(desiredHandBrakeEffectScaling, m_Metadata.m_HandBrakeSwingSettings.m_SpringConstant);

		const float handBrakeEffectScaling	= m_HandBrakeEffectSpring.GetResult();

		const float maxSwingSpeed			= m_Metadata.m_HandBrakeSwingSettings.m_SwingSpeedAtMaxSkidSpeed * DtoR;
		const float gameTimeStep			= fwTimer::GetTimeStep();
		const float maxSwingDelta			= maxSwingSpeed * gameTimeStep;

		followHeading						+= handBrakeEffectScaling * maxSwingDelta;
	}
	else
	{
		m_HandBrakeEffectSpring.Reset();
	}
}

void camFollowVehicleCamera::ComputeLookAroundOrientationOffsets(float& headingOffset, float& pitchOffset) const
{
	bool isLookAroundInputValid = true;

	//NOTE: m_AttachParent && m_AttachParent->GetIsTypeVehicle() have already been validated.
	const CVehicle* attachVehicle					= (CVehicle*)m_AttachParent.Get();
	const CVehicleIntelligence* vehicleIntelligence	= attachVehicle->GetIntelligence();
	if(vehicleIntelligence)
	{
		const CTaskManager* taskManager = vehicleIntelligence->GetTaskManager();
		if(taskManager)
		{
			if(taskManager->FindTaskByTypeActive(VEHICLE_TASK_TREE_PRIMARY, CTaskTypes::TASK_VEHICLE_PLAYER_DRIVE_DIGGER_ARM))
			{
				//NOTE: The look-around input controls the articulated arm when this task is active, so we must disable camera look-around.
				isLookAroundInputValid = false;
			}
		}
	}

	if(isLookAroundInputValid)
	{
		camFollowCamera::ComputeLookAroundOrientationOffsets(headingOffset, pitchOffset);
	}
	else
	{
		headingOffset	= 0.0f;
		pitchOffset		= 0.0f;
	}
}

float camFollowVehicleCamera::ComputeOrbitPitchOffset() const
{
	float orbitPitchOffset = camFollowCamera::ComputeOrbitPitchOffset();

	//Apply a custom orbit pitch offset for the third-person far view mode.
	if(m_ControlHelper)
	{
		const s32 viewMode = m_ControlHelper->GetViewMode();
		if(viewMode == camControlHelperMetadataViewMode::THIRD_PERSON_FAR)
		{
			orbitPitchOffset += (m_Metadata.m_ExtraOrbitPitchOffsetForThirdPersonFarViewMode * DtoR);
		}
	}

	const int cameraHeightSetting	= CPauseMenu::GetMenuPreference(PREF_CAMERA_HEIGHT);
	bool shouldUseHighAngleMode		= (cameraHeightSetting == 1);

	camInterface::GetGameplayDirector().GetScriptOverriddenFollowVehicleCameraHighAngleModeThisUpdate(shouldUseHighAngleMode);

	if(shouldUseHighAngleMode)
	{
		orbitPitchOffset += (m_Metadata.m_ExtraOrbitPitchOffsetForHighAngleMode * DtoR);
	}

	//Reduce the orbit pitch offset when ducking under overhead collision.
	const float maxDuckLevel		= m_DuckUnderOverheadCollisionSpring.GetResult();
	const float blendedDuckLevel	= m_DuckUnderOverheadCollisionBlendLevel * maxDuckLevel;
	orbitPitchOffset				= Lerp(blendedDuckLevel, orbitPitchOffset,
										m_Metadata.m_DuckUnderOverheadCollisionSettings.m_OrbitPitchOffsetWhenFullyDucked * DtoR);

	return orbitPitchOffset;
}

void camFollowVehicleCamera::UpdateCollision(Vector3& cameraPosition, float &orbitHeadingOffset, float &orbitPitchOffset)
{
	if(m_Collision == NULL)
	{
		return;
	}

	//Ignore occlusion with nearby bikes. Temporary work-around for B*171224.
	//NOTE: m_AttachParent && m_AttachParent->GetIsTypeVehicle() have already been validated.
	const CVehicle* attachVehicle							= (CVehicle*)m_AttachParent.Get();
	const CVehicleIntelligence* attachVehicleIntelligence	= attachVehicle->GetIntelligence();

	const bool considerMetadataFlagForIgnoringBikeCollision = NetworkInterface::IsGameInProgress() && ms_ConsiderMetadataFlagForIgnoringBikeCollision;
	if(attachVehicleIntelligence && (m_Metadata.m_IgnoreOcclusionWithNearbyBikes || !considerMetadataFlagForIgnoringBikeCollision))
	{
		const CEntityScannerIterator nearbyVehicles = attachVehicleIntelligence->GetVehicleScanner().GetIterator();

		for(const CEntity* nearbyEntity = nearbyVehicles.GetFirst(); nearbyEntity; nearbyEntity = nearbyVehicles.GetNext())
		{
			const CVehicle* nearbyVehicle = static_cast<const CVehicle*>(nearbyEntity);
			if(nearbyVehicle->InheritsFromBike())
			{
				m_Collision->IgnoreEntityThisUpdate(*nearbyVehicle);
			}
		}
	}

	const s32 numVehicleGadgets = attachVehicle->GetNumberOfVehicleGadgets();
	for(s32 gadgetIndex=0; gadgetIndex<numVehicleGadgets; gadgetIndex++)
	{
		const CVehicleGadget* vehicleGadget = attachVehicle->GetVehicleGadget(gadgetIndex);

		if(vehicleGadget)
		{
			if(vehicleGadget->GetType() == VGT_FORKS)
			{
				//If the attach vehicle has a forks gadget, ignore collision with, and push beyond, any objects attached to it.
				const atArray<CPhysical*>& attachedObjects	= static_cast<const CVehicleGadgetForks*>(vehicleGadget)->GetListOfAttachedObjects();
				const s32 numAttachedObjects				= attachedObjects.GetCount();
				for(s32 objectIndex=0; objectIndex<numAttachedObjects; objectIndex++)
				{
					const CPhysical* attachedObject = attachedObjects[objectIndex];
					if(attachedObject)
					{
						m_Collision->IgnoreEntityThisUpdate(*attachedObject);
						m_Collision->PushBeyondEntityIfClippingThisUpdate(*attachedObject);
					}
				}
			}
			else if(vehicleGadget->GetType() == VGT_HANDLER_FRAME)
			{
				// ... likewise for the Handler when a container is attached.
				const CEntity* attachedObject = static_cast<const CVehicleGadgetHandlerFrame*>(vehicleGadget)->GetAttachedObject();
				if(attachedObject)
				{
					m_Collision->IgnoreEntityThisUpdate(*attachedObject);
					m_Collision->PushBeyondEntityIfClippingThisUpdate(*attachedObject);
				}
			}
		}
	}

	// Find if the current vehicle is attached to a magnet (b*2078851)
	const CEntity* magnetAttachment = ComputeIsVehicleAttachedToMagnet(*attachVehicle);
	if(magnetAttachment)
	{
		// Ignore the magnet and all of its parents
		const CEntity* currentEntity = magnetAttachment;
		while(currentEntity)
		{
			m_Collision->IgnoreEntityThisUpdate(*currentEntity);
			m_Collision->PushBeyondEntityIfClippingThisUpdate(*currentEntity);

			currentEntity = static_cast<const CEntity*>(currentEntity->GetAttachParent());
		}		
	}

	camFollowCamera::UpdateCollision(cameraPosition, orbitHeadingOffset, orbitPitchOffset);
}

void camFollowVehicleCamera::UpdateCollisionRootPosition(const Vector3& cameraPosition, float& collisionTestRadius)
{
	const Vector3 collisionRootPositionOnPreviousUpdate(m_CollisionRootPosition);

	camFollowCamera::UpdateCollisionRootPosition(cameraPosition, collisionTestRadius);

	const float preCollisionOrbitDistance = m_PivotPosition.Dist(cameraPosition);

	UpdateDuckingUnderOverheadCollision(collisionRootPositionOnPreviousUpdate, preCollisionOrbitDistance, collisionTestRadius);
}

void camFollowVehicleCamera::UpdateDuckingUnderOverheadCollision(const Vector3& collisionRootPositionOnPreviousUpdate, float preCollisionOrbitDistance,
	float collisionTestRadius)
{
	const camFollowVehicleCameraMetadataDuckUnderOverheadCollisionSettings& settings = m_Metadata.m_DuckUnderOverheadCollisionSettings;

	const float duckLevel	= ComputeDuckUnderOverheadCollisionLevel(collisionRootPositionOnPreviousUpdate, preCollisionOrbitDistance, collisionTestRadius);
	bool shouldDuck			= (duckLevel >= SMALL_FLOAT);
	if(shouldDuck)
	{
		m_DuckLevelForMostRecentOverheadCollision				= Max(m_DuckLevelForMostRecentOverheadCollision, duckLevel);
		m_CollisionRootPositionForMostRecentOverheadCollision	= m_CollisionRootPosition;
	}
	else
	{
		//Sustain ducking whilst the attach vehicle is in a tunnel, for improved continuity.
		//NOTE: m_AttachParent && m_AttachParent->GetIsTypeVehicle() have already been validated.
		const CVehicle* attachVehicle		= (CVehicle*)m_AttachParent.Get();
		const CPortalTracker* portalTracker	= attachVehicle->GetPortalTracker();
		const CInteriorInst* interiorInst	= portalTracker ? portalTracker->GetInteriorInst() : NULL;
		//NOTE: We exclude subway/metro interiors (where the GroupId is 1.)
		const bool isAttachVehicleInTunnel	= (interiorInst && (interiorInst->GetGroupId() > 1));
		if(isAttachVehicleInTunnel && (m_DuckLevelForMostRecentOverheadCollision >= SMALL_FLOAT))
		{
			//Update the cached collision root position to prevent the ducking from terminating as soon as the vehicle exits the tunnel.
			m_CollisionRootPositionForMostRecentOverheadCollision = m_CollisionRootPosition;

			shouldDuck = true;
		}
		else
		{
			const float distance2FromMostRecentOverheadCollision = m_CollisionRootPosition.Dist2(m_CollisionRootPositionForMostRecentOverheadCollision);

			shouldDuck = distance2FromMostRecentOverheadCollision < (settings.m_MaxDistanceToPersist * settings.m_MaxDistanceToPersist);
		}
	}

	const bool shouldCutDuckingBehaviour =	((m_NumUpdatesPerformed == 0) || m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutPosition) ||
												m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutOrientation));

	bool isDuckingActive;

	if(m_DuckUnderOverheadCollisionEnvelope)
	{
		if(shouldCutDuckingBehaviour)
		{
			if(shouldDuck)
			{
				m_DuckUnderOverheadCollisionEnvelope->Start(1.0f);
			}
			else
			{
				m_DuckUnderOverheadCollisionEnvelope->Stop();
			}
		}
		else
		{
			m_DuckUnderOverheadCollisionEnvelope->AutoStartStop(shouldDuck);
		}

		m_DuckUnderOverheadCollisionBlendLevel = m_DuckUnderOverheadCollisionEnvelope->Update();
		m_DuckUnderOverheadCollisionBlendLevel = Clamp(m_DuckUnderOverheadCollisionBlendLevel, 0.0f, 1.0f);
		m_DuckUnderOverheadCollisionBlendLevel = SlowIn(SlowInOut(m_DuckUnderOverheadCollisionBlendLevel));

		isDuckingActive = m_DuckUnderOverheadCollisionEnvelope->IsActive();
	}
	else
	{
		//We don't have an envelope, so just use the input state directly.
		m_DuckUnderOverheadCollisionBlendLevel = shouldDuck ? 1.0f : 0.0f;

		isDuckingActive = shouldDuck;
	}

	if(isDuckingActive)
	{
		//NOTE: We apply damping to the cached duck level in order to smooth any discontinuities when the level increases.
		const float springConstant = shouldCutDuckingBehaviour ? 0.0f : settings.m_SpringConstant;
		m_DuckUnderOverheadCollisionSpring.Update(m_DuckLevelForMostRecentOverheadCollision, springConstant);
	}
	else
	{
		m_DuckUnderOverheadCollisionSpring.Reset();
		m_CollisionRootPositionForMostRecentOverheadCollision.Set(LARGE_FLOAT);
		m_DuckLevelForMostRecentOverheadCollision = 0.0f;
	}

	PF_SET(followVehicleCurrentDuckLevel, duckLevel);
	PF_SET(followVehicleCachedDuckLevel, m_DuckLevelForMostRecentOverheadCollision);
	PF_SET(followVehicleBlendedDuckLevel, m_DuckUnderOverheadCollisionBlendLevel * m_DuckUnderOverheadCollisionSpring.GetResult());
}

float camFollowVehicleCamera::ComputeDuckUnderOverheadCollisionLevel(const Vector3& collisionRootPositionOnPreviousUpdate,
	float preCollisionOrbitDistance, float collisionTestRadius) const
{
	const camFollowVehicleCameraMetadataDuckUnderOverheadCollisionSettings& settings = m_Metadata.m_DuckUnderOverheadCollisionSettings;
	if(!settings.m_ShouldDuck)
	{
		return 0.0f;
	}

	//NOTE: We must call the base follow camera implementation of ComputeOrbitPitchOffset, as we wish to ignore any scaling applied when ducking. 
	const float baseOrbitPitchOffset = camFollowCamera::ComputeOrbitPitchOffset();
	if(baseOrbitPitchOffset > -SMALL_FLOAT)
	{
		return 0.0f;
	}

	//Perform a custom swept sphere test upwards to detect overhead collision.
	const float capsuleLength = settings.m_CapsuleSettings.m_LengthScaling * preCollisionOrbitDistance * Sinf(-baseOrbitPitchOffset);
	if(capsuleLength < SMALL_FLOAT)
	{
		return 0.0f;
	}

	if(!cameraVerifyf(collisionTestRadius >= SMALL_FLOAT,
		"Unable to compute a valid duck under collision fall back position due to an invalid test radius."))
	{
		return 0.0f;
	}

	WorldProbe::CShapeTestCapsuleDesc capsuleTest;
	WorldProbe::CShapeTestHitPoint capsuleIsect;
	WorldProbe::CShapeTestResults shapeTestResults(capsuleIsect);
	capsuleTest.SetResultsStructure(&shapeTestResults);
	capsuleTest.SetIncludeFlags(camCollision::GetCollisionIncludeFlags());
	capsuleTest.SetIsDirected(true);
	capsuleTest.SetTypeFlags(ArchetypeFlags::GTA_CAMERA_TEST);
	capsuleTest.SetOptions(WorldProbe::LOS_IGNORE_NO_CAM_COLLISION);
	capsuleTest.SetContext(WorldProbe::LOS_Camera);

	//NOTE: m_Collision has already been validated.
	const s32 numEntitiesToIgnore = m_Collision->GetNumEntitiesToIgnoreThisUpdate();
	if(numEntitiesToIgnore > 0)
	{
		const CEntity** entitiesToIgnore = const_cast<const CEntity**>(m_Collision->GetEntitiesToIgnoreThisUpdate());
		capsuleTest.SetExcludeEntities(entitiesToIgnore, numEntitiesToIgnore, WorldProbe::EIEO_DONT_ADD_VEHICLE_OCCUPANTS);
	}

	Vector3 capsuleDelta;
	capsuleDelta.SetScaled(ZAXIS, capsuleLength);

	Vector3 collisionRootToPreviousDelta;
	collisionRootToPreviousDelta.Subtract(collisionRootPositionOnPreviousUpdate, m_CollisionRootPosition);

	Vector3 collisionRootToPreviousDirection(collisionRootToPreviousDelta);
	collisionRootToPreviousDirection.NormalizeSafe(YAXIS);

	const float collisionRootToPreviousDistance = collisionRootToPreviousDelta.Mag();

	float distanceFromCollisionRootOnPreviousTest = 0.0f;
	float duckLevel = 0.0f;

	for(u32 i=0; i<settings.m_CapsuleSettings.m_NumTests; i++)
	{
		const float collisionRootBlendValue		= static_cast<float>(i) / static_cast<float>(settings.m_CapsuleSettings.m_NumTests);
		const float distanceFromCollisionRoot	= collisionRootBlendValue * collisionRootToPreviousDistance;

		//Ensure we have a valid offset from the current collision root position.
		if(i > 0)
		{
			if(distanceFromCollisionRoot > settings.m_CapsuleSettings.m_OffsetLimits.y)
			{
				break;
			}

			if((distanceFromCollisionRoot - distanceFromCollisionRootOnPreviousTest) < settings.m_CapsuleSettings.m_OffsetLimits.x)
			{
				continue;
			}
		}

		Vector3 capsuleStartPosition;
		capsuleStartPosition.AddScaled(m_CollisionRootPosition, collisionRootToPreviousDirection, distanceFromCollisionRoot);

		Vector3 capsuleEndPosition;
		capsuleEndPosition.Add(capsuleStartPosition, capsuleDelta);

		capsuleTest.SetCapsule(capsuleStartPosition, capsuleEndPosition, collisionTestRadius);

		const bool hasHit = WorldProbe::GetShapeTestManager()->SubmitTest(capsuleTest);
		if(hasHit)
		{
			const float localDuckLevel	= 1.0f - shapeTestResults[0].GetHitTValue();
			duckLevel					= Max(duckLevel, localDuckLevel);
		}

		distanceFromCollisionRootOnPreviousTest = distanceFromCollisionRoot;
	}

	return duckLevel;
}

void camFollowVehicleCamera::ComputeOrbitDistanceLimitsForBasePosition( Vector2& orbitDistanceLimitsForBasePosition ) const
{
	camFollowCamera::ComputeOrbitDistanceLimitsForBasePosition(orbitDistanceLimitsForBasePosition);

	orbitDistanceLimitsForBasePosition = Lerp(m_VerticalFlightModeBlendLevel, orbitDistanceLimitsForBasePosition, g_VFMOrbitDistanceLimitsForBasePosition);
}

void camFollowVehicleCamera::ComputeLookBehindPullAroundSettings(camFollowCameraMetadataPullAroundSettings& settings) const
{
	camFollowCamera::ComputeLookBehindPullAroundSettings(settings);

	if (m_VerticalFlightModeBlendLevel > SMALL_FLOAT)
	{
		settings.m_ShouldBlendOutWhenAttachParentIsOnGround					= g_VFMShouldBlendOutWhenAttachParentIsOnGround;
		settings.m_ShouldBlendWithAttachParentMatrixForRelativeOrbitBlend	= g_VFMShouldBlendWithAttachParentMatrixForRelativeOrbitBlend;
	}
	settings.m_HeadingPullAroundMaxMoveSpeed	= Lerp(m_VerticalFlightModeBlendLevel, settings.m_HeadingPullAroundMaxMoveSpeed, g_VFMHeadingPullAroundMaxMoveSpeed);
	settings.m_HeadingPullAroundSpringConstant	= Lerp(m_VerticalFlightModeBlendLevel, settings.m_HeadingPullAroundSpringConstant,g_VFMHeadingPullAroundSpringConstant);
	settings.m_PitchPullAroundMaxMoveSpeed		= Lerp(m_VerticalFlightModeBlendLevel, settings.m_PitchPullAroundMaxMoveSpeed, g_VFMPitchPullAroundMaxMoveSpeed);
	settings.m_PitchPullAroundSpringConstant	= Lerp(m_VerticalFlightModeBlendLevel, settings.m_PitchPullAroundSpringConstant, g_VFMPitchPullAroundSpringConstant);
}

void camFollowVehicleCamera::ComputeBasePullAroundSettings(camFollowCameraMetadataPullAroundSettings& settings) const
{
	camFollowCamera::ComputeBasePullAroundSettings(settings);

	if (m_VerticalFlightModeBlendLevel > SMALL_FLOAT)
	{
		settings.m_ShouldBlendOutWhenAttachParentIsOnGround					= g_VFMShouldBlendOutWhenAttachParentIsOnGround;
		settings.m_ShouldBlendWithAttachParentMatrixForRelativeOrbitBlend	= g_VFMShouldBlendWithAttachParentMatrixForRelativeOrbitBlend;
	}
	settings.m_HeadingPullAroundMaxMoveSpeed	= Lerp(m_VerticalFlightModeBlendLevel, settings.m_HeadingPullAroundMaxMoveSpeed, g_VFMHeadingPullAroundMaxMoveSpeed);
	settings.m_HeadingPullAroundSpringConstant	= Lerp(m_VerticalFlightModeBlendLevel, settings.m_HeadingPullAroundSpringConstant,g_VFMHeadingPullAroundSpringConstant);
	settings.m_PitchPullAroundMaxMoveSpeed		= Lerp(m_VerticalFlightModeBlendLevel, settings.m_PitchPullAroundMaxMoveSpeed, g_VFMPitchPullAroundMaxMoveSpeed);
	settings.m_PitchPullAroundSpringConstant	= Lerp(m_VerticalFlightModeBlendLevel, settings.m_PitchPullAroundSpringConstant, g_VFMPitchPullAroundSpringConstant);
}

bool camFollowVehicleCamera::ComputeShouldPullAroundToBasicAttachParentMatrix() const
{
	bool shouldPullAroundToBasicAttachParentMatrix = camFollowCamera::ComputeShouldPullAroundToBasicAttachParentMatrix();

	if (m_VerticalFlightModeBlendLevel > SMALL_FLOAT)
	{
		shouldPullAroundToBasicAttachParentMatrix = g_VFMShouldPullAroundToBasicAttachParentMatrix;
	}

#if KEYBOARD_MOUSE_SUPPORT
	if (m_AttachParent && m_AttachParent->GetIsTypeVehicle())
	{
		const CVehicle* attachVehicle = static_cast<const CVehicle*>(m_AttachParent.Get());
		if(attachVehicle->InheritsFromSubmarine())
		{
			CControl* control = camInterface::GetGameplayDirector().GetActiveControl();
			if( control && control->WasKeyboardMouseLastKnownSource() &&
				CPauseMenu::GetMenuPreference(PREF_MOUSE_SUB) != FALSE )
			{
				shouldPullAroundToBasicAttachParentMatrix = true;
			}
		}
	}
#endif

	return shouldPullAroundToBasicAttachParentMatrix;
}

float camFollowVehicleCamera::ComputeDesiredFov()
{
	float desiredFov = camFollowCamera::ComputeDesiredFov();
	
	if(ShouldBlockHighSpeedEffects())
	{
		return desiredFov;
	}

	//Attempt to scale the desired FOV upwards when the attach vehicle is traveling at a high speed.
	//NOTE: This scaling is applied w.r.t. the base FOV, so this effect is not multiplicative with other FOV scaling effects.
	//NOTE: The FOV cannot be reduced as a result of this effect.

	const camFollowVehicleCameraMetadataHighSpeedZoomSettings& settings = m_Metadata.m_HighSpeedZoomSettings;

	//Consider the forward speed only.
	const CVehicle* attachVehicle	= static_cast<const CVehicle*>(m_AttachParent.Get());
	const Vector3 vehicleFront		= VEC3V_TO_VECTOR3(m_AttachParent->GetTransform().GetB());
	const float forwardSpeed		= DotProduct(m_BaseAttachVelocityToConsider, vehicleFront);
	float desiredSpeedFactor		= SlowInOut(RampValueSafe(forwardSpeed, settings.m_MinForwardSpeed, settings.m_MaxForwardSpeed, 0.0f, 1.0f));
	const bool IsOwnedByCutscene	= attachVehicle->GetOwnedBy() == ENTITY_OWNEDBY_CUTSCENE; 

	if(IsOwnedByCutscene)
	{
		m_HighSpeedZoomCutsceneBlendSpring.Reset(1.0f);
	}
	else
	{
		m_HighSpeedZoomCutsceneBlendSpring.Update(0.0f, settings.m_CutsceneBlendSpringConstant, settings.m_CutsceneBlendSpringDampingRatio); 
		
	}
	
	float cutsceneBlendLevel = (1.0f - m_HighSpeedZoomCutsceneBlendSpring.GetResult()); 
	cutsceneBlendLevel = Clamp(cutsceneBlendLevel, 0.0f, 1.0f); 

	desiredSpeedFactor *= cutsceneBlendLevel;

	if(m_NumUpdatesPerformed == 0)
	{
		m_HighSpeedZoomSpring.Reset(desiredSpeedFactor);
	}
	else
	{
		m_HighSpeedZoomSpring.Update(desiredSpeedFactor, settings.m_SpringConstant);
	}

	const float maxFovToApply = m_Metadata.m_BaseFov * settings.m_MaxBaseFovScaling;

	if(maxFovToApply >= (desiredFov + SMALL_FLOAT))
	{
		const float speedFactorToApply	= Clamp(m_HighSpeedZoomSpring.GetResult(), 0.0f, 1.0f); //For safety.
		desiredFov						= Lerp(speedFactorToApply, desiredFov, maxFovToApply);
	}

	return desiredFov;
}

float camFollowVehicleCamera::ComputeMotionBlurStrength()
{
	return camThirdPersonCamera::ComputeMotionBlurStrength() + ComputeMotionBlurStrengthForImpulse();
}

float camFollowVehicleCamera::ComputeMotionBlurStrengthForImpulse()
{
	const camMotionBlurSettingsMetadata* motionBlurSettings = camFactory::FindObjectMetadata<camMotionBlurSettingsMetadata>(m_Metadata.m_MotionBlurSettings.GetHash());
	if(!motionBlurSettings)
	{
		return 0.0f;
	}

	if(motionBlurSettings->m_ImpulseMotionBlurMaxStrength < SMALL_FLOAT)
	{
		return 0.0f;
	}

	float motionBlurStrength = 0.0f;

	CVehicle* attachParentPhysical			= (CVehicle*)m_AttachParent.Get();

	float impulse = attachParentPhysical->GetFrameCollisionHistory()->GetCollisionImpulseMagSum();
	impulse *= InvertSafe(attachParentPhysical->pHandling->m_fMass, attachParentPhysical->GetInvMass());						

	m_MotionBlurPreviousAttachParentImpuse	= impulse;

	u32 time								= fwTimer::GetCamTimeInMilliseconds();

	if(impulse > m_MotionBlurPeakAttachParentImpulse)
	{
		m_MotionBlurForAttachParentImpulseStartTime	= (float)time;
		m_MotionBlurPeakAttachParentImpulse			= impulse;
	}

	if(m_MotionBlurPeakAttachParentImpulse > 0.0f)
	{
		u32 timeSinceStart = (u32)(time - m_MotionBlurForAttachParentImpulseStartTime);
		if((timeSinceStart < motionBlurSettings->m_ImpulseMotionBlurDuration))
		{
			motionBlurStrength	= RampValueSafe(m_MotionBlurPeakAttachParentImpulse, motionBlurSettings->m_ImpulseMotionBlurMinImpulse,
				motionBlurSettings->m_ImpulseMotionBlurMaxImpulse, 0.0f, motionBlurSettings->m_ImpulseMotionBlurMaxStrength);
		}
		else
		{
			m_MotionBlurPeakAttachParentImpulse = 0.0f;
		}
	}

	return motionBlurStrength;
}

void camFollowVehicleCamera::UpdateParachuteBehavior()
{
	if(!m_AttachParent || camInterface::GetGameplayDirector().IsScriptRequestingVehicleCamStuntSettingsThisUpdate() || m_FrameInterpolator != nullptr)
	{
		return;
	}

	const CVehicle* attachParentVehicle = (const CVehicle*)m_AttachParent.Get();
	if(attachParentVehicle->InheritsFromAutomobile())
	{
		const CAutomobile* pAutomobile = (const CAutomobile*) attachParentVehicle;
		if(m_NumUpdatesPerformed == 0 && m_FrameInterpolator && pAutomobile->IsParachuteDeployed() && m_Metadata.m_DeployParachuteShakeRef.IsNotNull())
		{
			Shake(m_Metadata.m_DeployParachuteShakeRef, 1.0f);
		}
	}
}

bool camFollowVehicleCamera::ShouldBlockHighSpeedEffects() const
{
	//We do not apply high-speed effects for network clone vehicles or when playing back a vehicle recording, as the vehicle can report quite
	//inaccurate/discontinuous position and velocity, which can result in undesirable camera effects.
	//NOTE: m_AttachParent && m_AttachParent->GetIsTypeVehicle() have already been validated.
	const CVehicle* attachVehicle	= static_cast<const CVehicle*>(m_AttachParent.Get());
	const bool isPlayingRecording	= CVehicleRecordingMgr::IsPlaybackGoingOnForCar(attachVehicle);

	return isPlayingRecording;
}

void camFollowVehicleCamera::InitTunables()
{
	ms_ConsiderMetadataFlagForIgnoringBikeCollision = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("FOLLOW_VEHICLE_CAMERA_CONSIDER_METADATA_FLAG_FOR_IGNORING_BIKE_COLLISION", 0x7a568360), true);
}

#if __BANK
void camFollowVehicleCamera::AddWidgets(bkBank &bank)
{
	bank.PushGroup("Follow-vehicle camera", false);
	{
		bank.AddToggle("Should follow-vehicle camera collide?", &ms_ShouldCollide);
	}
	bank.PopGroup();
}
#endif // __BANK
