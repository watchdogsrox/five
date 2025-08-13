#include "Events.h"

// Framework Headers
#include "fwmaths/angle.h"
#include "pharticulated/articulatedcollider.h"
#include "fragmentnm/nm_channel.h "

//game headers
#include "animation/AnimBones.h"
#include "animation/AnimDefines.h"
#include "animation/debug/AnimViewer.h"
#include "event/EventDamage.h"
#include "Event/EventHandler.h"
#include "Event/EventMovement.h"
#include "Event/EventShocking.h"
#include "event/EventSound.h"
#include "Event/EventWeapon.h"
#include "game/cheat.h"
#include "game/crime.h"
#include "game/Dispatch/Incidents.h"
#include "game/dispatch/Orders.h"
#include "game/localisation.h"
#include "modelinfo/VehicleModelInfo.h"
#include "network/NetworkInterface.h"
#include "network/Events/NetworkEventTypes.h"
#include "objects/door.h"
#include "pedgroup/PedGroup.h"
#include "peds/Ped.h"
#include "peds/PedCapsule.h"
#include "peds/PedGeometryAnalyser.h"
#include "peds/PedIntelligence.h"
#include "peds/PedMoveBlend/PedMoveBlendOnFoot.h"
#include "Peds/PedTaskRecord.h"
#include "Peds/pedType.h"
#include "peds/Ped.h"
#include "peds/PedWeapons/PedTargeting.h"
#include "peds/pedpopulation.h"
#include "physics/GtaArchetype.h"
#include "physics/GtaInst.h"
#include "physics/WorldProbe/worldprobe.h"
#include "scene/Building.h"
#include "script/Script.h"
#include "script/script_cars_and_peds.h"
#include "streaming/SituationalClipSetStreamer.h"
#include "system/Timer.h"
#include "Task/Default/Patrol/TaskPatrol.h"
#include "task/System/Task.h"
#include "task/Animation/TaskAnims.h"
#include "task/Animation/TaskScriptedAnimation.h"
#include "task/Combat/TaskThreatResponse.h"
#include "Task/Combat/TaskReact.h"
#include "task/General/TaskBasic.h"
#include "task/General/Phone/TaskMobilePhone.h"
#include "Task/Vehicle/TaskCar.h"
#include "Task/Vehicle/TaskCarAccessories.h"
#include "Task/Vehicle/TaskGoToVehicleDoor.h"
#include "Task/Vehicle/TaskCarUtils.h"
#include "Task/Movement/TaskCollisionResponse.h"
#include "Task/Movement/TaskMoveWander.h"
#include "Task/Movement/TaskMoveFollowEntityOffset.h"
#include "Task/Movement/TaskMoveFollowLeader.h"
#include "Task/Movement/TaskNavMesh.h"
#include "Task/Movement/TaskGoto.h"
#include "Task/Movement/TaskFollowWaypointRecording.h"
#include "Task/Movement/TaskParachute.h"
#include "Task/Movement/TaskFall.h"
#include "Task/Movement/TaskSeekEntity.h"
#include "Task/Movement/Jumping/TaskJump.h"
#include "Task/Combat/TaskNewCombat.h"
#include "task/Combat/TaskCombat.h"
#include "Task/Combat/Cover/TaskCover.h"
#include "Task/Combat/TaskInvestigate.h"
#include "task/Combat/TaskThreatResponse.h"
#include "Task/Physics/TaskNM.h"
#include "Task/Physics/TaskNMBalance.h"
#include "Task/Physics/TaskNMJumpRollFromRoadVehicle.h"
#include "Task/Response/Info/AgitatedManager.h"
#include "Task/Response/TaskSidestep.h"
#include "Task/Vehicle/TaskEnterVehicle.h"
#include "Task/Vehicle/TaskExitVehicle.h"
#include "task/Default/TaskPlayer.h"
#include "task/System/TaskTypes.h"
#include "Task/Response/TaskAgitated.h"
#include "Task/Response/TaskReactToExplosion.h"
#include "Task/Response/TaskShockingEvents.h"
#include "Task/Scenario/Types/TaskCowerScenario.h"
#include "vehicleAi/task/TaskVehicleCruise.h"
#include "vehicleAi/task/TaskVehicleTempAction.h"
#include "vehicles/automobile.h"
#include "vehicles/Bike.h"
#include "vehicles/Boat.h"
#include "Vehicles/Metadata/VehicleMetadataManager.h"
#include "Vehicles/Metadata/VehicleSeatInfo.h"
#include "audio/policescanner.h"
#include "task/Response/TaskFlee.h"
#include "Task/Combat/TaskDamageDeath.h"
#include "Task/Weapons/TaskSwapWeapon.h"
#include "Task/Combat/Cover/TaskSeekCover.h"
#include "Task/Combat/TaskCombatMelee.h"
#include "Task/Movement/Jumping/TaskFallGetUp.h"
#include "Task/Motion/Locomotion/TaskLocomotion.h"
#include "Task/Motion/TaskMotionBase.h"
#include "Peds/PopCycle.h"
#include "Task/Response/TaskGangs.h"
#include "Task/Response/TaskReactToDeadPed.h"
#include "Task/Vehicle/TaskCarCollisionResponse.h"
#include "Vehicles/Heli.h"
#include "Vfx/Misc/Fire.h"

#include "event/Events_parser.h"

AI_OPTIMISATIONS()
AI_EVENT_OPTIMISATIONS()
AI_COVER_OPTIMISATIONS()
/////////////////////
//EDITABLE RESPONSE EVENT
/////////////////////

CInformFriendsEvent::CInformFriendsEvent()
: m_pPedToInform(0),
m_pEvent(0),
m_informTime(0)
{

}

CInformFriendsEvent::~CInformFriendsEvent()
{
	Flush();
}

void CInformFriendsEvent::Flush()
{
	m_pPedToInform=0;

	if(m_pEvent)
	{
		delete m_pEvent;
		m_pEvent=0;
	}

	m_informTime=0;
}

void CInformFriendsEvent::Set(CPed* pPed, CEvent* pEvent, const int iTime)
{
	//Set the ped.
	m_pPedToInform=pPed;

	//Set the event.
	m_pEvent=pEvent;

	//Set the time.
	m_informTime=fwTimer::GetTimeInMilliseconds()+iTime;
}

void CInformFriendsEvent::Process()
{
	if(0==m_pPedToInform)
	{
		//Ped must have been deleted so flush.
		Flush();
	}
	else if(m_informTime < fwTimer::GetTimeInMilliseconds())
	{
		//Time to inform the ped about the event.
		// NOTE: This has no effect on network clones
		m_pPedToInform->GetPedIntelligence()->AddEvent(*m_pEvent);
		
		// If this is a network game
		if( NetworkInterface::IsGameInProgress() )
		{
			// and the ped to inform is a network clone
			if( m_pPedToInform->IsNetworkClone() )
			{
				// check for communication event
				if( m_pEvent->GetEventType() == EVENT_COMMUNICATE_EVENT )
				{
					const CEventCommunicateEvent* pCommunicateEvent = static_cast<const CEventCommunicateEvent*>(m_pEvent.Get());
					if(pCommunicateEvent && pCommunicateEvent->CanBeCommunicatedTo(m_pPedToInform))
					{
						// Check for and handle CEventRespondedToThreat across the network
						ProcessNetworkCommunicationEvent(pCommunicateEvent->GetEventToBeCommunicated());
					}
				}
				else if(m_pEvent->GetEventType() == EVENT_SHOUT_TARGET_POSITION)
				{
					// we need this event to go over the network even if the target ped has 
					// BF_HasRadio set and it's not a EVENT_COMMUNICATE_EVENT otherwise 
					// helicopter pilots won't get informed about target sightings and 
					// will give up the chase.
					ProcessNetworkCommunicationEvent(m_pEvent);
				}
			}
		}

		//Finished with this event now.
		Flush();
	}
}

void CInformFriendsEvent::ProcessNetworkCommunicationEvent(const CEvent* pEventToBeCommunicated)
{
	if( AssertVerify(pEventToBeCommunicated) )
	{
		// Check if the event to be communicated is a CEventRespondedToThreat
		if(pEventToBeCommunicated->GetEventType() == EVENT_RESPONDED_TO_THREAT)
		{
			const CEventRespondedToThreat* pEventRespondedToThreat = static_cast<const CEventRespondedToThreat*>(pEventToBeCommunicated);
			if( pEventRespondedToThreat )
			{
				const CPed* pRecipientPed = m_pPedToInform;
				const CPed* pResponderPed = pEventRespondedToThreat->GetPedBeingThreatened();
				const CPed* pThreatPed = pEventRespondedToThreat->GetPedBeingThreatening();
				if( pRecipientPed && pResponderPed && pThreatPed && NetworkUtils::IsNetworkCloneOrMigrating(pRecipientPed) )
				{
					// Check if the recipient has a hostile target
					const CEntity* pRecipientHostileTarget = pRecipientPed->GetPedIntelligence()->GetQueriableInterface()->GetHostileTarget();
					if( pRecipientHostileTarget )
					{
						// if the hostile target is the threatening ped
						if( pRecipientHostileTarget == pThreatPed )
						{
							// no need to transmit network message
							return;
						}
						
						// if the hostile target is player controlled
						if( pRecipientHostileTarget->GetIsTypePed() )
						{
							const CPed* pHostileTargetPed = static_cast<const CPed*>(pRecipientHostileTarget);
							if( pHostileTargetPed->IsControlledByLocalOrNetworkPlayer() )
							{
								// no need to transmit network message
								return;
							}
						}
					}

					// send message to recipient ped for this event
					CNetworkRespondedToThreatEvent::Trigger( pRecipientPed, pResponderPed, pThreatPed);
				}
			}
		}
		else if(pEventToBeCommunicated->GetEventType() == EVENT_SHOUT_TARGET_POSITION)
		{
			const CEventShoutTargetPosition* pEventShoutTargetPosition = static_cast<const CEventShoutTargetPosition*>(pEventToBeCommunicated);

			if(pEventShoutTargetPosition)
			{
				const CPed* pRecipientPed	= m_pPedToInform;
				const CPed* pPedShouting	= pEventShoutTargetPosition->GetPedThatNeedsHelp();
				const CPed* pTargetPed		= pEventShoutTargetPosition->GetTargetPed();

				if( pRecipientPed && pPedShouting && pTargetPed && NetworkUtils::IsNetworkCloneOrMigrating(pRecipientPed) )
				{
					CNetworkShoutTargetPositionEvent::Trigger( pRecipientPed, pPedShouting, pTargetPed );
				}
			}
		}
	}
}

CInformFriendsEvent CInformFriendsEventQueue::ms_informFriendsEvents[MAX_QUEUE_SIZE];

CInformFriendsEventQueue::CInformFriendsEventQueue()
{
}

CInformFriendsEventQueue::~CInformFriendsEventQueue()
{
}

void CInformFriendsEventQueue::Init()
{
	Flush();
}	

bool CInformFriendsEventQueue::Add(CPed* pPedToInform, CEvent* pEvent)
{
	Assert(pEvent);
	Assert(pPedToInform);

	int iEmptySlot=-1;
	int i;
	for(i=0;i<MAX_QUEUE_SIZE;i++)
	{
		//Check if the entry related  to pPedToInform.
		if(ms_informFriendsEvents[i].m_pPedToInform==pPedToInform)
		{
			//Entry has a ped ptr so it must have an event and an inform time.
			Assert(ms_informFriendsEvents[i].m_pEvent);
			Assert(ms_informFriendsEvents[i].m_informTime!=0);

			//Check if entry is the same kind of event as pEvent.
			if(pEvent->GetEventType()==ms_informFriendsEvents[i].m_pEvent->GetEventType())
			{
				// If this is a communication event, allow duplication if the events are different
				if( pEvent->GetEventType() == EVENT_COMMUNICATE_EVENT && 
					ms_informFriendsEvents[i].m_pEvent->GetEventType() == EVENT_COMMUNICATE_EVENT )
				{
					if( ((CEventCommunicateEvent*)pEvent)->GetEventToBeCommunicated()->GetEventType() ==
						((CEventCommunicateEvent*)(ms_informFriendsEvents[i].m_pEvent.Get()))->GetEventToBeCommunicated()->GetEventType() )
					{
						// Don't add this entry cos the event to be communicated is a duplicate
						return false;
					}
				}
				else
				{
					//Don't add an entry because it will just be a duplicate.
					return false;
				}
			}
		}
		else if(-1==iEmptySlot)
		{
			//Entry doesn't have a ped ptr.
			//Check if it is empty.
			if(0==ms_informFriendsEvents[i].m_pEvent)
			{
				Assert(0==ms_informFriendsEvents[i].m_informTime);
				iEmptySlot=i;
			}			
		}
	}

	if(iEmptySlot!=-1)
	{
		ms_informFriendsEvents[iEmptySlot].Set(pPedToInform,pEvent,0);//fwRandom::GetRandomNumberInRange(MIN_INFORM_TIME,MAX_INFORM_TIME));
		return true;
	}


	return false;
}

void CInformFriendsEventQueue::Flush()
{
	int i;
	for(i=0;i<MAX_QUEUE_SIZE;i++)
	{
		ms_informFriendsEvents[i].Flush();
	}
}

bool CInformFriendsEventQueue::HasEvent(const CEvent* pEvent)
{
	int i;
	for(i=0;i<MAX_QUEUE_SIZE;i++)
	{
		if(ms_informFriendsEvents[i].GetEvent() == pEvent)
		{
			return true;
		}
	}
	return false;
}

void CInformFriendsEventQueue::Process()
{
	int i;
	for(i=0;i<MAX_QUEUE_SIZE;i++)
	{
		ms_informFriendsEvents[i].Process();
	}
}


CInformGroupEvent::CInformGroupEvent()
: m_pPedWhoInformed(0),
m_pEvent(0),
m_informTime(0)
{
}

CInformGroupEvent::~CInformGroupEvent()
{
	Flush();
}

void CInformGroupEvent::Flush()
{
	m_pPedWhoInformed=0;
	
	if(m_pEvent)
	{
		delete m_pEvent;
		m_pEvent=0;
	}

	m_informTime=0;
}

void CInformGroupEvent::Set(CPed* pPed, CPedGroup* pGroup, CEvent* pEvent, const int iTime)
{
	//Set the ped.
	m_pPedWhoInformed=pPed;
	Assert(m_pPedWhoInformed);
	m_pPedWhoInformedGroup=pGroup;
	Assert(m_pPedWhoInformedGroup);
	Assert(m_pPedWhoInformed->GetPedsGroup()==m_pPedWhoInformedGroup);

	//Set the event.
	m_pEvent=pEvent;

	//Set the time.
	m_informTime=fwTimer::GetTimeInMilliseconds()+iTime;
}

void CInformGroupEvent::Process()
{
	if(0==m_pPedWhoInformed)
	{
		//Ped must have been deleted so flush.
		Flush();
	}
	else if(m_informTime < fwTimer::GetTimeInMilliseconds())
	{
		//Time to inform the ped about the event.
		CPedGroup* pPedGroup = m_pPedWhoInformed->GetPedsGroup();
		if(pPedGroup)
		{
			const int iGroupID=CPedGroups::GetGroupId(pPedGroup);
			Assert(iGroupID>=0);
			if(iGroupID>=0 && CPedGroups::ms_groups[iGroupID].IsActive())
			{
				//				CEventGroupEvent groupEvent(m_pPedWhoInformed, m_pEvent->Clone());
				//				pPedGroup->GetGroupIntelligence()->AddEvent(groupEvent);
			}
		}
		//Finished with this event now.
		Flush();
	}
}

CInformGroupEvent CInformGroupEventQueue::ms_informGroupEvents[MAX_QUEUE_SIZE];

CInformGroupEventQueue::CInformGroupEventQueue()
{
}

CInformGroupEventQueue::~CInformGroupEventQueue()
{
}

void CInformGroupEventQueue::Init()
{
	Flush();
}	

bool CInformGroupEventQueue::Add(CPed* pPedWhoInformed, CPedGroup* pPedGroup, CEvent* pEvent)
{
	Assert(pEvent);
	Assert(pPedWhoInformed);
	Assert(pPedGroup);
	Assert(pPedWhoInformed->GetPedsGroup()==pPedGroup);

	int iEmptySlot=-1;
	int i;
	for(i=0;i<MAX_QUEUE_SIZE;i++)
	{
		//Check if the entry related  to pPedToInform.
		if(ms_informGroupEvents[i].m_pPedWhoInformed==pPedWhoInformed)
		{
			//Entry has a ped ptr so it must have an event and an inform time.
			Assert(ms_informGroupEvents[i].m_pEvent);
			Assert(ms_informGroupEvents[i].m_informTime!=0);

			//Check if entry is the same kind of event as pEvent.
			if(pEvent->GetEventType()==ms_informGroupEvents[i].m_pEvent->GetEventType())
			{
				//Don't add an entry because it will just be a duplicate.
				return false;
			}
		}
		else if(-1==iEmptySlot)
		{
			//Entry doesn't have a ped ptr.
			//Check if it is empty.
			if(0==ms_informGroupEvents[i].m_pEvent)
			{
				Assert(0==ms_informGroupEvents[i].m_informTime);
				iEmptySlot=i;
			}			
		}
	}

	if(iEmptySlot!=-1)
	{
		s32 timeToWait = 0;
		ms_informGroupEvents[iEmptySlot].Set(pPedWhoInformed, pPedGroup, pEvent, timeToWait);
		return true;
	}


	return false;
}

void CInformGroupEventQueue::Flush()
{
	int i;
	for(i=0;i<MAX_QUEUE_SIZE;i++)
	{
		ms_informGroupEvents[i].Flush();
	}
}

void CInformGroupEventQueue::Process()
{
	int i;
	for(i=0;i<MAX_QUEUE_SIZE;i++)
	{
		ms_informGroupEvents[i].Process();
	}
}

//////////////////////////////////////////////////////////////////////////
//Ran Over Ped
//////////////////////////////////////////////////////////////////////////
CEventRanOverPed::CEventRanOverPed(CVehicle* pVehicle, CPed* pPedRanOver, float fDamagedCaused)
: CEvent(),
m_pVehicle(pVehicle),
m_pPedRanOver(pPedRanOver),
m_fDamageCaused(fDamagedCaused)
{
#if !__FINAL
	m_EventType = EVENT_RAN_OVER_PED;
#endif
}

CEventRanOverPed::~CEventRanOverPed()
{
}

bool CEventRanOverPed::AffectsPedCore(CPed* pPed) const
{
	if(pPed->IsDead())
		return false;

	if( pPed->IsAPlayerPed() || pPed->PopTypeIsMission() || !pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) )
	{
		return false;
	}

	//Ensure the ped is not law-enforcement.
	if(pPed->IsLawEnforcementPed())
	{
		return false;
	}

	//Ensure the ped is not a medic.
	if(pPed->GetPedType() == PEDTYPE_MEDIC)
	{
		return false;
	}

	//Ensure the ped is not a fireman.
	if(pPed->GetPedType() == PEDTYPE_FIRE)
	{
		return false;
	}

	//Ensure the ped is not driving a train, or an airplane.
	if(pPed->GetIsDrivingVehicle())
	{
		const CVehicle* pVehicle = pPed->GetVehiclePedInside();
		if(pVehicle && (pVehicle->InheritsFromTrain() || pVehicle->InheritsFromPlane()))
		{
			return false;
		}
	}

	//Ensure the ped does not think they ran themselves over.
	if(!aiVerify(pPed != GetPedRanOver()))
	{
		return false;
	}

	//Ensure the ped ran over is valid.
	if(!GetPedRanOver())
	{
		return false;
	}

	return true;
}

bool CEventRanOverPed::TakesPriorityOver(const CEvent& otherEvent) const
{
	if(otherEvent.GetEventType() == EVENT_STATIC_COUNT_REACHED_MAX)
		return false;

	return CEvent::TakesPriorityOver(otherEvent);
}

bool CEventRanOverPed::CreateResponseTask(CPed* pPed, CEventResponse& response) const
{
	//Check if the ped is aggressive after running someone over.
	bool bAggressive = pPed->CheckBraveryFlags(BF_AGGRESSIVE_AFTER_RUNNING_PED_OVER);

	bool bTriedToHitPlayerRecently = false;
	if (GetPedRanOver()->IsPlayer() && m_pVehicle)
	{
		if ( m_pVehicle->GetIntelligence()->HasBeenSlowlyPushingPlayer())
		{
			bAggressive = true;
			bTriedToHitPlayerRecently = true;
		}
	}

	if(bAggressive)
	{
		//Say the audio.
		if (bTriedToHitPlayerRecently)
		{
			pPed->NewSay("GENERIC_INSULT_MED");
		}
		else
		{
			if(!pPed->NewSay("CRASH_GENERIC"))
			{
				pPed->NewSay("CRASH_CAR");
			}
		}

		//Make the ped an aggressive driver.
		pPed->GetPedIntelligence()->SetDriverAggressivenessOverride(1.0f);
		pPed->GetPedIntelligence()->SetDriverAbilityOverride(0.0f);

		CVehicle* pVehicle = pPed->GetVehiclePedInside();
		if(pVehicle)
		{
			pVehicle->m_nVehicleFlags.bMadDriver = true;
		}
	}
	else
	{
		//Say the audio.
		if(!pPed->NewSay("RUN_OVER_PLAYER"))
		{
			pPed->NewSay("APOLOGY_NO_TROUBLE");
		}
	}

	//Flee the ped that was run over.
	taskAssert(pPed != GetPedRanOver());
	CTaskSmartFlee* pTask = rage_new CTaskSmartFlee(CAITarget(GetPedRanOver()));

	//If we are aggressive, don't panic.
	if(bAggressive)
	{
		pTask->GetConfigFlags().SetFlag(CTaskSmartFlee::CF_DisablePanicInVehicle);
	}

	//Don't exit the vehicle.
	pTask->GetConfigFlags().SetFlag(CTaskSmartFlee::CF_DisableExitVehicle);

	//Skid to a stop, and hesitate after.
	if (!bTriedToHitPlayerRecently)
	{
		pTask->GetConfigFlagsForVehicle().SetFlag(CTaskVehicleFlee::CF_SkidToStop);
		pTask->GetConfigFlagsForVehicle().SetFlag(CTaskVehicleFlee::CF_HesitateAfterSkidToStop);
		pTask->GetConfigFlagsForVehicle().SetFlag(CTaskVehicleFlee::CF_HesitateUntilTargetGetsUp);
	}

	//Set the event response.
	response.SetEventResponse(CEventResponse::EventResponse, pTask);

	return (response.GetEventResponse(CEventResponse::EventResponse) != NULL);
}

bool CEventRanOverPed::ShouldCreateResponseTaskForPed( const CPed* pPed ) const
{
	return pPed && pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle );
}

//////////////////////////////////////////////////////////////////////////
//Open a door
//////////////////////////////////////////////////////////////////////////
CEventOpenDoor::CEventOpenDoor(CDoor* pDoor)
: CEvent()
, m_pDoor(pDoor)
{
#if !__FINAL
	m_EventType = EVENT_OPEN_DOOR;
#endif
}

CEventOpenDoor::~CEventOpenDoor()
{
}

bool CEventOpenDoor::TakesPriorityOver(const CEvent& otherEvent) const
{
	if(otherEvent.GetEventType() == EVENT_STATIC_COUNT_REACHED_MAX)
		return false;

	return CEvent::TakesPriorityOver(otherEvent);
}

bool CEventOpenDoor::CreateResponseTask(CPed* pPed, CEventResponse& response) const
{
	pPed->GetPedIntelligence()->AddTaskSecondary( rage_new CTaskOpenDoor( m_pDoor ), PED_TASK_SECONDARY_PARTIAL_ANIM );	
	response.SetEventResponse(CEventResponse::EventResponse, NULL);
	return response.HasEventResponse();
}

//////////////////////////////////////////////////////////////////////////
//Shove a ped.
//////////////////////////////////////////////////////////////////////////
CEventShovePed::CEventShovePed(CPed* pPed) 
: CEvent()
, m_pPedToShove(pPed)
{
#if !__FINAL
	m_EventType = EVENT_SHOVE_PED;
#endif
}

CEventShovePed::~CEventShovePed()
{
}

bool CEventShovePed::AffectsPedCore(CPed* pPed) const
{
	if(pPed->IsDead())
		return false;

	if (pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_SwimmingTasksRunning))
		return false;

	if (CTaskMobilePhone::IsRunningMobilePhoneTask(*pPed))
		return false;

	if (pPed->GetUsingRagdoll())
		return false;
	
	return true;
}

bool CEventShovePed::TakesPriorityOver(const CEvent& otherEvent) const
{
	if(otherEvent.GetEventType() == EVENT_STATIC_COUNT_REACHED_MAX)
		return false;

	return CEvent::TakesPriorityOver(otherEvent);
}

bool CEventShovePed::CreateResponseTask(CPed* pPed, CEventResponse& response) const
{
	pPed->SetLastPedShoved(m_pPedToShove);
	pPed->GetPedIntelligence()->AddTaskSecondary( rage_new CTaskShovePed( m_pPedToShove ), PED_TASK_SECONDARY_PARTIAL_ANIM );	
	response.SetEventResponse(CEventResponse::EventResponse, NULL);
	return response.HasEventResponse();
}

//////////////////////
//VEHICLE COLLISION EVENT
//////////////////////

const float CEventVehicleCollision::ms_fDamageThresholdSpeed=0.25f;
const float CEventVehicleCollision::ms_fDamageThresholdSpeedAlongContact=0.5f;
const float CEventVehicleCollision::ms_fMaxPlayerImpulse=1000.0f;
const float CEventVehicleCollision::ms_fHighDamageImpulseThreshold=650.0f;
const float CEventVehicleCollision::ms_fLowDamageImpulseThreshold=250.0f;

CEventVehicleCollision::CEventVehicleCollision(const CVehicle* pVehicle, const Vector3& vNormal, const Vector3& vPos, const float fImpulseMagnitude, const u16 iVehicleComponent, const float fMoveBlendRatio, u16 nForceReaction)
: CEvent(),
m_nForceReaction(nForceReaction), 
m_pVehicle((CVehicle*)pVehicle),
m_vNormal(vNormal),
m_vPos(vPos),
m_fImpulseMagnitude(fImpulseMagnitude),
m_iVehicleComponent(iVehicleComponent),
m_fMoveBlendRatio(fMoveBlendRatio),
m_bForcedDoorCollision(false),
m_iDirectionToWalkRoundCar((u8)0),
m_bVehicleStationary(false)
{
#if !__FINAL
	m_EventType = EVENT_VEHICLE_COLLISION;
#endif

	m_fVehicleSpeedSq = pVehicle ? pVehicle->GetVelocity().Mag2() : 0.0f;
}

CEventVehicleCollision::~CEventVehicleCollision()
{
}

bool CEventVehicleCollision::TakesPriorityOver(const CEvent& otherEvent) const
{
	if(otherEvent.GetEventType() == EVENT_STATIC_COUNT_REACHED_MAX)
		return false;

	return true;
}

