/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : Editor.cpp
// PURPOSE : the NG video editor
// AUTHOR  : Derek Payne
// STARTED : 17/06/2014
//
/////////////////////////////////////////////////////////////////////////////////
#include "frontend/VideoEditor/ui/Menu.h"

#if defined( GTA_REPLAY ) && GTA_REPLAY

// rage
#include "atl/array.h"
#include "input/mouse.h"
#include "math/amath.h"
#include "parser/manager.h"

// fw
#include "fwlocalisation/textUtil.h"
#include "fwrenderer/renderthread.h"
#include "fwutil/ProfanityFilter.h"

// game
#include "audio/frontendaudioentity.h"
#include "audio/radioaudioentity.h"
#include "audio/northaudioengine.h"
#include "audio/music/musicplayer.h"
#include "frontend/VideoEditor/ui/Editor.h"
#include "frontend/VideoEditor/ui/Menu.h"
#include "frontend/VideoEditor/ui/InstructionalButtons.h"
#include "frontend/VideoEditor/ui/Playback.h"
#include "frontend/VideoEditor/ui/TextTemplate.h"
#include "frontend/VideoEditor/ui/Timeline.h"
#include "frontend/VideoEditor/ui/WatermarkRenderer.h"
#include "frontend/VideoEditor/Core/VideoEditorUtil.h"
#include "frontend/VideoEditor/Core/VideoProjectAudioTrackProvider.h"
#include "frontend/VideoEditor/Views/RawClipFileView.h"
#include "frontend/VideoEditor/Views/VideoProjectFileView.h"
#include "frontend/VideoEditor/Views/ThumbnailFileView.h"
#include "frontend/VideoEditor/Views/FileViewFilter.h"
#include "frontend/VideoEditor/VideoEditorInterface.h"

#include "frontend/BusySpinner.h"
#include "frontend/GameStreamMgr.h"
#include "frontend/hud_colour.h"
#include "frontend/MousePointer.h"
#include "frontend/NewHud.h"
#include "frontend/HudTools.h"
#include "Frontend/PauseMenu.h"
#include "Frontend/ui_channel.h"
#include "frontend/WarningScreen.h"
#include "frontend/landing_page/LandingPage.h"

#include "renderer/sprite2d.h"
#include "replaycoordinator/ReplayEditorParameters.h"
#include "replaycoordinator/storage/Montage.h"
#include "replaycoordinator/Storage/MontageMusicClip.h"
#include "replaycoordinator/Storage/MontageText.h"

#include "camera/CamInterface.h"
#include "control/replay/replay.h"
#include "control/replay/ReplayRecording.h"
#include "control/videorecording/videorecording.h"
#include "Network/Live/NetworkRichPresence.h"
#include "network/NetworkInterface.h"
#include "Network/xlast/presenceutil.h"
#include "Peds/ped.h"
#include "text/TextFormat.h"
#include "scene/world/GameWorld.h"
#include "System/controlMgr.h"
#include "streaming/streaming.h"

#include "Text/Messages.h"
#include "Text/Text.h"
#include "Text/TextFile.h"
#include "Text/TextFormat.h"
#include "text/TextConversion.h"
#include "Network/Cloud/VideoUploadManager.h"

#include "audiohardware/driver.h"

FRONTEND_OPTIMISATIONS();
//OPTIMISATIONS_OFF();

#if __BANK
PARAM(showEditorMemory, "Show memory used by the Editor Movie");
#endif

#define __IGNORE_PROFANITY_FILTER (0)

s32 CVideoEditorUi::ms_iMovieId = INVALID_MOVIE_ID;
eME_STATES CVideoEditorUi::ms_iState = ME_STATE_NONE;

bool CVideoEditorUi::ms_bRebuildTimeline = false;
bool CVideoEditorUi::ms_bRebuildAnchors = false;
bool CVideoEditorUi::ms_bProjectHasBeenEdited = false;
bool CVideoEditorUi::ms_bGameDestroyWarned = false;
bool CVideoEditorUi::ms_refreshVideoViewOnMemBlockFree = false;
CSprite2d CVideoEditorUi::ms_codeSprite[MAX_CODE_SPRITES];

s32 CVideoEditorUi::ms_iAutomatedLengthSelection;
bool CVideoEditorUi::ms_directorModeLaunched = false;
bool CVideoEditorUi::ms_hasStoppedSounds = false;

bool CVideoEditorUi::ms_actionEventsOnGameDestruction = false;

grcBlendStateHandle CVideoEditorUi::ms_StandardSpriteBlendStateHandle = grcStateBlock::BS_Invalid;

sSelectedAudioProperties CVideoEditorUi::ms_SelectedAudio;
sExportProperties CVideoEditorUi::ms_ExportProperties;

netProfanityFilter::CheckTextToken CVideoEditorUi::ms_profanityToken = netProfanityFilter::INVALID_TOKEN;
fwTemplateString<CMontageText::MAX_MONTAGE_TEXT_BYTES> CVideoEditorUi::ms_textBuffer;
CVideoEditorUi::FilenameInputBuffer CVideoEditorUi::ms_exportNameBuffer;
rage::SimpleString_128 CVideoEditorUi::ms_literalBuffer;
rage::SimpleString_128 CVideoEditorUi::ms_literalBuffer2;

bool CVideoEditorUi::ms_bRenderStageText;

#if defined(VIDEO_PLAYBACK_ENABLED) && VIDEO_PLAYBACK_ENABLED
rage::VideoInstanceHandle CVideoEditorUi::ms_activePlaybackHandle = INDEX_NONE;
#endif

CRawClipFileView *CVideoEditorUi::ms_clipFileView = NULL;
CVideoProjectFileView *CVideoEditorUi::ms_ProjectFileView = NULL;
#if defined(VIDEO_PLAYBACK_ENABLED) && VIDEO_PLAYBACK_ENABLED
CVideoFileView * CVideoEditorUi::ms_VideoFileView = NULL;
#endif
CVideoEditorProject *CVideoEditorUi::ms_project = NULL;


float const CVideoEditorUi::sm_minMemeTextSize			= 1.65f;
float const CVideoEditorUi::sm_maxMemeTextSize			= 15.0f;
float const CVideoEditorUi::sm_minMemeTextSizeIncrement	= 0.05f;

float const CVideoEditorUi::sm_triggerThreshold			= 0.1f;
float const CVideoEditorUi::sm_thumbstickThreshold		= 0.1f;
float const CVideoEditorUi::sm_thumbstickMultiplier		= 0.01f;

eOpenedFromSource CVideoEditorUi::sm_openedFromSource = OFS_UNKNOWN;

#if __BANK
static bool ms_bVideoEditorWidgetsCreated = false;
#endif // __BANK

#if LIGHT_EFFECTS_SUPPORT
const ioConstantLightEffect CVideoEditorUi::sm_LightEffect(255, 255, 255);
#endif // LIGHT_EFFECTS_SUPPORT

enum eUGCFeatureArea
{
    // Telemetry decided that "creator" and "editor" were part of the same setup, so 0 is for the creator, but lives off in script land
    ROCKSTAR_EDITOR_TELEMETRY_ID = 1,
};

enum eRockstarEditorLaunchSource
{
    ROCKSTAR_EDITOR_LAUNCHED_SP = 0,
    ROCKSTAR_EDITOR_LAUNCHED_MP = 1, // You cannot launch this from MP, but Telemetry gave it a value anyway
    ROCKSTAR_EDITOR_LAUNCHED_LANDING = 2,
};

void SendUGCTelemetry( eUGCFeatureArea const feature, eRockstarEditorLaunchSource const location )
{
    MetricUGC_NAV const c_metric( (int)feature, (int)location );
    APPEND_METRIC( c_metric );
}

//PURPOSE: constructor for sExportProperties
sExportProperties::sExportProperties()
{
	m_fps = MediaEncoderParams::OUTPUT_FPS_DEFAULT;
	m_quality = MediaEncoderParams::QUALITY_DEFAULT;
}

/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorUi::Init
// PURPOSE:	Initialise the main manager class
/////////////////////////////////////////////////////////////////////////////////////////
void CVideoEditorUi::Init(unsigned initMode)
{
	if ( initMode == rage::INIT_CORE )
	{
#if __BANK
		InitWidgets();
#endif
	}

	ms_iMovieId = INVALID_MOVIE_ID;
	ms_iState = ME_STATE_NONE;
	ms_directorModeLaunched = false;

	CHelpMessage::ClearAll();  // clear any help text that may be active
}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorUi::Shutdown
// PURPOSE:	shuts down the main manager class
/////////////////////////////////////////////////////////////////////////////////////////
void CVideoEditorUi::Shutdown(unsigned shutdownMode)
{
	if ( shutdownMode == rage::SHUTDOWN_CORE )
	{
#if __BANK
		ShutdownWidgets();
#endif
	}
}



void CVideoEditorUi::SetProjectEdited(const bool bEdit)
{
	ms_bProjectHasBeenEdited = bEdit;

	CVideoEditorMenu::RefreshProjectHeading();
/*
	if (CVideoEditorMenu::IsMenuSelected())
	{
		CVideoEditorMenu::RebuildMenu();  // rebuild the menu so the new project title is inserted into the menu as the 2nd string
	}*/
}

void CVideoEditorUi::SaveProject( bool const showSpinnerWithText )
{
	if( ms_project->GetClipCount() > 0 )  // only perform the save if we have more than 1 clip
	{
		u64 const c_saveResult =  ms_project->SaveProject( ms_project->GetName(), showSpinnerWithText );
		if( ( c_saveResult & CReplayEditorParameters::PROJECT_SAVE_RESULT_FAILED ) == 0 )
		{
			SetProjectEdited(false);  // it has saved, so set it as unedited again
		}
		else
		{
			DisplaySaveProjectResultPrompt( c_saveResult );
			ms_iState = ME_STATE_ACTIVE;
			ms_textBuffer.Clear();
			return;
		}
	}

	if( ms_ProjectFileView && ms_ProjectFileView->isInitialized() )
	{
		ms_ProjectFileView->refresh( false );
	}
	
	ms_textBuffer.Clear();
				
	if (ms_iState == ME_STATE_ACTIVE_WAITING_FOR_FILENAME_THEN_EXIT)
	{
		ms_iState = ME_STATE_ACTIVE;

		CVideoEditorMenu::ExitToMainMenu();
	}
	else
	{
		ms_iState = ME_STATE_ACTIVE;

		CVideoEditorMenu::RefreshProjectHeading();
		CVideoEditorMenu::RebuildMenu();
	}
}

void CVideoEditorUi::CheckAndTriggerExportPrompts( char const * const filename )
{
#if defined(VIDEO_PLAYBACK_ENABLED) && VIDEO_PLAYBACK_ENABLED
	bool const c_checkDisplayName = RSG_ORBIS ? true : false;
	bool const c_fileExists = ms_VideoFileView ? ms_VideoFileView->doesFileExistInProvider( filename, c_checkDisplayName, c_checkDisplayName ? 0 : CVideoEditorInterface::GetActiveUserId() ) : false;
#else
	bool const c_fileExists = false;
#endif

	ms_exportNameBuffer.Set( filename );
	CVideoEditorUi::getExportTimeAsString( ms_literalBuffer.getBufferRef() );

	if( c_fileExists )
	{
#if RSG_ORBIS
		CVideoEditorUi::TriggerOverwriteUnavailablePrompt( ms_exportNameBuffer.getBuffer() );
#else
		CVideoEditorMenu::SetUserConfirmationScreenWithAction( "TRIGGER_PUBLISH", "VE_EXP_OVRW", "VE_EXPORT", FE_WARNING_YES_NO, false, ms_literalBuffer.getBuffer() );
#endif
	}
	else
	{
		char const * const triggerAction = RSG_ORBIS ? "TRIGGER_EXP_PROMPT2" : "TRIGGER_PUBLISH";

		CVideoEditorMenu::SetUserConfirmationScreenWithAction( triggerAction, 
			"VE_EXP", "VE_EXPORT", FE_WARNING_YES_NO, false, ms_literalBuffer.getBuffer() );
	}
}

void CVideoEditorUi::UpdateLiteralStringAsDuration()
{
	if( ms_project )
	{
		getExportTimeAsString( ms_literalBuffer.getBufferRef() );
	}
}

void CVideoEditorUi::UpdateLiteralStringAsSize( size_t const sizeNeeded, size_t const sizeAvailable )
{
	size_t const c_neededKb = sizeNeeded / 1024;
	size_t const c_neededMb = c_neededKb / 1024;

	size_t const c_availableKb = sizeAvailable / 1024;
	size_t const c_availableMb = c_availableKb / 1024;

	bool const c_useMb = c_neededMb > 0 && ( c_availableMb > 0 || c_availableMb == 0 );
	bool const c_useKb = c_neededKb > 0 && ( c_availableKb > 0 || c_availableKb == 0 );

	char const * const c_sizeSuffix = c_useMb ? "mb" : c_useKb ? "k" : "b";

	size_t const c_neededDisplaySize = c_useMb ? c_neededMb + 1 : c_useKb ? c_neededKb + 1 : sizeNeeded;
	size_t const c_availableDisplaySize = c_useMb ? c_availableMb + 1 : c_useKb ? c_availableKb + 1 : sizeAvailable;

	ms_literalBuffer.sprintf( "%" SIZETFMT "u%s", c_neededDisplaySize, c_sizeSuffix );
	ms_literalBuffer2.sprintf( "%" SIZETFMT "u%s", c_availableDisplaySize, c_sizeSuffix );
}

void CVideoEditorUi::UpdateLiteralStringAsNumber( u32 const c_number )
{
	ms_literalBuffer.sprintf( "%u", c_number );
}

bool CVideoEditorUi::CanDuplicateClips()
{
	return ms_project ? ms_project->CanAddClip() : false;
}

bool CVideoEditorUi::HasMinDecoderMemory()
{
#if RSG_ORBIS || RSG_DURANGO
    //! PS4 and Xb1 hijack the replay allocator, so let's check if we need to free up some blocks...
    sysMemAllocator * const c_replayAllocator = sysMemManager::GetInstance().GetReplayAllocator();
    bool const c_hasEnoughMemory = c_replayAllocator ? c_replayAllocator->GetMemoryAvailable() > ( c_replayAllocator->GetHeapSize() / 2 ) : true;
    return c_hasEnoughMemory;
#else
    return true;
#endif
}

bool CVideoEditorUi::CheckForInviteInterruption()
{
	// if we've received an invite, that's not confirmed and we are not suppressed by script
	//static bool simulateInvite = false; 
	if ( ( CLiveManager::GetInviteMgr().HasPendingAcceptedInvite() && !CLiveManager::GetInviteMgr().HasConfirmedOrIsAutoConfirm() && !CLiveManager::GetInviteMgr().IsSuppressed()) /*|| simulateInvite*/ )
	{
		CWarningScreen::SetMessage( WARNING_MESSAGE_STANDARD, "NT_INV", FE_WARNING_OK_CANCEL);

		eWarningButtonFlags result = CWarningScreen::CheckAllInput();

		if (result == FE_WARNING_OK)
		{
			// exit video editor, not via pausemenu
			Close();

			if (CPauseMenu::IsActive(PM_SkipVideoEditor) && !CPauseMenu::IsClosingDown())
			{
				CPauseMenu::Close(); // ensure pausemenu is also shut down if its active
			}

			//Confirm invite
			CLiveManager::GetInviteMgr().AutoConfirmInvite();
			//simulateInvite = false;
		}
		else if (result == FE_WARNING_CANCEL)
		{
			CLiveManager::GetInviteMgr().CancelInvite();
			//simulateInvite = false;
		}

		return true;
	}

	return false;
}


