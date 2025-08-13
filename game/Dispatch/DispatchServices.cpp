// Title	:	DispatchServices.cpp
// Purpose	:	Defines the different dispatch services
// Started	:	13/05/2010

// File header
#include "Game/Dispatch/DispatchServices.h"

// Rage headers
#include "ai/aichannel.h"
#include "fwmaths/random.h"

// Game headers
#include "audio/scannermanager.h"
#include "camera/CamInterface.h"
#include "control/gamelogic.h"
#include "debug/DebugGlobals.h"
#include "debug/DebugScene.h"
#include "debug/VectorMap.h"
#include "Game/Dispatch/DispatchData.h"
#include "Game/Dispatch/DispatchManager.h"
#include "Game/Dispatch/IncidentManager.h"
#include "Game/Dispatch/Incidents.h"
#include "Game/Dispatch/OrderManager.h"
#include "Game/Dispatch/RoadBlock.h"
#include "Game/ModelIndices.h"
#include "Network/Objects/NetworkObjectPopulationMgr.h"
#include "PedGroup/PedGroup.h"
#include "Peds/ped.h"
#include "Peds/Ped.h"
#include "Peds/PedFactory.h"			// For CPedFactory::CreatePed(), etc.
#include "Peds/pedIntelligence.h"
#include "Peds/pedpopulation.h"
#include "peds/popzones.h"
#include "Scene/World/GameWorld.h"
#include "streaming/streaming.h"		// For HasObjectLoaded(), etc.
#include "task/Combat/TaskNewCombat.h"
#include "task/Movement/Climbing/TaskRappel.h"
#include "task/Movement/TaskNavMesh.h"	// Probably temporary, for giving a task in CPoliceRidersDispatch.
#include "task/Response/TaskGangs.h"	//for CTaskGangPatrol
#include "Vehicles/Automobile.h"
#include "Vehicles/Boat.h"
#include "Vehicles/VehicleFactory.h"
#include "Vehicles/Vehiclepopulation.h"
#include "Vehicles/Heli.h"
#include "Vehicles/Metadata/VehicleEntryPointInfo.h"
#include "weapons/inventory/PedInventoryLoadOut.h"

// Parser headers
#include "Game/Dispatch/DispatchServices_parser.h"

AI_OPTIMISATIONS()

RAGE_DEFINE_CHANNEL(dispatch_services)

#define IMPLEMENT_DISPATCH_SERVICES_TUNABLES(classname, hash)	IMPLEMENT_TUNABLES_PSO(classname, #classname, hash, "Dispatch Tuning", "Game Logic")

bank_u32 CDispatchService::SEARCH_DISTANCE_IN_VEHICLE_LERP_TIME_MS = 60000;
bank_u32 CDispatchService::SEARCH_DISTANCE_ON_FOOT_LERP_TIME_MS = 120000;
bank_float CDispatchService::SEARCH_AREA_MOVE_SPEED = 0.75f;
bank_float CDispatchService::SEARCH_AREA_MOVE_SPEED_HIGH_WANTED_LEVEL = 0.9f;
bank_float CDispatchService::MINIMUM_SEARCH_AREA_RADIUS = 5.0f;
bank_float CDispatchService::SEARCH_AREA_RADIUS = 30.0f;
bank_float CDispatchService::SEARCH_AREA_HEIGHT = 20.0f;
bank_float CDispatchService::DEFAULT_TIME_BETWEEN_SEARCH_POS_UPDATES = 2.0f;
bank_float CDispatchService::MAX_SPEED_FOR_MODIFIER = 40.0f;
bank_float CDispatchService::MIN_EMERGENCY_SERVICES_DISTANCE_FROM_WANTED_PLAYER = 300.0f;

CDispatchSpawnHelpers CDispatchService::ms_SpawnHelpers;

#if __BANK
const char* CDispatchService::ms_aSpawnFailureReasons[DT_Max];
#endif

extern audScannerManager g_AudioScannerManager;

CDispatchService::CDispatchService( DispatchType dispatchType )
: m_iDispatchType(dispatchType)
, m_bDispatchActive(true)
, m_bBlockResourceCreation(false)
, m_bTimerTicking(false)
, m_fSpawnTimer(0.0f)
, m_fTimeBetweenSpawnAttemptsOverride(-1.0f)
, m_fTimeBetweenSpawnAttemptsMultiplierOverride(-1.0f)
, m_CurrVehicleSet(0)
, m_iForcedVehicleSet(-1)
, m_pDispatchVehicleSets(NULL)
{
#if __BANK 
	m_bRenderDebug = false;
	m_iDebugRequestedNumbers = 0;
#endif //__BANK
}

CDispatchService::~CDispatchService()
{
	Flush();
}

u8 CDispatchService::GetNumResourcesPerUnit(DispatchType nDispatchType)
{
	//Check the type.
	switch(nDispatchType)
	{
		case DT_PoliceAutomobile:
		{
			return 2;
		}
		case DT_PoliceHelicopter:
		{
			return 3;
		}
		case DT_FireDepartment:
		{
			return 0;
		}
		case DT_SwatAutomobile:
		{
			return 3;
		}
		case DT_AmbulanceDepartment:
		{
			return 0;
		}
		case DT_PoliceRiders:
		{
			return 0;
		}
		case DT_PoliceVehicleRequest:
		{
			return 0;
		}
		case DT_PoliceRoadBlock:
		{
			return 1;
		}
		case DT_Gangs:
		{
			return 0;
		}
		case DT_SwatHelicopter:
		{
			return 5;
		}
		case DT_PoliceBoat:
		{
			return 3;
		}
		case DT_ArmyVehicle:
		case DT_BikerBackup:
		case DT_Assassins:
		{
			return 0;
		}
		default:
		{
			aiAssertf(false, "Unknown dispatch type: %d", nDispatchType);
			return 0;
		}
	}
}

dev_float MAX_DISTANCE_FOR_SEARCH_TO_TRAIL = 60.0f;
dev_float MAX_DISTANCE_FOR_SEARCH_TO_TRAIL_IN_CAR = 30.0f;

void CDispatchService::UpdateCurrentSearchAreas(float& fTimeBetweenSearchPosUpdate)
{
	CPed * pPlayerPed = CGameWorld::FindLocalPlayer();

	if (pPlayerPed)	// this player exists
	{
		CWanted *pWanted = pPlayerPed->GetPlayerWanted();

		bool bCanUpdateCurrentSearchAreas = (pWanted->GetWantedLevel() > WANTED_CLEAN) &&
			(pWanted->GetTimeUntilHiddenEvasionStarts() <= 0.0f);
		if(bCanUpdateCurrentSearchAreas)
		{
			// The speed increases slightly with a higher wanted level
			float fWantedLevel = Clamp((float)pWanted->GetWantedLevel(), 0.0f, 6.0f);
			fWantedLevel *= 0.16666f;
			float fMoveSpeed = SEARCH_AREA_MOVE_SPEED + ((SEARCH_AREA_MOVE_SPEED_HIGH_WANTED_LEVEL - SEARCH_AREA_MOVE_SPEED)*fWantedLevel);

			if( pPlayerPed->GetVehiclePedInside() )
				fMoveSpeed = SEARCH_AREA_MOVE_SPEED_HIGH_WANTED_LEVEL;

			const Vector3 vPlayerPosition = VEC3V_TO_VECTOR3(CDispatchHelperPedPositionToUse::Get(*pPlayerPed));
			// The search area is never more than 30m behind the player, so increase the speed depending on how far outside 30m it is
			float fDistFromSearchCenterToPlayer = pWanted->m_CurrentSearchAreaCentre.Dist(vPlayerPosition);
			float fSpeed = fMoveSpeed * fwTimer::GetTimeStep();

			float fMaxDistTrailingSearchArea = pPlayerPed->GetVehiclePedInside() ? MAX_DISTANCE_FOR_SEARCH_TO_TRAIL_IN_CAR : MAX_DISTANCE_FOR_SEARCH_TO_TRAIL;
			if( fDistFromSearchCenterToPlayer > fMaxDistTrailingSearchArea )
			{
				fSpeed += fDistFromSearchCenterToPlayer - fMaxDistTrailingSearchArea;
			}

			// Move the searchsphere towards the player at a constant rate
			if( fDistFromSearchCenterToPlayer < fSpeed )
			{
				pWanted->m_CurrentSearchAreaCentre = vPlayerPosition;
			}
			else
			{
				Vector3 vDiff = vPlayerPosition - pWanted->m_CurrentSearchAreaCentre;
				vDiff.Normalize();
				vDiff.Scale(fSpeed);
				pWanted->m_CurrentSearchAreaCentre += vDiff;
			}

			Vector3 vSearchPosition = pWanted->m_CurrentSearchAreaCentre;

			fTimeBetweenSearchPosUpdate -= fwTimer::GetTimeStep();

			if (fTimeBetweenSearchPosUpdate < 0.0f)
			{
				fTimeBetweenSearchPosUpdate = DEFAULT_TIME_BETWEEN_SEARCH_POS_UPDATES;

				// Is the player is in a vehicle moving at speed, favour locations in a cone in front of the player
				if (pPlayerPed->GetIsInVehicle())
				{
					static bank_float SEARCH_OUTSIDE_CONE_CHANCE = 30.0f;
					if (fwRandom::GetRandomNumberInRange(0.0f, 100.0f) > SEARCH_OUTSIDE_CONE_CHANCE)
					{
						TUNE_GROUP_FLOAT(DISPATCH, MIN_SEARCH_DISTANCE_LOW, 15.0f, 0.0f, 100.0f, 0.05f);
						TUNE_GROUP_FLOAT(DISPATCH, MIN_SEARCH_DISTANCE_HIGH, 50.0f, 0.0f, 150.0f, 0.05f);
						TUNE_GROUP_FLOAT(DISPATCH, MAX_SEARCH_DISTANCE, 450.0f, 0.0f, 600.0f, 0.1f);
						static bank_float SEARCH_CONE_ANGLE = EIGHTH_PI;
						
						u32 iTimeSincePlayerLastSeen = pWanted->GetTimeSinceSearchLastRefocused();
						float fLerpAmount = 1.0f - MIN((float)iTimeSincePlayerLastSeen / (float)SEARCH_DISTANCE_IN_VEHICLE_LERP_TIME_MS, 1.0f);
						float fMinSearchDistance = Lerp(fLerpAmount, MIN_SEARCH_DISTANCE_LOW, MIN_SEARCH_DISTANCE_HIGH);
						float fMaxSearchDistance = Lerp(fLerpAmount, MIN_SEARCH_DISTANCE_HIGH, MAX_SEARCH_DISTANCE);

						// Scale the search radius by the speed, the slower we're going, the closer the radius
						const float fSearchRadius = MAX(fMinSearchDistance, fMaxSearchDistance * ComputeSpeedModifier(*pPlayerPed->GetMyVehicle()));

						float fRandomDistance = fwRandom::GetRandomNumberInRange(fSearchRadius, fMaxSearchDistance);
						float fRandomHeight = fwRandom::GetRandomNumberInRange(-SEARCH_AREA_HEIGHT, SEARCH_AREA_HEIGHT);
						float fRandomAngle = fwRandom::GetRandomNumberInRange(-SEARCH_CONE_ANGLE, SEARCH_CONE_ANGLE);

						Vector3 vRotate(0.0f, fRandomDistance, fRandomHeight);
						vRotate.RotateZ(pPlayerPed->GetCurrentHeading());
						vRotate.RotateZ(fRandomAngle);
						vSearchPosition += vRotate;
#if __BANK
						Vector3 vOriginal(0.0f, fRandomDistance, fRandomHeight);
						vOriginal.RotateZ(pPlayerPed->GetCurrentHeading());

						Vector3 vLeft = vOriginal;
						vLeft.RotateZ(-SEARCH_CONE_ANGLE);
						vLeft += pWanted->m_CurrentSearchAreaCentre;

						Vector3 vRight = vOriginal;
						vRight.RotateZ(SEARCH_CONE_ANGLE);
						vRight += pWanted->m_CurrentSearchAreaCentre;

						static u32 DEBUG_TIME = 1000;
						CPoliceAutomobileDispatch::ms_debugDraw.AddVectorMapLine(RCC_VEC3V(pWanted->m_CurrentSearchAreaCentre), RCC_VEC3V(vLeft), Color_yellow, DEBUG_TIME);
						CPoliceAutomobileDispatch::ms_debugDraw.AddVectorMapLine(RCC_VEC3V(pWanted->m_CurrentSearchAreaCentre), RCC_VEC3V(vRight), Color_yellow, DEBUG_TIME);
						CPoliceAutomobileDispatch::ms_debugDraw.AddVectorMapLine(RCC_VEC3V(pWanted->m_CurrentSearchAreaCentre), RCC_VEC3V(vSearchPosition), Color_green, DEBUG_TIME);
#endif
					}
				}
			}	
		}
	}
}

bool CDispatchService::IsLocationWithinSearchArea(const Vector3& vLocation)
{
	//Ensure the local player is valid.
	const CPed* pLocalPlayer = CGameWorld::FindLocalPlayer();
	if(!aiVerifyf(pLocalPlayer, "The local player is invalid."))
	{
		return false;
	}
	
	//Ensure the wanted is valid.
	const CWanted* pWanted = pLocalPlayer->GetPlayerWanted();
	if(!aiVerifyf(pWanted, "The wanted is invalid."))
	{
		return false;
	}
	
	//Calculate the flat distance.
	Vector2 vLocationFlat;
	vLocation.GetVector2XY(vLocationFlat);
	Vector2 vCenterFlat;
	pWanted->m_CurrentSearchAreaCentre.GetVector2XY(vCenterFlat);
	float fDistSq = vCenterFlat.Dist2(vLocationFlat);
	
	//Ensure the distance is within the threshold.
	float fMaxDistSq = square(150.0f);
	if(fDistSq > fMaxDistSq)
	{
		return false;
	}
	
	return true;
}

float CDispatchService::ComputeSpeedModifier(const CVehicle& vehicle)
{
	const float fSpeedMagSqr = vehicle.GetVelocity().Mag2();

	return rage::Clamp(fSpeedMagSqr / (MAX_SPEED_FOR_MODIFIER * MAX_SPEED_FOR_MODIFIER), 0.0f, 1.0f);
}

// Static callback, grabs the correct dispatch and calls it's callback function
void CDispatchService::DispatchDelayedSpawnStaticCallback(CEntity* pEntity, CIncident* pIncident, DispatchType iDispatchType, s32 pedGroupId, COrder* pOrder)
{
	CDispatchManager::GetInstance().GetDispatch(iDispatchType)->DispatchDelayedSpawnCallback(pEntity, pIncident, pedGroupId, pOrder);
}

void CDispatchService::Update()
{
	// Updates an internal list of incidents, sorted by priority
	UpdateSortedIncidentList();

	// Adjust the resource requests
	AdjustResourceRequests();

	// Calculates and sets the number of resources to be allocated to each incident
	AllocateResourcesToIncidents();

	// Preserve still valid orders in the order list to give some continuity
	PreserveStillValidOrders();

	// Adjust the resource allocations
	AdjustResourceAllocations();

	// Updates any streaming requests, releases streaming requests or adds new requests
	UpdateModelStreamingRequests();
	
	//Manage the resources for the incidents.
	ManageResourcesForIncidents();
}

bool CDispatchService::IsLawType() const
{
	//Check the dispatch type.
	switch(m_iDispatchType)
	{
		case DT_PoliceAutomobile:
		case DT_PoliceHelicopter:
		case DT_SwatAutomobile:
		case DT_PoliceRiders:
		case DT_PoliceRoadBlock:
		case DT_SwatHelicopter:
		case DT_PoliceBoat:
		{
			return true;
		}
		case DT_Invalid:
		case DT_FireDepartment:
		case DT_AmbulanceDepartment:
		case DT_PoliceVehicleRequest:
		case DT_Gangs:
		case DT_ArmyVehicle:
		case DT_BikerBackup:
		case DT_Assassins:
		case DT_Max:
		{
			return false;
		}
		default:
		{
			aiAssertf(false, "Unknown dispatch type: %d", m_iDispatchType);
			return false;
		}
	}
}

void CDispatchService::Flush()
{
	m_ResponseIncidents.Reset();
	m_streamingRequests.ReleaseAll();
}

void CDispatchService::Init()
{
#if __BANK
	for(int i = DT_Invalid + 1; i < DT_Max; ++i)
	{
		ms_aSpawnFailureReasons[i] = NULL;
	}
#endif
}

#if __BANK
const char* CDispatchService::GetLastSpawnFailureReason(DispatchType nDispatchType)
{
	return ms_aSpawnFailureReasons[nDispatchType];
}
#endif

static s32 SORT_DISPATCH_TYPE = 0;
int QSortResponseIncidentsFunc( CIncident* const* pA, CIncident* const* pB)
{
	return (*pB)->GetRequestedResources(SORT_DISPATCH_TYPE) - (*pA)->GetRequestedResources(SORT_DISPATCH_TYPE);
}

void CDispatchService::UpdateSortedIncidentList()
{
	m_ResponseIncidents.Reset();

	s32 iSize = CIncidentManager::GetInstance().GetMaxCount();
	for( s32 i = 0; i < iSize; i++ )
	{
		CIncident* pIncident = CIncidentManager::GetInstance().GetIncident(i);
		if( pIncident )
		{
			//Clear the allocated & assigned resources.
			pIncident->SetAllocatedResources(GetDispatchType(),	0);
			pIncident->SetAssignedResources(GetDispatchType(),	0);

			//Check if the incident is locally controlled.
			if( pIncident->IsLocallyControlled() )
			{
				// Dont respond to any incidents if disabled.
				if( IsActive() || pIncident->GetIsIncidentValidIfDispatchDisabled() )
				{
					m_ResponseIncidents.PushAndGrow(pIncident);
				}
			}
		}
	}

	// Sort the incidents
	SORT_DISPATCH_TYPE = GetDispatchType();
	m_ResponseIncidents.QSort(0, -1, QSortResponseIncidentsFunc);
}


void CDispatchService::AllocateResourcesToIncidents()
{
	s32 iSize = m_ResponseIncidents.GetCount();
	for( s32 i = 0; i < iSize; i++ )
	{
		CIncident* pIncident = m_ResponseIncidents[i];
		if( pIncident )
		{
			pIncident->SetAllocatedResources(GetDispatchType(), pIncident->GetRequestedResources(m_iDispatchType));
		}
	}
}

bool CDispatchService::ShouldAllocateNewResourcesForIncident(const CIncident& rIncident) const
{
	//Ensure new resources should be allocated for the incident.
	if(!rIncident.GetShouldAllocateNewResourcesForIncident(m_iDispatchType))
	{
		return false;
	}

	return true;
}

void CDispatchService::PreserveStillValidOrders()
{
	//Iterate over the orders.
	s32 iSize = COrderManager::GetInstance().GetMaxCount();
	for(s32 i = 0; i < iSize; i++)
	{
		//Ensure the order is valid.
		COrder* pOrder = COrderManager::GetInstance().GetOrder(i);
		if(!pOrder)
		{
			continue;
		}

		//Ensure the dispatch type matches.
		if(pOrder->GetDispatchType() != GetDispatchType())
		{
			continue;
		}

		//Check if the order is valid.
		bool bIsOrderValid = (!pOrder->IsLocallyControlled());
		if(!bIsOrderValid)
		{
			//Check if we are active.
			if(IsActive() && pOrder->GetIncident())
			{
				//Check if the entity is valid.
				CEntity* pEntity = pOrder->GetEntity();
				if(pEntity && GetIsEntityValidForOrder(pEntity))
				{
					bIsOrderValid = true;
				}
			}
		}

		if(bIsOrderValid)
		{
			//Check if the incident is valid.
			CIncident* pIncident = pOrder->GetIncident();
			if(pIncident)
			{
				//Increase the assigned resources.
				pIncident->SetAssignedResources(GetDispatchType(), pIncident->GetAssignedResources(GetDispatchType()) + 1);
			}
		}
		//Check if the order is locally controlled.
		else if(pOrder->IsLocallyControlled())
		{
			//Flag the order for deletion.
			pOrder->SetFlagForDeletion(true);
		}
	}

	//Iterate over the incidents.
	int iNumIncidents = m_ResponseIncidents.GetCount();
	for(int i = 0; i < iNumIncidents; ++i)
	{
		//Ensure the incident is valid.
		CIncident* pIncident = m_ResponseIncidents[i];
		if(!pIncident)
		{
			continue;
		}

		// Increase our assigned resources by the number of scheduled peds
		int iScheduledPedsForIncident = CPedPopulation::CountScheduledPedsForIncident(pIncident, GetDispatchType());
		pIncident->SetAssignedResources(GetDispatchType(), pIncident->GetAssignedResources(GetDispatchType()) + iScheduledPedsForIncident);
	}
}

void CDispatchService::AssignNewOrder(CEntity& rEntity, CIncident& rIncident)
{
	//Add the order.
	COrder order(GetDispatchType(), &rEntity, &rIncident);
	COrderManager::GetInstance().AddOrder(order);
}

void CDispatchService::AssignNewOrders(CIncident& rIncident, const Resources& rResources)
{
	//Assign the resources to the incident.
	int iNumResources = rResources.GetCount();
	for(int i = 0; i < iNumResources; ++i)
	{
		//Ensure the entity is valid.
		CEntity* pEntity = rResources[i];
		if(!aiVerifyf(pEntity, "Entity is invalid."))
		{
			continue;
		}
		
		if (NetworkInterface::IsGameInProgress() && !NetworkUtils::GetNetworkObjectFromEntity(pEntity))
		{
			continue;
		}

		//Assign a new order to the entity.
		aiAssert(!pEntity->GetIsTypePed() || !static_cast<CPed *>(pEntity)->GetPedIntelligence()->GetOrder());
		AssignNewOrder(*pEntity, rIncident);
	}
}

void CDispatchService::SpawnNewResourcesTimed(CIncident& rIncident, Resources& rResources)
{

#if RAGE_TIMEBARS && INCLUDE_DETAIL_TIMEBARS
	char timebarName[32];
	sprintf(timebarName, "Dispatch Type %d", m_iDispatchType);
	pfAutoMarker(timebarName, m_iDispatchType);
#endif

#if !__FINAL
	const bool bPoliceDispatch = IsLawType();
	if( (!bPoliceDispatch && !gbAllowVehGenerationOrRemoval) || (bPoliceDispatch && !gbAllowCopGenerationOrRemoval) )
	{
		BANK_ONLY(SetLastSpawnFailureReason("Generation and removal have been blocked.");)
		return;
	}
#endif

	//Ensure resource creation is not blocked.
	if(IsResourceCreationBlocked())
	{
		BANK_ONLY(SetLastSpawnFailureReason("Resource creation is blocked.");)
		return;
	}
	
	//Ensure some resources are needed.
	int iResourcesNeeded = rResources.GetCapacity() - rResources.GetCount();
	if(iResourcesNeeded <= 0)
	{
		return;
	}
	
	//Ensure the spawn timer has expired.
	m_fSpawnTimer -= fwTimer::GetTimeStep();
	if(m_fSpawnTimer > 0.0f)
	{
		m_bTimerTicking = true;
		return;
	}
	
	//Ensure we can spawn resources.
	if(!CanSpawnResources(rIncident, iResourcesNeeded))
	{
		BANK_ONLY(SetLastSpawnFailureReason("Can't spawn resources.");)
		return;
	}

	//Spawn the resources.
	if(!SpawnNewResources(rIncident, rResources))
	{
		return;
	}
	// if we get here, we've spawned
	CDispatchManager::ms_SpawnedResourcesThisFrame = true;
	//Reset the spawn timer.
	ResetSpawnTimer();
	m_bTimerTicking = false;
}

#if __BANK
void CDispatchService::AddWidgets( bkBank *bank )
{
	bank->PushGroup(GetName());
	bank->AddToggle("Is Active", &m_bDispatchActive, NullCB);
	bank->AddToggle("Stop resource creation", &m_bBlockResourceCreation, NullCB);
	bank->AddToggle("Render debug", &m_bRenderDebug, NullCB);
	bank->AddSlider("Debug request numbers", &m_iDebugRequestedNumbers, 0, 24, 1);
	AddExtraWidgets(bank);
	bank->PopGroup();
}
#endif //__BANK

bool CDispatchService::CanSpawnResources(const CIncident& rIncident, int UNUSED_PARAM(iResourcesNeeded)) const
{
	//Ensure the incident can spawn resources.
	if(rIncident.GetDisableResourceSpawning())
	{
		dispatchServicesDebugf3("CanSpawnResources: Resource spawning is disabled");
		return false;
	}

	// Don't spawn anything if the pools are nearly full
	if( CPed::GetPool()->GetNoOfFreeSpaces() <= MinSpaceInPools ||
		CVehicle::GetPool()->GetNoOfFreeSpaces() <= MinSpaceInPools )
	{
		dispatchServicesDebugf3("CanSpawnResources: pools are full");
		return false;
	}

	if(NetworkInterface::IsGameInProgress())
	{
		u32 numPeds = 0;
		u32 numVehicles = 0;
		u32 numObjects = 0;

		GetNumNetResourcesToReservePerIncident(numPeds, numVehicles, numObjects);

		CNetworkObjectPopulationMgr::eVehicleGenerationContext eOldGenContext = CNetworkObjectPopulationMgr::GetVehicleGenerationContext();
		CNetworkObjectPopulationMgr::SetVehicleGenerationContext(CNetworkObjectPopulationMgr::VGT_Dispatch);

		if(!NetworkInterface::CanRegisterObjects(numPeds, numVehicles, numObjects, 0, 0, false))
		{
			dispatchServicesDebugf3("CanSpawnResources: NetworkInterface::CanRegisterObjects() failed");
			return false;
		}

		CNetworkObjectPopulationMgr::SetVehicleGenerationContext(eOldGenContext);
	}

	// Don't dispatch to an injury or fire incident if the local player has a wanted level, or if any players with wanted levels
	// are close in an MP game.
	if (rIncident.GetType() == CIncident::IT_Injury || rIncident.GetType() == CIncident::IT_Fire)
	{
		const CPed* pPlayer = CGameWorld::FindLocalPlayer();
		if (pPlayer && pPlayer->GetPlayerWanted() && pPlayer->GetPlayerWanted()->GetWantedLevel() > WANTED_CLEAN)
		{
			dispatchServicesDebugf3("CanSpawnResources: local player has wanted level and incident is Injury or Fire");
			return false;
		}

		if (NetworkInterface::IsGameInProgress())
		{
			Vector3 vLocation = rIncident.GetLocation();
			const float fThresholdDistanceSquared = MIN_EMERGENCY_SERVICES_DISTANCE_FROM_WANTED_PLAYER * MIN_EMERGENCY_SERVICES_DISTANCE_FROM_WANTED_PLAYER;
			const CPed* pClosestWantedPlayerPed = NetworkInterface::FindClosestWantedPlayer(vLocation, WANTED_LEVEL1);
			if (pClosestWantedPlayerPed && 
				IsLessThanAll(DistSquared(VECTOR3_TO_VEC3V(vLocation), pClosestWantedPlayerPed->GetTransform().GetPosition()), ScalarV(fThresholdDistanceSquared)))
			{
				BANK_ONLY(dispatchServicesDebugf3("CanSpawnResources: closest wanted player [%s] is closer than %.2f",AILogging::GetDynamicEntityNameSafe(pClosestWantedPlayerPed),fThresholdDistanceSquared);)
				return false;
			}
		}
	}

	// Don't spawn resources during an arrest or death fade
	if(CGameLogic::GetGameState() == CGameLogic::GAMESTATE_ARREST_FADE || CGameLogic::GetGameState() == CGameLogic::GAMESTATE_DEATH_FADE)
	{
		dispatchServicesDebugf3("CanSpawnResources: arrest or death screen fade is active");
		return false;
	}

	return true;
}

bool CDispatchService::GetIsEntityValidForOrder(CEntity* pEntity)
{
	//Ensure the entity is a ped.
	aiAssert(pEntity);
	if(!pEntity->GetIsTypePed())
	{
		return false;
	}

	// If being removed from the world they are not valid for the order
	if(pEntity->IsBaseFlagSet(fwEntity::REMOVE_FROM_WORLD))
	{
		return false;
	}

	const CPed* pPed = static_cast<const CPed *>(pEntity);

	//Check if the ped has a valid order.
	const COrder* pOrder = pPed->GetPedIntelligence()->GetOrder();
	if(pOrder && pOrder->GetIsValid())
	{
		//Ensure the entity is alive (if not exiting a vehicle)
		if(pPed->IsInjured() && !pPed->IsExitingVehicle())
		{
			return false;
		}

		//Ensure the order was not assigned recently.
		static dev_u32 s_uMinTime = 20000;
		if(pOrder->GetAssignedToEntity() && ((pOrder->GetTimeOfAssignment() + s_uMinTime) > fwTimer::GetTimeInMilliseconds()))
		{
			return true;
		}
	}
	else if(pPed->IsInjured())
	{
		return false;
	}

	//Calculate the max order distance.
	const DispatchType eDispatchType = GetDispatchType();
	float fMaxOrderDistance = CDispatchData::GetOnFootOrderDistance(eDispatchType);
	if(pPed->GetIsInVehicle())
	{
		float fMinOrderDistance = 0.0f;
		if (CDispatchData::GetMinMaxInVehicleOrderDistances(eDispatchType, fMinOrderDistance, fMaxOrderDistance))
		{
			// Scale the order assignment distance depending on how fast the player is moving if in a vehicle to
			// try to remove peds which fall behind to allow new ones to spawn in front
			CPed* pPlayerPed = CGameWorld::FindLocalPlayer();
			if (pPlayerPed && pPlayerPed->GetIsInVehicle())
			{
				//Grab the vehicle.
				CVehicle* pPlayerVehicle = pPlayerPed->GetMyVehicle();
				
				//Grab the forward vector.
				Vec3V vPlayerVehicleFwd = pPlayerVehicle->GetTransform().GetForward();
				
				//Calculate the normalized offset between the entity and the player.
				Vec3V vOffset = Subtract(pPed->GetTransform().GetPosition(), pPlayerPed->GetTransform().GetPosition());
				
				//Ensure the player is moving away from the entity.
				ScalarV scDot = Dot(vOffset, vPlayerVehicleFwd);
				if(IsLessThanAll(scDot, ScalarV(V_ZERO)))
				{
					const float MIN_SPEED_MODIFIER = rage::Clamp(fMinOrderDistance / fMaxOrderDistance, 0.0f, 1.0f); // Restrict the modifier from scaling the order distance too much
					const float fSpeedModifier = MAX(MIN_SPEED_MODIFIER, (1.0f - ComputeSpeedModifier(*pPlayerVehicle)));
					fMaxOrderDistance *= fSpeedModifier;
				}
			}
		}
	}

	//Check if the distance is within the maximum.
	ScalarV scDistSq = DistSquared(pPed->GetTransform().GetPosition(), VECTOR3_TO_VEC3V(CGameWorld::FindLocalPlayerCoors()));
	ScalarV scMaxDistSq = ScalarVFromF32(square(fMaxOrderDistance));
	if(IsLessThanAll(scDistSq, scMaxDistSq))
	{
		return true;
	}

	return false;
}

