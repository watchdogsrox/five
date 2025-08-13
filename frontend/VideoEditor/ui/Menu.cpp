/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : Menu.cpp
// PURPOSE : menu of the video editor
// AUTHOR  : Derek Payne
// STARTED : 01/12/2013
//
/////////////////////////////////////////////////////////////////////////////////
#include "frontend/VideoEditor/ui/Menu.h"

#if defined( GTA_REPLAY ) && GTA_REPLAY

// rage
#include "math/amath.h"
#include "system/param.h"
#include "system/service.h"

// fw
#include "rline/rlsystemui.h"
#include "fwlocalisation/templateString.h"
#include "fwlocalisation/textUtil.h"
#include "fwrenderer/renderthread.h"
#include "video/mediaencoder_params.h"
#include "parser/manager.h"

// game
#include "audio/frontendaudioentity.h"
#include "audio/radioaudioentity.h"

#include "frontend/VideoEditor/ui/Menu_parser.h"
#include "frontend/VideoEditor/ui/InstructionalButtons.h"
#include "frontend/VideoEditor/ui/Editor.h"
#include "frontend/VideoEditor/ui/Playback.h"
#include "frontend/VideoEditor/ui/TextTemplate.h"
#include "frontend/VideoEditor/ui/Timeline.h"
#include "frontend/VideoEditor/Core/VideoEditorUtil.h"
#include "frontend/VideoEditor/Core/VideoProjectAudioTrackProvider.h"
#include "frontend/VideoEditor/Views/RawClipFileView.h"
#include "frontend/VideoEditor/Views/VideoProjectFileView.h"
#include "frontend/VideoEditor/Views/ThumbnailFileView.h"
#include "frontend/VideoEditor/Views/FileViewFilter.h"
#include "frontend/VideoEditor/VideoEditorInterface.h"

#include "frontend/BusySpinner.h"
#include "frontend/hud_colour.h"
#include "frontend/MousePointer.h"
#include "frontend/NewHud.h"
#include "Frontend/PauseMenu.h"
#include "frontend/Scaleform/ScaleFormMgr.h"
#include "Frontend/ui_channel.h"
#include "frontend/WarningScreen.h"

#include "frontend/VideoEditor/Views/FileViewFilter.h"
#include "replaycoordinator/ReplayAudioTrackProvider.h"
#include "replaycoordinator/ReplayEditorParameters.h"
#include "replaycoordinator/storage/Montage.h"
#include "replaycoordinator/Storage/MontageText.h"

#include "Peds/ped.h"
#include "text/Text.h"
#include "text/TextFormat.h"
#include "text/TextConversion.h"
#include "scene/world/GameWorld.h"
#include "script/script.h"
#include "System/controlMgr.h"

#include "Network/Cloud/VideoUploadManager.h"

FRONTEND_OPTIMISATIONS();
//OPTIMISATIONS_OFF();

PARAM(rse_allowOtherUserFiles, "Allow the use of other user files in the R* Editor");

CVideoEditorMenuArray CVideoEditorMenu::ms_MenuArray;

s32 CVideoEditorMenu::ms_iMenuIdForColumn[MAX_COL_NUMs] = { -1 };
s32 CVideoEditorMenu::ms_iCurrentItem[MAX_COL_NUMs];
s32 CVideoEditorMenu::ms_iCurrentColumn;
s32 CVideoEditorMenu::ms_iColumnHoveredOver = 0;
s32 CVideoEditorMenu::ms_iCurrentScrollOffset[MAX_COL_NUMs];
s32 CVideoEditorMenu::ms_iClipThumbnailInUse = -1;
u32 CVideoEditorMenu::ms_lastSoundHashPlayed = 0;
Vector2 CVideoEditorMenu::ms_vStagePos;
Vector2 CVideoEditorMenu::ms_vStageSize;
s32 CVideoEditorMenu::ms_previewDataId[MAX_COL_NUMs] = { -1 };
s32 CVideoEditorMenu::ms_previewDataSort[MAX_COL_NUMs] = { -1 };
s32 CVideoEditorMenu::ms_previewDataFilter[MAX_COL_NUMs] = { -1 };
bool CVideoEditorMenu::ms_previewDataInversion[MAX_COL_NUMs] = { true };
bool CVideoEditorMenu::ms_isUpdatingUpload = false;
bool CVideoEditorMenu::ms_forceUpdatingUpload = false;
bool CVideoEditorMenu::ms_isHighlightedVideoUploading = false;
bool CVideoEditorMenu::ms_clipFavouritesNeedingSaved = false;
#if RSG_PC
s32 CVideoEditorMenu::ms_mousePressedForToggle = 0;
#endif // #if RSG_PC
#if RSG_ORBIS
s32 CVideoEditorMenu::ms_userLastShownUGCMessage = 0;
#endif
static const s32 sc_galleryPreviewId = ATSTRINGHASH("POPULATE_GALLERY_PREVIEW",0x3bc5b337);
static const s32 sc_loadProjPreviewId = ATSTRINGHASH("POPULATE_LOAD_PREVIEW",0x394c5397);
static const s32 sc_clipPreviewId = ATSTRINGHASH("POPULATE_CLIP_PREVIEW",0x2df93d10);
static const s32 sc_ambientPreviewId =  ATSTRINGHASH("POPULATE_AMBIENT_PREVIEW",0x7ae56a6a);
static const s32 sc_audioRadioStationPreviewId =  ATSTRINGHASH("POPULATE_RADIO_STATION_PREVIEW",0x15746e56);
static const s32 sc_audioScorePreviewId =  ATSTRINGHASH("POPULATE_SCORE_PREVIEW",0xeb3f4c1a);
static const s32 sc_filePreviewId = ATSTRINGHASH("FILE_PREVIEW",0x948589CC);

static const s32 sc_sortDate			= ATSTRINGHASH("SORT_BY_DATE",0xB549D88D);
static const s32 sc_sortName			= ATSTRINGHASH("SORT_BY_NAME",0x61A68EE1);
static const s32 sc_sortSize			= ATSTRINGHASH("SORT_BY_SIZE",0x9272CED0);
static const s32 sc_sortCreatedToday	= ATSTRINGHASH("SORT_BY_TODAY",0xFBFB63C5);
static const s32 sc_sortClipsInProject	= ATSTRINGHASH("SORT_BY_IN_PROJECT",0xBF8CD0A5);
static const s32 sc_sortAllUsers		= ATSTRINGHASH("SORT_BY_ALL_USERS",0x6470DC0F);
static const s32 sc_sortOtherUsers		= ATSTRINGHASH("SORT_BY_OTHER_USERS",0x520c19fd);
static const s32 sc_sortFav				= ATSTRINGHASH("SORT_BY_FAV",0x90F0F4AC);
static const s32 sc_sortMarkDelete		= ATSTRINGHASH("SORT_BY_DELETE",0x6BE94ADC);

atArray<CVideoEditorPreviewData> CVideoEditorMenu::ms_PreviewData[MAX_COL_NUMs];
u32 CVideoEditorMenu::ms_inProjectToDeleteCount = 0;
u32 CVideoEditorMenu::ms_otherUserToDeleteCount = 0;

CVideoEditorUserConfirmationData CVideoEditorMenu::ms_UserConfirmationScreen;
CThumbnailFileView const* CVideoEditorMenu::ms_pendingDeleteView = NULL;
bool CVideoEditorMenu::ms_multiDeleteLastUsed = false;


#define MAX_ITEMS_IN_COLUMN (16)
#define MOUSE_WHEEL_ITEM_SCROLL (3)

#if RSG_PC
#define ROCKSTAR_EDITOR_TUTORIAL_URL "http://socialclub.rockstargames.com/games/gtav/pc/tutorials/editor"  // 2205119, new 2450485
#endif // RSG_PC

#if RSG_ORBIS
#define ROCKSTAR_EDITOR_TUTORIAL_URL "http://socialclub.rockstargames.com/games/gtav/ps4/tutorials/editor"  // 2450485
#endif // RSG_ORBIS

#if RSG_DURANGO
#define ROCKSTAR_EDITOR_TUTORIAL_URL "http://socialclub.rockstargames.com/games/gtav/xboxone/tutorials/editor"  // 2450485
#endif // RSG_DURANGO

bool CVideoEditorPreviewData::BelongsToCurrentUser() const
{
    u64 const c_activeUserId = CVideoEditorInterface::GetActiveUserId();
    return c_activeUserId == m_ownerId;
}

/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorMenu::Update
// PURPOSE:	main UT update
/////////////////////////////////////////////////////////////////////////////////////////
bool CVideoEditorMenu::Update()
{
	static u32 s_MusicPreviewTimer = fwTimer::GetSystemTimeInMilliseconds();
	static u32 s_SoundHashLastFinished = 0;

	if ( IsMenuSelected() && IsOnAudioMenu() && ms_iCurrentColumn == COL_2 )
	{
		if (ms_lastSoundHashPlayed != 0 && !g_RadioAudioEntity.IsReplayMusicPreviewPlaying())  // if music has stopped by itself, reset the timer
		{
			ShowAnimatedAudioIcon(false);
			s_SoundHashLastFinished = ms_lastSoundHashPlayed;
			ms_lastSoundHashPlayed = 0;
		}

#define __MUSIC_PREVIEW_DELAY (2000)

		s32 const c_indexToPreview = ms_iCurrentItem[ms_iCurrentColumn];

		atArray<CVideoEditorPreviewData>& previewDataArray = ms_PreviewData[ms_iCurrentColumn];
		u32 const c_soundHash = c_indexToPreview >= 0 && c_indexToPreview < previewDataArray.GetCount() ?
			previewDataArray[c_indexToPreview].m_soundHash : 0;

		if (s_SoundHashLastFinished != c_soundHash)
		{
			s_SoundHashLastFinished = 0;
		}

		if (g_RadioAudioEntity.IsReplayMusicPreviewPlaying() && ms_lastSoundHashPlayed != c_soundHash)  // if the current playing track is different, stop the track & reset the timer
		{
			ShowAnimatedAudioIcon(false);

			g_RadioAudioEntity.StopReplayTrackPreview();
			s_SoundHashLastFinished = ms_lastSoundHashPlayed;
			ms_lastSoundHashPlayed = 0;
			s_MusicPreviewTimer = fwTimer::GetSystemTimeInMilliseconds();
		}

		// if its time to play the preview track...
		if (!s_SoundHashLastFinished != 0 && !g_RadioAudioEntity.IsReplayMusicPreviewPlaying() && fwTimer::GetSystemTimeInMilliseconds() > s_MusicPreviewTimer + __MUSIC_PREVIEW_DELAY)
		{
			s_MusicPreviewTimer = fwTimer::GetSystemTimeInMilliseconds();

			if (c_soundHash != 0)
			{
				ShowAnimatedAudioIcon(true);

				g_RadioAudioEntity.StartReplayTrackPreview(c_soundHash);  // play the track
				ms_lastSoundHashPlayed = c_soundHash;
			}
		}
	}
	else
	{
		// turn off the music if we are no longer on the menu
		if (ms_lastSoundHashPlayed != 0 || (IsMenuSelected() && g_RadioAudioEntity.IsReplayMusicPreviewPlaying()))
		{
			g_RadioAudioEntity.StopReplayTrackPreview();
			ms_lastSoundHashPlayed = 0;
		}
	}

	if( ms_pendingDeleteView )
	{
		if( ms_pendingDeleteView->hasDeleteFailed() )
		{
			if (ms_multiDeleteLastUsed)
			{
				// they'll be no files to show, so back up
				GoBackOnePane( -1 );
				RebuildMenu();
				SetErrorState("MULTI_DELETE_FAILED" );
			}
			else
			{
				SetErrorState("DELETE_FAILED" );
			}
			
			ms_pendingDeleteView = NULL;
			ms_multiDeleteLastUsed = false;
		}
		else if( ms_pendingDeleteView->hasDeleteFinished() )
		{
			ReleaseThumbnailOfItem();

			atArray<CVideoEditorPreviewData>& previewDataArray = ms_PreviewData[ms_iCurrentColumn];
			s32 currentItem = ms_iCurrentItem[ ms_iCurrentColumn ];

			s32 const c_oldArrayCount = previewDataArray.GetCount();
			bool goBackOnePane = true;
			if (ms_multiDeleteLastUsed)
			{
				currentItem = 0;
				// all multidelete all done on marked for delete filter, so just go back a pane
				// if it's wanted in the future on any page, you'd have to loop through previewdata to see if any files aren't marked
			}
			else
			{
				previewDataArray.erase( previewDataArray.begin() + currentItem );
				currentItem -= ( currentItem == c_oldArrayCount -1 ) ? 1 : 0;
				goBackOnePane = c_oldArrayCount == 1;
			}

			if( goBackOnePane )
			{
				GoBackOnePane( -1 );
				RebuildMenu();
 			}
			else
			{
				ClearPreviewData( ms_iCurrentColumn, true, true );
				RebuildMenu();
				GoToItem( currentItem, ms_iCurrentColumn, GOTO_REBUILD_AUTO );
			}

			ms_pendingDeleteView = NULL;
			ms_multiDeleteLastUsed = false;
		}
		else if (!ms_pendingDeleteView->isDeletingFile())
		{
			// if no longer deleting, but not succeeded or failed ... it's probably not even started a delete
			// if so, clean up, take a step back
			// deals with specific case of marked for delete filter having all files unmarked, but needing a refresh
			ReleaseThumbnailOfItem();

			GoBackOnePane( -1 );
			RebuildMenu();

			ms_pendingDeleteView = NULL;
			ms_multiDeleteLastUsed = false;
		}
	}

	UpdateUploadValues();

	return ms_pendingDeleteView != NULL;
}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorMenu::Open
// PURPOSE:	called each time we open the video editor menu
/////////////////////////////////////////////////////////////////////////////////////////
void CVideoEditorMenu::Open()
{
	ms_UserConfirmationScreen.Reset();

	for (s32 i = 0; i < MAX_COL_NUMs; i++)
	{
		ms_iCurrentItem[i] = 0;
		ms_iMenuIdForColumn[i] = -1;
		ms_PreviewData[i].Reset();
		ms_iCurrentScrollOffset[i] = 0;
	}

	ms_iColumnHoveredOver = 0;
	ms_iCurrentColumn = 0;

#if RSG_PC
	ms_mousePressedForToggle = 0;
#endif // RSG_PC

	ms_vStagePos = SetupStagePosition();
	ms_vStageSize = SetupStageSize();

	// load xml
	parSettings settings = PARSER.Settings();
	
	settings.SetFlag(parSettings::CULL_OTHER_PLATFORM_DATA, true);
	PARSER.LoadObject("common:/data/ui/VideoEditorMenu.XML", "", ms_MenuArray, &settings);

	uiAssertf(ms_MenuArray.CVideoEditorMenuItems.GetCount() != 0, "No menu items to display!");

	ClearPreviewData( -1, true, true );
}

bool CVideoEditorMenu::IsOpen()
{
    return ms_MenuArray.CVideoEditorMenuItems.GetCount() != 0;
}

/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorMenu::Close
// PURPOSE:	called each time we close the video editor menu
/////////////////////////////////////////////////////////////////////////////////////////
void CVideoEditorMenu::Close()
{
	ReleaseThumbnailOfItem();
	ms_UserConfirmationScreen.Reset();
	ms_MenuArray.CVideoEditorMenuItems.Reset();  // remove XML data
}



void CVideoEditorMenu::SetErrorState( const char *pHashString, char const * const literalString, const char* literalString2 )
{
	for (s32 i=0; i < ms_MenuArray.CVideoEditorErrorMessages.GetCount(); i++)
	{
		if (atStringHash(pHashString) == ms_MenuArray.CVideoEditorErrorMessages[i].type)
		{
			ms_UserConfirmationScreen.SetWithHeader( ms_MenuArray.CVideoEditorErrorMessages[i].body, ms_MenuArray.CVideoEditorErrorMessages[i].header, FE_WARNING_OK, literalString, literalString2 );

			return;
		}
	}

}

void CVideoEditorMenu::SetUserConfirmationScreen(const char *pTextLabel, const char *pTitle, u32 const type, bool const allowSpinner, char const * const literalString, char const * const literalString2 )
{
	if( pTitle == NULL )
	{
		ms_UserConfirmationScreen.Set( atStringHash(pTextLabel), type, literalString, literalString2, allowSpinner );
	}
	else
	{
		ms_UserConfirmationScreen.SetWithHeader( atStringHash(pTextLabel), atStringHash( pTitle), type, literalString, literalString2 );
	}	
}


void CVideoEditorMenu::SetUserConfirmationScreenWithAction(const char *pAction, const char *pTextLabel, const char *pTitle, u32 const type, bool const allowSpinner, char const * const literalString, char const * const literalString2 )
{
	if( pTitle == NULL )
	{
		ms_UserConfirmationScreen.Set(atStringHash(pAction), atStringHash(pTextLabel), type, literalString, literalString2, allowSpinner );
	}
	else
	{
		ms_UserConfirmationScreen.SetWithHeader(atStringHash(pAction), atStringHash(pTextLabel), atStringHash( pTitle), type, literalString, literalString2 );
	}
	
}


bool CVideoEditorMenu::CheckForUserConfirmationScreens()
{
	if (ms_UserConfirmationScreen.IsActive())
	{
		atHashString TriggerHash = ms_UserConfirmationScreen.GetHash();
#if RSG_PC
		atHashString TriggerBodyHash = ms_UserConfirmationScreen.GetBodyText();
#endif

		//
		// some hashes we need to alter if we dont have a valid project:
		//
		if (!CVideoEditorUi::GetProject() || CVideoEditorUi::GetProject()->GetClipCount() <= 0)
		{
			if( TriggerHash == ATSTRINGHASH("TRIGGER_DIRECTOR_MODE",0x7eaab768) )
			{
				if (CTheScripts::GetIsInDirectorMode())
				{
					ms_UserConfirmationScreen.Set(TriggerHash, atStringHash("VE_DIR_TRAILER_SURE"), FE_WARNING_YES_NO );
				}
			}

			if (TriggerHash == ATSTRINGHASH("TRIGGER_SAVE",0x3cc31922))
			{
				ms_UserConfirmationScreen.Set(atStringHash("VE_CANNOTSAVE"), FE_WARNING_OK );
			}

			if( TriggerHash == ATSTRINGHASH("TRIGGER_EXP_SIZECHECK",0xC99FC3D8) )
			{
				ms_UserConfirmationScreen.Set(atStringHash("VE_CANNOTPUB"), FE_WARNING_OK );
				TriggerHash.Clear();
			}

			if (TriggerHash == ATSTRINGHASH("TRIGGER_FULLSCREEN_PREVIEW",0xedea9449))
			{
				ms_UserConfirmationScreen.Set(atStringHash("VE_CANNOTPREV"), FE_WARNING_OK );
			}
		}
		else if( g_SysService.IsConstrained() )
		{
			if( TriggerHash == ATSTRINGHASH("TRIGGER_EXP_SIZECHECK",0xC99FC3D8) )
			{
				ms_UserConfirmationScreen.Set(TriggerHash, atStringHash("VE_EXP_CONSTRAINED"), FE_WARNING_OK );
				TriggerHash.Clear();
			}
		}

		CWarningScreen::SetMessageWithHeader( WARNING_MESSAGE_STANDARD, 
			ms_UserConfirmationScreen.GetHeaderText(), 
			ms_UserConfirmationScreen.GetBodyText(), 
			(eWarningButtonFlags)ms_UserConfirmationScreen.GetType(),
			false,
			0,
			ms_UserConfirmationScreen.GetLiteralString(),
			ms_UserConfirmationScreen.GetLiteralString2(),
			true,
			ms_UserConfirmationScreen.GetAllowSpinner()
			);

		eWarningButtonFlags result = CWarningScreen::CheckAllInput(false);

		if( result == FE_WARNING_YES || result == FE_WARNING_OK )
		{
			bool bJumpAfterTrigger = false;
			ms_UserConfirmationScreen.Reset();

#if RSG_PC
			// B* 2087772 : Just in case the user has locked the computer during export, force the timeline to be redraw
			if (TriggerBodyHash == ATSTRINGHASH("VE_BAKE_FIN_NAMED",0x4e6b9a97) ||  TriggerBodyHash == ATSTRINGHASH("VE_BAKE_FIN",0x7b183eea))
			{
				CVideoEditorUi::RebuildTimeline();
			}
#endif
			// if the user clicks "yes/OK" then decide what action to take based on hash
			if (TriggerHash == ATSTRINGHASH("TRIGGER_NEW_BLANK",0x861dbd1a))
			{
				bJumpAfterTrigger = CVideoEditorUi::TriggerNewBlank();
			}
			else if (TriggerHash == ATSTRINGHASH("TRIGGER_DIRECTOR_MODE",0x7eaab768))
			{
				CVideoEditorUi::TriggerDirectorMode();
			}
			else if (TriggerHash == ATSTRINGHASH("TRIGGER_PREVIEW_CLIP",0xf1830098))
			{
				s32 const c_fileViewIndex = GetFileViewIndexForItem( ms_iCurrentColumn, ms_iCurrentItem[ms_iCurrentColumn] );
				CVideoEditorUi::TriggerPreviewClip( (u32)c_fileViewIndex );
			}
			else if (TriggerHash == ATSTRINGHASH("TRIGGER_SAVE",0x3cc31922) )
			{
				CVideoEditorUi::TriggerSave( false );
			}
			else if( TriggerHash == ATSTRINGHASH( "TRIGGER_OVERWRITE_PROJ_NEW_NAME", 0x2D3A2819 ) )
			{
				CVideoEditorUi::TriggerSaveAs( false );
			}
            else if (TriggerHash == ATSTRINGHASH("TRIGGER_EXP_DURCHECK",0x960FF080))
            {
                CVideoEditorUi::CheckExportDuration();
            }
			else if (TriggerHash == ATSTRINGHASH("TRIGGER_EXP_SIZECHECK",0xC99FC3D8))
			{
				CVideoEditorUi::CheckExportLimits();
			}
			else if( TriggerHash == ATSTRINGHASH("TRIGGER_EXP_PROMPT2", 0x88FB31EA ) )
			{
				CVideoEditorUi::TriggerAdditionalExportPrompt();
			}
			else if( TriggerHash == ATSTRINGHASH("TRIGGER_PUBLISH",0x0bad94d7) )
			{
				CVideoEditorUi::TriggerExport();
			}
			else if( TriggerHash == ATSTRINGHASH("TRIGGER_OVRW_UNAVAIL", 0x8AC43D16) )
			{
				 // NOP
			}
			else if (TriggerHash == ATSTRINGHASH("TRIGGER_AUTOMATED",0x39a42354))
			{
				CVideoEditorUi::TriggerAutomatedMovie();
			}
			else if (TriggerHash == ATSTRINGHASH("TRIGGER_FULLSCREEN_PREVIEW",0xedea9449))
			{
				CVideoEditorUi::TriggerFullScreenPreview();
			}
			else if (TriggerHash == ATSTRINGHASH("TRIGGER_EDIT_CLIP",0x14227a70))
			{
				CVideoEditorUi::TriggerEditClip();
			}
			else if (TriggerHash == ATSTRINGHASH("TRIGGER_OVERWRITE_CLIP",0x2257104a))
			{
				CVideoEditorTimeline::TriggerOverwriteClip();
			}
			else if (TriggerHash == ATSTRINGHASH("TRIGGER_ADD_TEXT_TO_TIMELINE",0xb3f24b1b))
			{
				if (CTextTemplate::GetTextIndexEditing() != -1)
				{
					CVideoEditorTimeline::DeleteTextClipFromTimeline((u32)CTextTemplate::GetTextIndexEditing());
					CVideoEditorUi::GetProject()->DeleteOverlayText(CTextTemplate::GetTextIndexEditing());
				}

				CVideoEditorUi::TriggerAddTextToTimeline();
			}
			else if (TriggerHash == ATSTRINGHASH("TRIGGER_LOAD",0xd3a71fc0))
			{
				s32 const c_fileViewIndex = GetFileViewIndexForItem( ms_iCurrentColumn, ms_iCurrentItem[ms_iCurrentColumn] );
				CVideoEditorUi::TriggerLoad( (u32)c_fileViewIndex );
			}
			else if (TriggerHash == ATSTRINGHASH("TRIGGER_DELETE_CLIP_FROM_TIMELINE",0x47ef1d66))
			{
				CVideoEditorUi::DeleteSelectedClip();
			}
			else if (TriggerHash == ATSTRINGHASH("TRIGGER_DELETE_CLIP_FROM_STORAGE",0x9EC7F572))
			{
				CRawClipFileView* clipView = CVideoEditorUi::GetClipFileView();
				if( clipView )
				{
					bool const c_onDeleteFilter = IsOnMarkedForDeleteFilter();
					bool const c_isMultiDelete = IsMultiDelete();

					if (!c_onDeleteFilter || c_isMultiDelete)
					{
						ms_pendingDeleteView = (CThumbnailFileView*)clipView;
						ReleaseThumbnailOfItem();

						if (c_isMultiDelete)
						{
							ms_multiDeleteLastUsed = true;
							clipView->deleteMarkedFiles();
						}
						else
						{
							s32 const c_fileViewIndex = GetFileViewIndexForItem( ms_iCurrentColumn, ms_iCurrentItem[ms_iCurrentColumn] );
							clipView->startDeleteFile( c_fileViewIndex );
						}
					}
				}
			}
#if defined(VIDEO_PLAYBACK_ENABLED) && VIDEO_PLAYBACK_ENABLED			
			// upload to YouTube
			else if (TriggerHash == ATSTRINGHASH("TRIGGER_UPLOAD_VIDEO",0x88bdbad1))
			{
				if (VideoUploadManager::GetInstance().IsUploadEnabled())
				{
					s32 const c_itemIndex = GetFileViewIndexForItem( ms_iCurrentColumn, ms_iCurrentItem[ms_iCurrentColumn] );

					CVideoFileView* videoView = CVideoEditorUi::GetVideoFileView();
					if( videoView )
					{
						if( videoView->IsValidVideoReference( c_itemIndex ) )
						{
							s64 contentId = 0;
#if USE_ORBIS_UPLOAD
							contentId = videoView->getVideoContentId( c_itemIndex );
#endif // USE_ORBIS_UPLOAD

							char pathBuffer[RAGE_MAX_PATH];
							videoView->getFilePath( c_itemIndex, pathBuffer );
							char dateBuffer[RAGE_MAX_PATH];
							videoView->getFileDateString( c_itemIndex, dateBuffer);
							char displayNameBuffer[RAGE_MAX_PATH];
							videoView->getDisplayName(c_itemIndex, displayNameBuffer);
							VideoUploadManager::GetInstance().RequestPostVideo(pathBuffer, videoView->getVideoDurationMs(c_itemIndex), dateBuffer, displayNameBuffer, contentId);
						}
						else
						{
							//ClearData( CVideoEditorMenu::GetCurrentColumn() );
							//videoView->refresh( false );
							//TODO - In-situ rebuild the menu list
						}
					}
				}
				else
				{
					// if no upload enabled, must have lost connection at this point
					SetUserConfirmationScreen( "YT_DISCON" );
				}
			}
			else if (TriggerHash == ATSTRINGHASH("TRIGGER_STOP_AND_UPLOAD_NEW_VIDEO",0xb8b3e670))
			{
				if (VideoUploadManager::GetInstance().IsProcessing())
				{
					SetUserConfirmationScreen( "VEUI_UPLOAD_CANT_CANCEL" );
				}
				else
				{
					s32 const c_itemIndex = GetFileViewIndexForItem( ms_iCurrentColumn, ms_iCurrentItem[ms_iCurrentColumn] );

	#if !USE_ORBIS_UPLOAD
					// Cancel upload
					VideoUploadManager::GetInstance().StopUpload(true, true, true);
	#endif	// !USE_ORBIS_UPLOAD
					// start the new one
					CVideoFileView* videoView = CVideoEditorUi::GetVideoFileView();
					if( videoView )
					{
						if( videoView->IsValidVideoReference( c_itemIndex ) )
						{
							s64 contentId = 0;
	#if USE_ORBIS_UPLOAD
							contentId = videoView->getVideoContentId( c_itemIndex );
	#endif // USE_ORBIS_UPLOAD

							char pathBuffer[RAGE_MAX_PATH];
							videoView->getFilePath( c_itemIndex, pathBuffer );
							char dateBuffer[RAGE_MAX_PATH];
							videoView->getFileDateString( c_itemIndex, dateBuffer);
							char displayNameBuffer[RAGE_MAX_PATH];
							videoView->getDisplayName(c_itemIndex, displayNameBuffer);
							VideoUploadManager::GetInstance().RequestPostVideo(pathBuffer, videoView->getVideoDurationMs(c_itemIndex), dateBuffer, displayNameBuffer, contentId);
						}
						else
						{
							//ClearData( CVideoEditorMenu::GetCurrentColumn() );
							//videoView->refresh( false );
							//TODO - In-situ rebuild the menu list
						}
					}
				}


				
				CVideoEditorInstructionalButtons::Refresh();
			}
#if !USE_ORBIS_UPLOAD
			else if (TriggerHash == ATSTRINGHASH("TRIGGER_UPLOAD_PAUSE",0x23146141))
			{
				if (VideoUploadManager::GetInstance().IsProcessing())
				{
					SetUserConfirmationScreen( "VEUI_UPLOAD_CANT_PAUSE" );
				}
				else
				{
					VideoUploadManager::GetInstance().SetForcePause(true);
					CVideoEditorInstructionalButtons::Refresh();
				}
			}
			else if (TriggerHash == ATSTRINGHASH("TRIGGER_UPLOAD_RESUME",0x476956f4))
			{
				VideoUploadManager::GetInstance().SetForcePause(false);
				CVideoEditorInstructionalButtons::Refresh();
			}
			else if (TriggerHash == ATSTRINGHASH("TRIGGER_UPLOAD_CANCEL",0x2f7c928b))
			{
				if (VideoUploadManager::GetInstance().IsProcessing())
				{
					SetUserConfirmationScreen( "VEUI_UPLOAD_CANT_CANCEL" );
				}
				else
				{
					VideoUploadManager::GetInstance().StopUpload(true, true, true);
				}
				CVideoEditorInstructionalButtons::Refresh();
			}
			else if (TriggerHash == ATSTRINGHASH("TRIGGER_UPLOAD_SUCCESS",0x434636eb))
			{
				VideoUploadManager::GetInstance().ReturnConfirmationResult(true);
				CVideoEditorInstructionalButtons::Refresh();
			}
#endif // !USE_ORBIS_UPLOAD

#endif // defined(VIDEO_PLAYBACK_ENABLED) && VIDEO_PLAYBACK_ENABLED

			else if (TriggerHash == ATSTRINGHASH("TRIGGER_DELETE_PROJ_FROM_STORAGE",0x9B184994))
			{
				CVideoProjectFileView* projView = CVideoEditorUi::GetProjectFileView();
				if( projView )
				{
					bool const c_onDeleteFilter = IsOnMarkedForDeleteFilter();
					bool const c_isMultiDelete = IsMultiDelete();

					if (!c_onDeleteFilter || c_isMultiDelete)
					{
						ms_pendingDeleteView = (CThumbnailFileView*)projView;
						ReleaseThumbnailOfItem();

						if (c_isMultiDelete)
						{
							ms_multiDeleteLastUsed = true;
							projView->deleteMarkedFiles();
						}
						else
						{
							s32 const c_fileViewIndex = GetFileViewIndexForItem( ms_iCurrentColumn, ms_iCurrentItem[ms_iCurrentColumn] );
							projView->startDeleteFile( c_fileViewIndex );
						}
					}
				}
			}

#if VIDEO_PLAYBACK_ENABLED
			else if (TriggerHash == ATSTRINGHASH("TRIGGER_DELETE_VID_FROM_STORAGE",0x26C33585))
			{
				CVideoFileView* videoView = CVideoEditorUi::GetVideoFileView();
				if( videoView )
				{
					bool const c_onDeleteFilter = IsOnMarkedForDeleteFilter();
					bool const c_isMultiDelete = c_onDeleteFilter && IsMultiDelete();

					if (!c_onDeleteFilter || c_isMultiDelete)
					{
						ms_pendingDeleteView = (CThumbnailFileView*)videoView;
						ReleaseThumbnailOfItem();

						if (c_isMultiDelete)
						{
							ms_multiDeleteLastUsed = true;
							videoView->deleteMarkedFiles();
						}
						else
						{
							s32 const c_fileViewIndex = GetFileViewIndexForItem( ms_iCurrentColumn, ms_iCurrentItem[ms_iCurrentColumn] );
							videoView->startDeleteFile( c_fileViewIndex );
						}
					}
				}
			}

#endif // VIDEO_PLAYBACK_ENABLED

			else if (TriggerHash == ATSTRINGHASH("TRIGGER_EXIT",0xf62d21bc))
			{
				ExitToMainMenu();
			}
			#if RSG_PC
			else if (TriggerHash == ATSTRINGHASH("TRIGGER_QUIT", 0xCD88E447))
			{
				CPauseMenu::SetGameWantsToExitToWindows(true);
				NetworkInterface::GetNetworkExitFlow().StartShutdownTasks();
			}
			#endif
			else if (TriggerHash == ATSTRINGHASH("WANT_TO_SAVE_BEFORE_EXIT",0x05756e16))
			{
				CVideoEditorUi::TriggerSave( true );
			}
			else if (TriggerHash == ATSTRINGHASH("WANT_TO_EXIT",0xeabb5f49))
			{
				CVideoEditorUi::Close();
			}

			if (bJumpAfterTrigger)
			{
				JumpPaneOnAction();
			}
		}
		else if ( result == FE_WARNING_NO || result == FE_WARNING_CANCEL || result == FE_WARNING_NO_ON_X )
		{
			if (TriggerHash == ATSTRINGHASH("TRIGGER_ADD_TEXT_TO_TIMELINE",0xb3f24b1b))
			{
				CVideoEditorUi::TriggerEditClip();
			}
			else if (TriggerHash == ATSTRINGHASH("WANT_TO_SAVE_BEFORE_EXIT",0x05756e16))
			{
				ExitToMainMenu();
			}
			else if( TriggerHash == ATSTRINGHASH( "TRIGGER_OVERWRITE_PROJ_NEW_NAME", 0x2D3A2819 ) )
			{
				CVideoEditorUi::SetActiveState();
			}

			ms_UserConfirmationScreen.Reset();
		}
		else if ( result == FE_WARNING_CANCEL_ON_B )
		{
			if (TriggerHash == ATSTRINGHASH("WANT_TO_SAVE_BEFORE_EXIT",0x05756e16))
			{
				CVideoEditorUi::SetActiveState();
			}

			ms_UserConfirmationScreen.Reset();
		}
		else if ( result == FE_WARNING_OK )
		{
			ms_UserConfirmationScreen.Reset();
		}

		return true;
	}

	return false;
}



