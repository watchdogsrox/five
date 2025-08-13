//
// name:		NetworkFakePlayerNames.h
// description:	Allows peds in the single player game to be given fake player names to display
//              over their heads to simulate network players. Useful for network tutorial scripts
//              run without joining a real network session
// written by:	Daniel Yelland
//

#ifndef NETWORK_FAKE_PLAYER_NAMES_H
#define NETWORK_FAKE_PLAYER_NAMES_H

#include "scene\RegdRefTypes.h"
#include "vector/color32.h"

class CPed;

class CFakePlayerNames
{
public:

    static void Init();
    static void AddFakePlayerName(CPed *ped, const char *playerName, Color32 colour);
    static void RemoveFakePlayerName(CPed *ped);
    static void RemoveAllFakePlayerNames();
    static bool DoesPedHaveFakeName(CPed *ped);
    static unsigned GetNumFakeNames();

    static void RenderFakePlayerNames();

#if __BANK
    static void AddDebugWidgets();
#endif // __BANK

private:

    static const unsigned MAX_FAKE_PLAYER_NAMES       = 32;
    static const unsigned MAX_FAKE_PLAYER_NAME_LENGTH = 128;

    struct FakePlayerName
    {
        RegdPed m_Ped;
        char    m_Name[MAX_FAKE_PLAYER_NAME_LENGTH];
        Color32 m_Colour;
    };

    static unsigned       ms_NumFakePlayerNames;
    static FakePlayerName ms_FakePlayerNames[MAX_FAKE_PLAYER_NAMES];
};

#endif  // NETWORK_FAKE_PLAYER_NAMES_H
