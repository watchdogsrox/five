//
// name:        NetObjAutomobile.cpp
// description: Derived from netObject, this class handles all automobile-related network object
//              calls. See NetworkObject.h for a full description of all the methods.
// written by:  John Gurney
//

#include "network/objects/entities/NetObjAutomobile.h"
#include "event/EventShocking.h"
#include "event/ShockingEvents.h"

#include "peds/ped.h"
#include "vehicles/Automobile.h"

NETWORK_OPTIMISATIONS()
NETWORK_OBJECT_OPTIMISATIONS()

CAutomobileSyncTree *CNetObjAutomobile::ms_autoSyncTree;

CNetObjAutomobile::CNetObjAutomobile(CAutomobile				*automobile,
                                     const NetworkObjectType	type,
                                     const ObjectId				objectID,
                                     const PhysicalPlayerIndex  playerIndex,
                                     const NetObjFlags			localFlags,
                                     const NetObjFlags			globalFlags) :
CNetObjVehicle(automobile, type, objectID, playerIndex, localFlags, globalFlags)

{
}

void CNetObjAutomobile::CreateSyncTree()
{
    ms_autoSyncTree = rage_new CAutomobileSyncTree(VEHICLE_SYNC_DATA_BUFFER_SIZE);
}

void CNetObjAutomobile::DestroySyncTree()
{
	ms_autoSyncTree->ShutdownTree();
    delete ms_autoSyncTree;
    ms_autoSyncTree = 0;
}

netINodeDataAccessor *CNetObjAutomobile::GetDataAccessor(u32 dataAccessorType)
{
    netINodeDataAccessor *dataAccessor = 0;

    if(dataAccessorType == IAutomobileNodeDataAccessor::DATA_ACCESSOR_ID())
    {
        dataAccessor = (IAutomobileNodeDataAccessor *)this;
    }
    else
    {
        dataAccessor = CNetObjVehicle::GetDataAccessor(dataAccessorType);
    }

    return dataAccessor;
}

// name:        ProcessControl
// description: Called from CGameWorld::Process, called in the same place as the local entity process controls
bool CNetObjAutomobile::ProcessControl()
{
	CNetObjVehicle::ProcessControl();

	CAutomobile *pAutomobile = GetAutomobile();
	if(pAutomobile)
	{
		//Only process this if the auto is a clone and doesn't have a driver, or if the driver is also a network clone.
		//In the case the vehicle has a local driver but is a clone - don't do this processing - can happen if the vehicle is being carried by a cargobob or towtruck.
		if(IsClone() && (!pAutomobile->GetDriver() || pAutomobile->GetDriver()->IsNetworkClone()))
		{
		    // reset stuff for frame
            bool preventDummyModes = ShouldPreventDummyModesThisFrame();

		    pAutomobile->m_nVehicleFlags.bWarnedPeds = false;
		    pAutomobile->m_nVehicleFlags.bRestingOnPhysical = false;
		    pAutomobile->m_nVehicleFlags.bPreventBeingDummyThisFrame      = preventDummyModes;
			pAutomobile->m_nVehicleFlags.bPreventBeingSuperDummyThisFrame = preventDummyModes;
			pAutomobile->m_nVehicleFlags.bPreventBeingDummyUnlessCollisionLoadedThisFrame = false;
		    pAutomobile->m_nVehicleFlags.bTasksAllowDummying = false;
		    pAutomobile->m_nAutomobileFlags.bIsBoggedDownInSand = false;

			// Play/stop horn if couldn't do it when network event received
			if(pAutomobile->GetVehicleAudioEntity() && pAutomobile->GetVehicleAudioEntity()->IsHornOn() != IsHornOnFromNetwork() && pAutomobile->GetDriver())
			{
				if(IsHornOnFromNetwork())
					pAutomobile->GetVehicleAudioEntity()->PlayVehicleHorn();
				else
					pAutomobile->GetVehicleAudioEntity()->StopHorn();
			}

			// Generate a horn shocking event, if the horn is being played
			if(pAutomobile->IsHornOn() && pAutomobile->ShouldGenerateMinorShockingEvent())
			{
				CEventShockingHornSounded ev(*pAutomobile);
				CShockingEventsManager::Add(ev);
			}

			pAutomobile->ProcessCarAlarm(fwTimer::GetTimeStep());

        }
    }

    return true;
}

// sync parser creation functions
void CNetObjAutomobile::GetAutomobileCreateData(CAutomobileCreationDataNode& data)
{
    CAutomobile* pAutomobile = GetAutomobile();
    gnetAssert(pAutomobile);

    data.m_allDoorsClosed = true;

    for (s32 i=0; i<NUM_VEH_DOORS_MAX; i++)
    {
        if(pAutomobile->GetDoor(i))
        {
            data.m_doorsClosed[i] = pAutomobile->GetDoor(i)->GetIsClosed();

			if (!data.m_doorsClosed[i])
				data.m_allDoorsClosed = false;
        }
        else
        {
            data.m_doorsClosed[i] = true;
        }
    }
}

void CNetObjAutomobile::SetAutomobileCreateData(const CAutomobileCreationDataNode& data)
{
    CAutomobile* pAutomobile = GetAutomobile();
    gnetAssert(pAutomobile);

    if (data.m_allDoorsClosed)
    {
        if (pAutomobile)
        {
            for (s32 i=0; i<pAutomobile->GetNumDoors(); i++)
            {
                pAutomobile->GetDoor(i)->SetShutAndLatched(pAutomobile);
            }
        }
    }
    else
    {
        for (s32 i=0; i<NUM_VEH_DOORS_MAX; i++)
        {
            if(pAutomobile && pAutomobile->GetDoor(i))
            {
                if (data.m_doorsClosed[i])
                {
                    pAutomobile->GetDoor(i)->SetShutAndLatched(pAutomobile);
                }
                else
                {
                    pAutomobile->GetDoor(i)->SetTargetDoorOpenRatio(1.0f, CCarDoor::DRIVEN_AUTORESET);
                }
            }
        }
    }
}