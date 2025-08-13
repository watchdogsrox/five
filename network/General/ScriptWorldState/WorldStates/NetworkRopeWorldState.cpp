//
// name:		NetworkRopeWorldState.cpp
// description:	Support for network scripts to create ropes via the script world state management code
// written by:	Daniel Yelland
//
//
#include "NetworkRopeWorldState.h"

#include "bank/bkmgr.h"
#include "bank/bank.h"
#include "grcore/debugdraw.h"

#include "fwpheffects/ropemanager.h"
#include "fwnet/NetLogUtils.h"

#include "Network/NetworkInterface.h"
#include "Physics/GtaArchetype.h"
#include "Physics/Physics.h"
#include "Script/Script.h"
#include "Script/Handlers/GameScriptResources.h"

NETWORK_OPTIMISATIONS()

static const unsigned MAX_ROPES_DATA = 20;
FW_INSTANTIATE_CLASS_POOL(CNetworkRopeWorldStateData, MAX_ROPES_DATA, atHashString("CNetworkRopeWorldStateData",0x8cc8e56e));

static const int MAX_NUM_NETWORK_ROPE_IDS = 128;
int CNetworkRopeWorldStateData::ms_NextNetworkID = 0;

CNetworkRopeWorldStateData::CNetworkRopeWorldStateData() :
m_NetworkRopeID(-1)
, m_ScriptRopeID(-1)
{
}

CNetworkRopeWorldStateData::CNetworkRopeWorldStateData(const CGameScriptId &scriptID,
                                                       bool                 locallyOwned,
                                                       int                  networkRopeID,
                                                       int                  scriptRopeID,
                                                       const RopeParameters &parameters) :
CNetworkWorldStateData(scriptID, locallyOwned)
, m_RopeParameters(parameters)
, m_NetworkRopeID(networkRopeID)
, m_ScriptRopeID(scriptRopeID)
{
    if(locallyOwned && m_NetworkRopeID == -1)
    {
        bool isHostOfScript = CTheScripts::GetScriptHandlerMgr().IsNetworkHost(GetScriptID());

        if(isHostOfScript)
        {
            m_NetworkRopeID = ms_NextNetworkID++;

            if(ms_NextNetworkID >= MAX_NUM_NETWORK_ROPE_IDS)
            {
                ms_NextNetworkID = 0;
            }
        }
    }
}

CNetworkRopeWorldStateData *CNetworkRopeWorldStateData::Clone() const
{
    return rage_new CNetworkRopeWorldStateData(GetScriptID(), IsLocallyOwned(), m_NetworkRopeID, m_ScriptRopeID, m_RopeParameters);
}

bool CNetworkRopeWorldStateData::IsEqualTo(const CNetworkWorldStateData &worldState) const
{
    if(CNetworkWorldStateData::IsEqualTo(worldState))
    {
        const CNetworkRopeWorldStateData *ropeNodeState = SafeCast(const CNetworkRopeWorldStateData, &worldState);

        if(m_RopeParameters.IsEqualTo(ropeNodeState->m_RopeParameters))
        {
            return true;
        }
    }

    return false;
}

void CNetworkRopeWorldStateData::ChangeWorldState()
{
    NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManagerLog(), "CREATING_ROPE", "");
    LogState(NetworkInterface::GetObjectManagerLog());

    ropeManager *pRopeManager = CPhysics::GetRopeManager();

    if(gnetVerifyf(pRopeManager, "No rope manager!"))
    {
        float initLength = Selectf(m_RopeParameters.m_InitLength, m_RopeParameters.m_InitLength, m_RopeParameters.m_MaxLength);

		ropeInstance *rope = pRopeManager->AddRope(VECTOR3_TO_VEC3V(m_RopeParameters.m_Position),
                                                   VECTOR3_TO_VEC3V(m_RopeParameters.m_Rotation),
                                                   initLength,
                                                   m_RopeParameters.m_MinLength,
                                                   m_RopeParameters.m_MaxLength,
                                                   m_RopeParameters.m_LengthChangeRate,
                                                   m_RopeParameters.m_Type,
                                                   -1,
                                                   m_RopeParameters.m_PpuOnly,
                                                   m_RopeParameters.m_LockFromFront,
                                                   m_RopeParameters.m_TimeMultiplier,
                                                   m_RopeParameters.m_Breakable,
												   m_RopeParameters.m_Pinned
												   );

        if(gnetVerifyf(rope, "Failed to create a rope!"))
        {
// TODO: disabled by svetli
// ropes will be marked GTA_ENVCLOTH_OBJECT_TYPE by default, see init func in ropemanager.cpp
//             if(m_RopeParameters.m_CollisionOn)
             {
 				rope->SetPhysInstFlags( ArchetypeFlags::GTA_ENVCLOTH_OBJECT_TYPE, ArchetypeFlags::GTA_ENVCLOTH_OBJECT_INCLUDE_TYPES & (~ArchetypeFlags::GTA_VEHICLE_TYPE) );
             }

			m_ScriptRopeID = rope->SetNewUniqueId();
        }
    }
}

