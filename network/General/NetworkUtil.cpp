//
// name:        NetworkUtil.cpp
// description: Network utilities: various bits & bobs used by network code
// written by:  John Gurney
//

#include "network/General/NetworkUtil.h"

// rage headers
#include "profile/timebars.h"

// framework headers
#include "fwnet/neteventmgr.h"
#include "fwnet/netutils.h"
#include "mathext/linearalgebra.h"

// game headers
#include "camera/CamInterface.h"
#include "camera/gameplay/GameplayDirector.h"
#include "camera/viewports/ViewportManager.h"
#include "debug/debug.h"
#include "frontend/PauseMenu.h"
#include "frontend/NewHud.h"
#include "network/Network.h"
#include "network/Debug/NetworkDebug.h"
#include "network/events/NetworkEventTypes.h"
#include "network/players/NetGamePlayer.h"
#include "network/objects/entities/NetObjPlayer.h"
#include "Peds/ped.h"
#include "Peds/PedIntelligence.h"
#include "PedGroup/PedGroup.h"
#include "Pickups/Pickup.h"
#include "scene/world/GameWorld.h"
#include "fwsys/timer.h"
#include "vehicles/train.h"
#include "vehicles/planes.h"
#include "vehicles/vehicle.h"
#include "vehicles/vehiclepopulation.h"
#include "text/text.h"

NETWORK_OPTIMISATIONS()

static const float ms_NetworkCreationScopedMultiple	= 4.0f;
static const float ms_NetworkPlayerPredictionTime	= 1.0f;

CNetworkRequester::CNetworkRequester() :
m_pObject(NULL),
m_requestTimer(0)
{
}

CNetworkRequester::CNetworkRequester(netObject* pObject) :
m_pObject(pObject),
m_requestTimer(0),
m_bWasPermanent(false)
{
    RequestObject(pObject);
}

CNetworkRequester::~CNetworkRequester()
{
    if (m_pObject && !m_pObject->IsClone() && !m_bWasPermanent)
    {
        // allow the object to migrate again
        m_pObject->SetGlobalFlag(netObject::GLOBALFLAG_PERSISTENTOWNER, false);
    }
}

void CNetworkRequester::Reset()
{
    m_pObject      = 0;
    m_requestTimer = 0;
}

void CNetworkRequester::RequestObject(netObject* pObject)
{
    gnetAssert(pObject);

    m_pObject = pObject;

    if (pObject->IsClone())
    {
        Request();
    }
    else
    {
        // stop the object migrating, we need it soon
        m_bWasPermanent = pObject->IsGlobalFlagSet(netObject::GLOBALFLAG_PERSISTENTOWNER);
        pObject->SetGlobalFlag(netObject::GLOBALFLAG_PERSISTENTOWNER, true);
    }
}

void CNetworkRequester::Process()
{
    if (!IsRequestSuccessful())
    {
        m_requestTimer -= fwTimer::GetSystemTimeStepInMilliseconds();

        if (m_requestTimer <= 0)
        {
            Request();
        }
    }
}

bool CNetworkRequester::IsRequestSuccessful() const
{
    if (!m_pObject)
        return true;

    if (!m_pObject->IsClone())
        return true;

    return false;
}

bool CNetworkRequester::IsInUse() const
{
    return m_pObject != 0;
}

netObject *CNetworkRequester::GetNetworkObject()
{
    return m_pObject;
}

void CNetworkRequester::Request()
{
    gnetAssert(m_pObject);

    if (m_pObject->IsClone())
    {
        CRequestControlEvent::Trigger(m_pObject NOTFINAL_ONLY(, "Network Requester"));
        m_requestTimer = NETWORK_REQUESTER_FREQ;
    }
}

//-------------------------------------------------------------------------
// Constructor
//-------------------------------------------------------------------------
CSyncedEntity::CSyncedEntity() :
m_entity(0),
m_entityID(NETWORK_INVALID_OBJECT_ID)
{
}

//-------------------------------------------------------------------------
// Constructor
//-------------------------------------------------------------------------
CSyncedEntity::CSyncedEntity(CEntity *entity) :
m_entity(0),
m_entityID(NETWORK_INVALID_OBJECT_ID)
{
	SetEntity(entity);
}

//-------------------------------------------------------------------------
// Constructor
//-------------------------------------------------------------------------
CSyncedEntity::CSyncedEntity(const CSyncedEntity& syncedEntity) :
m_entity(syncedEntity.m_entity),
m_entityID(syncedEntity.m_entityID)
{
}

//-------------------------------------------------------------------------
// Gets the entity pointer
//-------------------------------------------------------------------------
CEntity *CSyncedEntity::GetEntity() 
{
	if(NetworkInterface::IsGameInProgress())
	{
		if(m_entity == 0 && m_entityID != NETWORK_INVALID_OBJECT_ID)
		{
			SetEntityFromEntityID();
		}
	}

	return m_entity.Get(); 
}

//-------------------------------------------------------------------------
// Gets the entity pointer
//-------------------------------------------------------------------------
const CEntity *CSyncedEntity::GetEntity() const
{
	return const_cast<CSyncedEntity *>(this)->GetEntity(); 
}

//-------------------------------------------------------------------------
// Sets the entity id
//-------------------------------------------------------------------------
void CSyncedEntity::SetEntity(CEntity *entity) 
{
	m_entity = entity;

	if(m_entity && NetworkInterface::IsGameInProgress())
	{
		SetEntityIDFromEntity();
	}
}

//-------------------------------------------------------------------------
// Gets the entity id
//-------------------------------------------------------------------------
ObjectId &CSyncedEntity::GetEntityID()
{ 
	if (m_entity && m_entityID == NETWORK_INVALID_OBJECT_ID && GetEntity() && NetworkInterface::IsGameInProgress())
	{
		SetEntityIDFromEntity();
	}

	return m_entityID; 
}

//-------------------------------------------------------------------------
// Gets the entity id
//-------------------------------------------------------------------------
const ObjectId &CSyncedEntity::GetEntityID() const
{
	return const_cast<CSyncedEntity *>(this)->GetEntityID();
}

//-------------------------------------------------------------------------
// Sets the entity pointer
//-------------------------------------------------------------------------
void CSyncedEntity::SetEntityID(const ObjectId &entityID) 
{ 
	m_entityID = entityID; 

	if(NetworkInterface::IsGameInProgress())
	{
		SetEntityFromEntityID(); 
	}
}

//-------------------------------------------------------------------------
// Sets the entity pointer from the entity network ID
//-------------------------------------------------------------------------
void CSyncedEntity::SetEntityFromEntityID()
{
	netObject *networkObject = NetworkInterface::GetNetworkObject(m_entityID);
	m_entity = networkObject ? networkObject->GetEntity() : 0;
}

//-------------------------------------------------------------------------
// Sets the entity network ID from the entity pointer
//-------------------------------------------------------------------------
void CSyncedEntity::SetEntityIDFromEntity()
{
	if (aiVerify(NetworkInterface::IsGameInProgress()))
	{
		netObject *networkObject = m_entity ? NetworkUtils::GetNetworkObjectFromEntity(m_entity) : 0;
		m_entityID = networkObject ? networkObject->GetObjectID() : NETWORK_INVALID_OBJECT_ID;
	}
}

//-------------------------------------------------------------------------
// Constructor
//-------------------------------------------------------------------------
CSyncedPed::CSyncedPed() :
m_pedEntityID(0),
m_pedID(NETWORK_INVALID_OBJECT_ID)
{
}

//-------------------------------------------------------------------------
// Constructor
//-------------------------------------------------------------------------
CSyncedPed::CSyncedPed(CPed *ped) :
m_pedEntityID(0),
m_pedID(NETWORK_INVALID_OBJECT_ID)
{
	SetPed(ped);
}

//-------------------------------------------------------------------------
// Gets the ped pointer
//-------------------------------------------------------------------------
CPed *CSyncedPed::GetPed() 
{
	if(NetworkInterface::IsGameInProgress())
	{
		if((m_pedEntityID == 0 || CPed::GetPool()->GetAt(m_pedEntityID) == NULL) && m_pedID != NETWORK_INVALID_OBJECT_ID)
		{
			SetPedFromPedID();
		}
	}

	return m_pedEntityID != 0 ? CPed::GetPool()->GetAt(m_pedEntityID) : NULL;
}

//-------------------------------------------------------------------------
// Gets the ped pointer
//-------------------------------------------------------------------------
CPed *CSyncedPed::GetPed() const
{
	return const_cast<CSyncedPed *>(this)->GetPed(); 
}

//-------------------------------------------------------------------------
// Sets the ped pointer
//-------------------------------------------------------------------------
void CSyncedPed::SetPed(CPed *ped) 
{
	m_pedEntityID = ped ? CPed::GetPool()->GetIndexFast(ped) : 0;

	if(m_pedEntityID && NetworkInterface::IsGameInProgress())
	{
		SetPedIDFromPed();
	}
}

//-------------------------------------------------------------------------
// Gets the ped id
//-------------------------------------------------------------------------
ObjectId &CSyncedPed::GetPedID() 
{ 
	if (m_pedEntityID && m_pedID == NETWORK_INVALID_OBJECT_ID && CPed::GetPool()->GetAt(m_pedEntityID) && NetworkInterface::IsGameInProgress())
	{
		SetPedIDFromPed();
	}

	return m_pedID; 
}

//-------------------------------------------------------------------------
// Gets the entity id
//-------------------------------------------------------------------------
const ObjectId &CSyncedPed::GetPedID() const
{
	return const_cast<CSyncedPed *>(this)->GetPedID();
}

//-------------------------------------------------------------------------
// Sets the ped id
//-------------------------------------------------------------------------
void CSyncedPed::SetPedID(const ObjectId &pedID) 
{ 
	m_pedID = pedID; 

	if (NetworkInterface::IsGameInProgress())
	{
		SetPedFromPedID(); 
	}
}

//-------------------------------------------------------------------------
// Sets the ped pointer from the ped network ID
//-------------------------------------------------------------------------
void CSyncedPed::SetPedFromPedID()
{
	netObject *networkObject = NetworkInterface::GetNetworkObject(m_pedID);
	CPed *pPed = NetworkUtils::GetPedFromNetworkObject(networkObject);
	m_pedEntityID = pPed ? CPed::GetPool()->GetIndexFast(pPed) : 0;
}

//-------------------------------------------------------------------------
// Sets the ped network ID from the ped pointer
//-------------------------------------------------------------------------
void CSyncedPed::SetPedIDFromPed()
{
	if (aiVerify(NetworkInterface::IsGameInProgress()))
	{
		CPed *pPed = m_pedEntityID != 0 ? CPed::GetPool()->GetAt(m_pedEntityID) : NULL;
		netObject *networkObject = pPed ? pPed->GetNetworkObject() : 0;
		m_pedID = networkObject ? networkObject->GetObjectID() : NETWORK_INVALID_OBJECT_ID;
	}
}

//-------------------------------------------------------------------------
// Constructor
//-------------------------------------------------------------------------
CSyncedVehicle::CSyncedVehicle() :
m_vehicle(0),
m_vehicleID(NETWORK_INVALID_OBJECT_ID)
{
}

//-------------------------------------------------------------------------
// Constructor
//-------------------------------------------------------------------------
CSyncedVehicle::CSyncedVehicle(CVehicle *vehicle) :
m_vehicle(0),
m_vehicleID(NETWORK_INVALID_OBJECT_ID)
{
	SetVehicle(vehicle);
}

//-------------------------------------------------------------------------
// Gets the vehicle pointer
//-------------------------------------------------------------------------
CVehicle *CSyncedVehicle::GetVehicle() 
{
	if(NetworkInterface::IsGameInProgress())
	{
		if(m_vehicle == 0 && m_vehicleID != NETWORK_INVALID_OBJECT_ID)
		{
			SetVehicleFromVehicleID();
		}
	}

	return m_vehicle; 
}

//-------------------------------------------------------------------------
// Gets the ped pointer
//-------------------------------------------------------------------------
const CVehicle *CSyncedVehicle::GetVehicle() const
{
	return const_cast<CSyncedVehicle *>(this)->GetVehicle(); 
}

//-------------------------------------------------------------------------
// Sets the vehicle pointer
//-------------------------------------------------------------------------
void CSyncedVehicle::SetVehicle(CVehicle *vehicle) 
{
	m_vehicle = vehicle;

	if(m_vehicle && NetworkInterface::IsGameInProgress())
	{
		SetVehicleIDFromVehicle();
	}
}
//-------------------------------------------------------------------------
// Gets the vehicle id
//-------------------------------------------------------------------------
ObjectId &CSyncedVehicle::GetVehicleID() 
{ 
	if (m_vehicle && m_vehicleID == NETWORK_INVALID_OBJECT_ID && GetVehicle() && NetworkInterface::IsGameInProgress())
	{
		SetVehicleIDFromVehicle();
	}

	return m_vehicleID; 
}

//-------------------------------------------------------------------------
// Gets the vehicle id
//-------------------------------------------------------------------------
const ObjectId &CSyncedVehicle::GetVehicleID() const
{
	return const_cast<CSyncedVehicle *>(this)->GetVehicleID();
}

