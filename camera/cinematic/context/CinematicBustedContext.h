//
// camera/cinematic/CinematicBustedContext.h
// 
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved.
//

#ifndef CINEMATIC_BUSTED_CONTEXT_H
#define CINEMATIC_BUSTED_CONTEXT_H

#include "camera/cinematic/context/BaseCinematicContext.h"

class camCinematicBustedContext : public camBaseCinematicContext
{
	DECLARE_RTTI_DERIVED_CLASS(camCinematicBustedContext, camBaseCinematicContext);

public:
	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.

	const CPed*		GetArrestingPed() const { return m_ArrestingPed; }

protected:
	virtual void	PreUpdate();
	virtual bool	IsValid(bool UNUSED_PARAM(shouldConsiderControlInput), bool UNUSED_PARAM(shouldConsiderViewMode)) const { return (m_ArrestingPed.Get() != NULL); }

	bool			ComputeIsPedArrestingPed(const CPed& arrestingPed, const CPed& pedToBeArrested) const;

	camCinematicBustedContext(const camBaseObjectMetadata& metadata);

	const camCinematicBustedContextMetadata& m_Metadata;

	RegdConstPed m_ArrestingPed;
};

#endif // CINEMATIC_BUSTED_CONTEXT_H
