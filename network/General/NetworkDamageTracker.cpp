//
// name:        NetworkDamageTracker.h
// description: Keeps track of damage dealt to a network object by participating players
// written by:  Daniel Yelland
//

// --- Include Files ------------------------------------------------------------
#include "NetworkDamageTracker.h"

#include "bank/bkmgr.h"

#include "grcore/debugdraw.h"
#include "fwnet/netplayer.h"
#include "fwnet/netchannel.h"

#include "debug/DebugScene.h"
#include "modelinfo/PedModelInfo.h"
#include "ModelInfo/ModelInfo.h"
#include "ModelInfo/ModelInfo_Factories.h"
#include "network/objects/entities/NetObjObject.h"
#include "network/objects/entities/NetObjPed.h"
#include "network/objects/entities/NetObjVehicle.h"
#include "peds/pedintelligence.h"
#include "peds/ped.h"
#include "vehicles/vehicle.h"
#include "weapons/info/WeaponInfoManager.h"
#include "peds/pedDefines.h"


NETWORK_OPTIMISATIONS()

RAGE_DEFINE_SUBCHANNEL(net, damage_tracker, DIAG_SEVERITY_DEBUG3)
#undef __net_channel
#define __net_channel net_damage_tracker


FW_INSTANTIATE_CLASS_POOL(CNetworkDamageTracker, MAX_NUM_DAMAGE_TRACKERS, atHashString("CNetworkDamageTracker",0x5f2201f7));

#if __BANK && __DEV
static bool s_displayKillTrackingDebug = false;
static bool s_displayHeadShotTrackingDebug = false;
#endif // __BANK && __DEV

// Name         :   CNetworkDamageTracker::CNetworkDamageTracker
// Purpose      :   Class constructor
// Parameters   :   networkObject: the network object to track damage for
CNetworkDamageTracker::CNetworkDamageTracker(netObject *networkObject) :
m_networkObject(networkObject)
{
    PhysicalPlayerIndex playerIndex = 0;

    for(playerIndex = 0; playerIndex < MAX_NUM_PHYSICAL_PLAYERS; playerIndex++)
    {
        m_damageDealt[playerIndex] = 0.0f;
    }
}

// Name         :   CNetworkDamageTracker::~CNetworkDamageTracker
// Purpose      :   Class destructor
CNetworkDamageTracker::~CNetworkDamageTracker()
{
}

// Name         :   CNetworkDamageTracker::AddDamageDealt
// Purpose      :   Adds damage dealt to the wrapped object by a specified player
// Parameters   :   playerIndex: index of player dealing the damage
//                  damage:      amount of damage dealt
void  CNetworkDamageTracker::AddDamageDealt(const netPlayer& player, float damage)
{
	if (gnetVerify(player.GetPhysicalPlayerIndex() != INVALID_PLAYER_INDEX))
	{
		m_damageDealt[player.GetPhysicalPlayerIndex()] += damage;
	}
}

// Name         :   CNetworkDamageTracker::GetDamageDealt
// Purpose      :   Returns the amount of damage dealt to the associated network object
//                  by the specified player.
// Parameters   :   playerIndex: the index of the player to query
float CNetworkDamageTracker::GetDamageDealt(const netPlayer& player)
{
	if (gnetVerify(player.GetPhysicalPlayerIndex() != INVALID_PLAYER_INDEX))
	{
		float damageDealt = m_damageDealt[player.GetPhysicalPlayerIndex()];
	    return damageDealt;
	}

	return 0.0f;
}

// Name         :   CNetworkDamageTracker::ResetDamageDealt
// Purpose      :   Resets the amount of damage dealt to the associated network object
//                  by the specified player.
// Parameters   :   playerIndex: the index of the player to reset
void CNetworkDamageTracker::ResetDamageDealt(const netPlayer& player)
{
	if (gnetVerify(player.GetPhysicalPlayerIndex() != INVALID_PLAYER_INDEX))
	{
		m_damageDealt[player.GetPhysicalPlayerIndex()] = 0.0f;
	}
}

// Name         :   CNetworkDamageTracker::ResetAll
// Purpose      :   Resets the amount of damage dealt to the associated network object
//                  for all players.
void CNetworkDamageTracker::ResetAll( )
{
	for (int i=0; i<MAX_NUM_PHYSICAL_PLAYERS; i++)
	{
		m_damageDealt[i] = 0.0f;
	}
}

#if __BANK

static void NetworkBank_StartDamageTrackingNearestCar()
{
    CPed *playerPed = FindFollowPed();

    if(playerPed)
    {
        CVehicle *closestVehicle = playerPed->GetPedIntelligence()->GetClosestVehicleInRange();

        if(closestVehicle)
        {
            CNetObjVehicle *netObjVehicle = static_cast<CNetObjVehicle *>(closestVehicle->GetNetworkObject());

            if(netObjVehicle && !netObjVehicle->IsTrackingDamage())
            {
                netObjVehicle->ActivateDamageTracking();
            }
        }
    }
}

static void NetworkBank_StartDamageTrackingNearestObject()
{
    CPed *playerPed = FindFollowPed();

    if(playerPed)
    {
        CObject *closestObject = playerPed->GetPedIntelligence()->GetClosestObjectInRange();

        if(closestObject)
        {
            CNetObjObject *netObjObject = static_cast<CNetObjObject *>(closestObject->GetNetworkObject());

            if(netObjObject && !netObjObject->IsTrackingDamage())
            {
                netObjObject->ActivateDamageTracking();
            }
        }
    }
}

