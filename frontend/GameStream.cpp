#
/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : GameStream.h
// PURPOSE : handles the Scaleform hud live info/updates
//
// See: http://rsgediwiki1/wiki/index.php/Scaleform_Game_Stream for reference.
//
/////////////////////////////////////////////////////////////////////////////////

#include "diag/channel.h"
#include "rline/rl.h"
#include "rline/ros/rlros.h"
#include "rline/cloud/rlcloud.h"
#include "parser/manager.h"
#include "grcore/quads.h"
#include "grcore/viewport.h"
#include "fwsys/timer.h"
#include "fwscript/scriptguid.h"
#include "system/simpleallocator.h"
#include "frontend/PauseMenu.h"
#include "frontend/Scaleform/ScaleFormMgr.h"
#include "frontend/ui_channel.h"
#include "frontend/Loadingscreens.h"
#include "frontend/NewHud.h"
#include "frontend/MiniMap.h"
#include "frontend/Credits.h"
#include "frontend/GameStreamMgr.h"
#include "frontend/Store/StoreScreenMgr.h"
#include "frontend/SocialClubMenu.h"
#include "frontend/MovieClipText.h"
#include "renderer/rendertargets.h"
#include "Cutscene/CutsceneManagerNew.h"
#include "text/TextFile.h"
#include "GameStream.h"
#include "Network/NetworkInterface.h"
#include "system/controlMgr.h"

FRONTEND_OPTIMISATIONS()
NETWORK_OPTIMISATIONS()
//OPTIMISATIONS_OFF()

#include "frontend/ui_channel.h"

RAGE_DEFINE_SUBCHANNEL(ui, gamestream)
#undef __ui_channel
#define __ui_channel ui_gamestream

const int ELIPSES_LENGTH = 4;

// Move to frontend.xml at some point?
gstPostStaticInfo s_PostTypeStaticData[] = 
{
	//{TYPE,							ClassName		UPDATE_METHOD,			SHOWTIME}
	{GAMESTREAM_TYPE_MESSAGE_TEXT,		"MSG_TXT",		"",						GAMESTREAM_SHOWTIME_MESSAGE_TEXT},
	{GAMESTREAM_TYPE_STATS,				"STATS",		"UPDATE_STREAM_STATS",	GAMESTREAM_SHOWTIME_DEFAULT},
	{GAMESTREAM_TYPE_TICKER,			"TICKER",		"UPDATE_STREAM_TICKER",	GAMESTREAM_SHOWTIME_DEFAULT},
	{GAMESTREAM_TYPE_AWARD,				"AWARD",		"",						GAMESTREAM_SHOWTIME_DEFAULT},
	{GAMESTREAM_TYPE_CREW_TAG,			"CREW",			"",						GAMESTREAM_SHOWTIME_DEFAULT},
	{GAMESTREAM_TYPE_UNLOCK,			"UNLOCK",		"",						GAMESTREAM_SHOWTIME_DEFAULT},
	{GAMESTREAM_TYPE_TOOLTIPS,			"TOOLTIPS",		"",						GAMESTREAM_SHOWTIME_DEFAULT},
	{GAMESTREAM_TYPE_CREW_RANKUP,		"CREW_RANKUP",	"",						GAMESTREAM_SHOWTIME_DEFAULT},
	{GAMESTREAM_TYPE_VERSUS,			"VERSUS",		"",						GAMESTREAM_SHOWTIME_DEFAULT},
	{GAMESTREAM_TYPE_REPLAY,			"REPLAY",		"",						GAMESTREAM_SHOWTIME_DEFAULT}	// SHOWTIME is irrelevant for REPLAY as these posts are auto-frozen to the feed
};
CompileTimeAssert(NELEM(s_PostTypeStaticData) == GAMESTREAM_TYPE_COUNT	);

// Important parameters defaults.
const int DEFAULT_FEED_RED_VALUE = 0;
const int DEFAULT_FEED_GREEN_VALUE = 0;
const int DEFAULT_FEED_BLUE_VALUE = 0;
const int DEFAULT_FEED_ALPHA_VALUE = 80;
const int DEFAULT_FEED_FLASH_RATE_VALUE = 2;


// ---------------------------------------------------------
CGameStream::CGameStream()
: m_IsActive(false)
, m_IsPaused(false)
, m_DisableAdd(0)
, m_CurrentTime(0)
, m_CurrentTimeOffset(0)
, m_TimePausedAt(0)
, m_CurrentSystemTime(0)
, m_BackoffTime(0)
, m_Sequence(0)
, m_iPauseMenuCount(0)
, m_iLastShownPhoneActivatableID(-1)
, m_BusySpinnerOn(false)
, m_BusySpinnerOnCmp(false)
, m_BusySpinnerUpd(false)
, m_BusySpinnerBackoffFrames(0)
, m_DisplayConfigChanged(true)
, m_ChangeMovieParams(false)
, m_MinimapState(DISPLAY_STATE_MAP_OFF)
, m_MinimapStateCmp(DISPLAY_STATE_MAP_OFF)
, m_HelptextHeight(0.0f)
, m_HelptextHeightCmp(0.0f)
, m_ScriptedMenuHeight(0.0f)
, m_ScriptedMenuHeightCmp(0.0f)
, m_IsOnLoadingScreen(false)
/*, m_HideUntilFlushComplete(false)*/
, m_IsLogoVisible(false)
, m_ImportantParamsbgR(DEFAULT_FEED_RED_VALUE)
, m_ImportantParamsbgG(DEFAULT_FEED_GREEN_VALUE)
, m_ImportantParamsbgB(DEFAULT_FEED_BLUE_VALUE)
, m_ImportantParamsalphaFlash(DEFAULT_FEED_ALPHA_VALUE)
, m_ImportantParamsflashRate(DEFAULT_FEED_FLASH_RATE_VALUE)
, m_IsForcedRender(false)
, m_ImportantParamsvibrate(false)
, m_IsShowing(false)
, m_AutoPostGameTip(false)
, m_HasShownFirstAutoPostedGameTip(false)
, m_bFreezeNextPost(false)
, m_bSnapFeedItemPositions(false)
, m_bSetFeedBackgroundColor(false)
, m_AutoPostGameTipType(TIP_TYPE_ANY)
, m_IdCounter( GAMESTREAM_MAX_POSTS )
, m_GameTipLast( -1 )
, m_OnlineTipLast( -1 )
#if __BANK
, m_bRenderedLastFrame(false)
, m_bShowDebugBounds(false)
#endif
{
	for( int i=0; i!=GAMESTREAM_MAX_POSTS; i++ )
	{
		m_PostList[i].Reset();
	}

	m_EnableGlobal = true;
	for( int m=0; m!=GAMESTREAM_TYPE_COUNT; m++ )
	{
		m_EnableList[m] = true;
	}

	for( int s=0; s!=GAMESTREAM_SHOW_FLAG_COUNT; s++ )
	{
		m_ShowCounters[s] = 0;
	}

	m_GameTipHistoryIndex = 0;
	for( int t=0; t!=GAMESTREAM_GAMETIP_HISTORY_MAX; t++ )
	{
		m_GameTipHistory[t] = GAMESTREAM_GAMETIP_HISTORY_UNSET;
	}

	PsSet( &m_MinimapCur, 0.0f, 0.0f, 0.0f, 0.0f );
	PsSet( &m_MinimapNew, 0.0f, 0.0f, 0.0f, 0.0f );
	
	m_BusySpinnerTxt[0]='\0';

#if __BANK
	m_previousRenderStateReason[0]='\0';
#endif

	m_FeedCache.Reset();
	m_SysTimer.Reset();
	m_GameTipTimer.Reset();
	m_IsActive = true;
}

// ---------------------------------------------------------
CGameStream::~CGameStream()
{
	if( m_IsActive )
	{
		for( int i=0; i!=GAMESTREAM_MAX_POSTS; i++ )
		{
			if( m_PostList[i].Status != GAMESTREAM_STATUS_FREE )
			{
				FreePost( i );
			}
		}
		m_IsActive = false;
	}
}

// ---------------------------------------------------------
void CGameStream::Update()
{
	if( !m_IsActive )
	{
		return;
	}

	if( !IsSafeGameDisplayState() )
	{
		m_ShowCounters[GAMESTREAM_SHOW_IDX_INT1] = 4;
	}

	/*
	if( m_HideUntilFlushComplete )
	{
		if( CountShowing() == 0 )
		{
			m_HideUntilFlushComplete = false;
		}
	}
	*/
	u32 sysTime =  (u32)m_SysTimer.GetMsTime();

	if( m_AutoPostGameTip )
	{
		AutoPostGameTipUpdate();
	}

	m_CurrentSystemTime = m_SysTimer.GetSystemMsTime();

	if( m_IsPaused && !m_IsForcedRender )
	{
		m_CurrentTimeOffset = sysTime-m_CurrentTime;
	}
	else
	{
		m_CurrentTime = sysTime-m_CurrentTimeOffset;
	}

	for( int i=0; i!=GAMESTREAM_MAX_POSTS; i++)
	{
		if( m_PostList[i].Status != GAMESTREAM_STATUS_FREE )
		{
			ProcessPost( i, &m_PostList[i] );
		}
	}
}

// Set everything back to how it was before the movie got unloaded.
// ---------------------------------------------------------
void CGameStream::JustReloaded()
{
	m_MinimapStateCmp = DISPLAY_STATE_MAP_OFF;
	SetDisplayConfigChanged();
	DefaultHidden();
}

// ---------------------------------------------------------
void CGameStream::DefaultHidden()
{
	m_IsShowing = false;
}

// ---------------------------------------------------------
void CGameStream::SetTipsLast( int gameTipLast, int onlineTipLast )
{
	m_GameTipLast = gameTipLast;
	m_OnlineTipLast = onlineTipLast;
}

// SHOW/HIDE only enables/disables the audio triggers now.
// ---------------------------------------------------------
bool CGameStream::UpdateShowHide( s32 MovieId )
{
	bool shouldRender = true;

	/*
	if( m_HideUntilFlushComplete )
	{
		shouldRender = false;
	}
	else
	{
	*/
		for( int i=0; i!=GAMESTREAM_SHOW_FLAG_COUNT; i++ )
		{ 
			if( m_ShowCounters[i] != 0 )
			{
				shouldRender = false;
				break;
			}
		}
	/*
	}
	*/
	if( shouldRender )
	{
		if(	!m_IsShowing )
		{
			if(CScaleformMgr::BeginMethod(MovieId, SF_BASE_CLASS_GAMESTREAM, "SHOW"))
			{
				uiDebugf3("CGameStream::UpdateShowHide -- SHOW called.");
				m_IsShowing = true;
				CScaleformMgr::EndMethod();
				return( true );
			}
		}
	}
	else
	{
		if(	m_IsShowing )
		{
			if(CScaleformMgr::BeginMethod(MovieId, SF_BASE_CLASS_GAMESTREAM, "HIDE"))
			{
				uiDebugf3("CGameStream::UpdateShowHide -- HIDE called.");
				m_IsShowing = false;
				CScaleformMgr::EndMethod();
				return( true );
			}
		}	
	}
	return( false );
}

// ---------------------------------------------------------
void CGameStream::ForceRenderOn()
{
	m_IsForcedRender = true;
}

// ---------------------------------------------------------
void CGameStream::ForceRenderOff()
{
	m_IsForcedRender = false;
}

// ---------------------------------------------------------
bool CGameStream::IsForceRenderOn() const
{
	return(m_IsForcedRender);
}

// ---------------------------------------------------------
void CGameStream::Pause( void )
{
	if (!m_IsPaused)
	{
		if (!CLoadingScreens::AreActive())
		{
			m_IsPaused = true;
			m_TimePausedAt = (u32)m_SysTimer.GetMsTime();
			uiDebugf3("CGameStream::Pause");
		}
	}
}

// ---------------------------------------------------------
void CGameStream::Resume( void )
{
	if (m_IsPaused)
	{
		m_IsPaused = false;
		uiDebugf3("CGameStream::Resume");
	}
}

// ---------------------------------------------------------
void CGameStream::HideForUpdates( int updates )
{ 
	m_ShowCounters[GAMESTREAM_SHOW_IDX_EXT4] = updates;
}

// ---------------------------------------------------------
void CGameStream::Hide( void )
{ 
	m_ShowCounters[GAMESTREAM_SHOW_IDX_EXT1] = GAMESTREAM_SHOWCOUNT_FOREVER;
}

// ---------------------------------------------------------
void CGameStream::Show( void )
{
	m_ShowCounters[GAMESTREAM_SHOW_IDX_EXT1] = 0;
}

// ---------------------------------------------------------
void CGameStream::AutoPostGameTipOn(eGameStreamTipType tipType/* = TOOLTIP_ANY*/)
{
	m_AutoPostGameTip = true;
	m_AutoPostGameTipType = tipType;
	m_HasShownFirstAutoPostedGameTip = false;
}

// ---------------------------------------------------------
void CGameStream::AutoPostGameTipOff()
{
	m_AutoPostGameTip = false;
}

void CGameStream::HandleShownStreamItem(int Id, int contentStrLen)
{
	for( int i = 0; i < GAMESTREAM_MAX_POSTS; ++i )
	{
		gstPost& post = m_PostList[i];

		if(post.Id == Id)
		{
			// Game_stream.gfx trimmed "content" text, so overwrite showtime based on the new trimmed character length of content
			if(contentStrLen != -1)
			{
				post.ShowTime = GetPostDefaultShowTimeAtIndex(i) + MsFromLength(contentStrLen);
			}

			// Feed item was shown, cache message for pause menu -- The proper way to cache feed items as the item is cached after it's shown onscreen, and not before it could potentially fail one of 100 ways
			// Just a shell for now until we have time to refactor
			/*
			if(post.m_bCacheMessage)
			{
				switch(post.Type)
				{
					default:
					{
						break;
					}
				}
			}
			*/

			// Update the last phone activatable ID
			if(post.Type == GAMESTREAM_TYPE_MESSAGE_TEXT)
			{
				if(post.Params[MESSAGE_TEXT_PARAM_TYPE][0] == 'I') // ensure we are working with an INT
				{
					int msgType = atoi( &post.Params[MESSAGE_TEXT_PARAM_TYPE][1]);

					if(msgType == MESSAGE_TEXT_TYPE_MSG || 
						msgType == MESSAGE_TEXT_TYPE_EMAIL ||
						msgType == MESSAGE_TEXT_TYPE_INV)
					{
						m_iLastShownPhoneActivatableID = post.Id; 
					}
				}
			}

			break;
		}
	}
}

