// game
#include "Frontend/InfoMenu.h"

#include "Frontend/PauseMenu.h"
#include "Frontend/ScaleformMenuHelper.h"
#include "Frontend/Scaleform/ScaleFormMgr.h"
#include "Frontend/ButtonEnum.h"
#include "Frontend/ui_channel.h"
#include "Frontend/GameStream.h"
#include "Frontend/GameStreamMgr.h"
#include "Text/TextConversion.h"
#include "Network/NetworkInterface.h"
#include "Network/Live/livemanager.h"
#include "Network/Live/NetworkClan.h"
#include "Network/SocialClubServices/SocialClubNewsStoryMgr.h"
#include "streaming/streaming.h"
#include "frontend/MousePointer.h"

//OPTIMISATIONS_OFF()
FRONTEND_OPTIMISATIONS()

#define OFFLINE_NEWSWIRE_CONTEXT "NewswireOffline"

CInfoMenu::CInfoMenu(CMenuScreen& owner) 
	: CMenuBase(owner)
	, m_iStreamedFeedCount(0)
	, m_bStoryTextPosted(false)
	, m_iMouseScrollDirection(SCROLL_CLICK_NONE)
{
}

CInfoMenu::~CInfoMenu()
{

}

void CInfoMenu::Update()
{
	if(m_CurrentScreen == MENU_UNIQUE_ID_HOME_FEED)
	{
		for(int i = 0; i < m_iStreamedFeedCount; ++i)
		{
			if(m_StreamedFeedItems[i].Status != GAMESTREAM_STATUS_FREE
				&& m_StreamedFeedItems[i].Status != GAMESTREAM_STATUS_SHOWING)
			{
				if(m_StreamedFeedItems[i].Status != GAMESTREAM_STATUS_READYTOSHOW)
				{
					UpdateFeedItem(m_StreamedFeedItems[i]);
				}
				else
				{
					PostFeedItem(m_StreamedFeedItems[i], i,true);
				}

			}
		}
	}
	else if(m_CurrentScreen == MENU_UNIQUE_ID_HOME_NEWSWIRE)
	{
		if(!CLiveManager::IsOnline())
		{
			if(!SUIContexts::IsActive(OFFLINE_NEWSWIRE_CONTEXT))
			{
				PopulateNewswireMenu();
				CScaleformMenuHelper::HIDE_COLUMN_SCROLL(PM_COLUMN_MIDDLE);
				CScaleformMenuHelper::SET_BUSY_SPINNER(PM_COLUMN_MIDDLE, false);
			}
		}
		else if(!NetworkInterface::IsCloudAvailable())
		{
			// We're online but the cloud isn't available
			if(!SUIContexts::IsActive(OFFLINE_NEWSWIRE_CONTEXT))
			{
				PopulateNewswireMenu();
				CScaleformMenuHelper::HIDE_COLUMN_SCROLL(PM_COLUMN_MIDDLE);
				CScaleformMenuHelper::SET_BUSY_SPINNER(PM_COLUMN_MIDDLE, false);
			}
		}
		else if(CNetworkPauseMenuNewsController::Get().HasPendingStoryFailed())
		{
			if(!SUIContexts::IsActive(OFFLINE_NEWSWIRE_CONTEXT))
			{
				SUIContexts::SetActive(OFFLINE_NEWSWIRE_CONTEXT, true);
				CScaleformMenuHelper::HIDE_COLUMN_SCROLL(PM_COLUMN_MIDDLE);
				CScaleformMenuHelper::SET_BUSY_SPINNER(PM_COLUMN_MIDDLE, false);

				ShowColumnWarning_Loc(PM_COLUMN_MIDDLE, 2, "RL_CLAN_UNKNOWN", "CWS_WARNING");
			}
		}
		else if(SUIContexts::IsActive(OFFLINE_NEWSWIRE_CONTEXT))
		{
			// We've just come online or the cloud has just become available
			SUIContexts::SetActive(OFFLINE_NEWSWIRE_CONTEXT, false);
			PopulateNewswireMenu();
		}
		else
		{
			CNetworkPauseMenuNewsController& newsController = CNetworkPauseMenuNewsController::Get();
			if(newsController.IsPendingStoryReady())
			{
				PostNewswireStory(newsController.GetPendingStory(), true);
				UpdateNewswireColumnScroll(newsController.GetNumStories(), newsController.GetPendingStoryIndex());
				newsController.MarkPendingStoryAsShown();
				m_bStoryTextPosted = false;
			}
			else if(!m_bStoryTextPosted && newsController.IsPendingStoryDataRcvd())
			{
				PostNewswireStory(newsController.GetPendingStory(), false);
				UpdateNewswireColumnScroll(newsController.GetNumStories(), newsController.GetPendingStoryIndex());
				m_bStoryTextPosted = true;
			}
		}
	}
}

