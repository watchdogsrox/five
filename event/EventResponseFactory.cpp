// Class header
#include "Event/EventResponseFactory.h"

// Game headers
#include "ai/ambient/ConditionalAnimManager.h"
#include "Game/Dispatch/Orders.h"
#include "Event/decision/EventDecisionMaker.h"
#include "Event/Event.h"
#include "Event/Events.h"
#include "Event/EventResponse.h"
#include "Event/EventMovement.h"
#include "Event/EventShocking.h"
#include "Network/NetworkInterface.h"
#include "Peds/Ped.h"
#include "Peds/PedCapsule.h"
#include "Peds/PedIntelligence.h"
#include "Task/Combat/TaskCombat.h"
#include "Task/Combat/TaskDamageDeath.h"
#include "Task/Combat/TaskNewCombat.h"
#include "Task/Combat/TaskSharkAttack.h"
#include "Task/Combat/TaskThreatResponse.h"
#include "Task/Default/TaskPlayer.h"
#include "Task/General/TaskBasic.h"
#include "Task/General/TaskSecondary.h"
#include "Task/General/TaskVehicleSecondary.h"
#include "Task/Movement/Jumping/TaskInAir.h"
#include "Task/Movement/Jumping/TaskJump.h"
#include "Task/Movement/TaskFall.h"
#include "Task/Movement/TaskJetpack.h"
#include "Task/Movement/TaskCollisionResponse.h"
#include "Task/Movement/TaskSeekEntity.h"
#include "Task/Response/TaskAgitated.h"
#include "Task/Response/TaskDuckAndCover.h"
#include "Task/Response/TaskFlee.h"
#include "Task/Response/TaskGrowlAndFlee.h"
#include "Task/Response/TaskReactToExplosion.h"
#include "Task/Response/TaskShockingEvents.h"
#include "Task/Service/Police/TaskPolicePatrol.h"
#include "Task/Service/Swat/TaskSwat.h"
#include "Task/System/TaskClassInfo.h"
#include "Task/Vehicle/TaskCar.h"
#include "Task/Vehicle/TaskExitVehicle.h"
#include "Task/Vehicle/TaskVehicleWeapon.h"
#include "Task/Weapons/Gun/TaskGun.h"
#include "Vehicles/Vehicle.h"
#include "Weapons/Info/WeaponInfoManager.h"

AI_OPTIMISATIONS()

u32 CEventResponseFactory::m_uNextNiceCarPictureTimeMS = 0;

void CEventResponseFactory::CreateEventResponse(CPed* pPed, const CEvent* pEvent, CEventResponse& response)
{
	Assert(pPed);
	Assert(pEvent);
	CreateEventResponse(pPed, pEvent, pEvent->GetResponseTaskType(), response);
}

