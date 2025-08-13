// Title	:	Incidents.cpp
// Purpose	:	Defines incidents that exist in the world
// Started	:	13/05/2010

#include "Game/Dispatch/Incidents.h"

// Framework headers
#include "fwnet/netplayer.h"
#include "fwmaths/angle.h"
#include "fwscene/stores/staticboundsstore.h"

// Game headers
#include "Debug/DebugScene.h"
#include "Game/Dispatch/DispatchData.h"
#include "Game/Dispatch/DispatchManager.h"
#include "game/Dispatch/DispatchServices.h"
#include "Game/Dispatch/IncidentManager.h"
#include "Game/zones.h"
#include "modelinfo/PedModelInfo.h"
#include "network/Arrays/NetworkArrayMgr.h"
#include "network/Objects/Entities/NetObjPlayer.h"
#include "Peds/ped.h"
#include "Peds/pedGeometryAnalyser.h"
#include "Peds/PedIntelligence.h"
#include "scene/Entity.h"
#include "scene/portals/portalTracker.h"
#include "task/Combat/CombatManager.h"
#include "task/Combat/TaskNewCombat.h"
#include "vehicleAi/pathfind.h"
#include "Vfx/misc/Fire.h"



AI_OPTIMISATIONS()

// allocate one extra incident to be used by the network array handler  
FW_INSTANTIATE_BASECLASS_POOL_SPILLOVER(CIncident, CIncidentManager::MAX_INCIDENTS+1, 0.35f, atHashString("Incidents",0xddbc66f5), MAX(sizeof(CInjuryIncident), MAX(sizeof(CWantedIncident), sizeof(CFireIncident))) );

//copy the resource counts that we could classify as input parameters
//right now that just means requested resources but that may expand to include
//allocated if it turns out we need it
void CIncident::CopySelectiveResourceCounts(CIncident* pIncident, const CIncident* pIncidentCopyFrom)
{
	if (!pIncident || !pIncidentCopyFrom)
	{
		return;
	}

	for (int i=0; i < DT_Max; i++)
	{
		pIncident->SetRequestedResources(i, pIncidentCopyFrom->GetRequestedResources(i));
	}
}

CIncident::CIncident(IncidentType eType)
: m_vLocation(VEC3_ZERO)
, m_fTimer(0.0f)
, m_bTimerSet(false)
, m_bHasEntity(false)
, m_bHasBeenAddressed(false)
, m_bDisableResourceSpawning(false)
, m_incidentIndex(-1)
, m_incidentID(0)
, m_eType(eType)
{
}

CIncident::CIncident(CEntity* pEntity, const Vector3& vLocation, IncidentType eType, float fTime)
: m_Entity(pEntity)
, m_vLocation(vLocation)
, m_fTimer(fTime)
, m_bTimerSet(fTime>0.0f)
, m_bHasEntity(pEntity != 0)
, m_bHasBeenAddressed(false)
, m_bDisableResourceSpawning(false)
, m_incidentIndex(-1)
, m_incidentID(0)
, m_eType(eType)
{
	// all incident entities must have a networked object in MP
	Assertf(!NetworkInterface::IsGameInProgress() || !pEntity || (pEntity->GetIsDynamic() && ((CDynamicEntity*)pEntity)->GetNetworkObject()), "Incident created with a non-networked entity!");

	for( s32 i = 0; i < DT_Max; i++ )
	{
		m_iDispatchRequested[i] = 0;
		m_iDispatchAllocated[i] = 0;
		m_iDispatchAssigned[i] = 0;
		m_iDispatchArrived[i] = 0;
		m_iDispatchFinished[i] = 0;
	}

	if (NetworkInterface::IsGameInProgress())
	{
		if (aiVerifyf(pEntity, "Incidents without an entity are not permitted in MP"))
		{
			netObject* pNetObj = NetworkUtils::GetNetworkObjectFromEntity(pEntity);
			
			if (aiVerifyf(pNetObj, "Cannot assign an incident to a non-networked entity in MP"))
			{
				if (pNetObj->GetObjectType() == NET_OBJ_TYPE_PLAYER)
				{
					if (aiVerifyf(!pNetObj->IsClone(), "Cannot assign an incident to a remote player in MP"))
					{
						static_cast<CNetObjPlayer*>(pNetObj)->IncrementIncidentNum();
					}
				}

				m_incidentID = GetIncidentIDFromNetworkObjectID(*pNetObj);

				Assertf(m_incidentID != 0, 
					"Created incident with invalid incident id! [%u]  objType %s   incidentId %u", 
					m_incidentID, NetworkUtils::GetNetworkObjectFromEntity(pEntity)? NetworkUtils::GetNetworkObjectFromEntity(pEntity)->GetObjectTypeName() : "None", m_incidentID);
			}
		}
	}
}

CIncident::~CIncident()
{

}

void CIncident::SetIncidentIndex(int index)
{
	aiAssert(index < CIncidentManager::MAX_INCIDENTS);
	m_incidentIndex = static_cast<s16>(index);
}

void CIncident::SetIncidentID(u32 id)
{
	//m_incidentID = static_cast<s16>(id); id of 32769 was getting passed in and converted to 4294934529 / -32567 
	// and blocking any orders associated with this incident from being registered.
	m_incidentID = id;
}

bool CIncident::IsEqual(const CIncident& otherIncident)
{
	if (GetType() == otherIncident.GetType())
	{
		if (GetEntity() && otherIncident.GetEntity())
		{
			if (GetEntity() == otherIncident.GetEntity())
			{
				return true;
			}
		}

		Vector3 diff = m_vLocation - otherIncident.GetLocation();

		if (diff.Mag2() < 0.01f)
		{
			return true;
		}
	}

	return false;
}

bool CIncident::ShouldCountFinishedResources() const
{
	//Check the type.
	switch(GetType())
	{
		case CIncident::IT_Wanted:
		{
			//Wanted incidents should not count their finished resources, because police
			//are commonly created/destroyed fairly quickly during the lifetime of
			//these incidents.  Counting those finished with the incident will result
			//in overflows of the 8-bit counting range.  There is currently no logic
			//dependent on finished resources for the wanted incident -- hopefully
			//it will stay that way.  Alternatively, we could increase the storage to
			//16 bits -- this wouldn't solve the problem, but it would alleviate it
			//in most cases.
			return false;
		}
		default:
		{
			return true;
		}
	}
}

netObject* CIncident::GetNetworkObject() const
{
	if (NetworkInterface::IsGameInProgress())
	{
		if (GetEntity())
		{
			return NetworkUtils::GetNetworkObjectFromEntity(GetEntity());
		}
		else 
		{
			return NetworkInterface::GetNetworkObject(GetNetworkObjectIDFromIncidentID(m_incidentID));
		}
	}

	return NULL;
}

bool CIncident::IsLocallyControlled() const
{
	if (m_incidentIndex == -1)
	{
		// if m_incidentIndex is -1 then the incident has not been registered with the manager yet, and has just been created
		return true;
	}

	return CIncidentManager::GetInstance().IsIncidentLocallyControlled(m_incidentIndex);
}

const netPlayer* CIncident::GetPlayerArbitrator() const
{
	CIncidentsArrayHandler* pIncidentsArrayHandler = NetworkInterface::IsGameInProgress() ? NetworkInterface::GetArrayManager().GetIncidentsArrayHandler() : NULL;

	if (pIncidentsArrayHandler)
	{
		return pIncidentsArrayHandler->GetElementArbitration(m_incidentIndex);
	}
	else
	{
		return NULL;
	}
}