static void NetworkBank_StartDamageTrackingNearestPed()
{
    CPed *playerPed = FindFollowPed();

    if(playerPed)
    {
        CPed *closestPed = playerPed->GetPedIntelligence()->GetClosestPedInRange();

        if(closestPed)
        {
            CNetObjPed *netObjPed = static_cast<CNetObjPed *>(closestPed->GetNetworkObject());

            if(netObjPed && !netObjPed->IsTrackingDamage())
            {
                netObjPed->ActivateDamageTracking();
            }
        }
    }
}

static void NetworkBank_StartHeadShotTracking()
{
    CEntity* pFocusEntity = CDebugScene::FocusEntities_Get(0);
    if(pFocusEntity && pFocusEntity->GetIsTypePed())
    {
        CNetworkHeadShotTracker::StartTracking(pFocusEntity->GetModelIndex());
    }
}

static void NetworkBank_StopHeadShotTracking()
{
    CEntity* pFocusEntity = CDebugScene::FocusEntities_Get(0);
    if(pFocusEntity && pFocusEntity->GetIsTypePed())
    {
        CNetworkHeadShotTracker::StopTracking(pFocusEntity->GetModelIndex());
    }
}

static void NetworkBank_StartKillTracking()
{
    CEntity* pFocusEntity = CDebugScene::FocusEntities_Get(0);
    if(pFocusEntity && (pFocusEntity->GetIsTypePed() || pFocusEntity->GetIsTypeVehicle()))
    {
        CNetworkKillTracker::StartTracking(pFocusEntity->GetModelIndex());
    }
}

static void NetworkBank_StopKillTracking()
{
    CEntity* pFocusEntity = CDebugScene::FocusEntities_Get(0);
    if(pFocusEntity && (pFocusEntity->GetIsTypePed() || pFocusEntity->GetIsTypeVehicle()))
    {
        CNetworkKillTracker::StopTracking(pFocusEntity->GetModelIndex());
    }
}

static void NetworkBank_StartKillTrackingCivilians()
{
    CNetworkKillTracker::StartTrackingCivilianKills();
}

static void NetworkBank_StopKillTrackingCivilians()
{
    CNetworkKillTracker::StopTrackingCivilianKills();
}

static void NetworkBank_ClearDamageEntity()
{
    CEntity *focusEntity = CDebugScene::FocusEntities_Get(0);

    netObject *networkObject = NetworkUtils::GetNetworkObjectFromEntity(focusEntity);

    if(networkObject)
    {
        NetworkInterface::ClearLastDamageObject(networkObject);
    }
}

void CNetworkDamageTracker::AddDebugWidgets()
{
    bkBank *bank = BANKMGR.FindBank("Network");

    if(gnetVerifyf(bank, "Unable to find network bank!"))
    {
        bank->PushGroup("Debug Damage Trackers", false);
#if __DEV
            bank->AddToggle("Display kill tracking debug info",     &s_displayKillTrackingDebug);
			bank->AddToggle("Display head shot tracking debug info",&s_displayHeadShotTrackingDebug);
#endif // __DEV
            bank->AddButton("Start damage tracking nearest vehicle", datCallback(NetworkBank_StartDamageTrackingNearestCar));
            bank->AddButton("Start damage tracking nearest ped",     datCallback(NetworkBank_StartDamageTrackingNearestPed));
            bank->AddButton("Start damage tracking nearest object",  datCallback(NetworkBank_StartDamageTrackingNearestObject));
            bank->AddButton("Start Kill Tracking",                   datCallback(NetworkBank_StartKillTracking));
            bank->AddButton("Stop Kill Tracking",                    datCallback(NetworkBank_StopKillTracking));
            bank->AddButton("Start Kill Tracking Civilians",         datCallback(NetworkBank_StartKillTrackingCivilians));
            bank->AddButton("Stop Kill Tracking Civilians",          datCallback(NetworkBank_StopKillTrackingCivilians));
            bank->AddButton("Clear Damage Entity",                   datCallback(NetworkBank_ClearDamageEntity));
            bank->AddButton("Start HeadShot Tracking",               datCallback(NetworkBank_StartHeadShotTracking));
            bank->AddButton("Stop HeadShot Tracking",                datCallback(NetworkBank_StopHeadShotTracking));
        bank->PopGroup();
    }
}

#endif // __BANK

CNetworkKillTracker CNetworkKillTracker::ms_killTrackers[CNetworkKillTracker::MAX_NUM_KILL_TRACKERS];
CNetworkKillTracker CNetworkKillTracker::ms_killTrackersForRankPoints[CNetworkKillTracker::MAX_NUM_KILL_TRACKERS_FOR_RANK_POINTS];
bool   CNetworkKillTracker::ms_killTrackCivilians = false;
u32 CNetworkKillTracker::ms_numCiviliansKilled = 0;

atFixedArray<u32, NUM_KILL_PED_CLASSIFICATIONS>		CNetworkKillTracker::sm_NumPedsKilledOfThisTypeArray;
atFixedArray <bool, NUM_KILL_PED_CLASSIFICATIONS>	CNetworkKillTracker::sm_IsKillTrackingPedsOfThisTypeArray;


