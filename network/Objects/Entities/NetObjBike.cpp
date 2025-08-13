//
// name:        CNetObjBike.cpp
// description: Derived from netObject, this class handles all bike-related network object
//              calls. See NetworkObject.h for a full description of all the methods.
// written by:  John Gurney
//

#include "network/objects/entities/NetObjBike.h"

// game headers
#include "network/objects/entities/NetObjPlayer.h"
#include "network/objects/prediction/NetBlenderPhysical.h"
#include "Peds/ped.h"
#include "vehicles/Bike.h"

NETWORK_OPTIMISATIONS()
NETWORK_OBJECT_OPTIMISATIONS()

CBikeSyncTree *CNetObjBike::ms_bikeSyncTree;

CNetObjBike::CNetObjBike(CBike						*bike,
                         const NetworkObjectType	type,
                         const ObjectId				objectID,
                         const PhysicalPlayerIndex  playerIndex,
                         const NetObjFlags			localFlags,
                         const NetObjFlags			globalFlags) :
CNetObjVehicle(bike, type, objectID, playerIndex, localFlags, globalFlags)
{
}

void CNetObjBike::CreateSyncTree()
{
    ms_bikeSyncTree = rage_new CBikeSyncTree(VEHICLE_SYNC_DATA_BUFFER_SIZE);
}

void CNetObjBike::DestroySyncTree()
{
	ms_bikeSyncTree->ShutdownTree();
    delete ms_bikeSyncTree;
    ms_bikeSyncTree = 0;
}

netINodeDataAccessor *CNetObjBike::GetDataAccessor(u32 dataAccessorType)
{
    netINodeDataAccessor *dataAccessor = 0;

    if(dataAccessorType == IBikeNodeDataAccessor::DATA_ACCESSOR_ID())
    {
        dataAccessor = (IBikeNodeDataAccessor *)this;
    }
    else
    {
        dataAccessor = CNetObjVehicle::GetDataAccessor(dataAccessorType);
    }

    return dataAccessor;
}

float CNetObjBike::GetLeanAngle() const
{
    float leanAngle = 0.0f;

    CBike *bike = GetBike();

    if(bike)
    {
        CNetBlenderPhysical *netBlenderPhysical = static_cast<CNetBlenderPhysical *>(GetNetBlender());

        if(netBlenderPhysical)
        {
            if(!netBlenderPhysical->IsBlendingOrientation())
            {
                Matrix34 matrix = netBlenderPhysical->GetLastMatrixReceived();
                leanAngle = matrix.GetEulers().y - bike->m_fGroundLeanAngle;
            }
            else
            {
                Matrix34 matrix = MAT34V_TO_MATRIX34(bike->GetTransform().GetMatrix());
                leanAngle = matrix.GetEulers().y - bike->m_fGroundLeanAngle;
            }
        }
    }

    return leanAngle;
}

// name:        ProcessControl
// description: Called from CGameWorld::Process, called in the same place as the local entity process controls
bool CNetObjBike::ProcessControl()
{
    CBike* pBike = GetBike();
    gnetAssert(pBike);

    CNetObjVehicle::ProcessControl();

    if (pBike)
    {
        if(IsClone())
        {
            // do a stripped down process control
            pBike->CalcAnimSteerRatio();

            if(pBike->GetDriver())
            {
                pBike->m_nBikeFlags.bGettingPickedUp = false;
            }

            if(IsClone())
            {
				pBike->ProcessCarAlarm(fwTimer::GetTimeStep());
            }

            // network clone dummy vehicle support
            bool preventDummyModes = ShouldPreventDummyModesThisFrame();

            pBike->m_nVehicleFlags.bPreventBeingDummyThisFrame      = preventDummyModes;
			pBike->m_nVehicleFlags.bPreventBeingSuperDummyThisFrame = preventDummyModes;
		    pBike->m_nVehicleFlags.bTasksAllowDummying = false;
			pBike->m_nVehicleFlags.bPreventBeingDummyUnlessCollisionLoadedThisFrame = false;

            // check for a remote player jumping for bikes that support it
            if(pBike->pHandling->GetBikeHandlingData()->m_fJumpForce > 0.0f )
            {
                CPed *driver = pBike->GetDriver();

                if(driver && driver->IsPlayer() && driver->IsNetworkClone())
                {
                    CNetObjPlayer *netObjPlayer = SafeCast(CNetObjPlayer, driver->GetNetworkObject());

                    if(netObjPlayer->IsVehicleJumpDown())
                    {
                        if(!pBike->m_nBikeFlags.bJumpPressed)
                        {
                            pBike->m_uJumpLastPressedTime = fwTimer::GetTimeInMilliseconds();

                            pBike->m_nBikeFlags.bJumpPressed  = true;
                            pBike->m_nBikeFlags.bJumpReleased = false;
                            pBike->ActivatePhysics();
                        }
                    }
                    else
                    {
                        if(pBike->m_nBikeFlags.bJumpPressed)
                        {
                            pBike->m_nBikeFlags.bJumpPressed  = false;
                            pBike->m_nBikeFlags.bJumpReleased = true;
                        }
                    }
                }
            }
        }
    }

    return true;
}

void CNetObjBike::GetBikeGameState(bool &isOnSideStand)
{
    CBike* pBike = GetBike();
    gnetAssert(pBike);
    isOnSideStand = pBike->m_nBikeFlags.bOnSideStand;
}

void CNetObjBike::SetBikeGameState(const bool onSideStand)
{
    CBike* pBike = GetBike();
    gnetAssert(pBike);
    pBike->m_nBikeFlags.bOnSideStand = onSideStand;
}
