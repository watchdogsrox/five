// File header
#include "streaming/SituationalClipSetStreamer.h"

// Game headers
#include "animation/AnimDefines.h"
#include "game/GameSituation.h"
#include "Peds/ped.h"
#include "scene/world/GameWorld.h"
#include "task/Movement/TaskParachute.h"
#include "weapons/inventory/PedWeaponManager.h"

// Parser headers
#include "streaming/SituationalClipSetStreamer_parser.h"

STREAMING_OPTIMISATIONS()

////////////////////////////////////////////////////////////////////////////////
// CSituationalClipSetStreamer
////////////////////////////////////////////////////////////////////////////////

CSituationalClipSetStreamer* CSituationalClipSetStreamer::sm_Instance = NULL;

CSituationalClipSetStreamer::Tunables CSituationalClipSetStreamer::sm_Tunables;
IMPLEMENT_TUNABLES_PSO(CSituationalClipSetStreamer, "CSituationalClipSetStreamer", 0x83698af8, "Situational Clip Sets", "Game Logic");

CSituationalClipSetStreamer::CSituationalClipSetStreamer()
: m_uTimeToReleaseParachutePackVariation(0)
, m_uRunningFlags(0)
{
	//Resize the array to match the number of variations.
	m_ClipSetsForAvoidVariations.Resize(sm_Tunables.m_Avoids.m_Variations.GetCount());
}

CSituationalClipSetStreamer::~CSituationalClipSetStreamer()
{

}

fwMvClipSetId CSituationalClipSetStreamer::GetClipSetForAvoids(bool bCasual) const
{
	//Check if we should use a variation.
	struct Variation
	{
		Variation()
		: m_ClipSetId(CLIP_SET_ID_INVALID)
		, m_fChances(0.0f)
		{}

		fwMvClipSetId	m_ClipSetId;
		float			m_fChances;
	};
	static const int s_iMaxVariations = 5;
	Variation aVariations[s_iMaxVariations];
	int iNumVariations = 0;

	//Iterate over the variations.
	aiAssert(m_ClipSetsForAvoidVariations.GetCount() == sm_Tunables.m_Avoids.m_Variations.GetCount());
	for(int i = 0; i < m_ClipSetsForAvoidVariations.GetCount(); ++i)
	{
		//Ensure the casual state matches.
		const Tunables::Avoids::Variation& rVariation = sm_Tunables.m_Avoids.m_Variations[i];
		if(rVariation.m_IsCasual != bCasual)
		{
			continue;
		}

		//Check if we should use the clip set.
		if(!m_ClipSetsForAvoidVariations[i].ShouldUseClipSet())
		{
			continue;
		}

		//Set the clip set id and the chances.
		aVariations[iNumVariations].m_ClipSetId = m_ClipSetsForAvoidVariations[i].GetClipSetId();
		aVariations[iNumVariations].m_fChances	= rVariation.m_Chances;
		++iNumVariations;
		if(iNumVariations == s_iMaxVariations)
		{
			break;
		}
	}

	//Check if any variations were found.
	if(iNumVariations > 0)
	{
		//Sum up the chances.
		float fTotalChances = 0.0f;
		for(int i = 0; i < iNumVariations; ++i)
		{
			fTotalChances += aVariations[i].m_fChances;
		}

		//Ensure the total is at least 1.
		fTotalChances = Max(1.0f, fTotalChances);

		//Choose a random number.
		float fRandom = fwRandom::GetRandomNumberInRange(0.0f, fTotalChances);

		//Choose a variation.
		float fAccumulation = 0.0f;
		for(int i = 0; i < iNumVariations; ++i)
		{
			fAccumulation += aVariations[i].m_fChances;
			if(fRandom < fAccumulation)
			{
				return aVariations[i].m_ClipSetId;
			}
		}
	}

	//We were unable to choose a variation.

	//Check if the avoid is casual.
	if(bCasual)
	{
		//Check if we should use the clip set for casual.
		if(m_ClipSetRequestHelperForCasualAvoids.ShouldUseClipSet())
		{
			return m_ClipSetRequestHelperForCasualAvoids.GetClipSetId();
		}
	}

	return fwMvClipSetId(sm_Tunables.m_Avoids.m_ClipSet);
}

void CSituationalClipSetStreamer::Init(unsigned UNUSED_PARAM(initMode))
{
	//Create the instance.
	Assert(!sm_Instance);
	sm_Instance = rage_new CSituationalClipSetStreamer;
}

void CSituationalClipSetStreamer::Shutdown(unsigned UNUSED_PARAM(shutdownMode))
{
	//Free the instance.
	Assert(sm_Instance);
	delete sm_Instance;
	sm_Instance = NULL;
}

void CSituationalClipSetStreamer::Update()
{
	//Grab the time step.
	float fTimeStep = fwTimer::GetTimeStep();

	//Update the instance.
	GetInstance().Update(fTimeStep);
}

