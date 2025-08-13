//
// camera/helpers/switch/ShortZoomToHeadSwitchHelper.cpp
// 
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved.
//

#include "camera/helpers/switch/ShortZoomToHeadSwitchHelper.h"

#include "fwanimation/boneids.h"

#include "camera/CamInterface.h"
#include "camera/system/CameraMetadata.h"
#include "peds/ped.h"

CAMERA_OPTIMISATIONS()

INSTANTIATE_RTTI_CLASS(camShortZoomToHeadSwitchHelper,0x811B8E44)


camShortZoomToHeadSwitchHelper::camShortZoomToHeadSwitchHelper(const camBaseObjectMetadata& metadata)
: camBaseSwitchHelper(metadata)
, m_Metadata(static_cast<const camShortZoomToHeadSwitchHelperMetadata&>(metadata))
{
}

bool camShortZoomToHeadSwitchHelper::ComputeShouldBlendIn() const
{
	bool shouldBlendIn = camBaseSwitchHelper::ComputeShouldBlendIn();

	//Reverse the conventional direction of the zoom for a quadruped, to better fit the transition to/from a first-person POV camera for the animal.
	const CPed* pedToConsider			= GetDesiredFollowPed();
	const CBaseCapsuleInfo* capsuleInfo	= pedToConsider ? pedToConsider->GetCapsuleInfo() : NULL;
	const bool isQuadruped				= (capsuleInfo && capsuleInfo->IsQuadruped());
	if(isQuadruped)
	{
		shouldBlendIn = !shouldBlendIn;
	}

	return shouldBlendIn;
}

void camShortZoomToHeadSwitchHelper::ComputeLookAtFront(const Vector3& cameraPosition, Vector3& lookAtFront) const
{
	if(!m_Metadata.m_ShouldLookAtBone)
	{
		return;
	}

	const CPed* followPed = camInterface::FindFollowPed();
	if(followPed)
	{
		const float zoomFactor = ComputeZoomFactor();
		if(zoomFactor < (1.0f - SMALL_FLOAT))
		{
			return;
		}

		//We are zooming in, so tend towards looking at the follow ped's head at maximum blend.

		const eAnimBoneTag lookAtBoneTag = (m_Metadata.m_LookAtBoneTag >= 0) ? (eAnimBoneTag)m_Metadata.m_LookAtBoneTag : BONETAG_HEAD;

		Matrix34 boneMatrix;
		const bool isBoneValid = followPed->GetBoneMatrix(boneMatrix, lookAtBoneTag);
		if(!isBoneValid)
		{
			return;
		}

		Vector3 lookAtPosition;
		boneMatrix.Transform(m_Metadata.m_BoneRelativeLookAtOffset, lookAtPosition);

		Vector3 toFollowPedHead = lookAtPosition - cameraPosition;
		toFollowPedHead.NormalizeSafe(lookAtFront);

		//Derive our blend level from the current zoom factor being applied, as we need to ensure that the follow ped's head consistently
		//moves towards the centre of the screen.

		const float lookAtAngleDelta = lookAtFront.Angle(toFollowPedHead);
		if(lookAtAngleDelta < SMALL_FLOAT)
		{
			return;
		}

		const float blendLevel = 1.0f - (1.0f / zoomFactor);

		Vector3 blendedLookAtFront;
		camFrame::SlerpOrientation(blendLevel, lookAtFront, toFollowPedHead, blendedLookAtFront);

		lookAtFront.Set(blendedLookAtFront);
	}
}

void camShortZoomToHeadSwitchHelper::ComputeDesiredFov(float& desiredFov) const
{
	const float zoomFactor = ComputeZoomFactor();
	if(zoomFactor >= SMALL_FLOAT)
	{
		desiredFov /= zoomFactor;
	}

	desiredFov = Max(desiredFov, m_Metadata.m_MinFovAfterZoom);
	desiredFov = Clamp(desiredFov, g_MinFov, g_MaxFov);
}

float camShortZoomToHeadSwitchHelper::ComputeZoomFactor() const
{
	const float zoomFactor = Powf(m_Metadata.m_DesiredZoomFactor, m_BlendLevel);

	return zoomFactor;
}
