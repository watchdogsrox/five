//
// camera/marketing/MarketingDirector.cpp
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#include "camera/marketing/MarketingDirector.h"

#if __BANK //Marketing tools are only available in BANK builds.

#include "bank/bkmgr.h"
#include "grcore/debugdraw.h"

#include "camera/marketing/MarketingFreeCamera.h"
#include "camera/system/CameraFactory.h"
#include "camera/system/CameraMetadata.h"
#include "debug/MarketingTools.h"
#include "system/controlMgr.h"
#include "text/messages.h"

CAMERA_OPTIMISATIONS()

INSTANTIATE_RTTI_CLASS(camMarketingDirector,0xBCD9177B)

XPARAM(rdrDebugCamera);


camMarketingDirector::camMarketingDirector(const camBaseObjectMetadata& metadata)
: camBaseDirector(metadata)
, m_Metadata(static_cast<const camMarketingDirectorMetadata&>(metadata))
, m_ModeIndex(0)
, m_ShakerScaleAngle(0.5f)
, m_DebugOverrideMotionBlurStrengthThisUpdate(-1.0f)
, m_IsActive(false)
, m_IsCameraActive(false)
, m_ShouldUseRdrControlMapping(PARAM_rdrDebugCamera.Get())
, m_IsShakerEnabled(false)
, m_MarketingCamerasInitialised(false)
{
}

camMarketingDirector::~camMarketingDirector()
{
    ShutdownMarketingCameras();
}

void camMarketingDirector::InitMarketingCameras()
{
    //if already initialised return immediately
    if( m_MarketingCamerasInitialised )
        return;

    //Create the cameras.
    const int numModes = m_Metadata.m_Modes.GetCount();
    for(int i=0; i<numModes; i++)
    {
        const camMarketingDirectorMetadataMode& mode = m_Metadata.m_Modes[i];

        const camMarketingFreeCameraMetadata* cameraMetadata =
            camFactory::FindObjectMetadata<camMarketingFreeCameraMetadata>(mode.m_CameraRef.GetHash());
        camMarketingFreeCamera* camera = NULL;
        if(cameraVerifyf(cameraMetadata, "The marketing director has a NULL camera for mode %d: %s", i, SAFE_CSTRING(mode.m_TextLabel)))
        {
            camera = camFactory::CreateObject<camMarketingFreeCamera>(*cameraMetadata);
            cameraAssertf(camera, "The marketing director failed to create a camera (name: %s, hash: %u) for mode %d: %s",
                SAFE_CSTRING(cameraMetadata->m_Name.GetCStr()), cameraMetadata->m_Name.GetHash(), i, SAFE_CSTRING(mode.m_TextLabel));
        }
        m_Cameras.Grow() = camera;

        m_TextLabels.PushAndGrow(mode.m_TextLabel);
    }

    m_MarketingCamerasInitialised = true;
}

void camMarketingDirector::ShutdownMarketingCameras()
{
    if( ! m_MarketingCamerasInitialised )
    {
        return;
    }

    //Destroy the cameras.
    const int numModes = m_Metadata.m_Modes.GetCount();
    for(int i=0; i<numModes; i++)
    {
        camBaseCamera* camera = m_Cameras[i];
        if(camera)
        {
            delete camera;
        }
    }

    m_Cameras.Reset();
    m_TextLabels.Reset();

    m_MarketingCamerasInitialised = false;
}

