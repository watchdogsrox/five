#include "event/ShockingEvents.h"
#include "event/EventGroup.h"
#include "event/EventShocking.h"
#include "debug/VectorMap.h"
#include "Peds/ped.h"
#include "profile/profiler.h"
#include "scene/world/GameWorld.h"

// TODO: clean these up

PF_PAGE(GTAShockingEvents, "GTA ShockingEvents");
PF_GROUP(ShockingEvents);
PF_LINK(GTAShockingEvents, ShockingEvents);

PF_TIMER(Add_Total, ShockingEvents);
PF_TIMER(Add_EventAlreadyExists, ShockingEvents);
PF_TIMER(Add_GlobalGroupAdd, ShockingEvents);

AI_OPTIMISATIONS()

//-----------------------------------------------------------------------------

#if __BANK
	bool CShockingEventsManager::RENDER_VECTOR_MAP			= false;
	bool CShockingEventsManager::RENDER_AS_LIST				= false;
	bool CShockingEventsManager::RENDER_ON_3D_SCREEN		= false;
	bool CShockingEventsManager::RENDER_EVENT_EFFECT_AREA	= false;

	bool CShockingEventsManager::RENDER_SHOCK_EVENT_LEVEL_INTERESTING			= true;
	bool CShockingEventsManager::RENDER_SHOCK_EVENT_LEVEL_AFFECTSOTHERS			= true;
	bool CShockingEventsManager::RENDER_SHOCK_EVENT_LEVEL_POTENTIALLYDANGEROUS	= true;
	bool CShockingEventsManager::RENDER_SHOCK_EVENT_LEVEL_SERIOUSDANGER			= true;
#endif


bool	CShockingEventsManager::ms_bDisableEvents = false;

fwFlags8 CShockingEventsManager::ms_SuppressedShockingEventPriorities;

u32 CShockingEventsManager::Add(CEventShocking &ev, bool createdByScript, float overrideLifeTime)
{
	PF_FUNC(Add_Total);

	if(ms_bDisableEvents)
	{
		return 0;
	}

	if (IsShockingEventSuppressed(ev))
	{
		return 0;
	}

#if 0
	if(eType == ESeenMeleeAction 
		|| eType == ESeenGangFight  
		|| eType == EGunFight 
		|| eType == EExplosion 
		|| eType == EFire
		|| eType == EGunshotFired)
	{
		//dont have to have an Entity
	}
	else
	{
		//*MUST* have a pEntity
		Assert(ev.m_pEntitySource);
	}
#endif

// TODO: Reimplement the visible weapon stuff somehow.
#if 0
	//special conversion for visible weapons - I *love* special cases.....
	if(eType == EVisibleWeapon)
		eType = (eSEventTypes)ConvertVisibleWeaponEType(nWeaponHash);
	ev.m_eType = eType;
#endif

	const CEventShocking::Tunables &tunables = ev.GetTunables();

	//vars to record which event is closest to completing in case ShockLevel is full 
	u32 nCurrentTime = fwTimer::GetTimeInMilliseconds(); 
	ev.m_iStartTime = nCurrentTime;

	ev.SetCreatedByScript(createdByScript);

	float lifeTime = tunables.m_LifeTime;
	if(overrideLifeTime >= 0.0f)
	{
		lifeTime = overrideLifeTime;
	}
	ev.m_iLifeTime = (int)(1000.0f*lifeTime);

	// If the event is script-created, only allow a lifetime of 0 (which is interpreted
	// as infinite) if passed in as an override. Events that happen to have the lifetime
	// set to 0 in their tuning data will just live for a short amount of time. Script-created
	// events with lifetime 0 are a bit risky in case they forget to remove them, but
	// should be OK as at least we tend to clear out all events on death etc.
	if(createdByScript && overrideLifeTime != 0.0f)
	{
		ev.m_iLifeTime = Max(ev.m_iLifeTime, 1U);
	}

	ev.m_fEventRadius += tunables.m_VisualReactionRange;

	// Set the priority of the event. It would be better if the parser knew about
	// eEventPriorityList so CEventShocking::Tunables could simply contain an enum
	// value of that type and eSEventShockLevels could go away, but that would be a bit
	// of a bigger change than I want to make right now.
	eEventPriorityList pri = E_PRIORITY_SHOCKING_EVENT_INTERESTING;
	switch(tunables.m_Priority)
	{
		case EInteresting:
			pri = E_PRIORITY_SHOCKING_EVENT_INTERESTING;
			break;
		case EAffectsOthers:
			pri = E_PRIORITY_SHOCKING_EVENT_AFFECTS_OTHERS;
			break;
		case EPotentiallyDangerous:
			pri = E_PRIORITY_SHOCKING_EVENT_POTENTIALLY_DANGEROUS;
			break;
		case EDangerous:
			pri = E_PRIORITY_SHOCKING_EVENT_DANGEROUS;
			break;
		case ESeriousDanger:
			pri = E_PRIORITY_SHOCKING_EVENT_SERIOUS_DANGER;
			break;
		default:
			Assertf(0, "Invalid shocking event priority %d.", tunables.m_Priority);
			break;
	}
	ev.m_Priority = pri;

	//check event does not already exist, if it does, the sub function updates with necessary changes then quit
	PF_START(Add_EventAlreadyExists);

	// Don't check if the event already exists if script wants to add the event.
	if(!createdByScript && EventAlreadyExists(ev))
	{
		PF_STOP(Add_EventAlreadyExists);
		return 0;
	}
	PF_STOP(Add_EventAlreadyExists);

	PF_START(Add_GlobalGroupAdd);
	u32 id = ev.GetId();
	if(!GetEventGlobalGroup()->Add(ev))
	{
		PF_STOP(Add_GlobalGroupAdd);
		// Failed to add it, not sure if we should print a warning or something here.
		return 0;
	}
	PF_STOP(Add_GlobalGroupAdd);

	return id;
}


