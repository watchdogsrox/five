//
// name:		NetworkRoadNodeWorldState.cpp
// description:	Support for network scripts to switch road nodes off via the script world state management code
// written by:	Daniel Yelland
//
#include "NetworkRoadNodeWorldState.h"

#include "bank/bkmgr.h"
#include "bank/bank.h"
#include "vector/geometry.h"

#include "fwnet/NetLogUtils.h"

#include "Network/NetworkInterface.h"
#include "peds/Ped.h"
#include "VehicleAI/PathFind.h"

NETWORK_OPTIMISATIONS()

static const unsigned MAX_ROAD_NODES_DATA = 20;
FW_INSTANTIATE_CLASS_POOL(CNetworkRoadNodeWorldStateData, MAX_ROAD_NODES_DATA, atHashString("CNetworkRoadNodeWorldStateData",0x7fee19a6));

CNetworkRoadNodeWorldStateData::CNetworkRoadNodeWorldStateData()
{
    m_NodeRegionMin.Zero();
    m_NodeRegionMax.Zero();
	m_BoxWidth = 0.0f;
	m_bAxisAligned = false;
	m_nodeState = eToOriginal;
}

CNetworkRoadNodeWorldStateData::CNetworkRoadNodeWorldStateData(const CGameScriptId &scriptID,
                                                               bool                 locallyOwned,
                                                               const Vector3       &regionMin,
                                                               const Vector3       &regionMax,
															   NodeSwitchState		nodeState) :
CNetworkWorldStateData(scriptID, locallyOwned)
{
    m_NodeRegionMin = regionMin;
    m_NodeRegionMax = regionMax;
	m_BoxWidth = 0.0f;
	m_bAxisAligned = true;
	m_nodeState = nodeState;
}

CNetworkRoadNodeWorldStateData::CNetworkRoadNodeWorldStateData(const CGameScriptId &scriptID,
															   bool                 locallyOwned,
															   const Vector3       &regionStart,
															   const Vector3       &regionEnd,
															   float				regionWidth,
															   NodeSwitchState		nodeState) :
CNetworkWorldStateData(scriptID, locallyOwned)
{
	m_NodeRegionMin = regionStart;
	m_NodeRegionMax = regionEnd;
	m_BoxWidth = regionWidth;
	m_bAxisAligned = false;
	m_nodeState = nodeState;
}

CNetworkRoadNodeWorldStateData *CNetworkRoadNodeWorldStateData::Clone() const
{
	if( m_bAxisAligned )
	{
		// Axis aligned version
		return rage_new CNetworkRoadNodeWorldStateData(GetScriptID(), IsLocallyOwned(), m_NodeRegionMin, m_NodeRegionMax, m_nodeState);
	}
	else
	{
		// Non axis aligned version
		return rage_new CNetworkRoadNodeWorldStateData(GetScriptID(), IsLocallyOwned(), m_NodeRegionMin, m_NodeRegionMax, m_BoxWidth, m_nodeState);
	}
}


void CNetworkRoadNodeWorldStateData::GetRegion(Vector3 &min, Vector3 &max) const
{
    min = m_NodeRegionMin;
    max = m_NodeRegionMax;
}

bool CNetworkRoadNodeWorldStateData::IsEqualTo(const CNetworkWorldStateData &worldState) const
{
    if(CNetworkWorldStateData::IsEqualTo(worldState))
    {
        const CNetworkRoadNodeWorldStateData *roadNodeState = SafeCast(const CNetworkRoadNodeWorldStateData, &worldState);

        const float epsilon = 0.2f; // a relatively high epsilon value to take quantisation from network serialising into account

        if(m_bAxisAligned == roadNodeState->m_bAxisAligned &&
           m_NodeRegionMin.IsClose(roadNodeState->m_NodeRegionMin, epsilon) &&
           m_NodeRegionMax.IsClose(roadNodeState->m_NodeRegionMax, epsilon) &&
           IsClose(m_BoxWidth, roadNodeState->m_BoxWidth, epsilon))
        {
			//if the new worldState is a reversion then return equal
			if(m_nodeState == roadNodeState->m_nodeState || roadNodeState->m_nodeState == eToOriginal)
			{
				return true;
			}
			else
			{
#if __ASSERT
				//check if we've got an activate and a deactivate in the same area. This is wrong
				if((m_nodeState == eActivate && roadNodeState->m_nodeState == eDeactivate) || 
					(m_nodeState == eDeactivate && roadNodeState->m_nodeState == eActivate))
				{
					gnetAssertf(false, "Found two CNetworkRoadNodeWorldStateData in same area trying to turn nodes on/off");
				}
#endif
				return true;
			}    
        }
    }

    return false;
}