u32 CGameStream::GetPostDefaultShowTimeAtIndex(int i)
{
	if(uiVerify(i >= 0 && i < GAMESTREAM_MAX_POSTS) && uiVerifyf(m_PostList[i].m_pStaticData != NULL, "Feed item id=%d does not have valid static data", i))
	{
		return m_PostList[i].m_pStaticData->m_ShowTime;
	}

	return 0;
}

bool CGameStream::GetRenderEnabledGlobal()
{
	return m_ShowCounters[GAMESTREAM_SHOW_IDX_EXT3] == 0;
}


// From script: CommandDisplayHud
// ---------------------------------------------------------
void CGameStream::SetRenderEnabledGlobal( bool IsEnabled )
{
	if( IsEnabled )
	{
		m_ShowCounters[GAMESTREAM_SHOW_IDX_EXT3] = 0;
	}
	else
	{
		m_ShowCounters[GAMESTREAM_SHOW_IDX_EXT3] = GAMESTREAM_SHOWCOUNT_FOREVER;
	}
}

// ---------------------------------------------------------
void CGameStream::HideThisUpdate( void )
{
	m_ShowCounters[GAMESTREAM_SHOW_IDX_EXT2] = 4;
}

// ---------------------------------------------------------
void CGameStream::FlushQueue( bool UNUSED_PARAM(bContinueToShow) )
{
	if (!m_AutoPostGameTip)
	{
		uiDebugf3("CGameStream::FlushQueue");
		for( int i=0; i!=GAMESTREAM_MAX_POSTS; i++ )
		{
			if(!m_PostList[i].m_bAlwaysRender && !m_PostList[i].IsFrozen())
			{
				RemoveItem( i );
			}
		}
	}

	return;
}

void CGameStream::ForceFlushQueue()
{
	uiDebugf3("CGameStream::ForceFlushQueue");
	for( int i=0; i!=GAMESTREAM_MAX_POSTS; i++ )
	{
		RemoveItem( i );
	}

	return;
}

// ---------------------------------------------------------
void CGameStream::RemoveItem( int Id )
{
	Id = ConvertToIndex( Id );

	if( Id>=0 && Id<GAMESTREAM_MAX_POSTS )
	{
		switch( m_PostList[Id].Status )
		{
			case GAMESTREAM_STATUS_SHOWING:
				m_PostList[Id].Status = GAMESTREAM_STATUS_READYTOREMOVE;
				break;
			case GAMESTREAM_STATUS_NEW:
			case GAMESTREAM_STATUS_DOWNLOADINGTEXTURE:
			case GAMESTREAM_STATUS_REQUESTING_CREW_EMBLEM:
			case GAMESTREAM_STATUS_READYTOSHOW:
				SetAlarm( Id, GAMESTREAM_LINGER_TIME );
				m_PostList[Id].Status = GAMESTREAM_STATUS_CLEANUP;
				break;
			default:
				break;
		}
	}
}

// ---------------------------------------------------------
int CGameStream::CreateNewId()
{
	if( m_IdCounter >= INT_MAX )
	{
		m_IdCounter = GAMESTREAM_MAX_POSTS;
	}
	
	int ret = m_IdCounter;
	m_IdCounter++;
	return( ret );
}

// ---------------------------------------------------------
int CGameStream::ConvertToIndex( int Id )
{
	if( Id >= GAMESTREAM_MAX_POSTS )
	{
		for( int i=0; i!=GAMESTREAM_MAX_POSTS; i++ )
		{
			if( m_PostList[i].IdScript == Id )
			{
				return( i );
			}
		}
		return( -1 );
	}
	return( Id );
}

// ---------------------------------------------------------
void CGameStream::EnableGlobal( bool Enable )
{
	m_EnableGlobal = Enable;
}

// ---------------------------------------------------------
void CGameStream::EnableFeedType( int FeedType, bool Enable )
{
	if( uiVerify(FeedType > GAMESTREAM_TYPE_INVALID && FeedType < GAMESTREAM_TYPE_COUNT))
	{
		m_EnableList[FeedType] = Enable;
	}
}


void CGameStream::EnableAllFeedTypes( bool Enable )
{
	for( int m=0; m!=GAMESTREAM_TYPE_COUNT; m++ )
	{
		EnableFeedType(m, Enable);
	}
}

// ---------------------------------------------------------
void CGameStream::ReportMinimapPosAndSize( float x, float y, float w, float h )
{
	PsSet( &m_MinimapNew, x, y, w, h );
}

// ---------------------------------------------------------
void CGameStream::GetMinimapPosAndSize( float* x, float* y, float* w, float* h )
{
	*x = m_MinimapCur.x;
	*y = m_MinimapCur.y;
	*w = m_MinimapCur.w;
	*h = m_MinimapCur.h;
}

// ---------------------------------------------------------
bool CGameStream::CanUseScaleform()
{
	if( m_CurrentTime >= m_BackoffTime )
	{
		m_BackoffTime = (m_CurrentTime+GAMESTREAM_BACKOFF_TIME);
		return true;
	}

	if( HasAlwaysRenderFeedItem() )
	{
		return true;
	}

	return false;
}

bool CGameStream::HasAlwaysRenderFeedItem()
{
	for(int i = 0; i < GAMESTREAM_MAX_POSTS; ++i)
	{
		if(m_PostList[i].m_bAlwaysRender)
		{
			return true;
		}
	}

	return false;
}

//Get a pointer to the beginning of the path, less the device
// device
const char* GetStringAfterDevice(const char* fullName)
{
	const char* pStr = strstr(fullName, ":/");

	//make sure there is something aftaer :/
	if (pStr && strlen(pStr) > 1)
	{
		//Return the next char
		return pStr+1;
	}

	return NULL;
	
}

// ---------------------------------------------------------
void CGameStream::ProcessPost( int Idx, gstPost* Post )
{
	switch( Post->Status )
	{
		case GAMESTREAM_STATUS_NEW:
			{
				if( Post->m_bDynamicTexture )
				{
					int PrmIdxTxN = FindParamType(Post,'n');
					const char* fileName =  &Post->Params[PrmIdxTxN][1];

					if(uiVerifyf(Post->RequestTextureForDownload(fileName, Post->m_cloudDownloadpath), "CGameStream::ProcessPost -- Failed to start download of %s for gamestream IDX %s", Post->m_cloudDownloadpath.c_str(), Post->GetTypeName()))
					{
						Post->Status = GAMESTREAM_STATUS_DOWNLOADINGTEXTURE;
					}
					else
					{
						uiErrorf("CGameStream::ProcessPost -- Failed to start download of %s for gamestream IDX %s", Post->m_cloudDownloadpath.c_str(), Post->GetTypeName());
						FreePost( Idx );
					}
				}
				else if (Post->Type == GAMESTREAM_TYPE_CREW_TAG)
				{
					bool success = false;
					if (Post->m_clanId != RL_INVALID_CLAN_ID)
					{
						//Check to see if the crew emblem is already available.
						NetworkCrewEmblemMgr &crewEmblemMgr = NETWORK_CLAN_INST.GetCrewEmblemMgr();
						EmblemDescriptor emblemDesc(EmblemDescriptor::EMBLEM_CLAN, (EmblemId)Post->m_clanId, RL_CLAN_EMBLEM_SIZE_128);

						if(crewEmblemMgr.RequestEmblem(emblemDesc  ASSERT_ONLY(, "CGameStream")))
						{
							success = true;
							Post->m_bClanEmblemRequested = true;
							Post->Status = GAMESTREAM_STATUS_REQUESTING_CREW_EMBLEM;
						}
					}

					if (!success)
					{
						uiDebugf1("CGameStream::ProcessPost -- Failed to request crew emblem for %d", (int)Post->m_clanId);
						FreePost(Idx);
					}

				}
				else
				{
					if(IsPostReadyToShow(Post))
					{
						Post->Status = GAMESTREAM_STATUS_READYTOSHOW;
						Post->iReadyToShowTime = m_SysTimer.GetSystemMsTime();
					}

				}
			}
			break;

		case GAMESTREAM_STATUS_DOWNLOADINGTEXTURE:
			{
				//This return true if something happened.
				//False means it's still waiting.
				if (Post->UpdateTextureDownloadStatus())
				{
					if( !DOWNLOADABLETEXTUREMGR.IsRequestPending(Post->m_requestHandle) )
					{
						if (Post->m_TextureDictionarySlot.Get() >= 0)
						{
							Post->Status = GAMESTREAM_STATUS_READYTOSHOW;
							Post->iReadyToShowTime = m_SysTimer.GetSystemMsTime();
						}
						else
						{
							uiErrorf("CGameStream::ProcessPost -- Failed to download texture %s for gamestream idx %s",  Post->m_cloudDownloadpath.c_str(), Post->GetTypeName()); 
							FreePost(Idx);
						}
					}
				}
			}
			break;

		case GAMESTREAM_STATUS_REQUESTING_CREW_EMBLEM:
			{	
				bool bRequestValid = Post->m_bClanEmblemRequested && Post->m_clanId != RL_INVALID_CLAN_ID;
				if(bRequestValid)
				{
					NetworkCrewEmblemMgr &crewEmblemMgr =NETWORK_CLAN_INST.GetCrewEmblemMgr();
					EmblemDescriptor emblemDesc(EmblemDescriptor::EMBLEM_CLAN, (EmblemId)Post->m_clanId, RL_CLAN_EMBLEM_SIZE_128);

					if(!crewEmblemMgr.RequestPending(emblemDesc))
					{
						if(crewEmblemMgr.RequestSucceeded(emblemDesc))
						{
							Post->Status = GAMESTREAM_STATUS_READYTOSHOW;
							Post->iReadyToShowTime = m_SysTimer.GetSystemMsTime();

							bool hasShowEmblem = false;

							const char* crewEmblemTxd = crewEmblemMgr.GetEmblemName(emblemDesc);
							if (crewEmblemTxd && strlen(crewEmblemTxd) > 0)
							{
								//Update the params on the post to fill the crew emblem
								if ( Post->ParamsCount == 11 ) // Can contain an optional parameter.
								{
									hasShowEmblem = true;

									//Need to put the silly prefix char in front to handle passing the param on to Scaleform;
									Post->Params[7] = "d";
									Post->Params[7] += crewEmblemTxd;

									Post->Params[8] = "n";
									Post->Params[8] += crewEmblemTxd;
								}
								else
								{
									uiAssert(false);
								}
							}

							Post->m_TextureDictionarySlot = crewEmblemMgr.GetEmblemTXDSlot(emblemDesc);

							//If it failed to download, we'll have to remove the emblem.
							if (0 > Post->m_TextureDictionarySlot.Get())
							{
								uiErrorf("Failed to get emblem %s for gamestream idx %s", emblemDesc.GetStr(), Post->GetTypeName());

								//Does not have GameName
								if(Post->ParamsCount == 11)
								{
									Post->ParamsCount = 7;

									//Set GameName again
									Post->AddParam( Post->Params[9].c_str() );
								}
								else
								{
									uiAssert(Post->ParamsCount == 11);
									Post->ParamsCount = 7;
								}
							}
						}
						else
						{
							uiErrorf("CGameStream::ProcessPost -- Request for emblem %s failed", emblemDesc.GetStr());
							bRequestValid = false;
						}
					}
				}

				if (!bRequestValid)
				{
					uiDebugf1("CGameStream::ProcessPost -- Failed to update crew embelm update");
					FreePost(Idx);
				}
			}
			break;

		case GAMESTREAM_STATUS_SHOWING:
		if( IsAlarm(Idx) && !Post->IsFrozen())
		{
			Post->Status = GAMESTREAM_STATUS_READYTOREMOVE;
		}
		break;

		case GAMESTREAM_STATUS_CLEANUP:
		if( IsAlarm(Idx) )
		{
			uiDebugf3("CGameStream::ProcessPost -- Cleanup (%d)", Post->Type);
			FreePost( Idx );
		}
		break;

		default:
		break;
	}
}

