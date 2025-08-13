//
// camera/helpers/AnimatedFrameShaker.h
// 
// Copyright (C) 1999-2013 Rockstar Games.  All Rights Reserved.
//

#ifndef ANIMATED_FRAME_SHAKER_H
#define ANIMATED_FRAME_SHAKER_H

#include "fwtl/pool.h"
#include "fwsys/timer.h"
#include "fwanimation/animdirector.h"

#include "camera/base/BaseObject.h"
#include "camera/helpers/BaseFrameShaker.h"
#include "scene/RegdRefTypes.h"

#include "atl/hashstring.h"
#include "cutscene/CutSceneAnimation.h"

#if __BANK
class CDebugClipSelector;
#endif

//A helper that shakes a camera's frame using an animation.
class camAnimatedFrameShaker : public camBaseFrameShaker
{
	DECLARE_RTTI_DERIVED_CLASS(camAnimatedFrameShaker, camBaseFrameShaker);

public:
	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.

private:
	camAnimatedFrameShaker(const camBaseObjectMetadata& metadata);

public:
	~camAnimatedFrameShaker();

	bool			SetAnimation(const char* pszAnimDictionary, const char* pszAnimClipName);

#if __BANK
	static void		AddWidgets(bkBank &bank);
#endif // __BANK

private:
	virtual bool	UpdateFrame(float overallEnvelopeLevel, camFrame& frameToShake);
	virtual bool	ShakeFrame(float overallEnvelopeLevel, camFrame& frameToShake) const;
	void			UpdateAnimationPhase();

#if __BANK
	static void		AddAnimationSelectorWidget();
	static void		ShakeGameplayDirector();
	static void		StopShakingGameplayDirector();

	static CDebugClipSelector ms_cameraAnimSelector;
	static bkBank*	ms_WidgetBank;
	static bkGroup*	ms_WidgetGroup;
	static char		ms_DebugAnimDictionary[100];
	static float	ms_fAmplitude;
#endif // __BANK

	CCutsceneAnimation m_Animation;
	strStreamingObjectName	m_AnimDictionary;
	strStreamingObjectName	m_AnimClip;
	s32				m_AnimDictIndex;
	float			m_AnimPhase;
	bool			m_bStartAnimNextFrame;

private:
	//Forbid copy construction and assignment.
	camAnimatedFrameShaker(const camAnimatedFrameShaker& other);
	camAnimatedFrameShaker& operator=(const camAnimatedFrameShaker& other);
};

#endif // ANIMATED_FRAME_SHAKER_H
