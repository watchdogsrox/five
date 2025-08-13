//
// camera/helpers/HintHelper.cpp
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#include "fwmaths/angle.h"

#include "camera/CamInterface.h"
#include "camera/gameplay/GameplayDirector.h"
#include "camera/helpers/HintHelper.h"
#include "camera/helpers/Frame.h"
#include "camera/helpers/LookAtDampingHelper.h"
#include "camera/system/CameraMetadata.h"
#include "camera/system/CameraFactory.h"
#include "scene/entity.h"

CAMERA_OPTIMISATIONS()

INSTANTIATE_RTTI_CLASS(camHintHelper,0xCEBE5A3A)

camHintHelper::camHintHelper(const camBaseObjectMetadata& metadata)
: camBaseObject(metadata)
, m_Metadata(static_cast<const camHintHelperMetadata&>(metadata))
, m_LookAtDampingHelper(NULL)
, m_HintBlendLevelForOrientationOnPreviousUpdate(0.0f)
, m_CachedOrbitHeading(0.0f)
, m_CachedOrbitPitch(0.0f)
, m_HaveIntialisedHintHelper(false)
, m_HaveTerminatedForPitchLimits(false)
{
	u32 helperHash = m_Metadata.m_LookAtVehicleDampingHelperRef.GetHash(); 

	if(camInterface::GetGameplayDirector().IsHintActive())
	{
		if(camInterface::GetGameplayDirector().GetHintEntity())
		{
			if(camInterface::GetGameplayDirector().GetHintEntity()->GetIsTypePed())
			{	
				helperHash = m_Metadata.m_LookAtPedDampingHelperRef.GetHash(); 
			}
		}	
	}

	const camLookAtDampingHelperMetadata* pLookAtHelperMetaData = camFactory::FindObjectMetadata<camLookAtDampingHelperMetadata>(helperHash);

	if(pLookAtHelperMetaData)
	{
		m_LookAtDampingHelper = camFactory::CreateObject<camLookAtDampingHelper>(*pLookAtHelperMetaData);
		cameraAssertf(m_LookAtDampingHelper, "Hint Helper (name: %s, hash: %u) cannot create a look at damping helper (name: %s, hash: %u)",
			GetName(), GetNameHash(), SAFE_CSTRING(pLookAtHelperMetaData->m_Name.GetCStr()), pLookAtHelperMetaData->m_Name.GetHash());	
	}
}

camHintHelper::~camHintHelper()
{
	if(m_LookAtDampingHelper)
	{
		delete m_LookAtDampingHelper; 
	}

	m_LookAtDampingHelper = NULL; 
}