// ---------------------------------------------------------
int CGameStream::PostMessageText( const char* Msg, const char* ContactTxD, const char* ContactTxN, bool IsImportant, int Icon, const char* CharacterName, const char* Subtitle, float timeMultiplier, const char* CrewTagPacked/*=""*/, int Icon2/*=0*/, int replaceId, int iNameColor)
{
	//@@: location AHOOK_0067_CHK_LOCATION_E
	if( m_EnableGlobal && m_EnableList[GAMESTREAM_TYPE_MESSAGE_TEXT] && CPauseMenu::GetMenuPreference(PREF_FEED_PHONE) )
	{
		if (replaceId != -1)
		{
			RemoveItem(replaceId);
		}

		int extraTime = (int) ( (GAMESTREAM_SHOWTIME_MESSAGE_TEXT+MsFromLength( Msg )) * timeMultiplier - GAMESTREAM_SHOWTIME_MESSAGE_TEXT);
		gstPost* Post = AllocPost( GAMESTREAM_TYPE_MESSAGE_TEXT, extraTime, IsImportant );
		if( Post != NULL)
		{
			if( IsImportant )
			{
				Post->IsImportant = true;
				Post->SetPostImportantParams( m_ImportantParamsbgR, m_ImportantParamsbgG, m_ImportantParamsbgB, m_ImportantParamsalphaFlash, m_ImportantParamsflashRate, m_ImportantParamsvibrate);
			}

			Post->AddParam( Msg );												// 0
			Post->AddParam( ContactTxD ASSERT_ONLY(, MESSAGE_TEXT_PARAM_TXD) );	// 1
			Post->AddParam( ContactTxN ASSERT_ONLY(, MESSAGE_TEXT_PARAM_TXN) );	// 2
			Post->AddParam( IsImportant );										// 3
			Post->AddParam( Icon ASSERT_ONLY(, MESSAGE_TEXT_PARAM_TYPE));		// 4 [MESSAGE_TEXT_PARAM_TYPE]
			Post->AddParam( CharacterName );									// 5
			Post->AddParam( Subtitle );											// 6
			Post->AddParam( CrewTagPacked );									// 7
			Post->AddParam( Icon2 );											// 8
			Post->AddParam( !(NetworkInterface::IsGameInProgress() && Icon == MESSAGE_TEXT_TYPE_MSG) );		// 9 -- Boolean to determine whether scaleform should trim this message
			Post->AddParam( iNameColor ); // 10

			if( Likely(Post->ParamsCount == 11) )
			{
				Post->IdScript = CreateNewId();

				stFeedItem feedItem;
				feedItem.iType = GAMESTREAM_TYPE_MESSAGE_TEXT;
				feedItem.sTitle = CharacterName;
				feedItem.sSubtitle = Subtitle;
				feedItem.sBody = Msg;
				feedItem.sTXD = ContactTxD;
				feedItem.sTXN = ContactTxN;
				feedItem.iScriptID = Post->IdScript;

				CacheMessage(feedItem);
				return( Post->IdScript );
			}

			FreePost( Post->Id );
		}
		uiDebugf3("CGameStream::PostMessageText -- failed.");
	}
	return( -1 );
}

// ---------------------------------------------------------
int CGameStream::PostStats( const char* Title, const char* Body, int LevelTotal, int LevelCurrent, bool IsImportant, const char* ContactTxD, const char* ContactTxN )
{
	if( m_EnableGlobal && m_EnableList[GAMESTREAM_TYPE_STATS] && CPauseMenu::GetMenuPreference(PREF_FEED_STATS))
	{
		gstPost* Post = AllocPost( GAMESTREAM_TYPE_STATS, 0, IsImportant);
		if( Post != NULL)
		{
			if( IsImportant )
			{
				Post->IsImportant = true;
				Post->SetPostImportantParams( m_ImportantParamsbgR, m_ImportantParamsbgG, m_ImportantParamsbgB, m_ImportantParamsalphaFlash, m_ImportantParamsflashRate, m_ImportantParamsvibrate);
			}
			Post->AddParam( Title );			// 0
			Post->AddParam( Body );				// 1
			Post->AddParam( 0 );				// 2
			Post->AddParam( LevelTotal );		// 3
			Post->AddParam( LevelCurrent );		// 4
			Post->AddParam( IsImportant );		// 5
			Post->AddParam( ContactTxD );		// 6
			Post->AddParam( ContactTxN );		// 7
			if( Post->ParamsCount == 8 )
			{
				stFeedItem feedItem;
				feedItem.iType = GAMESTREAM_TYPE_STATS;
				feedItem.sTitle = Title;
				feedItem.sSubtitle = Body;
				feedItem.m_stat.iProgressIncrease = LevelTotal;
				feedItem.m_stat.iProgress = LevelCurrent;
				feedItem.sTXD = ContactTxD;
				feedItem.sTXN = ContactTxN;
				CacheMessage(feedItem);
				Post->IdScript = CreateNewId();
				return( Post->IdScript );
			}
			FreePost( Post->Id );
		}
		uiDebugf3("CGameStream::PostStats -- failed.");
	}
	return( -1 );
}


// ---------------------------------------------------------
int CGameStream::PostTickerF10( const char* TopLine, const char* Body, bool IsImportant, bool bCacheMessage/*=true*/ )
{
	if( (istrlen(TopLine)+istrlen(Body)+40) < 512 )
	{
		char tmp[512];
		formatf( tmp, "<font color='#F0C850'>%s</font> <font color='#FF0000'>[DEBUG ONLY]</font><BR/>%s", TopLine, Body);
		return( PostTicker(tmp,IsImportant, bCacheMessage));
	}
	return( -1 );
}

// ---------------------------------------------------------
int CGameStream::PostTicker( const char* Body, bool IsImportant, bool bCacheMessage/*=true*/, bool bHasTokens/*=false*/, int numIconFlashes/*= 0*/, bool bAlwaysRender/* = false*/ )
{
	if( m_EnableGlobal && m_EnableList[GAMESTREAM_TYPE_TICKER] )
	{
		int extraTime = MsFromLength( Body );
		gstPost* Post = AllocPost( GAMESTREAM_TYPE_TICKER, extraTime, IsImportant );
		if( Post != NULL)
		{
			if( IsImportant )
			{
				Post->IsImportant = true;
				Post->SetPostImportantParams( m_ImportantParamsbgR, m_ImportantParamsbgG, m_ImportantParamsbgB, m_ImportantParamsalphaFlash, m_ImportantParamsflashRate, m_ImportantParamsvibrate);
			}

			if (bHasTokens)
			{
				Post->AddStringParamWithTokens(Body);	// 0
			}
			else
			{
				Post->AddStringParamAndParseGamerNameSpaces(Body);			// 0
			}
			Post->AddParam( IsImportant );				// 1
			Post->AddParam( bHasTokens );				// 2	
			Post->AddParam( numIconFlashes );			// 3
			
			Post->m_bCacheMessage = bCacheMessage;
			Post->m_bAlwaysRender = bAlwaysRender;
			
			if( Post->ParamsCount == 4 )
			{
				Post->IdScript = CreateNewId();
				if(bCacheMessage)
				{
					stFeedItem feedItem;
					feedItem.iType = GAMESTREAM_TYPE_TICKER;
					feedItem.sBody = Body;
					feedItem.iScriptID = Post->IdScript;
					CacheMessage(feedItem);
				}
				return Post->IdScript;
			}

			FreePost( Post->Id );
		}
		uiDebugf3("CGameStream::PostTicker -- failed.");
	}

	return( -1 );
}

// ---------------------------------------------------------
int CGameStream::PostAward(const char* AwardName, const char* TxD, const char* TxN, int iXP, eHUD_COLOURS eAwardColour, const char* Title)
{
	if( m_EnableGlobal && m_EnableList[GAMESTREAM_TYPE_AWARD] )
	{
		int extraTime = MsFromLength( AwardName );
		gstPost* Post = AllocPost( GAMESTREAM_TYPE_AWARD, extraTime);
		if( Post != NULL )
		{
			Post->AddParam( AwardName );	// 0
			Post->AddParam( TxD );			// 1
			Post->AddParam( TxN );			// 2
			Post->AddParam( iXP );			// 3
			Post->AddParam( eAwardColour);	// 4
			Post->AddParam( Title );		// 5
			
			if( Post->ParamsCount == 6 )
			{
				stFeedItem feedItem;
				feedItem.iType = GAMESTREAM_TYPE_AWARD;
				feedItem.eColour = eAwardColour;
				feedItem.sTitle = Title;
				feedItem.sSubtitle = AwardName;
				feedItem.sTXD = TxD;
				feedItem.sTXN = TxN;
				CacheMessage(feedItem);
				Post->IdScript = CreateNewId();
				return( Post->IdScript );
			}
			FreePost( Post->Id );
		}
		uiDebugf3("CGameStream::PostAward -- failed.");
	}

	return( -1 );
}

// ---------------------------------------------------------
int CGameStream::PostCrewTag( bool IsPrivate, bool ShowLogoFlag, const char* CrewString, int UNUSED_PARAM(CrewRank), bool FounderStatus, const char* Body, bool IsImportant, rlClanId crewId, const char* GameName, Color32 CrewColour)
{
	if( m_EnableGlobal && m_EnableList[GAMESTREAM_TYPE_CREW_TAG] && CPauseMenu::GetMenuPreference(PREF_FEED_CREW))
	{
		int extraTime = MsFromLength( Body );
		gstPost* Post = AllocPost( GAMESTREAM_TYPE_CREW_TAG, extraTime, IsImportant);
		if( Post != NULL )
		{
			if( IsImportant )
			{
				Post->IsImportant = true;
				Post->SetPostImportantParams( m_ImportantParamsbgR, m_ImportantParamsbgG, m_ImportantParamsbgB, m_ImportantParamsalphaFlash, m_ImportantParamsflashRate, m_ImportantParamsvibrate);
			}
			Post->AddParam( IsPrivate );		// 0
			Post->AddParam( ShowLogoFlag );		// 1
			Post->AddParam( CrewString );		// 2
			Post->AddParam( 0 );				// 3
			Post->AddParam( FounderStatus );	// 4
			Post->AddParam( Body );				// 5
			Post->AddParam( IsImportant );		// 6

			//Just slam an INVALID place holder TXD name in there for now to hold the place of the param.
			char crewTxdPlaceholderString[64];
			formatf(crewTxdPlaceholderString, "nCrewTXD%d", (int)crewId);
			Post->m_clanId = crewId;

			Post->AddParam( crewTxdPlaceholderString); // 7
			Post->AddParam( crewTxdPlaceholderString); // 8
			if (GameName)
			{
				Post->AddStringParamAndParseGamerNameSpaces(GameName);
			}
			else
			{
				Post->AddParam( "" );	// 9  *optional*
			}

			char CrewTagCond[NetworkClan::FORMATTED_CLAN_TAG_LEN];
			NetworkClan::GetUIFormattedClanTag( IsPrivate, ShowLogoFlag, CrewString, -1, CrewColour, CrewTagCond, NetworkClan::FORMATTED_CLAN_TAG_LEN);

			Post->AddParam( CrewTagCond );	// 10

			if( uiVerify(Post->ParamsCount == 11 ))
			{
				stFeedItem feedItem;
				feedItem.iType = GAMESTREAM_TYPE_CREW_TAG;
				feedItem.sBody = Body;
				feedItem.sCrewTag = CrewTagCond;
				feedItem.m_clanId = crewId;
				CacheMessage(feedItem);
				Post->IdScript = CreateNewId();
				return( Post->IdScript );
			}

			FreePost( Post->Id );
		}
		uiDebugf3("CGameStream::PostCrewTag -- failed.");
	}
	return( -1 );
}

// ---------------------------------------------------------
int CGameStream::PostTooltips( const char* Body, bool const IsImportant, eGameStreamTipType const type )
{
	bool const c_toolTipsEnabled = type == CGameStream::TIP_TYPE_REPLAY ? CPauseMenu::GetMenuPreference(PREF_ROCKSTAR_EDITOR_TOOLTIP) != 0 : CPauseMenu::GetMenuPreference(PREF_FEED_TOOLTIP) != 0;

	if( m_EnableGlobal && m_EnableList[GAMESTREAM_TYPE_TOOLTIPS] && c_toolTipsEnabled )
	{
		int extraTime = MsFromLength( Body );
		gstPost* Post = AllocPost( GAMESTREAM_TYPE_TOOLTIPS, extraTime, IsImportant);
		if( Post != NULL )
		{
			if( IsImportant )
			{
				Post->IsImportant = true;
				Post->SetPostImportantParams( m_ImportantParamsbgR, m_ImportantParamsbgG, m_ImportantParamsbgB, m_ImportantParamsalphaFlash, m_ImportantParamsflashRate, m_ImportantParamsvibrate);
			}

			Post->AddStringParamWithTokens( Body );	//  0
			Post->AddParam( IsImportant );			//  1

			if( uiVerify(Post->ParamsCount == 2 ))
			{
				stFeedItem feedItem;
				feedItem.iType = GAMESTREAM_TYPE_TOOLTIPS;
				feedItem.sBody = Body;
				CacheMessage(feedItem);

				Post->IdScript = CreateNewId();
				return( Post->IdScript );
			}
			FreePost( Post->Id );
		}
		uiDebugf3("CGameStream::PostTooltips -- failed.");
	}
	return( -1 );
}

// ---------------------------------------------------------
int	CGameStream::PostCrewRankup(const char* cTitle, const char* cSubtitle, const char* cTXD, const char* cTXN, bool bIsImportant)
{
	if( m_EnableGlobal && m_EnableList[GAMESTREAM_TYPE_CREW_RANKUP] )
	{
		int extraTime = MsFromLength( cSubtitle );
		gstPost* Post = AllocPost( GAMESTREAM_TYPE_CREW_RANKUP, extraTime, bIsImportant);
		if( Post != NULL )
		{
			if( bIsImportant )
			{
				Post->IsImportant = true;
				Post->SetPostImportantParams( m_ImportantParamsbgR, m_ImportantParamsbgG, m_ImportantParamsbgB, m_ImportantParamsalphaFlash, m_ImportantParamsflashRate, m_ImportantParamsvibrate);
			}
			Post->AddParam( cTitle );		//  0
			Post->AddParam( cSubtitle );	//  1
			Post->AddParam( cTXD );			//  2
			Post->AddParam( cTXN );			//  3
			Post->AddParam( bIsImportant );	//  4

			if( uiVerify(Post->ParamsCount == 5 ))
			{
				stFeedItem feedItem;
				feedItem.iType = GAMESTREAM_TYPE_CREW_RANKUP;
				feedItem.sTitle = cTitle;
				feedItem.sSubtitle = cSubtitle;
				feedItem.sTXD = cTXD;
				feedItem.sTXN = cTXN;
				CacheMessage(feedItem);

				Post->IdScript = CreateNewId();
				return( Post->IdScript );
			}
			FreePost( Post->Id );
		}
		uiDebugf3("CGameStream::PostCrewRankup -- failed.");
	}
	return( -1 );
}

