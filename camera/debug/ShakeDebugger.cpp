//
// camera/debug/ShakeDebugger.h
// 
// Copyright (C) 1999-2020 Rockstar Games.  All Rights Reserved.
//
#include "camera/debug/ShakeDebugger.h"

#if __BANK
#include "bank/bank.h"
#include "camera/base/BaseCamera.h"
#include "camera/CamInterface.h"
#include "camera/gameplay/GameplayDirector.h"
#include "camera/helpers/BaseFrameShaker.h"
#include "camera/system/CameraFactory.h"
#include "camera/system/CameraMetadata.h"
#include "data/callback.h"
#include "math/simplemath.h"
#include "parser/macros.h"

CAMERA_OPTIMISATIONS();

EXTERN_PARSER_ENUM(eShakeFrameComponent);

camShakeDebugger::camShakeDebugger()
	: m_ShakeDebuggerWidget(nullptr)
	, m_SelectedShakeWidget(nullptr)
	, m_SelectedShake(-1)
	, m_SelectedVehiclePart(VEH_INVALID_ID)
	, m_Amplitude(1.0f)
{

}

camShakeDebugger::~camShakeDebugger()
{
	Shutdown();
}

void camShakeDebugger::Initialise(const camMetadataStore& metadataStore)
{
	for(const camBaseObjectMetadata* object : metadataStore.m_MetadataList)
	{
		if(camFactory::IsObjectMetadataOfClass<camBaseShakeMetadata>(*object))
		{
			m_ShakesNamesRaw.PushAndGrow(SAFE_CSTRING(object->m_Name.GetCStr()));
		}
	}

	if(!BANKMGR.FindWidget("Camera/Create camera widgets"))
	{
		bkBank* bank = BANKMGR.FindBank("Camera");
		if(bank)
		{
			AddWidgets(*bank);
		}
	}
}

void camShakeDebugger::Shutdown()
{
	m_ShakesNamesRaw.clear();
	m_SelectedShake = -1;
	m_SelectedVehiclePart = -1;
	m_Amplitude = 1.0f;
	if(m_BaseCamera)
	{
		m_BaseCamera->StopShaking(true);
	}
	m_BaseCamera = nullptr;
	m_FrameShaker = nullptr;
	if(m_ShakeDebuggerWidget)
	{
		m_ShakeDebuggerWidget->Destroy();
	}
	m_ShakeDebuggerWidget = nullptr;
	m_SelectedShakeWidget = nullptr;
}

void camShakeDebugger::AddWidgets(bkBank& bank)
{
	m_ShakeDebuggerWidget = bank.PushGroup("Shake Debugger");
	{
		bank.AddCombo("Shakes List", &m_SelectedShake, m_ShakesNamesRaw.GetCount(), m_ShakesNamesRaw.GetElements(), datCallback(MFA(camShakeDebugger::OnShakeSelected), (datBase*)this));
		bank.AddSlider("Amplitude", &m_Amplitude, 0.0f, 10.0f, 0.001f, datCallback(MFA(camShakeDebugger::OnAmplitudeChanged), (datBase*)this));
		bank.AddButton("Start shake on current camera", datCallback(MFA(camShakeDebugger::OnStartShake), (datBase*)this));
		bank.AddButton("Stop shake on current camera", datCallback(MFA(camShakeDebugger::OnStopShake), (datBase*)this));
		m_SelectedShakeWidget = bank.AddGroup("Selected Shake", false);
		m_SelectedShakeWidget->AddButton("Refresh metadata", datCallback(MFA(camShakeDebugger::OnShakeSelected), (datBase*)this));
		bank.PushGroup("Vehicle part broken shake");
		{
			bank.AddSlider("Vehicle part", &m_SelectedVehiclePart, VEH_INVALID_ID, VEH_NUM_NODES, 1);
			bank.AddButton("Test vehicle part broken shake on follow vehicle", datCallback(MFA(camShakeDebugger::OnVehiclePartBrokenShake), (datBase*)this));
		}
		bank.PopGroup();
	}
	bank.PopGroup();
}

