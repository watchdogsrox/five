//
// camera/helpers/switch/ShortZoomInOutSwitchHelper.cpp
// 
// Copyright (C) 1999-2013 Rockstar Games.  All Rights Reserved.
//

#include "camera/helpers/switch/ShortZoomInOutSwitchHelper.h"

#include "math/vecmath.h"

#include "camera/helpers/Frame.h"
#include "camera/system/CameraMetadata.h"

CAMERA_OPTIMISATIONS()

INSTANTIATE_RTTI_CLASS(camShortZoomInOutSwitchHelper,0xAC76F2B5)


camShortZoomInOutSwitchHelper::camShortZoomInOutSwitchHelper(const camBaseObjectMetadata& metadata)
: camBaseSwitchHelper(metadata)
, m_Metadata(static_cast<const camShortZoomInOutSwitchHelperMetadata&>(metadata))
{
}

void camShortZoomInOutSwitchHelper::ComputeOrbitDistance(float& orbitDistance) const
{
	const bool shouldZoomIn				= (m_DirectionSign >= 0.0f);
	const float desiredScalingFactor	= shouldZoomIn ? m_Metadata.m_DesiredOrbitDistanceScalingForZoomIn :
											m_Metadata.m_DesiredOrbitDistanceScalingForZoomOut;

	//NOTE: We punch in and zoom out, rather than zooming out from the base orbit distance.
	const float blendLevel				= shouldZoomIn ? m_BlendLevel : (1.0f - m_BlendLevel);
	const float scalingFactorToApply	= Lerp(blendLevel, 1.0f, desiredScalingFactor);

	orbitDistance *= scalingFactorToApply;
}

void camShortZoomInOutSwitchHelper::ComputeDesiredFov(float& desiredFov) const
{
	const float zoomFactor = ComputeZoomFactor(desiredFov);
	if(zoomFactor >= SMALL_FLOAT)
	{
		desiredFov /= zoomFactor;
	}
	desiredFov = Clamp(desiredFov, g_MinFov, g_MaxFov);
}

float camShortZoomInOutSwitchHelper::ComputeZoomFactor(float baseFov) const
{
	const bool shouldZoomIn		= (m_DirectionSign >= 0.0f);
	const float maxZoomFactor	= baseFov / Max(m_Metadata.m_MinFovAfterZoom, g_MinFov);
	float desiredZoomFactor		= shouldZoomIn ? m_Metadata.m_DesiredZoomInFactor : m_Metadata.m_DesiredZoomOutFactor;
	desiredZoomFactor			= Min(desiredZoomFactor, maxZoomFactor);

	//NOTE: We punch in and zoom out, rather than zooming out from the base FOV.
	const float blendLevel		= shouldZoomIn ? m_BlendLevel : (1.0f - m_BlendLevel);
	const float zoomFactor		= Powf(desiredZoomFactor, blendLevel);

	return zoomFactor;
}
