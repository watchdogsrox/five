//
// camera/helpers/Oscillator.h
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#ifndef CAMERA_OSCILLATOR_H
#define CAMERA_OSCILLATOR_H

#include "camera/base/BaseObject.h"
#include "control/replay/Replay.h"

class camOscillatorMetadata;

//A simple oscillator.
class camOscillator : public camBaseObject
{
	DECLARE_RTTI_DERIVED_CLASS(camOscillator, camBaseObject);

public:
	FW_REGISTER_CLASS_POOL(camOscillator);

	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.

private:
	camOscillator(const camBaseObjectMetadata& metadata);

public:
	float	Update(float fFrequencyScale REPLAY_ONLY(, float timeScaleFactor = 1.0f, s32 timeOffset = 0));

	float	GetLevel() const { return m_Level; }

	void	SetUseGameTime(bool b)	{ m_UseGameTime = b; }
	void	SetUseNonPausedCameraTime(bool b)	{ m_UseNonPausedCameraTime = b; }

private:
	const camOscillatorMetadata& m_Metadata;

	float	m_Phase;
	float	m_Angle;
	float	m_Decay;
	float	m_Level;

	bool	m_UseGameTime;
	bool	m_UseNonPausedCameraTime;

private:
	//Forbid copy construction and assignment.
	camOscillator(const camOscillator& other);
	camOscillator& operator=(const camOscillator& other);
};

#endif // CAMERA_OSCILLATOR_H