int CGameStream::PostVersus( const char* ch1TXD, const char* ch1TXN, int val1, const char* ch2TXD, const char* ch2TXN, int val2, eHUD_COLOURS iCustomColor1, eHUD_COLOURS iCustomColor2 )
{
	if( m_EnableGlobal && m_EnableList[GAMESTREAM_TYPE_VERSUS] )
	{
		gstPost* Post = AllocPost( GAMESTREAM_TYPE_VERSUS, 0, false);
		if( Post != NULL )
		{
			char* pVersus = TheText.Get("VERSUS_SHORT");

			if(iCustomColor1 == HUD_COLOUR_INVALID)
			{
				iCustomColor1 = HUD_COLOUR_ENEMY;
			}
			if(iCustomColor2 == HUD_COLOUR_INVALID)
			{
				iCustomColor2 = HUD_COLOUR_FRIENDLY;
			}

			Post->AddParam( ch1TXD );	//  0
			Post->AddParam( ch1TXN );	//  1
			Post->AddParam( val1 );		//  2
			Post->AddParam( ch2TXD );	//  3
			Post->AddParam( ch2TXN );	//  4
			Post->AddParam( val2 );		//  5
			Post->AddParam( pVersus );	//  6
			Post->AddParam( iCustomColor1 );	//  7
			Post->AddParam( iCustomColor2 );	//  8

			if( uiVerify(Post->ParamsCount == 9 ))
			{
				stFeedItem feedItem;
				feedItem.iType = GAMESTREAM_TYPE_VERSUS;
				feedItem.sTXD = ch1TXD;
				feedItem.sTXN = ch1TXN;

				char tmp[1024];
				formatf(tmp, "~HC_%d~<C>%d ~s~%s ~HC_%d~%d</C>", iCustomColor1, val1, pVersus, iCustomColor2, val2);
				feedItem.sTitle = tmp;
				feedItem.sTXDLarge = ch2TXD;
				feedItem.sTXNLarge = ch2TXN;

				CacheMessage(feedItem);

				Post->IdScript = CreateNewId();
				return( Post->IdScript );
			}
			FreePost( Post->Id );
		}
		uiDebugf3("CGameStream::PostVersus -- failed.");
	}
	return( -1 );
}

int CGameStream::PostReplay(eFeedReplayState replayState, const char* cTitle, const char* cSubtitle, int iIcon, float fPctComplete, bool bFlashBufferFull, const char* cIcon)
{
	if( m_EnableGlobal && m_EnableList[GAMESTREAM_TYPE_REPLAY] )
	{
		if (replayState == DIRECTOR_RECORDING)
		{
			int directorPostIndex = GetReplayDirectorPost();
			if (directorPostIndex > -1)
			{
				return UpdateRecordingIcon( directorPostIndex, iIcon, fPctComplete, bFlashBufferFull );
			}
		}

		gstPost* Post = AllocPost( GAMESTREAM_TYPE_REPLAY, 0, false);
		if( Post )
		{
			if(uiVerify(Post->AddParam(replayState) &&
				Post->AddParam(cTitle)	&&
				Post->AddParam(cSubtitle) &&
				Post->AddParam(iIcon) &&
				Post->AddParam(fPctComplete) &&
				Post->AddParam(bFlashBufferFull) &&
				Post->AddStringParamWithTokens(cIcon)))
			{
				Post->m_bAlwaysRender = true;
				if ( replayState != ACTION_REPLAY )
				{
					// Only action replay feed items should disappear normally
					Post->SetFrozen(true);
				}
				Post->IdScript = CreateNewId();
				return( Post->IdScript );
			}

			FreePost( Post->Id );
		}

		uiDebugf3("CGameStream::PostReplay -- failed.");
	}

	return -1;
}

void CGameStream::UpdateFeedItemTexture(const char* cOldTXD, const char* cOldTXN, const char* cNewTXD, const char* cNewTXN)
{
	// Update any live feed items that aren't showing yet
	for(int i = 0; i < GAMESTREAM_MAX_POSTS; ++i)
	{
		gstPost& post = m_PostList[i];

		if( post.Status == GAMESTREAM_STATUS_NEW || post.Status == GAMESTREAM_STATUS_READYTOSHOW)
		{
			// Find the texture argument
			int txdArg;
			int txnArg;

			if( GetTextureArgs(post.Type, txdArg, txnArg) )
			{
				if( !strcmp(&post.Params[txdArg][1], cOldTXD) && !strcmp(&post.Params[txnArg][1], cOldTXN) )
				{
					post.SetParam( txdArg, cNewTXD );
					post.SetParam( txnArg, cNewTXN );
				}
			}
		}
	}

	// Update the cached feed items
	for(int i = 0; i < m_FeedCache.GetCount(); ++i)
	{
		stFeedItem& cachedItem = m_FeedCache[i];

		if(cachedItem.sTXD == cOldTXD && cachedItem.sTXN == cOldTXN)
		{
			cachedItem.sTXD = cNewTXD;
			cachedItem.sTXN = cNewTXN;
		}
	}
}

bool CGameStream::GetTextureArgs(gstPostType postType, int& txdArg, int& txnArg)
{
	bool bArgsFound = true;

	switch(postType)
	{
		case GAMESTREAM_TYPE_MESSAGE_TEXT:
			txdArg = MESSAGE_TEXT_PARAM_TXD;
			txnArg = MESSAGE_TEXT_PARAM_TXN;
			break;
		default:
			bArgsFound = false;
			break;
	}

	return bArgsFound;
}

// ---------------------------------------------------------
int CGameStream::PostUnlock( const char* chTitle, const char* chSubtitle, int iIconType, bool bIsImportant, eHUD_COLOURS eTitleColour)
{
	
	if( m_EnableGlobal && m_EnableList[GAMESTREAM_TYPE_UNLOCK] )
	{
		int extraTime = MsFromLength( chSubtitle );
		gstPost* Post = AllocPost( GAMESTREAM_TYPE_UNLOCK, extraTime);
		if( Post != NULL )
		{
			if( bIsImportant )
			{
				Post->IsImportant = true;
				Post->SetPostImportantParams( m_ImportantParamsbgR, m_ImportantParamsbgG, m_ImportantParamsbgB, m_ImportantParamsalphaFlash, m_ImportantParamsflashRate, m_ImportantParamsvibrate);
			}

			Post->AddParam( chTitle );		// 0
			Post->AddParam( chSubtitle );	// 1
			Post->AddParam( iIconType );	// 2
			Post->AddParam( bIsImportant );	// 3
			Post->AddParam( eTitleColour ); // 4


			if( uiVerify(Post->ParamsCount == 5) )
			{
				stFeedItem feedItem;
				feedItem.iType = GAMESTREAM_TYPE_UNLOCK;

				if(eTitleColour == HUD_COLOUR_PURE_WHITE)
				{
					feedItem.sTitle = chTitle;
				}
				else
				{
					// Colour the title string if given a valid color
					Color32 titleColour = CHudColour::GetRGBA(eTitleColour);
					char tmp[1024];
					formatf(tmp, "<FONT COLOR='#%02X%02X%02X'><C>%s</C></FONT>", titleColour.GetRed(), titleColour.GetGreen(), titleColour.GetBlue(), chTitle);
					feedItem.sTitle = tmp;
				}

				feedItem.sSubtitle = chSubtitle;
				feedItem.iIcon = iIconType;

				CacheMessage(feedItem);
				Post->IdScript = CreateNewId();
				return( Post->IdScript );
			}
			FreePost( Post->Id );
		}
		uiDebugf3("CGameStream::PostUnlock -- failed.");
	}
	return( -1 );
}

bool CGameStream::PostGametip( s32 gameTip, eGameStreamTipType tipType )
{
	if( CountQueued() > 0 )
	{
		return false;
	}

	if(gameTip == GAMESTREAM_GAMETIP_RANDOM )
	{
		if( m_GameTipLast < 0 )
		{
			return false;
		}

		gameTip = GetNextRandomGameTip( tipType );

		if( gameTip < 0 )
		{
			return false;
		}
	}

	bool bIsOnlineOnly = false;

	if (tipType == TIP_TYPE_MP)  // only display "GTA Online" to the tip if its unique to MP
	{
		bIsOnlineOnly = true;

		for (s32 iCount = 0; iCount < CGameStreamMgr::GetNumSpToolTips(); iCount++)
		{
			if (gameTip == CGameStreamMgr::GetSpToolTipId(iCount))
			{
				bIsOnlineOnly = false;
				break;
			}
		}
	}

	// Get the appropriate tooltip label and lookup the loc string
	char cTipLabel[64];
	char cTipLocText[512];
	formatf(cTipLabel, sizeof(cTipLabel), "TOOLTIPS_%d", gameTip);

#if KEYBOARD_MOUSE_SUPPORT
	if ( CGameStreamMgr::GetIsToolTipKeyboardSpecific(gameTip) )
	{
		const CControl& control = CControlMgr::GetMainFrontendControl();
		s32 lastKnownInput = control.GetLastKnownInputDeviceIndex();
		if ( lastKnownInput == ioSource::IOMD_KEYBOARD_MOUSE ||
			lastKnownInput == ioSource::IOMD_UNKNOWN )
		{
			safecat(cTipLabel, "_KM");
		}
	}
#endif // KEYBOARD_MOUSE_SUPPORT

	safecpy(cTipLocText, TheText.Get(cTipLabel));

	// Create the final string to post to the feed
	char cFinalTipString[1024];
	if (bIsOnlineOnly)
	{
		// For Online Only Tips -- Prefix tooltip text with "[FEED_GTAO]" for 1626068
		safecpy(cFinalTipString, TheText.Get("FEED_GTAO"));
		safecat(cFinalTipString, &cTipLocText[16]);  // skip the ~BLIP_INFO_ICON~ as this is in the new GTA Online text
	}
	else
	{
		safecpy(cFinalTipString, cTipLocText);
	}

	PostTooltips( cFinalTipString, false, tipType);
	return true;
}

int CGameStream::GetNextRandomGameTip( eGameStreamTipType tipType )
{
	int returnId = -1;
#if RSG_PC
	// Gametips that are marked to show on load should be shown before random gametips
	returnId = CGameStreamMgr::GetNextShowOnLoadTipId();
	if (returnId != -1)
	{
		return returnId;
	}
#endif // RSG_PC

	//Get the range to check from CGameStreamManager
	int last = -1;

	switch(tipType)
	{
	case TIP_TYPE_SP:
		last = CGameStreamMgr::GetNumSpToolTips();
		break;
	case TIP_TYPE_MP:
		last = CGameStreamMgr::GetNumMpToolTips();
		break;
	case TIP_TYPE_REPLAY:
		last = CGameStreamMgr::GetNumReplayToolTips();
		break;
	default:
		break;
	}

	int retry = 5;
	while( retry > 0 )
	{
		int v = fwRandom::GetRandomNumberInRange( 0, last );

		int iToolTipID = -1;
		switch(tipType)
		{
			case TIP_TYPE_SP:
				iToolTipID = CGameStreamMgr::GetSpToolTipId(v);
				break;
			case TIP_TYPE_MP:
				iToolTipID = CGameStreamMgr::GetMpToolTipId(v);
				break;
			case TIP_TYPE_REPLAY:
				iToolTipID = CGameStreamMgr::GetReplayToolTipId(v);
				break;
			default:
				break;
		}

		bool match = false;
		for( int i = 0; i < GAMESTREAM_GAMETIP_HISTORY_MAX; ++i )
		{
			if( m_GameTipHistory[i] == iToolTipID)
			{
				match = true;
				break;
			}
		}

		if( !match )
		{

			m_GameTipHistory[m_GameTipHistoryIndex] = iToolTipID;
			m_GameTipHistoryIndex++;
			if( m_GameTipHistoryIndex >= GAMESTREAM_GAMETIP_HISTORY_MAX )
			{
				m_GameTipHistoryIndex = 0;
			}

			return iToolTipID;
		}
		retry--;
	}

	int randomIndex =  fwRandom::GetRandomNumberInRange( 0, last );
	
	switch(tipType)
	{
	case TIP_TYPE_SP:
		returnId = CGameStreamMgr::GetSpToolTipId(randomIndex);
		break;
	case TIP_TYPE_MP:
		returnId = CGameStreamMgr::GetMpToolTipId(randomIndex);
		break;
	case TIP_TYPE_REPLAY:
		returnId = CGameStreamMgr::GetReplayToolTipId(randomIndex);
		break;
	default:
		break;
	}

	return returnId;
}


void CGameStream::AutoPostGameTipUpdate() 
{
	if( m_GameTipTimer.GetMsTime() > GAMESTREAM_GAMETIP_CYCLE_TIME || !m_HasShownFirstAutoPostedGameTip )
	{

		CGameStream::eGameStreamTipType currentTipType = m_AutoPostGameTipType;

		if(m_AutoPostGameTipType == TIP_TYPE_ANY)
		{
			if (CNetwork::GetGoStraightToMultiplayer() || CNetwork::IsNetworkOpen())
			{
				currentTipType = CGameStream::TIP_TYPE_MP;
			}
			else
			{
				currentTipType = CGameStream::TIP_TYPE_SP;
			}
		}
		
		if( PostGametip(GAMESTREAM_GAMETIP_RANDOM, currentTipType) )
		{
			m_HasShownFirstAutoPostedGameTip = true;
			m_GameTipTimer.Reset();
		}
	}
}

// ---------------------------------------------------------
void CGameStream::BusySpinnerOn( const char* Body, int Icon )
{
	m_BusySpinnerOn = true;
	if( strcmp(Body,m_BusySpinnerTxt)==0 && Icon==m_BusySpinnerIcon )
	{
		return;
	}
	strncpy( m_BusySpinnerTxt, Body, GAMESTREAM_MAX_BUSY_TXT );
	m_BusySpinnerIcon =  Icon;
	m_BusySpinnerUpd = true;
}

