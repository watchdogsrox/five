
//rage headers
#include "fwscene/world/InteriorLocation.h"
#include "math/vecmath.h"
#include "spatialdata/aabb.h"

// Game headers
#include "AI/stats.h"
#include "Event\Event.h"
#include "Event\Events.h"
#include "Event/EventMovement.h"	// CEventClimbNavMeshOnRoute, for largest event size calculations.
#include "Event\EventNames.h"
#include "Event\EventResponseFactory.h"
#include "Event/EventShocking.h"
#include "Event/EventSound.h"
#include "Network/NetworkInterface.h"
#include "Peds/Ped.h"
#include "Peds\PedIntelligence.h"
#include "scene/EntityIterator.h"
#include "scene/portals/InteriorInst.h"
#include "scene/portals/InteriorProxy.h"
#include "scene/portals/PortalInst.h"
#include "Task\Combat\TaskCombat.h"
#include "Task\Combat\TaskThreatResponse.h"
#include "Task\Movement\TaskCollisionResponse.h"
#include "Task\General\TaskBasic.h"
#include "Task\Response\TaskFlee.h"
#include "Task\General\TaskSecondary.h"
#include "vehicleAi\VehicleIntelligence.h"

using namespace AIStats;


AI_OPTIMISATIONS()

//////////////////////////////////////////////////////////////////////////
// Construct CEvent pool
//////////////////////////////////////////////////////////////////////////

#define LARGEST_EVENT_CLASS sizeof(CEventShocking)	// This appears to be the largest event now.

// #include "system/findsize.h"
// 
// FindSize(CEvent);
// FindSize(CEventShocking);
// FindSize(CEventClimbNavMeshOnRoute);

FW_INSTANTIATE_BASECLASS_POOL_SPILLOVER(CEvent, CONFIGURED_FROM_FILE, 0.55f, atHashString("CEvent",0xb7ed840d), LARGEST_EVENT_CLASS);

CompileTimeAssert(sizeof(CEvent)					<= LARGEST_EVENT_CLASS);
CompileTimeAssert(sizeof(CEventShocking)			<= LARGEST_EVENT_CLASS);
// CompileTimeAssert(sizeof(CEventClimbNavMeshOnRoute) <= LARGEST_EVENT_CLASS);

#if __BANK
bool CEvent::sm_bInPoolFullCallback = false;
#endif // __BANK

//////////////////////////////////////////////////////////////////////////
// CEvent
//////////////////////////////////////////////////////////////////////////

CEvent::CEvent()
: m_fDelayTimer(0.0f)
, m_bIsDelayInitialized(false)
, m_bIsShockingEvent(false)
DEV_ONLY(, m_fInitialDelay(0.0f))
{
#if !__FINAL
	m_EventType = (EVENT_NONE);
#endif
}

CEvent::~CEvent()
{
#if __BANK
	Assert(!sm_bInPoolFullCallback);
#endif // __BANK
}

bool CEvent::CanCreateEvent(u32 nFreeSpaces)
{
	if (!CEvent::GetPool())
	{
		return false;
	}

	if (CEvent::GetPool()->GetNoOfFreeSpaces() < nFreeSpaces)
	{
#if __DEV
		static bool sbDumped = false;
		// dump event pool. Only do this once.
		if (!sbDumped)
		{
			_ms_pPool->PoolFullCallback();
			sbDumped = true;
		}
#endif // __DEV

		aiAssertf(0, "CEvent pool full");
		
		return false;
	}

	return true;
}

bool CEvent::IsEventReady(CPed* pPed)
{
	//Check if we have not initialized the delay.
	if(!IsDelayInitialized())
	{
		//Initialize the delay.
		InitializeDelay(pPed);
	}
	else
	{
		//Update the delay.
		UpdateDelay();
	}

	//Check if we should delay.
	bool bShouldDelay = ShouldDelay();
	return !bShouldDelay;
}

