//
// name:        NetworkScriptWorldStateTypes.cpp
// description: Different NetworkEvent types
// written by:  Daniel Yelland
//
#include "network/general/scriptworldstate/NetworkScriptWorldStateTypes.h"

#include "bank/bkmgr.h"
#include "bank/bank.h"

#include "network/general/scriptworldstate/worldstates/NetworkCarGenWorldState.h"
#include "network/general/scriptworldstate/worldstates/NetworkEntityAreaWorldState.h"
#include "network/general/scriptworldstate/worldstates/NetworkRoadNodeWorldState.h"
#include "network/general/scriptworldstate/worldstates/NetworkPopGroupOverrideWorldState.h"
#include "network/general/scriptworldstate/worldstates/NetworkPopMultiplierAreaWorldState.h"
#include "network/general/scriptworldstate/worldstates/NetworkPtFXWorldState.h"
#include "network/general/scriptworldstate/worldstates/NetworkRopeWorldState.h"
#include "network/general/scriptworldstate/worldstates/NetworkScenarioBlockingAreaWorldState.h"
#include "network/general/scriptworldstate/worldstates/NetworkVehiclePlayerLockingWorldState.h"

NETWORK_OPTIMISATIONS()

namespace NetworkScriptWorldStateTypes
{
    typedef CNetworkWorldStateData *(*fnCallback)(void);

    static fnCallback s_WorldStateCreators[NET_NUM_WORLD_STATES];

#if __BANK
    static bool s_displayScriptWorldStateDebug = false;
#endif // __BANK

    template<class T> static bool CheckSpaceInPool()
    {
        bool spaceAvailable = true;

        s32 numFreeSpaces = T::GetPool()->GetNoOfFreeSpaces();

        if(numFreeSpaces == 0)
        {
            T worldState;
            gnetAssertf(0, "%s World state pool full!", worldState.GetName());
            spaceAvailable = false;
        }

        return spaceAvailable;
    }

    CNetworkWorldStateData *CreateCarGenWorldState()
    {
        if(CheckSpaceInPool<CNetworkCarGenWorldStateData>())
        {
            return rage_new CNetworkCarGenWorldStateData();
        }

        return 0;
    }

    CNetworkWorldStateData *CreateEntityAreaWorldState()
	{
		if(CheckSpaceInPool<CNetworkEntityAreaWorldStateData>())
		{
			return rage_new CNetworkEntityAreaWorldStateData();
		}

		return 0;
	}

    CNetworkWorldStateData *CreatePopGroupOverrideWorldState()
    {
        if(CheckSpaceInPool<CNetworkPopGroupOverrideWorldStateData>())
        {
            return rage_new CNetworkPopGroupOverrideWorldStateData();
        }

        return 0;
    }

    CNetworkWorldStateData *CreatePopMultiplierAreaWorldState()
    {
        if(CheckSpaceInPool<CNetworkPopMultiplierAreaWorldStateData>())
        {
            return rage_new CNetworkPopMultiplierAreaWorldStateData();
        }

        return 0;
    }

    CNetworkWorldStateData *CreateVehicleLockingForPlayersWorldState()
    {
        if(CheckSpaceInPool<CNetworkVehiclePlayerLockingWorldState>())
        {
            return rage_new CNetworkVehiclePlayerLockingWorldState();
        }

        return 0;
    }

    CNetworkWorldStateData *CreatePtFXWorldState()
    {
        if(CheckSpaceInPool<CNetworkPtFXWorldStateData>())
        {
            return rage_new CNetworkPtFXWorldStateData();
        }

        return 0;
    }

    CNetworkWorldStateData *CreateRoadNodeWorldState()
    {
        if(CheckSpaceInPool<CNetworkRoadNodeWorldStateData>())
        {
            return rage_new CNetworkRoadNodeWorldStateData();
        }

        return 0;
    }

    CNetworkWorldStateData *CreateRopeWorldState()
    {
        if(CheckSpaceInPool<CNetworkRopeWorldStateData>())
        {
            return rage_new CNetworkRopeWorldStateData();
        }

        return 0;
    }

    CNetworkWorldStateData *CreateScenarioBlockingAreaWorldState()
	{
		if(CheckSpaceInPool<CNetworkScenarioBlockingAreaWorldStateData>())
		{
			return rage_new CNetworkScenarioBlockingAreaWorldStateData();
		}

		return 0;
	}

