// File header
#include "Task/Crimes/RandomEventManager.h"

// Required headers
#include "debug/debugglobals.h"
#include "game/Dispatch/DispatchData.h"
#include "game/ModelIndices.h"
#include "game/wanted.h"
#include "modelinfo/ModelInfo.h"
#include "modelinfo/ModelInfo_Factories.h"
#include "network/NetworkInterface.h"
#include "Peds/Ped.h"
#include "Peds/CombatBehaviour.h"
#include "peds/PedDebugVisualiser.h"
#include "peds/PedFactory.h"
#include "peds/PlayerInfo.h"
#include "peds/PedIntelligence.h"
#include "peds/pedpopulation.h"
#include "scene/world/GameWorld.h"
#include "script/script.h"
#include "task/Combat/TaskNewCombat.h"
#include "Task/Crimes/TaskReactToPursuit.h"
#include "Task/Crimes/TaskPursueCriminal.h"
#include "Task/Crimes/TaskStealVehicle.h"
#include "vehicleAi/driverpersonality.h"
#include "vehicles/heli.h"
#include "vehicles/vehicle.h"
#include "vehicles/vehiclefactory.h"
#include "vehicles/vehiclepopulation.h"
#include "weapons/inventory/PedInventoryLoadOut.h"
#include "control/replay/replay.h"

//Rage header
#include "data/callback.h"

// Parser files
#include "CrimesInfoManager_parser.h"
#include "DefaultCrimeInfo_parser.h"


AI_OPTIMISATIONS()
AI_VEHICLE_OPTIMISATIONS()

////////////////////////////////////////////////////////////////////////////////

static const s32 MAX_CRIME_INFOS = 13;

#define LARGEST_CRIMES_INFO_CLASS sizeof(CStealVehicleCrime)

CompileTimeAssert(sizeof(CStealVehicleCrime) <= LARGEST_CRIMES_INFO_CLASS);

FW_INSTANTIATE_BASECLASS_POOL(CDefaultCrimeInfo, MAX_CRIME_INFOS, atHashString("CDefaultCrimeInfo",0x9fe48203), LARGEST_CRIMES_INFO_CLASS);

INSTANTIATE_RTTI_CLASS(CDefaultCrimeInfo,0x9FE48203);
INSTANTIATE_RTTI_CLASS(CStealVehicleCrime,0xDD3791C9);


CRandomEventManager::Tunables CRandomEventManager::sm_Tunables;

IMPLEMENT_RANDOM_EVENT_TUNABLES(CRandomEventManager, 0xda84b2ea);

// Static initialisation
CRandomEventManager CRandomEventManager::ms_Instance;

float CRandomEventManager::m_GlobalTimer = CRandomEventManager::sm_Tunables.m_EventInitInterval;
bool CRandomEventManager::m_GlobalTimerTicking = true;
bool CRandomEventManager::m_DebugMode = false;
float CRandomEventManager::m_SpawnPoliceChaseTimer = 0.0f;
strRequestArray<4> CRandomEventManager::m_streamingRequests;
bool CRandomEventManager::m_bSpawnHeli = false;
Vector3 CRandomEventManager::m_LastPoliceChasePlayerLocation = Vector3::ZeroType;


////////////////////////////////////////////////////////////////////////////////
// CRandomEventManager
////////////////////////////////////////////////////////////////////////////////

CRandomEventManager::CRandomEventManager() 
{
	ResetPossibilities();
}

CRandomEventManager::~CRandomEventManager()
{
}

void CRandomEventManager::Init(unsigned /*initMode*/)
{
	INIT_CRIMEINFOMGR;

	for ( u8 i = 0; i < sm_Tunables.m_RandomEventType.size(); ++i)
	{
		sm_Tunables.m_RandomEventType[i].m_RandomEventTimer = fwRandom::GetRandomNumberInRange(sm_Tunables.m_RandomEventType[i].m_RandomEventTimeIntervalMin,sm_Tunables.m_RandomEventType[i].m_RandomEventTimeIntervalMax);
	}

	CRandomEventManager::GetInstance().UpdatePossibilities();
}

////////////////////////////////////////////////////////////////////////////////

void CRandomEventManager::Shutdown(unsigned /*shutdownMode*/)
{
	FlushCopAsserts();
	SHUTDOWN_CRIMEINFOMGR;
}

////////////////////////////////////////////////////////////////////////////////

