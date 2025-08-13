//
// camera/cinematic/camCinematicOnFootSpectatingContext.cpp
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#include "camera/cinematic/context/CinematicOnFootSpectatingContext.h"

#include "camera/cinematic/CinematicDirector.h"
#include "camera/helpers/Collision.h"
#include "camera/helpers/ControlHelper.h"
#include "network/NetworkInterface.h"

INSTANTIATE_RTTI_CLASS(camCinematicOnFootSpectatingContext,0x760DF29B)

CAMERA_OPTIMISATIONS()

camCinematicOnFootSpectatingContext::camCinematicOnFootSpectatingContext(const camBaseObjectMetadata& metadata)
: camBaseCinematicContext(metadata)
, m_Metadata(static_cast<const camCinematicOnFootSpectatingContextMetadata&>(metadata))
{
}

bool camCinematicOnFootSpectatingContext::IsValid(bool shouldConsiderControlInput, bool shouldConsiderViewMode) const
{
	if(NetworkInterface::IsInSpectatorMode())
	{
		const camGameplayDirector& gameplayDirector	= camInterface::GetGameplayDirector();
		const s32 vehicleEntryExitState				= gameplayDirector.GetVehicleEntryExitState();
		
		const CPed* followPed = camInterface::FindFollowPed();

		if(followPed)
		{
			if(vehicleEntryExitState != camGameplayDirector::INSIDE_VEHICLE)
			{
				if(!shouldConsiderControlInput)
				{
					//Bypass all further checks, relating to control input.
					return true;
				}

				const camCinematicDirector& cinematicDirector = camInterface::GetCinematicDirector();
				if(cinematicDirector.IsCinematicInputActive())
				{
					return true;
				}
				else if(shouldConsiderViewMode)
				{
					s32 desiredViewMode = camControlHelper::GetViewModeForContext(camControlHelperMetadataViewMode::ON_FOOT); 
					if (desiredViewMode == camControlHelperMetadataViewMode::CINEMATIC)
					{
						return true;
					}
				}
			}
		}
	}
	return false; 
}
