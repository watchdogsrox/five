
// game
#include "Frontend/StatsMenu.h"

#include "Frontend/FrontendStatsMgr.h"
#include "frontend/MousePointer.h"
#include "Frontend/PauseMenu.h"
#include "Frontend/ScaleformMenuHelper.h"
#include "Frontend/Scaleform/ScaleFormMgr.h"
#include "Network/NetworkInterface.h"
#include "Stats/StatsMgr.h"
#include "Stats/StatsUtils.h"
#include "Frontend/ButtonEnum.h"
#include "Frontend/ui_channel.h"
#include "Frontend/NewHud.h"
#include "Script/script_hud.h"

//OPTIMISATIONS_OFF()
FRONTEND_OPTIMISATIONS()

#define INVAILD_STATS_CATEGORY -1
#define MAX_VISIBLE_STATS 16
#define CHECKLIST_CATEGORY 7
#define SP_STATS_CATEGORY 0
#define SKILL_SWITCH_THRESHOLD 10000

CStatsMenu::CStatsMenu(CMenuScreen& owner) 
	: CMenuBase(owner)
	, m_statsCategoryIndex(INVAILD_STATS_CATEGORY)
	, m_statsPlayer(0)
	, m_iReturnIndex(-1)
	, m_iPreviousSubMenu(MENU_UNIQUE_ID_INVALID)
	, m_iStatOffset(0)
	, m_iTotalStatsInCategory(0)
	, m_bShowPlayerColumn(false)
	, m_bForcePlayerColumn(false)
	, m_bOnChecklistPage(false)
	, m_bOnSPSkillsPage(false)
	, m_uSkillStatIndex(0)
	, m_uTimeSinceLastSkillSwitch(0)
	, m_iMouseScrollDirection(SCROLL_CLICK_NONE)
{
}

CStatsMenu::~CStatsMenu()
{
}

void CStatsMenu::Update()
{
	CFrontendStatsMgr::Update();

	if(m_bOnSPSkillsPage)
	{
		if(sysTimer::GetSystemMsTime() - m_uTimeSinceLastSkillSwitch >= SKILL_SWITCH_THRESHOLD)
		{
			m_uSkillStatIndex++;
			m_uTimeSinceLastSkillSwitch = sysTimer::GetSystemMsTime();

			int iIndex = m_uSkillStatIndex % CFrontendStatsMgr::GetSkillsStats().GetCount();
			u32 uDetailsHash = CFrontendStatsMgr::GetSkillsStats()[iIndex]->GetDetailsHash(m_statsPlayer);
			CPauseMenu::GetMovieWrapper(PAUSE_MENU_MOVIE_CONTENT).CallMethod("SET_DESCRIPTION", 1, TheText.Get(uDetailsHash, "DETAILS_HASH"));
		}
	}
}

bool CStatsMenu::CheckIncomingFunctions(atHashWithStringBank methodName, const GFxValue* args)
{
	if (methodName == ATSTRINGHASH("HANDLE_SCROLL_CLICK", 0x9CE8ECE9))
	{
		m_iMouseScrollDirection = (int)args[1].GetNumber();
		return true;
	}

	return false;
}

bool CStatsMenu::TriggerEvent(MenuScreenId MenuId, s32 UNUSED_PARAM(iUniqueId))
{
	if( MenuId == MENU_UNIQUE_ID_STATS_CATEGORY )
	{
		if(CPauseMenu::IsNavigatingContent() && m_bShowPlayerColumn && !IsMP())
		{
			// Determine which character is being played
			m_bForcePlayerColumn = false;

			CPauseMenu::PlaySound("NAV_LEFT_RIGHT");
			SUIContexts::Activate(ATSTRINGHASH("HACK_SKIP_SELECT_SOUND",0x9fb52674));

			m_statsPlayer = Wrap(m_statsPlayer+1, 0, STAT_SP_CATEGORY_MAX-1);
			UpdateCategory(m_statsCategoryIndex, m_statsPlayer);
			return true;
		}
	}

	return false;
}

