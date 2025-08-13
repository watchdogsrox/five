//
// camera/scripted/ScriptedFlyCamera.h
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#ifndef SCRIPTED_FLY_CAMERA_H
#define SCRIPTED_FLY_CAMERA_H

#include "camera/scripted/ScriptedCamera.h"
#include "camera/system/CameraMetadata.h"
#include "physics/WorldProbe/shapetestresults.h"
class CControl;

class camScriptedFlyCamera : public camScriptedCamera
{
	DECLARE_RTTI_DERIVED_CLASS(camScriptedFlyCamera, camScriptedCamera);

public:
	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.

	camScriptedFlyCameraMetadata& GetMetadata() { return m_LocalMetadata; }
	bool			WasConstrained() const		{ return m_WasConstrained; }

	void			SetScriptPosition(const Vector3& vec)	{ m_ScriptPosition = vec; m_ScriptPositionValid = true; }

	void			SetVerticalControlThisUpdate(bool b)	{ m_EnableVerticalControl = b; }

protected:
	camScriptedFlyCamera(const camBaseObjectMetadata& metadata);

	virtual void	PreUpdate();
	virtual bool	Update();

	void			UpdateControl();
	void			UpdateTranslationControl(const CControl& control);
	void			ComputeTranslationInput(const CControl& control, Vector3& translationInput) const;
	void			UpdatePosition();
	bool			UpdateWaterCollision(Vector3& cameraPosition);
	bool			UpdateCollision(const Vector3& initialPosition, Vector3& cameraPosition) const;

	float			ComputeFilteredHitTValue(const WorldProbe::CShapeTestFixedResults<>& shapeTestResults, bool isSphereTest) const;

	//NOTE: We store a local copy of the metadata in order to simplify the method of overriding these settings from script. See GetMetadata().
	camScriptedFlyCameraMetadata m_LocalMetadata;
	
	Vector3			m_DesiredTranslation;
	Vector3			m_ScriptPosition;
	Vector2			m_CachedNormalisedHorizontalTranslationInput;
	
	float			m_CachedVerticalTranslationInputSign;
	float			m_HorizontalSpeed;
	float			m_VerticalSpeed;

	u32				m_PushingAgainstWaterSurfaceTimer;

	bool			m_WasConstrained;
	bool			m_ScriptPositionValid;
	bool			m_EnableVerticalControl;
	bool			m_IsUnderWater;

private:
	//Forbid copy construction and assignment.
	camScriptedFlyCamera(const camScriptedFlyCamera& other);
	camScriptedFlyCamera& operator=(const camScriptedFlyCamera& other);
};

#endif // SCRIPTED_FLY_CAMERA_H