void CNetworkRoadNodeWorldStateData::ChangeWorldState()
{
	if( m_bAxisAligned )
	{
        NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManagerLog(), "DISABLING_ROAD_NODES", 
																			"%s: (%.2f, %.2f, %.2f)->(%.2f, %.2f, %.2f)",
																			GetScriptID().GetLogName(),
																			m_NodeRegionMin.x, m_NodeRegionMin.y, m_NodeRegionMin.z,
																			m_NodeRegionMax.x, m_NodeRegionMax.y, m_NodeRegionMax.z);

		ThePaths.SwitchRoadsOffInArea(0,	m_NodeRegionMin.x, m_NodeRegionMax.x,
											m_NodeRegionMin.y, m_NodeRegionMax.y,
											m_NodeRegionMin.z, m_NodeRegionMax.z, m_nodeState == eDeactivate, true, false, false);
	}
	else
	{
        NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManagerLog(), "DISABLING_ROAD_NODES_ANGLED",  
                                                                            "%s: (%.2f, %.2f, %.2f)->(%.2f, %.2f, %.2f) Width: %.2f",
																			GetScriptID().GetLogName(),
																			m_NodeRegionMin.x, m_NodeRegionMin.y, m_NodeRegionMin.z,
																			m_NodeRegionMax.x, m_NodeRegionMax.y, m_NodeRegionMax.z,
                                                                            m_BoxWidth);

		Vector3 vStart(m_NodeRegionMin.x, m_NodeRegionMin.y, m_NodeRegionMin.z);
		Vector3 vEnd(m_NodeRegionMax.x, m_NodeRegionMax.y, m_NodeRegionMax.z);
		ThePaths.SwitchRoadsOffInAngledArea(0,	vStart, vEnd, m_BoxWidth, m_nodeState == eDeactivate, true, false, false);
	}
}

void CNetworkRoadNodeWorldStateData::RevertWorldState()
{
	if( m_bAxisAligned )
	{
        NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManagerLog(), "ENABLING_ROAD_NODES", 
																			"%s: (%.2f, %.2f, %.2f)->(%.2f, %.2f, %.2f)",
																			GetScriptID().GetLogName(),
																			m_NodeRegionMin.x, m_NodeRegionMin.y, m_NodeRegionMin.z,
																			m_NodeRegionMax.x, m_NodeRegionMax.y, m_NodeRegionMax.z);

        bool canEnableRoadNodes = true;

        CNetworkRoadNodeWorldStateData::Pool *pool = CNetworkRoadNodeWorldStateData::GetPool();

	    s32 i = pool->GetSize();

	    while(i--)
	    {
		    CNetworkRoadNodeWorldStateData *roadNodeState = pool->GetSlot(i);

		    if(roadNodeState && roadNodeState != this)
		    {
                const float epsilon = 0.2f; // a relatively high epsilon value to take quantisation from network serialising into account

                if(m_NodeRegionMin.IsClose(roadNodeState->m_NodeRegionMin, epsilon) &&
                   m_NodeRegionMax.IsClose(roadNodeState->m_NodeRegionMax, epsilon) && roadNodeState->m_nodeState != eToOriginal)
                {
                    canEnableRoadNodes = false;
                }
            }
        }

        if(canEnableRoadNodes)
        {
	        ThePaths.SwitchRoadsOffInArea(0, m_NodeRegionMin.x, m_NodeRegionMax.x,
		                                     m_NodeRegionMin.y, m_NodeRegionMax.y,
			                                 m_NodeRegionMin.z, m_NodeRegionMax.z, FALSE, true, true, false);
        }
	}
	else
	{
        NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManagerLog(), "ENABLING_ROAD_NODES_ANGLED", 
																			"%s: (%.2f, %.2f, %.2f)->(%.2f, %.2f, %.2f) Width: %.2f",
																			GetScriptID().GetLogName(),
																			m_NodeRegionMin.x, m_NodeRegionMin.y, m_NodeRegionMin.z,
																			m_NodeRegionMax.x, m_NodeRegionMax.y, m_NodeRegionMax.z,
                                                                            m_BoxWidth);

		Vector3 vStart(m_NodeRegionMin.x, m_NodeRegionMin.y, m_NodeRegionMin.z);
		Vector3 vEnd(m_NodeRegionMax.x, m_NodeRegionMax.y, m_NodeRegionMax.z);
		ThePaths.SwitchRoadsOffInAngledArea(0,	vStart, vEnd, m_BoxWidth, FALSE, true, true, false);
	}
}