//-------------------------------------------------------------------------
// Sets the ped id
//-------------------------------------------------------------------------
void CSyncedVehicle::SetVehicleID(const ObjectId &vehicleID) 
{ 
	m_vehicleID = vehicleID; 

	if (NetworkInterface::IsGameInProgress())
	{
		SetVehicleFromVehicleID(); 
	}
}
//-------------------------------------------------------------------------
// Sets the vehicle pointer from the vehicle network ID
//-------------------------------------------------------------------------
void CSyncedVehicle::SetVehicleFromVehicleID()
{
	netObject *networkObject = NetworkInterface::GetNetworkObject(m_vehicleID);
	m_vehicle = NetworkUtils::GetVehicleFromNetworkObject(networkObject);
}

//-------------------------------------------------------------------------
// Sets the vehicle network ID from the vehicle pointer
//-------------------------------------------------------------------------
void CSyncedVehicle::SetVehicleIDFromVehicle()
{
	if (aiVerify(NetworkInterface::IsGameInProgress()))
	{
		netObject *networkObject = m_vehicle ? m_vehicle->GetNetworkObject() : 0;
		m_vehicleID = networkObject ? networkObject->GetObjectID() : NETWORK_INVALID_OBJECT_ID;
	}
}

namespace NetworkUtils
{
   //Returns the position of a network player regardless of whether the player is physical/non physical
    Vector3 GetNetworkPlayerPosition(const CNetGamePlayer &networkPlayer)
    {
        if(networkPlayer.GetPlayerPed())
        {
            return VEC3V_TO_VECTOR3(networkPlayer.GetPlayerPed()->GetTransform().GetPosition());
        }
        else
        {
			nonPhysicalPlayerDataBase* pdata = networkPlayer.GetNonPhysicalData();
			return pdata ? static_cast<CNonPhysicalPlayerData *>(pdata)->GetPosition() : VEC3_ZERO;
        }
    }

    //Returns the viewport of a network player
    grcViewport *GetNetworkPlayerViewPort(const CNetGamePlayer &networkPlayer)
    {
        grcViewport *viewport = 0;

        if(networkPlayer.GetPlayerPed() && networkPlayer.GetPlayerPed()->GetNetworkObject())
        {
            CNetObjPlayer *netObjPlayer = SafeCast(CNetObjPlayer, networkPlayer.GetPlayerPed()->GetNetworkObject());

            viewport = netObjPlayer ? netObjPlayer->GetViewport() : 0;
        }

        return viewport;
    }

    //Returns a name for the specified object type
    const char *GetObjectTypeName(const NetworkObjectType objectType, bool isMissionObject)
    {
        const char *name = "Unknown Type";

        switch(objectType)
        {
        case NET_OBJ_TYPE_AUTOMOBILE:
            name = isMissionObject ? "SCRIPT_AUTOMOBILE" : "AUTOMOBILE";
            break;
        case NET_OBJ_TYPE_BIKE:
            name = isMissionObject ? "SCRIPT_BIKE" : "BIKE";
            break;
        case NET_OBJ_TYPE_BOAT:
            name = isMissionObject ? "SCRIPT_BOAT" : "BOAT";
            break;
		case NET_OBJ_TYPE_DOOR:
			name = isMissionObject ? "SCRIPT_DOOR" : "DOOR";
			break;
        case NET_OBJ_TYPE_HELI:
            name = isMissionObject ? "SCRIPT_HELI" : "HELI";
            break;
        case NET_OBJ_TYPE_OBJECT:
            name = isMissionObject ? "SCRIPT_OBJECT" : "OBJECT";
            break;
        case NET_OBJ_TYPE_PED:
            name = isMissionObject ? "SCRIPT_PED" : "PED";
            break;
        case NET_OBJ_TYPE_PICKUP:
            name = isMissionObject ? "SCRIPT_PICKUP" : "PICKUP";
            break;
        case NET_OBJ_TYPE_PICKUP_PLACEMENT:
            name = isMissionObject ? "SCRIPT_PICKUP_PLACEMENT" : "PICKUP_PLACEMENT";
            break;
        case NET_OBJ_TYPE_GLASS_PANE:
            name = isMissionObject ? "SCRIPT_GLASS_PANE" : "GLASS_PANE";
            break;
		case NET_OBJ_TYPE_PLANE:
			name = isMissionObject ? "SCRIPT_PLANE" : "PLANE";
			break;
		case NET_OBJ_TYPE_SUBMARINE:
			name = isMissionObject ? "SCRIPT_SUBMARINE" : "SUBMARINE";
			break;
        case NET_OBJ_TYPE_PLAYER:
            name = "PLAYER";
            break;
        case NET_OBJ_TYPE_TRAILER:
            name = isMissionObject ? "SCRIPT_TRAILER" : "TRAILER";
            break;
        case NET_OBJ_TYPE_TRAIN:
            name = isMissionObject ? "SCRIPT_TRAIN" : "TRAIN";
            break;
        default:
            gnetAssertf(0, "Unknown type supplied to NetworkUtils::GetObjectTypeName()!");
        }

        return name;
    }

    // Returns a CObject pointer from the specified network object if it is of the correct type
    CObject *GetCObjectFromNetworkObject(netObject *networkObject)
    {
        CObject *object = 0;

        if(networkObject && networkObject->GetEntity() && networkObject->GetEntity()->GetIsTypeObject())
        {
            object = static_cast<CObject *>(networkObject->GetEntity());
        }

        return object;
    }

    const CObject *GetCObjectFromNetworkObject(const netObject *networkObject)
    {
        const CObject *object = GetCObjectFromNetworkObject(const_cast<netObject*>(networkObject));
        return object;
    }

	// Returns a CPickup pointer from the specified network object if it is of the correct type
	CPickup *GetPickupFromNetworkObject(netObject *networkObject)
	{
		CPickup *pickup = 0;

		if(networkObject && networkObject->GetEntity() && networkObject->GetEntity()->GetIsTypeObject() &&
			static_cast<CObject*>(networkObject->GetEntity())->m_nObjectFlags.bIsPickUp)
		{
			pickup = static_cast<CPickup *>(networkObject->GetEntity());
		}

		return pickup;
	}

    const CPickup *GetPickupFromNetworkObject(const netObject *networkObject)
	{
		const CPickup *pickup = GetPickupFromNetworkObject(const_cast<netObject*>(networkObject));
		return pickup;
	}

	// Returns a CPickupPlacement pointer from the specified network object if it is of the correct type
	CPickupPlacement *GetPickupPlacementFromNetworkObject(netObject *networkObject)
	{
		CPickupPlacement *pickupPlacement = 0;

		if (networkObject && networkObject->GetObjectType() == NET_OBJ_TYPE_PICKUP_PLACEMENT)
		{
			pickupPlacement = static_cast<CNetObjPickupPlacement *>(networkObject)->GetPickupPlacement();
		}

		return pickupPlacement;
	}

	const CPickupPlacement *GetPickupPlacementFromNetworkObject(const netObject *networkObject)
	{
		const CPickupPlacement *pickupPlacement = GetPickupPlacementFromNetworkObject(const_cast<netObject*>(networkObject));
		return pickupPlacement;
	}

	// Returns a ped pointer from the specified network object if it is of the correct type
    CPed *GetPedFromNetworkObject(netObject *networkObject)
    {
        CPed *ped = 0;

        if(networkObject && networkObject->GetEntity() && networkObject->GetEntity()->GetIsTypePed())
        {
            ped = static_cast<CPed *>(networkObject->GetEntity());
        }

        return ped;
    }

    const CPed *GetPedFromNetworkObject(const netObject *networkObject)
    {
        const CPed *ped = GetPedFromNetworkObject(const_cast<netObject*>(networkObject));
        return ped;
    }

    // Returns a ped pointer from the specified network object if it is of the correct type
    CPed *GetPlayerPedFromNetworkObject(netObject *networkObject)
    {
        CPed *ped = GetPedFromNetworkObject(networkObject);
		return (ped && ped->IsPlayer()) ? ped : NULL;
    }

    // Returns a ped pointer from the specified network object if it is of the correct type
    const CPed *GetPlayerPedFromNetworkObject(const netObject *networkObject)
    {
        const CPed *ped = GetPlayerPedFromNetworkObject(const_cast<netObject*>(networkObject));
		return ped;
    }

    // Returns a vehicle pointer from the specified network object if it is of the correct type
    CVehicle *GetVehicleFromNetworkObject(netObject *networkObject)
    {
        CVehicle *vehicle = 0;

        if(networkObject && networkObject->GetEntity() && networkObject->GetEntity()->GetIsTypeVehicle())
        {
            vehicle = static_cast<CVehicle *>(networkObject->GetEntity());
        }

        return vehicle;
    }

    const CVehicle *GetVehicleFromNetworkObject(const netObject *networkObject)
    {
        const CVehicle *vehicle = GetVehicleFromNetworkObject(const_cast<netObject*>(networkObject));
        return vehicle;
    }

    CTrain *GetTrainFromNetworkObject(netObject *networkObject)
    {
        CTrain   *train   = 0;
        CVehicle *vehicle = GetVehicleFromNetworkObject(networkObject);

        if(vehicle && vehicle->InheritsFromTrain())
        {
            train = static_cast<CTrain *>(vehicle);
        }

        return train;
    }

    const CTrain *GetTrainFromNetworkObject(const netObject *networkObject)
    {
        const CTrain *train = GetTrainFromNetworkObject(const_cast<netObject*>(networkObject));
        return train;
    }

    CPlane *GetPlaneFromNetworkObject(netObject *networkObject)
    {
        CPlane   *plane   = 0;
        CVehicle *vehicle = GetVehicleFromNetworkObject(networkObject);

        if(vehicle && vehicle->InheritsFromPlane())
        {
            plane = static_cast<CPlane *>(vehicle);
        }

        return plane;
    }

    const CPlane *GetPlaneFromNetworkObject(const netObject *networkObject)
    {
        const CPlane *plane = GetPlaneFromNetworkObject(const_cast<netObject*>(networkObject));
        return plane;
    }

	// name:        IsVisibleToPlayer
	// description: Checks to see if a given position and radius is visible to supplied player.
	// Parameters   : pPlayer - player to check against
	//				  position - the potential position of a network object
	//                radius - this object's radius
	// Returns      : true if the position is in scope of supplied player
	bool IsVisibleToPlayer(const CNetGamePlayer* pPlayer, const Vector3& position, const float radius, const float distance, const bool bIgnoreExtendedRange)
	{
#if __BANK
		if (NetworkDebug::ShouldIgnoreVisibilityChecks())
		{
			return false;
		}
#endif
		if (!pPlayer)
		{
			return false;
		}

		if(pPlayer->GetPlayerPed())
		{
			grcViewport *pViewport = NULL; 

			if(!pPlayer->GetPlayerPed()->IsNetworkClone())
			{
				pViewport = &gVpMan.GetGameViewport()->GetNonConstGrcViewport();
			}
			else
			{
				pViewport = ((CNetObjPlayer*)pPlayer->GetPlayerPed()->GetNetworkObject())->GetViewport();
			}

			float fEvaluateOnScreenDistance = distance;
			if (pViewport)
			{
				fEvaluateOnScreenDistance *= ((CNetObjPlayer*)pPlayer->GetPlayerPed()->GetNetworkObject())->GetExtendedScopeFOVMultiplier(pViewport->GetFOVY());
			}

			if(Unlikely(((CNetObjPlayer*)pPlayer->GetPlayerPed()->GetNetworkObject())->IsUsingExtendedPopulationRange() && !bIgnoreExtendedRange))
			{
				// player has extended population range, so they can see extra far
				fEvaluateOnScreenDistance *= ms_NetworkCreationScopedMultiple;					
			}

			const Vector3 vPlayerFocusPosition = NetworkInterface::GetPlayerFocusPosition(*pPlayer);
			Vector3 diff = vPlayerFocusPosition - position;

			if (diff.Mag() < fEvaluateOnScreenDistance && pViewport && pViewport->IsSphereVisible(position.x, position.y, position.z, radius))
			{
				return true;
			}
		}

		return false;
	}

