/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : GameStreamMgr.cpp
// PURPOSE : manages the Scaleform hud live info/updates
//
// See: http://rsgediwiki1/wiki/index.php/HUD_Game_Stream for reference.
//
/////////////////////////////////////////////////////////////////////////////////

#if __BANK
#include "parser/manager.h"
#include "bank/bkmgr.h"
#include "text/text.h"
#include "frontend/hud_colour.h"
#endif // __BANK

#include "parser/manager.h"
#include "fwsys/gameskeleton.h"
#include "grcore/quads.h"
#include "grcore/viewport.h"
#include "fwscript/scriptguid.h"
#include "frontend/Scaleform/ScaleFormMgr.h"
#include "frontend/Scaleform/ScaleFormStore.h"
#include "frontend/PauseMenu.h"
#include "frontend/NewHud.h"
#include "GameStreamMgr.h"
#include "system/StreamingInstall.winrt.h"
#if __BANK
#include "text/TextConversion.h"
#endif


FRONTEND_OPTIMISATIONS()
NETWORK_OPTIMISATIONS()

CGameStream*	CGameStreamMgr::ms_pGameStream = NULL;
s32				CGameStreamMgr::ms_GameStreamMovieId = SF_INVALID_MOVIE;
CGameStreamMgr::movSetup CGameStreamMgr::ms_GameStreamIngame;
CGameStreamMgr::movSetup CGameStreamMgr::ms_GameStreamLoading;
int CGameStreamMgr::ms_GameTipLast = -1;
int CGameStreamMgr::ms_OnlineTipLast = -1;
eLoadStat CGameStreamMgr::ms_LoadStatus = GAMESTREAM_UNLOADED;
int CGameStreamMgr::ms_UnloadUpdateCount = GAMESTREAM_UNLOAD_UPDATEWAIT;
atArray<CGameStreamMgr::sToolTip> CGameStreamMgr::m_SinglePlayerToolTips;
atArray<CGameStreamMgr::sToolTip> CGameStreamMgr::m_MultiPlayerToolTips;
atArray<CGameStreamMgr::sToolTip> CGameStreamMgr::m_ReplayToolTips;
#if RSG_PC
atArray<CGameStreamMgr::sToolTip> CGameStreamMgr::m_ShowOnLoadToolTips;
int CGameStreamMgr::ms_ShowOnLoadTipIdIndex = 0;
#endif // RSG_PC
Vector2		CGameStreamMgr::ms_vLastAlignedPos(-1.0f, -1.0f);
Vector2		CGameStreamMgr::ms_vLastAlignedSize(-1.0f, -1.0f);

BANK_ONLY(static bool ms_bShowDebugBounds;)

void CGameStreamMgr::RequestAssets()
{
	strLocalIndex gameStreamIndex = g_ScaleformStore.FindSlot("GAME_STREAM");
	if (gameStreamIndex != -1)
	{
		g_ScaleformStore.StreamingRequest(gameStreamIndex, STRFLAG_FORCE_LOAD | STRFLAG_DONTDELETE);
	}
}

// ---------------------------------------------------------
void CGameStreamMgr::Init(unsigned initMode)
{
	if( initMode == rage::INIT_CORE )
	{
		if(ms_pGameStream == NULL)
		{
			m_SinglePlayerToolTips.Reset();
			m_MultiPlayerToolTips.Reset();
			m_ReplayToolTips.Reset();

			REGISTER_FRONTEND_XML(CGameStreamMgr::HandleXML, "GameStream");
			REGISTER_GAMESTREAM_XML(CGameStreamMgr::HandleTooltipXml, "ToolTips");
			LoadXmlData(!StreamingInstall::HasInstallFinished());
			ms_pGameStream = rage_new CGameStream;
			GetPosAndSize(ms_vLastAlignedPos, ms_vLastAlignedSize);
			SetMovieID(CScaleformMgr::CreateMovieAndWaitForLoad(
				"GAME_STREAM", 
				ms_vLastAlignedPos, 
				ms_vLastAlignedSize, 
				false,
				-1,
				-1,
				true,
				SF_MOVIE_TAGGED_BY_CODE,
				false,
				true));
			CScaleformMgr::ForceMovieUpdateInstantly(ms_GameStreamMovieId, true);
			SetMovieLoadStatus(GAMESTREAM_LOADED);
			ms_UnloadUpdateCount = GAMESTREAM_UNLOAD_UPDATEWAIT;
			ms_pGameStream->DefaultHidden();
			ms_pGameStream->SetTipsLast( ms_GameTipLast, ms_OnlineTipLast );

			//if( CScaleformMgr::BeginMethod(ms_GameStreamMovieId, SF_BASE_CLASS_GAMESTREAM, "ENABLE_DEBUG") )
			//{
			//	CScaleformMgr::AddParamBool( true );
			//	CScaleformMgr::EndMethod();
			//}
		}
		else if (!CFrontendXMLMgr::IsLoaded())
		{
			CFrontendXMLMgr::LoadXML();
		}
	}

	if( initMode == INIT_SESSION )
	{
		ms_pGameStream->DefaultHidden();
		if( ms_pGameStream!=NULL )
		{
			ms_pGameStream->SetDisplayConfigChanged();
		}
	}
}

// ---------------------------------------------------------
void CGameStreamMgr::Shutdown(unsigned shutdownMode)
{
	if( shutdownMode == rage::SHUTDOWN_CORE )
	{
#if __BANK
		ShutdownWidgets();
#endif
		if( ms_pGameStream != NULL )
		{
			delete ms_pGameStream;
			ms_pGameStream = NULL;
			
			if ( ms_GameStreamMovieId != SF_INVALID_MOVIE )
			{
				CScaleformMgr::RequestRemoveMovie(ms_GameStreamMovieId);
				SetMovieID(SF_INVALID_MOVIE);
			}
		}
	}
}

// ---------------------------------------------------------
CGameStream* CGameStreamMgr::GetGameStream()
{
	return( ms_pGameStream );
}

// ---------------------------------------------------------
void CGameStreamMgr::Update()
{
	// During the loading screens, CGameStream::Update is called from both the
	// main thread AND the render thread :(  Use the Scaleform mutex to ensure
	// that we are not running both at the same time.
	SYS_CS_SYNC(CScaleformMgr::GetRealSafeZoneToken());

	if( ms_pGameStream != NULL )
	{
		UpdateLoadUnload();
		UpdateChangeMovieParams();
		ms_pGameStream->Update();
		if( ms_LoadStatus == GAMESTREAM_LOADED )
			ms_pGameStream->InteractWithScaleform( ms_GameStreamMovieId );
	}
#if __BANK
		DebugUpdate();
#endif
}

