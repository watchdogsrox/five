//
// camera/cinematic/CinematicSpectatorNewsChannelContext.cpp
// 
// Copyright (C) 1999-2013 Rockstar Games.  All Rights Reserved.
//

#include "camera/cinematic/context/CinematicSpectatorNewsChannelContext.h"
#include "camera/CamInterface.h"
#include "camera/system/CameraMetadata.h"
#include "camera/CamInterface.h"
#include "camera/cinematic/CinematicDirector.h"

INSTANTIATE_RTTI_CLASS(camCinematicSpectatorNewsChannelContext,0xF3A62C59)

CAMERA_OPTIMISATIONS()

camCinematicSpectatorNewsChannelContext::camCinematicSpectatorNewsChannelContext(const camBaseObjectMetadata& metadata)
: camBaseCinematicContext(metadata)
, m_Metadata(static_cast<const camCinematicSpectatorNewsChannelContextMetadata&>(metadata))
{
}

bool camCinematicSpectatorNewsChannelContext::IsValid(bool UNUSED_PARAM(shouldConsiderControlInput), bool UNUSED_PARAM(shouldConsiderViewMode)) const
{	
	if(camInterface::GetCinematicDirector().CanActivateNewsChannelContext())
	{
		return true; 
	}

	return false;
}