//clears all existing events
void CShockingEventsManager::ClearAllEvents(bool clearNonScriptCreated, bool clearScriptCreated)
{
	CEventGroupGlobal& global = *GetEventGlobalGroup();
	AI_EVENT_GROUP_LOCK(&global)

	int num = global.GetNumEvents();
	for(int j = 0; j < num; j++)
	{
		fwEvent* ev = global.GetEventByIndex(j);
		if(!ev || !static_cast<const CEvent*>(ev)->IsShockingEvent())
		{
			continue;
		}
		const CEventShocking& shocking = *static_cast<const CEventShocking*>(ev);
		const bool createdByScript = shocking.GetCreatedByScript();
		if((createdByScript && !clearScriptCreated) || (!createdByScript && !clearNonScriptCreated))
		{
			continue;
		}
		global.Remove(*ev);
		j--;
		num--;
	}
}


void CShockingEventsManager::ClearEventsInSphere(Vec3V_In sphereCenter, const float& sphereRadius, bool clearNonScriptCreated, bool clearScriptCreated)
{
	const Vec3V sphereCenterV = sphereCenter;
	const ScalarV sphereRadiusV = LoadScalar32IntoScalarV(sphereRadius);
	const ScalarV sphereRadiusSqV = Scale(sphereRadiusV, sphereRadiusV);

	CEventGroupGlobal& global = *GetEventGlobalGroup();
	AI_EVENT_GROUP_LOCK(&global)

	int num = global.GetNumEvents();
	for(int j = 0; j < num; j++)
	{
		fwEvent* ev = global.GetEventByIndex(j);
		if(!ev || !static_cast<const CEvent*>(ev)->IsShockingEvent())
		{
			continue;
		}
		const CEventShocking& shocking = *static_cast<const CEventShocking*>(ev);
		const bool createdByScript = shocking.GetCreatedByScript();
		if((createdByScript && !clearScriptCreated) || (!createdByScript && !clearNonScriptCreated))
		{
			continue;
		}

		const Vec3V eventPosV = shocking.GetEntityOrSourcePosition();
		const ScalarV distSqV = DistSquared(sphereCenterV, eventPosV);
		if(IsGreaterThanAll(distSqV, sphereRadiusSqV))
		{
			continue;
		}

		global.Remove(*ev);
		j--;
		num--;
	}
}

//clears all existing events that have the entity as the source
void CShockingEventsManager::ClearAllEventsWithSourceEntity(const CEntity* pSourceEntity)
{
	CEventGroupGlobal& global = *GetEventGlobalGroup();
	AI_EVENT_GROUP_LOCK(&global)

	int num = global.GetNumEvents();
	for(int j = 0; j < num; j++)
	{
		fwEvent* ev = global.GetEventByIndex(j);
		if(!ev || !static_cast<const CEvent*>(ev)->IsShockingEvent())
		{
			continue;
		}
		CEntity* pEventSourceEntity = static_cast<const CEventShocking*>(ev)->GetSourceEntity();
		if( pEventSourceEntity != pSourceEntity)
		{
			bool bPedDrivingSourceVeh = false;
			if (pSourceEntity && pSourceEntity->GetIsTypePed())
			{
				const CPed* pSrcPed = static_cast<const CPed*>(pSourceEntity);
				if (pSrcPed->GetIsDrivingVehicle())
				{
					CVehicle* pSrcVeh = pSrcPed->GetVehiclePedInside();
					if (pSrcVeh->GetDriver() == pSrcPed && pSrcVeh == pEventSourceEntity)
					{
						bPedDrivingSourceVeh = true;
					}
				}
			}

			if (!bPedDrivingSourceVeh)
				continue;
		}
		global.Remove(*ev);
		j--;
		num--;
	}
}


