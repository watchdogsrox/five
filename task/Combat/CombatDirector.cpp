// File header
#include "Task/Combat/CombatDirector.h"

// Game headers
#include "audio/scannermanager.h"
#include "ai/EntityScanner.h"
#include "event/EventGroup.h"
#include "event/Events.h"
#include "Peds/ped.h"
#include "Peds/PedIntelligence.h"
#include "Network/NetworkInterface.h"
#include "scene/world/GameWorld.h"
#include "Task/Combat/CombatSituation.h"
#include "Task/Combat/TaskNewCombat.h"
#include "task/Combat/TaskThreatResponse.h"
#include "task/Combat/Subtasks/TaskAdvance.h"
#include "weapons/info/WeaponInfoManager.h"
#include "weapons/inventory/PedWeaponManager.h"

AI_OPTIMISATIONS()

extern audScannerManager g_AudioScannerManager;

//////////////////////////////////////////////////////////////////////////
// CCombatDirector
//////////////////////////////////////////////////////////////////////////

FW_INSTANTIATE_CLASS_POOL(CCombatDirector, CONFIGURED_FROM_FILE, atHashString("CCombatDirector",0x7e653152));

namespace
{
	const float ACTION_DROP_PER_SECOND				= 0.1f;
	const float ACTION_FOR_GUN_SHOT					= 0.05f;
	const float ACTION_FOR_GUN_SHOT_BULLET_IMPACT	= 0.025f;
	const float ACTION_FOR_GUN_SHOT_WHIZZED_BY		= 0.025f;
	const float TIME_TO_LIVE						= 10.0f;

	const ScalarV MAX_DIST_SQ	= ScalarVFromF32(square(100.0f));
}

CCombatDirector::CCombatDirector()
: fwRefAwareBase()
, m_aPeds()
, m_aPedKillTimes()
, m_pCombatSituation(NULL)
, m_pLastPedKilled(NULL)
, m_fAction(0.0f)
, m_uActionFlags(0)
, m_uNumPedsWith1HWeapons(0)
, m_uNumPedsWith2HWeapons(0)
, m_uNumPedsWithGuns(0)
{
	//Change to the standard situation.
	ChangeSituation(rage_new CCombatSituationNormal());

	//Initialise cover variations
	m_CoverClipVariationHelper.Init();
}

CCombatDirector::~CCombatDirector()
{
	//Clear the situation.
	ChangeSituation(NULL, false);
}

void CCombatDirector::Activate(CPed& rPed)
{
	//Find the index.
	const int iIndex = FindIndex(rPed);
	if(iIndex < 0)
	{
		return;
	}
	
	//Ensure the ped is inactive.
	PedInfo& rInfo = m_aPeds[iIndex];
	if(rInfo.m_bActive)
	{
		return;
	}
	
	//Activate the ped.
	rInfo.m_bActive = true;
	rInfo.m_fInactiveTimer = 0.0f;
	
	//Note that the ped has been activated.
	OnActivate(rPed);
}

void CCombatDirector::ChangeSituation(CCombatSituation* pCombatSituation, const bool ASSERT_ONLY(bAssertIfInvalid))
{
	Assertf(!bAssertIfInvalid || pCombatSituation, "Combat situation is invalid.");
	Assertf(!bAssertIfInvalid || (pCombatSituation != m_pCombatSituation), "Combat situation is not changing.");
	
	//Ensure the situation is changing.
	if(pCombatSituation == m_pCombatSituation)
	{
		return;
	}
	
	//Uninitialize the situation.
	UninitializeSituation();

	//Free the old situation.
	delete m_pCombatSituation;

	//Assign the situation.
	m_pCombatSituation = pCombatSituation;

	//Initialize the situation.
	InitializeSituation();
}

int CCombatDirector::CountPedsAdvancingWithSeek(const CEntity& rTarget, int iMaxCount) const
{
	//Keep track of the count.
	int iCount = 0;

	//Iterate over the peds.
	int iNumPeds = m_aPeds.GetCount();
	for(int i = 0; i < iNumPeds; ++i)
	{
		//Ensure the ped is active.
		const PedInfo& rInfo = m_aPeds[i];
		if(!rInfo.m_bActive)
		{
			continue;
		}

		//Ensure the ped is valid.
		CPed* pPed = rInfo.m_pPed;
		if(!pPed)
		{
			continue;
		}

		//Ensure the ped is alive.
		if(pPed->IsInjured())
		{
			continue;
		}

		//Ensure the ped is advancing.
		const CTaskAdvance* pTask = static_cast<CTaskAdvance *>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_ADVANCE));
		if(!pTask)
		{
			continue;
		}

		//Ensure the target matches.
		if(&rTarget != pTask->GetTargetPed())
		{
			continue;
		}

		//Ensure the task is seeking.
		if(!pTask->IsSeeking())
		{
			continue;
		}

		//Increase the count.
		++iCount;
		if((iMaxCount >= 0) && (iCount >= iMaxCount))
		{
			break;
		}
	}

	return iCount;
}

