//
// camera/helpers/CatchUpHelper.h
// 
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved.
//

#ifndef CATCH_UP_HELPER_H
#define CATCH_UP_HELPER_H

#include "camera/base/BaseObject.h"
#include "camera/helpers/Frame.h"

class camCatchUpHelperMetadata; 
class camThirdPersonCamera;

class camCatchUpHelper : public camBaseObject
{
	DECLARE_RTTI_DERIVED_CLASS(camCatchUpHelper, camBaseObject);

public:
	FW_REGISTER_CLASS_POOL(camCatchUpHelper);

	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.

private:
	camCatchUpHelper(const camBaseObjectMetadata& metadata);

public:
	enum eBlendType
	{
		NONE = 0,
		SLOW_IN,
		SLOW_OUT,
		SLOW_IN_OUT,
		NUM_BLEND_TYPES
	};

	float		GetBlendLevel() const { return m_BlendLevel; }

	void		Init(const camFrame& sourceFrame, float maxDistanceToBlend = 0.0f, int blendType = (int)SLOW_IN_OUT);
	bool		Update(const camThirdPersonCamera& thirdPersonCamera, float maxOrbitDistance, bool hasCut, const Vector3& baseAttachVelocity);
	void		ComputeDesiredOrbitDistance(const camThirdPersonCamera& thirdPersonCamera, float& desiredOrbitDistance) const;
	bool		ComputeOrbitOrientation(const camThirdPersonCamera& thirdPersonCamera, float& orbitHeading, float& orbitPitch) const;
	void		ComputeOrbitRelativePivotOffset(const camThirdPersonCamera& thirdPersonCamera, Vector3& orbitRelativeOffset);
	void		UpdateCameraPositionDelta(const Vector3& orbitFront, Vector3& cameraPosition);
	bool		UpdateLookAtOrientationDelta(const Vector3& basePivotPosition, const Vector3& orbitFront, const Vector3& cameraPosition,
					Vector3& lookAtFront);
	void		ComputeDesiredRoll(float& desiredRoll) const;
	void		ComputeLensParameters(camFrame& destinationFrame) const;
	void		PostUpdate() { m_ShouldInitBehaviour = false; }
	void		ComputeDofParameters(camFrame& destinationFrame) const;

	bool		WillFinishThisUpdate() const;

private:
	float		ApplyCustomBlendCurve(float sourceLevel, s32 curveType) const;
	bool		ComputeIsCatchUpSourceFrameValid(const camThirdPersonCamera& thirdPersonCamera) const;
	float		ComputeDistanceAlongCatchUpSourceFrontToBasePivotPlane(const camThirdPersonCamera& thirdPersonCamera) const;
	void		ComputeDesiredOrbitOrientation(const camThirdPersonCamera& thirdPersonCamera, float& orbitHeading, float& orbitPitch) const;

	const camCatchUpHelperMetadata& m_Metadata;

	camFrame	m_SourceFrame;
	Vector3		m_OrbitRelativePivotOffset;
	Vector3		m_OrbitRelativeCameraPositionDelta;
	Vector3		m_RelativeLookAtPositionDelta;
	u32			m_StartTime;
	float		m_MaxDistanceToBlend;
	float		m_DistanceTravelled;
	float		m_BlendLevel;
	eBlendType	m_BlendType;
	bool		m_ShouldInitBehaviour;

	//Forbid copy construction and assignment.
	camCatchUpHelper(const camCatchUpHelper& other);
	camCatchUpHelper& operator=(const camCatchUpHelper& other);
};

#endif // CATCH_UP_HELPER_H