void CNetworkRoadNodeWorldStateData::SetNodesToOriginal(const CGameScriptId &scriptID,
														const Vector3       &regionMin,
														const Vector3       &regionMax,
														bool                 fromNetwork)
{
	CNetworkRoadNodeWorldStateData worldStateData(scriptID, !fromNetwork, regionMin, regionMax, eToOriginal);
	CNetworkScriptWorldStateMgr::RevertWorldState(worldStateData, fromNetwork);    
}

void CNetworkRoadNodeWorldStateData::SetNodesToOriginalAngled(const CGameScriptId &scriptID,
															  const Vector3       &regionStart,
															  const Vector3       &regionEnd,
															  float				  regionWidth,
															  bool                 fromNetwork)
{
	CNetworkRoadNodeWorldStateData worldStateData(scriptID, !fromNetwork, regionStart, regionEnd, regionWidth, eToOriginal);
	CNetworkScriptWorldStateMgr::RevertWorldState(worldStateData, fromNetwork);    
}

void CNetworkRoadNodeWorldStateData::SwitchOnNodes(const CGameScriptId &scriptID,
												   const Vector3       &regionMin,
												   const Vector3       &regionMax,
												   bool                 fromNetwork)
{
	CNetworkRoadNodeWorldStateData worldStateData(scriptID, !fromNetwork, regionMin, regionMax, eActivate);
	CNetworkScriptWorldStateMgr::ChangeWorldState(worldStateData, fromNetwork);    
}

void CNetworkRoadNodeWorldStateData::SwitchOnNodesAngled(const CGameScriptId &scriptID,
														 const Vector3       &regionStart,
														 const Vector3       &regionEnd,
														 float				  regionWidth,
														 bool                 fromNetwork)
{
	CNetworkRoadNodeWorldStateData worldStateData(scriptID, !fromNetwork, regionStart, regionEnd, regionWidth, eActivate);
	CNetworkScriptWorldStateMgr::ChangeWorldState(worldStateData, fromNetwork);    
}

void CNetworkRoadNodeWorldStateData::SwitchOffNodes(const CGameScriptId &scriptID,
													const Vector3       &regionMin,
													const Vector3       &regionMax,
													bool                 fromNetwork)
{
	CNetworkRoadNodeWorldStateData worldStateData(scriptID, !fromNetwork, regionMin, regionMax, eDeactivate);
	CNetworkScriptWorldStateMgr::ChangeWorldState(worldStateData, fromNetwork);
}

void CNetworkRoadNodeWorldStateData::SwitchOffNodesAngled(const CGameScriptId &scriptID,
														  const Vector3       &regionStart,
														  const Vector3       &regionEnd,
														  float				   regionWidth,
														  bool                 fromNetwork)
{
	CNetworkRoadNodeWorldStateData worldStateData(scriptID, !fromNetwork, regionStart, regionEnd, regionWidth, eDeactivate);
	CNetworkScriptWorldStateMgr::ChangeWorldState(worldStateData, fromNetwork);
}