void CCombatDirector::Deactivate(CPed& rPed)
{
	//Find the index.
	const int iIndex = FindIndex(rPed);
	if(iIndex < 0)
	{
		return;
	}

	//Ensure the ped is active.
	PedInfo& rInfo = m_aPeds[iIndex];
	if(!rInfo.m_bActive)
	{
		return;
	}

	//Deactivate the ped.
	rInfo.m_bActive = false;
	rInfo.m_fInactiveTimer = 0.0f;
	
	//Note that the ped has been deactivated.
	OnDeactivate(rPed);
}

bool CCombatDirector::DoesAnyoneHaveClearLineOfSight(const CEntity& rTarget) const
{
	//Iterate over the peds.
	int iCount = m_aPeds.GetCount();
	for(int i = 0; i < iCount; ++i)
	{
		//Ensure the ped is active.
		const PedInfo& rInfo = m_aPeds[i];
		if(!rInfo.m_bActive)
		{
			continue;
		}

		//Ensure the ped is valid.
		CPed* pPed = rInfo.m_pPed;
		if(!pPed)
		{
			continue;
		}

		//Ensure the ped is alive.
		if(pPed->IsInjured())
		{
			continue;
		}

		//Ensure the targeting is valid.
		CPedTargetting* pTargeting = pPed->GetPedIntelligence()->GetTargetting(false);
		if(!pTargeting)
		{
			continue;
		}

		//Ensure the line of sight is clear.
		if(pTargeting->GetLosStatus(&rTarget, false) != Los_clear)
		{
			continue;
		}

		return true;
	}

	return false;
}

bool CCombatDirector::DoesPedHaveGun(const CPed& rPed) const
{
	//Ensure the weapon manager is valid.
	const CPedWeaponManager* pWeaponManager = rPed.GetWeaponManager();
	if(!pWeaponManager)
	{
		return false;
	}

	//Ensure the weapon is valid.
	const CWeapon* pWeapon = pWeaponManager->GetEquippedWeapon();
	if(!pWeapon)
	{
		return false;
	}

	return pWeapon->GetWeaponInfo()->GetIsGun();
}

int CCombatDirector::GetNumPeds() const
{
	return m_aPeds.GetCount();
}

u32 CCombatDirector::GetNumPedsKilledWithinTime(const float fTime) const
{
	//Generate the time in MS.
	u32 uTime = (u32)(fTime * 1000.0f);

	//Count the peds killed within the time period.
	//Count in reverse order, which will take us from
	//the most recent ped killed to the oldest.
	u32 uNumPedsKilled = 0;
	int iCount = m_aPedKillTimes.GetCount();
	for(int i = iCount - 1; i >= 0; --i)
	{
		if((m_aPedKillTimes[i] + uTime) < fwTimer::GetTimeInMilliseconds())
		{
			break;
		}

		++uNumPedsKilled;
	}

	return uNumPedsKilled;
}

CPed* CCombatDirector::GetPed(const int iIndex) const
{
	return m_aPeds[iIndex].m_pPed;
}

bool CCombatDirector::IsFull() const
{
	return m_aPeds.IsFull();
}

void CCombatDirector::OnGunShot(const CEventGunShot& UNUSED_PARAM(rEvent))
{
	//Set the action flag.
	m_uActionFlags.SetFlag(CDAF_GunShot);
}

void CCombatDirector::OnGunShotBulletImpact(const CEventGunShotBulletImpact& UNUSED_PARAM(rEvent))
{
	//Set the action flag.
	m_uActionFlags.SetFlag(CDAF_GunShotBulletImpact);
}

void CCombatDirector::OnGunShotWhizzedBy(const CEventGunShotWhizzedBy& UNUSED_PARAM(rEvent))
{
	//Set the action flag.
	m_uActionFlags.SetFlag(CDAF_GunShotWhizzedBy);
}

void CCombatDirector::Update(const float fTimeStep)
{
	//Update the peds.
	UpdatePeds(fTimeStep);
	
	//Update the action.
	UpdateAction(fTimeStep);
	
	//Update the situation.
	UpdateSituation(fTimeStep);

	//Update the weapon stats.
	UpdateWeaponStats();

	//Update the weapon streaming.
	UpdateWeaponStreaming();

	//Update the coordinated shooting.
	UpdateCoordinatedShooting(fTimeStep);
}

