//
// camera/replay/ReplayPresetCamera.h
// 
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved.
//

#ifndef REPLAY_PRESET_CAMERA_H
#define REPLAY_PRESET_CAMERA_H

#include "camera/replay/ReplayBaseCamera.h"

#if GTA_REPLAY

#include "camera/helpers/DampedSpring.h"
#include "scene/RegdRefTypes.h"

class CControl;
class camReplayPresetCameraMetadata;

class camReplayPresetCamera : public camReplayBaseCamera
{
	DECLARE_RTTI_DERIVED_CLASS(camReplayPresetCamera, camReplayBaseCamera);

public:
	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.

	void			GetMinMaxFov(float& minFov, float& maxFov) const;
	void			GetDefaultFov(float& defaultFov) const;

#if __BANK
	virtual void	DebugRender();
#endif // __BANK

protected:
	camReplayPresetCamera(const camBaseObjectMetadata& metadata);

	virtual bool	Init(const s32 markerIndex, const bool shouldForceDefaultFrame = false);
	virtual void	GetDefaultFrame(camFrame& frame) const;
	virtual bool	Update();

	void			UpdateTrackingEntityMatrix();
	void			ComputeLookAtAndDefaultAttachPositions(Vector3& lookAtPosition, Vector3& defaultAttachPosition) const;
	float			GetBoundRadius(const CEntity* entity) const;
	void			ResetCamera();
	void			UpdateControl(const CControl& control, float& desiredDistanceOffset, float& desiredRollOffset, float& desiredZoom);
	void			UpdateDistanceControl(const CControl& control, float& desiredDistanceOffset);
	void			ComputeDistanceInput(const CControl& control, float& distanceInput) const;
	void			UpdateRollControl(const CControl& control, float& desiredRollOffset);
	void			ComputeRollInput(const CControl& control, float& rollInput) const;
	void			UpdateZoomControl(const CControl& control, float& desiredZoom);
	void			ComputeZoomInput(const CControl& control, float& zoomInput) const;
	bool			GetShouldResetCamera(const CControl& control) const;
	void			UpdateZoom(const float desiredZoom);
	void			UpdateRotation(const float desiredRollOffset);
	void			UpdatePosition(const float desiredDistanceOffset, Vector3& preCollisionPosition);
	bool			UpdateCollision(Vector3& cameraPosition);
	void			UpdateCollisionOrigin(float& collisionTestRadius);
	void			UpdateCollisionRootPosition(float& collisionTestRadius);
	const CEntity*	UpdateTrackingEntity();

	virtual void	SaveMarkerCameraInfo(sReplayMarkerInfo& markerInfo, const camFrame& camFrame) const;
	virtual void	LoadMarkerCameraInfo(sReplayMarkerInfo& markerInfo, camFrame& camFrame);
	
	static void		ComputeLookAtOrientation(Matrix34& rMatrix, const Matrix34& entityMatrix, const Vector3& lookAtPosition, const Vector3& attachPosition, const float roll);

	const camReplayPresetCameraMetadata& m_Metadata;
	camDampedSpring	m_CollisionTestRadiusSpring;
	Matrix34		m_TrackingEntityMatrix;
	Vector3			m_CollisionOrigin;
	Vector3			m_CollisionRootPosition;
	RegdConstEnt	m_TrackingEntity;
	float			m_DesiredDistanceScale;
	float			m_DefaultDistanceScalar;
	float			m_CachedDistanceInputSign;
	float			m_CachedRollRotationInputSign;
	float			m_DistanceSpeed;
	float			m_RollSpeed;
	float			m_ZoomSpeed;
	float			m_SmoothedWaterHeight;
	float			m_RelativeRoll;
	bool			m_TrackingEntityIsInVehicle;

private:
	//Forbid copy construction and assignment.
	camReplayPresetCamera(const camReplayPresetCamera& other);
	camReplayPresetCamera& operator=(const camReplayPresetCamera& other);

};

#endif // GTA_REPLAY

#endif // REPLAY_PRESET_CAMERA_H
