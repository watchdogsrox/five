//
// camera/debug/DebugDirector.cpp
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#include "camera/debug/DebugDirector.h"

#if !__FINAL //No debug cameras in final builds.

#include "bank/bkmgr.h"
#include "grcore/debugdraw.h"

#include "camera/debug/FreeCamera.h"
#include "camera/system/CameraFactory.h"
#include "camera/system/CameraMetadata.h"
#include "debug/MarketingTools.h"
#include "system/controlMgr.h"
#include "text/messages.h"

CAMERA_OPTIMISATIONS()

INSTANTIATE_RTTI_CLASS(camDebugDirector,0x9B676D1B)

PARAM(mouseDebugCamera, "Enable mouse control of the debug cameras");

#if __BANK
static const char* g_DebugCameraNames[] =
{
	"Free",		// DEBUG_CAMERA_MODE_FREE
	"None"		// DEBUG_CAMERA_MODE_NONE
};
#endif // __BANK


camDebugDirector::camDebugDirector(const camBaseObjectMetadata& metadata)
: camBaseDirector(metadata)
, m_Metadata(static_cast<const camDebugDirectorMetadata&>(metadata))
, m_ActiveMode(DEBUG_CAMERA_MODE_NONE)
, m_ShouldIgnoreDebugPadCameraToggle(false)
, m_ShouldUseMouseControl(PARAM_mouseDebugCamera.Get())
{
	m_FreeCamera = camFactory::CreateObject<camFreeCamera>(m_Metadata.m_FreeCameraRef.GetHash());
	cameraAssertf(m_FreeCamera, "The debug director failed to create a free camera (name: %s, hash: %u)",
		SAFE_CSTRING(m_Metadata.m_FreeCameraRef.GetCStr()), m_Metadata.m_FreeCameraRef.GetHash());
}

camDebugDirector::~camDebugDirector()
{
	if(m_FreeCamera)
	{
		delete m_FreeCamera;
	}
}

bool camDebugDirector::Update()
{
	//Default to not rendering unless we have a camera to render.
	m_ActiveRenderState = RS_NOT_RENDERING;

	//NOTE: We don't update the debug cameras while the marketing tools are active, as this avoids control conflicts.
	if(CMarketingTools::IsActive())
	{
		return false;
	}

	m_Frame.CloneFrom(m_CachedInitialDestinationFrame);

	UpdateControl(); //Handle user input.

	m_UpdatedCamera = ComputeActiveCamera();

	bool hasSucceeded = false;
	if(m_UpdatedCamera)
	{
		hasSucceeded = m_UpdatedCamera->BaseUpdate(m_Frame);
		if(hasSucceeded)
		{
			m_ActiveRenderState = RS_FULLY_RENDERING;

			m_Frame.ComputeDofFromFovAndClipRange();
		}
	}

	return hasSucceeded;
}

//Handles debug pad and mouse input for toggling activation of the cameras.
void camDebugDirector::UpdateControl()
{
	//Allow activation and deactivation of the free camera and debug pad on Start+Circle/B. 
	CPad* playerPad = CControlMgr::GetPlayerPad();
	bool isToggleFreeCameraAndDebugPadRequested = playerPad && (playerPad->GetPressedDebugButtons() & ioPad::RRIGHT);

	CPad& debugPad = CControlMgr::GetDebugPad();
	const bool isDebugButtonDown = (debugPad.GetDebugButtons() & ioPad::GetCurrentDebugButton()) || (playerPad && (playerPad->GetDebugButtons() & ioPad::GetCurrentDebugButton()));

	const bool isToggleFreeCameraRequested	= debugPad.ButtonCircleJustDown() && !isDebugButtonDown;

	const bool shouldActivateMouseCam	= m_ShouldUseMouseControl && ((ioMouse::GetButtons() & ioMouse::MOUSE_MIDDLE) &&
											ioMouse::GetPressedButtons() == ioMouse::MOUSE_LEFT);

	const bool shouldToggleFreeCamera	= shouldActivateMouseCam || (!m_ShouldIgnoreDebugPadCameraToggle &&
											(isToggleFreeCameraAndDebugPadRequested || isToggleFreeCameraRequested));

	if(m_FreeCamera && shouldToggleFreeCamera)
	{
		camFreeCamera* freeCamera = (camFreeCamera*)m_FreeCamera.Get();

		freeCamera->Reset();
		freeCamera->SetMouseEnableState(shouldActivateMouseCam);

		if(m_ActiveMode == DEBUG_CAMERA_MODE_FREE)
		{
			m_ActiveMode = DEBUG_CAMERA_MODE_NONE;
		}
		else
		{
			m_ActiveMode = DEBUG_CAMERA_MODE_FREE;

			//Note that the free camera clones the frame that was composed prior to this director being updated.
			freeCamera->SetFrame(m_CachedInitialDestinationFrame);
		}

		const bool isFreeCameraActive = (m_ActiveMode == DEBUG_CAMERA_MODE_FREE);
#if __BANK
		const char* stringLabel = isFreeCameraActive ? "HELP_DBGCAMON" : "HELP_DBGCAMOFF";
#endif // __BANK

		if(isToggleFreeCameraAndDebugPadRequested)
		{
			const bool isDebugPadOn = CControlMgr::IsDebugPadOn();

			if(isDebugPadOn != isFreeCameraActive)
			{
				CControlMgr::SetDebugPadOn(isFreeCameraActive);
#if __BANK
				stringLabel	= isFreeCameraActive ? "HELP_DBGCMPDON" : "HELP_DBGCMPDOFF";
#endif // __BANK
			}
		}

#if __BANK
		char *string = TheText.Get(stringLabel);
		CHelpMessage::SetMessageText(HELP_TEXT_SLOT_STANDARD, string);
#endif // __BANK
	}
}