CNetworkKillTracker::CNetworkKillTracker() :
m_modelIndex(fwModelId::MI_INVALID),
m_weaponTypeHash(0),
m_numKills(0)
{
}

CNetworkKillTracker::~CNetworkKillTracker()
{
}

void CNetworkKillTracker::Init()
{
    for(u32 index = 0; index < MAX_NUM_KILL_TRACKERS; index++)
    {
        ms_killTrackers[index].Reset();
    }

    for(u32 index = 0; index < MAX_NUM_KILL_TRACKERS_FOR_RANK_POINTS; index++)
    {
        ms_killTrackersForRankPoints[index].Reset();
    }

	ms_killTrackCivilians = false;
	ms_numCiviliansKilled = 0;

	//Noticed clear reset and resize was done on other atFixedArray's
	sm_NumPedsKilledOfThisTypeArray.clear();
	sm_NumPedsKilledOfThisTypeArray.Reset();
	sm_NumPedsKilledOfThisTypeArray.Resize(NUM_KILL_PED_CLASSIFICATIONS);
	sm_IsKillTrackingPedsOfThisTypeArray.clear();
	sm_IsKillTrackingPedsOfThisTypeArray.Reset();
	sm_IsKillTrackingPedsOfThisTypeArray.Resize(NUM_KILL_PED_CLASSIFICATIONS);
	for (s32 i=0; i<NUM_KILL_PED_CLASSIFICATIONS; i++) 
	{
		sm_NumPedsKilledOfThisTypeArray[i] = 0;
		sm_IsKillTrackingPedsOfThisTypeArray[i]=false;
	}

}

bool CNetworkKillTracker::IsValid()
{
    bool isValid = false;

    if(m_modelIndex != fwModelId::MI_INVALID)
    {
        isValid = true;
    }

    return isValid;
}

bool CNetworkKillTracker::IsTracking(s32 modelIndex, u32 weaponTypeHash)
{
	gnetAssert(modelIndex != fwModelId::MI_INVALID);

    bool isTracking = false;

    if(m_modelIndex == modelIndex && (weaponTypeHash == m_weaponTypeHash || m_weaponTypeHash == 0))
    {
        isTracking = true;
    }

    return isTracking;
}

void CNetworkKillTracker::IncrementNumKills()
{
    m_numKills++;
}

void CNetworkKillTracker::Reset()
{
    m_modelIndex = fwModelId::MI_INVALID;
    m_weaponTypeHash = 0;
    m_numKills   = 0;
}

u32 CNetworkKillTracker::GetNumKills()
{
    return m_numKills;
}

// STATIC FUNCTIONS

void CNetworkKillTracker::RegisterKill(u32 modelIndex, u32 weaponTypeHash, bool awardRankPoints)
{
    gnetAssert(modelIndex != fwModelId::MI_INVALID);

    // register with the kill trackers for one-off missions
    for(u32 index = 0; index < MAX_NUM_KILL_TRACKERS; index++)
    {
        if(ms_killTrackers[index].IsTracking(modelIndex, weaponTypeHash))
        {
            ms_killTrackers[index].IncrementNumKills();
        }
    }

    if(awardRankPoints)
    {
        // register with the kill trackers for giving rank points
        bool foundKillTracker = false;

        for(u32 index = KILL_TRACKER_FOR_MISSION_RANK_POINTS_START; index < MAX_NUM_KILL_TRACKERS_FOR_RANK_POINTS && !foundKillTracker; index++)
        {
            if(ms_killTrackersForRankPoints[index].IsTracking(modelIndex))
            {
                ms_killTrackersForRankPoints[index].IncrementNumKills();
                foundKillTracker = true;
            }
        }

        // the model index of the ped killed has not been registered for tracking for rank points, so assume this is an NPC ped
        if(!foundKillTracker)
        {
            CBaseModelInfo *modelInfo = CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(modelIndex)));

            if(modelInfo && modelInfo->GetModelType() == MI_TYPE_PED)
            {
                ms_killTrackersForRankPoints[KILL_TRACKER_FOR_NPC_RANK_POINTS].IncrementNumKills();
            }
        }

        if(ms_killTrackCivilians)
        {
            CBaseModelInfo *modelInfo = CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(modelIndex)));

            if(modelInfo && modelInfo->GetModelType() == MI_TYPE_PED)
            {
                const CPedModelInfo *pedModelInfo = static_cast<CPedModelInfo*>(modelInfo);

			    if( pedModelInfo->GetDefaultPedType() == PEDTYPE_CIVMALE ||
					pedModelInfo->GetDefaultPedType() == PEDTYPE_CIVFEMALE ||
					pedModelInfo->GetDefaultPedType() == PEDTYPE_ANIMAL)
                {
                    ms_numCiviliansKilled++;
                }
			
            }
        }

		CBaseModelInfo *modelInfo = CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(modelIndex)));

		if(modelInfo && modelInfo->GetModelType() == MI_TYPE_PED)
		{
			const CPedModelInfo *pedModelInfo = static_cast<CPedModelInfo*>(modelInfo);

			for(u32 index = 0; index < PEDTYPE_LAST_PEDTYPE; index++)
			{
				if( sm_IsKillTrackingPedsOfThisTypeArray[index] && ( pedModelInfo->GetDefaultPedType() == ePedType(index)) )
				{
					sm_NumPedsKilledOfThisTypeArray[index]++;
				}
			}

			//Check if requested tracking all peds
			if( sm_IsKillTrackingPedsOfThisTypeArray[ALL_KILL_PED_TYPES] )
			{
				sm_NumPedsKilledOfThisTypeArray[ALL_KILL_PED_TYPES]++;
			}
		}
    }
}