bool CDispatchService::GetIsEntityValidToBeAssignedOrder(CEntity* pEntity)
{
	//Check if we are in MP.
	if(NetworkInterface::IsGameInProgress())
	{
		//Note: These checks should match the asserts in Orders.cpp .

		//Ensure the entity is valid.
		if(!pEntity)
		{
			return false;
		}

		//Ensure the net object is valid.
		netObject* pNetObject = NetworkUtils::GetNetworkObjectFromEntity(pEntity);
		if(!pNetObject)
		{
			return false;
		}
	}

	return GetIsEntityValidForOrder(pEntity);
}

bool CDispatchService::GetAndVerifyModelIds(fwModelId &vehicleModelId, fwModelId &pedModelId, fwModelId& objModelId, int maxVehicleInMap)
{
	if(!m_pDispatchVehicleSets)
	{
		return false;
	}

	int iVehicleSetscount = m_pDispatchVehicleSets->GetCount();

	// If we want to force a vehicle set then set our current vehicle set to the one we chose
	if(m_iForcedVehicleSet >= 0 && m_iForcedVehicleSet < iVehicleSetscount)
	{
		m_CurrVehicleSet = m_iForcedVehicleSet;
	}
	// Make sure our current vehicle set is within our count (otherwise loop back to the beginning)
	else if(m_CurrVehicleSet >= iVehicleSetscount)
	{		
		m_CurrVehicleSet = 0;
	}

	if (aiVerifyf(m_CurrVehicleSet >=0 && m_CurrVehicleSet < iVehicleSetscount, "m_CurrVehicleSet %d is invalid", m_CurrVehicleSet))
	{
		// Here we have to get the current vehicle set we want to use
		CConditionalVehicleSet* pVehicleSet = CDispatchData::FindConditionalVehicleSet((*m_pDispatchVehicleSets)[m_CurrVehicleSet].GetHash());
		if(pVehicleSet)
		{	
			// Get the vehicle names array and choose a random one from the entries
			const atArray<atHashWithStringNotFinal>& vehicleModelNames = pVehicleSet->GetVehicleModelHashes();
			int iVehicleCount = vehicleModelNames.GetCount();
			if(iVehicleCount > 0)
			{
				CModelInfo::GetBaseModelInfoFromHashKey(vehicleModelNames[fwRandom::GetRandomNumberInRange(0, iVehicleCount)].GetHash(), &vehicleModelId);
				if(vehicleModelId.IsValid() && CModelInfo::HaveAssetsLoaded(vehicleModelId))
				{
					if(maxVehicleInMap >= 0)	// CPoliceRidersDispatch passes in -1, this is not actually a vehicle in that case, but a horse.
					{
						// Don't spawn if there are already a number of police cars on the map. Exclude parked vehicles.
						if( CVehiclePopulation::CountVehsOfType(vehicleModelId.GetModelIndex(), true, true, true) >= maxVehicleInMap)
						{
							return false;
						}
					}
				}
				else
				{
					return false;
				}
			}
			else
			{
				return false;
			}

			// Get the ped names array and choose a random one from the entries
			const atArray<atHashWithStringNotFinal>& pedModelNames = pVehicleSet->GetPedModelHashes();
			int iPedCount = pedModelNames.GetCount();
			if(iPedCount > 0)
			{
				CModelInfo::GetBaseModelInfoFromHashKey(pedModelNames[fwRandom::GetRandomNumberInRange(0, iPedCount)].GetHash(), &pedModelId);
			}
			else
			{
				pedModelId.Invalidate();
			}
			
			// Get the object names array and choose a random one from the entries
			const atArray<atHashWithStringNotFinal>& objModelNames = pVehicleSet->GetObjectModelHashes();
			int iObjCount = objModelNames.GetCount();
			if(iObjCount > 0)
			{
				CModelInfo::GetBaseModelInfoFromHashKey(objModelNames[fwRandom::GetRandomNumberInRange(0, iObjCount)].GetHash(), &objModelId);
			}
			else
			{
				objModelId.Invalidate();
			}

			// For dev builds assert that needed peds are loaded
#if __DEV
			for(int j = 0; j < pedModelNames.GetCount(); j++)
			{
				fwModelId iPedModelId;
				CModelInfo::GetBaseModelInfoFromHashKey(pedModelNames[j].GetHash(), &iPedModelId);
				if(iPedModelId.IsValid())
				{
					aiAssert(CModelInfo::HaveAssetsLoaded(iPedModelId));
				}
			}
#endif

			return true;
		}
	}

	return false;
}

void CDispatchService::RequestAllModels(CDispatchResponse* pDispatchResponse)
{
    m_pDispatchVehicleSets = NULL;//reset for later.. 
    if(pDispatchResponse)
    {
        m_pDispatchVehicleSets = pDispatchResponse->GetDispatchVehicleSets();
        if(m_pDispatchVehicleSets)
        {
			m_iForcedVehicleSet = -1;

			if(pDispatchResponse->GetUseOneVehicleSet())
			{
				int iNumVehSets = m_pDispatchVehicleSets->GetCount();
				for(int iVehicleSet = 0; iVehicleSet < iNumVehSets; iVehicleSet++)
				{
					// Here we have to get the current vehicle set we want to use
					CConditionalVehicleSet* pVehicleSet = CDispatchData::FindConditionalVehicleSet((*m_pDispatchVehicleSets)[iVehicleSet].GetHash());
					if(pVehicleSet)
					{
						// Get the vehicle model name array and loop through
						const atArray<atHashWithStringNotFinal>& vehicleModelNames = pVehicleSet->GetVehicleModelHashes();
						for(int j = 0; j < vehicleModelNames.GetCount(); j++)
						{
							// Get the model id for that set and verify it
							fwModelId vehicleModelId;
							CModelInfo::GetBaseModelInfoFromHashKey(vehicleModelNames[j].GetHash(), &vehicleModelId);
							if(vehicleModelId.IsValid() && CModelInfo::HaveAssetsLoaded(vehicleModelId))
							{
								m_iForcedVehicleSet = iVehicleSet;
								break;
							}
						}
						
						//Check if we have found a forced vehicle set.
						if(m_iForcedVehicleSet >= 0)
						{
							break;
						}
					}
				}

				if(m_iForcedVehicleSet < 0 && iNumVehSets)
				{
					m_iForcedVehicleSet = fwRandom::GetRandomNumberInRange(0, iNumVehSets);
				}
			}
			else
			{
				//Keep track of the vehicle set info.
				struct VehicleSetInfo
				{
					VehicleSetInfo()
					: m_bIsMutuallyExclusive(false)
					, m_bIsStreamedIn(false)
					{}

					bool m_bIsMutuallyExclusive;
					bool m_bIsStreamedIn;
				};
				static const int s_iMaxVehicleSetInfos = 8;
				VehicleSetInfo aVehicleSetInfos[s_iMaxVehicleSetInfos];
				int iNumVehicleSetInfos = 0;
				aiAssert(m_pDispatchVehicleSets->GetCount() <= s_iMaxVehicleSetInfos);

				//Keep track of some information.
				bool bHasMutuallyExclusiveVehicleSet = false;
				bool bHasStreamedInMutuallyExclusiveVehicleSet = false;

				//Load up the vehicle set infos.
				int iNumVehSets = m_pDispatchVehicleSets->GetCount();
				for(int iVehicleSet = 0; iVehicleSet < iNumVehSets; iVehicleSet++)
				{
					//Initialize the info.
					VehicleSetInfo& rInfo = aVehicleSetInfos[iNumVehicleSetInfos++];
					rInfo.m_bIsMutuallyExclusive = false;
					rInfo.m_bIsStreamedIn = false;

					//Ensure the vehicle set is valid.
					CConditionalVehicleSet* pVehicleSet = CDispatchData::FindConditionalVehicleSet((*m_pDispatchVehicleSets)[iVehicleSet].GetHash());
					if(!pVehicleSet)
					{
						continue;
					}

					//Ensure the vehicle set is mutually exclusive.
					if(!pVehicleSet->IsMutuallyExclusive())
					{
						continue;
					}

					//Set the flags.
					rInfo.m_bIsMutuallyExclusive = true;
					bHasMutuallyExclusiveVehicleSet = true;

					//Check if the vehicle set is streamed in.
					const atArray<atHashWithStringNotFinal>& vehicleModelNames = pVehicleSet->GetVehicleModelHashes();
					for(int j = 0; j < vehicleModelNames.GetCount(); j++)
					{
						fwModelId vehicleModelId;
						CModelInfo::GetBaseModelInfoFromHashKey(vehicleModelNames[j].GetHash(), &vehicleModelId);
						if(vehicleModelId.IsValid() && CModelInfo::HaveAssetsLoaded(vehicleModelId))
						{
							rInfo.m_bIsStreamedIn = true;
							bHasStreamedInMutuallyExclusiveVehicleSet = true;
							break;
						}
					}
				}

				//Check if we have a streamed in mutually exclusive vehicle set.
				if(bHasStreamedInMutuallyExclusiveVehicleSet)
				{
					//Choose a random vehicle set.
					if(iNumVehicleSetInfos)
					{
						SemiShuffledSequence shuffler(iNumVehicleSetInfos);
						for(int i = 0; i < iNumVehicleSetInfos; ++i)
						{
							//Check if the vehicle set is not mutually exclusive, or is streamed in.
							int iIndex = shuffler.GetElement(i);
							const VehicleSetInfo& rInfo = aVehicleSetInfos[iIndex];
							if(!rInfo.m_bIsMutuallyExclusive || rInfo.m_bIsStreamedIn)
							{
								m_iForcedVehicleSet = iIndex;
								break;
							}
						}
					}
				}
				//Check if we have a mutually exclusive vehicle set.
				else if(bHasMutuallyExclusiveVehicleSet)
				{
					//Choose a random vehicle set.
					if(iNumVehSets)
					{
						m_iForcedVehicleSet = fwRandom::GetRandomNumberInRange(0, iNumVehSets);
					}
				}
			}

            for(int i = 0; i < m_pDispatchVehicleSets->GetCount(); i++)
            {
                CConditionalVehicleSet* pVehicleSet = CDispatchData::FindConditionalVehicleSet((*m_pDispatchVehicleSets)[i].GetHash());
                if(pVehicleSet && (m_iForcedVehicleSet < 0 || m_iForcedVehicleSet == i))
                {
					//Ensure we can use the vehicle set.
					if(!CanUseVehicleSet(*pVehicleSet))
					{
						continue;
					}

					// Get the vehicle model name array and loop through
					const atArray<atHashWithStringNotFinal>& vehicleModelNames = pVehicleSet->GetVehicleModelHashes();
					for(int j = 0; j < vehicleModelNames.GetCount(); j++)
					{
						// We invalidate our model ID then get it, if it comes back valid we request a stream on it
						fwModelId iVehicleModelId;
						CModelInfo::GetBaseModelInfoFromHashKey(vehicleModelNames[j].GetHash(), &iVehicleModelId);
						if(Verifyf(iVehicleModelId.IsValid(), "Vehicle model {%s} not found", vehicleModelNames[j].GetCStr()))
						{
							s32 transientLocalIdx = CModelInfo::AssignLocalIndexToModelInfo(iVehicleModelId).Get();
							m_streamingRequests.PushRequest(transientLocalIdx, CModelInfo::GetStreamingModuleId());
						}
					}

                    // Get the ped model name array and loop through
                    const atArray<atHashWithStringNotFinal>& pedModelNames = pVehicleSet->GetPedModelHashes();
                    for(int j = 0; j < pedModelNames.GetCount(); j++)
                    {
                        // We invalidate our model ID then get it, if it comes back valid we request a stream on it
                        fwModelId iPedModelId;
                        CModelInfo::GetBaseModelInfoFromHashKey(pedModelNames[j].GetHash(), &iPedModelId);
                        if(Verifyf(iPedModelId.IsValid(), "Ped model {%s} not found", pedModelNames[j].GetCStr()))
                        {
							s32 transientLocalIdx = CModelInfo::AssignLocalIndexToModelInfo(iPedModelId).Get();
                            m_streamingRequests.PushRequest(transientLocalIdx, CModelInfo::GetStreamingModuleId());
                        }
                    }
                    
					// Get the object model name array and loop through
					const atArray<atHashWithStringNotFinal>& objectModelNames = pVehicleSet->GetObjectModelHashes();
					for(int j = 0; j < objectModelNames.GetCount(); j++)
					{
						// We invalidate our model ID then get it, if it comes back valid we request a stream on it
						fwModelId iObjectModelId;
						CModelInfo::GetBaseModelInfoFromHashKey(objectModelNames[j].GetHash(), &iObjectModelId);
						if(Verifyf(iObjectModelId.IsValid(), "Object model {%s} not found", objectModelNames[j].GetCStr()))
						{
							s32 transientLocalIdx = CModelInfo::AssignLocalIndexToModelInfo(iObjectModelId).Get();
							m_streamingRequests.PushRequest(transientLocalIdx, CModelInfo::GetStreamingModuleId());
						}
					}
					
					// Get the clip set name array and loop through
					const atArray<atHashWithStringNotFinal>& clipSetNames = pVehicleSet->GetClipSetHashes();
					for(int j = 0; j < clipSetNames.GetCount(); j++)
					{
						// We get our clip set id, if it comes back valid we request a stream on it
						fwMvClipSetId clipSetId(clipSetNames[j].GetHash());
						if(Verifyf(clipSetId != CLIP_SET_ID_INVALID, "Clip set {%s} not found", clipSetNames[j].GetCStr()))
						{
							s32 transientLocalIdx = fwClipSetManager::GetClipDictionaryIndex(clipSetId);
							if(Verifyf(transientLocalIdx != -1, "Clip set (%s) not found", clipSetNames[j].GetCStr()))
							{
								m_streamingRequests.PushRequest(transientLocalIdx, fwAnimManager::GetStreamingModuleId());
							}
						}
					}
                }
            }
        }
    }
}

Vec3V_Out CDispatchService::GenerateDispatchLocationForIncident(const CIncident& rIncident) const
{
	//Check if the spawn helper is valid.
	CDispatchSpawnHelper* pSpawnHelper = ms_SpawnHelpers.FindSpawnHelperForIncident(rIncident);
	if(pSpawnHelper)
	{
		//Check if we can generate a random dispatch position.
		Vec3V vDispatchPosition;
		if(pSpawnHelper->CanGenerateRandomDispatchPosition() && pSpawnHelper->GenerateRandomDispatchPosition(vDispatchPosition))
		{
			return vDispatchPosition;
		}
	}

	//Use the incident location.
	Vec3V vIncidentLocation = VECTOR3_TO_VEC3V(rIncident.GetLocation());

	return vIncidentLocation;
}

#if __BANK
void CDispatchService::ClearLastSpawnFailureReason()
{
	SetLastSpawnFailureReason(NULL);
}

void CDispatchService::SetLastSpawnFailureReason(const char* pReason) const
{
	if(pReason)
	{
		dispatchServicesDebugf3("Frame %d : %s : Spawn for dispatch type: %s failed due to: %s", fwTimer::GetFrameCount(), __FUNCTION__, CDispatchManager::GetDispatchTypeName(GetDispatchType()), pReason);
	}

	ms_aSpawnFailureReasons[GetDispatchType()] = pReason;
}
#endif

void CDispatchService::AdjustResourceAllocations()
{
	//Iterate over the incidents.
	int iSize = m_ResponseIncidents.GetCount();
	for(int i = 0; i < iSize; i++)
	{
		//Ensure the incident is valid.
		CIncident* pIncident = m_ResponseIncidents[i];
		if(!pIncident)
		{
			continue;
		}

		//Ensure the incident is wanted.
		if(pIncident->GetType() != CIncident::IT_Wanted)
		{
			continue;
		}

		//Adjust the resource allocations.
		static_cast<CWantedIncident *>(pIncident)->AdjustResourceAllocations(GetDispatchType());
	}
}

void CDispatchService::AdjustResourceRequests()
{
	//Iterate over the incidents.
	int iSize = m_ResponseIncidents.GetCount();
	for(int i = 0; i < iSize; i++)
	{
		//Ensure the incident is valid.
		CIncident* pIncident = m_ResponseIncidents[i];
		if(!pIncident)
		{
			continue;
		}

		//Ensure the incident is wanted.
		if(pIncident->GetType() != CIncident::IT_Wanted)
		{
			continue;
		}

		//Adjust the resource allocations.
		static_cast<CWantedIncident *>(pIncident)->AdjustResourceRequests(GetDispatchType());
	}
}

bool CDispatchService::ArePedsInVehicleValidToBeAssignedOrder(const CVehicle& rVehicle, const Resources& rResources)
{
	//Grab the seat manager.
	const CSeatManager* pSeatManager = rVehicle.GetSeatManager();

	//Iterate over the seats.
	int iNumSeats = pSeatManager->GetMaxSeats();
	for(int i = 0; i < iNumSeats; ++i)
	{
		//Ensure the ped in the seat is valid.
		CPed* pPed = pSeatManager->GetPedInSeat(i);
		if(!pPed)
		{
			continue;
		}

		//Ensure the ped is valid to be assigned an order.
		if(!IsPedValidToBeAssignedOrder(*pPed, rResources))
		{
			return false;
		}
	}

	return true;
}

bool CDispatchService::CanAcquireFreeResources(const CIncident& UNUSED_PARAM(rIncident)) const
{
	//Hack to make this merge better with the BOB version -- this function should be made virtual at some point.
	if(GetDispatchType() == DT_PoliceRoadBlock)
	{
		return false;
	}

	return true;
}

void CDispatchService::FindFreeResources(const CIncident& rIncident, Resources& rResources)
{
	//Find free resources from nearby.
	FindFreeResourcesFromNearby(rIncident, rResources);

	//Find free resources from pooled.
	FindFreeResourcesFromPooled(rIncident, rResources);
}

void CDispatchService::FindFreeResourcesFromNearby(const CIncident& rIncident, Resources& rResources)
{
	//Find free resources from nearby peds in vehicles.
	FindFreeResourcesFromNearbyPedsInVehicles(rIncident, rResources);

	bool bUsePedsOnFoot = true;
	if(rIncident.GetType() == CIncident::IT_Scripted)
	{
		const CScriptIncident* pScriptIncident = static_cast<const CScriptIncident *>(&rIncident);
		if(!pScriptIncident->GetUseExistingPedsOnFoot())
		{
			bUsePedsOnFoot = false;
		}
	}

	if(bUsePedsOnFoot)
	{
		//Find free resources from nearby peds on foot.
		FindFreeResourcesFromNearbyPedsOnFoot(rIncident, rResources);
	}
}

void CDispatchService::FindFreeResourcesFromNearbyPedsInVehicles(const CIncident& rIncident, Resources& rResources)
{
	//Ensure the incident ped is valid.
	const CPed* pIncidentPed = GetPedForIncident(rIncident);
	if(!pIncidentPed)
	{
		return;
	}

	//Iterate over the nearby vehicles.
	const CEntityScannerIterator entityList = pIncidentPed->GetPedIntelligence()->GetNearbyVehicles();
	for(const CEntity* pEntity = entityList.GetFirst(); pEntity; pEntity = entityList.GetNext())
	{
		//Ensure the resources are valid.
		int iResourcesNeeded = rResources.GetCapacity() - rResources.GetCount();
		if(iResourcesNeeded <= 0)
		{
			break;
		}

		//Grab the vehicle.
		const CVehicle* pVehicle = static_cast<const CVehicle *>(pEntity);

		//Find free resources from peds in the vehicle.
		FindFreeResourcesFromPedsInVehicle(*pVehicle, rResources);
	}
}

void CDispatchService::FindFreeResourcesFromNearbyPedsOnFoot(const CIncident& rIncident, Resources& rResources)
{
	//Ensure the incident ped is valid.
	const CPed* pIncidentPed = GetPedForIncident(rIncident);
	if(!pIncidentPed)
	{
		return;
	}

	//Iterate over the nearby peds.
	CEntityScannerIterator entityList = pIncidentPed->GetPedIntelligence()->GetNearbyPeds();
	for(CEntity* pEntity = entityList.GetFirst(); pEntity; pEntity = entityList.GetNext())
	{
		//Ensure the resources are valid.
		int iResourcesNeeded = rResources.GetCapacity() - rResources.GetCount();
		if(iResourcesNeeded <= 0)
		{
			break;
		}

		//Grab the ped.
		CPed* pPed = static_cast<CPed *>(pEntity);

		//Ensure the ped is on foot.
		if(!pPed->GetIsOnFoot())
		{
			continue;
		}

		//Find free resources from the ped.
		FindFreeResourcesFromPedOnFoot(*pPed, rResources);
	}
}

void CDispatchService::FindFreeResourcesFromPedOnFoot(CPed& rPed, Resources& rResources)
{
	//Assert that the ped is on foot.
	aiAssert(rPed.GetIsOnFoot());

	//Ensure the ped is valid to be assigned an order.
	if(!IsPedValidToBeAssignedOrder(rPed, rResources))
	{
		return;
	}

	//Add the ped.
	aiAssert(!rPed.GetPedIntelligence()->GetOrder());
	aiAssert(rResources.GetCapacity() != rResources.GetCount());
	rResources.Push(&rPed);
}

void CDispatchService::FindFreeResourcesFromPedsInVehicle(const CVehicle& rVehicle, Resources& rResources)
{
	//Ensure the peds in the vehicle are valid to be assigned the order.
	if(!ArePedsInVehicleValidToBeAssignedOrder(rVehicle, rResources))
	{
		return;
	}

	//Grab the seat manager.
	const CSeatManager* pSeatManager = rVehicle.GetSeatManager();

	//Iterate over the seats.
	int iNumSeats = pSeatManager->GetMaxSeats();
	for(int i = 0; i < iNumSeats; ++i)
	{
		//Ensure the resources are valid.
		int iResourcesNeeded = rResources.GetCapacity() - rResources.GetCount();
		if(iResourcesNeeded <= 0)
		{
			break;
		}

		//Ensure the ped in the seat is valid.
		CPed* pPed = pSeatManager->GetPedInSeat(i);
		if(!pPed)
		{
			continue;
		}

		//Add the ped.
		aiAssert(!pPed->GetPedIntelligence()->GetOrder());
		aiAssert(rResources.GetCapacity() != rResources.GetCount());
		rResources.Push(pPed);
	}
}

void CDispatchService::FindFreeResourcesFromPooled(const CIncident& rIncident, Resources& rResources)
{
	//Find free resources from pooled peds in vehicles.
	FindFreeResourcesFromPooledPedsInVehicles(rIncident, rResources);
	
	bool bUsePedsOnFoot = true;
	if(rIncident.GetType() == CIncident::IT_Scripted)
	{
		const CScriptIncident* pScriptIncident = static_cast<const CScriptIncident *>(&rIncident);
		if(!pScriptIncident->GetUseExistingPedsOnFoot())
		{
			bUsePedsOnFoot = false;
		}
	}

	if(bUsePedsOnFoot)
	{
		//Find free resources from pooled peds on foot.
		FindFreeResourcesFromPooledPedsOnFoot(rIncident, rResources);
	}
}

void CDispatchService::FindFreeResourcesFromPooledPedsInVehicles(const CIncident& UNUSED_PARAM(rIncident), Resources& rResources)
{
	//Iterate over the vehicles.
	CVehicle::Pool* pPool = CVehicle::GetPool();
	for(int i = 0; i < pPool->GetSize(); ++i)
	{
		//Ensure more resources are needed.
		int iResourcesNeeded = rResources.GetCapacity() - rResources.GetCount();
		if(iResourcesNeeded <= 0)
		{
			break;
		}

		//Ensure the vehicle is valid.
		CVehicle* pVehicle = pPool->GetSlot(i);
		if(!pVehicle)
		{
			continue;
		}

		//Find free resources from peds in the vehicle.
		FindFreeResourcesFromPedsInVehicle(*pVehicle, rResources);
	}
}

void CDispatchService::FindFreeResourcesFromPooledPedsOnFoot(const CIncident& UNUSED_PARAM(rIncident), Resources& rResources)
{
	//Iterate over the peds.
	CPed::Pool* pPool = CPed::GetPool();
	for(int i = 0; i < pPool->GetSize(); ++i)
	{
		//Ensure more resources are needed.
		int iResourcesNeeded = rResources.GetCapacity() - rResources.GetCount();
		if(iResourcesNeeded <= 0)
		{
			break;
		}

		//Ensure the ped is valid.
		CPed* pPed = pPool->GetSlot(i);
		if(!pPed)
		{
			continue;
		}

		//Ensure the ped is on foot.
		if(!pPed->GetIsOnFoot())
		{
			continue;
		}

		//Find free resources from the ped on foot.
		FindFreeResourcesFromPedOnFoot(*pPed, rResources);
	}
}

const CPed* CDispatchService::GetPedForIncident(const CIncident& rIncident) const
{
	//Ensure the incident entity is valid.
	const CEntity* pIncidentEntity = rIncident.GetEntity();
	if(!pIncidentEntity)
	{
		return NULL;
	}

	//Ensure the incident entity is a ped.
	if(!pIncidentEntity->GetIsTypePed())
	{
		return NULL;
	}

	return static_cast<const CPed *>(pIncidentEntity);
}

bool CDispatchService::IsPedValidToBeAssignedOrder(const CPed& rPed, const Resources& rResources)
{
	//Ensure the ped does not have an order.
	if(rPed.GetPedIntelligence()->GetOrder())
	{
		return false;
	}

	//Iterate over the resources.
	int iCount = rResources.GetCount();
	for(int i = 0; i < iCount; ++i)
	{
		//Ensure the ped is not a resource.
		if(&rPed == rResources[i])
		{
			return false;
		}
	}

	//Ensure the entity is valid to be assigned an order.
	CPed* pPed = const_cast<CPed *>(&rPed);
	if(!GetIsEntityValidToBeAssignedOrder(pPed))
	{
		return false;
	}

	return true;
}

bool CDispatchService::ShouldStreamInModels() const
{
	//Iterate over the incidents.
	int iSize = m_ResponseIncidents.GetCount();
	for(int i = 0; i < iSize; i++)
	{
		//Ensure the incident is valid.
		CIncident* pIncident = m_ResponseIncidents[i];
		if(!pIncident)
		{
			continue;
		}

		//Ensure we have a request.
		if(pIncident->GetRequestedResources(GetDispatchType()) <= 0)
		{
			continue;
		}

		//Don't attempted to spawn in the fire or ambulance whilst we are still waiting for the spawn timer.
		if(pIncident->GetType() == CIncident::IT_Fire || pIncident->GetType() == CIncident::IT_Injury)
		{
			if(m_fSpawnTimer > 0.0f)
			{
				continue;
			}
		}

		return true;
	}

	return false;
}

void CDispatchService::ResetSpawnTimer()
{
	//Calculate the time between spawn attempts.
	float fTimeBetweenSpawnAttempts;
	if(m_fTimeBetweenSpawnAttemptsOverride < 0.0f)
	{
		fTimeBetweenSpawnAttempts = GetTimeBetweenSpawnAttempts();
	}
	else
	{
		fTimeBetweenSpawnAttempts = m_fTimeBetweenSpawnAttemptsOverride;
	}
	
	//Calculate the time between spawn attempts multiplier.
	float fTimeBetweenSpawnAttemptsMultiplier;
	if(m_fTimeBetweenSpawnAttemptsMultiplierOverride < 0.0f)
	{
		fTimeBetweenSpawnAttemptsMultiplier = GetTimeBetweenSpawnAttemptsMultiplier();
	}
	else
	{
		fTimeBetweenSpawnAttemptsMultiplier = m_fTimeBetweenSpawnAttemptsMultiplierOverride;
	}
	
	//Calculate the spawn timer.
	m_fSpawnTimer = fTimeBetweenSpawnAttempts * fTimeBetweenSpawnAttemptsMultiplier;
}

void CDispatchService::UpdateModelStreamingRequests()
{
	bool bShouldStreamInModels = ShouldStreamInModels();

	if(!bShouldStreamInModels || CDispatchManager::ShouldReleaseStreamingRequests())
	{
		m_streamingRequests.ReleaseAll();
	}
	
	if(bShouldStreamInModels)
	{
		StreamModels();
	}
}

void CDispatchService::ManageResourcesForIncidents()
{
	BANK_ONLY(ClearLastSpawnFailureReason();)

	//Iterate over the incidents.
	int iNumIncidents = m_ResponseIncidents.GetCount();
	for(int i = 0; i < iNumIncidents; ++i)
	{
		//Ensure the incident is valid.
		CIncident* pIncident = m_ResponseIncidents[i];
		if(!pIncident)
		{
			continue;
		}

		//Ensure we should allocate new resources for the incident.
		if(!ShouldAllocateNewResourcesForIncident(*pIncident))
		{
			continue;
		}
		
		//Ensure the resources needed are valid.
		int iResourcesNeeded = (pIncident->GetAllocatedResources(GetDispatchType()) - pIncident->GetAssignedResources(GetDispatchType()));
		if(iResourcesNeeded <= 0)
		{
			continue;
		}
		
		//Keep track of the total resources needed for the incident.
		aiAssertf(iResourcesNeeded <= Resources::MaxEntities, "iResourcesNeeded: %d, Max: %d, Dispatch Type: %d, Requested Resources: %d, Allocated Resources: %d, Assigned Resources: %d",
			iResourcesNeeded, Resources::MaxEntities, GetDispatchType(), pIncident->GetRequestedResources(GetDispatchType()), pIncident->GetAllocatedResources(GetDispatchType()),
			pIncident->GetAssignedResources(GetDispatchType()));
		Resources resources(iResourcesNeeded);
		
		//Check if we can acquire free resources.
		if(CanAcquireFreeResources(*pIncident))
		{
			//Find free resources that can be used for the incident.
			FindFreeResources(*pIncident, resources);
		}
		
		//Spawn new resources.
		SpawnNewResourcesTimed(*pIncident, resources);

		s32 newPotentialOrders = resources.GetCount();

		//No need to call AssignNewOrders if new potential orders are <= 0.  Internally within AssignNewOrders it bails anyway if resources.GetCount <= 0, just do it here an avoid processing.
		if (newPotentialOrders <= 0)
		{
			continue;
		}

		//Ensure that if we assign resources and therefore orders to this incident it will not go over the maximum number of orders available.
		s32 ordersActive = COrder::GetPool()->GetNoOfUsedSpaces();
		s32 potentialOrdersActive = ordersActive + newPotentialOrders;
		if (potentialOrdersActive >= COrderManager::MAX_ORDERS)
		{
			continue;
		}

#if __ASSERT
		//Assert that none of the peds have an order.
		for(int i = 0; i < resources.GetCount(); ++i)
		{
			CEntity* pEntity = resources[i];
			if(pEntity)
			{
				aiAssert(!pEntity->GetIsTypePed() || !static_cast<CPed *>(pEntity)->GetPedIntelligence()->GetOrder());
			}
		}
#endif

		//Assign new orders.
		AssignNewOrders(*pIncident, resources);
		pIncident->SetAssignedResources(GetDispatchType(), pIncident->GetAssignedResources(GetDispatchType()) + resources.GetCount());
	}
}

