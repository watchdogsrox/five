// File header
#include "Task/Response/Info/AgitatedConditions.h"

// Game headers
#include "Peds/ped.h"
#include "Peds/PedIntelligence.h"
#include "Task/Combat/TaskCombatMelee.h"
#include "Task/Default/TaskUnalerted.h"
#include "Task/Response/TaskAgitated.h"
#include "Task/Scenario/Types/TaskUseScenario.h"
#include "task/Weapons/Gun/TaskVehicleDriveBy.h"
#include "vehicles/vehicle.h"

AI_OPTIMISATIONS()

////////////////////////////////////////////////////////////////////////////////

INSTANTIATE_RTTI_CLASS(CAgitatedCondition,0x191F0AC5);
INSTANTIATE_RTTI_CLASS(CAgitatedConditionMulti,0xBC4DD029);
INSTANTIATE_RTTI_CLASS(CAgitatedConditionAnd,0x71F395E8);
INSTANTIATE_RTTI_CLASS(CAgitatedConditionOr,0x3E0E821);
INSTANTIATE_RTTI_CLASS(CAgitatedConditionNot,0xC3982418);
INSTANTIATE_RTTI_CLASS(CAgitatedConditionTimeout,0xCEFF5E04);
INSTANTIATE_RTTI_CLASS(CAgitatedConditionCanCallPolice,0x95B424BB);
INSTANTIATE_RTTI_CLASS(CAgitatedConditionCanFight,0xDB07CE55);
INSTANTIATE_RTTI_CLASS(CAgitatedConditionCanHurryAway,0x890035a6);
INSTANTIATE_RTTI_CLASS(CAgitatedConditionCanStepOutOfVehicle,0xFBE42662);
INSTANTIATE_RTTI_CLASS(CAgitatedConditionCanWalkAway,0x21be3104);
INSTANTIATE_RTTI_CLASS(CAgitatedConditionCheckBraveryFlags,0x40F03408);
INSTANTIATE_RTTI_CLASS(CAgitatedConditionHasBeenHostileFor,0x33595B83);
INSTANTIATE_RTTI_CLASS(CAgitatedConditionHasContext,0x5DECA580);
INSTANTIATE_RTTI_CLASS(CAgitatedConditionHasFriendsNearby,0xECA7E6F4);
INSTANTIATE_RTTI_CLASS(CAgitatedConditionHasLeader,0x19E293C4);
INSTANTIATE_RTTI_CLASS(CAgitatedConditionHasLeaderBeenFightingFor,0x12943952);
INSTANTIATE_RTTI_CLASS(CAgitatedConditionHasVehicle,0xF2B99E0C);
INSTANTIATE_RTTI_CLASS(CAgitatedConditionIsAGunPulled,0x7CCBB3);
INSTANTIATE_RTTI_CLASS(CAgitatedConditionIsAgitatorArmed,0xB65A009D);
INSTANTIATE_RTTI_CLASS(CAgitatedConditionIsAgitatorEnteringVehicle,0x13B77F4E);
INSTANTIATE_RTTI_CLASS(CAgitatedConditionIsAgitatorInOurTerritory,0x43f4eee8);
INSTANTIATE_RTTI_CLASS(CAgitatedConditionIsAgitatorInVehicle,0x24FE87ED);
INSTANTIATE_RTTI_CLASS(CAgitatedConditionIsAgitatorInjured,0xF4BF851);
INSTANTIATE_RTTI_CLASS(CAgitatedConditionIsAgitatorMovingAway,0x62A8A0E0);
INSTANTIATE_RTTI_CLASS(CAgitatedConditionIsAngry,0x23E9D7E4);
INSTANTIATE_RTTI_CLASS(CAgitatedConditionIsArgumentative,0x28F344B3);
INSTANTIATE_RTTI_CLASS(CAgitatedConditionIsAvoiding,0xCAC1ABBB);
INSTANTIATE_RTTI_CLASS(CAgitatedConditionIsBecomingArmed,0x8679AAD2);
INSTANTIATE_RTTI_CLASS(CAgitatedConditionIsBumped,0xA52407FA);
INSTANTIATE_RTTI_CLASS(CAgitatedConditionIsBumpedByVehicle,0xE7B85CD2);
INSTANTIATE_RTTI_CLASS(CAgitatedConditionIsBumpedInVehicle,0x68885222);
INSTANTIATE_RTTI_CLASS(CAgitatedConditionIsConfrontational,0x8FD60625);
INSTANTIATE_RTTI_CLASS(CAgitatedConditionIsConfronting,0x114027B2);
INSTANTIATE_RTTI_CLASS(CAgitatedConditionIsContext,0x88337BB6);
INSTANTIATE_RTTI_CLASS(CAgitatedConditionIsDodged,0x6EA9980C);
INSTANTIATE_RTTI_CLASS(CAgitatedConditionIsDodgedVehicle,0x99DD3BF7);
INSTANTIATE_RTTI_CLASS(CAgitatedConditionIsDrivingVehicle,0xCD19472D);
INSTANTIATE_RTTI_CLASS(CAgitatedConditionIsExitingScenario,0x70A6DE07);
INSTANTIATE_RTTI_CLASS(CAgitatedConditionIsFacing,0x9E0B04C2);
INSTANTIATE_RTTI_CLASS(CAgitatedConditionIsFearful,0x64145E3E);
INSTANTIATE_RTTI_CLASS(CAgitatedConditionIsFighting,0x3884FD16);
INSTANTIATE_RTTI_CLASS(CAgitatedConditionIsFollowing,0xF6B56491);
INSTANTIATE_RTTI_CLASS(CAgitatedConditionIsFleeing,0x2540D3D8);
INSTANTIATE_RTTI_CLASS(CAgitatedConditionIsFlippingOff,0xF488CB0F);
INSTANTIATE_RTTI_CLASS(CAgitatedConditionIsFriendlyTalking,0xacbe8b3c);
INSTANTIATE_RTTI_CLASS(CAgitatedConditionIsGettingUp,0x4B1B0D5E);
INSTANTIATE_RTTI_CLASS(CAgitatedConditionIsGriefing,0x38edbd05);
INSTANTIATE_RTTI_CLASS(CAgitatedConditionIsGunAimedAt,0x499D4FA);
INSTANTIATE_RTTI_CLASS(CAgitatedConditionIsHarassed,0x644367E9);
INSTANTIATE_RTTI_CLASS(CAgitatedConditionIsHash,0x63E6C9F0);
INSTANTIATE_RTTI_CLASS(CAgitatedConditionIsHostile,0xDAC41EE0);
INSTANTIATE_RTTI_CLASS(CAgitatedConditionIsHurryingAway,0x7ad48215);
INSTANTIATE_RTTI_CLASS(CAgitatedConditionIsInjured,0xFD797CF5);
INSTANTIATE_RTTI_CLASS(CAgitatedConditionIsInsulted,0xA366184A);
INSTANTIATE_RTTI_CLASS(CAgitatedConditionIsIntervene,0xBE365D08);
INSTANTIATE_RTTI_CLASS(CAgitatedConditionIsIntimidate,0x2DFF81E);
INSTANTIATE_RTTI_CLASS(CAgitatedConditionIsInVehicle,0x49FECD40);
INSTANTIATE_RTTI_CLASS(CAgitatedConditionIsLastAgitationApplied,0x4DDE11D5);
INSTANTIATE_RTTI_CLASS(CAgitatedConditionIsLawEnforcement,0x24A05314);
INSTANTIATE_RTTI_CLASS(CAgitatedConditionIsLeaderAgitated,0xB7579203);
INSTANTIATE_RTTI_CLASS(CAgitatedConditionIsLeaderFighting,0x4FC159D9);
INSTANTIATE_RTTI_CLASS(CAgitatedConditionIsLeaderInState,0x72C537FC);
INSTANTIATE_RTTI_CLASS(CAgitatedConditionIsLeaderStill,0xC381BD38);
INSTANTIATE_RTTI_CLASS(CAgitatedConditionIsLeaderTalking,0x18742B14);
INSTANTIATE_RTTI_CLASS(CAgitatedConditionIsLeaderUsingResponse,0x90A64BFE);
INSTANTIATE_RTTI_CLASS(CAgitatedConditionIsLoitering,0xED8E5DD9);
INSTANTIATE_RTTI_CLASS(CAgitatedConditionIsMale,0x28F8E187);
INSTANTIATE_RTTI_CLASS(CAgitatedConditionIsOutsideClosestDistance,0x9885E4DB);
INSTANTIATE_RTTI_CLASS(CAgitatedConditionIsOutsideDistance,0xF8D49471);
INSTANTIATE_RTTI_CLASS(CAgitatedConditionIsProvoked,0xC14A1711);
INSTANTIATE_RTTI_CLASS(CAgitatedConditionIsRanting,0x49608F50);
INSTANTIATE_RTTI_CLASS(CAgitatedConditionIsSirenOn,0xB47B6BE8);
INSTANTIATE_RTTI_CLASS(CAgitatedConditionIsStanding,0x2C9141B8);
INSTANTIATE_RTTI_CLASS(CAgitatedConditionIsTalking,0x25AFA45);
INSTANTIATE_RTTI_CLASS(CAgitatedConditionIsTargetDoingAMeleeMove,0x1556d029);
INSTANTIATE_RTTI_CLASS(CAgitatedConditionIsWandering,0xF60DB78);
INSTANTIATE_RTTI_CLASS(CAgitatedConditionIsUsingScenario,0xB8FBE43E);
INSTANTIATE_RTTI_CLASS(CAgitatedConditionIsTerritoryIntruded,0x33E0983C);
INSTANTIATE_RTTI_CLASS(CAgitatedConditionIntruderLeft,0x147D2B62);
INSTANTIATE_RTTI_CLASS(CAgitatedConditionIsUsingTerritoryScenario,0x3EDB8EFD);
INSTANTIATE_RTTI_CLASS(CAgitatedConditionIsPlayingAmbientsInScenario,0x42A8B554);
INSTANTIATE_RTTI_CLASS(CAgitatedConditionIsReadyForScenarioResponse,0xB3F22B34);
INSTANTIATE_RTTI_CLASS(CAgitatedConditionIsUsingRagdoll,0x2782CA19);
INSTANTIATE_RTTI_CLASS(CAgitatedConditionRandom,0x76525E1B);
INSTANTIATE_RTTI_CLASS(CAgitatedConditionWasLeaderHit, 0x137fb920);
INSTANTIATE_RTTI_CLASS(CAgitatedConditionWasRecentlyBumpedWhenStill,0x42fa3ac7);
INSTANTIATE_RTTI_CLASS(CAgitatedConditionWasUsingTerritorialScenario,0x4F6E1A);
INSTANTIATE_RTTI_CLASS(CAgitatedConditionHasPavement, 0x8A343DB5)
INSTANTIATE_RTTI_CLASS(CAgitatedConditionIsCallingPolice, 0xB2BBE0D1)
INSTANTIATE_RTTI_CLASS(CAgitatedConditionIsSwimming, 0x2AA4E24D)