void CVideoEditorMenu::SetupStartingPanel(const atHashString startingMenu)
{
	if (CScaleformMgr::BeginMethod(CVideoEditorUi::GetMovieId(), SF_BASE_CLASS_VIDEO_EDITOR, "SET_PAGE_HEADING"))
	{
		CScaleformMgr::AddParamString(TheText.Get("PM_PANE_MOV"));
		CScaleformMgr::EndMethod();
	}

	CVideoEditorUi::ShowMenu(startingMenu);
}


void CVideoEditorMenu::RefreshProjectHeading()
{
    PathBuffer filenameBuffer;

	if (IsProjectValid())
	{
		if (CVideoEditorUi::HasProjectBeenEdited())
		{
            filenameBuffer.Set( "* " );
		}

        filenameBuffer.append( CVideoEditorUi::GetProject()->GetName() );
	}

	if (CScaleformMgr::BeginMethod(CVideoEditorUi::GetMovieId(), SF_BASE_CLASS_VIDEO_EDITOR, "SET_PAGE_HEADING_RIGHT"))
	{
		CScaleformMgr::AddParamString( filenameBuffer.getBuffer(), false);
		CScaleformMgr::EndMethod();
	}
}


void CVideoEditorMenu::TurnOffMenu(const bool bClearContents)
{
	if( IsOnAudioMenu() )
	{
		// reset all the tree to be item 0
		for (s32 i = 0; i < MAX_COL_NUMs; i++)
		{
			SetColumnActive(i);
			GoToItem( 0, i, GOTO_REBUILD_AUTO );
			UnHighlight();
		}

		if (bClearContents)
		{
			for (s32 i = 0; i < MAX_COL_NUMs; i++)
			{
				RemoveColumn(i);
			}
		}
	}

	ReleaseThumbnailOfItem();
}


/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorMenu::SetupStagePosition
// PURPOSE:	returns the position of the stage area (large thumbnail)
/////////////////////////////////////////////////////////////////////////////////////////
Vector2 CVideoEditorMenu::SetupStagePosition()
{
	float fX = 0.0f;
	float fY = 0.0f;

	if (CVideoEditorTimeline::ms_ComplexObject[TIMELINE_OBJECT_STAGE].IsActive())
	{
		fX = 0.457f; // _x:585 @ 16:9
		fY = 0.246f; // _y:177 @ 16:9

		float const c_aspectRatio = CHudTools::GetAspectRatio();
		float const c_SIXTEEN_BY_NINE = 16.0f/9.0f;
		float const c_difference = (c_SIXTEEN_BY_NINE / c_aspectRatio);
		float const c_finalX = 0.5f - ((0.5f - fX) * c_difference);

		if(!CHudTools::IsSuperWideScreen())
			fX = c_finalX;
	}

	return Vector2(fX, fY);
}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorMenu::SetupStageSize
// PURPOSE:	returns the size of the stage area (large thumbnail)
/////////////////////////////////////////////////////////////////////////////////////////
Vector2 CVideoEditorMenu::SetupStageSize()
{
	return CVideoEditorTimeline::GetStageSize();
}

#if RSG_PC
/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorMenu::ToggleOptionWithMouseViaArrows
// PURPOSE:	toggles the menu option based on the ms_mousePressedForToggle flag
/////////////////////////////////////////////////////////////////////////////////////////
bool CVideoEditorMenu::ToggleOptionWithMouseViaArrows()
{
	static u32 s_toggleHoldTimer = fwTimer::GetSystemTimeInMilliseconds();

	if (ms_mousePressedForToggle != 0)
	{
		const bool c_mouseHeldThisFrame = (ioMouse::GetButtons() & ioMouse::MOUSE_LEFT) != 0;

#define HELD_TOGGLE_TIMER_DURATION_MS (400)  // JeffK says pausemenu uses a 400ms interval, so lets copy that
		if (fwTimer::GetSystemTimeInMilliseconds() > s_toggleHoldTimer + HELD_TOGGLE_TIMER_DURATION_MS)
		{
			s_toggleHoldTimer = fwTimer::GetSystemTimeInMilliseconds();

			u32 const c_optionIndex = ms_iCurrentItem[ms_iCurrentColumn];
			CVideoEditorMenuItem& menuItem = ms_MenuArray.CVideoEditorMenuItems[ GetCurrentMenuId() ];

			if( c_optionIndex < menuItem.Option.size() )
			{
				atHashString const c_toggleHash = menuItem.Option[ c_optionIndex ].ToggleValue;

				if (c_toggleHash != 0)
				{
					AdjustToggleValue(ms_mousePressedForToggle, c_toggleHash);

					g_FrontendAudioEntity.PlaySound( "NAV_LEFT_RIGHT", "HUD_FRONTEND_DEFAULT_SOUNDSET" );
				}
			}
		}

		if (!c_mouseHeldThisFrame)  // if no longer holding down, then reset the toggle
		{
			ms_mousePressedForToggle = 0;
		}

		return true;
	}

	return false;
}
#endif  // RSG_PC



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorMenu::UpdateInput
// PURPOSE:	checks for input and acts on it
/////////////////////////////////////////////////////////////////////////////////////////
bool CVideoEditorMenu::UpdateInput()
{
#if !USE_ORBIS_UPLOAD
	if (VideoUploadManager::GetInstance().IsUploadEnabled())
	{
		if (VideoUploadManager::GetInstance().IsInSetup())
		{				
			ms_isUpdatingUpload = true;
			return false;
		}
	}
#endif // !USE_ORBIS_UPLOAD

	float const fAxis = CControlMgr::GetMainFrontendControl().GetFrontendLeftRight().GetNorm();
	bool const c_wasKbmLastFrame =  CVideoEditorInstructionalButtons::LatestInstructionsAreKeyboardMouse();

#if RSG_PC
	if (ToggleOptionWithMouseViaArrows())
	{
		return true;
	}
#endif

	if( CPauseMenu::CheckInput(FRONTEND_INPUT_UP) )
	{
		ActionInputUp();

		return true;
	}

	if( CMousePointer::IsMouseWheelUp() )
	{
		if (ms_iColumnHoveredOver != GetCurrentColumn())
		{
			if (ms_iColumnHoveredOver > GetCurrentColumn())
			{
				JumpPane();
			}
			else
			{
				GoBackOnePane();
			}

			ms_iColumnHoveredOver = GetCurrentColumn();
		}

		/*if (IsOnMouseScrollableMenu())
		{
			MouseScrollList( -MOUSE_WHEEL_ITEM_SCROLL );
		}
		else*/
		{
			ActionInputUp();
		}

		return true;
	}

	if( CPauseMenu::CheckInput(FRONTEND_INPUT_DOWN) )
	{
		ActionInputDown();

		return true;
	}

	if( CMousePointer::IsMouseWheelDown() )
	{
		if (ms_iColumnHoveredOver != GetCurrentColumn())
		{
			if (ms_iColumnHoveredOver > GetCurrentColumn())
			{
				JumpPane();
			}
			else
			{
				GoBackOnePane();
			}

			ms_iColumnHoveredOver = GetCurrentColumn();
		}

		/*if (IsOnMouseScrollableMenu())
		{
			MouseScrollList( MOUSE_WHEEL_ITEM_SCROLL );
		}
		else*/
		{
			ActionInputDown();
		}

		return true;
	}

	if (CPauseMenu::CheckInput(FRONTEND_INPUT_LEFT) || CMousePointer::IsMouseWheelUp())
	{
		ActionInputLeft(fAxis != 0.0f);

		return true;
	}

	if (CPauseMenu::CheckInput(FRONTEND_INPUT_RIGHT) || CMousePointer::IsMouseWheelDown())
	{
		ActionInputRight(fAxis != 0.0f);

		return true;
	}

	if (CPauseMenu::CheckInput(FRONTEND_INPUT_ACCEPT) || CMousePointer::IsMouseAccept())
	{
		if( !CanItemBeActioned( GetCurrentMenuId(), ms_iCurrentColumn,  ms_iCurrentItem[ms_iCurrentColumn] ) )  // item cannot be selected (2177147)
		{
			g_FrontendAudioEntity.PlaySound( "ERROR", "HUD_FRONTEND_DEFAULT_SOUNDSET" );

			return false;
		}
		
		g_FrontendAudioEntity.PlaySound( "SELECT", "HUD_FRONTEND_DEFAULT_SOUNDSET" );

		const s32 c_currentMenuId = GetCurrentMenuId();
		
		const s32 iIndex = GetIndexToUseForAction(ms_iCurrentItem[ms_iCurrentColumn]);  // make sure we are using the correct action id
		const atHashString JumpMenuId = GenerateMenuId( c_currentMenuId, ms_MenuArray.CVideoEditorMenuItems[c_currentMenuId].Option[iIndex].JumpMenuId );

		CVideoEditorProject const* c_project = CVideoEditorUi::GetProject();
		if ( c_project )
		{
			if( JumpMenuId == ATSTRINGHASH("CLIP_FILT_LIST",0x83238537) )
			{
				s32 const c_clipCount = c_project->GetClipCount();
				s32 const c_maxClipCount = c_project->GetMaxClipCount();

				if( c_clipCount >= c_maxClipCount )
				{
					CVideoEditorUi::UpdateLiteralStringAsNumber( (u32)c_maxClipCount );
					SetErrorState( "MAX_CLIPS_IN_PROJ", CVideoEditorUi::GetLiteralStringBuffer() );
					return false;
				}
			}
			else if( JumpMenuId == ATSTRINGHASH("RADIO_STATION_LIST",0x3F4989B2) ||
				JumpMenuId == ATSTRINGHASH("SCORE_LIST",0xFE1489D8) ||
				JumpMenuId == ATSTRINGHASH("AMBIENT_LIST", 0xD8229566))
			{
				u32 const c_musicCount = c_project->GetMusicClipCount();
				u32 const c_maxMusicCount = c_project->GetMaxMusicClipCount();

				if( c_musicCount >= c_maxMusicCount )
				{
					CVideoEditorUi::UpdateLiteralStringAsNumber( c_maxMusicCount );
					SetErrorState( "MAX_MUSIC_IN_PROJ", CVideoEditorUi::GetLiteralStringBuffer() );
					return false;
				}
			}
            else if( JumpMenuId == ATSTRINGHASH( "EXPORT_SETTINGS", 0x65DD979D ) )
            {
                if( !CVideoEditorUi::CheckExportDuration() )
                {
                    return false;
                }
            }
			else if( JumpMenuId == ATSTRINGHASH("TEXT_SELECTION",0x33E8BF85) )
			{
				u32 const c_textCount = c_project->GetOverlayTextCount();
				u32 const c_maxTextCount = c_project->GetMaxOverlayTextCount();

				if( c_textCount >= c_maxTextCount )
				{
					CVideoEditorUi::UpdateLiteralStringAsNumber( (u32)c_maxTextCount );
					SetErrorState( "MAX_TEXT_IN_PROJ", CVideoEditorUi::GetLiteralStringBuffer() );
					return false;
				}
			}
		}

		bool bJumpAfterwards = TriggerAction(iIndex);  // trigger any action, but still continue to jump afterwards

		if (bJumpAfterwards)
		{
			if (FindMenuIndex(JumpMenuId) != -1)
			{
				JumpPane();
			}
		}
		
		CVideoEditorUi::GarbageCollect();

		return true;
	}

	bool const c_haveAnyVideoClipsInProject = (CVideoEditorTimeline::GetMaxClipCount(PLAYHEAD_TYPE_VIDEO) != 0);  // do we have any clips currently setup on the timeline?

	// mouse wheel button will go back to the timeline if on the options screen
	if (CMousePointer::IsMouseWheelPressed())
	{
		if (IsOnOptionsMenu())
		{
			if (c_haveAnyVideoClipsInProject)
			{
				CVideoEditorUi::ShowTimeline();
				return true;
			}
		}
	}

	if (CControlMgr::GetMainFrontendControl().GetReplayToggleTimeline().IsReleased() ||
		CPauseMenu::CheckInput(FRONTEND_INPUT_BACK) ||
		CMousePointer::IsMouseBack())
	{
		g_FrontendAudioEntity.PlaySound( "BACK", "HUD_FRONTEND_DEFAULT_SOUNDSET" );

		CVideoEditorUi::ActivateStageClipTextOverlay(false);  // kill any stage text we may of set up when we go back

		if (CTextTemplate::GetTextIndexEditing() != -1)
		{
			// return to timeline and cancel editing the text, as we went 'back' whilst editing text
			CTextTemplate::SetTextIndexEditing(-1);
			CVideoEditorUi::ShowTimeline();
		}
		else if( IsOnMainMenu() )
		{
			char const * const c_exitPromptMessage = !CVideoEditorUi::HasUserBeenWarnedOfGameDestory() ? 
				"VEUI_EXIT_WARN" : "VEUI_EXIT_WARN_D";

			SetUserConfirmationScreenWithAction( "WANT_TO_EXIT", c_exitPromptMessage, "VEUI_HDR_ALERT" );
		} 
		else if( IsOnOptionsMenu() )
		{
			if (c_haveAnyVideoClipsInProject)
			{
				CVideoEditorUi::ShowTimeline();
			}
		}
		else  // go back
		{
			s32 iBackMenuId = FindMenuIndex( ms_MenuArray.CVideoEditorMenuItems[ GetCurrentMenuId() ].BackMenuId );

			if (!ms_UserConfirmationScreen.IsActive())  // only go back a pane if we are not showing a warning screen
			{
				if (iBackMenuId == -1)
				{
					CVideoEditorUi::ShowTimeline();  // no valid menu to go back to, return to the timeline
				}
				else
				{
					GoBackOnePane();  // we have a valid menu to go back to so, go back to it
				}
			}

			CVideoEditorUi::GarbageCollect();
		}

		return true;
	}

	if ( IsOnAddClipMenu() || IsOnProjectMenu() || IsOnClipManagementMenu() || IsOnGalleryMenu() )
	{
		if( CPauseMenu::CheckInput(FRONTEND_INPUT_RB) && ms_PreviewData[ms_iCurrentColumn].size() > 1 )  // only sort if 2 or more items
		{
			if ( !CVideoEditorUi::IsStateWaitingForData()  )
			{
				ActionSortInverseRequest();
			}

			return true;
		}

		if( ( IsOnAddClipMenu() || IsOnClipManagementMenu() ) && 
			CPauseMenu::CheckInput(FRONTEND_INPUT_X) )
		{
			s32 const c_currentItem = ms_iCurrentItem[ ms_iCurrentColumn ];
			if( ToggleClipAsFavourite( ms_iCurrentColumn, c_currentItem - ms_iCurrentScrollOffset[ms_iCurrentColumn], c_currentItem ) )
			{
				ms_clipFavouritesNeedingSaved = true;
				CVideoEditorInstructionalButtons::Refresh();  
			}

			return true;
		}

		if( !IsOnAddClipMenu() && CPauseMenu::CheckInput(FRONTEND_INPUT_LB) )
		{
			s32 const c_currentItem = ms_iCurrentItem[ ms_iCurrentColumn ];
			if( ToggleClipAsDelete( ms_iCurrentColumn, c_currentItem - ms_iCurrentScrollOffset[ms_iCurrentColumn], c_currentItem ) )
			{
				RebuildMenu();
				CVideoEditorInstructionalButtons::Refresh();  
			}
			
			return true;
		}
	}

	if (VideoUploadManager::GetInstance().IsUploadEnabled() )
	{
		if( IsOnGalleryMenu() && CanActionFile( ms_iCurrentColumn, ms_iCurrentItem[ ms_iCurrentColumn ] ) )
		{
#if USE_ORBIS_UPLOAD
			if (CPauseMenu::CheckInput(FRONTEND_INPUT_X))
			{
				SetUserConfirmationScreenWithAction( "TRIGGER_UPLOAD_VIDEO", "VEUI_UPLOAD_SURE" );
			}
#else
			bool pressedX = CPauseMenu::CheckInput(FRONTEND_INPUT_X);
			bool pressedY = CPauseMenu::CheckInput(FRONTEND_INPUT_Y);

			if (pressedX || pressedY)
			{
				bool isOnUploadingVideo = false;
				CVideoFileView* videoView = CVideoEditorUi::GetVideoFileView();
				if( videoView )
				{
					if (VideoUploadManager::GetInstance().IsUploading())
					{
						char pathBuffer[RAGE_MAX_PATH];
						videoView->getFilePath( ms_iCurrentItem[ms_iCurrentColumn], pathBuffer );
						if (VideoUploadManager::GetInstance().IsPathTheCurrentUpload(pathBuffer))
							isOnUploadingVideo = true;
					}

					if( !videoView->IsValidVideoReference( ms_iCurrentItem[ms_iCurrentColumn] ) )
					{
						if (isOnUploadingVideo && !VideoUploadManager::GetInstance().IsProcessing())
						{
							VideoUploadManager::GetInstance().StopUpload(true, true, true);
							SetUserConfirmationScreen( "VEUI_UPLOAD_FAILED_ERROR", NULL, FE_WARNING_OK, false, TheText.Get("VEUI_ERR_FAILED_NOT_VALID") );
							return true;
						}
						else if (pressedX)
						{
							SetUserConfirmationScreen( "VEUI_ERR_FAILED_NOT_VALID" );
							return true;
						}
					}
				}


				if (isOnUploadingVideo)
				{
					if (pressedX)
					{
						if (VideoUploadManager::GetInstance().IsUploadingPaused())
						{
							SetUserConfirmationScreenWithAction( "TRIGGER_UPLOAD_RESUME", "VEUI_RESUME_UPLOAD_QUESTION" );
						}
						else
						{
							if (!VideoUploadManager::GetInstance().IsProcessing())
								SetUserConfirmationScreenWithAction( "TRIGGER_UPLOAD_PAUSE", "VEUI_PAUSE_UPLOAD_QUESTION" );
						}

						return true;
					}
					else if (pressedY)
					{
						if (!VideoUploadManager::GetInstance().IsProcessing())
							SetUserConfirmationScreenWithAction( "TRIGGER_UPLOAD_CANCEL", "VEUI_CANCEL_UPLOAD_QUESTION" );

						// even if not, return true anyway to stop later triggering of deletion process
						return true;
					}
				}
				else
				{
					if( pressedX )
					{
						// if doesn't have the correct privileges, inform the user 
						if (!VideoUploadManager::GetInstance().HasCorrectPrivileges())
						{
							// Display system UI.
							XBOX_ONLY(CLiveManager::ResolvePlatformPrivilege(NetworkInterface::GetLocalGamerIndex(), rlPrivileges::PrivilegeTypes::PRIVILEGE_SOCIAL_NETWORK_SHARING, true));

							SetUserConfirmationScreen("VEUI_UPLOAD_INCORRECT_PRIVILEGES");
						}
						else
						{
							u32 privacySetting = VIDEO_UPLOAD_PRIVACY_PRIVATE;
						
							CProfileSettings& settings = CProfileSettings::GetInstance();
							if(settings.AreSettingsValid())
							{
								if(settings.Exists(CProfileSettings::VIDEO_UPLOAD_PRIVACY))
								{
									privacySetting = settings.GetInt(CProfileSettings::VIDEO_UPLOAD_PRIVACY);
								}
							}

							if (VideoUploadManager::GetInstance().IsUploading())
							{
								switch (privacySetting)
								{
									case VIDEO_UPLOAD_PRIVACY_PUBLIC: 
										{
											SetUserConfirmationScreenWithAction( "TRIGGER_STOP_AND_UPLOAD_NEW_VIDEO", "VEUI_UPLOAD_START_NEW_PUBLIC" );
										}
										break;
									case VIDEO_UPLOAD_PRIVACY_UNLISTED: 
										{
											SetUserConfirmationScreenWithAction( "TRIGGER_STOP_AND_UPLOAD_NEW_VIDEO", "VEUI_UPLOAD_START_NEW_UNLISTED");
										}
										break;
									case VIDEO_UPLOAD_PRIVACY_PRIVATE: 
										{
											SetUserConfirmationScreenWithAction( "TRIGGER_STOP_AND_UPLOAD_NEW_VIDEO", "VEUI_UPLOAD_START_NEW_PRIVATE");
										}
										break;
									default: break;
								}
							}
							else
							{
								switch (privacySetting)
								{
								case VIDEO_UPLOAD_PRIVACY_PUBLIC: 
									{
										SetUserConfirmationScreenWithAction( "TRIGGER_UPLOAD_VIDEO", "VEUI_UPLOAD_SURE_PUBLIC" );
									}
									break;
								case VIDEO_UPLOAD_PRIVACY_UNLISTED: 
									{
										SetUserConfirmationScreenWithAction( "TRIGGER_UPLOAD_VIDEO", "VEUI_UPLOAD_SURE_UNLISTED");
									}
									break;
								case VIDEO_UPLOAD_PRIVACY_PRIVATE: 
									{
										SetUserConfirmationScreenWithAction( "TRIGGER_UPLOAD_VIDEO", "VEUI_UPLOAD_SURE_PRIVATE");
									}
									break;
								default: break;
								}
							}
						}


						return true;
					}
				}
			}
#endif // USE_ORBIS_UPLOAD
		}
	}

	if( ( ( c_wasKbmLastFrame && CControlMgr::GetMainFrontendControl().GetReplayClipDelete().IsReleased() ) ||
		( !c_wasKbmLastFrame && CPauseMenu::CheckInput(FRONTEND_INPUT_Y) ) ) && 
		ms_PreviewData[ms_iCurrentColumn].size() > 0 )
	{
		bool handled = false;

        s32 const c_currentItem = ms_iCurrentItem[ms_iCurrentColumn];
        FileViewIndex const c_viewIndex = GetFileViewIndexForItem( ms_iCurrentColumn, c_currentItem );
        CFileViewFilter const * const c_fileViewFilter = GetFileViewFilterForMenu( GetCurrentMenuId() );

        if( c_fileViewFilter && c_fileViewFilter->isValidIndex( c_viewIndex ) )
        {
            bool c_isMine = c_fileViewFilter ? c_fileViewFilter->getOwnerId( c_viewIndex ) == CVideoEditorInterface::GetActiveUserId() : false;

			if (ms_multiDeleteLastUsed)
			{
				c_isMine = ms_otherUserToDeleteCount == 0;
			}

            char const * c_action = NULL;
            char const * c_warningString = NULL;

			bool const c_isOnMarkedForDeleteFilter = IsOnMarkedForDeleteFilter();
			bool const c_multiDelete = IsMultiDelete();

            if( IsOnClipManagementMenu() )
            {
                CVideoEditorPreviewData& previewData = ms_PreviewData[ms_iCurrentColumn][c_currentItem];

				bool const c_isUsedInProject = c_isOnMarkedForDeleteFilter ? ms_inProjectToDeleteCount != 0 : previewData.m_usedInProject;

				if (c_isOnMarkedForDeleteFilter)
				{
					if (c_multiDelete)
					{
						c_warningString = c_isMine ? c_isUsedInProject ? "VEUI_DEL_MULTI_CLIPU" : "VEUI_DEL_MULTI_CLIP" : "VEUI_DEL_MULTI_CLIPOU";
						c_action = "TRIGGER_DELETE_CLIP_FROM_STORAGE";
					}
					else
					{
						c_warningString = "VEUI_DEL_MULTI_NO_CLIPS";
					}
				}
				else
				{
					c_warningString = c_isMine ? c_isUsedInProject ? "VEUI_DEL_CLIPU" : "VEUI_DEL_CLIP" : "VEUI_DEL_CLIPOU";
					c_action = "TRIGGER_DELETE_CLIP_FROM_STORAGE";
				}
            }
            else if( IsOnProjectMenu() )
            {
				if (c_isOnMarkedForDeleteFilter)
				{
					if (c_multiDelete)
					{
						c_warningString = c_isMine ? "VEUI_DEL_MULTI_PROJ" : "VEUI_DEL_MULTI_PROJOU";
						c_action = "TRIGGER_DELETE_PROJ_FROM_STORAGE";
					}
					else
					{
						c_warningString = "VEUI_DEL_MULTI_NO_PROJ";
					}
				}
				else
				{
					c_warningString = c_isMine ? "VEUI_DEL_PROJ" : "VEUI_DEL_PROJOU";
					c_action = "TRIGGER_DELETE_PROJ_FROM_STORAGE";
				}
            }
            else if( IsOnGalleryMenu() )
            {
				if (c_isOnMarkedForDeleteFilter)
				{
					if (c_multiDelete)
					{
						c_warningString = c_isMine ? "VEUI_DEL_MULTI_VID" : "VEUI_DEL_MULTI_VIDOU";
						c_action = "TRIGGER_DELETE_VID_FROM_STORAGE";
					}
					else
					{
						c_warningString = "VEUI_DEL_MULTI_NO_VID";
					}
				}
				else
				{
					CVideoFileView* videoView = (CVideoFileView*)c_fileViewFilter;
					if( videoView->IsValidVideoReference( ms_iCurrentItem[ms_iCurrentColumn] ) )
					{
						c_action = "TRIGGER_DELETE_VID_FROM_STORAGE";
						c_warningString = c_isMine ? "VEUI_DEL_VID" : "VEUI_DEL_VIDOU";
					}
					else
					{
						c_warningString = "VEUI_ERR_FAILED_NOT_VALID";
					}
				}
            }

            if( c_warningString && c_action )
            {
                SetUserConfirmationScreenWithAction( c_action, c_warningString );
                handled = true;
            }
            else if( c_warningString )
            {
                SetUserConfirmationScreen( c_warningString );
                handled = true;
            }
        }


		return handled;
	}

	return false;
}






#define NUM_INDEXES_ACROSS_COL_TYPE_IMG_TWELVE (4)