void CNetworkRoadNodeWorldStateData::LogState(netLoggingInterface &log) const
{
    log.WriteDataValue("Script Name", "%s", GetScriptID().GetLogName());
    log.WriteDataValue("Axis Aligned", "%s", m_bAxisAligned ? "TRUE" : "FALSE");
    log.WriteDataValue("Min", "%.2f, %.2f, %.2f", m_NodeRegionMin.x, m_NodeRegionMin.y, m_NodeRegionMin.z);
    log.WriteDataValue("Max", "%.2f, %.2f, %.2f", m_NodeRegionMax.x, m_NodeRegionMax.y, m_NodeRegionMax.z);
	log.WriteDataValue("Node state", "%s", m_nodeState == eToOriginal ? "To Original" : (m_nodeState == eActivate ? "Activate" : "Deactivate"));

    if(!m_bAxisAligned)
    {
        log.WriteDataValue("Box Width", "%.2f", m_BoxWidth);
    }
}

void CNetworkRoadNodeWorldStateData::Serialise(CSyncDataReader &serialiser)
{
    SerialiseWorldState(serialiser);
}

void CNetworkRoadNodeWorldStateData::Serialise(CSyncDataWriter &serialiser)
{
    SerialiseWorldState(serialiser);
}

template <class Serialiser> void CNetworkRoadNodeWorldStateData::SerialiseWorldState(Serialiser &serialiser)
{
    GetScriptID().Serialise(serialiser);
    SERIALISE_POSITION(serialiser, m_NodeRegionMin);
    SERIALISE_POSITION(serialiser, m_NodeRegionMax);
    SERIALISE_BOOL(serialiser, m_bAxisAligned);
	s8 iNodeState = (s8)m_nodeState;
	static const unsigned int SIZEOF_NODESWITCHSTATE = 3;
	SERIALISE_INTEGER(serialiser, iNodeState, SIZEOF_NODESWITCHSTATE);
	m_nodeState = (NodeSwitchState)iNodeState;

    if(!m_bAxisAligned)
    {
        const float    MAX_BOX_WIDTH    = 1000.0f;
        const unsigned SIZEOF_BOX_WIDTH = 16;
        SERIALISE_PACKED_FLOAT(serialiser, m_BoxWidth, MAX_BOX_WIDTH, SIZEOF_BOX_WIDTH);
    }
}

#if __DEV

bool CNetworkRoadNodeWorldStateData::IsConflicting(const CNetworkWorldStateData &worldState) const
{
    bool isConflicting = false;

    if(!IsEqualTo(worldState))
    {
        if(worldState.GetType() == GetType())
        {
            const CNetworkRoadNodeWorldStateData *roadNodeState = SafeCast(const CNetworkRoadNodeWorldStateData, &worldState);

            if(m_bAxisAligned && roadNodeState->m_bAxisAligned)
            {
                // road nodes world state is only conflicting if two different areas overlap, if the areas are exactly the
                // same the car gens will not be reactivated until the last one is reverted

                const float epsilon = 0.2f; // a relatively high epsilon value to take quantisation from network serialising into account

                if(!m_NodeRegionMin.IsClose(roadNodeState->m_NodeRegionMin, epsilon) ||
                   !m_NodeRegionMax.IsClose(roadNodeState->m_NodeRegionMax, epsilon))
                {
                    isConflicting = geomBoxes::TestAABBtoAABB(m_NodeRegionMin, m_NodeRegionMax,
                                                              roadNodeState->m_NodeRegionMin, roadNodeState->m_NodeRegionMax);
                }
            }
        }
    }

    return isConflicting;
}

#endif // __DEV

#if __BANK

static void NetworkBank_SwitchRoadNodesToOriginal()
{
	if(NetworkInterface::IsGameInProgress())
	{
		CPed *playerPed = FindFollowPed();

		if(playerPed)
		{
			CGameScriptId scriptID("freemode", -1);
			const Vector3 vPlayerPosition = VEC3V_TO_VECTOR3(playerPed->GetTransform().GetPosition());
			Vector3 boxStart = vPlayerPosition - Vector3(10.0f, 10.0f, 0.0f);
			Vector3 boxEnd   = vPlayerPosition + Vector3(10.0f, 10.0f, 0.0f);

			ThePaths.SwitchRoadsOffInArea(0, boxStart.x, boxEnd.x,
											 boxStart.y, boxEnd.y,
											 boxStart.z, boxEnd.z, FALSE, true, true, false);

			CNetworkRoadNodeWorldStateData::SetNodesToOriginal(scriptID, boxStart, boxEnd, false);
		}
	}
}