// Check and process the loading/unloading of the feed movie
// ---------------------------------------------------------
void CGameStreamMgr::UpdateLoadUnload()
{
	if (!CSystem::IsThisThreadId(SYS_THREAD_UPDATE))
	{
		return;
	}

	// we have a valid id, but the movie is shutting down, so invalidate the movie id and set the load status to unloaded
	if( ms_GameStreamMovieId != SF_INVALID_MOVIE && CScaleformMgr::IsMovieShuttingDown( ms_GameStreamMovieId ) )
	{
		SetMovieID(SF_INVALID_MOVIE);  // DP: once a movie has been requested to be removed, the id you had is dead - reset it here or expect fuck ups - should fix obscure issues like 1613784 and 1613794
		SetMovieLoadStatus(GAMESTREAM_UNLOADED);
	}

	switch ( ms_LoadStatus )
	{
		case GAMESTREAM_UNLOADED:
		if( ms_pGameStream->CountQueued() > 0 )
		{
			GetPosAndSize(ms_vLastAlignedPos, ms_vLastAlignedSize);
			SetMovieID(CScaleformMgr::CreateMovie(
				"GAME_STREAM", 
				ms_vLastAlignedPos, 
				ms_vLastAlignedSize, 
				false,
				-1,
				-1,
				true,
				SF_MOVIE_TAGGED_BY_CODE,
				false,
				true));  // fix for 1784907 (this movie needs to stay around between sessions)
			
			uiAssertf(ms_GameStreamMovieId != SF_INVALID_MOVIE, "Call to CScaleformMgr::CreateMovie returned an invalid movie");

			if ( ms_GameStreamMovieId != SF_INVALID_MOVIE )
			{
				SetMovieLoadStatus(GAMESTREAM_LOADING);
				ms_UnloadUpdateCount = GAMESTREAM_UNLOAD_UPDATEWAIT;
			}


		}
		break;

		case GAMESTREAM_LOADING:
		if( ms_GameStreamMovieId != SF_INVALID_MOVIE && CScaleformMgr::IsMovieActive( ms_GameStreamMovieId ) )
		{
			ms_pGameStream->JustReloaded();
			SetMovieLoadStatus(GAMESTREAM_LOADED);
			BANK_ONLY(ms_pGameStream->SetShowDebugBoundsEnabled(ms_GameStreamMovieId, ms_bShowDebugBounds, true);)
		}
		break;
	
		case GAMESTREAM_LOADED:
		if( ms_pGameStream->CountQueued() == 0 )
		{
			if( ms_UnloadUpdateCount == 0 )
			{
				if( ms_GameStreamMovieId != SF_INVALID_MOVIE && CScaleformMgr::IsMovieActive( ms_GameStreamMovieId ) )
				{
					CScaleformMgr::RequestRemoveMovie(ms_GameStreamMovieId);
					SetMovieID(SF_INVALID_MOVIE);  // DP: once a movie has been requested to be removed, the id you had is dead - reset it here or expect fuck ups - should fix obscure issues like 1613784 and 1613794
				}
				SetMovieLoadStatus(GAMESTREAM_UNLOADING);
			}
			else
			{
				ms_UnloadUpdateCount--;
			}
		}
		else
		{
			ms_UnloadUpdateCount = GAMESTREAM_UNLOAD_UPDATEWAIT;
		}
		break;
		
		case GAMESTREAM_UNLOADING:
		if ( SF_INVALID_MOVIE == ms_GameStreamMovieId )
		{
			//A safety check. In case we have got here without a valid movie we can switch the state over to unloaded.
			SetMovieLoadStatus(GAMESTREAM_UNLOADED);
		}
		else if( !CScaleformMgr::IsMovieShuttingDown(ms_GameStreamMovieId) )
		{
			SetMovieLoadStatus(GAMESTREAM_UNLOADED);
			SetMovieID(SF_INVALID_MOVIE);
		}
		break;

		default:
		break;
	}
}

// ---------------------------------------------------------
void CGameStreamMgr::UpdateChangeMovieParams()
{
	if ( ms_LoadStatus != GAMESTREAM_LOADED )
	{
		return;
	}

	//Shouldn't occur, but this is hostile territory so play safe.
	if ( ms_GameStreamMovieId == SF_INVALID_MOVIE )
	{
		return;
	}

	if( ms_pGameStream->ChangeMovieParams() )
	{
		Vector2 alignedPos;
		Vector2 alignedSize;
		GetPosAndSize(alignedPos, alignedSize);

		// in order to stave off weird shizz like B* 1582465, don't do this unless we absolutely have to
		if( alignedPos != ms_vLastAlignedPos || alignedSize != ms_vLastAlignedSize )
		{
			uiDebugf3("GameStream wants to change movie params from Pos %fx%f to %fx%f", ms_vLastAlignedPos.x, ms_vLastAlignedPos.y, alignedPos.x, alignedPos.y);
			uiDebugf3("And size %fx%f to %fx%f", ms_vLastAlignedSize.x, ms_vLastAlignedSize.y, alignedSize.x, alignedSize.y);

			ms_vLastAlignedPos = alignedPos;
			ms_vLastAlignedSize = alignedSize;

			CScaleformMgr::ChangeMovieParams(ms_GameStreamMovieId, alignedPos, alignedSize, GFxMovieView::SM_ExactFit);
		}
	}
}

void CGameStreamMgr::GetPosAndSize(Vector2& alignedPos, Vector2& alignedSize)
{
	CGameStreamMgr::movSetup& curSetup = ms_pGameStream->IsOnLoadingScreen() ? ms_GameStreamLoading : ms_GameStreamIngame;

	alignedPos = curSetup.moviePos;
	alignedSize = curSetup.movieSize;
	CHudTools::AdjustNormalized16_9ValuesForCurrentAspectRatio(WIDESCREEN_FORMAT_LEFT, &alignedPos, &alignedSize);
	alignedPos = CHudTools::CalculateHudPosition( alignedPos, alignedSize, curSetup.alignX, curSetup.alignY );
}

// ---------------------------------------------------------
void CGameStreamMgr::SetMovieLoadStatus(eLoadStat loadStatus) 
{ 	
	uiDebugf3("Setting Game Stream Movie Load Status to %d ", loadStatus); 
	ms_LoadStatus = loadStatus;
}

// ---------------------------------------------------------
void CGameStreamMgr::SetMovieID(int iMovieID)
{
	uiDebugf3("Changing Game Stream Movie ID to %d ", iMovieID); 
	ms_GameStreamMovieId = iMovieID;
}

// ---------------------------------------------------------
void CGameStreamMgr::Render()
{
	if( ms_GameStreamMovieId != SF_INVALID_MOVIE )
	{
		if( ms_pGameStream && ms_pGameStream->ShouldRender() )
		{
			CScaleformMgr::RenderMovie( ms_GameStreamMovieId, 0.0f, true );
		}
	}
}


void CGameStreamMgr::ReadSetup( const char* tag, movSetup* pDest, parTreeNode* pGameStreamNode )
{
	pDest->alignX = 'L';
	pDest->alignY = 'L';
	pDest->moviePos.Zero();
	pDest->movieSize.Zero();

	parTreeNode* pNode = NULL;
	while( (pNode=pGameStreamNode->FindChildWithName(tag,pNode)) != NULL )
	{
		if(pNode->GetElement().FindAttribute("alignX"))
		{
			pDest->alignX = (pNode->GetElement().FindAttributeAnyCase("alignX")->GetStringValue())[0];
		}

		if(pNode->GetElement().FindAttribute("alignY"))
		{
			pDest->alignY = (pNode->GetElement().FindAttributeAnyCase("alignY")->GetStringValue())[0];
		}

		if (pNode->GetElement().FindAttribute("posX"))
		{
			pDest->moviePos.x = (float)atof(pNode->GetElement().FindAttributeAnyCase("posX")->GetStringValue());
		}

		if (pNode->GetElement().FindAttribute("posY"))
		{
			pDest->moviePos.y = (float)atof(pNode->GetElement().FindAttributeAnyCase("posY")->GetStringValue());
		}

		if (pNode->GetElement().FindAttribute("sizeX"))
		{
			pDest->movieSize.x = (float)atof(pNode->GetElement().FindAttributeAnyCase("sizeX")->GetStringValue());
		}

		if (pNode->GetElement().FindAttribute("sizeY"))
		{
			pDest->movieSize.y = (float)atof(pNode->GetElement().FindAttributeAnyCase("sizeY")->GetStringValue());
		}
	}
}

int CGameStreamMgr::GetTipLast( const char* tag, parTreeNode* pGameStreamNode )
{
	int last = -1;
	parTreeNode* pNode = NULL;
	while( (pNode=pGameStreamNode->FindChildWithName(tag,pNode)) != NULL )
	{
		if (pNode->GetElement().FindAttribute("lasttip"))
		{
			last = (int)atoi(pNode->GetElement().FindAttributeAnyCase("lasttip")->GetStringValue());
		}
	}
	return( last );
}