bool CStatsMenu::UpdateInput(s32 sInput)
{
	if(CPauseMenu::IsNavigatingContent())
	{
		// Determine which character is being played
		m_bForcePlayerColumn = false;

		if(m_bShowPlayerColumn && !IsMP())
		{
			if (sInput == PAD_DPADLEFT)
			{
				CPauseMenu::PlaySound("NAV_LEFT_RIGHT");
				
				m_statsPlayer = Wrap(m_statsPlayer-1, 0, STAT_SP_CATEGORY_MAX-1);
				UpdateCategory(m_statsCategoryIndex, m_statsPlayer);
			}
			else if (sInput == PAD_DPADRIGHT)
			{
				CPauseMenu::PlaySound("NAV_LEFT_RIGHT");
				
				m_statsPlayer = Wrap(m_statsPlayer+1, 0, STAT_SP_CATEGORY_MAX-1);
				UpdateCategory(m_statsCategoryIndex, m_statsPlayer);
			}
		}
		if (SUIContexts::IsActive("STATS_CanScroll") && 
			(CPauseMenu::CheckInput(FRONTEND_INPUT_RUP) || CMousePointer::IsMouseWheelUp() || m_iMouseScrollDirection == SCROLL_CLICK_UP))
		{
			if(m_iStatOffset > 0)
			{
				m_iStatOffset--;
				UpdateCategory(m_statsCategoryIndex, m_statsPlayer, true);
				CPauseMenu::PlaySound("NAV_UP_DOWN");
			}
		}

		if (SUIContexts::IsActive("STATS_CanScroll") && 
			(CPauseMenu::CheckInput(FRONTEND_INPUT_RDOWN) || CMousePointer::IsMouseWheelDown() || m_iMouseScrollDirection == SCROLL_CLICK_DOWN))
		{
			if(m_iStatOffset < (m_iTotalStatsInCategory - MAX_VISIBLE_STATS))
			{
				m_iStatOffset++;
				UpdateCategory(m_statsCategoryIndex, m_statsPlayer, true);
				CPauseMenu::PlaySound("NAV_UP_DOWN");
			}
		}

		m_iMouseScrollDirection = SCROLL_CLICK_NONE;
	}
	else
	{
		if(!IsMP())
		{
			if(m_bShowPlayerColumn && !m_bForcePlayerColumn)
			{
				UpdateCategory(m_statsCategoryIndex, m_statsPlayer);
			}

			if(!m_bForcePlayerColumn && CPauseMenu::CheckInput(FRONTEND_INPUT_ACCEPT))
			{
				m_bForcePlayerColumn = true;
				UpdateCategory(m_statsCategoryIndex, m_statsPlayer);
			}
		}
	}

	return false;
}