bool CEventVehicleCollision::CreateResponseTask(CPed* pPed, CEventResponse& response) const
{
	//Get the  vehicle collision event and the collision vehicle.
	CVehicle* pVehicle=m_pVehicle;
	if(0==pVehicle)
	{
		return false;
	}
	if(pPed->GetIsInVehicle())
	{
		return false;
	} 

	// Disable based on task flags.
	if (pPed->GetTaskData().GetIsFlagSet(CTaskFlags::DisableEvasiveStep))
	{
		return false;
	}

	if(GetForceReaction()==CEventVehicleCollision::PED_FORCE_EVADE_STEP)
	{	
		if( !pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_DisableEvasiveDives ) )
		{
			response.SetEventResponse(CEventResponse::EventResponse, rage_new CTaskComplexEvasiveStep(pVehicle));
		}
	}	
	else
	{
		//Vehicle speed is quite slow so just walk round the vehicle unless the ped is the player.
		CTask* pActiveSimplestMoveTask = pPed->GetPedIntelligence()->GetActiveSimplestMovementTask();
		//If ped is player then can the ped can respond if he isn't in control; that is, he has a primary task
		//that prevents CTaskPlayerOnFoot::ProcessPed(pPlayerPed) being called.
		//Non-players can always respond to the event.
		
		const bool bPlayerInControl = pActiveSimplestMoveTask && pActiveSimplestMoveTask->GetTaskType()==CTaskTypes::TASK_MOVE_PLAYER;

		if(!bPlayerInControl && pActiveSimplestMoveTask)
		{
			Assert( pActiveSimplestMoveTask->IsMoveTask() );
			//Ped isn't under player control and is walking.
			CTaskMoveInterface* pTaskMove = pActiveSimplestMoveTask->GetMoveInterface();
			Assert(pTaskMove);

			static float s_fMaxMoveBlendRatioForEvasiveStep = 1.3f;
			if( pTaskMove->GetMoveBlendRatio() > s_fMaxMoveBlendRatioForEvasiveStep )
			{
				//Test if the ped is standing on vehicle.
				if(pVehicle == pPed->GetGroundPhysical())
				{
					//Ped is standing on vehicle.
					//If ped is facing direction he wants to go and he can jump up then set him to jump.
					const CEntity* pEntity=(const CEntity*)pVehicle;
					if(!pPed->GetTaskData().GetIsFlagSet(CTaskFlags::DisableWallHitAnimation) && rage::Abs(pPed->GetCurrentHeading() - pPed->GetDesiredHeading()) < 0.01f && CPedGeometryAnalyser::CanPedJumpObstacle(*pPed,*pEntity)/*m_pPed->CanPedJumpThis((CEntity*)pVehicle)*/)
					{
						response.SetEventResponse(CEventResponse::EventResponse, rage_new CTaskJumpVault());
					}
				}
			}
			else
			{
				// if the ped isn't trying to go anywhere, do a casual evade
				if( !pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_DisableEvasiveDives ) && !pPed->IsPlayer() && !m_bVehicleStationary )
				{
					pPed->BumpedByEntity(*pVehicle, m_fVehicleSpeedSq);

					response.SetEventResponse(CEventResponse::EventResponse, rage_new CTaskComplexEvasiveStep(pVehicle, CTaskComplexEvasiveStep::CF_Casual));
				}
			}
		}
	}	

	// This is to prevent peds getting stuck behind a vehicle door whilst trying to path out
	if (!response.HasEventResponse() && !pPed->GetMotionData()->GetPlayerHasControlOfPedThisFrame())
	{
		if(!pPed->GetTaskData().GetIsFlagSet(CTaskFlags::DisableWallHitAnimation) && 
			pPed->GetPedIntelligence()->GetEventScanner()->GetStaticMovementScanner().GetStaticCounter() >= 25)
		{
			// Climbing over the doors of planes/helis can lead to falling into the propeller (url:bugstar:1394052)
			if(!m_pVehicle->InheritsFromHeli() && !m_pVehicle->InheritsFromPlane())
			{
				// Find out if the ped has collided with a car door
				const CCarDoor * pDoor = CCarEnterExit::GetCollidedDoorFromCollisionComponent(m_pVehicle, m_iVehicleComponent, 0.05f);
				if (pDoor)
				{
					static dev_float fDotEps = 0.967f;

					// If ped is navigating, then we have a better knowledge of where they're trying to go
					CTaskMoveFollowNavMesh * pTaskNavMesh = static_cast<CTaskMoveFollowNavMesh*>((CTaskMoveFollowNavMesh*) pPed->GetPedIntelligence()->FindTaskActiveMovementByType(CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH));
					if (pTaskNavMesh)
					{
						// Obtain the segment we are currently following.
						// Note, this is the CNavMeshRoute points - and not the spline segments, and therefore not
						// as accurate as it could be.  But for our purposes it should be sufficient to determine
						// whether the ped's route is moving him into the door (rather than some steering response, etc)
						Vector3 vSegStart, vSegEnd;
						if(pTaskNavMesh->GetThisWaypointLine(pPed, vSegStart, vSegEnd))
						{
							Vector3 vToTarget = vSegEnd - vSegStart;
							vToTarget.z = 0.f;
							vToTarget.Normalize();

							if (Dot(VEC3V_TO_VECTOR3(pPed->GetTransform().GetForward()), vToTarget) > fDotEps)	// We must face the dir somewhat in case we are strafing
							{
								response.SetEventResponse(CEventResponse::EventResponse, rage_new CTaskJumpVault(JF_ForceVault));
							}
						}
					}
					// Otherwise examine their simplest movement task
					else
					{
						CTaskMove * pTaskSimplestMove = pPed->GetPedIntelligence()->GetActiveSimplestMovementTask();
						if (pTaskSimplestMove && pTaskSimplestMove->IsTaskMoving() && pTaskSimplestMove->GetMoveInterface()->HasTarget())
						{
							Assert(pTaskSimplestMove->IsMoveTask());
							Vector3 vToTarget = pTaskSimplestMove->GetTarget() - VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
							vToTarget.z = 0.f;
							vToTarget.Normalize();
							
							if (Dot(VEC3V_TO_VECTOR3(pPed->GetTransform().GetForward()), vToTarget) > fDotEps)	// We must face the dir somewhat in case we are strafing
							{
								response.SetEventResponse(CEventResponse::EventResponse, rage_new CTaskJumpVault(JF_ForceVault));
							}
						}
					}
				}
			}
		}

		// HACK GTA
		// Workaround hack for url:bugstar:1421315
		// If walking into the wingtips on the stunt plane, and facing away from the vehicle - enable a move to walk around it
		// Its a pretty sad day when it comes to this sort of workaround: next game we'll have navmesh objects for all major components of a vehicle
		if(pVehicle->GetModelIndex() == MI_PLANE_STUNT.GetModelIndex())
		{
			if( CreateHackResponseTaskToAvoidStuntPlaneWingTips(pVehicle, pPed, response) )
			{
				return true;
			}
		}

		// Could also check for fleeing I guess
		if (pVehicle != pPed->GetGroundPhysical())
		{
			if (pPed->GetPedIntelligence()->GetEventScanner()->GetStaticMovementScanner().GetStaticCounter() >= 3)
			{
				CTaskNavBase * pTaskNavBase = static_cast<CTaskNavBase*>((CTaskMoveFollowNavMesh*) pPed->GetPedIntelligence()->FindTaskActiveMovementByType(CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH));
				if (pTaskNavBase)
				{
					// Special case : we're bumping into a plane vehicle, and we started our path inside a plane's bounding box - then there's a good
					// chance we're stuck trying to get outside of the bounds of that same plane.  Enable a response task to walk around the plane's convex hull.
					// See (url:bugstar:1329489)
					if( pTaskNavBase->GetPathStartedInsidePlaneVehicle() && m_pVehicle->GetVehicleType()==VEHICLE_TYPE_PLANE &&
						 pPed->GetPedIntelligence()->GetActiveSimplestMovementTask() && pPed->GetPedIntelligence()->GetActiveSimplestMovementTask()->HasTarget())
					{
						CTaskMove * pTaskSimplestMove = pPed->GetPedIntelligence()->GetActiveSimplestMovementTask();
						response.SetEventResponse(CEventResponse::MovementResponse, rage_new CTaskMoveWalkRoundVehicle(pTaskSimplestMove->GetTarget(), m_pVehicle, m_fMoveBlendRatio, CTaskMoveWalkRoundVehicle::Flag_TestHasClearedEntity) );
					}
					// Otherwise : we're bumping into a vehicle - force a scan
					else if (pTaskNavBase->GetTimeSinceLastScanForObstructions() > 250)
					{
						pTaskNavBase->SetScanForObstructionsNextFrame();
					}
				}
			}

			if (pPed->GetPedIntelligence()->GetEventScanner()->GetStaticMovementScanner().GetStaticCounter() >= 20)
			{
				CTaskWander * pTaskWander = (CTaskWander*)pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_WANDER);
				if(pTaskWander)
				{
					pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_JustBumpedIntoVehicle, true);
				}
			}
		}
	}

	return response.HasEventResponse();
}

bool CEventVehicleCollision::CreateHackResponseTaskToAvoidStuntPlaneWingTips(CVehicle * pVehicle, CPed * pPed, CEventResponse& response) const
{
	Vector3 vToVehicle = VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetPosition() - pPed->GetTransform().GetPosition());
	vToVehicle.Normalize();
	Vector3 vPedForwards = VEC3V_TO_VECTOR3(pPed->GetTransform().GetForward());

	// Walking away from vehicle?
	if(DotProduct(vToVehicle, vPedForwards) < -0.0f)
	{
		Vector3 vOffset = VEC3_ZERO;

		// Left wing tip?
		if(m_iVehicleComponent == 13)
		{
			vOffset = Vector3(-5.0f, -3.0f, 0.0f);
		}
		else if(m_iVehicleComponent == 15)
		{
			vOffset = Vector3(5.0f, -3.0f, 0.0f);
		}

		if(!vOffset.IsZero())
		{
			response.SetEventResponse(
				CEventResponse::MovementResponse,
				rage_new CTaskMoveGoToPointRelativeToEntityAndStandStill(
				pVehicle,
				m_fMoveBlendRatio,
				vOffset,
				1.0f,
				2000) );

			return true;
		}
	}
	return false;
}

bool CEventVehicleCollision::operator==( const fwEvent& otherEvent ) const
{
	if( static_cast<const CEvent&>(otherEvent).GetEventType() == GetEventType() )
	{
		const CEventVehicleCollision& otherCollisionEvent = static_cast<const CEventVehicleCollision&>(otherEvent);
		if( otherCollisionEvent.GetVehicle() == GetVehicle())
		{
			return true;
		}
	}
	return false;
}

bool CEventVehicleCollision::AffectsPedCore(CPed* pPed) const
{
	if(pPed->IsDead() || !m_pVehicle || pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_InVehicle) || m_pVehicle->InheritsFromBoat())
		return false;

	if(m_nForceReaction)
		return true;

	if(pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_IsSwimming ))
		return false; 

	// don't accept if there is a melee event in the queue
	CPedIntelligence* pPedIntelligence = pPed->GetPedIntelligence();
	if( pPedIntelligence )
	{
		// If the ped is doing a melee move then ignore the event.
		if( pPedIntelligence->GetTaskMeleeActionResult() )
			return false;

		if( pPedIntelligence->HasPairedDamageEvent() )
			return false;

		// ignore if the ped is fighting someone
		if (pPedIntelligence->GetQueriableInterface())
		{
			if (
				pPedIntelligence->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_MELEE)
				|| pPedIntelligence->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_COMBAT)
				|| pPedIntelligence->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_GUN)
				)
			{
				return false;
			}
		}
	}

	// If the ped is seeking cover against this vehicle, ignore.
	if( pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_IN_COVER ) &&
		pPed->GetCoverPoint() && pPed->GetCoverPoint()->m_pEntity == (CEntity*) m_pVehicle )
	{
		return false;
	}

	bool bAgainstDoor = false;
	// Find out if the ped has collided with a car door
	const CCarDoor * pDoor = CCarEnterExit::GetCollidedDoorFromCollisionComponent(m_pVehicle, m_iVehicleComponent, 0.05f);

	// We may be able to trivially ignore this collision by pushing the door closed
	if(pDoor)
	{
		CTaskMove * pTaskSimplestMove = pPed->GetPedIntelligence()->GetActiveSimplestMovementTask();
		if(pTaskSimplestMove && pTaskSimplestMove->IsTaskMoving() && pTaskSimplestMove->GetMoveInterface()->HasTarget())
		{
			Assert(pTaskSimplestMove->IsMoveTask());
			const Vector3& vTargetPos = pTaskSimplestMove->GetTarget();
			const Vector3 vPedPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());	
			if(CCarEnterExit::CanDoorBePushedClosed(m_pVehicle, pDoor, vPedPosition, vTargetPos))
			{
				return false;
			}
			else
			{
				bAgainstDoor = true;
			}
		}
	}

	if (!bAgainstDoor)
	{
		// If we're entering this car then don't accept the event (unless we've hit a door which is open)
		CVehicle * pCarEntering = pPed->GetPedIntelligence()->IsInACarOrEnteringOne();
		if(pCarEntering && pCarEntering == m_pVehicle) /* && (!pDoor || pDoor->GetIsClosed() || pDoor->GetDoorRatio() < 0.1f )) */
			return false;

		// If we're running CTaskMoveGoToVehicleDoor and are in the State_FollowPointRouteToBoardingPoint or State_GoDirectlyToBoardingPoint states,
		// ignore collisions since that that task tests regularly for obstructions and adjusts its state accordingly.
		CTaskMoveGoToVehicleDoor * pGoToVehicleDoor = (CTaskMoveGoToVehicleDoor*)pPed->GetPedIntelligence()->FindTaskActiveMovementByType(CTaskTypes::TASK_MOVE_GO_TO_VEHICLE_DOOR);
		if(pGoToVehicleDoor && (pGoToVehicleDoor->GetState()==CTaskMoveGoToVehicleDoor::State_FollowPointRouteToBoardingPoint || pGoToVehicleDoor->GetState()==CTaskMoveGoToVehicleDoor::State_GoDirectlyToBoardingPoint))
		{
			return false;
		}

		// If we're already walking around this vehicle, then ignore the event
		CTaskMoveWalkRoundVehicle * pWalkRoundVehicle = (CTaskMoveWalkRoundVehicle*)pPed->GetPedIntelligence()->FindTaskActiveMovementByType(CTaskTypes::TASK_MOVE_WALK_ROUND_VEHICLE);
		if(pWalkRoundVehicle && m_pVehicle==pWalkRoundVehicle->GetVehicle())
		{
			return false;
		}
	}

	// Likewise is we are already walking around this vehicle's door
	CTaskWalkRoundVehicleDoor * pWalkRoundDoor = (CTaskWalkRoundVehicleDoor*)pPed->GetPedIntelligence()->FindTaskActiveMovementByType(CTaskTypes::TASK_MOVE_WALK_ROUND_VEHICLE_DOOR);
	if(pWalkRoundDoor && m_pVehicle==pWalkRoundDoor->GetVehicle())
	{
		return false;
	}

	// Waypoint following tasks may wish to prevent this event from occurring at all
	CTaskFollowWaypointRecording * pWaypointTask = (CTaskFollowWaypointRecording*)pPed->GetPedIntelligence()->FindTaskPrimaryByType(CTaskTypes::TASK_FOLLOW_WAYPOINT_RECORDING);
	if(pWaypointTask && !pWaypointTask->GetRespondsToCollisionEvents())
		return false;

	// we're already doing an evasive step
	if (pPed->GetPedIntelligence()->FindTaskPrimaryByType(CTaskTypes::TASK_COMPLEX_EVASIVE_STEP))
		return false;

	return true;
}


//////////////////////
//PED COLLISION EVENT
//////////////////////

const float CEventPedCollisionWithPed::ms_fPedBrushKnockdown=1.5f;

CEventPedCollisionWithPed::CEventPedCollisionWithPed(const u16 nPieceType, const float fImpulseMagnitude, const CPed* pPed, const Vector3& vNormal, const Vector3& vPos, const float fPedMoveBlendRatio, const float fOtherPedMoveBlendRatio)
: CEvent(),
m_iPieceType(nPieceType),
m_fImpulseMagnitude(fImpulseMagnitude),
m_pOtherPed((CPed*)pPed),
m_vNormal(vNormal),
m_vPos(vPos),
m_fPedMoveBlendRatio(fPedMoveBlendRatio),
m_fOtherPedMoveBlendRatio(fOtherPedMoveBlendRatio)
{
#if !__FINAL
	m_EventType = EVENT_PED_COLLISION_WITH_PED;
#endif
}

CEventPedCollisionWithPed::~CEventPedCollisionWithPed()
{
}

bool CEventPedCollisionWithPed::TakesPriorityOver(const CEvent& otherEvent) const
{
	if(otherEvent.IsTemporaryEvent())	//CEventHandlerHistory::IsTemporaryEvent(otherEvent))
	{
		return true;
	}
	else
	{
		return CEvent::TakesPriorityOver(otherEvent);
	}
}

bool CEventPedCollisionWithPed::CreateResponseTask(CPed* pPed, CEventResponse& response) const
{
	if (m_pOtherPed->GetPedResetFlag(CPED_RESET_FLAG_IsExactStopping) && !pPed->GetTaskData().GetIsFlagSet(CTaskFlags::DisableWallHitAnimation))
	{
		response.SetEventResponse(CEventResponse::EventResponse, rage_new CTaskHitWall());
		return response.HasEventResponse();
	}

	return CEvent::CreateResponseTask(pPed, response);
}

bool CEventPedCollisionWithPed::AffectsPedCore(CPed* pPed) const
{
	if(pPed->IsDead())
		return false;

	if(pPed->GetIsAttached())
		return false;

	if(!m_pOtherPed)
		return false;

	// TEMP: Peds are doing some crazy NM behaviours underwater, so am disabling this completely for now.
	if(pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_IsSwimming ) || m_pOtherPed->GetPedConfigFlag( CPED_CONFIG_FLAG_IsSwimming ))
		return false; 

	if(pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) || m_pOtherPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ))
		return false;

	if(pPed->GetPedResetFlag( CPED_RESET_FLAG_DisablePedCollisionWithPedEvent ))
		return false;

	// don't accept if there is a melee event in the queue
	CPedIntelligence* pPedIntelligence = pPed->GetPedIntelligence();
	if( pPedIntelligence )
	{
		if( pPedIntelligence->HasPairedDamageEvent() )
			return false;

		// ignore if the ped is fighting someone
		if (pPedIntelligence->GetQueriableInterface())
		{
			if ( pPedIntelligence->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_COMBAT) ||
				 pPedIntelligence->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_GUN)
				)
			{
				return false;
			}
		}
	}
	
	// don't accept if this ped is stuck		
	if(m_pOtherPed->GetPedIntelligence()->GetEventScanner()->GetStaticMovementScanner().GetStaticCounter() > CStaticMovementScanner::ms_iStaticCounterStuckLimit)
		return false;

	if(pPed->GetPedResetFlag(CPED_RESET_FLAG_IsExactStopping) && m_pOtherPed->GetPedResetFlag(CPED_RESET_FLAG_IsExactStopping))
		return true;

	// Allow moving ragdolls to activate live non-player peds
	if (m_pOtherPed && m_pOtherPed->GetUsingRagdoll() && !pPed->IsPlayer() && m_pOtherPed->GetRagdollInst()->GetARTAssetID() >= 0
		&& pPed->GetRagdollInst()->GetARTAssetID() >= 0 && !pPed->IsDead())
	{
		phArticulatedBody *body = ((phArticulatedCollider*) m_pOtherPed->GetCollider())->GetBody();
		Vector3 otherLinVel = VEC3V_TO_VECTOR3(body->GetLinearVelocity(0));
		Vector3 vToPed = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()) - VEC3V_TO_VECTOR3(m_pOtherPed->GetTransform().GetPosition());
		vToPed.Normalize();
		static float dotMin = 0.2f; // Considering a non-unit vel
		float dot = vToPed.Dot(otherLinVel);
		if (dot > dotMin)
		{
			return true;
		}
	}

	// don't accept collision events for peds we hate
	if(pPed->GetPedIntelligence()->IsThreatenedBy(*m_pOtherPed))
		return false;

	// if both peds are standing still then ignore collisions
	// If the other ped is standing and this ped is walking, ignore collisions
	if( !pPed->GetPedResetFlag( CPED_RESET_FLAG_IsJumping ) && ( pPed->GetMotionData()->GetIsStill() || pPed->GetMotionData()->GetIsWalking() ) && m_pOtherPed->GetMotionData()->GetIsStill() )
		return false;

	//static const float fIgnoreCosTheta = rage::Cosf(( DtoR * 120.0f));	// equals -0.5f
	static const float fIgnoreCosTheta = 1.0f; // never rule out ped/ped collision response based angle

	if(DotProduct(m_vNormal, VEC3V_TO_VECTOR3(pPed->GetTransform().GetB())) > fIgnoreCosTheta)	// -0.5f
		return false;

	CTask * pMoveResponseTask = pPed->GetPedIntelligence()->GetMovementResponseTask();
	if(pMoveResponseTask)
	{
		if(pMoveResponseTask->GetTaskType()==CTaskTypes::TASK_WALK_ROUND_ENTITY &&
			((CTaskWalkRoundEntity*)pMoveResponseTask)->GetEntity()==m_pOtherPed)
		{
			return false;
		}
	}

	// Waypoint following tasks may wish to prevent this event from occurring at all
	CTaskFollowWaypointRecording * pWaypointTask = (CTaskFollowWaypointRecording*)pPed->GetPedIntelligence()->FindTaskPrimaryByType(CTaskTypes::TASK_FOLLOW_WAYPOINT_RECORDING);
	if(pWaypointTask && !pWaypointTask->GetRespondsToCollisionEvents())
		return false;

	return true;
}

CEventPedCollisionWithPlayer::CEventPedCollisionWithPlayer(const u16 nPieceType, const float fImpulseMagnitude, const CPed* pOtherPed, const Vector3& vNormal, const Vector3& vPos, const float fPedMoveBlendRatio, const float fOtherPedMoveBlendRatio)
: CEventPedCollisionWithPed(nPieceType,fImpulseMagnitude,pOtherPed,vNormal,vPos,fPedMoveBlendRatio,fOtherPedMoveBlendRatio)
{
	m_fOtherPedSpeedSq = pOtherPed ? pOtherPed->GetVelocity().Mag2() : 0.0f;
}

CEvent* CEventPedCollisionWithPlayer::Clone() const
{
	CEventPedCollisionWithPlayer* pEvent = rage_new CEventPedCollisionWithPlayer(m_iPieceType,m_fImpulseMagnitude,m_pOtherPed,m_vNormal,m_vPos,m_fPedMoveBlendRatio,m_fOtherPedMoveBlendRatio);
	pEvent->m_fOtherPedSpeedSq = m_fOtherPedSpeedSq;

	return pEvent;
}

bool CEventPedCollisionWithPlayer::AffectsPedCore(CPed* pPed) const
{
	if (pPed && pPed->IsUsingKinematicPhysics())
		return false;
	if ( !pPed->GetTaskData().GetIsFlagSet(CTaskFlags::CanBeShoved) && !pPed->GetTaskData().GetIsFlagSet(CTaskFlags::PlayNudgeAnimationForBumps))
		return false;

	const bool bDontRagdoll				=  pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_DontActivateRagdollFromAnyPedImpact);
	const bool bDontPlayNudge			= !pPed->GetTaskData().GetIsFlagSet(CTaskFlags::PlayNudgeAnimationForBumps);
	if (bDontRagdoll && bDontPlayNudge)
	{
		const bool bRunningFollowerTask = (NULL != pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_FOLLOW_LEADER_ANY_MEANS));
		if (!bRunningFollowerTask)
		{
			return false;
		}
	}

	// If we're running CTaskMoveGoToVehicleDoor and are in the State_FollowPointRouteToBoardingPoint or State_GoDirectlyToBoardingPoint states,
	// ignore collisions with player. Otherwise we might need to test against the player which is pretty quirky but doable ofc. This fixes: B* 1038743
	CTaskMoveGoToVehicleDoor * pGoToVehicleDoor = (CTaskMoveGoToVehicleDoor*)pPed->GetPedIntelligence()->FindTaskActiveMovementByType(CTaskTypes::TASK_MOVE_GO_TO_VEHICLE_DOOR);
	if(pGoToVehicleDoor && (pGoToVehicleDoor->GetState()==CTaskMoveGoToVehicleDoor::State_FollowPointRouteToBoardingPoint || pGoToVehicleDoor->GetState()==CTaskMoveGoToVehicleDoor::State_GoDirectlyToBoardingPoint))
	{
		return false;
	}
	
	//Ensure we have not recently collided with the player.
	static const u32 s_uMinTimeSinceLastCollidedWithPlayer = 3000;
	if((pPed->GetPedIntelligence()->GetLastTimeCollidedWithPlayer() + s_uMinTimeSinceLastCollidedWithPlayer) > fwTimer::GetTimeInMilliseconds())
	{
		return false;
	}

	//Ensure we are not fleeing.
	if(pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_REACT_AND_FLEE, true))
	{
		return false;
	}
	else if(pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_SMART_FLEE, true))
	{
		return false;
	}

	// CNC: Disable evasive step response if other ped is a player doing a melee shoulder barge towards this ped.
	if (CEventPedCollisionWithPlayer::CNC_ShouldPreventEvasiveStepDueToPlayerShoulderBarging(pPed, m_pOtherPed))
	{
		return false;
	}

	return CEventPedCollisionWithPed::AffectsPedCore(pPed);
}

bool CEventPedCollisionWithPlayer::CNC_ShouldPreventEvasiveStepDueToPlayerShoulderBarging(const CPed* pPed, const CPed* pOtherPed)
{
	// CNC: Disable evasive step response if other ped is a player doing a melee shoulder barge towards this ped.
	if (!pPed || !pOtherPed)
	{
		return false;
	}

	if (!NetworkInterface::IsInCopsAndCrooks())
	{
		return false;
	}

	CPlayerInfo* pOtherPlayerInfo = pOtherPed->GetPlayerInfo();
	if (!pOtherPlayerInfo)
	{
		return false;
	}

	const CPedIntelligence* pOtherPedIntelligence = pOtherPed->GetPedIntelligence();
	if (pOtherPedIntelligence)
	{
		const CTaskMelee* pTaskMelee = pOtherPedIntelligence->GetTaskMelee();
		if (pTaskMelee)
		{
			// Ensure this ped is the melee target.
			CEntity* pMeleeTargetEntity = pTaskMelee->GetTargetEntity();
			if (pMeleeTargetEntity && pMeleeTargetEntity == pPed)
			{
				const CTaskMeleeActionResult* pTaskMeleeActionResult = pOtherPedIntelligence->GetTaskMeleeActionResult();
				if (pTaskMeleeActionResult && pTaskMeleeActionResult->GetActionResult())
				{
					const CActionResult* pActionResult = pTaskMeleeActionResult->GetActionResult();
					if (pActionResult && pActionResult->GetIsShoulderBarge())
					{
						return true;
					}
				}
			}
		}
	}

	return false;
}

bool CEventPedCollisionWithPlayer::CreateResponseTask(CPed* pPed, CEventResponse& response) const
{
	if (!m_pOtherPed)
	{
		return false;
	}

#if ENABLE_HORSE
	//if someone is trying to mount me, ignore this event
	if (pPed->GetHorseComponent())
	{
		if (m_pOtherPed->GetPedIntelligence()->FindTaskByType(CTaskTypes::TASK_MOUNT_ANIMAL)!=NULL)
		{
			return false;
		}
	}
#endif

	pPed->BumpedByEntity(*m_pOtherPed, m_fOtherPedSpeedSq);

	// Disable based on task flags.
	if (pPed->GetTaskData().GetIsFlagSet(CTaskFlags::DisableEvasiveStep))
	{
		// Don't play sidestep if the ped is running or the target is stationary.
		if (pPed->GetTaskData().GetIsFlagSet(CTaskFlags::PlayNudgeAnimationForBumps) && pPed->GetPedModelInfo()->GetSidestepClipSet() != CLIP_SET_ID_INVALID 
			&& (pPed->GetMotionData()->GetIsLessThanRun() && !m_pOtherPed->GetMotionData()->GetIsStill()))
		{
			//For nudge animations - play a short clip in response to being bumped
			CTaskSidestep* pNewTask = rage_new CTaskSidestep(m_pOtherPed->GetTransform().GetPosition());

			response.SetEventResponse(CEventResponse::EventResponse, pNewTask);
			pPed->GetPedIntelligence()->CollidedWithPlayer();
			return true;
		}
		return false;
	}

	fwFlags8 uConfigFlags = 0;
	if(m_pOtherPed->GetMotionData()->GetIsLessThanRun() || (fwRandom::GetRandomNumberInRange(0.0f, 1.0f) >= CEventPotentialBeWalkedInto::sm_Tunables.m_ChanceToUseCasualAvoidAgainstRunningPed))
	{
		uConfigFlags.SetFlag(CTaskComplexEvasiveStep::CF_Casual);
	}

	CTask* pNewTask = rage_new CTaskComplexEvasiveStep(m_pOtherPed, uConfigFlags);

	response.SetEventResponse(CEventResponse::EventResponse, pNewTask);
	pPed->GetPedIntelligence()->CollidedWithPlayer();
	
	return true;
}

//////////////////////
//OBJECT COLLISION EVENT
//////////////////////

const float CEventObjectCollision::ms_fStraightAheadDotProduct=0.9397f;//cos(PI/9)

CEventObjectCollision::CEventObjectCollision(const u16 nPieceType, const CObject* pObject, const Vector3& vNormal, const Vector3& vPos, const float fMoveBlendRatio)
:	CEvent(),
	m_fMoveBlendRatio(fMoveBlendRatio),
	m_pObject((CObject*)pObject),
	m_vNormal(vNormal),
	m_vPos(vPos),
	m_iPieceType(nPieceType),
	m_bJustPushDoor(false)
{
#if !__FINAL
	m_EventType = EVENT_OBJECT_COLLISION;
#endif
}

CEventObjectCollision::~CEventObjectCollision()
{
}

bool CEventObjectCollision::AffectsPedCore(CPed* pPed) const 
{
	if(pPed->GetIsAttached() || pPed->GetIsInVehicle())
		return false;
	if(!m_pObject)
		return false;
	if(m_pObject == pPed->GetGroundPhysical())
		return false;
	if(m_pObject->GetOwnedBy() == ENTITY_OWNEDBY_FRAGMENT_CACHE)
		return false;
	if(!CPedIntelligence::ms_bPedsRespondToObjectCollisionEvents)
		return false;
	if(pPed->GetMotionData()->GetPlayerHasControlOfPedThisFrame())
		return false;

	CTaskMove * pSimplestMoveTask = pPed->GetPedIntelligence()->GetActiveSimplestMovementTask();
	if(!pSimplestMoveTask || !pSimplestMoveTask->HasTarget())
		return false;

	Assert(m_pObject->GetIsPhysical());

	// If we are already walking around this entity as a movement event response, then don't accept event again.
	CTask * pMoveEventResponse = pPed->GetPedIntelligence()->GetMovementResponseTask();
	if(pMoveEventResponse && pMoveEventResponse->GetTaskType()==CTaskTypes::TASK_WALK_ROUND_ENTITY)
	{
		return false;
	}

	// Waypoint following tasks may wish to prevent this event from occurring at all
	CTaskFollowWaypointRecording * pWaypointTask = (CTaskFollowWaypointRecording*)pPed->GetPedIntelligence()->FindTaskPrimaryByType(CTaskTypes::TASK_FOLLOW_WAYPOINT_RECORDING);
	if(pWaypointTask && !pWaypointTask->GetRespondsToCollisionEvents())
		return false;

	Vector3 vGoToTarget = pSimplestMoveTask->GetTarget();
	Vector3 vRouteTarget;
	CTaskNavBase * pTaskNavBase = GetRouteTarget(pPed, vGoToTarget, vRouteTarget);

	if(pTaskNavBase && pTaskNavBase->GetState()==CTaskMoveFollowNavMesh::NavBaseState_MovingWhilstWaitingForPath)
	{
		return false;
	}

	if(!pTaskNavBase)
	{
		if(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_USE_CLIMB_ON_ROUTE))
			return false;
		if(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_USE_DROPDOWN_ON_ROUTE))
			return false;
		if(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_USE_LADDER_ON_ROUTE))
			return false;
	}

	//****************************************************************************
	//	Special check for doors : we will want to do a walk-round-entity response
	//  if we are pushing up against the door, but it is at its joint limit.
	//	All other situations we will not avoid it, but just push it open instead.

	CBaseModelInfo * pModelInfo = m_pObject->GetBaseModelInfo();
	const bool bIsDoor = m_pObject->IsADoor() && CDoor::DoorTypeOpensForPedsWhenNotLocked(((CDoor*)m_pObject.Get())->GetDoorType());
	const bool bIsFixed = (m_pObject->IsBaseFlagSet(fwEntity::IS_FIXED));
	const bool bIsFixedForNav = pModelInfo->GetIsFixedForNavigation();

	bool bPushingAgainstDoorLimit = false;
	if(bIsDoor && !(bIsFixed || bIsFixedForNav))
	{
		const Vector3 vPedPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
		CEntityBoundAI bound(*m_pObject, vPedPosition.z, pPed->GetCapsuleInfo()->GetHalfWidth());
		const int iPedSide = bound.ComputeHitSideByPosition(vPedPosition);
		const int iTargetSide = bound.ComputeHitSideByPosition(vGoToTarget);
		if(iPedSide != iTargetSide)
		{
			Vector3 vMoveDir = vGoToTarget - vPedPosition;
			vMoveDir.Normalize();
			bPushingAgainstDoorLimit = static_cast<CDoor*>(m_pObject.Get())->GetDoorIsAtLimit(vPedPosition, vMoveDir);
		}
	}

	// If the player is pushing into a door under player control, then allow the event - it will play the opening anim
	CTask * pMoveTask = pPed->GetPedIntelligence()->GetActiveSimplestMovementTask();
	const bool bUnderPlayerControl = pMoveTask && pMoveTask->GetTaskType()==CTaskTypes::TASK_MOVE_PLAYER;
	if(bUnderPlayerControl && bIsDoor && !bPushingAgainstDoorLimit)
	{
		const_cast<CEventObjectCollision*>(this)->SetJustPushDoor(true);
		return( !pPed->IsDead() );	
	}

	// We only walk round doors if we're pushing up against their joint limit (ie. they won't open any more)
	const bool bWalkRoundThis = (!bIsDoor || bPushingAgainstDoorLimit);

	// Only walk round this object if we're not on a route & this is a fixed obj, as we shouldn't need to - it'll be baked into the navmesh
	if(bWalkRoundThis) // && !pTaskNavBase)
	{
		if(CanIgnoreCollision(pPed, m_vNormal, m_vPos, vRouteTarget))
			return false;

		return (!bUnderPlayerControl && !pPed->IsDead());
	}
	else if( bIsDoor && !bIsFixed )
	{
		if(!bPushingAgainstDoorLimit)
			const_cast<CEventObjectCollision*>(this)->SetJustPushDoor(true);

		// Respond to this event
		// If we can't just push the door, we will create a WalkRoundEntity task to deal with it
		return true;
	}

	return false;
}