void CRandomEventManager::Update()
{
	if ( !sm_Tunables.m_Enabled REPLAY_ONLY(|| CReplayMgr::IsEditModeActive()) )
	{
		//release all the assets
		m_streamingRequests.ReleaseAll();
		return;
	}

	if(fwTimer::IsGamePaused())
	{
		//keep the requested assets alive
		m_streamingRequests.HaveAllLoaded();
		return;
	}

	if ( m_GlobalTimerTicking )
	{
		if (m_GlobalTimer > 0.0f)
		{
			m_GlobalTimer -= fwTimer::GetTimeStep();
		}
		else
		{
			m_GlobalTimerTicking = false;
		}
	}
	
	//Update event timers
	for (u8 i = 0; i < sm_Tunables.m_RandomEventType.size(); ++i )
	{
		if (sm_Tunables.m_RandomEventType[i].m_RandomEventTimer  > 0.0f && sm_Tunables.m_RandomEventData[i].m_Enabled)
		{
			float fScaleTimer = 100.0f;
			if(CanTickTimers(sm_Tunables.m_RandomEventType[i],fScaleTimer))
			{
				sm_Tunables.m_RandomEventType[i].m_RandomEventTimer -= ( fwTimer::GetTimeStep() * (fScaleTimer / 100.0f));
			}

			if (sm_Tunables.m_RandomEventType[i].m_RandomEventTimer  <= 0.0f)
			{
				CRandomEventManager::GetInstance().UpdatePossibilities();
			}
		}
		sm_Tunables.m_RandomEventData[i].m_Enabled = true;
	}


	//Check if we can start a police chase, update timer
	if (m_SpawnPoliceChaseTimer > 0.0f)
	{
		m_SpawnPoliceChaseTimer -= fwTimer::GetTimeStepInMilliseconds();
	}

	if(CanCreatePoliceChase())
	{
		CreatePoliceChase();
	}
}

////////////////////////////////////////////////////////////////////////////////
bool CRandomEventManager::CanTickTimers(RandomEventType iEventType, float& fScaleTimer)
{
	if ( NetworkInterface::IsGameInProgress() || CTheScripts::GetPlayerIsOnAMission() || CTheScripts::GetPlayerIsOnARandomEvent())
	{
		fScaleTimer = 0.0f;
		return false;
	}

	CPed* localPlayer = CPedFactory::GetFactory()->GetLocalPlayer();
	if (localPlayer)
	{
		if (localPlayer->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) && localPlayer->GetMyVehicle())
		{
			if (localPlayer->GetMyVehicle()->GetDistanceTravelled() <= ((0.02f)* 50.0f*fwTimer::GetTimeStep()))
			{
				fScaleTimer = iEventType.m_DeltaScaleWhenPlayerStationary; 
				return true;
			}
		}
		else
		{
			if (localPlayer->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_JUMPVAULT)) 
			{
				fScaleTimer = 100.0f;
				return true;
			}
			
			if (localPlayer->GetMotionData()->GetIsStill())
			{
				if ( (localPlayer->GetPedConfigFlag( CPED_CONFIG_FLAG_IsInTheAir ) == false) && (localPlayer->GetPedResetFlag( CPED_RESET_FLAG_IsLanding ) == false) && localPlayer->GetIsStanding() )
				{
					fwDynamicEntityComponent *pedDynComp = localPlayer->GetDynamicComponent();

					if (!pedDynComp || pedDynComp->GetAnimatedVelocity().XYMag2() == 0.0f)
					{
						fScaleTimer = iEventType.m_DeltaScaleWhenPlayerStationary;
						return true;
					}
				}
			}
		}
	}

	return true;
}

////////////////////////////////////////////////////////////////////////////////

bool CRandomEventManager::CanCreateEventOfType(eRandomEvent aType, bool bIgnoreGlobalTimer, bool bIgnoreIndividualTimer)
{
	if (!sm_Tunables.m_Enabled)
	{
		return false;
	}

	if ( m_GlobalTimerTicking && !bIgnoreGlobalTimer )
	{	
		return false;
	}

	Assertf(sm_Tunables.m_RandomEventData.GetCount() > aType, "Asking for an invalid event type! Check that 'build/dev/common/data/tune/randomevents.meta' matches 'eRandomEvent' from RandomEventManager.h");
	
	// Check it's currently a possibility
	if (!bIgnoreIndividualTimer && !sm_Tunables.m_RandomEventData[aType].m_RandomEventPossibility)
	{
		return false;
	}

	if(!bIgnoreIndividualTimer && !sm_Tunables.m_RandomEventData[aType].m_Enabled)
	{
		return false;
	}

	if ( NetworkInterface::IsGameInProgress() || CTheScripts::GetPlayerIsOnAMission() || CTheScripts::GetPlayerIsOnARandomEvent() )
	{
		return false;
	}

	if(aType == RC_COP_PURSUE_VEHICLE_FLEE_SPAWNED || aType == RC_COP_PURSUE )
	{
		if( CGameWorld::FindLocalPlayer() && 
			CGameWorld::FindLocalPlayer()->GetPlayerWanted() && 
			(CGameWorld::FindLocalPlayer()->GetPlayerWanted()->GetWantedLevel() > WANTED_CLEAN)) 
		{ 
			return false; 
		} 

		if(CVehiclePopulation::GetRandomVehDensityMultiplier() == 0.0f)
		{
			return false;
		}
	
		if(CVehiclePopulation::ms_numRandomCars >= CRandomEventManager::sm_Tunables.m_MaxAmbientVehiclesToSpawnChase)
		{
			return false;
		}

		CPed* localPlayer = CPedFactory::GetFactory()->GetLocalPlayer();
		if( !m_DebugMode && (localPlayer && !m_LastPoliceChasePlayerLocation.IsZero() && 
			(VEC3V_TO_VECTOR3(localPlayer->GetTransform().GetPosition()) - m_LastPoliceChasePlayerLocation).Mag2() < rage::square(CRandomEventManager::sm_Tunables.m_MinPlayerMoveDistanceToSpawnChase)) )
		{
			return false;
		}
	}

	return true;
}