bool CEvent::HasPriority(const fwEvent& otherEvent) const
{
	const CEvent& other = static_cast<const CEvent&>(otherEvent);

#if __ASSERT

	if (TakesPriorityOver(other))
	{
		if (other.RequiresAbortForRagdoll() && !RequiresAbortForRagdoll() && GetTargetPed() && GetTargetPed()->GetUsingRagdoll())
		{
			Displayf("Non ragdoll event %s is higher priority than ragdoll event %s. This could cause an unmanaged ragdoll. entity: %s, time: %d frame: %d", GetName().c_str(), other.GetName().c_str(), GetTargetPed() ? GetTargetPed()->GetModelName() : "NULL", fwTimer::GetTimeInMilliseconds(), fwTimer::GetFrameCount());
		}
	}
	else
	{
		if (RequiresAbortForRagdoll() && !other.RequiresAbortForRagdoll() && GetTargetPed() && GetTargetPed()->GetUsingRagdoll())
		{
			Displayf("Non ragdoll event %s is higher priority than ragdoll event %s. This could cause an unmanaged ragdoll.  entity: %s, time: %d frame: %d", other.GetName().c_str(), GetName().c_str(), GetTargetPed() ? GetTargetPed()->GetModelName() : "NULL", fwTimer::GetTimeInMilliseconds(), fwTimer::GetFrameCount());
		}	
	}

#endif //__ASSERT
	// CScriptedPriorities has been disabled, could be re-enabled if needed.
#if 0
	// If script has forced one event to take priority over another event then defer to the script.
	if(CScriptedPriorities::TakesPriorityOver(GetEventType(), other.GetEventType()))
	{
		return true;
	}
	else
	{
		return TakesPriorityOver(other);
	}
#endif	// 0
	return TakesPriorityOver(other);
}

bool CEvent::CreateResponseTask(CPed* pPed, CEventResponse& response) const
{
	CEventResponseFactory::CreateEventResponse(pPed, this, response);
	return response.GetEventResponse(CEventResponse::EventResponse) != NULL;
}

CTask* CEvent::CreateVehicleReponseTaskWithParams(sVehicleMissionParams& /*paramsOut*/) const
{
	return NULL;
}

CPed* CEvent::GetSourcePed() const
{
	CEntity* pSourceEntity = GetSourceEntity();
	if(pSourceEntity && pSourceEntity->GetType() == ENTITY_TYPE_PED)
	{
		return static_cast<CPed*>(pSourceEntity);
	}

	return NULL;
}

CPed* CEvent::GetTargetPed() const
{
	CEntity* pTargetEntity = GetTargetEntity();
	if(pTargetEntity && pTargetEntity->GetType() == ENTITY_TYPE_PED)
	{
		return static_cast<CPed*>(pTargetEntity);
	}

	return NULL;
}

bool CEvent::AffectsPed(CPed* pPed) const
{
	if(!AffectsPedCore(pPed))
	{
		return false;
	}

	return true;
}

bool CEvent::AffectsVehicle(CVehicle* pVehicle) const
{
	if(!AffectsVehicleCore(pVehicle))
	{
		return false;
	}

	return true;
}

void CEvent::OnEventAdded(CPed* pPed) const
{
	if (RequiresAbortForRagdoll())
	{
		// Make sure the new event is processed next frame
		pPed->GetPedAiLod().SetForceNoTimesliceIntelligenceUpdate(true);
	}
}

void CEvent::CommunicateEvent(CPed* pPed, bool bInformRespectedPedsOnly, bool bUseRadioIfPossible) const
{
	//Create the input.
	CommunicateEventInput input;
	input.m_bInformRespectedPedsOnly = bInformRespectedPedsOnly;
	input.m_bUseRadioIfPossible = bUseRadioIfPossible;
	
	//Communicate the event.
	CommunicateEvent(pPed, input);
}

