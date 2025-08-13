//
// camera/cinematic/shot/camCinematicOnFootSpectatingShot.h
// 
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved.
//

#ifndef CINNEMATIC_ON_FOOT_SPECTATING_SHOT_H
#define CINNEMATIC_ON_FOOT_SPECTATING_SHOT_H

#include "camera/base/BaseObject.h"
#include "camera/cinematic/shot/BaseCinematicShot.h"

class camCinematicOnFootSpectatingShot : public camBaseCinematicShot
{
	DECLARE_RTTI_DERIVED_CLASS(camCinematicOnFootSpectatingShot, camBaseCinematicShot);
public:

	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.


protected:
	virtual void InitCamera(); 

	camCinematicOnFootSpectatingShot(const camBaseObjectMetadata& metadata);


private:
	const camCinematicOnFootSpectatingShotMetadata&	m_Metadata;
};

#endif