// File header
#include "EventReactions.h"

// Framework headers
#include "ai/aichannel.h"

// Game Headers
#include "Event/EventResponse.h"
#include "Peds/Ped.h"
#include "Peds/PedIntelligence.h"
#include "Task/Combat/TaskThreatResponse.h"
#include "Task/Combat/TaskInvestigate.h"
#include "Task/Combat/TaskReact.h"
#include "Task/Response/TaskReactToDeadPed.h"
#include "vehicleAi/VehicleIntelligence.h"
#include "vehicleAi/task/TaskVehicleCruise.h"

#include "event/EventReactions_parser.h"

AI_OPTIMISATIONS()

CEventReactionEnemyPed::CEventReactionEnemyPed(CPed* pThreatPed, bool bForceThreatResponse)
: 
m_pThreatPed(pThreatPed),
m_bForceThreatResponse(bForceThreatResponse) 
{
#if !__FINAL
	m_EventType = EVENT_REACTION_ENEMY_PED;
#endif
}

bool CEventReactionEnemyPed::CreateResponseTask(CPed* pPed, CEventResponse& response) const
{
	if (pPed->IsLocalPlayer())
	{
		return false;
	}

	response.SetEventResponse(CEventResponse::EventResponse, rage_new CTaskThreatResponse(m_pThreatPed));

	return response.HasEventResponse();
}

CEventReactionInvestigateThreat::CEventReactionInvestigateThreat(const CAITarget& threatTarget, s32 iEventType, s32 iThreatType) 
:	
m_threatTarget(threatTarget), 
m_iEventType(iEventType),
m_iThreatType(iThreatType)
{
#if !__FINAL
	m_EventType = EVENT_REACTION_INVESTIGATE_THREAT;
#endif
}

bool CEventReactionInvestigateThreat::CreateResponseTask(CPed* pPed, CEventResponse& response) const
{
	if (pPed->IsLocalPlayer())
	{
		return false;
	}

	// Make sure threat target exists
	if (!m_threatTarget.GetIsValid())
	{
		return false;
	}

	CCombatBehaviour& combatBehaviour = pPed->GetPedIntelligence()->GetCombatBehaviour();

	// If we're allowed to move, investigate, otherwise go into default threat response
	if (combatBehaviour.IsFlagSet(CCombatData::BF_CanInvestigate) && combatBehaviour.GetCombatMovement() != CCombatData::CM_Stationary)
	{
		Vector3 vTargetPos;
		aiVerifyf(m_threatTarget.GetPosition(vTargetPos),"Failed to get target position");
		response.SetEventResponse(CEventResponse::EventResponse, rage_new CTaskInvestigate(vTargetPos, NULL, m_iEventType));
	}
	else if (m_threatTarget.GetEntity() && m_threatTarget.GetEntity()->GetIsTypePed())
	{
		const CEntity* pEntity = m_threatTarget.GetEntity();
		const CPed* pThreatPed = static_cast<const CPed*>(pEntity);
		if (!pThreatPed->GetPedIntelligence()->IsFriendlyWith(*pPed)) 
		{
			response.SetEventResponse(CEventResponse::EventResponse, rage_new CTaskThreatResponse(pThreatPed));
		}
	}

	return response.HasEventResponse();
}

bool CEventReactionInvestigateDeadPed::CreateResponseTask(CPed* pPed, CEventResponse& response) const
{
	if (pPed->IsLocalPlayer())
	{
		return false;
	}

	// Make sure threat ped exists and is dead
	if (!m_pDeadPed || !m_pDeadPed->IsDead())
	{
		return false;
	}

	// If we're allowed to move, investigate, otherwise go into default threat response
	if (pPed->GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_CanInvestigate))
	{
		response.SetEventResponse(CEventResponse::EventResponse, rage_new CTaskInvestigate(VEC3V_TO_VECTOR3(m_pDeadPed->GetTransform().GetPosition())));
	}
	else
	{
		m_pDeadPed->SetPedConfigFlag( CPED_CONFIG_FLAG_HasDeadPedBeenReported, 1 );
	}

	return response.HasEventResponse();
}

