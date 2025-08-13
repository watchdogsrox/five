//
// camera/cinematic/shot/camCinematicParachuteShot.h
// 
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved.
//

#ifndef CINEMATIC_PARACHUTE_SHOT_H
#define CINEMATIC_PARACHUTE_SHOT_H

#include "camera/base/BaseObject.h"
#include "camera/cinematic/shot/BaseCinematicShot.h"

class camCinematicParachuteHeliShot : public camBaseCinematicShot
{
	DECLARE_RTTI_DERIVED_CLASS(camCinematicParachuteHeliShot, camBaseCinematicShot);
public:

	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.

protected:

	virtual void InitCamera(); 

	virtual void Update();

	camCinematicParachuteHeliShot(const camBaseObjectMetadata& metadata);

private:
	u32 m_HeliTrackingAccuracyMode;
	u32 m_DesiredTrackingAccuracyMode; 

	const camCinematicParachuteHeliShotMetadata&	m_Metadata;
};


class camCinematicParachuteCameraManShot : public camBaseCinematicShot
{
	DECLARE_RTTI_DERIVED_CLASS(camCinematicParachuteCameraManShot, camBaseCinematicShot);
public:

	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.

protected:

	virtual void InitCamera(); 

	camCinematicParachuteCameraManShot(const camBaseObjectMetadata& metadata);


private:
	const camCinematicParachuteCameraManShotMetadata&	m_Metadata;
};


#endif