void CNetworkRopeWorldStateData::RevertWorldState()
{
    NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManagerLog(), "DELETING_ROPE", "");
    LogState(NetworkInterface::GetObjectManagerLog());

    ropeManager *pRopeManager = CPhysics::GetRopeManager();

    if(gnetVerifyf(pRopeManager, "No rope manager!"))
    {
        if(m_ScriptRopeID > 0)
        {
            // ropes are script resources so get cleaned up automatically by the script
            // that created them - we still need to clean the rope up on remote machines though
            if(!CTheScripts::GetScriptHandlerMgr().GetScriptResource(CGameScriptResource::SCRIPT_RESOURCE_ROPE, m_ScriptRopeID))
            {
		        pRopeManager->RemoveRope(m_ScriptRopeID);
            }
	    }
	}
}

void CNetworkRopeWorldStateData::UpdateHostState(CNetworkWorldStateData &hostWorldState)
{
    CNetworkRopeWorldStateData *hostRopeData = SafeCast(CNetworkRopeWorldStateData, &hostWorldState);

    hostRopeData->m_ScriptRopeID = m_ScriptRopeID;
}

void CNetworkRopeWorldStateData::CreateRope(const CGameScriptId  &scriptID,
                                            int                   scriptRopeID,
                                            const RopeParameters &parameters)
{
    CNetworkRopeWorldStateData worldStateData(scriptID, true, -1, scriptRopeID, parameters);
    CNetworkScriptWorldStateMgr::ChangeWorldState(worldStateData, false);
}

void CNetworkRopeWorldStateData::DeleteRope(const CGameScriptId &scriptID,
                                            int                  scriptRopeID)
{
    // look up the rope world state from the rope ID
    CNetworkRopeWorldStateData::Pool *pool = CNetworkRopeWorldStateData::GetPool();

    bool foundRope = false;
    s32  poolIndex = pool->GetSize();

    while(poolIndex-- && !foundRope)
    {
        CNetworkRopeWorldStateData *ropeWorldState = pool->GetSlot(poolIndex);

        if(ropeWorldState && ropeWorldState->m_ScriptRopeID == scriptRopeID && gnetVerifyf(scriptID == ropeWorldState->GetScriptID(), "Unexpected script ID!"))
        {
            CNetworkScriptWorldStateMgr::RevertWorldState(*ropeWorldState, false);
            foundRope = true;
        }
    }
}

int CNetworkRopeWorldStateData::GetNetworkIDFromRopeID(int scriptRopeID)
{
    CNetworkRopeWorldStateData::Pool *pool = CNetworkRopeWorldStateData::GetPool();

    int networkRopeID = -1;

    if(scriptRopeID != -1)
    {
        bool foundRope = false;
        s32  poolIndex = pool->GetSize();

        while(poolIndex-- && !foundRope)
        {
            CNetworkRopeWorldStateData *ropeWorldState = pool->GetSlot(poolIndex);

            if(ropeWorldState && ropeWorldState->m_ScriptRopeID == scriptRopeID)
            {
                networkRopeID = ropeWorldState->m_NetworkRopeID;
                foundRope     = true;
            }
        }
    }

    return networkRopeID;
}

