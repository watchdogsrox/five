//
// camera/helpers/HintHelper.h
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#ifndef HINT_HELPER_H
#define HINT_HELPER_H

#include "vector/vector3.h"

#include "camera/base/BaseObject.h"

class camHintHelperMetadata; 
class camLookAtDampingHelper; 

class camHintHelper : public camBaseObject
{
	DECLARE_RTTI_DERIVED_CLASS(camHintHelper, camBaseObject);

public:
	FW_REGISTER_CLASS_POOL(camHintHelper);

	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.
	
	~camHintHelper();
private:
	camHintHelper(const camBaseObjectMetadata& metadata);

public:
	void ComputeHintOrientation(const Vector3& basePivotPosition, const Vector2& orbitPitchLimits, float lookAroundInputEnvelopeLevel,
		float& orbitHeading, float& orbitPitch, float Fov, float orbitPitchOffset, const Vector3& orbitRelativePivotOffset); 

	void ComputeOrbitPitchOffset(float& orbitPitchOffset) const; 

	void ComputeHintZoomScalar(float &FovScalar) const; 
	
	void ComputeHintFollowDistanceScalar(float &DistanceScale) const; 

	void ComputeHintPivotOverBoundingBoxBlendLevel(float& pivotOverBlendLevel) const;

	void ComputeHintPivotPositionAdditiveOffset(float& SideOffset, float& VerticalOffset) const; 

	void ComputeHintParentSizeScalingAdditiveOffset(float& SideOffset, float& VerticalOffset) const; 
	
	void ComputeDesiredHeadingAndPitch(float &DesiredHeading, float &DesiredPitch, float Fov, const Vector3& desiredOrbitDelta, float blendLevel, const Vector2& orbitPitchLimits, float orbitPitchOffset, const Vector3& orbitRelativePivotOffset, const Vector3& basePivotPosition); 
private:
// 	float ComputeErrorRatioToApplyThisUpdate(float blendLevel, float blendLevelOnPreviousUpdate) const;

private:
	const camHintHelperMetadata& m_Metadata;
	
	camLookAtDampingHelper* m_LookAtDampingHelper; 

	float m_HintBlendLevelForOrientationOnPreviousUpdate;
	float m_CachedOrbitHeading; 
	float m_CachedOrbitPitch; 

	bool m_HaveIntialisedHintHelper : 1;
	bool m_HaveTerminatedForPitchLimits : 1; 

private:
	//Forbid copy construction and assignment.
	camHintHelper(const camHintHelper& other);
	camHintHelper& operator=(const camHintHelper& other);
};

#endif // HINT_HELPER_H
