//
// camera/cinematic/shot/camCinematicOnFootAssistedAimingShot.h
// 
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved.
//

#ifndef CINEMATIC_ON_FOOT_ASSISTED_AIMING_SHOT_H
#define CINEMATIC_ON_FOOT_ASSISTED_AIMING_SHOT_H

#include "camera/base/BaseObject.h"
#include "camera/cinematic/shot/BaseCinematicShot.h"

class camCinematicOnFootAssistedAimingKillShot : public camBaseCinematicShot
{
	DECLARE_RTTI_DERIVED_CLASS(camCinematicOnFootAssistedAimingKillShot, camBaseCinematicShot);
public:

	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.

	virtual void	PreUpdate(bool IsCurrentShot);
	virtual bool	IsValid() const; 
	
protected:
	camCinematicOnFootAssistedAimingKillShot(const camBaseObjectMetadata& metadata);

	virtual void	InitCamera(); 
	virtual bool	CanCreate();
	const CEntity*	ComputeLookAtEntity() const;

private:
	const camCinematicOnFootAssistedAimingKillShotMetadata&	m_Metadata;
};

class camCinematicOnFootAssistedAimingReactionShot : public camBaseCinematicShot
{
	DECLARE_RTTI_DERIVED_CLASS(camCinematicOnFootAssistedAimingReactionShot, camBaseCinematicShot);
public:

	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.
	
	virtual bool IsValid() const;

protected:
	virtual void InitCamera(); 
	
	virtual bool CanCreate(); 

	camCinematicOnFootAssistedAimingReactionShot(const camBaseObjectMetadata& metadata);
	
private:
	const camCinematicOnFootAssistedAimingReactionShotMetadata&	m_Metadata;

};

#endif