bool CNetworkKillTracker::StartTracking(u32 modelIndex, u32 weaponTypeHash)
{
    gnetAssert(modelIndex != -1);

    for(u32 index = 0; index < MAX_NUM_KILL_TRACKERS; index++)
    {
        if(ms_killTrackers[index].IsTracking(modelIndex, weaponTypeHash))
        {
            AssertMsg(0, "Attempting to start kill tracking with parameters we are already tracking!");
            return false;
        }
    }

    bool addedKillTracker = false;

    for(u32 index = 0; index < MAX_NUM_KILL_TRACKERS && !addedKillTracker; index++)
    {
        if(!ms_killTrackers[index].IsValid())
        {
            ms_killTrackers[index].m_modelIndex = modelIndex;
            ms_killTrackers[index].m_weaponTypeHash = weaponTypeHash;
            ms_killTrackers[index].m_numKills   = 0;
            addedKillTracker = true;

			gnetDebug2("Start kill tracking:");
			gnetDebug2("....... model: %d", ms_killTrackers[index].m_modelIndex.Get());
			gnetDebug2("...... weapon: %d", ms_killTrackers[index].m_weaponTypeHash);
        }
    }

    AssertMsg(addedKillTracker, "Ran out of network kill trackers!");

    return addedKillTracker;
}

bool CNetworkKillTracker::StopTracking(u32 modelIndex, u32 weaponTypeHash)
{
    gnetAssert(modelIndex != -1);

    bool removedKillTracker = false;

    for(u32 index = 0; index < MAX_NUM_KILL_TRACKERS && !removedKillTracker; index++)
    {
        if(ms_killTrackers[index].IsTracking(modelIndex, weaponTypeHash))
        {
			gnetDebug2("Stop kill tracking:");
			gnetDebug2("....... model: %d", ms_killTrackers[index].m_modelIndex.Get());
			gnetDebug2("...... weapon: %d", ms_killTrackers[index].m_weaponTypeHash);

            ms_killTrackers[index].Reset();
            removedKillTracker = true;
        }
    }

    AssertMsg(removedKillTracker, "Kill tracking has not been started on the specified parameters!");

    return removedKillTracker;
}

bool CNetworkKillTracker::IsKillTracking(u32 modelIndex, u32 weaponTypeHash)
{
    bool foundKillTracker = false;

    for(u32 index = 0; index < MAX_NUM_KILL_TRACKERS && !foundKillTracker; index++)
    {
        if(ms_killTrackers[index].IsTracking(modelIndex, weaponTypeHash))
        {
            foundKillTracker = true;
        }
    }

    return foundKillTracker;
}

u32 CNetworkKillTracker::GetTrackingResults(u32 modelIndex, u32 weaponTypeHash)
{
    gnetAssert(modelIndex != -1);

    u32 numKills         = 0;
    bool   foundKillTracker = false;

    for(u32 index = 0; index < MAX_NUM_KILL_TRACKERS && !foundKillTracker; index++)
    {
        if(ms_killTrackers[index].IsTracking(modelIndex, weaponTypeHash))
        {
            numKills = ms_killTrackers[index].GetNumKills();
            foundKillTracker = true;
        }
    }

    AssertMsg(foundKillTracker, "Kill tracking has not been started on the specified parameters!");

    return numKills;
}

void CNetworkKillTracker::ResetTrackingResults(u32 modelIndex, u32 weaponTypeHash)
{
    gnetAssert(modelIndex != -1);

    bool foundKillTracker = false;

    for(u32 index = 0; index < MAX_NUM_KILL_TRACKERS && !foundKillTracker; index++)
    {
        if(ms_killTrackers[index].IsTracking(modelIndex, weaponTypeHash))
        {
            ms_killTrackers[index].m_numKills = 0;
            foundKillTracker = true;
        }
    }

    AssertMsg(foundKillTracker, "Kill tracking has not been started on the specified parameters!");
}

// kill tracking for rank points - persistent kill tracking for entire session
bool CNetworkKillTracker::RegisterModelIndexForRankPoints(u32 modelIndex)
{
    gnetAssert(modelIndex != -1);

    for(u32 index = KILL_TRACKER_FOR_MISSION_RANK_POINTS_START; index < MAX_NUM_KILL_TRACKERS_FOR_RANK_POINTS; index++)
    {
        if(ms_killTrackersForRankPoints[index].IsTracking(modelIndex))
        {
            AssertMsg(0, "Registering a model index for rank points that is already registered!");
            return false;
        }
    }

    bool addedKillTracker = false;

    for(u32 index = KILL_TRACKER_FOR_MISSION_RANK_POINTS_START; index < MAX_NUM_KILL_TRACKERS_FOR_RANK_POINTS && !addedKillTracker; index++)
    {
        if(!ms_killTrackersForRankPoints[index].IsValid())
        {
            ms_killTrackersForRankPoints[index].m_modelIndex = modelIndex;
            ms_killTrackersForRankPoints[index].m_weaponTypeHash = 0;
            ms_killTrackersForRankPoints[index].m_numKills   = 0;
            addedKillTracker = true;
        }
    }

    AssertMsg(addedKillTracker, "Too many models registered for rank points!");

    return addedKillTracker;
}