static void NetworkBank_SwitchOffRoadNodes()
{
    if(NetworkInterface::IsGameInProgress())
    {
        CPed *playerPed = FindFollowPed();

        if(playerPed)
        {
            CGameScriptId scriptID("freemode", -1);
			const Vector3 vPlayerPosition = VEC3V_TO_VECTOR3(playerPed->GetTransform().GetPosition());
            Vector3 boxStart = vPlayerPosition - Vector3(10.0f, 10.0f, 0.0f);
            Vector3 boxEnd   = vPlayerPosition + Vector3(10.0f, 10.0f, 0.0f);

            ThePaths.SwitchRoadsOffInArea(0, boxStart.x, boxEnd.x,
                                             boxStart.y, boxEnd.y,
                                             boxStart.z, boxEnd.z, TRUE, true, false, false);

            CNetworkRoadNodeWorldStateData::SwitchOffNodes(scriptID, boxStart, boxEnd, false);
        }
    }
}

static void NetworkBank_SwitchOnRoadNodes()
{
    if(NetworkInterface::IsGameInProgress())
    {
        CPed *playerPed = FindFollowPed();

        if(playerPed)
        {
            CGameScriptId scriptID("freemode", -1);
			const Vector3 vPlayerPosition = VEC3V_TO_VECTOR3(playerPed->GetTransform().GetPosition());
            Vector3 boxStart = vPlayerPosition - Vector3(10.0f, 10.0f, 0.0f);
            Vector3 boxEnd   = vPlayerPosition + Vector3(10.0f, 10.0f, 0.0f);

            ThePaths.SwitchRoadsOffInArea(0, boxStart.x, boxEnd.x,
                                             boxStart.y, boxEnd.y,
                                             boxStart.z, boxEnd.z, FALSE, true, false, false);

            CNetworkRoadNodeWorldStateData::SwitchOnNodes(scriptID, boxStart, boxEnd, false);
        }
    }
}

void CNetworkRoadNodeWorldStateData::AddDebugWidgets()
{
    bkBank *bank = BANKMGR.FindBank("Network");

    if(gnetVerifyf(bank, "Unable to find network bank!"))
    {
        bank->PushGroup("Road Nodes", false);
        {
            bank->AddButton("Switch Off Road Nodes Around Player", datCallback(NetworkBank_SwitchOffRoadNodes));
            bank->AddButton("Switch On Road Nodes Around Player", datCallback(NetworkBank_SwitchOnRoadNodes));
			bank->AddButton("Switch Road Nodes To Original Around Player", datCallback(NetworkBank_SwitchRoadNodesToOriginal));		
        }
        bank->PopGroup();
    }
}

void CNetworkRoadNodeWorldStateData::DisplayDebugText()
{
    CNetworkRoadNodeWorldStateData::Pool *pool = CNetworkRoadNodeWorldStateData::GetPool();

	s32 i = pool->GetSize();

	while(i--)
	{
		CNetworkRoadNodeWorldStateData *worldState = pool->GetSlot(i);

		if(worldState)
		{
            Vector3 min,max;
            worldState->GetRegion(min, max);
            grcDebugDraw::AddDebugOutput("Road nodes %s (%s): %s (%.2f,%.2f,%.2f)->(%.2f,%.2f,%.2f) width: %.2f",
										 worldState->m_nodeState == eToOriginal ? "To Original" : (worldState->m_nodeState == eActivate ? "Activate" : "Deactivate"),
                                         worldState->m_bAxisAligned ? "AABB" : "Angled",
                                         worldState->GetScriptID().GetLogName(),
                                         min.x, min.y, min.z,
                                         max.x, max.y, max.z,
                                         worldState->m_BoxWidth);
        }
    }
}
#endif // __BANK