bool CStatsMenu::Populate(MenuScreenId newScreenId)
{
	if(newScreenId.GetValue() != MENU_UNIQUE_ID_STATS)
	{
		return false;
	}

	m_iStatOffset = 0;
	m_iTotalStatsInCategory = 0;
	m_iCategoryOffset = -1;
	m_statsPlayer = CPauseMenu::GetCurrentCharacter();

	m_bOnChecklistPage = false;
	m_bOnSPSkillsPage = false;
	CScaleformMenuHelper::SET_DATA_SLOT_EMPTY(PM_COLUMN_MIDDLE);
	CScaleformMenuHelper::SET_DATA_SLOT_EMPTY(PM_COLUMN_RIGHT);

	CScaleformMenuHelper::SHOW_COLUMN(PM_COLUMN_MIDDLE, true);
	CScaleformMenuHelper::SHOW_COLUMN(PM_COLUMN_RIGHT, false);

	int iCount = 0;
	int iCategoryOffset = 0;
	int iFirstCategory = -1;

	for(CFrontendStatsMgr::UICategoryList::const_iterator iter = CFrontendStatsMgr::CategoryConstIterBegin(); iter != CFrontendStatsMgr::CategoryConstIterEnd(); iter++)
	{
		const sUICategoryData* pCategory = *iter;
		if( IsMP() != pCategory->m_bSinglePlayer )
		{
			if (iFirstCategory == -1)
				iFirstCategory = iCount;

			if(m_iCategoryOffset == -1)
				m_iCategoryOffset = iCategoryOffset;

			CScaleformMenuHelper::SET_DATA_SLOT(PM_COLUMN_LEFT, iCount, PREF_OPTIONS_THRESHOLD + MENU_UNIQUE_ID_STATS_CATEGORY, iCount, OPTION_DISPLAY_NONE, 0, true, TheText.Get(pCategory->m_Name));
			++iCount;
		}
		++iCategoryOffset;
	}

	if(IsMP() && !CScriptHud::bUsingMissionCreator)
	{
		SetMenuceptableSlot(iCount++,	MENU_UNIQUE_ID_WEAPONS,		"PM_WEAPONS");
		SetMenuceptableSlot(iCount++,	MENU_UNIQUE_ID_MEDALS,		"PM_AWARDS");
		SetMenuceptableSlot(iCount,		MENU_UNIQUE_ID_PARTY_LIST,	"PM_INF_UNLT");
	}

	if(m_statsCategoryIndex != INVAILD_STATS_CATEGORY)
	{
		return true;
	}

	if(m_iReturnIndex == -1)
	{
		m_bForcePlayerColumn = !IsMP();
		UpdateCategory(iFirstCategory, m_statsPlayer);
	}
	else
	{
		CScaleformMenuHelper::SET_COLUMN_HIGHLIGHT(PM_COLUMN_LEFT, m_iReturnIndex);
		UpdateMiscInfo(m_iPreviousSubMenu);
	}

	// Pre-cache for async stats
	if(IsMP())
	{
		CFrontendStatsMgr::UIStatsList::const_iterator iter = CFrontendStatsMgr::MPStatsUiConstIterBegin();
		CFrontendStatsMgr::UIStatsList::const_iterator endIter = endIter = CFrontendStatsMgr::MPStatsUiConstIterEnd();
		const CStatsDataMgr& statsDataMgr = CStatsMgr::GetStatsDataMgr();
		const u32 c_stringSize = 128;
		char szString[c_stringSize];
		for(; iter != endIter ;iter++)
		{
			const sUIStatData* pStat = (*iter);
			const sStatDataPtr* pActualStat = statsDataMgr.GetStatPtr(pStat->GetStatHash(m_statsPlayer));

			if(pStat->GetType() == eStatType_PlayerID)
			{
				CFrontendStatsMgr::GetDataAsString(pActualStat, pStat, szString, c_stringSize);
			}
		}
#if RSG_DURANGO
		CFrontendStatsMgr::RequestDisplayNameLookup();
#elif RSG_ORBIS
		if (rlGamerHandle::IsUsingAccountIdAsKey())
		{
			CFrontendStatsMgr::RequestDisplayNameLookup();
		}
#endif // RSG_DURANGO
	}

	return true;
}

void CStatsMenu::SetMenuceptableSlot(int iSlotIndex, eMenuScreen iMenuID, const char* locKey)
{
	m_menuIDToSlotIndex[iMenuID] = iSlotIndex;
	CScaleformMenuHelper::SET_DATA_SLOT(PM_COLUMN_LEFT, iSlotIndex, PREF_OPTIONS_THRESHOLD + MENU_UNIQUE_ID_INCEPT_TRIGGER, iMenuID, OPTION_DISPLAY_NONE, 0, true, TheText.Get(locKey));
}