CTaskNavBase * CEventObjectCollision::GetRouteTarget(const CPed * pPed, const Vector3 & vGoToPointTarget, Vector3 & vOut_RouteTarget)
{
	vOut_RouteTarget = vGoToPointTarget;

	const Vector3 vPedPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());

	CTaskNavBase * pTaskNavBase = dynamic_cast<CTaskNavBase*> ((CTaskMoveFollowNavMesh*) pPed->GetPedIntelligence()->FindTaskActiveMovementByType(CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH));
	if(!pTaskNavBase)
	{
		pTaskNavBase = dynamic_cast<CTaskNavBase*> ((CTaskMoveWander*) pPed->GetPedIntelligence()->FindTaskActiveMovementByType(CTaskTypes::TASK_MOVE_WANDER));
	}

	if(pTaskNavBase)
	{
		if( pTaskNavBase->GetState()==CTaskMoveFollowNavMesh::NavBaseState_FollowingPath && pTaskNavBase->GetRoute() && pTaskNavBase->GetRoute()->GetSize() > 0 )
		{
			const Vector3 & vNavMeshTarget = pTaskNavBase->GetRoute()->GetPosition( Min(pTaskNavBase->GetProgress(), pTaskNavBase->GetRoute()->GetSize()-1) );
			vOut_RouteTarget = (vGoToPointTarget-vPedPos).XYMag2() < (vNavMeshTarget-vPedPos).XYMag2() ? vNavMeshTarget : vGoToPointTarget;
		}
	}

	return pTaskNavBase;
}

bool CEventObjectCollision::CanIgnoreCollision(const CPed * pPed, const Vector3 & vCollisionNormal, const Vector3 & vCollisionPos, const Vector3 & vTargetPos)
{
	const Vector3 vPedPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());

	// Ignore collisions whose point of impact is too low to care about
	const CBaseCapsuleInfo * pCapsule = pPed->GetCapsuleInfo();
	const float fZThreshold = Max(pCapsule->GetGroundToRootOffset() - 0.25f, 0.0f);
	const float fZDiff = vPedPos.z - vCollisionPos.z;
	if(fZDiff > fZThreshold)
		return true;

	// Ignore collisions which are mostly in the +/-Z direction
	if(rage::Abs(vCollisionNormal.z) > rage::Abs(vCollisionNormal.x) && rage::Abs(vCollisionNormal.z) > rage::Abs(vCollisionNormal.y))
		return true;

	// If this ped is going somewhere, then only accept collisions whose normal
	// is facing in the opposite direction to the vector towards the target
	CTaskMove * pMoveTask = (CTaskMove*)pPed->GetPedIntelligence()->GetActiveSimplestMovementTask();
	if(pMoveTask && pMoveTask->HasTarget())
	{
		/*
		const Vector3 vMoveTarget = pMoveTask->GetTarget();
		*/

		Vector3 vToTarget = vTargetPos - vPedPos;
		if(vToTarget.Mag2() < 0.01f)
			return true;
		vToTarget.Normalize();

		const float fDot = DotProduct(vCollisionNormal, vToTarget);
		static dev_float fMinDot = 0.0f;
		if(fDot > fMinDot)
			return true;
	}

	CTaskMotionBase* pTask = pPed->GetCurrentMotionTask();
	if (pTask && pTask->GetTaskType()==CTaskTypes::TASK_MOTION_BASIC_LOCOMOTION)
	{
		CTaskMotionBasicLocomotion* pTaskLocomotion = (CTaskMotionBasicLocomotion*)pTask;

		if (pTaskLocomotion->IsStartingToMove() || pTaskLocomotion->GetIsStopping())
		{
			return true;
		}
	}
	return false;
}

bool CEventObjectCollision::IsHeadOnCollision(const CPed& ped) const
{
	Vector3 vPedDir = ped.GetVelocity();
	vPedDir.z = 0.0f;
	vPedDir.NormalizeSafe();

	Vector3 vNormal=m_vNormal;
	vNormal.z=0;
	vNormal.Normalize();
	const float fDot=-vPedDir.Dot(vNormal);
	const bool bIsHeadOnCollision=(fDot>ms_fStraightAheadDotProduct);
	return bIsHeadOnCollision;
}

bool CEventObjectCollision::TakesPriorityOver(const CEvent&) const
{
	return true;
}

bool CEventObjectCollision::CreateResponseTask(CPed* pPed, CEventResponse& response) const
{
	CTask* pTaskActiveSimplest=pPed->GetPedIntelligence()->GetTaskActiveSimplest();
	if(!pTaskActiveSimplest)
	{
		return false;
	}
	if(pPed->GetIsInVehicle())
		return false;

	CTaskMove * pSimplestMoveTask = pPed->GetPedIntelligence()->GetActiveSimplestMovementTask();

	if(CPedIntelligence::ms_bPedsRespondToObjectCollisionEvents && pSimplestMoveTask && pSimplestMoveTask->GetMoveInterface()->HasTarget() && (pSimplestMoveTask->IsTaskMoving() || pPed->IsPlayer()) )
	{
		//Create the response task.
		CObject* pObject = GetCObject();
		if(pObject)
		{
			// const Vector3 vPedPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
			const bool bIsDoor = pObject->IsADoor() && CDoor::DoorTypeOpensForPedsWhenNotLocked(((CDoor*)m_pObject.Get())->GetDoorType());

			//Get the collision details.
			// + Being that GetMoveBlendRatio relies on player input simply tapping the stick would move the player forward
			// resulting in a door collision that was ignored here. We now make an exception with doors to allow for
			// the arm fixup to always happen.
			float fMoveBlendRatio=GetMoveBlendRatio();        
			if(fMoveBlendRatio == MOVEBLENDRATIO_STILL && !bIsDoor)
				return false;

			CTaskMoveInterface* pTaskMove = pSimplestMoveTask->GetMoveInterface();
			CBaseModelInfo * pModelInfo = pObject->GetBaseModelInfo();
			const bool bIsFixed = (pObject->IsBaseFlagSet(fwEntity::IS_FIXED));
			const bool bIsFixedForNav = pModelInfo->GetIsFixedForNavigation();

			const Vector3 & vGoToTarget = pTaskMove->GetTarget();

			//****************************************************************************
			//	Special check for doors : we will want to do a walk-round-entity response
			//  if we are pushing up against the door, but it is at its joint limit.
			//	All other situations we will not avoid it, but just push it open instead.

			bool bPushingAgainstDoorLimit = false;
			if(bIsDoor && !(bIsFixed || bIsFixedForNav) && pTaskMove && pTaskMove->HasTarget())
			{
				const Vector3 vPedPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
				CEntityBoundAI bound(*pObject, vPedPosition.z, pPed->GetCapsuleInfo()->GetHalfWidth());
				const int iPedSide = bound.ComputeHitSideByPosition(vPedPosition);
				const int iTargetSide = bound.ComputeHitSideByPosition(vGoToTarget);
				if(iPedSide != iTargetSide)
				{
					Vector3 vMoveDir = vGoToTarget - vPedPosition;
					vMoveDir.Normalize();
					bPushingAgainstDoorLimit = static_cast<CDoor*>(pObject)->GetDoorIsAtLimit(vPedPosition, vMoveDir);
				}
			}

			// If we are following a navmesh route, then examine the current route progress.
			// If this waypoint is further away than the goto target, then use this instead.
			// This is because the pTaskMove target will likely be a lookahead position quite close
			// to the ped, and therefore not particularly suitable as a target for the walk-round-entity response.

			Vector3 vRouteTarget;
			CTaskNavBase * pTaskNavBase = GetRouteTarget(pPed, vGoToTarget, vRouteTarget);

			if(pTaskNavBase && pTaskNavBase->GetState()==CTaskMoveFollowNavMesh::NavBaseState_MovingWhilstWaitingForPath)
			{
				return false;
			}

			//*************************************************************
			//	If we collide with an entity then try to walk around it.

			if (pPed->GetPedResetFlag(CPED_RESET_FLAG_IsExactStopping))
			{
				CTask* pMoveTask = pPed->GetPedIntelligence()->GetActiveMovementTask();
				if (pMoveTask && pMoveTask->IsMoveTask())
					fMoveBlendRatio = ((CTaskMove*)pMoveTask)->GetMoveBlendRatio();

			}

			const bool bTooSlowToPass = (fMoveBlendRatio < MBR_WALK_BOUNDARY);

			if( !bTooSlowToPass &&
				!GetJustPushDoor() &&
				!pPed->GetMotionData()->GetPlayerHasControlOfPedThisFrame() &&
				pTaskMove->HasTarget() &&
				(!bIsDoor || bPushingAgainstDoorLimit) &&
				// Don't walk rnd fixed objects when following a route, as the navmesh cuts around them
				(!(pPed->GetPedResetFlag( CPED_RESET_FLAG_FollowingRoute ) && (bIsFixed||bIsFixedForNav))) )
			{
				if( pTaskNavBase && !bIsDoor )
				{
					if( pTaskNavBase->GetTimeSinceLastScanForObstructions() > 250 )
					{
						pTaskNavBase->SetScanForObstructionsNextFrame();
					}
				}
				else if (!pPed->GetPedResetFlag(CPED_RESET_FLAG_DontWalkRoundObjects))
				{
					CTaskWalkRoundEntity * pWlkRndTask = rage_new CTaskWalkRoundEntity(fMoveBlendRatio, vRouteTarget, pObject);

					pWlkRndTask->SetLineTestAgainstBuildings(true);			
					if(bIsDoor)
						pWlkRndTask->SetLineTestAgainstObjects(true);
					if(bPushingAgainstDoorLimit)
						pWlkRndTask->SetTestWhetherPedHasClearedEntity(true);

					response.SetEventResponse(CEventResponse::MovementResponse, pWlkRndTask);
				}
			}
		}
	}

	return response.HasEventResponse();
}

//////////////////////
//DRAGGED OUT CAR EVENT
//////////////////////

CEventDraggedOutCar::CEventDraggedOutCar(const CVehicle* pVehicle, const CPed* pDraggingPed, const bool bIsDriver, const bool bForceFlee)
: CEventEditableResponse(),
m_pDraggingPed((CPed*)pDraggingPed),
m_pVehicle((CVehicle*)pVehicle),
m_bWasDriver(bIsDriver),
m_bForceFlee(bForceFlee),
m_bSwitchToNM(false)
{
#if !__FINAL
	m_EventType = EVENT_DRAGGED_OUT_CAR;
#endif
}

CEventDraggedOutCar::~CEventDraggedOutCar()
{
}

int CEventDraggedOutCar::GetEventPriority() const 
{
	if (m_bSwitchToNM)
	{
		return E_PRIORITY_DAMAGE_WITH_RAGDOLL;
	}
	else
	{
		return E_PRIORITY_DRAGGED_OUT_CAR;
	}
}


bool CEventDraggedOutCar::AffectsPedCore(CPed* pPed) const
{
	//Ensure the ped is not a player.
	if(pPed->IsPlayer())
	{
		return false;
	}

	//Ensure the ped is not injured.
	if(pPed->IsInjured())
	{
		return false;
	}

	//Ensure the dragging ped is valid.
	if(!m_pDraggingPed)
	{
		return false;
	}

	//Ensure the vehicle is valid.
	if(!m_pVehicle)
	{
		return false;
	}

	return true;
}

bool CEventDraggedOutCar::CreateResponseTask(CPed* pPed, CEventResponse& response) const
{
	CVehicle* pVehicle=GetVehicle();
	CPed* pDraggingPed=GetDraggingPed();

	if (m_bSwitchToNM && CTaskNMBehaviour::CanUseRagdoll(pPed, RAGDOLL_TRIGGER_VEHICLE_JUMPOUT))
	{
		nmEntityDebugf(pPed, "CEventDraggedOutCar - Creating jump from road vehicle task.");
		CTaskNMControl* pNmControlTask = rage_new CTaskNMControl(6000, 6000, rage_new CTaskNMJumpRollFromRoadVehicle(1000, 6000, true, false, atHashString::Null(), pVehicle), CTaskNMControl::DO_BLEND_FROM_NM);
		response.SetEventResponse(CEventResponse::PhysicalResponse, pNmControlTask);
	}
	else if (m_bSwitchToNM)
	{
		m_bSwitchToNM = false;
	}

	if(!pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_PreventDraggedOutOfCarThreatResponse))
	{
		//Create the threat response task.
		CTaskThreatResponse* pTaskThreatResponse = CTaskThreatResponse::CreateForTargetDueToEvent(pPed, pDraggingPed, *this);

		//Check if we should force flee.
		bool bFlee = m_bForceFlee || (pVehicle->InheritsFromBoat());
		if(bFlee)
		{
			pTaskThreatResponse->SetThreatResponseOverride(CTaskThreatResponse::TR_Flee);
		}

		if( pVehicle->ContainsPed(pPed) )
		{
			CTaskSequenceList* pSequence=rage_new CTaskSequenceList;
			CTask* pLeaveCar=0;

			//pPed still in vehicle so get the ped out the vehicle.
			VehicleEnterExitFlags vehicleFlags;
			vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::ForcedExit);
			pLeaveCar = rage_new CTaskExitVehicle(pVehicle,vehicleFlags, 0.0f,pDraggingPed);
			pSequence->AddTask(pLeaveCar);
			pSequence->AddTask(pTaskThreatResponse);

			response.SetEventResponse(CEventResponse::EventResponse, rage_new CTaskUseSequence(*pSequence));
		}
		else
		{
			response.SetEventResponse(CEventResponse::EventResponse, pTaskThreatResponse);
		}
	}

	return response.HasEventResponse();
}

CEntity* CEventDraggedOutCar::GetSourceEntity() const
{
	return m_pDraggingPed;
}

CEventExplosion::CEventExplosion(Vec3V_In vPosition, CEntity* pOwner, float fRadius, bool bIsMinor)
: CEventEditableResponse()
, m_vPosition(vPosition)
, m_pOwner(pOwner)
, m_fRadius(fRadius)
, m_bIsMinor(bIsMinor)
, m_bIsTemporaryEvent(true)
{

}

CEventExplosion::~CEventExplosion()
{

}

bool CEventExplosion::AffectsPedCore(CPed* pPed) const
{
	//Ensure the explosion is not minor.
	if(m_bIsMinor)
	{
		return false;
	}

	//Ensure the ped is not injured.
	if(pPed->IsInjured())
	{
		return false;
	}

	//Ensure the ped is not a player.
	if(pPed->IsAPlayerPed())
	{
		return false;
	}

	//Ensure the flag is not set.
	if(pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_DisableExplosionReactions))
	{
		return false;
	}

	//Ensure the ped is on foot.
	if(!pPed->GetIsOnFoot())
	{
		return false;
	}

	//Check if we are within the max look-at distance.
	ScalarV scDistSq = DistSquared(pPed->GetTransform().GetPosition(), m_vPosition);
	ScalarV scMaxLookAtDistSq = ScalarVFromF32(square(CTaskReactToExplosion::GetMaxLookAtDistance(m_fRadius)));
	if(IsLessThanAll(scDistSq, scMaxLookAtDistSq))
	{
		//Do a head look towards the explosion.
		Vector3 vPosition = VEC3V_TO_VECTOR3(m_vPosition);
		pPed->GetIkManager().LookAt(0, NULL, 2000, BONETAG_INVALID, &vPosition, 0, 500, 500, CIkManager::IK_LOOKAT_HIGH);
	}
	
	//Ensure the ped is within the max reaction distance.
	ScalarV scMaxReactionDistSq = ScalarVFromF32(square(CTaskReactToExplosion::GetMaxReactionDistance(m_fRadius)));
	if(IsGreaterThanAll(scDistSq, scMaxReactionDistSq))
	{
		return false;
	}
	
	//When peds are in combat, allow the combat task to handle the explosion.
	CTaskCombat* pCombatTask = static_cast<CTaskCombat *>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_COMBAT));
	if(pCombatTask)
	{
		//Allow the combat task to handle the explosion.
		pCombatTask->OnExplosion(*this);
		return false;
	}

	// When in cowering, allow the cower task to handle the explosion.
	CTaskCowerScenario* pCowerTask = static_cast<CTaskCowerScenario*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_COWER_SCENARIO));
	if (pCowerTask)
	{
		// Update the cowering status.
		if (pCowerTask->ShouldUpdateCowerStatus(*this))
		{
			pCowerTask->UpdateCowerStatus(*this);
		}
	}

	return true;
}

bool CEventExplosion::IsEventReady(CPed* pPed)
{
	//Set the temporary event flag.
	m_bIsTemporaryEvent = (pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_COMBAT) ||
		CTaskReactToExplosion::IsTemporary(*pPed, m_pOwner));

	return CEventEditableResponse::IsEventReady(pPed);
}

bool CEventExplosion::TakesPriorityOver(const CEvent& otherEvent) const
{
	//Check the other event type.
	switch(otherEvent.GetEventType())
	{
		case EVENT_SHOCKING_POTENTIAL_BLAST:
		{
			return false;
		}
		default:
		{
			break;
		}
	}

	return CEventEditableResponse::TakesPriorityOver(otherEvent);
}

/////////////////
//REVIVED
/////////////////

CEventRevived::CEventRevived(CPed* pMedic, CVehicle* pAmbulance, bool bRevived )
:  CEvent(),
m_pMedic(pMedic),
m_pAmbulance(pAmbulance),
m_bRevived(bRevived)
{
}

CEventRevived::~CEventRevived()
{
}

bool CEventRevived::AffectsPedCore(CPed* pPed) const
{
	// don't let mission peds be revived
	if(pPed->PopTypeIsMission())
		return false;

	if (!pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_AllowMedicsToReviveMe))
	{
		return false;
	}

	//Can only be revived if ped is dead.
	return ( pPed->IsDead() );
}

int CEventRevived::GetEventPriority() const
{
	if( m_bRevived )
	{
		return E_PRIORITY_REVIVED_AFTER_REVIVED;
	}
	else
	{
		return E_PRIORITY_REVIVED;
	}
}

bool CEventRevived::CreateResponseTask(CPed* UNUSED_PARAM(pPed), CEventResponse& response) const
{
	response.SetEventResponse(CEventResponse::EventResponse, rage_new CTaskRevive(GetTreatingMedic(), GetAmbulance()));
	return response.HasEventResponse();
}

///////////////////////
//PED JACKING VEHICLE EVENT
///////////////////////

IMPLEMENT_GENERAL_EVENT_TUNABLES(CEventPedJackingMyVehicle, 0x2b994b08);
CEventPedJackingMyVehicle::Tunables CEventPedJackingMyVehicle::sm_Tunables;

CEventPedJackingMyVehicle::CEventPedJackingMyVehicle(CVehicle* pVehicle, CPed* pJackedPed, const CPed* pJackingPed)
: CEvent()
, m_pVehicle(pVehicle)
, m_pJackedPed(pJackedPed)
, m_pJackingPed(pJackingPed)
{
#if !__FINAL
	m_EventType = EVENT_PED_JACKING_MY_VEHICLE;
#endif
}

CEventPedJackingMyVehicle::~CEventPedJackingMyVehicle()
{
}

bool CEventPedJackingMyVehicle::AffectsPedCore(CPed* pPed) const
{
	//Ensure the ped is not injured.
	if(pPed->IsInjured())
	{
		return false;
	}
	
	//Ensure the ped is in a vehicle.
	if(!pPed->GetIsInVehicle())
	{
		return false;
	}
	
	//Ensure the ped is not a player.
    if(pPed->IsPlayer() && !m_pJackingPed->IsPlayer())
	{
		return false;
	}
	
	//Ensure the prevention config flag is not set.
	if(pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_PreventPedFromReactingToBeingJacked))
	{
		return false;
	}

	return true;
}

CEntity* CEventPedJackingMyVehicle::GetSourceEntity() const
{
	//Grab the ped.
	return const_cast<CPed *>(m_pJackingPed.Get());
}

bool CEventPedJackingMyVehicle::CreateResponseTask(CPed* pPed, CEventResponse& response) const
{
	//Clear the current event response.
	response.ClearEventResponse(CEventResponse::EventResponse);

	//Create the task.
	CTask* pTask = rage_new CTaskReactToBeingJacked(m_pVehicle, m_pJackedPed, m_pJackingPed);

	pPed->GetPedAudioEntity()->HandleCarWasJacked();

	//Set the task as the response.
	response.SetEventResponse(CEventResponse::EventResponse, pTask);

	return response.HasEventResponse();
}

///////////////////////
//PED ENTERED VEHICLE EVENT
///////////////////////

CEventPedEnteredMyVehicle::CEventPedEnteredMyVehicle(const CPed* pPedThatEnteredVehicle, const CVehicle* pVehicle)
: CEventEditableResponse(),
m_pPedThatEnteredVehicle((CPed*)pPedThatEnteredVehicle),
m_pVehicle((CVehicle*)pVehicle)
{
#if !__FINAL
	m_EventType = EVENT_PED_ENTERED_MY_VEHICLE;
#endif
}

CEventPedEnteredMyVehicle::~CEventPedEnteredMyVehicle()
{
}

bool CEventPedEnteredMyVehicle::AffectsPedCore(CPed* pPed) const
{	
	if(pPed->IsDead())
	{
		return false;
	}

	//if(!((pPed->m_pMyVehicle)&&(pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ))))
	if(!pPed->GetMyVehicle() || !pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ))
	{
		return false;
	}

	if(pPed->GetMyVehicle()!=m_pVehicle)
	{
		return false;
	}

	if( pPed->GetMyVehicle()->GetVehicleType() == VEHICLE_TYPE_TRAIN )
		return false;

	if(0==m_pPedThatEnteredVehicle)
	{
		return false;
	}

	// Don't react if we are the ones that entered the vehicle
	if(pPed == m_pPedThatEnteredVehicle)
	{
		return false;
	}

	if(pPed->GetPedResetFlag(CPED_RESET_FLAG_DisablePedEnteredMyVehicleEvents))
	{
		return false;
	}

	//Ensure the ped is not running threat response.
	if(pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_THREAT_RESPONSE))
	{
		return false;
	}

	//Ensure the ped is not running flee.
	if(pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_SMART_FLEE))
	{
		return false;
	}

	//Ensure the ped is not running combat.
	if(pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_COMBAT))
	{
		return false;
	}

	return true;
}

bool CEventPedEnteredMyVehicle::CreateResponseTask(CPed* pPed, CEventResponse& response) const
{
	Assert(pPed);

	CPed* pPedThatEnteredVehicle=GetPedThatEnteredVehicle();
	if(0==pPedThatEnteredVehicle)
	{
		return false;
	}    

	CVehicle* pVehicle=GetVehicle();
	if(0==pVehicle)
	{
		return false;
	}

	if(!pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ))
	{
		return false;
	}

	Assert(pPed->GetMyVehicle());
	if(pPed->GetMyVehicle()->IsDriver(pPed))
	{
		//	    m_pPed->Say(CONTEXT_GLOBAL_JACKED_IN_CAR);
	}


	//Ensure the ped is either random, or not friendly with the entering ped.
	if(!pPed->PopTypeIsRandom() && pPed->GetPedIntelligence()->IsFriendlyWith(*pPedThatEnteredVehicle))
	{
		return false;
	}

	//Register this as an 'interesting event' (focus is the vehicle, not the ped!)
	g_InterestingEvents.Add(CInterestingEvents::ECarJacking, pVehicle);
	
	//Clear the current event response.
	response.ClearEventResponse(CEventResponse::EventResponse);
	
	//Create the task.
	CTask* pTask = CTaskThreatResponse::CreateForTargetDueToEvent(pPed, pPedThatEnteredVehicle, *this);
	
	//Set the task as the response.
	response.SetEventResponse(CEventResponse::EventResponse, pTask);
	
	return response.HasEventResponse();
}

CEntity* CEventPedEnteredMyVehicle::GetSourceEntity() const
{
	return m_pPedThatEnteredVehicle;
}

////////////////////
//PED TO CHASE
////////////////////

CEventPedToChase::CEventPedToChase(CPed* pPedToChase)
: CEvent(),
m_pPedToChase(pPedToChase)
{
#if !__FINAL
	m_EventType = EVENT_PED_TO_CHASE;
#endif
}

CEventPedToChase::~CEventPedToChase()
{
}

bool CEventPedToChase::AffectsPedCore(CPed* pPed) const
{
	if(pPed->IsDead())
	{
		return false;
	}

	return true;
}

CEntity* CEventPedToChase::GetSourceEntity() const
{
	return GetPedToChase();
}

////////////////////
//PED TO FLEE
////////////////////

CEventPedToFlee::CEventPedToFlee(CPed* pPedToFlee)
: CEvent(),
m_pPedToFlee(pPedToFlee)
{
#if !__FINAL
	m_EventType = EVENT_PED_TO_FLEE;
#endif
}

CEventPedToFlee::~CEventPedToFlee()
{
}

bool CEventPedToFlee::AffectsPedCore(CPed* pPed) const
{
	if(pPed->IsDead())
	{
		return false;
	}

	return true;
}

CEntity* CEventPedToFlee::GetSourceEntity() const
{
	return GetPedToFlee();
}
///////////////////
//ACQUAINTANCE PED
///////////////////

CEventAcquaintancePed::CEventAcquaintancePed(CPed* pAcquaintancePed)
: CEventEditableResponse(),
m_pAcquaintancePed(pAcquaintancePed)
{

}

CEventAcquaintancePed::~CEventAcquaintancePed()
{
}

bool CEventAcquaintancePed::TakesPriorityOver(const CEvent& otherEvent) const
{
	if(otherEvent.GetEventType()==GetEventType())
	{
		const CEventAcquaintancePed& otherAcquaintanceEvent=(const CEventAcquaintancePed&)otherEvent;
		const bool bIsPlayer=m_pAcquaintancePed ? m_pAcquaintancePed->IsPlayer() : false;		
		const bool bIsOtherPlayer=otherAcquaintanceEvent.m_pAcquaintancePed ? otherAcquaintanceEvent.m_pAcquaintancePed->IsPlayer() : false;

		if(bIsPlayer && bIsOtherPlayer)
		{
			//Both involve player so otherEvent will do.
			return false;
		}
		else if(bIsPlayer)
		{
			//Only this event involves the player. Always respond to the player.
			return true;
		}
		else if(bIsOtherPlayer)
		{
			//Only other event is the player.  Just carry on with other event.
			return false;
		}
		else
		{
			//Neither is the player so just carry on with other event.
			return false;
		}
	}
	else
	{
		//Events of different type so use the priorities.
		return CEvent::TakesPriorityOver(otherEvent);
	}
}


bool CEventAcquaintancePed::AffectsPedCore(CPed* pPed) const
{		
	if(0==m_pAcquaintancePed)
	{
		return false;
	}

	if(g_fireMan.IsEntityOnFire(pPed))
	{
		//Don't bother having an acquaintance if the ped is on fire.
		return false;
	}

	if(pPed->IsDead())
	{
		return false;
	}

	if(m_pAcquaintancePed->IsDead())
	{
		return false;
	}

	if(CPedGroups::AreInSameGroup(pPed,m_pAcquaintancePed))
	{
		return false;
	}

	return true;
}

bool CEventAcquaintancePed::ShouldCreateResponseTaskForPed(const CPed* pPed) const
{
	if( CTaskTypes::IsCombatTask(GetResponseTaskType()) && 
		pPed && !pPed->GetPedIntelligence()->CanAttackPed(m_pAcquaintancePed) )
	{
		return false;
	}

	return true;
}

CEntity* CEventAcquaintancePed::GetSourceEntity() const
{
	return m_pAcquaintancePed;
}

bool CEventAcquaintancePedHate::AffectsPedCore(CPed* pPed) const
{
	const bool b=CEventAcquaintancePed::AffectsPedCore(pPed);
	
	if(!b)
	{
		return false;
	}

	if (pPed->IsLocalPlayer())
	{
		return false;
	}

	// You can't hate someone you like
	if( pPed->GetPedIntelligence()->IsFriendlyWith(*m_pAcquaintancePed) )
	{
		return false;
	}

	// Ignore this event if the ped is set to ignore injured peds.
	if( m_pAcquaintancePed->IsInjured() && !pPed->WillAttackInjuredPeds() )
	{
		return false;
	}

	// Ignore if already in combat (allow the event to go through if we'd ignore it anyways and we want to test for hated peds)
	if( pPed->GetPedIntelligence()->FindTaskByType(CTaskTypes::TASK_COMBAT) &&
		(!pPed->GetPedResetFlag(CPED_RESET_FLAG_CanPedSeeHatedPedBeingUsed) || !pPed->GetBlockingOfNonTemporaryEvents()) )
	{
		return false;
	}

// 	int iEventType = pPed->GetPedIntelligence()->GetCurrentEventType();
// 
// 	// Check previous events, if we've already reacted, don't bother with this event
// 	switch (iEventType)
// 	{
// 	case EVENT_REACTION_INVESTIGATE_THREAT:
// 	case EVENT_SUSPICIOUS_ACTIVITY:
// 		{
// 			float fDistToPed = pPed->GetPosition().Dist(m_pAcquaintancePed->GetPosition());
// 			float fIdentDist = pPed->GetPedIntelligence()->GetPedPerception().GetIdentificationRange();
// 			if (fDistToPed  < fIdentDist)
// 			{
// 				return true; // Within identification range, accept the event
// 			}
// 			else
// 			{
// 				return false;
// 			}
// 		}
// 		break;
// 	case EVENT_REACTION_ENEMY_PED:
// 	case EVENT_ACQUAINTANCE_PED_HATE:
// 		return false;
// 	default: 
// 		break;
// 	}

	// Affect the react to dead ped task
	CTaskReactToDeadPed* pReactTask = static_cast<CTaskReactToDeadPed*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_REACT_TO_DEAD_PED));
	if (pReactTask)
	{
		aiFatalAssertf(GetSourceEntity(),"No Source Entity Exists for this event!");
		// The guy who I hate most likely killed him
		aiFatalAssertf(GetSourceEntity()->GetIsTypePed(),"Isn't a ped!");
		pReactTask->SetThreatPed(static_cast<CPed*>(GetSourceEntity()));
		return true;
	}

	return true;
}

