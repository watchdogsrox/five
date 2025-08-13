// Title	:	DispatchMangaer.cpp
// Purpose	:	Manages the separate dispatch subsystems
// Started	:	13/05/2010

#include "Game/Dispatch/DispatchManager.h"

// Game includes
#include "Control/Gamelogic.h"
#include "Debug/DebugScene.h"
#include "Game/Dispatch/DispatchData.h"
#include "Game/Dispatch/DispatchServices.h"
#include "Game/Dispatch/IncidentManager.h"
#include "Game/Dispatch/OrderManager.h"
#include "Network/Objects/NetworkObjectPopulationMgr.h"
#include "Network/Objects/Entities/NetObjHeli.h"
#include "Network/Objects/Entities/NetObjPed.h"
#include "Network/Objects/Entities/NetObjVehicle.h"
#include "peds/ped.h"
#include "peds/PedFactory.h"
#include "Peds/PedIntelligence.h"
#include "peds/PopCycle.h"
#include "peds/PopZones.h"
#include "peds/rendering/PedVariationDebug.h"
#include "scene/world/GameWorld.h"
#include "Task/Service/Fire/TaskFirePatrol.h"
#include "Task/vehicle/TaskCar.h"
#include "vehicleAi/VehicleIntelligence.h"
#include "Vehicles/VehicleFactory.h"
#include "Vehicles/vehiclepopulation.h"
#include "vfx/misc/Fire.h"

// Framework includes
#include "ai/aichannel.h"

AI_OPTIMISATIONS()

CDispatchManager CDispatchManager::ms_instance;

bool CDispatchManager::m_bShouldReleaseStreamRequests = false;
float CDispatchManager::m_fLawResponseDelayOverride = -1.0f;
int CDispatchManager::m_iCurrZoneVehicleResponseType = VEHICLE_RESPONSE_DEFAULT;
eWantedLevel CDispatchManager::m_nPlayerWantedLevel = WANTED_CLEAN;
const char* CDispatchManager::ms_DispatchTypeNames[DT_Max] = {"DT_Invalid", "DT_PoliceAutomobile", "DT_PoliceHelicopter", "DT_FireDepartment", "DT_SwatAutomobile", 
														"DT_AmbulanceDepartment", "DT_PoliceRiders", "DT_PoliceVehicleRequest", "DT_PoliceRoadBlock", 
														"DT_PoliceAutomobileWaitPulledOver", "DT_PoliceAutomobileWaitCruising", "DT_Gangs", "DT_SwatHelicopter",
														"DT_PoliceBoat", "DT_ArmyVehicle", "DT_BikerBackup", "DT_Assassins"};
float CDispatchManager::ms_fUpdateTimeStep = 0.f;
u32 CDispatchManager::m_netPopulationReservationId = CNetworkObjectPopulationMgr::UNUSED_RESERVATION_ID;
u32 CDispatchManager::m_numNetPedsReserved = 0;
u32 CDispatchManager::m_numNetVehiclesReserved = 0;
u32 CDispatchManager::m_networkReservationCounter = 0;

bool CDispatchManager::ms_SpawnedResourcesThisFrame = false;;

#define DISPATCH_UPDATE_IDLE_TIME .5f

#if __BANK
s32			 CDispatchManager::m_DebugWantedLevel = 1;
bool			 CDispatchManager::m_RenderResourceDebug = false;
#endif 

CDispatchManager::CDispatchManager()
{
	for( s32 i = 0; i < DT_Max; i++ )
	{
		m_apDispatch[i] = NULL;
	}
}

CDispatchManager::~CDispatchManager()
{
}

void CDispatchManager::InitSession()
{
	AddDispatchService(rage_new CPoliceAutomobileDispatch());
	AddDispatchService(rage_new CPoliceHelicopterDispatch());
	AddDispatchService(rage_new CFireDepartmentDispatch());
	AddDispatchService(rage_new CSwatAutomobileDispatch());
	AddDispatchService(rage_new CAmbulanceDepartmentDispatch());
	AddDispatchService(rage_new CPoliceRidersDispatch());
	AddDispatchService(rage_new CPoliceVehicleRequest());
	AddDispatchService(rage_new CPoliceRoadBlockDispatch());
	AddDispatchService(rage_new CGangDispatch());
	AddDispatchService(rage_new CSwatHelicopterDispatch());
	AddDispatchService(rage_new CPoliceBoatDispatch());
	AddDispatchService(rage_new CArmyVehicleDispatch());
	AddDispatchService(rage_new CBikerBackupDispatch());
	AddDispatchService(rage_new CAssassinsDispatch());

#if __BANK
	m_IncidentTime = 30.0f;
	m_bDispatchToPlayerEntity = false;
	InitWidgets();
#endif // __BANK
}