void CEvent::CommunicateEvent(CPed* pPed, const CommunicateEventInput& rInput) const
{
	Assert(pPed);
	
	//Determine whether the radio should be used.
	bool bUseRadio = (rInput.m_bUseRadioIfPossible && pPed->GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_HasRadio));

	// Communicate the event
	float fMaxCommunicationDistance = (bUseRadio && pPed->IsLawEnforcementPed()) ? rInput.m_fMaxRangeWithRadio : rInput.m_fMaxRangeWithoutRadio;
	Vec3V v = pPed->GetTransform().GetPosition();

	int maxNumPeds = CPed::GetPool()->GetSize();
	CSpatialArray::FindResult* results = Alloca(CSpatialArray::FindResult, maxNumPeds);
	int numFound = CPed::ms_spatialArray->FindInSphere(v, fMaxCommunicationDistance, results, maxNumPeds);

	for(int i = 0; i < numFound; i++)
	{
		CPed* pFoundPed = CPed::GetPedFromSpatialArrayNode(results[i].m_Node);

		// If we only want to inform respected peds and we don't respect this ped, don't communicate the event to them
		bool bRespectsPed = (pPed->GetPedIntelligence()->IsFriendlyWith(*pFoundPed) ||
			(rInput.m_bInformRespectedPedsDueToScenarios && pPed->GetPedIntelligence()->IsFriendlyWithDueToScenarios(*pFoundPed)) ||
			(rInput.m_bInformRespectedPedsDueToSameVehicle && pPed->GetPedIntelligence()->IsFriendlyWithDueToSameVehicle(*pFoundPed)));

		if(rInput.m_bInformRespectedPedsOnly && !bRespectsPed)
		{
			continue;
		}

		bool bCanCreate = false;

		// If we don't have a radio then attempt to communicate the event,
		// this takes into account whether the recipient will hear the event
		CEvent* pEventToAdd = NULL;
		if(bUseRadio && pFoundPed->GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_HasRadio) &&
			!pFoundPed->GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_UseMaxSenseRangeWhenReceivingEvents))
		{
			if (CEvent::CanCreateEvent())
			{
				pEventToAdd = static_cast<CEvent*>(Clone());
				bCanCreate = true;
			}
		}
		else
		{
			if (CEvent::CanCreateEvent(2))
			{
				pEventToAdd = rage_new CEventCommunicateEvent(pPed, static_cast<CEvent*>(Clone()), fMaxCommunicationDistance);
				bCanCreate = true;
			}
		}

		if (bCanCreate && !CInformFriendsEventQueue::Add(pFoundPed, pEventToAdd))
		{
			delete pEventToAdd;
		}
	}
}

// Ignore gunshots if they occur in a different interior state than the sensing ped.
bool CEvent::CanEventBeIgnoredBecauseOfInteriorStatus(const CPed& ped, const CEntity* pSourceEntity, Vec3V_In vSourcePos)
{
	// Track this since checking the interior status like this is potentially expensive if lots of events happen at the same time.
	PF_FUNC(CanEventBeIgnoredBecauseOfInteriorStatus);

	// return false if we're disabling this check
	if(ped.GetPedConfigFlag(CPED_CONFIG_FLAG_DisableEventInteriorStatusCheck))
	{
		return false;
	}

	if (pSourceEntity)
	{
		return CanEventWithSourceBeIgnoredBecauseOfInteriorStatus(ped, *pSourceEntity);
	}
	else
	{
		return CanEventWithNoSourceBeIgnoredBecauseOfInteriorStatus(ped, vSourcePos);
	}
}

bool CEvent::CanEventWithSourceBeIgnoredBecauseOfInteriorStatus(const CPed& ped, const CEntity& source)
{
	const fwInteriorLocation sensingInterior = ped.GetInteriorLocation();
	ScalarV vCloseToPortalDist(V_FOUR);

	// Events can not be ignored if the entities are in the same location.
	if (sensingInterior.IsSameLocation(source.GetInteriorLocation()))
	{
		return false;
	}

	// Make sure the source is in an exterior.
	if (!source.GetIsInExterior())
	{
		return false;
	}

	//  Check if the source entity is close to portals leading from the interior of the sensing ped to the exterior.
	CInteriorProxy* pProxy = CInteriorProxy::GetFromLocation(sensingInterior);
	if (pProxy)
	{
		CInteriorInst* pInst = pProxy->GetInteriorInst();
		if (pInst)
		{
			// Obtain a list of portals that lead to room zero (outside).
			const atArray<CPortalInst*>& portalInsts = pInst->GetRoomZeroPortalInsts();
			for (s32 i=0; i < portalInsts.GetCount(); i++)
			{
				CPortalInst* pPortalInst = portalInsts[i];
				if (pPortalInst)
				{
					// Check the distance.
					const Vec3V vPortalPos = pPortalInst->GetTransform().GetPosition();
					const ScalarV sourceDistSqd = DistSquared(vPortalPos, source.GetTransform().GetPosition());
					if (IsLessThanOrEqualAll(sourceDistSqd, vCloseToPortalDist))
					{
						return false;
					}
				}
			}

			// This event can be ignored as the source is not close to any of the portals leading outside.
			return true;
		}
	}

	// The event can not be ignored as the interior status of the sensing ped could not be determined.
	return false;
}

