//
// event/EventShocking.cpp
//
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#include "EventShocking.h"


#include "event/EventResponseFactory.h"
#include "event/ShockingEvents.h"
#include "camera/CamInterface.h"
#include "camera/gameplay/GameplayDirector.h"
#include "game/Crime.h"
#include "Peds/ped.h"
#include "Peds/PedIntelligence.h"
#include "Peds/PedMotionData.h"
#include "pathserver/PathServer.h"
#include "Vehicles/Heli.h"
#include "Vehicles/Planes.h"
#include "scene/Physical.h"
#include "scene/world/GameWorld.h"
#include "system/ControlMgr.h"
#include "Task/Default/TaskAmbient.h"
#include "Task/Movement/TaskCollisionResponse.h"
#include "Task/Scenario/Info/ScenarioInfo.h"
#include "Task/Scenario/Types/TaskUseScenario.h"
#include "task/Response/TaskAgitated.h"
#include "Task/Response/TaskShockingEvents.h"
#include "weapons/projectiles/Projectile.h"
#include "Peds/PedGeometryAnalyser.h"

#include "event/EventShocking_parser.h"

AI_OPTIMISATIONS()

//////////////////////////////////////////////////////////////////////////
//SHOCKING EVENTS
//////////////////////////////////////////////////////////////////////////

u32 CEventShocking::sm_IdCounter;

CEventShocking::CEventShocking()
{
	m_pEntitySource = NULL;
	m_pEntityOther = NULL;
	m_bUseSourcePosition = false;

	m_vReactPosition = Vec3V(V_ZERO);
	m_bHasReactPosition = false;

	m_vEventPosition = Vec3V(V_ZERO);
	m_fEventRadius = 0.0f;
	m_nCountAdds = 0;

	m_bHasSourceEntity = false;
	m_bUseSourcePosition = false;

	m_pEntitySource = NULL;
	m_pEntityOther = NULL;

	m_iStartTime = 0;
	m_iLifeTime = 0;
	m_iPedGenBlockingAreaIndex = -1;

	m_Priority = E_PRIORITY_SHOCKING_EVENT_INTERESTING;

	// Generate a new ID for the new event. Note that if we're copying another event, this will
	// actually not be used, so we may waste some ID numbers, but using 32 bits it's unlikely that
	// wrapping will be a significant problem, at least as long as the ID number is only used for minor
	// things (like now, it's only used for head tracking).
	if(sm_IdCounter == 0)
	{
		sm_IdCounter++;			// Probably good to reserve 0 as a placeholder for invalid events.
	}
	m_iId = sm_IdCounter++;

	m_CreatedByScript = false;

	m_bHasAppliedMotivationalImpacts = false;
	m_bUseGenericAudio = false;
	m_bIgnoreIfLaw = false;
	m_bCanBeMerged = true;

	m_fVisualReactionRangeOverride = -1.0f;
	m_fAudioReactionRangeOverride = -1.0f;

	m_bDisableSenseChecks = false;

	SetIsShockingEvent(true);
}


bool CEventShocking::IsExposedToPed(CPed *UNUSED_PARAM(pPed)) const
{
	// For now, this should prevent shocking events from being
	// dispatched to peds.
	return false;
}

bool CEventShocking::IsExposedToScripts() const
{
	return false;
}


bool CEventShocking::ProcessInGlobalGroup()
{
	if(!CheckHasExpired())
	{
		ApplySpecialBehaviors();
		return true;
	}
	return false;
}


void CEventShocking::ApplySpecialBehaviors()
{
	const Tunables& tune = GetTunables();
	if(tune.m_AddPedGenBlockedArea)
	{
		// Make sure the radius of the blocking areas is at least as large as the minimum
		// value set from the tuning data. Hopefully this will more reliably prevent spawning
		// close to gunfights. See B* 361141: "[LB] We were arrested and were put back outside
		// the police station, we had a wanted level and loads of scenario cops spawned and killed us."
		float blockRadius = Max(m_fEventRadius, tune.m_PedGenBlockedAreaMinRadius);
		
		if (m_iPedGenBlockingAreaIndex == -1)
		{
			const bool bShrink   = false; // Turn this on to enable the pedGenBlockingArea to shrink over time [3/4/2013 mdawe]
			m_iPedGenBlockingAreaIndex = CPathServer::GetAmbientPedGen().AddPedGenBlockedArea(RCC_VECTOR3(m_vEventPosition), blockRadius, tune.m_PedGenBlockingAreaLifeTimeMS, CPedGenBlockedArea::EShockingEvent, bShrink);
		}
		else
		{
			CPathServer::GetAmbientPedGen().UpdatePedGenBlockedArea(m_iPedGenBlockingAreaIndex, RCC_VECTOR3(m_vEventPosition), blockRadius, tune.m_PedGenBlockingAreaLifeTimeMS);
		}
	}
}


bool CEventShocking::CheckHasExpired() const
{
	//if the events SourceEntity has died, it will die too (may get changed later - MGG)
	if(m_bHasSourceEntity && !m_pEntitySource)
		return true;

	// If event has a lifetime (duration 0 events are controlled externally, i.e. fires)
	if(m_iLifeTime > 0)
	{
		//if time is up!
		if(m_iLifeTime + m_iStartTime <= fwTimer::GetTimeInMilliseconds())
		{
			return true;
		}
	}

	return false;
}

bool CEventShocking::ReadyForShockingEvents(const CPed& rPed)
{
	// Not ready when ragdolling.
	if (rPed.GetUsingRagdoll())
	{
		return false;
	}

	// Not fully exposed to this event until after certain high priority tasks run.
	int iType = rPed.GetPedIntelligence()->GetCurrentEventType();
	switch(iType)
	{
		case EVENT_POTENTIAL_BE_WALKED_INTO:
		case EVENT_POTENTIAL_GET_RUN_OVER:
		case EVENT_IN_WATER:
		case EVENT_CLIMB_LADDER_ON_ROUTE:
		case EVENT_CLIMB_NAVMESH_ON_ROUTE:
		case EVENT_IN_AIR:
			return false;
		default:
			return true;
	}
}

bool CEventShocking::DoesEventInvolvePlayer(const CEntity* pSource, const CEntity* pOther)
{
	bool bInvolvesPlayer = IsEntityRelatedToPlayer(pSource);

	if (bInvolvesPlayer)
	{
		return true;
	}

	return IsEntityRelatedToPlayer(pOther);
}

bool CEventShocking::IsEntityRelatedToPlayer(const CEntity* pEntity)
{
	if (pEntity)
	{
		if (pEntity->GetIsTypeVehicle())
		{
			const CVehicle* pVehicle = static_cast<const CVehicle*>(pEntity);
			if (pVehicle->ContainsPlayer())
			{
				return true;
			}
		}
		else if (pEntity->GetIsTypePed())
		{
			const CPed* pPed = static_cast<const CPed*>(pEntity);
			if (pPed->IsPlayer())
			{
				return true;
			}
		}
	}
	return false;
}

bool CEventShocking::ShouldUseGunfireReactsWhenFleeing(const CEventShocking& rEvent, const CPed& rRespondingPed)
{
	const Tunables& tune = rEvent.GetTunables();
	const float fDistance = tune.m_DistanceToUseGunfireReactAndFleeAnimations;

	// A negative threshold is interpreted here as meaning never using the gunfire reactions.
	if (fDistance < 0.0f)
	{
		return false;
	}

	// Otherwise check against the distance.
	const ScalarV vThresh(fDistance * fDistance);
	const Vec3V vPed = rRespondingPed.GetTransform().GetPosition();
	const Vec3V vEvent = rEvent.GetReactPosition();
	if (IsLessThanAll(DistSquared(vPed, vEvent), vThresh))
	{
		return true;
	}

	return false;
}

bool CEventShocking::AffectsPedCore(CPed* pPed) const
{
	if (CShockingEventsManager::IsShockingEventSuppressed(*this))
	{
		return false;
	}

	if(pPed->IsInjured())
		return false;

	if(pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_DisableShockingEvents))
	{
		return false;
	}

	// Make sure the entities are still about
// TODO
//	if(!SanityCheckEntities())
//	{
//		return false;
//	}

	// TODO: Implement special cases from CShockingEventsManager::SpecialCaseIgnore().

	if(!m_bDisableSenseChecks && !CanBeSensedBy(*pPed))
	{
		return false;
	}

	CPedShockingEvent* pShocking = pPed->GetShockingEvent();
	if(!pShocking)
	{
		// Probably shouldn't even have gotten here if we didn't have a CPedShockingEvent object.
		return false;
	}

	bool bReadyForShockingEvents = ReadyForShockingEvents(*pPed);

	if (bReadyForShockingEvents)
	{
		// If we get past here, consider us as having had a chance to react to
		// the event. Note that we can't return false here if HasReactedToShockingEvent(),
		// because AffectsPedCore() is actually called more than once while handling
		// the event.

		if (!pShocking->HasReactedToShockingEvent(m_iId))
		{
			pShocking->SetShockingEventReactedTo(m_iId);
		}
	}

	// Check if this is considered a low priority event, and if so, if it should
	// be ignored. Note that we do this after calling SetShockingEventReactedTo() -
	// I think this may make some sense, in that when we turn
	// SET_IGNORE_LOW_PRIORITY_SHOCKING_EVENTS back off again, peds won't react
	// unless there is new stimuli.
	const Tunables& rTunables = GetTunables();
	if(rTunables.m_AllowIgnoreAsLowPriority)
	{
		if(pPed->GetPedIntelligence()->ShouldIgnoreLowPriShockingEvents())
		{
			// Ignored by CTaskAmbientClips or something similar.
			return false;
		}

		// Check SET_IGNORE_LOW_PRIORITY_SHOCKING_EVENTS.
		const CWanted* pWanted = CGameWorld::FindLocalPlayerWanted();
		if(pWanted && pWanted->m_IgnoreLowPriorityShockingEvents)
		{
			// SET_IGNORE_LOW_PRIORITY_SHOCKING_EVENTS is in use.
			return false;
		}
	}


	if (bReadyForShockingEvents)
	{
		//Check if an ambient reaction should be triggered.
		if(ShouldTriggerAmbientReaction(*pPed))
		{
			//Ensure the ambient clips are valid.
			CTaskAmbientClips* pTask = static_cast<CTaskAmbientClips *>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_AMBIENT_CLIPS));
			if(!pTask)
			{
				return false;
			}

			//Create an ambient event.
			CAmbientEvent ambientEvent(rTunables.m_AmbientEventType, GetEntityOrSourcePosition(), rTunables.m_AmbientEventLifetime, fwRandom::GetRandomNumberInRange(rTunables.m_MinTimeForAmbientReaction, rTunables.m_MaxTimeForAmbientReaction));

			//Add the ambient event.
			pTask->AddEvent(ambientEvent);

			//Nothing else to do.
			return false;
		}
	}

	return true;
}


