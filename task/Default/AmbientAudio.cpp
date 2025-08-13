// File header
#include "Task/Default/AmbientAudio.h"

// Rage headers
#include "parser/macros.h"
#include "parser/manager.h"
#include "system/threadtype.h"

// Game headers
#include "modelinfo/PedModelInfo.h"
#include "Peds/PedIntelligence.h"
#include "task/Response/Info/AgitatedManager.h"
#include "task/Scenario/ScenarioPoint.h"
#include "task/Scenario/ScenarioPointManager.h"

// Parser files
#include "AmbientAudio_parser.h"

AI_OPTIMISATIONS()

////////////////////////////////////////////////////////////////////////////////
// CAmbientAudio
////////////////////////////////////////////////////////////////////////////////

u32 CAmbientAudio::GetFinalizedHash() const
{
	//Check if this is not a conversation.
	if(!m_Flags.IsFlagSet(IsConversation))
	{
		return m_uFinalizedHash;
	}
	else
	{
		//Get the partial hash.
		u32 uPartialHash = GetPartialHash();

		//Create the finalized hash.
		u32 uFinalizedHash = atFinalizeHash(uPartialHash);

		return uFinalizedHash;
	}
}

u32 CAmbientAudio::GetPartialHash() const
{
	//Check if this is not a conversation.
	if(!m_Flags.IsFlagSet(IsConversation))
	{
		return m_Context;
	}
	else
	{
		//Create the partial hash.
		char buf[32];
		formatf(buf, "_CONV%02d_PART%d", m_uConversationVariation, m_uConversationPart);
		u32 uPartialHash = atPartialStringHash(buf, m_Context);

		return uPartialHash;
	}
}

bool CAmbientAudio::IsFollowUp() const
{
	//Ensure this is a conversation.
	if(!m_Flags.IsFlagSet(IsConversation))
	{
		return false;
	}

	//Ensure the part is > 1.
	if(m_uConversationPart <= 1)
	{
		return false;
	}

	return true;
}

////////////////////////////////////////////////////////////////////////////////
// CAmbientAudioManager
////////////////////////////////////////////////////////////////////////////////

CAmbientAudioManager *CAmbientAudioManager::sm_Instance = NULL;

CAmbientAudioManager::CAmbientAudioManager()
: m_AmbientAudios()
{
	//Parse the ambient audio.
	PARSER.LoadObject("common:/data/ai/ambientaudio", "meta", m_AmbientAudios);
}

CAmbientAudioManager::~CAmbientAudioManager()
{

}

const CAmbientAudioExchange* CAmbientAudioManager::GetExchange(const CPed& rPed, const CPed& rTarget) const
{
	Options options;
	return GetExchange(rPed, rTarget, options);
}

const CAmbientAudioExchange* CAmbientAudioManager::GetExchange(const CPed& rPed, const CPed& rTarget, const Options& rOptions) const
{
	//Assert that this function is called from the main thread.
	//This was added to call attention to the fact that we are generally using the return value of
	//this function to copy and store data, which may not be thread-safe in the future (depending on usage).
	Assert(sysThreadType::IsUpdateThread());

	return GetExchangeFromTriggers(rPed, rTarget, rOptions);
}

void CAmbientAudioManager::Init(unsigned UNUSED_PARAM(initMode))
{
	Assert(!sm_Instance);
	sm_Instance = rage_new CAmbientAudioManager;
}

void CAmbientAudioManager::Shutdown(unsigned UNUSED_PARAM(shutdownMode))
{
	Assert(sm_Instance);
	delete sm_Instance;
	sm_Instance = NULL;
}