void CEventResponseFactory::CreateEventResponse(CPed* pPed, const CEvent* pEvent, CTaskTypes::eTaskType iTaskType, CEventResponse& response)
{
	Assert(pPed);
	Assert(pEvent);
	int fleeFlags;
	if ( pEvent->HasEditableResponse() )
	{
		fleeFlags = static_cast<const CEventEditableResponse&>(*pEvent).GetFleeFlags();
	}
	else
	{
		fleeFlags = 0;
	}

	switch(iTaskType)
	{
	case CTaskTypes::TASK_NONE:
		break;

	case CTaskTypes::TASK_CROUCH:
		if(!pPed->GetIsCrouching())
		{
			response.SetEventResponse(CEventResponse::EventResponse, rage_new CTaskCrouch(3000));
		}
		break;

	case CTaskTypes::TASK_SCENARIO_FLEE:
		{
			if ( pEvent->GetSourceEntity() )
			{
				response.SetEventResponse(CEventResponse::EventResponse, rage_new CTaskScenarioFlee(pEvent->GetSourceEntity(), fleeFlags));
			}
			// Allow peds to respond to this event for explosions, which don't have a source entity necessarily but may still have an owner.
			else if (pEvent->GetEventType() == EVENT_EXPLOSION)
			{
				const CEventExplosion* pEventExplosion = static_cast<const CEventExplosion*>(pEvent);
				response.SetEventResponse(CEventResponse::EventResponse, rage_new CTaskScenarioFlee(pEventExplosion->GetOwner(), fleeFlags));
			}
		}
		break;

	case CTaskTypes::TASK_SMART_FLEE:
		{
			CPed* pSourcePed = NULL;
			CEntity* pSourceEntity = pEvent->GetSourceEntity();
			// Enable threat response flee from peds and drivers of vehicles.
			if ( pSourceEntity )
			{
				if (pSourceEntity->GetIsTypePed())
				{
					pSourcePed = static_cast<CPed*>(pSourceEntity);
				}
				else if (pSourceEntity->GetIsTypeVehicle())
				{
					pSourcePed = static_cast<CVehicle*>(pSourceEntity)->GetDriver();
				}
			}
			if (pSourcePed)
			{
				CTaskThreatResponse* pCombatTask = CTaskThreatResponse::CreateForTargetDueToEvent(pPed, pSourcePed, *pEvent);
				if (Verifyf(pCombatTask, "NULL threat response task task."))
				{
					pCombatTask->SetThreatResponseOverride(CTaskThreatResponse::TR_Flee);
					pCombatTask->SetFleeFlags(fleeFlags);
					response.SetEventResponse(CEventResponse::EventResponse, pCombatTask);

					if(!pPed->GetPedIntelligence()->IsFriendlyWith(*pSourcePed))
					{
						pPed->GetPedIntelligence()->RegisterThreat(pSourcePed, true);
					}
				}
			}
			else if(static_cast<const CEvent*>(pEvent)->IsShockingEvent())
			{
				// This case is mostly intended for fleeing from script-created shocking events, which may
				// not have an associated entity.

				const CEventShocking& shocking = static_cast<const CEventShocking&>(*pEvent);

				Vec3V posV = shocking.GetEntityOrSourcePosition();

				Assertf(!IsCloseAll(posV, Vec3V(V_ZERO), ScalarV(SMALL_FLOAT)), "Invalid position for shocking event.  Type:  %s  Created by script:  %d.", shocking.GetName().c_str(), shocking.GetCreatedByScript());

				CTaskSmartFlee* pFleeTask = rage_new CTaskSmartFlee(CAITarget(RCC_VECTOR3(posV)));
				pFleeTask->SetFleeFlags( fleeFlags );
				response.SetEventResponse(CEventResponse::EventResponse, pFleeTask);
			}
		}
		break;

	case CTaskTypes::TASK_FLY_AWAY:
		{
			if ( pEvent->GetSourceEntity() )
			{
				response.SetEventResponse(CEventResponse::EventResponse, rage_new CTaskFlyAway(pEvent->GetSourceEntity()));
			}
		}
		break;

	case CTaskTypes::TASK_COWER:
		{
			response.SetEventResponse(CEventResponse::EventResponse, rage_new CTaskCower());
		}
		break;  

	case CTaskTypes::TASK_COMPLEX_TURN_TO_FACE_ENTITY:
		{
			if ( pEvent->GetSourceEntity() )
			{
				response.SetEventResponse(CEventResponse::EventResponse, rage_new CTaskTurnToFaceEntityOrCoord(pEvent->GetSourceEntity()));
			}
		}
		break;

	case CTaskTypes::TASK_HANDS_UP:
		if(CPed* pSourcePed = pEvent->GetSourcePed())
		{
			if(pSourcePed->CanPutHandsUp(fleeFlags))
			{
				response.SetEventResponse(CEventResponse::EventResponse, rage_new CTaskReactToGunAimedAt(pSourcePed));
			}
		}
		break;

	case CTaskTypes::TASK_REACT_TO_GUN_AIMED_AT:	
		if(CPed* pSourcePed = pEvent->GetSourcePed())
		{
			if(!pPed->GetPedIntelligence()->IsFriendlyWith(*pSourcePed))
			{
				CTaskThreatResponse* pCombatTask = CTaskThreatResponse::CreateForTargetDueToEvent(pPed, pSourcePed, *pEvent);
				if (Verifyf(pCombatTask, "NULL threat response task returned from CreateForTargetDueToEvent, ped won't have a response."))
				{
					pPed->GetPedIntelligence()->RegisterThreat(pSourcePed, true);
					response.SetEventResponse(CEventResponse::EventResponse, pCombatTask);
				}
			}
		}
		break;

	case CTaskTypes::TASK_VEHICLE_GUN:
		if(CPed* pSourcePed = pEvent->GetSourcePed())
		{
			if(pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) && pPed->GetMyVehicle() && pPed != pPed->GetMyVehicle()->GetDriver() && 
				pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_IN_VEHICLE_BASIC) && 
				!pPed->GetPedIntelligence()->IsFriendlyWith(*pSourcePed))
			{
				const CWeaponInfo* pWeaponInfo = pPed->GetWeaponManager() ? CWeaponInfoManager::GetInfo<CWeaponInfo>(pPed->GetWeaponManager()->GetEquippedWeaponHash()) : NULL;
				if(pWeaponInfo && pWeaponInfo->GetFireType() == FIRE_TYPE_INSTANT_HIT)
				{
					// Compute the abort range of the drive by
					const float fWeaponRange = pWeaponInfo->GetLockOnRange();
					const float fSenseRange = pPed->GetPedIntelligence()->GetPedPerception().GetSeeingRange() + 5.0f;
					const float fAbortRange = (fWeaponRange > fSenseRange) ? fWeaponRange : fSenseRange;

					// Only start a driveby if distance to target is less than abort range
					if (Dist(pSourcePed->GetTransform().GetPosition(), pPed->GetTransform().GetPosition()).Getf() < fAbortRange)
					{
						const float fShootRateModifier = pPed->GetPedIntelligence()->GetCombatBehaviour().GetShootRateModifier();
						CAITarget target = CAITarget(pSourcePed);
						response.SetEventResponse(CEventResponse::EventResponse, rage_new CTaskVehicleGun(CTaskVehicleGun::Mode_Fire,ATSTRINGHASH("FIRING_PATTERN_BURST_FIRE_DRIVEBY", 0x0d31265f2),&target,fShootRateModifier));

					}
					else
					{
						response.ClearEventResponse(CEventResponse::EventResponse);
					}
				}
			}
		}
		break;

	case CTaskTypes::TASK_COMPLEX_SCREAM_IN_CAR_THEN_LEAVE:
		{
			if ( pPed->GetMyVehicle() && pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ))
			{
				//pPed->NewSay("Scream");
				VehicleEnterExitFlags vehicleFlags;
				vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::DontCloseDoor);
				response.SetEventResponse(CEventResponse::EventResponse, rage_new CTaskExitVehicle(pPed->GetMyVehicle(), vehicleFlags));
			}
		}
		break;

	case CTaskTypes::TASK_SEEK_ENTITY_AIMING:			
		if(CPed* pSourcePed = pEvent->GetSourcePed())
		{
			response.SetEventResponse(CEventResponse::EventResponse, rage_new CTaskSeekEntityAiming(pSourcePed, 10.0f, 5.0f));
		}
		break;

	case CTaskTypes::TASK_COMBAT:
		if (const CPed* pTargetPed = CTaskCombat::GetCombatTargetFromEvent(*pEvent))
		{
			if(taskVerifyf(!pPed->GetPedIntelligence()->IsFriendlyWith(*pTargetPed), 
				"Ped %s with decision maker %s, is mission ped : %u, is trying to respond to event %s with a threat response task", 
				pPed->GetModelName(), pPed->GetPedIntelligence()->GetDecisionMakerName(), pPed->PopTypeIsMission(), pEvent->GetName().c_str()))
			{
				pPed->GetPedIntelligence()->RegisterThreat(pTargetPed, true);

				//Create the task.
				CTaskThreatResponse* pTask = CTaskThreatResponse::CreateForTargetDueToEvent(pPed, pTargetPed, *pEvent);

				if (Verifyf(pTask, "NULL threat response task returned from CreateForTargetDueToEvent, ped won't have an event response."))
				{
					//Set the event response.
					response.SetEventResponse(CEventResponse::EventResponse, pTask);
				}
			}
		}
		else if(static_cast<const CEvent*>(pEvent)->IsShockingEvent())
		{
			// Best we can do is try to flee.
			const CEventShocking& shocking = static_cast<const CEventShocking&>(*pEvent);

			Vec3V posV = shocking.GetEntityOrSourcePosition();
			CTaskSmartFlee* pFleeTask = rage_new CTaskSmartFlee(CAITarget(RCC_VECTOR3(posV)));
			pFleeTask->SetFleeFlags(fleeFlags);
			response.SetEventResponse(CEventResponse::EventResponse, pFleeTask);
		}
		break;

	case CTaskTypes::TASK_THREAT_RESPONSE:
		if(CPed* pSourcePed = pEvent->GetSourcePed())
		{
			bool bProcessThreatResponse = true;

			if (NetworkInterface::IsGameInProgress())
			{
				bProcessThreatResponse = NetworkInterface::AllowCrimeOrThreatResponsePlayerJustResurrected(pSourcePed,pEvent->GetTargetPed());
				wantedDebugf3("CEventResponseFactory::CreateEventResponse pEvent[%p][%s] TASK_THREAT_RESPONSE bProcessThreatResponse[%d]",pEvent,pEvent ? pEvent->GetName().c_str() : "",bProcessThreatResponse);
			}

			if (bProcessThreatResponse)
			{
				//Create the task.
				CTaskThreatResponse* pTask = CTaskThreatResponse::CreateForTargetDueToEvent(pPed, pSourcePed, *pEvent);

				//Set the event response.
				response.SetEventResponse(CEventResponse::EventResponse, pTask);
			}
		}
		else if(static_cast<const CEvent*>(pEvent)->IsShockingEvent())
		{
			// Best we can do is try to flee.
			const CEventShocking& shocking = static_cast<const CEventShocking&>(*pEvent);

			Vec3V posV = shocking.GetEntityOrSourcePosition();
			CTaskSmartFlee* pFleeTask = rage_new CTaskSmartFlee(CAITarget(RCC_VECTOR3(posV)));
			pFleeTask->SetFleeFlags(fleeFlags);
			response.SetEventResponse(CEventResponse::EventResponse, pFleeTask);
		}
		break;

	case CTaskTypes::TASK_EXIT_VEHICLE:
		if(pPed->GetMyVehicle() && pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ))
		{
			VehicleEnterExitFlags vehicleFlags;
			response.SetEventResponse(CEventResponse::EventResponse, rage_new CTaskExitVehicle(pPed->GetMyVehicle(), vehicleFlags));
		}
		break;

	case CTaskTypes::TASK_FALL:

		// Check we're not back on the ground! Or a horse (for now).
		if(!pPed->GetIsStanding())
		{
			// Player birds just take off instead of running the fall task.
			if (pPed->IsAPlayerPed() && pPed->GetPedType() == PEDTYPE_ANIMAL)
			{
				CTaskMotionBase* pTaskMotion = pPed->GetCurrentMotionTask();
				if (pTaskMotion && pTaskMotion->GetTaskType() == CTaskTypes::TASK_ON_FOOT_BIRD)
				{
					pPed->GetMotionData()->SetIsFlyingForwards();
					return;
				}
			}

#if ENABLE_JETPACK
			if(pPed->GetHasJetpackEquipped())
			{
				response.SetEventResponse(CEventResponse::EventResponse, rage_new CTaskJetpack(0));
			}
			else
#endif
			{
				bool bDisableSkydiveTransition;
				bool bCheckForDive;
				bool bForceNMFall;
				if(pEvent->GetEventType() == EVENT_IN_AIR)
				{
					bDisableSkydiveTransition = static_cast<const CEventInAir *>(pEvent)->GetDisableSkydiveTransition();
					bCheckForDive = static_cast<const CEventInAir *>(pEvent)->GetCheckForDive();
					bForceNMFall = static_cast<const CEventInAir *>(pEvent)->GetForceNMFall();
				}
				else
				{
					bDisableSkydiveTransition = false;
					bCheckForDive = false;
					bForceNMFall = false;
				}

				//Generate the flags.
				fwFlags32 uFlags = 0;

				const CTaskJump* pJumpTask = static_cast<const CTaskJump*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_JUMP));
				if(pJumpTask)
				{
					uFlags.SetFlag(FF_ForceJumpFallLeft);
				}
				
				if(bDisableSkydiveTransition)
				{
					uFlags.SetFlag(FF_DisableSkydiveTransition);
				}
				if(bForceNMFall)
				{
					uFlags.SetFlag(FF_ForceHighFallNM);
				}
				if(bCheckForDive)
				{
					uFlags.SetFlag(FF_CheckForDiveOrRagdoll);
				}
				
				//! If no flags specified, try to check for immediate cancellation.
				if(uFlags == 0)
				{
					uFlags.SetFlag(FF_CheckForGroundOnEntry);
				}

				CTaskComplexControlMovement * pCtrlTask;
				pCtrlTask = rage_new CTaskComplexControlMovement( rage_new CTaskMoveInAir(), rage_new CTaskFall(uFlags), CTaskComplexControlMovement::TerminateOnSubtask );
				response.SetEventResponse(CEventResponse::EventResponse, pCtrlTask);
			}
		}
		break;

	case CTaskTypes::TASK_COMPLEX_STUCK_IN_AIR:
		{
			response.SetEventResponse(CEventResponse::EventResponse, rage_new CTaskComplexStuckInAir());
		}
		break;

	case CTaskTypes::TASK_COMPLEX_LEAVE_CAR_AND_FLEE:
		{
			if(pPed->GetMyVehicle() && pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ))
			{
				CTaskSequenceList* pSequence = rage_new CTaskSequenceList();
				CTaskSmartFlee *pFlee = rage_new CTaskSmartFlee(CAITarget(pPed->GetMyVehicle()));
				pFlee->SetFleeFlags(fleeFlags);
				VehicleEnterExitFlags vehicleFlags;
				vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::DontCloseDoor);
				pSequence->AddTask(rage_new CTaskExitVehicle(pPed->GetMyVehicle(), vehicleFlags));
				pSequence->AddTask(pFlee);
				response.SetEventResponse(CEventResponse::EventResponse, rage_new CTaskUseSequence(*pSequence));
			}
		}
		break;

	case CTaskTypes::TASK_DO_NOTHING:
		{
			response.SetEventResponse(CEventResponse::EventResponse, rage_new CTaskDoNothing(1));
		}
		break;

	case CTaskTypes::TASK_COMPLEX_GET_OFF_BOAT:
		{
			response.SetEventResponse(CEventResponse::EventResponse, rage_new CTaskComplexGetOffBoat());
		}
		break;

	case CTaskTypes::TASK_HIT_WALL:
		{
			response.SetEventResponse(CEventResponse::EventResponse, rage_new CTaskHitWall());
		}
		break;

	case CTaskTypes::TASK_COMPLEX_ON_FIRE:
		{
			// Don't allow players in network games to be set on fire
			bool bPlayerInNetworkGame = false;

            if(pPed->IsAPlayerPed() && NetworkInterface::IsGameInProgress())
			{
				bPlayerInNetworkGame = true;
			}

            if(!bPlayerInNetworkGame)
			{
				u32 weaponHash = WEAPONTYPE_FIRE;

				if (pEvent->GetEventType() == EVENT_ON_FIRE)
				{
					const CEventOnFire* eventOnFire = static_cast<const CEventOnFire *>( pEvent );
					weaponHash = eventOnFire->GetWeaponHash();
					if (0 == weaponHash)
					{
						weaponHash = WEAPONTYPE_FIRE;
					}
				}

				response.SetEventResponse(CEventResponse::EventResponse, rage_new CTaskComplexOnFire( weaponHash ));
			}
		}
		break;

	case CTaskTypes::TASK_POLICE_WANTED_RESPONSE:
		{
			if (CPed* pTargetPed = pEvent->GetTargetPed())
			{
				const COrder* pOrder = pPed->GetPedIntelligence()->GetOrder();
				if (pOrder && pOrder->GetIsValid())
				{
					response.SetEventResponse(CEventResponse::EventResponse, rage_new CTaskPoliceWantedResponse(pTargetPed));
				}
				else
				{
					//Create the task.
					CTaskThreatResponse* pTask = CTaskThreatResponse::CreateForTargetDueToEvent(pPed, pTargetPed, *pEvent);

					//Set the event response.
					response.SetEventResponse(CEventResponse::EventResponse, pTask);
				}
			}
		}
		break;

	case CTaskTypes::TASK_SWAT_WANTED_RESPONSE:
		{
			if (CPed* pTargetPed = pEvent->GetTargetPed())
			{
				const COrder* pOrder = pPed->GetPedIntelligence()->GetOrder();
				if (pOrder && pOrder->GetIsValid())
				{
					response.SetEventResponse(CEventResponse::EventResponse, rage_new CTaskSwatWantedResponse(pTargetPed));
				}
				else
				{
					//Create the task.
					CTaskThreatResponse* pTask = CTaskThreatResponse::CreateForTargetDueToEvent(pPed, pTargetPed, *pEvent);

					if (Verifyf(pTask, "NULL threat response task returned from CreateForTargetDueToEvent, ped won't have an event response."))
					{
						//Set the event response.
						response.SetEventResponse(CEventResponse::EventResponse, pTask);
					}
				}	
			}
		}
		break;

	case CTaskTypes::TASK_AMBIENT_LOOK_AT_EVENT:
		{
			if(pEvent)
			{
				response.SetEventResponse(CEventResponse::EventResponse, rage_new CTaskAmbientLookAtEvent(pEvent));

				// This task is meant to run on the secondary task controller.
				// Note: not entirely sure if this should be done here, or if it should always be a property of
				// CEventDataResponseTaskHeadTrack, or if it should be set for each response in 'events.meta'.

				response.SetRunOnSecondary(true);
			}
		}
		break;
	case CTaskTypes::TASK_AGGRESSIVE_RUBBERNECK:
		{
			if (pEvent)
			{
				response.SetEventResponse(CEventResponse::EventResponse, rage_new CTaskAggressiveRubberneck(pEvent));
				response.SetRunOnSecondary(true);
			}
		}
		break;

	case CTaskTypes::TASK_SHOCKING_EVENT_GOTO:
		{
			if(pEvent && static_cast<const CEvent*>(pEvent)->IsShockingEvent())	// TODO: Maybe adapt this for other events if needed.
			{
				response.SetEventResponse(CEventResponse::EventResponse, rage_new CTaskShockingEventGoto(static_cast<const CEventShocking*>(pEvent)));
			}
		}
		break;

	case CTaskTypes::TASK_SHOCKING_EVENT_HURRYAWAY:
		{
			if(pEvent && static_cast<const CEvent*>(pEvent)->IsShockingEvent())	// TODO: Maybe adapt this for other events if needed.
			{
				CTaskShockingEventHurryAway* pHurryAwayTask = rage_new CTaskShockingEventHurryAway(static_cast<const CEventShocking*>(pEvent));
				if (pEvent->GetEventType() == EVENT_SHOCKING_DEAD_BODY)
				{
					pHurryAwayTask->SetForcePavementNotRequired(true);
				}

				response.SetEventResponse(CEventResponse::EventResponse, pHurryAwayTask);
			}
		}
		break;

	case CTaskTypes::TASK_SHOCKING_EVENT_WATCH:
		{
			if(pEvent && static_cast<const CEvent*>(pEvent)->IsShockingEvent())	// TODO: Maybe adapt this for other events if needed.
			{
				response.SetEventResponse(CEventResponse::EventResponse, rage_new CTaskShockingEventWatch(static_cast<const CEventShocking*>(pEvent)));
			}
		}
		break;

	case CTaskTypes::TASK_SHOCKING_POLICE_INVESTIGATE:
		{
			if (pEvent && static_cast<const CEvent*>(pEvent)->IsShockingEvent())
			{
				response.SetEventResponse(CEventResponse::EventResponse, rage_new CTaskShockingPoliceInvestigate(static_cast<const CEventShocking*>(pEvent)));
			}
		}
		break;
		
	case CTaskTypes::TASK_ESCAPE_BLAST:
		{
			//Ensure the event is valid.
			if(pEvent && (pEvent->GetEventType() == EVENT_POTENTIAL_BLAST))
			{
				//Grab the blast event.
				const CEventPotentialBlast* pBlastEvent = static_cast<const CEventPotentialBlast *>(pEvent);

				//Check if the target is valid.
				const CAITarget& rTarget = pBlastEvent->GetTarget();
				if(rTarget.GetIsValid())
				{
					//Check if the target position is valid.
					Vector3 vTargetPosition;
					if(rTarget.GetPosition(vTargetPosition))
					{
						//Set the response.
						response.SetEventResponse(CEventResponse::EventResponse, rage_new CTaskEscapeBlast(
							const_cast<CEntity *>(rTarget.GetEntity()), vTargetPosition, pBlastEvent->GetRadius() + 5.0f, true));
					}
				}
			}
		}
		break;
		
	case CTaskTypes::TASK_AGITATED:
		{
			//Check if the event is valid.
			if(pEvent)
			{
				//Create the task.
				CTaskAgitated* pTask = CTaskAgitated::CreateDueToEvent(pPed, *pEvent);
				if(pTask)
				{
					//Set the response.
					response.SetEventResponse(CEventResponse::EventResponse, pTask);

					//Run the agitated task on the secondary tree.
					response.SetRunOnSecondary(true);
				}
			}
		}
		break;
		
	case CTaskTypes::TASK_REACT_TO_EXPLOSION:
		{
			//Ensure the event is valid.
			if(pEvent && (pEvent->GetEventType() == EVENT_EXPLOSION))
			{
				//Grab the event.
				const CEventExplosion* pExplosionEvent = static_cast<const CEventExplosion *>(pEvent);

				//Set the response.
				response.SetEventResponse(CEventResponse::EventResponse, rage_new CTaskReactToExplosion(
					pExplosionEvent->GetPosition(), pExplosionEvent->GetOwner(), pExplosionEvent->GetRadius()));
			}
		}
		break;

	case CTaskTypes::TASK_ON_FOOT_DUCK_AND_COVER:
		{
			//Ensure the event is valid.
			if (pEvent)
			{
				//Set the response.

				CTaskDuckAndCover* pTask = rage_new CTaskDuckAndCover();
				response.SetEventResponse(CEventResponse::EventResponse, pTask);
			}
		}
		break;
	case CTaskTypes::TASK_SHARK_ATTACK:
		{
			// Ensure the event and event's ped are valid.
			if (pEvent && pEvent->GetTargetPed())
			{
				CTaskSharkAttack* pTask = rage_new CTaskSharkAttack(pEvent->GetTargetPed());
				response.SetEventResponse(CEventResponse::EventResponse, pTask);
			}
		}
		break;
	case CTaskTypes::TASK_SHOCKING_EVENT_REACT_TO_AIRCRAFT:
		{
			//Ensure the event is valid.
			if (pEvent && (pEvent->GetEventType() == EVENT_SHOCKING_HELICOPTER_OVERHEAD || pEvent->GetEventType() == EVENT_SHOCKING_PLANE_FLY_BY))
			{
				//Set the response.

				const CEventShocking* pShockingEvent = static_cast<const CEventShocking *>(pEvent);

				CTaskShockingEventReactToAircraft* pTask = rage_new CTaskShockingEventReactToAircraft(pShockingEvent);
				response.SetEventResponse(CEventResponse::EventResponse, pTask);
			}
		}
		break;
	case CTaskTypes::TASK_SHOCKING_EVENT_REACT:
		{
			//Ensure the event is valid.
			if (pEvent && static_cast<const CEvent*>(pEvent)->IsShockingEvent())
			{
				//Set the response.

				const CEventShocking* pShockingEvent = static_cast<const CEventShocking *>(pEvent);

				CTaskShockingEventReact* pTask = rage_new CTaskShockingEventReact(pShockingEvent);
				response.SetEventResponse(CEventResponse::EventResponse, pTask);
			}
		}
		break;
	case CTaskTypes::TASK_SHOCKING_EVENT_BACK_AWAY:
		{
			//Ensure the event is valid.
			if (pEvent && static_cast<const CEvent*>(pEvent)->IsShockingEvent())
			{
				//Set the response

				const CEventShocking* pShockingEvent = static_cast<const CEventShocking *>(pEvent);
				CTaskShockingEventBackAway* pTask = rage_new CTaskShockingEventBackAway(pShockingEvent, CTaskShockingEventBackAway::GetDefaultBackAwayPositionForPed(*pPed));
				response.SetEventResponse(CEventResponse::EventResponse, pTask);
			}
		}
		break;
	case CTaskTypes::TASK_SHOCKING_EVENT_STOP_AND_STARE:
		{
			//Ensure the event is valid.
			if (pEvent && static_cast<const CEvent*>(pEvent)->IsShockingEvent())
			{

				const CEventShocking* pShockingEvent = static_cast<const CEventShocking*>(pEvent);

				CTaskShockingEventStopAndStare* pTask = rage_new CTaskShockingEventStopAndStare(pShockingEvent);
				response.SetEventResponse(CEventResponse::EventResponse, pTask);
			}
		}
		break;
	case CTaskTypes::TASK_MOVE_ACHIEVE_HEADING:
		{
			//Grab the ped position.
			Vector3 vPedPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
			
			//Grab the event position.
			const Vector3& vEventPosition = pEvent->GetSourcePos();

			//Check if the positions differ.
			if(!vPedPosition.IsClose(vEventPosition, SMALL_FLOAT))
			{
				//Calculate the desired heading.
				float fDesiredHeading = fwAngle::GetRadianAngleBetweenPoints(vEventPosition.x, vEventPosition.y, vPedPosition.x, vPedPosition.y);
				fDesiredHeading = fwAngle::LimitRadianAngle(fDesiredHeading);

				//Create the move task.
				CTask* pMoveTask = rage_new CTaskMoveAchieveHeading(fDesiredHeading);

				//Create the task.
				CTask* pTask = rage_new CTaskComplexControlMovement(pMoveTask);
				response.SetEventResponse(CEventResponse::EventResponse, pTask);
			}
		}
		break;
	case CTaskTypes::TASK_EXHAUSTED_FLEE:
		{
			// Verify the event.
			if (pEvent)
			{
				CEntity* pEntity = pEvent->GetSourceEntity();

				// Check the type of the entity - only supported for vehicles and peds.
				if (pEntity && (pEntity->GetIsTypeVehicle() || pEntity->GetIsTypePed()))
				{
					CTaskExhaustedFlee* pTask = rage_new CTaskExhaustedFlee(pEntity);
					response.SetEventResponse(CEventResponse::EventResponse, pTask);
				}
			}
		}
		break;
	case CTaskTypes::TASK_GROWL_AND_FLEE:
		{
			// Verify the event and the event source entity.
			if (pEvent && pEvent->GetSourceEntity())
			{
				CTaskGrowlAndFlee* pTask = rage_new CTaskGrowlAndFlee(pEvent->GetSourceEntity());
				response.SetEventResponse(CEventResponse::EventResponse, pTask);
			}
		}
		break;
	case CTaskTypes::TASK_WALK_AWAY:
		{
			// Verify the event and the event source entity.
			if (pEvent && pEvent->GetSourceEntity())
			{
				CTaskWalkAway* pTask = rage_new CTaskWalkAway(pEvent->GetSourceEntity());
				response.SetEventResponse(CEventResponse::EventResponse, pTask);
			}
			break;
		}
	case CTaskTypes::TASK_SHOCKING_NICE_CAR_PICTURE:
		{
			// Verify the event and the event source entity.
			if (pEvent && pEvent->GetSourceEntity() && pEvent->GetEventType() == EVENT_SHOCKING_SEEN_NICE_CAR)
			{
				if (!pPed->GetVehiclePedInside())
				{
					const CConditionalAnimsGroup* pConditionalAnimsGroup = CONDITIONALANIMSMGR.GetConditionalAnimsGroup(atHashString("WORLD_HUMAN_TOURIST_MOBILE_CAR",0xE35A2E46));

					if ( AssertVerify(pConditionalAnimsGroup && pConditionalAnimsGroup->GetNumAnims() == 1) ) // Assumption below that there is only one anim
					{
						// Make sure this ped hasn't taken a picture of a nice car before and enough time has passed since the last ped took a picture of a car
						if (!pPed->GetPedIntelligence()->HasTakenNiceCarPicture() && fwTimer::GetTimeInMilliseconds() > m_uNextNiceCarPictureTimeMS)
						{

							CScenarioCondition::sScenarioConditionData conditions;
							conditions.pPed = pPed;

							// Check the conditions on the animation
							if (pConditionalAnimsGroup->GetAnims(0)->GetConditionalProbability(conditions) >= fwRandom::GetRandomNumberInRange(0.f, 100.f) ) // Assumes only one animation
							{
								if (pConditionalAnimsGroup->CheckForMatchingConditionalAnims(conditions))
								{
									CTaskShockingNiceCarPicture *pNiceCarPictureTask = rage_new CTaskShockingNiceCarPicture(static_cast<const CEventShocking*>(pEvent));
									response.SetEventResponse(CEventResponse::EventResponse, pNiceCarPictureTask);
									pPed->GetPedIntelligence()->SetTakenNiceCarPicture(true);
									m_uNextNiceCarPictureTimeMS = fwTimer::GetTimeInMilliseconds() + sm_uMinMSBetweenNiceCarPictures;
								}
							}
						}
					}
				}
				
				// If we decided not to take a picture, set a LookAtEvent [1/4/2013 mdawe]
				if (response.GetEventResponse(CEventResponse::EventResponse) == NULL)
				{
					response.SetEventResponse(CEventResponse::EventResponse, rage_new CTaskAmbientLookAtEvent(pEvent));
					// This task is meant to run on the secondary task controller.
					response.SetRunOnSecondary(true);
				}
			}
		}
		break;
	case CTaskTypes::TASK_SHOCKING_EVENT_THREAT_RESPONSE:
		{
			if(pEvent && pEvent->IsShockingEvent())
			{
				//Create the task.
				CTask* pTask = rage_new CTaskShockingEventThreatResponse(static_cast<const CEventShocking *>(pEvent));

				//Set the response.
				response.SetEventResponse(CEventResponse::EventResponse, pTask);
			}
		}
		break;
	case CTaskTypes::TASK_FRIENDLY_FIRE_NEAR_MISS:
		{
			if ( pEvent && pEvent->GetSourcePed() && (CPedGroups::AreInSameGroup(pPed, pEvent->GetSourcePed()) || pPed->GetPedIntelligence()->IsFriendlyWith(*pEvent->GetSourcePed())) )
			{
				//Someone's fired a gun near us.
				aiAssert(!pPed->GetPedResetFlag(CPED_RESET_FLAG_DisableFriendlyGunReactAudio));

				pPed->NewSay("FRIENDLY_FIRE");
			}
		}
		break;
	case CTaskTypes::TASK_FRIENDLY_AIMED_AT:
		{
			if ( ASSERT_ONLY(CPed* pSourcePed =) pEvent->GetSourcePed() )
			{
				aiAssert( pPed->GetPedIntelligence()->IsFriendlyWith(*pSourcePed) );
				aiAssert( !pPed->GetPedResetFlag(CPED_RESET_FLAG_DisableFriendlyGunReactAudio) );

				// Try to say the AIMED_AT_BY_PLAYER line. If we don't have that line, say a generic insult/frightened response.
				audSpeechAudioEntity *pAudioEntity = pPed->GetSpeechAudioEntity();
				if (pAudioEntity)
				{
					if (pAudioEntity->DoesContextExistForThisPed("AIMED_AT_BY_PLAYER"))
					{
						pPed->NewSay("AIMED_AT_BY_PLAYER"); 
					}
					else 
					{
						pPed->NewSay("FRIENDLY_FIRE");
					}
				}
			}
		}
		break;
	case CTaskTypes::TASK_DEFER_TO_SCENARIO:
		{
			const CScenarioPoint* pPoint = CPed::GetScenarioPoint(*pPed, true);
			if (Verifyf(pPoint, "Expected the ped responding with TASK_DEFER_TO_SCENARIO to have a valid scenario point."))
			{
				CTaskTypes::eTaskType iRealResponseTaskType = (CTaskTypes::eTaskType)PickEventResponseTaskTypeFromFlags(*pPoint);

				if (!Verifyf(iRealResponseTaskType != CTaskTypes::TASK_DEFER_TO_SCENARIO, "Avoided a recursive call to CreateEventResponse when deferring task creation!"))
				{
					return;
				}
				CreateEventResponse(pPed, pEvent, iRealResponseTaskType, response);
			}
			break;
		}
	case CTaskTypes::TASK_PLAYER_DEATH_REACT:
		{
			if (pEvent && pEvent->GetEventType() == EVENT_PLAYER_DEATH)
			{
				const CEventPlayerDeath* pPlayerDeathEvent = static_cast<const CEventPlayerDeath*>(pEvent);
				pPed->NewSay(pPlayerDeathEvent->GetReactionContext());
			}
		}
		break;
	case CTaskTypes::TASK_INVALID_ID:
	default:
		Assertf(false, "CEventResponseFactory: Failed to create Task Response [%d][%s] for Event [%s]", pEvent->GetResponseTaskType(), TASKCLASSINFOMGR.GetTaskName(pEvent->GetResponseTaskType()), pEvent->GetName().c_str());
		break;
	}
}

s32 CEventResponseFactory::PickEventResponseTaskTypeFromFlags(const CScenarioPoint& rPoint)
{
	if (rPoint.IsFlagSet(CScenarioPointFlags::EventsInRadiusTriggerThreatResponse))
	{
		return CTaskTypes::TASK_THREAT_RESPONSE;
	}
	else if (rPoint.IsFlagSet(CScenarioPointFlags::EventsInRadiusTriggerDisputes))
	{
		return CTaskTypes::TASK_AGITATED;
	}
	return CTaskTypes::TASK_INVALID_ID;
}
