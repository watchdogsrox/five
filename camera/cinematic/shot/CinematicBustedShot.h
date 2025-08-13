//
// camera/cinematic/shot/CinematicBustedShot.h
// 
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved.
//

#ifndef CINEMATIC_BUSTED_SHOT_H
#define CINEMATIC_BUSTED_SHOT_H

#include "camera/cinematic/shot/BaseCinematicShot.h"

class camCinematicBustedShot : public camBaseCinematicShot
{
	DECLARE_RTTI_DERIVED_CLASS(camCinematicBustedShot, camBaseCinematicShot);

public:
	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.
	
protected:
	camCinematicBustedShot(const camBaseObjectMetadata& metadata);
	
protected:
	virtual void	InitCamera();
	virtual bool	CanCreate();

	virtual const CEntity* ComputeAttachEntity() const; 

	const CPed*		ComputeArrestingPed() const;

	const camCinematicBustedShotMetadata&	m_Metadata;
};

#endif // CINEMATIC_BUSTED_SHOT_H
