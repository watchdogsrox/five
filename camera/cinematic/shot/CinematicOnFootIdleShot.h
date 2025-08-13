//
// camera/cinematic/shot/camCinematicOnFootIdleShot.h
// 
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved.
//

#ifndef CINEMATIC_ON_FOOT_IDLE_SHOT_H
#define CINEMATIC_ON_FOOT_IDLE_SHOT_H

#include "camera/base/BaseObject.h"
#include "camera/cinematic/shot/BaseCinematicShot.h"

class camCinematicOnFootIdleShot : public camBaseCinematicShot
{
	DECLARE_RTTI_DERIVED_CLASS(camCinematicOnFootIdleShot, camBaseCinematicShot);
public:

	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.

protected:
	virtual void InitCamera(); 

	camCinematicOnFootIdleShot(const camBaseObjectMetadata& metadata);

	void SetUpCamera(camBaseCamera* camera); 
	
	virtual bool CreateAndTestCamera(u32 hash);
	
	virtual void ClearShot(bool ResetCameraEndTime = false); 
private:
	const camCinematicOnFootIdleShotMetadata&	m_Metadata;
	u32 m_cameraCreationsForThisShot; 
	 
};

#endif