bool CInfoMenu::UpdateInput(s32 UNUSED_PARAM(sInput))
{
	if(SUIContexts::IsActive(OFFLINE_NEWSWIRE_CONTEXT) && !CLiveManager::IsOnline())
	{
		if (CPauseMenu::CheckInput(FRONTEND_INPUT_ACCEPT, true))
		{
			CLiveManager::ShowSigninUi();
		}
	}

	if(CPauseMenu::IsNavigatingContent() && SUIContexts::IsActive("INFO_CanScroll"))
	{
		if (m_CurrentScreen == MENU_UNIQUE_ID_HOME_NEWSWIRE)
		{
			if (CPauseMenu::CheckInput(FRONTEND_INPUT_RDOWN) || m_iMouseScrollDirection == SCROLL_CLICK_DOWN)
			{
				CPauseMenu::PlaySound("NAV_UP_DOWN");

				if(CNetworkPauseMenuNewsController::Get().RequestStory(-1))
				{
					CScaleformMenuHelper::SET_BUSY_SPINNER(PM_COLUMN_MIDDLE_RIGHT, true);
					ClearNewswireStory();
					UpdateNewswireColumnScroll(CNetworkPauseMenuNewsController::Get().GetNumStories(), CNetworkPauseMenuNewsController::Get().GetPendingStoryIndex());				
				}
			}
			else if (CPauseMenu::CheckInput(FRONTEND_INPUT_RUP) || m_iMouseScrollDirection == SCROLL_CLICK_UP)
			{
				CPauseMenu::PlaySound("NAV_UP_DOWN");

				if(CNetworkPauseMenuNewsController::Get().RequestStory(1))
				{
					ClearNewswireStory();
					CScaleformMenuHelper::SET_BUSY_SPINNER(PM_COLUMN_MIDDLE_RIGHT, true);
					UpdateNewswireColumnScroll(CNetworkPauseMenuNewsController::Get().GetNumStories(), CNetworkPauseMenuNewsController::Get().GetPendingStoryIndex());				
				}
			}
		}
		else
		{
			if (CPauseMenu::CheckInput(FRONTEND_INPUT_RUP) || m_iMouseScrollDirection == SCROLL_CLICK_UP)
			{
				CPauseMenu::PlaySound("NAV_UP_DOWN");

				CScaleformMovieWrapper &statMovie = CPauseMenu::GetMovieWrapper(PAUSE_MENU_MOVIE_CONTENT);
				if(statMovie.BeginMethod("SET_COLUMN_INPUT_EVENT"))
				{
					const int AS_INPUT_UP = 8;
					statMovie.AddParam(1); // ColumnID
					statMovie.AddParam(AS_INPUT_UP); // inputID
					statMovie.EndMethod();
				}
			}

			if (CPauseMenu::CheckInput(FRONTEND_INPUT_RDOWN) || m_iMouseScrollDirection == SCROLL_CLICK_DOWN)
			{
				CPauseMenu::PlaySound("NAV_UP_DOWN");

				CScaleformMovieWrapper &statMovie = CPauseMenu::GetMovieWrapper(PAUSE_MENU_MOVIE_CONTENT);
				if(statMovie.BeginMethod("SET_COLUMN_INPUT_EVENT"))
				{
					const int AS_INPUT_DOWN = 9;
					statMovie.AddParam(1); // ColumnID
					statMovie.AddParam(AS_INPUT_DOWN); // inputID
					statMovie.EndMethod();
				}
			}
		}

		m_iMouseScrollDirection = SCROLL_CLICK_NONE;
	}

	return false;
}