bool CNetworkKillTracker::IsTrackingForRankPoints(u32 modelIndex)
{
    bool foundKillTracker = false;

    for(u32 index = KILL_TRACKER_FOR_MISSION_RANK_POINTS_START; index < MAX_NUM_KILL_TRACKERS_FOR_RANK_POINTS && !foundKillTracker; index++)
    {
        if(ms_killTrackersForRankPoints[index].IsTracking(modelIndex))
        {
            foundKillTracker = true;
        }
    }

    return foundKillTracker;
}

u32 CNetworkKillTracker::GetNumKillsForRankPoints(u32 modelIndex)
{
    gnetAssert(modelIndex != -1);

    u32 numKills         = 0;
    bool   foundKillTracker = false;

    for(u32 index = KILL_TRACKER_FOR_MISSION_RANK_POINTS_START; index < MAX_NUM_KILL_TRACKERS_FOR_RANK_POINTS && !foundKillTracker; index++)
    {
        if(ms_killTrackersForRankPoints[index].IsTracking(modelIndex))
        {
            numKills = ms_killTrackersForRankPoints[index].GetNumKills();
            foundKillTracker = true;
        }
    }

    // model index was not registered as a mission type, so return the count for NPCs
    if(!foundKillTracker)
    {
        numKills = ms_killTrackersForRankPoints[KILL_TRACKER_FOR_NPC_RANK_POINTS].GetNumKills();
    }

    return numKills;
}

void CNetworkKillTracker::ResetNumKillsForRankPoints(u32 modelIndex)
{
    gnetAssert(modelIndex != -1);

    bool foundKillTracker = false;

    for(u32 index = KILL_TRACKER_FOR_MISSION_RANK_POINTS_START; index < MAX_NUM_KILL_TRACKERS_FOR_RANK_POINTS && !foundKillTracker; index++)
    {
        if(ms_killTrackersForRankPoints[index].IsTracking(modelIndex))
        {
            ms_killTrackersForRankPoints[index].m_numKills = 0;
            foundKillTracker = true;
        }
    }

    // model index was not registered as a mission type, so reset the count for NPCs
    if(!foundKillTracker)
    {
        ms_killTrackersForRankPoints[KILL_TRACKER_FOR_NPC_RANK_POINTS].m_numKills = 0;
    }
}

void CNetworkKillTracker::StartTrackingCivilianKills()
{
    gnetAssertf(!IsTrackingCivilianKills(), "Kill tracking for civilians is already active");
    ms_killTrackCivilians = true;
    ms_numCiviliansKilled = 0;
}

void CNetworkKillTracker::StopTrackingCivilianKills()
{
    gnetAssertf(IsTrackingCivilianKills(), "Kill tracking for civilians is not active");
    ms_killTrackCivilians = false;
    ms_numCiviliansKilled = 0;
}

bool CNetworkKillTracker::IsTrackingCivilianKills()
{
    return ms_killTrackCivilians;
}

u32 CNetworkKillTracker::GetNumCivilianKills()
{
    u32 numKills = 0;

    if(gnetVerifyf(IsTrackingCivilianKills(), "Kill tracking for civilians is not active"))
    {
        numKills = ms_numCiviliansKilled;
    }

    return numKills;
}

void CNetworkKillTracker::ResetNumCivilianKills()
{
    if(gnetVerifyf(IsTrackingCivilianKills(), "Kill tracking for civilians is not active"))
    {
        ms_numCiviliansKilled = 0;
    }
}

//Kill tracking for specified type of ped 
void CNetworkKillTracker::StartTrackingPedKills(s32 pedClassification)
{
	gnetAssert( pedClassification>=0 && pedClassification <NUM_KILL_PED_CLASSIFICATIONS);

	gnetAssertf(!IsTrackingPedKills(pedClassification), "Kill tracking for %s ped is already active", (pedClassification < PEDTYPE_LAST_PEDTYPE) ? CPedType::GetPedTypeNameFromId(ePedType(pedClassification)): "ALL PEDS" );
	sm_IsKillTrackingPedsOfThisTypeArray[pedClassification] = true;
	sm_NumPedsKilledOfThisTypeArray[pedClassification] = 0;
}

void CNetworkKillTracker::StopTrackingPedKills(s32 pedClassification)
{
	gnetAssert( pedClassification>=0 && pedClassification <NUM_KILL_PED_CLASSIFICATIONS);

	gnetAssertf(IsTrackingPedKills(pedClassification), "Kill tracking for %s ped is not active", (pedClassification < PEDTYPE_LAST_PEDTYPE) ? CPedType::GetPedTypeNameFromId(ePedType(pedClassification)): "ALL PEDS" );
	sm_IsKillTrackingPedsOfThisTypeArray[pedClassification] = false;
	sm_NumPedsKilledOfThisTypeArray[pedClassification] = 0;
}

bool CNetworkKillTracker::IsTrackingPedKills(s32 pedClassification)
{
	gnetAssert( pedClassification>=0 && pedClassification <NUM_KILL_PED_CLASSIFICATIONS);
	return sm_IsKillTrackingPedsOfThisTypeArray[pedClassification];
}