// Callback for delayed resource spawning - assigns orders for resources spawned, to mimic what happens after SpawnResourcesTimed above,
// only we currently only spawn one new resource per call of this function (e.g. spawning a new swat ped)
void CDispatchService::DispatchDelayedSpawnCallback(CEntity* pEntity, CIncident* pIncident, s32 UNUSED_PARAM(groupId), COrder* UNUSED_PARAM(pOrder))
{
	if(pEntity && pIncident)
	{
		Resources rResources(1); // enough room for our new resources

		//Add the entity.
		aiAssert(!pEntity->GetIsTypePed() || !static_cast<CPed *>(pEntity)->GetPedIntelligence()->GetOrder());
		aiAssert(rResources.GetCapacity() != rResources.GetCount());
		rResources.Push(pEntity);

		AssignNewOrders(*pIncident, rResources);
	}
}

float CDispatchServiceWanted::GetTimeBetweenSpawnAttemptsMultiplier() const
{
	//Ensure the wanted is valid.
	const CWanted* pWanted = CGameWorld::FindLocalPlayerWanted();
	if(!pWanted)
	{
		return 1.0f;
	}
	
	return pWanted->CalculateTimeBetweenSpawnAttemptsModifier();
}

void CDispatchServiceWanted::StreamModels()
{
    if( m_streamingRequests.GetRequestCount() == 0 )
    {
        StreamWantedIncidentModels();
    }

	// This needs to be called to add refs to the requests.
	m_streamingRequests.HaveAllLoaded();
}

// PURPOSE: Wanted services call this function to stream in the models they use as there is specific behavior to do so
void CDispatchServiceWanted::StreamWantedIncidentModels()
{
    // Clear out our vehicle sets array pointer for now
    m_pDispatchVehicleSets = NULL;

    // Find and verify both the wanted and the response set for that wanted level
    CWanted* pWanted = CGameWorld::FindLocalPlayerWanted();
    CWantedResponse* pWantedResponse = pWanted ? CDispatchData::GetWantedResponse(pWanted->GetWantedLevel()) : NULL;
    if(pWantedResponse)
    {
        // Find and verify the dispatch service/response for our dispatch type
        RequestAllModels(pWantedResponse->GetDispatchServices(GetDispatchType()));
    }
}

CWanted* CDispatchServiceWanted::GetIncidentPedWanted(const CIncident& rIncident)
{
	const CEntity* pEntity = rIncident.GetEntity();
	if(pEntity && pEntity->GetIsTypePed())
	{
		return static_cast<const CPed*>(pEntity)->GetPlayerWanted();
	}

	return NULL;
}

void CDispatchServiceWanted::SetupPedVariation(CPed& rPed, eWantedLevel wantedLevel)
{
	sDispatchModelVariations* pModelVariations = CDispatchData::FindDispatchModelVariations(rPed);
	if(pModelVariations)
	{
		int iNumVariations = pModelVariations->m_ModelVariations.GetCount();
		for(int i = 0; i < iNumVariations; i++)
		{
			DispatchModelVariation &pedVariation = pModelVariations->m_ModelVariations[i];
			if(wantedLevel >= pedVariation.m_MinWantedLevel)
			{
				rPed.SetVariation(pedVariation.m_Component, pedVariation.m_DrawableId, 0, 0, 0, 0, false);
				rPed.SetArmour(MAX(rPed.GetArmour(), pedVariation.m_Armour));
			}
		}
	}
}

void CDispatchServiceWanted::DispatchDelayedSpawnCallback(CEntity* pEntity, CIncident* pIncident, s32 UNUSED_PARAM(pedGroupIndex), COrder* UNUSED_PARAM(pOrder))
{
	if(pEntity)
	{
		if(pEntity->GetIsTypePed())
		{
			if(pIncident)
			{
				CWanted* pWanted = CDispatchServiceWanted::GetIncidentPedWanted(*pIncident);
				if(pWanted)
				{
					CDispatchServiceWanted::SetupPedVariation(*(static_cast<CPed*>(pEntity)), pWanted->GetWantedLevel());
				}
			}

			static_cast<CPed*>(pEntity)->GetPedIntelligence()->GetCombatBehaviour().SetFlag(CCombatData::BF_DisableFleeFromCombat);
			static_cast<CPed*>(pEntity)->SetPedConfigFlag(CPED_CONFIG_FLAG_CreatedByDispatch, true);
		}

		// Call Parent to add order for the ped and add a resource to the incident
		CDispatchService::DispatchDelayedSpawnCallback(pEntity, pIncident);
	}
}

bool CDispatchServiceEmergancy::IsActive() const
{ 
	CPed* pPlayer = CGameWorld::FindLocalPlayer();
	aiAssert(pPlayer);
	
	if(pPlayer->GetPlayerWanted() && FindPlayerPed()->GetPlayerWanted()->GetWantedLevel() > WANTED_LEVEL2)
	{
		return false;
	}
	
	return m_bDispatchActive; 
}

void CDispatchServiceEmergancy::StreamModels()
{
    if( m_streamingRequests.GetRequestCount() == 0 )
    {
        RequestServiceModels();
    }

	// This needs to be called to add refs to the requests.
	m_streamingRequests.HaveAllLoaded();
}

void CDispatchServiceEmergancy::RequestServiceModels()
{
    RequestAllModels(CDispatchData::GetEmergencyResponse(GetDispatchType()));
}

CPoliceAutomobileDispatch::CPoliceAutomobileDispatch(const DispatchType dispatchType)
: CDispatchServiceWanted(dispatchType)
, m_vLastDispatchLocation(Vector3::ZeroType)
, m_nDriverOrderOverride(CPoliceOrder::PO_NONE)
, m_nPassengerOrderOverride(CPoliceOrder::PO_NONE)
, m_fTimeBetweenSearchPosUpdate(DEFAULT_TIME_BETWEEN_SEARCH_POS_UPDATES)
, m_uRunningFlags(0)
{

}

CPoliceAutomobileDispatch::~CPoliceAutomobileDispatch()
{

}

void CPoliceAutomobileDispatch::AssignNewOrder(CEntity& rEntity, CIncident& rIncident)
{
	//Keep track of the police dispatch order type.
	CPoliceOrder::ePoliceDispatchOrderType ePoliceOrder = CPoliceOrder::PO_NONE;

	//Check the entity is a ped.
	if(aiVerify(rEntity.GetIsTypePed()))
	{
		//Check if the ped is driving a vehicle.
		if(static_cast<CPed *>(&rEntity)->GetIsDrivingVehicle())
		{
			//Check if the override is valid.
			if(m_nDriverOrderOverride != CPoliceOrder::PO_NONE)
			{
				ePoliceOrder = m_nDriverOrderOverride;
			}
			else
			{
				ePoliceOrder = CPoliceOrder::PO_WANTED_GOTO_DISPATCH_LOCATION_AS_DRIVER;
			}
		}
		else 
		{
			//Check if the override is valid.
			if(m_nPassengerOrderOverride != CPoliceOrder::PO_NONE)
			{
				ePoliceOrder = m_nPassengerOrderOverride;
			}
			else
			{
				ePoliceOrder = CPoliceOrder::PO_WANTED_GOTO_DISPATCH_LOCATION_AS_PASSENGER;
			}
		}
	}

	//Generate the dispatch location for the new order.
	Vec3V vDispatchLocation = GenerateDispatchLocationForNewOrder(rIncident);
	
	//Add the order.
	CPoliceOrder order(GetDispatchType(), &rEntity, &rIncident, ePoliceOrder, VEC3V_TO_VECTOR3(vDispatchLocation));
	COrderManager::GetInstance().AddOrder(order);
}

bool CPoliceAutomobileDispatch::GetIsEntityValidForOrder( CEntity* pEntity )
{
	//Ensure the entity is valid for the order.
	if(!CDispatchServiceWanted::GetIsEntityValidForOrder(pEntity))
	{
		return false;
	}

	//Assert that the entity is valid, and a ped.
	aiAssert(pEntity);
	aiAssert(pEntity->GetIsTypePed());

	CPed* pPed = static_cast<CPed*>(pEntity);
	if(pPed->PopTypeIsRandom() && (pPed->IsCopPed() || pPed->IsArmyPed()))
	{
		return true;
	}

	return false;
}

bool CPoliceAutomobileDispatch::GetIsEntityValidToBeAssignedOrder(CEntity* pEntity)
{
	//Ensure the base class is valid.
	if(!CDispatchService::GetIsEntityValidToBeAssignedOrder(pEntity))
	{
		return false;
	}
	
	//Ensure the entity is valid.
	if(!aiVerifyf(pEntity, "Entity is invalid."))
	{
		return false;
	}
	
	//Ensure the entity is a ped.
	if(!pEntity->GetIsTypePed())
	{
		return false;
	}

	// Only give to peds on foot or in normal cars/trucks
	CPed* pPed = static_cast<CPed*>(pEntity);
	CVehicle* pVehicle = pPed->GetVehiclePedInside();
	if(pVehicle && pVehicle->GetVehicleType() != VEHICLE_TYPE_CAR)
	{
		return false;
	}
	
	return true;
}


#define MIN_POLICE_AUTOMOBILE_RESOURCES_NEEDED 2

void CPoliceAutomobileDispatch::GetNumNetResourcesToReservePerIncident(u32 &numPeds, u32 &numVehicles, u32 &numObjects) const
{
	numPeds			= MIN_POLICE_AUTOMOBILE_RESOURCES_NEEDED;
	numVehicles		= 1;
	numObjects		= 0;
}

bool CPoliceAutomobileDispatch::SpawnNewResources(CIncident& rIncident, Resources& rResources)
{
	int iResourcesNeeded = rResources.GetCapacity() - rResources.GetCount();

	// Make sure the assets have been streamed
	if((m_streamingRequests.GetRequestCount() <= 0) || !m_streamingRequests.HaveAllLoaded())
	{
		BANK_ONLY(SetLastSpawnFailureReason("Streaming has not completed.");)
		return false;
	}

	fwModelId iVehicleModelId;
	fwModelId iSelectedPedModelId;
	fwModelId iObjectModelId;
	if(!GetAndVerifyModelIds(iVehicleModelId, iSelectedPedModelId, iObjectModelId, MaxVehiclesInMap))
	{
		BANK_ONLY(SetLastSpawnFailureReason("The model ids are invalid.");)
		return false;
	}
	
	//Ensure the spawn helper is valid.
	CDispatchSpawnHelper* pSpawnHelper = ms_SpawnHelpers.FindSpawnHelperForIncident(rIncident);
	if(!pSpawnHelper)
	{
		BANK_ONLY(SetLastSpawnFailureReason("The spawn helper is invalid.");)
		return false;
	}
	
	//Ensure the spawn helper can spawn a vehicle.
	if(!pSpawnHelper->CanSpawnVehicle())
	{
		BANK_ONLY(SetLastSpawnFailureReason("The spawn helper can't spawn a vehicle.");)
		return false;
	}

	// If we decided on a car we need to make sure we need at least the min number of police car resources
	// Bikes don't matter because they only need one ped and are handled by the first resources needed check
	CVehicleModelInfo *pModelInfo = (CVehicleModelInfo *)CModelInfo::GetBaseModelInfo(iVehicleModelId);
	if(pModelInfo)
	{
		if(!pModelInfo->GetIsBike() && iResourcesNeeded < MIN_POLICE_AUTOMOBILE_RESOURCES_NEEDED)
		{
			// Increment our current vehicle set so we can try and find a bike (as this failure implies we only need 1 resource)
			m_CurrVehicleSet++;
			return false;
		}
	}
	
	//Generate a random dispatch position.
	if(!pSpawnHelper->GenerateRandomDispatchPosition(RC_VEC3V(m_vLastDispatchLocation)))
	{
		BANK_ONLY(SetLastSpawnFailureReason("The spawn helper failed to generate a random dispatch position.");)
		return false;
	}

	//Note that the last dispatch location is valid.
	m_uRunningFlags.SetFlag(RF_IsLastDispatchLocationValid);
	
	//Spawn a new vehicle.
	CVehicle* pVehicle = SpawnNewVehicleWithOrders(*pSpawnHelper, iVehicleModelId, m_nDriverOrderOverride, m_nPassengerOrderOverride);
	if(pVehicle)
	{
		TUNE_GROUP_INT(POLICE_AUTOMOBILE_DISPATCH, iExtraRemovalTime, 30000, 0, 120000, 1000);
		pVehicle->DelayedRemovalTimeReset(iExtraRemovalTime);
		pVehicle->m_nVehicleFlags.bWasCreatedByDispatch = true;

		//Add the police occupants to the vehicle.
		CVehiclePopulation::AddPoliceVehOccupants(pVehicle, true, false, iSelectedPedModelId.GetModelIndex(), const_cast<CIncident*>(&rIncident), GetDispatchType());

		CWanted* pWanted = CDispatchServiceWanted::GetIncidentPedWanted(rIncident);

		//Grab the seat manager.
		const CSeatManager* pSeatManager = pVehicle->GetSeatManager();

		//Iterate over the seats.
		for(int i = 0; i < pSeatManager->GetMaxSeats(); ++i)
		{
			//Ensure the ped is valid.
			CPed* pPed = pSeatManager->GetPedInSeat(i);
			if(!pPed)
			{
				continue;
			}

			if(pWanted)
			{
				CDispatchServiceWanted::SetupPedVariation(*pPed, pWanted->GetWantedLevel());
			}

			// Dispatched law peds don't flee from combat
			pPed->GetPedIntelligence()->GetCombatBehaviour().SetFlag(CCombatData::BF_DisableFleeFromCombat);
			pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_CreatedByDispatch, true);

			//Add the ped.
			aiAssert(!pPed->GetPedIntelligence()->GetOrder());
			aiAssert(rResources.GetCapacity() != rResources.GetCount());
			rResources.Push(pPed);
		}

		// Increment our current vehicle set and reset it to 0 if we've hit the max
		m_CurrVehicleSet++;
	}
	
	return true;
}

void CPoliceAutomobileDispatch::Update()
{
	//Call the base class version.
	CDispatchService::Update();

	//Clear the order overrides.
	m_nDriverOrderOverride		= CPoliceOrder::PO_NONE;
	m_nPassengerOrderOverride	= CPoliceOrder::PO_NONE;

	//Note that the last dispatch location is no longer valid.
	m_uRunningFlags.ClearFlag(RF_IsLastDispatchLocationValid);

	if( IsActive() )
	{
		CDispatchService::UpdateCurrentSearchAreas(m_fTimeBetweenSearchPosUpdate);

		UpdateOrders();
	}
}

bool CPoliceAutomobileDispatch::IsOrderValidForPed(const CPoliceOrder& rOrder, const CPed& rPed)
{
	//Check the order type.
	switch(rOrder.GetPoliceOrderType())
	{
		case CPoliceOrder::PO_NONE:
		{
			//This order is always considered invalid.
			return false;
		}
		case CPoliceOrder::PO_ARREST_WAIT_AT_DISPATCH_LOCATION_AS_DRIVER:
		case CPoliceOrder::PO_ARREST_WAIT_AT_DISPATCH_LOCATION_AS_PASSENGER:
		case CPoliceOrder::PO_WANTED_GOTO_DISPATCH_LOCATION_ON_FOOT:
		case CPoliceOrder::PO_WANTED_GOTO_DISPATCH_LOCATION_AS_DRIVER:
		case CPoliceOrder::PO_WANTED_GOTO_DISPATCH_LOCATION_AS_PASSENGER:
		case CPoliceOrder::PO_WANTED_SEARCH_FOR_PED_ON_FOOT:
		case CPoliceOrder::PO_WANTED_ATTEMPT_ARREST:
		case CPoliceOrder::PO_WANTED_WAIT_AT_ROAD_BLOCK:
		{
			//These orders have no prerequisites.
			return true;
		}
		case CPoliceOrder::PO_WANTED_COMBAT:
		{
			//Check if we can lose our target.
			if(rPed.GetPedIntelligence()->GetCombatBehaviour().GetTargetLossResponse() != CCombatData::TLR_NeverLoseTarget)
			{
				//Ensure the targeting is valid.
				CPedTargetting* pTargeting = rPed.GetPedIntelligence()->GetTargetting(false);
				if(!pTargeting)
				{
					return false;
				}

				//Ensure the target info is valid.
				const CSingleTargetInfo* pTargetInfo = pTargeting->FindTarget(rOrder.GetIncident()->GetEntity());
				if(!pTargetInfo)
				{
					return false;
				}

				//Ensure the target whereabouts are known.
				if(!pTargetInfo->AreWhereaboutsKnown())
				{
					return false;
				}
			}
			
			return true;
		}
		case CPoliceOrder::PO_ARREST_GOTO_DISPATCH_LOCATION_AS_DRIVER:
		case CPoliceOrder::PO_WANTED_SEARCH_FOR_PED_IN_VEHICLE:
		case CPoliceOrder::PO_WANTED_WAIT_PULLED_OVER_AS_DRIVER:
		case CPoliceOrder::PO_WANTED_WAIT_CRUISING_AS_DRIVER:
		{
			//Ensure the ped is driving a vehicle.
			if(!rPed.GetIsDrivingVehicle())
			{
				return false;
			}
			
			return true;
		}
		case CPoliceOrder::PO_ARREST_GOTO_DISPATCH_LOCATION_AS_PASSENGER:
		case CPoliceOrder::PO_WANTED_WAIT_PULLED_OVER_AS_PASSENGER:
		case CPoliceOrder::PO_WANTED_WAIT_CRUISING_AS_PASSENGER:
		{
			//Ensure the ped is in a vehicle.
			CVehicle* pVehicle = rPed.GetVehiclePedInside();
			if(!pVehicle)
			{
				return false;
			}
			
			//Ensure the ped is not driving.
			if(pVehicle->IsDriver(&rPed))
			{
				return false;
			}
			
			return true;
		}
		default:
		{
			aiAssertf(false, "Unknown order: %d", rOrder.GetPoliceOrderType());
			return true;
		}
	}
}

void CPoliceAutomobileDispatch::RequestNewOrder(CPed& rPed, bool bCurrentOrderSucceeded)
{
	aiAssert(rPed.GetPedIntelligence()->GetOrder());
	aiAssert(rPed.GetPedIntelligence()->GetOrder()->IsPoliceOrder());
	CPoliceOrder* pPoliceOrder = static_cast<CPoliceOrder*>(rPed.GetPedIntelligence()->GetOrder());
	if(!pPoliceOrder)
	{
		return;
	}
	
	CIncident* pIncident = pPoliceOrder->GetIncident();
	CEntity* pIncidentEntity = pIncident ? pIncident->GetEntity() : NULL;
	CPed* pTargetPed = pIncidentEntity && pIncidentEntity->GetIsTypePed() ? static_cast<CPed*> (pIncidentEntity) : NULL;

	switch (pPoliceOrder->GetPoliceOrderType())
	{
		case CPoliceOrder::PO_WANTED_GOTO_DISPATCH_LOCATION_ON_FOOT:
		{
			//Check if the target is in a vehicle.
			if(pTargetPed && pTargetPed->GetIsInVehicle())
			{
				//Search in a vehicle.
				pPoliceOrder->SetPoliceOrderType(CPoliceOrder::PO_WANTED_SEARCH_FOR_PED_IN_VEHICLE);
			}
			else
			{
				//Search on foot.
				pPoliceOrder->SetPoliceOrderType(CPoliceOrder::PO_WANTED_SEARCH_FOR_PED_ON_FOOT);
			}
		}
		break;
		case CPoliceOrder::PO_WANTED_GOTO_DISPATCH_LOCATION_AS_DRIVER:
		{
			//Check if the target is in a vehicle.
			if(pTargetPed && pTargetPed->GetIsInVehicle())
			{
				//Search in a vehicle.
				pPoliceOrder->SetPoliceOrderType(CPoliceOrder::PO_WANTED_SEARCH_FOR_PED_IN_VEHICLE);
			}
			else
			{
				//Go to the dispatch location on foot.
				pPoliceOrder->SetPoliceOrderType(CPoliceOrder::PO_WANTED_GOTO_DISPATCH_LOCATION_ON_FOOT);
			}
		}
		break;
		case CPoliceOrder::PO_WANTED_GOTO_DISPATCH_LOCATION_AS_PASSENGER:
		{
			//Go to the dispatch location on foot.
			pPoliceOrder->SetPoliceOrderType(CPoliceOrder::PO_WANTED_GOTO_DISPATCH_LOCATION_ON_FOOT);
		}
		break;
		case CPoliceOrder::PO_WANTED_SEARCH_FOR_PED_IN_VEHICLE:
		{
			//Check if the current order succeeded.
			if(bCurrentOrderSucceeded)
			{
				//Check if the target is in a vehicle.
				if(pTargetPed && pTargetPed->GetIsInVehicle())
				{
					//Search in the vehicle.
					pPoliceOrder->SetPoliceOrderType(CPoliceOrder::PO_WANTED_SEARCH_FOR_PED_IN_VEHICLE);
				}
				else
				{
					//Search on foot.
					pPoliceOrder->SetPoliceOrderType(CPoliceOrder::PO_WANTED_SEARCH_FOR_PED_ON_FOOT);
				}
			}
			else
			{
				//Search on foot.
				pPoliceOrder->SetPoliceOrderType(CPoliceOrder::PO_WANTED_SEARCH_FOR_PED_ON_FOOT);
			}
		}
		break;
		case CPoliceOrder::PO_WANTED_SEARCH_FOR_PED_ON_FOOT:
		{
			//Search on foot.
			pPoliceOrder->SetPoliceOrderType(CPoliceOrder::PO_WANTED_SEARCH_FOR_PED_ON_FOOT);
		}
		break;
		case CPoliceOrder::PO_WANTED_ATTEMPT_ARREST:
		{
		}
		break;
		case CPoliceOrder::PO_WANTED_COMBAT:
		{
			//Check if the ped is in a vehicle.
			if(rPed.GetIsInVehicle())
			{
				//Search in vehicle.
				pPoliceOrder->SetPoliceOrderType(CPoliceOrder::PO_WANTED_SEARCH_FOR_PED_IN_VEHICLE);
			}
			else
			{
				//Search on foot.
				pPoliceOrder->SetPoliceOrderType(CPoliceOrder::PO_WANTED_SEARCH_FOR_PED_ON_FOOT);
			}
		}
		break;
		case CPoliceOrder::PO_WANTED_WAIT_PULLED_OVER_AS_DRIVER:
		case CPoliceOrder::PO_WANTED_WAIT_CRUISING_AS_DRIVER:
		{
			//Check if the current order succeeded.
			if(bCurrentOrderSucceeded)
			{
				//Enter combat.
				pPoliceOrder->SetPoliceOrderType(CPoliceOrder::PO_WANTED_COMBAT);
			}
			else
			{
				if(rPed.GetIsInVehicle())
				{
					pPoliceOrder->SetPoliceOrderType(CPoliceOrder::PO_WANTED_SEARCH_FOR_PED_IN_VEHICLE);
				}
				else
				{
					pPoliceOrder->SetPoliceOrderType(CPoliceOrder::PO_WANTED_SEARCH_FOR_PED_ON_FOOT);
				}
			}
		}
		break;
		case CPoliceOrder::PO_WANTED_WAIT_PULLED_OVER_AS_PASSENGER:
		case CPoliceOrder::PO_WANTED_WAIT_CRUISING_AS_PASSENGER:
		{
		}
		break;
		case CPoliceOrder::PO_WANTED_WAIT_AT_ROAD_BLOCK:
		{
			//Check if the current order succeeded.
			if(bCurrentOrderSucceeded)
			{
				//Enter combat.
				pPoliceOrder->SetPoliceOrderType(CPoliceOrder::PO_WANTED_COMBAT);
			}
			else
			{
				//Search for the ped on foot.
				pPoliceOrder->SetPoliceOrderType(CPoliceOrder::PO_WANTED_SEARCH_FOR_PED_ON_FOOT);
			}
		}
		break;
		case CPoliceOrder::PO_NONE:
		{
		}
		break;
		default:
		{
			aiAssertf(false, "Unknown order: %d", pPoliceOrder->GetPoliceOrderType());
		}
		break;
	}
	
	//Ensure the order is valid for the ped.
	if(!IsOrderValidForPed(*pPoliceOrder, rPed))
	{
		//The order is not valid for the ped in their current state.
		//Set the default order for the ped.
		if(rPed.GetIsInVehicle())
		{
			if(rPed.GetIsDrivingVehicle())
			{
				pPoliceOrder->SetPoliceOrderType(CPoliceOrder::PO_WANTED_SEARCH_FOR_PED_IN_VEHICLE);
			}
			else
			{
				pPoliceOrder->SetPoliceOrderType(CPoliceOrder::PO_WANTED_GOTO_DISPATCH_LOCATION_AS_PASSENGER);
			}
		}
		else
		{
			pPoliceOrder->SetPoliceOrderType(CPoliceOrder::PO_WANTED_SEARCH_FOR_PED_ON_FOOT);
		}
	}
}

#if __BANK

#define MAX_DISPATCH_DRAWABLES 100
CDebugDrawStore CPoliceAutomobileDispatch::ms_debugDraw(MAX_DISPATCH_DRAWABLES);

void CPoliceAutomobileDispatch::Debug() const
{ 
	ms_debugDraw.Render();

	// Render wanted debug
	CPed * pPlayerPed = CGameWorld::FindLocalPlayer();
	if (pPlayerPed)	
	{
		CWanted* pWanted = pPlayerPed->GetPlayerWanted();
		if (pWanted && pWanted->GetWantedLevel() > WANTED_CLEAN)
		{
			CVectorMap::DrawCircle(pWanted->m_LastSpottedByPolice, CDispatchData::GetWantedZoneRadius(pWanted->GetWantedLevel(), pPlayerPed), Color_red, false);
			CVectorMap::DrawCircle(pWanted->m_CurrentSearchAreaCentre, SEARCH_AREA_RADIUS, Color_orange, false);
			CVectorMap::DrawCircle(m_vLastDispatchLocation, 3.0f, Color_yellow, false);
			grcDebugDraw::Sphere(m_vLastDispatchLocation, 0.3f, Color_yellow);

			s32 iSize = COrderManager::GetInstance().GetMaxCount();
			for( s32 i = 0; i < iSize; i++ )
			{
				// Find orders attributed to this dispatcher
				COrder* pOrder = COrderManager::GetInstance().GetOrder(i);
				if( pOrder )
				{
					if( pOrder->GetDispatchType() == GetDispatchType() )
					{
						CPoliceOrder* pPoliceOrder = static_cast<CPoliceOrder*>(pOrder);
						Vector3 vDispatchLocation = pPoliceOrder->GetDispatchLocation();
						Vector3 vSearchLocation = pPoliceOrder->GetSearchLocation();

						if (pPoliceOrder->GetEntity())
						{
							Vector3 vPolicePedPos = VEC3V_TO_VECTOR3(pPoliceOrder->GetEntity()->GetTransform().GetPosition());

							// Draw the cops position on the vectormap
							CVectorMap::DrawCircle(vPolicePedPos, 2.0f, Color_blue);

							if (pPoliceOrder->GetPoliceOrderType() == CPoliceOrder::PO_WANTED_GOTO_DISPATCH_LOCATION_AS_DRIVER)
							{
								// Draw the vehicle dispatch location
								CVectorMap::DrawLine(vPolicePedPos, vDispatchLocation, Color_blue, Color_yellow);
								CVectorMap::DrawCircle(vDispatchLocation, 2.0f, Color_yellow);
							}

							if (pPoliceOrder->GetPoliceOrderType() == CPoliceOrder::PO_WANTED_SEARCH_FOR_PED_ON_FOOT)
							{
								// Draw the on foot search location
								CVectorMap::DrawLine(vPolicePedPos, vSearchLocation, Color_blue, Color_green);
								CVectorMap::DrawCircle(vSearchLocation, 2.0f, Color_green);
							}
						}
					}
				}
			}
		}
	}
}
#endif //__BANK

