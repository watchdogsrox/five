//
// name:		NetworkScenarioBlockingAreaWorldState.cpp
// description:	Support for network scripts to set up scenario blocking areas via the script world state management code
// written by:	Daniel Yelland
//
//
#include "NetworkScenarioBlockingAreaWorldState.h"

#include "bank/bkmgr.h"
#include "bank/bank.h"
#include "vector/geometry.h"

#include "fwnet/NetLogUtils.h"

#include "AI/BlockingBounds.h"
#include "Network/NetworkInterface.h"
#include "peds/Ped.h"
#include "Script/Script.h"
#include "Script/Handlers/GameScriptResources.h"

NETWORK_OPTIMISATIONS()

static const unsigned MAX_SCENARIO_BLOCKING_DATA = 50;
FW_INSTANTIATE_CLASS_POOL(CNetworkScenarioBlockingAreaWorldStateData, MAX_SCENARIO_BLOCKING_DATA, atHashString("CNetworkScenarioBlockingAreaWorldStateData",0xe59d025a));

CNetworkScenarioBlockingAreaWorldStateData::CNetworkScenarioBlockingAreaWorldStateData() :
m_BlockingAreaID(CScenarioBlockingAreas::kScenarioBlockingAreaIdInvalid)
{
    m_NodeRegionMin.Zero();
    m_NodeRegionMax.Zero();

	m_bCancelActive = m_bBlocksPeds = m_bBlocksVehicles = true;
}

CNetworkScenarioBlockingAreaWorldStateData::CNetworkScenarioBlockingAreaWorldStateData(const CGameScriptId &scriptID,
                                                                                       bool                 locallyOwned,
                                                                                       const Vector3       &regionMin,
                                                                                       const Vector3       &regionMax,
                                                                                       const int            blockingAreaID,
																					   bool					bCancelActive,
																					   bool					bBlocksPeds,
																					   bool					bBlocksVehicles) :
CNetworkWorldStateData(scriptID, locallyOwned)
, m_BlockingAreaID(blockingAreaID)
{
    m_NodeRegionMin = regionMin;
    m_NodeRegionMax = regionMax;
	m_bCancelActive = bCancelActive;
	m_bBlocksPeds = bBlocksPeds;
	m_bBlocksVehicles = bBlocksVehicles;
}

CNetworkScenarioBlockingAreaWorldStateData *CNetworkScenarioBlockingAreaWorldStateData::Clone() const
{
    return rage_new CNetworkScenarioBlockingAreaWorldStateData(GetScriptID(), IsLocallyOwned(), m_NodeRegionMin, m_NodeRegionMax, m_BlockingAreaID, m_bCancelActive, m_bBlocksPeds, m_bBlocksVehicles); 
}

void CNetworkScenarioBlockingAreaWorldStateData::GetRegion(Vector3 &min, Vector3 &max) const
{
    min = m_NodeRegionMin;
    max = m_NodeRegionMax;
}

bool CNetworkScenarioBlockingAreaWorldStateData::IsEqualTo(const CNetworkWorldStateData &worldState) const
{
    if(CNetworkWorldStateData::IsEqualTo(worldState))
    {
        const CNetworkScenarioBlockingAreaWorldStateData *scenarioBlockingState = SafeCast(const CNetworkScenarioBlockingAreaWorldStateData, &worldState);

        const float epsilon = 0.25f; // a relatively high epsilon value to take quantisation from network serialising into account

        if( m_bCancelActive == scenarioBlockingState->m_bCancelActive &&
			m_NodeRegionMin.IsClose(scenarioBlockingState->m_NodeRegionMin, epsilon) &&
			m_bBlocksPeds == scenarioBlockingState->m_bBlocksPeds &&
			m_bBlocksVehicles == scenarioBlockingState->m_bBlocksVehicles &&
           m_NodeRegionMax.IsClose(scenarioBlockingState->m_NodeRegionMax, epsilon))
        {
            return true;
        }
    }

    return false;
}

void CNetworkScenarioBlockingAreaWorldStateData::ChangeWorldState()
{
    NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManagerLog(), "ADDING_SCENARIO_BLOCKING_AREA", "");
    LogState(NetworkInterface::GetObjectManagerLog());

	m_BlockingAreaID = CScenarioBlockingAreas::AddScenarioBlockingArea(m_NodeRegionMin, m_NodeRegionMax, m_bCancelActive, m_bBlocksPeds, m_bBlocksVehicles, CScenarioBlockingAreas::kUserTypeNetScript);
}

