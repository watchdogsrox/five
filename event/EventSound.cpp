//
// event/EventSound.cpp
//
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#include "event/EventSound.h"
#include "audio/pedaudioentity.h"		// Temp, audFootstepEventTuning is probably misplaced in 'EventSound.psc'.
#include "Peds/ped.h"
#include "Peds/PedIntelligence.h"
#include "task/Combat/TaskReact.h"
#include "task/Combat/TaskThreatResponse.h"

#include "event/EventSound_parser.h"

AI_OPTIMISATIONS()

//-----------------------------------------------------------------------------
// CEventSoundBase

CEventSoundBase::CEventSoundBase(CEntity* pEntityMakingSound, float soundRange)
		: m_pEntityMakingSound(pEntityMakingSound)
		, m_SoundRange(soundRange)
{
//	m_audioPathRequest = PATH_HANDLE_NULL;	//static_cast<TPathHandle>(PATH_INVALID_HANDLE);
//
//	if(-1==m_iTimeHeard)
//	{
//		Assert(m_pEntityMakingSound);
//		m_iTimeHeard=fwTimer::GetTimeInMilliseconds();
//		m_vTargetPosAtHeardTime=VEC3V_TO_VECTOR3(m_pEntityMakingSound->GetTransform().GetPosition());
//	}
}


CEventSoundBase::~CEventSoundBase()
{
	// Cancel any outstanding audio pathfinding requests
//	if( m_audioPathRequest != PATH_HANDLE_NULL)
//	{
//		CPathServer::CancelRequest( m_audioPathRequest );
//		m_audioPathRequest = PATH_HANDLE_NULL;	//static_cast<TPathHandle>(PATH_INVALID_HANDLE);
//	}
}

#if 0

//-------------------------------------------------------------------------
// Returns true if the event is ready, this is where any event processing
// is carried out, for example path finding requests
//-------------------------------------------------------------------------

bool CEventSoundBase::IsEventReady(CPed* /*pPed*/)
{
	// Audio paths not yet implemented

	/*
	// If the audio request is invalid, begin an audio search
	if( m_audioPathRequest == PATH_HANDLE_NULL )
	{
	if( GetSourceEntity() )
	{
	m_audioPathRequest = CPathServer::RequestAudioPath( GetSourceEntity()->GetPosition(), pPed->GetPosition(), m_Volume, NULL);
	}
	else
	{
	m_audioPathRequest = CPathServer::RequestAudioPath( m_vTargetPosAtHeardTime, pPed->GetPosition(), m_Volume, NULL);
	}
	}
	else
	{
	// There is a request outstanding, is it finised?
	if( CPathServer::IsRequestResultReady( m_audioPathRequest )==ERequest_Ready )
	{
	// [TODO] PH query the audio path result, not implemented on the pathserver side yet
	CPathServer::CancelRequest( m_audioPathRequest );
	m_audioPathRequest = PATH_HANDLE_NULL;
	m_VolumeFromPathServer = GetSoundLevel( NULL, pPed->GetPosition() );

	// reset any already calculated task,
	// and re-calculate
	m_iTaskType = CTaskTypes::TASK_NONE;
	ComputeResponseTaskType( pPed, false );
	return true;
	}		
	}
	return false;
	*/
}

#endif	// 0

/*
PURPOSE
	Returns true if the sound event will affect the ped
	for a dynamic event
*/
bool CEventSoundBase::AffectsPedCore(CPed* pPed) const
{
	// Check to make sure the listening ped can respond to sound events.
	if(!pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_ListensToSoundEvents))
		return false;

	if(pPed->IsDead())
		return false;

	if(!GetSourceEntity())
		return false;

	// Check to make sure the source ped (if there is one) can generate sound events.
	CPed* pSourcePed = GetSourcePed();
	if (pSourcePed && !pSourcePed->GetPedConfigFlag(CPED_CONFIG_FLAG_GeneratesSoundEvents))
	{
		return false;
	}

	// In the best case scenario, a direct LOS, would the ped be able to hear this
	// event? if not don't add the event, there's no point calculating the audio path
	// as theres no-way they would be able to hear
	if(!CanPedHearSound(*pPed, m_pEntityMakingSound->GetTransform().GetPosition(), m_SoundRange))
	{
		return false;
	}
	return true;
}

bool CEventSoundBase::ShouldCreateResponseTaskForPed(const CPed* pPed) const
{
	// reject this event if the source entity is no longer valid, or if it was this ped that generated the event
	if( ( 0==GetSourceEntity() ) || ( GetSourceEntity() == pPed ) )
	{
		return false;
	}

	return true;
}

