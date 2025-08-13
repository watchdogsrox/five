//
// camera/replay/ReplayDirector.h
// 
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved.
//

#ifndef REPLAY_DIRECTOR_H
#define REPLAY_DIRECTOR_H

#include "control/replay/ReplayMarkerInfo.h"
#include "control/replay/ReplaySettings.h"
#include "control/replay/ReplaySupportClasses.h"
#include "camera/base/BaseDirector.h"
#include "camera/replay/ReplayFreeCamera.h"
#include "camera/replay/ReplayRecordedCamera.h"

#define MAX_PEDS_TRACKED		32


#if GTA_REPLAY
typedef	fwRegdRef<class camReplayRecordedCamera> RegdCamReplayRecordedCamera;

class camReplayDirectorMetadata;
struct sReplayMarkerInfo;

struct ReplayTargetInfo
{
	CReplayID	TargetID;
	float		TargetDistanceSq;
};
#endif // GTA_REPLAY

class camReplayDirectorMetadata;
class camTimedSplineCamera;

//The director responsible for the replay cameras.
class camReplayDirector : public camBaseDirector
{
	DECLARE_RTTI_DERIVED_CLASS(camReplayDirector, camBaseDirector);

public:
	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.

private:
	camReplayDirector(const camBaseObjectMetadata& metadata);

#if GTA_REPLAY
public:
	~camReplayDirector();

	void	Reset();

	float	GetMaxDistanceAllowedFromPlayer(const bool considerEditModeForDistanceReduction = false) const;
	float	GetFovOfRenderedFrame() const;
	float	GetFocalLengthOfRenderedFrameInMM() const;

	const camBaseCamera* GetActiveCamera() const	{ return m_IsFallingBackToRecordedCamera ? m_FallbackCamera.Get() : m_ActiveCamera.Get(); }
	bool	IsFallingBackToRecordedCamera() const	{ return m_IsFallingBackToRecordedCamera; }

    const camBaseCamera* GetPreviousCamera() const { return m_PreviousCamera.Get(); }

	bool	IsCurrentlyBlending() const				{ return (m_ActiveCamera && m_ActiveCamera->GetIsClassId(camReplayFreeCamera::GetStaticClassId()) &&
														static_cast<const camReplayFreeCamera*>(m_ActiveCamera.Get())->IsBlendingFromCamera()); }

	bool	IsRecordedCamera() const				{ return (GetActiveCamera() && GetActiveCamera()->GetIsClassId(camReplayRecordedCamera::GetStaticClassId())); }
	bool	IsFreeCamActive() const					{ return (GetActiveCamera() && GetActiveCamera()->GetIsClassId(camReplayFreeCamera::GetStaticClassId())); }

	void	FindTargetInList(s32 TargetReplayId, s32& ListIndex, s32& LastValidIndex) const;
	s32		IncrementTarget(const s32 TargetReplayId, bool shouldIncrease, s32& targetIndex, bool canSelectNone = true) const;
	s32		GetTargetIdFromIndex(const s32 targetIndex) const;

	void	GetDofSettingsHash(u32& dofSettingsHash) const;
	void	GetDofMinMaxFNumberAndFocalLengthMultiplier(float& minFNumber, float& maxFNumber, float& fNumberExponent, float& minMultiplier, float& maxMultiplier) const;

	void	OnReplayClipChanged();

	void	IncreaseAttachTargetSelection(const u32 markerIndex);
	void	DecreaseAttachTargetSelection(const u32 markerIndex);
	void	DeselectAttachTarget(const u32 markerIndex);

	void	IncreaseLookAtTargetSelection(const u32 markerIndex);
	void	DecreaseLookAtTargetSelection(const u32 markerIndex);
	void	DeselectLookAtTarget(const u32 markerIndex);

	void	IncreaseFocusTargetSelection(const u32 markerIndex);
	void	DecreaseFocusTargetSelection(const u32 markerIndex);
	void	DeselectFocusTarget(const u32 markerIndex);

	s32		GetNumValidTargets() const						{ return m_ValidTargetCount; }
	
	static bool	IsReplayPlaybackSetupFinished()				{ return ms_IsReplayPlaybackSetupFinished; }
	static bool	WasReplayPlaybackSetupFinishedThisUpdate()	{ return ms_IsReplayPlaybackSetupFinished && !ms_WasReplayPlaybackSetupFinished; }
	static s32	GetCurrentMarkerIndex();
	
	void	SetLastValidFreeCamFrame(const camFrame& Frame) { m_LastValidFreeCamFrame.CloneFrom(Frame); m_HaveLastValidFreeCamFrame = true; }
	camFrame& GetLastValidFreeCamFrame() { return m_LastValidFreeCamFrame; }
	bool	IsLastFreeCamFrameValid() const { return m_HaveLastValidFreeCamFrame;  }
	void	SetLastCamFrameValid(bool valid) { m_HaveLastValidFreeCamFrame = valid;  }

