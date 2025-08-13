/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : VideoEditorPauseMenu.cpp
// PURPOSE : the pausemenu menu tab for replay editor
// AUTHOR  : Derek Payne
// STARTED : 11/09/2014
//
/////////////////////////////////////////////////////////////////////////////////

#include "Frontend/VideoEditorPauseMenu.h"

#if GTA_REPLAY
#include "audio/frontendaudioentity.h"
#include "control/replay/ReplaySettings.h"
#include "Frontend/ButtonEnum.h"
#include "Frontend/CMenuBase.h"
#include "Frontend/VideoEditor/ui/Editor.h"
#include "Frontend/PauseMenu.h"
#include "Network/Live/livemanager.h"
#include "Network/NetworkInterface.h"
#include "Script/Script.h"

FRONTEND_OPTIMISATIONS()

bool CPauseVideoEditorMenu::sm_waitOnSignIn = false;
bool CPauseVideoEditorMenu::sm_waitOnSignInUi = false;
char CPauseVideoEditorMenu::sm_ButtonURL[MAX_NEWS_URL_LENGTH];


CPauseVideoEditorMenu::CPauseVideoEditorMenu( CMenuScreen& owner ): CMenuBase(owner),
	m_isDisplayingNews(false),
	m_storyTextPosted(false)
{	
	Reset();
}



CPauseVideoEditorMenu::~CPauseVideoEditorMenu()
{
	Reset();
}



void CPauseVideoEditorMenu::Reset()
{
	sm_waitOnSignInUi = false;
	sm_waitOnSignIn = false;
	memset(sm_ButtonURL, '\0', MAX_NEWS_URL_LENGTH);
}

void CPauseVideoEditorMenu::Init()
{
	Reset();
}
 


void CPauseVideoEditorMenu::Update()
{
    CMenuBase::Update();

	if (sm_waitOnSignInUi)
	{
		if (CLiveManager::IsSystemUiShowing())  // we were waiting on the ui, once appears, we wait on a sign in by the user
		{
			sm_waitOnSignInUi = false;
			sm_waitOnSignIn = true;

			Populate(MENU_UNIQUE_ID_REPLAY_EDITOR);  // repopulate
		}

		return;
	}

	if (sm_waitOnSignIn)  // waiting on sign in by the user
	{
		// the !SystemShowing check was causing some issues on Durango, so made it Orbisy only.

		if (CLiveManager::IsSignedIn() && NetworkInterface::IsCloudAvailable() ORBIS_ONLY( || !CLiveManager::IsSystemUiShowing() ) )  // if we are now signed in, or the ui is put away (Orbis only)
		{
			Reset(); // no longer waiting on sign in

			Populate(MENU_UNIQUE_ID_REPLAY_EDITOR);  // repopulate
		}

		return;
	}

	// update news events - 2205108
	if (m_isDisplayingNews)
	{
		CNetworkPauseMenuNewsController& newsController = CNetworkPauseMenuNewsController::Get();
		if(newsController.IsPendingStoryReady())
		{
			PostEditorStory(newsController.GetPendingStory(), true);
			UpdateStoreColumnScroll(newsController.GetNumStories(), newsController.GetPendingStoryIndex());
			CNetworkPauseMenuNewsController::Get().MarkPendingStoryAsShown();
			m_storyTextPosted = false;
		}
		else if(!m_storyTextPosted && newsController.IsPendingStoryDataRcvd())
		{
			PostEditorStory(newsController.GetPendingStory(), false);
			UpdateStoreColumnScroll(newsController.GetNumStories(), newsController.GetPendingStoryIndex());
			m_storyTextPosted = true;
		}
	}
}



bool CPauseVideoEditorMenu::UpdateInput( s32 sInput )
{
    if( sInput == PAD_CROSS  )
    {
#if USE_SAVE_SYSTEM		//	If we've enabled the code to trigger a savegame on viewing a clip, we can't allow the player to enter the Replay Editor while they're on a mission or in director mode
		const bool bIsOnMission = CTheScripts::GetPlayerIsOnAMission() || CTheScripts::GetPlayerIsInAnimalForm() || CTheScripts::GetPlayerIsRepeatingAMission();
#else	//	USE_SAVE_SYSTEM
		//	Viewing a clip while repeating a mission causes problems even if the savegame code is disabled, so we'll block the Replay Editor during a mission repeat
		const bool bIsOnMission = CTheScripts::GetPlayerIsRepeatingAMission();
#endif	//	USE_SAVE_SYSTEM

		if (!bIsOnMission)
		{
			const bool bNeedsToSignIn = RSG_PC ? false : !CLiveManager::IsSignedIn();

			if ( (!sm_waitOnSignInUi) && (bNeedsToSignIn) )
			{
				CLiveManager::ShowSigninUi();
				sm_waitOnSignInUi = true;
			}
			else if ( (!sm_waitOnSignInUi) && (!sm_waitOnSignIn) )
			{
#if GTA_REPLAY
				g_FrontendAudioEntity.PlaySound( "SELECT", "HUD_FRONTEND_DEFAULT_SOUNDSET" );  // play sound here as we never actually "enter" this menu
				CPauseMenu::UpdateProfileFromMenuOptions(false);				
				CVideoEditorUi::Open(OFS_CODE_PAUSE_MENU);
#endif // #if GTA_REPLAY
			}
		}
    }

    return CMenuBase::UpdateInput( sInput );
}