CVehicle* CPoliceAutomobileDispatch::SpawnNewVehicleWithOrders(CDispatchSpawnHelper& rSpawnHelper, const fwModelId iModelId, CPoliceOrder::ePoliceDispatchOrderType& nDriverOrder, CPoliceOrder::ePoliceDispatchOrderType& nPassengerOrder)
{
	//Check if we can wait in front.
	bool bCanWaitPulledOver	= CanWaitPulledOver(rSpawnHelper);
	bool bCanWaitCruising	= CanWaitCruising(rSpawnHelper);

	//Check if we should force a wait in front.
	bool bForceWaitInFront = false;
	if(bCanWaitPulledOver || bCanWaitCruising)
	{
		//Check if the chances are valid.
		float fChances = GetChancesToForceWaitInFront(rSpawnHelper);
		if(fwRandom::GetRandomNumberInRange(0.0f, 1.0f) < fChances)
		{
			//Check if nobody is waiting in front.
			if(!IsAnyoneWaitingInFront(rSpawnHelper))
			{
				bForceWaitInFront = true;
			}
		}
	}

	//Check if we should wait in front.
	bool bWaitPulledOver	= false;
	bool bWaitCruising		= false;
	if(bForceWaitInFront)
	{
		//Check if we can only wait pulled over.
		if(bCanWaitPulledOver && !bCanWaitCruising)
		{
			bWaitPulledOver = true;
		}
		//Check if we can only wait cruising.
		else if(!bCanWaitPulledOver && bCanWaitCruising)
		{
			bWaitCruising = true;
		}
		else
		{
			//Choose randomly.
			if(fwRandom::GetRandomTrueFalse())
			{
				bWaitPulledOver = true;
			}
			else
			{
				bWaitCruising = true;
			}
		}
	}
	else
	{
		//Check if we should wait pulled over.
		if(bCanWaitPulledOver)
		{
			//Get the chances.
			static dev_float s_fChances = 0.1f;
			float fChances = s_fChances;

			//Check if the chances are valid.
			if(fwRandom::GetRandomNumberInRange(0.0f, 1.0f) < fChances)
			{
				bWaitPulledOver = true;
			}
		}

		//Check if we should wait cruising.
		if(!bWaitPulledOver && bCanWaitCruising)
		{
			//Check if we are on the highway, and driving quickly.
			const CPed* pPed = rSpawnHelper.GetPed();
			bool bIsOnHighway = (pPed && pPed->IsPlayer() && pPed->GetPlayerInfo()->GetPlayerDataPlayerOnHighway());
			static dev_float s_fMinSpeed = 7.5f;
			bool bIsOnHighwayAndDrivingQuickly = (bIsOnHighway && pPed->GetIsInVehicle() &&
				(pPed->GetVehiclePedInside()->GetVelocity().XYMag2() > square(s_fMinSpeed)));

			//Get the chances.
			static dev_float s_fChances = 0.1f;
			float fChances = s_fChances;
			if(bIsOnHighwayAndDrivingQuickly)
			{
				static dev_float s_fChancesOnHighwayAndDrivingQuickly = 0.6f;
				fChances = s_fChancesOnHighwayAndDrivingQuickly;
			}
			else if(bIsOnHighway)
			{
				static dev_float s_fChancesOnHighway = 0.25f;
				fChances = s_fChancesOnHighway;
			}

			//Check if the chances are valid.
			if(fwRandom::GetRandomNumberInRange(0.0f, 1.0f) < fChances)
			{
				bWaitCruising = true;
			}
		}
	}

	//Assert that everything resolved correctly.
	aiAssert(bCanWaitPulledOver || !bWaitPulledOver);
	aiAssert(bCanWaitCruising	|| !bWaitCruising);
	aiAssert(!bWaitPulledOver	|| !bWaitCruising);

	//Check if we should wait pulled over.
	if(bWaitPulledOver)
	{
		//Spawn a vehicle in front.
		CVehicle* pVehicle = rSpawnHelper.SpawnVehicleInFront(iModelId, true);
		if(pVehicle)
		{
			//Assign the orders.
			nDriverOrder	= CPoliceOrder::PO_WANTED_WAIT_PULLED_OVER_AS_DRIVER;
			nPassengerOrder	= CPoliceOrder::PO_WANTED_WAIT_PULLED_OVER_AS_PASSENGER;

			return pVehicle;
		}
	}
	else if(bWaitCruising)
	{
		//Spawn a vehicle in front.
		CVehicle* pVehicle = rSpawnHelper.SpawnVehicleInFront(iModelId, false);
		if(pVehicle)
		{
			//Assign the orders.
			nDriverOrder	= CPoliceOrder::PO_WANTED_WAIT_CRUISING_AS_DRIVER;
			nPassengerOrder	= CPoliceOrder::PO_WANTED_WAIT_CRUISING_AS_PASSENGER;

			return pVehicle;
		}
	}

	//Generate a vehicle around the player.
	CVehicle* pVehicle = rSpawnHelper.SpawnVehicle(iModelId);
	if(pVehicle)
	{
		//Assign the orders.
		nDriverOrder	= CPoliceOrder::PO_WANTED_GOTO_DISPATCH_LOCATION_AS_DRIVER;
		nPassengerOrder	= CPoliceOrder::PO_WANTED_GOTO_DISPATCH_LOCATION_AS_PASSENGER;
		
		return pVehicle;
	}
	
	return NULL;
}

bool CPoliceAutomobileDispatch::CanWaitCruising(const CDispatchSpawnHelper& rSpawnHelper) const
{
#if __BANK
	TUNE_GROUP_BOOL(POLICE_AUTOMOBILE_DISPATCH, bDisableWaitCruising, false);
	if(bDisableWaitCruising)
	{
		return false;
	}
#endif

	//Ensure the ped is valid.
	const CPed* pPed = rSpawnHelper.GetPed();
	if(!pPed)
	{
		return false;
	}

	//Ensure the ped is a player.
	if(!pPed->IsPlayer())
	{
		return false;
	}

	//Ensure the ped is in a vehicle.
	const CVehicle* pVehicle = pPed->GetVehiclePedInside();
	if(!pVehicle)
	{
		return false;
	}

	//Ensure a vehicle can be spawned in front.
	if(!rSpawnHelper.CanSpawnVehicleInFront())
	{
		return false;
	}

	//Ensure the incident is valid.
	const CIncident* pIncident = rSpawnHelper.GetIncident();
	if(!pIncident)
	{
		return false;
	}

	//Check if the player is not on a highway.
	if(!pPed->GetPlayerInfo()->GetPlayerDataPlayerOnHighway())
	{
		//Ensure no one has the order.
		if(DoesAnyoneHaveOrder(CPoliceOrder::PO_WANTED_WAIT_CRUISING_AS_DRIVER, *pIncident))
		{
			return false;
		}
	}

	return true;
}

bool CPoliceAutomobileDispatch::CanWaitPulledOver(const CDispatchSpawnHelper& rSpawnHelper) const
{
#if __BANK
	TUNE_GROUP_BOOL(POLICE_AUTOMOBILE_DISPATCH, bDisableWaitPulledOver, false);
	if(bDisableWaitPulledOver)
	{
		return false;
	}
#endif

	//Ensure the ped is valid.
	const CPed* pPed = rSpawnHelper.GetPed();
	if(!pPed)
	{
		return false;
	}

	//Ensure the ped is a player.
	if(!pPed->IsPlayer())
	{
		return false;
	}

	//Ensure the ped is in a vehicle.
	const CVehicle* pVehicle = pPed->GetVehiclePedInside();
	if(!pVehicle)
	{
		return false;
	}

	//Ensure the player info is valid.
	CPlayerInfo* pPlayerInfo = pPed->GetPlayerInfo();
	if(!pPlayerInfo)
	{
		return false;
	}

	//Ensure the player is not on a highway.
	if(pPlayerInfo->GetPlayerDataPlayerOnHighway())
	{
#if __BANK
		TUNE_GROUP_BOOL(POLICE_AUTOMOBILE_DISPATCH, bDisableWaitPulledOverOnHighways, true);
		if(!bDisableWaitPulledOverOnHighways)
		{

		}
		else
#endif
		{
			return false;
		}
	}

	//Ensure a vehicle can be spawned in front.
	if(!rSpawnHelper.CanSpawnVehicleInFront())
	{
		return false;
	}

	//Ensure the incident is valid.
	const CIncident* pIncident = rSpawnHelper.GetIncident();
	if(!pIncident)
	{
		return false;
	}

	//Ensure no one has the order.
	if(DoesAnyoneHaveOrder(CPoliceOrder::PO_WANTED_WAIT_PULLED_OVER_AS_DRIVER, *pIncident))
	{
		return false;
	}

	return true;
}

bool CPoliceAutomobileDispatch::DoesAnyoneHaveOrder(int iPoliceOrderType, const CIncident& rIncident) const
{
	//Iterate over the orders.
	s32 iSize = COrderManager::GetInstance().GetMaxCount();
	for(s32 i = 0; i < iSize; i++ )
	{
		//Ensure the order is valid.
		COrder* pOrder = COrderManager::GetInstance().GetOrder(i);
		if(!pOrder)
		{
			continue;
		}

		//Ensure the order is locally controlled.
		if(!pOrder->IsLocallyControlled())
		{
			continue;
		}

		//Ensure the dispatch type matches.
		if(pOrder->GetDispatchType() != GetDispatchType())
		{
			continue;
		}

		//Ensure the order type is valid.
		if(!pOrder->IsPoliceOrder())
		{
			continue;
		}

		//Ensure the police order type matches.
		const CPoliceOrder* pPoliceOrder = static_cast<CPoliceOrder *>(pOrder);
		if(iPoliceOrderType != pPoliceOrder->GetPoliceOrderType())
		{
			continue;
		}

		//Ensure the incident is valid.
		const CIncident* pIncident = pOrder->GetIncident();
		if(!pIncident)
		{
			continue;
		}

		//Ensure the incident matches.
		if(&rIncident != pIncident)
		{
			continue;
		}

		return true;
	}

	return false;
}

Vec3V_Out CPoliceAutomobileDispatch::GenerateDispatchLocationForNewOrder(const CIncident& rIncident) const
{
	//Check if the last dispatch location is valid.
	if(m_uRunningFlags.IsFlagSet(RF_IsLastDispatchLocationValid))
	{
		return VECTOR3_TO_VEC3V(m_vLastDispatchLocation);
	}

	return GenerateDispatchLocationForIncident(rIncident);
}

float CPoliceAutomobileDispatch::GetChancesToForceWaitInFront(const CDispatchSpawnHelper& rSpawnHelper) const
{
	//Ensure the ped is a player.
	const CPed* pPed = rSpawnHelper.GetPed();
	if(pPed && pPed->IsPlayer())
	{
		return (pPed->GetPlayerWanted()->CalculateChancesToForceWaitInFront());
	}
	else
	{
		static dev_float s_fChances = 0.25f;
		return s_fChances;
	}
}

bool CPoliceAutomobileDispatch::IsAnyoneWaitingInFront(const CDispatchSpawnHelper& rSpawnHelper) const
{
	//Ensure the incident is valid.
	const CIncident* pIncident = rSpawnHelper.GetIncident();
	if(!pIncident)
	{
		return false;
	}

	//Check if anyone is waiting pulled over.
	if(DoesAnyoneHaveOrder(CPoliceOrder::PO_WANTED_WAIT_PULLED_OVER_AS_DRIVER, *pIncident))
	{
		return true;
	}

	//Check if anyone is waiting cruising.
	if(DoesAnyoneHaveOrder(CPoliceOrder::PO_WANTED_WAIT_CRUISING_AS_DRIVER, *pIncident))
	{
		return true;
	}

	return false;
}

void CPoliceAutomobileDispatch::UpdateOrders()
{
	//Iterate over the orders.
	s32 iSize = COrderManager::GetInstance().GetMaxCount();
	for(s32 i = 0; i < iSize; i++ )
	{
		//Ensure the order is valid.
		COrder* pOrder = COrderManager::GetInstance().GetOrder(i);
		if(!pOrder)
		{
			continue;
		}

		//Ensure the order is locally controlled.
		if(!pOrder->IsLocallyControlled())
		{
			continue;
		}

		//Ensure the dispatch type matches.
		if(pOrder->GetDispatchType() != GetDispatchType())
		{
			continue;
		}

		//Ensure the order type is valid.
		if(!pOrder->IsPoliceOrder())
		{
			continue;
		}

		//Ensure the incident is valid.
		const CIncident* pIncident = pOrder->GetIncident();
		if(!pIncident)
		{
			continue;
		}

		//Grab the police order.
		CPoliceOrder* pPoliceOrder = static_cast<CPoliceOrder *>(pOrder);

		//Ensure the entity is valid.
		CEntity* pEntity = pPoliceOrder->GetEntity();
		if(!pEntity)
		{
			continue;
		}

		//Ensure the entity is a ped.
		if(!pEntity->GetIsTypePed())
		{
			continue;
		}

		//Grab the police officer.
		CPed* pPoliceOfficer = static_cast<CPed *>(pEntity);

		//Ensure the officer is the driver.
		if(!pPoliceOfficer->GetIsDrivingVehicle())
		{
			continue;
		}

		//Just follow in bikes.
		if(pPoliceOfficer->GetVehiclePedInside() && pPoliceOfficer->GetVehiclePedInside()->InheritsFromBike())
		{
			pPoliceOfficer->GetPedIntelligence()->GetCombatBehaviour().ChangeFlag(CCombatData::BF_JustFollowInVehicle, true);
		}

		//Check if the dispatch location needs to be updated.
		Vector3 vDispatchLocation = pPoliceOrder->GetDispatchLocation();
		if(!pPoliceOrder->GetNeedsToUpdateLocation() && !CDispatchService::IsLocationWithinSearchArea(vDispatchLocation))
		{
			//Set the dispatch location.
			vDispatchLocation = VEC3V_TO_VECTOR3(GenerateDispatchLocationForIncident(*pIncident));
			pPoliceOrder->SetDispatchLocation(vDispatchLocation);

			//Note that the location needs to be updated.
			pPoliceOrder->SetNeedsToUpdateLocation(true);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// Helicopter dispatch services
//////////////////////////////////////////////////////////////////////////

#define MIN_POLICE_HELI_RESOURCES_NEEDED 3
#define MIN_SWAT_HELI_RESOURCES_NEEDED 5

#define HELI_DROP_OFF_MIN_RADIUS_FOR_TACTICAL_POINT 25.0f
#define HELI_DROP_OFF_MIN_RADIUS 30.0f
#define HELI_DROP_OFF_SEARCH_RADIUS_XY 50.0f
#define HELI_DROP_OFF_SEARCH_RADIUS_XY_FOR_TACTICAL_POINT 1.5f
#define HELI_DROP_OFF_SEARCH_DISTANCE_Z 15.0f
#define HELI_DROP_OFF_CLEAR_AREA_RADIUS 6.5f
#define HELI_RAPPEL_CLEAR_AREA_RADIUS 2.25f
#define HELI_DROP_OFF_DESIRED_HEIGHT_ABOVE_TARGET 20.0f
#define HELI_DROP_OFF_IN_AIR_CLEAR_AREA_RADIUS 9.25f

#define MAX_FAILED_CLEAR_AREA_SEARCHES 2
#define MIN_TIME_BETWEEN_CLEAR_AREA_SEARCHES 0.75f

CWantedHelicopterDispatch::Tunables CWantedHelicopterDispatch::sm_Tunables;
IMPLEMENT_DISPATCH_SERVICES_TUNABLES(CWantedHelicopterDispatch, 0x3ecdbaed);

bool CWantedHelicopterDispatch::StartTacticalPointClearAreaSearch(const CEntity* pIncidentEntity, Vector3& rLocation)
{
	// Failing to find a drop off location means we should try to find a rappel location (has a smaller radius required)
	if(m_aExcludedDropOffPositions.IsFull())
	{
		return false;
	}
	else if(m_aExcludedDropOffPositions.IsEmpty())
	{
		BuildExcludedOrderPositions();

		if(!m_bFailedToFindDropOffLocation)
		{
			// If we are starting the search to try and find a place, decide before hand if we will rappel or drop off
			m_bRappelFromDropOffLocation = fwRandom::GetRandomTrueFalse();
		}

		m_bFailedToFindDropOffLocation = false;
	}

	const CPed* pIncidentPed = static_cast<const CPed*>(pIncidentEntity);
	ScalarV scBestScore(V_FLT_LARGE_8);
	ScalarV scDesiredZ = pIncidentPed->GetTransform().GetPosition().GetZ() + ScalarV(LoadScalar32IntoScalarV(HELI_DROP_OFF_DESIRED_HEIGHT_ABOVE_TARGET));
	ScalarV scMinDistSq = LoadScalar32IntoScalarV(HELI_DROP_OFF_MIN_RADIUS_FOR_TACTICAL_POINT);
	scMinDistSq = (scMinDistSq * scMinDistSq);

	//Always rappel if target has flag set
	if (NetworkInterface::IsGameInProgress() && pIncidentPed->GetPedConfigFlag(CPED_CONFIG_FLAG_BlockDispatchedHelicoptersFromLanding))
	{
		m_bRappelFromDropOffLocation = true;
	}

	CTacticalAnalysis* pTacticalAnalysis = CTacticalAnalysis::Find(*pIncidentPed);
	if(pTacticalAnalysis)
	{
		Vec3V vTargetPosition = VECTOR3_TO_VEC3V(rLocation);
		bool bFoundTacticalPoint = false;

		CTacticalAnalysisNavMeshPoints& rNavMeshPoints = pTacticalAnalysis->GetNavMeshPoints();
		for(int i = 0; i < rNavMeshPoints.GetNumPoints(); i++)
		{
			const CTacticalAnalysisNavMeshPoint& rInfo = rNavMeshPoints.GetPointForIndex(i);
			if(!rInfo.IsValid() || rInfo.IsReserved())
			{
				continue;
			}

			if(!rInfo.IsLineOfSightClear())
			{
				continue;
			}

			if(rInfo.HasNearby())
			{
				continue;
			}

			Vec3V vPositionOnNavMesh = rInfo.GetPosition();
			if(IsPositionExcluded(vPositionOnNavMesh))
			{
				continue;
			}

			const ScalarV scDistanceToTargetSq = DistSquared(vPositionOnNavMesh, vTargetPosition);
			if(IsLessThanAll(scDistanceToTargetSq, scMinDistSq))
			{
				continue;
			}

			ScalarV scScore = (scDesiredZ - vPositionOnNavMesh.GetZ());
			if(IsGreaterThanAll(vPositionOnNavMesh.GetZ(), scDesiredZ))
			{
				scScore = ScalarV(V_FLT_LARGE_8);
			}

			if(!bFoundTacticalPoint || IsLessThanAll(scScore, scBestScore))
			{
				m_vLocationForDropOff = VEC3V_TO_VECTOR3(vPositionOnNavMesh);
				scBestScore = scScore;
				bFoundTacticalPoint = true;
			}
		}

		// Start the clear area search
		if(bFoundTacticalPoint)
		{
			// Make sure we know we are testing a tactical point
			m_bCheckingTacticalPointForDropOff = true;

			float fRadiusForClearAreaSearch = m_bRappelFromDropOffLocation ? HELI_RAPPEL_CLEAR_AREA_RADIUS : HELI_DROP_OFF_CLEAR_AREA_RADIUS;
			u32 iFlags = CNavmeshClearAreaHelper::Flag_AllowExterior;
			if(!m_bRappelFromDropOffLocation)
			{
				iFlags |= CNavmeshClearAreaHelper::Flag_TestForObjects;
			}

			m_NavMeshClearAreaHelper.StartClearAreaSearch(m_vLocationForDropOff, HELI_DROP_OFF_SEARCH_RADIUS_XY_FOR_TACTICAL_POINT, HELI_DROP_OFF_SEARCH_DISTANCE_Z,
														fRadiusForClearAreaSearch, iFlags, 0.0f);

			return true;
		}
	}

	return false;
}

bool CWantedHelicopterDispatch::GetLocationForDropOff(const CEntity* pIncidentEntity, Vector3& rLocation)
{
	// First check if our route helper is active
	if(!m_RouteSearchHelper.IsSearchActive())
	{
		// if route helper is not active check if the clear area search is active
		if(!m_NavMeshClearAreaHelper.IsSearchActive())
		{
			// We can only start a clear area search every so often, so make sure the timer has run out
			m_fTimeUntilNextClearAreaSearch -= fwTimer::GetTimeStep();
			if(m_fTimeUntilNextClearAreaSearch > 0.0f)
			{
				return false;
			}

			m_fTimeUntilNextClearAreaSearch = MIN_TIME_BETWEEN_CLEAR_AREA_SEARCHES;
			m_bCheckingTacticalPointForDropOff = false;

			// First try to start a clear area search for a tactical point. If that doesn't work fall back to just checking the nav mesh
			if(!StartTacticalPointClearAreaSearch(pIncidentEntity, rLocation))
			{
				if(m_bRappelFromDropOffLocation)
				{
					float fRadiusForClearAreaSearch = m_bRappelFromDropOffLocation ? HELI_RAPPEL_CLEAR_AREA_RADIUS : HELI_DROP_OFF_CLEAR_AREA_RADIUS;

					m_NavMeshClearAreaHelper.StartClearAreaSearch(rLocation, HELI_DROP_OFF_SEARCH_RADIUS_XY, HELI_DROP_OFF_SEARCH_DISTANCE_Z, fRadiusForClearAreaSearch,
						CNavmeshClearAreaHelper::Flag_AllowExterior|CNavmeshClearAreaHelper::Flag_TestForObjects, HELI_DROP_OFF_MIN_RADIUS);
				}
				else
				{
					// We went through our tactical points and couldn't find a drop off location, so try to find a rappel location
					m_aExcludedDropOffPositions.Reset();
					m_bRappelFromDropOffLocation = true;
					m_bFailedToFindDropOffLocation = true;
				}
			}

			return false;
		}
		else
		{
			// if the clear area search is active we need to check if the results are ready
			SearchStatus searchResult;
			Vector3 vTestedPosition = m_vLocationForDropOff;
			if(!m_NavMeshClearAreaHelper.GetSearchResults(searchResult, m_vLocationForDropOff))
			{
				return false;
			}

			// if the test was successful then we start a route search to see if the found position can reach the target
			if(searchResult == SS_SearchSuccessful && IsClearAreaPositionSafe(m_vLocationForDropOff))
			{
				if(m_bCheckingTacticalPointForDropOff)
				{
					return true;
				}

				m_RouteSearchHelper.StartSearch( NULL, m_vLocationForDropOff, rLocation, 0.25f, PATH_FLAG_NEVER_ENTER_WATER );
			}
			else if(m_bCheckingTacticalPointForDropOff)
			{
				// Add this point to the exclusion list because we don't want to test it again
				m_aExcludedDropOffPositions.Push(VECTOR3_TO_VEC3V(vTestedPosition));
			}
			else
			{
				// Increase the number of failed clear area searches
				m_iNumFailedClearAreaSearches++;
			}

			// make sure to reset the clear area search helper
			m_NavMeshClearAreaHelper.ResetSearch();

			return false;
		}
	}
	else
	{
		// When the route helper is active we need to check if the results are ready yet
		float fDistance = 0.0f;
		Vector3 vLastPoint(0.0f, 0.0f, 0.0f);
		SearchStatus eSearchStatus;

		if(m_RouteSearchHelper.GetSearchResults( eSearchStatus, fDistance, vLastPoint ))
		{
			// reset the search when the results are ready
			m_RouteSearchHelper.ResetSearch();

			// if we have a path found then we just return true
			if(eSearchStatus == SS_SearchSuccessful)
			{
				return IsClearAreaPositionSafe(m_vLocationForDropOff);
			}
			else
			{
				// Increase the number of failed clear area searches
				m_iNumFailedClearAreaSearches++;
			}
		}
	}

	return false;
}

void CWantedHelicopterDispatch::AssignNewOrders(CIncident& rIncident, const Resources& rResources)
{
	s32 iNumResouces = rResources.GetCount();
	if(iNumResouces == 0)
	{
		return;
	}
	
	Vector3 vDispatchLocation = rIncident.GetLocation();

	// I'd rather do the additional check on the resources than doing the get position for each entity.
	if(GetDispatchType() == DT_SwatHelicopter && m_bHasLocationForDropOff)
	{
		vDispatchLocation = m_vLocationForDropOff;
	}

	for(s32 iCurrentResource = 0; iCurrentResource < iNumResouces; iCurrentResource++)
	{
		CEntity* pEntity = rResources[iCurrentResource];
		if( pEntity && pEntity->GetIsTypePed())
		{
			const CSwatOrder* pDriverSwatOrder = NULL;
			CSwatOrder::eSwatDispatchOrderType eSwatOrder = CSwatOrder::SO_NONE;

			if (rIncident.GetType() == CIncident::IT_Wanted)
			{
				if (static_cast<CPed*>(pEntity)->GetIsDrivingVehicle())
				{
					eSwatOrder = m_bHasLocationForDropOff ?
						(m_bRappelFromDropOffLocation ? CSwatOrder::SO_WANTED_GOTO_ABSEILING_LOCATION_AS_DRIVER : CSwatOrder::SO_WANTED_GOTO_DROP_OFF_LOCATION_AS_DRIVER) :
						CSwatOrder::SO_WANTED_GOTO_STAGING_LOCATION_AS_DRIVER;
				}
				else 
				{
					CVehicle* pMyVehicle = static_cast<CPed*>(pEntity)->GetMyVehicle();
					if(pMyVehicle && pMyVehicle->GetDriver())
					{
						const COrder* pDriverOrder = pMyVehicle->GetDriver()->GetPedIntelligence()->GetOrder();
						if(pDriverOrder && pDriverOrder->GetType() == COrder::OT_Swat)
						{
							pDriverSwatOrder = static_cast<const CSwatOrder*>(pDriverOrder);
						}
					}

					if(pDriverSwatOrder)
					{
						CSwatOrder::eSwatDispatchOrderType driverOrderType = pDriverSwatOrder->GetSwatOrderType();
						if(driverOrderType == CSwatOrder::SO_WANTED_GOTO_ABSEILING_LOCATION_AS_DRIVER)
						{
							eSwatOrder = CSwatOrder::SO_WANTED_GOTO_ABSEILING_LOCATION_AS_PASSENGER;
						}
						else if(driverOrderType == CSwatOrder::SO_WANTED_GOTO_DROP_OFF_LOCATION_AS_DRIVER)
						{
							eSwatOrder = CSwatOrder::SO_WANTED_GOTO_DROP_OFF_LOCATION_AS_PASSENGER;
						}
						else
						{
							eSwatOrder = CSwatOrder::SO_WANTED_GOTO_STAGING_LOCATION_AS_PASSENGER;
						}
					}
					else
					{
						eSwatOrder = m_bHasLocationForDropOff ?
							(m_bRappelFromDropOffLocation ? CSwatOrder::SO_WANTED_GOTO_ABSEILING_LOCATION_AS_PASSENGER : CSwatOrder::SO_WANTED_GOTO_DROP_OFF_LOCATION_AS_PASSENGER) :
							CSwatOrder::SO_WANTED_GOTO_STAGING_LOCATION_AS_PASSENGER;
					}
				}
			}

			CSwatOrder order(GetDispatchType(), pEntity, &rIncident, eSwatOrder, vDispatchLocation, false);
			if(eSwatOrder == CSwatOrder::SO_WANTED_GOTO_ABSEILING_LOCATION_AS_PASSENGER || eSwatOrder == CSwatOrder::SO_WANTED_GOTO_DROP_OFF_LOCATION_AS_PASSENGER)
			{
				if(!static_cast<CPed*>(pEntity)->GetPedConfigFlag(CPED_CONFIG_FLAG_AdditionalRappellingPed))
				{
					order.SetAllowAdditionalPed(true);
				}

				if(pDriverSwatOrder)
				{
					order.SetStagingLocation(pDriverSwatOrder->GetStagingLocation());
				}
			}

			COrderManager::GetInstance().AddOrder(order);
		}
	}
}

bool CWantedHelicopterDispatch::IsActive() const
{
	if(!CDispatchServiceWanted::IsActive())
	{
		return false;
	}

	// It was requested (mostly for audio reasons?) that we don't spawn
	// new helicopters if we are freezing exterior vehicles.
	if(CGameWorld::GetFreezeExteriorVehicles())
	{
		return false;
	}

	return true;
}

void CWantedHelicopterDispatch::GetNumNetResourcesToReservePerIncident(u32 &numPeds, u32 &numVehicles, u32 &numObjects) const
{
	if (GetDispatchType() == DT_PoliceHelicopter)
	{
		numPeds	= MIN_POLICE_HELI_RESOURCES_NEEDED;
	}
	else
	{
		numPeds	= MIN_SWAT_HELI_RESOURCES_NEEDED;
	}

	numVehicles = 1;
	numObjects  = 0;
}

bool CWantedHelicopterDispatch::SpawnNewResources(CIncident& rIncident, Resources& rResources)
{
	// We don't want to spawn a heli until we need all 3 of the passengers
	// Currently the generate heli function doesn't support creating less than the pilot + 2 passengers
	int iResourcesNeeded = rResources.GetCapacity() - rResources.GetCount();
	bool bIsPoliceHeli = (GetDispatchType() == DT_PoliceHelicopter);
	int iMinResourcesNeeded = bIsPoliceHeli ? MIN_POLICE_HELI_RESOURCES_NEEDED : MIN_SWAT_HELI_RESOURCES_NEEDED;
	if( iResourcesNeeded < iMinResourcesNeeded)
	{
		return false;
	}

	const CEntity* pIncidentEntity = rIncident.GetEntity();
	const CPed* pIncidentPed = pIncidentEntity ? static_cast<const CPed*>(pIncidentEntity) : NULL;
	if (pIncidentPed)
	{
		if (pIncidentPed->GetPedConfigFlag(CPED_CONFIG_FLAG_DisableWantedHelicopterSpawning))
		{
			return false;
		}
	}

	if( bIsPoliceHeli && rIncident.GetType() == CIncident::IT_Wanted )
	{
		if(!pIncidentPed || !pIncidentPed->GetPlayerResetFlag(CPlayerResetFlags::PRF_DISABLE_DISPATCHED_HELI_DESTROYED_SPAWN_DELAY))
		{
			u32 uTimeLastPoliceHeliDestroyed = static_cast<CWantedIncident&>(rIncident).GetTimeLastPoliceHeliDestroyed();
			if( uTimeLastPoliceHeliDestroyed > 0 && (fwTimer::GetTimeInMilliseconds() - uTimeLastPoliceHeliDestroyed) < sm_Tunables.m_MinSpawnTimeForPoliceHeliAfterDestroyed )
			{
				return false;
			}
		}
	}

	// Make sure the assets have been streamed
	if((m_streamingRequests.GetRequestCount() <= 0) || !m_streamingRequests.HaveAllLoaded())
	{
		return false;
	}

	fwModelId iVehicleModelId;
	fwModelId iSelectedPedModelId;
	fwModelId iObjectModelId;
	if(!GetAndVerifyModelIds(iVehicleModelId, iSelectedPedModelId, iObjectModelId, MaxVehiclesInMap))
	{
		return false;
	}

	//Ensure the helis are not refueling.
	const CWanted* pWanted = GetIncidentPedWanted(rIncident);
	if(pWanted && pWanted->AreHelisRefueling())
	{
		return false;
	}

	// If this is a swat helicopter spawn attempt
	if(GetDispatchType() == DT_SwatHelicopter)
	{
		Vec3V vSearchOrigin;
		if(CanDropOff(vSearchOrigin))
		{
			// Check if we have a drop off location. If we don't then see if we've failed enough times to just go to the incident location
			m_bHasLocationForDropOff = GetLocationForDropOff(rIncident.GetEntity(), RC_VECTOR3(vSearchOrigin));
			if(!m_bHasLocationForDropOff && m_iNumFailedClearAreaSearches < MAX_FAILED_CLEAR_AREA_SEARCHES)
			{
				return false;
			}
		}
		else
		{
			m_bHasLocationForDropOff = false;
		}

		// Clear the number of clear area checks and the excluded tactical points
		m_iNumFailedClearAreaSearches = 0;
		m_aExcludedDropOffPositions.Reset();
		m_bFailedToFindDropOffLocation = false;
	}

	CHeli* pHeli = CVehiclePopulation::GenerateOneHeli(iVehicleModelId.GetModelIndex(), iSelectedPedModelId.GetModelIndex(), true);
	if(pHeli)
	{
		TUNE_GROUP_INT(SWAT_HELI_DISPATCH, iExtraRemovalTime, 30000, 0, 120000, 1000);
		pHeli->DelayedRemovalTimeReset(iExtraRemovalTime);

		pHeli->SetLiveryId(0);
		pHeli->SetLivery2Id(0);
		pHeli->UpdateBodyColourRemapping();
		pHeli->m_nVehicleFlags.bWasCreatedByDispatch = true;
		pHeli->m_nVehicleFlags.bCanEngineDegrade = false;

		if(bIsPoliceHeli)
		{
			pHeli->SetIsPoliceDispatched(true);
		}
		else if(GetDispatchType() == DT_SwatHelicopter)
		{
			pHeli->SetIsSwatDispatched(true);
		}

		//Add each ped to the resources.
		aiAssertf(pHeli->GetSeatManager()->CountPedsInSeats(true) <= iResourcesNeeded, "Too many peds were spawned in the heli.");
		for(s32 iSeat = 0; iSeat < pHeli->GetSeatManager()->GetMaxSeats(); ++iSeat)
		{
			//Ensure the ped is valid.
			CPed* pPed = pHeli->GetSeatManager()->GetPedInSeat(iSeat);
			if(!pPed)
			{
				continue;
			}

			// Dispatched law peds don't flee from combat
			pPed->GetPedIntelligence()->GetCombatBehaviour().SetFlag(CCombatData::BF_DisableFleeFromCombat);
			pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_CreatedByDispatch, true);

			//Add the ped.
			aiAssert(!pPed->GetPedIntelligence()->GetOrder());
			aiAssert(rResources.GetCapacity() != rResources.GetCount());
			rResources.Push(pPed);
		}

		//Add each ped to the group.
		s32 iPedGroupIndex = CPedGroups::AddGroup(POPTYPE_RANDOM_PATROL);
		if(aiVerifyf(iPedGroupIndex >= 0, "Invalid ped group index, group array is full?"))
		{
			//Grab the group.
			CPedGroup& rGroup = CPedGroups::ms_groups[iPedGroupIndex];

			rGroup.SetNeedsGroupEventScan(false);

			//The leader is the driver.
			s32 iLeaderSeat = 0;

			//Add the leader.
			CPed* pLeader = pHeli->GetSeatManager()->GetPedInSeat(iLeaderSeat);
			if(aiVerifyf(pLeader, "The leader is invalid -- no ped in seat: %d.", iLeaderSeat))
			{
				rGroup.GetGroupMembership()->SetLeader(pLeader);	
				rGroup.Process();
			}

			// Schedule additional peds - they will be added to the ped grounp in the callback for this dispatch
			for(int i = 2; i <= 3; i++)
			{
				if(CPedPopulation::SchedulePedInVehicle(pHeli, i, false, iSelectedPedModelId.GetModelIndex(), NULL, const_cast<CIncident*>(&rIncident), GetDispatchType(), iPedGroupIndex))
				{
					// set that we have peds scheduled - this keeps our helicopters from fleeing					
					//pHeli->AddScheduledOccupant(); // this is done inside SchedulePedInVehicle now
				}
			}			
		}

		g_AudioScannerManager.IncrementHelisInChase();
		g_AudioScannerManager.TriggerCopDispatchInteraction(SCANNER_CDIT_HELI_APPROACHING_DISPATCH);

		// Increment our current vehicle set and reset it to 0 if we've hit the max
		m_CurrVehicleSet++;

		return true;
	}

	return false;	
}

void CWantedHelicopterDispatch::DispatchDelayedSpawnCallback(CEntity* pEntity, CIncident* pIncident, s32 pedGroupIndex, COrder* UNUSED_PARAM(pOrder))
{
	if(pEntity)
	{
		CPed* pPed = static_cast<CPed*>(pEntity);
		if(pedGroupIndex >= 0)
		{
			//Grab the group.
			CPedGroup& rGroup = CPedGroups::ms_groups[pedGroupIndex];
			//B*1809612: Guard against adding the ped if it's already a member of the group
			if (!rGroup.GetGroupMembership()->IsMember(pPed))
			{
				//Add the follower.
				rGroup.GetGroupMembership()->AddFollower(pPed);
				rGroup.Process();
			}
		}		

		bool bIsPoliceHeli = (GetDispatchType() == DT_PoliceHelicopter);
		//Add the inventory load-out (?).
		static const atHashWithStringNotFinal COP_HELI_LOADOUT("LOADOUT_COP_HELI",0x47E9948C);
		static const atHashWithStringNotFinal SWAT_HELI_LOADOUT("LOADOUT_SWAT_NO_LASER",0xC9705531);
		CPedInventoryLoadOutManager::SetLoadOut(pPed, bIsPoliceHeli ? COP_HELI_LOADOUT.GetHash() : SWAT_HELI_LOADOUT.GetHash());

		CDispatchServiceWanted::DispatchDelayedSpawnCallback(pEntity, pIncident, pedGroupIndex);
	}
}

float CWantedHelicopterDispatch::GetTimeBetweenSpawnAttempts()
{
	return sm_Tunables.m_TimeBetweenSpawnAttempts;
}

bool CWantedHelicopterDispatch::GetIsEntityValidToBeAssignedOrder(CEntity* pEntity)
{
	//Ensure the entity is valid for the order.
	if(!GetIsEntityValidForOrder(pEntity))
	{
		return false;
	}

	//Assert that the entity is valid, and a ped.
	aiAssert(pEntity);
	aiAssert(pEntity->GetIsTypePed());

	//Grab the ped.
	const CPed* pPed = static_cast<CPed *>(pEntity);

	// Return true if this is a helicopter passenger or is using the abseiling order
	CVehicle* pVehicle = pPed->GetVehiclePedInside();
	if(!pVehicle || !pVehicle->InheritsFromHeli())
	{
		return false;
	}

	return true;
}

bool CWantedHelicopterDispatch::GetIsEntityValidForOrder( CEntity* pEntity )
{
	//Ensure the entity is valid for the order.
	if(!CDispatchServiceWanted::GetIsEntityValidForOrder(pEntity))
	{
		return false;
	}

	//Assert that the entity is valid, and a ped.
	aiAssert(pEntity);
	aiAssert(pEntity->GetIsTypePed());

	CPed* pPed = static_cast<CPed*>(pEntity);
	if( pPed->PopTypeIsRandom() && pPed->GetPedType() == PEDTYPE_SWAT )
	{
		return true;
	}

	return false;
}

bool CWantedHelicopterDispatch::CanDropOff(Vec3V_InOut vOrigin) const
{
	//Ensure the player is valid.
	CPed* pPlayer = CGameWorld::FindLocalPlayer();
	if(!pPlayer)
	{
		return false;
	}

	//Ensure the player is on foot.
	if(!pPlayer->GetIsOnFoot())
	{
		return false;
	}

	//Ensure the player is not in or around water.
	if(CPoliceBoatDispatch::IsInOrAroundWater(*pPlayer))
	{
		return false;
	}

	//Check if the weapon manager is valid.
	if(pPlayer->GetWeaponManager())
	{
		//Ensure the player's weapon is not scary.
		if(pPlayer->GetWeaponManager()->GetEquippedWeapon() &&
			pPlayer->GetWeaponManager()->GetEquippedWeapon()->GetWeaponInfo()->GetIsScary())
		{
			return false;
		}
	}

	//Set the origin.
	vOrigin = pPlayer->GetTransform().GetPosition();

	return true;
}

bool CWantedHelicopterDispatch::IsPositionExcluded(Vec3V_In vPosition) const
{
	ScalarV scMaxDistSq = ScalarV(V_TWO);

	//Iterate over the excluded positions.
	int iCount = m_aExcludedDropOffPositions.GetCount();
	for(int i = 0; i < iCount; ++i)
	{
		//Check if the position is close.
		ScalarV scDistSq = DistSquared(vPosition, m_aExcludedDropOffPositions[i]);
		if(IsLessThanAll(scDistSq, scMaxDistSq))
		{
			return true;
		}
	}

	// Check if any of our staging/order locations are already using this position
	if(IsPositionAlreadyInUse(vPosition))
	{
		return true;
	}

	return false;
}

bool CWantedHelicopterDispatch::IsPositionAlreadyInUse(Vec3V_In vPosition) const
{
	ScalarV scMaxDistSq = LoadScalar32IntoScalarV(HELI_DROP_OFF_IN_AIR_CLEAR_AREA_RADIUS * 2.0f);
	scMaxDistSq = (scMaxDistSq * scMaxDistSq);

	// Now check the cached order positions from other dispatched helicopters
	int iCount = m_aExcludedOrderPositions.GetCount();
	for(int i = 0; i < iCount; ++i)
	{
		ScalarV scDistSq = DistSquared(vPosition, m_aExcludedOrderPositions[i]);
		if(IsLessThanAll(scDistSq, scMaxDistSq))
		{
			return true;
		}
	}

	return false;
}

void CWantedHelicopterDispatch::BuildExcludedOrderPositions()
{
	m_aExcludedOrderPositions.Reset();

	//Iterate over the orders.
	s32 iSize = COrderManager::GetInstance().GetMaxCount();
	for(s32 i = 0; i < iSize && m_aExcludedOrderPositions.GetAvailable(); i++)
	{
		//Ensure the order is valid.
		COrder* pOrder = COrderManager::GetInstance().GetOrder(i);
		if(!pOrder)
		{
			continue;
		}

		//Ensure the dispatch type matches.
		if(pOrder->GetDispatchType() != GetDispatchType())
		{
			continue;
		}

		if(pOrder->GetType() == COrder::OT_Swat)
		{
			CSwatOrder* pSwatOrder = static_cast<CSwatOrder*>(pOrder);
			if( pSwatOrder->GetSwatOrderType() == CSwatOrder::SO_WANTED_GOTO_DROP_OFF_LOCATION_AS_DRIVER ||
				pSwatOrder->GetSwatOrderType() == CSwatOrder::SO_WANTED_GOTO_ABSEILING_LOCATION_AS_DRIVER )
			{
				Vec3V vStagingLocation = VECTOR3_TO_VEC3V(pSwatOrder->GetStagingLocation());
				m_aExcludedOrderPositions.Push(vStagingLocation);
			}
		}
	}
}

bool CWantedHelicopterDispatch::IsClearAreaPositionSafe(const Vector3& vPosition) const
{
	if(IsPositionAlreadyInUse(VECTOR3_TO_VEC3V(vPosition)))
	{
		return false;
	}

	Vector3 vInAirPosition(vPosition.x, vPosition.y, vPosition.z + 30.0f);

	// Do a sphere check at this point to make sure it's clear
	WorldProbe::CShapeTestSphereDesc sphereDesc;
	sphereDesc.SetSphere(vInAirPosition, HELI_DROP_OFF_IN_AIR_CLEAR_AREA_RADIUS);
	sphereDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES | ArchetypeFlags::GTA_VEHICLE_TYPE);
	sphereDesc.SetTypeFlags(ArchetypeFlags::GTA_AI_TEST);

	return !WorldProbe::GetShapeTestManager()->SubmitTest(sphereDesc);
}

//////////////////////////////////////////////////////////////////////////
// Swat helicopter dispatch service
//////////////////////////////////////////////////////////////////////////

void CSwatHelicopterDispatch::Update()
{
	// Call the base update first
	CDispatchService::Update();

	// If this dispatch is active we need to check if we should spawn the second wave of rappelling swat
	if(IsActive())
	{
		static dev_bool s_bUseAdditionalRappellingPeds = true;
		if(!s_bUseAdditionalRappellingPeds)
		{
			return;
		}

		// Only do this check every half second since we have to loop through the orders
		m_fAdditionalPedsTimer -= fwTimer::GetTimeStep();
		if( m_fAdditionalPedsTimer > 0.0f )
		{
			return;
		}

		m_fAdditionalPedsTimer = 0.5f;

		s32 iSize = COrderManager::GetInstance().GetMaxCount();
		for( s32 i = 0; i < iSize; i++ )
		{
			// Find orders attributed to this dispatcher and are of the correct type and will allow additional rappelling peds
			COrder* pOrder = COrderManager::GetInstance().GetOrder(i);
			if(!pOrder || !pOrder->IsLocallyControlled() || pOrder->GetDispatchType() != GetDispatchType() || pOrder->GetType() != COrder::OT_Swat)
			{
				continue;
			}

			CSwatOrder* pSwatOrder = static_cast<CSwatOrder*>(pOrder);
			if(!pSwatOrder->GetAllowAdditionalPed() || !pSwatOrder->GetIncident())
			{
				continue;
			}

			// If the order is assigned to an entity, make sure that entity is still valid
			CEntity* pEntity = pOrder->GetEntity();
			if(!pEntity || !pEntity->GetIsTypePed())
			{
				continue;
			}

			CIncident* pIncident = pSwatOrder->GetIncident();
			if(!pIncident || pIncident->GetAssignedResources(GetDispatchType()) >= pIncident->GetAllocatedResources(GetDispatchType()))
			{
				continue;
			}

			// Need to make sure the vehicle is a heli and our ped is running the correct task
			CPed* pPed = static_cast<CPed*>(pEntity);
			CVehicle* pVehicle = pPed->GetMyVehicle();
			if(!pVehicle || !pVehicle->InheritsFromHeli())
			{
				continue;
			}

			// Make sure the ped is in the correct state (don't want to spawn new guys on top of the existing ones) and the heli is not visible
			CTaskHeliPassengerRappel* pRappelTask = static_cast<CTaskHeliPassengerRappel*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_HELI_PASSENGER_RAPPEL));
			bool bCanSpawnPed = pRappelTask ? pRappelTask->GetState() == CTaskHeliPassengerRappel::State_Descend : !pPed->GetIsInVehicle();
			if(bCanSpawnPed)
			{
				CPedGroup* pPedGroup = pPed->GetPedsGroup();
				s32 iGroupIndex = pPedGroup ? pPedGroup->GetGroupIndex() : -1;
				s32 iSeatIndex = pRappelTask ? pRappelTask->GetSeatIndex() : pVehicle->GetSeatManager()->GetLastPedsSeatIndex(pPed);
				if(iSeatIndex >= 0 && SpawnAdditionalRappellingSwatPed(*pVehicle, iSeatIndex, pPed->GetModelId(), iGroupIndex, *pSwatOrder))
				{
					// if we succeeded then we don't want to spawn any more peds directly above this one
					pSwatOrder->SetAllowAdditionalPed(false);
				}
			}
		}
	}
}

