//
// camera/helpers/BaseFrameShaker.cpp
// 
// Copyright (C) 1999-2013 Rockstar Games.  All Rights Reserved.
//

#include "camera/helpers/BaseFrameShaker.h"

#include "bank/bank.h"
#include "fwmaths/angle.h"
#include "fwsys/timer.h"
#include "grcore/debugdraw.h"

#include "camera/CamInterface.h"
#include "camera/gameplay/GameplayDirector.h"
#include "camera/helpers/Envelope.h"
#include "camera/system/CameraMetadata.h"
#include "camera/system/CameraFactory.h"

CAMERA_OPTIMISATIONS()

INSTANTIATE_RTTI_CLASS(camBaseFrameShaker,0xD7946DAB)

camBaseFrameShaker::camBaseFrameShaker(const camBaseObjectMetadata& metadata)
: camBaseObject(metadata)
, m_Metadata(static_cast<const camShakeMetadata&>(metadata))
, m_NumUpdatesPerformed(0)
, m_Amplitude(1.0f)
, m_IsActive(true)
, m_ShouldBypassThisUpdate(false)
, m_UseNonPausedCameraTime(false)
#if GTA_REPLAY
, m_TimeScaleFactor(1.0f)
, m_TimeOffset(0)
#endif
{
	//Instantiate the optional overall envelope.
	const camEnvelopeMetadata* envelopeMetadata = camFactory::FindObjectMetadata<camEnvelopeMetadata>(m_Metadata.m_OverallEnvelopeRef.GetHash());
	if(envelopeMetadata)
	{
		m_OverallEnvelope = camFactory::CreateObject<camEnvelope>(*envelopeMetadata);

		if(cameraVerifyf(m_OverallEnvelope, "A camera frame shaker (name: %s, hash: %u) failed to create an overall envelope (name: %s, hash: %u)",
			GetName(), GetNameHash(), SAFE_CSTRING(envelopeMetadata->m_Name.GetCStr()),
			envelopeMetadata->m_Name.GetHash()))
		{
			m_OverallEnvelope->SetUseGameTime(m_Metadata.m_UseGameTime);				// Set flag first or start time will be incorrect.
			m_OverallEnvelope->SetReversible(m_Metadata.m_IsReversible);
			m_OverallEnvelope->Start();
		}
	}
	else
	{
		m_OverallEnvelope = NULL;
	}

	m_StartTime = GetCurrentTimeMilliseconds();
}

camBaseFrameShaker::~camBaseFrameShaker()
{
	if(m_OverallEnvelope)
	{
		delete m_OverallEnvelope;
		m_OverallEnvelope = NULL;
	}
}

void camBaseFrameShaker::Update(camFrame& frameToShake, float amplitudeScalingFactor)
{
	if(!m_IsActive)
	{
		//NOTE: This is safe as frame shakers are persistently referenced using registered references (of type RegdCamBaseFramShaker.)
		delete this;
		return;
	}

	if(m_ShouldBypassThisUpdate)
	{
		m_ShouldBypassThisUpdate = false;
		return;
	}

	float overallEnvelopeLevel = 1.0f;
	if(m_OverallEnvelope)
	{
		if(m_OverallEnvelope->IsActive())
		{
			overallEnvelopeLevel = m_OverallEnvelope->Update(REPLAY_ONLY(m_TimeScaleFactor, m_TimeOffset));
		}
		else
		{
			//The overall envelope has completed, so there is nothing to update.
			//NOTE: This is safe as frame shakers are persistently referenced using registered references (of type RegdCamFramShaker.)
			delete this;
			return;
		}
	}

	//Temporarily scale the amplitude.
	const float initialAmplitude	= m_Amplitude;
	m_Amplitude						*= amplitudeScalingFactor;

	const bool hasSucceeded = UpdateFrame(overallEnvelopeLevel, frameToShake);
	if(hasSucceeded)
	{
		m_NumUpdatesPerformed++;

		m_Amplitude = initialAmplitude;
	}
	else
	{
		//NOTE: This is safe as frame shakers are persistently referenced using registered references (of type RegdCamBaseFramShaker.)
		delete this;
	}
}

void camBaseFrameShaker::AutoStart()
{
	if(m_OverallEnvelope)
	{
		m_OverallEnvelope->AutoStart();
	}
	else
	{
		//We don't have an overall envelope, so just ensure the shake is active.
		m_IsActive = true;
	}
}

void camBaseFrameShaker::Release()
{
	if(m_OverallEnvelope)
	{
		m_OverallEnvelope->Release();
	}
	else
	{
		//We don't have an overall envelope, so just deactivate.
		m_IsActive = false;
	}
}

void camBaseFrameShaker::RemoveRef(RegdCamBaseFrameShaker& shakeRef)
{
	shakeRef = NULL;
	if(!IsReferenced())
	{
		//This frame shake is no longer referenced, so it can be deleted.
		delete this;
	}
}

void camBaseFrameShaker::CleanupUnreferencedFrameShakers()
{
	const s32 maxNumFrameShakers = camBaseFrameShaker::GetPool()->GetSize();
	for(s32 i=0; i<maxNumFrameShakers; i++)
	{
		camBaseFrameShaker* frameShaker = camBaseFrameShaker::GetPool()->GetSlot(i);
		if(frameShaker && !frameShaker->IsReferenced())
		{
			delete frameShaker;
		}
	}
}

float camBaseFrameShaker::GetReleaseLevel()
{
	float fLevel = 1.0f;
	if (m_OverallEnvelope && m_OverallEnvelope->IsInReleasePhase())
	{
		const float maxReleaseLevel = m_OverallEnvelope->GetMaxReleaseLevel();
		fLevel = (maxReleaseLevel >= SMALL_FLOAT) ? (m_OverallEnvelope->GetLevel() / maxReleaseLevel) : 0.0f;
	}

	return fLevel;
}

void camBaseFrameShaker::OverrideStartTime(u32 startTime)
{
	m_StartTime = startTime;
	if (m_OverallEnvelope)
	{
		m_OverallEnvelope->OverrideStartTime(startTime);
	}
}

void camBaseFrameShaker::SetUseNonPausedCameraTime(bool b)
{ 
	m_UseNonPausedCameraTime = b;
	if (m_OverallEnvelope)
	{
		m_OverallEnvelope->SetUseNonPausedCameraTime(b);
		if (b)
		{
			m_OverallEnvelope->SetReversible(false);
		}
	}
}

bool camBaseFrameShaker::IsOneShot() const
{
	return m_OverallEnvelope && m_OverallEnvelope->GetHoldDuration()!=-1;
}

u32 camBaseFrameShaker::GetCurrentTimeMilliseconds() const
{
	return (m_Metadata.m_UseGameTime) ? fwTimer::GetTimeInMilliseconds() : fwTimer::GetCamTimeInMilliseconds();
}

void camBaseFrameShaker::ShakeFrame(camFrame& frameToShake) const
{
	float overallEnvelopeLevel = 1.0f;

	if(m_OverallEnvelope)
	{
		if(m_OverallEnvelope->IsActive())
		{
			overallEnvelopeLevel = m_OverallEnvelope->GetLevel();
		}
		else
		{
			return;
		}
	}

	ShakeFrame(overallEnvelopeLevel, frameToShake);
}