void CVideoEditorMenu::ActionInputUp()
{
	s32 const c_currentMenuId = GetCurrentMenuId();
	if (GetNumberOfItemsInColumn( c_currentMenuId, ms_iCurrentColumn ) > 1)
	{
		g_FrontendAudioEntity.PlaySound( "NAV_UP_DOWN", "HUD_FRONTEND_DEFAULT_SOUNDSET" );

		if (ms_MenuArray.CVideoEditorMenuItems[c_currentMenuId].columnType == COL_TYPE_IMG_TWELVE)
		{
			s32 iNewIndex = ms_iCurrentItem[ms_iCurrentColumn];

			for (s32 i = 0; i < NUM_INDEXES_ACROSS_COL_TYPE_IMG_TWELVE; i++)
			{
				// wrap around:
				iNewIndex--;

				if (iNewIndex < 0)
				{
					iNewIndex = ms_MenuArray.CVideoEditorMenuItems[c_currentMenuId].Option.GetCount()-1;
					iNewIndex = Max( iNewIndex, 0 );
				}
			}

			SetItemActive(iNewIndex);
		}
		else
		{
			GoToPreviousItem();

			if( IsFileFilteringMenu( c_currentMenuId ) )
			{
				RebuildMenu();
			}

			CVideoEditorInstructionalButtons::Refresh();  // refresh the buttons
		}
	}
}



void CVideoEditorMenu::ActionInputDown()
{
	s32 const c_currentMenuId = GetCurrentMenuId();
	if (GetNumberOfItemsInColumn( c_currentMenuId, ms_iCurrentColumn ) > 1)
	{
		g_FrontendAudioEntity.PlaySound( "NAV_UP_DOWN", "HUD_FRONTEND_DEFAULT_SOUNDSET" );

		if (ms_MenuArray.CVideoEditorMenuItems[c_currentMenuId].columnType == COL_TYPE_IMG_TWELVE)
		{
			s32 iNewIndex = ms_iCurrentItem[ms_iCurrentColumn];

			for (s32 i = 0; i < NUM_INDEXES_ACROSS_COL_TYPE_IMG_TWELVE; i++)
			{
				// wrap around:
				iNewIndex++;

				if (iNewIndex >= ms_MenuArray.CVideoEditorMenuItems[c_currentMenuId].Option.GetCount())
				{
					iNewIndex = 0;
				}
			}

			SetItemActive(iNewIndex);
		}
		else
		{
			GoToNextItem();

			if( IsFileFilteringMenu( c_currentMenuId ) )
			{
				RebuildMenu();
			}

			CVideoEditorInstructionalButtons::Refresh();  // refresh the buttons
		}
	}
}

void CVideoEditorMenu::ActionInputLeft(const bool bStick)
{
	s32 const c_currentMenuId = GetCurrentMenuId();
	if (ms_MenuArray.CVideoEditorMenuItems[c_currentMenuId].columnType == COL_TYPE_IMG_TWELVE)
	{
		// wrap:
		s32 iNewIndex = ms_iCurrentItem[ms_iCurrentColumn];

		if (iNewIndex == 0 || iNewIndex == 4 || iNewIndex == 8)
		{
			iNewIndex+=3;
			SetItemActive(iNewIndex);
		}
		else
		{
			GoToPreviousItem();
		}

		g_FrontendAudioEntity.PlaySound( "NAV_LEFT_RIGHT", "HUD_FRONTEND_DEFAULT_SOUNDSET" );
	}
	else
	{
		u32 const c_optionIndex = ms_iCurrentItem[ms_iCurrentColumn];
		CVideoEditorMenuItem& menuItem = ms_MenuArray.CVideoEditorMenuItems[c_currentMenuId];

		if( c_optionIndex < menuItem.Option.size() )
		{
			atHashString const c_toggleHash = menuItem.Option[ c_optionIndex ].ToggleValue;

			if (c_toggleHash != 0)
			{
				s32 iAmount = -1;

				if (bStick)
				{
					iAmount = -5;  // bigger increase if stick is used (5 seconds)
				}

				AdjustToggleValue(iAmount, c_toggleHash);

				g_FrontendAudioEntity.PlaySound( "NAV_LEFT_RIGHT", "HUD_FRONTEND_DEFAULT_SOUNDSET" );
			}
		}
	}
}



void CVideoEditorMenu::ActionInputRight(const bool bStick)
{
	s32 const c_currentMenuId = GetCurrentMenuId();
	if (ms_MenuArray.CVideoEditorMenuItems[c_currentMenuId].columnType == COL_TYPE_IMG_TWELVE)
	{
		// wrap:
		s32 iNewIndex = ms_iCurrentItem[ms_iCurrentColumn];

		if (iNewIndex == 3 || iNewIndex == 7 || iNewIndex == 11)
		{
			iNewIndex-=3;
			SetItemActive(iNewIndex);
		}
		else
		{
			GoToNextItem();
		}

		g_FrontendAudioEntity.PlaySound( "NAV_LEFT_RIGHT", "HUD_FRONTEND_DEFAULT_SOUNDSET" );
	}
	else
	{
		u32 const c_optionIndex = ms_iCurrentItem[ms_iCurrentColumn];
		CVideoEditorMenuItem& menuItem = ms_MenuArray.CVideoEditorMenuItems[c_currentMenuId];

		if( c_optionIndex < menuItem.Option.size() )
		{
			atHashString const c_toggleHash = menuItem.Option[ c_optionIndex ].ToggleValue;

			if (c_toggleHash != 0)
			{
				s32 iAmount = 1;
				
				if (bStick)
				{
					iAmount = 5;  // bigger increase if stick is used (5 seconds)
				}

				AdjustToggleValue(iAmount, c_toggleHash);

				g_FrontendAudioEntity.PlaySound( "NAV_LEFT_RIGHT", "HUD_FRONTEND_DEFAULT_SOUNDSET" );
			}
		}
		
	}
}


bool CVideoEditorMenu::IsOnMainMenu()
{
	return (GetCurrentMenuId() == FindMenuIndex("MAIN_MENU"));
}

bool CVideoEditorMenu::IsOnOptionsMenu()
{
	return (GetCurrentMenuId() == FindMenuIndex("ADD_LIST"));
}

bool CVideoEditorMenu::IsClipFilteringMenu( s32 const menuId )
{
	return ( menuId == FindMenuIndex("CLIP_MGR_FILT_LIST") ) || ( menuId == FindMenuIndex("CLIP_FILT_LIST") );
}

bool CVideoEditorMenu::IsAddClipMenu( s32 const menuId )
{
	return ( menuId == FindMenuIndex("CLIP_LIST"));
}

bool CVideoEditorMenu::IsAddClipMenuOrAddClipFilterList( s32 const menuId )
{
	return ( menuId == FindMenuIndex("CLIP_LIST") ) || ( menuId == FindMenuIndex("CLIP_FILT_LIST") );
}

bool CVideoEditorMenu::IsOnClipFilteringMenu()
{
	return IsClipFilteringMenu( GetCurrentMenuId() );
}

bool CVideoEditorMenu::IsOnAddClipMenu()
{
	return IsAddClipMenu( GetCurrentMenuId() );
}

bool CVideoEditorMenu::IsOnAddClipMenuOrAddClipFilterList()
{
	return IsAddClipMenuOrAddClipFilterList( GetCurrentMenuId() );
}

bool CVideoEditorMenu::IsClipManagementMenu( s32 const menuId )
{
	return (menuId == FindMenuIndex("CLIP_MGR_LIST"));
}

bool CVideoEditorMenu::IsOnClipManagementMenu()
{
	return IsClipManagementMenu( GetCurrentMenuId() );
}

bool CVideoEditorMenu::IsClipMenuClipList(const s32 iMenuId)
{
	const s32 iClipMgrListMenu = FindMenuIndex("CLIP_MGR_LIST");
	const s32 iClipListMenu = FindMenuIndex("CLIP_LIST");

	return (iMenuId == iClipMgrListMenu || iMenuId == iClipListMenu);
}

bool CVideoEditorMenu::IsProjectFilteringMenu( s32 const menuId )
{
	return ( menuId == FindMenuIndex("LOAD_FILT_LIST"));
}

bool CVideoEditorMenu::IsProjectMenu( s32 const menuId )
{
	return (menuId == FindMenuIndex("LOAD_LIST"));
}

bool CVideoEditorMenu::IsOnProjectFilteringMenu()
{
	return IsProjectFilteringMenu( GetCurrentMenuId() );
}

bool CVideoEditorMenu::IsOnProjectMenu()
{
	return IsProjectMenu( GetCurrentMenuId() );
}

bool CVideoEditorMenu::IsAudioTrackList( s32 const menuId )
{
	return ( menuId == FindMenuIndex("RADIO_TRACK_LIST") || menuId == FindMenuIndex("SCORE_TRACK_LIST") || menuId == FindMenuIndex("AMBIENT_TRACK_LIST") );
}

bool CVideoEditorMenu::IsOnAudioTrackList()
{
	return IsAudioTrackList( GetCurrentMenuId() );
}

bool CVideoEditorMenu::IsAudioCategoryList( s32 const menuId )
{
	return ( menuId == FindMenuIndex("RADIO_STATION_LIST") || menuId == FindMenuIndex("SCORE_LIST") || menuId == FindMenuIndex("AMBIENT_LIST") );
}

bool CVideoEditorMenu::IsOnAudioCategoryList()
{
	return IsAudioCategoryList( GetCurrentMenuId() );
}

bool CVideoEditorMenu::IsGalleryMenu( s32 const menuId )
{
	return (menuId == FindMenuIndex("GALLERY_LIST"));
}

bool CVideoEditorMenu::IsGalleryFilteringMenu( s32 const menuId )
{
	return (menuId == FindMenuIndex("GALLERY_FILT_LIST"));
}

bool CVideoEditorMenu::IsOnGalleryFilteringMenu()
{
	return IsGalleryFilteringMenu( GetCurrentMenuId() );
}

bool CVideoEditorMenu::IsOnGalleryMenu()
{
	return IsGalleryMenu( GetCurrentMenuId() );
}

bool CVideoEditorMenu::IsOnAudioMenu()
{
	s32 const c_currentMenuId = GetCurrentMenuId();
	return ( c_currentMenuId == FindMenuIndex("RADIO_STATION_LIST") ) || ( c_currentMenuId == FindMenuIndex("RADIO_TRACK_LIST") ) || 
		( c_currentMenuId == FindMenuIndex("SCORE_LIST") ) || ( c_currentMenuId == FindMenuIndex("SCORE_TRACK_LIST") ) ||
		( c_currentMenuId == FindMenuIndex("AMBIENT_LIST") ) || ( c_currentMenuId == FindMenuIndex("AMBIENT_TRACK_LIST") );
}

bool CVideoEditorMenu::IsOnTrackListMenu()
{
	s32 const c_currentMenuId = GetCurrentMenuId();
	return ( c_currentMenuId == FindMenuIndex("RADIO_TRACK_LIST") || c_currentMenuId == FindMenuIndex("SCORE_TRACK_LIST") );
}

bool CVideoEditorMenu::IsMenuSelected()
{
	return !CVideoEditorTimeline::IsTimelineSelected();
}

bool CVideoEditorMenu::CanActionFile( s32 const iColumn, s32 const iIndex )
{
    CVideoEditorPreviewData const * const c_previewData = GetPreviewData( iColumn, iIndex );
    bool const c_canAction = ( c_previewData && c_previewData->BelongsToCurrentUser() ) || PARAM_rse_allowOtherUserFiles.Get();
    return c_canAction;
}

bool CVideoEditorMenu::CanActionCurrentFile()
{
    bool const c_canAction = CanActionFile( ms_iCurrentColumn, ms_iCurrentItem[ ms_iCurrentColumn ] );
    return c_canAction;
}

s32 CVideoEditorMenu::GenerateMenuId( s32 const menuId, s32 const startingId )
{
	s32 resultId = startingId;

	//! The XML defines the "fallback" for if no files meet the filtering, so here we substitute the appropriate file 
	//! view if we have a valid filter with files in it.
	CFileViewFilter const* fileFilter = GetFileViewFilterForMenu( menuId );
	if( fileFilter && fileFilter->getFilteredFileCount() > 0 )
	{
		if( IsClipFilteringMenu( menuId ) )
		{
			resultId = ATSTRINGHASH( "CLIP_MGR_LIST", 0x921FF152 );
		}
		else if( IsProjectFilteringMenu( menuId )  )
		{
			resultId = ATSTRINGHASH( "LOAD_LIST", 0xBB54B5B );
		}
		else if( IsGalleryFilteringMenu( menuId ) )
		{
			resultId = ATSTRINGHASH( "GALLERY_LIST", 0x1964B317 );
		}
	}

	return resultId;
}

void CVideoEditorMenu::GenerateListFooterText( SimpleString_32& out_buffer, s32 const c_columnId )
{
	out_buffer.Clear();

	s32 const c_currentIndex = ms_iCurrentItem[c_columnId];
	s32 const c_totalListCount = ms_PreviewData[ c_columnId ].GetCount();

	out_buffer.sprintf( "%d / %d", c_currentIndex + 1, c_totalListCount );
}

void CVideoEditorMenu::UpdateFooterText()
{
	if( CScaleformMgr::BeginMethod(CVideoEditorUi::GetMovieId(), SF_BASE_CLASS_VIDEO_EDITOR, "UPDATE_SCROLL_LIST_LABEL") )
	{
		SimpleString_32 footerText;
		GenerateListFooterText( footerText, ms_iCurrentColumn );

		CScaleformMgr::AddParamString( footerText.getBuffer(), false );
		CScaleformMgr::EndMethod();
	}
}

CFileViewFilter* CVideoEditorMenu::GetFileViewFilterForMenu( s32 const menuId  )
{
	CFileViewFilter* fileFilter = NULL;

	if( IsGalleryMenu( menuId ) || IsGalleryFilteringMenu( menuId ) )
	{
		fileFilter = (CFileViewFilter*)CVideoEditorUi::GetVideoFileView();
	}
	else if( IsProjectMenu( menuId ) || IsProjectFilteringMenu( menuId ) )
	{
		fileFilter = (CFileViewFilter*)CVideoEditorUi::GetProjectFileView();
	}
	else if( IsAddClipMenu( menuId ) || IsClipManagementMenu( menuId ) || IsClipFilteringMenu( menuId ) )
	{
		fileFilter = (CFileViewFilter*)CVideoEditorUi::GetClipFileView();
	}

	return fileFilter;
}

CThumbnailFileView* CVideoEditorMenu::GetThumbnailViewForMenu( s32 const menuId )
{
	CThumbnailFileView* thumbnailView = (CThumbnailFileView*)GetFileViewFilterForMenu( menuId );
	return thumbnailView;
}

CThumbnailFileView* CVideoEditorMenu::GetCurrentThumbnailView()
{
	return GetThumbnailViewForMenu( GetCurrentMenuId() );
}

CThumbnailFileView* CVideoEditorMenu::GetThumbnailViewForPreviewData( s32 const previewId )
{
	CThumbnailFileView *pThumbnailView = NULL;

#if defined(VIDEO_PLAYBACK_ENABLED) && VIDEO_PLAYBACK_ENABLED
	if( previewId == sc_galleryPreviewId )
	{
		pThumbnailView = (CThumbnailFileView*)CVideoEditorUi::GetVideoFileView();
	}
	else
#endif // defined(VIDEO_PLAYBACK_ENABLED) && VIDEO_PLAYBACK_ENABLED
		if( previewId == sc_loadProjPreviewId )
		{
			pThumbnailView = CVideoEditorUi::GetThumbnailViewFromProjectFileView();
		}
		else if ( previewId == sc_clipPreviewId )
		{
			pThumbnailView = CVideoEditorUi::GetThumbnailViewFromClipFileView();
		}

		if( pThumbnailView && pThumbnailView->isPopulated() )
		{
			return pThumbnailView;
		}

		return NULL;
}

void CVideoEditorMenu::ActionSortInverseRequest()
{
	s32 const c_currentItem = ms_iCurrentItem[ ms_iCurrentColumn ];
	s32 const c_itemCount = ms_PreviewData[ ms_iCurrentColumn ].GetCount();

	if( c_currentItem >= 0 && c_currentItem < c_itemCount )
	{
        ReleaseThumbnailOfItem();

		ToggleCurrentMenuSortInverse();
        InvertPreviewDataInternal( ms_iCurrentColumn );

		const s32 newPosition = c_itemCount - c_currentItem - 1;
		GoToItem( newPosition, GOTO_REBUILD_AUTO );
    
        RebuildMenu();

		CVideoEditorInstructionalButtons::Refresh();  
	}	
}

void CVideoEditorMenu::SetCurrentMenuSortInverse( bool const inverse )
{
	CFileViewFilter* fileFilter = GetFileViewFilterForMenu( GetCurrentMenuId() );
	if( fileFilter && inverse != fileFilter->IsSortInverted() )
	{
		fileFilter->invertSort();
	}
}

void CVideoEditorMenu::ToggleCurrentMenuSortInverse()
{
	SetCurrentMenuSortInverse( !IsCurrentMenuSortInverse() );
}

bool CVideoEditorMenu::IsCurrentMenuSortInverse()
{
	CFileViewFilter const* fileFilter = GetFileViewFilterForMenu( GetCurrentMenuId() );
	bool const c_isInverse = fileFilter ? fileFilter->IsSortInverted() : false;
	return c_isInverse;
}

bool CVideoEditorMenu::ToggleClipAsFavourite( s32 const columnIndex, s32 const listIndex, s32 const itemIndex )
{
	bool toggled = false;

	if( ( IsOnClipManagementMenu() || IsOnAddClipMenu() ) &&
        CanActionFile( columnIndex, itemIndex ) )
	{
		s32 const c_fileIndex = GetFileViewIndexForItem( columnIndex, itemIndex );
		CRawClipFileView* pClipFileView = CVideoEditorUi::GetClipFileView();
		s32 const c_clipCount = pClipFileView ? (s32)pClipFileView->getFilteredFileCount() : 0;
		if( pClipFileView && c_fileIndex >= 0  && c_fileIndex < c_clipCount )
		{
			pClipFileView->toggleClipFavouriteState( c_fileIndex );
			toggled = true;

			atArray<CVideoEditorPreviewData>& previewDataArray = ms_PreviewData[ms_iCurrentColumn];
			char const * audioTrigger = NULL;

			if( ms_previewDataFilter[ ms_iCurrentColumn ] == CRawClipFileView::FILTERTYPE_FAVOURITES )
			{
				ReleaseThumbnailOfItem();

				s32 const c_oldArrayCount = previewDataArray.GetCount();
				previewDataArray.erase( previewDataArray.begin() + itemIndex );
				s32 currentItem = ms_iCurrentItem[ ms_iCurrentColumn ];
				currentItem -= ( currentItem == c_oldArrayCount -1 ) ? 1 : 0;
				audioTrigger = "ERROR";
				
				if( c_oldArrayCount == 1 )
				{
					GoBackOnePane( -1 );
				}
				else
				{
					GoToItem( currentItem, ms_iCurrentColumn, GOTO_REBUILD_NONE );
				}

				RebuildMenu();
			}
			else
			{
				CVideoEditorPreviewData& previewData = previewDataArray[ itemIndex ];
				previewData.m_favourite = !previewData.m_favourite;
				UpdateItemColouration( ms_iCurrentColumn, listIndex, itemIndex );

				audioTrigger = previewData.m_favourite ? "TOGGLE_ON" : "ERROR";
			}

			if( audioTrigger )
			{
				g_FrontendAudioEntity.PlaySound( audioTrigger, "HUD_FRONTEND_DEFAULT_SOUNDSET" );
			}
		}
	}

	return toggled;
}

bool CVideoEditorMenu::ToggleClipAsDelete( s32 const columnIndex, s32 const listIndex, s32 const itemIndex )
{
	bool toggled = false;

	s32 const c_fileIndex = GetFileViewIndexForItem( columnIndex, itemIndex );
	CFileViewFilter* pFileView = GetFileViewFilterForMenu( GetCurrentMenuId() );
	s32 const c_fileCount = pFileView ? (s32)pFileView->getFilteredFileCount() : 0;

	if( pFileView && c_fileIndex >= 0  && c_fileIndex < c_fileCount )
	{
		pFileView->toggleMarkAsDeleteState( c_fileIndex );
		toggled = true;

		atArray<CVideoEditorPreviewData>& previewDataArray = ms_PreviewData[ms_iCurrentColumn];
		
		CVideoEditorPreviewData& previewData = previewDataArray[ itemIndex ];
		previewData.m_flaggedForDelete = !previewData.m_flaggedForDelete;
		
		if (previewData.m_usedInProject)
		{
			previewData.m_flaggedForDelete ? ++ms_inProjectToDeleteCount : --ms_inProjectToDeleteCount;
		}

		if (previewData.BelongsToCurrentUser())
		{
			previewData.m_flaggedForDelete ? ++ms_otherUserToDeleteCount : --ms_otherUserToDeleteCount;
		}

		char const * audioTrigger = NULL;
		if( ms_previewDataFilter[ ms_iCurrentColumn ] == CFileViewFilter::FILTERTYPE_MARKED_DELETE_ANY_USER )
		{
			ReleaseThumbnailOfItem();

			s32 const c_oldArrayCount = previewDataArray.GetCount();
			previewDataArray.erase( previewDataArray.begin() + itemIndex );
			s32 currentItem = ms_iCurrentItem[ ms_iCurrentColumn ];
			currentItem -= ( currentItem == c_oldArrayCount -1 ) ? 1 : 0;
			audioTrigger = "ERROR";

			if( c_oldArrayCount == 1 )
			{
				GoBackOnePane( -1 );
			}
			else
			{
				GoToItem( currentItem, ms_iCurrentColumn, GOTO_REBUILD_NONE );
			}

			RebuildMenu();
		}
		else
		{
			UpdateItemColouration( ms_iCurrentColumn, listIndex, itemIndex );

			audioTrigger = previewData.m_flaggedForDelete ? "TOGGLE_ON" : "ERROR";
		}

		if( audioTrigger )
		{
			g_FrontendAudioEntity.PlaySound( audioTrigger, "HUD_FRONTEND_DEFAULT_SOUNDSET" );
		}
	}

	return toggled;
}

bool CVideoEditorMenu::IsOnMarkedForDeleteFilter()
{
	CFileViewFilter const * const fileViewFilter = GetFileViewFilterForMenu( GetCurrentMenuId() );
	bool const c_markedForDeleteFilter = fileViewFilter && fileViewFilter->GetCurrentFilterType() == CFileViewFilter::FILTERTYPE_MARKED_DELETE_ANY_USER;
	return c_markedForDeleteFilter;
}

bool CVideoEditorMenu::IsMultiDelete()
{
	CFileViewFilter const * const fileViewFilter = GetFileViewFilterForMenu( GetCurrentMenuId() );
	bool const c_multiDelete = fileViewFilter && fileViewFilter->GetCurrentFilterType() == CFileViewFilter::FILTERTYPE_MARKED_DELETE_ANY_USER && fileViewFilter->getCountOfFilesToDelete() > 0;
	return c_multiDelete;
}

void CVideoEditorMenu::SaveClipFavourites()
{
	CRawClipFileView* pClipFileView = CVideoEditorUi::GetClipFileView();
	if( pClipFileView )
	{
		pClipFileView->saveClipFavourites();
	}
}

/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorMenu::JumpPane
// PURPOSE:	Moves To another pane
/////////////////////////////////////////////////////////////////////////////////////////
void CVideoEditorMenu::JumpPane()
{
	s32 const c_currentMenuId = GetCurrentMenuId();
	CVideoEditorMenuItem& currentMenu = ms_MenuArray.CVideoEditorMenuItems[c_currentMenuId];
	// store the index we are on before we jump (so we can highlight it later when we go back)
	currentMenu.iLastKnownSelectedItem = ms_iCurrentItem[ms_iCurrentColumn];

	s32 const c_index = GetIndexToUseForAction(ms_iCurrentItem[ms_iCurrentColumn]);  // make sure we are using the correct action id
	atHashString const c_jumpMenuId = currentMenu.Option[c_index].JumpMenuId;
	s32 const c_jumpMenuIndex = FindMenuIndex( c_jumpMenuId );

	if (c_jumpMenuIndex != -1)
	{
		uiDisplayf("Jumping to new Menu %d", c_jumpMenuIndex);

		// move column in the direction of the new column ID we are jumping to
		if (ms_iCurrentColumn < ms_MenuArray.CVideoEditorMenuItems[c_jumpMenuIndex].columnId)
		{
			GoToNextColumn();
		}
		else if (ms_iCurrentColumn > ms_MenuArray.CVideoEditorMenuItems[c_jumpMenuIndex].columnId)
		{
			GoToPreviousColumn();
		}
		else
		{
			GoToItem( 0, GOTO_REBUILD_AUTO );
		}

		s32 iNumberOfColumnsBuiltThisTime = BuildMenu(c_jumpMenuIndex, true) + 1;

		// clear any remaining columns
		while (iNumberOfColumnsBuiltThisTime < MAX_COL_NUMs)
		{
			RemoveColumn(iNumberOfColumnsBuiltThisTime++);
		}

		GoToItem( ms_iCurrentItem[ms_iCurrentColumn], ms_iCurrentColumn, GOTO_REBUILD_PREVIEW_PANE );

		if( !CMousePointer::HasMouseInputOccurred() )
		{
			s32 uploadIndex = -1;
#if !USE_ORBIS_UPLOAD
			if (IsOnGalleryMenu() && VideoUploadManager::GetInstance().IsUploadEnabled() && VideoUploadManager::GetInstance().IsUploading())
			{
				CVideoFileView* videoView = CVideoEditorUi::GetVideoFileView();
				if( videoView )
				{
					const char* pathAndFilename = VideoUploadManager::GetInstance().GetSourcePathAndFileName();
					if (pathAndFilename)
					{
						uploadIndex = videoView->getIndexForFileName(ASSET.FileName(pathAndFilename), false, CVideoEditorInterface::GetActiveUserId());
					}
				}
			}
#endif

			if (uploadIndex != -1)
			{
				GoToItem( uploadIndex, GOTO_REBUILD_PREVIEW_PANE );
			}
			else if (ms_iCurrentColumn == COL_1)
			{
				ms_iCurrentScrollOffset[ms_iCurrentColumn] = 0;  // reset the scroll
				GoToItem(0, ms_iCurrentColumn, GOTO_REBUILD_AUTO);  // when jumping but still on column 1, always set the top item active (otherwise retain the current index of the column)
			}
		}

		CVideoEditorInstructionalButtons::Refresh();  // refresh the buttons
	}
}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorMenu::GoBackOnePane
// PURPOSE:	Goes back one pane in the tree
/////////////////////////////////////////////////////////////////////////////////////////
void CVideoEditorMenu::GoBackOnePane( s32 const itemIndex )
{
	const s32 c_currentMenuId = GetCurrentMenuId();
	const s32 iBackMenuId = c_currentMenuId >= 0 ? FindMenuIndex( ms_MenuArray.CVideoEditorMenuItems[ c_currentMenuId ].BackMenuId ) : -1;

	if( ms_clipFavouritesNeedingSaved &&
		( IsAddClipMenu( c_currentMenuId ) || IsClipMenuClipList( c_currentMenuId ) ) )
	{
		CRawClipFileView* clipView = CVideoEditorUi::GetClipFileView();
		if( clipView )
		{
			clipView->saveClipFavourites();
		}

		ms_clipFavouritesNeedingSaved = false;
	}

    bool returnToActiveState = true;
	
	if( iBackMenuId != -1 )
	{
		CVideoEditorMenuItem const& menuItem = ms_MenuArray.CVideoEditorMenuItems[iBackMenuId];
		s32 const c_targetItem = itemIndex >= 0 ? itemIndex : menuItem.iLastKnownSelectedItem;

		if( IsOnAudioMenu() && g_RadioAudioEntity.IsReplayMusicPreviewPlaying())  // turn off any preview music if its playing
		{
			g_RadioAudioEntity.StopReplayTrackPreview();
		}

		uiDisplayf("Going back to new Menu %d", iBackMenuId);

		// Don't release when going back to a file filtering menu as the thumbnail should be the same, 
		// unless we used the mouse to jump to a different filter
        bool const c_mouseFocusChangedOnNewColumn = ( CMousePointer::HasMouseInputOccurred() && c_targetItem != menuItem.iLastKnownSelectedItem );
		bool const c_releaseThumbnailAndRebuildMenu = !IsFileFilteringMenu( iBackMenuId ) || c_mouseFocusChangedOnNewColumn;
        returnToActiveState = !IsFileFilteringMenu( iBackMenuId ) || !c_mouseFocusChangedOnNewColumn;

		s32 const c_columnLeft = ms_iCurrentColumn;

		UnHighlight();
		GoToPreviousColumn();

		if( c_releaseThumbnailAndRebuildMenu )
		{
			// Set the target item explicitly to ensure the correct menu is rebuilt
			// Don't use GoToItem here as we want to control the rebuild behaviour explicitly and will call GoToItem at the end...
            s32 const c_oldItem = ms_iCurrentItem[ ms_iCurrentColumn ];
			ms_iCurrentItem[ ms_iCurrentColumn ] = c_targetItem;

			ReleaseThumbnailOfItem();

            //! Clear focus on other columns back to default if we've switched focus on our new column
            if( c_oldItem != c_targetItem )
            {   
                for( int columnIndex = ms_iCurrentColumn + 1; columnIndex < MAX_COL_NUMs; ++columnIndex )
                {
                    ms_iCurrentItem[ columnIndex ] = 0;
                }
            }

			s32 iNumberOfColumnsBuiltThisTime = BuildMenu( iBackMenuId, true ) + 1;

			// clear any remaining columns
			while (iNumberOfColumnsBuiltThisTime < MAX_COL_NUMs)
			{
				RemoveColumn(iNumberOfColumnsBuiltThisTime);
				iNumberOfColumnsBuiltThisTime++;
			}

			GoToItem( c_targetItem, ms_iCurrentColumn, GOTO_REBUILD_AUTO );
		}
		else
		{
			BuildMenu( c_currentMenuId, false, c_columnLeft );
			if( c_targetItem != ms_iCurrentItem[ ms_iCurrentColumn ] )
			{
				GoToItem( c_targetItem, ms_iCurrentColumn, GOTO_REBUILD_NONE );
			}
		}

		CVideoEditorInstructionalButtons::Refresh();  // refresh the buttons
	}

	if( returnToActiveState && CVideoEditorUi::IsStateWaitingForData() )
	{
		CVideoEditorUi::SetActiveState();
	}
}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorMenu::JumpToMenu
// PURPOSE:	goes direct to the passed menu
/////////////////////////////////////////////////////////////////////////////////////////
bool CVideoEditorMenu::JumpToMenu(const atHashString menuHash)
{
	const s32 c_currentMenuId = GetCurrentMenuId();
	const s32 iNewMenuId = FindMenuIndex(menuHash);

	if (iNewMenuId != -1)
	{
		if (iNewMenuId == 0 || iNewMenuId != c_currentMenuId )  // set the value before we build the menu
		{
			ms_iMenuIdForColumn[0] = iNewMenuId;
			SetColumnActive(0);
			GoToItem( 0, ms_iCurrentColumn, GOTO_REBUILD_AUTO );
		}

		ms_iMenuIdForColumn[ms_iCurrentColumn] = iNewMenuId;

		s32 iNumberOfColumnsBuiltThisTime = BuildMenu(iNewMenuId, true) + 1;
		SetItemSelected(ms_iCurrentItem[ms_iCurrentColumn] - ms_iCurrentScrollOffset[ms_iCurrentColumn]);

		// clear any remaining columns
		while (iNumberOfColumnsBuiltThisTime < MAX_COL_NUMs)
		{
			RemoveColumn(iNumberOfColumnsBuiltThisTime++);
		}

		return true;
	}

	return false;
}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorMenu::ExitToMainMenu
// PURPOSE:	goes direct to the main menu and leaves editing mode
/////////////////////////////////////////////////////////////////////////////////////////
void CVideoEditorMenu::ExitToMainMenu()
{
	if (JumpToMenu(ATSTRINGHASH("MAIN_MENU",0xed3744e0)))
	{
		uiDisplayf("Going back to main menu");
	}

	if (CVideoEditorUi::IsStateWaitingForData())
	{
		CVideoEditorUi::SetActiveState();
	}

	ClearPreviewData( -1, true, false );

	CVideoEditorUi::LeaveEditingMode();
}