camBaseCamera* camDebugDirector::ComputeActiveCamera()
{
	camBaseCamera* activeCamera;

	switch(m_ActiveMode)
	{
	case DEBUG_CAMERA_MODE_FREE:
		activeCamera = m_FreeCamera;
		break;

	//case DEBUG_CAMERA_MODE_NONE:
	default:
		activeCamera = NULL;
		break;
	}

	return activeCamera;
}

const camFrame& camDebugDirector::GetCamFrame(camBaseCamera* camera) const
{
	const camFrame& frame = camera ? camera->GetFrame() : camBaseCamera::GetScratchFrame(); //Use the scratch frame here so we can stick with references.
	return frame;
}

camFrame& camDebugDirector::GetCamFrameNonConst(camBaseCamera* camera)
{
	camFrame& frame = camera ? camera->GetFrameNonConst() : camBaseCamera::GetScratchFrame(); //Use the scratch frame here so we can stick with references.
	return frame;
}

#if __BANK
void camDebugDirector::AddWidgetsForInstance()
{
	if(m_WidgetGroup == NULL)
	{
		bkBank* bank = BANKMGR.FindBank("Camera");
		if(bank)
		{
			m_WidgetGroup = bank->PushGroup("Debug director", false);
			{
				bank->AddCombo("Active mode", (int*)&m_ActiveMode, NUM_DEBUG_CAMERA_MODES, g_DebugCameraNames, NullCB, "Select a debug camera mode");
				bank->AddToggle("Use mouse control", &m_ShouldUseMouseControl);
			}
			bank->PopGroup();
		}
	}
}

void camDebugDirector::DebugGetCameras(atArray<camBaseCamera*> &cameraList) const
{
	if(m_FreeCamera)
	{
		cameraList.PushAndGrow(m_FreeCamera);
	}
}
#endif // __BANK

void camDebugDirector::ActivateFreeCamAndCloneFrame()
{
	ActivateFreeCam();

	camFreeCamera* freeCamera = (camFreeCamera*)m_FreeCamera.Get();
	freeCamera->SetFrame(m_CachedInitialDestinationFrame);
}

#else // __FINAL - Stub out everything.

#include "camera/base/BaseCamera.h"

INSTANTIATE_RTTI_CLASS(camDebugDirector,0x9B676D1B)


camDebugDirector::camDebugDirector(const camBaseObjectMetadata& metadata)
: camBaseDirector(metadata)
{
}

//NOTE: These functions should never be called, but because they return references, use the base camera's scratch frame to avoid #if !__FINAL everywhere.

const camFrame&	camDebugDirector::GetFreeCamFrame() const
{
	return camBaseCamera::GetScratchFrame();
}

camFrame& camDebugDirector::GetFreeCamFrameNonConst()
{
	return camBaseCamera::GetScratchFrame();
}

#endif // __FINAL