void camHintHelper::ComputeHintOrientation(const Vector3& basePivotPosition, const Vector2& orbitPitchLimits, float lookAroundInputEnvelopeLevel,
	float& orbitHeading, float& orbitPitch, float Fov, float orbitPitchOffset, const Vector3& orbitRelativePivotOffset)
{
	if(!camInterface::GetGameplayDirector().IsHintActive())
	{
		return;
	}

	const float hintBlendLevel = camInterface::GetGameplayDirector().GetHintBlend(); 
	if(hintBlendLevel < SMALL_FLOAT)
	{
		return;
	}

	const Vector3& hintTargetPosition	= camInterface::GetGameplayDirector().UpdateHintPosition();
	const Vector3 desiredOrbitDelta		= hintTargetPosition - basePivotPosition;
	const float distance2ToTarget		= desiredOrbitDelta.Mag2();
	
	if(distance2ToTarget < VERY_SMALL_FLOAT)
	{
		return;
	}
	
	float DesiredOrbitHeading; 
	float DesiredOrbitPitch; 

	if(m_HaveTerminatedForPitchLimits && m_Metadata.m_ShouldStopUpdatingLookAtPosition)
	{
		DesiredOrbitHeading = m_CachedOrbitHeading; 
		DesiredOrbitPitch = m_CachedOrbitPitch; 
	}
	else
	{
		//NOTE: We blend out the hint orbit pitch limits when looking around.
		const float blendLevelToConsider = hintBlendLevel * (1.0f - lookAroundInputEnvelopeLevel);

		ComputeDesiredHeadingAndPitch(DesiredOrbitHeading, DesiredOrbitPitch, Fov, desiredOrbitDelta, blendLevelToConsider, orbitPitchLimits, orbitPitchOffset,orbitRelativePivotOffset, basePivotPosition ); 

		m_CachedOrbitHeading = DesiredOrbitHeading; 
		m_CachedOrbitPitch = DesiredOrbitPitch; 
	}

	//If we are blending in the hint orientation based upon the overall hint blend, determine the blend level required this update to allow an appropriate blend in
	//response given that the orientation change is applied cumulatively. Simply blending in the desired hint orientation conventionally results in a skewed blend.
	float blendLevelToApply;
	if((hintBlendLevel >= m_HintBlendLevelForOrientationOnPreviousUpdate) && ((1.0f - m_HintBlendLevelForOrientationOnPreviousUpdate) >= SMALL_FLOAT))
	{
		blendLevelToApply = (hintBlendLevel - m_HintBlendLevelForOrientationOnPreviousUpdate) / (1.0f - m_HintBlendLevelForOrientationOnPreviousUpdate);
	}
	else
	{
		blendLevelToApply = hintBlendLevel;
	}

	m_HintBlendLevelForOrientationOnPreviousUpdate = hintBlendLevel;

	//NOTE: We blend out the hint orientation offset when looking around.
	blendLevelToApply *= (1.0f - lookAroundInputEnvelopeLevel);

	const float orbitHeadingToApply	= fwAngle::LerpTowards(orbitHeading, DesiredOrbitHeading, blendLevelToApply); 
	orbitHeading					= orbitHeadingToApply;

	const float orbitPitchToApply	= fwAngle::LerpTowards(orbitPitch, DesiredOrbitPitch, blendLevelToApply);
	orbitPitch						= orbitPitchToApply;
}


void camHintHelper::ComputeDesiredHeadingAndPitch(float &DesiredHeading, float &DesiredPitch, float Fov, const Vector3& desiredOrbitDelta, float blendLevel, 
												  const Vector2& orbitPitchLimits, float orbitPitchOffset, const Vector3& orbitRelativePivotOffset, const Vector3& basePivotPosition)
{
	Vector3 desiredOrbitFront;
	desiredOrbitFront.Normalize(desiredOrbitDelta);
	
	const Vector3 lockOnDelta	= camInterface::GetGameplayDirector().UpdateHintPosition() - basePivotPosition;
	const float lockOnDistance	= lockOnDelta.Mag();

	//Only update the lock-on orientation if there is sufficient distance between the base pivot position and the target positions,
	//otherwise the lock-on behavior can become unstable.
	if(lockOnDistance >= m_Metadata.m_MinDistanceForLockOn && m_Metadata.m_ShouldLockOn)
	{
		Matrix34 orbitMatrix(Matrix34::IdentityType);
		camFrame::ComputeWorldMatrixFromFront(desiredOrbitFront, orbitMatrix);

		const float sinHeadingDelta		= orbitRelativePivotOffset.x / lockOnDistance;
		const float headingDelta		= AsinfSafe(sinHeadingDelta);

		const float sinPitchDelta		= -orbitRelativePivotOffset.z / lockOnDistance;
		float pitchDelta				= AsinfSafe(sinPitchDelta);

		pitchDelta						-= orbitPitchOffset;

		orbitMatrix.RotateLocalZ(headingDelta);
		orbitMatrix.RotateLocalX(pitchDelta);

		desiredOrbitFront = orbitMatrix.b;
	}

	//add look at damping helper
	if(m_LookAtDampingHelper)
	{
		Matrix34 CamMat; 

		camFrame::ComputeWorldMatrixFromFront(desiredOrbitFront, CamMat); 

		m_LookAtDampingHelper->Update(CamMat, Fov, !m_HaveIntialisedHintHelper); 
		m_HaveIntialisedHintHelper = true; 

		desiredOrbitFront = CamMat.b; 
	}

	float desiredOrbitHeading;
	float desiredOrbitPitch;
	camFrame::ComputeHeadingAndPitchFromFront(desiredOrbitFront, desiredOrbitHeading, desiredOrbitPitch);

	Vector2 hintOrbitPitchLimits;
	hintOrbitPitchLimits.SetScaled(m_Metadata.m_OrbitPitchLimits, DtoR);

	if(m_Metadata.m_ShouldTerminateForPitch)
	{
		if( desiredOrbitPitch < hintOrbitPitchLimits.x ||  desiredOrbitPitch > hintOrbitPitchLimits.y)
		{
			m_HaveTerminatedForPitchLimits  = true;

			camInterface::GetGameplayDirector().StopHinting(false);

			DesiredPitch = Clamp(desiredOrbitPitch, hintOrbitPitchLimits.x, hintOrbitPitchLimits.y);
			DesiredHeading = desiredOrbitHeading; 

			return; 
		}
	}

	//Inhibit loosening of the regular pitch limits.
	hintOrbitPitchLimits.x = Max(hintOrbitPitchLimits.x, orbitPitchLimits.x);
	hintOrbitPitchLimits.y = Min(hintOrbitPitchLimits.y, orbitPitchLimits.y);

	Vector2 orbitPitchLimitsToApply;
	orbitPitchLimitsToApply.Lerp(blendLevel, orbitPitchLimits, hintOrbitPitchLimits);

	DesiredPitch = Clamp(desiredOrbitPitch, orbitPitchLimitsToApply.x, orbitPitchLimitsToApply.y);

	DesiredHeading = desiredOrbitHeading; 
}