void CDispatchManager::ShutdownSession()
{
	for( s32 i = 0; i < DT_Max; i++ )
	{
		if( m_apDispatch[i] )
		{
			delete m_apDispatch[i];
			m_apDispatch[i] = NULL;
		}
	}
}

void CDispatchManager::GetNetworkReservations(u32& reservedPeds, u32& reservedVehicles, u32& reservedObjects, u32& createdPeds, u32& createdVehicles, u32& createdObjects)
{
	if (m_netPopulationReservationId != CNetworkObjectPopulationMgr::UNUSED_RESERVATION_ID)
	{
		CNetworkObjectPopulationMgr::GetNumReservedObjects(m_netPopulationReservationId, reservedPeds, reservedVehicles, reservedObjects);
		CNetworkObjectPopulationMgr::GetNumCreatedObjects(m_netPopulationReservationId, createdPeds, createdVehicles, createdObjects);
	}
	else
	{
		reservedPeds = reservedVehicles = reservedObjects = 0;
		createdPeds = createdVehicles = createdObjects = 0;
	}
}

#if ENABLE_NETWORK_LOGGING
void CDispatchManager::LogNetworkReservations(netLoggingInterface& log)
{
	u32 reservedPeds;
	u32 reservedVehicles;
	u32 reservedObjects;
	u32 createdPeds;
	u32 createdVehicles;
	u32 createdObjects;

	GetNetworkReservations(reservedPeds, reservedVehicles, reservedObjects, createdPeds, createdVehicles, createdObjects);

	NetworkLogUtils::WriteLogEvent(log, "DISPATCH_RESERVATIONS", "");
	log.WriteDataValue("Peds", "%d/%d", createdPeds, reservedPeds);
	log.WriteDataValue("Vehicles", "%d/%d", createdVehicles, reservedVehicles);
	log.WriteDataValue("Objects", "%d/%d", createdObjects, reservedObjects);
}
#else
void CDispatchManager::LogNetworkReservations(netLoggingInterface&)
{

}
#endif // ENABLE_NETWORK_LOGGING

void CDispatchManager::AddDispatchService( CDispatchService* pDispatch )
{
	aiAssert(pDispatch);
	aiAssert(pDispatch->GetDispatchType() >= 0 && pDispatch->GetDispatchType() < DT_Max);
	aiAssert(m_apDispatch[pDispatch->GetDispatchType()] == NULL);
	m_apDispatch[pDispatch->GetDispatchType()] = pDispatch;
}

void CDispatchManager::Update()
{
	ms_SpawnedResourcesThisFrame = false;

	ms_fUpdateTimeStep += fwTimer::GetTimeStep();
	if(CIncidentManager::GetInstance().GetIncidentCount() == 0 && ms_fUpdateTimeStep < DISPATCH_UPDATE_IDLE_TIME)
	{
		// No incidents, do nothing until timestep is greater than idle time
		return;
	}

	//Update the spawn point helpers.
	CDispatchService::GetSpawnHelpers().Update(ms_fUpdateTimeStep);
	ms_fUpdateTimeStep = 0; // Reset to zero after we updated.
	
	//If the pop zone changes, we need to release the streaming requests so the dispatch vehicle set gets updated.
	int iPopZoneVehicleResponseType = CPopCycle::GetCurrentZoneVehicleResponseType();
	if(iPopZoneVehicleResponseType != m_iCurrZoneVehicleResponseType) // Zone Changed
	{
		m_bShouldReleaseStreamRequests = true; // Release Requests
		m_iCurrZoneVehicleResponseType = iPopZoneVehicleResponseType; // Set Response vehicle type for new zone
	}
	
	//If the wanted level changes, we need to release the streaming requests so the dispatch vehicle set gets updated.
	CWanted* pWanted = CGameWorld::FindLocalPlayerWanted();
	eWantedLevel nPlayerWantedLevel = pWanted ? pWanted->GetWantedLevel() : WANTED_CLEAN;

	if(nPlayerWantedLevel != m_nPlayerWantedLevel) // Wanted level changed
	{
		m_bShouldReleaseStreamRequests = true; // Reset Streaming Requests
		m_nPlayerWantedLevel = nPlayerWantedLevel; // Set new wanted level
	}

	bool bHasReservedNetEntities = false;
	
	if (NetworkInterface::IsGameInProgress())
	{
		bHasReservedNetEntities = m_netPopulationReservationId != CNetworkObjectPopulationMgr::UNUSED_RESERVATION_ID;

		// inform the network population code that any entities created by the dispatch services will be linked to the dispatch manager reservation 
		if (bHasReservedNetEntities)
		{
			CNetworkObjectPopulationMgr::SetCurrentExternalReservationIndex(m_netPopulationReservationId);
		}
	}
	else
	{
		m_netPopulationReservationId = CNetworkObjectPopulationMgr::UNUSED_RESERVATION_ID;
	}

	if (NetworkInterface::IsGameInProgress())
	{
		UpdateNetworkReservations();
	}

	for( s32 i = 0; i < DT_Max; i++ )
	{
		if( m_apDispatch[i] )
		{
			m_apDispatch[i]->Update();
		}
	}

	if (bHasReservedNetEntities)
	{
		CNetworkObjectPopulationMgr::ClearCurrentReservationIndex();
	}

	m_bShouldReleaseStreamRequests = false;
}