void CStatsMenu::LayoutChanged( MenuScreenId OUTPUT_ONLY(iPreviousLayout), MenuScreenId iNewLayout, s32 iUniqueId, eLAYOUT_CHANGED_DIR UNUSED_PARAM(eDir) )
{
	uiDebugf3("CStatsMenu::LayoutChanged - Prev: %i, New: %i, Unique: %i", iPreviousLayout.GetValue(), iNewLayout.GetValue(), iUniqueId);
	// We've moved to a menuceptable tab OR we've backed out from a menuceptable menu.  This is all because when you back out of menuception the iNewLayout becomes MENU_UNIQUE_ID_STATS instead of MENU_UNIQUE_ID_INCEPT_TRIGGER
	SUIContexts::SetActive("STATS_CanSelect", iNewLayout.GetValue() == MENU_UNIQUE_ID_INCEPT_TRIGGER || (iNewLayout.GetValue() == MENU_UNIQUE_ID_STATS && m_iPreviousSubMenu != MENU_UNIQUE_ID_INVALID));

	switch(iNewLayout.GetValue())
	{
	case MENU_UNIQUE_ID_STATS_CATEGORY:
		m_iStatOffset = 0;
		m_iReturnIndex = -1;
		m_iPreviousSubMenu = MENU_UNIQUE_ID_INVALID;
		UpdateCategory(iUniqueId, m_statsPlayer);
		break;
	case MENU_UNIQUE_ID_INCEPT_TRIGGER:
		m_iPreviousSubMenu = static_cast<eMenuScreen>(iUniqueId);
		UpdateMiscInfo(m_iPreviousSubMenu);
		break;
	case MENU_UNIQUE_ID_MEDALS:
	case MENU_UNIQUE_ID_WEAPONS:
	case MENU_UNIQUE_ID_PARTY_LIST:
		HIDE_WARNING_MESSAGE_EX(PM_COLUMN_MIDDLE_RIGHT);
		break;
	case MENU_UNIQUE_ID_STATS:
	case MENU_UNIQUE_ID_STATS_LISTITEM:
	default:
		break;
	};
}

void CStatsMenu::LoseFocus()
{
	uiDebugf3("CStatsMenu::LoseFocus");
	m_statsCategoryIndex = INVAILD_STATS_CATEGORY;
	m_statsPlayer = 0;

#if RSG_DURANGO
	CFrontendStatsMgr::CancelDisplayNameLookup();
#endif // RSG_DURANGO
}

void CStatsMenu::UpdateMiscInfo(eMenuScreen iMenuID)
{
	CPauseMenu::UnlockMenuControl();
	CScaleformMenuHelper::SHOW_COLUMN(PM_COLUMN_MIDDLE, false);
	SUIContexts::Deactivate("STATS_CanScroll");

	m_iReturnIndex = m_menuIDToSlotIndex[iMenuID];

	switch(iMenuID)
	{
	case MENU_UNIQUE_ID_WEAPONS:
		ShowColumnWarning_EX(PM_COLUMN_MIDDLE_RIGHT, "PM_INF_WEPB", "PM_INF_WEPT");
		break;
	case MENU_UNIQUE_ID_MEDALS:
		ShowColumnWarning_EX(PM_COLUMN_MIDDLE_RIGHT, "PM_INF_AWDB", "PM_INF_AWDT");
		break;
	case MENU_UNIQUE_ID_PARTY_LIST:
		ShowColumnWarning_EX(PM_COLUMN_MIDDLE_RIGHT, "PM_INF_UNLB", "PM_INF_UNLT");
		break;
	default:
		m_iReturnIndex = -1;
		break;
	}
}

