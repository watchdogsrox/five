//
// camera/CamInterface.h
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#ifndef CAMERA_INTERFACE_H
#define CAMERA_INTERFACE_H

#include "vectormath/vec4v.h"

#include "camera/viewports/ViewportFader.h"
#include "control/replay/ReplaySettings.h"
#include "peds/pedDefines.h"		// include for FPS_MODE_SUPPORTED define
#include "scene/RegdRefTypes.h"

struct tRenderedCameraObjectSettings;

class camBaseCamera;
class camCinematicDirector;
class camCutsceneDirector;
class camDebugDirector;
class camFrame;
class camFrontendDirector;
class camGameplayDirector;
class camMarketingDirector;
class camScriptDirector;
class camSwitchDirector;
class camSyncedSceneDirector; 
class camReplayDirector;
class camAnimSceneDirector;
class CPed;
class CVehicle;

namespace rage
{
	class bkBank;
	class crSkeleton;
	class Color32;
	class Matrix34;
	class Vector3;
}

//----------------------------------------------------------------------------------
// An interface that reduces the multiple camera complexity
// This interface provides simple access methods about whatever the camera is doing

class camInterface
{
public:
	enum eLookDirection
	{
		LOOKING_FORWARD,
		LOOKING_LEFT,
		LOOKING_RIGHT,
		LOOKING_BEHIND,
		LOOKING_UP,
		LOOKING_DOWN
	};

	enum eDeathFailEffectState
	{
		DEATH_FAIL_EFFECT_INACTIVE,
		DEATH_FAIL_EFFECT_INTRO,
		DEATH_FAIL_EFFECT_OUTRO
	};

	static void	Init();
	static void	Shutdown();

	static const camFrame& GetFrame()		{ return ms_CachedFrame; }
	static const Matrix34& GetMat();
	static const Vector3& GetPos();
	static const Vector3& GetPosDelta()		{ return ms_FramePositionDelta; }
	static const Vector3& GetVelocity()		{ return ms_FrameVelocity; }
	static const Vector3& GetAcceleration()	{ return ms_FrameAcceleration; }
	static const Vector3& GetRateOfChangeOfAcceleration() { return ms_FrameRateOfChangeOfAcceleration; }
	static const Vector3& GetFront();
	static const Vector3& GetRight();
	static const Vector3& GetUp();
	static float GetHeading()				{ return ms_CachedHeading; }
	static float GetPitch()					{ return ms_CachedPitch; }
	static float GetRoll()					{ return ms_CachedRoll; }
	static float GetNear();
	static float GetFar();
	static float GetNearDof();
	static float GetFarDof();
	static float GetFov();
	static float GetMotionBlurStrength();

	//NOTE: The 'dominant rendered' director is the director with the highest blend level that is being rendered to the final output frame.
	// - Use IsRenderingDirector or GetRenderedDirectors to consider all directors that have been blended to produce the final output frame.
	static bool	IsDominantRenderedDirector(const camBaseDirector& director);
	//NOTE: The 'dominant rendered' director is the director with the highest blend level that is being rendered to the final output frame.
	// - Use GetRenderedDirectors to obtain a detailed breakdown of all directors that have been blended to produce the final output frame.
	static const camBaseDirector* GetDominantRenderedDirector();
	//Returns true if the specified director is being rendered to the final output frame, with any non-zero blend level.
	static bool	IsRenderingDirector(const camBaseDirector& director);
	static const atArray<tRenderedCameraObjectSettings>& GetRenderedDirectors();

	//NOTE: The 'dominant rendered' camera is the camera with the highest blend level that is being rendered to the final output frame.
	// - Use IsRenderingCamera or GetRenderedCameras to consider all cameras that have been blended to produce the final output frame.
	static bool	IsDominantRenderedCamera(const camBaseCamera& camera);
	//NOTE: The 'dominant rendered' camera is the camera with the highest blend level that is being rendered to the final output frame.
	// - Use GetRenderedCameras to obtain a detailed breakdown of all cameras that have been blended to produce the final output frame.
	static const camBaseCamera* GetDominantRenderedCamera();
	//Returns true if the specified camera is being rendered to the final output frame, with any non-zero blend level.
	static bool	IsRenderingCamera(const camBaseCamera& camera);
	static const atArray<tRenderedCameraObjectSettings>& GetRenderedCameras();

