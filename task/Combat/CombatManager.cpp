// File header
#include "Task/Combat/CombatManager.h"

// Rage headers
#include "bank/bank.h"
#include "bank/bkmgr.h"
#include "math/angmath.h"

// Game headers
#include "Debug/DebugScene.h"
#include "Game/Dispatch/Incidents.h"
#include "Game/Dispatch/Orders.h"
#include "Peds/Ped.h"
#include "Peds/PedIntelligence.h"
#include "Scene/World/GameWorld.h"
#include "Task/Combat/TaskCombatMelee.h"
#include "Task/Combat/TaskCombatMounted.h"
#include "Task/Combat/TaskNewCombat.h"
#include "Task/Combat/TaskThreatResponse.h"

AI_OPTIMISATIONS()

//-----------------------------------------------------------------------------
CCombatManager *CCombatManager::sm_Instance	= NULL;

CCombatManager::CCombatManager()
{
#if __BANK
	m_bDebugRender = false;
#endif
}

CCombatManager::~CCombatManager()
{
}

void CCombatManager::InitClass( void )
{
	Assert(!sm_Instance);
	sm_Instance = rage_new CCombatManager;
}

void CCombatManager::ShutdownClass( void )
{
	Assert(sm_Instance);
	delete sm_Instance;
	sm_Instance = NULL;
}

void CCombatManager::Update( void )
{
	m_MountedCombatManager.Update();
	m_MeleeCombatManager.Update();
	m_CombatTaskManager.Update();
}

#if __BANK
void CCombatManager::AddWidgets( bkBank &bank )
{
	m_MountedCombatManager.AddWidgets( bank );
	m_MeleeCombatManager.AddWidgets( bank );
	m_CombatTaskManager.AddWidgets( bank );
}
#endif	// __BANK

#if !__FINAL
void CCombatManager::DebugRender( void )
{
#if __BANK
	m_MountedCombatManager.DebugRender();
	m_MeleeCombatManager.DebugRender();
	m_CombatTaskManager.DebugRender();
#endif	// __BANK
}
#endif	// !__FINAL

//-----------------------------------------------------------------------------

IMPLEMENT_COMBAT_TASK_TUNABLES(CCombatTaskManager, 0x9d23a8fb)
CCombatTaskManager::Tunables CCombatTaskManager::sm_Tunables;

const int CCombatTaskManager::sMaxPedsInCombatWithNearbyIncidents[WANTED_LEVEL_LAST] = { 0, 10, 12, 18, 24, 24 };
const int CCombatTaskManager::sMaxHelisInCombatWithNearbyIncidents[WANTED_LEVEL_LAST] = { 0, 2, 2, 2, 3, 3 };

#if __BANK
bool CCombatTaskManager::sm_bDebugDisplayPedsInCombatWithTarget = false;
bool CCombatTaskManager::sm_bDebugDisplay_MustHaveClearLOS = false;
bool CCombatTaskManager::sm_bDebugDisplay_MustHaveGunWeaponEquipped = false;
#endif // __BANK

int CCombatTaskManager::CountPedsInCombatWithTarget(const CPed &pTargetPed, fwFlags8 uOptionFlags, const Vec3V* pvSearchCenter, const ScalarV* scSearchRadiusSq)
{
	int iCount = 0;
	for( int i = 0; i < m_PedList.GetCount(); i++ )
	{
		const CPed* pPed = m_PedList[i];
		if(!pPed)
		{
			continue;
		}

		if( uOptionFlags.IsFlagSet(OF_MustBeLawEnforcement) && !pPed->IsLawEnforcementPed() )
		{
			continue;
		}

		if( uOptionFlags.IsFlagSet(OF_MustHaveGunWeaponEquipped) && !HelperPedHasWeaponEquipped(pPed) )
		{
			continue;
		}

		if( uOptionFlags.IsFlagSet(OF_MustBeVehicleDriver) && !pPed->GetIsDrivingVehicle() )
		{
			continue;
		}

		if( uOptionFlags.IsFlagSet(OF_MustBeOnFoot) && !pPed->GetIsOnFoot() )
		{
			continue;
		}

		const CEntity* pHostileTarget = pPed->GetPedIntelligence()->GetQueriableInterface()->GetHostileTarget();
		if( pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_COMBAT, true) && pHostileTarget == &pTargetPed )
		{
			if( uOptionFlags.IsFlagSet(OF_MustHaveClearLOS) && !HelperPedHasClearLOSToTarget(pPed, pHostileTarget) )
			{
				continue;
			}

			if( pvSearchCenter && scSearchRadiusSq )
			{
				ScalarV scDistToSearchCenterSq = DistSquared(pPed->GetTransform().GetPosition(), *pvSearchCenter);
				if(IsGreaterThanAll(scDistToSearchCenterSq, *scSearchRadiusSq))
				{
					continue;
				}
			}

			iCount++;
#if __BANK
			if( uOptionFlags.IsFlagSet(OF_DebugRender) )
			{
				grcDebugDraw::Line(VEC3V_TO_VECTOR3(pTargetPed.GetTransform().GetPosition()), VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()), Color_red, Color_red);
			}
#endif // __BANK

			if(uOptionFlags.IsFlagSet(OF_ReturnAfterInitialValidPed))
			{
				break;
			}
		}
	}

	return iCount;
}

int CCombatTaskManager::CountPedsInCombatStateWithTarget(const CPed& rTarget, s32 iState) const
{
	int iCount = 0;
	for( int i = 0; i < m_PedList.GetCount(); i++ )
	{
		const CPed* pPed = m_PedList[i];
		if(!pPed)
		{
			continue;
		}

		if(pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_COMBAT, true) &&
			(pPed->GetPedIntelligence()->GetQueriableInterface()->GetStateForTaskType(CTaskTypes::TASK_COMBAT) == iState) &&
			(pPed->GetPedIntelligence()->GetQueriableInterface()->GetHostileTarget() == &rTarget))
			
		{
			iCount++;
		}
	}

	return iCount;
}

int CCombatTaskManager::CountPedsWithClearLosToTarget(const CPed& rTarget, fwFlags8 uFlags) const
{
	int iCount = 0;
	for( int i = 0; i < m_PedList.GetCount(); i++ )
	{
		const CPed* pPed = m_PedList[i];
		if(!pPed)
		{
			continue;
		}

		if(uFlags.IsFlagSet(CPWCLTTF_MustBeOnFoot) && !pPed->GetIsOnFoot())
		{
			continue;
		}

		if(!HelperPedHasClearLOSToTarget(pPed, &rTarget))
		{
			continue;
		}

		iCount++;
	}

	return iCount;
}

// If looking for a combat target for a non-player ped, consider CQueriableInterface::FindHostileTarget().
CPed* CCombatTaskManager::GetPedInCombatWithPlayer(const CPed& pPlayerPed)
{
	aiAssert(pPlayerPed.IsPlayer());

	for( int i = 0; i < m_PedList.GetCount(); i++ )
	{
		CPed* pPed = m_PedList[i];
		if(!pPed)
		{
			continue;
		}

		if( pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_COMBAT, true) &&
			pPed->GetPedIntelligence()->GetQueriableInterface()->GetHostileTarget() == &pPlayerPed )
		{
			return pPed;
		}
	}

	return NULL;
}


bool CCombatTaskManager::SortByDistanceFromTarget(const sCombatPed& pFirstPed, const sCombatPed& pSecondPed)
{
	if(pFirstPed.m_fDistToTargetSq != pSecondPed.m_fDistToTargetSq)
	{
		return (pFirstPed.m_fDistToTargetSq > pSecondPed.m_fDistToTargetSq);
	}
	else
	{
		return (pFirstPed.m_pPed > pSecondPed.m_pPed);
	}
}

#if __BANK
void CCombatTaskManager::AddWidgets( bkBank& bank )
{
	bank.PushGroup("Combat Task Manager", false);

	bank.PushGroup("CountPedsInCombatWithTarget");
	 bank.AddToggle("Render counted ped lines", &sm_bDebugDisplayPedsInCombatWithTarget);
	 bank.AddToggle("Render filter: require clear LOS", &sm_bDebugDisplay_MustHaveClearLOS);
	 bank.AddToggle("Render filter: require gun", &sm_bDebugDisplay_MustHaveGunWeaponEquipped);
	bank.PopGroup();
	
	bank.PopGroup();
}
#endif // __BANK

#if !__FINAL
void CCombatTaskManager::DebugRender()
{
#if __BANK
	if( sm_bDebugDisplayPedsInCombatWithTarget )
	{
		for( int i = 0; i < m_PedList.GetCount(); i++ )
		{
			const CPed* pPed = m_PedList[i];
			if(!pPed)
			{
				continue;
			}

			if( sm_bDebugDisplay_MustHaveGunWeaponEquipped && !HelperPedHasWeaponEquipped(pPed) )
			{
				continue;
			}

			const CEntity* pHostileTarget = pPed->GetPedIntelligence()->GetQueriableInterface()->GetHostileTarget();
			if( pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_COMBAT, true) && pHostileTarget != NULL )
			{
				if( sm_bDebugDisplay_MustHaveClearLOS && !HelperPedHasClearLOSToTarget(pPed, pHostileTarget) )
				{
					continue;
				}

				grcDebugDraw::Line(VEC3V_TO_VECTOR3(pHostileTarget->GetTransform().GetPosition()), VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()), Color_red, Color_red);
			}
		}
	}
