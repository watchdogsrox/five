//
// camera/helpers/FrameInterpolator.h
// 
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved.
//

#ifndef FRAME_INTERPOLATOR_H
#define FRAME_INTERPOLATOR_H

#include "data/base.h"

#include "fwsys/timer.h"
#include "fwutil/rtti.h"

#include "camera/helpers/Frame.h"
#include "scene/RegdRefTypes.h"

const u32 g_DefaultInterpolationDuration = 2000;

class camBaseCamera;

class camFrameInterpolator: public datBase	// Necessary to avoid weird 2012 codegen bug and weird data packing on earlier compilers
{
	DECLARE_RTTI_BASE_CLASS(camFrameInterpolator);

public:
	FW_REGISTER_CLASS_POOL(camFrameInterpolator);

	enum eCurveTypes
	{
		LINEAR,
		SIN_ACCEL_DECEL,
		ACCELERATION,
		DECELERATION,
		SLOW_IN,
		SLOW_OUT,
		SLOW_IN_OUT,
		VERY_SLOW_IN,
		VERY_SLOW_OUT,
		VERY_SLOW_IN_SLOW_OUT,
		SLOW_IN_VERY_SLOW_OUT,
		VERY_SLOW_IN_VERY_SLOW_OUT,
		EASE_IN,
		EASE_OUT,
		QUADRATIC_EASE_IN,
		QUADRATIC_EASE_OUT,
		QUADRATIC_EASE_IN_OUT,
		CUBIC_EASE_IN,
		CUBIC_EASE_OUT,
		CUBIC_EASE_IN_OUT,
		QUARTIC_EASE_IN,
		QUARTIC_EASE_OUT,
		QUARTIC_EASE_IN_OUT,
		QUINTIC_EASE_IN,
		QUINTIC_EASE_OUT,
		QUINTIC_EASE_IN_OUT,  
		CIRCULAR_EASE_IN,
		CIRCULAR_EASE_OUT,
		CIRCULAR_EASE_IN_OUT,
		NUM_CURVE_TYPES
	};

	camFrameInterpolator(camBaseCamera& sourceCamera, const camBaseCamera& destinationCamera, u32 duration = g_DefaultInterpolationDuration,
		bool shouldDeleteSourceCamera = false, u32 timeInMilliseconds = fwTimer::GetCamTimeInMilliseconds());
	camFrameInterpolator(const camFrame& sourceFrame, u32 duration = g_DefaultInterpolationDuration,
		u32 timeInMilliseconds = fwTimer::GetCamTimeInMilliseconds());
	virtual ~camFrameInterpolator();

	bool					IsInterpolating() const							{ return m_IsActive; }
	virtual bool			HasCachedSourceRelativeToDestination() const	{ return m_IsSourceFrameCachedRelativeToDestination; }
	float					GetBlendLevel() const							{ return m_BlendLevel; }
	u32						GetDuration() const								{ return m_Duration; }
	const camFrame&			GetFrame()	const								{ return m_ResultFrame; }
	const camBaseCamera*	GetSourceCamera() const							{ return m_SourceCamera; }

	void	SetSourceCamera(camBaseCamera* camera)				{ m_SourceCamera = camera; }
	void	SetSourceFrame(const camFrame& frame)				{ m_CachedSourceFrame.CloneFrom(frame); m_IsSourceFrameCachedRelativeToDestination = false; }
	void	SetCurveTypeForFrameComponent(camFrame::eComponentType frameComponent, eCurveTypes curveType);
	void	SetMaxDistanceToTravel(float distance)				{ m_MaxDistanceToTravel = distance; }
	void	SetMaxOrientationDelta(float orientationDelta)		{ m_MaxOrientationDelta = orientationDelta; }
	void	SetExtraMotionBlurStrength(float strength)			{ m_ExtraMotionBlurStrength = strength; }

	virtual const camFrame&	Update(const camBaseCamera& destinationCamera, u32 timeInMilliseconds = fwTimer::GetCamTimeInMilliseconds());
	const camFrame&			Update(const camFrame& destinationFrame, u32 timeInMilliseconds = fwTimer::GetCamTimeInMilliseconds(),
								const camBaseCamera* destinationCamera = NULL);

protected:
	virtual void	CacheSourceCameraRelativeToDestination(const camBaseCamera& sourceCamera, const camFrame& destinationFrame,
						const camBaseCamera* UNUSED_PARAM(destinationCamera))
						{ CacheSourceCameraRelativeToDestinationInternal(sourceCamera, destinationFrame); }
	void			CacheSourceCameraRelativeToDestinationInternal(const camBaseCamera& sourceCamera, const camFrame& destinationFrame);
	virtual bool	UpdateBlendLevel(const camFrame& sourceFrame, const camFrame& destinationFrame, u32 timeInMilliseconds,
						const camBaseCamera* destinationCamera);
	virtual void	InterpolateFrames(const camFrame& sourceFrame, const camFrame& destinationFrame, const camBaseCamera* destinationCamera);
	void			InterpolatePosition(const camFrame& sourceFrame, const camFrame& destinationFrame, const camBaseCamera* destinationCamera);
	virtual void	InterpolatePosition(float blendLevel, const camFrame& sourceFrame, const camFrame& destinationFrame,
						const camBaseCamera* destinationCamera, Vector3& resultPosition);
	void			InterpolateOrientation(const camFrame& sourceFrame, const camFrame& destinationFrame, const camBaseCamera* destinationCamera);
	virtual void	InterpolateOrientation(float blendLevel, const camFrame& sourceFrame, const camFrame& destinationFrame,
						const camBaseCamera* destinationCamera, Matrix34& resultWorldMatrix);
	void			InterpolateFov(const camFrame& sourceFrame, const camFrame& destinationFrame);
	void			InterpolateNearClip(const camFrame& sourceFrame, const camFrame& destinationFrame);
	void			InterpolateFarClip(const camFrame& sourceFrame, const camFrame& destinationFrame);
	void			InterpolateCameraTimeScale(const camFrame& sourceFrame, const camFrame& destinationFrame);
	void			InterpolateDofSettings(const camFrame& sourceFrame, const camFrame& destinationFrame);
	void			InterpolateMotionBlurStrength(const camFrame& sourceFrame, const camFrame& destinationFrame);
	void			InterpolateFlags(const camFrame& sourceFrame, const camFrame& destinationFrame);
	float			ApplyCurve(eCurveTypes type, float value);
	void			Finish();

	u32				m_StartTime;
	u32				m_Duration;	
	float			m_BlendLevel;
	camFrame		m_CachedSourceFrame;
	camFrame		m_ResultFrame;

	eCurveTypes		m_ComponentCurveTypes[camFrame::CAM_FRAME_MAX_COMPONENTS];

	RegdCamBaseCamera m_SourceCamera;
	float			m_MaxDistanceToTravel;
	float			m_MaxOrientationDelta;
	float			m_ExtraMotionBlurStrength;
	bool			m_ShouldDeleteSourceCamera					: 1;
	bool			m_IsActive									: 1;
	bool			m_IsSourceFrameCachedRelativeToDestination	: 1;
};

#endif // FRAME_INTERPOLATOR_H