void CGameStreamMgr::HandleXML( parTreeNode* pGameStreamNode )
{
	ReadSetup( "position", &ms_GameStreamIngame, pGameStreamNode );
#if RSG_ORBIS
	ReadSetup( "position", &ms_GameStreamLoading, pGameStreamNode );
#else
	ReadSetup( "posloading", &ms_GameStreamLoading, pGameStreamNode );
#endif
	ms_GameTipLast = GetTipLast( "gametips", pGameStreamNode );
	ms_OnlineTipLast = GetTipLast( "onlinetips", pGameStreamNode );
}


void CGameStreamMgr::LoadXmlData(bool bLoadGameStreamDataOnly)
{
	if(bLoadGameStreamDataOnly)
	{
		CFrontendXMLMgr::LoadXMLNode("GameStream");
	}
	else
	{
		CFrontendXMLMgr::LoadXML();
	}

	CGamestreamXMLMgr::LoadXML();
}


// ---------------------------------------------------------
void CGameStreamMgr::CheckIncomingFunctions( atHashWithStringBank methodName, const GFxValue* args )
{
	if(ATSTRINGHASH("STREAM_ITEM_SHOWN",0x81041534) == methodName)
	{
		if (uiVerifyf(args[1].IsNumber() && args[2].IsNumber(), "STREAM_ITEM_SHOWN params not compatible: %s", sfScaleformManager::GetTypeName(args[1])))
		{
			CGameStream* GameStream = CGameStreamMgr::GetGameStream();
			if( GameStream != NULL )
			{
				GameStream->HandleShownStreamItem( (s32)args[1].GetNumber(), (s32)args[2].GetNumber());
			}
		}
		return;
	}
}


// ----------------------------------------------------------
// Debug
// ----------------------------------------------------------

#if __BANK

// MESSAGE TEXT
static bool ms_bDebugMessageTextFlash;
static bool ms_bDebugMessageTextOverrideTexture;
static char ms_iDebugMessageTextIcon[8];
static int  ms_iDebugMessageTextIcon2 = 0;
static char ms_cDebugMessageTextMsg[256];
static char ms_bDebugMessageTextChrName[128];
static char ms_bDebugMessageTextSubtitle[128];
static float ms_timeMultiplier;
static char ms_cDebugMessageTextContactTxD[128];	// *
static char ms_cDebugMessageTextContactTxN[128];	// *
static char ms_cDebugMessageTextCrewName[5];
// END MESSAGE TEXT

static bool ms_bDebugStatsFlash;
static char ms_cDebugStatsTitle[32];
static char ms_cDebugStatsBody[256];
static char ms_cDebugStatsLevelTotal[8];
static char ms_cDebugStatsLevelCurrent[8];
static char ms_cDebugStatsTxD[128];
static char ms_cDebugStatsTxN[128];

static bool ms_bDebugTickerFlash;
static char ms_cDebugTickerBody[256];
static bool ms_bDebugTickerIsLocKey;
static int	ms_bDebugTickerNumIconFlashes;

static char ms_cDebugTickerF10Title[256];
static char ms_cDebugTickerF10Body[256];

static char ms_cDebugAwardTitle[128];
static char ms_cDebugAwardName[128];
static char ms_cDebugAwardTXD[128];
static char ms_cDebugAwardTXN[128];
static int ms_iAwardXP;
static int ms_iAwardColour;

static bool ms_bDebugCrewTagFlash;
static bool ms_bDebugCrewTagIsPrivate;
static bool ms_bDebugCrewTagShowLogoFlag;
static char ms_cDebugCrewTagCrewString[8];
static char ms_cDebugCrewTagCrewRank[8];
static bool ms_bDebugCrewTagFounderStatus;
static char ms_cDebugCrewTagBody[256];
static int ms_cDebugCrewTagCrewId = 724;
static s32 ms_uDebugCrewTagCrewColourR = 255;
static int ms_uDebugCrewTagCrewColourG = 255;
static int ms_uDebugCrewTagCrewColourB = 255;

static char ms_cDebugCrewRankupTitle[256];
static char ms_cDebugCrewRankupSubtitle[256];
static char ms_cDebugCrewRankupTXD[128];
static char ms_cDebugCrewRankupTXN[128];

static char ms_cDebugVersusCh1TXD[128];
static char ms_cDebugVersusCh1TXN[128];
static int ms_iDebugVersusVal1 = 0;
static char ms_cDebugVersusCh2TXD[128];
static char ms_cDebugVersusCh2TXN[128];
static int ms_iDebugVersusVal2 = 0;
static int ms_iCustomColor1 = -1;
static int ms_iCustomColor2 = -1;

static char ms_cDebugReplayTitle[128];
static char ms_cDebugReplaySubtitle[128];
static bool ms_bDebugReplayBufferFullFlash;
static int ms_iDebugReplayType;
static int ms_iDebugReplayButtonIcon;
static char ms_cDebugReplayButtonToken[64];
static float ms_fDebugReplayPercentage = 0.0f;

static bool ms_bDebugUnlockFlash;
static char ms_cDebugUnlockTitle[128];
static char ms_cDebugUnlockBody[128];
static int ms_iUnlockTypeIcon = 0;
static int ms_eDebugUnlockTitleColour = 0;

static bool ms_bBankWidgetsCreated;

static int ms_iDebugAutoTestPost;
static int ms_iDebugAutoTestCount;
static bool ms_bDebugTestFeed;
static bool ms_bDebugFeedAlwaysRender;

static char ms_cDebugSpinnerIcon[32];
static char ms_cDebugSpinnerBody[256];

static char ms_cDebugImportantParamsbgR[16];
static char ms_cDebugImportantParamsbgG[16];
static char ms_cDebugImportantParamsbgB[16];
static char ms_cDebugImportantParamsalphaFlash[16];
static char ms_cDebugImportantParamsflashRate[16];
static bool ms_cDebugImportantParamsVibrate;
static bool ms_bDebugFreezeNextPost;
static bool ms_bDebugSnapPositions;

static int ms_iDebugHudColor = -1;

static char		ms_cDebugScriptedMenuHeight[32];
static char		ms_cDebugGameTip[32];

// ---------------------------------------------------------
void CGameStreamMgr::AutoPostGameTipOn()
{
	ms_pGameStream->AutoPostGameTipOn();
}

// ---------------------------------------------------------
void CGameStreamMgr::AutoPostGameTipOff()
{
	ms_pGameStream->AutoPostGameTipOff();
}

// ---------------------------------------------------------
void CGameStreamMgr::DebugClearFrozenPost()
{
	ms_pGameStream->ClearFrozenPost();
}

// ---------------------------------------------------------
void CGameStreamMgr::DebugSetImportantParams()
{
	ms_pGameStream->SetImportantParams( atoi(ms_cDebugImportantParamsbgR), atoi(ms_cDebugImportantParamsbgG), atoi(ms_cDebugImportantParamsbgB),
		atoi(ms_cDebugImportantParamsalphaFlash), atoi(ms_cDebugImportantParamsflashRate), ms_cDebugImportantParamsVibrate
	);
}

// ---------------------------------------------------------
void CGameStreamMgr::DebugResetImportantParams()
{
	ms_pGameStream->ResetImportantParams();
}

// ---------------------------------------------------------
void CGameStreamMgr::DebugSetNextPostBackgroundColor()
{
	ms_pGameStream->SetNextPostBackgroundColor( static_cast<eHUD_COLOURS>(ms_iDebugHudColor) );
}

// ---------------------------------------------------------
void CGameStreamMgr::DebugSetScriptedMenuHeight()
{
	ms_pGameStream->SetScriptedMenuHeight( (float)atof(ms_cDebugScriptedMenuHeight) );
}

// ---------------------------------------------------------
void CGameStreamMgr::DebugSpinnerOn()
{
	ms_pGameStream->BusySpinnerOn( ms_cDebugSpinnerBody, atoi(ms_cDebugSpinnerIcon) );
}