////////////////////////////////////////////////////////////////////////////////

void CRandomEventManager::SetEventStarted( eRandomEvent aType ) 
{
	Assert(sm_Tunables.m_Enabled);
	
	//Update history
	float fHistory = GetRandomEventHistory(aType);
	SetRandomEventHistory(aType, fHistory+1);

	//update m_LastPoliceChaseStartLocation 
	if(aType == RC_COP_PURSUE_VEHICLE_FLEE_SPAWNED || aType == RC_COP_PURSUE )
	{
		CPed* localPlayer = CPedFactory::GetFactory()->GetLocalPlayer();
		if(localPlayer)
		{
			m_LastPoliceChasePlayerLocation = VEC3V_TO_VECTOR3(localPlayer->GetTransform().GetPosition());
		}
	}

	if (!m_DebugMode)
	{
		//Update timer for that type
		eRandomEventType eventType = sm_Tunables.m_RandomEventData[aType].m_RandomEventType;
		sm_Tunables.m_RandomEventType[eventType].m_RandomEventTimer = GetRandomEventTimerInterval(eventType);

		//Update possibilities
		UpdatePossibilities();

		//Update global timers
		m_GlobalTimerTicking = true; 
		m_GlobalTimer = sm_Tunables.m_EventInterval;
	}
}

////////////////////////////////////////////////////////////////////////////////

void CRandomEventManager::ResetPossibilities()
{
	for ( u8 i = 1; i < sm_Tunables.m_RandomEventData.size(); ++i)
	{
		ResetRandomEvent(i);
	}
}

////////////////////////////////////////////////////////////////////////////////

void CRandomEventManager::UpdatePossibilities()
{
	//Standardised crime history
	for ( u8 i = 0; i < sm_Tunables.m_RandomEventData.size(); i++)
	{
		if (GetRandomEventHistory(i) > 128.0f)
		{
			eRandomEventType eventType = GetRandomEventType(i);
			for ( u8 j = 0; j < sm_Tunables.m_RandomEventData.size();j++)
			{
				eRandomEventType otherEventType = GetRandomEventType(j);
				float fHistory = GetRandomEventHistory(j);
				if (fHistory > 0 && eventType == otherEventType)
				{
					SetRandomEventHistory(j, fHistory-1);
				}
			}
		}
	}

	//Set Crime possibilities 
	for (u8 i = 0; i < sm_Tunables.m_RandomEventType.size(); i++ )
	{
		eRandomEventType eventType = (eRandomEventType)i;

		//If the timer is still ticking then non of the events of this type are possible.
		if ( sm_Tunables.m_RandomEventType[i].m_RandomEventTimer > 0.0f)
		{
			for ( u8 j = 0; j < sm_Tunables.m_RandomEventData.size(); j++)
			{
				if (eventType == GetRandomEventType(j))
				{
					SetRandomEventPossibility(j,false);
				}
			}
			continue;
		}

		float lowest = 128.0f;
		for ( u8 j = 0; j < sm_Tunables.m_RandomEventData.size(); j++)
		{
			if (eventType == GetRandomEventType(j))
			{
				float fHistory = GetRandomEventHistory(j);
				if (fHistory < lowest)
				{
					lowest = fHistory;
				}
			}
		}

		for ( u8 j = 0; j < sm_Tunables.m_RandomEventData.size(); j++)
		{
			if (eventType == GetRandomEventType(j))
			{
				float fHistory = GetRandomEventHistory(j);
				if (fHistory == lowest)
				{
					SetRandomEventPossibility(j,true);
				}
				else
				{
					SetRandomEventPossibility(j,false);
				}
			}
		}
	}
}

float CRandomEventManager::GetRandomEventTimerInterval(eRandomEventType eventType)
{	
	return fwRandom::GetRandomNumberInRange(sm_Tunables.m_RandomEventType[(s32)eventType].m_RandomEventTimeIntervalMin,sm_Tunables.m_RandomEventType[(s32)eventType].m_RandomEventTimeIntervalMax);

}

float& CRandomEventManager::GetRandomEventTimer(eRandomEventType eventType) const
{	
	return sm_Tunables.m_RandomEventType[(s32)eventType].m_RandomEventTimer;
}

eRandomEventType CRandomEventManager::GetRandomEventType(s32 index) const
{
	return sm_Tunables.m_RandomEventData[index].m_RandomEventType;
} 

void CRandomEventManager::SetRandomEventPossibility(s32 index, bool bPossible)
{
	sm_Tunables.m_RandomEventData[index].m_RandomEventPossibility = bPossible;
}

bool& CRandomEventManager::GetRandomEventPossibility(s32 index) const
{
	return sm_Tunables.m_RandomEventData[index].m_RandomEventPossibility;
}