void CNetworkScenarioBlockingAreaWorldStateData::RevertWorldState()
{
    NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManagerLog(), "REMOVING_SCENARIO_BLOCKING_AREA", "");
    LogState(NetworkInterface::GetObjectManagerLog());

    if(m_BlockingAreaID != CScenarioBlockingAreas::kScenarioBlockingAreaIdInvalid)
    {
        // scenario blocking areas are script resources so get cleaned up automatically by the script
        // that created them - we still need to clean the area up on remote machines though
        if(!CTheScripts::GetScriptHandlerMgr().GetScriptResource(CGameScriptResource::SCRIPT_RESOURCE_SCENARIO_BLOCKING_AREA, m_BlockingAreaID))
        {
            if(CScenarioBlockingAreas::IsScenarioBlockingAreaValid(m_BlockingAreaID))
            {
                CScenarioBlockingAreas::RemoveScenarioBlockingArea(m_BlockingAreaID);
            }
			else
			{
				gnetDebug1("CNetworkScenarioBlockingAreaWorldStateData::RevertWorldState Scenario blocking area is invalid! Blocking area ID %d", m_BlockingAreaID);
			}
        }
		else
		{
			gnetDebug1("CNetworkScenarioBlockingAreaWorldStateData::RevertWorldState Failed to get script resource from script handler manager!");
		}

        m_BlockingAreaID = CScenarioBlockingAreas::kScenarioBlockingAreaIdInvalid;
    }
	else
	{
		gnetDebug1("CNetworkScenarioBlockingAreaWorldStateData::RevertWorldState m_BlockingAreaID is invalid (0) !");
	}
}

void CNetworkScenarioBlockingAreaWorldStateData::AddScenarioBlockingArea(const CGameScriptId &scriptID,
                                                                         const Vector3       &regionMin,
                                                                         const Vector3       &regionMax,
                                                                         const int            blockingAreaID,
																		 bool				  bCancelActive,
																		 bool				  bBlocksPeds,
																		 bool				  bBlocksVehicles)
{
	if (gnetVerifyf(CNetworkScenarioBlockingAreaWorldStateData::GetPool()->GetNoOfFreeSpaces() > 0, "The CNetworkScenarioBlockingAreaWorldStateData pool is full"))
	{
		CNetworkScenarioBlockingAreaWorldStateData worldStateData(scriptID, true, regionMin, regionMax, blockingAreaID, bCancelActive, bBlocksPeds, bBlocksVehicles);
		CNetworkScriptWorldStateMgr::ChangeWorldState(worldStateData, false);
	}
}

void CNetworkScenarioBlockingAreaWorldStateData::RemoveScenarioBlockingArea(const CGameScriptId &scriptID,
                                                                            int                  blockingAreaID)
{
    // look up the scenario blocking area from the area ID
    CNetworkScenarioBlockingAreaWorldStateData::Pool *pool = CNetworkScenarioBlockingAreaWorldStateData::GetPool();

    bool foundArea = false;
    s32  poolIndex = pool->GetSize();

    while(poolIndex-- && !foundArea)
    {
        CNetworkScenarioBlockingAreaWorldStateData *blockingAreaWorldState = pool->GetSlot(poolIndex);

        if(blockingAreaWorldState && blockingAreaWorldState->m_BlockingAreaID == blockingAreaID)
		{
			if (scriptID.GetScriptNameHash() == blockingAreaWorldState->GetScriptID().GetScriptNameHash())
			{
				CNetworkScriptWorldStateMgr::RevertWorldState(*blockingAreaWorldState, false);
				foundArea = true;
			}
			else
			{
				char scriptName1[200];
				char scriptName2[200];
				formatf(scriptName1, blockingAreaWorldState->GetScriptID().GetLogName());
				formatf(scriptName2, scriptID.GetLogName());
			
				gnetAssertf(0, "Scenario blocking area %d has been added by %s. %s is attempting to remove it", blockingAreaID, scriptName1, scriptName2);
			}
        }
    }
}

void CNetworkScenarioBlockingAreaWorldStateData::LogState(netLoggingInterface &log) const
{
    log.WriteDataValue("Script Name", "%s", GetScriptID().GetLogName());
    log.WriteDataValue("Min", "%.2f, %.2f, %.2f", m_NodeRegionMin.x, m_NodeRegionMin.y, m_NodeRegionMin.z);
    log.WriteDataValue("Max", "%.2f, %.2f, %.2f", m_NodeRegionMax.x, m_NodeRegionMax.y, m_NodeRegionMax.z);
	log.WriteDataValue("Cancels Active", "%s", m_bCancelActive ? "true" : "false");
	log.WriteDataValue("Blocks Peds", "%s", m_bBlocksPeds ? "true" : "false");
	log.WriteDataValue("Blocks Vehicles", "%s", m_bBlocksVehicles ? "true" : "false");
}

void CNetworkScenarioBlockingAreaWorldStateData::Serialise(CSyncDataReader &serialiser)
{
    SerialiseWorldState(serialiser);
}

void CNetworkScenarioBlockingAreaWorldStateData::Serialise(CSyncDataWriter &serialiser)
{
    SerialiseWorldState(serialiser);
}

template <class Serialiser> void CNetworkScenarioBlockingAreaWorldStateData::SerialiseWorldState(Serialiser &serialiser)
{
    GetScriptID().Serialise(serialiser);
    SERIALISE_POSITION(serialiser, m_NodeRegionMin);
    SERIALISE_POSITION(serialiser, m_NodeRegionMax);
	SERIALISE_BOOL(serialiser, m_bCancelActive);
	SERIALISE_BOOL(serialiser, m_bBlocksPeds);
	SERIALISE_BOOL(serialiser, m_bBlocksVehicles);
}