bool CSituationalClipSetStreamer::HasBeenInCombatForTime(float fTime) const
{
	//Ensure we are in combat.
	if(!IsInCombat())
	{
		return false;
	}

	//Ensure we have been in combat for the time specified.
	float fTimeInCombat = CGameSituation::GetInstance().GetTimeInCombat();
	if(fTimeInCombat < fTime)
	{
		return false;
	}

	return true;
}

bool CSituationalClipSetStreamer::IsInCombat() const
{
	//Ensure the game situation has the combat flag set.
	if(!CGameSituation::GetInstance().GetFlags().IsFlagSet(CGameSituation::Combat))
	{
		return false;
	}

	return true;
}

bool CSituationalClipSetStreamer::IsPedArmedWithGun(const CPed& rPed) const
{
	//Ensure the weapon manager is valid.
	const CPedWeaponManager* pWeaponManager = rPed.GetWeaponManager();
	if(!pWeaponManager)
	{
		return false;
	}

	//Ensure the ped is armed with a gun.
	if(!pWeaponManager->GetIsArmedGun())
	{
		return false;
	}

	return true;
}

bool CSituationalClipSetStreamer::IsPlayerArmedWithGun() const
{
	//Ensure the local player is valid.
	const CPed* pPed = CGameWorld::FindLocalPlayer();
	if(!pPed)
	{
		return false;
	}

	//Ensure the local player is armed with a gun.
	if(!IsPedArmedWithGun(*pPed))
	{
		return false;
	}

	return true;
}

bool CSituationalClipSetStreamer::IsPlayerOnFoot() const
{
	//Ensure the local player is valid.
	const CPed* pPed = CGameWorld::FindLocalPlayer();
	if(!pPed)
	{
		return false;
	}

	//Ensure the local player is on foot.
	if(!pPed->GetIsOnFoot())
	{
		return false;
	}

	return true;
}

bool CSituationalClipSetStreamer::ShouldStreamInClipSetForFleeReactions() const
{
	//Ensure we have not been in combat for a while.
	float fTime = sm_Tunables.m_FleeReactions.m_MinTimeInCombatToNotStreamIn;
	if(HasBeenInCombatForTime(fTime))
	{
		return false;
	}

	return true;
}

bool CSituationalClipSetStreamer::ShouldStreamInClipSetsForCasualAvoids() const
{
	//Ensure the player is on foot.
	if(!IsPlayerOnFoot())
	{
		return false;
	}

	return true;
}

void CSituationalClipSetStreamer::Update(float UNUSED_PARAM(fTimeStep))
{
	//Update the clip sets.
	UpdateClipSets();

	//Update the ped variations.
	UpdatePedVariations();
}

void CSituationalClipSetStreamer::UpdateClipSetForFleeReactions()
{
	//Check if we should stream in the clip set for flee reactions.
	if(ShouldStreamInClipSetForFleeReactions())
	{
		if(aiVerifyf(sm_Tunables.m_FleeReactions.m_ClipSetForIntro.IsNotNull(), "The flee reactions intro clip set is invalid."))
		{
			m_ClipSetRequestHelperForFleeReactionsIntro.RequestClipSet(fwMvClipSetId(sm_Tunables.m_FleeReactions.m_ClipSetForIntro));
		}

		if(aiVerifyf(sm_Tunables.m_FleeReactions.m_ClipSetForIntroV1.IsNotNull(), "The flee reactions intro clip (v1) set is invalid."))
		{
			m_ClipSetRequestHelperForFleeReactionsIntroV1.RequestClipSet(fwMvClipSetId(sm_Tunables.m_FleeReactions.m_ClipSetForIntroV1));
		}

		if(aiVerifyf(sm_Tunables.m_FleeReactions.m_ClipSetForIntroV2.IsNotNull(), "The flee reactions intro clip set (v2) is invalid."))
		{
			m_ClipSetRequestHelperForFleeReactionsIntroV2.RequestClipSet(fwMvClipSetId(sm_Tunables.m_FleeReactions.m_ClipSetForIntroV2));
		}

		if(aiVerifyf(sm_Tunables.m_FleeReactions.m_ClipSetForRuns.IsNotNull(), "The flee reactions runs clip set is invalid."))
		{
			m_ClipSetRequestHelperForFleeReactionsRuns.RequestClipSet(fwMvClipSetId(sm_Tunables.m_FleeReactions.m_ClipSetForRuns));
		}

		if(aiVerifyf(sm_Tunables.m_FleeReactions.m_ClipSetForRunsV1.IsNotNull(), "The flee reactions runs clip set (v1) is invalid."))
		{
			m_ClipSetRequestHelperForFleeReactionsRunsV1.RequestClipSet(fwMvClipSetId(sm_Tunables.m_FleeReactions.m_ClipSetForRunsV1));
		}

		if(aiVerifyf(sm_Tunables.m_FleeReactions.m_ClipSetForRunsV2.IsNotNull(), "The flee reactions runs clip set (v2) is invalid."))
		{
			m_ClipSetRequestHelperForFleeReactionsRunsV2.RequestClipSet(fwMvClipSetId(sm_Tunables.m_FleeReactions.m_ClipSetForRunsV2));
		}
	}
	else
	{
		m_ClipSetRequestHelperForFleeReactionsIntro.ReleaseClipSet();
		m_ClipSetRequestHelperForFleeReactionsIntroV1.ReleaseClipSet();
		m_ClipSetRequestHelperForFleeReactionsIntroV2.ReleaseClipSet();
		m_ClipSetRequestHelperForFleeReactionsRuns.ReleaseClipSet();
		m_ClipSetRequestHelperForFleeReactionsRunsV1.ReleaseClipSet();
		m_ClipSetRequestHelperForFleeReactionsRunsV2.ReleaseClipSet();
	}
}