int CNetworkRopeWorldStateData::GetRopeIDFromNetworkID(int networkRopeID)
{
    CNetworkRopeWorldStateData::Pool *pool = CNetworkRopeWorldStateData::GetPool();

    int scriptRopeID = -1;

    if(networkRopeID != -1)
    {
        bool foundRope = false;
        s32  poolIndex = pool->GetSize();

        while(poolIndex-- && !foundRope)
        {
            CNetworkRopeWorldStateData *ropeWorldState = pool->GetSlot(poolIndex);

            if(ropeWorldState && ropeWorldState->m_NetworkRopeID == networkRopeID)
            {
                scriptRopeID = ropeWorldState->m_ScriptRopeID;
                foundRope     = true;
            }
        }
    }

    return scriptRopeID;
}

void CNetworkRopeWorldStateData::LogState(netLoggingInterface &log) const
{
    log.WriteDataValue("Script Name",        "%s", GetScriptID().GetLogName());
    log.WriteDataValue("Script Rope ID",     "%d", m_ScriptRopeID);
    log.WriteDataValue("Network Rope ID",    "%d", m_NetworkRopeID);
    log.WriteDataValue("Position",           "%.2f, %.2f, %.2f", m_RopeParameters.m_Position.x, m_RopeParameters.m_Position.y, m_RopeParameters.m_Position.z);
    log.WriteDataValue("Rotation",           "%.2f, %.2f, %.2f", m_RopeParameters.m_Rotation.x, m_RopeParameters.m_Rotation.y, m_RopeParameters.m_Rotation.z);
    log.WriteDataValue("Max Length",         "%.2f", m_RopeParameters.m_MaxLength);
    log.WriteDataValue("Type",               "%d",   m_RopeParameters.m_Type);
    log.WriteDataValue("Init Length",        "%.2f", m_RopeParameters.m_InitLength);
    log.WriteDataValue("Min Length",         "%.2f", m_RopeParameters.m_MinLength);
    log.WriteDataValue("Length Change Rate", "%.2f", m_RopeParameters.m_LengthChangeRate);
    log.WriteDataValue("PPU Only",           "%s",   m_RopeParameters.m_PpuOnly       ? "TRUE" : "FALSE");
    log.WriteDataValue("Collision On",       "%s",   m_RopeParameters.m_CollisionOn   ? "TRUE" : "FALSE");
    log.WriteDataValue("Lock From Front",    "%s",   m_RopeParameters.m_LockFromFront ? "TRUE" : "FALSE");
    log.WriteDataValue("Time Multiplier",    "%.2f", m_RopeParameters.m_TimeMultiplier);
    log.WriteDataValue("Breakable",          "%s",   m_RopeParameters.m_Breakable     ? "TRUE" : "FALSE");
}

void CNetworkRopeWorldStateData::Serialise(CSyncDataReader &serialiser)
{
    SerialiseWorldState(serialiser);
}

void CNetworkRopeWorldStateData::Serialise(CSyncDataWriter &serialiser)
{
    SerialiseWorldState(serialiser);
}

