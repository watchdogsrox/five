//
// camera/helpers/FrameShaker.h
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#ifndef FRAME_SHAKER_H
#define FRAME_SHAKER_H

#include "camera/helpers/BaseFrameShaker.h"
#include "scene/RegdRefTypes.h"

class camEnvelope;
class camOscillator;
class camShakeMetadata;

struct tShakeComponent
{
	tShakeComponent() :
	m_FrameComponent(0),
	m_Oscillator(NULL),
	m_Envelope(NULL)
	{
	}

	u32				m_FrameComponent;
	camOscillator*	m_Oscillator;
	camEnvelope*	m_Envelope;
};

//A helper that shakes a camera's frame using a predefined set of oscillators.
class camOscillatingFrameShaker : public camBaseFrameShaker
{
	DECLARE_RTTI_DERIVED_CLASS(camOscillatingFrameShaker, camBaseFrameShaker);

public:
	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.

private:
	camOscillatingFrameShaker(const camBaseObjectMetadata& metadata);

public:
	~camOscillatingFrameShaker();

	void	SetFreqScale(float scale);

	void	Update(camFrame& frameToShake);
	void	AutoStart();

	static void	PostUpdateClass();

	virtual void OverrideStartTime(u32 startTime);
	virtual void SetUseNonPausedCameraTime(bool b);

#if __BANK
	static void	AddWidgets(bkBank &bank);
#endif // __BANK

private:
	virtual bool	UpdateFrame(float overallEnvelopeLevel, camFrame& frameToShake);
	virtual bool	ShakeFrame(float overallEnvelopeLevel, camFrame& frameToShake) const;
	void			AppyShakeToFrameComponent(float shakeValue, camFrame& frameToShake, u32 frameComponent) const;

	const camShakeMetadata& m_Metadata;

	atArray<tShakeComponent> m_Components;
	float			m_FrequencyScale;

#if __BANK
	static void		ShakeGameplayDirector();
	static void		StopShakingGameplayDirector();
	static void		RenderShakePath();

	static char		ms_DebugCustomShakeName[100];
	static s32		ms_DebugStockShakeNameIndex;
	static s32		ms_ShakeComponentToGraph; 
	static float	ms_DebugShakeAmplitude;
	static float	ms_ShakeComponentValueToGraph; 
	static bool		ms_DebugShouldStopShakingImmediately;
	static bool		ms_RenderShakePath;
#endif // __BANK

private:
	//Forbid copy construction and assignment.
	camOscillatingFrameShaker(const camOscillatingFrameShaker& other);
	camOscillatingFrameShaker& operator=(const camOscillatingFrameShaker& other);
};

#endif // FRAME_SHAKER_H