bool camMarketingDirector::Update()
{
	bool hasSucceeded = false;

	//Default to not rendering unless we have a camera to render.
	m_ActiveRenderState = RS_NOT_RENDERING;

	//The marketing director is only active when the marketing tools are active.
	m_IsActive = CMarketingTools::IsActive();
	if(m_IsActive)
	{
        //first of all check if the marketing cameras have been initialised...
        if( ! m_MarketingCamerasInitialised )
        {
            //...if not do it right now.
            InitMarketingCameras();
        }

		m_Frame.CloneFrom(m_CachedInitialDestinationFrame);

		const bool wasActive				= m_IsCameraActive;
		const s32 previousActiveCameraIndex	= m_ModeIndex;

		UpdateControl(); //Handle user input.

		const bool hasCameraStateChanged	= (m_IsCameraActive != wasActive) || (m_ModeIndex != previousActiveCameraIndex);
		const char* textLabel				= NULL;

		if(m_IsCameraActive)
		{
			m_UpdatedCamera = m_Cameras[m_ModeIndex];
			if(m_UpdatedCamera)
			{
				if(hasCameraStateChanged && m_UpdatedCamera->GetIsClassId(camMarketingFreeCamera::GetStaticClassId()))
				{
					//Reset the camera on activation.
					camMarketingFreeCamera& marketingFreeCamera = static_cast<camMarketingFreeCamera&>(*m_UpdatedCamera);
					marketingFreeCamera.Reset();
				}

				hasSucceeded = m_UpdatedCamera->BaseUpdate(m_Frame);
				if(hasSucceeded)
				{
					m_ActiveRenderState = RS_FULLY_RENDERING;

					if(m_IsShakerEnabled)
					{
						m_Shaker.Update(m_ShakerScaleAngle);

						Matrix34 output = m_Frame.GetWorldMatrix();
						m_Shaker.ApplyShake(output);
						m_Frame.SetWorldMatrix(output);
					}
				}
			}

			textLabel = m_TextLabels[m_ModeIndex];
		}
		else
		{
			textLabel = "MKT_NO_CAM";
		}

		if(hasCameraStateChanged && textLabel)
		{
			//Display confirmation debug text (if the HUD is enabled.)
			char *string = TheText.Get(textLabel);
			CHelpMessage::SetMessageText(HELP_TEXT_SLOT_STANDARD, string);
		}

		UpdateMotionBlur(m_Frame);
	}
    else
    {
        //not active, destroy the cameras
        ShutdownMarketingCameras();
        m_UpdatedCamera = NULL;
        m_IsCameraActive = false;
    }

	return hasSucceeded;
}

//Handles debug pad for toggling activation of the cameras.
void camMarketingDirector::UpdateControl()
{
	CPad& pad = CControlMgr::GetDebugPad();

	if(pad.GetPressedDebugButtons() & ioPad::RRIGHT)
	{
		//Toggle activation.
		m_IsCameraActive = !m_IsCameraActive;
	}
	else if(pad.GetPressedDebugButtons() & ioPad::R1)
	{
		//Cycle the active camera mode.
		m_ModeIndex = (m_ModeIndex + 1) % m_Metadata.m_Modes.GetCount();
	}
}

void camMarketingDirector::UpdateMotionBlur(camFrame& destinationFrame)
{
	if(m_DebugOverrideMotionBlurStrengthThisUpdate >= 0.0f)
	{
		destinationFrame.SetMotionBlurStrength(m_DebugOverrideMotionBlurStrengthThisUpdate);
		m_DebugOverrideMotionBlurStrengthThisUpdate = -1.0f;
	}
}

void camMarketingDirector::AddWidgetsForInstance()
{
	if(m_WidgetGroup == NULL)
	{
		bkBank* bank = BANKMGR.FindBank("Marketing Tools");
		if(bank)
		{
			m_WidgetGroup = bank->PushGroup("Marketing director", false);
			{
				bank->AddCombo("Active camera mode", &m_ModeIndex, m_Metadata.m_Modes.GetCount(), m_TextLabels.GetElements(), NullCB,
					"Select a camera mode");
				bank->AddToggle("Use RDR-style control mapping", &m_ShouldUseRdrControlMapping);
				bank->AddToggle("Shake Enabled", &m_IsShakerEnabled);
				bank->AddSlider("Shaker Scale Angle", &m_ShakerScaleAngle, 0.0f, 30.0f, 0.05f);
				m_Shaker.AddWidgets(*bank);
			}
			bank->PopGroup();
		}
	}
}

void camMarketingDirector::DebugGetCameras(atArray<camBaseCamera*> &cameraList) const
{
    if( ! m_MarketingCamerasInitialised )
    {
        return;
    }

	const int numModes = m_Metadata.m_Modes.GetCount();
	for(int i=0; i<numModes; i++)
	{
		camBaseCamera* camera = m_Cameras[i];
		if(camera)
		{
			cameraList.PushAndGrow(camera);
		}
	}
}

void camMarketingDirector::DebugSetOverrideMotionBlurThisUpdate(const float blurStrength)
{
	m_DebugOverrideMotionBlurStrengthThisUpdate = blurStrength;
}

#else // __BANK

INSTANTIATE_RTTI_CLASS(camMarketingDirector,0xBCD9177B)


camMarketingDirector::camMarketingDirector(const camBaseObjectMetadata& metadata)
: camBaseDirector(metadata)
{
}

#endif // __BANK