bool CSwatHelicopterDispatch::SpawnAdditionalRappellingSwatPed(CVehicle& heli, int iSeatIndex, fwModelId pedModelId, s32 iGroupIndex, CSwatOrder& swatOrder)
{
	// Setup the order and task and update the incident
	CIncident* pIncident = swatOrder.GetIncident();
	if(pIncident)
	{
		if (!NetworkInterface::IsGameInProgress() || CNetworkObjectPopulationMgr::CanRegisterLocalObjectsOfType(NET_OBJ_TYPE_PED, 1, false))
		{
			// Try to schedule an extra ped, will return false if the scheduling fails (if queue is full)
			if(CPedPopulation::SchedulePedInVehicle(&heli, iSeatIndex, false, pedModelId.GetModelIndex(), NULL, pIncident, GetDispatchType(), iGroupIndex, &swatOrder))
			{
				// set that we have peds scheduled - this keeps our helicopters from fleeing					
				// heli.AddScheduledOccupant(); // this is done inside SchedulePedinVehicle now
				return true;
			}
		}
	}

	return false;
}

void CSwatHelicopterDispatch::DispatchDelayedSpawnCallback(CEntity* pEntity, CIncident* pIncident, s32 groupId, COrder* pOrder)
{
	// if we have a pOrder here, it's because we are spawning rappelling passengers
	if(pOrder && pEntity && pEntity->GetIsTypePed())
	{
		CPed* pPed = static_cast<CPed*>(pEntity);
		if(pPed->GetPedIntelligence()->GetOrder() == NULL)
		{
			pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_AdditionalRappellingPed, true);
		}
	}

	// rest of the setup is done here
	CWantedHelicopterDispatch::DispatchDelayedSpawnCallback(pEntity, pIncident, groupId, pOrder);
}

//////////////////////////////////////////////////////////////////////////

#if __BANK
void CFireDepartmentDispatch::AddExtraWidgets(bkBank *bank)
{
	bank->AddButton("Add Fire incident", CDispatchManager::AddFireIncident);
	bank->AddText("Spawn Timer",&m_fSpawnTimer,true);
}
#endif 

void CFireDepartmentDispatch::AssignNewOrders(CIncident& rIncident, const Resources& rResources)
{
	s32 iNumResouces = rResources.GetCount();
	for(s32 iCurrentResource = 0; iCurrentResource < iNumResouces; iCurrentResource++)
	{
		CEntity* pEntity = rResources[iCurrentResource];
		if( pEntity )
		{
			CFireOrder order(GetDispatchType(), pEntity, &rIncident);
			COrderManager::GetInstance().AddOrder(order);
		}
	}
}

bool CFireDepartmentDispatch::GetIsEntityValidForOrder( CEntity* pEntity )
{
	aiAssert(pEntity);
	if( !pEntity->GetIsTypePed() )
	{
		return false;
	}

	if(!CDispatchService::GetIsEntityValidForOrder(pEntity))
	{
		return false;
	}

	CPed* pPed = static_cast<CPed*>(pEntity);
	if( pPed->PopTypeIsRandom() )
	{
		// Return true if this is a firefighter
		// If they're in a vehicle, require that it be a firetruck
		// (I saw a firefighter who had been healed and was riding in an ambulance
		// get dispatched to a fire)
		CVehicle* pVehicle = pPed->GetVehiclePedInside();
		if( pPed->GetPedType() == PEDTYPE_FIRE && (!pVehicle || (u32)pVehicle->GetModelIndex() == MI_CAR_FIRETRUCK) )
		{
			return true;
		}
	}

	return false;
}

void CFireDepartmentDispatch::GetNumNetResourcesToReservePerIncident(u32 &numPeds, u32 &numVehicles, u32 &numObjects) const
{
	numPeds		= (s32)CFireIncident::NumPedsPerDispatch;
	numVehicles = 1;
	numObjects  = 0;
}

bool CFireDepartmentDispatch::SpawnNewResources(CIncident& rIncident, Resources& rResources)
{
	//If we aren't requesting the max number of firemen then it means there is already a dispatch in the world available to be 
	//used but with not enough firemen, so do not spawn a new fire truck unless the need is for it being full.

	// New incident requested => NumPedsPerDispatch firemen requested => PASS
	// Incident in progress, request for top up is in range 0 - NumPedsPerDispatch:
	//	 - 0 handled at the top
	//   - Unless there are no more left (in which case request will be for NumPedsPerDispatch), we still have people attending => FAIL
	//   - If there is no one left, we have a full request => PASS.  New full truck requested, but other one couldnt move anyway.
	int iResourcesNeeded = rResources.GetCapacity() - rResources.GetCount();
	if (iResourcesNeeded < (s32)CFireIncident::NumPedsPerDispatch)
	{
		return false;
	}

	// Make sure the assets have been streamed
	if((m_streamingRequests.GetRequestCount() <= 0) || !m_streamingRequests.HaveAllLoaded())
	{
		return false;
	}

    fwModelId iVehicleModelId;
    fwModelId iSelectedPedModelId;
    fwModelId iObjectModelId;
    if(!GetAndVerifyModelIds(iVehicleModelId, iSelectedPedModelId, iObjectModelId, MaxVehiclesInMap))
    {
        return false;
    }
    
	//Ensure the spawn helper is valid.
	CDispatchSpawnHelper* pSpawnHelper = ms_SpawnHelpers.FindSpawnHelperForIncident(rIncident);
	if(!pSpawnHelper)
	{
		return false;
	}

	//Ensure the spawn helper can spawn a vehicle.
	if(!pSpawnHelper->CanSpawnVehicle())
	{
		return false;
	}
	
	//Generate a vehicle around the player.
	CVehicle* pVehicle = pSpawnHelper->SpawnVehicle(iVehicleModelId, true);
	if(pVehicle)
	{	
		pVehicle->SetUpDriver(false);

		s32 iMaxSeats = pVehicle->GetSeatManager()->GetMaxSeats(); 

		s32 iPassengerSeats = Min(iMaxSeats,(s32)CFireIncident::NumPedsPerDispatch ) - 1;

		atFixedArray<u32,4> availableSeats;
		availableSeats.Append() = 5;
		availableSeats.Append() = 4;
		availableSeats.Append() = 1;

		for(s32 i = 0; i < iPassengerSeats; i++)
		{
			//Fill the ladder seats and then the front seat.
			const int iSeatIndex =  availableSeats[i];

			if (aiVerifyf(iSeatIndex > 0, "Invalid Seat Index %d", iSeatIndex))
			{
				const s32 iDirectEntryPointIndex = pVehicle->GetDirectEntryPointIndexForSeat(iSeatIndex);
				if (pVehicle->IsEntryIndexValid(iDirectEntryPointIndex))
				{
					// Don't allow entry to these seats in sp otherwise, unless marked to also be allowed in in sp
					if (!pVehicle->GetEntryInfo(iDirectEntryPointIndex)->GetMPWarpInOut() || pVehicle->GetEntryInfo(iDirectEntryPointIndex)->GetSPEntryAllowedAlso())
					{
						pVehicle->SetupPassenger(iSeatIndex, true, iSelectedPedModelId.GetModelIndex());
					}
				}
			}
		}	

		// For every ped spawned, mark a resource as being available
		for( s32 iSeat = 0; iSeat < pVehicle->GetSeatManager()->GetMaxSeats(); iSeat++ )
		{
			CPed* pPassenger = pVehicle->GetSeatManager()->GetPedInSeat(iSeat);
			if( pPassenger )
			{
				pPassenger->GetPedIntelligence()->GetCombatBehaviour().ClearFlag(CCombatData::BF_WillScanForDeadPeds);
                static const atHashWithStringNotFinal FIREMAN_LOADOUT("LOADOUT_FIREMAN",0x1FDC4A4C);
                CPedInventoryLoadOutManager::SetLoadOut(pPassenger, FIREMAN_LOADOUT.GetHash());

				//Add the passenger.
				aiAssert(!pPassenger->GetPedIntelligence()->GetOrder());
				aiAssert(rResources.GetCapacity() != rResources.GetCount());
				rResources.Push(pPassenger);
			}
		}
		return true;
	}

	SetSpawnTimer(2.0f);

	//Remove the incident if we run out of spawn attempts
	if(rIncident.GetType() == CIncident::IT_Fire)
	{
		CFireIncident* pFireIncident = static_cast<CFireIncident*>(&rIncident);
		pFireIncident->IncrementSpawnAttempts();
		if(pFireIncident->ExceededSpawnAttempts())
		{
			CIncidentManager::GetInstance().ClearIncident(pFireIncident);
		}
	}
	
	return false;
}

float CFireDepartmentDispatch::GetTimeBetweenSpawnAttempts()
{
	int iFireResponseDelayType = FIRE_RESPONSE_DELAY_MEDIUM;

	CPed* pLocalPlayerPed = CGameWorld::FindLocalPlayer();
	if(pLocalPlayerPed)
	{
		Vector3 vPlayerPosition = VEC3V_TO_VECTOR3(pLocalPlayerPed->GetTransform().GetPosition());
		CPopZone* pZone = CPopZones::FindSmallestForPosition(&vPlayerPosition, ZONECAT_ANY, NetworkInterface::IsGameInProgress() ? ZONETYPE_MP : ZONETYPE_SP);
		if(pZone)
		{
			// Using the lawResponse Delay but as long as FIRE_RESPONSE_DELAY_MEDIUM == LAW_RESPONSE_DELAY_MEDIUM it shouldn't be a problem.
			iFireResponseDelayType = pZone->m_lawResponseDelayType;
		}
	}

	return CDispatchData::GetRandomFireSpawnDelay(iFireResponseDelayType);
}

#if __BANK
void CSwatAutomobileDispatch::Debug() const
{ 
	s32 iSize = COrderManager::GetInstance().GetMaxCount();
	for( s32 i = 0; i < iSize; i++ )
	{
		// Find orders attributed to this dispatcher
		COrder* pOrder = COrderManager::GetInstance().GetOrder(i);
		if( pOrder )
		{
			if( pOrder->GetDispatchType() == GetDispatchType() )
			{
				if (pOrder->GetEntity())
				{
					Vector3 vSwatPedPos = VEC3V_TO_VECTOR3(pOrder->GetEntity()->GetTransform().GetPosition());

					// Draw the swat position on the vectormap
					CVectorMap::DrawCircle(vSwatPedPos, 2.0f, Color_red);
				}
			}
		}
	}
}

void CSwatAutomobileDispatch::AddExtraWidgets(bkBank * UNUSED_PARAM(bank))
{

}
#endif 

CSwatAutomobileDispatch::CSwatAutomobileDispatch()
: CDispatchServiceWanted(DT_SwatAutomobile)
, m_vLastDispatchLocation(Vector3::ZeroType)
, m_fTimeBetweenSearchPosUpdate(DEFAULT_TIME_BETWEEN_SEARCH_POS_UPDATES)
{

}

void CSwatAutomobileDispatch::AssignNewOrders(CIncident& rIncident, const Resources& rResources)
{
	s32 iNumResouces = rResources.GetCount();
	for(s32 iCurrentResource = 0; iCurrentResource < iNumResouces; iCurrentResource++)
	{
		CEntity* pEntity = rResources[iCurrentResource];
		if( pEntity && pEntity->GetIsTypePed())
		{
			CSwatOrder::eSwatDispatchOrderType eSwatOrder = CSwatOrder::SO_NONE;
			if (rIncident.GetType() == CIncident::IT_Wanted)
			{
				if (static_cast<CPed*>(pEntity)->GetIsDrivingVehicle())
				{
					eSwatOrder = CSwatOrder::SO_WANTED_GOTO_STAGING_LOCATION_AS_DRIVER;
				}
				else 
				{
					eSwatOrder = CSwatOrder::SO_WANTED_GOTO_STAGING_LOCATION_AS_PASSENGER;
				}
			}

			CSwatOrder order(GetDispatchType(), pEntity, &rIncident, eSwatOrder, m_vLastDispatchLocation, false);
			COrderManager::GetInstance().AddOrder(order);
		}
	}
}

void CSwatAutomobileDispatch::Update()
{
	CDispatchService::Update();

	if( IsActive() )
	{
		CDispatchService::UpdateCurrentSearchAreas(m_fTimeBetweenSearchPosUpdate);

		UpdateOrders();
	}
}

void CSwatAutomobileDispatch::UpdateOrders()
{
	//Iterate over the orders.
	s32 iSize = COrderManager::GetInstance().GetMaxCount();
	for(s32 i = 0; i < iSize; i++ )
	{
		//Ensure the order is valid.
		COrder* pOrder = COrderManager::GetInstance().GetOrder(i);
		if(!pOrder)
		{
			continue;
		}

		//Ensure the order is locally controlled.
		if(!pOrder->IsLocallyControlled())
		{
			continue;
		}

		//Ensure the dispatch type matches.
		if(pOrder->GetDispatchType() != GetDispatchType())
		{
			continue;
		}

		//Ensure the order type is valid.
		if(pOrder->GetType() != COrder::OT_Swat)
		{
			continue;
		}

		//Ensure the incident is valid.
		const CIncident* pIncident = pOrder->GetIncident();
		if(!pIncident)
		{
			continue;
		}

		//Grab the swat order.
		CSwatOrder* pSwatOrder = static_cast<CSwatOrder *>(pOrder);

		//Ensure the entity is valid.
		CEntity* pEntity = pSwatOrder->GetEntity();
		if(!pEntity)
		{
			continue;
		}

		//Ensure the entity is a ped.
		if(!pEntity->GetIsTypePed())
		{
			continue;
		}

		//Grab the swat member.
		CPed* pSwatMember = static_cast<CPed *>(pEntity);

		//Ensure the swat member is driving the vehicle.
		if(!pSwatMember->GetIsDrivingVehicle())
		{
			continue;
		}

		//Check if the staging location needs to be updated.
		Vector3 vStagingLocation = pSwatOrder->GetStagingLocation();
		if(!pSwatOrder->GetNeedsToUpdateLocation() && !CDispatchService::IsLocationWithinSearchArea(vStagingLocation))
		{
			//Set the staging location.
			vStagingLocation = VEC3V_TO_VECTOR3(GenerateDispatchLocationForIncident(*pIncident));
			pSwatOrder->SetStagingLocation(vStagingLocation);

			//Note that the location needs to be updated.
			pSwatOrder->SetNeedsToUpdateLocation(true);
		}
	}
}

#define MIN_SWAT_AUTOMOBILE_RESOURCES_NEEDED 3

void CSwatAutomobileDispatch::GetNumNetResourcesToReservePerIncident(u32 &numPeds, u32 &numVehicles, u32 &numObjects) const
{
	numPeds		= MIN_SWAT_AUTOMOBILE_RESOURCES_NEEDED;
	numVehicles = 1;
	numObjects  = 0;
}