CCombatDirector* CCombatDirector::FindNearbyCombatDirector(const CPed& rPed, CCombatDirector* pException)
{
	//Scan the surrounding peds.
	const CEntityScannerIterator entityList = rPed.GetPedIntelligence()->GetNearbyPeds();
	for(const CEntity* pEnt = entityList.GetFirst(); pEnt != NULL; pEnt = entityList.GetNext())
	{
		//Grab the nearby ped.
		const CPed* pNearbyPed = static_cast<const CPed*>(pEnt);
		
		//Ensure the nearby combat director is valid.
		CCombatDirector* pCombatDirector = pNearbyPed->GetPedIntelligence()->GetCombatDirector();
		if(!pCombatDirector)
		{
			continue;
		}

		//Ensure the nearby combat director is not the exception.
		if(pCombatDirector == pException)
		{
			continue;
		}

		//Ensure the combat director is not full.
		if(pCombatDirector->IsFull())
		{
			continue;
		}

		//Ensure the ped is friendly.
		if(!pNearbyPed->GetPedIntelligence()->IsFriendlyWith(rPed) || !rPed.GetPedIntelligence()->IsFriendlyWith(*pNearbyPed))
		{
			continue;
		}

		//Ensure the ped is within the distance threshold.
		ScalarV scDistSq = DistSquared(rPed.GetTransform().GetPosition(), pNearbyPed->GetTransform().GetPosition());
		if(IsGreaterThanAll(scDistSq, MAX_DIST_SQ))
		{
			break;
		}
		
		return pCombatDirector;
	}
	
	return NULL;
}

void CCombatDirector::Init(unsigned UNUSED_PARAM(initMode))
{
	//Init the pools.
	CCombatDirector::InitPool(MEMBUCKET_GAMEPLAY);
	CCombatSituation::InitPool(MEMBUCKET_GAMEPLAY);
}

void CCombatDirector::RefreshCombatDirector(CPed& rPed)
{
	//Grab the ped's combat director.
	CCombatDirector* pCombatDirector = rPed.GetPedIntelligence()->GetCombatDirector();
	
	//Ensure the nearby combat director is valid.
	//Exclude the current combat director.
	CCombatDirector* pNearbyCombatDirector = FindNearbyCombatDirector(rPed, pCombatDirector);
	if(!pNearbyCombatDirector)
	{
		return;
	}
	
	//Ensure the nearby combat director is different.
	if(pCombatDirector == pNearbyCombatDirector)
	{
		return;
	}
	
	//Ensure the nearby combat director's pointer is less than the current combat director.
	//This should help stop combat directors from oscillating back and forth if there are two large groups.
	//The combat directors will converge on the one with the lowest pointer.
	//Of course, this only applies if the current combat director is valid.
	if(pCombatDirector && (pNearbyCombatDirector >= pCombatDirector))
	{
		return;
	}
	
	//Remove the ped from the combat director.
	if(pCombatDirector)
	{
		pCombatDirector->RemovePed(rPed);
	}
	
	//Add the ped to the nearby combat director.
	if(Verifyf(pNearbyCombatDirector->AddPed(rPed), "Couldn't add ped to combat director."))
	{
		//Activate the ped.
		pNearbyCombatDirector->Activate(rPed);
	}
}

void CCombatDirector::ReleaseCombatDirector(CPed& rPed)
{
	//Ensure the combat director is valid.
	CCombatDirector* pCombatDirector = rPed.GetPedIntelligence()->GetCombatDirector();
	if(!pCombatDirector)
	{
		return;
	}
	
	//Deactivate the ped.
	pCombatDirector->Deactivate(rPed);
}

void CCombatDirector::RequestCombatDirector(CPed& rPed)
{
	//Grab the combat director.
	CCombatDirector* pCombatDirector = rPed.GetPedIntelligence()->GetCombatDirector();
	if(!pCombatDirector)
	{
		//Find a nearby combat director.
		pCombatDirector = FindNearbyCombatDirector(rPed);
		if(!pCombatDirector)
		{
			//A valid combat director could not be found.
			//Ensure there is enough space in the pool to create one.
			if(CCombatDirector::GetPool()->GetNoOfFreeSpaces() == 0)
			{
				//No need to assert here, it is likely during dispatch that we will run out of combat directors.
				//When they converge to a location, the combat directors will converge as well.
				return;
			}

			//Create a new combat director.
			pCombatDirector = rage_new CCombatDirector();
		}

		//Add the ped.
		if(!Verifyf(pCombatDirector->AddPed(rPed), "Couldn't add ped to combat director."))
		{
			//This will not leak, the combat director will be freed in the update since there is no one referencing it.
			return;
		}
	}

	//Activate the ped.
	pCombatDirector->Activate(rPed);
}