void CRandomEventManager::ResetRandomEvent(s32 index)
{
	sm_Tunables.m_RandomEventData[index].m_RandomEventHistory = 0;
	sm_Tunables.m_RandomEventData[index].m_RandomEventPossibility = true;
}

void CRandomEventManager::SetRandomEventHistory(s32 index, float history)
{
	sm_Tunables.m_RandomEventData[index].m_RandomEventHistory = history;
}

float& CRandomEventManager::GetRandomEventHistory(s32 index) const
{
	return sm_Tunables.m_RandomEventData[index].m_RandomEventHistory;
}

void CRandomEventManager::SetEventTypeEnabled(s32 index, bool var)
{
	sm_Tunables.m_RandomEventData[index].m_Enabled = var;
}

bool CRandomEventManager::GetEventTypeEnabled(s32 index) const
{
	return sm_Tunables.m_RandomEventData[index].m_Enabled;
}

void CRandomEventManager::ResetGlobalTimerTicking()
{
	//Update global timers
	m_GlobalTimerTicking = true; 
	m_GlobalTimer = sm_Tunables.m_EventInterval;
}

bool CRandomEventManager::CanCreatePoliceChase()
{
	//keep the requested assets alive
	m_streamingRequests.HaveAllLoaded();
	
#if !__FINAL
	if(!gbAllowVehGenerationOrRemoval || !gbAllowCopGenerationOrRemoval)
	{
		return false;
	}
#endif

	if(!CRandomEventManager::sm_Tunables.m_SpawningChasesEnabled)
	{
		return false;
	}

	if(!CRandomEventManager::GetInstance().CanCreateEventOfType(RC_COP_PURSUE_VEHICLE_FLEE_SPAWNED))
	{
		return false;
	}

	if( CPed::GetPool()->GetNoOfFreeSpaces() <= 10 ||
		CVehicle::GetPool()->GetNoOfFreeSpaces() <= 10 )
	{
		return false;
	}

	if( CGameWorld::FindLocalPlayerWanted() && CGameWorld::FindLocalPlayerWanted()->GetWantedLevel() > WANTED_CLEAN)
	{
		return false;
	}

	CPed* pPlayerPed = FindPlayerPed();
	if (pPlayerPed && pPlayerPed->GetPedIntelligence()->IsBattleAware())
	{
		return false;
	}

	//Cop car vehicle + ped
	CConditionalVehicleSet* pVehicleSet = CDispatchData::FindConditionalVehicleSet(ATSTRINGHASH("POLICE_CAR",0xea4dcda7));
	if(!pVehicleSet)
	{
		return false;
	}

	// Get the vehicle/ped model name array and loop through
	fwModelId iVehicleModelId;
	const atArray<atHashWithStringNotFinal>& vehicleModelNames = pVehicleSet->GetVehicleModelHashes();
	for(int j = 0; j < vehicleModelNames.GetCount(); j++)
	{
		CModelInfo::GetBaseModelInfoFromHashKey(vehicleModelNames[j].GetHash(), &iVehicleModelId);
	}

	fwModelId driverPedModelId;
	const atArray<atHashWithStringNotFinal>& pedModelNames = pVehicleSet->GetPedModelHashes();
	for(int j = 0; j < pedModelNames.GetCount(); j++)
	{
		// We invalidate our model ID then get it, if it comes back valid we request a stream on it
		CModelInfo::GetBaseModelInfoFromHashKey(pedModelNames[j].GetHash(), &driverPedModelId);
	}

	fwModelId iVehicleHeliModelId;
	CModelInfo::GetBaseModelInfoFromHashKey(CRandomEventManager::sm_Tunables.m_HeliVehicleModelId, &iVehicleHeliModelId);

	fwModelId driverHeliPedModelId;
	CModelInfo::GetBaseModelInfoFromHashKey(CRandomEventManager::sm_Tunables.m_HeliPedModelId, &driverHeliPedModelId);

	if(m_streamingRequests.GetRequestCount() == 0)
	{
		//Stream the require assets
		m_streamingRequests.ReleaseAll();

		// We invalidate our model ID then get it, if it comes back valid we request a stream on it
		if(Verifyf(iVehicleModelId.IsValid(), "CRandomEventManager::CanCreatePoliceChase Vehicle model not found"))
		{
			s32 transientLocalIdx = CModelInfo::AssignLocalIndexToModelInfo(iVehicleModelId).Get();
			m_streamingRequests.PushRequest(transientLocalIdx, CModelInfo::GetStreamingModuleId());
		}

		if(Verifyf(driverPedModelId.IsValid(), "CRandomEventManager::CanCreatePoliceChase Ped model not found"))
		{
			s32 transientLocalIdx = CModelInfo::AssignLocalIndexToModelInfo(driverPedModelId).Get();
			m_streamingRequests.PushRequest(transientLocalIdx, CModelInfo::GetStreamingModuleId());
		}
		
		m_bSpawnHeli = false;
		if(sm_Tunables.m_ProbSpawnHeli > fwRandom::GetRandomNumberInRange(0,99))
		{
			m_bSpawnHeli = true;
			
			//heli vehicle
			if(Verifyf(iVehicleHeliModelId.IsValid(),"CRandomEventManager::CanCreatePoliceChase iVehicleHeliModelId Invalid model id"))
			{
				s32 transientLocalIdx = CModelInfo::AssignLocalIndexToModelInfo(iVehicleHeliModelId).Get();
				m_streamingRequests.PushRequest(transientLocalIdx, CModelInfo::GetStreamingModuleId());
			}

			//Heli ped
			if(Verifyf(driverHeliPedModelId.IsValid(),"CRandomEventManager::CanCreatePoliceChase driverHeliPedModelId Invalid model id"))
			{
				s32 transientLocalIdx = CModelInfo::AssignLocalIndexToModelInfo(driverHeliPedModelId).Get();
				m_streamingRequests.PushRequest(transientLocalIdx, CModelInfo::GetStreamingModuleId());
			}
		}

		// This needs to be called to add refs to the requests.
		m_streamingRequests.HaveAllLoaded();
		
		return false;
	}
	else if (!m_streamingRequests.HaveAllLoaded())
	{
		return false;
	}

	if(m_SpawnPoliceChaseTimer > 0.0f)
	{
		return false;
	}

	//Double check if the vehicle assets are loaded
	if(!CModelInfo::HaveAssetsLoaded(iVehicleModelId) || !CModelInfo::HaveAssetsLoaded(driverPedModelId) || (m_bSpawnHeli && !CModelInfo::HaveAssetsLoaded(iVehicleHeliModelId)))
	{
		//If the assets aren't loaded then we require different assets so reset the streaming.
		m_streamingRequests.ReleaseAll();
		return false;
	}


	return true;
}