template <class Serialiser> void CNetworkRopeWorldStateData::SerialiseWorldState(Serialiser &serialiser)
{
    const float    MAX_MAX_LENGTH         = 100.0f;
    const float    MAX_INIT_LENGTH        = 100.0f;
    const float    MAX_MIN_LENGTH         = 100.0f;
    const float    MAX_CHANGE_RATE        = 10.0f;
    const float    MAX_TIME_MULTIPLIER    = 100.0f;
    const unsigned SIZEOF_ROPE_ID         = datBitsNeeded<MAX_NUM_NETWORK_ROPE_IDS>::COUNT + 1;
    const unsigned SIZEOF_MAX_LENGTH      = 16;
    const unsigned SIZEOF_TYPE            = 4;
    const unsigned SIZEOF_INIT_LENGTH     = 16;
    const unsigned SIZEOF_MIN_LENGTH      = 16;
    const unsigned SIZEOF_CHANGE_RATE     = 13;
    const unsigned SIZEOF_TIME_MULTIPLIER = 13;


    GetScriptID().Serialise(serialiser);
    SERIALISE_INTEGER(serialiser, m_NetworkRopeID, SIZEOF_ROPE_ID, "Network Rope ID");
    SERIALISE_POSITION(serialiser, m_RopeParameters.m_Position, "Position");
    SERIALISE_POSITION(serialiser, m_RopeParameters.m_Rotation, "Rotation");
    SERIALISE_PACKED_FLOAT(serialiser, m_RopeParameters.m_MaxLength, MAX_MAX_LENGTH, SIZEOF_MAX_LENGTH, "Max Length");
    SERIALISE_INTEGER(serialiser, m_RopeParameters.m_Type, SIZEOF_TYPE, "Type");
    SERIALISE_PACKED_FLOAT(serialiser, m_RopeParameters.m_InitLength, MAX_INIT_LENGTH, SIZEOF_INIT_LENGTH, "Init Length");
    SERIALISE_PACKED_FLOAT(serialiser, m_RopeParameters.m_MinLength, MAX_MIN_LENGTH, SIZEOF_MIN_LENGTH, "Min Length");
    SERIALISE_PACKED_FLOAT(serialiser, m_RopeParameters.m_LengthChangeRate, MAX_CHANGE_RATE, SIZEOF_CHANGE_RATE, "Length Change Rate");
	SERIALISE_PACKED_FLOAT(serialiser, m_RopeParameters.m_TimeMultiplier, MAX_TIME_MULTIPLIER, SIZEOF_TIME_MULTIPLIER, "Time Multiplier");
    SERIALISE_BOOL(serialiser, m_RopeParameters.m_PpuOnly, "Ppu Only");
    SERIALISE_BOOL(serialiser, m_RopeParameters.m_CollisionOn, "Collision On");
    SERIALISE_BOOL(serialiser, m_RopeParameters.m_LockFromFront, "Lock From Front");    
    SERIALISE_BOOL(serialiser, m_RopeParameters.m_Breakable, "Breakable");
	SERIALISE_BOOL(serialiser, m_RopeParameters.m_Pinned, "Pinned");
}

void CNetworkRopeWorldStateData::PostSerialise()
{
    if(m_NetworkRopeID == -1)
    {
        bool isHostOfScript = CTheScripts::GetScriptHandlerMgr().IsNetworkHost(GetScriptID());

        if(isHostOfScript)
        {
            m_NetworkRopeID = ms_NextNetworkID++;

            if(ms_NextNetworkID >= MAX_NUM_NETWORK_ROPE_IDS)
            {
                ms_NextNetworkID = 0;
            }
        }
    }
}

#if __BANK

static int g_DebugRopeID = -1;

static void NetworkBank_CreateRope()
{
    if(NetworkInterface::IsGameInProgress() && g_DebugRopeID == -1)
    {
        CGameScriptId scriptID("freemode", -1);
        Vector3 pos(-127.62f, -599.96f, 201.75f);
        Vector3 rot(0.0f, 90.0f, 0.0f);
		CNetworkRopeWorldStateData::RopeParameters parameters(pos, rot, 65.6f, 2, -1.0f, 0.5f, 0.5f, true, false, true, 1.0f, false, true );

        ropeManager *pRopeManager = CPhysics::GetRopeManager();

        if(gnetVerifyf(pRopeManager, "No rope manager!"))
        {
            float initLength = Selectf(parameters.m_InitLength, parameters.m_InitLength, parameters.m_MaxLength);

		    ropeInstance *rope = pRopeManager->AddRope(VECTOR3_TO_VEC3V(parameters.m_Position),
                                                       VECTOR3_TO_VEC3V(parameters.m_Rotation),
                                                       initLength,
                                                       parameters.m_MinLength,
                                                       parameters.m_MaxLength,
                                                       parameters.m_LengthChangeRate,
                                                       parameters.m_Type,
                                                       -1,
                                                       parameters.m_PpuOnly,
                                                       parameters.m_LockFromFront,
                                                       parameters.m_TimeMultiplier,
                                                       parameters.m_Breakable,
													   parameters.m_Pinned
													   );

            if(gnetVerifyf(rope, "Failed to create a rope!"))
            {
// TODO: disabled by svetli
// ropes will be marked GTA_ENVCLOTH_OBJECT_TYPE by default
//                 if(parameters.m_CollisionOn)
                 {
 				    rope->SetPhysInstFlags( ArchetypeFlags::GTA_ENVCLOTH_OBJECT_TYPE, ArchetypeFlags::GTA_ENVCLOTH_OBJECT_INCLUDE_TYPES & (~ArchetypeFlags::GTA_VEHICLE_TYPE) );
                 }

			    g_DebugRopeID = rope->SetNewUniqueId();
            }
        }

        if(g_DebugRopeID != -1)
        {
            NetworkInterface::CreateRopeOverNetwork(scriptID,
                                                    g_DebugRopeID,
                                                    parameters.m_Position,
                                                    parameters.m_Rotation,
                                                    parameters.m_MaxLength,
                                                    parameters.m_Type,
                                                    parameters.m_InitLength,
                                                    parameters.m_MinLength,
                                                    parameters.m_LengthChangeRate,
                                                    parameters.m_PpuOnly,
                                                    parameters.m_CollisionOn,
                                                    parameters.m_LockFromFront,
                                                    parameters.m_TimeMultiplier,
                                                    parameters.m_Breakable,
													parameters.m_Pinned
													);
        }
    }
}