void CDispatchManager::UpdateNetworkReservations()
{
	static unsigned const NETWORK_RESERVATION_UPDATE_TIME = 1000; // update once a sec

	if (m_networkReservationCounter == 0)
	{
		m_numNetPedsReserved = 0;
		m_numNetVehiclesReserved = 0;

		float numRemotePedsReserved = 0.0f;
		float numRemoteVehiclesReserved = 0.0f;

		float pedScopeDist = CNetObjPed::GetStaticScopeData().m_scopeDistance * 1.2f;
		float pedScopeDistSqr = pedScopeDist*pedScopeDist;
		float pedScopeMaxDist = pedScopeDist*2.0f;
		float pedScopeMaxDistSqr = pedScopeMaxDist * pedScopeMaxDist;

		float vehicleScopeDist = CNetObjVehicle::GetStaticScopeData().m_scopeDistance * 1.2f;
		float vehicleScopeDistSqr = vehicleScopeDist * vehicleScopeDist;
		float vehicleScopeMaxDist = vehicleScopeDist*2.0f;
		float vehicleScopeMaxDistSqr = vehicleScopeMaxDist * vehicleScopeMaxDist;

		float heliScopeDist = CNetObjHeli::GetStaticScopeData().m_scopeDistance * 1.2f;
		float heliScopeDistSqr = heliScopeDist * heliScopeDist;
		float heliScopeMaxDist = vehicleScopeDist*2.0f;
		float heliScopeMaxDistSqr = vehicleScopeMaxDist * vehicleScopeMaxDist;

		s32 iSize = CIncidentManager::GetInstance().GetMaxCount();
		for( s32 i = 0; i < iSize; i++ )
		{
			CIncident* pIncident = CIncidentManager::GetInstance().GetIncident(i);

			if( pIncident)
			{
				for( s32 i = 0; i < DT_Max; i++ )
				{
					if (m_apDispatch[i] && pIncident->GetRequestedResources(i) > 0 && m_apDispatch[i]->ShouldAllocateNewResourcesForIncident(*pIncident))
					{
						u32 numPeds     = 0;
						u32 numVehicles = 0;
						u32 numObjects = 0;

						if( m_apDispatch[i]->IsActive() || pIncident->GetIsIncidentValidIfDispatchDisabled() )
						{
							m_apDispatch[i]->GetNumNetResourcesToReservePerIncident(numPeds, numVehicles, numObjects);
						}

						if (numPeds > 0 || numVehicles > 0)
						{
							// take the scope range of the entities into account
							CEntity* pIncidentEntity = pIncident->GetEntity();

							CNetObjProximityMigrateable* pNetObj = pIncidentEntity ? SafeCast(CNetObjProximityMigrateable, NetworkUtils::GetNetworkObjectFromEntity(pIncidentEntity)) : NULL;

							if (pNetObj)
							{
								if (!pNetObj->IsClone())
								{
									m_numNetVehiclesReserved += numVehicles;
									m_numNetPedsReserved += numPeds;
								}
								else
								{
									// scale the reservations so that we gradually increase them as we approach the incident position. This gives the population
									// time to adjust
									Vector3 diff = VEC3V_TO_VECTOR3(pIncidentEntity->GetTransform().GetPosition()) - NetworkInterface::GetLocalPlayerFocusPosition();
									float dist = diff.Mag2();

									if (numVehicles > 0)
									{
										float scopeDist, scopeDistSqr, scopeMaxDist, scopeMaxDistSqr;

										if (i == DT_SwatHelicopter)
										{
											scopeDist = heliScopeDist;
											scopeDistSqr = heliScopeDistSqr;
											scopeMaxDist = heliScopeMaxDist;
											scopeMaxDistSqr = heliScopeMaxDistSqr;
										}
										else
										{
											scopeDist = vehicleScopeDist;
											scopeDistSqr = vehicleScopeDistSqr;
											scopeMaxDist = vehicleScopeMaxDist;
											scopeMaxDistSqr = vehicleScopeMaxDistSqr;
										}

										if (dist < scopeMaxDistSqr)
										{
											float scopeScale = 1.0f;

											if (dist > scopeDistSqr)
											{
												scopeScale = 1.0f - (dist - scopeDistSqr) / (scopeMaxDistSqr - scopeDistSqr);
											}

											numRemoteVehiclesReserved += (float)numVehicles * scopeScale;
										}
									}

									if (numPeds > 0 && dist < pedScopeMaxDistSqr) 
									{
										float scopeScale = 1.0f;

										if (dist > pedScopeDistSqr)
										{
											scopeScale = 1.0f - (dist - pedScopeDistSqr) / (pedScopeMaxDistSqr - pedScopeDistSqr);
										}

										numRemotePedsReserved += (float)numPeds * scopeScale;
									}
								}
							}
						}
					}
				}
			}
		}

		m_numNetPedsReserved += (u32)ceilf(numRemotePedsReserved);
		m_numNetVehiclesReserved += (u32)ceilf(numRemoteVehiclesReserved);

		if (m_numNetPedsReserved != 0 || m_numNetVehiclesReserved != 0)
		{
			// cap the reservations
			m_numNetPedsReserved		= Min(CNetworkObjectPopulationMgr::MAX_EXTERNAL_RESERVED_PEDS, m_numNetPedsReserved);
			m_numNetVehiclesReserved	= Min(CNetworkObjectPopulationMgr::MAX_EXTERNAL_RESERVED_VEHICLES, m_numNetVehiclesReserved);

			// inform the network population code to reserve and make space for any entities the dispatch services failed to create due to too much ambient population 
			if (m_netPopulationReservationId != CNetworkObjectPopulationMgr::UNUSED_RESERVATION_ID)
			{
				CNetworkObjectPopulationMgr::UpdateExternalReservation(m_netPopulationReservationId, m_numNetPedsReserved, m_numNetVehiclesReserved, 0);
			}
			else
			{	
				m_netPopulationReservationId = CNetworkObjectPopulationMgr::CreateExternalReservation(m_numNetPedsReserved, m_numNetVehiclesReserved, 0, true);
			}
		}
		else if (m_netPopulationReservationId != CNetworkObjectPopulationMgr::UNUSED_RESERVATION_ID)
		{
			if (CNetworkObjectPopulationMgr::IsExternalReservationInUse(m_netPopulationReservationId))
			{
				CNetworkObjectPopulationMgr::ReleaseExternalReservation(m_netPopulationReservationId);
			}

			m_netPopulationReservationId = CNetworkObjectPopulationMgr::UNUSED_RESERVATION_ID;
		}
	}

	m_networkReservationCounter += fwTimer::GetTimeStepInMilliseconds();

	if (m_networkReservationCounter >= NETWORK_RESERVATION_UPDATE_TIME)
	{
		m_networkReservationCounter = 0;
	}
}