#endif // __BANK
}
#endif // !__FINAL

void CCombatTaskManager::FindAndRemoveLawPedsMP(sCombatPed* aCombatPeds, s32 iNumPedsInList, int iMaxPeds)
{
	// Only remove law peds when over a certain count in combat
	int iNumPedsOverMax = iNumPedsInList - iMaxPeds;
	if(iNumPedsOverMax <= 0)
	{
		return;
	}

	static dev_float s_fMinDistanceForLawToFleeInMP = 20.0f;
	ScalarV scMinDistanceForLawToFleeSq = LoadScalar32IntoScalarV(s_fMinDistanceForLawToFleeInMP);
	scMinDistanceForLawToFleeSq = (scMinDistanceForLawToFleeSq * scMinDistanceForLawToFleeSq);

	int iNumPedsRemoved = 0;
	for( int i = 0; i < iNumPedsInList; i++ )
	{
		CPed* pPed = aCombatPeds[i].m_pPed;

		if(pPed->IsNetworkClone())
		{
			continue;
		}

		if(pPed->PopTypeIsMission())
		{
			continue;
		}

		if(!pPed->IsLawEnforcementPed())
		{
			continue;
		}

		if(pPed->GetIsInVehicle())
		{
			continue;
		}

		if(IsLessThanAll(LoadScalar32IntoScalarV(aCombatPeds[i].m_fDistToTargetSq), scMinDistanceForLawToFleeSq))
		{
			continue;
		}

		CTaskThreatResponse* pThreatResponseTask = static_cast<CTaskThreatResponse*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_THREAT_RESPONSE));
		if(pThreatResponseTask && pThreatResponseTask->GetState() == CTaskThreatResponse::State_Combat)
		{
			pThreatResponseTask->SetFleeFromCombat(true);
			pPed->SetRemoveAsSoonAsPossible(true);

			++iNumPedsRemoved;
			if(iNumPedsRemoved >= iNumPedsOverMax)
			{
				break;
			}
		}
	}
}

void CCombatTaskManager::FindAndRemoveHelisMP(sCombatPed* aCombatPeds, s32 iNumPedsInList, int iNumHelis, int iMaxHelis)
{
	// Only remove over the max
	int iNumHelisOverMax = iNumHelis - iMaxHelis;
	if(iNumHelisOverMax <= 0)
	{
		return;
	}

	int iNumHelisRemoved = 0;
	for( int i = 0; i < iNumPedsInList; i++ )
	{
		CPed* pPed = aCombatPeds[i].m_pPed;

		if(pPed->IsNetworkClone())
		{
			continue;
		}

		if(!pPed->PopTypeIsRandom())
		{
			continue;
		}

		if(!pPed->IsLawEnforcementPed())
		{
			continue;
		}

		CVehicle* pMyVehicle = pPed->GetVehiclePedInside();
		if(!pMyVehicle)
		{
			continue;
		}

		if(!pMyVehicle->InheritsFromHeli())
		{
			continue;
		}

		if(!pMyVehicle->IsDriver(pPed))
		{
			continue;
		}

		// We are checking for any police helis (ignoring swat) so they must have a valid order
		COrder* pOrder = pPed->GetPedIntelligence()->GetOrder();
		if(!pOrder || !pOrder->GetIsValid())
		{
			continue;
		}

		// Must be a swat order type
		if(pOrder->GetDispatchType() != DT_PoliceHelicopter)
		{
			continue;
		}

		CTaskThreatResponse* pThreatResponseTask = static_cast<CTaskThreatResponse*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_THREAT_RESPONSE));
		if(pThreatResponseTask && pThreatResponseTask->GetState() == CTaskThreatResponse::State_Combat)
		{
			pThreatResponseTask->SetFleeFromCombat(true);

			++iNumHelisRemoved;
			if(iNumHelisRemoved >= iNumHelisOverMax)
			{
				break;
			}
		}
	}
}

// RETURNS - true if no more peds are needed to be removed, false if we need to remove more peds
bool CCombatTaskManager::FindPedsToRemove(sCombatPed* aCombatPeds, u32 iNumPedsInList, s32 &iNumPedsToRemove, bool bIgnoreLaw)
{
	ScalarV scMinDistanceForLawToFleeSq = LoadScalar32IntoScalarV(square(CTaskCombat::ms_Tunables.m_MinDistanceForLawToFleeFromCombat));
	bool bFoundLawWithinMinFleeRange = false;

	for( int i = 0; i < iNumPedsInList && iNumPedsToRemove > 0; i++ )
	{
		CPed* pPed = aCombatPeds[i].m_pPed;

		if(pPed->IsNetworkClone())
		{
			continue;
		}

		if(pPed->PopTypeIsMission())
		{
			continue;
		}
		
		if(bIgnoreLaw && pPed->IsLawEnforcementPed())
		{
			continue;
		}

		if(pPed->IsSecurityPed() || pPed->CheckBraveryFlags(BF_DONT_FORCE_FLEE_COMBAT))
		{
			continue;
		}

		COrder* pOrder = pPed->GetPedIntelligence()->GetOrder();
		if (pOrder && pOrder->GetIsValid())
		{
			continue;
		}

		if(pPed->GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_DisableFleeFromCombat))
		{
			continue;
		}

		CVehicle* pVehicle = pPed->GetVehiclePedInside();
		if(pVehicle && pVehicle->IsTank())
		{
			continue;
		}

		CTaskThreatResponse* pThreatResponseTask = static_cast<CTaskThreatResponse*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_THREAT_RESPONSE));
		if(pThreatResponseTask && pThreatResponseTask->GetState() == CTaskThreatResponse::State_Combat)
		{
			if(pPed->IsLawEnforcementPed())
			{
				if(!bFoundLawWithinMinFleeRange && IsGreaterThanAll(LoadScalar32IntoScalarV(aCombatPeds[i].m_fDistToTargetSq), scMinDistanceForLawToFleeSq))
				{
					pThreatResponseTask->SetFleeFromCombat(true);
					pPed->SetRemoveAsSoonAsPossible(true);
					iNumPedsToRemove--;
				}
				else
				{
					bFoundLawWithinMinFleeRange = true;
				}
			}
			else
			{
				pThreatResponseTask->SetFleeFromCombat(true);
				pPed->SetRemoveAsSoonAsPossible(true);
				iNumPedsToRemove--;
			}
		}
	}

	return (iNumPedsToRemove == 0);
}

bool CCombatTaskManager::HelperPedHasClearLOSToTarget(const CPed* pPed, const CEntity* pTarget) const
{
	const bool bWakeUpIfInactive = false;
	const CPedTargetting* pPedTargetting = pPed->GetPedIntelligence()->GetTargetting(bWakeUpIfInactive);
	if( pPedTargetting )
	{
		const CSingleTargetInfo* pCurrentTargetInfo = pPedTargetting->FindTarget(pTarget);
		if( pCurrentTargetInfo )
		{
			const LosStatus losStatus = pCurrentTargetInfo->GetLosStatus();
			if( losStatus == Los_clear )
			{
				return true;
			}
		}
	}

	return false;
}

bool CCombatTaskManager::HelperPedHasWeaponEquipped(const CPed* pPed) const
{
	const CPedWeaponManager* pWeaponManager = pPed->GetWeaponManager();
	if( pWeaponManager )
	{
		const CObject* pWeaponObj = pWeaponManager->GetEquippedWeaponObject();
		if( NULL == pWeaponObj )
		{
			// no equipped weapon object
			return false;
		}
		// else pWeaponObj != NULL
		const CWeaponInfo* pWeaponInfo = pWeaponManager->GetEquippedWeaponInfo();
		if(pWeaponInfo)
		{
			if( pWeaponInfo->GetIsGunOrCanBeFiredLikeGun() )
			{
				return true;
			}
		}
	}
	return false;
}

