//
// camera/cinematic/camCinematicOnFootSpectatingContext.h
// 
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved.
//

#ifndef CINEMATIC_ON_FOOT_SPECTATING_CONTEXT_H
#define CINEMATIC_ON_FOOT_SPECTATING_CONTEXT_H

#include "camera/cinematic/context/BaseCinematicContext.h"

class camCinematicContextMetadata;
class camBaseCinematicShot; 

class camCinematicOnFootSpectatingContext : public camBaseCinematicContext
{
	DECLARE_RTTI_DERIVED_CLASS(camCinematicOnFootSpectatingContext, camBaseCinematicContext);

public:
	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.

protected:
	virtual bool IsValid(bool shouldConsiderControlInput = true, bool shouldConsiderViewMode = true) const; 

	camCinematicOnFootSpectatingContext(const camBaseObjectMetadata& metadata);

	const camCinematicOnFootSpectatingContextMetadata&	m_Metadata;

private:

};


#endif