void CCombatDirector::Shutdown(unsigned UNUSED_PARAM(shutdownMode))
{
	//Shut down the pools.
	CCombatDirector::ShutdownPool();
	CCombatSituation::ShutdownPool();
}

void CCombatDirector::UpdateCombatDirectors()
{
	//Ensure the game is not paused.
	if(fwTimer::IsGamePaused())
	{
		return;
	}

	//Grab the pool of directors.
	CCombatDirector::Pool* pPool = CCombatDirector::GetPool();
	
	//Grab the time step.
	const float fTimeStep = fwTimer::GetTimeStep();
	
	//Update the active directors.
	s32 iCount = pPool->GetSize();
	for(s32 i = 0; i < iCount; ++i)
	{
		//Grab the director.
		CCombatDirector* pCombatDirector = pPool->GetSlot(i);
		if(!pCombatDirector)
		{
			continue;
		}
		
		//Update the director.
		pCombatDirector->Update(fTimeStep);
		
		//Ensure the director still has references.
		if(pCombatDirector->IsReferenced())
		{
			continue;
		}
		
		//Free the director.
		delete pCombatDirector;
	}
}

bool CCombatDirector::AddPed(CPed& rPed)
{
	//Ensure the combat director is not full.
	if(IsFull())
	{
		return false;
	}
	
	//Assign the ped.
	m_aPeds.Append().m_pPed = &rPed;
	
	//Note that the ped was added.
	OnAdd(rPed);
	
	//Set the combat director.
	rPed.GetPedIntelligence()->SetCombatDirector(this);
	
	return true;
}

int CCombatDirector::FindIndex(const CPed& rPed) const
{
	//Find the index of the ped.
	int iCount = m_aPeds.GetCount();
	for(int i = 0; i < iCount; ++i)
	{
		if(&rPed == m_aPeds[i].m_pPed)
		{
			return i;
		}
	}
	
	return -1;
}

CPed* CCombatDirector::FindLeaderForCoordinatedShooting() const
{
	//Iterate over the peds.
	int iCount = m_aPeds.GetCount();
	for(int i = 0; i < iCount; ++i)
	{
		//Ensure the index is active.
		if(!m_aPeds[i].m_bActive)
		{
			continue;
		}

		//Ensure the ped is valid.
		CPed* pPed = m_aPeds[i].m_pPed;
		if(!pPed)
		{
			continue;
		}

		//Ensure the ped is law-enforcement.
		if(!pPed->IsLawEnforcementPed())
		{
			continue;
		}

		//Ensure the ped is not talking.
		if(!pPed->GetSpeechAudioEntity() || pPed->GetSpeechAudioEntity()->IsAmbientSpeechPlaying())
		{
			continue;
		}

		//Ensure the ped is close to the player.
		ScalarV scDistSq = DistSquared(pPed->GetTransform().GetPosition(),
			VECTOR3_TO_VEC3V(CGameWorld::FindLocalPlayerCoors()));
		static dev_float s_fMaxDistance = 30.0f;
		ScalarV scMaxDistSq = ScalarVFromF32(square(s_fMaxDistance));
		if(IsGreaterThanAll(scDistSq, scMaxDistSq))
		{
			continue;
		}

		return pPed;
	}

	return NULL;
}

void CCombatDirector::InitializeSituation()
{
	//Ensure the situation is valid.
	if(!m_pCombatSituation)
	{
		return;
	}
	
	//Initialize the situation.
	m_pCombatSituation->Initialize(*this);
}

void CCombatDirector::OnActivate(CPed& rPed)
{
	//Note that the ped has been activated.
	m_pCombatSituation->OnActivate(*this, rPed);
}

void CCombatDirector::OnAdd(CPed& rPed)
{
	//Note that the ped has been added.
	m_pCombatSituation->OnAdd(*this, rPed);

	//Update the gun count.
	if(DoesPedHaveGun(rPed))
	{
		++m_uNumPedsWithGuns;
	}
}

void CCombatDirector::OnDeactivate(CPed& rPed)
{
	//Note that the ped has been deactivated.
	m_pCombatSituation->OnDeactivate(*this, rPed);
}