#if __DEV

bool CNetworkScenarioBlockingAreaWorldStateData::IsConflicting(const CNetworkWorldStateData &worldState) const
{
    bool isConflicting = false;

    if(!IsEqualTo(worldState))
    {
        if(worldState.GetType() == GetType())
        {
            const CNetworkScenarioBlockingAreaWorldStateData *scenarioBlockingState = SafeCast(const CNetworkScenarioBlockingAreaWorldStateData, &worldState);

            // car generators world state is only conflicting if two different areas overlap, if the areas are exactly the
            // same the car gens will not be reactivated until the last one is reverted

            const float epsilon = 0.25f; // a relatively high epsilon value to take quantisation from network serialising into account

            if(!m_NodeRegionMin.IsClose(scenarioBlockingState->m_NodeRegionMin, epsilon) ||
               !m_NodeRegionMax.IsClose(scenarioBlockingState->m_NodeRegionMax, epsilon))
            {
                isConflicting = geomBoxes::TestAABBtoAABB(m_NodeRegionMin, m_NodeRegionMax,
                                                          scenarioBlockingState->m_NodeRegionMin, scenarioBlockingState->m_NodeRegionMax);
            }
        }
    }

    return isConflicting;
}

#endif // __DEV

#if __BANK

static int gBlockingAreaID = CScenarioBlockingAreas::kScenarioBlockingAreaIdInvalid;

static void NetworkBank_AddScenarioBlockingArea()
{
    if(NetworkInterface::IsGameInProgress())
    {
        CPed *playerPed = FindFollowPed();

        if(playerPed && (gBlockingAreaID == CScenarioBlockingAreas::kScenarioBlockingAreaIdInvalid))
        {
            CGameScriptId scriptID("freemode", -1);
			const Vector3 vPlayerPosition = VEC3V_TO_VECTOR3(playerPed->GetTransform().GetPosition());
            Vector3 boxStart = vPlayerPosition - Vector3(10.0f, 10.0f, 10.0f);
            Vector3 boxEnd   = vPlayerPosition + Vector3(10.0f, 10.0f, 10.0f);

			bool bBlocksPeds = true;
			bool bBlocksVehicles = true;
			gBlockingAreaID = CScenarioBlockingAreas::AddScenarioBlockingArea(boxStart, boxEnd, true, bBlocksPeds, bBlocksVehicles, CScenarioBlockingAreas::kUserTypeNetScript);

            CNetworkScenarioBlockingAreaWorldStateData::AddScenarioBlockingArea(scriptID, boxStart, boxEnd, gBlockingAreaID, true, bBlocksPeds, bBlocksVehicles );
        }
    }
}

static void NetworkBank_RemoveScenarioBlockingArea()
{
    if(NetworkInterface::IsGameInProgress())
    {
		if(gBlockingAreaID != CScenarioBlockingAreas::kScenarioBlockingAreaIdInvalid)
        {
            CScenarioBlockingAreas::RemoveScenarioBlockingArea(gBlockingAreaID);

            CGameScriptId scriptID("freemode", -1);
            CNetworkScenarioBlockingAreaWorldStateData::RemoveScenarioBlockingArea(scriptID, gBlockingAreaID);

            gBlockingAreaID = CScenarioBlockingAreas::kScenarioBlockingAreaIdInvalid;
        }
    }
}

void CNetworkScenarioBlockingAreaWorldStateData::AddDebugWidgets()
{
    bkBank *bank = BANKMGR.FindBank("Network");

    if(gnetVerifyf(bank, "Unable to find network bank!"))
    {
        bank->PushGroup("Scenario Blocking Areas", false);
        {
            bank->AddButton("Add scenario blocking area around player", datCallback(NetworkBank_AddScenarioBlockingArea));
            bank->AddButton("Remove scenario blocking area around player", datCallback(NetworkBank_RemoveScenarioBlockingArea));
        }
        bank->PopGroup();
    }
}

void CNetworkScenarioBlockingAreaWorldStateData::DisplayDebugText()
{
    CNetworkScenarioBlockingAreaWorldStateData::Pool *pool = CNetworkScenarioBlockingAreaWorldStateData::GetPool();

	s32 i = pool->GetSize();

	while(i--)
	{
		CNetworkScenarioBlockingAreaWorldStateData *worldState = pool->GetSlot(i);

		if(worldState)
		{
            Vector3 min,max;
            worldState->GetRegion(min, max);
            grcDebugDraw::AddDebugOutput("Scenarios Blocked: %s (%.2f,%.2f,%.2f)->(%.2f,%.2f,%.2f)", worldState->GetScriptID().GetLogName(),
                                                                                                     min.x, min.y, min.z,
                                                                                                     max.x, max.y, max.z);
        }
    }
}
#endif // __BANK
