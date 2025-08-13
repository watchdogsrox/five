//
// name:		NetworkCarGenWorldState.cpp
// description:	Support for network scripts to switch car generators off via the script world state management code
// written by:	Daniel Yelland
//
//
#include "NetworkCarGenWorldState.h"

#include "bank/bkmgr.h"
#include "bank/bank.h"
#include "vector/geometry.h"

#include "fwnet/NetLogUtils.h"

#include "Network/NetworkInterface.h"
#include "peds/Ped.h"
#include "Vehicles/CarGen.h"

NETWORK_OPTIMISATIONS()

static const unsigned MAX_CAR_GENS_DATA = 100;
FW_INSTANTIATE_CLASS_POOL(CNetworkCarGenWorldStateData, MAX_CAR_GENS_DATA, atHashString("CNetworkCarGenWorldStateData",0x22167df0));

CNetworkCarGenWorldStateData::CNetworkCarGenWorldStateData()
{
    m_NodeRegionMin.Zero();
    m_NodeRegionMax.Zero();
}

CNetworkCarGenWorldStateData::CNetworkCarGenWorldStateData(const CGameScriptId &scriptID,
                                                           bool                 locallyOwned,
                                                           const Vector3       &regionMin,
                                                           const Vector3       &regionMax) :
CNetworkWorldStateData(scriptID, locallyOwned)
{
    m_NodeRegionMin = regionMin;
    m_NodeRegionMax = regionMax;
}

CNetworkCarGenWorldStateData *CNetworkCarGenWorldStateData::Clone() const
{
    return rage_new CNetworkCarGenWorldStateData(GetScriptID(), IsLocallyOwned(), m_NodeRegionMin, m_NodeRegionMax); 
}

void CNetworkCarGenWorldStateData::GetRegion(Vector3 &min, Vector3 &max) const
{
    min = m_NodeRegionMin;
    max = m_NodeRegionMax;
}

bool CNetworkCarGenWorldStateData::IsEqualTo(const CNetworkWorldStateData &worldState) const
{
    if(CNetworkWorldStateData::IsEqualTo(worldState))
    {
        const CNetworkCarGenWorldStateData *carGenState = SafeCast(const CNetworkCarGenWorldStateData, &worldState);

        const float epsilon = 0.2f; // a relatively high epsilon value to take quantisation from network serialising into account

        if(m_NodeRegionMin.IsClose(carGenState->m_NodeRegionMin, epsilon) &&
           m_NodeRegionMax.IsClose(carGenState->m_NodeRegionMax, epsilon))
        {
            return true;
        }
    }

    return false;
}

void CNetworkCarGenWorldStateData::ChangeWorldState()
{
    NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManagerLog(), "DISABLING_CAR_GENERATORS", 
                                                                            "%s: (%.2f, %.2f, %.2f)->(%.2f, %.2f, %.2f)",
                                                                            GetScriptID().GetLogName(),
                                                                            m_NodeRegionMin.x, m_NodeRegionMin.y, m_NodeRegionMin.z,
                                                                            m_NodeRegionMax.x, m_NodeRegionMax.y, m_NodeRegionMax.z);
    CTheCarGenerators::SetCarGeneratorsActiveInArea(m_NodeRegionMin, m_NodeRegionMax, false);
}

void CNetworkCarGenWorldStateData::RevertWorldState()
{
    NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManagerLog(), "ENABLING_CAR_GENERATORS", 
                                                                            "%s: (%.2f, %.2f, %.2f)->(%.2f, %.2f, %.2f)",
                                                                            GetScriptID().GetLogName(),
                                                                            m_NodeRegionMin.x, m_NodeRegionMin.y, m_NodeRegionMin.z,
                                                                            m_NodeRegionMax.x, m_NodeRegionMax.y, m_NodeRegionMax.z);

    bool canEnableCarGens = true;

    CNetworkCarGenWorldStateData::Pool *pool = CNetworkCarGenWorldStateData::GetPool();

	s32 i = pool->GetSize();

	while(i--)
	{
		CNetworkCarGenWorldStateData *carGenState = pool->GetSlot(i);

		if(carGenState && carGenState != this)
		{
            const float epsilon = 0.2f; // a relatively high epsilon value to take quantisation from network serialising into account

            if(m_NodeRegionMin.IsClose(carGenState->m_NodeRegionMin, epsilon) &&
               m_NodeRegionMax.IsClose(carGenState->m_NodeRegionMax, epsilon))
            {
                canEnableCarGens = false;
            }
        }
    }

    if(canEnableCarGens)
    {
        CTheCarGenerators::SetCarGeneratorsActiveInArea(m_NodeRegionMin, m_NodeRegionMax, true);
    }
}

void CNetworkCarGenWorldStateData::SwitchOnCarGenerators(const CGameScriptId &scriptID,
                                                         const Vector3       &regionMin,
                                                         const Vector3       &regionMax,
                                                         bool                 fromNetwork)
{
    CNetworkCarGenWorldStateData worldStateData(scriptID, !fromNetwork, regionMin, regionMax);
    CNetworkScriptWorldStateMgr::RevertWorldState(worldStateData, fromNetwork);    
}

void CNetworkCarGenWorldStateData::SwitchOffCarGenerators(const CGameScriptId &scriptID,
                                                          const Vector3       &regionMin,
                                                          const Vector3       &regionMax,
                                                          bool                 fromNetwork)
{
    CNetworkCarGenWorldStateData worldStateData(scriptID, !fromNetwork, regionMin, regionMax);
    CNetworkScriptWorldStateMgr::ChangeWorldState(worldStateData, fromNetwork);
}