void CCombatTaskManager::Update()
{
	for( int i = 0; i < m_PedList.GetCount(); i++ )
	{
		if(!m_PedList[i])
		{
			m_PedList.Delete(i--);
		}
	}
	
	// Only update our timer if we haven't added a ped recently, because we want to update no matter what when we add new peds
	if(!m_bPedRecentlyAdded)
	{
		// If we haven't added a new ped then check if we can update based on our timer
		m_fUpdateTimer -= fwTimer::GetTimeStep();
		if(m_fUpdateTimer > 0.0f)
		{
			return;
		}
	}

	int iMaxPedsInCombatTask = !NetworkInterface::IsGameInProgress() ? sm_Tunables.m_iMaxPedsInCombatTask :
		Min(sMaxPedsInCombatWithNearbyIncidents[WANTED_LEVEL1], sm_Tunables.m_iMaxPedsInCombatTask);

	int nCount = m_PedList.GetCount();
	if(nCount > iMaxPedsInCombatTask)
	{
		const CPed* pLocalPlayer = CGameWorld::FindLocalPlayer();
		if(!pLocalPlayer)
		{
			return;
		}

		// Copy over the peds and store their distance from the local player
		Vec3V vPlayerPosition = pLocalPlayer->GetTransform().GetPosition();
		sCombatPed aPedInfo[sMaxPedsInArray];

		if(!NetworkInterface::IsGameInProgress())
		{
			for( int i = 0; i < nCount; i++ )
			{
				CPed* pPed = m_PedList[i];
				Vec3V vPedPosition = pPed->GetTransform().GetPosition();

				aPedInfo[i].m_fDistToTargetSq = DistSquared(vPedPosition, vPlayerPosition).Getf();
				aPedInfo[i].m_pPed = pPed;
			}
			
			// sort from largest to smallest distance
			std::sort(&aPedInfo[0], &aPedInfo[0] + nCount, CCombatTaskManager::SortByDistanceFromTarget);

			// First try to remove any non law peds
			s32 iNumPedsToRemove = nCount - sm_Tunables.m_iMaxPedsInCombatTask;
			if(!FindPedsToRemove(aPedInfo, nCount, iNumPedsToRemove, true))
			{
				// If we still have more peds to be removed, we can go ahead and include law enforcement ones
				FindPedsToRemove(aPedInfo, nCount, iNumPedsToRemove, false);
			}
		}
		else
		{
			// For network games, make sure it's our turn to try and remove peds from combat before trying to do so
			Vec3V vPosition(V_ZERO);
			static dev_float s_fMaxDistance = FLT_MAX;
			static dev_float s_fDurationOfTurns = 0.25f;
			static dev_float s_fTimeBetweenTurns = 0.25f;
			if(!NetworkInterface::IsLocalPlayersTurn(vPosition, s_fMaxDistance, s_fDurationOfTurns, s_fTimeBetweenTurns))
			{
				return;
			}

			static dev_float sMaxPedDistanceFromLocalPlayerMP = 200.0f;
			ScalarV scMaxDistanceFromLocalPlayerMPSq = LoadScalar32IntoScalarV(sMaxPedDistanceFromLocalPlayerMP);
			scMaxDistanceFromLocalPlayerMPSq = (scMaxDistanceFromLocalPlayerMPSq * scMaxDistanceFromLocalPlayerMPSq);

			s32 nPedsAdded = 0;
			for( int i = 0; i < nCount; i++ )
			{
				CPed* pPed = m_PedList[i];

				Vec3V vPedPosition = pPed->GetTransform().GetPosition();
				ScalarV scDistToLocalPlayerSq = DistSquared(vPedPosition, vPlayerPosition);
				if(IsLessThanAll(scDistToLocalPlayerSq, scMaxDistanceFromLocalPlayerMPSq))
				{
					aPedInfo[nPedsAdded].m_pPed = pPed;

					// We need to hack the distance for heli peds because we don't want them to flee but we need to consider them as part of the
					// max peds we keep in combat, otherwise they will be the "possible" flee peds but refuse to flee due to being in a vehicle
					CEntity* pTarget = pPed->GetPedIntelligence()->GetQueriableInterface()->GetHostileTarget();
					CVehicle* pMyVehicle = pPed->GetVehiclePedInside();
					if(pMyVehicle && pMyVehicle->InheritsFromHeli())
					{
						aPedInfo[nPedsAdded].m_fDistToTargetSq = 0.0f;
					}
					else
					{
						aPedInfo[nPedsAdded].m_fDistToTargetSq = DistSquared(vPedPosition, pTarget ? pTarget->GetTransform().GetPosition() : vPlayerPosition).Getf();
					}
					++nPedsAdded;
				}
			}

			// sort from largest to smallest distance
			std::sort(&aPedInfo[0], &aPedInfo[0] + nPedsAdded, CCombatTaskManager::SortByDistanceFromTarget);

			// First try to remove any non law peds
			s32 iNumPedsToRemove = nPedsAdded - sm_Tunables.m_iMaxPedsInCombatTask;
			if(iNumPedsToRemove > 0)
			{
				FindPedsToRemove(aPedInfo, nPedsAdded, iNumPedsToRemove, true);
			}

			//Find the nearby wanted incidents.
			static const int s_iMaxIncidents = 15;
			CWantedIncident* aNearbyIncidents[s_iMaxIncidents];
			int iNumNearbyIncidents = CWantedIncident::FindNearbyIncidents(CWantedIncident::GetMaxDistanceForNearbyIncidents(), aNearbyIncidents, s_iMaxIncidents);

			//Find the highest wanted level.
			int iHighestWantedLevel = WANTED_CLEAN;
			for(int i = 0; i < iNumNearbyIncidents; ++i)
			{
				iHighestWantedLevel = Max(iHighestWantedLevel, aNearbyIncidents[i]->GetWantedLevel());
			}

			//Find the nearby wanted incidents.
			iNumNearbyIncidents = CWantedIncident::FindNearbyIncidents(CWantedIncident::GetMaxDistanceForNearbyIncidents(iHighestWantedLevel), aNearbyIncidents, s_iMaxIncidents);

			//Find the highest wanted level.
			iHighestWantedLevel = WANTED_CLEAN;
			for(int i = 0; i < iNumNearbyIncidents; ++i)
			{
				iHighestWantedLevel = Max(iHighestWantedLevel, aNearbyIncidents[i]->GetWantedLevel());
			}
			
			// Make sure the wanted level is valid before trying to remove helicopters
			if(iHighestWantedLevel > WANTED_CLEAN && iHighestWantedLevel < WANTED_LEVEL_LAST)
			{
				// Get the number of helis in combat
				int iNumHelis = GetNumHelis(aPedInfo, nPedsAdded);

				// Try and remove helis if possible/needed
				FindAndRemoveHelisMP(aPedInfo, nPedsAdded, iNumHelis, sMaxHelisInCombatWithNearbyIncidents[iHighestWantedLevel]);
			}

			// Get the number of law peds in combat, minus the peds who are in vehicles where the driver has been told to flee
			int iNumLawPeds = GetNumLawPedsInCombat(aPedInfo, nPedsAdded);

			// Now tell any peds over our max to flee
			int iMaxPeds = iHighestWantedLevel > WANTED_CLEAN && iHighestWantedLevel < WANTED_LEVEL_LAST ? sMaxPedsInCombatWithNearbyIncidents[iHighestWantedLevel] : sMaxPedsInCombatMP;
			if(iNumLawPeds > iMaxPeds)
			{
				FindAndRemoveLawPedsMP(aPedInfo, nPedsAdded, iMaxPeds);
			}
		}
	}

	// Reset the timer and recently added variables
	m_fUpdateTimer = sm_Tunables.m_fTimeBetweenUpdates;
	m_bPedRecentlyAdded = false;
}

int CCombatTaskManager::GetNumLawPedsInCombat(sCombatPed* aCombatPeds, s32 iNumPedsInList)
{
	int iNumLawPeds = 0;
	for( int i = 0; i < iNumPedsInList; i++ )
	{
		CPed* pPed = aCombatPeds[i].m_pPed;

		if(!pPed->IsLawEnforcementPed())
		{
			continue;
		}

		// Is this ped in a heli
		CVehicle* pMyVehicle = pPed->GetVehiclePedInside();
		if(pMyVehicle && pMyVehicle->InheritsFromHeli())
		{
			// Get our driver
			CPed* pDriver = pMyVehicle->GetDriver();
			if(pDriver)
			{
				// Anybody that has a police dispatch type should be checked, swat rappelling and landing helis should still be considered in combat
				COrder* pOrder = pDriver->GetPedIntelligence()->GetOrder();
				if( pOrder && pOrder->GetIsValid() && pOrder->GetDispatchType() == DT_PoliceHelicopter )
				{
					// See if our driver is fleeing
					if(pDriver->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_SMART_FLEE, true))
					{
						continue;
					}

					// See if they were told to flee
					const CTaskThreatResponse* pTaskThreatResponse = static_cast<CTaskThreatResponse *>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_THREAT_RESPONSE));
					if(pTaskThreatResponse && pTaskThreatResponse->IsFleeFromCombatFlagSet())
					{
						continue;
					}
				}
			}
		}

		iNumLawPeds++;
	}

	return iNumLawPeds;
}

