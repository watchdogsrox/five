//
// camera/cinematic/shot/camCinematicWaterCrashShot.h
// 
// Copyright (C) 1999-2013 Rockstar Games.  All Rights Reserved.
//

#ifndef CINEMATIC_WATER_CRASH_SHOT_H
#define CINEMATIC_WATER_CRASH_SHOT_H

#include "camera/base/BaseObject.h"
#include "camera/cinematic/shot/BaseCinematicShot.h"

class camCinematicWaterCrashShot : public camBaseCinematicShot
{
	DECLARE_RTTI_DERIVED_CLASS(camCinematicWaterCrashShot, camBaseCinematicShot);
public:

	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.

protected:
	virtual void InitCamera(); 
	
	virtual bool IsValid() const;   

	virtual const CEntity* ComputeAttachEntity() const;
	virtual const CEntity* ComputeLookAtEntity() const;

	camCinematicWaterCrashShot(const camBaseObjectMetadata& metadata);

private:
	const camCinematicWaterCrashShotMetadata&	m_Metadata;
};

#endif