void CEventShocking::Set(Vec3V_In posV, CEntity* pSource, CEntity* pOther)
{
	m_bUseSourcePosition = false;

	m_vEventPosition = posV;

	Assertf(!IsCloseAll(m_vEventPosition, Vec3V(V_ZERO), ScalarV(SMALL_FLOAT)), "A shocking event has been created at the origin.  This is a bug in the higher level code that created the shocking event.");
	m_fEventRadius = 0.0f;	// fRadius;
	m_nCountAdds = 0;

	Assert(!m_pEntitySource);
	m_pEntitySource = pSource;
	if(pSource)
	{
		m_bHasSourceEntity = true;
		Assertf(!IsCloseAll(pSource->GetTransform().GetPosition(), Vec3V(V_ZERO), ScalarV(SMALL_FLOAT)), 
			"An Entity of type %d (owned by %u ) is reporting its transform position to be the origin.  This will cause later asserts in AI code - why is it at the origin?  Art/Physics problem?", pSource->GetType(), pSource->GetOwnedBy());
	}

	Assert(!m_pEntityOther);
	m_pEntityOther = pOther;

	m_iStartTime = 0;	// nStartTime;
	m_iLifeTime = 0;	// nLifeTime;

	m_iPedGenBlockingAreaIndex = -1;
}


void CEventShocking::InitClone(CEventShocking& clone, bool cloneId) const
{
	clone.Set(m_vEventPosition, m_pEntitySource, m_pEntityOther);

	// May be needed, as CShockingEventsManager::ApplySpecialBehaviours() changes this.
	clone.m_iPedGenBlockingAreaIndex = m_iPedGenBlockingAreaIndex;

	clone.m_fEventRadius = m_fEventRadius;

	clone.m_iStartTime = m_iStartTime;
	clone.m_iLifeTime = m_iLifeTime;

	clone.m_Priority = m_Priority;

	if(cloneId)
	{
		clone.m_iId = m_iId;
	}

	clone.m_CreatedByScript = m_CreatedByScript;

	clone.m_bHasAppliedMotivationalImpacts = m_bHasAppliedMotivationalImpacts;
	clone.m_bUseGenericAudio = m_bUseGenericAudio;
	clone.m_bIgnoreIfLaw = m_bIgnoreIfLaw;
	clone.m_bCanBeMerged = m_bCanBeMerged;
	clone.m_bDisableSenseChecks = m_bDisableSenseChecks;

	clone.m_fAudioReactionRangeOverride = m_fAudioReactionRangeOverride;
	clone.m_fVisualReactionRangeOverride = m_fVisualReactionRangeOverride;

#if !__FINAL
	clone.m_EventType = m_EventType;
#endif
}


void CEventShocking::UpdateLifeTime(u32 nLifeTime, u32 nCurrentTime)
{
	if(nCurrentTime + nLifeTime > m_iStartTime + m_iLifeTime)
	{
		m_iStartTime = nCurrentTime;
		m_iLifeTime = nLifeTime;
	}
}


void CEventShocking::RemoveEntities()
{
	m_bHasSourceEntity = false;
	m_pEntitySource = NULL;
	m_pEntityOther = NULL;
}


Vec3V_Out CEventShocking::GetEntityOrSourcePosition() const 
{ 
	CEntity* pSrc = GetSourceEntity();
	if(pSrc && !m_bUseSourcePosition)
	{
		return pSrc->GetTransform().GetPosition();
	}
	else
	{
		return RCC_VEC3V(GetSourcePosition());
	}
}


bool CEventShocking::CanBeSensedBy(const CPed& ped, float rangeMultiplier, bool bIgnoreDir) const
{
	if (CEvent::CanEventBeIgnoredBecauseOfInteriorStatus(ped, m_pEntitySource, m_vEventPosition))
	{
		return false;
	}

	Vector3 vecEventPos;
	GetEntityOrSourcePosition(vecEventPos);

	// Potentially modify the perception distances if the player was not involved in this event.
	// The primary motivation here is that the player won't know why AI is reacting if they never saw the event.
	if (GetTunables().m_AIOnlyReactionRangeScaleFactor >= 0.0f && !DoesEventInvolvePlayer(m_pEntitySource, m_pEntityOther))
	{
		rangeMultiplier *= GetTunables().m_AIOnlyReactionRangeScaleFactor;
	}

	return CanSense(ped, GetTunables(), vecEventPos, true, true, rangeMultiplier, bIgnoreDir, m_fVisualReactionRangeOverride, m_fAudioReactionRangeOverride);
}


bool CEventShocking::CanSense(const CPed& ped, const Tunables& tunables,
		Vector3::Vector3Param vecEventPos, bool bAudio, bool bVisual, float fRangeMult, bool bIgnoreDir, float fVisualRangeOverride, float fAudioRangeOverride)
{
	//ped position
	Vector3 vecPedPos = VEC3V_TO_VECTOR3(ped.GetTransform().GetPosition());
	Vector3 vecPedFwd = VEC3V_TO_VECTOR3(ped.GetTransform().GetB());

	float fVisualReactionRange	= fVisualRangeOverride >= 0 ? fVisualRangeOverride * fRangeMult : tunables.m_VisualReactionRange * fRangeMult;
	float fVisualCopInVehicleReactionRange = fVisualRangeOverride >= 0 ? fVisualRangeOverride * fRangeMult : tunables.m_CopInVehicleVisualReactionRange * fRangeMult;
	float fAudioReactionRange	= fAudioRangeOverride >= 0 ? fAudioRangeOverride * fRangeMult : tunables.m_AudioReactionRange * fRangeMult;
	
	// Now do required calculations
	Vector3 vecDistance = vecEventPos;
	vecDistance -= vecPedPos;
	float fDistSqr = vecDistance.Mag2();

	if(fVisualReactionRange > 0.0f && bVisual)
	{
		if(fDistSqr <= fVisualReactionRange*fVisualReactionRange)
		{
			// check ped is facing towards the event
			if (bIgnoreDir || vecPedFwd.Dot(vecDistance) > 0.0f)
				return true;
		}
	}

	if(fAudioReactionRange > 0.0f && bAudio)
	{
		if(fDistSqr <= fAudioReactionRange*fAudioReactionRange)
			return true;
	}

	if ( ped.GetIsInVehicle() && ped.IsLawEnforcementPed() )
	{
		if(fDistSqr <= fVisualCopInVehicleReactionRange*fVisualCopInVehicleReactionRange)
		{
			return true;
		}
	}

	return false;
}

// Construct a task in response to this event
bool CEventShocking::CreateResponseTask(CPed* pPed, CEventResponse& response) const
{
	// Apply the motivation changes if they have not been applied.
	if (!m_bHasAppliedMotivationalImpacts)
	{
		ApplyMotivationalImpact(*pPed);
	}

	// Call the parent method.
	return CEventEditableResponse::CreateResponseTask(pPed, response);
}

void CEventShocking::ApplyMotivationalImpact(CPed& rPed) const
{
	// Grab the tunables.
	const Tunables& rTunables = GetTunables();

	// Cache off the ped's motivations.
	CPedMotivation& rPedMotivation = rPed.GetPedIntelligence()->GetPedMotivation();
	
	// Apply impacts.
	rPedMotivation.IncreasePedMotivation(CPedMotivation::FEAR_STATE, rTunables.m_PedFearImpact);

	// Note that impacts have been applied.
	m_bHasAppliedMotivationalImpacts = true;
}

bool CEventShocking::ShouldTriggerAmbientReaction(const CPed& rPed) const
{
	//Grab the tunables.
	const Tunables& rTunables = GetTunables();
	
	//Ensure the random check succeeded.
	float fRandom = fwRandom::GetRandomNumberInRange(0.0f, 1.0f);
	if (fRandom >= rTunables.m_TriggerAmbientReactionChances)
	{
		return false;
	}

	if (rPed.GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_USE_SCENARIO, true))
	{
		//Don't trigger an ambient event reaction if the ped is using a scenario.
		return false;
	}
	
	//Ensure the ambient clips are running.
	CTaskAmbientClips* pTask = static_cast<CTaskAmbientClips *>(rPed.GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_AMBIENT_CLIPS));
	if(pTask)
	{
		//there must be a valid reaction clip to this shocking event for the ped to use
		if (!pTask->HasReactionClip(this))
		{
			return false;	
		}
	}
	else
	{
		return false;
	}
	
	//Grab the ped position.
	Vec3V vPedPosition = rPed.GetTransform().GetPosition();

	//Get the event position.
	Vec3V vEventPosition = GetEntityOrSourcePosition();

	//Calculate the distance squared.
	ScalarV scDistSq = DistSquared(vPedPosition, vEventPosition);

	//Ensure the distance exceeds the minimum.
	ScalarV scMinDistSq = ScalarVFromF32(square(rTunables.m_MinDistanceForAmbientReaction));
	if(IsLessThanAll(scDistSq, scMinDistSq))
	{
		return false;
	}

	//Ensure the distance does not exceed the maximum.
	ScalarV scMaxDistSq = ScalarVFromF32(square(rTunables.m_MaxDistanceForAmbientReaction));
	if(IsGreaterThanAll(scDistSq, scMaxDistSq))
	{
		return false;
	}
	
	return true;
}

//  Returns the position of the event, or if this explosion was merged from previous explosions, the most recently exploded instance
Vec3V_Out CEventShocking::GetReactPosition() const {
	CEntity* pSrc = GetSourceEntity();
	if(pSrc && !m_bUseSourcePosition)
	{
		return pSrc->GetTransform().GetPosition();
	}
	else
	{
		if (m_bHasReactPosition)
		{
			return m_vReactPosition;
		}
		return m_vEventPosition;
	}
}

atHashWithStringNotFinal CEventShocking::GetShockingSpeechHash() const
{
	if (m_bUseGenericAudio)
	{
		return atHashWithStringNotFinal("GENERIC_SHOCKED_HIGH",0xB97E0BC9);
	}
	return GetTunables().m_ShockingSpeechHash;
}

atHashWithStringNotFinal CEventShocking::GetShockingFilmSpeechHash() const
{
	if (m_bUseGenericAudio)
	{
		return atHashWithStringNotFinal("GENERIC_SHOCKED_HIGH",0xB97E0BC9);
	}
	return GetTunables().m_ShockingFilmSpeechHash;
}

