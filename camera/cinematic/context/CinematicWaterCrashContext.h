//
// camera/cinematic/CinematicWaterCrashContext.h
// 
// Copyright (C) 1999-2013 Rockstar Games.  All Rights Reserved.
//

#ifndef CINEMATIC_WATER_CRASH_CONTEXT_H
#define CINEMATIC_WATER_CRASH_CONTEXT_H

#include "camera/cinematic/context/BaseCinematicContext.h"

class camCinematicContextMetadata;
class camBaseCinematicShot; 

class camCinematicWaterCrashContext : public camBaseCinematicContext
{
	DECLARE_RTTI_DERIVED_CLASS(camCinematicWaterCrashContext, camBaseCinematicContext);

public:
	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.
	
	const CEntity* GetTargetEntity() const { return m_TargetEntity; }

	u32 GetContextDuration() const			{ return m_Metadata.m_ContextDuration; }

	void ClearTarget()						{ m_TargetEntity = NULL; }

protected:
	virtual bool Update(); 
	virtual void PreUpdate(); 
	
	virtual bool IsValid(bool shouldConsiderControlInput = true, bool shouldConsiderViewMode = true) const; 

	camCinematicWaterCrashContext(const camBaseObjectMetadata& metadata);

	const camCinematicWaterCrashContextMetadata&	m_Metadata;

private:
	RegdConstEnt m_TargetEntity;
	RegdConstEnt m_InvalidEntity;
	u32 m_StartTime;

};

#endif // CINEMATIC_WATER_CRASH_CONTEXT_H