/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorUi::Update
// PURPOSE:	main UT update
/////////////////////////////////////////////////////////////////////////////////////////
void CVideoEditorUi::Update()
{
	PF_AUTO_PUSH_TIMEBAR("CVideoEditorUi Update");

	if (CSystem::IsThisThreadId(SYS_THREAD_RENDER))  // only on UT
	{
		uiAssertf(0, "CVideoEditorUi::Update() can only be called on the Updatethread!");

		return;
	}
	
	if (IsActive())
	{
		ReplayRecordingScriptInterface::PreventRecordingThisFrame();
	}

	if (ms_iState == ME_STATE_NONE)  // return if we are not in an active state
	{
		return;
	}

	if (ms_iState == ME_STATE_SHUTDOWN)
	{
		CloseInternal();
		return;
	}

	if (CheckForInviteInterruption())
	{
		return;
	}

	CControlMgr::CheckForControllerDisconnect();

	if (ms_actionEventsOnGameDestruction && CReplayMgr::IsEditModeActive())
	{
		ActionEventsOnGameDestruction();
		ms_actionEventsOnGameDestruction = false;
	}

	if(CReplayMgr::IsEditorActive() && CReplayMgr::IsReplayInControlOfWorld() && (!CReplayMgr::IsEditModeActive() || CReplayMgr::IsLoading()) && !audDriver::GetMixer()->IsCapturing() && !CReplayMgr::IsPlaying() && !VideoPlayback::IsPlaybackActive())
	{
		if(!ms_hasStoppedSounds)
		{
			g_InteractiveMusicManager.OnReplayEditorActivate();
			audNorthAudioEngine::StopAllSounds();
			ms_hasStoppedSounds = true;
		}		
	}
	else
	{
		ms_hasStoppedSounds = false;
	}

	if (CVideoEditorMenu::Update())
	{
		return;
	}

	if (ms_bRebuildTimeline)
	{
		ms_bRebuildTimeline = !CVideoEditorTimeline::RebuildTimeline();
		return;
	}

	if( ms_bRebuildAnchors )
	{
		CVideoEditorTimeline::RebuildAnchors();

		ms_bRebuildAnchors = false;
	}

	//Update audio track preview
	if(!audDriver::GetMixer()->IsCapturing())
	{
		g_RadioAudioEntity.RequestUpdateMusicPreview();
	}

	switch (ms_iState)
	{
		case ME_STATE_ACTIVE_WAITING_FOR_STAGING_CLIP:
		{
			if (ms_project->IsStagingClipTextureRequested())
			{
				atString stagingTxdName;
				atString stagingName;

				if( ms_project->IsStagingClipTextureLoaded() )
				{
					ms_project->GetStagingClipTextureName( stagingName );
					stagingTxdName = stagingName;
				}
				else if( ms_project->IsStagingClipTextureLoadFailed() )
				{
					stagingTxdName = VIDEO_EDITOR_UI_FILENAME;
					stagingName = VIDEO_EDITOR_LOAD_ERROR_ICON;
				}

				if( stagingName.GetLength() > 0 )
				{
					CVideoEditorTimeline::AddVideoToPlayheadPreview( stagingTxdName.c_str(), stagingName.c_str() );

					// "auto-drop" the video clip if its the 1st one in the project
					if (GetProject()->GetClipCount() == 0)
					{
						CVideoEditorTimeline::DropPreviewClipAtPosition(0, false);
					}

					ms_iState = ME_STATE_ACTIVE;
				}
			}
			else
			{
				ms_project->InvalidateStagingClip();
				ms_iState = ME_STATE_ACTIVE;
			}

			break;
		}

		case ME_STATE_ACTIVE_WAITING_FOR_FILENAME:
		case ME_STATE_ACTIVE_WAITING_FOR_FILENAME_THEN_EXIT:
		case ME_STATE_ACTIVE_WAITING_FOR_EXPORT_FILENAME:
		{
			if (CVideoEditorMenu::CheckForUserConfirmationScreens())
			{
				break;
			}

			bool const c_isExporting = ms_iState == ME_STATE_ACTIVE_WAITING_FOR_EXPORT_FILENAME;

			if( !ms_textBuffer.IsNullOrEmpty() )
			{
				netProfanityFilter::ReturnCode rc = netProfanityFilter::GetStatusForRequest( ms_profanityToken );

#if __BANK
#if __IGNORE_PROFANITY_FILTER
				rc = netProfanityFilter::RESULT_STRING_OK;
#endif // __IGNORE_PROFANITY_FILTER
#endif // __BANK

				if( rc == netProfanityFilter::RESULT_ERROR || rc == netProfanityFilter::RESULT_INVALID_TOKEN )
				{					
					SetErrorState("PROFANITY_FILT_UNAVAIL_EXP");
					ms_iState = ME_STATE_ACTIVE;
                    ms_textBuffer.Clear();
                    ms_exportNameBuffer.Clear();
					ms_profanityToken = netProfanityFilter::INVALID_TOKEN;
					break;
				}
				else if( rc == netProfanityFilter::RESULT_STRING_FAILED  )
				{
					SetErrorState("TITLE_PROFANITY_FAIL");
					ms_iState = ME_STATE_ACTIVE;
					ms_textBuffer.Clear();
                    ms_exportNameBuffer.Clear();
					ms_profanityToken = netProfanityFilter::INVALID_TOKEN;
					break;
				}
				else if( rc == netProfanityFilter::RESULT_PENDING )
				{
					// NOP
				}
				else if( rc == netProfanityFilter::RESULT_STRING_OK )
				{
					ms_profanityToken = netProfanityFilter::INVALID_TOKEN;
                    ms_textBuffer.Trim();

					if ( c_isExporting )
					{
                        
                        bool const c_hadValidName = CVideoEditorUtil::IsValidSaveName( ms_textBuffer.getBuffer(), (u32)ms_exportNameBuffer.GetMaxByteCount() );
                        if( c_hadValidName )
                        {
                            CheckAndTriggerExportPrompts( ms_textBuffer.getBuffer() );                          
                        }
                        else
                        {
                            ms_literalBuffer.Set( ms_textBuffer.getBuffer() );
                            SetErrorState( "EXPORT_NAME_INVALID", ms_literalBuffer.getBuffer() );	
                            ms_exportNameBuffer.Clear();
                        }		

                        ms_iState = ME_STATE_ACTIVE;
                        ms_textBuffer.Clear();
					}
					else
					{
						// file already exists, so ask the user if they want to overwrite it
						if( FindProjectNameInUse( ms_textBuffer.getBuffer() ) )
						{
							CVideoEditorMenu::SetUserConfirmationScreenWithAction( "TRIGGER_OVERWRITE_PROJ_NEW_NAME", "VEUI_OVRWRITE_SURE" );
						}
						else
						{
							CVideoProjectFileView const* projectFileView = GetProjectFileView();
							bool const c_noProjectSlotsLeft =
								projectFileView ? CVideoEditorInterface::HasReachedMaxProjectLimit( *projectFileView ) : false;

							if( c_noProjectSlotsLeft )
							{
								SetErrorState( "TOO_MANY_PROJECTS" );	
							}
							else
							{
                                bool const c_hadValidName = ms_project ? ms_project->SetName( ms_textBuffer.getBuffer() ) : false;

								if( c_hadValidName )
                                {
                                    SaveProject( true );
                                }
                                else
                                {
                                    ms_literalBuffer.Set( ms_textBuffer.getBuffer() );
                                    SetErrorState( "PROJECT_NAME_INVALID", ms_literalBuffer.getBuffer() );	
                                }
								
							}

							ms_iState = ME_STATE_ACTIVE;
							ms_textBuffer.Clear();
						}
					}
				}	
			}
			else if (CControlMgr::UpdateVirtualKeyboard() != ioVirtualKeyboard::kKBState_PENDING)
			{
				if (CControlMgr::GetVirtualKeyboardState() == ioVirtualKeyboard::kKBState_SUCCESS)
				{
					if (ms_project)
					{
						const char *pKeyboardResult = CControlMgr::GetVirtualKeyboardResult();

						if( pKeyboardResult[0] != '\0' && pKeyboardResult[0] != ' ')
						{
                            ms_textBuffer.Set( pKeyboardResult );
                            ms_textBuffer.Trim();

                            if( ms_textBuffer.GetLength() > 0 )
                            {
                                atString invalidCharacterBuffer;

                                rage::fwProfanityFilter const& c_filter = rage::fwProfanityFilter::GetInstance();
                                fwProfanityFilter::eRESULT profanityFilterResult = c_filter.CheckForReservedTermsOnly( ms_textBuffer.getBuffer() );
                                profanityFilterResult = profanityFilterResult == fwProfanityFilter::RESULT_VALID ? 
                                    c_filter.CheckForUnsupportedFileSystemCharactersOnly( ms_textBuffer.getBuffer(), &invalidCharacterBuffer ) : profanityFilterResult;

                                if( profanityFilterResult == fwProfanityFilter::RESULT_RESERVED_WORD )
                                {
                                    char const * const c_errorType = c_isExporting ? "EXPORT_NAME_RESERVED" : "PROJECT_NAME_RESERVED";
                                    SetErrorState( c_errorType, ms_textBuffer.getBuffer() );
                                    ms_iState = ME_STATE_ACTIVE;
                                }
                                else if( profanityFilterResult == fwProfanityFilter::RESULT_UNSUPPORTED_CHARACTER )
                                {
                                    char const * const c_errorType = c_isExporting ? "EXPORT_NAME_CHAR" : "PROJECT_NAME_CHAR";
                                    ms_literalBuffer2.Set( invalidCharacterBuffer.c_str() );

                                    SetErrorState( c_errorType, ms_textBuffer.getBuffer(), ms_literalBuffer2.getBuffer() );
                                    ms_iState = ME_STATE_ACTIVE;
                                }
                                else
                                {
                                    netProfanityFilter::VerifyStringForProfanity( ms_textBuffer.getBuffer(),
                                        CText::GetScLanguageFromCurrentLanguageSetting(),
										true,
                                        ms_profanityToken );
                                }
                            }
                            else
                            {
                                char const * const c_errorType = c_isExporting ? "EXPORT_NAME_NULL" : "PROJECT_NAME_NULL";
                                SetErrorState( c_errorType, ms_textBuffer.getBuffer() );
                                ms_iState = ME_STATE_ACTIVE;
                            }
							
						}
						else
						{
							SetErrorState("FAILED_TO_INPUT_FILENAME");
							ms_textBuffer.Clear();
							ms_iState = ME_STATE_ACTIVE;
							break;
						}
					}
				}
				else  // exit back to active state (if we canceled or it failed for example)
				{
					ms_iState = ME_STATE_ACTIVE;
				}

				CVideoEditorInstructionalButtons::Refresh();
			}

			break;
		}

		case ME_STATE_ACTIVE_WAITING_FOR_TEXT_INPUT:
		{
			if( !ms_textBuffer.IsNullOrEmpty() )
			{
				netProfanityFilter::ReturnCode rc = netProfanityFilter::GetStatusForRequest( ms_profanityToken );

#if __BANK
#if __IGNORE_PROFANITY_FILTER
				rc = netProfanityFilter::RESULT_STRING_OK;
#endif // __IGNORE_PROFANITY_FILTER
#endif // __BANK

				if( rc == netProfanityFilter::RESULT_ERROR || rc == netProfanityFilter::RESULT_INVALID_TOKEN )
				{					
					SetErrorState("PROFANITY_FILT_UNAVAIL_TEXT");
					ms_iState = ME_STATE_ACTIVE;
					ms_textBuffer.Clear();
					ms_profanityToken = netProfanityFilter::INVALID_TOKEN;
					break;
				}
				else if( rc == netProfanityFilter::RESULT_STRING_FAILED  )
				{
					SetErrorState("TITLE_PROFANITY_FAIL");
					ms_iState = ME_STATE_ACTIVE;
					ms_textBuffer.Clear();
					ms_profanityToken = netProfanityFilter::INVALID_TOKEN;
					break;
				}
				else if( rc == netProfanityFilter::RESULT_PENDING )
				{
					// NOP
				}
				else if( rc == netProfanityFilter::RESULT_STRING_OK )
				{
					ms_iState = ME_STATE_ACTIVE;
					ms_profanityToken = netProfanityFilter::INVALID_TOKEN;
					sEditedTextProperties &editedText = CTextTemplate::GetEditedText();
					safecpy(editedText.m_text, ms_textBuffer.getBuffer(), NELEM(editedText.m_text));

					CTextTemplate::UpdateTemplate(0, editedText, "TEXT");

					CVideoEditorMenu::RebuildMenu();
				}
			}
			else if (CControlMgr::UpdateVirtualKeyboard() != ioVirtualKeyboard::kKBState_PENDING)
			{
				sEditedTextProperties &editedText = CTextTemplate::GetEditedText();

				if (CControlMgr::GetVirtualKeyboardState() == ioVirtualKeyboard::kKBState_SUCCESS)
				{
					const char *pKeyboardResult = CControlMgr::GetVirtualKeyboardResult();
					bool allSpaces = false;

					if (pKeyboardResult[0] != '\0')
					{
						if (pKeyboardResult[0] == ' ')
						{
							allSpaces = true;

							for (s32 i = 1; i < strlen(pKeyboardResult); i++)
							{
								if (pKeyboardResult[i] != ' ')
								{
									allSpaces = false;
									break;
								}
							}
						}

						// need to check for valid characters with the currently selected font, warn if not available (as virtual keyboard only checks STANDARD font at entry time)
						char16 wideString[ioVirtualKeyboard::VIRTUAL_KEYBOARD_RESULT_BUFFER_SIZE];
						Utf8ToWide(wideString, pKeyboardResult, ioVirtualKeyboard::VIRTUAL_KEYBOARD_RESULT_BUFFER_SIZE);

						if ( (!allSpaces) && (CTextFormat::DoAllCharactersExistInFont(editedText.m_style, wideString, NELEM(wideString))) )  // check with the active font
						{
							ms_textBuffer.Set( pKeyboardResult );

							netProfanityFilter::VerifyStringForProfanity( ms_textBuffer.getBuffer(),
																			CText::GetScLanguageFromCurrentLanguageSetting(),
																			true,
																			ms_profanityToken );
						}
						else
						{
							SetErrorState("FAILED_CHARS_TEXT");
							ms_textBuffer.Clear();
							ms_profanityToken = netProfanityFilter::INVALID_TOKEN;
							safecpy(editedText.m_text, CTextTemplate::GetEditedTextBackup().m_text, NELEM(editedText.m_text));
							ms_iState = ME_STATE_ACTIVE;
							CVideoEditorMenu::RebuildMenu();
						}
					}
					else
					{
						SetErrorState("FAILED_TO_INPUT_TEXT");

						safecpy(editedText.m_text, CTextTemplate::GetEditedTextBackup().m_text, NELEM(editedText.m_text));
						ms_iState = ME_STATE_ACTIVE;
						CVideoEditorMenu::RebuildMenu();
					}
				}
				else  // exit back to active state (if we canceled or it failed for example)
				{
					safecpy(editedText.m_text, CTextTemplate::GetEditedTextBackup().m_text, NELEM(editedText.m_text));
					ms_iState = ME_STATE_ACTIVE;
					CVideoEditorMenu::RebuildMenu();
				}

				CVideoEditorInstructionalButtons::Refresh();
			}

			break;
		}

#if defined(VIDEO_PLAYBACK_ENABLED) && VIDEO_PLAYBACK_ENABLED
#if !USE_ORBIS_UPLOAD
		case ME_STATE_ACTIVE_WAITING_FOR_VIDEO_UPLOAD_TEXT_INPUT:
			{
				if (!CWarningScreen::IsActive() && !CVideoEditorMenu::IsUserConfirmationScreenActive())
				{
					if (CControlMgr::UpdateVirtualKeyboard() != ioVirtualKeyboard::kKBState_PENDING)
					{
						if (CControlMgr::GetVirtualKeyboardState() == ioVirtualKeyboard::kKBState_SUCCESS)
						{
							const char *pKeyboardResult = CControlMgr::GetVirtualKeyboardResult();

							if (pKeyboardResult[0] != '\0' && pKeyboardResult[0] != ' ')
							{
								VideoUploadManager::GetInstance().ReturnStringResult(pKeyboardResult);
								ms_iState = ME_STATE_ACTIVE;
								CVideoEditorMenu::RebuildMenu();
							}
							else
							{
								VideoUploadManager::GetInstance().ReturnStringResult(NULL);
								ms_iState = ME_STATE_ACTIVE;
								CVideoEditorMenu::RebuildMenu();
							}
						}
						else  // exit back to active state (if we canceled or it failed for example)
						{
							VideoUploadManager::GetInstance().ReturnConfirmationResult(FALSE);
							ms_iState = ME_STATE_ACTIVE;
							CVideoEditorMenu::RebuildMenu();
						}

						CVideoEditorInstructionalButtons::Refresh();
					}

					UpdateInput();
				}

			}
			break;
#endif // !USE_ORBIS_UPLOAD
#endif

		case ME_STATE_INIT:
		{
			CVideoEditorTimeline::Init();
			CTextTemplate::Init();

			CVideoEditorMenu::Open();
			CVideoEditorInstructionalButtons::Refresh();
			CVideoEditorMenu::SetupStartingPanel("MAIN_MENU");

			InitCodeTextures();

			ms_iState = ME_STATE_SETUP;
			break;
		}

		case ME_STATE_SETUP:
		{
			ms_iState = ME_STATE_ACTIVE;  // extra frame of initialisation to allow complex objects set up in _INIT to flush
			break;
		}

        case ME_STATE_ACTIVE_WAITING_FOR_DECODER_MEM:
            {
                if( HasMinDecoderMemory() )
                {
                    
#if defined(VIDEO_PLAYBACK_ENABLED) && VIDEO_PLAYBACK_ENABLED
                        if( ms_VideoFileView && ms_refreshVideoViewOnMemBlockFree )
                        {
                            ms_VideoFileView->refresh(false);
                        }
#endif 
                        ms_iState = ME_STATE_ACTIVE;
                        ms_refreshVideoViewOnMemBlockFree = false;
                }
                else
                {
                    if( CReplayMgr::CanFreeUpBlocks() )
                    {
                        CReplayMgr::FreeUpBlocks();
                    }
                }

                break;
            }

		case ME_STATE_ACTIVE_WAITING_FOR_MENU_DATA:
		{
			if ( !IsInBlockingOperation() && !UpdateInput() )
			{
				CVideoEditorMenu::UpdatePane();
			}
			
			break;
		}

		case ME_STATE_ACTIVE:
		case ME_STATE_ACTIVE_TEXT_ADJUSTMENT:
		{
			if (CVideoEditorPlayback::IsActive())
			{
				CVideoEditorPlayback::Update();
			}
			else
			{
				UpdateInput();
				CVideoEditorTimeline::Update();
			}

			break;
		}

		case ME_STATE_WAITING_PROJECT_LOAD:
			{
				u64 const c_loadResult = CVideoEditorInterface::GetProjectLoadExtendedResultData();

				if( CVideoEditorInterface::HasLoadingProjectFailed() )
				{
					CVideoEditorInterface::ReleaseActiveProject();

					ms_project = NULL;
					ms_iState = ME_STATE_ACTIVE;

					// TODO - handle specific fail conditions here
					SetErrorState( "FAILED_TO_LOAD_PROJECT" );

					CVideoEditorMenu::ExitToMainMenu();
				}
				else if( !CVideoEditorInterface::IsLoadingProject() )
				{
					ms_project = CVideoEditorInterface::GetActiveProject();

					if( uiVerifyf( ms_project, "No active project after load? That's bad!" ) )
					{
						CVideoEditorMenu::ClearPreviewData( -1, true, false );

						bool const c_wasValidAfterLoad = ms_project->Validate();
						bool goStraightToTimeline = true;

						if( ms_project->GetClipCount() == 0 )
						{
							CVideoEditorMenu::SetUserConfirmationScreen( "VEUI_PROJ_EMPTY" );
							goStraightToTimeline = false;
						}
						else if( !c_wasValidAfterLoad )
						{
							CVideoEditorMenu::SetUserConfirmationScreen( "VEUI_PROJ_REPAIRED" );
						}
						else if( ( c_loadResult & CReplayEditorParameters::PROJECT_LOAD_RESULT_VERSION_UPGRADED ) != 0 )
						{
							CVideoEditorMenu::SetUserConfirmationScreen( "VEUI_PROJ_UPGRADED" );
						}
						else if( ( c_loadResult & CReplayEditorParameters::PROJECT_LOAD_RESULT_CONTENTS_MODIFIED ) != 0 )
						{
							// NOTE - We only show _one_ warning to the player, so be sure to order things here appropriately
							//	      based on priority of warning. Multi-state cases should be handled as their own blocks
							if( !CReplayEditorParameters::IsOnlyOneErrorSet( c_loadResult, CReplayEditorParameters::PROJECT_LOAD_RESULT_CONTENTS_REMOVED ) )
							{
								CVideoEditorMenu::SetUserConfirmationScreen( "VEUI_PROJ_TRUNCATED" );
							}
							else if( ( c_loadResult & CReplayEditorParameters::PROJECT_LOAD_RESULT_CLIPS_TRUNCATED ) != 0 )
							{
								CVideoEditorMenu::SetUserConfirmationScreen( "VEUI_PROJ_CLIPS_REMOVED" );
							}
							else if( ( c_loadResult & CReplayEditorParameters::PROJECT_LOAD_RESULT_CLIP_MARKERS_TRUNCATED ) != 0 )
							{
								CVideoEditorMenu::SetUserConfirmationScreen( "VEUI_PROJ_MARKERS_REMOVED" );
							}
							else if( ( c_loadResult & CReplayEditorParameters::PROJECT_LOAD_RESULT_TEXT_REMOVED ) != 0 )
							{
								CVideoEditorMenu::SetUserConfirmationScreen( "VEUI_PROJ_TEXT_REMOVED" );
							}
							else if( ( c_loadResult & CReplayEditorParameters::PROJECT_LOAD_RESULT_MUSIC_REMOVED ) != 0 )
							{
								CVideoEditorMenu::SetUserConfirmationScreen( "VEUI_PROJ_MUSIC_REMOVED" );
							}
						}

						CVideoEditorMenu::RefreshProjectHeading();

						if (goStraightToTimeline)
						{
							ShowTimeline();
						}

						RebuildTimeline();  // build the timeline from the new data
						CVideoEditorMenu::JumpPaneOnAction();
					}

					ms_iState = ME_STATE_ACTIVE;
				}

				break;
			}

		case ME_STATE_WAITING_PROJECT_GENERATION:
			{
				bool failed = false;
				if( CVideoEditorInterface::HasAutogenerationFailed() )
				{
					failed = true;
				}
				else if( CVideoEditorInterface::IsAutogeneratedProjectReady() )
				{
					ms_project = CVideoEditorInterface::GetActiveProject();
					if (ms_project)
					{
						CVideoEditorMenu::RefreshProjectHeading();

						ShowTimeline();

						RebuildTimeline();  

						CVideoEditorMenu::JumpPaneOnAction();

						ms_iState = ME_STATE_ACTIVE;
					}
					else
					{
						failed = true;
					}
				}

				if( failed )
				{
					CVideoEditorInterface::ReleaseActiveProject();
					ms_project = NULL;
					ms_iState = ME_STATE_ACTIVE;
					SetErrorState( "FAILED_TO_AUTO_PROJECT" );
					CVideoEditorMenu::ExitToMainMenu();
				}

				break;
			}

		case ME_STATE_LOADING:
		{
			bool bFoundTxd = false;
			const strLocalIndex iTxdId = g_TxdStore.FindSlot(VIDEO_EDITOR_CODE_TXD);

			if (iTxdId != -1)
			{
				if (g_TxdStore.HasObjectLoaded(iTxdId))
				{
					bFoundTxd = true;
				}
			}

			const bool bCreatedAllFileViews = CreateFileViews();

			if ( CScaleformMgr::IsMovieActive(GetMovieId()) && CVideoEditorInstructionalButtons::IsActive() &&
#if USE_TEXT_CANVAS
				CScaleformMgr::IsMovieActive(CTextTemplate::GetMovieId()) && 
#endif // USE_TEXT_CANVAS
				IsAudioTrackTextLoaded() && bFoundTxd && bCreatedAllFileViews)
			{
				ms_iState = ME_STATE_INIT;
			}

			break;
		}

		default:
		{
			uiAssertf(0, "CVideoEditorMenu - Invalid state in update (%d)", (s32)ms_iState);
			return;
		}
	}

	if (ms_iState == ME_STATE_ACTIVE || ms_iState == ME_STATE_ACTIVE_TEXT_ADJUSTMENT)  // only update instructional buttons when in active state
	{
		CVideoEditorInstructionalButtons::Update();
	}
}