bool CEventShocking::operator==(const fwEvent& rOtherEvent) const
{
	if (rOtherEvent.GetEventType() == GetEventType())
	{
		const CEventShocking& rOtherShockingEvent = static_cast<const CEventShocking&>(rOtherEvent);

		CEntity* pEntity = GetSourceEntity();
		CEntity* pOtherEntity = rOtherShockingEvent.GetSourceEntity();

		// Tuning here looks from the perspective of this event not the larger, although it isn't done on a per event basis anyway really.
		const Tunables& tune = GetTunables();

		// The events are the same if they are of the same type and occur at similar points in space/time UNLESS the source entity is different.
		if ((pEntity && pOtherEntity && pEntity == pOtherEntity) || (!pEntity && !pOtherEntity))
		{
			return Max(m_iStartTime, rOtherShockingEvent.m_iStartTime) - Min(m_iStartTime, rOtherShockingEvent.m_iStartTime) < tune.m_DuplicateTimeCheck && 
				IsLessThanAll(DistSquared(rOtherShockingEvent.GetReactPosition(), GetReactPosition()), ScalarV(tune.m_DuplicateDistanceCheck * tune.m_DuplicateDistanceCheck));
		}
	}
	return false;
}

// Define the static tuning data members for shocking events.
#define DEFINE_SHOCKING_EVENT_TUNABLES(x, hash)								\
	x::Tunables x::sm_Tunables(#x, hash, "Shocking Events", "Event Tuning")
DEFINE_SHOCKING_EVENT_TUNABLES(CEventShockingBrokenGlass, 0x79d1299);
DEFINE_SHOCKING_EVENT_TUNABLES(CEventShockingCarChase, 0x214be074);
DEFINE_SHOCKING_EVENT_TUNABLES(CEventShockingCarCrash, 0x528fe49b);
DEFINE_SHOCKING_EVENT_TUNABLES(CEventShockingBicycleCrash, 0xdae8b6ab);
DEFINE_SHOCKING_EVENT_TUNABLES(CEventShockingCarPileUp, 0xe37d8dd8);
DEFINE_SHOCKING_EVENT_TUNABLES(CEventShockingCarOnCar, 0x38900aee);
DEFINE_SHOCKING_EVENT_TUNABLES(CEventShockingDangerousAnimal, 0x8b52a971);
DEFINE_SHOCKING_EVENT_TUNABLES(CEventShockingDeadBody, 0x6cebf863);
DEFINE_SHOCKING_EVENT_TUNABLES(CEventShockingDrivingOnPavement, 0xe9625db0);
DEFINE_SHOCKING_EVENT_TUNABLES(CEventShockingBicycleOnPavement, 0xcf33691b);
DEFINE_SHOCKING_EVENT_TUNABLES(CEventShockingEngineRevved, 0xcd95d642);
DEFINE_SHOCKING_EVENT_TUNABLES(CEventShockingExplosion, 0x0469d9c1);
DEFINE_SHOCKING_EVENT_TUNABLES(CEventShockingFire, 0xfcc1f6b9);
DEFINE_SHOCKING_EVENT_TUNABLES(CEventShockingGunFight, 0xdf425d62);
DEFINE_SHOCKING_EVENT_TUNABLES(CEventShockingGunshotFired, 0xdeca14cc);
DEFINE_SHOCKING_EVENT_TUNABLES(CEventShockingHelicopterOverhead, 0xda24134c);
DEFINE_SHOCKING_EVENT_TUNABLES(CEventShockingParachuterOverhead, 0x4771a035);
DEFINE_SHOCKING_EVENT_TUNABLES(CEventShockingHornSounded, 0xb5f9e520);
DEFINE_SHOCKING_EVENT_TUNABLES(CEventShockingInjuredPed, 0xcd39086e);
DEFINE_SHOCKING_EVENT_TUNABLES(CEventShockingInDangerousVehicle, 0xf138b835);
DEFINE_SHOCKING_EVENT_TUNABLES(CEventShockingMadDriver, 0x341ee36f);
DEFINE_SHOCKING_EVENT_TUNABLES(CEventShockingMadDriverExtreme, 0xfd20e346);
DEFINE_SHOCKING_EVENT_TUNABLES(CEventShockingMadDriverBicycle, 0x69d4a2cc);
DEFINE_SHOCKING_EVENT_TUNABLES(CEventShockingMugging, 0xf0424eaa);
DEFINE_SHOCKING_EVENT_TUNABLES(CEventShockingPedKnockedIntoByPlayer, 0xfb6401a6);
DEFINE_SHOCKING_EVENT_TUNABLES(CEventShockingPedRunOver, 0x8c36b30c);
DEFINE_SHOCKING_EVENT_TUNABLES(CEventShockingPedShot, 0xa02afa26);
DEFINE_SHOCKING_EVENT_TUNABLES(CEventShockingPlaneFlyby, 0x02c97da1);
DEFINE_SHOCKING_EVENT_TUNABLES(CEventShockingPotentialBlast, 0xefd736bb);
DEFINE_SHOCKING_EVENT_TUNABLES(CEventShockingPropertyDamage, 0xc3c23327);
DEFINE_SHOCKING_EVENT_TUNABLES(CEventShockingRunningPed, 0x32eef95d);
DEFINE_SHOCKING_EVENT_TUNABLES(CEventShockingRunningStampede, 0x93fb2958);
DEFINE_SHOCKING_EVENT_TUNABLES(CEventShockingSeenCarStolen, 0xb2b297b9);
DEFINE_SHOCKING_EVENT_TUNABLES(CEventShockingSeenConfrontation, 0xbf76f709);
DEFINE_SHOCKING_EVENT_TUNABLES(CEventShockingSeenGangFight, 0x1b719245);
DEFINE_SHOCKING_EVENT_TUNABLES(CEventShockingSeenInsult, 0x64fcf367);
DEFINE_SHOCKING_EVENT_TUNABLES(CEventShockingSeenMeleeAction, 0xf2b3030c);
DEFINE_SHOCKING_EVENT_TUNABLES(CEventShockingSeenNiceCar, 0x6458a0ad);
DEFINE_SHOCKING_EVENT_TUNABLES(CEventShockingSeenPedKilled, 0xffa14387);
DEFINE_SHOCKING_EVENT_TUNABLES(CEventShockingSiren, 0x99f92c54);
DEFINE_SHOCKING_EVENT_TUNABLES(CEventShockingCarAlarm, 0xeb0f15f7);
DEFINE_SHOCKING_EVENT_TUNABLES(CEventShockingStudioBomb, 0xc975ee6a);
DEFINE_SHOCKING_EVENT_TUNABLES(CEventShockingVehicleTowed, 0xbb1a793c);
DEFINE_SHOCKING_EVENT_TUNABLES(CEventShockingWeaponThreat, 0x631b11cf);
DEFINE_SHOCKING_EVENT_TUNABLES(CEventShockingVisibleWeapon, 0x9aa0ed3b);
DEFINE_SHOCKING_EVENT_TUNABLES(CEventShockingWeirdPed, 0xc3fc294d);
DEFINE_SHOCKING_EVENT_TUNABLES(CEventShockingWeirdPedApproaching, 0xf1e3888b);
DEFINE_SHOCKING_EVENT_TUNABLES(CEventShockingNonViolentWeaponAimedAt, 0xdbca8129);

//////////////////////////////////////////////////////////////////////////
//Shocking - broken glass
//////////////////////////////////////////////////////////////////////////

bool CEventShockingBrokenGlass::AffectsPedCore(CPed* pPed) const
{
	if (!CEventShocking::AffectsPedCore(pPed))
	{
		return false;
	}

	if (CTaskAgitated::ShouldIgnoreEvent(*pPed, *this))
	{
		return false;
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
//Shocking - Car Chase
//////////////////////////////////////////////////////////////////////////

// Return the driver if it exists otherwise return null.
CPed* CEventShockingCarChase::GetSourcePed() const
{
	if (m_pEntitySource && m_pEntitySource.Get()->GetIsTypeVehicle())
	{
		CVehicle* pVehicle = static_cast<CVehicle*>(m_pEntitySource.Get());
		return pVehicle->GetDriver();
	}
	return NULL;
}

// Return the driver if it exists otherwise return the vehicle.
CEntity* CEventShockingCarChase::GetSourceEntity() const
{
	if (m_pEntitySource && m_pEntitySource.Get()->GetIsTypeVehicle())
	{
		CVehicle* pVehicle = static_cast<CVehicle*>(m_pEntitySource.Get());
		CPed* pDriver = pVehicle->GetDriver();
		if (pDriver)
		{
			return pDriver;
		}
	}
	return m_pEntitySource;
}

//////////////////////////////////////////////////////////////////////////
//Shocking - Car Crash
//////////////////////////////////////////////////////////////////////////

bool CEventShockingCarCrash::CheckHasExpired() const
{
	if(CEventShocking::CheckHasExpired())
	{
		return true;
	}

	if(!m_pEntityOther && !m_CreatedByScript)
	{
		return true;
	}

	return false;
}

// Return the driver if it exists otherwise return null.
CPed* CEventShockingCarCrash::GetSourcePed() const
{
	if (m_pEntitySource && m_pEntitySource.Get()->GetIsTypeVehicle())
	{
		CVehicle* pVehicle = static_cast<CVehicle*>(m_pEntitySource.Get());
		return pVehicle->GetDriver();
	}
	return NULL;
}

// Return the driver if it exists otherwise return the vehicle.
CEntity* CEventShockingCarCrash::GetSourceEntity() const
{
	if (m_pEntitySource && m_pEntitySource.Get()->GetIsTypeVehicle())
	{
		CVehicle* pVehicle = static_cast<CVehicle*>(m_pEntitySource.Get());
		CPed* pDriver = pVehicle->GetDriver();
		if (pDriver)
		{
			return pDriver;
		}
	}
	return m_pEntitySource;
}

bool CEventShockingCarCrash::AffectsPedCore(CPed* pPed) const
{
	if(!CEventShocking::AffectsPedCore(pPed))
	{
		return false;
	}

	//Ensure we are not in the vehicle.
	if(m_pEntitySource && m_pEntitySource.Get()->GetIsTypeVehicle() && (pPed->GetVehiclePedInside() == m_pEntitySource.Get()))
	{
		return false;
	}

	//Ensure the ped is not already agitated.
	if(CTaskAgitated::ShouldIgnoreEvent(*pPed, *this))
	{
		return false;
	}

	CTaskComplexEvasiveStep* pTask = static_cast<CTaskComplexEvasiveStep*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_COMPLEX_EVASIVE_STEP));
	if (pTask)
	{
		pTask->InterruptWhenPossible();
	}

	return true;
}

atHashWithStringNotFinal CEventShockingCarCrash::GetShockingSpeechHash() const
{
	if (m_pEntitySource && m_pEntitySource->GetIsTypeVehicle())
	{
		const CVehicle* pVehicle = static_cast<const CVehicle*>(m_pEntitySource.Get());
		if (pVehicle->InheritsFromBike())
		{
			return atHashWithStringNotFinal("GENERIC_SHOCKED_HIGH",0xB97E0BC9);
		}
	}
	return CEventShocking::GetShockingSpeechHash();
}


//////////////////////////////////////////////////////////////////////////
//Shocking - Bike Crash
//////////////////////////////////////////////////////////////////////////

bool CEventShockingBicycleCrash::CheckHasExpired() const
{
	if(CEventShocking::CheckHasExpired())
	{
		return true;
	}

	if(!m_pEntityOther && !m_CreatedByScript)
	{
		return true;
	}

	return false;
}

// Return the driver if it exists otherwise return null.
CPed* CEventShockingBicycleCrash::GetSourcePed() const
{
	if (m_pEntitySource && m_pEntitySource.Get()->GetIsTypeVehicle())
	{
		CVehicle* pVehicle = static_cast<CVehicle*>(m_pEntitySource.Get());
		return pVehicle->GetDriver();
	}
	return NULL;
}

// Return the driver if it exists otherwise return the vehicle.
CEntity* CEventShockingBicycleCrash::GetSourceEntity() const
{
	if (m_pEntitySource && m_pEntitySource.Get()->GetIsTypeVehicle())
	{
		CVehicle* pVehicle = static_cast<CVehicle*>(m_pEntitySource.Get());
		CPed* pDriver = pVehicle->GetDriver();
		if (pDriver)
		{
			return pDriver;
		}
	}
	return m_pEntitySource;
}

bool CEventShockingBicycleCrash::AffectsPedCore(CPed* pPed) const
{
	if(!CEventShocking::AffectsPedCore(pPed))
	{
		return false;
	}

	//Ensure we are not in the vehicle.
	if(m_pEntitySource && m_pEntitySource.Get()->GetIsTypeVehicle() && (pPed->GetVehiclePedInside() == m_pEntitySource.Get()))
	{
		return false;
	}

	if(CTaskAgitated::ShouldIgnoreEvent(*pPed, *this))
	{
		return false;
	}

	CTaskComplexEvasiveStep* pTask = static_cast<CTaskComplexEvasiveStep*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_COMPLEX_EVASIVE_STEP));
	if (pTask)
	{
		pTask->InterruptWhenPossible();
	}

	return true;
}

