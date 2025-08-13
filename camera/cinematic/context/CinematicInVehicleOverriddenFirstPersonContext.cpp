//
// camera/cinematic/camCinematicInVehicleOverriddenFirstPersonContext.cpp
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#include "camera/cinematic/context/CinematicInVehicleOverriddenFirstPersonContext.h"
#include "camera/system/CameraMetadata.h"
#include "camera/cinematic/CinematicDirector.h"
#include "frontend/MobilePhone.h"

INSTANTIATE_RTTI_CLASS(camCinematicInVehicleOverriddenFirstPersonContext,0xA757F0AC)

CAMERA_OPTIMISATIONS()

camCinematicInVehicleOverriddenFirstPersonContext::camCinematicInVehicleOverriddenFirstPersonContext(const camBaseObjectMetadata& metadata)
: camBaseCinematicContext(metadata)
, m_Metadata(static_cast<const camCinematicInVehicleOverriddenFirstPersonContextMetadata&>(metadata))
{
}

bool camCinematicInVehicleOverriddenFirstPersonContext::IsValid(bool UNUSED_PARAM(shouldConsiderControlInput), bool UNUSED_PARAM(shouldConsiderViewMode)) const
{
	const bool isRenderingMobilePhoneCamera = CPhoneMgr::CamGetState();
	
	camGameplayDirector& gameplayDirector	= camInterface::GetGameplayDirector();
	const s32 vehicleEntryExitState			= gameplayDirector.GetVehicleEntryExitState();
	
	if(vehicleEntryExitState == camGameplayDirector::INSIDE_VEHICLE)
	{
		if(!camInterface::GetCinematicDirector().IsFirstPersonInVehicleDisabledThisUpdate() && isRenderingMobilePhoneCamera)
		{
			return true;
		}
	}
	return false;

}