bool CEventAcquaintancePedDead::CreateResponseTask(CPed* pPed, CEventResponse& response) const
{
	if (pPed->IsAPlayerPed())
	{
		return false;
	}

	pPed->GetPedIntelligence()->IncreaseAlertnessState();

	aiFatalAssertf(m_pAcquaintancePed->GetIsTypePed(), "The entity is not a ped");

	CTask* pTask = rage_new CTaskInvestigate(VEC3V_TO_VECTOR3(m_pAcquaintancePed->GetTransform().GetPosition()));
	
	response.SetEventResponse(CEventResponse::EventResponse, pTask);

	return response.HasEventResponse();
}

bool CEventAcquaintancePedDead::AffectsPedCore(CPed* pPed) const
{
	if (!m_pAcquaintancePed)
	{
		return false;
	}
	
	if (pPed->IsLocalPlayer())
	{
		return false;
	}

	// You only react to dead peds who we're friendly with
	if (!pPed->GetPedIntelligence()->IsFriendlyWith(*m_pAcquaintancePed))
	{
		return false;
	}

	// Ignore if already in combat or reacting
	if (pPed->GetPedIntelligence()->FindTaskByType(CTaskTypes::TASK_COMBAT))
	{
		return false;
	}

	if (!pPed->GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_WillScanForDeadPeds))
	{
		return false;
	}

	Vector3 vDeadPedPos = VEC3V_TO_VECTOR3(m_pAcquaintancePed->GetTransform().GetPosition());
	const Vector3 vPedPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
	// Only accept event if the ped can see the dead ped
	Vector3 vDiff = vPedPosition - vDeadPedPos;
	float fDist2 = vDiff.Mag2();
	float fSeeingRange = pPed->GetPedIntelligence()->GetPedPerception().GetSeeingRange();

	if (fDist2 > (fSeeingRange * fSeeingRange))
	{
		// Can't see dead ped
		return false;
	}

	Vector3	vToDeadPed = vDeadPedPos - vPedPosition;
	if (DotProduct(VEC3V_TO_VECTOR3(pPed->GetTransform().GetB()), vToDeadPed) <= 0)
	{
		// Ped is facing away from dead ped
		return false;
	}

	const u32 includeFlags = ArchetypeFlags::GTA_ALL_TYPES_MOVER;
	const u32 typeFlags = 0xffffffff;
	// Possibility of having 2 full cars excluded from the test, so make the array big enough
	const int EXCLUSION_MAX = MAX_INDEPENDENT_VEHICLE_ENTITY_LIST * 2;
	const CEntity* exclusionList[EXCLUSION_MAX];
	int nExclusions = 0;

	// Exclude the reacting ped and the m_pFiringEntity from the collision check
	if(pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) && pPed->GetMyVehicle())
	{
		// Exclude pPed's vehicle as well
		pPed->GetMyVehicle()->GeneratePhysExclusionList(exclusionList, nExclusions, EXCLUSION_MAX,includeFlags,typeFlags, pPed->GetMyVehicle());
	}
	else
	{
		pPed->GeneratePhysExclusionList(exclusionList, nExclusions, EXCLUSION_MAX,includeFlags,typeFlags, pPed);
	}

	if (m_pAcquaintancePed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) && m_pAcquaintancePed->GetMyVehicle())
	{
		// Exclude m_pFiringEntity's vehicle as well
		m_pAcquaintancePed->GetMyVehicle()->GeneratePhysExclusionList(exclusionList, nExclusions, EXCLUSION_MAX,includeFlags,typeFlags, m_pAcquaintancePed->GetMyVehicle());
	}
	else
	{
		m_pAcquaintancePed->GeneratePhysExclusionList(exclusionList, nExclusions, EXCLUSION_MAX,includeFlags,typeFlags, m_pAcquaintancePed);
	}

	// Do a LOS check

	// If the peds skeleton has been updated then use their head bone as the end point for the LOS check
	// Otherwise just use the root bone
	Vector3 vLOSEndPos;
	pPed->GetBonePosition(vLOSEndPos, BONETAG_HEAD);

	// Fire a probe from m_vShotOrigin to vLOSEndPos - if its clear then
	WorldProbe::CShapeTestProbeDesc probeDesc;
	probeDesc.SetStartAndEnd(vDeadPedPos, vLOSEndPos);
	probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_TYPES_MOVER);
	probeDesc.SetExcludeEntities(exclusionList, nExclusions);
	probeDesc.SetContext(WorldProbe::LOS_GeneralAI);

	if(WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc))
	{
		// Ped is facing source of shot but can't see it
		return false;
	}

	return true;
}

bool CEventAcquaintancePedDislike::AffectsPedCore( CPed* pPed ) const
{
	const bool b=CEventAcquaintancePed::AffectsPedCore(pPed);
	if(!b)
	{
		return false;
	}

	// You can't hate someone you like
	if( pPed->GetPedIntelligence()->IsFriendlyWith(*m_pAcquaintancePed) )
	{
		return false;
	}

	// Or mission peds who aren't guards
	if (pPed->PopTypeIsMission())
	{
		return false;
	}

	return true;
}

void CEventAcquaintancePedWanted::OnEventAdded(CPed *pPed) const 
{
	CEvent::OnEventAdded(pPed);

	if(m_pAcquaintancePed)
	{
		if(pPed->GetPedIntelligence()->IsThreatenedBy(*m_pAcquaintancePed) && pPed->GetPedIntelligence()->CanAttackPed(m_pAcquaintancePed))
		{
			CPedTargetting* pPedTargeting = pPed->GetPedIntelligence()->GetTargetting(true);
			if(pPedTargeting)
			{
				pPedTargeting->RegisterThreat(m_pAcquaintancePed);
			}
		}
	}
}

bool CEventAcquaintancePedWanted::AffectsPedCore(CPed* pPed) const
{
	const bool b=CEventAcquaintancePed::AffectsPedCore(pPed);
	if(!b)
	{
		return false;
	}

	if(m_pAcquaintancePed)
	{
		// Check if we still have a wanted relationship feeling towards the other ped
		CRelationshipGroup* pRelGroup = pPed->GetPedIntelligence()->GetRelationshipGroup();
		const int otherRelGroupIndex = m_pAcquaintancePed->GetPedIntelligence()->GetRelationshipGroupIndex();
		if(pRelGroup && !pRelGroup->CheckRelationship(ACQUAINTANCE_TYPE_PED_WANTED, otherRelGroupIndex))
		{
			// We don't have a wanted relationship feeling, so if this isn't a network game then we aren't affected by this event anymore
			if(!NetworkInterface::IsGameInProgress())
			{
				return false;
			}
			
			// In network games we might have wanted players and cops who don't have rel groups with wanted feelings
			// so if the target is dead, doesn't have a wanted object or is not wanted then we are no longer affected
			bool bHeliLawPed = pPed->IsLawEnforcementPed() && pPed->GetMyVehicle() && pPed->GetMyVehicle()->InheritsFromHeli();
			if(m_pAcquaintancePed->IsDead() || !m_pAcquaintancePed->GetPlayerWanted() || m_pAcquaintancePed->GetPlayerWanted()->GetWantedLevel() == WANTED_CLEAN || (bHeliLawPed && m_pAcquaintancePed->GetPedConfigFlag(CPED_CONFIG_FLAG_DisableWantedHelicopterSpawning)))
			{
				return false;
			}

			// Lastly, we need to make sure the ped being affected is a cop. So if they aren't a law enforcement ped or part of the cop team, they are no longer affected
			bool bIsLawEnforcementPed = pPed->ShouldBehaveLikeLaw();

			// url:bugstar:2557284 - eTEAM_COP isn't used any more, script use teams 0-3 freely with assuming certain team types.
			// || (pPed->IsPlayer() && pPed->GetNetworkObject() && pPed->GetNetworkObject()->GetPlayerOwner() && pPed->GetNetworkObject()->GetPlayerOwner()->GetTeam() == eTEAM_COP);

			if(!bIsLawEnforcementPed)
			{
				return false;
			}
		}

		//Ensure we should not ignore the hostile event.
		if(CTaskAgitated::ShouldIgnoreEvent(*pPed, *this))
		{
			return false;
		}
	}

	return true;
}

////////////////////
// A ped is encroaching (or has encroached) into our "personal space"
////////////////////

IMPLEMENT_GENERAL_EVENT_TUNABLES(CEventEncroachingPed, 0x7445af93);
CEventEncroachingPed::Tunables CEventEncroachingPed::sm_Tunables;

CEventEncroachingPed::CEventEncroachingPed(CPed *pClosePed) :
	CEventEditableResponse	()
,	m_pClosePed				(pClosePed)
{
#if !__FINAL
	m_EventType = EVENT_ENCROACHING_PED;
#endif
}

CEventEncroachingPed::~CEventEncroachingPed()
{
}

bool CEventEncroachingPed::AffectsPedCore(CPed* pPed) const
{
	if(pPed->IsDead())
	{
		return false;
	}

	return true;
}

CEntity *CEventEncroachingPed::GetSourceEntity() const
{
	return GetEncroachingPed();
}

/////////////////
//SCRIPT COMMAND
/////////////////

CEventScriptCommand::CEventScriptCommand(const int iTaskPriority, aiTask* pTask, const bool bAcceptWhenDead)
: m_iTaskPriority(iTaskPriority),
m_pTask(pTask),
m_bAcceptWhenDead(bAcceptWhenDead)
{
#if !__FINAL
	m_EventType = EVENT_SCRIPT_COMMAND;
#endif

#if __BANK
	if(pTask && (static_cast<CTask*>(m_pTask.Get()))->IsMoveTask())
	{
		taskAssertf(0, "Creating a move task directly without using a control movement task. This will result in adding a move task to the primary task tree!");
	}
#endif
}

CEventScriptCommand::~CEventScriptCommand()
{
	CPedScriptedTaskRecordData* pRecordData = CPedScriptedTaskRecord::GetRecordAssociatedWithEvent(this);
	if( pRecordData )
	{
		pRecordData->Flush();
	}
	if(m_pTask) delete m_pTask;
}

CEvent* CEventScriptCommand::Clone() const
{
	aiTask* p=CloneScriptTask();
	CEventScriptCommand* pEvent = rage_new CEventScriptCommand(m_iTaskPriority,p,m_bAcceptWhenDead);
#if __DEV
	pEvent->SetScriptNameAndCounter(m_ScriptName, m_iProgramCounter);
#endif
	return pEvent;
}

aiTask* CEventScriptCommand::CloneScriptTask() const
{
	aiTask* p=0;
	if(m_pTask)
	{
		p=m_pTask->Copy();
		Assert( p->GetTaskType() == m_pTask->GetTaskType() );
	}
	return p;
}

bool CEventScriptCommand::IsValidForPed(CPed* pPed) const
{
	if(0==pPed)
	{
		return CEvent::IsValidForPed(NULL);
	}
	else
	{
		return ( ( (!pPed->IsDead() && !pPed->IsInjured() && !pPed->IsDeadByMelee()) ) || m_bAcceptWhenDead);
	}
}


bool CEventScriptCommand::AffectsPedCore(CPed* pPed) const
{
#if !__FINAL
	if( pPed->IsInjured() && !m_bAcceptWhenDead)
	{		
		Errorf( "Task (%s) given to injured ped (%s)! Melee Death: %s IsValidForPed: %s", m_pTask ? ((const char*) m_pTask->GetName()) : "NULL", pPed->GetDebugName(), pPed->IsDeadByMelee() ? "T" : "F", IsValidForPed(pPed) ? "T" : "F");
		Assertf(false, "Task (%s) given to injured ped (%s)! Melee Death: %s IsValidForPed: %s", m_pTask ? ((const char*) m_pTask->GetName())  : "NULL", pPed->GetDebugName(), pPed->IsDeadByMelee() ? "T" : "F", IsValidForPed(pPed) ? "T" : "F");
	}
#endif
	return ( ( !pPed->IsInjured() && !pPed->IsDeadByMelee() && !pPed->IsDead()) || m_bAcceptWhenDead );
}

int CEventScriptCommand::GetEventPriority() const	
{
	if(m_bAcceptWhenDead)
	{
		//If the ped will accept this event when dead then it 
		//must take priority over everything.
		return E_PRIORITY_MAX;
	}
	else if(m_pTask)
	{
		// if this is a sequence task, check the first task in the sequence
		if (m_pTask->GetTaskType()==CTaskTypes::TASK_USE_SEQUENCE)
		{
			const CTaskUseSequence* pSeqTask = static_cast<const CTaskUseSequence*>(m_pTask.Get());
			if (pSeqTask->GetTaskSequenceList() && pSeqTask->GetTaskSequenceList()->GetTask(0))
			{
				const CTask* pTask = static_cast<const CTask*>(pSeqTask->GetTaskSequenceList()->GetTask(0));
				if (IsHighPriorityTask(pTask))
				{
					return E_PRIORITY_SCRIPT_COMMAND_SP;
				}
				else if (IsHighPriorityTaskNoNM(pTask))
				{
					return E_PRIORITY_SCRIPT_COMMAND_SP_NO_NM;
				}
			}
		}
		else
		{
			const CTask* pTask = static_cast<const CTask*>(m_pTask.Get());
			if (IsHighPriorityTask(pTask))
			{
				return E_PRIORITY_SCRIPT_COMMAND_SP;
			}
			else if (IsHighPriorityTaskNoNM(pTask))
			{
				return E_PRIORITY_SCRIPT_COMMAND_SP_NO_NM;
			}
		}
	}
	return E_PRIORITY_SCRIPT_COMMAND;
}

bool CEventScriptCommand::IsHighPriorityTask(const CTask* pTask) const
{
	if(pTask->GetTaskType()==CTaskTypes::TASK_COMPLEX_USE_MOBILE_PHONE)
		return true;
	if(pTask->GetTaskType()==CTaskTypes::TASK_SYNCHRONIZED_SCENE)
		return true;
	if(pTask->GetTaskType()==CTaskTypes::TASK_PARACHUTE || pTask->GetTaskType()==CTaskTypes::TASK_JETPACK)
		return true;
	
	if(pTask->GetTaskType()==CTaskTypes::TASK_COMPLEX_CONTROL_MOVEMENT)
	{
		const CTaskComplexControlMovement* pComplexTask = static_cast<const CTaskComplexControlMovement*>(pTask);
		if(pComplexTask->GetMoveTaskType()==CTaskTypes::TASK_MOVE_IN_AIR)
		{
			return true;
		}
	}

	if ( pTask->GetTaskType()==CTaskTypes::TASK_RUN_NAMED_CLIP )
	{
		const CTaskRunNamedClip* pClipTask = static_cast<const CTaskRunNamedClip*>(pTask);
		if (pClipTask->ShouldForceStart())
		{
			return true;
		}
	}
	else if (pTask->GetTaskType()==CTaskTypes::TASK_SCRIPTED_ANIMATION)
	{
		const CTaskScriptedAnimation* pClipTask = static_cast<const CTaskScriptedAnimation*>(pTask);
		if (pClipTask->ShouldForceStart())
		{
			return true;
		}
	}

	return false;
}

bool CEventScriptCommand::IsHighPriorityTaskNoNM(const CTask* pTask) const
{
	if(pTask->GetTaskType() == CTaskTypes::TASK_SMART_FLEE)
	{
		return true;
	}

	return false;
}

bool CEventScriptCommand::TakesPriorityOver(const CEvent& otherEvent) const
{
	if(m_bAcceptWhenDead && otherEvent.GetEventType()==EVENT_DEATH)
	{
		return true;
	} 
	else if(m_bAcceptWhenDead && otherEvent.GetEventType()==EVENT_DAMAGE)
	{
		return true;
	}
	// MUST_FIX_THIS( PH - Scripted tasks specifically need to overwrite damage events due to a tricky priority problem until I can put in a full fix )
	else if( otherEvent.GetEventType()==EVENT_DAMAGE )
	{
		return true;
	}
	else 
	{
		return CEvent::TakesPriorityOver(otherEvent);
	}
}

#if !__NO_OUTPUT
rage::atString CEventScriptCommand::GetDescription() const
{
	rage::atString sDescription = CEvent::GetDescription();

	aiTask* pTask = GetTask();
	if (pTask)
	{
		sDescription += ":";
		sDescription +=  pTask->GetName();
	}

	return sDescription;
}
#endif // !__NO_OUTPUT


#if __DEV
void CEventScriptCommand::SetScriptNameAndCounter( const char* szScriptName, s32 iProgramCounter )
{
	strncpy( m_ScriptName, szScriptName, 31);
	m_ScriptName[31] = '\0';
	m_iProgramCounter = iProgramCounter;
}
#endif

bool CEventScriptCommand::ShouldCreateResponseTaskForPed(const CPed* UNUSED_PARAM(pPed)) const
{
	if (NetworkInterface::IsNetworkOpen())
	{
		if (m_pTask->GetTaskType() == CTaskTypes::TASK_CAR_SET_TEMP_ACTION)
		{
			CTaskCarSetTempAction* pTask = (CTaskCarSetTempAction*) m_pTask.Get();
			if (pTask && pTask->IsTargetVehicleNetworkClone())
			{
				return false;
			}
		}
	}

	return true;
}

bool CEventScriptCommand::CreateResponseTask(CPed* pPed, CEventResponse& response) const
{
	// Some scipt command events are higher priority than ragdoll (because we want to be able to trigger them while the ped is falling, etc)
	// If this is one of those events, and the ped is currently in ragdoll, do an immediate switch to ragdoll if the response task isn't going
	// to handle it.
	if (GetEventPriority()==E_PRIORITY_SCRIPT_COMMAND_SP && pPed->GetUsingRagdoll() && !((const CTask*)m_pTask.Get())->HandlesRagdoll(pPed))
	{
		nmDebugf2("[%u] EventScriptCommand::CreateResponseTask:%s(%p) Switching to animated for task '%s'", fwTimer::GetTimeInMilliseconds(), pPed->GetModelName(), pPed, m_pTask->GetName().c_str());
		pPed->SwitchToAnimated();
	}
	response.SetEventResponse(CEventResponse::EventResponse, CloneScriptTask());
	return response.HasEventResponse();
}

/////////////////
//SCRIPT COMMAND
/////////////////

CEventGivePedTask::CEventGivePedTask(const int iTaskPriority, aiTask* pTask, const bool bAcceptWhenDead,const int iCustomEventPriority, const bool bRequiresAbortForRagdoll, const bool bDeleteActiveTask, const bool bBlockWhenInRagdoll )
: m_iTaskPriority(iTaskPriority),
m_pTask(pTask),
m_bAcceptWhenDead(bAcceptWhenDead),
m_iEventPriority(iCustomEventPriority),
m_bRequiresAbortForRagdoll(bRequiresAbortForRagdoll), 
m_bDeleteActiveTask(bDeleteActiveTask),
m_fMinDelay(-1.0f),
m_fMaxDelay(-1.0f),
m_bBlockWhenInRagdoll(bBlockWhenInRagdoll)
{
#if !__FINAL
	 m_EventType = EVENT_GIVE_PED_TASK;
#endif // !__FINAL
}

CEventGivePedTask::~CEventGivePedTask()
{
	if(m_pTask) delete m_pTask;
}

bool CEventGivePedTask::GetTunableDelay(const CPed* UNUSED_PARAM(pPed), float& fMin, float& fMax) const
{
	if(m_fMinDelay > 0.0f && m_fMaxDelay > 0.0f)
	{
		fMin = m_fMinDelay;
		fMax = m_fMaxDelay;
		return true;
	}
	else
	{
		return false;
	}
}

CEvent* CEventGivePedTask::Clone() const
{
	aiTask* p=CloneTask();
	CEventGivePedTask* pGiveTaskEvent = rage_new CEventGivePedTask(m_iTaskPriority,p,m_bAcceptWhenDead,m_iEventPriority, m_bRequiresAbortForRagdoll, m_bDeleteActiveTask, m_bBlockWhenInRagdoll);
	pGiveTaskEvent->SetTunableDelay(m_fMinDelay, m_fMaxDelay);
	return pGiveTaskEvent;
}

aiTask* CEventGivePedTask::CloneTask() const
{
	aiTask* p=0;
	if(m_pTask)
	{
		p=m_pTask->Copy();
		Assert( p->GetTaskType() == m_pTask->GetTaskType() );
	}
	return p;
}

bool CEventGivePedTask::IsValidForPed(CPed* pPed) const
{
	if(0==pPed)
	{
		return CEvent::IsValidForPed(NULL);
	}
	else
	{
		return ((!pPed->IsDead() && !pPed->IsInjured()) || m_bAcceptWhenDead);
	}
}


bool CEventGivePedTask::AffectsPedCore(CPed* pPed) const
{
#if !__FINAL
	//	if( pPed->IsInjured() && !m_bAcceptWhenDead )
	//	{		
	//        Errorf( "Task (%s) given to injured ped!", m_pTask ? ((const char*) m_pTask->GetName()) : "NULL");
	//		Assertf(false, "Task (%s) given to injured ped!", m_pTask ? ((const char*) m_pTask->GetName())  : "NULL");
	//
	//	}
#endif

	// Give ped task events that don't require abort for ragdoll are not valid on ragdolling peds
	if ( m_bBlockWhenInRagdoll && pPed->GetUsingRagdoll())
		return false;

	// I think this code might be wrong, but I'm preserving the existing functionality for now.
	// I think it's supposed to be a && (b || c) instead of the way it is now, (a && b) || c.
	return ( (( !pPed->IsInjured() && !pPed->IsDead()) || m_bAcceptWhenDead) );
}

bool CEventGivePedTask::RequiresAbortForMelee(const CPed* UNUSED_PARAM(pPed)) const
{
	if (m_iEventPriority == E_PRIORITY_DAMAGE_BY_MELEE ||
		m_iEventPriority == E_PRIORITY_DAMAGE_PAIRED_MOVE )
	{
		return true;
	}
	return false;
}

bool CEventGivePedTask::TakesPriorityOver(const CEvent& otherEvent) const
{
	if(m_bAcceptWhenDead && otherEvent.GetEventType()==EVENT_DEATH)
	{
		return true;
	} 
	else if(m_bAcceptWhenDead && otherEvent.GetEventType()==EVENT_DAMAGE)
	{
		return true;
	}
	// MUST_FIX_THIS( PH - Scripted tasks specifically need to overwrite damage events due to a tricky priority problem until I can put in a full fix )
	else if( otherEvent.GetEventType()==EVENT_DAMAGE )
	{
		return true;
	}
	else 
	{
		return CEvent::TakesPriorityOver(otherEvent);
	}
}


bool CEventGivePedTask::CreateResponseTask(CPed* UNUSED_PARAM(pPed), CEventResponse& response) const
{
	response.SetEventResponse(CEventResponse::EventResponse, CloneTask());
	return response.HasEventResponse();
}

////////////////////
//IN AIR
/////////////////////

bool CEventInAir::AffectsPedCore(CPed* pPed) const 
{
	if(pPed->GetPedResetFlag( CPED_RESET_FLAG_IsJumping ) && !pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_UseReserveParachute ))
	{
		//Ped in air but already handled by a task.
		return false;
	}

	if(pPed->GetPedResetFlag( CPED_RESET_FLAG_IsRappelling ))
	{
		// We are rappelling and we probably don't want to abort that task anyway even
		// if we had one of these events, but if we wait for the task to reject us,
		// we would end up doing expensive CPedGeometryAnalyser::IsInAir() checks
		// for nothing.
		return false;
	}

	CTask* pTaskActiveSimplest=pPed->GetPedIntelligence()->GetTaskActiveSimplest();
	if(pTaskActiveSimplest &&
		(pTaskActiveSimplest->GetTaskType()==CTaskTypes::TASK_FALL_OVER))
	{
		//Ped in air but already handled by a task.
		return false;
	}

	TUNE_GROUP_BOOL(DEBUG_COVER_FALL, DO_COLLIDED_WITH_CHECK, false);
	if(pPed->GetFrameCollisionHistory()->GetNumCollidedEntities() > 0 && (DO_COLLIDED_WITH_CHECK || !pPed->GetPedResetFlag(CPED_RESET_FLAG_InCoverTaskActive)))
	{
		const CCollisionRecord *pCollisionRecord = pPed->GetFrameCollisionHistory()->GetMostSignificantCollisionRecord();
		if(!pCollisionRecord || pCollisionRecord->m_MyCollisionNormal.z > 0.1f)
		{
			//Ped has hit something so can't be in free-fall.
			return false;
		}
	}

	if(!pPed->IsCollisionEnabled())
	{
		//Ped not using collision so can't be in free-fall.
		return false;
	}

	if(pPed->GetUseExtractedZ())
	{
		//Ped is not currently affected by gravity
		return false;
	}

	if(pPed->GetIsAttached() && !pPed->GetPedResetFlag(CPED_RESET_FLAG_CanAbortExitForInAirEvent))
	{
		//Ped attached to an entity so can't be in free-fall.
		return false;
	}

	if(pPed->IsDead())
	{
		return false;
	}   

#if __DEV
	// Allows us to debug play vertical anims
	if(CAnimViewer::m_extractZ)
	{
		return false;
	}
#endif

	if(!CPedGeometryAnalyser::IsInAir(*pPed))
	{
		return false;
	}

	// Don't handle fall events if the player is a fish as this interferes with their gravity flipping logic.
	if (pPed->IsAPlayerPed() && pPed->GetPedType() == PEDTYPE_ANIMAL)
	{
		CTask* pTaskMotion = pPed->GetPedIntelligence()->GetMotionTaskActiveSimplest();

		if (pTaskMotion && pTaskMotion->GetTaskType() == CTaskTypes::TASK_ON_FOOT_FISH)
		{
			return false;
		}
	}

	return true;
}

bool CEventInAir::CanInterruptNMForParachute() const
{
	//Ensure the ped is valid.
	if(!m_pPed)
	{
		return false;
	}
	
	//Ensure the ped is in ragdoll mode.
	if(!m_pPed->GetUsingRagdoll())
	{
		return false;
	}
	
	//Ensure the ped can parachute.
	if(!CTaskParachute::CanPedParachute(*m_pPed))
	{
		return false;
	}
	
	return true;
}

bool CEventInAir::IsLandingWithParachute() const
{
	//Ensure the ped is valid.
	if(!m_pPed)
	{
		return false;
	}

	//Ensure the ped is parachuting.
	const CTaskParachute* pTask = static_cast<CTaskParachute *>(m_pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_PARACHUTE));
	if(!pTask)
	{
		return false;
	}

	//Ensure the ped is landing.
	if(!pTask->IsLanding())
	{
		return false;
	}

	//Ensure the ped is not crash landing.
	if(pTask->IsCrashLanding())
	{
		return false;
	}

	return true;
}

bool CEventInAir::CanBeInterruptedBySameEvent() const
{
	//Check if we can interrupt NM for parachute.
	if(CanInterruptNMForParachute())
	{
		return true;
	}

	//Check if we are landing with a parachute.
	if(IsLandingWithParachute())
	{
		return true;
	}

	//Call the base class version.
	return CEvent::CanBeInterruptedBySameEvent();
}

bool CEventInAir::HasExpired() const
{
	//Check if we can interrupt NM for parachute.
	if(CanInterruptNMForParachute())
	{
		return false;
	}

	//Call the base class version.
	return CEvent::HasExpired();
}

bool CEventInAir::RequiresAbortForRagdoll() const
{
	//Check if we can interrupt NM for parachute.
	if(CanInterruptNMForParachute())
	{
		return true;
	}

	//Call the base class version.
	return CEvent::RequiresAbortForRagdoll();
}

bool CEventInAir::TakesPriorityOver(const CEvent& otherEvent) const
{
	//Check if we can interrupt NM for parachute.
	if(CanInterruptNMForParachute())
	{
		return true;
	}

	//Call the base class version.
	return CEvent::TakesPriorityOver(otherEvent);
}

CEventInAir::CEventInAir(CPed* pPed)
: CEvent()
, m_pPed(pPed)
, m_bDisableSkydiveTransition(false)
, m_bCheckForDive(false)
, m_bForceNMFall(false)
, m_bForceInterrupt(false)
{
#if !__FINAL
	m_EventType = (EVENT_IN_AIR);
#endif
}

CEventInAir::~CEventInAir()
{
}

CEvent* CEventInAir::Clone() const
{
	CEventInAir* pEvent = rage_new CEventInAir(m_pPed);
	pEvent->SetDisableSkydiveTransition(m_bDisableSkydiveTransition);
	pEvent->SetCheckForDive(m_bCheckForDive);
	pEvent->SetForceNMFall(m_bForceNMFall);
	pEvent->SetForceInterrupt(m_bForceInterrupt);

	return pEvent;
}

CEventMustLeaveBoat::CEventMustLeaveBoat(CBoat * pBoat) : CEvent()
{
	m_pBoat = pBoat;
	Assert(m_pBoat);

#if !__FINAL
	m_EventType = EVENT_MUST_LEAVE_BOAT;
#endif
}
CEventMustLeaveBoat::~CEventMustLeaveBoat() 
{
}

bool CEventMustLeaveBoat::AffectsPedCore(CPed * pPed) const
{
	CBoat * pBoatToLeave = NULL;

	if(pPed->GetGroundPhysical() && pPed->GetGroundPhysical()->GetIsTypeVehicle() && ((CVehicle*)pPed->GetGroundPhysical())->GetVehicleType()==VEHICLE_TYPE_BOAT)
	{
		pBoatToLeave = (CBoat*)pPed->GetGroundPhysical();
	}
	else if(pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) && pPed->GetMyVehicle() && pPed->GetMyVehicle()->GetVehicleType()==VEHICLE_TYPE_BOAT)
	{
		pBoatToLeave = (CBoat*)pPed->GetMyVehicle();
	}

	if(!pBoatToLeave)
		return false;

	return true;
}



////////////
//ON FIRE
///////////

bool CEventOnFire::AffectsPedCore(CPed* pPed) const
{
	if(g_fireMan.IsEntityOnFire(pPed)==false)
	{
		return false;
	}

	if(pPed->m_nPhysicalFlags.bNotDamagedByFlames || pPed->m_nPhysicalFlags.bNotDamagedByAnything)
	{
		return false;	
	}

	if(pPed->GetPlayerInfo() && pPed->GetPlayerInfo()->bFireProof)
	{
		return false;
	}

	/*
	if(pPed->IsPlayer())
	{
	return false;
	}
	*/

	if (pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IgnoreBeingOnFire))
	{
		return false;
	}

	//Check if the ped is already on fire.
	CTask* pTaskActive=pPed->GetPedIntelligence()->GetTaskActive();
	if(pTaskActive)
	{
		const int iTaskType=pTaskActive->GetTaskType();
		if(CTaskTypes::TASK_COMPLEX_ON_FIRE==iTaskType)
		{
			//Ped is already on fire so don't accept the event.
			return false;
		}
	}

	if(pPed->IsDead())
	{
		return false;
	}

	return true;
}