void CDispatchManager::Flush()
{
	for( s32 i = 0; i < DT_Max; i++ )
	{
		if( m_apDispatch[i] )
		{
			m_apDispatch[i]->Flush();
		}
	}
}

void CDispatchManager::EnableDispatch( const DispatchType dispatchType, const bool bEnable )
{
	aiAssert(dispatchType >= 0 && dispatchType < DT_Max);
	if( m_apDispatch[dispatchType] )
	{
		m_apDispatch[dispatchType]->SetActive(bEnable);
	}
}


void CDispatchManager::BlockDispatchResourceCreation( const DispatchType dispatchType, const bool bBlock )
{
	aiAssert(dispatchType >= 0 && dispatchType < DT_Max);
	if( m_apDispatch[dispatchType] )
	{
		m_apDispatch[dispatchType]->BlockResourceCreation(bBlock);
	}
}


void CDispatchManager::EnableAllDispatch( const bool bEnable )
{
	for( s32 i = 0; i < DT_Max; i++ )
	{
		EnableDispatch(static_cast<DispatchType>(i), bEnable);
	}
}


void CDispatchManager::BlockAllDispatchResourceCreation( const bool bBlock )
{
	for( s32 i = 0; i < DT_Max; i++ )
	{
		BlockDispatchResourceCreation(static_cast<DispatchType>(i), bBlock);
	}
}