u32 CNetworkKillTracker::GetNumPedKills(s32 pedClassification)
{
	gnetAssert( pedClassification>=0 && pedClassification <NUM_KILL_PED_CLASSIFICATIONS);
	u32 numKills = 0;

	if(gnetVerifyf(IsTrackingPedKills(pedClassification), "Kill tracking for %s ped is not active", (pedClassification < PEDTYPE_LAST_PEDTYPE) ? CPedType::GetPedTypeNameFromId(ePedType(pedClassification)): "ALL PEDS" ))
	{
		numKills = sm_NumPedsKilledOfThisTypeArray[pedClassification];
	}

	return numKills;
}

void CNetworkKillTracker::ResetNumPedKills(s32 pedClassification)
{
	gnetAssert( pedClassification>=0 && pedClassification <NUM_KILL_PED_CLASSIFICATIONS);

	if(gnetVerifyf(IsTrackingPedKills(pedClassification), "Kill tracking for %s ped is not active", (pedClassification < PEDTYPE_LAST_PEDTYPE) ? CPedType::GetPedTypeNameFromId(ePedType(pedClassification)): "ALL PEDS" ))
	{
		sm_NumPedsKilledOfThisTypeArray[pedClassification] = 0;
	}
}


#if __BANK && __DEV

void CNetworkKillTracker::DisplayDebugInfo()
{
    if(s_displayKillTrackingDebug)
    {
        for(u32 index = 0; index < MAX_NUM_KILL_TRACKERS; index++)
        {
            if(ms_killTrackers[index].IsValid())
            {
                const CItemInfo *item = CWeaponInfoManager::GetInfo(ms_killTrackers[index].m_weaponTypeHash);
                gnetAssert(item || ms_killTrackers[index].m_weaponTypeHash==0);
                const char *weaponType = item ? item->GetName() : "Any weapon";

                grcDebugDraw::AddDebugOutput("Kill tracking %s : %s (%d)", CModelInfo::GetBaseModelInfoName(fwModelId(ms_killTrackers[index].m_modelIndex)),
                                                                        weaponType,
                                                                        ms_killTrackers[index].m_numKills);
            }
        }

        if(ms_killTrackCivilians)
        {
            grcDebugDraw::AddDebugOutput("Num civilians killed: %d", ms_numCiviliansKilled);
        }

		for(u32 index = 0; index < ALL_KILL_PED_TYPES; index++)
		{
			if(sm_IsKillTrackingPedsOfThisTypeArray[index])
			{
				grcDebugDraw::AddDebugOutput("Num %s peds killed: %d", (index < PEDTYPE_LAST_PEDTYPE) ? CPedType::GetPedTypeNameFromId(ePedType(index)): "ALL PEDS", sm_NumPedsKilledOfThisTypeArray[index]);
			}
		}
    }
}

#endif // __BANK && __DEV

//--------------------------------------------------------------------------------//
//--------------------------------------------------------------------------------//
//--------------------------------------------------------------------------------//
//--------------------------------------------------------------------------------//
//--------------------------------------------------------------------------------//

CNetworkHeadShotTracker CNetworkHeadShotTracker::ms_headShotTrackers[CNetworkHeadShotTracker::MAX_NUM_HEADSHOT_TRACKERS];

CNetworkHeadShotTracker::CNetworkHeadShotTracker() :
m_modelIndex(fwModelId::MI_INVALID),
m_weaponTypeHash(0),
m_numHeadShots(0)
{
}

CNetworkHeadShotTracker::~CNetworkHeadShotTracker()
{
}

void CNetworkHeadShotTracker::Init()
{
    for(u32 index = 0; index < MAX_NUM_HEADSHOT_TRACKERS; index++)
    {
        ms_headShotTrackers[index].Reset();
    }
}

bool CNetworkHeadShotTracker::IsValid()
{
    bool isValid = false;

    if(m_modelIndex != fwModelId::MI_INVALID)
    {
        isValid = true;
    }

    return isValid;
}

bool CNetworkHeadShotTracker::IsTracking(s32 modelIndex, u32 weaponTypeHash)
{
	gnetAssert(modelIndex != fwModelId::MI_INVALID);

    bool isTracking = false;

    if(m_modelIndex.Get() == modelIndex && (weaponTypeHash == m_weaponTypeHash || m_weaponTypeHash == 0))
    {
        isTracking = true;
    }

    return isTracking;
}

void CNetworkHeadShotTracker::IncrementNumHeadShots()
{
    m_numHeadShots++;
}

void CNetworkHeadShotTracker::Reset()
{
    m_modelIndex = fwModelId::MI_INVALID;
    m_weaponTypeHash = 0;
    m_numHeadShots   = 0;
}

u32 CNetworkHeadShotTracker::GetNumHeadShots()
{
    return m_numHeadShots;
}

// STATIC FUNCTIONS

void CNetworkHeadShotTracker::RegisterHeadShot(u32 modelIndex, u32 weaponTypeHash )
{
    gnetAssert(modelIndex != fwModelId::MI_INVALID);

    // register with the HeadShot trackers for one-off missions
    for(u32 index = 0; index < MAX_NUM_HEADSHOT_TRACKERS; index++)
    {
        if(ms_headShotTrackers[index].IsTracking(modelIndex, weaponTypeHash))
        {
            ms_headShotTrackers[index].IncrementNumHeadShots();
        }
    }
}