// float camHintHelper::ComputeErrorRatioToApplyThisUpdate(float blendLevel, float blendLevelOnPreviousUpdate) const
// {
// 	float errorRatioToApplyThisUpdate;
// 
// 	if(blendLevel < SMALL_FLOAT)
// 	{
// 		errorRatioToApplyThisUpdate = 0.0f;
// 	}
// 	else if(blendLevel > (1.0f - SMALL_FLOAT))
// 	{
// 		errorRatioToApplyThisUpdate = 1.0f;
// 	}
// 	else if(blendLevel >= blendLevelOnPreviousUpdate)
// 	{
// 		errorRatioToApplyThisUpdate = (blendLevel - blendLevelOnPreviousUpdate) / (1.0f - blendLevelOnPreviousUpdate);
// 	}
// 	else
// 	{
// 		errorRatioToApplyThisUpdate = (blendLevelOnPreviousUpdate - blendLevel) / (blendLevelOnPreviousUpdate - (2.0f * blendLevel) + 1);
// 	}
// 
// 	return errorRatioToApplyThisUpdate; 
// }

void camHintHelper::ComputeOrbitPitchOffset(float& orbitPitchOffset) const
{
	if(camInterface::GetGameplayDirector().IsHintActive())
	{
		const float blendLevel				= camInterface::GetGameplayDirector().GetHintBlend();
		float baseOrbitPitchOffset = 0.0f; 

		if(camInterface::GetGameplayDirector().IsScriptOverridingScriptHintBaseOrbitPitchOffset())
		{
			baseOrbitPitchOffset = camInterface::GetGameplayDirector().GetScriptOverriddenHintBaseOrbitPitchOffset() * DtoR; 
		}
		else
		{
			baseOrbitPitchOffset = m_Metadata.m_BaseOrbitPitchOffset * DtoR;
		} 

		orbitPitchOffset = Lerp(blendLevel, orbitPitchOffset, baseOrbitPitchOffset);
	}
}

void camHintHelper::ComputeHintZoomScalar(float &desiredFov) const
{
	if(camInterface::GetGameplayDirector().IsHintActive() )	
	{	
		if(camInterface::GetGameplayDirector().IsScriptOverridingHintFov())
		{
			desiredFov = Lerp(camInterface::GetGameplayDirector().GetHintBlend(), desiredFov, camInterface::GetGameplayDirector().GetScriptOverriddenHintFov());
		}
		else
		{
			float FovScalar = 1.0f; 
			FovScalar = Lerp(camInterface::GetGameplayDirector().GetHintBlend(), 1.0f, m_Metadata.m_FovScalar);
			desiredFov /= FovScalar;
		}
	}
}