    // name:        IsVisibleToAnyRemotePlayer
    // description: Checks to see if a given position and radius is visible to any player's players.
    // Parameters   : position - the potential position of a network object
    //                radius - this object's radius
    //                pVisiblePlayer - the first player found who can see this position
    // Returns      : true if the position is in scope with any remote players
    bool IsVisibleToAnyRemotePlayer(const Vector3& position, const float radius, const float distance, const netPlayer** ppVisiblePlayer, const bool bIgnoreExtendedRange)
    {
#if __BANK
        if (NetworkDebug::ShouldIgnoreVisibilityChecks())
        {
            return false;
        }
#endif

        if (ppVisiblePlayer)
        {
            *ppVisiblePlayer = 0;
        }

        unsigned                 numRemotePhysicalPlayers = netInterface::GetNumRemotePhysicalPlayers();
        const netPlayer * const *remotePhysicalPlayers    = netInterface::GetRemotePhysicalPlayers();

        for(unsigned index = 0; index < numRemotePhysicalPlayers; index++)
        {
	        const CNetGamePlayer *remotePlayer = SafeCast(const CNetGamePlayer, remotePhysicalPlayers[index]);

            if(remotePlayer->GetPlayerPed())
            {
                grcViewport *pViewport = ((CNetObjPlayer*)remotePlayer->GetPlayerPed()->GetNetworkObject())->GetViewport();

				float fEvaluateOnScreenDistance = distance;
				if (pViewport)
				{
					fEvaluateOnScreenDistance *= ((CNetObjPlayer*)remotePlayer->GetPlayerPed()->GetNetworkObject())->GetExtendedScopeFOVMultiplier(pViewport->GetFOVY());
				}

				if(Unlikely(((CNetObjPlayer*)remotePlayer->GetPlayerPed()->GetNetworkObject())->IsUsingExtendedPopulationRange() && !bIgnoreExtendedRange))
				{
					// player has extended population range, so they can see extra far
					fEvaluateOnScreenDistance *= ms_NetworkCreationScopedMultiple;					
				}

                Vector3 diff = VEC3V_TO_VECTOR3(remotePlayer->GetPlayerPed()->GetTransform().GetPosition()) - position;

                if (diff.Mag() < distance && pViewport && pViewport->IsSphereVisible(position.x, position.y, position.z, radius))
                {
                    if (ppVisiblePlayer)
                        *ppVisiblePlayer = remotePlayer;
                    return true;
                }
            }
        }

        return false;
    }

    static bool IsVisibleOrCloseToNetworkPlayer(const Vector3& position,
                                                const float radius,
                                                const float onScreenDistance,
                                                const float offScreenDistance,
                                                const netPlayer** ppVisiblePlayer,
                                                const bool predictFuturePosition,
                                                const netPlayer * const *playerArray,
                                                const unsigned numPlayers,
												const bool bIgnoreExtendedRange)
    {
        #if __BANK
        if (NetworkDebug::ShouldIgnoreVisibilityChecks())
        {
            return false;
        }
#endif

        if (ppVisiblePlayer)
            *ppVisiblePlayer = NULL;

        for(unsigned index = 0; index < numPlayers; index++)
        {
            const CNetGamePlayer *player = SafeCast(const CNetGamePlayer, playerArray[index]);

            CPed* playerPed = player->GetPlayerPed();

            if(playerPed)
            {
				//If we are dealing with the local player here - need to use the local players real viewport and not the faked viewport created in CNetObjPlayer because that will be incorrect on the local. lavalley.
                grcViewport *pViewport = playerPed->IsNetworkClone() ? ((CNetObjPlayer*)playerPed->GetNetworkObject())->GetViewport() : &gVpMan.GetGameViewport()->GetNonConstGrcViewport();

				const Vector3 vPlayerFocusPosition = NetworkInterface::GetPlayerFocusPosition(*player);
                Vector3 diff = vPlayerFocusPosition - position;

				float dist = diff.Mag2();

				bool bComparePredictedDistance = false;
				float distPrediction = 0.f;

                if(predictFuturePosition && !playerPed->IsLocalPlayer())
                {
					bComparePredictedDistance = true;
                    bool playerInVehicle   = playerPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) && playerPed->GetMyVehicle();
                    Vector3 velocity       = playerInVehicle ? playerPed->GetMyVehicle()->GetVelocity() : playerPed->GetVelocity();
					const Vector3 vPlayerPedPosition = VEC3V_TO_VECTOR3(playerPed->GetTransform().GetPosition());
                    Vector3 futurePosition = vPlayerPedPosition + (velocity * ms_NetworkPlayerPredictionTime); // predict one second into the future
                    diff = futurePosition - position;
					distPrediction = diff.Mag2();
                }

 
				// Not sure about the !pViewport case - seems more natural to treat that as "not visible", but not sure when it happens.
				bool visible = pViewport && pViewport->IsSphereVisible(position.x, position.y, position.z, radius);

				float fEvaluateOnScreenDistance = onScreenDistance;
				if (pViewport)
				{
					fEvaluateOnScreenDistance *= ((CNetObjPlayer*)playerPed->GetNetworkObject())->GetExtendedScopeFOVMultiplier(pViewport->GetFOVY());
				}
				
				if(Unlikely(((CNetObjPlayer*)playerPed->GetNetworkObject())->IsUsingExtendedPopulationRange() && !bIgnoreExtendedRange))
				{
					// player has extended population range, so they can see extra far
					fEvaluateOnScreenDistance *= ms_NetworkCreationScopedMultiple;					
				}
				
                if ((!visible && dist < square(offScreenDistance)) || (visible && dist < square(fEvaluateOnScreenDistance)))
                {
                    if (ppVisiblePlayer)
                        *ppVisiblePlayer = player;
                    return true;
                }

				if (bComparePredictedDistance)
				{
					if ((!visible && distPrediction < square(offScreenDistance)) || (visible && distPrediction < square(fEvaluateOnScreenDistance)))
					{
						if (ppVisiblePlayer)
							*ppVisiblePlayer = player;
						return true;
					}
				}
            }
        }