int CCombatTaskManager::GetNumHelis(sCombatPed* aCombatPeds, s32 iNumPedsInList)
{
	int iNumHelis = 0;
	for( int i = 0; i < iNumPedsInList; i++ )
	{
		CPed* pPed = aCombatPeds[i].m_pPed;

		// Ped must be random
		if(!pPed->PopTypeIsRandom())
		{
			continue;
		}

		// Must be law enforcement
		if(!pPed->IsLawEnforcementPed())
		{
			continue;
		}

		// Must be the driver of the heli
		CVehicle* pMyVehicle = pPed->GetVehiclePedInside();
		if(!pMyVehicle)
		{
			continue;
		}

		if(!pMyVehicle->InheritsFromHeli())
		{
			continue;
		}

		if(!pMyVehicle->IsDriver(pPed))
		{
			continue;
		}

		// We can count this ped as a "heli"
		iNumHelis++;
	}

	return iNumHelis;
}

void CCombatTaskManager::AddPed(CPed* pPed)
{
	if(!pPed)
	{
		return;
	}

	int nCount = m_PedList.GetCount();
	for(int i = 0; i < nCount; i++)
	{
		if(m_PedList[i] == pPed)
		{
			return;
		}
	}

	if( aiVerifyf(!m_PedList.IsFull(), "Too many peds running CTaskCombat.") )
	{
		m_PedList.Append() = pPed;
		m_bPedRecentlyAdded = true;
	}
}

void CCombatTaskManager::RemovePed(CPed* pPed)
{
	if(!pPed)
	{
		return;
	}

	int nCount = m_PedList.GetCount();
	for(int i = 0; i < nCount; i++)
	{
		if(m_PedList[i] == pPed)
		{
			m_PedList.Delete(i);
			return;
		}
	}
}

//-----------------------------------------------------------------------------
s32	CCombatMeleeManager::sm_nMaxNumActiveCombatants			= 1;
s32	CCombatMeleeManager::sm_nMaxNumActiveSupportCombatants	= 3;

CCombatMeleeManager::CCombatMeleeManager()
: m_Groups( NULL )
, m_MaxGroups( 0 )
, m_MaxGroupsMP( 0 )
, m_NumGroups( 0 )
{
	const int nMaxGroups = fwConfigManager::GetInstance().GetSizeOfPool( ATSTRINGHASH( "CombatMeleeManager_Groups", 0xb7d08969 ) , CONFIGURED_FROM_FILE );
	const int nMaxGroupsMP = fwConfigManager::GetInstance().GetSizeOfPool( ATSTRINGHASH("CombatMeleeManager_GroupsMP",0x3FA6EC68) , CONFIGURED_FROM_FILE );
	m_MaxGroups = nMaxGroups;
	m_MaxGroupsMP = nMaxGroupsMP;
	if( nMaxGroups > 0 )
		m_Groups = rage_new CCombatMeleeGroup[ Max(nMaxGroups,nMaxGroupsMP) ]; // Allocate enough memory to store both SP and MP because we do it once (when booting the game)

#if __BANK
	m_bDebugRender = false;
#endif
}

CCombatMeleeManager::~CCombatMeleeManager()
{
	DestroyAllGroups();
	Assert( !m_NumGroups );
	delete[] m_Groups;
}

void CCombatMeleeManager::Update( void )
{
	for( int i = 0; i < GetMaxGroupsCount(); i++ )
	{
		CCombatMeleeGroup& group = m_Groups[i];
		if( !group.IsActive() )
			continue;

		if( !group.Update() )
			DestroyGroup( group );
	}
}

void CCombatMeleeManager::DestroyGroup( CCombatMeleeGroup& group )
{
	Assert( group.IsActive() );
	group.Shutdown();
	m_NumGroups--;
	Assert( m_NumGroups >= 0 );
}

void CCombatMeleeManager::DestroyAllGroups( void )
{
	for( int i = 0; i < GetMaxGroupsCount(); i++ )
	{
		CCombatMeleeGroup& group = m_Groups[i];
		if( !group.IsActive() )
			continue;

		DestroyGroup( group );
	}
}

CCombatMeleeGroup *CCombatMeleeManager::FindAvailableMeleeGroup()
{
	if( m_NumGroups >= GetMaxGroupsCount() )
		return NULL;

	for( int i = 0; i < GetMaxGroupsCount(); i++ )
	{
		CCombatMeleeGroup& group = m_Groups[i];
		if( !group.IsActive() )
			return &group;
	}

	return NULL;
}

CCombatMeleeGroup* CCombatMeleeManager::FindMeleeGroup( const CEntity& target )
{
	for( int i = 0; i < GetMaxGroupsCount(); i++ )
	{
		CCombatMeleeGroup& group = m_Groups[i];
		if( !group.IsActive() )
			continue;

		if( group.GetTarget() == &target )
			return &m_Groups[i];
	}

	return NULL;
}

CCombatMeleeGroup* CCombatMeleeManager::FindOrCreateMeleeGroup( const CEntity& target )
{
	CCombatMeleeGroup* pMeleeGroup = FindMeleeGroup( target );
	if( pMeleeGroup )
		return pMeleeGroup;

	pMeleeGroup = FindAvailableMeleeGroup();
	if( !pMeleeGroup )
		return NULL;

	pMeleeGroup->Init( target );
	m_NumGroups++;
	Assert( m_NumGroups <= GetMaxGroupsCount() );
	return pMeleeGroup;
}

#if __BANK
void CCombatMeleeManager::AddWidgets( bkBank &bank )
{
	bank.PushGroup("Melee Combat", false);
		bank.AddToggle("Draw", &m_bDebugRender);
	bank.PopGroup();
}

bool CCombatMeleeManager::IsMeleeOpponentInGroup(CCombatMeleeOpponent *pOpponent)
{
	for( int i = 0; i < GetMaxGroupsCount(); i++ )
	{
		CCombatMeleeGroup& group = m_Groups[i];
		if(group.ContainsOpponent(pOpponent))
		{
			return true;
		}
	}

	return false;
}

#endif	// __BANK

#if !__FINAL
void CCombatMeleeManager::DebugRender( void )
{
#if __BANK
	if( !m_bDebugRender )
		return;

	for( int i = 0; i < GetMaxGroupsCount(); i++ )
	{
		CCombatMeleeGroup& group = m_Groups[i];
		if( !group.IsActive() )
			continue;

		group.DebugRender();
	}
#endif	// __BANK
}
#endif	// !__FINAL

//-----------------------------------------------------------------------------
CCombatMeleeGroup::CCombatMeleeGroup()
: m_v3Center( VEC3_ZERO )
, m_fRadius( 0.0f )
, m_Target( NULL )
, m_uTimeTargetStartedMovingAwayFromActiveCombatant(0)
, m_uLastInitialPursuitTime(0)
, m_FlagActive( false )
, m_bHasCheckedBattleAwareness( false )
{
}

CCombatMeleeGroup::~CCombatMeleeGroup()
{
	Assert( !m_FlagActive );
}

void CCombatMeleeGroup::Init( const CEntity& target )
{
	Assert( !m_Target );
	Assert( !m_FlagActive );
	m_FlagActive = true;
	m_Target = &target;
	m_v3Center = target.GetTransform().GetPosition();
	m_aOpponents.Reset();
	m_uLastInitialPursuitTime = fwTimer::GetTimeInMilliseconds();
	m_bHasCheckedBattleAwareness = false;
}

void CCombatMeleeGroup::Shutdown()
{
	Assert( m_FlagActive );
	m_FlagActive = false;
	m_Target.Reset( NULL );
	m_uLastInitialPursuitTime = 0;
	m_bHasCheckedBattleAwareness = false;
}

int CCombatMeleeGroup::SortByDistance( CCombatMeleeOpponent* const* pA1, CCombatMeleeOpponent* const* pA2 )
{
	return (s32)((*pA1)->GetDistanceToTarget() - (*pA2)->GetDistanceToTarget());
}