void CRandomEventManager::CreatePoliceChase()
{
	//Reset timer so we don't attempted to spawn another chase
	m_SpawnPoliceChaseTimer = 2000.0f;

	CNodeAddress CopSpawnNode;
	CNodeAddress CopSpawnPreviousNode;

	//Grab the conditional vehicle set.
	CConditionalVehicleSet* pVehicleSet = CDispatchData::FindConditionalVehicleSet(ATSTRINGHASH("POLICE_CAR",0xea4dcda7));
	if(!pVehicleSet)
	{
		return;
	}

	const atArray<atHashWithStringNotFinal>& vehicleModelNames = pVehicleSet->GetVehicleModelHashes();

	//Spawn a police vehicle
	fwModelId modelId;
	CModelInfo::GetBaseModelInfoFromHashKey(vehicleModelNames[0].GetHash(), &modelId);
	
	//Create cop vehicle
	CVehicle* pCopVehicle = CVehiclePopulation::GenerateCopVehicleChasingCriminal(CGameWorld::FindLocalPlayerCoors(), modelId, CopSpawnNode, CopSpawnPreviousNode);
	
	if(!pCopVehicle)
	{
		//If we failed to spawn a cop vehicle then release the streaming and request it again, we could need new models now
		m_streamingRequests.ReleaseAll();
		return;
	}

	static dev_float s_fChancesToDoDriveBys = 0.0f;
	bool bCanDoDriveBys = (fwRandom::GetRandomNumberInRange(0.0f, 1.0f) < s_fChancesToDoDriveBys);

	if(!SetUpCopPedsInVehicle(pCopVehicle, bCanDoDriveBys))
	{
		CVehiclePopulation::RemoveVeh(pCopVehicle, true, CVehiclePopulation::Removal_None);
		return;
	}
	
	//Spawn a criminal vehicle
	CVehicle* pCriminalVehicle = CVehiclePopulation::GenerateOneCriminalVehicle(CopSpawnNode,CopSpawnPreviousNode, bCanDoDriveBys);
	if(!pCriminalVehicle  || !pCriminalVehicle->GetDriver())
	{
		if(pCriminalVehicle)
		{
			CVehiclePopulation::RemoveVeh(pCriminalVehicle, true, CVehiclePopulation::Removal_None);
		}
		CVehiclePopulation::RemoveVeh(pCopVehicle, true, CVehiclePopulation::Removal_None);
		return;
	}

	//Give the cop the pursuit task
	SetUpCopPursuitTasks(pCopVehicle, pCriminalVehicle, true);

	//Spawn heli if possible
	if(m_bSpawnHeli)
	{
		fwModelId heliModelId;
		CModelInfo::GetBaseModelInfoFromHashKey(CRandomEventManager::sm_Tunables.m_HeliVehicleModelId, &heliModelId);

		fwModelId driverHeliPedModelId;
		CModelInfo::GetBaseModelInfoFromHashKey(CRandomEventManager::sm_Tunables.m_HeliPedModelId, &driverHeliPedModelId);

		CHeli* pHeli = CHeli::GenerateHeli(pCriminalVehicle->GetDriver(), heliModelId.GetModelIndex(),driverHeliPedModelId.GetModelIndex());
		if(pHeli && pHeli->GetDriver())
		{
			pHeli->SetLiveryId(0);
			pHeli->SetLivery2Id(0);
			pHeli->UpdateBodyColourRemapping();
			pHeli->m_nVehicleFlags.bCanEngineDegrade = false;

			// Schedule addition cops in the helicopter
			// Use the dispatch system callback to give them the right loadout (LOADOUT_SWAT_NO_LASER) - see CWantedHelicopterDispatch::DispatchDelayedSpawnCallback
			CPedPopulation::SchedulePedInVehicle(pHeli, 2, false, fwModelId::MI_INVALID, NULL, NULL, DT_SwatHelicopter);				
			CPedPopulation::SchedulePedInVehicle(pHeli, 3, false, fwModelId::MI_INVALID, NULL, NULL, DT_SwatHelicopter);				

			//Add the followers.
			for(s32 iSeat = 0; iSeat < pHeli->GetSeatManager()->GetMaxSeats(); ++iSeat)
			{
				//Ensure the ped is valid.
				CPed* pPed = pHeli->GetSeatManager()->GetPedInSeat(iSeat);
				if(!pPed)
				{
					continue;
				}

				//Add the inventory load-out (?).
				static const atHashWithStringNotFinal SWAT_HELI_LOADOUT("LOADOUT_SWAT_NO_LASER",0xC9705531);
				CPedInventoryLoadOutManager::SetLoadOut(pPed, SWAT_HELI_LOADOUT.GetHash());
			}

			pHeli->GetDriver()->GetPedIntelligence()->GetCombatBehaviour().SetTargetLossResponse(CCombatData::TLR_NeverLoseTarget);


			CTask* pTask = rage_new CTaskCombat(pCriminalVehicle->GetDriver());
			CEventGivePedTask event(PED_TASK_PRIORITY_EVENT_RESPONSE_NONTEMP, pTask);
			pHeli->GetDriver()->GetPedIntelligence()->AddEvent(event);
		}
		else if(pHeli)
		{
			//No driver, remove the heli
			CVehiclePopulation::RemoveVeh(pHeli, true, CVehiclePopulation::Removal_None);
			return;
		}
	}

	//Spawn addition cop vehicles if needed, start at one as we have already spawned one
	int iNumPoliceVehicles = fwRandom::GetRandomNumberInRange(1,sm_Tunables.m_MaxNumberCopVehiclesInChase);
	for(int i = 1; i < iNumPoliceVehicles; ++i)
	{
		CVehicle* pVehicle = CVehiclePopulation::GenerateCopVehicleChasingCriminal(CGameWorld::FindLocalPlayerCoors(), modelId, CopSpawnNode, CopSpawnPreviousNode);

		if(!pVehicle)
		{
			break;
		}

		if(!SetUpCopPedsInVehicle(pVehicle, bCanDoDriveBys))
		{
			CVehiclePopulation::RemoveVeh(pVehicle, true, CVehiclePopulation::Removal_None);
			break;
		}

		//Give the cop the pursuit task
		SetUpCopPursuitTasks(pVehicle, pCriminalVehicle, false);		
	}

	m_streamingRequests.ReleaseAll();
}