// ---------------------------------------------------------
void CGameStreamMgr::DebugSpinnerOff()
{
	ms_pGameStream->BusySpinnerOff();
}

// ---------------------------------------------------------
void CGameStreamMgr::DebugMessageTextDoPost()
{
	char CrewTagPacked[NetworkClan::FORMATTED_CLAN_TAG_LEN];
	NetworkClan::GetUIFormattedClanTag( false, false, ms_cDebugMessageTextCrewName, -1, Color32(0), CrewTagPacked, NetworkClan::FORMATTED_CLAN_TAG_LEN);

	char StrippedString[MAX_CHARS_FOR_TEXT_STRING];
	CTextConversion::StripNonRenderableText(StrippedString, GetLocAttempt(ms_cDebugMessageTextMsg));
	ms_pGameStream->PostMessageText(StrippedString, ms_cDebugMessageTextContactTxD, ms_cDebugMessageTextContactTxN, ms_bDebugMessageTextFlash, atoi(ms_iDebugMessageTextIcon), GetLocAttempt(ms_bDebugMessageTextChrName),  GetLocAttempt(ms_bDebugMessageTextSubtitle), ms_timeMultiplier, CrewTagPacked, ms_iDebugMessageTextIcon2);	
	
	if(ms_bDebugMessageTextOverrideTexture)
	{
		ms_pGameStream->UpdateFeedItemTexture(ms_cDebugMessageTextContactTxD, ms_cDebugMessageTextContactTxN, "CHAR_DEFAULT", "CHAR_DEFAULT");
	}
}

// ---------------------------------------------------------
void CGameStreamMgr::DebugStatsDoPost()
{
	ms_pGameStream->PostStats( GetLocAttempt(ms_cDebugStatsTitle), GetLocAttempt(ms_cDebugStatsBody), atoi(ms_cDebugStatsLevelTotal), atoi(ms_cDebugStatsLevelCurrent), ms_bDebugStatsFlash, ms_cDebugStatsTxD, ms_cDebugStatsTxN );
}

// ---------------------------------------------------------
void CGameStreamMgr::DebugTickerDoPost()
{
	ms_pGameStream->PostTicker( ms_cDebugTickerBody, ms_bDebugTickerFlash, true, ms_bDebugTickerIsLocKey, ms_bDebugTickerNumIconFlashes, ms_bDebugFeedAlwaysRender );
}

// ---------------------------------------------------------
void CGameStreamMgr::DebugTickerF10DoPost()
{
	ms_pGameStream->PostTickerF10( GetLocAttempt(ms_cDebugTickerF10Title), GetLocAttempt(ms_cDebugTickerF10Body), false );
}

// ---------------------------------------------------------

void CGameStreamMgr::DebugAwardDoPost()
{
	ms_pGameStream->PostAward( GetLocAttempt(ms_cDebugAwardName), ms_cDebugAwardTXD, ms_cDebugAwardTXN, ms_iAwardXP, static_cast<eHUD_COLOURS>(ms_iAwardColour), GetLocAttempt(ms_cDebugAwardTitle));
}

// ---------------------------------------------------------

void CGameStreamMgr::DebugCrewTagDoPost()
{
	ms_pGameStream->PostCrewTag( ms_bDebugCrewTagIsPrivate, ms_bDebugCrewTagShowLogoFlag, ms_cDebugCrewTagCrewString, atoi(ms_cDebugCrewTagCrewRank), ms_bDebugCrewTagFounderStatus, ms_cDebugCrewTagBody, ms_bDebugCrewTagFlash, (rlClanId)ms_cDebugCrewTagCrewId, NULL, Color32(ms_uDebugCrewTagCrewColourR, ms_uDebugCrewTagCrewColourG, ms_uDebugCrewTagCrewColourB));
}

// ---------------------------------------------------------

void CGameStreamMgr::DebugCrewRankupDoPost()
{
	ms_pGameStream->PostCrewRankup(GetLocAttempt(ms_cDebugCrewRankupTitle), GetLocAttempt(ms_cDebugCrewRankupSubtitle), ms_cDebugCrewRankupTXD, ms_cDebugCrewRankupTXN, false);
}

// ---------------------------------------------------------

void CGameStreamMgr::DebugVersusDoPost()
{
	ms_pGameStream->PostVersus(ms_cDebugVersusCh1TXD, ms_cDebugVersusCh1TXN, ms_iDebugVersusVal1, ms_cDebugVersusCh2TXD, ms_cDebugVersusCh2TXN, ms_iDebugVersusVal2, static_cast<eHUD_COLOURS>(ms_iCustomColor1), static_cast<eHUD_COLOURS>(ms_iCustomColor2));
}

void CGameStreamMgr::DebugReplayDoPost()
{
	ms_pGameStream->PostReplay(static_cast<CGameStream::eFeedReplayState>(ms_iDebugReplayType), ms_cDebugReplayTitle, ms_cDebugReplaySubtitle, ms_iDebugReplayButtonIcon, ms_fDebugReplayPercentage, ms_bDebugReplayBufferFullFlash, ms_cDebugReplayButtonToken);
}

// ---------------------------------------------------------

void CGameStreamMgr::DebugGameTipRandomPost()
{
	CGameStream::eGameStreamTipType currentTipType;

	if (CNetwork::GetGoStraightToMultiplayer() || CNetwork::IsNetworkOpen())
	{
		currentTipType = CGameStream::TIP_TYPE_MP;
	}
	else
	{
		currentTipType = CGameStream::TIP_TYPE_SP;
	}

	ms_pGameStream->PostGametip(GAMESTREAM_GAMETIP_RANDOM, currentTipType);
}

// ---------------------------------------------------------
void CGameStreamMgr::DebugGameTipPost()
{
	CGameStream::eGameStreamTipType currentTipType;

	if (CNetwork::GetGoStraightToMultiplayer() || CNetwork::IsNetworkOpen())
	{
		currentTipType = CGameStream::TIP_TYPE_MP;
	}
	else
	{
		currentTipType = CGameStream::TIP_TYPE_SP;
	}

	ms_pGameStream->PostGametip(atoi(ms_cDebugGameTip), currentTipType);
}

void CGameStreamMgr::DebugUnlockDoPost()
{
	ms_pGameStream->PostUnlock( GetLocAttempt(ms_cDebugUnlockTitle), GetLocAttempt(ms_cDebugUnlockBody), ms_iUnlockTypeIcon, ms_bDebugUnlockFlash, (eHUD_COLOURS)ms_eDebugUnlockTitleColour);
}

void CGameStreamMgr::DebugUnloadGameStream()
{
	if ( ms_GameStreamMovieId != SF_INVALID_MOVIE )
	{
		CScaleformMgr::RequestRemoveMovie(ms_GameStreamMovieId);
		SetMovieID(SF_INVALID_MOVIE);
	}
}

void CGameStreamMgr::DebugReloadGameStream()
{
	if ( ms_GameStreamMovieId == SF_INVALID_MOVIE )
	{
		GetPosAndSize(ms_vLastAlignedPos, ms_vLastAlignedSize);
	
		SetMovieID(CScaleformMgr::CreateMovieAndWaitForLoad(
			"GAME_STREAM", 
			ms_vLastAlignedPos, 
			ms_vLastAlignedSize, 
			false,
			-1,
			-1,
			true,
			SF_MOVIE_TAGGED_BY_CODE,
			false,
			true));
		CScaleformMgr::ForceMovieUpdateInstantly(ms_GameStreamMovieId, true);
		if(ms_pGameStream)
		{
			ms_pGameStream->SetDisplayConfigChanged();
		}
	}
}

// ---------------------------------------------------------
void CGameStreamMgr::DebugFlushQueue()
{
	ms_pGameStream->FlushQueue();
}

// ---------------------------------------------------------
void CGameStreamMgr::DebugForceFlushQueue()
{
	ms_pGameStream->ForceFlushQueue();
}

