//
// camera/cinematic/CinematicStuntJumpContext.h
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#ifndef CINEMATIC_STUNT_JUMP_CONTEXT_H
#define CINEMATIC_STUNT_JUMP_CONTEXT_H

#include "camera/cinematic/context/BaseCinematicContext.h"

class camCinematicContextMetadata;
class camBaseCinematicShot; 

class camCinematicStuntJumpContext : public camBaseCinematicContext
{
	DECLARE_RTTI_DERIVED_CLASS(camCinematicStuntJumpContext, camBaseCinematicContext);

public:
	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.

protected:
	virtual void PreUpdate();

	virtual bool IsValid(bool UNUSED_PARAM(shouldConsiderControlInput), bool UNUSED_PARAM(shouldConsiderViewMode)) const { return m_IsValid; }

	camCinematicStuntJumpContext(const camBaseObjectMetadata& metadata);

	const camCinematicStuntJumpContextMetadata&	m_Metadata;

private:
	bool m_IsValid;
	bool m_WasAimingDuringJump;
};


#endif




//bool			UpdateStuntJumpViewMode(const CPed& followPed); 
//bool			UpdateStuntJumpShotSelection();
//void			UpdateStuntJumpSlowMotionControl(const CPed& followPed); 
//bool			ShouldSwitchToStuntJumpCinematicMode(); 