bool CVideoEditorUi::FindProjectNameInUse(const char *pName)
{
	bool const c_nameUsed =  ms_ProjectFileView && ms_ProjectFileView->isPopulated() ? ms_ProjectFileView->doesFileExistInProvider( pName, false, CVideoEditorInterface::GetActiveUserId() ) : false;
	return c_nameUsed;
}

void CVideoEditorUi::SetErrorState(const char *pHashString, const char* literalString, const char* literalString2 )
{
	CVideoEditorMenu::SetErrorState(pHashString,literalString,literalString2);
}

void CVideoEditorUi::ToggleVideoPlaybackPaused()
{
#if defined(VIDEO_PLAYBACK_ENABLED) && VIDEO_PLAYBACK_ENABLED
	if( VideoPlayback::IsValidHandle( ms_activePlaybackHandle ) && !VideoPlayback::IsAtEndOfVideo( ms_activePlaybackHandle ) )
	{
		bool const c_paused = VideoPlayback::IsPaused( ms_activePlaybackHandle );
		char const * const audioTrigger = c_paused ?
			"RESUME" : "PAUSE_FIRST_HALF";

		g_FrontendAudioEntity.PlaySound( audioTrigger, "HUD_FRONTEND_DEFAULT_SOUNDSET" );

		if( c_paused )
		{
			VideoPlayback::PlayVideo( ms_activePlaybackHandle );
		}
		else
		{
			VideoPlayback::PauseVideo( ms_activePlaybackHandle );
		}
	}
#endif
}

void CVideoEditorUi::SeekToVideoPlaybackStart()
{
#if defined(VIDEO_PLAYBACK_ENABLED) && VIDEO_PLAYBACK_ENABLED
	if( VideoPlayback::IsValidHandle( ms_activePlaybackHandle ) )
	{
		VideoPlayback::Seek( ms_activePlaybackHandle, 0 );
	}
#endif
}

void CVideoEditorUi::EndFullscreenVideoPlayback()
{
#if defined(VIDEO_PLAYBACK_ENABLED) && VIDEO_PLAYBACK_ENABLED
#if RSG_ORBIS
	if (VideoPlayback::IsPlaybackActive())
#endif
	{
		VideoPlayback::CleanupVideo( ms_activePlaybackHandle );
		CPauseMenu::TogglePauseRenderPhases(true, OWNER_VIDEO, __FUNCTION__ );
	}
#endif
}

void CVideoEditorUi::TriggerAdditionalExportPrompt()
{
	CVideoEditorMenu::SetUserConfirmationScreenWithAction( "TRIGGER_PUBLISH", "VE_EXP_PT2", "VE_EXPORT", FE_WARNING_OK_CANCEL, false, ms_literalBuffer.getBuffer() );
}

void CVideoEditorUi::TriggerOverwriteUnavailablePrompt( char const * const filename )
{
	CVideoEditorMenu::SetUserConfirmationScreenWithAction( "TRIGGER_OVRW_UNAVAIL", "VE_EXP_OVRWU", "VEUI_HDR_ALERT", FE_WARNING_OK, false, filename ? filename : "" );
}

void CVideoEditorUi::TriggerExport()
{
	CVideoEditorProject* project = CVideoEditorInterface::GetActiveProject();
	if (project)
	{
		if( project->AreAllClipFilesValid() )
		{
			CVideoEditorPlayback::Open( PLAYBACK_TYPE_BAKE );
			SetActionEventsOnGameDestruction();
		}
		else
		{
			CVideoEditorMenu::SetUserConfirmationScreen( "VE_EXP_CORRUPT_CLIPS", "VEUI_HDR_ALERT");
		}
	}

}

bool CVideoEditorUi::TriggerNewBlank()
{
	CVideoEditorMenu::ClearPreviewData( -1, true, false );

	char cNewProjFilename[ CVideoEditorInterface::sc_nameBufferSize ];
	if( ms_ProjectFileView &&
		CVideoEditorInterface::GenerateUniqueProjectName( cNewProjFilename, *ms_ProjectFileView ) &&
		CVideoEditorInterface::GrabNewProject(cNewProjFilename, ms_project) )
	{
		CVideoEditorMenu::RefreshProjectHeading();
		SetProjectEdited( false );
		CVideoEditorTimeline::SetPlayheadToStartOfNearestValidClip();
		
		return true;
	}
	else
	{
		SetErrorState("FAILED_TO_CREATE_PROJECT");

		return false;
	}
}

bool CVideoEditorUi::TriggerLoad(const u32 uProjectIndex)
{
	bool loadRequested = false;

	if( ms_ProjectFileView )
	{
		if( ms_ProjectFileView->IsValidProjectReference( uProjectIndex ) )
		{
			char pathBuffer[RAGE_MAX_PATH];
			if( ms_ProjectFileView->getFilePath(uProjectIndex, pathBuffer))
			{
				if( CVideoEditorInterface::StartLoadProject( pathBuffer ) )
				{
					SetProjectEdited( false );

					ShowTimeline();

					ms_iState = ME_STATE_WAITING_PROJECT_LOAD;
					loadRequested = true;
				}
			}
		}
	}

	if (!loadRequested)
	{
		SetErrorState("FAILED_TO_LOAD_PROJECT");
	}

	return loadRequested;
}

void CVideoEditorUi::TriggerSaveAs( const bool bExit )
{
    bool const c_hadValidName = ms_project ? ms_project->SetName( ms_textBuffer.getBuffer() ) : false;
    if( c_hadValidName )
	{
        TriggerSave( bExit );
    }
    else
    {
        ms_literalBuffer.Set( ms_textBuffer.getBuffer() );
        SetErrorState( "PROJECT_NAME_INVALID", ms_literalBuffer.getBuffer() );	
    }
}

void CVideoEditorUi::TriggerSave(const bool bExit)
{
	SaveProject( true );
	if (bExit)
	{
		CVideoEditorMenu::ExitToMainMenu();
	}
}


void CVideoEditorUi::SetupFilenameEntry()
{
	static char16 s_TitleForKeyboardUI[100];
	static char16 s_InitialValueForKeyboard[MAX_FILEDATA_INPUT_FILENAME_BUFFER_SIZE];

	Utf8ToWide(s_TitleForKeyboardUI, TheText.Get("VEUI_ENTER_FILENAME"), 100);

	char cFilename[MAX_FILEDATA_INPUT_FILENAME_BUFFER_SIZE];
	cFilename[0] = '\0';

	if( ms_project )
	{
		safecpy(cFilename, ms_project->GetName(), NELEM(cFilename));
	}

	Utf8ToWide(s_InitialValueForKeyboard, cFilename, NELEM(cFilename));

	ioVirtualKeyboard::Params params;
	params.m_KeyboardType = ioVirtualKeyboard::kTextType_FILENAME;
	params.m_MaxLength = MAX_FILEDATA_INPUT_FILENAME_LENGTH_WITH_TERMINATOR;
	params.m_Title = s_TitleForKeyboardUI;
	params.m_InitialValue = s_InitialValueForKeyboard;
	ms_textBuffer.Clear();

	CControlMgr::ShowVirtualKeyboard(params);
	CVideoEditorInstructionalButtons::Refresh();

	ms_iState = ME_STATE_ACTIVE_WAITING_FOR_FILENAME;
}

void CVideoEditorUi::SetupExportFilenameEntry()
{
	static char16 s_TitleForKeyboardUI[100];
	static char16 s_InitialValueForKeyboard[ MAX_FILEDATA_INPUT_FILENAME_BUFFER_SIZE ];

	Utf8ToWide(s_TitleForKeyboardUI, TheText.Get("VEUI_ENTER_EXPORTNAME"), 100);

	char cFilename[MAX_FILEDATA_INPUT_FILENAME_BUFFER_SIZE];
	cFilename[0] = '\0';

	if( ms_project )
	{
		safecpy(cFilename, ms_project->GetName(), NELEM(cFilename));
	}

	Utf8ToWide(s_InitialValueForKeyboard, cFilename, NELEM(cFilename));

	ioVirtualKeyboard::Params params;
	params.m_KeyboardType = ioVirtualKeyboard::kTextType_FILENAME;
	params.m_MaxLength = MAX_FILEDATA_INPUT_FILENAME_LENGTH_WITH_TERMINATOR;
	params.m_Title = s_TitleForKeyboardUI;
	params.m_InitialValue = s_InitialValueForKeyboard;
	ms_textBuffer.Clear();
    ms_exportNameBuffer.Clear();

	CControlMgr::ShowVirtualKeyboard(params);
	CVideoEditorInstructionalButtons::Refresh();

	ms_iState = ME_STATE_ACTIVE_WAITING_FOR_EXPORT_FILENAME;
}

void CVideoEditorUi::SetupTextEntry(const char* title, bool clearKeyboard)
{
	sEditedTextProperties &editedText = CTextTemplate::GetEditedText();
	sEditedTextProperties &editedTextBackup = CTextTemplate::GetEditedTextBackup();

	editedTextBackup = editedText;

	static char16 s_TitleForKeyboardUI[100];
	static char16 s_InitialValueForKeyboard[CMontageText::MAX_MONTAGE_TEXT];

	if (!title)
		Utf8ToWide(s_TitleForKeyboardUI, TheText.Get("VEUI_ENTER_TEXT"), 100);
	else
		Utf8ToWide(s_TitleForKeyboardUI, TheText.Get(title), 100);

	if (clearKeyboard)
		editedText.m_text[0] = '\0';

	Utf8ToWide(s_InitialValueForKeyboard, editedText.m_text, CMontageText::MAX_MONTAGE_TEXT);

	ioVirtualKeyboard::Params params;
	params.m_KeyboardType = ioVirtualKeyboard::kTextType_ALPHABET;
	params.m_MaxLength = CMontageText::MAX_MONTAGE_TEXT;
	params.m_Title = s_TitleForKeyboardUI;
	params.m_InitialValue = s_InitialValueForKeyboard;
	ms_textBuffer.Clear();

	CControlMgr::ShowVirtualKeyboard(params);
	CVideoEditorInstructionalButtons::Refresh();

	ActivateStageClipTextOverlay(true);

	ms_iState = ME_STATE_ACTIVE_WAITING_FOR_TEXT_INPUT;
}

bool CVideoEditorUi::CheckExportDuration()
{
    bool withinLimits = true;

    float const c_totalClipTimeMs = ms_project ? ms_project->GetTotalClipTimeMs() : 0;
    float const c_maxClipTimeMs = ms_project ? ms_project->GetMaxDurationMs() : 0;

    const float minLengthOfExportMs = 1000; // can't encode if less than a second

    if( c_totalClipTimeMs < minLengthOfExportMs ) 
    {
        CVideoEditorMenu::SetUserConfirmationScreen( "VE_EXP_TOO_SHORT", "VEUI_HDR_ALERT");
        withinLimits = false;
    }
    else if( c_totalClipTimeMs >= c_maxClipTimeMs )
    {
        CVideoEditorMenu::SetUserConfirmationScreen( "VE_EXP_TOO_LONG", "VEUI_HDR_ALERT");
        withinLimits = false;
    }

    return withinLimits;
}