// ---------------------------------------------------------
void CGameStreamMgr::DebugForceRenderOn()
{
	ms_pGameStream->ForceRenderOn();

}

// ---------------------------------------------------------
void CGameStreamMgr::DebugForceRenderOff()
{
	ms_pGameStream->ForceRenderOff();

}

// ---------------------------------------------------------
void CGameStreamMgr::DebugSetShowDebugBoundsEnabled()
{
	ms_pGameStream->SetShowDebugBoundsEnabled(ms_GameStreamMovieId, ms_bShowDebugBounds);
}

// ---------------------------------------------------------
void CGameStreamMgr::DebugPause()
{
	ms_pGameStream->Pause();
	
}

// ---------------------------------------------------------
void CGameStreamMgr::DebugResume()
{
	ms_pGameStream->Resume();
}

// ---------------------------------------------------------
void CGameStreamMgr::DebugHideThisUpdate()
{
	ms_pGameStream->HideThisUpdate();
}

// ---------------------------------------------------------
void CGameStreamMgr::DebugHide()
{
	ms_pGameStream->Hide();
}

// ---------------------------------------------------------
void CGameStreamMgr::DebugShow()
{
	ms_pGameStream->Show();
}

// ---------------------------------------------------------
void CGameStreamMgr::DebugUpdate()
{
	if( ms_pGameStream == NULL )
	{
		return;
	}

	if(ms_bDebugFreezeNextPost)
	{
		ms_pGameStream->FreezeNextPost();
		ms_bDebugFreezeNextPost = false;
	}

	ms_pGameStream->SetSnapFeedItemPositions(ms_bDebugSnapPositions);

	ms_pGameStream->SetShowDebugBoundsEnabled(ms_GameStreamMovieId, ms_bShowDebugBounds);

	if( ms_bDebugTestFeed )
	{
		ms_iDebugAutoTestPost++;
		if( ms_iDebugAutoTestPost > 400 )
		{	
			char teststr[128];
			sprintf(teststr, "This is a test ticker %d",ms_iDebugAutoTestCount);
			ms_pGameStream->PostTickerF10( "Hello World", teststr, false );
			ms_iDebugAutoTestCount++;
			if( ms_iDebugAutoTestPost > 430 )
			{
				ms_iDebugAutoTestPost = 0;
			}
		}
	}
	else
	{
		ms_iDebugAutoTestPost = 400;
	}
}