bool CAmbientAudioManager::EvaluateConditions(const CAmbientAudioConditions& rConditions, const CAmbientAudioExchange& rExchange, const CPed& rPed, const CPed& rTarget, const Options& rOptions) const
{
	//Check if we only care about follow-ups.
	if(rOptions.m_uFlags.IsFlagSet(Options::OnlyFollowUps))
	{
		//Ensure this is a follow-up.
		if(!rConditions.IsFollowUp() && !rExchange.m_Initial.IsFollowUp())
		{
			return false;
		}
	}

	//Check if we must be friendly.
	if(rOptions.m_uFlags.IsFlagSet(Options::MustBeFriendly))
	{
		//Ensure the exchange is friendly.
		if(!IsExchangeFriendly(rExchange))
		{
			return false;
		}
	}
	//Check if we must be unfriendly.
	else if(rOptions.m_uFlags.IsFlagSet(Options::MustBeUnfriendly))
	{
		//Ensure the exchange is unfriendly.
		if(IsExchangeFriendly(rExchange))
		{
			return false;
		}
	}

	//Check if we have either ped types, model names, or ped personalities specified.  If not, all peds are affected.
	if(rConditions.HasPedTypes() || rConditions.HasModelNames() || rConditions.HasPedPersonalities())
	{
		//Ensure either the ped type, model name, or ped personality matches the trigger.
		if(!EvaluatePedType(rConditions, rTarget) && !EvaluateModelName(rConditions, rTarget) && !EvaluatePedPersonality(rConditions, rTarget))
		{
			return false;
		}
	}

	//Ensure the chances are valid.
	float fChances = rConditions.m_Chances;
	if(fChances <= 0.0f)
	{
		return false;
	}

	//Ensure the randomness is enforced.
	float fRandom = fwRandom::GetRandomNumberInRange(0.0f, 1.0f);
	if(fRandom > fChances)
	{
		return false;
	}

	//Check if the flags are valid.
	if(!EvaluateFlags(rConditions.m_Flags, rExchange, rPed, rTarget, rOptions))
	{
		return false;
	}

	//Ensure the 'no repeat' is valid.
	if(!EvaluateNoRepeat(rConditions.m_NoRepeat, rExchange, rPed, rTarget))
	{
		return false;
	}

	//Ensure the 'we said' follow-up is valid.
	if(!EvaluateFollowUp(rConditions.m_WeSaid, rPed, rTarget))
	{
		return false;
	}

	//Ensure the 'they said' follow-up is valid.
	if(!EvaluateFollowUp(rConditions.m_TheySaid, rTarget, rPed))
	{
		return false;
	}

	//Ensure the friends are valid.
	if(!EvaluateFriends(rConditions.m_Friends, rPed))
	{
		return false;
	}

	//Ensure the distance is within the threshold.
	ScalarV scDistSq = DistSquared(rPed.GetTransform().GetPosition(), rTarget.GetTransform().GetPosition());
	ScalarV scMinDistSq = ScalarVFromF32(square(rConditions.m_MinDistance));
	ScalarV scMaxDistSq = ScalarVFromF32(square(rConditions.m_MaxDistance));
	if(IsLessThanAll(scDistSq, scMinDistSq) || IsGreaterThanAll(scDistSq, scMaxDistSq))
	{
		return false;
	}

	//Ensure the distance from player is within the threshold.
	ScalarV scDistFromPlayerSq = DistSquared(rPed.GetTransform().GetPosition(), VECTOR3_TO_VEC3V(CGameWorld::FindLocalPlayerCoors()));
	ScalarV scMinDistFromPlayerSq = ScalarVFromF32(square(rConditions.m_MinDistanceFromPlayer));
	ScalarV scMaxDistFromPlayerSq = ScalarVFromF32(square(rConditions.m_MaxDistanceFromPlayer));
	if(IsLessThanAll(scDistFromPlayerSq, scMinDistFromPlayerSq) || IsGreaterThanAll(scDistFromPlayerSq, scMaxDistFromPlayerSq))
	{
		return false;
	}

	return true;
}