void CIncident::SetDirty()
{
	if (m_incidentIndex != -1 && AssertVerify(IsLocallyControlled()))
	{
		CIncidentManager::GetInstance().SetIncidentDirty(m_incidentIndex);
	}
}

u32 CIncident::GetIncidentIDFromNetworkObjectID(netObject& netObj)
{
	u32 incidentId = (u32)netObj.GetObjectID();

	// we must allow more than one incident to be assigned to the local player, so we need to build a unique id for each incident. Normally the network
	// object id is used but we must combine this with the player's current incident number so that the id is unique.
	if (netObj.GetObjectType() == NET_OBJ_TYPE_PLAYER)
	{
		incidentId |= static_cast<CNetObjPlayer&>(netObj).GetIncidentNum()<<SIZEOF_OBJECTID;
	}

	Assertf(incidentId < static_cast<u32>(((u64)1u<<COrder::SIZEOF_INCIDENT_ID)-1), 
		"Created out of bounds incident id! [%u]  objType %s   objId %u   incidentNum %u", 
		incidentId, netObj.GetObjectTypeName(), (u32)netObj.GetObjectID(), incidentId>>SIZEOF_OBJECTID);

	return incidentId;
}

ObjectId CIncident::GetNetworkObjectIDFromIncidentID(u32 id)
{
	u32 mask = (1<<SIZEOF_OBJECTID)-1;
	return (ObjectId)(id & mask);
}

void CIncident::Serialise(CSyncDataBase& serialiser)
{
	static const unsigned SIZEOF_DISPATCH_UNITS = 8; 
	static const unsigned SIZEOF_TIMER			= 10; 

	bool hasEntity = m_Entity.IsValid();

	SERIALISE_BOOL(serialiser, hasEntity);

	if (hasEntity || serialiser.GetIsMaximumSizeSerialiser())
	{
		m_Entity.Serialise(serialiser, "Entity");
	}
	else
	{
		m_Entity.Invalidate();
	}

	SERIALISE_POSITION(serialiser, m_vLocation, "Position");

	bool hasBeenAddressed = m_bHasBeenAddressed;
	SERIALISE_BOOL(serialiser, hasBeenAddressed, "Has Been Addressed");
	m_bHasBeenAddressed = hasBeenAddressed;

	bool timerSet = m_bTimerSet;
	SERIALISE_BOOL(serialiser, timerSet);
	m_bTimerSet = timerSet;

	if (m_bTimerSet || serialiser.GetIsMaximumSizeSerialiser())
	{
		float timer = m_fTimer < 0.0f ? 0.0f : m_fTimer;
		u32 timerSecs = (u32)timer;

		SERIALISE_UNSIGNED(serialiser, timerSecs, SIZEOF_TIMER, "Timer (secs)");

		m_fTimer = (float)timerSecs;
	}

	for (u32 i=0; i<DT_Max; i++)
	{
		bool hasRequested = (m_iDispatchRequested[i] != 0);
		bool hasArrived	  = (m_iDispatchArrived[i] != 0);
		bool hasFinished  = (m_iDispatchFinished[i] != 0);

		SERIALISE_BOOL(serialiser, hasRequested);
		SERIALISE_BOOL(serialiser, hasArrived);
		SERIALISE_BOOL(serialiser, hasFinished);

#if ENABLE_NETWORK_LOGGING
		const char* typeName = CDispatchManager::GetInstance().GetDispatchTypeName((DispatchType)i);
#endif

		if (hasRequested || serialiser.GetIsMaximumSizeSerialiser())
		{
#if ENABLE_NETWORK_LOGGING
			char logName[100];
			sprintf(logName, "Requested %s", typeName);
#endif
			SERIALISE_UNSIGNED(serialiser, m_iDispatchRequested[i], SIZEOF_DISPATCH_UNITS, logName);
		}
		else
		{
			m_iDispatchRequested[i] = 0;
		}

		if (hasArrived || serialiser.GetIsMaximumSizeSerialiser())
		{
#if ENABLE_NETWORK_LOGGING
			char logName[100];
			sprintf(logName, "Arrived %s", typeName);
#endif
			SERIALISE_UNSIGNED(serialiser, m_iDispatchArrived[i], SIZEOF_DISPATCH_UNITS, logName);
		}
		else
		{
			m_iDispatchArrived[i] = 0;
		}

		if (hasFinished || serialiser.GetIsMaximumSizeSerialiser())
		{
#if ENABLE_NETWORK_LOGGING
			char logName[100];
			sprintf(logName, "Finished %s", typeName);
#endif
			SERIALISE_UNSIGNED(serialiser, m_iDispatchFinished[i], SIZEOF_DISPATCH_UNITS, logName);
		}
		else
		{
			m_iDispatchFinished[i] = 0;
		}
	}
}

// copies serialised incident data to this incident
void CIncident::CopyNetworkInfo(const CIncident& incident)
{
	Assert(GetType() == incident.GetType());

	m_Entity.SetEntityID(incident.m_Entity.GetEntityID());

	m_bHasEntity = m_Entity.IsValid();
	m_vLocation	= incident.m_vLocation;
	m_bTimerSet = incident.m_bTimerSet;
	m_fTimer	= incident.m_fTimer;

	for (u32 i=0; i<DT_Max; i++)
	{
		m_iDispatchRequested[i] = incident.m_iDispatchRequested[i];
		m_iDispatchArrived[i]	= incident.m_iDispatchArrived[i];
		m_iDispatchFinished[i]	= incident.m_iDispatchFinished[i];
	}
}

const CPed* CIncident::GetPed() const
{
	const CEntity* pEntity = GetEntity();

	if( pEntity == NULL || pEntity->GetType() != ENTITY_TYPE_PED )
	{
		return NULL;
	}
	return static_cast<const CPed*>(pEntity);
}

// called when the incident migrates
void CIncident::Migrate() 
{
	// migrate any orders associated with the incident
	NetworkInterface::GetArrayManager().GetOrdersArrayHandler()->IncidentHasMigrated(*this);
}

CIncident::IncidentState CIncident::Update(float UNUSED_PARAM(fTimeStep))
{
	aiAssert(IsLocallyControlled());

	if (m_bHasEntity && !GetEntity())
	{
		return IS_Finished;
	}

	return IS_Active;
}

bool CIncident::UpdateTimer( float fTimeStep )
{
	if( !m_bTimerSet )
	{
		return false;
	}
	if( m_fTimer > 0.0f )
	{
		m_fTimer -= fTimeStep;
	}
	return m_fTimer <= 0.0f;
}

bool CIncident::HasArrivedResources() const
{
	for(int i = 0; i < DT_Max; ++i)
	{
		if(GetArrivedResources(i) > 0)
		{
			return true;
		}
	}

	return false;
}

#if __BANK
dev_float INCIDENT_SPHERE_RADIUS = 0.3f;

void CIncident::DebugRender(Color32 color) const
{
	grcDebugDraw::Sphere(GetLocation(), INCIDENT_SPHERE_RADIUS, color );
}
#endif //__BANK

const float CWantedIncident::kMaxDistanceForNearbyIncidents[WANTED_LEVEL_LAST] = { 0.0f, 100.0f, 100.0f, 133.0f, 166.0f, 200.0f };

CWantedIncident::CWantedIncident()
: CIncident(IT_Wanted)
, m_uTimeOfNextTargetUnreachableViaRoadUpdate(0)
, m_uTimeOfNextTargetUnreachableViaFootUpdate(0)
, m_uTimeLastPoliceHeliDestroyed(0)
, m_uRunningFlags(0)
{

}