/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorMenu::GetIndexToUseForAction
// PURPOSE: decide based on type, what element we need to use for any actions
/////////////////////////////////////////////////////////////////////////////////////////
s32 CVideoEditorMenu::GetIndexToUseForAction(const s32 iIndex)
{
	s32 iNewIndex = iIndex;
	if (iIndex < 0 || iIndex >= ms_MenuArray.CVideoEditorMenuItems[ GetCurrentMenuId() ].Option.GetCount())
	{
		iNewIndex = 0;  // always use element zero
	}

	return iNewIndex;
}

/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorMenu::TriggerAction
// PURPOSE: When a menu option is selected this gets called.
//			Returns true if we need to continue to jump after triggering the relevant
//			action
/////////////////////////////////////////////////////////////////////////////////////////
bool CVideoEditorMenu::TriggerAction(const s32 iIndex)
{
	atHashString TriggerAction;
	atHashString TriggerWarningText;
	atHashString TriggerHeaderText;
	char const * literalText = NULL;

	CVideoEditorMenuItem const& menuItemRef = ms_MenuArray.CVideoEditorMenuItems[GetCurrentMenuId()];
	if( iIndex >= 0 && iIndex < menuItemRef.Option.GetCount() )
	{
		CVideoEditorMenuOption const& menuOptionRef = menuItemRef.Option[iIndex];

		TriggerAction = menuOptionRef.TriggerAction;
		TriggerWarningText = menuOptionRef.WarningText;
		TriggerHeaderText = atStringHash( "VEUI_HDR_ALERT" );

		u64 warningType = FE_WARNING_YES_NO;

		// no trigger action then just return out (maybe play an invalid sound here)
		if (TriggerAction.IsNull())
		{
			return true;  // no trigger action, so lets try to jump when we leave
		}

		if (TriggerAction == ATSTRINGHASH("TRIGGER_CHECK_FOR_CLIPS",0xE5C195A2))
		{
			return CheckForClips();  
		}

		if (TriggerAction == ATSTRINGHASH("TRIGGER_QUITGAME_CHECK", 0xB950DBD5) )
		{
			TriggerAction = "TRIGGER_QUIT";
		}

		if (TriggerAction == ATSTRINGHASH("TRIGGER_DISP_AUD_ICON",0x3b542ac1) ||
			TriggerAction == ATSTRINGHASH("TRIGGER_DISP_AMB_ICON",0x39b0526a) ||
			TriggerAction == ATSTRINGHASH("TRIGGER_DISP_SCORE_ICON",0x81165d07))
		{
			return true;  // no trigger action here
		}

		if (TriggerAction == ATSTRINGHASH("TRIGGER_CHECK_FOR_PROJECTS",0x2F4FC570))
		{
			return CheckForProjects();  
		}

		if (TriggerAction == ATSTRINGHASH("TRIGGER_CHECK_FOR_VIDEOS",0xDE73478A))
		{
			return CheckForVideos(); 
		}

		if (TriggerAction == ATSTRINGHASH("TRIGGER_TUTORIAL_URL",0xcb44b46c))
		{
			return LaunchTutorialsUrl(); 
		}

		if (TriggerAction == ATSTRINGHASH("TRIGGER_ADJUST_TEXT_POS_SIZE",0x32b24a94))
		{
			if (CTextTemplate::GetEditedText().m_text[0] != '\0')  // check we have some text
			{
				CVideoEditorUi::SetEditingText();
				CVideoEditorInstructionalButtons::Refresh();
				CVideoEditorMenu::RebuildMenu();
				CVideoEditorMenu::GreyOutCurrentColumn(true);
			}

			return false;
		}

		if (TriggerAction == ATSTRINGHASH("TRIGGER_ADD_CLIP",0xa4c668c1))
		{
			SaveClipFavourites();

			s32 const c_itemIndex = GetFileViewIndexForItem( ms_iCurrentColumn, ms_iCurrentItem[ms_iCurrentColumn] );
			CVideoEditorUi::TriggerAddClipToTimeline( c_itemIndex );

			return false;  // no jump
		}

		if (TriggerAction == ATSTRINGHASH("TRIGGER_ADD_AMBIENT_TRACK",0xfa1054aa))
		{
			if (g_RadioAudioEntity.IsReplayMusicPreviewPlaying())  // turn off any preview music if its playing
			{
				g_RadioAudioEntity.StopReplayTrackPreview();
			}

			s32 const c_previewItem = ms_iCurrentItem[ms_iCurrentColumn];
			if( c_previewItem >= 0 && c_previewItem < ms_PreviewData[ms_iCurrentColumn].GetCount() )
			{
                CVideoEditorPreviewData const& previewData = ms_PreviewData[ms_iCurrentColumn][c_previewItem];

                // Clamp adding ambient tracks at the minimum duration, even though audio can report them as less
                u32 const c_durationMs = previewData.m_durationMs >= MIN_AMBIENT_TRACK_DURATION_MS ? previewData.m_durationMs : MIN_AMBIENT_TRACK_DURATION_MS;
				CVideoEditorUi::SetSelectedAudio( previewData.m_soundHash, 0, c_durationMs, false);

				CVideoEditorUi::TriggerAddAmbientToTimeline();
			}

			return false;  // no jump
		}

		if (TriggerAction == ATSTRINGHASH("TRIGGER_ADD_RADIO_TRACK",0xb6dbf423) || TriggerAction == ATSTRINGHASH("TRIGGER_ADD_SCORE_TRACK",0xd212f3f8))
		{
			if (g_RadioAudioEntity.IsReplayMusicPreviewPlaying())  // turn off any preview music if its playing
			{
				g_RadioAudioEntity.StopReplayTrackPreview();
			}

			s32 const c_previewItem = ms_iCurrentItem[ms_iCurrentColumn];
			if( c_previewItem >= 0 && c_previewItem < ms_PreviewData[ms_iCurrentColumn].GetCount() )
			{
				CVideoEditorUi::SetSelectedAudio( ms_PreviewData[ms_iCurrentColumn][c_previewItem].m_soundHash, 0, ms_PreviewData[ms_iCurrentColumn][c_previewItem].m_durationMs, TriggerAction != ATSTRINGHASH("TRIGGER_ADD_RADIO_TRACK",0xb6dbf423));
				CVideoEditorUi::TriggerAddAudioToTimeline();
			}

			return false;  // no jump
		}

		if (TriggerAction == ATSTRINGHASH("TRIGGER_ADD_TEXT",0x9bc8e760))
		{
			if (CTextTemplate::GetEditedText().m_text[0] != '\0')  // check we have some text
			{
				// if not editing existing text, add text to the timeline that can be positioned
				if (CTextTemplate::GetTextIndexEditing() == -1)
				{
					GoBackOnePane();
					CVideoEditorUi::TriggerAddTextToTimeline();
				}
				else
				{
					CVideoEditorUi::AttemptToReplaceEditedTextWithNewText();
				}
			}
			else
			{
				SetErrorState("FAILED_NO_TEXT");
			}

			return false;  // no jump
		}

		if (TriggerAction == ATSTRINGHASH("TRIGGER_ENTER_TEXT",0x82dec3f0))
		{
			CVideoEditorUi::SetupTextEntry();
			return false;  // no jump
		}

		if (TriggerAction == ATSTRINGHASH("TRIGGER_RENAME_PROJECT",0xe06945d8))
		{
			CVideoProjectFileView const* projectFileView = CVideoEditorUi::GetProjectFileView();
			bool const c_noProjectSlotsLeft = projectFileView ? CVideoEditorInterface::HasReachedMaxProjectLimit( *projectFileView ) : false;
			bool const c_noProjectSpaceLeft = !CVideoEditorInterface::HasMinSpaceToSaveNewProject();

			if( c_noProjectSlotsLeft )
			{
				SetErrorState( "TOO_MANY_PROJECTS_SAVE_AS" );
			}
			else if( c_noProjectSpaceLeft )
			{
				SetErrorState( "NO_SPACE_FOR_PROJECTS_SAVE_AS" );
			}
			else
			{
				CVideoEditorUi::SetupFilenameEntry();
			}

			return false;  // no jump
		}

		if (TriggerAction == ATSTRINGHASH("TRIGGER_NEW_TEXT",0x0434c447))
		{
			CVideoEditorUi::TriggerNewText();
			return true;  // we want to jump from here still
		}

		if( TriggerAction == ATSTRINGHASH("VIEW_GALLERY_FULLSCREEN", 0x805fc7ff ) )
		{
#if defined(VIDEO_PLAYBACK_ENABLED) && VIDEO_PLAYBACK_ENABLED
#if RSG_ORBIS && GTA_REPLAY
			// unlikely to happen, so don't bother making a big song and dance about it
			// but we have to be sure a clip isn't being saved, as we use the replay memory for playback on PS4
			if (!CReplayMgr::IsSaving())
#endif
			{
				CVideoEditorUi::BeginFullscreenVideoPlayback(ms_iCurrentItem[ms_iCurrentColumn]);
			}
#endif
			return false;
		}

		if (TriggerAction == ATSTRINGHASH("TRIGGER_EXP_SIZECHECK",0xC99FC3D8))
		{
			CVideoEditorProject const* project = CVideoEditorUi::GetProject();
			if( ( project && project->GetClipCount() <= 0 ) || g_SysService.IsConstrained() )
			{
				CVideoEditorUi::UpdateLiteralStringAsDuration();
				TriggerWarningText = atStringHash( "VE_EXP" );
				TriggerHeaderText = atStringHash( "VE_EXPORT" );
				literalText = CVideoEditorUi::GetLiteralStringBuffer();
			}
			else 
			{
				CVideoEditorUi::CheckExportLimits();
				return false;
			}
		}

		if ( TriggerAction == ATSTRINGHASH("TRIGGER_EXIT_CHECK", 0x1E0C11D5 ) )
		{
			CVideoEditorProject const* project = CVideoEditorUi::GetProject();
			if( ( project && project->GetClipCount() > 0 ) && CVideoEditorUi::HasProjectBeenEdited() )
			{
				TriggerAction.SetFromString( "WANT_TO_SAVE_BEFORE_EXIT" );
				TriggerWarningText.SetFromString( "VE_EXIT_SAVE_SURE" );
				warningType = FE_WARNING_YES_NO_CANCEL;
			}
			else
			{
				TriggerAction = "TRIGGER_EXIT";
			}
		}

		if( TriggerAction == ATSTRINGHASH( "TRIGGER_PREVIEW_CLIP", 0xF1830098 ) )
		{
			SaveClipFavourites();
			TriggerWarningText = CVideoEditorUi::HasUserBeenWarnedOfGameDestory() ? "VE_PREVIEW_CLIP_DE" : "VE_PREVIEW_CLIP";
		}

		if( TriggerAction == ATSTRINGHASH( "TRIGGER_NEW_BLANK", 0x861DBD1A ) )
		{
			bool const c_hasAnyClips = CheckForClips();

			CVideoProjectFileView const* projectFileView = CVideoEditorUi::GetProjectFileView();
			bool const c_noProjectSlotsLeft = projectFileView ? CVideoEditorInterface::HasReachedMaxProjectLimit( *projectFileView ) : false;
			bool const c_noProjectSpaceLeft = !CVideoEditorInterface::HasMinSpaceToSaveNewProject();

			if( !c_hasAnyClips )
			{
				return false;
			}
			else if( c_noProjectSlotsLeft )
			{
				SetErrorState( "TOO_MANY_PROJECTS" );
				return false;
			}
			else if( c_noProjectSpaceLeft )
			{
				SetErrorState( "NO_SPACE_FOR_PROJECTS" );
				return false;
			}
		}

		// anything else should have a user confirmation screen
		ms_UserConfirmationScreen.SetWithHeader( TriggerAction, TriggerWarningText, TriggerHeaderText, warningType, literalText );
	}

	return false;
}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorMenu::BeginAddingColumnData
// PURPOSE: AS call wrapper - begins adding column data
/////////////////////////////////////////////////////////////////////////////////////////
void CVideoEditorMenu::BeginAddingColumnData(const s32 iColumnId, const s32 iColumnType)
{
	if (CScaleformMgr::BeginMethod(CVideoEditorUi::GetMovieId(), SF_BASE_CLASS_VIDEO_EDITOR, "BEGIN_ADDING_COLUMN_DATA"))
	{
		CScaleformMgr::AddParamInt(iColumnId);
		CScaleformMgr::AddParamInt(iColumnType);
		CScaleformMgr::EndMethod();
	}
}



///////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorMenu::EndAddingColumnData
// PURPOSE: AS call wrapper - ending creating the column data
//			whether we should render the column and whether it is usable ie move up/down on
///////////////////////////////////////////////////////////////////////////////////////////
void CVideoEditorMenu::EndAddingColumnData(const bool bShouldRender, const bool bUsable)
{
	if (CScaleformMgr::BeginMethod(CVideoEditorUi::GetMovieId(), SF_BASE_CLASS_VIDEO_EDITOR, "END_ADDING_COLUMN_DATA"))
	{
		CScaleformMgr::AddParamBool(bShouldRender);
		CScaleformMgr::AddParamBool(bUsable);
		CScaleformMgr::EndMethod();
	}
}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorMenu::ShowAnimatedAudioIcon
// PURPOSE: turns on/off the animated "audio playing" icon on the currently selected
//			item, taking into account any scrolling
/////////////////////////////////////////////////////////////////////////////////////////
void CVideoEditorMenu::ShowAnimatedAudioIcon(bool const c_show)
{
	if (CScaleformMgr::BeginMethod(CVideoEditorUi::GetMovieId(), SF_BASE_CLASS_VIDEO_EDITOR, "SET_ANIMATED_AUDIO_ICON"))
	{
		CScaleformMgr::AddParamInt(ms_iCurrentItem[ms_iCurrentColumn]-ms_iCurrentScrollOffset[ms_iCurrentColumn]);
		CScaleformMgr::AddParamBool(c_show);
		CScaleformMgr::EndMethod();
	}
}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorMenu::GreyOutCurrentColumn
// PURPOSE: Directly sets the passed item as active
/////////////////////////////////////////////////////////////////////////////////////////
void CVideoEditorMenu::GreyOutCurrentColumn(const bool bGreyOut)
{
	if (bGreyOut)
	{
		if (CScaleformMgr::BeginMethod(CVideoEditorUi::GetMovieId(), SF_BASE_CLASS_VIDEO_EDITOR, "SET_ITEMS_GREYED_OUT"))
		{
			CScaleformMgr::EndMethod();
		}
	}
	else
	{
		if (CScaleformMgr::BeginMethod(CVideoEditorUi::GetMovieId(), SF_BASE_CLASS_VIDEO_EDITOR, "SET_ITEMS_UNGREYED_OUT"))
		{
			CScaleformMgr::EndMethod();
		}

		if (CScaleformMgr::BeginMethod(CVideoEditorUi::GetMovieId(), SF_BASE_CLASS_VIDEO_EDITOR, "HIGHLIGHT_ITEM"))
		{
			CScaleformMgr::AddParamInt(ms_iCurrentItem[ms_iCurrentColumn]);
			CScaleformMgr::EndMethod();
		}

		SetItemSelected(ms_iCurrentItem[ms_iCurrentColumn]);
	}
}


void CVideoEditorMenu::SetItemActive( s32 const c_itemIndex )
{
	SetItemActive( GetCurrentMenuId(), ms_iCurrentColumn, c_itemIndex );
}


/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorMenu::SetItemActive
// PURPOSE: sets the menu as the current active item.   Hovers and highlights this item
/////////////////////////////////////////////////////////////////////////////////////////
void CVideoEditorMenu::SetItemActive( s32 const c_menuId, s32 const c_colIndex, s32 const c_itemIndex )
{
	ms_iCurrentItem[c_colIndex] = c_itemIndex;
	ms_iCurrentItem[c_colIndex] = Max( ms_iCurrentItem[c_colIndex], 0 );

	// skip down to next available item
	if( !CanHighlightItem( c_menuId, ms_iCurrentColumn, ms_iCurrentItem[c_colIndex] ) )
	{
		GoToNextItem();
		return;
	}

	SetItemSelected( ms_iCurrentItem[c_colIndex] - ms_iCurrentScrollOffset[c_colIndex] );
}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorMenu::SetItemHovered
// PURPOSE: hovers over the item (GREY)
/////////////////////////////////////////////////////////////////////////////////////////
void CVideoEditorMenu::SetItemHovered( s32 const c_colIndex, s32 const c_itemIndex )
{
	if (uiVerifyf(c_itemIndex >= 0 && c_itemIndex < MAX_ITEMS_IN_COLUMN, "SetItemHovered - trying to call SET_ITEM_ACTIVE on an item outside of the supported range %d", c_itemIndex))
	{
		UnsetAllItemsHovered();

		if (CScaleformMgr::BeginMethod(CVideoEditorUi::GetMovieId(), SF_BASE_CLASS_VIDEO_EDITOR, "SET_ITEM_ACTIVE"))
		{
			CScaleformMgr::AddParamInt(c_colIndex);
			CScaleformMgr::AddParamInt(c_itemIndex);
			CScaleformMgr::EndMethod();
		}
	}
}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorMenu::UnsetAllItemsHovered
// PURPOSE: hovers over the item (GREY)
/////////////////////////////////////////////////////////////////////////////////////////
void CVideoEditorMenu::UnsetAllItemsHovered()
{
	if (CScaleformMgr::BeginMethod(CVideoEditorUi::GetMovieId(), SF_BASE_CLASS_VIDEO_EDITOR, "UNSET_ITEM_ACTIVE"))
	{
		CScaleformMgr::EndMethod();
	}
}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorMenu::SetItemSelected
// PURPOSE: Highlights the item (WHITE)
/////////////////////////////////////////////////////////////////////////////////////////
void CVideoEditorMenu::SetItemSelected( s32 const c_itemIndex )
{
	if (uiVerifyf(c_itemIndex >= 0 && c_itemIndex < MAX_ITEMS_IN_COLUMN, "SetItemSelected - trying to call SET_ITEM_SELECTED on an item outside of the supported range %d", c_itemIndex))
	{
		UnsetAllItemsHovered();

		if (CScaleformMgr::BeginMethod(CVideoEditorUi::GetMovieId(), SF_BASE_CLASS_VIDEO_EDITOR, "SET_ITEM_SELECTED"))
		{
			CScaleformMgr::AddParamInt(c_itemIndex);
			CScaleformMgr::EndMethod();
		}
	}
}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorMenu::SetColumnActive
// PURPOSE: Directly sets the passed column as active
/////////////////////////////////////////////////////////////////////////////////////////
void CVideoEditorMenu::SetColumnActive(const s32 iIndex)
{
	// need some check here on iIndex!
	if (uiVerifyf(iIndex >= 0 && iIndex < MAX_COL_NUMs, "SetColumnActive - trying to set a column outside of the supported range %d", iIndex))
	{
		UnsetAllItemsHovered();

		if (CScaleformMgr::BeginMethod(CVideoEditorUi::GetMovieId(), SF_BASE_CLASS_VIDEO_EDITOR, "SET_COLUMN_ACTIVE"))
		{
			CScaleformMgr::AddParamInt(iIndex);
			CScaleformMgr::EndMethod();
		}

		ms_iCurrentColumn = iIndex;
		ms_iColumnHoveredOver = iIndex;
	}
}

void CVideoEditorMenu::JumpPaneOnAction()
{
	s32 iIndex = GetIndexToUseForAction(ms_iCurrentItem[ms_iCurrentColumn]);
	s32 const c_currentMenuId = GetCurrentMenuId();

	if (FindMenuIndex(ms_MenuArray.CVideoEditorMenuItems[c_currentMenuId].Option[iIndex].JumpMenuId) != -1)
	{
		for (s32 i = 0; i < MAX_COL_NUMs; i++)  // fix for 2192753 - reset the selected items before we build the menus after a jump pane on action
		{
			ms_iCurrentScrollOffset[i] = 0;
			ms_iCurrentItem[i] = 0;
		}

		// store the index we are on before we jump (so we can highlight it later when we go back)
		ms_MenuArray.CVideoEditorMenuItems[c_currentMenuId].iLastKnownSelectedItem = ms_iCurrentItem[ms_iCurrentColumn];

		JumpPane();
	}
}

void CVideoEditorMenu::UnHighlight()
{
	if (CScaleformMgr::BeginMethod(CVideoEditorUi::GetMovieId(), SF_BASE_CLASS_VIDEO_EDITOR, "UNHIGHLIGHT"))
	{
		CScaleformMgr::EndMethod();
	}
}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorMenu::GoToPreviousItem
// PURPOSE: Go to previous item in column
/////////////////////////////////////////////////////////////////////////////////////////
void CVideoEditorMenu::GoToPreviousItem()
{
	// we always set the next columns in the tree to be 0
	for (s32 i = ms_iCurrentColumn + 1; i < MAX_COL_NUMs; i++)
	{
		ms_iCurrentScrollOffset[i] = 0;
		ms_iCurrentItem[i] = 0;
	}

	s32 newIndex = ms_iCurrentItem[ms_iCurrentColumn];
	if (ms_iCurrentItem[ms_iCurrentColumn]-ms_iCurrentScrollOffset[ms_iCurrentColumn] < 0 || ms_iCurrentItem[ms_iCurrentColumn]-ms_iCurrentScrollOffset[ms_iCurrentColumn] >= MAX_ITEMS_IN_COLUMN)  // if out of view, bring back into view 1st 
	{
		MouseScrollList(-(ms_iCurrentScrollOffset[ms_iCurrentColumn]-ms_iCurrentItem[ms_iCurrentColumn]));
	}
	else
	{
		newIndex--;
	}

	eGoToRebuildBehaviour const c_rebuildBehavior = ShouldUpdatePaneOnItemChange() ? GOTO_REBUILD_PREVIEW_PANE : GOTO_REBUILD_AUTO;
	GoToItem( newIndex, c_rebuildBehavior );
}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorMenu::GoToNextItem
// PURPOSE: Go to next item in column
/////////////////////////////////////////////////////////////////////////////////////////
void CVideoEditorMenu::GoToNextItem()
{
	// we always set the next columns in the tree to be 0
	for (s32 i = ms_iCurrentColumn + 1; i < MAX_COL_NUMs; i++)
	{
		ms_iCurrentScrollOffset[i] = 0;
		ms_iCurrentItem[i] = 0;
	}

	s32 newIndex = ms_iCurrentItem[ms_iCurrentColumn];
	if (ms_iCurrentItem[ms_iCurrentColumn]-ms_iCurrentScrollOffset[ms_iCurrentColumn] < 0 || ms_iCurrentItem[ms_iCurrentColumn]-ms_iCurrentScrollOffset[ms_iCurrentColumn] >= MAX_ITEMS_IN_COLUMN)  // if out of view, bring back into view 1st 
	{
		MouseScrollList(-(ms_iCurrentScrollOffset[ms_iCurrentColumn]-ms_iCurrentItem[ms_iCurrentColumn]));
	}
	else
	{
		newIndex++;
	}

	eGoToRebuildBehaviour const c_rebuildBehavior = ShouldUpdatePaneOnItemChange() ? GOTO_REBUILD_PREVIEW_PANE : GOTO_REBUILD_AUTO;
	GoToItem( newIndex, c_rebuildBehavior );
}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	ShouldUpdatePaneOnItemChange
// PURPOSE: returns true/false whether a pane needs to be updated or not based on
//			the pane it is linked to
/////////////////////////////////////////////////////////////////////////////////////////
bool CVideoEditorMenu::ShouldUpdatePaneOnItemChange()
{
	CVideoEditorMenuItem& menuItem = ms_MenuArray.CVideoEditorMenuItems[GetCurrentMenuId()];

	if (ms_iCurrentItem[ms_iCurrentColumn] < menuItem.Option.GetCount())
	{
		atHashString const c_linkMenuId = menuItem.Option[ms_iCurrentItem[ms_iCurrentColumn]].LinkMenuId;

		bool const c_TextPreview = (c_linkMenuId == ATSTRINGHASH("TEXT_PREVIEW",0x66ebfd0f));

		if (c_TextPreview)  // do not update pane when on text preview
		{
			return false;
		}
	}

	return true;  // update pane
}


s32 CVideoEditorMenu::GetNumberOfItemsInColumn( s32 const menuId, s32 const c_column )
{
	s32 maxItemsInColumn = 0;

	if( menuId >= 0 && menuId < ms_MenuArray.CVideoEditorMenuItems.GetCount() )
	{
		CVideoEditorMenuItem const& menu = ms_MenuArray.CVideoEditorMenuItems[ menuId ];
		switch( menu.columnDataType )
		{
		case COL_DATA_TYPE_STANDARD:
			{
				maxItemsInColumn = menu.Option.GetCount();
				break;
			}

		case COL_DATA_TYPE_LOAD_FILES:
		case COL_DATA_TYPE_AUDIO_LIST:
			{
				maxItemsInColumn = ms_PreviewData[c_column].GetCount();
				break;
			}

		default:
			{
				uiAssertf(0, "CVideoEditorMenu::GetNumberOfItemsInColumn() Invalid columnDataType %d", 
					(s32)menu.columnDataType);
			}
		}
	}

	return maxItemsInColumn;
}


void CVideoEditorMenu::GoToItem( s32 const c_itemIndex, eGoToRebuildBehaviour const rebuildBehaviour )
{
	eGoToRebuildBehaviour localRebuildBehaviour = rebuildBehaviour;
	// we never update preview pane when we are editing text as its always the same texture
	if( CTextTemplate::GetTextIndexEditing() != -1 )
	{
		localRebuildBehaviour = GOTO_REBUILD_NONE;
	}

	GoToItem( c_itemIndex, ms_iCurrentColumn, localRebuildBehaviour );
}

void CVideoEditorMenu::GoToItem( s32 itemIndex, const s32 iColumn, eGoToRebuildBehaviour const rebuildBehaviour )
{
	ms_iColumnHoveredOver = iColumn;

	s32 const c_menuId = ms_iMenuIdForColumn[ iColumn ];
	s32 const c_maxItemsInColumn = GetNumberOfItemsInColumn( c_menuId, iColumn );

	s32 const c_lastItemIndex = c_maxItemsInColumn - 1;
	s32 const c_lastHighlightIndex = MAX_ITEMS_IN_COLUMN - 1;

	bool const c_overflowEnd = itemIndex >= c_maxItemsInColumn;
	bool const c_underflowStart = itemIndex < 0;

	//! Wrap around on out of range
	itemIndex = c_underflowStart ? c_lastItemIndex : c_overflowEnd ? 0 : itemIndex;

	bool const c_nonSelectable = !CanHighlightItem( c_menuId, ms_iCurrentColumn, itemIndex );

	// skip any items that are not selectable
	if (c_nonSelectable)
	{
		// find new index
		const s32 c_newIndex = ((itemIndex < ms_iCurrentItem[iColumn] && !c_overflowEnd) || c_underflowStart) ? itemIndex - 1 : itemIndex + 1;

		ms_iCurrentItem[iColumn] = itemIndex;

		GoToItem( c_newIndex, iColumn, rebuildBehaviour );

		return;
	}

	if ( itemIndex >= 0 && itemIndex < c_maxItemsInColumn && c_maxItemsInColumn >= 1 )
	{
		bool const c_updatingPreviewPane = rebuildBehaviour == GOTO_REBUILD_PREVIEW_PANE;
		bool fullRebuild = rebuildBehaviour == GOTO_REBUILD_AUTO;

		// Target data for new item
		s32 newScrollOffset = 0;
		s32 newItemHighlight = itemIndex;

		// If we need to do scrolling, then we need to adjust the target data appropriately
		if( c_maxItemsInColumn > MAX_ITEMS_IN_COLUMN ) 
		{
			s32 const c_currentItemIndex = ms_iCurrentItem[iColumn];
			s32 const c_currentScrollOffset = ms_iCurrentScrollOffset[iColumn];
			s32 const c_currentScrollSpaceIndex = c_currentItemIndex - c_currentScrollOffset;
			s32 const c_diffIndex = itemIndex - c_currentItemIndex;

			//! Wrap top around
			if( c_underflowStart )
			{
				newScrollOffset = c_maxItemsInColumn - MAX_ITEMS_IN_COLUMN;
				newItemHighlight = c_lastHighlightIndex;
				fullRebuild = true;
			}
			//! Wrap bottom around
			else if( c_overflowEnd )
			{
				newScrollOffset = 0;
				newItemHighlight = 0;
				fullRebuild = true;
			}
			//! Edgewise scrolling
			else if( ( c_currentScrollSpaceIndex == 0 && c_diffIndex < 0 ) || 
					 ( c_currentScrollSpaceIndex == c_lastHighlightIndex && c_diffIndex > 0 ) )
			{
				newScrollOffset = c_currentScrollOffset + c_diffIndex;
				newItemHighlight = c_currentScrollSpaceIndex;
				fullRebuild = true;
			}
			// Jumping our of current scroll window
			else if( c_currentScrollSpaceIndex + c_diffIndex >= MAX_ITEMS_IN_COLUMN || 
				c_currentScrollSpaceIndex + c_diffIndex < 0 )
			{
				newItemHighlight = c_diffIndex > 0 ? c_lastHighlightIndex : 0;
				newScrollOffset = c_currentScrollOffset + c_currentScrollSpaceIndex + c_diffIndex - newItemHighlight;
				fullRebuild = true;
			}
			// ...otherwise scrolling internally
			else
			{
				newScrollOffset = c_currentScrollOffset;
				newItemHighlight = c_currentScrollSpaceIndex + c_diffIndex;
			}

			newScrollOffset = uiVerifyf( newScrollOffset >= 0, "CVideoEditorMenu::GoToItem - Negative scroll offset calculated.\n"
				"newScrollOffset %d, newItemHighlight %d, c_diffIndex %d, c_lastHighlightIndex %d, c_currentScrollOffset %d, c_currentScrollSpaceIndex %d",
				newScrollOffset, newItemHighlight, c_diffIndex, c_lastHighlightIndex, c_currentScrollOffset, c_currentScrollSpaceIndex ) 
				? newScrollOffset : 0;
		}

		ms_iCurrentScrollOffset[iColumn] = newScrollOffset;

		SetItemActive( c_menuId, iColumn, itemIndex );

		if( rebuildBehaviour != GOTO_REBUILD_NONE )
		{
			if( fullRebuild && c_updatingPreviewPane )
			{
				RebuildMenu();
				SetItemActive( c_menuId, iColumn, itemIndex );
			}
			else
			{
				if ( c_maxItemsInColumn > MAX_ITEMS_IN_COLUMN )  // only rebuild when its the column we are on that is over the max items (fixes 2285492)
				{
					RebuildColumn(iColumn);
				}

				if( c_updatingPreviewPane )
				{
					UpdatePane();
					SetItemActive( c_menuId, iColumn, itemIndex );
				}
			}
		}
	}

	if( c_maxItemsInColumn > MAX_ITEMS_IN_COLUMN )
	{
		UpdateFooterText();
	}
}

