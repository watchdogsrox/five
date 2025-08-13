//
// camera/scripted/CustomTimedSplineCamera.h
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#ifndef CUSTOM_TIMED_SPLINE_CAMERA_H
#define CUSTOM_TIMED_SPLINE_CAMERA_H

#include "camera/scripted/TimedSplineCamera.h"

class camCustomTimedSplineCameraMetadata;

struct SplineVelControl
{
	float	start;				// Position for speed change. Format: node.phase (i.e. 1.33f == 33% between nodes 1 and 2)
	float	speedAtStart;		// Speed to use when the specified node and phase are reached.
};

struct SplineBlurControl
{
	float	start;				// Position for blur change. Format: node.phase (i.e. 1.33f == 33% between nodes 1 and 2)
	float	motionBlurStrength;	// Motion blur strength for spline camera, if zero... motion blur set on scripted cameras (for nodes) is used.
};

#define MAX_NODES			16
#define MAX_VEL_CHANGES		16
#define MAX_BLUR_CHANGES	16

class camCustomTimedSplineCamera : public camTimedSplineCamera
{
	DECLARE_RTTI_DERIVED_CLASS(camCustomTimedSplineCamera, camTimedSplineCamera);

public:
	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.

private:
	camCustomTimedSplineCamera(const camBaseObjectMetadata& metadata);
	const camCustomTimedSplineCameraMetadata& m_Metadata;

public:

	virtual void  SetPhase(float progress);
	virtual float GetPhase(s32* time = NULL) const;

	void OverrideVelocity(u32 Entry, float SpeedStart, float Speed);
	void OverrideMotionBlur(u32 Entry, float BlurStart, float MotionBlur);

#if __BANK
	static void	AddWidgets(bkBank &bank);
#endif // __BANK

private:
	virtual bool Update();

	virtual void AddNodeInternal(camSplineNode* node, u32 transitionTime); 
	virtual void ValidateSplineDuration(); 
	//NOTE: The transition deltas are fixed times, so we need not update this.
	virtual void UpdateTransitionDeltas() {}
	virtual void BuildTranslationSpline();
	void ConstrainTranslationVelocities();

	virtual Quaternion	ComputeOrientation(u32 nodeIndex, float nodeTime);

	float ComputePhaseNodeIndexAndNodePhase(u32& nodeIndex, float& nodePhase);

private:
	//Forbid copy construction and assignment.
	camCustomTimedSplineCamera(const camCustomTimedSplineCamera& other);
	camCustomTimedSplineCamera& operator=(const camCustomTimedSplineCamera& other);

#if __BANK && 0
	void FixTangentsForSmoothAcceleration();
#endif // __BANK
	void FixupSplineTimes();

	void UpdateOverridenMotionBlur();

	SplineVelControl m_aoVelControl[MAX_VEL_CHANGES];
	SplineBlurControl m_aoBlurControl[MAX_BLUR_CHANGES];

	float	m_NodeDistance[MAX_NODES];
	float	m_ElapsedDistance;
	float	m_TotalDistance;
	float	m_SplineSpeed;
	float	m_TotalPhase;

	float	m_PreviousAcceleration;
	u32		m_PreviousNodeIndex;
	u32		m_PreviousDistCalcNode;
	u32		m_PreviousSpeedCalcNode;
	u32		m_TimeBalanceCalcNode;

	bool	m_SplineTimesAdjusted;
	bool	m_bConstantVelocity;
	bool	m_bOverrideAcceleration;
	bool	m_bFixupZeroNodeTangent;
	bool	m_bFixupSplineNodeDuration;

	u8		m_CurrentVelChangeIndex;
	u8		m_CurrentBlurChangeIndex;

#if __BANK
	static bool	ms_bConstantVelocity;
	static bool	ms_bOverrideAcceleration;
	static bool	ms_bFixupSplineNodeDuration;
#endif // __BANK

};

#endif // CUSTOM_TIMED_SPLINE_CAMERA_H