void CStatsMenu::UpdateCategory(int iCategory, int iForcedCharacter, bool bScrolling)
{
	CPauseMenu::UnlockMenuControl();

	int iCount = 0;
	int iCharacter;

	CScaleformMenuHelper::SET_DATA_SLOT_EMPTY(PM_COLUMN_MIDDLE);
	CScaleformMenuHelper::SET_DATA_SLOT_EMPTY(PM_COLUMN_RIGHT);

	if(CPauseMenu::IsNavigatingContent())
	{
		if(IsMP())
		{
			m_bShowPlayerColumn = false;
			m_bOnChecklistPage = false;
			m_bOnSPSkillsPage = false;
			ClearWarningColumn_EX(PM_COLUMN_MIDDLE_RIGHT);
			CScaleformMenuHelper::SHOW_COLUMN(PM_COLUMN_MIDDLE, true);
			CScaleformMenuHelper::SHOW_COLUMN(PM_COLUMN_RIGHT, false);
		}
		else
		{
			if(iCategory == CHECKLIST_CATEGORY ) // 100% Checklist
			{
				m_bShowPlayerColumn = false;
				m_bOnChecklistPage = true;
				m_bOnSPSkillsPage = false;
				CScaleformMenuHelper::SHOW_COLUMN(PM_COLUMN_MIDDLE, false);
				CScaleformMenuHelper::SHOW_COLUMN(PM_COLUMN_RIGHT, true);
			}
			else
			{
				m_bShowPlayerColumn = true;
				m_bOnChecklistPage = false;
				CScaleformMenuHelper::SHOW_COLUMN(PM_COLUMN_MIDDLE, true);
				CScaleformMenuHelper::SHOW_COLUMN(PM_COLUMN_RIGHT, false);

				if(!m_bOnSPSkillsPage)
				{
					m_bOnSPSkillsPage = !IsMP() && iCategory == SP_STATS_CATEGORY;
					m_uTimeSinceLastSkillSwitch = sysTimer::GetSystemMsTime();
					m_uSkillStatIndex = 0;

					u32 uDetailsHash = CFrontendStatsMgr::GetSkillsStats()[0]->GetDetailsHash(m_statsPlayer);
					CPauseMenu::GetMovieWrapper(PAUSE_MENU_MOVIE_CONTENT).CallMethod("SET_DESCRIPTION", 1, TheText.Get(uDetailsHash, "DETAILS_HASH"));
				}
				else
				{
					m_bOnSPSkillsPage = !IsMP() && iCategory == SP_STATS_CATEGORY;
				}
			}
		}
	}
	else if(!m_bForcePlayerColumn && !CPauseMenu::IsNavigatingContent())
	{
		m_bShowPlayerColumn = false;
		m_bOnSPSkillsPage = false;
	}
	else
	{
		m_bShowPlayerColumn = true;
		if(!m_bOnSPSkillsPage)
		{
			m_bOnSPSkillsPage = !IsMP() && iCategory == SP_STATS_CATEGORY;
			m_uTimeSinceLastSkillSwitch = sysTimer::GetSystemMsTime();
			m_uSkillStatIndex = 0;

			u32 uDetailsHash = CFrontendStatsMgr::GetSkillsStats()[0]->GetDetailsHash(m_statsPlayer);
			CPauseMenu::GetMovieWrapper(PAUSE_MENU_MOVIE_CONTENT).CallMethod("SET_DESCRIPTION", 1, TheText.Get(uDetailsHash, "DETAILS_HASH"));
		}
		else
		{
			m_bOnSPSkillsPage = !IsMP() && iCategory == SP_STATS_CATEGORY;
		}
	}

	if(m_bForcePlayerColumn)
	{
		if(iCategory == 9 ) // 100% Checklist
		{
			m_bForcePlayerColumn = false;
		}
	}

	SUIContexts::SetActive("STATS_PlayerSwitch", m_bShowPlayerColumn || m_bForcePlayerColumn);

	m_statsCategoryIndex = iCategory;
	iCharacter =  PopulateColumnTitleByPlayerCharacter(iForcedCharacter);
	m_iTotalStatsInCategory = GetTotalStatsThisCategory(iCharacter);

	CFrontendStatsMgr::UIStatsList::const_iterator iter;
	CFrontendStatsMgr::UIStatsList::const_iterator endIter;

	if (IsMP())
	{
		iter = CFrontendStatsMgr::MPStatsUiConstIterBegin();
		endIter = CFrontendStatsMgr::MPStatsUiConstIterEnd();
	}
	else
	{
		iter = CFrontendStatsMgr::SPStatsUiConstIterBegin();
		endIter = CFrontendStatsMgr::SPStatsUiConstIterEnd();
	}

	const CStatsDataMgr& statsDataMgr = CStatsMgr::GetStatsDataMgr();
	const u32 c_stringSize = 128;
	char szString[c_stringSize];
	int iOffsetter = 0;
	const bool countOnlyKillsInInstancedContent = CStatsMgr::CountKillsOnlyInInstancedContent();

	for(iCount = m_bOnChecklistPage ? 1 : 0;iter != endIter && iCount < MAX_VISIBLE_STATS ;iter++)
	{
		const sUIStatData* pStat = (*iter);
		const sStatDataPtr* pActualStat = statsDataMgr.GetStatPtr(pStat->GetStatHash(iCharacter));
		char szStatLocName[64];
		::sprintf(szStatLocName,"%s","tempToFixBuild");


		if (pStat->GetCategory() == (u8)iCategory && pActualStat &&
			CFrontendStatsMgr::GetVisibilityFromRule(pStat, pActualStat))
		{
			if(iOffsetter != m_iStatOffset)
			{
				iOffsetter++;
				continue;
			}

			CFrontendStatsMgr::GetDataAsString(pActualStat, pStat, szString, c_stringSize);

			char* gxt = 0;
			int descHash = pStat->GetDescriptionHash();
			if(countOnlyKillsInInstancedContent)
			{
				if (pStat->GetBaseHash() == (int) STAT_MP_DEATHS_PLAYER.GetHash())
				{
					descHash = ATSTRINGHASH("MPPLY_DEATHS_PLAYERS_INSTANCED", 0xECE01647);
				}
				if (pStat->GetBaseHash() == (int)STAT_MP_KILLS_PLAYERS.GetHash())
				{
					descHash = ATSTRINGHASH("MPPLY_KILLS_PLAYERS_INSTANCED", 0x815341BB);
				}
				if (pStat->GetBaseHash() == (int)STAT_MPPLY_KILL_DEATH_RATIO.GetHash())
				{
					descHash = ATSTRINGHASH("MPPLY_KILL_DEATH_RATIO_INSTANCED", 0xE955CBC9);
				}
			}
			gxt = TheText.Get(descHash, "UNKNOWN");


			if(m_bOnChecklistPage)
			{
				if( CScaleformMenuHelper::SET_DATA_SLOT(PM_COLUMN_RIGHT, iCount, PREF_OPTIONS_THRESHOLD + MENU_UNIQUE_ID_STATS_LISTITEM, iCount , OPTION_DISPLAY_NONE, 0, true, gxt, false, false  ))
				{
					float percentage = CFrontendStatsMgr::GetDataAsPercent(pActualStat, pStat);
					CScaleformMgr::AddParamString(szString);
					CScaleformMgr::AddParamFloat(percentage);
					CScaleformMgr::AddParamInt(GetChecklistColor(iCount));
					CScaleformMgr::EndMethod();

					iCount++;
				}
			}
			else
			{
				if(pStat->GetType() == eStatType_Bar)
				{
					if( CScaleformMenuHelper::SET_DATA_SLOT(PM_COLUMN_MIDDLE, iCount, PREF_OPTIONS_THRESHOLD + MENU_UNIQUE_ID_STATS_LISTITEM, iCount, OPTION_DISPLAY_STYLE_TEXTFIELD,0,true,gxt,false) )
					{
						CScaleformMgr::AddParamInt(StatsInterface::GetIntStat(pActualStat->GetKey())); 
						CScaleformMgr::AddParamInt(CNewHud::GetCharacterColour(iCharacter));
						CScaleformMgr::EndMethod();

						iCount++;
					} 	
				}
				else
				{
					if( CScaleformMenuHelper::SET_DATA_SLOT(PM_COLUMN_MIDDLE, iCount, PREF_OPTIONS_THRESHOLD + MENU_UNIQUE_ID_STATS_LISTITEM, iCount , OPTION_DISPLAY_NONE, 0, true, gxt, false, bScrolling ))
					{
						CScaleformMgr::AddParamString(szString); 
						CScaleformMgr::EndMethod();

						iCount++;
					}
				}
			}
		}
	}

	if(m_bOnChecklistPage)
	{
		CScaleformMovieWrapper& pauseContent = CPauseMenu::GetMovieWrapper(PAUSE_MENU_MOVIE_CONTENT);
		if( pauseContent.BeginMethod("SET_DATA_SLOT") )
		{
			char totalPercentString[8];
			sprintf(totalPercentString, "%d%%", (int)CStatsMgr::GetPercentageProgress());

			const u32 c_nameStringSize = 128;
			char michaelTime[c_nameStringSize];
			char franklinTime[c_nameStringSize];
			char trevorTime[c_nameStringSize];
			u64 uMichaelTime = StatsInterface::GetUInt64Stat(STAT_TOTAL_PLAYING_TIME.GetHash(CHAR_MICHEAL));
			u64 uFranklinTime = StatsInterface::GetUInt64Stat(STAT_TOTAL_PLAYING_TIME.GetHash(CHAR_FRANKLIN));
			u64 uTrevorTime = StatsInterface::GetUInt64Stat(STAT_TOTAL_PLAYING_TIME.GetHash(CHAR_TREVOR));

			CStatsUtils::GetTime(uMichaelTime, michaelTime, c_nameStringSize, true);
			CStatsUtils::GetTime(uFranklinTime, franklinTime, c_nameStringSize, true);
			CStatsUtils::GetTime(uTrevorTime, trevorTime, c_nameStringSize, true);

			pauseContent.AddParam(PM_COLUMN_RIGHT);
			pauseContent.AddParam(0);
			pauseContent.AddParam(PREF_OPTIONS_THRESHOLD + MENU_UNIQUE_ID_STATS_LISTITEM);
			pauseContent.AddParam(totalPercentString);
			pauseContent.AddParamString(TheText.Get("BLIP_MICHAEL"));
			pauseContent.AddParamString(TheText.Get("BLIP_FRANKLIN"));
			pauseContent.AddParamString(TheText.Get("BLIP_TREV"));
			pauseContent.AddParamString(michaelTime);
			pauseContent.AddParamString(franklinTime);
			pauseContent.AddParamString(trevorTime);
			pauseContent.EndMethod();
		}
	}


	if (iForcedCharacter == -1 || !m_bShowPlayerColumn)
	{
		CPauseMenu::GetMovieWrapper(PAUSE_MENU_MOVIE_CONTENT).CallMethod("SET_COLUMN_HIGHLIGHT", 1, 0);
	}
	CScaleformMenuHelper::DISPLAY_DATA_SLOT(m_bOnChecklistPage ? PM_COLUMN_RIGHT : PM_COLUMN_MIDDLE);

	if(m_bOnSPSkillsPage)
	{
		int iIndex = m_uSkillStatIndex % CFrontendStatsMgr::GetSkillsStats().GetCount();
		u32 uDetailsHash = CFrontendStatsMgr::GetSkillsStats()[iIndex]->GetDetailsHash(iCharacter);
		CPauseMenu::GetMovieWrapper(PAUSE_MENU_MOVIE_CONTENT).CallMethod("SET_DESCRIPTION", 1, TheText.Get(uDetailsHash, "DETAILS_HASH"));
	}
	else
	{
		CPauseMenu::GetMovieWrapper(PAUSE_MENU_MOVIE_CONTENT).CallMethod("SET_DESCRIPTION", 1, "");
	}

	OnNavigatingContent(CPauseMenu::IsNavigatingContent());
}