CWantedIncident::CWantedIncident( CEntity* pEntity, const Vector3& vLocation )
: CIncident(pEntity, vLocation, IT_Wanted)
, m_uTimeOfNextTargetUnreachableViaRoadUpdate(0)
, m_uTimeOfNextTargetUnreachableViaFootUpdate(0)
, m_uTimeLastPoliceHeliDestroyed(0)
, m_uRunningFlags(0)
{
}

CWantedIncident::~CWantedIncident()
{
}

void CWantedIncident::AdjustResourceAllocations(DispatchType nDispatchType)
{
	aiAssert(IsLocallyControlled());

	//Ensure the dispatch type is used.
	if(!IsDispatchTypeUsed(nDispatchType))
	{
		return;
	}

	//Check if the game is MP.
	if(NetworkInterface::IsGameInProgress())
	{
		//Adjust resource allocations for MP.
		AdjustResourceAllocationsForMP(nDispatchType);
	}
}

void CWantedIncident::AdjustResourceRequests(DispatchType nDispatchType)
{
	aiAssert(IsLocallyControlled());

	//Ensure the dispatch type is used.
	if(!IsDispatchTypeUsed(nDispatchType))
	{
		return;
	}

	//Adjust the resource requests for an unreachable location.
	AdjustResourceRequestsForUnreachableLocation(nDispatchType);
}

void CWantedIncident::AdjustResourceAllocationsForMP(DispatchType nDispatchType)
{
	aiAssert(NetworkInterface::IsGameInProgress());
	aiAssert(IsLocallyControlled());
	aiAssert(IsDispatchTypeUsed(nDispatchType));

	//Ensure resource spawning is not disabled.
	if(GetDisableResourceSpawning())
	{
		return;
	}

	//Find the nearby wanted incidents.
	static const int s_iMaxIncidents = 15;
	CWantedIncident* aNearbyIncidents[s_iMaxIncidents];
	int iNumNearbyIncidents = FindNearbyIncidents(GetMaxDistanceForNearbyIncidents(), aNearbyIncidents, s_iMaxIncidents, GetEntity(), this);
	if(iNumNearbyIncidents <= 0)
	{
		return;
	}

	//Find the highest wanted level.
	int iHighestWantedLevel = GetWantedLevel();
	for(int i = 0; i < iNumNearbyIncidents; ++i)
	{
		iHighestWantedLevel = Max(iHighestWantedLevel, aNearbyIncidents[i]->GetWantedLevel());
	}

	//Find the nearby wanted incidents.
	iNumNearbyIncidents = FindNearbyIncidents(GetMaxDistanceForNearbyIncidents(iHighestWantedLevel), aNearbyIncidents, s_iMaxIncidents, GetEntity(), this);
	if(iNumNearbyIncidents <= 0)
	{
		return;
	}

	//Find the highest wanted level.
	iHighestWantedLevel = GetWantedLevel();
	for(int i = 0; i < iNumNearbyIncidents; ++i)
	{
		iHighestWantedLevel = Max(iHighestWantedLevel, aNearbyIncidents[i]->GetWantedLevel());
	}

	//Ensure the wanted response is valid.
	CWantedResponse* pWantedResponse = CDispatchData::GetWantedResponse(iHighestWantedLevel);
	if(!aiVerifyf(pWantedResponse, "Could not find wanted response data for wanted level: %d.", iHighestWantedLevel))
	{
		return;
	}

	//Calculate the current allocations.
	u8 uCurrentAllocations = (u8)GetAllocatedResources(nDispatchType);

	//Calculate the max assignments.
	u8 uMaxAssignments = (u8)pWantedResponse->CalculateNumPedsToSpawn(nDispatchType);

	//Calculate the nearby assignments.
	u8 uNearbyAssignments = 0;
	for(int i = 0; i < iNumNearbyIncidents; ++i)
	{
		uNearbyAssignments += (u8)aNearbyIncidents[i]->GetAssignedResources(nDispatchType);
	}

	//Calculate the available assignments.
	u8 uAvailableAssignments = (uMaxAssignments > uNearbyAssignments) ? (uMaxAssignments - uNearbyAssignments) : 0;

	//Some dispatch types have a minimum available.
	if(nDispatchType == DT_PoliceAutomobile)
	{
		uAvailableAssignments = Max((u8)2, uAvailableAssignments);
	}

	//Set the allocations.
	u8 uAllocations = Min(uCurrentAllocations, uAvailableAssignments);
	SetAllocatedResources(nDispatchType, uAllocations);
}

void CWantedIncident::AdjustResourceRequestsForUnreachableLocation(DispatchType nDispatchType)
{
	aiAssert(IsLocallyControlled());
	aiAssert(IsDispatchTypeUsed(nDispatchType));

	//Ensure the wanted level exceeds 3*.
	if(GetWantedLevel() <= WANTED_LEVEL3)
	{
		return;
	}

	//Ensure the ped is on foot.
	const CPed* pPed = GetPed();
	if(!pPed || !pPed->GetIsOnFoot())
	{
		return;
	}

	//Ensure the wanted is valid.
	const CWanted* pWanted = pPed->GetPlayerWanted();
	if(!pWanted)
	{
		return;
	}

	//Ensure cops should be dispatched for this player.
	if(pWanted->m_DontDispatchCopsForThisPlayer)
	{
		return;
	}

	//Check if the target is unreachable.
	if(m_uRunningFlags.IsFlagSet(RF_IsTargetUnreachableViaRoad) || m_uRunningFlags.IsFlagSet(RF_IsTargetUnreachableViaFoot))
	{
		//Check the dispatch type.
		int iRequestedResources = GetRequestedResources(nDispatchType);
		switch(nDispatchType)
		{
			case DT_PoliceAutomobile:
			case DT_PoliceHelicopter:
			case DT_PoliceRiders:
			case DT_PoliceRoadBlock:
			case DT_PoliceBoat:
			{
				//No change.
				break;
			}
			case DT_SwatAutomobile:
			{
				//Remove two dispatch units.
				static dev_s32 s_iNumUnitsToRemove = 2;
				iRequestedResources = Max(0, iRequestedResources -
					(s_iNumUnitsToRemove * (int)CDispatchService::GetNumResourcesPerUnit(nDispatchType)));
				break;
			}
			case DT_SwatHelicopter:
			{
				//Add a dispatch unit.
				iRequestedResources += CDispatchService::GetNumResourcesPerUnit(nDispatchType);
				break;
			}
			default:
			{
				aiAssertf(false, "Unknown dispatch type: %d", nDispatchType);
				return;
			}
		}

		//Set the requested resources.
		SetRequestedResources(nDispatchType, iRequestedResources);
	}
}

bool CWantedIncident::IsIncidentNearby(const float fMaxDistance, const CEntity* pBaseEntity, const CIncident* pBaseIncident)
{
	CWantedIncident* pWantedIncident;

	return FindNearbyIncidents(fMaxDistance, &pWantedIncident, 1, pBaseEntity, pBaseIncident) > 0;
}