void CVideoEditorUi::CheckExportLimits()
{
    if( CheckExportDuration() )
    {
        float const c_totalClipTimeMs = ms_project ? ms_project->GetTotalClipTimeMs() : 0;
        u32 const c_musicTrackHash = ms_project ? ms_project->GetHashOfFirstInvalidMusicTrack() : 0;
        bool const c_containsBothMusicAndCommercial = ms_project ? ms_project->GetMusicClipMusicOnlyCount() > 0 && ms_project->GetMusicClipCommercialsOnlyCount() > 0 : false;
        bool const c_containsCommercialsAndTooLong = ms_project ? ms_project->GetMusicClipCommercialsOnlyCount() > 0 && ms_project->GetTotalClipTimeMs() > 10*60*1000 : false;

        if( c_containsBothMusicAndCommercial )
        {
            CVideoEditorMenu::SetUserConfirmationScreen( "VE_EXP_COMMER_MUSIC", "VEUI_HDR_ALERT");
        }
        else if( c_containsCommercialsAndTooLong )
        {
            CVideoEditorMenu::SetUserConfirmationScreen( "VE_EXP_COMMER_LENGTH", "VEUI_HDR_ALERT");
        }
        else if( c_musicTrackHash != 0 )
        {
            LocalizationKey key;
            CReplayAudioTrackProvider::GetMusicTrackNameKey( REPLAY_RADIO_MUSIC, c_musicTrackHash, key.getBufferRef() );

            ms_literalBuffer.Set( TheText.Get( key.getBuffer() ) );
            ms_literalBuffer2.Set( TheText.Get( key.getBuffer() ) );
            CVideoEditorMenu::SetUserConfirmationScreen( "VE_EXP_MUSIC_SHORT", "VEUI_HDR_ALERT", FE_WARNING_OK, false, ms_literalBuffer.getBuffer(), ms_literalBuffer2.getBuffer() );
        }
        else
        {
            size_t sizeNeeded = 0;
            size_t sizeAvailable = 0;

            // TODO - Doesn't account for overwriting files
            if( CVideoEditorInterface::IsSpaceAvailableToBake( c_totalClipTimeMs, sizeNeeded, sizeAvailable ) )
            {
                uiDisplayf( "CVideoEditorUi::CheckExportSizeLimits - Exporting video of size %" SIZETFMT "u bytes.", sizeNeeded );
                CVideoEditorUi::SetupExportFilenameEntry();
                // pass quality settings onto encoder
                VideoRecording::SetExportFps((MediaEncoderParams::eOutputFps)CVideoEditorUi::GetExportProperties().m_fps);
                VideoRecording::SetExportQuality((MediaEncoderParams::eQualityLevel)CVideoEditorUi::GetExportProperties().m_quality);
#if RSG_PC
                // some video metadata requires to have estimates passed in for later verification checks
                VideoRecording::SetMetadataInfo( (u32)c_totalClipTimeMs );
#endif
            }
            else
            {
                // Not enough space!
                CVideoEditorUi::UpdateLiteralStringAsSize( sizeNeeded, sizeAvailable );
                CVideoEditorMenu::SetUserConfirmationScreen( "VE_EXP_SPACE", "VEUI_HDR_ALERT", FE_WARNING_OK, false, 
                    GetLiteralStringBuffer(), GetLiteralStringBuffer2() );
            }
        }
    }
}

void CVideoEditorUi::SetFreeReplayBlocksNeeded()
{
    ms_iState = ME_STATE_ACTIVE_WAITING_FOR_DECODER_MEM;
}

#if defined(VIDEO_PLAYBACK_ENABLED) && VIDEO_PLAYBACK_ENABLED
#if !USE_ORBIS_UPLOAD
void CVideoEditorUi::SetupVideoUploadTextEntry(const char* title, u32 maxStringLength, const char* initialValue)
{
	static char16 s_TitleForKeyboardUI[VideoUploadTextInfo::MAX_VIDEOUPLOAD_TEXT];
	
	if (maxStringLength > VideoUploadTextInfo::MAX_VIDEOUPLOAD_TEXT)
		maxStringLength = VideoUploadTextInfo::MAX_VIDEOUPLOAD_TEXT;
	static char16 s_InitialValueForKeyboard[VideoUploadTextInfo::MAX_VIDEOUPLOAD_TEXT];

	Utf8ToWide(s_TitleForKeyboardUI, TheText.Get(title), VideoUploadTextInfo::MAX_VIDEOUPLOAD_TEXT);

	if (initialValue)
		Utf8ToWide(s_InitialValueForKeyboard, initialValue, VideoUploadTextInfo::MAX_VIDEOUPLOAD_TEXT);
	else
		s_InitialValueForKeyboard[0] ='\0';

	ioVirtualKeyboard::Params params;
	params.m_KeyboardType = ioVirtualKeyboard::kTextType_FILENAME;
	params.m_MaxLength = maxStringLength;
	params.m_Title = s_TitleForKeyboardUI;
	params.m_InitialValue = s_InitialValueForKeyboard;
//	ms_textBuffer.Clear();

	CControlMgr::ShowVirtualKeyboard(params);
	CVideoEditorInstructionalButtons::Refresh();
	CVideoEditorInstructionalButtons::Update();

	ms_iState = ME_STATE_ACTIVE_WAITING_FOR_VIDEO_UPLOAD_TEXT_INPUT;
}
#endif // !USE_ORBIS_UPLOAD

void CVideoEditorUi::BeginFullscreenVideoPlayback(const s32 iIndex)
{
	bool startedPlayback = false;

	if( VideoPlayback::IsPlaybackInstanceAvailabile() )
	{
		if( ms_VideoFileView && !VideoPlayback::IsValidHandle( ms_activePlaybackHandle ) )
		{
			if( ms_VideoFileView->IsValidVideoReference( iIndex ) )
			{
				int const c_fileIndex = iIndex;
				if ( c_fileIndex < ms_VideoFileView->getFilteredFileCount() )
				{
                    // PS4/Xb1 media decoder needs the memory from the replay allocator
                    // Doing it on all platforms though for consistency.
                    CReplayMgr::FreeUpBlocks();

					char pathBuffer[RAGE_MAX_PATH];
					ms_VideoFileView->getFilePath( c_fileIndex, pathBuffer );
					ms_activePlaybackHandle = VideoPlayback::PlayVideo( pathBuffer );
					CVideoEditorInstructionalButtons::Refresh();
					CPauseMenu::TogglePauseRenderPhases(false, OWNER_VIDEO, __FUNCTION__ );
					startedPlayback = true;
				} 
			}
		}

		if( !startedPlayback )
		{
			SetErrorState("FAILED_TO_PLAY_VIDEO");
		}
	}
}

bool CVideoEditorUi::IsPlaybackPaused()
{
	bool const c_result = VideoPlayback::IsPaused( ms_activePlaybackHandle );
	return c_result;
}

#endif // defined(VIDEO_PLAYBACK_ENABLED) && VIDEO_PLAYBACK_ENABLED



void CVideoEditorUi::TriggerNewText()
{
	ms_textBuffer.Clear();

	CTextTemplate::DefaultText();

	EditTextDuration(0); // means it will cap at the max it can be for the current selected clip

	// limiting text to 3 seconds as JMac requested in url:bugstar:2231153
	if(CTextTemplate::GetEditedText().m_durationMs > DEFAULT_TEXT_DURATION_MS)
		CTextTemplate::GetEditedText().m_durationMs = DEFAULT_TEXT_DURATION_MS;

	ActivateStageClipTextOverlay(true);
}



void CVideoEditorUi::PickupSelectedClip(const u32 uProjectIndex, const bool bDeleteOriginal)
{
	CVideoEditorTimeline::SetMaxPreviewAreaMs(0);

	switch (CVideoEditorTimeline::GetItemSelected())
	{
		case PLAYHEAD_TYPE_VIDEO:
		{
			u32 uMinimumClipCount = bDeleteOriginal ? 1 : 0;

			if ( ms_project && ms_project->GetClipCount() > uMinimumClipCount)
			{
				if( ms_project->PrepareStagingClip( uProjectIndex ) )
				{
					ms_iState = ME_STATE_ACTIVE_WAITING_FOR_STAGING_CLIP;

					if (bDeleteOriginal)
					{
						CVideoEditorTimeline::SetMaxPreviewAreaMs(0);
						CVideoEditorTimeline::ClearAllStageClips(true);
						CVideoEditorTimeline::ReleaseThumbnail( uProjectIndex );
						CVideoEditorTimeline::DeleteVideoClipFromTimeline(uProjectIndex);  // must delete from timeline before proj as we integrate data

						ms_project->DeleteClip(uProjectIndex);

						CVideoEditorTimeline::RebuildAnchors();  // fix for 2063560 - remove and rebuild anchors when we delete a clip
					}
				}
			}

			break;
		}

		case PLAYHEAD_TYPE_AMBIENT:
		{
			CAmbientClipHandle const &clip = ms_project->GetAmbientClip(uProjectIndex);

			if (clip.IsValid())
			{
				ms_SelectedAudio.m_soundHash = clip.getClipSoundHash();
				ms_SelectedAudio.m_durationMs = clip.getTrimmedDurationMs();
				ms_SelectedAudio.m_startOffsetMs = clip.getStartOffsetMs();
				ms_SelectedAudio.m_isScoreTrack = false;

				CVideoEditorTimeline::AddAmbientToPlayheadPreview();

				if (bDeleteOriginal)
				{
					CVideoEditorTimeline::SetMaxPreviewAreaMs(clip.getStartTimeWithOffsetMs() + clip.getTrimmedDurationMs());
					CVideoEditorTimeline::DeleteAmbientClipFromTimeline(uProjectIndex);
					ms_project->DeleteAmbientClip(uProjectIndex);
				}
			}

			break;
		}

		case PLAYHEAD_TYPE_AUDIO:
		{
			CMusicClipHandle const &music = ms_project->GetMusicClip(uProjectIndex);

			if (music.IsValid())
			{
				ms_SelectedAudio.m_soundHash = music.getClipSoundHash();
				ms_SelectedAudio.m_durationMs = music.getTrimmedDurationMs();
				ms_SelectedAudio.m_startOffsetMs = music.getStartOffsetMs();
				ms_SelectedAudio.m_isScoreTrack = audRadioTrack::IsScoreTrack(music.getClipSoundHash());

				CVideoEditorTimeline::AddAudioToPlayheadPreview();

				if (bDeleteOriginal)
				{
					CVideoEditorTimeline::SetMaxPreviewAreaMs(music.getStartTimeWithOffsetMs() + music.getTrimmedDurationMs());
					CVideoEditorTimeline::DeleteMusicClipFromTimeline(uProjectIndex);
					ms_project->DeleteMusicClip(uProjectIndex);
				}
			}

			break;
		}

		case PLAYHEAD_TYPE_TEXT:
		{
			CTextOverlayHandle const &text = ms_project->GetOverlayText(uProjectIndex);

			if (text.IsValid())
			{
				sEditedTextProperties &editedText = CTextTemplate::GetEditedText();

				editedText.m_position = text.getTextPosition( 0 );
				editedText.m_scale = text.getTextScale( 0 ).y;
				editedText.m_colorRGBA = text.getTextColor( 0 );
				editedText.m_style = text.getTextStyle( 0 );
				editedText.m_durationMs = text.getDurationMs();
				safecpy(editedText.m_text, text.getText( 0 ), NELEM(editedText.m_text));

				CVideoEditorTimeline::AddTextToPlayheadPreview();

				if (bDeleteOriginal)
				{
					CVideoEditorTimeline::SetMaxPreviewAreaMs(text.getStartTimeMs() + text.getDurationMs());
					CVideoEditorTimeline::DeleteTextClipFromTimeline(uProjectIndex);
					ms_project->DeleteOverlayText(uProjectIndex);
				}
			}

			break;
		}

		default:
		{
			uiAssertf(0, "CVideoEditorUi::PickupSelectedClip() - invalid type passed (%d)", (s32)CVideoEditorTimeline::GetItemSelected());
		}
	}

	u32 const c_totalTimeMs = (u32)GetProject()->GetTotalClipTimeMs();  // ensure playhead is not over end of project space after picking up a clip
	if (CVideoEditorTimeline::GetPlayheadPositionMs() > c_totalTimeMs)
	{
		CVideoEditorTimeline::SetPlayheadPositionMs(c_totalTimeMs, true);
	}

	SetProjectEdited(true);  // flag as edited

	g_FrontendAudioEntity.PlaySound( "SELECT", "HUD_FRONTEND_DEFAULT_SOUNDSET" );
}



void CVideoEditorUi::DeleteSelectedClip()
{
	const s32 iProjectIndex = CVideoEditorTimeline::GetProjectIndexAtTime(CVideoEditorTimeline::GetItemSelected(), CVideoEditorTimeline::GetPlayheadPositionMs());

	if (iProjectIndex != -1)
	{
		if (CVideoEditorTimeline::GetItemSelected() == PLAYHEAD_TYPE_VIDEO)
		{
			CVideoEditorTimeline::ClearAllStageClips(true);

			CVideoEditorTimeline::ReleaseThumbnail((u32)iProjectIndex);

			CVideoEditorTimeline::DeleteVideoClipFromTimeline((u32)iProjectIndex);  // must delete from timeline before proj as we integrate data

			ms_project->DeleteClip((u32)iProjectIndex);

			if (ms_project->GetClipCount() == 0 && ms_project->GetMusicClipCount() == 0 && ms_project->GetOverlayTextCount() == 0)  // fully cleanup up timeline if we have no clips in project after a delete
			{
				if (CVideoEditorUi::ShowOptionsMenu())
				{
					if (!CMousePointer::HasMouseInputOccurred())
					{
						CVideoEditorMenu::SetItemActive(0);
						CVideoEditorMenu::UpdatePane();
					}
				}

				CVideoEditorTimeline::ClearAllClips();
				CVideoEditorTimeline::SetPlayheadPositionMs(0, false);
			}
		}

		if (CVideoEditorTimeline::GetItemSelected() == PLAYHEAD_TYPE_AMBIENT)
		{
			CVideoEditorTimeline::DeleteAmbientClipFromTimeline((u32)iProjectIndex);

			ms_project->DeleteAmbientClip(iProjectIndex);
		}

		if (CVideoEditorTimeline::GetItemSelected() == PLAYHEAD_TYPE_AUDIO)
		{
			CVideoEditorTimeline::DeleteMusicClipFromTimeline((u32)iProjectIndex);

			ms_project->DeleteMusicClip(iProjectIndex);
		}

		if (CVideoEditorTimeline::GetItemSelected() == PLAYHEAD_TYPE_TEXT)
		{
			CVideoEditorTimeline::DeleteTextClipFromTimeline((u32)iProjectIndex);

			ms_project->DeleteOverlayText(iProjectIndex);
		}

		CVideoEditorTimeline::SetPlayheadToStartOfNearestValidClip();

		CVideoEditorTimeline::RepositionPlayheadIfNeeded(true);

		CVideoEditorInstructionalButtons::Refresh();

		SetProjectEdited(true);  // flag as edited

		CVideoEditorTimeline::UpdateClipsOutsideProjectTime();
	}
}

// we need to call this in update, so we can check against if replay mode is ready for it
void CVideoEditorUi::SetActionEventsOnGameDestruction()
{
	ms_actionEventsOnGameDestruction = true;
}

// anything ui related that needs to happen on a game destruction event can go in here
void CVideoEditorUi::ActionEventsOnGameDestruction()
{
	SetUserWarnedOfGameDestroy(true);  // flag as 'been warned' atleast once

	if( CPauseMenu::IsActive(PM_SkipVideoEditor) && !CPauseMenu::IsClosingDown() ) // if pausemenu is active, then close it as we cant go back to the pausemenu now as game session will restart when we exit replay editor
	{
		CPauseMenu::Close();  // close the pausemenu behind
		fwTimer::StartUserPause();  // keep the game paused throughout the editor
	}
}



void CVideoEditorUi::TriggerAddClipToTimeline(const u32 uProjectIndex)
{
	if (uiVerifyf(ms_project, "No project active to add a clip to!"))
	{
		if (ms_clipFileView)
		{
			int const c_clipIndex = uProjectIndex;
			if (c_clipIndex < ms_clipFileView->getFilteredFileCount())
			{
				CThumbnailFileView *pThumbnailView = (CThumbnailFileView*)ms_clipFileView;

				if (pThumbnailView)
				{
					char pathBuffer[RAGE_MAX_PATH];
					pThumbnailView->getFileName(c_clipIndex, pathBuffer);

					u64 const ownerId = pThumbnailView->getOwnerId( c_clipIndex );

					if( ms_project->PrepareStagingClip( pathBuffer, ownerId ) )
					{
						ms_iState = ME_STATE_ACTIVE_WAITING_FOR_STAGING_CLIP;

						SetProjectEdited(true);  // flag as edited
					}
					else
					{
						SetErrorState( "FAILED_TO_ADD_CLIP" );
					}
				}
			}
		}
	}
}


void CVideoEditorUi::TriggerAddAmbientToTimeline()
{
	if (uiVerifyf(ms_project, "No project active to add a clip to!"))
	{
		if (ms_project->GetClipCount() != 0)
		{
			CVideoEditorTimeline::AddAmbientToPlayheadPreview();

			CVideoEditorMenu::GoBackOnePane();

			SetProjectEdited(true);  // flag as edited
		}
	}
}


void CVideoEditorUi::TriggerAddAudioToTimeline()
{
	if (uiVerifyf(ms_project, "No project active to add a clip to!"))
	{
		if (ms_project->GetClipCount() != 0)
		{
			CVideoEditorTimeline::AddAudioToPlayheadPreview();

			CVideoEditorMenu::GoBackOnePane();

			SetProjectEdited(true);  // flag as edited
		}
	}
}


