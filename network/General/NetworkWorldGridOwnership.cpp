//
// filename:    NetworkWorldGridOwnership.cpp
// description: Splits the world into a grid and allows players to take control of squares in that grid. Players in control
//              of a grid square can be responsible for managing any resource conflicts between players in the network game
// written by:  Daniel Yelland
//

// --- Include Files ------------------------------------------------------------
#include "NetworkWorldGridOwnership.h"

#include "fwnet/netchannel.h"
#include "fwscene/world/WorldLimits.h"

#include "Debug/VectorMap.h"
#include "Network/NetworkInterface.h"
#include "Network/Arrays/NetworkArrayMgr.h"
#include "Network/Objects/Entities/NetObjPlayer.h"
#include "Network/Players/NetGamePlayer.h"
#include "Peds/Ped.h"

NETWORK_OPTIMISATIONS()

const float CNetworkWorldGridManager::PLAYER_ACTIVE_GRID_RADIUS = 149.0f; // slightly less than 2 grid squares, to avoid a players scope bringing in an extra square
const float CNetworkWorldGridManager::GRID_SQUARE_SIZE          = 75.0f;

NetworkGridSquareInfo CNetworkWorldGridManager::m_GridSquareInfos[MAX_GRID_SQUARES];
PhysicalPlayerIndex   CNetworkWorldGridManager::m_PlayerToUpdateForFrame = 0;

#if __BANK
#if __DEV
static bool gbDisplayOnVectorMap = false;
#endif // __DEV
#endif // __BANK

void NetworkGridSquareInfo::SetDirty(unsigned elementIndex)
{
    CWorldGridOwnerArrayHandler *worldGridHandler = NetworkInterface::GetArrayManager().GetWorldGridOwnerArrayHandler();

    if(worldGridHandler)
    {
        worldGridHandler->SetElementDirty(elementIndex);
    }
}

CNetworkWorldGridManager::CNetworkWorldGridManager()
{
}

CNetworkWorldGridManager::~CNetworkWorldGridManager()
{
}

void CNetworkWorldGridManager::Init()
{
    for(unsigned index = 0; index < MAX_GRID_SQUARES; index++)
    {
        m_GridSquareInfos[index].Reset();
    }

    m_PlayerToUpdateForFrame = 0;
}

void CNetworkWorldGridManager::Shutdown()
{
    for(unsigned index = 0; index < MAX_GRID_SQUARES; index++)
    {
        m_GridSquareInfos[index].Reset();
    }
}