bool CDispatchManager::IsDispatchEnabled(const DispatchType dispatchType) const
{
	bool bEnabled = false;

	if( m_apDispatch[dispatchType] )
	{
		bEnabled = m_apDispatch[dispatchType]->IsActive();
	}

	return bEnabled;
}


bool CDispatchManager::IsDispatchResourceCreationBlocked( const DispatchType dispatchType ) const
{
	bool bBlocked = false;

	if( m_apDispatch[dispatchType] )
	{
		bBlocked = m_apDispatch[dispatchType]->IsResourceCreationBlocked();
	}

	return bBlocked;
}

//////////////////////////////////////////////////////////////////////////
// PURPOSE: Go through the dispatch systems and delay spawning of any law
// RETURNS: The max law spawn delay for the found delay type
//////////////////////////////////////////////////////////////////////////
float CDispatchManager::DelayLawSpawn()
{
	int iLawResponseDelayType = LAW_RESPONSE_DELAY_MEDIUM;

	CPed* pLocalPlayerPed = CGameWorld::FindLocalPlayer();
	if(pLocalPlayerPed)
	{
		Vector3 vPlayerPosition = VEC3V_TO_VECTOR3(pLocalPlayerPed->GetTransform().GetPosition());
		CPopZone* pZone = CPopZones::FindSmallestForPosition(&vPlayerPosition, ZONECAT_ANY, NetworkInterface::IsGameInProgress() ? ZONETYPE_MP : ZONETYPE_SP);
		if(pZone)
		{
			iLawResponseDelayType = pZone->m_lawResponseDelayType;
		}
	}

	bool bHasOverride = (m_fLawResponseDelayOverride >= 0.0f);
	for( s32 i = 0; i < DT_Max; i++ )
	{
		if(m_apDispatch[i])
		{
			bool bIsLawDispatch = m_apDispatch[i]->IsLawType();
			if(bIsLawDispatch)
			{
				m_apDispatch[i]->SetSpawnTimer(bHasOverride ? m_fLawResponseDelayOverride : CDispatchData::GetRandomLawSpawnDelay(iLawResponseDelayType));
			}
		}
	}

	return bHasOverride ? m_fLawResponseDelayOverride : CDispatchData::GetMaxLawSpawnDelay(iLawResponseDelayType);
}


#if __BANK

void StartVehicleChaseTest()
{
	//Remove the ambient vehicles.
	CVehiclePopulation::RemoveAllVehsHard();

	//Limit the number of vehicles.
	CVehiclePopulation::ms_maxNumberOfCarsInUse = 10;
	
	//Grab the player.
	CPed* pPed = CGameWorld::FindLocalPlayer();
	if(!aiVerify(pPed))
	{
		return;
	}
	
	//Create a vehicle.
	CVehicleFactory::CreateBank();
	CVehicleFactory::CreateCar();
	
	//Ensure the vehicle was created.
	CVehicle* pVehicle = CVehicleFactory::ms_pLastCreatedVehicle;
	if(!aiVerify(pVehicle))
	{
		return;
	}
	
	//Make the car invincible.
	pVehicle->m_nPhysicalFlags.bNotDamagedByBullets = true;
	pVehicle->m_nPhysicalFlags.bNotDamagedByFlames = true;
	pVehicle->m_nPhysicalFlags.bNotDamagedByCollisions = true;
	pVehicle->m_nPhysicalFlags.bNotDamagedByMelee = true;
	pVehicle->m_nPhysicalFlags.bNotDamagedByAnything = true;
	
	//Flush the player tasks.
	pPed->GetPedIntelligence()->FlushImmediately(true);
	
	//Put the player in the vehicle.
	pPed->SetPedInVehicle(pVehicle, 0, CPed::PVF_Warp);
	
	//Switch the engine on.
	pVehicle->SwitchEngineOn(true);
	
	//Calculate the start/end positions.
	Vector3 vStart(-796.2f, -66.8f, 37.4f);
	Vector3 vEnd(-1291.3f, -1188.8f, 4.4f);
	
	//Calculate the driving flags.
	s32 iDrivingFlags = DMode_PloughThrough;
	
	//Calculate the cruise speed.
	float fCruiseSpeed = 50.0f;
	
	//Teleport the vehicle to the start location.
	pVehicle->Teleport(vStart);
	
	//Create the vehicle task.
	aiTask* pVehicleTask = CVehicleIntelligence::GetTaskFromMissionIdentifier(pVehicle, MISSION_CRUISE, NULL, &vEnd, iDrivingFlags, 0.0f, sVehicleMissionParams::DEFAULT_SWITCH_TO_STRAIGHT_LINE_DIST, fCruiseSpeed, false);
	
	//Create the task.
	CTask* pTask = rage_new CTaskControlVehicle(pVehicle, pVehicleTask);
	
	//Give the task to the player.
	CEventGivePedTask event(PED_TASK_PRIORITY_PRIMARY, pTask);
	pPed->GetPedIntelligence()->AddEvent(event);
	
	//Set the wanted level.
	pPed->GetPlayerWanted()->CheatWantedLevel(WANTED_LEVEL3);
}