int CWantedIncident::FindNearbyIncidents(const float fMaxDistance, CWantedIncident** aNearbyIncidents, const int iMaxNearbyIncidents, const CEntity* pBaseEntity, const CIncident* pBaseIncident)
{
	//Get the position.
	Vec3V vPosition;
	
	if(pBaseEntity)
	{
		vPosition = pBaseEntity->GetTransform().GetPosition();
	}
	else if(pBaseIncident)
	{
		vPosition = VECTOR3_TO_VEC3V(pBaseIncident->GetLocation());
	}
	else
	{
		const CPed* pLocalPlayer = CGameWorld::FindLocalPlayer();

		Assert(pLocalPlayer);

		if(pLocalPlayer)
		{
			vPosition = pLocalPlayer->GetTransform().GetPosition();
		}
		else
		{
			return 0;
		}
	}

	//Keep track of the nearby incidents.
	int iNumNearbyIncidents = 0;

	//Cache the max distance.
	ScalarV scMaxDistSq = ScalarVFromF32(square(fMaxDistance));

	//Iterate over the incidents.
	for(int i = 0; i < CIncidentManager::GetInstance().GetMaxCount(); ++i)
	{
		//Ensure the incident is valid.
		CIncident* pIncident = CIncidentManager::GetInstance().GetIncident(i);
		if(!pIncident)
		{
			continue;
		}

		//Ensure the incident is not the same.
		if(pBaseIncident == pIncident)
		{
			continue;
		}

		//Ensure the incident is wanted.
		if(pIncident->GetType() != IT_Wanted)
		{
			continue;
		}

		//Ensure the incident is nearby.
		Vec3V vOtherPosition = pIncident->GetEntity() ?
			pIncident->GetEntity()->GetTransform().GetPosition() : VECTOR3_TO_VEC3V(pIncident->GetLocation());
		ScalarV scDistSq = DistSquared(vPosition, vOtherPosition);
		if(IsGreaterThanAll(scDistSq, scMaxDistSq))
		{
			continue;
		}

		//Add the nearby incident.
		aNearbyIncidents[iNumNearbyIncidents++] = static_cast<CWantedIncident *>(pIncident);
		if(iNumNearbyIncidents >= iMaxNearbyIncidents)
		{
			break;
		}
	}

	return iNumNearbyIncidents;
}

bool CWantedIncident::ShouldIgnoreCombatEvent(const CPed& rPed, const CPed& rTarget)
{
	//Ensure the ped is random.
	if(!rPed.PopTypeIsRandom())
	{
		return false;
	}

	//Ensure the ped is law enforcement.
	if(!rPed.IsLawEnforcementPed())
	{
		return false;
	}

	//Ensure this is MP.
	if(!NetworkInterface::IsGameInProgress())
	{
		return false;
	}

	//Ensure the ped has an order.
	const COrder* pOrder = rPed.GetPedIntelligence()->GetOrder();
	if(!pOrder)
	{
		return false;
	}

	//Ensure the incident is valid.
	const CIncident* pIncident = pOrder->GetIncident();
	if(!pIncident)
	{
		return false;
	}

	//Ensure the incident ped is valid.
	const CPed* pIncidentPed = pIncident->GetPed();
	if(!pIncidentPed)
	{
		return false;
	}

	//Ensure the incident ped is a player.
	if(!pIncidentPed->IsAPlayerPed())
	{
		return false;
	}

	//Ensure the target is a player.
	if(!rTarget.IsAPlayerPed())
	{
		return false;
	}

	//Ensure the players do not match.
	if(pIncidentPed == &rTarget)
	{
		return false;
	}

	return true;
}

int CWantedIncident::GetWantedLevel() const
{
	//Ensure the ped is valid.
	const CPed* pPed = GetPed();
	if(!pPed)
	{
		return WANTED_CLEAN;
	}

	//Ensure the wanted is valid.
	CWanted* pWanted = pPed->GetPlayerWanted();
	if(!pWanted)
	{
		return WANTED_CLEAN;
	}

	return pWanted->GetWantedLevel();
}

bool CWantedIncident::IsDispatchTypeUsed(DispatchType nDispatchType) const
{
	switch(nDispatchType)
	{
		case DT_PoliceAutomobile:
		case DT_PoliceHelicopter:
		case DT_PoliceRiders:
		case DT_PoliceRoadBlock:
		case DT_PoliceBoat:
		case DT_SwatAutomobile:
		case DT_SwatHelicopter:
		{
			return true;
		}
		default:
		{
			return false;
		}
	}
}

bool CWantedIncident::IsTargetUnreachableViaFoot() const
{
	//Ensure the ped is valid.
	const CPed* pPed = GetPed();
	if(!pPed)
	{
		return false;
	}

	//Count the peds in target unreachable.
	int iCount = CCombatManager::GetInstance().GetCombatTaskManager()->CountPedsInCombatStateWithTarget(*pPed, CTaskCombat::State_TargetUnreachable);

	//Calculate the minimum count.
	int iMinCount = Clamp((GetWantedLevel() + 1), 2, 5);

	//Check if the count exceeds the minimum.
	if(iCount >= iMinCount)
	{
		return true;
	}

	return false;
}

bool CWantedIncident::IsTargetUnreachableViaRoad() const
{
	//Ensure the entity is valid.
	const CEntity* pEntity = GetEntity();
	if(!pEntity)
	{
		return false;
	}

	//Ensure the spawn helper is valid.
	const CDispatchSpawnHelper* pSpawnHelper = CDispatchService::GetSpawnHelpers().FindSpawnHelperForEntity(*pEntity);
	if(!pSpawnHelper)
	{
		return false;
	}

	//Check if the spawn helper failed to find spawn points.
	if(pSpawnHelper->FailedToFindSpawnPoint())
	{
		return true;
	}

	//Check if the dispatch node is valid.
	const CPathNode* pNode = ThePaths.FindNodePointerSafe(pSpawnHelper->GetDispatchNode());
	if(pNode)
	{
		//Get the node position.
		Vec3V vNodePosition;
		pNode->GetCoors(RC_VECTOR3(vNodePosition));

		//Check if collision is not loaded around the node.
		if(!g_StaticBoundsStore.GetBoxStreamer().HasLoadedAboutPos(vNodePosition, fwBoxStreamerAsset::FLAG_STATICBOUNDS_MOVER))
		{
			return true;
		}

		//Check if the node is too far from the entity.
		ScalarV scDistSq = DistSquared(pEntity->GetTransform().GetPosition(), vNodePosition);
		static dev_float s_fMaxDistance = 150.0f;
		ScalarV scMaxDistSq = ScalarVFromF32(square(s_fMaxDistance));
		if(IsGreaterThanAll(scDistSq, scMaxDistSq))
		{
			return true;
		}
	}

	return false;
}

void CWantedIncident::UpdateTargetUnreachable()
{
	//Update target unreachable via road.
	UpdateTargetUnreachableViaRoad();

	//Update target unreachable via foot.
	UpdateTargetUnreachableViaFoot();
}

void CWantedIncident::UpdateTargetUnreachableViaFoot()
{
	//Ensure we should do an update.
	u32 uCurrentTime = fwTimer::GetTimeInMilliseconds();
	bool bDoUpdate = !m_uRunningFlags.IsFlagSet(RF_IsTargetUnreachableViaRoad) &&
		(uCurrentTime >= m_uTimeOfNextTargetUnreachableViaFootUpdate);
	if(!bDoUpdate)
	{
		return;
	}

	//Update the flag.
	bool bIsTargetUnreachableViaFoot = IsTargetUnreachableViaFoot();
	m_uRunningFlags.ChangeFlag(RF_IsTargetUnreachableViaFoot, bIsTargetUnreachableViaFoot);

	//Check if the target is unreachable.
	u32 uTimeBetweenUpdates;
	if(bIsTargetUnreachableViaFoot)
	{
		static dev_u32 s_uTimeBetweenUpdates = 45000;
		uTimeBetweenUpdates = s_uTimeBetweenUpdates;
	}
	else
	{
		static dev_u32 s_uTimeBetweenUpdates = 3000;
		uTimeBetweenUpdates = s_uTimeBetweenUpdates;
	}

	//Set the timer.
	m_uTimeOfNextTargetUnreachableViaFootUpdate = uCurrentTime + uTimeBetweenUpdates;
}