atHashWithStringNotFinal CEventShockingBicycleCrash::GetShockingSpeechHash() const
{
	if (m_pEntitySource && m_pEntitySource->GetIsTypeVehicle())
	{
		const CVehicle* pVehicle = static_cast<const CVehicle*>(m_pEntitySource.Get());
		if (pVehicle->InheritsFromBike())
		{
			return atHashWithStringNotFinal("GENERIC_SHOCKED_MED",0x6FA7EF88);
		}
	}
	return CEventShocking::GetShockingSpeechHash();
}


//////////////////////////////////////////////////////////////////////////
//Shocking - Car Pile Up
//////////////////////////////////////////////////////////////////////////

bool CEventShockingCarPileUp::AffectsPedCore( CPed* pPed ) const
{
	if(!CEventShocking::AffectsPedCore(pPed))
	{
		return false;
	}

	if(pPed == GetSourcePed())
	{
		return false;
	}

	return true;
}

bool CEventShockingCarPileUp::CheckHasExpired() const
{
	if(CEventShocking::CheckHasExpired())
	{
		return true;
	}

	if(!m_pEntityOther && !m_CreatedByScript)
	{
		return true;
	}	

	return false;
}

atHashWithStringNotFinal CEventShockingCarPileUp::GetShockingSpeechHash() const
{
	if (m_pEntitySource && m_pEntitySource->GetIsTypeVehicle())
	{
		const CVehicle* pVehicle = static_cast<const CVehicle*>(m_pEntitySource.Get());
		if (pVehicle->InheritsFromBike())
		{
			return atHashWithStringNotFinal("GENERIC_SHOCKED_HIGH",0xB97E0BC9);
		}
	}
	return CEventShocking::GetShockingSpeechHash();
}

// Return the driver if it exists otherwise return null.
CPed* CEventShockingCarPileUp::GetSourcePed() const
{
	if (m_pEntitySource && m_pEntitySource.Get()->GetIsTypeVehicle())
	{
		CVehicle* pVehicle = static_cast<CVehicle*>(m_pEntitySource.Get());
		return pVehicle->GetDriver();
	}
	return NULL;
}

// Return the driver if it exists otherwise return the vehicle.
CEntity* CEventShockingCarPileUp::GetSourceEntity() const
{
	if (m_pEntitySource && m_pEntitySource.Get()->GetIsTypeVehicle())
	{
		CVehicle* pVehicle = static_cast<CVehicle*>(m_pEntitySource.Get());
		CPed* pDriver = pVehicle->GetDriver();
		if (pDriver)
		{
			return pDriver;
		}
	}
	return m_pEntitySource;
}

//////////////////////////////////////////////////////////////////////////
//Shocking - Vehicle is on top of another vehicle
//////////////////////////////////////////////////////////////////////////
bool CEventShockingCarOnCar::AffectsPedCore(CPed* pPed) const
{	
	if (!CEventShocking::AffectsPedCore(pPed) )
	{
		return false;
	}

	//We don't want to respond to the event if my vehicle is the source.
	CVehicle* pVehicle = pPed->GetMyVehicle();
	CVehicle* pBottomVehicle = static_cast<CVehicle*>(m_pEntityOther.Get());
	if(pVehicle && pBottomVehicle && pBottomVehicle != pVehicle)
	{
		return false;
	}

	return true;
}

atHashWithStringNotFinal CEventShockingCarOnCar::GetShockingSpeechHash() const
{
	if (m_pEntitySource && m_pEntitySource->GetIsTypeVehicle())
	{
		const CVehicle* pVehicle = static_cast<const CVehicle*>(m_pEntitySource.Get());
		if (pVehicle->InheritsFromBike())
		{
			return atHashWithStringNotFinal("GENERIC_SHOCKED_HIGH",0xB97E0BC9);
		}
	}
	return CEventShocking::GetShockingSpeechHash();
}

//////////////////////////////////////////////////////////////////////////
//Shocking - dangerous animal
//////////////////////////////////////////////////////////////////////////
bool CEventShockingDangerousAnimal::CheckHasExpired() const
{
	if(CEventShocking::CheckHasExpired())
	{
		return true;
	}

	if(m_pEntitySource.Get()->GetIsTypePed() && static_cast<CPed*>(m_pEntitySource.Get())->IsInjured())
	{
		return true;
	}

	return false;
}


//////////////////////////////////////////////////////////////////////////
//Shocking - Explosion
//////////////////////////////////////////////////////////////////////////
bool CEventShockingExplosion::AffectsPedCore(CPed* pPed) const
{	
	if (!CEventShocking::AffectsPedCore(pPed) )
	{
		return false;
	}

	if (pPed == GetSourcePed())
	{
		return false;
	}

	if(pPed->GetPedType() == PEDTYPE_COP)
	{
		if(CTaskShockingPoliceInvestigate::GetNumberTaskUses() > 1)
		{
			return false;
		}
	}

	if(m_bIgnoreIfLaw && pPed->ShouldBehaveLikeLaw())
	{
		return false;
	}

	return true;
}

// Return the position of the explosion not the position of the source entity, who could be
// quite far away from the actual explosion.
Vec3V_Out CEventShockingExplosion::GetEntityOrSourcePosition() const
{
	return m_vEventPosition;
}

CPed* CEventShockingExplosion::GetSourcePed() const
{
	if(m_pEntitySource && m_pEntitySource->GetIsTypeVehicle())
	{
		return (static_cast<CVehicle *>(m_pEntitySource.Get())->GetDriver());
	}

	return CEventShocking::GetSourcePed();
}

//////////////////////////////////////////////////////////////////////////
//Shocking - Fire
//////////////////////////////////////////////////////////////////////////

CPed* CEventShockingFire::GetSourcePed() const
{
	if(m_pEntitySource && m_pEntitySource->GetIsTypeVehicle())
	{
		return (static_cast<CVehicle *>(m_pEntitySource.Get())->GetDriver());
	}

	return CEventShocking::GetSourcePed();
}

