//
// camera/helpers/SplineNode.cpp
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#include "camera/helpers/SplineNode.h"

#include "math/angmath.h"
#include "math/random.h"

#include "camera/camera_channel.h"
#include "script/script.h"

CAMERA_OPTIMISATIONS()

camSplineNode::camSplineNode(const Vector3& position, eFlags flags)
: m_Orientation(Quaternion::sm_I)
, m_OrientationEulers(VEC3_ZERO)
, m_Position(position)
, m_LookAtPosition(VEC3_ZERO)
, m_TranslationVelocity(VEC3_ZERO)
, m_Camera(NULL)
, m_Director(NULL)
, m_Flags(flags)
, m_RotationOrder(EULER_YXZ)
, m_TransitionDelta(0.0f)
, m_TangentInterp(0.0f)
, m_EaseScale(1.0f)
, m_IsLocalFrameValid(false)
, m_IsForceLinear(false)
, m_IsForceCut(false)
, m_IsForcePause(false)
{
}

camSplineNode::camSplineNode(const camBaseCamera* camera, eFlags flags)
: m_Orientation(Quaternion::sm_I)
, m_OrientationEulers(VEC3_ZERO)
, m_Position(VEC3_ZERO)
, m_LookAtPosition(VEC3_ZERO)
, m_TranslationVelocity(VEC3_ZERO)
, m_Camera(camera)
, m_Director(NULL)
, m_Flags(flags)
, m_RotationOrder(EULER_YXZ)
, m_TransitionDelta(0.0f)
, m_TangentInterp(0.0f)
, m_EaseScale(1.0f)
, m_IsLocalFrameValid(false)
, m_IsForceLinear(false)
, m_IsForceCut(false)
, m_IsForcePause(false)
{
	UpdatePosition();
	UpdateOrientation();
}

camSplineNode::camSplineNode(const camBaseDirector& director, eFlags flags)
: m_Orientation(Quaternion::sm_I)
, m_OrientationEulers(VEC3_ZERO)
, m_Position(VEC3_ZERO)
, m_LookAtPosition(VEC3_ZERO)
, m_TranslationVelocity(VEC3_ZERO)
, m_Camera(NULL)
, m_Director(&director)
, m_Flags(flags)
, m_RotationOrder(EULER_YXZ)
, m_TransitionDelta(0.0f)
, m_TangentInterp(0.0f)
, m_EaseScale(1.0f)
, m_IsLocalFrameValid(false)
, m_IsForceLinear(false)
, m_IsForceCut(false)
, m_IsForcePause(false)
{
	UpdatePosition();
	UpdateOrientation();
}

camSplineNode::camSplineNode(const camFrame& frame, eFlags flags)
: m_Frame(frame)
, m_Orientation(Quaternion::sm_I)
, m_OrientationEulers(VEC3_ZERO)
, m_Position(VEC3_ZERO)
, m_LookAtPosition(VEC3_ZERO)
, m_TranslationVelocity(VEC3_ZERO)
, m_Camera(NULL)
, m_Director(NULL)
, m_Flags(flags)
, m_RotationOrder(EULER_YXZ)
, m_TransitionDelta(0.0f)
, m_TangentInterp(0.0f)
, m_EaseScale(1.0f)
, m_IsLocalFrameValid(true)
, m_IsForceLinear(false)
, m_IsForceCut(false)
, m_IsForcePause(false)
{
	UpdatePosition();
	UpdateOrientation();
}

bool camSplineNode::UpdatePosition()
{
	bool hasChanged = false;

	if(IsFrameValid())
	{
		const camFrame& frame = GetFrame();
		const Vector3& position = frame.GetPosition();
		if(m_Position != position)
		{
			m_Position = position;
			hasChanged = true;
		}
	}

	return hasChanged;
}

bool camSplineNode::UpdateOrientation()
{
	bool hasChanged = false;

	Matrix34 matrix;
	ComputeLookAtMatrix(matrix);
	Quaternion orientation;
	matrix.ToQuaternion(orientation);

	Quaternion negatedOrientation;
	negatedOrientation.Negate(orientation);

	//NOTE: We need to test the new orientation against the existing orientation AND the negation of the existing orientation, as selected negation
	//of orientation is applied to the spline nodes in order to enforce shortest angle interpolation.
	if(!m_Orientation.IsEqual(orientation) && !m_Orientation.IsEqual(negatedOrientation))
	{
		m_Orientation = orientation;
		hasChanged = true;
	}

	return hasChanged;
}

void camSplineNode::ComputeLookAtMatrix(Matrix34& matrix) const
{
	if(m_LookAtPosition.IsNonZero())
	{
		const Vector3 front = m_LookAtPosition - m_Position;
		if(cameraVerifyf(front.IsNonZero(), "A spline camera cannot be made to look at itself"))
		{
			camFrame frame;
			frame.SetWorldMatrixFromFront(front);
			matrix = frame.GetWorldMatrix();
		}
	}
	else if(IsFrameValid())
	{
		const camFrame& frame = GetFrame();
		matrix = frame.GetWorldMatrix();
	}
	else //Use the specified orientation Eulers.
	{
		CScriptEulers::MatrixFromEulers(matrix, m_OrientationEulers, static_cast<EulerAngleOrder>(m_RotationOrder));
	}
}

void camSplineNode::SetEaseFlag(eFlags easeFlags)
{
	AssertMsg(	easeFlags == SHOULD_EASE_IN ||
				easeFlags == SHOULD_EASE_OUT ||
				easeFlags == SHOULD_EASE_IN_OUT, "Invalid flag set" );

	// Clear existing ease flags, in case they were set to another value previously.
	u32 currentFlags = (u32)m_Flags;
	u32 flagsToAdd   = (u32)easeFlags;
	currentFlags &= ~(SHOULD_EASE_IN+SHOULD_EASE_OUT+SHOULD_EASE_IN_OUT);

	flagsToAdd   &= SHOULD_EASE_IN+SHOULD_EASE_OUT+SHOULD_EASE_IN_OUT;
	currentFlags |= flagsToAdd;

	m_Flags = (eFlags)currentFlags;
}