void camShakeDebugger::OnShakeSelected()
{
	bkGroup* shakeMetadataWidget = m_SelectedShakeWidget->AddGroup("Metadata", false);

	if(m_SelectedShake >= 0)
	{	
		camBaseShakeMetadata* baseShakeMetadata = const_cast<camBaseShakeMetadata*>(camFactory::FindObjectMetadata<camBaseShakeMetadata>(m_ShakesNamesRaw[m_SelectedShake]));
		
		{
			camBuildWidgetsVisitor visitor;
			visitor.m_CurrBank = static_cast<bkBank*>(shakeMetadataWidget);
			visitor.Visit(*baseShakeMetadata);
		}

		if(baseShakeMetadata->m_OverallEnvelopeRef.IsNotNull())
		{
			camEnvelopeMetadata* envelopeMetadata = const_cast<camEnvelopeMetadata*>(camFactory::FindObjectMetadata<camEnvelopeMetadata>(baseShakeMetadata->m_OverallEnvelopeRef));
			if(envelopeMetadata)
			{
				bkGroup* envelopeWidget = m_SelectedShakeWidget->AddGroup("Overall envelope", false);

				camBuildWidgetsVisitor visitor;
				visitor.m_CurrBank = static_cast<bkBank*>(envelopeWidget);
				visitor.Visit(*envelopeMetadata);
			}
		}

		if(camFactory::IsObjectMetadataOfClass<camShakeMetadata>(*baseShakeMetadata))
		{
			camShakeMetadata* shakeMetadata = static_cast<camShakeMetadata*>(baseShakeMetadata);
			for(camShakeMetadataFrameComponent& comp : shakeMetadata->m_FrameComponents)
			{
				bkGroup* compWidget = m_SelectedShakeWidget->AddGroup(PARSER_ENUM(eShakeFrameComponent).NameFromValue(comp.m_Component), false);

				if(comp.m_EnvelopeRef.IsNotNull())
				{
					camEnvelopeMetadata* envelopeMetadata = const_cast<camEnvelopeMetadata*>(camFactory::FindObjectMetadata<camEnvelopeMetadata>(comp.m_EnvelopeRef));
					if(envelopeMetadata)
					{
						bkGroup* envelopeWidget = compWidget->AddGroup(comp.m_EnvelopeRef.GetCStr(), false);
						camBuildWidgetsVisitor visitor;
						visitor.m_CurrBank = static_cast<bkBank*>(envelopeWidget);
						visitor.Visit(*envelopeMetadata);
					}
				}
				
				if(comp.m_OscillatorRef.IsNotNull())
				{
					camOscillatorMetadata* oscillatorMetadata = const_cast<camOscillatorMetadata*>(camFactory::FindObjectMetadata<camOscillatorMetadata>(comp.m_OscillatorRef));
					if(oscillatorMetadata)
					{
						bkGroup* oscillatorWidget = compWidget->AddGroup(comp.m_OscillatorRef.GetCStr(), false);
						camBuildWidgetsVisitor visitor;
						visitor.m_CurrBank = static_cast<bkBank*>(oscillatorWidget);
						visitor.Visit(*oscillatorMetadata);
					}
				}
			}
		}
	}
}

void camShakeDebugger::OnStartShake()
{
	if(m_SelectedShake >= 0)
	{
		m_BaseCamera = const_cast<camBaseCamera*>(camInterface::GetDominantRenderedCamera());
		m_FrameShaker = m_BaseCamera->Shake(m_ShakesNamesRaw[m_SelectedShake], m_Amplitude);
	}
}

void camShakeDebugger::OnStopShake()
{
	if(m_BaseCamera)
	{
		m_BaseCamera->StopShaking();
	}
	m_BaseCamera = nullptr;
	m_FrameShaker = nullptr;
}

void camShakeDebugger::OnAmplitudeChanged()
{
	if(m_FrameShaker)
	{
		m_FrameShaker->SetAmplitude(m_Amplitude);
	}
}

void camShakeDebugger::OnVehiclePartBrokenShake()
{
	if(m_SelectedVehiclePart == VEH_INVALID_ID)
	{
		return;
	}

	const CVehicle* followVehicle = camInterface::GetGameplayDirector().GetFollowVehicle();

	if(followVehicle)
	{
		camInterface::GetGameplayDirector().RegisterVehiclePartBroken(*followVehicle, static_cast<eHierarchyId>(m_SelectedVehiclePart));
	}
}

#endif //__BANK