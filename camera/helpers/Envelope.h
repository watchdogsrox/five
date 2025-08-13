//
// camera/helpers/Envelope.h
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#ifndef CAMERA_ENVELOPE_H
#define CAMERA_ENVELOPE_H

#include "camera/base/BaseObject.h"
#include "control/replay/Replay.h"
#include "fwsys/timer.h"

class camEnvelopeMetadata;

//A simple Attack/Decay/Sustain/Hold/Release (ADSHR) envelope.
class camEnvelope : public camBaseObject
{
	DECLARE_RTTI_DERIVED_CLASS(camEnvelope, camBaseObject);

public:
	FW_REGISTER_CLASS_POOL(camEnvelope);

	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.

private:
	enum ePhase
	{
		PREDELAY_PHASE,
		ATTACK_PHASE,
		DECAY_PHASE,
		HOLD_PHASE,
		RELEASE_PHASE
	};

	camEnvelope(const camBaseObjectMetadata& metadata);

public:
	bool	IsActive() const	{ return m_IsActive; }
	float	GetLevel() const	{ return m_Level; }

	void	AutoStartStop(bool shouldStart, bool shouldWaitForHoldOnRelease = false);
	void	AutoStart();
	void	Start(float initialLevel = 0.0f);
	float	Update(REPLAY_ONLY(float timeScaleFactor = 1.0f, s32 timeOffset = 0));
	void	Release();
	void	Stop();

	void	SetUseGameTime(bool b)				{ m_UseGameTime = b; }
	void	SetUseNonPausedCameraTime(bool b)	{ m_UseNonPausedCameraTime = b; }
	void	SetReversible(bool b)				{ m_IsReversible = b; }

	bool	IsInAttackPhase() const		{ return (m_IsActive && m_Phase == ATTACK_PHASE); }
	bool	IsInReleasePhase() const	{ return (m_IsActive && m_Phase == RELEASE_PHASE); }
	float	GetMaxReleaseLevel() const	{ return (m_MaxReleaseLevel); }

	void	OverrideAttackDuration(u32 duration)	{ m_OverrideAttackDuration = duration; }
	void	OverrideHoldDuration(s32 duration)		{ m_OverrideHoldDuration = duration; }
	void	OverrideStartTime(u32 startTime);

	u32		GetAttackDuration() const;
	u32		GetHoldDuration() const;
	u32		GetReleaseDuration() const;

private:
	float	UpdateReversible(REPLAY_ONLY(float timeScaleFactor = 1.0f) REPLAY_ONLY(, s32 timeOffset = 0));
	
	float CalculateReleaseResponse(float Phase) const; 
	float CalculateAttackResponse(float Phase) const;
	float CalculateInverseAttackResponse(float desiredLevel) const;

	u32 GetCurrentTimeInMilliseconds() const { return (m_UseNonPausedCameraTime) ? fwTimer::GetNonPausableCamTimeInMilliseconds() : (m_UseGameTime) ? fwTimer::GetTimeInMilliseconds() : fwTimer::GetCamTimeInMilliseconds(); }

	const camEnvelopeMetadata& m_Metadata;
	ePhase	m_Phase;
	u32		m_PhaseStartTime;
	u32		m_EnvelopeStartTime;
	u32		m_OverrideStartTime;
	u32		m_OverrideAttackDuration;
	s32		m_OverrideHoldDuration;
	float	m_Level;
	float	m_MaxReleaseLevel;
	bool	m_IsActive;
	bool	m_UseGameTime;
	bool	m_IsReversible;
	bool	m_UseNonPausedCameraTime;

private:
	//Forbid copy construction and assignment.
	camEnvelope(const camEnvelope& other);
	camEnvelope& operator=(const camEnvelope& other);
};

#endif // CAMERA_ENVELOPE_H