void CVideoEditorMenu::AddSizeIndicator()
{
	if( CScaleformMgr::BeginMethod(CVideoEditorUi::GetMovieId(), SF_BASE_CLASS_VIDEO_EDITOR, "ADD_PROJECT_SIZE_DISPLAY" ) )
	{
		CScaleformMgr::EndMethod();
	}
}

void CVideoEditorMenu::SetupSizeIndicator( char const * const titleLngKey, float const startingValue )
{
	if( titleLngKey && 
		CScaleformMgr::BeginMethod(CVideoEditorUi::GetMovieId(), SF_BASE_CLASS_VIDEO_EDITOR, "UPDATE_COLUMN_PROJECT_SIZE_TITLE" ) )
	{
		CScaleformMgr::AddParamString( TheText.Get( titleLngKey ), false );
		CScaleformMgr::EndMethod();
	}

	UpdateSizeIndicator( startingValue );
}

void CVideoEditorMenu::UpdateSizeIndicator( float const sizeValue )
{
	if( CScaleformMgr::BeginMethod(CVideoEditorUi::GetMovieId(), SF_BASE_CLASS_VIDEO_EDITOR, "UPDATE_COLUMN_PROJECT_SIZE" ) )
	{
		CScaleformMgr::AddParamFloat( sizeValue );
		CScaleformMgr::EndMethod();
	}
}

void CVideoEditorMenu::RepositionSizeIndicator()
{
	if( CScaleformMgr::BeginMethod(CVideoEditorUi::GetMovieId(), SF_BASE_CLASS_VIDEO_EDITOR, "UPDATE_COLUMN_PROJECT_SIZE_X_POSITION" ) )
	{
		Vector2 safeZoneMin, safeZoneMax;
		CHudTools::GetMinSafeZoneForScaleformMovies(safeZoneMin.x, safeZoneMin.y, safeZoneMax.x, safeZoneMax.y);

		float const c_xPosition = ACTIONSCRIPT_STAGE_SIZE_X * safeZoneMin.x;
		CScaleformMgr::AddParamFloat( c_xPosition );
		CScaleformMgr::EndMethod();
	}
}

float CVideoEditorMenu::CalculateActiveProjectSize()
{
	CVideoEditorProject const* currentProject = CVideoEditorUi::GetProject();

	float const c_maxSize = currentProject ? (float)currentProject->ApproximateMaximumSizeForSaving() : 100.f;
	float const c_currentSize = currentProject ? (float)currentProject->ApproximateSizeForSaving() : 0.f;

	float const c_result = ( 100.f / c_maxSize ) * c_currentSize;
	uiDisplayf( "CVideoEditorMenu::CalculateActiveProjectSize - Project size calculated as %.3f%%. Max size %.3f mb, current size %.3f mb", 
		c_result, c_maxSize / 1024 / 1024, c_currentSize / 1024 / 1024 );

	return c_result;
}

void CVideoEditorMenu::MouseScrollList( s32 const delta )
{
	// NOTE - This isn't called anymore because UI Designers don't understand how to use a mouse wheel.
	//		  Code is left here in the hopes they come to their senses and we can enable proper scrolling for long lists

	s32 const c_currentMenuId = GetCurrentMenuId();
	s32 const c_maxItemsInColumn = GetNumberOfItemsInColumn( c_currentMenuId, ms_iCurrentColumn );
	s32 const targetScrollOffset = ms_iCurrentScrollOffset[ms_iCurrentColumn] + delta;
	s32 const newScrollOffset = Clamp( targetScrollOffset, 0, c_maxItemsInColumn - MAX_ITEMS_IN_COLUMN );
	
	if( c_maxItemsInColumn > MAX_ITEMS_IN_COLUMN && newScrollOffset != ms_iCurrentScrollOffset[ms_iCurrentColumn] )
	{
		s32 const c_currentItemIndex = ms_iCurrentItem[ms_iCurrentColumn] - newScrollOffset;

		ms_iCurrentScrollOffset[ms_iCurrentColumn] = newScrollOffset;
		
		BuildMenu( c_currentMenuId, false );

		// only set the item selected if its within the menu range (as it can now be off the screen (scrolled off))
		if (c_currentItemIndex >= 0 && c_currentItemIndex < MAX_ITEMS_IN_COLUMN)
		{
			SetItemSelected(c_currentItemIndex);
		}
	}
}

bool CVideoEditorMenu::IsProjectValid()
{
	return (CVideoEditorUi::GetProject() != NULL);
}

/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorMenu::GoToPreviousColumn
// PURPOSE: Go to previous column in tree
/////////////////////////////////////////////////////////////////////////////////////////
void CVideoEditorMenu::GoToPreviousColumn()
{
	if (ms_iCurrentColumn > 0)
	{
		ms_iCurrentColumn--;
	}

	UnsetAllItemsHovered();

	if (CScaleformMgr::BeginMethod(CVideoEditorUi::GetMovieId(), SF_BASE_CLASS_VIDEO_EDITOR, "GO_TO_PREVIOUS_COLUMN"))
	{
		CScaleformMgr::EndMethod();
	}

	ms_iColumnHoveredOver = ms_iCurrentColumn;
}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorMenu::GoToNextColumn
// PURPOSE: Go to next column in tree
/////////////////////////////////////////////////////////////////////////////////////////
void CVideoEditorMenu::GoToNextColumn()
{
	eGoToRebuildBehaviour const rebuildBehavior = IsFileFilteringMenu( GetCurrentMenuId() ) ? GOTO_REBUILD_NONE : GOTO_REBUILD_PREVIEW_PANE;

	if (ms_iCurrentColumn < MAX_COL_NUMs)
	{
		ms_iCurrentColumn++;
	}

	UnsetAllItemsHovered();

	if (CScaleformMgr::BeginMethod(CVideoEditorUi::GetMovieId(), SF_BASE_CLASS_VIDEO_EDITOR, "GO_TO_NEXT_COLUMN"))
	{
		CScaleformMgr::EndMethod();
	}

	ms_iColumnHoveredOver = ms_iCurrentColumn;

	GoToItem( ms_iCurrentItem[ms_iCurrentColumn], ms_iCurrentColumn, rebuildBehavior );
}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorMenu::UpdatePane
// PURPOSE: Updates the columns to the right of the current column
/////////////////////////////////////////////////////////////////////////////////////////
void CVideoEditorMenu::UpdatePane()
{
	s32 const c_currentMenuId = GetCurrentMenuId();
	CVideoEditorMenuItem& menuItem = ms_MenuArray.CVideoEditorMenuItems[c_currentMenuId];
	s32 const c_optionIndex = menuItem.columnDataType == COL_DATA_TYPE_STANDARD ? ms_iCurrentItem[ms_iCurrentColumn] : 0;
	s32 const c_optionCount = menuItem.Option.GetCount();
	s32 const c_linkMenuId =  c_optionCount > 0 && c_optionIndex < c_optionCount  ? menuItem.Option[c_optionIndex].LinkMenuId.GetHash() : -1;
	s32 const c_linkMenuIndex = FindMenuIndex( GenerateMenuId( c_currentMenuId, c_linkMenuId ) );

	if ( c_linkMenuIndex != -1 )
	{
		uiDisplayf( "Building sub-Menu %d", c_linkMenuIndex );
		BuildMenu( c_linkMenuIndex, true );
		SetItemSelected( ms_iCurrentItem[ms_iCurrentColumn] - ms_iCurrentScrollOffset[ms_iCurrentColumn] );
	}
}


bool CVideoEditorMenu::SortAndFilterCurrentFileView( s32 const menuId, s32 const sort, s32 const filter )
{
	bool success = false;

	CFileViewFilter* currentFileView = GetFileViewFilterForMenu( menuId );
	if( currentFileView )
	{
		ReleaseThumbnailOfItem();
		currentFileView->applyFilterAndSort( filter, sort, currentFileView->IsSortInverted() );
		success = true;
	}

	return success;
}


/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorMenu::ExecuteDependentAction
// PURPOSE: Executes a dependent action for basic lists
/////////////////////////////////////////////////////////////////////////////////////////
bool CVideoEditorMenu::ExecuteDependentAction( s32 const menuId, s32 const action )
{
	bool success = false;

	if( action == sc_sortDate )
	{
		s32 const c_sortType = CFileViewFilter::SORTTYPE_FILETIME;
		s32 const c_filterType = CFileViewFilter::FILTERTYPE_LOCAL_USER_OWNED;

		success = SortAndFilterCurrentFileView( menuId, c_sortType, c_filterType );
	}
	else if( action == sc_sortName )
	{
		s32 const c_sortType = CFileViewFilter::SORTTYPE_DISPLAYNAME;
		s32 const c_filterType = CFileViewFilter::FILTERTYPE_LOCAL_USER_OWNED;

		success = SortAndFilterCurrentFileView( menuId, c_sortType, c_filterType );
	}
	else if( action == sc_sortSize )
	{
		s32 const c_sortType = CFileViewFilter::SORTTYPE_FILESIZE;
		s32 const c_filterType = CFileViewFilter::FILTERTYPE_LOCAL_USER_OWNED;

		success = SortAndFilterCurrentFileView( menuId, c_sortType, c_filterType );
	}
	else if( action == sc_sortCreatedToday )
	{
		s32 const c_sortType = CFileViewFilter::SORTTYPE_FILETIME;
		s32 const c_filterType = CFileViewFilter::FILTERTYPE_MODIFIED_TODAY_LOCAL_USER_OWNED;

		success = SortAndFilterCurrentFileView( menuId, c_sortType, c_filterType );
	}
	else if( action == sc_sortClipsInProject && IsOnClipFilteringMenu() )
	{
		s32 const c_sortType = CFileViewFilter::SORTTYPE_DISPLAYNAME;
		s32 const c_filterType = CRawClipFileView::FILTERTYPE_USED_IN_PROJECT;

		success = SortAndFilterCurrentFileView( menuId, c_sortType, c_filterType );
	}
	else if( action == sc_sortOtherUsers )
	{
		s32 const c_sortType = CFileViewFilter::SORTTYPE_DISPLAYNAME;
		s32 const c_filterType = CFileViewFilter::FILTERTYPE_OTHER_USER;

		success = SortAndFilterCurrentFileView( menuId, c_sortType, c_filterType );
	}
	else if( action == sc_sortAllUsers )
	{
		s32 const c_sortType = CFileViewFilter::SORTTYPE_DISPLAYNAME;
		s32 const c_filterType = CFileViewFilter::FILTERTYPE_ANY_USER;

		success = SortAndFilterCurrentFileView( menuId, c_sortType, c_filterType );
	}
	else if( action == sc_sortFav && IsOnClipFilteringMenu() )
	{
		s32 const c_sortType = CFileViewFilter::SORTTYPE_DISPLAYNAME;
		s32 const c_filterType = CRawClipFileView::FILTERTYPE_FAVOURITES;

		success = SortAndFilterCurrentFileView( menuId, c_sortType, c_filterType );
	}
	else if( action == sc_sortMarkDelete  )
	{
		s32 const c_sortType = CFileViewFilter::SORTTYPE_DISPLAYNAME;
		s32 const c_filterType = CFileViewFilter::FILTERTYPE_MARKED_DELETE_ANY_USER;

		success = SortAndFilterCurrentFileView( menuId, c_sortType, c_filterType );
	}

	return success;
}


/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorMenu::CheckForDependentData
// PURPOSE: Checks to see if we need to set up any data before we populate the current
//			pane
/////////////////////////////////////////////////////////////////////////////////////////
bool CVideoEditorMenu::CheckForDependentData(const s32 iMenuId, const s32 iIndex, const s32 iColumn )
{	
	if (iIndex == -1)
	{
		return false;
	}

	bool dataChanged = false;
	s32 iIndexToCheck = iIndex;

	if ( ms_MenuArray.CVideoEditorMenuItems[iMenuId].columnDataType == COL_DATA_TYPE_LOAD_FILES ||
  		 ms_MenuArray.CVideoEditorMenuItems[iMenuId].columnDataType == COL_DATA_TYPE_AUDIO_LIST )
	{
		iIndexToCheck = 0;
	}

	atHashString const c_dependentAction = ms_MenuArray.CVideoEditorMenuItems[iMenuId].Option[iIndexToCheck].DependentAction;  // find out if any dependent action is required before populating this menu

	if ( c_dependentAction.IsNotNull() )
	{
		s32 const c_actionHash = (s32)c_dependentAction.GetHash();

		if( DoesDependentDataNeedRebuilt( iColumn, iMenuId, c_actionHash ) )
		{
			bool const c_releaseThumbnail = true;
			ClearPreviewData( iColumn, c_releaseThumbnail, false );

			if (c_dependentAction == sc_loadProjPreviewId )
			{
				dataChanged = FillInLoadProjectData(iColumn);
			}
			else if (c_dependentAction == sc_galleryPreviewId )
			{
#if defined(VIDEO_PLAYBACK_ENABLED) && VIDEO_PLAYBACK_ENABLED
				dataChanged = FillInGalleryData(iColumn);
#endif
			}
			else if (c_dependentAction == sc_clipPreviewId )
			{
				dataChanged = FillInClipData(iColumn);
			}
			else if (c_dependentAction == sc_ambientPreviewId )
			{
				dataChanged = FillInAudioData(iColumn, REPLAY_AMBIENT_TRACK);
			}
			else if (c_dependentAction == sc_audioRadioStationPreviewId )
			{
				dataChanged = FillInAudioData(iColumn, REPLAY_RADIO_MUSIC);
			}
			else if (c_dependentAction == sc_audioScorePreviewId )
			{
				dataChanged = FillInAudioData(iColumn, REPLAY_SCORE_MUSIC);
			}

			if( dataChanged )
			{
				ms_previewDataId[ iColumn ] = c_actionHash;
				RefreshDependentDataBuildFlags( iColumn, iMenuId );
			}
			else
			{
				ms_previewDataId[ iColumn ] = -1;
				ms_previewDataSort[ iColumn ] =-1;
				ms_previewDataFilter[ iColumn ] = -1;
                ms_previewDataInversion[ iColumn ] = true;
			}
		}
	}

	return dataChanged;
}

bool CVideoEditorMenu::DoesDependentDataNeedRebuilt( const s32 iColumn, const s32 iMenuId, const s32 actionHash )
{
	CFileViewFilter const * fileviewFilter = GetFileViewFilterForMenu( iMenuId ); 

	bool const c_hasDataTypeChanged = actionHash != ms_previewDataId[iColumn];
	bool const c_hasFileSortChanged = fileviewFilter ? ms_previewDataSort[iColumn] != fileviewFilter->GetCurrentSortType() : false;
    bool const c_hasFileFilterChanged = fileviewFilter ? ms_previewDataFilter[iColumn] != fileviewFilter->GetCurrentFilterType() : false;
    bool const c_hasFileInversionChanged = fileviewFilter ? ms_previewDataInversion[iColumn] != fileviewFilter->IsSortInverted() : false;
	bool const c_hasAudioFilterChanged = IsAudioCategoryList( iMenuId ) || IsAudioTrackList( iMenuId );
	
	bool const c_result = c_hasDataTypeChanged || c_hasFileSortChanged || c_hasFileFilterChanged || c_hasAudioFilterChanged || c_hasFileInversionChanged;
	return c_result;
}

void CVideoEditorMenu::RefreshDependentDataBuildFlags( const s32 iColumn, const s32 iMenuId )
{
	CFileViewFilter const * fileviewFilter = GetFileViewFilterForMenu( iMenuId );
	ms_previewDataSort[iColumn] = fileviewFilter ? fileviewFilter->GetCurrentSortType() : -1;
	ms_previewDataFilter[iColumn] = fileviewFilter ? fileviewFilter->GetCurrentFilterType() : -1;
    ms_previewDataInversion[iColumn] = fileviewFilter ? fileviewFilter->IsSortInverted() : true;
}

// PURPOSE: returns if there are any clips available. Also displays error message
bool CVideoEditorMenu::CheckForClips()
{
    CRawClipFileView const * const c_clipFileView = CVideoEditorUi::GetClipFileView();

	size_t const c_fileCount = c_clipFileView ? c_clipFileView->getUnfilteredFileCount() : 0;
    size_t const c_currentUserfileCount = c_clipFileView ? c_clipFileView->getUnfilteredFileCount( true ) : 0;

    s32 const c_currentMenuId = GetCurrentMenuId();
    const atHashString JumpMenuId = ms_MenuArray.CVideoEditorMenuItems[c_currentMenuId].Option[ms_iCurrentItem[ms_iCurrentColumn]].JumpMenuId;

    bool result = true;

	if ( c_fileCount == 0 && JumpMenuId == ATSTRINGHASH("CLIP_MGR_FILT_LIST",0x712BF34B))  // going to clip management
	{
		SetErrorState("FAILED_NO_CLIPS_MGR");
        result = false;
	}
	else if ( c_currentUserfileCount == 0 && JumpMenuId == ATSTRINGHASH("ADD_LIST",0x0eae3958) )  // going to 'new project'
	{
		SetErrorState("FAILED_NO_CLIPS_NEW");
        result = false;
	}
	else if ( c_currentUserfileCount == 0 && JumpMenuId == ATSTRINGHASH("CLIP_FILT_LIST",0x83238537) )  // going to 'add clip'
	{
		SetErrorState("FAILED_NO_CLIPS_ADD");
        result = false;
	}

	return result;
}

// PURPOSE: returns if there are any projects available. Also displays error message
bool CVideoEditorMenu::CheckForProjects()
{
	size_t fileCount = 0;
	CVideoProjectFileView *pProjectFileView = CVideoEditorUi::GetProjectFileView();
	if (pProjectFileView)
		fileCount = pProjectFileView->getUnfilteredFileCount();

	if(fileCount == 0)
	{
		SetErrorState("FAILED_NO_PROJECTS");
		return false;
	}
	return true;
}

// PURPOSE: returns if there are any videos available. Also displays error message
bool CVideoEditorMenu::CheckForVideos()
{
	size_t fileCount = 0;
#if VIDEO_PLAYBACK_ENABLED
	CVideoFileView *pVideoFileView = CVideoEditorUi::GetVideoFileView();
	if (pVideoFileView)
		fileCount = pVideoFileView->getUnfilteredFileCount();
#endif
	if(fileCount == 0)
	{
		SetErrorState("FAILED_NO_VIDEOS");
		return false;
	}
	return true;
}


bool CVideoEditorMenu::LaunchTutorialsUrl()
{
	return (g_SystemUi.ShowWebBrowser(NetworkInterface::GetLocalGamerIndex(), ROCKSTAR_EDITOR_TUTORIAL_URL));
}

/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorMenu::RemoveColumn
// PURPOSE: AS wrapper: remove column
/////////////////////////////////////////////////////////////////////////////////////////
void CVideoEditorMenu::RemoveColumn(const s32 iColumnToClear)
{
	if( iColumnToClear < MAX_COL_NUMs )
	{
		uiDisplayf("Removing Column %d", iColumnToClear);

		if (CScaleformMgr::BeginMethod(CVideoEditorUi::GetMovieId(), SF_BASE_CLASS_VIDEO_EDITOR, "REMOVE_COLUMN"))
		{
			CScaleformMgr::AddParamInt(iColumnToClear);
			CScaleformMgr::EndMethod();
		}

		ms_iCurrentItem[iColumnToClear] = 0;
		ms_iMenuIdForColumn[iColumnToClear] = -1;
	}
}


void CVideoEditorMenu::UpdateItemColouration( s32 const columnId, s32 const listIndex, s32 const itemIndex )
{
	if( CScaleformMgr::BeginMethod(CVideoEditorUi::GetMovieId(), SF_BASE_CLASS_VIDEO_EDITOR, "SET_COL_TYPE_LIST_COLOUR_IN_COLUMN"))
	{
		CVideoEditorPreviewData const& previewData = ms_PreviewData[columnId][itemIndex];

		s32 const c_colour = previewData.m_flaggedForDelete ? MENU_ITEM_COLOUR_PENDING_DELETE :
			previewData.m_favourite ? MENU_ITEM_COLOUR_FAVOURITE : MENU_ITEM_COLOUR_NONE;

		CScaleformMgr::AddParamInt( columnId );
		CScaleformMgr::AddParamInt( listIndex );
		CScaleformMgr::AddParamInt( c_colour );
		CScaleformMgr::EndMethod();
	}
}


/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorMenu::AdjustToggleValue
// PURPOSE: takes the hash of a toogle value and changes the variable we use with it
/////////////////////////////////////////////////////////////////////////////////////////
void CVideoEditorMenu::AdjustToggleValue(const s32 iDirection, const atHashString ToggleHash)
{
	s32 const c_currentMenuId = GetCurrentMenuId();
	if (!IsItemSelectable(c_currentMenuId, ms_iCurrentColumn, ms_iCurrentItem[ms_iCurrentColumn]))  // if item is not selectable, then do not allow adjustment
	{
		return;
	}

	SetItemSelected(ms_iCurrentItem[ms_iCurrentColumn]);  // ensure item being toggled is the selected item

	if (ToggleHash == ATSTRINGHASH("TOGGLE_LENGTH_SELECTION",0x6f041b3d))
	{
		CVideoEditorUi::EditLengthSelection(iDirection);
	}

	if (ToggleHash == ATSTRINGHASH("TOGGLE_TEXT_COLOR",0xa88888ea))
	{
		CTextTemplate::EditTextColor(iDirection);
	}

	if (ToggleHash == ATSTRINGHASH("TOGGLE_TEXT_FONT",0x721248b3))
	{
		CTextTemplate::EditTextFont(iDirection);
	}

	if (ToggleHash == ATSTRINGHASH("TOGGLE_TEXT_OPACITY",0x10bdaa2c))
	{
		CTextTemplate::EditTextOpacity(iDirection);
	}

	if (ToggleHash == ATSTRINGHASH("TOGGLE_TEXT_DURATION",0xb2c85bb6))
	{
		CVideoEditorUi::EditTextDuration(iDirection);
	}

	if (ToggleHash == ATSTRINGHASH("TOGGLE_TEXT_TEMPLATE",0xa70897f7))
	{
		CVideoEditorUi::EditTextTemplate(iDirection);
	}

	if (ToggleHash == ATSTRINGHASH("TOGGLE_TEXT_POSITION",0xbd49fe26))
	{
		CVideoEditorUi::EditTemplatePosition(iDirection);
	}

	if (ToggleHash == ATSTRINGHASH("TOGGLE_EXPORT_FPS",0x8a8e3b0c))
	{
		CVideoEditorUi::EditExportFps(iDirection);
	}

	if (ToggleHash == ATSTRINGHASH("TOGGLE_EXPORT_QUALITY",0x24866c43))
	{
		CVideoEditorUi::EditExportQuality(iDirection);
	}

	RebuildColumn();
}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorMenu::GetToggleString
// PURPOSE: returns the string we want to display based on the passed ToggleHash and the
//			current value of the variable in use
/////////////////////////////////////////////////////////////////////////////////////////
const char *CVideoEditorMenu::GetToggleString(atHashString const c_toggleHash, atHashString const c_UniqueId)
{
	static char cReturnText[100];

	cReturnText[0] = '\0';

	//
	// Auto Generated Movie Length Selection:
	//
	if (c_toggleHash == ATSTRINGHASH("TOGGLE_LENGTH_SELECTION",0x6f041b3d))
	{
		return TheText.Get( CReplayEditorParameters::GetAutogenDurationLngKey( (CReplayEditorParameters::eAutoGenerationDuration)CVideoEditorUi::GetAutomatedLengthSelection() ).GetHash(), "" );
	}

	sEditedTextProperties &editedText = CTextTemplate::GetEditedText();

	if (c_toggleHash == ATSTRINGHASH("TOGGLE_TEXT_COLOR",0xa88888ea))
	{
		if (editedText.m_hudColor != HUD_COLOUR_PURE_WHITE)
		{
			s32 const c_PropertyIndex = CTextTemplate::GetPropertyIndex(c_UniqueId);
			s32 const c_index = CTextTemplate::GetPropertyValueHash(c_PropertyIndex, CHudColour::GetKeyFromColour(editedText.m_hudColor));
			CPropertyData const *pPropertyData = CTextTemplate::GetPropertyData(c_UniqueId, c_index);

			if (pPropertyData)
			{
				eHUD_COLOURS const c_hudColor = CHudColour::GetColourFromKey(pPropertyData->valueHash.GetHash());

				formatf(cReturnText, "%s", CHudColour::GetNameString(c_hudColor), NELEM(cReturnText));
			}
		}

		return cReturnText;
	}

	if (c_toggleHash == ATSTRINGHASH("TOGGLE_TEXT_FONT",0x721248b3))
	{
		s32 const c_PropertyIndex = CTextTemplate::GetPropertyIndex(c_UniqueId);
		s32 const c_index = CTextTemplate::GetPropertyValueNumber(c_PropertyIndex, editedText.m_style);
		CPropertyData const *pPropertyData = CTextTemplate::GetPropertyData(c_UniqueId, c_index);

		if (pPropertyData)
		{
			formatf(cReturnText, "%s %d", TheText.Get("VEUI_TEXT_FONT"), c_index + 1, NELEM(cReturnText));
		}

		return cReturnText;
	}

	if (c_toggleHash == ATSTRINGHASH("TOGGLE_TEXT_OPACITY",0x10bdaa2c))
	{
		formatf(cReturnText, "%0.0f%%", editedText.m_alpha * 100.0f, NELEM(cReturnText));

		return cReturnText;
	}

	if (c_toggleHash == ATSTRINGHASH("TOGGLE_TEXT_DURATION",0xb2c85bb6))
	{
		formatf(cReturnText, "%0.2f%s", float(editedText.m_durationMs / 1000.0f), TheText.Get("VEUI_DUR_SEC"), NELEM(cReturnText));

		return cReturnText;
	}

	if (c_toggleHash == ATSTRINGHASH("TOGGLE_TEXT_TEMPLATE",0xa70897f7))
	{
		const char *c_currentTemplateName = CTextTemplate::FindTemplateName(CTextTemplate::GetCurrentTemplate());

		formatf(cReturnText, "%s", c_currentTemplateName, NELEM(cReturnText));

		return cReturnText;
	}

	if (c_toggleHash == ATSTRINGHASH("TOGGLE_TEXT_POSITION",0xbd49fe26))
	{
		const CPropertyData *pPropertyData = CTextTemplate::GetPropertyData(c_UniqueId, editedText.m_positionType);

		if (pPropertyData)
		{
			const char *c_displayText = TheText.Get(pPropertyData->textId.GetHash(), "");
				
			formatf(cReturnText, "%s", c_displayText, NELEM(cReturnText));
		}
			
		return cReturnText;
	}

	const sExportProperties& exportProperties = CVideoEditorUi::GetExportProperties();
	if (c_toggleHash == ATSTRINGHASH("TOGGLE_EXPORT_FPS",0x8a8e3b0c))
	{
		formatf(cReturnText, "%s", TheText.Get(MediaEncoderParams::GetOutputFpsDisplayName(MediaEncoderParams::eOutputFps(exportProperties.m_fps))));

		return cReturnText;
	}

	if (c_toggleHash == ATSTRINGHASH("TOGGLE_EXPORT_QUALITY",0x24866c43))
	{
		formatf(cReturnText, "%s", TheText.Get(MediaEncoderParams::GetQualityLevelDisplayName(MediaEncoderParams::eQualityLevel(exportProperties.m_quality))));

		return cReturnText;
	}

	return NULL;
}


