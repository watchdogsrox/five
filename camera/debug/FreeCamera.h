//
// camera/debug/FreeCamera.h
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#ifndef FREE_CAMERA_H
#define FREE_CAMERA_H

#if !__FINAL //No debug cameras in final builds.

#include "camera/base/BaseCamera.h"

class camFreeCameraMetadata;

#if __BANK
namespace rage
{
	class bkData;
}
#endif

//A debug camera that allows free movement via control input.
class camFreeCamera : public camBaseCamera
{
	DECLARE_RTTI_DERIVED_CLASS(camFreeCamera, camBaseCamera);

public:
	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.

private:
	camFreeCamera(const camBaseObjectMetadata& metadata);

public:
#if __BANK
	~camFreeCamera(); 
#endif

	virtual void SetFrame(const camFrame& frame);

	void	SetMouseEnableState(bool shouldEnable)	{ m_IsMouseEnabled = shouldEnable;	}

	void	Reset();

	void	ReEnableAllControls()					{ SetAllControlsEnabled(true); }
	void	DisableAllControls()					{ SetAllControlsEnabled(false); }
	void	SetLeftStickEnabled(bool enable)		{ m_LeftStickEnabled = enable; }
	void	SetRightStickEnabled(bool enable)		{ m_RightStickEnabled = enable; }
	void	SetCrossEnabled(bool enable)			{ m_CrossEnabled = enable; }
	void	SetDPadDownEnabled(bool enable)			{ m_DPadDownEnabled = enable; }
	void	SetDPadLeftEnabled(bool enable)			{ m_DPadLeftEnabled = enable; }
	void	SetDPadRightEnabled(bool enable)		{ m_DPadRightEnabled = enable; }
	void	SetDPadUpEnabled(bool enable)			{ m_DPadUpEnabled = enable; }
	void	SetLeftShoulder1Enabled(bool enable)	{ m_LeftShoulder1Enabled = enable; }
	void	SetLeftShoulder2Enabled(bool enable)	{ m_LeftShoulder2Enabled = enable; }
	void	SetLeftShockEnabled(bool enable)		{ m_LeftShockEnabled = enable; }
	void	SetRightShoulder1Enabled(bool enable)	{ m_RightShoulder1Enabled = enable; }
	void	SetRightShoulder2Enabled(bool enable)	{ m_RightShoulder2Enabled = enable; }
	void	SetRightShockEnabled(bool enable)		{ m_RightShockEnabled = enable; }
	void	SetSquareEnabled(bool enable)			{ m_SquareEnabled = enable; }
	void	SetTriangleEnabled(bool enable)			{ m_TriangleEnabled = enable; }
	
	bool	GetUseRdrMapping() const				{ return m_ShouldUseRdrMapping; }
	void 	SetUseRdrMapping(bool b)				{ m_ShouldUseRdrMapping = b; }

	bool	ComputeIsCarryingFollowPed() const;

#if __BANK
	virtual void AddWidgetsForInstance();
#endif // __BANK

private:
	virtual bool Update();
	void	UpdateMouseControl(const float timeStep);
	void	TeleportTarget(const Vector3& position, bool shouldLoadScene = true);
	void	ResetAngles();
	void	ResetSpeeds();
	void	SetAllControlsEnabled(bool enabled);

#if __BANK
	void	OnDebugOrientationChange();
	void	OnDebugMountChange();
	const CEntity* FindNearestPedVehicleOrObjectToPosition(const Vector3& position, float maxDistanceToTest) const;
	void	BankCameraWidgetReceiveDataCallback(); 
#endif // __BANK

	const camFreeCameraMetadata& m_Metadata;

	camFrame m_ClonedFrame;

	float	m_ForwardSpeed;
	float	m_StrafeSpeed;
	float	m_VerticalSpeed;
	float	m_HeadingSpeed;
	float	m_PitchSpeed;
	float	m_RollSpeed;
	float	m_FovSpeed;

	Vector3 m_OrbitPosition;
	float	m_OrbitDistance;
	u32		m_NumUpdatesMouseButtonUp;
	u32		m_NumUpdatesToPushNearClip;
	s32		m_CachedMouseX;
	s32		m_CachedMouseY;
	bool	m_IsMouseEnabled;
	bool	m_IsMouseButtonDown;
	s32		m_MovementSpeed;
	bool	m_OverrideNearClip;

	//Flags that can permit any control to be switched off - for a frame at least. 
	bool	m_LeftStickEnabled;
	bool	m_RightStickEnabled;
	bool	m_CrossEnabled;
	bool	m_DPadDownEnabled;
	bool	m_DPadLeftEnabled;
	bool	m_DPadRightEnabled;
	bool	m_DPadUpEnabled;	
	bool	m_LeftShoulder1Enabled;
	bool	m_LeftShoulder2Enabled;
	bool	m_LeftShockEnabled;
	bool	m_RightShoulder1Enabled;
	bool	m_RightShoulder2Enabled;
	bool	m_RightShockEnabled;
	bool	m_SquareEnabled;
	bool	m_TriangleEnabled;

	bool	m_ShouldUseRdrMapping;
	bool	m_ShouldOverrideStreamingFocus;

#if __BANK
	Vector3	m_DebugOrientation;
	Vector3	m_DebugLightColour;
	Vector3	m_DebugLightPosition;
	Vector3	m_DebugLightDirection;
	Vector3	m_DebugLightTangent;
	float	m_DebugLightRange;
	float	m_DebugLightIntensity;
	float	m_DebugOverridenNearClip;
	bool	m_IsDebugLightActive;
	bool	m_IsDebugLightAttached;
	bool	m_ShouldUseADebugRimLight;
	bool	m_ShouldDebugOverrideNearClip;

	RegdConstEnt m_DebugMountEntity;
	Vector3 m_DebugMountRelativePosition;
	float	m_DebugMountRelativeHeading;
	float	m_DebugMountRelativePitch;
	bool	m_ShouldDebugMountPlayer;
	bool	m_ShouldDebugMountPickerSelection;
	bool	m_ShouldDebugMountNearestPedVehicleOrObject;
	bkData* m_CameraWidgetData;	
#endif

	//Forbid copy construction and assignment.
	camFreeCamera(const camFreeCamera& other);
	camFreeCamera& operator=(const camFreeCamera& other);
};

#endif // !__FINAL

#endif // FREE_CAMERA_H
