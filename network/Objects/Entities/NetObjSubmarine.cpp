//
// name:        CNetObjSubmarine.cpp
// description: Derived from netObject, this class handles all plane-related network object
//              calls. See NetworkObject.h for a full description of all the methods.
// written by:  
//


//Network Headers
#include "network/objects/entities/NetObjSubmarine.h"
#include "network/objects/entities/NetObjAutomobile.h"
#include "network/network.h"

//Game Headers
#include "vehicles/Submarine.h"
#include "Peds/ped.h"
#include "scene/world/GameWorld.h"


NETWORK_OPTIMISATIONS()

CSubmarineSyncTree*  CNetObjSubmarine::ms_submarineSyncTree;

CNetObjSubmarine::CNetObjSubmarine(CSubmarine*                submarine
								  ,const NetworkObjectType    type
								  ,const ObjectId             objectID
								  ,const PhysicalPlayerIndex  playerIndex
								  ,const NetObjFlags          localFlags
								  ,const NetObjFlags          globalFlags)
: CNetObjVehicle(submarine, type, objectID, playerIndex, localFlags, globalFlags)
, m_IsAnchored(false)
{
}

void CNetObjSubmarine::CreateSyncTree()
{
	ms_submarineSyncTree = rage_new CSubmarineSyncTree(VEHICLE_SYNC_DATA_BUFFER_SIZE);
}

void CNetObjSubmarine::DestroySyncTree()
{
	ms_submarineSyncTree->ShutdownTree();
	delete ms_submarineSyncTree;
	ms_submarineSyncTree = 0;
}

netINodeDataAccessor *CNetObjSubmarine::GetDataAccessor(u32 dataAccessorType)
{
	netINodeDataAccessor *dataAccessor = 0;

	if(dataAccessorType == ISubmarineNodeDataAccessor::DATA_ACCESSOR_ID())
	{
		dataAccessor = (ISubmarineNodeDataAccessor *)this;
	}
	else
	{
		dataAccessor = CNetObjVehicle::GetDataAccessor(dataAccessorType);
	}

	return dataAccessor;
}

void 
CNetObjSubmarine::GetSubmarineControlData(CSubmarineControlDataNode& data)
{
	CSubmarine *submarine = GetSubmarine();
	gnetAssert(submarine);
	CSubmarineHandling* subHandling = submarine->GetSubHandling();
	gnetAssert(subHandling);

	data.m_yaw		= subHandling->GetYawControl();
	data.m_pitch	= subHandling->GetPitchControl();
	data.m_dive		= subHandling->GetDiveControl();

	//clamp
	data.m_yaw		= Clamp(data.m_yaw, -1.0f, 1.0f);
	data.m_pitch	= Clamp(data.m_pitch, -1.0f, 1.0f);
	data.m_dive     = Clamp(data.m_dive, -1.0f, 1.0f);
}

void 
CNetObjSubmarine::SetSubmarineControlData(const CSubmarineControlDataNode& data)
{
	CSubmarine *submarine = GetSubmarine();
	gnetAssert(submarine);
	CSubmarineHandling* subHandling = submarine->GetSubHandling();
	gnetAssert(subHandling);

	subHandling->SetYawControl(data.m_yaw);
	subHandling->SetPitchControl(data.m_pitch);
	subHandling->SetDiveControl(data.m_dive);
}

void CNetObjSubmarine::GetSubmarineGameState(CSubmarineGameStateDataNode& data)
{
    CSubmarine *submarine = GetSubmarine();
    gnetAssert(submarine);
	CSubmarineHandling* subHandling = submarine->GetSubHandling();
	gnetAssert(subHandling);

    data.m_IsAnchored = subHandling->GetAnchorHelper().IsAnchored();
}

void CNetObjSubmarine::SetSubmarineGameState(const CSubmarineGameStateDataNode& data)
{
    m_IsAnchored = data.m_IsAnchored;
}

// name:        ProcessControl
// description: Called from CGameWorld::Process, called in the same place as the local entity process controls
bool CNetObjSubmarine::ProcessControl()
{
	CSubmarine* submarine = GetSubmarine();
	gnetAssert(submarine);
	CSubmarineHandling* subHandling = submarine->GetSubHandling();
	gnetAssert(subHandling);

	CNetObjVehicle::ProcessControl();

	if (IsClone())
	{
        // clone submarines should never be anchored as it causes problems with the prediction code
        subHandling->GetAnchorHelper().Anchor(false);

		submarine->ProcessBuoyancy();
	}

	return true;
}

void CNetObjSubmarine::ChangeOwner(const netPlayer& player, eMigrationType migrationType)
{
    bool bWasClone = IsClone();

    CNetObjVehicle::ChangeOwner(player, migrationType);

    if(bWasClone && !IsClone())
    {
        CSubmarine *submarine = GetSubmarine();

        if(submarine)
        {
			CSubmarineHandling* subHandling = submarine->GetSubHandling();
			gnetAssert(subHandling);

			//Anchor if desired and it's safe to do so
			if (m_IsAnchored)
			{
				if (subHandling->GetAnchorHelper().CanAnchorHere(true))
				{
					subHandling->GetAnchorHelper().Anchor(true);
				}
				else
				{
					m_IsAnchored = false;
#if ENABLE_NETWORK_LOGGING
					NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManager().GetLog(), "DISCARDING_SUB_ANCHOR", GetLogName());
#endif
				}
			}
			else
			{
				subHandling->GetAnchorHelper().Anchor(false);
			}
        }
    }
}