bool CRandomEventManager::SetUpCopPedsInVehicle(CVehicle* pVehicle, bool bCanDoDriveBys)
{
	Assertf(pVehicle, "SetUpCopPedsInVehicle no vehicle");
	if(!CVehiclePopulation::AddPoliceVehOccupants(pVehicle, true) || !pVehicle->GetDriver())
	{
		return false;
	}

	//Set the ped attributes.
	for(int i = 0; i < pVehicle->GetSeatManager()->GetMaxSeats(); ++i)
	{
		CPed* pPedInSeat = pVehicle->GetPedInSeat(i);
		if(pPedInSeat)
		{
			pPedInSeat->GetPedIntelligence()->GetCombatBehaviour().ChangeFlag(CCombatData::BF_CanDoDrivebys, bCanDoDriveBys);
		}
	}

	return true;
}

void CRandomEventManager::SetUpCopPursuitTasks(CVehicle* pVehicle, CVehicle* pCriminalVehicle, bool bLeader)
{
	//Assert that the criminal vehicle is valid.
	aiAssert(pCriminalVehicle);

	//Check if the vehicle is valid.
	if(aiVerify(pVehicle))
	{
		//Iterate over the seats.
		for(int i = 0; i < pVehicle->GetSeatManager()->GetMaxSeats(); ++i)
		{
			//Check if the cop is valid.
			CPed* pCop = pVehicle->GetSeatManager()->GetPedInSeat(i);
			if(pCop)
			{
				//Give the task to the ped.
				bool bIsLeader = (bLeader && (i == 0));
				CTaskPursueCriminal::GiveTaskToPed(*pCop, pCriminalVehicle, true, bIsLeader);
			}
		}
	}
}


void CRandomEventManager::FlushCopAsserts()
{
	m_streamingRequests.ReleaseAll();
}

////////////////////////////////////////////////////////////////////////////////

#if __BANK