void CVideoEditorUi::TriggerAddTextToTimeline()
{
	if (uiVerifyf(ms_project, "No project active to add a clip to!"))
	{
		if (ms_project->GetClipCount() != 0)
		{
			CVideoEditorTimeline::AddTextToPlayheadPreview();

// 			// move cursor to the nearest position that this item can be dropped:
//  			u32 positionMs = CVideoEditorTimeline::GetPlayheadPositionMs();
//  			positionMs = CVideoEditorTimeline::AdjustToNearestDroppablePosition(positionMs);
//  			CVideoEditorTimeline::SetPlayheadPositionMs(positionMs, true);

			SetProjectEdited(true);  // flag as edited
		}
	}
}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorMenu::AttemptToReplaceEditedTextWithNewText
// PURPOSE:	if text is valid and still physically fits in space, then drop it on the
//			timeline, otherwise we warn the user and give them the choice to re-edit
//			or to re-position
/////////////////////////////////////////////////////////////////////////////////////////
void CVideoEditorUi::AttemptToReplaceEditedTextWithNewText()
{
	const s32 iProjectIndex = CTextTemplate::GetTextIndexEditing();

	CTextOverlayHandle const &TextClip = GetProject()->GetOverlayText(iProjectIndex);

	u32 uPositionMs = CVideoEditorTimeline::GetPlayheadPositionMs();
	bool bDropAtOriginalPostion = true;

	if (TextClip.IsValid())
	{
		uPositionMs = TextClip.getStartTimeMs();
	}
	else
	{
		bDropAtOriginalPostion = false;
	}

	if (!CVideoEditorTimeline::PlaceEditedTextOntoTimeline(uPositionMs))
	{
		bDropAtOriginalPostion = false;
	}

	if (bDropAtOriginalPostion)
	{
		CVideoEditorMenu::GoBackOnePane();

		CTextTemplate::SetTextIndexEditing(-1);

		// if editing existing text, then just drop it where the edited text was:
		CVideoEditorTimeline::DeleteTextClipFromTimeline((u32)iProjectIndex);
		GetProject()->DeleteOverlayText(iProjectIndex);

		SetProjectEdited(true);  // flag as edited

		ShowTimeline();
	}
	else
	{
		CVideoEditorMenu::SetUserConfirmationScreenWithAction( "TRIGGER_ADD_TEXT_TO_TIMELINE", "VEUI_EDIT_CLIP_DOESNT_FIT" );
	}
}



void CVideoEditorUi::TriggerDirectorMode()
{
	ms_directorModeLaunched = true;

	// exit video editor, not via pausemenu
	Close();

	if (CPauseMenu::IsActive(PM_SkipVideoEditor) && !CPauseMenu::IsClosingDown())
	{
		CPauseMenu::Close(); // ensure pausemenu is also shut down if its active
	}

	camInterface::FadeOut(0);  // set on a faded out screen, waiting for script to kick in after they poll HAS_DIRECTOR_MODE_BEEN_LAUNCHED_BY_CODE
}


void CVideoEditorUi::TriggerFullScreenPreview()
{
	CVideoEditorProject* project = CVideoEditorInterface::GetActiveProject();
	if (project)
	{
		if (project->AreAllClipFilesValid())
		{
			ActionEventsOnGameDestruction();
			CReplayMgr::StopAllSounds();
			audController::GetController()->ClearAllSounds();	// removed from UnfreezeAudio(), but it's needed here
			CVideoEditorPlayback::Open( PLAYBACK_TYPE_PREVIEW_FULL_PROJECT );
		}
		else
		{
			CVideoEditorMenu::SetUserConfirmationScreen( "VE_EXP_CORRUPT_CLIPS", "VEUI_HDR_ALERT");
		}
	}
}


bool CVideoEditorUi::TriggerPreviewClip( u32 const clipIndex )
{
	bool previewStarted = false;

	if( CVideoEditorMenu::IsOnClipManagementMenu() )
	{
		if (ms_clipFileView)
		{
			if( ms_clipFileView->IsValidClipReference( clipIndex ) )
			{
				char pathBuffer[RAGE_MAX_PATH];
				if( ms_clipFileView->getFileName(clipIndex, pathBuffer))
				{
					u64 const ownerId = ms_clipFileView->getOwnerId( clipIndex );
					if( CVideoEditorInterface::GrabClipPreviewProject( ownerId, pathBuffer, ms_project ) )
					{
						ActionEventsOnGameDestruction();
                        CReplayMgr::StopAllSounds();
                        audController::GetController()->ClearAllSounds();	// removed from UnfreezeAudio(), but it's needed here
						CVideoEditorPlayback::Open( PLAYBACK_TYPE_RAW_CLIP_PREVIEW, 0 );
						previewStarted = true;
					}
				}
			}
		}

		if (!previewStarted)
		{
			SetErrorState("FAILED_TO_PREVIEW_CLIP");
			CVideoEditorInterface::ReleaseProject( ms_project );
		}
	}

	return previewStarted;
}


void CVideoEditorUi::TriggerEditClip()
{
	const u32 uPlayheadPos = CVideoEditorTimeline::GetPlayheadPositionMs();
	const s32 iProjectIndex = CVideoEditorTimeline::GetProjectIndexAtTime(CVideoEditorTimeline::GetItemSelected(), uPlayheadPos);

	if (iProjectIndex != -1)
	{
		switch (CVideoEditorTimeline::GetItemSelected())
		{
			case PLAYHEAD_TYPE_VIDEO:
			{
				if ( ms_project->GetClipCount() > 0 && ms_project->IsClipReferenceValid( iProjectIndex ) )
				{
					ActionEventsOnGameDestruction();
					CReplayMgr::StopAllSounds();
					audController::GetController()->ClearAllSounds();	// removed from UnfreezeAudio(), but it's needed here
					CVideoEditorPlayback::Open( PLAYBACK_TYPE_EDIT_CLIP, iProjectIndex );
				}

				break;
			}

			case PLAYHEAD_TYPE_AMBIENT:
			case PLAYHEAD_TYPE_AUDIO:
			{
				// TODO? - edit audio clip
				break;
			}

			case PLAYHEAD_TYPE_TEXT:
			{
#if USE_OLD_TEXT_EDITOR
				if (ShowMenu("TEXT_SELECTION_ORIGINAL"))
#else
				if (ShowMenu("TEXT_TEMPLATE_ADJUSTMENT"))
#endif
				{
					if (CTextTemplate::GetTextIndexEditing() == -1)
					{
						// copy this text index data into the editing text structure for editing
						CTextOverlayHandle const &text = ms_project->GetOverlayText((u32)iProjectIndex);

						if (text.IsValid())
						{
							sEditedTextProperties &editedText = CTextTemplate::GetEditedText();
							editedText.m_position = text.getTextPosition(0);
							editedText.m_scale = text.getTextScale(0).y;
							editedText.m_colorRGBA = text.getTextColor(0);

							Color32 comparisonColor = editedText.m_colorRGBA;  // these hud colours have 255 as the alpha so always use that for the comparison
							comparisonColor.SetAlpha(255);
							editedText.m_hudColor = CHudColour::GetColourFromRGBA(comparisonColor);

							editedText.m_alpha = editedText.m_colorRGBA.GetAlphaf();
							editedText.m_style = text.getTextStyle(0);
							editedText.m_durationMs = text.getDurationMs();
							editedText.m_template.Clear();//.SetHash(text.getTemplate());
							editedText.m_positionType = 0;//text.templatePositionType;
							safecpy(editedText.m_text, text.getText(0), NELEM(editedText.m_text));
						}
					}

					CTextTemplate::SetTextIndexEditing(iProjectIndex);  // store project index we are editing (we later delete this, or retain it)

					ActivateStageClipTextOverlay(true);
					
					CVideoEditorMenu::RebuildMenu();
				}
				break;
			}

			default:
			{
				uiAssertf(0, "CVideoEditorUi::TriggerFullScreenPreview() - invalid type passed (%d)", (s32)CVideoEditorTimeline::GetItemSelected());
			}
		}
	}
}



bool CVideoEditorUi::TriggerAutomatedMovie()
{
	CVideoEditorMenu::ClearPreviewData( -1, true, false );

	if( CVideoEditorInterface::StartAutogeneratingProject( GetAutomatedMovieLength() ) )
	{
		ms_iState = ME_STATE_WAITING_PROJECT_GENERATION;
		return true;
	}
	else
	{
		SetErrorState("FAILED_TO_AUTO_PROJECT");

		return false;
	}
}


void CVideoEditorUi::SetSelectedAudio( u32 const c_soundHash, const s32 c_startOffsetMs, const u32 c_durationMs, const bool c_isScoreTrack)
{
	ms_SelectedAudio.m_soundHash = c_soundHash;
	ms_SelectedAudio.m_durationMs = c_durationMs;
	ms_SelectedAudio.m_startOffsetMs = c_startOffsetMs;
	ms_SelectedAudio.m_isScoreTrack = c_isScoreTrack;
}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorMenu::UpdateInput
// PURPOSE:	checks for input and acts on it
/////////////////////////////////////////////////////////////////////////////////////////
bool CVideoEditorUi::UpdateInput()
{
	if ( IsInBlockingOperation() || CVideoEditorMenu::CheckForUserConfirmationScreens() )  // no input here if we have confirmation screens
	{
		return false;
	}

	CVideoEditorInstructionalButtons::CheckAndAutoUpdateIfNeeded();

	bool const c_wasKbmLastFrame = CVideoEditorInstructionalButtons::LatestInstructionsAreKeyboardMouse();

#if defined(VIDEO_PLAYBACK_ENABLED) && VIDEO_PLAYBACK_ENABLED
	if( VideoPlayback::IsValidHandle( ms_activePlaybackHandle ) )
	{
		static bool isAtEndLastFrame = false;
		bool const c_isAtEnd = VideoPlayback::IsAtEndOfVideo( ms_activePlaybackHandle );
		 
		if( ( c_wasKbmLastFrame && CControlMgr::GetMainFrontendControl().GetReplayPause().IsReleased() ) || 
			( !c_wasKbmLastFrame && CControlMgr::GetMainFrontendControl().GetFrontendRT().IsReleased() ) )
		{
			ToggleVideoPlaybackPaused();
			CVideoEditorInstructionalButtons::Refresh();
		}
#if !RSG_ORBIS
		else if( ( c_wasKbmLastFrame && CControlMgr::GetMainFrontendControl().GetReplayMoveToStartPoint().IsReleased() ) ||
			( !c_wasKbmLastFrame && CControlMgr::GetMainFrontendControl().GetFrontendLT().IsReleased() ) )
		{
			SeekToVideoPlaybackStart();
			CVideoEditorInstructionalButtons::Refresh();
		}
#endif
		else if( CPauseMenu::CheckInput( FRONTEND_INPUT_BACK, true ) )
		{
			EndFullscreenVideoPlayback();
			CVideoEditorInstructionalButtons::Refresh();
		}
		else if( c_isAtEnd != isAtEndLastFrame )
		{
			CVideoEditorInstructionalButtons::Refresh();
		}

		isAtEndLastFrame = c_isAtEnd;

		return true;
	}
#endif // defined(VIDEO_PLAYBACK_ENABLED) && VIDEO_PLAYBACK_ENABLED

	if (ms_iState == ME_STATE_ACTIVE_TEXT_ADJUSTMENT)
	{
		return StageTextAdjustment();
	}
	else
	{
		if (CVideoEditorTimeline::IsTimelineSelected())
		{
			CVideoEditorTimeline::UpdateInput();
			return false;
		}
		else
		{
			return CVideoEditorMenu::UpdateInput();
		}
	}
}

bool CVideoEditorUi::IsInBlockingOperation( bool* opt_needsSpinner )
{  
	bool const c_waitingOnMemory = !CVideoEditorPlayback::IsActive() VIDEO_PLAYBACK_ONLY( && !VideoPlayback::IsValidHandle( CVideoEditorUi::GetActivePlaybackHandle() ) ) && !HasMinDecoderMemory();

	// 1945305 - grey out screen behind text editing box when its active, or we have profanity check pending
	bool const c_waitingText =	ms_iState == ME_STATE_ACTIVE_WAITING_FOR_FILENAME || ms_iState == ME_STATE_ACTIVE_WAITING_FOR_FILENAME_THEN_EXIT || 
								ms_iState == ME_STATE_ACTIVE_WAITING_FOR_EXPORT_FILENAME ||
								ms_iState == ME_STATE_ACTIVE_WAITING_FOR_TEXT_INPUT || ms_iState == ME_STATE_ACTIVE_WAITING_FOR_VIDEO_UPLOAD_TEXT_INPUT; 
				
				// Async file operations pending
	bool const c_waitingPopulate =	( ms_clipFileView && !ms_clipFileView->isPopulated() ) || ( ms_ProjectFileView && !ms_ProjectFileView->isPopulated() ) 
#if defined(VIDEO_PLAYBACK_ENABLED) && VIDEO_PLAYBACK_ENABLED
		|| ( ms_VideoFileView && !ms_VideoFileView->isPopulated() )
#endif
		;

	bool const c_WaitingDelete = ( ms_clipFileView && ms_clipFileView->isDeletingFile() ) || ( ms_ProjectFileView && ms_ProjectFileView->isDeletingFile() ) 
#if defined(VIDEO_PLAYBACK_ENABLED) && VIDEO_PLAYBACK_ENABLED
		|| ( ms_VideoFileView && ms_VideoFileView->isDeletingFile() )
#endif
		|| CVideoEditorMenu::IsPendingDelete();

	bool const c_savingFavourites =	( ms_clipFileView && ms_clipFileView->isSavingFavourites() );

	bool const c_generatingMontage = ms_iState == ME_STATE_WAITING_PROJECT_GENERATION;
	bool const c_loadingProject = ms_iState == ME_STATE_WAITING_PROJECT_LOAD;

	bool const c_waitingStagingClip = ms_iState == ME_STATE_ACTIVE_WAITING_FOR_STAGING_CLIP;

	if( opt_needsSpinner )
	{
		*opt_needsSpinner = c_waitingPopulate || c_generatingMontage || c_loadingProject || c_WaitingDelete || c_waitingStagingClip || c_savingFavourites || c_waitingOnMemory ||
			( c_waitingText && !ms_textBuffer.IsNullOrEmpty() );
	}

	bool const c_IsInBlockingOperation = c_waitingText || c_waitingPopulate || c_generatingMontage || c_loadingProject || c_WaitingDelete || c_waitingStagingClip || c_savingFavourites || c_waitingOnMemory;
#if __BANK
	if (c_IsInBlockingOperation)
	{
		uiDisplayf("CVideoEditorUi::IsInBlockingOperation - c_waitingText %d ms_iState %d c_waitingPopulate %d c_generatingMontage %d c_loadingProject %d c_WaitingDelete %d c_waitingStagingClip %d c_savingFavourites %d c_waitingOnMemory %d", 
			c_waitingText, ms_iState, c_waitingPopulate, c_generatingMontage, c_loadingProject, c_WaitingDelete, c_waitingStagingClip, c_savingFavourites, c_waitingOnMemory);
		return true;
	}
#endif
	return c_IsInBlockingOperation;
}

bool CVideoEditorUi::IsInHideEditorOperation(bool* out_showInstructionalButtons)
{
#if defined(VIDEO_PLAYBACK_ENABLED) && VIDEO_PLAYBACK_ENABLED
#if !USE_ORBIS_UPLOAD
	if (VideoUploadManager::GetInstance().IsInSetup())
	{
		if (out_showInstructionalButtons)
			*out_showInstructionalButtons = VideoUploadManager::GetInstance().IsAddingText();
		return true;
	}
#endif // !USE_ORBIS_UPLOAD
#endif
	return false;
}

void CVideoEditorUi::LeaveEditingMode()
{
	CVideoEditorTimeline::ClearAllClips();
	CVideoEditorTimeline::SetPlayheadPositionMs(0, false);

	ReleaseFileViewThumbnails();

	CVideoEditorUi::ReleaseProject();

	ShowMainMenu();

	RefreshFileViews();

	ms_iState = ME_STATE_ACTIVE;

	CVideoEditorMenu::RebuildMenu();

	CVideoEditorMenu::RefreshProjectHeading();

	CVideoEditorTimeline::SetTimeOnPlayhead(false);
}