int CStatsMenu::GetChecklistColor(int iCurrentItem)
{
	switch(iCurrentItem)
	{
	case 1:
		return HUD_COLOUR_YELLOW;
	case 2:
		return HUD_COLOUR_GREEN;
	case 3:
		return HUD_COLOUR_BLUE;
	case 4:
		return HUD_COLOUR_ORANGE;
	case 5:
		return HUD_COLOUR_RED;
	case 6:
		return HUD_COLOUR_PURPLE;
	default:
		return HUD_COLOUR_WHITE;
	}
}

void CStatsMenu::OnNavigatingContent(bool bAreWe)
{
	// technically the scroll column exists on the left column
	if( bAreWe && m_iTotalStatsInCategory > MAX_VISIBLE_STATS )
	{
		CScaleformMenuHelper::SET_COLUMN_SCROLL(PM_COLUMN_MIDDLE, m_iStatOffset, m_iTotalStatsInCategory+1);
		SUIContexts::Activate("STATS_CanScroll");
	}
	else
	{
		CScaleformMenuHelper::HIDE_COLUMN_SCROLL(PM_COLUMN_MIDDLE);
		SUIContexts::Deactivate("STATS_CanScroll");
	}
}

bool CStatsMenu::InitScrollbar()
{
	if( CPauseMenu::IsScreenDataValid(MENU_UNIQUE_ID_STATS_LISTITEM) )
	{
		CMenuScreen& rData = CPauseMenu::GetScreenData(MENU_UNIQUE_ID_STATS_LISTITEM);
		rData.INIT_SCROLL_BAR();
	}
	return false;
}

