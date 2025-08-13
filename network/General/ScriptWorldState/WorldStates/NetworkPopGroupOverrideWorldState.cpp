//
// name:		NetworkPopGroupOverrideWorldState.cpp
// description:	Support for network scripts to override pop group percentages via the script world state management code
// written by:	Daniel Yelland
//
//
#include "NetworkPopGroupOverrideWorldState.h"

#include "bank/bkmgr.h"
#include "bank/bank.h"
#include "grcore/debugdraw.h"
#include "vector/geometry.h"

#include "fwnet/NetLogUtils.h"

#include "Network/NetworkInterface.h"
#include "Peds/PopCycle.h"

NETWORK_OPTIMISATIONS()

static const unsigned MAX_POP_GROUP_OVERRIDE_DATA = 50;
FW_INSTANTIATE_CLASS_POOL(CNetworkPopGroupOverrideWorldStateData, MAX_POP_GROUP_OVERRIDE_DATA, atHashString("CNetworkPopGroupOverrideWorldStateData",0xb24957d2));

CNetworkPopGroupOverrideWorldStateData::CNetworkPopGroupOverrideWorldStateData() :
m_PopSchedule(-1)
, m_PopGroupHash(0)
, m_Percentage(0)
{
}

CNetworkPopGroupOverrideWorldStateData::CNetworkPopGroupOverrideWorldStateData(const CGameScriptId &scriptID,
                                                                               bool                 locallyOwned,
                                                                               int                  popSchedule,
                                                                               u32                  popGroupHash,
                                                                               u32                  percentage) :
CNetworkWorldStateData(scriptID, locallyOwned)
, m_PopSchedule(popSchedule)
, m_PopGroupHash(popGroupHash)
, m_Percentage(percentage)
{
}

CNetworkPopGroupOverrideWorldStateData *CNetworkPopGroupOverrideWorldStateData::Clone() const
{
    return rage_new CNetworkPopGroupOverrideWorldStateData(GetScriptID(), IsLocallyOwned(), m_PopSchedule, m_PopGroupHash, m_Percentage); 
}

bool CNetworkPopGroupOverrideWorldStateData::IsEqualTo(const CNetworkWorldStateData &worldState) const
{
    if(CNetworkWorldStateData::IsEqualTo(worldState))
    {
        const CNetworkPopGroupOverrideWorldStateData *popGroupOverrideState = SafeCast(const CNetworkPopGroupOverrideWorldStateData, &worldState);

        if(m_PopSchedule  == popGroupOverrideState->m_PopSchedule  &&
           m_PopGroupHash == popGroupOverrideState->m_PopGroupHash &&
           m_Percentage   == popGroupOverrideState->m_Percentage)
        {
            return true;
        }
    }

    return false;
}

void CNetworkPopGroupOverrideWorldStateData::ChangeWorldState()
{
    NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManagerLog(), "APPLYING_POP_GROUP_OVERRIDE", "");
    LogState(NetworkInterface::GetObjectManagerLog());

    CPopSchedule& popSchedule = CPopCycle::GetPopSchedules().GetSchedule(m_PopSchedule);
	popSchedule.SetOverridePedGroup(m_PopGroupHash, m_Percentage);
}

void CNetworkPopGroupOverrideWorldStateData::RevertWorldState()
{
    NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManagerLog(), "REMOVING_POP_GROUP_OVERRIDE", "");
    LogState(NetworkInterface::GetObjectManagerLog());

    CPopSchedule& popSchedule = CPopCycle::GetPopSchedules().GetSchedule(m_PopSchedule);
	popSchedule.SetOverridePedGroup(0, 0);
}

void CNetworkPopGroupOverrideWorldStateData::OverridePopGroupPercentage(const CGameScriptId &scriptID,
                                                                        int                  popSchedule,
                                                                        u32                  popGroupHash,
                                                                        u32                  percentage)
{
    CNetworkPopGroupOverrideWorldStateData worldStateData(scriptID, true, popSchedule, popGroupHash, percentage);
    CNetworkScriptWorldStateMgr::ChangeWorldState(worldStateData, false);
}

void CNetworkPopGroupOverrideWorldStateData::RemovePopGroupPercentageOverride(const CGameScriptId &scriptID,
                                                                              int                  popScheduleID,
                                                                              u32                  popGroupHash,
                                                                              u32                  percentage)
{
    CNetworkPopGroupOverrideWorldStateData worldStateData(scriptID, true, popScheduleID, popGroupHash, percentage);
    CNetworkScriptWorldStateMgr::RevertWorldState(worldStateData, false);
}

void CNetworkPopGroupOverrideWorldStateData::LogState(netLoggingInterface &log) const
{
    log.WriteDataValue("Script Name", "%s", GetScriptID().GetLogName());
#if !__FINAL
    log.WriteDataValue("Pop Schedule", "%s", CPopCycle::GetPopSchedules().GetNameFromIndex(m_PopSchedule));
    const char *popGroupName = 0;
    
    u32 popGroupIndex = 0;
    if(CPopCycle::GetPopGroups().FindPedGroupFromNameHash(m_PopGroupHash, popGroupIndex))
    {
        popGroupName = CPopCycle::GetPopGroups().GetPedGroup(popGroupIndex).GetName().TryGetCStr();
    }

    if(popGroupName)
    {
        log.WriteDataValue("Pop Group", "%s", popGroupName);
    }
    else
#endif
    {
        log.WriteDataValue("Pop Group", "%d", m_PopGroupHash);
    }

    log.WriteDataValue("Percentage", "%d", m_Percentage);
}