void CShockingEventsManager::ClearAllBlockingAreas()
{
	// First, just clear out any references we have to the blocking areas.
	// We don't want to risk messing with somebody else's reuse of the blocking areas.
	// Note: this already seems a bit fragile in other parts of this code: we seem to basically
	// be relying on the delay to be longer than the period of ApplySpecialBehaviors() calls...
	CEventGroupGlobal& global = *GetEventGlobalGroup();
	AI_EVENT_GROUP_LOCK(&global)

	int num = global.GetNumEvents();
	for(int j = 0; j < num; j++)
	{
		fwEvent* ev = global.GetEventByIndex(j);
		if(!ev || !static_cast<const CEvent*>(ev)->IsShockingEvent())
		{
			continue;
		}

		CEventShocking* pShockingEvent = static_cast<CEventShocking*>(ev);
		pShockingEvent->m_iPedGenBlockingAreaIndex = -1;
	}

	// Clear all blocking areas for shocking events.
	// Note that we couldn't just have used the m_iPedGenBlockingAreaIndex values
	// from the events, because we intentionally let the blocking areas live longer
	// than the events.
	CPathServer::GetAmbientPedGen().ClearAllPedGenBlockingAreasWithSpecificOwner(CPedGenBlockedArea::EShockingEvent);
}