bool CSwatAutomobileDispatch::SpawnNewResources(CIncident& rIncident, Resources& rResources)
{
	// If not enough resources are needed, we're done
	int iResourcesNeeded = rResources.GetCapacity() - rResources.GetCount();
	if( iResourcesNeeded < MIN_SWAT_AUTOMOBILE_RESOURCES_NEEDED )
	{
		return false;
	}

	// Make sure the assets have been streamed
	if((m_streamingRequests.GetRequestCount() <= 0) || !m_streamingRequests.HaveAllLoaded())
	{
		return false;
	}

	fwModelId iVehicleModelId;
	fwModelId iSelectedPedModelId;
	fwModelId iObjectModelId;
	if(!GetAndVerifyModelIds(iVehicleModelId, iSelectedPedModelId, iObjectModelId, MaxVehiclesInMap))
	{
		return false;
	}
	
	//Ensure the spawn helper is valid.
	CDispatchSpawnHelper* pSpawnHelper = ms_SpawnHelpers.FindSpawnHelperForIncident(rIncident);
	if(!pSpawnHelper)
	{
		return false;
	}

	//Ensure the spawn helper can spawn a vehicle.
	if(!pSpawnHelper->CanSpawnVehicle())
	{
		return false;
	}

	//Generate a random dispatch position.
	if(!pSpawnHelper->GenerateRandomDispatchPosition(RC_VEC3V(m_vLastDispatchLocation)))
	{
		return false;
	}
	
	s32 iPedGroupIndex = CPedGroups::AddGroup(POPTYPE_RANDOM_PATROL);
	aiAssertf(iPedGroupIndex >= 0, "Ped Group Index Invalid - group array is full?");

	//Generate a vehicle around the player.
	CVehicle* pVehicle = pSpawnHelper->SpawnVehicle(iVehicleModelId);
	if (pVehicle && CVehiclePopulation::AddSwatAutomobileOccupants(pVehicle, false, false, iSelectedPedModelId.GetModelIndex(), const_cast<CIncident*>(&rIncident), GetDispatchType(), iPedGroupIndex))
	{
		//Make sure the correct number of peds were spawned.
		aiAssertf(pVehicle->GetSeatManager()->CountPedsInSeats(true) <= MIN_SWAT_AUTOMOBILE_RESOURCES_NEEDED, "Too many peds were spawned in the vehicle.");

		TUNE_GROUP_INT(SWAT_AUTOMOBILE_DISPATCH, iExtraRemovalTime, 30000, 0, 120000, 1000);
		pVehicle->DelayedRemovalTimeReset(iExtraRemovalTime);
		pVehicle->m_nVehicleFlags.bWasCreatedByDispatch = true;
		
		//Add each ped to the resources.
		for(s32 iSeat = 0; iSeat < pVehicle->GetSeatManager()->GetMaxSeats(); ++iSeat)
		{
			//Ensure the ped is valid.
			CPed* pPed = pVehicle->GetSeatManager()->GetPedInSeat(iSeat);
			if(!pPed)
			{
				continue;
			}

			// Dispatched law peds don't flee from combat
			pPed->GetPedIntelligence()->GetCombatBehaviour().SetFlag(CCombatData::BF_DisableFleeFromCombat);
			pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_CreatedByDispatch, true);

			//Add the ped.
			aiAssert(!pPed->GetPedIntelligence()->GetOrder());
			aiAssert(rResources.GetCapacity() != rResources.GetCount());
			rResources.Push(pPed);
		}
		
		//Add each ped to the group.
		if(aiVerifyf(iPedGroupIndex >= 0, "Invalid ped group index, group array is full?"))
		{
			//Grab the group.
			CPedGroup& rGroup = CPedGroups::ms_groups[iPedGroupIndex];
			
			rGroup.SetNeedsGroupEventScan(false);

			//The leader is the driver.
			s32 iLeaderSeat = 0;
			
			//Add the leader.
			CPed* pLeader = pVehicle->GetSeatManager()->GetPedInSeat(iLeaderSeat);
			if(aiVerifyf(pLeader, "The leader is invalid -- no ped in seat: %d.", iLeaderSeat))
			{
				rGroup.GetGroupMembership()->SetLeader(pLeader);	
				rGroup.Process();
			}			
		}

		// Increment our current vehicle set and reset it to 0 if we've hit the max
		m_CurrVehicleSet++;
	}
	
	return true;
}

void CSwatAutomobileDispatch::DispatchDelayedSpawnCallback(CEntity* pEntity, CIncident* pIncident, s32 pedGroupIndex, COrder* UNUSED_PARAM(pOrder))
{
	if(pEntity)
	{
		if(pedGroupIndex >= 0 )
		{
			CPedGroup& rGroup = CPedGroups::ms_groups[pedGroupIndex];
			rGroup.GetGroupMembership()->AddFollower(static_cast<CPed*>(pEntity));
			rGroup.Process();
		}
		
		// Call Parent to add order for the ped and add a resource to the incident
		CDispatchServiceWanted::DispatchDelayedSpawnCallback(pEntity, pIncident, pedGroupIndex);
	}
	
}

bool CSwatAutomobileDispatch::GetIsEntityValidForOrder( CEntity* pEntity )
{
	//Ensure the entity is valid for the order.
	if(!CDispatchServiceWanted::GetIsEntityValidForOrder(pEntity))
	{
		return false;
	}

	//Assert that the entity is valid, and a ped.
	aiAssert(pEntity);
	aiAssert(pEntity->GetIsTypePed());

	CPed* pPed = static_cast<CPed*>(pEntity);
	if(pPed->PopTypeIsRandom() && (pPed->IsSwatPed() || pPed->IsArmyPed()))
	{
		CVehicle* pVehicle = pPed->GetVehiclePedInside();
		if(!pVehicle || pVehicle->GetVehicleType() == VEHICLE_TYPE_CAR)
		{
			return true;
		}
	}

	return false;
}


#if __BANK
void CAmbulanceDepartmentDispatch::AddExtraWidgets(bkBank *bank)
{
	bank->AddText("Spawn Timer",&m_fSpawnTimer,true);
}
#endif 


void CAmbulanceDepartmentDispatch::AssignNewOrders(CIncident& rIncident, const Resources& rResources)
{
	s32 iNumResouces = rResources.GetCount();
	for(s32 iCurrentResource = 0; iCurrentResource < iNumResouces; iCurrentResource++)
	{
		CEntity* pEntity = rResources[iCurrentResource];
		if( pEntity )
		{
			CAmbulanceOrder order(GetDispatchType(), pEntity, &rIncident);
			COrderManager::GetInstance().AddOrder(order);
		}
	}
}


bool CAmbulanceDepartmentDispatch::GetIsEntityValidForOrder( CEntity* pEntity )
{
	aiAssert(pEntity);
	if( !pEntity->GetIsTypePed() )
	{
		return false;
	}

	if(!CDispatchService::GetIsEntityValidForOrder(pEntity))
	{
		return false;
	}

	CPed* pPed = static_cast<CPed*>(pEntity);
	if( pPed->PopTypeIsRandom() )
	{
		// Return true if this is a paramedic
		CVehicle* pVehicle = pPed->GetVehiclePedInside();

		//allow medics to be used unless they are transporting a victim already
		if((!pVehicle || (pVehicle && !pVehicle->HasAliveVictimInIt())) && pPed->GetPedType() == PEDTYPE_MEDIC)
		{
			return true;
		}
	}

	return false;
}

void CAmbulanceDepartmentDispatch::GetNumNetResourcesToReservePerIncident(u32 &numPeds, u32 &numVehicles, u32 &numObjects) const
{
	numPeds		= (s32)CInjuryIncident::NumPedsPerDispatch;
	numVehicles = 1;
	numObjects  = 0;
}

bool CAmbulanceDepartmentDispatch::SpawnNewResources(CIncident& rIncident, Resources& rResources)
{
	int iResourcesNeeded = rResources.GetCapacity() - rResources.GetCount();

	// Make sure the assets have been streamed
	if((m_streamingRequests.GetRequestCount() <= 0) || !m_streamingRequests.HaveAllLoaded())
	{
		return false;
	}

    fwModelId iVehicleModelId;
    fwModelId iSelectedPedModelId;
    fwModelId iObjectModelId;
    if(!GetAndVerifyModelIds(iVehicleModelId, iSelectedPedModelId, iObjectModelId, MaxVehiclesInMap))
    {
        return false;
    }
    
	//Ensure the spawn helper is valid.
	CDispatchSpawnHelper* pSpawnHelper = ms_SpawnHelpers.FindSpawnHelperForIncident(rIncident);
	if(!pSpawnHelper)
	{
		return false;
	}

	//Ensure the spawn helper can spawn a vehicle.
	if(!pSpawnHelper->CanSpawnVehicle())
	{
		return false;
	}
	
	//Generate a vehicle around the player.
	CVehicle* pVehicle = pSpawnHelper->SpawnVehicle(iVehicleModelId, true);
	if(pVehicle)
	{	
		pVehicle->SetUpDriver(false);

		const s32 iNumPedsToSpawn = Min((s32)CInjuryIncident::NumPedsPerDispatch, iResourcesNeeded);

		for(s32 i = 1; i < iNumPedsToSpawn; i++)
		{
			pVehicle->SetupPassenger(i, true, iSelectedPedModelId.GetModelIndex());
		}

		// For every ped spawned, mark a resource as being available
		for( s32 iSeat = 0; iSeat < pVehicle->GetSeatManager()->GetMaxSeats(); iSeat++ )
		{
			CPed* pPassenger = pVehicle->GetSeatManager()->GetPedInSeat(iSeat);
			if( pPassenger )
			{
				pPassenger->GetPedIntelligence()->GetCombatBehaviour().ClearFlag(CCombatData::BF_WillScanForDeadPeds);

				//Add the passenger.
				aiAssert(!pPassenger->GetPedIntelligence()->GetOrder());
				aiAssert(rResources.GetCapacity() != rResources.GetCount());
				rResources.Push(pPassenger);
			}
		}
		return true;
	}

	SetSpawnTimer(2.0f);

	//Remove the incident if we run out of spawn attempts
	if(rIncident.GetType() == CIncident::IT_Injury)
	{
		CInjuryIncident* pInjuryIncident = static_cast<CInjuryIncident*>(&rIncident);
		pInjuryIncident->IncrementSpawnAttempts();
		if(pInjuryIncident->ExceededSpawnAttempts())
		{
			CIncidentManager::GetInstance().ClearIncident(pInjuryIncident);
		}
	}

	return false;
	
}


float CAmbulanceDepartmentDispatch::GetTimeBetweenSpawnAttempts()
{
	int iAmbulanceResponseDelayType = AMBULANCE_RESPONSE_DELAY_MEDIUM;

	CPed* pLocalPlayerPed = CGameWorld::FindLocalPlayer();
	if(pLocalPlayerPed)
	{
		Vector3 vPlayerPosition = VEC3V_TO_VECTOR3(pLocalPlayerPed->GetTransform().GetPosition());
		CPopZone* pZone = CPopZones::FindSmallestForPosition(&vPlayerPosition, ZONECAT_ANY, NetworkInterface::IsGameInProgress() ? ZONETYPE_MP : ZONETYPE_SP);
		if(pZone)
		{
			// Using the lawResponse Delay but as long as AMBULANCE_RESPONSE_DELAY_MEDIUM == LAW_RESPONSE_DELAY_MEDIUM it shouldn't be a problem.
			iAmbulanceResponseDelayType = pZone->m_lawResponseDelayType;
		}
	}

	return CDispatchData::GetRandomAmbulanceSpawnDelay(iAmbulanceResponseDelayType);
}


CPoliceRidersDispatch::CPoliceRidersDispatch()
		: CDispatchServiceWanted(DT_PoliceRiders)
{
}

void CPoliceRidersDispatch::Update()
{
	CDispatchService::Update();
}

bool CPoliceRidersDispatch::GetIsEntityValidForOrder(CEntity* pEntity)
{
	aiAssert(pEntity);
	if(!pEntity->GetIsTypePed())
	{
		return false;
	}

	if(!CDispatchService::GetIsEntityValidForOrder(pEntity))
	{
		return false;
	}

	CPed* pPed = static_cast<CPed*>(pEntity);

	if(!pPed->GetMyMount())
	{
		return false;
	}

	if(pPed->PopTypeIsRandom() && pPed->IsRegularCop())
	{
		return true;
	}

	return false;
}

void CPoliceRidersDispatch::GetNumNetResourcesToReservePerIncident(u32 &numPeds, u32 &numVehicles, u32 &numObjects) const
{
	CPed* pPlayerPed = CGameWorld::FindLocalPlayer();

	CWanted* pWanted = pPlayerPed->GetPlayerWanted();
	if(!pWanted)
	{
		return;
	}

	CWantedResponse* pWantedResponse = pWanted ? CDispatchData::GetWantedResponse(pWanted->GetWantedLevel()) : NULL;
	if(!pWantedResponse)
	{
		return;
	}

	const CDispatchResponse* pResp = pWantedResponse->GetDispatchServices(GetDispatchType());
	if(!pResp)
	{
		return;
	}

	numPeds		= pResp->GetNumPedsToSpawn()*2;
	numVehicles = 0;
	numObjects	= 0;
}

bool CPoliceRidersDispatch::SpawnNewResources(CIncident& /*rIncident*/, Resources& rResources)
{
	CPed* pPlayerPed = CGameWorld::FindLocalPlayer();
	if(!pPlayerPed)
	{
		return false;
	}

	CWanted* pWanted = pPlayerPed->GetPlayerWanted();
	if(!pWanted)
	{
		return false;
	}

	CWantedResponse* pWantedResponse = pWanted ? CDispatchData::GetWantedResponse(pWanted->GetWantedLevel()) : NULL;
	if(!pWantedResponse)
	{
		return false;
	}
	const CDispatchResponse* pResp = pWantedResponse->GetDispatchServices(GetDispatchType());
	if(!pResp)
	{
		return false;
	}

	// Not sure about this, but we probably want to spawn groups of riders,
	// not spawning in single additional riders.
	int iResourcesNeeded = rResources.GetCapacity() - rResources.GetCount();
	if(iResourcesNeeded < pResp->GetNumPedsToSpawn())
	{
		return false;
	}

	// Make sure the assets have been streamed
	if((m_streamingRequests.GetRequestCount() <= 0) || !m_streamingRequests.HaveAllLoaded())
	{
		return false;
	}

	// TODO: Improve this stuff.
	float fMinRadius = 80.0f;
	float fMaxRadius = 100.0f;
	float fHeight = 20.0f;
	float fRandomDistance = fwRandom::GetRandomNumberInRange(fMinRadius, fMaxRadius);
	float fRandomHeight = fwRandom::GetRandomNumberInRange(-fHeight, fHeight);
	float fRandomAngle = fwRandom::GetRandomNumberInRange(-PI, PI);

	Vector3 vRotate(0.0f, fRandomDistance, fRandomHeight);
	vRotate.RotateZ(fRandomAngle);

	// Not sure if we should use this position, rather than the player's current position or something.
	Vector3 vSearchPos = pWanted->m_CurrentSearchAreaCentre + vRotate;
	Vector3 vReturnPos;

	if(CPathServer::GetClosestPositionForPed(vSearchPos, vReturnPos, 40.0f) == ENoPositionFound)
	{
		return false;
	}


	// Setup the control type.
	const CControlledByInfo localAiControl(false, false);

	Matrix34 mtrx;
	mtrx.Identity();	// TODO: Set a better direction.
	mtrx.d = vReturnPos;

	fwModelId pedModelId;
	fwModelId horseModelId;
	fwModelId objModelId;
	if(!GetAndVerifyModelIds(horseModelId, pedModelId, objModelId, -1))
	{
		return false;
	}

	const int numToSpawn = iResourcesNeeded;
	for(int i = 0; i < numToSpawn; i++)
	{
		CPed* pPed = CPedFactory::GetFactory()->CreatePed(localAiControl, pedModelId, &mtrx, true, true, false);
		if(!pPed)
		{
			return false;
		}

		CPed* pHorse = CPedFactory::GetFactory()->CreatePed(localAiControl, horseModelId, &mtrx, true, true, false);

		// For now, fail an assert if something goes wrong when creating the horse. Not sure if there
		// are any valid cases where that could happen, as we should have checked earlier that we'll
		// be able to spawn and have a valid model.
		if(!Verifyf(pHorse, "Failed to create the horse, will destroy the rider."))
		{
			CPedFactory::GetFactory()->DestroyPed(pPed);

			return false;
		}

		// TODO: Probably remove this later (replace with assert?). It's useful for now
		// when we don't have actual law enforcement ped models set up.
		pPed->SetPedType(PEDTYPE_COP);

		pPed->PopTypeSet(POPTYPE_RANDOM_AMBIENT);
		pPed->SetDefaultDecisionMaker();
		pPed->SetCharParamsBasedOnManagerType();
		pPed->GetPedIntelligence()->SetDefaultRelationshipGroup();
		pPed->SetVarRandom(PV_FLAG_NONE, PV_FLAG_NONE, NULL, NULL, NULL, PV_RACE_UNIVERSAL);
		CGameWorld::Add(pPed, CGameWorld::OUTSIDE);

		pHorse->PopTypeSet(POPTYPE_RANDOM_AMBIENT);
		pHorse->SetDefaultDecisionMaker();
		pHorse->SetCharParamsBasedOnManagerType();
		pHorse->GetPedIntelligence()->SetDefaultRelationshipGroup();
		CGameWorld::Add(pHorse, CGameWorld::OUTSIDE);

		pPed->SetPedOnMount(pHorse);

		// One horse/rider pair counts as one "resource" here.
		aiAssert(!pPed->GetPedIntelligence()->GetOrder());
		aiAssert(rResources.GetCapacity() != rResources.GetCount());
		rResources.Push(pPed);

		CTask* pTask = rage_new CTaskCombat(pPlayerPed);
		CEventGivePedTask event(PED_TASK_PRIORITY_EVENT_RESPONSE_NONTEMP, pTask);
		pPed->GetPedIntelligence()->AddEvent(event);

		// Dispatched law peds don't flee from combat
		pPed->GetPedIntelligence()->GetCombatBehaviour().SetFlag(CCombatData::BF_DisableFleeFromCombat);
		pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_CreatedByDispatch, true);

		float separation = 1.0f;	// MAGIC!
		mtrx.d.AddScaled(mtrx.a, separation);

		// Try to give the rider and horse some time before they can be destroyed.
		// Note: I think this may not always be respected, if they get seen and then
		// unseen or something like that.
		// TODO: Think more about cleanup of horse riders. We probably never want the
		// horse to be destroyed without the rider, or the other way around.
		u32 extraTimeMs = 30*1000;	// MAGIC!
		pPed->DelayedRemovalTimeReset(extraTimeMs);
		pHorse->DelayedRemovalTimeReset(extraTimeMs);
	}

	return true;
}

float CPoliceRidersDispatch::GetTimeBetweenSpawnAttempts()
{
	// Not sure about this.
	return 10.0f;
}

#if __BANK
#define MAX_DISPATCH_DRAWABLES 100
CDebugDrawStore CPoliceVehicleRequest::ms_debugDraw(MAX_DISPATCH_DRAWABLES);

void CPoliceVehicleRequest::AddExtraWidgets( bkBank * UNUSED_PARAM(bank) )
{

}

void CPoliceVehicleRequest::Debug() const
{ 
	ms_debugDraw.Render();

	// Render wanted debug
	CPed * pPlayerPed = CGameWorld::FindLocalPlayer();
	if (pPlayerPed)	
	{
		//CVectorMap::DrawCircle(pWanted->m_LastSpottedByPolice, CDispatchData::GetWantedZoneRadius(pWanted->GetWantedLevel(), pPlayerPed), Color_red, false);
		//CVectorMap::DrawCircle(pWanted->m_CurrentSearchAreaCentre, SEARCH_AREA_RADIUS, Color_orange, false);
		//CVectorMap::DrawCircle(m_vLastDispatchLocation, 3.0f, Color_yellow, false);
		//grcDebugDraw::Sphere(m_vLastDispatchLocation, 0.3f, Color_yellow);

		s32 iSize = COrderManager::GetInstance().GetMaxCount();
		for( s32 i = 0; i < iSize; i++ )
		{
			// Find orders attributed to this dispatcher
			COrder* pOrder = COrderManager::GetInstance().GetOrder(i);
			if( pOrder )
			{
				if( pOrder->GetDispatchType() == GetDispatchType() )
				{
					CPoliceOrder* pPoliceOrder = static_cast<CPoliceOrder*>(pOrder);
					Vector3 vDispatchLocation = pPoliceOrder->GetDispatchLocation();
					// Vector3 vSearchLocation = pPoliceOrder->GetSearchLocation();

					if (pPoliceOrder->GetEntity())
					{
						Vector3 vPolicePedPos = VEC3V_TO_VECTOR3(pPoliceOrder->GetEntity()->GetTransform().GetPosition());

						// Draw the cops position on the vectormap
						CVectorMap::DrawCircle(vPolicePedPos, 2.0f, Color_blue);

						if (pPoliceOrder->GetPoliceOrderType() == CPoliceOrder::PO_ARREST_GOTO_DISPATCH_LOCATION_AS_DRIVER )
						{
							// Draw the vehicle dispatch location
							CVectorMap::DrawLine(vPolicePedPos, vDispatchLocation, Color_blue, Color_yellow);
							CVectorMap::DrawCircle(vDispatchLocation, 2.0f, Color_yellow);
						}
					}
				}
			}
		}
	}
}
#endif //__BANK

void CPoliceVehicleRequest::StreamModels()
{
	if( m_streamingRequests.GetRequestCount() == 0 )
	{
		StreamWantedIncidentModels();
	}

	// This needs to be called to add refs to the requests.
	m_streamingRequests.HaveAllLoaded();
}

void CPoliceVehicleRequest::Update()
{
	//Call the base class version.
	CDispatchService::Update();

	//Clear the last dispatch location.
	m_vLastDispatchLocation.Zero();
}

// PURPOSE: Wanted services call this function to stream in the models they use as there is specific behavior to do so
void CPoliceVehicleRequest::StreamWantedIncidentModels()
{
	// Clear out our vehicle sets array pointer for now
	m_pDispatchVehicleSets = NULL;

	//Request vehicles
	RequestAllModels(CDispatchData::GetEmergencyResponse(GetDispatchType()));
}

void CPoliceVehicleRequest::AssignNewOrders(CIncident& rIncident, const Resources& rResources)
{
	s32 iNumResouces = rResources.GetCount();
	for(s32 iCurrentResource = 0; iCurrentResource < iNumResouces; iCurrentResource++)
	{
		CEntity* pEntity = rResources[iCurrentResource];
		if( pEntity )
		{
			CPoliceOrder::ePoliceDispatchOrderType ePoliceOrder = CPoliceOrder::PO_NONE;
			if ( rIncident.GetType() == CIncident::IT_Arrest )
			{
				if (static_cast<CPed*>(pEntity)->GetIsDrivingVehicle())
				{
					ePoliceOrder = CPoliceOrder::PO_ARREST_GOTO_DISPATCH_LOCATION_AS_DRIVER;
				}
				else 
				{
					ePoliceOrder = CPoliceOrder::PO_ARREST_GOTO_DISPATCH_LOCATION_AS_PASSENGER;
				}
			}

			//Generate the dispatch location.
			Vector3 vDispatchLocation = (m_vLastDispatchLocation.IsNonZero() ? m_vLastDispatchLocation :
				VEC3V_TO_VECTOR3(GenerateDispatchLocationForIncident(rIncident)));

			CPoliceOrder order(GetDispatchType(), pEntity, &rIncident, ePoliceOrder, vDispatchLocation);
			COrderManager::GetInstance().AddOrder(order);
		}
	}
}

void CPoliceVehicleRequest::RequestNewOrder(CPed* pPed, bool bCurrentOrderSucceeded)
{
	aiAssert(pPed && pPed->GetPedIntelligence()->GetOrder());
	CPoliceOrder* pPoliceOrder = static_cast<CPoliceOrder*>(pPed->GetPedIntelligence()->GetOrder());
	if (pPoliceOrder)
	{
		aiAssertf(pPoliceOrder->GetDispatchType() == DT_PoliceVehicleRequest, "CPoliceVehicleRequest::RequestNewOrder incorrect dispatch type");

		switch (pPoliceOrder->GetPoliceOrderType())
		{
			case CPoliceOrder::PO_ARREST_GOTO_DISPATCH_LOCATION_AS_DRIVER: 
			case CPoliceOrder::PO_ARREST_GOTO_DISPATCH_LOCATION_AS_PASSENGER: 
			{
				if (bCurrentOrderSucceeded)
				{
					if (pPed->GetIsDrivingVehicle())
					{
						pPoliceOrder->SetPoliceOrderType(CPoliceOrder::PO_ARREST_WAIT_AT_DISPATCH_LOCATION_AS_DRIVER);
					}
					else
					{
						pPoliceOrder->SetPoliceOrderType(CPoliceOrder::PO_ARREST_WAIT_AT_DISPATCH_LOCATION_AS_PASSENGER);
					}
				}
				break;
			}
			
			default: break;
		}
	}
}

void CPoliceVehicleRequest::GetNumNetResourcesToReservePerIncident(u32 &numPeds, u32 &numVehicles, u32 &numObjects) const
{
	numPeds		= MIN_POLICE_AUTOMOBILE_RESOURCES_NEEDED;
	numVehicles = 1;
	numObjects  = 0;
}

bool CPoliceVehicleRequest::SpawnNewResources(CIncident& rIncident, Resources& rResources)
{
	// Make sure the assets have been streamed
	if((m_streamingRequests.GetRequestCount() <= 0) || !m_streamingRequests.HaveAllLoaded())
	{
		return false;
	}

	fwModelId iVehicleModelId;
	fwModelId iSelectedPedModelId;
	fwModelId iObjectModelId;
	if(!GetAndVerifyModelIds(iVehicleModelId, iSelectedPedModelId, iObjectModelId, MaxVehiclesInMap))
	{
		return false;
	}
	
	//Ensure the spawn helper is valid.
	CDispatchSpawnHelper* pSpawnHelper = ms_SpawnHelpers.FindSpawnHelperForIncident(rIncident);
	if(!pSpawnHelper)
	{
		return false;
	}

	//Ensure the spawn helper can spawn a vehicle.
	if(!pSpawnHelper->CanSpawnVehicle())
	{
		return false;
	}

	//Generate a random dispatch position.
	if(!pSpawnHelper->GenerateRandomDispatchPosition(RC_VEC3V(m_vLastDispatchLocation)))
	{
		return false;
	}

	int iResourcesNeeded = rResources.GetCapacity() - rResources.GetCount();

	// If we decided on a car we need to make sure we need at least the min number of police car resources
	// Bikes don't matter because they only need one ped and are handled by the first resources needed check
	CVehicleModelInfo *pModelInfo = (CVehicleModelInfo *)CModelInfo::GetBaseModelInfo(iVehicleModelId);
	if(pModelInfo)
	{
		if(!pModelInfo->GetIsBike() && iResourcesNeeded < MIN_POLICE_AUTOMOBILE_RESOURCES_NEEDED)
		{
			// Increment our current vehicle set so we can try and find a bike (as this failure implies we only need 1 resource)
			m_CurrVehicleSet++;
			return false;
		}
	}
	
	//Generate a vehicle around the player.
	CVehicle* pVehicle = pSpawnHelper->SpawnVehicle(iVehicleModelId);
	if(pVehicle)
	{
		TUNE_GROUP_INT(POLICE_VEHICLE_REQUEST_DISPATCH, iExtraRemovalTime, 30000, 0, 120000, 1000);
		pVehicle->DelayedRemovalTimeReset(iExtraRemovalTime);
		pVehicle->m_nVehicleFlags.bWasCreatedByDispatch = true;

		CVehiclePopulation::AddPoliceVehOccupants(pVehicle, true, false, iSelectedPedModelId.GetModelIndex(), const_cast<CIncident*>(&rIncident), GetDispatchType());

		for( s32 iSeat = 0; iSeat < pVehicle->GetSeatManager()->GetMaxSeats(); iSeat++ )
		{
			CPed* pPassenger = pVehicle->GetSeatManager()->GetPedInSeat(iSeat);
			if( pPassenger )
			{
				// Dispatched law peds don't flee from combat
				pPassenger->GetPedIntelligence()->GetCombatBehaviour().SetFlag(CCombatData::BF_DisableFleeFromCombat);
				pPassenger->SetPedConfigFlag(CPED_CONFIG_FLAG_CreatedByDispatch, true);

				//Add the passenger.
				aiAssert(!pPassenger->GetPedIntelligence()->GetOrder());
				aiAssert(rResources.GetCapacity() != rResources.GetCount());
				rResources.Push(pPassenger);
			}
		}

		// Increment our current vehicle set and reset it to 0 if we've hit the max
		m_CurrVehicleSet++;
	}
	return true;
}

bool CPoliceVehicleRequest::GetIsEntityValidForOrder( CEntity* pEntity )
{
	aiAssert(pEntity);

	if( !pEntity->GetIsTypePed() )
	{
		return false;
	}

	if(!CDispatchService::GetIsEntityValidForOrder(pEntity))
	{
		return false;
	}

	CPed* pPed = static_cast<CPed*>(pEntity);
	if( pPed->PopTypeIsRandom() && pPed->IsRegularCop() )
	{
		return true;
	}

	return false;
}

#if __BANK
static strRequestArray<4> s_aRoadBlockStreamingRequests;
#endif

CPoliceRoadBlockDispatch::CPoliceRoadBlockDispatch()
: CDispatchServiceWanted(DT_PoliceRoadBlock)
{}

bool CPoliceRoadBlockDispatch::GetIsEntityValidForOrder(CEntity* pEntity)
{
	aiAssert(pEntity);
	if(!pEntity->GetIsTypePed())
	{
		return false;
	}

	if(!CDispatchServiceWanted::GetIsEntityValidForOrder(pEntity))
	{
		return false;
	}

	CPed* pPed = static_cast<CPed*>(pEntity);
	if(pPed->PopTypeIsRandom() && pPed->IsRegularCop())
	{
		return true;
	}

	return false;
}


void CPoliceRoadBlockDispatch::GetNumNetResourcesToReservePerIncident(u32 &numPeds, u32 &numVehicles, u32 &numObjects) const
{
	// roadblocks currently don't use the reservation system
	numPeds		= 0;
	numVehicles = 0;
	numObjects	= 0;
}

