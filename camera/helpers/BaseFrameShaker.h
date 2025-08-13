//
// camera/helpers/BaseFrameShaker.h
// 
// Copyright (C) 1999-2013 Rockstar Games.  All Rights Reserved.
//

#ifndef BASE_FRAME_SHAKER_H
#define BASE_FRAME_SHAKER_H

#include "fwtl/pool.h"

#include "camera/base/BaseObject.h"
#include "control/replay/Replay.h"
#include "scene/RegdRefTypes.h"

class camEnvelope;
class camBaseShakeMetadata;

//A base helper class that forms the basis of all data driven frame shaker classes.
class camBaseFrameShaker : public camBaseObject
{
	DECLARE_RTTI_DERIVED_CLASS(camBaseFrameShaker, camBaseObject);

public:
	FW_REGISTER_CLASS_POOL(camBaseFrameShaker);

	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.

protected:
	camBaseFrameShaker(const camBaseObjectMetadata& metadata);

public:
	virtual ~camBaseFrameShaker();

	u32		GetCurrentTimeMilliseconds() const;
	u32		GetStartTime() const			{ return m_StartTime; }
	u32		GetNumUpdatesPerformed() const	{ return m_NumUpdatesPerformed; }

	void	SetAmplitude(float amplitude)	{ m_Amplitude = amplitude; }
	float	GetAmplitude() const			{ return (m_Amplitude); }

#if GTA_REPLAY
	void	SetTimeScaleFactor(float timeScaleFactor)	{ m_TimeScaleFactor = timeScaleFactor; }
	float	GetTimeScaleFactor() const					{ return m_TimeScaleFactor; }

	void	SetTimeOffset(s32 timeOffset)			{ m_TimeOffset = timeOffset; }
	s32		GetTimeOffset() const					{ return m_TimeOffset; }
#endif

	void			Update(camFrame& frameToShake, float amplitudeScalingFactor = 1.0f);
	virtual void	AutoStart();
	void			Release();

	float	GetReleaseLevel();

	void	RemoveRef(RegdCamBaseFrameShaker& shakeRef);

	void	BypassThisUpdate()				{ m_ShouldBypassThisUpdate = true; }

	void	ShakeFrame(camFrame& frameToShake) const;

	static void		CleanupUnreferencedFrameShakers();

	virtual void OverrideStartTime(u32 startTime);
	virtual void SetUseNonPausedCameraTime(bool b);

	bool IsOneShot() const;

protected:
	virtual bool	UpdateFrame(float UNUSED_PARAM(overallEnvelopeLevel), camFrame& UNUSED_PARAM(frameToShake)) { return true; }
	virtual bool	ShakeFrame(float UNUSED_PARAM(overallEnvelopeLevel), camFrame& UNUSED_PARAM(frameToShake)) const { return true; }

	const camBaseShakeMetadata& m_Metadata;

	camEnvelope*	m_OverallEnvelope;
	u32				m_StartTime;
	u32				m_NumUpdatesPerformed;
#if GTA_REPLAY
	s32				m_TimeOffset;
	float			m_TimeScaleFactor;
#endif
	float			m_Amplitude;
	bool			m_IsActive					: 1;
	bool			m_ShouldBypassThisUpdate	: 1;
	bool			m_UseNonPausedCameraTime	: 1;

private:
	//Forbid copy construction and assignment.
	camBaseFrameShaker(const camBaseFrameShaker& other);
	camBaseFrameShaker& operator=(const camBaseFrameShaker& other);
};

#endif // BASE_FRAME_SHAKER_H