void CCombatDirector::OnPedKilled(const CPed& rPed)
{
	//Check if the ped kill times array is full.
	if(m_aPedKillTimes.IsFull())
	{
		//Remove the oldest kill time.
		m_aPedKillTimes.Pop();
	}

	//Note the time the ped was killed.
	m_aPedKillTimes.Push(fwTimer::GetTimeInMilliseconds());

	//Save the last ped to be killed.
	m_pLastPedKilled = &rPed;

	//Iterate over the nearby peds.
	int iNumPedsCheckedForAudio = 0;
	static dev_s32 s_iMaxPedsToCheckForAudio = 4;
	int iNumPedsTriedToSayAudio = 0;
	static dev_s32 s_iMaxPedsToTryToSayAudio = 2;
	CEntityScannerIterator entityList = rPed.GetPedIntelligence()->GetNearbyPeds();
	for(CEntity* pEnt = entityList.GetFirst();
		(pEnt != NULL) && (iNumPedsCheckedForAudio < s_iMaxPedsToCheckForAudio) && (iNumPedsTriedToSayAudio < s_iMaxPedsToTryToSayAudio);
		pEnt = entityList.GetNext())
	{
		//Increase the number of peds checked.
		++iNumPedsCheckedForAudio;

		//Grab the nearby ped.
		CPed* pNearbyPed = static_cast<CPed *>(pEnt);

		//Ensure the ped is alive.
		if(pNearbyPed->IsInjured())
		{
			continue;
		}

		//Ensure the ped has the same combat director.
		if(this != pNearbyPed->GetPedIntelligence()->GetCombatDirector())
		{
			continue;
		}

		//Increase the number of peds that tried to say audio.
		++iNumPedsTriedToSayAudio;

		//Keep track of whether we said something.
		bool bSaidSomething = false;

		//Check if a few peds have died in the last few seconds.
		static u32 s_uMinPedsKilled = 2;
		static float s_fMaxTime = 3.0f;
		if(GetNumPedsKilledWithinTime(s_fMaxTime) >= s_uMinPedsKilled)
		{
			g_AudioScannerManager.TriggerCopDispatchInteraction(SCANNER_CDIT_REQUEST_BACKUP);
			//bSaidSomething = pNearbyPed->NewSay("REQUEST_BACKUP");
		}

		if(!bSaidSomething)
		{
			const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(rPed.GetCauseOfDeath());
			if(pWeaponInfo && !pWeaponInfo->GetIsMelee())
				bSaidSomething = rPed.IsLawEnforcementPed() && pNearbyPed->IsLawEnforcementPed() ? pNearbyPed->NewSay("OFFICER_DOWN") : pNearbyPed->NewSay("BUDDY_DOWN");
		}

		if(bSaidSomething)
		{
			break;
		}
	}

	//Check if the ped was killed with a scary weapon.
	if(WasPedKilledWithScaryWeapon(rPed))
	{
		//Iterate over the nearby peds.
		int iNumPedsCheckedForFlee = 0;
		static dev_s32 s_iMaxPedsToCheckForFlee = 4;
		for(CEntity* pEnt = entityList.GetFirst();
			(pEnt != NULL) && (iNumPedsCheckedForFlee < s_iMaxPedsToCheckForFlee);
			pEnt = entityList.GetNext())
		{
			//Increase the number of peds checked.
			++iNumPedsCheckedForFlee;

			//Ensure the ped is alive.
			CPed* pNearbyPed = static_cast<CPed *>(pEnt);
			if(pNearbyPed->IsInjured())
			{
				continue;
			}

			//Ensure the ped is ambient.
			if(!pNearbyPed->PopTypeIsRandom())
			{
				continue;
			}

			//Ensure the ped has the same combat director.
			if(this != pNearbyPed->GetPedIntelligence()->GetCombatDirector())
			{
				continue;
			}

			//Ensure flee during combat is enabled.
			const CPedModelInfo::PersonalityFleeDuringCombat& rInfo = pNearbyPed->GetPedModelInfo()->GetPersonalitySettings().GetFleeDuringCombat();
			if(!rInfo.m_Enabled)
			{
				continue;
			}

			//Enforce the chances.
			if(fwRandom::GetRandomNumberInRange(0.0f, 1.0f) >= rInfo.m_ChancesWhenBuddyKilledWithScaryWeapon)
			{
				continue;
			}

			//Ensure the task is valid.
			CTaskThreatResponse* pTask = static_cast<CTaskThreatResponse *>(
				pNearbyPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_THREAT_RESPONSE));
			if(!pTask)
			{
				continue;
			}

			//Flee from combat.
			pTask->SetFleeFromCombat(true);
		}
	}
}

void CCombatDirector::OnRemove(CPed& rPed)
{
	//Note that the ped has been removed.
	m_pCombatSituation->OnRemove(*this, rPed);
}

bool CCombatDirector::RemoveIndex(const int iIndex)
{
	//Ensure the index is valid.
	if(iIndex < 0 || iIndex >= m_aPeds.GetCount())
	{
		return false;
	}

	//Delete the index.
	m_aPeds.Delete(iIndex);

	//Note that the index was removed.
	return true;
}

