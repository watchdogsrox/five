//
// camera/scripted/ScriptedFlyCamera.cpp
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#include "camera/scripted/ScriptedFlyCamera.h"

#include "math/vecmath.h"

#include "fwscene/world/WorldLimits.h"

#include "camera/CamInterface.h"
#include "camera/gameplay/GameplayDirector.h"
#include "camera/helpers/Collision.h"
#include "camera/helpers/ControlHelper.h"
#include "physics/WorldProbe/worldprobe.h"
#include "renderer/Water.h"
#include "scene/world/GameWorldHeightMap.h"
#include "system/control.h"

CAMERA_OPTIMISATIONS()

INSTANTIATE_RTTI_CLASS(camScriptedFlyCamera,0x664D5623)


camScriptedFlyCamera::camScriptedFlyCamera(const camBaseObjectMetadata& metadata)
: camScriptedCamera(metadata)
, m_LocalMetadata(static_cast<const camScriptedFlyCameraMetadata&>(metadata))
, m_DesiredTranslation(VEC3_ZERO)
, m_ScriptPosition(VEC3_ZERO)
, m_CachedNormalisedHorizontalTranslationInput(0.0f, 0.0f)
, m_CachedVerticalTranslationInputSign(0.0f)
, m_HorizontalSpeed(0.0f)
, m_VerticalSpeed(0.0f)
, m_PushingAgainstWaterSurfaceTimer(0)
, m_WasConstrained(false)
, m_ScriptPositionValid(false)
, m_EnableVerticalControl(false)
, m_IsUnderWater(false)
{
	m_ShouldControlMiniMapHeading = true;

	//Apply default rotation.
	m_Frame.SetWorldMatrixFromHeadingPitchAndRoll(0.0f, m_LocalMetadata.m_DefaultPitch * DtoR, 0.0f);
}

void camScriptedFlyCamera::PreUpdate()
{
	camScriptedCamera::PreUpdate();

	const bool hasCut = m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutPosition);
	if(hasCut)
	{
		//The script has cut the camera to a new position, so we must ensure that the world-z is a safe distance above the world height map.
		Vector3 position			= m_Frame.GetPosition();
		const float maxWorldHeight	= CGameWorldHeightMap::GetMaxHeightFromWorldHeightMap(
										position.x - m_LocalMetadata.m_HalfBoxExtentForHeightMapQuery,
										position.y - m_LocalMetadata.m_HalfBoxExtentForHeightMapQuery,
										position.x + m_LocalMetadata.m_HalfBoxExtentForHeightMapQuery,
										position.y + m_LocalMetadata.m_HalfBoxExtentForHeightMapQuery);
		const float minSafeHeight	= maxWorldHeight + m_LocalMetadata.m_MinDistanceAboveHeightMapToCut;
		position.z					= Max(position.z, minSafeHeight);

		m_Frame.SetPosition(position);

        //a cut has been flagged, means that the camera was warped in this frame, refresh the underwater flag
        float waterHeight			= 0.0f;
        const bool hasFoundWater	= Water::GetWaterLevel(position, &waterHeight, true, LARGE_FLOAT, LARGE_FLOAT, NULL);
        if(hasFoundWater)
        {
            if(position.z > (waterHeight - m_LocalMetadata.m_MinHeightAboveWater - SMALL_FLOAT))
            {
                m_IsUnderWater = false;
            }
            else if(position.z < (waterHeight + m_LocalMetadata.m_MinHeightAboveWater - SMALL_FLOAT))
            {
                m_IsUnderWater = true;
            }

            //else, the camera is now very close to the surface we leave it undefined and let the UpdateWaterCollision handle it.
        }

		//Also reset the translation control speeds on a cut.
		m_HorizontalSpeed	= 0.0f;
		m_VerticalSpeed		= 0.0f;
	}
}

bool camScriptedFlyCamera::Update()
{
	camScriptedCamera::Update();

	m_WasConstrained = false;

	if(m_AttachParent)
	{
		return true;
	}

	UpdateControl();

	UpdatePosition();

	// Reset per-frame flags.
	m_EnableVerticalControl = false;

	return true;
}

void camScriptedFlyCamera::UpdateControl()
{
	const CControl* control = camInterface::GetGameplayDirector().GetActiveControl();
	if(!control)
	{
		return;
	}

	UpdateTranslationControl(*control);
}