/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorMenu::BuildMenu
// PURPOSE: Builds the menu from the xml structure
/////////////////////////////////////////////////////////////////////////////////////////
s32 CVideoEditorMenu::BuildMenu(const s32 iIndex, const bool bBuildBranches, const s32 iStartingColumn)
{
	s32 iNextLinkMenuId = -1;

	s32 iColumnId = ms_MenuArray.CVideoEditorMenuItems[iIndex].columnId;

	if (iStartingColumn != -1 && iColumnId != iStartingColumn) // if a specific column is passed, only process the menu that uses this column
	{
		return iColumnId;
	}

	uiDisplayf("GOING TO BUILD COLUMN %d...", iColumnId);

	switch (ms_MenuArray.CVideoEditorMenuItems[iIndex].columnDataType)
	{
		case COL_DATA_TYPE_STANDARD:
		{
			switch (ms_MenuArray.CVideoEditorMenuItems[iIndex].columnType)
			{
				case COL_TYPE_LIST:
				{
					BeginAddingColumnData(iColumnId, ms_MenuArray.CVideoEditorMenuItems[iIndex].columnType);

					bool const c_isOnAddList = iIndex == FindMenuIndex("ADD_LIST");
					bool shouldAddSizeIndicator = c_isOnAddList;

					for (s32 iCount = 0; iCount < ms_MenuArray.CVideoEditorMenuItems[iIndex].Option.GetCount(); iCount++)
					{
						CVideoEditorMenuOption& thisItem = ms_MenuArray.CVideoEditorMenuItems[iIndex].Option[iCount];

						if ( (iColumnId == ms_iCurrentColumn && iCount == ms_iCurrentItem[ms_iCurrentColumn]) ||  // if this is the current highlighted index then build next column with this link
							 (iColumnId != ms_iCurrentColumn && iCount == 0) )
						{
							if( IsFileFilteringMenu( iIndex ) )
							{
								ExecuteDependentAction( iIndex, thisItem.DependentAction );

								// link menu id is dependent on if we have files or not!
								iNextLinkMenuId = GenerateMenuId( iIndex, thisItem.LinkMenuId );
							}
							else
							{
								iNextLinkMenuId = thisItem.LinkMenuId;
							}			
						}

						uiDisplayf("Building Menu %d:   Building item %d   ms_iCurrentItem: %d  CurrentColumn: %d", iIndex, iCount, ms_iCurrentItem[ms_iCurrentColumn], ms_iCurrentColumn);
						
						atHashString c_toggleHash = ms_MenuArray.CVideoEditorMenuItems[iIndex].Option[iCount].ToggleValue;
						atHashString c_uniqueId = ms_MenuArray.CVideoEditorMenuItems[iIndex].Option[iCount].UniqueId;
						atHashString TriggerAction = ms_MenuArray.CVideoEditorMenuItems[iIndex].Option[iCount].TriggerAction;

						eICON_TYPE iconType = ICON_TYPE_NONE;
						char cMethodName[MAX_CHARS_IN_SCALEFORM_METHOD_NAME];
						char cSecondString[256] = {0};  // enough for CMontageText::MAX_MONTAGE_TEXT but the string is also used for other uses

						sEditedTextProperties &editedText = CTextTemplate::GetEditedText();

						if (thisItem.Context == atStringHash("FORCED_TUTORIAL_NO_CLIPS"))  // if forced tutorial, we display help text at bottom of menu
						{
							if (CVideoEditorUi::GetProject() && CVideoEditorUi::GetProject()->GetClipCount() == 0)
							{
								safecpy(cMethodName, "ADD_COLUMN_HELP_TEXT", NELEM(cMethodName));
							}
							else
							{
								continue;
							}
						}
						else
						{
							safecpy(cMethodName, "ADD_COLUMN_ITEM", NELEM(cMethodName));

							if (c_toggleHash != 0)
							{
								safecpy(cSecondString, GetToggleString(c_toggleHash, c_uniqueId));
								safecpy(cMethodName, "ADD_COLUMN_ITEM_WITH_OPTIONS", NELEM(cMethodName));
							}

							if( c_isOnAddList )
							{
								if (TriggerAction == ATSTRINGHASH("TRIGGER_CHECK_FOR_CLIPS",0xe5c195a2))
								{
									safecpy(cMethodName, "ADD_COLUMN_ITEM_WITH_ICON", NELEM(cMethodName));
									iconType = ICON_TYPE_VIDEO;
								}

								if (TriggerAction == ATSTRINGHASH("TRIGGER_DISP_AMB_ICON",0x39b0526a))
								{
									safecpy(cMethodName, "ADD_COLUMN_ITEM_WITH_ICON", NELEM(cMethodName));
									iconType = ICON_TYPE_AMBIENT;
								}

								if (TriggerAction == ATSTRINGHASH("TRIGGER_DISP_AUD_ICON",0x3b542ac1))
								{
									safecpy(cMethodName, "ADD_COLUMN_ITEM_WITH_ICON", NELEM(cMethodName));
									iconType = ICON_TYPE_AUDIO;
								}

								if (TriggerAction == ATSTRINGHASH("TRIGGER_DISP_SCORE_ICON",0x81165d07))
								{
									safecpy(cMethodName, "ADD_COLUMN_ITEM_WITH_ICON", NELEM(cMethodName));
									iconType = ICON_TYPE_SCORE;
								}

								if (TriggerAction == ATSTRINGHASH("TRIGGER_NEW_TEXT",0x0434c447))
								{
									safecpy(cMethodName, "ADD_COLUMN_ITEM_WITH_ICON", NELEM(cMethodName));
									iconType = ICON_TYPE_TEXT;
								}

								if (CVideoEditorUi::HasProjectBeenEdited())  // display "unsaved" icon if project has not been saved
								{
									if (TriggerAction == ATSTRINGHASH("TRIGGER_SAVE",0x3cc31922))
									{
										safecpy(cMethodName, "ADD_COLUMN_ITEM_WITH_ICON", NELEM(cMethodName));
										iconType = ICON_TYPE_UNSAVED;
									}
								}
							}

							if (TriggerAction == ATSTRINGHASH("TRIGGER_ENTER_TEXT",0x82dec3f0))
							{
								if (editedText.m_text[0] != '\0')
								{
									static const int MAX_SECONDY_STR_LEN = 15;

									fwTextUtil::StrcpyWithCharacterAndByteLimits(cSecondString, editedText.m_text, MAX_SECONDY_STR_LEN+1, sizeof(cSecondString));

									if (fwTextUtil::GetCharacterCount(cSecondString) > MAX_SECONDY_STR_LEN)
									{
										int lastCharIndex = 0;
										for(int i = 0; i < MAX_SECONDY_STR_LEN; ++i)
										{
											lastCharIndex += fwTextUtil::ByteCountForCharacter(&cSecondString[lastCharIndex]);
											uiAssert(lastCharIndex < sizeof(cSecondString));
										}

										if(lastCharIndex < sizeof(cSecondString))
										{
											cSecondString[lastCharIndex] = '\0';  // cap it at 25 chars with ... on
											safecat(cSecondString, "...", NELEM(cSecondString));
											safecpy(cMethodName, "ADD_COLUMN_ITEM_WITH_TWO_STRINGS", NELEM(cMethodName));
										}
									}
								}
							}

							if (TriggerAction == ATSTRINGHASH("TRIGGER_RENAME_PROJECT",0xe06945d8))
							{
								if (IsProjectValid())
								{
									safecpy(cSecondString, CVideoEditorUi::GetProject()->GetName(), NELEM(cSecondString));
									safecpy(cMethodName, "ADD_COLUMN_ITEM_WITH_TWO_STRINGS", NELEM(cMethodName));
								}
							}
						}

						if (CScaleformMgr::BeginMethod(CVideoEditorUi::GetMovieId(), SF_BASE_CLASS_VIDEO_EDITOR, cMethodName))
						{
							char cFullString[255];

							if (thisItem.cTextId.IsNotNull())
							{
								safecpy(cFullString, TheText.Get(thisItem.cTextId.GetHash(),""), NELEM(cFullString));
							}
							else
							{
								cFullString[0] = '\0';
							}

							if (TriggerAction == ATSTRINGHASH("TRIGGER_ENTER_TEXT",0x82dec3f0))  // addition for 1945384
							{
								if (editedText.m_text[0] != '\0')
								{
									safecpy(cFullString, TheText.Get("VEUI_TEXT_STRING_EDIT"), NELEM(cFullString));
								}
							}

							if (TriggerAction == ATSTRINGHASH("WANT_TO_EXIT",0xeabb5f49))
							{
								if (CVideoEditorUi::HasUserBeenWarnedOfGameDestory() && !CPauseMenu::IsActive(PM_SkipVideoEditor))
								{
									safecpy(cFullString, TheText.Get("VEUI_EXIT_TO_SP"), NELEM(cFullString));
								}
							}

							CScaleformMgr::AddParamString(cFullString, false);

							if (cSecondString[0] != '\0')
							{
								CScaleformMgr::AddParamString(cSecondString, false);
							}
							else if (iconType != ICON_TYPE_NONE)
							{
								CScaleformMgr::AddParamInt((s32)iconType);
							}

							CScaleformMgr::EndMethod();
						}
					}

					if( shouldAddSizeIndicator )
					{
						AddSizeIndicator();
					}

					EndAddingColumnData(true, ms_iCurrentColumn == iColumnId);

					if( shouldAddSizeIndicator )
					{
						if( c_isOnAddList )
						{
							SetupSizeIndicator( "VEUI_PROJ_SIZE", CalculateActiveProjectSize() );
						}

						// TODO - DerekP - Drop in the setup code for the clip management version here

						// Action-script can't seem to position this, so we have to
						RepositionSizeIndicator();
					}

					//
					// grey out anything that is not selectable (apart from any tutorial help text)
					//
					for (s32 iCount = 0; iCount < ms_MenuArray.CVideoEditorMenuItems[iIndex].Option.GetCount(); iCount++)
					{
						CVideoEditorMenuOption& thisItem = ms_MenuArray.CVideoEditorMenuItems[iIndex].Option[iCount];

						if( !IsItemEnabled( iIndex, ms_iCurrentColumn, iCount ) && thisItem.Context != atStringHash("FORCED_TUTORIAL_NO_CLIPS") )
						{
							// grey out this menu item if we have no clips
							if (CScaleformMgr::BeginMethod(CVideoEditorUi::GetMovieId(), SF_BASE_CLASS_VIDEO_EDITOR, "SET_ITEMS_GREYED_OUT"))
							{
								CScaleformMgr::AddParamInt((s32)iCount);
								CScaleformMgr::EndMethod();
							}
						}
					}

					break;
				}

				case COL_TYPE_BASIC_PAGE:
				{
					// switch to alternative director mode message if user has been warned of game destroy
					bool const c_isOnDirectorMode = iIndex == FindMenuIndex("DIR_MODE_HELP");
					if (c_isOnDirectorMode && (!CTheScripts::GetIsDirectorModeAvailable() || CVideoEditorUi::HasUserBeenWarnedOfGameDestory()))
					{
						BuildMenu(FindMenuIndex("DIR_MODE_HELP_ALT"), bBuildBranches);
						return iColumnId;
					}


					bool const c_isOnExitToGame = iIndex == FindMenuIndex("EXIT_TO_GAME_HELP");
					if (c_isOnExitToGame && CVideoEditorUi::HasUserBeenWarnedOfGameDestory() && !CPauseMenu::IsActive(PM_SkipVideoEditor))
					{
						BuildMenu(FindMenuIndex("EXIT_TO_GAME_HELP_ALT"), bBuildBranches);
						return iColumnId;
					}


					BeginAddingColumnData(iColumnId, ms_MenuArray.CVideoEditorMenuItems[iIndex].columnType);

					if (CScaleformMgr::BeginMethod(CVideoEditorUi::GetMovieId(), SF_BASE_CLASS_VIDEO_EDITOR, "ADD_COLUMN_ITEM"))
					{
						CVideoEditorMenuBasicPage *pContentData = ms_MenuArray.CVideoEditorMenuItems[iIndex].m_pContent;

						if (pContentData)
						{
							CScaleformMgr::AddParamString(TheText.Get(pContentData->Header.GetHash(),""));
							CScaleformMgr::AddParamString(TheText.Get(pContentData->Body.GetHash(),""), false);  // pass raw text
						}

						CScaleformMgr::EndMethod();
					}

					EndAddingColumnData(true, ms_iCurrentColumn == iColumnId);

					break;
				}

				case COL_TYPE_LOAD_PROJ_INFO:
				{
					s32 const c_fileListColumn = COL_2;

					if( ms_PreviewData[c_fileListColumn].GetCount() != 0 )  // uses data based on index of the previous column
					{
						s32 currentItem = ms_iCurrentItem[c_fileListColumn];

						if( Verifyf( currentItem >= 0 && currentItem < ms_PreviewData[c_fileListColumn].GetCount(), "outside of PreviewData Array %d", currentItem ) )
						{
							CThumbnailFileView *pPopulatedThumbnailView = GetThumbnailViewForPreviewData( ms_previewDataId[c_fileListColumn] );

							if( pPopulatedThumbnailView )
							{
								static bool s_thumbnailWasLoadedLastFrame = false;

								s32 const c_thumbnailIndexForCurrentItem = GetFileViewIndexForItem( c_fileListColumn, currentItem );
                                CVideoEditorPreviewData const& previewData = ms_PreviewData[c_fileListColumn][currentItem];
                                                   
								bool const c_thumbnailIndexForCurrentItemValid = pPopulatedThumbnailView->isValidIndex( c_thumbnailIndexForCurrentItem );
								bool const c_thumbnailExists = c_thumbnailIndexForCurrentItemValid && pPopulatedThumbnailView->doesThumbnailExist(c_thumbnailIndexForCurrentItem);
								bool const c_thumbnailRequested = c_thumbnailExists && IsLoadingThumbnail();
								bool const c_newThumbnailNeeded = ms_iClipThumbnailInUse == -1 || ( ms_iClipThumbnailInUse != c_thumbnailIndexForCurrentItem ) || !c_thumbnailRequested;
								bool const c_isCurrentThumbnailValidFile = ms_iClipThumbnailInUse != -1 && pPopulatedThumbnailView->isValidIndex( ms_iClipThumbnailInUse );
								bool const c_thumbnailLoadFailed = c_thumbnailRequested && !c_newThumbnailNeeded && c_isCurrentThumbnailValidFile && 
									pPopulatedThumbnailView->hasThumbnailLoadFailed( ms_iClipThumbnailInUse );
								bool const c_thumbnailLoaded = c_thumbnailRequested && !c_newThumbnailNeeded && c_isCurrentThumbnailValidFile && !c_thumbnailLoadFailed && 
									pPopulatedThumbnailView->isThumbnailLoaded( ms_iClipThumbnailInUse );

								bool const c_allowRebuild = (c_newThumbnailNeeded || c_thumbnailLoaded != s_thumbnailWasLoadedLastFrame || ms_forceUpdatingUpload );

								if( c_allowRebuild && 
									c_thumbnailIndexForCurrentItem < pPopulatedThumbnailView->getFilteredFileCount() && c_thumbnailIndexForCurrentItemValid )
								{
									
                                    bool const c_canRequestThumbnail = previewData.BelongsToCurrentUser();

									char cTempString[32];

									float const c_fileSize = pPopulatedThumbnailView->getFileSize( c_thumbnailIndexForCurrentItem ) / 1024.f;

									if (c_fileSize < 1024.f)
									{
										formatf(cTempString, "%0.2fk", c_fileSize, NELEM(cTempString));
									}
									else
									{
										formatf(cTempString, "%0.2fmb", c_fileSize / 1024.f, NELEM(cTempString));
									}

									// Request thumbnail if needed
                                    if( c_canRequestThumbnail )
                                    {
                                        if( c_newThumbnailNeeded )
                                        {
                                            ReleaseThumbnailOfItem();
                                            RequestThumbnailOfItem( c_fileListColumn, currentItem );

                                            s_thumbnailWasLoadedLastFrame = false;
                                        }
                                        else
                                        {
                                            s_thumbnailWasLoadedLastFrame = c_thumbnailLoaded;
                                        }
                                    }
                                    else
                                    {
                                        ReleaseThumbnailOfItem();
                                        s_thumbnailWasLoadedLastFrame = true;
                                    }

									char const * const c_txdName = c_thumbnailLoaded && c_canRequestThumbnail ? previewData.m_thumbnailTxdName.c_str() : NULL;
									char const * const c_textureName = c_thumbnailLoaded && c_canRequestThumbnail ? previewData.m_thumbnailTextureName.c_str() : NULL;

									BeginAddingColumnData(iColumnId, ms_MenuArray.CVideoEditorMenuItems[iIndex].columnType);

									if (CScaleformMgr::BeginMethod(CVideoEditorUi::GetMovieId(), SF_BASE_CLASS_VIDEO_EDITOR, "ADD_COLUMN_ITEM"))
									{
										CScaleformMgr::AddParamString( c_txdName, false );
										CScaleformMgr::AddParamString( c_textureName, false );

										CScaleformMgr::AddParamString( TheText.Get("VEUI_NAME") );
										CScaleformMgr::AddParamString( previewData.m_title );

										CScaleformMgr::AddParamString( TheText.Get("VEUI_CREATED") );
										CScaleformMgr::AddParamString( previewData.m_date );

										CScaleformMgr::AddParamString( TheText.Get("VEUI_SIZE") );
										CScaleformMgr::AddParamString( cTempString );

										CScaleformMgr::AddParamString( TheText.Get("VEUI_LENGTH") );
										CTextConversion::FormatMsTimeAsString( cTempString, NELEM(cTempString), previewData.m_durationMs );
										CScaleformMgr::AddParamString(cTempString);

										// if there are no upload values, check for warnings
										if( !SetUploadValues() )
										{
											DisplayClipRestrictions( c_fileListColumn, currentItem );
										}

										CScaleformMgr::EndMethod();
									}

									EndAddingColumnData(true, ms_iCurrentColumn == iColumnId);

									if (CScaleformMgr::BeginMethod(CVideoEditorUi::GetMovieId(), SF_BASE_CLASS_VIDEO_EDITOR, "UPDATE_UPLOAD_BACKGROUND"))
									{
										CScaleformMgr::AddParamInt(1);	 
										CScaleformMgr::EndMethod();
									}		
								}

                                if( !c_thumbnailExists || c_thumbnailLoadFailed || c_thumbnailLoaded )
                                {
                                    CVideoEditorUi::SetActiveState();
                                }

								CVideoEditorInstructionalButtons::Refresh();
							}
						}
					}
					else
					{
						// no files found, give up but display a message
						BeginAddingColumnData(iColumnId, ms_MenuArray.CVideoEditorMenuItems[iIndex].columnType);

						if (CScaleformMgr::BeginMethod(CVideoEditorUi::GetMovieId(), SF_BASE_CLASS_VIDEO_EDITOR, "ADD_COLUMN_ITEM"))
						{
							CScaleformMgr::AddParamString(NULL);
							CScaleformMgr::AddParamString(NULL);
							CScaleformMgr::AddParamString( TheText.Get( "VEUI_NOFILES" ) );
							CScaleformMgr::AddParamString(NULL);
							CScaleformMgr::AddParamString(NULL);
							CScaleformMgr::AddParamString(NULL);
							CScaleformMgr::AddParamString(NULL);

							CScaleformMgr::EndMethod();
						}

						EndAddingColumnData(true, ms_iCurrentColumn == iColumnId);

						CVideoEditorUi::SetActiveState();
					}

					break;
				}

				case COL_TYPE_TEXT_PLACEMENT:
				{
					BeginAddingColumnData(iColumnId, ms_MenuArray.CVideoEditorMenuItems[iIndex].columnType);

					if (CScaleformMgr::BeginMethod(CVideoEditorUi::GetMovieId(), SF_BASE_CLASS_VIDEO_EDITOR, "ADD_COLUMN_ITEM"))
					{
						s32 const c_videoIndexUsedInText = CVideoEditorTimeline::GetProjectIndexAtTime(PLAYHEAD_TYPE_VIDEO, CVideoEditorTimeline::GetPlayheadPositionMs());

						if (c_videoIndexUsedInText != -1)
						{
							atString textureNameString;

							if (CVideoEditorUi::GetProject() && CVideoEditorUi::GetProject()->IsClipThumbnailLoaded((u32)c_videoIndexUsedInText))
							{
								CVideoEditorUi::GetProject()->getClipTextureName((u32)c_videoIndexUsedInText, textureNameString);

								CScaleformMgr::AddParamString(textureNameString.c_str(), false);
								CScaleformMgr::AddParamString(textureNameString.c_str(), false);
							}
						}
						else
						{
							CScaleformMgr::AddParamString("\0", false);
							CScaleformMgr::AddParamString("\0", false);
						}

						CScaleformMgr::EndMethod();
					}

					EndAddingColumnData(true, ms_iCurrentColumn == iColumnId);

					break;
				}

				default:
				{
					uiAssertf(0, "CVideoEditorMenu::BuildMenu() Invalid columnType %d", (s32)ms_MenuArray.CVideoEditorMenuItems[iIndex].columnType);
				}

			}

			break;
		}

		case COL_DATA_TYPE_LOAD_FILES:
		{
			if (bBuildBranches)
			{
				CheckForDependentData(iIndex, 0, iColumnId);
			}

			BeginAddingColumnData(iColumnId, ms_MenuArray.CVideoEditorMenuItems[iIndex].columnType);

			if ( ms_MenuArray.CVideoEditorMenuItems[iIndex].Option.GetCount() != 0)
			{
				iNextLinkMenuId = ms_MenuArray.CVideoEditorMenuItems[iIndex].Option[0].LinkMenuId;

				s32 const c_totalFileCount = ms_PreviewData[iColumnId].GetCount();

				u64 const c_currentUserId = CVideoEditorInterface::GetActiveUserId();

				for (s32 iCount = ms_iCurrentScrollOffset[iColumnId]; iCount < ms_iCurrentScrollOffset[iColumnId] + MAX_ITEMS_IN_COLUMN && iCount < ms_PreviewData[iColumnId].GetCount(); iCount++)
				{
					if (ms_PreviewData[iColumnId][iCount].m_ownerId != c_currentUserId)
					{
						if (CScaleformMgr::BeginMethod(CVideoEditorUi::GetMovieId(), SF_BASE_CLASS_VIDEO_EDITOR, "ADD_COLUMN_ITEM_WITH_ICON"))
						{
							CScaleformMgr::AddParamString(ms_PreviewData[iColumnId][iCount].m_title, false);
							CScaleformMgr::AddParamInt((s32)ICON_TYPE_OTHER_USER);
							CScaleformMgr::EndMethod();
						}
					}
					else if (ms_PreviewData[iColumnId][iCount].m_usedInProject)
					{
						if (CScaleformMgr::BeginMethod(CVideoEditorUi::GetMovieId(), SF_BASE_CLASS_VIDEO_EDITOR, "ADD_COLUMN_ITEM_WITH_ICON"))
						{
							CScaleformMgr::AddParamString(ms_PreviewData[iColumnId][iCount].m_title, false);
							CScaleformMgr::AddParamInt((s32)ICON_TYPE_LINKED); // 1907434 - link icon is '4'
							CScaleformMgr::EndMethod();
						}
					}
					else if (ms_PreviewData[iColumnId][iCount].m_trafficLight > 0)
					{
						if (CScaleformMgr::BeginMethod(CVideoEditorUi::GetMovieId(), SF_BASE_CLASS_VIDEO_EDITOR, "ADD_COLUMN_ITEM_WITH_ICON"))
						{
							CScaleformMgr::AddParamString(ms_PreviewData[iColumnId][iCount].m_title, false);
							CScaleformMgr::AddParamInt(static_cast<s32>(ICON_TYPE_REDLIGHT + ms_PreviewData[iColumnId][iCount].m_trafficLight - 1));
							CScaleformMgr::EndMethod();
						}
					}
					else
					{
						if (CScaleformMgr::BeginMethod(CVideoEditorUi::GetMovieId(), SF_BASE_CLASS_VIDEO_EDITOR, "ADD_COLUMN_ITEM"))
						{
							CScaleformMgr::AddParamString(ms_PreviewData[iColumnId][iCount].m_title, false);
							CScaleformMgr::EndMethod();
						}
					}

				}

				if( c_totalFileCount > MAX_ITEMS_IN_COLUMN &&
					CScaleformMgr::BeginMethod(CVideoEditorUi::GetMovieId(), SF_BASE_CLASS_VIDEO_EDITOR, "ADD_LIST_SCROLL_ITEM"))
				{
					SimpleString_32 footerText;
					GenerateListFooterText( footerText, iColumnId );

					CScaleformMgr::AddParamString( footerText.getBuffer(), false );
					CScaleformMgr::EndMethod();
				}
			}

			EndAddingColumnData(true, ms_iCurrentColumn == iColumnId);

			for( s32 iCount = ms_iCurrentScrollOffset[iColumnId]; iCount < ms_iCurrentScrollOffset[iColumnId] + MAX_ITEMS_IN_COLUMN && iCount < ms_PreviewData[iColumnId].GetCount(); iCount++ )
			{
				UpdateItemColouration( iColumnId,  iCount - ms_iCurrentScrollOffset[iColumnId], iCount );
			}

			break;
		}

		case COL_DATA_TYPE_AUDIO_LIST:
		{
			if (bBuildBranches)
			{
				CheckForDependentData(iIndex, 0, iColumnId);
			}

			BeginAddingColumnData(iColumnId, ms_MenuArray.CVideoEditorMenuItems[iIndex].columnType);

			if (ms_MenuArray.CVideoEditorMenuItems[iIndex].Option.GetCount() != 0)
			{
				iNextLinkMenuId = ms_MenuArray.CVideoEditorMenuItems[iIndex].Option[0].LinkMenuId;

				s32 const c_totalFileCount = ms_PreviewData[iColumnId].GetCount();

				for (s32 iCount = ms_iCurrentScrollOffset[iColumnId]; iCount < ms_iCurrentScrollOffset[iColumnId] + MAX_ITEMS_IN_COLUMN && iCount < ms_PreviewData[iColumnId].GetCount(); iCount++)
				{
                    CVideoEditorPreviewData const& previewData = ms_PreviewData[iColumnId][iCount];

					if (CScaleformMgr::BeginMethod(CVideoEditorUi::GetMovieId(), SF_BASE_CLASS_VIDEO_EDITOR, "ADD_COLUMN_ITEM"))
					{
						CScaleformMgr::AddParamString( previewData.m_title, false );

						if( iColumnId == COL_2)  
						{
							char cTempString[32];
							CTextConversion::FormatMsTimeAsString(cTempString, NELEM(cTempString), previewData.m_durationMs);

							CScaleformMgr::AddParamString(cTempString, false);
						}

						CScaleformMgr::EndMethod();
					}
				}

				if( c_totalFileCount > MAX_ITEMS_IN_COLUMN &&
					CScaleformMgr::BeginMethod(CVideoEditorUi::GetMovieId(), SF_BASE_CLASS_VIDEO_EDITOR, "ADD_LIST_SCROLL_ITEM"))
				{
					SimpleString_32 footerText;
					GenerateListFooterText( footerText, iColumnId );

					CScaleformMgr::AddParamString( footerText.getBuffer(), false );
					CScaleformMgr::EndMethod();
				}
			}

			EndAddingColumnData(true, true);  // always want this column to be in a usable (active) state

			CVideoEditorInstructionalButtons::Refresh();

			if (ms_iCurrentColumn != iColumnId)  // but ensure actionscript is in sync with what column code considers active
			{
				UnHighlight();
				SetColumnActive(ms_iCurrentColumn);  // set back to the current column
			}

			// Column 3 always needs to act like it's column 2, because reasons.
			ms_iMenuIdForColumn[ COL_3 ] = ms_iMenuIdForColumn[ COL_2 ];

			break;
		}

		default:
		{
			uiAssertf(0, "CVideoEditorMenu::BuildMenu() Invalid columnDataType %d", (s32)ms_MenuArray.CVideoEditorMenuItems[iIndex].columnDataType);
		}
	}

	ms_iMenuIdForColumn[ iColumnId ] = iIndex;

	//
	// now we need to try and build the next link
	//
	if ( bBuildBranches )  // we think we have a link...
	{
		if( iNextLinkMenuId != -1 )
		{
			s32 iLinkMenuId = FindMenuIndex(iNextLinkMenuId);

			if (iLinkMenuId != -1)
			{

				uiDisplayf("Building sub-Menu %d", iLinkMenuId);
				iColumnId = BuildMenu(iLinkMenuId, true);
			}
		}
		else
		{
			RemoveColumn( iColumnId + 1 );
		}
	}

	return iColumnId;
}


bool CVideoEditorMenu::DisplayClipRestrictions( s32 const iColumn, s32 const iIndex )
{
	bool success = false;

	if ( IsOnClipFilteringMenu() || IsOnAddClipMenu() || IsOnClipManagementMenu() )
	{
		CRawClipFileView *pClipFileView = CVideoEditorUi::GetClipFileView();
		if (pClipFileView)
		{
            FileViewIndex const c_fileViewIndex = GetFileViewIndexForItem( iColumn, iIndex );
            u64 const c_ownerId = pClipFileView->getOwnerId( c_fileViewIndex );
            bool const c_belongsToCurrentUser = c_ownerId == CVideoEditorInterface::GetActiveUserId();
			IReplayMarkerStorage::eEditRestrictionReason restrictions = pClipFileView->getClipRestrictions( c_fileViewIndex );
			
            if( c_belongsToCurrentUser )
            {
                if (restrictions == IReplayMarkerStorage::EDIT_RESTRICTION_FIRST_PERSON)
                {
                    CScaleformMgr::AddParamString(TheText.Get("VEUI_WARN_DIS_FPCAM"));
                    success = true;
                }
                else if (restrictions == IReplayMarkerStorage::EDIT_RESTRICTION_CUTSCENE)
                {
                    CScaleformMgr::AddParamString(TheText.Get("VEUI_WARN_DIS_CUTSCENE"));
                    success = true;
                }
                else if (restrictions ==IReplayMarkerStorage::EDIT_RESTRICTION_CAMERA_BLOCKED)
                {
                    CScaleformMgr::AddParamString(TheText.Get("VEUI_WARN_DIS_CAMBLOCK"));
                    success = true;
                }
            }
		}
	}

	return success;
}