void StopVehicleChaseTest()
{
	//Reset the number of vehicles.
	CVehiclePopulation::ms_maxNumberOfCarsInUse = CPoolHelpers::GetVehiclePoolSize();
	
	//Grab the player.
	CPed* pPed = CGameWorld::FindLocalPlayer();
	if(!aiVerify(pPed))
	{
		return;
	}
	
	//Flush the player tasks.
	pPed->GetPedIntelligence()->FlushImmediately(true);
	
	//Clear the wanted level.
	pPed->GetPlayerWanted()->CheatWantedLevel(WANTED_CLEAN);
}

void CDispatchManager::InitWidgets()
{
	bkBank *bank = CGameLogic::GetGameLogicBank();
	bank->PushGroup("Dispatch Manager");
		bank->AddToggle("Render Incident Debug", &CIncidentManager::s_bRenderDebug);
		bank->AddToggle("Render Resource Debug", &m_RenderResourceDebug);
		bank->AddToggle("Render Spawn Properties", &CDispatchSpawnProperties::sm_bRenderDebug);
		bank->AddButton("Add Wanted incident", AddWantedIncident);
		bank->AddSlider("Debug Wanted Level", &m_DebugWantedLevel, 0, 6, 1);
		bank->AddButton("Add Script incident", AddScriptIncident);
		bank->AddSlider("Script incident time", &m_IncidentTime, 0.0f, 100.0f, 0.01f );
		bank->AddButton("Add Injury incident", AddInjuryIncident);
		bank->AddToggle("Dispatch To Player Entity", &m_bDispatchToPlayerEntity);
		
		for( s32 i = 0; i < DT_Max; i++ )
		{
			if( m_apDispatch[i] )
			{
				m_apDispatch[i]->AddWidgets(bank);
			}
		}
		
		bank->PushGroup("Vehicle Chase Test");
		
			bank->AddButton("Start Test", StartVehicleChaseTest);
			bank->AddButton("Stop Test", StopVehicleChaseTest);
		
		bank->PopGroup();

	bank->PopGroup();
}


#include "game/Dispatch/incidents.h"
#include "scene/world/GameWorld.h"
#include "Game/Dispatch/OrderManager.h"
#include "Game/Dispatch/Orders.h"
void CDispatchManager::AddFireIncident()
{
	CEntity* pSourceEntity = CGameWorld::FindLocalPlayer();
	if( pSourceEntity )
	{
		Vector3 vLocation = VEC3V_TO_VECTOR3(pSourceEntity->GetTransform().GetPosition());
		Vector3 fireNormal(0.0f, 0.0f, 1.0f);
		g_fireMan.StartMapFire(RCC_VEC3V(vLocation), RCC_VEC3V(fireNormal), 0, NULL, 20.0f, 1.0f, 2.0f, 0.0f, 0.0f, 1, Vec3V(V_ZERO), BANK_ONLY(Vec3V(V_ZERO),) false);
	}
}

