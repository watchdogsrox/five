//
// camera/cinematic/shot/camCinematicScriptShot.h
// 
// Copyright (C) 1999-2013 Rockstar Games.  All Rights Reserved.
//

#ifndef CINEMATIC_SCRIPT_SHOT_H
#define CINEMATIC_SCRIPT_SHOT_H

#include "camera/base/BaseObject.h"
#include "camera/cinematic/shot/BaseCinematicShot.h"

class camCinematicScriptRaceCheckPointShot : public camBaseCinematicShot
{
	DECLARE_RTTI_DERIVED_CLASS(camCinematicScriptRaceCheckPointShot, camBaseCinematicShot);
public:

	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.

	void SetPosition(const Vector3& pos) { m_Position = pos; }

protected:
	virtual void InitCamera(); 

	camCinematicScriptRaceCheckPointShot(const camBaseObjectMetadata& metadata);

	
private:
	const camCinematicScriptRaceCheckPointShotMetadata&	m_Metadata;

	Vector3 m_Position; 
};

#endif