//
// script/commands_camera.cpp
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#include "script/wrapper.h"

#include "fwanimation/directorcomponentsyncedscene.h"

#include "animation/debug/AnimViewer.h"
#include "camera/CamInterface.h"
#include "camera/cinematic/CinematicDirector.h"
#include "camera/cinematic/camera/mounted/CinematicMountedCamera.h"
#include "camera/cinematic/context/CinematicInVehicleContext.h"
#include "camera/cinematic/context/CinematicInVehicleOverriddenFirstPersonContext.h"
#include "camera/cinematic/context/CinematicScriptContext.h"
#include "camera/debug/DebugDirector.h"
#include "camera/cutscene/CutsceneDirector.h"
#include "camera/debug/FreeCamera.h"
#include "camera/gameplay/GameplayDirector.h"
#include "camera/gameplay/aim/ThirdPersonAimCamera.h"
#include "camera/gameplay/aim/FirstPersonShooterCamera.h"
#include "camera/gameplay/follow/FollowPedCamera.h"
#include "camera/helpers/ControlHelper.h"
#include "camera/helpers/FrameInterpolator.h"
#include "camera/helpers/FrameShaker.h"
#include "camera/helpers/AnimatedFrameShaker.h"
#include "camera/replay/ReplayDirector.h"
#include "camera/scripted/BaseSplineCamera.h"
#include "camera/scripted/CustomTimedSplineCamera.h"
#include "camera/scripted/ScriptDirector.h"
#include "camera/scripted/ScriptedCamera.h"
#include "camera/scripted/ScriptedFlyCamera.h"
#include "camera/syncedscene/SyncedSceneDirector.h"
#include "camera/system/CameraManager.h"
#include "camera/viewports/ViewportManager.h"
#include "frontend/PauseMenu.h"
#include "peds/ped.h"
#include "peds/pedpopulation.h"
#include "renderer/PostProcessFX.h"
#include "script/script_channel.h"
#include "script/Handlers/GameScriptResources.h"
#include "script/script_helper.h"
#include "task/Animation/TaskScriptedAnimation.h"
#include "vehicles/vehiclepopulation.h"

#include "control/replay/replay.h"

CAMERA_OPTIMISATIONS()

const s32 INVALID_CAM_INDEX = -1; 

#if __BANK
#define REGISTER_CAMERA_SCRIPT_COMMAND(FORMAT, ...) camManager::DebugRegisterScriptCommand(__FUNCTION__, FORMAT, __VA_ARGS__)
#define REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS() camManager::DebugRegisterScriptCommand(__FUNCTION__, "")
#else
#define REGISTER_CAMERA_SCRIPT_COMMAND(FORMAT, ...)
#define REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS()
#endif //SCRIPT_DEBUGGING

namespace camera_commands
{
	camBaseCamera* GetCameraFromIndex(int cameraIndex, const char* ASSERT_ONLY(commandName))
	{
		camBaseCamera* camera = camBaseCamera::GetCameraAtPoolIndex(cameraIndex);
		SCRIPT_ASSERT_TWO_STRINGS(camera, commandName, "- Camera does not exist");

		return camera;
	}

	void CommandRenderScriptCams(bool bSetActive, bool bInterp, int iDuration, bool bShouldLockInterpolationSourceFrame, bool bShouldApplyAcrossAllThreads, int RenderOptions)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();

		scriptcameraDebugf3("FC: %d - CommandRenderScriptCams( bSetActive(%d), bInterp (%d), iDuration (%d), bShouldLockInterpolationSourceFrame(%d), bShouldApplyAcrossAllThreads(%d), %s )",
			fwTimer::GetFrameCount(), bSetActive, bInterp, iDuration, bShouldLockInterpolationSourceFrame, bShouldApplyAcrossAllThreads, CTheScripts::GetCurrentScriptNameAndProgramCounter());

		const u32 interpolationDuration = bInterp ? static_cast<u32>(Max(iDuration, 0)) : 0;
	
		//These options will get reset when the update runs
		camInterface::GetScriptDirector().SetRenderOptions(RenderOptions); 

