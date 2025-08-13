//
// filename:    NetworkColours.cpp
// description: Network colours: functions for manipulating colours of network players/teams etc.
// written by:  Daniel Yelland
//

// --- Include Files ------------------------------------------------------------
#include "NetworkColours.h"

#include "frontend/hud_colour.h"
#include "frontend/minimap.h"
#include "network/network.h"

#include "bank/bank.h"

namespace NetworkColours
{
	NetworkColour g_TeamColours[MAX_NUM_TEAMS] =
	{
		NETWORK_COLOUR_1,
		NETWORK_COLOUR_2,
		NETWORK_COLOUR_3,
		NETWORK_COLOUR_4,
		NETWORK_COLOUR_5,
		NETWORK_COLOUR_6,
		NETWORK_COLOUR_7,
		NETWORK_COLOUR_8,
		NETWORK_COLOUR_9,
		NETWORK_COLOUR_10,
		NETWORK_COLOUR_11,
		NETWORK_COLOUR_12,
		NETWORK_COLOUR_13,
		NETWORK_COLOUR_14,
		NETWORK_COLOUR_15,
		NETWORK_COLOUR_16,
 	};

    Color32 g_CustomTeamColours[MAX_NUM_TEAMS];

    //
    // name:        GetNetworkColour
    // description: Returns a Color32 for the specified network colour
    //
    Color32 GetNetworkColour (NetworkColour netColour)
    {
        Color32 colour(0,0,0,0);

        if(AssertVerify(netColour > INVALID_COLOUR && netColour < MAX_NETWORK_COLOURS))
        {
			colour = CHudColour::GetRGB(static_cast<eHUD_COLOURS>(MapNetworkColourToHudColour(netColour)), 255);
        }

        return colour;
    }

 	//
    // name:        GetNumNetworkColours
    // description: Returns the total number of available network colours
    //
    unsigned int GetNumNetworkColours()
    {
        return MAX_NETWORK_COLOURS;
    }

	//
    // name:        GetDefaultPlayerColour
    // description: Returns a default player colour based on the player Id
    //
    NetworkColour GetDefaultPlayerColour(PhysicalPlayerIndex playerIndex)
    {
        NetworkColour playerColour = INVALID_COLOUR;

        int colourIndex = playerIndex+1;

        if(AssertVerify(colourIndex > INVALID_COLOUR) && AssertVerify(colourIndex < MAX_NETWORK_COLOURS))
        {
            playerColour = static_cast<NetworkColour>(colourIndex);
        }

        return playerColour;
    }

 	//
    // name:        SetTeamColour
    // description: Sets the colour for a team
    //
    void SetTeamColour(int team, NetworkColour netColour)
	{
		gnetAssert(team < MAX_NUM_TEAMS);
		g_TeamColours[team] = netColour;
	}

    void SetCustomTeamColour(int team, const Color32 &customColour)
    {
        if(team >= 0 && team < MAX_NUM_TEAMS)
        {
            g_CustomTeamColours[team] = customColour;
            g_TeamColours[team]       = NETWORK_COLOUR_CUSTOM;
        }
    }

 	//
    // name:        GetTeamColour
    // description: Returns the colour of a team
    //
    NetworkColour GetTeamColour(int team)
	{
		gnetAssert(team < MAX_NUM_TEAMS);
		return g_TeamColours[team];
	}

    Color32 GetCustomTeamColour(int team)
    {
        if(team >= 0 && team < MAX_NUM_TEAMS)
        {
            return g_CustomTeamColours[team];
        }

        return Color32(255,255,255,255);
    }

	int MapNetworkColourToHudColour(NetworkColour netColour)
	{
		if(NETWORK_COLOUR_1 <= netColour && netColour <= NETWORK_COLOUR_32)
		{
			return (netColour - NETWORK_COLOUR_1)
				+ HUD_COLOUR_NET_PLAYER1;
		}

		return HUD_COLOUR_PURE_WHITE;
	}


	int MapNetworkColourToRadarBlipColour(NetworkColour netColour)
	{
		if(NETWORK_COLOUR_1 <= netColour && netColour <= NETWORK_COLOUR_32)
		{
			return (netColour - NETWORK_COLOUR_1)
				+ BLIP_COLOUR_NET_PLAYER1;
		}

		return BLIP_COLOUR_DEFAULT;
	}
}