bool CEventReactionCombatVictory::CreateResponseTask(CPed* pPed, CEventResponse& response) const
{
	if (pPed->IsLocalPlayer())
	{
		return false;
	}
	// New scenario here?

	// Make sure ped exists
	if (m_pDeadPed)
	{
		if (fwRandom::GetRandomTrueFalse())
		{
			pPed->NewSay("TARGET_KILLED");
			return false;
		}
		else
		{
			response.SetEventResponse(CEventResponse::EventResponse, rage_new CTaskCheckPedIsDead(m_pDeadPed));
		}
	}

	return response.HasEventResponse();
}

bank_float CEventWhistlingHeard::ms_fRange = 10.0f;

CEventWhistlingHeard::CEventWhistlingHeard(CPed* pPedHeardWhistling)
:	
m_pPedHeardWhistling(pPedHeardWhistling)
{
#if !__FINAL
	// Debug variable storing the event type
	m_EventType = EVENT_WHISTLING_HEARD;
#endif // !__FINAL
#if __BANK
	aiFatalAssertf(m_pPedHeardWhistling,"NULL pointer");
	const Vec3V vWhistlingPosition = m_pPedHeardWhistling->GetTransform().GetPosition();
	CEventDebug::ms_debugDraw.AddText(vWhistlingPosition, 0, 0, "Whistling Event Source", Color_blue, 5000);
	CEventDebug::DrawCircle(vWhistlingPosition, ms_fRange, Color_blue);
#endif // __BANK
}

bool CEventWhistlingHeard::CreateResponseTask(CPed* pPed, CEventResponse& response) const
{
	if (pPed->IsLocalPlayer())
	{
		return false;
	}

	// Make sure ped exists
	if (m_pPedHeardWhistling)
	{
		pPed->GetPedIntelligence()->IncreaseAlertnessState();

		//TODO: React to the whistle.
	}

	return response.HasEventResponse();
}

bool CEventWhistlingHeard::AffectsPedCore(CPed* UNUSED_PARAM(pPed)) const
{
	return false;
}

bank_float CEventExplosionHeard::ms_fMaxLoudness = 100.0f;	// Player footsteps = 7.5 

IMPLEMENT_REACTION_EVENT_TUNABLES(CEventExplosionHeard, 0x0d143f2e);

CEventExplosionHeard::Tunables CEventExplosionHeard::sm_Tunables;

bool CEventExplosionHeard::CreateResponseTask(CPed* pPed, CEventResponse& response) const
{
	if (pPed->IsLocalPlayer())
	{
		return false;
	}

	pPed->GetPedIntelligence()->IncreaseAlertnessState();

	if (pPed->GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_CanInvestigate))
	{
		response.SetEventResponse(CEventResponse::EventResponse, rage_new CTaskInvestigate(m_vExplosionPosition, NULL, EVENT_EXPLOSION_HEARD));
	}

	return response.HasEventResponse();
}

CTask* CEventExplosionHeard::CreateVehicleReponseTaskWithParams(sVehicleMissionParams& paramsOut) const
{
	paramsOut.Clear();
	paramsOut.m_iDrivingFlags =DMode_AvoidCars|DF_AvoidTargetCoors|DF_SteerAroundPeds;
	paramsOut.m_fCruiseSpeed = 40.0f;
	paramsOut.SetTargetPosition(m_vExplosionPosition);

	return rage_new CTaskVehicleFlee(paramsOut);
}

bool CEventExplosionHeard::AffectsVehicleCore(CVehicle* pVehicle) const
{
	if (!pVehicle->GetIntelligence()->ShouldHandleEvents())
	{
		return false;
	}

	return CanHearExplosion(pVehicle);
}