void CWantedIncident::UpdateTargetUnreachableViaRoad()
{
	//Ensure we should do an update.
	u32 uCurrentTime = fwTimer::GetTimeInMilliseconds();
	bool bDoUpdate = (uCurrentTime >= m_uTimeOfNextTargetUnreachableViaRoadUpdate);
	if(!bDoUpdate)
	{
		return;
	}

	//Update the flag.
	m_uRunningFlags.ChangeFlag(RF_IsTargetUnreachableViaRoad, IsTargetUnreachableViaRoad());

	//Set the timer.
	static dev_u32 s_uTimeBetweenUpdates = 3000;
	m_uTimeOfNextTargetUnreachableViaRoadUpdate = uCurrentTime + s_uTimeBetweenUpdates;
}

void CWantedIncident::RequestDispatchServices()
{
	//Ensure the ped is valid.
	const CPed* pPed = GetPed();
	if(!pPed)
	{
		return;
	}
	
	//Ensure the wanted is valid.
	CWanted* pWanted = pPed->GetPlayerWanted();
	if(!pWanted)
	{
		return;
	}
	
	//Ensure the wanted level is not clean.
	eWantedLevel nWantedLevel = pWanted->GetWantedLevel();
	if(nWantedLevel == WANTED_CLEAN)
	{
		return;
	}
	aiAssert(nWantedLevel >= 0 && nWantedLevel < WANTED_LEVEL_LAST);
	
	//Ensure cops should be dispatched for this player.
	if(pWanted->m_DontDispatchCopsForThisPlayer)
	{
		return;
	}
	
	//Ensure the wanted response is valid.
	CWantedResponse* pWantedResponse = CDispatchData::GetWantedResponse(nWantedLevel);
	if(!aiVerifyf(pWantedResponse, "Could not find wanted response data, check build/common/data/wanted.meta"))
	{
		return;
	}

	//Request the dispatch response.
	for(int i = 0; i < DT_Max; ++i)
	{
		//Ensure the dispatch type is used.
		if(!IsDispatchTypeUsed((DispatchType)i))
		{
			continue;
		}

		//Calculate the number of peds to spawn.
		SetRequestedResources(i, pWantedResponse->CalculateNumPedsToSpawn((DispatchType)i));
	}
}

CIncident::IncidentState CWantedIncident::Update( float fTimeStep )
{
	aiAssert(IsLocallyControlled());

	const CPed* pPed = GetPed();
	if( pPed == NULL || pPed->GetPlayerWanted() == NULL )
	{
		return IS_Finished;
	}

	eWantedLevel wantedLevel = pPed->GetPlayerWanted()->GetWantedLevel();
	// If the player is no longer wanted remove it from the system
	if(wantedLevel == WANTED_CLEAN)
	{
		return IS_Finished;
	}

	// keep the search centre updated
	SetLocation(pPed->GetPlayerWanted()->m_CurrentSearchAreaCentre);

	//Update the target unreachable states.
	UpdateTargetUnreachable();

	//Check if we are in MP.
	if(NetworkInterface::IsGameInProgress())
	{
		//Take turns spawning in MP.
		Vec3V vPosition = GetEntity() ? GetEntity()->GetTransform().GetPosition() : VECTOR3_TO_VEC3V(GetLocation());
		static dev_float s_fMaxDistance = 100.0f;
		static dev_float s_fDurationOfTurns = 0.25f;
		static dev_float s_fTimeBetweenTurns = 0.05f;
		bool bDisableResourceSpawning = !NetworkInterface::IsLocalPlayersTurn(vPosition, s_fMaxDistance, s_fDurationOfTurns, s_fTimeBetweenTurns);
		SetDisableResourceSpawning(bDisableResourceSpawning);
	}
	else
	{
		//No need to take turns in SP.
		SetDisableResourceSpawning(false);
	}
	
	return CIncident::Update(fTimeStep);
}

void CWantedIncident::UpdateRemote( float UNUSED_PARAM(fTimeStep) )
{
	const CPed* pPed = GetPed();
	
	if (pPed && pPed->GetPlayerWanted())
	{
		// keep the search centre updated
		SetLocation(pPed->GetPlayerWanted()->m_CurrentSearchAreaCentre);
	}
}


CArrestIncident::CArrestIncident()
: CIncident(IT_Arrest)
{

}

CArrestIncident::CArrestIncident( CEntity* pEntity, const Vector3& vLocation )
: CIncident(pEntity, vLocation,IT_Arrest)
{
}

CArrestIncident::~CArrestIncident()
{
}

void CArrestIncident::RequestDispatchServices()
{
	const CPed* pPed = GetPed();

	if (pPed)
	{
		SetRequestedResources(DT_PoliceVehicleRequest, 2);
	}
}

CIncident::IncidentState CArrestIncident::Update( float fTimeStep )
{
	const CPed* pPed = GetPed();
	if( pPed == NULL )
	{
		return IS_Finished;
	}

	return CIncident::Update(fTimeStep);
}


int CFireIncident::ms_iCount = 0;
const float CFireIncident::sm_fClusterRangeXY = 50.0f;
const float CFireIncident::sm_fClusterRangeZ = 10.0f;

CFireIncident::CFireIncident()
: CIncident(IT_Fire)
, m_pFireTruck(NULL)
, m_pRemainsForInvestigation(NULL)
, m_fFireIncidentRadius(0.0f)
, m_iSpawnAttempts(0)
{
	// Timer used to keep the fires alive whilst we wait for a fire truck.
	// Only set the the inital spawn timer so if it runs out and we don't spawn 
	// a fire truck then let the fire die out.
	CDispatchService* pFireDispatchService = CDispatchManager::GetInstance().GetDispatch(DT_FireDepartment);
	if(pFireDispatchService)
	{
		m_fKeepFiresAliveTimer = pFireDispatchService->GetSpawnTimer();

		if(!pFireDispatchService->GetTimerTicking())
		{
			pFireDispatchService->ResetSpawnTimer();
		}
	}
}

CFireIncident::CFireIncident(const FireList& rFireList, CEntity* pEntity, const Vector3& vLocation ) 
: CIncident(pEntity, vLocation, IT_Fire, 0.0f)
, m_pFireTruck(NULL)
, m_pRemainsForInvestigation(NULL)
, m_fFireIncidentRadius(0.0f)
, m_fKeepFiresAliveTimer(0.0f)
, m_iSpawnAttempts(0)
{
	++CFireIncident::ms_iCount;

	m_pFires = rFireList;

	// the fire incident could potentially be synced across the network before Update() has been called, so we need to get the
	// fire's location immediately
	// at this point, the centroid hasn't been set so just use an arbitrary fire's point
	CFire* pBestFire = GetFire(0);
	if( aiVerify(pBestFire) )
	{
		Vec3V v3Position = pBestFire->GetPositionWorld();
		SetLocation(RCC_VECTOR3(v3Position));
		m_fFireIncidentRadius = pBestFire->CalcCurrRadius();
	}

	// Timer used to keep the fires alive whilst we wait for a fire truck.
	// Only set the the inital spawn timer so if it runs out and we don't spawn 
	// a fire truck then let the fire die out.
	CDispatchService* pFireDispatchService = CDispatchManager::GetInstance().GetDispatch(DT_FireDepartment);
	if(pFireDispatchService)
	{
		m_fKeepFiresAliveTimer = pFireDispatchService->GetSpawnTimer();
		
		if(!pFireDispatchService->GetTimerTicking())
		{
			pFireDispatchService->ResetSpawnTimer();
		}
	}

}

CFireIncident::~CFireIncident()
{
	--CFireIncident::ms_iCount;
}