////////////////////////////////////////////////////////////////////////////////

CAgitatedConditionMulti::CAgitatedConditionMulti()
: CAgitatedCondition()
, m_Conditions()
{
}

CAgitatedConditionMulti::~CAgitatedConditionMulti()
{
	//Free the conditions.
	for(s32 i = 0; i < m_Conditions.GetCount(); i++)
	{
		delete m_Conditions[i];
	}
}

////////////////////////////////////////////////////////////////////////////////

bool CAgitatedConditionAnd::Check(const CAgitatedConditionContext& aContext) const
{
	for(s32 i = 0; i < m_Conditions.GetCount(); i++)
	{
		if(!m_Conditions[i]->Check(aContext))
		{
			return false;
		}
	}
	
	return true;
}

////////////////////////////////////////////////////////////////////////////////

bool CAgitatedConditionOr::Check(const CAgitatedConditionContext& aContext) const
{
	for(s32 i = 0; i < m_Conditions.GetCount(); i++)
	{
		if(m_Conditions[i]->Check(aContext))
		{
			return true;
		}
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

CAgitatedConditionNot::CAgitatedConditionNot()
: m_Condition(NULL)
{
}

CAgitatedConditionNot::~CAgitatedConditionNot()
{
	delete m_Condition;
}

bool CAgitatedConditionNot::Check(const CAgitatedConditionContext& aContext) const
{
	if(!m_Condition)
	{
		return false;
	}

	return !m_Condition->Check(aContext);
}

////////////////////////////////////////////////////////////////////////////////

bool CAgitatedConditionTimeout::Check(const CAgitatedConditionContext& aContext) const
{
	//Generate the time.
	float fTime = m_Time;
	if(fTime <= 0.0f)
	{
		//Grab the seed for the ped.
		u16 uSeed = aContext.m_Task->GetPed()->GetRandomSeed();

		//Set the time.
		float fValue = (float)uSeed / (float)RAND_MAX_16;
		fTime = Lerp(fValue, m_MinTime, m_MaxTime);
	}

	return (aContext.m_Task->GetTimeSpentWaitingInState() > fTime);
}

////////////////////////////////////////////////////////////////////////////////

bool CAgitatedConditionCanCallPolice::Check(const CAgitatedConditionContext& aContext) const
{
	return (aContext.m_Task->CanCallPolice());
}

////////////////////////////////////////////////////////////////////////////////

bool CAgitatedConditionCanFight::Check(const CAgitatedConditionContext& aContext) const
{
	return (aContext.m_Task->CanFight());
}

////////////////////////////////////////////////////////////////////////////////

bool CAgitatedConditionCanHurryAway::Check(const CAgitatedConditionContext& aContext) const
{
	return (aContext.m_Task->CanHurryAway());
}

////////////////////////////////////////////////////////////////////////////////

bool CAgitatedConditionCanStepOutOfVehicle::Check(const CAgitatedConditionContext& aContext) const
{
	//Ensure the vehicle is valid.
	const CVehicle* pVehicle = aContext.m_Task->GetPed()->GetVehiclePedInside();
	if(!aiVerifyf(pVehicle, "The vehicle is invalid."))
	{
		return false;
	}
	
	return pVehicle->CanPedStepOutCar(NULL, false);
}

////////////////////////////////////////////////////////////////////////////////

bool CAgitatedConditionCanWalkAway::Check(const CAgitatedConditionContext& aContext) const
{
	return (aContext.m_Task->CanWalkAway());
}

////////////////////////////////////////////////////////////////////////////////

bool CAgitatedConditionCheckBraveryFlags::Check(const CAgitatedConditionContext& aContext) const
{
	return (aContext.m_Task->GetPed()->CheckBraveryFlags(m_Flags));
}

////////////////////////////////////////////////////////////////////////////////

bool CAgitatedConditionHasBeenHostileFor::Check(const CAgitatedConditionContext& aContext) const
{
	return (aContext.m_Task->HasBeenHostileFor(m_MinTime, m_MaxTime));
}

////////////////////////////////////////////////////////////////////////////////

bool CAgitatedConditionHasContext::Check(const CAgitatedConditionContext& aContext) const
{
	return (aContext.m_Task->GetPed()->GetSpeechAudioEntity() &&
		aContext.m_Task->GetPed()->GetSpeechAudioEntity()->DoesContextExistForThisPed(m_Context));
}

////////////////////////////////////////////////////////////////////////////////

bool CAgitatedConditionHasFriendsNearby::Check(const CAgitatedConditionContext& aContext) const
{
	return (aContext.m_Task->HasFriendsNearby(m_Min));
}

////////////////////////////////////////////////////////////////////////////////

bool CAgitatedConditionHasLeader::Check(const CAgitatedConditionContext& aContext) const
{
	return (aContext.m_Task->HasLeader());
}

////////////////////////////////////////////////////////////////////////////////

bool CAgitatedConditionHasLeaderBeenFightingFor::Check(const CAgitatedConditionContext& aContext) const
{
	return (aContext.m_Task->HasLeaderBeenFightingFor(m_MinTime, m_MaxTime));
}

////////////////////////////////////////////////////////////////////////////////

bool CAgitatedConditionHasVehicle::Check(const CAgitatedConditionContext& aContext) const
{
	return (aContext.m_Task->GetPed()->GetMyVehicle() != NULL);
}

////////////////////////////////////////////////////////////////////////////////

bool CAgitatedConditionIsAGunPulled::Check(const CAgitatedConditionContext& aContext) const
{
	return (aContext.m_Task->IsAGunPulled());
}

////////////////////////////////////////////////////////////////////////////////

bool CAgitatedConditionIsAgitatorArmed::Check(const CAgitatedConditionContext& aContext) const
{
	return (aContext.m_Task->IsAgitatorArmed());
}

////////////////////////////////////////////////////////////////////////////////

bool CAgitatedConditionIsAgitatorEnteringVehicle::Check(const CAgitatedConditionContext& aContext) const
{
	return (aContext.m_Task->IsAgitatorEnteringVehicle());
}

////////////////////////////////////////////////////////////////////////////////

bool CAgitatedConditionIsAgitatorInOurTerritory::Check(const CAgitatedConditionContext& aContext) const
{
	return (aContext.m_Task->IsAgitatorInOurTerritory());
}

////////////////////////////////////////////////////////////////////////////////

bool CAgitatedConditionIsAgitatorInVehicle::Check(const CAgitatedConditionContext& aContext) const
{
	return (aContext.m_Task->GetAgitator()->GetIsInVehicle());
}

////////////////////////////////////////////////////////////////////////////////

bool CAgitatedConditionIsAgitatorInjured::Check(const CAgitatedConditionContext& aContext) const
{
	return (aContext.m_Task->GetAgitator()->IsInjured());
}

////////////////////////////////////////////////////////////////////////////////

bool CAgitatedConditionIsAgitatorMovingAway::Check(const CAgitatedConditionContext& aContext) const
{
	return (aContext.m_Task->IsAgitatorMovingAway(m_Dot));
}

////////////////////////////////////////////////////////////////////////////////

bool CAgitatedConditionIsAngry::Check(const CAgitatedConditionContext& aContext) const
{
	return (aContext.m_Task->IsAngry(m_Threshold));
}

////////////////////////////////////////////////////////////////////////////////

bool CAgitatedConditionIsArgumentative::Check(const CAgitatedConditionContext& aContext) const
{
	return (aContext.m_Task->IsArgumentative());
}

////////////////////////////////////////////////////////////////////////////////

bool CAgitatedConditionIsAvoiding::Check(const CAgitatedConditionContext& aContext) const
{
	return (aContext.m_Task->IsAvoiding());
}

////////////////////////////////////////////////////////////////////////////////

bool CAgitatedConditionIsBecomingArmed::Check(const CAgitatedConditionContext& aContext) const
{
	return (aContext.m_Task->IsBecomingArmed());
}

////////////////////////////////////////////////////////////////////////////////

bool CAgitatedConditionIsBumped::Check(const CAgitatedConditionContext& aContext) const
{
	return (aContext.m_Task->IsBumped());
}

////////////////////////////////////////////////////////////////////////////////

bool CAgitatedConditionIsBumpedByVehicle::Check(const CAgitatedConditionContext& aContext) const
{
	return (aContext.m_Task->IsBumpedByVehicle());
}

////////////////////////////////////////////////////////////////////////////////

bool CAgitatedConditionIsBumpedInVehicle::Check(const CAgitatedConditionContext& aContext) const
{
	return (aContext.m_Task->IsBumpedInVehicle());
}

////////////////////////////////////////////////////////////////////////////////

bool CAgitatedConditionIsConfrontational::Check(const CAgitatedConditionContext& aContext) const
{
	return (aContext.m_Task->IsConfrontational());
}

////////////////////////////////////////////////////////////////////////////////

bool CAgitatedConditionIsConfronting::Check(const CAgitatedConditionContext& aContext) const
{
	// Not using queriable interface because later one we will be grabbing the task via FindTaskActiveByType
	// Want to avoid circumstances where it QueriableInterface returns true but the task is not found by the function below.
	return aContext.m_Task->GetPed()->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_CONFRONT) != NULL;
}

////////////////////////////////////////////////////////////////////////////////

bool CAgitatedConditionIsContext::Check(const CAgitatedConditionContext& aContext) const
{
	return (aContext.m_Task->GetContext() == m_Context);
}

////////////////////////////////////////////////////////////////////////////////

bool CAgitatedConditionIsDodged::Check(const CAgitatedConditionContext& aContext) const
{
	return (aContext.m_Task->IsDodged());
}

////////////////////////////////////////////////////////////////////////////////

bool CAgitatedConditionIsDodgedVehicle::Check(const CAgitatedConditionContext& aContext) const
{
	return (aContext.m_Task->IsDodgedVehicle());
}

////////////////////////////////////////////////////////////////////////////////

bool CAgitatedConditionIsDrivingVehicle::Check(const CAgitatedConditionContext& aContext) const
{
	return (aContext.m_Task->GetPed()->GetIsDrivingVehicle());
}

////////////////////////////////////////////////////////////////////////////////

bool CAgitatedConditionIsExitingScenario::Check(const CAgitatedConditionContext& aContext) const
{
	CTaskUseScenario* pScenarioTask = 
		static_cast<CTaskUseScenario*>(aContext.m_Task->GetPed()->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_USE_SCENARIO));
	if (pScenarioTask)
	{
		return (pScenarioTask->HasTriggeredExit());
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CAgitatedConditionIsFacing::Check(const CAgitatedConditionContext& aContext) const
{
	return (aContext.m_Task->IsFacing());
}

////////////////////////////////////////////////////////////////////////////////

bool CAgitatedConditionIsFearful::Check(const CAgitatedConditionContext& aContext) const
{
	return (aContext.m_Task->IsFearful(m_Threshold));
}

////////////////////////////////////////////////////////////////////////////////

bool CAgitatedConditionIsFighting::Check(const CAgitatedConditionContext& aContext) const
{
	return aContext.m_Task->GetPed()->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_THREAT_RESPONSE);
}

////////////////////////////////////////////////////////////////////////////////

bool CAgitatedConditionIsFollowing::Check(const CAgitatedConditionContext& aContext) const
{
	return (aContext.m_Task->IsFollowing());
}

////////////////////////////////////////////////////////////////////////////////

bool CAgitatedConditionIsFleeing::Check(const CAgitatedConditionContext& aContext) const
{
	// Not using queriable interface because later one we will be grabbing the task via FindTaskActiveByType
	// Want to avoid circumstances where it QueriableInterface returns true but the task is not found by the function below.
	if (aContext.m_Task->GetPed()->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_SMART_FLEE) != NULL)
	{
		return true;
	}
	else if (aContext.m_Task->GetPed()->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_REACT_AND_FLEE) != NULL)
	{
		return true;
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CAgitatedConditionIsFlippingOff::Check(const CAgitatedConditionContext& aContext) const
{
	return (aContext.m_Task->IsFlippingOff());
}

////////////////////////////////////////////////////////////////////////////////

bool CAgitatedConditionIsFriendlyTalking::Check(const CAgitatedConditionContext& aContext) const
{
	return (aContext.m_Task->IsFriendlyTalking());
}

////////////////////////////////////////////////////////////////////////////////

bool CAgitatedConditionIsGettingUp::Check(const CAgitatedConditionContext& aContext) const
{
	return (aContext.m_Task->GetPed()->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_GET_UP) != NULL);
}