bool CVideoEditorUi::StageTextAdjustment()
{
	if (CControlMgr::UpdateVirtualKeyboard() == ioVirtualKeyboard::kKBState_PENDING)
		return false;

	sEditedTextProperties &editedText = CTextTemplate::GetEditedText();

#if RSG_PC
	const float fMousePosX = ioMouse::GetNormX();
	const float fMousePosY = ioMouse::GetNormY();
	const bool bMouseLeftDown = (ioMouse::GetButtons() & ioMouse::MOUSE_LEFT) != 0;
	const bool bMouseRightClick = (ioMouse::GetReleasedButtons() & ioMouse::MOUSE_RIGHT) != 0;
	const bool bShiftKeyPressed = (CControlMgr::GetKeyboard().GetKeyDown(KEY_LSHIFT, KEYBOARD_MODE_GAME) || CControlMgr::GetKeyboard().GetKeyDown(KEY_RSHIFT, KEYBOARD_MODE_GAME));
#else
	const bool bMouseRightClick = false;
	const bool bShiftKeyPressed = false;
#endif

	float const c_shrinkFactor = (CMousePointer::IsMouseWheelDown() && !bShiftKeyPressed) ? 4.0f : CControlMgr::GetMainFrontendControl().GetFrontendLT().GetNorm01();
	float const c_growFactor = (CMousePointer::IsMouseWheelUp() && !bShiftKeyPressed) ? 4.0f : CControlMgr::GetMainFrontendControl().GetFrontendRT().GetNorm01();

	bool const c_shrink  = c_shrinkFactor > sm_triggerThreshold;
	bool const c_grow  = c_growFactor > sm_triggerThreshold;

	float textSize = editedText.m_scale;

	if( c_shrink != c_grow )
	{
		textSize += ( c_shrink ? -c_shrinkFactor : c_growFactor ) * sm_minMemeTextSizeIncrement;
		textSize = Clamp( textSize, sm_minMemeTextSize, sm_maxMemeTextSize );
	}

	float const c_xAxis = CControlMgr::GetMainFrontendControl().GetFrontendLeftRight().GetNorm();
	float const c_yAxis = CControlMgr::GetMainFrontendControl().GetFrontendUpDown().GetNorm();

	bool const c_moveX = abs(c_xAxis) > sm_thumbstickThreshold;
	bool const c_moveY = abs(c_yAxis) > sm_thumbstickThreshold;
	bool mouse_moved = false;

	Vector2 vPosition( editedText.m_position );

	const float fMinY = 0.0f;
	const float fMaxY = 1.0f;

#if RSG_PC
	CMousePointer::SetMouseCursorStyle(MOUSE_CURSOR_STYLE_ARROW);

	if (bMouseLeftDown)
	{
		if (fMousePosX >= CVideoEditorMenu::GetStagePosition().x &&
			fMousePosY >= CVideoEditorMenu::GetStagePosition().y &&
			fMousePosX <= CVideoEditorMenu::GetStagePosition().x+CVideoEditorTimeline::GetStageSize().x &&
			fMousePosY <= CVideoEditorMenu::GetStagePosition().y+CVideoEditorTimeline::GetStageSize().y)
		{
			CMousePointer::SetMouseCursorStyle(MOUSE_CURSOR_STYLE_HAND_GRAB);

			const float fMouseOffsetX = fMousePosX - CVideoEditorMenu::GetStagePosition().x;
			const float fMouseOffsetY = fMousePosY - CVideoEditorMenu::GetStagePosition().y;
			const float fFinalOffsetX = fMouseOffsetX / CVideoEditorTimeline::GetStageSize().x;
			const float fFinalOffsetY = fMouseOffsetY / CVideoEditorTimeline::GetStageSize().y;

			vPosition.x = editedText.m_position.x = Clamp( fFinalOffsetX, 0.f, 1.f );
			vPosition.y = editedText.m_position.y = Clamp( fFinalOffsetY, fMinY, fMaxY );

			mouse_moved = true;
		}
	}
	else
#endif // #if RSG_PC
	{
		if( c_moveX )
		{
			vPosition.x += c_xAxis * sm_thumbstickMultiplier;
			vPosition.x = Clamp( vPosition.x, 0.f, 1.f );
		}

		if( c_moveY )
		{
			vPosition.y += c_yAxis * sm_thumbstickMultiplier;
			vPosition.y = Clamp( vPosition.y, fMinY, fMaxY );
		}
	}

	if( c_moveX || c_moveY || c_shrink || c_grow || mouse_moved )
	{
		editedText.m_position = vPosition;
		editedText.m_scale = textSize;

		if (c_moveX || mouse_moved)
		{
			CTextTemplate::UpdateTemplate(0, editedText, "POSITION_X");
		}

		if (c_moveY || mouse_moved)
		{
			CTextTemplate::UpdateTemplate(0, editedText, "POSITION_Y");
		}

		if (c_shrink || c_grow)
		{
			CTextTemplate::UpdateTemplate(0, editedText, "SCALE");
		}
	}

	if (CPauseMenu::CheckInput(FRONTEND_INPUT_ACCEPT, true) || CPauseMenu::CheckInput(FRONTEND_INPUT_BACK, true) || bMouseRightClick)
	{
		ms_iState = ME_STATE_ACTIVE;
		CVideoEditorInstructionalButtons::Refresh();
#if RSG_PC
	CMousePointer::SetMouseCursorStyle(MOUSE_CURSOR_STYLE_ARROW);
#endif // #if RSG_PC

		CVideoEditorMenu::RebuildMenu();
		CVideoEditorMenu::GreyOutCurrentColumn(false);

		return true;
	}

	return false;
}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorUi::ActivateStageClipTextOverlay
// PURPOSE:	sets whether to show text overlay over the centre stage clip
/////////////////////////////////////////////////////////////////////////////////////////
void CVideoEditorUi::ActivateStageClipTextOverlay(bool const c_activate)
{
	ms_bRenderStageText = c_activate;

#if USE_TEXT_CANVAS
	if (c_activate)
	{
		CTextTemplate::SetupTemplate(CTextTemplate::GetEditedText());
	}
	else
	{
		CVideoEditorTimeline::SetCurrentTextOverlayIndex(-1);
		CTextTemplate::UnloadTemplate();
	}
#endif

}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorUi::Open
// PURPOSE:	called each time we open the video editor menu
/////////////////////////////////////////////////////////////////////////////////////////
void CVideoEditorUi::Open(eOpenedFromSource source)
{
	uiDisplayf("EDITOR OPEN");

	if (!CPauseMenu::IsActive(PM_SkipVideoEditor) || CPauseMenu::IsClosingDown())
	{
		fwTimer::StartUserPause();  // keep the game paused throughout the editor
	}

	if (CGameWorld::FindLocalPlayer() && CGameWorld::FindLocalPlayer()->GetPlayerInfo())
	{
		CGameWorld::FindLocalPlayer()->GetPlayerInfo()->DisableControlsFrontend();
	}

	if (!CScaleformMgr::IsMovieActive(ms_iMovieId))
	{
		LoadAudioTrackText();
		ms_iMovieId = CScaleformMgr::CreateMovie(VIDEO_EDITOR_UI_FILENAME, Vector2(0,0), Vector2(1.0f,1.0f));

		uiAssertf(ms_iMovieId != INVALID_MOVIE_ID, "CVideoEditorUi - couldnt load '%s' movie!", VIDEO_EDITOR_UI_FILENAME);
	}

	CTextTemplate::OpenMovie();
	WatermarkRenderer::OpenMovie();

	CVideoEditorInstructionalButtons::Init();

	// B* 2454572-  Rockstar Editor rich presence only updates once the player starts editing a clip.
	// Rich Presence Set - Set to Rockstar Editor
	if (NetworkInterface::IsLocalPlayerOnline())
	{
#if RSG_DURANGO
		atArray< int > fieldIds;
		fieldIds.PushAndGrow(player_schema::PresenceUtil::GetFieldIdFromFieldIndex(player_schema::PRESENCE_PRES_8, 0));
		atArray< richPresenceFieldStat > fieldData;
		fieldData.PushAndGrow(richPresenceFieldStat(player_schema::PRESENCE_PRES_8, "SPMG_30"));
		NetworkInterface::GetRichPresenceMgr().SetPresenceStatus(NetworkInterface::GetLocalGamerIndex(), player_schema::PRESENCE_PRES_8, fieldIds.GetElements(), fieldData.GetElements(), 1);
#elif RSG_ORBIS
		const char* presence = "SPMG_30_STR";
		char* presencechar = TheText.Get(atStringHash(presence), presence);
		NetworkInterface::GetRichPresenceMgr().SetPresenceStatus(player_schema::PRESENCE_PRES_8, presencechar);
#endif
	}

	//
	// init variables:
	//
	ms_iState = ME_STATE_NONE;

	ms_bRenderStageText = false;
	SetUserWarnedOfGameDestroy(false);
	ms_iAutomatedLengthSelection = CReplayEditorParameters::AUTO_GENERATION_SMALL;

	CVideoEditorInterface::Activate();

	const strLocalIndex iTxdId = g_TxdStore.FindSlot(VIDEO_EDITOR_CODE_TXD);

	if (uiVerifyf(iTxdId != -1, "Invalid or missing slot %s", VIDEO_EDITOR_CODE_TXD))
	{
		g_TxdStore.StreamingRequest(iTxdId, (STRFLAG_FORCE_LOAD | STRFLAG_PRIORITY_LOAD | STRFLAG_DONTDELETE));
		CStreaming::SetDoNotDefrag(iTxdId, g_TxdStore.GetStreamingModuleId());
	}

	LIGHT_EFFECTS_ONLY(CControlMgr::GetPlayerMappingControl().SetLightEffect(&sm_LightEffect, CControl::CODE_LIGHT_EFFECT));
	
	// Flush game stream queue and resume it
	CGameStreamMgr::GetGameStream()->FlushQueue();
	CGameStreamMgr::GetGameStream()->Resume();

	uiAssertf(!ms_project, "CVideoEditorUi - ms_project is not invalid at init!");
	uiAssertf(!ms_ProjectFileView, "CVideoEditorUi - ms_ProjectFileView is not invalid at init!");
	uiAssertf(!ms_VideoFileView, "CVideoEditorUi - ms_VideoFileView is not invalid at init!");
	uiAssertf(!ms_clipFileView, "CVideoEditorUi - ms_clipFileView is not invalid at init!");

	ms_project = NULL;
	ms_ProjectFileView = NULL;
	ms_VideoFileView = NULL;
	ms_clipFileView = NULL;

	ms_bRebuildTimeline = false;
	ms_bRebuildAnchors = false;
	ms_bProjectHasBeenEdited = false;
	ms_bGameDestroyWarned = false;
    ms_refreshVideoViewOnMemBlockFree = false;
	ms_directorModeLaunched = false;

	ms_actionEventsOnGameDestruction = false;

	ms_iState = ME_STATE_LOADING;

	sm_openedFromSource = source;

    eRockstarEditorLaunchSource const c_launchSource = ( source == OFS_CODE_LANDING_PAGE) ? ROCKSTAR_EDITOR_LAUNCHED_LANDING : ROCKSTAR_EDITOR_LAUNCHED_SP;
    SendUGCTelemetry( ROCKSTAR_EDITOR_TELEMETRY_ID, c_launchSource );
}

void CVideoEditorUi::GarbageCollect()
{
	CScaleformMgr::ForceCollectGarbage(GetMovieId());  // do some garbage collection
}

void CVideoEditorUi::ShowTimeline()
{
	SetMovieMode(ME_MOVIE_MODE_TIMELINE);
	CVideoEditorTimeline::SetTimelineSelected(true);
	CVideoEditorTimeline::SetTimeOnPlayhead(true);
	CVideoEditorMenu::TurnOffMenu(false);
	CVideoEditorInstructionalButtons::Refresh();
	ActivateStageClipTextOverlay(false);
	SetActiveState();

	GarbageCollect();
}

bool CVideoEditorUi::ShowMenu(const atHashString menuToShow)
{
	const bool bSuccess = CVideoEditorMenu::JumpToMenu(menuToShow);

	if (bSuccess)
	{
		SetMovieMode(ME_MOVIE_MODE_MENU);

		CVideoEditorTimeline::SetTimelineSelected(false);
		CVideoEditorInstructionalButtons::Refresh();
		ActivateStageClipTextOverlay(false);
		SetActiveState();

		GarbageCollect();
	}

	return bSuccess;
}

bool CVideoEditorUi::ShowMainMenu()
{
	return ShowMenu("MAIN_MENU");
}

bool CVideoEditorUi::ShowOptionsMenu()
{
	return ShowMenu("ADD_LIST");
}

bool CVideoEditorUi::ShowExportMenu()
{
#if RSG_PC
	return ShowMenu("EXPORT_SETTINGS");
#else
	return ShowMenu("ADD_LIST");
#endif
}
void CVideoEditorUi::ShowPlayback()
{
	SetMovieMode(ME_MOVIE_MODE_PLAYBACK);
	CVideoEditorTimeline::SetTimelineSelected(false);
	CVideoEditorMenu::TurnOffMenu(true);
	CVideoEditorInstructionalButtons::Refresh();
	ActivateStageClipTextOverlay(false);
	SetActiveState();

	GarbageCollect();
}


void CVideoEditorUi::SetMovieMode(eME_MOVIE_MODE mode)
{
	if (CScaleformMgr::BeginMethod(GetMovieId(), SF_BASE_CLASS_VIDEO_EDITOR, "SET_MOVIE_MODE"))
	{
		CScaleformMgr::AddParamInt((s32)mode);
		CScaleformMgr::EndMethod();
	}
}

void CVideoEditorUi::Close(const bool fadeOut)
{
	if (IsActive() && ms_iState != ME_STATE_SHUTDOWN)
	{
        VIDEO_PLAYBACK_ONLY( VideoPlayback::CleanupVideo( ms_activePlaybackHandle ) );

		CVideoEditorPlayback::Close();
		CVideoEditorMenu::Close();
		CTextTemplate::Shutdown();
		CVideoEditorTimeline::Shutdown();
		CReplayMgrInternal::Close();

		if (fadeOut && !CPauseMenu::IsActive(PM_SkipVideoEditor))
		{
			camInterface::FadeOut(0);  // fade out the screen, ready to shut down the editor and return to either the pausemenu or game
		}

		ms_actionEventsOnGameDestruction = false;
		ms_iState = ME_STATE_SHUTDOWN;

#if GEN9_LANDING_PAGE_ENABLED
		// Check for landing page launch
		if (Gen9LandingPageEnabled() &&																				// landing page is enabled								
			!ms_directorModeLaunched &&																				// director mode is not active (i.e. editor has not closed to launch director mode)
			(sm_openedFromSource == OFS_CODE_LANDING_PAGE || sm_openedFromSource == OFS_SCRIPT_LANDING_PAGE))		// editor was opened from the landing page in code or script
		{
			CLandingPage::SetShouldLaunch( LandingPageConfig::eEntryPoint::ENTRY_FROM_PAUSE );
		}
#endif // GEN9_LANDING_PAGE_ENABLED

		sm_openedFromSource = OFS_UNKNOWN;
	}
}


/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorUi::OnSignOut
// PURPOSE:	called each time we sign out.
/////////////////////////////////////////////////////////////////////////////////////////
void CVideoEditorUi::OnSignOut( )
{
	if (CVideoEditorUi::IsActive())  // only do this if the video editor is active!  - fix for 2131760 
	{
		CVideoEditorUi::Close( false );
		CloseInternal();  // immediately close down upon sign out as we will start a new session and update wont hit again until signed back in/booted up
	}
}

/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorUi::Close
// PURPOSE:	called each time we close the video editor menu
/////////////////////////////////////////////////////////////////////////////////////////
void CVideoEditorUi::CloseInternal()
{
	CVideoEditorInstructionalButtons::Shutdown();

	ReleaseFileViewThumbnails();

	ReleaseFileViews();

	CVideoEditorUi::ReleaseProject();

	CVideoEditorInterface::Deactivate();

	CTextTemplate::UnloadTemplate();

	CScaleformMgr::RequestRemoveMovie(ms_iMovieId);
	ms_iMovieId = INVALID_MOVIE_ID;

	CTextTemplate::CloseMovie();
	WatermarkRenderer::CloseMovie();

	if (!CPauseMenu::IsActive(PM_SkipVideoEditor))  // if pausemenu is not active, then end the user pause on close of the replay editor
	{
		if (CGameWorld::FindLocalPlayer() && CGameWorld::FindLocalPlayer()->GetPlayerInfo())
		{
			CGameWorld::FindLocalPlayer()->GetPlayerInfo()->EnableControlsFrontend();
		}

		fwTimer::EndUserPause();
	}

	ShutdownCodeTextures();

#if LIGHT_EFFECTS_SUPPORT
	if(CControlMgr::GetPlayerMappingControl().IsCurrentLightEffect(&sm_LightEffect, CControl::CODE_LIGHT_EFFECT))
	{
		CControlMgr::GetPlayerMappingControl().ClearLightEffect(&sm_LightEffect, CControl::CODE_LIGHT_EFFECT);
	}
#endif // LIGHT_EFFECTS_SUPPORT

	// B* 2454572-  Rockstar Editor rich presence only updates once the player starts editing a clip.
	// Rich Presence Cleanup - Reset to Story Mode
	if (NetworkInterface::IsLocalPlayerOnline())
	{
#if RSG_DURANGO
		NetworkInterface::GetRichPresenceMgr().SetPresenceStatus(NetworkInterface::GetLocalGamerIndex(), player_schema::PRESENCE_PRES_0, NULL, NULL, 0);
#elif RSG_ORBIS
		const char* presence = "PRESENCE_0_STR";
		char* presencechar = TheText.Get(atStringHash(presence), presence);
		NetworkInterface::GetRichPresenceMgr().SetPresenceStatus(player_schema::PRESENCE_PRES_0, presencechar);
#endif
	}

	ms_iState = ME_STATE_NONE;

	uiDisplayf("EDITOR CLOSE");
}