int CStatsMenu::PopulateColumnTitleByPlayerCharacter(int iCharacter /* = -1 */ )
{
	if(iCharacter == -1)
	{
		iCharacter = CPauseMenu::GetCurrentCharacter();
	}

	const char* pCharacterName = NULL;
	const char* characterNames[] = {
		TheText.Get("BLIP_MICHAEL"),
		TheText.Get("BLIP_FRANKLIN"),
		TheText.Get("BLIP_TREV")
	};

	CompileTimeAssert(NELEM(characterNames) == (CHAR_SP_END - CHAR_SP_START + 1));
	pCharacterName = characterNames[iCharacter];

	if((pCharacterName && !NetworkInterface::IsGameInProgress() && m_bShowPlayerColumn) || m_bForcePlayerColumn)
	{
		int iCount = 0;
		for(CFrontendStatsMgr::UICategoryList::const_iterator iter = CFrontendStatsMgr::CategoryConstIterBegin(); iter != CFrontendStatsMgr::CategoryConstIterEnd(); iter++)
		{
			const sUICategoryData* pCategory = *iter;
			if( IsMP() != pCategory->m_bSinglePlayer  && iCount != CHECKLIST_CATEGORY)
			{
				if( CScaleformMenuHelper::SET_DATA_SLOT(PM_COLUMN_LEFT, iCount, PREF_OPTIONS_THRESHOLD + MENU_UNIQUE_ID_STATS_CATEGORY, iCount, OPTION_DISPLAY_STYLE_TEXTFIELD,0,true,TheText.Get(pCategory->m_Name),false,true) )
				{
					CScaleformMgr::AddParamString(pCharacterName);
					CScaleformMgr::EndMethod();
				}
				++iCount;
			}
		}
	}
	else
	{
		CPauseMenu::GetMovieWrapper(PAUSE_MENU_MOVIE_CONTENT).CallMethod("SET_COLUMN_TITLE", 1, "", "");
		CPauseMenu::GetMovieWrapper(PAUSE_MENU_MOVIE_CONTENT).CallMethod("SET_COLUMN_HIGHLIGHT", 1, 0);
	}

	return iCharacter;
}

