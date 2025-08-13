//
// name:        NetworkWorldGridOwnership.h
// description: Splits the world into a grid and allows players to take control of squares in that grid. Players in control
//              of a grid square can be responsible for managing any resource conflicts between players in the network game
// written by:  Daniel Yelland
//

#ifndef NETWORK_WORLD_GRID_OWNERSHIP_H
#define NETWORK_WORLD_GRID_OWNERSHIP_H

#include "vector/vector3.h"
#include "fwnet/netserialisers.h"
#include "fwnet/nettypes.h"

namespace rage
{
    class netPlayer;
}

class CWorldGridOwnerArrayHandler;

//PURPOSE
// Describes one grid square of the map that can be owned by a network player
struct NetworkGridSquareInfo
{
    //PURPOSE
    // Resets the grid square data to a default state
    void Reset()
    {
        m_GridX = 0;
        m_GridY = 0;
        m_Owner = INVALID_PLAYER_INDEX;
    }

    //PURPOSE
    // Marks the specified grid square as dirty, so it will be synced by the
    // network code to other machines
    static void SetDirty(unsigned elementIndex);

    //PURPOSE
    // Serialiser for the data associated with this grid square.
    //PARAMS
    // serialiser - The serialiser to use with the data
    void Serialise(CSyncDataBase& serialiser)
    {
        SERIALISE_UNSIGNED(serialiser, m_GridX, SIZEOF_GRID_X, "Grid X");
        SERIALISE_UNSIGNED(serialiser, m_GridY, SIZEOF_GRID_Y, "Grid Y");
        SERIALISE_UNSIGNED(serialiser, m_Owner, SIZEOF_OWNER,  "Owner");
    }

    static const unsigned SIZEOF_GRID_X = 8;
    static const unsigned SIZEOF_GRID_Y = 8;
    static const unsigned SIZEOF_OWNER  = 5;

    u8                  m_GridX; // The X position of the square on the grid
    u8                  m_GridY; // The Y position of the square on the grid
    PhysicalPlayerIndex m_Owner; // The current player owner of the grid square (INVALID_PLAYER_INDEX if currently not owned)
};

//PURPOSE
// Manager class for determining ownership of grid squares in the network game. This class works by
// dividing the world into a grid and assigning ownership of the grid squares nearby to the players in
// the session. The owner of a grid square can take responsibility for working with resources that players
// may have conflict over (e.g. spawning cars on car generators) to resolve these conflicts
class CNetworkWorldGridManager
{
public:

    friend class CWorldGridOwnerArrayHandler;

    //PURPOSE
    // Initialises the manager at the start of the network game
    static void Init();

    //PURPOSE
    // Shuts down the manager at the end of the network game
    static void Shutdown();

    //PURPOSE
    // Updates the manager. This should be called once per frame when the network game is running
    static void Update();

    //PURPOSE
    // Called when a new player joins the network session
    //PARAMS
    // player - The player joining the session
    static void PlayerHasJoined(const netPlayer& player);

    //PURPOSE
    // Called when a player leaves the network session
    //PARAMS
    // player - The player leaving the session
    static void PlayerHasLeft(const netPlayer& player);

    //PURPOSE
    // Returns whether the specified position is within a locally
    // controlled grid square
    //PARAMS
    // position - The position to check
    static bool IsPosLocallyControlled(const Vector3 &position);

#if __BANK
    //PURPOSE
    // Adds debug widgets related to this system
    static void AddDebugWidgets();

#if __DEV
    //PURPOSE
    // Displays the active grid squares on the vector map
    static void DisplayOnVectorMap();
#endif // __DEV
#endif // __BANK

private:

    //PURPOSE
    // Class constructor
    CNetworkWorldGridManager();

    //PURPOSE
    // Class destructor
    ~CNetworkWorldGridManager();

    static const float    PLAYER_ACTIVE_GRID_RADIUS;
    static const float    GRID_SQUARE_SIZE;
    static const unsigned MAX_GRID_SQUARES_PER_PLAYER = 36; // 5X5 square centred around a player's position, plus extra space around boundary to prevent thrashing
    static const unsigned MAX_GRID_SQUARES            = MAX_NUM_PHYSICAL_PLAYERS * MAX_GRID_SQUARES_PER_PLAYER;
    static const unsigned INVALID_GRID_SQUARE_INDEX   = MAX_GRID_SQUARES;

    // Data about grid squares that are owned by the different players in the session. Each network player is allocated
    // a section of this array to describe the grid squares they are currently in control of. This avoids the need to
    // use memory for grid squares for map sections that are far away from the current players in the session
    static NetworkGridSquareInfo  m_GridSquareInfos[MAX_GRID_SQUARES];
    static PhysicalPlayerIndex    m_PlayerToUpdateForFrame;            // Players grid square allocation is calculated round-robin over frames
};

#endif  // NETWORK_WORLD_GRID_OWNERSHIP_H