bool CEventOnFire::TakesPriorityOver(const CEvent& otherEvent) const
{
	int otherEventPriority = otherEvent.GetEventPriority();
	if((otherEventPriority >= E_PRIORITY_CLIMB_LADDER_ON_ROUTE && otherEventPriority <= E_PRIORITY_CLIMB_NAVMESH_ON_ROUTE) || otherEventPriority == E_PRIORITY_GETUP)
	{
		return true;
	}

	return CEvent::TakesPriorityOver(otherEvent);
}

//-------------------------------------------------------------------------
// call for cover event
//-------------------------------------------------------------------------

bool CEventCallForCover::AffectsPedCore(CPed* pPed) const
{	
	if(!pPed->IsInjured())
	{
		//Grab the combat task.
		CTaskCombat* pCombatTask = static_cast<CTaskCombat *>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_COMBAT));
		if(pCombatTask) 
		{
			//Note that the ped is requesting cover.
			pCombatTask->OnCallForCover(*this);
		}
	}

	return false;
}

CEventCallForCover::CEventCallForCover( CPed* pPedCallingForCover ) : CEventEditableResponse()
{
#if !__FINAL
	m_EventType = EVENT_CALL_FOR_COVER;
#endif

	m_pPedCallingForCover = pPedCallingForCover;
}

CEventCallForCover::~CEventCallForCover()
{
	m_pPedCallingForCover = NULL;
}

//-------------------------------------------------------------------------
// providing cover event
//-------------------------------------------------------------------------

bool CEventProvidingCover::AffectsPedCore(CPed* pPed) const
{	
	if(!pPed->IsInjured())
	{
		//Grab the combat task.
		CTaskCombat* pCombatTask = static_cast<CTaskCombat *>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_COMBAT));
		if(pCombatTask) 
		{
			//Note that the ped is providing cover.
			pCombatTask->OnProvidingCover(*this);
		}
	}

	return false;
}

CEventProvidingCover::CEventProvidingCover( CPed* pPedProvidingCover ) : CEventEditableResponse()
{
#if !__FINAL
	m_EventType = EVENT_PROVIDING_COVER;
#endif

	m_pPedProvidingCover = pPedProvidingCover;
}

CEventProvidingCover::~CEventProvidingCover()
{
	m_pPedProvidingCover = NULL;
}

//-------------------------------------------------------------------------
// Request help event
//-------------------------------------------------------------------------

IMPLEMENT_GENERAL_EVENT_TUNABLES(CEventRequestHelp, 0xab020cb3);
CEventRequestHelp::Tunables CEventRequestHelp::sm_Tunables;

CEventRequestHelp::CEventRequestHelp(CPed* pPedThatNeedsHelp, CPed* pTarget)
: CEventEditableResponse()
, m_pPedThatNeedsHelp(pPedThatNeedsHelp)
, m_pTarget(pTarget)
{

}

CEventRequestHelp::~CEventRequestHelp()
{

}

bool CEventRequestHelp::AffectsPedCore(CPed* pPed) const
{
	//Ensure the ped is not injured.
	if(pPed->IsInjured())
	{
		return false;
	}
	
	//Ensure the target is valid.
	if(!m_pTarget)
	{
		return false;
	}
	
	//Ensure the target is not injured.
	if(m_pTarget->IsInjured())
	{
		return false;
	}
	
	//Ensure the ped that needs help is valid.
	if(!m_pPedThatNeedsHelp)
	{
		return false;
	}
	
	//Ensure the ped is not the one that needs help.
	if(pPed == m_pPedThatNeedsHelp)
	{
		return false;
	}
	
	//Ensure the ped is not the target.
	if(pPed == m_pTarget)
	{
		return false;
	}

	return true;
}

void CEventRequestHelp::CommunicateEvent(CPed* pPed, const CommunicateEventInput& rInput) const
{
	//Create the input.
	CommunicateEventInput input(rInput);

	//If this is a fist fight, reduce the range without radio, in order to draw less attention from nearby buddies.
	if(!IsPedArmedWithGun(m_pPedThatNeedsHelp) && !IsPedArmedWithGun(m_pTarget))
	{
		input.m_fMaxRangeWithoutRadio = sm_Tunables.m_MaxRangeWithoutRadioForFistFights;
	}

	//Communicate the event.
	CEventEditableResponse::CommunicateEvent(pPed, input);
}

bool CEventRequestHelp::IsPedArmedWithGun(const CPed* pPed) const
{
	//Ensure the ped is valid.
	if(!pPed)
	{
		return false;
	}

	//Ensure the weapon manager is valid.
	const CPedWeaponManager* pWeaponManager = pPed->GetWeaponManager();
	if(!pWeaponManager)
	{
		return false;
	}

	return pWeaponManager->GetIsArmedGun();
}


//-------------------------------------------------------------------------
// Injured ped event
//-------------------------------------------------------------------------
bool CEventInjuredCryForHelp::AffectsPedCore(CPed* pPed) const
{
	//Call the base class version.
	if(!CEventRequestHelp::AffectsPedCore(pPed))
	{
		return false;
	}
	
	//null checks
	if (!m_pPedThatNeedsHelp || !m_pTarget) {
        return false;
	}
	// Injured peds ignore this event, they've got enough to worry about
	//also ignore if they are friends with the target
	if(pPed->IsInjured() || pPed->GetPedIntelligence()->IsFriendlyWith(*m_pTarget))
	{
		return false;
	}

	//don't respond to a cry for help if you are the aggressor or the injured ped
	if (pPed == m_pPedThatNeedsHelp || pPed == m_pTarget)
	{
		return false;
	}

	//only respond to request helps from friends, unless the ped config flag is set to always respond
	if (!pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_AlwaysRespondToCriesForHelp) && !pPed->GetPedIntelligence()->IsFriendlyWith(*m_pPedThatNeedsHelp))
	{
		return false;
	}

	// don't respond to ped's killed by stealth - this is taken care of by CEventDisturbance
	if (m_pPedThatNeedsHelp->GetPedConfigFlag( CPED_CONFIG_FLAG_KilledByStealth))
	{
		return false;
	}

	//ignore if already in combat, but at least add the target as a potential threat
	const CTask * pCombatTask = pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_COMBAT);
	if(pCombatTask) 
	{
		if( pPed->GetPedIntelligence()->IsFriendlyWith(*m_pPedThatNeedsHelp) && pPed->GetPedIntelligence()->CanAttackPed(m_pTarget) )
		{
			// Already in combat, but add the target ped as a threat
			pPed->GetPedIntelligence()->RegisterThreat(m_pTarget, true );
		}
		return false;
	}

	if(m_bIgnoreIfLaw && pPed->ShouldBehaveLikeLaw())
	{
		return false;
	}

	return true;
}

CEventInjuredCryForHelp::CEventInjuredCryForHelp( CPed* pInjuredPed , CPed* pTarget)
: CEventRequestHelp(pInjuredPed, pTarget)
, m_bIgnoreIfLaw(false)
{
#if !__FINAL
	m_EventType = EVENT_INJURED_CRY_FOR_HELP;
#endif

}

CEventInjuredCryForHelp::~CEventInjuredCryForHelp()
{
}

CEventRequestHelpWithConfrontation::CEventRequestHelpWithConfrontation(CPed* pPedThatNeedsHelp, CPed* pTarget)
: CEventRequestHelp(pPedThatNeedsHelp, pTarget)
{
}

CEventRequestHelpWithConfrontation::~CEventRequestHelpWithConfrontation()
{
}

atHashWithStringNotFinal CEventRequestHelpWithConfrontation::GetAgitatedContext() const
{
	static atHashWithStringNotFinal s_hContext("Help",0x74AECFEC);
	return s_hContext;
}

bool CEventRequestHelpWithConfrontation::AffectsPedCore(CPed* pPed) const
{
	//Call the base class version.
	if(!CEventRequestHelp::AffectsPedCore(pPed))
	{
		return false;
	}

	//Ensure the ped is not agitated.
	if(pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsAgitated))
	{
		return false;
	}

	//Ensure the ped has the context.
	if(!CTaskAgitated::HasContext(*pPed, GetAgitatedContext()))
	{
		return false;
	}

	//Ensure dispatch don't response
	if(pPed->GetPedType() == PEDTYPE_MEDIC)
	{
		return false;
	}

	//Check if we are friendly with the ped that needs help.
	if(IsFriendlyWithPedThatNeedsHelp(*pPed))
	{
		return true;
	}

	//Check if we should intervene.
	if(ShouldIntervene(*pPed))
	{
		return true;
	}

	return false;
}

bool CEventRequestHelpWithConfrontation::CreateResponseTask(CPed* pPed, CEventResponse& response) const
{
	//Assert that the response task type is correct.
	aiAssertf(m_iTaskType == CTaskTypes::TASK_AGITATED, "The task type is invalid: %d.", m_iTaskType);

	//Assert that we are not agitated with ourself.
	aiAssert(pPed != m_pTarget);
	
	//Create the task.
	CTaskAgitated* pTask = rage_new CTaskAgitated(m_pTarget);

	//Set the context.
	pTask->SetContext(GetAgitatedContext());

	//Set the leader.
	pTask->SetLeader(m_pPedThatNeedsHelp);

	//Check if we are intervening.
	if(IsIntervening(*pPed))
	{
		pTask->ApplyAgitation(AT_Intervene);
	}
	
	//Set the event response.
	response.SetEventResponse(CEventResponse::EventResponse, pTask);
	
	//Run the task on the secondary tree.
	response.SetRunOnSecondary(true);
	
	return response.HasEventResponse();
}

bool CEventRequestHelpWithConfrontation::IsFriendlyWithPedThatNeedsHelp(const CPed& rPed) const
{
	return (rPed.GetPedIntelligence()->IsFriendlyWith(*m_pPedThatNeedsHelp) ||
		rPed.GetPedIntelligence()->IsFriendlyWithDueToScenarios(*m_pPedThatNeedsHelp));
}

bool CEventRequestHelpWithConfrontation::IsIntervening(const CPed& rPed) const
{
	return (!IsFriendlyWithPedThatNeedsHelp(rPed) && ShouldIntervene(rPed));
}

bool CEventRequestHelpWithConfrontation::ShouldIntervene(const CPed& rPed) const
{
	//Check if the ped that needs help is a female.
	if(!m_pPedThatNeedsHelp->IsMale())
	{
		//Check if the ped is law enforcement.
		if(rPed.IsLawEnforcementPed())
		{
			return true;
		}

		//Check if we have the HIT_WOMAN context.
		if(rPed.GetSpeechAudioEntity() && rPed.GetSpeechAudioEntity()->DoesContextExistForThisPed("HIT_WOMAN"))
		{
			return true;
		}
	}

	return false;
}

//-------------------------------------------------------------------------
// CEventCrimeCryForHelp
//-------------------------------------------------------------------------

IMPLEMENT_GENERAL_EVENT_TUNABLES(CEventCrimeCryForHelp, 0xded650cb);
CEventCrimeCryForHelp::Tunables CEventCrimeCryForHelp::sm_Tunables;

CEventCrimeCryForHelp::CEventCrimeCryForHelp( const CPed* pAssaultedPed, const CPed* pAssaultingPed) : CEventEditableResponse()
, m_pAssaultedPed(pAssaultedPed)
, m_pAssaultingPed(pAssaultingPed)
{
#if !__FINAL
	m_EventType = EVENT_CRIME_CRY_FOR_HELP;
#endif
}

CEventCrimeCryForHelp::~CEventCrimeCryForHelp()
{
	m_pAssaultedPed = NULL;
	m_pAssaultingPed = NULL;
}

bool CEventCrimeCryForHelp::AffectsPedCore(CPed* pPed) const
{
	// There is already an implicit distance check inside of the communication of the event.
	// If the event got this far, then the ped can already hear it.

	//Check for null.
	if (!m_pAssaultingPed || !m_pAssaultedPed)
	{
		return false;
	}

	// Don't respond to a cry for help if you are the aggressor.
	if (pPed == m_pAssaultingPed)
	{
		return false;
	}

	// If the assaulting ped is friendly with the assaulted ped, we should be ignoring this event.
	Assertf(!m_pAssaultingPed->GetPedIntelligence()->IsFriendlyWith(*m_pAssaultedPed), "Invalid relationship for event CRIME_CRY_FOR_HELP");

	// Injured peds and peds friendly with the assaulter ignore this event.
	if(pPed->IsInjured() || pPed->GetPedIntelligence()->IsFriendlyWith(*m_pAssaultingPed))
	{
		return false;
	}

	// Ignore this event if the ped is already in combat.
	const CTask * pCombatTask = pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_COMBAT);
	if(pCombatTask)
	{
		return false;
	}

	// Possibly ignore due to interior status.
	if (CEvent::CanEventBeIgnoredBecauseOfInteriorStatus(*pPed, m_pAssaultingPed, GetEntityOrSourcePosition()))
	{
		return false;
	}

	return true;
}

bool CEventCrimeCryForHelp::operator==(const fwEvent& rOtherEvent) const
{
	if (rOtherEvent.GetEventType() == GetEventType())
	{
		const CEventCrimeCryForHelp& otherCryEvent = static_cast<const CEventCrimeCryForHelp&>(rOtherEvent);
		return otherCryEvent.m_pAssaultedPed == m_pAssaultedPed && otherCryEvent.m_pAssaultingPed == m_pAssaultingPed;
	}

	return false;
}

bool CEventCrimeCryForHelp::GetTunableDelay(const CPed* pPed, float& fMin, float& fMax) const
{
	float fDistanceDelay = 0.0f;
	if (pPed)
	{
		const ScalarV scDistance = Dist(pPed->GetTransform().GetPosition(), GetEntityOrSourcePosition());
		fDistanceDelay = RampValue(scDistance.Getf(), sm_Tunables.m_MinDelayDistance, sm_Tunables.m_MaxDelayDistance, 0.0f, sm_Tunables.m_MaxDistanceDelay);
	}

	fMin = sm_Tunables.m_MinRandomDelay + fDistanceDelay;
	fMax = sm_Tunables.m_MaxRandomDelay + fDistanceDelay;

	return true;
}


void CEventCrimeCryForHelp::CreateAndCommunicateEvent(CPed* pPed, const CPed* pAggressorPed)
{
	// Create event
	CEventCrimeCryForHelp eCrimeEvent(pPed, pAggressorPed); 

	//Create the input.
	CommunicateEventInput input;
	input.m_bInformRespectedPedsOnly = false;
	input.m_bUseRadioIfPossible = false;
	input.m_fMaxRangeWithoutRadio = sm_Tunables.m_MaxDistance;

	// Communicate
	eCrimeEvent.CommunicateEvent(pPed, false, false);
}


//-------------------------------------------------------------------------
// Cry for help event
//-------------------------------------------------------------------------

void CEventShoutTargetPosition::OnEventAdded(CPed *pPed) const 
{
	CEvent::OnEventAdded(pPed);

	CPed *pTarget = GetTargetPed();

	if( pTarget && !pPed->GetPedIntelligence()->IsFriendlyWith(*pTarget) )
	{
		// Make sure we are friendly with the shouting ped, otherwise why would we want to help them?
		CPed *pShoutingPed = static_cast<CPed*>(GetSourceEntity());
		if(!pShoutingPed || pPed->GetPedIntelligence()->IsFriendlyWith(*pShoutingPed))
		{
			CPedIntelligence* pPedIntelligence = pPed->GetPedIntelligence();

			// Already in combat, add the ped as a threat
			pPedIntelligence->RegisterThreat(pTarget, true );

			//NOTE[egarcia]: Keep targetting alive for the CTaskCombat
			if (pPedIntelligence->GetCombatBehaviour().IsFlagSet(CCombatData::BF_PreferKnownTargetsWhenCombatClosestTarget))
			{
				CPedTargetting* pPedTargetting = pPedIntelligence->GetTargetting(true);
				if (pPedTargetting)
				{
					pPedTargetting->SetInUse();
				}
			}
		}
	}
}

bool CEventShoutTargetPosition::AffectsPedCore(CPed* pPed) const
{
	//Call the base class version.
	if(!CEventRequestHelp::AffectsPedCore(pPed))
	{
		return false;
	}
	
	if(pPed->IsDead())
	{
		return false;
	}

	CPed *pTarget = static_cast<CPed*>(GetTargetEntity());
	
	// Special checks for random law enforcement peds
	bool bHeliPed = pPed->GetMyVehicle() && pPed->GetMyVehicle()->InheritsFromHeli();
	if( pPed->PopTypeIsRandom() && pPed->IsLawEnforcementPed() &&
		((pTarget->GetPlayerWanted() && pTarget->GetPlayerWanted()->GetWantedLevel() == WANTED_CLEAN) || (bHeliPed && pTarget->GetPedConfigFlag(CPED_CONFIG_FLAG_DisableWantedHelicopterSpawning))) )
	{
		return false;
	}

	// This handles some other cases related to law peds and wanted levels
	if( !pPed->GetPedIntelligence()->CanAttackPed(pTarget) )
	{
		return false;
	}

	// Melee peds can get caught in a loop of quitting melee victoriously and then responding to a shout position
	// This logic prevents that from happening for all random ambient peds.
	if( pPed->PopTypeIsRandom() )
	{
		CCombatMeleeGroup* pGroup = CCombatManager::GetMeleeCombatManager()->FindMeleeGroup(*pTarget);
		if( pGroup )
		{
			CPed* pThatNeedsHelp = GetPedThatNeedsHelp();
			if( pGroup->IsPedInMeleeGroup( pThatNeedsHelp ) && !pGroup->HasActiveCombatant() )
				return false;
		}

		// Random emergency type peds should react to a shout target position event by fleeing IF they are already in combat with the
		// shouted target AND the shouting ped is a law enforcement ped
		if( CPedType::IsEmergencyType(pPed->GetPedType()) &&
			pTarget == pPed->GetPedIntelligence()->GetQueriableInterface()->GetHostileTarget() )
		{
			CEntity* pShoutingEntity = GetSourceEntity();
			if(pShoutingEntity && pShoutingEntity->GetIsTypePed() && static_cast<CPed*>(pShoutingEntity)->IsLawEnforcementPed())
			{
				CTaskThreatResponse* pThreatResponseTask = static_cast<CTaskThreatResponse*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_THREAT_RESPONSE));
				if(pThreatResponseTask && pThreatResponseTask->GetState() == CTaskThreatResponse::State_Combat)
				{
					pThreatResponseTask->SetFleeFromCombat(true);
					return false;
				}
			}
		}
	}

	//Register this as a hostile event with CTaskAgitated and ignore if possible.
	if(CTaskAgitated::ShouldIgnoreEvent(*pPed, *this))
	{
		return false;
	}

	//This was added to stop cops from all heading towards the first incident that gets started, in MP.
	if(pTarget && CWantedIncident::ShouldIgnoreCombatEvent(*pPed, *pTarget))
	{
		return false;
	}

	return true;
}

bool CEventShoutTargetPosition::ShouldCreateResponseTaskForPed(const CPed* pPed) const
{
	const CEntity* pTargetEntity = GetTargetEntity();

	if(!pTargetEntity || !pTargetEntity->GetIsTypePed())
	{
		return false;
	}

	// Don't attack friendlies
	if(pPed->GetPedIntelligence()->IsFriendlyWith(*(CPed*)pTargetEntity))
	{
		return false;
	}

	// Don't attack peds we aren't allowed to attack
	if( CTaskTypes::IsCombatTask(GetResponseTaskType()) && 
		pPed && !pPed->GetPedIntelligence()->CanAttackPed(static_cast<const CPed*>(pTargetEntity)) )
	{
		return false;
	}

	return true;
}

/////////////////////////////////////////////////////////////
// CEventHelpAmbientFriend
/////////////////////////////////////////////////////////////

CEventHelpAmbientFriend::CEventHelpAmbientFriend(const CPed* pFriend, const CPed* pAssaultingPed, eHelpAmbientReaction iResponseLevel)
	: CEvent()
	, m_pFriend(pFriend)
	, m_pAssaultingPed(pAssaultingPed)
	, m_iResponseLevel(iResponseLevel)
{
#if !__FINAL
	m_EventType = EVENT_HELP_AMBIENT_FRIEND;
#endif
}

bool CEventHelpAmbientFriend::AffectsPedCore(CPed* pPed) const
{
	// Sanity check ped.
	if (!pPed)
	{
		return false;
	}

	// Sanity check stored friend pointer.
	if (!m_pFriend)
	{
		return false;
	}

	// Sanity check that the stored friend pointer really is the friend of the sensing ped.
	if (m_pFriend != pPed->GetPedIntelligence()->GetAmbientFriend())
	{
		return false;
	}

	// Make sure we aren't dying.
	if (pPed->GetIsDeadOrDying())
	{
		return false;
	}

	return true;
}

bool CEventHelpAmbientFriend::CreateResponseTask(CPed* pPed, CEventResponse& response) const
{
	//Set the event response based on the stored type.
	CTask* pTask = NULL;
	if (pPed->GetTaskData().GetIsFlagSet(CTaskFlags::DependentAmbientFriend))
	{
		if (m_iResponseLevel == HAR_FOLLOW)
		{
			// Follow your owner.
			CTaskMoveBeInFormation* pFormationTask = rage_new CTaskMoveBeInFormation(const_cast<CPed*>(m_pFriend.Get()));
			pFormationTask->SetNonGroupSpacing(2.5f);
			pFormationTask->SetNonGroupWalkAlongsideLeader(false);
			pTask = rage_new CTaskComplexControlMovement(pFormationTask);
		}
		else if (m_iResponseLevel == HAR_FLEE)
		{
			// Run away.
			if (m_pAssaultingPed)
			{
				// From the attacker.
				pTask = rage_new CTaskSmartFlee(CAITarget(m_pAssaultingPed));
			}
			else
			{
				// From the corpse of our owner.
				pTask = rage_new CTaskSmartFlee(CAITarget(m_pFriend));
			}
		}
		else if (m_iResponseLevel == HAR_THREAT && m_pAssaultingPed)
		{
			// Attack.
			CTaskThreatResponse* pTaskThreat = rage_new CTaskThreatResponse(m_pAssaultingPed);
			pTaskThreat->SetThreatResponseOverride(CTaskThreatResponse::TR_Fight);
			pTask = pTaskThreat;
		}
	}
	else
	{
		// Go into threat response if the assaulter exists.
		if (m_pAssaultingPed)
		{
			pTask = rage_new CTaskThreatResponse(m_pAssaultingPed);
		}
		// Else if your friend is dead then flee its corpse.
		else if (m_pFriend && m_pFriend->GetIsDeadOrDying())
		{
			CAITarget fleeTarget(VEC3V_TO_VECTOR3(m_pFriend->GetTransform().GetPosition()));
			pTask = rage_new CTaskSmartFlee(fleeTarget);
		}
	}

	response.SetEventResponse(CEventResponse::EventResponse, pTask);

	return (response.GetEventResponse(CEventResponse::EventResponse) != NULL);
}

bool CEventHelpAmbientFriend::TakesPriorityOver(const CEvent& otherEvent) const
{
	// If your friend is not dependent (meaning you, the owner of the event are), then this event takes priority over damage.
	if (m_pFriend && !m_pFriend->GetTaskData().GetIsFlagSet(CTaskFlags::DependentAmbientFriend))
	{
		if (otherEvent.GetEventType() == EVENT_DAMAGE)
		{
			return true;
		}
	}

	return CEvent::TakesPriorityOver(otherEvent);
}

bool CEventHelpAmbientFriend::operator==(const fwEvent& rOtherEvent) const
{
	if(static_cast<const CEvent&>(rOtherEvent).GetEventType() == GetEventType())
	{
		const CEventHelpAmbientFriend& rOtherFriendEvent = static_cast<const CEventHelpAmbientFriend&>(rOtherEvent);
		if (rOtherFriendEvent.GetSourceEntity() == GetSourceEntity() && rOtherFriendEvent.GetOtherEntity() == GetOtherEntity())
		{
			return true;
		}
	}
	return false;
}

CEventHelpAmbientFriend::eHelpAmbientReaction CEventHelpAmbientFriend::DetermineNewHelpResponse(CPed& responderPed, CPed& friendPed, CEvent* pFriendEvent)
{
	if (responderPed.GetTaskData().GetIsFlagSet(CTaskFlags::DependentAmbientFriend))
	{
		// A NULL event means that the owner ped started to wander away, so follow.
		if (!pFriendEvent)
		{
			return HAR_FOLLOW;
		}

		if (friendPed.GetPedIntelligence()->GetTaskCombat())
		{
			// If our owner is attacking the assaulting ped - then attack.
			return HAR_THREAT;
		}
		else if (pFriendEvent->GetEventType() == EVENT_DAMAGE)
		{
			CEventDamage& eventDamage = static_cast<CEventDamage&>(*pFriendEvent);
			if (eventDamage.GetIsMeleeDamage())
			{
				// Owner damaged by melee.  Fight if you can.
				if (CTaskThreatResponse::DetermineThreatResponseState(responderPed, eventDamage.GetSourcePed(), CTaskThreatResponse::TR_None, CTaskThreatResponse::CF_PreferFight, 0) == CTaskThreatResponse::TR_Fight)
				{
					return HAR_THREAT;
				}
			}
			// Otherwise flee.
			return HAR_FLEE;
		}
		else
		{
			// Otherwise, follow.
			return HAR_FOLLOW;
		}
	}
	else
	{
		// Either run away or help in the fight against your dog.
		return HAR_THREAT;
	}
}

///////////////////////////////////////////////////////
// CEventShoutTargetPosition
///////////////////////////////////////////////////////

CEventShoutTargetPosition::CEventShoutTargetPosition( CPed* pPedShouting, CPed* pTarget )
: CEventRequestHelp(pPedShouting, pTarget)
{
#if !__FINAL
	m_EventType = EVENT_SHOUT_TARGET_POSITION;
#endif
}

CEventShoutTargetPosition::~CEventShoutTargetPosition()
{
}

#define MAX_RADIO_DISTANCE (200.0f)

// Radio for help
bool CEventRadioTargetPosition::AffectsPedCore(CPed* UNUSED_PARAM(pPed)) const
{	
	return false;
}

bool CEventRadioTargetPosition::CreateResponseTask(CPed* UNUSED_PARAM(pPed), CEventResponse& response) const
{	
	const CEntity* pEntity = m_pTarget.GetEntity();
	if (pEntity && pEntity->GetIsTypePed())
	{
		response.SetEventResponse(CEventResponse::EventResponse, rage_new CTaskThreatResponse(static_cast<const CPed*>(pEntity)));
	}

	return response.HasEventResponse();
}

CEventRadioTargetPosition::CEventRadioTargetPosition( CPed* pPedRadioing, const CAITarget& pTarget )
: 
m_pPedRadioing(pPedRadioing),
m_pTarget(pTarget)
{
#if !__FINAL
	m_EventType = EVENT_RADIO_TARGET_POSITION;
#endif // !__FINAL
}

CEventRadioTargetPosition::~CEventRadioTargetPosition()
{
	m_pPedRadioing = NULL;
	m_pTarget.Clear();
}

///////////////////
//SHOUT BLOCKING LOS
///////////////////

CEventShoutBlockingLos::CEventShoutBlockingLos( CPed* pPedShouting, CEntity* pTarget ) : CEvent()
{
#if !__FINAL
	m_EventType = EVENT_SHOUT_BLOCKING_LOS;
#endif

	m_pPedShouting = pPedShouting;
	m_pTarget = pTarget;
}

CEventShoutBlockingLos::~CEventShoutBlockingLos()
{
}

bool CEventShoutBlockingLos::AffectsPedCore(CPed* pPed) const
{
	//Ensure the ped is not injured.
	if(pPed->IsInjured())
	{
		return false;
	}
	
	//Check if the combat task is running.
	CTaskCombat* pTask = static_cast<CTaskCombat *>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_COMBAT));
	if(pTask) 
	{
		//Notify the task.
		pTask->OnShoutBlockingLos(*this);
	}
	
	return false;
}

///////////////////
//VEHICLE ON FIRE
///////////////////

CEventVehicleOnFire::CEventVehicleOnFire(CVehicle* pVehicleOnFire)
: CEventEditableResponse(),
m_pVehicleOnFire(pVehicleOnFire)
{
#if !__FINAL
	m_EventType = EVENT_VEHICLE_ON_FIRE;
#endif
}

CEventVehicleOnFire::~CEventVehicleOnFire()
{
}

bool CEventVehicleOnFire::AffectsPedCore(CPed* pPed) const
{
	if(0==m_pVehicleOnFire)
	{
		//Vehicle has been deleted.
		return false;
	}

	if(g_fireMan.IsEntityOnFire(m_pVehicleOnFire)==false)
	{
		//Fire has gone out.
	}

	const CVehicle* pMyVehicle = pPed->GetMyVehicle();
	if(pMyVehicle && pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) && pMyVehicle!=m_pVehicleOnFire)
	{
		//Ped is in another vehicle. Don't respond to the vehicle that is on fire.  Just wait
		//for the fire to spread and then the ped's vehicle will catch fire as well.
		return false;
	}

	if(pPed->IsPlayer())
	{
		return false;
	}

	if(pPed->IsDead())
	{
		return false;
	}

	//Peds that are group followers and are in a vehicle don't accept vehicle fire events.  Don't want 
	//group followers leaving the vehicle before the leader leaves.
	CPedGroup* pPedGroup=pPed->GetPedsGroup();
	if(pPedGroup && !pPedGroup->GetGroupMembership()->IsLeader(pPed) && pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ))
	{
		return false;
	}

	// we've already determined this is this ped's vehicle, so doesn't matter how big it is
	if(!pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ))
	{
		if(VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition() - m_pVehicleOnFire->GetTransform().GetPosition()).Mag2() > rage::square(m_pVehicleOnFire->GetBoundRadius() + 5.0f))
			return false;
	}
	
	//Some peds do not care about fire.
	if(!pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_RunFromFiresAndExplosions))
	{
		return false;
	}

	// HACK - peds in tanks don't care if their vehicle is on fire.
	if (pMyVehicle && pMyVehicle->IsTank())
	{
		return false;
	}

	return true;
}