bool CShockingEventsManager::EventAlreadyExists(CEventShocking& event)
{	
	const int eType = event.GetEventType();
	const Vector3 &vEventPosition1 = RCC_VECTOR3(event.m_vEventPosition);
	CEntity* pEntityA = event.m_pEntitySource;
	CEntity* pEntityB = event.m_pEntityOther;

	// Would be nice to not have to do this, but since we seem to be getting these sometimes,
	// it's probably worth it for the increased stability, assuming that we still try to track
	// down the root of the problem.
	if(!Verifyf(vEventPosition1.FiniteElements(),
			"Invalid numbers in shocking event position (%f, %f, %f), please track down where they came from.",
			vEventPosition1.x, vEventPosition1.y, vEventPosition1.z))
	{
		// This should prevent events with bad numbers from making it further into the system.
		return true;
	}

	CEventGroupGlobal& global = *GetEventGlobalGroup();
	AI_EVENT_GROUP_LOCK(&global)

	const int num = global.GetNumEvents();
	CEventShocking* pEvent = NULL;
	for(int j = 0; j < num; j++)
	{
		fwEvent* ev = global.GetEventByIndex(j);
		if(!ev || !static_cast<const CEvent*>(ev)->IsShockingEvent())
		{
			continue;
		}
		pEvent = static_cast<CEventShocking*>(ev);

		if (!pEvent->GetCanBeMerged())
		{
			continue;
		}

		bool bTypeMatch = false;

		if(pEvent->GetEventType() == event.GetEventType())
			bTypeMatch = true;

		/*if(eType >= EVisibleWeapon && eType <= EVisibleWeaponHEAVY )
		{
			if(pEvent->m_eType >= EVisibleWeapon && pEvent->m_eType <= EVisibleWeaponHEAVY )
				bTypeMatch = true;
		}
		else*/ if(event.GetEventType() == EVENT_SHOCKING_GUNSHOT_FIRED && pEvent->GetEventType() == EVENT_SHOCKING_GUN_FIGHT)
			bTypeMatch = true;
		else if(event.GetEventType() == EVENT_SHOCKING_CAR_CRASH && pEvent->GetEventType() == EVENT_SHOCKING_CAR_PILE_UP)
			bTypeMatch = true;
		else if(event.GetEventType() == EVENT_SHOCKING_SEEN_MELEE_ACTION && pEvent->GetEventType() == EVENT_SHOCKING_SEEN_GANG_FIGHT)
			bTypeMatch = true;
		else if(event.GetEventType() == EVENT_SHOCKING_RUNNING_PED && pEvent->GetEventType() == EVENT_SHOCKING_RUNNING_STAMPEDE)
			bTypeMatch = true;

		u32 nLifeTime = event.m_iLifeTime;

		// Never match with script created events.  Script is expecting them to persist and not be merged / etc.
		if (pEvent->GetCreatedByScript())
		{
			bTypeMatch = false;
		}

		if(bTypeMatch)
		{
			bool bMatch = false;
			if(eType == EVENT_SHOCKING_EXPLOSION || eType == EVENT_SHOCKING_FIRE)
			{
				//if another explosion occurs near a current one, we merge them into a super explosion
				float fDistanceSqr = (RCC_VECTOR3(pEvent->m_vEventPosition) - vEventPosition1).XYMag2();
				if(( !pEntityA && !pEvent->m_pEntitySource && fDistanceSqr <= rage::square(10.0f)) || 
					(pEntityA && pEntityA==pEvent->m_pEntitySource))
				{
					//update the event with the new details - keep the longest lifetime
					pEvent->UpdateLifeTime(event.m_iLifeTime, fwTimer::GetTimeInMilliseconds());

					//re-position halfway between the 2 explosions
					Vector3 vecNewPos = RCC_VECTOR3(pEvent->m_vEventPosition) + vEventPosition1;
					vecNewPos.Scale(0.5f);

					Assert(pEvent->GetEventType() == event.GetEventType());
					pEvent->m_vEventPosition = VECTOR3_TO_VEC3V(vecNewPos);

					if(fDistanceSqr > rage::square(pEvent->m_fEventRadius))
						pEvent->m_fEventRadius = rage::Sqrtf(fDistanceSqr);

					//m_Events[i].m_uiEventID	= GetNextIDnumber();
					pEvent->m_bHasReactPosition = true;
					pEvent->m_vReactPosition = event.m_vEventPosition;
					pEvent->ApplySpecialBehaviors();
					return true;
				}
			}
			else if(eType == EVENT_SHOCKING_GUNSHOT_FIRED)
			{
				//if another gunshot occurs near a current one, we merge them into a gun fight
				// of course, we also need to check for a gun fight.....
				float fDistanceSqr = (RCC_VECTOR3(pEvent->m_vEventPosition) - vEventPosition1).XYMag2();

				if(fDistanceSqr < 400.0f || fDistanceSqr < rage::square(pEvent->m_fEventRadius))
				{
					// count how many times this event has been supplemented
					pEvent->m_nCountAdds++;

					//re-position event position based on the influence of previous shots
					float fNewInfluence = 1.0f / (float)(pEvent->m_nCountAdds + 1);
					Vector3 vecNewPos = fNewInfluence * Vector3(vEventPosition1) + (1.0f - fNewInfluence)*RCC_VECTOR3(pEvent->m_vEventPosition);
					//Vector3 vecDeltaPos = vecNewPos - pEvent->m_vEventPosition;
					pEvent->m_vEventPosition = VECTOR3_TO_VEC3V(vecNewPos);

					if(pEvent->m_nCountAdds > 10)
					{
						pEvent->SetUseSourcePosition(true);
						nLifeTime = Max(nLifeTime, (u32)(1000.0f*CEventShockingGunFight::StaticGetTunables().m_LifeTime));

						Assert(pEvent->GetEventType() == EVENT_SHOCKING_GUNSHOT_FIRED || pEvent->GetEventType() == EVENT_SHOCKING_GUN_FIGHT);
						if(pEvent->GetEventType() == EVENT_SHOCKING_GUNSHOT_FIRED)
						{
							pEvent->m_fEventRadius += 10.0f;
							// switch Id when it becomes a new type of event
							//pEvent->m_uiEventID	= GetNextIDnumber();
							//pEvent->m_eType	= EGunFight;

							// Note: the old CShockingEventsManager used to change one event
							// into another here, but now we remove the old event and create a
							// new one instead. This may not be strictly necessary, but probably
							// safest in case we have different subclasses and stuff, and may also
							// help to ensure that the peds reconsider the new event.

							CEventShockingGunFight newEvent;
							pEvent->InitClone(newEvent, false);	// Probably best to generate new ID.
							Assert(newEvent.GetEventType() == EVENT_SHOCKING_GUN_FIGHT);

							global.Remove(*pEvent);
							pEvent = static_cast<CEventShocking*>(global.Add(newEvent));
						}

					}

					//update the event with the new details
					pEvent->UpdateLifeTime(nLifeTime, fwTimer::GetTimeInMilliseconds());
					pEvent->ApplySpecialBehaviors();
					return true;
				}
			}
			else if(eType == EVENT_SHOCKING_CAR_CRASH)
			{
				//if another car crash occurs near a current one, we merge them into a car pile up
				float fDistanceSqr = (RCC_VECTOR3(pEvent->m_vEventPosition) - vEventPosition1).XYMag2();
				if(fDistanceSqr < 100.0f || fDistanceSqr < rage::square(pEvent->m_fEventRadius))
				{
					// count how many times this event has been supplemented
					pEvent->m_nCountAdds++;

					//re-position event position based on the influence of previous shots
					float fNewInfluence = 1.0f / (float)(pEvent->m_nCountAdds + 1);
					Vector3 vecNewPos = fNewInfluence * Vector3(vEventPosition1) + (1.0f - fNewInfluence)*RCC_VECTOR3(pEvent->m_vEventPosition);
					//Vector3 vecDeltaPos = vecNewPos - pEvent->m_vEventPosition;
					pEvent->m_vEventPosition = VECTOR3_TO_VEC3V(vecNewPos);

					if(pEvent->m_nCountAdds > 3 && pEntityA != pEvent->m_pEntitySource && pEntityA != pEvent->m_pEntitySource)
					{
						Assert(pEvent->GetEventType() == EVENT_SHOCKING_CAR_CRASH || pEvent->GetEventType() == EVENT_SHOCKING_CAR_PILE_UP);
						if(pEvent->GetEventType() == EVENT_SHOCKING_CAR_CRASH)
						{
							pEvent->m_fEventRadius += 10.0f;
							//pEvent->m_uiEventID = GetNextIDnumber();
							//pEvent->m_eType = ECarPileUp;

							CEventShockingCarPileUp newEvent;
							pEvent->InitClone(newEvent, false);		// Probably best to generate new ID.
							Assert(newEvent.GetEventType() == EVENT_SHOCKING_CAR_PILE_UP);

							global.Remove(*pEvent);
							pEvent = static_cast<CEventShocking*>(global.Add(newEvent));
						}
					}

					if(fDistanceSqr > rage::square(pEvent->m_fEventRadius))
					{
						pEvent->m_fEventRadius = rage::Sqrtf(fDistanceSqr);
					}

					//update the event with the new details
					pEvent->UpdateLifeTime(nLifeTime, fwTimer::GetTimeInMilliseconds());
					pEvent->ApplySpecialBehaviors();
					return true;
				}
			}
			else if(eType == EVENT_SHOCKING_SEEN_MELEE_ACTION)
			{
				//if another fight occurs near a current one, we merge them into a gang fight
				float fDistanceSqr = (RCC_VECTOR3(pEvent->m_vEventPosition) - vEventPosition1).XYMag2();
				if(fDistanceSqr <= 100.0f || fDistanceSqr < rage::square(pEvent->m_fEventRadius))
				{
					//re-position event position based on the influence of previous shots
					float fNewInfluence = 1.0f / (float)(pEvent->m_nCountAdds + 1);
					Vector3 vecNewPos = fNewInfluence * Vector3(vEventPosition1) + (1.0f - fNewInfluence)*RCC_VECTOR3(pEvent->m_vEventPosition);
					//Vector3 vecDeltaPos = vecNewPos - pEvent->m_vEventPosition;
					pEvent->m_vEventPosition = VECTOR3_TO_VEC3V(vecNewPos);

					if(pEvent->m_nCountAdds > 10 && pEntityA != pEvent->m_pEntitySource && pEntityA != pEvent->m_pEntitySource)
					{
						Assert(pEvent->GetEventType() == EVENT_SHOCKING_SEEN_MELEE_ACTION || pEvent->GetEventType() == EVENT_SHOCKING_SEEN_GANG_FIGHT);
						if(pEvent->GetEventType() == EVENT_SHOCKING_SEEN_MELEE_ACTION)
						{
							 pEvent->m_fEventRadius += 5.0f;
							 //pEvent->m_uiEventID = GetNextIDnumber();
							//pEvent->m_eType = ESeenGangFight;

							CEventShockingSeenGangFight newEvent;
							pEvent->InitClone(newEvent, false);	// Probably best to generate new ID.
							Assert(newEvent.GetEventType() == EVENT_SHOCKING_SEEN_GANG_FIGHT);

							global.Remove(*pEvent);
							pEvent = static_cast<CEventShocking*>(global.Add(newEvent));
						}
					}

					if(pEvent->m_pEntitySource && pEntityA != pEvent->m_pEntitySource)
					{
						// event is not attached to any one entity any more
						pEvent->RemoveEntities();
						//pEvent->m_uiEventID = GetNextIDnumber();
					}

					if(fDistanceSqr > rage::square(pEvent->m_fEventRadius))
						pEvent->m_fEventRadius = rage::Sqrtf(fDistanceSqr);

					//update the event with the new details
					pEvent->UpdateLifeTime(nLifeTime, fwTimer::GetTimeInMilliseconds());
					pEvent->ApplySpecialBehaviors();
					return true;
				}
			}
			else if(eType == EVENT_SHOCKING_RUNNING_PED)
			{
				//If multiple peds are running next to each other, merge them into a stampede.
				float fDistanceSqr = (RCC_VECTOR3(pEvent->m_vEventPosition) - vEventPosition1).XYMag2();
				// Always keep player running events unique
				const bool bOneContainsThePlayer = pEntityA == CGameWorld::FindLocalPlayer() || pEvent->m_pEntitySource == CGameWorld::FindLocalPlayer();
				if( pEntityA == pEvent->m_pEntitySource || ((fDistanceSqr < 225.0f || fDistanceSqr < rage::square(pEvent->m_fEventRadius)) && !bOneContainsThePlayer ) )
				{
					//Check if we have updated the event.
					bool bUpdatedEvent = false;
					
					//Check if the event is a running ped.
					Assert(pEvent->GetEventType() == EVENT_SHOCKING_RUNNING_PED || pEvent->GetEventType() == EVENT_SHOCKING_RUNNING_STAMPEDE);
					if(pEvent->GetEventType() == EVENT_SHOCKING_RUNNING_PED)
					{
						//Check if the source is different.
						if(pEntityA != pEvent->m_pEntitySource)
						{
							//Update the event position.
							pEvent->m_vEventPosition = Average(pEvent->m_vEventPosition, VECTOR3_TO_VEC3V(vEventPosition1));
							pEvent->SetUseSourcePosition(true);
							
							//Increase the radius.
							pEvent->m_fEventRadius += 10.0f;
							
							//Create a stampede event.
							CEventShockingRunningStampede newEvent;
							pEvent->InitClone(newEvent, false);	// Probably best to generate new ID.
							Assert(newEvent.GetEventType() == EVENT_SHOCKING_RUNNING_STAMPEDE);
							
							//Add the stampede event.
							global.Remove(*pEvent);
							pEvent = static_cast<CEventShocking*>(global.Add(newEvent));
							
							//Note that we updated the event.
							bUpdatedEvent = true;
						}
						else
						{
							//Note that our event matches.
							bMatch = true;
						}
					}
					//The event is a stampede.
					else
					{
						//Merge with the stampede.
						pEvent->m_nCountAdds++;
						
						//Calculate the new position.
						float fNewInfluence = 1.0f / (float)(pEvent->m_nCountAdds);
						Vector3 vecNewPos = fNewInfluence * Vector3(vEventPosition1) + (1.0f - fNewInfluence)*RCC_VECTOR3(pEvent->m_vEventPosition);
						pEvent->m_vEventPosition = VECTOR3_TO_VEC3V(vecNewPos);
						pEvent->SetUseSourcePosition(true);
						
						//Note that we updated the event.
						bUpdatedEvent = true;
					}
					
					//Check if we updated the event.
					if(bUpdatedEvent)
					{
						//Update the event with the new details.
						pEvent->UpdateLifeTime(nLifeTime, fwTimer::GetTimeInMilliseconds());
						pEvent->ApplySpecialBehaviors();
						return true;
					}
				}
			}
			else if (eType == EVENT_SHOCKING_BROKEN_GLASS)
			{
				// Merge IFF the smasher is the same and the two locations are close.
				if (pEntityA == pEvent->m_pEntitySource)
				{
					if (IsLessThanAll(DistSquared(pEvent->m_vEventPosition, VECTOR3_TO_VEC3V(vEventPosition1)), ScalarV(10.0f * 10.0f)))
					{
						bMatch = true;
					}
				}
			}
			else
			{
				//for certain events, i.e. fighting, we don't want an event for each punch
				if(pEntityA == pEvent->m_pEntitySource)
					bMatch = true;
				if(pEntityA == pEvent->m_pEntityOther)
					bMatch = true;
				if(pEntityB && pEntityB == pEvent->m_pEntitySource)
					bMatch = true;
				if(pEntityB && pEntityB == pEvent->m_pEntityOther)
					bMatch = true;	
			}

			if(bMatch)
			{
				//update the event with the new details
				pEvent->UpdateLifeTime(nLifeTime, fwTimer::GetTimeInMilliseconds());

				//if the event is a visible weapon, has the ped changed it to a "heavier" weapon?
// TODO: work out the visible weapon issues
#if 0
				if(pEvent->m_eType >= EVisibleWeapon && pEvent->m_eType <= EVisibleWeaponHEAVY)
				{
					//if so, record the new weapon and give event new ID number, to ensure new reactions		
					if(eType > pEvent->m_eType)
					{
						pEvent->m_eType			= eType;
						pEvent->m_uiEventID		= GetNextIDnumber();
					}
				}
#endif
				return true;
			}
		}
	}

	return false;
}