bool CNetworkHeadShotTracker::StartTracking(s32 modelIndex, u32 weaponTypeHash)
{
    gnetAssert(modelIndex != -1);

    for(u32 index = 0; index < MAX_NUM_HEADSHOT_TRACKERS; index++)
    {
        if(ms_headShotTrackers[index].IsTracking(modelIndex, weaponTypeHash))
        {
            AssertMsg(0, "Attempting to start HeadShot tracking with parameters we are already tracking!");
            return false;
        }
    }

    bool addedHeadShotTracker = false;

    for(u32 index = 0; index < MAX_NUM_HEADSHOT_TRACKERS && !addedHeadShotTracker; index++)
    {
        if(!ms_headShotTrackers[index].IsValid())
        {
            ms_headShotTrackers[index].m_modelIndex = modelIndex;
            ms_headShotTrackers[index].m_weaponTypeHash = weaponTypeHash;
            ms_headShotTrackers[index].m_numHeadShots   = 0;
            addedHeadShotTracker = true;
        }
    }

    AssertMsg(addedHeadShotTracker, "Ran out of network HeadShot trackers!");

    return addedHeadShotTracker;
}

bool CNetworkHeadShotTracker::StopTracking(s32 modelIndex, u32 weaponTypeHash)
{
    gnetAssert(modelIndex != -1);

    bool removedHeadShotTracker = false;

    for(u32 index = 0; index < MAX_NUM_HEADSHOT_TRACKERS && !removedHeadShotTracker; index++)
    {
        if(ms_headShotTrackers[index].IsTracking(modelIndex, weaponTypeHash))
        {
            ms_headShotTrackers[index].Reset();
            removedHeadShotTracker = true;
        }
    }

    AssertMsg(removedHeadShotTracker, "HeadShot tracking has not been started on the specified parameters!");

    return removedHeadShotTracker;
}

bool CNetworkHeadShotTracker::IsHeadShotTracking(s32 modelIndex, u32 weaponTypeHash)
{
    bool foundHeadShotTracker = false;

    for(u32 index = 0; index < MAX_NUM_HEADSHOT_TRACKERS && !foundHeadShotTracker; index++)
    {
        if(ms_headShotTrackers[index].IsTracking(modelIndex, weaponTypeHash))
        {
            foundHeadShotTracker = true;
        }
    }

    return foundHeadShotTracker;
}

u32 CNetworkHeadShotTracker::GetTrackingResults(s32 modelIndex, u32 weaponTypeHash)
{
    gnetAssert(modelIndex != -1);

    u32 numHeadShots         = 0;
    bool   foundHeadShotTracker = false;

    for(u32 index = 0; index < MAX_NUM_HEADSHOT_TRACKERS && !foundHeadShotTracker; index++)
    {
        if(ms_headShotTrackers[index].IsTracking(modelIndex, weaponTypeHash))
        {
            numHeadShots = ms_headShotTrackers[index].GetNumHeadShots();
            foundHeadShotTracker = true;
        }
    }

    AssertMsg(foundHeadShotTracker, "HeadShot tracking has not been started on the specified parameters!");

    return numHeadShots;
}

void CNetworkHeadShotTracker::ResetTrackingResults(s32 modelIndex, u32 weaponTypeHash)
{
    gnetAssert(modelIndex != -1);

    bool foundHeadShotTracker = false;

    for(u32 index = 0; index < MAX_NUM_HEADSHOT_TRACKERS && !foundHeadShotTracker; index++)
    {
        if(ms_headShotTrackers[index].IsTracking(modelIndex, weaponTypeHash))
        {
            ms_headShotTrackers[index].m_numHeadShots = 0;
            foundHeadShotTracker = true;
        }
    }

    AssertMsg(foundHeadShotTracker, "HeadShot tracking has not been started on the specified parameters!");
}

#if __BANK && __DEV

void CNetworkHeadShotTracker::DisplayDebugInfo()
{
    if(s_displayHeadShotTrackingDebug)
    {
        for(u32 index = 0; index < MAX_NUM_HEADSHOT_TRACKERS; index++)
        {
            if(ms_headShotTrackers[index].IsValid())
            {
                const CItemInfo *item = CWeaponInfoManager::GetInfo(ms_headShotTrackers[index].m_weaponTypeHash);
                gnetAssert(item || ms_headShotTrackers[index].m_weaponTypeHash==0);
                const char *weaponType = item ? item->GetName() : "Any weapon";

                grcDebugDraw::AddDebugOutput("HeadShot tracking %s : %s (%d)", CModelInfo::GetBaseModelInfoName(fwModelId(ms_headShotTrackers[index].m_modelIndex)),
                                                                        weaponType,
                                                                        ms_headShotTrackers[index].m_numHeadShots);


            }
        }
	}
}

#endif // __BANK && __DEV


//Used by the scripts to determine which body parts of the player were damaged.
u32  CNetworkBodyDamageTracker::m_DamageComponents     = 0;
u32  CNetworkBodyDamageTracker::m_NumComponentsHit     = 0;
u32  CNetworkBodyDamageTracker::m_HistoryIndex         = 0;
CNetworkBodyDamageTracker::ShotHistory CNetworkBodyDamageTracker::m_ShotsHistory[CNetworkBodyDamageTracker::NUM_SHOTS_TRACKED];

CompileTimeAssert(CNetworkBodyDamageTracker::MAX_NUM_COMPONENTS < 32);