bool CEventSoundBase::operator==(const fwEvent& otherEvent) const
{
	if(static_cast<const CEvent&>(otherEvent).GetEventType() == GetEventType())
	{
		const CEventSoundBase& otherSoundEvent = static_cast<const CEventSoundBase&>(otherEvent);
		if(otherSoundEvent.m_pEntityMakingSound.Get() == m_pEntityMakingSound.Get() && 
			otherSoundEvent.m_SoundRange == m_SoundRange &&
			otherSoundEvent.m_iTaskType == m_iTaskType && 
			otherSoundEvent.m_FleeFlags == m_FleeFlags &&
			otherSoundEvent.m_SpeechHash == m_SpeechHash &&
			otherSoundEvent.m_bWitnessedFirstHand == m_bWitnessedFirstHand)
		{
			return true;
		}
	}
	return false;
}

bool CEventSoundBase::CanPedHearSound(const CPed& listener, Vec3V_In soundPos, const float& soundRange)
{
	const Vec3V soundPosV = soundPos;
	const Vec3V listenerPosV = listener.GetTransform().GetPosition();
	const ScalarV soundRangeV = LoadScalar32IntoScalarV(soundRange);
	const ScalarV soundRangeSqV = Scale(soundRangeV, soundRangeV);
	const ScalarV distSqV = DistSquared(soundPosV, listenerPosV);
	return IsLessThanAll(distSqV, soundRangeSqV) != 0;

	// For reference, this is how the hearing logic used to work for CEventFootStepHeard:
#if 0
	// Only accept noise that the ped can hear
	Vector3 vDiff = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition() - m_pNoiseMaker->GetTransform().GetPosition());
	float fDist2 = vDiff.Mag2();

	if (fDist2 > (fHearingRange * fHearingRange))
	{
		// Footstep was out of range
		return false;
	}
	else if (fDist2 > 1.0f) // Avoid divide by zero
	{
		// Scale to range 0.0f, 1.0f
		float fLoudnessScaled = m_fLoudness / ms_fMaxLoudness; 
		
		// Footstep was in hearing range, scale the loudness depending on how far away the ped is 
		fLoudnessScaled *= fHearingRange / Sqrtf(fDist2);	
		
		// TODO: Make this detection criteria better 
		// The bigger this number the louder the sound is / the closer we are
		// E.g if we take the player's footsteps loudness = 7.5
		//		if the ped has a hearing range of 60.0f and is 5m away
		//		fLoudnessScaled = 0.075*60 / 5 = 0.9
		//		fLoudnessScaled = 0.075*60 / 10 = 0.45
		if (fLoudnessScaled < 0.7f)
		{
			return false;
		}
	}
	else
	{
		// distance must be < 1m
	}
#endif
}

//-----------------------------------------------------------------------------
// CEventCommunicateEvent

/*
PURPOSE
	Constructor.
*/
CEventCommunicateEvent::CEventCommunicateEvent(CEntity* pEntityMakingSound, CEvent* pEvent, float soundRange)
		//, const int iTimeHeard, const Vector3& vTargetPosAtHeardTime )
		: m_bFinished(false)
{
#if !__FINAL
	m_EventType = EVENT_COMMUNICATE_EVENT;
#endif

	m_pEntityMakingSound = pEntityMakingSound;
	m_SoundRange = soundRange;

	Assert( pEvent );

	m_pEvent = pEvent;

//	m_audioPathRequest = 0;
//	if(-1==m_iTimeHeard && m_pEntityMakingSound)
//	{
//		m_iTimeHeard=fwTimer::GetTimeInMilliseconds();
//		m_vTargetPosAtHeardTime= VEC3V_TO_VECTOR3(m_pEntityMakingSound->GetTransform().GetPosition());
//	}
}


/*
PURPOSE
	Destructor.
*/
CEventCommunicateEvent::~CEventCommunicateEvent()
{
	// Cancel any outstanding audio pathfinding requests
//	if( m_audioPathRequest != PATH_HANDLE_NULL )
//	{
//		CPathServer::CancelRequest( m_audioPathRequest );
//		m_audioPathRequest = PATH_HANDLE_NULL;
//	}

	if( m_pEvent )
	{
		delete m_pEvent;
		m_pEvent = NULL;
	}
}


CEvent* CEventCommunicateEvent::Clone() const
{
	return rage_new CEventCommunicateEvent( m_pEntityMakingSound, static_cast<CEvent*>(m_pEvent->Clone()), m_SoundRange);	//, m_iTimeHeard, m_vTargetPosAtHeardTime );
}