void CRandomEventManager::AddWidgets(bkBank& bank)
{
	bank.PushGroup("Random Events",					false);
	bank.AddToggle("Enable Debug Mode",				&m_DebugMode,CRandomEventManager::EnableDebugMode);	


	bank.AddToggle("Disable additional checks in scan",		&CRandomEventManager::GetInstance().GetTunables().m_ForceCrime);	
	bank.AddButton("Create police ped",				CPedDebugVisualiserMenu::CreatePolicePursuitCB );
	bank.AddButton("Create vehicle police",			CPedDebugVisualiserMenu::CreateVehiclePolicePursuitCB );
	bank.AddSlider("Global Timer",					&CRandomEventManager::m_GlobalTimer, 0.0f, 400.0f, 0.1f);
	bank.PushGroup("Event history",					true);
	{
		for ( u8 i = 0; i < RC_MAX; ++i)
		{
			float& history = CRandomEventManager::GetInstance().GetRandomEventHistory(i);
			bank.AddSlider(CRandomEventManager::GetInstance().GetEventName((eRandomEvent)i),	&history, -10.0f, 128.0f, 1.0f);
			bool& possiblitiy = CRandomEventManager::GetInstance().GetRandomEventPossibility(i);

			bank.AddToggle("Possible",  &possiblitiy,datCallback(CFA1 (CRandomEventManager::UpdatePossibilitiesCB),(CallbackData)(size_t)i));
		}
	}

	bank.PushGroup("Event type timers",				true);
	{
		for ( u8 i = 0; i < ET_MAX; ++i)
		{
			float& timer = CRandomEventManager::GetInstance().GetRandomEventTimer((eRandomEventType)i);
			bank.AddSlider(CRandomEventManager::GetInstance().GetEventTypeName((eRandomEventType)i),	&timer, -1.0f, 2000.0f, 1.0f);
		}
	}

	bank.PopGroup();
	bank.PopGroup();
	bank.PopGroup();	
}

void CRandomEventManager::UpdatePossibilitiesCB( CallbackData data)
{
	int button = (int)(size_t)data;

	bool& possiblitiy = CRandomEventManager::GetInstance().GetRandomEventPossibility(button);

	if (m_DebugMode)
	{
		float& history = CRandomEventManager::GetInstance().GetRandomEventHistory(button);
		if(possiblitiy)
		{
			history = 0.0f;
		}
		else
		{
			history = 128.0f;
		}
	}
}

void CRandomEventManager::EnableDebugMode()
{
	for ( u8 i = 0; i < RC_MAX; ++i)
	{
		float& history = CRandomEventManager::GetInstance().GetRandomEventHistory(i);
		bool& possiblitiy = CRandomEventManager::GetInstance().GetRandomEventPossibility(i);

		if (m_DebugMode)
		{
			history = 128.0f;
			possiblitiy = false;
		}
		else
		{
			history = 0.0f;
		}
	}

	if (m_DebugMode)
	{
		for ( u8 i = 0; i < ET_MAX; ++i)
		{
			float& timer = CRandomEventManager::GetInstance().GetRandomEventTimer((eRandomEventType)i);

			if (m_DebugMode)
			{
				timer = 0.0f;
			}
		}

		m_GlobalTimerTicking = false;
		m_GlobalTimer = 0.0f;
	}
	else
	{
		for ( u8 i = 0; i < sm_Tunables.m_RandomEventType.size(); ++i)
		{
			sm_Tunables.m_RandomEventType[i].m_RandomEventTimer = fwRandom::GetRandomNumberInRange(sm_Tunables.m_RandomEventType[i].m_RandomEventTimeIntervalMin,sm_Tunables.m_RandomEventType[i].m_RandomEventTimeIntervalMax);
		}
		CRandomEventManager::GetInstance().UpdatePossibilities();

		//Update global timers
		m_GlobalTimerTicking = true; 
		m_GlobalTimer = sm_Tunables.m_EventInterval;
	}
}

const char* CRandomEventManager::GetEventName(eRandomEvent aEvent) const
{
	taskAssert(aEvent >= RC_INVALID && aEvent < RC_MAX);

	switch (aEvent)
	{
		case RC_INVALID:								return "Invalid";
		case RC_PED_STEAL_VEHICLE:						return "Ped steal vehicle";
		case RC_PED_JAY_WALK_LIGHTS:					return "Ped jay walking";
		case RC_COP_PURSUE:								return "Cop pursue";
		case RC_COP_PURSUE_VEHICLE_FLEE_SPAWNED:		return "Spawned cop pursue vehicle flee";
		case RC_COP_VEHICLE_DRIVING_FAST:				return "Cop vehicle driving fast";
		case RC_DRIVER_SLOW:							return "Slow Cruisin Driver";
		case RC_DRIVER_RECKLESS:						return "Reckless Driver";
		case RC_DRIVER_PRO:								return "Pro Driver";
		case RC_PED_PURSUE_WHEN_HIT_BY_CAR:				return "Ped pursue when hit by car";
		default: taskAssert(0); 	
	}
	return "CRandomEventManager::GetEventName, Missing debug text, invalid";
}

