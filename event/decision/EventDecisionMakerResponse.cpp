// File header
#include "Event/Decision/EventDecisionMakerResponse.h"

// Rage headers
#include "atl/bitset.h"

// Framework headers
#include "ai/aichannel.h"
#include "fwmaths/Random.h"

// Game headers
#include "Event/Decision/EventDecisionMakerResponse_parser.h"
#include "Event/EventEditable.h"
#include "Event/EventShocking.h"
#include "Event/System/EventDataManager.h"
#include "Modelinfo/ModelSeatInfo.h"
#include "Peds/Ped.h"
#include "Peds/PedIntelligence.h"
#include "task/Combat/TaskSharkAttack.h"
#include "task/Scenario/Info/ScenarioTypes.h"
#include "task/Scenario/Info/ScenarioInfo.h"
#include "task/Scenario/Types/TaskUseScenario.h"
#include "Vehicles/Vehicle.h"

// Macro to disable optimisations if set
AI_OPTIMISATIONS()

//////////////////////////////////////////////////////////////////////////
// CEventDecisionMakerResponse
//////////////////////////////////////////////////////////////////////////


CEventDataResponse_Decision::CEventDataResponse_Decision()
		: TaskType(CTaskTypes::TASK_INVALID_ID)
{
}

void CEventDataResponse_Decision::PostLoadCB()
{
	// Get the task type by looking up the CEventDataResponseTask from the hash in TaskRef.
	int taskType = CEventEditableResponse::FindTaskTypeForResponse(TaskRef.GetHash());	
	aiAssert(taskType >= -0x8000 && taskType <= 0x7fff);
	TaskType = (s16)taskType;
}

CEventDecisionMakerResponse::CEventDecisionMakerResponse() : m_Event(EVENT_NONE)
{
}

void CEventDecisionMakerResponse::MakeDecision(const CPed* pPed, const CEventEditableResponse* pEvent, EventSourceType eventSourceType, CEventResponseTheDecision &theDecision ) const
{
	MakeDecision(pPed, pEvent, eventSourceType, m_Decision, theDecision );
}

void CEventDecisionMakerResponse::MakeDecision(const CPed* pPed, const CEventEditableResponse* pEvent, EventSourceType eventSourceType, const tDecisionArray & rDecisions, CEventResponseTheDecision &theDecision) const
{
	aiFatalAssertf(pPed,					"pPed is NULL");
	aiFatalAssertf(pEvent,					"pEvent is NULL");
	aiFatalAssertf(rDecisions.GetCount(),	"pDecisions is empty");

	if(GetEventType() == pEvent->GetEventType())
	{
		// Storage for return data
		atArray<sDecisionResult> results(rDecisions.GetCount(), rDecisions.GetCount());

		//Printf("MakeDecision %s : %d\n", this->GetName().GetCStr(), rDecisions.GetCount());

		// Accumulate the total chance
		float fTotalChance = 0.0f;

		for(int i = 0; i < rDecisions.GetCount(); i++)
		{
			// Add to the total
			fTotalChance += GetDecisionResult(pPed, rDecisions[i], eventSourceType, results[i], *pEvent);
		}

		// Get a random number
		float fRandom = fwRandom::GetRandomNumberInRange(SMALL_FLOAT, fTotalChance);

		for(s32 i = 0; i < results.GetCount(); i++)
		{
			if(results[i].fChance > 0.0f)
			{
				if(fRandom <= results[i].fChance)
				{
					CTaskTypes::eTaskType type = (CTaskTypes::eTaskType)results[i].iTaskType;
					if(type != CTaskTypes::TASK_INVALID_ID)
					{
						theDecision.SetResponse(this);
						theDecision.SetTaskType(type);
						theDecision.SetFleeFlags(results[i].uFleeFlags);
						theDecision.SetSpeechHash(results[i].SpeechHash);
					}
					return;
				}

				fRandom -= results[i].fChance;
			}
		}
	}

	theDecision.Reset();
}

