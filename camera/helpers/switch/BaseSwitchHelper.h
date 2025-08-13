//
// camera/helpers/switch/BaseSwitchHelper.h
// 
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved.
//

#ifndef BASE_SWITCH_HELPER_H
#define BASE_SWITCH_HELPER_H

#include "camera/base/BaseObject.h"
#include "scene/RegdRefTypes.h"

namespace rage
{
	class Vector3;
}

class camBaseSwitchHelperMetadata; 
class camFrame;

class camBaseSwitchHelper : public camBaseObject
{
	DECLARE_RTTI_DERIVED_CLASS(camBaseSwitchHelper, camBaseObject);

public:
	FW_REGISTER_CLASS_POOL(camBaseSwitchHelper);

	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.

protected:
	camBaseSwitchHelper(const camBaseObjectMetadata& metadata);

public:
	void			Init(bool isIntro, const CPed* oldPed, const CPed* newPed, float directionSign, bool isFirstPersonSwitch, bool firstPersonCameraModeEnabled)
						{ m_IsIntro = isIntro; m_OldPed = oldPed; m_NewPed = newPed; m_DirectionSign = directionSign; m_IsFirstPersonSwitch = isFirstPersonSwitch; m_FirstPersonCameraModeEnabled = firstPersonCameraModeEnabled; }

	const CPed*		GetDesiredFollowPed() const	{ return (m_IsIntro ? m_OldPed : m_NewPed); }
	const CPed*		GetNewPed() const			{ return m_NewPed; }
	bool			IsFinished() const;
	void			SetPauseState(bool state)	{ m_IsPaused = state; }

	void			Update();
	virtual void	ComputeOrbitDistance(float& UNUSED_PARAM(orbitDistance)) const {}
	virtual void	ComputeOrbitOrientation(float UNUSED_PARAM(attachParentHeading), float& UNUSED_PARAM(orbitHeading), float& UNUSED_PARAM(orbitPitch)) const {}
	virtual void	ComputeOrbitRelativePivotOffset(Vector3& UNUSED_PARAM(orbitRelativeOffset)) const {}
	virtual void	ComputeLookAtFront(const Vector3& UNUSED_PARAM(cameraPosition), Vector3& UNUSED_PARAM(lookAtFront)) const {}
	virtual void	ComputeDesiredFov(float& UNUSED_PARAM(desiredFov)) const {}
	void			ComputeDesiredMotionBlurStrength(float& desiredMotionBlurStrength) const;

protected:

	bool IsFirstPersonSwitch() const;

	void			UpdatePhase();
	float			ComputeBlendLevel(float phase, s32 curveType, bool shouldBlendIn = true) const;
	virtual bool	ComputeShouldBlendIn() const { return m_IsIntro; }

	void			UpdatePostEffects();
	void			UpdateFpsTransitionEffect();

	const camBaseSwitchHelperMetadata& m_Metadata;
	RegdConstPed	m_OldPed;
	RegdConstPed	m_NewPed;
	float			m_DirectionSign;
	float			m_PhaseNonClipped;
	float			m_Phase;
	float			m_BlendLevel;
	bool			m_IsIntro;
	bool			m_IsPaused							: 1;
	bool			m_IsFinished						: 1;
	bool			m_HasTriggeredPostEffects			: 1;
	bool			m_HasTriggeredFpsTransitionFlash	: 1;
	bool			m_IsFirstPersonSwitch				: 1;
	bool			m_FirstPersonCameraModeEnabled		: 1;

	//Forbid copy construction and assignment.
	camBaseSwitchHelper(const camBaseSwitchHelper& other);
	camBaseSwitchHelper& operator=(const camBaseSwitchHelper& other);
};

#endif // BASE_SWITCH_HELPER_H
