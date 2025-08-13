//
// name:        CNetObjBoat.cpp
// description: Derived from netObject, this class handles all boat-related network object
//              calls. See NetworkObject.h for a full description of all the methods.
// written by:  John Gurney
//
#include "network/objects/entities/NetObjBoat.h"

// game headers
#include "network/objects/prediction/NetBlenderBoat.h"
#include "vehicles/Boat.h"

NETWORK_OPTIMISATIONS()
NETWORK_OBJECT_OPTIMISATIONS()

#define NETOBJBOAT_ANCHOR_TIME_DELAY 1000

CBoatSyncTree              *CNetObjBoat::ms_boatSyncTree;
CNetObjBoat::CBoatScopeData CNetObjBoat::ms_boatScopeData;

CBoatBlenderData s_boatBlenderData;
CBoatBlenderData *CNetObjBoat::ms_boatBlenderData = &s_boatBlenderData;

CNetObjBoat::CNetObjBoat(CBoat						*boat,
                         const NetworkObjectType	type,
                         const ObjectId				objectID,
                         const PhysicalPlayerIndex  playerIndex,
                         const NetObjFlags			localFlags,
                         const NetObjFlags			globalFlags) :
CNetObjVehicle(boat, type, objectID, playerIndex, localFlags, globalFlags)
{
}

void CNetObjBoat::CreateSyncTree()
{
    ms_boatSyncTree = rage_new CBoatSyncTree(VEHICLE_SYNC_DATA_BUFFER_SIZE);
}

void CNetObjBoat::DestroySyncTree()
{
	ms_boatSyncTree->ShutdownTree();
    delete ms_boatSyncTree;
    ms_boatSyncTree = 0;
}

netINodeDataAccessor *CNetObjBoat::GetDataAccessor(u32 dataAccessorType)
{
    netINodeDataAccessor *dataAccessor = 0;

    if(dataAccessorType == IBoatNodeDataAccessor::DATA_ACCESSOR_ID())
    {
        dataAccessor = (IBoatNodeDataAccessor *)this;
    }
    else
    {
        dataAccessor = CNetObjVehicle::GetDataAccessor(dataAccessorType);
    }

    return dataAccessor;
}

// name:        ProcessControl
// description: Called from CGameWorld::Process, called in the same place as the local entity process controls
bool CNetObjBoat::ProcessControl()
{
    CBoat* pBoat = GetBoat();
    gnetAssert(pBoat);

    CNetObjVehicle::ProcessControl();

    if (IsClone())
    {
        // clone boats should never be anchored as it causes problems with the prediction code
        pBoat->GetAnchorHelper().Anchor(false);
    }
	else
	{
		//Is Local

		//Establish an anchor because one couldn't be established on ChangeOwner - continue to try at a throttled rate
		//If the boat has a driver - it shouldn't be anchored
		if (pBoat->GetDriver())
		{
			m_bLockedToXY = false;
		}

		if (pBoat->m_BoatHandling.IsInWater() && m_bLockedToXY && !pBoat->GetAnchorHelper().IsAnchored())
		{
			//Throttle this check
			if (m_attemptedAnchorTime < fwTimer::GetSystemTimeInMilliseconds())
			{
				m_attemptedAnchorTime = fwTimer::GetSystemTimeInMilliseconds() + NETOBJBOAT_ANCHOR_TIME_DELAY;

				//Try to anchor because it should be anchored
				if (pBoat->GetAnchorHelper().CanAnchorHere(true))
				{
					pBoat->GetAnchorHelper().Anchor(m_bLockedToXY);
				}
			}
		}
	}

    return true;
}

// Name         :   CreateNetBlender
// Purpose      :
void CNetObjBoat::CreateNetBlender()
{
    CBoat *pBoat = GetBoat();
    gnetAssert(pBoat);
    gnetAssert(!GetNetBlender());

    netBlender *blender = rage_new CNetBlenderBoat(pBoat, ms_boatBlenderData);
    gnetAssert(blender);
    blender->Reset();
    SetNetBlender(blender);
}

// Name         :   ChangeOwner
// Purpose      :   change ownership from one player to another
void CNetObjBoat::ChangeOwner(const netPlayer& player, eMigrationType migrationType)
{
    bool bWasClone = IsClone();

    CNetObjVehicle::ChangeOwner(player, migrationType);

    if(bWasClone && !IsClone())
    {
        CBoat *pBoat = GetBoat();

        if(pBoat)
        {
			if (m_bLockedToXY && pBoat->GetAnchorHelper().CanAnchorHere(true))
			{
				pBoat->GetAnchorHelper().Anchor(m_bLockedToXY);
			}
			else
			{
				m_attemptedAnchorTime = fwTimer::GetSystemTimeInMilliseconds() + NETOBJBOAT_ANCHOR_TIME_DELAY;
			}
        }
    }
}

// Name         :   IsOnGround
// Purpose      :   Returns true if the boat is on the water
bool CNetObjBoat::IsOnGround()
{
    CBoat* pBoat = GetBoat();
    gnetAssert(pBoat);

    return pBoat->m_BoatHandling.IsInWater();
}

// sync parser getter functions
void CNetObjBoat::GetBoatGameState(CBoatGameStateDataNode& data)
{
    CBoat* pBoat = GetBoat();
    gnetAssert(pBoat);
	data.m_lockedToXY	                     = pBoat->GetAnchorHelper().IsAnchored();
	data.m_boatWreckedAction                 = pBoat->m_BoatHandling.GetWreckedAction();
	data.m_ForceLowLodMode                   = pBoat->GetAnchorHelper().ShouldAlwaysUseLowLodMode();
	data.m_UseWidestToleranceBoundingBoxTest = pBoat->m_Buoyancy.m_buoyancyFlags.bUseWidestRiverBoundTolerance;
    data.m_AnchorLodDistance                 = pBoat->m_BoatHandling.GetLowLodBuoyancyDistance();
	data.m_interiorLightOn                   = pBoat->m_nVehicleFlags.bInteriorLightOn;
	data.m_BuoyancyForceMultiplier           = pBoat->m_Buoyancy.m_fForceMult;
}

// sync parser setter functions
void CNetObjBoat::SetBoatGameState(const CBoatGameStateDataNode& data)
{
    CBoat* pBoat = GetBoat();
    gnetAssert(pBoat);
    m_bLockedToXY = data.m_lockedToXY;
    pBoat->m_BoatHandling.SetWreckedAction( (u8)data.m_boatWreckedAction );
    pBoat->GetAnchorHelper().ForceLowLodModeAlways(data.m_ForceLowLodMode);
	pBoat->m_Buoyancy.m_buoyancyFlags.bUseWidestRiverBoundTolerance = data.m_UseWidestToleranceBoundingBoxTest;
    pBoat->m_BoatHandling.SetLowLodBuoyancyDistance(data.m_AnchorLodDistance);
	pBoat->m_nVehicleFlags.bInteriorLightOn = data.m_interiorLightOn;
	pBoat->m_Buoyancy.m_fForceMult = data.m_BuoyancyForceMultiplier;
}