/*
PURPOSE
	Returns true if the event is ready, this is where any event processing
	is carried out, for example path finding requests.
*/
bool CEventCommunicateEvent::IsEventReady(CPed* pPed)
{
	if(CanBeHeardBy(pPed))
	{
		Assert(CEvent::GetPool()->IsValidPtr(m_pEvent));
		pPed->GetPedIntelligence()->AddEvent( *m_pEvent, false, false );
		m_bFinished = true;
	}
	return true;
}

/*
PURPOSE
	Returns true if the communication event will be able to reach the ped.
*/
bool CEventCommunicateEvent::AffectsPedCore(CPed* pPed) const
{
	if (!CanBeCommunicatedTo(pPed))
	{
		return false;
	}

	if( !m_pEvent->AffectsPed( pPed ) )
	{
		return false;
	}

	return true;
}

/*
PURPOSE
	Returns true if the communication event can be communicated to the ped (without taking into account it the communicated event itself can affect him).
*/
bool CEventCommunicateEvent::CanBeCommunicatedTo(const CPed* pPed) const
{
	if (!pPed)
	{
		return false;
	}

	if( m_bFinished )
	{
		return false;
	}

	if(pPed->IsPlayer())
		return false;

	if(pPed->IsDead())
		return false;

	if(!GetSourceEntity())
		return false;

	if( !m_pEvent )
	{
		return false;
	}

	// In the best case scenario, a direct LOS, would the ped be able to hear this
	// event? if not don't add the event, there's no point calculating the audio path
	// as theres no-way they would be able to hear
	if(!CanBeHeardBy(pPed))
	{
		return false;
	}

	return true;
}

/*
PURPOSE
	Returns true if the communication event is at hear range.
*/
bool CEventCommunicateEvent::CanBeHeardBy(const CPed* pPed) const
{
	if (!pPed)
	{
		return false;
	}

	const CEntity* pSrc = GetSourceEntity();
	if (!pSrc)
	{
		return false;
	}

	const float fSoundRange = pPed->GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_UseMaxSenseRangeWhenReceivingEvents) ?
		pPed->GetPedIntelligence()->GetPedPerception().GetSenseRange() : m_SoundRange;

	return CEventSoundBase::CanPedHearSound(*pPed, pSrc->GetTransform().GetPosition(), fSoundRange);
}

bool CEventCommunicateEvent::operator==(const fwEvent& otherEvent) const
{
	if(static_cast<const CEvent&>(otherEvent).GetEventType() == GetEventType())
	{
		const CEventCommunicateEvent& otherCommunicateEvent = static_cast<const CEventCommunicateEvent&>(otherEvent);
		if(m_pEntityMakingSound == otherCommunicateEvent.m_pEntityMakingSound &&
			m_SoundRange == otherCommunicateEvent.m_SoundRange &&
			m_bFinished == otherCommunicateEvent.m_bFinished)
		{
			if(m_pEvent != NULL && otherCommunicateEvent.m_pEvent != NULL && *m_pEvent == *otherCommunicateEvent.m_pEvent)
			{
				return true;
			}
			else if(m_pEvent == otherCommunicateEvent.m_pEvent)
			{
				return true;
			}
		}
	}
	return false;
}

//-----------------------------------------------------------------------------
// CEventFootStepHeard

CEventFootStepHeard::Tunables CEventFootStepHeard::sm_Tunables;
IMPLEMENT_SOUND_EVENT_TUNABLES(CEventFootStepHeard, 0x1d8409c6);


CEventFootStepHeard::CEventFootStepHeard(CPed* pPedHeardWalking, float soundRange)
		: CEventSoundBase(pPedHeardWalking, soundRange)
{
#if !__FINAL
	m_EventType = EVENT_FOOT_STEP_HEARD;
#endif
}


CEventEditableResponse* CEventFootStepHeard::CloneEditable() const
{
	return rage_new CEventFootStepHeard(GetSourcePed(), m_SoundRange);
}


bool CEventFootStepHeard::CreateResponseTask(CPed* pPed, CEventResponse& response) const
{
	// Not sure if this is necessary:
	if (pPed->IsLocalPlayer())
	{
		return false;
	}

	// Make sure ped exists
	CPed* pNoiseMaker = GetSourcePed();
	if (pNoiseMaker)
	{
		response.SetEventResponse(CEventResponse::EventResponse, rage_new CTaskThreatResponse(pNoiseMaker));
	}

	return response.HasEventResponse();
}


