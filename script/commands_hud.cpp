//
// filename:	commands_hud.cpp
// description:	
//

// --- Include Files ------------------------------------------------------------

// C headers
// Rage headers
#include "script/array.h"
#include "script/wrapper.h"
#include "script/hash.h"
// Game headers
#include "camera/viewports/ViewportManager.h"
#include "control/replay/ReplayMovieControllerNew.h"
#include "Cutscene/CutSceneManagerNew.h"
#include "frontend/CFriendsMenu.h"
#include "frontend/NewHud.h"
#include "frontend/PauseMenu.h"
#include "frontend/PauseMenuData.h"
#include "frontend/CMapMenu.h"
#include "frontend/hud_colour.h"
#include "frontend/MultiplayerGamerTagHud.h"
#include "frontend/MiniMap.h"
#include "frontend/MiniMapMenuComponent.h"
#include "frontend/HudMarkerManager.h"
#if RSG_PC
#include "frontend/MousePointer.h"
#include "frontend/MultiplayerChat.h"
#include "frontend/TextInputBox.h"
#endif // RSG_PC
#include "frontend/WarningScreen.h"
#include "frontend/GameStream.h"
#include "frontend/GameStreamMgr.h"
#include "frontend/BusySpinner.h"
#include "Frontend/SocialClubMenu.h"
#include "Frontend/Store/StoreScreenMgr.h"
#include "frontend/ReportMenu.h"
#include "frontend/ui_channel.h"
#include "frontend/VideoEditor/ui/Editor.h"
#include "game/user.h"
#include "Network/Live/NetworkClan.h"
#include "Network/Live/PlayerCardDataManager.h"
#include "objects/object.h"
#include "peds/Ped.h"
#include "physics/WorldProbe/worldprobe.h"
#include "pickups/PickupManager.h"
#include "SaveLoad/savegame_frontend.h"
#include "scene/world/GameWorld.h"
#include "script/commands_player.h"
#include "script/Handlers/GameScriptResources.h"
#include "script/script.h"
#include "script/script_helper.h"
#include "script/script_text_construction.h"
#include "script/script_ai_blips.h"
#include "Stats/StatsInterface.h"
#include "Stats/MoneyInterface.h"
#include "text/messages.h"
#include "vehicles/vehicle.h"
#include "network/Live/livemanager.h"
#include "network/NetworkInterface.h"
#include "renderer/rendertargetmgr.h"
#include "control/gps.h"
#include "renderer/DrawLists/DrawListMgr.h"
#include "audio/radiostation.h"
#include "script/commands_graphics.h"
#include "renderer/PostProcessFX.h"
#include "text/TextConversion.h"
#include "text/Text.h"
#include "network/events/NetworkEventTypes.h"

//FRONTEND_SCRIPT_OPTIMISATIONS()

namespace hud_commands
{
	bool AreInstructionalButtonsActive()
	{
		return CNewHud::AreInstructionalButtonsActive();
	}

	void CommandSetScriptedMenuHeight(float MenuHeight)
	{
		CGameStreamMgr::GetGameStream()->SetScriptedMenuHeight(MenuHeight);
	}

	void CommandAutoPostGameTipsOn()
	{
		CGameStreamMgr::GetGameStream()->AutoPostGameTipOn();
	}

	void CommandAutoPostGameTipsOff()
	{
		CGameStreamMgr::GetGameStream()->AutoPostGameTipOff();
	}

	void CommandBeginBusySpinnerOn(const char *pMainTextLabel)
	{
		scriptDebugf1("BEGIN_TEXT_COMMAND_BUSYSPINNER_ON(%s) called", pMainTextLabel);
		CScriptTextConstruction::BeginBusySpinnerOn(pMainTextLabel);
	}

	void CommandEndBusySpinnerOn( int Icon )
	{
		scriptDebugf1("END_TEXT_COMMAND_BUSYSPINNER_ON(%d) called", Icon);
		CScriptTextConstruction::EndBusySpinnerOn(Icon);
	}

	void CommandBusySpinnerOff()
	{
		scriptDebugf1("BUSYSPINNER_OFF called");
		spinnerDisplayf("CBusySpinner::Off called on BUSYSPINNER_OFF ");
		CBusySpinner::Off( SPINNER_SOURCE_SCRIPT );
	}

	void CommandPreloadBusySpinner()
	{
		spinnerDisplayf("CBusySpinner::Preload() called on PRELOAD_BUSYSPINNER ");

		CBusySpinner::Preload();
	}

	bool CommandIsBusySpinnerOn()
	{
		return( CBusySpinner::IsOn() );
	}

	bool CommandIsBusySpinnerDisplaying()
	{
		return( CBusySpinner::IsDisplaying() );
	}

	void CommandDisablePauseMenuBusySpinner(bool WIN32PC_ONLY(b))
	{
		#if RSG_PC
		CPauseMenu::sm_bDisableSpinner = b;
		#endif
	}

	void CommandSetMouseCursorThisFrame()
	{
#if RSG_PC
		CMousePointer::SetMouseCursorThisFrame();
#endif
	}

	void CommandSetMouseCursorStyle(s32 iStyle)
	{
#if RSG_PC
		CMousePointer::SetMouseCursorStyle((eMOUSE_CURSOR_STYLE)iStyle);
#else
		(void)iStyle;
#endif
	}

	void CommandSetMouseCursorVisibility(bool bVisible)
	{
#if RSG_PC
		CMousePointer::SetMouseCursorVisible(bVisible);
#else
		(void)bVisible;
#endif
	}

	bool CommandIsMouseRolledOverInstructionalButtons()
	{
#if RSG_PC
		return CMousePointer::IsMouseRolledOverInstructionalButtons();
#else
		return false;
#endif // RSG_PC
	}

	bool CommandGetMouseEvent(int KEYBOARD_MOUSE_ONLY(iScriptMovieID), int& KEYBOARD_MOUSE_ONLY(evtType), int& KEYBOARD_MOUSE_ONLY(iUID), int& KEYBOARD_MOUSE_ONLY(iContext))
	{
#if RSG_PC
		if(scriptVerifyf(iScriptMovieID > 0 && iScriptMovieID <= NUM_SCRIPT_SCALEFORM_MOVIES, "GET_MOUSE_EVENT - Invalid Scaleform movie id (%d)", iScriptMovieID))
		{
			int iMovieID = CScriptHud::ScriptScaleformMovie[iScriptMovieID - 1].iId;

			const sScaleformMouseEvent evt = CMousePointer::GetLastMouseEvent();
			if(evt.iMovieID != SF_INVALID_MOVIE && 
				scriptVerifyf(CScaleformMgr::IsMovieActive(iMovieID), "GET_MOUSE_EVENT -- Attempting to check for mouse events on an inactive movie") && evt.iMovieID == iMovieID)
			{
				evtType = evt.evtType;
				iUID = evt.iUID;
				iContext = evt.iContext;

				return true;
			}
		}
#endif

		return false;
	}

	void CommandTheFeedOnlyShowToolTips(bool bOnlyToolTips)
	{
		if (bOnlyToolTips)
		{
			CGameStreamMgr::GetGameStream()->EnableAllFeedTypes(false);
			CGameStreamMgr::GetGameStream()->EnableFeedType(GAMESTREAM_TYPE_TOOLTIPS, true);
		}
		else
		{
			CGameStreamMgr::GetGameStream()->EnableAllFeedTypes(true);
		}

	}

	void CommandTheFeedHide( void )
	{
		CGameStreamMgr::GetGameStream()->Hide();
	}

	void CommandTheFeedHideThisFrame( void )
	{
		CGameStreamMgr::GetGameStream()->HideThisUpdate();
	}

	void CommandTheFeedShow( void )
	{
		CGameStreamMgr::GetGameStream()->Show();
	}

	void CommandTheFeedFlushQueue( void )
	{
		CGameStreamMgr::GetGameStream()->FlushQueue();
	}

	void CommandTheFeedRemoveItem( int Id )
	{
		CGameStreamMgr::GetGameStream()->RemoveItem( Id );
	}
	
	void CommandTheFeedForceRenderOn( void )
	{
		CGameStreamMgr::GetGameStream()->ForceRenderOn();
	}

	void CommandTheFeedForceRenderOff( void )
	{
		CGameStreamMgr::GetGameStream()->ForceRenderOff();
	}

	void CommandTheFeedPause( void )
	{
		CGameStreamMgr::GetGameStream()->Pause();
	}

	void CommandTheFeedResume( void )
	{
		CGameStreamMgr::GetGameStream()->Resume();
	}


	bool CommandTheFeedIsPaused( void )
	{
		return CGameStreamMgr::GetGameStream()->IsPaused();
	}


	void CommandTheFeedReportLogoOn( void )
	{
		CGameStreamMgr::GetGameStream()->ReportLogoIsOn();
	}

	void CommandTheFeedReportLogoOff( void )
	{
		CGameStreamMgr::GetGameStream()->ReportLogoIsOff();
	}

	void CommandTheFeedSetGamer1Info( const char* UNUSED_PARAM(Name), bool UNUSED_PARAM(IsPrivate), bool UNUSED_PARAM(ShowRockstarLogo), const char* UNUSED_PARAM(CrewString), int UNUSED_PARAM(Rank), bool UNUSED_PARAM(IsFounder) )
	{
		scriptAssertf(0, "Using deprecated command THEFEED_SET_GAMER1_INFO");
	}

	void CommandTheFeedSetGamer2Info( const char* UNUSED_PARAM(Name), bool UNUSED_PARAM(IsPrivate), bool UNUSED_PARAM(ShowRockstarLogo), const char* UNUSED_PARAM(CrewString), int UNUSED_PARAM(Rank), bool UNUSED_PARAM(IsFounder) )
	{
		scriptAssertf(0, "Using deprecated command THEFEED_SET_GAMER2_INFO");
	}

	int CommandGetLastShownPhoneActivatableFeedID()
	{
		return CGameStreamMgr::GetGameStream()->GetLastShownPhoneActivatableScriptID();
	}

    void CommandTheFeedSetRGBAParameter( int UNUSED_PARAM(red), int UNUSED_PARAM(green), int UNUSED_PARAM(blue), int UNUSED_PARAM(alpha) )
    {
    }

    void CommandTheFeedSetFlashDurationParameter( int UNUSED_PARAM(flashRate) )
    {
    }

	void CommandTheFeedResetRGBAFlashParameters()
	{
	}
	
	void CommandTheFeedSetBackgroundColorForNextPost( s32 color )
	{
		CGameStreamMgr::GetGameStream()->SetNextPostBackgroundColor( static_cast<eHUD_COLOURS>(color) );
	}

	void CommandTheFeedSetRGBAForNextMessage( int red, int green, int blue, int alpha )
	{
		CGameStreamMgr::GetGameStream()->SetImportantParamsRGBA(red,green,blue, (int)(alpha/255.0 * 100)); // Script sends values 0-255 
	}

	void CommandTheFeedSetFlashDurationForNextMessage( int flashrate )
	{
		CGameStreamMgr::GetGameStream()->SetImportantParamsFlashCount(flashrate);
	}

	void CommandTheFeedSetVibrateForNextMessage( bool vibrate )
	{
		CGameStreamMgr::GetGameStream()->SetImportantParamsVibrate(vibrate);
	}

	void CommandTheFeedResetAllParameters()
	{
		CGameStreamMgr::GetGameStream()->ResetImportantParams();
	}

	void CommandTheFeedFreezeNextPost()
	{
		CGameStreamMgr::GetGameStream()->FreezeNextPost();
	}

	void CommandTheFeedClearFrozenPost()
	{
		CGameStreamMgr::GetGameStream()->ClearFrozenPost();
	}

	void CommandTheFeedSetSnapFeedItemPositions(bool bSet)
	{
		CGameStreamMgr::GetGameStream()->SetSnapFeedItemPositions(bSet);
	}

	void CommandUpdateFeedItemTexture(const char* cOldTXD, const char* cOldTXN, const char* cNewTXD, const char* cNewTXN)
	{
		CGameStream* pGameStream = CGameStreamMgr::GetGameStream();
		if(pGameStream)
		{
			pGameStream->UpdateFeedItemTexture(cOldTXD, cOldTXN, cNewTXD, cNewTXN);
		}
	}

	void CommandBeginTheFeedPost(const char *pMainTextLabel)
	{
		CScriptTextConstruction::BeginTheFeedPost(pMainTextLabel);
	}
	
	int CommandEndTheFeedPostStats(const char* Title, int UNUSED_PARAM(Icon), int LevelTotal, int LevelCurrent, bool IsImportant, const char* ContactTxD, const char* ContactTxN )
	{
		return( CScriptTextConstruction::EndTheFeedPostStats(Title, LevelTotal, LevelCurrent, IsImportant, ContactTxD, ContactTxN) );
	}

	int CommandEndTheFeedPostMessageText(const char* ContactTxD, const char* ContactTxN, bool IsImportant, int Icon, const char* CharacterName, const char* Subtitle)
	{
		return( CScriptTextConstruction::EndTheFeedPostMessageText(ContactTxD, ContactTxN, IsImportant, Icon, CharacterName, Subtitle ? Subtitle : "") );
	}

	int CommandEndTheFeedPostMessageTextSubtitleLabel(const char* ContactTxD, const char* ContactTxN, bool IsImportant, int Icon, const char* CharacterName, const char* Subtitle)
	{
		return( CScriptTextConstruction::EndTheFeedPostMessageText(ContactTxD, ContactTxN, IsImportant, Icon, CharacterName, Subtitle ? TheText.Get(Subtitle) : "") );
	}

	int CommandEndTheFeedPostMessageTextTU(const char* ContactTxD, const char* ContactTxN, bool IsImportant, int Icon, const char* CharacterName, const char* Subtitle, float timeMultiplier)
	{
		return( CScriptTextConstruction::EndTheFeedPostMessageText(ContactTxD, ContactTxN, IsImportant, Icon, CharacterName, Subtitle ? Subtitle : "", timeMultiplier) );
	}

	int CommandEndTheFeedPostMessageTextWithCrewTag(const char* ContactTxD, const char* ContactTxN, bool IsImportant, int Icon, const char* CharacterName, const char* Subtitle, float timeMultiplier, const char* CrewTagPacked)
	{
		return( CScriptTextConstruction::EndTheFeedPostMessageText(ContactTxD, ContactTxN, IsImportant, Icon, CharacterName, Subtitle ? Subtitle : "", timeMultiplier, CrewTagPacked) );
	}

	int CommandEndTheFeedPostMessageTextWithCrewTagAndAdditionalIcon(const char* ContactTxD, const char* ContactTxN, bool IsImportant, int Icon, const char* CharacterName, const char* Subtitle, float timeMultiplier, const char* CrewTagPacked, int Icon2, int iHudColor)
	{
		return( CScriptTextConstruction::EndTheFeedPostMessageText(ContactTxD, ContactTxN, IsImportant, Icon, CharacterName, Subtitle ? Subtitle : "", timeMultiplier, CrewTagPacked, Icon2, iHudColor) );
	}

	int CommandEndTheFeedPostTicker(bool IsImportant, bool bCacheMessage)
	{
		return( CScriptTextConstruction::EndTheFeedPostTicker(IsImportant, bCacheMessage, false) );
	}

	int CommandEndTheFeedPostTickerForced(bool IsImportant, bool bCacheMessage)
	{
		bool bPausedWhenCommandCalled = CGameStreamMgr::GetGameStream()->IsPaused();  // store paused state when the command was called

		CGameStreamMgr::GetGameStream()->FlushQueue(true);  // flush everything in the queue but lets not disable rendering as we need to render new message

		if (bPausedWhenCommandCalled)
		{
			CGameStreamMgr::GetGameStream()->Resume();  // resume if we were paused
		}

		s32 iReturnValue = ( CScriptTextConstruction::EndTheFeedPostTicker(IsImportant, bCacheMessage, false) );  // post it

		return iReturnValue;
	}

	int CommandEndTheFeedPostTickerWithTokens(bool bIsImportant, bool bCacheMessage)
	{
		return ( CScriptTextConstruction::EndTheFeedPostTicker(bIsImportant, bCacheMessage, true));
	}


	int CommandEndTheFeedPostTickerF10(const char* TopLine, bool IsImportant, bool bCacheMessage)
	{
		return( CScriptTextConstruction::EndTheFeedPostTickerF10(TopLine, IsImportant, bCacheMessage) );
	}

	int CommandEndTheFeedPostAward(const char* TxD, const char* TxN, int iXP, s32 eAwardColour, const char* Title)
	{
		return( CScriptTextConstruction::EndTheFeedPostAward(TxD, TxN, iXP, static_cast<eHUD_COLOURS>(eAwardColour), Title) );
	}

	int CommandEndTheFeedPostCrewTag(bool IsPrivate, bool ShowLogoFlag, const char* CrewString, int CrewRank, bool FounderStatus, bool IsImportant, int crewId, int crewColR, int crewColG, int crewColB)
	{
		return( CScriptTextConstruction::EndTheFeedPostCrewTag(IsPrivate, ShowLogoFlag, CrewString, CrewRank, FounderStatus, IsImportant, crewId, NULL, crewColR, crewColG, crewColB));
	}

	int CommandEndTheFeedPostCrewTagWithGameName(bool IsPrivate, bool ShowLogoFlag, const char* CrewString, int CrewRank, bool FounderStatus, bool IsImportant, int crewId, const char* GameName, int crewColR, int crewColG, int crewColB )
	{
		return( CScriptTextConstruction::EndTheFeedPostCrewTag(IsPrivate, ShowLogoFlag, CrewString, CrewRank, FounderStatus, IsImportant, crewId, GameName, crewColR, crewColG, crewColB ) );
	}

	int CommandEndTheFeedPostUnlockTU(const char* Title, int iconType, const char* UNUSED_PARAM(chFullBody), bool bIsImportant)
	{
		return( CScriptTextConstruction::EndTheFeedPostUnlock(Title, iconType, bIsImportant, HUD_COLOUR_PURE_WHITE, false ));
	}

	int CommandEndTheFeedPostUnlockTUWithColor(const char* Title, int iconType, const char* UNUSED_PARAM(chFullBody), bool bIsImportant, s32 eTitleColour, bool bTitleIsLiteral)
	{
		return( CScriptTextConstruction::EndTheFeedPostUnlock(Title, iconType, bIsImportant, static_cast<eHUD_COLOURS>(eTitleColour), bTitleIsLiteral) );
	}

	int CommandEndTheFeedPostUnlock(const char* Title, int iconType, const char* UNUSED_PARAM(chFullBody))
	{
		return CommandEndTheFeedPostUnlockTU(Title, iconType, "", false);
	}

	int CommandEndTheFeedPostMpTicker(bool IsImportant, bool bCacheMessage)
	{
		scriptAssertf(0, "Using deprecated command END_TEXT_COMMAND_THEFEED_POST_MPTICKER.  Please use END_TEXT_COMMAND_THEFEED_POST_TICKER instead.");
		return( CScriptTextConstruction::EndTheFeedPostTicker(IsImportant, bCacheMessage, false) );
	}
	
	int CommandEndTheFeedPostCrewRankup(const char* chSubtitle, const char* chTXD, const char* chTXN, bool bIsImportant)
	{
		return( CScriptTextConstruction::EndTheFeedPostCrewRankup(chSubtitle, chTXD, chTXN, bIsImportant, false) );
	}

	int CommandEndTheFeedPostCrewRankupWithLiteralFlag(const char* chSubtitle, const char* chTXD, const char* chTXN, bool bIsImportant, bool bSubtitleIsLiteral)
	{
		return( CScriptTextConstruction::EndTheFeedPostCrewRankup(chSubtitle, chTXD, chTXN, bIsImportant, bSubtitleIsLiteral) );
	}

	int CommandEndTheFeedPostVersus(const char* ch1TXD, const char* ch1TXN, int val1, const char* ch2TXD, const char* ch2TXN, int val2, int iCustomColor1, int iCustomColor2)
	{
		return( CScriptTextConstruction::EndTheFeedPostVersus(ch1TXD, ch1TXN, val1, ch2TXD, ch2TXN, val2, (eHUD_COLOURS)iCustomColor1, (eHUD_COLOURS)iCustomColor2) );
	}	

	int CommandEndTheFeedPostReplay(int replayState, int iIcon, const char* cSubtitle)
	{
		return CScriptTextConstruction::EndTheFeedPostReplay(static_cast<CGameStream::eFeedReplayState>(replayState), iIcon, cSubtitle);
	}

	int CommandEndTheFeedPostReplayInput(int replayState, const char* cIcon, const char* cSubtitle)
	{
		return CScriptTextConstruction::EndTheFeedPostReplayInput(static_cast<CGameStream::eFeedReplayState>(replayState), cIcon, cSubtitle);
	}

	void CommandBeginTextCommandPrint(const char *pMainTextLabel)
	{
		CScriptTextConstruction::BeginPrint(pMainTextLabel);
	}

	void CommandEndTextCommandPrint(s32 Duration, bool bPrintNow)
	{
		CScriptTextConstruction::EndPrint(Duration, bPrintNow);
	}

	void CommandBeginTextCommandIsMessageDisplayed(const char *pMainTextLabel)
	{
		CScriptTextConstruction::BeginIsMessageDisplayed(pMainTextLabel);
	}

	bool CommandEndTextCommandIsMessageDisplayed()
	{
		return CScriptTextConstruction::EndIsMessageDisplayed();
	}


	void CommandBeginTextCommandDisplayText(const char *pMainTextLabel)
	{
		CScriptTextConstruction::BeginDisplayText(pMainTextLabel);
		CTheScripts::Frack();
	}

	void CommandEndTextCommandDisplayText(float DisplayAtX, float DisplayAtY, int Stereo)
	{
		CScriptTextConstruction::EndDisplayText(DisplayAtX, DisplayAtY, !CTheScripts::Frack() /*false*/, Stereo );
	}

	void CommandEndTextCommandDisplayDebugText(float DisplayAtX, float DisplayAtY)
	{
		CScriptTextConstruction::EndDisplayText(DisplayAtX, DisplayAtY, CTheScripts::Frack() /*true*/);
	}

	void CommandBeginTextCommandGetScreenWidthOfDisplayText(const char *pMainTextLabel)
	{
		CScriptTextConstruction::BeginGetScreenWidthOfDisplayText(pMainTextLabel);
	}

	float CommandEndTextCommandGetScreenWidthOfDisplayText(bool bIncludeSpaces)
	{
		return CScriptTextConstruction::EndGetScreenWidthOfDisplayText(bIncludeSpaces);
	}


	void CommandBeginTextCommandGetNumberOfLinesForString(const char *pMainTextLabel)
	{
		CScriptTextConstruction::BeginGetNumberOfLinesForString(pMainTextLabel);
	}

	s32 CommandEndTextCommandGetNumberOfLinesForString(float DisplayAtX, float DisplayAtY)
	{
		return CScriptTextConstruction::EndGetNumberOfLinesForString(DisplayAtX, DisplayAtY);
	}


	void CommandBeginTextCommandDisplayHelp(const char *pMainTextLabel)
	{
		CScriptTextConstruction::BeginDisplayHelp(pMainTextLabel);
	}

	void CommandEndTextCommandDisplayHelp(s32 iHelpId, bool bDisplayForever, bool bPlaySound, s32 OverrideDuration)
	{
		CScriptTextConstruction::EndDisplayHelp(iHelpId, bDisplayForever, bPlaySound, OverrideDuration);
	}


	void CommandBeginTextCommandIsThisHelpMessageBeingDisplayed(const char *pMainTextLabel)
	{
		CScriptTextConstruction::BeginIsThisHelpMessageBeingDisplayed(pMainTextLabel);
	}

	bool CommandEndTextCommandIsThisHelpMessageBeingDisplayed(s32 iHelpId)
	{
		return CScriptTextConstruction::EndIsThisHelpMessageBeingDisplayed(iHelpId);
	}

	void CommandBeginTextCommandPlayerNameRow(const char *pMainTextLabel)
	{
		CScriptTextConstruction::BeginRowAbovePlayersHead(pMainTextLabel);
	}

	void CommandEndTextCommandPlayerNameRow()
	{
		CScriptTextConstruction::EndRowAbovePlayersHead();
	}

	void CommandBeginTextCommandSetBlipName(const char *pMainTextLabel)
	{
		CScriptTextConstruction::BeginSetBlipName(pMainTextLabel);
	}

	void CommandEndTextCommandSetBlipName(s32 iBlipIndex)
	{
		CScriptTextConstruction::EndSetBlipName(iBlipIndex);
	}

	
	void CommandBeginTextCommandAddDirectlyToPreviousBriefs(const char *pMainTextLabel)
	{
		CScriptTextConstruction::BeginAddDirectlyToPreviousBriefs(pMainTextLabel);
	}

	void CommandEndTextCommandAddDirectlyToPreviousBriefs(bool bUsesUnderscore)
	{
		CScriptTextConstruction::EndAddDirectlyToPreviousBriefs(bUsesUnderscore);
	}

 	void CommandBeginTextCommandOverrideButtonText(const char* pMainTextLabel)
 	{
		CScriptTextConstruction::BeginOverrideButtonText(pMainTextLabel);
 	}
 
 	void CommandEndTextCommandOverrideButtonText(int iSlotIndex)
 	{
		CScriptTextConstruction::EndOverrideButtonText(iSlotIndex);
 	}

	void CommandBeginTextCommandClearPrint(const char *pMainTextLabel)
	{
		CScriptTextConstruction::BeginClearPrint(pMainTextLabel);
	}

	void CommandEndTextCommandClearPrint()
	{
		CScriptTextConstruction::EndClearPrint();
	}

	void CommandAddTextComponentInteger(s32 IntegerToAdd)
	{
		CScriptTextConstruction::AddInteger(IntegerToAdd);
	}

	void CommandAddTextComponentFloat(float FloatToAdd, s32 NumberOfDecimalPlaces)
	{
		CScriptTextConstruction::AddFloat(FloatToAdd, NumberOfDecimalPlaces);
	}

	void CommandAddTextComponentSubStringTextLabel(const char *pSubStringTextLabelToAdd)
	{
		CScriptTextConstruction::AddSubStringTextLabel(pSubStringTextLabelToAdd);
	}

	void CommandAddTextComponentSubStringTextLabelHashKey(s32 HashKeyOfSubStringTextLabelToAdd)
	{
		CScriptTextConstruction::AddSubStringTextLabelHashKey((u32) HashKeyOfSubStringTextLabelToAdd);
	}

	void CommandAddTextComponentSubStringLiteralString(const char *pSubStringLiteralStringToAdd)
	{
		CScriptTextConstruction::AddSubStringLiteralString(pSubStringLiteralStringToAdd);
	}

	void CommandAddTextComponentSubStringBlipName(s32 blipIndex)
	{
		CScriptTextConstruction::AddSubStringBlipName(blipIndex);
	}

	void CommandAddTextComponentSubStringPlayerName(const char *pPlayerName)
	{
		CScriptTextConstruction::AddSubStringLiteralString(pPlayerName);
	}

	void CommandAddTextComponentSubStringIpAddress(const char *pIpAddressString)
	{
		CScriptTextConstruction::AddSubStringLiteralString(pIpAddressString);
	}

	void CommandAddTextComponentSubStringPassword(const char *pPasswordString)
	{
		CScriptTextConstruction::AddSubStringLiteralString(pPasswordString);
	}


	enum
	{
		TEXT_FORMAT_MILLISECONDS = 1,	//	BIT(0)
		TEXT_FORMAT_SECONDS = 2,	//	BIT(1)
		TEXT_FORMAT_MINUTES = 4,	//	BIT(2)
		TEXT_FORMAT_HOURS = 8,	//	BIT(3)
		TEXT_FORMAT_DAYS = 16,	//	BIT(4)
		TEXT_FORMAT_HIDE_LEADING_UNITS_EQUAL_TO_ZERO = 32,		//	e.g. show 12:20 not 00:12:20	//	BIT(5)
		TEXT_FORMAT_HIDE_LEADING_ZEROS_ON_LEADING_UNITS = 64,	//	e.g. show 1:14 not 01:14	//	BIT(6)
		TEXT_FORMAT_SHOW_UNIT_DIVIDERS_AS_LETTERS = 128,			//	e.g. show 3m24s not 3:24	//	BIT(7)
		TEXT_FORMAT_HIDE_UNIT_LETTER_FOR_SMALLEST_UNITS = 256,	//	e.g. show 3m24 not 3m24s	//	BIT(8)
		TEXT_FORMAT_HIDE_MILLISECONDS_UNITS_DIGIT = 512,		//	e.g. show 05:51 not 05:519	//	BIT(9)
		TEXT_FORMAT_HIDE_MILLISECONDS_TENS_DIGIT = 1024,		//	e.g. show 05:5 not 05:519	//	BIT(10)
		TEXT_FORMAT_USE_DOT_FOR_MILLISECOND_DIVIDER = 2048		//	e.g. show 12.345 not 12:345	//	BIT(11)
	};

	enum
	{
		INDEX_MILLISECONDS = 0,
		INDEX_SECONDS = 1,
		//	INDEX_MINUTES,
		//	INDEX_HOURS,
		INDEX_DAYS = 4
	};


	char TimeUnitsTextLabels[5][13] =
	{
		"TIMER_MSEC",
		"TIMER_SEC",
		"TIMER_MINUTE",
		"TIMER_HOUR",
		"TIMER_DAY"
	};

	s32 UnitDenominators[5] = { 1000, 60, 60, 24, 1 };	//	Last value added after static analysis showed that I was accessing UnitDenominators[INDEX_DAYS]

	void CommandAddTextComponentSubstringTime(s32 TimeInMilliseconds, s32 TimeFormat)
	{
		s32 loop = 0;

		s32 LargestUnits = -1;
		s32 SmallestUnits = 99;
		for (loop = INDEX_MILLISECONDS; loop <= INDEX_DAYS; loop++)
		{
			if (TimeFormat & BIT(loop))
			{
				if (loop > LargestUnits)
				{
					LargestUnits = loop;
				}
				if (loop < SmallestUnits)
				{
					SmallestUnits = loop;
				}
			}
		}

		if ((SmallestUnits == 99) || (LargestUnits == -1))
		{
			scriptAssertf(0, "ADD_TEXT_COMPONENT_SUBSTRING_TIME - no time units have been set in the TimeFormat parameter");
			return;
		}

		//	Make sure all bits are set between the largest and smallest units
		for (loop = SmallestUnits; loop < LargestUnits; loop++)
		{
			if ( (TimeFormat & BIT(loop)) == 0)
			{
//				scriptDisplayf("ADD_TEXT_COMPONENT_SUBSTRING_TIME - setting bit %d as it lies between the largest and smallest units", loop);
				TimeFormat |= BIT(loop);
			}
		}

		// Fill the Value array with days, hours, minutes, seconds and milliseconds
		s32 Value[5];
		s32 WorkingValue = 0;
		char TimeString[64];

		if (TimeInMilliseconds < 0)
		{
			WorkingValue = -TimeInMilliseconds;
			safecpy(TimeString, "-", NELEM(TimeString));
		}
		else
		{
			WorkingValue = TimeInMilliseconds;
			safecpy(TimeString, "", NELEM(TimeString));
		}


		for (loop = INDEX_MILLISECONDS; loop <= INDEX_DAYS; loop++)
		{
			if (loop == LargestUnits)
			{	//	then display the full remaining number in these units
				Value[loop] = WorkingValue;
				WorkingValue = 0;
			}
			else
			{
				Value[loop] = WorkingValue % UnitDenominators[loop];
			}

			WorkingValue /= UnitDenominators[loop];
		}


		if ((TimeFormat & TEXT_FORMAT_SHOW_UNIT_DIVIDERS_AS_LETTERS) == 0)
		{	//	Never show the ':' after the last units
			TimeFormat |= TEXT_FORMAT_HIDE_UNIT_LETTER_FOR_SMALLEST_UNITS;
		}

		for (loop = LargestUnits; loop >= SmallestUnits; loop--)
		{
			bool bLeadingUnits = false;
			if ( (loop == LargestUnits) || ((TimeFormat & BIT(loop+1)) == 0) )
			{
				bLeadingUnits = true;
			}

			if (loop != SmallestUnits)
			{	//	Always display the smallest units even if the value is 0
				if ( (bLeadingUnits) && (Value[loop] == 0) && (TimeFormat & TEXT_FORMAT_HIDE_LEADING_UNITS_EQUAL_TO_ZERO) )
				{	//	Clear BIT(loop) so that Value[loop] is not displayed
					TimeFormat &= ~BIT(loop);
				}
			}

			if (TimeFormat & BIT(loop))
			{	//	If this value hasn't been hidden because it's equal to 0
				bool bDisplayLeadingZeros = true;
				if (bLeadingUnits && (TimeFormat & TEXT_FORMAT_HIDE_LEADING_ZEROS_ON_LEADING_UNITS) )
				{
					bDisplayLeadingZeros = false;
				}


				u32 NumberOfZerosToPrefix = 0;
				if (bDisplayLeadingZeros)
				{
					if ( (loop == INDEX_MILLISECONDS) && (Value[loop] < 100) )
					{
						NumberOfZerosToPrefix++;
					}

					if (Value[loop] < 10)
					{
						NumberOfZerosToPrefix++;
					}
				}

				const u32 SizeOfNumberAsString = 16;
				char NumberAsString[SizeOfNumberAsString];
				switch (NumberOfZerosToPrefix)
				{
					case 0 :
						formatf(NumberAsString, SizeOfNumberAsString, "%d", Value[loop]);
						break;

					case 1 :
						formatf(NumberAsString, SizeOfNumberAsString, "0%d", Value[loop]);
						break;

					case 2 :
						formatf(NumberAsString, SizeOfNumberAsString, "00%d", Value[loop]);
						break;

					default :
						scriptAssertf(0, "ADD_TEXT_COMPONENT_SUBSTRING_TIME - unexpected value for NumberOfZerosToPrefix");
						formatf(NumberAsString, SizeOfNumberAsString, "%d", Value[loop]);
						break;
				}

				if (loop == INDEX_MILLISECONDS)
				{
					if (!bLeadingUnits)	//	We probably don't want to hide the ten or unit digits if Milliseconds is the leading time units
					{
						if ( (TimeFormat & TEXT_FORMAT_HIDE_MILLISECONDS_UNITS_DIGIT) || (TimeFormat & TEXT_FORMAT_HIDE_MILLISECONDS_TENS_DIGIT) )
						{
							s32 LengthOfMillisecondsString = istrlen(NumberAsString);  // ok as its just ascii

							if (TimeFormat & TEXT_FORMAT_HIDE_MILLISECONDS_UNITS_DIGIT)
							{
								if (LengthOfMillisecondsString > 0)
								{
									NumberAsString[LengthOfMillisecondsString-1] = '\0';
								}
							}

							if (TimeFormat & TEXT_FORMAT_HIDE_MILLISECONDS_TENS_DIGIT)
							{
								if (LengthOfMillisecondsString > 1)
								{
									NumberAsString[LengthOfMillisecondsString-2] = '\0';
								}
							}
						}
					}
				}
				safecat(TimeString, NumberAsString, NELEM(TimeString));

				if ( (loop > SmallestUnits) || ((TimeFormat & TEXT_FORMAT_HIDE_UNIT_LETTER_FOR_SMALLEST_UNITS) == 0) )
				{
					if (TimeFormat & TEXT_FORMAT_SHOW_UNIT_DIVIDERS_AS_LETTERS)
					{
						char *pGxtUnitsString = TheText.Get(TimeUnitsTextLabels[loop]);
						safecat(TimeString, pGxtUnitsString, NELEM(TimeString));
					}
					else
					{
						if ( (TimeFormat & TEXT_FORMAT_USE_DOT_FOR_MILLISECOND_DIVIDER) && (loop == INDEX_SECONDS) && (TimeFormat & BIT(INDEX_MILLISECONDS)) )
						{
							safecat(TimeString, ".", NELEM(TimeString));
						}
						else
						{
							safecat(TimeString, ":", NELEM(TimeString));
						}
					}
				}
			}	//	if (TimeFormat & BIT(loop)) i.e. display the value for these units
		}	//	for (loop = LargestUnits; loop >= SmallestUnits; loop--)

		CScriptTextConstruction::AddSubStringLiteralString(TimeString);
	}

	enum
	{
		INTEGER_TEXT_FORMAT_COMMA_SEPARATORS = 1	//	BIT(0)
	};

	void CommandAddTextComponentFormattedInteger(s32 integerToFormat, s32 FormattingFlags)
	{
		const u32 lengthOfFormattedString = 64;
		char formattedString[lengthOfFormattedString];

		if ( (FormattingFlags & INTEGER_TEXT_FORMAT_COMMA_SEPARATORS) != 0)
		{
			if (integerToFormat < 0)
			{
				safecpy(formattedString, "-", lengthOfFormattedString);
				integerToFormat *= -1;
			}
			else
			{
				safecpy(formattedString, "", lengthOfFormattedString);
			}

			u32 number_of_digits = 1;
			s32 workingNumber = integerToFormat;
			s32 divideBy = 1;
			while (workingNumber > 9)
			{
				workingNumber /= 10;
				divideBy *= 10;
				number_of_digits++;
			}

			bool bNeedToAddAComma = false;
			u32 countdownToNextComma = 0;
			if (number_of_digits > 3)
			{
				bNeedToAddAComma = true;
				countdownToNextComma = (number_of_digits % 3);
				if (countdownToNextComma == 0)
				{
					countdownToNextComma = 3;
				}
			}

			u32 digitCountdown = number_of_digits;
			workingNumber = integerToFormat;
			char digitToAdd[2];

			while (digitCountdown > 0)
			{
				s32 thisDigit = 0;
				if (digitCountdown == 1)
				{
					thisDigit = workingNumber;					
				}
				else
				{
					thisDigit = workingNumber / divideBy;
					workingNumber -= (thisDigit * divideBy);

					divideBy /= 10;
				}

				scriptAssertf( (thisDigit >= 0) && (thisDigit <= 9), "%s: ADD_TEXT_COMPONENT_FORMATTED_INTEGER - expected thisDigit to be a single digit but it's %d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), thisDigit);
				formatf(digitToAdd, 2, "%d", thisDigit);
				safecat(formattedString, digitToAdd, lengthOfFormattedString);

				digitCountdown--;

				countdownToNextComma--;
				if (countdownToNextComma == 0)
				{
					if (bNeedToAddAComma)
					{
						safecat(formattedString, ",", lengthOfFormattedString);

						if (digitCountdown <= 3)
						{
							bNeedToAddAComma = false;
						}
					}

					countdownToNextComma = 3;
				}
			}
		}
		else
		{
			formatf(formattedString, lengthOfFormattedString, "%d", integerToFormat);
		}

		CScriptTextConstruction::AddSubStringLiteralString(formattedString);
	}

	void CommandAddTextComponentSubstringPhoneNumber(const char *pPhoneNumberString, s32 NumberOfCharactersToDisplay)
	{
		if (NumberOfCharactersToDisplay == -1)
		{	//	Display the full string
			CScriptTextConstruction::AddSubStringLiteralString(pPhoneNumberString);
		}
		else
		{
			const s32 MaxCharsInPhoneNumber = 64;
			char TruncatedPhoneNumber[MaxCharsInPhoneNumber];
			if (NumberOfCharactersToDisplay >= MaxCharsInPhoneNumber)
			{
				scriptAssertf(0, "ADD_TEXT_COMPONENT_SUBSTRING_PHONE_NUMBER - too many characters. Maximum is %d", MaxCharsInPhoneNumber-1);
				NumberOfCharactersToDisplay = (MaxCharsInPhoneNumber-1);	//	Can only fit 63 actual characters (Need 1 for the NULL terminator)
			}

			safecpy(TruncatedPhoneNumber, pPhoneNumberString, NumberOfCharactersToDisplay+1);	//	Add 1 for the NULL terminator
			CScriptTextConstruction::AddSubStringLiteralString(TruncatedPhoneNumber);
		}
	}

	void CommandAddTextComponentSubstringWebsite(const char *pWebsite)
	{
		CScriptTextConstruction::AddSubStringLiteralString(pWebsite);
	}

	void CommandAddTextComponentSubstringKeyboardDisplay(const char *pStringContainingKeysTypedByPlayer)
	{
		CScriptTextConstruction::AddSubStringLiteralString(pStringContainingKeysTypedByPlayer);
	}

	void CommandSetColourOfNextTextComponent(s32 HudColourIndex)
	{
		CScriptTextConstruction::SetColourOfNextTextComponent(HudColourIndex);
	}

	void CommandGetExpectedNumberOfTextComponents(const char *pTextLabel, s32 &ReturnNumberOfNumbers, s32 &ReturnNumberOfSubstrings)
	{
		ReturnNumberOfNumbers = 0;
		ReturnNumberOfSubstrings = 0;

		if (SCRIPT_VERIFY(pTextLabel, "GET_EXPECTED_NUMBER_OF_TEXT_COMPONENTS - Text label is NULL"))
		{
			if (SCRIPT_VERIFY(strlen(pTextLabel) > 0, "GET_EXPECTED_NUMBER_OF_TEXT_COMPONENTS - Text label is an empty string"))
			{
				char *pGxtString = TheText.Get(pTextLabel);
				CMessages::GetExpectedNumberOfTextComponents(pGxtString, ReturnNumberOfNumbers, ReturnNumberOfSubstrings);
			}
		}
	}

	void CommandCustomMinimapSetActive(bool bActive)
	{
		sMiniMapMenuComponent.SetActive(bActive);
	}

	bool CommandCustomMinimapIsActive()
	{
		return sMiniMapMenuComponent.IsActive();
	}

	bool CommandCustomMinimapSnapToBlipWithIndex(int iIndex)
	{
		bool bResult = false;

		if (SCRIPT_VERIFY(iIndex >= 0, "CUSTOM_MINIMAP_SNAP_TO_BLIP_WITH_INDEX - negative index"))
		{
			bResult = sMiniMapMenuComponent.SnapToBlip(iIndex);
		}

		return bResult;
	}


	bool CommandCustomMinimapSnapToBlipWithUniqueId(int iUniqueId)
	{
		bool bResult = false;

		if (SCRIPT_VERIFY(iUniqueId >= 0, "CUSTOM_MINIMAP_SNAP_TO_BLIP_WITH_UNIQUE_ID - negative index"))
		{
			bResult = sMiniMapMenuComponent.SnapToBlipUsingUniqueId(iUniqueId);
		}

		return bResult;
	}

	void CommandCustomMinimapSetWaypointWithIndex()
	{
		sMiniMapMenuComponent.SetWaypoint();
	}

	int CommandCustomMinimapGetNumberOfBlips()
	{
		return sMiniMapMenuComponent.GetNumberOfBlips();
	}

	void CommandCustomMinimapSetBlipObject(int iBlipObject)
	{
		sMiniMapMenuComponent.SetBlipObject(iBlipObject);
	}

	int CommandCustomMinimapGetBlipObject()
	{
		return sMiniMapMenuComponent.GetBlipObject();
	}

	void CommandCustomMinimapClearBlips()
	{
		sMiniMapMenuComponent.ResetBlips();
	}

	s32 CommandCustomMinimapCreateBlip(const scrVector & pos)
	{
		Vector3 vPos = Vector3(pos);
		return sMiniMapMenuComponent.CreateBlip(vPos);
	}
	
	bool CommandCustomMinimapRemoveBlip(s32 iIndex)
	{
		return sMiniMapMenuComponent.RemoveBlip(iIndex);
	}

	void CommandForceSonarBlipsThisFrame()
	{
		CMiniMap::SetForceSonarBlipsThisFrame(true);
	}

s32 CommandGetNorthBlipId()
{
	return CMiniMap::GetNorthBlipIndex();
}

void CommandPrint(const char *pTextLabel, int Duration, int UNUSED_PARAM(Colour))
{
	CScriptTextConstruction::BeginPrint(pTextLabel);
	CScriptTextConstruction::EndPrint(Duration, false);
}

void CommandPrintNow(const char *pTextLabel, int Duration, int UNUSED_PARAM(Colour))
{
	CScriptTextConstruction::BeginPrint(pTextLabel);
	CScriptTextConstruction::EndPrint(Duration, true);
}

void CommandClearPrints()
{
	CMessages::ClearMessages();
}

void CommandClearBrief()
{
	// Clear Mission / Help / Dialogue
	CMessages::ClearAllBriefMessages();
	
	// Clear Notifications
	CGameStream* pGameStream = CGameStreamMgr::GetGameStream();
	if(pGameStream)
	{
		pGameStream->ClearCache();
	}
}

void CommandClearAllHelpMessages()
{
	CHelpMessage::ClearAll();
}


void CommandPrintWithNumber(const char *pTextLabel, int NumberToInsert, int Duration, int UNUSED_PARAM(Colour))
{
	CScriptTextConstruction::BeginPrint(pTextLabel);
	CScriptTextConstruction::AddInteger(NumberToInsert);
	CScriptTextConstruction::EndPrint(Duration, false);
}

void CommandPrintWithNumberNow(const char *pTextLabel, int NumberToInsert, int Duration, int UNUSED_PARAM(Colour))
{
	CScriptTextConstruction::BeginPrint(pTextLabel);
	CScriptTextConstruction::AddInteger(NumberToInsert);
	CScriptTextConstruction::EndPrint(Duration, true);
}

void CommandPrintWith2Numbers(const char *pTextLabel, int FirstNumberToInsert, int SecondNumberToInsert, int Duration, int UNUSED_PARAM(Colour))
{
	CScriptTextConstruction::BeginPrint(pTextLabel);
	CScriptTextConstruction::AddInteger(FirstNumberToInsert);
	CScriptTextConstruction::AddInteger(SecondNumberToInsert);
	CScriptTextConstruction::EndPrint(Duration, false);
}

void CommandPrintWith2NumbersNow(const char *pTextLabel, int FirstNumberToInsert, int SecondNumberToInsert, int Duration, int UNUSED_PARAM(Colour))
{
	CScriptTextConstruction::BeginPrint(pTextLabel);
	CScriptTextConstruction::AddInteger(FirstNumberToInsert);
	CScriptTextConstruction::AddInteger(SecondNumberToInsert);
	CScriptTextConstruction::EndPrint(Duration, true);
}

void CommandPrintStringInString(const char *pTextLabel, const char *pShortTextLabel, int Duration, int UNUSED_PARAM(Colour))
{
	CScriptTextConstruction::BeginPrint(pTextLabel);
	CScriptTextConstruction::AddSubStringTextLabel(pShortTextLabel);
	CScriptTextConstruction::EndPrint(Duration, false);
}

void CommandPrintStringInStringNow(const char *pTextLabel, const char *pShortTextLabel, int Duration, int UNUSED_PARAM(Colour))
{
	CScriptTextConstruction::BeginPrint(pTextLabel);
	CScriptTextConstruction::AddSubStringTextLabel(pShortTextLabel);
	CScriptTextConstruction::EndPrint(Duration, true);
}

void CommandPrintStringWithTwoStrings(const char *pTextLabel, const char *pShortTextLabel1, const char *pShortTextLabel2, int Duration, int UNUSED_PARAM(Colour), bool bPrintNow)
{
	CScriptTextConstruction::BeginPrint(pTextLabel);
	CScriptTextConstruction::AddSubStringTextLabel(pShortTextLabel1);
	CScriptTextConstruction::AddSubStringTextLabel(pShortTextLabel2);
	CScriptTextConstruction::EndPrint(Duration, bPrintNow);
}

void CommandPrintStringWithLiteralString(const char *pTextLabel, const char *pLiteralString, int Duration, int UNUSED_PARAM(Colour))
{
	CScriptTextConstruction::BeginPrint(pTextLabel);
	CScriptTextConstruction::AddSubStringLiteralString(pLiteralString);
	CScriptTextConstruction::EndPrint(Duration, false);
}

void CommandPrintStringWithLiteralStringNow(const char *pTextLabel, const char *pLiteralString, int Duration, int UNUSED_PARAM(Colour))
{
	CScriptTextConstruction::BeginPrint(pTextLabel);
	CScriptTextConstruction::AddSubStringLiteralString(pLiteralString);
	CScriptTextConstruction::EndPrint(Duration, true);
}

void CommandPrintStringWithTwoLiteralStrings(const char *pTextLabel, const char *pLiteralString1, const char *pLiteralString2, int Duration, int UNUSED_PARAM(Colour))
{
	CScriptTextConstruction::BeginPrint(pTextLabel);
	CScriptTextConstruction::AddSubStringLiteralString(pLiteralString1);
	CScriptTextConstruction::AddSubStringLiteralString(pLiteralString2);
	CScriptTextConstruction::EndPrint(Duration, false);
}

void CommandPrintStringWithTwoLiteralStringsNow(const char *pTextLabel, const char *pLiteralString1, const char *pLiteralString2, int Duration, int UNUSED_PARAM(Colour))
{
	CScriptTextConstruction::BeginPrint(pTextLabel);
	CScriptTextConstruction::AddSubStringLiteralString(pLiteralString1);
	CScriptTextConstruction::AddSubStringLiteralString(pLiteralString2);
	CScriptTextConstruction::EndPrint(Duration, true);
}


void CommandClearThisPrint(const char *pTextLabel)
{
	char *pMainString = TheText.Get(pTextLabel);
	s32 MainTextBlock = TheText.GetBlockContainingLastReturnedString();

	CSubtitleMessage MessageToClear;
	MessageToClear.Set(pMainString, MainTextBlock, 
		NULL, 0, 
		NULL, 0, 
		false, false);	//	Not sure what bUsesUnderscore should be set to

	CMessages::ClearThisPrint(MessageToClear, true);
}

void CommandClearSmallPrints()
{
	CMessages::ClearMessages();
}

bool CommandIsThisPrintBeingDisplayed(const char *pTextLabel, int ExtraParamsFlag, const char *pSubStringTextLabel, const char *pLiteralString1, const char *pLiteralString2, int FirstNumberToInsert, int SecondNumberToInsert, int ThirdNumberToInsert, int FourthNumberToInsert, int FifthNumberToInsert, int SixthNumberToInsert)
{
	CScriptTextConstruction::BeginIsMessageDisplayed(pTextLabel);

	u32 NumberOfNumbersToInsert = 0;
	if ( (ExtraParamsFlag >= SCRIPT_PRINT_ONE_NUMBER) && (ExtraParamsFlag <= SCRIPT_PRINT_SIX_NUMBERS) )
	{
		NumberOfNumbersToInsert = ExtraParamsFlag - SCRIPT_PRINT_ONE_NUMBER + 1;
	}

	if (NumberOfNumbersToInsert)
	{
		CScriptTextConstruction::AddInteger(FirstNumberToInsert);
		NumberOfNumbersToInsert--;
	}
	if (NumberOfNumbersToInsert)
	{
		CScriptTextConstruction::AddInteger(SecondNumberToInsert);
		NumberOfNumbersToInsert--;
	}
	if (NumberOfNumbersToInsert)
	{
		CScriptTextConstruction::AddInteger(ThirdNumberToInsert);
		NumberOfNumbersToInsert--;
	}
	if (NumberOfNumbersToInsert)
	{
		CScriptTextConstruction::AddInteger(FourthNumberToInsert);
		NumberOfNumbersToInsert--;
	}
	if (NumberOfNumbersToInsert)
	{
		CScriptTextConstruction::AddInteger(FifthNumberToInsert);
		NumberOfNumbersToInsert--;
	}
	if (NumberOfNumbersToInsert)
	{
		CScriptTextConstruction::AddInteger(SixthNumberToInsert);
		NumberOfNumbersToInsert--;
	}

	if (ExtraParamsFlag == SCRIPT_PRINT_ONE_SUBSTRING)
	{
		CScriptTextConstruction::AddSubStringTextLabel(pSubStringTextLabel);
	}

	u32 NumberOfLiteralStringsToInsert = 0;
	if ( (ExtraParamsFlag >= SCRIPT_PRINT_ONE_LITERAL_STRING) && (ExtraParamsFlag <= SCRIPT_PRINT_TWO_LITERAL_STRINGS) )
	{
		NumberOfLiteralStringsToInsert = ExtraParamsFlag - SCRIPT_PRINT_ONE_LITERAL_STRING + 1;
	}

	if (NumberOfLiteralStringsToInsert)
	{
		CScriptTextConstruction::AddSubStringLiteralString(pLiteralString1);
		NumberOfLiteralStringsToInsert--;
	}

	if (NumberOfLiteralStringsToInsert)
	{
		CScriptTextConstruction::AddSubStringLiteralString(pLiteralString2);
		NumberOfLiteralStringsToInsert--;
	}

	return CScriptTextConstruction::EndIsMessageDisplayed();
}

bool CommandDoesTextBlockExist(const char *pTextBlockName)
{
	if (TheText.GetAdditionalTextIndex(pTextBlockName) != -1)
	{
		return true;
	}

	return false;
}

void CommandRequestAdditionalText(const char *pTextBlockName, int SlotNumber)
{
	TheText.RequestAdditionalText(pTextBlockName, SlotNumber);
}

void CommandRequestAdditionalTextForDLC(const char *pTextBlockName, int SlotNumber)
{
	TheText.RequestAdditionalText(pTextBlockName, SlotNumber,CTextFile::TEXT_LOCATION_ADDED_FOR_DLC);
}

bool CommandHasAdditionalTextLoaded(int SlotNumber)
{
	return TheText.HasAdditionalTextLoaded(SlotNumber);
}

void CommandClearAdditionalText(int SlotNumber, bool bClearPreviousBriefs)
{
	CMessages::ClearAllDisplayedMessagesThatBelongToThisTextBlock(SlotNumber, bClearPreviousBriefs);
}

bool CommandIsStreamingAdditionalText(int SlotNumber)
{
	return TheText.IsRequestingAdditionalText(SlotNumber);
}

bool CommandHasThisAdditionalTextLoaded(const char *pTextBlockName, int SlotNumber)
{
	return TheText.HasThisAdditionalTextLoaded(pTextBlockName, SlotNumber);
}

bool CommandIsMessageBeingDisplayed()
{
	bool LatestCmpFlagResult = false;

	if (!CMessages::BriefMessages[0].IsEmpty())
	{
		LatestCmpFlagResult = true;
	}
	return (LatestCmpFlagResult);
}

bool CommandDoesTextLabelExist(const char *pTextLabel)
{
	if (TheText.DoesTextLabelExist(pTextLabel))
	{
		return true;
	}

	return false;
}

#define MAXIMUM_EMAIL_LENGTH	(1024)

const char *pNotFound = "NULL";

const char*CommandGetStringFromTextFile(const char *pTextLabel)
{
	static char cReturnString[MAXIMUM_EMAIL_LENGTH];

	if (TheText.DoesTextLabelExist(pTextLabel))
	{
		safecpy( cReturnString, TheText.Get(pTextLabel) );
		return cReturnString;
	}
	else
	{
		return pNotFound;
	}
}

const char*CommandGetFirstNCharactersOfString(const char *pTextLabel, int NumberOfCharacters)
{
	static char ReturnString[MAXIMUM_EMAIL_LENGTH];

	if (NumberOfCharacters < 0)
	{
		scriptAssertf(0, "%s:GET_FIRST_N_CHARACTERS_OF_STRING - NumberOfCharacters is negative", pTextLabel);
		return pNotFound;
	}
	if (NumberOfCharacters >= MAXIMUM_EMAIL_LENGTH)
	{
		scriptAssertf(0, "%s:GET_FIRST_N_CHARACTERS_OF_STRING - NumberOfCharacters >= MAXIMUM_EMAIL_LENGTH", pTextLabel);
		return pNotFound;
	}

	if (TheText.DoesTextLabelExist(pTextLabel))
	{
		char const * cActualMessage = TheText.Get(pTextLabel);

		int string_length = istrlen(cActualMessage);
		if (NumberOfCharacters > string_length)
		{
			scriptAssertf(0, "%s:GET_FIRST_N_CHARACTERS_OF_STRING - NumberOfCharacters is larger than the string length", pTextLabel);
			return pNotFound;
		}

		strncpy(ReturnString, cActualMessage, NumberOfCharacters);
		ReturnString[NumberOfCharacters] = '\0';
		return (const char *)&ReturnString;
	}
	else
	{
		scriptAssertf(0, "%s:GET_FIRST_N_CHARACTERS_OF_STRING - string doesn't exist", pTextLabel);
		return pNotFound;
	}
}

const char*CommandGetFirstNCharactersOfLiteralString(const char *pLiteralString, int NumberOfCharacters)
{
	static char ReturnString[MAXIMUM_EMAIL_LENGTH];

	if (pLiteralString == NULL)
	{
		scriptAssertf(0, "%s:GET_FIRST_N_CHARACTERS_OF_LITERAL_STRING - literal string is a NULL pointer", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		return pNotFound;
	}

	if (NumberOfCharacters < 0)
	{
		scriptAssertf(0, "%s:GET_FIRST_N_CHARACTERS_OF_LITERAL_STRING - NumberOfCharacters is negative", pLiteralString);
		return pNotFound;
	}
	if (NumberOfCharacters >= MAXIMUM_EMAIL_LENGTH)
	{
		scriptAssertf(0, "%s:GET_FIRST_N_CHARACTERS_OF_LITERAL_STRING - NumberOfCharacters >= MAXIMUM_EMAIL_LENGTH", pLiteralString);
		return pNotFound;
	}

	int string_length = istrlen(pLiteralString);
	if (NumberOfCharacters > string_length)
	{
		scriptAssertf(0, "%s:GET_FIRST_N_CHARACTERS_OF_LITERAL_STRING - NumberOfCharacters is larger than the string length", pLiteralString);
		return pNotFound;
	}

	strncpy(ReturnString, pLiteralString, NumberOfCharacters);
	ReturnString[NumberOfCharacters] = '\0';
	return ReturnString;
}


int CommandGetLengthOfStringWithThisTextLabel(const char *pTextLabel)
{
	if (TheText.DoesTextLabelExist(pTextLabel))
	{
		int string_length = CTextConversion::GetCharacterCount(TheText.Get(pTextLabel));
		return string_length;
	}
	else
	{
		scriptAssertf(0, "%s:GET_LENGTH_OF_STRING_WITH_THIS_TEXT_LABEL - string doesn't exist", pTextLabel);
		return 0;
	}
}

const char*GetStringFromStringInternal(const char *pText, int startPoint, int endPoint, bool bBytes, int maxBytesWhenCopyingCharacters)
{
	char CopyString[MAXIMUM_EMAIL_LENGTH];
	static char ReturnString[MAXIMUM_EMAIL_LENGTH];

	if (pText == NULL)
	{
		scriptAssertf(0, "%s:GET_STRING_FROM_STRING - string is a NULL pointer", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		return pNotFound;
	}

	if (startPoint < 0)
	{
		scriptAssertf(0, "%s:GET_STRING_FROM_STRING - startPoint is negative", pText);
		return pNotFound;
	}

	if (endPoint >= MAXIMUM_EMAIL_LENGTH)
	{
		scriptAssertf(0, "%s:GET_STRING_FROM_STRING - startPoint >= MAXIMUM_EMAIL_LENGTH", pText);
		return pNotFound;
	}
    if (endPoint < 0)
    {
        scriptAssertf(0, "%s:GET_STRING_FROM_STRING - endPoint negative: %d!", pText, endPoint);
        return pNotFound;
    }

//	if (TheText.DoesTextLabelExist(pTextLabel))
//	{
	//	const char* pActualMessage = charToAscii(TheText.Get(pTextLabel));

		int string_length;
		
		if (bBytes)
		{
			string_length = istrlen(pText/*pActualMessage*/);
		}
		else
		{
			string_length = CountUtf8Characters(pText/*pActualMessage*/);
		}

		if (endPoint > string_length)
		{
			scriptAssertf(0, "%s:GET_STRING_FROM_STRING - endPoint is larger than the string length", pText);
			return pNotFound;
		}

		s32 b = 0;

		if (bBytes)
		{
			strncpy(CopyString, pText, endPoint);
			CopyString[endPoint] = '\0';

			for (s32 a = startPoint; a <= endPoint; a++)
			{
				ReturnString[b++] = CopyString[a];
			}
		}
		else
		{
			CTextConversion::StrcpyWithCharacterAndByteLimits(CopyString, pText, endPoint, -1);

			// ensure we start the string at the character we want
			s32 iUtf8StartPoint = 0;
			s32 iUtf8EndPoint = 0;

			// work out the actual utf8 character positions of the start and end points
			for (s32 iCount = 0; iCount < startPoint; iCount++)
				iUtf8StartPoint += CTextConversion::ByteCountForCharacter(&CopyString[iUtf8StartPoint]);

			bool bFinished = false;
			for (s32 iCount = 0; !bFinished && (iCount < endPoint); iCount++)
			{
				u32 byteCountForThisCharacter = CTextConversion::ByteCountForCharacter(&CopyString[iUtf8EndPoint]);

				if (maxBytesWhenCopyingCharacters > 0)
				{
					if (iUtf8EndPoint >= iUtf8StartPoint)
					{
						if ( (iUtf8EndPoint - iUtf8StartPoint + byteCountForThisCharacter) > maxBytesWhenCopyingCharacters)
						{
							if (iUtf8EndPoint > 0)
							{
								iUtf8EndPoint--;	//	Graeme 27.11.13 - rather than changing the <= in the for loop below, I'll subtract one here so that the first byte of the next character isn't copied
							}
							bFinished = true;
						}
					}
				}

				if (!bFinished)
			{
					iUtf8EndPoint += byteCountForThisCharacter;
				}
			}

			for (s32 a = iUtf8StartPoint; a <= iUtf8EndPoint; a++)	//	Graeme 27.11.13 - maybe this <= should have been < but I don't want to change any existing behaviour now
			{
				ReturnString[b++] = CopyString[a];
			}
		}

		ReturnString[b] = '\0';

		return ReturnString;
// 	}
// 	else
// 	{
// 		Assertf(0, "%s:GET_STRING_FROM_STRING - string doesn't exist", pTextLabel);
// 		return pNotFound;
// 	}
}


const char*CommandGetStringFromString(const char *pText, int startPoint, int endPoint)
{
	return GetStringFromStringInternal(pText, startPoint, endPoint, false, -1);
}

const char*CommandGetStringFromStringWithByteLimit(const char *pText, int startPoint, int endPoint, int maxBytesToCopy)
{
	return GetStringFromStringInternal(pText, startPoint, endPoint, false, maxBytesToCopy);
}

const char*CommandGetStringFromStringBytes(const char *pText, int startPoint, int endPoint)
{
	return GetStringFromStringInternal(pText, startPoint, endPoint, true, -1);
}

int CommandGetLengthOfLiteralString(const char *pLiteralString)
{
	if (!pLiteralString)
	{
		return 0;
	}
	
	int string_length = CountUtf8Characters(pLiteralString);  // length of a literal string should be in characters
	return string_length;
}

int CommandGetLengthOfLiteralStringInBytes(const char *pLiteralString)
{
	if (!pLiteralString)
	{
		return 0;
	}

	int string_length = istrlen(pLiteralString);
	return string_length;
}


const char*CommandGetStringFromHashKey(int TextLabelHashKey)
{
	char NumberAsString[32];
	formatf_n(NumberAsString, "%d", TextLabelHashKey);

	static char cReturnString[MAXIMUM_EMAIL_LENGTH];
	safecpy( cReturnString, TheText.Get(TextLabelHashKey, NumberAsString) );

	return (const char *)&cReturnString;
}

int CommandGetNthIntegerInString(const char *pTextLabel, int CharacterIndex)
{
	if (TheText.DoesTextLabelExist(pTextLabel))
	{
		char const * cActualMessage = TheText.Get(pTextLabel);

		int string_length = istrlen(cActualMessage);
		if (CharacterIndex < 0)
		{
			scriptAssertf(0, "%s:GET_NTH_INTEGER_IN_STRING - CharacterIndex is negative", pTextLabel);
			return -1;
		}
		if (CharacterIndex >= string_length)
		{
			scriptAssertf(0, "%s:GET_NTH_INTEGER_IN_STRING - CharacterIndex is larger than the string length", pTextLabel);
			return -1;
		}

		const char number_as_char = cActualMessage[CharacterIndex];
		if ( (number_as_char >= '0') && (number_as_char <= '9') )
		{
			return (number_as_char - '0');
		}
		scriptAssertf(0, "%s:GET_NTH_INTEGER_IN_STRING - The specified character is not an integer", pTextLabel);
		return -1;
	}
	else
	{
		scriptAssertf(0, "%s:GET_NTH_INTEGER_IN_STRING - Text Label doesn't exist", pTextLabel);
		return -1;
	}
}


bool CommandIsHudPreferenceSwitchedOn()
{
	if (CPauseMenu::GetMenuPreference(PREF_DISPLAY_HUD))
	{
		return true;
	}
	else
	{
		return false;
	}
}


bool CommandIsRadarPreferenceSwitchedOn()
{
	if (CPauseMenu::GetMenuPreference(PREF_RADAR_MODE))
	{
		return true;
	}
	else
	{
		return false;
	}
}

bool CommandIsSubtitlePreferenceSwitchedOn()
{
	if (CPauseMenu::GetMenuPreference(PREF_SUBTITLES))
	{
		return true;
	}
	else
	{
		return false;
	}
}

void CommandDisplayHud(bool bDisplayHudFlag)
{
	if (bDisplayHudFlag)
	{
#if __DEV
		if (!CScriptHud::bDisplayHud)
		{
			uiDisplayf("DISPLAY_HUD TRUE called by script %s", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		}
#endif // __DEV

		CScriptHud::bDisplayHud = true;
	}
	else
	{
#if __DEV
		if (CScriptHud::bDisplayHud)
		{
			uiDisplayf("DISPLAY_HUD FALSE called by script %s", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		}
#endif // __DEV

		CScriptHud::bDisplayHud = false;
	}
	CGameStreamMgr::GetGameStream()->SetRenderEnabledGlobal( CScriptHud::bDisplayHud );
}

// name:		CommandDisplayHudWhenNotInStateOfPlayThisFrame
// description: allows the hud to display even when not in a state of play this frame
void CommandDisplayHudWhenNotInStateOfPlayThisFrame()
{
	CScriptHud::ms_bDisplayHudWhenNotInStateOfPlayThisFrame.GetWriteBuf() = true;  // allow the hud to display this frame even if the player is dead/arrested
}

// name:		CommandDisplayHudWhenPausedThisFrame
// description: allows the hud to display even when paused this frame
void CommandDisplayHudWhenPausedThisFrame()
{
	CScriptHud::ms_bDisplayHudWhenPausedThisFrame.GetWriteBuf() = true;
}

void CommandSetRouteFlashing(bool UNUSED_PARAM(bOnOff))
{
// 	CHud::ResetFlash();
// 	CGps::SetFlashing(bOnOff);
}

void CommandOverrideAreaVehicleStreetNameTimeOn(s32 iTimeInMs)
{
	CScriptHud::ms_iOverrideTimeForAreaVehicleNames = iTimeInMs;
}

void CommandOverrideAreaVehicleStreetNameTimeOff()
{
	CScriptHud::ms_iOverrideTimeForAreaVehicleNames = -1;
}

void CommandForceReticleMode(s32 iMode)
{
	if (iMode != 0)
	{
		CScriptHud::iScriptReticleMode = iMode;
	}
	else
	{
		CScriptHud::iScriptReticleMode = -1;
	}
}

void CommandSetUseVehicleTargetingReticule(bool bUseVehicleTargetingReticule)
{
	CScriptHud::bUseVehicleTargetingReticule = bUseVehicleTargetingReticule;
}

void CommandAddValidVehicleHitHash(s32 uVehicleHash)
{
	CNewHud::GetReticule().AddValidVehicleHitHash(uVehicleHash);
}

void CommandClearValidVehicleHitHashes()
{
	CNewHud::GetReticule().ClearValidVehicleHitHashes();
}

void CommandAddNextMessageToPreviousBriefs(bool bAddToPrevBriefsFlag)
{
	CScriptHud::SetAddNextMessageToPreviousBriefs(bAddToPrevBriefsFlag);
}

void CommandForceNextMessageToPreviousBriefsList(s32 PreviousBriefsOverride)
{
	if (scriptVerifyf(PreviousBriefsOverride >= PREVIOUS_BRIEF_NO_OVERRIDE && PreviousBriefsOverride <= PREVIOUS_BRIEF_FORCE_GOD_TEXT, "FORCE_NEXT_MESSAGE_TO_PREVIOUS_BRIEFS_LIST - parameter is out of range"))
	{
		CScriptHud::SetNextMessagePreviousBriefOverride(static_cast<ePreviousBriefOverride>(PreviousBriefsOverride));
	}
}


//	Text commands
float CommandGetRenderedTextPaddingSize()
{
	return (CScriptHud::ms_FormattingForNextDisplayText.GetTextPaddingOnScreen());
}

float CommandGetRenderedCharacterHeight(float fTextScale, s32 iTextFont)
{
	if (fTextScale != -1.0f)
	{
		CTextLayout ScriptTextLayoutForWidth;
		ScriptTextLayoutForWidth.SetStyle((u8)iTextFont);
		ScriptTextLayoutForWidth.SetScale(Vector2(fTextScale, fTextScale));

		return (ScriptTextLayoutForWidth.GetCharacterHeight());
	}
	else
	{
		return (CScriptHud::ms_FormattingForNextDisplayText.GetCharacterHeightOnScreen());
	}
}

float CommandGetStringWidth(const char *pTextLabel)
{
	CScriptTextConstruction::BeginGetScreenWidthOfDisplayText(pTextLabel);
	return CScriptTextConstruction::EndGetScreenWidthOfDisplayText(true);
}

float CommandGetStringWidthWithNumber(const char *pTextLabel, int NumberToInsert)
{
	CScriptTextConstruction::BeginGetScreenWidthOfDisplayText(pTextLabel);
	CScriptTextConstruction::AddInteger(NumberToInsert);
	return CScriptTextConstruction::EndGetScreenWidthOfDisplayText(true);
}

float CommandGetStringWidthWithString(const char *pTextLabel, const char *pSmallString)
{
	CScriptTextConstruction::BeginGetScreenWidthOfDisplayText(pTextLabel);
	CScriptTextConstruction::AddSubStringLiteralString(pSmallString);
	return CScriptTextConstruction::EndGetScreenWidthOfDisplayText(true);
}


int CommandGetNumberLinesWithSubstrings(float DisplayAtX, float DisplayAtY, const char *pTextLabel, const char *pSubString1Label, const char *pSubString2Label)
{
	CScriptTextConstruction::BeginGetNumberOfLinesForString(pTextLabel);
	if (pSubString1Label && (strlen(pSubString1Label) > 0) )
	{
		CScriptTextConstruction::AddSubStringTextLabel(pSubString1Label);
	}

	if (pSubString2Label && (strlen(pSubString2Label) > 0) )
	{
		CScriptTextConstruction::AddSubStringTextLabel(pSubString2Label);
	}
	return CScriptTextConstruction::EndGetNumberOfLinesForString(DisplayAtX, DisplayAtY);
}

void CommandDisplayText(float DisplayAtX, float DisplayAtY, const char *pTextLabel)
{
	CScriptTextConstruction::BeginDisplayText(pTextLabel);
	CScriptTextConstruction::EndDisplayText(DisplayAtX, DisplayAtY, false);
}

void CommandSetWidescreenFormat(int WidescreenFormat)
{
	// store the current script widescreen format to use for the next script text/graphic
	CScriptHud::CurrentScriptWidescreenFormat = (eWIDESCREEN_FORMAT)WidescreenFormat;
	CScriptHud::bScriptHasChangedWidescreenFormat = true;
}

s32 CommandGetWidescreenFormat()
{
	// returns the current widescreen format set by script
	return (s32)CScriptHud::CurrentScriptWidescreenFormat;
}


void CommandSetTextScale(float TextXScale, float TextYScale)
{
	CScriptHud::ms_FormattingForNextDisplayText.SetTextScale(TextXScale, TextYScale);
}


void CommandSetTextColour(int Red, int Green, int Blue, int Alpha)
{
	CRGBA tempRGBA = CRGBA((u8) Red, (u8) Green, (u8) Blue, (u8) Alpha);
	CScriptHud::ms_FormattingForNextDisplayText.SetColor(tempRGBA);
}

void CommandSetTextHudColour(s32 HudColour, s32 AlphaOverride)
{
	if(SCRIPT_VERIFY( ( (HudColour >= 0) && (HudColour < HUD_COLOUR_MAX_COLOURS ) ), "SET_TEXT_HUD_COLOUR - Invalid Index"))
	{
		CRGBA rgba = CHudColour::GetRGBA((eHUD_COLOURS)HudColour);
		if (AlphaOverride >= 0)
		{
			rgba.SetAlpha(AlphaOverride);
		}
		CScriptHud::ms_FormattingForNextDisplayText.SetColor(rgba);
	}
}

void CommandSetTextJustify(bool bTextJustifyFlag)
{
	if (bTextJustifyFlag)
	{
		CScriptHud::ms_FormattingForNextDisplayText.SetTextJustification(FONT_JUSTIFY);
	}
}

void CommandSetTextCentre(bool bTextCentreFlag)
{
	if (bTextCentreFlag)
	{
		CScriptHud::ms_FormattingForNextDisplayText.SetTextJustification(FONT_CENTRE);
	}
}

void CommandSetTextRightJustify(bool bTextRightJustifyFlag)
{
	if (bTextRightJustifyFlag)
	{
		CScriptHud::ms_FormattingForNextDisplayText.SetTextJustification(FONT_RIGHT);
	}
}

void CommandSetTextJustification(s32 Justification)
{
	CScriptHud::ms_FormattingForNextDisplayText.SetTextJustification(static_cast<u8>(Justification));
}

void CommandSetTextToUseTextFileColours(bool bUseTextColours)
{
	CScriptHud::ms_FormattingForNextDisplayText.SetUseTextFileColours(bUseTextColours);
}

void CommandSetTextLineHeightMult(float UNUSED_PARAM(LineHeight))
{
	//CScriptHud::ms_FormattingForNextDisplayText.ScriptTextLineHeightMult = LineHeight;
}

void CommandSetTextWrap(float textWrapStart, float textWrapEnd)
{
	CScriptHud::ms_FormattingForNextDisplayText.SetTextWrap(textWrapStart, textWrapEnd);
}

void CommandSetTextBackground(bool bTextBackgroundFlag)
{
	CScriptHud::ms_FormattingForNextDisplayText.SetTextBackground(bTextBackgroundFlag);
}

void CommandSetTextLeading(s32 iTextLeading)
{
	CScriptHud::ms_FormattingForNextDisplayText.SetTextLeading(static_cast<u8>(iTextLeading));
}

void CommandSetTextUseUnderScore(bool UNUSED_PARAM(bTextUnderScoreFlag))
{
//	CScriptHud::ms_FormattingForNextDisplayText.SetUsesUnderScore(bTextUnderScoreFlag);
}


void CommandSetTextProportional(bool UNUSED_PARAM(bTextProportionalFlag))
{
//	CScriptHud::ms_FormattingForNextDisplayText.SetTextProportional(bTextProportionalFlag);
}


// THIS COMMAND IS DUE TO GET DITCHED!
void CommandLoadTextFont(int UNUSED_PARAM(TextFont))
{
#if __DEV
	uiDisplayf("FONT: Script requested to LOAD font");
#endif

}

// THIS COMMAND IS DUE TO GET DITCHED!
bool CommandIsFontLoaded(int UNUSED_PARAM(TextFont))
{
	return true;
//	return CText::IsFontLoaded(TextFont);
}

void CommandSetTextFont(int TextFont)
{
	CScriptHud::ms_FormattingForNextDisplayText.SetTextFontStyle(TextFont);
}

void CommandDisplayTextWithNumber(float DisplayAtX, float DisplayAtY, const char *pTextLabel, int NumberToDisplay)
{
	CScriptTextConstruction::BeginDisplayText(pTextLabel);
	CScriptTextConstruction::AddInteger(NumberToDisplay);
	CScriptTextConstruction::EndDisplayText(DisplayAtX, DisplayAtY, false);
}

void CommandDisplayTextWith2Numbers(float DisplayAtX, float DisplayAtY, const char *pTextLabel, int FirstNumberToDisplay, int SecondNumberToDisplay)
{
	CScriptTextConstruction::BeginDisplayText(pTextLabel);
	CScriptTextConstruction::AddInteger(FirstNumberToDisplay);
	CScriptTextConstruction::AddInteger(SecondNumberToDisplay);
	CScriptTextConstruction::EndDisplayText(DisplayAtX, DisplayAtY, false);
}

void CommandDisplayTextWith3Numbers(float DisplayAtX, float DisplayAtY, const char *pTextLabel, int FirstNumberToDisplay, int SecondNumberToDisplay, int ThirdNumberToDisplay)
{
	CScriptTextConstruction::BeginDisplayText(pTextLabel);
	CScriptTextConstruction::AddInteger(FirstNumberToDisplay);
	CScriptTextConstruction::AddInteger(SecondNumberToDisplay);
	CScriptTextConstruction::AddInteger(ThirdNumberToDisplay);
	CScriptTextConstruction::EndDisplayText(DisplayAtX, DisplayAtY, false);
}

void CommandSetTextDropShadow()
{
	CScriptHud::ms_FormattingForNextDisplayText.SetTextDropShadow(true);
}

// old command - to be removed once scripts sync/update
void CommandSetTextDropshadowOld(int DropAmount, int UNUSED_PARAM(Red), int UNUSED_PARAM(Green), int UNUSED_PARAM(Blue), int UNUSED_PARAM(Alpha))
{
	if (DropAmount > 0)  // to be revised once scripts sync/update
	{
		CScriptHud::ms_FormattingForNextDisplayText.SetTextDropShadow(true);
	}
	else
	{
		CScriptHud::ms_FormattingForNextDisplayText.SetTextDropShadow(false);
	}
}

void CommandDisplayTextWithFloat(float DisplayAtX, float DisplayAtY, const char *pTextLabel, float FloatToDisplay, int NumOfDecimalPlaces)
{
	CScriptTextConstruction::BeginDisplayText(pTextLabel);
	CScriptTextConstruction::AddFloat(FloatToDisplay, NumOfDecimalPlaces);
	CScriptTextConstruction::EndDisplayText(DisplayAtX, DisplayAtY, false);
}

void CommandDisplayTextWithLiteralString(float DisplayAtX, float DisplayAtY, const char *pTextLabel, const char *pLiteralString)
{
	CScriptTextConstruction::BeginDisplayText(pTextLabel);
	CScriptTextConstruction::AddSubStringLiteralString(pLiteralString);
	CScriptTextConstruction::EndDisplayText(DisplayAtX, DisplayAtY, false);
}

void CommandDisplayTextWithTwoLiteralStrings(float DisplayAtX, float DisplayAtY, const char *pTextLabel, const char *pLiteralString1, const char *pLiteralString2)
{
	CScriptTextConstruction::BeginDisplayText(pTextLabel);
	CScriptTextConstruction::AddSubStringLiteralString(pLiteralString1);
	CScriptTextConstruction::AddSubStringLiteralString(pLiteralString2);
	CScriptTextConstruction::EndDisplayText(DisplayAtX, DisplayAtY, false);
}

void CommandDisplayTextWithString(float DisplayAtX, float DisplayAtY, const char *pTextLabel, const char *pShortTextLabel)
{
	CScriptTextConstruction::BeginDisplayText(pTextLabel);
	CScriptTextConstruction::AddSubStringTextLabel(pShortTextLabel);
	CScriptTextConstruction::EndDisplayText(DisplayAtX, DisplayAtY, false);
}

void CommandDisplayTextWithTwoStrings(float DisplayAtX, float DisplayAtY, const char *pTextLabel, const char *pShortTextLabel1, const char *pShortTextLabel2)
{
	CScriptTextConstruction::BeginDisplayText(pTextLabel);
	CScriptTextConstruction::AddSubStringTextLabel(pShortTextLabel1);
	CScriptTextConstruction::AddSubStringTextLabel(pShortTextLabel2);
	CScriptTextConstruction::EndDisplayText(DisplayAtX, DisplayAtY, false);
}

void CommandDisplayTextWithBlipName(float DisplayAtX, float DisplayAtY, const char *pTextLabel, s32 blipIndex)
{
	CScriptTextConstruction::BeginDisplayText(pTextLabel);
	CScriptTextConstruction::AddSubStringBlipName(blipIndex);
	CScriptTextConstruction::EndDisplayText(DisplayAtX, DisplayAtY, false);
}

void CommandSetTextOutline()
{
	CScriptHud::ms_FormattingForNextDisplayText.SetTextEdge(true);
}

// command no longer available
void CommandSetTextEdge(int UNUSED_PARAM(EdgeAmount), int UNUSED_PARAM(Red), int UNUSED_PARAM(Green), int UNUSED_PARAM(Blue), int UNUSED_PARAM(Alpha))
{
/*	CRGBA tempRGBA = CRGBA(0,0,0,255);

	if (EdgeAmount > 0)
	{
		CScriptHud::ms_FormattingForNextDisplayText.SetTextEdge(tempRGBA, 0);
	}
	else
	{
		CScriptHud::ms_FormattingForNextDisplayText.SetTextEdge(tempRGBA, 0);
	}*/
}

void CommandSetTextRenderId(int renderID)
{
	//CText::Flush();  // flush the buffer before we change the render target
	CScriptHud::scriptTextRenderID = renderID;
	gRenderTargetMgr.UseRenderTarget((CRenderTargetMgr::RenderTargetId)renderID);
	CScriptHud::CurrentScriptWidescreenFormat = WIDESCREEN_FORMAT_STRETCH;
}

int CommandGetDefaultScriptRenderTargetRenderId()
{
	return (CRenderTargetMgr::RTI_MainScreen);
}

int CommandGetScriptRenderTargetRenderId()
{
#if ENABLE_LEGACY_SCRIPTED_RT_SUPPORT
	return (CRenderTargetMgr::RTI_Scripted);
#else // ENABLE_LEGACY_SCRIPTED_RT_SUPPORT
	scriptAssertf(false,"%s:GET_SCRIPT_RENDERTARGET_RENDER_ID has been removed, please use the new named rendertarget system",CTheScripts::GetCurrentScriptNameAndProgramCounter());
	return -1;
#endif // ENABLE_LEGACY_SCRIPTED_RT_SUPPORT
}

int CommandGetNamedRenderTargetid(const char *name)
{
	scriptAssertf(gRenderTargetMgr.IsNamedRenderTargetValid(name) == true, "%s:GET_NAMED_RENDERTARGET_RENDER_ID - Badly named : %s. please remove \"script_rt\" from the target name. ", CTheScripts::GetCurrentScriptNameAndProgramCounter(), name);
	scrThreadId scriptThreadId = CTheScripts::GetCurrentGtaScriptThread()->GetThreadId();
	u32 rtId = gRenderTargetMgr.GetNamedRendertargetId(name,scriptThreadId);
	scriptAssertf(rtId != 0, "%s:GET_NAMED_RENDERTARGET_RENDER_ID - unknown Rendertarget : %s. The Rendertarget was not registered. ", CTheScripts::GetCurrentScriptNameAndProgramCounter(), name);
	
	return (int)rtId;
}

bool CommandRegisterNamedRendertarget(const char *name, bool delay)
{
	scriptAssertf(gRenderTargetMgr.IsNamedRenderTargetValid(name) == true, "%s:REGISTER_NAMED_RENDERTARGET - Badly named : %s. please remove \"script_rt\" from the target name. ", CTheScripts::GetCurrentScriptNameAndProgramCounter(), name);
	scrThreadId scriptThreadId = CTheScripts::GetCurrentGtaScriptThread()->GetThreadId();
	if (gRenderTargetMgr.RegisterNamedRendertarget(name,scriptThreadId))
	{
		if(gRenderTargetMgr.SetNamedRendertargetDelayLoad(atFinalHashString(name),scriptThreadId, delay))
		{
			REPLAY_ONLY(ReplayMovieControllerNew::OnRegisterNamedRenderTarget(name, scriptThreadId));
			return true;
		}
	}
	return false;
}

bool CommmandIsNamedRendertargetRegistered(const char *name)
{
	scriptAssertf(gRenderTargetMgr.IsNamedRenderTargetValid(name) == true, "%s:IS_NAMED_RENDERTARGET_REGISTERED - Badly named : %s. please remove \"script_rt\" from the target name. ", CTheScripts::GetCurrentScriptNameAndProgramCounter(), name);
	scrThreadId scriptThreadId = CTheScripts::GetCurrentGtaScriptThread()->GetThreadId();
	return gRenderTargetMgr.GetNamedRendertargetId(name,scriptThreadId) != 0;
}

bool CommmandIsNamedRendertargetLinked(int ModelHashKey)
{
	fwModelId modelId;
	CBaseModelInfo* modelinfo = CModelInfo::GetBaseModelInfoFromHashKey(ModelHashKey,&modelId);
	scriptAssertf(modelinfo, "%s:IS_NAMED_RENDERTARGET_LINKED - unknown modelinfo", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	if( modelinfo )
	{
		scrThreadId scriptThreadId = CTheScripts::GetCurrentGtaScriptThread()->GetThreadId();
		modelinfo->SetCarryScriptedRT(true);
		modelinfo->SetScriptId(scriptThreadId);

		if( modelinfo->GetDrawable() )
		{
			return gRenderTargetMgr.IsNamedRenderTargetLinked(modelId.GetModelIndex(),scriptThreadId);
		}
	}

	return false;
}

bool CommandReleaseNamedRendertarget(const char *name)
{
	scriptAssertf(gRenderTargetMgr.IsNamedRenderTargetValid(name) == true, "%s:RELEASE_NAMED_RENDERTARGET - Badly named : %s. please remove \"script_rt\" from the target name. ", CTheScripts::GetCurrentScriptNameAndProgramCounter(), name);
	scrThreadId scriptThreadId = CTheScripts::GetCurrentGtaScriptThread()->GetThreadId();

	REPLAY_ONLY(ReplayMovieControllerNew::OnReleaseNamedRenderTarget(name));

	return gRenderTargetMgr.ReleaseNamedRendertarget(name,scriptThreadId);
}

void CommandLinkNamedRenderTarget(int ModelHashKey)
{
	fwModelId modelId;
	CBaseModelInfo* modelinfo = CModelInfo::GetBaseModelInfoFromHashKey(ModelHashKey,&modelId);
	scriptAssertf(modelinfo, "%s:LINK_NAMED_RENDERTARGET - unknown modelinfo", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	if( modelinfo )
	{
		scrThreadId scriptThreadId = CTheScripts::GetCurrentGtaScriptThread()->GetThreadId();
		modelinfo->SetCarryScriptedRT(true);
		modelinfo->SetScriptId(scriptThreadId);
	
		CRenderTargetMgr::namedRendertarget const* pRT = nullptr;
		if( modelinfo->GetDrawable() )
		{
			pRT = gRenderTargetMgr.LinkNamedRendertargets(modelId.GetModelIndex(),scriptThreadId);
		}

		REPLAY_ONLY(ReplayMovieControllerNew::OnLinkNamedRenderTarget(ModelHashKey, scriptThreadId, pRT));
	}
}

bool CommandSetupNamedRendertargetDelayLoad(const char *name, const char *textureDictionaryName, const char *textureName)
{
	scriptAssertf(gRenderTargetMgr.IsNamedRenderTargetValid(name) == true, "%s:SETUP_DELAYEDLOAD_NAMED_RENDERTARGET - Badly named : %s. please remove \"script_rt\" from the target name. ", CTheScripts::GetCurrentScriptNameAndProgramCounter(), name);
	scrThreadId scriptThreadId = CTheScripts::GetCurrentGtaScriptThread()->GetThreadId();
	
	grcTexture* texture = graphics_commands::GetTextureFromStreamedTxd(textureDictionaryName, textureName);
	
	return gRenderTargetMgr.SetupNamedRendertargetDelayLoad(atFinalHashString(name),scriptThreadId, texture);
}


void AddToPreviousBrief(const char* UNUSED_PARAM(pLiteralText), const char* UNUSED_PARAM(pExtraString1), const char* UNUSED_PARAM(pExtraString2), bool UNUSED_PARAM(bUsesUnderscore))
{
	scriptWarningf("AddToPreviousBrief - Graeme has had to remove the code for this command so ticker messages won't currently be getting added to the previous briefs. Replacement commands will be added soon.");
/*
	if (pLiteralText[0] != '\0')
	{
		// #if __DEV
		// 		Displayf("Text to add to previous brief: %s\n", pLiteralText);
		// #endif // __DEV
		s32 iNewLiteralStringIndex1 = CScriptHud::ScriptLiteralStrings.SetLiteralString(pLiteralText, true);
		s32 iNewLiteralStringIndex2 = -1;

		// create a 2nd literal string using the contents of the two "extra" strings.
		// this allows us to be able to pass more strings into this command but only still have 2 literal strings
		// aslong as the sizes are small enough (which I'm assured by Script they will be)
		char cJoinedExtraLiteralString[SCRIPT_MAX_CHARS_IN_LITERAL_STRING];
		if (pExtraString1)  // if we have atleast one extra string
		{
			safecpy(cJoinedExtraLiteralString, pExtraString1, SCRIPT_MAX_CHARS_IN_LITERAL_STRING);

			if (pExtraString2)
			{
				safecat(cJoinedExtraLiteralString, pExtraString2, SCRIPT_MAX_CHARS_IN_LITERAL_STRING);
			}

			iNewLiteralStringIndex2 = CScriptHud::ScriptLiteralStrings.SetLiteralString(cJoinedExtraLiteralString, true);
		}

		if (iNewLiteralStringIndex1 != -1)
		{
			char *pString = TheText.Get("BRIEF_STRING");

			CScriptHud::ScriptLiteralStrings.SetPersistentLiteralOccursInSubtitles(iNewLiteralStringIndex1, false);

			if (iNewLiteralStringIndex2 != -1)
			{
				CScriptHud::ScriptLiteralStrings.SetPersistentLiteralOccursInSubtitles(iNewLiteralStringIndex2, false);

				pString = TheText.Get("BRIEF_STRING2");
			}

			CMessages::AddToPreviousBriefArray(pString, -1, NULL, -1, NULL, -1, -1, -1, -1, -1, -1, -1, iNewLiteralStringIndex1, iNewLiteralStringIndex2, bUsesUnderscore);
		}
	}
*/
}

void CommandAddToPreviousBrief(const char *pLiteralText, const char *pExtraString1, const char *pExtraString2)
{
	AddToPreviousBrief(pLiteralText, pExtraString1, pExtraString2, false);
}

void CommandAddToPreviousBriefWithUnderscore(const char *pLiteralText, const char *pExtraString1, const char *pExtraString2)
{
	AddToPreviousBrief(pLiteralText, pExtraString1, pExtraString2, true);
}

//
//	Help Message commands:
//
void CommandHideHelpTextThisFrame()
{
#if RSG_PC
	if(ioKeyboard::ImeIsInProgress())
		return;
#endif // RSG_PC

//	uiDisplayf("HIDE_HELP_TEXT_THIS_FRAME called by script %s", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	CNewHud::SetHudComponentToBeHidden(NEW_HUD_HELP_TEXT);
}

bool CommandIsIMEInProgress()
{
#if RSG_PC
	return ioKeyboard::ImeIsInProgress();
#else
	return false;
#endif
}

void CommandDisplayHelpTextThisFrame(const char *pTextLabel, bool UNUSED_PARAM(curvedWindow))
{
	char *pString = TheText.Get(pTextLabel);
	CHelpMessage::SetMessageText(HELP_TEXT_SLOT_STANDARD, pString);
}

void CommandHudForceWeaponWheel(bool bOnOrOff)
{
	CNewHud::SetWeaponWheelForcedByScript(bOnOrOff);
}

void SetWeaponWheelSpecialVehicleForcedByScript() 
{
	CNewHud::SetWeaponWheelSpecialVehicleForcedByScript();
}

void CommandHudSuppressWeaponWheelResultsThisFrame()
{
	CNewHud::SetWeaponWheelSuppressResultsThisFrame();
}

// EXPECTS AN ARRAY of SWeaponWheelScriptNodes
// BUT due to script shenanigans, needs to adjust
void CommandHudSetWeaponWheelContents( scrArrayBaseAddr& ArrayWithScriptNumbersInFront )
{
	int iNumOfEntries = scrArray::GetCount(ArrayWithScriptNumbersInFront); // this is the number of entries (what we want)
	SWeaponWheelScriptNode* pArrayAsProperType = scrArray::GetElements<SWeaponWheelScriptNode>(ArrayWithScriptNumbersInFront);

	CNewHud::SetWeaponWheelContents(pArrayAsProperType, iNumOfEntries);
}

int CommandHudGetWeaponWheelCurrentlyHighlighted()
{
	return CNewHud::GetWheelCurrentlyHighlightedWeapon();
}

bool CommandHudGetWeaponWheelHasASlotWithMultipleWeapons()
{
	return CNewHud::GetWheelHasASlotWithMultipleWeapons();
}

void CommandHudSetWeaponWheelTopSlot(int WeaponAsHash)
{
	CNewHud::SetWheelMemoryWeapon( static_cast<u32>(WeaponAsHash) );
}

int CommandHudGetWeaponWheelTopSlot(int iSlotIndex)
{
	if(scriptVerifyf(iSlotIndex >= 0 && iSlotIndex < MAX_WHEEL_SLOTS, "HUD_GET_WEAPON_WHEEL_TOP_SLOT passed invalid slot index of %d", iSlotIndex))
	{
		return CNewHud::GetWheelMemoryWeaponAtSlot(static_cast<u8>(iSlotIndex));
	}

	return 0;
}

void CommandHudShowingCharacterSwitch(bool bOnOrOff)
{
	CNewHud::SetShowingCharacterSwitch(bOnOrOff);
}

void CommandPrintHelp(const char *pTextLabel)
{
	CScriptTextConstruction::BeginDisplayHelp(pTextLabel);
	CScriptTextConstruction::EndDisplayHelp(HELP_TEXT_SLOT_STANDARD, false, true, -1);
}

void CommandPrintHelpForever(const char *pTextLabel)
{
	CScriptTextConstruction::BeginDisplayHelp(pTextLabel);
	CScriptTextConstruction::EndDisplayHelp(HELP_TEXT_SLOT_STANDARD, true, true, -1);
}

void CommandPrintHelpWithNumber(const char *pTextLabel, int NumberToDisplay)
{
	CScriptTextConstruction::BeginDisplayHelp(pTextLabel);
	CScriptTextConstruction::AddInteger(NumberToDisplay);
	CScriptTextConstruction::EndDisplayHelp(HELP_TEXT_SLOT_STANDARD, false, true, -1);
}

void CommandPrintHelpForeverWithNumber(const char *pTextLabel, int NumberToDisplay)
{
	CScriptTextConstruction::BeginDisplayHelp(pTextLabel);
	CScriptTextConstruction::AddInteger(NumberToDisplay);
	CScriptTextConstruction::EndDisplayHelp(HELP_TEXT_SLOT_STANDARD, true, true, -1);
}

void CommandPrintHelpWithString(const char *pTextLabel, const char *pSubStringTextLabel)
{
	CScriptTextConstruction::BeginDisplayHelp(pTextLabel);
	CScriptTextConstruction::AddSubStringTextLabel(pSubStringTextLabel);
	CScriptTextConstruction::EndDisplayHelp(HELP_TEXT_SLOT_STANDARD, false, true, -1);
}

void CommandPrintHelpWithStringNoSound(const char *pTextLabel, const char *pSubStringTextLabel)
{
	CScriptTextConstruction::BeginDisplayHelp(pTextLabel);
	CScriptTextConstruction::AddSubStringTextLabel(pSubStringTextLabel);
	CScriptTextConstruction::EndDisplayHelp(HELP_TEXT_SLOT_STANDARD, false, false, -1);
}

void CommandPrintHelpForeverWithString(const char *pTextLabel, const char *pSubStringTextLabel)
{
	CScriptTextConstruction::BeginDisplayHelp(pTextLabel);
	CScriptTextConstruction::AddSubStringTextLabel(pSubStringTextLabel);
	CScriptTextConstruction::EndDisplayHelp(HELP_TEXT_SLOT_STANDARD, true, true, -1);
}

void CommandPrintHelpForeverWithStringNoSound(const char *pTextLabel, const char *pSubStringTextLabel)
{
	CScriptTextConstruction::BeginDisplayHelp(pTextLabel);
	CScriptTextConstruction::AddSubStringTextLabel(pSubStringTextLabel);
	CScriptTextConstruction::EndDisplayHelp(HELP_TEXT_SLOT_STANDARD, true, false, -1);
}

void CommandPrintHelpWithStringAndNumber(const char *pTextLabel, const char *pSubStringTextLabel, int NumberToDisplay)
{
	CScriptTextConstruction::BeginDisplayHelp(pTextLabel);
	CScriptTextConstruction::AddSubStringTextLabel(pSubStringTextLabel);
	CScriptTextConstruction::AddInteger(NumberToDisplay);
	CScriptTextConstruction::EndDisplayHelp(HELP_TEXT_SLOT_STANDARD, false, true, -1);
}

void CommandPrintHelpForeverWithStringAndNumber(const char *pTextLabel, const char *pSubStringTextLabel, int NumberToDisplay)
{
	CScriptTextConstruction::BeginDisplayHelp(pTextLabel);
	CScriptTextConstruction::AddSubStringTextLabel(pSubStringTextLabel);
	CScriptTextConstruction::AddInteger(NumberToDisplay);
	CScriptTextConstruction::EndDisplayHelp(HELP_TEXT_SLOT_STANDARD, true, true, -1);
}

/*
void CommandPrintHelpForeverWithLiteralString(const char *pTextLabel, const char *pSubStringText)
{
	PrintHelpWithString(pTextLabel, pSubStringText, true, true);
}*/


void CommandClearHelp(bool bClearNow)
{
	uiDebugf1("CLEAR_HELP %d called by %s script", (s32)bClearNow, CTheScripts::GetCurrentScriptName());
#if __ASSERT
	//
	// simple check to catch any instances where help text is cleared every frame
	// this is a waste and seems to be a common issue and we need to catch it here
	// otherwise it could go un-noticed.  it will cause help text not to appear if its
	// cleared after setting it and also is inefficient as it will cause an ActionScript invoke
	// will only assert if its cleared 10 frames in a row to deal with multiple scripts
	//
	static u32 iPreviousFrameHelpCleared = 0;
	static u32 iTimesHelpTextClearedInARow = 0;
	scrThreadId scriptThreadId = THREAD_INVALID;
	
	if (CTheScripts::GetCurrentGtaScriptThread())
	{
		scriptThreadId = CTheScripts::GetCurrentGtaScriptThread()->GetThreadId();  // check we are testing the same script
	}

	static scrThreadId previousScriptThreadId = scriptThreadId;

	if (iPreviousFrameHelpCleared == GRCDEVICE.GetFrameCounter()-1 && previousScriptThreadId == scriptThreadId)
	{
		iTimesHelpTextClearedInARow++;

		if (iTimesHelpTextClearedInARow > 10)
		{
			scriptAssertf(0, "Help text is getting cleared every frame by script %s", CTheScripts::GetCurrentScriptName());
			iTimesHelpTextClearedInARow = 0;
		}
	}
	else
	{
		iTimesHelpTextClearedInARow = 0;
	}

	iPreviousFrameHelpCleared = GRCDEVICE.GetFrameCounter(); 
	previousScriptThreadId = scriptThreadId;

#endif // __ASSERT

	CHelpMessage::Clear(HELP_TEXT_SLOT_STANDARD, bClearNow);
}

bool CommandIsHelpMessageOnScreen()
{
	return (CHelpMessage::GetDirectionHelpText(HELP_TEXT_SLOT_STANDARD, CHelpMessage::GetScreenPosition(HELP_TEXT_SLOT_STANDARD)) == HELP_TEXT_ARROW_NORMAL);
}

bool CommandHasScriptHiddenHelpThisFrame()
{
	return (CNewHud::IsHelpTextHiddenByScriptThisFrame(HELP_TEXT_SLOT_STANDARD));
}


bool CommandIsHelpMessageBeingDisplayed()
{
	bool bReturnValue = CHelpMessage::DoesMessageTextExist(HELP_TEXT_SLOT_STANDARD);

#if __BANK
	if (bReturnValue)
	{
		if (CNewHud::IsHelpTextHiddenByScriptThisFrame(HELP_TEXT_SLOT_STANDARD))
		{
			uiDebugf1("IS_HELP_MESSAGE_BEING_DISPLAYED() returning TRUE but help text is hidden by script via HIDE_HELP_TEXT_THIS_FRAME");
		}

		if (CScriptHud::bDontDisplayHudOrRadarThisFrame)
		{
			uiDebugf1("IS_HELP_MESSAGE_BEING_DISPLAYED() returning TRUE but help text is hidden by script via HIDE_HUD_AND_RADAR_THIS_FRAME");
		}
	}

	if (CNewHud::ms_bDebugHelpText)
	{
		if (bReturnValue)
			uiDebugf1("IS_HELP_MESSAGE_BEING_DISPLAYED() returning TRUE with string %s", (char*)CHelpMessage::GetMessageText(HELP_TEXT_SLOT_STANDARD));
		else
			uiDebugf1("IS_HELP_MESSAGE_BEING_DISPLAYED() returning FALSE");
	}
#endif // __BANK

	return (bReturnValue);
}

bool CommandIsHelpMessageFadingOut()
{
	return CHelpMessage::IsMessageFadingOut(HELP_TEXT_SLOT_STANDARD);
}

void CommandSetHelpMessageBoxSize(float UNUSED_PARAM(BoxSize))
{
	// CHelpMessage - requires support?
	//CMessages::HelpMessage.SetBoxSize(BoxSize);
}

void CommandSetHelpMessageScreenPosition(float fPosX, float fPosY)
{
	CHelpMessage::SetScreenPosition(HELP_TEXT_SLOT_STANDARD, Vector2(fPosX, fPosY));
}

void CommandSetHelpMessageWorldPosition(const scrVector & vWorldPos)
{
	CHelpMessage::SetWorldPosition(HELP_TEXT_SLOT_STANDARD, Vector3(vWorldPos));
}

void CommandSetHelpMessageStyle(s32 iNewStyle, s32 hudColour, s32 iAlpha, s32 iArrowDirection, s32 iFloatingTextOffset)
{
	CHelpMessage::SetStyle(HELP_TEXT_SLOT_STANDARD, iNewStyle, iArrowDirection, iFloatingTextOffset, hudColour, iAlpha);
}

bool CommandIsThisHelpMessageBeingDisplayed(const char *pTextLabel)
{
	CScriptTextConstruction::BeginIsThisHelpMessageBeingDisplayed(pTextLabel);
	return CScriptTextConstruction::EndIsThisHelpMessageBeingDisplayed(HELP_TEXT_SLOT_STANDARD);
}

bool CommandIsThisHelpMessageWithNumberBeingDisplayed(const char *pTextLabel, int NumberToInsert)
{
	CScriptTextConstruction::BeginIsThisHelpMessageBeingDisplayed(pTextLabel);
	CScriptTextConstruction::AddInteger(NumberToInsert);
	return CScriptTextConstruction::EndIsThisHelpMessageBeingDisplayed(HELP_TEXT_SLOT_STANDARD);
}

bool CommandIsThisHelpMessageWithStringBeingDisplayed(const char *pTextLabel, const char *pSubStringTextLabel)
{
	CScriptTextConstruction::BeginIsThisHelpMessageBeingDisplayed(pTextLabel);
	CScriptTextConstruction::AddSubStringTextLabel(pSubStringTextLabel);
	return CScriptTextConstruction::EndIsThisHelpMessageBeingDisplayed(HELP_TEXT_SLOT_STANDARD);
}

bool CommandIsThisHelpMessageWithStringAndNumberBeingDisplayed(const char *pTextLabel, const char *pSubStringTextLabel, int iNumber)
{
	CScriptTextConstruction::BeginIsThisHelpMessageBeingDisplayed(pTextLabel);
	CScriptTextConstruction::AddSubStringTextLabel(pSubStringTextLabel);
	CScriptTextConstruction::AddInteger(iNumber);
	return CScriptTextConstruction::EndIsThisHelpMessageBeingDisplayed(HELP_TEXT_SLOT_STANDARD);
}

void CommandDisplayNonMinigameHelpMessages(bool bDisplayMessages)
{
	if (CTheScripts::GetCurrentGtaScriptThread()->bIsThisAMiniGameScript)
	{
		if (bDisplayMessages)
		{
			if (CTheScripts::GetCurrentGtaScriptThread()->bThisScriptAllowsNonMiniGameHelpMessages)
			{
				scriptAssertf(0, "%s:DISPLAY_NON_MINIGAME_HELP_MESSAGES - have already called this command with TRUE for this script", CTheScripts::GetCurrentScriptNameAndProgramCounter());
			}
			else
			{
				CScriptHud::NumberOfMiniGamesAllowingNonMiniGameHelpMessages++;
				scriptAssertf(CScriptHud::NumberOfMiniGamesAllowingNonMiniGameHelpMessages < 3, "%s:DISPLAY_NON_MINIGAME_HELP_MESSAGES - didn't expect the number of minigame scripts calling this with TRUE to ever go above 2", CTheScripts::GetCurrentScriptNameAndProgramCounter());
				CTheScripts::GetCurrentGtaScriptThread()->bThisScriptAllowsNonMiniGameHelpMessages = true;
			}
		}
		else
		{
			if (CTheScripts::GetCurrentGtaScriptThread()->bThisScriptAllowsNonMiniGameHelpMessages)
			{
				if (CScriptHud::NumberOfMiniGamesAllowingNonMiniGameHelpMessages > 0)
				{
					CScriptHud::NumberOfMiniGamesAllowingNonMiniGameHelpMessages--;
				}
				else
				{
					scriptAssertf(0, "%s:DISPLAY_NON_MINIGAME_HELP_MESSAGES - setting to FALSE - expecting to decrement NumberOfMiniGamesAllowingNonMiniGameHelpMessages but it's already less than 1", CTheScripts::GetCurrentScriptNameAndProgramCounter());
				}

				CTheScripts::GetCurrentGtaScriptThread()->bThisScriptAllowsNonMiniGameHelpMessages = false;
			}
			else
			{
				scriptAssertf(0, "%s:DISPLAY_NON_MINIGAME_HELP_MESSAGES - this script has called this command with FALSE when the flag is already FALSE", CTheScripts::GetCurrentScriptNameAndProgramCounter());
			}
		}
	}
	else
	{
		scriptAssertf(0, "%s:DISPLAY_NON_MINIGAME_HELP_MESSAGES - can only call this command in minigame scripts", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

bool CommandDoesThisMiniGameScriptAllowNonMinigameHelpMessages()
{
	if (CTheScripts::GetCurrentGtaScriptThread())
	{
		if (CTheScripts::GetCurrentGtaScriptThread()->bIsThisAMiniGameScript)
		{
			if (CTheScripts::GetCurrentGtaScriptThread()->bThisScriptAllowsNonMiniGameHelpMessages)
			{
				return true;
			}
		}
		else
		{
			scriptAssertf(0, "%s:DOES_THIS_MINIGAME_SCRIPT_ALLOW_NON_MINIGAME_HELP_MESSAGES - this is not a minigame script", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		}
	}

	return false;
}



//
// floating help message commands:
//
void CommandDisplayFloatingHelpText(s32 iId, const char *pTextLabel)
{
	CScriptTextConstruction::BeginDisplayHelp(pTextLabel);
	CScriptTextConstruction::EndDisplayHelp(HELP_TEXT_SLOT_FLOATING_1+iId, false, true, -1);
}

void CommandDisplayFloatingHelpTextWithNumber(s32 iId, const char *pTextLabel, int NumberToDisplay)
{
	CScriptTextConstruction::BeginDisplayHelp(pTextLabel);
	CScriptTextConstruction::AddInteger(NumberToDisplay);
	CScriptTextConstruction::EndDisplayHelp(HELP_TEXT_SLOT_FLOATING_1+iId, false, true, -1);
}

void CommandDisplayFloatingHelpTextWithString(s32 iId, const char *pTextLabel, const char *pSubStringTextLabel)
{
	CScriptTextConstruction::BeginDisplayHelp(pTextLabel);
	CScriptTextConstruction::AddSubStringTextLabel(pSubStringTextLabel);
	CScriptTextConstruction::EndDisplayHelp(HELP_TEXT_SLOT_FLOATING_1+iId, false, true, -1);
}

void CommandDisplayFloatingHelpTextWithStringAndNumber(s32 iId, const char *pTextLabel, const char *pSubStringTextLabel, int NumberToDisplay)
{
	CScriptTextConstruction::BeginDisplayHelp(pTextLabel);
	CScriptTextConstruction::AddSubStringTextLabel(pSubStringTextLabel);
	CScriptTextConstruction::AddInteger(NumberToDisplay);
	CScriptTextConstruction::EndDisplayHelp(HELP_TEXT_SLOT_FLOATING_1+iId, false, true, -1);
}

bool CommandIsFloatingHelpTextOnScreen(s32 iId)
{
	return (CHelpMessage::GetDirectionHelpText(HELP_TEXT_SLOT_FLOATING_1+iId, CHelpMessage::GetScreenPosition(HELP_TEXT_SLOT_FLOATING_1+iId)) == HELP_TEXT_ARROW_NORMAL);
}

void CommandSetFloatingHelpTextScreenPosition(s32 iId, float fPosX, float fPosY)
{
	CHelpMessage::SetScreenPosition(HELP_TEXT_SLOT_FLOATING_1+iId, Vector2(fPosX, fPosY));
}

void CommandSetFloatingHelpTextWorldPosition(s32 iId, const scrVector & vWorldPos)
{
	CHelpMessage::SetWorldPosition(HELP_TEXT_SLOT_FLOATING_1+iId, Vector3(vWorldPos));
}

void CommandSetFloatingHelpTextToEntity(s32 iId, s32 entityIndex, float fOffsetX, float fOffsetY)
{
	CPhysical* pEntity = CTheScripts::GetEntityToModifyFromGUID<CPhysical>(entityIndex, CTheScripts::GUID_ASSERT_FLAG_ENTITY_EXISTS);

	if (pEntity)
	{
		CHelpMessage::SetWorldPositionFromEntity(HELP_TEXT_SLOT_FLOATING_1+iId, pEntity, Vector2(fOffsetX, fOffsetY));
	}
}

void CommandSetFloatingHelpTextStyle(s32 iId, s32 iNewStyle, s32 hudColour, s32 iAlpha, s32 iArrowDirection, s32 iFloatingTextOffset)
{
	CHelpMessage::SetStyle(HELP_TEXT_SLOT_FLOATING_1+iId, iNewStyle, iArrowDirection, iFloatingTextOffset, hudColour, iAlpha);
}

void CommandClearFloatingHelp(s32 iId, bool bClearNow)
{
	//Displayf("CLEAR_FLOATING_HELP %d called by %s script", (s32)bClearNow, CTheScripts::GetCurrentScriptName());

	CHelpMessage::Clear(HELP_TEXT_SLOT_FLOATING_1+iId, bClearNow);
}

bool CommandIsThisFloatingHelpTextBeingDisplayed(s32 iId, const char *pTextLabel)
{
	CScriptTextConstruction::BeginIsThisHelpMessageBeingDisplayed(pTextLabel);
	return CScriptTextConstruction::EndIsThisHelpMessageBeingDisplayed(HELP_TEXT_SLOT_FLOATING_1+iId);
}

bool CommandIsThisFloatingHelpTextWithNumberBeingDisplayed(s32 iId, const char *pTextLabel, int NumberToInsert)
{
	CScriptTextConstruction::BeginIsThisHelpMessageBeingDisplayed(pTextLabel);
	CScriptTextConstruction::AddInteger(NumberToInsert);
	return CScriptTextConstruction::EndIsThisHelpMessageBeingDisplayed(HELP_TEXT_SLOT_FLOATING_1+iId);
}

bool CommandIsThisFloatingHelpTextWithStringBeingDisplayed(s32 iId, const char *pTextLabel, const char *pSubStringTextLabel)
{
	CScriptTextConstruction::BeginIsThisHelpMessageBeingDisplayed(pTextLabel);
	CScriptTextConstruction::AddSubStringTextLabel(pSubStringTextLabel);
	return CScriptTextConstruction::EndIsThisHelpMessageBeingDisplayed(HELP_TEXT_SLOT_FLOATING_1+iId);
}

bool CommandIsThisFloatingHelpTextWithStringAndNumberBeingDisplayed(s32 iId, const char *pTextLabel, const char *pSubStringTextLabel, int iNumber)
{
	CScriptTextConstruction::BeginIsThisHelpMessageBeingDisplayed(pTextLabel);
	CScriptTextConstruction::AddSubStringTextLabel(pSubStringTextLabel);
	CScriptTextConstruction::AddInteger(iNumber);
	return CScriptTextConstruction::EndIsThisHelpMessageBeingDisplayed(HELP_TEXT_SLOT_FLOATING_1+iId);
}



// --- Radar Blip commands -----------------------------------------------------------------------------------

s32 GetStandardBlipEnumId()
{
	return BLIP_LEVEL;
}

s32 GetWaypointBlipEnumId()
{
	return BLIP_WAYPOINT;
}

s32 GetNumberOfActiveBlips()
{
	s32 iNumberOfBlips = 0;

	for (s32 nTrace = 0; nTrace < CMiniMap::GetMaxCreatedBlips(); nTrace++)
	{
		if (CMiniMap::GetBlipFromActualId(nTrace))
		{
			if (nTrace != CMiniMap::GetActualSimpleBlipId() &&
				nTrace != CMiniMap::GetActualNorthBlipId() &&
				nTrace != CMiniMap::GetActualCentreBlipId() )
			{
				iNumberOfBlips++;
			}
		}
	}

	return iNumberOfBlips;
}

int GetClosestBlipInfoId(int iBlipSprite)
{
	s32 iUniqueBlipUsed = INVALID_BLIP_ID;

	const float INVALID_DISTANCE = -1.0f;
	float fClosestDist = INVALID_DISTANCE;

	for (s32 i = 0; i < CMiniMap::GetMaxCreatedBlips(); i++)
	{
		CMiniMapBlip *pBlip = CMiniMap::GetBlipFromActualId(i);
		if (pBlip)
		{
			if (CMiniMap::GetBlipObjectNameId(pBlip) == iBlipSprite)
			{
				if ( i == CMiniMap::GetActualSimpleBlipId() ||
					i == CMiniMap::GetActualNorthBlipId() ||
					i == CMiniMap::GetActualCentreBlipId() )
				{
					continue;
				}

				if( CMiniMap::IsFlagSet(pBlip, BLIP_FLAG_HIDDEN_ON_LEGEND) ||
					CMiniMap::GetBlipDisplayValue(pBlip) == BLIP_DISPLAY_NEITHER ||
					CMiniMap::GetBlipAlphaValue(pBlip) == 0.0f)
				{
					continue;
				}

				Vector3 vBlipPosition = CMiniMap::GetBlipPositionValue(pBlip);
				CPed *pLocalPlayer = CMiniMap::GetMiniMapPed();

				if(pLocalPlayer)
				{
					Vector3 vPlayerPos(pLocalPlayer->GetTransform().GetPosition().GetXf(), 
						pLocalPlayer->GetTransform().GetPosition().GetYf(), 
						pLocalPlayer->GetTransform().GetPosition().GetZf());

					float fDist = vPlayerPos.Dist2(vBlipPosition);
					if(fDist < fClosestDist || fClosestDist == INVALID_DISTANCE)
					{
						iUniqueBlipUsed = CMiniMap::GetUniqueBlipUsed(pBlip);
						CScriptHud::iFindNextRadarBlipId = i+1;
						fClosestDist = fDist;
					}
				}
				else
				{
					return iUniqueBlipUsed;
				}

#if __DEV
				uiDisplayf("ENUMERATING BLIPS FROM SCRIPT - Found sprite %s (%d) active, returning unique id %d (actual id %d) - display: %d - type: %d - coords: %0.2f,%0.2f,%0.2f", CMiniMap_Common::GetBlipName(iBlipSprite), iBlipSprite, iUniqueBlipUsed, i, CMiniMap::GetBlipDisplayValue(pBlip), CMiniMap::GetBlipTypeValue(pBlip), vBlipPosition.x, vBlipPosition.y, vBlipPosition.z);
#endif // __DEV
			}
		}
	}

	return iUniqueBlipUsed;
}

int GetNextBlipInfoId(int iBlipSprite)
{
	for (s32 nTrace = CScriptHud::iFindNextRadarBlipId; nTrace < CMiniMap::GetMaxCreatedBlips(); nTrace++)
	{
		CMiniMapBlip *pBlip = CMiniMap::GetBlipFromActualId(nTrace);
		if (pBlip)
		{
			if (CMiniMap::GetBlipObjectNameId(pBlip) == iBlipSprite)
			{
				if ( nTrace == CMiniMap::GetActualSimpleBlipId() ||
					nTrace == CMiniMap::GetActualNorthBlipId() ||
					nTrace == CMiniMap::GetActualCentreBlipId() )
				{
					continue;
				}

				CScriptHud::iFindNextRadarBlipId = nTrace+1;

				s32 iUniqueBlipUsed = CMiniMap::GetUniqueBlipUsed(pBlip);

#if __DEV
				Vector3 vReturnPosition = CMiniMap::GetBlipPositionValue(pBlip);

				uiDisplayf("ENUMERATING BLIPS FROM SCRIPT - Found sprite %s (%d) active, returning unique id %d (actual id %d) - display: %d - type: %d - coords: %0.2f,%0.2f,%0.2f", CMiniMap_Common::GetBlipName(iBlipSprite), iBlipSprite, iUniqueBlipUsed, nTrace, CMiniMap::GetBlipDisplayValue(pBlip), CMiniMap::GetBlipTypeValue(pBlip), vReturnPosition.x, vReturnPosition.y, vReturnPosition.z);
#endif // __DEV
				return iUniqueBlipUsed;
			}
		}
	}

	return INVALID_BLIP_ID;
}

int GetFirstBlipInfoId(int iBlipSprite)
{
	CScriptHud::iFindNextRadarBlipId = 0;

	return (GetNextBlipInfoId(iBlipSprite));
}

scrVector GetBlipInfoIdCoord(int iBlipId) 
{
	Vector3 vReturnPosition(ORIGIN);

	if (scriptVerifyf(iBlipId != INVALID_BLIP_ID, "%s: GET_BLIP_INFO_ID_COORD - Blip %d doesn't exist", CTheScripts::GetCurrentScriptNameAndProgramCounter(), iBlipId))
	{
		CMiniMapBlip *pBlip = CMiniMap::GetBlip(iBlipId);

		if (pBlip)
		{
			vReturnPosition = CMiniMap::GetBlipPositionValue(pBlip);
		}
	}

	return vReturnPosition;
}

int GetBlipInfoIdDisplay(int iBlipId)
{
	if (scriptVerifyf(iBlipId != INVALID_BLIP_ID, "%s: GET_BLIP_INFO_ID_DISPLAY - Blip %d doesn't exist", CTheScripts::GetCurrentScriptNameAndProgramCounter(), iBlipId))
	{
		CMiniMapBlip *pBlip = CMiniMap::GetBlip(iBlipId);

		if (pBlip)
		{
			return (CMiniMap::GetBlipDisplayValue(pBlip));
		}
	}

	return BLIP_DISPLAY_NEITHER;
}

int GetBlipInfoIdType(int iBlipId)
{
	if (scriptVerifyf(iBlipId != INVALID_BLIP_ID, "%s: GET_BLIP_INFO_ID_TYPE - Blip %d doesn't exist", CTheScripts::GetCurrentScriptNameAndProgramCounter(), iBlipId))
	{
		CMiniMapBlip *pBlip = CMiniMap::GetBlip(iBlipId);
		
		if (pBlip)
		{
			return (CMiniMap::GetBlipTypeValue(pBlip));
		}
	}

	return BLIP_TYPE_UNUSED;
}

int GetBlipFromEntity(s32 entityIndex)
{
	CPhysical* pEntity = CTheScripts::GetEntityToModifyFromGUID<CPhysical>(entityIndex, CTheScripts::GUID_ASSERT_FLAG_ENTITY_EXISTS);

	if(pEntity)
	{
		CMiniMapBlip* pBlip = NULL;

		if (pEntity->GetIsTypePed())
		{
			pBlip = CMiniMap::GetBlipAttachedToEntity(pEntity, BLIP_TYPE_CHAR, 0);
			if(!pBlip)
			{
				pBlip = CMiniMap::GetBlipAttachedToEntity(pEntity, BLIP_TYPE_COP, 0);
			}
		}
		else if (pEntity->GetIsTypeVehicle())
		{
			pBlip = CMiniMap::GetBlipAttachedToEntity(pEntity, BLIP_TYPE_CAR, 0);
		}
		else if (pEntity->GetIsTypeObject())
		{
			CObject *pObject = static_cast<CObject*>(pEntity);

			if(pObject->m_nObjectFlags.bIsPickUp)
			{
				pBlip = CMiniMap::GetBlipAttachedToEntity(pObject, BLIP_TYPE_PICKUP_OBJECT, 0);
			}
			else
			{
				pBlip = CMiniMap::GetBlipAttachedToEntity(pObject, BLIP_TYPE_OBJECT, 0);
			}
		}

		if (pBlip)
		{
			return (CMiniMap::GetUniqueBlipUsed(pBlip));
		}
	}

	return INVALID_BLIP_ID;
}



int GetBlipInfoIdEntityIndex(int iBlipId)
{
	if (scriptVerifyf(iBlipId != INVALID_BLIP_ID, "%s: GET_BLIP_INFO_ID_ENTITY_INDEX - Blip %d doesn't exist", CTheScripts::GetCurrentScriptNameAndProgramCounter(), iBlipId))
	{
		CMiniMapBlip *pBlip = CMiniMap::GetBlip(iBlipId);

		if (pBlip)
		{
			eBLIP_TYPE blipType = CMiniMap::GetBlipTypeValue(pBlip);
			if (scriptVerifyf(blipType == BLIP_TYPE_CHAR || 
				blipType == BLIP_TYPE_CAR || 
				blipType == BLIP_TYPE_OBJECT || 
				blipType == BLIP_TYPE_PICKUP_OBJECT,
				"%s: GET_BLIP_INFO_ID_ENTITY_INDEX - Blip %d is not a valid entity! Has type value %d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), iBlipId, (int)blipType))
			{
				CEntity *pEntity = CMiniMap::FindBlipEntity(pBlip);
				if (scriptVerifyf(pEntity, "%s: GET_BLIP_INFO_ID_ENTITY_INDEX - Blip is not attached to an entity!", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
				{
					int guid = fwScriptGuid::GetGuidFromBase(*pEntity);
					if(guid == 0)
					{
						scriptWarningf("%s: GET_BLIP_INFO_ID_ENTITY_INDEX - entity is not a script entity", CTheScripts::GetCurrentScriptNameAndProgramCounter());
					}
					return guid;
				}
			}
		}
	}

	return 0;
}

//	Did this actually work?
//	It seems like it's returning an array index
//	But CreatePickup returns pPlacement->GetScriptInfo()->GetObjectId()
//	On the script side, they're both of type PICKUP_INDEX
int GetBlipInfoIdPickupIndex(int UNUSED_PARAM(iBlipId))
{
// 	if (scriptVerifyf(iBlipId != INVALID_BLIP_ID, "%s: GET_BLIP_INFO_ID_PICKUP_INDEX - Blip %d doesn't exist", CTheScripts::GetCurrentScriptNameAndProgramCounter(), iBlipId))
// 	{
// 		CMiniMapBlip *pBlip = CMiniMap::GetBlip(iBlipId);
// 
// 		if (pBlip)
// 		{
// 			scriptAssertf(CMiniMap::GetBlipTypeValue(pBlip) == BLIP_TYPE_PICKUP, "Blip is not a pickup!");
// 
// 			return (CMiniMap::GetBlipPoolIndexValue(pBlip));
// 		}
// 	}

	scriptAssertf(0, "GET_BLIP_INFO_ID_PICKUP_INDEX - this command has been deprecated. Ask Graeme if you need it back again");
	return 0;
}



//
// name:		AddStealthBlipForPed
// description:	
int AddStealthBlipForPed(s32 pedIndex)
{
	s32 iNewBlipIndex = NULL_IN_SCRIPTING_LANGUAGE;

	CPhysical* pEntity = CTheScripts::GetEntityToModifyFromGUID<CPhysical>(pedIndex, CTheScripts::GUID_ASSERT_FLAG_ENTITY_EXISTS);

	if(pEntity)
	{
		if (scriptVerifyf(pEntity->GetIsTypePed(), "%s: ADD_STEALTH_BLIP_FOR_PED - Entity is not a ped type", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		{
			CPed *pPed = static_cast<CPed*>(pEntity);
			pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_BlippedByScript, true );

			CEntityPoolIndexForBlip pedPoolIndex(pPed, BLIP_TYPE_STEALTH);

			if (pedPoolIndex.IsValid())
			{
				CScriptResource_RadarBlip blip(BLIP_TYPE_STEALTH, pedPoolIndex, BLIP_DISPLAY_BOTH, CTheScripts::GetCurrentScriptName(), 0.0f);
				iNewBlipIndex = CTheScripts::GetCurrentGtaScriptHandler()->RegisterScriptResourceAndGetRef(blip);
			}
		}
	}

	scriptAssertf(iNewBlipIndex != NULL_IN_SCRIPTING_LANGUAGE, "ADD_STEALTH_BLIP_FOR_PED - Failed to create stealth blip for entity index %d", pedIndex);

	return iNewBlipIndex;
}


// UNUSED COMMAND
int AddBlipForGangTerritory(const scrVector & UNUSED_PARAM(scrVecCoors))
{
	return NULL_IN_SCRIPTING_LANGUAGE;
}

//
// name:		AddBlipForRadius
// description:	adds a radius blip
int AddBlipForRadius(const scrVector & scrVecCoors, float fSize)
{
	s32 iNewBlipIndex = NULL_IN_SCRIPTING_LANGUAGE;
	Vector3 vBlipCoords = Vector3(scrVecCoors);

	CScriptResource_RadarBlip blip(BLIP_TYPE_RADIUS, vBlipCoords, BLIP_DISPLAY_BLIPONLY, CTheScripts::GetCurrentScriptName(), fSize);

	iNewBlipIndex = CTheScripts::GetCurrentGtaScriptHandler()->RegisterScriptResourceAndGetRef(blip);

	scriptAssertf(iNewBlipIndex != NULL_IN_SCRIPTING_LANGUAGE, "ADD_BLIP_FOR_RADIUS - Failed to create radius blip for coord(%0.2f, %0.2f, %0.2f)", vBlipCoords.x, vBlipCoords.y, vBlipCoords.z);

	return iNewBlipIndex;
}

//
// name:		AddBlipForArea
// description:	adds an area blip
int AddBlipForArea(const scrVector & scrVecCoors, float fSizeX, float fSizeY)
{
	s32 iNewBlipIndex = NULL_IN_SCRIPTING_LANGUAGE;
	Vector3 vBlipCoords = Vector3(scrVecCoors);

	CScriptResource_RadarBlip blip(BLIP_TYPE_AREA, vBlipCoords, BLIP_DISPLAY_BLIPONLY, CTheScripts::GetCurrentScriptName());

	iNewBlipIndex = CTheScripts::GetCurrentGtaScriptHandler()->RegisterScriptResourceAndGetRef(blip);

	if( scriptVerifyf(iNewBlipIndex != NULL_IN_SCRIPTING_LANGUAGE, "ADD_BLIP_FOR_AREA - Failed to create area blip for coord(%0.2f, %0.2f, %0.2f)", vBlipCoords.x, vBlipCoords.y, vBlipCoords.z) )
	{
		CMiniMap::SetBlipParameter(BLIP_CHANGE_SCALE, iNewBlipIndex, Vector3(fSizeX, fSizeY, 0.0f));
	}

	return iNewBlipIndex;
}

//
// name:		AddBlipForFakeWantedRadius
// description:	adds a 'fake wanted radius' blip (for bug 409051)
int AddBlipForFakeWantedRadius(const scrVector & scrVecCoors, float fSize)
{
	s32 iNewBlipIndex = NULL_IN_SCRIPTING_LANGUAGE;
	Vector3 vBlipCoords = Vector3(scrVecCoors);

	CScriptResource_RadarBlip blip(BLIP_TYPE_RADIUS, vBlipCoords, BLIP_DISPLAY_BLIPONLY, CTheScripts::GetCurrentScriptName(), fSize);

	iNewBlipIndex = CTheScripts::GetCurrentGtaScriptHandler()->RegisterScriptResourceAndGetRef(blip);

	scriptAssertf(iNewBlipIndex != NULL_IN_SCRIPTING_LANGUAGE, "ADD_BLIP_FOR_FAKE_WANTED_RADIUS - Failed to create wanted radius blip for coord(%0.2f, %0.2f, %0.2f)", vBlipCoords.x, vBlipCoords.y, vBlipCoords.z);

	if (iNewBlipIndex != NULL_IN_SCRIPTING_LANGUAGE)
	{
		CMiniMap::SetBlipParameter(BLIP_CHANGE_OBJECT_NAME, iNewBlipIndex, BLIP_WANTED_RADIUS);
	}

	return iNewBlipIndex;
}

//
// name:		AddBlipForEntity
// description:	
int AddBlipForEntity(s32 entityIndex)
{
	s32 iNewBlipIndex = NULL_IN_SCRIPTING_LANGUAGE;

	CPhysical* pEntity = CTheScripts::GetEntityToModifyFromGUID<CPhysical>(entityIndex, CTheScripts::GUID_ASSERT_FLAG_ENTITY_EXISTS);

	if(pEntity)
	{
		eBLIP_TYPE blipType = BLIP_TYPE_UNUSED;
		float scale = 0.0f;

		CEntityPoolIndexForBlip entityPoolIndex;

		if (pEntity->GetIsTypePed())
		{
			blipType = BLIP_TYPE_CHAR;
			static_cast<CPed*>(pEntity)->SetPedConfigFlag( CPED_CONFIG_FLAG_BlippedByScript, true );

			entityPoolIndex = CEntityPoolIndexForBlip(pEntity, blipType);
		}
		else if (pEntity->GetIsTypeVehicle())
		{
			blipType = BLIP_TYPE_CAR;
			scale = 1.0f;

			entityPoolIndex = CEntityPoolIndexForBlip(pEntity, blipType);
		}
		else if (pEntity->GetIsTypeObject())
		{
			CObject *pObject = static_cast<CObject*>(pEntity);
			
			if(pObject->m_nObjectFlags.bIsPickUp)
			{
				blipType = BLIP_TYPE_PICKUP_OBJECT;
			}
			else
			{
				blipType = BLIP_TYPE_OBJECT;
			}
			entityPoolIndex = CEntityPoolIndexForBlip(pEntity, blipType);
		}
		else
		{
			scriptAssertf(0, "%s: ADD_BLIP_FOR_ENTITY - unrecognised entity type", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		}

		if ( (blipType != BLIP_TYPE_UNUSED) && (entityPoolIndex.IsValid()) )
		{
			CScriptResource_RadarBlip blip(blipType, entityPoolIndex, BLIP_DISPLAY_BOTH, CTheScripts::GetCurrentScriptName(), scale);
			iNewBlipIndex = CTheScripts::GetCurrentGtaScriptHandler()->RegisterScriptResourceAndGetRef(blip);
		}
	}

	scriptAssertf(iNewBlipIndex != NULL_IN_SCRIPTING_LANGUAGE, "ADD_BLIP_FOR_ENTITY - Failed to create blip for entity index %d", entityIndex);

	return iNewBlipIndex;
}

void CommandRemoveCopBlipFromPed(s32 pedIndex)
{
	CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(pedIndex, CTheScripts::GUID_ASSERT_FLAG_ENTITY_EXISTS);

	if(pPed)
	{
		pPed->RemoveBlip(BLIP_TYPE_COP);
	}
}

//
// name:		AddBlipForPickup
// description:	Add radar blip for a pickup object in world
int AddBlipForPickup(s32 pickupRef)
{
	s32 iNewBlipIndex = NULL_IN_SCRIPTING_LANGUAGE;

	if (scriptVerify(CTheScripts::GetCurrentGtaScriptHandler()))
	{
		CPickupPlacement* pPlacement = static_cast<CPickupPlacement*>(CTheScripts::GetCurrentGtaScriptHandler()->GetScriptObject(pickupRef));

		if(SCRIPT_VERIFY(pPlacement, "ADD_BLIP_FOR_PICKUP - Pickup does not exist, or has been collected") &&
			scriptVerifyf(pPlacement->GetRadarBlip() == INVALID_BLIP_ID, "%s: ADD_BLIP_FOR_PICKUP - Pickup already has a blip. The Id of the existing blip is %d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pPlacement->GetRadarBlip()) &&
		   SCRIPT_VERIFY(!pPlacement->GetIsCollected(), "ADD_BLIP_FOR_PICKUP - The pickup has been collected"))
		{
			pPlacement->SetHasComplexBlip();
			iNewBlipIndex = pPlacement->CreateBlip();

			CScriptResource_RadarBlip blip(iNewBlipIndex);
			iNewBlipIndex = CTheScripts::GetCurrentGtaScriptHandler()->RegisterScriptResourceAndGetRef(blip);
		}
	}

	scriptAssertf(iNewBlipIndex != NULL_IN_SCRIPTING_LANGUAGE, "ADD_BLIP_FOR_PICKUP - Failed to create blip for pickup ref %d", pickupRef);

	return iNewBlipIndex;
}


//
// name:		AddSimpleBlipForPickup
// description:	Add radar blip for a pickup object in world
/*
void AddSimpleBlipForPickup(s32 pickupRef)
{
	if (scriptVerify(CTheScripts::GetCurrentGtaScriptHandler()))
	{
		CPickupPlacement* pPlacement = static_cast<CPickupPlacement*>(CTheScripts::GetCurrentGtaScriptHandler()->GetScriptObject(pickupRef));

		if(SCRIPT_VERIFY(pPlacement, "ADD_SIMPLE_BLIP_FOR_PICKUP - Pickup does not exist, or has been collected") &&
			SCRIPT_VERIFY(pPlacement->GetRadarBlip() == INVALID_BLIP_ID, "ADD_SIMPLE_BLIP_FOR_PICKUP - Pickup already has a blip"))
		{
			pPlacement->SetHasSimpleBlip();
			pPlacement->CreateBlip();
		}
	}
}
*/

//
// name:		AddBlipForCoord
// description:	Add radar blip at coordinate in world
int AddBlipForCoord(const scrVector & scrVecCoors)
{
	s32 iNewBlipIndex = NULL_IN_SCRIPTING_LANGUAGE;

	Vector3 vBlipCoords = Vector3(scrVecCoors);
	CScriptResource_RadarBlip blip(BLIP_TYPE_COORDS, vBlipCoords, BLIP_DISPLAY_BOTH, CTheScripts::GetCurrentScriptName());

	iNewBlipIndex = CTheScripts::GetCurrentGtaScriptHandler()->RegisterScriptResourceAndGetRef(blip);

	scriptAssertf(iNewBlipIndex != NULL_IN_SCRIPTING_LANGUAGE, "ADD_BLIP_FOR_COORD - Failed to create blip for coord(%0.2f, %0.2f, %0.2f)", vBlipCoords.x, vBlipCoords.y, vBlipCoords.z);

	return iNewBlipIndex;
}


//
// name:		CommandTriggerSonarBlip
// description:	Trigger a sonar blip on the minimap
void CommandTriggerSonarBlip(const scrVector & scrVecPosition, float fNoiseRange, s32 iHudColour)
{
	Vector3 vecPosition = Vector3(scrVecPosition);
	CMiniMap::CreateSonarBlip(vecPosition, fNoiseRange, (eHUD_COLOURS)iHudColour, NULL, true);
}


//
// name:		CommandAllowSonarBlips
// description:	flags to allow sonar blips or not
void CommandAllowSonarBlips(bool bSet)
{
	CMiniMap::AllowSonarBlips(bSet);
}



void CommandSetBlipCoords(int blipIndex, const scrVector & scrVecPosition)
{
	if (scriptVerifyf(blipIndex != INVALID_BLIP_ID, "%s: SET_BLIP_COORDS - Blip %d doesn't exist", CTheScripts::GetCurrentScriptNameAndProgramCounter(), blipIndex))
	{
		CMiniMapBlip *pBlip = CMiniMap::GetBlip(blipIndex);

		if (pBlip)
		{
			eBLIP_TYPE blipType = CMiniMap::GetBlipTypeValue(pBlip);
	
			if (scriptVerifyf(blipType != BLIP_TYPE_CHAR && blipType != BLIP_TYPE_CAR && blipType != BLIP_TYPE_OBJECT && blipType != BLIP_TYPE_PICKUP_OBJECT, "%s: SET_BLIP_COORDS - Cannot set coords on a blip attached to an entity!", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
			{
				Vector3 vecPosition = Vector3(scrVecPosition);
				CMiniMap::SetBlipParameter(BLIP_CHANGE_POSITION, blipIndex, vecPosition);
			}
		}
	}
}

scrVector CommandGetBlipCoords(int iBlipIndex)
{
	Vector3 vecBlipCoords(ORIGIN);

	if (scriptVerifyf(iBlipIndex != INVALID_BLIP_ID, "%s: GET_BLIP_COORDS - Blip %d doesn't exist", CTheScripts::GetCurrentScriptNameAndProgramCounter(), iBlipIndex))
	{
		CMiniMapBlip *pBlip = CMiniMap::GetBlip(iBlipIndex);

		if (pBlip)
		{
			vecBlipCoords = CMiniMap::GetBlipPositionValue(pBlip);
		}
	}

	return vecBlipCoords;
}


//
// name:		AddSpriteBlipForContactPoint
// description:	Add radar blip at contact point
int AddBlipForContactPoint(const scrVector & scrVecCoors)
{
	s32 iNewBlipIndex = NULL_IN_SCRIPTING_LANGUAGE;
	Vector3 vBlipCoords = Vector3(scrVecCoors);

	if (vBlipCoords.z <= INVALID_MINIMUM_WORLD_Z)
	{
		vBlipCoords.z = WorldProbe::FindGroundZForCoord(TOP_SURFACE, vBlipCoords.x, vBlipCoords.y);
	}

	CScriptResource_RadarBlip blip(BLIP_TYPE_CONTACT, vBlipCoords, BLIP_DISPLAY_BOTH, CTheScripts::GetCurrentScriptName());

	iNewBlipIndex = CTheScripts::GetCurrentGtaScriptHandler()->RegisterScriptResourceAndGetRef(blip);

	scriptAssertf(iNewBlipIndex != NULL_IN_SCRIPTING_LANGUAGE, "ADD_BLIP_FOR_CONTACT - Failed to create blip for contact point(%0.2f, %0.2f, %0.2f)", vBlipCoords.x, vBlipCoords.y, vBlipCoords.z);

	return iNewBlipIndex;
}


void CommandDisplayRadar(bool bDisplayRadarFlag)
{
#if __BANK
	static bool bScriptWantRadarToDisplay = true;

	if ( (bDisplayRadarFlag) && (!bScriptWantRadarToDisplay) )
	{
		uiDebugf1("DISPLAY_RADAR(TRUE) called by script: %s", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
	
	if ( (!bDisplayRadarFlag) && (bScriptWantRadarToDisplay) )
	{
		uiDebugf1("DISPLAY_RADAR(FALSE) called by script: %s", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}

	bScriptWantRadarToDisplay = bDisplayRadarFlag;
#endif // __BANK

	CMiniMap::SetVisible(bDisplayRadarFlag);
	CScriptHud::bDisplayRadar = bDisplayRadarFlag;
}

void CommandSetFakeSpectatorMode(bool bVlaue)
{
	CScriptHud::ms_bFakeSpectatorMode = bVlaue;
}

bool CommandGetFakeSpectatorMode()
{
	return CScriptHud::ms_bFakeSpectatorMode;
}


bool CommandIsHudHidden()
{
	return !CScriptHud::bDisplayHud;
}


bool CommandIsRadarHidden()
{
	return !CScriptHud::bDisplayRadar;
}

bool CommandIsMiniMapRendering()
{
	return CMiniMap::IsRendering();
}


void CommandSetRadarZoomPrecise(float fZoomValue)
{
	CScriptHud::ms_fMiniMapForcedZoomPercentage = fZoomValue;

	if (CScriptHud::ms_fMiniMapForcedZoomPercentage > 100.0f)
		CScriptHud::ms_fMiniMapForcedZoomPercentage = 100.0f;

	if (CScriptHud::ms_fMiniMapForcedZoomPercentage < 0.0f)
		CScriptHud::ms_fMiniMapForcedZoomPercentage = 0.0f;

	CScriptHud::ms_iRadarZoomValue = 0;
	CScriptHud::ms_fRadarZoomDistanceThisFrame = 0.0f;
}


void CommandSetRadarZoom(int ZoomValue)
{
#define FORCED_MIN_ZOOM_VALUE	(100)

#define MAX_ALLOWABLE_SCRIPT_SCALE_AMOUNT_BIGMAP (6000)  // linked to MAX_SCRIPT_SCALE_AMOUNT_BIGMAP but may not be the same
#define MAX_ALLOWABLE_SCRIPT_SCALE_AMOUNT_STANDARD (1500)  // linked to MAX_SCRIPT_SCALE_AMOUNT_STANDARD but may not be the same

	if (!CPauseMenu::IsActive())  // lets not modify the zoom value inside frontend as the map uses the radar stuff
	{
		s32 iMaxAllowableAmountInThisMode;

		if (CMiniMap::IsInBigMap())
		{
			iMaxAllowableAmountInThisMode = MAX_ALLOWABLE_SCRIPT_SCALE_AMOUNT_BIGMAP;
		}
		else
		{
			iMaxAllowableAmountInThisMode = MAX_ALLOWABLE_SCRIPT_SCALE_AMOUNT_STANDARD;
		}

		if(scriptVerifyf( (ZoomValue >= 0) && (ZoomValue <= iMaxAllowableAmountInThisMode), "SET_RADAR_ZOOM - value should be between 0 and %d", iMaxAllowableAmountInThisMode))
		{
			if (ZoomValue != 0)
			{
				CScriptHud::ms_iRadarZoomValue = ZoomValue+FORCED_MIN_ZOOM_VALUE;
			}
			else
			{
				CScriptHud::ms_iRadarZoomValue = 0;  // 0 switches it off
			}
		}
	}
	else
	{
		CScriptHud::ms_iRadarZoomValue = 0;
	}

	CScriptHud::ms_fRadarZoomDistanceThisFrame = 0.0f;
	CScriptHud::ms_fMiniMapForcedZoomPercentage = 0.0f;
}



#define __MIN_ZOOM_DISTANCE (10.0f)
#define __MAX_ZOOM_DISTANCE (6700.0f)

void CommandSetRadarZoomToBlip(s32 iBlipIndex, float fOffset)
{
	if (iBlipIndex != INVALID_BLIP_ID)
	{
		CMiniMapBlip *pBlip = CMiniMap::GetBlip(iBlipIndex);

		if (scriptVerifyf(pBlip, "%s: SET_RADAR_ZOOM_TO_BLIP - Blip %d doesn't exist", CTheScripts::GetCurrentScriptNameAndProgramCounter(), iBlipIndex))
		{
			Vector3 vCentrePos;
			Vector3 vBlipPos = CMiniMap::GetBlipPositionValue(pBlip);
			CMiniMap::GetCentrePosition(vCentrePos);

			Vector3 deltaFromTargetXY = vCentrePos-vBlipPos;
			deltaFromTargetXY.z = 0.0f;

			CScriptHud::ms_fRadarZoomDistanceThisFrame = deltaFromTargetXY.Mag() + fOffset;

			// some limits otherwise things look crap
			CScriptHud::ms_fRadarZoomDistanceThisFrame = rage::Max(CScriptHud::ms_fRadarZoomDistanceThisFrame, __MIN_ZOOM_DISTANCE);
			CScriptHud::ms_fRadarZoomDistanceThisFrame = rage::Min(CScriptHud::ms_fRadarZoomDistanceThisFrame, __MAX_ZOOM_DISTANCE);

			uiDisplayf("SET_RADAR_ZOOM_TO_BLIP - Attempting to zoom minimap to blip %d, with a distance of %0.2f", iBlipIndex, CScriptHud::ms_fRadarZoomDistanceThisFrame);
		}
	}

	CScriptHud::ms_iRadarZoomValue = 0;
	CScriptHud::ms_fMiniMapForcedZoomPercentage = 0.0f;
}



void CommandSetRadarZoomToDistance(float fDistance)
{
	CScriptHud::ms_iRadarZoomValue = 0;
	CScriptHud::ms_fMiniMapForcedZoomPercentage = 0.0f;

	CScriptHud::ms_fRadarZoomDistanceThisFrame = fDistance;

	// some limits otherwise things look crap
	CScriptHud::ms_fRadarZoomDistanceThisFrame = rage::Max(CScriptHud::ms_fRadarZoomDistanceThisFrame, __MIN_ZOOM_DISTANCE);
	CScriptHud::ms_fRadarZoomDistanceThisFrame = rage::Min(CScriptHud::ms_fRadarZoomDistanceThisFrame, __MAX_ZOOM_DISTANCE);

#if __BANK  // anti spam filter:
	static float fPreviousRadarZoomDistanceThisFrame = 0.0f;

	if (CScriptHud::ms_fRadarZoomDistanceThisFrame != fPreviousRadarZoomDistanceThisFrame)
	{
		uiDisplayf("SET_RADAR_ZOOM_TO_DISTANCE - Attempting to zoom minimap to a distance of %0.2f", CScriptHud::ms_fRadarZoomDistanceThisFrame);
		fPreviousRadarZoomDistanceThisFrame = CScriptHud::ms_fRadarZoomDistanceThisFrame;
	}
#endif // __BANK
}



void CommandUpdateRadarZoomToBlip()
{
	// depreciated command
}



int GetBlipSprite(int iBlipIndex)
{
	s32 iReturnValue = -1;

	if (scriptVerifyf(iBlipIndex != INVALID_BLIP_ID, "%s: GET_BLIP_SPRITE - Blip %d doesn't exist", CTheScripts::GetCurrentScriptNameAndProgramCounter(), iBlipIndex))
	{
		CMiniMapBlip *pBlip = CMiniMap::GetBlip(iBlipIndex);

		if (pBlip)
		{
			iReturnValue = CMiniMap::GetBlipObjectNameId(pBlip);
		}
	}

	return iReturnValue;
}


const char *GetBlipName(s32 iBlipIndex)
{
	static char cRetString[MAX_BLIP_NAME_SIZE];
	cRetString[0] = '\0';

	if (scriptVerifyf(iBlipIndex != INVALID_BLIP_ID, "%s: GET_NAME_OF_BLIP - Blip %d doesn't exist", CTheScripts::GetCurrentScriptNameAndProgramCounter(), iBlipIndex))
	{
		CMiniMapBlip *pBlip = CMiniMap::GetBlip(iBlipIndex);

		if (pBlip)
		{
			safecpy( cRetString, CMiniMap::GetBlipNameValue(pBlip) );
		}
	}

	return (const char *)&cRetString;
}



void ChangeBlipNameFromTextFile(s32 iBlipIndex, const char *pTextLabel)
{
	if (scriptVerifyf(iBlipIndex != INVALID_BLIP_ID, "%s: SET_BLIP_NAME_FROM_TEXT_FILE - Blip %d doesn't exist", CTheScripts::GetCurrentScriptNameAndProgramCounter(), iBlipIndex))
	{
		if (scriptVerifyf(strlen(pTextLabel) < MAX_BLIP_NAME_SIZE, "SET_BLIP_NAME_FROM_TEXT_FILE - text label is too long"))
		{
			CMiniMap::SetBlipParameter(BLIP_CHANGE_NAME, iBlipIndex, pTextLabel);
		}
	}
}


void ChangeBlipNameFromAscii(s32 iBlipIndex, const char *pTextString)
{
	if (scriptVerifyf(iBlipIndex != INVALID_BLIP_ID, "%s: SET_BLIP_NAME_FROM_ASCII - Blip %d doesn't exist", CTheScripts::GetCurrentScriptNameAndProgramCounter(), iBlipIndex))
	{
		if (scriptVerifyf(strlen(pTextString) < MAX_BLIP_NAME_SIZE, "SET_BLIP_NAME_FROM_ASCII - string is too long"))
		{
			CMiniMap::SetBlipParameter(BLIP_CHANGE_NAME_FROM_ASCII, iBlipIndex, pTextString);
		}
	}
}

//PURPOSE:  Change Radar blip name with name of this player
//	Do the equivalent of CHANGE_BLIP_NAME_FROM_ASCII(blip, GET_PLAYER_NAME(PlayerIndex)) in code
void CommandSetBlipNameToPlayerName(s32 iBlipIndex, s32 iPlayerIndex)
{
/*
		CPed * pPlayerPed = CTheScripts::FindLocalPlayerPed(iPlayerIndex);
		if (scriptVerifyf(pPlayerPed, "%s: SET_BLIP_NAME_TO_PLAYER_NAME - can't find a player with index %d",
			CTheScripts::GetCurrentScriptNameAndProgramCounter(),
			iPlayerIndex))
		{
			CPlayerInfo *pPlayer = pPlayerPed->GetPlayerInfo();

			if(SCRIPT_VERIFY(pPlayer, "SET_BLIP_NAME_TO_PLAYER_NAME - failed to get playerinfo from player ped"))
			{
				char GxtPlayerName[64];
				CTextConversion::AsciiTochar(pPlayer->m_GamerInfo.GetName(), GxtPlayerName);
				CMiniMap::SetBlipParameter(BLIP_CHANGE_NAME_FROM_ASCII, iBlipIndex, GxtPlayerName);
			}
		}
*/
	if (scriptVerifyf(iBlipIndex != INVALID_BLIP_ID, "%s: SET_BLIP_NAME_TO_PLAYER_NAME - Blip %d doesn't exist", CTheScripts::GetCurrentScriptNameAndProgramCounter(), iBlipIndex))
	{
		const char *pAsciiPlayerName = player_commands::GetPlayerName(iPlayerIndex, "SET_BLIP_NAME_TO_PLAYER_NAME");
		CMiniMap::SetBlipParameter(BLIP_CHANGE_NAME_FROM_ASCII, iBlipIndex, pAsciiPlayerName);
	}
}

//
// name:		ChangeBlipColour
// description:	
void ChangeBlipColour(s32 iBlipIndex, s32 iBlipColour)
{
	if (scriptVerifyf(iBlipIndex != INVALID_BLIP_ID, "%s: SET_BLIP_COLOUR - Blip %d doesn't exist", CTheScripts::GetCurrentScriptNameAndProgramCounter(), iBlipIndex))
	{
#if __BANK
		if (iBlipIndex == CMiniMap::GetUniqueCentreBlipId())
		{
			CMiniMapBlip *pBlip = CMiniMap::GetBlip(iBlipIndex);
			if ( (pBlip) && (iBlipColour != (s32)CMiniMap::GetBlipColourValue(pBlip)) )
			{
				uiDisplayf("%s: SET_BLIP_COLOUR called on centre blip - changed to BLIP COLOUR=%d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), iBlipColour);
			}
		}
#endif
		CMiniMap::SetBlipParameter(BLIP_CHANGE_COLOUR, iBlipIndex, iBlipColour);
	}
}



//
// name:		ChangeBlipSecondaryColour
// description:	
void ChangeBlipSecondaryColour(s32 iBlipIndex, s32 iColourR, s32 iColourG, s32 iColourB)
{
	if (scriptVerifyf(iBlipIndex != INVALID_BLIP_ID, "%s: SET_BLIP_SECONDARY_COLOUR - Blip %d doesn't exist", CTheScripts::GetCurrentScriptNameAndProgramCounter(), iBlipIndex))
	{
		if (iColourR > 255)
			iColourR = 255;

		if (iColourG > 255)
			iColourG = 255;

		if (iColourB > 255)
			iColourB = 255;

		CMiniMap::SetBlipParameter(BLIP_CHANGE_COLOUR32, iBlipIndex, Color32(iColourR, iColourG, iColourB));
	}
}



//
// name:		ChangeBlipAlpha
// description:	
void ChangeBlipAlpha(s32 iBlipIndex, s32 blipAlpha)
{
	if (scriptVerifyf(iBlipIndex != INVALID_BLIP_ID, "%s: SET_BLIP_ALPHA - Blip %d doesn't exist", CTheScripts::GetCurrentScriptNameAndProgramCounter(), iBlipIndex))
	{
#if __BANK
		if (iBlipIndex == CMiniMap::GetUniqueCentreBlipId())
		{
			CMiniMapBlip *pBlip = CMiniMap::GetBlip(iBlipIndex);
			if ( (pBlip) && (blipAlpha != CMiniMap::GetBlipAlphaValue(pBlip)) )
			{
				uiDisplayf("%s: SET_BLIP_ALPHA called on centre blip - changed to %d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), blipAlpha);
			}
		}
#endif

		CMiniMap::SetBlipParameter(BLIP_CHANGE_ALPHA, iBlipIndex, blipAlpha);
		CScriptHud::GetBlipFades().RemoveBlipFade(iBlipIndex);
	}
}



//
// name:		CommandGetBlipAlpha
// description:	
s32 CommandGetBlipAlpha(s32 iBlipIndex)
{
	s32 iReturnBlipAlpha = 0;

	if (scriptVerifyf(iBlipIndex != INVALID_BLIP_ID, "%s: GET_BLIP_ALPHA - Blip %d doesn't exist", CTheScripts::GetCurrentScriptNameAndProgramCounter(), iBlipIndex))
	{
		CMiniMapBlip *pBlip = CMiniMap::GetBlip(iBlipIndex);

		if (pBlip)
		{
			iReturnBlipAlpha = CMiniMap::GetBlipAlphaValue(pBlip);
		}
	}

	return iReturnBlipAlpha;
}

void CommandSetBlipFade(s32 iBlipIndex, s32 DestinationAlpha, s32 FadeDuration)
{
	if (DestinationAlpha < 0)
	{
		scriptAssertf(0, "SET_BLIP_FADE - DestinationAlpha must be >= 0, setting to 0");
		DestinationAlpha = 0;
	}

	if (DestinationAlpha > 255)
	{
		scriptAssertf(0, "SET_BLIP_FADE - DestinationAlpha must be <= 255, setting to 255");
		DestinationAlpha = 255;
	}

	CScriptHud::GetBlipFades().AddBlipFade(iBlipIndex, DestinationAlpha, FadeDuration);
}

s32 CommandGetBlipFadeDirection(s32 iBlipIndex)
{
	return CScriptHud::GetBlipFades().GetBlipFadeDirection(iBlipIndex);
}

void CommandSetBlipRotationFloat(s32 iBlipIndex, float fDegrees)
{
	if (scriptVerifyf(iBlipIndex != INVALID_BLIP_ID, "%s: SET_BLIP_ROTATION - Blip %d doesn't exist", CTheScripts::GetCurrentScriptNameAndProgramCounter(), iBlipIndex))
	{
		CMiniMap::SetBlipParameter(BLIP_CHANGE_DIRECTION, iBlipIndex, fDegrees);
	}
}

//
// name:		CommandSetBlipRotation
// description:	set the rotation (in degrees) of the blip
void CommandSetBlipRotation(s32 iBlipIndex, s32 iDegrees)
{
	CommandSetBlipRotationFloat(iBlipIndex, static_cast<float>(iDegrees));
}

//
// name:		CommandGetBlipRotation
// description:	Get the current rotation value of this blip
s32 CommandGetBlipRotation(s32 iBlipId)
{
	float fRotationDegrees = 0.0f;

	if (scriptVerifyf(iBlipId != INVALID_BLIP_ID, "%s: GET_BLIP_ROTATION - Blip %d doesn't exist", CTheScripts::GetCurrentScriptNameAndProgramCounter(), iBlipId))
	{
		CMiniMapBlip *pBlip = CMiniMap::GetBlip(iBlipId);

		if (pBlip)
		{
			fRotationDegrees = CMiniMap::GetBlipDirectionValue(pBlip);
		}
	}

	return static_cast<s32>(fRotationDegrees);
}

//
// name:		ChangeBlipFlashTimer
// description:	time a blip will flash for, then it automatically stops flashing by code
void ChangeBlipFlashTimer(s32 iBlipIndex, s32 iFlashTimer)
{
	if (scriptVerifyf(iBlipIndex != INVALID_BLIP_ID, "%s: SET_BLIP_FLASH_TIMER - Blip %d doesn't exist", CTheScripts::GetCurrentScriptNameAndProgramCounter(), iBlipIndex))
	{
		CMiniMap::SetBlipParameter(BLIP_CHANGE_FLASH_DURATION, iBlipIndex, iFlashTimer);
	}
}



//
// name:		ChangeBlipFlashInterval
// description:	the duration between each flash
void ChangeBlipFlashInterval(s32 iBlipIndex, s32 iFlashInterval)
{
	if (scriptVerifyf(iBlipIndex != INVALID_BLIP_ID, "%s: SET_BLIP_FLASH_INTERVAL - Blip %d doesn't exist", CTheScripts::GetCurrentScriptNameAndProgramCounter(), iBlipIndex))
	{
		CMiniMap::SetBlipParameter(BLIP_CHANGE_FLASH_INTERVAL, iBlipIndex, iFlashInterval);
	}
}

//
// name:		CommandGetBlipColour
// description:	
s32 CommandGetBlipColour(s32 iBlipIndex)
{
	s32 iReturnBlipColour = 0;

	if (scriptVerifyf(iBlipIndex != INVALID_BLIP_ID, "%s: GET_BLIP_COLOUR - Blip %d doesn't exist", CTheScripts::GetCurrentScriptNameAndProgramCounter(), iBlipIndex))
	{
		CMiniMapBlip *pBlip = CMiniMap::GetBlip(iBlipIndex);

		if (pBlip)
		{
			iReturnBlipColour = CMiniMap::GetBlipColourValue(pBlip);
		}
	}
	
	return iReturnBlipColour;
}



eHUD_COLOURS CommandGetBlipHudColour(s32 iBlipIndex)
{
	eHUD_COLOURS iReturnHudColour = HUD_COLOUR_WHITE;

	if (scriptVerifyf(iBlipIndex != INVALID_BLIP_ID, "%s: GET_BLIP_HUD_COLOUR - Blip %d doesn't exist", CTheScripts::GetCurrentScriptNameAndProgramCounter(), iBlipIndex))
	{
		CMiniMapBlip *pBlip = CMiniMap::GetBlip(iBlipIndex);

		if (pBlip)
		{
			CMiniMap_Common::GetColourFromBlipSettings(CMiniMap::GetBlipColourValue(pBlip), CMiniMap::IsFlagSet(pBlip, BLIP_FLAG_BRIGHTNESS), &iReturnHudColour);
		}
	}

	return iReturnHudColour;
}



bool CommandIsBlipShortRange(s32 iBlipIndex)
{
	if (scriptVerifyf(iBlipIndex != INVALID_BLIP_ID, "%s: GET_BLIP_SHORT_RANGE - Blip %d doesn't exist", CTheScripts::GetCurrentScriptNameAndProgramCounter(), iBlipIndex))
	{
		CMiniMapBlip *pBlip = CMiniMap::GetBlip(iBlipIndex);

		if (pBlip)
		{
			return (CMiniMap::IsFlagSet(pBlip, BLIP_FLAG_SHORTRANGE));
		}
	}

	return false;
}


bool CommandIsBlipOnMiniMap(s32 iBlipIndex)
{
	if (scriptVerifyf(iBlipIndex != INVALID_BLIP_ID, "%s: IS_BLIP_ON_MINIMAP - Blip %d doesn't exist", CTheScripts::GetCurrentScriptNameAndProgramCounter(), iBlipIndex))
	{
		CMiniMapBlip *pBlip = CMiniMap::GetBlip(iBlipIndex);
		if (pBlip)
		{
			return (CMiniMap::IsBlipOnStage(pBlip));
		}
	}

	return false;
}



//
// name:		CommandDoesBlipHaveGpsRoute
// description:	returns whether this blip has a route attached
bool CommandDoesBlipHaveGpsRoute(s32 iBlipIndex)
{
	if (scriptVerifyf(iBlipIndex != INVALID_BLIP_ID, "%s: DOES_BLIP_HAVE_GPS_ROUTE - Blip %d doesn't exist", CTheScripts::GetCurrentScriptNameAndProgramCounter(), iBlipIndex))
	{
		CMiniMapBlip *pBlip = CMiniMap::GetBlip(iBlipIndex);

		if (pBlip)
		{
			return (CMiniMap::IsFlagSet(pBlip, BLIP_FLAG_ROUTE));
		}
	}

	return false;
}


void CommandSetBlipDebugNumber(s32 iBlipIndex, s32 iValue)
{
	if (scriptVerifyf(iBlipIndex != INVALID_BLIP_ID, "%s: SET_BLIP_DEBUG_NUMBER - Blip %d doesn't exist", CTheScripts::GetCurrentScriptNameAndProgramCounter(), iBlipIndex))
	{
		CMiniMap::SetBlipParameter(BLIP_CHANGE_DEBUG_NUMBER, iBlipIndex, iValue);
	}
}


void CommandSetBlipDebugNumberOff(s32 iBlipIndex)
{
	if (scriptVerifyf(iBlipIndex != INVALID_BLIP_ID, "%s: SET_BLIP_DEBUG_NUMBER_OFF - Blip %d doesn't exist", CTheScripts::GetCurrentScriptNameAndProgramCounter(), iBlipIndex))
	{
		CMiniMap::SetBlipParameter(BLIP_CHANGE_DEBUG_NUMBER, iBlipIndex, -1);
	}
}


void CommandSetBlipHiddenOnLegend(s32 iBlipIndex, bool bValue)
{
	if (scriptVerifyf(iBlipIndex != INVALID_BLIP_ID, "%s: SET_BLIP_HIDDEN_ON_LEGEND - Blip %d doesn't exist", CTheScripts::GetCurrentScriptNameAndProgramCounter(), iBlipIndex))
	{
		CMiniMap::SetBlipParameter(BLIP_CHANGE_HIDDEN_ON_LEGEND, iBlipIndex, bValue);
	}
}


bool CommandIsBlipHiddenOnLegend(s32 iBlipIndex)
{
	if (scriptVerifyf(iBlipIndex != INVALID_BLIP_ID, "%s: IS_BLIP_HIDDEN_ON_LEGEND - Blip %d doesn't exist", CTheScripts::GetCurrentScriptNameAndProgramCounter(), iBlipIndex))
	{
		CMiniMapBlip *pBlip = CMiniMap::GetBlip(iBlipIndex);

		if (pBlip)
		{
			return (CMiniMap::IsFlagSet(pBlip, BLIP_FLAG_HIDDEN_ON_LEGEND));
		}
	}

	return false;
}



void CommandSetBlipHighDetail(s32 iBlipIndex, bool bValue)
{
	if (scriptVerifyf(iBlipIndex != INVALID_BLIP_ID, "%s: SET_BLIP_HIGH_DETAIL - Blip %d doesn't exist", CTheScripts::GetCurrentScriptNameAndProgramCounter(), iBlipIndex))
	{
		CMiniMap::SetBlipParameter(BLIP_CHANGE_HIGH_DETAIL, iBlipIndex, bValue);

/*		CMiniMapBlip *pBlip = CMiniMap::GetBlip(iBlipIndex);

		if (pBlip)
		{
			if (bValue)
			{
				sfDisplayf("SET_BLIP_HIGH_DETAIL(TRUE) called on %s %d blip", CMiniMap::GetBlipNameValue(pBlip), iBlipIndex);
			}
			else
			{
				sfDisplayf("SET_BLIP_HIGH_DETAIL(FALSE) called on %s %d blip", CMiniMap::GetBlipNameValue(pBlip), iBlipIndex);
			}
		}*/
	}
}



void CommandSetBlipAsMissionCreatorBlip(s32 iBlipIndex, bool bValue)
{
	if (scriptVerifyf(iBlipIndex != INVALID_BLIP_ID, "%s: SET_BLIP_AS_MISSION_CREATOR_BLIP - Blip %d doesn't exist", CTheScripts::GetCurrentScriptNameAndProgramCounter(), iBlipIndex))
	{
		CMiniMap::SetBlipParameter(BLIP_CHANGE_MISSION_CREATOR, iBlipIndex, bValue);
	}
}



//
// name:		CommandIsMissionCreatorBlip
// description:	returns whether this blip is a "mission creator" blip
bool CommandIsMissionCreatorBlip(s32 iBlipIndex)
{
	if (scriptVerifyf(iBlipIndex != INVALID_BLIP_ID, "%s: IS_MISSION_CREATOR_BLIP - Blip %d doesn't exist", CTheScripts::GetCurrentScriptNameAndProgramCounter(), iBlipIndex))
	{
		CMiniMapBlip *pBlip = CMiniMap::GetBlip(iBlipIndex);

		if (pBlip)
		{
			return (CMiniMap::IsFlagSet(pBlip, BLIP_FLAG_MISSION_CREATOR));
		}
	}

	return false;
}



//
// name:		CommandGetNewSelectedMissionCreatorBlip
// description:	returns INVALID_BLIP_ID if not hovering otherwise a valid current blip index
s32 CommandGetNewSelectedMissionCreatorBlip()
{
	if (CPauseMenu::IsActive() && CPauseMenu::IsInMapScreen() && CPauseMenu::IsNavigatingContent())
	{
		s32 iMissionCreatorBlipInUse = CPauseMenu::GetCurrentSelectedMissionCreatorBlip();

		if (iMissionCreatorBlipInUse != INVALID_BLIP_ID)
		{
			uiDisplayf("GET_NEW_SELECTED_MISSION_CREATOR_BLIP returning: %d", iMissionCreatorBlipInUse);
		}

		return (iMissionCreatorBlipInUse);
	}
	
	return INVALID_BLIP_ID;
}



//
// name:		CommandIsHoveringOverMissionCreatorBlip
// description:	returns INVALID_BLIP_ID if not hovering otherwise a valid current blip index
bool CommandIsHoveringOverMissionCreatorBlip()
{
	if (CPauseMenu::IsActive() && CPauseMenu::IsInMapScreen() && CPauseMenu::IsNavigatingContent())
	{
		return (CPauseMenu::IsHoveringOnMissionCreatorBlip());
	}
	
	return false;
}



//
// name:		CommandShowStartMissionInstructionalButton
// description:	displays the instructional button for start mission on the pausemap screen
void CommandShowStartMissionInstructionalButton(bool bShow)
{
	if (CPauseMenu::IsActive() && CPauseMenu::IsInMapScreen() && CPauseMenu::IsNavigatingContent())
	{
		CMapMenu::ShowStartMissionInstructionalButton(bShow);
		CPauseMenu::RedrawInstructionalButtons(MENU_UNIQUE_ID_MAP);
	}
}

//
// name:		CommandShowContactInstructionalButton
// description:	displays the instructional button for start mission on the pausemap screen
void CommandShowContactInstructionalButton(bool bShow)
{
	if (CPauseMenu::IsActive() && CPauseMenu::IsInMapScreen() && CPauseMenu::IsNavigatingContent())
	{
		CMapMenu::ShowContactInstructionalButton(bShow);
		CPauseMenu::RedrawInstructionalButtons(MENU_UNIQUE_ID_MAP);
	}
}

void CommandReloadMapMenu()
{
	if (CPauseMenu::IsActive() && CPauseMenu::IsInMapScreen())
	{
		CMapMenu::ReloadMap();
	}
}

//
// name:		CommandSetBlipFlashes
// description:	
void CommandSetBlipFlashes(s32 iBlipIndex, bool bOnOff)
{
	if (scriptVerifyf(iBlipIndex != INVALID_BLIP_ID, "%s: SET_BLIP_FLASHES - Blip %d doesn't exist", CTheScripts::GetCurrentScriptNameAndProgramCounter(), iBlipIndex))
	{
		CMiniMap::SetBlipParameter(BLIP_CHANGE_FLASH, iBlipIndex, bOnOff);
	}
}


//
// name:		CommandIsBlipFlashing
// description:	
bool CommandIsBlipFlashing(s32 iBlipIndex)
{
	if (scriptVerifyf(iBlipIndex != INVALID_BLIP_ID, "%s: IS_BLIP_FLASHING - Blip %d doesn't exist", CTheScripts::GetCurrentScriptNameAndProgramCounter(), iBlipIndex))
	{
		CMiniMapBlip *pBlip = CMiniMap::GetBlip(iBlipIndex);

		if (pBlip)
		{
			return (CMiniMap::IsFlagSet(pBlip, BLIP_FLAG_FLASHING));
		}
	}

	return false;
}




//
// name:		CommandSetMarkerLongDistance
// description:	
void CommandSetBlipMarkerLongDistance(s32 iBlipIndex, bool bOnOff)
{
	if (scriptVerifyf(iBlipIndex != INVALID_BLIP_ID, "%s: SET_BLIP_MARKER_LONG_DISTANCE - Blip %d doesn't exist", CTheScripts::GetCurrentScriptNameAndProgramCounter(), iBlipIndex))
	{
		CMiniMap::SetBlipParameter(BLIP_CHANGE_MARKER_LONG_DISTANCE, iBlipIndex, bOnOff);
	}
}


//
// name:		CommandFlashBlipAlt
// description:	
void CommandSetBlipFlashesAlternate(s32 iBlipIndex, bool bOnOff)
{
	if (scriptVerifyf(iBlipIndex != INVALID_BLIP_ID, "%s: SET_BLIP_FLASHES_ALTERNATE - Blip %d doesn't exist", CTheScripts::GetCurrentScriptNameAndProgramCounter(), iBlipIndex))
	{
		CMiniMap::SetBlipParameter(BLIP_CHANGE_FLASH_ALT, iBlipIndex, bOnOff);
	}
}

//
// name:		CommandSetBlipAsShortRange
// description:	
void CommandSetBlipAsShortRange(s32 iBlipIndex, bool bShortRange)
{
	if (scriptVerifyf(iBlipIndex != INVALID_BLIP_ID, "%s: SET_BLIP_AS_SHORT_RANGE - Blip %d doesn't exist", CTheScripts::GetCurrentScriptNameAndProgramCounter(), iBlipIndex))
	{
		CMiniMap::SetBlipParameter(BLIP_CHANGE_SHORTRANGE, iBlipIndex, bShortRange);
	}
}


//
// name:		CommandSetRoute
// description:	
void CommandSetRoute(s32 iBlipIndex, bool bEnable)
{
	if (scriptVerifyf(iBlipIndex != INVALID_BLIP_ID, "%s: SET_BLIP_ROUTE - Blip %d doesn't exist", CTheScripts::GetCurrentScriptNameAndProgramCounter(), iBlipIndex))
	{
		scriptDebugf3("%s: SET_BLIP_ROUTE - %s blip route for Blip %d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), bEnable ? "Enabling" : "Disabling", iBlipIndex);
		CMiniMap::SetBlipParameter(BLIP_CHANGE_ROUTE, iBlipIndex, bEnable);
	}
}

void CommandClearAllBlipRoutes()
{
	scriptDebugf3("%s: CLEAR_ALL_BLIP_ROUTES - Clearing blip routes", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	CMiniMap::ClearAllBlipRoutes();
}

//
// name:		CommandSetBlipRouteColour
// description:	
void CommandSetBlipRouteColour(s32 iBlipIndex, s32 iRouteColour)
{
	if (scriptVerifyf(iBlipIndex != INVALID_BLIP_ID, "%s: SET_BLIP_ROUTE_COLOUR - Blip %d doesn't exist", CTheScripts::GetCurrentScriptNameAndProgramCounter(), iBlipIndex))
	{
		CMiniMap::SetBlipParameter(BLIP_CHANGE_COLOUR_ROUTE, iBlipIndex, iRouteColour);
	}
}

//
// name:		CommandSetForceShowGPS
// description:
void CommandSetForceShowGPS(bool bShowGPS)
{
	CScriptHud::bForceShowGPS = bShowGPS;
}

void CommandUseSetDestinationInPauseMap(bool bUseSetDestination)
{
	CScriptHud::bSetDestinationInMapMenu = bUseSetDestination;
}

//
// name:		CommandSetBlockWantedFlash
// description:
void CommandSetBlockWantedFlash(bool bBlockWantedFlash)
{
	CScriptHud::bWantsToBlockWantedFlash = bBlockWantedFlash;
}

//
// name:		ChangeBlipScale
// description:	
void CommandChangeBlipScale(s32 iBlipIndex, float fBlipScale)
{
	if (scriptVerifyf(iBlipIndex != INVALID_BLIP_ID, "%s: SET_BLIP_SCALE - Blip %d doesn't exist", CTheScripts::GetCurrentScriptNameAndProgramCounter(), iBlipIndex))
	{
		CMiniMap::SetBlipParameter(BLIP_CHANGE_SCALE, iBlipIndex, fBlipScale);
	}
}

void CommandChangeBlipScale2D(s32 iBlipIndex, float fBlipScaleX, float fBlipScaleY)
{
	if (scriptVerifyf(iBlipIndex != INVALID_BLIP_ID, "%s: SET_BLIP_SCALE_2D - Blip %d doesn't exist", CTheScripts::GetCurrentScriptNameAndProgramCounter(), iBlipIndex))
	{
		CMiniMap::SetBlipParameter(BLIP_CHANGE_SCALE, iBlipIndex, Vector3(fBlipScaleX, fBlipScaleY, 0.0f));
	}
}


// UNUSED COMMAND
void CommandSetTerritoryBlipScale(s32 UNUSED_PARAM(iBlipIndex), float UNUSED_PARAM(fBlipScaleX), float UNUSED_PARAM(fBlipScaleY))
{
}



//
// name:		ChangeBlipDisplay
// description:	Change the way a blip is displayed
void CommandChangeBlipDisplay(s32 iBlipIndex, s32 iBlipDisplay)
{
	if (scriptVerifyf(iBlipIndex != INVALID_BLIP_ID, "%s: SET_BLIP_DISPLAY - Blip %d doesn't exist", CTheScripts::GetCurrentScriptNameAndProgramCounter(), iBlipIndex))
	{
		CMiniMap::SetBlipParameter(BLIP_CHANGE_DISPLAY, iBlipIndex, iBlipDisplay);
	}
}



//
// name:		CommandChangeBlipCategory
// description:	Change the category the blip is in on the pausemap legend
void CommandChangeBlipCategory(s32 iBlipIndex, s32 iBlipCategory)
{
	if (scriptVerifyf(iBlipIndex != INVALID_BLIP_ID, "%s: SET_BLIP_CATEGORY - Blip %d doesn't exist", CTheScripts::GetCurrentScriptNameAndProgramCounter(), iBlipIndex))
	{
		CMiniMap::SetBlipParameter(BLIP_CHANGE_CATEGORY, iBlipIndex, iBlipCategory);
	}
}



//
// name:		ChangeBlipDisplay
// description:	Change the way a blip is displayed
void CommandChangeBlipPriority(s32 iBlipIndex, s32 iBlipPriority)
{
		// removing this for bug 348148 as priority has changed since so we dont need this anymore
// 	if (iBlipPriority == BLIP_PRIORITY_HIGHEST)  // script should never set a blip with HIGHEST as the code should only do this!!!
// 		iBlipPriority = BLIP_PRIORITY_HIGH;

	if (scriptVerifyf(iBlipIndex != INVALID_BLIP_ID, "%s: SET_BLIP_PRIORITY - Blip %d doesn't exist", CTheScripts::GetCurrentScriptNameAndProgramCounter(), iBlipIndex))
	{
/*#if __BANK  // allows us to see exactly what script is doing
		CMiniMapBlip *pBlip = CMiniMap::GetBlip(iBlipIndex);

		if (pBlip)
		{
			if (iBlipPriority != CMiniMap::GetBlipPriorityValue(pBlip))
			{
				sfDisplayf("SET_BLIP_PRIORITY(%d:%s, %d) called by '%s' SCRIPT", iBlipIndex, CMiniMap::GetBlipNameValue(pBlip), iBlipPriority, CTheScripts::GetCurrentScriptName());
			}
		}
#endif  // __BANK*/

		CMiniMap::SetBlipParameter(BLIP_CHANGE_PRIORITY, iBlipIndex, iBlipPriority);
	}
}

//
// name:		ChangeBlipDisplay
// description:	Change the way a blip is displayed
void CommandSetBlipSprite(s32 iBlipIndex, s32 iBlipSprite)
{
	scriptAssertf((/*iBlipSprite != BLIP_LEVEL &&*/ iBlipSprite != BLIP_WAYPOINT), "SET_BLIP_SPRITE - cannot change sprite to value %d", iBlipSprite);

/*#if __DEV
	sfDisplayf("SET_BLIP_SPRITE(%d, %d) called on %d blip by script: %s", iBlipIndex, iBlipSprite, iBlipIndex, CTheScripts::GetCurrentScriptNameAndProgramCounter());
#endif // __DEV*/

	if (scriptVerifyf(iBlipIndex != INVALID_BLIP_ID, "%s: SET_BLIP_SPRITE - Blip %d doesn't exist", CTheScripts::GetCurrentScriptNameAndProgramCounter(), iBlipIndex))
	{
		CMiniMap::SetBlipParameter(BLIP_CHANGE_OBJECT_NAME, iBlipIndex, iBlipSprite);
	}
}


void CommandSetCopBlipSprite(s32 SpriteToUse, float fScale)
{
	CWanted::GetWantedBlips().SetBlipSpriteToUseForCopPeds(SpriteToUse, fScale);
}

void CommandSetCopBlipSpriteAsStandard()
{
	CWanted::GetWantedBlips().SetBlipSpriteToUseForCopPeds(BLIP_POLICE_PED, 0.4f);
}


//
// name:		RemoveBlip
// description:	Remove Radar blip
void CommandRemoveBlip(s32 &iBlipId)
{
	if (CTheScripts::GetCurrentGtaScriptThread()->bThisScriptCanRemoveBlipsCreatedByAnyScript)
	{
		CTheScripts::GetScriptHandlerMgr().RemoveScriptResource(CGameScriptResource::SCRIPT_RESOURCE_RADAR_BLIP, iBlipId); // searches all the script handler lists for the blip
	}
	else
	{
		if (scriptVerifyf(CTheScripts::GetCurrentGtaScriptHandler(), "%s: REMOVE_BLIP(%d) - ScriptHandler is NULL", CTheScripts::GetCurrentScriptNameAndProgramCounter(), iBlipId))  // makes 1488977 ignorable
		{
			if(!CTheScripts::GetCurrentGtaScriptHandler()->RemoveScriptResource(CGameScriptResource::SCRIPT_RESOURCE_RADAR_BLIP, iBlipId))
			{
				if(iBlipId != 0)
				{
					scriptWarningf("%s - REMOVE_BLIP - failed to find a blip with id %d for this script. Maybe the blip was created by another script or has already been deleted. It could also be that the blip was attached to a pickup or entity that has already been deleted.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), iBlipId);
				}
			}
		}
	}

	iBlipId = 0;
}

void CommandSetBlipAsFriendly(s32 iBlipIndex, s32 iFriendly)
{
	if (scriptVerifyf(iBlipIndex != INVALID_BLIP_ID, "%s: SET_BLIP_AS_FRIENDLY - Blip %d doesn't exist", CTheScripts::GetCurrentScriptNameAndProgramCounter(), iBlipIndex))
	{
		CMiniMap::SetBlipParameter(BLIP_CHANGE_FRIENDLY, iBlipIndex, iFriendly);
	}
}

void CommandSetBlipAsDead(s32 UNUSED_PARAM(iBlipIndex), bool UNUSED_PARAM(bIsDead))
{
	scriptAssertf(0, "SET_BLIP_AS_DEAD - depreciated command!  shouldnt be used!");

/*	if (scriptVerifyf(iBlipIndex != INVALID_BLIP_ID, "%s: SET_BLIP_AS_DEAD - Blip %d doesn't exist", CTheScripts::GetCurrentScriptNameAndProgramCounter(), iBlipIndex))
	{
		CMiniMap::SetBlipParameter(BLIP_CHANGE_DEAD, iBlipIndex, bIsDead);
	}*/
}

void CommandPulseBlip(s32 iBlipIndex)
{
	if (scriptVerifyf(iBlipIndex != INVALID_BLIP_ID, "%s: PULSE_BLIP - Blip %d doesn't exist", CTheScripts::GetCurrentScriptNameAndProgramCounter(), iBlipIndex))
	{
		CMiniMap::SetBlipParameter(BLIP_CHANGE_PULSE, iBlipIndex, true);
	}
}

void CommandShowNumberOnBlip(s32 iBlipIndex, s32 iNumberToShow)
{
	if (scriptVerifyf(iBlipIndex != INVALID_BLIP_ID, "%s: SHOW_NUMBER_ON_BLIP - Blip %d doesn't exist", CTheScripts::GetCurrentScriptNameAndProgramCounter(), iBlipIndex))
	{
		if (scriptVerifyf(iNumberToShow >= 0 && iNumberToShow <= 99, "%s: SHOW_NUMBER_ON_BLIP - Blip %d - Number must be between 0 and 99", CTheScripts::GetCurrentScriptNameAndProgramCounter(), iBlipIndex))
		{
			CMiniMap::SetBlipParameter(BLIP_CHANGE_SHOW_NUMBER, iBlipIndex, iNumberToShow);
		}
		else
		{
			CMiniMap::SetBlipParameter(BLIP_CHANGE_SHOW_NUMBER, iBlipIndex, -1);
		}
	}
}

void CommandHideNumberOnBlip(s32 iBlipIndex)
{
	if (scriptVerifyf(iBlipIndex != INVALID_BLIP_ID, "%s: HIDE_NUMBER_ON_BLIP - Blip %d doesn't exist", CTheScripts::GetCurrentScriptNameAndProgramCounter(), iBlipIndex))
	{
		CMiniMap::SetBlipParameter(BLIP_CHANGE_SHOW_NUMBER, iBlipIndex, -1);
	}
}

void CommandShowHeightOnBlip(s32 iBlipIndex, bool bShowHeight)
{
	if (scriptVerifyf(iBlipIndex != INVALID_BLIP_ID, "%s: SHOW_HEIGHT_ON_BLIP - Blip %d doesn't exist", CTheScripts::GetCurrentScriptNameAndProgramCounter(), iBlipIndex))
	{
/*#if __DEV
		CMiniMapBlip *pBlip = CMiniMap::GetBlip(iBlipIndex);

		if (pBlip)
		{
			if (bShowHeight)
			{
				sfDisplayf("SHOW_HEIGHT_ON_BLIP(%d, TRUE) called on %s %d blip by script: %s", iBlipIndex, CMiniMap::GetBlipNameValue(pBlip), iBlipIndex, CTheScripts::GetCurrentScriptNameAndProgramCounter());
			}
			else
			{
				sfDisplayf("SHOW_HEIGHT_ON_BLIP(%d, FALSE) called on %s %d blip by script: %s", iBlipIndex, CMiniMap::GetBlipNameValue(pBlip), iBlipIndex, CTheScripts::GetCurrentScriptNameAndProgramCounter());
			}
		}
#endif // __DEV*/

		CMiniMap::SetBlipParameter(BLIP_CHANGE_SHOW_HEIGHT, iBlipIndex, bShowHeight);
	}
}

void CommandShowTickOnBlip(s32 iBlipIndex, bool bShowTick)
{
	if (scriptVerifyf(iBlipIndex != INVALID_BLIP_ID, "%s: SHOW_TICK_ON_BLIP - Blip %d doesn't exist", CTheScripts::GetCurrentScriptNameAndProgramCounter(), iBlipIndex))
	{
		CMiniMap::SetBlipParameter(BLIP_CHANGE_SHOW_TICK, iBlipIndex, bShowTick);
	}
}

void CommandShowGoldTickOnBlip(s32 iBlipIndex, bool bShowGoldTick)
{
	if (scriptVerifyf(iBlipIndex != INVALID_BLIP_ID, "%s: SHOW_GOLD_TICK_ON_BLIP - Blip %d doesn't exist", CTheScripts::GetCurrentScriptNameAndProgramCounter(), iBlipIndex))
	{
		CMiniMap::SetBlipParameter(BLIP_CHANGE_SHOW_TICK_GOLD, iBlipIndex, bShowGoldTick);
	}
}

void CommandShowForSaleIconOnBlip(s32 iBlipIndex, bool bShow)
{
	if (scriptVerifyf(iBlipIndex != INVALID_BLIP_ID, "%s: SHOW_FOR_SALE_ICON_ON_BLIP - Blip %d doesn't exist", CTheScripts::GetCurrentScriptNameAndProgramCounter(), iBlipIndex))
	{
		CMiniMap::SetBlipParameter(BLIP_CHANGE_SHOW_FOR_SALE, iBlipIndex, bShow);
	}
}

void CommandShowHeadingIndicatorOnBlip(s32 iBlipIndex, bool bShow)
{
	if (scriptVerifyf(iBlipIndex != INVALID_BLIP_ID, "%s: SHOW_HEADING_ON_BLIP - Blip %d doesn't exist", CTheScripts::GetCurrentScriptNameAndProgramCounter(), iBlipIndex))
	{
		CMiniMap::SetBlipParameter(BLIP_CHANGE_SHOW_HEADING_INDICATOR, iBlipIndex, bShow);
	}
}

void CommandShowOutlineIndicatorOnBlip(s32 iBlipIndex, bool bShow)
{
	if (scriptVerifyf(iBlipIndex != INVALID_BLIP_ID, "%s: SHOW_OUTLINE_INDICATOR_ON_BLIP - Blip %d doesn't exist", CTheScripts::GetCurrentScriptNameAndProgramCounter(), iBlipIndex))
	{
		CMiniMap::SetBlipParameter(BLIP_CHANGE_SHOW_OUTLINE_INDICATOR, iBlipIndex, bShow);
	}
}

void CommandShowFriendIndicatorOnBlip(s32 iBlipIndex, bool bShow)
{
	if (scriptVerifyf(iBlipIndex != INVALID_BLIP_ID, "%s: SHOW_FRIEND_INDICATOR_ON_BLIP - Blip %d doesn't exist", CTheScripts::GetCurrentScriptNameAndProgramCounter(), iBlipIndex))
	{
		CMiniMap::SetBlipParameter(BLIP_CHANGE_SHOW_FRIEND_INDICATOR, iBlipIndex, bShow);
	}
}

void CommandShowCrewIndicatorOnBlip(s32 iBlipIndex, bool bShow)
{
	if (scriptVerifyf(iBlipIndex != INVALID_BLIP_ID, "%s: SHOW_CREW_INDICATOR_ON_BLIP - Blip %d doesn't exist", CTheScripts::GetCurrentScriptNameAndProgramCounter(), iBlipIndex))
	{
		CMiniMap::SetBlipParameter(BLIP_CHANGE_SHOW_CREW_INDICATOR, iBlipIndex, bShow);
	}
}

void CommandSetBlipShortHeightThreshold(s32 iBlipIndex, bool bUseShortHeightThreshold)
{
	if (scriptVerifyf(iBlipIndex != INVALID_BLIP_ID, "%s: SET_BLIP_SHORT_HEIGHT_THRESHOLD - Blip %d doesn't exist", CTheScripts::GetCurrentScriptNameAndProgramCounter(), iBlipIndex))
	{
		CMiniMap::SetBlipParameter(BLIP_CHANGE_SHORT_HEIGHT_THRESHOLD, iBlipIndex, bUseShortHeightThreshold);
	}
}

void CommandSetBlipExtendedHeightThreshold(s32 iBlipIndex, bool bUseExtendedHeightThreshold)
{
	if (scriptVerifyf(iBlipIndex != INVALID_BLIP_ID, "%s: SET_BLIP_EXTENDED_HEIGHT_THRESHOLD - Blip %d doesn't exist", CTheScripts::GetCurrentScriptNameAndProgramCounter(), iBlipIndex))
	{
		CMiniMap::SetBlipParameter(BLIP_CHANGE_EXTENDED_HEIGHT_THRESHOLD, iBlipIndex, bUseExtendedHeightThreshold);
	}
}

void CommandSetBlipUseHeightIndicatorOnEdge(s32 iBlipIndex, bool bUseHeight)
{
	if (scriptVerifyf(iBlipIndex != INVALID_BLIP_ID, "%s: SET_BLIP_USE_HEIGHT_INDICATOR_ON_EDGE - Blip %d doesn't exist", CTheScripts::GetCurrentScriptNameAndProgramCounter(), iBlipIndex))
	{
		CMiniMap::SetBlipParameter(BLIP_CHANGE_HEIGHT_ON_EDGE, iBlipIndex, bUseHeight);
	}
}

void CommandSetBlipAsMinimalOnEdge(s32 iBlipIndex, bool bMinimiseOnEdge)
{
	if (scriptVerifyf(iBlipIndex != INVALID_BLIP_ID, "%s: SET_BLIP_AS_MINIMAL_ON_EDGE - Blip %d doesn't exist", CTheScripts::GetCurrentScriptNameAndProgramCounter(), iBlipIndex))
	{
		CMiniMap::SetBlipParameter(BLIP_CHANGE_MINIMISE_ON_EDGE, iBlipIndex, bMinimiseOnEdge);
	}
}

void CommandSetAreaBlipEdge(s32 iBlipIndex, bool bEdge)
{
	if (scriptVerifyf(iBlipIndex != INVALID_BLIP_ID, "%s: SET_AREA_BLIP_EDGE - Blip %d doesn't exist", CTheScripts::GetCurrentScriptNameAndProgramCounter(), iBlipIndex))
	{
		CMiniMap::SetBlipParameter(BLIP_CHANGE_AREA_EDGE, iBlipIndex, bEdge);
	}
}

void CommandSetRadiusBlipEdge(s32 iBlipIndex, bool bEdge)
{
	if (scriptVerifyf(iBlipIndex != INVALID_BLIP_ID, "%s: SET_RADIUS_BLIP_EDGE - Blip %d doesn't exist", CTheScripts::GetCurrentScriptNameAndProgramCounter(), iBlipIndex))
	{
		CMiniMap::SetBlipParameter(BLIP_CHANGE_RADIUS_EDGE, iBlipIndex, bEdge);
	}
}

void CommandSetBlipBright(int iBlipIndex, s32 iOrOrOff)
{
	if (scriptVerifyf(iBlipIndex != INVALID_BLIP_ID, "%s: SET_BLIP_BRIGHT - Blip %d doesn't exist", CTheScripts::GetCurrentScriptNameAndProgramCounter(), iBlipIndex))
	{
		CMiniMap::SetBlipParameter(BLIP_CHANGE_BRIGHTNESS, iBlipIndex, iOrOrOff);
	}
}

void CommandSetBlipShowCone(s32 iBlipIndex, bool bShowCone, s32 sHudColor)
{
	if (scriptVerifyf(iBlipIndex != INVALID_BLIP_ID, "%s: SET_BLIP_SHOW_CONE - Blip %d doesn't exist", CTheScripts::GetCurrentScriptNameAndProgramCounter(), iBlipIndex))
	{
		if(bShowCone)
		{
			CMiniMap::AddConeColorForBlip(iBlipIndex, static_cast<eHUD_COLOURS>(sHudColor));
		}
		CMiniMap::SetBlipParameter(BLIP_CHANGE_SHOW_CONE, iBlipIndex, bShowCone);
	}
}

void CommandSetupFakeConeData(int iBlipIndex, float fVisualFieldMinAzimuthAngle, float fVisualFieldMaxAzimuthAngle, float fCentreOfGazeMaxAngle, float fPeripheralRange, float fFocusRange, float fRotation, bool bContinuousUpdate, s32 sHudColor)
{
	if (scriptVerifyf(iBlipIndex != INVALID_BLIP_ID, "%s: SETUP_FAKE_CONE_DATA - Blip %d doesn't exist", CTheScripts::GetCurrentScriptNameAndProgramCounter(), iBlipIndex))
	{
		CMiniMap::AddFakeConeData(iBlipIndex, fVisualFieldMinAzimuthAngle, fVisualFieldMaxAzimuthAngle, 
			fCentreOfGazeMaxAngle, fPeripheralRange, fFocusRange, fRotation, bContinuousUpdate, static_cast<eHUD_COLOURS>(sHudColor));
	}
}

void CommandDestroyFakeConeData(int iBlipIndex)
{
	if (scriptVerifyf(iBlipIndex != INVALID_BLIP_ID, "%s: REMOVE_FAKE_CONE_DATA - Blip %d doesn't exist", CTheScripts::GetCurrentScriptNameAndProgramCounter(), iBlipIndex))
	{
		CMiniMap::DestroyFakeConeData(iBlipIndex);
	}
}

void CommandClearFakeConeArray()
{
	CMiniMap::ClearFakeConesArray();
}

void CommandShowAccountPicker()
{
	scriptDisplayf("SHOW_ACCOUNT_PICKER - Showing signin UI from script.");
	CLiveManager::ShowSigninUi();
}

void CommandSetMPPropertyOwner(s32 iPropertyId, s32 iOwner1, s32 iOwner2, s32 iOwner3, s32 iOwner4)
{
	CMiniMap::ToggleTerritory(iPropertyId, iOwner1, iOwner2, iOwner3, iOwner4);
}


void CommandShowMinimapYoke(const bool bVisible, const float fPosX, const float fPosY, const s32 iAlpha)
{
	CMiniMap::ShowYoke(bVisible, fPosX, fPosY, iAlpha);
}

void CommandShowMinimapSonarSweep(const bool bVisible)
{
	CMiniMap::ShowSonarSweep(bVisible);
}

bool CommandSetMiniMapComponent(int const c_componentId, bool const c_OnOff, s32 const c_color)
{
	CMiniMap::ToggleComponent(c_componentId, c_OnOff, false, static_cast<eHUD_COLOURS>(c_color));

	return true;
}



s32 CommandGetMainPlayerBlipId()
{
	s32 iUniqueBlipId = INVALID_BLIP_ID;

	CMiniMapBlip *pBlip = CMiniMap::GetBlipFromActualId(CMiniMap::GetActualCentreBlipId());

	if (pBlip)
	{
		iUniqueBlipId = CMiniMap::GetUniqueBlipUsed(pBlip);
	}

	return iUniqueBlipId;
}



bool CommandDoesBlipExist(int iBlipIndex)
{
	if (iBlipIndex == INVALID_BLIP_ID)
	{
//		scriptAssertf(0, "DOES_BLIP_EXIST - Blip index passed into DOES_BLIP_EXIST is invalid (NULL)");  // cannot do this as they call it all the time with invalid blip id
		return false;
	}

	return CMiniMap::IsBlipIdInUse(iBlipIndex);
}

void CommandSetWaypoinGpsFlags(int iFlags)
{
	scriptDisplayf("CommandSetWaypoinGPSFlags - Setting waypoint flags = %d", iFlags);
	CMiniMap::SetWaypointGpsFlags((u16)iFlags);
}

void CommandSetWaypointOff()
{
	scriptDisplayf("CommandSetWaypointOff - Clearing Waypoint");
	CMiniMap::SwitchOffWaypoint();
}

void CommandDeleteWaypointsFromThisPlayer()
{
	if (CMiniMap::IsWaypointLocallyOwned())
	{
		scriptDisplayf("CommandDeleteWaypointsFromThisPlayer - Clearing Waypoint");
		CMiniMap::SwitchOffWaypoint();
	}
}

void CommandRefreshWaypoint()
{
	CMiniMap::UpdateWaypoint();
}

bool CommandIsWaypointActive()
{
	return (CMiniMap::IsWaypointActive());
}

bool CommandIsWaypointActiveForThisPlayer(int PlayerModelHashKey)
{
	return (CMiniMap::IsPlayerWaypointActive(PlayerModelHashKey));
}

void CommandSetNewWaypoint(float x, float y)
{
	if(!CMiniMap::IsActiveWaypoint(Vector2(x, y)))
	{
		scriptDisplayf("CommandSetNewWaypoint - Clearing Waypoint");
		CMiniMap::SwitchOffWaypoint();
		CMiniMap::SwitchOnWaypoint(x, y, NETWORK_INVALID_OBJECT_ID, false);
	}
}

bool CommandIsEntityWaypointAllowed()
{
	return CMiniMap::IsEntityWaypointAllowed();
}

void CommandSetEntityWaypointAllowed(bool Value)
{
	CMiniMap::SetEntityWaypointAllowed(Value);
}

int CommandGetWaypointClearOnArrivalMode()
{
	return (int)CMiniMap::GetWaypointClearOnArrivalMode();
}

void CommandSetWaypointClearOnArrivalMode(int Value)
{
	if (uiVerifyf(Value >= 0 && Value < (int)eWaypointClearOnArrivalMode::MAX, "CommandSetWaypointClearOnArrivalMode: value %d is outside eWaypointClearOnArrivalMode range", Value))
	{
		CMiniMap::SetWaypointClearOnArrivalMode((eWaypointClearOnArrivalMode)Value);
	}
}

void CommandGetHudColour(int HudColour, int& red, int& green, int& blue, int& alpha)
{
	if(SCRIPT_VERIFY( ( (HudColour >= 0) && (HudColour < HUD_COLOUR_MAX_COLOURS ) ), "GET_HUD_COLOUR - Invalid Index"))
	{
		CRGBA rgba = CHudColour::GetRGBA((eHUD_COLOURS)HudColour);
		red = rgba.GetRed();
		green = rgba.GetGreen();
		blue = rgba.GetBlue();
		alpha = rgba.GetAlpha();
	}

	//Displayf("GET_HUD_COLOUR called with HUD COLOUR %d =  %d,%d,%d,%d", HudColour, red, green, blue, alpha);
}

void CommandSetScriptVariableHudColour(int Red, int Green, int Blue, int Alpha)
{
	CHudColour::SetRGBAValue(HUD_COLOUR_SCRIPT_VARIABLE, Red, Green, Blue, Alpha);
}

void CommandSetSecondScriptVariableHudColour(int Red, int Green, int Blue, int Alpha)
{
	CHudColour::SetRGBAValue(HUD_COLOUR_SCRIPT_VARIABLE_2, Red, Green, Blue, Alpha);
}

void CommandReplaceHudColour(int destHudColour, int srcHudColour)
{
	uiDisplayf("REPLACE_HUD_COLOUR - called by script %s (%d), Replaced %d with %d", scrThread::GetActiveThread()->GetScriptName(), CTheScripts::GetCurrentGtaScriptThread()->GetThreadId(), destHudColour, srcHudColour);

	CHudColour::SetIntColour((eHUD_COLOURS)destHudColour, CHudColour::GetIntColour((eHUD_COLOURS)srcHudColour));
}

void CommandReplaceHudColourWithRGBA(int destHudColour, int Red, int Green, int Blue, int Alpha)
{
	uiDisplayf("REPLACE_HUD_COLOUR_WITH_RGBA - called by script %s (%d), Replaced %d with %d,%d,%d,%d", scrThread::GetActiveThread()->GetScriptName(), CTheScripts::GetCurrentGtaScriptThread()->GetThreadId(), destHudColour, Red, Green, Blue, Alpha);

	CHudColour::SetRGBAValue((eHUD_COLOURS)destHudColour, Red, Green, Blue, Alpha);
}

void CommandSetCustomMPHudColor(int customHudColor)
{
	CTheScripts::SetCustomMPHudColor(customHudColor);
	CNewHud::HandleCharacterChange();
}

s32 CommandGetCustomMPHudColor()
{
	return CTheScripts::GetCustomMPHudColor();
}

void CommandSetTickColorOverride(int UNUSED_PARAM(customTickColor))
{
	// DEPRECATED
}

void CommandSetAbilityBarVisibility(bool bTurnOn)
{
	CNewHud::SetToggleAbilityBar(bTurnOn);
}

void CommandAllowAbilityBar(bool bAllow)
{
	if(!bAllow)
	{
		CNewHud::SetToggleAbilityBar(false);
	}
	CNewHud::SetAllowAbilityBar(bAllow);
}

void CommandFlashAbilityBar(int millisecondsToFlash)
{
	CNewHud::BlinkAbilityBar(millisecondsToFlash);
}

void CommandSetAbilityBarGlow(bool bTurnOn)
{
	uiDisplayf("SET_ABILITY_BAR_GLOW- called by script %s (%d), Value: %d.", scrThread::GetActiveThread()->GetScriptName(), CTheScripts::GetCurrentGtaScriptThread()->GetThreadId(), bTurnOn);
	CNewHud::SetAbilityBarGlow(bTurnOn);
}

void CommandSetAbilityBarValue(float fPercentage, float fMaxValue)
{
	CNewHud::SetAbilityOverride(fPercentage, fMaxValue);
}

void CommandSetHudDisplayMode(int iDisplayMode)
{
	if (iDisplayMode >= 0 && iDisplayMode < CNewHud::DM_NUM_DISPLAY_MODES)
	{
		CNewHud::SetDisplayMode((CNewHud::eDISPLAY_MODE)iDisplayMode);
	}
}

void CommandFlashWantedDisplay(bool bShouldFlash)
{
	CScriptHud::bFlashWantedStarDisplay = bShouldFlash;
}

void CommandbForceOffWantedStarFlash(bool bShouldForceOff)
{
	CScriptHud::bForceOffWantedStarFlash = bShouldForceOff;
}

void CommandUpdateWantedDrainLevel(int iDrainLevelPercentage)
{
	CScriptHud::bUpdateWantedDrainLevel = true;
	CScriptHud::iWantedDrainLevelPercentage = iDrainLevelPercentage;
}

void CommandbUpdateWantedThreatVisibility(bool bIsVisible)
{
	CScriptHud::bUpdateWantedThreatVisibility = true;
	CScriptHud::bIsWantedThreatVisible = bIsVisible;
}

void CommandForceOnWantedStarFlash(bool bShouldForceOn)
{
	CScriptHud::bForceOnWantedStarFlash = bShouldForceOn;
}

void CommandDisplayAreaName(bool bValue)
{
	if (bValue)
	{
		CNewHud::SetHudComponentToBeShown(NEW_HUD_AREA_NAME);
	}
	else
	{
		CNewHud::SetHudComponentToBeHidden(NEW_HUD_AREA_NAME);
	}
}

void CommandDisplayCash(bool bValue)
{
	if (bValue)
	{
		CNewHud::SetHudComponentToBeShown(NEW_HUD_CASH);
	}
	else
	{
		CNewHud::SetHudComponentToBeHidden(NEW_HUD_CASH);
	}
}

void CommandUseFakeMPCash(bool bUseFakeMPCash)
{
	CNewHud::UseFakeMPCash(bUseFakeMPCash);
}

void CommandChangeFakeMPCash(int iWalletCashDifference, int iBankCashDifference)
{
	CNewHud::ChangeFakeMPCash(iWalletCashDifference, iBankCashDifference);
}

void CommandFakeSPCashChange(int iAmountChanged)
{
	CNewHud::DisplayFakeSPCash(iAmountChanged);
}

void CommandDisplayAmmoThisFrame(bool bValue)
{
	if (bValue)
	{
		CNewHud::SetHudComponentToBeShown(NEW_HUD_WEAPON_ICON);
	}
	else
	{
		CNewHud::SetHudComponentToBeHidden(NEW_HUD_WEAPON_ICON);
	}
}

void CommandDisplaySniperScopeThisFrame()
{
	CNewHud::SetHudComponentToBeShown(NEW_HUD_RETICLE);
}

void CommandHideHudAndRadarThisFrame()
{
#if __BANK
	static u32 iFrameNumCheck = 0;

	u32 iThisFrameNum = GRCDEVICE.GetFrameCounter();

	if (iThisFrameNum > iFrameNumCheck + 150 || iThisFrameNum < iFrameNumCheck)
	{
		scrThread::PrePrintStackTrace();
		uiDebugf1("HIDE_HUD_AND_RADAR_THIS_FRAME() is being called by script: %s Check the lines above for the script callstack (note this log is not outputted every frame)", CTheScripts::GetCurrentScriptNameAndProgramCounter());

		iFrameNumCheck = iThisFrameNum;
	}
#endif // __BANK

	CScriptHud::bDontDisplayHudOrRadarThisFrame = true;  // dont display for 1 frame only
}

void CommandDontZoomMiniMapWhenRunningThisFrame()
{
	CScriptHud::bDontZoomMiniMapWhenRunningThisFrame = true;  // dont zoom for 1 frame only
}

void CommandDontZoomMiniMapWhenSnipingThisFrame()
{
	CScriptHud::bDontZoomMiniMapWhenSnipingThisFrame = true;
}


void CommandDontTiltMiniMapThisFrame()
{
	CScriptHud::bDontTiltMiniMapThisFrame = true;  // dont zoom for 1 frame only
}

void CommandAllowDisplayOfMultiplayerCashText(bool bAllow)
{
	CScriptHud::ms_bAllowDisplayOfMultiplayerCashText = bAllow;
}


void CommandSetMultiplayerWalletCash()
{
	CScriptHud::ms_bTurnOnMultiplayerWalletCashThisFrame = true;
	CScriptHud::ms_bTurnOffMultiplayerWalletCashThisFrame = false;
}

void CommandRemoveMultiplayerWalletCash()
{
	CScriptHud::ms_bTurnOnMultiplayerWalletCashThisFrame = false;
	CScriptHud::ms_bTurnOffMultiplayerWalletCashThisFrame = true;
}


void CommandSetMultiplayerBankCash()
{
	CScriptHud::ms_bTurnOnMultiplayerBankCashThisFrame = true;
	CScriptHud::ms_bTurnOffMultiplayerBankCashThisFrame = false;
}

void CommandRemoveMultiplayerBankCash()
{
	CScriptHud::ms_bTurnOnMultiplayerBankCashThisFrame = false;
	CScriptHud::ms_bTurnOffMultiplayerBankCashThisFrame = true;
}

void CommandSetMultiplayerHudCash(int UNUSED_PARAM(iCash), bool UNUSED_PARAM(bForever))
{
// 	CommandSetMultiplayerWalletCash();
// 	CommandSetMultiplayerBankCash();
}

void CommandRemoveMultiplayerHudCash()
{
	CommandRemoveMultiplayerWalletCash();
	CommandRemoveMultiplayerBankCash();
}

void CommandSetHudCashAsCopStar(bool bActive)
{
	CScriptHud::bDisplayCashStar = bActive;
}



void CommandHideLoadingOnFadeThisFrame()
{
	CScriptHud::bHideLoadingAnimThisFrame = true;
}



void CommandSetRadarAsInteriorThisFrame(s32 iInteriorHash, float fInteriorPositionX, float fInteriorPositionY, s32 iInteriorRotation, s32 iLevel)
{
	CScriptHud::FakeInterior.bActive = true;
	
	// OPTIONAL: if we also have interior hash then we setup the fake interior map information aswell
	if (iInteriorHash != 0)
	{
		CScriptHud::FakeInterior.iHash = (u32)iInteriorHash;
		CScriptHud::FakeInterior.vPosition = Vector2(fInteriorPositionX, fInteriorPositionY);
		CScriptHud::FakeInterior.iRotation = iInteriorRotation;
		CScriptHud::FakeInterior.iLevel = iLevel;
	}
}

void CommandsSetInsideVerySmallInterior(bool bVerySmallInterior)
{
	CScriptHud::ms_bUseVerySmallInteriorZoom = bVerySmallInterior;
}

void CommandsSetInsideVeryLargeInterior(bool bVeryLargeInterior)
{
	CScriptHud::ms_bUseVeryLargeInteriorZoom = bVeryLargeInterior;
}

void CommandSetRadarAsExteriorThisFrame()
{
	CScriptHud::ms_bFakeExteriorThisFrame = true;
}


void CommandSetFakePauseMapPlayerPositionThisFrame(float fPosX, float fPosY)
{
	CScriptHud::vFakePauseMapPlayerPos = Vector2(fPosX, fPosY);
}

void CommandSetFakeGPSPlayerPositionThisFrame(float fPosX, float fPosY, float fPosZ )
{
    CScriptHud::vFakeGPSPlayerPos = Vector3(fPosX, fPosY, fPosZ);
}

void CommandSetFakePauseMapPlayerPositionThisInterior(float fPosX, float fPosY)
{
	if (scriptVerifyf(CMiniMap::GetIsInsideInterior(true),
		"SET_FAKE_PAUSEMAP_PLAYER_POSITION_THIS_INTERIOR called by script %s while not in an interior!", scrThread::GetActiveThread()->GetScriptName()))
	{
		CScriptHud::vInteriorFakePauseMapPlayerPos.Set(fPosX, fPosY);
		if (CScriptHud::FakeInterior.bActive && CScriptHud::FakeInterior.iHash != 0)
		{
			CScriptHud::ms_iCurrentFakedInteriorHash = CScriptHud::FakeInterior.iHash;
		}
		else
		{
			CPed *pLocalPlayer = CMiniMap::GetMiniMapPed();
			CScriptHud::ms_iCurrentFakedInteriorHash = pLocalPlayer->GetPortalTracker()->GetInteriorNameHash();
		}
	}
}


bool CommandIsPauseMapInInteriorMode()
{
	return CMapMenu::GetIsInInteriorMode();
}


void CommandHideMiniMapExteriorMapThisFrame()
{
	CScriptHud::ms_bHideMiniMapExteriorMapThisFrame = true;
}

void CommandHideMiniMapInteriorMapThisFrame()
{
	CScriptHud::ms_bHideMiniMapInteriorMapThisFrame = true;
}

void CommandSetUseIslandMap(bool bUseIslandMap)
{
	CMiniMap::SetOnIslandMap(bUseIslandMap);
}



//--------------------------------------
// GPS general-purpose commands

void CommandSetGpsPlayerWaypointOnEntity(int iEntityIndex)
{
	CEntity* pEntity = (CEntity*) CTheScripts::GetEntityToQueryFromGUID<fwEntity>(iEntityIndex, CTheScripts::GUID_ASSERT_FLAGS_ALL);
	Assert(pEntity);
	if(pEntity)
	{
		Vector3 vPos = VEC3V_TO_VECTOR3(pEntity->GetTransform().GetPosition());
		CGps::m_Slots[GPS_SLOT_WAYPOINT].Start(
			vPos,
			pEntity,
			INVALID_BLIP_ID,
			true,
			GPS_SLOT_WAYPOINT,
			CHudColour::GetRGB(HUD_COLOUR_GREEN, 255)
		);
	}
}
void CommandClearGpsPlayerWaypoint()
{
	CGps::m_Slots[GPS_SLOT_WAYPOINT].Clear(GPS_SLOT_WAYPOINT, true);
}

void CommandSetGPSFlashes(bool bFlash)
{
	CGps::SetGpsFlashes(bFlash);
}

void CommandSetGpsFlags(int iFlags, float fBlippedRouteDisplayDistance)
{
#if __ASSERT
	scriptDisplayf("SET_GPS_FLAGS - called by script %s (%d), iFlags: %u, fDistance: %.2f", scrThread::GetActiveThread()->GetScriptName(), CTheScripts::GetCurrentGtaScriptThread()->GetThreadId(), iFlags, fBlippedRouteDisplayDistance);
#endif

	CGps::SetGpsFlagsForScript( CTheScripts::GetCurrentGtaScriptThread()->GetThreadId(), iFlags, fBlippedRouteDisplayDistance );
}

void CommandClearGpsFlags()
{
#if __ASSERT
	scriptDisplayf("CLEAR_GPS_FLAGS - called by script %s", scrThread::GetActiveThread()->GetScriptName());
#endif

	CGps::ClearGpsFlagsOnScriptExit( CTheScripts::GetCurrentGtaScriptThread()->GetThreadId() );
}

//--------------------------------------
// GPS multiple-waypoint route commands

void CommandStartGpsMultiRoute(int colour, bool bTrackPlayer, bool bOnFoot)
{
	CGps::StartMultiGPS(colour, bTrackPlayer, bOnFoot);

#if __ASSERT
	scriptDisplayf("START_GPS_MULTI_ROUTE - called by script %s", scrThread::GetActiveThread()->GetScriptName());
#endif
}
void CommandAddPointToGpsMultiRoute(const scrVector & vPosition)
{
	Vector3 VecCoors(vPosition);
	CGps::AddPointToMultiGPS(VecCoors);

#if __ASSERT
	scriptDisplayf("ADD_POINT_TO_GPS_MULTI_ROUTE - called by script %s", scrThread::GetActiveThread()->GetScriptName());
#endif
}
void CommandRenderGpsMultiRoute(bool bOn)
{
	CGps::RenderMultiGPS(bOn);
}
void CommandClearGpsMultiRoute()
{
	CGps::RenderMultiGPS(false);
}

//-----------------------------
// GPS custom route commands

void CommandStartGpsCustomRoute(int colour, bool bOnFoot, bool bInVehicle)
{
	scriptDebugf3("%s: START_GPS_CUSTOM_ROUTE - Starting gps custom route (Colour = %d, On Foot? = %s, In Vehicle? = %s)", CTheScripts::GetCurrentScriptNameAndProgramCounter(), colour, bOnFoot ? "True" : "False", bInVehicle ? "True" : "False" );
	CGps::StartCustomRoute(colour, bOnFoot, bInVehicle);
}

void CommandAddPointToGpsCustomRoute(const scrVector & vPosition)
{
	Vector3 VecCoors(vPosition);
	CGps::AddPointToCustomRoute(VecCoors);
}

void CommandRenderGpsCustomRoute(bool bOn, int iMiniMapWidth, int iPauseMapWidth)
{
	scriptDebugf3("%s: SET_GPS_CUSTOM_ROUTE_RENDER - %s gps custom route (Minimap Width = %d, Pause Map Width = %d)", CTheScripts::GetCurrentScriptNameAndProgramCounter(), bOn ? "Enabling" : "Disabling", iMiniMapWidth, iPauseMapWidth);
	CGps::RenderCustomRoute(bOn, iMiniMapWidth, iPauseMapWidth);
}

void CommandClearGpsCustomRoute()
{
	scriptDebugf3("%s: CLEAR_GPS_CUSTOM_ROUTE", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	CGps::RenderCustomRoute(false, -1, -1);
}

void CommandStartGpsCustomRouteFromWaypointRecordingRoute(const char * pRouteName, int iStartIndex, int iNumPts, int colour, bool bOnFoot, bool bInVehicle)
{
	CGps::InitCustomRouteFromWaypointRecordingRoute(pRouteName, iStartIndex, iNumPts, colour, bOnFoot, bInVehicle);
}

void CommandStartGpsCustomRouteFromAssistedRoute(const char * pRouteName, int iStartIndex, int iNumPts, int iDirection, int colour, bool bOnFoot, bool bInVehicle)
{
	CGps::InitCustomRouteFromAssistedMovementRoute(pRouteName, iDirection, iStartIndex, iNumPts, colour, bOnFoot, bInVehicle);
}

//-----------------------------
// GPS race-track commands

void CommandStartGpsRaceTrack(int colour)
{
	CGps::StartRaceTrack(colour);
}

void CommandAddPointToGpsRaceTrack(const scrVector & vPosition)
{
	Vector3 VecCoors(vPosition);
	
	CGps::AddPointToRaceTrack(VecCoors);
}

void CommandRenderRaceTrack(bool bOn)
{
	CGps::RenderRaceTrack(bOn);
}

void CommandClearGpsRaceTrack()
{
	CGps::RenderRaceTrack(false);
}

void CommandDisplayFrontendMapBlips(bool bDisplay)
{
	if (bDisplay)
	{
		CScriptHud::bHideFrontendMapBlips = false;
	}
	else
	{
		CScriptHud::bHideFrontendMapBlips = true;
	}
}

void CommandTurnOffRadioHudInLobby()
{
//	NA_RADIO_ENABLED_ONLY(CRadioHud::DeActivateRadioHud(true));
}

void CommandGetFrontendDesignValue(int UNUSED_PARAM(WidgetId), float UNUSED_PARAM(&x), float UNUSED_PARAM(&y))
{
	scriptAssertf(0, "GET_FRONTEND_DESIGN_VALUE - depreciated command!  shouldnt be used!");

/*	eWIDGET_NAME WidgetName = (eWIDGET_NAME)WidgetId;

	if(SCRIPT_VERIFY(WidgetName >= 0 && WidgetName < MAX_FRONTEND_WIDGETS, "GET_FRONTEND_DESIGN_VALUE - invalid frontend widget id"))
	{
		x = CFrontEnd::GetWidgetValue(WidgetName).x;
		y = CFrontEnd::GetWidgetValue(WidgetName).y;
	}*/
}


void CommandSetPlayerIconColour(int iColour)
{
	CScriptHud::m_playerBlipColourOverride = iColour; 
}

//
// name:		ChangePickupBlipScale
// description:	
void CommandChangePickupBlipScale(float blipScale)
{
	CPickupPlacement::ms_customBlipScale = blipScale;
}

//
// name:		ChangePickupBlipPriority
// description:	
void CommandChangePickupBlipPriority(int priority)
{
	if (priority == BLIP_PRIORITY_HIGHEST)  // script should never set a blip with HIGHEST as the code should only do this!!!
		priority = BLIP_PRIORITY_HIGH;

	CPickupPlacement::ms_customBlipPriority = priority;
}

//
// name:		ChangePickupBlipDisplay
// description:	
void CommandChangePickupBlipDisplay(int display)
{
	CPickupPlacement::ms_customBlipDisplay = display;
}

//
// name:		ChangePickupBlipSprite
// description:	
void CommandChangePickupBlipSprite(int sprite)
{
	CPickupPlacement::ms_customBlipSprite = sprite;
}

//
// name:		ChangePickupBlipColour
// description:	
void CommandChangePickupBlipColour(int colour)
{
	CPickupPlacement::ms_customBlipColour = colour;
}

void CommandInitFrontendHelperText()
{
	scriptAssertf(0, "INIT_FRONTEND_HELPER_TEXT - depreciated command!  shouldnt be used!");

	//CFrontEnd::ResetHelperText();
}

void CommandDrawFrontendHelperText(const char *UNUSED_PARAM(pHelpText), const char *UNUSED_PARAM(pHelpButton), bool UNUSED_PARAM(bStartNewLine))
{
	scriptAssertf(0, "DRAW_FRONTEND_HELPER_TEXT - depreciated command!  shouldnt be used!");
/*
	if (CPauseMenu::IsActive())  // we dont want script helper text interfering with frontend code
		return;

	CFrontEnd::AddTextToHelperText(pHelpText, pHelpButton, bStartNewLine);*/
}

void CommandFlashMinimapDisplay()
{
	CMiniMap::FlashMiniMapDisplay(HUD_COLOUR_WHITE);
}

void CommandFlashMinimapDisplayWithColor(int hudColor)
{
	CMiniMap::FlashMiniMapDisplay((eHUD_COLOURS)hudColor);
}

void CommandToggleStealthRadar(const bool bOn)
{
	CMiniMap::SetInStealthMode(bOn);
}

void CommandDrawPedVisualField(s32 pedIndex, const bool bDraw)
{
	CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(pedIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
	if(pPed)
	{
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_DrawRadarVisualField, bDraw );
	}
}


//
// name:		CommandSetMiniMapDimmed
// description:	sets the minimap in "dimmed" mode
void CommandSetMiniMapDimmed(bool UNUSED_PARAM(bDimmed))
{
//	CMiniMap::SetDimmed(bDimmed);
}

//
// name:		CommandSetMinimapHideFoW
// description:	sets the minimap in "dimmed" mode
void CommandSetMinimapHideFoW(bool bHideFow)
{
	CMiniMap::SetHideFow(bHideFow);
}

float CommandGetFoWDiscoveryRatio()
{
	float ratio = CMiniMap_Common::GetFogOfWarDiscoveryRatio();
	sfDisplayf("GET_MINIMAP_FOW_DISCOVERY_RATIO ratio is %f", ratio);
	return ratio;
}

bool CommandIsCoordinateRevealed(const scrVector & vWorldPos)
{
	Vector3 pos(vWorldPos);
	return CMiniMap_Common::IsCoordinateRevealed(pos);
}

void CommandSetMinimapDoNotUpdateFoW(bool bDoNotUpdate)
{
	CMiniMap::SetDoNotUpdateFoW(bDoNotUpdate);
}

void CommandRevealCoordinates(const scrVector & vWorldPos)
{
	Vector2 pos(vWorldPos.x,vWorldPos.y);
	CMiniMap::RevealCoordinate(pos);
}


void CommandSetAllowFoWInMP(bool b)
{
	CMiniMap::SetAllowFoWInMP(b);
}

bool CommandGetAllowFoWInMP()
{
	return CMiniMap::GetAllowFoWInMP();
}

void CommandSetRequestClearFoW(bool b)
{
	CMiniMap::SetRequestFoWClear(b);
}

void CommandSetRequestRevealFoW(bool b)
{
	CMiniMap::SetRequestRevealFoW(b);
}

void CommandEnableFowCustomWorldExtents(bool b)
{
	CMiniMap::EnableFowCustomWorldExtents(b);
}

bool CommandAreFowCustomWorldExtentsEnabled()
{
	return CMiniMap::AreFowCustomWorldExtentsEnabled();
}

void CommandSetFowCustomWorldPosAndSize(float x, float y, float w, float h)
{
	CMiniMap::SetFowCustomWorldPos(x, y);
	CMiniMap::SetFowCustomWorldSize(w, h);
}

void CommandSetMinimapFowMpSaveDetails(bool bEnableSave, s32 minX, s32 minY, s32 maxX, s32 maxY, s32 fillValueForRestOfMap)
{
	if ( (scriptVerifyf( (minX >= 0) && (minX < FOG_OF_WAR_RT_SIZE), "CommandSetMinimapFowMpSaveDetails - minX is %d. Expected it to be >=0 and <%u", minX, FOG_OF_WAR_RT_SIZE))
		&& (scriptVerifyf( (minY >= 0) && (minY < FOG_OF_WAR_RT_SIZE), "CommandSetMinimapFowMpSaveDetails - minY is %d. Expected it to be >=0 and <%u", minY, FOG_OF_WAR_RT_SIZE))
		&& (scriptVerifyf( (maxX >= 0) && (maxX < FOG_OF_WAR_RT_SIZE), "CommandSetMinimapFowMpSaveDetails - maxX is %d. Expected it to be >=0 and <%u", maxX, FOG_OF_WAR_RT_SIZE))
		&& (scriptVerifyf( (maxY >= 0) && (maxY < FOG_OF_WAR_RT_SIZE), "CommandSetMinimapFowMpSaveDetails - maxY is %d. Expected it to be >=0 and <%u", maxY, FOG_OF_WAR_RT_SIZE))
		&& (scriptVerifyf( (fillValueForRestOfMap >= 0) && (fillValueForRestOfMap <= 255), "CommandSetMinimapFowMpSaveDetails - fillValueForRestOfMap is %d. Expected it to be >=0 and <=255", fillValueForRestOfMap)) )
	{
		CScriptHud::GetMultiplayerFogOfWarSavegameDetails().Set(bEnableSave, 
			static_cast<u8>(minX), static_cast<u8>(minY), 
			static_cast<u8>(maxX), static_cast<u8>(maxY), 
			static_cast<u8>(fillValueForRestOfMap));
	}
}

bool CommandGetMinimapFowMpSaveDetails(s32 &minX, s32 &minY, s32 &maxX, s32 &maxY, s32 &fillValueForRestOfMap)
{
	u8 u8minx = 0, u8miny = 0, u8maxx = 0, u8maxy = 0, u8fillvalue = 0;
	bool bEnabled = CScriptHud::GetMultiplayerFogOfWarSavegameDetails().Get(u8minx, u8miny, u8maxx, u8maxy, u8fillvalue);

	minX = u8minx;
	maxX = u8maxx;
	minY = u8miny;
	maxY = u8maxy;
	fillValueForRestOfMap = u8fillvalue;

	return bEnabled;
}

//
// name:		CommandSetMiniMapBackgroundHidden
// description:	sets the minimap background on/off
void CommandSetMiniMapBackgroundHidden(bool bOnOff)
{
	CMiniMap::SetBackgroundMapAsHidden(bOnOff);
}


//
// name:		CommandSetMiniMapBlockWaypoint
// description:	Blocks adding of the waypoint
void CommandSetMiniMapBlockWaypoint(bool bBlock)
{
#if __BANK
#if !__NO_OUTPUT
	static bool bSpamSupressor = !bBlock;

	if (bSpamSupressor != bBlock)
	{
		if (bBlock)
			uiDebugf1("SET_MINIMAP_BLOCK_WAYPOINT(TRUE) has been called by script: %s", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		else
			uiDebugf1("SET_MINIMAP_BLOCK_WAYPOINT(FALSE) has been called by script: %s", CTheScripts::GetCurrentScriptNameAndProgramCounter());

		bSpamSupressor = bBlock;
	}
#endif  // #if !__NO_OUTPUT
#endif  // __BANK

	CMiniMap::SetBlockWaypoint(bBlock);
}


//
// name:		CommandSetMiniMapInPrologue
// description:	sets the minimap in "prologue map" mode
void CommandSetMiniMapInPrologue(bool bInPrologue)
{
	CMiniMap::SetInPrologue(bInPrologue);
}


//
// name:		CommandSetMiniMapGolfCourse
// description:	sets the minimap in golf course mode with a hole
void CommandSetMiniMapGolfCourse(s32 iGolfCourseHole)
{
	CMiniMap::SetCurrentGolfMap((eGOLF_COURSE_HOLES)iGolfCourseHole);
}


//
// name:		CommandSetMiniMapGolfCourseOff
// description:	turns the golf course minimap off
void CommandSetMiniMapGolfCourseOff()
{
	CMiniMap::SetCurrentGolfMap(GOLF_COURSE_OFF);
}



//
// name:		CommandLockMiniMapAngle
// description:	locks the minimap rotation to a desired angle
void CommandLockMiniMapAngle(s32 iAngle)
{
	if (iAngle >= 0 && iAngle <= 360)
	{
		CMiniMap::LockMiniMapAngle(iAngle);
	}
	else
	{
		scriptAssertf(0, "LOCK_MINIMAP_ANGLE - Minimap locked angle must be between 0 and 360 degrees");
		CMiniMap::LockMiniMapAngle(-1);
	}
}



//
// name:		CommandUnlockMiniMapAngle
// description:	unlocks the minimap so it rotates with the camera angle again as normal
void CommandUnlockMiniMapAngle()
{
	CMiniMap::LockMiniMapAngle(-1);
}



//
// name:		CommandLockMiniMapPosition
// description:	locks the minimap position to a world coord
void CommandLockMiniMapPosition(float fPosX, float fPosY)
{
	CMiniMap::LockMiniMapPosition(Vector2(fPosX, fPosY));
}



//
// name:		CommandUnlockMiniMapPosition
// description:	unlocks the minimap so it moves with the player position as normal
void CommandUnlockMiniMapPosition()
{
	CMiniMap::LockMiniMapPosition(Vector2(-9999.0f, -9999.0f));
}



//
// name:		CommandSetFakeMinimapMaxAltimeterHeight
// description:	sets a fake max height for the minimap altimeter
void CommandSetFakeMinimapMaxAltimeterHeight(float fMaxHeight, bool bColourAltimeterArea, s32 hudColor)
{
	CScriptHud::fFakeMinimapAltimeterHeight = fMaxHeight;
	CScriptHud::ms_bColourMinimapAltimeter = bColourAltimeterArea;
	CScriptHud::ms_MinimapAltimeterColor = (eHUD_COLOURS)hudColor;
}

void CommandSetHealthHudDisplayValues(int iHealth, int iArmour, bool bShowDmg)
{
	CNewHud::SetHealthOverride(iHealth, iArmour, bShowDmg);
}

//
// name:		CommandSetMaxHealthHudDisplay
// description:	sets max health display %
void CommandSetMaxHealthHudDisplay(s32 iMaxDisplay)
{
	CScriptHud::iHudMaxHealthDisplay = iMaxDisplay;
}



//
// name:		CommandSetMaxArmourHudDisplay
// description:	sets max armour display %
void CommandSetMaxArmourHudDisplay(s32 iMaxDisplay)
{
	CScriptHud::iHudMaxArmourDisplay = iMaxDisplay;
}



//
// name:		CommandSetBigMapActive
// description:	sets the big map active/inactive and whether it should show full map overview or not (default to true)
void CommandSetBigMapActive(bool bActive, bool bFullMap)
{
	if (sMiniMapMenuComponent.IsActive() ||
		SocialClubMenu::IsActive() ||  // fix for 1649425
		cStoreScreenMgr::IsStoreMenuOpen())
	{
		return;
	}

	if (bActive)
	{
		if (!CMiniMap::IsInBigMap())
		{
			if (!CMiniMap::IsInPauseMap())
			{
#if !__FINAL
				if(CMiniMap_Common::OutputDebugTransitions())
				{
					uiDisplayf("MINIMAP_TRANSITION: SET_BIGMAP_ACTIVE(TRUE) is being called by script: %s", CTheScripts::GetCurrentScriptNameAndProgramCounter());
				}
#endif
				CMiniMap::SetMinimapModeState(MINIMAP_MODE_STATE_SETUP_FOR_BIGMAP); // we now call SetInBigmap(true) inside the minimap transition state setter function
			}
		}

		CMiniMap::SetBigMapFullZoom(bFullMap);
	}
	else
	{
		if (CMiniMap::WasInBigMap())  // want to check it regardless of whether in the pausemenu or not
		{
			if (!CPauseMenu::IsActive())
			{
#if !__FINAL
				if(CMiniMap_Common::OutputDebugTransitions())
				{
					uiDisplayf("MINIMAP_TRANSITION: SET_BIGMAP_ACTIVE(FALSE) is being called by script: %s", CTheScripts::GetCurrentScriptNameAndProgramCounter());
				}
#endif
				CMiniMap::SetMinimapModeState(MINIMAP_MODE_STATE_SETUP_FOR_MINIMAP);  // we now call SetInBigmap(false) inside the minimap transition state setter function
			}
		}
	}
}



//
// name:		CommandSetBigMapFullScreen
// description:	sets the big map as full screen or not
void CommandSetBigMapFullScreen(bool bFullScreen)
{
#if !__NO_OUTPUT
	if (!NetworkInterface::IsGameInProgress())  // only output in SP
	{
		if (bFullScreen)
			uiDebugf1("SET_BIGMAP_FULLSCREEN(TRUE) is being called by script: %s", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		else
			uiDebugf1("SET_BIGMAP_FULLSCREEN(FALSE) is being called by script: %s", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
#endif

	CMiniMap::SetBigMapAsFullScreen(bFullScreen);
}



//
// name:		CommandSetMiniMapInSpectatorMode
// description:	sets the minimap into spectator mode, locked to any ped
void CommandSetMiniMapInSpectatorMode(bool bOnOff, s32 pedIndex)
{
	if (bOnOff)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(pedIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
		
		if(pPed)
		{
			CMiniMap::SetMiniMapPedPoolIndex(CPed::GetPool()->GetIndex(pPed));

			return;
		}

		scriptAssertf(0, "%s: SET_MINIMAP_IN_SPECTATOR_MODE - ped is not valid", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
	else
	{
		CMiniMap::SetMiniMapPedPoolIndex(0);
	}
}



//
// name:		CommandSetMissionName
// description:	sets the current mission name to display on the HUD
void CommandSetMissionName(const bool bActive, const char *pTextLabel)
{
	CMessages::SetMissionTitleActive(bActive);

	if (bActive && pTextLabel)
	{
		CLoadingText::SetText(TheText.Get(pTextLabel));
		CPauseMenu::SetCurrentMissionActive(bActive);
		CPauseMenu::SetCurrentMissionLabel(pTextLabel, false);
		//		CHud::Component[CHud::HudData[HUD_MISSION_NAME].iItemId]->SetValue(TheText.Get(pTextLabel));
	}
	else
	{
		CLoadingText::Clear();
		CPauseMenu::SetCurrentMissionActive(false);
		CPauseMenu::SetCurrentMissionLabel("", true);
		//		CHud::Component[CHud::HudData[HUD_MISSION_NAME].iItemId]->SetValue(NULL);
	}
}

void CommandSetMissionNameForUGCMission(const bool bActive, const char *pMissionName)
{
	CMessages::SetMissionTitleActive(bActive);

	if (bActive && pMissionName)
	{
		CLoadingText::SetText(pMissionName);
		CPauseMenu::SetCurrentMissionActive(bActive);
		CPauseMenu::SetCurrentMissionLabel(pMissionName, true);
		//		CHud::Component[CHud::HudData[HUD_MISSION_NAME].iItemId]->SetValue(TheText.Get(pTextLabel));
	}
	else
	{
		CLoadingText::Clear();
		CPauseMenu::SetCurrentMissionActive(false);
		CPauseMenu::SetCurrentMissionLabel("", true);
		//		CHud::Component[CHud::HudData[HUD_MISSION_NAME].iItemId]->SetValue(NULL);
	}
}

void CommandSetDescriptionForUGCMissionEightStrings(const bool bActive, const char *pString1, const char *pString2, const char *pString3, const char *pString4, const char *pString5, const char *pString6, const char *pString7, const char *pString8)
{
	CPauseMenu::SetCurrentMissionDescription(bActive, pString1, pString2, pString3, pString4, pString5, pString6, pString7, pString8);
}

void CommandSetDescriptionForUGCMission(const bool bActive, const char *pString1, const char *pString2, const char *pString3, const char *pString4)
{
	CPauseMenu::SetCurrentMissionDescription(bActive, pString1, pString2, pString3, pString4, NULL, NULL, NULL, NULL);
}


//
// name:		CommandIsHudComponentActive
// description:	returns the hud component is currently active (on screen)
bool CommandIsHudComponentActive(s32 iHudComponent)
{
	if( iHudComponent == NEW_HUD_GAME_STREAM )
	{
		return( CGameStreamMgr::GetGameStream()->IsShowEnabled() );
	}
	return (CNewHud::IsHudComponentActive(iHudComponent));
}



//
// name:		CommandIsScriptedHudComponentActive
// description:	returns if a scripted hud component is currently active (on screen)
bool CommandIsScriptedHudComponentActive(s32 iHudComponent)
{
	iHudComponent += SCRIPT_HUD_COMPONENTS_START;  // offset from script enum to code hud enum

	return (CNewHud::IsHudComponentActive(iHudComponent));
}


//
// name:		CommandHideScriptedHudComponentThisFrame
// description:	hides a hud component
void CommandHideScriptedHudComponentThisFrame(s32 iHudComponent)
{
	iHudComponent += SCRIPT_HUD_COMPONENTS_START;  // offset from script enum to code hud enum

#if __BANK  // allows us to see exactly what scripts hide each components to diagnose issues where things are not showing on the screen
	if (CNewHud::iDebugComponentVisibilityDisplay == iHudComponent)
	{
		sfDisplayf("HIDE_HUD_SCRIPTED_COMPONENT_THIS_FRAME called on %d component by '%s' SCRIPT", iHudComponent, CTheScripts::GetCurrentScriptName());
	}
#endif

	CNewHud::SetHudComponentToBeHidden(iHudComponent);
}

void CommandShowScriptedHudComponentThisFrame(s32 iHudComponent)
{

	iHudComponent += SCRIPT_HUD_COMPONENTS_START;  // offset from script enum to code hud enum

#if __BANK  // allows us to see exactly what scripts hide each components to diagnose issues where things are not showing on the screen
	if (CNewHud::iDebugComponentVisibilityDisplay == iHudComponent)
	{
		sfDisplayf("SHOW_SCRIPTED_HUD_COMPONENT_THIS_FRAME called on %d component by '%s' SCRIPT", iHudComponent, CTheScripts::GetCurrentScriptName());
	}
#endif

	CNewHud::SetHudComponentToBeShown(iHudComponent);
}

//
// name:		CommandIsScriptedHudComponentHiddenThisFrame
// description:	returns whether this scripted hud component is hidden this frame
bool CommandIsScriptedHudComponentHiddenThisFrame(s32 iHudComponent)
{
	iHudComponent += SCRIPT_HUD_COMPONENTS_START;  // offset from script enum to code hud enum

	return (CNewHud::DoesScriptWantToForceHideThisFrame(iHudComponent, false));
}

//
// name:		CommandHideHudComponentThisFrame
// description:	hides a hud component
void CommandHideHudComponentThisFrame(s32 iHudComponent)
{
	if( iHudComponent == NEW_HUD_GAME_STREAM )
	{
		//CGameStreamMgr::GetGameStream()->HideThisUpdate();
	}
	else
	{
#if __BANK  // allows us to see exactly what scripts hide each components to diagnose issues where things are not showing on the screen
		if (CNewHud::iDebugComponentVisibilityDisplay != 0)
		{
			sfDisplayf("HIDE_HUD_COMPONENT_THIS_FRAME called on %d component by '%s' SCRIPT", iHudComponent, CTheScripts::GetCurrentScriptName());
		}
#endif
		CNewHud::SetHudComponentToBeHidden((eNewHudComponents)iHudComponent);
	}
}


//
// name:		CommandIsScriptedHudComponentHiddenThisFrame
// description:	returns whether this scripted hud component is hidden this frame
bool CommandIsHudComponentHiddenThisFrame(s32 iHudComponent)
{
	return (CNewHud::DoesScriptWantToForceHideThisFrame(iHudComponent, false));
}




//
// name:		CommandShowHudComponentThisFrame
// description:	show a hud component
void CommandShowHudComponentThisFrame(s32 iHudComponent)
{
	CNewHud::SetHudComponentToBeShown((eNewHudComponents)iHudComponent);
}

void CommandHideStreetAndCarNamesThisFrame()
{
	CNewHud::HideStreetNamesThisFrame();
}


void CommandResetReticuleValues()
{
	CNewHud::GetReticule().Reset();
}

//
// name:		CommandGetLowestTopRightYPos
// description:	returns the lowest top right hud component position sent to code via actionscript
float CommandGetLowestTopRightYPos()
{
	return (CNewHud::GetLowestTopRightYPos());
}



//
// name:		CommandSetHudComponentPosition
// description:	sets the current position of the hud component
void CommandSetHudComponentPosition(s32 iHudComponent, float fPosX, float fPosY)
{
	uiDisplayf("[HUD_OVERRIDE] SET_HUD_COMPONENT_POSITION called on %s %d component by '%s' SCRIPT.  Active: %d", CNewHud::GetHudComponentFileName(iHudComponent), iHudComponent, CTheScripts::GetCurrentScriptName(), (s32)CNewHud::IsHudComponentActive(iHudComponent));

	CNewHud::SetHudComponentPosition((eNewHudComponents)iHudComponent, Vector2(fPosX, fPosY));
}



//
// name:		CommandResetHudComponentValues
// description:	sets the current position of the hud component
void CommandResetHudComponentValues(s32 iHudComponent)
{
	uiDisplayf("[HUD_OVERRIDE] RESET_HUD_COMPONENT_VALUES called on %s %d component by '%s' SCRIPT.  Active: %d", CNewHud::GetHudComponentFileName(iHudComponent), iHudComponent, CTheScripts::GetCurrentScriptName(), (s32)CNewHud::IsHudComponentActive(iHudComponent));

	CNewHud::ResetHudComponentPositionFromOriginalXmlValues((eNewHudComponents)iHudComponent);
}



//
// name:		CommandGetHudComponentPosition
// description:	returns the current position of the hud component
Vector3 CommandGetHudComponentPosition(s32 iHudComponent)
{
	return (Vector3(CNewHud::GetHudComponentPosition((eNewHudComponents)iHudComponent), Vector2::kXY));
}



//
// name:		CommandGetHudComponentSize
// description:	returns the current position of the hud component
Vector3 CommandGetHudComponentSize(s32 iHudComponent)
{
	return (Vector3(CNewHud::GetHudComponentSize((eNewHudComponents)iHudComponent), Vector2::kXY));
}



//
// name:		CommandSetHudComponentColour
// description:	sets the colour of a hud component using a hud colour and an optional alpha
void CommandSetHudComponentColour(s32 iHudComponent, s32 iHudColourIndex, s32 iAlpha)
{
	CNewHud::SetHudComponentColour((eNewHudComponents)iHudComponent, (eHUD_COLOURS)iHudColourIndex, iAlpha);
}


//
// name:		CommandSetHudComponentAlpha
// description:	sets the alpha of a hud component
void CommandSetHudComponentAlpha(s32 iHudComponent, s32 iAlpha)
{
	CNewHud::SetHudComponentAlpha((eNewHudComponents)iHudComponent, iAlpha);
}

//
// name:		CommandSetHudComponentAlpha
// description:	sets the alpha of a hud component
void CommandSetReticuleOverrideThisFrame(s32 overrideHash)
{
	CNewHud::GetReticule().SetOverrideThisFrame(overrideHash);
}


//
// name:		CommandClearReminderMessage
// description:	clears any reminder message onscreen
// DEPRECIATED
void CommandClearReminderMessage()
{
	// DEPRECIATED
}



//
// name:		CommandDisplayReminderMessages
// description: DEPRECIATED
void CommandDisplayReminderMessages(bool UNUSED_PARAM(bReminderMessages))
{
	// DEPRECIATED
}

void CommmandOpenUGCMenu()
{
	SReportMenu::GetInstance().Open(NetworkInterface::GetLocalGamerHandle(), CReportMenu::eReportMenu_UGCMenu);
}

void CommandForceCloseReportMenu()
{
	SReportMenu::GetInstance().Close();
}

bool CommandIsReportMenuOpen()
{
	bool bResult = false;

	bResult = SReportMenu::GetInstance().IsActive();

	return bResult;
}

int CommandGetCurrentReportMenuPage()
{
	int iPage = 0;

	iPage = SReportMenu::GetInstance().GetCurrentMenu();

	return iPage;
}
//
// name:		CommandGetHudScreenPositionFromWorldPosition
// description:	converts a world position to a position on screen for the HUD and returns direction off screen
eTEXT_ARROW_ORIENTATION CommandGetHudScreenPositionFromWorldPosition(const scrVector & vWorldPos, float &fScreenPosX, float &fScreenPosY)
{
	eTEXT_ARROW_ORIENTATION iArrowDir;

	Vector2 vNewScreenPos = CHudTools::GetHudPosFromWorldPos(Vector3(vWorldPos), &iArrowDir);

	fScreenPosX = vNewScreenPos.x;
	fScreenPosY = vNewScreenPos.y;

	return iArrowDir;
}

//
// name:		CommandGetMinimapPosAndSize
// description:	Returns the current minimap position and size.
// GET_HUD_MINIMAP_POSITION_AND_SIZE
void CommandGetMinimapPosAndSize( float &fMinimapPosX, float &fMinimapPosY, float &fMinimapWidth, float &fMinimapHeight )
{
	CGameStream* GameStream = CGameStreamMgr::GetGameStream();
	if( GameStream != NULL )
	{
		float x,y,w,h;
		GameStream->GetMinimapPosAndSize( &x, &y, &w, &h );
		fMinimapPosX = x;
		fMinimapPosY = y;
		fMinimapWidth = w;
		fMinimapHeight = h;
	}
	else
	{
		fMinimapPosX = 0.0f;
		fMinimapPosY = 0.0f;
		fMinimapWidth = 0.0f;
		fMinimapHeight = 0.0f;
	}
}




//
// name:		CommandCreateMPGamerTag
// description: 
void CommandCreateMPGamerTagWithCrewColor(s32 iPlayerId, const char *pPlayerName, bool bCrewTypeIsPrivate, bool bCrewTagContainsRockstar, const char *pCrewTag, s32 iCrewRank, s32 crewR, s32 crewG, s32 crewB)
{
	if(scriptVerifyf(SMultiplayerGamerTagHud::IsInstantiated(), "CREATE_MP_GAMER_TAG_WITH_CREW_COLOR - The Gamer Tag system hasn't been initialized yet."))
	{
		char crewTagBuffer[NetworkClan::FORMATTED_CLAN_TAG_LEN] = {0};
		NetworkClan::GetUIFormattedClanTag(!bCrewTypeIsPrivate, bCrewTagContainsRockstar, pCrewTag, iCrewRank, CRGBA((u8)crewR, (u8)crewG, (u8)crewB), crewTagBuffer, NetworkClan::FORMATTED_CLAN_TAG_LEN);

		SMultiplayerGamerTagHud::GetInstance().UT_CreatePlayerTag(iPlayerId, pPlayerName, crewTagBuffer);
	}
}

void CommandCreateMPGamerTag(s32 iPlayerId, const char *pPlayerName, bool bCrewTypeIsPrivate, bool bCrewTagContainsRockstar, const char *pCrewTag, s32 iCrewRank)
{
	CRGBA color = CHudColour::GetRGBA(HUD_COLOUR_WHITE);
	CommandCreateMPGamerTagWithCrewColor(iPlayerId, pPlayerName, bCrewTypeIsPrivate, bCrewTagContainsRockstar, pCrewTag, iCrewRank, color.GetRed(), color.GetGreen(), color.GetBlue());
}

bool CommandIsMPGamerTagMovieActive()
{
	return SMultiplayerGamerTagHud::IsInstantiated() && SMultiplayerGamerTagHud::GetInstance().IsMovieActive();
}

//
// name:		CommandCreateFakeMPGamerTag
// description: 
s32 CommandCreateFakeMPGamerTagWithCrewColor(s32 pedIndex, const char *pPlayerName, bool bCrewTypeIsPrivate, bool bCrewTagContainsRockstar, const char *pCrewTag, s32 iCrewRank, s32 crewR, s32 crewG, s32 crewB)
{
	s32 iId = -1;

	CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(pedIndex, CTheScripts::GUID_ASSERT_FLAG_ENTITY_EXISTS);

	scriptAssertf(pPed, "CREATE_FAKE_MP_GAMER_TAG_WITH_CREW_COLOR - unable to set fake gamer tag as ped doesnt exist");

	if (pPed)
	{
		if(scriptVerifyf(SMultiplayerGamerTagHud::IsInstantiated(), "CREATE_FAKE_MP_GAMER_TAG_WITH_CREW_COLOR - The Gamer Tag system hasn't been initialized yet."))
		{
			char crewTagBuffer[NetworkClan::FORMATTED_CLAN_TAG_LEN] = {0};
			NetworkClan::GetUIFormattedClanTag(!bCrewTypeIsPrivate, bCrewTagContainsRockstar, pCrewTag, iCrewRank, CRGBA((u8)crewR, (u8)crewG, (u8)crewB), crewTagBuffer, NetworkClan::FORMATTED_CLAN_TAG_LEN);

			iId = SMultiplayerGamerTagHud::GetInstance().UT_CreateFakePlayerTag(pPed, pPlayerName, crewTagBuffer);
		}

		scriptAssertf(iId != -1, "CREATE_FAKE_MP_GAMER_TAG_WITH_CREW_COLOR - unable to set fake gamer tag");
	}

	return iId;
}

s32 CommandCreateFakeMPGamerTag(s32 pedIndex, const char *pPlayerName, bool bCrewTypeIsPrivate, bool bCrewTagContainsRockstar, const char *pCrewTag, s32 iCrewRank)
{
	CRGBA color = CHudColour::GetRGBA(HUD_COLOUR_WHITE);
	return CommandCreateFakeMPGamerTagWithCrewColor(pedIndex, pPlayerName, bCrewTypeIsPrivate, bCrewTagContainsRockstar, pCrewTag, iCrewRank, color.GetRed(), color.GetGreen(), color.GetBlue());
}

s32 CommandCreateMPGamerTagForVehicle(s32 vehicleId, const char *pPlayerName)
{
	if(scriptVerifyf(SMultiplayerGamerTagHud::IsInstantiated(), "CREATE_MP_GAMER_TAG_FOR_VEHICLE - The Gamer Tag system hasn't been initialized yet."))
	{
		CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(vehicleId, CTheScripts::GUID_ASSERT_FLAG_ENTITY_EXISTS);

		char crewTagBuffer[NetworkClan::FORMATTED_CLAN_TAG_LEN] = {0};
		return SMultiplayerGamerTagHud::GetInstance().UT_CreateFakePlayerTag(pVehicle, pPlayerName, crewTagBuffer);
	}

	return -1;
}

//
// name:		CommandRemoveMPGamerTag
// description: 
void CommandRemoveMPGamerTag(s32 iPlayerId)
{
	if(scriptVerifyf(SMultiplayerGamerTagHud::IsInstantiated(), "REMOVE_MP_GAMER_TAG - The Gamer Tag system hasn't been initialized yet."))
	{
		SMultiplayerGamerTagHud::GetInstance().UT_RemovePlayerTag(iPlayerId);
	}
}

//
// name:		CommandIsMPGamerTagActive
// description: 
bool CommandIsMPGamerTagActive(s32 iPlayerId)
{
	return (CMultiplayerGamerTagHud::UT_IsGamerTagActive(iPlayerId));
}

// name:		CommandIsMPGamerTagAvailable
// description: 
bool CommandIsMPGamerTagFree(s32 iPlayerId)
{
	return (CMultiplayerGamerTagHud::UT_IsGamerTagFree(iPlayerId));
}

//
// name:		CommandSetMPGamerTagVisibility
// description: 
void CommandSetMPGamerTagVisibility(s32 iPlayerId, s32 iTag, bool bVisible, bool bShowEvenInBigVehicles)
{
	if(SMultiplayerGamerTagHud::IsInstantiated())
	{
		SMultiplayerGamerTagHud::GetInstance().UT_SetGamerTagVisibility(iPlayerId, (eMP_TAG)iTag, bVisible, bShowEvenInBigVehicles);
	}
}

//
// name:		CommandSetAllMPGamerTagsVisibility
// description: 
void CommandSetAllMPGamerTagsVisibility(s32 iPlayerId, bool bVisible)
{
	if(SMultiplayerGamerTagHud::IsInstantiated())
	{
		SMultiplayerGamerTagHud::GetInstance().UT_SetAllGamerTagsVisibility(iPlayerId, bVisible);
	}
}

// name:		CommandSetGamerTagCanRenderWhenPaused
// description: 
void CommandSetGamerTagCanRenderWhenPaused(s32 iPlayerId, bool bCanRenderWhenPaused)
{
	if(SMultiplayerGamerTagHud::IsInstantiated())
	{
		SMultiplayerGamerTagHud::GetInstance().UT_SetGamerTagsCanRenderWhenPaused(iPlayerId, bCanRenderWhenPaused);
	}
}

// name:		CommandSetGamerTagsShouldUseVehicleHealth
// description: 
void CommandSetGamerTagsShouldUseVehicleHealth(s32 iPlayerId, bool bUseVehicleHealth)
{
	if(SMultiplayerGamerTagHud::IsInstantiated())
	{
		SMultiplayerGamerTagHud::GetInstance().UT_SetGamerTagsShouldUseVehicleHealth(iPlayerId, bUseVehicleHealth);
	}
}

void CommandSetGamerTagsShouldUsePointHealth(s32 iPlayerId, bool bUsePointHealth)
{
	if(SMultiplayerGamerTagHud::IsInstantiated())
	{
		SMultiplayerGamerTagHud::GetInstance().UT_SetGamerTagsShouldUsePointHealth(iPlayerId, bUsePointHealth);
	}
}

void CommandSetGamerTagsPointHealth(s32 iPlayerId, int iPoints, int iMaxPoints)
{
	if(SMultiplayerGamerTagHud::IsInstantiated())
	{
		SMultiplayerGamerTagHud::GetInstance().UT_SetGamerTagsPointHealth(iPlayerId, iPoints, iMaxPoints);
	}
}

//
// name:		CommandSetMPGamerTagColour
// description: 
void CommandSetMPGamerTagColour(s32 iPlayerId, s32 iTag, s32 hudColour)
{
	if(SMultiplayerGamerTagHud::IsInstantiated())
	{
		SMultiplayerGamerTagHud::GetInstance().UT_SetGamerTagValueHudColour(iPlayerId, (eMP_TAG)iTag, (eHUD_COLOURS)hudColour);
	}
}

void CommandSetMPGamerTagHealthBarColour(s32 iPlayerId, s32 hudColour)
{
	if(SMultiplayerGamerTagHud::IsInstantiated())
	{
		SMultiplayerGamerTagHud::GetInstance().UT_SetGamerTagHealthBarColour(iPlayerId, (eHUD_COLOURS)hudColour);
	}
}

//
// name:		CommandSetMPGamerTagAlpha
// description: 
void CommandSetMPGamerTagAlpha(s32 iPlayerId, s32 iTag, s32 iAlpha)
{
	if(SMultiplayerGamerTagHud::IsInstantiated())
	{
		SMultiplayerGamerTagHud::GetInstance().UT_SetGamerTagValueHudAlpha(iPlayerId, (eMP_TAG)iTag, iAlpha);
	}
}


//
// name:		CommandSetMPGamerTagWantedLevel
// description: 
void CommandSetMPGamerTagWantedLevel(s32 iPlayerId, s32 iWantedLevel)
{
	if(SMultiplayerGamerTagHud::IsInstantiated())
	{
		SMultiplayerGamerTagHud::GetInstance().UT_SetGamerTagValueInt(iPlayerId, MP_TAG_WANTED_LEVEL, iWantedLevel);
	}
}

//
// name:		CommandSetMPGamerTagNumPackages
// description: 
void CommandSetMPGamerTagNumPackages(s32 iPlayerId, s32 iNumPackages)
{
	if(SMultiplayerGamerTagHud::IsInstantiated())
	{
		SMultiplayerGamerTagHud::GetInstance().UT_SetGamerTagValueInt(iPlayerId, MP_TAG_PACKAGE_LARGE, iNumPackages);
	}
}

//
// name:		CommandSetMPGamerTagRank
// description: 
void CommandSetMPGamerTagRank(s32 iPlayerId, s32 iRank)
{
	if(SMultiplayerGamerTagHud::IsInstantiated())
	{
		SMultiplayerGamerTagHud::GetInstance().UT_SetGamerTagValueInt(iPlayerId, MP_TAG_RANK, iRank);
	}
}

void CommandSetMPGamerCrewDetails(s32 iPlayerId, bool bCrewTypeIsPrivate, bool bCrewTagContainsRockstar, const char *pCrewTag, s32 iCrewRank, s32 crewR, s32 crewG, s32 crewB)
{
	if(SMultiplayerGamerTagHud::IsInstantiated())
	{
		char crewTagBuffer[NetworkClan::FORMATTED_CLAN_TAG_LEN] = {0};
		NetworkClan::GetUIFormattedClanTag(!bCrewTypeIsPrivate, bCrewTagContainsRockstar, pCrewTag, iCrewRank, CRGBA((u8)crewR, (u8)crewG, (u8)crewB), crewTagBuffer, NetworkClan::FORMATTED_CLAN_TAG_LEN);

		SMultiplayerGamerTagHud::GetInstance().UT_SetGamerTagCrewDetails(iPlayerId, crewTagBuffer);
	}
}

void CommandSetMPGamerName(s32 iPlayerId, const char *pGamerName)
{
	if(SMultiplayerGamerTagHud::IsInstantiated())
	{
		SMultiplayerGamerTagHud::GetInstance().UT_SetGamerName(iPlayerId, pGamerName);
	}
}

bool CommandIsUpdatingMPGamerNameAndCrewTag(s32 iPlayerId)
{
	return SMultiplayerGamerTagHud::IsInstantiated() && SMultiplayerGamerTagHud::GetInstance().UT_IsReinitingGamerNameAndCrewTag(iPlayerId);
}

void CommandSetMPGamerBigText(s32 iPlayerId, const char *pBigText)
{
	if(SMultiplayerGamerTagHud::IsInstantiated())
	{
		SMultiplayerGamerTagHud::GetInstance().UT_SetBigText(iPlayerId, pBigText);
	}
}

void CommandSetGamerTagVerticalOffset(float fVerticalOffset)
{
	if(SMultiplayerGamerTagHud::IsInstantiated())
	{
		SMultiplayerGamerTagHud::GetInstance().UT_SetGamerTagVerticalOffset(fVerticalOffset);
	}
}


//
// name:		CommandGetCurrentWebpageId
// description: 
s32 CommandGetCurrentWebpageId()
{
	// we're not resetting the value anymore ...scripting has no need
	// and now being used over multiple threads/scripts ...gets should only get
	return CScriptHud::iCurrentWebpageIdFromActionScript;
}




//
// name:		CommandGetGlobalActionscriptFlag
// description: 
s32 CommandGetGlobalActionscriptFlag(s32 iIndex)
{
	if (scriptVerifyf(iIndex < MAX_ACTIONSCRIPT_FLAGS, "GET_GLOBAL_ACTIONSCRIPT_FLAG tried to use an id outwith the range (range = 0 to %d)", MAX_ACTIONSCRIPT_FLAGS-1))
	{
		s32 iCurrentFlagValue = CScriptHud::iActionScriptGlobalFlags[iIndex];

//		uiDisplayf("GET_GLOBAL_ACTIONSCRIPT_FLAG[%d] returned %d to script - %s", iIndex, iCurrentFlagValue, CTheScripts::GetCurrentScriptNameAndProgramCounter());

		return (iCurrentFlagValue);
	}

	return 0;
}



//
// name:		CommandResetGlobalActionscriptFlag
// description: 
void CommandResetGlobalActionscriptFlag(s32 iIndex)
{
	if (scriptVerifyf(iIndex < MAX_ACTIONSCRIPT_FLAGS, "RESET_GLOBAL_ACTIONSCRIPT_FLAG tried to use an id outwith the range (range = 0 to %d)", MAX_ACTIONSCRIPT_FLAGS-1))
	{
		CScriptHud::iActionScriptGlobalFlags[iIndex] = 0;
	}
}



//
// name:		CommandGetCurrentWebsiteId
// description: 
s32 CommandGetCurrentWebsiteId()
{
	// we're not resetting the value anymore ...scripting has no need
	// and now being used over multiple threads/scripts ...gets should only get
	return CScriptHud::iCurrentWebsiteIdFromActionScript;
}

// name:		CommandSetWarningMessageWithHeader
// description: 
void CommandSetWarningMessageWithHeader(const char *pTextLabelHeading,
										const char *pTextLabelBody,
										int iFlagBitField,
										const char *pTextLabelSubtext,
										const bool bInsertNumber,
										const s32 NumberToInsert, 
										const char *pFirstSubString, 
										const char *pSecondSubString,
										const bool bBackground,
										int errorNumber)
{
//	Displayf("SET_WARNING_MESSAGE_WITH_HEADER called: frame: %d", fwTimer::GetFrameCount());

	char *pGxtFirstSubString = NULL;
	char *pGxtSecondSubString = NULL;

	if (pFirstSubString)
	{
		pGxtFirstSubString = TheText.Get(pFirstSubString);
	}

	if (pSecondSubString)
	{
		pGxtSecondSubString = TheText.Get(pSecondSubString);
	}

	// close the calibration screen if open (warning screen compatibility)
	CPauseMenu::CloseDisplayCalibrationScreen();

	CWarningScreen::SetMessageWithHeaderAndSubtext(	WARNING_MESSAGE_STANDARD, 
													pTextLabelHeading, 
													pTextLabelBody,
													pTextLabelSubtext,
													static_cast<eWarningButtonFlags>(iFlagBitField),
													bInsertNumber,
													NumberToInsert,
													pGxtFirstSubString,
													pGxtSecondSubString,
													bBackground,
													false,
													errorNumber);
}

void CommandSetWarningMessageWithHeaderExtended(
	const char *pTextLabelHeading,
	const char *pTextLabelBody,
	int iFlagBitFieldLower,
	int iFlagBitFieldUpper,
	const char *pTextLabelSubtext,
	const bool bInsertNumber,
	const s32 NumberToInsert, 
	const char *pFirstSubString, 
	const char *pSecondSubString,
	const bool bBackground,
	int errorNumber)
{
	//	Displayf("SET_WARNING_MESSAGE_WITH_HEADER called: frame: %d", fwTimer::GetFrameCount());

	char *pGxtFirstSubString = NULL;
	char *pGxtSecondSubString = NULL;
	u64 uFlagBits = (u64)iFlagBitFieldUpper << 32 | iFlagBitFieldLower;

	if (pFirstSubString)
	{
		pGxtFirstSubString = TheText.Get(pFirstSubString);
	}

	if (pSecondSubString)
	{
		pGxtSecondSubString = TheText.Get(pSecondSubString);
	}

	// close the calibration screen if open (warning screen compatibility)
	CPauseMenu::CloseDisplayCalibrationScreen();

	CWarningScreen::SetMessageWithHeaderAndSubtext(	WARNING_MESSAGE_STANDARD, 
		pTextLabelHeading, 
		pTextLabelBody,
		pTextLabelSubtext,
		static_cast<eWarningButtonFlags>(uFlagBits),
		bInsertNumber,
		NumberToInsert,
		pGxtFirstSubString,
		pGxtSecondSubString,
		bBackground,
		false,
		errorNumber);
}


const s32 WARNING_MESSAGE_FIRST_SUBSTRING_IS_LITERAL = 0x01;
const s32 WARNING_MESSAGE_SECOND_SUBSTRING_IS_LITERAL = 0x02;

void CommandSetWarningMessageWithHeaderAndSubstringFlags(const char *pTextLabelHeading,
										const char *pTextLabelBody,
										int iFlagBitField,
										const char *pTextLabelSubtext,
										const bool bInsertNumber,
										const s32 NumberToInsert, 
										const s32 SubStringFlags,
										const char *pFirstSubString, 
										const char *pSecondSubString,
										const bool bBackground,
										int errorNumber)
{
	const char *pGxtFirstSubString = NULL;
	const char *pGxtSecondSubString = NULL;

	if (pFirstSubString)
	{
		if (SubStringFlags & WARNING_MESSAGE_FIRST_SUBSTRING_IS_LITERAL)
		{
			pGxtFirstSubString = pFirstSubString;
		}
		else
		{
			pGxtFirstSubString = TheText.Get(pFirstSubString);
		}
	}

	if (pSecondSubString)
	{
		if (SubStringFlags & WARNING_MESSAGE_SECOND_SUBSTRING_IS_LITERAL)
		{
			pGxtSecondSubString = pSecondSubString;
		}
		else
		{
			pGxtSecondSubString = TheText.Get(pSecondSubString);
		}
	}

	// close the calibration screen if open (warning screen compatibility)
	CPauseMenu::CloseDisplayCalibrationScreen();

	CWarningScreen::SetMessageWithHeaderAndSubtext(	WARNING_MESSAGE_STANDARD, 
		pTextLabelHeading, 
		pTextLabelBody,
		pTextLabelSubtext,
		static_cast<eWarningButtonFlags>(iFlagBitField),
		bInsertNumber,
		NumberToInsert,
		pGxtFirstSubString,
		pGxtSecondSubString,
		bBackground,
		false,
		errorNumber);
}

void CommandSetWarningMessageWithHeaderAndSubstringFlagsExtended(
	const char *pTextLabelHeading,
	const char *pTextLabelBody,
	int iFlagBitFieldLower,
	int iFlagBitFieldUpper,
	const char *pTextLabelSubtext,
	const bool bInsertNumber,
	const s32 NumberToInsert, 
	const s32 SubStringFlags,
	const char *pFirstSubString, 
	const char *pSecondSubString,
	const bool bBackground,
	int errorNumber)
{
	const char *pGxtFirstSubString = NULL;
	const char *pGxtSecondSubString = NULL;
	u64 uFlagBits = (u64)iFlagBitFieldUpper << 32 | iFlagBitFieldLower;

	if (pFirstSubString)
	{
		if (SubStringFlags & WARNING_MESSAGE_FIRST_SUBSTRING_IS_LITERAL)
		{
			pGxtFirstSubString = pFirstSubString;
		}
		else
		{
			pGxtFirstSubString = TheText.Get(pFirstSubString);
		}
	}

	if (pSecondSubString)
	{
		if (SubStringFlags & WARNING_MESSAGE_SECOND_SUBSTRING_IS_LITERAL)
		{
			pGxtSecondSubString = pSecondSubString;
		}
		else
		{
			pGxtSecondSubString = TheText.Get(pSecondSubString);
		}
	}

	// close the calibration screen if open (warning screen compatibility)
	CPauseMenu::CloseDisplayCalibrationScreen();

	CWarningScreen::SetMessageWithHeaderAndSubtext(	WARNING_MESSAGE_STANDARD, 
		pTextLabelHeading, 
		pTextLabelBody,
		pTextLabelSubtext,
		static_cast<eWarningButtonFlags>(uFlagBits),
		bInsertNumber,
		NumberToInsert,
		pGxtFirstSubString,
		pGxtSecondSubString,
		bBackground,
		false,
		errorNumber);
}

int CommandGetWarningScreenMessageHash()
{
	return CWarningScreen::GetActiveMessage();
}

bool CommandSetWarningMessageOptionItems(const int iHighlightIndex,
										 const char *pNameString,
										 const int iCash,
										 const int iRp,
										 const int iLvl,
										 const int iCol)
{
	return (CWarningScreen::SetMessageOptionItems(iHighlightIndex, pNameString, iCash, iRp, iLvl, iCol));
}

bool CommandSetWarningMessageOptionHighlight(const int iHighlightIndex)
{
	return (CWarningScreen::SetMessageOptionHighlight(iHighlightIndex));
}

void CommandRemoveWarningMessageOptionItems()
{
	CWarningScreen::RemoveMessageOptionItems();
}

void CommandSetWarningMessageInUseThisFrame()
{
	CWarningScreen::SetWarningMessageInUse();
}

bool CommandIsWarningMessageReadyForControl()
{
	return CWarningScreen::IsActive() && CWarningScreen::GetMovieWrapper().IsActive();
}
//
// name:		CommandBeginScaleformMovieMethodOnFrontendHeader
// description: BEGIN scaleform movie method call on pausemenu header
bool CommandBeginScaleformMovieMethodOnWarningMessage(const char *pMethodName)
{
	if(SCRIPT_VERIFY(CWarningScreen::IsActive(), "BEGIN_SCALEFORM_MOVIE_METHOD_ON_WARNING_MESSAGE - Warning message not active"))
	{
		if(SCRIPT_VERIFY(CWarningScreen::GetMovieWrapper().IsActive(), "BEGIN_SCALEFORM_MOVIE_METHOD_ON_WARNING_MESSAGE - Warning message movie not active"))
		{
			sfDebugf3("Script %s called ActionScript method on warning message: %s", CTheScripts::GetCurrentScriptName(), pMethodName);

			return CWarningScreen::GetMovieWrapper().BeginMethod(pMethodName);
		}
	}

	return false;
}


// name:		CommandSetWarningMessage
// description: 
void CommandSetWarningMessage(const char *pTextLabelBody,
							  int iFlagBitField,
							  const char *pTextLabelSubtext,
							  const bool bInsertNumber,
							  const s32 NumberToInsert, 
							  const char *pFirstSubString, 
							  const char *pSecondSubString,
							  const bool bBackground,
							  int errorNumber)
{
	char *pGxtFirstSubString = NULL;
	char *pGxtSecondSubString = NULL;

	if (pFirstSubString)
	{
		pGxtFirstSubString = TheText.Get(pFirstSubString);
	}

	if (pSecondSubString)
	{
		pGxtSecondSubString = TheText.Get(pSecondSubString);
	}

	CWarningScreen::SetMessageAndSubtext(WARNING_MESSAGE_STANDARD, 
										 pTextLabelBody,
										 pTextLabelSubtext,
										 static_cast<eWarningButtonFlags>(iFlagBitField),
										 bInsertNumber,
										 NumberToInsert,
										 pGxtFirstSubString,
										 pGxtSecondSubString,
										 bBackground,
										 false,
										 errorNumber);
}

void CommandSetWarningMessageExtended(const char *pTextLabelBody,
									  int iFlagBitFieldLower,
									  int iFlagBitFieldUpper,
									  const char *pTextLabelSubtext,
									  const bool bInsertNumber,
									  const s32 NumberToInsert, 
									  const char *pFirstSubString, 
									  const char *pSecondSubString,
									  const bool bBackground,
									  int errorNumber)
{
	char *pGxtFirstSubString = NULL;
	char *pGxtSecondSubString = NULL;
	u64 uFlagBits = (u64)iFlagBitFieldUpper << 32 | iFlagBitFieldLower;

	if (pFirstSubString)
	{
		pGxtFirstSubString = TheText.Get(pFirstSubString);
	}

	if (pSecondSubString)
	{
		pGxtSecondSubString = TheText.Get(pSecondSubString);
	}

	CWarningScreen::SetMessageAndSubtext(WARNING_MESSAGE_STANDARD, 
		pTextLabelBody,
		pTextLabelSubtext,
		static_cast<eWarningButtonFlags>(uFlagBits),
		bInsertNumber,
		NumberToInsert,
		pGxtFirstSubString,
		pGxtSecondSubString,
		bBackground,
		false,
		errorNumber);
}


bool CommandIsWarningMessageActive()
{
	return CWarningScreen::IsActive();
}


void CommandDisplayPlayerNameTagsOnBlips(bool bDisplayTags)
{
	CScriptHud::ms_bDisplayPlayerNameBlipTags = bDisplayTags;
}


// name:		CommandDrawFrontendBackgroundThisFrame
// description: forces the background of the pausemenu to render this frame
void CommandDrawFrontendBackgroundThisFrame()
{
	// turns out this completely stops the pause menu from rendering, and ultimately doesn't visually do that much. Rather than fix that, let's just turn it off.
	//CScriptHud::bRenderFrontendBackgroundThisFrame = true;
}

void CommandDrawHudOverFadeThisFrame()
{
	CScriptHud::bRenderHudOverFadeThisFrame = true;
}

// name:		CommandActivateFrontEndMenu
// description: activates a frontend menu
void CommandActivateFrontEndMenu(s32 iMenuVersion, bool bPauseGame, s32 iHighlightTab)
{
	Displayf("##################################################################################################");
	Displayf("SCRIPT CALLED ACTIVATE_FRONTEND_MENU WITH Version %i, bPause %s, Highlight %i", iMenuVersion, bPauseGame?"true":"false", iHighlightTab);
	Displayf("##################################################################################################");


	if (CPauseMenu::IsActive())  // this ensures we can switch between menus easily (needs rework to be more efficient)
	{
#if !__FINAL
		Warningf("Pause Menu was at %s (0x%08x), but closing that down and firing up %s (0x%08x) instead! (This line can be ignored if everything's fine. But if it's not...)"
		, atHashString::TryGetString(CPauseMenu::GetCurrentMenuVersion()), CPauseMenu::GetCurrentMenuVersion()
		, atHashString::TryGetString(iMenuVersion),							iMenuVersion 
		);
#endif

		CPauseMenu::Close();
	}

	CPauseMenu::QueueOpen( eFRONTEND_MENU_VERSION(iMenuVersion), bPauseGame, eMenuScreen(iHighlightTab));
}


// name:		CommandRestartFrontEndMenu
// description: restart a frontend menu
void CommandRestartFrontEndMenu(s32 iMenuVersion, s32 iHighlightTab)
{
	Displayf("##################################################################################################");
	Displayf("SCRIPT CALLED RESTART_FRONTEND_MENU WITH Version %i, Highlight %i", iMenuVersion, iHighlightTab);
	Displayf("##################################################################################################");

	if(SCRIPT_VERIFY(CPauseMenu::IsActive(), "RESTART_FRONTEND_MENU - frontend not active - cannot restart if not active!"))
	{
		CPauseMenu::Restart(eFRONTEND_MENU_VERSION(iMenuVersion), eMenuScreen(iHighlightTab));
	}
	else
	{
		CPauseMenu::QueueOpen(eFRONTEND_MENU_VERSION(iMenuVersion), true, eMenuScreen(iHighlightTab));
	}
}



// name:		CommandGetCurrentFrontEndMenuVersion
// description: returns current frontend version
s32 CommandGetCurrentFrontEndMenuVersion()
{
	if(SCRIPT_VERIFY(CPauseMenu::IsActive(), "GET_CURRENT_FRONTEND_MENU_VERSION - frontend not active - cannot restart if not active!"))
	{
		return CPauseMenu::GetCurrentMenuVersion();
	}

	return FE_MENU_VERSION_INVALID;
}


// name:		CommandDisableFrontEndThisFrame
// description: disables the frontend menu this frame
void CommandDisableFrontEndThisFrame()
{
	CScriptHud::bDisablePauseMenuThisFrame = true;  // disable the pause menu this frame
}

// name:		CommandDisableFrontEndThisFrame
// description: disables the frontend menu from rendering this frame
void CommandSuppressFrontEndRenderingThisFrame()
{
	CScriptHud::bSuppressPauseMenuRenderThisFrame = true;  // disable the pause menu this frame
}

// name:		CommandAllowPauseWhenNotInStateOfPlayThisFrame
// description: allows pause even when not in a state of play this frame
void CommandAllowPauseWhenNotInStateOfPlayThisFrame()
{
	CScriptHud::bAllowPauseWhenInNotInStateOfPlayThisFrame = true;  // disable the pause menu this frame
}




// name:		CommandSetPauseMenuActive
// description: disables the pause menu
void CommandSetPauseMenuActive(bool bAllowPauseMenu)
{
	// we dont want to allow the pause menu to work
	if (!bAllowPauseMenu)
	{
		CScriptHud::bDisablePauseMenuThisFrame = true;  // disable the pause menu this frame
	}
	else
	{
		CScriptHud::bDisablePauseMenuThisFrame = false;  // allow the pause menu this frame (will always reset to this every frame update anyway)
	}
}



// name:		CommmandSetFrontendActive
// description: activate or deactivate the single player pause menu
void CommmandSetFrontendActive(bool bActive)
{
#if !__FINAL  // to catch bugs where script are removing the pausemenu - we can catch the offenders from the logs with this output
	if (bActive)
		uiDisplayf("SET_FRONTEND_ACTIVE - Script %s has called SET_FRONTEND_ACTIVE(TRUE)", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	else
		uiDisplayf("SET_FRONTEND_ACTIVE - Script %s has called SET_FRONTEND_ACTIVE(FALSE)", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		
	scrThread::PrePrintStackTrace();
#endif

	if (bActive)
	{
		CPauseMenu::QueueOpen(FE_MENU_VERSION_SP_PAUSE);
	}
	else
	{
		// clear UT script gfx buffers here if we close on the "ONLINE" menu tab: (fixes 1674299)
		// no real nice way of adding this in so if script close down the pausemenu after a warning screen and we are on the online tab we
		// reset the script draw array - this will mean they are starting a new type of session, ie ONLINE or CREATOR and we want the
		// existing sprites/rect removed
		if ( (!CPauseMenu::IsClosingDown()) && (CPauseMenu::IsInOnlineScreen()) && CWarningScreen::IsActive() )
		{
			uiDisplayf("RESETTING SCRIPT ARRAY because Script closed down pausemenu whilst in ONLINE tab");

			CScriptHud::GetIntroTexts().GetWriteBuf().Initialise(true);
			CScriptHud::GetIntroRects().GetWriteBuf().Initialise(true);
		}

		CPauseMenu::Close();
	}
}

void CommandPauseMenuceptionGoDeeper(s32 eMenuScreenId)
{
	CPauseMenu::MenuceptionGoDeeper( MenuScreenId(eMenuScreenId) );
}

void CommandPauseMenuceptionTheKick()
{
	CPauseMenu::MenuceptionTheKick();
}

void CommandPauseToggleFullscreenMap(bool bOpen)
{
	CPauseMenu::ToggleFullscreenMap(bOpen);
}

// name:		CommandIsPauseMenuActive
// description: whether the pause menu is active
bool CommandIsPauseMenuActive()
{
	// Needs to return true when in the SC and Store menus - jchagaris
	bool bReturnValue = CPauseMenu::IsActive( PM_EssentialAssets );

/*#if __DEV
	if (bReturnValue)
		uiDisplayf("IS_PAUSE_MENU_ACTIVE returned TRUE - script can use pausemenu commands");
	else
		uiDisplayf("IS_PAUSE_MENU_ACTIVE returned FALSE - script not ready to use pausemenu commands");
#endif // __DEV*/

	return bReturnValue;
}

void CommandSetPadCanShakeDuringPauseMenu(bool bAllowShake)
{
	CPauseMenu::AllowRumble(bAllowShake);
}

bool CommandIsStorePendingNetworkShutdownToOpen()
{
	return (cStoreScreenMgr::IsPendingNetworkShutdownToOpen());
}

int CommandGetPauseMenuState()
{
	return static_cast<int>(CPauseMenu::GetStateForScript());
}

scrVector CommandGetPauseMenuPosition()
{
	if(scriptVerifyf(CPauseMenu::IsActive(), "GET_PAUSE_MENU_POSITION -- Trying to get pause menu position, but the pause menu is not active"))
	{
		return CPauseMenu::GetPosition();
	}

	return Vector3( Vector3::ZeroType);
}


// name:		CommandIsPauseMenuRestarting
// description: whether the pause menu is restarting
bool CommandIsPauseMenuRestarting()
{
	return CPauseMenu::IsRestarting();
}

// Activates a given context
void CommandPauseMenuActivateContext(int whichContext)
{
	SUIContexts::Activate(whichContext);
}

// deactivates a given context
void CommandPauseMenuDeactivateContext(int whichContext)
{
	SUIContexts::Deactivate(whichContext);
}

// determines if a context is active or not
bool CommandPauseMenuIsContextActive(int whichContext)
{
	return SUIContexts::IsActive(whichContext);
}

//Returns true if the context menu is currently open.
bool CommandPauseMenuIsContextMenuActive()
{
	return (CPauseMenu::IsCurrentScreenValid() && CPauseMenu::GetCurrentScreenData().IsContextMenuOpen()) ||
		(SReportMenu::GetInstance().IsActive());
}

// Returns the colour index of the hair selected by AS with the mouse.
int CommandPauseMenuGetHairColourId()
{
	return CPauseMenu::GetHairColourIndex();
}


// Returns the index of the pause menu item currently moused over
int CommandPauseMenuGetMouseHoverIndex()
{
	return CPauseMenu::GetMouseHoverIndex();
}

// Returns the menu item ID of the pause menu item currently moused over
int CommandPauseMenuGetMouseHoverMenuItemId()
{
	return CPauseMenu::GetMouseHoverMenuItemId();
}

// Returns the unique ID of the pause menu item currently moused over
int CommandPauseMenuGetMouseHoverUniqueId()
{
	return CPauseMenu::GetMouseHoverUniqueId();
}

// Returns the index of the pause menu item currently moused over
bool CommandPauseMenuGetMouseClickEvent(int& KEYBOARD_MOUSE_ONLY(index), int& KEYBOARD_MOUSE_ONLY(menuID), int& KEYBOARD_MOUSE_ONLY(uniqueID))
{
#if KEYBOARD_MOUSE_SUPPORT
	const CPauseMenu::PMMouseEvent& clickEvent = CPauseMenu::GetClickEvent();

	if(clickEvent != CPauseMenu::PMMouseEvent::INVALID_MOUSE_EVENT)
	{
		index		= clickEvent.iIndex;
		menuID		= clickEvent.iMenuItemId;
		uniqueID	= clickEvent.iUniqueId;

		return true;
	}
#endif

	return false;
}

// useful to call after changing some
void CommandPauseMenuRedrawInstructionalButtons(int iUniqueId)
{
	CPauseMenu::RedrawInstructionalButtons(iUniqueId);
}

void CommandPauseMenuSetBusySpinner(bool bIsVisible, int iColumn, int iSpinnerIndex)
{
	if( SCRIPT_VERIFYF(iColumn >= PauseMenuRenderData::NO_SPINNER && iColumn < CPauseMenu::GetMaxSpinnerColumn(), "Trying to set a spinner column (%i) out of range [%i, %i)", iColumn, PauseMenuRenderData::NO_SPINNER, CPauseMenu::GetMaxSpinnerColumn()) 
		&& SCRIPT_VERIFYF(iSpinnerIndex >= 0 && iSpinnerIndex < PauseMenuRenderData::MAX_SPINNERS, "Trying to set a iSpinnerIndex (%i) out of accepted range [0,%i)", iSpinnerIndex, PauseMenuRenderData::MAX_SPINNERS)
		)
	{
		CPauseMenu::SetBusySpinner(bIsVisible, static_cast<s8>(iColumn), iSpinnerIndex);
	}
}

void CommandPauseMenuSetWarnOnTabChange(bool bWarn)
{
	CPauseMenu::SetWarnOnTabChange(bWarn ? "TAB_CHANGE" : NULL);
}

#if RSG_GEN9
void CommandPauseMenuSetCloudBusySpinner(const char* pTextLabel, bool bNoMenu, bool bBlackBackground)
{
	CPauseMenu::SetCloudBusySpinner(pTextLabel, bNoMenu, bBlackBackground);
}

void CommandPauseMenuClearCloudBusySpinner()
{
	CPauseMenu::ClearCloudBusySpinner();
}
#endif //RSG_GEN9

// name:		CommandForceScriptedGfxWhenFrontendActive
// description: forces scripted sprites and rectangles etc to update and display when pausemenu is active
void CommandForceScriptedGfxWhenFrontendActive(bool /*bAllow*/)
{
	//scriptAssertf(false, "%s:FORCE_SCRIPTED_GFX_WHEN_FRONTEND_ACTIVE - Deprecated. ", CTheScripts::GetCurrentScriptNameAndProgramCounter(), name);
}



// name:		CommandIsFrontendReadyForControl
// description: this should be checked before script try to do anything with the frontend
bool CommandIsFrontendReadyForControl()
{
	return (CPauseMenu::IsActive() && CScaleformMgr::IsMovieActive(CPauseMenu::GetContentMovieId()));
}


// name:		CommandTakeControlOfFrontend
// description: allows script to take control of the frontend menus
void CommandTakeControlOfFrontend()
{
	return CPauseMenu::LockMenuControl(false);
}


// name:		CommandReleaseControlOfFrontend
// description: releases control of the frontend menus back to code
void CommandReleaseControlOfFrontend()
{
	return CPauseMenu::UnlockMenuControl();
}


// name:		CommandCodeWantsScriptToTakeControl
// description: This should be checked every frame and when it returns true, check CommandGetScreenCodeWantsScriptToTakeControl
bool CommandCodeWantsScriptToTakeControl()
{
	return (CPauseMenu::GetCodeWantsScriptToControl());
}

// name:		CommandGetScreenCodeWantsScriptToControl
// description: This should only be called once CommandCodeWantsScriptToTakeControl returns true and will give the id of the screen code wants script to populate
s32 CommandGetScreenCodeWantsScriptToControl()
{
	return (CPauseMenu::GetScreenCodeWantsScriptToControl());
}

// name:		CommandIsNavigatingMenuContent
// description: returns whether navigating pause menu content to script
bool CommandIsNavigatingMenuContent()
{
	return (CPauseMenu::IsNavigatingContent());
}

// name:		CommandGetMenuTriggerEventOccurred
// description: This should be checked every frame and when it returns true, check CommandGetMenuTriggerEventDetails
bool CommandHasMenuTriggerEventOccurred()
{
	return (CPauseMenu::GetMenuTriggerEventOccurred());
}

// name:		CommandGetMenuLayoutChangedEventOccurred
// description: This should be checked every frame and when it returns true, check CommandGetMenuLayoutChangedEventDetails
bool CommandHasMenuLayoutChangedEventOccurred()
{
	return (CPauseMenu::GetMenuLayoutChangedEventOccurred());
}

void CommandSetSavegameListUniqueId(s32 iUniqueId)
{
	CSavegameFrontEnd::SetHighlightedMenuItem(iUniqueId);
}

// name:		CommandSetFrontendTabLocked
// description: sets the tab to be locked or unlocked
void CommandSetFrontendTabLocked(s32 iTabNumber, bool bLock)
{
	CPauseMenu::LockMenuTab(iTabNumber, bLock);
}

// name:		CommandGetMenuTriggerEventDetails
// description: This should only be called once CommandGetMenuTriggerEventOccurred returns true and will give the details back to script
void CommandGetMenuTriggerEventDetails(int &iTriggerId, int &iMenuIndex)
{
	iTriggerId = CPauseMenu::GetEventOccurredUniqueId(0);  // 0 and 2 only (we ignore 1 for this)
	iMenuIndex = CPauseMenu::GetEventOccurredUniqueId(2);
}

// name:		CommandGetMenuTriggerEventDetails
// description: This should only be called once CommandGetMenuTriggerEventOccurred returns true and will give the details back to script
void CommandGetMenuLayoutChangedEventDetails(int &iPreviousId, int &iNextId, int &iMenuIndex)
{
	iPreviousId = CPauseMenu::GetEventOccurredUniqueId(0);
	iNextId = CPauseMenu::GetEventOccurredUniqueId(1);
	iMenuIndex = CPauseMenu::GetEventOccurredUniqueId(2);
}

// name:		CommandSetContentArrowVisible
// description:
void CommandSetContentArrowVisible(bool bLeftArrow, bool bVisible)
{
	CPauseMenu::SetContentArrowVisible(bLeftArrow, bVisible);
}

// name:		CommandSetContentArrowPosition
// description:
void CommandSetContentArrowPosition(bool bLeftArrow, float fPositionX, float fPositionY)
{
	CPauseMenu::SetContentArrowPosition(bLeftArrow, fPositionX, fPositionY);
}

bool GetMenuPedu64Stat(const int statHash, u64& data, int iCharacterSlot = -1)
{
	const sStatDescription* pDesc = StatsInterface::GetStatDesc(statHash);
	if(scriptVerifyf(pDesc, "GET_MENU_PED_INT_SET - Couldn't find stat with hash 0x%08x(%d)", statHash, statHash) &&
		scriptVerifyf(pDesc->GetIsProfileStat(), "GET_MENU_PED_INT_SET - 0x%08x(%d) isn't a profile stat, we need to make it one for the menu ped to work.", statHash, statHash) &&
		scriptVerifyf(CPauseMenu::IsActive(), "GET_MENU_PED_INT_SET - called when the pause menu isn't active."))
	{
		int playerIndex = SPlayerCardManager::GetInstance().GetCurrentGamerIndex();

		if (playerIndex == -1)
			return false;

		int characterSlot = iCharacterSlot;
		
		if (iCharacterSlot == -1)
			characterSlot = CPlayerListMenu::GetCharacterSlot(playerIndex);

		const rlProfileStatsValue* pStat = SPlayerCardManager::GetInstance().GetCorrectedMultiStatForCurrentGamer(statHash, characterSlot);
		if(pStat)
		{
			switch(pStat->GetType())
			{
			case RL_PROFILESTATS_TYPE_INT32:
				data = static_cast<u64>(pStat->GetInt32());
				break;
			case RL_PROFILESTATS_TYPE_INT64:
				data = static_cast<u64>(pStat->GetInt64());
				break;
			case RL_PROFILESTATS_TYPE_FLOAT:
				scriptAssertf(0, "GET_MENU_PED_INT_SET - 0x%08x(%d) - Trying to get a stat as a int which is actually a float.", statHash, statHash);
				data = static_cast<u64>(pStat->GetFloat());
				break;
			default:
				data = 0;
				scriptDisplayf("GET_MENU_PED_INT_SET - 0x%08x(%d) is an unknown stat type (%d).", statHash, statHash, (int)pStat->GetType());
				break;
			}

			return true;
		}
	}

	return false;
}

bool CommandGetMenuPedMaskedIntStat(const int statHash, int& data, int shift, int numberOfBits)
{
	u64 statValue = 0;

	const bool ret = GetMenuPedu64Stat(statHash, statValue);
	if(ret)
	{
		data = SPlayerCardManager::GetInstance().ExtractPackedValue(statValue, (u64)shift, (u64)numberOfBits);
	}

	return ret;
}

bool CommandGetCharacterMenuPedMaskedIntStat(const int statHash, int& data, int shift, int numberOfBits, int characterSlot)
{
	if(scriptVerifyf(characterSlot >= 0, "CommandGetCharacterMenuPedMaskedIntStat - characterSlot < 0") )
	{
		u64 statValue = 0;

		const bool ret = GetMenuPedu64Stat(statHash, statValue, characterSlot);
		if(ret)
		{
			data = SPlayerCardManager::GetInstance().ExtractPackedValue(statValue, (u64)shift, (u64)numberOfBits);
		}

		return ret;
	}

	return false;
}

bool CommandGetMenuPedIntStat(const int statHash, int& data)
{
	u64 uData = 0;
	if(GetMenuPedu64Stat(statHash, uData))
	{
		data = static_cast<int>(uData);
		return true;
	}
	else if(SPlayerCardManager::GetInstance().GetPackedStatValue(SPlayerCardManager::GetInstance().GetCurrentGamerIndex(), statHash, data))
	{
		return true;
	}

	return false;
}

bool CommandGetCharacterMenuPedIntStat(const int statHash, int& data, int characterSlot = -1)
{
	u64 uData = 0;
	if(GetMenuPedu64Stat(statHash, uData, characterSlot))
	{
		data = static_cast<int>(uData);
		return true;
	}
	else if(SPlayerCardManager::GetInstance().GetPackedStatValue(SPlayerCardManager::GetInstance().GetCurrentGamerIndex(), statHash, data))
	{
		return true;
	}

	return false;
}

bool CommandGetMenuPedFloatStat(const int statHash, float& data)
{
	const sStatDescription* pDesc = StatsInterface::GetStatDesc(statHash);
	if(scriptVerifyf(pDesc, "GET_MENU_PED_FLOAT_STAT - Couldn't find stat with hash 0x%08x(%d)", statHash, statHash) &&
		scriptVerifyf(pDesc->GetIsProfileStat(), "GET_MENU_PED_FLOAT_STAT - 0x%08x(%d) isn't a profile stat, we need to make it one for the menu ped to work.", statHash, statHash) &&
		scriptVerifyf(CPauseMenu::IsActive(), "GET_MENU_PED_FLOAT_STAT - called when the pause menu isn't active."))
	{
		int playerIndex = SPlayerCardManager::GetInstance().GetCurrentGamerIndex();
		int characterSlot = CPlayerListMenu::GetCharacterSlot(playerIndex);
		const rlProfileStatsValue* pStat = SPlayerCardManager::GetInstance().GetCorrectedMultiStatForCurrentGamer(statHash, characterSlot);
		if(pStat)
		{
			switch(pStat->GetType())
			{
			case RL_PROFILESTATS_TYPE_INT32:
				scriptAssertf(0, "GET_MENU_PED_FLOAT_STAT - 0x%08x(%d) - Trying to get a stat as a float which is actually a int.", statHash, statHash);
				data = (float) pStat->GetInt32();
				break;
			case RL_PROFILESTATS_TYPE_INT64:
				scriptAssertf(0, "GET_MENU_PED_FLOAT_STAT - 0x%08x(%d) - Trying to get a stat as a float which is actually a int64.", statHash, statHash);
				data = (float) pStat->GetInt64();
				break;
			case RL_PROFILESTATS_TYPE_FLOAT:
				data = pStat->GetFloat();
				break;
			default:
				data = 0.0f;
				scriptDisplayf("GET_MENU_PED_FLOAT_STAT - 0x%08x(%d) is an unknown stat type (%d).", statHash, statHash, (int)pStat->GetType());
				break;
			}

			return true;
		}
	}

	return false;
}

bool CommandGetCharacterMenuPedFloatStat(const int statHash, float& data, int iCharacterSlot = -1)
{
	const sStatDescription* pDesc = StatsInterface::GetStatDesc(statHash);
	if(scriptVerifyf(pDesc, "GET_MENU_PED_FLOAT_STAT - Couldn't find stat with hash 0x%08x(%d)", statHash, statHash) &&
		scriptVerifyf(pDesc->GetIsProfileStat(), "GET_MENU_PED_FLOAT_STAT - 0x%08x(%d) isn't a profile stat, we need to make it one for the menu ped to work.", statHash, statHash) &&
		scriptVerifyf(CPauseMenu::IsActive(), "GET_MENU_PED_FLOAT_STAT - called when the pause menu isn't active."))
	{
		int playerIndex = SPlayerCardManager::GetInstance().GetCurrentGamerIndex();
		if(iCharacterSlot == - 1)
			iCharacterSlot = CPlayerListMenu::GetCharacterSlot(playerIndex);
		const rlProfileStatsValue* pStat = SPlayerCardManager::GetInstance().GetCorrectedMultiStatForCurrentGamer(statHash, iCharacterSlot);
		if(pStat)
		{
			switch(pStat->GetType())
			{
			case RL_PROFILESTATS_TYPE_INT32:
				scriptAssertf(0, "GET_MENU_PED_FLOAT_STAT - 0x%08x(%d) - Trying to get a stat as a float which is actually a int.", statHash, statHash);
				data = (float) pStat->GetInt32();
				break;
			case RL_PROFILESTATS_TYPE_INT64:
				scriptAssertf(0, "GET_MENU_PED_FLOAT_STAT - 0x%08x(%d) - Trying to get a stat as a float which is actually a int64.", statHash, statHash);
				data = (float) pStat->GetInt64();
				break;
			case RL_PROFILESTATS_TYPE_FLOAT:
				data = pStat->GetFloat();
				break;
			default:
				data = 0.0f;
				scriptDisplayf("GET_MENU_PED_FLOAT_STAT - 0x%08x(%d) is an unknown stat type (%d).", statHash, statHash, (int)pStat->GetType());
				break;
			}

			return true;
		}
	}

	return false;
}

bool CommandGetMenuPedBoolStat(const int statHash, int& data)
{
	const sStatDescription* pDesc = StatsInterface::GetStatDesc(statHash);
	if(scriptVerifyf(pDesc, "GET_MENU_PED_BOOL_STAT - Couldn't find stat with hash 0x%08x(%d)", statHash, statHash) &&
		scriptVerifyf(pDesc->GetIsProfileStat(), "GET_MENU_PED_BOOL_STAT - 0x%08x(%d) isn't a profile stat, we need to make it one for the menu ped to work.", statHash, statHash) &&
		scriptVerifyf(CPauseMenu::IsActive(), "GET_MENU_PED_BOOL_STAT - called when the pause menu isn't active."))
	{
		int playerIndex = SPlayerCardManager::GetInstance().GetCurrentGamerIndex();
		int characterSlot = CPlayerListMenu::GetCharacterSlot(playerIndex);
		const rlProfileStatsValue* pStat = SPlayerCardManager::GetInstance().GetCorrectedMultiStatForCurrentGamer(statHash, characterSlot);
		if(pStat)
		{
			switch(pStat->GetType())
			{
			case RL_PROFILESTATS_TYPE_INT32:
				data = pStat->GetInt32();
				break;
			case RL_PROFILESTATS_TYPE_INT64:
				data = (int)pStat->GetInt64();
				break;
			case RL_PROFILESTATS_TYPE_FLOAT:
				scriptAssertf(0, "GET_MENU_PED_BOOL_STAT - 0x%08x(%d) - Trying to get a stat as a bool which is actually a float.", statHash, statHash);
				data = (int)pStat->GetFloat();
				break;
			default:
				data = false;
				scriptDisplayf("GET_MENU_PED_BOOL_STAT - 0x%08x(%d) is an unknown stat type (%d).", statHash, statHash, (int)pStat->GetType());
				break;
			}

			return true;
		}
	}

	return false;
}

bool CommandGetCharacterMenuPedBoolStat(const int statHash, int& data, int iCharacterSlot = -1)
{
	const sStatDescription* pDesc = StatsInterface::GetStatDesc(statHash);
	if(scriptVerifyf(pDesc, "GET_MENU_PED_BOOL_STAT - Couldn't find stat with hash 0x%08x(%d)", statHash, statHash) &&
		scriptVerifyf(pDesc->GetIsProfileStat(), "GET_MENU_PED_BOOL_STAT - 0x%08x(%d) isn't a profile stat, we need to make it one for the menu ped to work.", statHash, statHash) &&
		scriptVerifyf(CPauseMenu::IsActive(), "GET_MENU_PED_BOOL_STAT - called when the pause menu isn't active."))
	{
		int playerIndex = SPlayerCardManager::GetInstance().GetCurrentGamerIndex();
		if(iCharacterSlot == - 1)
			iCharacterSlot = CPlayerListMenu::GetCharacterSlot(playerIndex);
		const rlProfileStatsValue* pStat = SPlayerCardManager::GetInstance().GetCorrectedMultiStatForCurrentGamer(statHash, iCharacterSlot);
		if(pStat)
		{
			switch(pStat->GetType())
			{
			case RL_PROFILESTATS_TYPE_INT32:
				data = pStat->GetInt32();
				break;
			case RL_PROFILESTATS_TYPE_INT64:
				data = (int)pStat->GetInt64();
				break;
			case RL_PROFILESTATS_TYPE_FLOAT:
				scriptAssertf(0, "GET_MENU_PED_BOOL_STAT - 0x%08x(%d) - Trying to get a stat as a bool which is actually a float.", statHash, statHash);
				data = (int)pStat->GetFloat();
				break;
			default:
				data = false;
				scriptDisplayf("GET_MENU_PED_BOOL_STAT - 0x%08x(%d) is an unknown stat type (%d).", statHash, statHash, (int)pStat->GetType());
				break;
			}

			return true;
		}
	}

	return false;
}

bool CommandGetPMPlayerCrewColor(int& r, int& g, int& b)
{
	CPlayerCardManager::CrewStatus status = CPlayerCardManager::CREWSTATUS_NODATA;

	if(SPlayerCardManager::IsInstantiated())
	{
		status = SPlayerCardManager::GetInstance().GetCrewStatus();
		if(status == CPlayerCardManager::CREWSTATUS_HASDATA)
		{
			const Color32& color = SPlayerCardManager::GetInstance().GetCurrentCrewColor();
			r = (int)color.GetRed();
			g = (int)color.GetGreen();
			b = (int)color.GetBlue();
			return true;
		}
	}

	r = -1;
	g = -1;
	b = -1;
	return status == CPlayerCardManager::CREWSTATUS_NODATA;
}

void CommandClearPedInPauseMenu()
{
	if(CPauseMenu::IsActive())
	{
		if(CPauseMenu::GetDisplayPed())
		{
			CPauseMenu::GetDisplayPed()->ClearPed();
		}
	}
}

void CommandClearDynamicPauseMenuErrorMessage()
{
	if(CPauseMenu::DynamicMenuExists())
	{
		CPauseMenu::GetDynamicPauseMenu()->GetErrorMessage().Clear();
	}
}

void CommandGivePedToPauseMenu(s32 pedIndex, s32 column)
{
	CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(pedIndex, CTheScripts::GUID_ASSERT_FLAG_ENTITY_EXISTS);
	if(pPed && CPauseMenu::IsActive())
	{
		CTheScripts::UnregisterEntity(pPed, true);
		pPed->PopTypeSet(POPTYPE_PERMANENT);

		if(CPauseMenu::GetDisplayPed())
		{
#if RSG_PC
			const u32 numFramesToDelayFade = 3;
			const u32 fadeDurationMS = 250;
#else
			const u32 numFramesToDelayFade = 1;
			const u32 fadeDurationMS = 175;
#endif // RSG_PC
			CPauseMenu::GetDisplayPed()->SetFadeIn(fadeDurationMS, fwTimer::GetTimeStepInMilliseconds() * numFramesToDelayFade);

 			if(column == PM_COLUMN_MIDDLE &&
 				CPauseMenu::GetDynamicPauseMenu()->IsLocalShowing())
 			{
 				column = PM_COLUMN_EXTRA3;
 			}

			CPauseMenu::GetDisplayPed()->SetPedModel(pPed, static_cast<PM_COLUMNS>(column));
		}
	}
}

void CommandSetPMPedBGVisibility(bool isVisible)
{
	if(CPauseMenu::GetDisplayPed())
	{
		CPauseMenu::GetDisplayPed()->SetVisible(isVisible);
	}
}

void CommandShowPlayerPedInPauseMenu(s32 iPlayerId, s32 column)
{
	if(scriptVerifyf(NetworkInterface::IsNetworkOpen(), "SHOW_PLAYER_PED_IN_PAUSE_MENU - Should only be called when the network is open.") &&
		scriptVerifyf(CPauseMenu::IsActive(), "SHOW_PLAYER_PED_IN_PAUSE_MENU - Should only be called when the pause menu is open."))
	{
		CPed* pPed = CTheScripts::FindNetworkPlayerPed(iPlayerId);
		if(pPed && pPed->GetPlayerInfo())
		{
			CNetGamePlayer* pPlayer = NetworkInterface::GetPhysicalPlayerFromIndex(static_cast<PhysicalPlayerIndex>(pPed->GetPlayerInfo()->GetPhysicalPlayerIndex()));
			if(scriptVerifyf(pPlayer, "SHOW_PLAYER_PED_IN_PAUSE_MENU - Was given an invalid player index - %d", iPlayerId))
			{
				if(CPauseMenu::GetDisplayPed())
				{
					CPauseMenu::GetDisplayPed()->SetPedModel(pPlayer->GetActivePlayerIndex(), static_cast<PM_COLUMNS>(column));
				}
			}
		}
	}
}

void CommandSetPauseMenuPedLighting(bool enableLights)
{
	if(scriptVerifyf(NetworkInterface::IsNetworkOpen(), "SET_PAUSE_MENU_PED_LIGHTING - Should only be called when the network is open.") &&
		scriptVerifyf(CPauseMenu::IsActive(), "SET_PAUSE_MENU_PED_LIGHTING - Should only be called when the pause menu is open."))
	{
		if(CPauseMenu::GetDisplayPed())
		{
			CPauseMenu::GetDisplayPed()->SetIsLit(enableLights);
		}
	}
}

void CommandSetPauseMenuPedSleepState(bool isAwake)
{
	if(scriptVerifyf(NetworkInterface::IsNetworkOpen(), "SET_PAUSE_MENU_PED_SLEEP_STATE - Should only be called when the network is open.") &&
		scriptVerifyf(CPauseMenu::IsActive(), "SET_PAUSE_MENU_PED_SLEEP_STATE - Should only be called when the pause menu is open."))
	{
		if(CPauseMenu::GetDisplayPed())
		{
			CPauseMenu::GetDisplayPed()->SetIsAsleep(!isAwake);
		}
	}
}

void CommandOpenOnlinePoliciesMenu()
{
	SocialClubMenu::Open(true);	
}

bool CommandAreOnlinePoliciesUpToDate()
{
#if __BANK
	if(!CLiveManager::GetSocialClubMgr().GetUpToDateToggleState())
	{
		return false;
	}
#endif

	if(!SPolicyVersions::IsInstantiated())
	{
		SPolicyVersions::Instantiate();
	}

	if(!SPolicyVersions::GetInstance().IsInitialized())
	{
		SPolicyVersions::GetInstance().Init();
	}

	if(SPolicyVersions::GetInstance().IsUpToDate())
	{
		return true;
	}
	else if(SPolicyVersions::GetInstance().HasEverAccepted() &&
		(SPolicyVersions::GetInstance().IsDownloading() || SPolicyVersions::GetInstance().GetError()))
	{
		return true;
	}

	return false;
}

bool CommandIsOnlinePoliciesActive()
{
	return SocialClubMenu::IsActive();
}

void CommandOpenSocialClubMenu()
{
	CPauseMenu::EnterSocialClub();
}

void CommandCloseSocialClubMenu()
{
	if (SocialClubMenu::IsActive())
	{
		SocialClubMenu::FlagForDeletion();
	}
}

void CommandSetSocialClubTour(const char* pTourName)
{
	if(scriptVerify(pTourName))
	{
		SocialClubMenu::SetTourHash(atStringHash(pTourName));
	}
}

bool CommandIsSocialClubActive()
{
	return SocialClubMenu::IsActive();
}

void CommandSetTextInputBoxEnabled(bool WIN32PC_ONLY(bEnabled))
{
#if RSG_PC
	STextInputBox::GetInstance().SetEnabled(bEnabled);
#endif // RSG_PC
}

void CommandCloseTextInputBox()
{
#if RSG_PC
	if(STextInputBox::GetInstance().IsActive())
	{
		STextInputBox::GetInstance().Close();
		STextInputBox::GetInstance().DestroyState();
	}
#endif // RSG_PC
}

void CommandSetAllowCommaOnTextInputBox(bool WIN32PC_ONLY(bAllowComma))
{
#if RSG_PC
	STextInputBox::GetInstance().SetAllowComma(bAllowComma);
#endif // RSG_PC
}


void CommandScriptOverrideMPTeamChatString(const int WIN32PC_ONLY(iOverrideHash))
{
#if RSG_PC
	SMultiplayerChat::GetInstance().OverrideTeamLocalizationString(iOverrideHash);
#endif
}

void CommandCloseMPTextChat()
{
#if RSG_PC
	if (SMultiplayerChat::IsInstantiated() && SMultiplayerChat::GetInstance().IsChatTyping())
	{
		SMultiplayerChat::GetInstance().ForceAbortTextChat();
	}
#endif // RSG_PC
}

bool CommandIsMPTextChatTyping()
{
	#if RSG_PC
	if (SMultiplayerChat::IsInstantiated() && SMultiplayerChat::GetInstance().IsChatTyping())
	{
		return true;
	}
	#endif // RSG_PC

	return false;
}

void CommandDisableTextChat(bool WIN32PC_ONLY(b))
{
	#if RSG_PC
	if (SMultiplayerChat::IsInstantiated())
	{
		SMultiplayerChat::GetInstance().DisableTextChat(b);
	}
	#endif
}

void CommandIsTeamBasedJob(bool b)
{
#if RSG_PC
	if (SMultiplayerChat::IsInstantiated())
	{
		SMultiplayerChat::GetInstance().IsTeamBasedJob(b);
	}
#else
	(void)b;
#endif // RSG_PC
}

void CommandOverrideMPTextChatColor(bool const c_onOff, s32 const c_hudColor)
{
#if RSG_PC
	if (SMultiplayerChat::IsInstantiated())
	{
		SMultiplayerChat::GetInstance().OverrideColor(c_onOff, static_cast<eHUD_COLOURS>(c_hudColor));
	}
#else
	(void)c_onOff;
	(void)c_hudColor;
#endif // RSG_PC
}

void CommandFlagPlayerContextInTournament(bool bInTournament)
{
	CContextMenuHelper::FlagPlayerContextInTournament(bInTournament);
}

// CScriptPedAIBlips Interface
void CommandSetPedHasAIBlip(s32 iPlayerIndex, bool bOnOff)
{
	scrThreadId scriptThreadId = CTheScripts::GetCurrentGtaScriptThread()->GetThreadId();
	CScriptPedAIBlips::SetPedHasAIBlip((s32)scriptThreadId, iPlayerIndex, bOnOff);
}

// CScriptPedAIBlips Interface
void CommandSetPedHasAIBlipWithColour(s32 iPlayerIndex, bool bOnOff, s32 colourOverride)
{
	scrThreadId scriptThreadId = CTheScripts::GetCurrentGtaScriptThread()->GetThreadId();
	CScriptPedAIBlips::SetPedHasAIBlip((s32)scriptThreadId, iPlayerIndex, bOnOff, colourOverride);
}

bool CommandDoesPedHaveAIBlip(s32 iPlayerIndex)
{
	return CScriptPedAIBlips::DoesPedhaveAIBlip(iPlayerIndex);
}

void CommandSetAIBlipGangID(s32 iPlayerIndex, s32 gangID)
{
	CScriptPedAIBlips::SetPedAIBlipGangID(iPlayerIndex, gangID);
}

void CommandsSetAIBlipHasCone(s32 iPlayerIndex, bool bHasCone)
{
	CScriptPedAIBlips::SetPedHasConeAIBlip(iPlayerIndex, bHasCone);
}

void CommandsSetAIBlipForcedOn(s32 iPlayerIndex, bool bOnOff)
{
	CScriptPedAIBlips::SetPedHasForcedBlip(iPlayerIndex, bOnOff);
}

void CommandsSetAIBlipNoticeRange(s32 iPlayerIndex, float range)
{
	CScriptPedAIBlips::SetPedAIBlipNoticeRange(iPlayerIndex, range);
}

void CommandsSetAIBlipChangeColour(s32 iPlayerIndex)
{
	CScriptPedAIBlips::SetPedAIBlipChangeColour(iPlayerIndex);
}

void CommandsSetAIBlipSprite(s32 iPlayerIndex, int spriteID)
{
	CScriptPedAIBlips::SetPedAIBlipSprite(iPlayerIndex, spriteID);
}

s32 CommandsGetAIBlipPedBlipIDX(s32 iPlayerIndex)
{
	return CScriptPedAIBlips::GetAIBlipPedBlipIDX(iPlayerIndex);	
}

s32 CommandsGetAIBlipVehicleBlipIDX(s32 iPlayerIndex)
{
	return CScriptPedAIBlips::GetAIBlipVehicleBlipIDX(iPlayerIndex);
}

void CommandSetPMWarningScreenActive(bool bActive)
{
	CPauseMenu::SetPauseMenuWarningScreenActive(bActive);
}

bool CommandHasDirectorModeBeenLaunchedByCode()
{
#if GTA_REPLAY
	return CVideoEditorUi::HasDirectorModeBeenLaunched();
#else
	return false;
#endif // GTA_REPLAY
}

void CommandSetDirectorModeLaunchedByScript()
{
#if GTA_REPLAY
	CVideoEditorUi::ClearDirectorMode();
#endif // GTA_REPLAY
}

void CommandSetPlayerIsInDirectorMode(bool bIsInDirectorMode)
{
	CTheScripts::SetIsInDirectorMode(bIsInDirectorMode);
}

void CommandSetDirectorModeAvailable(bool const c_active)
{
	CTheScripts::SetDirectorModeAvailable(c_active);
}

//=================================================================
//HUD Markers

SHudMarkerState* GetHudMarkerState(s32 iMarkerId)
{
	if (auto HudMarkerManager = CHudMarkerManager::Get())
	{
		return HudMarkerManager->GetMarkerState(HudMarkerID::FromScript(iMarkerId));
	}
	return nullptr;
}

SHudMarkerGroup* GetHudMarkerGroup(s32 iGroupId)
{
	if (auto HudMarkerManager = CHudMarkerManager::Get())
	{
		return HudMarkerManager->GetMarkerGroup(HudMarkerGroupID::FromScript(iGroupId));
	}
	return nullptr;
}

void CommandHideHudMarkersThisFrame()
{
	if (auto HudMarkerManager = CHudMarkerManager::Get())
	{
		HudMarkerManager->SetHideThisFrame();
	}
}

bool CommandIsHudMarkerValid(s32 iMarkerId)
{
	return GetHudMarkerState(iMarkerId) != nullptr;
}

s32 CommandAddHudMarker(const scrVector& vWorldPos, s32 iHudColor)
{
	if (auto HudMarkerManager = CHudMarkerManager::Get())
	{
		SHudMarkerState InitState;
		InitState.WorldPosition = vWorldPos;
		InitState.Color.Set(CHudColour::GetIntColour((eHUD_COLOURS)iHudColor));
		return HudMarkerManager->CreateMarker(InitState).ToScript();
	}
	uiWarningf("CommandAddHudMarker: Failed to add marker vWorldPos=[%.2f,%.2f,%.2f] iHudColor=[%d]", vWorldPos.x, vWorldPos.y, vWorldPos.z, iHudColor);
	return HudMarkerID();
}

s32 CommandAddHudMarkerForBlip(s32 iBlipId)
{
	if (auto HudMarkerManager = CHudMarkerManager::Get())
	{
		return HudMarkerManager->CreateMarkerForBlip(iBlipId).ToScript();
	}
	uiWarningf("CommandAddHudMarkerForBlip: Failed to add marker for iBlipId=[%d]", iBlipId);
	return HudMarkerID();
}

bool CommandRemoveHudMarker(s32& iMarkerId)
{
	if (auto HudMarkerManager = CHudMarkerManager::Get())
	{
		HudMarkerID ID = HudMarkerID::FromScript(iMarkerId);
		if (HudMarkerManager->RemoveMarker(ID))
		{
			iMarkerId = ID;
		}
	}
	uiWarningf("CommandRemoveHudMarker: Failed to remove iMarkerId=[%d]", iMarkerId);
	return false;
}

void CommandSetHudMarkerGroup(s32 iMarkerId, s32 iGroupId)
{
	if (auto State = GetHudMarkerState(iMarkerId))
	{
		State->GroupID = HudMarkerGroupID::FromScript(iGroupId);
		return;
	}
	uiWarningf("CommandSetHudMarkerGroup: Failed to set iMarkerId=[%d] iGroupId=[%d]", iMarkerId, iGroupId);
}

void CommandSetHudMarkerPosition(s32 iMarkerId, const scrVector& vWorldPos)
{
	if (auto State = GetHudMarkerState(iMarkerId))
	{
		State->WorldPosition = vWorldPos;
		return;
	}
	uiWarningf("CommandSetHudMarkerPosition: Failed to set iMarkerId=[%d] vWorldPos=[%.2f,%.2f,%.2f]", iMarkerId, vWorldPos.x, vWorldPos.y, vWorldPos.z);
}

void CommandSetHudMarkerHeightOffset(s32 iMarkerId, const float fHeightOffset)
{
	if (auto State = GetHudMarkerState(iMarkerId))
	{
		State->WorldHeightOffset = fHeightOffset;
		return;
	}
	uiWarningf("CommandSetHudMarkerHeightOffset: Failed to set iMarkerId=[%d] fHeightOffset=[%.2f]", iMarkerId, fHeightOffset);
}

void CommandSetHudMarkerColour(s32 iMarkerId,  s32 iHudColor)
{
	if (auto State = GetHudMarkerState(iMarkerId))
	{
		State->Color.Set(CHudColour::GetIntColour((eHUD_COLOURS)iHudColor));
		return;
	}
	uiWarningf("CommandSetHudMarkerColour: Failed to set iMarkerId=[%d] iHudColor=[%d]", iMarkerId, iHudColor);
}

void CommandSetHudMarkerIcon(s32 iMarkerId, s32 iIconIndex)
{
	if (auto State = GetHudMarkerState(iMarkerId))
	{
		State->IconIndex = iIconIndex;
		return;
	}
	uiWarningf("CommandSetHudMarkerIcon: Failed to set iMarkerId=[%d] iIconIndex=[%d]", iMarkerId, iIconIndex);
}

void CommandSetHudMarkerClampEnabled(s32 iMarkerId, bool bClampEnabled)
{
	if (auto State = GetHudMarkerState(iMarkerId))
	{
		State->ClampEnabled = bClampEnabled;
		return;
	}
	uiWarningf("CommandSetHudMarkerClampEnabled: Failed to set iMarkerId=[%d] bClampEnabled=[%d]", iMarkerId, bClampEnabled);
}

void CommandSetHudMarkerGroupMaxClampNum(s32 iGroupId, s32 iMaxClampNum)
{
	if (auto Group = GetHudMarkerGroup(iGroupId))
	{
		Group->MaxClampNum = iMaxClampNum;
		return;
	}
	uiWarningf("CommandSetHudMarkerGroupMaxClampNum: Failed to set iGroupId=[%d] iMaxClampNum=[%d]", iGroupId, iMaxClampNum);
}

void CommandSetHudMarkerGroupForceVisibleNum(s32 iGroupId, s32 iForceVisibleNum)
{
	if (auto Group = GetHudMarkerGroup(iGroupId))
	{
		Group->ForceVisibleNum = iForceVisibleNum;
		return;
	}
	uiWarningf("CommandSetHudMarkerGroupForceVisibleNum: Failed to set iGroupId=[%d] iForceVisibleNum=[%d]", iGroupId, iForceVisibleNum);
}

void CommandSetHudMarkerGroupMaxVisible(s32 iGroupId, s32 iMaxVisible)
{
	if (auto Group = GetHudMarkerGroup(iGroupId))
	{
		Group->MaxVisible = iMaxVisible;
		return;
	}
	uiWarningf("CommandSetHudMarkerGroupMaxVisible: Failed to set iGroupId=[%d] iMaxVisible=[%d]", iGroupId, iMaxVisible);
}

void CommandSetHudMarkerGroupMinDistance(s32 iGroupId, float fMinDistance)
{
	if (auto Group = GetHudMarkerGroup(iGroupId))
	{
		Group->MinDistance = fMinDistance;
		return;
	}
	uiWarningf("CommandSetHudMarkerGroupMinDistance: Failed to set iGroupId=[%d] fMinDistance=[%f]", iGroupId, fMinDistance);
}

void CommandSetHudMarkerGroupMaxDistance(s32 iGroupId, float fMaxDistance)
{
	if (auto Group = GetHudMarkerGroup(iGroupId))
	{
		Group->MaxDistance = fMaxDistance;
		return;
	}
	uiWarningf("CommandSetHudMarkerGroupMaxDistance: Failed to set iGroupId=[%d] fMaxDistance=[%f]", iGroupId, fMaxDistance);
}

void CommandSetHudMarkerGroupMinTextDistance(s32 iGroupId, float fMinTextDistance)
{
	if (auto Group = GetHudMarkerGroup(iGroupId))
	{
		Group->MinTextDistance = fMinTextDistance;
		return;
	}
	uiWarningf("CommandSetHudMarkerGroupMaxDistance: Failed to set iGroupId=[%d] fMinTextDistance=[%f]", iGroupId, fMinTextDistance);
}

void CommandSetHudMarkerGroupIsSolo(s32 iGroupId, bool bIsSolo)
{
	if (auto Group = GetHudMarkerGroup(iGroupId))
	{
		Group->IsSolo = bIsSolo;
		return;
	}
	uiWarningf("CommandSetHudMarkerGroupIsSolo: Failed to set iGroupId=[%d] bIsSolo=[%d]", iGroupId, (int)bIsSolo);
}

void CommandSetHudMarkerGroupIsMuted(s32 iGroupId, bool bIsMuted)
{
	if (auto Group = GetHudMarkerGroup(iGroupId))
	{
		Group->IsMuted = bIsMuted;
		return;
	}
	uiWarningf("CommandSetHudMarkerGroupIsMuted: Failed to set iGroupId=[%d] bIsMuted=[%d]", iGroupId, (int)bIsMuted);
}

void CommandSetHudMarkerAlwaysShowDistanceTextForClosest(s32 iGroupId, bool bAlwaysShowDistanceTextForClosest)
{
	if (auto Group = GetHudMarkerGroup(iGroupId))
	{
		Group->AlwaysShowDistanceTextForClosest = bAlwaysShowDistanceTextForClosest;
		return;
	}
	uiWarningf("CommandSetHudMarkerAlwaysShowDistanceTextForClosest: Failed to set iGroupId=[%d] bAlwaysShowDistanceTextForClosest=[%d]", iGroupId, (int)bAlwaysShowDistanceTextForClosest);
}

//=================================================================

//
// name:		SetupScriptCommands
// description:	Setup streaming script commands
void SetupScriptCommands()
{
	SCR_REGISTER_UNUSED(ARE_INSTRUCTIONAL_BUTTONS_ACTIVE,0xe3dc5fd3cfbc54f4, AreInstructionalButtonsActive);

	// Busy spinner
	SCR_REGISTER_SECURE(BEGIN_TEXT_COMMAND_BUSYSPINNER_ON,0x65f03588435fe39b, CommandBeginBusySpinnerOn);
	SCR_REGISTER_SECURE(END_TEXT_COMMAND_BUSYSPINNER_ON,0xa22cde353f373a10, CommandEndBusySpinnerOn);
	SCR_REGISTER_SECURE(BUSYSPINNER_OFF,0xbe3ac6d6682e4a5e, CommandBusySpinnerOff);
	SCR_REGISTER_SECURE(PRELOAD_BUSYSPINNER,0xdb18c83ed1b45a43, CommandPreloadBusySpinner);
	SCR_REGISTER_SECURE(BUSYSPINNER_IS_ON,0xabf37173278f2c66, CommandIsBusySpinnerOn);
	SCR_REGISTER_SECURE(BUSYSPINNER_IS_DISPLAYING,0xf2cab66b1eeaf469, CommandIsBusySpinnerDisplaying);
	SCR_REGISTER_SECURE(DISABLE_PAUSEMENU_SPINNER,0x66a7bcfc9dcf9410, CommandDisablePauseMenuBusySpinner);


	// mousepointer commands:
	SCR_REGISTER_SECURE(SET_MOUSE_CURSOR_THIS_FRAME,0x60236500251fde8e, CommandSetMouseCursorThisFrame);
	SCR_REGISTER_SECURE(SET_MOUSE_CURSOR_STYLE,0x147141484022751b, CommandSetMouseCursorStyle);
	SCR_REGISTER_SECURE(SET_MOUSE_CURSOR_VISIBLE,0x284537aa10dee2ed, CommandSetMouseCursorVisibility);
	SCR_REGISTER_SECURE(IS_MOUSE_ROLLED_OVER_INSTRUCTIONAL_BUTTONS,0xff792ddc17afa777, CommandIsMouseRolledOverInstructionalButtons);
	SCR_REGISTER_SECURE(GET_MOUSE_EVENT,0x64a9da79c6fa2378, CommandGetMouseEvent);
	
	// Extra feed commands
	SCR_REGISTER_SECURE(THEFEED_ONLY_SHOW_TOOLTIPS,0x000323ba1a15023e, CommandTheFeedOnlyShowToolTips);
	SCR_REGISTER_SECURE(THEFEED_SET_SCRIPTED_MENU_HEIGHT,0x7ed668fc4cb0f4c4, CommandSetScriptedMenuHeight);
	SCR_REGISTER_SECURE(THEFEED_HIDE,0xedd332b8622a1aaa, CommandTheFeedHide);
	SCR_REGISTER_SECURE(THEFEED_HIDE_THIS_FRAME,0xf43060df31acea55, CommandTheFeedHideThisFrame);
	SCR_REGISTER_SECURE(THEFEED_SHOW,0xc8483c4c42152921, CommandTheFeedShow);
	SCR_REGISTER_SECURE(THEFEED_FLUSH_QUEUE,0x1b7aaa85d46fe6fb, CommandTheFeedFlushQueue);
	SCR_REGISTER_SECURE(THEFEED_REMOVE_ITEM,0x35a0954fc4dc4cad, CommandTheFeedRemoveItem);
	SCR_REGISTER_SECURE(THEFEED_FORCE_RENDER_ON,0xf8937c8d901dd3e3, CommandTheFeedForceRenderOn);
	SCR_REGISTER_SECURE(THEFEED_FORCE_RENDER_OFF,0x7ed9c25cd67a47b4, CommandTheFeedForceRenderOff);
	SCR_REGISTER_SECURE(THEFEED_PAUSE,0xa240f4c910b19938, CommandTheFeedPause);
	SCR_REGISTER_SECURE(THEFEED_RESUME,0x45227777d3ebece5, CommandTheFeedResume);
	SCR_REGISTER_SECURE(THEFEED_IS_PAUSED,0xd73fbb13d109c7e5, CommandTheFeedIsPaused);
	SCR_REGISTER_SECURE(THEFEED_REPORT_LOGO_ON,0xaf6b61516f451e7b, CommandTheFeedReportLogoOn);
	SCR_REGISTER_SECURE(THEFEED_REPORT_LOGO_OFF,0x033c89f75a484139, CommandTheFeedReportLogoOff);
	SCR_REGISTER_UNUSED(THEFEED_SET_GAMER1_INFO,0xe6a266c47725eb47, CommandTheFeedSetGamer1Info);
	SCR_REGISTER_UNUSED(THEFEED_SET_GAMER2_INFO,0xaeb6aaf75158e40a, CommandTheFeedSetGamer2Info);
	SCR_REGISTER_SECURE(THEFEED_GET_LAST_SHOWN_PHONE_ACTIVATABLE_FEED_ID,0x361847535f5edc1b, CommandGetLastShownPhoneActivatableFeedID);

    SCR_REGISTER_UNUSED(THEFEED_SET_RGBA_PARAMETER,0xad838fe60889939d, CommandTheFeedSetRGBAParameter);
    SCR_REGISTER_UNUSED(THEFEED_SET_FLASH_DURATION_PARAMETER,0xe9ceb47bb9f08cb6, CommandTheFeedSetFlashDurationParameter);
    SCR_REGISTER_UNUSED(THEFEED_RESET_RGBA_FLASHRATE_PARAMETERS,0xcd8580404dc7b784, CommandTheFeedResetRGBAFlashParameters);
	SCR_REGISTER_SECURE(THEFEED_AUTO_POST_GAMETIPS_ON,0xb1f47b55abffc2c6, CommandAutoPostGameTipsOn);
	SCR_REGISTER_SECURE(THEFEED_AUTO_POST_GAMETIPS_OFF,0x80b433623a323832, CommandAutoPostGameTipsOff);

	SCR_REGISTER_SECURE(THEFEED_SET_BACKGROUND_COLOR_FOR_NEXT_POST,0xa5d2a61128a52bee, CommandTheFeedSetBackgroundColorForNextPost);
	SCR_REGISTER_SECURE(THEFEED_SET_RGBA_PARAMETER_FOR_NEXT_MESSAGE,0xde1e80cd298714f6, CommandTheFeedSetRGBAForNextMessage);
	SCR_REGISTER_SECURE(THEFEED_SET_FLASH_DURATION_PARAMETER_FOR_NEXT_MESSAGE,0x6b4fdbc0c48138eb, CommandTheFeedSetFlashDurationForNextMessage);
	SCR_REGISTER_SECURE(THEFEED_SET_VIBRATE_PARAMETER_FOR_NEXT_MESSAGE,0x41b7a61cd05e09f7, CommandTheFeedSetVibrateForNextMessage);
	SCR_REGISTER_SECURE(THEFEED_RESET_ALL_PARAMETERS,0xcafd40d2e1c5badf, CommandTheFeedResetAllParameters);

	SCR_REGISTER_SECURE(THEFEED_FREEZE_NEXT_POST,0xa3a13cfc0759da78, CommandTheFeedFreezeNextPost);
	SCR_REGISTER_SECURE(THEFEED_CLEAR_FROZEN_POST,0x4d6d7401e41088cc, CommandTheFeedClearFrozenPost);

	SCR_REGISTER_SECURE(THEFEED_SET_SNAP_FEED_ITEM_POSITIONS,0x5ef96ccb73988e08, CommandTheFeedSetSnapFeedItemPositions);	

	SCR_REGISTER_SECURE(THEFEED_UPDATE_ITEM_TEXTURE,0x8bc4d76163fa9e13, CommandUpdateFeedItemTexture);

	// Message commands
	SCR_REGISTER_SECURE(BEGIN_TEXT_COMMAND_THEFEED_POST,0x150d8f58b26e9f70, CommandBeginTheFeedPost);
	SCR_REGISTER_SECURE(END_TEXT_COMMAND_THEFEED_POST_STATS,0x4d190c4e8cafbed1, CommandEndTheFeedPostStats);
	SCR_REGISTER_SECURE(END_TEXT_COMMAND_THEFEED_POST_MESSAGETEXT,0x3b81b9627e7885cf, CommandEndTheFeedPostMessageText);
	SCR_REGISTER_SECURE(END_TEXT_COMMAND_THEFEED_POST_MESSAGETEXT_SUBTITLE_LABEL,0x197e7b3a20bf59ad, CommandEndTheFeedPostMessageTextSubtitleLabel);
	SCR_REGISTER_SECURE(END_TEXT_COMMAND_THEFEED_POST_MESSAGETEXT_TU,0x450880c8bbf08892, CommandEndTheFeedPostMessageTextTU);
	SCR_REGISTER_SECURE(END_TEXT_COMMAND_THEFEED_POST_MESSAGETEXT_WITH_CREW_TAG,0xe29f3dff64c18dae, CommandEndTheFeedPostMessageTextWithCrewTag);
	SCR_REGISTER_SECURE(END_TEXT_COMMAND_THEFEED_POST_MESSAGETEXT_WITH_CREW_TAG_AND_ADDITIONAL_ICON,0xe7018078e5cde830, CommandEndTheFeedPostMessageTextWithCrewTagAndAdditionalIcon);
	SCR_REGISTER_SECURE(END_TEXT_COMMAND_THEFEED_POST_TICKER,0x187df98ed95e5557, CommandEndTheFeedPostTicker);
	SCR_REGISTER_SECURE(END_TEXT_COMMAND_THEFEED_POST_TICKER_FORCED,0xf5b7761e0022ee4c, CommandEndTheFeedPostTickerForced);
	SCR_REGISTER_SECURE(END_TEXT_COMMAND_THEFEED_POST_TICKER_WITH_TOKENS,0x0b47c28802142e86, CommandEndTheFeedPostTickerWithTokens);
	SCR_REGISTER_UNUSED(END_TEXT_COMMAND_THEFEED_POST_TICKER_F10,0x6e8a43b68a3a516e, CommandEndTheFeedPostTickerF10);
	SCR_REGISTER_SECURE(END_TEXT_COMMAND_THEFEED_POST_AWARD,0x5e6cc764bdafa84b, CommandEndTheFeedPostAward);
	SCR_REGISTER_SECURE(END_TEXT_COMMAND_THEFEED_POST_CREWTAG,0xbc04c06c5ebc8bd5, CommandEndTheFeedPostCrewTag);
	SCR_REGISTER_SECURE(END_TEXT_COMMAND_THEFEED_POST_CREWTAG_WITH_GAME_NAME,0x2fe2977f89f2c159, CommandEndTheFeedPostCrewTagWithGameName );
	SCR_REGISTER_SECURE(END_TEXT_COMMAND_THEFEED_POST_UNLOCK,0x13436442fa22e816, CommandEndTheFeedPostUnlock);
	SCR_REGISTER_SECURE(END_TEXT_COMMAND_THEFEED_POST_UNLOCK_TU,0xb36438590d294199, CommandEndTheFeedPostUnlockTU);
	SCR_REGISTER_SECURE(END_TEXT_COMMAND_THEFEED_POST_UNLOCK_TU_WITH_COLOR,0x213ec586d6b81324, CommandEndTheFeedPostUnlockTUWithColor);

	SCR_REGISTER_SECURE(END_TEXT_COMMAND_THEFEED_POST_MPTICKER,0x14fe8aefbebb7595, CommandEndTheFeedPostMpTicker);
	SCR_REGISTER_UNUSED(END_TEXT_COMMAND_THEFEED_POST_CREW_RANKUP,0xc412e0634ae3c3ab, CommandEndTheFeedPostCrewRankup);
	SCR_REGISTER_SECURE(END_TEXT_COMMAND_THEFEED_POST_CREW_RANKUP_WITH_LITERAL_FLAG,0x5de7fbbfcccf8a9b, CommandEndTheFeedPostCrewRankupWithLiteralFlag);
	SCR_REGISTER_SECURE(END_TEXT_COMMAND_THEFEED_POST_VERSUS_TU,0x6e98512bacab301c, CommandEndTheFeedPostVersus);
	SCR_REGISTER_SECURE(END_TEXT_COMMAND_THEFEED_POST_REPLAY,0xe815b0e0beb78386, CommandEndTheFeedPostReplay);
	SCR_REGISTER_SECURE(END_TEXT_COMMAND_THEFEED_POST_REPLAY_INPUT,0xc400185efbad9333, CommandEndTheFeedPostReplayInput);
	

	SCR_REGISTER_SECURE(BEGIN_TEXT_COMMAND_PRINT,0x38bd019deccc5482, CommandBeginTextCommandPrint);
	SCR_REGISTER_SECURE(END_TEXT_COMMAND_PRINT,0x23d3ee043de19c4b, CommandEndTextCommandPrint);
	SCR_REGISTER_SECURE(BEGIN_TEXT_COMMAND_IS_MESSAGE_DISPLAYED,0x09684fd679b0e474, CommandBeginTextCommandIsMessageDisplayed);
	SCR_REGISTER_SECURE(END_TEXT_COMMAND_IS_MESSAGE_DISPLAYED,0xd7aa4c82b5d00977, CommandEndTextCommandIsMessageDisplayed);

	SCR_REGISTER_SECURE(BEGIN_TEXT_COMMAND_DISPLAY_TEXT,0xcdda0c58b818f6b2, CommandBeginTextCommandDisplayText);
	SCR_REGISTER_SECURE(END_TEXT_COMMAND_DISPLAY_TEXT,0x1a53079994915fa6, CommandEndTextCommandDisplayText);
	SCR_REGISTER_UNUSED(END_TEXT_COMMAND_DISPLAY_DEBUG_TEXT,0x20270a24691cf680, CommandEndTextCommandDisplayDebugText);

	SCR_REGISTER_SECURE(BEGIN_TEXT_COMMAND_GET_SCREEN_WIDTH_OF_DISPLAY_TEXT,0x27ed46ea48c0a455, CommandBeginTextCommandGetScreenWidthOfDisplayText);
	SCR_REGISTER_SECURE(END_TEXT_COMMAND_GET_SCREEN_WIDTH_OF_DISPLAY_TEXT,0x63f498818b4dee3e, CommandEndTextCommandGetScreenWidthOfDisplayText);
	SCR_REGISTER_SECURE(BEGIN_TEXT_COMMAND_GET_NUMBER_OF_LINES_FOR_STRING,0xb733c0853476f0c1, CommandBeginTextCommandGetNumberOfLinesForString);
	SCR_REGISTER_SECURE(END_TEXT_COMMAND_GET_NUMBER_OF_LINES_FOR_STRING,0xbd7dcbc4f2df91f8, CommandEndTextCommandGetNumberOfLinesForString);

	SCR_REGISTER_SECURE(BEGIN_TEXT_COMMAND_DISPLAY_HELP,0xa83426f7ceded182, CommandBeginTextCommandDisplayHelp);
	SCR_REGISTER_SECURE(END_TEXT_COMMAND_DISPLAY_HELP,0x89b545a74f93b1dd, CommandEndTextCommandDisplayHelp);
	SCR_REGISTER_SECURE(BEGIN_TEXT_COMMAND_IS_THIS_HELP_MESSAGE_BEING_DISPLAYED,0xb65782d82090bb33, CommandBeginTextCommandIsThisHelpMessageBeingDisplayed);
	SCR_REGISTER_SECURE(END_TEXT_COMMAND_IS_THIS_HELP_MESSAGE_BEING_DISPLAYED,0x73cb3406a86a3a02, CommandEndTextCommandIsThisHelpMessageBeingDisplayed);

	SCR_REGISTER_UNUSED(BEGIN_TEXT_COMMAND_PLAYER_NAME_ROW,0xefbb7fbba7e8187b, CommandBeginTextCommandPlayerNameRow);
	SCR_REGISTER_UNUSED(END_TEXT_COMMAND_PLAYER_NAME_ROW,0x6d6a2cf78bbc0899, CommandEndTextCommandPlayerNameRow);

	SCR_REGISTER_SECURE(BEGIN_TEXT_COMMAND_SET_BLIP_NAME,0xb6ad9f9931d821ca, CommandBeginTextCommandSetBlipName);
	SCR_REGISTER_SECURE(END_TEXT_COMMAND_SET_BLIP_NAME,0xa630bf119308f2f4, CommandEndTextCommandSetBlipName);

	SCR_REGISTER_SECURE(BEGIN_TEXT_COMMAND_ADD_DIRECTLY_TO_PREVIOUS_BRIEFS,0xfa61eba0db93d000, CommandBeginTextCommandAddDirectlyToPreviousBriefs);
	SCR_REGISTER_SECURE(END_TEXT_COMMAND_ADD_DIRECTLY_TO_PREVIOUS_BRIEFS,0x33c7d05305878ca8, CommandEndTextCommandAddDirectlyToPreviousBriefs);

	SCR_REGISTER_SECURE(BEGIN_TEXT_COMMAND_CLEAR_PRINT,0x1419d7b938cbff2e, CommandBeginTextCommandClearPrint);
	SCR_REGISTER_SECURE(END_TEXT_COMMAND_CLEAR_PRINT,0xf4666ba5b36dbcb2, CommandEndTextCommandClearPrint);

	SCR_REGISTER_SECURE(BEGIN_TEXT_COMMAND_OVERRIDE_BUTTON_TEXT,0xf496bf691df1252b, CommandBeginTextCommandOverrideButtonText);
	SCR_REGISTER_SECURE(  END_TEXT_COMMAND_OVERRIDE_BUTTON_TEXT,0xa01b659e856f50c8, CommandEndTextCommandOverrideButtonText);

	SCR_REGISTER_SECURE(ADD_TEXT_COMPONENT_INTEGER,0x2ae954ba77a66307, CommandAddTextComponentInteger);
	SCR_REGISTER_SECURE(ADD_TEXT_COMPONENT_FLOAT,0x957e514a6e999374, CommandAddTextComponentFloat);
	SCR_REGISTER_SECURE(ADD_TEXT_COMPONENT_SUBSTRING_TEXT_LABEL,0x26c23be14f66f202, CommandAddTextComponentSubStringTextLabel);
	SCR_REGISTER_SECURE(ADD_TEXT_COMPONENT_SUBSTRING_TEXT_LABEL_HASH_KEY,0x5ce9b15c197df898, CommandAddTextComponentSubStringTextLabelHashKey);
	SCR_REGISTER_UNUSED(ADD_TEXT_COMPONENT_SUBSTRING_LITERAL_STRING,0x019a7e0bf2a3d979, CommandAddTextComponentSubStringLiteralString);
	SCR_REGISTER_SECURE(ADD_TEXT_COMPONENT_SUBSTRING_BLIP_NAME,0x26e6a0f02144bed3, CommandAddTextComponentSubStringBlipName);
	SCR_REGISTER_SECURE(ADD_TEXT_COMPONENT_SUBSTRING_PLAYER_NAME,0x76db21247ae4e4e2, CommandAddTextComponentSubStringPlayerName);
	SCR_REGISTER_UNUSED(ADD_TEXT_COMPONENT_SUBSTRING_IP_ADDRESS,0x2b1066417767ccc9, CommandAddTextComponentSubStringIpAddress);
	SCR_REGISTER_UNUSED(ADD_TEXT_COMPONENT_SUBSTRING_PASSWORD,0x4dd41b503d2c2524, CommandAddTextComponentSubStringPassword);
	SCR_REGISTER_SECURE(ADD_TEXT_COMPONENT_SUBSTRING_TIME,0x28594d0d61db1278, CommandAddTextComponentSubstringTime);
	SCR_REGISTER_SECURE(ADD_TEXT_COMPONENT_FORMATTED_INTEGER,0x3788872a07ba04b3, CommandAddTextComponentFormattedInteger);
	SCR_REGISTER_SECURE(ADD_TEXT_COMPONENT_SUBSTRING_PHONE_NUMBER,0xfccaeceb5e052fbe, CommandAddTextComponentSubstringPhoneNumber);
	SCR_REGISTER_SECURE(ADD_TEXT_COMPONENT_SUBSTRING_WEBSITE,0x93843c22c86afb2e, CommandAddTextComponentSubstringWebsite);
	SCR_REGISTER_SECURE(ADD_TEXT_COMPONENT_SUBSTRING_KEYBOARD_DISPLAY,0x702b349761f63a9e, CommandAddTextComponentSubstringKeyboardDisplay);
	SCR_REGISTER_SECURE(SET_COLOUR_OF_NEXT_TEXT_COMPONENT,0x138506d6c7564ef1, CommandSetColourOfNextTextComponent);

	SCR_REGISTER_UNUSED(GET_EXPECTED_NUMBER_OF_TEXT_COMPONENTS,0xdeacccb2bcd655a9, CommandGetExpectedNumberOfTextComponents);

//	The following two commands are deliberately hooked up to pre-existing functions
//	GET_STRING_FROM_STRING and GET_STRING_FROM_TEXT_FILE will soon only be available in Debug scripts
//	but Steve still needs this functionality in dialogue_handler.sc
	SCR_REGISTER_SECURE(GET_CHARACTER_FROM_AUDIO_CONVERSATION_FILENAME,0x6abf9c6f18308b78, CommandGetStringFromString);
	SCR_REGISTER_SECURE(GET_CHARACTER_FROM_AUDIO_CONVERSATION_FILENAME_WITH_BYTE_LIMIT,0x8bb54beea2e17ec8, CommandGetStringFromStringWithByteLimit);
	SCR_REGISTER_SECURE(GET_CHARACTER_FROM_AUDIO_CONVERSATION_FILENAME_BYTES,0xc98697a0990910c1, CommandGetStringFromStringBytes);
	SCR_REGISTER_SECURE(GET_FILENAME_FOR_AUDIO_CONVERSATION,0xaef70623d03f7b02, CommandGetStringFromTextFile);


	SCR_REGISTER_UNUSED(PRINT,0x84e5be8eec43facf, CommandPrint);
	SCR_REGISTER_UNUSED(PRINT_NOW,0x1042cef6534dd91d, CommandPrintNow);
	SCR_REGISTER_SECURE(CLEAR_PRINTS,0xba6c3c5e9e5a9442, CommandClearPrints);
	SCR_REGISTER_SECURE(CLEAR_BRIEF,0xe2c4655d0361beac, CommandClearBrief);
	SCR_REGISTER_SECURE(CLEAR_ALL_HELP_MESSAGES,0x687e1afe919b49bd, CommandClearAllHelpMessages);
	SCR_REGISTER_UNUSED(PRINT_WITH_NUMBER,0x26dbf60753df5219, CommandPrintWithNumber);
	SCR_REGISTER_UNUSED(PRINT_WITH_NUMBER_NOW,0x2b100196f80abeef, CommandPrintWithNumberNow);
	SCR_REGISTER_UNUSED(PRINT_WITH_2_NUMBERS,0xc920c727cc40f23b, CommandPrintWith2Numbers);
	SCR_REGISTER_UNUSED(PRINT_WITH_2_NUMBERS_NOW,0x456e3873846c9dc0, CommandPrintWith2NumbersNow);
	SCR_REGISTER_UNUSED(PRINT_STRING_IN_STRING,0x5941dc3e879dd622, CommandPrintStringInString);
	SCR_REGISTER_UNUSED(PRINT_STRING_IN_STRING_NOW,0x3f35ebd8a5e7371d, CommandPrintStringInStringNow);
	SCR_REGISTER_UNUSED(PRINT_STRING_WITH_TWO_STRINGS,0x851fb92d20714610, CommandPrintStringWithTwoStrings);

	SCR_REGISTER_UNUSED(PRINT_STRING_WITH_LITERAL_STRING,0x0a4fc8ce65e08b71, CommandPrintStringWithLiteralString);
	SCR_REGISTER_UNUSED(PRINT_STRING_WITH_LITERAL_STRING_NOW,0xc711cbf26ea21e4c, CommandPrintStringWithLiteralStringNow);
	SCR_REGISTER_UNUSED(PRINT_STRING_WITH_TWO_LITERAL_STRINGS,0x724d17b284b2e6ed, CommandPrintStringWithTwoLiteralStrings);
	SCR_REGISTER_UNUSED(PRINT_STRING_WITH_TWO_LITERAL_STRINGS_NOW,0xace56cb3563ffba4, CommandPrintStringWithTwoLiteralStringsNow);

	SCR_REGISTER_SECURE(CLEAR_THIS_PRINT,0xe2ec74d02a707b37, CommandClearThisPrint);
	SCR_REGISTER_SECURE(CLEAR_SMALL_PRINTS,0xd016d3608079eee3, CommandClearSmallPrints);
	SCR_REGISTER_UNUSED(IS_THIS_PRINT_BEING_DISPLAYED,0xc6c99315eaeb73f3, CommandIsThisPrintBeingDisplayed);

	SCR_REGISTER_SECURE(DOES_TEXT_BLOCK_EXIST,0x7b32afe3b3f7d5eb, CommandDoesTextBlockExist);
	SCR_REGISTER_SECURE(REQUEST_ADDITIONAL_TEXT,0x249a0d3c5714bcf4, CommandRequestAdditionalText);
	SCR_REGISTER_SECURE(REQUEST_ADDITIONAL_TEXT_FOR_DLC,0xc5bf49fef70ebf12, CommandRequestAdditionalTextForDLC);
	SCR_REGISTER_SECURE(HAS_ADDITIONAL_TEXT_LOADED,0x01896b71c5ac966e, CommandHasAdditionalTextLoaded);
	SCR_REGISTER_SECURE(CLEAR_ADDITIONAL_TEXT,0xc511e7c272201cf7, CommandClearAdditionalText);
	SCR_REGISTER_SECURE(IS_STREAMING_ADDITIONAL_TEXT,0xf00d2317ec791d94, CommandIsStreamingAdditionalText);
	SCR_REGISTER_SECURE(HAS_THIS_ADDITIONAL_TEXT_LOADED,0x5fabfb823fd821d4, CommandHasThisAdditionalTextLoaded);
	SCR_REGISTER_SECURE(IS_MESSAGE_BEING_DISPLAYED,0x559c03f53e6937fc, CommandIsMessageBeingDisplayed);
	SCR_REGISTER_SECURE(DOES_TEXT_LABEL_EXIST,0xe73671e3d37cf79e, CommandDoesTextLabelExist);
	SCR_REGISTER_UNUSED(GET_STRING_FROM_TEXT_FILE,0xab9b3315100b5cc6, CommandGetStringFromTextFile);
	SCR_REGISTER_UNUSED(GET_FIRST_N_CHARACTERS_OF_STRING,0x0887602ddb4965a0, CommandGetFirstNCharactersOfString);
	SCR_REGISTER_SECURE(GET_FIRST_N_CHARACTERS_OF_LITERAL_STRING,0x9c0b44284bae32ce, CommandGetFirstNCharactersOfLiteralString);
	SCR_REGISTER_SECURE(GET_LENGTH_OF_STRING_WITH_THIS_TEXT_LABEL,0xd3b1139766362a75, CommandGetLengthOfStringWithThisTextLabel);
	SCR_REGISTER_UNUSED(GET_STRING_FROM_STRING,0x8ca1cb8c4be69967, CommandGetStringFromString);
	SCR_REGISTER_SECURE(GET_LENGTH_OF_LITERAL_STRING,0xee91150b7f06c500, CommandGetLengthOfLiteralString);
	SCR_REGISTER_SECURE(GET_LENGTH_OF_LITERAL_STRING_IN_BYTES,0x4b83ffc4b00987d9, CommandGetLengthOfLiteralStringInBytes);
	SCR_REGISTER_UNUSED(GET_STRING_FROM_HASH_KEY,0xeca456d3ea4c26c7, CommandGetStringFromHashKey);
	SCR_REGISTER_SECURE(GET_STREET_NAME_FROM_HASH_KEY,0xfa3d01be2a9bfe4f, CommandGetStringFromHashKey);
	SCR_REGISTER_UNUSED(GET_NTH_INTEGER_IN_STRING,0x9486d801f2787584, CommandGetNthIntegerInString);
	SCR_REGISTER_SECURE(IS_HUD_PREFERENCE_SWITCHED_ON,0x130b8ac038bc42bd, CommandIsHudPreferenceSwitchedOn);
	SCR_REGISTER_SECURE(IS_RADAR_PREFERENCE_SWITCHED_ON,0x368afb9d49d819fe, CommandIsRadarPreferenceSwitchedOn);
	SCR_REGISTER_SECURE(IS_SUBTITLE_PREFERENCE_SWITCHED_ON,0x0038ca9239065bca, CommandIsSubtitlePreferenceSwitchedOn);
	SCR_REGISTER_SECURE(DISPLAY_HUD,0x72db022c36fcea24, CommandDisplayHud);
	SCR_REGISTER_SECURE(DISPLAY_HUD_WHEN_NOT_IN_STATE_OF_PLAY_THIS_FRAME,0x9ecd85a9dab06cf5, CommandDisplayHudWhenNotInStateOfPlayThisFrame);
	SCR_REGISTER_SECURE(DISPLAY_HUD_WHEN_PAUSED_THIS_FRAME,0x470a86bed022c62c, CommandDisplayHudWhenPausedThisFrame);
	SCR_REGISTER_SECURE(DISPLAY_RADAR,0xb60709bd0e075903, CommandDisplayRadar);
	SCR_REGISTER_SECURE(SET_FAKE_SPECTATOR_MODE,0xa1b263c28d444317, CommandSetFakeSpectatorMode);
	SCR_REGISTER_SECURE(GET_FAKE_SPECTATOR_MODE,0x788404226972208f, CommandGetFakeSpectatorMode);
	SCR_REGISTER_SECURE(IS_HUD_HIDDEN,0x9c0ec6d1b2fd2795, CommandIsHudHidden);
	SCR_REGISTER_SECURE(IS_RADAR_HIDDEN,0xb7b9fcc926422b4b, CommandIsRadarHidden);
	SCR_REGISTER_SECURE(IS_MINIMAP_RENDERING,0xf78361b5b167bc18, CommandIsMiniMapRendering);
	SCR_REGISTER_UNUSED(SET_ROUTE_FLASHES, 0x93bd3e80, CommandSetRouteFlashing);
	SCR_REGISTER_UNUSED(FORCE_RETICLE_MODE,0xfab6d4f9eb15e697, CommandForceReticleMode);
	SCR_REGISTER_SECURE(USE_VEHICLE_TARGETING_RETICULE,0x01d81b27347ddf48, CommandSetUseVehicleTargetingReticule);
	SCR_REGISTER_SECURE(ADD_VALID_VEHICLE_HIT_HASH,0x64e297fbc1b775f2, CommandAddValidVehicleHitHash);
	SCR_REGISTER_SECURE(CLEAR_VALID_VEHICLE_HIT_HASHES,0x705ecaf58aa98795, CommandClearValidVehicleHitHashes);
	SCR_REGISTER_SECURE(SET_BLIP_ROUTE,0xaad76b24a5282fdd, CommandSetRoute);
	SCR_REGISTER_SECURE(CLEAR_ALL_BLIP_ROUTES,0x48b0037539974c2f, CommandClearAllBlipRoutes);
	SCR_REGISTER_SECURE(SET_BLIP_ROUTE_COLOUR,0xda6437ea059f152a, CommandSetBlipRouteColour);
	SCR_REGISTER_SECURE(SET_FORCE_SHOW_GPS,0x21d57e4bdb028ccb, CommandSetForceShowGPS);
	SCR_REGISTER_SECURE(SET_USE_SET_DESTINATION_IN_PAUSE_MAP,0x1ca1e8c07c447958, CommandUseSetDestinationInPauseMap);
	SCR_REGISTER_SECURE(SET_BLOCK_WANTED_FLASH,0x09162f5d5428a5d8, CommandSetBlockWantedFlash);
	SCR_REGISTER_SECURE(ADD_NEXT_MESSAGE_TO_PREVIOUS_BRIEFS,0x4976ab7d95c4657e, CommandAddNextMessageToPreviousBriefs);
	SCR_REGISTER_SECURE(FORCE_NEXT_MESSAGE_TO_PREVIOUS_BRIEFS_LIST,0xb19edf7c1655975b, CommandForceNextMessageToPreviousBriefsList);
	SCR_REGISTER_SECURE(SET_RADAR_ZOOM_PRECISE,0x93d95f240bbb4805, CommandSetRadarZoomPrecise);
	SCR_REGISTER_SECURE(SET_RADAR_ZOOM,0xcd6c63a108b12434, CommandSetRadarZoom);
	SCR_REGISTER_SECURE(SET_RADAR_ZOOM_TO_BLIP,0xd786ff78f1c0208c, CommandSetRadarZoomToBlip);
	SCR_REGISTER_SECURE(SET_RADAR_ZOOM_TO_DISTANCE,0xc391630fc1d4f7d4, CommandSetRadarZoomToDistance);
	SCR_REGISTER_SECURE(UPDATE_RADAR_ZOOM_TO_BLIP,0x553a61b953cd77f2, CommandUpdateRadarZoomToBlip);
	SCR_REGISTER_SECURE(GET_HUD_COLOUR,0x2f046c9381d8e91b, CommandGetHudColour);
	SCR_REGISTER_SECURE(SET_SCRIPT_VARIABLE_HUD_COLOUR,0xb84f6e7b50d72dda, CommandSetScriptVariableHudColour);
	SCR_REGISTER_SECURE(SET_SECOND_SCRIPT_VARIABLE_HUD_COLOUR,0xe1b27b622cd5ef09, CommandSetSecondScriptVariableHudColour);
	SCR_REGISTER_SECURE(REPLACE_HUD_COLOUR,0x7eed971b613c9387, CommandReplaceHudColour);
	SCR_REGISTER_SECURE(REPLACE_HUD_COLOUR_WITH_RGBA,0x969b16b605d0d249, CommandReplaceHudColourWithRGBA);
	SCR_REGISTER_SECURE(SET_ABILITY_BAR_VISIBILITY,0x7af3ff08d316f95c, CommandSetAbilityBarVisibility);
	SCR_REGISTER_SECURE(SET_ALLOW_ABILITY_BAR,0xb5146be6389fe092, CommandAllowAbilityBar);
	SCR_REGISTER_SECURE(FLASH_ABILITY_BAR,0xd5f181065e8e4832, CommandFlashAbilityBar);
	SCR_REGISTER_UNUSED(SET_ABILITY_BAR_GLOW,0x6d9ab055fa93278e, CommandSetAbilityBarGlow);
	SCR_REGISTER_SECURE(SET_ABILITY_BAR_VALUE,0xe7ab8b0870fbeb3f, CommandSetAbilityBarValue);
	SCR_REGISTER_SECURE(FLASH_WANTED_DISPLAY,0x6e99c8b17122ed29, CommandFlashWantedDisplay);
	SCR_REGISTER_SECURE(FORCE_OFF_WANTED_STAR_FLASH,0xb563c2f3fae04d24, CommandbForceOffWantedStarFlash);
	SCR_REGISTER_UNUSED(FORCE_ON_WANTED_STAR_FLASH,0x562107706ce3e550, CommandForceOnWantedStarFlash);
	SCR_REGISTER_UNUSED(UPDATE_WANTED_DRAIN_LEVEL,0xe43e54b6d0a69812, CommandUpdateWantedDrainLevel);
	SCR_REGISTER_UNUSED(UPDATE_WANTED_THREAT_VISIBILITY,0xa6f2854b04b1277c, CommandbUpdateWantedThreatVisibility);
	SCR_REGISTER_UNUSED(SET_HUD_DISPLAY_MODE,0xd49a49608f7d75bc, CommandSetHudDisplayMode);

	SCR_REGISTER_SECURE(SET_CUSTOM_MP_HUD_COLOR,0x2accb195f3ccd9de, CommandSetCustomMPHudColor);
	SCR_REGISTER_UNUSED(GET_CUSTOM_MP_HUD_COLOR,0x879bedc40ff9a105, CommandGetCustomMPHudColor);

	SCR_REGISTER_UNUSED(SET_TICK_COLOR_OVERRIDE, 0x1817a17ad054e2af, CommandSetTickColorOverride);

	//	Text commands
	SCR_REGISTER_UNUSED(GET_RENDERED_TEXT_PADDING_SIZE,0x60343bb12e4c469a, CommandGetRenderedTextPaddingSize);
	SCR_REGISTER_SECURE(GET_RENDERED_CHARACTER_HEIGHT,0xc23444e9b1b8d081, CommandGetRenderedCharacterHeight);
	SCR_REGISTER_UNUSED(GET_STRING_WIDTH,0xf83e288091642eda, CommandGetStringWidth);
	SCR_REGISTER_UNUSED(GET_STRING_WIDTH_WITH_NUMBER,0x16b948d9adbf532e, CommandGetStringWidthWithNumber);
	SCR_REGISTER_UNUSED(GET_STRING_WIDTH_WITH_STRING,0xd2154323bf886ec2, CommandGetStringWidthWithString);
	SCR_REGISTER_UNUSED(GET_NUMBER_LINES_WITH_SUBSTRINGS, 0xd172184d, CommandGetNumberLinesWithSubstrings);
	SCR_REGISTER_UNUSED(DISPLAY_TEXT,0x92596f9448f7ce53, CommandDisplayText);
	SCR_REGISTER_SECURE(SET_TEXT_SCALE,0x3f03c2d4efa888bc, CommandSetTextScale);
	SCR_REGISTER_SECURE(SET_TEXT_COLOUR,0x71f4002cb1a0b451, CommandSetTextColour);
	SCR_REGISTER_UNUSED(SET_TEXT_HUD_COLOUR,0xc9be98a81f921a0e, CommandSetTextHudColour);

	SCR_REGISTER_UNUSED(SET_TEXT_JUSTIFY,0x95e056a1e98bb41e, CommandSetTextJustify);
	SCR_REGISTER_SECURE(SET_TEXT_CENTRE,0x21fc15ae6b6188c4, CommandSetTextCentre);
	SCR_REGISTER_SECURE(SET_TEXT_RIGHT_JUSTIFY,0xb9a0cf201cf801f2, CommandSetTextRightJustify);
	SCR_REGISTER_SECURE(SET_TEXT_JUSTIFICATION,0xb8306da8a5d18c52, CommandSetTextJustification);

	SCR_REGISTER_UNUSED(SET_TEXT_TO_USE_TEXT_FILE_COLOURS,0xf0b90def1f81d9df, CommandSetTextToUseTextFileColours);
	SCR_REGISTER_UNUSED(SET_TEXT_LINE_HEIGHT_MULT,0x5e5a9ce59af800eb, CommandSetTextLineHeightMult);
	SCR_REGISTER_SECURE(SET_TEXT_WRAP,0xe835f806be49c67b, CommandSetTextWrap);
	SCR_REGISTER_UNUSED(SET_TEXT_BACKGROUND,0x16c632c2cbbce4d0, CommandSetTextBackground);
	SCR_REGISTER_SECURE(SET_TEXT_LEADING,0x28ec5961fd3b75f0, CommandSetTextLeading);
	SCR_REGISTER_UNUSED(SET_TEXT_USE_UNDERSCORE,0x4d5d279aed0e8745, CommandSetTextUseUnderScore);
	SCR_REGISTER_SECURE(SET_TEXT_PROPORTIONAL,0x0365ab5b6db2f6e3, CommandSetTextProportional);
	SCR_REGISTER_UNUSED(LOAD_TEXT_FONT,0xd5d9531c1640e0bd, CommandLoadTextFont);
	SCR_REGISTER_UNUSED(IS_TEXT_FONT_LOADED,0x6828b4b08d3d122d, CommandIsFontLoaded);
	SCR_REGISTER_SECURE(SET_TEXT_FONT,0xf68e5437af17efbc, CommandSetTextFont);
	SCR_REGISTER_UNUSED(DISPLAY_TEXT_WITH_NUMBER,0x750e1c400d0153e4, CommandDisplayTextWithNumber);
	SCR_REGISTER_UNUSED(DISPLAY_TEXT_WITH_2_NUMBERS,0x9f0146ff310f374a, CommandDisplayTextWith2Numbers);
	SCR_REGISTER_UNUSED(DISPLAY_TEXT_WITH_3_NUMBERS,0x95eb052eafb24267, CommandDisplayTextWith3Numbers);
	SCR_REGISTER_SECURE(SET_TEXT_DROP_SHADOW,0xb4143d1df367ec1b, CommandSetTextDropShadow);
	SCR_REGISTER_SECURE(SET_TEXT_DROPSHADOW,0xe570b77d940667ff, CommandSetTextDropshadowOld);
	SCR_REGISTER_UNUSED(DISPLAY_TEXT_WITH_FLOAT,0xaf54de6252981c3d, CommandDisplayTextWithFloat);
	SCR_REGISTER_UNUSED(DISPLAY_TEXT_WITH_LITERAL_STRING,0xede77e6d5a45b373, CommandDisplayTextWithLiteralString);
	SCR_REGISTER_UNUSED(DISPLAY_TEXT_WITH_TWO_LITERAL_STRINGS,0x358c6ea1d24c7e80, CommandDisplayTextWithTwoLiteralStrings);
	SCR_REGISTER_UNUSED(DISPLAY_TEXT_WITH_STRING,0x8a7be0a77de5b1c8, CommandDisplayTextWithString);
	SCR_REGISTER_UNUSED(DISPLAY_TEXT_WITH_TWO_STRINGS,0xd4cda4fff11a2669, CommandDisplayTextWithTwoStrings);
	SCR_REGISTER_UNUSED(DISPLAY_TEXT_WITH_BLIP_NAME,0xbb6e64d23c80ac52, CommandDisplayTextWithBlipName);

	SCR_REGISTER_SECURE(SET_TEXT_OUTLINE,0x435b084ae3274e4c, CommandSetTextOutline);
	SCR_REGISTER_SECURE(SET_TEXT_EDGE,0xb133f9d28dfd4e1e, CommandSetTextEdge);
	SCR_REGISTER_SECURE(SET_TEXT_RENDER_ID,0xcf0e3bb302eac0ea, CommandSetTextRenderId);
	SCR_REGISTER_SECURE(GET_DEFAULT_SCRIPT_RENDERTARGET_RENDER_ID,0x56555a9ed8b80dfd, CommandGetDefaultScriptRenderTargetRenderId);
	SCR_REGISTER_UNUSED(GET_SCRIPT_RENDERTARGET_RENDER_ID,0x95ada3bc66b34dc0, CommandGetScriptRenderTargetRenderId);
	
	SCR_REGISTER_SECURE(REGISTER_NAMED_RENDERTARGET,0x7f31e4cdb9fb2193, CommandRegisterNamedRendertarget);
	SCR_REGISTER_SECURE(IS_NAMED_RENDERTARGET_REGISTERED,0xbb75dcb31b62483c, CommmandIsNamedRendertargetRegistered);
	SCR_REGISTER_SECURE(RELEASE_NAMED_RENDERTARGET,0xefd883943e59207a, CommandReleaseNamedRendertarget);
	SCR_REGISTER_SECURE(LINK_NAMED_RENDERTARGET,0xc043f1f3cf279111, CommandLinkNamedRenderTarget);
	SCR_REGISTER_SECURE(GET_NAMED_RENDERTARGET_RENDER_ID,0x014679e62446327a, CommandGetNamedRenderTargetid);
	SCR_REGISTER_UNUSED(SETUP_DELAYEDLOAD_NAMED_RENDERTARGET,0x85064dcb837190d9, CommandSetupNamedRendertargetDelayLoad);
	SCR_REGISTER_SECURE(IS_NAMED_RENDERTARGET_LINKED,0x593c5e295b4590f4, CommmandIsNamedRendertargetLinked);

	//	Help Message commands
	SCR_REGISTER_UNUSED(PRINT_HELP,0xc53a58c89fffbd5c, CommandPrintHelp);
	SCR_REGISTER_UNUSED(PRINT_HELP_WITH_NUMBER,0x80e201ef5d381369, CommandPrintHelpWithNumber);
	SCR_REGISTER_UNUSED(PRINT_HELP_WITH_STRING,0x392b2b4770f6823d, CommandPrintHelpWithString);
	SCR_REGISTER_UNUSED(PRINT_HELP_WITH_STRING_NO_SOUND,0xea29b0e8bad8bd65, CommandPrintHelpWithStringNoSound);
	SCR_REGISTER_SECURE(CLEAR_HELP,0x3410421c60bf7143, CommandClearHelp);
	SCR_REGISTER_UNUSED(PRINT_HELP_FOREVER,0x415e19d6a1931d42, CommandPrintHelpForever);
	SCR_REGISTER_UNUSED(PRINT_HELP_FOREVER_WITH_NUMBER,0x1b68caadad6a080b, CommandPrintHelpForeverWithNumber);
	SCR_REGISTER_UNUSED(PRINT_HELP_FOREVER_WITH_STRING,0xd63d527e046277d2, CommandPrintHelpForeverWithString);
	SCR_REGISTER_UNUSED(PRINT_HELP_FOREVER_WITH_STRING_NO_SOUND,0xe8267a5ee3ff4311, CommandPrintHelpForeverWithStringNoSound);

	SCR_REGISTER_UNUSED(PRINT_HELP_WITH_STRING_AND_NUMBER,0x60874373a3d5f108, CommandPrintHelpWithStringAndNumber);
	SCR_REGISTER_UNUSED(PRINT_HELP_FOREVER_WITH_STRING_AND_NUMBER,0x83c1ab9c1e783b82, CommandPrintHelpForeverWithStringAndNumber);

	SCR_REGISTER_SECURE(IS_HELP_MESSAGE_ON_SCREEN,0x2d3aaabb780ed9b6, CommandIsHelpMessageOnScreen);
	SCR_REGISTER_SECURE(HAS_SCRIPT_HIDDEN_HELP_THIS_FRAME,0x697282ecbfd7b449, CommandHasScriptHiddenHelpThisFrame);
	SCR_REGISTER_SECURE(IS_HELP_MESSAGE_BEING_DISPLAYED,0xf847447d4467709d, CommandIsHelpMessageBeingDisplayed);
	SCR_REGISTER_SECURE(IS_HELP_MESSAGE_FADING_OUT,0xd2fc6b4fe564a470, CommandIsHelpMessageFadingOut);
	SCR_REGISTER_UNUSED(SET_HELP_MESSAGE_BOX_SIZE,0xbfb1d3fcd71baa44, CommandSetHelpMessageBoxSize);
	SCR_REGISTER_UNUSED(SET_HELP_MESSAGE_SCREEN_POSITION,0x00ce23d7fa4b9fff, CommandSetHelpMessageScreenPosition);
	SCR_REGISTER_UNUSED(SET_HELP_MESSAGE_WORLD_POSITION,0x50bd7a0828dc3829, CommandSetHelpMessageWorldPosition);
	SCR_REGISTER_SECURE(SET_HELP_MESSAGE_STYLE,0xad8bf87dddacf427, CommandSetHelpMessageStyle);
	SCR_REGISTER_UNUSED(IS_THIS_HELP_MESSAGE_BEING_DISPLAYED,0x9c07a1673146c156, CommandIsThisHelpMessageBeingDisplayed);
	SCR_REGISTER_UNUSED(IS_THIS_HELP_MESSAGE_WITH_NUMBER_BEING_DISPLAYED,0x756682794550425e, CommandIsThisHelpMessageWithNumberBeingDisplayed);
	SCR_REGISTER_UNUSED(IS_THIS_HELP_MESSAGE_WITH_STRING_BEING_DISPLAYED,0x59122f8b2ba3ba26, CommandIsThisHelpMessageWithStringBeingDisplayed);
	SCR_REGISTER_UNUSED(IS_THIS_HELP_MESSAGE_WITH_STRING_AND_NUMBER_BEING_DISPLAYED,0x3481e86636caf8a5, CommandIsThisHelpMessageWithStringAndNumberBeingDisplayed);
	SCR_REGISTER_UNUSED(DISPLAY_NON_MINIGAME_HELP_MESSAGES,0xce4d057354d100e0, CommandDisplayNonMinigameHelpMessages);
	SCR_REGISTER_UNUSED(DOES_THIS_MINIGAME_SCRIPT_ALLOW_NON_MINIGAME_HELP_MESSAGES,0xaad5f59ffc251db3, CommandDoesThisMiniGameScriptAllowNonMinigameHelpMessages);

	SCR_REGISTER_UNUSED(ADD_TO_PREVIOUS_BRIEF,0x808472b723e22dbb, CommandAddToPreviousBrief);
	SCR_REGISTER_UNUSED(ADD_TO_PREVIOUS_BRIEF_WITH_UNDERSCORE,0x4e7f4185560241a7, CommandAddToPreviousBriefWithUnderscore);

	// radar blip command
	SCR_REGISTER_SECURE(GET_STANDARD_BLIP_ENUM_ID,0x7b826893b2e27385, GetStandardBlipEnumId); 
	SCR_REGISTER_SECURE(GET_WAYPOINT_BLIP_ENUM_ID,0x19f6152d9806bbf8, GetWaypointBlipEnumId); 
	SCR_REGISTER_SECURE(GET_NUMBER_OF_ACTIVE_BLIPS,0x5a6048c9bd40c5db, GetNumberOfActiveBlips); 
	SCR_REGISTER_SECURE(GET_NEXT_BLIP_INFO_ID,0xa529c1d8769201e5, GetNextBlipInfoId); 
	SCR_REGISTER_SECURE(GET_FIRST_BLIP_INFO_ID,0xfd3a7cd556889d1d, GetFirstBlipInfoId);
	SCR_REGISTER_SECURE(GET_CLOSEST_BLIP_INFO_ID,0x5ebd67d5f4953c98, GetClosestBlipInfoId);
	SCR_REGISTER_SECURE(GET_BLIP_INFO_ID_COORD,0x9a497500ddc52c34, GetBlipInfoIdCoord);
	SCR_REGISTER_SECURE(GET_BLIP_INFO_ID_DISPLAY,0x5185d043bd203744, GetBlipInfoIdDisplay);
	SCR_REGISTER_SECURE(GET_BLIP_INFO_ID_TYPE,0x0096542092b7c49b, GetBlipInfoIdType);
	SCR_REGISTER_SECURE(GET_BLIP_INFO_ID_ENTITY_INDEX,0xd806a0199657b31f, GetBlipInfoIdEntityIndex);
	SCR_REGISTER_SECURE(GET_BLIP_INFO_ID_PICKUP_INDEX,0xda4fd237416e009f, GetBlipInfoIdPickupIndex);
	SCR_REGISTER_SECURE(GET_BLIP_FROM_ENTITY,0x47b4a2f6a146c0bb, GetBlipFromEntity);
	

	SCR_REGISTER_UNUSED(ADD_STEALTH_BLIP_FOR_PED,0x629f899c72f98088, AddStealthBlipForPed);
	SCR_REGISTER_UNUSED(ADD_BLIP_FOR_GANG_TERRITORY,0x0b67e0670567170f, AddBlipForGangTerritory);
	SCR_REGISTER_SECURE(ADD_BLIP_FOR_RADIUS,0x434cc3c60683b171, AddBlipForRadius);
	SCR_REGISTER_SECURE(ADD_BLIP_FOR_AREA,0x4828d2fe7319bea7, AddBlipForArea);
	SCR_REGISTER_UNUSED(ADD_BLIP_FOR_FAKE_WANTED_RADIUS,0xfa7cf5ee31751135, AddBlipForFakeWantedRadius);
	SCR_REGISTER_SECURE(ADD_BLIP_FOR_ENTITY,0xefd6451bf0f3352f, AddBlipForEntity);
	SCR_REGISTER_SECURE(ADD_BLIP_FOR_PICKUP,0x455b7ffc90053189, AddBlipForPickup);
	SCR_REGISTER_SECURE(ADD_BLIP_FOR_COORD,0xc5b823372b67998a, AddBlipForCoord);
	SCR_REGISTER_SECURE(TRIGGER_SONAR_BLIP,0x6e2ddd7fa8b9b61e, CommandTriggerSonarBlip);
	SCR_REGISTER_SECURE(ALLOW_SONAR_BLIPS,0xd56db2802e4bf824, CommandAllowSonarBlips);

	SCR_REGISTER_SECURE(SET_BLIP_COORDS,0xfb7acc9d9d6401a8, CommandSetBlipCoords);
	SCR_REGISTER_SECURE(GET_BLIP_COORDS,0x1a3931a61b3223a2, CommandGetBlipCoords);
	SCR_REGISTER_UNUSED(ADD_BLIP_FOR_CONTACT,0x1ef7a71f36624bf5, AddBlipForContactPoint);
	SCR_REGISTER_SECURE(SET_BLIP_SPRITE,0x1a5b5ba56167d412, CommandSetBlipSprite);
	SCR_REGISTER_SECURE(GET_BLIP_SPRITE,0xf33cd92713fc4e3a, GetBlipSprite);
	SCR_REGISTER_SECURE(SET_COP_BLIP_SPRITE,0xba517a1bcae9f455, CommandSetCopBlipSprite);
	SCR_REGISTER_SECURE(SET_COP_BLIP_SPRITE_AS_STANDARD,0x37664f5f7b801b72, CommandSetCopBlipSpriteAsStandard);
	SCR_REGISTER_UNUSED(GET_BLIP_NAME,0x5701b290ef2286a2, GetBlipName);
	SCR_REGISTER_SECURE(SET_BLIP_NAME_FROM_TEXT_FILE,0xaf62582f3ea39095, ChangeBlipNameFromTextFile);
	SCR_REGISTER_UNUSED(SET_BLIP_NAME_FROM_ASCII,0xd8a05a427401160f, ChangeBlipNameFromAscii);
	SCR_REGISTER_SECURE(SET_BLIP_NAME_TO_PLAYER_NAME,0xd7e3027fdc6e8da5, CommandSetBlipNameToPlayerName);
	SCR_REGISTER_SECURE(SET_BLIP_ALPHA,0xfbbd8f124403b0f5, ChangeBlipAlpha);
	SCR_REGISTER_SECURE(GET_BLIP_ALPHA,0x6babdf7a460158ce, CommandGetBlipAlpha);
	SCR_REGISTER_SECURE(SET_BLIP_FADE,0x7105eaf252d92508, CommandSetBlipFade);
	SCR_REGISTER_SECURE(GET_BLIP_FADE_DIRECTION,0x0b14bbb374a35daa, CommandGetBlipFadeDirection);
	SCR_REGISTER_SECURE(SET_BLIP_ROTATION,0xb02954d4f69b7a25, CommandSetBlipRotation);
	SCR_REGISTER_SECURE(SET_BLIP_ROTATION_WITH_FLOAT,0x0f32d83531f90470, CommandSetBlipRotationFloat);
	
	SCR_REGISTER_SECURE(GET_BLIP_ROTATION,0xb08b076e80441a59, CommandGetBlipRotation);
	SCR_REGISTER_SECURE(SET_BLIP_FLASH_TIMER,0xe5d4469dccfb5170, ChangeBlipFlashTimer);
	SCR_REGISTER_SECURE(SET_BLIP_FLASH_INTERVAL,0x1209f9274aff1a3d, ChangeBlipFlashInterval);
	SCR_REGISTER_SECURE(SET_BLIP_COLOUR,0xa582ee8380437b1b, ChangeBlipColour);
	SCR_REGISTER_SECURE(SET_BLIP_SECONDARY_COLOUR,0x6557afc30f6af5b1, ChangeBlipSecondaryColour);
	SCR_REGISTER_SECURE(GET_BLIP_COLOUR,0xca3605134e97db2b, CommandGetBlipColour);
	SCR_REGISTER_SECURE(GET_BLIP_HUD_COLOUR,0xe8fcd9a6754a5fb2, CommandGetBlipHudColour);
	SCR_REGISTER_SECURE(IS_BLIP_SHORT_RANGE,0xc35f1f30893ad957, CommandIsBlipShortRange);
	SCR_REGISTER_SECURE(IS_BLIP_ON_MINIMAP,0xc12ac7d5e9c1afea, CommandIsBlipOnMiniMap);
	SCR_REGISTER_SECURE(DOES_BLIP_HAVE_GPS_ROUTE,0x76774fc82d7f9e46, CommandDoesBlipHaveGpsRoute);
	SCR_REGISTER_SECURE(SET_BLIP_HIDDEN_ON_LEGEND,0x2ae77dbcbf631065, CommandSetBlipHiddenOnLegend);
	SCR_REGISTER_UNUSED(IS_BLIP_HIDDEN_ON_LEGEND,0xa0e3754a0dae8431, CommandIsBlipHiddenOnLegend);
	SCR_REGISTER_SECURE(SET_BLIP_HIGH_DETAIL,0x2f82f997df899c00, CommandSetBlipHighDetail);
	SCR_REGISTER_SECURE(SET_BLIP_AS_MISSION_CREATOR_BLIP,0x9f66168013d830e4, CommandSetBlipAsMissionCreatorBlip);
	SCR_REGISTER_SECURE(IS_MISSION_CREATOR_BLIP,0xd0064e66bc157bb6, CommandIsMissionCreatorBlip);
	SCR_REGISTER_SECURE(GET_NEW_SELECTED_MISSION_CREATOR_BLIP,0x71b3290c410aea94, CommandGetNewSelectedMissionCreatorBlip);
	SCR_REGISTER_SECURE(IS_HOVERING_OVER_MISSION_CREATOR_BLIP,0x0617568957d6ca6c, CommandIsHoveringOverMissionCreatorBlip);
	SCR_REGISTER_SECURE(SHOW_START_MISSION_INSTRUCTIONAL_BUTTON,0x5d95f05c8c06ac86, CommandShowStartMissionInstructionalButton);
	SCR_REGISTER_SECURE(SHOW_CONTACT_INSTRUCTIONAL_BUTTON,0xc772a904cde1186f, CommandShowContactInstructionalButton);
	SCR_REGISTER_SECURE(RELOAD_MAP_MENU,0x7400fb520dc2da92, CommandReloadMapMenu);
	SCR_REGISTER_SECURE(SET_BLIP_MARKER_LONG_DISTANCE,0x0fb22292b765bcfa, CommandSetBlipMarkerLongDistance);
	SCR_REGISTER_SECURE(SET_BLIP_FLASHES,0x9705014c37e60003, CommandSetBlipFlashes);
	SCR_REGISTER_SECURE(SET_BLIP_FLASHES_ALTERNATE,0xeab0f1c2ac4270fc, CommandSetBlipFlashesAlternate);
	SCR_REGISTER_SECURE(IS_BLIP_FLASHING,0x7962b3383c030376, CommandIsBlipFlashing);
	SCR_REGISTER_SECURE(SET_BLIP_AS_SHORT_RANGE,0xa241a7085a493f20, CommandSetBlipAsShortRange);
	SCR_REGISTER_UNUSED(SET_TERRITORY_BLIP_SCALE,0x76b361295a8f0971, CommandSetTerritoryBlipScale);
	SCR_REGISTER_SECURE(SET_BLIP_SCALE,0x293a9399e902a33b, CommandChangeBlipScale);
	SCR_REGISTER_SECURE(SET_BLIP_SCALE_2D,0xb3c7ad8bf7e5d5e4, CommandChangeBlipScale2D);
	SCR_REGISTER_SECURE(SET_BLIP_PRIORITY,0x7a610b2ec5da34e7, CommandChangeBlipPriority);
	SCR_REGISTER_SECURE(SET_BLIP_DISPLAY,0x94c2f928b167aa54, CommandChangeBlipDisplay);
	SCR_REGISTER_SECURE(SET_BLIP_CATEGORY,0x29c63b19a4798623, CommandChangeBlipCategory);
	SCR_REGISTER_SECURE(REMOVE_BLIP,0xffd8eb5462b07b59, CommandRemoveBlip);
	SCR_REGISTER_SECURE(SET_BLIP_AS_FRIENDLY,0xd809204f14ef9b68, CommandSetBlipAsFriendly);
	SCR_REGISTER_SECURE(PULSE_BLIP,0x1fe0f42cece5c3e0, CommandPulseBlip);
	SCR_REGISTER_UNUSED(SET_BLIP_AS_DEAD,0x877ac124146a370b, CommandSetBlipAsDead);
	SCR_REGISTER_SECURE(SHOW_NUMBER_ON_BLIP,0x21cd92bdc21b6453, CommandShowNumberOnBlip);
	SCR_REGISTER_SECURE(HIDE_NUMBER_ON_BLIP,0x2c5c132762d25e92, CommandHideNumberOnBlip);
	SCR_REGISTER_SECURE(SHOW_HEIGHT_ON_BLIP,0x3430966ac4550bb9, CommandShowHeightOnBlip);
	SCR_REGISTER_SECURE(SHOW_TICK_ON_BLIP,0x5baabf9faede4d96, CommandShowTickOnBlip);
	SCR_REGISTER_SECURE(SHOW_GOLD_TICK_ON_BLIP, 0xcac2031ebf79b1a8, CommandShowGoldTickOnBlip);
	SCR_REGISTER_UNUSED(SHOW_FOR_SALE_ICON_ON_BLIP,0xd6d9ffd3b3882737, CommandShowForSaleIconOnBlip);
	SCR_REGISTER_SECURE(SHOW_HEADING_INDICATOR_ON_BLIP,0xf68844669fc3f9aa, CommandShowHeadingIndicatorOnBlip);
	SCR_REGISTER_SECURE(SHOW_OUTLINE_INDICATOR_ON_BLIP,0x4d89ef15b620f774, CommandShowOutlineIndicatorOnBlip);
	SCR_REGISTER_SECURE(SHOW_FRIEND_INDICATOR_ON_BLIP,0x300e57267e7c5abd, CommandShowFriendIndicatorOnBlip);
	SCR_REGISTER_SECURE(SHOW_CREW_INDICATOR_ON_BLIP,0x3e802d5892e92bc3, CommandShowCrewIndicatorOnBlip);
	SCR_REGISTER_SECURE(SET_BLIP_EXTENDED_HEIGHT_THRESHOLD,0x19805abde5c3f525, CommandSetBlipExtendedHeightThreshold);
	SCR_REGISTER_SECURE(SET_BLIP_SHORT_HEIGHT_THRESHOLD,0x49b02aa1a8b546e2, CommandSetBlipShortHeightThreshold);
	SCR_REGISTER_SECURE(SET_BLIP_USE_HEIGHT_INDICATOR_ON_EDGE,0x997989c3a8dc14d9, CommandSetBlipUseHeightIndicatorOnEdge);
	SCR_REGISTER_SECURE(SET_BLIP_AS_MINIMAL_ON_EDGE,0xffe087414d6aca04, CommandSetBlipAsMinimalOnEdge);
	SCR_REGISTER_UNUSED(SET_AREA_BLIP_EDGE,0x64b11f185c6e9ef8, CommandSetAreaBlipEdge);
	SCR_REGISTER_SECURE(SET_RADIUS_BLIP_EDGE,0x28f396dd9b791e03, CommandSetRadiusBlipEdge);
	SCR_REGISTER_SECURE(DOES_BLIP_EXIST,0x12dd4a0b247d9b4d, CommandDoesBlipExist);
	SCR_REGISTER_UNUSED(SET_WAYPOINT_GPS_FLAGS,0x593b300966e66cd6, CommandSetWaypoinGpsFlags);
	SCR_REGISTER_SECURE(SET_WAYPOINT_OFF,0x0ce8f4161957de76, CommandSetWaypointOff);
	SCR_REGISTER_SECURE(DELETE_WAYPOINTS_FROM_THIS_PLAYER,0xca9aaacb147ff121, CommandDeleteWaypointsFromThisPlayer);
	SCR_REGISTER_SECURE(REFRESH_WAYPOINT,0x73207668365cdade, CommandRefreshWaypoint);
	SCR_REGISTER_SECURE(IS_WAYPOINT_ACTIVE,0x309a7e82d2306f1d, CommandIsWaypointActive);
	SCR_REGISTER_UNUSED(IS_WAYPOINT_ACTIVE_FOR_THIS_PLAYER,0xe8f41ec8d0741028, CommandIsWaypointActiveForThisPlayer);
	SCR_REGISTER_SECURE(SET_NEW_WAYPOINT,0x6b075f00044133ea, CommandSetNewWaypoint);
	SCR_REGISTER_UNUSED(IS_ENTITY_WAYPOINT_ALLOWED,0x3a273994fdae27ab, CommandIsEntityWaypointAllowed);
	SCR_REGISTER_UNUSED(SET_ENTITY_WAYPOINT_ALLOWED,0x9ee01dd505d0e186, CommandSetEntityWaypointAllowed);
	SCR_REGISTER_UNUSED(GET_WAYPOINT_CLEAR_ON_ARRIVAL_MODE,0xa5b0d908dc9bf2b0, CommandGetWaypointClearOnArrivalMode);
	SCR_REGISTER_UNUSED(SET_WAYPOINT_CLEAR_ON_ARRIVAL_MODE,0x7765eb5e928b70fc, CommandSetWaypointClearOnArrivalMode);
	SCR_REGISTER_SECURE(SET_BLIP_BRIGHT,0x842b2d20fdd53978, CommandSetBlipBright);
	SCR_REGISTER_SECURE(SET_BLIP_SHOW_CONE,0xff833b22ce9ebabd, CommandSetBlipShowCone);
	SCR_REGISTER_SECURE(REMOVE_COP_BLIP_FROM_PED,0xe38ee0041b752011, CommandRemoveCopBlipFromPed);
	SCR_REGISTER_SECURE(SETUP_FAKE_CONE_DATA,0xeec25a2d0e80a33d, CommandSetupFakeConeData);
	SCR_REGISTER_SECURE(REMOVE_FAKE_CONE_DATA,0xa65547a7512713de, CommandDestroyFakeConeData);
	SCR_REGISTER_SECURE(CLEAR_FAKE_CONE_ARRAY,0x498a1c47f5c9a109, CommandClearFakeConeArray);

	SCR_REGISTER_UNUSED(SET_BLIP_DEBUG_NUMBER,0x587091ad2555a49d, CommandSetBlipDebugNumber);
	SCR_REGISTER_UNUSED(SET_BLIP_DEBUG_NUMBER_OFF,0x46f49532877e9ee2, CommandSetBlipDebugNumberOff);

	SCR_REGISTER_SECURE(SET_MINIMAP_COMPONENT,0xf4a39430885655c8, CommandSetMiniMapComponent);

	SCR_REGISTER_UNUSED(SET_MP_PROPERTY_OWNER,0x3f9df0c437e2e31a, CommandSetMPPropertyOwner);
	SCR_REGISTER_UNUSED(SET_MINIMAP_YOKE,0x8ec4f142ec3651b0, CommandShowMinimapYoke);
	SCR_REGISTER_SECURE(SET_MINIMAP_SONAR_SWEEP,0x66bfb53a5f30bf64, CommandShowMinimapSonarSweep);

	SCR_REGISTER_SECURE(SHOW_ACCOUNT_PICKER,0x076c7a1c039099c5, CommandShowAccountPicker);
	
	SCR_REGISTER_SECURE(GET_MAIN_PLAYER_BLIP_ID,0x74248a89dfcaa77f, CommandGetMainPlayerBlipId);
	
	SCR_REGISTER_SECURE(SET_PM_WARNINGSCREEN_ACTIVE,0x892af48f0da9a598, CommandSetPMWarningScreenActive);

	SCR_REGISTER_SECURE(HIDE_LOADING_ON_FADE_THIS_FRAME,0x33f5772842f16e82, CommandHideLoadingOnFadeThisFrame);
	SCR_REGISTER_SECURE(SET_RADAR_AS_INTERIOR_THIS_FRAME,0x7886a9e2dc3cf65b, CommandSetRadarAsInteriorThisFrame);
	SCR_REGISTER_SECURE(SET_INSIDE_VERY_SMALL_INTERIOR,0x5183d88df0899683, CommandsSetInsideVerySmallInterior);
	SCR_REGISTER_SECURE(SET_INSIDE_VERY_LARGE_INTERIOR,0xee765c06d6b4cda1, CommandsSetInsideVeryLargeInterior);
	SCR_REGISTER_SECURE(SET_RADAR_AS_EXTERIOR_THIS_FRAME,0xa6ce32374156efa0, CommandSetRadarAsExteriorThisFrame);
	
    SCR_REGISTER_SECURE(SET_FAKE_PAUSEMAP_PLAYER_POSITION_THIS_FRAME,0x9c929b838435db54, CommandSetFakePauseMapPlayerPositionThisFrame);
    SCR_REGISTER_SECURE(SET_FAKE_GPS_PLAYER_POSITION_THIS_FRAME,0xc8813dfdeca7bb27, CommandSetFakeGPSPlayerPositionThisFrame);
	SCR_REGISTER_UNUSED(SET_FAKE_PAUSEMAP_PLAYER_POSITION_THIS_INTERIOR,0x3bdb8d353133b790, CommandSetFakePauseMapPlayerPositionThisInterior);

	SCR_REGISTER_SECURE(IS_PAUSEMAP_IN_INTERIOR_MODE,0x77a41b07d1f0a831, CommandIsPauseMapInInteriorMode);
	SCR_REGISTER_SECURE(HIDE_MINIMAP_EXTERIOR_MAP_THIS_FRAME,0x439378d3bad6a019, CommandHideMiniMapExteriorMapThisFrame);
	SCR_REGISTER_SECURE(HIDE_MINIMAP_INTERIOR_MAP_THIS_FRAME,0x9a42dd879a371772, CommandHideMiniMapInteriorMapThisFrame);
	SCR_REGISTER_SECURE(SET_USE_ISLAND_MAP,0x2c524d7cba043e37, CommandSetUseIslandMap);
	
	SCR_REGISTER_SECURE(DONT_TILT_MINIMAP_THIS_FRAME,0x676c385e11afca47, CommandDontTiltMiniMapThisFrame);
	SCR_REGISTER_SECURE(DONT_ZOOM_MINIMAP_WHEN_RUNNING_THIS_FRAME,0x0f84e4b0e0f11477, CommandDontZoomMiniMapWhenRunningThisFrame);
	SCR_REGISTER_SECURE(DONT_ZOOM_MINIMAP_WHEN_SNIPING_THIS_FRAME,0x62ce3d3b82942315, CommandDontZoomMiniMapWhenSnipingThisFrame);

// Radar Zone Mark commands


	SCR_REGISTER_SECURE(SET_WIDESCREEN_FORMAT,0x890999e6a3060606, CommandSetWidescreenFormat);
	SCR_REGISTER_UNUSED(GET_WIDESCREEN_FORMAT,0xfbf2e183925e140d, CommandGetWidescreenFormat);

	SCR_REGISTER_UNUSED(OVERRIDE_AREA_VEHICLE_STREET_NAME_TIME_ON,0x22582a1f2a9de331, CommandOverrideAreaVehicleStreetNameTimeOn);
	SCR_REGISTER_UNUSED(OVERRIDE_AREA_VEHICLE_STREET_NAME_TIME_OFF,0x21c7760c41becfd5, CommandOverrideAreaVehicleStreetNameTimeOff);

	SCR_REGISTER_SECURE(DISPLAY_AREA_NAME,0xece811c504ef8d48, CommandDisplayAreaName);
	SCR_REGISTER_SECURE(DISPLAY_CASH,0x6611550cbd3a321b, CommandDisplayCash);
	SCR_REGISTER_SECURE(USE_FAKE_MP_CASH,0xe0ef2aa080296002, CommandUseFakeMPCash);
	SCR_REGISTER_SECURE(CHANGE_FAKE_MP_CASH,0x32663800f0617d17, CommandChangeFakeMPCash);
	SCR_REGISTER_UNUSED(FAKE_SP_CASH_CHANGED,0x9943868983b70bc5, CommandFakeSPCashChange);
	SCR_REGISTER_SECURE(DISPLAY_AMMO_THIS_FRAME,0x3f5b55c881fe2e15, CommandDisplayAmmoThisFrame);
	SCR_REGISTER_SECURE(DISPLAY_SNIPER_SCOPE_THIS_FRAME,0x4da42d6554c5f1d5, CommandDisplaySniperScopeThisFrame);
	SCR_REGISTER_UNUSED(DISPLAY_FRONTEND_MAP_BLIPS,0x31a65f483a865fd2, CommandDisplayFrontendMapBlips);
	SCR_REGISTER_SECURE(HIDE_HUD_AND_RADAR_THIS_FRAME,0xc43e67c9ba871ecb, CommandHideHudAndRadarThisFrame);

	SCR_REGISTER_SECURE(ALLOW_DISPLAY_OF_MULTIPLAYER_CASH_TEXT,0xd032b1c1c61f216c, CommandAllowDisplayOfMultiplayerCashText);
	SCR_REGISTER_SECURE(SET_MULTIPLAYER_WALLET_CASH,0xa8958773478e3a2f, CommandSetMultiplayerWalletCash);
	SCR_REGISTER_SECURE(REMOVE_MULTIPLAYER_WALLET_CASH,0xb011dbfd44946d95, CommandRemoveMultiplayerWalletCash);
	SCR_REGISTER_SECURE(SET_MULTIPLAYER_BANK_CASH,0xb24c6b8e89ffe5d3, CommandSetMultiplayerBankCash);
	SCR_REGISTER_SECURE(REMOVE_MULTIPLAYER_BANK_CASH,0x3e095d3a28ad69bb, CommandRemoveMultiplayerBankCash);
	SCR_REGISTER_SECURE(SET_MULTIPLAYER_HUD_CASH,0xc98480c5886269ba, CommandSetMultiplayerHudCash);
	SCR_REGISTER_SECURE(REMOVE_MULTIPLAYER_HUD_CASH,0x086a4a67765b653b, CommandRemoveMultiplayerHudCash);

	SCR_REGISTER_UNUSED(SET_HUD_CASH_AS_COP_STAR,0x8be0ac94dcc00325, CommandSetHudCashAsCopStar);

	SCR_REGISTER_SECURE(HIDE_HELP_TEXT_THIS_FRAME,0x19dd1c202b338df3, CommandHideHelpTextThisFrame);
	SCR_REGISTER_SECURE(IS_IME_IN_PROGRESS,0xe4f7d42808db1edb, CommandIsIMEInProgress);
	SCR_REGISTER_SECURE(DISPLAY_HELP_TEXT_THIS_FRAME,0x284e43c619cc0d26, CommandDisplayHelpTextThisFrame);

	// WEAPON WHEEL
	SCR_REGISTER_SECURE(HUD_FORCE_WEAPON_WHEEL,0x1e672eb1f161aeeb, CommandHudForceWeaponWheel);
	SCR_REGISTER_SECURE(HUD_FORCE_SPECIAL_VEHICLE_WEAPON_WHEEL,0xda8fce6cbd6b62d2, SetWeaponWheelSpecialVehicleForcedByScript);
	SCR_REGISTER_SECURE(HUD_SUPPRESS_WEAPON_WHEEL_RESULTS_THIS_FRAME,0x6b3ec5908ea03c43, CommandHudSuppressWeaponWheelResultsThisFrame);
	SCR_REGISTER_UNUSED(HUD_SET_WEAPON_WHEEL_CONTENTS,0x2fd223883e4919e7, CommandHudSetWeaponWheelContents);
	SCR_REGISTER_SECURE(HUD_GET_WEAPON_WHEEL_CURRENTLY_HIGHLIGHTED,0xc965a5495f599392, CommandHudGetWeaponWheelCurrentlyHighlighted);
	SCR_REGISTER_UNUSED(HUD_GET_WEAPON_WHEEL_HAS_A_SLOT_WITH_MULTIPLE_WEAPONS, 0xbfdb0ea6, CommandHudGetWeaponWheelHasASlotWithMultipleWeapons);
	SCR_REGISTER_SECURE(HUD_SET_WEAPON_WHEEL_TOP_SLOT,0xa1f68c79e9ad5cf3, CommandHudSetWeaponWheelTopSlot);
	SCR_REGISTER_SECURE(HUD_GET_WEAPON_WHEEL_TOP_SLOT,0x48ee6c0e28668c6b, CommandHudGetWeaponWheelTopSlot);

	// CHARACTER SWITCH
	SCR_REGISTER_SECURE(HUD_SHOWING_CHARACTER_SWITCH_SELECTION,0x55ea7fd57aa96c7f, CommandHudShowingCharacterSwitch);

	// GPS

	SCR_REGISTER_SECURE(SET_GPS_FLAGS,0xec128d03e0c3566d, CommandSetGpsFlags);
	SCR_REGISTER_SECURE(CLEAR_GPS_FLAGS,0xf61564e12e6e0155, CommandClearGpsFlags);

	SCR_REGISTER_UNUSED(START_GPS_RACE_TRACK,0xd1600b19d13b4197, CommandStartGpsRaceTrack);
	SCR_REGISTER_UNUSED(ADD_POINT_TO_GPS_RACE_TRACK,0x6fde04be006eabcb, CommandAddPointToGpsRaceTrack);
	SCR_REGISTER_SECURE(SET_RACE_TRACK_RENDER,0x8662c9ec55537876, CommandRenderRaceTrack);
	SCR_REGISTER_SECURE(CLEAR_GPS_RACE_TRACK,0xdf41a4edb14c7aac, CommandClearGpsRaceTrack);

	SCR_REGISTER_SECURE(START_GPS_CUSTOM_ROUTE,0xedd2d3a6ead9a565, CommandStartGpsCustomRoute);
	SCR_REGISTER_SECURE(ADD_POINT_TO_GPS_CUSTOM_ROUTE,0xb48d56626cb36cf6, CommandAddPointToGpsCustomRoute);
	SCR_REGISTER_SECURE(SET_GPS_CUSTOM_ROUTE_RENDER,0x3d8b09bcf3ba1641, CommandRenderGpsCustomRoute);
	SCR_REGISTER_SECURE(CLEAR_GPS_CUSTOM_ROUTE,0xf73e7c096acc54d1, CommandClearGpsCustomRoute);

	SCR_REGISTER_SECURE(START_GPS_MULTI_ROUTE,0x31df7483722ba3a1, CommandStartGpsMultiRoute);
	SCR_REGISTER_SECURE(ADD_POINT_TO_GPS_MULTI_ROUTE,0x899cd4eca1259e41, CommandAddPointToGpsMultiRoute);
	SCR_REGISTER_SECURE(SET_GPS_MULTI_ROUTE_RENDER,0xc5239b40cad5176c, CommandRenderGpsMultiRoute);
	SCR_REGISTER_SECURE(CLEAR_GPS_MULTI_ROUTE,0xb2c30aead64a860f, CommandClearGpsMultiRoute);

	SCR_REGISTER_UNUSED(START_GPS_CUSTOM_ROUTE_FROM_WAYPOINT_ROUTE,0xd1d241c430d0fb81, CommandStartGpsCustomRouteFromWaypointRecordingRoute);
	SCR_REGISTER_UNUSED(START_GPS_CUSTOM_ROUTE_FROM_ASSISTED_ROUTE,0x3f35d6792b3b3928, CommandStartGpsCustomRouteFromAssistedRoute);

	SCR_REGISTER_UNUSED(SET_GPS_PLAYER_WAYPOINT_ON_ENTITY,0x285d346f06e1855d, CommandSetGpsPlayerWaypointOnEntity);
	SCR_REGISTER_SECURE(CLEAR_GPS_PLAYER_WAYPOINT,0xdd20be7783fde484, CommandClearGpsPlayerWaypoint);

	SCR_REGISTER_SECURE(SET_GPS_FLASHES,0x00838c2dc14803ce, CommandSetGPSFlashes);

	SCR_REGISTER_UNUSED(NETWORK_SET_RADIOHUD_IN_LOBBY_OFF,0x553d2d41bce5d774, CommandTurnOffRadioHudInLobby);

	SCR_REGISTER_UNUSED(GET_FRONTEND_DESIGN_VALUE,0xe4aee5bd670291d5, CommandGetFrontendDesignValue);

	SCR_REGISTER_SECURE(SET_PLAYER_ICON_COLOUR,0x4976ac35652b02ff, CommandSetPlayerIconColour);

	SCR_REGISTER_UNUSED(SET_PICKUP_BLIP_SCALE,0x28259352f5ebd281, CommandChangePickupBlipScale);
	SCR_REGISTER_UNUSED(SET_PICKUP_BLIP_PRIORITY,0x3558a1a866345c11, CommandChangePickupBlipPriority);
	SCR_REGISTER_UNUSED(SET_PICKUP_BLIP_DISPLAY,0x3a8bac5b6386a97e, CommandChangePickupBlipDisplay);
	SCR_REGISTER_UNUSED(SET_PICKUP_BLIP_COLOUR,0xe57d0e6de6caca13, CommandChangePickupBlipColour);
	SCR_REGISTER_UNUSED(SET_PICKUP_BLIP_SPRITE,0xcd934209d949e777,CommandChangePickupBlipSprite);

	SCR_REGISTER_UNUSED(INIT_FRONTEND_HELPER_TEXT,0x72dcd9be5c5e3e6a,								CommandInitFrontendHelperText);
	SCR_REGISTER_UNUSED(DRAW_FRONTEND_HELPER_TEXT,0x412e7ee717b21ec5,								CommandDrawFrontendHelperText);
	
	SCR_REGISTER_SECURE(FLASH_MINIMAP_DISPLAY,0xc70e97154cc5b243,									CommandFlashMinimapDisplay);
	SCR_REGISTER_SECURE(FLASH_MINIMAP_DISPLAY_WITH_COLOR,0x1abde97f42182259,						CommandFlashMinimapDisplayWithColor);
	SCR_REGISTER_SECURE(TOGGLE_STEALTH_RADAR,0xb803aee78bc2dd02,									CommandToggleStealthRadar);
	SCR_REGISTER_UNUSED(DRAW_PED_VISUAL_FIELD,0xb54cb9d6f2a5a405,									CommandDrawPedVisualField);

	SCR_REGISTER_SECURE(SET_MINIMAP_IN_SPECTATOR_MODE,0x6caf65c4f61a4bbd,							CommandSetMiniMapInSpectatorMode);

	SCR_REGISTER_SECURE(SET_MISSION_NAME,0xefe837b21d5cf02c,										CommandSetMissionName);
	SCR_REGISTER_SECURE(SET_MISSION_NAME_FOR_UGC_MISSION,0x02f3bf0b2f6fd7ee,						CommandSetMissionNameForUGCMission);
	SCR_REGISTER_UNUSED(SET_DESCRIPTION_FOR_UGC_MISSION,0x210b3e5d2bf734dc,						CommandSetDescriptionForUGCMission);
	SCR_REGISTER_SECURE(SET_DESCRIPTION_FOR_UGC_MISSION_EIGHT_STRINGS,0x5865e793fd7d704f,			CommandSetDescriptionForUGCMissionEightStrings);
	SCR_REGISTER_SECURE(SET_MINIMAP_BLOCK_WAYPOINT,0x0704376d0ea246e8,								CommandSetMiniMapBlockWaypoint);
	SCR_REGISTER_SECURE(SET_MINIMAP_IN_PROLOGUE,0x45125ae10bf524d3,								CommandSetMiniMapInPrologue);
	SCR_REGISTER_UNUSED(SET_MINIMAP_BACKGROUND_HIDDEN,0x64caf105d50753cf,							CommandSetMiniMapBackgroundHidden);
	SCR_REGISTER_UNUSED(SET_MINIMAP_DIMMED,0x8ee4ed78e579ffcd,										CommandSetMiniMapDimmed);
	SCR_REGISTER_SECURE(SET_MINIMAP_HIDE_FOW,0x9d58f1f38990e4fe,									CommandSetMinimapHideFoW);
	SCR_REGISTER_SECURE(GET_MINIMAP_FOW_DISCOVERY_RATIO,0x0ba77a703b108691,						CommandGetFoWDiscoveryRatio);
	SCR_REGISTER_SECURE(GET_MINIMAP_FOW_COORDINATE_IS_REVEALED,0x9dccb0e0a6fba49f,					CommandIsCoordinateRevealed);
	SCR_REGISTER_SECURE(SET_MINIMAP_FOW_DO_NOT_UPDATE,0x6c65593af9494db5,							CommandSetMinimapDoNotUpdateFoW);
	SCR_REGISTER_SECURE(SET_MINIMAP_FOW_REVEAL_COORDINATE,0xfd58f8cb1eb5622d,						CommandRevealCoordinates);

	SCR_REGISTER_UNUSED(SET_MINIMAP_ALLOW_FOW_IN_MP,0xb6cb984991549930,								CommandSetAllowFoWInMP);
	SCR_REGISTER_UNUSED(GET_MINIMAP_ALLOW_FOW_IN_MP,0xed108a94f2decd16,								CommandGetAllowFoWInMP);
	SCR_REGISTER_UNUSED(SET_MINIMAP_REQUEST_CLEAR_FOW,0xadc2f1a8d311be2c,							CommandSetRequestClearFoW);
	SCR_REGISTER_UNUSED(SET_MINIMAP_REQUEST_REVEAL_FOW,0xa36b744fd072342f,							CommandSetRequestRevealFoW);

	SCR_REGISTER_UNUSED(ENABLE_MINIMAP_FOW_CUSTOM_WORLD_EXTENTS,0x6b86f81fc54a6954,				CommandEnableFowCustomWorldExtents);
	SCR_REGISTER_UNUSED(ARE_MINIMAP_FOW_CUSTOM_WORLD_EXTENTS_ENABLED,0x5101958df4a0622e,			CommandAreFowCustomWorldExtentsEnabled);
	SCR_REGISTER_UNUSED(SET_MINIMAP_FOW_CUSTOM_WORLD_POS_AND_SIZE,0x388b4d51a7710bb8,				CommandSetFowCustomWorldPosAndSize);

	SCR_REGISTER_UNUSED(SET_MINIMAP_FOW_MP_SAVE_DETAILS,0x72e17e2bf5d1a314,						CommandSetMinimapFowMpSaveDetails);
	SCR_REGISTER_UNUSED(GET_MINIMAP_FOW_MP_SAVE_DETAILS,0x2903602213426462,						CommandGetMinimapFowMpSaveDetails);

	SCR_REGISTER_SECURE(SET_MINIMAP_GOLF_COURSE,0x62460755296d69f6,								CommandSetMiniMapGolfCourse);
	SCR_REGISTER_SECURE(SET_MINIMAP_GOLF_COURSE_OFF,0xac73a443d41d4898,							CommandSetMiniMapGolfCourseOff);
	SCR_REGISTER_SECURE(LOCK_MINIMAP_ANGLE,0xf3f07aaf13926729,										CommandLockMiniMapAngle);
	SCR_REGISTER_SECURE(UNLOCK_MINIMAP_ANGLE,0x1c35fdd57f36fbea,									CommandUnlockMiniMapAngle);
	SCR_REGISTER_SECURE(LOCK_MINIMAP_POSITION,0x262d43ebe3ca4397,									CommandLockMiniMapPosition);
	SCR_REGISTER_SECURE(UNLOCK_MINIMAP_POSITION,0xbd18a1ef31f0166b,								CommandUnlockMiniMapPosition);
	SCR_REGISTER_SECURE(SET_FAKE_MINIMAP_MAX_ALTIMETER_HEIGHT,0xdaa3c9fef77614c4,					CommandSetFakeMinimapMaxAltimeterHeight);
	SCR_REGISTER_SECURE(SET_HEALTH_HUD_DISPLAY_VALUES,0x1cbea88a9d0641c9,							CommandSetHealthHudDisplayValues);
	SCR_REGISTER_SECURE(SET_MAX_HEALTH_HUD_DISPLAY,0xc6a49d61f9d6c54e,								CommandSetMaxHealthHudDisplay);
	SCR_REGISTER_SECURE(SET_MAX_ARMOUR_HUD_DISPLAY,0x6ca9d991fdb47a79,								CommandSetMaxArmourHudDisplay);

	SCR_REGISTER_SECURE(SET_BIGMAP_ACTIVE,0x111b8c39ee6c95e7,										CommandSetBigMapActive);
	SCR_REGISTER_UNUSED(SET_BIGMAP_FULLSCREEN,0xf2f503577676c986,									CommandSetBigMapFullScreen);

	SCR_REGISTER_SECURE(IS_HUD_COMPONENT_ACTIVE,0x1ec008858f146889,								CommandIsHudComponentActive);
	SCR_REGISTER_SECURE(IS_SCRIPTED_HUD_COMPONENT_ACTIVE,0xd8e7fbba43b54355,						CommandIsScriptedHudComponentActive);
	SCR_REGISTER_SECURE(HIDE_SCRIPTED_HUD_COMPONENT_THIS_FRAME,0x6ec26fc7c9ec0d16,					CommandHideScriptedHudComponentThisFrame);
	SCR_REGISTER_SECURE(SHOW_SCRIPTED_HUD_COMPONENT_THIS_FRAME,0x5de9a897c16d7b8b,					CommandShowScriptedHudComponentThisFrame);
	SCR_REGISTER_SECURE(IS_SCRIPTED_HUD_COMPONENT_HIDDEN_THIS_FRAME,0x83bb81f26b7fb1ad,			CommandIsScriptedHudComponentHiddenThisFrame);
	SCR_REGISTER_SECURE(HIDE_HUD_COMPONENT_THIS_FRAME,0xac765ef46e85a446,							CommandHideHudComponentThisFrame);
	SCR_REGISTER_UNUSED(IS_HUD_COMPONENT_HIDDEN_THIS_FRAME,0xe03a785866b92062,						CommandIsHudComponentHiddenThisFrame);
	SCR_REGISTER_SECURE(SHOW_HUD_COMPONENT_THIS_FRAME,0x128ebde26c467532,							CommandShowHudComponentThisFrame);
	SCR_REGISTER_SECURE(HIDE_STREET_AND_CAR_NAMES_THIS_FRAME,0x0e0732d108a5bfa0,					CommandHideStreetAndCarNamesThisFrame);
	SCR_REGISTER_SECURE(RESET_RETICULE_VALUES,0xe8d409e227acd71d,									CommandResetReticuleValues);

	SCR_REGISTER_UNUSED(GET_LOWEST_TOP_RIGHT_Y_POS,0x663bf9727e93790e,								CommandGetLowestTopRightYPos);
	SCR_REGISTER_SECURE(RESET_HUD_COMPONENT_VALUES,0x4fef096d3ab4aa8c,								CommandResetHudComponentValues);
	SCR_REGISTER_SECURE(SET_HUD_COMPONENT_POSITION,0xe73f8cb95153f5b1,								CommandSetHudComponentPosition);
	SCR_REGISTER_SECURE(GET_HUD_COMPONENT_POSITION,0x71e36dd92e2a1642,								CommandGetHudComponentPosition);
	SCR_REGISTER_UNUSED(GET_HUD_COMPONENT_SIZE,0x64eb3223597aa5d7,									CommandGetHudComponentSize);
	SCR_REGISTER_UNUSED(SET_HUD_COMPONENT_COLOUR,0x69061d765a5ba3c4,								CommandSetHudComponentColour);
	SCR_REGISTER_UNUSED(SET_HUD_COMPONENT_ALPHA,0xd9af2ccd0f55094e,								CommandSetHudComponentAlpha);
	SCR_REGISTER_UNUSED(SET_RETICULE_OVERRIDE_THIS_FRAME,0xaa76c6965cd73e6b,						CommandSetReticuleOverrideThisFrame);

	SCR_REGISTER_SECURE(CLEAR_REMINDER_MESSAGE,0xc166f812177b2f12,									CommandClearReminderMessage);
	SCR_REGISTER_UNUSED(DISPLAY_REMINDER_MESSAGES,0x2ae22182486262f0,								CommandDisplayReminderMessages);

	SCR_REGISTER_SECURE(GET_HUD_SCREEN_POSITION_FROM_WORLD_POSITION,0x611083c6f030f850,			CommandGetHudScreenPositionFromWorldPosition);
	SCR_REGISTER_SECURE(OPEN_REPORTUGC_MENU,0xfe02f8243877a5c2,									CommmandOpenUGCMenu);
	SCR_REGISTER_SECURE(FORCE_CLOSE_REPORTUGC_MENU,0x811996236e7644b0,								CommandForceCloseReportMenu);
	SCR_REGISTER_SECURE(IS_REPORTUGC_MENU_OPEN,0x11391c9c9581b53d,									CommandIsReportMenuOpen);
	SCR_REGISTER_UNUSED(GET_CURRENT_UGC_PAGE,0x0703babda39038c8,									CommandGetCurrentReportMenuPage);
	
	

	//
	// "floating help text"
	// 
	SCR_REGISTER_UNUSED(DISPLAY_FLOATING_HELP_TEXT,0xde7b01da678ca1fb, CommandDisplayFloatingHelpText);
	SCR_REGISTER_UNUSED(DISPLAY_FLOATING_HELP_TEXT_WITH_NUMBER,0xdfc5601d3c3209c3, CommandDisplayFloatingHelpTextWithNumber);
	SCR_REGISTER_UNUSED(DISPLAY_FLOATING_HELP_TEXT_WITH_STRING,0xa541c631fc380b6b, CommandDisplayFloatingHelpTextWithString);
	SCR_REGISTER_UNUSED(DISPLAY_FLOATING_HELP_TEXT_WITH_STRING_AND_NUMBER,0xbf27e45fa7fd6e72, CommandDisplayFloatingHelpTextWithStringAndNumber);
	SCR_REGISTER_SECURE(IS_FLOATING_HELP_TEXT_ON_SCREEN,0x42a08f69ad1ff739, CommandIsFloatingHelpTextOnScreen);
	SCR_REGISTER_SECURE(SET_FLOATING_HELP_TEXT_SCREEN_POSITION,0x103e53b30af92ca0, CommandSetFloatingHelpTextScreenPosition);
	SCR_REGISTER_SECURE(SET_FLOATING_HELP_TEXT_WORLD_POSITION,0x9c0f9e5181b36182, CommandSetFloatingHelpTextWorldPosition);
	SCR_REGISTER_SECURE(SET_FLOATING_HELP_TEXT_TO_ENTITY,0x4ae51be152ccc94c, CommandSetFloatingHelpTextToEntity);
	SCR_REGISTER_SECURE(SET_FLOATING_HELP_TEXT_STYLE,0xc3cb73ea7347d9bd, CommandSetFloatingHelpTextStyle);
	SCR_REGISTER_SECURE(CLEAR_FLOATING_HELP,0xd4df0f443b8fd284, CommandClearFloatingHelp);
	SCR_REGISTER_UNUSED(IS_THIS_FLOATING_HELP_TEXT_BEING_DISPLAYED,0x5e6d313d859438f5, CommandIsThisFloatingHelpTextBeingDisplayed);
	SCR_REGISTER_UNUSED(IS_THIS_FLOATING_HELP_TEXT_WITH_NUMBER_BEING_DISPLAYED,0x33bc31da5078d09b, CommandIsThisFloatingHelpTextWithNumberBeingDisplayed);
	SCR_REGISTER_UNUSED(IS_THIS_FLOATING_HELP_TEXT_WITH_STRING_BEING_DISPLAYED,0xf95f0a818f73c7e5, CommandIsThisFloatingHelpTextWithStringBeingDisplayed);
	SCR_REGISTER_UNUSED(IS_THIS_FLOATING_HELP_TEXT_WITH_STRING_AND_NUM_BEING_DISPLAYED,0x365da543723da97b, CommandIsThisFloatingHelpTextWithStringAndNumberBeingDisplayed);


	// MP Tags:
	SCR_REGISTER_UNUSED(CREATE_MP_GAMER_TAG,0xb8f47fca9cb5ff4c, CommandCreateMPGamerTag);
	SCR_REGISTER_SECURE(CREATE_MP_GAMER_TAG_WITH_CREW_COLOR,0x9a7db029f0aa4c6f, CommandCreateMPGamerTagWithCrewColor);
	SCR_REGISTER_SECURE(IS_MP_GAMER_TAG_MOVIE_ACTIVE,0x80efb1e2e2560ad6, CommandIsMPGamerTagMovieActive);
	SCR_REGISTER_SECURE(CREATE_FAKE_MP_GAMER_TAG,0x03fc61b7f2a5b2d2, CommandCreateFakeMPGamerTag);
	SCR_REGISTER_UNUSED(CREATE_FAKE_MP_GAMER_TAG_WITH_CREW_COLOR,0x111db92ebdcf1648, CommandCreateFakeMPGamerTagWithCrewColor);
	SCR_REGISTER_UNUSED(CREATE_MP_GAMER_TAG_FOR_VEHICLE,0xf06f7a38d13e297f, CommandCreateMPGamerTagForVehicle);
	SCR_REGISTER_SECURE(REMOVE_MP_GAMER_TAG,0xa826a32e54aa4c15, CommandRemoveMPGamerTag);
	SCR_REGISTER_SECURE(IS_MP_GAMER_TAG_ACTIVE,0xac5dda397cada2c6, CommandIsMPGamerTagActive);
	SCR_REGISTER_SECURE(IS_MP_GAMER_TAG_FREE,0x6237b54b11850790, CommandIsMPGamerTagFree);
	SCR_REGISTER_SECURE(SET_MP_GAMER_TAG_VISIBILITY,0x465d668cf4119c04, CommandSetMPGamerTagVisibility);
	SCR_REGISTER_SECURE(SET_ALL_MP_GAMER_TAGS_VISIBILITY,0x3961e00a7ab016e7, CommandSetAllMPGamerTagsVisibility);
	SCR_REGISTER_UNUSED(SET_MP_GAMER_TAGS_CAN_RENDER_WHEN_PAUSED,0x914886cd24de155b, CommandSetGamerTagCanRenderWhenPaused);
	SCR_REGISTER_SECURE(SET_MP_GAMER_TAGS_SHOULD_USE_VEHICLE_HEALTH,0xef07efe231052d7f, CommandSetGamerTagsShouldUseVehicleHealth);
	SCR_REGISTER_SECURE(SET_MP_GAMER_TAGS_SHOULD_USE_POINTS_HEALTH,0xd791b361a95ef923, CommandSetGamerTagsShouldUsePointHealth);
	SCR_REGISTER_SECURE(SET_MP_GAMER_TAGS_POINT_HEALTH,0x4e142c761b569fcb, CommandSetGamerTagsPointHealth);
	SCR_REGISTER_SECURE(SET_MP_GAMER_TAG_COLOUR,0xd68a138d534769e8, CommandSetMPGamerTagColour);
	SCR_REGISTER_SECURE(SET_MP_GAMER_TAG_HEALTH_BAR_COLOUR,0x34ce544f73db9118, CommandSetMPGamerTagHealthBarColour);
	SCR_REGISTER_SECURE(SET_MP_GAMER_TAG_ALPHA,0xcc3c77459d6604f8, CommandSetMPGamerTagAlpha);
	SCR_REGISTER_SECURE(SET_MP_GAMER_TAG_WANTED_LEVEL,0x7ada62f58586bf28, CommandSetMPGamerTagWantedLevel);
	SCR_REGISTER_SECURE(SET_MP_GAMER_TAG_NUM_PACKAGES,0x61a2f420e5936c0b, CommandSetMPGamerTagNumPackages);	
	SCR_REGISTER_UNUSED(SET_MP_GAMER_TAG_RANK,0xc4f08c7219c1e691, CommandSetMPGamerTagRank);
	SCR_REGISTER_UNUSED(SET_MP_GAMER_TAG_CREW_DETAILS,0x014c2dfdc698b6d1, CommandSetMPGamerCrewDetails);
	SCR_REGISTER_SECURE(SET_MP_GAMER_TAG_NAME,0x3a32e5636851aa85, CommandSetMPGamerName);
	SCR_REGISTER_SECURE(IS_UPDATING_MP_GAMER_TAG_NAME_AND_CREW_DETAILS,0x9b3b33e47dab4bdc, CommandIsUpdatingMPGamerNameAndCrewTag);
	SCR_REGISTER_SECURE(SET_MP_GAMER_TAG_BIG_TEXT,0xed4f77976d49cbd2, CommandSetMPGamerBigText);
	SCR_REGISTER_UNUSED(SET_MP_GAMER_TAG_VERTICAL_OFFSET,0x7a8bb1e36e1c430b, CommandSetGamerTagVerticalOffset);
	
	// webpage:
	SCR_REGISTER_SECURE(GET_CURRENT_WEBPAGE_ID,0x521b7005b6f45a1e, CommandGetCurrentWebpageId);
	SCR_REGISTER_SECURE(GET_CURRENT_WEBSITE_ID,0x377a5b907f1da56d, CommandGetCurrentWebsiteId);

	SCR_REGISTER_SECURE(GET_GLOBAL_ACTIONSCRIPT_FLAG,0x974b1c69fc991e56, CommandGetGlobalActionscriptFlag);
	SCR_REGISTER_SECURE(RESET_GLOBAL_ACTIONSCRIPT_FLAG,0x28ee7e032936a666, CommandResetGlobalActionscriptFlag);

	// warning screens:
	SCR_REGISTER_UNUSED(SET_WARNING_MESSAGE_IN_USE_THIS_FRAME,0x20b8419054192c7a, CommandSetWarningMessageInUseThisFrame);
	SCR_REGISTER_SECURE(IS_WARNING_MESSAGE_READY_FOR_CONTROL,0x0b24c5748895a04d, CommandIsWarningMessageReadyForControl);
	SCR_REGISTER_UNUSED(BEGIN_SCALEFORM_MOVIE_METHOD_ON_WARNING_MESSAGE,0x5499272ac38beb55, CommandBeginScaleformMovieMethodOnWarningMessage);

	SCR_REGISTER_SECURE(SET_WARNING_MESSAGE,0x7b1776b3b53f8d74, CommandSetWarningMessage);
	SCR_REGISTER_SECURE(SET_WARNING_MESSAGE_WITH_HEADER,0xdc38cc1e35b6a5d7, CommandSetWarningMessageWithHeader);
	SCR_REGISTER_SECURE(SET_WARNING_MESSAGE_WITH_HEADER_AND_SUBSTRING_FLAGS,0x701919482c74b5ab, CommandSetWarningMessageWithHeaderAndSubstringFlags);
	
	SCR_REGISTER_UNUSED(SET_WARNING_MESSAGE_EXTENDED,0xf3621bb3182cbdae, CommandSetWarningMessageExtended);
	SCR_REGISTER_SECURE(SET_WARNING_MESSAGE_WITH_HEADER_EXTENDED,0x38b55259c2e078ed, CommandSetWarningMessageWithHeaderExtended);
	SCR_REGISTER_SECURE(SET_WARNING_MESSAGE_WITH_HEADER_AND_SUBSTRING_FLAGS_EXTENDED,0x15803fec3b9a872b, CommandSetWarningMessageWithHeaderAndSubstringFlagsExtended);

	SCR_REGISTER_SECURE(GET_WARNING_SCREEN_MESSAGE_HASH,0x99c5758d126de2c4, CommandGetWarningScreenMessageHash);

	SCR_REGISTER_SECURE(SET_WARNING_MESSAGE_OPTION_ITEMS,0x0c5a80a9e096d529, CommandSetWarningMessageOptionItems);
	SCR_REGISTER_SECURE(SET_WARNING_MESSAGE_OPTION_HIGHLIGHT,0xa2ed59c9836377b1, CommandSetWarningMessageOptionHighlight);
	SCR_REGISTER_SECURE(REMOVE_WARNING_MESSAGE_OPTION_ITEMS,0x681caca8eac238f8, CommandRemoveWarningMessageOptionItems);


	SCR_REGISTER_SECURE(IS_WARNING_MESSAGE_ACTIVE,0x3e6c9dad84cefed1, CommandIsWarningMessageActive);

	SCR_REGISTER_UNUSED(GET_HUD_MINIMAP_POSITION_AND_SIZE,0xc5a34aba01fc3107, CommandGetMinimapPosAndSize);

	SCR_REGISTER_SECURE(CLEAR_DYNAMIC_PAUSE_MENU_ERROR_MESSAGE,0x4b3922c7d61ef53d, CommandClearDynamicPauseMenuErrorMessage);

	// custom minimap for menus
	SCR_REGISTER_SECURE(CUSTOM_MINIMAP_SET_ACTIVE,0xa252a30b2bfab672, CommandCustomMinimapSetActive);
	SCR_REGISTER_UNUSED(CUSTOM_MINIMAP_IS_ACTIVE,0x60ab184d423f538e, CommandCustomMinimapIsActive);
	
	SCR_REGISTER_UNUSED(CUSTOM_MINIMAP_SNAP_TO_BLIP_WITH_INDEX,0x0c8e521862509939, CommandCustomMinimapSnapToBlipWithIndex);
	SCR_REGISTER_UNUSED(CUSTOM_MINIMAP_SNAP_TO_BLIP_WITH_UNIQUEID,0x0b18d6d50a40bbe8, CommandCustomMinimapSnapToBlipWithUniqueId);

	SCR_REGISTER_UNUSED(CUSTOM_MINIMAP_SET_WAYPOINT_WITH_INDEX,0xe573f8bd122a4db1, CommandCustomMinimapSetWaypointWithIndex);

	SCR_REGISTER_SECURE(CUSTOM_MINIMAP_SET_BLIP_OBJECT,0x3205db2634bd3142, CommandCustomMinimapSetBlipObject);
	SCR_REGISTER_UNUSED(CUSTOM_MINIMAP_GET_BLIP_OBJECT,0xb5d1397de561de1b, CommandCustomMinimapGetBlipObject);

	SCR_REGISTER_SECURE(CUSTOM_MINIMAP_CREATE_BLIP,0x76e0b18c5fdf0430, CommandCustomMinimapCreateBlip);
	SCR_REGISTER_UNUSED(CUSTOM_MINIMAP_REMOVE_BLIP,0xa6f8f845101bf88d, CommandCustomMinimapRemoveBlip);
	SCR_REGISTER_SECURE(CUSTOM_MINIMAP_CLEAR_BLIPS,0xa407e52c82d0e3fd, CommandCustomMinimapClearBlips);
	SCR_REGISTER_UNUSED(CUSTOM_MINIMAP_GET_NUMBER_OF_BLIPS,0x606e00f238f61016, CommandCustomMinimapGetNumberOfBlips);

	SCR_REGISTER_SECURE(FORCE_SONAR_BLIPS_THIS_FRAME,0xca69a1cc253d2d00, CommandForceSonarBlipsThisFrame);

	SCR_REGISTER_SECURE( GET_NORTH_BLID_INDEX,0x95f54f350e397d05, CommandGetNorthBlipId);
	SCR_REGISTER_SECURE(DISPLAY_PLAYER_NAME_TAGS_ON_BLIPS,0xa12ae3835d111ddb, CommandDisplayPlayerNameTagsOnBlips);
	
	// frontend menu control:
	SCR_REGISTER_SECURE(DRAW_FRONTEND_BACKGROUND_THIS_FRAME,0x2b8143efb6dc045a, CommandDrawFrontendBackgroundThisFrame);
	SCR_REGISTER_SECURE(DRAW_HUD_OVER_FADE_THIS_FRAME,0xb33d86ef66b25071, CommandDrawHudOverFadeThisFrame);
	SCR_REGISTER_SECURE(ACTIVATE_FRONTEND_MENU,0xe395f22b346cef9d, CommandActivateFrontEndMenu);
	SCR_REGISTER_SECURE(RESTART_FRONTEND_MENU,0x0d45994817d78358, CommandRestartFrontEndMenu);
	SCR_REGISTER_SECURE(GET_CURRENT_FRONTEND_MENU_VERSION,0x09afb3c8f99a613f, CommandGetCurrentFrontEndMenuVersion);
	SCR_REGISTER_SECURE(SET_PAUSE_MENU_ACTIVE,0xe6d8097f6a553f2a, CommandSetPauseMenuActive);
	SCR_REGISTER_SECURE(DISABLE_FRONTEND_THIS_FRAME,0xcafe3d4fd6c43219, CommandDisableFrontEndThisFrame);
	SCR_REGISTER_SECURE(SUPPRESS_FRONTEND_RENDERING_THIS_FRAME,0xbcaccafa8d0e5c94, CommandSuppressFrontEndRenderingThisFrame);
	
	SCR_REGISTER_SECURE(ALLOW_PAUSE_WHEN_NOT_IN_STATE_OF_PLAY_THIS_FRAME,0x95071e89d6440ce1, CommandAllowPauseWhenNotInStateOfPlayThisFrame);

	SCR_REGISTER_SECURE(SET_FRONTEND_ACTIVE,0xdd0b677578e78e45, CommmandSetFrontendActive);
	SCR_REGISTER_SECURE(IS_PAUSE_MENU_ACTIVE,0xf1ec2c71fd1371b8, CommandIsPauseMenuActive);
	SCR_REGISTER_UNUSED(SET_PAD_CAN_SHAKE_DURING_PAUSE_MENU,0x8e43141fb98cd586, CommandSetPadCanShakeDuringPauseMenu);

	SCR_REGISTER_SECURE(IS_STORE_PENDING_NETWORK_SHUTDOWN_TO_OPEN,0xa7a987a7ce42c2da, CommandIsStorePendingNetworkShutdownToOpen);

	SCR_REGISTER_SECURE(GET_PAUSE_MENU_STATE,0x1a76a9a82bd6228c, CommandGetPauseMenuState);
	SCR_REGISTER_SECURE(GET_PAUSE_MENU_POSITION,0x20f495803c1d7e9b, CommandGetPauseMenuPosition);
	
	SCR_REGISTER_SECURE(IS_PAUSE_MENU_RESTARTING,0x9b13832261890609, CommandIsPauseMenuRestarting);
	SCR_REGISTER_SECURE(FORCE_SCRIPTED_GFX_WHEN_FRONTEND_ACTIVE,0x9eddad8c2d04a4a2, CommandForceScriptedGfxWhenFrontendActive);

	SCR_REGISTER_SECURE(PAUSE_MENUCEPTION_GO_DEEPER,0x13a559f6ce99dd31, CommandPauseMenuceptionGoDeeper);
	SCR_REGISTER_SECURE(PAUSE_MENUCEPTION_THE_KICK,0x8965bcbb343c3074, CommandPauseMenuceptionTheKick);
	SCR_REGISTER_SECURE(PAUSE_TOGGLE_FULLSCREEN_MAP,0x543277ff566ea8b0, CommandPauseToggleFullscreenMap);

	SCR_REGISTER_SECURE(PAUSE_MENU_ACTIVATE_CONTEXT,0x324093e167766493, CommandPauseMenuActivateContext);
	SCR_REGISTER_SECURE(PAUSE_MENU_DEACTIVATE_CONTEXT,0x24db748bf8e6bf86, CommandPauseMenuDeactivateContext);
	SCR_REGISTER_SECURE(PAUSE_MENU_IS_CONTEXT_ACTIVE,0xfe728873ce709085, CommandPauseMenuIsContextActive);
	SCR_REGISTER_SECURE(PAUSE_MENU_IS_CONTEXT_MENU_ACTIVE,0x94eccfff64aa04fa, CommandPauseMenuIsContextMenuActive);
	SCR_REGISTER_SECURE(PAUSE_MENU_GET_HAIR_COLOUR_INDEX,0x79fcde980db89304, CommandPauseMenuGetHairColourId);
	
	SCR_REGISTER_SECURE(PAUSE_MENU_GET_MOUSE_HOVER_INDEX,0xc3bab7ccbb2b42fe, CommandPauseMenuGetMouseHoverIndex);
	SCR_REGISTER_UNUSED(PAUSE_MENU_GET_MOUSE_HOVER_MENU_ITEM_ID,0xc06b4592110d9759, CommandPauseMenuGetMouseHoverMenuItemId);
	SCR_REGISTER_SECURE(PAUSE_MENU_GET_MOUSE_HOVER_UNIQUE_ID,0x6b459cd1218d1d96, CommandPauseMenuGetMouseHoverUniqueId);
	SCR_REGISTER_SECURE(PAUSE_MENU_GET_MOUSE_CLICK_EVENT,0x8851d00cee7c0186, CommandPauseMenuGetMouseClickEvent);

	SCR_REGISTER_SECURE(PAUSE_MENU_REDRAW_INSTRUCTIONAL_BUTTONS,0x58a612f25790461c, CommandPauseMenuRedrawInstructionalButtons);
	SCR_REGISTER_SECURE(PAUSE_MENU_SET_BUSY_SPINNER,0x8251081b36660220, CommandPauseMenuSetBusySpinner);
	SCR_REGISTER_SECURE(PAUSE_MENU_SET_WARN_ON_TAB_CHANGE,0xf5b86abb5f4b3a2f, CommandPauseMenuSetWarnOnTabChange);

#if RSG_GEN9
	SCR_REGISTER_UNUSED(PAUSE_MENU_SET_CLOUD_BUSY_SPINNER, 0xcde8a5996ca4edc0, CommandPauseMenuSetCloudBusySpinner);
	SCR_REGISTER_UNUSED(PAUSE_MENU_CLEAR_CLOUD_BUSY_SPINNER, 0xb67b38329f5c473a, CommandPauseMenuClearCloudBusySpinner);
#endif // RSG_GEN9

	SCR_REGISTER_SECURE(IS_FRONTEND_READY_FOR_CONTROL,0xf9bdbaa4fc5b46fe, CommandIsFrontendReadyForControl);
	SCR_REGISTER_SECURE(TAKE_CONTROL_OF_FRONTEND,0x373ae80d2ddb5c46, CommandTakeControlOfFrontend);
	SCR_REGISTER_SECURE(RELEASE_CONTROL_OF_FRONTEND,0x918a52708638ea9b, CommandReleaseControlOfFrontend);
	SCR_REGISTER_SECURE(CODE_WANTS_SCRIPT_TO_TAKE_CONTROL,0xae1c7c8cfcc275f9, CommandCodeWantsScriptToTakeControl);
	SCR_REGISTER_SECURE(GET_SCREEN_CODE_WANTS_SCRIPT_TO_CONTROL,0x113cc89632e2c20d, CommandGetScreenCodeWantsScriptToControl);

	
	SCR_REGISTER_SECURE(IS_NAVIGATING_MENU_CONTENT,0x1d1604fa184430f7, CommandIsNavigatingMenuContent);
	SCR_REGISTER_SECURE(HAS_MENU_TRIGGER_EVENT_OCCURRED,0x40deeffceb637529, CommandHasMenuTriggerEventOccurred);
	SCR_REGISTER_SECURE(HAS_MENU_LAYOUT_CHANGED_EVENT_OCCURRED,0x09327ba0c008a581, CommandHasMenuLayoutChangedEventOccurred);

	SCR_REGISTER_SECURE(SET_SAVEGAME_LIST_UNIQUE_ID,0x46978fca0f1ea6ce, CommandSetSavegameListUniqueId);

	SCR_REGISTER_SECURE(GET_MENU_TRIGGER_EVENT_DETAILS,0x75088fad0b49b1a8, CommandGetMenuTriggerEventDetails);
	SCR_REGISTER_SECURE(GET_MENU_LAYOUT_CHANGED_EVENT_DETAILS,0xa981c26f706d5ecf, CommandGetMenuLayoutChangedEventDetails);

	SCR_REGISTER_UNUSED(SET_FRONTEND_TAB_LOCKED,0x52af440b981cb6c8, CommandSetFrontendTabLocked);

	SCR_REGISTER_UNUSED(SET_CONTENT_ARROW_VISIBLE,0x98080636c10f2d62, CommandSetContentArrowVisible);
	SCR_REGISTER_UNUSED(SET_CONTENT_ARROW_POSITION,0x7e1fe2bcbb891233, CommandSetContentArrowPosition);

	SCR_REGISTER_SECURE(GET_PM_PLAYER_CREW_COLOR,0xd4c335ee280ddb77, CommandGetPMPlayerCrewColor);

	SCR_REGISTER_SECURE(GET_MENU_PED_INT_STAT,0x08c246faea34e413, CommandGetMenuPedIntStat);
	SCR_REGISTER_SECURE(GET_CHARACTER_MENU_PED_INT_STAT,0x12fa79d99f6d66db, CommandGetCharacterMenuPedIntStat);
	SCR_REGISTER_SECURE(GET_MENU_PED_MASKED_INT_STAT,0x344ecddc16c7def1, CommandGetMenuPedMaskedIntStat);
	SCR_REGISTER_SECURE(GET_CHARACTER_MENU_PED_MASKED_INT_STAT,0x4f46ae5cc69707ce, CommandGetCharacterMenuPedMaskedIntStat);

	SCR_REGISTER_SECURE(GET_MENU_PED_FLOAT_STAT,0x33a710bcac7ab802, CommandGetMenuPedFloatStat);
	SCR_REGISTER_SECURE(GET_CHARACTER_MENU_PED_FLOAT_STAT,0x86b9025b867d9380, CommandGetCharacterMenuPedFloatStat);
	SCR_REGISTER_SECURE(GET_MENU_PED_BOOL_STAT,0x7470d10647b74302, CommandGetMenuPedBoolStat);
	SCR_REGISTER_UNUSED(GET_CHARACTER_MENU_PED_BOOL_STAT,0x22be07335de545a4, CommandGetCharacterMenuPedBoolStat);

	SCR_REGISTER_SECURE(CLEAR_PED_IN_PAUSE_MENU,0x86cf46c25b8d6473, CommandClearPedInPauseMenu);
	SCR_REGISTER_SECURE(GIVE_PED_TO_PAUSE_MENU,0xd125b0ecda9c1a27, CommandGivePedToPauseMenu);
	SCR_REGISTER_UNUSED(SET_PM_PED_BG_VISIBILITY,0x04ab3f0c93e59c35, CommandSetPMPedBGVisibility);
	SCR_REGISTER_UNUSED(SHOW_PLAYER_PED_IN_PAUSE_MENU,0x25c0f74a3afb1c29, CommandShowPlayerPedInPauseMenu);
	SCR_REGISTER_SECURE(SET_PAUSE_MENU_PED_LIGHTING,0x552750077ba51925, CommandSetPauseMenuPedLighting);
	SCR_REGISTER_SECURE(SET_PAUSE_MENU_PED_SLEEP_STATE,0x6811e51206bb9d1a, CommandSetPauseMenuPedSleepState);

	SCR_REGISTER_SECURE(OPEN_ONLINE_POLICIES_MENU,0xbb1a9a32ddfce6d7, CommandOpenOnlinePoliciesMenu);
	SCR_REGISTER_SECURE(ARE_ONLINE_POLICIES_UP_TO_DATE,0x8844efa0218dc31d, CommandAreOnlinePoliciesUpToDate);
	SCR_REGISTER_SECURE(IS_ONLINE_POLICIES_MENU_ACTIVE,0x88c6ff5a86356e32, CommandIsOnlinePoliciesActive);

	SCR_REGISTER_SECURE(OPEN_SOCIAL_CLUB_MENU,0x7bff54dbead33da7, CommandOpenSocialClubMenu);
	SCR_REGISTER_SECURE(CLOSE_SOCIAL_CLUB_MENU,0xe62cffb4b4aa12e6, CommandCloseSocialClubMenu);
	SCR_REGISTER_SECURE(SET_SOCIAL_CLUB_TOUR,0x6e6999b535c86055, CommandSetSocialClubTour);
	SCR_REGISTER_SECURE(IS_SOCIAL_CLUB_ACTIVE,0x483cf2539a512e5e, CommandIsSocialClubActive);

	SCR_REGISTER_SECURE(SET_TEXT_INPUT_BOX_ENABLED,0x87068d181565c4b7, CommandSetTextInputBoxEnabled);
	SCR_REGISTER_SECURE(FORCE_CLOSE_TEXT_INPUT_BOX,0x087fa48489dcbcee, CommandCloseTextInputBox);
	SCR_REGISTER_SECURE(SET_ALLOW_COMMA_ON_TEXT_INPUT,0x8de1b9b6e92dfeb8, CommandSetAllowCommaOnTextInputBox);

	
	SCR_REGISTER_SECURE(OVERRIDE_MP_TEXT_CHAT_TEAM_STRING,0xa6a63ef67b2895af,CommandScriptOverrideMPTeamChatString);
	SCR_REGISTER_SECURE(IS_MP_TEXT_CHAT_TYPING,0xf9e7fedd44686ba3, CommandIsMPTextChatTyping);
	SCR_REGISTER_SECURE(CLOSE_MP_TEXT_CHAT,0x5ff5a6b4eaed8565, CommandCloseMPTextChat);
	SCR_REGISTER_SECURE(MP_TEXT_CHAT_IS_TEAM_JOB,0xe61c968f64f118d6, CommandIsTeamBasedJob);
	SCR_REGISTER_SECURE(OVERRIDE_MP_TEXT_CHAT_COLOR,0xc8119e9ea63506a3, CommandOverrideMPTextChatColor);
	SCR_REGISTER_SECURE(MP_TEXT_CHAT_DISABLE,0x81b302aaeec2f2b6, CommandDisableTextChat);
	SCR_REGISTER_SECURE(FLAG_PLAYER_CONTEXT_IN_TOURNAMENT,0xeefc54f43c5eba92, CommandFlagPlayerContextInTournament);

	// CScriptPedAIBlips interface
	SCR_REGISTER_SECURE(SET_PED_HAS_AI_BLIP,0x0b69dcf6fe80e8eb, CommandSetPedHasAIBlip);
	SCR_REGISTER_SECURE(SET_PED_HAS_AI_BLIP_WITH_COLOUR,0xbfd785b1d608bc11, CommandSetPedHasAIBlipWithColour);
	SCR_REGISTER_SECURE(DOES_PED_HAVE_AI_BLIP,0x227d9dce160dda9d, CommandDoesPedHaveAIBlip);
	SCR_REGISTER_SECURE(SET_PED_AI_BLIP_GANG_ID,0xeec8a34ff9213fe4, CommandSetAIBlipGangID);
	SCR_REGISTER_SECURE(SET_PED_AI_BLIP_HAS_CONE,0x922263c0fea956c8, CommandsSetAIBlipHasCone);
	SCR_REGISTER_SECURE(SET_PED_AI_BLIP_FORCED_ON,0x0fb9b56364b11bc9, CommandsSetAIBlipForcedOn);
	SCR_REGISTER_SECURE(SET_PED_AI_BLIP_NOTICE_RANGE,0x7c87f71c68a9af0b, CommandsSetAIBlipNoticeRange);
	SCR_REGISTER_UNUSED(SET_PED_AI_BLIP_CHANGE_COLOUR,0xe1cfb21f0ae3ad76, CommandsSetAIBlipChangeColour);
	SCR_REGISTER_SECURE(SET_PED_AI_BLIP_SPRITE,0x21d1ace7ce56627c, CommandsSetAIBlipSprite);
	SCR_REGISTER_SECURE(GET_AI_PED_PED_BLIP_INDEX,0xc6cefb49e88bdd9b, CommandsGetAIBlipPedBlipIDX);
	SCR_REGISTER_SECURE(GET_AI_PED_VEHICLE_BLIP_INDEX,0xfeb99716362809a3, CommandsGetAIBlipVehicleBlipIDX);

	// Rockstar Editor
	SCR_REGISTER_SECURE(HAS_DIRECTOR_MODE_BEEN_LAUNCHED_BY_CODE,0x2940f2270f5f4f7c, CommandHasDirectorModeBeenLaunchedByCode);
	SCR_REGISTER_SECURE(SET_DIRECTOR_MODE_LAUNCHED_BY_SCRIPT,0xe823a8ca7abbb847, CommandSetDirectorModeLaunchedByScript);
	SCR_REGISTER_SECURE(SET_PLAYER_IS_IN_DIRECTOR_MODE,0xeec83eb4b5afd94c, CommandSetPlayerIsInDirectorMode);
	SCR_REGISTER_SECURE(SET_DIRECTOR_MODE_AVAILABLE,0x49f2ab96167a1aef, CommandSetDirectorModeAvailable);

	//HUD Markers
	SCR_REGISTER_SECURE(HIDE_HUDMARKERS_THIS_FRAME,0x66f2f60e8589a96d, CommandHideHudMarkersThisFrame);
	SCR_REGISTER_UNUSED(IS_HUDMARKER_VALID,0xf5e12865c75047c5, CommandIsHudMarkerValid);
	SCR_REGISTER_UNUSED(ADD_HUDMARKER,0xecb732f50d878f6d, CommandAddHudMarker);
	SCR_REGISTER_UNUSED(ADD_HUDMARKER_FOR_BLIP,0x440a33ec66764381, CommandAddHudMarkerForBlip);
	SCR_REGISTER_UNUSED(REMOVE_HUDMARKER,0xed8c89a34ba06487, CommandRemoveHudMarker);
	SCR_REGISTER_UNUSED(SET_HUDMARKER_GROUP,0xe37ec263bc38fff4, CommandSetHudMarkerGroup);
	SCR_REGISTER_UNUSED(SET_HUDMARKER_POSITION,0x80798640dbe1f798, CommandSetHudMarkerPosition);
	SCR_REGISTER_UNUSED(SET_HUDMARKER_HEIGHT_OFFSET,0x965f9ffad59c35ec, CommandSetHudMarkerHeightOffset);
	SCR_REGISTER_UNUSED(SET_HUDMARKER_COLOUR,0x4fc4f815de2fe321, CommandSetHudMarkerColour);
	//SCR_REGISTER_UNUSED(SET_HUDMARKER_ICON, 0x8301637e1bf2826a, CommandSetHudMarkerIcon);
	SCR_REGISTER_UNUSED(SET_HUDMARKER_CLAMP_ENABLED,0xb5b784f3acd0d4b4, CommandSetHudMarkerClampEnabled);

	//HUD Marker Groups
	SCR_REGISTER_UNUSED(SET_HUDMARKER_GROUP_FORCE_VISIBLE_NUM,0xa187218b08fc7769, CommandSetHudMarkerGroupMaxClampNum);
	SCR_REGISTER_UNUSED(SET_HUDMARKER_GROUP_MAX_CLAMP_NUM,0x589a8e0e473389ef, CommandSetHudMarkerGroupForceVisibleNum);
	SCR_REGISTER_UNUSED(SET_HUDMARKER_GROUP_MAX_VISIBLE,0x5b26f46c9b42c167, CommandSetHudMarkerGroupMaxVisible);
	SCR_REGISTER_UNUSED(SET_HUDMARKER_GROUP_MIN_DISTANCE,0xaca091ec77fd993f, CommandSetHudMarkerGroupMinDistance);
	SCR_REGISTER_UNUSED(SET_HUDMARKER_GROUP_MAX_DISTANCE,0xc1cd9ad0799b22e5, CommandSetHudMarkerGroupMaxDistance);
	SCR_REGISTER_UNUSED(SET_HUDMARKER_GROUP_MIN_TEXT_DISTANCE,0x9ce4d6f2d3144248, CommandSetHudMarkerGroupMinTextDistance);
	SCR_REGISTER_UNUSED(SET_HUDMARKER_GROUP_IS_SOLO,0xccd3e8ba4c026c9d, CommandSetHudMarkerGroupIsSolo);
	SCR_REGISTER_UNUSED(SET_HUDMARKER_GROUP_IS_MUTED,0x7781537104c7acb3, CommandSetHudMarkerGroupIsMuted);
	SCR_REGISTER_UNUSED(SET_HUDMARKER_GROUP_ALWAYS_SHOW_DISTANCE_TEXT_FOR_CLOSEST,0x4f744d6bf85593b7, CommandSetHudMarkerAlwaysShowDistanceTextForClosest);
}

};