void camScriptedFlyCamera::UpdateTranslationControl(const CControl& control)
{
	Vector3 translationInput;
	ComputeTranslationInput(control, translationInput);

	const float horizontalInputMag = translationInput.XYMag();
	if(horizontalInputMag >= SMALL_FLOAT)
	{
		//Cache the normalised horizontal translation inputs, for use in deceleration.
		m_CachedNormalisedHorizontalTranslationInput.InvScale(Vector2(translationInput, Vector2::kXY), horizontalInputMag);
	}

	const float verticalInputMag = Abs(translationInput.z);
	if(verticalInputMag >= SMALL_FLOAT)
	{
		//Cache the sign of the vertical translation input, for use in deceleration.
		m_CachedVerticalTranslationInputSign = Sign(translationInput.z);
	}

	const camScriptedFlyCameraMetadataInputResponse& horizontalInputResponse	= m_LocalMetadata.m_HorizontalTranslationInputResponse;
	const camScriptedFlyCameraMetadataInputResponse& verticalInputResponse		= m_LocalMetadata.m_VerticalTranslationInputResponse;

	//Apply the custom power factor scaling specified in metadata to change the control response across the range of stick input.
	const float scaledHorizontalInputMag	= rage::Powf(Min(horizontalInputMag, 1.0f), horizontalInputResponse.m_InputMagPowerFactor);
	const float scaledVerticalInputMag		= rage::Powf(Min(verticalInputMag, 1.0f), verticalInputResponse.m_InputMagPowerFactor);

	const float timeStep = fwTimer::GetCamTimeStep();

	const float horizontalOffset	= camControlHelper::ComputeOffsetBasedOnAcceleratedInput(scaledHorizontalInputMag,
										horizontalInputResponse.m_MaxAcceleration, horizontalInputResponse.m_MaxDeceleration,
										horizontalInputResponse.m_MaxSpeed, m_HorizontalSpeed, timeStep, false);
	const float verticalOffset		= camControlHelper::ComputeOffsetBasedOnAcceleratedInput(scaledVerticalInputMag,
										verticalInputResponse.m_MaxAcceleration, verticalInputResponse.m_MaxDeceleration,
										verticalInputResponse.m_MaxSpeed, m_VerticalSpeed, timeStep, false);

	Vector2 horizontalTranslation;
	horizontalTranslation.Scale(m_CachedNormalisedHorizontalTranslationInput, horizontalOffset);

	const float verticalTranslation = m_CachedVerticalTranslationInputSign * verticalOffset;

	if (!m_EnableVerticalControl)
	{
		m_DesiredTranslation.Set(horizontalTranslation.x, horizontalTranslation.y, 0.0f);
	}
	else
	{
		m_DesiredTranslation.Set(horizontalTranslation.x, horizontalTranslation.y, verticalTranslation);
	}

	//Apply the XY translation relative to the camera heading, but ignore the pitch.
	const float heading = m_Frame.ComputeHeading();
	m_DesiredTranslation.RotateZ(heading);

	if(!m_EnableVerticalControl)
	{
		//Apply the old 'vertical' translation relative to the camera front, as we want the camera to zoom in and out without moving the look-at position.
		const Vector3& cameraFront	= m_Frame.GetFront();
		m_DesiredTranslation		+= verticalTranslation * cameraFront;
	}
}

void camScriptedFlyCamera::ComputeTranslationInput(const CControl& control, Vector3& translationInput) const
{
	if(IsNearZero(m_LocalMetadata.m_HorizontalTranslationInputResponse.m_MaxSpeed))
	{
		translationInput.x = translationInput.y = 0.0f;
	}
	else
	{
		translationInput.x					= control.GetScriptedFlyLeftRight().GetNorm();
		translationInput.y					= -control.GetScriptedFlyUpDown().GetNorm();
	}

	if(IsNearZero(m_LocalMetadata.m_VerticalTranslationInputResponse.m_MaxSpeed))
	{
		translationInput.z = 0.0f;
	}
	else
	{
		const float translateUpInputValue	= control.GetScriptedFlyVertUp().GetNorm01();
		const float translateDownInputValue	= control.GetScriptedFlyVertDown().GetNorm01();

		translationInput.z = translateUpInputValue - translateDownInputValue;
		if (!m_EnableVerticalControl)
		{
			translationInput.z *= -1.0f;
		}
	}
}