        return false;
    }

    bool IsVisibleOrCloseToAnyPlayer(const Vector3& position, const float radius, const float onScreenDistance, 
									 const float offScreenDistance, const netPlayer** ppVisiblePlayer, const bool predictFuturePosition, const bool bIgnoreExtendedRange)
    {
        return IsVisibleOrCloseToNetworkPlayer(position, radius, onScreenDistance, offScreenDistance, 
					ppVisiblePlayer, predictFuturePosition, netInterface::GetAllPhysicalPlayers(), netInterface::GetNumPhysicalPlayers(), bIgnoreExtendedRange);
    }

    bool IsVisibleOrCloseToAnyRemotePlayer(const Vector3& position, const float radius, const float onScreenDistance, 
										   const float offScreenDistance, const netPlayer** ppVisiblePlayer, 
										   const bool predictFuturePosition, const bool bIgnoreExtendedRange)
    {
        return IsVisibleOrCloseToNetworkPlayer(position, radius, onScreenDistance, offScreenDistance, 
					ppVisiblePlayer, predictFuturePosition, netInterface::GetRemotePhysicalPlayers(), netInterface::GetNumRemotePhysicalPlayers(), bIgnoreExtendedRange);
    }

    static bool IsCloseToNetworkPlayer(const Vector3& position, const float radius, const netPlayer*& pClosestPlayer, const netPlayer * const *playerArray, const unsigned numPlayers)
    {
#if __BANK
        if (NetworkDebug::ShouldIgnoreVisibilityChecks())
        {
            return false;
        }
#endif

        pClosestPlayer = 0;
        float closestDist = 0.0f;

        for(unsigned index = 0; index < numPlayers; index++)
        {
            const CNetGamePlayer *player = SafeCast(const CNetGamePlayer, playerArray[index]);

            if(player->GetPlayerPed())
            {
                Vector3 diff = NetworkInterface::GetPlayerFocusPosition(*player) - position;

                float dist = diff.Mag();

                if (dist <= radius && (!pClosestPlayer || dist < closestDist))
                {
                    pClosestPlayer = player;
                    closestDist = dist;
                }
            }
        }

        return (pClosestPlayer != 0);
    }

    bool IsCloseToAnyPlayer(const Vector3& position, const float radius, const netPlayer*& pClosestPlayer)
    {
        return IsCloseToNetworkPlayer(position, radius, pClosestPlayer, netInterface::GetAllPhysicalPlayers(), netInterface::GetNumPhysicalPlayers());
    }

    bool IsCloseToAnyRemotePlayer(const Vector3& position, const float radius, const netPlayer*& pClosestPlayer)
    {
        return IsCloseToNetworkPlayer(position, radius, pClosestPlayer, netInterface::GetRemotePhysicalPlayers(), netInterface::GetNumRemotePhysicalPlayers());
    }

    // used as a global parameter for a qsort comparison function
    static Vector3 ms_positionToSortPlayers;
    //
    // Name         :   ComparePlayerDistanceToPosition
    // Purpose      :   qsort comparison function for sorting players relative to a position
    static int ComparePlayerDistanceToPosition(const void *paramA, const void *paramB)
    {
        const CNetGamePlayer *playerA = *((CNetGamePlayer **)(paramA));
        const CNetGamePlayer *playerB = *((CNetGamePlayer**)(paramB));
        gnetAssert(playerA);
        gnetAssert(playerB);

        if(playerA && playerB)
        {
            float distanceSquaredA = (NetworkInterface::GetPlayerFocusPosition(*playerA) - ms_positionToSortPlayers).Mag2();
            float distanceSquaredB = (NetworkInterface::GetPlayerFocusPosition(*playerB) - ms_positionToSortPlayers).Mag2();

            if(distanceSquaredA < distanceSquaredB)
            {
                return -1;
            }
            else if(distanceSquaredA > distanceSquaredB)
            {
                return 1;
            }
        }

        return 0;
    }

    //
    // name:        GetClosestRemotePlayers
    // description: builds an array of players within the given position and radius and can be sorted by distance, returns the number of players added to the array
    u32 GetClosestRemotePlayers(const Vector3& position, const float radius, const netPlayer *closestPlayers[MAX_NUM_PHYSICAL_PLAYERS], bool sortResults, bool alwaysGetPlayerPos)
    {
        u32 numPlayers = 0;

		const bool bCheckDistance = radius < LARGE_FLOAT;
        const float radiusSq = radius * radius;

        unsigned                 numRemotePhysicalPlayers = netInterface::GetNumRemotePhysicalPlayers();
        const netPlayer * const *remotePhysicalPlayers    = netInterface::GetRemotePhysicalPlayers();

        for(unsigned index = 0; index < numRemotePhysicalPlayers; index++)
        {
	        const CNetGamePlayer *remotePlayer = SafeCast(const CNetGamePlayer, remotePhysicalPlayers[index]);

            if(remotePlayer->GetPlayerPed())
            {
				if(bCheckDistance)
				{
					Vector3 diff = NetworkInterface::GetPlayerFocusPosition(*remotePlayer, NULL, alwaysGetPlayerPos) - position;
					float dist = diff.XYMag2();

					if (dist <= radiusSq)
					{
						closestPlayers[numPlayers] = remotePlayer;
						numPlayers++;
					}
				}
				else
				{
					closestPlayers[numPlayers] = remotePlayer;
					numPlayers++;
				}
            }
        }

        if(sortResults)
        {
            // sort them based on distance
            ms_positionToSortPlayers = position;
            qsort(closestPlayers, numPlayers, sizeof(netPlayer *), ComparePlayerDistanceToPosition);
        }

        return numPlayers;
    }

	//
	// name:        GetClosestRemotePlayersInScope
	// description: builds an array of players in scope with the given network object and sorted by distance, returns the number of players added to the array
	u32 GetClosestRemotePlayersInScope(const CNetObjProximityMigrateable& networkObject, const netPlayer *closestPlayers[MAX_NUM_PHYSICAL_PLAYERS], bool sortResults)
	{
		u32 numPlayers = 0;

		unsigned                 numRemotePhysicalPlayers = netInterface::GetNumRemotePhysicalPlayers();
		const netPlayer * const *remotePhysicalPlayers    = netInterface::GetRemotePhysicalPlayers();

		for(unsigned index = 0; index < numRemotePhysicalPlayers; index++)
		{
			const CNetGamePlayer *remotePlayer = SafeCast(const CNetGamePlayer, remotePhysicalPlayers[index]);

			if (networkObject.IsInScope(*remotePlayer))
			{
				closestPlayers[numPlayers] = remotePlayer;
				numPlayers++;
			}
		}

		if(sortResults)
		{
			// sort them based on distance
			ms_positionToSortPlayers = networkObject.GetScopePosition();
			qsort(closestPlayers, numPlayers, sizeof(netPlayer *), ComparePlayerDistanceToPosition);
		}

		return numPlayers;
	}
	
	// gets the distance from our player to another players player
    float  GetDistanceToPlayer(const CNetGamePlayer* pRemotePlayer)
    {
        Vector3 ourPlayerPos = VEC3V_TO_VECTOR3(NetworkInterface::GetLocalPlayer()->GetPlayerPed()->GetTransform().GetPosition());
        Vector3 remotePlayerPos = VEC3V_TO_VECTOR3(pRemotePlayer->GetPlayerPed()->GetTransform().GetPosition());

        Vector3 diff = remotePlayerPos - ourPlayerPos;

        return diff.Mag();
    }

    //
    // name:        CanDisplayPlayerName
    // description: returns whether the player name should be rendered for the specified player ped
    //
    bool CanDisplayPlayerName(CPed *playerPed)
    {
        gnetAssert(playerPed);

        bool canDisplayPlayerNames = true;//m_displayPlayerNames;  // this check now gets done later as we still need to display the dot above the head

        if(playerPed)
        {
			if(NetworkInterface::IsInMPCutscene())
			{
				canDisplayPlayerNames = false;
			}
			else if(playerPed->GetPlayerInfo() && playerPed->GetPlayerInfo()->GetPlayerState() == CPlayerInfo::PLAYERSTATE_IN_MP_CUTSCENE)
			{
				canDisplayPlayerNames = false;	
			}
			else if(playerPed->GetPlayerInfo() && playerPed->GetPlayerInfo()->IsControlsScriptDisabled())
			{
                // we also don't display their name when the script has disabled their controls (i.e. when playing a cutscene)
                canDisplayPlayerNames = false;
            }
            else if(playerPed->IsDead())
            {
                // we also don't display player names when the player ped is dead
                canDisplayPlayerNames = false;
            }
            else if(camInterface::GetGameplayDirector().IsFirstPersonAiming())
            {
                // check for sniper scope & only check things that we want in sinper mode underneath here
                canDisplayPlayerNames = true;
            }
            else if(playerPed->GetIsCrouching())
            {
                // we also don't display player names when the player is crouching
                canDisplayPlayerNames = false;
            }
            else if(playerPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_IN_COVER))
            {
                // we also don't display player names when the player is cover
                canDisplayPlayerNames = false;
            }
        }

        return canDisplayPlayerNames;
    }

    static bool NetTransformWorldPosToScreenPos(const Vector3 & vWorldPos, Vector2 & vScreenPos)
    {
        const grcViewport * pViewport = NULL;

        for (s32 i=0;i<gVpMan.GetNumViewports() && (pViewport == 0);i++)
        {
            CViewport *pVp = gVpMan.GetViewport(i);
            if (pVp && pVp->IsActive() && pVp->IsUsedForNetworking())
            {
                pViewport = &pVp->GetGrcViewport();
            }
        }

        if(!pViewport)
        {
            return false;
        }

        if(pViewport->IsSphereVisible(vWorldPos.x, vWorldPos.y, vWorldPos.z, 0.1f)==cullOutside)
            return false;

        pViewport->Transform(VECTOR3_TO_VEC3V(vWorldPos), vScreenPos.x, vScreenPos.y );

        // convert to 0-1 space:
        vScreenPos.x /= SCREEN_WIDTH;
        vScreenPos.y /= SCREEN_HEIGHT;

        return true;
    }

    bool GetScreenCoordinatesForOHD( const Vector3& position, Vector2 &screenPos, float &scale, const Vector3& offset, float displayRange /*= 100.0f*/, 
		float nearScalingDistance /*= 0.0f*/, float farScalingDistance /*= 100.0f*/, float maxScale /*= 1.0f*/, float minScale /*= 0.2f*/ )
    {
        bool display = false;

        Vector3 vDiff   = position - camInterface::GetPos();
        float   fDist   = vDiff.Mag();

        // need to compensate if the FOV has changed (this happens when aiming with a sniper rifle for example)
        float fDistanceScale = camInterface::GetFov() / g_DefaultFov;
        gnetAssert(fDistanceScale >= 0.0f);

        fDist *= fDistanceScale;

        if(fDist <= displayRange)
        {
            Vector3 WorldCoors = position + offset;
            if (NetTransformWorldPosToScreenPos(WorldCoors, screenPos))
            {
#if __ASSERT
				if(!AssertVerify(nearScalingDistance < farScalingDistance))
				{
					SWAP(nearScalingDistance, farScalingDistance);
				}

				if(!AssertVerify(minScale < maxScale))
				{
					SWAP(minScale, maxScale);
				}

				if(!AssertVerify(nearScalingDistance + 1.0f <= farScalingDistance))
				{
					farScalingDistance = nearScalingDistance + 1.0f;
				}

				if(!AssertVerify(minScale + 0.05f <= maxScale))
				{
					maxScale = minScale + 0.05f;
				}
#endif

				scale = RampValue(fDist, nearScalingDistance, farScalingDistance, maxScale, minScale);

				display = true;
            }
        }

        return display;
    }

    void SetFontSettingsForNetworkOHD(CTextLayout *pTextLayout, float scale, bool bSmallerVersion)
    {
        Vector2 vTextSize = Vector2(0.27f*scale, 0.42f*scale);

        if (bSmallerVersion)
        {
    #define MP_NAME_FONT_SCALER (0.8f)

            vTextSize.x *= MP_NAME_FONT_SCALER;  // make it thinner
        }

        CHudTools::AdjustForWidescreen(WIDESCREEN_FORMAT_SIZE_ONLY, NULL, &vTextSize, NULL);

		pTextLayout->Default();
        pTextLayout->SetScale(&vTextSize);
        pTextLayout->SetWrap(Vector2(-0.5f, 1.5f));
        pTextLayout->SetEdge(true);
    }

	CPed* GetPedFromDamageEntity(CEntity *entity, CEntity* pWeaponDamageEntity)
    {
        CPed* pKillerPed = NULL;

        netObject *networkObject = NetworkUtils::GetNetworkObjectFromEntity(pWeaponDamageEntity);

        if (networkObject)
        {
			pKillerPed = GetPedFromNetworkObject(networkObject);

			if (!pKillerPed)
            {
                CVehicle *damageVehicle = GetVehicleFromNetworkObject(networkObject);

                if (damageVehicle)
                {
                    // a car has killed the player
                    pKillerPed = damageVehicle->GetDriver();

					// vehicle had no driver - give the last driver the points.
					if (!pKillerPed && damageVehicle->GetSeatManager())
					{
						pKillerPed = damageVehicle->GetSeatManager()->GetLastPedInSeat(0);
					}

                    if(pKillerPed == 0)
                    {
                        // vehicle had no driver - give the last player driver the points.
                        unsigned                 numPhysicalPlayers = netInterface::GetNumPhysicalPlayers();
                        const netPlayer * const *allPhysicalPlayers = netInterface::GetAllPhysicalPlayers();

                        for(unsigned index = 0; index < numPhysicalPlayers; index++)
                        {
                            const CNetGamePlayer *player = SafeCast(const CNetGamePlayer, allPhysicalPlayers[index]);

                            if(player->GetPlayerPed())
                            {
                                if (player->GetPlayerPed()->GetMyVehicle() == damageVehicle)
                                {
                                    pKillerPed = player->GetPlayerPed();
                                    break;
                                }
                            }
                        }
                    }

                    // don't award the kill to a player driving a vehicle that has killed one of the passengers
                    if(entity->GetIsTypePed() && damageVehicle->ContainsPed(static_cast<CPed *>(entity)))
                    {
                        pKillerPed = 0;
                    }
                }
            }
		}

		return pKillerPed;
	}

    CNetGamePlayer* GetPlayerFromDamageEntity(CEntity *entity, CEntity *pWeaponDamageEntity)
    {
        CNetGamePlayer* pResult = NULL;

		CPed* pKillerPed = GetPedFromDamageEntity(entity, pWeaponDamageEntity);
		if (pKillerPed)
		{
			// this ped is a player, return his id
			if (pKillerPed->IsPlayer())
			{
				pResult = pKillerPed->GetNetworkObject()->GetPlayerOwner();
			}
			// is the killer a member of the player's team?
			else
			{
				CPedGroup* pPedGroup = pKillerPed->GetPedsGroup();

				if (pPedGroup && pPedGroup->GetGroupMembership()->GetLeader() && pPedGroup->GetGroupMembership()->GetLeader()->IsPlayer())
				{
					pResult = pPedGroup->GetGroupMembership()->GetLeader()->GetNetworkObject()->GetPlayerOwner();
				}
			}
		}

        return pResult;
    }

	CEntity* GetPlayerDamageEntity(const CNetGamePlayer* player, int& weaponHash)
	{
		CEntity* playerDamageEntity = 0;
		CPed* playerPed = player->GetPlayerPed();

		if(playerPed && playerPed->GetNetworkObject())
		{
			CNetObjPhysical *netobjPhysical = SafeCast(CNetObjPhysical, playerPed->GetNetworkObject());

			playerDamageEntity = netobjPhysical->GetLastDamageObjectEntity();
			weaponHash         = netobjPhysical->GetLastDamageWeaponHash();

			if(playerDamageEntity == 0)
			{
				if (playerPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) && playerPed->GetMyVehicle() && playerPed->GetMyVehicle()->GetNetworkObject())
				{
					netobjPhysical = SafeCast(CNetObjPhysical, playerPed->GetMyVehicle()->GetNetworkObject());

					playerDamageEntity = netobjPhysical->GetLastDamageObjectEntity();
					weaponHash         = netobjPhysical->GetLastDamageWeaponHash();
				}
			}
		}

		return playerDamageEntity;
    }

    NetworkColours::NetworkColour GetDebugColourForPlayer(const netPlayer *player)
    {
        int colourIndex = 0;

        if(player)
        {
            colourIndex = player->GetPhysicalPlayerIndex();
        }

        return NetworkColours::GetDefaultPlayerColour(static_cast<PhysicalPlayerIndex>(colourIndex));
    }