////////////////////////////////////////////////////////////////////////////////

bool CAgitatedConditionIsGriefing::Check(const CAgitatedConditionContext& aContext) const
{
	return (aContext.m_Task->IsGriefing());
}

////////////////////////////////////////////////////////////////////////////////

bool CAgitatedConditionIsGunAimedAt::Check(const CAgitatedConditionContext& aContext) const
{
	return (aContext.m_Task->IsGunAimedAt());
}

////////////////////////////////////////////////////////////////////////////////

bool CAgitatedConditionIsHarassed::Check(const CAgitatedConditionContext& aContext) const
{
	return (aContext.m_Task->IsHarassed());
}

////////////////////////////////////////////////////////////////////////////////

bool CAgitatedConditionIsHash::Check(const CAgitatedConditionContext& aContext) const
{
	return (aContext.m_Task->IsHash(m_Value));
}

////////////////////////////////////////////////////////////////////////////////

bool CAgitatedConditionIsHostile::Check(const CAgitatedConditionContext& aContext) const
{
	return (aContext.m_Task->IsHostile());
}

////////////////////////////////////////////////////////////////////////////////

bool CAgitatedConditionIsHurryingAway::Check(const CAgitatedConditionContext& aContext) const
{
	return (aContext.m_Task->IsHurryingAway());
}