// ---------------------------------------------------------
void CGameStream::BusySpinnerOff()
{
	m_BusySpinnerOn = false;
}

// ---------------------------------------------------------
bool CGameStream::IsBusySpinnerOn()
{
	return( m_BusySpinnerOn );
}

// ---------------------------------------------------------
void CGameStream::BusySpinnerUpdate()
{
	if( m_BusySpinnerBackoffFrames > 0 )
	{
		m_BusySpinnerBackoffFrames--;
		return;
	}
	
	if( m_BusySpinnerOn )
	{
		if( m_BusySpinnerUpd )
		{
			//PostSaving( m_BusySpinnerTxt, m_BusySpinnerIcon, false );
			m_BusySpinnerOnCmp = true;
			m_BusySpinnerUpd = false;
			m_BusySpinnerBackoffFrames = GAMESTREAM_SPINNER_BACKOFF;
			uiDebugf3("CGameStream::BusySpinnerUpdate -- UPDATE" );
			return;
		}

		if( !m_BusySpinnerOnCmp )
		{
			//PostSaving( m_BusySpinnerTxt, m_BusySpinnerIcon, false );
			m_BusySpinnerOnCmp = true;
			m_BusySpinnerBackoffFrames = GAMESTREAM_SPINNER_BACKOFF;
			uiDebugf3("CGameStream::BusySpinnerUpdate -- ON" );
			return;
		}
	}
	else
	{
		if( m_BusySpinnerOnCmp )
		{
			//for( int i = 0; i != GAMESTREAM_MAX_POSTS; i++ )
			//{
				//if( m_PostList[i].Type == GAMESTREAM_TYPE_SAVING )
				//{
					//RemoveItem( i );
				//}
			//}
			m_BusySpinnerOnCmp = false;
			m_BusySpinnerBackoffFrames = GAMESTREAM_SPINNER_BACKOFF;
			uiDebugf3("CGameStream::BusySpinnerUpdate -- OFF" );
		}
	}
}

int CGameStream::UpdateRecordingIcon( int PostIdx, int iIcon, float fPctComplete, bool bBufferFilled )
{
	if ( uiVerify( PostIdx < GAMESTREAM_MAX_POSTS ) )
	{
		m_PostList[ PostIdx ].SetParam( 0, iIcon );	// Replace Icon
		m_PostList[ PostIdx ].SetParam( 4, fPctComplete );	// Replace percentage
		m_PostList[ PostIdx ].SetParam( 5, bBufferFilled );	// Replace buffer filled bool
		m_PostList[ PostIdx ].m_bShouldUpdate = true;
		return m_PostList[ PostIdx ].IdScript;
	}

	return -1;
}

bool CGameStream::gstPost::AddStringParamWithTokens(const char* Data)
{
	return AddParam(Data, "T");
}

bool CGameStream::gstPost::AddStringParamAndParseGamerNameSpaces( const char* Data )
{
	if (uiVerify(Data))
	{
		bool bInsideGamerName = false;

		const int closeTagLength = 4;
		const int openTagLength = 3;
		const char closeTag[closeTagLength + 1] = "</C>";
		const char openTag[openTagLength + 1] = "<C>";
		atString cTagCheck;
		atString parsedMessage;

		while (*Data != '\0')
		{
			if (bInsideGamerName)
			{
				for (int tagIdx = 0; tagIdx < closeTagLength; tagIdx++)
				{
					// Get the next few characters in order to check if we've exited the gamer name
					if (*Data != '\0')
						cTagCheck += Data[0];
					++Data;
				}
				Data -= closeTagLength;		// Go back to where we were before getting the tag check characters
				if (strcmp(cTagCheck, closeTag) == 0)
				{
					// We found a closing tag - we've exited the gamer name. Skip past the tag and add it to our resulting message
					bInsideGamerName = false;
					Data += closeTagLength - 1;
					parsedMessage += closeTag;
				}
				else if (*Data == ' ')
				{
					// We've found a space in the gamer name - replace it with a non-breaking space
					parsedMessage += BLANK_NBSP_CHARACTER;
				}
				else
				{
					parsedMessage += Data[0];	// Normal character in the gamer name - add it.
				}
			}
			else
			{
				for (int tagIdx = 0; tagIdx < openTagLength; tagIdx++)
				{
					// Get the next few characters in order to check if we've entered a gamer name
					if (*Data != '\0')
						cTagCheck += Data[0];
					++Data;
				}
				Data -= openTagLength;		// Go back to where we were before getting the tag check characters
				if (strcmp(cTagCheck, openTag) == 0)
				{
					// We found an opening tag - we're inside the gamer name. Skip past the tag and add it to the resulting message
					bInsideGamerName = true;
					Data += openTagLength - 1;
					parsedMessage += openTag;
				}
				else
				{
					parsedMessage += Data[0];	// Normal message character - add it.
				}
			}
			cTagCheck.Clear();	// Clear the tag check so it's ready to check the next few characters
			++Data;
		}

		parsedMessage += "\0";
		return AddParam( parsedMessage.c_str() );
	}

	return false;
}

// ---------------------------------------------------------
bool CGameStream::gstPost::AddParam( const char* Data ASSERT_ONLY(, int paramCountToVerify/* = -1*/))
{
	ASSERT_ONLY(VerifyParamCount(paramCountToVerify));

	if( uiVerify(ParamsCount < GAMESTREAM_MAX_PARAMS) )
	{
		SetParam(ParamsCount++, Data);
		return true;
	}

	return false;
}

// ---------------------------------------------------------
bool CGameStream::gstPost::AddParam( int Data ASSERT_ONLY(, int paramCountToVerify/* = -1*/) )
{
	ASSERT_ONLY(CGameStream::gstPost::VerifyParamCount(paramCountToVerify));

	if( uiVerify(ParamsCount < GAMESTREAM_MAX_PARAMS ))
	{
		SetParam(ParamsCount++, Data);
		return true;
	}

	return false;
}


bool CGameStream::gstPost::AddParam( float Data )
{
	if( uiVerify(ParamsCount < GAMESTREAM_MAX_PARAMS ))
	{
		SetParam(ParamsCount++, Data);
		return true;
	}

	return false;
}

// ---------------------------------------------------------
bool CGameStream::gstPost::AddParam( bool Data )
{
	if( uiVerify(ParamsCount < GAMESTREAM_MAX_PARAMS ))
	{
		SetParam(ParamsCount++, Data);
		return true;
	}

	return false;
}

// ---------------------------------------------------------
bool CGameStream::gstPost::AddParam( const char* Data, const char* Prefix )
{
	if( uiVerify(ParamsCount < GAMESTREAM_MAX_PARAMS) )
	{
		Params[ParamsCount] = Prefix;
		Params[ParamsCount] += Data;
		ParamsCount++;
		return true;
	}

	return false;
}

// ---------------------------------------------------------
#if __ASSERT
void CGameStream::gstPost::VerifyParamCount(int paramCountToVerify)
{
	if(paramCountToVerify != -1)
	{
		uiAssertf(paramCountToVerify == ParamsCount, "CGameStream::gstPost::VerifyParamCount -- Argument Mismatch.  Next param should be %d, but instead is %d", paramCountToVerify, ParamsCount);
	}
}
#endif // __ASSERT


// Set a param that's already formatted with a modifier
bool CGameStream::gstPost::SetFormattedParam( int Idx, const char* FormattedData )
{
	if(uiVerify(Idx < ParamsCount))
	{
		Params[Idx] = FormattedData;
		return true;
	}
	return false;
}

void CGameStream::gstPost::SetParam( int i, int Data )
{
	if(uiVerify(i < ParamsCount))
	{
		char buffer[32];
		Params[i] = formatf( buffer, "I%d", Data );
	}
}

void CGameStream::gstPost::SetParam( int i, float Data )
{
	if(uiVerify(i < ParamsCount))
	{
		char buffer[64];
		Params[i] = formatf( buffer, "F%f", Data );
	}
}

void CGameStream::gstPost::SetParam( int i, bool Data )
{
	if(uiVerify(i < ParamsCount))
	{
		char buffer[3];
		Params[i] = formatf( buffer, "B%d", Data ? 1 : 0 );
	}
}

void CGameStream::gstPost::SetParam( int i, const char* Data)
{
	if(uiVerify(i < ParamsCount))
	{
		Params[i] = "S";
		Params[i] += Data;
	}
}

void CGameStream::gstPost::SetPostImportantParams( int r, int g, int b, int a, int flash, bool vibrate)
{
	m_ImportantParameters.r = (u8)r;
	m_ImportantParameters.g = (u8)g;
	m_ImportantParameters.b = (u8)b;
	m_ImportantParameters.a = (u8)a;
	m_ImportantParameters.flash = (u8)flash;
	m_ImportantParameters.vibrate = (u8)vibrate;
}

const char* CGameStream::gstPost::GetTypeName() const
{
	if (m_pStaticData && Type != GAMESTREAM_TYPE_INVALID)
	{
		return m_pStaticData->m_typeName;
	}

	return "";
}

const char* CGameStream::gstPost::GetUpdateMethod() const
{
	if (m_pStaticData && Type != GAMESTREAM_TYPE_INVALID)
	{
		return m_pStaticData->m_UpdateMethod;
	}

	return "";
}

bool CGameStream::gstPost::RequestTextureForDownload( const char* textureName, const char* cloudFilePath )
{
	uiAssert(m_bDynamicTexture);

	m_requestDesc.m_Type = CTextureDownloadRequestDesc::TITLE_FILE;
	m_requestDesc.m_GamerIndex = 0;
	m_requestDesc.m_CloudFilePath = cloudFilePath;
	m_requestDesc.m_BufferPresize = 22 *1024;  //arbitrary pre size of 22k
	m_requestDesc.m_TxtAndTextureHash = textureName;
	m_requestDesc.m_CloudOnlineService = RL_CLOUD_ONLINE_SERVICE_NATIVE;

	CTextureDownloadRequest::Status retVal = CTextureDownloadRequest::REQUEST_FAILED;
	retVal = DOWNLOADABLETEXTUREMGR.RequestTextureDownload(m_requestHandle, m_requestDesc);

	m_TextureDictionarySlot = -1;

	return retVal == CTextureDownloadRequest::IN_PROGRESS || retVal == CTextureDownloadRequest::READY_FOR_USER;
}

//NOTE:
//  Returns true if something needs to be dealth with.
bool CGameStream::gstPost::UpdateTextureDownloadStatus()
{
	if (m_requestHandle == CTextureDownloadRequest::INVALID_HANDLE || !m_bDynamicTexture)
	{
		return true;
	}

	if (DOWNLOADABLETEXTUREMGR.IsRequestPending(m_requestHandle))
	{
		return true;
	}

	if ( DOWNLOADABLETEXTUREMGR.IsRequestReady( m_requestHandle ) )
	{
		//Update the slot that the texture was slammed into
		strLocalIndex txdSlot = strLocalIndex(DOWNLOADABLETEXTUREMGR.GetRequestTxdSlot(m_requestHandle));
		if (uiVerifyf(g_TxdStore.IsValidSlot(txdSlot), "CGameStream::gstPost::UpdateTextureDownloadStatus- failed to get valid txd with name %s from DTM at slot %d", m_requestDesc.m_TxtAndTextureHash.GetCStr(), txdSlot.Get()))
		{
			g_TxdStore.AddRef(txdSlot, REF_OTHER);
			m_TextureDictionarySlot = txdSlot;
			uiDebugf1("CGameStream::gstPost::UpdateTextureDownloadStatus -- Downloaded texture for gamestream item %d has been received in TXD slot %d", Id, txdSlot.Get());
		}

		//Tell the DTM that it can release it's hold on the TXD (since we've taken over the Reference to keep it in memory.
		DOWNLOADABLETEXTUREMGR.ReleaseTextureDownloadRequest( m_requestHandle );
		m_requestHandle = CTextureDownloadRequest::INVALID_HANDLE;

		return true;
	}
	else if (DOWNLOADABLETEXTUREMGR.HasRequestFailed(m_requestHandle))
	{
		//Texture request failed 
		//@TODO 
		// Set to cleanup?
		m_TextureDictionarySlot = -1;

		//Tell the DTM that it can release it's hold on the TXD (since we've taken over the Reference to keep it in memory.
		DOWNLOADABLETEXTUREMGR.ReleaseTextureDownloadRequest( m_requestHandle );
		m_requestHandle = CTextureDownloadRequest::INVALID_HANDLE;

		return true;
	}

	uiErrorf(" UpdateTextureDownloadStatus - failed, Somethign has gone wrong ");
	m_TextureDictionarySlot = -1;
	DOWNLOADABLETEXTUREMGR.ReleaseTextureDownloadRequest( m_requestHandle );
	m_requestHandle = CTextureDownloadRequest::INVALID_HANDLE;

	return false;
}

void CGameStream::gstPost::ReleaseAndCancelTexture()
{
	if (m_requestHandle != CTextureDownloadRequest::INVALID_HANDLE)
	{
		DOWNLOADABLETEXTUREMGR.ReleaseTextureDownloadRequest( m_requestHandle );
		m_requestHandle = CTextureDownloadRequest::INVALID_HANDLE;
	}

	if (m_bClanEmblemRequested)
	{
		EmblemDescriptor emblemDesc(EmblemDescriptor::EMBLEM_CLAN, (EmblemId)m_clanId, RL_CLAN_EMBLEM_SIZE_128);
		NETWORK_CLAN_INST.GetCrewEmblemMgr().ReleaseEmblem(emblemDesc  ASSERT_ONLY(, "CGameStream"));
		m_bClanEmblemRequested = false;
		m_clanId = RL_INVALID_CLAN_ID;
		m_TextureDictionarySlot = -1;
	}
	else if (m_TextureDictionarySlot.Get() >= 0)
	{
		// Check the txd slot is valid and has at least one reference
		if (g_TxdStore.IsValidSlot(m_TextureDictionarySlot) != false && g_TxdStore.GetNumRefs(m_TextureDictionarySlot) > 0)
		{
			g_TxdStore.RemoveRef(m_TextureDictionarySlot, REF_OTHER);
		}
		m_TextureDictionarySlot = strLocalIndex(-1);
	}
}

