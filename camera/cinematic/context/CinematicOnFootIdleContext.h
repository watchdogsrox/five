//
// camera/cinematic/CinematicOnFootIdleContext.h
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#ifndef CINEMATIC_ON_FOOT_IDLE_CONTEXT_H
#define CINEMATIC_ON_FOOT_IDLE_CONTEXT_H

#include "camera/cinematic/context/BaseCinematicContext.h"

class camCinematicContextMetadata;
class camBaseCinematicShot; 

class camCinematicOnFootIdleContext : public camBaseCinematicContext
{
	DECLARE_RTTI_DERIVED_CLASS(camCinematicOnFootIdleContext, camBaseCinematicContext);

public:
	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.

	void Invalidate();

	virtual bool Update(); 
protected:
	virtual void PreUpdate(); 

	virtual bool IsValid(bool shouldConsiderControlInput = true, bool shouldConsiderViewMode = true) const; 
	
	virtual void ClearContext(); 

	bool IsPedIdle() const; 
	
	bool IsPlayerInATrain(const CPed* followPed) const; 

	camCinematicOnFootIdleContext(const camBaseObjectMetadata& metadata);

	const camCinematicOnFootIdleContextMetadata&	m_Metadata;

private:

private:
	Vector3 m_FollowPedPositionOnPreviousUpdate; 
	float m_FollowPedHealthOnPreviousUpdate;
	u32 m_PreviousIdleCameraInvalidationTime; 
	bool m_IsPedIdle; 
};


#endif


//enum eInIdleViewMode
//{
//	II_THIRD_PERSON,
//	II_IDLE,
//	II_NUM_MODES
//};

//bool			UpdateInIdle(const CPed& followPed); 
//void			UpdateInIdleShotSelection(const CPed& followPed); 
//bool			UpdateInIdleViewMode(const CPed& followPed); 
//bool			ComputeShouldActivateIdleCamera(const CPed& followPed);
