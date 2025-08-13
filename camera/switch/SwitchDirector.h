//
// camera/switch/SwitchDirector.h
// 
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved.
//

#ifndef SWITCH_DIRECTOR_H
#define SWITCH_DIRECTOR_H

#include "camera/base/BaseDirector.h"

class camSwitchCamera;
class camSwitchDirectorMetadata;
class CPlayerSwitchParams;

//The director responsible for Switch cameras.
class camSwitchDirector : public camBaseDirector
{
	DECLARE_RTTI_DERIVED_CLASS(camSwitchDirector, camBaseDirector);

public:
	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.

private:
	camSwitchDirector(const camBaseObjectMetadata& metadata);

public:
	~camSwitchDirector();

	bool			InitSwitch(u32 cameraNameHash, const CPed& sourcePed, const CPed& destinationPed, const CPlayerSwitchParams& switchParams);
	void			ShutdownSwitch();

	camSwitchCamera* GetSwitchCamera();
	camBaseCamera* GetActiveCamera() const { return m_ActiveCamera; }

private:
	virtual bool	Update();

	const camSwitchDirectorMetadata& m_Metadata;

	RegdCamBaseCamera m_ActiveCamera;

private:
	//Forbid copy construction and assignment.
	camSwitchDirector(const camSwitchDirector& other);
	camSwitchDirector& operator=(const camSwitchDirector& other);
};

#endif // SWITCH_DIRECTOR_H