#if ENABLE_NETWORK_LOGGING
    const char *GetCanCloneErrorString(unsigned resultCode)
    {
        switch(resultCode)
        {
        case CC_DIFFERENT_BUBBLE:
            return "Target player has different roaming bubble";
        case CC_INVALID_PLAYER_INDEX:
            return "Target player has invalid player index";
        case CC_REMOVE_FROM_WORLD_FLAG:
            return "Remove from world flag is set";
        case CC_HAS_SP_MODEL:
            return "Using single player model";
        default:
            return rage::GetCanCloneErrorString(resultCode);
        }
    }

    const char *GetCanPassControlErrorString(unsigned resultCode)
    {
        switch(resultCode)
        {
        case CPC_FAIL_NO_GAME_OBJECT:
            return "No game object";
        case CPC_FAIL_SPECTATING:
            return "Target player spectating";
        case CPC_FAIL_NOT_SCRIPT_PARTICIPANT:
            return "Not script participant";
		case CPC_FAIL_DEBUG_NO_PROXIMITY:
			return "Debug proximity disabled";
		case CPC_FAIL_DEBUG_NO_OWNERSHIP_CHANGE:
			return "Ownership change disabled";
        case CPC_FAIL_PLAYER_NO_ACCEPT:
            return "Rejecting due to player state";
        case CPC_FAIL_PROXIMITY_TIMER:
            return "Proximity Timer Active";
        case CPC_FAIL_SCRIPT_TIMER:
            return "Script Migration Timer Active";
        case CPC_FAIL_DELETION_TIMER:
            return "Deletion timer active";
        case CPC_FAIL_OVERRIDING_CRITICAL_STATE:
            return "Overriding critical state";
        case CPC_FAIL_IN_CUTSCENE:
            return "In local cutscene";
        case CPC_FAIL_NOT_ADDED_TO_WORLD:
            return "Not added to world yet";
        case CPC_FAIL_REMOVING_FROM_WORLD:
            return "Removing from world";
		case CPC_FAIL_PICKUP_PLACEMENT:
			return "Pickup not cloned";
		case CPC_FAIL_PICKUP_PLACEMENT_DESTROYED:
			return "Pickup Placement being destroyed";
        case CPC_FAIL_PHYSICAL_ATTACH:
            return "Object Attached";
        case CPC_FAIL_PED_TASKS:
            return "Ped Tasks";
        case CPC_FAIL_IS_PLAYER:
            return "Object is a player";
        case CPC_FAIL_IS_TRAIN:
            return "Object is a train";
        case CPC_FAIL_VEHICLE_OCCUPANT:
            return "Vehicle Occupant";
        case CPC_FAIL_VEHICLE_OCCUPANT_CLONE_STATE:
            return "Vehicle Occupant Clone State";
        case CPC_FAIL_VEHICLE_OCCUPANT_SEAT_STATE_UNSYNCED:
            return "Vehicle Occupant Seat State Unsynced";
		case CPC_FAIL_VEHICLE_DRIVER_CANT_MIGRATE:
			return "Script driver can't migrate";
        case CPC_FAIL_VEHICLE_RESPOT:
            return "Respot timer active";
        case CPC_FAIL_VEHICLE_COMPONENT_IN_USE:
            return "Vehicle Components In Use";
        case CPC_FAIL_VEHICLE_PLAYER_DRIVER:
            return "Has Player Driver";
		case CPC_FAIL_VEHICLE_TASK:
			return "Vehicle AI task cannot migrate";
		case CPC_FAIL_CRITICAL_STATE_UNSYNCED:
			return "Critical state is unsynced";
		case CPC_FAIL_BEING_TELEPORTED:
			return "Rejecting due to being teleported";
		case CPC_FAIL_SCHEDULED_FOR_CREATION:
			return "Rejecting due to being scheduled for creation";
		case CPC_FAIL_RESPAWN_IN_PROGRESS:
			return "Rejecting due to network respawn event is in progress";
		case CPC_FAIL_OWNERSHIP_TOKEN_UNSYNCED:
			return "Ownership token is unsynced";
		case CPC_FAIL_VEHICLE_HAS_SCHEDULED_OCCUPANTS:
			return "Vehicle has pending occupants";
		case CPC_FAIL_VEHICLE_IN_ROAD_BLOCK:
			return "Vehicle is in road block";
		case CPC_FAIL_VEHICLE_ATTACHED_TO_CARGOBOB:
			return "Vehicle is attached to cargobob";
		case CPC_FAIL_PHYSICAL_ATTACH_STATE_MISMATCH:
			return "Attachment state mismatch";
		case CPC_FAIL_WRECKED_VEHICLE_IN_AIR:
			return "Wrecked vehicle in the air";
		case CPC_PICKUP_WARPING:
			return "Pickup is warping to an accessible location";
		case CPC_PICKUP_HAS_PLACEMENT:
			return "Pickup has a placement";
		case CPC_PICKUP_COMING_TO_REST:
			return "Pickup coming to rest";
		case CPC_FAIL_PED_GROUP_UNSYNCED:
			return "Ped's group not synced";
		case CPC_FAIL_VEHICLE_GADGET_OBJECT:
			return "Object is part of a vehicle gadget";
		case CPC_FAIL_REMOVE_FROM_WORLD_WHEN_INVISIBLE:
			return "Object is about to be removed when invisible";
		case CPC_PICKUP_ATTACHED_TO_LOCAL_PED:
			return "The pickup is attached to a local ped";
		case CPC_FAIL_CONCEALED:
			return "Physical object is concealed for this player";
		case CPC_FAIL_ATTACHED_CHILD_NOT_CLONED:
			return "Child attachment not cloned for this player";
		case CPC_FAIL_TRAIN_ENGINE_LOCAL:
			return "Train's engine is locally owned";
		default:
            return rage::GetCanPassControlErrorString(resultCode);
        }
    }

    const char *GetCanAcceptControlErrorString(unsigned resultCode)
    {
        switch(resultCode)
        {
        case CAC_FAIL_SPECTATING:
            return "Local player spectating";
        case CAC_FAIL_SCRIPT_REJECT:
            return "Script Handler Reject";
        case CAC_FAIL_NOT_SCRIPT_PARTICIPANT:
            return "Not Script Participant";
        case CAC_FAIL_NOT_ADDED_TO_WORLD:
            return "Not added to world yet";
        case CAC_FAIL_PED_TASKS:
            return "Ped Tasks";
		case CAC_FAIL_VEHICLE_OCCUPANT:
			return "Vehicle Occupants";
		case CAC_FAIL_PENDING_TUTORIAL_CHANGE:
			return "Local player pending tutorial session change";
		case CAC_FAIL_COLLECTED_WITH_PICKUP:
			return "Placement is collected but still has a pickup";
		case CAC_FAIL_UNMANAGED_RAGDOLL:
			return "Ped is ragdolling and there is no task to manage it";
		case CAC_FAIL_UNEXPLODED:
			return "Object is trying to explode";
		case CAC_FAIL_SYNCED_SCENE_NO_TASK:
			return "The ped is running a synced scene without a synced scene task";
		case CAC_FAIL_VEHICLE_I_AM_IN_NOT_CLONED:
			return "This is a script ped that is in a car we don't know about";
		case CAC_FAIL_CONCEALED:
			return "Physical object is concealed for this player";
        default:
            return rage::GetCanAcceptControlErrorString(resultCode);
        }
    }

    const char *GetCanBlendErrorString(unsigned resultCode)
    {
        switch(resultCode)
        {
        case CB_SUCCESS:
            return "Success";
        case CB_FAIL_NO_GAME_OBJECT:
            return "No Game Object";
        case CB_FAIL_BLENDING_DISABLED:
            return "Blending Disabled";
        case CB_FAIL_ATTACHED:
            return "Is Attached";
        case CB_FAIL_FIXED:
            return "Is Fixed";
        case CB_FAIL_COLLISION_TIMER:
            return "Collision Timer Active";
        case CB_FAIL_IN_VEHICLE:
            return "In Vehicle";
        case CB_FAIL_BLENDER_OVERRIDDEN:
            return "Network Blender Overridden";
		case CB_FAIL_FRAG_OBJECT:
			return "Frag object";
        case CB_FAIL_STOPPED_PREDICTING:
            return "Stopped Predicting";
        case CB_FAIL_SYNCED_SCENE:
            return "Running Synced Scene";
        default:
            gnetAssertf(0, "Unknown CanBlend() error string!");
            return "Unknown";
        }
    }

    const char *GetNetworkBlenderOverriddenReason(unsigned resultCode)
    {
        switch(resultCode)
        {
        case NBO_NOT_OVERRIDDEN:
            return "Not Overridden";
        case NBO_COLLISION_TIMER:
            return "Collision Timer";
        case NBO_PED_TASKS:
            return "Ped Tasks";
        case NBO_DIFFERENT_TUTORIAL_SESSION:
            return "Different Tutorial Session";
		case NBO_FRAG_OBJECT:
			return "Frag object";
        case NBO_SYNCED_SCENE:
			return "Running Synced Scene";
		case NBO_ATTACHED_PICKUP:
			return "Attached Pickup";
		case NBO_PICKUP_CLOSE_TO_TARGET:
			return "Pickup close to target";
		case NBO_DEAD_PED:
			return "The ped is dead";
		case NBO_FADING_OUT:
			return "The entity is fading out";
        case NBO_FAIL_STOPPED_PREDICTING:
            return "Stopped Predicting";
        case NBO_NOT_ACCEPTING_OBJECTS:
            return "Not Accepting Objects";
        case NBO_FULL_BODY_SECONDARY_ANIM:
            return "Full Body Secondary Anim";
        case NBO_FAIL_RUNNING_CUTSCENE:
            return "Running Cutscene";
		case NBO_MAGNET_PICKUP:
			return "Involved in magnet pick-up";
		case NBO_VEHICLE_FRAG_SETTLED:
			return "Vehicle frag settled";
        case NBO_FOCUS_ENTITY_DEBUG:
            return "Focus Entity Debug";
        case NBO_ARENA_BALL_TIMER:
            return "Arena Ball Timer";
        default:
            gnetAssertf(0, "Unknown NetworkBlenderOverridden() reason!");
            return "Unknown";
        }
    }

	const char *GetCanProcessAttachmentErrorString(unsigned resultCode)
	{
		switch(resultCode)
		{
		case CPA_SUCCESS:
			return "Success";
		case CPA_OVERRIDDEN:
			return "Attachment overridden";
		case CPA_NO_ENTITY:
			return "No Game Entity";
		case CPA_NOT_NETWORKED:
			return "Entity Not Networked";
		case CPA_NO_TARGET_ENTITY:
			return "No Target Entity";
		case CPA_CIRCULAR_ATTACHMENT:
			return "Entity Attached To This Entity";
		case CPA_PED_IN_CAR:
			return "Ped Is In Vehicle";
		case CPA_PED_HAS_ATTACHED_RAGDOLL:
			return "Ped Blender Has Attached Ragdoll";
		case CPA_PED_RAGDOLLING:
			return "Ped Is Ragdolling";
		case CPA_VEHICLE_DUMMY:
			return "Vehicle Has Correct Dummy Attachment";
		case CPA_LOCAL_PLAYER_ATTACHMENT:
			return "Trying To Attach To Local Player";
		case CPA_LOCAL_PLAYER_DRIVER_ATTACHMENT:
			return "Trying to Attach To Vehicle Driven By Local Player";
		default:
			gnetAssertf(0, "Unknown CanProcessPendingAttachment() error string!");
			return "Unknown";
		}
	}

	const char *GetAttemptPendingAttachmentFailReason(unsigned resultCode)
	{
		switch (resultCode)
		{
		case APA_NONE_PHYSICAL:
			return "NetObjPhysical couldn't attach it";
		case APA_NONE_PED:
			return "NetObjPed couldn't attach it";
		case APA_ROPE_ATTACHED_ENTITY_DIFFER:
			return "Parent's rope's attached entity is not this vehicle";
		case APA_TRAILER_IS_DUMMY:
			return "This trailer is a dummy and can't make from dummy";
		case APA_TRAILER_PARENT_IS_DUMMY:
			return "This trailer's parent is a dummy and can't make from dummy";
		case APA_VEHICLE_IS_DUMMY_TRAILER:
			return "This vehicle is a dummy trailer";
		case APA_VEHICLE_IS_DUMMY:
			return "This vehicle is a dummy and can't make from dummy";
		case APA_VEHICLE_PARENT_IS_DUMMY_TRAILER:
			return "This vehicle's parent is a dummy trailer";
		case APA_VEHICLE_PARENT_IS_DUMMY:
			return "This vehicle's parent is a dummy and can't make from dummy";
		case APA_VEHICLE_TOWARM_ENTITY_DIFFER:
			return "Parent towarm's attachment differs from this vehicle";
		case APA_CARGOBOB_HAS_NO_ROPE_ON_CLONE:
			return "Cargobob has no rope on clone";
		default:
			gnetAssertf(0, "Unknown GetAttemptPendingAttachmentFailReason(): %d error string!", resultCode);
			return "Unknown";
		}
	}

	const char *GetFixedByNetworkReason(unsigned resultCode)
	{
		switch(resultCode)
		{
		case FBN_NONE:
			return "None";
		case FBN_HIDDEN_FOR_CUTSCENE:
			return "Hidden for cutscene";
		case FBN_UNSTREAMED_INTERIOR:
			return "The entity is in an unstreamed interior";
		case FBN_ON_RETAIN_LIST:
			return "The entity is on an interior retain list";
		case FBN_BOX_STREAMER:
			return "box streamer collision";
		case FBN_DISTANCE:
			return "The entity is too far away";
		case FBN_TUTORIAL:
			return "The entity is hidden for a tutorial";
		case FBN_ATTACH_PARENT:
			return "The entity is attached to an entity that is fixed by network";
		case FBN_IN_AIR:
			return "The entity is in the air";
		case FBN_PLAYER_CONTROLS_DISABLED:
			return "The player has controls disabled";
		case FBN_SYNCED_SCENE_NO_TASK:
			return "The ped is in a synced scene but not running synced scene task";
        case FBN_HIDDEN_UNDER_MAP:
            return "The ped is being hidden under the map";
        case FBN_CANNOT_SWITCH_TO_DUMMY:
            return "The vehicle cannot switch to dummy physics";
        case FBN_PARENT_VEHICLE_FIXED_BY_NETWORK:
            return "The trailer is attached to a parent vehicle that is fixed by network";
        case FBN_EXPOSE_LOCAL_PLAYER_AFTER_CUTSCENE:
            return "The local player is being exposed after a cutscene";
		case FBN_FIXED_FOR_LOCAL_CONCEAL:
			return "This entity is locally concealed and fixed by network";
		case FBN_NOT_PROCESSING_FIXED_BY_NETWORK:
			return "Not processing Fixed By Network";
		default:
			gnetAssertf(0, "Unknown GetFixedByNetworkReason() error string!");
			return "Unknown";
		}
	}
	
	const char *GetNpfbnNetworkReason(unsigned resultCode)
	{
		switch(resultCode)
		{
		case NPFBN_NONE:
			return "None";
		case NPFBN_LOCAL_IN_WATER:
			return "Local entity is in water";
		case NPFBN_CLONE_IN_WATER:
			return "Clone entity is in water";
		case NPFBN_ATTACHED:
			return "Entity is attached";
		case NPFBN_IN_AIR:
			return "Plane or Heli is in air";
		case NPFBN_IS_PARACHUTE:
			return "Is a parachute";
		case NPFBN_IS_VEHICLE_GADGET:
			return "Is a vehicle gadget";
		case NPFBN_LOW_LOD_NAVMESH:
			return "Low LOD ped is on navmesh";
		case NPFBN_ANIMATED_Z_POS:
			return "Ped's Z pos set by Anim";
		default:
			gnetAssertf(0, "Unknown GetNpfbnNetworkReason() error string!");
			return "Unknown";
		}
	}

	const char *GetScopeReason(unsigned resultCode)
	{
		switch(resultCode)
		{
		case SCOPE_NONE:
			return "None";
		case SCOPE_IN_ALWAYS:
			return "GLOBALFLAG_CLONEALWAYS";
		case SCOPE_IN_PROXIMITY_RANGE_PLAYER:
			return "PROXIMITY PED";
		case SCOPE_IN_PROXIMITY_RANGE_CAMERA:
			return "PROXIMITY CAMERA";
		case SCOPE_IN_SCRIPT_PARTICIPANT:
			return "SCRIPT PARTICIPANT";
		case SCOPE_IN_ENTITY_ALWAYS_CLONED:
			return "SET_NETWORK_ID_ALWAYS_EXISTS_FOR_PLAYER";
		case SCOPE_IN_PED_VEHICLE_CLONED:
			return "VEHICLE CLONED";
		case SCOPE_IN_PED_DRIVER_VEHICLE_OWNER:
			return "PED DRIVING REMOTE VEHICLE";
		case SCOPE_IN_PED_RAPPELLING_HELI_CLONED:
			return "RAPPELLING PED HELI CLONED";
		case SCOPE_IN_PED_RAPPELLING_HELI_IN_SCOPE:
			return "RAPPELLING PED HELI IN SCOPE";
		case SCOPE_IN_VEHICLE_OCCUPANT_IN_SCOPE:
			return "VEHICLE OCCUPANT IN SCOPE";
		case SCOPE_IN_VEHICLE_OCCUPANT_OWNER:
			return "VEHICLE OCCUPANT OWNED BY PLAYER";
		case SCOPE_IN_VEHICLE_OCCUPANT_PLAYER:
			return "VEHICLE OCCUPANT IS PLAYER";
		case SCOPE_IN_VEHICLE_ATTACH_PARENT_IN_SCOPE:
			return "ATTACH PARENT IN SCOPE";
		case SCOPE_IN_OBJECT_ATTACH_CHILD_IN_SCOPE:
			return "ATTACH CHILD IN SCOPE";
		case SCOPE_IN_OBJECT_ATTACH_PARENT_IN_SCOPE:
			return "ATTACH PARENT IN SCOPE";
		case SCOPE_IN_OBJECT_VEHICLE_GADGET_IN_SCOPE:
			return "VEHICLE GADGET IN SCOPE";
		case SCOPE_IN_PICKUP_DEBUG_CREATED:
			return "DEBUG CREATED";
		case SCOPE_IN_PICKUP_PLACEMENT_REGENERATING:
			return "REGENERATING";
		case SCOPE_IN_CHILD_ATTACHMENT_OWNER:
			return "SCOPE_IN_CHILD_ATTACHMENT_IN_SCOPE";

		case SCOPE_OUT_NON_PHYSICAL_PLAYER:
			return "PLAYER NOT PHYSICAL";
		case SCOPE_OUT_NOT_SCRIPT_PARTICIPANT:
			return "NOT SCRIPT PARTICIPANT";
		case SCOPE_OUT_PROXIMITY_RANGE:
			return "OUT OF PROXIMITY";
		case SCOPE_OUT_BENEATH_WATER:
			return "BENEATH WATER";
		case SCOPE_OUT_PLAYER_BENEATH_WATER:
			return "PLAYER BENEATH WATER";
		case SCOPE_OUT_ENTITY_NOT_ADDED_TO_WORLD:
			return "NOT ADDED TO WORLD";
		case SCOPE_OUT_PLAYER_IN_DIFFERENT_TUTORIAL:
			return "DIFFERENT TUTORIAL";
		case SCOPE_OUT_PLAYER_CONCEALED_FOR_FLAGGED_PED:
			return "SCENARIO PED CREATED BY CONCEALED PLAYER";
		case SCOPE_OUT_NO_GAME_OBJECT:
			return "NO GAME OBJECT";
		case SCOPE_OUT_PED_RESPAWN_FAILED:
			return "RESPAWN FAILED";
		case SCOPE_OUT_PED_VEHICLE_NOT_CLONED:
			return "VEHICLE NOT CLONED";
		case SCOPE_OUT_PED_RAPPELLING_NOT_CLONED:
			return "RAPPELLING PED NOT CLONED";
		case SCOPE_OUT_VEHICLE_GARAGE_INDEX_MISMATCH:
			return "GARAGE INDEX MISMATCH";
		case SCOPE_OUT_OBJECT_HEIGHT_RANGE:
			return "OUT OF HEIGHT SCOPE";
		case SCOPE_OUT_OBJECT_NO_FRAG_PARENT:
			return "NO FRAG PARENT";
		case SCOPE_OUT_PICKUP_UNINITIALISED:
			return "PICKUP UNINITIALISED";
		case SCOPE_OUT_PICKUP_PLACEMENT_DESTROYED:
			return "PLACEMENT DESTROYED";
		case SCOPE_OUT_VEHICLE_ATTACH_PARENT_OUT_SCOPE:
			return "ATTACH PARENT NOT IN SCOPE";
		case SCOPE_OUT_OBJECT_FRAGMENT_PARENT_OUT_SCOPE:
			return "OBJECT_FRAGMENT_PARENT_OUT_SCOPE";
		case SCOPE_OUT_PICKUP_IS_MONEY:
			return "SCOPE_OUT_PICKUP_IS_MONEY";

		case SCOPE_PLAYER_BUBBLE:
			return "ROAMING BUBBLE";
		case SCOPE_TRAIN_USE_ENGINE_SCOPE:
			return "TRAIN ENGINE SCOPE";
		case SCOPE_TRAIN_ENGINE_WAIING_ON_CARRIAGES:
			return "TRAIN_ENGINE_WAIING_ON_CARRIAGES";
		case SCOPE_TRAIN_ENGINE_NOT_LOCAL:
			return "TRAIN_ENGINE_NOT_LOCAL"; 
		case SCOPE_NO_TRAIN_ENGINE:
			return "NO_TRAIN_ENGINE";
		case SCOPE_PICKUP_USE_PLACEMENT_SCOPE:
			return "PLACEMENT SCOPE";	

		default:
			gnetAssertf(0, "Unknown GetScopeReason() error string!");
			return "Unknown";
		}
	}

    const char *GetMigrationFailTrackingErrorString(unsigned resultCode)
    {
        switch(resultCode)
        {
        case MFT_SUCCESS:
            return "Success";
        case MFT_REMOTE_NOT_ACCEPTING_OBJECTS:
            return "Remote player not accepting objects";
        case MFT_INTERIOR_STATE:
            return "Interior state";
        case MFT_SCOPE_CHECKS:
            return "Scope checks";
        case MFT_SCRIPT_CHECKS:
            return "Script checks";
        case MFT_PROXIMITY_CHECKS:
            return "Proximity checks";
        case MFT_CAN_PASS_CONTROL_CHECKS:
            return "Can't Pass Control";
        default:
            gnetAssertf(0, "Unknown migration failure tracking error string!");
            return "Unknown";
        }
    }

    const char *GetPlayerFocus(unsigned resultCode)
    {
        switch (resultCode)
        {
        case PLAYER_FOCUS_AT_PED: 
            return "Ped";
        case PLAYER_FOCUS_AT_EXTENDED_POPULATION_RANGE_CENTER:
            return "Extended population range center";
        case PLAYER_FOCUS_AT_CAMERA_POSITION:
            return "Camera position";
        case PLAYER_FOCUS_SPECTATING:
            return "Spectating";
        case PLAYER_FOCUS_AT_LOCAL_CAMERA_POSITION:
            return "Local camera position";
        default:
            gnetAssertf(0, "Unknown GetPlayerFocus() error string!");
            return "Unknown";
        }
    }