static void NetworkBank_DeleteRope()
{
    if(NetworkInterface::IsGameInProgress() && g_DebugRopeID != -1)
    {
        ropeManager *pRopeManager = CPhysics::GetRopeManager();

        if(gnetVerifyf(pRopeManager, "No rope manager!"))
        {
			pRopeManager->RemoveRope(g_DebugRopeID);
        }

        CGameScriptId scriptID("freemode", -1);
        NetworkInterface::DeleteRopeOverNetwork(scriptID, g_DebugRopeID);
    }
}

void CNetworkRopeWorldStateData::AddDebugWidgets()
{
    bkBank *bank = BANKMGR.FindBank("Network");

    if(gnetVerifyf(bank, "Unable to find network bank!"))
    {
        bank->PushGroup("Ropes", false);
        {
            bank->AddButton("Create Test Rope", datCallback(NetworkBank_CreateRope));
            bank->AddButton("Delete Test Rope", datCallback(NetworkBank_DeleteRope));
        }
        bank->PopGroup();
    }
}

void CNetworkRopeWorldStateData::DisplayDebugText()
{
    CNetworkRopeWorldStateData::Pool *pool = CNetworkRopeWorldStateData::GetPool();

	s32 i = pool->GetSize();

	while(i--)
	{
		CNetworkRopeWorldStateData *worldState = pool->GetSlot(i);

		if(worldState)
		{
            grcDebugDraw::AddDebugOutput("Rope created: %s: Pos %.2f, %.2f, %.2f: Rot %.2f, %.2f, %.2f :MaxL %.2f: Type %d: IL %.2f: MinL %.2f: CR %.2f",
                                         worldState->GetScriptID().GetLogName(),
                                         worldState->m_RopeParameters.m_Position.x,
                                         worldState->m_RopeParameters.m_Position.y,
                                         worldState->m_RopeParameters.m_Position.z,
                                         worldState->m_RopeParameters.m_Rotation.x,
                                         worldState->m_RopeParameters.m_Rotation.y,
                                         worldState->m_RopeParameters.m_Rotation.z,
                                         worldState->m_RopeParameters.m_MaxLength,
                                         worldState->m_RopeParameters.m_Type,
                                         worldState->m_RopeParameters.m_InitLength,
                                         worldState->m_RopeParameters.m_MinLength,
                                         worldState->m_RopeParameters.m_LengthChangeRate);
            grcDebugDraw::AddDebugOutput("\t\t\t\t\t\tScript ID: %d, Network ID: %d PPU Only: %s: Collision: %s: Lock FF: %s: Time Mult: %.2f: Breakable: %s",
                                         worldState->m_ScriptRopeID,
                                         worldState->m_NetworkRopeID,
                                         worldState->m_RopeParameters.m_PpuOnly       ? "TRUE" : "FALSE",
                                         worldState->m_RopeParameters.m_CollisionOn   ? "TRUE" : "FALSE",
                                         worldState->m_RopeParameters.m_LockFromFront ? "TRUE" : "FALSE",
                                         worldState->m_RopeParameters.m_TimeMultiplier,
                                         worldState->m_RopeParameters.m_Breakable     ? "TRUE" : "FALSE");
        }
    }
}
#endif // __BANK
