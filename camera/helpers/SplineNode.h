//
// camera/helpers/SplineNode.h
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#ifndef CAM_SPLINE_NODE_H
#define CAM_SPLINE_NODE_H

#include "vector/quaternion.h"

#include "camera/base/BaseCamera.h"
#include "camera/base/BaseDirector.h"

class camSplineNode
{
public:
	//NOTE: This enumeration must match CAM_SPLINE_NODE_FLAGS in commands_camera.sch.
	enum eFlags
	{
		SHOULD_SMOOTH_ROTATION		= 1,
		SHOULD_SMOOTH_LENS_PARAMS	= 2,
		SHOULD_EASE_IN				= 4,
		SHOULD_EASE_OUT				= 8,
		SHOULD_EASE_IN_OUT			= 16,
	};

	//NOTE: This enumeration must match CAM_SPLINE_NODE_FORCE_FLAGS in commands_camera.sch.
	enum eExtraFlags
	{
		FORCE_LINEAR				= 1,
		FORCE_CUT					= 2,
		FORCE_PAUSE					= 4,
		FORCE_LEVEL					= 8,
	};

	FW_REGISTER_CLASS_POOL(camSplineNode);

	camSplineNode(const Vector3& position, eFlags flags);
	camSplineNode(const camBaseCamera* camera, eFlags flags);
	camSplineNode(const camBaseDirector& director, eFlags flags);
	camSplineNode(const camFrame& frame, eFlags flags);
	~camSplineNode() {}

	const Vector3&	GetPosition() const									{ return m_Position; }
	float			GetTransitionDelta() const							{ return m_TransitionDelta; } //could be a velocity or a time
	const Vector3&	GetTranslationVelocity() const						{ return m_TranslationVelocity; }
	const Quaternion& GetOrientation() const							{ return m_Orientation; }
	bool			IsFrameValid() const								{ return (m_Camera || m_Director || m_IsLocalFrameValid); }
	const camBaseCamera* GetCamera() const								{ return m_Camera; }
	const camBaseDirector* GetDirector() const							{ return m_Director; }
	const camFrame&	GetFrame() const									{ return m_Camera ? m_Camera->GetFrame() : (m_Director ? m_Director->GetFrame() : m_Frame); }
	bool			ShouldSmoothOrientation() const						{ return (m_Flags & SHOULD_SMOOTH_ROTATION) != 0; }
	bool 			ShouldSmoothLensParams() const						{ return (m_Flags & SHOULD_SMOOTH_LENS_PARAMS) != 0; }

	bool			ShouldEaseIn() const								{ return (m_Flags & SHOULD_EASE_IN) != 0; }
	bool			ShouldEaseOut() const								{ return (m_Flags & SHOULD_EASE_OUT) != 0; }
	bool			ShouldEaseInOut() const								{ return (m_Flags & SHOULD_EASE_IN_OUT) != 0; }
	void			SetEaseFlag(eFlags easeFlags);
	float			GetEaseScale() const								{ return m_EaseScale; }
	void			SetEaseScale(float scale)							{ m_EaseScale = scale; }

	void			ForceLinearBlend(bool enable)						{ m_IsForceLinear = enable; }
	bool			IsForceLinear() const								{ return m_IsForceLinear; }
	void			ForceCut(bool enable)								{ m_IsForceCut = enable; }
	bool			IsForceCut() const									{ return m_IsForceCut; }
	void			ForcePause(bool enable)								{ m_IsForcePause = enable; }
	bool			IsForcePause() const								{ return m_IsForcePause; }
	void			ForceLevel(bool enable)								{ m_IsForceLevel = enable; }
	bool			IsForceLevel() const								{ return m_IsForceLevel; }
#if __BANK
	void			SaveAndClearForceLinear(bool& temp)					{ temp = m_IsForceLinear; m_IsForceLinear = false; }
	void			RestoreForceLinear(bool savedValue)					{ m_IsForceLinear = savedValue; }
#endif // __BANK

	void			SetPosition(const Vector3& position)				{ m_Position = position; }
	void			SetTransitionDelta(float distance)					{ m_TransitionDelta = distance; } //could be a velocity or a time
	void			SetTranslationVelocity(const Vector3& velocity)		{ m_TranslationVelocity = velocity; }
	void			SetLookAtPosition(const Vector3& position)			{ m_LookAtPosition = position; UpdateOrientation(); }
	void			SetOrientationEulers(const Vector3& orientationEulers, int rotationOrder)
	{
		m_OrientationEulers = orientationEulers;
		m_RotationOrder = rotationOrder;
		UpdateOrientation();
	}

	void			SetTangentInterp(float num)							{ m_TangentInterp = num; }
	float			GetTangentInterp() const							{ return m_TangentInterp; }

	void			SetOrientation(const Quaternion& orientation)		{ m_Orientation = orientation; }
	void			SetCamera(const camBaseCamera* camera)				{ m_Camera = camera; }
	void			SetDirector(const camBaseDirector& director)		{ m_Director = &director; }
	void			SetLocalFrame(const camFrame& frame)				{ m_Frame = frame; }
	void			SetFrameMatrix(const Matrix34& mat, bool bSet3x3)
	{
		if (m_Camera)
			const_cast<camBaseCamera*>(m_Camera.Get())->GetFrameNonConst().SetWorldMatrix(mat, bSet3x3);
		else if (m_IsLocalFrameValid)
			m_Frame.SetWorldMatrix(mat, bSet3x3);
	}
	void			SetFlags(eFlags flags)								{ m_Flags = flags; }

	bool			UpdatePosition();
	bool			UpdateOrientation();

private:
	void			ComputeLookAtMatrix(Matrix34& matrix) const;

	camFrame		m_Frame;
	Quaternion		m_Orientation;
	Vector3			m_OrientationEulers;
	Vector3			m_Position;
	Vector3			m_LookAtPosition;
	Vector3			m_TranslationVelocity;
	RegdConstCamBaseCamera m_Camera;
	RegdConstCamBaseDirector m_Director;
	eFlags			m_Flags;
	int				m_RotationOrder;
	float			m_TransitionDelta;
	float			m_TangentInterp;
	float			m_EaseScale;
	bool			m_IsLocalFrameValid;
	bool			m_IsForceLinear;
	bool			m_IsForceCut;
	bool			m_IsForcePause;
	bool			m_IsForceLevel;
};

#endif // CAM_SPLINE_NODE_H