bool CEventExplosionHeard::CanHearExplosion(const CEntity* pEntity) const
{
	// Only accept explosions that the ped can hear
	Vector3 vDiff = VEC3V_TO_VECTOR3(pEntity->GetTransform().GetPosition()) - m_vExplosionPosition;
	float fDist2 = vDiff.Mag2();
	static dev_float s_fHearingRangeForPretendOccupants = CPedPerception::ms_fSenseRange;
	float fHearingRange = pEntity->GetIsTypePed() 
		? (static_cast<const CPed*>(pEntity))->GetPedIntelligence()->GetPedPerception().GetHearingRange()
		: s_fHearingRangeForPretendOccupants;

	if (fDist2 > (fHearingRange * fHearingRange))
	{
		// Explosion was out of range
		return false;
	}
	else
	{
		return true; // If it's within hearing range treat all explosions as being heard
	}
	// 		else if (fDist2 > 1.0f) // Avoid divide by zero
	// 		{
	// 			// Scale to range 0.0f, 1.0f
	// 			float fLoudnessScaled = m_fLoudness / ms_fMaxLoudness; 
	// 
	// 			// Footstep was in hearing range, scale the loudness depending on how far away the ped is 
	// 			fLoudnessScaled *= fHearingRange / Sqrtf(fDist2);	
	// 
	// 			// TODO: Make this detection criteria better 
	// 			// The bigger this number the louder the sound is / the closer we are
	// 			// E.g if we take the  rocket loudness = 16
	// 			//		if the ped has a hearing range of 60.0f and is 5m away
	// 			//		fLoudnessScaled = 0.16*60 / 20 = 0.48
	// 			//		fLoudnessScaled = 0.16*60 / 10 = 0.96
	// 			if (fLoudnessScaled > 0.1f)
	// 			{
	// 				return true;
	// 			}
	// 		}
	// 		else
	// 		{
	// 			// distance must be < 1m
	// 			return true;
	// 		}
}

bool CEventExplosionHeard::AffectsPedCore(CPed* pPed) const
{
	if (pPed->IsDead() || pPed->IsPlayer())
	{
		return false;
	}
	else
	{
 		return CanHearExplosion(pPed);
	}
//	return false;
}


bool CEventExplosionHeard::TryToCombine(const CEventExplosionHeard& other)
{
	const Vec3V pos1V = RCC_VEC3V(m_vExplosionPosition);
	const Vec3V pos2V = RCC_VEC3V(other.m_vExplosionPosition);
	const ScalarV thresholdDistSqV = LoadScalar32IntoScalarV(sm_Tunables.m_MaxCombineDistThresholdSquared);
	const ScalarV distSqV = DistSquared(pos1V, pos2V);
	if(IsGreaterThanAll(distSqV, thresholdDistSqV))
	{
		return false;
	}

	// Recompute the explosion position as the average between the two.
	const Vec3V avgPosV = Average(pos1V, pos2V);
	m_vExplosionPosition = VEC3V_TO_VECTOR3(avgPosV);

	// This parameter isn't really used right now. If it had been used and didn't
	// always have the same value, we could use it for scaling the position adaptation,
	// or do something to make the combined explosion louder.
	m_fLoudness = Max(m_fLoudness, other.m_fLoudness);

	return true;
}

bool CEventExplosionHeard::operator==( const fwEvent& otherEvent ) const
{
	if( static_cast<const CEvent&>(otherEvent).GetEventType() == GetEventType() )
	{
		const CEventExplosionHeard& otherExplosionHeardEvent = static_cast<const CEventExplosionHeard&>(otherEvent);
		if( m_vExplosionPosition.IsClose(otherExplosionHeardEvent.m_vExplosionPosition, 20.f) &&
			Abs(m_fLoudness - otherExplosionHeardEvent.m_fLoudness) < SMALL_FLOAT )
		{
			return true;
		}
	}
	return false;
}

CEventDisturbance::CEventDisturbance(CPed* pedBeingDisturbed, CPed* pedDoingDisturbance, eDisturbanceType disturbanceType)
: m_pPedBeingDisturbed(pedBeingDisturbed)
, m_pPedDoingDisturbance(pedDoingDisturbance)
, m_eDisturbanceType(disturbanceType)
{
#if !__FINAL
	// Debug variable storing the event type
	m_EventType = EVENT_DISTURBANCE;
#endif // !__FINAL
}

