//
// NetworkObjectTypes.h
//
// Copyright (C) 1999-2009 Rockstar Games.  All Rights Reserved.
//

#ifndef NETWORK_SCRIPT_WORLD_STATE_TYPES_H
#define NETWORK_SCRIPT_WORLD_STATE_TYPES_H

enum NetScriptWorldStateTypes
{
    NET_WORLD_STATE_FIRST,
    NET_WORLD_STATE_CAR_GEN = NET_WORLD_STATE_FIRST,
	NET_WORLD_STATE_ENTITY_AREA,
    NET_WORLD_STATE_POP_GROUP_OVERRIDE,
	NET_WORLD_STATE_POP_MULTIPLIER_AREA,
    NET_WORLD_STATE_PTFX,
    NET_WORLD_STATE_ROAD_NODE,
    NET_WORLD_STATE_ROPE,
    NET_WORLD_STATE_SCENARIO_BLOCKING_AREA,
	NET_WORLD_STATE_VEHICLE_PLAYER_LOCKING,
	NET_NUM_WORLD_STATES
};

class CNetworkWorldStateData;

namespace NetworkScriptWorldStateTypes
{
    void Init();
    void Shutdown();

    CNetworkWorldStateData *CreateWorldState(NetScriptWorldStateTypes type);

#if __BANK
    void AddDebugWidgets();
    void DisplayDebugText();
#endif // __BANK
}

#endif // NETWORK_SCRIPT_WORLD_STATE_TYPES_H
