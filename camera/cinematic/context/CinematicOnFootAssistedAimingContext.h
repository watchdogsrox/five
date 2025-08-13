//
// camera/cinematic/CinematicOnFootAssistedAimingContext.h
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#ifndef CINEMATIC_ON_FOOT_ASSITED_AIMING_CONTEXT_H
#define CINEMATIC_ON_FOOT_ASSITED_AIMING_CONTEXT_H

#include "camera/cinematic/context/BaseCinematicContext.h"

class camCinematicContextMetadata;
class camBaseCinematicShot;

class camCinematicOnFootAssistedAimingContext : public camBaseCinematicContext
{
	DECLARE_RTTI_DERIVED_CLASS(camCinematicOnFootAssistedAimingContext, camBaseCinematicContext);

public:
	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.

protected:
	virtual void	PreUpdate();
	
	virtual bool	IsValid(bool shouldConsiderControlInput = true, bool shouldConsiderViewMode = true) const;

	camCinematicOnFootAssistedAimingContext(const camBaseObjectMetadata& metadata);

private:
	const camCinematicOnFootAssistedAimingContextMetadata&	m_Metadata;

	u32		m_NumUpdates;
	bool	m_IsCinematicKillShotRunning;
};

#endif