bool CEventVehicleOnFire::CreateResponseTask(CPed* pPed, CEventResponse& response) const
{
	CVehicle* pVehicleOnFire=GetVehicleOnFire();
	if(0==pVehicleOnFire)
	{
		return false;
	}

	const int iTaskType=GetResponseTaskType();		
	switch(iTaskType)
	{
	case CTaskTypes::TASK_EXIT_VEHICLE:
	case CTaskTypes::TASK_COMPLEX_LEAVE_CAR_AND_FLEE:
	case CTaskTypes::TASK_SMART_FLEE:
		{
			//Check if abnormal exits are disabled.
			const CVehicleSeatAnimInfo* pVehicleSeatAnimInfo = CVehicleMetadataMgr::GetSeatAnimInfoFromPed(pPed);
			if(pVehicleSeatAnimInfo && pVehicleSeatAnimInfo->GetDisableAbnormalExits())
			{
				// Vehicle can't be exited, cower inside.
				response.SetEventResponse(CEventResponse::EventResponse, rage_new CTaskSmartFlee(CAITarget(pVehicleOnFire)));
			}
			else if(pVehicleOnFire->GetHealth() == 0.0f && !pPed->PopTypeIsMission())
			{
				Vector3 vLocalOffset(0.0f, 0.0f, 0.0f);
				vLocalOffset.z = fwRandom::GetRandomTrueFalse() ? 1.0f : -1.0f;

				// Vehicle is destroyed, perform get out on fire task
				response.SetEventResponse(CEventResponse::EventResponse, rage_new CTaskCarReactToVehicleCollisionGetOut(pVehicleOnFire, NULL, vLocalOffset, 0.0f, CTaskCarReactToVehicleCollisionGetOut::FLAG_ON_FIRE));
			}
			else
			{
				//Check if we should not flee forever.
				bool bDoNotFleeForever = (pPed->IsLawEnforcementPed() || !pPed->PopTypeIsRandom());

				//Create the task.
				float fStopDistance = bDoNotFleeForever ? (pVehicleOnFire->GetBoundRadius() + 5.0f) : -1.0f;
				CTaskSmartFlee* pTask = rage_new CTaskSmartFlee(CAITarget(pVehicleOnFire), fStopDistance);
				pTask->SetQuitIfOutOfRange(bDoNotFleeForever);
				pTask->SetExecConditions(CTaskSmartFlee::EC_DontUseCars | CTaskSmartFlee::EC_DisableCower);

				//Set the event response.
				response.SetEventResponse(CEventResponse::EventResponse, pTask);
			}

			if(fwRandom::GetRandomTrueFalse()) // 50% chance of exclaiming
			{
				pPed->NewSay("EXPLOSION_IS_IMMINENT");
			}
		}
		break;		
// CS - Disabled due to TASK_COMPLEX_CAR_DRIVE_MISSION_FLEE_SCENE no longer existing
// 	case CTaskTypes::TASK_COMPLEX_CAR_DRIVE_MISSION_FLEE_SCENE:
// 		{
// 			CTaskSequenceList* pSequence=rage_new CTaskSequenceList;
// 			pSequence->AddTask(rage_new CTaskLeaveAnyCar(0,false,true,false));
// 			pSequence->AddTask(rage_new CTaskEscapeBlast( pVehicleOnFire, pVehicleOnFire->GetPosition(), pVehicleOnFire->GetBlastRadius()+1.0f, true ));
// 			response.SetEventResponse(CEventResponse::EventResponse, pSequence);
// 		}
// 		break;		
	default:
		return CEventEditableResponse::CreateResponseTask(pPed, response);
	}

	return response.HasEventResponse();
}

//
//	For now this event is basically just a notifier.
//
CEventInWater::CEventInWater(float fBuoyancyFraction)
: CEvent(),
m_fBuoyancyFraction(fBuoyancyFraction)
{
#if !__FINAL
	m_EventType = EVENT_IN_WATER;
#endif
}

CEventInWater::~CEventInWater()
{
}

bool CEventInWater::AffectsPedCore(CPed* pPed) const
{
	if(pPed->IsDead())
	{
		return false;
	}

	// If ProcessProbes() for this ped has returned that we are standing then don't accept the
	// in-water event, otherwise our state will flip-flop between in water & not in water, as
	// the water task ends when the ped is standing.
	if(pPed->GetIsStanding())
	{
		return false;
	}

	if( pPed->IsPlayer() )
	{
		/*CTask* pSimpleMovementTask = pPed->GetPedIntelligence()->GetGeneralMovementTask();
		if( pSimpleMovementTask && pSimpleMovementTask->GetTaskType()==CTaskTypes::TASK_MOVE_PLAYER)
		{
		((CTaskSimpleMovePlayer*)pSimpleMovementTask)->SetIsStrafing(false);
		}*/

		/*CTask* pComplexMovementTask = pPed->GetPedIntelligence()->GetTaskManager()->FindTaskByTypeActive(CTaskTypes::TASK_COMPLEX_PLAYER_ON_FOOT);
		if( pComplexMovementTask)
		{
		pComplexMovementTask->MakeAbortable(CTask::ABORT_PRIORITY_IMMEDIATE,0);
		}*/

		/*CTask* pGunTask = pPed->GetPedIntelligence()->GetTaskManager()->FindTaskByTypeActive(CTaskTypes::TASK_COMPLEX_PLAYER_GUN);
		if(pGunTask)
		{
		pGunTask->MakeAbortable(CTask::ABORT_PRIORITY_IMMEDIATE,0);
		}*/
	}

	return true;
}

bool CEventInWater::TakesPriorityOver(const CEvent& otherEvent) const
{
	if(otherEvent.GetEventType()==EVENT_DAMAGE)
	{
		return true;
	}
	else if(otherEvent.GetEventType()==EVENT_STUCK_IN_AIR
		&& m_fBuoyancyFraction > 1.0f)
	{
		return true;
	}
	else 
	{
		return CEvent::TakesPriorityOver(otherEvent);
	}
}

bool CEventInWater::CreateResponseTask(CPed* pPed, CEventResponse& UNUSED_PARAM(response)) const
{
	pPed->SetIsInWater(						true );
	pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_IsStanding, false );

	/*bool bIsGettingIntoVehicle = false;
	if(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_COMPLEX_NEW_GET_IN_VEHICLE))
	{
		bIsGettingIntoVehicle = true;
	}

	CTaskComplexInWater* pWaterTask = rage_new CTaskComplexInWater(NULL, NULL, bIsGettingIntoVehicle);

	if(!pPed->IsPlayer())
	{
		// Now to check if ped has any underlying tasks that are telling him to go somewhere or follow someone...

		if(pPed->GetMoveState() != PEDMOVE_STILL)
		{
			// If there is a movement task active which has a target, then obtain the target & set in the swim task
			CTask* pTaskMove = pPed->GetPedIntelligence()->GetActiveSimplestMovementTask();
			Assert(pTaskMove);
			if(pTaskMove)
			{
				CTaskMoveInterface* pMoveInt = pTaskMove->GetMoveInterface();
				if(pMoveInt->HasTarget())
				{
					Vector3 vTarget = pMoveInt->GetTarget();
					pWaterTask->SetTarget(&vTarget);
				}
			}
		}

	}

	response.SetEventResponse(CEventResponse::EventResponse, pWaterTask);
	return response.GetEventResponse(CEventResponse::EventResponse) != NULL;*/
	
	return true;
}

CEventGetOutOfWater::CEventGetOutOfWater()
: CEvent()
{
#if !__FINAL
	m_EventType = EVENT_GET_OUT_OF_WATER;
#endif
}

CEventGetOutOfWater::~CEventGetOutOfWater()
{
}

bool CEventGetOutOfWater::AffectsPedCore(CPed* pPed) const
{
	if(pPed->IsDead())
	{
		return false;
	}

	if(pPed->GetPedResetFlag(CPED_RESET_FLAG_IsEnteringVehicle))
	{
		return false;
	}

	if (!pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_DrownsInWater))
	{
		return false;
	}

	return true;
}

bool CEventGetOutOfWater::CreateResponseTask(CPed* UNUSED_PARAM(pPed), CEventResponse& response) const
{
	static const float fSearchDist = 40.0f;
	CTaskGetOutOfWater* getOutOfWaterTask = rage_new CTaskGetOutOfWater(fSearchDist, MOVEBLENDRATIO_RUN);

	response.SetEventResponse(CEventResponse::EventResponse, getOutOfWaterTask);
	return response.GetEventResponse(CEventResponse::EventResponse) != NULL;
}

//
//	For now this event is basically just a notifier.
//
CEventStuckInAir::CEventStuckInAir(CPed *pPed)
: CEvent(),
m_pPed(pPed)
{
#if !__FINAL
	m_EventType = EVENT_STUCK_IN_AIR;
#endif
}

CEventStuckInAir::~CEventStuckInAir()
{
}

bool CEventStuckInAir::AffectsPedCore(CPed* pPed) const
{
	if(pPed->GetIsStanding())
		return false;

	if(pPed->GetPedIntelligence()->GetEventScanner()->GetStuckChecker().IsPedStuck())
		return true;

	return false;
}

bool CEventStuckInAir::TakesPriorityOver(const CEvent& otherEvent) const
{
	if(otherEvent.GetEventPriority() < E_PRIORITY_REVIVED
		&& otherEvent.GetEventType() != EVENT_STUCK_IN_AIR)
		return true;

	return CEvent::TakesPriorityOver(otherEvent);
}

int CEventStuckInAir::GetEventPriority() const
{
	// if the stuck in air event hasn't already been processed
	//then make sure it overrides any other events in the queue
	if(m_pPed && m_pPed->GetPedIntelligence()->GetCurrentEventType()!=EVENT_STUCK_IN_AIR
		&& m_pPed->GetPedIntelligence()->GetNumEventsInQueue() > 1)
	{
		return E_PRIORITY_MAX;
	}

	return E_PRIORITY_STUCK_IN_AIR;
}

///////////////////////////
//MUST LEAVE BOAT
///////////////////////////

bool CEventMustLeaveBoat::IsValidForPed(CPed* pPed) const
{
	if(0==pPed)
	{
		return CEvent::IsValidForPed(NULL);
	}
	else
	{
		return (!pPed->IsDead());
	}
}

/////////////////////////
//COP CAR BEING STOLEN
/////////////////////////

CEventCopCarBeingStolen::CEventCopCarBeingStolen(CPed* pCriminal, CVehicle* pTargetVehicle)
: m_pCriminal(pCriminal),
m_pTargetVehicle(pTargetVehicle)
{
#if !__FINAL
	m_EventType = EVENT_COP_CAR_BEING_STOLEN;
#endif
}

CEventCopCarBeingStolen::~CEventCopCarBeingStolen()
{
}

bool CEventCopCarBeingStolen::AffectsPedCore(CPed* pPed) const
{
	//Don't accept event if criminal or stolen vehicle have
	//been deleted.
	if(0==m_pCriminal || 0==m_pTargetVehicle)
	{
		return false;
	}

	//Don't accept event if criminal or ped are dead.
	if(m_pCriminal->IsDead() || pPed->IsDead())
	{
		return false;
	}

	//Don't accept if it's a player ped.
	if(pPed->IsAPlayerPed())
	{
		return false;
	}

	//Only cop peds respond to the event.
	if(pPed->GetPedType()!=PEDTYPE_COP)
	{
		return false;
	}	

	return (m_pTargetVehicle==pPed->GetMyVehicle() && pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ));
}

bool CEventCopCarBeingStolen::CreateResponseTask(CPed* pPed, CEventResponse& response) const
{
	CPed* pCriminal=GetCriminal();
	CVehicle* pVehicle=GetTargetVehicle();
	if(pCriminal && pVehicle && 
		pPed->GetMyVehicle()==pVehicle &&
		pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) &&
		pPed != pCriminal)
	{
		VehicleEnterExitFlags vehicleFlags;
		response.SetEventResponse(CEventResponse::EventResponse, rage_new CTaskExitVehicle(pVehicle,vehicleFlags));

		if(pCriminal->IsPlayer())
		{
			pCriminal->GetPlayerWanted()->SetWantedLevelNoDrop(VEC3V_TO_VECTOR3(pCriminal->GetTransform().GetPosition()), WANTED_LEVEL1, WANTED_CHANGE_DELAY_QUICK, WL_FROM_LAW_RESPONSE_EVENT);
		}
	}

	return response.HasEventResponse();
}

////////////////////
//VEHICLE DAMAGE
////////////////////

CEventVehicleDamage::CEventVehicleDamage(CVehicle* pVehicle, CEntity* pInflictor, const u32 nWeaponUsedHash, const float fDamageCaused, const Vector3& vDamagePos, const Vector3& vDamageDir)
: CEventEditableResponse(),
m_pVehicle(pVehicle),
m_pInflictor(pInflictor),
m_nWeaponUsedHash(nWeaponUsedHash),
m_fDamageCaused(fDamageCaused),
m_vDamagePos(vDamagePos),
m_vDamageDir(vDamageDir)
{

}

CEventVehicleDamage::~CEventVehicleDamage()
{
}

bool CEventVehicleDamage::AffectsPedCore(CPed* pPed) const
{
	if(pPed->IsDead())
	{
		return false;
	}

	//Ensure the event is not disabled.
	if(pPed->GetPedResetFlag(CPED_RESET_FLAG_DisableVehicleDamageReactions))
	{
		return false;
	}

	if (!m_pInflictor)
	{
		return false;
	}

	const CPed* pInflictorPed = NULL;
	if(m_pInflictor->GetIsTypePed())
	{
		pInflictorPed = static_cast<CPed *>(m_pInflictor.Get());

		//Ensure the inflictor has not disabled vehicle damage reactions.
		if(pInflictorPed->GetPedResetFlag(CPED_RESET_FLAG_DisableVehicleDamageReactions))
		{
			return false;
		}

		//Friendly reactions are now disabled via the decision maker.
		//This was required to support Family/Franklin reactions for this event.
// 		if( pPed->GetPedIntelligence()->IsFriendlyWith(*pInflictorPed) )
// 		{
// 			return false;
// 		}
	}
	else if(m_pInflictor->GetIsTypeVehicle())
	{
		const CPed* pDriver = static_cast<CVehicle *>(m_pInflictor.Get())->GetDriver();
		if(pDriver)
		{
			//Ensure the driver has not disabled vehicle damage reactions.
			if(pDriver->GetPedResetFlag(CPED_RESET_FLAG_DisableVehicleDamageReactions))
			{
				return false;
			}
		}
	}

	if(pPed->GetPedIntelligence()->HasDuplicateEvent(this))
	{
		return false;
	}

	/*
	if(m_pInflictor->GetIsTypeVehicle())
	{
	return false;
	}
	*/
	
	//Ensure the vehicle is valid.
	if(!m_pVehicle)
	{
		return false;
	}
	
	//Ensure the vehicle matches.
	if(pPed->GetMyVehicle() != m_pVehicle)
	{
		return false;
	}
	
	//Ensure we should not ignore the hostile event.
	if(CTaskAgitated::ShouldIgnoreEvent(*pPed, *this))
	{
		return false;
	}

	//Check if the ped is fleeing.
	CTaskSmartFlee* pTaskSmartFlee = static_cast<CTaskSmartFlee *>(
		pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_SMART_FLEE));
	if(pTaskSmartFlee)
	{
		//Set the priority event.
		pTaskSmartFlee->SetPriorityEvent(EVENT_VEHICLE_DAMAGE_WEAPON);

		return false;
	}

	//Ensure the ped is not already running threat response.
	if(pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_THREAT_RESPONSE))
	{
		return false;
	}

	return true;
}

bool CEventVehicleDamage::operator==( const fwEvent& otherEvent ) const
{
	if( static_cast<const CEvent&>(otherEvent).GetEventType() == GetEventType() )
	{
		const CEventVehicleDamage& otherDamageEvent = static_cast<const CEventVehicleDamage&>(otherEvent);
		if( otherDamageEvent.GetInflictor() == GetInflictor() && 
			otherDamageEvent.GetWeaponUsed() == GetWeaponUsed() && 
			otherDamageEvent.GetDamageCaused() < GetDamageCaused() )
		{
			return true;
		}
	}
	return false;
}

CEntity* CEventVehicleDamage::GetSourceEntity() const
{
	if(m_pInflictor && m_pInflictor->GetType()==ENTITY_TYPE_VEHICLE && ((CVehicle*)m_pInflictor.Get())->GetSeatManager()->GetDriver())
	{
		return ((CVehicle*)m_pInflictor.Get())->GetSeatManager()->GetDriver();
	}
	else
	{
		return m_pInflictor;
	}
}

CPed* CEventVehicleDamage::GetPlayerCriminal() const 
{	
	CPed* pPedInflictor = NULL;

	if(m_pInflictor)
	{
		if( m_pInflictor->GetIsTypePed() )
		{
			pPedInflictor = (CPed*)m_pInflictor.Get();
		}
		else if( m_pInflictor->GetIsTypeVehicle() )
		{
			pPedInflictor = ((CVehicle*)m_pInflictor.Get())->GetSeatManager()->GetDriver();
		}

		if( pPedInflictor && pPedInflictor->IsPlayer() )
		{
			return pPedInflictor;
		}
	}
	return NULL;
}

//////////////////
//CAR UPSIDE DOWN
//////////////////
dev_u16 CEventCarUndriveable::ms_uMinStuckTimeMS = 5000;

CEventCarUndriveable::CEventCarUndriveable(CVehicle* pVehicle)
: m_pVehicle(pVehicle)
, m_bIsTemporaryEvent(false)
{
#if !__FINAL
	m_EventType = EVENT_CAR_UNDRIVEABLE;
#endif
}

CEventCarUndriveable::~CEventCarUndriveable()
{
}


bool CEventCarUndriveable::AffectsPedCore(CPed* pPed) const
{
	if(pPed->IsPlayer())
		return false;

	if(!pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_GetOutUndriveableVehicle ))
	{
		return false;
	}

	if(!pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ))
	{
		return false;
	}

	if(pPed->PopTypeIsMission() && (!pPed->GetMyVehicle() || (pPed->GetMyVehicle()->InheritsFromBoat() && pPed->GetMyVehicle()->GetIsInWater())))
		return false;

	if(pPed->IsDead())
		return false;

	//Ensure the vehicle is valid.
	if(!m_pVehicle)
	{
		return false;
	}

	//Check if we are already fleeing.
	CTaskSmartFlee* pTask = static_cast<CTaskSmartFlee *>(
		pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_SMART_FLEE));
	if(pTask)
	{
		pTask->OnCarUndriveable(*this);
		return false;
	}

	// Dont accept if this ped is in a player's group and the boat is beached
	// this prevents the ped from getting out until the player does
	if (pPed->GetPedsGroup())
	{
		CPed* pLeader = pPed->GetPedsGroup()->GetGroupMembership()->GetLeader();
		if (pLeader && pLeader->IsAPlayerPed())
		{
			if (CBoatIsBeachedCheck::IsBoatBeached(*m_pVehicle))
			{
				return false;
			}
		}
	}

	//HACK
	const_cast<CEventCarUndriveable *>(this)->m_bIsTemporaryEvent = (pPed->IsLawEnforcementPed());

	return true;
}

bool CEventCarUndriveable::CreateResponseTask(CPed* pPed, CEventResponse& response) const
{
	if(pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) && pPed->GetMyVehicle() && pPed->GetMyVehicle() == GetVehicle())
	{
		// If we should use the flee response
		bool bUseFleeResponse = false;
		
		//Check if abnormal exits are disabled or if the car is upside down.
		const CVehicleSeatAnimInfo* pVehicleSeatAnimInfo = CVehicleMetadataMgr::GetSeatAnimInfoFromPed(pPed);
		if( (pVehicleSeatAnimInfo && pVehicleSeatAnimInfo->GetDisableAbnormalExits()) || (!pPed->PopTypeIsMission() && CUpsideDownCarCheck::IsCarUpsideDown(GetVehicle())) )
		{
			bUseFleeResponse = true;
		}
		
		// Check if ped should flee due to being otherwise stuck
		if( !bUseFleeResponse && ShouldPedUseFleeResponseDueToStuck(pPed, GetVehicle()) )
		{
			bUseFleeResponse = true;
		}
		
		if( bUseFleeResponse )
		{
			//Cower in the vehicle or flee the vehicle as appropriate.
			response.SetEventResponse(CEventResponse::EventResponse, rage_new CTaskSmartFlee(CAITarget(m_pVehicle)));
			return response.HasEventResponse();
		}

		// Create the get out task, we'll need this whatever
		int iRandomDelay = fwRandom::GetRandomNumberInRange(100, 400);
		VehicleEnterExitFlags vehicleFlags;
		vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::DelayForTime);
		vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::DontCloseDoor);

		// Jump out of helis/planes that are in the air, so that the ped properly gets CEventInAir.
		if(m_pVehicle 
			&& ( ( (m_pVehicle->InheritsFromHeli() || m_pVehicle->InheritsFromPlane() ) && m_pVehicle->IsInAir()  )
				|| ( (m_pVehicle->InheritsFromBoat() ) && m_pVehicle->GetIsBeingTowed() ) ) )
		{
			vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::JumpOut);
		}
		CTaskExitVehicle* pExitCarTask = rage_new CTaskExitVehicle(pPed->GetMyVehicle(), vehicleFlags, (float)iRandomDelay/1000.0f);

		// If not a cop, get out and either attack the ped that caused their death, or run away
		if (!pPed->IsLawEnforcementPed())
		{
			CTaskSequenceList* pSequence = rage_new CTaskSequenceList;
			pSequence->AddTask(pExitCarTask);
			pSequence->AddTask(rage_new CTaskSmartFlee(CAITarget(m_pVehicle)));
			response.SetEventResponse(CEventResponse::EventResponse, rage_new CTaskUseSequence(*pSequence));
		}
		else // ped is a cop
		{
			response.SetEventResponse(CEventResponse::EventResponse, pExitCarTask);
		}

		return response.GetEventResponse(CEventResponse::EventResponse) != NULL;
	}

	return CEvent::CreateResponseTask(pPed, response);
}

// static
bool CEventCarUndriveable::ShouldPedUseFleeResponseDueToStuck(const CPed* pPed, const CVehicle* pVehicle)
{
	if( AssertVerify(pPed) && AssertVerify(pVehicle) )
	{
		//Check if the ped qualifies for fleeing due to stuck
		if( pPed->PopTypeIsRandom() && !pPed->IsLawEnforcementPed() )
		{
			// Check if the vehicle qualifies for being stuck
			if( pVehicle->GetVehicleType() == VEHICLE_TYPE_CAR || pVehicle->GetVehicleType() == VEHICLE_TYPE_BIKE || pVehicle->GetVehicleType() == VEHICLE_TYPE_BICYCLE || pVehicle->GetVehicleType() == VEHICLE_TYPE_QUADBIKE || pVehicle->GetVehicleType() == VEHICLE_TYPE_AMPHIBIOUS_QUADBIKE)
			{
				//Check if the vehicle is physically stuck
				const CVehicleDamage* pVehicleDamage = pVehicle->GetVehicleDamage();
				if( pVehicleDamage && (pVehicleDamage->GetIsStuck(VEH_STUCK_HUNG_UP, ms_uMinStuckTimeMS) || pVehicleDamage->GetIsStuck(VEH_STUCK_ON_SIDE, ms_uMinStuckTimeMS)) )
				{
					// ped should use flee response
					return true;
				}
			}
		}
	}

	// by default do not use flee response
	return false;
}

////////////////////////
//FIRE NEARBY
////////////////////////

CEventFireNearby::CEventFireNearby(const Vector3& vFirePos)
: CEventEditableResponse(),
m_vFirePos(vFirePos)
{
#if !__FINAL
	m_EventType = EVENT_FIRE_NEARBY;
#endif
}

CEventFireNearby::~CEventFireNearby()
{
}

bool CEventFireNearby::AffectsPedCore(CPed* pPed) const
{
	if(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_COMPLEX_EXTINGUISH_FIRES))
	{
		return false;
	}

	return (!pPed->IsDead());
}

bool CEventFireNearby::TakesPriorityOver(const CEvent& UNUSED_PARAM(otherEvent) ) const 
{
	//Is a temporary event so always respond.
	return true;
}

//////////////////////
//PED ON CAR ROOF
//////////////////////

CEventPedOnCarRoof::CEventPedOnCarRoof(CPed* pPedOnRoof, CVehicle* pVehicle)
: m_pPed(pPedOnRoof),
m_pVehicle(pVehicle)
{
#if !__FINAL
	m_EventType = EVENT_PED_ON_CAR_ROOF;
#endif
}

CEventPedOnCarRoof::~CEventPedOnCarRoof()
{
}

dev_s32 TIME_BETWEEN_MOVES_TO_SHAKE_PED_FROM_CAR = 15000;

bool CEventPedOnCarRoof::AffectsPedCore(CPed* pPed) const
{
	if(0==m_pPed || 0==m_pVehicle)
	{
		//Need active ped to be on roof and active vehicle roof.
		return false;
	}

	if(pPed->GetMyVehicle() != m_pVehicle)
	{
		//Need ped on roof to be on the correct roof.
		return false;
	}

	if (NetworkInterface::GetSyncedTimeInMilliseconds() < 
		m_pVehicle->GetIntelligence()->m_nTimeLastTriedToShakePedFromRoof + TIME_BETWEEN_MOVES_TO_SHAKE_PED_FROM_CAR)
	{
		return false;
	}

	if(pPed->IsDead() || m_pPed->IsDead())
	{
		//Everyone needs to be alive.
		return false;
	}

	// Ignore this event if the ped is friendly with the ped on the roof.
	if (pPed->GetPedIntelligence()->IsFriendlyWith(*m_pPed))
	{
		bool bCanReactToFriendly = (pPed->PopTypeIsRandom() && pPed->IsGangPed());
		if(!bCanReactToFriendly)
		{
			return false;
		}
	}

	return true;
}

dev_s32 HORN_RESPONSE_TIME = 3000;
dev_s32 TEMP_RESPONSE_DRIVE_TIME = 1000;

bool CEventPedOnCarRoof::CreateResponseTask(CPed* pPed, CEventResponse& response) const
{
	CPed* pSourcePed = GetSourcePed();
	if(pSourcePed)
	{
		CVehicle* pVehicle = GetVehicle();
		if(NULL == pVehicle || pPed->GetMyVehicle() == NULL || pPed->GetMyVehicle() != pVehicle)
		{
			return false;
		}

		if (pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ))
		{
			// Only react to this event if you are driving. [4/30/2013 mdawe]
			if (!pPed->GetIsDrivingVehicle())
			{
				return false;
			}

			if(pVehicle->GetVehicleAudioEntity())
			{
				pVehicle->GetVehicleAudioEntity()->SetHornType(ATSTRINGHASH("Agressive", 0xC54A1CCB));
				pVehicle->GetVehicleAudioEntity()->PlayVehicleHorn();
			}
			float fRandom = fwRandom::GetRandomNumberInRange(0.0f, 1.0f);
			if(fRandom < 0.15f)
			{
				//response.SetEventResponse(CEventResponse::EventResponse, rage_new CTaskCarSetTempAction(pVehicle, TEMPACT_REVERSE, TEMP_RESPONSE_DRIVE_TIME));
				CTask* pVehicleTask = rage_new CTaskVehicleReverse(NetworkInterface::GetSyncedTimeInMilliseconds() + TEMP_RESPONSE_DRIVE_TIME, CTaskVehicleReverse::Reverse_Straight_Hard);
				CTask* pTask = rage_new CTaskControlVehicle(pVehicle, pVehicleTask);
				response.SetEventResponse(CEventResponse::EventResponse, pTask);
			}
			else if(fRandom < 0.30f)
			{
				//response.SetEventResponse(CEventResponse::EventResponse, rage_new CTaskCarSetTempAction(pVehicle, TEMPACT_REVERSE_LEFT, TEMP_RESPONSE_DRIVE_TIME));
				//response.SetEventResponse(CEventResponse::EventResponse, rage_new CTaskVehicleReverse(TEMP_RESPONSE_DRIVE_TIME, CTaskVehicleReverse::Reverse_Straight_Hard));
				CTask* pVehicleTask = rage_new CTaskVehicleReverse(NetworkInterface::GetSyncedTimeInMilliseconds() + TEMP_RESPONSE_DRIVE_TIME, CTaskVehicleReverse::Reverse_Straight_Hard);
				CTask* pTask = rage_new CTaskControlVehicle(pVehicle, pVehicleTask);
				response.SetEventResponse(CEventResponse::EventResponse, pTask);
			}
			else if(fRandom < 0.45f)
			{
				//response.SetEventResponse(CEventResponse::EventResponse, rage_new CTaskCarSetTempAction(pVehicle, TEMPACT_REVERSE_RIGHT, TEMP_RESPONSE_DRIVE_TIME));
				//response.SetEventResponse(CEventResponse::EventResponse, rage_new CTaskVehicleReverse(TEMP_RESPONSE_DRIVE_TIME, CTaskVehicleReverse::Reverse_Straight_Hard));
				CTask* pVehicleTask = rage_new CTaskVehicleReverse(NetworkInterface::GetSyncedTimeInMilliseconds() + TEMP_RESPONSE_DRIVE_TIME, CTaskVehicleReverse::Reverse_Straight_Hard);
				CTask* pTask = rage_new CTaskControlVehicle(pVehicle, pVehicleTask);
				response.SetEventResponse(CEventResponse::EventResponse, pTask);
			}
			else if(fRandom < 0.60f)
			{
				//response.SetEventResponse(CEventResponse::EventResponse, rage_new CTaskCarSetTempAction(pVehicle, TEMPACT_GOFORWARD, TEMP_RESPONSE_DRIVE_TIME));
				//response.SetEventResponse(CEventResponse::EventResponse, rage_new CTaskVehicleGoForward(TEMP_RESPONSE_DRIVE_TIME, true));
				CTask* pVehicleTask = rage_new CTaskVehicleGoForward(NetworkInterface::GetSyncedTimeInMilliseconds() + TEMP_RESPONSE_DRIVE_TIME, true);
				CTask* pTask = rage_new CTaskControlVehicle(pVehicle, pVehicleTask);
				response.SetEventResponse(CEventResponse::EventResponse, pTask);
			}

			if(!pPed->NewSay("GENERIC_FUCK_YOU"))
			{
				pPed->NewSay("GENERIC_INSULT_MED");
			}

			//Look at the ped.
			pPed->GetIkManager().LookAt(0, pSourcePed, 5000, BONETAG_HEAD, NULL);

			//also make this guy more aggressive
			pPed->GetPedIntelligence()->SetDriverAggressivenessOverride(1.0f);

			pVehicle->GetIntelligence()->m_nTimeLastTriedToShakePedFromRoof = NetworkInterface::GetSyncedTimeInMilliseconds();
		} // if In Vehicle
		else
		{
			// Add an event for this ped who's not in the car.
			response.SetEventResponse(CEventResponse::EventResponse, rage_new CTaskAgitated(pSourcePed));
			
			// Run the task on the secondary tree.
			response.SetRunOnSecondary(true);
		}
	}

	return response.HasEventResponse();
}