void camScriptedFlyCamera::UpdatePosition()
{
	Vector3 initialPosition(m_Frame.GetPosition());
	Vector3 positionToApply(initialPosition);

	if (m_ScriptPositionValid)
	{
		// Script can override the camera position, which has to be done each frame as we reset it on each update.
		positionToApply = m_ScriptPosition;
		m_ScriptPositionValid = false;
	}

	positionToApply += m_DesiredTranslation;

    // Removing the world limits clamp as it is not representative of the size of the map and is causing issues when the replay is recorded near the edges [8/10/2015 carlo.mangani]

	//First constrain the camera to the world limits.
	//const Vector3& worldMin = g_WorldLimits_MapDataExtentsAABB.GetMinVector3();
	//Vector3 worldMax = g_WorldLimits_MapDataExtentsAABB.GetMaxVector3();

	////limit to 10m below because the player is placed at the camera origin, causing him to be out of the world bounds
	//worldMax.z = g_WorldLimits_AABB.GetMaxVector3().z - 10.0f; 

	//positionToApply.Min(positionToApply, worldMax);
	//positionToApply.Max(positionToApply, worldMin);

	positionToApply.z = Min(positionToApply.z, m_LocalMetadata.m_MaxHeight);

	//update the water collision, don't set constraint so we don't shake!
	UpdateWaterCollision(positionToApply);

	//Constrain against world collision.
	const bool wasConstrainedAgainstCollision	= UpdateCollision(initialPosition, positionToApply);
	m_WasConstrained							|= wasConstrainedAgainstCollision;

	m_Frame.SetPosition(positionToApply);

	//Zero the cached speeds when we constrain against water or collision.
	if(m_WasConstrained)
	{
		m_HorizontalSpeed = m_VerticalSpeed = 0.0f;
	}

	//Force the game to focus on this camera, so that map streams around its location.
	m_Frame.GetFlags().SetFlag(camFrame::Flag_ShouldOverrideStreamingFocus);
}

bool camScriptedFlyCamera::UpdateWaterCollision(Vector3& positionToTest)
{
	bool isConstrained = false;

	//Vertically constrain against simulated water.
	//NOTE: River bounds are covered by the collision shapetests below.
	float waterHeight			= 0.0f;
	const bool hasFoundWater	= Water::GetWaterLevel(positionToTest, &waterHeight, true, LARGE_FLOAT, LARGE_FLOAT, NULL);
	if(hasFoundWater)
	{
		if(m_IsUnderWater)
		{
			if(positionToTest.z > (waterHeight - m_LocalMetadata.m_MinHeightAboveWater - SMALL_FLOAT))
			{
				positionToTest.z = waterHeight - m_LocalMetadata.m_MinHeightAboveWater;
				isConstrained = true;
			}
		}
		else
		{
			if(positionToTest.z < (waterHeight + m_LocalMetadata.m_MinHeightAboveWater - SMALL_FLOAT))
			{
				positionToTest.z = waterHeight + m_LocalMetadata.m_MinHeightAboveWater;
				isConstrained = true;
			}
		}
	}

	if(isConstrained)
	{
		m_PushingAgainstWaterSurfaceTimer += fwTimer::GetCamTimeStepInMilliseconds();
		if(m_PushingAgainstWaterSurfaceTimer >= m_LocalMetadata.m_TimeToChangeWaterSurfaceState)
		{
			m_IsUnderWater = !m_IsUnderWater;
			m_PushingAgainstWaterSurfaceTimer = 0;

			positionToTest.z = waterHeight + (m_IsUnderWater ? -m_LocalMetadata.m_MinHeightAboveWater : m_LocalMetadata.m_MinHeightAboveWater);

			isConstrained = false;
			m_Frame.GetFlags().SetFlag(camFrame::Flag_HasCutPosition);
		}
	}
	else
	{
		m_PushingAgainstWaterSurfaceTimer = 0;
	}

	return isConstrained;
}