bool CEventShockingFire::AffectsPedCore(CPed* pPed) const
{
	//Ensure the base class version is valid.
	if(!CEventShocking::AffectsPedCore(pPed))
	{
		return false;
	}

	//Ensure the ped is not the source.
	if(pPed == GetSourcePed())
	{
		return false;
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
//Shocking - Horn Sounded
//////////////////////////////////////////////////////////////////////////

bool CEventShockingHornSounded::AffectsPedCore(CPed* pPed) const
{
	if(!CEventShocking::AffectsPedCore(pPed))
	{
		return false;
	}

	if(pPed == GetSourcePed())
	{
		return false;
	}

	//the source entity of a horn is always a vehicle, so we need to
	//check against that here
	if (pPed->GetMyVehicle() && pPed->GetMyVehicle() == GetSourceEntity())
	{
		return false;
	}

	// If we have an ambient friend and we are dependent to them, ignore this event.
	if (pPed->GetTaskData().GetIsFlagSet(CTaskFlags::DependentAmbientFriend) && pPed->GetPedIntelligence()->GetAmbientFriend())
	{
		return false;
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
//Shocking - In dangerous vehicle.
//////////////////////////////////////////////////////////////////////////

CEventEditableResponse* CEventShockingInDangerousVehicle::CloneEditable() const
{	
	CEventShocking* dest = rage_new CEventShockingInDangerousVehicle;
	InitClone(*dest);
	return dest;
}

bool CEventShockingInDangerousVehicle::CheckHasExpired() const
{
	if(CEventShocking::CheckHasExpired())
	{
		return true;
	}

	// If this event was created by script, we probably don't want to require the conditions below,
	// regardless of whether the event was created on an entity or not.
	if(m_CreatedByScript)
	{
		return false;
	}

	if(m_pEntitySource && m_pEntitySource->GetIsTypeVehicle())
	{
		CVehicle* pVehicle = (CVehicle*)m_pEntitySource.Get();

		// Check if the source is still driving a dangerous vehicle that has a player driver.
		if (pVehicle->GetDriver() && pVehicle->GetDriver()->IsAPlayerPed())
		{
			// Check if the source is still dangerous.
			if (pVehicle->GetIsConsideredDangerousForShockingEvents())
			{
				return false;
			}
		}
	}

	return true;
}

// Return the driver.
CPed* CEventShockingInDangerousVehicle::GetSourcePed() const
{
	if (m_pEntitySource)
	{
		if (m_pEntitySource.Get()->GetIsTypeVehicle())
		{
			CVehicle* pVehicle = static_cast<CVehicle*>(m_pEntitySource.Get());
			return pVehicle->GetDriver();
		}
	}
	return NULL;
}

//////////////////////////////////////////////////////////////////////////
//Shocking - Injured Ped
//////////////////////////////////////////////////////////////////////////
bool CEventShockingInjuredPed::AffectsPedCore(CPed* pPed) const
{	
	if (!CEventShocking::AffectsPedCore(pPed) )
	{
		return false;
	}

	// Don't respond to the event if you are the source.
	if (GetSourcePed() == pPed)
	{
		return false;
	}

	if(pPed->GetPedType() == PEDTYPE_COP)
	{
		if(CTaskShockingPoliceInvestigate::GetNumberTaskUses() > 1)
		{
			return false;
		}
	}

	//Ensure the ped is not already agitated.
	if(CTaskAgitated::ShouldIgnoreEvent(*pPed, *this))
	{
		return false;
	}

	return true;
}

// Return the source if ped, or the driver if the source is a vehicle.  Otherwise return null.
CPed* CEventShockingInjuredPed::GetSourcePed() const
{
	if (m_pEntitySource)
	{
		if (m_pEntitySource.Get()->GetIsTypePed())
		{
			return static_cast<CPed*>(m_pEntitySource.Get());
		}
		if (m_pEntitySource.Get()->GetIsTypeVehicle())
		{
			CVehicle* pVehicle = static_cast<CVehicle*>(m_pEntitySource.Get());
			return pVehicle->GetDriver();
		}
	}
	return NULL;
}

// Return the driver if it exists otherwise return the vehicle.
CEntity* CEventShockingInjuredPed::GetSourceEntity() const
{
	if (m_pEntitySource && m_pEntitySource.Get()->GetIsTypeVehicle())
	{
		CVehicle* pVehicle = static_cast<CVehicle*>(m_pEntitySource.Get());
		CPed* pDriver = pVehicle->GetDriver();
		if (pDriver)
		{
			return pDriver;
		}
	}
	return m_pEntitySource;
}

CPed* CEventShockingInjuredPed::GetTargetPed() const
{
	CEntity* pTargetEntity = GetOtherEntity();
	if(pTargetEntity && pTargetEntity->GetType() == ENTITY_TYPE_PED)
	{
		return static_cast<CPed*>(pTargetEntity);
	}

	return NULL;
}

//////////////////////////////////////////////////////////////////////////
//Shocking - Dead Body
//////////////////////////////////////////////////////////////////////////

bool CEventShockingDeadBody::CheckHasExpired() const
{
	if(CEventShocking::CheckHasExpired())
	{
		return true;
	}

	// If this event was created by script, we probably don't want to require the conditions below,
	// regardless of whether the event was created on an entity or not.
	if(m_CreatedByScript)
	{
		return false;
	}

	if(!m_pEntitySource || !m_pEntitySource->GetIsTypePed())
	{
		return true;
	}

	CPed* pPedSource = static_cast<CPed*>(m_pEntitySource.Get());
	if(!pPedSource->IsInjured() && !pPedSource->GetPedConfigFlag(CPED_CONFIG_FLAG_PedGeneratesDeadBodyEvents))
	{
		return true;
	}

	return false;
}

bool CEventShockingDeadBody::AffectsPedCore(CPed* pPed) const
{	
	if (!CEventShocking::AffectsPedCore(pPed) )
	{
		return false;
	}

	if(pPed->GetPedType() == PEDTYPE_COP)
	{
		if(m_pEntitySource && m_pEntitySource->GetIsTypePed())
		{
			CPed* pPedSource = static_cast<CPed*>(m_pEntitySource.Get());
			if(pPedSource->HasBeenDamagedByEntity(pPed))
			{
				return false;
			}

			if(pPedSource->GetPedConfigFlag(CPED_CONFIG_FLAG_DisablePoliceInvestigatingBody))
			{
				return false;
			}
		}
	
		if(CTaskShockingPoliceInvestigate::GetNumberTaskUses() > 1)
		{
			return false;
		}
	}

	if(pPed == GetSourcePed())
	{
		return false;
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
//Shocking - Driving on Pavement
//////////////////////////////////////////////////////////////////////////

bool CEventShockingDrivingOnPavement::AffectsPedCore( CPed* pPed ) const
{
	if(!CEventShocking::AffectsPedCore(pPed))
	{
		return false;
	}

	if(pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_DisableShockingDrivingOnPavementEvents))
	{
		return false;
	}

	if(pPed == GetSourcePed())
	{
		return false;
	}

	if(CTaskAgitated::ShouldIgnoreEvent(*pPed, *this))
	{
		return false;
	}

	return true;
}

CPed* CEventShockingDrivingOnPavement::GetSourcePed() const
{
	if(m_pEntitySource && m_pEntitySource->GetIsTypeVehicle())
	{
		return (static_cast<CVehicle *>(m_pEntitySource.Get())->GetDriver());
	}

	return CEventShocking::GetSourcePed();
}

//////////////////////////////////////////////////////////////////////////
//Shocking - Bicycle on Pavement
//////////////////////////////////////////////////////////////////////////

bool CEventShockingBicycleOnPavement::AffectsPedCore(CPed* pPed) const
{
	if(!CEventShocking::AffectsPedCore(pPed))
	{
		return false;
	}

	if(CTaskAgitated::ShouldIgnoreEvent(*pPed, *this))
	{
		return false;
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
//Shocking - Gun Fight
//////////////////////////////////////////////////////////////////////////

bool CEventShockingGunFight::CheckHasExpired() const
{
	if(CEventShocking::CheckHasExpired())
	{
		return true;
	}

	CEntity * pEntitySource = m_pEntitySource;
	if(pEntitySource && pEntitySource->GetIsTypePed())
	{
		CPed* pPedSource = static_cast<CPed *>(pEntitySource);
		if(pPedSource->GetIsDeadOrDying())
		{
			return true;
		}

		if(pPedSource->IsInjured())
		{
			return true;
		}
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
//Shocking - Gun Shot Fired
//////////////////////////////////////////////////////////////////////////

bool CEventShockingGunshotFired::CheckHasExpired() const
{
	if(CEventShocking::CheckHasExpired())
	{
		return true;
	}

	CEntity * pEntitySource = m_pEntitySource;
	if(pEntitySource && pEntitySource->GetIsTypePed())
	{
		CPed* pPedSource = static_cast<CPed *>(pEntitySource);
		if(pPedSource->GetIsDeadOrDying())
		{
			return true;
		}
		
		if(pPedSource->IsInjured())
		{
			return true;
		}
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
//Shocking - Helicopter Overhead
//////////////////////////////////////////////////////////////////////////

bool CEventShockingHelicopterOverhead::CheckHasExpired() const
{
	if(CEventShocking::CheckHasExpired())
	{
		return true;
	}

	// If this event was created by script, we probably don't want to require the conditions below,
	// regardless of whether the event was created on an entity or not.
	if(m_CreatedByScript)
	{
		return false;
	}

	if(!m_pEntitySource || !m_pEntitySource->GetIsTypeVehicle() || static_cast<CVehicle*>(m_pEntitySource.Get())->GetVehicleType() != VEHICLE_TYPE_HELI)
	{
		return true;
	}

	CHeli* pHeli = static_cast<CHeli*>(m_pEntitySource.Get());
	if(pHeli->GetMainRotorSpeed() <= 0.1f)
	{
		return true;
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
//Shocking - Mad driver
//////////////////////////////////////////////////////////////////////////
bool CEventShockingMadDriver::AffectsPedCore(CPed* pPed) const
{	
	if (!CEventShocking::AffectsPedCore(pPed) )
	{
		return false;
	}

	//We don't want to respond to the event if my vehicle is the source.
	CVehicle* pVehicle = pPed->GetMyVehicle();
	CPed* pMadDriver = static_cast<CPed*>(m_pEntitySource.Get());
	if(pVehicle && pMadDriver && pMadDriver->GetMyVehicle() && pMadDriver->GetMyVehicle() == pVehicle)
	{
		return false;
	}
	//We don't want to respond to the event if we are attached to the vehicle.
	else if(pPed->GetAttachParent() && pMadDriver && pMadDriver->GetMyVehicle() && (pPed->GetAttachParent() == pMadDriver->GetMyVehicle()))
	{
		return false;
	}

	if(CTaskAgitated::ShouldIgnoreEvent(*pPed, *this))
	{
		return false;
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
//Shocking - Mad driver extreme
//////////////////////////////////////////////////////////////////////////
bool CEventShockingMadDriverExtreme::AffectsPedCore(CPed* pPed) const
{	
	if (!CEventShocking::AffectsPedCore(pPed) )
	{
		return false;
	}

	//We don't want to respond to the event if my vehicle is the source.
	CVehicle* pVehicle = pPed->GetMyVehicle();
	CPed* pMadDriver = static_cast<CPed*>(m_pEntitySource.Get());
	if(pVehicle && pMadDriver && pMadDriver->GetMyVehicle() && pMadDriver->GetMyVehicle() == pVehicle)
	{
		return false;
	}
	//We don't want to respond to the event if we are attached to the vehicle.
	else if(pPed->GetAttachParent() && pMadDriver && pMadDriver->GetMyVehicle() && (pPed->GetAttachParent() == pMadDriver->GetMyVehicle()))
	{
		return false;
	}

	if(CTaskAgitated::ShouldIgnoreEvent(*pPed, *this))
	{
		return false;
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
//Shocking - ped knocked into by player
//////////////////////////////////////////////////////////////////////////
bool CEventShockingPedKnockedIntoByPlayer::AffectsPedCore(CPed* pPed) const
{
	if (!CEventShocking::AffectsPedCore(pPed) )
	{
		return false;
	}

	if(CTaskAgitated::ShouldIgnoreEvent(*pPed, *this))
	{
		return false;
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
//Shocking - Ped run over
//////////////////////////////////////////////////////////////////////////
bool CEventShockingPedRunOver::AffectsPedCore(CPed* pPed) const
{	
	if (!CEventShocking::AffectsPedCore(pPed) )
	{
		return false;
	}

	const CPed* pSourcePed = GetSourcePed();
	//Ensure we are not the source.
	if(pPed == pSourcePed)
	{
		return false;
	}

	//Ensure we were not run over.
	if(pPed == m_pEntityOther.Get())
	{
		return false;
	}

	if(CTaskAgitated::ShouldIgnoreEvent(*pPed, *this))
	{
		return false;
	}

	if (pSourcePed && pSourcePed->IsAPlayerPed())
	{
		u32 scenarioType = CPed::GetScenarioType(*pPed, true);
		if (scenarioType != Scenario_Invalid)
		{
			const CScenarioInfo* pScenarioInfo = CScenarioManager::GetScenarioInfo(scenarioType);
			if (pScenarioInfo && pScenarioInfo->GetIsFlagSet(CScenarioInfoFlags::IgnoreShockingPlayerRunOverEvent))
			{
				return false;
			}
		}
	}

	return true;
}

// Return the source if ped, or the driver if the source is a vehicle.  Otherwise return null.
CPed* CEventShockingPedRunOver::GetSourcePed() const
{
	if (m_pEntitySource)
	{
		if (m_pEntitySource.Get()->GetIsTypePed())
		{
			return static_cast<CPed*>(m_pEntitySource.Get());
		}
		if (m_pEntitySource.Get()->GetIsTypeVehicle())
		{
			CVehicle* pVehicle = static_cast<CVehicle*>(m_pEntitySource.Get());
			return pVehicle->GetDriver();
		}
	}
	return NULL;
}

// Return the driver if it exists otherwise return the vehicle.
CEntity* CEventShockingPedRunOver::GetSourceEntity() const
{
	if (m_pEntitySource && m_pEntitySource.Get()->GetIsTypeVehicle())
	{
		CVehicle* pVehicle = static_cast<CVehicle*>(m_pEntitySource.Get());
		CPed* pDriver = pVehicle->GetDriver();
		if (pDriver)
		{
			return pDriver;
		}
	}
	return m_pEntitySource;
}

//////////////////////////////////////////////////////////////////////////
//Shocking - Ped Shot
//////////////////////////////////////////////////////////////////////////

Vec3V_Out CEventShockingPedShot::GetEntityOrSourcePosition() const
{
	//Check if the other entity is valid.
	const CEntity* pOtherEntity = GetOtherEntity();
	if(pOtherEntity)
	{
		return pOtherEntity->GetTransform().GetPosition();
	}

	return (CEventShocking::GetEntityOrSourcePosition());
}

//////////////////////////////////////////////////////////////////////////
//Shocking - Plane FlyBy
//////////////////////////////////////////////////////////////////////////

bool CEventShockingPlaneFlyby::CheckHasExpired() const
{
	if (CEventShocking::CheckHasExpired())
	{
		return true;
	}

	if (m_CreatedByScript)
	{
		return false;
	}

	if(!m_pEntitySource || !m_pEntitySource->GetIsTypeVehicle() || static_cast<CVehicle*>(m_pEntitySource.Get())->GetVehicleType() != VEHICLE_TYPE_PLANE)
	{
		return true;
	}

	CPlane* pPlane = static_cast<CPlane*>(m_pEntitySource.Get());

	if(pPlane->IsEngineOn() && pPlane->IsInAir() && pPlane->ShouldGenerateEngineShockingEvents())
	{
		return false;
	}
	else if(pPlane->IsPlayerDrivingOnGround())
	{
		return false;
	}
	else
	{
		return true;
	}
}



//////////////////////////////////////////////////////////////////////////
//Shocking - Parachuter Overhead
//////////////////////////////////////////////////////////////////////////

bool CEventShockingParachuterOverhead::operator==(const fwEvent& rOtherEvent) const
{
	if (rOtherEvent.GetEventType() == GetEventType())
	{
		const CEventShocking& rOtherShockingEvent = static_cast<const CEventShocking&>(rOtherEvent);

		// Tuning here looks from the perspective of this event not the larger, although it isn't done on a per event basis anyway really.
		const Tunables& tune = GetTunables();

		// Because there can be lots of parachuters together in the same space in MP games, we don't require the source entities to be the same.
		// This avoids rare crashes with the event pool overflowing.
		return Max(m_iStartTime, rOtherShockingEvent.GetStartTime()) - Min(m_iStartTime, rOtherShockingEvent.GetStartTime()) < tune.m_DuplicateTimeCheck && 
				IsLessThanAll(DistSquared(rOtherShockingEvent.GetReactPosition(), GetReactPosition()), ScalarV(tune.m_DuplicateDistanceCheck * tune.m_DuplicateDistanceCheck));
	}
	return false;
}

bool CEventShockingParachuterOverhead::CheckHasExpired() const
{
	if(CEventShocking::CheckHasExpired())
	{
		return true;
	}

	// If this event was created by script, we probably don't want to require the conditions below,
	// regardless of whether the event was created on an entity or not.
	if(m_CreatedByScript)
	{
		return false;
	}

	if(!m_pEntitySource || !m_pEntitySource->GetIsTypePed())
	{
		return true;
	}
	
	CPed* pPed = static_cast<CPed*>(m_pEntitySource.Get());

	if(!pPed->GetIsParachuting() && !pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_FALL, true))
	{
		return true;
	}

	return false;
}

atHashWithStringNotFinal CEventShockingParachuterOverhead::GetShockingSpeechHash() const
{
	// If parachuting use the tunable.
	if (m_pEntitySource && m_pEntitySource->GetIsTypePed())
	{
		if (static_cast<CPed*>(m_pEntitySource.Get())->GetIsParachuting())
		{
			return CEventShocking::GetShockingSpeechHash();
		}
	}

	// If not parachuting, use the normal shocked hash.
	return atHashWithStringNotFinal("GENERIC_SHOCKED_MED",0x6FA7EF88);
}

//////////////////////////////////////////////////////////////////////////
//Shocking - Property damage
//////////////////////////////////////////////////////////////////////////

bool CEventShockingPropertyDamage::AffectsPedCore(CPed* pPed) const
{	
	if(!CEventShocking::AffectsPedCore(pPed))
	{
		return false;
	}

	if(CTaskAgitated::ShouldIgnoreEvent(*pPed, *this))
	{
		return false;
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
//Shocking - Running Ped
//////////////////////////////////////////////////////////////////////////

bool CEventShockingRunningPed::CheckHasExpired() const
{
	if(CEventShocking::CheckHasExpired())
	{
		return true;
	}

	// If this event was created by script, we probably don't want to require the conditions below,
	// regardless of whether the event was created on an entity or not.
	if(m_CreatedByScript)
	{
		return false;
	}

	if(!m_pEntitySource || !m_pEntitySource->GetIsTypePed())
	{
		return true;
	}
	//is source still running?
	else
	{
		CPed* pPed = (CPed*)m_pEntitySource.Get();
		if(!pPed->GetMotionData()->GetIsRunning() && !pPed->GetMotionData()->GetIsSprinting())
			return true;
	}

	return false;
}


//////////////////////////////////////////////////////////////////////////
//Shocking - Running Stampede
//////////////////////////////////////////////////////////////////////////

bool CEventShockingRunningStampede::CheckHasExpired() const
{
	if(CEventShocking::CheckHasExpired())
	{
		return true;
	}

	// If this event was created by script, we probably don't want to require the conditions below,
	// regardless of whether the event was created on an entity or not.
	if(m_CreatedByScript)
	{
		return false;
	}

	if(!m_pEntitySource || !m_pEntitySource->GetIsTypePed())
	{
		return true;
	}
	//is source still running?
	else
	{
		CPed* pPed = (CPed*)m_pEntitySource.Get();
		if(!pPed->GetMotionData()->GetIsRunning() && !pPed->GetMotionData()->GetIsSprinting())
			return true;
	}

	return false;
}


bool CEventShockingRunningStampede::ProcessInGlobalGroup()
{
	//Call the base class version.
	if(!CEventShocking::ProcessInGlobalGroup())
	{
		return false;
	}
	
	//Clear the adds.
	//This will allow the position to be re-calculated correctly every frame.
	m_nCountAdds = 0;
	
	return true;
}


//////////////////////////////////////////////////////////////////////////
//Shocking - Vehicle is being stolen
//////////////////////////////////////////////////////////////////////////
bool CEventShockingSeenCarStolen::AffectsPedCore(CPed* pPed) const
{	

	if (!CEventShocking::AffectsPedCore(pPed) )
	{
		return false;
	}

	//We don't want to respond to the event if there wasn't a thief or if the thief was myself
	if(!m_pEntitySource || pPed == m_pEntitySource)
	{
		return false;
	}

	// Disable looking at stolen cars for mission characters - these are controlled by script
	if( pPed->PopTypeIsMission() )
	{
		return false;
	}

	// Always react to your own vehicle being stolen
	if (m_pEntityOther && pPed->GetMyVehicle() == m_pEntityOther)
	{
		//Ensure we are not already agitated.
		if(CTaskAgitated::ShouldIgnoreEvent(*pPed, *this))
		{
			return false;
		}

		return true;
	}

	// HACK:  Medics and Fire men only care about their own car getting stolen, so if they haven't returned true above they will ignore the event.
	// They also care about other emergency services and law enforcement vehicles, as it looks pretty strange for them not to react to a firetruck getting stolen
	// next to them.
	if (pPed->GetPedType() == PEDTYPE_MEDIC || pPed->GetPedType() == PEDTYPE_FIRE)
	{
		bool bCanIgnore = true;
		if (m_pEntityOther && m_pEntityOther->GetIsTypeVehicle())
		{
			const CVehicle* pVehicle = static_cast<CVehicle*>(m_pEntityOther.Get());
			const CVehicleModelInfo* pModelInfo = pVehicle->GetVehicleModelInfo();
			if (pModelInfo && (pModelInfo->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_EMERGENCY_SERVICE) || pModelInfo->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_LAW_ENFORCEMENT)))
			{
				bCanIgnore = false;
			}
		}

		if (bCanIgnore)
		{
			return false;
		}
	}

	// Let this event go through to scenario peds nearby even if it isn't "broken into".
	if (RequiresNearbyAlertFlagOnPed())
	{
		if (m_pEntityOther)
		{
			Assertf(m_pEntityOther->GetIsTypeVehicle(), "Other entity in CEventShockingSeenCarStolen was not a vehicle...");
			CVehicle* pVehicle = static_cast<CVehicle*>(m_pEntityOther.Get());
			
			// Apply this logic for only cargen created vehicles.
			if (pVehicle->m_CarGenThatCreatedUs != -1)
			{
				CScenarioPoint* pPedScenarioPoint = NULL;
				CTask* pTask = pPed->GetPedIntelligence()->GetTaskActive();
				if (pTask)
				{
					pPedScenarioPoint = pTask->GetScenarioPoint();
				}

				if (pPedScenarioPoint)
				{
					if (pPedScenarioPoint->IsFlagSet(CScenarioPointFlags::EventsInRadiusTriggerDisputes) || 
						pPedScenarioPoint->IsFlagSet(CScenarioPointFlags::EventsInRadiusTriggerThreatResponse))
					{
						return true;
					}
				}
			}
		}
		return false;
	}
	return true; 
}

CPed* CEventShockingSeenCarStolen::GetPlayerCriminal() const
{
	//Ensure the source entity is valid.
	CEntity* pEntity = GetSourceEntity();
	if(!pEntity)
	{
		return NULL;
	}

	//Ensure the entity is a ped.
	if(!pEntity->GetIsTypePed())
	{
		return NULL;
	}

	//Ensure the ped is a player.
	CPed* pPed = static_cast<CPed *>(pEntity);
	if(!pPed->IsPlayer())
	{
		return NULL;
	}

	return pPed;
}


//////////////////////////////////////////////////////////////////////////
//Shocking - Seen Confrontation
//////////////////////////////////////////////////////////////////////////

bool CEventShockingSeenConfrontation::CheckHasExpired() const
{
	if(CEventShocking::CheckHasExpired())
	{
		return true;
	}	

	return false;
}

//////////////////////////////////////////////////////////////////////////
//Shocking - Seen Gang Fight
//////////////////////////////////////////////////////////////////////////

bool CEventShockingSeenGangFight::CheckHasExpired() const
{
	if(CEventShocking::CheckHasExpired())
	{
		return true;
	}

	if(!m_pEntityOther && !m_CreatedByScript)
	{
		return true;
	}	

	return false;
}

//////////////////////////////////////////////////////////////////////////
//Shocking - Seen Insult
//////////////////////////////////////////////////////////////////////////

bool CEventShockingSeenInsult::CheckHasExpired() const
{
	if(CEventShocking::CheckHasExpired())
	{
		return true;
	}	

	return false;
}

//////////////////////////////////////////////////////////////////////////
//Shocking - Seen melee action
//////////////////////////////////////////////////////////////////////////

bool CEventShockingSeenMeleeAction::AffectsPedCore(CPed* pPed) const
{
	//Ensure the base class is valid.
	if(!CEventShocking::AffectsPedCore(pPed))
	{
		return false;
	}

	//Ensure the event should not be ignored.
	if(CTaskAgitated::ShouldIgnoreEvent(*pPed, *this))
	{
		return false;
	}

	return true;
}

CPed* CEventShockingSeenMeleeAction::GetTargetPed() const
{
	CEntity* pTargetEntity = GetOtherEntity();
	if(pTargetEntity && pTargetEntity->GetType() == ENTITY_TYPE_PED)
	{
		return static_cast<CPed*>(pTargetEntity);
	}

	return NULL;
}

bool CEventShockingSeenMeleeAction::IsExposedToPed(CPed *pPed) const
{
	//! HACK. Need to add this to all non random peds aswell. In CShockingEventScanner::ScanForShockingEvents, we only add shocking events to ambient peds, so 
	//! this corrects that.
	//! Note: Only do this for MP games atm. May want to remove this on future projects (or create a seperate non shocking event that is handles differently).
	return pPed->PopTypeIsMission() && NetworkInterface::IsGameInProgress();
}

// Construct a task in response to this event
bool CEventShockingSeenMeleeAction::CreateResponseTask(CPed* pPed, CEventResponse& response) const
{
	// NOTE[egarcia]: HACK. Added to avoid changing the current relationships between multiplayer peds. 
	// It would be better get rid of this flag and review that relationships whenever is safe to do that.
	if (pPed && pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_ForceThreatResponseToNonFriendToFriendMeleeActions))
	{
		const CPed* pSourcePed = GetSourcePed();
		const CPed* pTargetPed = GetTargetPed();
		if (pSourcePed && pTargetPed && !pPed->GetPedIntelligence()->IsFriendlyWith(*pSourcePed) && pPed->GetPedIntelligence()->IsFriendlyWith(*pTargetPed))
		{
			CEventResponseFactory::CreateEventResponse(pPed, this, CTaskTypes::TASK_THREAT_RESPONSE, response);
			return response.GetEventResponse(CEventResponse::EventResponse) != NULL;
		}
	}

	// Call the parent method.
	return CEventShocking::CreateResponseTask(pPed, response);
}

//////////////////////////////////////////////////////////////////////////
//Shocking - Seen Nice Car
//////////////////////////////////////////////////////////////////////////
bool CEventShockingSeenNiceCar::AffectsPedCore(CPed* pPed) const
{
	if (!CEventShocking::AffectsPedCore(pPed) )
	{
		return false;
	}

	//don't comment on the vehicle you enter
	if ( pPed->GetIsEnteringVehicle()  )
	{
		return false;
	}

	//don't comment on the vehicle you exit
	if ( pPed->IsExitingVehicle() )
	{
		return false;
	}
	
	CVehicle* pVehicle = pPed->GetMyVehicle();
	CVehicle* pNiceVehicle = static_cast<CVehicle*>(CEventShocking::GetSourceEntity());

	if ( !pNiceVehicle || pNiceVehicle->GetVehicleType() != VEHICLE_TYPE_CAR )
	{
		return false;
	}

	// Heavily damaged vehicles aren't nice.
	static const float s_fNiceCarCommentHPThreshold = 0.9f;
	if (pNiceVehicle->GetHealth() < pNiceVehicle->GetMaxHealth() * s_fNiceCarCommentHPThreshold)
	{
		return false;
	}

	if ( pVehicle )
	{
		//Vehicle exists and it isn't my current vehicle
		if(pVehicle == pNiceVehicle )
		{
			return false;
		}	

		//Only comment on stationary vehicles
		if (pNiceVehicle->GetVelocity().Mag2() > rage::square(1.0f) ) 
		{
			return false;
		}

		//The ped commenting can't be going too fast
		if( pVehicle->GetVelocity().Mag2() > rage::square(20.0f) )
		{
			return false;
		}

		// facing same direction
		Vector3 vPedForward = VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetForward());
		vPedForward.Normalize();
		Vector3 vNiceForward = VEC3V_TO_VECTOR3(pNiceVehicle->GetTransform().GetForward());
		vNiceForward.Normalize();

		if ( vNiceForward.Dot(vPedForward) < 0.7f ) 
		{
			return false;
		}

		//work out which window to open, could be passenger side
		s32 iSeatPos = pVehicle->GetSeatManager()->GetPedsSeatIndex(pPed);

		//only peds in front seats should comment
		if ( iSeatPos > 1 ) 
		{
			return false;
		}
	}
	else 
	{
		ScalarV vDistToVehicleSq  = DistSquared(pPed->GetTransform().GetPosition(), pNiceVehicle->GetTransform().GetPosition());
		ScalarV vEventRangeSq			= ScalarVFromF32(rage::square(sm_Tunables.m_VisualReactionRange));
		if (IsGreaterThanAll(vDistToVehicleSq, vEventRangeSq))
		{
			return false;
		}

		//vehicles need to react sooner to slow down correctly, limit the distance for peds on foot
		Vector3 vEventDirection = VEC3V_TO_VECTOR3(pNiceVehicle->GetTransform().GetPosition()) - VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
		if (vEventDirection.Mag2() > rage::square(8.0f))
		{
			return false;
		}

		//check the ped is facing the correct direction
		Vector3 vPedForward = VEC3V_TO_VECTOR3(pPed->GetTransform().GetForward());
		vPedForward.Normalize();
		vEventDirection.Normalize();
		float fDot = vEventDirection.Dot(vPedForward);
		if ( fDot < -0.2) 
		{
			return false;
		}
	}

	return true; 
}

//////////////////////////////////////////////////////////////////////////
//Shocking - Seen ped killed
//////////////////////////////////////////////////////////////////////////

bool CEventShockingSeenPedKilled::IsExposedToPed(CPed* pPed) const
{
	//! HACK. Need to add this to all non random peds aswell. In CShockingEventScanner::ScanForShockingEvents, we only add shocking events to ambient peds, so 
	//! this corrects that.
	//! Note: Only do this for MP games atm. May want to remove this on future projects (or create a seperate non shocking event that is handles differently).
	return pPed->PopTypeIsMission() && NetworkInterface::IsGameInProgress();
}

bool CEventShockingSeenPedKilled::AffectsPedCore(CPed* pPed) const
{
	if(!CEventShocking::AffectsPedCore(pPed))
	{
		return false;
	}

	if(IsExposedToPed(pPed))
	{
		if(pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_InVehicle) && pPed->GetMyVehicle())
		{
			CVehicle* pPedVehicle = pPed->GetMyVehicle();
			if(pPedVehicle->GetVehicleType() == VEHICLE_TYPE_BOAT
				|| pPedVehicle->GetVehicleType() == VEHICLE_TYPE_TRAIN
				|| pPedVehicle->GetVehicleType() == VEHICLE_TYPE_HELI
				|| pPedVehicle->GetVehicleType() == VEHICLE_TYPE_PLANE
				|| pPedVehicle->HasMissionCharsInIt()
				|| pPedVehicle->PopTypeIsMission())
			{
				return false;
			}
		}

		if(pPed->IsLawEnforcementPed())
		{
			// if this cop is chasing someone (it has an order) don't do shocking events
			if(pPed->GetPedIntelligence()->GetOrder())
			{
				return false;
			}
		}

		//! If we are actively tracking ped through targeting system, get LOS.
		if(pPed->GetPedIntelligence()->GetTargetting() && pPed->GetPedIntelligence()->GetTargetting()->FindTarget(GetSourceEntity()))
		{
			if((pPed->GetPedIntelligence()->GetTargetting()->GetLosStatus(GetSourceEntity(), true) == Los_clear))
			{
				return true;
			}
		}
		else
		{
			if(GetTargetPed())
			{
				s32 iTargetFlags = TargetOption_UseFOVPerception;
				return CPedGeometryAnalyser::CanPedTargetPed( *pPed, *GetTargetPed(), iTargetFlags, NULL);
			}

			return false;
		}
	}

	return true;
}

CPed* CEventShockingSeenPedKilled::GetTargetPed() const
{
	CEntity* pTargetEntity = GetOtherEntity();
	if(pTargetEntity && pTargetEntity->GetType() == ENTITY_TYPE_PED)
	{
		return static_cast<CPed*>(pTargetEntity);
	}

	return NULL;
}

//////////////////////////////////////////////////////////////////////////
//Shocking - Siren
//////////////////////////////////////////////////////////////////////////

bool CEventShockingSiren::CheckHasExpired() const
{
	if (CEventShocking::CheckHasExpired())
	{
		return true;
	}

	// If this event was created by script, we probably don't want to require the conditions below,
	// regardless of whether the event was created on an entity or not.
	if (m_CreatedByScript)
	{
		return false;
	}

	if (!m_pEntitySource || !m_pEntitySource->GetIsTypeVehicle())
	{
		return true;
	}
	else
	{
		// Does the source vehicle still have its sirens (or car alarm) on?
		CVehicle* pVehicle = (CVehicle*)m_pEntitySource.Get();
		if (!pVehicle->m_nVehicleFlags.GetIsSirenOn())
		{
			return true;
		}
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
//Shocking - Car Alarm
//////////////////////////////////////////////////////////////////////////

bool CEventShockingCarAlarm::CheckHasExpired() const
{
	if (CEventShocking::CheckHasExpired())
	{
		return true;
	}

	// If this event was created by script, we probably don't want to require the conditions below,
	// regardless of whether the event was created on an entity or not.
	if (m_CreatedByScript)
	{
		return false;
	}

	if (!m_pEntitySource || !m_pEntitySource->GetIsTypeVehicle())
	{
		return true;
	}
	else
	{
		// Does the source vehicle still have its sirens (or car alarm) on?
		CVehicle* pVehicle = (CVehicle*)m_pEntitySource.Get();
		if (!pVehicle->IsAlarmActivated())
		{
			return true;
		}
	}

	return false;
}

// Return the driver if it exists otherwise return null.
CPed* CEventShockingCarAlarm::GetSourcePed() const
{
	if (m_pEntitySource && m_pEntitySource->GetIsTypeVehicle())
	{
		CVehicle* pVehicle = static_cast<CVehicle*>(m_pEntitySource.Get());
		return pVehicle->GetDriver();
	}
	return CEventShocking::GetSourcePed();
}

// Return the driver if it exists otherwise return the vehicle.
CEntity* CEventShockingCarAlarm::GetSourceEntity() const
{
	if (m_pEntitySource && m_pEntitySource->GetIsTypeVehicle())
	{
		CVehicle* pVehicle = static_cast<CVehicle*>(m_pEntitySource.Get());
		CPed* pDriver = pVehicle->GetDriver();
		if(pDriver)
		{
			return pDriver;
		}
	}
	return m_pEntitySource;
}

//////////////////////////////////////////////////////////////////////////
//Shocking - Vehicle is being towed
//////////////////////////////////////////////////////////////////////////
bool CEventShockingVehicleTowed::AffectsPedCore(CPed* pPed) const
{
	if (CShockingEventsManager::IsShockingEventSuppressed(*this))
	{
		return false;
	}

	CEntity* pTowedEntity = GetOtherEntity();
	CEntity* pSourceEntity = GetSourceEntity();
	CVehicle* pVehicle = pPed->GetMyVehicle();
	// Check if the target of the towing is the vehicle this ped is currently inside.
	if (pVehicle && pVehicle == pTowedEntity)
	{
		if (pSourceEntity && pSourceEntity->GetIsTypeVehicle())
		{
			CPed* pTowingPed = static_cast<CVehicle*>(pSourceEntity)->GetDriver();

			// Check if the towing ped is a player ped.
			if (pTowingPed && pTowingPed->IsAPlayerPed())
			{
				// If we are a cop and our vehicle is getting towed, report the crime.
				if (pPed->IsLawEnforcementPed())
				{
					CCrime::ReportCrime(CRIME_STEAL_VEHICLE, pTowedEntity, pTowingPed, m_iStartTime);
				}
			}
		}
		return true;
	}

	return false;
}


//////////////////////////////////////////////////////////////////////////
//Shocking - Visible Weapon
//////////////////////////////////////////////////////////////////////////

bool CEventShockingVisibleWeapon::CheckHasExpired() const
{
	if(CEventShocking::CheckHasExpired())
	{
		return true;
	}

	// If this event was created by script, we probably don't want to require the conditions below.
	if(m_CreatedByScript)
	{
		return false;
	}

	if(!m_pEntitySource || !m_pEntitySource->GetIsTypePed())
	{
		return true;
	}

	CPed* pSourcePed = static_cast<CPed*>(m_pEntitySource.Get());
	if(!pSourcePed->GetWeaponManager())
	{
		return true;
	}

	// For now, make sure the event is removed if we're unarmed. Probably what we should
	// do is to add a parameter for each weapon type which specifies which
	// visible weapon event type to use.
	const CWeaponInfo* pWeaponInfo = pSourcePed->GetWeaponManager()->GetEquippedWeaponInfo();
	if( !pWeaponInfo || pWeaponInfo->GetIsUnarmed() )
	{
		return true;
	}

	// If you get on a vehicle, the weapon is probably not going to be visible,
	// and a different weapon may be used if you do a driveby anyway. At least
	// for now, we let the shocking event expire if you are on a vehicle. It
	// looks like it gets added back properly once you exit the vehicle.
	if(pSourcePed->GetPedConfigFlag(CPED_CONFIG_FLAG_InVehicle) && pSourcePed->GetMyVehicle())
	{
		return true;
	}

#if 0
	eSEventTypes eType = (eSEventTypes)CShockingEventsManager::ConvertVisibleWeaponEType(nWeaponHash);
	if( eType != m_eType )
		return true;
#endif

	return false;
}

//////////////////////////////////////////////////////////////////////////
//Shocking - Weapon Threat
//////////////////////////////////////////////////////////////////////////

CPed* CEventShockingWeaponThreat::GetTargetPed() const
{
	CEntity* pTargetEntity = GetOtherEntity();
	if(pTargetEntity && pTargetEntity->GetType() == ENTITY_TYPE_PED)
	{
		return static_cast<CPed*>(pTargetEntity);
	}

	return NULL;
}

bool CEventShockingWeaponThreat::AffectsPedCore(CPed* pPed) const
{
	// Disable this event for mission peds.
	if (pPed->PopTypeIsMission())
	{
		return false;
	}

#if FPS_MODE_SUPPORTED
	if( camInterface::GetGameplayDirector().IsFirstPersonModeEnabled() &&
		camInterface::GetGameplayDirector().IsFirstPersonModeAllowed() )
	{
		CPed* pPedThreat = GetTargetPed();
		if( pPedThreat && pPedThreat->GetPlayerInfo() && !pPedThreat->GetPlayerInfo()->IsAiming() )
		{
			// In first person mode, player is alway aiming, so ignore 
			// weapon unless player is really aiming or shooting.
			return false;
		}
	}
#endif // FPS_MODE_SUPPORTED

	//Ensure we are either on-screen, or within a certain distance of the ped being aimed at.
	if(!pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_VisibleOnScreen))
	{
		Vec3V vPedPosition = pPed->GetTransform().GetPosition();
		Vec3V vOtherPosition = GetOtherEntity() ?
			GetOtherEntity()->GetTransform().GetPosition() : GetEntityOrSourcePosition();
		ScalarV scDistSq = DistSquared(vPedPosition, vOtherPosition);
		static dev_float s_fMaxDistance = 7.0f;
		ScalarV scMaxDistSq = ScalarVFromF32(square(s_fMaxDistance));
		if(IsGreaterThanAll(scDistSq, scMaxDistSq))
		{
			return false;
		}
	}

	if (!CEventShocking::AffectsPedCore(pPed) )
	{
		return false;
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
//Shocking - Weird Ped
//////////////////////////////////////////////////////////////////////////

bool CEventShockingWeirdPed::AffectsPedCore(CPed* pPed) const
{
	if (!CEventShocking::AffectsPedCore(pPed) )
	{
		return false;
	}

	//Ensure the flag is not set.
	if(pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_DisableWeirdPedEvents))
	{
		return false;
	}

	return true;
}

bool CEventShockingWeirdPed::CheckHasExpired() const
{
	if(CEventShocking::CheckHasExpired())
	{
		return true;
	}

	// If this event was created by script, we probably don't want to require the conditions below,
	// regardless of whether the event was created on an entity or not.
	if(m_CreatedByScript)
	{
		return false;
	}

	if(!m_pEntitySource || !m_pEntitySource->GetIsTypePed())
	{
		return true;
	}
	//is source still weird?
	else
	{
		CPed* pPed = (CPed*)m_pEntitySource.Get();
		if(!pPed->GetPedModelInfo()->GetPersonalitySettings().GetIsWeird() && !pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_PlayerIsWeird))
		{
			return true;
		}
	}

	return false;
}

bool CEventShockingWeirdPedApproaching::AffectsPedCore(CPed* pPed) const
{
	if (!CEventShocking::AffectsPedCore(pPed) )
	{
		return false;
	}

	//Ensure the flag is not set.
	if(pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_DisableWeirdPedEvents))
	{
		return false;
	}

	//Only respond if the ped is approaching the source or the source is approaching the ped.
	static const float s_fApproachingPlayerCosineThreshold = 0.98f;

	CPed* pSourcePed = GetSourcePed();

	// Not valid if there isn't a source ped.
	if (pSourcePed && pPed->GetMotionData()->GetCurrentMbrY() > MOVEBLENDRATIO_STILL)
	{
		//Is this ped moving towards the source?
		Vec3V vStraightLine = pSourcePed->GetTransform().GetPosition() - pPed->GetTransform().GetPosition();
		vStraightLine.SetZ(ScalarV(V_ZERO));
		vStraightLine = NormalizeFastSafe(vStraightLine, Vec3V(V_ZERO));

		if (IsGreaterThanAll(Dot(pPed->GetTransform().GetForward(), vStraightLine), ScalarV(s_fApproachingPlayerCosineThreshold)))
		{
			return true;
		}
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
//Shocking - Potential Blast
//////////////////////////////////////////////////////////////////////////

bool CEventShockingPotentialBlast::AffectsPedCore(CPed* pPed) const
{
	if(!CEventShocking::AffectsPedCore(pPed))
	{
		return false;
	}

	//Ensure we are not the source ped.
	if(pPed == GetSourcePed())
	{
		return false;
	}

	//Ensure reactions to this event are not disabled.
	if(pPed->GetPedResetFlag(CPED_RESET_FLAG_DisablePotentialBlastReactions))
	{
		return false;
	}

	//Ensure we are not already running threat response, combat, or flee.
	if(pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_THREAT_RESPONSE) ||
		pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_COMBAT) ||
		pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_SMART_FLEE))
	{
		return false;
	}

	//Ensure the ped is not already agitated.
	if(CTaskAgitated::ShouldIgnoreEvent(*pPed, *this))
	{
		return false;
	}

	return true;
}

CPed* CEventShockingPotentialBlast::GetSourcePed() const
{
	//Check if the entity is valid.
	CEntity* pEntity = GetSourceEntity();
	if(pEntity)
	{
		//Check if the entity is a ped.
		if(pEntity->GetIsTypePed())
		{
			return static_cast<CPed *>(pEntity);
		}
		//Check if the entity is a vehicle.
		else if(pEntity->GetIsTypeVehicle())
		{
			return static_cast<CVehicle *>(pEntity)->GetDriver();
		}
		//Check if the entity is an object.
		else if(pEntity->GetIsTypeObject())
		{
			CObject* pObject = static_cast<CObject *>(pEntity);

			//Check if the object is a projectile.
			CProjectile* pProjectile = pObject->GetAsProjectile();
			if(pProjectile)
			{
				//Check if the owner is valid.
				CEntity* pOwner = pProjectile->GetOwner();
				if(pOwner)
				{
					//Check if the owner is a ped.
					if(pOwner->GetIsTypePed())
					{
						return static_cast<CPed *>(pOwner);
					}
				}
			}
		}
	}

	return NULL;
}

CProjectile* CEventShockingPotentialBlast::GetProjectile() const
{
	//Check if the entity is valid.
	CEntity* pEntity = GetSourceEntity();
	if(pEntity && pEntity->GetIsTypeObject())
	{
		// Return this object as a projectile (will return NULL if not a projectile)
		return static_cast<CObject *>(pEntity)->GetAsProjectile();
	}

	return NULL;
}

// End of file 'event/EventShocking.cpp'
