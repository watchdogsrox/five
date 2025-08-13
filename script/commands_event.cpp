// File header
#include "script/commands_event.h"

// Rage headers
#include "script/wrapper.h"

// Game headers
#include "Event/Decision/EventDecisionMakerManager.h"
#include "Event/EventShocking.h"
#include "Event/ShockingEvents.h"
#include "Peds/Ped.h"
#include "Peds/PedIntelligence.h"
#include "Script/Handlers/GameScriptResources.h"
#include "Script/Script.h"
#include "Script/Script_Channel.h"
#include "task/Response/Info/AgitatedManager.h"

// Macro to disable optimisations if set
AI_OPTIMISATIONS()
SCRIPT_OPTIMISATIONS()

namespace event_commands
{

void CommandSetDecisionMaker(s32 iPedIndex, s32 iDecisionMakerId)
{
	CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(iPedIndex);

	if(pPed)
	{
#if __DEV
		if(pPed->m_nDEflags.bCheckedForDead)
#endif // __DEV
		{
			if(scriptVerifyf(!pPed->IsPlayer(), "%s: SET_DECISION_MAKER - Can't change a player's decision maker", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
			{
				if(scriptVerifyf(iDecisionMakerId != NULL_IN_SCRIPTING_LANGUAGE, "%s: SET_DECISION_MAKER -  NULL passed for decision maker, or variable not initialised", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
				{
					pPed->GetPedIntelligence()->SetDecisionMakerId(iDecisionMakerId);
				}
			}
		}
	}
}

void CommandSetDecisionMakerToDefault(s32 iPedIndex)
{
	CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(iPedIndex);

	if(pPed)
	{
#if __DEV
		if(pPed->m_nDEflags.bCheckedForDead)
#endif // __DEV
		{
			if(scriptVerifyf(!pPed->IsPlayer(), "%s: SET_DECISION_MAKER_TO_DEFAULT - Can't change a player's decision maker", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
			{
				pPed->SetDefaultDecisionMaker();
			}
		}
	}
}

s32 CommandCopyDecisionMaker(s32 iSourceDecisionMakerId, const char* pNewName)
{
	CScriptResource_DecisionMaker decisionMaker(iSourceDecisionMakerId, pNewName);

	return CTheScripts::GetCurrentGtaScriptHandler()->RegisterScriptResourceAndGetRef(decisionMaker);
}

void CommandDeleteDecisionMaker(s32 iDecisionMakerId)
{
	CTheScripts::GetCurrentGtaScriptHandler()->RemoveScriptResource(CGameScriptResource::SCRIPT_RESOURCE_DECISION_MAKER, iDecisionMakerId);
}

bool CommandDoesDecisionMakerExist(s32 iDecisionMakerId)
{
	return CEventDecisionMakerManager::GetIsDecisionMakerValid(iDecisionMakerId);
}

void CommandAddDecisionMakerEventResponse(
	s32 iDecisionMakerId, s32 iEventType, s32 iTaskType,
	f32 fFriendProbability, f32 fThreatProbability, f32 fPlayerProbability, f32 fOtherProbability,
	bool bInCarFlag, bool bOnFootFlag)
{
	CEventDecisionMaker* pDecisionMaker = CEventDecisionMakerManager::GetDecisionMaker(iDecisionMakerId);
	if(scriptVerifyf(pDecisionMaker, "%s: ADD_DECISION_MAKER_EVENT_RESPONSE - decision maker with Id [%d] doesn't exist", CTheScripts::GetCurrentScriptNameAndProgramCounter(), iDecisionMakerId))
	{
		// Construct decision
		CEventDataResponse_Decision decision;

		// Chance
		decision.DistanceMinSq = -1.0f;
		decision.DistanceMaxSq = -1.0f;
		decision.Chance_SourcePlayer = fPlayerProbability;
		decision.Chance_SourceFriend = fFriendProbability;
		decision.Chance_SourceThreat = fThreatProbability;
		decision.Chance_SourceOther  = fOtherProbability;
		scriptAssertf((fPlayerProbability + fFriendProbability + fThreatProbability + fOtherProbability) > 0, "%s:ADD_PED_DECISION_MAKER_EVENT_RESPONSE - All four probabilities are 0", CTheScripts::GetCurrentScriptNameAndProgramCounter());

		// Flags
		if (bOnFootFlag)
			decision.SetResponseDecisionFlag(CEventResponseDecisionFlags::ValidOnFoot);
		if (bInCarFlag)
			decision.SetResponseDecisionFlag(CEventResponseDecisionFlags::ValidInCar);
		scriptAssertf(bOnFootFlag || bInCarFlag, "%s:ADD_DECISION_MAKER_EVENT_RESPONSE - OnFoot and InCar flags are both FALSE", CTheScripts::GetCurrentScriptNameAndProgramCounter());

		// Response
		decision.TaskRef = iTaskType;
		decision.PostLoadCB();

		// Add to the decision maker
		pDecisionMaker->AddResponse(static_cast<eEventType>(iEventType), decision);
	}
}

void CommandClearDecisionMakerEventResponse(s32 iDecisionMakerId, s32 iEventType)
{
	CEventDecisionMaker* pDecisionMaker = CEventDecisionMakerManager::GetDecisionMaker(iDecisionMakerId);
	if(scriptVerifyf(pDecisionMaker, "%s: CLEAR_DECISION_MAKER_EVENT_RESPONSE - decision maker with Id [%d] doesn't exist", CTheScripts::GetCurrentScriptNameAndProgramCounter(), iDecisionMakerId))
	{
		pDecisionMaker->ClearResponse(static_cast<eEventType>(iEventType));
	}
}

void CommandBlockDecisionMakerEvent(s32 iDecisionMakerId, s32 iEventType)
{
	CEventDecisionMaker* pDecisionMaker = CEventDecisionMakerManager::GetDecisionMaker(iDecisionMakerId);
	if(scriptVerifyf(pDecisionMaker, "%s: BLOCK_DECISION_MAKER_EVENT - decision maker with Id [%d] doesn't exist", CTheScripts::GetCurrentScriptNameAndProgramCounter(), iDecisionMakerId))
	{
		pDecisionMaker->BlockEvent(static_cast<eEventType>(iEventType));
	}
}

void CommandUnblockDecisionMakerEvent(s32 iDecisionMakerId, s32 iEventType)
{
	CEventDecisionMaker* pDecisionMaker = CEventDecisionMakerManager::GetDecisionMaker(iDecisionMakerId);
	if(scriptVerifyf(pDecisionMaker, "%s: UNBLOCK_DECISION_MAKER_EVENT - decision maker with Id [%d] doesn't exist", CTheScripts::GetCurrentScriptNameAndProgramCounter(), iDecisionMakerId))
	{
		pDecisionMaker->UnblockEvent(static_cast<eEventType>(iEventType));
	}
}

// Helper function for creating a shocking event.
CEventShocking* CreateShockingEvent(eEventType eventType, const char* ASSERT_ONLY(cmdName))
{
	CEventShocking* ev = NULL;
	switch(eventType)
	{
		case EVENT_SHOCKING_CAR_ALARM:
			ev = rage_new CEventShockingCarAlarm;
			break;
		case EVENT_SHOCKING_CAR_CHASE:
			ev = rage_new CEventShockingCarChase;
			break;
		case EVENT_SHOCKING_CAR_CRASH:
			ev = rage_new CEventShockingCarCrash;
			break;
		case EVENT_SHOCKING_BICYCLE_CRASH:
			ev = rage_new CEventShockingBicycleCrash;
			break;
		case EVENT_SHOCKING_CAR_PILE_UP:
			ev = rage_new CEventShockingCarPileUp;
			break;
		case EVENT_SHOCKING_CAR_ON_CAR:
			ev = rage_new CEventShockingCarOnCar;
			break;
		case EVENT_SHOCKING_DANGEROUS_ANIMAL:
			ev = rage_new CEventShockingDangerousAnimal;
			break;
		case EVENT_SHOCKING_DEAD_BODY:
			ev = rage_new CEventShockingDeadBody;
			break;
		case EVENT_SHOCKING_DRIVING_ON_PAVEMENT:
			ev = rage_new CEventShockingDrivingOnPavement;
			break;
		case EVENT_SHOCKING_ENGINE_REVVED:
			ev = rage_new CEventShockingEngineRevved;
			break;
		case EVENT_SHOCKING_EXPLOSION:
			ev = rage_new CEventShockingExplosion;
			break;
		case EVENT_SHOCKING_FIRE:
			ev = rage_new CEventShockingFire;
			break;
		case EVENT_SHOCKING_GUN_FIGHT:
			ev = rage_new CEventShockingGunFight;
			break;
		case EVENT_SHOCKING_GUNSHOT_FIRED:
			ev = rage_new CEventShockingGunshotFired;
			break;
		case EVENT_SHOCKING_HELICOPTER_OVERHEAD:
			ev = rage_new CEventShockingHelicopterOverhead;
			break;
		case EVENT_SHOCKING_PARACHUTER_OVERHEAD:
			ev = rage_new CEventShockingParachuterOverhead;
			break;
		case EVENT_SHOCKING_HORN_SOUNDED:
			ev = rage_new CEventShockingHornSounded;
			break;
		case EVENT_SHOCKING_INJURED_PED:
			ev = rage_new CEventShockingInjuredPed;
			break;
		case EVENT_SHOCKING_MAD_DRIVER:
			ev = rage_new CEventShockingMadDriver;
			break;
		case EVENT_SHOCKING_MAD_DRIVER_EXTREME:
			ev = rage_new CEventShockingMadDriverExtreme;
			break;
		case EVENT_SHOCKING_MUGGING:
			ev = rage_new CEventShockingMugging;
			break;
		case EVENT_SHOCKING_NON_VIOLENT_WEAPON_AIMED_AT:
			ev = rage_new CEventShockingNonViolentWeaponAimedAt;
			break;
		case EVENT_SHOCKING_PED_RUN_OVER:
			ev = rage_new CEventShockingPedRunOver;
			break;
		case EVENT_SHOCKING_PED_SHOT:
			ev = rage_new CEventShockingPedShot;
			break;
		case EVENT_SHOCKING_PLANE_FLY_BY:
			ev = rage_new CEventShockingPlaneFlyby;
			break;
		case EVENT_SHOCKING_PROPERTY_DAMAGE:
			ev = rage_new CEventShockingPropertyDamage;
			break;
		case EVENT_SHOCKING_RUNNING_PED:
			ev = rage_new CEventShockingRunningPed;
			break;
		case EVENT_SHOCKING_RUNNING_STAMPEDE:
			ev = rage_new CEventShockingRunningStampede;
			break;
		case EVENT_SHOCKING_SEEN_CAR_STOLEN:
			ev = rage_new CEventShockingSeenCarStolen;
			break;
		case EVENT_SHOCKING_SEEN_CONFRONTATION:
			ev = rage_new CEventShockingSeenConfrontation;
			break;
		case EVENT_SHOCKING_SEEN_GANG_FIGHT:
			ev = rage_new CEventShockingSeenGangFight;
			break;
		case EVENT_SHOCKING_SEEN_INSULT:
			ev = rage_new CEventShockingSeenInsult;
			break;
		case EVENT_SHOCKING_SEEN_MELEE_ACTION:
			ev = rage_new CEventShockingSeenMeleeAction;
			break;
		case EVENT_SHOCKING_SEEN_NICE_CAR:
			ev = rage_new CEventShockingSeenNiceCar;
			break;
		case EVENT_SHOCKING_SEEN_PED_KILLED:
			ev = rage_new CEventShockingSeenPedKilled;
			break;
		case EVENT_SHOCKING_SEEN_VEHICLE_TOWED:
			ev = rage_new CEventShockingVehicleTowed;
			break;
		case EVENT_SHOCKING_SEEN_WEAPON_THREAT:
			ev = rage_new CEventShockingWeaponThreat;
			break;
		case EVENT_SHOCKING_SEEN_WEIRD_PED:
			ev = rage_new CEventShockingWeirdPed;
			break;
		case EVENT_SHOCKING_SEEN_WEIRD_PED_APPROACHING:
			ev = rage_new CEventShockingWeirdPedApproaching;
			break;
		case EVENT_SHOCKING_SIREN:
			ev = rage_new CEventShockingSiren;
			break;
		case EVENT_SHOCKING_STUDIO_BOMB:
			ev = rage_new CEventShockingStudioBomb;
			break;
		case EVENT_SHOCKING_VISIBLE_WEAPON:
			ev = rage_new CEventShockingVisibleWeapon;
			break;
		default:
			scriptAssertf(0, "%s:%s - event may not be a shocking event (%s)",
					CTheScripts::GetCurrentScriptNameAndProgramCounter(),
					cmdName,
					CEventNames::GetEventName(eventType));
			break;
	};

	return ev;
}

int CommandAddShockingEventAtPosition(int eventType, const scrVector &pos, float overrideLifeTime)
{
	wantedDebugf3("CommandAddShockingEventAtPosition eventType[%d]",eventType);
	
	int ret = 0;

	CEventShocking* ev = CreateShockingEvent((eEventType)eventType, "ADD_SHOCKING_EVENT_AT_POSITION");
	if(ev)
	{
		Vec3V posV = pos;
		ev->Set(posV, NULL, NULL);

		ret = CShockingEventsManager::Add(*ev, true, overrideLifeTime);

		delete ev;
	}

	return ret;
}

int CommandAddShockingEventForEntity(int eventType, int entityIndex, float overrideLifeTime)
{
	wantedDebugf3("CommandAddShockingEventForEntity eventType[%d] entityIndex[%d]",eventType,entityIndex);

	int ret = 0;

	CEntity *pEntity = CTheScripts::GetEntityToModifyFromGUID<CEntity>(entityIndex);
	if(pEntity)
	{
		CEventShocking* ev = CreateShockingEvent((eEventType)eventType, "ADD_SHOCKING_EVENT_FOR_ENTITY");
		if(ev)
		{
			const Vec3V posV = pEntity->GetTransform().GetPosition();
			ev->Set(posV, pEntity, NULL);

			ret = CShockingEventsManager::Add(*ev, true, overrideLifeTime);

			delete ev;
		}
	}

	return ret;
}

bool CommandIsShockingEventInSphere(int eventType, const scrVector &pos, float radius)
{
	Vec3V vPos = pos;
	ScalarV vRadiusSquared(rage::square(radius));

	CEventGroupGlobal& global = *GetEventGlobalGroup();
	int num = global.GetNumEvents();
	for(int j = 0; j < num; j++)
	{
		fwEvent* ev = global.GetEventByIndex(j);
		if(!ev || !static_cast<const CEvent*>(ev)->IsShockingEvent())
		{
			continue;
		}
		const CEventShocking& shocking = *static_cast<const CEventShocking*>(ev);
		if(eventType == shocking.GetEventType())
		{
			Vec3V vEventPos = RCC_VEC3V(shocking.GetSourcePos());
			if (IsLessThanAll(DistSquared(vPos, vEventPos), vRadiusSquared))
			{
				return true;
			}
		}
	}

	return false;
}

bool CommandRemoveShockingEvent(int shockingEventId)
{
	bool removed = false;

	CEventGroupGlobal& global = *GetEventGlobalGroup();
	int num = global.GetNumEvents();
	for(int j = 0; j < num; j++)
	{
		fwEvent* ev = global.GetEventByIndex(j);
		if(!ev || !static_cast<const CEvent*>(ev)->IsShockingEvent())
		{
			continue;
		}
		const CEventShocking& shocking = *static_cast<const CEventShocking*>(ev);

		// Check if we've found the event we're looking for.
		// Note: GetCreatedByScript() is here in case a script-created event timed out by itself,
		// and then a code-created one reused the same ID (which would require a wraparound, so it's
		// unlikely anyway). Since there are currently no commands to give scripts the ID numbers
		// of code-created shocking events, the only valid use of this command is for script-created
		// events.
		if(shockingEventId == (int)shocking.GetId()
				&& shocking.GetCreatedByScript())
		{
			global.Remove(*ev);
			j--;
			num--;

			removed = true;
		}
	}

	return removed;
}


void CommandRemoveAllShockingEvents(bool scriptCreatedOnly)
{
	CShockingEventsManager::ClearAllEvents(!scriptCreatedOnly, true);
}


void CommandRemoveAllShockingEventsOfType(int eventType, bool scriptCreatedOnly)
{
	CEventGroupGlobal& global = *GetEventGlobalGroup();
	int num = global.GetNumEvents();
	for(int j = 0; j < num; j++)
	{
		fwEvent* ev = global.GetEventByIndex(j);
		if(!ev || !static_cast<const CEvent*>(ev)->IsShockingEvent())
		{
			continue;
		}
		const CEventShocking& shocking = *static_cast<const CEventShocking*>(ev);
		if(eventType != shocking.GetEventType())
		{
			continue;
		}
		if(scriptCreatedOnly && !shocking.GetCreatedByScript())
		{
			continue;
		}
		global.Remove(*ev);
		j--;
		num--;
	}
}

void CommandRemoveShockingEventSpawnBlockingAreas()
{
	CShockingEventsManager::ClearAllBlockingAreas();
}


bool CommandSetShockingEventPosition(int shockingEventId, const scrVector &pos)
{
	CEventGroupGlobal& global = *GetEventGlobalGroup();
	int num = global.GetNumEvents();
	for(int j = 0; j < num; j++)
	{
		fwEvent* ev = global.GetEventByIndex(j);
		if(!ev || !static_cast<CEvent*>(ev)->IsShockingEvent())
		{
			continue;
		}
		CEventShocking& shocking = *static_cast<CEventShocking*>(ev);
		if(shockingEventId == (int)shocking.GetId()
				&& shocking.GetCreatedByScript())
		{
			Vec3V posV = pos;
			shocking.SetSourcePosition(posV);
			return true;
		}
	}
	return false;
}

void CommandSuppressShockingEventsNextFrame()
{
	CShockingEventsManager::SuppressAllShockingEvents();
}

void CommandSuppressShockingEventTypeNextFrame(int eventType)
{
	CShockingEventsManager::SetSuppressingShockingEvents(true, (eSEventShockLevels)eventType);
}

void CommandSuppressAgitationEventsNextFrame()
{
	CAgitatedManager::GetInstance().SetSuppressAgitationEvents(true);
}

void SetupScriptCommands()
{
	// Decision maker functions
	SCR_REGISTER_SECURE(SET_DECISION_MAKER,0x0ec3a788f361170a,	CommandSetDecisionMaker);
	SCR_REGISTER_UNUSED(SET_DECISION_MAKER_TO_DEFAULT,0x534af051e09b7c9e,	CommandSetDecisionMakerToDefault);
	SCR_REGISTER_UNUSED(COPY_DECISION_MAKER,0x29949f782b0765d3,	CommandCopyDecisionMaker);
	SCR_REGISTER_UNUSED(DELETE_DECISION_MAKER,0x77a0e78655287bbf, CommandDeleteDecisionMaker);
	SCR_REGISTER_UNUSED(DOES_DECISION_MAKER_EXIST,0xa4a6542175ca13dc, CommandDoesDecisionMakerExist);
	SCR_REGISTER_UNUSED(ADD_DECISION_MAKER_EVENT_RESPONSE,0x5a2e778e47bd0d5d, CommandAddDecisionMakerEventResponse);
	SCR_REGISTER_SECURE(CLEAR_DECISION_MAKER_EVENT_RESPONSE,0x5458163cf0744e90, CommandClearDecisionMakerEventResponse);
	SCR_REGISTER_SECURE(BLOCK_DECISION_MAKER_EVENT,0x955b3121f93782b0, CommandBlockDecisionMakerEvent);
	SCR_REGISTER_SECURE(UNBLOCK_DECISION_MAKER_EVENT,0x558a12ae451664d1, CommandUnblockDecisionMakerEvent);
	SCR_REGISTER_SECURE(ADD_SHOCKING_EVENT_AT_POSITION,0xaa8ea67d77d37c61,	CommandAddShockingEventAtPosition);
	SCR_REGISTER_SECURE(ADD_SHOCKING_EVENT_FOR_ENTITY,0x6f3d4a01b7ad6fb1,	CommandAddShockingEventForEntity);
	SCR_REGISTER_SECURE(IS_SHOCKING_EVENT_IN_SPHERE,0x0b0f73fef90fd476,	CommandIsShockingEventInSphere);
	SCR_REGISTER_SECURE(REMOVE_SHOCKING_EVENT,0x527aab28db255a95, CommandRemoveShockingEvent);
	SCR_REGISTER_SECURE(REMOVE_ALL_SHOCKING_EVENTS,0x828750f785b6a54d, CommandRemoveAllShockingEvents);
	SCR_REGISTER_UNUSED(REMOVE_ALL_SHOCKING_EVENTS_OF_TYPE,0x5fcbe5f476de8740, CommandRemoveAllShockingEventsOfType);
	SCR_REGISTER_SECURE(REMOVE_SHOCKING_EVENT_SPAWN_BLOCKING_AREAS,0x95bd5cc1ee27a0b8, CommandRemoveShockingEventSpawnBlockingAreas);
	SCR_REGISTER_UNUSED(SET_SHOCKING_EVENT_POSITION,0x4b12efb735f34dd9,	CommandSetShockingEventPosition);
	SCR_REGISTER_SECURE(SUPPRESS_SHOCKING_EVENTS_NEXT_FRAME,0xaea20bbb896153f2, CommandSuppressShockingEventsNextFrame);
	SCR_REGISTER_SECURE(SUPPRESS_SHOCKING_EVENT_TYPE_NEXT_FRAME,0x50ee268371d6a236, CommandSuppressShockingEventTypeNextFrame);
	SCR_REGISTER_SECURE(SUPPRESS_AGITATION_EVENTS_NEXT_FRAME,0x56b00356b09ab994, CommandSuppressAgitationEventsNextFrame);
}

} // event_commands