bool CVideoEditorMenu::SetUploadValues()
{
	bool success = false;

	ms_isUpdatingUpload = false;
	// B*2367477 - need to check to see if gallery filter is loading now for this to work on entry to a filter
	if ((IsOnGalleryMenu() || IsOnGalleryFilteringMenu()) && VideoUploadManager::GetInstance().IsUploadEnabled())
	{
		CVideoFileView* videoView = CVideoEditorUi::GetVideoFileView();
		if( videoView && videoView->getFilteredFileCount() > 0 )
		{
			u32 const c_targetColumn = COL_2;

			CScaleformMgr::AddParamString(TheText.Get("VEUI_STATUS"));

			char dateBuffer[RAGE_MAX_PATH];
			videoView->getFileDateString( ms_iCurrentItem[ c_targetColumn ], dateBuffer);

			CVideoUploadManager::UploadProgressState::Enum progressState = CVideoUploadManager::UploadProgressState::NotUploaded;
#if USE_ORBIS_UPLOAD
			s64 contentId = videoView->getVideoContentId(ms_iCurrentItem[c_targetColumn]);
			progressState = VideoUploadManager::GetInstance().GetProgressState(contentId, videoView->getVideoDurationMs(ms_iCurrentItem[c_targetColumn]), videoView->getFileSize(ms_iCurrentItem[c_targetColumn]) );
#else
			char pathBuffer[RAGE_MAX_PATH];
			videoView->getFilePath( ms_iCurrentItem[c_targetColumn], pathBuffer );
			progressState = VideoUploadManager::GetInstance().GetProgressState(pathBuffer, videoView->getVideoDurationMs(ms_iCurrentItem[c_targetColumn]), videoView->getFileSize(ms_iCurrentItem[c_targetColumn]) );
#endif

			switch( progressState )
			{
			case CVideoUploadManager::UploadProgressState::Uploading:
				{
#if USE_ORBIS_UPLOAD
					CScaleformMgr::AddParamString(TheText.Get("YT_UPLOAD_UPLOADING"));
#else
					const u8 maxStringLength = 64;
					char labelString[maxStringLength];

					u32 percentage = static_cast<u32>(VideoUploadManager::GetInstance().GetUploadFileProgress() * 100.0f);
					if (percentage == 0)
						formatf(labelString, maxStringLength, "%s", TheText.Get("YT_UPLOAD_UPLOADING"));
					else
						formatf(labelString, maxStringLength, "%s %d%%", TheText.Get("YT_UPLOAD_UPLOADING"), percentage);

					CScaleformMgr::AddParamString(labelString);
#endif
					CScaleformMgr::AddParamInt(1);

					ms_isUpdatingUpload = true;
				}
				break;
			case CVideoUploadManager::UploadProgressState::Processing:
				{
					CScaleformMgr::AddParamString(TheText.Get("YT_UPLOAD_PROCESSING"));
					CScaleformMgr::AddParamInt(1);
					ms_isUpdatingUpload = true;

					if (ms_isHighlightedVideoUploading)
						CVideoEditorInstructionalButtons::Refresh();
				}
				break;
			case CVideoUploadManager::UploadProgressState::NotUploaded:
				{
					CScaleformMgr::AddParamString(TheText.Get("YT_UPLOAD_NOT_UPLOADED"));
					CScaleformMgr::AddParamInt(2);
					ms_isUpdatingUpload = true;
				}
				break;
			case CVideoUploadManager::UploadProgressState::Uploaded:
				{
					CScaleformMgr::AddParamString(TheText.Get("YT_UPLOAD_UPLOADED"));
					CScaleformMgr::AddParamInt(0);
					ms_isUpdatingUpload = true;
				}
				break;

			default : break;
			}

			// set after cases, as this is used in there
			ms_isHighlightedVideoUploading = (progressState == CVideoUploadManager::UploadProgressState::Uploading);
			success = true;
		}
	}

	return success;
}

void CVideoEditorMenu::UpdateUploadValues()
{
	// updates either when video is uploading, to update percentage
	// or when forced to refresh when an operation is performed
	if (ms_isUpdatingUpload || ms_forceUpdatingUpload)
	{
		// is this is true, we can expect to be online
		// don't refresh until we are
		if (ms_isUpdatingUpload && !VideoUploadManager::GetInstance().IsUploadEnabled())
			return;

		bool c_IsGalleryMenu = IsOnGalleryMenu();
		bool c_IsGalleryFiltMenu = c_IsGalleryMenu ? false : IsOnGalleryFilteringMenu();
		bool c_IsAnyGalleryMenu = (c_IsGalleryMenu || c_IsGalleryFiltMenu);

		// Only update the UI portion if it's valid
		s32 const c_currentMenuId = GetCurrentMenuId();
		if( c_currentMenuId >= 0 && ms_forceUpdatingUpload && c_IsAnyGalleryMenu )
		{
			FillInGalleryData(COL_2);
			
			if ( c_IsGalleryMenu )
			{
				// rebuild just parts of the menu, so the thumbnail doesn't refresh (for ages)
				RebuildColumn();
				UpdatePane();
				CVideoEditorInstructionalButtons::Refresh();
			}
			else
			{
				// need to be more forceful with filter menu, need to refresh main gallery, and then jump back to filter menu again
 				JumpPane();
 				GoBackOnePane();
			}
		}

		ms_isUpdatingUpload = false;
		ms_forceUpdatingUpload = false;
		if (c_IsAnyGalleryMenu && VideoUploadManager::GetInstance().IsUploadEnabled())
		{
			CVideoFileView* videoView = CVideoEditorUi::GetVideoFileView();
			if( videoView && videoView->getFilteredFileCount() > 0 )
			{
				u32 const c_targetColumn = COL_2;

				const u8 maxStringLength = 64;
				char labelString[maxStringLength];

				s32 colourValue = -1;

				char dateBuffer[RAGE_MAX_PATH];
				videoView->getFileDateString(ms_iCurrentItem[c_targetColumn], dateBuffer);

				CVideoUploadManager::UploadProgressState::Enum progressState = CVideoUploadManager::UploadProgressState::NotUploaded;
#if USE_ORBIS_UPLOAD
				s64 contentId = videoView->getVideoContentId(ms_iCurrentItem[c_targetColumn]);
				progressState = VideoUploadManager::GetInstance().GetProgressState(contentId, videoView->getVideoDurationMs(ms_iCurrentItem[c_targetColumn]), videoView->getFileSize(ms_iCurrentItem[c_targetColumn]) );
#else
				char pathBuffer[RAGE_MAX_PATH];
				videoView->getFilePath( ms_iCurrentItem[c_targetColumn], pathBuffer );
				progressState = VideoUploadManager::GetInstance().GetProgressState(pathBuffer, videoView->getVideoDurationMs(ms_iCurrentItem[c_targetColumn]), videoView->getFileSize(ms_iCurrentItem[c_targetColumn]) );
#endif
				switch( progressState )
				{
				case CVideoUploadManager::UploadProgressState::Uploading:
					{
#if USE_ORBIS_UPLOAD
						formatf(labelString, maxStringLength, "%s", TheText.Get("YT_UPLOAD_UPLOADING"));
#else
						u32 percentage = static_cast<u32>(VideoUploadManager::GetInstance().GetUploadFileProgress() * 100.0f);
						if (percentage == 0)
							formatf(labelString, maxStringLength, "%s", TheText.Get("YT_UPLOAD_UPLOADING"));
						else
							formatf(labelString, maxStringLength, "%s %d%%", TheText.Get("YT_UPLOAD_UPLOADING"), percentage);
#endif
						colourValue = 1;
						ms_isUpdatingUpload = true;
					}
					break;
				case CVideoUploadManager::UploadProgressState::Processing:
					{
						formatf(labelString, maxStringLength, "%s", TheText.Get("YT_UPLOAD_PROCESSING"));
						colourValue = 1;
						ms_isUpdatingUpload = true;
						if (ms_isHighlightedVideoUploading)
							CVideoEditorInstructionalButtons::Refresh();
					}
					break;
				case CVideoUploadManager::UploadProgressState::NotUploaded:
					{
						formatf(labelString, maxStringLength, "%s", TheText.Get("YT_UPLOAD_NOT_UPLOADED"));
						colourValue = 2;
					}
					break;
				case CVideoUploadManager::UploadProgressState::Uploaded:
					{
						formatf(labelString, maxStringLength, "%s", TheText.Get("YT_UPLOAD_UPLOADED"));
						colourValue = 0;
						CVideoEditorInstructionalButtons::Refresh();
					}
					break;

				default : return; // bail. don't set anything
				}

				// set after cases, as this is used in there
				ms_isHighlightedVideoUploading = (progressState == CVideoUploadManager::UploadProgressState::Uploading);

				if (colourValue >= 0) // but let's check the colour value to be on the safe side
				{
					if (CScaleformMgr::BeginMethod(CVideoEditorUi::GetMovieId(), SF_BASE_CLASS_VIDEO_EDITOR, "UPDATE_UPLOAD_PROCESS"))
					{
						CScaleformMgr::AddParamString(labelString);

						CScaleformMgr::EndMethod();
					}

					if (CScaleformMgr::BeginMethod(CVideoEditorUi::GetMovieId(), SF_BASE_CLASS_VIDEO_EDITOR, "UPDATE_UPLOAD_BACKGROUND"))
					{
						CScaleformMgr::AddParamInt(colourValue);

						CScaleformMgr::EndMethod();
					}
				}
			}
		}


	}
}

/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorMenu::RebuildMenu
// PURPOSE: rebuild the menu from scratch
/////////////////////////////////////////////////////////////////////////////////////////
void CVideoEditorMenu::RebuildMenu()
{
	BuildMenu(GetCurrentMenuId(), true);
	SetItemSelected(ms_iCurrentItem[ms_iCurrentColumn] - ms_iCurrentScrollOffset[ms_iCurrentColumn]);
}

void CVideoEditorMenu::RebuildColumn(s32 const c_column)
{
	BuildMenu(GetCurrentMenuId(), false, c_column);  // only rebuild this column

	if (c_column != -1)
	{
		SetItemSelected(ms_iCurrentItem[c_column] - ms_iCurrentScrollOffset[c_column]);
	}
	else
	{
		SetItemSelected(ms_iCurrentItem[ms_iCurrentColumn] - ms_iCurrentScrollOffset[ms_iCurrentColumn]);
	}
}


/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorMenu::IsItemSelectable
// PURPOSE: Some menu items are not selectable.  We return true if the item is selectable
//			false if we cannot select this item under the current conditions
/////////////////////////////////////////////////////////////////////////////////////////
bool CVideoEditorMenu::IsItemSelectable( s32 const iMenuId,  s32 const iColumn, s32 const iIndex )
{
	bool result = false;

	if( iIndex >= 0 && iMenuId >= 0 && iMenuId < ms_MenuArray.CVideoEditorMenuItems.GetCount() )
	{
		if( IsFileFilteringMenu( iMenuId ) )
		{
			CFileViewFilter const* fileView = GetFileViewFilterForMenu( iMenuId );
			size_t const c_fileCount = fileView ? fileView->getFilteredFileCount() : 0;
			result = c_fileCount > 0;
		}
		else if( IsFileMenu( iMenuId ) )
		{
            CFileViewFilter const* fileView = GetFileViewFilterForMenu( iMenuId );
            size_t const c_fileCount = fileView ? fileView->getFilteredFileCount() : 0;
            result = iIndex >= 0 && iIndex < c_fileCount;
		}
		else if( iColumn == 0 )
		{
			// non standard items are always selectable
			if( ms_MenuArray.CVideoEditorMenuItems[iMenuId].columnDataType != COL_DATA_TYPE_STANDARD)
			{
				result = true;
			}
			else
			{
				if (ms_MenuArray.CVideoEditorMenuItems[iMenuId].Option[iIndex].Context == atStringHash("ONLY_VALID_TEMPLATE"))
				{
					return CTextTemplate::IsPropertyValidInTemplate(ms_MenuArray.CVideoEditorMenuItems[iMenuId].Option[iIndex].UniqueId);
				}
				// only selectable when we have clips in the project:
				else if( ms_MenuArray.CVideoEditorMenuItems[iMenuId].Option[iIndex].Context == atStringHash("ONLY_SELECTABLE_TEXT_ADDED") )
				{
					if (ms_MenuArray.CVideoEditorMenuItems[iMenuId].Option[iIndex].ToggleValue == atStringHash("TOGGLE_TEXT_FONT"))
					{
						if (CText::IsAsianLanguage() || CPauseMenu::GetMenuPreference(PREF_CURRENT_LANGUAGE) == LANGUAGE_RUSSIAN)
						{
							return false;
						}
					}

					result = (CTextTemplate::GetEditedText().m_text[0] != '\0');
				}
				// only selectable when we have clips in the project:
				else if( ms_MenuArray.CVideoEditorMenuItems[iMenuId].Option[iIndex].Context == atStringHash("ONLY_SELECTABLE_WITH_CLIPS") )
				{
					CVideoEditorProject const* currentProject = CVideoEditorUi::GetProject();
					result = ( currentProject && currentProject->GetClipCount() > 0 );
				}
				// only selectable when we have clips in the project:
				else if( ms_MenuArray.CVideoEditorMenuItems[iMenuId].Option[iIndex].Context == atStringHash("ONLY_SELECTABLE_IN_SESSION") )
				{
					result = ( !CVideoEditorUi::HasUserBeenWarnedOfGameDestory() );
				}
				// only selectable when director mode is available and we haven't unloaded the game world:
				else if( ms_MenuArray.CVideoEditorMenuItems[iMenuId].Option[iIndex].Context == atStringHash("ONLY_SELECTABLE_DIR_MODE_AVAILABLE") )
				{
					result = ( CTheScripts::GetIsDirectorModeAvailable() && !CVideoEditorUi::HasUserBeenWarnedOfGameDestory() );
				}
				// otherwise anything that can be highlighted is selectable
				else 
				{
					result = CanHighlightItem( iMenuId, iColumn, iIndex );
				}
			}
		}
		else
		{
			result = CanHighlightItem( iMenuId, iColumn, iIndex );
		}
	}

	return result;
}


bool CVideoEditorMenu::IsItemEnabled( s32 const iMenuId,  s32 const iColumn, s32 const iIndex )
{
	bool result = false;

	if( iIndex >= 0 && iMenuId >= 0 && iMenuId < ms_MenuArray.CVideoEditorMenuItems.GetCount() )
	{
		//! Items are always enabled for the filtering menu, otherwise use the selected state
		if( IsFileFilteringMenu( iMenuId ) )
		{
			result = true; 
		}
		else
		{
			result = IsItemSelectable( iMenuId, iColumn, iIndex );
		}
	}

	return result;
}


/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorMenu::CanItemBeActioned
// PURPOSE: can this item be actioned with the ACCEPT button?
/////////////////////////////////////////////////////////////////////////////////////////
bool CVideoEditorMenu::CanItemBeActioned( s32 const iMenuId,  s32 const iColumn, s32 const iIndex )
{
	// if the item isn't selectable, it shouldn't be actionable.
	if( !IsItemSelectable(iMenuId, iColumn, iIndex) )
		return false;
	
	bool result = false;

	if( iMenuId >= 0 && iMenuId < ms_MenuArray.CVideoEditorMenuItems.GetCount() )
	{
		if( IsFileFilteringMenu( iMenuId ) )
		{
			CFileViewFilter const* fileView = GetFileViewFilterForMenu( iMenuId );
			size_t const c_fileCount = fileView ? fileView->getFilteredFileCount() : 0;
			result = c_fileCount > 0;
		}
		else if( IsFileMenu( iMenuId ) )
		{
            if( CanActionFile( iColumn, iIndex ) )
            {
                CFileViewFilter const* fileView = GetFileViewFilterForMenu( iMenuId );
                size_t const c_fileCount = fileView ? fileView->getFilteredFileCount() : 0;
                result = iIndex >= 0 && iIndex < c_fileCount;
            }
		}
		else if( iColumn == 0 )
		{
			// non standard items are always selectable
			if( ms_MenuArray.CVideoEditorMenuItems[iMenuId].columnDataType != COL_DATA_TYPE_STANDARD)
			{
				result = true;
			}
			else if (iIndex < ms_MenuArray.CVideoEditorMenuItems[iMenuId].Option.GetCount())
			{
				CVideoEditorMenuOption const& menuOption = ms_MenuArray.CVideoEditorMenuItems[iMenuId].Option[iIndex];

				result = (	menuOption.ToggleValue != ATSTRINGHASH("TOGGLE_TEXT_COLOR",0xa88888ea) &&
							menuOption.ToggleValue != ATSTRINGHASH("TOGGLE_TEXT_FONT",0x721248b3) &&
							menuOption.ToggleValue != ATSTRINGHASH("TOGGLE_TEXT_OPACITY",0x10bdaa2c) &&
							menuOption.ToggleValue != ATSTRINGHASH("TOGGLE_TEXT_TEMPLATE",0xa70897f7) &&
							menuOption.ToggleValue != ATSTRINGHASH("TOGGLE_TEXT_DURATION",0xb2c85bb6));
			}		
		}
		else
		{
			result = CanHighlightItem(iMenuId, iColumn, iIndex);
		}
	}

	return result;
}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorMenu::CanHighlightItem
// PURPOSE: can the item be highlighted?
/////////////////////////////////////////////////////////////////////////////////////////
bool CVideoEditorMenu::CanHighlightItem( s32 const iMenuId,  s32 const iColumn, s32 const iIndex )
{
	if( iColumn == 0 )
	{
		if (iMenuId < ms_MenuArray.CVideoEditorMenuItems.GetCount())
		{
			// non standard items
			if( ms_MenuArray.CVideoEditorMenuItems[iMenuId].columnDataType != COL_DATA_TYPE_STANDARD)
			{
				return true;
			}

			if (iIndex < ms_MenuArray.CVideoEditorMenuItems[iMenuId].Option.GetCount())
			{
				if (ms_MenuArray.CVideoEditorMenuItems[iMenuId].Option[iIndex].Context == atStringHash("ONLY_VALID_TEMPLATE"))
				{
					return CTextTemplate::IsPropertyValidInTemplate(ms_MenuArray.CVideoEditorMenuItems[iMenuId].Option[iIndex].UniqueId);
				}

				// do not allow the text properties to be highlighted if we have no text
				if( ms_MenuArray.CVideoEditorMenuItems[iMenuId].Option[iIndex].Context == atStringHash("ONLY_SELECTABLE_TEXT_ADDED"))
				{
					if (ms_MenuArray.CVideoEditorMenuItems[iMenuId].Option[iIndex].ToggleValue == atStringHash("TOGGLE_TEXT_FONT"))
					{
						if (CText::IsAsianLanguage() || CPauseMenu::GetMenuPreference(PREF_CURRENT_LANGUAGE) == LANGUAGE_RUSSIAN)
						{
							return false;
						}
					}

					if (CTextTemplate::GetEditedText().m_text[0] == '\0')
					{
						return false;
					}
				}

				// forced tutorial message is never selectable:
				if( ms_MenuArray.CVideoEditorMenuItems[iMenuId].Option[iIndex].Context == atStringHash("FORCED_TUTORIAL_NO_CLIPS") )
				{
					return false;
				}
			}
		}
	}

	return true;
}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorMenu::FindMenuIndex
// PURPOSE: Finds the array index of the passed hash menu id from the xml
/////////////////////////////////////////////////////////////////////////////////////////
s32 CVideoEditorMenu::FindMenuIndex(const atHashString theMenuId)
{
	for (s32 iCount = 0; iCount < ms_MenuArray.CVideoEditorMenuItems.GetCount(); iCount++)
	{
		if (theMenuId == ms_MenuArray.CVideoEditorMenuItems[iCount].MenuId)
		{
			return iCount;
		}
	}
	
	return -1;  // no index found
}

s32 CVideoEditorMenu::FindMenuIndex( const u32 theMenuId )
{
	for (s32 iCount = 0; iCount < ms_MenuArray.CVideoEditorMenuItems.GetCount(); iCount++)
	{
		if (theMenuId == ms_MenuArray.CVideoEditorMenuItems[iCount].MenuId)
		{
			return iCount;
		}
	}

	return -1;  // no index found
}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorMenu::FillInClipData
// PURPOSE: populates the Load data ready to be set up in the menu ui
/////////////////////////////////////////////////////////////////////////////////////////
bool CVideoEditorMenu::FillInClipData(const s32 iColumn)
{
	CRawClipFileView *pClipFileView = CVideoEditorUi::GetClipFileView();
	if( pClipFileView && pClipFileView->isPopulated() )
	{
		ms_inProjectToDeleteCount = 0;
		ms_otherUserToDeleteCount = 0;

		bool const c_isClipMgrPage = !IsOnAddClipMenuOrAddClipFilterList();

		bool const c_isFilteringByFavourties = pClipFileView->GetCurrentFilterType() == CRawClipFileView::FILTERTYPE_FAVOURITES;
		bool const c_isFilteringByDelete = c_isClipMgrPage && pClipFileView->GetCurrentFilterType() == CFileViewFilter::FILTERTYPE_MARKED_DELETE_ANY_USER;
		s32 const c_filteredFileCount = (s32)pClipFileView->getFilteredFileCount();
		uiDisplayf("CVideoEditorMenu::FillInClipData - c_filteredFileCount - %d", c_filteredFileCount);

        u64 const c_currentUserId = CVideoEditorInterface::GetActiveUserId();

		// go through all slots we intend to list on the menu in this pane and store the details
		for (s32 i = 0; i < c_filteredFileCount; i++)
		{
			bool const c_isCurrentClipFavourite = pClipFileView->isClipFavourite( i );
			bool const c_isCurrentClipForDelete = c_isClipMgrPage && pClipFileView->isMarkedAsDelete( i );

			bool addToList = !c_isFilteringByFavourties && !c_isFilteringByDelete;
			if (!addToList)
			{
				if (c_isFilteringByFavourties)
				{
					addToList = c_isCurrentClipFavourite;
				}
				else if (c_isFilteringByDelete)
				{
					addToList = c_isCurrentClipForDelete;
				}
			}

			if( addToList )
			{
				CVideoEditorPreviewData newData;

				char cTextureName[RAGE_MAX_PATH];

				pClipFileView->getThumbnailName( i, cTextureName );

				newData.m_thumbnailTxdName = cTextureName;
				newData.m_thumbnailTextureName = cTextureName;
				newData.m_fileViewIndex = i;

				pClipFileView->getDisplayName(i, newData.m_title);

				pClipFileView->getFileDateString(i, newData.m_date);

				newData.m_fileSize = pClipFileView->getFileSize(i);
				newData.m_durationMs = pClipFileView->getClipFileDurationMs(i);

				PathBuffer path;
				pClipFileView->getFilePath( i, path.getBufferRef() );

				newData.m_usedInProject = CVideoEditorInterface::IsClipUsedInAnyProject( pClipFileView->getClipUID( i ) );
                newData.m_ownerId = pClipFileView->getOwnerId( i );

				newData.m_favourite = c_isCurrentClipFavourite && c_currentUserId == newData.m_ownerId;
				newData.m_flaggedForDelete = c_isCurrentClipForDelete;

				if (newData.m_flaggedForDelete)
				{
					if (newData.m_usedInProject)
						ms_inProjectToDeleteCount++;

					if (newData.m_ownerId != c_currentUserId)
						ms_otherUserToDeleteCount++;
				}

				uiDisplayf("CVideoEditorMenu::FillInClipData - m_flaggedForDelete - %d - %d", i, newData.m_flaggedForDelete);
				newData.m_trafficLight = 0;

				ms_PreviewData[iColumn].PushAndGrow(newData);
			}
		}			

		return true;
	}

	return false;
}

s32 CVideoEditorMenu::GetFileViewIndexForDataIndex( const s32 iFileDataIndex )
{
	CFileViewFilter* fileView = GetFileViewFilterForMenu( GetCurrentMenuId() );
	s32 const c_result = fileView ? fileView->getFileViewIndex( iFileDataIndex ) : -1;
	return c_result;
}

s32 CVideoEditorMenu::GetFileViewIndexForItem( const s32 iColumn, const s32 iItemIndex )
{
    CVideoEditorPreviewData const * const previewData = GetPreviewData( iColumn, iItemIndex );
	s32 const c_fileViewIndex = previewData ? previewData->m_fileViewIndex : -1;

	return c_fileViewIndex;
}

s32 CVideoEditorMenu::GetItemIndexForFileViewIndex( const s32 iColumn, const s32 viewIndex )
{
	s32 result = -1;

	if( iColumn >= 0 && iColumn < MAX_COL_NUMs )
	{
		atArray<CVideoEditorPreviewData>& previewDataArray = ms_PreviewData[iColumn];

		for( s32 itemIndex = 0; result == -1 && itemIndex < previewDataArray.GetCount(); ++itemIndex )
		{
			CVideoEditorPreviewData& previewData = previewDataArray[ itemIndex ];
			result = previewData.m_fileViewIndex == viewIndex ? itemIndex : -1;
		}
	}

	return result;
}

/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorMenu::RequestThumbnailOfItem
// PURPOSE: requests a new thumb for the index passed from the menu item.   Also
//			asserts and deals with multiple requests so we only ever deal with 1
//			request for menu items which is what we want
/////////////////////////////////////////////////////////////////////////////////////////
void CVideoEditorMenu::RequestThumbnailOfItem( const s32 iColumn, const s32 iIndex )
{
	// request the thumb:
	CThumbnailFileView *pPopulatedThumbnailView = GetThumbnailViewForPreviewData( ms_previewDataId[iColumn] );

	if (pPopulatedThumbnailView)
	{
		s32 const c_thumbnailIndex = GetFileViewIndexForItem( iColumn, iIndex );

		if( pPopulatedThumbnailView->isValidIndex( c_thumbnailIndex ) )
		{
            if( pPopulatedThumbnailView->canRequestThumbnails() )
            {
                if( pPopulatedThumbnailView->requestThumbnail( c_thumbnailIndex ) )
                {
                    ms_iClipThumbnailInUse = c_thumbnailIndex;

                    uiDisplayf("RequestThumbnailOfItem %d (%d)", c_thumbnailIndex, ms_iClipThumbnailInUse );

                    if (ms_iClipThumbnailInUse != -1 && pPopulatedThumbnailView->isThumbnailRequested(c_thumbnailIndex))
                    {
                        uiDisplayf("RequestThumbnailOfItem %d (%d) - in waiting state....", c_thumbnailIndex, ms_iClipThumbnailInUse);

                        CVideoEditorUi::SetStateWaitingForData();  // only set waiting on data once its been requested and got a valid index

                        return;
                    }
                    else
                    {
                        uiDisplayf("RequestThumbnailOfItem %d (%d) - not in waiting state....", c_thumbnailIndex, ms_iClipThumbnailInUse);
                    }
                }
                else
                {
                    uiDisplayf("RequestThumbnailOfItem %d (%d) - could not request this thumbnail, releasing....", c_thumbnailIndex, ms_iClipThumbnailInUse);
                    pPopulatedThumbnailView->releaseThumbnail(c_thumbnailIndex);
                }
            }
            else
            {
                // Set waiting for data as we need to attempt a rebuild when it's safe to re-request thumbnails
                CVideoEditorUi::SetStateWaitingForData(); 
            }
		}
	}
}


bool CVideoEditorMenu::IsLoadingThumbnail()
{
	return ms_iClipThumbnailInUse != -1;
}


bool CVideoEditorMenu::IsThumbnailLoadedOrFailed()
{
	CThumbnailFileView* pThumbnailView = GetThumbnailViewForPreviewData( ms_previewDataId[COL_2] );
	bool const c_isLoaded = ms_iClipThumbnailInUse == -1 ||
		( pThumbnailView && ( pThumbnailView->isThumbnailLoaded( ms_iClipThumbnailInUse ) || pThumbnailView->hasThumbnailLoadFailed( ms_iClipThumbnailInUse ) ) );

	return c_isLoaded;
}