CEntity* CEventPedOnCarRoof::GetSourceEntity() const
{
	return m_pPed;
}
///////////////////////////
//STATIC COUNT REACHED MAX
///////////////////////////
#if __BANK
bool CEventStaticCountReachedMax::ms_bDebugClimbAttempts = false;
#endif

CEventStaticCountReachedMax::CEventStaticCountReachedMax(int iMovementTaskType) :
m_iMovementTaskType(iMovementTaskType)
{
#if !__FINAL
	m_EventType = EVENT_STATIC_COUNT_REACHED_MAX;
#endif
}

CEventStaticCountReachedMax::~CEventStaticCountReachedMax(void)
{

}

bool
CEventStaticCountReachedMax::AffectsPedCore(CPed* pPed ) const
{
	if( pPed->GetUsingRagdoll() )
		return false;

	if (pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_NM_CONTROL))
	{
		return false;
	}

	return true;
}

CTaskNavBase * CEventStaticCountReachedMax::FindNavTask(CPed * pPed, Vector3 & vOut_FirstRouteTarget, float & fOut_HeadingToTarget, Matrix34 & mOut_ClimbMat) const
{
	// Obtain the route task
	CTaskNavBase * pTaskNavBase = (CTaskNavBase*) pPed->GetPedIntelligence()->FindTaskActiveMovementByType(CTaskTypes::TASK_MOVE_WANDER);
	if(!pTaskNavBase)
		pTaskNavBase = (CTaskNavBase*) pPed->GetPedIntelligence()->FindTaskActiveMovementByType(CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH);

	// Ensure we have a route, and we are trying unsuccessfully to get to the start of it (ie. progress is 1)
	if(pTaskNavBase && pTaskNavBase->GetRoute() && pTaskNavBase->GetRoute()->GetSize()>0 && pTaskNavBase->GetProgress()==1)
	{
		Vector3 vPedPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
		vOut_FirstRouteTarget = pTaskNavBase->GetRoute()->GetPosition(0);

		if((pTaskNavBase->GetRoute()->GetWaypoint(0).GetSpecialActionFlags()&WAYPOINT_FLAG_IS_ON_WATER_SURFACE)==0)
		{
			vOut_FirstRouteTarget.z -= pPed->GetCapsuleInfo()->GetGroundToRootOffset();
		}

		Vector3 vToTarget = vOut_FirstRouteTarget-vPedPos;

		fOut_HeadingToTarget = fwAngle::GetRadianAngleBetweenPoints(vToTarget.x, vToTarget.y, 0.0f, 0.0f);
		mOut_ClimbMat.Identity();
		mOut_ClimbMat.MakeTranslate(vPedPos);
		mOut_ClimbMat.RotateZ(fOut_HeadingToTarget);

		return pTaskNavBase;
	}

	return NULL;
}


bool CEventStaticCountReachedMax::TestForClimb(CPed * pPed, float & fOut_HeadingToClimb) const
{
	static dev_float fMaxDistXY = 6.0f;
	static dev_float fMaxDistZ = 3.0f;

	CPhysical * pStuckInsideProp = IsStuckInsideProp(pPed);
	const CPhysical* pStuckOnVehicle = IsStuckOnVehicle(pPed);
	
	// If we are not on any navmesh currently
	if(!pPed->GetNavMeshTracker().GetIsValid() || (pStuckInsideProp!=NULL) || (pStuckOnVehicle != nullptr))
	{
		Vector3 vTarget;
		Matrix34 mClimbMat;
		float fHeading;

		if( FindNavTask(pPed, vTarget, fHeading, mClimbMat) )
		{
			Vector3 vPedPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
			Vector3 vToTarget = vTarget-vPedPos;

			// If we're close enough in XY
			if(vToTarget.XYMag2() < fMaxDistXY*fMaxDistXY)
			{
				// And close enough in Z
				if(Abs(vToTarget.z) < fMaxDistZ)
				{
					// Run the climb detector to look for a climb in our current heading
					CClimbHandHoldDetected handHold;
					sPedTestParams params;
					params.bForceSynchronousTest = true;
					pPed->SetPedResetFlag(CPED_RESET_FLAG_SearchingForClimb, true);
					pPed->SetPedResetFlag(CPED_RESET_FLAG_SearchingForAutoVaultClimb, true);

					CClimbDetector & climbDetector = pPed->GetPedIntelligence()->GetClimbDetector();
					climbDetector.Process(&mClimbMat, &params);

					// If we have a handhold in this direction
					if(climbDetector.GetDetectedHandHold(handHold))
					{
						// Don't climb over objects/vehicles
						if( (!handHold.GetPhysical()) || handHold.GetPhysical()==pStuckInsideProp || handHold.GetPhysical()==pStuckOnVehicle)
						{
							// Now perform capsule tests to determine whether we can travel the short distance to our target
							static dev_float fCapsuleRadius = 0.25f;

							Vector3 vTestFromPosition;
							vTestFromPosition = handHold.GetMaxGroundPosition();
							vTestFromPosition.z += fCapsuleRadius;

							Vector3 vPositionAboveTarget = vTarget;
							vPositionAboveTarget.z = vTestFromPosition.z;

							// Raise the target position to overcome intersections with uneven/sloped ground
							vTarget.z += fCapsuleRadius*2.0f;

							// Test a capsule from the top of the climb point, horizontally to the start of our route
							if(TestCapsule(vTestFromPosition, vPositionAboveTarget, fCapsuleRadius))
								return false;

							// And then test a capsule directly up/down to the route target
							if(TestCapsule(vPositionAboveTarget, vTarget, fCapsuleRadius))
								return false;

							Vector3 vToHandhold = handHold.GetMaxGroundPosition() - vPedPos;
							fOut_HeadingToClimb = fwAngle::GetRadianAngleBetweenPoints(vToHandhold.x, vToHandhold.y, 0.0f, 0.0f);

							return true;
						}
					}
				}
			}
		}
	}

#if __BANK
	// For debugging make it obvious where this took place
	if(ms_bDebugClimbAttempts)
	{
		Vector3 vPedPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
		grcDebugDraw::Sphere(vPedPos, 2.0f, Color_red, false, 5000);
		grcDebugDraw::Line(vPedPos, vPedPos+(ZAXIS*100.0f), Color_red, Color_red, 5000);
	}
#endif

	return false;
}

bool CEventStaticCountReachedMax::TestCapsule(const Vector3 & v1, const Vector3 & v2, float fRadius) const
{
#if __BANK
	if(ms_bDebugClimbAttempts)
		grcDebugDraw::Capsule(RCC_VEC3V(v1), RCC_VEC3V(v2), fRadius, Color_orange, false, 500);
#endif

	WorldProbe::CShapeTestCapsuleDesc capsule1;
	WorldProbe::CShapeTestResults probeResults;
	capsule1.SetResultsStructure(&probeResults);

	capsule1.SetCapsule(v1, v2, fRadius);
	capsule1.SetIsDirected(true);
	capsule1.SetDoInitialSphereCheck(false);
	capsule1.SetIncludeFlags(ArchetypeFlags::GTA_MAP_TYPE_MOVER|ArchetypeFlags::GTA_VEHICLE_TYPE|ArchetypeFlags::GTA_OBJECT_TYPE|ArchetypeFlags::GTA_GLASS_TYPE);
	capsule1.SetStateIncludeFlags(phLevelBase::STATE_FLAGS_ALL);
	capsule1.SetExcludeTypeFlags(ArchetypeFlags::GTA_PICKUP_TYPE);
	capsule1.SetContext(WorldProbe::LOS_GeneralAI);

	WorldProbe::GetShapeTestManager()->SubmitTest(capsule1, WorldProbe::PERFORM_SYNCHRONOUS_TEST);

	return capsule1.GetResultsStructure()->GetNumHits() != 0;
}

// PURPOSE: Hack function to return whether a ped is stuck inside specific prop(s)
CPhysical * CEventStaticCountReachedMax::IsStuckInsideProp(CPed * pPed) const
{
	if(pPed->GetGroundPhysical())
	{
		const u32 iModelIdx = pPed->GetGroundPhysical()->GetModelIndex();
		if(MI_PROP_BIN_14A.IsValid() && MI_PROP_BIN_14A.GetModelIndex()==iModelIdx)
		{
			return pPed->GetGroundPhysical();
		}
	}
	return NULL;
}

const CPhysical* CEventStaticCountReachedMax::IsStuckOnVehicle(const CPed* pPed) const
{
	if (NetworkInterface::IsGameInProgress() && pPed->GetGroundPhysical() && pPed->GetGroundPhysical()->GetIsTypeVehicle())
	{
		const CVehicle* pVehicle = static_cast<const CVehicle*>(pPed->GetGroundPhysical());
		if (const CVehicleModelInfo* pModelInfo = pVehicle->GetVehicleModelInfo())
		{
			if (pModelInfo->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_FORCE_AUTO_VAULT_ON_VEHICLE_WHEN_STUCK))
			{
				return pVehicle;
			}
		}
	}

	return nullptr;
}

bool CEventStaticCountReachedMax::CreateResponseTask(CPed* pPed, CEventResponse& response) const
{
	pPed->GetPedIntelligence()->GetEventScanner()->GetStaticMovementScanner().ResetStaticCounter();

	// Never do a jump back response on a ped running low-lod physics - they may penetrate the scenery
	if(pPed->GetPedAiLod().IsLodFlagSet(CPedAILod::AL_LodPhysics))
		return false;

	const bool bDisableWallHitAnimation = pPed->GetTaskData().GetIsFlagSet(CTaskFlags::DisableWallHitAnimation) || pPed->GetPedResetFlag(CPED_RESET_FLAG_DisableWallHitAnimation);

	CTask * pMainTask = pPed->GetPedIntelligence()->GetTaskActive();
	if(pMainTask)
	{
		switch(pMainTask->GetTaskType())
		{
			// If we're stuck trying to use a drop-down, then 50% of the time we will attempt a jump.
			case CTaskTypes::TASK_USE_DROPDOWN_ON_ROUTE:
			{
				if(!bDisableWallHitAnimation && fwRandom::GetRandomNumberInRange(0, 100) < 50)
				{
					response.SetEventResponse(CEventResponse::EventResponse, rage_new CTaskJumpVault(JF_ForceVault));
				}
			}
		}
	}

	// If ped has had 3 static count events in same location, within a short period of time - then trigger a jump.
	// Its quite likely that the ped is stuck behind something.

	//s32 iNumTimesStuck = pPed->GetPedIntelligence()->RecordStaticCountReachedMax(VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()));

	if(!response.HasEventResponse())
	{
		switch(GetMovementTaskType())
		{
			case CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH:
			case CTaskTypes::TASK_GO_TO_POINT_AIMING:
			{
				// Make swimming peds attempt to clamber out/over obstruction
				if(pPed->GetIsSwimming())
				{
					if (!bDisableWallHitAnimation)
					{
						response.SetEventResponse(CEventResponse::EventResponse, rage_new CTaskJumpVault(JF_ForceVault));
					}
				}
				else
				{
					// If we've been stuck in this position for a while, then try jumping
					// JB: I have disabled this since it causes too many odd issues
				
					float fHeadingToClimb;
					if( !bDisableWallHitAnimation && TestForClimb(pPed, fHeadingToClimb) )
					{
						static dev_u32 iFlags = JF_UseCachedHandhold;

						// Create a task sequence to orientate towards the handhold, and climb using it
						CTaskSequenceList* pSequence = rage_new CTaskSequenceList();
						pSequence->AddTask( rage_new CTaskComplexControlMovement( rage_new CTaskMoveAchieveHeading(fHeadingToClimb, 2.0f, PI/16.0f ) ) );
						pSequence->AddTask( rage_new CTaskJumpVault( iFlags ) );

						response.SetEventResponse(CEventResponse::EventResponse, rage_new CTaskUseSequence(*pSequence));
#if __BANK
						// For debugging make it obvious where this took place
						if(ms_bDebugClimbAttempts)
						{
							Vector3 vPedPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
							grcDebugDraw::Sphere(vPedPos, 2.0f, Color_green, false, 5000);
							grcDebugDraw::Line(vPedPos, vPedPos+(ZAXIS*100.0f), Color_green, Color_green, 5000);
						}
#endif
					}
					// Otherwise play the hit wall anim, which causes the ped to step back briefly
					else
					{
						//only play the animation if the task data allows us to
						if (!bDisableWallHitAnimation)
						{
							response.SetEventResponse(CEventResponse::EventResponse, rage_new CTaskHitWall());
						}
						else
						{
							response.SetEventResponse(CEventResponse::EventResponse, rage_new CTaskDoNothing(500));
						}
					}
				}
				break;
			}
			// Default behaviour is to jump backwards as if we've run into a wall
			default:
			{
				//only play the animation if the task data allows us to
				if (!bDisableWallHitAnimation && !pPed->GetIsSwimming())
				{
					response.SetEventResponse(CEventResponse::EventResponse, rage_new CTaskHitWall());
				}
				else
				{
					response.SetEventResponse(CEventResponse::EventResponse, rage_new CTaskDoNothing(500));
				}
				break;
			}
		}
	}

	return response.HasEventResponse();
}

#if __CATCH_EVENT_LEAK
bool CEventSwitch2NM::ms_bCatchEventLeak = false;
#endif

CEventSwitch2NM::CEventSwitch2NM(u32 nMaxTime, aiTask* pTaskNM, bool bScriptEvent /*= false*/, u32 nMinTime /*= 1000*/)
: m_nMinTime(nMinTime)
, m_nMaxTime(nMaxTime)
, m_pTaskNM(pTaskNM)
, m_bScriptEvent(bScriptEvent)
, m_bRagdollFinished(false)
{
#if !__FINAL
	m_EventType = EVENT_SWITCH_2_NM_TASK;
#endif

#if __CATCH_EVENT_LEAK
#define STACKENTRIES 32
	m_pCreationStackTrace = NULL;
	if(ms_bCatchEventLeak)
	{
		size_t trace[STACKENTRIES];
		sysStack::CaptureStackTrace(trace, STACKENTRIES, 1);
		sysStack::OpenSymbolFile();
		
		s32 entries = STACKENTRIES;
		char buffer[2048] = { 0 };

		for (const size_t *tp=trace; *tp && entries--; tp++)
		{
			char symname[256] = "unknown";
			sysStack::ParseMapFileSymbol(symname,sizeof(symname),*tp);
			strcat(buffer, symname);
			strcat(buffer, "\n");
		}

		m_pCreationStackTrace = StringDuplicate(buffer);

		sysStack::CloseSymbolFile();
	}
#endif
}
CEventSwitch2NM::~CEventSwitch2NM()
{
	if(m_pTaskNM)
		delete m_pTaskNM;

#if __CATCH_EVENT_LEAK
	if(m_pCreationStackTrace)
		StringFree(m_pCreationStackTrace);
#endif
}

bool CEventSwitch2NM::operator==(const fwEvent& otherEvent) const
{
	if(static_cast<const CEvent&>(otherEvent).GetEventType() == GetEventType())
	{
		const CEventSwitch2NM& otherNMEvent = static_cast<const CEventSwitch2NM&>(otherEvent);
		if(otherNMEvent.m_nMaxTime == m_nMaxTime &&
			otherNMEvent.m_bScriptEvent == m_bScriptEvent &&
			otherNMEvent.m_bRagdollFinished == m_bRagdollFinished &&
			((otherNMEvent.m_pTaskNM.Get() == m_pTaskNM.Get()) || (otherNMEvent.m_pTaskNM && m_pTaskNM && otherNMEvent.m_pTaskNM->GetTaskType() == m_pTaskNM->GetTaskType())))
		{
			return true;
		}
	}
	return false;
}

bool CEventSwitch2NM::AffectsPedCore(CPed* pPed) const
{
	if(m_pTaskNM)
	{
		if (!pPed->IsInjured() || (pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_DYING_DEAD)==NULL))
		{
			return true;
		}
	}

	return false;
}

bool CEventSwitch2NM::CreateResponseTask(CPed* pPed, CEventResponse& response) const
{
	taskAssertf(!dynamic_cast<CTaskNMControl*>(m_pTaskNM.Get()), "NM task is already associated with a control task.");
	taskAssertf(((CTask *) m_pTaskNM.Get())->IsNMBehaviourTask(), "Response task is not an NM behaviour task.");
	if (pPed->ShouldBeDead())
	{
		CTaskDyingDead* pDeadTask = rage_new CTaskDyingDead(NULL, 0, 0);
		pDeadTask->SetForcedNaturalMotionTask(rage_new CTaskNMControl(GetMinTime(), GetMaxTime(), CloneTaskNM(), 0));
		nmEntityDebugf(pPed, "EventSwitch2NM - Creating CTaskDyingDead response task: nm task '%s'", pDeadTask->GetForcedNaturalMotionTask() ? pDeadTask->GetForcedNaturalMotionTask()->GetName() : "None");
		response.SetEventResponse(CEventResponse::EventResponse, pDeadTask);
	}
	else
	{
		response.SetEventResponse(CEventResponse::EventResponse, rage_new CTaskNMControl(GetMinTime(), GetMaxTime(), CloneTaskNM(), CTaskNMControl::DO_BLEND_FROM_NM));
		nmEntityDebugf(pPed, "EventSwitch2NM - Creating response task: nm task '%s'", response.GetEventResponse(CEventResponse::EventResponse) ? response.GetEventResponse(CEventResponse::EventResponse)->GetName() : "None");
	}
	return response.GetEventResponse(CEventResponse::EventResponse) != NULL;
}

aiTask* CEventSwitch2NM::CloneTaskNM() const
{
	return (m_pTaskNM ? m_pTaskNM->Copy() : NULL);
}

// Disabled, no current code would ever modify the data here, and it had the potential
// to get in the way of making event types more data driven.
#if 0

int CScriptedPriorities::ms_priorities[NUM_AI_EVENTTYPE][MAX_NUM_PRIORITIES_PER_EVENT];

CScriptedPriorities::CScriptedPriorities()
{
	Clear();
}

CScriptedPriorities::~CScriptedPriorities()
{
	Clear();
}

void CScriptedPriorities::Init()
{
	Clear();
}

void CScriptedPriorities::Shutdown()
{
	Clear();
}

void CScriptedPriorities::Clear()
{
	int i;
	for(i=0;i<NUM_AI_EVENTTYPE;i++)
	{
		int j;
		for(j=0;j<MAX_NUM_PRIORITIES_PER_EVENT;j++)
		{
			ms_priorities[i][j]=-1;
		}
	}
}

bool CScriptedPriorities::Add(const int eventType, const int takesPriorityOverEventType)
{
	Assert(eventType>=0);
	Assert(eventType<NUM_AI_EVENTTYPE);
	Assert(takesPriorityOverEventType>=0);
	Assert(takesPriorityOverEventType<NUM_AI_EVENTTYPE);

	bool b=false;
	int j;
	for(j=0;j<MAX_NUM_PRIORITIES_PER_EVENT;j++)
	{
		if(-1==ms_priorities[eventType][j])
		{
			ms_priorities[eventType][j]=takesPriorityOverEventType;
			b=true;
			break;
		}
	}
	return b;
}

bool CScriptedPriorities::Remove(const int eventType, const int takesPriorityOverEventType)
{
	Assert(eventType>=0);
	Assert(eventType<NUM_AI_EVENTTYPE);
	Assert(takesPriorityOverEventType>=0);
	Assert(takesPriorityOverEventType<NUM_AI_EVENTTYPE);

	bool b=false;
	int j;
	for(j=0;j<MAX_NUM_PRIORITIES_PER_EVENT;j++)
	{
		if(takesPriorityOverEventType==ms_priorities[eventType][j])
		{
			ms_priorities[eventType][j]=-1;
			b=true;
			break;
		}
	}
	return b;
}

bool CScriptedPriorities::TakesPriorityOver(const int eventType, const int takesPriorityOverEventType)
{
	Assert(eventType>=0);
	Assert(eventType<NUM_AI_EVENTTYPE);
	Assert(takesPriorityOverEventType>=0);
	Assert(takesPriorityOverEventType<NUM_AI_EVENTTYPE);

	bool b=false;
	int j;
	for(j=0;j<MAX_NUM_PRIORITIES_PER_EVENT;j++)
	{
		if(ms_priorities[eventType][j]==takesPriorityOverEventType)
		{
			b=true;
			break;
		}
	}
	return b;
}

#endif	// 0

bool CEventPlayerCollisionWithPed::AffectsPedCore(CPed* pPed) const
{
	if (pPed->GetUsingRagdoll())
		return false;

	return CEventPedCollisionWithPed::AffectsPedCore(pPed);
}

bool CEventPlayerCollisionWithPed::CreateResponseTask(CPed* pPed, CEventResponse& response) const
{
	// Get the collision data.    
	CPed* pOtherPed = GetOtherPed();
	if(NULL == pOtherPed)
	{
		return false;
	}

	Assert(pPed->GetPlayerInfo());

	if(!CPedMotionData::GetIsStill(GetPedMoveBlendRatio()) && pPed->CanShovePed(pOtherPed) && CTaskShovePed::ShouldAllowTask(pPed, pOtherPed))
	{
		pPed->GetPedIntelligence()->AddTaskSecondary( rage_new CTaskShovePed((CPed*)pOtherPed), PED_TASK_SECONDARY_PARTIAL_ANIM );
		pPed->SetLastPedShoved(pOtherPed);
	}

	response.SetEventResponse(CEventResponse::EventResponse, NULL);
	return response.HasEventResponse();
}

bool CEventVehicleDamageWeapon::AffectsPedCore(CPed* pPed) const
{
	// Don't be affected by ourself
	CEntity* pInflictor = GetInflictor();
	if(pInflictor && pInflictor == pPed)
	{
		return false;
	}

	CWanted* pWanted = pPed->GetPlayerWanted();
	if (pPed->IsLocalPlayer() && pWanted)
	{
		if (fwTimer::GetTimeInMilliseconds() - pWanted->GetLastTimeWantedLevelClearedByScript() < 10000 )
		{
			return false;
		}
	}

	return true;
}

bool CEventVehicleDamageWeapon::ShouldCreateResponseTaskForPed(const CPed* pPed) const
{
	CEntity* pInflictor = GetInflictor();

	if( CTaskTypes::IsCombatTask(GetResponseTaskType()) &&
		pInflictor && pInflictor->GetIsTypePed() &&		
		pPed && !pPed->GetPedIntelligence()->CanAttackPed(static_cast<const CPed*>(pInflictor)) )
	{
		return false;
	}

	return true;
}

bool CEventVehicleDamageWeapon::CreateResponseTask(CPed* pPed, CEventResponse& response) const
{
	CEntity* pInflictor=GetInflictor();

	//Check if we should react.
	bool bReact = (GetDamageCaused() > 0.0f) || m_bIsContinuouslyColliding ||
		(pPed->GetIsInVehicle() && pPed->GetVehiclePedInside()->InheritsFromBoat());
	if(!bReact)
	{
		return false;
	}

	if(pPed->GetMyVehicle() && pPed->GetMyVehicle()==GetVehicle())
	{
		const int iTaskType=GetResponseTaskType();
		switch(iTaskType)
		{
		case CTaskTypes::TASK_NONE:
			break;
		case CTaskTypes::TASK_COMPLEX_LEAVE_CAR_AND_FLEE:
			{
				CTaskSequenceList* pSequence = rage_new CTaskSequenceList();
				VehicleEnterExitFlags vehicleFlags;
				vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::DontCloseDoor);
				pSequence->AddTask(rage_new CTaskExitVehicle(pPed->GetMyVehicle(), vehicleFlags));
				CTaskSmartFlee* pFleeTask = rage_new CTaskSmartFlee(pPed->GetMyVehicle());
				pFleeTask->SetFleeFlags(m_FleeFlags);
				pSequence->AddTask(pFleeTask);
				response.SetEventResponse(CEventResponse::EventResponse, rage_new CTaskUseSequence(*pSequence));
			}
			break;
// CS - Disabled due to TASK_COMPLEX_CAR_DRIVE_MISSION_FLEE_SCENE no longer existing
// 		case CTaskTypes::TASK_COMPLEX_CAR_DRIVE_MISSION_FLEE_SCENE:
// 			if(pInflictor->GetType()==ENTITY_TYPE_PED)
// 			{
// 				CTask* pCombatTask = CTaskThreatResponse::CreateCombatTask((CPed*)pInflictor, true, false);
// 				pPed->GetPedIntelligence()->RegisterThreat((CPed*)pInflictor, true);
// 				response.SetEventResponse(CEventResponse::EventResponse, pCombatTask);
// 			}
// 			else
// 			{
// 				CTaskSmartFlee* pFleeTask=rage_new CTaskSmartFlee(CAITarget(pInflictor));
// 				response.SetEventResponse(CEventResponse::EventResponse, pFleeTask);
// 			}
// 			break;
		case CTaskTypes::TASK_COMPLEX_CAR_DRIVE_MISSION_KILL_PED:
			if(pInflictor && pInflictor->GetType()==ENTITY_TYPE_PED)
			{					
				aiTask* pCombatTask = rage_new CTaskThreatResponse((CPed*)pInflictor);
				pPed->GetPedIntelligence()->RegisterThreat((CPed*)pInflictor, true);
				response.SetEventResponse(CEventResponse::EventResponse, pCombatTask);
			}
			break;

		case CTaskTypes::TASK_COMBAT:
			if(pInflictor && pInflictor->GetIsTypePed())
			{
				CPed* pInflictorPed = (CPed*)pInflictor;
				// Up the wanted level of the player
				if( pPed->GetPedType() == PEDTYPE_COP && pInflictorPed->IsPlayer() && pInflictorPed->GetPlayerWanted()->GetWantedLevel() == WANTED_CLEAN &&  pInflictorPed->GetPlayerWanted()->m_fMultiplier > 0.0f )
				{
					const Vector3 vInflictorPedPosition = VEC3V_TO_VECTOR3(pInflictorPed->GetTransform().GetPosition());
					pInflictorPed->GetPlayerWanted()->SetWantedLevelNoDrop(vInflictorPedPosition, WANTED_LEVEL1, WANTED_CHANGE_DELAY_QUICK, WL_FROM_LAW_RESPONSE_EVENT);
					NA_POLICESCANNER_ENABLED_ONLY(g_PoliceScanner.ReportPlayerCrime(vInflictorPedPosition, CRIME_HIT_COP, WANTED_CHANGE_DELAY_QUICK));
				}

				aiTask* pCombatTask = CTaskThreatResponse::CreateForTargetDueToEvent(pPed, (CPed*)pInflictor, *this);

				if (Verifyf(pCombatTask, "NULL threat response task returned from CreateForTargetDueToEvent, ped won't have a response."))
				{
					response.SetEventResponse(CEventResponse::EventResponse, pCombatTask);
					pPed->GetPedIntelligence()->RegisterThreat((CPed*)pInflictor, true);
				}
			}
			else if(pInflictor && pInflictor->GetIsTypeVehicle())
			{
				if(pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ))
				{
					CVehicle* pOtherVehicle = (CVehicle*)pInflictor;

					if(pPed->GetPedType() == PEDTYPE_COP)
					{
						if(pPed->GetMyVehicle()->m_nVehicleFlags.GetIsSirenOn())
						{
							// Work out if the vehicle we hit is our target vehicle
							CEntity* pTarget = pPed->GetPedIntelligence()->GetQueriableInterface()->GetHostileTarget();
							bool bHitTarget = pTarget && pTarget->GetIsTypePed() && pOtherVehicle && pOtherVehicle->ContainsPed(static_cast<CPed*>(pTarget));
							if(!bHitTarget)
								pPed->NewSay("BLOCKED_IN_PURSUIT");
						}
					}
					else if(!pPed->IsAPlayerPed() && (!pPed->PopTypeIsMission() || pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_AllowMinorReactionsAsMissionPed)))
					{
						//is player in THIS vehicle or is player in OTHER vehicle
						if((pPed->GetMyVehicle()->ContainsPlayer() && !pPed->GetMyVehicle()->GetIsRotaryAircraft()) || (pOtherVehicle && pOtherVehicle->ContainsPlayer() && !pOtherVehicle->GetIsRotaryAircraft()))
						{
							if(pPed->GetPedIntelligence()->FindTaskByType(CTaskTypes::TASK_REACT_TO_PURSUIT) == NULL)
							{
								// We have not been bumped yet - perform random reaction for the first time - say angry VO etc...
								CTaskCarReactToVehicleCollision* pTask = rage_new CTaskCarReactToVehicleCollision(pPed->GetMyVehicle(), pOtherVehicle, GetDamageCaused(), GetDamagePos(), GetDamageDir());
								pTask->SetIsContinuouslyColliding(m_bIsContinuouslyColliding, m_fSpeedOfContinuousCollider);
								response.SetEventResponse(CEventResponse::EventResponse, pTask);
								if(pPed && !pPed->IsLocalPlayer() && pPed->GetSpeechAudioEntity() && pPed->GetMyVehicle() && pPed->GetIsDrivingVehicle() && pOtherVehicle)
								{
									pPed->SetGestureAnimsAllowed(true);
									pPed->GetSpeechAudioEntity()->PlayCarCrashSpeech(pOtherVehicle->GetDriver(), pPed->GetMyVehicle(), pOtherVehicle);
									
									if (pPed->CheckBraveryFlags(BF_PLAY_CAR_HORN))
									{
										const bool bLongHorn = pPed->GetMyVehicle()->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_BIG);
										const u32 hornTypeHash = bLongHorn ? ATSTRINGHASH("HELDDOWN",0x839504CB) : 0;
										pPed->GetMyVehicle()->PlayCarHorn(true, hornTypeHash);
									}
								}
							}
							else if(pPed->GetPedIntelligence()->GetPedMotivation().GetMotivation(CPedMotivation::ANGRY_STATE) > REALLY_ANGRY)
							{
								// We have been continuously harassed - go into combat task
								if(!pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_COMBAT))
								{
									CPed* pInflictingPed = pOtherVehicle->GetSeatManager()->GetDriver() ? pOtherVehicle->GetSeatManager()->GetDriver() : pOtherVehicle->PickRandomPassenger();
									if(pInflictingPed)
									{
										aiTask* pCombatTask = CTaskThreatResponse::CreateForTargetDueToEvent(pPed, pInflictingPed, *this);

										if (Verifyf(pCombatTask, "NULL threat response task returned from CreateForTargetDueToEvent, ped won't have a response."))
										{
											response.SetEventResponse(CEventResponse::EventResponse, pCombatTask);
											pPed->GetPedIntelligence()->RegisterThreat(pInflictingPed, true);
										}
									}
								}
							}
						}
					}
				}
				//if not, finish
			}
			break;

		default:
			return CEventVehicleDamage::CreateResponseTask(pPed, response);
		}
	}

	return response.HasEventResponse();
}