void CNetworkWorldGridManager::Update()
{
    if(NetworkInterface::IsGameInProgress())
    {
        // the host updates the grid info for all players
		bool bIsHost = false;

		CWorldGridOwnerArrayHandler* pWorldGridArray = NetworkInterface::GetArrayManager().GetWorldGridOwnerArrayHandler();

		if (AssertVerify(pWorldGridArray))
		{
			bIsHost = pWorldGridArray->IsArrayLocallyArbitrated();
		}
		
        if (bIsHost)
        {
            CNetGamePlayer *player = NetworkInterface::GetPhysicalPlayerFromIndex(m_PlayerToUpdateForFrame);
            m_PlayerToUpdateForFrame++;
            m_PlayerToUpdateForFrame%=MAX_NUM_PHYSICAL_PLAYERS;

            if(player)
            {
                PhysicalPlayerIndex playerIndex = player->GetPhysicalPlayerIndex();

                if(gnetVerifyf(playerIndex != INVALID_PLAYER_INDEX, "Processing a physical player with an invalid physical player index!"))
                {
                    CPed *playerPed = player->GetPlayerPed();

                    if(playerPed)
                    {
                        unsigned gridArraySectionStart = playerIndex * MAX_GRID_SQUARES_PER_PLAYER;
                        unsigned gridArraySectionEnd   = gridArraySectionStart + MAX_GRID_SQUARES_PER_PLAYER;

                        if(gnetVerifyf(gridArraySectionStart <  MAX_GRID_SQUARES, "Invalid grid section calculated!") &&
                           gnetVerifyf(gridArraySectionEnd   <= MAX_GRID_SQUARES, "Invalid grid section calculated!"))
                        {
                            CNetObjPlayer *netObjPlayer = static_cast<CNetObjPlayer *>(playerPed->GetNetworkObject());

                            if(netObjPlayer && netObjPlayer->IsInTutorialSession())
                            {
                                // players in tutorial sessions (including the host) are not allowed to control grid squares
                                for(unsigned index = gridArraySectionStart; index < gridArraySectionEnd; index++)
                                {
                                    NetworkGridSquareInfo &squareInfo = m_GridSquareInfos[index];

                                    if(squareInfo.m_Owner != INVALID_PLAYER_INDEX)
                                    {
                                        gnetAssertf(squareInfo.m_Owner == playerIndex, "Grid square owned by unexpected player!");

                                        // this player no longer owns this grid square
                                        squareInfo.Reset();
                                        NetworkGridSquareInfo::SetDirty(index);
                                    }
                                }
                            }
                            else
                            {
                                // calculate the grid around the player focus position
                                Vector3 playerPos = NetworkInterface::GetPlayerFocusPosition(*player);

                                // at the time of programming the world representation limits are a 16km square and the
                                // grid square size we are using is 100m, giving a grid of 160x160 squares. As the
                                // player radius is 200m, each player's area of influence over the grid is a 9x9 areas in grid squares
                                float fStartX = MAX((playerPos.x - PLAYER_ACTIVE_GRID_RADIUS) - WORLDLIMITS_REP_XMIN, 0.0f);
                                float fStartY = MAX((playerPos.y - PLAYER_ACTIVE_GRID_RADIUS) - WORLDLIMITS_REP_YMIN, 0.0f);
                                float fEndX   = MAX((playerPos.x + PLAYER_ACTIVE_GRID_RADIUS) - WORLDLIMITS_REP_XMIN, 0.0f);
                                float fEndY   = MAX((playerPos.y + PLAYER_ACTIVE_GRID_RADIUS) - WORLDLIMITS_REP_YMIN, 0.0f);

                                u8 gridStartX = static_cast<u8>(fStartX / GRID_SQUARE_SIZE);
                                u8 gridStartY = static_cast<u8>(fStartY / GRID_SQUARE_SIZE);
                                u8 gridEndX   = static_cast<u8>(fEndX   / GRID_SQUARE_SIZE);
                                u8 gridEndY   = static_cast<u8>(fEndY   / GRID_SQUARE_SIZE);

								if(gnetVerifyf(gridStartX <= gridEndX, "Invalid grid calculated! (Player X: %.2f, Start X: %.2f, End X: %.2f)", playerPos.x, fStartX, fEndX) &&
									gnetVerifyf(gridStartY <= gridEndY, "Invalid grid calculated! (Player Y: %.2f, Start Y: %.2f, End Y: %.2f)", playerPos.y, fStartY, fEndY))
								{
									// check if the player has moved out of scope of any grid squares they were controlling,
									// we use a one grid square threshold to avoid thrashing
									for(unsigned index = gridArraySectionStart; index < gridArraySectionEnd; index++)
									{
										NetworkGridSquareInfo &squareInfo = m_GridSquareInfos[index];

										if(squareInfo.m_Owner != INVALID_PLAYER_INDEX)
										{
											gnetAssertf(squareInfo.m_Owner == playerIndex, "Grid square owned by unexpected player!");

											if(squareInfo.m_GridX <  (gridStartX-1) ||
											   squareInfo.m_GridX >= (gridEndX+1)   ||
											   squareInfo.m_GridY <  (gridStartY-1) ||
											   squareInfo.m_GridY >= (gridEndY+1))
											{
												// this player no longer owns this grid square
												squareInfo.Reset();
												NetworkGridSquareInfo::SetDirty(index);
											}
										}
									}

									// check if this player has moved into an unused grid square
									for(u8 gridX = gridStartX; gridX <= gridEndX; gridX++)
									{
										for(u8 gridY = gridStartY; gridY <= gridEndY; gridY++)
										{
											bool squareInUse = false;

											for(unsigned index = 0; index < MAX_GRID_SQUARES && !squareInUse; index++)
											{
												if(m_GridSquareInfos[index].m_GridX == gridX &&
												   m_GridSquareInfos[index].m_GridY == gridY &&
												   m_GridSquareInfos[index].m_Owner != INVALID_PLAYER_INDEX)
												{
													squareInUse = true;
												}
											}

											if(!squareInUse)
											{
												bool added = false;

												for(unsigned index = gridArraySectionStart; index < gridArraySectionEnd && !added; index++)
												{
													NetworkGridSquareInfo &squareInfo = m_GridSquareInfos[index];

													if(squareInfo.m_Owner == INVALID_PLAYER_INDEX)
													{
														squareInfo.m_GridX = gridX;
														squareInfo.m_GridY = gridY;
														squareInfo.m_Owner = playerIndex;
														added = true;

														NetworkGridSquareInfo::SetDirty(index);
													}
												}
											}
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}
}

void CNetworkWorldGridManager::PlayerHasJoined(const netPlayer &UNUSED_PARAM(player))
{
}

void CNetworkWorldGridManager::PlayerHasLeft(const netPlayer &player)
{
    if(NetworkInterface::IsGameInProgress())
    {
	    PhysicalPlayerIndex playerIndex = player.GetPhysicalPlayerIndex();

        if(playerIndex != INVALID_PLAYER_INDEX)
        {
            unsigned gridArraySectionStart = playerIndex * MAX_GRID_SQUARES_PER_PLAYER;
            unsigned gridArraySectionEnd   = gridArraySectionStart + MAX_GRID_SQUARES_PER_PLAYER;

            for(unsigned index = gridArraySectionStart; index < gridArraySectionEnd; index++)
            {
				m_GridSquareInfos[index].Reset();
            }
        }
    }
}

bool CNetworkWorldGridManager::IsPosLocallyControlled(const Vector3 &position)
{
    bool locallyControlled = false;

    PhysicalPlayerIndex playerIndex = NetworkInterface::GetLocalPhysicalPlayerIndex();

    if(playerIndex != INVALID_PLAYER_INDEX)
    {
        float fX = MAX(position.x - WORLDLIMITS_REP_XMIN, 0.0f);
        float fY = MAX(position.y - WORLDLIMITS_REP_YMIN, 0.0f);

        u8 gridX = static_cast<u8>(fX / GRID_SQUARE_SIZE);
        u8 gridY = static_cast<u8>(fY / GRID_SQUARE_SIZE);

        unsigned gridArraySectionStart = playerIndex * MAX_GRID_SQUARES_PER_PLAYER;
        unsigned gridArraySectionEnd   = gridArraySectionStart + MAX_GRID_SQUARES_PER_PLAYER;

        gnetAssertf(gridArraySectionStart <  MAX_GRID_SQUARES, "Invalid grid section calculated!");
        gnetAssertf(gridArraySectionEnd   <= MAX_GRID_SQUARES, "Invalid grid section calculated!");

        for(unsigned index = gridArraySectionStart; index < gridArraySectionEnd && !locallyControlled; index++)
        {
            const NetworkGridSquareInfo &squareInfo = m_GridSquareInfos[index];

            if(squareInfo.m_Owner == playerIndex &&
               squareInfo.m_GridX == gridX       &&
               squareInfo.m_GridY == gridY)
            {
                locallyControlled = true;
            }
        }
    }

    return locallyControlled;
}

#if __BANK

void CNetworkWorldGridManager::AddDebugWidgets()
{
#if __DEV
    bkBank *bank = BANKMGR.FindBank("Network");

    if(gnetVerifyf(bank, "Unable to find network bank!"))
    {
        bank->PushGroup("Debug World Grid", false);
            bank->AddToggle("Display Grid Ownership On Vector Map", &gbDisplayOnVectorMap);
        bank->PopGroup();
    }
#endif // __DEV
}

#if __DEV

void CNetworkWorldGridManager::DisplayOnVectorMap()
{
    if(gbDisplayOnVectorMap)
    {
        for(unsigned index = 0; index < MAX_GRID_SQUARES; index++)
        {
            const NetworkGridSquareInfo &squareInfo = m_GridSquareInfos[index];

            if(squareInfo.m_Owner != INVALID_PLAYER_INDEX)
            {
                Vector3 min, max;

                min.x = (squareInfo.m_GridX * GRID_SQUARE_SIZE) + WORLDLIMITS_REP_XMIN;
                min.y = (squareInfo.m_GridY * GRID_SQUARE_SIZE) + WORLDLIMITS_REP_YMIN;
                min.z = 0.0f;
                max.x   = min.x + GRID_SQUARE_SIZE;
                max.y   = min.y + GRID_SQUARE_SIZE;
                max.z   = 0.0f;
                
                Vector3 v1(min.x, min.y, 0.0f); // tl
                Vector3 v2(max.x, min.y, 0.0f); // tr
                Vector3 v3(min.x, max.y, 0.0f); // bl
                Vector3 v4(max.x, max.y, 0.0f); // br

                Color32 playerColour = NetworkColours::GetNetworkColour(NetworkColours::GetDefaultPlayerColour(squareInfo.m_Owner));
                playerColour.SetAlpha(128);

                CVectorMap::DrawPoly(v1, v2, v4, playerColour, true);
                CVectorMap::DrawPoly(v1, v3, v4, playerColour, true);
            }
        }
    }
}

#endif // __DEV
#endif // __BANK