////////////////////////////////////////////////////////////////////////////////

bool CAgitatedConditionIsInjured::Check(const CAgitatedConditionContext& aContext) const
{
	return (aContext.m_Task->IsInjured());
}

////////////////////////////////////////////////////////////////////////////////

bool CAgitatedConditionIsInsulted::Check(const CAgitatedConditionContext& aContext) const
{
	return (aContext.m_Task->IsInsulted());
}

////////////////////////////////////////////////////////////////////////////////

bool CAgitatedConditionIsIntervene::Check(const CAgitatedConditionContext& aContext) const
{
	return (aContext.m_Task->IsIntervene());
}

////////////////////////////////////////////////////////////////////////////////

bool CAgitatedConditionIsIntimidate::Check(const CAgitatedConditionContext& aContext) const
{
	return (aContext.m_Task->IsIntimidate());
}

////////////////////////////////////////////////////////////////////////////////

bool CAgitatedConditionIsInVehicle::Check(const CAgitatedConditionContext& aContext) const
{
	return (aContext.m_Task->GetPed()->GetIsInVehicle());
}

////////////////////////////////////////////////////////////////////////////////

bool CAgitatedConditionIsLastAgitationApplied::Check(const CAgitatedConditionContext& aContext) const
{
	return (aContext.m_Task->IsLastAgitationApplied(m_Type));
}