void CDispatchManager::AddWantedIncident()
{
	CPed * pPlayerPed = CGameWorld::FindLocalPlayer();
	if( pPlayerPed )
	{
		CWanted * pWanted = pPlayerPed->GetPlayerWanted();
		pWanted->CheatWantedLevel(m_DebugWantedLevel);
		Vector3 vLocation = VEC3V_TO_VECTOR3(pPlayerPed->GetTransform().GetPosition());
		CWantedIncident wantedIncident(pPlayerPed, vLocation);
		CIncidentManager::GetInstance().AddIncident(wantedIncident, true);
	}
}

dev_float SPHERE_RADIUS = 0.3f;

void CDispatchManager::DebugRender()
{
	for( s32 i = 0; i < DT_Max; i++ )
	{
		if( m_apDispatch[i] && m_apDispatch[i]->m_bRenderDebug)
		{
#if __BANK
			m_apDispatch[i]->Debug();
#endif 

			TUNE_GROUP_BOOL(DISPATCH, bRenderIncidentDebug, true);

			if (bRenderIncidentDebug)
			{
				for( s32 iIncident = 0; iIncident < CIncidentManager::GetInstance().GetMaxCount() ; iIncident++ )
				{
					if( CIncidentManager::GetInstance().GetIncident(iIncident) && CIncidentManager::GetInstance().GetIncident(iIncident)->GetRequestedResources(m_apDispatch[i]->GetDispatchType()) > 0 )
					{
						CIncidentManager::GetInstance().GetIncident(iIncident)->DebugRender(m_apDispatch[i]->GetColour());
					}
				}

				for( s32 iOrder = 0; iOrder < COrderManager::GetInstance().GetMaxCount(); iOrder++ )
				{
					if( COrderManager::GetInstance().GetOrder(iOrder) )
					{
						if( COrderManager::GetInstance().GetOrder(iOrder)->GetDispatchType() == m_apDispatch[i]->GetDispatchType() )
						{
							if( COrderManager::GetInstance().GetOrder(iOrder)->GetEntity() && COrderManager::GetInstance().GetOrder(iOrder)->GetIncident())
							{
								const Vector3 vPosition = VEC3V_TO_VECTOR3(COrderManager::GetInstance().GetOrder(iOrder)->GetEntity()->GetTransform().GetPosition());
								grcDebugDraw::Sphere(vPosition, SPHERE_RADIUS, m_apDispatch[i]->GetColour());
								grcDebugDraw::Line(vPosition, COrderManager::GetInstance().GetOrder(iOrder)->GetIncident()->GetLocation(), m_apDispatch[i]->GetColour());
								
								if ( m_apDispatch[i]->GetDispatchType() == DT_FireDepartment)
								{
									if ( COrderManager::GetInstance().GetOrder(iOrder)->GetEntity()->GetIsTypePed() )
									{
										CIncident* pIncident = COrderManager::GetInstance().GetOrder(iOrder)->GetIncident();
										if ( pIncident->GetType() == CIncident::IT_Fire)
										{
											CFireIncident* pFireIncident = static_cast<CFireIncident*>(pIncident);
											for( int i = 0 ; i < pFireIncident->GetNumFires(); i++)
											{
												CPed* pPed = pFireIncident->GetPedAttendingFire(i);
												if(pPed)
												{
													CFire* pFire = pFireIncident->GetFire(i);
													grcDebugDraw::Line(VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()), VEC3V_TO_VECTOR3(pFire->GetPositionWorld()), Color_purple);
												}
											}
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}

#if __BANK
	//Render the spawn properties.
	CDispatchSpawnProperties::GetInstance().RenderDebug();

	//Render the spawn helpers.
	CDispatchService::GetSpawnHelpers().RenderDebug();

	//Render the helper volumes.
	CDispatchHelperVolumes::RenderDebug();
#endif
}

void CDispatchManager::RenderResourceUsageForIncident(s32& iNoTexts, Color32 debugColor, CIncident* pIncident)
{
	if (m_RenderResourceDebug)
	{
		bool foundIncident = false;
		for( s32 iIncident = 0; iIncident < CIncidentManager::GetInstance().GetMaxCount() ; iIncident++ )
		{
			s32 iNoResourcesRequested = 0;
			s32 iNoResourcesAllocated = 0;
			s32 iNoResourcesAssigned = 0;
			s32 iNoResourcesArrived = 0;
			s32 iNoResourcesFinished = 0;
			for( s32 i = 0; i < DT_Max; i++ )
			{
				if( m_apDispatch[i] )
				{
					if( CIncidentManager::GetInstance().GetIncident(iIncident) == pIncident )
					{
						iNoResourcesRequested += CIncidentManager::GetInstance().GetIncident(iIncident)->GetRequestedResources(m_apDispatch[i]->GetDispatchType());
						iNoResourcesAllocated += CIncidentManager::GetInstance().GetIncident(iIncident)->GetAllocatedResources(m_apDispatch[i]->GetDispatchType());
						iNoResourcesAssigned += CIncidentManager::GetInstance().GetIncident(iIncident)->GetAssignedResources(m_apDispatch[i]->GetDispatchType());
						iNoResourcesArrived += CIncidentManager::GetInstance().GetIncident(iIncident)->GetArrivedResources(m_apDispatch[i]->GetDispatchType());
						iNoResourcesFinished += CIncidentManager::GetInstance().GetIncident(iIncident)->GetFinishedResources(m_apDispatch[i]->GetDispatchType());
						foundIncident = true;
					}
				}
			}

			if (foundIncident)
			{
				char szText[128];
				formatf(szText, "Requested Resources : %i", iNoResourcesRequested);
				grcDebugDraw::Text(CIncidentManager::GetTextPosition(iNoTexts,1), debugColor, szText);
				formatf(szText, "Allocated Resources : %i", iNoResourcesAllocated);
				grcDebugDraw::Text(CIncidentManager::GetTextPosition(iNoTexts,1), debugColor, szText);
				formatf(szText, "Assigned Resources : %i", iNoResourcesAssigned);
				grcDebugDraw::Text(CIncidentManager::GetTextPosition(iNoTexts,1), debugColor, szText);
				formatf(szText, "Arrived Resources : %i", iNoResourcesArrived);
				grcDebugDraw::Text(CIncidentManager::GetTextPosition(iNoTexts,1), debugColor, szText);
				formatf(szText, "Finished Resources : %i", iNoResourcesFinished);
				grcDebugDraw::Text(CIncidentManager::GetTextPosition(iNoTexts,1), debugColor, szText);
				
				return;
			}
		}
	}
}

#if __BANK
bool CDispatchManager::DebugEnabled(const DispatchType dispatchType)
{
	for( s32 i = 0; i < DT_Max; i++ )
	{
		if( GetInstance().m_apDispatch[i] && GetInstance().m_apDispatch[i]->GetDispatchType() == dispatchType)
		{
			return GetInstance().m_apDispatch[i]->m_bRenderDebug;
		}
	}
	return false;
}
#endif

void CDispatchManager::AddScriptIncident()
{
	Vector3 vCoords =  CGameWorld::FindLocalPlayerCoors();
	CEntity* pEntity = NULL;
	if( GetInstance().m_bDispatchToPlayerEntity )
	{
		pEntity = CGameWorld::FindLocalPlayer();
	}
	CScriptIncident scriptIncident(pEntity, vCoords, GetInstance().m_IncidentTime);

	s32 iNumRequested = 0;
	for( s32 i = 0; i < DT_Max; i++ )
	{
		if( GetInstance().m_apDispatch[i] )
		{
			iNumRequested += GetInstance().m_apDispatch[i]->m_iDebugRequestedNumbers;
			scriptIncident.SetRequestedResources(i, GetInstance().m_apDispatch[i]->m_iDebugRequestedNumbers);
			GetInstance().m_apDispatch[i]->m_iDebugRequestedNumbers = 0;
		}
	}

	if (aiVerifyf(iNumRequested > 0, "Setup requested debug numbers in each dispatch!"))
	{
		if (!CIncidentManager::GetInstance().AddIncident(scriptIncident, true))
		{
			aiAssertf(0, "Failed to add a script incident");
		}
	}
}

void CDispatchManager::AddInjuryIncident()
{
	//Use player as 
	Vector3 vCoords = CGameWorld::FindLocalPlayerCoors();

	// create ped at specified position
	CPedVariationDebug::CycleTypeId();
	CPed::CreateBank();
	CPedVariationDebug::CreatePedCB();
	CPed* pPed = CPedFactory::GetLastCreatedPed();
	if (pPed)
	{
		pPed->SetPosition(vCoords, true, true);
		pPed->SetHealth(0.0f);

		if(CInjuryIncident::IsPedValidForInjuryIncident(pPed))
		{
			int incidentIndex = -1;
			CInjuryIncident incident(pPed, vCoords);
			CIncidentManager::GetInstance().AddIncident(incident, true, &incidentIndex);
		}
	}
}

#endif