bool camScriptedFlyCamera::UpdateCollision(const Vector3& initialPosition, Vector3& cameraPosition) const
{
	Vector3 cameraPositionBeforeCollision(cameraPosition);

	//Default to persisting the initial position to maintain a safe distance from collision.
	cameraPosition = initialPosition;

	const Vector3 desiredTranslation	= cameraPositionBeforeCollision - initialPosition;
	const float desiredTranslationMag2	= desiredTranslation.Mag2();
	if(desiredTranslationMag2 < VERY_SMALL_FLOAT)
	{
		return false;
	}

	const u32 includeFlags = ArchetypeFlags::GTA_MAP_TYPE_MOVER | ArchetypeFlags::GTA_GLASS_TYPE | ArchetypeFlags::GTA_RIVER_TYPE;

	//Sweep a sphere to the desired camera position.
	WorldProbe::CShapeTestCapsuleDesc capsuleTest;
	WorldProbe::CShapeTestFixedResults<> shapeTestResults;
	capsuleTest.SetResultsStructure(&shapeTestResults);
	capsuleTest.SetIncludeFlags(includeFlags);
	capsuleTest.SetTypeFlags(ArchetypeFlags::GTA_CAMERA_TEST);
	capsuleTest.SetContext(WorldProbe::LOS_Camera);
	capsuleTest.SetIsDirected(true);
	capsuleTest.SetCapsule(initialPosition, cameraPositionBeforeCollision, m_LocalMetadata.m_CapsuleRadius);

	bool hasHit = WorldProbe::GetShapeTestManager()->SubmitTest(capsuleTest);
	const float hitTValue = ComputeFilteredHitTValue(shapeTestResults, false);
	hasHit &= hitTValue < 1.0f;

	cameraPosition.Lerp(hitTValue, initialPosition, cameraPositionBeforeCollision);

	bool wasConstrained = hasHit;

	//Now perform a sphere test at the prospective camera position, with a radius mid-way between that of the swept sphere and the non-directed
	//capsule (below). This ensures that the camera cannot be placed so close to collision that the non-directed test could always hit and leave
	//the camera permanently stuck to the collision surface.

	const float nonDirectedCapsuleRadius	= m_LocalMetadata.m_CapsuleRadius - camCollision::GetMinRadiusReductionBetweenInterativeShapetests() * 2.0f;
	const float sphereRadius				= 0.5f * (m_LocalMetadata.m_CapsuleRadius + nonDirectedCapsuleRadius);

	WorldProbe::CShapeTestSphereDesc sphereTest;
	shapeTestResults.Reset();
	sphereTest.SetResultsStructure(&shapeTestResults);
	sphereTest.SetIncludeFlags(includeFlags);
	sphereTest.SetTypeFlags(ArchetypeFlags::GTA_CAMERA_TEST);
	sphereTest.SetContext(WorldProbe::LOS_Camera);
	sphereTest.SetSphere(cameraPosition, sphereRadius);

	hasHit = WorldProbe::GetShapeTestManager()->SubmitTest(sphereTest);
	hasHit &= ComputeFilteredHitTValue(shapeTestResults, true) < 1.0f;
	if(hasHit)
	{
		//The prospective camera position is too close to collision, so revert to persisting the initial position.
		cameraPosition = initialPosition;
		return true;
	}

	//Finally performed a slightly smaller non-directed capsule test from the initial position to the prospective camera position to apply.
	//This ensures that this path is completely clear and avoids problems associated with the initial position of the swept sphere test.
	shapeTestResults.Reset();
	capsuleTest.SetResultsStructure(&shapeTestResults);
	capsuleTest.SetIsDirected(false);
	capsuleTest.SetCapsule(initialPosition, cameraPosition, nonDirectedCapsuleRadius);

	hasHit = WorldProbe::GetShapeTestManager()->SubmitTest(capsuleTest);
	hasHit &= ComputeFilteredHitTValue(shapeTestResults, false) < 1.0f;
	if(hasHit)
	{
		//The path is not clear, so revert to persisting the initial position.
		cameraPosition	= initialPosition;
		wasConstrained	= true;
	}

	return wasConstrained;
}

float camScriptedFlyCamera::ComputeFilteredHitTValue(const WorldProbe::CShapeTestFixedResults<>& shapeTestResults, bool isSphereTest) const
{
	//special case for the stunt ramp surface, we don't want the camera to collide with the stunt pipes as it will affect the editor usability.
	const u32 numberOfHits = shapeTestResults.GetNumHits();

	float hitTValue = 1.0f;
	for(int index = 0; index < numberOfHits; ++index)
	{
		const WorldProbe::CShapeTestHitPoint& hitPoint = shapeTestResults[index];
		const phMaterialMgrGta::Id hitPointMaterial = hitPoint.GetHitMaterialId();
		const phMaterialMgrGta::Id unpackedMaterialId = PGTAMATERIALMGR->UnpackMtlId(hitPointMaterial);
		if(unpackedMaterialId != PGTAMATERIALMGR->g_idStuntRampSurface &&
			unpackedMaterialId != PGTAMATERIALMGR->g_idStuntTargetA &&
			unpackedMaterialId != PGTAMATERIALMGR->g_idStuntTargetB &&
			unpackedMaterialId != PGTAMATERIALMGR->g_idStuntTargetC)
		{
			hitTValue = Min(hitTValue, isSphereTest ? 0.0f : hitPoint.GetHitTValue());
		}
	}
	return hitTValue;
}