		if(bSetActive)
		{
			camInterface::GetScriptDirector().Render(interpolationDuration, bShouldLockInterpolationSourceFrame);
		}
		else if(bShouldApplyAcrossAllThreads)
		{
			//NOTE: This should only be exercised by scripts that absolutely require such global behaviour, as this can result in conflicts between
			//concurrent script threads.
			camInterface::GetScriptDirector().ForceStopRendering(false, interpolationDuration, bShouldLockInterpolationSourceFrame);
		}
		else
		{
			camInterface::GetScriptDirector().StopRendering(interpolationDuration, bShouldLockInterpolationSourceFrame);
		}
	}

	void CommandStopRenderingScriptCamsUsingCatchUp(bool bShouldApplyAcrossAllThreads, float distanceToBlend, int blendType, int RenderOptions)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();

		//These options will get reset when the update runs
		camInterface::GetScriptDirector().SetRenderOptions(RenderOptions); 

		scriptcameraDebugf3("FC: %d - CommandStopRenderingScriptCamsUsingCatchUp( bShouldApplyAcrossAllThreads(%d), distanceToBlend (%f), blendType (%d) )", 
			fwTimer::GetFrameCount(), bShouldApplyAcrossAllThreads, distanceToBlend, blendType);

		if(bShouldApplyAcrossAllThreads)
		{
			//NOTE: This should only be exercised by scripts that absolutely require such global behaviour,
			//      as this can result in conflicts between concurrent script threads.
			camInterface::GetScriptDirector().ForceStopRendering(true, 0, false, distanceToBlend, blendType);
		}
		else
		{
			camInterface::GetScriptDirector().StopRendering(true, 0, false, distanceToBlend, blendType);
		}
	}

	int CommandCreateCam(const char* CameraName, bool bActivate)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();

		CScriptResource_Camera camera(CameraName, bActivate);

		// cameras are referenced using their real pool index, this is inconsistent with the other resources. Everything should use the resource id.
		return CTheScripts::GetCurrentGtaScriptHandler()->RegisterScriptResourceAndGetRef(camera);
	}
	
	int CommandCreateCameraFromHash(int CameraHash, bool bActivate)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();

		CScriptResource_Camera camera(CameraHash, bActivate);

		// cameras are referenced using their real pool index, this is inconsistent with the other resources. Everything should use the resource id.
		return CTheScripts::GetCurrentGtaScriptHandler()->RegisterScriptResourceAndGetRef(camera);
	}

	int CommandCreateCameraWithParams(int CameraHash, const scrVector &scrCamPos, const scrVector &scrCamRot, float fCamFov, bool bActivate, int RotOrder)		//add pos, rot and fov
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();

		int CameraIndex = CommandCreateCameraFromHash(CameraHash, bActivate);

		Vector3 vPos (scrCamPos);
		Vector3 vRot (scrCamRot);

		camInterface::GetScriptDirector().SetCameraFrameParameters(CameraIndex, vPos, vRot * DtoR, fCamFov, 0, camFrameInterpolator::LINEAR,
			camFrameInterpolator::LINEAR, RotOrder);

		// cameras are referenced using their real pool index, this is inconsistent with the other resources. Everything should use the resource id.
		return CameraIndex;
	}

	int CommandCreateCamWithParams(const char* CameraName, const scrVector &scrCamPos, const scrVector &scrCamRot, float fCamFov, bool bActivate, int RotOrder)		//add pos, rot and fov
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();

		int CameraIndex = CommandCreateCam(CameraName, bActivate);

		Vector3 vPos (scrCamPos);
		Vector3 vRot (scrCamRot);

		camInterface::GetScriptDirector().SetCameraFrameParameters(CameraIndex, vPos, vRot * DtoR, fCamFov, 0, camFrameInterpolator::LINEAR,
			camFrameInterpolator::LINEAR, RotOrder);

		// cameras are referenced using their real pool index, this is inconsistent with the other resources. Everything should use the resource id.
		return CameraIndex;
	}

	void CommandDestroyCam(int CameraIndex, bool bShouldApplyAcrossAllThreads)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		// cameras are referenced using their real pool index, this is inconsistent with the other resources. Everything should use the resource id.

		if(bShouldApplyAcrossAllThreads)
		{
			//Destroy this cameras, no matter which script thread owns it.
			//NOTE: This should only be exercised by scripts that absolutely require such global behaviour, as this can result in conflicts between
			//concurrent script threads.
			CTheScripts::GetScriptHandlerMgr().RemoveScriptResource(CGameScriptResource::SCRIPT_RESOURCE_CAMERA, CameraIndex);
		}
		else
		{
			//Only destroy the camera if it's associated with the current script thread.
			CTheScripts::GetCurrentGtaScriptHandler()->RemoveScriptResource(CGameScriptResource::SCRIPT_RESOURCE_CAMERA, CameraIndex);
		}
	}

	void CommandDestroyAllCams(bool bShouldApplyAcrossAllThreads)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		if(bShouldApplyAcrossAllThreads)
		{
			//Destroy all script-controlled cameras, no matter which script thread owns them.
			//NOTE: This should only be exercised by scripts that absolutely require such global behaviour, as this can result in conflicts between
			//concurrent script threads.
			CTheScripts::GetScriptHandlerMgr().RemoveAllScriptResourcesOfType(CGameScriptResource::SCRIPT_RESOURCE_CAMERA);
		}
		else
		{
			//Only destroy the cameras associated with the current script thread.
			CTheScripts::GetCurrentGtaScriptHandler()->RemoveAllScriptResourcesOfType(CGameScriptResource::SCRIPT_RESOURCE_CAMERA);
		}
	}

	bool CommandDoesCamExist(int CameraIndex)
	{
		camBaseCamera* pCamera = camBaseCamera::GetCameraAtPoolIndex(CameraIndex);

		return (pCamera!=NULL);
	}
	
	void CommandSetCamActive(int CameraIndex, bool bActiveState)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		camBaseCamera* pCamera = GetCameraFromIndex(CameraIndex, "SET_CAM_ACTIVE");
		if(pCamera)
		{
			if (SCRIPT_VERIFY (camInterface::GetScriptDirector().IsCameraScriptControllable(CameraIndex), "SET_CAM_ACTIVE - Camera is not a script controllable "))
			{
				if(bActiveState)
				{
					camInterface::GetScriptDirector().ActivateCamera(CameraIndex);		//Inform the script director that a camera it may own is being activated.
				}
				else
				{
					camInterface::GetScriptDirector().DeactivateCamera(CameraIndex);	//Inform the script director that a camera it may own is being deactivated.
				}
			}
		
		}
	}

	bool CommandIsCamActive(int CameraIndex)
	{
		camBaseCamera* pCamera = GetCameraFromIndex(CameraIndex, "IS_CAM_ACTIVE");
		if (pCamera)
		{
			if(camInterface::GetScriptDirector().IsCameraActive(CameraIndex))
			{
				return true;
			}
		#if __BANK
			else if (pCamera->GetIsClassId(camFreeCamera::GetStaticClassId()) &&
					 camInterface::GetDebugDirector().IsFreeCamActive())
			{
				return true;
			}
		#endif // __BANK
		}
		return false;
	}

	bool CommandIsCamRendering (int CameraIndex)
	{
		camBaseCamera* pCamera = GetCameraFromIndex(CameraIndex, "IS_CAM_RENDERING");
		if(pCamera)
		{			
			if (SCRIPT_VERIFY (camInterface::GetScriptDirector().IsCameraScriptControllable(CameraIndex),"IS_CAM_RENDERING - Camera is not a script controllable " )) 
			{
				const bool isRendering = camInterface::IsDominantRenderedCamera(*pCamera);

				return isRendering;
			}
		}
	
		return false;
	}

	int CommandGetRenderingCam ()
	{
		int iCamIndex = INVALID_CAM_INDEX;
		
		const camBaseCamera* pCamera = camInterface::GetDominantRenderedCamera();
		
		if (pCamera)
		{
			int tempCamIndex = pCamera->GetPoolIndex();
			if (camInterface::GetScriptDirector().IsCameraScriptControllable(tempCamIndex))
			{
				iCamIndex = tempCamIndex;
			}
		}			
		
		return iCamIndex;
	}

	scrVector CommandGetCamCoord(int CameraIndex)
	{
		Vector3 vPos(VEC3_ZERO);

		camBaseCamera* pCamera = GetCameraFromIndex(CameraIndex, "GET_CAM_COORD");
		if(pCamera)
		{
			vPos = pCamera->GetFrame().GetPosition();
		}

		return vPos;
	}

	scrVector CommandGetCamRotation(int CameraIndex, int RotOrder)
	{
		Vector3 vRot(VEC3_ZERO);

		camBaseCamera* pCamera = GetCameraFromIndex(CameraIndex, "GET_CAM_ROT");
		if(pCamera)
		{
			const Matrix34& worldMatrix = pCamera->GetFrame().GetWorldMatrix();
			vRot = CScriptEulers::MatrixToEulers(worldMatrix, static_cast<EulerAngleOrder>(RotOrder));
			vRot *= RtoD;
		}

		return vRot;
	}

	float CommandGetCamFov(int CameraIndex)
	{
		float fov = 0.0f;

		camBaseCamera* pCamera = GetCameraFromIndex(CameraIndex, "GET_CAM_FOV");
		if(pCamera)
		{
			fov = pCamera->GetFrame().GetFov();
		}

		return fov;
	}

	float CommandGetCamNearClip(int CameraIndex)
	{
		float nearClip = 0.0f;

		camBaseCamera* pCamera = GetCameraFromIndex(CameraIndex, "GET_CAM_NEAR_CLIP");
		if(pCamera)
		{
			nearClip = pCamera->GetFrame().GetNearClip();
		}

		return nearClip;
	}

	float CommandGetCamFarClip(int CameraIndex)
	{
		float farClip = 0.0f;

		camBaseCamera* pCamera = GetCameraFromIndex(CameraIndex, "GET_CAM_FAR_CLIP");
		if(pCamera)
		{
			farClip = pCamera->GetFrame().GetFarClip();
		}

		return farClip;
	}

	float CommandGetCamNearDof(int CameraIndex)
	{
		float nearDof = 0.0f;

		camBaseCamera* pCamera = GetCameraFromIndex(CameraIndex, "GET_CAM_NEAR_DOF");
		if(pCamera)
		{
			nearDof = pCamera->GetFrame().GetNearInFocusDofPlane();
		}

		return nearDof;
	}

	float CommandGetCamFarDof(int CameraIndex)
	{
		float farDof = 0.0f;

		camBaseCamera* pCamera = GetCameraFromIndex(CameraIndex, "GET_CAM_FAR_DOF");
		if(pCamera)
		{
			farDof = pCamera->GetFrame().GetFarInFocusDofPlane();
		}

		return farDof;
	}

	float CommandGetCamDofStrength(int CameraIndex)
	{
		float dofStrength = 0.0f;

		camBaseCamera* pCamera = GetCameraFromIndex(CameraIndex, "GET_CAM_DOF_STRENGTH");
		if(pCamera)
		{
			dofStrength = pCamera->GetFrame().GetDofStrength();
		}

		return dofStrength;
	}

	void CommandGetCamDofPlanes(int CameraIndex, float& nearOutOfFocusPlane, float& nearInFocusPlane, float& farInFocusPlane, float& farOutOfFocusPlane)
	{
		camBaseCamera* pCamera = GetCameraFromIndex(CameraIndex, "GET_CAM_DOF_PLANES");
		if(pCamera)
		{
			Vector4 dofPlanes;
			pCamera->GetFrame().ComputeDofPlanes(dofPlanes);

			nearOutOfFocusPlane	= dofPlanes[camFrame::NEAR_OUT_OF_FOCUS_DOF_PLANE];
			nearInFocusPlane	= dofPlanes[camFrame::NEAR_IN_FOCUS_DOF_PLANE];
			farInFocusPlane		= dofPlanes[camFrame::FAR_IN_FOCUS_DOF_PLANE];
			farOutOfFocusPlane	= dofPlanes[camFrame::FAR_OUT_OF_FOCUS_DOF_PLANE];
		}
		else
		{
			nearOutOfFocusPlane = nearInFocusPlane = farInFocusPlane = farOutOfFocusPlane = 0.0f;
		}
	}

	bool CommandGetCamUseShallowDofMode(int CameraIndex)
	{
		bool shouldUseShallowDof = false;

		camBaseCamera* pCamera = GetCameraFromIndex(CameraIndex, "GET_CAM_USE_SHALLOW_DOF_MODE");
		if(pCamera)
		{
			shouldUseShallowDof = pCamera->GetFrame().GetFlags().IsFlagSet(camFrame::Flag_ShouldUseShallowDof);
		}

		return shouldUseShallowDof;
	}

	float CommandGetCamMotionBlurStrength(int CameraIndex)
	{
		float motionBlurStrength = 0.0f;

		camBaseCamera* pCamera = GetCameraFromIndex(CameraIndex, "GET_CAM_MOTION_BLUR_STRENGTH");
		if(pCamera)
		{
			motionBlurStrength = pCamera->GetFrame().GetMotionBlurStrength();
		}

		return motionBlurStrength;
	}

	void CommandSetCamParams (int CameraIndex, const scrVector &scrCamPos, const scrVector &scrCamRot, float fCamFov, int duration, int graphTypePos, int graphTypeRot, int RotOrder)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		Vector3 vPos (scrCamPos);
		Vector3 vRot (scrCamRot);

		camBaseCamera* pCamera = GetCameraFromIndex(CameraIndex, "SET_CAM_PARAMS");
		if(pCamera)
		{
			if (SCRIPT_VERIFY (camInterface::GetScriptDirector().IsCameraScriptControllable(CameraIndex), "SET_CAM_PARAMS - Camera is not a script controllable " )) 
			{
				camInterface::GetScriptDirector().SetCameraFrameParameters(CameraIndex, vPos, vRot * DtoR, fCamFov, duration, graphTypePos, graphTypeRot, RotOrder);
			}
		}
	}

	void CommandSetCamCoord(int CameraIndex, const scrVector &scrVecNewCoors)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		Vector3 vPos (scrVecNewCoors);

		camBaseCamera* pCamera = GetCameraFromIndex(CameraIndex, "SET_CAM_COORD");
		if(pCamera)
		{
			if (SCRIPT_VERIFY (camInterface::GetScriptDirector().IsCameraScriptControllable(CameraIndex), "SET_CAM_COORD - Camera is not a script controllable " )) 
			{
				//Terminate any existing interpolation, as the script wants to cut to the new position.
				pCamera->StopInterpolating();

				camFrame& cameraFrame = pCamera->GetFrameNonConst();
				cameraFrame.SetPosition(vPos);
				//The camera position is being modified, so we must report a cut. It is unlikely that a script would manually animate a camera
				//using this interface.
				cameraFrame.GetFlags().SetFlag(camFrame::Flag_HasCutPosition);
			}
		}
	}

	void CommandSetCamRotation(int CameraIndex, const scrVector &scrVecNewRot, int RotOrder)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		camBaseCamera* pCamera = GetCameraFromIndex(CameraIndex, "SET_CAM_ROT");
		if(pCamera)
		{
			if (SCRIPT_VERIFY (camInterface::GetScriptDirector().IsCameraScriptControllable(CameraIndex), "SET_CAM_ROT - Camera is not a script controllable " )) 
			{
				Vector3 vRot(scrVecNewRot);

				Matrix34 worldMatrix;
				CScriptEulers::MatrixFromEulers(worldMatrix, vRot * DtoR, static_cast<EulerAngleOrder>(RotOrder));

				//Terminate any existing interpolation, as the script wants to cut to the new rotation.
				pCamera->StopInterpolating();

				camFrame& cameraFrame = pCamera->GetFrameNonConst();
				cameraFrame.SetWorldMatrix(worldMatrix);

				//The camera orientation is being modified, so we must report a cut. It is unlikely that a script would manually animate a camera
				//using this interface.
				cameraFrame.GetFlags().SetFlag(camFrame::Flag_HasCutOrientation);
			}
		}
	}

	void CommandSetCamFov(int CameraIndex, float fov)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();

		camBaseCamera* pCamera = GetCameraFromIndex(CameraIndex, "SET_CAM_FOV");
		if(pCamera)
		{
			if(SCRIPT_VERIFY(camInterface::GetScriptDirector().IsCameraScriptControllable(CameraIndex), "SET_CAM_FOV - Camera is not a script controllable "))
			{
				//Terminate any existing interpolation, as the script wants to cut to the new FOV.
				pCamera->StopInterpolating();

				pCamera->GetFrameNonConst().SetFov(fov);
			}
		}
	}

	void CommandSetCamNearClip(int CameraIndex, float nearClip)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		camBaseCamera* pCamera = GetCameraFromIndex(CameraIndex, "SET_CAM_NEAR_CLIP");
		if(pCamera)
		{
			if(SCRIPT_VERIFY(camInterface::GetScriptDirector().IsCameraScriptControllable(CameraIndex), "SET_CAM_NEAR_CLIP - Camera is not a script controllable "))
			{
				cameraDebugf3("%u SET_CAM_NEAR_CLIP %i %.3f %s", fwTimer::GetFrameCount(), CameraIndex, nearClip, CTheScripts::GetCurrentScriptNameAndProgramCounter());

				pCamera->GetFrameNonConst().SetNearClip(nearClip);
			}
		}
	}

	void CommandSetCamFarClip(int CameraIndex, float farClip)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		camBaseCamera* pCamera = GetCameraFromIndex(CameraIndex, "SET_CAM_FAR_CLIP");
		if(pCamera)
		{
			if(SCRIPT_VERIFY(camInterface::GetScriptDirector().IsCameraScriptControllable(CameraIndex), "SET_CAM_FAR_CLIP - Camera is not a script controllable "))
			{
				pCamera->GetFrameNonConst().SetFarClip(farClip);
				
				if(pCamera->GetIsClassId(camAnimatedCamera::GetStaticClassId()))
				{
					camAnimatedCamera* pAnimatedCamera = static_cast<camAnimatedCamera*>(pCamera);
					pAnimatedCamera->SetScriptShouldOverrideFarClip(true); 
				}
			}
		}
	}

	void CommandForceCamFarClip(int CameraIndex, float farClip)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		camBaseCamera* pCamera = GetCameraFromIndex(CameraIndex, "FORCE_CAM_FAR_CLIP");
		if(pCamera)
		{
			if(SCRIPT_VERIFY(camInterface::GetScriptDirector().IsCameraScriptControllable(CameraIndex), "FORCE_CAM_FAR_CLIP - Camera is not a script controllable "))
			{
				pCamera->GetFrameNonConst().SetFarClip(farClip);

				if(pCamera->GetIsClassId(camScriptedCamera::GetStaticClassId()))
				{
					camScriptedCamera* pScriptedCamera = static_cast<camScriptedCamera*>(pCamera);
					pScriptedCamera->SetShouldOverrideFarClip(true); 
				}
				else if(pCamera->GetIsClassId(camAnimatedCamera::GetStaticClassId()))
				{
					camAnimatedCamera* pAnimatedCamera = static_cast<camAnimatedCamera*>(pCamera);
					pAnimatedCamera->SetScriptShouldOverrideFarClip(true); 
				}
			}
		}
	}

	void CommandSetCamMotionBlurStrength(int CameraIndex, float strength)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		camBaseCamera* pCamera = GetCameraFromIndex(CameraIndex, "SET_CAM_MOTION_BLUR_STRENGTH");
		if(pCamera)
		{
			if(SCRIPT_VERIFY(camInterface::GetScriptDirector().IsCameraScriptControllable(CameraIndex), "SET_CAM_MOTION_BLUR_STRENGTH - Camera is not a script controllable "))
			{
				pCamera->GetFrameNonConst().SetMotionBlurStrength(strength);
			}
		}
	}

	void CommandSetCamNearDof(int CameraIndex, float nearDof)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		camBaseCamera* pCamera = GetCameraFromIndex(CameraIndex, "SET_CAM_NEAR_DOF");
		if(pCamera)
		{
			if(SCRIPT_VERIFY(camInterface::GetScriptDirector().IsCameraScriptControllable(CameraIndex), "SET_CAM_NEAR_DOF - Camera is not a script controllable "))
			{
				pCamera->GetFrameNonConst().SetNearInFocusDofPlane(nearDof);
			}
		}
	}

	void CommandSetCamFarDof(int CameraIndex, float farDof)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		camBaseCamera* pCamera = GetCameraFromIndex(CameraIndex, "SET_CAM_FAR_DOF");
		if(pCamera)
		{
			if(SCRIPT_VERIFY(camInterface::GetScriptDirector().IsCameraScriptControllable(CameraIndex), "SET_CAM_FAR_DOF - Camera is not a script controllable "))
			{
				pCamera->GetFrameNonConst().SetFarInFocusDofPlane(farDof);
			}
		}
	}

	void CommandSetCamDofStrength(int CameraIndex, float dofStrength)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		camBaseCamera* pCamera = GetCameraFromIndex(CameraIndex, "SET_CAM_DOF_STRENGTH");
		if(pCamera)
		{
			if(SCRIPT_VERIFY(camInterface::GetScriptDirector().IsCameraScriptControllable(CameraIndex), "SET_CAM_DOF_STRENGTH - Camera is not a script controllable "))
			{
				pCamera->GetFrameNonConst().SetDofStrength(dofStrength);
			}
		}
	}

	void CommandSetCamDofPlanes(int CameraIndex, float nearOutOfFocusPlane, float nearInFocusPlane, float farInFocusPlane, float farOutOfFocusPlane)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		camBaseCamera* pCamera = GetCameraFromIndex(CameraIndex, "SET_CAM_DOF_PLANES");
		if(pCamera)
		{
			if(SCRIPT_VERIFY(camInterface::GetScriptDirector().IsCameraScriptControllable(CameraIndex), "SET_CAM_DOF_PLANES - Camera is not a script controllable "))
			{
				Vector4 dofPlanes;
				dofPlanes[camFrame::NEAR_OUT_OF_FOCUS_DOF_PLANE]	= nearOutOfFocusPlane;
				dofPlanes[camFrame::NEAR_IN_FOCUS_DOF_PLANE]		= nearInFocusPlane;
				dofPlanes[camFrame::FAR_IN_FOCUS_DOF_PLANE]			= farInFocusPlane;
				dofPlanes[camFrame::FAR_OUT_OF_FOCUS_DOF_PLANE]		= farOutOfFocusPlane;

				pCamera->GetFrameNonConst().SetDofPlanes(dofPlanes);
			}
		}
	}

	void CommandSetCamUseShallowDofMode(int CameraIndex, bool state)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		camBaseCamera* pCamera = GetCameraFromIndex(CameraIndex, "SET_CAM_USE_SHALLOW_DOF_MODE");
		if(pCamera)
		{
			if(SCRIPT_VERIFY(camInterface::GetScriptDirector().IsCameraScriptControllable(CameraIndex), "SET_CAM_USE_SHALLOW_DOF_MODE - Camera is not a script controllable "))
			{
				pCamera->GetFrameNonConst().GetFlags().ChangeFlag(camFrame::Flag_ShouldUseShallowDof, state);
			}
		}
	}

	//Use the high-quality DOF effect that handles four planes.
	void CommandUseHiDof()
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		camInterface::GetScriptDirector().SetHighQualityDofThisUpdate(true); 
	}

	void CommandUseHiDofOnSyncedSceneThisUpdate()
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		camInterface::GetSyncedSceneDirector().SetHighQualityDofThisUpdate(true);
	}

	void CommandSetDofOverriddenFocusDistance(int CameraIndex, float distance)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		camBaseCamera* pCamera = GetCameraFromIndex(CameraIndex, "SET_CAM_DOF_OVERRIDDEN_FOCUS_DISTANCE");
		if(pCamera)
		{
			if(SCRIPT_VERIFY(camInterface::GetScriptDirector().IsCameraScriptControllable(CameraIndex), "SET_CAM_DOF_OVERRIDDEN_FOCUS_DISTANCE - Camera is not a script controllable "))
			{
				distance = Clamp(distance, 0.0f, 99999.0f);

				if(pCamera->GetIsClassId(camScriptedCamera::GetStaticClassId()))
				{
					static_cast<camScriptedCamera*>(pCamera)->SetOverriddenDofFocusDistance(distance);
				}
				else if(pCamera->GetIsClassId(camAnimatedCamera::GetStaticClassId()))
				{
					static_cast<camAnimatedCamera*>(pCamera)->SetOverriddenDofFocusDistance(distance);
				}
			}
		}
	}

	void CommandSetDofOverriddenFocusDistanceBlendLevel(int CameraIndex, float blendLevel)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		camBaseCamera* pCamera = GetCameraFromIndex(CameraIndex, "SET_CAM_DOF_OVERRIDDEN_FOCUS_DISTANCE_BLEND_LEVEL");
		if(pCamera)
		{
			if(SCRIPT_VERIFY(camInterface::GetScriptDirector().IsCameraScriptControllable(CameraIndex), "SET_CAM_DOF_OVERRIDDEN_FOCUS_DISTANCE_BLEND_LEVEL - Camera is not a script controllable "))
			{
				blendLevel = Clamp(blendLevel, 0.0f, 1.0f);

				if(pCamera->GetIsClassId(camScriptedCamera::GetStaticClassId()))
				{
					static_cast<camScriptedCamera*>(pCamera)->SetOverriddenDofFocusDistanceBlendLevel(blendLevel);
				}
				else if(pCamera->GetIsClassId(camAnimatedCamera::GetStaticClassId()))
				{
					static_cast<camAnimatedCamera*>(pCamera)->SetOverriddenDofFocusDistanceBlendLevel(blendLevel);
				}
			}
		}
	}

	camDepthOfFieldSettingsMetadata* GetOverriddenDepthOfFieldSettingsFromCameraIndex(int CameraIndex, const char* commandName)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		camDepthOfFieldSettingsMetadata* overriddenDofSettings = NULL;

		camBaseCamera* pCamera = GetCameraFromIndex(CameraIndex, commandName);
		if(pCamera)
		{
			if(SCRIPT_VERIFY_TWO_STRINGS(camInterface::GetScriptDirector().IsCameraScriptControllable(CameraIndex), commandName, " - Camera is not a script controllable "))
			{
				if(pCamera->GetIsClassId(camScriptedCamera::GetStaticClassId()))
				{
					overriddenDofSettings = static_cast<camScriptedCamera*>(pCamera)->GetOverriddenDofSettings();
				}
				else if(pCamera->GetIsClassId(camAnimatedCamera::GetStaticClassId()))
				{
					overriddenDofSettings = static_cast<camAnimatedCamera*>(pCamera)->GetOverriddenDofSettings();
				}

				SCRIPT_ASSERT_TWO_STRINGS(overriddenDofSettings, commandName, " - Camera has no depth of field settings to be overridden");
			}
		}

		return overriddenDofSettings;
	}

	void CommandSetDofFocusDistanceGridScaling(int CameraIndex, float gridScalingX, float gridScalingY)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		camDepthOfFieldSettingsMetadata* overriddenDofSettings = GetOverriddenDepthOfFieldSettingsFromCameraIndex(CameraIndex, "SET_CAM_DOF_FOCUS_DISTANCE_GRID_SCALING");
		if(overriddenDofSettings)
		{
			gridScalingX = Clamp(gridScalingX, 0.0f, 1.0f);
			gridScalingY = Clamp(gridScalingY, 0.0f, 1.0f);

			const Vector2 gridScaling(gridScalingX, gridScalingY);
			overriddenDofSettings->m_FocusDistanceGridScaling.Set(gridScaling);
		}
	}

	void CommandSetDofSubjectMagnificationPowerFactorNearFar(int CameraIndex, float powerFactorNear, float powerFactorFar)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		camDepthOfFieldSettingsMetadata* overriddenDofSettings = GetOverriddenDepthOfFieldSettingsFromCameraIndex(CameraIndex, "SET_CAM_DOF_SUBJECT_MAGNIFICATION_POWER_FACTOR_NEAR_FAR");
		if(overriddenDofSettings)
		{
			powerFactorNear	= Clamp(powerFactorNear, 0.0f, 1.0f);
			powerFactorFar	= Clamp(powerFactorFar, 0.0f, 1.0f);

			const Vector2 powerFactorNearFar(powerFactorNear, powerFactorFar);
			overriddenDofSettings->m_SubjectMagnificationPowerFactorNearFar.Set(powerFactorNearFar);
		}
	}

	void CommandSetDofMaxPixelDepth(int CameraIndex, float depth)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		camDepthOfFieldSettingsMetadata* overriddenDofSettings = GetOverriddenDepthOfFieldSettingsFromCameraIndex(CameraIndex, "SET_CAM_DOF_MAX_PIXEL_DEPTH");
		if(overriddenDofSettings)
		{
			overriddenDofSettings->m_MaxPixelDepth = Clamp(depth, 0.0f, 99999.0f);
		}
	}

	void CommandSetDofPixelDepthPowerFactor(int CameraIndex, float powerFactor)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		camDepthOfFieldSettingsMetadata* overriddenDofSettings = GetOverriddenDepthOfFieldSettingsFromCameraIndex(CameraIndex, "SET_CAM_DOF_PIXEL_DEPTH_POWER_FACTOR");
		if(overriddenDofSettings)
		{
			overriddenDofSettings->m_PixelDepthPowerFactor = Clamp(powerFactor, 0.0f, 1.0f);
		}
	}

	void CommandSetDofFNumberOfLens(int CameraIndex, float fNumber)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		camDepthOfFieldSettingsMetadata* overriddenDofSettings = GetOverriddenDepthOfFieldSettingsFromCameraIndex(CameraIndex, "SET_CAM_DOF_FNUMBER_OF_LENS");
		if(overriddenDofSettings)
		{
			overriddenDofSettings->m_FNumberOfLens = Clamp(fNumber, 0.5f, 256.0f);
		}
	}

	void CommandSetDofFocalLengthMultiplier(int CameraIndex, float multiplier)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		camDepthOfFieldSettingsMetadata* overriddenDofSettings = GetOverriddenDepthOfFieldSettingsFromCameraIndex(CameraIndex, "SET_CAM_DOF_FOCAL_LENGTH_MULTIPLIER");
		if(overriddenDofSettings)
		{
			overriddenDofSettings->m_FocalLengthMultiplier = Clamp(multiplier, 0.1f, 10.0f);
		}
	}

	void CommandSetDofFocusDistanceBias(int CameraIndex, float distanceBias)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		camDepthOfFieldSettingsMetadata* overriddenDofSettings = GetOverriddenDepthOfFieldSettingsFromCameraIndex(CameraIndex, "SET_CAM_DOF_FOCUS_DISTANCE_BIAS");
		if(overriddenDofSettings)
		{
			overriddenDofSettings->m_FocusDistanceBias = Clamp(distanceBias, -100.0f, 100.0f);
		}
	}

	void CommandSetDofMaxNearInFocusDistance(int CameraIndex, float distance)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		camDepthOfFieldSettingsMetadata* overriddenDofSettings = GetOverriddenDepthOfFieldSettingsFromCameraIndex(CameraIndex, "SET_CAM_DOF_MAX_NEAR_IN_FOCUS_DISTANCE");
		if(overriddenDofSettings)
		{
			overriddenDofSettings->m_MaxNearInFocusDistance = Clamp(distance, 0.0f, 99999.0f);
		}
	}

	void CommandSetDofMaxNearInFocusDistanceBlendLevel(int CameraIndex, float blendLevel)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		camDepthOfFieldSettingsMetadata* overriddenDofSettings = GetOverriddenDepthOfFieldSettingsFromCameraIndex(CameraIndex, "SET_CAM_DOF_MAX_NEAR_IN_FOCUS_DISTANCE_BLEND_LEVEL");
		if(overriddenDofSettings)
		{
			overriddenDofSettings->m_MaxNearInFocusDistanceBlendLevel = Clamp(blendLevel, 0.0f, 1.0f);
		}
	}

	void CommandSetDofMaxBlurRadiusAtNearInFocusLimit(int CameraIndex, float radius)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		camDepthOfFieldSettingsMetadata* overriddenDofSettings = GetOverriddenDepthOfFieldSettingsFromCameraIndex(CameraIndex, "SET_CAM_DOF_MAX_BLUR_RADIUS_AT_NEAR_IN_FOCUS_LIMIT");
		if(overriddenDofSettings)
		{
			overriddenDofSettings->m_MaxBlurRadiusAtNearInFocusLimit = Clamp(radius, 0.0f, 30.0f);
		}
	}

	void CommandSetDofFocusDistanceIncreaseSpringConstant(int CameraIndex, float springConstant)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		camDepthOfFieldSettingsMetadata* overriddenDofSettings = GetOverriddenDepthOfFieldSettingsFromCameraIndex(CameraIndex, "SET_CAM_DOF_FOCUS_DISTANCE_INCREASE_SPRING_CONSTANT");
		if(overriddenDofSettings)
		{
			overriddenDofSettings->m_FocusDistanceIncreaseSpringConstant = Clamp(springConstant, 0.0f, 1000.0f);
		}
	}

	void CommandSetDofFocusDistanceDecreaseSpringConstant(int CameraIndex, float springConstant)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		camDepthOfFieldSettingsMetadata* overriddenDofSettings = GetOverriddenDepthOfFieldSettingsFromCameraIndex(CameraIndex, "SET_CAM_DOF_FOCUS_DISTANCE_DECREASE_SPRING_CONSTANT");
		if(overriddenDofSettings)
		{
			overriddenDofSettings->m_FocusDistanceDecreaseSpringConstant = Clamp(springConstant, 0.0f, 1000.0f);
		}
	}

	void CommandSetDofShouldFocusOnLookAtTarget(int CameraIndex, bool state)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		camDepthOfFieldSettingsMetadata* overriddenDofSettings = GetOverriddenDepthOfFieldSettingsFromCameraIndex(CameraIndex, "SET_CAM_DOF_SHOULD_FOCUS_ON_LOOK_AT_TARGET");
		if(overriddenDofSettings)
		{
			overriddenDofSettings->m_ShouldFocusOnLookAtTarget = state;
		}
	}

	void CommandSetDofShouldFocusOnAttachParent(int CameraIndex, bool state)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		camDepthOfFieldSettingsMetadata* overriddenDofSettings = GetOverriddenDepthOfFieldSettingsFromCameraIndex(CameraIndex, "SET_CAM_DOF_SHOULD_FOCUS_ON_ATTACH_PARENT");
		if(overriddenDofSettings)
		{
			overriddenDofSettings->m_ShouldFocusOnAttachParent = state;
		}
	}

	void CommandSetDofShouldKeepLookAtTargetInFocus(int CameraIndex, bool state)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		camDepthOfFieldSettingsMetadata* overriddenDofSettings = GetOverriddenDepthOfFieldSettingsFromCameraIndex(CameraIndex, "SET_CAM_DOF_SHOULD_KEEP_LOOK_AT_TARGET_IN_FOCUS");
		if(overriddenDofSettings)
		{
			overriddenDofSettings->m_ShouldKeepLookAtTargetInFocus = state;
		}
	}
	
	void CommandSetDofShouldKeepAttachParentInFocus(int CameraIndex, bool state)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		camDepthOfFieldSettingsMetadata* overriddenDofSettings = GetOverriddenDepthOfFieldSettingsFromCameraIndex(CameraIndex, "SET_CAM_DOF_SHOULD_KEEP_ATTACH_PARENT_IN_FOCUS");
		if(overriddenDofSettings)
		{
			overriddenDofSettings->m_ShouldKeepAttachParentInFocus = state;
		}
	}

	void CommandSetDofShouldMeasurePostAlphaPixelDepth(int CameraIndex, bool state)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		camDepthOfFieldSettingsMetadata* overriddenDofSettings = GetOverriddenDepthOfFieldSettingsFromCameraIndex(CameraIndex, "SET_CAM_DOF_SHOULD_MEASURE_POST_ALPHA_PIXEL_DEPTH");
		if(overriddenDofSettings)
		{
			overriddenDofSettings->m_ShouldMeasurePostAlphaPixelDepth = state;
		}
	}

	void CommandAttachCamToEntity(int CameraIndex, int EntityIndex, const scrVector &scrCamOffset, bool bRelativeOffset)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		Vector3 VecCamOffset (scrCamOffset);

		camBaseCamera* pCamera = GetCameraFromIndex(CameraIndex, "ATTACH_CAM_TO_ENTITY");
		if(pCamera)
		{
			if (SCRIPT_VERIFY(camInterface::GetScriptDirector().IsCameraScriptControllable(CameraIndex), "ATTACH_CAM_TO_ENTITY - Camera is not script controlled" ))
			{
				const CEntity *pEntity	= CTheScripts::GetEntityToQueryFromGUID<CEntity>(EntityIndex, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);

				if (pEntity)	
				{
					if(pCamera && pCamera->GetIsClassId(camScriptedCamera::GetStaticClassId()))
					{
						camScriptedCamera* scriptedCamera = static_cast<camScriptedCamera*>(pCamera);
						scriptedCamera->AttachTo(pEntity);
						scriptedCamera->SetAttachBoneTag(-1);
						scriptedCamera->SetAttachOffset(VecCamOffset);
						scriptedCamera->SetAttachOffsetRelativeToMatrixState(bRelativeOffset);
					}		
				}
			}
		}
	}

	void CommandAttachCamToPedBone(int CameraIndex, int PedIndex, int BoneTag, const scrVector &scrCamOffset, bool bRelativeOffset)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		Vector3 VecCamOffset (scrCamOffset);

		camBaseCamera* pCamera = GetCameraFromIndex(CameraIndex, "ATTACH_CAM_TO_PED_BONE");
		if(pCamera)
		{
			if (SCRIPT_VERIFY(camInterface::GetScriptDirector().IsCameraScriptControllable(CameraIndex), "ATTACH_CAM_TO_PED_BONE - Camera is not script controlled" ))
			{
				const CPed *pPed	= CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);
				if (pPed)
				{
					if(pCamera && pCamera->GetIsClassId(camScriptedCamera::GetStaticClassId()))
					{
						camScriptedCamera* scriptedCamera = static_cast<camScriptedCamera*>(pCamera);
						scriptedCamera->AttachTo(pPed);
						scriptedCamera->SetAttachBoneTag(BoneTag);
						scriptedCamera->SetAttachOffset(VecCamOffset);
						scriptedCamera->SetAttachOffsetRelativeToMatrixState(bRelativeOffset);
					}		
				}
			}
		}
	}

	void CommandHardAttachCamToPedBone(int CameraIndex, int PedIndex, int BoneTag, const scrVector &scrCamRotationOffset, const scrVector &scrCamOffset, bool bRelativeOffset)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		Vector3 VecCamOffset(scrCamOffset);
		Vector3 VecCamRotationOffset(scrCamRotationOffset);

		camBaseCamera* pCamera = GetCameraFromIndex(CameraIndex, "HARD_ATTACH_CAM_TO_PED_BONE");
		if(pCamera)
		{
			if (SCRIPT_VERIFY(camInterface::GetScriptDirector().IsCameraScriptControllable(CameraIndex), "HARD_ATTACH_CAM_TO_PED_BONE - Camera is not script controlled" ))
			{
				const CPed *pPed	= CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);
				if (pPed)
				{
					if(pCamera && pCamera->GetIsClassId(camScriptedCamera::GetStaticClassId()))
					{
						camScriptedCamera* scriptedCamera = static_cast<camScriptedCamera*>(pCamera);
						scriptedCamera->AttachTo(pPed);
						scriptedCamera->SetAttachBoneTag(BoneTag, true);
						scriptedCamera->SetAttachRotationOffset(VecCamRotationOffset);
						scriptedCamera->SetAttachOffset(VecCamOffset);
						scriptedCamera->SetAttachOffsetRelativeToMatrixState(bRelativeOffset);
					}		
				}
			}
		}
	}

	void CommandHardAttachCamToEntity(int CameraIndex, int EntityIndex, const scrVector &scrCamRotationOffset, const scrVector &scrCamOffset, bool bRelativeOffset)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		Vector3 VecCamOffset(scrCamOffset);
		Vector3 VecCamRotationOffset(scrCamRotationOffset);

		camBaseCamera* pCamera = GetCameraFromIndex(CameraIndex, "HARD_ATTACH_CAM_TO_ENTITY");
		if(pCamera)
		{
			if (SCRIPT_VERIFY(camInterface::GetScriptDirector().IsCameraScriptControllable(CameraIndex), "HARD_ATTACH_CAM_TO_ENTITY - Camera is not script controlled" ))
			{
				const CEntity *pEntity	= CTheScripts::GetEntityToQueryFromGUID<CEntity>(EntityIndex, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);
				if (pEntity)
				{
					if(pCamera && pCamera->GetIsClassId(camScriptedCamera::GetStaticClassId()))
					{
						camScriptedCamera* scriptedCamera = static_cast<camScriptedCamera*>(pCamera);
						scriptedCamera->AttachTo(pEntity);
						scriptedCamera->SetAttachBoneTag(-1, true);
						scriptedCamera->SetAttachRotationOffset(VecCamRotationOffset);
						scriptedCamera->SetAttachOffset(VecCamOffset);
						scriptedCamera->SetAttachOffsetRelativeToMatrixState(bRelativeOffset);
					}		
				}
			}
		}
	}

	void CommandAttachCamToVehicleBone(int CameraIndex, int VehicleIndex, int vehicleBone, bool bHardAttachment, const scrVector &scrCamRotationOffset, const scrVector &scrCamOffset, bool bRelativeOffset)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		Vector3 VecCamOffset(scrCamOffset);
		Vector3 VecCamRotationOffset(scrCamRotationOffset);

		camBaseCamera* pCamera = GetCameraFromIndex(CameraIndex, "ATTACH_CAM_TO_VEHICLE_BONE");
		if(pCamera)
		{
			if (SCRIPT_VERIFY(camInterface::GetScriptDirector().IsCameraScriptControllable(CameraIndex), "ATTACH_CAM_TO_VEHICLE_BONE - Camera is not script controlled" ))
			{
				const CVehicle *pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(VehicleIndex, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);
				if (pVehicle)
				{
					if(pCamera && pCamera->GetIsClassId(camScriptedCamera::GetStaticClassId()))
					{
						camScriptedCamera* scriptedCamera = static_cast<camScriptedCamera*>(pCamera);
						scriptedCamera->AttachTo(pVehicle);
						scriptedCamera->SetAttachBoneTag(vehicleBone, bHardAttachment);
						scriptedCamera->SetAttachRotationOffset(VecCamRotationOffset);
						scriptedCamera->SetAttachOffset(VecCamOffset);
						scriptedCamera->SetAttachOffsetRelativeToMatrixState(bRelativeOffset);
					}		
				}
			}
		}
	}

	void CommandDetachCam(int CameraIndex)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		camBaseCamera* pCamera = GetCameraFromIndex(CameraIndex, "DETTACH_CAM");
		if(pCamera)
		{
			if (SCRIPT_VERIFY(camInterface::GetScriptDirector().IsCameraScriptControllable(CameraIndex),"DETTACH_CAM - Camera is not script controlled" ))
			{
				if(pCamera->GetIsClassId(camScriptedCamera::GetStaticClassId()))		
				{
					camScriptedCamera* scriptedCamera = static_cast<camScriptedCamera*>(pCamera);
					scriptedCamera->AttachTo(NULL);
				}
			}
		}
	}

	// roll

	void CommandSetCamInheritRollObject(int CameraIndex, int ObjectIndex)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		camBaseCamera* pCamera = GetCameraFromIndex(CameraIndex, "SET_CAM_INHERIT_OBJECT_ROLL");
		if(pCamera)
		{
			if (SCRIPT_VERIFY(camInterface::GetScriptDirector().IsCameraScriptControllable(CameraIndex),"SET_CAM_INHERIT_OBJECT_ROLL - Camera is not script controlled" ))
			{
				const CObject* pObject	= CTheScripts::GetEntityToQueryFromGUID<CObject>(ObjectIndex, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);
				if (pObject)
				{
					if(pCamera && pCamera->GetIsClassId(camScriptedCamera::GetStaticClassId()))
					{
						camScriptedCamera* scriptedCamera = static_cast<camScriptedCamera*>(pCamera);
						scriptedCamera->SetRollTarget(pObject);
					}
				}
			}
		}
	}

	void CommandSetCamInheritRollVehicle(int CameraIndex, int VehicleIndex)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		camBaseCamera* pCamera = GetCameraFromIndex(CameraIndex, "SET_CAM_INHERIT_VEHICLE_ROLL");
		if(pCamera)
		{
			if (SCRIPT_VERIFY(camInterface::GetScriptDirector().IsCameraScriptControllable(CameraIndex),"SET_CAM_INHERIT_VEHICLE_ROLL - Camera is not script controlled" ))
			{
				const CVehicle* pVehicle	= CTheScripts::GetEntityToQueryFromGUID<CVehicle>(VehicleIndex, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);
				if (pVehicle)
				{
					if(pCamera && pCamera->GetIsClassId(camScriptedCamera::GetStaticClassId()))
					{
						camScriptedCamera* scriptedCamera = static_cast<camScriptedCamera*>(pCamera);
						scriptedCamera->SetRollTarget(pVehicle);
					}
				}
			}
		}
	}

	void CommandStopInheritedRoll (int CameraIndex)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		camBaseCamera* pCamera = GetCameraFromIndex(CameraIndex, "STOP_CAM_INHERIT_ROLL");
		if(pCamera)
		{
			if (SCRIPT_VERIFY(camInterface::GetScriptDirector().IsCameraScriptControllable(CameraIndex),"STOP_CAM_INHERIT_ROLL - Camera is not script controlled" ))
			{
				if(pCamera && pCamera->GetIsClassId(camScriptedCamera::GetStaticClassId()))
				{
					camScriptedCamera* scriptedCamera = static_cast<camScriptedCamera*>(pCamera);
					scriptedCamera->ClearRollTarget();
				}
			}
		}
	}



	//Pointing 

	void CommandPointCamAtCoord(int CameraIndex, const scrVector &scrVecCoors)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		Vector3 vecPos (scrVecCoors);

		camBaseCamera* pCamera = GetCameraFromIndex(CameraIndex, "POINT_CAM_AT_COORD");
		if(pCamera)
		{
			if (SCRIPT_VERIFY(camInterface::GetScriptDirector().IsCameraScriptControllable(CameraIndex),"POINT_CAM_AT_COORD - Camera is not script controlled" ))
			{
				if(pCamera && pCamera->GetIsClassId(camScriptedCamera::GetStaticClassId()))
				{
					camScriptedCamera* scriptedCamera = static_cast<camScriptedCamera*>(pCamera);
					scriptedCamera->LookAt(vecPos);
				}
			}
		}
	}

	void CommandPointCamAtCam(int LookingCameraIndex, int LookedAtCameraIndex)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		camBaseCamera* pLookingCamera = GetCameraFromIndex(LookingCameraIndex, "POINT_CAM_AT_CAM");
		camBaseCamera* pLookedAtCamera = GetCameraFromIndex(LookedAtCameraIndex, "POINT_CAM_AT_CAM");

		if(pLookingCamera && pLookedAtCamera)
		{
			if (SCRIPT_VERIFY(camInterface::GetScriptDirector().IsCameraScriptControllable(LookingCameraIndex),"POINT_CAM_AT_CAM - Camera is not script controlled" ))
			{
				if (SCRIPT_VERIFY(camInterface::GetScriptDirector().IsCameraScriptControllable(LookedAtCameraIndex),"POINT_CAM_AT_CAM - Camera is not script controlled" ))
				{
					if(pLookingCamera->GetIsClassId(camScriptedCamera::GetStaticClassId()) && pLookedAtCamera->GetIsClassId(camScriptedCamera::GetStaticClassId()))
					{
						camScriptedCamera* scriptedLookingCamera = static_cast<camScriptedCamera*>(pLookingCamera);
						scriptedLookingCamera->LookAt(pLookedAtCamera);
					}
				}
			}
		}
	}

	void CommandPointCamAtEntity(int CameraIndex, int EntityIndex, const scrVector &scrCamOffset, bool bRelativeOffset)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		Vector3 vecOffset (scrCamOffset);

		camBaseCamera* pCamera = GetCameraFromIndex(CameraIndex, "POINT_CAM_AT_ENTITY");
		if(pCamera)
		{
			if (SCRIPT_VERIFY(camInterface::GetScriptDirector().IsCameraScriptControllable(CameraIndex),"POINT_CAM_AT_ENTITY - Camera is not script controlled" ))
			{
				const CEntity *pEntity	= CTheScripts::GetEntityToQueryFromGUID<CEntity>(EntityIndex, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);
				if (pEntity)
				{
					if(pCamera && pCamera->GetIsClassId(camScriptedCamera::GetStaticClassId()))
					{
						camScriptedCamera* scriptedCamera = static_cast<camScriptedCamera*>(pCamera);
						scriptedCamera->LookAt(pEntity);
						scriptedCamera->SetLookAtBoneTag(-1);
						scriptedCamera->SetLookAtOffset(vecOffset);
						scriptedCamera->SetLookAtOffsetRelativeToMatrixState(bRelativeOffset);
					}
				}
			}
		}
	}

	void CommandPointCamAtPedBone(int CameraIndex, int PedIndex, int BoneTag, const scrVector &scrCamOffset, bool bRelativeOffset)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		Vector3 vecOffset (scrCamOffset);

		camBaseCamera* pCamera = GetCameraFromIndex(CameraIndex, "POINT_CAM_AT_PED_BONE");
		if(pCamera)
		{
			if (SCRIPT_VERIFY(camInterface::GetScriptDirector().IsCameraScriptControllable(CameraIndex),"POINT_CAM_AT_PED_BONE - Camera is not script controlled" ))
			{
				const CPed* pPed	= CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);
				if (pPed)
				{
					if(pCamera && pCamera->GetIsClassId(camScriptedCamera::GetStaticClassId()))
					{
						camScriptedCamera* scriptedCamera = static_cast<camScriptedCamera*>(pCamera);
						scriptedCamera->LookAt(pPed);
						scriptedCamera->SetLookAtBoneTag(BoneTag);
						scriptedCamera->SetLookAtOffset(vecOffset);
						scriptedCamera->SetLookAtOffsetRelativeToMatrixState(bRelativeOffset);
					}
				}
			}
		}
	}

	void CommandStopCamPointing(int CameraIndex)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		camBaseCamera* pCamera = GetCameraFromIndex(CameraIndex, "STOP_CAM_POINTING");
		if(pCamera)
		{
			if (SCRIPT_VERIFY(camInterface::GetScriptDirector().IsCameraScriptControllable(CameraIndex),"STOP_CAM_POINTING - Camera is not script controlled" ))
			{
				if(pCamera && pCamera->GetIsClassId(camScriptedCamera::GetStaticClassId()))
				{
					camScriptedCamera* scriptedCamera = static_cast<camScriptedCamera*>(pCamera);
					scriptedCamera->ClearLookAtTarget();
				}
			}
		}
	}

	void CommandSetCamAffectsAiming(int CameraIndex, bool state)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		camBaseCamera* pCamera = GetCameraFromIndex(CameraIndex, "SET_CAM_AFFECTS_AIMING");
		if(pCamera)
		{
			if(SCRIPT_VERIFY(camInterface::GetScriptDirector().IsCameraScriptControllable(CameraIndex),
				"SET_CAM_AFFECTS_AIMING - Camera is not script controlled"))
			{
				if(pCamera && pCamera->GetIsClassId(camScriptedCamera::GetStaticClassId()))
				{
					camScriptedCamera* scriptedCamera = static_cast<camScriptedCamera*>(pCamera);
					scriptedCamera->SetShouldAffectAiming(state);
				}
			}
		}
	}

	void CommandSetCamControlsMiniMapHeading(int CameraIndex, bool state)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		camBaseCamera* pCamera = GetCameraFromIndex(CameraIndex, "SET_CAM_CONTROLS_MINI_MAP_HEADING");
		if(pCamera)
		{
			if(SCRIPT_VERIFY(camInterface::GetScriptDirector().IsCameraScriptControllable(CameraIndex),
				"SET_CAM_CONTROLS_MINI_MAP_HEADING - Camera is not script controlled"))
			{
				if(pCamera)
				{
					if(pCamera->GetIsClassId(camScriptedCamera::GetStaticClassId()))
					{
						camScriptedCamera* scriptedCamera = static_cast<camScriptedCamera*>(pCamera);
						scriptedCamera->SetShouldControlMiniMapHeading(state);
					}
					else if(pCamera->GetIsClassId(camAnimatedCamera::GetStaticClassId()))
					{
						camAnimatedCamera* animatedCamera = static_cast<camAnimatedCamera*>(pCamera);
						animatedCamera->SetShouldControlMiniMapHeading(state);
					}
				}
			}
		}
	}

	void CommandSetCamIsInsideVehicle(int CameraIndex, bool state)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		camBaseCamera* pCamera = GetCameraFromIndex(CameraIndex, "SET_CAM_IS_INSIDE_VEHICLE");
		if(pCamera)
		{
			if(SCRIPT_VERIFY(camInterface::GetScriptDirector().IsCameraScriptControllable(CameraIndex),
				"SET_CAM_IS_INSIDE_VEHICLE - Camera is not script controlled"))
			{
				if(pCamera)
				{
					if(pCamera->GetIsClassId(camScriptedCamera::GetStaticClassId()))
					{
						camScriptedCamera* scriptedCamera = static_cast<camScriptedCamera*>(pCamera);
						scriptedCamera->SetIsInsideVehicle(state);
					}
				}
			}
		}
	}

	void CommandSetCustomMaxNearClip(int CameraIndex, float customMaxNearClip)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		camBaseCamera* pCamera = GetCameraFromIndex(CameraIndex, "SET_CAM_CUSTOM_MAX_NEAR_CLIP");
		if(pCamera)
		{
			if(SCRIPT_VERIFY(camInterface::GetScriptDirector().IsCameraScriptControllable(CameraIndex),
				"SET_CAM_CUSTOM_MAX_NEAR_CLIP - Camera is not script controlled"))
			{
				if(pCamera)
				{
					if(pCamera->GetIsClassId(camScriptedCamera::GetStaticClassId()))
					{
						camScriptedCamera* scriptedCamera = static_cast<camScriptedCamera*>(pCamera);
						scriptedCamera->SetCustomMaxNearClip(customMaxNearClip);
					}
				}
			}
		}
	}

	void CommandAllowMotionBlurDecay(int CameraIndex, bool enable)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		camBaseCamera* pCamera = GetCameraFromIndex(CameraIndex, "ALLOW_MOTION_BLUR_DECAY");
		if(pCamera)
		{
			if (SCRIPT_VERIFY(camInterface::GetScriptDirector().IsCameraScriptControllable(CameraIndex), "ALLOW_MOTION_BLUR_DECAY - Camera is not script controlled" ))
			{
				if(pCamera && pCamera->GetIsClassId(camScriptedCamera::GetStaticClassId()))
				{
					pCamera->AllowMotionBlurDecay(enable);
				}
			}
		}
	}

	void CommandSetCamDebugName(int BANK_ONLY(CameraIndex), const char* BANK_ONLY(szDebugName))
	{
#if __BANK
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		camBaseCamera* pCamera = GetCameraFromIndex(CameraIndex, "SET_CAM_DEBUG_NAME");
		if(pCamera)
		{
			if(SCRIPT_VERIFY(camInterface::GetScriptDirector().IsCameraScriptControllable(CameraIndex),
				"SET_CAM_DEBUG_NAME - Camera is not script controlled"))
			{
				if(pCamera && pCamera->GetIsClassId(camScriptedCamera::GetStaticClassId()))
				{
					camScriptedCamera* scriptedCamera = static_cast<camScriptedCamera*>(pCamera);
					scriptedCamera->SetDebugName(szDebugName);
				}
			}
		}
#endif // __BANK
	}

	int CommandGetDebugCam()
	{
		camBaseCamera* freeCam = (camBaseCamera*)camInterface::GetDebugDirector().GetFreeCam();
		
		if (SCRIPT_VERIFY(freeCam, "GET_DEBUG_CAM - Failed to get the Debug cam see a coder"))
		{
			return freeCam->GetPoolIndex();
		}
		else
		{
			return 0;
		}
	}

	void CommandSetDebugCamActive(bool state, bool shouldIgnoreDebugPadCameraToggle)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		//NOTE: The debug pad camera toggle can only be ignored when the free camera is being activated by script.
		camInterface::GetDebugDirector().SetFreeCamActiveState(state);
		camInterface::GetDebugDirector().SetShouldIgnoreDebugPadCameraToggle(state ? shouldIgnoreDebugPadCameraToggle : false);
	}

	//new spline
	void CommandAddCamSplineNode(int CameraIndex, const scrVector &scrVecPos, const scrVector &scrVecRot, int iDuration, int Flags, int RotOrder)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		Vector3 vPos (scrVecPos);
		Vector3 vRot (scrVecRot);

		camBaseCamera* pCamera = GetCameraFromIndex(CameraIndex, "ADD_CAM_SPLINE_NODE");
		if(pCamera)
		{
			if (SCRIPT_VERIFY(camInterface::GetScriptDirector().IsCameraScriptControllable(CameraIndex),"ADD_CAM_SPLINE_NODE - Camera is not script controlled" ))
			{	
				if (SCRIPT_VERIFY(pCamera->GetIsClassId(camBaseSplineCamera::GetStaticClassId()),"ADD_CAM_SPLINE_NODE - Cam is not a named spline cam" ))
				{
					camBaseSplineCamera* splineCamera = static_cast<camBaseSplineCamera*>(pCamera);
					splineCamera->AddNodeWithOrientation(vPos, (vRot * DtoR), static_cast<u32>(iDuration), (camSplineNode::eFlags)Flags, RotOrder);
				}
			}
		}
	}

	void CommandAddCamSplineNodeUsingCameraFrame(int CameraIndex, int SourceCameraIndex, int iDuration, int Flags)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		camBaseCamera* pSourceCamera		= GetCameraFromIndex(SourceCameraIndex, "ADD_CAM_SPLINE_NODE_USING_CAMERA_FRAME");
		camBaseCamera* pDestinationCamera	= GetCameraFromIndex(CameraIndex, "ADD_CAM_SPLINE_NODE_USING_CAMERA_FRAME");

		if(pSourceCamera && pDestinationCamera)
		{
			if (SCRIPT_VERIFY(camInterface::GetScriptDirector().IsCameraScriptControllable(CameraIndex),"ADD_CAM_SPLINE_NODE_USING_CAMERA_FRAME - Camera is not script controlled" ))
			{	
				if (SCRIPT_VERIFY(pDestinationCamera->GetIsClassId(camBaseSplineCamera::GetStaticClassId()),"ADD_CAM_SPLINE_NODE_USING_CAMERA_FRAME - Cam is not a named spline cam" ))
				{
					camBaseSplineCamera* splineCamera		= static_cast<camBaseSplineCamera*>(pDestinationCamera);
					const camFrame& sourceFrame	= pSourceCamera->GetFrame();
					splineCamera->AddNode(sourceFrame, static_cast<u32>(iDuration), (camSplineNode::eFlags)Flags);
				}
			}
		}
	}

	void CommandAddCamSplineNodeUsingCamera(int CameraIndex, int SourceCameraIndex, int iDuration, int Flags)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		camBaseCamera* pSourceCamera		= GetCameraFromIndex(SourceCameraIndex, "ADD_CAM_SPLINE_NODE_USING_CAMERA");
		camBaseCamera* pDestinationCamera	= GetCameraFromIndex(CameraIndex, "ADD_CAM_SPLINE_NODE_USING_CAMERA");

		if(pSourceCamera && pDestinationCamera)
		{
			if (SCRIPT_VERIFY(camInterface::GetScriptDirector().IsCameraScriptControllable(CameraIndex),"ADD_CAM_SPLINE_NODE_USING_CAMERA - Camera is not script controlled" ))
			{	
				if (SCRIPT_VERIFY(pDestinationCamera->GetIsClassId(camBaseSplineCamera::GetStaticClassId()),"ADD_CAM_SPLINE_NODE_USING_CAMERA - Cam is not a named spline cam" ))
				{
					camBaseSplineCamera* splineCamera		= static_cast<camBaseSplineCamera*>(pDestinationCamera);
					splineCamera->AddNode(pSourceCamera, static_cast<u32>(iDuration), (camSplineNode::eFlags)Flags);
				}
			}
		}
	}

	void CommandAddCamSplineNodeUsingGameplayFrame(int CameraIndex, int iDuration, int Flags)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		camBaseCamera* pCamera	= GetCameraFromIndex(CameraIndex, "ADD_CAM_SPLINE_NODE_USING_GAMEPLAY_FRAME");
		if(pCamera)
		{
			if (SCRIPT_VERIFY(camInterface::GetScriptDirector().IsCameraScriptControllable(CameraIndex),"ADD_CAM_SPLINE_NODE_USING_GAMEPLAY_FRAME - Camera is not script controlled" ))
			{	
				if (SCRIPT_VERIFY(pCamera->GetIsClassId(camBaseSplineCamera::GetStaticClassId()),"ADD_CAM_SPLINE_NODE_USING_GAMEPLAY_FRAME - Cam is not a named spline cam" ))
				{
					camBaseSplineCamera* splineCamera = static_cast<camBaseSplineCamera*>(pCamera);
					splineCamera->AddNode(camInterface::GetGameplayDirector(), static_cast<u32>(iDuration), (camSplineNode::eFlags)Flags);
				}
			}
		}
	}

	void CommandSetCamSplinePhase(int CameraIndex, float phase)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		camBaseCamera* pCamera = GetCameraFromIndex(CameraIndex, "SET_CAM_SPLINE_PHASE");
		if(pCamera)
		{
			if (SCRIPT_VERIFY(camInterface::GetScriptDirector().IsCameraScriptControllable(CameraIndex),"SET_CAM_SPLINE_PHASE - Camera is not script controlled" ))
			{
				if(SCRIPT_VERIFY(pCamera->GetIsClassId(camBaseSplineCamera::GetStaticClassId()),"SET_CAM_SPLINE_PHASE - Cam is not a named spline cam" ))
				{
					camBaseSplineCamera* splineCamera	= static_cast<camBaseSplineCamera*>(pCamera);
					splineCamera->SetPhase(phase);
				}
			}
		}
	}
	
	float CommandGetCamSplinePhase(int CameraIndex)
	{
		camBaseCamera* pCamera = GetCameraFromIndex(CameraIndex, "GET_CAM_SPLINE_PHASE");
		if(pCamera)
		{
			if (SCRIPT_VERIFY(camInterface::GetScriptDirector().IsCameraScriptControllable(CameraIndex),"GET_CAM_SPLINE_PHASE - Camera is not script controlled" ))
			{
				if(SCRIPT_VERIFY(pCamera->GetIsClassId(camBaseSplineCamera::GetStaticClassId()),"GET_CAM_SPLINE_PHASE - Cam is not a named spline cam" ))
				{
					camBaseSplineCamera* splineCamera	= static_cast<camBaseSplineCamera*>(pCamera);
					return splineCamera->GetPhase(NULL); 
				}
			}
		}
		return 0.0f; 
	}

	float CommandGetCamSplineNodePhase(int CameraIndex)
	{
		camBaseCamera* pCamera = GetCameraFromIndex(CameraIndex, "GET_CAM_SPLINE_NODE_PHASE");
		if(pCamera)
		{
			if (SCRIPT_VERIFY(camInterface::GetScriptDirector().IsCameraScriptControllable(CameraIndex),"GET_CAM_SPLINE_NODE_PHASE - Camera is not script controlled" ))
			{
				if(SCRIPT_VERIFY(pCamera->GetIsClassId(camBaseSplineCamera::GetStaticClassId()),"GET_CAM_SPLINE_NODE_PHASE - Cam is not a named spline cam" ))
				{
					camBaseSplineCamera* splineCamera	= static_cast<camBaseSplineCamera*>(pCamera);
					return splineCamera->GetNodePhase();
				}
			}
		}
		return 0.0f; 
	}

	int CommandGetCamSplineNodeIndex(int CameraIndex)
	{
		camBaseCamera* pCamera = GetCameraFromIndex(CameraIndex, "GET_CAM_SPLINE_NODE_INDEX");
		if(pCamera)
		{
			if (SCRIPT_VERIFY(camInterface::GetScriptDirector().IsCameraScriptControllable(CameraIndex),"GET_CAM_SPLINE_NODE_INDEX - Camera is not script controlled" ))
			{
				if(SCRIPT_VERIFY(pCamera->GetIsClassId(camBaseSplineCamera::GetStaticClassId()),"GET_CAM_SPLINE_NODE_INDEX - Cam is not a named spline cam" ))
				{
					camBaseSplineCamera* splineCamera	= static_cast<camBaseSplineCamera*>(pCamera);
					return (int)splineCamera->GetCurrentNode();
				}
			}
		}
		return (int)0;
	}

	void CommandSetCamSplineDuration(int CameraIndex, int Duration)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		camBaseCamera* pCamera = GetCameraFromIndex(CameraIndex, "SET_SPLINE_CAM_DURATION");
		if(pCamera)
		{
			if (SCRIPT_VERIFY(camInterface::GetScriptDirector().IsCameraScriptControllable(CameraIndex),"SET_SPLINE_CAM_DURATION - Camera is not script controlled" ))
			{
				if(SCRIPT_VERIFY(pCamera->GetIsClassId(camBaseSplineCamera::GetStaticClassId()) ,"SET_SPLINE_CAM_DURATION - Cam is not a spline camera" ))
				{
					camBaseSplineCamera* splineCamera	= static_cast<camBaseSplineCamera*>(pCamera);
					return splineCamera->SetDuration(Duration); 
				}
			}
		}
	}

	void CommandSetCamSplineSmoothingStyle(int CameraIndex, int Smoothing)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		camBaseCamera* pCamera = GetCameraFromIndex(CameraIndex, "SET_CAM_SPLINE_SMOOTHING_STYLE");
		if(pCamera)
		{
			if (SCRIPT_VERIFY(camInterface::GetScriptDirector().IsCameraScriptControllable(CameraIndex),"SET_CAM_SPLINE_SMOOTHING_STYLE - Camera is not script controlled" ))
			{
				if(SCRIPT_VERIFY(pCamera->GetIsClassId(camBaseSplineCamera::GetStaticClassId()),"SET_CAM_SPLINE_SMOOTHING_STYLE - Cam is not a named spline cam" ))
				{
					camBaseSplineCamera* splineCamera	= static_cast<camBaseSplineCamera*>(pCamera);
					splineCamera->SetSmoothingStyle((camBaseSplineCamera::eSmoothingStyle) Smoothing); 
				}
			}
		}
	}

	void CommandSetCamSplineNodeEase(int CameraIndex, int NodeIndex, int Flags, float Scale)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		if (SCRIPT_VERIFY(NodeIndex >= 0, "SET_CAM_SPLINE_NODE_EASE - NodeIndex must be >= 0") &&
			SCRIPT_VERIFY(Flags >= 0,     "SET_CAM_SPLINE_NODE_EASE - Flags must be >= 0"))
		{
			camBaseCamera* pCamera = GetCameraFromIndex(CameraIndex, "SET_CAM_SPLINE_NODE_EASE");
			if(pCamera)
			{
				if (SCRIPT_VERIFY(camInterface::GetScriptDirector().IsCameraScriptControllable(CameraIndex), "SET_CAM_SPLINE_NODE_EASE - Camera is not script controlled" ))
				{
					if(SCRIPT_VERIFY(pCamera->GetIsClassId(camBaseSplineCamera::GetStaticClassId()), "SET_CAM_SPLINE_NODE_EASE - Cam is not a named spline cam" ))
					{
						camBaseSplineCamera* splineCamera	= static_cast<camBaseSplineCamera*>(pCamera);
						if (SCRIPT_VERIFY(NodeIndex < splineCamera->GetNumNodes(), "SET_CAM_SPLINE_NODE_EASE - NodeIndex must be < number of nodes"))
						{
							splineCamera->SetEase((u32)NodeIndex, (u32)Flags, Scale);
						}
					}
				}
			}
		}
	}

	void CommandSetCamSplineNodeVelocityScale(int CameraIndex, int NodeIndex, float Scale)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		if (SCRIPT_VERIFY(NodeIndex >= 0, "SET_CAM_SPLINE_NODE_EASE - NodeIndex must be >= 0"))
		{
			camBaseCamera* pCamera = GetCameraFromIndex(CameraIndex, "SET_CAM_SPLINE_NODE_VELOCITY_SCALE");
			if(pCamera)
			{
				if (SCRIPT_VERIFY(camInterface::GetScriptDirector().IsCameraScriptControllable(CameraIndex), "SET_CAM_SPLINE_NODE_VELOCITY_SCALE - Camera is not script controlled" ))
				{
					if(SCRIPT_VERIFY(pCamera->GetIsClassId(camBaseSplineCamera::GetStaticClassId()), "SET_CAM_SPLINE_NODE_VELOCITY_SCALE - Cam is not a named spline cam" ))
					{
						camBaseSplineCamera* splineCamera	= static_cast<camBaseSplineCamera*>(pCamera);
						if (SCRIPT_VERIFY(NodeIndex < splineCamera->GetNumNodes(), "SET_CAM_SPLINE_NODE_VELOCITY_SCALE - NodeIndex must be < number of nodes"))
						{
							if (SCRIPT_VERIFY(Scale >= -1.0f && Scale <= 1.0f, "SET_CAM_SPLINE_NODE_VELOCITY_SCALE - Scale must be between -1 and 1 (inclusive)"))
							{
								splineCamera->SetVelocityScale((u32)NodeIndex, Scale);
							}
						}
					}
				}
			}
		}
	}

	void CommandOverrideCamSplineVelocity(int CameraIndex, int Entry, float Start, float Speed)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		if (SCRIPT_VERIFY(Entry >= 0, "OVERRIDE_CAM_SPLINE_VELOCITY - Entry must be >= 0") &&
			SCRIPT_VERIFY(Entry < MAX_VEL_CHANGES, "OVERRIDE_CAM_SPLINE_VELOCITY - Entry must be >= 0"))
		{
			camBaseCamera* pCamera = GetCameraFromIndex(CameraIndex, "OVERRIDE_CAM_SPLINE_VELOCITY");
			if(pCamera)
			{
				if (SCRIPT_VERIFY(camInterface::GetScriptDirector().IsCameraScriptControllable(CameraIndex), "OVERRIDE_CAM_SPLINE_VELOCITY - Camera is not script controlled" ))
				{
					if(SCRIPT_VERIFY(pCamera->GetIsClassId(camCustomTimedSplineCamera::GetStaticClassId()), "OVERRIDE_CAM_SPLINE_VELOCITY - Cam is not a timed spline cam" ))
					{
						camCustomTimedSplineCamera* timedSplineCamera	= static_cast<camCustomTimedSplineCamera*>(pCamera);
						if (SCRIPT_VERIFY(Start < (float)timedSplineCamera->GetNumNodes(), "OVERRIDE_CAM_SPLINE_VELOCITY - Start must be < number of nodes"))
						{
							timedSplineCamera->OverrideVelocity((u32)Entry, Start, Speed);
						}
					}
				}
			}
		}
	}

	void CommandOverrideCamSplineMotionBlur(int CameraIndex, int Entry, float BlurStart, float MotionBlur)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		if (SCRIPT_VERIFY(Entry >= 0, "OVERRIDE_CAM_SPLINE_MOTION_BLUR - Entry must be >= 0") &&
			SCRIPT_VERIFY(Entry < MAX_BLUR_CHANGES, "OVERRIDE_CAM_SPLINE_MOTION_BLUR - Entry must be >= 0"))
		{
			camBaseCamera* pCamera = GetCameraFromIndex(CameraIndex, "OVERRIDE_CAM_SPLINE_MOTION_BLUR");
			if(pCamera)
			{
				if (SCRIPT_VERIFY(camInterface::GetScriptDirector().IsCameraScriptControllable(CameraIndex), "OVERRIDE_CAM_SPLINE_MOTION_BLUR - Camera is not script controlled" ))
				{
					if(SCRIPT_VERIFY(pCamera->GetIsClassId(camCustomTimedSplineCamera::GetStaticClassId()), "OVERRIDE_CAM_SPLINE_MOTION_BLUR - Cam is not a timed spline cam" ))
					{
						camCustomTimedSplineCamera* timedSplineCamera	= static_cast<camCustomTimedSplineCamera*>(pCamera);
						if (SCRIPT_VERIFY(BlurStart < (float)timedSplineCamera->GetNumNodes(), "OVERRIDE_CAM_SPLINE_MOTION_BLUR - Start must be < number of nodes"))
						{
							timedSplineCamera->OverrideMotionBlur((u32)Entry, BlurStart, MotionBlur);
						}
					}
				}
			}
		}
	}

	void CommandSetCamSplineNodeExtraFlags(int CameraIndex, int NodeIndex, int Flags)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		if (SCRIPT_VERIFY(NodeIndex >= 0, "SET_CAM_SPLINE_NODE_FORCE_LINEAR - NodeIndex must be >= 0"))
		{
			camBaseCamera* pCamera = GetCameraFromIndex(CameraIndex, "SET_CAM_SPLINE_NODE_FORCE_LINEAR");
			if(pCamera)
			{
				if (SCRIPT_VERIFY(camInterface::GetScriptDirector().IsCameraScriptControllable(CameraIndex), "SET_CAM_SPLINE_NODE_FORCE_LINEAR - Camera is not script controlled" ))
				{
					if(SCRIPT_VERIFY(pCamera->GetIsClassId(camBaseSplineCamera::GetStaticClassId()), "SET_CAM_SPLINE_NODE_FORCE_LINEAR - Cam is not a named spline cam" ))
					{
						camBaseSplineCamera* splineCamera	= static_cast<camBaseSplineCamera*>(pCamera);
						if (SCRIPT_VERIFY(NodeIndex < splineCamera->GetNumNodes(), "SET_CAM_SPLINE_NODE_FORCE_LINEAR - NodeIndex must be < number of nodes"))
						{
							splineCamera->ForceLinearBlend(	(u32)NodeIndex, (Flags & camSplineNode::FORCE_LINEAR)	!= 0);

							splineCamera->ForceCut(			(u32)NodeIndex, (Flags & camSplineNode::FORCE_CUT)		!= 0);

							splineCamera->ForcePause(		(u32)NodeIndex, (Flags & camSplineNode::FORCE_PAUSE)	!= 0);

							splineCamera->ForceLevel(		(u32)NodeIndex, (Flags & camSplineNode::FORCE_LEVEL)	!= 0);
						}
					}
				}
			}
		}
	}

	bool CommandIsCamSplinePaused(int CameraIndex)
	{
		camBaseCamera* pCamera = GetCameraFromIndex(CameraIndex, "IS_CAM_SPLINE_PAUSED");
		if(pCamera)
		{
			if (SCRIPT_VERIFY(camInterface::GetScriptDirector().IsCameraScriptControllable(CameraIndex), "IS_CAM_SPLINE_PAUSED - Camera is not script controlled" ))
			{
				if(SCRIPT_VERIFY(pCamera->GetIsClassId(camBaseSplineCamera::GetStaticClassId()), "IS_CAM_SPLINE_PAUSED - Cam is not a named spline cam" ))
				{
					camBaseSplineCamera* splineCamera	= static_cast<camBaseSplineCamera*>(pCamera);
					return splineCamera->IsPaused();
				}
			}
		}
		return false;
	}

	// interp
	void CommandSetCamActiveWithInterp (int CameraIndexDest, int CameraIndexSource, int iDuration, int graphTypePos, int graphTypeRot  )
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		CommandSetCamActive(CameraIndexDest, true);

		camBaseCamera* sourceCamera			= camBaseCamera::GetCameraAtPoolIndex(CameraIndexSource);
		camBaseCamera* destinationCamera	= camBaseCamera::GetCameraAtPoolIndex(CameraIndexDest);

		if (SCRIPT_VERIFY (sourceCamera, "SET_CAM_ACTIVE_WITH_INTERP - Source camera does not exist") && 
			SCRIPT_VERIFY (destinationCamera, "SET_CAM_ACTIVE_WITH_INTERP - Destination camera does not exist"))
		{	
			if (SCRIPT_VERIFY(camInterface::GetScriptDirector().IsCameraScriptControllable(CameraIndexDest),"SET_CAM_ACTIVE_WITH_INTERP - Destination camera is not script controlled" ) && 
					SCRIPT_VERIFY(camInterface::GetScriptDirector().IsCameraScriptControllable(CameraIndexSource),"SET_CAM_ACTIVE_WITH_INTERP - Source Camera is not script controlled" )	)
			{
				camFrameInterpolator* sourceCameraFrameInterpolator = sourceCamera->GetFrameInterpolator();
				if(sourceCameraFrameInterpolator && (sourceCameraFrameInterpolator->GetSourceCamera() == destinationCamera))
				{
					//The interpolation source camera is currently interpolating from our new destination camera, so we must lock the existing
					//interpolation to a static (destination-relative) frame. Otherwise we would get a circular reference that can result in
					//camera glitches.
					sourceCameraFrameInterpolator->SetSourceCamera(NULL);
				}

				destinationCamera->InterpolateFromCamera(*sourceCamera, iDuration);

				camFrameInterpolator* interpolator = destinationCamera->GetFrameInterpolator();
				if(interpolator)
				{
					interpolator->SetCurveTypeForFrameComponent(camFrame::CAM_FRAME_COMPONENT_POS, (camFrameInterpolator::eCurveTypes)graphTypePos);
					interpolator->SetCurveTypeForFrameComponent(camFrame::CAM_FRAME_COMPONENT_MATRIX, (camFrameInterpolator::eCurveTypes)graphTypeRot);
				}
			}
		}
	}
	
	bool CommandIsCamInterpolating(int CameraIndex)
	{
		bool isInterpolating = false;

		camBaseCamera* pCamera = GetCameraFromIndex(CameraIndex, "IS_CAM_INTERPOLATING");
		if(pCamera)
		{
			if (SCRIPT_VERIFY(camInterface::GetScriptDirector().IsCameraScriptControllable(CameraIndex),"IS_CAM_INTERPOLATING - Camera is not script controlled" ))
			{	
				if(pCamera->GetIsClassId(camBaseSplineCamera::GetStaticClassId()))
				{
					camBaseSplineCamera* splineCamera = static_cast<camBaseSplineCamera*>(pCamera);
					isInterpolating = !splineCamera->HasReachedEnd();
				}
				else
				{
					isInterpolating = pCamera->IsInterpolating();
				}
			}
		}
		
		return isInterpolating;
	}

	bool CommandIsInterpolatingFromScriptCams()
	{
		const bool isInterpolatingOut = (camInterface::GetScriptDirector().GetRenderState() == camBaseDirector::RS_INTERPOLATING_OUT);

		return isInterpolatingOut;
	}

	bool CommandIsInterpolatingToScriptCams()
	{
		const bool isInterpolatingIn = (camInterface::GetScriptDirector().GetRenderState() == camBaseDirector::RS_INTERPOLATING_IN);

		return isInterpolatingIn;
	}

	void CommandSetGameplayCamAltitudeFovScalingState(bool state)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		camInterface::GetGameplayDirector().SetPersistentHighAltitudeFovScalingState(state);
	}

	void CommandDisableGameplayCamAltitudeFovScalingThisUpdate()
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		camInterface::GetGameplayDirector().DisableHighAltitudeFovScalingThisUpdate();
	}

	bool CommandIsGameplayCamLookingBehind()
	{
		const bool isLookingBehind = camInterface::GetGameplayDirector().IsLookingBehind();

		return isLookingBehind;
	}

	void CommandSetIgnoreCollisionForEntityThisUpdate(int index)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		const CEntity* pEntity	= CTheScripts::GetEntityToQueryFromGUID<CEntity>(index, CTheScripts::GUID_ASSERT_FLAG_ENTITY_EXISTS);
		
		if(pEntity)
		{
			camInterface::GetGameplayDirector().SetScriptEntityToIgnoreCollisionThisUpdate(pEntity); 
		}
	}
	
	void CommandDisableCamCollisionForObject(int index)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		 CObject* pObject = CTheScripts::GetEntityToModifyFromGUID<CObject>(index, CTheScripts::GUID_ASSERT_FLAG_ENTITY_EXISTS);
		
		 if(pObject)
		 {
			phInst* pObjectPhysicsInst = pObject->GetCurrentPhysicsInst();
			if(pObjectPhysicsInst)
			{
				phArchetype* pObjectArchetype = pObjectPhysicsInst->GetArchetype();
				if(pObjectArchetype)
				{
					pObjectArchetype->RemoveIncludeFlags(ArchetypeFlags::GTA_CAMERA_TEST);
				}
			}
		}
	}

	void CommandBypassCameraCollisionBuoyancyTestThisUpdate()
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		camThirdPersonCamera* pThirdPersonCamera = camInterface::GetGameplayDirector().GetThirdPersonCamera();
		if(pThirdPersonCamera)
		{
			pThirdPersonCamera->BypassBuoyancyThisUpdate();
		}
	}

	void CommandSetGameplayCamEntityToLimitFocusOverBoundingSphereThisUpdate(int index)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		const CEntity* entity = CTheScripts::GetEntityToQueryFromGUID<CEntity>(index, CTheScripts::GUID_ASSERT_FLAG_ENTITY_EXISTS);
		if(entity)
		{
			camInterface::GetGameplayDirector().SetScriptEntityToLimitFocusOverBoundingSphereThisUpdate(entity);
		}
	}

	void CommandDisableFirstPersonCameraWaterClippingTestThisUpdate()
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		camInterface::GetCinematicDirector().ScriptDisableWaterClippingTestThisUpdate();
	}

	//shake

	void CommandShakeCam(int CameraIndex, const char* ShakeName, float fAmplitudeScalar)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		camBaseCamera* pCamera = GetCameraFromIndex(CameraIndex, "SHAKE_CAM");
		if(pCamera)
		{
			if(SCRIPT_VERIFY(camInterface::GetScriptDirector().IsCameraScriptControllable(CameraIndex), "SHAKE_CAM - Camera is not script controlled"))
			{
				pCamera->StopShaking();
#if __BANK
				camInterface::GetScriptDirector().DebugSetScriptDisabledShake();
#endif // __BANK

				SCRIPT_VERIFY_TWO_STRINGS(pCamera->Shake(ShakeName, fAmplitudeScalar),
					"SHAKE_CAM - Failed to create the requested shake: ", ShakeName);

#if __BANK
				camInterface::GetScriptDirector().DebugSetScriptEnabledShakeName(ShakeName);
				camInterface::GetScriptDirector().DebugSetScriptEnabledShakeAmplitude(fAmplitudeScalar);
#endif // __BANK
			}
		}
	}

	void CommandAnimatedShakeCam(int CameraIndex, const char* AnimDictName, const char* AnimClipName, const char* ShakeName, float fAmplitudeScalar)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		camBaseCamera* pCamera = GetCameraFromIndex(CameraIndex, "ANIMATED_SHAKE_CAM");
		if(pCamera)
		{
			if(SCRIPT_VERIFY(camInterface::GetScriptDirector().IsCameraScriptControllable(CameraIndex), "ANIMATED_SHAKE_CAM - Camera is not script controlled"))
			{
				pCamera->StopShaking();
#if __BANK
				camInterface::GetScriptDirector().DebugSetScriptDisabledShake();
#endif // __BANK

				SCRIPT_VERIFY_TWO_STRINGS(pCamera->Shake(AnimDictName, AnimClipName, ShakeName, fAmplitudeScalar),
					"ANIMATED_SHAKE_CAM - Failed to create the requested shake using anim: ", AnimClipName);
#if __BANK
				camInterface::GetScriptDirector().DebugSetScriptEnabledShakeName(ShakeName);
				camInterface::GetScriptDirector().DebugSetScriptEnabledShakeAmplitude(fAmplitudeScalar);
#endif // __BANK
			}
		}
	}

	bool CommandIsCamShaking(int CameraIndex)
	{
		bool isShaking = false;

		camBaseCamera* pCamera = GetCameraFromIndex(CameraIndex, "IS_CAM_SHAKING");
		if(pCamera)
		{
			if(SCRIPT_VERIFY(camInterface::GetScriptDirector().IsCameraScriptControllable(CameraIndex), "IS_CAM_SHAKING - Camera is not script controlled"))
			{
				isShaking = pCamera->IsShaking();
			}
		}

		return isShaking;
	}

	void CommandSetCamShakeAmplitude(int CameraIndex, float fAmplitudeScalar)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		camBaseCamera* pCamera = GetCameraFromIndex(CameraIndex, "SET_CAM_SHAKE_AMPLITUDE");
		if(pCamera)
		{
			if(SCRIPT_VERIFY(camInterface::GetScriptDirector().IsCameraScriptControllable(CameraIndex), "SET_CAM_SHAKE_AMPLITUDE - Camera is not script controlled"))
			{
				SCRIPT_VERIFY(pCamera->SetShakeAmplitude(fAmplitudeScalar), "SET_CAM_SHAKE_AMPLITUDE - Camera is not shaking");
#if __BANK
				camInterface::GetScriptDirector().DebugSetScriptEnabledShakeAmplitude(fAmplitudeScalar);
#endif // __BANK
			}
		}
	}

	void CommandStopCamShaking(int CameraIndex, bool shouldStopImmediately)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		camBaseCamera* pCamera = GetCameraFromIndex(CameraIndex, "STOP_CAM_SHAKING");
		if(pCamera)
		{
			if(SCRIPT_VERIFY(camInterface::GetScriptDirector().IsCameraScriptControllable(CameraIndex), "STOP_CAM_SHAKING - Camera is not script controlled"))
			{
				pCamera->StopShaking(shouldStopImmediately);
#if __BANK
				camInterface::GetScriptDirector().DebugSetScriptDisabledShake();
#endif // __BANK
			}
		}
	}

	void CommandShakeScriptGlobal(const char* ShakeName, float fAmplitudeScalar)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		SCRIPT_VERIFY_TWO_STRINGS(camInterface::GetScriptDirector().Shake(ShakeName, fAmplitudeScalar),
			"SHAKE_SCRIPT_GLOBAL - Failed to create the requested shake: ", ShakeName);