////////////////////////////////////////////////////////////////////////////////

bool CAgitatedConditionIsLawEnforcement::Check(const CAgitatedConditionContext& aContext) const
{
	return (aContext.m_Task->GetPed()->IsLawEnforcementPed());
}

////////////////////////////////////////////////////////////////////////////////

bool CAgitatedConditionIsLeaderAgitated::Check(const CAgitatedConditionContext& aContext) const
{
	//Ensure the leader is valid.
	const CPed* pLeader = aContext.m_Task->GetLeader();
	if(!pLeader)
	{
		return false;
	}

	//Ensure the leader is agitated.
	if(!pLeader->GetPedConfigFlag(CPED_CONFIG_FLAG_IsAgitated))
	{
		return false;
	}

	return true;
}

////////////////////////////////////////////////////////////////////////////////

bool CAgitatedConditionIsLeaderFighting::Check(const CAgitatedConditionContext& aContext) const
{
	//Ensure the leader is valid.
	const CPed* pLeader = aContext.m_Task->GetLeader();
	if(!pLeader)
	{
		return false;
	}

	return (pLeader->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_COMBAT, true));
}

////////////////////////////////////////////////////////////////////////////////

bool CAgitatedConditionIsLeaderInState::Check(const CAgitatedConditionContext& aContext) const
{
	//Ensure the leader is valid.
	const CPed* pLeader = aContext.m_Task->GetLeader();
	if(!pLeader)
	{
		return false;
	}

	//Ensure the task is valid.
	CTaskAgitated* pTask = CTaskAgitated::FindAgitatedTaskForPed(*pLeader);
	if(!pTask)
	{
		return false;
	}

	//Ensure the leader is in the state.
	if(!pTask->IsInState(m_State))
	{
		return false;
	}

	return true;
}