bool CAmbientAudioManager::EvaluateFlags(fwFlags32 uFlags, const CAmbientAudioExchange& rExchange, const CPed& rPed, const CPed& rTarget, const Options& UNUSED_PARAM(rOptions)) const
{
	//Ensure the exercising flag is enforced.
	if(uFlags.IsFlagSet(CAmbientAudioConditions::IsExercising))
	{
		//Ensure the ped is exercising.
		if(!IsPedExercising(rPed))
		{
			return false;
		}
	}

	//Ensure the jeer flag is enforced.
	if(uFlags.IsFlagSet(CAmbientAudioConditions::WillJeerAtHotPeds))
	{
		//Ensure the ped will jeer at hot peds.
		if(!WillPedJeerAtHotPeds(rPed))
		{
			return false;
		}
	}

	//Ensure the friendly flags are enforced.
	if(uFlags.IsFlagSet(CAmbientAudioConditions::IsFriendlyWithTarget))
	{
		//Ensure we are friendly with the target.
		if(!rPed.GetPedIntelligence()->IsFriendlyWith(rTarget))
		{
			return false;
		}
	}
	else if(uFlags.IsFlagSet(CAmbientAudioConditions::IsNotFriendlyWithTarget))
	{
		//Ensure we are not friendly with the target.
		if(rPed.GetPedIntelligence()->IsFriendlyWith(rTarget))
		{
			return false;
		}
	}

	//Ensure the threatened flags are enforced.
	if(uFlags.IsFlagSet(CAmbientAudioConditions::IsThreatenedByTarget))
	{
		//Ensure we are threatened by the target.
		if(!rPed.GetPedIntelligence()->IsThreatenedBy(rTarget))
		{
			return false;
		}
	}
	else if(uFlags.IsFlagSet(CAmbientAudioConditions::IsNotThreatenedByTarget))
	{
		//Ensure we are not threatened by the target.
		if(rPed.GetPedIntelligence()->IsThreatenedBy(rTarget))
		{
			return false;
		}
	}

	//Ensure the hot flags is enforced.
	if(uFlags.IsFlagSet(CAmbientAudioConditions::IsTargetHot))
	{
		//Ensure the target is hot.
		if(!IsPedHot(rTarget))
		{
			return false;
		}
	}
	else if(uFlags.IsFlagSet(CAmbientAudioConditions::IsTargetNotHot))
	{
		//Ensure the target is not hot.
		if(IsPedHot(rTarget))
		{
			return false;
		}
	}

	//Ensure the player flags is enforced.
	if(uFlags.IsFlagSet(CAmbientAudioConditions::IsTargetPlayer))
	{
		//Ensure the target is a player.
		if(!rTarget.IsPlayer())
		{
			return false;
		}
	}
	else if(uFlags.IsFlagSet(CAmbientAudioConditions::IsTargetNotPlayer))
	{
		//Ensure the target is not a player.
		if(rTarget.IsPlayer())
		{
			return false;
		}
	}

	//Ensure the response flag is enforced.
	if(uFlags.IsFlagSet(CAmbientAudioConditions::TargetMustHaveResponse))
	{
		//Ensure the target has the response.
		if(!rExchange.m_Response.DoesContextExistForPed(rTarget))
		{
			return false;
		}
	}

	//Ensure the on-foot flag is enforced.
	if(uFlags.IsFlagSet(CAmbientAudioConditions::IsTargetOnFoot))
	{
		//Ensure the target is on foot.
		if(!IsPedOnFoot(rTarget))
		{
			return false;
		}
	}

	//Ensure the male flags are enforced.
	if(uFlags.IsFlagSet(CAmbientAudioConditions::IsTargetMale))
	{
		//Ensure the target is male.
		if(!IsPedMale(rTarget))
		{
			return false;
		}
	}
	else if(uFlags.IsFlagSet(CAmbientAudioConditions::IsTargetNotMale))
	{
		//Ensure the target is not male.
		if(IsPedMale(rTarget))
		{
			return false;
		}
	}

	//Ensure the cop flags are enforced.
	if(uFlags.IsFlagSet(CAmbientAudioConditions::IsTargetCop))
	{
		//Ensure the target is a cop.
		if(!rTarget.IsRegularCop())
		{
			return false;
		}
	}

	//Ensure the gang flags are enforced.
	if(uFlags.IsFlagSet(CAmbientAudioConditions::IsTargetGang))
	{
		//Ensure the target is a gang member.
		if(!rTarget.IsGangPed())
		{
			return false;
		}
	}

	//Ensure the fat flags are enforced.
	if(uFlags.IsFlagSet(CAmbientAudioConditions::IsTargetFat))
	{
		//Ensure the target is fat.
		if(!IsPedFat(rTarget))
		{
			return false;
		}
	}

	//Ensure the last talked to target flags are enforced.
	if(uFlags.IsFlagSet(CAmbientAudioConditions::DidNotTalkToTargetLast))
	{
		//Ensure we did not talk to the target last.
		if(TalkedToPedLast(rPed, rTarget))
		{
			return false;
		}
	}

	//Ensure the friendly flags are enforced.
	if(uFlags.IsFlagSet(CAmbientAudioConditions::IsTargetFriendlyWithUs))
	{
		//Ensure the target is friendly with us.
		if(!rTarget.GetPedIntelligence()->IsFriendlyWith(rPed))
		{
			return false;
		}
	}
	else if(uFlags.IsFlagSet(CAmbientAudioConditions::IsTargetNotFriendlyWithUs))
	{
		//Ensure the target is not friendly with us.
		if(rTarget.GetPedIntelligence()->IsFriendlyWith(rPed))
		{
			return false;
		}
	}

	bool bMustHaveContext = (uFlags.IsFlagSet(CAmbientAudioConditions::MustHaveContext) ||
		rExchange.m_Initial.GetFlags().IsFlagSet(CAmbientAudio::IsConversation));
	if(bMustHaveContext)
	{
		//Ensure we have the context.
		if(!rExchange.m_Initial.DoesContextExistForPed(rPed))
		{
			return false;
		}
	}

	if(uFlags.IsFlagSet(CAmbientAudioConditions::IsUsingScenario))
	{
		//Ensure we are using a scenario.
		if(!IsPedUsingScenario(rPed))
		{
			return false;
		}
	}

	//Ensure the flee flags are enforced.
	if(uFlags.IsFlagSet(CAmbientAudioConditions::TargetMustBeFleeing))
	{
		if(!IsPedFleeing(rTarget))
		{
			return false;
		}
	}
	else if(!uFlags.IsFlagSet(CAmbientAudioConditions::TargetCanBeFleeing))
	{
		if(IsPedFleeing(rTarget))
		{
			return false;
		}
	}

	//Ensure the combat flags are enforced.
	if(uFlags.IsFlagSet(CAmbientAudioConditions::TargetMustBeInCombat))
	{
		if(!IsPedInCombat(rTarget))
		{
			return false;
		}
	}
	else if(!uFlags.IsFlagSet(CAmbientAudioConditions::TargetCanBeInCombat))
	{
		if(IsPedInCombat(rTarget))
		{
			return false;
		}
	}

	//Ensure the dead flags are enforced.
	if(uFlags.IsFlagSet(CAmbientAudioConditions::TargetMustBeDead))
	{
		if(!IsPedDead(rTarget))
		{
			return false;
		}
	}
	else if(!uFlags.IsFlagSet(CAmbientAudioConditions::TargetCanBeDead))
	{
		if(IsPedDead(rTarget))
		{
			return false;
		}
	}

	if(uFlags.IsFlagSet(CAmbientAudioConditions::TargetMustBeKnockedDown))
	{
		if(!IsPedKnockedDown(rTarget))
		{
			return false;
		}
	}

	if (uFlags.IsFlagSet(CAmbientAudioConditions::IsFriendFollowedByPlayer))
	{
		if (!IsFriendFollowedByPlayer(rPed))
		{
			return false;
		}
	}

	if(uFlags.IsFlagSet(CAmbientAudioConditions::IsInGangTerritory))
	{
		if(!IsPedInGangTerritory(rPed))
		{
			return false;
		}
	}

	if(uFlags.IsFlagSet(CAmbientAudioConditions::HasTargetAchievedCombatVictory))
	{
		if(!HasPedAchievedCombatVictory(rTarget, rPed))
		{
			return false;
		}
	}

	return true;
}

