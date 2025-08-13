//
// camera/cinematic/CinematicOnFootMeleeContext.h
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#ifndef CINEMATIC_ON_FOOT_MELEE_CONTEXT_H
#define CINEMATIC_ON_FOOT_MELEE_CONTEXT_H

#include "camera/cinematic/context/BaseCinematicContext.h"

class camCinematicContextMetadata;
class camBaseCinematicShot; 

class camCinematicOnFootMeleeContext : public camBaseCinematicContext
{
	DECLARE_RTTI_DERIVED_CLASS(camCinematicOnFootMeleeContext, camBaseCinematicContext);

public:
	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.

protected:
	virtual bool IsValid(bool shouldConsiderControlInput = true, bool shouldConsiderViewMode = true) const; 

	camCinematicOnFootMeleeContext(const camBaseObjectMetadata& metadata);

	const camCinematicOnFootMeleeContextMetadata&	m_Metadata;

private:
};


#endif