#if __BANK
		camInterface::GetScriptDirector().DebugSetScriptEnabledShakeName(ShakeName);
		camInterface::GetScriptDirector().DebugSetScriptEnabledShakeAmplitude(fAmplitudeScalar);
#endif // __BANK
	}

	void CommandAnimatedShakeScriptGlobal(const char* AnimDictName, const char* AnimClipName, const char* ShakeName, float fAmplitudeScalar)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		SCRIPT_VERIFY_TWO_STRINGS(camInterface::GetScriptDirector().Shake(AnimDictName, AnimClipName, ShakeName, fAmplitudeScalar),
			"ANIMATED_SHAKE_SCRIPT_GLOBAL - Failed to create the requested shake: ", AnimClipName);
#if __BANK
		camInterface::GetScriptDirector().DebugSetScriptEnabledShakeName(ShakeName);
		camInterface::GetScriptDirector().DebugSetScriptEnabledShakeAmplitude(fAmplitudeScalar);
#endif // __BANK
	}

	bool CommandIsScriptGlobalShaking()
	{
		const bool isShaking = camInterface::GetScriptDirector().IsShaking();

		return isShaking;
	}

	void CommandSetScriptGlobalShakeAmplitude(float fAmplitudeScalar)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		SCRIPT_VERIFY(camInterface::GetScriptDirector().SetShakeAmplitude(fAmplitudeScalar),
			"SET_SCRIPT_GLOBAL_SHAKE_AMPLITUDE - The script director is not shaking");
