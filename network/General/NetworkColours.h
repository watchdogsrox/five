//
// name:        NetworkColours.h
// description: Network colours: functions for manipulating colours of network players/teams etc.
// written by:  Daniel Yelland
//

#ifndef NETWORK_COLOURS_H
#define NETWORK_COLOURS_H

#include "fwnet/NetTypes.h"
#include "renderer/color.h"

namespace rage
{
    class bkBank;
}

namespace NetworkColours
{
	enum NetworkColour
	{
		INVALID_COLOUR,
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
		NETWORK_COLOUR_17,
		NETWORK_COLOUR_18,
		NETWORK_COLOUR_19,
		NETWORK_COLOUR_20,
		NETWORK_COLOUR_21,
		NETWORK_COLOUR_22,
		NETWORK_COLOUR_23,
		NETWORK_COLOUR_24,
		NETWORK_COLOUR_25,
		NETWORK_COLOUR_26,
		NETWORK_COLOUR_27,
		NETWORK_COLOUR_28,
		NETWORK_COLOUR_29,
		NETWORK_COLOUR_30,
		NETWORK_COLOUR_31,
		NETWORK_COLOUR_32,

        NETWORK_COLOUR_CUSTOM,
		MAX_NETWORK_COLOURS
	};

    //const char*		GetNetworkColourName   (NetworkColour netColour);
    Color32			GetNetworkColour       (NetworkColour netColour);
    unsigned int	GetNumNetworkColours();

    NetworkColour	GetDefaultPlayerColour(PhysicalPlayerIndex     playerIndex);

    NetworkColour	GetTeamColour(int team);
    void			SetTeamColour(int team, NetworkColour);
    Color32         GetCustomTeamColour(int team);
    void            SetCustomTeamColour(int team, const Color32 &customColour);

	int				MapNetworkColourToHudColour(NetworkColour netColour);
	int				MapNetworkColourToRadarBlipColour(NetworkColour netColour);
/*
#if __BANK
    void AddPlayerColourComboWidget(bkBank *bank, const char *comboName, int *currentSelection, void (*comboChangeCallback)());
#endif
*/
}

#endif  // #NETWORK_COLOURS_H