bool CInfoMenu::Populate(MenuScreenId newScreenId)
{
	if( newScreenId.GetValue() == MENU_UNIQUE_ID_STATS ||
		newScreenId.GetValue() == MENU_UNIQUE_ID_FRIENDS_MP)
	{
		// There is a very rare edge case going from the stats menu to the brief menu (roughly a 2 frame window) where
		// a late fill content call from the stats menu will try to make it into here. This tells CPauseMenu::FillContent
		// that everything is fine and this menu populates normally the next frame as the correct fill content call comes
		// through. B*1827893
		return true;
	}

	if( newScreenId.GetValue() != MENU_UNIQUE_ID_INFO &&
		newScreenId.GetValue() != MENU_UNIQUE_ID_HOME_MISSION &&
		newScreenId.GetValue() != MENU_UNIQUE_ID_HOME_HELP &&
		newScreenId.GetValue() != MENU_UNIQUE_ID_HOME_BRIEF &&
		newScreenId.GetValue() != MENU_UNIQUE_ID_HOME_FEED &&
		newScreenId.GetValue() != MENU_UNIQUE_ID_HOME_NEWSWIRE &&
		newScreenId.GetValue() != MENU_UNIQUE_ID_HELP)
	{
		return false;
	}

	if(newScreenId == MENU_UNIQUE_ID_HOME_FEED)
	{
		m_iStreamedFeedCount = CGameStreamMgr::GetGameStream()->GetFeedCache().GetCount();
		for(int i = 0; i < m_iStreamedFeedCount; ++i)
		{
			stFeedItem fi = CGameStreamMgr::GetGameStream()->GetFeedCache()[i];
			m_StreamedFeedItems[i].stItem = fi;
			SetupStreamingParams(m_StreamedFeedItems[i]);
		}
	}

	// Release control if moving off of newswire tab
	if(m_CurrentScreen == MENU_UNIQUE_ID_HOME_NEWSWIRE)
	{
		CScaleformMenuHelper::SET_WARNING_MESSAGE_VISIBILITY(false);
		CScaleformMenuHelper::SET_BUSY_SPINNER(PM_COLUMN_MIDDLE_RIGHT, false);
		CNetworkPauseMenuNewsController::Get().Reset();
	}

	m_CurrentScreen = newScreenId;
	FillContent(newScreenId);

	return true;
}

void CInfoMenu::FillContent(MenuScreenId iPaneId)
{
	ClearWarningColumn();
	CScaleformMenuHelper::SET_DATA_SLOT_EMPTY(PM_COLUMN_MIDDLE);

	SUIContexts::SetActive(OFFLINE_NEWSWIRE_CONTEXT, false);

	bool bPopulated = false;

	switch (iPaneId.GetValue())
	{
		case MENU_UNIQUE_ID_HOME_BRIEF:
		{
			bPopulated = PopulateInfoMenu(CMessages::PREV_MESSAGE_TYPE_DIALOG);
			break;
		}

		case MENU_UNIQUE_ID_HOME_HELP:
		{
			bPopulated = PopulateInfoMenu(CMessages::PREV_MESSAGE_TYPE_HELP);
			break;
		}

		case MENU_UNIQUE_ID_HOME_MISSION:
		{
			bPopulated = PopulateInfoMenu(CMessages::PREV_MESSAGE_TYPE_MISSION);
			break;
		}

		case MENU_UNIQUE_ID_HOME_FEED:
		{
			bPopulated = PopulateFeedMenu();
			break;
		}
		case MENU_UNIQUE_ID_HOME_NEWSWIRE:
		{
			bPopulated = PopulateNewswireMenu();
			break;
		}
	}

	if(!bPopulated)
	{
		PopulateWarningMessage(iPaneId.GetValue());
	}

}

void CInfoMenu::PopulateWarningMessage(s32 iMenuID)
{
	// hide normal column
	CScaleformMenuHelper::SHOW_COLUMN(PM_COLUMN_MIDDLE, false);

	// get warning message body
	char* pTitleStringToShow;
	char* pBodyStringToShow;
	if(iMenuID == MENU_UNIQUE_ID_HOME_MISSION)
	{
		if(NetworkInterface::IsNetworkOpen())
		{
			pTitleStringToShow = TheText.Get("PM_PANE_MPJ");
			pBodyStringToShow = TheText.Get("PM_MP_NO_JOB");
		}
		else
		{
			pTitleStringToShow = TheText.Get("PM_PANE_MIS");
			pBodyStringToShow = TheText.Get("PM_NO_MIS");
		}
	}
	else if(iMenuID == MENU_UNIQUE_ID_HOME_HELP)
	{
		pTitleStringToShow = TheText.Get("PM_PANE_HLP");
		pBodyStringToShow = TheText.Get("PM_NO_HELP");
	}
	else if(iMenuID == MENU_UNIQUE_ID_HOME_BRIEF)
	{
		pTitleStringToShow = TheText.Get("PM_PANE_BRI");
		if(NetworkInterface::IsNetworkOpen())
			pBodyStringToShow = TheText.Get("PM_MP_NO_DIA");
		else
			pBodyStringToShow = TheText.Get("PM_NO_DIA");
	}
	else if(iMenuID == MENU_UNIQUE_ID_HOME_FEED)
	{
		pTitleStringToShow = TheText.Get("PM_PANE_FEE");
		pBodyStringToShow = TheText.Get("PM_NO_FEED");
	}
	else if(iMenuID == MENU_UNIQUE_ID_HELP)
	{
		pTitleStringToShow = TheText.Get("PM_PANE_XBHELP");
		pBodyStringToShow = TheText.Get("PM_NO_XBHELP");
	}
	else
	{
		pTitleStringToShow = TheText.Get("PM_PANE_NEWS");
		pBodyStringToShow = TheText.Get("PM_NO_NEWS");
	}

	ShowColumnWarning(PM_COLUMN_MIDDLE, 2, pBodyStringToShow, pTitleStringToShow);
	
}