#endif // ENABLE_NETWORK_LOGGING

	const char* GetLanguageCode()
	{
		// ISO Languages codes
		switch( CPauseMenu::GetLanguagePreference() )
		{
		case LANGUAGE_ENGLISH:
			return("en");
		case LANGUAGE_FRENCH:
			return("fr");
		case LANGUAGE_GERMAN:
			return("de");
		case LANGUAGE_ITALIAN:
			return("it");
		case LANGUAGE_MEXICAN:
			return("mx");
		case LANGUAGE_SPANISH:
			return("es");
		case LANGUAGE_JAPANESE:
			return("ja"); 
		case LANGUAGE_PORTUGUESE:
			return("pt");
		case LANGUAGE_POLISH:
			return("pl");
		case LANGUAGE_RUSSIAN:
			return("ru");
		case LANGUAGE_KOREAN:
			return("ko");
		case LANGUAGE_CHINESE_TRADITIONAL:
			return("zh");
		case LANGUAGE_CHINESE_SIMPLIFIED:
			return("zh-Hans");
		default:
			return("en");
		}
	}

	bool IsLocalPlayersTurn(Vec3V_In vPosition, float fMaxDistance, float fDurationOfTurns, float fTimeBetweenTurns)
	{
		bool isPlayerTurn = false;

		const netPlayer *closestPlayers[MAX_NUM_PHYSICAL_PLAYERS];
		u32 numPlayersInRange = NetworkInterface::GetClosestRemotePlayers(VEC3V_TO_VECTOR3(vPosition), fMaxDistance, closestPlayers, false);

		if(numPlayersInRange == 0)
		{
			isPlayerTurn = true;
		}
		else
		{
			unsigned localPlayerTurn = 0;

			for(u32 playerIndex = 0; playerIndex < numPlayersInRange; playerIndex++)
			{
				const netPlayer *player = closestPlayers[playerIndex];

				if(AssertVerify(player))
				{
					if (NetworkInterface::GetLocalPlayer()->GetRlGamerId() < player->GetRlGamerId())
					{
						localPlayerTurn++;
					}
				}
			}

			// players within range are given turns to create vehicles, highest peer ID first.
			// the game waits for a short period between changing turns, to help prevent
			// cars being created on top of one another
			const unsigned numTurns = numPlayersInRange + 1;

			const unsigned uDurationOfTurns = (unsigned)(fDurationOfTurns * 1000.0f);
			const unsigned uTimeBetweenTurns = (unsigned)(fTimeBetweenTurns * 1000.0f);

			unsigned currentTime    = NetworkInterface::GetNetworkTime();
			unsigned timeWithinTurn = currentTime % (uDurationOfTurns + uTimeBetweenTurns);
			currentTime /= (uDurationOfTurns + uTimeBetweenTurns);

			unsigned currentTurn = currentTime % (numTurns);

			if((currentTurn == localPlayerTurn) && (timeWithinTurn <= uDurationOfTurns))
			{
				isPlayerTurn = true;
			}
			else
			{
				isPlayerTurn = false;
			}
		}

		return isPlayerTurn;
	}

	s16 GetSpecialAlphaRampingValue(s16& currAlpha, bool& bAlphaIncreasing, s16 totalTime, s16 timeRemaining, bool bRampOut)
	{
		if (!AssertVerify(timeRemaining <= totalTime))
		{
			timeRemaining = totalTime;
		}

		float ratio = (float)timeRemaining / (float)totalTime;

		if (bRampOut)
		{
			ratio = (1.0f - ratio);
		}

		TUNE_INT(ALPHA_RAMP_MIN_FADE_IN_RATE, 15, 0, 255, 1);
		TUNE_INT(ALPHA_RAMP_MAX_FADE_IN_RATE, 80, 0, 255, 1);
		TUNE_INT(ALPHA_RAMP_MIN_FADE_OUT_RATE, 30, 0, 255, 1);
		TUNE_INT(ALPHA_RAMP_MAX_FADE_OUT_RATE, 160, 0, 255, 1);

		s16 minRate = (s16)(bRampOut ? ALPHA_RAMP_MIN_FADE_OUT_RATE : ALPHA_RAMP_MIN_FADE_IN_RATE);
		s16 maxRate = (s16)(bRampOut ? ALPHA_RAMP_MAX_FADE_OUT_RATE : ALPHA_RAMP_MAX_FADE_IN_RATE);

		s16 alphaRampingRate = (s16)(minRate + (int)(ratio * (float)(maxRate-minRate)));
	
		currAlpha = bAlphaIncreasing ? currAlpha + alphaRampingRate : currAlpha - alphaRampingRate;

		if (currAlpha >= 255)
		{
			currAlpha = 255; 
			bAlphaIncreasing = false;
		}
		else if (currAlpha <= 0)
		{
			currAlpha = 0;
			bAlphaIncreasing = true;
		}

		TUNE_FLOAT(ALPHA_RAMP_ALPHA_SCALE, 1.0f, 0.0f, 1.0f, 0.1f);
		s16 finalAlpha = (s16)((float)currAlpha * ALPHA_RAMP_ALPHA_SCALE*(1.0f-ratio));

		return finalAlpha;
	}

	const s16 GetSpecialAlphaRampingFadeInTime()
	{
		TUNE_INT(ALPHA_RAMP_FADE_IN_TIME, 3000, 1000, 5000, 50);
		return (s16)ALPHA_RAMP_FADE_IN_TIME;
	}

	const s16 GetSpecialAlphaRampingFadeOutTime()
	{
		TUNE_INT(ALPHA_RAMP_FADE_OUT_TIME, 1000, 500, 5000, 50);
		return (s16)ALPHA_RAMP_FADE_OUT_TIME;
	}

	const char* GetBailReasonAsString(eBailReason nBailReason)
	{
		switch(nBailReason)
		{
#if !__NO_OUTPUT
		case BAIL_FROM_SCRIPT:					 return "BAIL_FROM_SCRIPT";
		case BAIL_FROM_RAG:						 return "BAIL_FROM_RAG";
#endif
		case BAIL_PROFILE_CHANGE:				 return "BAIL_PROFILE_CHANGE";
		case BAIL_NEW_CONTENT_INSTALLED:		 return "BAIL_NEW_CONTENT_INSTALLED";
		case BAIL_SESSION_FIND_FAILED:			 return "BAIL_SESSION_FIND_FAILED";
		case BAIL_SESSION_HOST_FAILED:			 return "BAIL_SESSION_HOST_FAILED";
		case BAIL_SESSION_JOIN_FAILED:			 return "BAIL_SESSION_JOIN_FAILED";
		case BAIL_SESSION_START_FAILED:			 return "BAIL_SESSION_START_FAILED";
		case BAIL_SESSION_ATTR_FAILED:			 return "BAIL_SESSION_ATTR_FAILED";
		case BAIL_SESSION_MIGRATE_FAILED:		 return "BAIL_SESSION_MIGRATE_FAILED";
		case BAIL_PARTY_HOST_FAILED:			 return "BAIL_PARTY_HOST_FAILED";
		case BAIL_PARTY_JOIN_FAILED:			 return "BAIL_PARTY_JOIN_FAILED";
		case BAIL_JOINING_STATE_TIMED_OUT:		 return "BAIL_JOINING_STATE_TIMED_OUT";
		case BAIL_NETWORK_ERROR:				 return "BAIL_NETWORK_ERROR";
		case BAIL_TRANSITION_LAUNCH_FAILED:		 return "BAIL_TRANSITION_LAUNCH_FAILED";
		case BAIL_END_TIMED_OUT:				 return "BAIL_END_TIMED_OUT";
		case BAIL_MATCHMAKING_TIMED_OUT:		 return "BAIL_MATCHMAKING_TIMED_OUT";
		case BAIL_CLOUD_FAILED:					 return "BAIL_CLOUD_FAILED";
		case BAIL_COMPAT_PACK_CONFIG_INCORRECT:	 return "BAIL_COMPAT_PACK_CONFIG_INCORRECT";
		case BAIL_CONSOLE_BAN:					 return "BAIL_CONSOLE_BAN";
		case BAIL_MATCHMAKING_FAILED:			 return "BAIL_MATCHMAKING_FAILED";
		case BAIL_ONLINE_PRIVILEGE_REVOKED:		 return "BAIL_ONLINE_PRIVILEGE_REVOKED";
		case BAIL_SYSTEM_SUSPENDED:				 return "BAIL_SYSTEM_SUSPENDED";
		case BAIL_EXIT_GAME:					 return "BAIL_EXIT_GAME";
		case BAIL_TOKEN_REFRESH_FAILED:			 return "BAIL_TOKEN_REFRESH_FAILED";
		case BAIL_CATALOG_REFRESH_FAILED:		 return "BAIL_CATALOG_REFRESH_FAILED";
		case BAIL_SESSION_REFRESH_FAILED:		 return "BAIL_SESSION_REFRESH_FAILED";
		case BAIL_SESSION_RESTART_FAILED:		 return "BAIL_SESSION_RESTART_FAILED";
		case BAIL_GAME_SERVER_MAINTENANCE:		 return "BAIL_GAME_SERVER_MAINTENANCE";
		case BAIL_GAME_SERVER_FORCE_BAIL:		 return "BAIL_GAME_SERVER_FORCE_BAIL";
		case BAIL_GAME_SERVER_HEART_BAIL:		 return "BAIL_GAME_SERVER_HEART_BAIL";
		case BAIL_GAME_SERVER_GAME_VERSION:		 return "BAIL_GAME_SERVER_GAME_VERSION";
		case BAIL_CATALOGVERSION_REFRESH_FAILED: return "BAIL_CATALOGVERSION_REFRESH_FAILED";
		case BAIL_CATALOG_BUFFER_TOO_SMALL:		 return "BAIL_CATALOG_BUFFER_TOO_SMALL";
		case BAIL_INVALIDATED_ROS_TICKET:		 return "BAIL_INVALIDATED_ROS_TICKET";

		default:
			gnetAssertf(0, "Unknown or invalid bail reason (%d)", nBailReason);
			return "INVALID";
		}
	}

	const char* GetSentFromGroupStub(const unsigned sentFromGroupBit)
	{
		// this will only return the first set bit
		if ((sentFromGroupBit & SentFromGroup::Group_LocalPlayer) != 0) return "Group_LocalPlayer";
		if ((sentFromGroupBit & SentFromGroup::Group_Friends) != 0) return "Group_Friends";
		if ((sentFromGroupBit & SentFromGroup::Group_SmallCrew) != 0) return "Group_SmallCrew";
		if ((sentFromGroupBit & SentFromGroup::Group_LargeCrew) != 0) return "Group_LargeCrew";
		if ((sentFromGroupBit & SentFromGroup::Group_RecentPlayer) != 0) return "Group_RecentPlayer";
		if ((sentFromGroupBit & SentFromGroup::Group_SameSession) != 0) return "Group_SameSession";
		if ((sentFromGroupBit & SentFromGroup::Group_SameTeam) != 0) return "Group_SameTeam";
		if ((sentFromGroupBit & SentFromGroup::Group_Invalid) != 0) return "Group_Invalid";
		if ((sentFromGroupBit & SentFromGroup::Group_NotSameSession) != 0) return "Group_NotSameSession";

		return "Group_Unknown";
	}