bool CCombatMeleeGroup::Update( void )
{
	if( !IsTargetValid() )
		return false;

	int nCount = m_aOpponents.GetCount();
	if( nCount < 1 )
		return false;

	for( int i = 0; i < nCount; i++ )
	{
		Assert(m_aOpponents[ i ]->m_pMeleeGroup == this);

		Vec3V vTargetToPed = m_aOpponents[ i ]->GetPedPos() - GetTargetPos();
		m_aOpponents[ i ]->SetDistanceToTarget( MagSquared( vTargetToPed ).Getf() );
		m_aOpponents[ i ]->SetAngleToTarget( fwAngle::LimitRadianAngle( rage::Atan2f( -vTargetToPed.GetXf(), vTargetToPed.GetYf() ) ) );
	}

	// Sort 
	m_aOpponents.QSort( 0, -1, CCombatMeleeGroup::SortByDistance );

	//Check if we have not checked battle awareness.
	if(!m_bHasCheckedBattleAwareness)
	{
		//Set the flag.
		m_bHasCheckedBattleAwareness = true;

		//Check if the target is a player.
		const CPed* pTargetPed = GetTargetPed();
		if(pTargetPed && pTargetPed->IsLocalPlayer())
		{
			//Check if the opponent is valid.
			const CPed* pOpponent = m_aOpponents[0]->GetPed();
			if(pOpponent && pOpponent->PopTypeIsRandom())
			{
				//Check if the opponent is close.
				float fDistance = m_aOpponents[0]->GetDistanceToTarget();
				static dev_float s_fMaxDistance = 15.0f;
				if(fDistance < s_fMaxDistance)
				{
					//Set the battle awareness.
					pTargetPed->GetPedIntelligence()->IncreaseBattleAwareness(CPedIntelligence::BATTLE_AWARENESS_MELEE_FIGHT, false, false);
				}
			}
		}
	}

	// walk through once more and update the melee opponent accordingly
	float fBaseHeading = 0.0f;
	fBaseHeading = m_aOpponents[ 0 ]->GetAngleToTarget();

	bool bInInactiveTauntMode = false;
	bool bForceIgnoreMeleeActiveCombatant = false;
	bool bForceIgnoreMaxMeleeActiveSupportCombatant = false;
	CTaskMelee* pTaskForActiveCombatant = NULL;
	float fAngleSpread = fBaseHeading;
	float fSpreadOffset = 0.0f;
	for( int i = 0; i < nCount; i++ )
	{
		CPed* pPed = m_aOpponents[ i ]->GetPed();
		if( pPed )
		{
			Assert( pPed->GetPedIntelligence() );
			CTaskMelee* pTaskMelee = pPed->GetPedIntelligence()->GetTaskMelee();
			if( pTaskMelee )
			{
				bInInactiveTauntMode = pTaskMelee->ShouldBeInInactiveTauntMode() || pTaskMelee->GetIsTaskFlagSet( CTaskMelee::MF_CannotFindNavRoute );
				bForceIgnoreMeleeActiveCombatant = pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_ForceIgnoreMeleeActiveCombatant );
				bForceIgnoreMaxMeleeActiveSupportCombatant = pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_ForceIgnoreMaxMeleeActiveSupportCombatants );

				// First figure out the scale direction
				fSpreadOffset = SubtractAngleShorter( fBaseHeading, m_aOpponents[ i ]->GetAngleToTarget() ) > 0.0f ? -1.0f : 1.0f;

				// Scale
				fSpreadOffset *= (float)i;

				// Random angular offset
				static dev_float sfAngleSpreadMin = 7.5f * DtoR;
				static dev_float sfAngleSpreadMax = 12.5f * DtoR;
				fSpreadOffset *= fwRandom::GetRandomNumberInRange( sfAngleSpreadMin, sfAngleSpreadMax );

				// Apply
				fAngleSpread = fwAngle::LimitRadianAngle( fBaseHeading + fSpreadOffset );

				if( ( i - CCombatMeleeManager::sm_nMaxNumActiveCombatants ) < 0 && !bForceIgnoreMeleeActiveCombatant && !bInInactiveTauntMode )
				{
					pTaskMelee->SetAngleSpread( fAngleSpread );
					pTaskMelee->SetQueueType( CTaskMelee::QT_ActiveCombatant );

					// Set the task for the active combatant
					pTaskForActiveCombatant = pTaskMelee;
				}
				else if( ( i - CCombatMeleeManager::sm_nMaxNumActiveSupportCombatants ) < 0 || bForceIgnoreMaxMeleeActiveSupportCombatant)
				{
					pTaskMelee->SetAngleSpread( fAngleSpread );
					pTaskMelee->SetQueueType( CTaskMelee::QT_SupportCombatant );
				}
				else
				{
					pTaskMelee->SetAngleSpread( fAngleSpread ); 
					pTaskMelee->SetQueueType( CTaskMelee::QT_InactiveObserver );
				}

				//If there are more than 3 peds fighting the same target, send the extra random civilians away.
				static const int s_iMaxCivilians = 3;
				if((i >= s_iMaxCivilians) && pPed->PopTypeIsRandom() && pPed->IsCivilianPed())
				{
					//Check if the task is valid.
					CTaskThreatResponse* pTaskThreatResponse = static_cast<CTaskThreatResponse *>(
						pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_THREAT_RESPONSE));
					if(pTaskThreatResponse)
					{
						//Change the decision from combat to flee.
						pTaskThreatResponse->SetFleeFromCombat(true);
					}
				}	
			}
		}
	}

	//Check if the task for the active combatant is valid.
	if(pTaskForActiveCombatant)
	{
		//Check if the target is moving away from the active combatant.
		const CPed* pActiveCombatant = pTaskForActiveCombatant->GetPed();
		if(IsTargetMovingAwayFromPed(*pActiveCombatant))
		{
			if(m_uTimeTargetStartedMovingAwayFromActiveCombatant == 0)
			{
				m_uTimeTargetStartedMovingAwayFromActiveCombatant = fwTimer::GetTimeInMilliseconds();
			}
		}
		else
		{
			m_uTimeTargetStartedMovingAwayFromActiveCombatant = 0;
		}
	}
	else
	{
		m_uTimeTargetStartedMovingAwayFromActiveCombatant = 0;
	}

	return true;
}

bool CCombatMeleeGroup::Insert( CCombatMeleeOpponent& opponent )
{
	Assert(!CCombatManager::GetMeleeCombatManager()->IsMeleeOpponentInGroup(&opponent));

	if(Verifyf(!opponent.GetMeleeGroup(), "Trying to add a melee opponent more than once!"))
	{
		opponent.m_pMeleeGroup = this;

		if( m_aOpponents.GetCount() < MAX_NUM_OPPONENTS_PER_GROUP )
		{
			m_aOpponents.Append() = &opponent;
			return true;
		}
	}

	return false;
}

void CCombatMeleeGroup::Remove( CCombatMeleeOpponent& opponent )
{
	if(Verifyf(opponent.GetMeleeGroup() == this, "Removing melee opponent from wrong group!"))
	{
		opponent.m_pMeleeGroup = NULL;

		int nCount = m_aOpponents.GetCount();
		for( int i = 0; i < nCount; i++ )
		{
			if( m_aOpponents[ i ] == &opponent )
			{
				m_aOpponents[ i ]->Shutdown();
				m_aOpponents.Delete( i );
				break;
			}
		}
	}
}
bool CCombatMeleeGroup::HasAllyInDirection( const CPed* pCombatPed, eAllyDirection allyDirection )
{
	if( pCombatPed && IsPedInMeleeGroup( pCombatPed ) )
	{
		static dev_float sfAngularThreshold = 35.0f * DtoR;
		Vec3V vAllyDirection( VEC3_ZERO );
		ScalarV vDotProduct( 0.0f );
		int nCount = m_aOpponents.GetCount();
		for( int i = 0; i < nCount; i++ )
		{
			CPed* pOpponentPed = m_aOpponents[ i ]->GetPed();
			if( pOpponentPed && pOpponentPed != pCombatPed )
			{
				// Calculate the difference between the combat peds
				vAllyDirection = pOpponentPed->GetTransform().GetPosition() - pCombatPed->GetTransform().GetPosition();
				vAllyDirection = Normalize( vAllyDirection );

				switch( allyDirection )
				{
					case AD_BACK_LEFT:
					{
						// first make sure they are behind
						vDotProduct = Dot( vAllyDirection, pCombatPed->GetTransform().GetB() );
						if( vDotProduct.Getf() > 0.0f )
							continue;

						// otherwise they are behind and we can test to see if they are on the right side
						vDotProduct = Dot( vAllyDirection, Negate( pCombatPed->GetTransform().GetA() ) );
						if( vDotProduct.Getf() > 0.0f && vDotProduct.Getf() < sfAngularThreshold )
							return true;
						break;
					}
					case AD_BACK_RIGHT:
					{
						// first make sure they are behind
						vDotProduct = Dot( vAllyDirection, pCombatPed->GetTransform().GetB() );
						if( vDotProduct.Getf() > 0.0f )
							continue;

						// otherwise they are behind and we can test to see if they are on the right side
						vDotProduct = Dot( vAllyDirection, pCombatPed->GetTransform().GetA() );
						if( vDotProduct.Getf() > 0.0f && vDotProduct.Getf() < sfAngularThreshold )
							return true;
						break;
					}
					case AD_FRONT:
					{
						vDotProduct = Dot( vAllyDirection, pCombatPed->GetTransform().GetB() );
						if( vDotProduct.Getf() > 0.0f )
							return true;
						break;
					}
					case AD_BACK:
					{
						vDotProduct = Dot( vAllyDirection, pCombatPed->GetTransform().GetB() );
						if( vDotProduct.Getf() < 0.0f )
							return true;
						break;
					}
				}
			}
		}
	}

	return false;
}

bool CCombatMeleeGroup::IsPedInMeleeGroup( const CPed* pCombatPed )
{
	if( pCombatPed )
	{
		int nCount = m_aOpponents.GetCount();
		for( int i = 0; i < nCount; i++ )
		{
			if( m_aOpponents[ i ]->GetPed() == pCombatPed )
				return true;
		}
	}

	return false;
}