    void Init()
    {
        CNetworkCarGenWorldStateData::InitPool();
        CNetworkEntityAreaWorldStateData::InitPool();
        CNetworkPopGroupOverrideWorldStateData::InitPool();
		CNetworkPopMultiplierAreaWorldStateData::InitPool();
        CNetworkPtFXWorldStateData::InitPool();
        CNetworkRoadNodeWorldStateData::InitPool();
        CNetworkRopeWorldStateData::InitPool();
        CNetworkScenarioBlockingAreaWorldStateData::InitPool();
		CNetworkVehiclePlayerLockingWorldState::InitPool();

        s_WorldStateCreators[NET_WORLD_STATE_CAR_GEN]				 = CreateCarGenWorldState;
 		s_WorldStateCreators[NET_WORLD_STATE_ENTITY_AREA]			 = CreateEntityAreaWorldState;
        s_WorldStateCreators[NET_WORLD_STATE_POP_GROUP_OVERRIDE]	 = CreatePopGroupOverrideWorldState;
        s_WorldStateCreators[NET_WORLD_STATE_POP_MULTIPLIER_AREA]	 = CreatePopMultiplierAreaWorldState;
        s_WorldStateCreators[NET_WORLD_STATE_PTFX]	                 = CreatePtFXWorldState;
        s_WorldStateCreators[NET_WORLD_STATE_ROAD_NODE]				 = CreateRoadNodeWorldState;
        s_WorldStateCreators[NET_WORLD_STATE_ROPE]	                 = CreateRopeWorldState;
		s_WorldStateCreators[NET_WORLD_STATE_SCENARIO_BLOCKING_AREA] = CreateScenarioBlockingAreaWorldState;
		s_WorldStateCreators[NET_WORLD_STATE_VEHICLE_PLAYER_LOCKING] = CreateVehicleLockingForPlayersWorldState;
	}

    void Shutdown()
    {
        CNetworkCarGenWorldStateData::ShutdownPool();
        CNetworkEntityAreaWorldStateData::ShutdownPool();
        CNetworkPopGroupOverrideWorldStateData::ShutdownPool();
		CNetworkPopMultiplierAreaWorldStateData::ShutdownPool();
        CNetworkPtFXWorldStateData::ShutdownPool();
        CNetworkRoadNodeWorldStateData::ShutdownPool();
        CNetworkRopeWorldStateData::ShutdownPool();
        CNetworkScenarioBlockingAreaWorldStateData::ShutdownPool();
		CNetworkVehiclePlayerLockingWorldState::ShutdownPool();
	}

    CNetworkWorldStateData *CreateWorldState(NetScriptWorldStateTypes type)
    {
        CNetworkWorldStateData *worldState = 0;

        if(gnetVerifyf(type >= NET_WORLD_STATE_FIRST && type < NET_NUM_WORLD_STATES,
                       "Trying to create a world state with an invalid type"))
        {
            worldState = s_WorldStateCreators[type]();
        }

        return worldState;
    }

#if __BANK
    void AddDebugWidgets()
    {
        bkBank *bank = BANKMGR.FindBank("Network");

        if(gnetVerifyf(bank, "Unable to find network bank!"))
        {
            bank->AddToggle("Display script world state info", &s_displayScriptWorldStateDebug);

            CNetworkCarGenWorldStateData::AddDebugWidgets();
            CNetworkEntityAreaWorldStateData::AddDebugWidgets();
            CNetworkPopGroupOverrideWorldStateData::AddDebugWidgets();
            CNetworkPopMultiplierAreaWorldStateData::AddDebugWidgets();
            CNetworkPtFXWorldStateData::AddDebugWidgets();
            CNetworkRoadNodeWorldStateData::AddDebugWidgets();
            CNetworkRopeWorldStateData::AddDebugWidgets();
            CNetworkScenarioBlockingAreaWorldStateData::AddDebugWidgets();
			CNetworkVehiclePlayerLockingWorldState::AddDebugWidgets();
		}
    }

    void DisplayDebugText()
    {
        if(s_displayScriptWorldStateDebug)
        {
            CNetworkCarGenWorldStateData::DisplayDebugText();
            CNetworkEntityAreaWorldStateData::DisplayDebugText();
            CNetworkPopGroupOverrideWorldStateData::DisplayDebugText();
			CNetworkPopMultiplierAreaWorldStateData::DisplayDebugText();
            CNetworkPtFXWorldStateData::DisplayDebugText();
            CNetworkRoadNodeWorldStateData::DisplayDebugText();
            CNetworkRopeWorldStateData::DisplayDebugText();
            CNetworkScenarioBlockingAreaWorldStateData::DisplayDebugText();
			CNetworkVehiclePlayerLockingWorldState::DisplayDebugText();
		}
    }
#endif // __BANK
}