bool CCombatDirector::RemovePed(CPed& rPed)
{
	//Remove the ped.
	if(!RemoveIndex(FindIndex(rPed)))
	{
		return false;
	}
	
	//Note that the ped was removed.
	OnRemove(rPed);
	
	//Clear the combat director.
	rPed.GetPedIntelligence()->SetCombatDirector(NULL);
	
	return true;
}

void CCombatDirector::UninitializeSituation()
{
	//Ensure the combat situation is valid.
	if(!m_pCombatSituation)
	{
		return;
	}
	
	//Uninitialize the situation.
	m_pCombatSituation->Uninitialize(*this);
}

void CCombatDirector::UpdateAction(const float fTimeStep)
{
	//Increase the action based on the action flags.
	if(m_uActionFlags.IsFlagSet(CDAF_GunShot))
	{
		m_fAction += ACTION_FOR_GUN_SHOT;
	}
	if(m_uActionFlags.IsFlagSet(CDAF_GunShotWhizzedBy))
	{
		m_fAction += ACTION_FOR_GUN_SHOT_WHIZZED_BY;
	}
	if(m_uActionFlags.IsFlagSet(CDAF_GunShotBulletImpact))
	{
		m_fAction += ACTION_FOR_GUN_SHOT_BULLET_IMPACT;
	}
	
	//Clear the action flags.
	m_uActionFlags.ClearAllFlags();
	
	//Decrease the action a bit.
	m_fAction -= (ACTION_DROP_PER_SECOND * fTimeStep);
	
	//Clamp the action.
	m_fAction = Clamp(m_fAction, 0.0f, 1.0f);
}

CPed* CCombatDirector::GetPlayerPedIsArresting(const CPed* pPed)
{
	if(!pPed->GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_CanBust))
	{
		return NULL;
	}

	if(!pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_COMBAT, true))
	{
		return NULL;
	}

	s32 iCombatState = pPed->GetPedIntelligence()->GetQueriableInterface()->GetStateForTaskType(CTaskTypes::TASK_COMBAT);
	if( !pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_WillArrestRatherThanJack) &&
		iCombatState != CTaskCombat::State_GoToEntryPoint && iCombatState != CTaskCombat::State_ArrestTarget)
	{
		return NULL;
	}

	CEntity* pTarget = pPed->GetPedIntelligence()->GetQueriableInterface()->GetHostileTarget();
	if(!pTarget || !pTarget->GetIsTypePed())
	{
		return NULL;
	}

	CPed* pTargetPed = static_cast<CPed*>(pTarget);
	if(!pTargetPed->IsAPlayerPed())
	{
		return NULL;
	}

	CWanted* pWanted = pTargetPed->GetPlayerWanted();
	if(pWanted->GetWantedLevel() > WANTED_LEVEL1)
	{
		ScalarV scDistSq = DistSquared(pPed->GetTransform().GetPosition(), pTargetPed->GetTransform().GetPosition());
		ScalarV scMaxDistSq = LoadScalar32IntoScalarV(CTaskCombat::ms_Tunables.m_MaxDistanceToHoldFireForArrest);
		scMaxDistSq = (scMaxDistSq * scMaxDistSq);
		if(IsGreaterThanAll(scDistSq, scMaxDistSq))
		{
			return NULL;
		}
	}

	return pTargetPed;
}