#if __BANK
		camInterface::GetScriptDirector().DebugSetScriptEnabledShakeAmplitude(fAmplitudeScalar);
#endif // __BANK
	}

	void CommandStopScriptGlobalShaking(bool shouldStopImmediately)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		camInterface::GetScriptDirector().StopShaking(shouldStopImmediately);
#if __BANK
		camInterface::GetScriptDirector().DebugSetScriptDisabledShake();
#endif // __BANK
	}

	void CommandTriggerVehiclePartBrokenCameraShake(int vehicleIndex, int vehiclePart, float amplitude)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND("vehicleIndex(%d) vehiclePart(%d) amplitude(%.2f)", vehicleIndex, vehiclePart, amplitude);

		const CVehicle* vehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(vehicleIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);

		if(SCRIPT_VERIFY(vehicle, "Invalid vehicle index passed to the TRIGGER_VEHICLE_PART_BROKEN_CAMERA_SHAKE native"))
		{
			vehiclePart += VEH_EXTRA_1 - 1;
			if (SCRIPT_VERIFY(vehiclePart >= VEH_EXTRA_1 && vehiclePart <= VEH_LAST_EXTRA, "TRIGGER_VEHICLE_PART_BROKEN_CAMERA_SHAKE - Extra is outside range"))
			{
				camInterface::GetGameplayDirector().RegisterVehiclePartBroken(*vehicle, static_cast<eHierarchyId>(vehiclePart), Max(0.0f, amplitude));
			}
		}
	}

	//camera animation

	void GetAnimInfoFromControlFlags(int nControlFlags, u32& iFlags)
	{
		bool bLooped				= (nControlFlags & BIT(AF_LOOPING)) ? true : false;

		//	Animation is looped if it has to play for a certain length of time
		if ( (bLooped) /*&&(bHoldLastFrame == false)*/ ) 
		{
			iFlags |= APF_ISLOOPED;
		}
	}

	bool CommandPlayCamAnim(int CameraIndex, const char* animName, const char* animDictName, const scrVector &scrVecOriginPosition, const scrVector &scrVecOriginRotation, int iflags, int RotOrder)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		bool hasSucceeded = false;
		if (SCRIPT_VERIFY(camInterface::GetScriptDirector().IsCameraScriptControllable(CameraIndex), "PLAY_CAM_ANIM - Camera is not script controlled"))
		{
			const crClip* clip = fwAnimManager::GetClipIfExistsByName(animDictName, animName);
			if (SCRIPT_VERIFYF(clip, "PLAY_CAM_ANIM - Couldn't find animation \"%s\\%s\". Has it been loaded?", animDictName, animName))
			{
				u32 iAnimFlags = 0; 
				GetAnimInfoFromControlFlags(iflags, iAnimFlags);

				Matrix34 sceneOrigin;
				CScriptEulers::MatrixFromEulers(sceneOrigin, Vector3(scrVecOriginRotation) * DtoR, static_cast<EulerAngleOrder>(RotOrder));

				sceneOrigin.d = Vector3(scrVecOriginPosition);
				hasSucceeded = camInterface::GetScriptDirector().AnimateCamera(CameraIndex, animDictName, *clip, sceneOrigin, -1, iAnimFlags);
			}
		}

		return hasSucceeded;
	}

	bool CommandPlaySynchronizedCamAnim(int CameraIndex, int sceneId, const char* animName, const char* animDictName)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		bool hasSucceeded = false;
		if (SCRIPT_VERIFY(camInterface::GetScriptDirector().IsCameraScriptControllable(CameraIndex), "PLAY_SYNCHRONIZED_CAM_ANIM - Camera is not script controlled"))
		{
			if (SCRIPT_VERIFY(fwAnimDirectorComponentSyncedScene::IsValidSceneId(static_cast<fwSyncedSceneId>(sceneId)), "PLAY_SYNCHRONIZED_CAM_ANIM - Invalid scene ID!"))
			{
				const crClip* clip = fwAnimManager::GetClipIfExistsByName(animDictName, animName);
				if (SCRIPT_VERIFYF(clip, "PLAY_SYNCHRONIZED_CAM_ANIM - Couldn't find animation \"%s\\%s\". Has it been loaded?", animDictName, animName))
				{
					Matrix34 sceneOrigin;
					fwAnimDirectorComponentSyncedScene::GetSyncedSceneOrigin(static_cast<fwSyncedSceneId>(sceneId), sceneOrigin);
					hasSucceeded = camInterface::GetScriptDirector().AnimateCamera(CameraIndex, animDictName, *clip, sceneOrigin, sceneId);

				}
			}
		}

		return hasSucceeded;
	}

	bool CommandIsCamPlayingAnim(int CameraIndex, const char* animName, const char* animDictName)
	{
		bool isPlaying = false;
		if (SCRIPT_VERIFY(camInterface::GetScriptDirector().IsCameraScriptControllable(CameraIndex), "IS_CAM_PLAYING_ANIM - Camera is not script controlled"))
		{
			const crClip* clip = fwAnimManager::GetClipIfExistsByName(animDictName, animName);
			if (SCRIPT_VERIFYF(clip, "IS_CAM_PLAYING_ANIM - Couldn't find animation \"%s\\%s\". Has it been loaded?", animDictName, animName))
			{
				isPlaying = camInterface::GetScriptDirector().IsCameraPlayingAnimation(CameraIndex, animDictName, *clip);
			}
		}

		return isPlaying;
	}

	void CommandSetCamAnimCurrentPhase(int CameraIndex, float phase)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		if (SCRIPT_VERIFY(camInterface::GetScriptDirector().IsCameraScriptControllable(CameraIndex), "SET_CAM_ANIM_CURRENT_PHASE - Camera is not script controlled"))
		{
			if (SCRIPT_VERIFY((phase >= 0.0f), "SET_CAM_ANIM_CURRENT_PHASE - phase should be greater than or equal to 0.0") &&
				SCRIPT_VERIFY((phase <= 1.0f), "SET_CAM_ANIM_CURRENT_PHASE - phase should be less than or equal to 1.0"))
			{
				camInterface::GetScriptDirector().SetCameraAnimationPhase(CameraIndex, phase);
			}
		}
	}

	float CommandGetCamAnimCurrentPhase(int CameraIndex)
	{
		float phase = 0.0f;

		if (SCRIPT_VERIFY(camInterface::GetScriptDirector().IsCameraScriptControllable(CameraIndex), "GET_CAM_ANIM_CURRENT_PHASE - Camera is not script controlled"))
		{
			phase = camInterface::GetScriptDirector().GetCameraAnimationPhase(CameraIndex);
		}

		return phase;
	}
	
	//Scene Transition - deprecated!

	void CommandStartSceneTransition(int UNUSED_PARAM(CameraIndex), const scrVector &UNUSED_PARAM(vStartPos), const scrVector &UNUSED_PARAM(vEndPos))
	{
		cameraAssertf(false, "START_CAM_TRANSITION is deprecated!");
	}

	bool CommandIsCameraTransitioning(int UNUSED_PARAM(CameraIndex))
	{
		cameraAssertf(false, "IS_CAMERA_TRANSITIONING is deprecated!");

		return false; 
	}

	float CommandGetCamTransitionPhase(int UNUSED_PARAM(CameraIndex))
	{
		cameraAssertf(false, "GET_CAM_TRANSITION_PHASE is deprecated!");

		return 0.0f; 
	}

	//Scripted fly camera

	void CommandSetFlyCamHorizontalResponse(int CameraIndex, float maxSpeed, float maxAcceleration, float maxDeceleration)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		camBaseCamera* pCamera = GetCameraFromIndex(CameraIndex, "SET_FLY_CAM_HORIZONTAL_RESPONSE");
		if(pCamera)
		{
			if(SCRIPT_VERIFY(camInterface::GetScriptDirector().IsCameraScriptControllable(CameraIndex), "SET_FLY_CAM_HORIZONTAL_RESPONSE - Camera is not script controlled"))
			{
				if(SCRIPT_VERIFY(pCamera->GetIsClassId(camScriptedFlyCamera::GetStaticClassId()), "SET_FLY_CAM_HORIZONTAL_RESPONSE - Camera is not a scripted fly camera"))
				{
					camScriptedFlyCamera* pScriptedFlyCamera	= static_cast<camScriptedFlyCamera*>(pCamera);
					camScriptedFlyCameraMetadata& metadata		= pScriptedFlyCamera->GetMetadata();

					metadata.m_HorizontalTranslationInputResponse.m_MaxAcceleration	= maxAcceleration;
					metadata.m_HorizontalTranslationInputResponse.m_MaxDeceleration	= maxDeceleration;
					metadata.m_HorizontalTranslationInputResponse.m_MaxSpeed		= maxSpeed;
				}
			}
		}
	}

	void CommandSetFlyCamVerticalResponse(int CameraIndex, float maxSpeed, float maxAcceleration, float maxDeceleration)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		camBaseCamera* pCamera = GetCameraFromIndex(CameraIndex, "SET_FLY_CAM_VERTICAL_RESPONSE");
		if(pCamera)
		{
			if(SCRIPT_VERIFY(camInterface::GetScriptDirector().IsCameraScriptControllable(CameraIndex), "SET_FLY_CAM_VERTICAL_RESPONSE - Camera is not script controlled"))
			{
				if(SCRIPT_VERIFY(pCamera->GetIsClassId(camScriptedFlyCamera::GetStaticClassId()), "SET_FLY_CAM_VERTICAL_RESPONSE - Camera is not a scripted fly camera"))
				{
					camScriptedFlyCamera* pScriptedFlyCamera	= static_cast<camScriptedFlyCamera*>(pCamera);
					camScriptedFlyCameraMetadata& metadata		= pScriptedFlyCamera->GetMetadata();

					metadata.m_VerticalTranslationInputResponse.m_MaxAcceleration	= maxAcceleration;
					metadata.m_VerticalTranslationInputResponse.m_MaxDeceleration	= maxDeceleration;
					metadata.m_VerticalTranslationInputResponse.m_MaxSpeed			= maxSpeed;
				}
			}
		}
	}

	void CommandSetFlyCamMaxHeight(int CameraIndex, float maxHeight)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		camBaseCamera* pCamera = GetCameraFromIndex(CameraIndex, "SET_FLY_CAM_MAX_HEIGHT");
		if(pCamera)
		{
			if(SCRIPT_VERIFY(camInterface::GetScriptDirector().IsCameraScriptControllable(CameraIndex), "SET_FLY_CAM_MAX_HEIGHT - Camera is not script controlled"))
			{
				if(SCRIPT_VERIFY(pCamera->GetIsClassId(camScriptedFlyCamera::GetStaticClassId()), "SET_FLY_CAM_MAX_HEIGHT - Camera is not a scripted fly camera"))
				{
					camScriptedFlyCamera* pScriptedFlyCamera	= static_cast<camScriptedFlyCamera*>(pCamera);
					camScriptedFlyCameraMetadata& metadata		= pScriptedFlyCamera->GetMetadata();
					metadata.m_MaxHeight						= maxHeight;
				}
			}
		}
	}

	void CommandSetFlyCamCoordAndConstrain(int CameraIndex, const scrVector &position)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		camBaseCamera* pCamera = GetCameraFromIndex(CameraIndex, "SET_FLY_CAM_COORD_AND_CONSTRAIN");
		if(pCamera)
		{
			if(SCRIPT_VERIFY(camInterface::GetScriptDirector().IsCameraScriptControllable(CameraIndex), "SET_FLY_CAM_COORD_AND_CONSTRAIN - Camera is not script controlled"))
			{
				if(SCRIPT_VERIFY(pCamera->GetIsClassId(camScriptedFlyCamera::GetStaticClassId()), "SET_FLY_CAM_COORD_AND_CONSTRAIN - Camera is not a scripted fly camera"))
				{
					Vector3 vOverridePos(position);
					camScriptedFlyCamera* pScriptedFlyCamera = static_cast<camScriptedFlyCamera*>(pCamera);
					pScriptedFlyCamera->SetScriptPosition(vOverridePos);
				}
			}
		}
	}

	void CommandSetFlyCamVerticalControlsThisUpdate(int CameraIndex)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		camBaseCamera* pCamera = GetCameraFromIndex(CameraIndex, "SET_FLY_CAM_VERTICAL_CONTROLS_THIS_UPDATE");
		if(pCamera)
		{
			if(SCRIPT_VERIFY(camInterface::GetScriptDirector().IsCameraScriptControllable(CameraIndex), "SET_FLY_CAM_VERTICAL_CONTROLS_THIS_UPDATE - Camera is not script controlled"))
			{
				if(SCRIPT_VERIFY(pCamera->GetIsClassId(camScriptedFlyCamera::GetStaticClassId()), "SET_FLY_CAM_VERTICAL_CONTROLS_THIS_UPDATE - Camera is not a scripted fly camera"))
				{
					camScriptedFlyCamera* pScriptedFlyCamera = static_cast<camScriptedFlyCamera*>(pCamera);
					pScriptedFlyCamera->SetVerticalControlThisUpdate(true);
				}
			}
		}
	}

	bool CommandWasFlyCamConstrainedOnPreviousUpdate(int CameraIndex)
	{
		bool wasConstrained = false;

		camBaseCamera* pCamera = GetCameraFromIndex(CameraIndex, "WAS_FLY_CAM_CONSTRAINED_ON_PREVIOUS_UDPATE");
		if(pCamera)
		{
			if(SCRIPT_VERIFY(camInterface::GetScriptDirector().IsCameraScriptControllable(CameraIndex), "WAS_FLY_CAM_CONSTRAINED_ON_PREVIOUS_UDPATE - Camera is not script controlled"))
			{
				if(SCRIPT_VERIFY(pCamera->GetIsClassId(camScriptedFlyCamera::GetStaticClassId()), "WAS_FLY_CAM_CONSTRAINED_ON_PREVIOUS_UDPATE - Camera is not a scripted fly camera"))
				{
					camScriptedFlyCamera* pScriptedFlyCamera = static_cast<camScriptedFlyCamera*>(pCamera);
					wasConstrained = pScriptedFlyCamera->WasConstrained();
				}
			}
		}

		return wasConstrained;
	}

	//Fade

	bool CommandIsScreenFadingIn()
	{
		return (camInterface::IsFadingIn());
	}

	bool CommandIsScreenFadingOut()
	{
		return (camInterface::IsFadingOut());
	}

	bool CommandIsScreenFadedOut()
	{
		return (camInterface::IsFadedOut());
	}

	bool CommandIsScreenFadedIn()
	{
		return (camInterface::IsFadedIn());
	}

	void CommandDoScreenFadeIn(int fadeTime)			
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
#if __BANK
		char DebugString[64];
		sprintf(DebugString, "DO_SCREEN_FADE_IN %d ms", fadeTime);
		CScriptDebug::SetContentsOfFadeCommandTextWidget(DebugString);