bool CCombatMeleeGroup::HasActiveCombatant( void )
{
	CPed* pPed = NULL;
	CTaskMelee* pTaskMelee = NULL;
	int nCount = m_aOpponents.GetCount();
	for( int i = 0; i < nCount; i++ )
	{
		pPed = m_aOpponents[ i ]->GetPed();
		if( pPed )
		{
			Assert( pPed->GetPedIntelligence() );
			pTaskMelee = pPed->GetPedIntelligence()->GetTaskMelee();
			if( pTaskMelee && pTaskMelee->GetQueueType() == CTaskMelee::QT_ActiveCombatant )
				return true;
		}
	}

	return false;
}

bool CCombatMeleeGroup::IsTargetMovingAwayFromPed(const CPed& rPed) const
{
	//Ensure the target is a ped.
	if(!m_Target || !m_Target->GetIsTypePed())
	{
		return false;
	}

	//Check if the target is on foot.
	Vec3V vPosition;
	Vec3V vVelocity;
	const CPed* pTarget = static_cast<const CPed *>(m_Target.Get());
	if(pTarget->GetIsOnFoot())
	{
		vPosition = pTarget->GetTransform().GetPosition();
		vVelocity = VECTOR3_TO_VEC3V(pTarget->GetVelocity());
	}
	//Check if the target is in a vehicle.
	else if(pTarget->GetIsInVehicle())
	{
		vPosition = pTarget->GetVehiclePedInside()->GetTransform().GetPosition();
		vVelocity = VECTOR3_TO_VEC3V(pTarget->GetVehiclePedInside()->GetVelocity());
	}
	else
	{
		return false;
	}

	//Ensure the target is not strafing.
	if (pTarget->IsStrafing())
	{
		return false;
	}

	//Ensure the velocity exceeds the threshold.
	ScalarV scSpeedSq = MagSquared(vVelocity);
	static dev_float s_fMinSpeed = 0.5f;
	ScalarV scMinSpeedSq = ScalarVFromF32(square(s_fMinSpeed));
	if(IsLessThanAll(scSpeedSq, scMinSpeedSq))
	{
		return false;
	}

	//Ensure the target is moving away from the ped.
	Vec3V vPedToTarget = Subtract(vPosition, rPed.GetTransform().GetPosition());
	ScalarV scDot = Dot(vPedToTarget, vVelocity);
	if(IsLessThanAll(scDot, ScalarV(V_ZERO)))
	{
		return false;
	}

	return true;
}

const CPed* CCombatMeleeGroup::GetTargetPed( void ) const
{
	//Ensure the target is valid.
	if(!m_Target)
	{
		return NULL;
	}

	//Ensure the target is a ped.
	if(!m_Target->GetIsTypePed())
	{
		return NULL;
	}

	return static_cast<const CPed *>(m_Target.Get());
}

#if !__FINAL
void CCombatMeleeGroup::DebugRender( void ) const
{
#if DEBUG_DRAW

#endif	// DEBUG_DRAW
}

#endif	// !__FINAL

#if __BANK

bool CCombatMeleeGroup::ContainsOpponent( CCombatMeleeOpponent *pOpponent )
{
	int nCount = m_aOpponents.GetCount();
	for( int i = 0; i < nCount; i++ )
	{
		if(m_aOpponents[i] == pOpponent)
		{
			return true;
		}
	}

	return false;
}

#endif

//-----------------------------------------------------------------------------
CCombatMeleeOpponent::CCombatMeleeOpponent()
: m_pMeleeGroup( NULL )
, m_pPed( NULL )
, m_fDistToTarget( LARGE_FLOAT )
, m_fAngleToTaget( 0.0f )
{
}

CCombatMeleeOpponent::~CCombatMeleeOpponent()
{
	Assert( !m_pPed );	// User should have called Shutdown().

	//! Verify that this opponent has been removed from melee group.
	Assert(!CCombatManager::GetMeleeCombatManager()->IsMeleeOpponentInGroup(this));
}

void CCombatMeleeOpponent::Init( CPed& ped )
{	
	Assert( !m_pPed );
	m_pPed = &ped;
}

void CCombatMeleeOpponent::Shutdown( void )
{
	m_pPed = NULL;
}

//-----------------------------------------------------------------------------

CNavMeshCircleTestHelper::CNavMeshCircleTestHelper()
{
	m_bWaitingForLosResult = false;
	m_hPathHandle = PATH_HANDLE_NULL;
}


CNavMeshCircleTestHelper::~CNavMeshCircleTestHelper()
{
	ResetTest();
}


bool CNavMeshCircleTestHelper::StartLosTest(Vec3V_In centerV, float radius, bool bTestDynamicObjects)
{
	ResetTest();

	// CPathServer has a limit to how many segments can be tested in a single request.
	CompileTimeAssert(kNumSegments + 1 <= MAX_LINEOFSIGHT_POINTS);

	Vec3V vVerts[kNumSegments + 1];
	const int numVerts = GenerateVerts(centerV, radius, vVerts, NELEM(vVerts));

	bool bQuitAtFirstLOSFail = true;
	m_hPathHandle = CPathServer::RequestLineOfSight((const Vector3*)&vVerts[0], numVerts, 0.0f, bTestDynamicObjects, bQuitAtFirstLOSFail);
	if(m_hPathHandle != PATH_HANDLE_NULL)
	{
		m_bWaitingForLosResult = true;
	}
	return m_bWaitingForLosResult;
}


bool CNavMeshCircleTestHelper::GetTestResults(bool& bLosIsClear)
{
	Assert(m_bWaitingForLosResult);

	EPathServerRequestResult ret = CPathServer::IsRequestResultReady(m_hPathHandle);
	if(ret == ERequest_NotReady)
	{
		m_bWaitingForLosResult = false;

		EPathServerErrorCode errCode = CPathServer::GetLineOfSightResult(m_hPathHandle, bLosIsClear);

		m_hPathHandle = PATH_HANDLE_NULL;

		if(errCode == PATH_FOUND)
		{
			return true;
		}
		else
		{
			bLosIsClear = false;
			return true;
		}
	}
	else if(ret == ERequest_NotFound)
	{
		m_hPathHandle = PATH_HANDLE_NULL;
		bLosIsClear = false;
		return true;
	}
	return false;
}


void CNavMeshCircleTestHelper::ResetTest()
{
	if(m_bWaitingForLosResult)
	{
		if(m_hPathHandle != PATH_HANDLE_NULL)
		{
			CPathServer::CancelRequest(m_hPathHandle);
			m_hPathHandle = PATH_HANDLE_NULL;
		}
		m_bWaitingForLosResult = false;
	}
}

#if !__FINAL

void CNavMeshCircleTestHelper::DebugDraw(Vec3V_In DEBUG_DRAW_ONLY(centerV), float DEBUG_DRAW_ONLY(radius), Color32 DEBUG_DRAW_ONLY(col))
{
#if DEBUG_DRAW
	Vec3V vVerts[kNumSegments + 1];
	const int numVerts = GenerateVerts(centerV, radius, vVerts, NELEM(vVerts));

	for(int i = 0; i < numVerts - 1; i++)
	{
		grcDebugDraw::Line(vVerts[i], vVerts[i + 1], col);
	}
#endif	// DEBUG_DRAW
}

#endif	// !__FINAL

int CNavMeshCircleTestHelper::GenerateVerts(Vec3V_In centerV, float radius, Vec3V* pVertexArray, int ASSERT_ONLY(maxVerts))
{
	Assert(maxVerts >= kNumSegments + 1);

	const float centerX = centerV.GetXf();
	const float centerY = centerV.GetYf();
	const ScalarV centerZV = centerV.GetZ();
	const float deltaAngle = 2.0f*PI/kNumSegments;

	float angle = 0.0f;
	int numVerts = 0;
	for(int i = 0; i < kNumSegments + 1; i++)
	{
		Vec3V& ptV = pVertexArray[numVerts++];
		float dirX, dirY;
		cos_and_sin(dirX, dirY, angle);
		ptV.SetXf(centerX + radius*dirX);
		ptV.SetYf(centerY + radius*dirY);
		ptV.SetZ(centerZV);
		angle += deltaAngle;
	}

	return numVerts;
}

//-----------------------------------------------------------------------------

void CCombatMountedAttackerGroup::CircleData::Reset()
{
	if(m_FlagActive)
	{
		while(1)
		{
			atDNode<CCombatMountedAttacker*>* pNode = m_AttackersUsingCircle.PopHead();
			if(pNode)
			{
				Assert(pNode->Data->m_InCircle == this);
				pNode->Data->m_InCircle = NULL;
			}
			else
			{
				break;
			}
		}
	}

	m_CenterAndRadius = Vec4V(V_ZERO);
	m_FlagActive = m_FlagTested = m_FlagTestedFreeFromObstructions = false;
	m_RadiusIndex = -1;
}


//-----------------------------------------------------------------------------

CCombatMountedAttackerGroup::CCombatMountedAttackerGroup()
: m_CircleTestsRequested(false)
, m_FlagActive(false)
, m_TimeNoGood(0.0f)
, m_CurrentCircleTestRadiusIndex(-1)
, m_NextCircleTestRadiusIndex(-1)
, m_CircleBeingTested(-1)
{
	for(int i = 0; i < kMaxCircles; i++)
	{
		m_Circles[i].Reset();
	}

	m_TimeSinceLastCircleJoined = LARGE_FLOAT;
	m_LastCircleJoined = -1;
}