const char* CRandomEventManager::GetEventTypeName(eRandomEventType aEventType) const
{
	taskAssert(aEventType >= ET_INVALID && aEventType < ET_MAX);

	switch (aEventType)
	{
		case ET_INVALID:			return "Invalid";
		case ET_CRIME:				return "Crime";
		case ET_JAYWALKING:			return "Jay Walking";
		case ET_COP_PURSUIT:		return "Cop Pursuit";
		case ET_SPAWNED_COP_PURSUIT:return "Spawned cop Pursuit";
		case ET_AMBIENT_COP:		return "Ambient Cop";
		case ET_INTERESTING_DRIVER:	return "Interesting Driver";
		case ET_AGGRESSIVE_DRIVER:	return "Aggressive Driver";
		default: taskAssert(0); 	
	}
	return "CRandomEventManager::GetEventTypeName, Missing debug text, invalid";
}


void CRandomEventManager::DebugRender()
{
	if(sm_Tunables.m_RenderDebug)
	{
		char timerDebug[256] = "";
		formatf(timerDebug, 255, "Enabled:%i\n" ,sm_Tunables.m_Enabled );
		grcDebugDraw::Text(Vector2(0.1f,0.1f),Color32(255,255,255),timerDebug);
	}
}

void CRandomEventManager::SetForceCrime()
{
	sm_Tunables.m_ForceCrime = true;
}

#endif //__BANK

//-------------------------------------------------------------------------
// Returns the crime info data for a particular scenario
//-------------------------------------------------------------------------
CDefaultCrimeInfo* CRandomEventManager::GetCrimeInfo(s32 iCrimeType)
{
	return CRIMEINFOMGR.GetCrimeInfoByIndex(iCrimeType);
}

////////////////////////////////////////////////////////////////////////////////
// CCrimeInfoManager
////////////////////////////////////////////////////////////////////////////////

CCrimeInfoManager::CCrimeInfoManager()
{
	// Initialise pools
	CDefaultCrimeInfo::InitPool(MEMBUCKET_GAMEPLAY);

	// Load
	Load();
}

////////////////////////////////////////////////////////////////////////////////

CCrimeInfoManager::~CCrimeInfoManager()
{
	// Delete the data
	Reset();

	// Shutdown pools
	CDefaultCrimeInfo::ShutdownPool();
}


////////////////////////////////////////////////////////////////////////////////

#if __BANK
void CCrimeInfoManager::AddWidgets(bkBank& bank)
{
	bank.PushGroup("Crimes Info");

	bank.AddButton("Load", datCallback(MFA(CCrimeInfoManager::Load), this));
	bank.AddButton("Save", datCallback(MFA(CCrimeInfoManager::Save), this));

	AddParserWidgets(&bank);

	bank.PopGroup();
}
#endif // __BANK

////////////////////////////////////////////////////////////////////////////////

void CCrimeInfoManager::Reset()
{
	for(s32 i = 0; i < m_Crimes.GetCount(); i++)
	{
		delete m_Crimes[i];
	}
	m_Crimes.Reset();
}

////////////////////////////////////////////////////////////////////////////////

void CCrimeInfoManager::Load()
{
#if __BANK
	// As the 2dfx store the index into the array for the relevant crimes,
	// we have to update them in case the order has changed
	atArray<u32> indices(0, m_Crimes.GetCount());
	for(s32 i = 0; i < m_Crimes.GetCount(); i++)
	{
		indices.Push(m_Crimes[i]->GetHash());
	}
#endif // __BANK

	// Delete any existing data
	Reset();

	// Load
	PARSER.LoadObject("common:/data/ai/crimes", "meta", *this);

#if __BANK

	// Update the widgets
	AddParserWidgets(NULL);
#endif // __BANK
}

////////////////////////////////////////////////////////////////////////////////

#if __BANK
void CCrimeInfoManager::Save()
{
	Verifyf(PARSER.SaveObject("common:/data/crimes", "meta", this, parManager::XML), "File is probably write protected");
}
#endif // __BANK

////////////////////////////////////////////////////////////////////////////////

#if __BANK
void CCrimeInfoManager::AddParserWidgets(bkBank* pBank)
{
	bkGroup* pGroup = NULL;
	if(!pBank)
	{
		pBank = BANKMGR.FindBank("Crimes");
		if(!pBank)
		{
			return;
		}

		bkWidget* pWidget = BANKMGR.FindWidget("Crimes/Crimes Info/Infos");
		if(!pWidget)
		{
			return;
		}

		pWidget->Destroy();

		pWidget = BANKMGR.FindWidget("Crimes/Crimes Info");
		if(pWidget)
		{
			pGroup = dynamic_cast<bkGroup*>(pWidget);
			if(pGroup)
			{
				pBank->SetCurrentGroup(*pGroup);
			}
		}
	}

	pBank->PushGroup("Infos");
	for(s32 i = 0; i < m_Crimes.GetCount(); i++)
	{
		pBank->PushGroup(m_Crimes[i]->GetName());
		PARSER.AddWidgets(*pBank, m_Crimes[i]);
		pBank->PopGroup();
	}
	pBank->PopGroup();

	if(pGroup)
	{
		pBank->UnSetCurrentGroup(*pGroup);
	}
}

#endif
