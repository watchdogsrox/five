//
// name:		NetworkFakePlayerNames.cpp
// description:	Allows peds in the single player game to be given fake player names to display
//              over their heads to simulate network players. Useful for network tutorial scripts
//              run without joining a real network session
// written by:	Daniel Yelland
//

#include "NetworkFakePlayerNames.h"

#include "debug/debugscene.h"
#include "network/general/NetworkUtil.h"
#include "peds/ped.h"
#include "renderer/DrawLists/drawListNY.h"

unsigned CFakePlayerNames::ms_NumFakePlayerNames = 0;
CFakePlayerNames::FakePlayerName CFakePlayerNames::ms_FakePlayerNames[CFakePlayerNames::MAX_FAKE_PLAYER_NAMES];

void CFakePlayerNames::Init()
{
    RemoveAllFakePlayerNames();
}

void CFakePlayerNames::AddFakePlayerName(CPed *ped, const char *playerName, Color32 colour)
{
    if(gnetVerifyf(playerName, "Invalid player name!") && gnetVerifyf(strlen(playerName) < MAX_FAKE_PLAYER_NAME_LENGTH, "Player name specified is too long!"))
    {
        // ensure the specified ped has not already been given a fake player name
        bool pedFound = false;

        for(unsigned index = 0; index < ms_NumFakePlayerNames && !pedFound; index++)
        {
            if(ms_FakePlayerNames[index].m_Ped == ped)
            {
                ms_FakePlayerNames[index].m_Colour = colour;

                safecpy(ms_FakePlayerNames[index].m_Name, playerName, MAX_FAKE_PLAYER_NAME_LENGTH);

                pedFound = true;
            }
        }

        // if not then add a new fake player name for this ped
        if(!pedFound)
        {
            if(gnetVerifyf(ms_NumFakePlayerNames < MAX_FAKE_PLAYER_NAMES, "The maximum number of fake player names have been registered!"))
            {
                ms_FakePlayerNames[ms_NumFakePlayerNames].m_Ped    = ped;
                ms_FakePlayerNames[ms_NumFakePlayerNames].m_Colour = colour;

                safecpy(ms_FakePlayerNames[ms_NumFakePlayerNames].m_Name, playerName, MAX_FAKE_PLAYER_NAME_LENGTH);

                ms_NumFakePlayerNames++;
            }
        }
    }
}

void CFakePlayerNames::RemoveFakePlayerName(CPed *ped)
{
    if(gnetVerifyf(ped, "Invalid ped specified!"))
    {
        bool pedFound = false;

        for(unsigned index = 0; index < ms_NumFakePlayerNames && !pedFound; index++)
        {
            if(ms_FakePlayerNames[index].m_Ped == ped)
            {
                // shuffle further entries over the top of the old entry
                for(unsigned index2 = index; index2 < ms_NumFakePlayerNames - 1; index2++)
                {
                    ms_FakePlayerNames[index2].m_Ped    = ms_FakePlayerNames[index2+1].m_Ped;
                    ms_FakePlayerNames[index2].m_Colour = ms_FakePlayerNames[index2+1].m_Colour;

                    safecpy(ms_FakePlayerNames[index].m_Name, ms_FakePlayerNames[index2].m_Name, MAX_FAKE_PLAYER_NAME_LENGTH);
                }

                // clear the reference to the ped at the end of the list
                ms_FakePlayerNames[ms_NumFakePlayerNames - 1].m_Ped = 0;

                ms_NumFakePlayerNames--;
                pedFound = true;
            }
        }
    }
}

void CFakePlayerNames::RemoveAllFakePlayerNames()
{
    // release all references to the peds
    for(unsigned index = 0; index < ms_NumFakePlayerNames; index++)
    {
        ms_FakePlayerNames[index].m_Ped = 0;
    }

    ms_NumFakePlayerNames = 0;
}

bool CFakePlayerNames::DoesPedHaveFakeName(CPed *ped)
{
    bool hasFakeName = false;

    if(gnetVerifyf(ped, "Invalid ped specified"))
    {
        for(unsigned index = 0; index < ms_NumFakePlayerNames && !hasFakeName; index++)
        {
            if(ms_FakePlayerNames[index].m_Ped == ped)
            {
                hasFakeName = true;
            }
        }
    }

    return hasFakeName;
}

unsigned CFakePlayerNames::GetNumFakeNames()
{
    return ms_NumFakePlayerNames;
}

void CFakePlayerNames::RenderFakePlayerNames()
{
    for(unsigned index = 0; index < ms_NumFakePlayerNames; index++)
    {
        if(gnetVerifyf(ms_FakePlayerNames[index].m_Ped, "Unexpected NULL ped!"))
        {
            float   scale = 1.0f;
	        Vector2 screenCoords;
	        if(NetworkUtils::GetScreenCoordinatesForOHD(VEC3V_TO_VECTOR3(ms_FakePlayerNames[index].m_Ped->GetTransform().GetPosition()), screenCoords, scale))
	        {
                DLC ( CDrawNetworkPlayerName_NY, (screenCoords, ms_FakePlayerNames[index].m_Colour, scale, ms_FakePlayerNames[index].m_Name));
            }
        }
    }
}

#if __BANK

static void NetworkBank_AddFakePlayerName()
{
    CEntity *focusEntity = CDebugScene::FocusEntities_Get(0);

    if(focusEntity && focusEntity->GetIsTypePed())
    {
        CPed *focusPed = static_cast<CPed *>(focusEntity);

        CFakePlayerNames::AddFakePlayerName(focusPed, "Fake Player", Color32(0, 0, 255));
    }
}

static void NetworkBank_RemoveFakePlayerName()
{
    CEntity *focusEntity = CDebugScene::FocusEntities_Get(0);

    if(focusEntity && focusEntity->GetIsTypePed())
    {
        CPed *focusPed = static_cast<CPed *>(focusEntity);

        CFakePlayerNames::RemoveFakePlayerName(focusPed);
    }
}

static void NetworkBank_RemoveAllFakePlayerNames()
{
    CFakePlayerNames::RemoveAllFakePlayerNames();
}

void CFakePlayerNames::AddDebugWidgets()
{
    bkBank *bank = BANKMGR.FindBank("Network");

    if(gnetVerifyf(bank, "Unable to find network bank!"))
    {
        bank->PushGroup("Debug Fake Player Names", false);
            bank->AddButton("Add Fake Player Name",         datCallback(NetworkBank_AddFakePlayerName));
            bank->AddButton("Remove Fake Player Name",      datCallback(NetworkBank_RemoveFakePlayerName));
            bank->AddButton("Remove All Fake Player Names", datCallback(NetworkBank_RemoveAllFakePlayerNames));
        bank->PopGroup();
    }
}

#endif // __BANK