CFire* CFireIncident::GetBestFire(CPed* pPed) const
{
	if (m_pFires.GetCount() == 0)
	{
		return NULL;
	}

	//right now just return the closest fire
	CFire* pBestFire = NULL;
	float fBestFireScore = LARGE_FLOAT;
	for (int i = 0; i < m_pFires.GetCount(); i++)
	{
		if (!m_pFires[i].pFire)
		{
			continue;
		}

		if (!m_pFires[i].pFire->GetFlag(FIRE_FLAG_IS_ACTIVE))
		{
			continue;
		}

		//Keep attending the same fire if it's valid
		if( m_pFires[i].pPedResponding == pPed)
		{
			return m_pFires[i].pFire;
		}

		//If someone is attending this fire then don't attend it
		if (m_pFires[i].pPedResponding != NULL)
		{
			continue;
		}

		//Check there isn't another fire that is already being attended within range
		bool bIgnoreFire = false;
		for (int j = 0; j < m_pFires.GetCount(); j++)
		{
			if (j != i && m_pFires[j].pPedResponding != NULL && m_pFires[j].pPedResponding != pPed)
			{
				const float fDistToFireSqr = MagSquared(m_pFires[j].pFire->GetPositionWorld() - m_pFires[i].pFire->GetPositionWorld()).Getf();
				if (fDistToFireSqr < 1.0f)
				{
					bIgnoreFire = true;
					break;
				}
			}
		}

		if (bIgnoreFire)
		{
			continue;
		}

		const float fDistToFireSqr = (pPed && m_pFires[i].pPedResponding != pPed) ? MagSquared(pPed->GetTransform().GetPosition() - m_pFires[i].pFire->GetPositionWorld()).Getf()
			: 0.0f;	

		float fScoreMultiplier = 1.0f;
		if (m_pFires[i].pFire->GetEntity() )
		{
			if (m_pFires[i].pFire->GetEntity()->GetIsTypePed())
			{
				fScoreMultiplier = 1.0f / 50.0f;
			}
			else if (m_pFires[i].pFire->GetEntity()->GetIsTypeVehicle())
			{
				fScoreMultiplier = 1.0f / 10.0f;
			}
		}

		float fScore = fDistToFireSqr * fScoreMultiplier;

		if (fScore < fBestFireScore)
		{
			pBestFire = m_pFires[i].pFire;
			fBestFireScore = fScore;
		}
	}

	return pBestFire;
}

void CFireIncident::AddFire(CFire* pFire)
{
	Assert(GetNumFires() < MaxFiresPerIncident);

	CFireInfo fireInfo(pFire);
	m_pFires.Push(fireInfo);
}

void CFireIncident::FindAllFires()
{
	if (GetNumFires() == 0)
	{
		for (u32 i=0; i<g_fireMan.GetNumActiveFires(); i++)
		{
			CFire * pFire = g_fireMan.GetActiveFire(i);

			if (pFire && !pFire->GetReportedIncident() && ShouldClusterWith(pFire))
			{
				AddFire(pFire);
				pFire->SetReportedIncident(this);
			}
		}
	}
}

bool CFireIncident::RemoveFire(CFire* pFire)
{
	CFireInfo tempFireInfo(pFire);
	const int index = m_pFires.Find(tempFireInfo);
	if (index < 0)
	{
		return false;
	}

	m_pFires.DeleteFast(index);

	return true;
}

CFire* CFireIncident::GetFire(s32 index) const
{
	if (m_pFires.GetCount() == 0)
	{
		return NULL;
	}

	Assert(index < m_pFires.GetCount() && index >= 0);

	return m_pFires[index].pFire;
}

CPed* CFireIncident::GetPedAttendingFire(s32 index) const
{
	if (m_pFires.GetCount() == 0)
	{
		return NULL;
	}

	Assert(index < m_pFires.GetCount() && index >= 0);

	return m_pFires[index].pPedResponding.Get();
}

s32 CFireIncident::GetNumFires() const
{
	return m_pFires.GetCount();
}

bool CFireIncident::AreAnyFiresActive() const
{
	for (int i = 0; i < m_pFires.GetCount(); i++)
	{
		if (m_pFires[i].pFire->GetFlag(FIRE_FLAG_IS_ACTIVE))
		{
			return true;
		}
	}

	return false;
}

bool CFireIncident::ShouldClusterWith(CFire* pFire) const
{
	if (!pFire)
	{
		return false;
	}

	if (m_pFires.IsFull())
	{
		return false;
	}

	//if we found this fire in the list, don't add it again
	CFireInfo tempFireInfo(pFire);
	if (m_pFires.Find(tempFireInfo) >= 0)
	{
		return false;
	}

	const float fXYRangeSqr = sm_fClusterRangeXY * sm_fClusterRangeXY;

	for (int i = 0; i < m_pFires.GetCount(); i++)
	{
		Vec3V deltaV = pFire->GetPositionWorld() - m_pFires[i].pFire->GetPositionWorld();
		const float distZ = fabsf(deltaV.GetZf());

		//flatten
		deltaV.SetZ(ScalarV(V_ZERO));

		const ScalarV magSquaredV = MagSquared(deltaV);
		const float distXYSqr = magSquaredV.Getf();
		
		if (distZ < sm_fClusterRangeZ && distXYSqr < fXYRangeSqr)
		{
			return true;
		}
	}

	// if the fire is close enough to the fire incident entity it can also be clustered with this incident
	const CDynamicEntity* pDynamicEntity = SafeCast(const CDynamicEntity, GetEntity());

	if (pDynamicEntity && pDynamicEntity->IsNetworkClone())
	{
		Vec3V deltaV = pFire->GetPositionWorld() - pDynamicEntity->GetTransform().GetPosition();
		const float distZ = fabsf(deltaV.GetZf());

		//flatten
		deltaV.SetZ(ScalarV(V_ZERO));

		const ScalarV magSquaredV = MagSquared(deltaV);
		const float distXYSqr = magSquaredV.Getf();

		if (distZ < sm_fClusterRangeZ && distXYSqr < fXYRangeSqr)
		{
			return true;
		}
	}

	return false;
}

//how many are already responding to this fire?
int CFireIncident::GetNumFightingFire(CFire* pFire) const
{
	CFireInfo tempFireInfo(pFire);
	int index = m_pFires.Find(tempFireInfo);

	return GetNumFightingFire(index);
}

int CFireIncident::GetNumFightingFire(s32 index) const
{
	if (m_pFires.GetCount() == 0)
	{
		return 0;
	}

	Assert(index < m_pFires.GetCount() && index >= 0);

	const CFireInfo& fireInfo = m_pFires[index];
	if ( fireInfo.pPedResponding != NULL )
	{
		return 1;
	}
	return 0;
}

void CFireIncident::IncFightingFireCount(CFire* pFire, CPed* pPed)
{
	if (m_pFires.GetCount() == 0)
	{
		return;
	}

	if (pFire == NULL)
	{
		return;
	}

	CFireInfo tempFireInfo(pFire);
	int index = m_pFires.Find(tempFireInfo);

	if (!aiVerifyf(index < m_pFires.GetCount() && index >= 0, "CFire not found in the Fire incident!"))
	{
		return;
	}

	Assert(m_pFires[index].pPedResponding.Get() == NULL || m_pFires[index].pPedResponding.Get() == pPed);
	m_pFires[index].pPedResponding = pPed;
}

void CFireIncident::DecFightingFireCount(CFire* pFire)
{
	if (m_pFires.GetCount() == 0)
	{
		return;
	}

	if (pFire == NULL)
	{
		return;
	}

	CFireInfo tempFireInfo(pFire);
	int index = m_pFires.Find(tempFireInfo);

	if (index >= m_pFires.GetCount() || index < 0)
	{
		return;
	}

	m_pFires[index].pPedResponding = NULL;
}