bool CPauseVideoEditorMenu::Populate(MenuScreenId newScreenId)
{
	bool result = false;

	if(newScreenId.GetValue() == MENU_UNIQUE_ID_REPLAY_EDITOR)
	{
		bool bShowAcceptButton = true;

		const bool bNeedsToSignIn = RSG_PC ? false : !CLiveManager::IsSignedIn();

#if USE_SAVE_SYSTEM		//	If we've enabled the code to trigger a savegame on viewing a clip, we can't allow the player to enter the Replay Editor while they're on a mission or in director mode
		const bool bIsOnMission = CTheScripts::GetPlayerIsOnAMission() || CTheScripts::GetPlayerIsInAnimalForm() || CTheScripts::GetPlayerIsRepeatingAMission();
#else	//	USE_SAVE_SYSTEM
		//	Viewing a clip while repeating a mission causes problems even if the savegame code is disabled, so we'll block the Replay Editor during a mission repeat
		const bool bIsOnMission = CTheScripts::GetPlayerIsRepeatingAMission();
#endif	//	USE_SAVE_SYSTEM

		if (bIsOnMission)
		{
			PopulateWarning("PM_PANE_MOV", "VE_NOTAVAIL");
			bShowAcceptButton = false;
		}
		else if (bNeedsToSignIn)
		{
			PopulateWarning("WARNING_NOT_CONNECTED_TITLE", "NOT_CONNECTED");
		}
		else if (!PopulateNews())  // populate the news 
		{
			PopulateWarning("PM_PANE_MOV", "VE_ENTERVIDEOEDITOR");
		}

		if (bShowAcceptButton)
		{
			if( SUIContexts::IsActive( "HIDE_ACCEPTBUTTON" ) )
			{
				SUIContexts::Deactivate("HIDE_ACCEPTBUTTON");
			}
		}
		else
		{
			SUIContexts::Activate("HIDE_ACCEPTBUTTON");
		}

		result = true;
	}

	return result;
}

void CPauseVideoEditorMenu::PopulateWarning(const char* locTitle, const char* locBody)
{
	ShowColumnWarning_Loc(PM_COLUMN_LEFT, 3, locBody, locTitle, 0, "PAUSE_MENU_WARNING_REDITOR", "REDITOR", IMG_ALIGN_RIGHT, "", true);
}

void CPauseVideoEditorMenu::LoseFocus()
{
	Reset();

	m_isDisplayingNews = false;

	// Cleanup Newswire
	CNetworkPauseMenuNewsController::Get().Reset();
}

void CPauseVideoEditorMenu::UpdateStoreColumnScroll(int iNumStories, int iSelectedStory)
{
	char pszCaption[64];
	CNumberWithinMessage pArrayOfNumbers[2];
	pArrayOfNumbers[0].Set(iSelectedStory+1);
	pArrayOfNumbers[1].Set(iNumStories);

	CMessages::InsertNumbersAndSubStringsIntoString(TheText.Get("NEWS_NUM_STORIES"),pArrayOfNumbers,2,NULL,0,pszCaption,64);
	CScaleformMenuHelper::SET_COLUMN_SCROLL(PM_COLUMN_LEFT, pszCaption);
}


bool CPauseVideoEditorMenu::PopulateNews()
{
	if (CLiveManager::IsOnline() && NetworkInterface::IsCloudAvailable())
	{
		CNetworkPauseMenuNewsController& newsController = CNetworkPauseMenuNewsController::Get();

		if(!m_isDisplayingNews && newsController.InitControl(NEWS_TYPE_ROCKSTAR_EDITOR_HASH))
		{
			int iNumStories = newsController.GetNumStories();
			if(iNumStories > 1)
			{
				if(!SUIContexts::IsActive("STORE_CanScroll"))
				{
					SUIContexts::Activate("STORE_CanScroll");
					CPauseMenu::RedrawInstructionalButtons();
				}
				UpdateStoreColumnScroll(iNumStories, newsController.GetPendingStoryIndex());
			}

			m_isDisplayingNews = true;
			ShowColumnWarning(PM_COLUMN_LEFT, 3, "");
			CScaleformMenuHelper::SET_BUSY_SPINNER(PM_COLUMN_MIDDLE, true);

			return true;
		}
	}

	return false;
}

void CPauseVideoEditorMenu::PostEditorStory(CNetworkSCNewsStoryRequest* pNewsItem, bool bUpdateTexture)
{
	CScaleformMenuHelper::SET_BUSY_SPINNER(PM_COLUMN_MIDDLE_RIGHT, false);

	if(bUpdateTexture)
	{
		ShowColumnWarning(PM_COLUMN_LEFT, 3, pNewsItem->GetContent(), pNewsItem->GetHeadline(), 0, pNewsItem->GetTxdName(), pNewsItem->GetTxdName(), IMG_ALIGN_RIGHT);
	}
	else
	{
		ShowColumnWarning(PM_COLUMN_LEFT, 3, pNewsItem->GetContent(), pNewsItem->GetHeadline(), 0, "", "", IMG_ALIGN_RIGHT);
	}

	if(pNewsItem->HasExtraDataMember("buttonurl"))
	{
		pNewsItem->GetExtraNewsData("buttonurl", sm_ButtonURL, MAX_NEWS_URL_LENGTH);
	}

	if(pNewsItem->HasExtraDataMember("showbutton"))
	{
		int iShowButton = 0;
		pNewsItem->GetExtraNewsData("showbutton", iShowButton);
		SUIContexts::SetActive("SHOW_VIDEO_BUTTON", iShowButton == 1);
		CPauseMenu::RedrawInstructionalButtons();
	}

	CScaleformMenuHelper::SET_BUSY_SPINNER(PM_COLUMN_MIDDLE, false);

}

#endif //GTA_REPLAY

// eof