bool CAmbientAudioManager::EvaluateFollowUp(const CAmbientAudioConditions::FollowUp& rFollowUp, const CPed& rPed, const CPed& rTarget) const
{
	//Check if the follow-up is valid.
	if(rFollowUp.IsValid())
	{
		//Ensure we have recently talked to the ped.
		if(!rPed.GetPedIntelligence()->HasRecentlyTalkedToPed(rTarget, rFollowUp.m_MaxTime))
		{
			return false;
		}

		//Ensure the context matches.
		const CAmbientAudio* pAudio = rPed.GetPedIntelligence()->GetLastAudioSaidToPed(rTarget);
		if(!pAudio || (pAudio->GetFinalizedHash() != rFollowUp.m_Context.GetHash()))
		{
			return false;
		}
	}

	return true;
}

bool CAmbientAudioManager::EvaluateFriends(const CAmbientAudioConditions::Friends& rFriends, const CPed& rPed) const
{
	//Check if the friends are valid.
	if(rFriends.IsValid())
	{
		//Ensure the friends are valid.
		u8 uFriends = rPed.GetPedIntelligence()->CountFriends(rFriends.m_MaxDistance, true, true);
		if((uFriends < rFriends.m_Min) || (uFriends > rFriends.m_Max))
		{
			return false;
		}
	}

	return true;
}