CPed* CFireIncident::GetPedRespondingToFire(CFire* pFire)
{
	CFireInfo tempFireInfo(pFire);
	int index = m_pFires.Find(tempFireInfo);

	if (index >= m_pFires.GetCount() || index < 0)
	{
		return NULL;
	}

	return m_pFires[index].pPedResponding;
}

#if __BANK
void CFireIncident::DebugRender(Color32 color) const
{
	for (int i = 0; i < m_pFires.GetCount(); i++)
	{
		grcDebugDraw::Line(GetLocation(), VEC3V_TO_VECTOR3(m_pFires[i].pFire->GetPositionWorld()), color);
		grcDebugDraw::Sphere(m_pFires[i].pFire->GetPositionWorld(), INCIDENT_SPHERE_RADIUS, color );

		Vector3 vTextPos = VEC3V_TO_VECTOR3(m_pFires[i].pFire->GetPositionWorld());
		vTextPos.z += 0.5f;
		char numFightingText[8];
		int textoutput = 0;
		if (m_pFires[i].pPedResponding)
		{
			textoutput = 1;
		}
		sprintf(numFightingText, "%d", textoutput);
		grcDebugDraw::Text(vTextPos, Color_purple, numFightingText);
	}
	
	Vector3 vTextPos = GetLocation();
	vTextPos.z += 0.5f;
	char numFightingText[8];
	sprintf(numFightingText, "%d", m_pFires.GetCount());
	grcDebugDraw::Text(vTextPos, Color_red, numFightingText);
	grcDebugDraw::Sphere(GetLocation(), m_fFireIncidentRadius, Color_green, false );

}
#endif //__BANK

void CFireIncident::UpdateFires(float fTimeStep )
{
	Vec3V vCentroidTemp(V_ZERO);
	u32 numFiresCounted = 0;

	for (int i = 0; i < m_pFires.GetCount(); i++)
	{
		if (m_pFires[i].pFire == NULL)
		{
			m_pFires.DeleteFast(i);
			i--;	//evaluate the same slot
			continue;
		}

		//if we know about any vehicles that are on fire, save in case we get here
		//after it's gone out.  we'll then play a different sequence where
		//the firefighters investigate the remains
		if (!m_pRemainsForInvestigation.Get() && m_pFires[i].pFire->GetEntity() && m_pFires[i].pFire->GetEntity()->GetIsTypeVehicle())
		{
			m_pRemainsForInvestigation = m_pFires[i].pFire->GetEntity();
		}

		vCentroidTemp += m_pFires[i].pFire->GetPositionWorld();
		++numFiresCounted;
	}

	//only replace the centroid if we actually still have valid fires
	//we want the position to persist after all fires have gone out
	//in order to support the firefighters approaching the scene of a past fire
	//for investigation
	if (numFiresCounted > 0)
	{
		vCentroidTemp /= ScalarV((float) numFiresCounted);
		SetLocation(RCC_VECTOR3(vCentroidTemp));
	}

	//Update the current size of the fire incident
	float fMaxRadius = 0.0f;
	for (int i = 0; i < m_pFires.GetCount(); i++)
	{
		if (m_pFires[i].pFire)
		{
			Vector3 vRadius = GetLocation() - VEC3V_TO_VECTOR3(m_pFires[i].pFire->GetPositionWorld());
			float fRadius = vRadius.Mag() + m_pFires[i].pFire->CalcCurrRadius();
			if ( fRadius > fMaxRadius)
			{
				fMaxRadius = fRadius;
			}
		}
	}

	m_fFireIncidentRadius = fMaxRadius;

	//Keep fires alive whilst the timer is still ticking.
	CDispatchService* pFireDispatchService = CDispatchManager::GetInstance().GetDispatch(DT_FireDepartment);
	if(pFireDispatchService)
	{
		if(m_fKeepFiresAliveTimer > 0.0f)
		{
			m_fKeepFiresAliveTimer -= fTimeStep;
			for (int i=0; i < GetNumFires(); i++)
			{
				CFire* pFire = GetFire(i);
				if (pFire)
				{
					FireType_e fireType = pFire->GetFireType();
					if (fireType == FIRETYPE_TIMED_VEH_WRECKED
						|| fireType == FIRETYPE_TIMED_VEH_GENERIC
						|| fireType == FIRETYPE_TIMED_OBJ_GENERIC)
					{
						pFire->RegisterInUseByDispatch();
					}
				}
			}
		}
	}

}

void CFireIncident::RequestDispatchServices()
{
	// We want to do whatever we can to make sure only fire resources are requested, so clear out the requested resources before setting them for the fire
	for( s32 i = 0; i < DT_Max; i++ )
	{
		SetRequestedResources(i, 0);
	}

	SetRequestedResources(DT_FireDepartment, NumPedsPerDispatch);
}

CIncident::IncidentState CFireIncident::Update( float fTimeStep )
{
	aiAssert(IsLocallyControlled());

	UpdateFires(fTimeStep);

	const bool bFiresGone = GetNumFires() == 0;

// 	if (bFiresGone)
// 	{
// 		return IS_Finished;
// 	}

	const bool bResourcesHaveBeenDispatched = GetAssignedResources(DT_FireDepartment) > 0;
	const bool bResourcesHaveFinishedWithIncident = GetFinishedResources(DT_FireDepartment) > 0 && GetFinishedResources(DT_FireDepartment) >= GetArrivedResources(DT_FireDepartment);
	if( UpdateTimer(fTimeStep) || 
		 (bFiresGone && (!bResourcesHaveBeenDispatched || bResourcesHaveFinishedWithIncident)))
	{
		//Reset the timer if failed to spawn a fire truck and we're the last incident.
		CDispatchService* pFireDispatchService = CDispatchManager::GetInstance().GetDispatch(DT_FireDepartment);
		if(pFireDispatchService)
		{
			int iNumberFireIncidents = 0;
			for(int i = 0 ; i < CIncidentManager::GetInstance().GetIncidentCount(); i++)
			{
				CIncident* pIncident = CIncidentManager::GetInstance().GetIncident(i);
				if(pIncident && pIncident->GetType() == CIncident::IT_Fire)
				{
					++iNumberFireIncidents;
				}
			}

			if(iNumberFireIncidents <= 1)
			{
				pFireDispatchService->ResetSpawnTimer();
			}
		}

		return IS_Finished;
	}

	return CIncident::Update(fTimeStep);
}


void CFireIncident::UpdateRemote( float fTimeStep )
{
	UpdateFires(fTimeStep);
}

CScriptIncident::CScriptIncident()
: CIncident(IT_Scripted)
, m_fIdealSpawnDistance(-1.0f)
, m_UseExistingPedsOnFoot(true)
, m_uOverrideRelGroupHash(0)
, m_iAssassinDispatchLevel(AL_Invalid)
{
}

CScriptIncident::CScriptIncident( CEntity* pEntity, const Vector3& vLocation, float fTime, bool bUseExistingPedsOnFoot, u32 uOverrideRelGroupHash, int iAssassinDispatchLevel)
: CIncident(pEntity, vLocation, IT_Scripted, fTime)
, m_fIdealSpawnDistance(-1.0f)
, m_UseExistingPedsOnFoot(bUseExistingPedsOnFoot)
, m_uOverrideRelGroupHash(uOverrideRelGroupHash)
, m_iAssassinDispatchLevel(iAssassinDispatchLevel)
{

}

CScriptIncident::~CScriptIncident()
{

}

void CScriptIncident::RequestDispatchServices()
{
	// these services will have already been requested just after the incident was created
}