bool CEventFootStepHeard::AffectsPedCore(CPed* pPed) const
{
	if(!CEventSoundBase::AffectsPedCore(pPed))
	{
		return false;
	}

	// Not sure if this is necessary:
	CPed* pNoiseMaker = GetSourcePed();
	if(pPed->GetPedIntelligence()->IsFriendlyWith(*pNoiseMaker))
	{
		return false;
	}

	// Treat crouching peds or peds in stealth mode as unhearable
	if(pNoiseMaker->GetIsCrouching() || pNoiseMaker->GetMotionData()->GetUsingStealth())
	{
		return false;
	}

	// If CPED_CONFIG_FLAG_CheckLoSForSoundEvents is set, don't use this event to respond to the ped; it'll be checked for in CPedAcquaintanceScanner [5/14/2013 mdawe]
	if (pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_CheckLoSForSoundEvents))
	{
		// Delegate to acquaintance scanner
		if (CPedAcquaintanceScanner::ms_bFixCheckLoSForSoundEvents)
		{
			static const ScalarV scDEFAULT_MAX_HEARING_DISTANCE_WHEN_CHECKING_LOS(3.0f);

			ScalarV scMaxHearingDistanceSquared(scDEFAULT_MAX_HEARING_DISTANCE_WHEN_CHECKING_LOS);

			if (pPed->GetPedIntelligence()->GetCombatBehaviour().GetCombatFloat(kAttribFloatMaxDistanceToHearEventsUsingLOS) >= 0)
			{
				scMaxHearingDistanceSquared = ScalarVFromF32(rage::square(pPed->GetPedIntelligence()->GetCombatBehaviour().GetCombatFloat(kAttribFloatMaxDistanceToHearEventsUsingLOS)));
			} 

			ScalarV scCurrentDistanceSq = DistSquared(pPed->GetTransform().GetPosition(), pNoiseMaker->GetTransform().GetPosition());
			if (IsLessThanAll(scMaxHearingDistanceSquared, scCurrentDistanceSq))
			{
				return false;
			}

			pPed->GetPedIntelligence()->GetEventScanner()->GetAcquaintanceScanner().RegisterScanRequest(*pNoiseMaker, 0x0, TargetOption_UseFOVPerception);
		}

		return false;
	}
	// We're not checking LOS. Script can set a distance beyond which sounds should not be heard.
	else if (pPed->GetPedIntelligence()->GetCombatBehaviour().GetCombatFloat(kAttribFloatMaxDistanceToHearEvents) >= 0)
	{
		ScalarV vMaxDistanceSq = ScalarVFromF32(rage::square(pPed->GetPedIntelligence()->GetCombatBehaviour().GetCombatFloat(kAttribFloatMaxDistanceToHearEvents)));
		ScalarV vCurrentDistanceSq = DistSquared(pPed->GetTransform().GetPosition(), pNoiseMaker->GetTransform().GetPosition());
		if (IsLessThanAll(vMaxDistanceSq, vCurrentDistanceSq))
		{
			return false;
		}
	}

	return true;
}

//-----------------------------------------------------------------------------

// For reference, this is the old sound attenuation model used by CEvent.
// Even if we now keep track of the sound range in meters, we could convert
// to dB, apply this attenuation model, and compare against a sensitivity threshold.
// This could make sense if we need something more accurate, perhaps taking into
// account background noise and different sensitivity for different animals.
#if 0

float CEvent::GetSoundLevel(const CEntity* pSourceEnt, const Vector3& vecMicPos) const
{
	if(pSourceEnt != NULL && GetSourceEntity() != pSourceEnt)
	{
		return 0.0f;
	}
	else if(GetLocalSoundLevel() == 0.0f)
	{
		return 0.0f;
	}

	float fRange = 1000.0f;
	if(GetSourceEntity())
	{
		fRange = (vecMicPos - VEC3V_TO_VECTOR3(GetSourceEntity()->GetTransform().GetPosition())).Mag();
	}

	if(fRange < 1.0f)
	{
		fRange = 1.0f;
	}

	// Decimal power level divided by distance squared can be calculated
	// in decibel scale by subtracting decibel scale range (kinda)
	// since: Log(a/b) = Log(a) - Log(b)
	// also take into account atmospheric absorption at a rate of 1db per 100m
	return (GetLocalSoundLevel() - 10.0f * rage::Log10(fRange * fRange) - fRange / 100.0f);
}

float CEvent::CalcSoundLevelIncrement(float fOrig, float fAdd)
{
	if(fAdd == 0.0f)
	{
		return 0.0f;
	}
	else if(fOrig == 0.0f)
	{
		return fAdd;
	}

	// Need to convert decibel levels to decimal power levels
	fOrig = rage::Powf(10.0f, 0.1f * fOrig);
	fAdd  = rage::Powf(10.0f, 0.1f * fAdd);

	// Then use ratio of new total to old total to work out how much decibels to add
	fAdd = 10.0f * rage::Log10((fOrig + fAdd) / fOrig);
	return fAdd;
}

#endif	// 0

// End of file 'event/EventSound.cpp'