CCombatMountedAttackerGroup::~CCombatMountedAttackerGroup()
{
	Assert(!m_FlagActive);
}


void CCombatMountedAttackerGroup::Init(const CEntity& tgt)
{
	Assert(!m_Target);
	Assert(!m_FlagActive);
	m_FlagActive = true;

	m_TimeSinceLastCircleJoined = LARGE_FLOAT;
	m_LastCircleJoined = -1;

	m_CircleTestsRequested = false;

	m_Target = &tgt;

	for(int i = 0; i < kMaxCircles; i++)
	{
		m_Circles[i].m_FlagActive = false;
		m_Circles[i].Reset();
	}

	Assert(m_CircleBeingTested < 0);

	m_CurrentCircleTestRadiusIndex = m_NextCircleTestRadiusIndex = -1;
}


void CCombatMountedAttackerGroup::Shutdown()
{
	if(m_CircleBeingTested >= 0)
	{
		m_CircleTestHelper.ResetTest();
		m_CircleBeingTested = -1;
	}

	while(1)
	{
		atDNode<CCombatMountedAttacker*>* pHeadNode = m_List.GetHead();
		if(!pHeadNode)
		{
			break;
		}
		Remove(*pHeadNode->Data);
	}

	Assert(m_FlagActive);
	m_FlagActive = false;

	m_Target.Reset(NULL);
}


bool CCombatMountedAttackerGroup::Update()
{
	if(!m_Target)
	{
		return false;
	}

	if(!m_List.GetHead())
	{
		return false;
	}

	const float deltaTime = fwTimer::GetTimeStep();

	m_TimeSinceLastCircleJoined += deltaTime;

	CheckExistingCircles();
	UpdateCircleTests();

#if 0
	// MAGIC!
	const int maxMembers = GetGroup().GetNumMembers();
	static const float removeTime = 1.5f;
	static const float timeNoChange = 2.0f;

	if(m_TimeNoGood > timeNoChange)
	{
		float dt = m_TimeNoGood - timeNoChange;
		m_NumDesiredMembers = Max((int)(maxMembers - dt*(1.0f/removeTime)), 0);
	}
	else
	{
		m_NumDesiredMembers = -1;
	}
#endif

	for(atDNode<CCombatMountedAttacker*>* pNode = m_List.GetHead(); pNode; pNode = pNode->GetNext())
	{
		BehaviorUpdate(*pNode->Data);
	}

	return true;
}


void CCombatMountedAttackerGroup::Insert(CCombatMountedAttacker &attacker)
{
	Assert(!attacker.m_AttackerGroup);
	attacker.m_AttackerGroup = this;

	m_List.Append(attacker.m_ListNode);
}


void CCombatMountedAttackerGroup::Remove(CCombatMountedAttacker &attacker)
{
	if(attacker.m_InCircle)
	{
		RemoveMemberFromCircles(attacker);
	}

	Assert(attacker.m_AttackerGroup == this);
	attacker.m_AttackerGroup = NULL;

	m_List.PopNode(attacker.m_ListNode);
}


void CCombatMountedAttackerGroup::RemoveMemberFromCircles(CCombatMountedAttacker& attacker)
{
	for(int i = 0; i < kMaxCircles; i++)
	{
		CircleData &c = m_Circles[i];
		Assert(c.m_FlagActive || !c.m_AttackersUsingCircle.GetHead());

		if(attacker.m_InCircle == &c)
		{
			c.m_AttackersUsingCircle.PopNode(attacker.m_CircleNode);
			attacker.m_InCircle = NULL;
			break;
		}
	}

	attacker.m_CircleIsNew = false;
}

#if !__FINAL

void CCombatMountedAttackerGroup::DebugRender() const
{
#if DEBUG_DRAW
	for(int i = 0; i < kMaxCircles; i++)
	{
		const CircleData &c = m_Circles[i];

		if(!c.m_FlagActive)
		{
			continue;
		}

		Color32 col;
		if(c.m_FlagTested)
		{
			if(c.m_FlagTestedFreeFromObstructions)
			{
				col = Color32(0x40, 0xff, 0x40, 0x80);
			}
			else
			{
				col = Color32(0xff, 0x40, 0x40, 0x80);
			}
		}
		else
		{
			col = Color32(0x80, 0x80, 0x80, 0x80);
		}

		CNavMeshCircleTestHelper::DebugDraw(c.m_CenterAndRadius.GetXYZ(),
			c.m_CenterAndRadius.GetWf(), col);

		Vector3 p1;
		p1 = VEC3V_TO_VECTOR3(c.m_CenterAndRadius.GetXYZ());
		p1.x += c.m_CenterAndRadius.GetWf();

		char buff[256];
		formatf(buff, "%c", 'A' + i);
		grcDebugDraw::Text(p1, col, 0, 0, buff);

		const atDNode<CCombatMountedAttacker*>* pNode;
		for(pNode = c.m_AttackersUsingCircle.GetHead(); pNode; pNode = pNode->GetNext())
		{
			CPed* pPed = pNode->Data->GetPed();
			Color32 col2;
			col2 = Color32(0xff, 0xff, 0x40, 0x80);
			grcDebugDraw::Line(VECTOR3_TO_VEC3V(p1), pPed->GetTransform().GetPosition(), col2);
		}
	}
#endif	// DEBUG_DRAW
}

#endif	// !__FINAL

void CCombatMountedAttackerGroup::BehaviorUpdate(CCombatMountedAttacker &member)
{
	if(!member.m_RequestCircle)
	{
		return;
	}

	int activeCircleIndices[kMaxCircles];
	int numActiveCircles = 0;

	int invalidCircle = -1;
	if(m_LastCircleJoined >= 0 && m_TimeSinceLastCircleJoined < CTaskMoveCombatMounted::sm_Tunables.m_MinTimeSinceSameCircleJoined)
	{
		invalidCircle = m_LastCircleJoined;
	}

	int usingCircle = -1;
	for(int circleIndex = 0; circleIndex < kMaxCircles; circleIndex++)
	{
		const CircleData &c = m_Circles[circleIndex];

		if(c.m_FlagActive)
		{
			if(member.m_InCircle == &c)
			{
				usingCircle = circleIndex;
				break;
			}

			if(invalidCircle == circleIndex)
			{
				continue;
			}

			Assert(numActiveCircles < kMaxCircles);
			activeCircleIndices[numActiveCircles++] = circleIndex;
		}
	}

	if(usingCircle < 0 && numActiveCircles > 0
		&& m_TimeSinceLastCircleJoined > CTaskMoveCombatMounted::sm_Tunables.m_MinTimeSinceAnyCircleJoined)
	{
		int r = g_ReplayRand.GetRanged(0, numActiveCircles - 1);
		Assert(r >= 0 && r < kMaxCircles);

		const int circleIndex = activeCircleIndices[r];
		CircleData &c = m_Circles[circleIndex];

		if(c.m_FlagActive)
		{
			Assert(!member.m_InCircle);
			member.m_InCircle = &c;
			c.m_AttackersUsingCircle.Append(member.m_CircleNode);

			member.m_CircleIsNew = true;

			usingCircle = circleIndex;

			m_TimeSinceLastCircleJoined = 0.0f;
			m_LastCircleJoined = circleIndex;
		}
	}
}


void CCombatMountedAttackerGroup::CheckExistingCircles()
{
	bool considerGood = false;

	if(HasValidTarget())
	{
		const Vec3V tgtPosV = GetTargetPos();

		bool hasGood = false;
		bool good[kMaxCircles];
		for(int i = 0; i < kMaxCircles; i++)
		{
			CircleData &c = m_Circles[i];

			if(!c.m_FlagActive)
			{
				good[i] = false;
				continue;
			}

			const float thresholdSq = square(c.m_CenterAndRadius.GetWf()*0.5f);

			//			const float distSq = ((const Vector3&)c.m_CenterAndRadius).FlatDist2(tgtPos);
			Vec3V deltaV = Subtract(c.m_CenterAndRadius.GetXYZ(), tgtPosV);
			deltaV.SetZ(ScalarV(V_ZERO));
			const ScalarV distSqV = MagSquared(deltaV);
			const float distSq = distSqV.Getf();

			bool g;
			good[i] = g = distSq <= thresholdSq;
			if(g)
				hasGood = true;
		}

		if(hasGood || !m_CircleTestsRequested)
		{
			for(int i = 0; i < kMaxCircles; i++)
			{
				CircleData &c = m_Circles[i];
				if(c.m_FlagActive && !good[i])
				{
					c.Reset();
					if(i == m_LastCircleJoined)
					{
						m_LastCircleJoined = -1;
					}
				}
			}

			considerGood = true;
		}
	}
	else
	{
		for(int i = 0; i < kMaxCircles; i++)
		{
			CircleData &c = m_Circles[i];
			c.Reset();
		}
		m_LastCircleJoined = -1;
	}

	if(considerGood)
	{
		m_TimeNoGood = 0.0f;
	}
	else
	{
		m_TimeNoGood += TIME.GetSeconds();
	}
}