int CStatsMenu::GetTotalStatsThisCategory(int iCharacter)
{
	int iCount = 0;
	CFrontendStatsMgr::UIStatsList::const_iterator iter;
	CFrontendStatsMgr::UIStatsList::const_iterator endIter;

	if (IsMP())
	{
		iter = CFrontendStatsMgr::MPStatsUiConstIterBegin();
		endIter = CFrontendStatsMgr::MPStatsUiConstIterEnd();
	}
	else
	{
		iter = CFrontendStatsMgr::SPStatsUiConstIterBegin();
		endIter = CFrontendStatsMgr::SPStatsUiConstIterEnd();
	}

	const CStatsDataMgr& statsDataMgr = CStatsMgr::GetStatsDataMgr();
	for(;iter != endIter;iter++)
	{
		const sUIStatData* pStat = (*iter);
		if (pStat->GetCategory() == m_statsCategoryIndex)
		{
			const sStatDataPtr* pActualStat = statsDataMgr.GetStatPtr(pStat->GetStatHash(iCharacter));
			if(pActualStat && !CFrontendStatsMgr::GetVisibilityFromRule(pStat, pActualStat))
				continue;

			iCount++;
		}
	}

	return iCount;
}

bool CStatsMenu::IsMP()
{
	return !CPauseMenu::IsSP() || CScriptHud::bUsingMissionCreator;
}


// eof