bool CAmbientAudioManager::EvaluateModelName(const CAmbientAudioConditions& rConditions, const CPed& rPed) const
{
	//Ensure the model info is valid.
	const CPedModelInfo* pPedModelInfo = rPed.GetPedModelInfo();
	if(!pPedModelInfo)
	{
		return false;
	}

	//Ensure the trigger has our model name.
	atHashWithStringNotFinal hModelName(pPedModelInfo->GetModelNameHash());
	if(!rConditions.HasModelName(hModelName))
	{
		return false;
	}

	return true;
}

bool CAmbientAudioManager::EvaluateNoRepeat(float fMaxTime, const CAmbientAudioExchange& rExchange, const CPed& rPed, const CPed& rTarget) const
{
	//Check if the max time is valid.
	if(fMaxTime > 0.0f)
	{
		//Ensure the ped has not recently said their line to the target.
		if(rPed.GetPedIntelligence()->HasRecentlyTalkedToPed(rTarget, fMaxTime, rExchange.m_Initial))
		{
			return false;
		}
	}

	return true;
}

bool CAmbientAudioManager::EvaluatePedPersonality(const CAmbientAudioConditions& rConditions, const CPed& rPed) const
{
	//Ensure the model info is valid.
	const CPedModelInfo* pPedModelInfo = rPed.GetPedModelInfo();
	if(!pPedModelInfo)
	{
		return false;
	}

	//Ensure the trigger has our personality.
	atHashWithStringNotFinal hPersonality(pPedModelInfo->GetPersonalitySettings().GetPersonalityNameHash());
	if(!rConditions.HasPedPersonality(hPersonality))
	{
		return false;
	}

	return true;
}

bool CAmbientAudioManager::EvaluatePedType(const CAmbientAudioConditions& rConditions, const CPed& rPed) const
{
	//Ensure the trigger has our ped type.
	if(!rConditions.HasPedType(rPed.GetPedType()))
	{
		return false;
	}

	return true;
}

bool CAmbientAudioManager::EvaluateTrigger(const CAmbientAudioTrigger& rTrigger, const CPed& rPed, const CPed& rTarget, const Options& rOptions) const
{
	//Ensure the exchange is valid.
	const CAmbientAudioExchange* pExchange = GetProcessedExchangeFromTrigger(rTrigger, rPed, rTarget, rOptions);
	if(!pExchange)
	{
		return false;
	}

	//Ensure the conditions are valid.
	if(!EvaluateConditions(rTrigger.m_Conditions, *pExchange, rPed, rTarget, rOptions))
	{
		return false;
	}

	return true;
}