	// PURPOSE:	Return the matrix of the main camera transform relative to the target entity, if there's a blending active
	//			the cached blended frame will be used.
	// RETURNS:	True if the target entity is valid and the camera matrix was provided.
	static bool GetObjectSpaceCameraMatrix(const CEntity* pTargetEntity, Matrix34& mat);

#if FPS_MODE_SUPPORTED
	//Returns true if a first-person-shooter camera or a first person vehicle is being rendered to the final output frame, with any non-zero blend level.
	// If called during camera update, this will tell you if the previous camera was first person as it may change during the update.
	static bool IsRenderingFirstPersonCamera();
	//Returns true if a first-person-shooter camera is being rendered to the final output frame, with any non-zero blend level.
	static bool IsRenderingFirstPersonShooterCamera();
	//Returns true if a first-person-shooter custom fall-back camera is being rendered to the final output frame, with any non-zero blend level.
	static bool IsRenderingFirstPersonShooterCustomFallBackCamera();
	//Returns true if any of the following are true:
	// Dominant camera is camFirstPersonShooterCamera (First person shooter)
	// Dominant camera is a first person camCinematicMountedCamera (First person vehicle)
	// Dominant camera is a scripted camera, and SetScriptedCameraIsFirstPersonThisFrame(true) has been called
	static bool IsDominantRenderedCameraAnyFirstPersonCamera();
	// Signal whether scripted cameras should be considered first person cameras by IsDominantRenderedCameraAnyFirstPersonCameraThisFrame()
	static void SetScriptedCameraIsFirstPersonThisFrame(bool isFirstPersonThisFrame);
#endif // FPS_MODE_SUPPORTED

	static camCinematicDirector&	GetCinematicDirector();
	static camCutsceneDirector&		GetCutsceneDirector();
	static camDebugDirector&		GetDebugDirector();
	static camDebugDirector*		GetDebugDirectorSafe();
	static camGameplayDirector&		GetGameplayDirector();
	static camGameplayDirector*		GetGameplayDirectorSafe();
	static camMarketingDirector&	GetMarketingDirector();
	static camScriptDirector&		GetScriptDirector();
	static camSwitchDirector&		GetSwitchDirector();
	static camSyncedSceneDirector&	GetSyncedSceneDirector();
	static camAnimSceneDirector&	GetAnimSceneDirector(); 
	static camReplayDirector&		GetReplayDirector();

	static float GetPlayerControlCamHeading(const CPed& player);
	static float GetPlayerControlCamPitch(const CPed& player);
	static const camFrame& GetPlayerControlCamAimFrame();

	static float ComputeMiniMapHeading();

#if GTA_REPLAY
	static void	CacheCameraAttachParent(const CEntity *pEntity)			{ ms_CachedAttachedEntity = pEntity; }
	static const CEntity *GetCachedCameraAttachParent()					{ return ms_CachedAttachedEntity; }

	static const CViewportFader& GetFader() { return ms_Fader; }
	static void SetFader(const CViewportFader& fader);
#endif	//GTA_REPLAY


	static void	CacheFrame(const camFrame& sourceFrame);
	static void SetCachedFrameOnly(const camFrame& sourceFrame);

	static void	SetScriptControlledFollowPedThisUpdate(const CPed* ped) { ms_ScriptControlledFollowPedThisUpdate = ped; }
	static const CPed* FindFollowPed();

	static void	ResurrectPlayer();

	static void	ShakeCameraExplosion(u32 shakeNameHash, float explosionStrength, float rollOffScaling, const Vector3& positionOfExplosion);
	static bool IsCameraShaking(const bool includeExplosions = false);
	static void RegisterWeaponFire(const CWeapon& weapon, const CEntity& firingEntity, const bool isNetworkClone = false);

	static float GetViewportTanFovH();
	static float GetViewportTanFovV();
	static const Vector3& GetViewportUp();

	static void SetFixedFade(float fadeValue);
	static void SetFixedFade(float fadeValue, const Color32 &fadeColor);
	static void	FadeOut(s32 duration, bool shouldHoldAtEnd = true);
	static void	FadeOut(s32 duration, bool shouldHoldAtEnd, const Color32& fadecolour);
	static bool IsFadingOut();
	static bool IsFadedOut();
	static void	FadeIn(s32 duration);
	static void	FadeIn(s32 duration, const Color32& fadecolour);
	static bool IsFadingIn();
	static bool IsFadedIn();
	static bool	IsFading();
	static bool	IsFadingOrJustFaded();
	static float GetFadeLevel();
	static void RenderFade();
	static void UpdateFade();

	static bool IsSphereVisibleInGameViewport(const Vector3& centre, float radius);
	static bool IsSphereVisibleInGameViewport(Vec4V_In centreAndRadius);
	static bool	IsSphereVisibleInViewport(int viewportId, const Vector3& centre, float radius);

	static bool IsInitialized() { return ms_IsInitialized; }

	static bool						ComputeVehicleEntryExitPointPositionAndOrientation(const CPed& targetPed, const CVehicle& targetVehicle,
										Vector3& entryExitPointPosition, Quaternion& entryExitOrientation, bool& hasClimbDown);