CNetworkBodyDamageTracker::CNetworkBodyDamageTracker()
{
	Reset();
}

CNetworkBodyDamageTracker::~CNetworkBodyDamageTracker()
{
}

void  CNetworkBodyDamageTracker::Reset()
{
	m_DamageComponents =  0;
	m_NumComponentsHit =  0;
	m_HistoryIndex     =  0;

	for (int i=0; i<NUM_SHOTS_TRACKED; i++)
	{
		m_ShotsHistory[i].Reset();
	}
}

void  CNetworkBodyDamageTracker::SetComponent(const CNetObjPhysical::NetworkDamageInfo& damageInfo)
{
	const int component = (damageInfo.m_WeaponComponent == RAGDOLL_INVALID && damageInfo.m_KillsVictim) ? COMPONENT_NOT_SET : damageInfo.m_WeaponComponent;

	gnetAssertf(component >= RAGDOLL_INVALID && component < CNetworkBodyDamageTracker::MAX_NUM_COMPONENTS, "Invalid RAGDOLL component=\"%d\"", component);
	if (component > RAGDOLL_INVALID && component < CNetworkBodyDamageTracker::MAX_NUM_COMPONENTS)
	{
		if (!GetComponentHit(component))
		{
			m_DamageComponents |= (1<<component);
			m_NumComponentsHit++;
		}

		m_ShotsHistory[m_HistoryIndex].m_ComponentHit = component;
		m_ShotsHistory[m_HistoryIndex].m_WasFatal     = damageInfo.m_KillsVictim;

		if (damageInfo.m_Inflictor)
		{
			CNetObjEntity* netInflictor = static_cast<CNetObjEntity *>(NetworkUtils::GetNetworkObjectFromEntity(damageInfo.m_Inflictor));
			if (netInflictor)
			{
				bool inflictorIsPlayer = (netInflictor->GetObjectType() == NET_OBJ_TYPE_PLAYER);

				if (!inflictorIsPlayer)
				{
					if(damageInfo.m_Inflictor->GetIsTypeVehicle())
					{
						CVehicle* vehicle = static_cast<CVehicle *>(damageInfo.m_Inflictor);
						CPed* driver      = vehicle->GetDriver();
						inflictorIsPlayer = (driver && driver->IsPlayer());
					}
				}

				if (inflictorIsPlayer && netInflictor->GetPlayerOwner())
				{
					m_ShotsHistory[m_HistoryIndex].m_Player = netInflictor->GetPlayerOwner()->GetGamerInfo().GetGamerHandle();

					if (!netInflictor->GetPlayerOwner()->GetGamerInfo().GetGamerHandle().IsValid())
					{
						OUTPUT_ONLY( netInflictor->GetPlayerOwner()->GetGamerInfo().GetGamerHandle().OutputValidity(); )
					}
				}
			}
		}

		m_HistoryIndex = (m_HistoryIndex + 1) % NUM_SHOTS_TRACKED;
	}
}

void  CNetworkBodyDamageTracker::UnSetComponent(const int component)
{
	if (component > RAGDOLL_INVALID && gnetVerify(component < CNetworkBodyDamageTracker::MAX_NUM_COMPONENTS) && GetComponentHit(component))
	{
		m_DamageComponents &= (~(1<<component));
		m_NumComponentsHit--;
	}
}

bool CNetworkBodyDamageTracker::GetComponentHit(const int component)
{
	if (component > RAGDOLL_INVALID && component < CNetworkBodyDamageTracker::MAX_NUM_COMPONENTS)
	{
		return (m_DamageComponents & (1<<component)) != 0;
	}

	return false;
}

u32 CNetworkBodyDamageTracker::GetNumComponents()
{
	return m_NumComponentsHit;
}

int CNetworkBodyDamageTracker::GetFatalComponent()
{
	for(int i=0; i<NUM_SHOTS_TRACKED; i++)
	{
		if (m_ShotsHistory[i].IsValid() && m_ShotsHistory[i].m_WasFatal)
		{
			return m_ShotsHistory[i].m_ComponentHit;
		}
	}

	return RAGDOLL_INVALID;
}

void  CNetworkBodyDamageTracker::GetFatalShotHistory(CNetworkBodyDamageTracker::ShotInfo& info)
{
	info.Reset();

	for(int i=0; i<NUM_SHOTS_TRACKED; i++)
	{
		if (m_ShotsHistory[i].IsValid() && m_ShotsHistory[i].m_WasFatal)
		{
			info.m_Player         = m_ShotsHistory[i].m_Player;
			info.m_Fatalcomponent = m_ShotsHistory[i].m_ComponentHit;
		}
	}

	if (info.m_Player.IsValid())
	{
		for(int i=0; i<NUM_SHOTS_TRACKED; i++)
		{
			if (m_ShotsHistory[i].IsValid() && m_ShotsHistory[i].m_Player == info.m_Player)
			{
				info.m_DamageComponents |= (1<<m_ShotsHistory[i].m_ComponentHit);
			}
		}
	}
}

bool CNetworkBodyDamageTracker::ShotInfo::GetComponentHit(const int component)
{
	if (component > RAGDOLL_INVALID && component < CNetworkBodyDamageTracker::MAX_NUM_COMPONENTS)
	{
		return (m_DamageComponents & (1<<component)) != 0;
	}

	return false;
}

//EOF