#endif

#if GTA_REPLAY && !__FINAL
		if( CReplayMgr::IsReplayInControlOfWorld())
		{
			scriptAssertf( 0, "CommandDoScreenFadeIn: Called during replay %s", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		}
#endif	//GTA_REPLAY
		// Handle Fade Ins, as an extra precaution, for our position tracking, as we'll need some safety there
		camInterface::FadeIn(fadeTime); 
	}

	void CommandDoScreenFadeOut(int fadeTime)			
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		// stop opening the frontend if the script initiates a fade
		//CFrontEnd::StopOpeningFrontend();
#if __BANK
		char DebugString[64];
		sprintf(DebugString, "DO_SCREEN_FADE_OUT %d ms", fadeTime);
		CScriptDebug::SetContentsOfFadeCommandTextWidget(DebugString);
#endif

#if GTA_REPLAY && !__FINAL
		if( CReplayMgr::IsReplayInControlOfWorld())
		{
			scriptAssertf( 0, "CommandDoScreenFadeOut: Called during replay %s", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		}
#endif	//GTA_REPLAY
		// Handle fade outs, because these are always going to cause awkwardness
		camInterface::FadeOut(fadeTime);
	}

//wide screen
	void CommandSetWidescreenBorders(bool bSet, int duration)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		gVpMan.SetWidescreenBorders(bSet, duration);
	}

	bool CommandAreWidescreenBordersActive()
	{
		return gVpMan.AreWidescreenBordersActive();
	}

//Game Cam
	scrVector CommandGetGameplayCamCoord ()
	{
		return camInterface::GetGameplayDirector().GetFrame().GetPosition();
	}
	
	scrVector CommandGetGameplayCamRot (int RotOrder)
	{
		const Matrix34& worldMatrix = camInterface::GetGameplayDirector().GetFrame().GetWorldMatrix();
		Vector3 vRot = CScriptEulers::MatrixToEulers(worldMatrix, static_cast<EulerAngleOrder>(RotOrder));
		vRot *= RtoD;

		return vRot;
	}

	float CommandGetGameplayCamFov ()
	{
		return camInterface::GetGameplayDirector().GetFrame().GetFov();
	}

	void CommandSetGameplayCamMotionBlurScalingThisUpdate(float scaling)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		scaling = Max(scaling, 0.0f); //Do not allow negative scaling to be specified by script.

		camInterface::GetGameplayDirector().SetScriptControlledMotionBlurScalingThisUpdate(scaling);
	}

	void CommandSetGameplayCamMaxMotionBlurStrengthThisUpdate(float maxStrength)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		maxStrength = Clamp(maxStrength, 0.0f, 1.0f); //Clamp the script input to the valid range for motion blur strength.

		camInterface::GetGameplayDirector().SetScriptControlledMaxMotionBlurStrengthThisUpdate(maxStrength);
	}

	float CommandGetGameplayCamRelativeHeading()
	{
		float heading = camInterface::GetGameplayDirector().GetRelativeCameraHeading();
		heading *= RtoD;

		return heading;
	}

	void CommandSetGameplayCamRelativeHeading(float fCamHeading)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		scriptcameraDebugf3("FC: %d - CommandSetGameplayCamRelativeHeading( fCamHeading(%f) )", fwTimer::GetFrameCount(), fCamHeading);

		const float relativeHeadingToApply = fwAngle::LimitRadianAngleSafe(fCamHeading * DtoR);
		camInterface::GetGameplayDirector().SetUseScriptHeading(relativeHeadingToApply); 
		
		// If we're rendering a cinematic mounted camera (e.g. in vehicle POV) we want to force the relative heading to what script are setting (instead of cloning the gameplay camera heading)
		const camBaseDirector* renderedDirector = camInterface::GetDominantRenderedDirector();
		const bool isCinematicDirector = renderedDirector && renderedDirector->GetIsClassId(camCinematicDirector::GetStaticClassId());
		if(isCinematicDirector)
		{
            camCinematicMountedCamera* firstPersonVehicleCamera = camInterface::GetCinematicDirector().GetFirstPersonVehicleCamera();
            if(firstPersonVehicleCamera)
			{				
				firstPersonVehicleCamera->SetUseScriptHeading(relativeHeadingToApply);
			}
		}		
	}

	float CommandGetGameplayCamRelativePitch()
	{
		float pitch = camInterface::GetGameplayDirector().GetRelativeCameraPitch();
		pitch *= RtoD;

		return pitch;
	}

	void CommandSetGameplayCamRelativePitch(float fCamPitch, float smoothRate)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		scriptcameraDebugf3("FC: %d - CommandSetGameplayCamRelativePitch( fCamPitch(%f), smoothRate(%f) )", fwTimer::GetFrameCount(), fCamPitch, smoothRate);

		const float relativePitchToApply = Clamp(fCamPitch * DtoR, -HALF_PI, HALF_PI);
		camInterface::GetGameplayDirector().SetUseScriptPitch(relativePitchToApply, smoothRate );
		
		//we intentionally don't set the pitch for the camCinematicMountedCamera because it might affect existing script code that
		//rely on this function on doing nothing for POVs.
		//If script want to set the heading and pitch on any gameplay camera (including POVs) it should use the function below CommandForceCameraRelativeHeadingAndPitch
	}

	void CommandResetGameplayCamFullAttachParentTransformTimer()
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();

		camThirdPersonCamera* thirdPersonCamera = camInterface::GetGameplayDirector().GetThirdPersonCamera();
		if(thirdPersonCamera)
		{
			thirdPersonCamera->ResetFullAttachParentMatrixForRelativeOrbitTimer();
		}
	}

    void CommandForceCameraRelativeHeadingAndPitch(float fHeading, float fPitch, float fPitchSmoothRate)
    {
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
        scriptcameraDebugf3("FC: %d - CommandForceCameraRelativeHeadingAndPitch( fHeading(%f), fPitch(%f), fPitchSmoothRate(%f) )", fwTimer::GetFrameCount(), fHeading, fPitch, fPitchSmoothRate);

        const float relativeHeadingToApply = fwAngle::LimitRadianAngleSafe(fHeading * DtoR);
        camInterface::GetGameplayDirector().SetUseScriptHeading(relativeHeadingToApply); 

        const float relativePitchToApply = Clamp(fPitch * DtoR, -HALF_PI, HALF_PI);
        camInterface::GetGameplayDirector().SetUseScriptPitch(relativePitchToApply, fPitchSmoothRate);

        //Get the cinematic director and check it is rendering a first person vehicle camera
        camCinematicDirector& cinematicDirector = camInterface::GetCinematicDirector();
        camCinematicMountedCamera* firstPersonVehicleCamera = cinematicDirector.GetFirstPersonVehicleCamera();
        if(firstPersonVehicleCamera)
        {
            firstPersonVehicleCamera->SetUseScriptHeading(relativeHeadingToApply);
            firstPersonVehicleCamera->SetUseScriptPitch(relativePitchToApply);
        }
    }

    void CommandForceBonnetCameraRelativeHeadingAndPitch(float fHeading, float fPitch)
    {
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
        const float relativeHeadingToApply = fwAngle::LimitRadianAngleSafe(fHeading * DtoR);
        const float relativePitchToApply = Clamp(fPitch * DtoR, -HALF_PI, HALF_PI);

        //Get the cinematic director and check it is rendering a first person vehicle camera
        camCinematicDirector& cinematicDirector = camInterface::GetCinematicDirector();

        //check the bonnet camera
        camCinematicMountedCamera* bonnetVehicleCamera = cinematicDirector.GetBonnetVehicleCamera();
        if(bonnetVehicleCamera)
        {
            bonnetVehicleCamera->SetUseScriptHeading(relativeHeadingToApply);
            bonnetVehicleCamera->SetUseScriptPitch(relativePitchToApply);
        }
    }

	void CommandSetFirstPersonShooterCameraHeading(float heading)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		camInterface::GetGameplayDirector().SetFirstPersonShooterCameraHeading( heading * DtoR );
	}
	
	void CommandSetFirstPersonShooterCameraPitch(float pitch)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		const float relativePitchToApply = Clamp(pitch * DtoR, -HALF_PI, HALF_PI);
		camInterface::GetGameplayDirector().SetFirstPersonShooterCameraPitch( relativePitchToApply );
	}

	void CommandSetScriptedCameraIsFirstPersonThisFrame(bool isFirstPersonThisFrame)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
#if FPS_MODE_SUPPORTED
		camInterface::SetScriptedCameraIsFirstPersonThisFrame(isFirstPersonThisFrame);
#endif
	}

	void CommandSetFollowCamIgnoreAttachParentMovementThisUpdate()
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		camInterface::GetGameplayDirector().SetShouldFollowCameraIgnoreAttachParentMovementThisUpdateForScript();
	}

	bool CommandIsGameFollowPedCamActive()
	{
		bool isActive = camInterface::GetGameplayDirector().IsFollowingPed();
		return isActive;
	}

	bool CommandSetFollowPedCamThisUpdate(const char* cameraName, int interpolationDuration)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		if(!SCRIPT_VERIFY(cameraName, "SET_FOLLOW_PED_CAM_THIS_UPDATE - NULL camera name"))
		{
			return false;
		}

		const u32 cameraNameHash = atStringHash(cameraName);
		tGameplayCameraSettings settings(NULL, cameraNameHash, interpolationDuration);

		const bool hasSucceeded = camInterface::GetGameplayDirector().SetScriptOverriddenFollowPedCameraSettingsThisUpdate(settings);

		SCRIPT_ASSERT_TWO_STRINGS(hasSucceeded, "SET_FOLLOW_PED_CAM_THIS_UPDATE has failed. Is this camera name valid: ", cameraName);

		return hasSucceeded;
	}

	int CommandGetFollowPedCamViewMode()
	{
		const int viewMode = camControlHelper::GetViewModeForContext(camControlHelperMetadataViewMode::ON_FOOT);

		return viewMode;
	}

	void CommandSetFollowPedCamViewMode(int viewMode)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		ASSERT_ONLY(const bool hasSucceeded =) camControlHelper::SetViewModeForContext(camControlHelperMetadataViewMode::ON_FOOT, viewMode);
		SCRIPT_ASSERT(hasSucceeded, "SET_FOLLOW_PED_CAM_VIEW_MODE failed, is the view mode valid?");
	}

	bool CommandIsGameFollowVehicleCamActive()
	{
		bool isActive = camInterface::GetGameplayDirector().IsFollowingVehicle();
		return isActive;
	}

	bool CommandSetFollowVehicleCamThisUpdate(const char* cameraName, int interpolationDuration)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		if(!SCRIPT_VERIFY(cameraName, "SET_FOLLOW_VEHICLE_CAM_THIS_UPDATE - NULL camera name"))
		{
			return false;
		}

		const u32 cameraNameHash = atStringHash(cameraName);
		tGameplayCameraSettings settings(NULL, cameraNameHash, interpolationDuration);

		const bool hasSucceeded = camInterface::GetGameplayDirector().SetScriptOverriddenFollowVehicleCameraSettingsThisUpdate(settings);

		SCRIPT_ASSERT_TWO_STRINGS(hasSucceeded, "SET_FOLLOW_VEHICLE_CAM_THIS_UPDATE has failed. Is this camera name valid: ", cameraName);

		return hasSucceeded;
	}

	void CommandSetFollowVehicleCamHighAngleModeThisUpdate(bool state)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		camInterface::GetGameplayDirector().SetScriptOverriddenFollowVehicleCameraHighAngleModeThisUpdate(state);
	}
	
	void CommandSetFollowVehicleCamHighAngleModeEveryUpdate(bool ShouldOverride, bool OverriddenState)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		camInterface::GetGameplayDirector().SetScriptOverriddenFollowVehicleCameraHighAngleModeEveryUpdate(ShouldOverride, OverriddenState);
	}

	//////////////////////////////////////////////////////////////////////////
	// Table games camera API
	bool CommandSetTableGamesCameraThisUpdate(int cameraNameHash)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		const atHashString cameraName((u32)cameraNameHash);
		if (!SCRIPT_VERIFY(cameraName.IsNotNull(), "SET_TABLE_GAMES_CAMERA_THIS_UPDATE - NULL camera name"))
		{
			return false;
		}

		tGameplayCameraSettings settings(nullptr, cameraName.GetHash(), 0); //interpolation duration is red from the camera metadata.

		const bool hasSucceeded = camInterface::GetGameplayDirector().SetScriptOverriddenTableGamesCameraSettingsThisUpdate(settings);

		SCRIPT_ASSERT_TWO_STRINGS(hasSucceeded, "SET_TABLE_GAMES_CAMERA_THIS_UPDATE has failed. Is this camera name valid: %s", SAFE_CSTRING(cameraName.GetCStr()));

		return hasSucceeded;
	}

	bool CommandIsTableGamesCameraActive()
	{
		return camInterface::GetGameplayDirector().IsTableGamesCameraRunning();
	}
	//////////////////////////////////////////////////////////////////////////

	//Deprecated! Replaced by CommandSetFollowVehicleCamViewMode
	void CommandSetGameFollowVehicleCamZoomLevel(int viewMode)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		//NOTE: The new view modes now match the legacy zoom levels, so it is not necessary to map the enum.
		ASSERT_ONLY(const bool hasSucceeded =) camControlHelper::SetViewModeForContext(camControlHelperMetadataViewMode::IN_VEHICLE, viewMode);
		SCRIPT_ASSERT(hasSucceeded, "SET_FOLLOW_VEHICLE_CAM_ZOOM_LEVEL failed, is the view mode valid?");
	}

	int CommandGetFollowVehicleCamViewMode()
	{
		const int viewMode = camControlHelper::GetViewModeForContext(camControlHelperMetadataViewMode::IN_VEHICLE);

		return viewMode;
	}

	void CommandSetFollowVehicleCamViewMode(int viewMode)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		ASSERT_ONLY(const bool hasSucceeded =) camControlHelper::SetViewModeForContext(camControlHelperMetadataViewMode::IN_VEHICLE, viewMode);
		SCRIPT_ASSERT(hasSucceeded, "SET_FOLLOW_VEHICLE_CAM_VIEW_MODE failed, is the view mode valid?");
	}

	int CommandGetCamViewModeForContext(int context)
	{
		const int viewMode = camControlHelper::GetViewModeForContext(context);

		return viewMode;
	}

	void CommandSetCamViewModeForContext(int context, int viewMode)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		ASSERT_ONLY(const bool hasSucceeded =) camControlHelper::SetViewModeForContext(context, viewMode);
		SCRIPT_ASSERT(hasSucceeded, "SET_CAM_VIEW_MODE_FOR_CONTEXT failed, are the context and view mode valid?");
	}

	int CommandGetCamActiveViewModeContext()
	{
		const int viewModeContext = camInterface::GetGameplayDirector().GetActiveViewModeContext();

		return viewModeContext;
	}

	void CommandUseVehicleCamStuntSettingsThisUpdate()
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		camInterface::GetGameplayDirector().ScriptRequestVehicleCamStuntSettingsThisUpdate();
	}

	void CommandUseDedicatedStuntCameraThisUpdate(const char* stuntCameraName)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		camInterface::GetGameplayDirector().ScriptRequestDedicatedStuntCameraThisUpdate(stuntCameraName);
	}

	void CommandForceVehicleCamStuntSettingsThisUpdate()
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		camInterface::GetGameplayDirector().ScriptForceVehicleCamStuntSettingsThisUpdate();
	}

    void CommandSetFollowVehicleCamSeatThisUpdate(int seatIndex)
    {
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
        camInterface::GetGameplayDirector().ScriptOverrideFollowVehicleCameraSeatThisUpdate(seatIndex);
    }

	bool CommandIsGameAimCamActive()
	{
		const bool isActive = camInterface::GetGameplayDirector().IsAiming();

		return isActive;
	}

	bool CommandIsGameAimCamActiveInAccurateMode()
	{
		const camThirdPersonAimCamera* camera	= camInterface::GetGameplayDirector().GetThirdPersonAimCamera();
		const bool isInAccurateMode				= camera && camera->IsInAccurateMode();

		return isInAccurateMode;
	}

	bool CommandIsGameFirstPersonAimCamActive()
	{
		const bool isActive = camInterface::GetGameplayDirector().IsFirstPersonAiming();

		return isActive;
	}

	void CommandDisableGameAimCamThisUpdate()
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		CPed* followPed = const_cast<CPed*>(camInterface::FindFollowPed());
		if(followPed && followPed->IsLocalPlayer())
		{
			CPlayerInfo* playerInfo = followPed->GetPlayerInfo();
			if(playerInfo)
			{
				playerInfo->GetPlayerResetFlags().SetFlag(CPlayerResetFlags::PRF_DISABLE_AIM_CAMERA);
			}
		}
	}

	//Deprecated! We no longer maintain a separate view mode for on foot aiming.
	int CommandGetPedAimCamViewMode()
	{
		return camControlHelperMetadataViewMode::THIRD_PERSON_MEDIUM;
	}

	//Deprecated! We no longer maintain a separate view mode for on foot aiming.
	void CommandSetPedAimCamViewMode(int UNUSED_PARAM(viewMode))
	{
	}

	float CommandGetGameFirstPersonAimCamZoomFactor()
	{
		const float zoomFactor = camControlHelper::GetZoomFactor();

		return zoomFactor;
	}

	void CommandSetGameFirstPersonAimCamZoomFactor(float zoomFactor)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		camControlHelper::SetZoomFactor(zoomFactor);
	}

	void CommandSetGameFirstPersonAimCamZoomFactorLimitsThisUpdate(float minZoomFactor, float maxZoomFactor)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		camControlHelper::OverrideZoomFactorLimits(minZoomFactor, maxZoomFactor);
	}
	
	void CommandSetGameFirstPersonAimCamNearClipThisUpdate(float NearClip)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		cameraDebugf3("%u SET_FIRST_PERSON_AIM_CAM_NEAR_CLIP_THIS_UPDATE %.3f %s", fwTimer::GetFrameCount(), NearClip, CTheScripts::GetCurrentScriptNameAndProgramCounter());

		camInterface::GetGameplayDirector().SetScriptOverriddenFirstPersonAimCameraNearClipThisUpdate(NearClip); 
	}

	void CommandSetGameThirdPersonAimCamNearClipThisUpdate(float NearClip)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		cameraDebugf3("%u SET_THIRD_PERSON_AIM_CAM_NEAR_CLIP_THIS_UPDATE %.3f %s", fwTimer::GetFrameCount(), NearClip, CTheScripts::GetCurrentScriptNameAndProgramCounter());

		camInterface::GetGameplayDirector().SetScriptOverriddenThirdPersonAimCameraNearClipThisUpdate(NearClip); 
	}

	void CommandSetGameFirstPersonAimCamRelativeHeadingLimitsThisUpdate(float minRelativeHeading, float maxRelativeHeading)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		const float minRelativeHeadingToApply = fwAngle::LimitRadianAngleSafe(minRelativeHeading * DtoR);
		const float maxRelativeHeadingToApply = fwAngle::LimitRadianAngleSafe(maxRelativeHeading * DtoR);

		Vector2 limits(minRelativeHeadingToApply, maxRelativeHeadingToApply);

		camInterface::GetGameplayDirector().SetScriptOverriddenFirstPersonAimCameraRelativeHeadingLimitsThisUpdate(limits);
	}

	void CommandSetGameFirstPersonAimCamRelativePitchLimitsThisUpdate(float minRelativePitch, float maxRelativePitch)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		minRelativePitch = Clamp(minRelativePitch, -90.0f, 90.0f);
		maxRelativePitch = Clamp(maxRelativePitch, -90.0f, 90.0f);

		Vector2 limits(minRelativePitch, maxRelativePitch);
		limits.Scale(DtoR);

		camInterface::GetGameplayDirector().SetScriptOverriddenFirstPersonAimCameraRelativePitchLimitsThisUpdate(limits);
	}

	void CommandSetAllowCustomVehicleDriveByCamThisUpdate(bool state)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		camInterface::GetGameplayDirector().SetShouldAllowCustomVehicleDriveByCamerasThisUpdate(state);
	}

	void CommandForceTightSpaceCustomFramingThisUpdate()
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		cameraDebugf3("Script is forcing the tight space custom framing this update");
		camInterface::GetGameplayDirector().ScriptForceUseTightSpaceCustomFramingThisUpdate();
	}

	bool CommandIsSphereVisible( const scrVector &scrVecCoors, float radius)
	{
		Vector3 vCoors (scrVecCoors); 
		
		return camInterface::IsSphereVisibleInGameViewport(vCoors, radius);
	}

	void CommandShakeGameplayCam(const char* ShakeName, float fAmplitudeScalar)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		camInterface::GetGameplayDirector().Shake(ShakeName, fAmplitudeScalar);
#if __BANK
		camInterface::GetScriptDirector().DebugSetScriptEnabledShakeName(ShakeName);
		camInterface::GetScriptDirector().DebugSetScriptEnabledShakeAmplitude(fAmplitudeScalar);
#endif // __BANK
	}

	bool CommandIsGameplayCamShaking()
	{
		bool isShaking = camInterface::GetGameplayDirector().IsShaking();
		return isShaking;
	}

	void CommandSetGameplayCamShakeAmplitude(float fAmplitudeScalar)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		camInterface::GetGameplayDirector().SetShakeAmplitude(fAmplitudeScalar);
#if __BANK
		camInterface::GetScriptDirector().DebugSetScriptEnabledShakeAmplitude(fAmplitudeScalar);
#endif // __BANK
	}

	void CommandStopGameplayCamShaking(bool shouldStopImmediately)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		camInterface::GetGameplayDirector().StopShaking(shouldStopImmediately);
#if __BANK
		camInterface::GetScriptDirector().DebugSetScriptDisabledShake();
#endif // __BANK
	}

	void CommandBypassCutsceneCamRenderingThisUpdate()
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		camInterface::GetCutsceneDirector().BypassRenderingThisUpdate(); 
	}

	void CommandShakeCutsceneCam(const char* ShakeName, float fAmplitudeScalar)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		camInterface::GetCutsceneDirector().Shake(ShakeName, fAmplitudeScalar);
#if __BANK
		camInterface::GetScriptDirector().DebugSetScriptEnabledShakeName(ShakeName);
		camInterface::GetScriptDirector().DebugSetScriptEnabledShakeAmplitude(fAmplitudeScalar);
#endif // __BANK
	}

	bool CommandIsCutsceneCamShaking()
	{
		bool isShaking = camInterface::GetCutsceneDirector().IsShaking();
		return isShaking;
	}

	void CommandSetCutsceneCamShakeAmplitude(float fAmplitudeScalar)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		camInterface::GetCutsceneDirector().SetShakeAmplitude(fAmplitudeScalar);
#if __BANK
		camInterface::GetScriptDirector().DebugSetScriptEnabledShakeAmplitude(fAmplitudeScalar);
#endif // __BANK
	}

	void CommandStopCutsceneCamShaking(bool shouldStopImmediately)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		camInterface::GetCutsceneDirector().StopShaking(shouldStopImmediately);
#if __BANK
		camInterface::GetScriptDirector().DebugSetScriptDisabledShake();
#endif // __BANK
	}

	void CommandSetCutsceneCamFarClipThisUpdate(float farClip)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		//NOTE: This functionality is multiplayer-only, as the single player game is now locked.
		if(NetworkInterface::IsGameInProgress())
		{
			camInterface::GetCutsceneDirector().OverrideFarClipThisUpdate(farClip);
		}
	}

	void CommandSetGameplayCamFollowPedThisUpdate(int pedIndex)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		const CPed* ped	= CTheScripts::GetEntityToQueryFromGUID<CPed>(pedIndex, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);
		cameraDebugf2("SET_GAMEPLAY_CAM_FOLLOW_PED_THIS_UPDATE: pedIndex %d pedName %s pedPosition <%.3f,%.3f,%.3f>", pedIndex, ped? ped->GetModelName() : "INVALID PED", ped ? VEC3V_ARGS(ped->GetMatrix().d()) : VEC3V_ARGS(Vec3V(V_ZERO)));
		if(ped)
		{
			camInterface::SetScriptControlledFollowPedThisUpdate(ped);
		}
	}

	bool CommandIsGameplayCamRendering()
	{
		const camGameplayDirector& gameplayDirector	= camInterface::GetGameplayDirector();
		const bool isRendering						= camInterface::IsDominantRenderedDirector(gameplayDirector);

		return isRendering;
	}

//Cinematic
	void CommandSetCinematicButtonActive(bool bActive)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		camInterface::GetCinematicDirector().SetCinematicButtonDisabledByScriptState(!bActive);
	}

	bool CommandIsCinematicCamRendering()
	{
		const camCinematicDirector& cinematicDirector	= camInterface::GetCinematicDirector();
		const bool isRendering							= camInterface::IsDominantRenderedDirector(cinematicDirector);

		return isRendering;
	}

	void CommandShakeCinematicCam(const char* ShakeName, float fAmplitudeScalar)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		camInterface::GetCinematicDirector().Shake(ShakeName, fAmplitudeScalar);
#if __BANK
		camInterface::GetScriptDirector().DebugSetScriptEnabledShakeName(ShakeName);
		camInterface::GetScriptDirector().DebugSetScriptEnabledShakeAmplitude(fAmplitudeScalar);
#endif // __BANK
	}

	bool CommandIsCinematicCamShaking()
	{
		bool isShaking = camInterface::GetCinematicDirector().IsShaking();
		return isShaking;
	}

	void CommandSetCinematicCamShakeAmplitude(float fAmplitudeScalar)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		camInterface::GetCinematicDirector().SetShakeAmplitude(fAmplitudeScalar);
#if __BANK
		camInterface::GetScriptDirector().DebugSetScriptEnabledShakeAmplitude(fAmplitudeScalar);
#endif // __BANK
	}

	void CommandStopCinematicCamShaking(bool shouldStopImmediately)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		camInterface::GetCinematicDirector().StopShaking(shouldStopImmediately);
#if __BANK
		camInterface::GetScriptDirector().DebugSetScriptDisabledShake();