CIncident::IncidentState CScriptIncident::Update( float fTimeStep )
{
	aiAssert(IsLocallyControlled());

	// Check to see if all requested services have responded.
	bool bAllServicesArrived = true;
	for( s32 i = 0; i < DT_Max; i++ )
	{
		if( GetRequestedResources(i) > 0 && GetArrivedResources(i) == 0 )
		{
			bAllServicesArrived = false;
		}
	}

	// Once all requested services have arrived, begin counting the timer down if set.
	if( bAllServicesArrived )
	{
		if( UpdateTimer(fTimeStep) )
		{
			return IS_Finished;
		}
	}

	if( GetEntity() )
	{
		SetLocation(VEC3V_TO_VECTOR3(GetEntity()->GetTransform().GetPosition()));
	}

	return CIncident::Update(fTimeStep);
}


void CScriptIncident::UpdateRemote( float UNUSED_PARAM(fTimeStep) )
{
	if( GetEntity() )
	{
		SetLocation(VEC3V_TO_VECTOR3(GetEntity()->GetTransform().GetPosition()));
	}
}

bool CScriptIncident::GetShouldAllocateNewResourcesForIncident( s32 iDispatchType ) const
{
	return GetArrivedResources(iDispatchType) < GetRequestedResources(iDispatchType);
}

CInjuryIncident::CInjuryIncident( )
: CIncident(IT_Injury)
{
	m_pEmergencyVehicle = NULL;
	m_pRespondingPed = NULL;
	m_pDrivingPed = NULL;
	m_iSpawnAttempts = 0;

	CDispatchService* pAmbulanceDispatchService = CDispatchManager::GetInstance().GetDispatch(DT_AmbulanceDepartment);
	if(pAmbulanceDispatchService)
	{
		if(!pAmbulanceDispatchService->GetTimerTicking())
		{
			pAmbulanceDispatchService->ResetSpawnTimer();
		}
	}
}

CInjuryIncident::CInjuryIncident( CEntity* pEntity, const Vector3& vLocation)
: CIncident(pEntity, vLocation, IT_Injury, 0.0f/*30.0f*/)
 , m_pEmergencyVehicle(NULL)
 , m_pRespondingPed(NULL)
 , m_pDrivingPed(NULL)
 , m_iSpawnAttempts(0)
{
	CDispatchService* pAmbulanceDispatchService = CDispatchManager::GetInstance().GetDispatch(DT_AmbulanceDepartment);
	if(pAmbulanceDispatchService)
	{
		if(!pAmbulanceDispatchService->GetTimerTicking())
		{
			pAmbulanceDispatchService->ResetSpawnTimer();
		}
	}
}

CInjuryIncident::~CInjuryIncident()
{

}

void CInjuryIncident::RequestDispatchServices()
{
	SetRequestedResources(DT_AmbulanceDepartment, NumPedsPerDispatch);
}

CIncident::IncidentState CInjuryIncident::Update( float fTimeStep )
{
	const CPed* pPed = GetPed();
	if (!CInjuryIncident::IsPedValidForInjuryIncident(pPed, GetHasBeenAddressed(), false))
	{
		return IS_Finished;
	}


	if(!ShouldDispatchAmbulance())
	{
		CDispatchService* pAmbulanceDispatchService = CDispatchManager::GetInstance().GetDispatch(DT_AmbulanceDepartment);
		if(pAmbulanceDispatchService)
		{
			pAmbulanceDispatchService->ResetSpawnTimer();
		}
	}

	// Once all requested services have arrived, begin counting the timer down if set.
	if( GetArrivedResources(DT_AmbulanceDepartment) > 0 )
	{
		if( UpdateTimer(fTimeStep) || GetFinishedResources(DT_AmbulanceDepartment) == GetArrivedResources(DT_AmbulanceDepartment))
		{
			return IS_Finished;
		}
	}

	SetLocation(VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()));

	return CIncident::Update(fTimeStep);
}

void CInjuryIncident::UpdateRemote( float UNUSED_PARAM(fTimeStep) )
{
	const CPed* pPed = GetPed();

	if (pPed)
	{
		SetLocation(VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()));
	}
}

bool CInjuryIncident::ShouldDispatchAmbulance()
{
	CPed* pPlayerPed = FindPlayerPed();
	if (pPlayerPed && pPlayerPed->GetPedIntelligence()->IsBattleAware())
	{
		float fdistanceSq = (GetLocation() - VEC3V_TO_VECTOR3(pPlayerPed->GetTransform().GetPosition())).Mag2();
		
		if(fdistanceSq < rage::square(100.0f))
		{
			return false;
		}
	}
	return true;
}

bool CInjuryIncident::IsPedValidForInjuryIncident(const CPed* pPed, bool bVictimhasBeenHealed, bool bInitalCheck)
{
	 if( pPed == NULL)
	 {
	 	return false;
	 }

	 if (pPed->IsAPlayerPed())
	 {
		 return false;
	 }

	 if (!pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_AllowMedicsToReviveMe))
	 {
		 return false;
	 }

	 if (pPed->GetIsInWater())
	 {
		 return false;
	 }

	 CPhysical* pPhysical = pPed->GetGroundPhysical();
	 if (pPhysical && pPhysical->GetIsTypeVehicle())
	 {
		return false;
	 }

	 if (pPed->IsInOrAttachedToCar())
	 {
		 return false;
	 }

	 if(pPed->GetIsInWater())
	 {
		 return false;
	 }

	 if (!bVictimhasBeenHealed && !pPed->IsInjured()) 
	 {
		return false;
	 }

	 if (!bVictimhasBeenHealed && pPed->GetIsInVehicle())
	 {
		return false;
	 }

	 if (!pPed->GetPedModelInfo()->GetAmbulanceShouldRespondTo())
	 {
		 return false;
	 }

	 CPortalTracker* pTrack = const_cast<CPortalTracker*>(pPed->GetPortalTracker());
	 if(pTrack && pTrack->IsInsideInterior())
	 {
		 return false;
	 }
	 
	 Vector3 PedPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());

	 if (!ThePaths.AreNodesLoadedForPoint(PedPos.x, PedPos.y))
	 {
		 return false;
	 }

     //This is expensive so only do the check when the event is created
	 //No need to do the check on update as we presume the road node won't be lost. 
	 //If the body is moving we really shouldn't be going to it
	 if (bInitalCheck)
	 {		 
		 if(pTrack)
		 {
			 pTrack->RequestRescanNextUpdate();
		 }

		 //Setting these to the same but leaving in the code as it may well be changed later.
		 float fDistanceToOnNode = (CMapAreas::GetAreaIndexFromPosition(PedPos) == 0 ) ? 40.0f : 40.0f;
		 
		 // don't add incidents if the ambulance won't be able to reach it
		 CNodeAddress CurrentNode;
		 CurrentNode = ThePaths.FindNodeClosestToCoors(PedPos, fDistanceToOnNode,  CPathFind::IgnoreSwitchedOffNodes);

		 if (CurrentNode.IsEmpty() )
		 {
			 return false;
		 }
		 else
		 {
			 //Guard against distance as FindNodeClosestToCoors doesn't use actual measurements
			 const CPathNode * pJunctionNode = ThePaths.FindNodePointerSafe(CurrentNode);
			 if(!pJunctionNode)
			 {
				 return false;
			 }

			 Vector3 vJunctionPos;
			 pJunctionNode->GetCoors(vJunctionPos);
			 if((PedPos-vJunctionPos).Mag2() > rage::square(fDistanceToOnNode))
			 {
				 return false;
			 }
		 }
	 }

	 return true;
}
