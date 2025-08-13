//
// camera/system/debug/CameraDebugInfo.cpp
// 
// Copyright (C) 1999-2022 Rockstar Games.  All Rights Reserved.
//
#include "camera/system/debug/CameraDebugInfo.h"

#if AI_DEBUG_OUTPUT_ENABLED

#include "camera/base/BaseCamera.h"
#include "camera/base/BaseDirector.h"
#include "camera/camInterface.h"
#include "camera/cinematic/camera/mounted/CinematicMountedCamera.h"
#include "camera/gameplay/aim/AimCamera.h"
#include "camera/gameplay/aim/FirstPersonShooterCamera.h"
#include "camera/gameplay/ThirdPersonCamera.h"
#include "camera/helpers/ControlHelper.h"

CAMERA_OPTIMISATIONS();

EXTERN_PARSER_ENUM(camControlHelperMetadataViewMode__eViewModeContext);
EXTERN_PARSER_ENUM(camControlHelperMetadataViewMode__eViewMode);

CCameraDebugInfo::CCameraDebugInfo(DebugInfoPrintParams printParams /*= DebugInfoPrintParams()*/)
	: CAIDebugInfo(printParams)
{
}

CCameraDebugInfo::~CCameraDebugInfo()
{
}

void CCameraDebugInfo::Print()
{
	WriteLogEvent("CAMERA SYSTEM");
	PushIndent(); PushIndent(); PushIndent();
	{
		const camBaseCamera* dominantCamera = camInterface::GetDominantRenderedCamera();
		if(dominantCamera)
		{
			WriteHeader("DOMINANT");
			PushIndent();
				PrintCameraInfo(*dominantCamera, "", Color_red);
			PopIndent();

			WriteHeader("VIEW MODES");
			PushIndent();
				PrintViewModes(*dominantCamera);
			PopIndent();
		}

#if __BANK
		atArray<camBaseCamera*> cameraList;
		camInterface::GetScriptDirector().DebugGetCameras(cameraList);
		if(cameraList.GetCount() > 0 )
		{
			WriteHeader("SCRIPT CAMERAS");
			PushIndent();
			{
				for(int index = 0; index < cameraList.GetCount(); ++index)
				{
					const camBaseCamera* camera = cameraList[index]; 
					if(camera)
					{
						char label[32];
						formatf(label, "Script%d: ", index);
						PrintCameraInfo(*camera, label, Color_blue);
					}
				}
			}
			PopIndent();
		}
#endif //__BANK

		PrintCameraFrameInfo(camInterface::GetFrame(), "Cached Frame", Color_violet);
	}
	PopIndent(); PopIndent(); PopIndent();
}

void CCameraDebugInfo::PrintCameraInfo(const camBaseCamera& camera, const char* label, Color32 color)
{
	if(strlen(label) > 0)
	{
		ColorPrintLn(color, "%s", label);
	}
	PrintCameraFrameInfo(camera.GetFrame(), camera.GetName(), color);
	if(camera.GetAttachParent())
	{
		ColorPrintLn(color, "%s", camera.GetAttachParent()->GetLogName());
	}
}

void CCameraDebugInfo::PrintCameraFrameInfo(const camFrame& frame, const char* label, Color32 color)
{
	const Vector3 pos = frame.GetPosition();
	float heading,pitch,roll;
	frame.ComputeHeadingPitchAndRoll(heading, pitch, roll);
	ColorPrintLn(color, "%s Pos(X,Y,Z) = (%.2f,%.2f,%.2f) Rot(H,P,R): (%.2f,%.2f,%.2f)", label, pos.x, pos.y, pos.z, heading*RtoD, pitch*RtoD, roll*RtoD);
}

void CCameraDebugInfo::PrintViewModes(const camBaseCamera& camera)
{
	s32 dominantCameraContext = camControlHelperMetadataViewMode::ON_FOOT;
	s32 dominantCameraViewMode = camControlHelperMetadataViewMode::THIRD_PERSON_MEDIUM;
	if(camera.GetIsClassId(camThirdPersonCamera::GetStaticClassId()))
	{
		const camThirdPersonCamera& thirdPersonCamera = static_cast<const camThirdPersonCamera&>(camera);
		if(thirdPersonCamera.GetControlHelper())
		{
			dominantCameraContext = thirdPersonCamera.GetControlHelper()->GetViewModeContext();
			dominantCameraViewMode = thirdPersonCamera.GetControlHelper()->GetViewMode();
		}
	}
	else if(camera.GetIsClassId(camThirdPersonAimCamera::GetStaticClassId()))
	{
		const camThirdPersonAimCamera& thirdPersonAimCamera = static_cast<const camThirdPersonAimCamera&>(camera);
		if(thirdPersonAimCamera.GetControlHelper())
		{
			dominantCameraContext = thirdPersonAimCamera.GetControlHelper()->GetViewModeContext();
			dominantCameraViewMode = thirdPersonAimCamera.GetControlHelper()->GetViewMode();
		}
	}
	else if(camera.GetIsClassId(camFirstPersonShooterCamera::GetStaticClassId()))
	{
		const camFirstPersonShooterCamera& firstPersonCamera = static_cast<const camFirstPersonShooterCamera&>(camera);
		if(firstPersonCamera.GetControlHelper())
		{
			dominantCameraContext = firstPersonCamera.GetControlHelper()->GetViewModeContext();
			dominantCameraViewMode = firstPersonCamera.GetControlHelper()->GetViewMode();
		}
	}
	else if(camera.GetIsClassId(camCinematicMountedCamera::GetStaticClassId()))
	{
		const camCinematicMountedCamera& mountedCamera = static_cast<const camCinematicMountedCamera&>(camera);
		if(mountedCamera.GetControlHelper())
		{
			dominantCameraContext = mountedCamera.GetControlHelper()->GetViewModeContext();
			dominantCameraViewMode = mountedCamera.GetControlHelper()->GetViewMode();
		}
	}

	ColorPrintLn(Color_green, "Dominant camera context is %s and dominant camera view mode is %s", PARSER_ENUM(camControlHelperMetadataViewMode__eViewModeContext).m_Names[dominantCameraContext], PARSER_ENUM(camControlHelperMetadataViewMode__eViewMode).m_Names[camControlHelper::GetViewModeForContext(dominantCameraViewMode)]);

	s32 attachParentDerivedContext = camControlHelperMetadataViewMode::ON_FOOT;
	if(camera.GetAttachParent() && camera.GetAttachParent()->GetIsTypeVehicle())
	{
		const CVehicle* attachParentVehicle = static_cast<const CVehicle*>(camera.GetAttachParent());
		attachParentDerivedContext = camControlHelper::ComputeViewModeContextForVehicle(*attachParentVehicle);
	}

	ColorPrintLn(Color_green, "AttachParentContext %s", PARSER_ENUM(camControlHelperMetadataViewMode__eViewModeContext).m_Names[attachParentDerivedContext]);

	for(s32 context = 0; context < camControlHelperMetadataViewMode::eViewModeContext_NUM_ENUMS; ++context)
	{
		const s32 viewModeForContext = camControlHelper::GetViewModeForContext(context);
		ColorPrintLn(Color_green, "Context %s is set on view mode %s", PARSER_ENUM(camControlHelperMetadataViewMode__eViewModeContext).m_Names[context], PARSER_ENUM(camControlHelperMetadataViewMode__eViewMode).m_Names[viewModeForContext]);
	}
}

#endif //AI_DEBUG_OUTPUT_ENABLED