//////////////////////////////////////////////////////////////////////////

bool CInfoMenu::CheckIncomingFunctions(atHashWithStringBank methodName, const GFxValue* args)
{
	if( methodName == ATSTRINGHASH("COLUMN_CAN_SCROLL",0x11b1b61a) )
	{
		// blink protection, only set this stuff once
		if(!SUIContexts::IsActive("INFO_CanScroll"))
		{
			SUIContexts::Activate("INFO_CanScroll");
			CScaleformMenuHelper::SET_COLUMN_SCROLL(PM_COLUMN_MIDDLE,0,0); // some poopy numbers 'cuz we don't really care that much
			CPauseMenu::RedrawInstructionalButtons();
		}
		return true;
	}

	if (methodName == ATSTRINGHASH("HANDLE_SCROLL_CLICK", 0x9CE8ECE9))
	{
		m_iMouseScrollDirection = (int)args[1].GetNumber();
		return true;
	}

	return false;
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CPauseMenu::PopulateInfoMenu
// PURPOSE: populates the info menu with data from the game
/////////////////////////////////////////////////////////////////////////////////////
bool CInfoMenu::PopulateInfoMenu(CMessages::ePreviousMessageTypes iInfoScreen)
{
	bool bPopulated = false;

	s32 maxNumPrevMessages = CMessages::GetMaxNumberOfPreviousMessages(iInfoScreen);
	
	if( (!CMessages::IsMissionTitleActive() || !CLoadingText::IsActive()) && maxNumPrevMessages == 0)
	{
		bPopulated = false;
	}
	else
	{
		char GxtBriefMessage[MAX_CHARS_IN_EXTENDED_MESSAGE];  // needs to cater for longer strings here as we display long subtitles

		int offset = 0;

		if (iInfoScreen == CMessages::PREV_MESSAGE_TYPE_MISSION)
		{
			if(CPauseMenu::GetCurrenMissionActive())
			{
				char buff[CPauseMenu::MAX_LENGTH_OF_MISSION_TITLE + 112];
				const char *pMissionName = CPauseMenu::GetCurrentMissionLabel();
				if (!CPauseMenu::GetCurrentMissionLabelIsALiteralString())
				{
					pMissionName = TheText.Get(CPauseMenu::GetCurrentMissionLabel());
				}
				if(NetworkInterface::IsNetworkOpen())
				{
					sprintf(buff, "~bold~%s~bold~", pMissionName);
				}
				else
				{
					sprintf(buff, "%s: %s", TheText.Get("PM_PANE_MIS"), pMissionName);
				}

				CScaleformMenuHelper::SET_DATA_SLOT(PM_COLUMN_MIDDLE,offset,-1,-1,OPTION_DISPLAY_NONE, 0, true, buff);
				offset++;
				bPopulated = true;
			}

			if (CPauseMenu::GetCurrentMissionDescriptionIsActive())
			{
				CScaleformMenuHelper::SET_DATA_SLOT(PM_COLUMN_MIDDLE,offset,-1,-1,OPTION_DISPLAY_NONE, 0, true, CPauseMenu::GetCurrentMissionDescription());
				offset++;
				bPopulated = true;
			}
		}

		// add the brief text to the list:
		bool bOnDialogScreen = (iInfoScreen == CMessages::PREV_MESSAGE_TYPE_DIALOG);
		for(s32 i = 0, slotIndex = 0; i < maxNumPrevMessages - offset; i++)
		{
			// populate in reverse
			s32 msgIndex = (maxNumPrevMessages - offset - 1) - i;
		
			if (CMessages::FillStringWithPreviousMessage(iInfoScreen, msgIndex, GxtBriefMessage, NELEM(GxtBriefMessage)))
			{
				CScaleformMenuHelper::SET_DATA_SLOT(PM_COLUMN_MIDDLE, slotIndex + offset, -1, -1, OPTION_DISPLAY_NONE, 0, false, GxtBriefMessage, false, false, false);

				if(bOnDialogScreen)
				{
					const char* txd = CMessages::GetPreviousMessageCharacterTXD(msgIndex);
					CScaleformMgr::AddParamString(txd);
					CScaleformMgr::AddParamString(txd);
				}
				
				CScaleformMgr::EndMethod(false);

				bPopulated = true;
				++slotIndex;
			}
		}
	}

	return bPopulated;
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CPauseMenu::PopulateFeedMenu
// PURPOSE: Populates the feed menu with the last 10 feed flashes
/////////////////////////////////////////////////////////////////////////////////////
bool CInfoMenu::PopulateFeedMenu()
{
	if(!CGameStreamMgr::GetGameStream())
		return false;

	bool bPopulated = false;

	for(int i = 0; i < m_iStreamedFeedCount; ++i)
	{
		bPopulated = PostFeedItem(m_StreamedFeedItems[i], i, false) || bPopulated;
	}

	return bPopulated;
}

bool CInfoMenu::PopulateNewswireMenu()
{
	bool bPopulated = false;
	
	if(!CLiveManager::IsOnline())
	{
		if(!SUIContexts::IsActive(OFFLINE_NEWSWIRE_CONTEXT))
		{
			SUIContexts::SetActive(OFFLINE_NEWSWIRE_CONTEXT, true);
			CPauseMenu::RedrawInstructionalButtons();
		}
		
		ShowColumnWarning_Loc(PM_COLUMN_MIDDLE, 2, CPauseMenu::GetOfflineReasonAsLocKey(), "CWS_WARNING");
		bPopulated = true;	// Prevents the warning message from being overwritten with generic warning message
	}
	else if(CLiveManager::CheckIsAgeRestricted())
	{
		ShowColumnWarning_Loc(PM_COLUMN_MIDDLE, 2, "HUD_PERM", "CWS_WARNING");
		bPopulated = true;	// Prevents the warning message from being overwritten with generic warning message
	}
	else if(!NetworkInterface::IsCloudAvailable())
	{
		if(!SUIContexts::IsActive(OFFLINE_NEWSWIRE_CONTEXT))
		{
			SUIContexts::SetActive(OFFLINE_NEWSWIRE_CONTEXT, true);
			CPauseMenu::RedrawInstructionalButtons();
		}

		ShowColumnWarning_Loc(PM_COLUMN_MIDDLE, 2, "RL_CLAN_UNKNOWN", "CWS_WARNING");
		bPopulated = true;	// Prevents the warning message from being overwritten with generic warning message
	}
	else
	{
		CNetworkPauseMenuNewsController& newsController = CNetworkPauseMenuNewsController::Get();
		
		if(newsController.InitControl(NEWS_TYPE_NEWSWIRE_HASH))
		{
			int iNumStories = newsController.GetNumStories();

			if(iNumStories > 1)
			{
				if(!SUIContexts::IsActive("INFO_CanScroll"))
				{
					SUIContexts::Activate("INFO_CanScroll");
					CPauseMenu::RedrawInstructionalButtons();
				}

				UpdateNewswireColumnScroll(iNumStories, newsController.GetPendingStoryIndex());
			}

			ClearNewswireStory();
			CScaleformMenuHelper::SET_BUSY_SPINNER(PM_COLUMN_MIDDLE_RIGHT, true);
			bPopulated = true;
		}
	}

	return bPopulated;
}

void CInfoMenu::PostNewswireStory(CNetworkSCNewsStoryRequest* pNewsItem, bool bUpdateTexture)
{
	CScaleformMenuHelper::SET_BUSY_SPINNER(PM_COLUMN_MIDDLE_RIGHT, false);

	if(bUpdateTexture)
	{
		ShowColumnWarning(PM_COLUMN_MIDDLE, 2, pNewsItem->GetContent(), pNewsItem->GetSubtitle(), 0, pNewsItem->GetTxdName(), pNewsItem->GetTxdName(), IMG_ALIGN_TOP, pNewsItem->GetURL());
	}
	else
	{
		ShowColumnWarning(PM_COLUMN_MIDDLE, 2, pNewsItem->GetContent(), pNewsItem->GetSubtitle(), 0, "", "", IMG_ALIGN_TOP, pNewsItem->GetURL());
	}
}

void CInfoMenu::ClearNewswireStory()
{
	ShowColumnWarning(PM_COLUMN_MIDDLE, 2, "", "", 0, "", "", IMG_ALIGN_TOP);
}

void CInfoMenu::UpdateNewswireColumnScroll(int iNumStories, int iSelectedStory)
{
	char pszCaption[64];
	CNumberWithinMessage pArrayOfNumbers[2];
	pArrayOfNumbers[0].Set(iSelectedStory+1);
	pArrayOfNumbers[1].Set(iNumStories);

	CMessages::InsertNumbersAndSubStringsIntoString(TheText.Get("NEWS_NUM_STORIES"),pArrayOfNumbers,2,NULL,0,pszCaption,64);
	CScaleformMenuHelper::SET_COLUMN_SCROLL(PM_COLUMN_MIDDLE, pszCaption);
}


bool CInfoMenu::PostFeedItem(stStreamedFeedItems& item, int iIndex, bool bIsUpdate)
{
	CScaleformMovieWrapper& pauseContent = CPauseMenu::GetMovieWrapper(PAUSE_MENU_MOVIE_CONTENT);

	const char* pMethod = bIsUpdate ? "UPDATE_SLOT" : "SET_DATA_SLOT";
	bool bStuffPosted = false;

	if(pauseContent.BeginMethod(pMethod))
	{
		pauseContent.AddParam(PM_COLUMN_MIDDLE);
		pauseContent.AddParam(iIndex);
		pauseContent.AddParam(-1);
		pauseContent.AddParam(999);
		pauseContent.AddParam(item.stItem.iType); // type
		pauseContent.AddParam(0);
		pauseContent.AddParam(1);

		// item data
		pauseContent.AddParam(item.stItem.sTitle);
		pauseContent.AddParam(item.stItem.sSubtitle);
		if(item.stItem.iType == GAMESTREAM_TYPE_TOOLTIPS || item.stItem.iType == GAMESTREAM_TYPE_TICKER)
		{
			pauseContent.AddParam(CScaleformMovieWrapper::Param(item.stItem.sBody, false)); // Don't convert tokens to html for tooltips or ticker
		}
		else
		{
			pauseContent.AddParam(item.stItem.sBody); // Convert tokens to html for non-tooltips
		}
		pauseContent.AddParam(bIsUpdate && item.stItem.bDynamicTexture ? ASSET.FileName(item.stItem.sCloudPath) : item.stItem.sTXD);
		pauseContent.AddParam(bIsUpdate && item.stItem.bDynamicTexture ? ASSET.FileName(item.stItem.sCloudPath) : item.stItem.sTXN);

		switch(item.stItem.iType)
		{
			case GAMESTREAM_TYPE_VERSUS:
				pauseContent.AddParam(item.stItem.sTXDLarge);
				pauseContent.AddParam(item.stItem.sTXNLarge);
				break;
			case GAMESTREAM_TYPE_STATS:
				pauseContent.AddParam(item.stItem.m_stat.iProgressIncrease);
				pauseContent.AddParam(item.stItem.m_stat.iProgress);
				pauseContent.AddParam(HUD_COLOUR_GREEN);  // Character Colour
				break;
			case GAMESTREAM_TYPE_UNLOCK:
				pauseContent.AddParam(item.stItem.iIcon);
				break;
			case GAMESTREAM_TYPE_CREW_TAG:
				pauseContent.AddParam(item.stItem.sCrewTag);
				break;
			case GAMESTREAM_TYPE_AWARD:
				pauseContent.AddParam(item.stItem.eColour);
				break;
			default:
				break;
		}
		
		pauseContent.EndMethod();
		
		bStuffPosted = true;
	}

	if(bIsUpdate)
		item.Status = GAMESTREAM_STATUS_SHOWING;

	return bStuffPosted;
}

void CInfoMenu::LoseFocus()
{
	uiDebugf3("CInfoMenu::LoseFocus");

	// force all menu children off
	for(int i=0; i < m_Owner.MenuItems.GetCount();++i)
		SUIContexts::Deactivate(m_Owner.MenuItems[i].MenuUniqueId.GetParserName());

	SUIContexts::SetActive(OFFLINE_NEWSWIRE_CONTEXT, false);

	// Cleanup Newswire
	CNetworkPauseMenuNewsController::Get().Reset();
}

void CInfoMenu::LayoutChanged( MenuScreenId iPreviousLayout, MenuScreenId iNewLayout, s32 OUTPUT_ONLY(iUniqueId), eLAYOUT_CHANGED_DIR UNUSED_PARAM(eDir) )
{
	uiDebugf3("CInfoMenu::LayoutChanged - Prev: %i, New: %i, Unique: %i", iPreviousLayout.GetValue(), iNewLayout.GetValue(), iUniqueId);
	SUIContexts::Deactivate("INFO_CanScroll");

	for(int i=0; i < m_Owner.MenuItems.GetCount();++i)
	{
		MenuScreenId& curId = m_Owner.MenuItems[i].MenuUniqueId;
		SUIContexts::SetActive(curId.GetParserName(), curId == iNewLayout);
	}
	
	bool bOnXBHelp = iNewLayout == MENU_UNIQUE_ID_HELP;
	if(bOnXBHelp)
	{
		if(iPreviousLayout == MENU_UNIQUE_ID_HOME_NEWSWIRE)
		{
			ClearNewswireStory();
		}

		Populate(iNewLayout);
	}

	SUIContexts::SetActive("SHOW_ACCEPTBUTTON_INFO", bOnXBHelp);
}

void CInfoMenu::UpdateFeedItem(stStreamedFeedItems& item)
{
	switch( item.Status )
	{
	case GAMESTREAM_STATUS_NEW:
		{
			if( item.stItem.bDynamicTexture )
			{
				if(uiVerifyf(item.RequestTextureForDownload(ASSET.FileName(item.stItem.sCloudPath), item.stItem.sCloudPath), "Failed to start download of %s for notifications menu", item.stItem.sCloudPath.c_str()))
				{
					item.Status = GAMESTREAM_STATUS_DOWNLOADINGTEXTURE;
				}
				else
				{
					uiErrorf("Failed to start download of %s for notifications menu", item.stItem.sCloudPath.c_str());
					FreePost( item );
				}
			}
			else if (item.stItem.iType == GAMESTREAM_TYPE_CREW_TAG)
			{
				bool success = false;
				if (item.stItem.m_clanId != RL_INVALID_CLAN_ID)
				{
					//Check to see if the crew emblem is already available.
					NetworkCrewEmblemMgr &crewEmblemMgr = NETWORK_CLAN_INST.GetCrewEmblemMgr();
					EmblemDescriptor emblemDesc(EmblemDescriptor::EMBLEM_CLAN, (EmblemId)item.stItem.m_clanId, RL_CLAN_EMBLEM_SIZE_128);

					if(crewEmblemMgr.RequestEmblem(emblemDesc  ASSERT_ONLY(, "CInfoMenu")))
					{
						success = true;
						item.bClanEmblemRequested = true;
						item.Status = GAMESTREAM_STATUS_REQUESTING_CREW_EMBLEM;
					}
				}

				if (!success)
				{
					uiDebugf1("Failed to request crew emblem for %d", (int)item.stItem.m_clanId);
					FreePost(item);
				}

			}
			else
			{
				strLocalIndex iTxdId = g_TxdStore.FindSlot(item.stItem.sTXD);

				if(iTxdId != -1 && g_TxdStore.IsObjectInImage(iTxdId) && !g_TxdStore.HasObjectLoaded(iTxdId))
				{
					// Sometimes TXDs will get unloaded by actions such as load game and won't be ready here.
					g_TxdStore.StreamingRequest(iTxdId, (STRFLAG_FORCE_LOAD | STRFLAG_PRIORITY_LOAD | STRFLAG_DONTDELETE));
					CStreaming::SetDoNotDefrag(iTxdId, g_TxdStore.GetStreamingModuleId());
					item.Status = GAMESTREAM_STATUS_STREAMING_TEXTURE;
				}
				else
				{
					item.Status = GAMESTREAM_STATUS_READYTOSHOW;
				}
			}
		}
		break;

	case GAMESTREAM_STATUS_STREAMING_TEXTURE:
		{
			strLocalIndex iTxdId = g_TxdStore.FindSlot(item.stItem.sTXD);

			if (g_TxdStore.HasObjectLoaded(iTxdId))
			{
				g_TxdStore.AddRef(iTxdId, REF_OTHER);
				g_TxdStore.PushCurrentTxd();
				g_TxdStore.SetCurrentTxd(iTxdId);
				g_TxdStore.PopCurrentTxd();

				item.iTextureDictionarySlot = iTxdId;

				item.Status = GAMESTREAM_STATUS_READYTOSHOW;
			}
			break;
		}

	case GAMESTREAM_STATUS_DOWNLOADINGTEXTURE:
		{
			//This return true if something happened.
			//False means it's still waiting.
			if (item.UpdateTextureDownloadStatus())
			{
				//If it succeeded, we'll have a valid TXD slot.
				if (item.iTextureDictionarySlot.Get() >= 0)
				{
					item.Status = GAMESTREAM_STATUS_READYTOSHOW;
				}
				else
				{
					uiErrorf("Failed to download texture %s for notifications feed",  item.stItem.sCloudPath.c_str()); 
					FreePost(item);
				}
			}
		}
		break;

	case GAMESTREAM_STATUS_REQUESTING_CREW_EMBLEM:
		{	
			bool bRequestValid = item.bClanEmblemRequested && item.stItem.m_clanId != RL_INVALID_CLAN_ID;
			if(bRequestValid)
			{
				NetworkCrewEmblemMgr &crewEmblemMgr =NETWORK_CLAN_INST.GetCrewEmblemMgr();
				EmblemDescriptor emblemDesc(EmblemDescriptor::EMBLEM_CLAN, (EmblemId)item.stItem.m_clanId, RL_CLAN_EMBLEM_SIZE_128);

				if(!crewEmblemMgr.RequestPending(emblemDesc))
				{
					if(crewEmblemMgr.RequestSucceeded(emblemDesc))
					{
						const char* crewEmblemTxd = crewEmblemMgr.GetEmblemName(emblemDesc);
						if (crewEmblemTxd && strlen(crewEmblemTxd) > 0)
						{
							// THIS MAY REQUIRE MORE CODE
							item.stItem.sTXD = crewEmblemTxd;
							item.stItem.sTXN = crewEmblemTxd;
							item.Status = GAMESTREAM_STATUS_READYTOSHOW;
						}
						else
						{
							bRequestValid = false;
						}
					}
					else
					{
						uiErrorf("Request for crew emblem for %d failed", (int)item.stItem.m_clanId);
						bRequestValid = false;
					}
				}
			}

			if (!bRequestValid)
			{
				uiDebugf1("Failed to update crew embelm update");
				FreePost(item);
			}
		}
		break;

	default:
		break;
	}
}

void CInfoMenu::SetupStreamingParams(stStreamedFeedItems& item)
{
	item.Status = GAMESTREAM_STATUS_NEW;
}

void CInfoMenu::FreePost(stStreamedFeedItems& item)
{
	item.ReleaseAndCancelTexture();
	item.Reset();
}

bool CInfoMenu::stStreamedFeedItems::RequestTextureForDownload( const char* textureName, const char* cloudFilePath )
{
	uiAssert(stItem.bDynamicTexture);

	requestDesc.m_Type = CTextureDownloadRequestDesc::TITLE_FILE;
	requestDesc.m_GamerIndex = 0;
	requestDesc.m_CloudFilePath = cloudFilePath;
	requestDesc.m_BufferPresize = 22 *1024;  //arbitrary pre size of 22k
	requestDesc.m_TxtAndTextureHash = textureName;
	requestDesc.m_CloudOnlineService = RL_CLOUD_ONLINE_SERVICE_NATIVE;

	CTextureDownloadRequest::Status retVal = CTextureDownloadRequest::REQUEST_FAILED;
	retVal = DOWNLOADABLETEXTUREMGR.RequestTextureDownload(requestHandle, requestDesc);

	iTextureDictionarySlot = -1;

	return retVal == CTextureDownloadRequest::IN_PROGRESS || retVal == CTextureDownloadRequest::READY_FOR_USER;
}

//  Returns true if something needs to be dealt with.
bool CInfoMenu::stStreamedFeedItems::UpdateTextureDownloadStatus()
{
	if (requestHandle == CTextureDownloadRequest::INVALID_HANDLE || !stItem.bDynamicTexture)
	{
		return true;
	}

	if (DOWNLOADABLETEXTUREMGR.HasRequestFailed(requestHandle))
	{
		//Texture request failed 
		//@TODO 
		// Set to cleanup?
		return true;
	}
	else if ( DOWNLOADABLETEXTUREMGR.IsRequestReady( requestHandle ) )
	{
		//Update the slot that the texture was slammed into
		strLocalIndex txdSlot = strLocalIndex(DOWNLOADABLETEXTUREMGR.GetRequestTxdSlot(requestHandle));
		if (uiVerifyf(g_TxdStore.IsValidSlot(txdSlot), "CGameStream::gstPost::UpdateTextureDownloadStatus- failed to get valid txd with name %s from DTM at slot %d", requestDesc.m_TxtAndTextureHash.GetCStr(), txdSlot.Get()))
		{
			g_TxdStore.AddRef(txdSlot, REF_OTHER);
			iTextureDictionarySlot = txdSlot;
			uiDebugf1("Downloaded texture for gamestream item has been received in TXD slot %d", txdSlot.Get());
		}

		//Tell the DTM that it can release it's hold on the TXD (since we've taken over the Reference to keep it in memory.
		DOWNLOADABLETEXTUREMGR.ReleaseTextureDownloadRequest( requestHandle );
		requestHandle = CTextureDownloadRequest::INVALID_HANDLE;

		return true;
	}

	return false;
}

void CInfoMenu::stStreamedFeedItems::ReleaseAndCancelTexture()
{
	if (iTextureDictionarySlot.Get() >= 0)
	{
		g_TxdStore.RemoveRef(iTextureDictionarySlot, REF_OTHER);
		iTextureDictionarySlot = strLocalIndex(-1);
	}

	if (requestHandle != CTextureDownloadRequest::INVALID_HANDLE)
	{
		DOWNLOADABLETEXTUREMGR.ReleaseTextureDownloadRequest( requestHandle );
		requestHandle = CTextureDownloadRequest::INVALID_HANDLE;
	}

	if (bClanEmblemRequested)
	{
		EmblemDescriptor emblemDesc(EmblemDescriptor::EMBLEM_CLAN, (EmblemId)stItem.m_clanId, RL_CLAN_EMBLEM_SIZE_128);
		NETWORK_CLAN_INST.GetCrewEmblemMgr().ReleaseEmblem(emblemDesc  ASSERT_ONLY(, "CInfoMenu"));
		bClanEmblemRequested = false;
		stItem.m_clanId = RL_INVALID_CLAN_ID;
	}
}
