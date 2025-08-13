//
// camera/system/CameraManager.h
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#ifndef CAMERA_MANAGER_H
#define CAMERA_MANAGER_H

#include "atl/string.h"
#include "atl/vector.h"
#include "vector/colors.h"
#include "vector/color32.h"
#include "camera/debug/ShakeDebugger.h"
#include "camera/system/CameraMetadata.h"
#include "control/replay/ReplaySettings.h"
#include "peds/pedDefines.h"
#include "scene/RegdRefTypes.h"
#include "vector/color32.h"

class camAnimatedCamera;
class camBaseCamera;
class camBaseDirector;
class camEnvelope;
class camFramePropagator;

struct tRenderedCameraObjectSettings
{
	tRenderedCameraObjectSettings(const camBaseObject* object = NULL, float blendLevel = 0.0f)
		: m_Object(object)
		, m_BlendLevel(blendLevel)
	{
	}

	RegdConstCamBaseObject	m_Object;
	float					m_BlendLevel;
};

//The top level of the camera system.
class camManager
{
	friend class camInterface;

public:

#if FPS_MODE_SUPPORTED
	enum eFirstPersonFlashEffectType
	{
		CAM_PUSH_IN_FX_NEUTRAL = 0,
		CAM_PUSH_IN_FX_MICHAEL,
		CAM_PUSH_IN_FX_FRANKLIN,
		CAM_PUSH_IN_FX_TREVOR,
		FIRST_PERSON_FLASH_EFFECT_COUNT
	};
#endif // FPS_MODE_SUPPORTED

	static void		Init(unsigned initMode);

	static void		Shutdown(unsigned shutdownMode);

	static void		Update();

	static const camFramePropagator& GetFramePropagator()	{ return ms_FramePropagator; }

	static const CPed* GetPedMadeInvisible()				{ return ms_PedMadeInvisible; }

	static u32		ComputeNumDirectorsInterpolating();

#if FPS_MODE_SUPPORTED
	static const char * GetFirstPersonFlashEffectName(eFirstPersonFlashEffectType type);
	static void		SuppressFirstPersonFlashEffectsThisFrame(bool bSuppress)	{ ms_SuppressFirstPersonFlashEffectThisFrame = bSuppress;}
	static void		TriggerFirstPersonFlashEffect(eFirstPersonFlashEffectType type = CAM_PUSH_IN_FX_NEUTRAL, u32 effectDelay=0, const char * debugContext = "NONE");
	static void		AbortPendingFirstPersonFlashEffect(const char * abortReason);
#endif // FPS_MODE_SUPPORTED

#if __BANK
	static void		InitWidgets();
	static void		AddWidgets();

	static void		DebugSetShouldRenderCameras(bool state)			{ ms_ShouldDebugRenderCameras = state; }

	static void		DebugRender();

	static void		Render(); 

	static void		ResetDebugVariables();

	static UnapprovedCameraLists& GetUnapprovedCameraList() {return ms_UnapprovedCameraLists; }

	static const camBaseCamera* GetSelectedCamera() { return ms_DebugSelectedCamera; }

	PRINTF_LIKE_N(2) static void DebugRegisterScriptCommand(const char* callerName, const char* format, ...);

	static bool		ms_ShouldDebugRenderCameraFarClip;
	static bool		ms_ShouldDebugRenderCameraNearDof;
	static bool		ms_ShouldDebugRenderCameraFarDof;
	static bool		ms_ShouldRenderCameraInfo;
#endif // __BANK

private:

	static void UpdateInternal();

	static const atArray<tRenderedCameraObjectSettings>& GetRenderedDirectors()	{ return ms_RenderedDirectors; }
	static const camBaseDirector* GetDominantRenderedDirector()					{ return ms_DominantRenderedDirector; }
	static const atArray<tRenderedCameraObjectSettings>& GetRenderedCameras()	{ return ms_RenderedCameras; }
	static const camBaseCamera* GetDominantRenderedCamera()						{ return ms_DominantRenderedCamera; }