#if __BANK

void CShockingEventsManager::RenderShockingEvents()
{
	CEventGroupGlobal* list = GetEventGlobalGroup();
	AI_EVENT_GROUP_LOCK(list)

	const int num = list->GetNumEvents();

	if( !RENDER_VECTOR_MAP && !RENDER_AS_LIST && !RENDER_ON_3D_SCREEN)
	{
		return;
	}

	char debugText[100];

	int iNoOfTexts=0;
	static Vector3 ScreenCoors(0.1f, 0.2f, 0.0f);

	Vector2 vTextRenderScale(0.3f, 0.3f);
	Vector2 vTextRenderPos(ScreenCoors.x, ScreenCoors.y);
	float fVertDist = 0.025f;

	if( RENDER_AS_LIST )
	{
		grcDebugDraw::Text(vTextRenderPos, Color_white, "Shocking events:");
		iNoOfTexts++;

#if 0
		sprintf( debugText, "Num Current Events: %d; Peds: %d", ms_uiNumActiveEvents, ms_uiPedsChecked );
		vTextRenderPos.y = ScreenCoors.y + (iNoOfTexts++)*fVertDist;
		grcDebugDraw::Text(vTextRenderPos, Color_white, debugText);

		sprintf( debugText, "[1 Frame Max's] Events: %d; Peds: %d", ms_uiMaxEventsAdded, ms_uiMaxPedsChecked );
		vTextRenderPos.y = ScreenCoors.y + (iNoOfTexts++)*fVertDist;
		grcDebugDraw::Text(vTextRenderPos, Color_white, debugText);
#endif
	}

	if(RENDER_VECTOR_MAP)
	{
		CVectorMap::DrawCircle(Vector3(0.0f, 0.0f, 0.0f), 0.5f, Color_white, false);
	}

	int nEventsFound = 0;

	for(int i=0; i<num; i++ )
	{
		fwEvent* ev = list->GetEventByIndex(i);
		if(!ev || !static_cast<const CEvent*>(ev)->IsShockingEvent())
		{
			continue;
		}
		CEventShocking& shocking = *static_cast<CEventShocking*>(ev);

		CEventShocking* pEvent = &shocking;
		eEventType nEventType = (eEventType)pEvent->GetEventType();

		const CEventShocking::Tunables& tunables = pEvent->GetTunables();

		nEventsFound++;

		float visualReactionRange = tunables.m_VisualReactionRange;
		float audioReactionRange = tunables.m_AudioReactionRange;
		float fEventEffectRange = pEvent->m_fEventRadius;

		Vector3 vecEventPos;

		u32 uStart = pEvent->GetStartTime();

		if(tunables.m_DebugDisplayAlwaysUseEventPosition)
		{
			vecEventPos = RCC_VECTOR3(pEvent->m_vEventPosition);
		}
		else
		{
			if(pEvent->m_pEntitySource)
				vecEventPos = VEC3V_TO_VECTOR3(pEvent->m_pEntitySource->GetTransform().GetPosition());
			else
				vecEventPos = RCC_VECTOR3(pEvent->m_vEventPosition);
		}

		if( RENDER_VECTOR_MAP )
		{
			switch(tunables.m_Priority)
			{
				case EInteresting:
					{
						if(RENDER_SHOCK_EVENT_LEVEL_INTERESTING)
						{
							if(RENDER_EVENT_EFFECT_AREA)
							{
								CVectorMap::DrawCircle(vecEventPos, fEventEffectRange, Color_white, false);
							}
							else
							{
								CVectorMap::DrawCircle(vecEventPos, audioReactionRange, Color_white, false);
								CVectorMap::DrawCircle(vecEventPos, visualReactionRange, Color_grey80, false);
							}				
							CVectorMap::DrawCircle(vecEventPos, 1.0f, Color_white, false);
						}
					}
					break;

				case EAffectsOthers:
					{
						if(RENDER_SHOCK_EVENT_LEVEL_AFFECTSOTHERS)
						{
							if(RENDER_EVENT_EFFECT_AREA)
							{
								CVectorMap::DrawCircle(vecEventPos, fEventEffectRange, Color_blue, false);
							}
							else
							{
								CVectorMap::DrawCircle(vecEventPos, audioReactionRange, Color_blue, false);
								CVectorMap::DrawCircle(vecEventPos, visualReactionRange, Color_navy, false);
							}				
							CVectorMap::DrawCircle(vecEventPos, 1.0f, Color_blue, false);
						}
					}
					break;

				case EPotentiallyDangerous:
					{
						if(RENDER_SHOCK_EVENT_LEVEL_POTENTIALLYDANGEROUS)
						{
							if(RENDER_EVENT_EFFECT_AREA)
							{
								CVectorMap::DrawCircle(vecEventPos, fEventEffectRange, Color_green, false);
							}
							else
							{
								CVectorMap::DrawCircle(vecEventPos, audioReactionRange, Color_green, false);
								CVectorMap::DrawCircle(vecEventPos, visualReactionRange, Color_DarkGreen, false);
							}		
							CVectorMap::DrawCircle(vecEventPos, 1.0f, Color_green, false);
						}
					}
					break;

				case ESeriousDanger:
					{
						if(RENDER_SHOCK_EVENT_LEVEL_SERIOUSDANGER)
						{
							if(RENDER_EVENT_EFFECT_AREA)
							{
								CVectorMap::DrawCircle(vecEventPos, fEventEffectRange, Color_red, false);
							}
							else
							{
								CVectorMap::DrawCircle(vecEventPos, audioReactionRange, Color_red, false);
								CVectorMap::DrawCircle(vecEventPos, visualReactionRange, Color_OrangeRed, false);
							}	
							CVectorMap::DrawCircle(vecEventPos, 1.0f, Color_red, false);
						}
					}
					break;

				default:
					break;
			}
		}
		if(RENDER_AS_LIST)
		{
			Color32 txtCol;

			switch(tunables.m_Priority)
			{
				case EInteresting:
					txtCol = Color_white;
					break;

				case EAffectsOthers:
					txtCol = Color_blue;
					break;

				case EPotentiallyDangerous:
					txtCol = Color_green;
					break;

				case ESeriousDanger:
					txtCol = Color_red;
					break;

				default:
					break;
			}

			const char* eventName = CEventNames::GetEventName(nEventType);

			const char* pScriptInfo = "";
			if(pEvent->GetCreatedByScript())
			{
				pScriptInfo = ", SCRIPT";
			}

			if(tunables.m_DebugDisplayListPlayerInfo
					&& pEvent->m_pEntitySource
					&& pEvent->m_pEntitySource->GetIsTypePed()
					&& static_cast<CPed*>(pEvent->m_pEntitySource.Get())->IsPlayer())
			{
				formatf( debugText, "%d:%s: (%.2f, %.2f, %.2f), Created at time %u PLAYER%s", nEventsFound, eventName, vecEventPos.x, vecEventPos.y, vecEventPos.z, uStart, pScriptInfo );
			}
			else
			{
				formatf( debugText, "%d:%s: (%.2f, %.2f, %.2f), Created at time %u %s", nEventsFound, eventName, vecEventPos.x, vecEventPos.y, vecEventPos.z, uStart, pScriptInfo );
			}
			
			vTextRenderPos.y = ScreenCoors.y + (iNoOfTexts++)*fVertDist;
			grcDebugDraw::Text(vTextRenderPos, txtCol, debugText);
		}
		if( RENDER_ON_3D_SCREEN )
		{
			switch(tunables.m_Priority)
			{
				case EInteresting:
					{
						if(RENDER_SHOCK_EVENT_LEVEL_INTERESTING)
						{
							if(RENDER_EVENT_EFFECT_AREA)
							{
								grcDebugDraw::Sphere( vecEventPos, fEventEffectRange, Color_white, false );
							}
							else
							{
								grcDebugDraw::Sphere( vecEventPos, audioReactionRange, Color_white, false );
								grcDebugDraw::Sphere( vecEventPos, visualReactionRange, Color_grey, false );
							}
							grcDebugDraw::Sphere( vecEventPos, 1.0f, Color_white, true );
						}
					}
					break;

				case EAffectsOthers:
					{
						if(RENDER_SHOCK_EVENT_LEVEL_AFFECTSOTHERS)
						{
							if(RENDER_EVENT_EFFECT_AREA)
							{
								grcDebugDraw::Sphere( vecEventPos, fEventEffectRange, Color_blue, false );
							}
							else
							{
								grcDebugDraw::Sphere( vecEventPos, audioReactionRange, Color_blue, false );
								grcDebugDraw::Sphere( vecEventPos, visualReactionRange, Color_cyan1, false );
							}
							grcDebugDraw::Sphere( vecEventPos, 1.0f, Color_blue, true );
						}
					}
					break;

				case EPotentiallyDangerous:
					{
						if(RENDER_SHOCK_EVENT_LEVEL_POTENTIALLYDANGEROUS)
						{
							if(RENDER_EVENT_EFFECT_AREA)
							{
								grcDebugDraw::Sphere( vecEventPos, fEventEffectRange, Color_green, false );
							}
							else
							{
								grcDebugDraw::Sphere( vecEventPos, audioReactionRange, Color_green, false );
								grcDebugDraw::Sphere( vecEventPos, visualReactionRange, Color_yellow4, false );
							}
							grcDebugDraw::Sphere( vecEventPos, 1.0f, Color_green, true );
						}
					}
					break;

				case ESeriousDanger:
					{
						if(RENDER_SHOCK_EVENT_LEVEL_SERIOUSDANGER)
						{
							if(RENDER_EVENT_EFFECT_AREA)
							{
								if(nEventType == EVENT_SHOCKING_EXPLOSION || nEventType == EVENT_SHOCKING_GUN_FIGHT)
								{
									grcDebugDraw::Sphere( vecEventPos, fEventEffectRange, Color_red, false );
								}	
							}
							else
							{
								grcDebugDraw::Sphere( vecEventPos, audioReactionRange, Color_red, false );
								grcDebugDraw::Sphere( vecEventPos, visualReactionRange, Color_orange, false );
							}
							grcDebugDraw::Sphere( vecEventPos, 1.0f, Color_red, true );
						}
					}
					break;

				default:
					break;
			}
		}
	}
}