const CAmbientAudioExchange* CAmbientAudioManager::GetExchangeFromTrigger(const CAmbientAudioTrigger& rTrigger, const CPed& rPed, const CPed& rTarget, const Options& rOptions) const
{
	//Check if the exchange is valid.
	atHashWithStringNotFinal hExchange = rTrigger.m_Exchange;
	if(hExchange.IsNotNull())
	{
		return GetExchange(hExchange);
	}

	//Check if the exchanges are valid.
	int iNumExchanges = rTrigger.m_Exchanges.GetCount();
	if(iNumExchanges > 0)
	{
		//Choose a random exchange.
		int iRandom = fwRandom::GetRandomNumberInRange(0, iNumExchanges);
		return GetExchange(rTrigger.m_Exchanges[iRandom]);
	}

	//Check if the weighted exchanges are valid.
	int iNumWeightedExchanges = rTrigger.m_WeightedExchanges.GetCount();
	if(iNumWeightedExchanges > 0)
	{
		//Calculate the total weight.
		float fTotalWeight = 0.0f;
		for(int i = 0; i < iNumWeightedExchanges; ++i)
		{
			//Grab the weight.
			float fWeight = rTrigger.m_WeightedExchanges[i].m_Weight;
			aiAssert(fWeight > 0.0f);

			//Add the weight.
			fTotalWeight += fWeight;
		}

		//Assert that the total weight is valid.
		aiAssert(fTotalWeight > 0.0f);

		//Generate a random weight.
		float fRandom = fwRandom::GetRandomNumberInRange(0.0f, fTotalWeight);

		//Choose the exchange.
		for(int i = 0; i < iNumWeightedExchanges; ++i)
		{
			//Grab the weight.
			float fWeight = rTrigger.m_WeightedExchanges[i].m_Weight;
			aiAssert(fWeight > 0.0f);

			//Subtract the weight.
			fRandom -= fWeight;
			if(fRandom <= 0.0f)
			{
				return GetExchange(rTrigger.m_WeightedExchanges[i].m_Exchange);
			}
		}
	}

	//Check if the conditional exchanges are valid.
	int iNumConditionalExchanges = rTrigger.m_ConditionalExchanges.GetCount();
	if(iNumConditionalExchanges > 0)
	{
		//Iterate over the conditional exchanges.
		for(int i = 0; i < iNumConditionalExchanges; ++i)
		{
			//Grab the conditional exchange.
			const CAmbientAudioTrigger::ConditionalExchange& rConditionalExchange = rTrigger.m_ConditionalExchanges[i];

			//Choose the exchange.
			hExchange = rConditionalExchange.m_Exchange;
			if(hExchange.IsNull())
			{
				iNumExchanges = rConditionalExchange.m_Exchanges.GetCount();
				if(iNumExchanges > 0)
				{
					//Choose a random exchange.
					int iRandom = fwRandom::GetRandomNumberInRange(0, iNumExchanges);
					hExchange = rConditionalExchange.m_Exchanges[iRandom];
					if(hExchange.IsNull())
					{
						continue;
					}
				}
				else
				{
					continue;
				}
			}
			aiAssert(hExchange.IsNotNull());

			//Ensure the exchange is valid.
			const CAmbientAudioExchange* pExchange = GetProcessedExchange(hExchange, rPed, rTarget);
			if(!pExchange)
			{
				continue;
			}

			//Ensure the conditions are valid.
			if(!EvaluateConditions(rConditionalExchange.m_Conditions, *pExchange, rPed, rTarget, rOptions))
			{
				continue;
			}

			return pExchange;
		}
	}

	return NULL;
}

const CAmbientAudioExchange* CAmbientAudioManager::GetExchangeFromTriggers(const CPed& rPed, const CPed& rTarget, const Options& rOptions) const
{
	//Ensure the set is valid.
	atHashWithStringNotFinal hSet = GetSet(rPed);
	if(hSet.IsNull())
	{
		return NULL;
	}

	return GetExchangeFromTriggersForSet(hSet, rPed, rTarget, rOptions);
}

const CAmbientAudioExchange* CAmbientAudioManager::GetExchangeFromTriggersForSet(atHashWithStringNotFinal hSet, const CPed& rPed, const CPed& rTarget, const Options& rOptions) const
{
	//Ensure the set is valid.
	const CAmbientAudioSet* pSet = GetSet(hSet);
	if(!aiVerifyf(pSet, "The set: %s is invalid.", hSet.GetCStr()))
	{
		return NULL;
	}

	//Iterate over the triggers.
	const atArray<CAmbientAudioTrigger>& rTriggers = pSet->m_Triggers;
	for(int i = 0; i < rTriggers.GetCount(); ++i)
	{
		//Grab the trigger.
		const CAmbientAudioTrigger& rTrigger = rTriggers[i];

		//Evaluate the trigger.
		if(!EvaluateTrigger(rTrigger, rPed, rTarget, rOptions))
		{
			continue;
		}

		//Ensure the exchange is valid.
		const CAmbientAudioExchange* pExchange = GetProcessedExchangeFromTrigger(rTrigger, rPed, rTarget, rOptions);
		if(!aiVerifyf(pExchange, "The exchange is invalid for trigger index: %d in set: %s.", i, hSet.GetCStr()))
		{
			continue;
		}

		return pExchange;
	}

	//Ensure the parent set is valid.
	hSet = pSet->m_Parent;
	if(hSet.IsNull())
	{
		return NULL;
	}

	return GetExchangeFromTriggersForSet(hSet, rPed, rTarget, rOptions);
}

