//
// camera/cinematic/CinematicSpectatorNewsChannelContext.h
// 
// Copyright (C) 1999-2013 Rockstar Games.  All Rights Reserved.
//

#ifndef CINEMATIC_SPECTATOR_NEWS_CHANNEL_CONTEXT_H
#define CINEMATIC_SPECTATOR_NEWS_CHANNEL_CONTEXT_H

#include "camera/cinematic/context/BaseCinematicContext.h"

class camCinematicContextMetadata;
class camBaseCinematicShot; 

class camCinematicSpectatorNewsChannelContext : public camBaseCinematicContext
{
	DECLARE_RTTI_DERIVED_CLASS(camCinematicSpectatorNewsChannelContext, camBaseCinematicContext);

public:
	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.

protected:

	virtual bool IsValid(bool UNUSED_PARAM(shouldConsiderControlInput), bool UNUSED_PARAM(shouldConsiderViewMode)) const; 

	camCinematicSpectatorNewsChannelContext(const camBaseObjectMetadata& metadata);

	const camCinematicSpectatorNewsChannelContextMetadata&	m_Metadata;

private:

};


#endif