void CCombatDirector::UpdateCoordinatedShooting(float fTimeStep)
{
	//Check the state.
	CoordinatedShooting::State nState = m_CoordinatedShooting.m_nState;
	switch(nState)
	{
		case CoordinatedShooting::Start:
		{
			//Set the timer.
			static dev_float s_fMinTime = 30.0f;
			static dev_float s_fMaxTime = 90.0f;
			m_CoordinatedShooting.m_fTimeToWait = fwRandom::GetRandomNumberInRange(s_fMinTime, s_fMaxTime);

			//Set the state.
			m_CoordinatedShooting.m_nState = CoordinatedShooting::WaitBeforeReady;
			break;
		}
		case CoordinatedShooting::WaitBeforeReady:
		{
#if __DEV
			TUNE_GROUP_BOOL(COMBAT_DIRECTOR, bForceReady, false);
			if(bForceReady)
			{
				m_CoordinatedShooting.m_fTimeToWait = 0.0f;
			}
#endif

			//Decrease the timer.
			m_CoordinatedShooting.m_fTimeToWait -= fTimeStep;
			if(m_CoordinatedShooting.m_fTimeToWait > 0.0f)
			{
				break;
			}

			//Choose a leader.
			m_CoordinatedShooting.m_pLeader = FindLeaderForCoordinatedShooting();
			if(!m_CoordinatedShooting.m_pLeader)
			{
				m_CoordinatedShooting.m_nState = CoordinatedShooting::Start;
				break;
			}

			//Say the audio
			CPed* pRespondingLawPed = NULL;
			if(m_CoordinatedShooting.m_pLeader->IsLawEnforcementPed())
			{
				CEntityScannerIterator entityList = m_CoordinatedShooting.m_pLeader->GetPedIntelligence()->GetNearbyPeds();
				for( CEntity* pEnt = entityList.GetFirst(); pEnt; pEnt = entityList.GetNext() )
				{
					CPed* pNearbyPed = static_cast<CPed*>(pEnt);
					if(pNearbyPed->IsLawEnforcementPed() && pNearbyPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_COMBAT, true))
					{
						pRespondingLawPed = pNearbyPed;
						break;
					}
				}
			}

			if(!m_CoordinatedShooting.m_pLeader->NewSay("SHOOTOUT_READY", 0, false, false, -1, pRespondingLawPed, ATSTRINGHASH("SHOOTOUT_READY_RESP", 0x39F80D16)))
			{
				m_CoordinatedShooting.m_nState = CoordinatedShooting::Start;
				break;
			}

			//Set the timer.
			static dev_float s_fMinTime = 3.0f;
			static dev_float s_fMaxTime = 4.0f;
			m_CoordinatedShooting.m_fTimeToWait = fwRandom::GetRandomNumberInRange(s_fMinTime, s_fMaxTime);

			//Set the state.
			m_CoordinatedShooting.m_nState = CoordinatedShooting::WaitBeforeShoot;
			break;
		}
		case CoordinatedShooting::WaitBeforeShoot:
		{
			//Decrease the timer.
			m_CoordinatedShooting.m_fTimeToWait -= fTimeStep;
			if(m_CoordinatedShooting.m_fTimeToWait > 0.0f)
			{
				break;
			}

			//Ensure the leader is valid.
			if(!m_CoordinatedShooting.m_pLeader)
			{
				m_CoordinatedShooting.m_nState = CoordinatedShooting::Start;
				break;
			}

			//Say the audio.
			if(!m_CoordinatedShooting.m_pLeader->NewSay("SHOOTOUT_OPEN_FIRE"))
			{
				m_CoordinatedShooting.m_nState = CoordinatedShooting::Start;
				break;
			}

			//Call for cover.
			CEventCallForCover event(m_CoordinatedShooting.m_pLeader);
			GetEventGlobalGroup()->Add(event);

			//Set the state.
			m_CoordinatedShooting.m_nState = CoordinatedShooting::Start;
			break;
		}
		default:
		{
			aiAssert(false);
			break;
		}
	}

	//Check if the state did not change.
	if(nState == m_CoordinatedShooting.m_nState)
	{
		m_CoordinatedShooting.m_fTimeInState += fTimeStep;
	}
	else
	{
		m_CoordinatedShooting.m_fTimeInState = 0.0f;
	}
}

void CCombatDirector::UpdatePeds(const float fTimeStep)
{
	// Keep track if any of our peds are arresting a target
	CPed* pPlayerBeingArrested = NULL;

	// keep track if we had any ped deaths
	bool bWasAnyPedKilled = false;

	//Update the peds.
	int iCount = m_aPeds.GetCount();
	for(int i = 0; i < iCount; ++i)
	{
		//Grab the info.
		PedInfo& rInfo = m_aPeds[i];
		
		//Check if the ped is inactive.
		if(!rInfo.m_bActive)
		{
			//Update the inactive time.
			rInfo.m_fInactiveTimer += fTimeStep;
		}
		
		//Peds can be removed for a variety of reasons:
		//	1) The ped is invalid.
		//	2) The ped is injured.
		//	3) The ped has been inactive for too long.
		bool bRemove = false;
		
		//Check if the ped is invalid.
		CPed* pPed = rInfo.m_pPed;
		if(!pPed)
		{
			//Remove the ped.
			bRemove = true;
		}
		//Check if the ped is injured.
		else if(pPed->IsInjured())
		{
			// yes we have had a ped die
			bWasAnyPedKilled = true;

			//Note that the ped has been killed.
			OnPedKilled(*pPed);
			
			//Remove the ped.
			bRemove = true;
		}
		//Check if the ped has been inactive for too long.
		else if(rInfo.m_fInactiveTimer > TIME_TO_LIVE)
		{
			//Remove the ped.
			bRemove = true;
		}
		
		//Ensure the ped should be removed.
		if(!bRemove)
		{
			if(!pPlayerBeingArrested)
			{
				pPlayerBeingArrested = GetPlayerPedIsArresting(pPed);
			}

			continue;
		}
		
		//Remove the ped.
		bool bRemoved = false;
		if(!pPed)
		{
			//Remove the index.
			bRemoved = RemoveIndex(i);
		}
		else
		{
			//Remove the ped.
			bRemoved = RemovePed(*pPed);
		}
		
		//Ensure the ped was removed.
		if(!bRemoved)
		{
			continue;
		}
		
		//Update the index and count.
		--i;
		--iCount;
	}

	//Update the peds.
	iCount = m_aPeds.GetCount();
	for(int i = 0; i < iCount; ++i)
	{
		//Grab the info.
		PedInfo& rInfo = m_aPeds[i];
		if(rInfo.m_pPed)
		{
			if(bWasAnyPedKilled)
			{
				rInfo.m_pPed->GetPedIntelligence()->IncreaseAmountPinnedDown(CTaskInCover::ms_Tunables.m_AmountPinnedDownByWitnessKill);
			}

			//Ensure the combat orders are valid.
			CCombatOrders* pCombatOrders = rInfo.m_pPed->GetPedIntelligence()->GetCombatOrders();
			if(pCombatOrders)
			{
				pCombatOrders->GetPassiveFlags().ClearFlag(CCombatOrders::PF_HoldFire);

				if( pPlayerBeingArrested &&
					rInfo.m_pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_COMBAT) &&
					rInfo.m_pPed->GetPedIntelligence()->GetQueriableInterface()->GetHostileTarget() == pPlayerBeingArrested )
				{
					pCombatOrders->GetPassiveFlags().SetFlag(CCombatOrders::PF_HoldFire);
				}
			}
		}
	}
}