IMPLEMENT_GENERAL_EVENT_TUNABLES(CEventSuspiciousActivity, 0x155e0024);
CEventSuspiciousActivity::Tunables CEventSuspiciousActivity::sm_Tunables;

CEventSuspiciousActivity::CEventSuspiciousActivity( CPed* pPed, const Vector3& vPosition, const float fRange )
: m_pPed(pPed)
, m_vPosition(vPosition)
, m_fRange(fRange)
{
#if !__FINAL
	m_EventType = EVENT_SUSPICIOUS_ACTIVITY;
#endif
}

CEventSuspiciousActivity::~CEventSuspiciousActivity()
{

}

bool CEventSuspiciousActivity::AffectsPedCore( CPed* pPed ) const
{
	if (!pPed->GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_CanInvestigate))
	{
		return false;
	}

	if (!m_pPed)
	{
		return false;
	}

	if (!pPed->GetPedIntelligence()->IsThreatenedBy(*m_pPed))
	{
		return false;
	}

	// Check to see if we're in range of the sound. [12/11/2012 mdawe]
	const float fDist2 = (VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()) - m_vPosition).Mag2();
	if (fDist2 > rage::square(m_fRange))
	{
		return false;
	}
	return true;
}

bool CEventSuspiciousActivity::CreateResponseTask( CPed* pPed, CEventResponse& response ) const
{
	if (!m_pPed)
	{
		return false;
	}

	if (pPed->IsLocalPlayer())
	{
		return false;
	}
	
	// Check to see if we're in range of the sound. [12/11/2012 mdawe]
	const float fDist2 = (VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()) - m_vPosition).Mag2();
	if (fDist2 > rage::square(m_fRange))
	{
		return false;
	}

 	// Check previous events, if we've already reacted, don't bother with this event.
	// SuspiciousActivity events are okay, since we'll just update the activity location.
	int iEventType = pPed->GetPedIntelligence()->GetCurrentEventType();
 	switch (iEventType)
 	{
 		case EVENT_REACTION_INVESTIGATE_THREAT:
 		case EVENT_REACTION_ENEMY_PED:
 		case EVENT_ACQUAINTANCE_PED_HATE:
 			return false;
 		default: 
 			break;
 	}

	if ( pPed->GetPedIntelligence()->IsThreatenedBy(*m_pPed) &&
			 pPed->GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_CanInvestigate) )
	{
		// If we're already investigating, just update the investigation position. [12/11/2012 mdawe]
		CTaskInvestigate* pExistingInvestigationTask = static_cast<CTaskInvestigate*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_INVESTIGATE));
		if (pExistingInvestigationTask)
		{
			pExistingInvestigationTask->SetInvestigationPos(m_vPosition);
		}
		else
		{
			// Otherwise start a new investigation.
			CTask* pTask = rage_new CTaskInvestigate(m_vPosition, NULL, EVENT_SUSPICIOUS_ACTIVITY);
			response.SetEventResponse(CEventResponse::EventResponse, pTask);
		}
	}	

	return response.HasEventResponse();
}

bool CEventSuspiciousActivity::TryToCombine(const CEventSuspiciousActivity& other)
{
	if (m_pPed == other.m_pPed)
	{
		if ( (m_vPosition - other.m_vPosition).Mag2() < sm_Tunables.fMinDistanceToBeConsideredSameEvent )
		{
			m_vPosition = other.m_vPosition;
			m_fRange = other.m_fRange;
			return true;
		}
	}
	return false;
}


CEventUnidentifiedPed::CEventUnidentifiedPed(CPed* pPed )
: m_pUnidentifiedPed(pPed)
{
#if !__FINAL
	m_EventType = EVENT_UNIDENTIFIED_PED;
#endif
}

CEventUnidentifiedPed::~CEventUnidentifiedPed()
{

}

bool CEventUnidentifiedPed::CreateResponseTask( CPed* pPed, CEventResponse& response ) const
{
	aiFatalAssertf(m_pUnidentifiedPed->GetIsTypePed(), "The entity is not a ped - no suitable response task");

    if (pPed->IsLocalPlayer())
	{
		return false;
	}

	CTask* pTask = rage_new CTaskInvestigate(VEC3V_TO_VECTOR3(m_pUnidentifiedPed->GetTransform().GetPosition()),NULL,EVENT_UNIDENTIFIED_PED);
	
	response.SetEventResponse(CEventResponse::EventResponse, pTask);

	return response.HasEventResponse();
}

CEventDeadPedFound::CEventDeadPedFound(CPed* pPed, bool bWitnessedFirstHand)
: m_pDeadPed(pPed),
m_bWitnessedFirstHand(bWitnessedFirstHand)
{
#if !__FINAL
	m_EventType = EVENT_DEAD_PED_FOUND;
#endif
}

bool CEventDeadPedFound::AffectsPedCore(CPed* pPed) const
{
	// Doesn't affect the player
	if (pPed->IsLocalPlayer())
	{
		return false;
	}

	CCombatBehaviour& combatBehaviour = pPed->GetPedIntelligence()->GetCombatBehaviour();
	// Or peds with dead ped scanning off
	if (!combatBehaviour.IsFlagSet(CCombatData::BF_WillScanForDeadPeds))
	{
		return false;
	}

	// Or mission peds who aren't guards
	if (pPed->PopTypeIsMission())
	{
		return false;
	}

	return true;
}

bool CEventDeadPedFound::CreateResponseTask( CPed* UNUSED_PARAM(pPed), CEventResponse& response ) const
{
	if (m_pDeadPed)
	{			
		if (m_bWitnessedFirstHand)
		{
			response.SetEventResponse(CEventResponse::EventResponse, rage_new CTaskReactToDeadPed(m_pDeadPed, true));
		}
		else
		{
			response.SetEventResponse(CEventResponse::EventResponse, rage_new CTaskReactToDeadPed(m_pDeadPed, false));
		}	
		return true;	
	}

	return false;
}

CEventDeadPedFound* CEventDeadPedFound::CloneEventSpecific(bool bWitnessedFirstHand) const
{
	CEventDeadPedFound* pClone = static_cast<CEventDeadPedFound*>(Clone());
	pClone->m_bWitnessedFirstHand  = bWitnessedFirstHand;
	return pClone;
}

IMPLEMENT_GENERAL_EVENT_TUNABLES(CEventAgitated, 0xfd9ed816);
CEventAgitated::Tunables CEventAgitated::sm_Tunables;

CEventAgitated::CEventAgitated(CPed* pAgitator, AgitatedType nType)
: m_pAgitator(pAgitator)
, m_nType(nType)
, m_uTimeStamp(fwTimer::GetTimeInMilliseconds())
, m_fMaxDistance(-1.0f)
{
#if !__FINAL
	m_EventType = EVENT_AGITATED;
#endif
}

CEventAgitated::~CEventAgitated()
{
}

int CEventAgitated::GetEventPriority() const
{
	//Check the type.
	switch(m_nType)
	{
		case AT_Bumped:
		case AT_BumpedByVehicle:
		{
			return E_PRIORITY_DAMAGE;
		}
		default:
		{
			return E_PRIORITY_AGITATED;
		}
	}
}

bool CEventAgitated::AffectsPedCore(CPed* pPed) const
{
	//Ensure the event is enabled.
	if(CAgitatedManager::GetInstance().AreAgitationEventsSuppressed())
	{
		return false;
	}

	//Ensure the ped is not a player.
	if(pPed->IsPlayer())
	{
		return false;
	}
	
	//Ensure the ped can be agitated.
	if(!pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_CanBeAgitated))
	{
		return false;
	}

	//Ensure the ped has not disabled agitation.
	if(pPed->GetPedResetFlag(CPED_RESET_FLAG_DisableAgitation))
	{
		return false;
	}

	//Don't get into disputes with stationary reaction peds.
	if (pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_CowerInsteadOfFlee))
	{
		return false;
	}

	//Ensure the ped is not riding a train.
	if(pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_RidingTrain))
	{
		return false;
	}

	//Check if the ped is law enforcement.
	if(pPed->IsLawEnforcementPed())
	{
		//Ensure wanted is enabled.
		if(m_pAgitator && m_pAgitator->GetPlayerWanted() &&
			((m_pAgitator->GetPlayerWanted()->MaximumWantedLevel <= WANTED_CLEAN) || (m_pAgitator->GetPlayerWanted()->m_fMultiplier <= 0.0f)))
		{
			return false;
		}
	}

	//Check if the event is stale
	if (fwTimer::GetTimeInMilliseconds() - m_uTimeStamp >= sm_Tunables.m_TimeToLive)
	{
		return false;
	}

	//Ensure the agitator is valid.
	if(!m_pAgitator)
	{
		return false;
	}

	//Ensure the agitator is a player.
	if(!m_pAgitator->IsPlayer())
	{
		return false;
	}

	//Ensure the agitator has not disabled agitation.
	if(m_pAgitator->GetPedResetFlag(CPED_RESET_FLAG_DisableAgitation))
	{
		return false;
	}
	else if(m_pAgitator->GetPlayerInfo()->GetDisableAgitation())
	{
		return false;
	}

	//Ensure we should not ignore.
	if(ShouldIgnore(*pPed))
	{
		return false;
	}

	//Ensure the ped is not friendly with the agitator.
	if(pPed->GetPedIntelligence()->IsFriendlyWith(*m_pAgitator))
	{
		//Check if we should set the friendly exception.
		if(ShouldSetFriendlyException(*pPed))
		{
			//Set the friendly exception.
			pPed->GetPedIntelligence()->SetFriendlyException(m_pAgitator);
		}
		else
		{
			return false;
		}
	}

	//Ensure the ped does not ignore the agitator.
	if(pPed->GetPedIntelligence()->Ignores(*m_pAgitator))
	{
		return false;
	}

	//Ensure the distance is valid.
	if(m_fMaxDistance >= 0.0f)
	{
		//Ensure the distance is valid.
		ScalarV scDistSq = DistSquared(pPed->GetTransform().GetPosition(), m_pAgitator->GetTransform().GetPosition());
		ScalarV scMaxDistSq = ScalarVFromF32(square(m_fMaxDistance));
		if(IsGreaterThanAll(scDistSq, scMaxDistSq))
		{
			return false;
		}
	}

	//Check if the event should be ignored.
	if(CTaskAgitated::ShouldIgnoreEvent(*pPed, *this))
	{
		return false;
	}

	//Check if the ped is running combat.
	if(pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_COMBAT))
	{
		//Register the ped as a threat.
		pPed->GetPedIntelligence()->RegisterThreat(m_pAgitator);

		return false;
	}

	//Check if we are already agitated (presumably from someone else, otherwise the agitated task would have consumed it).
	if(pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsAgitated))
	{
		return false;
	}

	//Check if the ped is fleeing.
	if(pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_SMART_FLEE))
	{
		return false;
	}
	//Check if the ped is hurrying away.
	else if(pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_SHOCKING_EVENT_HURRYAWAY))
	{
		return false;
	}

	//Check if the ped has an order.
	if(pPed->GetPedIntelligence()->GetOrder())
	{
		//Check if the ped is law-enforcement.
		if(pPed->IsLawEnforcementPed())
		{
			//Say some audio.
			pPed->NewSay("GENERIC_INSULT_MED");
		}

		return false;
	}

	return true;
}

bool CEventAgitated::TakesPriorityOver(const CEvent& otherEvent) const
{
	//Check the other event type.
	switch(otherEvent.GetEventType())
	{
		case EVENT_PED_COLLISION_WITH_PLAYER:
			return true;
		default:
			break;
	}

	//Call the base class version.
	return CEvent::TakesPriorityOver(otherEvent);
}

bool CEventAgitated::ShouldTriggerAmbientReaction(const CPed& rPed) const
{
	//ensure that the agitator pointer is valid
	if (!m_pAgitator)
	{
		return false;
	}

	//Grab the tunables.
	const Tunables& rTunables = sm_Tunables;

	//Ensure the random check succeeded.
	float fRandom = fwRandom::GetRandomNumberInRange(0.0f, 1.0f);
	if(fRandom >= rTunables.m_TriggerAmbientReactionChances)
	{
		return false;
	}

	//Ensure the ambient clips are running.
	if(!rPed.GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_AMBIENT_CLIPS))
	{
		return false;
	}

	//Grab the ped position.
	Vec3V vPedPosition = rPed.GetTransform().GetPosition();

	//Get the event position.
	Vec3V vEventPosition = m_pAgitator->GetTransform().GetPosition();

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

void CEventAgitated::TriggerAmbientReaction(const CPed& rPed) const
{

	if (!m_pAgitator)
	{
		return;
	}

	//Grab the tunables.
	const Tunables& rTunables = sm_Tunables;


	//Ensure the ambient clips are valid.
	CTaskAmbientClips* pTask = static_cast<CTaskAmbientClips *>(rPed.GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_AMBIENT_CLIPS));
	if(pTask)
	{
		//Create an ambient event.
		CAmbientEvent ambientEvent(rTunables.m_AmbientEventType, m_pAgitator->GetTransform().GetPosition(), rTunables.m_AmbientEventLifetime, fwRandom::GetRandomNumberInRange(rTunables.m_MinTimeForAmbientReaction, rTunables.m_MaxTimeForAmbientReaction));

		//Add the ambient event.
		pTask->AddEvent(ambientEvent);
	}
}

bool CEventAgitated::ShouldIgnore(const CPed& rPed) const
{
	//Check if we should ignore honked at.
	if((m_nType == AT_HonkedAt) && CAgitatedManager::GetInstance().GetTriggers().IsFlagSet(rPed, CAgitatedTriggerSet::IgnoreHonkedAt))
	{
		return true;
	}

	//Not valid for animals to respond to bumped.
	if(m_nType == AT_Bumped && rPed.GetPedType() == PEDTYPE_ANIMAL)
	{
		return true;
	}

	return false;
}

bool CEventAgitated::ShouldSetFriendlyException(const CPed& rPed) const
{
	//Ensure we are not in MP.
	if(NetworkInterface::IsGameInProgress())
	{
		return false;
	}

	//Ensure the ped is random.
	if(!rPed.PopTypeIsRandom())
	{
		return false;
	}

	//Check the type.
	switch(m_nType)
	{
		case AT_Bumped:
		case AT_BumpedByVehicle:
		case AT_Insulted:
		case AT_Griefing:
		{
			break;
		}
		default:
		{
			return false;
		}
	}

	return true;
}

// Return a cloned event with the same timestamp
CEventEditableResponse* CEventAgitated::CloneEditable() const
{ 
	CEventAgitated* pClone = rage_new CEventAgitated(m_pAgitator, m_nType);
	pClone->m_uTimeStamp = m_uTimeStamp;
	pClone->SetMaxDistance(m_fMaxDistance);
	return pClone;
}

CEventAgitatedAction::CEventAgitatedAction(const CPed* pAgitator, aiTask* pTask)
: m_pAgitator(pAgitator)
, m_pTask(pTask)
{
#if !__FINAL
	m_EventType = EVENT_AGITATED_ACTION;
#endif
}

CEventAgitatedAction::~CEventAgitatedAction()
{
	//Free the task.
	if(m_pTask)
	{
		delete m_pTask;
	}
}

bool CEventAgitatedAction::AffectsPedCore(CPed* pPed) const
{
	//Ensure the task is valid.
	if(!m_pTask)
	{
		return false;
	}
	
	//Ensure the ped is not injured.
	if(pPed->IsInjured())
	{
		return false;
	}
	
	//Ensure the ped is not a player.
	if(pPed->IsPlayer())
	{
		return false;
	}
	
	return true;
}

bool CEventAgitatedAction::CreateResponseTask(CPed* UNUSED_PARAM(pPed), CEventResponse& response) const
{
	//Create the task.
	CTask* pTask = rage_new CTaskAgitatedAction(m_pTask ? m_pTask->Copy() : NULL);
	
	//Set the event response.
	response.SetEventResponse(CEventResponse::EventResponse, pTask);
	
	return response.HasEventResponse();
}

CEntity* CEventAgitatedAction::GetSourceEntity() const
{
	CPed* pPed = const_cast<CPed*>(m_pAgitator.Get());
	return pPed;
}

//////////////////////////////////////////////////////////////////////////
//
// CEventScenarioForceAction
//
//////////////////////////////////////////////////////////////////////////

CEventScenarioForceAction::CEventScenarioForceAction()
	: m_pTask(NULL)
{
}

CEventScenarioForceAction::CEventScenarioForceAction(const CEventScenarioForceAction& in_rhs)
{
	if ( in_rhs.m_pTask )
	{
		m_pTask = in_rhs.m_pTask->Copy();
	}
	
	m_ForceActionType = in_rhs.m_ForceActionType;
	m_Position = in_rhs.m_Position;
	
}

CEventScenarioForceAction::~CEventScenarioForceAction()
{
	delete m_pTask;
}

void CEventScenarioForceAction::SetTask(aiTask* in_Task)
{
	m_pTask = in_Task;
}

bool CEventScenarioForceAction::CreateResponseTask(CPed*, CEventResponse& response) const
{
	response.SetEventResponse(CEventResponse::EventResponse, CloneScriptTask());
	return response.HasEventResponse();
}

aiTask* CEventScenarioForceAction::CloneScriptTask() const
{
	aiTask* p=0;
	if(m_pTask)
	{
		p=m_pTask->Copy();
		Assert( p->GetTaskType() == m_pTask->GetTaskType() );
	}
	return p;
}

CEventCombatTaunt::CEventCombatTaunt(const CPed* pTaunter)
: m_pTaunter(pTaunter)
, m_fTotalTime(0.0f)
, m_fTimeSinceTaunterStoppedTalking(0.0f)
, m_bHasRegisteredThreat(false)
{

}

CEventCombatTaunt::~CEventCombatTaunt()
{

}

bool CEventCombatTaunt::IsTaunterTalking() const
{
	//Ensure the taunter is valid.
	if(!m_pTaunter)
	{
		return false;
	}

	//Ensure the speech audio entity is valid.
	const audSpeechAudioEntity* pSpeechAudioEntity = m_pTaunter->GetSpeechAudioEntity();
	if(!pSpeechAudioEntity)
	{
		return false;
	}

	//Ensure ambient speech is playing.
	if(!pSpeechAudioEntity->IsAmbientSpeechPlaying())
	{
		return false;
	}

	return true;
}

bool CEventCombatTaunt::ShouldPedIgnoreEvent(const CPed* pPed) const
{
	//Ensure the ped is not injured.
	if(pPed->IsInjured())
	{
		return true;
	}

	//Ensure the ped is not a player.
	if(pPed->IsPlayer())
	{
		return true;
	}

	//Ensure the ped isn't ignoring combat taunts.
	if(pPed->GetPedResetFlag(CPED_RESET_FLAG_IgnoreCombatTaunts))
	{
		return true;
	}

	//Ensure the taunter is valid.
	if(!m_pTaunter)
	{
		return true;
	}

	//Ensure the taunter is not injured.
	if(m_pTaunter->IsInjured())
	{
		return true;
	}

	//Check if we have registered the target as a threat.
	const CPedTargetting* pTargeting = pPed->GetPedIntelligence()->GetTargetting(false);
	if(pTargeting && pTargeting->FindTarget(m_pTaunter))
	{
		return false;
	}

	//Check if our order is associated with the target.
	const COrder* pOrder = pPed->GetPedIntelligence()->GetOrder();
	if(pOrder && pOrder->GetIncident() && (m_pTaunter.Get() == pOrder->GetIncident()->GetEntity()))
	{
		return false;
	}

	return true;
}

bool CEventCombatTaunt::AffectsPedCore(CPed* pPed) const
{
	//Ensure the ped should not ignore the event.
	if(ShouldPedIgnoreEvent(pPed))
	{
		return false;
	}

	return true;
}

bool CEventCombatTaunt::HasExpired() const
{
	//Ensure the event is not flagged for deletion.
	if(!GetFlagForDeletion())
	{
		return false;
	}

	return true;
}

bool CEventCombatTaunt::IsEventReady(CPed* pPed)
{
	//Check if we should not ignore the event.
	if(!ShouldPedIgnoreEvent(pPed))
	{
		//Grab the time step.
		float fTimeStep = GetTimeStep();

		//Add to the total time.
		float fLastTotalTime = m_fTotalTime;
		m_fTotalTime += fTimeStep;

		//Check if we should register the threat.
		bool bIsTaunterTalking = IsTaunterTalking();
		static float s_fMaxTimeToRegisterThreat = 1.0f;
		bool bShouldRegisterThreat = !m_bHasRegisteredThreat &&
			(!bIsTaunterTalking || ((fLastTotalTime < s_fMaxTimeToRegisterThreat) && (m_fTotalTime >= s_fMaxTimeToRegisterThreat)));
		if(bShouldRegisterThreat)
		{
			//Note that we have registered the threat.
			m_bHasRegisteredThreat = true;

			//Note that we know where the target is.
			CPedTargetting* pTargeting = pPed->GetPedIntelligence()->GetTargetting(true);
			aiAssert(pTargeting);
			pTargeting->RegisterThreat(m_pTaunter, true);
		}

		//Ensure the taunter is not talking.
		if(IsTaunterTalking())
		{
			return false;
		}

		//Add to the time since the taunter stopped talking.
		float fLastTimeSinceTaunterStoppedTalking = m_fTimeSinceTaunterStoppedTalking;
		m_fTimeSinceTaunterStoppedTalking += fTimeStep;

		//Ensure we should respond to the taunter.
		static float s_fMinDelayForResponse = 0.5f;
		static float s_fMaxDelayForResponse = 1.0f;
		float fDelayForResponse = pPed->GetRandomNumberInRangeFromSeed(s_fMinDelayForResponse, s_fMaxDelayForResponse);
		bool bShouldRespond = ((fLastTimeSinceTaunterStoppedTalking < fDelayForResponse) && (m_fTimeSinceTaunterStoppedTalking >= fDelayForResponse));
		if(!bShouldRespond)
		{
			return false;
		}

		//Say a response.
		const_cast<CPed *>(pPed)->NewSay("GENERIC_FUCK_YOU");
	}

	//Flag the event for deletion.
	SetFlagForDeletion(true);

	return false;
}

IMPLEMENT_GENERAL_EVENT_TUNABLES(CEventRespondedToThreat, 0x004df11d);
CEventRespondedToThreat::Tunables CEventRespondedToThreat::sm_Tunables;

CEventRespondedToThreat::CEventRespondedToThreat(const CPed* pPedBeingThreatened, const CPed* pPedBeingThreatening)
: m_pPedBeingThreatened(pPedBeingThreatened)
, m_pPedBeingThreatening(pPedBeingThreatening)
, m_bThreateningPedWasWanted(false)
{
	// If the ped being threatened is a random cop then we should check if the threatening ped is wanted
	if(m_pPedBeingThreatened && m_pPedBeingThreatened->PopTypeIsRandom() && m_pPedBeingThreatened->IsLawEnforcementPed())
	{
		m_bThreateningPedWasWanted = ThreateningPedIsWanted();
	}
}

CEventRespondedToThreat::~CEventRespondedToThreat()
{

}

void CEventRespondedToThreat::OnEventAdded(CPed *pPed) const 
{
	CEvent::OnEventAdded(pPed);

	//Register the threat.
	if(m_pPedBeingThreatening)
	{
		CPedIntelligence* pPedIntelligence = pPed->GetPedIntelligence();

		pPedIntelligence->RegisterThreat(m_pPedBeingThreatening, false);

		//NOTE[egarcia]: Keep targetting alive for the CTaskCombat
		if (pPedIntelligence->GetCombatBehaviour().IsFlagSet(CCombatData::BF_PreferKnownTargetsWhenCombatClosestTarget))
		{
			CPedTargetting* pPedTargetting = pPedIntelligence->GetTargetting(true);
			if (pPedTargetting)
			{
				pPedTargetting->SetInUse();
			}
		}
	}
}

bool CEventRespondedToThreat::AffectsPedCore(CPed* pPed) const
{
	//Ensure the ped is not injured.
	if(pPed->IsInjured())
	{
		return false;
	}

	//Ensure the ped is not a player.
	if(pPed->IsPlayer())
	{
		return false;
	}

	//Ensure the ped being threatened is valid.
	if(!m_pPedBeingThreatened)
	{
		return false;
	}

	//Ensure the ped being threatening is valid.
	if(!m_pPedBeingThreatening)
	{
		return false;
	}

	//Ensure the ped being threatening is not injured.
	if(m_pPedBeingThreatening->IsInjured())
	{
		return false;
	}

	//Ensure the ped is not being threatened by themselves.
	if(m_pPedBeingThreatened == pPed)
	{
		return false;
	}

	//Ensure that the threatening ped is not themselves.
	if(m_pPedBeingThreatening == pPed)
	{
		return false;
	}

	//Ensure the ped is not friendly with the threatening ped.
	if(pPed->GetPedIntelligence()->IsFriendlyWith(*m_pPedBeingThreatening))
	{
		return false;
	}

	//Ensure that the ped is friendly with the ped being threatened.
	if(!pPed->GetPedIntelligence()->IsFriendlyWith(*m_pPedBeingThreatened) &&
		!pPed->GetPedIntelligence()->IsFriendlyWithDueToScenarios(*m_pPedBeingThreatened) &&
		!pPed->GetPedIntelligence()->IsFriendlyWithDueToSameVehicle(*m_pPedBeingThreatened))
	{
		return false;
	}

	//If the threatened ped is a random cop and the threatening ped was wanted but is no longer wanted then ignore the event
	if( m_bThreateningPedWasWanted && m_pPedBeingThreatened->PopTypeIsRandom() && m_pPedBeingThreatened->IsLawEnforcementPed() &&
		!ThreateningPedIsWanted() )
	{
		return false;
	}

	//Possibly ignore due to interior status.
	if (CEvent::CanEventBeIgnoredBecauseOfInteriorStatus(*pPed, m_pPedBeingThreatening, GetEntityOrSourcePosition()))
	{
		return false;
	}

	//Register this as a hostile event with CTaskAgitated and ignore if possible.
	if(CTaskAgitated::ShouldIgnoreEvent(*pPed, *this))
	{
		return false;
	}

	//Ensure the ped is not already running threat response.
	if(pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_THREAT_RESPONSE, true))
	{
		return false;
	}

	//This was added to stop cops from all heading towards the first incident that gets started, in MP.
	if(CWantedIncident::ShouldIgnoreCombatEvent(*pPed, *m_pPedBeingThreatening))
	{
		return false;
	}

	return true;
}

bool CEventRespondedToThreat::ThreateningPedIsWanted() const
{
	CWanted* pThreateningPedWanted = m_pPedBeingThreatening ? m_pPedBeingThreatening->GetPlayerWanted() : NULL;
	if(pThreateningPedWanted && pThreateningPedWanted->GetWantedLevel() > WANTED_CLEAN)
	{
		return true;
	}

	return false;
}

bool CEventRespondedToThreat::CreateResponseTask(CPed* pPed, CEventResponse& response) const
{
	//Create the task.
	CTaskThreatResponse* pTask = CTaskThreatResponse::CreateForTargetDueToEvent(pPed, m_pPedBeingThreatening, *this);
	if(aiVerifyf(pTask, "NULL threat response task returned from CreateForTargetDueToEvent, ped won't have a response."))
	{
		aiAssert(m_pPedBeingThreatened);

		//Find the task running on the threatened ped.
		const CTaskThreatResponse* pTaskThreatResponse = static_cast<CTaskThreatResponse *>(
			m_pPedBeingThreatened->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_THREAT_RESPONSE));
		if(pTaskThreatResponse)
		{
			//Check the threat response.
			switch(pTaskThreatResponse->GetThreatResponse())
			{
				case CTaskThreatResponse::TR_Fight:
				{
					pTask->GetConfigFlags().SetFlag(CTaskThreatResponse::CF_PreferFight);
					break;
				}
				case CTaskThreatResponse::TR_Flee:
				{
					pTask->GetConfigFlags().SetFlag(CTaskThreatResponse::CF_PreferFlee);
					break;
				}
				default:
				{
					break;
				}
			}
		}

		//Set the event response.
		response.SetEventResponse(CEventResponse::EventResponse, pTask);
	}

	return response.GetEventResponse(CEventResponse::EventResponse) != NULL;
}


const char* CEventPlayerDeath::sm_caReactionContexts[] =
{
	  "BUDDY_SEES_MICHAEL_DEATH",  
		"BUDDY_SEES_FRANKLIN_DEATH", 
		"BUDDY_SEES_TREVOR_DEATH"
};

CEventPlayerDeath::CEventPlayerDeath(CPed* pDyingPed)
	: m_pDyingPed(pDyingPed)
	, m_eDeadPedType(PEDTYPE_INVALID)
{
	if (m_pDyingPed)
	{
		if ( m_pDyingPed->GetPedModelInfo()->GetModelNameHash() == MI_PLAYERPED_PLAYER_ZERO.GetName().GetHash() )
		{
			m_eDeadPedType = PEDTYPE_PLAYER_0;
		}
		else if ( m_pDyingPed->GetPedModelInfo()->GetModelNameHash() == MI_PLAYERPED_PLAYER_ONE.GetName().GetHash() )
		{
			m_eDeadPedType = PEDTYPE_PLAYER_1;
		}
		else if ( m_pDyingPed->GetPedModelInfo()->GetModelNameHash() == MI_PLAYERPED_PLAYER_TWO.GetName().GetHash() ) 
		{
			m_eDeadPedType = PEDTYPE_PLAYER_2;	
		}
	}
}

bool CEventPlayerDeath::AffectsPedCore(CPed* pPed) const
{
	if (m_eDeadPedType == PEDTYPE_INVALID)
	{
		return false;
	}
	
	if (!m_pDyingPed || !pPed->GetPedIntelligence()->IsFriendlyWith(*m_pDyingPed))
	{
		return false;
	}

	if (!pPed->GetSpeechAudioEntity() || !pPed->GetSpeechAudioEntity()->DoesContextExistForThisPed(GetReactionContext()))
	{
		return false;
	}

	return true;
}

const char* CEventPlayerDeath::GetReactionContext() const
{
	aiAssert(m_eDeadPedType != PEDTYPE_INVALID);
	if (m_eDeadPedType == PEDTYPE_PLAYER_0)
	{
		return CEventPlayerDeath::sm_caReactionContexts[0];
	}
	else if (m_eDeadPedType == PEDTYPE_PLAYER_1)
	{
		return CEventPlayerDeath::sm_caReactionContexts[1];
	}
	else if (m_eDeadPedType == PEDTYPE_PLAYER_2)
	{
		return CEventPlayerDeath::sm_caReactionContexts[2];
	}
	else 
	{
		return NULL;
	}
}