void CNetworkPopGroupOverrideWorldStateData::Serialise(CSyncDataReader &serialiser)
{
    SerialiseWorldState(serialiser);
}

void CNetworkPopGroupOverrideWorldStateData::Serialise(CSyncDataWriter &serialiser)
{
    SerialiseWorldState(serialiser);
}

template <class Serialiser> void CNetworkPopGroupOverrideWorldStateData::SerialiseWorldState(Serialiser &serialiser)
{
    const unsigned SIZEOF_POP_SCHEDULE = 8;
    const unsigned SIZEOF_POP_GROUP    = 32;
    const unsigned SIZEOF_PERCENTAGE   = 7;

    GetScriptID().Serialise(serialiser);
    SERIALISE_INTEGER(serialiser, m_PopSchedule,    SIZEOF_POP_SCHEDULE, "Pop Schedule");
    SERIALISE_UNSIGNED(serialiser, m_PopGroupHash,  SIZEOF_POP_GROUP,    "Pop Group");
    SERIALISE_UNSIGNED(serialiser, m_Percentage,    SIZEOF_PERCENTAGE,   "Percentage");
}

#if __DEV

bool CNetworkPopGroupOverrideWorldStateData::IsConflicting(const CNetworkWorldStateData &worldState) const
{
    bool conflicting = false;

    if(!IsEqualTo(worldState))
    {
        if(worldState.GetType() == GetType())
        {
            const CNetworkPopGroupOverrideWorldStateData *popGroupOverrideState = SafeCast(const CNetworkPopGroupOverrideWorldStateData, &worldState);

            conflicting = (m_PopSchedule == popGroupOverrideState->m_PopSchedule);
        }
    }

    return conflicting;
}

#endif // __DEV

#if __BANK

static void NetworkBank_OverridePopGroupPercentage()
{
    if(NetworkInterface::IsGameInProgress())
    {
        CGameScriptId scriptID("cops_and_crooks", -1);

        const int popScheduleID = 92;         // Vagos gang
        const u32 popGroupHash  = 1725084566; // Vagos gang ped
        const int percentage    = 100;

        CPopSchedule& popSchedule = CPopCycle::GetPopSchedules().GetSchedule(popScheduleID);
	    popSchedule.SetOverridePedGroup(popGroupHash, percentage);

        CNetworkPopGroupOverrideWorldStateData::OverridePopGroupPercentage(scriptID, popScheduleID, popGroupHash, percentage);
    }
}

static void NetworkBank_RemovePopGroupPercentageOverride()
{
    if(NetworkInterface::IsGameInProgress())
    {
        CGameScriptId scriptID("cops_and_crooks", -1);

        const int popScheduleID = 92; // Vagos gang

        CPopSchedule& popSchedule = CPopCycle::GetPopSchedules().GetSchedule(popScheduleID);

        const u32 popGroupHash  = popSchedule.GetOverridePedGroup(); // Vagos gang ped
        const int percentage    = popSchedule.GetOverridePedPercentage();

	    popSchedule.SetOverridePedGroup(0, 0);

        CNetworkPopGroupOverrideWorldStateData::RemovePopGroupPercentageOverride(scriptID, popScheduleID, popGroupHash, percentage);
    }
}

void CNetworkPopGroupOverrideWorldStateData::AddDebugWidgets()
{
    bkBank *bank = BANKMGR.FindBank("Network");

    if(gnetVerifyf(bank, "Unable to find network bank!"))
    {
        bank->PushGroup("Pop Group Override", false);
        {
            bank->AddButton("Override Pop Group Percentage",        datCallback(NetworkBank_OverridePopGroupPercentage));
            bank->AddButton("Remove Pop Group Percentage Override", datCallback(NetworkBank_RemovePopGroupPercentageOverride));
        }
        bank->PopGroup();
    }
}

void CNetworkPopGroupOverrideWorldStateData::DisplayDebugText()
{
    CNetworkPopGroupOverrideWorldStateData::Pool *pool = CNetworkPopGroupOverrideWorldStateData::GetPool();

	s32 i = pool->GetSize();

	while(i--)
	{
		CNetworkPopGroupOverrideWorldStateData *worldState = pool->GetSlot(i);

		if(worldState)
		{
            const char *popGroupName = 0;
    
            u32 popGroupIndex = 0;
            if(CPopCycle::GetPopGroups().FindPedGroupFromNameHash(worldState->m_PopGroupHash, popGroupIndex))
            {
                popGroupName = CPopCycle::GetPopGroups().GetPedGroup(popGroupIndex).GetName().TryGetCStr();
            }

            if(popGroupName)
            {
                grcDebugDraw::AddDebugOutput("Pop Group Percentage Override: %s (%s,%s,%d)", worldState->GetScriptID().GetLogName(),
                                                                                             CPopCycle::GetPopSchedules().GetNameFromIndex(worldState->m_PopSchedule),
                                                                                             popGroupName,
                                                                                             worldState->m_Percentage);
            }
            else
            {
                grcDebugDraw::AddDebugOutput("Pop Group Percentage Override: %s (%s,%d,%d)", worldState->GetScriptID().GetLogName(),
                                                                                             CPopCycle::GetPopSchedules().GetNameFromIndex(worldState->m_PopSchedule),
                                                                                             worldState->m_PopGroupHash,
                                                                                             worldState->m_Percentage);
            }
        }
    }
}
#endif // __BANK