// ---------------------------------------------------------
void CGameStreamMgr::DebugCreateTheFeedBankWidgets()
{
	if( ms_bBankWidgetsCreated )
	{
		return; // Please do not press this button again.
	}

	bkBank* bank = BANKMGR.FindBank("ui");
	if( bank == NULL )
	{
		return;
	}
	
	bank->PushGroup("TheFeed");
		
		bank->AddButton("FlushQueue", &DebugFlushQueue);
		bank->AddButton("ForceFlushQueue", &DebugForceFlushQueue);
		bank->AddSeparator();
		bank->AddButton("Force render ON", &DebugForceRenderOn);
		bank->AddButton("Force render OFF", &DebugForceRenderOff);
		bank->AddSeparator();
		bank->AddToggle("Show feed position and movie bounds info", &ms_bShowDebugBounds);
		ms_bShowDebugBounds = false;
		bank->AddSeparator();
		bank->AddButton("Unload Game Stream", &DebugUnloadGameStream);
		bank->AddButton("Reload Game Stream", &DebugReloadGameStream);
		bank->AddSeparator();
		bank->AddButton("Pause", &DebugPause);
		bank->AddButton("Resume", &DebugResume);
		bank->AddSeparator();
		bank->AddButton("Hide", &DebugHide);
		bank->AddButton("Show", &DebugShow);
		bank->AddSeparator();
		bank->AddButton("Hide This Update", &DebugHideThisUpdate);
		bank->AddButton("Auto post game tips ON", &AutoPostGameTipOn);
		bank->AddButton("Auto post game tips OFF", &AutoPostGameTipOff);
		bank->AddToggle("Enable test feed", &ms_bDebugTestFeed);
		ms_bDebugFreezeNextPost = false;
		bank->AddToggle("Freeze Next Post", &ms_bDebugFreezeNextPost);
		bank->AddButton("Clear Frozen Post", &DebugClearFrozenPost);
		ms_bDebugSnapPositions = false;
		bank->AddToggle("Snap Feed Items Into Position", &ms_bDebugSnapPositions);

		bank->AddSeparator();

		strcpy( ms_cDebugImportantParamsbgR, "0" );
		strcpy( ms_cDebugImportantParamsbgG, "0" );
		strcpy( ms_cDebugImportantParamsbgB, "0" );
		strcpy( ms_cDebugImportantParamsalphaFlash, "80" );
		strcpy( ms_cDebugImportantParamsflashRate, "2" );
		ms_cDebugImportantParamsVibrate = false;
		bank->AddText("BgRFlash", ms_cDebugImportantParamsbgR, sizeof(ms_cDebugImportantParamsbgR), false);
		bank->AddText("BgGFlash", ms_cDebugImportantParamsbgG, sizeof(ms_cDebugImportantParamsbgG), false);
		bank->AddText("BgBFlash", ms_cDebugImportantParamsbgB, sizeof(ms_cDebugImportantParamsbgB), false);
		bank->AddText("BgAlphaFlash", ms_cDebugImportantParamsalphaFlash, sizeof(ms_cDebugImportantParamsalphaFlash), false);
		bank->AddText("BgFlashRate", ms_cDebugImportantParamsflashRate, sizeof(ms_cDebugImportantParamsflashRate), false);
		bank->AddToggle("Vibrate", &ms_cDebugImportantParamsVibrate);
		bank->AddButton("SetImportantParams", &DebugSetImportantParams);
		bank->AddButton("ResetImportantParams", &DebugResetImportantParams);

		bank->AddSeparator();

		ms_iDebugHudColor = static_cast<int>(HUD_COLOUR_BLACK);
		bank->AddSlider("HUD Color", &ms_iDebugHudColor, 0, HUD_COLOUR_MAX_COLOURS - 1, 1);
		bank->AddButton("SetNextPostBackgroundColor", &DebugSetNextPostBackgroundColor);

		bank->AddSeparator();
		
		strcpy( ms_cDebugScriptedMenuHeight, "0.0" );
		bank->AddText("Scripted Menu Height", ms_cDebugScriptedMenuHeight, sizeof(ms_cDebugScriptedMenuHeight), false);
		bank->AddButton("Set Scripted Menu Height", &DebugSetScriptedMenuHeight);

		bank->AddSeparator();

		strcpy( ms_cDebugSpinnerIcon, "1" );
		memset( ms_cDebugSpinnerBody, 0, sizeof(ms_cDebugSpinnerBody) );
		bank->PushGroup("Busy Spinner", false);
			bank->AddButton("On", &DebugSpinnerOn);
			bank->AddButton("Off", &DebugSpinnerOff);
			bank->AddSeparator();
			bank->AddText("Body", ms_cDebugSpinnerBody, sizeof(ms_cDebugSpinnerBody), false);
			bank->AddText("Icon", ms_cDebugSpinnerIcon, sizeof(ms_cDebugSpinnerIcon), false);
		bank->PopGroup();

		// ---
			
		ms_bDebugMessageTextFlash = false;
		ms_bDebugMessageTextOverrideTexture = false;
		strcpy( ms_iDebugMessageTextIcon, "0" );
		memset( ms_cDebugMessageTextMsg, 0, sizeof(ms_cDebugMessageTextMsg) );
		memset( ms_cDebugMessageTextContactTxD, 0, sizeof(ms_cDebugMessageTextContactTxD) );
		memset( ms_cDebugMessageTextContactTxN, 0, sizeof(ms_cDebugMessageTextContactTxN) );
		memset( ms_bDebugMessageTextChrName, 0, sizeof(ms_bDebugMessageTextChrName) );
		memset( ms_bDebugMessageTextSubtitle, 0, sizeof(ms_bDebugMessageTextSubtitle) );
		ms_timeMultiplier = 1.0f;
		memset( ms_cDebugMessageTextCrewName, 0, sizeof(ms_cDebugMessageTextCrewName) );

		bank->PushGroup("Message Text", false);
			bank->AddToggle("Flash", &ms_bDebugMessageTextFlash);
			bank->AddToggle("Override Texture", &ms_bDebugMessageTextOverrideTexture);
			bank->AddButton("Post", &DebugMessageTextDoPost);
			bank->AddSeparator();
			bank->AddText("Title", ms_bDebugMessageTextChrName, sizeof(ms_bDebugMessageTextChrName), false);
			bank->AddText("Subtitle", ms_bDebugMessageTextSubtitle, sizeof(ms_bDebugMessageTextSubtitle), false);
			bank->AddText("Message", ms_cDebugMessageTextMsg, sizeof(ms_cDebugMessageTextMsg), false);
			bank->AddText("Crew Name", ms_cDebugMessageTextCrewName, sizeof(ms_cDebugMessageTextCrewName), false);		
			bank->AddText("ContactTxD", ms_cDebugMessageTextContactTxD, sizeof(ms_cDebugMessageTextContactTxD), false);
			bank->AddText("ContactTxN", ms_cDebugMessageTextContactTxN, sizeof(ms_cDebugMessageTextContactTxN), false);
			bank->AddText("Icon", ms_iDebugMessageTextIcon, sizeof(ms_iDebugMessageTextIcon), false);
			bank->AddText("Icon 2", &ms_iDebugMessageTextIcon2, false);
			bank->AddText("Time Multiplier", &ms_timeMultiplier, false);
		bank->PopGroup();
	
		// ---
	
		ms_bDebugStatsFlash = false;
		memset( ms_cDebugStatsTitle, 0, sizeof(ms_cDebugStatsTitle) );
		memset( ms_cDebugStatsBody, 0, sizeof(ms_cDebugStatsBody) );
		strcpy( ms_cDebugStatsLevelTotal, "1" );
		strcpy( ms_cDebugStatsLevelCurrent, "0" );
		memset( ms_cDebugStatsTxD, 0, sizeof(ms_cDebugStatsTxD) );
		memset( ms_cDebugStatsTxN, 0, sizeof(ms_cDebugStatsTxN) );

		bank->PushGroup("Stats", false);
			bank->AddToggle("Flash", &ms_bDebugStatsFlash);
			bank->AddButton("Post", &DebugStatsDoPost);
			bank->AddSeparator();
			bank->AddText("Title", ms_cDebugStatsTitle, sizeof(ms_cDebugStatsTitle), false);
			bank->AddText("Body", ms_cDebugStatsBody, sizeof(ms_cDebugStatsBody), false);
			bank->AddText("Difference", ms_cDebugStatsLevelTotal, sizeof(ms_cDebugStatsLevelTotal), false);
			bank->AddText("Current Level (0-100)", ms_cDebugStatsLevelCurrent, sizeof(ms_cDebugStatsLevelCurrent), false);
			bank->AddText("Contact TxD", ms_cDebugStatsTxD, sizeof(ms_cDebugStatsTxD), false);
			bank->AddText("Contact TxD", ms_cDebugStatsTxN, sizeof(ms_cDebugStatsTxN), false);
		bank->PopGroup();
	
		// --- TICKER
	
		ms_bDebugTickerFlash = false;
		ms_bDebugTickerIsLocKey = false;
		memset( ms_cDebugTickerBody, 0, sizeof(ms_cDebugTickerBody) );
		ms_bDebugTickerNumIconFlashes = 0;

		bank->PushGroup("Ticker", false);
			bank->AddToggle("Flash", &ms_bDebugTickerFlash);
			bank->AddToggle("Localised String Key?", &ms_bDebugTickerIsLocKey);
			bank->AddToggle("Always Render", &ms_bDebugFeedAlwaysRender);
			bank->AddButton("Post", &DebugTickerDoPost);
			bank->AddSeparator();
			bank->AddText("Body", ms_cDebugTickerBody, sizeof(ms_cDebugTickerBody), false);
			bank->AddText("Number of Icon Flashes", &ms_bDebugTickerNumIconFlashes, false);
		bank->PopGroup();
	
		// --- TICKER F10 [DEBUG ONLY ITEM]
		memset( ms_cDebugTickerF10Title, 0, sizeof(ms_cDebugTickerF10Title) );
		memset( ms_cDebugTickerBody, 0, sizeof(ms_cDebugTickerBody) );
		bank->PushGroup("Ticker F10 [DEBUG ONLY ITEM]", false);
			bank->AddButton("Post", &DebugTickerF10DoPost);
			bank->AddSeparator();
			bank->AddText("Title", ms_cDebugTickerF10Title, sizeof(ms_cDebugTickerF10Title), false);
			bank->AddText("Body", ms_cDebugTickerF10Body, sizeof(ms_cDebugTickerF10Body), false);
		bank->PopGroup();

		memset( ms_cDebugAwardTitle, 0, sizeof(ms_cDebugAwardTitle) );
		memset( ms_cDebugAwardName, 0, sizeof(ms_cDebugAwardName) );
		memset( ms_cDebugAwardTXD, 0, sizeof(ms_cDebugAwardTXD) );
		memset( ms_cDebugAwardTXN, 0, sizeof(ms_cDebugAwardTXN) );
		ms_iAwardXP = 0;
		ms_iAwardColour = (int)HUD_COLOUR_BRONZE;
		bank->PushGroup("Award", false);
			bank->AddButton("Post", &DebugAwardDoPost);
			bank->AddSeparator();
			bank->AddText("Title", ms_cDebugAwardTitle, sizeof(ms_cDebugAwardTitle), false);
			bank->AddText("Award Name", ms_cDebugAwardName, sizeof(ms_cDebugAwardName), false);
			bank->AddText("TXD", ms_cDebugAwardTXD, sizeof(ms_cDebugAwardTXD), false);
			bank->AddText("TXN", ms_cDebugAwardTXN, sizeof(ms_cDebugAwardTXN), false);
			bank->AddText("XP", &ms_iAwardXP, false);
			bank->AddText("Hud Colour", &ms_iAwardColour, false); 
		bank->PopGroup();
	
		// ---

		ms_bDebugCrewTagFlash = false;
		ms_bDebugCrewTagIsPrivate = false;
		ms_bDebugCrewTagShowLogoFlag = false;
		ms_cDebugCrewTagCrewId = 724;  //Crew ID for the RSNE crew.
		ms_uDebugCrewTagCrewColourR = 255;
		ms_uDebugCrewTagCrewColourG = 255;
		ms_uDebugCrewTagCrewColourB = 255;

		memset( ms_cDebugCrewTagCrewString, 0, sizeof(ms_cDebugCrewTagCrewString) );
		strcpy( ms_cDebugCrewTagCrewRank, "0" );
		ms_bDebugCrewTagFounderStatus = false;
		memset( ms_cDebugCrewTagBody, 0, sizeof(ms_cDebugCrewTagBody) );

		bank->PushGroup("Crew Tag", false);
			bank->AddToggle("Flash", &ms_bDebugCrewTagFlash);
			bank->AddButton("Post", &DebugCrewTagDoPost);
			bank->AddSeparator();
			bank->AddToggle("Is Private", &ms_bDebugCrewTagIsPrivate);
			bank->AddToggle("Show Logo Flag", &ms_bDebugCrewTagShowLogoFlag);
			bank->AddText("Crew String", ms_cDebugCrewTagCrewString, sizeof(ms_cDebugCrewTagCrewString), false);
			bank->AddText("Crew Rank", ms_cDebugCrewTagCrewRank, sizeof(ms_cDebugCrewTagCrewRank), false);
			bank->AddToggle("Founder Status", &ms_bDebugCrewTagFounderStatus);
			bank->AddText("Body", ms_cDebugCrewTagBody, sizeof(ms_cDebugCrewTagBody), false);
			bank->AddText("CrewId", &ms_cDebugCrewTagCrewId, false);
			bank->AddText("Crew Colour R (0-255)", &ms_uDebugCrewTagCrewColourR, false);
			bank->AddText("Crew Colour G (0-255)", &ms_uDebugCrewTagCrewColourG, false);
			bank->AddText("Crew Colour B (0-255)", &ms_uDebugCrewTagCrewColourB, false);
		bank->PopGroup();
	
		// ---

		memset( ms_cDebugCrewRankupTitle, 0, sizeof(ms_cDebugCrewRankupTitle) );
		memset( ms_cDebugCrewRankupSubtitle, 0, sizeof(ms_cDebugCrewRankupSubtitle) );
		memset( ms_cDebugCrewRankupTXD, 0, sizeof(ms_cDebugCrewRankupTXD) );
		memset( ms_cDebugCrewRankupTXN, 0, sizeof(ms_cDebugCrewRankupTXN) );

		bank->PushGroup("Crew Rankup", false);
			bank->AddButton("Post", &DebugCrewRankupDoPost);
			bank->AddText("Title", ms_cDebugCrewRankupTitle, sizeof(ms_cDebugCrewRankupTitle), false);
			bank->AddText("Body", ms_cDebugCrewRankupSubtitle, sizeof(ms_cDebugCrewRankupSubtitle), false);
			bank->AddText("TXD", ms_cDebugCrewRankupTXD, sizeof(ms_cDebugCrewRankupTXD), false);
			bank->AddText("TXN", ms_cDebugCrewRankupTXN, sizeof(ms_cDebugCrewRankupTXN), false);
		bank->PopGroup();

		// ---

		memset( ms_cDebugVersusCh1TXD, 0, sizeof(ms_cDebugVersusCh1TXD) );
		memset( ms_cDebugVersusCh1TXN, 0, sizeof(ms_cDebugVersusCh1TXN) );
		ms_iDebugVersusVal1 = 0;
		memset( ms_cDebugVersusCh2TXD, 0, sizeof(ms_cDebugVersusCh2TXD) );
		memset( ms_cDebugVersusCh2TXN, 0, sizeof(ms_cDebugVersusCh2TXN) );
		ms_iDebugVersusVal2 = 0;
		ms_iCustomColor1 = -1;
		ms_iCustomColor2 = -1;

		bank->PushGroup("Versus", false);
			bank->AddButton("Post", &DebugVersusDoPost);
			bank->AddText("Character 1 TXD", ms_cDebugVersusCh1TXD, sizeof(ms_cDebugVersusCh1TXD), false);
			bank->AddText("Character 1 TXN", ms_cDebugVersusCh1TXN, sizeof(ms_cDebugVersusCh1TXN), false);
			bank->AddText("Character 1 Value", &ms_iDebugVersusVal1, false);
			bank->AddText("Character 2 TXD", ms_cDebugVersusCh2TXD, sizeof(ms_cDebugVersusCh2TXD), false);
			bank->AddText("Character 2 TXN", ms_cDebugVersusCh2TXN, sizeof(ms_cDebugVersusCh2TXN), false);
			bank->AddText("Character 2 Value", &ms_iDebugVersusVal2, false);
			bank->AddText("Custom Color Enum 1", &ms_iCustomColor1, false);
			bank->AddText("Custom Color Enum 1", &ms_iCustomColor2, false);
		bank->PopGroup();

		bank->PushGroup("Replay", false);
			bank->AddButton("Post", &DebugReplayDoPost);
			bank->AddText("Title", ms_cDebugReplayTitle, sizeof(ms_cDebugReplayTitle), false);
			bank->AddText("Subtitle", ms_cDebugReplaySubtitle, sizeof(ms_cDebugReplaySubtitle), false);
			bank->AddToggle("Buffer Full", &ms_bDebugReplayBufferFullFlash);
			bank->AddSlider("Type", &ms_iDebugReplayType, CGameStream::DIRECTOR_RECORDING, CGameStream::MAX_REPLAY_STATES - 1, 1);
			bank->AddSlider("Button Icon", &ms_iDebugReplayButtonIcon, ARROW_UP, MAX_INSTRUCTIONAL_BUTTONS, 1);
			bank->AddText("Button Token (overrides icon)", ms_cDebugReplayButtonToken, sizeof(ms_cDebugReplayButtonToken), false);
			bank->AddSlider("Percentage", &ms_fDebugReplayPercentage, 0.0f, 1.0f, .1f);
		bank->PopGroup();

		// ---
		ms_bDebugUnlockFlash = false;

		memset( ms_cDebugUnlockTitle, 0, sizeof(ms_cDebugUnlockTitle) );
		memset( ms_cDebugUnlockBody, 0, sizeof(ms_cDebugUnlockBody) );
		ms_iUnlockTypeIcon = 0;
		ms_eDebugUnlockTitleColour = 0;

		bank->PushGroup("Unlock", false);
			bank->AddToggle("Flash", &ms_bDebugUnlockFlash);
			bank->AddButton("Post", &DebugUnlockDoPost);
			bank->AddSeparator();
			bank->AddText("Title", ms_cDebugUnlockTitle, sizeof(ms_cDebugUnlockTitle), false);
			bank->AddText("Title Colour", &ms_eDebugUnlockTitleColour, false);
			bank->AddText("Body", ms_cDebugUnlockBody, sizeof(ms_cDebugUnlockBody), false);
			bank->AddText("Icon (0-11)", &ms_iUnlockTypeIcon, false);
		bank->PopGroup();

		// ---
		strcpy(ms_cDebugGameTip,"0");

		bank->PushGroup("Gametips", false);
			bank->AddButton("Post Random Gametip", &DebugGameTipRandomPost);
			bank->AddButton("Post Specified Gametip", &DebugGameTipPost);
			bank->AddText("Specified Gametip", ms_cDebugGameTip, sizeof(ms_cDebugGameTip), false);
		bank->PopGroup();

		// ---

	bank->PopGroup(); // "TheFeed"
	ms_bBankWidgetsCreated = true;
}