#endif // __BANK
	}

	void CommandDisableCinematicBonnetCameraThisUpdate()
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		camInterface::GetCinematicDirector().DisableFirstPersonInVehicleThisUpdate();
	}

	void CommandDisableCinematicVehicleIdleModeThisUpdate()
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		camInterface::GetCinematicDirector().DisableVehicleIdleModeThisUpdate();
	}

	void CommandInvalidateCinematicVehicleIdleMode()
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		camInterface::GetCinematicDirector().InvalidateCinematicVehicleIdleMode();
	}

	bool CommandIsCinematicIdleCamRendering()
	{
		return camInterface::GetCinematicDirector().IsRenderingCinematicIdleCamera();
	}

	void CommandDisableCinematicSlowModeThisUpdate()
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		camInterface::GetCinematicDirector().DisableCinematicSlowMoThisUpdate();
	}

	bool CommandIsBonnetCinematicCamRendering()
	{
		// Note: this also returns true for point of view cinematic mounted cameras.
		return camInterface::GetCinematicDirector().IsRenderingCinematicMountedCamera();
	}

	bool CommandIsCinematicCamInputActive()
	{
		return camInterface::GetCinematicDirector().IsCinematicInputActive();
	}

	bool CommandIsCinematicFirstPersonVehicleInteriorCamRendering()
	{
		return camInterface::GetCinematicDirector().IsRenderingCinematicPointOfViewCamera();
	}

	void CommandInvalidateIdleCam()
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		camInterface::GetCinematicDirector().InvalidateIdleCamera();
	}

	void CommandCreateCinematicShot(int ShotRef, int Duration, int AttachEntityIndex, int LookAtEntityIndex)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		camCinematicDirector& cinematicDirector	= camInterface::GetCinematicDirector();
		camBaseCinematicContext* pContext = cinematicDirector.FindContext(camCinematicScriptContext::GetStaticClassId()); 
		
		if(pContext)
		{
			camCinematicScriptContext* pScriptContext = static_cast<camCinematicScriptContext*>(pContext); 
			scrThreadId scriptThreadId = CTheScripts::GetCurrentGtaScriptThread()->GetThreadId(); 
			
			const CEntity* pAttachEntity = NULL; 
			const CEntity* pLookAtEntity = NULL; 

			if(AttachEntityIndex != NULL_IN_SCRIPTING_LANGUAGE)
			{
				pAttachEntity = CTheScripts::GetEntityToQueryFromGUID<CEntity>(AttachEntityIndex, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);

				if(!scriptVerifyf(pAttachEntity, "COMMAND_CREATE_CINEMATIC_SHOT: Attach entity is null %s", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
				{
					return; 
				}
			}
			
			if(LookAtEntityIndex != NULL_IN_SCRIPTING_LANGUAGE)
			{
				pLookAtEntity = CTheScripts::GetEntityToQueryFromGUID<CEntity>(LookAtEntityIndex, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);

				if(!scriptVerifyf(pLookAtEntity, "COMMAND_CREATE_CINEMATIC_SHOT: Look at entity is null %s", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
				{
					return; 
				}
			} 

			 if(!pScriptContext->CreateScriptCinematicShot((u32)ShotRef, Duration, scriptThreadId, pAttachEntity, pLookAtEntity))
			 {
				scriptAssertf( 0, "CREATE_CINEMATIC_SHOT: %s: Failed to create a cinematic shot: %d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), ShotRef);
			 }

		}
	}

	bool CommandIsCinematicShotActive(int ShotRef)
	{
		camCinematicDirector& cinematicDirector	= camInterface::GetCinematicDirector();
		camBaseCinematicContext* pContext = cinematicDirector.FindContext(camCinematicScriptContext::GetStaticClassId()); 

		bool IsActive = false; 

		if(pContext->IsValid())
		{
			camCinematicScriptContext* pScriptContext = static_cast<camCinematicScriptContext*>(pContext); 

			if(pScriptContext)
			{
				IsActive = pScriptContext->IsScriptCinematicShotActive((u32)ShotRef); 
			}
		}
		return IsActive;
	}

	void CommandStopCinematicShot(int ShotRef)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		camCinematicDirector& cinematicDirector	= camInterface::GetCinematicDirector();
		camBaseCinematicContext* pContext = cinematicDirector.FindContext(camCinematicScriptContext::GetStaticClassId()); 

		if(pContext->IsValid())
		{
			camCinematicScriptContext* pScriptContext = static_cast<camCinematicScriptContext*>(pContext); 

			if(pScriptContext)
			{
				scrThreadId scriptThreadId = CTheScripts::GetCurrentGtaScriptThread()->GetThreadId(); 
				if(!pScriptContext->DeleteCinematicShot((u32)ShotRef, scriptThreadId))
				{
					scriptAssertf(0,"STOP_CINEMATIC_SHOT: %s: Failed to delete cinematic shot %d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), ShotRef ); 
				}
			}
		}
	}

	bool CommandIsCinematicShotRendering(int ShotRef)
	{
		camCinematicDirector& cinematicDirector	= camInterface::GetCinematicDirector();
		camBaseCinematicContext* pContext = cinematicDirector.FindContext(camCinematicScriptContext::GetStaticClassId()); 

		bool IsRendering = false; 

		if(pContext)
		{
			const camCinematicScriptContext* pScriptContext = static_cast<camCinematicScriptContext*>(pContext); 

			if(pScriptContext->IsValid())
			{
				const camBaseCinematicShot* pShot = static_cast<const camBaseCinematicShot*>(pScriptContext->GetCurrentShot()); 
				
				if(pShot && pShot->GetNameHash () == (u32)ShotRef)
				{
					const camBaseCamera* renderedCamera = camInterface::GetDominantRenderedCamera();
					if(renderedCamera && pContext->GetCurrentShotsCamera() && renderedCamera == pContext->GetCurrentShotsCamera())
					{
						IsRendering = true;
					}
				}
			}
		}
		return IsRendering;
	}

	void CommandScriptForceCinematicRenderingThisUpdate(bool OverrideCinematicInput)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		camInterface::GetCinematicDirector().SetScriptOverridesCinematicInputThisFrame(OverrideCinematicInput); 
	}
	
	void CommandSetCinematicNewsChannelActiveThisUpdate()
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		camInterface::GetCinematicDirector().SetCanActivateNewsChannelContext(true); 	
	}
	
	void  CommandCinematicMissionCreatorFailContextActiveThisUpdate()
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		camInterface::GetCinematicDirector().SetCanActivateMissionCreatorFailContext(true); 	
	}

	void CommandSetCinematicModeActive(bool Active)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		camInterface::GetCinematicDirector().SetCinematicLatchedStateForScript(Active); 
	}

	bool CommandRegisterRaceCheckPoint(const scrVector &scrVecCoors)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		Vector3 vCoors (scrVecCoors);

		camCinematicDirector& cinematicDirector	= camInterface::GetCinematicDirector();
		camBaseCinematicContext* pContext = cinematicDirector.FindContext(camCinematicScriptedRaceCheckPointContext::GetStaticClassId()); 

		if(pContext)
		{
			camCinematicScriptedRaceCheckPointContext* pScriptContext = static_cast<camCinematicScriptedRaceCheckPointContext*>(pContext); 

			if(pScriptContext)
			{
				return pScriptContext->RegisterPosition(vCoors); 
			}
		}
		return false; 
	}
	
	bool CommandIsInVehicleMobilePhoneCameraRendering()
	{
		bool IsRendering = false;

		const camBaseCamera* renderedCamera = camInterface::GetDominantRenderedCamera();

		camCinematicDirector& cinematicDirector	= camInterface::GetCinematicDirector();

		//First check the custom first-person context that is conventionally used for the mobile phone camera in a vehicle.
		camBaseCinematicContext* pContext = cinematicDirector.FindContext(camCinematicInVehicleOverriddenFirstPersonContext::GetStaticClassId()); 
		if(pContext)
		{
			const camCinematicInVehicleOverriddenFirstPersonContext* pFirstPersonContext = static_cast<const camCinematicInVehicleOverriddenFirstPersonContext*>(pContext); 

			if(pFirstPersonContext)
			{
				if(pFirstPersonContext->IsValid())
				{
					const camBaseCinematicShot* pShot = static_cast<const camBaseCinematicShot*>(pFirstPersonContext->GetCurrentShot()); 

					if(pShot)
					{
						if(renderedCamera && pContext->GetCurrentShotsCamera() && renderedCamera == pContext->GetCurrentShotsCamera())
						{
							IsRendering = true;
						}
					}
				}
			}
		}

		if(!IsRendering)
		{
			//Otherwise, check the context that is used to render a first-person POV shot for passengers in certain vehicles within Multiplayer.
			pContext = cinematicDirector.FindContext(camCinematicInVehicleMultiplayerPassengerContext::GetStaticClassId()); 
			if(pContext)
			{
				const camCinematicInVehicleMultiplayerPassengerContext* pMultiplayerPassengerContext = static_cast<const camCinematicInVehicleMultiplayerPassengerContext*>(pContext); 

				if(pMultiplayerPassengerContext)
				{
					if(pMultiplayerPassengerContext->IsValid())
					{
						const camBaseCinematicShot* pShot = static_cast<const camBaseCinematicShot*>(pMultiplayerPassengerContext->GetCurrentShot()); 

						if(pShot)
						{
							if(renderedCamera && pContext->GetCurrentShotsCamera() && renderedCamera == pContext->GetCurrentShotsCamera())
							{
								IsRendering = true;
							}
						}
					}
				}
			}
		}

		return IsRendering;
	}

    void CommandIgnoreMenuPreferenceForBonnetCameraThisUpdate()
    {
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
        camInterface::GetCinematicDirector().IgnoreMenuPreferenceForBonnetCameraThisUpdate();
    }

//Hint
	//legacy

	void CommandSetGameplayHintRelativePitch(float UNUSED_PARAM(relativePitch), float UNUSED_PARAM(smoothRate))
	{
		//camInterface::GetGameplayDirector().GetHintHelper().SetDesiredRelativePitch(relativePitch * DtoR, smoothRate);
	}
	
	void CommandSetGameplayHintFov(float fFov)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		if(fFov >= g_MinFov && fFov <= g_MaxFov)
		{
			camInterface::GetGameplayDirector().SetScriptHintFov(fFov);
		}
	}

	void CommandSetGameplayHintFollowDistanceScalar(float Distance)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		camInterface::GetGameplayDirector().SetScriptHintDistanceScalar(Distance);
	}

	void CommandSetGameplayHintBaseOrbitPitchOffset(float fOffset)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		camInterface::GetGameplayDirector().SetScriptHintBaseOrbitPitchOffset(fOffset);
	}
	
	void CommandSetGameplayHintCameraRelativeSideOffsetAdditive(float fOffset)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		camInterface::GetGameplayDirector().SetScriptHintCameraRelativeSideOffsetAdditive(fOffset);
	}
	
	void CommandSetGameplayHintCameraRelativeVerticalOffsetAdditive(float fOffset)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		camInterface::GetGameplayDirector().SetScriptHintCameraRelativeVerticalOffsetAdditive(fOffset);
	}

	void CommandSetGameplayHintCameraBlendToFollowPedMediumViewMode(bool state)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		camInterface::GetGameplayDirector().SetScriptHintCameraBlendToFollowPedMediumViewMode(state);
	}

	void CommandSetGameplayCoordHint(const scrVector &scrVecCoors,int dwellDuration, int interpolateInDuration, int interpolateOutDuration, int HintType)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		Vector3 vCoors (scrVecCoors);

		camInterface::GetGameplayDirector().StartHinting(vCoors, interpolateInDuration, dwellDuration, interpolateOutDuration, HintType); 
	
	}
	
	void CommandSetGameplayEntityHint(int EntityIndex, const scrVector &scrOffset, bool bRelativeOffset, int dwellDuration, int interpolateInDuration, int interpolateOutDuration, int HintType)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		Vector3 OffsetCoords (scrOffset);

		CEntity* pEntity = CTheScripts::GetEntityToModifyFromGUID<CEntity>(EntityIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
		
		if (pEntity)
		{
			camInterface::GetGameplayDirector().StartHinting(pEntity, OffsetCoords , bRelativeOffset , interpolateInDuration, dwellDuration, interpolateOutDuration, HintType); 
		}
	}
	
	bool CommandIsGameplayHintActive()
	{
		if (!camInterface::GetGameplayDirector().IsCodeHintActive() && camInterface::GetGameplayDirector().IsHintActive())
		{
			return true; 
		}

		return false;
	}

	bool CommandIsCodeGameplayHintActive()
	{
		return camInterface::GetGameplayDirector().IsCodeHintActive();
	}

	void CommandStopGameplayHint(bool bStopImmediately)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		if(!camInterface::GetGameplayDirector().IsCodeHintActive() BANK_ONLY(&& !camInterface::GetGameplayDirector().IsDebugHintActive()))
		{
			camInterface::GetGameplayDirector().StopHinting(bStopImmediately);
		}
	}

	void CommandStopCodeGamePlayHint(bool bStopImmediately)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		if(camInterface::GetGameplayDirector().IsCodeHintActive())
		{	
			camInterface::GetGameplayDirector().StopHinting(bStopImmediately);
		}
	}

	void CommandStopGameplayHintBeingCancelledUsingRightStick(bool UNUSED_PARAM(bCanCancel))
	{
		//Deprecated.
		//camInterface::GetGameplayDirector().SetStopHintBeingCancelled(bCanCancel); 
	}

	void CommmandBlockCodeHintThisUpdate()
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		camInterface::GetGameplayDirector().BlockCodeHintThisUpdate(); 
	}

