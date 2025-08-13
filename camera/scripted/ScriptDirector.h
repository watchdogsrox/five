//
// camera/scripted/ScriptDirector.h
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#ifndef SCRIPT_DIRECTOR_H
#define SCRIPT_DIRECTOR_H

#include "script/thread.h"

#include "camera/base/BaseDirector.h"
#include "Peds\pedDefines.h" // for FPS_MODE_SUPPORTED
#include "streaming/streamingmodule.h"

#if __BANK
extern const char* g_ScriptDumpFile;
const u32 g_MaxDebugShakeNameLength = 64;
#endif // __BANK

namespace rage
{
	class crClip;
}

class camAnimatedCamera;
class camFrame;
class camScriptDirectorMetadata;
class GtaThread;

enum ScriptDirectorRenderingOptionFlags
{
	SCRIPT_DIRECTOR_RENDERING_OPTIONS_NONE = 0,
	SCRIPT_DIRECTOR_STOP_RENDERING_OPTION_PLAYER_EXITS_INTO_COVER = BIT0, 
}; 

//The director responsible for the scripted cameras.
class camScriptDirector : public camBaseDirector
{
	DECLARE_RTTI_DERIVED_CLASS(camScriptDirector, camBaseDirector);

public:
	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.

private:
	camScriptDirector(const camBaseObjectMetadata& metadata);

public:
	~camScriptDirector();

	bool	IsCameraScriptControllable(s32 cameraIndex) const;
	bool	IsCameraActive(s32 cameraIndex) const;

	virtual void Render(u32 interpolationDuration = 0, bool shouldLockInterpolationSourceFrame = false);
	void StopRendering(bool gameplayCatchUp, u32 interpolationDuration = 0, bool shouldLockInterpolationSourceFrame = false, float distanceToBlend = 0.0f, int blendType = -1);
	virtual void StopRendering(u32 interpolationDuration = 0, bool shouldLockInterpolationSourceFrame = false) { StopRendering(false, interpolationDuration, shouldLockInterpolationSourceFrame); }

	void	ForceStopRendering(bool gameplayCatchUp = false, u32 interpolationDuration = 0, bool shouldLockInterpolationSourceFrame = false, float distanceToBlend = 0.0f, int blendType = -1);

	void	OnScriptTermination(const GtaThread* scriptThread);

	void	Reset();

	s32		CreateCamera(const char* cameraName);
	s32		CreateCamera(u32 cameraNameHash);
	void	DestroyCamera(s32 cameraIndex);

	void	ActivateCamera(s32 cameraIndex);
	void	DeactivateCamera(s32 cameraIndex);

	void	SetCameraFrameParameters(s32 cameraIndex, const Vector3& position, const Vector3& orientation, float fov, u32 duration,
				int graphTypePos, int graphTypeRot, int rotationOrder);

	bool	AnimateCamera(s32 cameraIndex, const strStreamingObjectName animDictName, const crClip& clip, const Matrix34& sceneOrigin, int sceneId =-1, u32 iFlgas = 0);
	bool	IsCameraPlayingAnimation(s32 cameraIndex, const strStreamingObjectName animDictName, const crClip& clip);
	void	SetCameraAnimationPhase(s32 cameraIndex, float phase);
	float	GetCameraAnimationPhase(s32 cameraIndex);
	void	SetRenderOptions(s32 RenderOptions) {m_RenderingOptions.SetAllFlags(RenderOptions); }

#if __BANK
	const char*	DebugBuildCameraStringForTextDisplay(char* string, u32 maxLength);
	const char*	DebugAppendScriptControlledCameraDetails(const camBaseCamera& camera, char* string, u32 maxLength) const;
	const camAnimatedCamera* GetAnimatedCamera(s32 cameraIndex) const;
	virtual void AddWidgetsForInstance();
	virtual void DebugGetCameras(atArray<camBaseCamera*> &cameraList) const;

	void DebugSetScriptEnabledShakeName(const char* shakeName);
	void DebugSetScriptEnabledShakeAmplitude(float amplitudeScalar);
	void DebugSetScriptDisabledShake();
	const char* DebugGetScriptEnabledShakeName() const;
	float DebugGetScriptEnabledShakeAmplitude() const;
#endif // __BANK

#if FPS_MODE_SUPPORTED
	void SetFirstPersonFlashEffectType(u32 type); // see camManager::eFirstPersonFlashEffectType;
	void SetFirstPersonFlashEffectVehicleModelHash(u32 modelHash);
#endif // FPS_MODE_SUPPORTED_ONLY

private:
#if FPS_MODE_SUPPORTED
	bool ShouldRequestFlashForFirstPersonExit(bool isCatchup, u32 interpolationDuration) const;
	bool WillExitToFirstPersonCamera(u32 vehicleModelHash) const;
	u32 GetFirstPersonFlashEffectType() const; // see camManager::eFirstPersonFlashEffectType;
	u32 GetFirstPersonFlashEffectVehicleModelHash() const;
	u32 GetFirstPersonFlashEffectDelay(u32 blendOutDuration, bool bCatchup) const;
#endif // FPS_MODE_SUPPORTED

	virtual bool Update();
	virtual void UpdateActiveRenderState();

	camAnimatedCamera* GetAnimatedCamera(s32 cameraIndex);
	void	CleanUpCameraReferences();

	void	StopRendering(bool gameplayCatchUp, u32 interpolationDuration, bool shouldLockInterpolationSourceFrame, const GtaThread* scriptThread, float distanceToBlend = 0.0f, int blendType = -1);

	void	DestroyAllCameras();

#if __BANK
	void	DebugDumpRootCamFrameParamsToScript();
	void	DebugDumpRootCameraMatrixRelativeToPlayerToScript();
	void	DebugDumpCameraFrameParamsToScript(const Vector3& position, const Vector3& orientationEulers, const float fov);
	void	DebugToggleWidescreenBordersActive();
#endif // __BANK

	const camScriptDirectorMetadata& m_Metadata;

	atArray<RegdCamBaseCamera>	m_Cameras;
	atArray<RegdCamBaseCamera>	m_ActiveCameras;
	atArray<scrThreadId>		m_RenderedThreadIds;

#if FPS_MODE_SUPPORTED
	u32 m_FirstPersonFlashEffectType;
	u32 m_FirstPersonFlashEffectVehicleModelHash;
#endif // FPS_MODE_SUPPORTED

#if __BANK
	float	m_DebugScriptTriggeredShakeAmplitude;
	char	m_DebugScriptTriggeredShakeName[g_MaxDebugShakeNameLength];
#endif // __BANK

	fwFlags32 m_RenderingOptions;

	//Forbid copy construction and assignment.
	camScriptDirector(const camScriptDirector& other);
	camScriptDirector& operator=(const camScriptDirector& other);
};

#endif // SCRIPT_DIRECTOR_H