const CAmbientAudioExchange* CAmbientAudioManager::GetProcessedExchange(atHashWithStringNotFinal hExchange, const CPed& rPed, const CPed& rTarget) const
{
	//Ensure the exchange is valid.
	const CAmbientAudioExchange* pExchange = GetExchange(hExchange);
	if(!pExchange)
	{
		return NULL;
	}

	//Process the exchange.
	pExchange = ProcessExchange(*pExchange, rPed, rTarget);
	if(!pExchange)
	{
		return NULL;
	}

	return pExchange;
}

const CAmbientAudioExchange* CAmbientAudioManager::GetProcessedExchangeFromTrigger(const CAmbientAudioTrigger& rTrigger, const CPed& rPed, const CPed& rTarget, const Options& rOptions) const
{
	//Ensure the exchange is valid.
	const CAmbientAudioExchange* pExchange = GetExchangeFromTrigger(rTrigger, rPed, rTarget, rOptions);
	if(!pExchange)
	{
		return NULL;
	}

	//Process the exchange.
	pExchange = ProcessExchange(*pExchange, rPed, rTarget);
	if(!pExchange)
	{
		return NULL;
	}

	return pExchange;
}

atHashWithStringNotFinal CAmbientAudioManager::GetSet(const CPed& rPed) const
{
	//Ensure the model info is valid.
	const CPedModelInfo* pPedModelInfo = rPed.GetPedModelInfo();
	if(!pPedModelInfo)
	{
		return atHashWithStringNotFinal::Null();
	}

	return (pPedModelInfo->GetPersonalitySettings().GetAmbientAudio());
}

bool CAmbientAudioManager::HasPedAchievedCombatVictory(const CPed& rPed, const CPed& rTarget) const
{
	return (rPed.GetPedIntelligence()->GetAchievedCombatVictoryOver() == &rTarget);
}

bool CAmbientAudioManager::IsAudioFriendly(const CAmbientAudio& rAudio) const
{
	//Ensure the audio is not insulting.
	if(rAudio.GetFlags().IsFlagSet(CAmbientAudio::IsInsulting))
	{
		return false;
	}

	//Ensure the audio is not harassing.
	if(rAudio.GetFlags().IsFlagSet(CAmbientAudio::IsHarassing))
	{
		return false;
	}

	return true;
}

bool CAmbientAudioManager::IsExchangeFriendly(const CAmbientAudioExchange& rExchange) const
{
	return (IsAudioFriendly(rExchange.m_Initial));
}

bool CAmbientAudioManager::IsPedDead(const CPed& rPed) const
{
	return (rPed.IsInjured());
}

bool CAmbientAudioManager::IsPedExercising(const CPed& UNUSED_PARAM(rPed)) const
{
	//TODO: Figure out how to determine if peds are exercising.

	return false;
}

bool CAmbientAudioManager::IsPedFat(const CPed& rPed) const
{
	//Ensure the model info is valid.
	const CPedModelInfo* pPedModelInfo = rPed.GetPedModelInfo();
	if(!pPedModelInfo)
	{
		return false;
	}

	//Ensure the movement clip set is valid.
	fwMvClipSetId clipSetId = pPedModelInfo->GetMovementClipSet();
	if(clipSetId == CLIP_SET_ID_INVALID)
	{
		return false;
	}

	static fwMvClipSetId s_MoveMFatAClipSetId("move_m@fat@a");
	if(clipSetId == s_MoveMFatAClipSetId)
	{
		return true;
	}

	static fwMvClipSetId s_MoveFFatAClipSetId("move_f@fat@a");
	if(clipSetId == s_MoveFFatAClipSetId)
	{
		return true;
	}

	return false;
}

bool CAmbientAudioManager::IsPedFleeing(const CPed& rPed) const
{
	//Check if the ped is fleeing.
	if(rPed.GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_SMART_FLEE))
	{
		return true;
	}

	//Check if the ped is reacting into a flee.
	if(rPed.GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_REACT_AND_FLEE))
	{
		return true;
	}

	return false;
}

bool CAmbientAudioManager::IsPedHot(const CPed& rPed) const
{
	//Ensure the ped is hot.
	if(!rPed.CheckSexinessFlags(SF_HOT_PERSON))
	{
		return false;
	}

	return true;
}