////////////////////////////////////////////////////////////////////////////////

bool CAgitatedConditionIsLeaderStill::Check(const CAgitatedConditionContext& aContext) const
{
	//Ensure the leader is valid.
	const CPed* pLeader = aContext.m_Task->GetLeader();
	if(!pLeader)
	{
		return false;
	}

	//Ensure the leader is still.
	if(!pLeader->GetMotionData()->GetIsStill())
	{
		return false;
	}

	return true;
}

////////////////////////////////////////////////////////////////////////////////

bool CAgitatedConditionIsLeaderTalking::Check(const CAgitatedConditionContext& aContext) const
{
	//Ensure the leader is valid.
	const CPed* pLeader = aContext.m_Task->GetLeader();
	if(!pLeader)
	{
		return false;
	}

	//Ensure the task is valid.
	const CTaskAgitated* pTask = CTaskAgitated::FindAgitatedTaskForPed(*pLeader);
	if(!pTask)
	{
		return false;
	}

	//Ensure the leader is talking.
	if(!pTask->IsTalking())
	{
		return false;
	}

	return true;
}

////////////////////////////////////////////////////////////////////////////////

bool CAgitatedConditionIsLeaderUsingResponse::Check(const CAgitatedConditionContext& aContext) const
{
	return (aContext.m_Task->IsLeaderUsingResponse(m_Name));
}