void CNetworkCarGenWorldStateData::LogState(netLoggingInterface &log) const
{
    log.WriteDataValue("Script Name", "%s", GetScriptID().GetLogName());
    log.WriteDataValue("Min", "%.2f, %.2f, %.2f", m_NodeRegionMin.x, m_NodeRegionMin.y, m_NodeRegionMin.z);
    log.WriteDataValue("Max", "%.2f, %.2f, %.2f", m_NodeRegionMax.x, m_NodeRegionMax.y, m_NodeRegionMax.z);
}

void CNetworkCarGenWorldStateData::Serialise(CSyncDataReader &serialiser)
{
    SerialiseWorldState(serialiser);
}

void CNetworkCarGenWorldStateData::Serialise(CSyncDataWriter &serialiser)
{
    SerialiseWorldState(serialiser);
}

template <class Serialiser> void CNetworkCarGenWorldStateData::SerialiseWorldState(Serialiser &serialiser)
{
    GetScriptID().Serialise(serialiser);
    SERIALISE_POSITION(serialiser, m_NodeRegionMin, NULL, MINMAX_POSITION_PRECISION);
    SERIALISE_POSITION(serialiser, m_NodeRegionMax, NULL, MINMAX_POSITION_PRECISION);
}

#if __DEV

bool CNetworkCarGenWorldStateData::IsConflicting(const CNetworkWorldStateData &worldState) const
{
    bool isConflicting = false;

    if(!IsEqualTo(worldState))
    {
        if(worldState.GetType() == GetType())
        {
            const CNetworkCarGenWorldStateData *carGenState = SafeCast(const CNetworkCarGenWorldStateData, &worldState);

            // car generators world state is only conflicting if two different areas overlap, if the areas are exactly the
            // same the car gens will not be reactivated until the last one is reverted

            const float epsilon = 0.2f; // a relatively high epsilon value to take quantisation from network serialising into account

            if(!m_NodeRegionMin.IsClose(carGenState->m_NodeRegionMin, epsilon) ||
               !m_NodeRegionMax.IsClose(carGenState->m_NodeRegionMax, epsilon))
            {
                isConflicting = geomBoxes::TestAABBtoAABB(m_NodeRegionMin, m_NodeRegionMax,
                                                          carGenState->m_NodeRegionMin, carGenState->m_NodeRegionMax);
            }
        }
    }

    return isConflicting;
}

#endif // __DEV

#if __BANK

static void NetworkBank_SwitchOffCarGenerators()
{
    if(NetworkInterface::IsGameInProgress())
    {
        CPed *playerPed = FindFollowPed();

        if(playerPed)
        {
            CGameScriptId scriptID("freemode", -1);
			const Vector3 vPlayerPosition = VEC3V_TO_VECTOR3(playerPed->GetTransform().GetPosition());
            Vector3 boxStart = vPlayerPosition - Vector3(10.0f, 10.0f, 10.0f);
            Vector3 boxEnd   = vPlayerPosition + Vector3(10.0f, 10.0f, 10.0f);

            CTheCarGenerators::SetCarGeneratorsActiveInArea(boxStart, boxEnd, false);

            CNetworkCarGenWorldStateData::SwitchOffCarGenerators(scriptID, boxStart, boxEnd, false);
        }
    }
}

static void NetworkBank_SwitchOnCarGenerators()
{
    if(NetworkInterface::IsGameInProgress())
    {
        CPed *playerPed = FindFollowPed();

        if(playerPed)
        {
            CGameScriptId scriptID("freemode", -1);
			const Vector3 vPlayerPosition = VEC3V_TO_VECTOR3(playerPed->GetTransform().GetPosition());
            Vector3 boxStart = vPlayerPosition - Vector3(10.0f, 10.0f, 10.0f);
            Vector3 boxEnd   = vPlayerPosition + Vector3(10.0f, 10.0f, 10.0f);

            CTheCarGenerators::SetCarGeneratorsActiveInArea(boxStart, boxEnd, true);

            CNetworkCarGenWorldStateData::SwitchOnCarGenerators(scriptID, boxStart, boxEnd, false);
        }
    }
}

void CNetworkCarGenWorldStateData::AddDebugWidgets()
{
    bkBank *bank = BANKMGR.FindBank("Network");

    if(gnetVerifyf(bank, "Unable to find network bank!"))
    {
        bank->PushGroup("Car Generators", false);
        {
            bank->AddButton("Switch Off Car Generators Around Player", datCallback(NetworkBank_SwitchOffCarGenerators));
            bank->AddButton("Switch On Car Generators Around Player", datCallback(NetworkBank_SwitchOnCarGenerators));
        }
        bank->PopGroup();
    }
}

void CNetworkCarGenWorldStateData::DisplayDebugText()
{
    CNetworkCarGenWorldStateData::Pool *pool = CNetworkCarGenWorldStateData::GetPool();

	s32 i = pool->GetSize();

	while(i--)
	{
		CNetworkCarGenWorldStateData *worldState = pool->GetSlot(i);

		if(worldState)
		{
            Vector3 min,max;
            worldState->GetRegion(min, max);
            grcDebugDraw::AddDebugOutput("Car generators disabled: %s (%.2f,%.2f,%.2f)->(%.2f,%.2f,%.2f)", worldState->GetScriptID().GetLogName(),
                                                                                                          min.x, min.y, min.z,
                                                                                                          max.x, max.y, max.z);
        }
    }
}
#endif // __BANK