	template <class _T> static _T* FindDirector()
	{
		return static_cast<_T*>(FindDirector(_T::GetStaticClassId()));
	}

	static camBaseDirector* FindDirector(const atHashWithStringNotFinal& classIdToFind);

	static void		PreUpdateCameras();
	static void		RemoveBlendedOutRenderedObjects(atArray<tRenderedCameraObjectSettings>& renderedObjects);
	static const camBaseObject* ComputeDominantRenderedObject(const atArray<tRenderedCameraObjectSettings>& renderedObjects);
	static void		UpdateCutFlags(const camBaseCamera* dominantRenderedCameraOnPreviousUpdate, const camFrame& renderedFrameOnPreviousUpdate,
						camFrame& renderedFrame);
	static void		ClearCameraAutoResetFlags();
	static void		UpdatePedVisibility();
	static void		ModifyPedVisibility(CPed& ped, bool isVisible, bool includeShadows = false);
	static void		UpdateVehicleVisibility();
#if FPS_MODE_SUPPORTED
	static void		UpdateCustomFirstPersonShooterBehaviour();
	static bool		IsRenderingFirstPersonCamera();
	static bool		IsRenderingFirstPersonShooterCamera()				{ return ms_IsRenderingFirstPersonShooterCamera; }
	static bool		IsRenderingFirstPersonShooterCustomFallBackCamera()	{ return ms_IsRenderingFirstPersonShooterCustomFallBackCamera; }
#endif // FPS_MODE_SUPPORTED

#if REGREF_VALIDATION
	static void		DebugDeleteUnreferencedCameras();
#endif // REGREF_VALIDATION

#if __BANK
	enum eDebugRenderTableColumns
	{
		DIRECTOR_NAME_COLUMN = 0,
		DIRECTOR_BLEND_COLUMN,
		SELECTED_COLUMN,
		CAMERA_NAME_COLUMN,
		UPDATING_COLUMN,
		BLEND_COLUMN,
		POSITION_X_COLUMN,
		POSITION_Y_COLUMN,
		POSITION_Z_COLUMN,
		ORIENTATION_P_COLUMN,
		ORIENTATION_R_COLUMN,
		ORIENTATION_Y_COLUMN,
		NEAR_CLIP_COLUMN,
		FAR_CLIP_COLUMN,
		FLAGS_COLUMN,
		DOF_1_COLUMN,
		DOF_2_COLUMN,
		DOF_3_COLUMN,
		DOF_4_COLUMN,
		FOV_COLUMN,
		MOTION_BLUR_COLUMN,
		INFO_COLUMN,
		NUM_DEBUG_RENDER_COLUMNS
	};

	enum eDebugRenderCycleOptions
	{
		DEFAULT_RENDER_NONE = 0,
		RENDER_TABLE_ONLY,
		RENDER_TABLE_AND_CAMERAS,
		RENDER_CAMERAS_ONLY,
		NUM_RENDER_OPTIONS
	};

	static void		AddWidgetsForFullScreenBlurEffect(bkBank& bank);

	static void		UpdateWidgets();

	static void		DebugRenderCameraTable();
	static void		DebugRenderDirectorText(const camBaseDirector* director, u8& rowIndex);
	static void		DebugRenderCameraText(const camBaseCamera* camera, u8& rowIndex, bool isFinal = false);
	static float	GetBlendValueForCamera(const camBaseCamera* camera);
	static float	GetBlendValueForDirector(const camBaseDirector* director);
	static void		DebugRenderFinalFrameText(u8 rowIndex);
	static void		DebugRenderGlobalState(u8 rowIndex);
	static void		DebugRenderTableEntry(const u8 columnIndex, const u8 rowIndex, const char* text, const Color32 colour = Color_default, const u8 endColumnIndex = 0);

	static void		DebugClearDisplay();
	static void		DebugCycleRenderOptions();
	static void		DebugSelectPreviousCamera();
	static void		DebugSelectNextCamera();
	static void		DebugSelectValidation();
	static void		DebugSelectValidation(atArray<camBaseCamera*> &cameras, int &directorIndex, int &cameraIndex);
	static void		DebugRenderCameras();
	static void		DebugRenderThirdLines();