void CSituationalClipSetStreamer::UpdateClipSets()
{
	//Update the clip set for flee reactions.
	UpdateClipSetForFleeReactions();

	//Update the clip set for avoids.
	UpdateClipSetsForAvoids();
}

void CSituationalClipSetStreamer::UpdateClipSetsForAvoids()
{
	//Check if we should stream in the clip set for casual.
	bool bStreamInClipSetsForCasual = ShouldStreamInClipSetsForCasualAvoids();

	//Stream in or out the clip set for casual.
	if(bStreamInClipSetsForCasual)
	{
		m_ClipSetRequestHelperForCasualAvoids.RequestClipSet(fwMvClipSetId(sm_Tunables.m_Avoids.m_ClipSetForCasual));
	}
	else
	{
		m_ClipSetRequestHelperForCasualAvoids.ReleaseClipSet();
	}

	//Iterate over the variations.
	aiAssert(m_ClipSetsForAvoidVariations.GetCount() == sm_Tunables.m_Avoids.m_Variations.GetCount());
	for(int i = 0; i < m_ClipSetsForAvoidVariations.GetCount(); ++i)
	{
		//Check if we should stream in the clip set.
		const Tunables::Avoids::Variation& rVariation = sm_Tunables.m_Avoids.m_Variations[i];
		bool bStreamInClipSet = ((rVariation.m_Chances > 0.0f) &&
			(!rVariation.m_IsCasual || bStreamInClipSetsForCasual));
		if(bStreamInClipSet)
		{
			m_ClipSetsForAvoidVariations[i].RequestClipSet(fwMvClipSetId(rVariation.m_ClipSet));
		}
		else
		{
			m_ClipSetsForAvoidVariations[i].ReleaseClipSet();
		}
	}
}

void CSituationalClipSetStreamer::UpdateParachutePackVariation()
{
	//Ensure the local player is valid.
	CPed* pLocalPlayer = CGameWorld::FindLocalPlayer();
	if(!pLocalPlayer)
	{
		return;
	}

	//Check if we should stream.
	bool bStream = (CTaskParachute::DoesPedHaveParachute(*pLocalPlayer) &&
		(pLocalPlayer->GetIsInVehicle() && pLocalPlayer->GetVehiclePedInside()->GetIsAircraft()));
	if(bStream)
	{
		//Check if we are not streamed in.
		if(!m_uRunningFlags.IsFlagSet(RF_HasRequestedParachutePackVariation))
		{
			//Request the variation.
			CTaskParachute::RequestStreamingOfParachutePackVariationForPed(*pLocalPlayer);

			//Set the flag.
			m_uRunningFlags.SetFlag(RF_HasRequestedParachutePackVariation);
		}
	}
	else
	{
		//Check if we have requested the parachute pack variation.
		if(m_uRunningFlags.IsFlagSet(RF_HasRequestedParachutePackVariation))
		{
			//Check if the time is not set.
			if(m_uTimeToReleaseParachutePackVariation == 0)
			{
				//Set the time.
				static dev_u32 s_uTime = 10000;
				m_uTimeToReleaseParachutePackVariation = fwTimer::GetTimeInMilliseconds() + s_uTime;
			}
			//Check if the time has expired.
			else if(fwTimer::GetTimeInMilliseconds() > m_uTimeToReleaseParachutePackVariation)
			{
				//Clear the timer.
				m_uTimeToReleaseParachutePackVariation = 0;

				//Release the variation.
				CTaskParachute::ReleaseStreamingOfParachutePackVariationForPed(*pLocalPlayer);

				//Clear the flag.
				m_uRunningFlags.ClearFlag(RF_HasRequestedParachutePackVariation);
			}
		}
	}
}

void CSituationalClipSetStreamer::UpdatePedVariations()
{
	//Update the parachute pack variation.
	UpdateParachutePackVariation();
}