bool CEvent::CanEventWithNoSourceBeIgnoredBecauseOfInteriorStatus(const CPed& ped, Vec3V_In vPos)
{
	const fwInteriorLocation sensingInterior = ped.GetInteriorLocation();

	// No source entity - check the position instead.
	if (sensingInterior.IsValid())
	{
		CInteriorProxy* pProxy = CInteriorProxy::GetFromLocation(sensingInterior);
		if (pProxy)
		{
			CInteriorInst* pInst = pProxy->GetInteriorInst();
			if (pInst)
			{
				// Check if the AABB of the sensing ped's interior contains the position of the event.
				spdAABB bounds;
				pInst->GetAABB(bounds);
				return !bounds.ContainsPoint(vPos);
			}
		}
	}

	// If the ped is not in a valid interior the event cannot be ignored.
	return false;
}

// If the source entity has a valid pointer, return its position.
// Otherwise, return the source position of the event.
Vec3V_Out CEvent::GetEntityOrSourcePosition() const
{
	CEntity* pSourceEntity = GetSourceEntity();
	if (pSourceEntity)
	{
		return pSourceEntity->GetTransform().GetPosition();
	}
	else
	{
		return VECTOR3_TO_VEC3V(GetSourcePos());
	}
}


#if __BANK
template<> void fwPool<CEvent>::PoolFullCallback() 
{
	CEvent::sm_bInPoolFullCallback = true;

	s32 size = GetSize();
	int iIndex = 0;
	while(size--)
	{
		CEvent* pEvent = GetSlot(size);
		if(pEvent)
		{
			s32* p = reinterpret_cast<s32*>(pEvent);
			if(Verifyf(*p != 0xffffffff, "Dodgy event, skipping"))
			{
				const CPed* pOwnerPed = NULL;
				const CVehicle* pOwnerVehicle = NULL;
				bool bInGlobalQueue = false;
				bool bInScriptAIQueue = false;
				bool bInScriptNetworkQueue = false;
				bool bInInformFriendsQueue = false;

				{
					CPed::Pool* pPedPool = CPed::GetPool();
					s32 i = pPedPool->GetSize();
					while(i-- && pOwnerPed==NULL)
					{
						CPed* pPed = pPedPool->GetSlot(i);
						if(pPed && pPed->GetPedIntelligence()->GetEventGroup().HasEvent(*pEvent))
						{
							pOwnerPed = pPed;
						}
					}
				}

				if(!pOwnerPed)
				{
					CVehicle::Pool* pVehiclePool = CVehicle::GetPool();
					s32 i = (s32) pVehiclePool->GetSize();
					while(i-- && pOwnerVehicle==NULL)
					{
						CVehicle* pVeh = pVehiclePool->GetSlot(i);
						if(pVeh && pVeh->GetIntelligence()->GetEventGroup().HasEvent(*pEvent))
						{
							pOwnerVehicle = pVeh;
						}
					}
				}

				if(!pOwnerPed && !pOwnerVehicle)
				{
					bInGlobalQueue = GetEventGlobalGroup()->HasEvent(*pEvent);
					bInScriptAIQueue = GetEventScriptAIGroup()->HasEvent(*pEvent);
					bInScriptNetworkQueue = GetEventScriptNetworkGroup()->HasEvent(*pEvent);
					bInInformFriendsQueue = CInformFriendsEventQueue::HasEvent(pEvent);
				}

				Vec3V vPosition = pEvent->GetEntityOrSourcePosition();
				Displayf("%i, %s, lifetime:%i, accum_time:%i, source_entity:%p, player:%d position:(%f %f %f), target_entity:%p, owner_ped:%p, owner_veh:%p, global:%d, scriptAI:%d, scriptNet:%d, informfriends:%d",
					iIndex,
					pEvent->GetName().c_str(),
					pEvent->GetLifeTime(),
					pEvent->GetAccumulatedTime(),
					pEvent->GetSourceEntity(),
					(pEvent->GetSourceEntity() && pEvent->GetSourceEntity()->GetIsTypePed()) ? static_cast<CPed*>(pEvent->GetSourceEntity())->IsAPlayerPed() : 0,
					vPosition.GetXf(), vPosition.GetYf(), vPosition.GetZf(),
					pEvent->GetTargetEntity(),
					pOwnerPed,
					pOwnerVehicle,
					bInGlobalQueue,
					bInScriptAIQueue,
					bInScriptNetworkQueue,
					bInInformFriendsQueue);
			}
		}
		else
		{
			Displayf("%i, NULL event", iIndex);
		}
		iIndex++;
	}

	CEvent::sm_bInPoolFullCallback = false;
}
#endif // __BANK