void CCombatDirector::UpdateSituation(const float fTimeStep)
{
	//Update the situation.
	m_pCombatSituation->Update(*this, fTimeStep);
}

void CCombatDirector::UpdateWeaponStats()
{
	//Reset the weapon counts
	m_uNumPedsWith1HWeapons = 0;
	m_uNumPedsWith2HWeapons = 0;
	m_uNumPedsWithGuns = 0;

	//Update the peds.
	int iCount = m_aPeds.GetCount();
	for(int i = 0; i < iCount; ++i)
	{
		//Grab the info.
		PedInfo& rInfo = m_aPeds[i];
		if(rInfo.m_pPed)
		{
			//Grab the ped's weapon manager.
			const CPedWeaponManager* pWeaponManager = rInfo.m_pPed->GetWeaponManager();
			if (pWeaponManager)
			{
				//Grab the ped's equipped weapon info.
				const CWeaponInfo* pWeaponInfo = pWeaponManager->GetEquippedWeaponInfo();
				if (pWeaponInfo)
				{
					//Update the weapon counts.
					if (pWeaponInfo->GetIsTwoHanded())
					{
						++m_uNumPedsWith2HWeapons;
					}
					else if (pWeaponInfo)
					{
						++m_uNumPedsWith1HWeapons;
					}
				}
			}

			if(DoesPedHaveGun(*rInfo.m_pPed))
			{
				++m_uNumPedsWithGuns;
			}
		}
	}
}

void CCombatDirector::UpdateWeaponStreaming()
{
	//Update the cover streaming.
	m_CoverClipVariationHelper.UpdateClipVariationStreaming(m_uNumPedsWith1HWeapons, m_uNumPedsWith2HWeapons);

	//Stream in the buddy shot animations.
	if(m_uNumPedsWith1HWeapons > 0)
	{
		m_ClipSetRequestHelperForBuddyShotPistol.RequestClipSet(CLIP_SET_COMBAT_BUDDY_SHOT_PISTOL);
	}
	else
	{
		m_ClipSetRequestHelperForBuddyShotPistol.ReleaseClipSet();
	}
	if(m_uNumPedsWith2HWeapons > 0)
	{
		m_ClipSetRequestHelperForBuddyShotRifle.RequestClipSet(CLIP_SET_COMBAT_BUDDY_SHOT_RIFLE);
	}
	else
	{
		m_ClipSetRequestHelperForBuddyShotRifle.ReleaseClipSet();
	}
}

bool CCombatDirector::WasPedKilledWithScaryWeapon(const CPed& rPed) const
{
	//Check if the cause of death is scary.
	const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(rPed.GetCauseOfDeath());
	if(pWeaponInfo && pWeaponInfo->GetIsScary())
	{
		return true;
	}

	//Check if the source of death is a ped.
	const CEntity* pEntity = rPed.GetSourceOfDeath();
	if(pEntity && pEntity->GetIsTypePed())
	{
		//Check if the weapon manager is valid.
		const CPedWeaponManager* pWeaponManager = static_cast<const CPed *>(pEntity)->GetWeaponManager();
		if(pWeaponManager)
		{
			//Check if the equipped weapon info is scary.
			pWeaponInfo = pWeaponManager->GetEquippedWeaponInfo();
			if(pWeaponInfo && pWeaponInfo->GetIsScary())
			{
				return true;
			}
		}
	}

	return false;
}