void CCombatMountedAttackerGroup::UpdateCircleTests()
{
	bool gotRequests = m_CircleTestsRequested;
	m_CircleTestsRequested = false;

	if(m_CircleBeingTested >= 0)
	{
		bool losIsClear = false;
		if(m_CircleTestHelper.GetTestResults(losIsClear))
		{
			Assert(m_CircleBeingTested >= 0 && m_CircleBeingTested < kMaxCircles);
			CircleData &c = m_Circles[m_CircleBeingTested];

			Assert(!c.m_FlagActive);

			c.m_CenterAndRadius = m_CircleBeingTestedCenterAndRadius;
			//			c.m_FlagActive = true;
			c.m_FlagTested = true;
			c.m_FlagTestedFreeFromObstructions = losIsClear;

			c.m_FlagActive = c.m_FlagTestedFreeFromObstructions;

			Assert(m_CurrentCircleTestRadiusIndex >= 0);
			c.m_RadiusIndex = m_CurrentCircleTestRadiusIndex;

			m_CircleBeingTested = -1;
			m_CurrentCircleTestRadiusIndex = -1;

		}
		else
		{
			return;
		}
	}

	if(!gotRequests)
	{
		return;
	}

	if(!HasValidTarget())
	{
		return;
	}

	const float tgtMoveDistSq = square(CTaskMoveCombatMounted::sm_Tunables.m_CircleTestsMoveDistToTestNewPos);

	int availableCircle = -1;
	m_CircleBeingTested = -1;
	for(int i = 0; i < kMaxCircles; i++)
	{
		if(!m_Circles[i].m_FlagActive)
		{
			availableCircle = i;
			break;
		}
	}

	if(availableCircle < 0)
	{
		return;
	}

	while(1)
	{
		const Vec3V targetPositionV = GetTargetPos();
		if(m_NextCircleTestRadiusIndex >= 0)
		{
			Vec3V deltaV = Subtract(targetPositionV, m_TargetPosForFirstCircleTest);
			deltaV.SetZ(ScalarV(V_ZERO));

			//			if(targetPosition.FlatDist2(m_TargetPosForFirstCircleTest) >= tgtMoveDistSq)
			if(MagSquared(deltaV).Getf() >= tgtMoveDistSq)
			{
				m_NextCircleTestRadiusIndex = -1;
			}
		}

		if(m_NextCircleTestRadiusIndex < 0)
		{
			m_TargetPosForFirstCircleTest = targetPositionV;
			m_NextCircleTestRadiusIndex = 0;
		}

		if(m_NextCircleTestRadiusIndex >= CTaskMoveCombatMounted::sm_Tunables.m_CircleTestRadii.GetCount())
		{
			return;
		}

		bool found = false;
		for(int i = 0; i < kMaxCircles; i++)
		{
			if(!m_Circles[i].m_FlagActive)
			{
				continue;
			}
			if(m_Circles[i].m_RadiusIndex == m_NextCircleTestRadiusIndex)
			{
				found = true;
				break;
			}
		}

		if(!found)
		{

			m_CircleBeingTested = availableCircle;

			//			m_CircleBeingTestedCenterAndRadius.SetVector3(m_MovementModel.GetTargetPosition());
			m_CircleBeingTestedCenterAndRadius.SetXYZ(GetTargetPos());

			m_CircleBeingTestedCenterAndRadius.SetWf(CTaskMoveCombatMounted::sm_Tunables.m_CircleTestRadii[m_NextCircleTestRadiusIndex]);

			m_CurrentCircleTestRadiusIndex = m_NextCircleTestRadiusIndex;

			// Not sure about testing for dynamic objects here. This code doesn't
			// really deal with changing environments (it assumes that tests that have
			// passed don't have to be repeated, etc), but if we don't include dynamic
			// objects in the tests, circles can pass through fences and other breakable
			// objects, it seems.
			bool testDynamicObjects = true;

			if(!m_CircleTestHelper.StartLosTest(m_CircleBeingTestedCenterAndRadius.GetXYZ(), m_CircleBeingTestedCenterAndRadius.GetWf(), testDynamicObjects))
			{
				m_CircleBeingTested = -1;

				// Do we need to do something else in this case? Needs to be tested.
				break;
			}
		}
		m_NextCircleTestRadiusIndex++;

		break;
	}
}

//-----------------------------------------------------------------------------
void CCombatMountedManager::Update()
{
	int i;
	const int num = m_MaxAttacks;
	for(i = 0; i < num; i++)
	{
		CCombatMountedAttackerGroup &atk = m_Attacks[i];
		if(!atk.IsActive())
			continue;

		if(!atk.Update())
		{
			DestroyAttack(atk);
		}
	}
}


void CCombatMountedManager::DestroyAllAttacks()
{
	int i;
	const int num = m_MaxAttacks;
	for(i = 0; i < num; i++)
	{
		CCombatMountedAttackerGroup &atk = m_Attacks[i];
		if(!atk.IsActive())
			continue;
		DestroyAttack(atk);
	}
}


void CCombatMountedManager::DestroyAttack(CCombatMountedAttackerGroup &atk)
{
	Assert(atk.IsActive());

	Assert(&atk - m_Attacks >= 0 && &atk - m_Attacks < m_MaxAttacks);
	atk.Shutdown();

	m_NumAttacks--;
	Assert(m_NumAttacks >= 0);
}


CCombatMountedAttackerGroup *CCombatMountedManager::FindAttackerGroup(const CEntity& tgt)
{
	const int num = m_MaxAttacks;
	int i;
	for(i = 0; i < num; i++)
	{
		const CCombatMountedAttackerGroup &atk = m_Attacks[i];
		if(!atk.IsActive())
		{
			continue;
		}
		if(atk.GetTarget() == &tgt)
		{
			return &m_Attacks[i];
		}
	}

	return NULL;
}


CCombatMountedAttackerGroup *CCombatMountedManager::FindOrCreateAttackerGroup(const CEntity& tgt)
{
	CCombatMountedAttackerGroup *atk = FindAttackerGroup(tgt);
	if(atk)
		return atk;
	atk = FindAvailableAttack();
	if(!atk)
		return atk;	// == NULL
	atk->Init(tgt);
	m_NumAttacks++;
	Assert(m_NumAttacks <= m_MaxAttacks);

	return atk;
}


#if __BANK

void CCombatMountedManager::AddWidgets(bkBank &bank)
{
	bank.PushGroup("Mounted Combat", false);
		bank.AddToggle("Draw", &m_BankDraw);
	bank.PopGroup();
}

#endif	// __BANK

#if !__FINAL

void CCombatMountedManager::DebugRender()
{
#if __BANK
	if(!m_BankDraw)
	{
		return;
	}

	const int num = m_MaxAttacks;
	int i;
	for(i = 0; i < num; i++)
	{
		const CCombatMountedAttackerGroup &atk = m_Attacks[i];
		if(!atk.IsActive())
		{
			continue;
		}
		atk.DebugRender();
	}
#endif	// __BANK
}

#endif	// !__FINAL

CCombatMountedManager::CCombatMountedManager()
: m_Attacks(NULL)
, m_MaxAttacks(0)
, m_NumAttacks(0)
{
	const int maxAttacks = fwConfigManager::GetInstance().GetSizeOfPool(ATSTRINGHASH("CombatMountedManager_Attacks", 0x9aad80fc), CONFIGURED_FROM_FILE);
	m_MaxAttacks = maxAttacks;
	if(maxAttacks > 0)
	{
		m_Attacks = rage_new CCombatMountedAttackerGroup[maxAttacks];
	}

#if __BANK
	m_BankDraw = false;
#endif
}


CCombatMountedManager::~CCombatMountedManager()
{
	DestroyAllAttacks();

	Assert(!m_NumAttacks);
	delete[] m_Attacks;
}


CCombatMountedAttackerGroup *CCombatMountedManager::FindAvailableAttack()
{
	const int num = m_MaxAttacks;

	if(m_NumAttacks >= num)
		return NULL;

	int i;
	for(i = 0; i < num; i++)
	{
		CCombatMountedAttackerGroup &atk = m_Attacks[i];
		if(!atk.IsActive())
		{
			return &atk;
		}
	}
	return NULL;
}

//-----------------------------------------------------------------------------

CCombatMountedAttacker::CCombatMountedAttacker()
: m_AttackerGroup(NULL)
, m_Ped(NULL)
{
	m_InCircle = NULL;
	m_CircleNode.Data = this;
	m_RequestCircle = false;
	m_CircleIsNew = false;
	m_ListNode.Data = this;
}


CCombatMountedAttacker::~CCombatMountedAttacker()
{
	Assert(!m_Ped);	// User should have called Shutdown().
}

//-----------------------------------------------------------------------------