#endif	// __BANK

#if 0	// For reference, old function for converting weapon type.

eSEventTypes CShockingEventsManager::ConvertVisibleWeaponEType(u32 UNUSED_PARAM(nWeaponHash))
{
	// CS - Removed because of hard coded WeaponType
// 	if(nWeaponHash==WEAPONTYPE_UNARMED || nWeaponHash == 0)
// 		return EVisibleWeapon;
// 
// 	const CWeaponInfo *pWeaponInfo = CWeaponInfoManager::GetItemInfo<CWeaponInfo>(nWeaponHash);
// 	u32 iWeaponSlot = pWeaponInfo->GetSlot();
// 	if(iWeaponSlot == WEAPONSLOT_MELEE)
// 	{
// 		return EVisibleWeaponMELEE;
// 	}
// 	else if(iWeaponSlot == WEAPONSLOT_HANDGUN)
// 	{
// 		return EVisibleWeaponHANDGUN;
// 	}
// 	else if(iWeaponSlot == WEAPONSLOT_SHOTGUN)
// 	{
// 		return EVisibleWeaponSHOTGUN;
// 	}
// 	else if(iWeaponSlot == WEAPONSLOT_SMG)
// 	{
// 		return EVisibleWeaponSMG;
// 	}
// 	else if(iWeaponSlot == WEAPONSLOT_RIFLE)
// 	{
// 		return EVisibleWeaponRIFLE;
// 	}
// 	else if(iWeaponSlot == WEAPONSLOT_SNIPER)
// 	{
// 		return EVisibleWeaponSNIPER;
// 	}
// 	else if(iWeaponSlot == WEAPONSLOT_HEAVY)
// 	{
// 		return EVisibleWeaponHEAVY;
// 	}
// 	else if(iWeaponSlot == WEAPONSLOT_THROWN)
// 	{
// 		return EVisibleWeaponTHROWN;
// 	}
// 	else
	{
		return EVisibleWeapon;
	}
}

#endif

//-----------------------------------------------------------------------------