////////////////////////////////////////////////////////////////////////////////

bool CAgitatedConditionIsLoitering::Check(const CAgitatedConditionContext& aContext) const
{
	return (aContext.m_Task->IsLoitering());
}

////////////////////////////////////////////////////////////////////////////////

bool CAgitatedConditionIsMale::Check(const CAgitatedConditionContext& aContext) const
{
	return (aContext.m_Task->GetPed()->IsMale());
}

////////////////////////////////////////////////////////////////////////////////

bool CAgitatedConditionIsOutsideClosestDistance::Check(const CAgitatedConditionContext& aContext) const
{
	return (aContext.m_Task->IsOutsideClosestDistance(m_Distance));
}

////////////////////////////////////////////////////////////////////////////////

bool CAgitatedConditionIsOutsideDistance::Check(const CAgitatedConditionContext& aContext) const
{
	return (aContext.m_Task->IsOutsideDistance(m_Distance));
}

////////////////////////////////////////////////////////////////////////////////

bool CAgitatedConditionIsProvoked::Check(const CAgitatedConditionContext& aContext) const
{
	return (aContext.m_Task->IsProvoked(m_IgnoreHostility));
}

////////////////////////////////////////////////////////////////////////////////

bool CAgitatedConditionIsRanting::Check(const CAgitatedConditionContext& aContext) const
{
	return (aContext.m_Task->IsRanting());
}

////////////////////////////////////////////////////////////////////////////////

bool CAgitatedConditionIsSirenOn::Check(const CAgitatedConditionContext& aContext) const
{
	return (aContext.m_Task->IsSirenOn());
}

////////////////////////////////////////////////////////////////////////////////

bool CAgitatedConditionIsStanding::Check(const CAgitatedConditionContext& aContext) const
{
	return (aContext.m_Task->GetPed()->GetIsStanding());
}

////////////////////////////////////////////////////////////////////////////////

bool CAgitatedConditionIsTalking::Check(const CAgitatedConditionContext& aContext) const
{
	return (aContext.m_Task->IsTalking());
}

////////////////////////////////////////////////////////////////////////////////

bool CAgitatedConditionIsTargetDoingAMeleeMove::Check(const CAgitatedConditionContext& aContext) const
{
	const CPed* pTarget = aContext.m_Task->GetAgitator();

	if (pTarget)
	{
		const CTaskMelee* pTaskMelee = static_cast<const CTaskMelee*>(pTarget->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_MELEE));
		if (pTaskMelee)
		{
			return pTaskMelee->IsCurrentlyDoingAMove();
		}
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CAgitatedConditionIsWandering::Check(const CAgitatedConditionContext& aContext) const
{
	// Not using queriable interface because later one we will be grabbing the task via FindTaskActiveByType
	// Want to avoid circumstances where it QueriableInterface returns true but the task is not found by the function below.
	return aContext.m_Task->GetPed()->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_WANDER) != NULL;
}

////////////////////////////////////////////////////////////////////////////////

bool CAgitatedConditionIsUsingScenario::Check(const CAgitatedConditionContext& aContext) const
{
	// Not using queriable interface because later one we will be grabbing the task via FindTaskActiveByType
	// Want to avoid circumstances where it QueriableInterface returns true but the task is not found by the function below.
	return aContext.m_Task->GetPed()->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_USE_SCENARIO) != NULL;
}

////////////////////////////////////////////////////////////////////////////////

bool CAgitatedConditionIsTerritoryIntruded::Check(const CAgitatedConditionContext& aContext) const
{
	return (aContext.m_Task->IsTerritoryIntruded());
}

////////////////////////////////////////////////////////////////////////////////