void CVideoEditorUi::InitCodeTextures()
{
	// stateblocks
	grcBlendStateDesc bsd;

	//	The remaining BlendStates should all have these two flags set
	bsd.BlendRTDesc[0].BlendEnable = true;
	bsd.BlendRTDesc[0].SrcBlend = grcRSV::BLEND_SRCALPHA;
	bsd.BlendRTDesc[0].DestBlend = grcRSV::BLEND_INVSRCALPHA;
	bsd.BlendRTDesc[0].BlendOp = grcRSV::BLENDOP_ADD;
	bsd.BlendRTDesc[0].SrcBlendAlpha = grcRSV::BLEND_SRCALPHA;
	bsd.BlendRTDesc[0].DestBlendAlpha = grcRSV::BLEND_INVSRCALPHA;
	bsd.BlendRTDesc[0].BlendOpAlpha = grcRSV::BLENDOP_ADD;
	bsd.BlendRTDesc[0].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_ALL;
	ms_StandardSpriteBlendStateHandle = grcStateBlock::CreateBlendState(bsd);


	for (u32 uNum = 0; uNum < MAX_CODE_SPRITES; uNum++)
	{
		ms_codeSprite[uNum].Delete();
	}

	const strLocalIndex iTxdId = g_TxdStore.FindSlot(VIDEO_EDITOR_CODE_TXD);

	if (uiVerifyf(iTxdId != -1, "Invalid or missing slot %s", VIDEO_EDITOR_CODE_TXD))
	{
		g_TxdStore.AddRef(iTxdId, REF_OTHER);
		g_TxdStore.PushCurrentTxd();
		g_TxdStore.SetCurrentTxd(iTxdId);

		uiAssert(MAX_CODE_SPRITES == CVideoEditorMenu::ms_MenuArray.CVideoEditorPositioning.GetCount());

		for (u32 uCount = 0; uCount < CVideoEditorMenu::ms_MenuArray.CVideoEditorPositioning.GetCount() && uCount < MAX_CODE_SPRITES; uCount++)
		{
			ms_codeSprite[uCount].SetTexture(CVideoEditorMenu::ms_MenuArray.CVideoEditorPositioning[uCount].name.c_str());
		}

		g_TxdStore.PopCurrentTxd();
	}
}

void CVideoEditorUi::ShutdownCodeTextures()
{
	for (u32 uNum = 0; uNum < MAX_CODE_SPRITES; uNum++)
	{
		ms_codeSprite[uNum].Delete();
	}

	const strLocalIndex iTxdId = g_TxdStore.FindSlot(VIDEO_EDITOR_CODE_TXD);

	if (uiVerifyf(iTxdId != -1, "Invalid or missing slot %s", VIDEO_EDITOR_CODE_TXD))
	{
		g_TxdStore.RemoveRef(iTxdId, REF_OTHER);
	}
}


void CVideoEditorUi::getExportTimeAsString( char (&out_buffer)[ 128 ] )
{
	out_buffer[0] = rage::TEXT_CHAR_TERMINATOR;

	CVideoEditorProject const* project = ms_project;
	if( project )
	{
		float const c_exportDurationMs = 2 * project->GetTotalClipTimeMs();

		// At minimum say 5 seconds, as we have loading and such to account for
		u32 const c_clampedExportDurationMs = (u32)MAX( c_exportDurationMs, 5000 );
		
		// B*2269977 ...round to 1,2,3,4,5,10,15,20,25,30 ...then every 10 minutes
		u32 const c_minuteInMs = 60000;
		u32 const c_unitsToRoundToInMs = (c_clampedExportDurationMs <= 5*c_minuteInMs) ? c_minuteInMs : (c_clampedExportDurationMs <= 30*c_minuteInMs) ? 5*c_minuteInMs : 10*c_minuteInMs;

		// quickest, cheapest way if always > 0 ... should be in this case
		u32 const c_printDurationMs = c_clampedExportDurationMs + c_unitsToRoundToInMs - 1 - (c_clampedExportDurationMs - 1) % c_unitsToRoundToInMs;

		CTextConversion::FormatMsTimeAsLongString( out_buffer, NELEM(out_buffer), c_printDurationMs );
	}
}

void CVideoEditorUi::DisplaySaveProjectResultPrompt( u64 const resultFlags )
{
	bool const c_hasFailuresToReport = ( resultFlags & CReplayEditorParameters::PROJECT_SAVE_RESULT_FAILED ) != 0;
	if( c_hasFailuresToReport )
	{
		if( ( resultFlags & CReplayEditorParameters::PROJECT_SAVE_RESULT_NO_SPACE ) != 0 )
		{
			SetErrorState("NO_SPACE_FOR_PROJECTS_SAVE");
		}
		else if( ( resultFlags & CReplayEditorParameters::PROJECT_SAVE_RESULT_FILE_ACCESS_ERROR) != 0 )
		{
			SetErrorState("FILE_NAME_INVALID");
		}
		else
		{
			SetErrorState("FAILED_TO_SAVE_PROJECT");
		}
	}
}

void CVideoEditorUi::EditLengthSelection(s32 const c_direction)
{
	ms_iAutomatedLengthSelection += c_direction;  // adjust value

	// wrap round both ends:
	if (ms_iAutomatedLengthSelection < 0)
	{
		ms_iAutomatedLengthSelection = CReplayEditorParameters::AUTO_GENERATION_EXTRA_LARGE;
	}

	if (ms_iAutomatedLengthSelection >= CReplayEditorParameters::AUTO_GENERATION_MAX)
	{
		ms_iAutomatedLengthSelection = CReplayEditorParameters::AUTO_GENERATION_SMALL;
	}
}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorUi::EditTextDuration
// PURPOSE:	adjusts the duration of the text
/////////////////////////////////////////////////////////////////////////////////////////
void CVideoEditorUi::EditTextDuration(s32 const c_direction)
{
	// ensure that the max duration we can use is the start of the next text clip along
	u32 const c_playheadPositionMs = CVideoEditorTimeline::GetPlayheadPositionMs();
	// get the start position of the nearest clip
	const s32 c_projectIndexAtPlayhead = CVideoEditorTimeline::GetProjectIndexAtTime(PLAYHEAD_TYPE_TEXT, c_playheadPositionMs);
	const s32 c_textProjectIndexToRight = CVideoEditorTimeline::GetProjectIndexNearestStartPos(PLAYHEAD_TYPE_TEXT, c_playheadPositionMs+1, 1, c_projectIndexAtPlayhead);  // nearest proj index

	const s32 c_diffTimeMs = c_projectIndexAtPlayhead != -1 ? GetProject()->GetOverlayText(c_projectIndexAtPlayhead).getStartTimeMs() : c_playheadPositionMs;

	u32 const c_precisionMaxTime = c_textProjectIndexToRight != -1 ? (GetProject()->GetOverlayText(c_textProjectIndexToRight).getStartTimeMs()-1) - c_diffTimeMs : (u32)ms_project->GetTotalClipTimeMs();
	u32 const c_roundedMaxTime = (u32(c_precisionMaxTime / 1000.0f)) * 1000;

	sEditedTextProperties &editedText = CTextTemplate::GetEditedText();

	// wrap
	if (c_direction < 0)
	{
		if (editedText.m_durationMs == c_precisionMaxTime)
		{
			editedText.m_durationMs = c_roundedMaxTime;
		}
		else
		{
			editedText.m_durationMs -=  MIN_TEXT_DURATION_MS;

			if (editedText.m_durationMs < MIN_TEXT_DURATION_MS)
			{
				editedText.m_durationMs = c_precisionMaxTime;
			}
		}
	}
	else if (c_direction > 0)
	{
		if (editedText.m_durationMs == c_precisionMaxTime)
		{
			editedText.m_durationMs = MIN_TEXT_DURATION_MS;
		}
		else
		{
			editedText.m_durationMs += MIN_TEXT_DURATION_MS;

			if (editedText.m_durationMs > c_roundedMaxTime)
			{
				editedText.m_durationMs = c_precisionMaxTime;
			}
		}
	}
	else if (c_direction == 0)
	{
		editedText.m_durationMs = c_precisionMaxTime;
	}

	// finally ensure we stay within our defined limits
	if (editedText.m_durationMs < MIN_TEXT_DURATION_MS)
	{
		editedText.m_durationMs = MIN_TEXT_DURATION_MS;
	}

	if (c_precisionMaxTime > MIN_TEXT_DURATION_MS && editedText.m_durationMs > c_precisionMaxTime)  // fix for bug 2411016
	{
		editedText.m_durationMs = c_precisionMaxTime;
	}
}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorUi::EditTextDuration
// PURPOSE:	adjusts the duration of the text
/////////////////////////////////////////////////////////////////////////////////////////
void CVideoEditorUi::EditTemplatePosition(s32 const c_direction)
{
	if (c_direction != 0)
	{
		sEditedTextProperties &editedText = CTextTemplate::GetEditedText();

		s32 const c_adjustment = c_direction > 0 ? 1 : -1;

		editedText.m_positionType += c_adjustment;

		// wrap:
		s32 const c_maxDataCount = CTextTemplate::GetPropertyCount("POSITION");

		if (editedText.m_positionType >= c_maxDataCount)
		{
			editedText.m_positionType = 0;
		}

		if (editedText.m_positionType < 0)
		{
			editedText.m_positionType = c_maxDataCount - 1;
		}
	}
}







/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorUi::EditTextTemplate
// PURPOSE:	adjusts the duration of the text
/////////////////////////////////////////////////////////////////////////////////////////
void CVideoEditorUi::EditTextTemplate(s32 const c_direction)
{
	if (c_direction != 0)
	{
		sEditedTextProperties *pEditedText = &CTextTemplate::GetEditedText();

		if (pEditedText)
		{
			s32 const c_currentTemplateIndex = CTextTemplate::FindTemplateArrayIndex();

			s32 const c_adjustment = c_direction > 0 ? 1 : -1;

			atHashString const c_newTemplateId = CTextTemplate::GetTemplateIdFromArrayIndex(c_currentTemplateIndex + c_adjustment);

			CTextTemplate::SetCurrentTemplate(c_newTemplateId);
		}
	}
}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorUi::EditTextOpacity
// PURPOSE:	adjusts the color of the text
/////////////////////////////////////////////////////////////////////////////////////////
void CVideoEditorUi::EditExportFps(s32 const c_direction)
{
	if (c_direction != 0)
	{
		ms_ExportProperties.m_fps = 1 - ms_ExportProperties.m_fps;
	}
}


/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorUi::EditTextOpacity
// PURPOSE:	adjusts the color of the text
/////////////////////////////////////////////////////////////////////////////////////////
void CVideoEditorUi::EditExportQuality(s32 const c_direction)
{
	if (c_direction != 0)
	{
		int const c_adjustment = c_direction > 0 ? 1 : -1;

		ms_ExportProperties.m_quality += c_adjustment;
		if(ms_ExportProperties.m_quality < 0)
			ms_ExportProperties.m_quality = 2;
		else if(ms_ExportProperties.m_quality > 2)
			ms_ExportProperties.m_quality = 0;
	}
}



void CVideoEditorUi::RefreshThumbnail( u32 const clipIndex )
{
	CVideoEditorTimeline::ReleaseThumbnail( clipIndex );

	// No need to re-request. Timeline will do it automatically
}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorUi::RebuildTimeline
// PURPOSE:	flags to rebuild the timeline, deals with releasing and requesting the
//          textures in use.   Allows to skip a clip index (the thumb wont be released)
/////////////////////////////////////////////////////////////////////////////////////////
void CVideoEditorUi::RebuildTimeline()
{
	CVideoEditorTimeline::ClearAllClips();

	ms_bRebuildTimeline = true;
}



void CVideoEditorUi::RebuildAnchors()
{
	ms_bRebuildAnchors = true;
}




void CVideoEditorUi::ReleaseFileViewThumbnails()
{
	if (ms_clipFileView)
	{
		ms_clipFileView->releaseAllThumbnails();
	}

#if defined(VIDEO_PLAYBACK_ENABLED) && VIDEO_PLAYBACK_ENABLED
	if( ms_VideoFileView )
	{
		ms_VideoFileView->releaseAllThumbnails();
	}
#endif // defined(VIDEO_PLAYBACK_ENABLED) && VIDEO_PLAYBACK_ENABLED

	if (ms_ProjectFileView)
	{
		ms_ProjectFileView->releaseAllThumbnails();
	}
}



bool CVideoEditorUi::IsProjectPopulated()
{
	if (GetProject())
	{
		return (GetProject()->GetClipCount() > 0 || GetProject()->GetMusicClipCount() > 0 || GetProject()->GetOverlayTextCount() > 0);
	}

	return false;
}


/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorUi::CreateFileViews
// PURPOSE:	all file views can only be considered valid and ready once this function
//			returns true, and create them in order, waiting for each one to finish 1st
/////////////////////////////////////////////////////////////////////////////////////////
bool CVideoEditorUi::CreateFileViews()
{
	char path[RAGE_MAX_PATH] = { 0 };

	if (!ms_clipFileView)
	{
		CVideoEditorUtil::getClipDirectory(path);
		CVideoEditorInterface::GrabRawClipFileView( path, ms_clipFileView);
		return false;
	}
	else if (!ms_clipFileView->isPopulated() && !ms_clipFileView->hasPopulationFailed() )  // return if not ready and not failed
	{
		return false;
	}

	if ( !ms_ProjectFileView )
	{
		CVideoEditorUtil::getVideoProjectDirectory(path);
		CVideoEditorInterface::GrabVideoProjectFileView(path, ms_ProjectFileView);
		return false;
	}
	else if (!ms_ProjectFileView->isPopulated() && !ms_ProjectFileView->hasPopulationFailed() )  // return if not ready and not failed
	{
		return false;
	}

    //! PS4 and Xb1 hijack the replay allocator, but for consistency we do this for all platforms
    CReplayMgr::FreeUpBlocks();

#if defined(VIDEO_PLAYBACK_ENABLED) && VIDEO_PLAYBACK_ENABLED
#if RSG_PC || RSG_DURANGO
	CVideoEditorUtil::refreshVideoDirectory();
#endif
	if ( !ms_VideoFileView )
	{
		CVideoEditorUtil::getVideoDirectory(path);
		CVideoEditorInterface::GrabVideoFileView(path, ms_VideoFileView);
		return false;
	}
	else if (!ms_VideoFileView->isPopulated() && !ms_VideoFileView->hasPopulationFailed() )  // return if not ready and not failed
	{
		return false;
	}
#endif
	
	// they should all now be initialised, so return true
	return true;
}


void CVideoEditorUi::ReleaseFileViews()
{
	if (ms_clipFileView)
	{
		CVideoEditorInterface::ReleaseRawClipFileView( ms_clipFileView );
		uiAssert(!ms_clipFileView);
	}

#if defined(VIDEO_PLAYBACK_ENABLED) && VIDEO_PLAYBACK_ENABLED
	if( ms_VideoFileView )
	{
		CVideoEditorInterface::ReleaseVideoFileView( ms_VideoFileView );
		uiAssert(!ms_VideoFileView);
	}
#endif // defined(VIDEO_PLAYBACK_ENABLED) && VIDEO_PLAYBACK_ENABLED

	if (ms_ProjectFileView)
	{
		CVideoEditorInterface::ReleaseVideoProjectFileView(ms_ProjectFileView);
		uiAssert(!ms_ProjectFileView);
	}
}



void CVideoEditorUi::RefreshFileViews()
{
	if (ms_ProjectFileView)
	{
		ms_ProjectFileView->refresh(false);
	}

#if defined(VIDEO_PLAYBACK_ENABLED) && VIDEO_PLAYBACK_ENABLED
	if (ms_VideoFileView)
	{
		ms_VideoFileView->refresh(false);
	}
	else
	{
		char path[RAGE_MAX_PATH] = { 0 };
		CVideoEditorUtil::getVideoDirectory(path);
		CVideoEditorInterface::GrabVideoFileView(path, ms_VideoFileView);
	}
#endif

	if (ms_clipFileView)
	{
		ms_clipFileView->refresh(false);
	}
}



void CVideoEditorUi::RefreshVideoViewWhenSafe(bool ORBIS_ONLY(completedExport) )
{
	// PS4 needs to go through all the videos in system software. not cheap like XB1, where it'll tag a video on
	// only do this when going into Editor main menu on PS4
#if RSG_ORBIS
  	if (completedExport && ms_VideoFileView)
	{
		ms_VideoFileView->TempAddVideoDisplayName(GetExportName());
	}
#else
	ms_refreshVideoViewOnMemBlockFree = true;
#endif
}


void CVideoEditorUi::ReleaseProject()
{
	if (CVideoEditorInterface::GetActiveProject())
	{
		CVideoEditorInterface::ReleaseProject(ms_project);

		uiAssert(!ms_project);
	}
}