// ---------------------------------------------------------
bool CGameStream::IsTypeShowing( int Type )
{
	for( int i=0; i!=GAMESTREAM_MAX_POSTS; i++ )
	{
		if( m_PostList[i].Status == GAMESTREAM_STATUS_SHOWING && m_PostList[i].Type == Type )
		{
			return( true );
		}
		if( m_PostList[i].Status == GAMESTREAM_STATUS_READYTOREMOVE && m_PostList[i].Type == Type )
		{
			return( true );
		}
		if( m_PostList[i].Status == GAMESTREAM_STATUS_CLEANUP && m_PostList[i].Type == Type )
		{
			return( true );
		}
	}
	return( false );
}

// ---------------------------------------------------------
bool CGameStream::IsShowing( int Idx ) const
{
	return m_PostList[Idx].Status == GAMESTREAM_STATUS_SHOWING ||
			m_PostList[Idx].Status == GAMESTREAM_STATUS_READYTOREMOVE ||
			m_PostList[Idx].Status == GAMESTREAM_STATUS_CLEANUP;
}


// ---------------------------------------------------------
int CGameStream::CountQueued() const
{
	int Count = 0;
	for( int i=0; i!=GAMESTREAM_MAX_POSTS; i++ )
	{
		if( m_PostList[i].Status != GAMESTREAM_STATUS_FREE )
		{
			Count++;
		}
	}
	return( Count );
}

// ---------------------------------------------------------
int CGameStream::CountShowing() const
{
	int Count = 0;
	for( int i=0; i!=GAMESTREAM_MAX_POSTS; i++ )
	{
		if( IsShowing(i) )
		{
			Count++;
		}
	}
	return( Count );
}

// ---------------------------------------------------------
//bool CGameStream::ReportMinimapSizePosToScaleform( s32 MovieId, int ContainerComponentId, gstPs* Ps )
//{
//	if(!CScaleformMgr::IsMovieActive(MovieId))
//	{
//		return( false );
//	}
//	
//	if(CScaleformMgr::BeginMethod(MovieId, SF_BASE_CLASS_HUD, "CONTAINER_METHOD"))
//	{
//		CScaleformMgr::AddParamInt( ContainerComponentId );
//		CScaleformMgr::AddParamString( "SET_MINIMAP_SIZE_AND_POS" );
//		CScaleformMgr::AddParamFloat( Ps->x );
//		CScaleformMgr::AddParamFloat( Ps->y );
//		CScaleformMgr::AddParamFloat( Ps->w );
//		CScaleformMgr::AddParamFloat( Ps->h );
//		CScaleformMgr::EndMethod();
//		uiDebugf3("*** Reported new Minimap Pos/Size: x:%f, y:%f, w:%f, h:%f", Ps->x, Ps->y, Ps->w, Ps->h);
//		return( true );
//	}
//	return( false );
//}

// ---------------------------------------------------------
bool CGameStream::PostToScaleform( s32 MovieId, gstPost* Post, gstPostScaleFormMethod Method )
{
	if(!CScaleformMgr::IsMovieActive(MovieId))
	{
		return( false );
	}

	if( Method == GAMESTREAM_METHOD_SET )
	{
		if( Post->IsImportant )
		{
			SetImportantParamsScaleform(MovieId,&Post->m_ImportantParameters);
			if( Post->m_ImportantParameters.vibrate )
			{
				CControlMgr::StartPlayerPadShakeByIntensity( 130, 1.0f );
			}
		}

		if( Post->m_bSnapIntoPosition )
		{
			CScaleformMgr::CallMethod(MovieId, SF_BASE_CLASS_GAMESTREAM, "SNAP_NEXT_FEED_ITEM_INTO_POSITION");
		}

		if ( Post->m_bChangeBgColor )
		{
			if (CScaleformMgr::BeginMethod(MovieId, SF_BASE_CLASS_GAMESTREAM, "SET_NEXT_FEED_POST_BACKGROUND_COLOR"))
			{
				CScaleformMgr::AddParamInt( Post->m_eBgColor );
				CScaleformMgr::EndMethod();
			}
		}

		if(CScaleformMgr::BeginMethod(MovieId, SF_BASE_CLASS_GAMESTREAM, "SET_STREAM_COMPONENT"))
		{
			AddInnerScaleformParams(Post);
			CScaleformMgr::EndMethod();
			return true;
		}
	}

	if( Method == GAMESTREAM_METHOD_UPDATE )
	{
		if(CScaleformMgr::BeginMethod(MovieId, SF_BASE_CLASS_GAMESTREAM, "UPDATE_STREAM_COMPONENT"))
		{
			AddInnerScaleformParams(Post);
			CScaleformMgr::EndMethod();
			return true;
		}
	}
	
	
	if( Method == GAMESTREAM_METHOD_REMOVE )
	{
		if(CScaleformMgr::BeginMethod(MovieId, SF_BASE_CLASS_GAMESTREAM, "REMOVE_STREAM_COMPONENT"))
		{
			CScaleformMgr::AddParamInt( Post->Id );
			CScaleformMgr::AddParamInt( Post->Type); // This will prepare us for one removal function
			CScaleformMgr::EndMethod();
			return( true );
		}
	}
	return( false );
}

void CGameStream::AddInnerScaleformParams(gstPost* post)
{
	CScaleformMgr::AddParamInt( post->Id );
	for( int j=0; j!=post->ParamsCount; ++j )
	{
		switch( post->Params[j][0] ) 
		{
		case 'S':	// Raw String
			CScaleformMgr::AddParamString( &post->Params[j][1] );
			break;
		case 'T':	// String Containing Tokens (So we choose to not format the html
			CScaleformMgr::AddParamString( &post->Params[j][1], false );
			break;
		case 'I':	// Integer
			CScaleformMgr::AddParamInt( atoi( &post->Params[j][1] ) );
			break;
		case 'F':	// Float
			CScaleformMgr::AddParamFloat( atof(&post->Params[j][1] ) );
			break;
		case 'B':	// Boolean
			CScaleformMgr::AddParamBool( StrToBool( &post->Params[j][1] ) );
			break;
		default:
			CScaleformMgr::AddParamString( &post->Params[j][1] );
			break;
		}
	}
	CScaleformMgr::AddParamInt( post->Type );
}

// ---------------------------------------------------------
void CGameStream::SetPostChangeBgColor( gstPost* post )
{
	if ( m_bSetFeedBackgroundColor )
	{
		post->m_bChangeBgColor = true;
		post->m_eBgColor = m_bgColor;
		m_bSetFeedBackgroundColor = false;
	}
}

// ---------------------------------------------------------
void CGameStream::SetIsOnLoadingScreen( bool IsOn )
{
	if( m_IsOnLoadingScreen != IsOn )
	{
		m_IsOnLoadingScreen = IsOn;
		SetDisplayConfigChanged();
	}
}

// ---------------------------------------------------------
bool CGameStream::IsOnLoadingScreen()
{
	return( m_IsOnLoadingScreen );
}

// ---------------------------------------------------------
void CGameStream::SetDisplayConfigChanged( void )
{
	m_DisplayConfigChanged = true;
}

// ---------------------------------------------------------
bool CGameStream::UpdateCheckDisplayConfig( s32 MovieId )
{
	if( m_DisplayConfigChanged )
	{
		if(CScaleformMgr::SetMovieDisplayConfig(MovieId, SF_BASE_CLASS_GAMESTREAM, CScaleformMgr::SDC::UseFakeSafeZoneOnBootup))
		{
			m_DisplayConfigChanged = false;
			m_ChangeMovieParams = true;
			return true;
		}
	}

	return false;
}

// ---------------------------------------------------------
bool CGameStream::ChangeMovieParams()
{
	if( m_ChangeMovieParams )
	{
		m_ChangeMovieParams=false;
		return( true );
	}
	return( false );
}

// ---------------------------------------------------------
void CGameStream::SetMinimapState( eDisplayState MinimapState )
{
	m_MinimapState = MinimapState;
}

// ---------------------------------------------------------
bool CGameStream::UpdateCheckMinimap( s32 MovieId )
{
	UpdateMinimapState();
	
	if( m_MinimapState != m_MinimapStateCmp )
	{
		uiDebugf3("CGameStream::UpdateCheckMinimap -- %d",m_MinimapState );
		if ( CScaleformMgr::BeginMethod(MovieId, SF_BASE_CLASS_GAMESTREAM, "SET_MINIMAP_VISIBLE_STATE") )
		{
			CScaleformMgr::AddParamInt( m_MinimapState );
			CScaleformMgr::EndMethod();
			m_MinimapStateCmp = m_MinimapState;
			return( true );
		}
	}
	return( false );
}

// ---------------------------------------------------------
void CGameStream::ReportLogoIsOn()
{
	m_IsLogoVisible = true;
}

// ---------------------------------------------------------
void CGameStream::ReportLogoIsOff()
{
	m_IsLogoVisible = false;
}

// Monitor changes to the minimap so we can adjust the
// positioning of the feed movie. (Off,Small,Large)
// ---------------------------------------------------------
void CGameStream::UpdateMinimapState()
{
	if( m_IsOnLoadingScreen )
	{
		SetMinimapState( DISPLAY_STATE_LOADING_SCREEN_LOGO );	// So it sits above the logo on the loading screen.
		return;
	}

	if( m_IsLogoVisible )
	{
		SetMinimapState( DISPLAY_STATE_ONLINE_LOGO );	// url:bugstar:1339823
		return;
	}

	if( !CMiniMap::ShouldRenderMinimap() || !CMiniMap::GetVisible() || !CMiniMap::IsRendering() )  // fix for 1537659
	{
		SetMinimapState( DISPLAY_STATE_MAP_OFF );
		return;
	}

	if (CMiniMap::IsInBigMap() || CMiniMap::IsInGolfMap() )  // fix for 1669410
	{
		SetMinimapState( DISPLAY_STATE_MAP_LARGE );
		return;
	}
	
	if( /* m_IsForcedRender && */ CPauseMenu::IsActive() )
	{
		SetMinimapState( DISPLAY_STATE_MAP_OFF );	// Assume the minimap isn't rendering if we have to force.
		return;
	}

	SetMinimapState( DISPLAY_STATE_MAP_SMALL );
}

// ---------------------------------------------------------
void CGameStream::ReportHelptextStatus( Vector2 Pos, Vector2 Size, bool IsActive )
{
	if( IsActive )
	{
		SetHelptextHeight( Pos.y+Size.y );
		return;
	}
	SetHelptextHeight( 0.0f );
}

// ---------------------------------------------------------
void CGameStream::SetHelptextHeight( float Height )
{
	m_HelptextHeight = Height;
}

// ---------------------------------------------------------
void CGameStream::SetScriptedMenuHeight( float Height )
{
	m_ScriptedMenuHeight = Height;
}

// ---------------------------------------------------------
bool CGameStream::UpdateCheckHelpTextHeight( s32 MovieId )
{
	if( m_HelptextHeight != m_HelptextHeightCmp )
	{
		uiDebugf3("CGameStream::UpdateCheckHelpTextHeight -- Set helptext height. %f\n", m_HelptextHeight );
		if ( CScaleformMgr::BeginMethod(MovieId, SF_BASE_CLASS_GAMESTREAM, "SET_HELPTEXT_HEIGHT") )
		{
			CScaleformMgr::AddParamFloat( m_HelptextHeight );
			CScaleformMgr::EndMethod();
			m_HelptextHeightCmp = m_HelptextHeight;
			return( true );
		}
	}

	if( m_ScriptedMenuHeight != m_ScriptedMenuHeightCmp )
	{
		uiDebugf3("CGameStream::UpdateCheckHelpTextHeight -- Set scripted menu height. %f\n", m_ScriptedMenuHeight );
		if ( CScaleformMgr::BeginMethod(MovieId, SF_BASE_CLASS_GAMESTREAM, "SET_HELPTEXT_HEIGHT") )
		{
			CScaleformMgr::AddParamFloat( m_ScriptedMenuHeight );
			CScaleformMgr::EndMethod();
			m_ScriptedMenuHeightCmp = m_ScriptedMenuHeight;
			return( true );
		}
	}

	return( false );
}


bool CGameStream::IsScriptHidingThisFrame() const
{
	return (m_ShowCounters[GAMESTREAM_SHOW_IDX_EXT2] == 4);
}

#if __BANK
void CGameStream::BankLogRenderState(bool bRenderedThisFrame, const char* reason)
{
	if(m_bRenderedLastFrame != bRenderedThisFrame || strcmp(reason, m_previousRenderStateReason) != 0)
	{
		m_bRenderedLastFrame = bRenderedThisFrame;
		safecpy(m_previousRenderStateReason, reason);		
		uiDebugf3("CGameStream::BankLogRenderState -- %sRendering GameStream -- Reason: %s", (bRenderedThisFrame ? "" : "Not "), reason);
	}
}

void CGameStream::SetShowDebugBoundsEnabled(s32 iMovieId, bool bIsEnabled, bool bForceToScaleform)
{
	if (bIsEnabled != m_bShowDebugBounds || bForceToScaleform)
	{
		if (iMovieId != SF_INVALID_MOVIE)
		{
			if (CScaleformMgr::BeginMethod(iMovieId, SF_BASE_CLASS_GAMESTREAM, "ENABLE_SHOW_DEBUG_BOUNDS"))
			{
				CScaleformMgr::AddParamBool(bIsEnabled);
				CScaleformMgr::EndMethod();
			}
		}

		m_bShowDebugBounds = bIsEnabled;
	}
}
#endif