	static void		LoadUnapprovedList();
	static bool		ShouldRenderUnapprovedCamerasDebugText(); 

	static void		DebugOutputNewVehicleMetadata();

	static void		DebugInitFullScreenBlurEffect();
	static void		DebugStartFullScreenBlurEffect();
	static void		DebugReleaseFullScreenBlurEffect();
	static void		DebugAbortFullScreenBlurEffect();
	static void		DebugUpdateFullScreenBlurEffect(camFrame& renderedFrame);

	static void		DebugFlushScriptCommands();
	static void		DebugRenderScriptCommands(u8& rowIndex);
#endif // __BANK

	static atArray<RegdCamBaseDirector>				ms_Directors;
	static atArray<tRenderedCameraObjectSettings>	ms_RenderedDirectors;
	static atArray<tRenderedCameraObjectSettings>	ms_RenderedCameras;
	static RegdConstCamBaseDirector					ms_DominantRenderedDirector;
	static RegdConstCamBaseCamera					ms_DominantRenderedCamera;

	static RegdPed				ms_PedMadeInvisible;
	static RegdVeh				ms_VehicleMadeInvisible;

	static camFramePropagator	ms_FramePropagator;

#if FPS_MODE_SUPPORTED
	static bool					ms_IsRenderingFirstPersonShooterCamera;
	static bool					ms_IsRenderingFirstPersonShooterCustomFallBackCamera;
	static atHashString ms_FirstPersonFlashEffectName[FIRST_PERSON_FLASH_EFFECT_COUNT];
	static eFirstPersonFlashEffectType ms_CurrentFirstPersonFlashEffect;
	static eFirstPersonFlashEffectType ms_PendingFirstPersonFlashEffect;
	static u32					ms_TriggerFirstPersonFlashEffectTime;
	static bool					ms_SuppressFirstPersonFlashEffectThisFrame;
	static bool					ms_WasInFirstPersonMode;
#if __BANK
	static atHashString			ms_FirstPersonFlashEffectCallingContext;
	static atHashString			ms_LastFirstPersonFlashContext;
	static u32					ms_LastFirstPersonFlashTime;
	static atHashString			ms_LastFirstPersonFlashAbortReason;
	static u32					ms_LastFirstPersonFlashAbortTime;
#endif //__BANK
#endif // FPS_MODE_SUPPORTED

#if __BANK
	static RegdConstCamBaseCamera	ms_DebugSelectedCamera;
	static RegdConstCamBaseDirector	ms_DebugSelectedDirector;

	static camEnvelopeMetadata	ms_DebugFullScreenBlurEffectEnvelopeMetadata;
	static camEnvelope*			ms_DebugFullScreenBlurEffectEnvelope;	
	static bool					ms_ShouldDebugFullScreenBlurEffectUseGameTime;
	static bool					ms_ShouldDebugRenderCameraTable;
	static bool					ms_ShouldDebugRenderCameras;
	static bool					ms_ShouldDebugRenderThirdLines;
	static bool					ms_ShouldRenderUnapprovedAnimatedCameraText;
	static float				ms_UnapprovedAnimatedCameraTextTimer;

	static UnapprovedCameraLists ms_UnapprovedCameraLists;
	static bool ms_UnapprovedCameraListsLoaded;

	static u8 ms_DebugRenderOption;

	struct DebugScriptCommand
	{
		atString text;
		u32 time;
	};

	static atVector<DebugScriptCommand> ms_DebugRegisteredScriptCommands;
#endif // __BANK

#if RSG_PC
	static float				ms_fStereoConvergence;
#endif

#if GTA_REPLAY && __BANK
public:
	static bool					ms_DebugReplayCameraMovementDisabledThisFrame;
	static bool					ms_DebugReplayCameraMovementDisabledThisFrameScript;
#endif // GTA_REPLAY

#if __BANK
	static camShakeDebugger		ms_ShakeDebugger;
#endif
};

#endif // CAMERA_MANAGER_H