bool CAgitatedConditionIntruderLeft::Check(const CAgitatedConditionContext& aContext) const
{
	CTaskUnalerted* pTask = static_cast<CTaskUnalerted*>(aContext.m_Task->GetPed()->GetPedIntelligence()->GetTaskDefault());
	CPed* pAgitator = aContext.m_Task->GetAgitator();
	if (pTask && pAgitator)
	{
		CScenarioPoint* pPoint = pTask->GetLastUsedScenarioPoint();
		if (pPoint)
		{
			if (IsGreaterThanAll(DistSquared(pPoint->GetPosition(), pAgitator->GetTransform().GetPosition()), ScalarV(square(pPoint->GetRadius()))))
			{
				return true;
			}
		}
	}
	return false;
}

/////////////////////////////////////////////////////////////////////////////////////

bool CAgitatedConditionIsUsingTerritoryScenario::Check(const CAgitatedConditionContext& aContext) const
{
	//Check if the task is valid.
	CTaskUseScenario* pTask = static_cast<CTaskUseScenario*>(
		aContext.m_Task->GetPed()->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_USE_SCENARIO));
	if(pTask)
	{
		//Check if the flag is set.
		CScenarioPoint* pPoint = pTask->GetScenarioPoint();
		if(pPoint && pPoint->IsFlagSet(CScenarioPointFlags::TerritorialScenario))
		{
			return true;
		}
	}

	return false;
}

/////////////////////////////////////////////////////////////////////////////////////

bool CAgitatedConditionIsPlayingAmbientsInScenario::Check(const CAgitatedConditionContext& aContext) const
{
	CTaskUseScenario* pScenarioTask = 
		static_cast<CTaskUseScenario*>(aContext.m_Task->GetPed()->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_USE_SCENARIO));
	// Count enter/idle/outro as playing ambients, as once those animations finish the ambient state will begin and pick up the event.
	if (pScenarioTask)
	{
		s32 iState = pScenarioTask->GetState();
		if (iState == CTaskUseScenario::State_PlayAmbients || iState == CTaskUseScenario::State_Enter ||
			iState == CTaskUseScenario::State_AggroOrCowardOutro)
		{
			return true;
		}
	}
	return false;
}

/////////////////////////////////////////////////////////////////////////////////////

bool CAgitatedConditionIsReadyForScenarioResponse::Check(const CAgitatedConditionContext& aContext) const
{
	//Check if the task is valid.
	CTaskUseScenario* pTask =
		static_cast<CTaskUseScenario*>(aContext.m_Task->GetPed()->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_USE_SCENARIO));
	if(pTask)
	{
		//Check if the state is valid.
		switch(pTask->GetState())
		{
			case CTaskUseScenario::State_GoToStartPosition:
			case CTaskUseScenario::State_PlayAmbients:
			{
				return true;
			}
			default:
			{
				break;
			}
		}
	}

	return false;
}

/////////////////////////////////////////////////////////////////////////////////////

bool CAgitatedConditionIsUsingRagdoll::Check(const CAgitatedConditionContext& aContext) const
{
	return (aContext.m_Task->GetPed()->GetUsingRagdoll());
}

/////////////////////////////////////////////////////////////////////////////////////

bool CAgitatedConditionRandom::Check(const CAgitatedConditionContext& UNUSED_PARAM(aContext)) const
{
	//Ensure the chances are valid.
	float fChances = m_Chances;
	if(fChances <= 0.0f)
	{
		return false;
	}

	//Enforce the chances.
	float fRandom = fwRandom::GetRandomNumberInRange(0.0f, 1.0f);
	return (fRandom <= fChances);
}

/////////////////////////////////////////////////////////////////////////////////////

bool CAgitatedConditionWasLeaderHit::Check(const CAgitatedConditionContext& aContext) const
{
	return (aContext.m_Task->WasLeaderHit());
}

/////////////////////////////////////////////////////////////////////////////////////

bool CAgitatedConditionWasRecentlyBumpedWhenStill::Check(const CAgitatedConditionContext& aContext) const
{
	return (aContext.m_Task->WasRecentlyBumpedWhenStill(m_MaxTime));
}

/////////////////////////////////////////////////////////////////////////////////////

bool CAgitatedConditionWasUsingTerritorialScenario::Check(const CAgitatedConditionContext& aContext) const
{
	return (aContext.m_Task->WasUsingTerritorialScenario());
}

/////////////////////////////////////////////////////////////////////////////////////

bool CAgitatedConditionHasPavement::Check(const CAgitatedConditionContext& aContext) const
{
	return (aContext.m_Task->HasPavement());
}

/////////////////////////////////////////////////////////////////////////////////////

bool CAgitatedConditionIsCallingPolice::Check(const CAgitatedConditionContext& aContext) const
{
	return aContext.m_Task->GetPed()->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_CALL_POLICE);
}

/////////////////////////////////////////////////////////////////////////////////////

bool CAgitatedConditionIsSwimming::Check(const CAgitatedConditionContext& aContext) const
{
	return aContext.m_Task->GetPed()->GetIsSwimming();
}

/////////////////////////////////////////////////////////////////////////////////////