/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorUi::Render
// PURPOSE: main render of the movie on the RT
/////////////////////////////////////////////////////////////////////////////////////////
void CVideoEditorUi::Render()
{
	if (!IsActive())
		return;

	GRC_ALLOC_SCOPE_AUTO_PUSH_POP()
	if (!CSystem::IsThisThreadId(SYS_THREAD_RENDER))  // only on RT
	{
		uiAssertf(0, "CVideoEditorMenu::Render() can only be called on the RenderThread!");

		return;
	}

	// render animated spinner during loading	
	if (CScaleformMgr::IsMovieActive(GetMovieId()))
	{
		bool renderInstructionalButtons = true;


		bool renderMovie = true;

#if defined(VIDEO_PLAYBACK_ENABLED) && VIDEO_PLAYBACK_ENABLED
		if( VideoPlayback::IsValidHandle( ms_activePlaybackHandle ) )
		{
			fwuiScaling::eMode const c_scaleMode = 
#if RSG_DURANGO || RSG_ORBIS
				fwuiScaling::UI_SCALE_CLAMP;
#else
				fwuiScaling::UI_SCALE_CONSTRAIN;
#endif

			PUSH_DEFAULT_SCREEN();
			VideoPlayback::RenderToGivenSize( ms_activePlaybackHandle, (u32)VideoResManager::GetUIWidth(), (u32)VideoResManager::GetUIHeight(), c_scaleMode );
			POP_DEFAULT_SCREEN();
		}
		else
#endif // defined(VIDEO_PLAYBACK_ENABLED) && VIDEO_PLAYBACK_ENABLED
		{
#define NUM_FRAMES_NOT_TO_RENDER_BUSY_SPINNER (4)  // dont want to render any busy activity until its been busy for a few frames to avoid a horrible flicker effect as some things take a couple of frames due to scaleform invokes

			static s32 iFramesNotToRenderSpinner = NUM_FRAMES_NOT_TO_RENDER_BUSY_SPINNER;
			
			bool const c_fadeScreen = !(CVideoEditorInterface::IsLoadingProject() && ms_iState != ME_STATE_LOADING && ms_iState != ME_STATE_WAITING_PROJECT_LOAD);

			// render animated spinner during loading or waiting on a project to load as it will do validility checks on that frame (fixes 1 frame of menu before warnings)
			if (ms_iState == ME_STATE_LOADING || ms_iState == ME_STATE_WAITING_PROJECT_LOAD || CVideoEditorInterface::IsLoadingProject())
			{
				if (c_fadeScreen)
				{
					GRCDEVICE.Clear(true, CRGBA(0,0,0,255), false, 0.0f, 0);
				}

				if (iFramesNotToRenderSpinner == 0)
				{
					CPauseMenu::RenderAnimatedSpinner(-1.0f, true, false);
				}
				else
				{
					iFramesNotToRenderSpinner--;
				}

				return;
			}
			else
			{
				iFramesNotToRenderSpinner = NUM_FRAMES_NOT_TO_RENDER_BUSY_SPINNER;

				renderMovie = (!IsInitialising() && !CWarningScreen::IsActive() && !CVideoEditorMenu::IsUserConfirmationScreenActive()) && 
                    ( !CVideoEditorPlayback::IsActive() || ( CVideoEditorPlayback::IsActive() && CVideoEditorPlayback::ShouldRender() ) );

				renderInstructionalButtons = renderMovie;
			}

			if (!CVideoEditorPlayback::IsActive())
			{
				GRCDEVICE.Clear(true, CRGBA(0,0,0,255), false, 0.0f, 0);

				if (renderMovie)
				{
					if (!RenderWallpaper())
					{
						renderMovie = false;  // do not render movie if we are unable to render the wallpaper this frame
					}
				}
			}

			if( renderMovie )
			{
				CScaleformMgr::RenderMovie(CVideoEditorUi::GetMovieId(), fwTimer::GetSystemTimeStep(), true);
			}


			if (CVideoEditorPlayback::IsActive())
			{
				if( !ms_bRebuildTimeline && !ms_bRebuildAnchors )
				{
					CVideoEditorPlayback::Render();  // render video editor playback if active
				}
			}
#if USE_CODE_TEXT
			else
			{
				RenderOverlayText();
			}
#endif // #if USE_CODE_TEXT
		}

		renderInstructionalButtons = renderInstructionalButtons && !CControlMgr::IsShowingKeyboardTextbox() && !CVideoEditorPlayback::IsDisplayingCameraShutters();

		if (IsInHideEditorOperation(&renderInstructionalButtons))
		{
			if (renderInstructionalButtons)
			{
				RenderWallpaper();
			}
			else
			{
				CSprite2d::DrawRectGUI(fwRect(0.0f, 0.0f, 1.0f, 1.0f), Color32(0, 0, 0, 255));
				
				if (VideoUploadManager::GetInstance().IsGettingOAuth())
				{
					CPauseMenu::RenderAnimatedSpinner(-1.0f, true, false);
				}
			}

		}
		else
		{
#define NUM_FRAMES_NOT_TO_RENDER_BUSY_ACTIVITY (2)  // dont want to render any busy activity until its been busy for a few frames to avoid a horrible flicker effect as some things take a couple of frames due to scaleform invokes

			static s32 iFramesNotToRenderFadeOutAndSpinner = NUM_FRAMES_NOT_TO_RENDER_BUSY_ACTIVITY;
			bool drawBlockingSpinner = false;
			if ( IsInBlockingOperation( &drawBlockingSpinner ) )
			{
				if (iFramesNotToRenderFadeOutAndSpinner == 0)
				{
					CSprite2d::DrawRectGUI(fwRect(0.0f, 0.0f, 1.0f, 1.0f), Color32(0, 0, 0, 127));

					if( drawBlockingSpinner )
					{
						CPauseMenu::RenderAnimatedSpinner(-1.0f, true, false);
					}
				}
				else
				{
					iFramesNotToRenderFadeOutAndSpinner--;
				}
			}
			else if (CVideoEditorTimeline::IsTimelineSelected() && CVideoEditorTimeline::IsScrollingMediumFast())
			{
				if (!CVideoEditorTimeline::IsPreviewing())
				{
					CPauseMenu::RenderAnimatedSpinner(Vector2(0.5f, 0.38f), true, false);
				}
			}
			else
			{
				iFramesNotToRenderFadeOutAndSpinner = NUM_FRAMES_NOT_TO_RENDER_BUSY_ACTIVITY;
			}
		}

		if( renderInstructionalButtons ) 
		{
			CVideoEditorInstructionalButtons::Render();
		}
	}
	else
	{
		GRCDEVICE.Clear(true, CRGBA(0,0,0,255), false, 0.0f, 0);  // ensure black background is always behind

		// render animated spinner during loading
		if (ms_iState == ME_STATE_LOADING)
		{
			CPauseMenu::RenderAnimatedSpinner(-1.0f, true, false);
		}
	}

#if __BANK
	if (PARAM_showEditorMemory.Get())
	{
		const s32 iMemoryUsage = CScaleformMgr::GetMemoryUsage(CVideoEditorUi::GetMovieId());

		CTextLayout StreamingVisualiseDebugText;
		StreamingVisualiseDebugText.SetScale(Vector2(0.5f, 1.0f));
		StreamingVisualiseDebugText.SetColor( Color32(0xFFE15050) );

		char cText[32];
		formatf(cText, "MEM %d", iMemoryUsage, NELEM(cText));
		StreamingVisualiseDebugText.Render( Vector2(0.05f, 0.05f), cText);
		CText::Flush();
	}
#endif
}

void CVideoEditorUi::RenderLoadingScreenOverlay()
{
	if (CVideoEditorPlayback::IsActive() && CVideoEditorInterface::ShouldShowLoadingScreen())
	{
		if (ms_codeSprite[CODE_SPRITE_LOGO].GetTexture() && ms_codeSprite[CODE_SPRITE_LOGO].GetShader())
		{
			grcStateBlock::SetRasterizerState(grcStateBlock::RS_NoBackfaceCull);
			grcStateBlock::SetDepthStencilState(grcStateBlock::DSS_IgnoreDepth);
			grcStateBlock::SetBlendState(ms_StandardSpriteBlendStateHandle);

			ms_codeSprite[CODE_SPRITE_LOGO].SetRenderState();

			Vector2 vSize = Vector2(CVideoEditorMenu::ms_MenuArray.CVideoEditorPositioning[CODE_SPRITE_LOGO].size_x, CVideoEditorMenu::ms_MenuArray.CVideoEditorPositioning[CODE_SPRITE_LOGO].size_y);
			CHudTools::AdjustForWidescreen(WIDESCREEN_FORMAT_SIZE_ONLY, NULL, &vSize, NULL);

			const Vector2& vPos = Vector2(CVideoEditorMenu::ms_MenuArray.CVideoEditorPositioning[CODE_SPRITE_LOGO].pos_x-(vSize.x * 0.5f), CVideoEditorMenu::ms_MenuArray.CVideoEditorPositioning[CODE_SPRITE_LOGO].pos_y-(vSize.y * 0.5f));

			Vector2 v[4], uv[4];
			v[0].Set(vPos.x,vPos.y+vSize.y);
			v[1].Set(vPos.x,vPos.y);
			v[2].Set(vPos.x+vSize.x,vPos.y+vSize.y);
			v[3].Set(vPos.x+vSize.x,vPos.y);

#define __UV_OFFSET (0.002f)
			uv[0].Set(__UV_OFFSET,1.0f-__UV_OFFSET);
			uv[1].Set(__UV_OFFSET,__UV_OFFSET);
			uv[2].Set(1.0f-__UV_OFFSET,1.0f-__UV_OFFSET);
			uv[3].Set(1.0f-__UV_OFFSET,__UV_OFFSET);

			ms_codeSprite[CODE_SPRITE_LOGO].DrawSwitch(true, false, 0.0f, v[0], v[1], v[2], v[3], uv[0], uv[1], uv[2], uv[3], CRGBA(255,255,255,255));

			grcStateBlock::SetRasterizerState(grcStateBlock::RS_Default);
			grcStateBlock::SetDepthStencilState(grcStateBlock::DSS_Default);
			grcStateBlock::SetBlendState(grcStateBlock::BS_Default);
			CSprite2d::ClearRenderState();
			CShaderLib::SetGlobalEmissiveScale(1.0f, true);
		}

		if( CGameStreamMgr::GetGameStream()->IsForceRenderOn() )
		{
			CGameStreamMgr::Render();
		}

		// If video capture is buffered, need to run this in loading screen to pass through buffered frames to encoder
		VideoRecording::CaptureVideoFrame(0.0f);

        if( CVideoEditorPlayback::IsActive() && CVideoEditorPlayback::ShouldRenderIBDuringLoading() )
        { 
           CVideoEditorInstructionalButtons::Render();
        }
	}
}

bool CVideoEditorUi::RenderWallpaper()
{
	GRCDEVICE.Clear(true, CRGBA(0,0,0,255), false, 0.0f, 0);  // ensure black background is always behind the wallpaper

	if (ms_codeSprite[CODE_SPRITE_WALLPAPER].GetTexture() && ms_codeSprite[CODE_SPRITE_WALLPAPER].GetShader())
	{
		grcStateBlock::SetRasterizerState(grcStateBlock::RS_NoBackfaceCull);
		grcStateBlock::SetDepthStencilState(grcStateBlock::DSS_IgnoreDepth);
		grcStateBlock::SetBlendState(ms_StandardSpriteBlendStateHandle);

		ms_codeSprite[CODE_SPRITE_WALLPAPER].SetRenderState();

		const Vector2& vPos = Vector2(CVideoEditorMenu::ms_MenuArray.CVideoEditorPositioning[CODE_SPRITE_WALLPAPER].pos_x, CVideoEditorMenu::ms_MenuArray.CVideoEditorPositioning[CODE_SPRITE_WALLPAPER].pos_x);
		const Vector2& vSize = Vector2(CVideoEditorMenu::ms_MenuArray.CVideoEditorPositioning[CODE_SPRITE_WALLPAPER].size_x, CVideoEditorMenu::ms_MenuArray.CVideoEditorPositioning[CODE_SPRITE_WALLPAPER].size_y);

		Vector2 v[4], uv[4];
		v[0].Set(vPos.x,vPos.y+vSize.y);
		v[1].Set(vPos.x,vPos.y);
		v[2].Set(vPos.x+vSize.x,vPos.y+vSize.y);
		v[3].Set(vPos.x+vSize.x,vPos.y);

#define __UV_OFFSET (0.002f)
		uv[0].Set(__UV_OFFSET,1.0f-__UV_OFFSET);
		uv[1].Set(__UV_OFFSET,__UV_OFFSET);
		uv[2].Set(1.0f-__UV_OFFSET,1.0f-__UV_OFFSET);
		uv[3].Set(1.0f-__UV_OFFSET,__UV_OFFSET);

		ms_codeSprite[CODE_SPRITE_WALLPAPER].Draw(v[0], v[1], v[2], v[3], uv[0], uv[1], uv[2], uv[3], CRGBA(255,255,255,255));

		grcStateBlock::SetRasterizerState(grcStateBlock::RS_Default);
		grcStateBlock::SetDepthStencilState(grcStateBlock::DSS_Default);
		grcStateBlock::SetBlendState(grcStateBlock::BS_Default);
		CSprite2d::ClearRenderState();
		CShaderLib::SetGlobalEmissiveScale(1.0f, true);

		return true;
	}

	return false;
}


/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorMenu::RenderOverlayText
// PURPOSE: renders any overlay text over the basic timeline movie in code
/////////////////////////////////////////////////////////////////////////////////////////
#if USE_CODE_TEXT
void CVideoEditorUi::RenderOverlayText()
{
	if (CWarningScreen::IsActive() || CVideoEditorMenu::IsUserConfirmationScreenActive())  // do not render overlay tax when warningscreen is active
	{
		return;
	}

	if (ms_project)
	{
		CVideoEditorTimeline::ProcessOverlayOverCentreStageClipAtPlayheadPosition();  // render any existing text on the stage

		if (!CVideoEditorTimeline::IsTimelineSelected())
		{
			if (ShouldRenderOverlayDuringTextEdit())  // render any text we are currently editing:
			{
				sEditedTextProperties &editedText = CTextTemplate::GetEditedText();

				CVideoEditorTimeline::RenderStageText(	editedText.m_text,
														editedText.m_position,
														Vector2(editedText.m_scale,editedText.m_scale),
														editedText.m_colorRGBA,
														editedText.m_style,
														CVideoEditorMenu::GetStagePosition(),
														CVideoEditorTimeline::GetStageSize());
			}
		}
	}
}
#endif // #if USE_CODE_TEXT

void CVideoEditorUi::LoadAudioTrackText()
{
	if( !IsAudioTrackTextLoaded() )
	{
		TheText.RequestAdditionalText("TRACKID", RADIO_WHEEL_TEXT_SLOT);
	}
}

#if RSG_PC
void CVideoEditorUi::UpdateMouseEvent(const GFxValue* args)
{
	if (ms_iState != ME_STATE_ACTIVE)  // if not in 'active' state then ignore anything from actionscript
	{
		return;
	}

	if (CWarningScreen::IsActive() || CVideoEditorMenu::IsUserConfirmationScreenActive())  // fix for 2151809 and 2151661
	{
		return;
	}

	if (uiVerifyf(args[2].IsNumber(), "CVideoEditorUi::UpdateMouseEvent params not compatible: %s", sfScaleformManager::GetTypeName(args[2])))
	{
		const s32 iButtonType = (s32)args[2].GetNumber();

		switch (iButtonType)  // button type
		{
			case BUTTON_TYPE_MENU:
			case BUTTON_TYPE_LIST_SCROLL_ARROWS:
			{
				if (CVideoEditorPlayback::IsActive())
				{
					CVideoEditorPlayback::UpdateMouseEvent(args);
				}
				else if (CVideoEditorMenu::IsMenuSelected())
				{
					CVideoEditorMenu::UpdateMouseEvent(args);
				}
				break;
			}

			case BUTTON_TYPE_VIDEO_CLIP:
			case BUTTON_TYPE_AMBIENT_CLIP:
			case BUTTON_TYPE_AUDIO_CLIP:
			case BUTTON_TYPE_SCORE_CLIP:
			case BUTTON_TYPE_TEXT_CLIP:
			case BUTTON_TYPE_TIMELINE_ARROWS:
			case BUTTON_TYPE_STAGE:
			case BUTTON_TYPE_OVERVIEW_SCROLLER:
			case BUTTON_TYPE_OVERVIEW_BACKGROUND:
			{
				if (CVideoEditorTimeline::IsTimelineSelected())
				{
					CVideoEditorTimeline::UpdateMouseEvent(args);
				}
				break;
			}
			
			case BUTTON_TYPE_CLIP_EDIT_TIMELINE:
			case BUTTON_TYPE_MARKER:
			case BUTTON_TYPE_PLAYBACK_BUTTON:
			{
				if (CVideoEditorPlayback::IsActive())
				{
					CVideoEditorPlayback::UpdateMouseEvent(args);
				}
				break;
			}

			default:
			{
				// nop
			}
		}
	}

}
#endif // #if RSG_PC

bool CVideoEditorUi::IsAudioTrackTextLoaded()
{
	return TheText.HasThisAdditionalTextLoaded( "TRACKID", RADIO_WHEEL_TEXT_SLOT);
}

#if __BANK
/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorUi::DebugCreateVideoEditorBankWidgets
// PURPOSE: Create some bank widgets
/////////////////////////////////////////////////////////////////////////////////////////
void CVideoEditorUi::DebugCreateVideoEditorBankWidgets()
{
	if( ms_bVideoEditorWidgetsCreated )
	{
		return; // already created these, lets bail
	}

	bkBank* bank = BANKMGR.FindBank("ui");
	if( bank == NULL )
	{
		return;
	}

	bank->PushGroup("Video Editor");
	bank->PopGroup();  // "Video Editor"

	ms_bVideoEditorWidgetsCreated = true;
}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorUi::InitWidgets
// PURPOSE: Init the bank widgets
/////////////////////////////////////////////////////////////////////////////////////////
void CVideoEditorUi::InitWidgets()
{
	ms_bVideoEditorWidgetsCreated = false;

	bkBank* pBank = BANKMGR.FindBank( "ui" );
	if( pBank == NULL )
	{
		pBank = &BANKMGR.CreateBank( "ui" );
	}

	if( pBank != NULL )
	{
		pBank->AddButton("Create the Video Editor widgets", &DebugCreateVideoEditorBankWidgets);
	}
}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorUi::ShutdownWidgets
// PURPOSE: Shuts down the bank widgets
/////////////////////////////////////////////////////////////////////////////////////////
void CVideoEditorUi::ShutdownWidgets()
{
	bkBank *bank = BANKMGR.FindBank( "ui" );
	if( bank != NULL )
	{
		bank->Destroy();
	}
}

#endif // __BANK

#endif // #if defined( GTA_REPLAY ) && GTA_REPLAY
// eof
