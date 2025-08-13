/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : BootupScreen.cpp
// PURPOSE : Base class for screens used in the boot flow
//
// AUTHOR  : derekp
// STARTED : 2013, relocated October 2020
//
/////////////////////////////////////////////////////////////////////////////////
#include "BootupScreen.h"

// rage 
#include "string/string.h"
#include "system/param.h"

// game
#include "frontend/ProfileSettings.h"
#include "Network/Live/livemanager.h"
#include "Network/NetworkInterface.h"
#include "Network/SocialClubServices/SocialClubCommunityEventMgr.h"
#include "text/messages.h"
#include "text/TextFile.h"
#include "scene/DynamicEntity.h"

PARAM(mpavailable, "Flag that MP is available as the user has completed the prologue");

char CBootupScreen::m_EventButtonDisplayName[MAX_CUSTOM_BUTTON_TEXT];
char CBootupScreen::m_JobButtonDisplayName[MAX_CUSTOM_BUTTON_TEXT];

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	HasCompletedPrologue
// PURPOSE:	Check if the prologue has been completed ( first autosave )
/////////////////////////////////////////////////////////////////////////////////////
bool CBootupScreen::HasCompletedPrologue()
{
#if	!__FINAL
	// command line parameter to skip completed prologue checks
	if(PARAM_mpavailable.Get())
		return true;
#endif

	if(NetworkInterface::IsMultiplayerLocked())
	{
		return false;
	}

	CProfileSettings& settings = CProfileSettings::GetInstance();
	if(settings.AreSettingsValid() && settings.Exists(CProfileSettings::PROLOGUE_COMPLETE))
	{
		return true;
	}

	return false;
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CanSelectOnline
// PURPOSE:	This is purely for UI purposes.  It's a lighter version of
// 			CNetwork::GetNetworkSession().CanAccessMultiplayer which is only a valid check
// 			later on in the loading sequence
/////////////////////////////////////////////////////////////////////////////////////
bool CBootupScreen::CanSelectOnline()
{
    bool result = NetworkInterface::IsLocalPlayerOnline() &&
        NetworkInterface::CheckOnlinePrivileges() &&
        !CLiveManager::GetInviteMgr().HasPendingAcceptedInvite();
#if !RSG_GEN9
    result = result && HasCompletedPrologue();
#endif
    return result;
}

#if RSG_SCE

bool CBootupScreen::ShouldShowPlusUpsell()
{
    if(!NetworkInterface::IsPlayStationPlusCustomUpsellEnabled())
		return false;

    if(NetworkInterface::DoesTunableDisablePlusLoadingButton())
        return false;

    if(NetworkInterface::HasRawPlayStationPlusPrivileges())
        return false;

    return true;
}
#endif // RSG_ORBIS

const char* CBootupScreen::GetEventButtonText(bool bGetLoadingText)
{
    char eventButtonName[MAX_CUSTOM_BUTTON_TEXT];
    bool hasDisplayName = false;

#if RSG_ORBIS
    if(ShouldShowPlusUpsell())
    {
        if(bGetLoadingText)
        {
            return TheText.Get( ATSTRINGHASH( "BUTTON_PLUS_PROMO_LOADING", 0x1C5405EE ), "BUTTON_PLUS_PROMO_LOADING" );
        }
        else
        {
            hasDisplayName = SocialClubEventMgr::Get().GetEventDisplayNameById(SocialClubEventMgr::Get().GetPlayStationPlusPromotionEventId(), eventButtonName);
            if(!hasDisplayName)
            {
                return TheText.Get( ATSTRINGHASH( "BUTTON_PLUS_PROMO", 0x5A093784 ), "BUTTON_PLUS_PROMO" );
            }
        }
    }
    else
#endif // RSG_ORBIS

    if(SocialClubEventMgr::Get().HasFeaturedEvent())
    {
        hasDisplayName = SocialClubEventMgr::Get().GetEventDisplayNameById(SocialClubEventMgr::Get().GetFeaturedEventId(), eventButtonName);
        if(!hasDisplayName)
        {
            if(SocialClubEventMgr::Get().IsEventActive("FeaturedPlaylist"))	// Featured Event
            {
                return bGetLoadingText ? TheText.Get( ATSTRINGHASH( "MP_LOADING_FEAT_PLAYLIST", 0x6181DD91 ), "MP_LOADING_FEAT_PLAYLIST" ) : TheText.Get( ATSTRINGHASH( "MP_FEAT_PLAYLIST", 0xA493A0DC ), "MP_FEAT_PLAYLIST" );
            }
            else // Generic Event
            {
                return bGetLoadingText ? TheText.Get( ATSTRINGHASH( "MP_LOADING_EVENT_NAME", 0x5AE876A9 ), "MP_LOADING_EVENT_NAME" ) : TheText.Get( ATSTRINGHASH( "MP_EVENT_NAME", 0x1D2843A5 ), "MP_EVENT_NAME" );
            }
        }
    }

    // handle this once
    if(hasDisplayName)
    {
        if(bGetLoadingText)
        {
            CSubStringWithinMessage SubStringArray[1];
            SubStringArray[0].SetCharPointer(eventButtonName);

            CMessages::InsertNumbersAndSubStringsIntoString( TheText.Get( ATSTRINGHASH( "MP_LOADING_EVENT_DNAME", 0x942499A2 ), "MP_LOADING_EVENT_DNAME" ),
                                                            NULL, 0,
                                                            SubStringArray, 1,
                                                            m_EventButtonDisplayName, MAX_CUSTOM_BUTTON_TEXT);
        }
        else
        {
            safecpy(m_EventButtonDisplayName, eventButtonName);
        }
        return m_EventButtonDisplayName;
    }

    return nullptr;
}

const char* CBootupScreen::GetJobButtonText(bool bGetLoadingText)
{
	if(SocialClubEventMgr::Get().HasFeaturedJob())
	{
		char jobButtonName[MAX_CUSTOM_BUTTON_TEXT];
		bool hasDisplayName = SocialClubEventMgr::Get().GetEventDisplayNameById(SocialClubEventMgr::Get().GetFeaturedJobId(), jobButtonName);
		if(hasDisplayName)
		{
			if(bGetLoadingText)
			{
				CSubStringWithinMessage SubStringArray[1];
				SubStringArray[0].SetCharPointer(jobButtonName);

				CMessages::InsertNumbersAndSubStringsIntoString( TheText.Get( ATSTRINGHASH( "MP_LOADING_EVENT_DNAME", 0x942499A2 ), "MP_LOADING_EVENT_DNAME" ),
																NULL, 0, 
																SubStringArray, 1, 
																m_JobButtonDisplayName, MAX_CUSTOM_BUTTON_TEXT);
			}
			else
			{
				safecpy(m_JobButtonDisplayName, jobButtonName);
			}
			return m_JobButtonDisplayName;
		}	
	}

	// we always fall back to the default
	return bGetLoadingText ? TheText.Get( ATSTRINGHASH( "MP_LOADING_RANDOM_NAME", 0x86CA8189 ), "MP_LOADING_RANDOM_NAME" ) : TheText.Get( ATSTRINGHASH( "PM_INF_QKMT6", 0xCF9CE5ED ), "PM_INF_QKMT6" );
}