	void	SetLastValidCamRelativeSettings(const Vector3& desiredPosition, float lastValidFreeCamAttachEntityRelativeHeading, float lastValidFreeCamAttachEntityRelativePitch, float lastValidFreeCameAttachEntityRelativeRoll); 
	void	GetLastValidCamRelativeSettings( Vector3& desiredPosition, float& lastValidFreeCamAttachEntityRelativeHeading, float& lastValidFreeCamAttachEntityRelativePitch, float& lastValidFreeCameAttachEntityRelativeRoll); 
	bool	IsLastRelativeSettingValid() const { return m_HaveLastValidFreeCamRelativeSettings; }
	void	SetLastRelativeSettingsValid(bool valid) { m_HaveLastValidFreeCamRelativeSettings = valid; }

	// PURPOSE: Return the target of the current camera
	const CEntity* GetActiveCameraTarget() const;

#if __BANK
	virtual void	DebugGetCameras(atArray<camBaseCamera*> &cameraList) const;
	virtual void	DebugRender() const;
	virtual void	AddWidgetsForInstance();
	void			GetDebugText(char* debugText) const;
#endif // __BANK

	static s32		GetPreviousMarkerIndex(s32 markerIndex);
	static s32		GetNextMarkerIndex(s32 markerIndex);
	void			ForceSmoothing(u32 duration, u32 blendInDuration, u32 blendOutDuration);
	void			DisableForceSmoothing();

private:
	virtual bool Update();
	virtual void PostUpdate();

	void	ApplyRotationalDampingToMatrix(const Matrix34& sourceMatrix, const float springConstant, const float dampingRatio, Matrix34& destinationMatrix);

	void	GenerateReplayCameraTargetList(const Vector3& vCenterPos);
	float	GetMaxTargetDistance() const;

	u32		CalculateCurrentCameraHash(const sReplayMarkerInfo* currentMarkerInfo) const;
	s32		GetNextMarkerIndex() const;
	s32		GetPreviousMarkerIndex() const;
	bool	CreateActiveCamera(u32 cameraHash);
	bool	CreateFallbackCamera();
	void	DestroyAllCameras();
	camReplayBaseCamera* GetReplayCameraWithIndex(const u32 markerIndex);

	void	UpdateShake();
	void	UpdateShakeTimeWarp();
	void	GetShakeSelection(const sReplayMarkerInfo& markerInfo, u32& shakeHash, float& shakeAmplitude, float& shapeSpeed) const;
	atHashString		GetMarkerShakeHash(eMarkerShakeType shakeType) const;
	camBaseFrameShaker*	Shake(u32 nameHash, float amplitude = 1.0f);
	void	UpdateSmoothing();

	const camReplayDirectorMetadata& m_Metadata;

	camFrame			m_LastFrame;

	camFrame			m_LastValidFreeCamFrame; 
	Vector3				m_LastValidFreeCamDesiredPos; 

	RegdCamBaseCamera	m_ActiveCamera;
	RegdCamBaseCamera	m_PreviousCamera;

	RegdCamReplayRecordedCamera m_FallbackCamera;

	ReplayTargetInfo	m_aoValidTarget[MAX_PEDS_TRACKED];

	camDampedSpring		m_SmoothingPositionXSpring;
	camDampedSpring		m_SmoothingPositionYSpring;
	camDampedSpring		m_SmoothingPositionZSpring;
	camDampedSpring		m_SmoothingHeadingSpring;
	camDampedSpring		m_SmoothingPitchSpring;
	camDampedSpring		m_SmoothingRollSpring;
	camDampedSpring		m_SmoothingFovSpring;
	camDampedSpring		m_FrameShakerAmplitudeSpring;

	u32					m_ShakeHash;
	s32					m_ValidTargetCount;
	s32					m_CurrentMarkerIndex;
	s32					m_PreviousMarkerIndex;

	u32					m_ForceSmoothingStartTime;
	u32					m_ForceSmoothingDuration;
	u32					m_ForceSmoothingBlendInDuration;
	u32					m_ForceSmoothingBlendOutDuration;

	float				m_TargetFrameShakerAmplitude;
	float				m_LastValidFreeCamAttachEntityRelativeHeading; 
	float				m_LastValidFreeCamAttachEntityRelativePitch; 
	float				m_LastValidFreeCamAttachEntityRelativeRoll; 
	float				m_ShakeAmplitude;
	float				m_ShakeSpeed;
	float				m_Speed;
#if __BANK
	static float		ms_DebugSmoothSpringConstant;
#endif //  __BANK

	bool				m_OutOfRangeWarningIssued;
	bool				m_HaveLastValidFreeCamFrame; 
	bool				m_HaveLastValidFreeCamRelativeSettings;
	bool				m_PreviousCameraWasBlending;
	bool				m_IsForcingCameraShake;
	bool				m_IsFallingBackToRecordedCamera;
	bool				m_ShouldResetSmoothing;
#if __BANK
	static bool			ms_DebugShouldSmooth;
	static bool			ms_DebugWasReplayPlaybackSetupFinished;
#endif //  __BANK

	static bool			ms_IsReplayPlaybackSetupFinished;
	static bool			ms_WasReplayPlaybackSetupFinished;

#else // !GTA_REPLAY - Stub out everything.

public:
	void	Reset() { }

private:
	virtual bool	Update() { return true; }
#endif // GTA_REPLAY

private:
	//Forbid copy construction and assignment.
	camReplayDirector(const camReplayDirector& other);
	camReplayDirector& operator=(const camReplayDirector& other);
};

#endif // REPLAY_DIRECTOR_H