bool CPoliceRoadBlockDispatch::SpawnNewResources(CIncident& rIncident, Resources& rResources)
{
	int iResourcesNeeded = rResources.GetCapacity() - rResources.GetCount();
	
	//Ensure the spawn helper is valid.
	CDispatchSpawnHelper* pSpawnHelper = ms_SpawnHelpers.FindSpawnHelperForIncident(rIncident);
	if(!pSpawnHelper)
	{
		return false;
	}

	//Ensure the ped is valid.
	const CPed* pPed = pSpawnHelper->GetPed();
	if(!pPed)
	{
		return false;
	}

	//Ensure the ped is wanted.
	CWanted* pWanted = pPed->GetPlayerWanted();
	if(!pWanted)
	{
		return false;
	}
	
	//Ensure the ped is in a vehicle.
	CVehicle* pVehicle = pPed->GetVehiclePedInside();
	if(!pVehicle)
	{
		return false;
	}

	if (NetworkInterface::IsGameInProgress() && pPed->IsPlayer() && pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_HasEstablishedDecoy))
	{
		return false;
	}
	
	//Ensure the wanted response is valid.
	CWantedResponse* pWantedResponse = pWanted ? CDispatchData::GetWantedResponse(pWanted->GetWantedLevel()) : NULL;
	if(!pWantedResponse)
	{
		return false;
	}
	
	//Ensure the dispatch response is valid.
	const CDispatchResponse* pResp = pWantedResponse->GetDispatchServices(GetDispatchType());
	if(!pResp)
	{
		return false;
	}

	//Ensure everything has been streamed.
	if((m_streamingRequests.GetRequestCount() <= 0) || !m_streamingRequests.HaveAllLoaded())
	{
		return false;
	}
	
	//Grab the vehicle position.
	Vector3 vVehiclePosition = VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetPosition());
	
	//Grab the vehicle velocity.
	const Vector3& vVehicleVelocity = pVehicle->GetVelocity();
	
	//Grab the vehicle's forward vector.
	Vector3 vVehicleForward = VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetForward());
	
	//Calculate the vehicle direction.
	Vector3 vVehicleDirection = vVehicleVelocity;
	vVehicleDirection.NormalizeSafe(vVehicleForward);
	
	//Generate the input arguments for the road node search.
	CPathFind::FindNodeInDirectionInput roadNodeInput(vVehiclePosition, vVehicleDirection, pSpawnHelper->GetIdealSpawnDistance());
	
	//Follow the road direction.
	roadNodeInput.m_bFollowRoad = true;
	
	//Road blocks do not care which way traffic is going.
	roadNodeInput.m_bCanFollowOutgoingLinks = true;
	roadNodeInput.m_bCanFollowIncomingLinks = true;
	
	//Find a node in the road direction.
	CPathFind::FindNodeInDirectionOutput roadNodeOutput;
	if(!ThePaths.FindNodeInDirection(roadNodeInput, roadNodeOutput))
	{
		return false;
	}
	
	//Ensure the node is valid.
	const CPathNode* pNode = ThePaths.FindNodePointerSafe(roadNodeOutput.m_Node);
	if(!pNode)
	{
		return false;
	}
	
	//Grab the node position.
	Vector3 vNodePosition;
	pNode->GetCoors(vNodePosition);
	
	//Generate a vector from the vehicle to the node.
	Vector3 vVehicleToNode = vNodePosition - vVehiclePosition;
	
	//Generate the direction from the vehicle to the node.
	Vector3 vVehicleToNodeDirection = vVehicleToNode;
	if(!vVehicleToNodeDirection.NormalizeSafeRet())
	{
		return false;
	}
	
	//Tune the min dot.
	TUNE_GROUP_FLOAT(POLICE_ROAD_BLOCK, fMinDot, 0.707f, -1.0f, 1.0f, 0.01f);
	
	//Ensure the node is generally in the vehicle direction.
	float fDot = vVehicleDirection.Dot(vVehicleToNodeDirection);
	if(fDot < fMinDot)
	{
		return false;
	}
	
	//Ensure we can spawn at the position.
	if(!pSpawnHelper->CanSpawnAtPosition(VECTOR3_TO_VEC3V(vNodePosition)))
	{
		return false;
	}
	
	//Generate the create input.
	CRoadBlock::CreateInput cInput(roadNodeOutput.m_Node, roadNodeOutput.m_PreviousNode, iResourcesNeeded);
	
	//Get and verify the model ids.
	if(!GetAndVerifyModelIds(cInput.m_aVehModelIds, cInput.m_aPedModelIds, cInput.m_aObjModelIds))
	{
		return false;
	}
	
	//Generate the type.
	CRoadBlock::RoadBlockType nType = (CRoadBlock::RoadBlockType)GenerateType(cInput.m_aObjModelIds);
	switch(nType)
	{
		case CRoadBlock::RBT_Vehicles:
		case CRoadBlock::RBT_SpikeStrip:
		{
			break;
		}
		default:
		{
			return false;
		}
	}
	
	//Tune the min time to live.
	TUNE_GROUP_FLOAT(POLICE_ROAD_BLOCK, fMinTimeToLive, 15.0f, 0.0f, 30.0f, 1.0f);
	
	//Create the road block from the type.
	CRoadBlock::CreateFromTypeInput cftInput(nType, pPed, fMinTimeToLive);
	CRoadBlock* pRoadBlock = CRoadBlock::CreateFromType(cftInput, cInput);
	if(!pRoadBlock)
	{
		return false;
	}

	//Disperse the road block if the target is no longer wanted.
	pRoadBlock->GetConfigFlags().SetFlag(CRoadBlock::CF_DisperseIfTargetIsNotWanted);
	
	//Mark the vehicles as created by dispatch
	for(s32 i = 0; i < pRoadBlock->GetVehCount(); ++i)
	{
		CVehicle* pVehicle = pRoadBlock->GetVeh(i);
		if(pVehicle)
		{
			pVehicle->m_nVehicleFlags.bWasCreatedByDispatch = true;
		}
	}

	//Add the peds to the resources.
	for(s32 i = 0; i < pRoadBlock->GetPedCount(); ++i)
	{
		CPed* pPed = pRoadBlock->GetPed(i);

		if(pPed)
		{
			if(pWanted)
			{
				CDispatchServiceWanted::SetupPedVariation(*pPed, pWanted->GetWantedLevel());
			}

			// Dispatched law peds don't flee from combat
			pPed->GetPedIntelligence()->GetCombatBehaviour().SetFlag(CCombatData::BF_DisableFleeFromCombat);
			pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_CreatedByDispatch, true);

			//Add the ped.
			aiAssert(!pPed->GetPedIntelligence()->GetOrder());
			aiAssert(rResources.GetCapacity() != rResources.GetCount());
			rResources.Push(pPed);
		}
	}
	
	//Check if the spawn helper is advanced.
	if(pSpawnHelper->GetType() == CDispatchSpawnHelper::Advanced)
	{
		//Grab the advanced spawn helper.
		CDispatchAdvancedSpawnHelper* pAdvancedSpawnHelper = static_cast<CDispatchAdvancedSpawnHelper *>(pSpawnHelper);
		
		//Generate the config flags.
		//Road block vehicles can have no driver and still be considered valid.
		//They also cannot be despawned by the spawn helper.
		u8 uConfigFlags = CDispatchAdvancedSpawnHelper::DispatchVehicle::CF_CanHaveNoDriver | CDispatchAdvancedSpawnHelper::DispatchVehicle::CF_CantDespawn;

		//Track the vehicles.
		for(s32 i = 0; i < pRoadBlock->GetVehCount(); ++i)
		{
			//Track the vehicle.
			pAdvancedSpawnHelper->TrackDispatchVehicle(*pRoadBlock->GetVeh(i), uConfigFlags);
		}
	}
	
	return true;
}

void CPoliceRoadBlockDispatch::StreamModels()
{
	//Ensure the local player is not in or around water.
	if(CPoliceBoatDispatch::IsLocalPlayerInOrAroundWater())
	{
		m_streamingRequests.ReleaseAll();
		return;
	}

	//Call the base class version.
	CDispatchServiceWanted::StreamModels();
}

float CPoliceRoadBlockDispatch::GetTimeBetweenSpawnAttempts()
{
	return 15.0f;
}

void CPoliceRoadBlockDispatch::AssignNewOrders(CIncident& rIncident, const Resources& rResources)
{
	s32 iNumResouces = rResources.GetCount();
	for(s32 iCurrentResource = 0; iCurrentResource < iNumResouces; iCurrentResource++)
	{
		CEntity* pEntity = rResources[iCurrentResource];
		if( pEntity )
		{
			CPoliceOrder order(GetDispatchType(), pEntity, &rIncident, CPoliceOrder::PO_WANTED_WAIT_AT_ROAD_BLOCK, VEC3V_TO_VECTOR3(pEntity->GetTransform().GetPosition()));
			COrderManager::GetInstance().AddOrder(order);
		}
	}
}

bool CPoliceRoadBlockDispatch::ShouldAllocateNewResourcesForIncident(const CIncident& rIncident) const
{
	//Call the base class version.
	if(!CDispatchServiceWanted::ShouldAllocateNewResourcesForIncident(rIncident))
	{
		return false;
	}

	//Ensure the entity is valid.
	const CEntity* pEntity = rIncident.GetEntity();
	if(!pEntity)
	{
		return false;
	}
	
	//Ensure the entity is a ped.
	if(!pEntity->GetIsTypePed())
	{
		return false;
	}
	
	//Ensure the ped is driving a vehicle.
	const CPed* pPed = static_cast<const CPed *>(pEntity);
	if(!pPed->GetIsDrivingVehicle())
	{
		return false;
	}
	
	return true;
}

bool CPoliceRoadBlockDispatch::CanUseVehicleSet(const CConditionalVehicleSet& rVehicleSet) const
{
	//Check if we've disabled spike strips.
	const CPed* pLocalPlayer = CGameWorld::FindLocalPlayer();
	if(pLocalPlayer && pLocalPlayer->GetPedResetFlag(CPED_RESET_FLAG_DisableSpikeStripRoadBlocks))
	{
		//Get the object model names.
		const atArray<atHashWithStringNotFinal>& objectModelNames = rVehicleSet.GetObjectModelHashes();

		//Keep track of the object model ids.
		atArray<fwModelId> objectModelIds;
		objectModelIds.Resize(objectModelNames.GetCount());

		//Iterate over the object model names.
		for(int i = 0; i < objectModelNames.GetCount(); i++)
		{
			//Load the model id.
			fwModelId& iModelId = objectModelIds[i];
			CModelInfo::GetBaseModelInfoFromHashKey(objectModelNames[i].GetHash(), &iModelId);
		}

		//Check if the type is spike strip.
		CRoadBlock::RoadBlockType nType = (CRoadBlock::RoadBlockType)GenerateType(objectModelIds);
		if(nType == CRoadBlock::RBT_SpikeStrip)
		{
			return false;
		}
	}

	return true;
}

void CPoliceRoadBlockDispatch::Update()
{
	//Call the base class version.
	CDispatchServiceWanted::Update();

#if __BANK
	//Process the streaming requests.
	s_aRoadBlockStreamingRequests.HaveAllLoaded();
#endif
}

s32 CPoliceRoadBlockDispatch::GenerateType(const atArray<fwModelId>& aObjModelIds) const
{
	//This is a terrible way to determine the road block type.
	//I wish I could think of a better way.
	//This must be kept in sync with dispatch.meta.
	fwModelId iConeModelId;
	CModelInfo::GetBaseModelInfoFromHashKey(atHashString("prop_roadcone01a",0xE0264F5D).GetHash(), &iConeModelId);
	fwModelId iSpikeStripModelId;
	CModelInfo::GetBaseModelInfoFromHashKey(atHashString("p_ld_stinger_s",0xCBE2A89C).GetHash(), &iSpikeStripModelId);
	
	//Iterate over the object models.
	for(int i = 0; i < aObjModelIds.GetCount(); ++i)
	{
		fwModelId iObjModelId = aObjModelIds[i];
		if(iObjModelId == iConeModelId)
		{
			return CRoadBlock::RBT_Vehicles;
		}
		else if(iObjModelId == iSpikeStripModelId)
		{
			return CRoadBlock::RBT_SpikeStrip;
		}
	}
	
	return CRoadBlock::RBT_None;
}

bool CPoliceRoadBlockDispatch::GetAndVerifyModelIds(atArray<fwModelId>& aVehModelIds, atArray<fwModelId>& aPedModelIds, atArray<fwModelId>& aObjModelIds)
{
	//Ensure the vehicle sets are valid.
	if(!m_pDispatchVehicleSets)
	{
		return false;
	}
	
	//Count the vehicle sets.
	int iVehicleSetsCount = m_pDispatchVehicleSets->GetCount();
	if(iVehicleSetsCount <= 0)
	{
		return false;
	}
	
	//Choose a random vehicle set.
	m_CurrVehicleSet = fwRandom::GetRandomNumberInRange(0, iVehicleSetsCount);
	
	//Grab the conditional vehicle set.
	CConditionalVehicleSet* pVehicleSet = CDispatchData::FindConditionalVehicleSet((*m_pDispatchVehicleSets)[m_CurrVehicleSet].GetHash());
	if(!pVehicleSet)
	{
		return false;
	}
	
	//Load the vehicle model ids.
	if(!GetAndVerifyModelIds(pVehicleSet->GetVehicleModelHashes(), aVehModelIds))
	{
		return false;
	}
	
	//Load the ped model ids.
	if(!GetAndVerifyModelIds(pVehicleSet->GetPedModelHashes(), aPedModelIds))
	{
		return false;
	}
	
	//Load the object model ids.
	if(!GetAndVerifyModelIds(pVehicleSet->GetObjectModelHashes(), aObjModelIds))
	{
		return false;
	}
	
	return true;
}

bool CPoliceRoadBlockDispatch::GetAndVerifyModelIds(const atArray<atHashWithStringNotFinal>& aModelNames, atArray<fwModelId>& aModelIds) const
{
	//Resize the model ids.
	aModelIds.Resize(aModelNames.GetCount());
	
	//Iterate over the model names.
	for(int i = 0; i < aModelNames.GetCount(); ++i)
	{	
		//Load the model id.
		fwModelId& iModelId = aModelIds[i];
		CModelInfo::GetBaseModelInfoFromHashKey(aModelNames[i].GetHash(), &iModelId);
		
		//Ensure the model is valid.
		if(!iModelId.IsValid())
		{
			return false;
		}
		
		//Ensure the model has loaded.
		if(!CModelInfo::HaveAssetsLoaded(iModelId))
		{
			return false;
		}
	}
	
	return true;
}

#if __BANK

static int s_iRoadBlockVehModelIndex = 0;
static int s_iRoadBlockPedModelIndex = 0;
static int s_iRoadBlockObjModelIndex = 0;
static int s_iRoadBlockClipSetIndex = 0;

const char** GetRoadBlockVehModels()
{
	static const char* s_aVehModels[] =
	{
		"none",
		"police",
		"policet",
		"riot",
		NULL
	};
	
	return s_aVehModels;
}

const char** GetRoadBlockPedModels()
{
	static const char* s_aPedModels[] =
	{
		"none",
		"S_M_Y_Cop_01",
		NULL
	};
	
	return s_aPedModels;
}

const char** GetRoadBlockObjModels()
{
	static const char* s_aObjModels[] =
	{
		"none",
		"prop_roadcone01a",
		"p_ld_stinger_s",
		NULL
	};

	return s_aObjModels;
}

const char** GetRoadBlockClipSets()
{
	static const char* s_aClipSets[] =
	{
		"none",
		NULL
	};

	return s_aClipSets;
}

void CreateRoadBlock(CRoadBlock::RoadBlockType nType)
{
	//Ensure all streaming requests have loaded.
	if(!aiVerifyf(s_aRoadBlockStreamingRequests.HaveAllLoaded(), "The road block streaming requests have not all loaded."))
	{
		return;
	}
	
	//Ensure the player is valid.
	CPed* pPlayer = CGameWorld::FindLocalPlayer();
	if(!aiVerifyf(pPlayer, "The player is invalid."))
	{
		return;
	}
	
	//Grab the camera position.
	Vector3 vCameraPosition = camInterface::GetPos();
	
	//Grab the player position.
	Vector3 vPlayerPosition = VEC3V_TO_VECTOR3(pPlayer->GetTransform().GetPosition());
	
	//Calculate the direction.
	Vector3 vDirection = vPlayerPosition - vCameraPosition;
	if(!aiVerifyf(vDirection.NormalizeSafeRet(), "The direction is invalid."))
	{
		return;
	}
	
	//Generate the create input.
	u32 uMaxVehs = 5;
	CRoadBlock::CreateInput cInput(vCameraPosition, Vector2(vDirection, Vector2::kXY), uMaxVehs);
	
	//Add the vehicle model.
	if(s_iRoadBlockVehModelIndex != 0)
	{
		fwModelId iModelId;
		CModelInfo::GetBaseModelInfoFromHashKey(atHashWithStringNotFinal(GetRoadBlockVehModels()[s_iRoadBlockVehModelIndex]), &iModelId);
		if(iModelId.IsValid())
		{
			cInput.m_aVehModelIds.PushAndGrow(iModelId);
		}
	}
	
	//Add the ped model.
	if(s_iRoadBlockPedModelIndex != 0)
	{
		fwModelId iModelId;
		CModelInfo::GetBaseModelInfoFromHashKey(atHashWithStringNotFinal(GetRoadBlockPedModels()[s_iRoadBlockPedModelIndex]), &iModelId);
		if(iModelId.IsValid())
		{
			cInput.m_aPedModelIds.PushAndGrow(iModelId);
		}
	}
	
	//Add the obj model.
	if(s_iRoadBlockObjModelIndex != 0)
	{
		fwModelId iModelId;
		CModelInfo::GetBaseModelInfoFromHashKey(atHashWithStringNotFinal(GetRoadBlockObjModels()[s_iRoadBlockObjModelIndex]), &iModelId);
		if(iModelId.IsValid())
		{
			cInput.m_aObjModelIds.PushAndGrow(iModelId);
		}
	}
	
	//Create the road block from the type.
	CPed* pPed = CGameWorld::FindLocalPlayer();
	float fTimeToLive = 15.0f;
	CRoadBlock::CreateFromTypeInput cftInput(nType, pPed, fTimeToLive);
	CRoadBlock::CreateFromType(cftInput, cInput);
}

void CreateVehicleRoadBlockCB()
{
	CreateRoadBlock(CRoadBlock::RBT_Vehicles);
}

void CreateSpikeStripRoadBlockCB()
{
	CreateRoadBlock(CRoadBlock::RBT_SpikeStrip);
}

void UpdateRoadBlockStreamingCB()
{
	//Release all the streaming requests.
	s_aRoadBlockStreamingRequests.ReleaseAll();
	
	//Stream the vehicle model.
	if(s_iRoadBlockVehModelIndex != 0)
	{
		fwModelId iModelId;
		CModelInfo::GetBaseModelInfoFromHashKey(atHashWithStringNotFinal(GetRoadBlockVehModels()[s_iRoadBlockVehModelIndex]), &iModelId);
		if(iModelId.IsValid())
		{
			s32 transientLocalIdx = CModelInfo::AssignLocalIndexToModelInfo(iModelId).Get();
			s_aRoadBlockStreamingRequests.PushRequest(transientLocalIdx, CModelInfo::GetStreamingModuleId(), STRFLAG_PRIORITY_LOAD);
		}
	}
	
	//Stream the ped model.
	if(s_iRoadBlockPedModelIndex != 0)
	{
		fwModelId iModelId;
		CModelInfo::GetBaseModelInfoFromHashKey(atHashWithStringNotFinal(GetRoadBlockPedModels()[s_iRoadBlockPedModelIndex]), &iModelId);
		if(iModelId.IsValid())
		{
			s32 transientLocalIdx = CModelInfo::AssignLocalIndexToModelInfo(iModelId).Get();
			s_aRoadBlockStreamingRequests.PushRequest(transientLocalIdx, CModelInfo::GetStreamingModuleId(), STRFLAG_PRIORITY_LOAD);
		}
	}
	
	//Stream the obj model.
	if(s_iRoadBlockObjModelIndex != 0)
	{
		fwModelId iModelId;
		CModelInfo::GetBaseModelInfoFromHashKey(atHashWithStringNotFinal(GetRoadBlockObjModels()[s_iRoadBlockObjModelIndex]), &iModelId);
		if(iModelId.IsValid())
		{
			s32 transientLocalIdx = CModelInfo::AssignLocalIndexToModelInfo(iModelId).Get();
			s_aRoadBlockStreamingRequests.PushRequest(transientLocalIdx, CModelInfo::GetStreamingModuleId());
		}
	}
	
	//Stream the clip set.
	if(s_iRoadBlockClipSetIndex != 0)
	{
		fwMvClipSetId clipSetId(atHashWithStringNotFinal(GetRoadBlockClipSets()[s_iRoadBlockClipSetIndex]).GetHash());
		if(clipSetId != CLIP_SET_ID_INVALID)
		{
			s32 transientLocalIdx = fwClipSetManager::GetClipDictionaryIndex(clipSetId);
			s_aRoadBlockStreamingRequests.PushRequest(transientLocalIdx, fwAnimManager::GetStreamingModuleId(), STRFLAG_PRIORITY_LOAD);
		}
	}
}

void CPoliceRoadBlockDispatch::AddExtraWidgets(bkBank* pBank)
{
	static const char** ppVehModels	= GetRoadBlockVehModels();
	static const char** ppPedModels	= GetRoadBlockPedModels();
	static const char** ppObjModels	= GetRoadBlockObjModels();
	static const char** ppClipSets	= GetRoadBlockClipSets();
	
	int iNumVehModels = 0;
	while(ppVehModels[iNumVehModels] != NULL) ++iNumVehModels;
	
	int iNumPedModels = 0;
	while(ppPedModels[iNumPedModels] != NULL) ++iNumPedModels;
	
	int iNumObjModels = 0;
	while(ppObjModels[iNumObjModels] != NULL) ++iNumObjModels;
	
	int iNumClipSets = 0;
	while(ppClipSets[iNumClipSets] != NULL) ++iNumClipSets;
	
	pBank->PushGroup("Creator");
		pBank->AddCombo("Veh Model:",	&s_iRoadBlockVehModelIndex,	iNumVehModels,	ppVehModels,	UpdateRoadBlockStreamingCB);
		pBank->AddCombo("Ped Model:",	&s_iRoadBlockPedModelIndex,	iNumPedModels,	ppPedModels,	UpdateRoadBlockStreamingCB);
		pBank->AddCombo("Obj Model:",	&s_iRoadBlockObjModelIndex,	iNumObjModels,	ppObjModels,	UpdateRoadBlockStreamingCB);
		pBank->AddCombo("Clip Set:",	&s_iRoadBlockClipSetIndex,	iNumClipSets,	ppClipSets,		UpdateRoadBlockStreamingCB);
		pBank->AddButton("Create Vehicle Road Block", CreateVehicleRoadBlockCB);
		pBank->AddButton("Create Spike Strip Road Block", CreateSpikeStripRoadBlockCB);
	pBank->PopGroup();
}
#endif

//=========================================================================
// CPoliceBoatDispatch
//=========================================================================

CPoliceBoatDispatch::Tunables CPoliceBoatDispatch::sm_Tunables;
IMPLEMENT_DISPATCH_SERVICES_TUNABLES(CPoliceBoatDispatch, 0x5ce5c79b);

CPoliceBoatDispatch::CPoliceBoatDispatch()
: CDispatchServiceWanted(DT_PoliceBoat)
{}

bool CPoliceBoatDispatch::IsInOrAroundWater(const CPed& rPed)
{
	//Check if the ped is swimming.
	if(rPed.GetIsSwimming())
	{
		return true;
	}

	//Check if the ped is in a boat.
	const CVehicle* pVehicle = rPed.GetVehiclePedInside();
	if(pVehicle && pVehicle->InheritsFromBoat())
	{
		return true;
	}

	//Check if the ped is standing on a boat.
	const CPhysical* pPhysical = rPed.GetGroundPhysical();
	if(pPhysical && pPhysical->GetIsTypeVehicle() && static_cast<const CVehicle *>(pPhysical)->InheritsFromBoat())
	{
		return true;
	}

	return false;
}

bool CPoliceBoatDispatch::IsLocalPlayerInOrAroundWater()
{
	const CPed* pPed = CGameWorld::FindLocalPlayer();
	return (pPed && IsInOrAroundWater(*pPed));
}

bool CPoliceBoatDispatch::GetIsEntityValidForOrder(CEntity* pEntity)
{
	//Call the base class version.
	if(!CDispatchServiceWanted::GetIsEntityValidForOrder(pEntity))
	{
		return false;
	}
	
	//Ensure the entity is valid.
	if(!pEntity)
	{
		return false;
	}
	
	//Ensure the entity is a ped.
	if(!pEntity->GetIsTypePed())
	{
		return false;
	}
	
	//Grab the ped.
	const CPed* pPed = static_cast<CPed *>(pEntity);
	
	//Ensure the ped is random.
	if(!pPed->PopTypeIsRandom())
	{
		return false;
	}
	
	//Ensure the ped is a cop.
	if(!pPed->IsRegularCop())
	{
		return false;
	}
	
	return true;
}

bool CPoliceBoatDispatch::GetIsEntityValidToBeAssignedOrder(CEntity* pEntity)
{
	//Ensure the entity is valid for the order.
	if(!GetIsEntityValidForOrder(pEntity))
	{
		return false;
	}
	
	//Assert that the entity is a ped.
	aiAssertf(pEntity, "The entity is invalid.");
	aiAssertf(pEntity->GetIsTypePed(), "The entity is not a ped.");
	
	//Grab the ped.
	const CPed* pPed = static_cast<CPed *>(pEntity);
	
	//Ensure the ped is in a vehicle.
	const CVehicle* pVehicle = pPed->GetVehiclePedInside();
	if(!pVehicle)
	{
		return false;
	}
	
	//Ensure the vehicle is a boat.
	if(!pVehicle->InheritsFromBoat())
	{
		return false;
	}
	
	return true;
}

#define MIN_POLICE_BOAT_RESOURCES_NEEDED 3

void CPoliceBoatDispatch::GetNumNetResourcesToReservePerIncident(u32 &numPeds, u32 &numVehicles, u32 &numObjects) const
{
	numPeds			= MIN_POLICE_BOAT_RESOURCES_NEEDED;
	numVehicles		= 1;
	numObjects		= 0;
}

bool CPoliceBoatDispatch::SpawnNewResources(CIncident& rIncident, Resources& rResources)
{
	//Ensure resources are needed.
	int iResourcesNeeded = rResources.GetCapacity() - rResources.GetCount();
	if(iResourcesNeeded < MIN_POLICE_BOAT_RESOURCES_NEEDED)
	{
		return false;
	}
	
	//Ensure the assets have been streamed.
	if((m_streamingRequests.GetRequestCount() <= 0) || !m_streamingRequests.HaveAllLoaded())
	{
		return false;
	}
	
	//Ensure the model ids are valid.
	fwModelId iVehicleModelId;
	fwModelId iSelectedPedModelId;
	fwModelId iObjectModelId;
	if(!GetAndVerifyModelIds(iVehicleModelId, iSelectedPedModelId, iObjectModelId, MaxVehiclesInMap))
	{
		return false;
	}
	
	//Ensure the spawn helper is valid.
	CDispatchSpawnHelper* pSpawnHelper = ms_SpawnHelpers.FindSpawnHelperForIncident(rIncident);
	if(!pSpawnHelper)
	{
		return false;
	}
	
	//Ensure the spawn helper can spawn a boat.
	if(!pSpawnHelper->CanSpawnBoat())
	{
		return false;
	}
	
	//Spawn the boat.
	CBoat* pBoat = pSpawnHelper->SpawnBoat(iVehicleModelId);
	if(!pBoat)
	{
		return false;
	}

	//Delay the removal time.	
	TUNE_GROUP_INT(POLICE_BOAT_DISPATCH, iExtraRemovalTime, 30000, 0, 120000, 1000);
	pBoat->DelayedRemovalTimeReset(iExtraRemovalTime);
	pBoat->m_nVehicleFlags.bWasCreatedByDispatch = true;
	
	//Add the occupants.
	CVehiclePopulation::AddPoliceVehOccupants(pBoat, true, false, iSelectedPedModelId.GetModelIndex(), const_cast<CIncident*>(&rIncident), GetDispatchType());

	CWanted* pWanted = CDispatchServiceWanted::GetIncidentPedWanted(rIncident);

	//Add the occupants to the resources.
	for(s32 iSeat = 0; iSeat < pBoat->GetSeatManager()->GetMaxSeats(); iSeat++)
	{
		CPed* pPassenger = pBoat->GetSeatManager()->GetPedInSeat(iSeat);
		if(pPassenger)
		{
			if(pWanted)
			{
				CDispatchServiceWanted::SetupPedVariation(*pPassenger, pWanted->GetWantedLevel());
			}

			// Dispatched law peds don't flee from combat
			pPassenger->GetPedIntelligence()->GetCombatBehaviour().SetFlag(CCombatData::BF_DisableFleeFromCombat);
			pPassenger->SetPedConfigFlag(CPED_CONFIG_FLAG_CreatedByDispatch, true);

			//Add the passenger.
			aiAssert(!pPassenger->GetPedIntelligence()->GetOrder());
			aiAssert(rResources.GetCapacity() != rResources.GetCount());
			rResources.Push(pPassenger);
		}
	}
	
	//Increase the vehicle set.
	m_CurrVehicleSet++;
	
	return true;
}

float CPoliceBoatDispatch::GetTimeBetweenSpawnAttempts()
{
	return sm_Tunables.m_TimeBetweenSpawnAttempts;
}

void CPoliceBoatDispatch::AssignNewOrder(CEntity& rEntity, CIncident& rIncident)
{
	//Generate the police dispatch order type.
	CPoliceOrder::ePoliceDispatchOrderType ePoliceOrder = CPoliceOrder::PO_NONE;
	if(rIncident.GetType() == CIncident::IT_Wanted)
	{
		aiAssertf(rEntity.GetIsTypePed(), "Entity is not a ped.");
		if(static_cast<CPed *>(&rEntity)->GetIsDrivingVehicle())
		{
			ePoliceOrder = CPoliceOrder::PO_WANTED_GOTO_DISPATCH_LOCATION_AS_DRIVER;
		}
		else 
		{
			ePoliceOrder = CPoliceOrder::PO_WANTED_GOTO_DISPATCH_LOCATION_AS_PASSENGER;
		}
	}

	//Add the order.
	CPoliceOrder order(GetDispatchType(), &rEntity, &rIncident, ePoliceOrder, rIncident.GetLocation());
	COrderManager::GetInstance().AddOrder(order);
}