/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorMenu::ReleaseThumbnailOfItem
// PURPOSE: releases the current menu item thumbnail in use
/////////////////////////////////////////////////////////////////////////////////////////
void CVideoEditorMenu::ReleaseThumbnailOfItem()
{
	if( IsLoadingThumbnail() )
	{
		CThumbnailFileView *pPopulatedThumbnailView = GetCurrentThumbnailView();

		if ( uiVerifyf( pPopulatedThumbnailView, "CVideoEditorMenu::ReleaseThumbnailOfItem - Thumbnail view not populated, can't release!" ) )
		{
			pPopulatedThumbnailView->releaseThumbnail( ms_iClipThumbnailInUse );
			uiDisplayf("ReleaseThumbnailOfItem %d", ms_iClipThumbnailInUse );
		}

		ms_iClipThumbnailInUse = -1;
	}
}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorMenu::FillInAudioData
// PURPOSE: populates the Load data ready to be set up in the menu ui
/////////////////////////////////////////////////////////////////////////////////////////
bool CVideoEditorMenu::FillInAudioData(s32 const c_column, eReplayMusicType const c_musicType)
{
	ms_PreviewData[c_column].Reset();

	if (c_column == COL_1)
	{
		for (u32 category = 0; category < CVideoProjectAudioTrackProvider::GetCountOfCategoriesAvailable(c_musicType); category++)
		{
			u32 const c_trackCount = CVideoProjectAudioTrackProvider::GetTotalCountOfTracksForCategory(c_musicType, category);

			s32 numberOfDisplayableItemsInCategory = 0;
			for (u32 track = 0; track < c_trackCount; track++)
			{
				u32 const c_soundHash = CVideoProjectAudioTrackProvider::GetMusicTrackSoundHash( c_musicType, category, track );
				bool const c_isCommercial = CVideoProjectAudioTrackProvider::IsCommercial(c_soundHash);

				bool const c_canDisplayTrack = 
#if DISABLE_COMMERCIALS
				!c_isCommercial &&
#else
				c_isCommercial ||
#endif
				( c_musicType == REPLAY_SCORE_MUSIC || c_musicType == REPLAY_AMBIENT_TRACK ||
				CVideoProjectAudioTrackProvider::GetMusicTrackUsageCount( c_musicType, category, track ) <= 0 );

				if( c_canDisplayTrack )
				{
					numberOfDisplayableItemsInCategory++;
				}
			}

			if (numberOfDisplayableItemsInCategory > 0)
			{
				CVideoEditorPreviewData newData;

				newData.m_title += TheText.Get( CVideoProjectAudioTrackProvider::GetMusicCategoryNameKey(c_musicType, category));
				newData.m_audioCategoryIndex = category;
				newData.m_usedInProject = false;
				newData.m_favourite = false;
				newData.m_flaggedForDelete = false;
				newData.m_trafficLight = 0;

				ms_PreviewData[c_column].PushAndGrow(newData);
			}	
		}
	}

	if (c_column == COL_2)
	{
		u32 const c_categoryDataIndex = ms_iCurrentItem[c_column-1];  // update based on what is the current item the previous column

		atArray<CVideoEditorPreviewData>& previewDataArray = ms_PreviewData[c_column-1];
		u32 const c_category = c_categoryDataIndex < previewDataArray.size() ? previewDataArray[c_categoryDataIndex].m_audioCategoryIndex : 0;

		u32 const c_trackCount = CVideoProjectAudioTrackProvider::GetTotalCountOfTracksForCategory(c_musicType, c_category);

		for (u32 track = 0; track < c_trackCount; track++)
		{
			CVideoEditorPreviewData newData;
			newData.m_soundHash = CVideoProjectAudioTrackProvider::GetMusicTrackSoundHash( c_musicType, c_category, track );

			bool const c_isCommercial = audRadioTrack::IsCommercial(newData.m_soundHash);

			bool const c_canDisplayTrack = c_isCommercial ||
										   c_musicType == REPLAY_SCORE_MUSIC || c_musicType == REPLAY_AMBIENT_TRACK ||
										   CVideoProjectAudioTrackProvider::GetMusicTrackUsageCount( c_musicType, c_category, track ) <= 0;

			if( c_canDisplayTrack )
			{
				LocalizationKey trackKey;
				LocalizationKey artistKey;
				CVideoProjectAudioTrackProvider::GetMusicTrackNameKey( c_musicType, c_category, track, trackKey.getBufferRef() );
				CVideoProjectAudioTrackProvider::GetMusicTrackArtistNameKey( c_musicType, c_category, track, artistKey.getBufferRef() );
#if __BANK
				if(!TheText.DoesTextLabelExist(trackKey.getBuffer()))
				{
					Displayf("Unable to find text key %s related to track %d in station %s check REPL_%s", trackKey.getBuffer(), track, CVideoProjectAudioTrackProvider::GetMusicCategoryNameKey(c_musicType, c_category), CVideoProjectAudioTrackProvider::GetMusicCategoryNameKey(c_musicType, c_category));
				}
				if(!TheText.DoesTextLabelExist(artistKey.getBuffer()))
				{
					Displayf("Unable to find text key %s related to track %d in station %s check REPL_%s", artistKey.getBuffer(), track, CVideoProjectAudioTrackProvider::GetMusicCategoryNameKey(c_musicType, c_category), CVideoProjectAudioTrackProvider::GetMusicCategoryNameKey(c_musicType, c_category));
				}
#endif
				newData.m_title = TheText.Get( trackKey.getBuffer() );

				if (c_musicType == REPLAY_RADIO_MUSIC && !c_isCommercial)  // fix for 2138863 and 2172592
				{
					newData.m_title += " - ";
					newData.m_title += TheText.Get( artistKey.getBuffer() );
				}

				newData.m_durationMs = CVideoProjectAudioTrackProvider::GetMusicTrackDurationMs(newData.m_soundHash);
                newData.m_durationMs = c_musicType == REPLAY_AMBIENT_TRACK && newData.m_durationMs <  MIN_AMBIENT_TRACK_DURATION_MS ?  MIN_AMBIENT_TRACK_DURATION_MS : newData.m_durationMs;

				newData.m_usedInProject = false;
				newData.m_favourite = false;
				newData.m_flaggedForDelete = false;
				newData.m_trafficLight = 0;

				ms_PreviewData[c_column].PushAndGrow(newData);
			}
		}
	}

	return true;
}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorMenu::FillInLoadProjectData
// PURPOSE: populates the Load data ready to be set up in the menu ui
/////////////////////////////////////////////////////////////////////////////////////////
bool CVideoEditorMenu::FillInLoadProjectData(const s32 iColumn)
{
	ms_PreviewData[iColumn].Reset();

	bool populated = false;

	CVideoProjectFileView *pProjectFileView = CVideoEditorUi::GetProjectFileView();
	if ( pProjectFileView && pProjectFileView->isPopulated() )
	{
		bool const c_hasOtherUsers = pProjectFileView->GetCurrentFilterType() == CFileViewFilter::FILTERTYPE_OTHER_USER || pProjectFileView->GetCurrentFilterType() == CFileViewFilter::FILTERTYPE_ANY_USER;
		bool const c_isFilteringByDelete = !c_hasOtherUsers && pProjectFileView->GetCurrentFilterType() == CFileViewFilter::FILTERTYPE_MARKED_DELETE_ANY_USER;
		
		bool const c_hideDetailsForUGC = ( c_hasOtherUsers || c_isFilteringByDelete ) && !HasUserContentPrivileges();

		bool hadToChangeNameforUGC = false;

        int const c_fileCount = (int)pProjectFileView->getFilteredFileCount();

		u64 const c_currentUserId = CVideoEditorInterface::GetActiveUserId();
		ms_otherUserToDeleteCount = 0;

		// go through all slots we intend to list on the menu in this pane and store the details
		for (s32 i = 0; i < c_fileCount; i++)
		{
			bool const c_isCurrentProjForDelete = pProjectFileView->isMarkedAsDelete( i );

			if (!c_isFilteringByDelete || c_isCurrentProjForDelete)
			{
				CVideoEditorPreviewData newData;
				char cTextureName[ RAGE_MAX_PATH ];

				pProjectFileView->getThumbnailName( i, cTextureName );

				newData.m_thumbnailTxdName = cTextureName;
				newData.m_thumbnailTextureName = cTextureName;
				newData.m_fileViewIndex = i;

				newData.m_ownerId = pProjectFileView->getOwnerId( i );

				// if file not for the current user, check privileges to see if we're allowed to show the custom file name
				if ( !c_hideDetailsForUGC || c_currentUserId == newData.m_ownerId )
				{
					pProjectFileView->getDisplayName(i, newData.m_title);
				}
				else
				{
					newData.m_title = TheText.Get("VEUI_PROJ_CUSTOM");
					hadToChangeNameforUGC = true;
				}

				pProjectFileView->getFileDateString(i, newData.m_date);

				newData.m_fileSize = pProjectFileView->getFileSize(i);

				newData.m_durationMs = pProjectFileView->getProjectFileDurationMs(i);
				newData.m_usedInProject = false;
				newData.m_favourite = false;
				newData.m_flaggedForDelete = c_isCurrentProjForDelete;
				newData.m_trafficLight = 0;

				if (newData.m_flaggedForDelete && newData.m_ownerId != c_currentUserId)
					ms_otherUserToDeleteCount++;

				ms_PreviewData[iColumn].PushAndGrow(newData);
			}
		}

#if RSG_ORBIS
		if (c_hideDetailsForUGC && hadToChangeNameforUGC)
		{
			s32 userId = ReplayFileManager::GetCurrentUserId();
			if (userId != ms_userLastShownUGCMessage)
			{
				g_rlNp.GetCommonDialog().ShowUGCRestrictionMsg( userId );
				ms_userLastShownUGCMessage = userId;
			}
		}
#endif

		populated = true;
	}

	return populated;
}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorMenu::FillInGalleryData
// PURPOSE: populdates the Gallery data ready to be set up in the menu ui
/////////////////////////////////////////////////////////////////////////////////////////
#if defined(VIDEO_PLAYBACK_ENABLED) && VIDEO_PLAYBACK_ENABLED
bool CVideoEditorMenu::FillInGalleryData(const s32 iColumn)
{
	ms_PreviewData[iColumn].Reset();

	bool populated = false;

	CVideoFileView *pVideoFileView = CVideoEditorUi::GetVideoFileView();

	u64 const c_currentUserId = CVideoEditorInterface::GetActiveUserId();
	ms_otherUserToDeleteCount = 0;

	if ( pVideoFileView && pVideoFileView->isPopulated() )
	{
		bool const c_isFilteringByDelete = pVideoFileView->GetCurrentFilterType() == CFileViewFilter::FILTERTYPE_MARKED_DELETE_ANY_USER;

		int const c_fileCount = (int)pVideoFileView->getFilteredFileCount();

#if !USE_ORBIS_UPLOAD
		VideoUploadManager::GetInstance().PrepOptimisedUploadedList();
#endif

		// go through all slots we intend to list on the menu in this pane and store the details
		for( int i = 0; i < c_fileCount; i++)
		{
			bool const c_isCurrentVideoForDelete = pVideoFileView->isMarkedAsDelete(i);

			if (!c_isFilteringByDelete || c_isCurrentVideoForDelete)
			{
				CVideoEditorPreviewData newData;

				char cTextureName[ RAGE_MAX_PATH ];
				pVideoFileView->getThumbnailName( i, cTextureName );
				newData.m_thumbnailTextureName = cTextureName;

				pVideoFileView->getThumbnailTxdName( i, cTextureName );
				newData.m_thumbnailTxdName = cTextureName;
				newData.m_fileViewIndex = i;

				pVideoFileView->getDisplayName(i, newData.m_title);
				pVideoFileView->getFileDateString(i, newData.m_date);

				newData.m_fileSize = pVideoFileView->getFileSize(i);
				newData.m_durationMs = pVideoFileView->getVideoDurationMs(i);
				newData.m_ownerId = pVideoFileView->getOwnerId( i );
				newData.m_usedInProject = false;
				newData.m_favourite = false;
				newData.m_flaggedForDelete = c_isCurrentVideoForDelete;

				if (newData.m_flaggedForDelete && newData.m_ownerId != c_currentUserId)
					ms_otherUserToDeleteCount++;

				if (VideoUploadManager::GetInstance().IsUploadEnabled())
				{
					CVideoUploadManager::UploadProgressState::Enum progressState = CVideoUploadManager::UploadProgressState::NotUploaded;

	#if !USE_ORBIS_UPLOAD
					char pathBuffer[RAGE_MAX_PATH];
					pVideoFileView->getFilePath( i, pathBuffer );
					progressState = VideoUploadManager::GetInstance().GetProgressState(pathBuffer, newData.m_durationMs, newData.m_fileSize );
	#endif
					newData.m_trafficLight = static_cast<u8>(progressState);
					// bump up notuploaded and uploading by one so it's off, red, amber, green
					if (newData.m_trafficLight < CVideoUploadManager::UploadProgressState::Processing)
						++newData.m_trafficLight;
				}
				else
				{
					newData.m_trafficLight = 0;
				}

				ms_PreviewData[iColumn].PushAndGrow(newData);
			}
		}

#if !USE_ORBIS_UPLOAD
		VideoUploadManager::GetInstance().FinaliseOptimisedUploadedList();
#endif
		populated = true;
	}

	return populated;
}
#endif // defined(VIDEO_PLAYBACK_ENABLED) && VIDEO_PLAYBACK_ENABLED



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorMenu::HasUserContentPrivileges
// PURPOSE: custom method of checking UGC privileges for showing all user files
/////////////////////////////////////////////////////////////////////////////////////////
bool CVideoEditorMenu::HasUserContentPrivileges()
{
#if RSG_ORBIS
	// similar to CGalleryMenu::HasUserContentPrivileges workaround to ignore PS+ check

	// not required for any other platform (yet), but we can just call CLiveManager::CheckUserContentPrivileges if that's needed

	bool bIsUgcRestricted = false;

	if ( g_rlNp.GetUgcRestriction(NetworkInterface::GetLocalGamerIndex(), &bIsUgcRestricted) == SCE_OK )
	{
		if (!bIsUgcRestricted)
		{
			return true;
		}
	}

	return false;
#else // other platforms, just say it's allowed
	return true;
#endif
}


/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorMenu::ClearData
// PURPOSE: Clears any preview data we may of set up - called when we leave a column
/////////////////////////////////////////////////////////////////////////////////////////
void CVideoEditorMenu::ClearPreviewData( const s32 iColumn, bool const releaseThumbnail, bool const wipeFocus )
{
	if( releaseThumbnail )
	{
		ReleaseThumbnailOfItem();

		if (ms_PreviewData[iColumn].GetCount() > 0)
		{
			CVideoEditorUi::ReleaseFileViewThumbnails();
		}
	}

	// clear preview array
	uiAssertf(ms_iClipThumbnailInUse == -1, "Thumbnail not released before ClearData called!");

	if (iColumn == -1)
	{
		for (s32 i = 0; i < MAX_COL_NUMs; i++)
		{
			ClearPreviewDataInternal(i,wipeFocus);
		}
	}
	else
	{
		ClearPreviewDataInternal(iColumn,wipeFocus);
	}
}

void CVideoEditorMenu::UpdateInstructionalButtons( CScaleformMovieWrapper& buttonsMovie )
{
	if( buttonsMovie.IsActive() )
	{
		s32 const c_currentMenuId = GetCurrentMenuId();
        s32 const c_fileListColumn = COL_2;

		bool const c_wasKbmLastFrame =  CVideoEditorInstructionalButtons::LatestInstructionsAreKeyboardMouse();
		bool const c_isItemSelectable = IsItemSelectable(c_currentMenuId, ms_iCurrentColumn, ms_iCurrentItem[ms_iCurrentColumn]);
		bool const c_canItemBeActioned = CanItemBeActioned(c_currentMenuId, ms_iCurrentColumn, ms_iCurrentItem[ms_iCurrentColumn]);

        // Safe to call this even if not on a file view, will just be -1 or garbage
        FileViewIndex const c_fileViewIndex = GetFileViewIndexForItem( c_fileListColumn, ms_iCurrentItem[c_fileListColumn] );

		if (CControlMgr::GetVirtualKeyboardState() == ioVirtualKeyboard::kKBState_PENDING)
		{
			CVideoEditorInstructionalButtons::AddButton(INPUT_FRONTEND_ACCEPT, TheText.Get("IB_CONFIRM"));
			CVideoEditorInstructionalButtons::AddButton(INPUT_FRONTEND_CANCEL, TheText.Get("IB_CANCEL"));
		}

#if defined(VIDEO_PLAYBACK_ENABLED) && VIDEO_PLAYBACK_ENABLED
#if !USE_ORBIS_UPLOAD
		else if (VideoUploadManager::GetInstance().IsAddingText())
		{
			// early out if we're adding text in upload flow, but text box isn't up
			return;
		}
#endif // !USE_ORBIS_UPLOAD
#endif
		else if (CVideoEditorUi::IsAdjustingStageText())
		{
			CVideoEditorInstructionalButtons::AddButton(INPUT_FRONTEND_ACCEPT, TheText.Get("IB_FIN"), true);
			CVideoEditorInstructionalButtons::AddButton(INPUTGROUP_FRONTEND_LSTICK_ALL, TheText.Get("IB_POS"));
			CVideoEditorInstructionalButtons::AddButton(INPUTGROUP_FRONTEND_TRIGGERS, TheText.Get("IB_SIZE"));
		}
		else
		{

	#if defined(VIDEO_PLAYBACK_ENABLED) && VIDEO_PLAYBACK_ENABLED
            VideoInstanceHandle const c_videoHandle = CVideoEditorUi::GetActivePlaybackHandle();

			if( VideoPlayback::IsValidHandle( c_videoHandle ) )
			{
				CVideoEditorInstructionalButtons::AddButton(INPUT_FRONTEND_CANCEL, TheText.Get("IB_BACK"), true);

				if( !VideoPlayback::IsAtEndOfVideo( c_videoHandle ) )
				{
					InputType const playInput = c_wasKbmLastFrame ? INPUT_REPLAY_PAUSE : INPUT_FRONTEND_RT;

					CVideoEditorInstructionalButtons::AddButton( playInput,
						TheText.Get( CVideoEditorUi::IsPlaybackPaused() ? "VEUI_PLAY" : "VEUI_PAUSE" ),
						true);
				}
#if !RSG_ORBIS
                if( !VideoPlayback::IsAtStartOfVideo( c_videoHandle ) )
                {
                    InputType const jumpToStartInput = c_wasKbmLastFrame ? INPUT_REPLAY_STARTPOINT : INPUT_FRONTEND_LT;
                    CVideoEditorInstructionalButtons::AddButton( jumpToStartInput, TheText.Get("VEUI_JUMP_START"), true);
                }	
#endif
			}
			else
	#endif // defined(VIDEO_PLAYBACK_ENABLED) && VIDEO_PLAYBACK_ENABLED
			{
                if( c_isItemSelectable && c_canItemBeActioned )
                {
                    char const * const c_promptLngKey = IsOnClipManagementMenu() ? "IB_PREVIEWC" : IsOnGalleryMenu() ? "IB_PREVIEWV" : "IB_SELECT";
                    CVideoEditorInstructionalButtons::AddButton(INPUT_FRONTEND_ACCEPT, TheText.Get( c_promptLngKey ), true);
                }

				if (IsOnOptionsMenu())
				{
					bool const c_haveAnyVideoClipsInProject = (CVideoEditorTimeline::GetMaxClipCount(PLAYHEAD_TYPE_VIDEO) != 0);  // do we have any clips currently setup on the timeline?

					if (c_haveAnyVideoClipsInProject)
					{
						InputType const toggleInput = c_wasKbmLastFrame ? INPUT_REPLAY_TOGGLE_TIMELINE : INPUT_FRONTEND_CANCEL;
						CVideoEditorInstructionalButtons::AddButton(toggleInput, TheText.Get("IB_VIEW_TIMELINE"), true);
					}
				}
				else if (IsOnMainMenu())
				{
					CVideoEditorInstructionalButtons::AddButton(INPUT_FRONTEND_CANCEL, TheText.Get("IB_EXIT_REDITOR"), true);
				}
				else
				{
					CVideoEditorInstructionalButtons::AddButton(INPUT_FRONTEND_CANCEL, TheText.Get("IB_BACK"), true);
				}

				u32 const c_optionIndex = ms_iCurrentItem[ms_iCurrentColumn];
				CVideoEditorMenuItem& menuItem = ms_MenuArray.CVideoEditorMenuItems[c_currentMenuId];
				if( c_optionIndex < menuItem.Option.size() )
				{
					atHashString const c_toggleHash = menuItem.Option[ c_optionIndex ].ToggleValue;

					if (c_toggleHash != 0)
					{
						CVideoEditorInstructionalButtons::AddButton(INPUTGROUP_FRONTEND_DPAD_LR, TheText.Get("VEUI_TIME_ADJ"), true);
					}
				}

				InputType const deleteInput = c_wasKbmLastFrame ? INPUT_REPLAY_CLIP_DELETE : INPUT_FRONTEND_Y;
				
				// could do with a tidy. all a bit messy because AddButton has to be called in order
				if( IsOnClipManagementMenu() || IsOnProjectMenu() )
				{
					bool const c_isOnMarkedForDeleteFilter = IsOnMarkedForDeleteFilter();
					CVideoEditorInstructionalButtons::AddButton( deleteInput, c_isOnMarkedForDeleteFilter ? TheText.Get("VEUI_DEL_MARKED") : TheText.Get("IB_DELETE"), true );
				}

				if( ( IsOnAddClipMenu() || IsOnClipManagementMenu() ) && c_isItemSelectable && c_canItemBeActioned )
				{
					InputType const favouriteInput = c_wasKbmLastFrame ? INPUT_FRONTEND_X : INPUT_FRONTEND_X;
					CRawClipFileView const * clipView = CVideoEditorUi::GetClipFileView();

					bool const c_isCurrentItemFavourite = clipView && clipView->isClipFavourite( c_fileViewIndex );
					char const * const c_favLngKey = c_isCurrentItemFavourite ? "IB_UNMARKFAV" : "IB_MARKFAV";

					CVideoEditorInstructionalButtons::AddButton( favouriteInput, TheText.Get( c_favLngKey ), true );
				}

				if (IsOnGalleryMenu())
                {
                    CVideoEditorPreviewData const * const c_previewData = GetPreviewData( ms_iCurrentColumn, ms_iCurrentItem[ms_iCurrentColumn] );
					bool isUploadEnabled = VideoUploadManager::GetInstance().IsUploadEnabled() && c_previewData && c_previewData->BelongsToCurrentUser();

#if !USE_ORBIS_UPLOAD
					bool uploadButtonSet = false;
					if (isUploadEnabled)
					{
						if (VideoUploadManager::GetInstance().IsUploading())
						{
							CVideoFileView* videoView = CVideoEditorUi::GetVideoFileView();
							if( videoView )
							{
								char pathBuffer[RAGE_MAX_PATH];
								videoView->getFilePath( c_fileViewIndex, pathBuffer );

								if (VideoUploadManager::GetInstance().IsPathTheCurrentUpload(pathBuffer))
								{
									if (!VideoUploadManager::GetInstance().IsProcessing())
									{
										CVideoEditorInstructionalButtons::AddButton(INPUT_FRONTEND_Y, TheText.Get("IB_CANCEL_UPLOAD"), true);

										if (VideoUploadManager::GetInstance().IsUploadingPaused())
										{
											CVideoEditorInstructionalButtons::AddButton(INPUT_FRONTEND_X, TheText.Get("IB_RESUME_UPLOAD"), true);
										}
										else
										{
											CVideoEditorInstructionalButtons::AddButton(INPUT_FRONTEND_X, TheText.Get("IB_PAUSE_UPLOAD"), true);
										}
									}

									uploadButtonSet = true;
								}
							}
						}
					}

					if (!uploadButtonSet)
#endif // !USE_ORBIS_UPLOAD
					{
						bool const c_isOnMarkedForDeleteFilter = IsOnMarkedForDeleteFilter();
						CVideoEditorInstructionalButtons::AddButton( deleteInput, c_isOnMarkedForDeleteFilter ? TheText.Get("VEUI_DEL_MARKED") : TheText.Get("IB_DELETE"), true );

						if (isUploadEnabled)
						{
							CVideoEditorInstructionalButtons::AddButton(INPUT_FRONTEND_X, TheText.Get("IB_UPLOADEXT"), true);
						}
					}
				}

				if( IsOnAddClipMenu() || IsOnProjectMenu() || IsOnClipManagementMenu() || IsOnGalleryMenu() )
				{	
					CFileViewFilter const* fileView = GetFileViewFilterForMenu( c_currentMenuId );
					bool const c_moreThanOneFile = fileView && fileView->getFilteredFileCount() > 1;

					if( c_moreThanOneFile  )
					{
						CVideoEditorInstructionalButtons::AddButton(INPUT_FRONTEND_RB, IsCurrentMenuSortInverse() ? TheText.Get("IB_SORT_ORDER_ASC") : TheText.Get("IB_SORT_ORDER_DEC"), true );
					}
				}

				if( IsOnClipManagementMenu() || IsOnProjectMenu() || IsOnGalleryMenu() )
				{
					CVideoEditorPreviewData const * const c_previewData = GetPreviewData( ms_iCurrentColumn, ms_iCurrentItem[ms_iCurrentColumn] );
					CVideoEditorInstructionalButtons::AddButton(INPUT_FRONTEND_LB, c_previewData->m_flaggedForDelete ? TheText.Get("VEUI_DEL_UNMARK") : TheText.Get("VEUI_DEL_MARK"), true );
				}
			}
		}
	}
}


CVideoEditorPreviewData const* CVideoEditorMenu::GetPreviewData( s32 const iColumn, s32 const itemIndex )
{
    atArray<CVideoEditorPreviewData> const * const c_previewDataArray = iColumn >= 0 && iColumn < MAX_COL_NUMs ? &(ms_PreviewData[ iColumn ]) : NULL;
    CVideoEditorPreviewData const * const c_result = c_previewDataArray && itemIndex >= 0 && itemIndex < c_previewDataArray->GetCount() ? &(*c_previewDataArray)[itemIndex] : NULL;
    return c_result;
}

void CVideoEditorMenu::ClearPreviewDataInternal( const s32 iColumn, bool const wipeFocus )
{
	ms_iCurrentItem[iColumn] = wipeFocus ? 0 : ms_iCurrentItem[iColumn];
	ms_iCurrentScrollOffset[iColumn] = wipeFocus ? 0 : ms_iCurrentScrollOffset[iColumn];
	ms_previewDataId[iColumn] = -1;
	ms_previewDataSort[iColumn] = -1;
	ms_previewDataFilter[iColumn] = -1;
    ms_previewDataInversion[iColumn] = true;
	ms_PreviewData[iColumn].Reset();
}

void CVideoEditorMenu::InvertPreviewDataInternal( const s32 iColumn )
{
    atArray<CVideoEditorPreviewData>& previewArray = ms_PreviewData[iColumn];

    CThumbnailFileView const * const c_fileView = GetThumbnailViewForMenu( GetMenuId( iColumn ) );
    if( c_fileView )
    {
        int const c_maxViewItems = (int)c_fileView->getFilteredFileCount();
        int const c_maxItems = previewArray.GetCount();

        for( int index = 0;  index < c_maxItems; ++index )
        {
            CVideoEditorPreviewData& previewData = previewArray[index];

            s32 const c_newIndex = c_maxViewItems - previewData.m_fileViewIndex - 1;
            previewData.m_fileViewIndex = c_newIndex;
        }

        // Update the active thumbnail index
        ms_iClipThumbnailInUse = ms_iClipThumbnailInUse >= 0 ? c_maxViewItems - ms_iClipThumbnailInUse - 1 : ms_iClipThumbnailInUse;
        std::reverse( previewArray.begin(), previewArray.end() );
    }

}

#if RSG_PC
void CVideoEditorMenu::UpdateMouseEvent(const GFxValue* args)
{
	const s32 iButtonType = (s32)args[2].GetNumber();

	bool ignoreEvent = false;

#if defined(VIDEO_PLAYBACK_ENABLED) && VIDEO_PLAYBACK_ENABLED

	// Must ignore mouse events if the video preview is playing, as it causes
	// inconsistencies on return to the screen.
	if( VideoPlayback::IsValidHandle( CVideoEditorUi::GetActivePlaybackHandle() ) )
	{
		ignoreEvent = true;
	}

#endif // defined(VIDEO_PLAYBACK_ENABLED) && VIDEO_PLAYBACK_ENABLED

	if( !ignoreEvent )
	{
		s32 const c_currentMenuId = GetCurrentMenuId();

		if( uiVerifyf(args[3].IsNumber() && args[5].IsNumber(), "CVideoEditorMenu::UpdateMouseEvent - Invalid param %s %s", 
			sfScaleformManager::GetTypeName(args[3]), sfScaleformManager::GetTypeName(args[5]) ) )
		{
			const s32 iEventType = (s32)args[3].GetNumber();  // item type

			if( iButtonType == BUTTON_TYPE_LIST_SCROLL_ARROWS )
			{
				if( iEventType == EVENT_TYPE_MOUSE_RELEASE )
				{
					s32 const scrollDirection = (s32)args[5].GetNumber();

					switch( scrollDirection )
					{
					case 0:
						{
							ActionInputDown();
							break;
						}
						
					case 1:
						{
							ActionInputUp();
							break;
						}

					default:
						{
							uiDisplayf( "Unknown scroll direction %d requested", scrollDirection );
						}
					}
				}
			}

			if( iButtonType == BUTTON_TYPE_MENU &&
				uiVerifyf( args[6].IsNumber(), "CVideoEditorMenu::UpdateMouseEvent - Invalid param %s", sfScaleformManager::GetTypeName(args[6])))
			{
				const s32 iEventType = (s32)args[3].GetNumber();  // item type
				const s32 iColumnIndex = (s32)args[5].GetNumber();  // column index
				const s32 iVisibleMenuItemHovered = (s32)args[6].GetNumber();
				const s32 iItemIndex = ms_iCurrentScrollOffset[iColumnIndex] + iVisibleMenuItemHovered;  // item index
				s32 const c_clickedMenuId = GetMenuId( iColumnIndex );

				if (iEventType == EVENT_TYPE_MOUSE_PRESS)
				{
					ms_mousePressedForToggle = 0;

					if (ms_iCurrentItem[iColumnIndex] == iItemIndex)
					{
						if (args[7].IsDefined())
						{
							u32 const c_optionIndex = iItemIndex;
							CVideoEditorMenuItem& menuItem = ms_MenuArray.CVideoEditorMenuItems[c_currentMenuId];

							if( c_optionIndex < menuItem.Option.size() )
							{
								atHashString const c_toggleHash = menuItem.Option[ c_optionIndex ].ToggleValue;

								if (c_toggleHash != 0)
								{
									ms_mousePressedForToggle = (s32)args[7].GetNumber() == 1 ? 1 : -1;
								}
							}
						}
						
						return;
					}
				}

				if (iEventType == EVENT_TYPE_MOUSE_ROLL_OVER)
				{
					if (CMousePointer::HasMouseInputOccurred())  // only if mouse has moved
					{
						bool bGotoItem = false;

						if (iColumnIndex == GetCurrentColumn())
						{
							if (iItemIndex >= 0)
							{
								if (CanHighlightItem(c_currentMenuId, ms_iCurrentColumn, iItemIndex))
								{
									if (ms_iCurrentItem[iColumnIndex] != iItemIndex)
									{
										if (IsOnMainMenu() || IsOnOptionsMenu())
										{
											bGotoItem = true;
										}
									}
								}
							}
						}

						ms_iColumnHoveredOver = iColumnIndex;

						if (bGotoItem)
						{
							// we always set the next columns in the tree to be 0
							for (s32 i = iColumnIndex + 1; i < MAX_COL_NUMs; i++)
							{
								ms_iCurrentScrollOffset[i] = 0;
								ms_iCurrentItem[i] = 0;
							}

							GoToItem( iItemIndex, GOTO_REBUILD_PREVIEW_PANE );
						}
						else
						{
							SetItemHovered(iColumnIndex, iVisibleMenuItemHovered);  // set the item hovered
						}

						return;
					}
				}

				if (iEventType == EVENT_TYPE_MOUSE_ROLL_OUT)
				{
					ms_mousePressedForToggle = 0;

					return;
				}

				if (iEventType == EVENT_TYPE_MOUSE_RELEASE)
				{
					bool refilterFiew = IsFileFilteringMenu( c_clickedMenuId );
					s32 const c_initialItemFocus = ms_iCurrentItem[iColumnIndex];

					if (iColumnIndex != GetCurrentColumn())
					{
						if( iColumnIndex > GetCurrentColumn() )
						{
							JumpPane();

							if( iItemIndex >= 0)   // do not want to continue applying the clicked item when we have changed columns and item is the one we are on, as we want to rehighlight it first
							{
								if( IsItemSelectable( c_currentMenuId, ms_iCurrentColumn, iItemIndex ) )
								{
									if( ms_iCurrentItem[iColumnIndex] == iItemIndex )
									{
										return;
									}
								}
							}
						}
						else
						{
							g_FrontendAudioEntity.PlaySound( "BACK", "HUD_FRONTEND_DEFAULT_SOUNDSET" );

							refilterFiew = false;
							GoBackOnePane( iItemIndex );

							// do not want to continue applying the clicked item when we go back if on these menus
							if( IsOnMainMenu() || IsOnOptionsMenu() )  
							{
								return;
							}
						}
					}

					if( refilterFiew )
					{
						CVideoEditorMenuItem& menuItem = ms_MenuArray.CVideoEditorMenuItems[ c_clickedMenuId ];
						ExecuteDependentAction( c_clickedMenuId, menuItem.Option[ iItemIndex ].DependentAction.GetHash() );
					}

					if( iItemIndex >= 0 )
					{
						if( CanHighlightItem( c_currentMenuId, ms_iCurrentColumn, iItemIndex ) )
						{
							if( c_initialItemFocus == iItemIndex && IsItemSelectable( c_currentMenuId, ms_iCurrentColumn, iItemIndex ) )
							{
								CMousePointer::SetMouseAccept();  // select the item as its the item we are on
							}
							else
							{
								// we always set the next columns in the tree to be 0
								for (s32 i = iColumnIndex + 1; i < MAX_COL_NUMs; i++)
								{
									ms_iCurrentScrollOffset[i] = 0;
									ms_iCurrentItem[i] = 0;
								}

								GoToItem( iItemIndex, GOTO_REBUILD_PREVIEW_PANE );
							}
						}
					}
				}
			}
		}
	}
}

#endif // #if RSG_PC

#endif // #if defined( GTA_REPLAY ) && GTA_REPLAY
// eof
