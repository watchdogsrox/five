//
// camera/replay/ReplayBaseCamera.h
// 
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved.
//

#ifndef REPLAY_BASE_CAMERA_H
#define REPLAY_BASE_CAMERA_H

#include "camera/base/BaseCamera.h"
#include "scene/RegdRefTypes.h"
#include "control/replay/ReplaySettings.h"

class camReplayBaseCameraMetadata;
class camCollision;

#if GTA_REPLAY
struct sReplayMarkerInfo;
#endif

class camReplayBaseCamera : public camBaseCamera
{
	DECLARE_RTTI_DERIVED_CLASS(camReplayBaseCamera, camBaseCamera);

public:
	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.

protected:
	camReplayBaseCamera(const camBaseObjectMetadata& metadata);

public:
#if GTA_REPLAY
	virtual bool Init(const s32 markerIndex, const bool shouldForceDefaultFrame = false);
	virtual void GetDefaultFrame(camFrame& frame) const;
	virtual bool ShouldFallbackToGameplayCamera() const { return false; }

	bool IsWaitingToInitialize() const			{ return m_WaitingToInit; }
	s32 GetMarkerIndex() const					{ return m_MarkerIndex; }
    
    s32 GetDofMarkerIndex() const               { return m_DofMarkerIndex; } 
    void SetDofMarkerIndex(s32 markerIndex)     { m_DofMarkerIndex = markerIndex; }

    u32 GetNumReplayUpdatesPerformed() const	{ return m_NumReplayUpdatesPerformed; }

#endif // GTA_REPLAY

protected:
	virtual bool Update();
	virtual void ComputeDofOverriddenFocusSettings(Vector3& focusPosition, float& blendLevel) const;
	virtual const CEntity* ComputeDofOverriddenFocusEntity() const;
	virtual void ComputeDofSettings(float overriddenFocusDistance, float overriddenFocusDistanceBlendLevel, camDepthOfFieldSettingsMetadata& settings) const;
	virtual void PostUpdate();

#if GTA_REPLAY
	virtual void SaveMarkerCameraInfo(sReplayMarkerInfo& markerInfo, const camFrame& camFrame) const;
	virtual void LoadMarkerCameraInfo(sReplayMarkerInfo& markerInfo, camFrame& camFrame);
	virtual void ValidateEntities(sReplayMarkerInfo& markerInfo);

    static bool  GetClosestParachuteCB(CEntity* pEntity, void* data);
    static void  AvoidCollisionWithParachutes(const Vector3& testPoint, const camCollision* collision);
#endif // GTA_REPLAY

	const camReplayBaseCameraMetadata& m_Metadata;

#if GTA_REPLAY
	s32	m_MarkerIndex;
    s32 m_DofMarkerIndex;
	u32 m_NumReplayUpdatesPerformed; //Resets in Init(), m_NumUpdatesPerformed doesn't
	bool m_WaitingToInit;
	bool m_ForcingDefaultFrame;
#endif // GTA_REPLAY

private:
	//Forbid copy construction and assignment.
	camReplayBaseCamera(const camReplayBaseCamera& other);
	camReplayBaseCamera& operator=(const camReplayBaseCamera& other);
};

#endif // REPLAY_BASE_CAMERA_H