float CEventDecisionMakerResponse::GetDecisionResult(const CPed* pPed, const CEventDataResponse_Decision& decision, EventSourceType eventSourceType, sDecisionResult& result, const CEvent& rEvent) const
{
	// Initialise the return variable
	result.fChance = 0.0f;

	// Does decision pass conditional checks
	if(((decision.IsResponseDecisionFlagSet(CEventResponseDecisionFlags::ValidInCar)) && pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) && pPed->GetMyVehicle() && !pPed->GetMyVehicle()->InheritsFromHeli() && !pPed->GetMyVehicle()->InheritsFromBike()) || 
	   ((decision.IsResponseDecisionFlagSet(CEventResponseDecisionFlags::ValidInHeli)) && pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) && pPed->GetMyVehicle() && pPed->GetMyVehicle()->InheritsFromHeli()) ||
	   ((decision.IsResponseDecisionFlagSet(CEventResponseDecisionFlags::ValidOnBicycle)) && pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) && pPed->GetMyVehicle() && pPed->GetMyVehicle()->InheritsFromBike()) ||
	   ((decision.IsResponseDecisionFlagSet(CEventResponseDecisionFlags::ValidOnFoot)) && pPed->GetIsOnFoot()))
	{
		//Distance check
		if (decision.DistanceMinSq != -1.0f || decision.DistanceMaxSq != -1.0f )  
		{
			Vector3 vPos = VEC3V_TO_VECTOR3(rEvent.GetEntityOrSourcePosition());
			float fDistance = vPos.Dist2(VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()));
			if(decision.DistanceMinSq != -1.0f  && fDistance < decision.DistanceMinSq)
			{
				return result.fChance;
			}

			if(decision.DistanceMaxSq != -1.0f  && fDistance > decision.DistanceMaxSq)
			{
				return result.fChance;
			}
		}

		if (decision.IsResponseDecisionFlagSet(CEventResponseDecisionFlags::NotValidInComplexScenario))
		{
			CTaskUseScenario* pTaskScenario = static_cast<CTaskUseScenario*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_USE_SCENARIO));
			if (pTaskScenario && !pTaskScenario->GetScenarioInfo().GetIsFlagSet(CScenarioInfoFlags::DoSimpleBlendoutForEvents))
			{
				return result.fChance;
			}
		}

		if (decision.IsResponseDecisionFlagSet(CEventResponseDecisionFlags::ValidOnlyInComplexScenario))
		{
			CTaskUseScenario* pTaskScenario = static_cast<CTaskUseScenario*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_USE_SCENARIO));
			if (!pTaskScenario || pTaskScenario->GetScenarioInfo().GetIsFlagSet(CScenarioInfoFlags::DoSimpleBlendoutForEvents))
			{
				return result.fChance;
			}
		}

		if (decision.IsResponseDecisionFlagSet(CEventResponseDecisionFlags::InvalidInCrosswalk))
		{
			if (pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_BeganCrossingRoad))
			{
				return result.fChance;
			}
		}

		if (decision.IsResponseDecisionFlagSet(CEventResponseDecisionFlags::ValidOnlyInCrosswalk))
		{
			if (!pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_BeganCrossingRoad))
			{
				return result.fChance;
			}
		}

		if (decision.IsResponseDecisionFlagSet(CEventResponseDecisionFlags::InvalidWhenAfraid))
		{
			if (pPed->GetPedIntelligence()->GetPedMotivation().IsAfraid())
			{
				return result.fChance;
			}
		}

		if (decision.IsResponseDecisionFlagSet(CEventResponseDecisionFlags::ValidOnlyForDrivers))
		{
			if (!pPed->GetIsDrivingVehicle())
			{
				return result.fChance;
			}
		}

		if (decision.IsResponseDecisionFlagSet(CEventResponseDecisionFlags::ValidOnlyForPassengersWithDriver))
		{
			if (pPed->GetIsInVehicle() && !pPed->GetIsDrivingVehicle())
			{
				CVehicle* pVehicle = pPed->GetMyVehicle();
				if (!pVehicle->GetDriver())
				{
					// Reacting ped's vehicle did not have a driver.  Invalid.
					return result.fChance;
				}
			}
			else
			{
				// Reacting ped was not a passenger.  Invalid.
				return result.fChance;
			}
		}

		if (decision.IsResponseDecisionFlagSet(CEventResponseDecisionFlags::ValidOnlyForPassengersWithNoDriver))
		{
			if (pPed->GetIsInVehicle() && !pPed->GetIsDrivingVehicle())
			{
				CVehicle* pVehicle = pPed->GetMyVehicle();
				if (pVehicle->GetDriver())
				{
					// Reacting ped's vehicle had a driver.  Invalid.
					return result.fChance;
				}
			}
			else
			{
				// Reacting ped was not a passenger.  Invalid.
				return result.fChance;
			}
		}

		if (decision.IsResponseDecisionFlagSet(CEventResponseDecisionFlags::ValidForTargetInsideVehicle))
		{
			const CPed* pTargetPed = rEvent.GetTargetPed();

			if (pTargetPed)
			{
				CVehicle* pTargetPedVehicle = pTargetPed->GetMyVehicle();
				if (!pTargetPedVehicle || pTargetPedVehicle != pPed->GetMyVehicle())
				{
					// Target ped either was not in a vehicle or was not in the same vehicle as the responding ped.  Invalid.
					return result.fChance;
				}
			}
			else
			{
				// Either no target or target was not a ped.  Invalid.
				return result.fChance;
			}
		}

		if (decision.IsResponseDecisionFlagSet(CEventResponseDecisionFlags::ValidForTargetOutsideVehicle))
		{
			const CPed* pTargetPed = rEvent.GetTargetPed();

			if (pTargetPed)
			{
				CVehicle* pTargetPedVehicle = pTargetPed->GetMyVehicle();
				if (pTargetPedVehicle && pTargetPedVehicle == pPed->GetMyVehicle())
				{
					// Target ped had a vehicle that was the same as the responding ped's vehicle.  Invalid.
					return result.fChance;
				}
			}
			else
			{
				// Either no target or target was not a ped.  Invalid.
				return result.fChance;
			}
		}

		if(decision.IsResponseDecisionFlagSet(CEventResponseDecisionFlags::ValidOnlyIfSourceHasGun))
		{
			const CPed* pSourcePed = rEvent.GetSourcePed();
			if (!pSourcePed || !DoesPedHaveGun(*pSourcePed))
			{
				return result.fChance;
			}
		}

		if (decision.IsResponseDecisionFlagSet(CEventResponseDecisionFlags::ValidOnlyIfSourceDoesNotHaveGun))
		{
			const CPed* pSourcePed = rEvent.GetSourcePed();
			if (pSourcePed && DoesPedHaveGun(*pSourcePed))
			{
				return result.fChance;
			}
		}

		if (decision.IsResponseDecisionFlagSet(CEventResponseDecisionFlags::ValidOnlyIfInsideScenarioRadius) || decision.IsResponseDecisionFlagSet(CEventResponseDecisionFlags::ValidOnlyIfOutsideScenarioRadius))
		{
			//Check if the ped is using a scenario.
			const CTaskUseScenario* pScenarioTask = static_cast<CTaskUseScenario*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_USE_SCENARIO));
			if(pScenarioTask)
			{
				//Check if the scenario point is valid.
				const CScenarioPoint* pScenarioPoint = pScenarioTask->GetScenarioPoint();
				if(pScenarioPoint && (pScenarioPoint->IsFlagSet(CScenarioPointFlags::EventsInRadiusTriggerDisputes) || 
					pScenarioPoint->IsFlagSet(CScenarioPointFlags::EventsInRadiusTriggerThreatResponse)))
				{
					//Check against the radius of the scenario point.
					float fRadiusSq = square(pScenarioPoint->GetRadius());
					ScalarV vDistanceSq = DistSquared(rEvent.GetEntityOrSourcePosition(), pPed->GetTransform().GetPosition());
					if (IsLessThanAll(vDistanceSq, ScalarV(fRadiusSq)))
					{
						// Invalid if the event occurs inside the radius but the response is marked as valid only for outside.
						if (decision.IsResponseDecisionFlagSet(CEventResponseDecisionFlags::ValidOnlyIfOutsideScenarioRadius))
						{
							return result.fChance;
						}
					}
					else
					{
						// Invalid if the event occurs outside the radius but the response is marked as valid only for inside.
						if (decision.IsResponseDecisionFlagSet(CEventResponseDecisionFlags::ValidOnlyIfInsideScenarioRadius))
						{
							return result.fChance;
						}
					}
				}
				else
				{
					// Not valid in either case if there is no radius.
					return result.fChance;
				}
			}
			else
			{
				// Not valid in either case if there is no radius.
				return result.fChance;
			}
		}

		if (decision.IsResponseDecisionFlagSet(CEventResponseDecisionFlags::ValidOnlyIfNoScenarioRadius))
		{
			//Check if the ped is using a scenario.
			const CTaskUseScenario* pScenarioTask = static_cast<CTaskUseScenario*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_USE_SCENARIO));
			if(pScenarioTask)
			{
				//Check if the scenario point is valid.
				const CScenarioPoint* pScenarioPoint = pScenarioTask->GetScenarioPoint();
				if(pScenarioPoint && (pScenarioPoint->IsFlagSet(CScenarioPointFlags::EventsInRadiusTriggerDisputes) ||
					pScenarioPoint->IsFlagSet(CScenarioPointFlags::EventsInRadiusTriggerThreatResponse)))
				{
					//Check against the radius of the scenario point.
					float fRadius = pScenarioPoint->GetRadius();

					if (fRadius > SMALL_FLOAT)
					{
						return result.fChance;
					}
				}
			}
		}

		if (decision.IsResponseDecisionFlagSet(CEventResponseDecisionFlags::InvalidIfHasARider))
		{
			// Check for a horse component.
			const CSeatManager* pSeatManager = pPed->GetSeatManager();
			if (pSeatManager && pSeatManager->CountPedsInSeats() > 0)
			{
				// The ped is being ridden.  Event is invalid.
				return result.fChance;
			}
		}

		if (decision.IsResponseDecisionFlagSet(CEventResponseDecisionFlags::ValidOnlyIfFriendlyWithTarget))
		{
			if (!pPed->GetPedIntelligence()->IsFriendlyWithEntity(rEvent.GetTargetEntity(), true))
			{
				return result.fChance;
			}
		}
		else if(decision.IsResponseDecisionFlagSet(CEventResponseDecisionFlags::InvalidIfFriendlyWithTarget))
		{
			if (pPed->GetPedIntelligence()->IsFriendlyWithEntity(rEvent.GetTargetEntity(), true))
			{
				return result.fChance;
			}
		}

		if (decision.IsResponseDecisionFlagSet(CEventResponseDecisionFlags::InvalidInAnInterior))
		{
			if (pPed->GetIsInInterior())
			{
				return result.fChance;
			}
		}

		if (decision.IsResponseDecisionFlagSet(CEventResponseDecisionFlags::ValidOnlyInAnInterior))
		{
			if (!pPed->GetIsInInterior())
			{
				return result.fChance;
			}
		}

		if (decision.IsResponseDecisionFlagSet(CEventResponseDecisionFlags::ValidOnlyIfSourceIsPlayer))
		{
			const CPed* pSourcePed = rEvent.GetSourcePed();
			if (!pSourcePed || !pSourcePed->IsPlayer())
			{
				return result.fChance;
			}
		}

		if(decision.IsResponseDecisionFlagSet(CEventResponseDecisionFlags::ValidOnlyIfSourceIsThreatening))
		{
			const CPed* pSourcePed = rEvent.GetSourcePed();
			if (!pSourcePed || !pPed->GetPedIntelligence()->IsPedThreatening(*pSourcePed, false))
			{
				return result.fChance;
			}
		}

		if(decision.IsResponseDecisionFlagSet(CEventResponseDecisionFlags::ValidOnlyIfSourceIsNotThreatening))
		{
			const CPed* pSourcePed = rEvent.GetSourcePed();
			if (pSourcePed && pPed->GetPedIntelligence()->IsPedThreatening(*pSourcePed, false))
			{
				return result.fChance;
			}
		}

		if (decision.IsResponseDecisionFlagSet(CEventResponseDecisionFlags::ValidOnlyIfRandom))
		{
			if(!pPed->PopTypeIsRandom())
			{
				return result.fChance;
			}
		}

		if(decision.IsResponseDecisionFlagSet(CEventResponseDecisionFlags::ValidOnlyIfTarget))
		{
			const CPed* pTargetPed = rEvent.GetTargetPed();
			if(!pTargetPed || (pPed != pTargetPed))
			{
				return result.fChance;
			}
		}

		if(decision.IsResponseDecisionFlagSet(CEventResponseDecisionFlags::ValidOnlyIfTargetIsInvalid))
		{
			const CPed* pTargetPed = rEvent.GetTargetPed();
			if(pTargetPed)
			{
				return result.fChance;
			}
		}

		if(decision.IsResponseDecisionFlagSet(CEventResponseDecisionFlags::ValidOnlyIfSource))
		{
			if(!rEvent.GetSourceEntity())
			{
				return result.fChance;
			}
		}

		if(decision.IsResponseDecisionFlagSet(CEventResponseDecisionFlags::ValidOnlyIfSourceIsInvalid))
		{
			if(rEvent.GetSourceEntity())
			{
				return result.fChance;
			}
		}

		if(decision.IsResponseDecisionFlagSet(CEventResponseDecisionFlags::ValidOnlyIfTargetIsInvalidOrFriendly))
		{
			const CPed* pTargetPed = rEvent.GetTargetPed();
			if(pTargetPed && !pPed->GetPedIntelligence()->IsFriendlyWithByAnyMeans(*pTargetPed))
			{
				return result.fChance;
			}
		}

		if(decision.IsResponseDecisionFlagSet(CEventResponseDecisionFlags::ValidOnlyIfTargetIsValidAndNotFriendly))
		{
			const CPed* pTargetPed = rEvent.GetTargetPed();
			if(!pTargetPed || pPed->GetPedIntelligence()->IsFriendlyWithByAnyMeans(*pTargetPed))
			{
				return result.fChance;
			}
		}

		if(decision.IsResponseDecisionFlagSet(CEventResponseDecisionFlags::ValidOnlyIfSourceIsAnAnimal))
		{
			const CPed* pSourcePed = rEvent.GetSourcePed();
			if (!pSourcePed || !CPedType::IsAnimalType(pSourcePed->GetPedType()))
			{
				return result.fChance;
			}
		}

		if(decision.IsResponseDecisionFlagSet(CEventResponseDecisionFlags::InvalidIfSourceIsAnAnimal))
		{
			const CPed* pSourcePed = rEvent.GetSourcePed();
			if (pSourcePed && CPedType::IsAnimalType(pSourcePed->GetPedType()))
			{
				return result.fChance;
			}
		}

		if(decision.IsResponseDecisionFlagSet(CEventResponseDecisionFlags::ValidOnlyForStationaryPeds))
		{
			if (!pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_CowerInsteadOfFlee))
			{
				return result.fChance;
			}
		}

		if(decision.IsResponseDecisionFlagSet(CEventResponseDecisionFlags::InvalidForStationaryPeds))
		{
			if (pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_CowerInsteadOfFlee))
			{
				return result.fChance;
			}
		}

		if(decision.IsResponseDecisionFlagSet(CEventResponseDecisionFlags::InvalidIfSourceEntityIsOtherEntity))
		{
			// This flag only applies to shocking events.  If this is not a shocking event, skip it.
			if (rEvent.IsShockingEvent())
			{
				const CEventShocking& rShockingEvent = static_cast<const CEventShocking&>(rEvent);
				if (rEvent.GetSourceEntity() == rShockingEvent.GetOtherEntity())
				{
					return result.fChance;
				}
			}
		}

		if(decision.IsResponseDecisionFlagSet(CEventResponseDecisionFlags::ValidOnlyIfSourceEntityIsOtherEntity))
		{
			// This flag only applies to shocking events.  If this is not a shocking event, skip it.
			if (rEvent.IsShockingEvent())
			{
				const CEventShocking& rShockingEvent = static_cast<const CEventShocking&>(rEvent);
				if (rEvent.GetSourceEntity() != rShockingEvent.GetOtherEntity())
				{
					return result.fChance;
				}
			}
		}

		if(decision.IsResponseDecisionFlagSet(CEventResponseDecisionFlags::ValidOnlyIfOtherVehicleIsYourVehicle))
		{
			// This flag only applies to shocking events.  If this is not a shocking event, skip it.
			if (rEvent.IsShockingEvent())
			{
				const CEventShocking& rShockingEvent = static_cast<const CEventShocking&>(rEvent);
				if (rShockingEvent.GetOtherEntity() != pPed->GetMyVehicle())
				{
					return result.fChance;
				}
			}
		}

		if(decision.IsResponseDecisionFlagSet(CEventResponseDecisionFlags::InvalidIfOtherVehicleIsYourVehicle))
		{
			// This flag only applies to shocking events.  If this is not a shocking event, skip it.
			if (rEvent.IsShockingEvent())
			{
				const CEventShocking& rShockingEvent = static_cast<const CEventShocking&>(rEvent);
				if (rShockingEvent.GetOtherEntity() == pPed->GetMyVehicle())
				{
					return result.fChance;
				}
			}
		}

		if(decision.IsResponseDecisionFlagSet(CEventResponseDecisionFlags::InvalidIfTargetDoesNotInfluenceWanted))
		{
			CPed* pEventTargetPed = rEvent.GetTargetPed();
			if(pEventTargetPed && pEventTargetPed->GetPedConfigFlag(CPED_CONFIG_FLAG_DontInfluenceWantedLevel))
			{
				return result.fChance;
			}
		}

		if(decision.IsResponseDecisionFlagSet(CEventResponseDecisionFlags::ValidOnlyIfSourcePed))
		{
			CPed* pEventSourcePed = rEvent.GetSourcePed();
			if (!pEventSourcePed)
			{
				return result.fChance;
			}
		}

		if(decision.IsResponseDecisionFlagSet(CEventResponseDecisionFlags::InvalidIfSourcePed))
		{
			CPed* pEventSourcePed = rEvent.GetSourcePed();
			if (pEventSourcePed)
			{
				return result.fChance;
			}
		}

		if(decision.IsResponseDecisionFlagSet(CEventResponseDecisionFlags::InvalidIfThereAreAttackingSharks))
		{
			if (CTaskSharkAttack::SharksArePresent())
			{
				return result.fChance;
			}
		}

		if(decision.IsResponseDecisionFlagSet(CEventResponseDecisionFlags::InvalidIfMissionPedInMP))
		{
			if(NetworkInterface::IsGameInProgress() && pPed->PopTypeIsMission())
			{
				return result.fChance;
			}
		}

		if(decision.IsResponseDecisionFlagSet(CEventResponseDecisionFlags::ValidOnlyIfMissionPedInMP))
		{
			if(!NetworkInterface::IsGameInProgress() || !pPed->PopTypeIsMission())
			{
				return result.fChance;
			}
		}

		switch(eventSourceType)
		{
		case EVENT_SOURCE_PLAYER:
			result.fChance = decision.Chance_SourcePlayer;
			break;
		case EVENT_SOURCE_FRIEND:
			result.fChance = decision.Chance_SourceFriend;
			break;
		case EVENT_SOURCE_THREAT:
			result.fChance = decision.Chance_SourceThreat;
			break;
		default:
			result.fChance = decision.Chance_SourceOther;
			break;
		}

		result.iTaskType = decision.TaskType;
		
		// Copy key flags across to the decision table
		result.SpeechHash.SetHash(decision.SpeechHash.GetHash());

		result.uFleeFlags = decision.EventResponseFleeFlags;
	}

	return result.fChance;
}

