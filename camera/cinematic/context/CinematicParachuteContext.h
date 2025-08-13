//
// camera/cinematic/CinematicParachuteContext.h
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#ifndef CINEMATIC_PARACHUTE_CONTEXT_H
#define CINEMATIC_PARACHUTE_CONTEXT_H

#include "camera/cinematic/context/BaseCinematicContext.h"

class camCinematicContextMetadata;
class camBaseCinematicShot; 

class camCinematicParachuteContext : public camBaseCinematicContext
{
	DECLARE_RTTI_DERIVED_CLASS(camCinematicParachuteContext, camBaseCinematicContext);

public:
	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.

protected:

	virtual bool IsValid(bool shouldConsiderControlInput = true, bool shouldConsiderViewMode = true) const; 

	camCinematicParachuteContext(const camBaseObjectMetadata& metadata);

	const camCinematicParachuteContextMetadata&	m_Metadata;

private:
};


#endif

//bool			UpdateParachuting(const CPed& followPed); 
//bool			UpdateParachutingShotSelection(const CPed& followPed); 
//bool			UpdateParachutingViewMode(const CPed& followPed); 
//void			ComputeHeliCamPathHeading(float currentHeading); 
//bool			ShouldSwitchToParachuteCinematicMode(); 