// ---------------------------------------------------------
void CGameStreamMgr::InitWidgets()
{
	ms_bBankWidgetsCreated = false;

	bkBank* pBank = BANKMGR.FindBank( "ui" );
	if( pBank == NULL )
	{
		pBank = &BANKMGR.CreateBank( "ui" );
	}

	if( pBank != NULL )
	{
		pBank->AddButton("Create TheFeed widgets", &DebugCreateTheFeedBankWidgets);
	}

	ms_iDebugAutoTestPost = 400;
	ms_iDebugAutoTestCount = 1;
	ms_bDebugTestFeed = false;
}

// ---------------------------------------------------------
void CGameStreamMgr::ShutdownWidgets()
{
	bkBank *bank = BANKMGR.FindBank( "ui" );
	if( bank != NULL )
	{
		bank->Destroy();
	}
}

const char* CGameStreamMgr::GetLocAttempt(const char* stringToTest)
{
	return (TheText.DoesTextLabelExist(stringToTest) ? TheText.Get(stringToTest) : stringToTest);
}

#endif

void CGameStreamMgr::HandleTooltipXml( parTreeNode* pGameStreamNode )
{
	parTreeNode* pNode = NULL;
	while ((pNode = pGameStreamNode->FindChildWithName("ToolTip", pNode)) != NULL)
	{
		sToolTip toolTip;
		toolTip.tipId = pNode->GetElement().FindAttributeAnyCase("stringId")->FindIntValue();

		atString tipTypeString(pNode->GetElement().FindAttributeAnyCase("tipType")->GetStringValue());

		bool addToSPArray = false;
		bool addToMPArray = false;
		bool addToReplayArray = false;
		bool showOnCurrentPlatform = false;

		parAttribute* pPlatformAttribute = pNode->GetElement().FindAttributeAnyCase("platform");
		if (pPlatformAttribute)
		{
			atString platformString(pPlatformAttribute->GetStringValue());

			if (platformString == RSG_PLATFORM_ID ||
				platformString == "")
			{
				showOnCurrentPlatform = true;
			}
		}
		else
		{
			// If the platformSpecific attribute is missing, the tooltip is not platform-specific
			showOnCurrentPlatform = true;
		}

		if (!showOnCurrentPlatform)
		{
			continue;
		}

#if KEYBOARD_MOUSE_SUPPORT
		parAttribute* pInputDeviceAttribute = pNode->GetElement().FindAttributeAnyCase("inputDevice");
		if (pInputDeviceAttribute)
		{
			int inputDevice = pInputDeviceAttribute->FindIntValue();
			toolTip.inputDevice = inputDevice;
		}
		else
		{
			toolTip.inputDevice = 0;
		}
#endif // KEYBOARD_MOUSE_SUPPORT

#if RSG_PC
		parAttribute* pShowOnLoadAttribute = pNode->GetElement().FindAttributeAnyCase("showOnLoad");
		if (showOnCurrentPlatform && pShowOnLoadAttribute)
		{
			bool showOnLoad = pShowOnLoadAttribute->FindBoolValue();

			if (showOnLoad)
			{
				m_ShowOnLoadToolTips.PushAndGrow(toolTip);
			}
		}
#endif // RSG_PC

		if ( tipTypeString == "SP" )
		{
			addToSPArray = true;
		}
		else if ( tipTypeString == "MP" )
		{
			addToMPArray = true;
		}
		else if ( tipTypeString == "SPMP")
		{
			addToSPArray = true;
			addToMPArray = true;
		}
		else if ( tipTypeString == "REPLAY")
		{
			addToReplayArray = true;
		}
		else
		{
			uiAssertf(0, "Found unsupported GameStream tipType == %s", tipTypeString.c_str());
		}

		if (addToSPArray)
		{
			m_SinglePlayerToolTips.PushAndGrow(toolTip);
		}

		if (addToMPArray)
		{
			m_MultiPlayerToolTips.PushAndGrow(toolTip);
		}

		if (addToReplayArray)
		{
			m_ReplayToolTips.PushAndGrow(toolTip);
		}
	}

#if RSG_PC
	if (m_ShowOnLoadToolTips.GetCount() > 0)
	{
		// There are tips that must be shown when the game loads; reinitialize to a valid index
		ms_ShowOnLoadTipIdIndex = 0;
	}
#endif // RSG_PC

	uiDebugf3("ToolTips data read complete. SP Tips: %d MP Tips: %d",m_SinglePlayerToolTips.GetCount(),m_MultiPlayerToolTips.GetCount());
}