bool CAmbientAudioManager::IsPedInCombat(const CPed& rPed) const
{
	//Check if the ped is in combat.
	if(rPed.GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_COMBAT))
	{
		return true;
	}

	return false;
}

bool CAmbientAudioManager::IsPedInGangTerritory(const CPed& rPed) const
{
	//Ensure the scenario point is valid.
	const CScenarioPoint* pScenarioPoint = rPed.GetScenarioPoint(rPed, true);
	if(!pScenarioPoint)
	{
		return false;
	}

	//Ensure custom models were used.
	if(pScenarioPoint->GetModelSetIndex() == CScenarioPointManager::kNoCustomModelSet)
	{
		return false;
	}

	return true;
}

bool CAmbientAudioManager::IsPedKnockedDown(const CPed& rPed) const
{
	//Check if the ped is using a ragdoll.
	if(rPed.GetUsingRagdoll())
	{
		return true;
	}

	//Check if the ped is getting up.
	if(rPed.GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_GET_UP, true))
	{
		return true;
	}

	return false;
}

bool CAmbientAudioManager::IsPedMale(const CPed& rPed) const
{
	//Ensure the ped is male.
	if(!rPed.IsMale())
	{
		return false;
	}

	return true;
}

bool CAmbientAudioManager::IsPedOnFoot(const CPed& rPed) const
{
	return (rPed.GetIsOnFoot());
}

bool CAmbientAudioManager::IsPedUsingScenario(const CPed& rPed) const
{
	return (rPed.GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_USE_SCENARIO));
}

void CAmbientAudioManager::ProcessConversation(CAmbientAudio& rAudio, const CPed& rPed, const CPed& rTarget) const
{
	//Ensure the audio is a conversation.
	if(!rAudio.GetFlags().IsFlagSet(CAmbientAudio::IsConversation))
	{
		return;
	}

	//Check if the ped last talked to the target.
	static dev_float s_fMaxTime = 15.0f;
	if(rPed.GetPedIntelligence()->HasRecentlyTalkedToPed(rTarget, s_fMaxTime))
	{
		//Check if the contexts match.
		const CAmbientAudio* pAudio = rPed.GetPedIntelligence()->GetLastAudioSaidToPed(rTarget);
		if(pAudio && (pAudio->GetContext() == rAudio.GetContext()))
		{
			//Set the conversation variation and part.
			rAudio.SetConversationVariation(pAudio->GetConversationVariation());
			rAudio.SetConversationPart(pAudio->GetConversationPart() + 1);
			return;
		}
	}

	//Set the conversation variation and part.
	rAudio.SetConversationVariation((u8)fwRandom::GetRandomNumberInRange(1, 15));
	rAudio.SetConversationPart(1);
}

const CAmbientAudioExchange* CAmbientAudioManager::ProcessExchange(const CAmbientAudioExchange& rExchange, const CPed& rPed, const CPed& rTarget) const
{
	//Check if neither audio is a conversation.
	if(!rExchange.m_Initial.GetFlags().IsFlagSet(CAmbientAudio::IsConversation) &&
		!rExchange.m_Response.GetFlags().IsFlagSet(CAmbientAudio::IsConversation))
	{
		return &rExchange;
	}

	//Copy the exchange.
	static CAmbientAudioExchange exchange;
	exchange = rExchange;

	//Process the conversation.
	ProcessConversation(exchange.m_Initial, rPed, rTarget);
	ProcessConversation(exchange.m_Response, rTarget, rPed);

	return &exchange;
}

bool CAmbientAudioManager::TalkedToPedLast(const CPed& rPed, const CPed& rTarget) const
{
	return (rPed.GetPedIntelligence()->GetTalkedToPedInfo().m_pPed == &rTarget);
}

bool CAmbientAudioManager::WillPedJeerAtHotPeds(const CPed& rPed) const
{
	//Ensure the ped will jeer at hot peds.
	if(!rPed.CheckSexinessFlags(SF_JEER_AT_HOT_PED))
	{
		return false;
	}

	return true;
}

bool CAmbientAudioManager::IsFriendFollowedByPlayer(const CPed& rPed) const
{
	return rPed.GetPedConfigFlag(CPED_CONFIG_FLAG_CanSayFollowedByPlayerAudio);
}