#if !__NO_OUTPUT
    const char* GetChannelAsString(const unsigned nChannelId)
    {        
        switch(nChannelId)
        {
        case NETWORK_GAME_CHANNEL_ID:               return "GAME";
        case NETWORK_SESSION_VOICE_CHANNEL_ID:      return "S_VCE";
        case NETWORK_SESSION_GAME_CHANNEL_ID:       return "S_GAM";
        case NETWORK_SESSION_ACTIVITY_CHANNEL_ID:   return "S_ACT";
        case NETWORK_VOICE_CHANNEL_ID:              return "VOICE";
        default: 
            gnetAssertf(0, "GetChannelAsString :: Unknown channel ID: %d", nChannelId);
            return "UNKNOWN_CHANNEL";
        }
    }

	const char* GetSessionTypeAsString(const SessionType sessionType)
	{
		switch(sessionType)
		{
		case SessionType::ST_None:			return "ST_None";
		case SessionType::ST_Physical:		return "ST_Physical";
		case SessionType::ST_Transition:	return "ST_Transition";
		default:
			gnetAssertf(0, "Unknown or invalid session TYPE (%d)", sessionType);
			return "ST_Invalid";
		};
	}

	const char* GetSessionPurposeAsString(const SessionPurpose sessionPurpose)
	{
		switch(sessionPurpose)
		{
		case SessionPurpose::SP_None:		return "SP_None";
		case SessionPurpose::SP_Freeroam:	return "SP_Freeroam";
		case SessionPurpose::SP_Activity:	return "SP_Activity";
		case SessionPurpose::SP_Party:		return "SP_Party";
		case SessionPurpose::SP_Presence:	return "SP_Presence";
		default:
			gnetAssertf(0, "Unknown or invalid session purpose (%d)", sessionPurpose);
			return "SP_Invalid";
		};
	}

	const char* GetResponseCodeAsString(eJoinResponseCode nResponseCode)
	{
		switch(nResponseCode)
		{
		case RESPONSE_ACCEPT:						  return "RESPONSE_ACCEPT";
		case RESPONSE_DENY_UNKNOWN:					  return "RESPONSE_DENY_UNKNOWN";
		case RESPONSE_DENY_WRONG_SESSION:			  return "RESPONSE_DENY_WRONG_SESSION";
		case RESPONSE_DENY_NOT_HOSTING:				  return "RESPONSE_DENY_NOT_HOSTING";
		case RESPONSE_DENY_NOT_READY:				  return "RESPONSE_DENY_NOT_READY";
		case RESPONSE_DENY_BLACKLISTED:				  return "RESPONSE_DENY_BLACKLISTED";
		case RESPONSE_DENY_INVALID_REQUEST_DATA:	  return "RESPONSE_DENY_INVALID_REQUEST_DATA";
		case RESPONSE_DENY_INCOMPATIBLE_ASSETS:		  return "RESPONSE_DENY_INCOMPATIBLE_ASSETS";
		case RESPONSE_DENY_SESSION_FULL:			  return "RESPONSE_DENY_SESSION_FULL";
		case RESPONSE_DENY_GROUP_FULL:				  return "RESPONSE_DENY_GROUP_FULL";
		case RESPONSE_DENY_WRONG_VERSION:			  return "RESPONSE_DENY_WRONG_VERSION";
		case RESPONSE_DENY_NOT_VISIBLE:				  return "RESPONSE_DENY_NOT_VISIBLE";
		case RESPONSE_DENY_BLOCKING:				  return "RESPONSE_DENY_BLOCKING";
		case RESPONSE_DENY_AIM_PREFERENCE:			  return "RESPONSE_DENY_AIM_PREFERENCE";
		case RESPONSE_DENY_CHEATER:					  return "RESPONSE_DENY_CHEATER";
		case RESPONSE_DENY_TIMEOUT:					  return "RESPONSE_DENY_TIMEOUT";
		case RESPONSE_DENY_DATA_HASH:				  return "RESPONSE_DENY_DATA_HASH";
		case RESPONSE_DENY_CREW_LIMIT:				  return "RESPONSE_DENY_CREW_LIMIT";
		case RESPONSE_DENY_POOL_NORMAL:				  return "RESPONSE_DENY_POOL_NORMAL";
		case RESPONSE_DENY_POOL_BAD_SPORT:			  return "RESPONSE_DENY_POOL_BAD_SPORT";
		case RESPONSE_DENY_POOL_CHEATER:			  return "RESPONSE_DENY_POOL_CHEATER";
		case RESPONSE_DENY_NOT_JOINABLE:			  return "RESPONSE_DENY_NOT_JOINABLE";
        case RESPONSE_DENY_PRIVATE_ONLY:			  return "RESPONSE_DENY_PRIVATE_ONLY";
		case RESPONSE_DENY_DIFFERENT_BUILD:			  return "RESPONSE_DENY_DIFFERENT_BUILD";
		case RESPONSE_DENY_DIFFERENT_PORT:			  return "RESPONSE_DENY_DIFFERENT_PORT";
        case RESPONSE_DENY_DIFFERENT_CONTENT_SETTING: return "RESPONSE_DENY_DIFFERENT_CONTENT_SETTING";
        case RESPONSE_DENY_NOT_FRIEND:                return "RESPONSE_DENY_NOT_FRIEND";
		case RESPONSE_DENY_REPUTATION:				  return "RESPONSE_DENY_REPUTATION";
		case RESPONSE_DENY_FAILED_TO_ESTABLISH:		  return "RESPONSE_DENY_FAILED_TO_ESTABLISH";
		case RESPONSE_DENY_PREMIUM:					  return "RESPONSE_DENY_PREMIUM";
		case RESPONSE_DENY_SOLO:					  return "RESPONSE_DENY_SOLO";
		case RESPONSE_DENY_ADMIN_BLOCKED:			  return "RESPONSE_DENY_ADMIN_BLOCKED";
		case RESPONSE_DENY_TOO_MANY_BOSSES:			  return "RESPONSE_DENY_TOO_MANY_BOSSES";
		default:
			gnetAssertf(0, "Unknown or invalid response code (%d)", nResponseCode);
			return "INVALID";
		}
	}


	const char* GetSessionResponseAsString(snJoinResponseCode nResponseCode)
	{
		switch(nResponseCode)
		{
		case SNET_JOIN_DENIED_NOT_JOINABLE:			return "SNET_JOIN_DENIED_NOT_JOINABLE";
		case SNET_JOIN_DENIED_FAILED_TO_ESTABLISH:	return "SNET_JOIN_DENIED_FAILED_TO_ESTABLISH";
		case SNET_JOIN_DENIED_NO_EMPTY_SLOTS:		return "SNET_JOIN_DENIED_NO_EMPTY_SLOTS";
		case SNET_JOIN_DENIED_ALREADY_JOINED:		return "SNET_JOIN_DENIED_ALREADY_JOINED";
		case SNET_JOIN_DENIED_PRIVATE:				return "SNET_JOIN_DENIED_PRIVATE";
		case SNET_JOIN_DENIED_APP_SAID_NO:			return "SNET_JOIN_DENIED_APP_SAID_NO";
		case SNET_JOIN_ACCEPTED:					return "SNET_JOIN_ACCEPTED";
		default:
			gnetAssertf(0, "Unknown or invalid response code (%d)", nResponseCode);
			return "INVALID";
		}
	}

	const char* GetPoolAsString(eMultiplayerPool nPool)
	{
		switch(nPool)
		{
		case POOL_NORMAL:		return "POOL_NORMAL";
		case POOL_CHEATER:		return "POOL_CHEATER";
		case POOL_BAD_SPORT:	return "POOL_BAD_SPORT";
		default:
			gnetAssertf(0, "Unknown or invalid pool (%d)", nPool);
			return "INVALID";
		}
	}

	const char* GetTransitionBailReasonAsString(eTransitionBailReason nBailReason)
	{
		switch(nBailReason)
		{
		case BAIL_TRANSITION_SCRIPT:				return "BAIL_TRANSITION_SCRIPT";
		case BAIL_TRANSITION_JOIN_FAILED:			return "BAIL_TRANSITION_JOIN_FAILED";
		case BAIL_TRANSITION_HOST_FAILED:			return "BAIL_TRANSITION_HOST_FAILED";
		case BAIL_TRANSITION_CANNOT_HOST_OR_JOIN:	return "BAIL_TRANSITION_CANNOT_HOST_OR_JOIN";
		case BAIL_TRANSITION_CLEAR_SESSION:			return "BAIL_TRANSITION_CLEAR_SESSION";
		
		default:
			gnetAssertf(0, "Unknown or invalid bail reason (%d)", nBailReason);
			return "INVALID";
		}
	}

	const char* GetBailErrorCodeAsString(const int errorCode)
	{
		gnetAssertf(errorCode != BAIL_CTX_COUNT, "BAIL_CTX_COUNT should not be used");

		switch (errorCode)
		{
		case BAIL_CTX_NONE:										return "BAIL_CTX_NONE";

			// find failed
		case BAIL_CTX_FIND_FAILED_JOIN_FAILED:					return "BAIL_CTX_FIND_FAILED_JOIN_FAILED";
		case BAIL_CTX_FIND_FAILED_NOT_QUICKMATCH:				return "BAIL_CTX_FIND_FAILED_NOT_QUICKMATCH";
		case BAIL_CTX_FIND_FAILED_HOST_FAILED:					return "BAIL_CTX_FIND_FAILED_HOST_FAILED";
		case BAIL_CTX_FIND_FAILED_CANNOT_HOST:					return "BAIL_CTX_FIND_FAILED_CANNOT_HOST";
		case BAIL_CTX_FIND_FAILED_WAITING_FOR_PRESENCE:			return "BAIL_CTX_FIND_FAILED_WAITING_FOR_PRESENCE";
		case BAIL_CTX_FIND_FAILED_JOINING:						return "BAIL_CTX_FIND_FAILED_JOINING";
#if !__FINAL
		case BAIL_CTX_FIND_FAILED_CMD_LINE:						return "BAIL_CTX_FIND_FAILED_CMD_LINE";
#endif

			// host failed
		case BAIL_CTX_HOST_FAILED_ERROR:						return "BAIL_CTX_HOST_FAILED_ERROR";
		case BAIL_CTX_HOST_FAILED_TIMED_OUT:					return "BAIL_CTX_HOST_FAILED_TIMED_OUT";
		case BAIL_CTX_HOST_FAILED_ON_STARTING:					return "BAIL_CTX_HOST_FAILED_ON_STARTING";
#if !__FINAL
		case BAIL_CTX_HOST_FAILED_CMD_LINE:						return "BAIL_CTX_HOST_FAILED_CMD_LINE";
#endif

			// join failed
		case BAIL_CTX_JOIN_FAILED_DIRECT_NO_FALLBACK:			return "BAIL_CTX_JOIN_FAILED_DIRECT_NO_FALLBACK";
		case BAIL_CTX_JOIN_FAILED_PENDING_INVITE:				return "BAIL_CTX_JOIN_FAILED_PENDING_INVITE";
		case BAIL_CTX_JOIN_FAILED_BUILDING_REQUEST:				return "BAIL_CTX_JOIN_FAILED_BUILDING_REQUEST";
		case BAIL_CTX_JOIN_FAILED_BUILDING_REQUEST_ACTIVITY:	return "BAIL_CTX_JOIN_FAILED_BUILDING_REQUEST_ACTIVITY";
		case BAIL_CTX_JOIN_FAILED_ON_STARTING:					return "BAIL_CTX_JOIN_FAILED_ON_STARTING";
		case BAIL_CTX_JOIN_FAILED_CHECK_MATCHMAKING:			return "BAIL_CTX_JOIN_FAILED_CHECK_MATCHMAKING";
		case BAIL_CTX_JOIN_FAILED_CHECK_JOIN_ACTION:			return "BAIL_CTX_JOIN_FAILED_CHECK_JOIN_ACTION";
		case BAIL_CTX_JOIN_FAILED_KICKED:						return "BAIL_CTX_JOIN_FAILED_KICKED";
		case BAIL_CTX_JOIN_FAILED_VALIDATION:					return "BAIL_CTX_JOIN_FAILED_VALIDATION";
		case BAIL_CTX_JOIN_FAILED_NOTIFY:						return "BAIL_CTX_JOIN_FAILED_NOTIFY";
		case BAIL_CTX_JOIN_FAILED_RESTART_SIGNALING:			return "BAIL_CTX_JOIN_FAILED_RESTART_SIGNALING";
#if !__FINAL
		case BAIL_CTX_JOIN_FAILED_CMD_LINE:						return "BAIL_CTX_JOIN_FAILED_CMD_LINE";
#endif

			// start failed
		case BAIL_CTX_START_FAILED_ERROR:						return "BAIL_CTX_START_FAILED_ERROR";
#if !__FINAL
		case BAIL_CTX_START_FAILED_CMD_LINE:					return "BAIL_CTX_START_FAILED_CMD_LINE";
#endif

			// joining state timed out
		case BAIL_CTX_JOINING_STATE_TIMED_OUT_JOINING:			return "BAIL_CTX_JOINING_STATE_TIMED_OUT_JOINING";
		case BAIL_CTX_JOINING_STATE_TIMED_OUT_LOBBY:			return "BAIL_CTX_JOINING_STATE_TIMED_OUT_LOBBY";
		case BAIL_CTX_JOINING_STATE_TIMED_OUT_START_PENDING:	return "BAIL_CTX_JOINING_STATE_TIMED_OUT_START_PENDING";
#if !__FINAL
		case BAIL_CTX_JOINING_STATE_TIMED_OUT_CMD_LINE:			return "BAIL_CTX_JOINING_STATE_TIMED_OUT_CMD_LINE";
#endif

			// transition launch
		case BAIL_CTX_TRANSITION_LAUNCH_LEAVING:				return "BAIL_CTX_TRANSITION_LAUNCH_LEAVING";
		case BAIL_CTX_TRANSITION_LAUNCH_NO_TRANSITION:			return "BAIL_CTX_TRANSITION_LAUNCH_NO_TRANSITION";
		case BAIL_CTX_TRANSITION_LAUNCH_MIGRATE_FAILED:			return "BAIL_CTX_TRANSITION_LAUNCH_MIGRATE_FAILED";

			// network error
		case BAIL_CTX_NETWORK_ERROR_KICKED:						return "BAIL_CTX_NETWORK_ERROR_KICKED";
		case BAIL_CTX_NETWORK_ERROR_OUT_OF_MEMORY:				return "BAIL_CTX_NETWORK_ERROR_OUT_OF_MEMORY";
		case BAIL_CTX_NETWORK_ERROR_EXHAUSTED_EVENTS_POOL:		return "BAIL_CTX_NETWORK_ERROR_EXHAUSTED_EVENTS_POOL";
		case BAIL_CTX_NETWORK_ERROR_CHEAT_DETECTED:				return "BAIL_CTX_NETWORK_ERROR_CHEAT_DETECTED";

			// profile change
		case BAIL_CTX_PROFILE_CHANGE_SIGN_OUT:					return "BAIL_CTX_PROFILE_CHANGE_SIGN_OUT";
		case BAIL_CTX_PROFILE_CHANGE_HARDWARE_OFFLINE:			return "BAIL_CTX_PROFILE_CHANGE_HARDWARE_OFFLINE";
		case BAIL_CTX_PROFILE_CHANGE_INVITE_DIFFERENT_USER:		return "BAIL_CTX_PROFILE_CHANGE_INVITE_DIFFERENT_USER";
		case BAIL_CTX_PROFILE_CHANGE_FROM_STORE:				return "BAIL_CTX_PROFILE_CHANGE_FROM_STORE";

			// suspending
		case BAIL_CTX_SUSPENDED_SIGN_OUT:						return "BAIL_CTX_SUSPENDED_SIGN_OUT";
		case BAIL_CTX_SUSPENDED_ON_RESUME:						return "BAIL_CTX_SUSPENDED_ON_RESUME";

			// exit game
		case BAIL_CTX_EXIT_GAME_FROM_APP:						return "BAIL_CTX_EXIT_GAME_FROM_APP";
		case BAIL_CTX_EXIT_GAME_FROM_FLOW:						return "BAIL_CTX_EXIT_GAME_FROM_FLOW";

		case BAIL_CTX_CATALOG_FAIL:								return "BAIL_CTX_CATALOG_FAIL";
		case BAIL_CTX_CATALOG_FAIL_TRIGGER_VERSION_CHECK:		return "BAIL_CTX_CATALOG_FAIL_TRIGGER_VERSION_CHECK";
		case BAIL_CTX_CATALOG_FAIL_TRIGGER_CATALOG_REFRESH:		return "BAIL_CTX_CATALOG_FAIL_TRIGGER_CATALOG_REFRESH";
		case BAIL_CTX_CATALOG_TASKCONFIGURE_FAILED:				return "BAIL_CTX_CATALOG_TASKCONFIGURE_FAILED";
		case BAIL_CTX_CATALOG_VERSION_TASKCONFIGURE_FAILED:		return "BAIL_CTX_CATALOG_VERSION_TASKCONFIGURE_FAILED";
		case BAIL_CTX_CATALOG_DESERIALIZE_FAILED:				return "BAIL_CTX_CATALOG_DESERIALIZE_FAILED";

			// privilege
		case BAIL_CTX_PRIVILEGE_DEFAULT:						return "BAIL_CTX_PRIVILEGE_DEFAULT";
		case BAIL_CTX_PRIVILEGE_PROMOTION_ENDED:				return "BAIL_CTX_PRIVILEGE_PROMOTION_ENDED";
		case BAIL_CTX_PRIVILEGE_SESSION_ACTION_AFTER_PROMOTION: return "BAIL_CTX_PRIVILEGE_SESSION_ACTION_AFTER_PROMOTION";

		case BAIL_CTX_COUNT: return "BAIL_CTX_COUNT";
		}

		return "";
	}

	const char* GetMultiplayerGameModeIntAsString(const int nGameMode)
	{
		return GetMultiplayerGameModeAsString(static_cast<u16>(nGameMode));
	}

	const char* GetMultiplayerGameModeAsString(const u16 nGameMode)
	{
		switch (nGameMode)
		{
		case static_cast<u16>(GameMode_Invalid): return "GameMode_Invalid";
		case static_cast<u16>(GameMode_Freeroam): return "GameMode_Freeroam";
		case static_cast<u16>(GameMode_TestBed): return "GameMode_TestBed";
		case static_cast<u16>(GameMode_Editor): return "GameMode_Editor";
		case static_cast<u16>(GameMode_Creator): return "GameMode_Creator";
		case static_cast<u16>(GameMode_FakeMultiplayer): return "GameMode_FakeMultiplayer";
		case static_cast<u16>(GameMode_Arcade_CnC): return "GameMode_Arcade_CnC";
		case static_cast<u16>(GameMode_Arcade_EndlessWinter): return "GameMode_Arcade_EndlessWinter";
		case static_cast<u16>(GameMode_Tutorial): return "GameMode_Tutorial";
		case static_cast<u16>(GameMode_Presence): return "GameMode_Presence";
		case static_cast<u16>(GameMode_Voice): return "GameMode_Voice";
		default: return "GameMode_Unknown";
		}
	}

	const char* GetMultiplayerGameModeStateAsString(const int nGameModeState)
	{
		switch (nGameModeState)
		{
		case GameModeState_Invalid: return "GameModeState_Invalid";
		case GameModeState_None: return "GameModeState_None";
		case GameModeState_CnC_Cash: return "GameModeState_CnC_Cash";
		case GameModeState_CnC_Escape: return "GameModeState_CnC_Escape";
		default: return "GameModeState_Unknown";
		}
	}

	const char* LogGamerHandle(const rlGamerHandle& hGamer)
	{
		static const unsigned MAX_HANDLES = 5;
		static const unsigned MAX_STRING = 256;
		static char szString[MAX_HANDLES][MAX_STRING];
		static unsigned nHandleIndex = 0;
		
		// get handle
		static char szHandleString[RL_MAX_GAMER_HANDLE_CHARS];
		if(hGamer.IsValid())
			hGamer.ToString(szHandleString, RL_MAX_GAMER_HANDLE_CHARS);
		else
			safecpy(szHandleString, "Invalid");

#if RSG_DURANGO
		snprintf(szString[nHandleIndex], MAX_STRING, "[%s - %llu]", szHandleString, hGamer.IsValid() ? hGamer.GetXuid() : 0);
#else
		safecpy(szString[nHandleIndex], szHandleString);
#endif

		// having a rolling buffer allows for multiple calls to this function in the same logging line
		int nHandleIndexUsed = nHandleIndex;
		nHandleIndex++;
		if(nHandleIndex >= MAX_HANDLES)
			nHandleIndex = 0;

		return szString[nHandleIndexUsed];
	}
#endif

#if __BANK
	const char* GetSetGhostingReason(unsigned reason)
	{
		switch(reason)
		{
		case SPG_SET_GHOST_PLAYER_FLAGS:
			return "CNetObjPlayer SetGhostPlayerFlags";
		case SPG_RAG:
			return "Setting from RAG";
		case SPG_PLAYER_STATE:
			return "CNetObjPlayer SetPlayerGameStateData";
		case SPG_PLAYER_VEHICLE:
			return "Set on vehicle belonging to the player";
		case SPG_NON_PARTICIPANTS:
			return "Set on non script participants";
		case SPG_TRAILER_INHERITS_GHOSTING:
			return "Trailer inherits ghosting";
		case SPG_RESET_GHOSTING:
			return "Vehicle reset ghosting";
		case SPG_RESET_GHOSTING_NON_PARTICIPANTS:
			return "Reset for non participants";
		case SPG_VEHICLE_STATE:
			return "Set vehicle as ghost by sync data";
		case SPG_SCRIPT:
			return CTheScripts::GetCurrentScriptNameAndProgramCounter();
		default:
			return "Unknown";
		}
	}
#endif // __BANK
}