bool CEventDecisionMakerResponse::DoesPedHaveGun(const CPed& rPed) const
{
	//Ensure the weapon manager is valid.
	const CPedWeaponManager* pWeaponManager = rPed.GetWeaponManager();
	if(!pWeaponManager)
	{
		return false;
	}

	//Ensure the equipped weapon info is valid.
	const CWeaponInfo* pEquippedWeaponInfo = rPed.GetEquippedWeaponInfo();
	if(!pEquippedWeaponInfo)
	{
		return false;
	}

	return pEquippedWeaponInfo->GetIsGun();
}

//////////////////////////////////////////////////////////////////////////
// CEventDecisionMakerResponseDynamic
//////////////////////////////////////////////////////////////////////////

FW_INSTANTIATE_CLASS_POOL(CEventDecisionMakerResponseDynamic, CEventDecisionMakerResponseDynamic::MAX_STORAGE, atHashString("CEventDecisionMakerResponseDynamic",0x490d814e));

CEventDecisionMakerResponseDynamic::CEventDecisionMakerResponseDynamic(eEventType eventType)
: CEventDecisionMakerResponse()
, m_eventType(eventType)
{
}

CEventDecisionMakerResponseDynamic::CEventDecisionMakerResponseDynamic(const CEventDecisionMakerResponseDynamic& other)
: CEventDecisionMakerResponse()
, m_eventType(other.m_eventType)
{
	for(s32 i = 0; i < other.m_decisions.GetCount(); i++)
	{
		m_decisions.Push(other.m_decisions[i]);
	}
}

void CEventDecisionMakerResponseDynamic::MakeDecision(const CPed* pPed, const CEventEditableResponse* pEvent, EventSourceType eventSourceType, CEventResponseTheDecision &theDecision ) const
{
	CEventDecisionMakerResponse::MakeDecision(pPed, pEvent, eventSourceType, m_decisions, theDecision );
}

bool CEventDecisionMakerResponseDynamic::AddDecision(const CEventDataResponse_Decision& decision)
{
	m_decisions.PushAndGrow(decision);
	return true;
}

void CEventDecisionMakerResponseDynamic::ClearDecisions()
{
	m_decisions.Reset();
}