	static void	ClearAutoResetVariables() { ms_ScriptControlledFollowPedThisUpdate = NULL; }

	static bool	ComputeShouldPauseCameras();

	static CPed* ComputeFocusPedOnScreen(float maxValidDistance, s32 screenPositionTestBoneTag, float maxScreenWidthRatioAroundCentreForTestBone,
		float maxScreenHeightRatioAroundCentreForTestBone, float minRelativeHeadingScore, float maxScreenCentreScoreBoost,
		float maxScreenRatioAroundCentreForScoreBoost, s32 losTestBoneTag1, s32 losTestBoneTag2);

	static bool	ComputeHasClearLineOfSightToPedBone(const CPed& ped, s32 boneTag, bool shouldIgnoreOcclusionDueToFollowPed = true, bool shouldIgnoreOcclusionDueToFollowVehicle = true);

	static bool IsRenderedCameraInsideVehicle();
	static bool IsRenderedCameraForcingMotionBlur();

	static bool ComputeShouldMakePedHeadInvisible(const CPed& ped);

	static void	DisableNearClipScanThisUpdate();

	static u32 ComputePovTurretCameraHash(const CVehicle* pVehicle);

#if RSG_PC
	static float GetStereoConvergence();
#endif

	static bool IsDeathFailEffectActive() { return (ms_DeathFailEffectState != DEATH_FAIL_EFFECT_INACTIVE); }
	static void SetDeathFailEffectState(s32 state);

#if FPS_MODE_SUPPORTED
	static void ApplyPedBoneOverridesForFirstPerson(const CPed& ped, crSkeleton& skeleton);
#endif // FPS_MODE_SUPPORTED

#if __BANK
	static void	AddWidgets(bkBank &bank);

	static void	DebugDisplayCameraInfoText();
	static void	DebugDumpSpectatorPedInfo();

	static void	DebugFollowFocusPed(bool b) { ms_DebugShouldFollowDebugFocusPed = b; }

	static void	DebugSetRespotCounterOnDebugFocusVehicle();

	static void UpdateAtEndOfFrame();
	static void DebugShouldRestartSystemAtEndOfFrame() { ms_DebugShouldRestartSystemAtEndOfFrame = true; }
#endif // __BANK

private:
	static void UpdateRenderedCamInVehicle(); 

	static CViewportFader ms_Fader;

	static ALIGNAS(16) camFrame		ms_CachedFrame; //A safely cached copy of the root camera frame.

#if GTA_REPLAY
	static RegdConstEnt 	ms_CachedAttachedEntity;
#endif	//GTA_REPLAY

	static Vector3		ms_FramePositionDelta;
	static Vector3		ms_FrameVelocity;
	static Vector3		ms_FrameAcceleration;
	static Vector3		ms_FrameRateOfChangeOfAcceleration;

	static RegdCamBaseDirector	ms_CinematicDirector;
	static RegdCamBaseDirector	ms_CutsceneDirector;
	static RegdCamBaseDirector	ms_DebugDirector;
	static RegdCamBaseDirector	ms_GameplayDirector;
	static RegdCamBaseDirector	ms_MarketingDirector;
	static RegdCamBaseDirector	ms_ScriptDirector;
	static RegdCamBaseDirector	ms_SwitchDirector;
	static RegdCamBaseDirector	ms_ReplayDirector;
	static RegdCamBaseDirector  ms_SyncedSceneDirector;
	static RegdCamBaseDirector	ms_AnimSceneDirector;

	static RegdConstPed	ms_ScriptControlledFollowPedThisUpdate;
	static s32			ms_DeathFailEffectState;
	static float		ms_CachedHeading;
	static float		ms_CachedPitch;
	static float		ms_CachedRoll;
	static bool			ms_IsInitialized;
	static bool			ms_IsOverridingStreamingFocus;
	static bool			ms_IsRenderedCamInVehicle;  

#if FPS_MODE_SUPPORTED
	static bool			ms_SetScriptedCameraIsFirstPersonThisFrame;  
#endif // FPS_MODE_SUPPORTED

#if __BANK
	static u32			ms_DebugRespotCounter;
	static bool			ms_DebugFlashLocallyWhileRespotting;
	static bool			ms_DebugFlashRemotelyWhileRespotting;
	static bool			ms_DebugShouldFollowDebugFocusPed;
	static bool			ms_DebugForceFollowPedControlOrientation;
	static bool			ms_DebugShouldDisplayActiveCameraInfo;
	static bool			ms_DebugShouldDisplayGameViewportInfo;

	static bool			ms_DebugShouldRestartSystemAtEndOfFrame;
#endif // __BANK

#if RSG_PC	
public:
	static bool			IsTripleHeadDisplay();
#endif
};

#endif // CAMERA_INTERFACE_H