// ---------------------------------------------------------
bool CGameStream::ShouldRender(bool bUpdateShowCounters/* = true*/)
{
	/*
	if( m_HideUntilFlushComplete )
	{
		BANK_ONLY(BankLogRenderState(false, "m_HideUntilFlushComplete"));
		return false;
	}*/

	if (CPauseMenu::IsNavigatingContent() && CPauseMenu::IsInMapScreen())
	{
		BANK_ONLY(BankLogRenderState(false, "CPauseMenu::IsNavigatingContent and CPauseMenu::IsInMapScreen"));
		return false;
	}

	if (m_AutoPostGameTip && !m_HasShownFirstAutoPostedGameTip)
	{
		BANK_ONLY(BankLogRenderState(false, "m_AutoPostGameTip and !m_HasShownFirstAutoPostedGameTip"));
		UpdateShouldRenderCounters();
		return false;
	}

	if( m_IsForcedRender )
	{
		BANK_ONLY(BankLogRenderState(true, "m_IsForcedRender"));
		return true;
	}

	if (CLoadingScreens::AreActive() && IsScriptHidingThisFrame())  // if we are on loadingscreens and script are hiding this frame then dont render any of the feed
	{
		BANK_ONLY(BankLogRenderState(false, "CLoadingScreens::AreActive and IsScriptHidingThisFrame"));
		return false;
	}

	// If the Minimap preference is set to show and the Minimap is hidden briefly after some sort of transition don't render game stream (fixes 1734237)
	if (CPauseMenu::GetMenuPreference(PREF_RADAR_MODE) && CMiniMap::GetIsHiddenAfterTransition())
	{
		BANK_ONLY(BankLogRenderState(false, "CMiniMap::GetIsHiddenAfterTransition"));
		return false;
	}

	int shouldRenderCount;

	if (bUpdateShowCounters)
	{
		shouldRenderCount = UpdateShouldRenderCounters();
	}
	else
	{
		shouldRenderCount = GetShouldRenderCount();
	}

	if (HasAlwaysRenderFeedItem())
	{
		BANK_ONLY(BankLogRenderState(true, "HasAlwaysRenderFeedItem"));
		return true;
	}

	BANK_ONLY(BankLogRenderState((shouldRenderCount==GAMESTREAM_SHOW_FLAG_COUNT) && (CountShowing()>0), "DEFAULT Render Check"));

	return( (shouldRenderCount==GAMESTREAM_SHOW_FLAG_COUNT) && (CountShowing()>0) );
}



int CGameStream::UpdateShouldRenderCounters()
{
	// This should probably be happening before any early outs, but it might break behavior
	// url:bugstar:2248567 - Show counters need to get updated if there's an always render feed item so new messages can come in
	int shouldRenderCount = 0;
	for( int i=0; i!=GAMESTREAM_SHOW_FLAG_COUNT; i++ )
	{ 
		if( m_ShowCounters[i] == 0 )
		{
			shouldRenderCount++;
		}
		else
		{
			if( m_ShowCounters[i] != GAMESTREAM_SHOWCOUNT_FOREVER )
			{
				m_ShowCounters[i]--;
			}
		}
	}

	return shouldRenderCount;
}



int CGameStream::GetShouldRenderCount()
{
	int shouldRenderCount = 0;
	for( int i=0; i!=GAMESTREAM_SHOW_FLAG_COUNT; i++ )
	{
		if( m_ShowCounters[i] == 0 )
		{
			shouldRenderCount++;
		}
	}

	return shouldRenderCount;
}



// Callback from scaleform if an item renders outside the safe area.
// ---------------------------------------------------------
void CGameStream::StreamItemFailed( s32 Index )
{
	Index = ConvertToIndex( Index );

	uiDebugf3("CGameStream::StreamItemFailed -- %d", Index);
	if( uiVerify(Index>=0 && Index<GAMESTREAM_MAX_POSTS ))
	{
		if( m_PostList[Index].Status == GAMESTREAM_STATUS_SHOWING )
		{
			m_PostList[Index].FailedCount++;
			if( m_PostList[Index].FailedCount >= GAMESTREAM_MAX_FAILCOUNT )
			{
				m_PostList[Index].Status = GAMESTREAM_STATUS_READYTOREMOVE;
				uiDebugf3("CGameStream::StreamItemFailed -- Too many failed attempts to show. Removing from the queue: %d", Index);
			}
			else
			{
				m_PostList[Index].Status = GAMESTREAM_STATUS_READYTOSHOW;
				m_PostList[Index].iReadyToShowTime = m_SysTimer.GetSystemMsTime();
			}
		}
		m_DisableAdd = GAMESTREAM_ADD_WAIT_COUNT;
	}
}

// ---------------------------------------------------------
bool CGameStream::InteractWithScaleformFast( s32 MovieId )
{
	if( UpdateShowHide(MovieId) )
	{
		return( true );
	}

	if( UpdateCheckDisplayConfig(MovieId) )
	{
		return( true );
	}

	if( UpdateCheckMinimap(MovieId) )
	{
		return( true );
	}

	if( UpdateCheckHelpTextHeight(MovieId) )
	{
		return( true );
	}
	
	return( false );
}

void CGameStream::SetImportantParamsRGBA( int bgR, int bgG, int bgB, int alphaFlash )
{
	m_ImportantParamsbgR = bgR;
	m_ImportantParamsbgG = bgG;
	m_ImportantParamsbgB = bgB;
	m_ImportantParamsalphaFlash = alphaFlash;
}

// ---------------------------------------------------------
void CGameStream::SetImportantParamsVibrate( bool vibrate )
{
	m_ImportantParamsvibrate = vibrate;
}

// ---------------------------------------------------------
void CGameStream::SetImportantParamsFlashCount( int flashCount )
{
	m_ImportantParamsflashRate = flashCount;
}

// ---------------------------------------------------------
void CGameStream::SetImportantParams( int bgR, int bgG, int bgB, int alphaFlash, int flashRate, bool vibrate )
{
	SetImportantParamsRGBA(bgR,bgG,bgB,alphaFlash);
	SetImportantParamsFlashCount(flashRate);
	SetImportantParamsVibrate(vibrate);
}

// ---------------------------------------------------------
void CGameStream::ResetImportantParams()
{
	m_ImportantParamsbgR = DEFAULT_FEED_RED_VALUE;
	m_ImportantParamsbgG = DEFAULT_FEED_GREEN_VALUE;
	m_ImportantParamsbgB = DEFAULT_FEED_BLUE_VALUE;
	m_ImportantParamsalphaFlash = DEFAULT_FEED_ALPHA_VALUE;
	m_ImportantParamsflashRate = DEFAULT_FEED_FLASH_RATE_VALUE;
	m_ImportantParamsvibrate = false;
}

void CGameStream::SetImportantParamsScaleform( s32 MovieId, gstImportantParameters* importantParameters)
{
	if(CScaleformMgr::BeginMethod(MovieId, SF_BASE_CLASS_GAMESTREAM, "SET_IMPORTANT_PARAMS"))
	{
		CScaleformMgr::AddParamInt( importantParameters->r );
		CScaleformMgr::AddParamInt( importantParameters->g );
		CScaleformMgr::AddParamInt( importantParameters->b );
		CScaleformMgr::AddParamInt( importantParameters->a );
		CScaleformMgr::AddParamInt( importantParameters->flash );
		CScaleformMgr::EndMethod();
	}
}

void CGameStream::FreezeNextPost()
{
	m_bFreezeNextPost = true;
}

void CGameStream::ClearFrozenPost()
{
	for( int i=0; i < GAMESTREAM_MAX_POSTS; ++i )
	{
		if( m_PostList[i].IsFrozen())
		{
			m_PostList[i].SetFrozen(false);
		}
	}
}

void CGameStream::SetNextPostBackgroundColor( eHUD_COLOURS color )
{
	m_bSetFeedBackgroundColor = true;
	m_bgColor = color;
}

// ---------------------------------------------------------
bool CGameStream::InteractWithScaleformSlow( s32 MovieId )
{
	bool bSuccess = false;

	if( !PsCmp( &m_MinimapNew, &m_MinimapCur ) )
	{
		PsCpy( &m_MinimapCur, &m_MinimapNew );
	}

	int NextReadyToShow = -1;
	int NextReadyToRemove = -1;
	int NextReadyToUpdate = -1;

	do 
	{
		GetNext( &NextReadyToShow, &NextReadyToRemove, &NextReadyToUpdate );

		if( NextReadyToShow >=0 && m_DisableAdd == 0 )
		{
			gstPost* Post = &m_PostList[NextReadyToShow];

			if(!IsPaused() || IsForceRenderOn() || Post->m_bAlwaysRender || Post->IsFrozen())
			{
				// Check if it's been in the queue for longer than it's max lifetime and delete if too old.
				if( m_CurrentSystemTime > (Post->CreatedTime+GAMESTREAM_MAX_LIFETIME) )
				{
					RemoveItem(NextReadyToShow);
					uiDebugf3("CGameStream::InteractWithScaleformSlow -- Removing: %d from queue. ( Older than lifetime )", NextReadyToShow );
					bSuccess = true;
				}
				else
				{
					if( CheckUpdate(NextReadyToShow) )
					{
						Post = &m_PostList[NextReadyToShow];
						uiDebugf3("CGameStream::InteractWithScaleformSlow -- Updating Id:%d", Post->Id);
						if( PostToScaleform( MovieId, Post, GAMESTREAM_METHOD_UPDATE ) )
						{
							SetAlarm( NextReadyToShow, Post->ShowTime );
							Post->Status = GAMESTREAM_STATUS_SHOWING;
							// Don't return here.  Continue processing so that if someone is attempting to post/update the same item every frame we don't prevent the removal of any items
							bSuccess = true;
						}
					}
					else
					{
						uiDebugf3("CGameStream::InteractWithScaleformSlow -- Showing Id:%d", Post->Id);
						if( PostToScaleform( MovieId, Post, GAMESTREAM_METHOD_SET ) )
						{
							SetAlarm( NextReadyToShow, Post->ShowTime );
							Post->Status = GAMESTREAM_STATUS_SHOWING;
							bSuccess = true;
						}
					}
				}
			}
		}

		// Remove next item if one is ready
		if( NextReadyToRemove >=0 )
		{
			gstPost* Post = &m_PostList[NextReadyToRemove];
			uiDebugf3("CGameStream::InteractWithScaleformSlow -- Removing Id:%d", Post->Id);
			if( PostToScaleform( MovieId, Post, GAMESTREAM_METHOD_REMOVE ) )
			{
				m_DisableAdd = 0;
				SetAlarm( NextReadyToRemove, GAMESTREAM_LINGER_TIME );
				Post->Status = GAMESTREAM_STATUS_CLEANUP;
				bSuccess = true;
			}
		}

		if ( NextReadyToUpdate >=0 )
		{
			gstPost* Post = &m_PostList[NextReadyToUpdate];
			uiDebugf3("CGameStream::InteractWithScaleformSlow -- Updating Id:%d", Post->Id);
			if( PostToScaleform( MovieId, Post, GAMESTREAM_METHOD_UPDATE ) )
			{
				SetAlarm( NextReadyToUpdate, Post->ShowTime );
				Post->m_bShouldUpdate = false;
				bSuccess = true;
			}
		}

	} while (m_bSnapFeedItemPositions && ((NextReadyToShow >= 0 && m_DisableAdd == 0) || NextReadyToRemove >= 0 || NextReadyToUpdate >= 0));

	if(bSuccess)
	{
		return true;
	}
	
	if( m_DisableAdd > 0 )
	{
		m_DisableAdd--;
	}
	BusySpinnerUpdate();

	return false;
}

// ---------------------------------------------------------
bool CGameStream::InteractWithScaleform( s32 MovieId )
{
	if( !CScaleformMgr::IsMovieActive(MovieId) )
	{
		return( false );
	}

	if( InteractWithScaleformFast(MovieId) )
	{
		return( true );
	}
	
	if( CanUseScaleform() )
	{
		if( InteractWithScaleformSlow(MovieId) )
		{
			return( true );
		}
	}
	return( false );
}

// ---------------------------------------------------------
bool CGameStream::CheckUpdate( int& ReadyToShowIdx )
{
	// Check for and process an active matching stat post.
	if( HandleMatchingStatsPost(ReadyToShowIdx) )
	{
		return true;
	}

	// Check for and process an active matching ticker post.
	if( HandleMatchingTickerPost(ReadyToShowIdx) )
	{
		return true;
	}

	if( HandleMatchingMessageTextPost(ReadyToShowIdx) )
	{
		return true;
	}

	return false;
}

bool CGameStream::IsPostReadyToShow(gstPost* Post)
{
	bool bIsReadyToShow = false;

	/*
	// If we're waiting for the flush to complete before showing, we shouldn't mark any new feed items as ready to show
	if(!m_HideUntilFlushComplete)
	{
	*/
		switch(Post->Type)
		{
		case GAMESTREAM_TYPE_AWARD:
			{
				int iLastShownAwardTime = -1;

				for( int i = 0; i != GAMESTREAM_MAX_POSTS; ++i )
				{
					if(m_PostList[i].Type == GAMESTREAM_TYPE_AWARD && (m_PostList[i].Status == GAMESTREAM_STATUS_SHOWING || m_PostList[i].Status == GAMESTREAM_STATUS_READYTOSHOW))
					{
						iLastShownAwardTime = MAX(m_PostList[i].iReadyToShowTime, iLastShownAwardTime);
					}
				}

				bIsReadyToShow = iLastShownAwardTime + GAMESTREAM_AWARD_POST_COOLDOWN < m_SysTimer.GetSystemMsTime();

				break;
			}
		default:
			bIsReadyToShow = true;
			break;
		}
	/*
	}
	*/
	return bIsReadyToShow;
}