//Cached frame
	scrVector CommandGetRenderedCamCoord()
	{
		return camInterface::GetPos();
	}

	scrVector CommandGetRenderedCamRot (int RotOrder)
	{
		Vector3 vRot(VEC3_ZERO);

		const Matrix34& worldMatrix = camInterface::GetMat();
		vRot = CScriptEulers::MatrixToEulers(worldMatrix, static_cast<EulerAngleOrder>(RotOrder));
		vRot *= RtoD;
	
		return vRot;
	}

    scrVector CommandGetRenderedRemotePlayerCamRot(int PlayerIndex, int RotOrder)
	{
        Vector3 vRot(VEC3_ZERO);

        CPed *pPlayerPed = CTheScripts::FindNetworkPlayerPed(PlayerIndex);

        if(pPlayerPed)
	    {
            SCRIPT_ASSERT(!pPlayerPed->IsLocalPlayer(), "GET_FINAL_RENDERED_REMOTE_PLAYER_CAM_ROT - Can't call this on the local player!");

            if(!pPlayerPed->IsLocalPlayer())
            {
                Matrix34 worldMatrix;
                NetworkInterface::GetRemotePlayerCameraMatrix(pPlayerPed, worldMatrix);

		        vRot = CScriptEulers::MatrixToEulers(worldMatrix, static_cast<EulerAngleOrder>(RotOrder));
		        vRot *= RtoD;
            }
        }
	
		return vRot;
	}

	float CommandGetRenderedCamFov()
	{
		return camInterface::GetFov();
	}

    float CommandGetRenderedRemotePlayerCamFov(int PlayerIndex)
    {
        CPed *pPlayerPed = CTheScripts::FindNetworkPlayerPed(PlayerIndex);

        if(pPlayerPed)
	    {
            SCRIPT_ASSERT(!pPlayerPed->IsLocalPlayer(), "GET_FINAL_RENDERED_REMOTE_PLAYER_CAM_FOV - Can't call this on the local player!");

            if(!pPlayerPed->IsLocalPlayer())
            {
                return NetworkInterface::GetRemotePlayerCameraFov(pPlayerPed);
            }
        }

        return g_DefaultFov;
    }

	float CommandGetRenderedCamNearClip()
	{
		return camInterface::GetNear();
	}

	float CommandGetRenderedCamFarClip()
	{
		return camInterface::GetFar();
	}

	float CommandGetRenderedCamNearDof()
	{
		return camInterface::GetNearDof();
	}

	float CommandGetRenderedCamFarDof()
	{
		return camInterface::GetFarDof(); 
	} 

	float CommandGetRenderedCamMotionBlurStrength()
	{
		return camInterface::GetMotionBlurStrength(); 
	} 
	
	void CommandUseScriptCamForAmbientPopulationOriginThisFrame(bool bVehicles, bool bPeds)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		if(camInterface::GetScriptDirector().IsRendering())
		{
			if(bVehicles)
				CVehiclePopulation::SetScriptCamPopulationOriginOverride();

			if(bPeds)
				CPedPopulation::SetScriptCamPopulationOriginOverride();
		}
	}

	void CommandSetFollowPedCamLadderAlignThisUpdate()
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		camFollowPedCamera* followPedCamera = camInterface::GetGameplayDirector().GetFollowPedCamera();
		if(followPedCamera)
		{
			followPedCamera->SetScriptForceLadder(true);
		}
	}

	void CommandSetThirdPersonOrbitDistanceThisUpdate(float minDistance, float maxDistance)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		Vector2 limits(minDistance, maxDistance);
		camInterface::GetGameplayDirector().SetScriptOverriddenThirdPersonCameraOrbitDistanceLimitsThisUpdate(limits);
	}

	void CommandSetThirdPersonRelativePitchThisUpdate(float minRelativePitch, float maxRelativePitch)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		minRelativePitch = Clamp(minRelativePitch, -90.0f, 90.0f);
		maxRelativePitch = Clamp(maxRelativePitch, -90.0f, 90.0f);

		Vector2 limits(minRelativePitch, maxRelativePitch);
		limits.Scale(DtoR);

		camInterface::GetGameplayDirector().SetScriptOverriddenThirdPersonCameraRelativePitchLimitsThisUpdate(limits);
	}

	void CommandSetThirdPersonRelativeHeadingThisUpdate(float minRelativeHeading, float maxRelativeHeading)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		minRelativeHeading = Clamp(minRelativeHeading, -180.0f, 180.0f);
		maxRelativeHeading = Clamp(maxRelativeHeading, -180.0f, 180.0f);

		Vector2 limits(minRelativeHeading, maxRelativeHeading);
		limits.Scale(DtoR);

		camInterface::GetGameplayDirector().SetScriptOverriddenThirdPersonCameraRelativeHeadingLimitsThisUpdate(limits);
	}
	
	void CommandSetInVehicleCameraStateThisUpdate(int VehicleIndex, int CamInVehicleState )
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		if(CamInVehicleState != camGameplayDirector::OUTSIDE_VEHICLE)
		{
			CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);

			if(pVehicle)
			{
				camInterface::GetGameplayDirector().SetScriptOverriddenVehicleExitEntryState(pVehicle, CamInVehicleState); 
			}
			else
			{
				return; 
			}
		}
		else
		{
			camInterface::GetGameplayDirector().SetScriptOverriddenVehicleExitEntryState(NULL, CamInVehicleState); 
		}
	}

	void CommandDisableOnFootFirstPersonViewThisUpdate()
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		FPS_MODE_SUPPORTED_ONLY( camInterface::GetGameplayDirector().DisableFirstPersonThisUpdate(NULL, true); )
	}

	void CommandDisableFirstPersonFlashEffectThisUpdate()
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		FPS_MODE_SUPPORTED_ONLY( camManager::SuppressFirstPersonFlashEffectsThisFrame(true); )
	}

	void CommandForceThirdPersonAferAimingBlendThisUpdate()
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		camInterface::GetGameplayDirector().ForceAfterAimingBlendThisUpdate();
	}

	void CommandDisableThirdPersonAferAimingBlendThisUpdate()
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		camInterface::GetGameplayDirector().DisableAfterAimingBlendThisUpdate();
	}

    void CommandBlockFirstPersonOrientationResetThisUpdate()
    {
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
        camFirstPersonShooterCamera* firstPersonShooterCamera = camInterface::GetGameplayDirector().GetFirstPersonShooterCamera();
        if(firstPersonShooterCamera)
        {
            firstPersonShooterCamera->SetScriptPreventingOrientationResetWhenRenderingAnotherCamera();
        }
    }

	//to be removed
	void CommandSetGameplayPedHint( int PedIndex, const scrVector &scrOffset, bool bRelativeOffset, int dwellDuration, int interpolateInDuration, int interpolateOutDuration )
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		Vector3 vOffset (scrOffset);
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);

		if (pPed)
		{
			camInterface::GetGameplayDirector().StartHinting(pPed, vOffset , bRelativeOffset , interpolateInDuration, dwellDuration, interpolateOutDuration, 0); 
		}
	}
	//to be removed
	void CommandSetGameplayVehicleHint(int VehicleIndex, const scrVector &scrOffset, bool bRelativeOffset, int dwellDuration, int interpolateInDuration, int interpolateOutDuration )
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		Vector3 vOffset (scrOffset);
		 CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);

		if (pVehicle)
		{
			camInterface::GetGameplayDirector().StartHinting(pVehicle, vOffset , bRelativeOffset , interpolateInDuration, dwellDuration, interpolateOutDuration, 0); 
		}
	}
	//to be removed
	void CommandSetGameplayObjectHint(int ObjectIndex ,  const scrVector &scrOffset, bool bRelativeOffset, int dwellDuration, int interpolateInDuration, int interpolateOutDuration)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		Vector3 vOffset (scrOffset);
		 CObject* pObject = CTheScripts::GetEntityToModifyFromGUID<CObject>(ObjectIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);

		if (pObject)
		{
			camInterface::GetGameplayDirector().StartHinting(pObject, vOffset , bRelativeOffset , interpolateInDuration, dwellDuration, interpolateOutDuration, 0); 
		}
	}

	int CommandGetFocusPedOnScreen(float maxValidDistance, int screenPositionTestBoneTag, float maxScreenWidthRatioAroundCentreForTestBone, float maxScreenHeightRatioAroundCentreForTestBone, float minRelativeHeadingScore, float maxScreenCentreScoreBoost, float maxScreenRatioAroundCentreForScoreBoost, int losTestBoneTag1, int losTestBoneTag2)
	{
		CPed* focusPed		= camInterface::ComputeFocusPedOnScreen(maxValidDistance, screenPositionTestBoneTag,
								maxScreenWidthRatioAroundCentreForTestBone, maxScreenHeightRatioAroundCentreForTestBone, minRelativeHeadingScore,
								maxScreenCentreScoreBoost, maxScreenRatioAroundCentreForScoreBoost, losTestBoneTag1, losTestBoneTag2);
		int focusPedIndex	= focusPed ? CTheScripts::GetGUIDFromEntity(*focusPed) : 0;

		return focusPedIndex;
	}

	void CommandDisableNearClipScanThisUpdate()
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		camInterface::DisableNearClipScanThisUpdate();
	}

	void CommandSetCamDeathFailEffectState(s32 state)
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		camInterface::SetDeathFailEffectState(state);
	}

	void CommandStartCameraCapture(int BANK_ONLY(cameraIndex), const char* BANK_ONLY(animName))
	{
#if __BANK
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		camBaseCamera* pCamera = GetCameraFromIndex(cameraIndex, "START_CAMERA_CAPTURE");
		if (pCamera)
		{
			camBaseCamera::Pool *pBaseCameraPool = camBaseCamera::GetPool();
			if(SCRIPT_VERIFY(pBaseCameraPool, "START_CAMERA_CAPTURE - Could not get specified camera!"))
			{
				for(int i = 0; i < pBaseCameraPool->GetSize(); i ++)
				{
					if(pCamera == pBaseCameraPool->GetSlot(i))
					{
						cameraIndex = i;

						break;
					}
				}

				if(SCRIPT_VERIFY(pCamera == pBaseCameraPool->GetSlot(cameraIndex), "START_CAMERA_CAPTURE"))
				{
					if (SCRIPT_VERIFY(animName && strlen(animName) > 0, "START_CAMERA_CAPTURE - No anim name was specified!"))
					{
						if (SCRIPT_VERIFY(!CAnimViewer::m_bCameraCapture_Started, "START_CAMERA_CAPTURE - Camera capture is already running!"))
						{
							CAnimViewer::CameraCapture_Start(cameraIndex, animName);
						}
					}
				}
			}
		}
#endif
	}

	void CommandStopCameraCapture()
	{
#if __BANK
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		if (SCRIPT_VERIFY(CAnimViewer::m_bCameraCapture_Started, "STOP_CAMERA_CAPTURE - Camera capture has not been started!"))
		{
			CAnimViewer::CameraCapture_Stop();
		}
#endif
	}

	void CommandSetFirstPersonFlashEffectType(s32 type)
	{
#if FPS_MODE_SUPPORTED
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		camInterface::GetScriptDirector().SetFirstPersonFlashEffectType((u32)type);
#endif //FPS_MODE_SUPPORTED
	}

	void CommandSetFirstPersonFlashEffectVehicleModelName(const char * pName)
	{
#if FPS_MODE_SUPPORTED
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		camInterface::GetScriptDirector().SetFirstPersonFlashEffectVehicleModelHash(atStringHash(pName));
#endif //FPS_MODE_SUPPORTED
	}

	void CommandSetFirstPersonFlashEffectVehicleModelHash(int modelHash)
	{
#if FPS_MODE_SUPPORTED
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		camInterface::GetScriptDirector().SetFirstPersonFlashEffectVehicleModelHash((u32)modelHash);
#endif //FPS_MODE_SUPPORTED
	}

	bool CommandIsAllowedIndependentCameraModes()
	{
#if FPS_MODE_SUPPORTED
		const bool bAllowIndependentViewModes = (CPauseMenu::GetMenuPreference( PREF_FPS_PERSISTANT_VIEW ) == TRUE);
		return bAllowIndependentViewModes;
#else
		return true;
#endif
	}

	void CommandPreventCollisionSettingsForTripleHeadInInteriorsThisUpdate()
	{
		REGISTER_CAMERA_SCRIPT_COMMAND_NO_ARGS();
		camInterface::GetGameplayDirector().SetScriptPreventCollisionSettingsForTripleHeadInInteriorsThisUpdate();
	}

	float CommandReplayGetMaxDistanceAllowedFromPlayer()
	{
#if GTA_REPLAY
		return camInterface::GetReplayDirector().GetMaxDistanceAllowedFromPlayer();
#else
		return 0.0f;
#endif
	}

	//
	// --------------------------------------------------------------------
	//
	void SetupScriptCommands()
	{
//Render cams
		SCR_REGISTER_SECURE(RENDER_SCRIPT_CAMS,0x850d4ef3d40fb068,	CommandRenderScriptCams);
		SCR_REGISTER_SECURE(STOP_RENDERING_SCRIPT_CAMS_USING_CATCH_UP,0xbaea2321313356d0,	CommandStopRenderingScriptCamsUsingCatchUp);

//create/destroy
		SCR_REGISTER_SECURE(CREATE_CAM,0xb93a4265cb0f5b1f,	CommandCreateCam);
		SCR_REGISTER_SECURE(CREATE_CAM_WITH_PARAMS,0x6729fa3af971be2a,	CommandCreateCamWithParams);
		SCR_REGISTER_SECURE(CREATE_CAMERA,0xae96f9aff21228dc,	CommandCreateCameraFromHash);
		SCR_REGISTER_SECURE(CREATE_CAMERA_WITH_PARAMS,0x1bcec0b1056bd6ac,	CommandCreateCameraWithParams);
		SCR_REGISTER_SECURE(DESTROY_CAM,0x588ddcb644c6486a,	CommandDestroyCam);
		SCR_REGISTER_SECURE(DESTROY_ALL_CAMS,0x4b248030f70a5d8b, CommandDestroyAllCams);
		SCR_REGISTER_SECURE(DOES_CAM_EXIST,0x876b1078e90c91cb,	CommandDoesCamExist);
//activate cam
		SCR_REGISTER_SECURE(SET_CAM_ACTIVE,0xdd786b89b15aa63f,	CommandSetCamActive);
		SCR_REGISTER_SECURE(IS_CAM_ACTIVE,0xa24fda4986456697, CommandIsCamActive);
		SCR_REGISTER_SECURE(IS_CAM_RENDERING,0x4b0b4e357722c507, CommandIsCamRendering);
		SCR_REGISTER_SECURE(GET_RENDERING_CAM,0x0065a2cb8ececaf1,	CommandGetRenderingCam);
//frame parameters
		SCR_REGISTER_SECURE(GET_CAM_COORD,0x97a9bb81c66772b5,	CommandGetCamCoord);
		SCR_REGISTER_SECURE(GET_CAM_ROT,0xe42645792657f001,	CommandGetCamRotation);
		SCR_REGISTER_SECURE(GET_CAM_FOV,0x703cb0b4057dddf5,	CommandGetCamFov);
		SCR_REGISTER_SECURE(GET_CAM_NEAR_CLIP,0x998fe8dab2221f27,	CommandGetCamNearClip);
		SCR_REGISTER_SECURE(GET_CAM_FAR_CLIP,0x9953cc74efa4f571,	CommandGetCamFarClip);
		SCR_REGISTER_SECURE(GET_CAM_NEAR_DOF,0xc2612d223d915a1c,	CommandGetCamNearDof);
		SCR_REGISTER_SECURE(GET_CAM_FAR_DOF,0x1abe956c93973baa,	CommandGetCamFarDof);
		SCR_REGISTER_SECURE(GET_CAM_DOF_STRENGTH,0x06d153c0b99b6128,	CommandGetCamDofStrength);
		SCR_REGISTER_UNUSED(GET_CAM_DOF_PLANES,0xa914246d4f8a957c,	CommandGetCamDofPlanes);
		SCR_REGISTER_UNUSED(GET_CAM_USE_SHALLOW_DOF_MODE,0xd119f6b1fb0b65b6,	CommandGetCamUseShallowDofMode);
		SCR_REGISTER_UNUSED(GET_CAM_MOTION_BLUR_STRENGTH,0xf1ccbbc140a5d518,	CommandGetCamMotionBlurStrength);

		SCR_REGISTER_SECURE(SET_CAM_PARAMS,0x15f5db94f871e803,	CommandSetCamParams);

		SCR_REGISTER_SECURE(SET_CAM_COORD,0xdee4f5f0b93be664,	CommandSetCamCoord);
		SCR_REGISTER_SECURE(SET_CAM_ROT,0x93150d31ce38fe75,	CommandSetCamRotation);
		SCR_REGISTER_SECURE(SET_CAM_FOV,0x0a18c027350d3c19,	CommandSetCamFov);
		SCR_REGISTER_SECURE(SET_CAM_NEAR_CLIP,0x74789da66781db64,	CommandSetCamNearClip);
		SCR_REGISTER_SECURE(SET_CAM_FAR_CLIP,0xac79343a28cea8e0,	CommandSetCamFarClip);
		SCR_REGISTER_SECURE(FORCE_CAM_FAR_CLIP,0xd9b532b49a80dfe1, CommandForceCamFarClip);
		SCR_REGISTER_SECURE(SET_CAM_MOTION_BLUR_STRENGTH,0x8e55b7fed2291f41,	CommandSetCamMotionBlurStrength);

//depth of field
		SCR_REGISTER_SECURE(SET_CAM_NEAR_DOF,0x1d809c24759a3fed,	CommandSetCamNearDof);
		SCR_REGISTER_SECURE(SET_CAM_FAR_DOF,0xe13ce1cd336ffac9,	CommandSetCamFarDof);
		SCR_REGISTER_SECURE(SET_CAM_DOF_STRENGTH,0xd6a650055b44902e,	CommandSetCamDofStrength);
		SCR_REGISTER_SECURE(SET_CAM_DOF_PLANES,0xe5744a8f282963ce,	CommandSetCamDofPlanes);
		SCR_REGISTER_SECURE(SET_CAM_USE_SHALLOW_DOF_MODE,0xcaadf41aa439a01f,	CommandSetCamUseShallowDofMode);
		SCR_REGISTER_SECURE(SET_USE_HI_DOF,0x44908cda9d1050c9, CommandUseHiDof);
		SCR_REGISTER_SECURE(SET_USE_HI_DOF_ON_SYNCED_SCENE_THIS_UPDATE, 0x731a880555da3647, CommandUseHiDofOnSyncedSceneThisUpdate);
		SCR_REGISTER_SECURE(SET_CAM_DOF_OVERRIDDEN_FOCUS_DISTANCE,0x1bf7583954f8933d, CommandSetDofOverriddenFocusDistance);
		SCR_REGISTER_SECURE(SET_CAM_DOF_OVERRIDDEN_FOCUS_DISTANCE_BLEND_LEVEL,0x718303d9313f7a97, CommandSetDofOverriddenFocusDistanceBlendLevel);
		SCR_REGISTER_UNUSED(SET_CAM_DOF_FOCUS_DISTANCE_GRID_SCALING,0xe9ac8c191ff37e64, CommandSetDofFocusDistanceGridScaling);
		SCR_REGISTER_UNUSED(SET_CAM_DOF_SUBJECT_MAGNIFICATION_POWER_FACTOR_NEAR_FAR,0x432e7a511de8170c, CommandSetDofSubjectMagnificationPowerFactorNearFar);
		SCR_REGISTER_UNUSED(SET_CAM_DOF_MAX_PIXEL_DEPTH,0xe8cd4c1732381e72, CommandSetDofMaxPixelDepth);
		SCR_REGISTER_UNUSED(SET_CAM_DOF_PIXEL_DEPTH_POWER_FACTOR,0xddbcf51529e88a3f, CommandSetDofPixelDepthPowerFactor);
		SCR_REGISTER_SECURE(SET_CAM_DOF_FNUMBER_OF_LENS,0x5ac409e1be1df384, CommandSetDofFNumberOfLens);
		SCR_REGISTER_SECURE(SET_CAM_DOF_FOCAL_LENGTH_MULTIPLIER,0xd3a34fb28d95d754, CommandSetDofFocalLengthMultiplier);
		SCR_REGISTER_SECURE(SET_CAM_DOF_FOCUS_DISTANCE_BIAS,0x6610a3e15c9aba9b, CommandSetDofFocusDistanceBias);
		SCR_REGISTER_SECURE(SET_CAM_DOF_MAX_NEAR_IN_FOCUS_DISTANCE,0xd4bc9962fda90b40, CommandSetDofMaxNearInFocusDistance);
		SCR_REGISTER_SECURE(SET_CAM_DOF_MAX_NEAR_IN_FOCUS_DISTANCE_BLEND_LEVEL,0x6344c0c82c899078, CommandSetDofMaxNearInFocusDistanceBlendLevel);
		SCR_REGISTER_UNUSED(SET_CAM_DOF_MAX_BLUR_RADIUS_AT_NEAR_IN_FOCUS_LIMIT,0xace748c80de5bf76, CommandSetDofMaxBlurRadiusAtNearInFocusLimit);
		SCR_REGISTER_UNUSED(SET_CAM_DOF_FOCUS_DISTANCE_INCREASE_SPRING_CONSTANT,0xa91ec7a1aa4dbc17, CommandSetDofFocusDistanceIncreaseSpringConstant);
		SCR_REGISTER_UNUSED(SET_CAM_DOF_FOCUS_DISTANCE_DECREASE_SPRING_CONSTANT,0x3ce8a2efc3febd6d, CommandSetDofFocusDistanceDecreaseSpringConstant);
		SCR_REGISTER_UNUSED(SET_CAM_DOF_SHOULD_FOCUS_ON_LOOK_AT_TARGET,0xdc1142c71eefffd2, CommandSetDofShouldFocusOnLookAtTarget);
		SCR_REGISTER_UNUSED(SET_CAM_DOF_SHOULD_FOCUS_ON_ATTACH_PARENT,0xf460dab6f0323c0a, CommandSetDofShouldFocusOnAttachParent);
		SCR_REGISTER_UNUSED(SET_CAM_DOF_SHOULD_KEEP_LOOK_AT_TARGET_IN_FOCUS,0xfcc9acbd7bfa65a0, CommandSetDofShouldKeepLookAtTargetInFocus);
		SCR_REGISTER_UNUSED(SET_CAM_DOF_SHOULD_KEEP_ATTACH_PARENT_IN_FOCUS,0x813b285667a55baf, CommandSetDofShouldKeepAttachParentInFocus);
		SCR_REGISTER_UNUSED(SET_CAM_DOF_SHOULD_MEASURE_POST_ALPHA_PIXEL_DEPTH,0x0264fdab5f244cc5, CommandSetDofShouldMeasurePostAlphaPixelDepth);

//attach
		SCR_REGISTER_SECURE(ATTACH_CAM_TO_ENTITY,0x3fb84450a94cb528,	CommandAttachCamToEntity);
		SCR_REGISTER_SECURE(ATTACH_CAM_TO_PED_BONE,0xb6b6ac43334c34a7,	CommandAttachCamToPedBone);
		SCR_REGISTER_SECURE(HARD_ATTACH_CAM_TO_PED_BONE,0x1cc06cd0e210ff39,	CommandHardAttachCamToPedBone);
		SCR_REGISTER_SECURE(HARD_ATTACH_CAM_TO_ENTITY,0x383a38da6b16db3a,	CommandHardAttachCamToEntity);
		SCR_REGISTER_SECURE(ATTACH_CAM_TO_VEHICLE_BONE,0xbe3ecd06b6bdf192,	CommandAttachCamToVehicleBone);
		SCR_REGISTER_SECURE(DETACH_CAM,0xdf900bcbc752c62a,	CommandDetachCam);	
//roll
		SCR_REGISTER_UNUSED(SET_CAM_INHERIT_ROLL_OBJECT,0x465a41a2097c11f0,	CommandSetCamInheritRollObject); 
		SCR_REGISTER_SECURE(SET_CAM_INHERIT_ROLL_VEHICLE,0x8fd169d152a83209,	CommandSetCamInheritRollVehicle);
//point
		SCR_REGISTER_SECURE(POINT_CAM_AT_COORD,0x46045ff49485808b,	CommandPointCamAtCoord);
		SCR_REGISTER_SECURE(POINT_CAM_AT_ENTITY,0x98a99a554b458431,	CommandPointCamAtEntity);
		SCR_REGISTER_SECURE(POINT_CAM_AT_PED_BONE,0x4940b67d34c2f6c1,	CommandPointCamAtPedBone); 
		SCR_REGISTER_UNUSED(POINT_CAM_AT_CAM,0x3482608561b83d01,	CommandPointCamAtCam);
		SCR_REGISTER_SECURE(STOP_CAM_POINTING,0x45e6cc1354100c1b,	CommandStopCamPointing);
//miscellaneous camera-specific commands
		SCR_REGISTER_SECURE(SET_CAM_AFFECTS_AIMING,0x156db56766eded12, CommandSetCamAffectsAiming);
		SCR_REGISTER_SECURE(SET_CAM_CONTROLS_MINI_MAP_HEADING,0x8d32379ec749b177, CommandSetCamControlsMiniMapHeading);
		SCR_REGISTER_SECURE(SET_CAM_IS_INSIDE_VEHICLE,0x225b8233719be2b5, CommandSetCamIsInsideVehicle);
		SCR_REGISTER_UNUSED(SET_CAM_CUSTOM_MAX_NEAR_CLIP,0x1df22d878d47fb84, CommandSetCustomMaxNearClip);
		SCR_REGISTER_SECURE(ALLOW_MOTION_BLUR_DECAY,0x7bd08352b65a3135, CommandAllowMotionBlurDecay);
		SCR_REGISTER_SECURE(SET_CAM_DEBUG_NAME,0x3e93523104b9387c, CommandSetCamDebugName);
//debug 		
		SCR_REGISTER_SECURE(GET_DEBUG_CAM,0xbc265d4ea63ebdd3,	CommandGetDebugCam);
		SCR_REGISTER_UNUSED(SET_DEBUG_CAM_ACTIVE,0xd1614197be2ad548,	CommandSetDebugCamActive);
//spline
		SCR_REGISTER_SECURE(ADD_CAM_SPLINE_NODE,0x8be9d374da4bb99b,	CommandAddCamSplineNode);
		SCR_REGISTER_SECURE(ADD_CAM_SPLINE_NODE_USING_CAMERA_FRAME,0x396c6b684ead1b8f,	CommandAddCamSplineNodeUsingCameraFrame);
		SCR_REGISTER_SECURE(ADD_CAM_SPLINE_NODE_USING_CAMERA,0x6c12d23212020231,	CommandAddCamSplineNodeUsingCamera); 
		SCR_REGISTER_SECURE(ADD_CAM_SPLINE_NODE_USING_GAMEPLAY_FRAME,0x5a43b6c214087091,	CommandAddCamSplineNodeUsingGameplayFrame); 
		SCR_REGISTER_SECURE(SET_CAM_SPLINE_PHASE,0x59489d17dc49b7b3,	CommandSetCamSplinePhase);
		SCR_REGISTER_SECURE(GET_CAM_SPLINE_PHASE,0x3f65eb34e4c562f2,	CommandGetCamSplinePhase);
		SCR_REGISTER_SECURE(GET_CAM_SPLINE_NODE_PHASE,0xa55408dfc92148e7,	CommandGetCamSplineNodePhase);
		SCR_REGISTER_SECURE(SET_CAM_SPLINE_DURATION,0x4bcdeab65827c4e2,	CommandSetCamSplineDuration);
		SCR_REGISTER_SECURE(SET_CAM_SPLINE_SMOOTHING_STYLE,0xadb0920102000613,	CommandSetCamSplineSmoothingStyle);
		SCR_REGISTER_SECURE(GET_CAM_SPLINE_NODE_INDEX,0x0631106d8d29716a,	CommandGetCamSplineNodeIndex);
		SCR_REGISTER_SECURE(SET_CAM_SPLINE_NODE_EASE,0x4617238b97beacf4,	CommandSetCamSplineNodeEase);
		SCR_REGISTER_SECURE(SET_CAM_SPLINE_NODE_VELOCITY_SCALE,0xa1d493603e8d15b4, CommandSetCamSplineNodeVelocityScale);
		SCR_REGISTER_SECURE(OVERRIDE_CAM_SPLINE_VELOCITY,0xa20e6cdd67dc817d, CommandOverrideCamSplineVelocity);
		SCR_REGISTER_SECURE(OVERRIDE_CAM_SPLINE_MOTION_BLUR,0x3437c3a2be385970, CommandOverrideCamSplineMotionBlur);
		SCR_REGISTER_SECURE(SET_CAM_SPLINE_NODE_EXTRA_FLAGS,0xcc1998034eecce71,	CommandSetCamSplineNodeExtraFlags);
		SCR_REGISTER_SECURE(IS_CAM_SPLINE_PAUSED,0xfb493c3d9e4e7ec9,	CommandIsCamSplinePaused);

//interp
		SCR_REGISTER_SECURE(SET_CAM_ACTIVE_WITH_INTERP,0x889b4f9d52e23dee,	CommandSetCamActiveWithInterp);
		SCR_REGISTER_SECURE(IS_CAM_INTERPOLATING,0x52522e25010580a1,	CommandIsCamInterpolating);
//Shake
		SCR_REGISTER_SECURE(SHAKE_CAM,0x9bae3263d9b1fcb9,	CommandShakeCam);
		SCR_REGISTER_SECURE(ANIMATED_SHAKE_CAM,0xbfd7467b93e06b26,	CommandAnimatedShakeCam);
		SCR_REGISTER_SECURE(IS_CAM_SHAKING,0x629b37af59212703,	CommandIsCamShaking);
		SCR_REGISTER_SECURE(SET_CAM_SHAKE_AMPLITUDE,0x1342e9e0cde9b323,	CommandSetCamShakeAmplitude);
		SCR_REGISTER_SECURE(STOP_CAM_SHAKING,0x80b66ef081475250,	CommandStopCamShaking);
		SCR_REGISTER_SECURE(SHAKE_SCRIPT_GLOBAL,0x7b6613dcaa2c2c29,	CommandShakeScriptGlobal);
		SCR_REGISTER_SECURE(ANIMATED_SHAKE_SCRIPT_GLOBAL,0x0d38c39fb4abd855,	CommandAnimatedShakeScriptGlobal);
		SCR_REGISTER_SECURE(IS_SCRIPT_GLOBAL_SHAKING,0xe9331aea6c1820c9,	CommandIsScriptGlobalShaking);
		SCR_REGISTER_UNUSED(SET_SCRIPT_GLOBAL_SHAKE_AMPLITUDE,0x7885a971a16a5e27,	CommandSetScriptGlobalShakeAmplitude);
		SCR_REGISTER_SECURE(STOP_SCRIPT_GLOBAL_SHAKING,0x8564f0807a5c1cd6,	CommandStopScriptGlobalShaking);
		SCR_REGISTER_SECURE(TRIGGER_VEHICLE_PART_BROKEN_CAMERA_SHAKE,0x6ffb2bece4cf4ed7, CommandTriggerVehiclePartBrokenCameraShake);
//Camera animation
		SCR_REGISTER_SECURE(PLAY_CAM_ANIM,0x8b28384f571e6da9,	CommandPlayCamAnim);
		SCR_REGISTER_SECURE(IS_CAM_PLAYING_ANIM,0x4d85bfc31f7d8d41,	CommandIsCamPlayingAnim);
 		SCR_REGISTER_SECURE(SET_CAM_ANIM_CURRENT_PHASE,0xa35860e818eac5b7,	CommandSetCamAnimCurrentPhase);
		SCR_REGISTER_SECURE(GET_CAM_ANIM_CURRENT_PHASE,0x7fdd65163647de8e,	CommandGetCamAnimCurrentPhase);
		SCR_REGISTER_SECURE(PLAY_SYNCHRONIZED_CAM_ANIM,0x7b8ce3a05613f41c, CommandPlaySynchronizedCamAnim);
//Scene Transition - deprecated!
		SCR_REGISTER_UNUSED(START_CAM_TRANSITION,0x5e6e0f6b80397c1d, CommandStartSceneTransition);
		SCR_REGISTER_UNUSED(IS_CAM_TRANSITIONING,0xe7ab8fa2bdb507c1, CommandIsCameraTransitioning);
		SCR_REGISTER_UNUSED(GET_CAM_TRANSITION_PHASE,0xebea23a22c057b10, CommandGetCamTransitionPhase); 
//Scripted fly camera
		SCR_REGISTER_SECURE(SET_FLY_CAM_HORIZONTAL_RESPONSE,0xaea1bfac30053852, CommandSetFlyCamHorizontalResponse);
		SCR_REGISTER_SECURE(SET_FLY_CAM_VERTICAL_RESPONSE,0xa2382ebb79e01e67, CommandSetFlyCamVerticalResponse);
		SCR_REGISTER_SECURE(SET_FLY_CAM_MAX_HEIGHT,0x3099b89127273567, CommandSetFlyCamMaxHeight);
		SCR_REGISTER_SECURE(SET_FLY_CAM_COORD_AND_CONSTRAIN,0x9eb45aa8353bd0b7, CommandSetFlyCamCoordAndConstrain);
		SCR_REGISTER_SECURE(SET_FLY_CAM_VERTICAL_CONTROLS_THIS_UPDATE,0x1aadc2d44870c7d9, CommandSetFlyCamVerticalControlsThisUpdate);
		SCR_REGISTER_SECURE(WAS_FLY_CAM_CONSTRAINED_ON_PREVIOUS_UDPATE,0x7f45f5de364061c3, CommandWasFlyCamConstrainedOnPreviousUpdate);
//Fade
		SCR_REGISTER_SECURE(IS_SCREEN_FADED_OUT,0xa829c9a2767ac8ef,	CommandIsScreenFadedOut);
		SCR_REGISTER_SECURE(IS_SCREEN_FADED_IN,0xe9e8955a780dda01,	CommandIsScreenFadedIn);
		SCR_REGISTER_SECURE(IS_SCREEN_FADING_OUT,0xcb1ef1e7b77adf4c,	CommandIsScreenFadingOut);
		SCR_REGISTER_SECURE(IS_SCREEN_FADING_IN,0xecd40fef3cf43bdb,	CommandIsScreenFadingIn);
		SCR_REGISTER_SECURE(DO_SCREEN_FADE_IN,0x5a7acd1cdf509b0d,	CommandDoScreenFadeIn);			
		SCR_REGISTER_SECURE(DO_SCREEN_FADE_OUT,0x859006db870314c5,	CommandDoScreenFadeOut);	
//Widescreen	
		SCR_REGISTER_SECURE(SET_WIDESCREEN_BORDERS,0x43f21fa00a1ce779,	CommandSetWidescreenBorders);
		SCR_REGISTER_SECURE(ARE_WIDESCREEN_BORDERS_ACTIVE,0xd0bf3d2e961ae546,	CommandAreWidescreenBordersActive);
//Gameplay
		SCR_REGISTER_SECURE(GET_GAMEPLAY_CAM_COORD,0xcb284f809b594322,	CommandGetGameplayCamCoord);					
		SCR_REGISTER_SECURE(GET_GAMEPLAY_CAM_ROT,0x9c0ed16b4f524508,	CommandGetGameplayCamRot);	
		SCR_REGISTER_SECURE(GET_GAMEPLAY_CAM_FOV,0x372e271fe8a97311,	CommandGetGameplayCamFov);	
		SCR_REGISTER_SECURE(SET_GAMEPLAY_CAM_MOTION_BLUR_SCALING_THIS_UPDATE,0xe776d70a603c3c81, CommandSetGameplayCamMotionBlurScalingThisUpdate);
		SCR_REGISTER_SECURE(SET_GAMEPLAY_CAM_MAX_MOTION_BLUR_STRENGTH_THIS_UPDATE,0x97e56574a6aa6c7d,	CommandSetGameplayCamMaxMotionBlurStrengthThisUpdate);
		SCR_REGISTER_SECURE(GET_GAMEPLAY_CAM_RELATIVE_HEADING,0x02ac46bf770385c4,	CommandGetGameplayCamRelativeHeading);	
		SCR_REGISTER_SECURE(SET_GAMEPLAY_CAM_RELATIVE_HEADING,0x94953c3ff0664f66,	CommandSetGameplayCamRelativeHeading);
		SCR_REGISTER_SECURE(GET_GAMEPLAY_CAM_RELATIVE_PITCH,0xd5b0bdc980eeb92f,	CommandGetGameplayCamRelativePitch);	
		SCR_REGISTER_SECURE(SET_GAMEPLAY_CAM_RELATIVE_PITCH,0x080c97b891e2f3aa,	CommandSetGameplayCamRelativePitch);
		SCR_REGISTER_SECURE(RESET_GAMEPLAY_CAM_FULL_ATTACH_PARENT_TRANSFORM_TIMER, 0x7295c203dd659dfe, CommandResetGameplayCamFullAttachParentTransformTimer);
        SCR_REGISTER_SECURE(FORCE_CAMERA_RELATIVE_HEADING_AND_PITCH,0x3f606b4abc46c107,	CommandForceCameraRelativeHeadingAndPitch);	
        SCR_REGISTER_SECURE(FORCE_BONNET_CAMERA_RELATIVE_HEADING_AND_PITCH,0x23fc9abda1de4638, CommandForceBonnetCameraRelativeHeadingAndPitch);
		SCR_REGISTER_SECURE(SET_FIRST_PERSON_SHOOTER_CAMERA_HEADING,0xe9aef8b7b1101b11, CommandSetFirstPersonShooterCameraHeading);
		SCR_REGISTER_SECURE(SET_FIRST_PERSON_SHOOTER_CAMERA_PITCH,0x4419cdfe572ac96b, CommandSetFirstPersonShooterCameraPitch);
		SCR_REGISTER_SECURE(SET_SCRIPTED_CAMERA_IS_FIRST_PERSON_THIS_FRAME,0x4b6a8fc4666f4a3b, CommandSetScriptedCameraIsFirstPersonThisFrame);
		SCR_REGISTER_SECURE(SHAKE_GAMEPLAY_CAM,0x87ca6430083e790d,	CommandShakeGameplayCam);
		SCR_REGISTER_SECURE(IS_GAMEPLAY_CAM_SHAKING,0xf8f3d410542c80db,	CommandIsGameplayCamShaking);
		SCR_REGISTER_SECURE(SET_GAMEPLAY_CAM_SHAKE_AMPLITUDE,0x0ed8565383e33da1,	CommandSetGameplayCamShakeAmplitude);
		SCR_REGISTER_SECURE(STOP_GAMEPLAY_CAM_SHAKING,0x84cf415aeab83b93,	CommandStopGameplayCamShaking);
		SCR_REGISTER_SECURE(SET_GAMEPLAY_CAM_FOLLOW_PED_THIS_UPDATE,0xb50594b7e6670f73,	CommandSetGameplayCamFollowPedThisUpdate);
		SCR_REGISTER_SECURE(IS_GAMEPLAY_CAM_RENDERING,0x66def7450747437a,	CommandIsGameplayCamRendering);
		SCR_REGISTER_SECURE(IS_INTERPOLATING_FROM_SCRIPT_CAMS,0x4709488c1d867377,	CommandIsInterpolatingFromScriptCams);
		SCR_REGISTER_SECURE(IS_INTERPOLATING_TO_SCRIPT_CAMS,0xe19e89b5fb4d4646,	CommandIsInterpolatingToScriptCams);
		SCR_REGISTER_SECURE(SET_GAMEPLAY_CAM_ALTITUDE_FOV_SCALING_STATE,0x3d1809e67cb9f79c, CommandSetGameplayCamAltitudeFovScalingState);
		SCR_REGISTER_SECURE(DISABLE_GAMEPLAY_CAM_ALTITUDE_FOV_SCALING_THIS_UPDATE,0x129dd42d09aac47e, CommandDisableGameplayCamAltitudeFovScalingThisUpdate);
		SCR_REGISTER_SECURE(IS_GAMEPLAY_CAM_LOOKING_BEHIND,0x594cc97203485127,	CommandIsGameplayCamLookingBehind);
		SCR_REGISTER_SECURE(SET_GAMEPLAY_CAM_IGNORE_ENTITY_COLLISION_THIS_UPDATE,0xa457f1126998c0ba,	CommandSetIgnoreCollisionForEntityThisUpdate);
		SCR_REGISTER_SECURE(DISABLE_CAM_COLLISION_FOR_OBJECT,0x31380d810d48740a,	CommandDisableCamCollisionForObject);
		SCR_REGISTER_SECURE(BYPASS_CAMERA_COLLISION_BUOYANCY_TEST_THIS_UPDATE,0x84b840750bfb7a66, CommandBypassCameraCollisionBuoyancyTestThisUpdate);
		SCR_REGISTER_SECURE(SET_GAMEPLAY_CAM_ENTITY_TO_LIMIT_FOCUS_OVER_BOUNDING_SPHERE_THIS_UPDATE,0x2d17c0f0a5e54afe, CommandSetGameplayCamEntityToLimitFocusOverBoundingSphereThisUpdate);
		SCR_REGISTER_SECURE(DISABLE_FIRST_PERSON_CAMERA_WATER_CLIPPING_TEST_THIS_UPDATE,0xd15ac77625fb5d5a,  CommandDisableFirstPersonCameraWaterClippingTestThisUpdate);

		SCR_REGISTER_SECURE(SET_FOLLOW_CAM_IGNORE_ATTACH_PARENT_MOVEMENT_THIS_UPDATE,0x328ba0168a94602f, CommandSetFollowCamIgnoreAttachParentMovementThisUpdate);

		SCR_REGISTER_SECURE(IS_SPHERE_VISIBLE,0x0318e2ee6fb4563f,	CommandIsSphereVisible);

		SCR_REGISTER_SECURE(IS_FOLLOW_PED_CAM_ACTIVE,0x72b75b6d474267d5,	CommandIsGameFollowPedCamActive);
		SCR_REGISTER_SECURE(SET_FOLLOW_PED_CAM_THIS_UPDATE,0x8753a341a596975f,	CommandSetFollowPedCamThisUpdate);

		SCR_REGISTER_SECURE(USE_SCRIPT_CAM_FOR_AMBIENT_POPULATION_ORIGIN_THIS_FRAME,0xa26c67a816927cc7, CommandUseScriptCamForAmbientPopulationOriginThisFrame);
		SCR_REGISTER_SECURE(SET_FOLLOW_PED_CAM_LADDER_ALIGN_THIS_UPDATE,0x5076c4358fbdaac7, CommandSetFollowPedCamLadderAlignThisUpdate);

		SCR_REGISTER_SECURE(SET_THIRD_PERSON_CAM_RELATIVE_HEADING_LIMITS_THIS_UPDATE,0xab5ee05015513abe, CommandSetThirdPersonRelativeHeadingThisUpdate);
		SCR_REGISTER_SECURE(SET_THIRD_PERSON_CAM_RELATIVE_PITCH_LIMITS_THIS_UPDATE,0x0e2d809fe9af80ae, CommandSetThirdPersonRelativePitchThisUpdate);
		SCR_REGISTER_SECURE(SET_THIRD_PERSON_CAM_ORBIT_DISTANCE_LIMITS_THIS_UPDATE,0x940fc4e949dd00d0, CommandSetThirdPersonOrbitDistanceThisUpdate);
		SCR_REGISTER_SECURE(SET_IN_VEHICLE_CAM_STATE_THIS_UPDATE,0xc605c03d36b56591, CommandSetInVehicleCameraStateThisUpdate);
		
		SCR_REGISTER_SECURE(DISABLE_ON_FOOT_FIRST_PERSON_VIEW_THIS_UPDATE,0xa84144d1edbb6d54,	CommandDisableOnFootFirstPersonViewThisUpdate);
		SCR_REGISTER_SECURE(DISABLE_FIRST_PERSON_FLASH_EFFECT_THIS_UPDATE,0x1109811fc4df0865,	CommandDisableFirstPersonFlashEffectThisUpdate);

		SCR_REGISTER_UNUSED(FORCE_THIRD_PERSON_AFTER_AIMING_BLEND_THIS_UPDATE,0x1a37fd458784ae4c,	CommandForceThirdPersonAferAimingBlendThisUpdate);
		SCR_REGISTER_UNUSED(DISABLE_THIRD_PERSON_AFTER_AIMING_BLEND_THIS_UPDATE,0x4c356901bdb72d7c,	CommandDisableThirdPersonAferAimingBlendThisUpdate);

        SCR_REGISTER_SECURE(BLOCK_FIRST_PERSON_ORIENTATION_RESET_THIS_UPDATE,0x39469160703c61ae, CommandBlockFirstPersonOrientationResetThisUpdate);

//Start deprecated
		//NOTE: The new view modes now match the legacy zoom levels, so it is not necessary to map the enum.
		SCR_REGISTER_SECURE(GET_FOLLOW_PED_CAM_ZOOM_LEVEL,0x64d620290bf268a3,	CommandGetFollowPedCamViewMode); 
//End deprecated
		SCR_REGISTER_SECURE(GET_FOLLOW_PED_CAM_VIEW_MODE,0x2b1d7439c26d5642,	CommandGetFollowPedCamViewMode); 
		SCR_REGISTER_SECURE(SET_FOLLOW_PED_CAM_VIEW_MODE,0xc9c7ccc4f117b115,	CommandSetFollowPedCamViewMode);

		SCR_REGISTER_SECURE(IS_FOLLOW_VEHICLE_CAM_ACTIVE,0xfe62696f731c151b,	CommandIsGameFollowVehicleCamActive);
		SCR_REGISTER_UNUSED(SET_FOLLOW_VEHICLE_CAM_THIS_UPDATE,0x79ee85ac07304324,	CommandSetFollowVehicleCamThisUpdate);
		SCR_REGISTER_SECURE(SET_FOLLOW_VEHICLE_CAM_HIGH_ANGLE_MODE_THIS_UPDATE,0xd299feeb1b4dedc6,	CommandSetFollowVehicleCamHighAngleModeThisUpdate);
		SCR_REGISTER_SECURE(SET_FOLLOW_VEHICLE_CAM_HIGH_ANGLE_MODE_EVERY_UPDATE,0x43b6850b58d1022c,	CommandSetFollowVehicleCamHighAngleModeEveryUpdate);

		//////////////////////////////////////////////////////////////////////////
		// Table Games Camera API
		SCR_REGISTER_SECURE(SET_TABLE_GAMES_CAMERA_THIS_UPDATE,0x01727103dd974710, CommandSetTableGamesCameraThisUpdate);
		SCR_REGISTER_UNUSED(IS_TABLE_GAMES_CAMERA_ACTIVE,0x23a755c9532e976a, CommandIsTableGamesCameraActive);
		//////////////////////////////////////////////////////////////////////////

//Start deprecated
		//NOTE: The new view modes now match the legacy zoom levels, so it is not necessary to map the enum.
		SCR_REGISTER_SECURE(GET_FOLLOW_VEHICLE_CAM_ZOOM_LEVEL,0x964cd83935ad0ed1,	CommandGetFollowVehicleCamViewMode); 
		SCR_REGISTER_SECURE(SET_FOLLOW_VEHICLE_CAM_ZOOM_LEVEL,0x842b2a274564caf5,	CommandSetGameFollowVehicleCamZoomLevel);
//End deprecated
		SCR_REGISTER_SECURE(GET_FOLLOW_VEHICLE_CAM_VIEW_MODE,0x5e29b0cf10cecb34,	CommandGetFollowVehicleCamViewMode); 
		SCR_REGISTER_SECURE(SET_FOLLOW_VEHICLE_CAM_VIEW_MODE,0xed74a3aa53be3520,	CommandSetFollowVehicleCamViewMode);

		SCR_REGISTER_SECURE(GET_CAM_VIEW_MODE_FOR_CONTEXT,0x4de6a646afb7cf5c,	CommandGetCamViewModeForContext); 
		SCR_REGISTER_SECURE(SET_CAM_VIEW_MODE_FOR_CONTEXT,0x4b1c22133022d7d3,	CommandSetCamViewModeForContext); 
		SCR_REGISTER_SECURE(GET_CAM_ACTIVE_VIEW_MODE_CONTEXT,0x47e529da87615548,	CommandGetCamActiveViewModeContext); 

		SCR_REGISTER_SECURE(USE_VEHICLE_CAM_STUNT_SETTINGS_THIS_UPDATE,0x016cdcb99a127243, CommandUseVehicleCamStuntSettingsThisUpdate);
		SCR_REGISTER_SECURE(USE_DEDICATED_STUNT_CAMERA_THIS_UPDATE,0xa9fc4ab61148c611, CommandUseDedicatedStuntCameraThisUpdate);
		SCR_REGISTER_SECURE(FORCE_VEHICLE_CAM_STUNT_SETTINGS_THIS_UPDATE,0x0394892bda1f1eda, CommandForceVehicleCamStuntSettingsThisUpdate);

        SCR_REGISTER_SECURE(SET_FOLLOW_VEHICLE_CAM_SEAT_THIS_UPDATE,0x65498c8065adcc5d, CommandSetFollowVehicleCamSeatThisUpdate);

		SCR_REGISTER_SECURE(IS_AIM_CAM_ACTIVE,0x2358ae4c940cf1df,	CommandIsGameAimCamActive);
		SCR_REGISTER_SECURE(IS_AIM_CAM_ACTIVE_IN_ACCURATE_MODE,0x19abd41af27cd2a7,	CommandIsGameAimCamActiveInAccurateMode);
		SCR_REGISTER_SECURE(IS_FIRST_PERSON_AIM_CAM_ACTIVE,0x28ff2c2261b6cba1,	CommandIsGameFirstPersonAimCamActive);
		SCR_REGISTER_SECURE(DISABLE_AIM_CAM_THIS_UPDATE,0x7c6105654788a17b,	CommandDisableGameAimCamThisUpdate);
		SCR_REGISTER_UNUSED(GET_PED_AIM_CAM_VIEW_MODE,0x1df54e09bd6ce2f0,	CommandGetPedAimCamViewMode); 
		SCR_REGISTER_UNUSED(SET_PED_AIM_CAM_VIEW_MODE,0x3a08e7e6adf155d2,	CommandSetPedAimCamViewMode);
		SCR_REGISTER_SECURE(GET_FIRST_PERSON_AIM_CAM_ZOOM_FACTOR,0x8e277ef62b1e0bdf,	CommandGetGameFirstPersonAimCamZoomFactor);
		SCR_REGISTER_SECURE(SET_FIRST_PERSON_AIM_CAM_ZOOM_FACTOR,0xec426a0c147ec075,	CommandSetGameFirstPersonAimCamZoomFactor);
		SCR_REGISTER_SECURE(SET_FIRST_PERSON_AIM_CAM_ZOOM_FACTOR_LIMITS_THIS_UPDATE,0x369ee77cbbd732b2,	CommandSetGameFirstPersonAimCamZoomFactorLimitsThisUpdate);
		SCR_REGISTER_SECURE(SET_FIRST_PERSON_AIM_CAM_RELATIVE_HEADING_LIMITS_THIS_UPDATE,0xd9a8ad65089229ce,	CommandSetGameFirstPersonAimCamRelativeHeadingLimitsThisUpdate);
		SCR_REGISTER_SECURE(SET_FIRST_PERSON_AIM_CAM_RELATIVE_PITCH_LIMITS_THIS_UPDATE,0x57ed17672f68f6fa,	CommandSetGameFirstPersonAimCamRelativePitchLimitsThisUpdate);
		SCR_REGISTER_SECURE(SET_FIRST_PERSON_AIM_CAM_NEAR_CLIP_THIS_UPDATE,0x5ded85df2208b943,	CommandSetGameFirstPersonAimCamNearClipThisUpdate);
		SCR_REGISTER_SECURE(SET_THIRD_PERSON_AIM_CAM_NEAR_CLIP_THIS_UPDATE,0xd195f5a86bfb85d4,	CommandSetGameThirdPersonAimCamNearClipThisUpdate);
		
		SCR_REGISTER_SECURE(SET_ALLOW_CUSTOM_VEHICLE_DRIVE_BY_CAM_THIS_UPDATE,0x0e83878033f3e16a,	CommandSetAllowCustomVehicleDriveByCamThisUpdate);

		SCR_REGISTER_SECURE(FORCE_TIGHTSPACE_CUSTOM_FRAMING_THIS_UPDATE,0xf41864db1cf85307, CommandForceTightSpaceCustomFramingThisUpdate);

//Cached frame 
		SCR_REGISTER_SECURE(GET_FINAL_RENDERED_CAM_COORD,0x524171c8263e7c3e,	CommandGetRenderedCamCoord);
		SCR_REGISTER_SECURE(GET_FINAL_RENDERED_CAM_ROT,0x52bd0fa2342c7246,	CommandGetRenderedCamRot);
        SCR_REGISTER_SECURE(GET_FINAL_RENDERED_REMOTE_PLAYER_CAM_ROT,0x58e4c4eb027d47b6,	CommandGetRenderedRemotePlayerCamRot);
		SCR_REGISTER_SECURE(GET_FINAL_RENDERED_CAM_FOV,0x07c8c8b0cbd44d3b,	CommandGetRenderedCamFov);
        SCR_REGISTER_SECURE(GET_FINAL_RENDERED_REMOTE_PLAYER_CAM_FOV,0x00f2683a6a133040,	CommandGetRenderedRemotePlayerCamFov);
		SCR_REGISTER_SECURE(GET_FINAL_RENDERED_CAM_NEAR_CLIP,0xe51db7b55b7f9f44,	CommandGetRenderedCamNearClip);
		SCR_REGISTER_SECURE(GET_FINAL_RENDERED_CAM_FAR_CLIP,0xdf083f70a83a7271,	CommandGetRenderedCamFarClip);
		SCR_REGISTER_SECURE(GET_FINAL_RENDERED_CAM_NEAR_DOF,0xa97deab4b3e49b73,	CommandGetRenderedCamNearDof);
		SCR_REGISTER_SECURE(GET_FINAL_RENDERED_CAM_FAR_DOF,0x658584b384fb3c2a,	CommandGetRenderedCamFarDof);
		SCR_REGISTER_SECURE(GET_FINAL_RENDERED_CAM_MOTION_BLUR_STRENGTH,0x26dab410cd0cec53,	CommandGetRenderedCamMotionBlurStrength);
//Hint cam		
		SCR_REGISTER_SECURE(SET_GAMEPLAY_COORD_HINT,0xdbfee73a55300843,	CommandSetGameplayCoordHint);
		SCR_REGISTER_SECURE(SET_GAMEPLAY_PED_HINT,0xda6126f0297ba5e8,	CommandSetGameplayPedHint); //TO BE REMOVED
		SCR_REGISTER_SECURE(SET_GAMEPLAY_VEHICLE_HINT,0x7c6d4fd402f1d4f9,	CommandSetGameplayVehicleHint);
		SCR_REGISTER_SECURE(SET_GAMEPLAY_OBJECT_HINT,0xb065566f582943d1,	CommandSetGameplayObjectHint);
		SCR_REGISTER_SECURE(SET_GAMEPLAY_ENTITY_HINT,0x56593357686570f4, CommandSetGameplayEntityHint);
		SCR_REGISTER_UNUSED(SET_GAMEPLAY_HINT_RELATIVE_PITCH,0x74c6600da8d135e3,	CommandSetGameplayHintRelativePitch);
		SCR_REGISTER_SECURE(IS_GAMEPLAY_HINT_ACTIVE,0x4131052989de1278,	CommandIsGameplayHintActive);
		SCR_REGISTER_SECURE(STOP_GAMEPLAY_HINT,0xbd1e8007d7dfa747,	CommandStopGameplayHint); 
		SCR_REGISTER_SECURE(STOP_GAMEPLAY_HINT_BEING_CANCELLED_THIS_UPDATE,0x457826ed772c86a4,CommandStopGameplayHintBeingCancelledUsingRightStick);	
		SCR_REGISTER_SECURE(STOP_CODE_GAMEPLAY_HINT,0x408cf406c7d78d6f,	CommandStopCodeGamePlayHint); 
		SCR_REGISTER_SECURE(IS_CODE_GAMEPLAY_HINT_ACTIVE,0x3f520e7ecf3381a3,	CommandIsCodeGameplayHintActive); 
		SCR_REGISTER_SECURE(SET_GAMEPLAY_HINT_FOV,0xc9b7a3b3228edc18,	CommandSetGameplayHintFov);
		SCR_REGISTER_SECURE(SET_GAMEPLAY_HINT_FOLLOW_DISTANCE_SCALAR,0xffd7feec98321a66,	CommandSetGameplayHintFollowDistanceScalar); 
		SCR_REGISTER_SECURE(SET_GAMEPLAY_HINT_BASE_ORBIT_PITCH_OFFSET,0xc3940c122330f4aa,	CommandSetGameplayHintBaseOrbitPitchOffset); 
		SCR_REGISTER_SECURE(SET_GAMEPLAY_HINT_CAMERA_RELATIVE_SIDE_OFFSET,0xc5fb663c1ce53c57,	CommandSetGameplayHintCameraRelativeSideOffsetAdditive); 
		SCR_REGISTER_SECURE(SET_GAMEPLAY_HINT_CAMERA_RELATIVE_VERTICAL_OFFSET,0xf2938a28c15db464,	CommandSetGameplayHintCameraRelativeVerticalOffsetAdditive); 
		SCR_REGISTER_SECURE(SET_GAMEPLAY_HINT_CAMERA_BLEND_TO_FOLLOW_PED_MEDIUM_VIEW_MODE,0xcb772e2a88f6a27f,	CommandSetGameplayHintCameraBlendToFollowPedMediumViewMode); 

//Cinematic
		SCR_REGISTER_SECURE(SET_CINEMATIC_BUTTON_ACTIVE,0xe4fa4fbf0bafd2a8,	CommandSetCinematicButtonActive);
		SCR_REGISTER_SECURE(IS_CINEMATIC_CAM_RENDERING,0x0085e4be8fd73703,	CommandIsCinematicCamRendering);
		SCR_REGISTER_SECURE(SHAKE_CINEMATIC_CAM,0x3b58be9e133fd48d,	CommandShakeCinematicCam);
		SCR_REGISTER_SECURE(IS_CINEMATIC_CAM_SHAKING,0xbee2f63c2da77f95,	CommandIsCinematicCamShaking);
		SCR_REGISTER_SECURE(SET_CINEMATIC_CAM_SHAKE_AMPLITUDE,0x86a2751c5f3af8f8,	CommandSetCinematicCamShakeAmplitude);
		SCR_REGISTER_SECURE(STOP_CINEMATIC_CAM_SHAKING,0xc3d04b421e077910,	CommandStopCinematicCamShaking);
		SCR_REGISTER_SECURE(DISABLE_CINEMATIC_BONNET_CAMERA_THIS_UPDATE,0x5ad47d8cfc3a86af,	CommandDisableCinematicBonnetCameraThisUpdate);
		SCR_REGISTER_SECURE(DISABLE_CINEMATIC_VEHICLE_IDLE_MODE_THIS_UPDATE,0x7936e7a8dbe3dbbe,	CommandDisableCinematicVehicleIdleModeThisUpdate);
		SCR_REGISTER_SECURE(INVALIDATE_CINEMATIC_VEHICLE_IDLE_MODE,0xb06c15f6118b032d,	CommandInvalidateCinematicVehicleIdleMode);
		SCR_REGISTER_SECURE(INVALIDATE_IDLE_CAM,0x62ea3913642edf8e,	CommandInvalidateIdleCam);
		SCR_REGISTER_SECURE(IS_CINEMATIC_IDLE_CAM_RENDERING,0x20192bf9e3bb2549,	CommandIsCinematicIdleCamRendering);
		SCR_REGISTER_SECURE(IS_CINEMATIC_FIRST_PERSON_VEHICLE_INTERIOR_CAM_RENDERING,0x94398b6170dca18c,	CommandIsCinematicFirstPersonVehicleInteriorCamRendering);
		SCR_REGISTER_SECURE(CREATE_CINEMATIC_SHOT,0x9b30483daa48199b,	CommandCreateCinematicShot);
		SCR_REGISTER_SECURE(IS_CINEMATIC_SHOT_ACTIVE,0x99636b3721921a29,	CommandIsCinematicShotActive);
		SCR_REGISTER_SECURE(STOP_CINEMATIC_SHOT,0x332e0449b21e13ab,	CommandStopCinematicShot);
		SCR_REGISTER_UNUSED(IS_CINEMATIC_SHOT_RENDERING,0x809c5dbfa8d8d422,	CommandIsCinematicShotRendering);
		SCR_REGISTER_SECURE(FORCE_CINEMATIC_RENDERING_THIS_UPDATE,0x683caf9e8ab4583d,	CommandScriptForceCinematicRenderingThisUpdate);
		SCR_REGISTER_SECURE(SET_CINEMATIC_NEWS_CHANNEL_ACTIVE_THIS_UPDATE,0x58787e2db6f54bac, CommandSetCinematicNewsChannelActiveThisUpdate); 
		SCR_REGISTER_UNUSED(REGISTER_CINEMATIC_CAMERA_CHECK_POINT_POS,0x26bba2903674460c, CommandRegisterRaceCheckPoint); 
		SCR_REGISTER_SECURE(SET_CINEMATIC_MODE_ACTIVE,0x71800f6ecfd14497, CommandSetCinematicModeActive); 
		SCR_REGISTER_UNUSED(SET_CINEMATIC_MISSION_CREATOR_FAIL_ACTIVE_THIS_UPDATE,0x9a341be2cbe64682, CommandCinematicMissionCreatorFailContextActiveThisUpdate); 
		SCR_REGISTER_SECURE(IS_IN_VEHICLE_MOBILE_PHONE_CAMERA_RENDERING,0xfb59e5247fb6b7f9,	CommandIsInVehicleMobilePhoneCameraRendering);
		SCR_REGISTER_SECURE(DISABLE_CINEMATIC_SLOW_MO_THIS_UPDATE,0x5951e2e1db653567,	CommandDisableCinematicSlowModeThisUpdate);
		SCR_REGISTER_SECURE(IS_BONNET_CINEMATIC_CAM_RENDERING,0x9bf14c0e44bdb17d,	CommandIsBonnetCinematicCamRendering);
		SCR_REGISTER_SECURE(IS_CINEMATIC_CAM_INPUT_ACTIVE,0x3698ce894e1342d5,	CommandIsCinematicCamInputActive);		
        SCR_REGISTER_SECURE(IGNORE_MENU_PREFERENCE_FOR_BONNET_CAMERA_THIS_UPDATE,0x438bb2738c8d1cc9,	CommandIgnoreMenuPreferenceForBonnetCameraThisUpdate);
//Cutscene
		SCR_REGISTER_SECURE(BYPASS_CUTSCENE_CAM_RENDERING_THIS_UPDATE,0x5a971a2d157136b6,	CommandBypassCutsceneCamRenderingThisUpdate);
		SCR_REGISTER_UNUSED(SHAKE_CUTSCENE_CAM,0x731970af18babee4,	CommandShakeCutsceneCam);
		SCR_REGISTER_UNUSED(IS_CUTSCENE_CAM_SHAKING,0x747c1186c8b8c7be,	CommandIsCutsceneCamShaking);
		SCR_REGISTER_UNUSED(SET_CUTSCENE_CAM_SHAKE_AMPLITUDE,0x9706a280275bce96,	CommandSetCutsceneCamShakeAmplitude);
		SCR_REGISTER_SECURE(STOP_CUTSCENE_CAM_SHAKING,0x1435be4d0e33c803,	CommandStopCutsceneCamShaking);
		SCR_REGISTER_SECURE(SET_CUTSCENE_CAM_FAR_CLIP_THIS_UPDATE,0xf0c79c8b198cd112,	CommandSetCutsceneCamFarClipThisUpdate);

//Misc
		SCR_REGISTER_SECURE(GET_FOCUS_PED_ON_SCREEN,0x337b1e5a3d6dedfa, CommandGetFocusPedOnScreen);
		SCR_REGISTER_SECURE(DISABLE_NEAR_CLIP_SCAN_THIS_UPDATE,0x822c724fc6fae724, CommandDisableNearClipScanThisUpdate);

		SCR_REGISTER_SECURE(SET_CAM_DEATH_FAIL_EFFECT_STATE,0xecc353e755ab76f1,	CommandSetCamDeathFailEffectState);

		SCR_REGISTER_UNUSED(START_CAMERA_CAPTURE,0x015c04f6d80f2ada, CommandStartCameraCapture);
		SCR_REGISTER_UNUSED(STOP_CAMERA_CAPTURE,0xf427f64662a73be0, CommandStopCameraCapture);
		SCR_REGISTER_SECURE(SET_FIRST_PERSON_FLASH_EFFECT_TYPE,0x7659886d4d8ac36a, CommandSetFirstPersonFlashEffectType);
		SCR_REGISTER_SECURE(SET_FIRST_PERSON_FLASH_EFFECT_VEHICLE_MODEL_NAME,0x7533e83009d5f82f, CommandSetFirstPersonFlashEffectVehicleModelName);
		SCR_REGISTER_SECURE(SET_FIRST_PERSON_FLASH_EFFECT_VEHICLE_MODEL_HASH,0x058fdcacb20a766a, CommandSetFirstPersonFlashEffectVehicleModelHash);
		SCR_REGISTER_SECURE(IS_ALLOWED_INDEPENDENT_CAMERA_MODES,0x9cf8be19558b3a3d, CommandIsAllowedIndependentCameraModes);
		SCR_REGISTER_SECURE(CAMERA_PREVENT_COLLISION_SETTINGS_FOR_TRIPLEHEAD_IN_INTERIORS_THIS_UPDATE,0xb88aab5ba3dc7936, CommandPreventCollisionSettingsForTripleHeadInInteriorsThisUpdate);

//Replay
		SCR_REGISTER_SECURE(REPLAY_GET_MAX_DISTANCE_ALLOWED_FROM_PLAYER,0x0b9cd460e6a37693, CommandReplayGetMaxDistanceAllowedFromPlayer);

	} // end of registered commands
}	//	end of namespace camera_commands
