//
// camera/gameplay/aim/ThirdPersonVehicleAimCamera.h
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#ifndef THIRD_PERSON_VEHICLE_AIM_CAMERA_H
#define THIRD_PERSON_VEHICLE_AIM_CAMERA_H

#include "camera/gameplay/aim/ThirdPersonAimCamera.h"

class camThirdPersonVehicleAimCameraMetadata;

class camThirdPersonVehicleAimCamera : public camThirdPersonAimCamera
{
	DECLARE_RTTI_DERIVED_CLASS(camThirdPersonVehicleAimCamera, camThirdPersonAimCamera);

public:
	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.

private:
	camThirdPersonVehicleAimCamera(const camBaseObjectMetadata& metadata);

private:
	virtual bool	Update();
	virtual void	ComputeOrbitRelativePivotOffsetInternal(Vector3& orbitRelativeOffset) const;
	virtual bool	ComputeShouldUseLockOnAiming() const;
	//Bypass the weapon-specific FOV and simply use the value specified in metadata.
	virtual float	ComputeBaseFov() const	{ return camThirdPersonCamera::ComputeBaseFov(); }
	virtual float	ComputeDesiredFov()		{ return camThirdPersonCamera::ComputeDesiredFov(); }

	const camThirdPersonVehicleAimCameraMetadata& m_Metadata;

	//Forbid copy construction and assignment.
	camThirdPersonVehicleAimCamera(const camThirdPersonVehicleAimCamera& other);
	camThirdPersonVehicleAimCamera& operator=(const camThirdPersonVehicleAimCamera& other);
};

#endif // THIRD_PERSON_VEHICLE_AIM_CAMERA_H