// Search for a matching "Stats" post which is currently being displayed.
// ---------------------------------------------------------
bool CGameStream::HandleMatchingStatsPost( int& ReadyToShowIdx )
{
	gstPost& ReadyToShowPost = m_PostList[ ReadyToShowIdx ];
	if( ReadyToShowPost.Type == GAMESTREAM_TYPE_STATS )
	{
		for( int i=0; i!=GAMESTREAM_MAX_POSTS; i++ )
		{
			if( m_PostList[i].Type==GAMESTREAM_TYPE_STATS && m_PostList[i].Status==GAMESTREAM_STATUS_SHOWING )
			{
				if( m_PostList[i].Params[0] == ReadyToShowPost.Params[0] &&	// Title
					m_PostList[i].Params[1] == ReadyToShowPost.Params[1] &&	// Body
					m_PostList[i].Params[2] == ReadyToShowPost.Params[2])	// Icon
				{
					m_PostList[i].SetFormattedParam( 3, ReadyToShowPost.Params[3] );
					m_PostList[i].SetFormattedParam( 4, ReadyToShowPost.Params[4] );
					ReadyToShowPost.Status = GAMESTREAM_STATUS_CLEANUP;
					ReadyToShowIdx = i;
					return true;
				}
			}
		}
	}
	return false;
}

// Search for a matching "Ticker" post which is currently being displayed.
// ---------------------------------------------------------
bool CGameStream::HandleMatchingTickerPost( int& ReadyToShowIdx )
{
	gstPost& ReadyToShowPost = m_PostList[ ReadyToShowIdx ];
	if( ReadyToShowPost.Type == GAMESTREAM_TYPE_TICKER )
	{
		for( int i=0; i!=GAMESTREAM_MAX_POSTS; i++ )
		{
			if( m_PostList[i].Type==GAMESTREAM_TYPE_TICKER && m_PostList[i].Status==GAMESTREAM_STATUS_SHOWING )
			{
				if( m_PostList[i].Params[0] == ReadyToShowPost.Params[0] ) // Body
				{
					ReadyToShowPost.Status = GAMESTREAM_STATUS_CLEANUP;
					ReadyToShowIdx = i;
					return true;
				}
			}
		}
	}
	return false;
}

// Search for a matching "MessageText" post which is currently being displayed.
// ---------------------------------------------------------
bool CGameStream::HandleMatchingMessageTextPost( int& ReadyToShowIdx )
{
	gstPost& ReadyToShowPost = m_PostList[ ReadyToShowIdx ];
	if( ReadyToShowPost.Type == GAMESTREAM_TYPE_MESSAGE_TEXT )
	{
		for( int i=0; i!=GAMESTREAM_MAX_POSTS; i++ )
		{
			if( m_PostList[i].Type==GAMESTREAM_TYPE_MESSAGE_TEXT && m_PostList[i].Status==GAMESTREAM_STATUS_SHOWING )
			{
				if( m_PostList[i].Params[0] == ReadyToShowPost.Params[0] && // Body
					m_PostList[i].Params[5] == ReadyToShowPost.Params[5] && // Character name
					m_PostList[i].Params[6] == ReadyToShowPost.Params[6] )	// Subtitle
				{
					ReadyToShowPost.Status = GAMESTREAM_STATUS_CLEANUP;
					ReadyToShowIdx = i;
					return true;
				}
			}
		}
	}
	return false;
}

int CGameStream::GetReplayDirectorPost()
{
	for( int i=0; i!=GAMESTREAM_MAX_POSTS; i++ )
	{
		if( m_PostList[i].Type==GAMESTREAM_TYPE_REPLAY &&
			( m_PostList[i].Status==GAMESTREAM_STATUS_SHOWING || m_PostList[i].Status==GAMESTREAM_STATUS_READYTOSHOW ) )
		{
			if( atoi(&m_PostList[i].Params[0][1]) == DIRECTOR_RECORDING)
			{
				return i;
			}
		}
	}

	return -1;
}

// ---------------------------------------------------------
void CGameStream::GetNext( int* ReadyToShow, int* ReadyToRemove, int* ReadyToUpdate )
{
	ReadyToShow[0] = -1;
	ReadyToRemove[0] = -1;
	ReadyToUpdate[0] = -1;
	
	u32 ShowLowest = 0xffffffff;
	u32 RemoveLowest = 0xffffffff;
	for( int i=0; i!=GAMESTREAM_MAX_POSTS; i++ )
	{
		switch( m_PostList[i].Status )
		{
			case GAMESTREAM_STATUS_READYTOSHOW:
			if( m_PostList[i].Sequence < ShowLowest && CanShowPost(m_PostList[i]))
			{
				ReadyToShow[0] = i;
				ShowLowest = m_PostList[i].Sequence;
			}
			break;

			case GAMESTREAM_STATUS_READYTOREMOVE:
			if( m_PostList[i].Sequence < RemoveLowest )
			{
				ReadyToRemove[0] = i;
				RemoveLowest = m_PostList[i].Sequence;
			}
			break;

			case GAMESTREAM_STATUS_SHOWING:
			if ( m_PostList[i].m_bShouldUpdate )
			{
				ReadyToUpdate[0] = i;
			}
			break;
		
			default:
			break;
		}
	}
	return;
}

bool CGameStream::CanShowPost(const gstPost& post) const
{
	if ( m_AutoPostGameTip && post.Type != GAMESTREAM_TYPE_TOOLTIPS )
	{
		return false;
	}

	if(post.m_bAlwaysRender || post.IsFrozen())
	{
		return true;
	}

	return (!IsPaused() && IsShowEnabled()) || IsForceRenderOn();
}

// ---------------------------------------------------------
CGameStream::gstPost* CGameStream::AllocPost( gstPostType Type, int extraShowTime, bool UNUSED_PARAM(bIsImportant) )
{
	if( !m_IsActive || ( (CPauseMenu::IsActive(PM_SkipVideoEditor)) && (!CLoadingScreens::AreActive()) && (m_iPauseMenuCount >= MAX_ALLOWED_WHILE_PAUSED) ))
	{
		m_bSetFeedBackgroundColor = false;
		return NULL;
	}

	for( int i=0; i!=GAMESTREAM_MAX_POSTS; i++ )
	{
		if( m_PostList[i].Status == GAMESTREAM_STATUS_FREE )
		{
			m_PostList[i].Status = GAMESTREAM_STATUS_NEW;
			m_PostList[i].Type = Type;
			m_PostList[i].Id = i;

			m_PostList[i].SetFrozen(m_bFreezeNextPost);
			m_bFreezeNextPost = false;

			m_PostList[i].m_bSnapIntoPosition = m_bSnapFeedItemPositions;

			SetPostChangeBgColor( &m_PostList[i] );

			//Assign the static data
			m_PostList[i].m_pStaticData = NULL;
			if(uiVerify(Type != GAMESTREAM_TYPE_INVALID))
				m_PostList[i].m_pStaticData = &s_PostTypeStaticData[Type];

			m_PostList[i].ShowTime = m_PostList[i].m_pStaticData->m_ShowTime + extraShowTime;
			m_PostList[i].CreatedTime = m_SysTimer.GetSystemMsTime();
			m_PostList[i].AlarmTime = 0;
			m_PostList[i].FailedCount = 0;
			m_PostList[i].ParamsCount = 0;
			m_PostList[i].m_bDynamicTexture = false;
			m_PostList[i].m_bCacheMessage = true;
			m_PostList[i].Sequence = m_Sequence;
			m_Sequence++;

			// Game Tips can be shown while the video editor is up (video editor loading screen), so let's not check that when seeing if the pause menu is active
			if( (CPauseMenu::IsActive(PM_SkipVideoEditor)) && (!CLoadingScreens::AreActive()) )
			{
				m_iPauseMenuCount++;
			}
			else
			{
				m_iPauseMenuCount = 0;
			}

			return &m_PostList[i];
		}
	}

	m_bSetFeedBackgroundColor = false;
	return NULL;
}

int CGameStream::GetLastShownPhoneActivatableScriptID()
{
	int iLastShownScriptID = -1;

	for( int i = 0; i < GAMESTREAM_MAX_POSTS; ++i )
	{
		if( m_PostList[i].Id == m_iLastShownPhoneActivatableID)
		{
			if(m_PostList[i].Status == GAMESTREAM_STATUS_SHOWING)
			{
				iLastShownScriptID = m_PostList[i].IdScript;
			}

			break;
		}
	}

	return iLastShownScriptID;
}

// ---------------------------------------------------------
void CGameStream::FreePost( int Idx )
{
	Idx = ConvertToIndex( Idx );

	if( uiVerify(Idx>=0 && Idx<GAMESTREAM_MAX_POSTS))
	{
		gstPost &rPost = m_PostList[Idx];
		rPost.ReleaseAndCancelTexture();
		rPost.Reset();
	}
}

// ---------------------------------------------------------
void CGameStream::SetAlarm( int Idx, int Ms )
{
	m_PostList[Idx].AlarmTime = ( m_CurrentTime + Ms );
}

// ---------------------------------------------------------
bool CGameStream::IsAlarm( int Idx )
{
	if ( m_IsPaused && ShouldRender(false) && m_PostList[Idx].Type != GAMESTREAM_TYPE_REPLAY)
	{
		// The feed will still render if we have an item marked as always render - we still need to update the alarm times while paused if this is the case.
		// To overcome the lack of m_currentTime updating, compare how much time has elapsed since we paused against
		// how much longer the feed item needs to display. url:bugstar:2301441
		u32 sysTime = (u32)m_SysTimer.GetMsTime();
		return( (sysTime - m_TimePausedAt) >= (m_PostList[Idx].AlarmTime - m_CurrentTime) );
	}
	return( m_CurrentTime >= m_PostList[Idx].AlarmTime );
}

// ---------------------------------------------------------
int CGameStream::FindParamType( gstPost* Post, char Type )
{
	for( int i=0; i!=GAMESTREAM_MAX_PARAMS; i++ )
	{
		if( Post->Params[i].length() > 0 && Post->Params[i][0] == Type)
		{
			return( i );
		}
	}
	return( -1 );
}

void CGameStream::ClearCache()
{
	m_FeedCache.Reset();
}

void CGameStream::CacheMessage(stFeedItem &feedItem)
{
	if(m_FeedCache.IsFull())
	{
		m_FeedCache.Drop();
	}

	m_FeedCache.Push(feedItem);
}

// ---------------------------------------------------------
bool CGameStream::StrToBool( const char* Str ) const
{
	if( Str != NULL && *Str=='1' )
	{
		return( true );
	}
	return( false );
}

// ---------------------------------------------------------
int CGameStream::MsFromLength( const char* Str ) const
{
	if( Str == NULL )
	{
		return 0;
	}

	return MsFromLength(istrlen(Str));
}

int CGameStream::MsFromLength(int strLen) const
{
	return strLen * GAMESTREAM_MS_PERCHAR;
}


// ---------------------------------------------------------
void CGameStream::PsSet( gstPs* Ps, float x, float y, float w, float h )
{
	Ps->x = x;
	Ps->y = y;
	Ps->w = w;
	Ps->h = h;
}

// ---------------------------------------------------------
bool CGameStream::PsCmp( gstPs* Ps1, gstPs* Ps2 )
{
	if( Ps1->x != Ps2->x || Ps1->y != Ps2->y )
	{
		return( false );
	}
	if( Ps1->w != Ps2->w || Ps1->h != Ps2->h )
	{
		return( false );
	}
	return( true );	
}

// ---------------------------------------------------------
void CGameStream::PsCpy( gstPs* Dst, gstPs* Src )
{
	Dst->x = Src->x;
	Dst->y = Src->y;
	Dst->w = Src->w;
	Dst->h = Src->h;
}

// ---------------------------------------------------------
bool CGameStream::IsShowEnabled() const
{
	for( int s=0; s!=GAMESTREAM_SHOW_FLAG_COUNT; s++ )
	{
		if( m_ShowCounters[s] != 0 )
		{
			return( false );
		}
	}
	return( true );
}

// Check if the state of the game supports the feed display.
// ---------------------------------------------------------
bool CGameStream::IsSafeGameDisplayState( void )
{
	if ((CutSceneManager::GetInstancePtr() && CutSceneManager::GetInstance()->IsRunning()))
	{
		return( false );
	}

	if (CCredits::IsRunning())
	{
		return( false );
	}

	if (cStoreScreenMgr::IsStoreMenuOpen())
	{
		return( false );
	}

	if(SocialClubMenu::IsActive())
	{
		return( false );
	}

	return( true );
}

#if !__FINAL
void CGameStream::gstPost::DebugPrint()
{
	uiDebugf1("CGameStream::gstPost::DebugPrint -- Status: %d", Status);
	uiDebugf1("CGameStream::gstPost::DebugPrint -- Id: %d", Id);
	uiDebugf1("CGameStream::gstPost::DebugPrint -- Type: %d", Type);
	uiDebugf1("CGameStream::gstPost::DebugPrint -- ShowTime: %d", ShowTime);
	uiDebugf1("CGameStream::gstPost::DebugPrint -- AlarmTime: %d", AlarmTime);
	uiDebugf1("CGameStream::gstPost::DebugPrint -- DynamicTexture: %s", m_bDynamicTexture ? "Yes" : "No");
	uiDebugf1("CGameStream::gstPost::DebugPrint -- Sequence: %d", Sequence);

	uiDebugf1("CGameStream::gstPost::DebugPrint -- ParamCount: %d", ParamsCount);
	for (int i = 0; i < ParamsCount; i++)
	{
		uiDebugf1("CGameStream::gstPost::DebugPrint --     %s", Params[i].c_str());
	}
}
#endif