#if !__FINAL

bool CEvent::VerifyEventCountCallback( void* pItem, void * )
{
	// Are we allocated out?
	if ( pItem != NULL )
	{
#if EVENT_EXTENDED_REF_COUNT_INFO
		CEvent* pEvent = static_cast<CEvent*>(pItem);
		pEvent->VerifyIntegrity( true );
#else
#if __ASSERT
		CEvent* pEvent = static_cast<CEvent*>(pItem);

#if __CATCH_EVENT_LEAK
		if(!pEvent->IsReferenced() && pEvent->m_EventType == EVENT_SWITCH_2_NM_TASK)
		{
			CEventSwitch2NM::ms_bCatchEventLeak = true;	// switch this on

			CEventSwitch2NM * pEventNM = (CEventSwitch2NM*)pEvent;
			if(pEventNM->m_pCreationStackTrace)
			{
				Displayf("*************************************************************************");
				Displayf("%s",pEventNM->m_pCreationStackTrace);
				Displayf("*************************************************************************");

				Assertf( pEvent->IsReferenced(), "EVENT_SWITCH_2_NM_TASK has been orphaned (url:bugstar:1371503).  Please add your TTY to this bug.");
			}
			else
			{
				Assertf( pEvent->IsReferenced(), "EVENT_SWITCH_2_NM_TASK has been orphaned (url:bugstar:1371503).\nStack tracing has been switched on - please continue playing in case this assert fires again..");
			}
		}
		else
#endif
		{
			Assertf( pEvent->IsReferenced(), "Event %d \"%s\" has been orphaned.\n", pEvent->m_EventType, pEvent->GetName().c_str());
		}
#endif
#endif
	}

	// Go through them all
	return true;
}

void CEvent::VerifyEventCounts()
{
	Pool *pool = GetPool();
	if( pool )
	{
		pool->ForAll( &VerifyEventCountCallback, NULL );
	}
}

#endif

void CEvent::InitializeDelay(CPed* pPed)
{
	//Note that the delay has been initialized.
	m_bIsDelayInitialized = true;

	//Clear the delay.
	m_fDelayTimer = 0.0f;

	//Check if we have a tunable delay.
	float fMin;
	float fMax;
	if(GetTunableDelay(pPed, fMin, fMax))
	{
		//Set the delay timer.
		m_fDelayTimer = (fwRandom::GetRandomNumberInRange(fMin, fMax));

		//Check if the ped is on-screen, a mission ped, and we are in MP.
		//This should probably be enabled for everyone on-screen, but it is very late to be affecting all peds.
		if(pPed && pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_VisibleOnScreen) && pPed->PopTypeIsMission() &&
			NetworkInterface::IsGameInProgress())
		{
			//Cache off data about the last delay.
			static u32 s_uLastTime		= 0;
			static int s_iLastType		= EVENT_INVALID;
			static float s_fLastDelay	= 0.0f;
			static float s_fThreshold	= 0.1f;

			//Check if this event matches, and the delay is too close.
			u32 uTime = fwTimer::GetTimeInMilliseconds();
			int iType = GetEventType();
			if((uTime == s_uLastTime) && (iType == s_iLastType) &&
				(Abs(m_fDelayTimer - s_fLastDelay) < s_fThreshold))
			{
				//Modify the delay timer.
				if(m_fDelayTimer > s_fLastDelay)
				{
					m_fDelayTimer += s_fThreshold;
				}
				else
				{
					m_fDelayTimer -= s_fThreshold;
				}
			}

			//Cache off the delay data.
			s_uLastTime		= uTime;
			s_iLastType		= iType;
			s_fLastDelay	= m_fDelayTimer;
		}
	}

	//Set the initial delay.
	DEV_ONLY(m_fInitialDelay = m_fDelayTimer);
}

void CEvent::UpdateDelay()
{
	//Ensure the delay timer is valid.
	if(m_fDelayTimer <= 0.0f)
	{
		return;
	}

	//Decrease the delay.
	m_fDelayTimer -= GetTimeStep();
	if(m_fDelayTimer > 0.0f)
	{
		return;
	}

	//Reset the accumulated time (so we don't expire immediately).
	ResetAccumulatedTime();
}
