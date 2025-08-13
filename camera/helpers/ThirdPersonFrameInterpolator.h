//
// camera/helpers/ThirdPersonFrameInterpolator.h
// 
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved.
//

#ifndef THIRD_PERSON_FRAME_INTERPOLATOR_H
#define THIRD_PERSON_FRAME_INTERPOLATOR_H

#include "camera/helpers/FrameInterpolator.h"

class camThirdPersonCamera;

class camThirdPersonFrameInterpolator : public camFrameInterpolator
{
	DECLARE_RTTI_DERIVED_CLASS(camThirdPersonFrameInterpolator, camFrameInterpolator);

public:
	FW_REGISTER_CLASS_POOL(camThirdPersonFrameInterpolator);

	camThirdPersonFrameInterpolator(camThirdPersonCamera& sourceCamera, const camThirdPersonCamera& destinationCamera,
		u32 duration = g_DefaultInterpolationDuration, bool shouldDeleteSourceCamera = false,
		u32 timeInMilliseconds = fwTimer::GetCamTimeInMilliseconds());

	virtual bool	HasCachedSourceRelativeToDestination() const	{ return m_AreSourceParametersCachedRelativeToDestination; }
	const Vector3&	GetInterpolatedBasePivotPosition() const		{ return m_InterpolatedBasePivotPosition; }
	const Vector3&	GetInterpolatedBaseFront() const				{ return m_InterpolatedBaseFront; }
	const Vector3&	GetInterpolatedPivotPosition() const			{ return m_InterpolatedPivotPosition; }
	const Vector3&	GetInterpolatedCollisionRootPosition() const	{ return m_InterpolatedCollisionRootPosition; }
	float			GetInterpolatedPreToPostCollisionLookAtOrientationBlendValue() const
						{ return m_InterpolatedPreToPostCollisionLookAtOrientationBlendValue; }
	const Vector3&	GetCachedSourceLookAtPosition() const			{ return m_CachedSourceLookAtPosition; }

	void			ApplyCollisionThisUpdate()						{ m_ShouldApplyCollisionThisUpdate = true; }
	void			ApplyFrameShakeThisUpdate()						{ m_ShouldApplyFrameShakeThisUpdate = true; }

	virtual const camFrame& Update(const camBaseCamera& destinationCamera, u32 timeInMilliseconds = fwTimer::GetCamTimeInMilliseconds());

#if __BANK
	static void		AddWidgets(bkBank& bank);
#endif // __BANK

private:
	virtual void	CacheSourceCameraRelativeToDestination(const camBaseCamera& sourceCamera, const camFrame& destinationFrame,
						const camBaseCamera* destinationCamera);
	void			CacheSourceCameraRelativeToDestinationInternal(const camThirdPersonCamera& sourceCamera,
						const camThirdPersonCamera& destinationCamera);
	virtual bool	UpdateBlendLevel(const camFrame& sourceFrame, const camFrame& destinationFrame, u32 timeInMilliseconds,
						const camBaseCamera* destinationCamera);
	virtual void	InterpolatePosition(float blendLevel, const camFrame& sourceFrame, const camFrame& destinationFrame,
						const camBaseCamera* destinationCamera, Vector3& resultPosition);
	void			UpdateCollision(const camThirdPersonCamera& destinationThirdPersonCamera, const Vector3& sourceCollisionRootPosition,
						float blendLevel, Vector3& cameraPosition);
	virtual void	InterpolateOrientation(float blendLevel, const camFrame& sourceFrame, const camFrame& destinationFrame,
						const camBaseCamera* destinationCamera, Matrix34& resultWorldMatrix);
	void			UpdateFrameShake(const camBaseCamera& destinationCamera);

	Quaternion		m_PreviousOrientationDeltaForInterpolatedBaseFront;
	Quaternion		m_PreviousOrientationDeltaForInterpolatedOrbitFront;
	Vector3			m_CachedSourceBasePivotPosition;
	Vector3			m_CachedSourceBaseFront;
	Vector3			m_CachedSourcePivotPosition;
	Vector3			m_CachedSourcePreCollisionCameraPosition;
	Vector3			m_CachedSourceCollisionRootPosition;
	Vector3			m_CachedSourceLookAtPosition;
	Vector3			m_InterpolatedBasePivotPosition;
	Vector3			m_InterpolatedBaseFront;
	Vector3			m_InterpolatedOrbitFront;
	Vector3			m_InterpolatedPivotPosition;
	Vector3			m_InterpolatedPreCollisionCameraPosition;
	Vector3			m_InterpolatedCollisionRootPosition;
	float			m_CachedSourcePreToPostCollisionLookAtOrientationBlendValue;
	float			m_InterpolatedPreToPostCollisionLookAtOrientationBlendValue;
	bool			m_IsCachedSourceBuoyant								: 1;
	bool			m_AreSourceParametersCachedRelativeToDestination	: 1;
	bool			m_ShouldApplyCollisionThisUpdate					: 1;
	bool			m_ShouldApplyFrameShakeThisUpdate					: 1;
	bool			m_HasClonedSourceCollisionDamping					: 1;

#if __BANK
	static bool		ms_ShouldDebugRender;
#endif // __BANK
};

#endif // THIRD_PERSON_FRAME_INTERPOLATOR_H