bool CEventDisturbance::CreateResponseTask(CPed* pPed, CEventResponse& response) const
{
	switch (m_eDisturbanceType)
	{
		case ePedKilledByStealthKill:
			return false;
		case ePedHeardFalling:
		case ePedInNearbyCar:
			if (pPed->IsLocalPlayer())
			{
				return false;
			}

			if (pPed->GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_CanInvestigate))
			{
				response.SetEventResponse(CEventResponse::EventResponse, rage_new CTaskInvestigate(VEC3V_TO_VECTOR3(m_pPedDoingDisturbance->GetTransform().GetPosition()), NULL, EVENT_DISTURBANCE));
			}
			else
			{
				response.SetEventResponse(CEventResponse::EventResponse, rage_new CTaskThreatResponse(m_pPedDoingDisturbance));
			}
			return response.HasEventResponse();
		default: aiAssertf(0,"Unhandled enum in CEventDisturbance::CreateResponseTask");
			break;
	}
	return false;
}

bool CEventDisturbance::AffectsPedCore(CPed* pPed) const
{
	if (pPed->IsDead() || pPed->IsPlayer() || !m_pPedDoingDisturbance)
	{
		return false;
	}

	return false;
}

#if __BANK

bool CEventDebug::ms_bRenderEventDebug = false;

#if DEBUG_DRAW
#define MAX_EVENT_DRAWABLES 100
CDebugDrawStore CEventDebug::ms_debugDraw(MAX_EVENT_DRAWABLES);
#endif // DEBUG_DRAW

void CEventDebug::InitWidgets()
{
	bkBank* pBank = BANKMGR.FindBank("A.I.");
	if (pBank)
	{
		pBank->PushGroup("Event Debug", false);
			pBank->AddToggle("Render Debug", &ms_bRenderEventDebug);
#if __DEV
			pBank->AddButton("Spew Event Pool", SpewPoolUsage);
#endif
			pBank->PushGroup("Whistling Event", false);
				pBank->AddSlider("Whistling Range", &CEventWhistlingHeard::ms_fRange, 0.0f, 50.0f, 0.1f);	
			pBank->PopGroup();
		pBank->PopGroup();
	}
}

#define NUM_CIRCLE_SEGMENTS 13

void CEventDebug::DrawCircle( Vec3V_In vCentre, const float fRadius, const Color32 colour )
{
	static const float s_fNumLinesToUse = 10.0f;

	const ScalarV fRadAngleSeparation = ScalarVFromF32(2.0f*PI / s_fNumLinesToUse);

	Vec3V vStartPoint = VECTOR3_TO_VEC3V(YAXIS * fRadius);
	Vec3V vEndPoint;

	for (s32 i = 0; i <= s_fNumLinesToUse; ++i)
	{
		// Get the end point of the line by rotating about by the angle separation
		vEndPoint = vStartPoint;
		vEndPoint = RotateAboutZAxis(vEndPoint, fRadAngleSeparation);

		// Get the world position of the vector
		vStartPoint += vCentre;
		vEndPoint += vCentre;

		// Draw the line segment
		ms_debugDraw.AddLine(vStartPoint,vEndPoint,colour,5000);

		// Get the local points again
		vStartPoint -= vCentre;
		vEndPoint -= vCentre;

		// Set the new start point to be the end point of this line
		vStartPoint = vEndPoint;
	}
}

#if __DEV
void CEventDebug::SpewPoolUsage()
{
	CEvent::GetPool()->PoolFullCallback();
}
#endif // __DEV

void CEventDebug::Debug()
{
#if DEBUG_DRAW
	if (ms_bRenderEventDebug)
	{
		ms_debugDraw.Render();
	}
#endif // DEBUG_DRAW
}
#endif // __BANK