bool CPoliceBoatDispatch::ShouldAllocateNewResourcesForIncident(const CIncident& rIncident) const
{
	//Call the base class version.
	if(!CDispatchServiceWanted::ShouldAllocateNewResourcesForIncident(rIncident))
	{
		return false;
	}

	//Ensure the entity is valid.
	const CEntity* pEntity = rIncident.GetEntity();
	if(!pEntity)
	{
		return false;
	}

	//Ensure the entity is a ped.
	if(!pEntity->GetIsTypePed())
	{
		return false;
	}

	//Ensure the target ped is in or around water.
	const CPed* pTargetPed = static_cast<const CPed *>(pEntity);
	if(!IsInOrAroundWater(*pTargetPed))
	{
		return false;
	}

	return true;
}

void CPoliceBoatDispatch::DispatchDelayedSpawnCallback(CEntity* pEntity, CIncident* pIncident, s32 pedGroupIndex, COrder* UNUSED_PARAM(pOrder))
{
	if(pEntity)
	{
		CPed* pPed = static_cast<CPed*>(pEntity);

		static const atHashWithStringNotFinal COP_BOAT_LOADOUT("LOADOUT_COP_BOAT",0x17721F2F);
		CPedInventoryLoadOutManager::SetLoadOut(pPed, COP_BOAT_LOADOUT.GetHash());

		CDispatchServiceWanted::DispatchDelayedSpawnCallback(pEntity, pIncident, pedGroupIndex);
	}
}

void CPoliceBoatDispatch::StreamModels()
{
	//Ensure the local player is in or around water.
	if(!IsLocalPlayerInOrAroundWater())
	{
		m_streamingRequests.ReleaseAll();
		return;
	}

	//Call the base class version.
	CDispatchServiceWanted::StreamModels();
}

void CGangDispatch::AssignNewOrders(CIncident& rIncident, const Resources& rResources)
{
	s32 iNumResouces = rResources.GetCount();
	for(s32 iCurrentResource = 0; iCurrentResource < iNumResouces; iCurrentResource++)
	{
		CEntity* pEntity = rResources[iCurrentResource];
		if( pEntity )
		{
			CGangOrder order(GetDispatchType(), pEntity, &rIncident);
			COrderManager::GetInstance().AddOrder(order);
		}
	}
}


bool CGangDispatch::GetIsEntityValidForOrder( CEntity* pEntity )
{
	aiAssert(pEntity);
	if( !pEntity->GetIsTypePed() )
	{
		return false;
	}

	if(!CDispatchService::GetIsEntityValidForOrder(pEntity))
	{
		return false;
	}


	// Make sure the assets have been streamed
	if((m_streamingRequests.GetRequestCount() <= 0) || !m_streamingRequests.HaveAllLoaded())
	{
		return false;
	}

	fwModelId iVehicleModelId;
	fwModelId iSelectedPedModelId;
	fwModelId iObjectModelId;
	if(!GetAndVerifyModelIds(iVehicleModelId, iSelectedPedModelId, iObjectModelId, -1))
	{
		return false;
	}

	CPed* pPed = static_cast<CPed*>(pEntity);
	if( pPed->PopTypeIsRandom() )
	{
		//allow medics to be used unless they are transporting a victim already
		if( pPed->GetModelId() == iSelectedPedModelId )
		{
			return true;
		}
	}

	return false;
}

void CGangDispatch::GetNumNetResourcesToReservePerIncident(u32 &numPeds, u32 &numVehicles, u32 &numObjects) const
{
	numPeds			= (s32)NumPedsPerVehicle;
	numVehicles		= 1;
	numObjects		= 0;
}

bool CGangDispatch::SpawnNewResources(CIncident& rIncident, Resources& rResources)
{
	int iResourcesNeeded = rResources.GetCapacity() - rResources.GetCount();

	// Make sure the assets have been streamed
	if((m_streamingRequests.GetRequestCount() <= 0) || !m_streamingRequests.HaveAllLoaded())
	{
		return false;
	}

	fwModelId iVehicleModelId;
	fwModelId iSelectedPedModelId;
	fwModelId iObjectModelId;
	if(!GetAndVerifyModelIds(iVehicleModelId, iSelectedPedModelId, iObjectModelId, MaxVehiclesInMap))
	{
		return false;
	}

	//Ensure the spawn helper is valid.
	CDispatchSpawnHelper* pSpawnHelper = ms_SpawnHelpers.FindSpawnHelperForIncident(rIncident);
	if(!pSpawnHelper)
	{
		return false;
	}

	//Ensure the spawn helper can spawn a vehicle.
	if(!pSpawnHelper->CanSpawnVehicle())
	{
		return false;
	}

	//Generate a vehicle around the player.
	CVehicle* pVehicle = pSpawnHelper->SpawnVehicle(iVehicleModelId);
	if(pVehicle)
	{	
		pVehicle->m_nVehicleFlags.bDisablePretendOccupants = true;
		pVehicle->SetUpDriver(false, true, iSelectedPedModelId.GetModelIndex());

		const s32 iNumPedsToSpawn = Min((s32)NumPedsPerVehicle, iResourcesNeeded) - 1;

		for(s32 i = 1; i < iNumPedsToSpawn; i++)
		{
			// only create passengers if we can register another ped
			if(!NetworkInterface::IsGameInProgress() || CNetworkObjectPopulationMgr::CanRegisterLocalObjectsOfType(NET_OBJ_TYPE_PED, 1, false))
			{
				pVehicle->SetupPassenger(i, true, iSelectedPedModelId.GetModelIndex());
			}
		}

		// For every ped spawned, mark a resource as being available
		for( s32 iSeat = 0; iSeat < pVehicle->GetSeatManager()->GetMaxSeats(); iSeat++ )
		{
			CPed* pPassenger = pVehicle->GetSeatManager()->GetPedInSeat(iSeat);
			if( pPassenger )
			{
				//Add the passenger.
				aiAssert(!pPassenger->GetPedIntelligence()->GetOrder());
				aiAssert(rResources.GetCapacity() != rResources.GetCount());
				rResources.Push(pPassenger);
			}
		}
	}
	return true;
}

void CArmyVehicleDispatch::AssignNewOrders(CIncident& rIncident, const Resources& rResources)
{
	s32 iNumResouces = rResources.GetCount();
	for(s32 iCurrentResource = 0; iCurrentResource < iNumResouces; iCurrentResource++)
	{
		CEntity* pEntity = rResources[iCurrentResource];
		if( pEntity )
		{
			CGangOrder order(GetDispatchType(), pEntity, &rIncident);
			COrderManager::GetInstance().AddOrder(order);
		}
	}
}


bool CArmyVehicleDispatch::GetIsEntityValidForOrder( CEntity* pEntity )
{
	aiAssert(pEntity);
	if( !pEntity->GetIsTypePed() )
	{
		return false;
	}

	if(!CDispatchService::GetIsEntityValidForOrder(pEntity))
	{
		return false;
	}


	// Make sure the assets have been streamed
	if((m_streamingRequests.GetRequestCount() <= 0) || !m_streamingRequests.HaveAllLoaded())
	{
		return false;
	}

	fwModelId iVehicleModelId;
	fwModelId iSelectedPedModelId;
	fwModelId iObjectModelId;
	if(!GetAndVerifyModelIds(iVehicleModelId, iSelectedPedModelId, iObjectModelId, -1))
	{
		return false;
	}

	CPed* pPed = static_cast<CPed*>(pEntity);
	if( pPed->PopTypeIsRandom() )
	{
		if( pPed->GetModelId() == iSelectedPedModelId )
		{
			return true;
		}
	}

	return false;
}

void CArmyVehicleDispatch::GetNumNetResourcesToReservePerIncident(u32 &numPeds, u32 &numVehicles, u32 &numObjects) const
{
	numPeds			= (s32)NumPedsPerVehicle;
	numVehicles		= 1;
	numObjects		= 0;
}

#define MIN_ARMY_VEHICLE_RESOURCES_NEEDED 2

bool CArmyVehicleDispatch::SpawnNewResources(CIncident& rIncident, Resources& rResources)
{
	int iResourcesNeeded = rResources.GetCapacity() - rResources.GetCount();
	if(iResourcesNeeded < MIN_ARMY_VEHICLE_RESOURCES_NEEDED)
	{
		return false;
	}

	// Make sure the assets have been streamed
	if((m_streamingRequests.GetRequestCount() <= 0) || !m_streamingRequests.HaveAllLoaded())
	{
		return false;
	}

	fwModelId iVehicleModelId;
	fwModelId iSelectedPedModelId;
	fwModelId iObjectModelId;
	if(!GetAndVerifyModelIds(iVehicleModelId, iSelectedPedModelId, iObjectModelId, MaxVehiclesInMap))
	{
		return false;
	}

	//Ensure the spawn helper is valid.
	CDispatchSpawnHelper* pSpawnHelper = ms_SpawnHelpers.FindSpawnHelperForIncident(rIncident);
	if(!pSpawnHelper)
	{
		return false;
	}

	//Ensure the spawn helper can spawn a vehicle.
	if(!pSpawnHelper->CanSpawnVehicle())
	{
		return false;
	}

	//Generate a vehicle around the player.
	CVehicle* pVehicle = pSpawnHelper->SpawnVehicle(iVehicleModelId);
	if(pVehicle)
	{	
		pVehicle->m_nVehicleFlags.bDisablePretendOccupants = true;
		pVehicle->m_nVehicleFlags.bRemoveWithEmptyCopOrWreckedVehs = true;
		pVehicle->m_nVehicleFlags.bMercVeh = true;
		pVehicle->SetUpDriver(false, true, iSelectedPedModelId.GetModelIndex());

		const s32 iNumPedsToSpawn = Min((s32)NumPedsPerVehicle, iResourcesNeeded) - 1;

		for(s32 i = 1; i <= iNumPedsToSpawn; i++)
		{
			// only create passengers if we can register another ped
			if(!NetworkInterface::IsGameInProgress() || CNetworkObjectPopulationMgr::CanRegisterLocalObjectsOfType(NET_OBJ_TYPE_PED, 1, false))
			{
				pVehicle->SetupPassenger(i, true, iSelectedPedModelId.GetModelIndex());
			}
		}

		// For every ped spawned, mark a resource as being available
		for( s32 iSeat = 0; iSeat < pVehicle->GetSeatManager()->GetMaxSeats(); iSeat++ )
		{
			CPed* pPassenger = pVehicle->GetSeatManager()->GetPedInSeat(iSeat);
			if( pPassenger )
			{
				//Add the ped.
				aiAssert(!pPassenger->GetPedIntelligence()->GetOrder());
				aiAssert(rResources.GetCapacity() != rResources.GetCount());
				rResources.Push(pPassenger);
			}
		}
	}
	return true;
}

void CBikerBackupDispatch::AssignNewOrders(CIncident& rIncident, const Resources& rResources)
{
	s32 iNumResouces = rResources.GetCount();
	for(s32 iCurrentResource = 0; iCurrentResource < iNumResouces; iCurrentResource++)
	{
		CEntity* pEntity = rResources[iCurrentResource];
		if( pEntity )
		{
			CGangOrder order(GetDispatchType(), pEntity, &rIncident);
			COrderManager::GetInstance().AddOrder(order);
		}
	}
}


bool CBikerBackupDispatch::GetIsEntityValidForOrder( CEntity* pEntity )
{
	aiAssert(pEntity);
	if( !pEntity->GetIsTypePed() )
	{
		return false;
	}

	if(!CDispatchService::GetIsEntityValidForOrder(pEntity))
	{
		return false;
	}


	// Make sure the assets have been streamed
	if((m_streamingRequests.GetRequestCount() <= 0) || !m_streamingRequests.HaveAllLoaded())
	{
		return false;
	}

	fwModelId iVehicleModelId;
	fwModelId iSelectedPedModelId;
	fwModelId iObjectModelId;
	if(!GetAndVerifyModelIds(iVehicleModelId, iSelectedPedModelId, iObjectModelId, -1))
	{
		return false;
	}

	CPed* pPed = static_cast<CPed*>(pEntity);
	if( pPed->PopTypeIsRandom() )
	{
		if( pPed->GetModelId() == iSelectedPedModelId )
		{
			return true;
		}
	}

	return false;
}

void CBikerBackupDispatch::GetNumNetResourcesToReservePerIncident(u32 &numPeds, u32 &numVehicles, u32 &numObjects) const
{
	numPeds			= (s32)NumPedsPerVehicle;
	numVehicles		= 1;
	numObjects		= 0;
}

#define MIN_BIKER_BACKUP_RESOURCES_NEEDED 2

bool CBikerBackupDispatch::SpawnNewResources(CIncident& rIncident, Resources& rResources)
{
	int iResourcesNeeded = rResources.GetCapacity() - rResources.GetCount();
	if(iResourcesNeeded < MIN_BIKER_BACKUP_RESOURCES_NEEDED)
	{
		return false;
	}

	// Make sure the assets have been streamed
	if((m_streamingRequests.GetRequestCount() <= 0) || !m_streamingRequests.HaveAllLoaded())
	{
		return false;
	}

	fwModelId iVehicleModelId;
	fwModelId iSelectedPedModelId;
	fwModelId iObjectModelId;
	if(!GetAndVerifyModelIds(iVehicleModelId, iSelectedPedModelId, iObjectModelId, MaxVehiclesInMap))
	{
		return false;
	}

	//Ensure the spawn helper is valid.
	CDispatchSpawnHelper* pSpawnHelper = ms_SpawnHelpers.FindSpawnHelperForIncident(rIncident);
	if(!pSpawnHelper)
	{
		return false;
	}

	//Ensure the spawn helper can spawn a vehicle.
	if(!pSpawnHelper->CanSpawnVehicle())
	{
		return false;
	}

	//Generate a vehicle around the player.
	CVehicle* pVehicle = pSpawnHelper->SpawnVehicle(iVehicleModelId);
	if(pVehicle)
	{	
		pVehicle->m_nVehicleFlags.bDisablePretendOccupants = true;
		pVehicle->m_nVehicleFlags.bRemoveWithEmptyCopOrWreckedVehs = true;
		pVehicle->m_nVehicleFlags.bMercVeh = true;
		pVehicle->SetUpDriver(false, true, iSelectedPedModelId.GetModelIndex());

		const s32 iNumPedsToSpawn = Min((s32)NumPedsPerVehicle, iResourcesNeeded) - 1;

		//Lookup the override relationship group that could be set by script
		CRelationshipGroup* pOverrideRelGroup = NULL;
		if(rIncident.GetType() == CIncident::IT_Scripted)
		{
			const CScriptIncident* pScriptIncident = static_cast<const CScriptIncident*>(&rIncident);
			if(pScriptIncident)
			{
				u32 uOverrideRelGroupHash = pScriptIncident->GetOverrideRelationshipGroupHash();
				if (uOverrideRelGroupHash)
				{
					pOverrideRelGroup = CRelationshipManager::FindRelationshipGroup(uOverrideRelGroupHash);
				}
			}
		}

		for(s32 i = 1; i <= iNumPedsToSpawn; i++)
		{
			// only create passengers if we can register another ped
			if(!NetworkInterface::IsGameInProgress() || CNetworkObjectPopulationMgr::CanRegisterLocalObjectsOfType(NET_OBJ_TYPE_PED, 1, false))
			{
				pVehicle->SetupPassenger(i, true, iSelectedPedModelId.GetModelIndex());
			}
		}

		// For every ped spawned, mark a resource as being available
		for( s32 iSeat = 0; iSeat < pVehicle->GetSeatManager()->GetMaxSeats(); iSeat++ )
		{
			CPed* pPassenger = pVehicle->GetSeatManager()->GetPedInSeat(iSeat);
			if( pPassenger )
			{
				//Add the ped.
				aiAssert(!pPassenger->GetPedIntelligence()->GetOrder());
				aiAssert(rResources.GetCapacity() != rResources.GetCount());
				rResources.Push(pPassenger);

				//If a relationship group override exists, set it for this ped
				if (pOverrideRelGroup)
				{
					pPassenger->GetPedIntelligence()->SetRelationshipGroup(pOverrideRelGroup);
				}
			}
		}
	}
	return true;
}

//////////////////////////////////////////////////////////////////////////
//	CAssassinsDispatch
//////////////////////////////////////////////////////////////////////////

CAssassinsDispatch::Tunables CAssassinsDispatch::ms_Tunables;
IMPLEMENT_DISPATCH_SERVICES_TUNABLES(CAssassinsDispatch, 0x3AD339CB);

void CAssassinsDispatch::GetNumNetResourcesToReservePerIncident(u32 &numPeds, u32 &numVehicles, u32 &numObjects) const
{
	numPeds			= (s32)NumPedsPerVehicle;
	numVehicles		= 1;
	numObjects		= 0;
}

bool CAssassinsDispatch::GetIsEntityValidForOrder( CEntity* pEntity )
{
	aiAssert(pEntity);
	if( !pEntity->GetIsTypePed() )
	{
		return false;
	}

	if(!CDispatchService::GetIsEntityValidForOrder(pEntity))
	{
		return false;
	}


	// Make sure the assets have been streamed
	if((m_streamingRequests.GetRequestCount() <= 0) || !m_streamingRequests.HaveAllLoaded())
	{
		return false;
	}

	fwModelId iVehicleModelId;
	fwModelId iSelectedPedModelId;
	fwModelId iObjectModelId;
	if(!CDispatchService::GetAndVerifyModelIds(iVehicleModelId, iSelectedPedModelId, iObjectModelId, -1))
	{
		return false;
	}

	CPed* pPed = static_cast<CPed*>(pEntity);
	if( pPed->PopTypeIsRandom() )
	{
		if( pPed->GetModelId() == iSelectedPedModelId )
		{
			return true;
		}
	}

	return false;
}

bool CAssassinsDispatch::SpawnNewResources(CIncident& rIncident, Resources& rResources)
{
	int iResourcesNeeded = rResources.GetCapacity() - rResources.GetCount();
	if(iResourcesNeeded < MIN_BIKER_BACKUP_RESOURCES_NEEDED)
	{
		return false;
	}

	// Make sure the assets have been streamed
	if((m_streamingRequests.GetRequestCount() <= 0) || !m_streamingRequests.HaveAllLoaded())
	{
		return false;
	}

	fwModelId iVehicleModelId;
	fwModelId iSelectedPedModelId;
	fwModelId iObjectModelId;
	const CScriptIncident* pScriptIncident = (rIncident.GetType() == CIncident::IT_Scripted) ? static_cast<const CScriptIncident*>(&rIncident) : NULL;

	int iAssassinsLevel = pScriptIncident ? pScriptIncident->GetAssassinDispatchLevel() : AL_Invalid;

#if __DEV
	if (iAssassinsLevel != AL_Invalid)
	{
		aiAssertf(iAssassinsLevel > AL_Invalid && iAssassinsLevel < AL_Max, "Assassin dispatch level %i is not supported! Max allowed - %i", iAssassinsLevel, AL_Max);
	}
#endif

	if (iAssassinsLevel > AL_Invalid)
	{
		// This picks models based on the passed in assassin level
		if(!GetAndVerifyModelIds(iVehicleModelId, iSelectedPedModelId, iObjectModelId, MaxVehiclesInMap, iAssassinsLevel))
		{
			return false;
		}
	}
	else
	{
		if(!CDispatchService::GetAndVerifyModelIds(iVehicleModelId, iSelectedPedModelId, iObjectModelId, MaxVehiclesInMap))
		{
			return false;
		}
	}

	//Ensure the spawn helper is valid.
	CDispatchSpawnHelper* pSpawnHelper = ms_SpawnHelpers.FindSpawnHelperForIncident(rIncident);
	if(!pSpawnHelper)
	{
		return false;
	}

	//Ensure the spawn helper can spawn a vehicle.
	if(!pSpawnHelper->CanSpawnVehicle())
	{
		return false;
	}

	//Generate a vehicle around the player.
	CVehicle* pVehicle = pSpawnHelper->SpawnVehicle(iVehicleModelId);
	if(pVehicle)
	{	
		pVehicle->m_nVehicleFlags.bDisablePretendOccupants = true;
		pVehicle->m_nVehicleFlags.bRemoveWithEmptyCopOrWreckedVehs = true;
		pVehicle->m_nVehicleFlags.bMercVeh = true;
		pVehicle->SetUpDriver(false, true, iSelectedPedModelId.GetModelIndex());

		const s32 iNumPedsToSpawn = Min((s32)NumPedsPerVehicle, iResourcesNeeded) - 1;

		//Lookup the override relationship group that could be set by script
		CRelationshipGroup* pOverrideRelGroup = NULL;
		if(pScriptIncident)
		{
			u32 uOverrideRelGroupHash = pScriptIncident->GetOverrideRelationshipGroupHash();
			if (uOverrideRelGroupHash)
			{
				pOverrideRelGroup = CRelationshipManager::FindRelationshipGroup(uOverrideRelGroupHash);
			}
		}

		for(s32 i = 1; i <= iNumPedsToSpawn; i++)
		{
			// only create passengers if we can register another ped
			if(!NetworkInterface::IsGameInProgress() || CNetworkObjectPopulationMgr::CanRegisterLocalObjectsOfType(NET_OBJ_TYPE_PED, 1, false))
			{
				pVehicle->SetupPassenger(i, true, iSelectedPedModelId.GetModelIndex());
			}
		}

		// For every ped spawned, mark a resource as being available
		for( s32 iSeat = 0; iSeat < pVehicle->GetSeatManager()->GetMaxSeats(); iSeat++ )
		{
			CPed* pPassenger = pVehicle->GetSeatManager()->GetPedInSeat(iSeat);
			if( pPassenger )
			{
				//Add the ped.
				aiAssert(!pPassenger->GetPedIntelligence()->GetOrder());
				aiAssert(rResources.GetCapacity() != rResources.GetCount());
				rResources.Push(pPassenger);

				//If a relationship group override exists, set it for this ped
				if (pOverrideRelGroup)
				{
					pPassenger->GetPedIntelligence()->SetRelationshipGroup(pOverrideRelGroup);
				}

				SetupAssassinPed(pPassenger, iAssassinsLevel, iSeat);
			}
		}
	}
	return true;
}

void CAssassinsDispatch::AssignNewOrders(CIncident& rIncident, const Resources& rResources)
{
	s32 iNumResouces = rResources.GetCount();
	for(s32 iCurrentResource = 0; iCurrentResource < iNumResouces; iCurrentResource++)
	{
		CEntity* pEntity = rResources[iCurrentResource];
		if( pEntity )
		{
			CGangOrder order(GetDispatchType(), pEntity, &rIncident);
			COrderManager::GetInstance().AddOrder(order);
		}
	}
}

bool CAssassinsDispatch::GetAndVerifyModelIds(fwModelId &vehicleModelId, fwModelId &pedModelId, fwModelId& objModelId, int maxVehicleInMap, int assassinsDispatchLevel)
{
	if(!m_pDispatchVehicleSets)
	{
		return false;
	}

	int iVehicleSetscount = m_pDispatchVehicleSets->GetCount();

	// If we want to force a vehicle set then set our current vehicle set to the one we chose
	if(m_iForcedVehicleSet >= 0 && m_iForcedVehicleSet < iVehicleSetscount)
	{
		m_CurrVehicleSet = m_iForcedVehicleSet;
	}
	// Make sure our current vehicle set is within our count (otherwise loop back to the beginning)
	else if(m_CurrVehicleSet >= iVehicleSetscount)
	{		
		m_CurrVehicleSet = 0;
	}

	if (aiVerifyf(m_CurrVehicleSet >=0 && m_CurrVehicleSet < iVehicleSetscount, "m_CurrVehicleSet %d is invalid", m_CurrVehicleSet))
	{
		// Here we have to get the current vehicle set we want to use
		CConditionalVehicleSet* pVehicleSet = CDispatchData::FindConditionalVehicleSet((*m_pDispatchVehicleSets)[m_CurrVehicleSet].GetHash());
		if(pVehicleSet)
		{	
			// Get the vehicle names array and choose a random one from the entries
			const atArray<atHashWithStringNotFinal>& vehicleModelNames = pVehicleSet->GetVehicleModelHashes();
			int iVehicleCount = vehicleModelNames.GetCount();
			if(iVehicleCount > 0)
			{
				int iVehicleIndexToSelect = fwRandom::GetRandomNumberInRange(0, iVehicleCount);
				if (assassinsDispatchLevel > AL_Invalid)
				{
					// assassins level is passed from script and starts at 1
					// There should be 3 assassins levels, 2 & 3 use same vehicle (in second slot in the array)
					iVehicleIndexToSelect = Clamp((assassinsDispatchLevel - 1), 0, iVehicleCount-1);
				}

				CModelInfo::GetBaseModelInfoFromHashKey(vehicleModelNames[iVehicleIndexToSelect].GetHash(), &vehicleModelId);
				if(vehicleModelId.IsValid() && CModelInfo::HaveAssetsLoaded(vehicleModelId))
				{
					if(maxVehicleInMap >= 0)
					{
						// Don't spawn if there are already a number of police cars on the map. Exclude parked vehicles.
						if( CVehiclePopulation::CountVehsOfType(vehicleModelId.GetModelIndex(), true, true, true) >= maxVehicleInMap)
						{
							return false;
						}
					}
				}
				else
				{
					return false;
				}
			}
			else
			{
				return false;
			}

			// Get the ped names array and choose a random one from the entries
			const atArray<atHashWithStringNotFinal>& pedModelNames = pVehicleSet->GetPedModelHashes();
			int iPedCount = pedModelNames.GetCount();
			if(iPedCount > 0)
			{
				CModelInfo::GetBaseModelInfoFromHashKey(pedModelNames[fwRandom::GetRandomNumberInRange(0, iPedCount)].GetHash(), &pedModelId);
			}
			else
			{
				pedModelId.Invalidate();
			}

			// Get the object names array and choose a random one from the entries
			const atArray<atHashWithStringNotFinal>& objModelNames = pVehicleSet->GetObjectModelHashes();
			int iObjCount = objModelNames.GetCount();
			if(iObjCount > 0)
			{
				CModelInfo::GetBaseModelInfoFromHashKey(objModelNames[fwRandom::GetRandomNumberInRange(0, iObjCount)].GetHash(), &objModelId);
			}
			else
			{
				objModelId.Invalidate();
			}

			// For dev builds assert that needed peds are loaded
#if __DEV
			for(int j = 0; j < pedModelNames.GetCount(); j++)
			{
				fwModelId iPedModelId;
				CModelInfo::GetBaseModelInfoFromHashKey(pedModelNames[j].GetHash(), &iPedModelId);
				if(iPedModelId.IsValid())
				{
					aiAssert(CModelInfo::HaveAssetsLoaded(iPedModelId));
				}
			}
#endif

			return true;
		}
	}

	return false;
}

void CAssassinsDispatch::SetupAssassinPed(CPed* pPed, int iAssassinsLevel, int pedCount)
{
	if (pPed && iAssassinsLevel > AL_Invalid && iAssassinsLevel < AL_Max)
	{
		CPedIntelligence* pIntelligence = pPed->GetPedIntelligence();
		CCombatBehaviour& rCombatBehaviour = pIntelligence->GetCombatBehaviour();
		CPedWeaponManager* pWeaponMgr = pPed->GetWeaponManager();
		CPedInventory* pInventory = pPed->GetInventory();

		pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_DontBehaveLikeLaw, true);

		float fAccuracy = 1.0f;
		float fShootRate = 0.5f;
		float fHealthMod = 1.0f;

		atHashString sPrimaryWeapon;
		atHashString sSecondaryWeapon;

		switch (iAssassinsLevel)
		{
		case AL_Low:
			{
				fAccuracy = ms_Tunables.m_AssassinLvl1Accuracy * 0.01f;
				fShootRate = ms_Tunables.m_AssassinLvl1ShootRate * 0.01f;
				fHealthMod = ms_Tunables.m_AssassinLvl1HealthMod;

				sPrimaryWeapon = ms_Tunables.m_AssassinLvl1WeaponPrimary;
				sSecondaryWeapon = ms_Tunables.m_AssassinLvl1WeaponSecondary;
			}
			break;
		case AL_Med:
			{
				fAccuracy = ms_Tunables.m_AssassinLvl2Accuracy * 0.01f;
				fShootRate = ms_Tunables.m_AssassinLvl2ShootRate * 0.01f;
				fHealthMod = ms_Tunables.m_AssassinLvl2HealthMod;

				sPrimaryWeapon = ms_Tunables.m_AssassinLvl2WeaponPrimary;
				sSecondaryWeapon = ms_Tunables.m_AssassinLvl2WeaponSecondary;
			}
			break;
		case AL_High:
			{
				fAccuracy = ms_Tunables.m_AssassinLvl3Accuracy * 0.01f;
				fShootRate = ms_Tunables.m_AssassinLvl3ShootRate * 0.01f;
				fHealthMod = ms_Tunables.m_AssassinLvl3HealthMod;

				sPrimaryWeapon = ms_Tunables.m_AssassinLvl3WeaponPrimary;
				sSecondaryWeapon = ms_Tunables.m_AssassinLvl3WeaponSecondary;
			}
			break;
		default:
			break;
		}

		rCombatBehaviour.SetCombatFloat( kAttribFloatWeaponAccuracy, fAccuracy );
		rCombatBehaviour.SetShootRateModifier( fShootRate );
		pPed->SetMaxHealth( pPed->GetMaxHealth() * fHealthMod );
		pPed->SetHealth( pPed->GetMaxHealth() );

		if (pWeaponMgr)
		{
			// For AL_High, equip some of them with primary weapons, and some with secondary, because both weapons are good and they want some variety
			if (sPrimaryWeapon != 0 && (iAssassinsLevel != AL_High || pedCount % 2 == 0))
			{
				pInventory->AddWeaponAndAmmo(sPrimaryWeapon, 200);
				pInventory->SetWeaponUsingInfiniteAmmo(sPrimaryWeapon, true);

				// Ensure current weapon is stored as a backup so we can restore it when getting out
				pWeaponMgr->SetBackupWeapon(sPrimaryWeapon);
			}

			if (sSecondaryWeapon != 0 && (iAssassinsLevel != AL_High || pedCount % 2 == 1))
			{
				pInventory->AddWeaponAndAmmo(sSecondaryWeapon, 200);
				pInventory->SetWeaponUsingInfiniteAmmo(sSecondaryWeapon, true);

				// Ensure current weapon is stored as a backup so we can restore it when getting out
				pWeaponMgr->SetBackupWeapon(sSecondaryWeapon);
			}

			pWeaponMgr->EquipBestWeapon();
		}
	}
}