#if RSG_PC
void CGameStreamMgr::DeviceLost()
{
}

void CGameStreamMgr::DeviceReset()
{
	if (ms_pGameStream)
		ms_pGameStream->SetDisplayConfigChanged();
}
#endif // RSG_PC

#if KEYBOARD_MOUSE_SUPPORT
bool CGameStreamMgr::GetIsToolTipKeyboardSpecific( int index )
{
	for ( int i = 0; i < m_MultiPlayerToolTips.GetCount(); i++ )
	{
		if ( index == m_MultiPlayerToolTips[i].tipId &&
			 m_MultiPlayerToolTips[i].inputDevice == ioSource::IOMD_KEYBOARD_MOUSE )
		{
			return true;
		}
	}

	for ( int i = 0; i < m_SinglePlayerToolTips.GetCount(); i++ )
	{
		if ( index == m_SinglePlayerToolTips[i].tipId &&
			 m_SinglePlayerToolTips[i].inputDevice == ioSource::IOMD_KEYBOARD_MOUSE )
		{
			return true;
		}
	}

	for ( int i = 0; i < m_ReplayToolTips.GetCount(); i++ )
	{
		if ( index == m_ReplayToolTips[i].tipId &&
			 m_ReplayToolTips[i].inputDevice == ioSource::IOMD_KEYBOARD_MOUSE )
		{
			return true;
		}
	}

	return false;
}
#endif // KEYBOARD_MOUSE_SUPPORT

#if RSG_PC
int CGameStreamMgr::GetNextShowOnLoadTipId()
{
	int returnId = -1;

	if ( ms_ShowOnLoadTipIdIndex >= 0 && ms_ShowOnLoadTipIdIndex < m_ShowOnLoadToolTips.GetCount() )
	{
		returnId = m_ShowOnLoadToolTips[ms_ShowOnLoadTipIdIndex].tipId;
		ms_ShowOnLoadTipIdIndex++;
	}

	return returnId;
}
#endif // RSG_PC

int CGameStreamMgr::GetMpToolTipId( int index )
{
	int returnId = -1;

	if ( index >= 0 && index < m_MultiPlayerToolTips.GetCount() )
	{
		returnId = m_MultiPlayerToolTips[index].tipId;
	}
	
	return returnId;
}

int CGameStreamMgr::GetSpToolTipId( int index )
{
	int returnId = -1;

	if ( index >= 0 && index < m_SinglePlayerToolTips.GetCount() )
	{
		returnId = m_SinglePlayerToolTips[index].tipId;
	}
	
	return returnId;
}

int CGameStreamMgr::GetReplayToolTipId( int index)
{
	if ( index >= 0 && index < GetNumReplayToolTips() )
	{
		return m_ReplayToolTips[index].tipId;
	}

	return -1;
}

//////////////////////////////////////////////////////////////////////////
//This is a SHAMELESS rip of the frontend XML stuff.
CGamestreamXMLMgr::XMLLoadTypeMap CGamestreamXMLMgr::sm_gsXmlLoadMap;
bool							CGamestreamXMLMgr::sm_bGSXmlLoadedOnce = false;

void CGamestreamXMLMgr::RegisterXMLReader(atHashWithStringBank objName, datCallback handleFunc)
{
	sm_gsXmlLoadMap[objName] = handleFunc;
}

void CGamestreamXMLMgr::LoadXML(bool bForceReload)
{
	if(sm_bGSXmlLoadedOnce && !bForceReload )
		return;

	uiDebugf3("==================================");
	uiDebugf3("==== GAMESTREAM DATA SETUP ====");
	uiDebugf3("==================================");

#define GAMESTREAM_XML_FILENAME	"common:/data/ui/gamestream.xml"

	INIT_PARSER;

	parTree* pTree = PARSER.LoadTree(GAMESTREAM_XML_FILENAME, "xml");

	if (pTree)
	{
		parTreeNode* pRootNode = pTree->GetRoot();

		parTreeNode::ChildNodeIterator pChildrenStart = pRootNode->BeginChildren();
		parTreeNode::ChildNodeIterator pChildrenEnd = pRootNode->EndChildren();

		for(parTreeNode::ChildNodeIterator ci = pChildrenStart; ci != pChildrenEnd; ++ci)
		{
			parTreeNode* curNode = *ci;
			atHashWithStringBank curNodeName(curNode->GetElement().GetName());

			datCallback* pReader = sm_gsXmlLoadMap.Access(curNodeName);
			if( uiVerifyf(pReader, "Encountered a node in gamestream.xml named '%s' without a valid reader attached to it!", curNodeName.GetCStr() ) )
			{
				pReader->Call( CallbackData(curNode) );
			}
		}

		delete pTree;
	}
	else
	{
		Assertf(0, "Missing GAMESTREAM.XML file!");
	}


	sm_bGSXmlLoadedOnce = true;
	SHUTDOWN_PARSER;

#if !__BANK
	// if there's no chance of forcing a reload, wipe out the mapping table
	sm_gsXmlLoadMap.Kill();
#endif

	uiDebugf3("==================================");
	uiDebugf3("== END GAMESTREAM DATA SETUP ==");
	uiDebugf3("==================================");
}