void camHintHelper::ComputeHintFollowDistanceScalar(float &DistanceScale) const
{
	DistanceScale = 1.0f; 

	if(camInterface::GetGameplayDirector().IsHintActive()  )	
	{	
		if(camInterface::GetGameplayDirector().IsScriptOverridingScriptHintDistanceScalar())
		{
			DistanceScale = Lerp(camInterface::GetGameplayDirector().GetHintBlend(), 1.0f, camInterface::GetGameplayDirector().GetScriptOverriddenHintDistanceScalar());
		}
		else
		{
			DistanceScale = Lerp(camInterface::GetGameplayDirector().GetHintBlend(), 1.0f, m_Metadata.m_FollowDistanceScalar);
		}
	}
}

void camHintHelper::ComputeHintPivotOverBoundingBoxBlendLevel(float& pivotOverBlendLevel) const
{
	if(camInterface::GetGameplayDirector().IsHintActive())	
	{
		const float blendLevel = camInterface::GetGameplayDirector().GetHintBlend();

		//Prevent hinting from reducing the blend level.
		const float hintPivotOverBlendLevel = Max(m_Metadata.m_PivotOverBoundingBoxBlendLevel, pivotOverBlendLevel);

		pivotOverBlendLevel = Lerp(blendLevel, pivotOverBlendLevel, hintPivotOverBlendLevel);
	}
}

void camHintHelper::ComputeHintPivotPositionAdditiveOffset(float& SideOffset, float& VerticalOffset) const
{
	SideOffset = 0.0f; 
	VerticalOffset = 0.0f; 
	if(camInterface::GetGameplayDirector().IsHintActive())	
	{	
		if(camInterface::GetGameplayDirector().IsScriptOverridingScriptHintCameraRelativeSideOffsetAdditive())
		{
			SideOffset = Lerp(camInterface::GetGameplayDirector().GetHintBlend(), 0.0f, camInterface::GetGameplayDirector().GetScriptOverriddenHintCameraRelativeSideOffsetAdditive());
		}
		else
		{
			SideOffset = Lerp(camInterface::GetGameplayDirector().GetHintBlend(), 0.0f, m_Metadata.m_PivotPositionAdditive.m_CameraRelativeSideOffsetAdditive);
		}
		
		if(camInterface::GetGameplayDirector().IsScriptOverridingScriptHintCameraRelativeVerticalOffsetAdditive())
		{
			VerticalOffset = Lerp(camInterface::GetGameplayDirector().GetHintBlend(), 0.0f, camInterface::GetGameplayDirector().GetScriptOverriddenHintCameraRelativeVerticalOffsetAdditive());
		}
		else
		{
			VerticalOffset = Lerp(camInterface::GetGameplayDirector().GetHintBlend(), 0.0f, m_Metadata.m_PivotPositionAdditive.m_CameraRelativeVerticalOffsetAdditive);
		}
		
	}
}

void camHintHelper::ComputeHintParentSizeScalingAdditiveOffset(float& SideScalar, float& VerticalScalar) const
{
	SideScalar = 0.0f; 
	VerticalScalar = 0.0f; 
	if(camInterface::GetGameplayDirector().IsHintActive())	
	{	
		SideScalar = Lerp(camInterface::GetGameplayDirector().GetHintBlend(), 0.0f, m_Metadata.m_PivotPositionAdditive.m_ParentWidthScalingForCameraRelativeSideOffsetAdditive);
		VerticalScalar = Lerp(camInterface::GetGameplayDirector().GetHintBlend(), 0.0f, m_Metadata.m_PivotPositionAdditive.m_ParentHeightScalingForCameraRelativeVerticalOffsetAdditive);
	}
}
