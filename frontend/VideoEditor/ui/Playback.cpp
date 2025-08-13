/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : Playback.cpp
// PURPOSE : the NG video editor playback ui system
// AUTHOR  : Derek Payne
// STARTED : 27/02/2014
//
/////////////////////////////////////////////////////////////////////////////////
#include "frontend/VideoEditor/ui/Playback.h"

#if defined( GTA_REPLAY ) && GTA_REPLAY

// rage
#include "math/amath.h"
#include "rline/rlgamerinfo.h"
#include "rline/rlsystemui.h"
#include "scaleform/Include/GFxPlayer.h"

// framework
#include "fwlocalisation/templateString.h"
#include "fwsys/timer.h"

// gamez
#include "audio/frontendaudioentity.h"
#include "audio/replayaudioentity.h"
#include "camera/CamInterface.h"
#include "camera/replay/ReplayDirector.h"
#include "camera/replay/ReplayFreeCamera.h"
#include "camera/replay/ReplayPresetCamera.h"
#include "control/replay/ReplayMarkerInfo.h"
#include "control/videorecording/videorecording.h"
#include "frontend/BusySpinner.h"
#include "frontend/GameStreamMgr.h"
#include "frontend/hud_colour.h"
#include "frontend/MousePointer.h"
#include "frontend/PauseMenu.h"
#include "frontend/ProfileSettings.h"
#include "frontend/Scaleform/ScaleFormMgr.h"
#include "Frontend/Scaleform/ScaleFormComplexObjectMgr.h"
#include "Frontend/ui_channel.h"
#include "frontend/VideoEditor/ui/Editor.h"
#include "frontend/VideoEditor/ui/Menu.h"
#include "frontend/VideoEditor/ui/InstructionalButtons.h"
#include "frontend/VideoEditor/ui/Timeline.h"
#include "frontend/VideoEditor/ui/TextTemplate.h"
#include "frontend/VideoEditor/ui/WatermarkRenderer.h"
#include "frontend/VideoEditor/VideoEditorInterface.h"
#include "frontend/WarningScreen.h"
#include "network/Live/livemanager.h"
#include "Peds/ped.h"
#include "renderer/PostProcessFX.h"
#include "replaycoordinator/rendering/AncillaryReplayRenderer.h"
#include "replaycoordinator/ReplayCoordinator.h"
#include "replaycoordinator/ReplayEditorParameters.h"
#include "replaycoordinator/ReplayMetadata.h"
#include "replaycoordinator/rendering/ReplayPostFxRegistry.h"
#include "SaveLoad/savegame_photo_manager.h"
#include "scene/world/GameWorld.h"
#include "system/controlMgr.h"
#include "text/TextFormat.h"
#include "text/TextConversion.h"
#include "vfx/misc/Markers.h"

FRONTEND_OPTIMISATIONS();
//OPTIMISATIONS_OFF();

CComplexObject CVideoEditorPlayback::ms_ComplexObject[MAX_PLAYBACK_COMPLEX_OBJECTS];
atFixedArray<CVideoEditorPlayback::MarkerDisplayObject, CVideoEditorProject::MAX_MARKERS_PER_CLIP> CVideoEditorPlayback::ms_ComplexObjectMarkers;
atArray<CVideoEditorPlayback::MenuOption> CVideoEditorPlayback::ms_activeMenuOptions;

CVideoEditorProject* CVideoEditorPlayback::ms_project = NULL;
s32 CVideoEditorPlayback::ms_clipIndexToEdit;

ePLAYBACK_TYPE CVideoEditorPlayback::ms_playbackType;
CVideoEditorPlayback::eWARNING_PROMPT_ACTION CVideoEditorPlayback::ms_pendingAction	= CVideoEditorPlayback::WARNING_PROMPT_NONE;
CVideoEditorPlayback::eFOCUS_CONTEXT CVideoEditorPlayback::ms_focusContext			= CVideoEditorPlayback::FOCUS_MARKERS;

s32 CVideoEditorPlayback::ms_currentFocusIndex = -1;
s32 CVideoEditorPlayback::ms_currentFocusIndexDepthBeforeFocus = -1;
s32 CVideoEditorPlayback::ms_menuFocusIndex = -1;

s32 CVideoEditorPlayback::ms_editMenuStackedIndex			= -1;
s32 CVideoEditorPlayback::ms_cameraMenuStackedIndex			= -1;

u32 CVideoEditorPlayback::ms_markerDepthCount				= 0;
u32 CVideoEditorPlayback::ms_timeDilationDepthCount			= 0;

s32 CVideoEditorPlayback::ms_pendingMouseToggleDirection		= UNDEFINED_INPUT;
bool CVideoEditorPlayback::ms_hasTriggeredMouseToggleDirection	= false;
u32 CVideoEditorPlayback::ms_mouseToggleDirectionPendingTimeMs	= 0;
float CVideoEditorPlayback::ms_mouseToggleDirectionDelayTimeMs	= 120.f;

u32 CVideoEditorPlayback::ms_markerDragPendingTimeMs								= 0;
CVideoEditorPlayback::eMARKER_DRAG_STATE CVideoEditorPlayback::ms_markerDragState	= CVideoEditorPlayback::MARKER_DRAG_NONE;
CReplayMarkerContext::eEditWarnings CVideoEditorPlayback::ms_currentEditWarning		= CReplayMarkerContext::EDIT_WARNING_NONE;
float CVideoEditorPlayback::ms_warningDisplayTimeRemainingMs						= 0.f;
float CVideoEditorPlayback::ms_cameraEditFovLastFrame								= 0.f;

s32 CVideoEditorPlayback::ms_cameraShuttersFrameDelay								= -1;
bool CVideoEditorPlayback::ms_bKeepCameraShuttersClosed								= false;
s32 CVideoEditorPlayback::ms_tutorialCountdownMs									= -1;

bool CVideoEditorPlayback::ms_jumpedToMarkerOnDragPending	        = false;
bool CVideoEditorPlayback::ms_isScrubbing					        = false;

bool CVideoEditorPlayback::ms_hasBeenEdited					        = false;
bool CVideoEditorPlayback::ms_hasAnchorsBeenEdited			        = false;
bool CVideoEditorPlayback::ms_bActive						        = false;
bool CVideoEditorPlayback::ms_bReadyToPlay					        = false;
bool CVideoEditorPlayback::ms_bWantDelayedClose				        = false;
bool CVideoEditorPlayback::ms_refreshInstructionalButtons	        = false;
bool CVideoEditorPlayback::ms_rebuildMarkers				        = false;
bool CVideoEditorPlayback::ms_refreshEditMarkerState		        = false;
bool CVideoEditorPlayback::ms_shouldRender					        = true;
bool CVideoEditorPlayback::ms_bLoadingScreen				        = false;
bool CVideoEditorPlayback::ms_bLoadingText					        = false;
bool CVideoEditorPlayback::ms_bPreCaching					        = false;
bool CVideoEditorPlayback::ms_bWasScrubbingOnLoad			        = false;
bool CVideoEditorPlayback::ms_bWasKbmLastFrame				        = false;
bool CVideoEditorPlayback::ms_bMouseInEditMenu				        = false;
bool CVideoEditorPlayback::ms_bUpdateAddMarkerButtonState	        = false;
bool CVideoEditorPlayback::ms_bAddNewMarkerOnNextUpdate             = false;
bool CVideoEditorPlayback::ms_bWasPlayingLastFrame                  = false;
bool CVideoEditorPlayback::ms_bDelayedBackOut                       = false;
bool CVideoEditorPlayback::ms_bBlockDraggingUntilPrecacheComplete   = false;

CVideoEditorPlayback::eCREATE_PHOTO_PROGRESS CVideoEditorPlayback::ms_CreatePhotoProgress = CVideoEditorPlayback::CREATE_PHOTO_PROGRESS_IDLE;


#if RSG_PC

bool CVideoEditorPlayback::ms_bMouseHeldDown[MAX_PLAYBACK_CLICKABLE_ITEMS]	=
{
	false
};

s32 CVideoEditorPlayback::ms_mouseHeldMarker								= -1;
float CVideoEditorPlayback::ms_markerXOffset								= 0.f;

#endif // #if RSG_PC

#if defined(SHOW_PLAYBACK_BUTTONS) && SHOW_PLAYBACK_BUTTONS

static char const * const sc_buttonObjectNames[ PLAYBACK_BUTTON_MAX ] = 
{
	"SKIPTOSTART_BUTTON", "REWIND_BUTTON", "PLAYPAUSE_BUTTON", "FASTFORWARD_BUTTON", "SKIPTOEND_BUTTON", 
	"PREV_MARKER_BUTTON", "MARKER_BUTTON", "NEXT_MARKER_BUTTON"
};

s32 CVideoEditorPlayback::ms_playbackButtonStates[ PLAYBACK_BUTTON_MAX ] = 
{
	TOGGLE_BUTTON_NO_FOCUS, TOGGLE_BUTTON_NO_FOCUS, TOGGLE_BUTTON_NO_FOCUS, TOGGLE_BUTTON_NO_FOCUS, TOGGLE_BUTTON_NO_FOCUS,
	TOGGLE_BUTTON_NO_FOCUS, TOGGLE_BUTTON_NO_FOCUS, TOGGLE_BUTTON_NO_FOCUS,
};

#endif // defined(SHOW_PLAYBACK_BUTTONS) && SHOW_PLAYBACK_BUTTONS

// First entry is dummy since the type menu option has different help text based on toggle value
char const * const CVideoEditorPlayback::sc_EditMarkerMenuHelpText[ MARKER_MENU_OPTION_MAX ] = 
{
	"", "VEUI_ED_TNS_TYPE_HLP", "VEUI_ED_MK_TIME_HLP", 
	
	// Filter Menu
	"VEUI_ED_MNU_PFX_HLP", "VEUI_ED_MK_FILT_HLP", "VEUI_ED_MK_FXIN_HLP", "VEUI_ED_MK_FXSA_HLP", "VEUI_ED_MK_FXCO_HLP", "VEUI_ED_MK_FXVG_HLP",
	"VEUI_ED_MK_FXBR_HLP",

	// DOF Menu
	"VEUI_ED_MNU_DOF_HLP", "", "VEUI_ED_MK_DOFI_HLP", "", "VEUI_ED_MK_DOFFD_HLP", "VEUI_ED_MK_DOFFTA_HLP", 

	// Camera Menu
	"VEUI_ED_MNU_CAM_HLP", "", "VEUI_ED_FNE_CAM_HLP", "VEUI_ED_ATT_CAM_HLP", "VEUI_ED_LAT_CAM_HLP",
	"VEUI_BL_MK_CAM_HLP", "VEUI_SH_MK_CAM_HLP", "VEUI_SI_MK_CAM_HLP", "VEUI_SS_MK_CAM_HLP", "", "VEUI_BLE_MK_CAM_HLP",

	// Audio Menu
	"VEUI_ED_MNU_AUD_HLP", "VEUI_ED_SFX_CAT_HLP", "VEUI_ED_SFX_HASH_HLP", "VEUI_ED_MK_SFXOS_HLP", "VEUI_ED_MK_SFX_HLP", "VEUI_ED_MK_DIALOG_HLP", "VEUI_ED_MK_MUS_HLP", "VEUI_ED_MK_AMB_HLP", 
    "VEUI_ED_MK_SCI_HLP", "",

	"VEUI_ED_MK_SPEED_HLP", 
	"VEUI_ED_MK_THUMB_HLP", ""
};

#define MARKER_PERCENT_MULTIPLIER			( 1.25f )
#define MARKER_METRES_MULTIPLIER			( 2.f )

#define MARKER_TRIGGER_THRESHOLD			( 0.1f )

#define SCRUB_MULTIPLIER					( 2.2f )
#define SCRUB_AXIS_THRESHOLD				( 0.01f )
#define FINE_SCRUB_INCREMENT				( 10.f )

#define WARNING_TEXT_DISPLAY_TIME			( 4.f )

#define CAMERA_SHUTTERS_FRAME_DELAY			( 8 )
#define CAMERA_TARGET_ARROW_SCALE			( 0.4f )
#define CAMERA_TARGET_ARROW_PADDING			( 0.2f )

void CVideoEditorPlayback::Open( ePLAYBACK_TYPE playbackType, s32 const clipIndex )
{
	ms_bActive = true;
	
	ms_playbackType = playbackType;
	ms_clipIndexToEdit = clipIndex;
	ms_project = CVideoEditorInterface::GetActiveProject();

	ms_focusContext = IsEditing() ? CVideoEditorPlayback::FOCUS_MARKERS : CVideoEditorPlayback::FOCUS_NONE;
	ms_currentFocusIndex = 0;

	ms_refreshInstructionalButtons = true;
	ms_refreshEditMarkerState = true;

	ms_shouldRender = true;
	ms_bLoadingScreen = false;
	ms_bPreCaching = false;
	ms_bWasKbmLastFrame = false;
	ms_bMouseInEditMenu = false;
	ms_bUpdateAddMarkerButtonState = false;
    ms_bAddNewMarkerOnNextUpdate = false;
    ms_bWasPlayingLastFrame = false;
    ms_bDelayedBackOut = false;

#if RSG_PC
	ms_bMouseHeldDown[CLICKABLE_ITEM_TIMELINE] = false;
	ms_mouseHeldMarker = -1;
#endif //RSG_PC
	ms_currentEditWarning = CReplayMarkerContext::EDIT_WARNING_NONE;
	ms_warningDisplayTimeRemainingMs = 0.f;

	ClearEditFlags();

	/*fwTimer::EndUserPause();*/

#if USE_TEXT_CANVAS
	CTextTemplate::UpdateTemplateWindow();
#endif  // USE_TEXT_CANVAS

	// lock player control
	if (CGameWorld::FindLocalPlayer() && CGameWorld::FindLocalPlayer()->GetPlayerInfo())
	{
		CGameWorld::FindLocalPlayer()->GetPlayerInfo()->DisableControlsFrontend();
	}

	ms_bReadyToPlay = false;
	ms_bWantDelayedClose = false;

	if( !IsBaking() )
	{
		CVideoEditorTimeline::ClearAllClips();
	}

	InitStep1();
	CVideoEditorUi::ShowPlayback();

	CVideoEditorPlayback::DisableMenu();
}

void CVideoEditorPlayback::Close()
{
	CleanupLoadingScreen( true );
	CHelpMessage::ClearAll();

	if( ms_bActive )
	{
        if( IsBaking() )
        {
            WatermarkRenderer::CleanupPlayback();
        }

		ms_bActive = false;

#if USE_TEXT_CANVAS
		CTextTemplate::UpdateTemplateWindow();
#endif  // USE_TEXT_CANVAS

		ClearWarningText();

		fwTimer::StartUserPause();

		// re-enable controls for consistency, but when pause menu opens it will lock controls itself
		if (CGameWorld::FindLocalPlayer() && CGameWorld::FindLocalPlayer()->GetPlayerInfo())
		{
			CGameWorld::FindLocalPlayer()->GetPlayerInfo()->EnableControlsFrontend();
		}

		CVideoEditorInterface::KillPlaybackOrBake( true );
		CVideoEditorInterface::CleanupPlayback();

		gRenderThreadInterface.Flush();

		if( IsViewingRawClip() )
		{
			CVideoEditorUi::ReleaseProject();
			CVideoEditorUi::SetMovieMode(ME_MOVIE_MODE_MENU);
		}
		else if( IsPreviewingProject() )
		{
			CVideoEditorUi::ShowOptionsMenu();
			CVideoEditorUi::SetMovieMode(ME_MOVIE_MODE_MENU);

			CVideoEditorUi::RebuildTimeline();
		}
		else
		{
			if( IsBaking() )
			{
				CVideoEditorUi::ShowExportMenu();
                CVideoEditorUi::SetMovieMode(ME_MOVIE_MODE_MENU);
			}
			else
			{
				CVideoEditorUi::ShowTimeline();  // will need to go back to whereever we came from here
				CVideoEditorUi::SetMovieMode(ME_MOVIE_MODE_TIMELINE);

				CVideoEditorUi::RebuildTimeline();

				if( HasBeenEdited() )
				{
					CVideoEditorUi::SetProjectEdited(true);  // flag as edited so if the project is closed, the UI will ask if the user wants to save it
				}
			}
		}
	}

	Shutdown();
	CAncillaryReplayRenderer::DisableReplayDefaultEffects();
	CReplayMarkerContext::Reset();
}

void CVideoEditorPlayback::Update()
{
	bool forcePlaybackButtonVisUpdate = false;

	CVideoProjectPlaybackController& playbackController = CVideoEditorInterface::GetPlaybackController();

	if( ms_bReadyToPlay )
	{
		if( !IsBaking() && g_SystemUi.IsUiShowing() )
		{
			playbackController.Pause();
		}
		else if ( ms_bWantDelayedClose )
		{
			Close();
			return;
		}
		else if( CheckExportStatus() )
		{
			// Early out the update if export status has changed
			return;
		}
	}
	else
	{
		if( !CPauseMenu::IsClosingDown() )
		{
			bool isReadyToPlay = false;

			switch( ms_playbackType )
			{
			case PLAYBACK_TYPE_BAKE:
				{
					const rlRosCredentials & cred = rlRos::GetCredentials( NetworkInterface::GetLocalGamerIndex() );
					bool isSCAccount = cred.IsValid() && cred.GetRockstarId() != 0 && CLiveManager::GetSocialClubMgr().GetLocalGamerAccountInfo();

					const char* nickname = isSCAccount ? CLiveManager::GetSocialClubMgr().GetLocalGamerAccountInfo()->m_Nickname : TheText.Get("PM_PANE_RPY");

					char const * const exportName = CVideoEditorUi::GetExportName();
					if( CVideoEditorInterface::StartBakeProject( exportName ) )
					{
						WatermarkRenderer::eLOGO_TYPE const c_logoType = WatermarkRenderer::LOGO_TYPE_SOCIAL_CLUB;
						WatermarkRenderer::PrepareForPlayback( nickname, exportName, c_logoType );
						isReadyToPlay = true;
					}
					else
					{
						// We failed, so call into the export status checks to drop into shared error handling
						CheckExportStatus();
					}

					break;
				}

			case PLAYBACK_TYPE_PREVIEW_FULL_PROJECT:
				{
					CVideoEditorInterface::PlayProjectPreview();
					isReadyToPlay = true;
					break;
				}

			case PLAYBACK_TYPE_EDIT_CLIP:
			case PLAYBACK_TYPE_RAW_CLIP_PREVIEW:
				{
					bool const c_isEditing = ms_playbackType == PLAYBACK_TYPE_EDIT_CLIP;

					uiAssertf( ms_clipIndexToEdit >= 0, "CVideoEditorPlayback - Attempting to play clip with negative index!" );
					CVideoEditorInterface::PlayClipPreview( (u32)ms_clipIndexToEdit, c_isEditing );
					isReadyToPlay = true;
					break;
				}

			default:
				{
					uiAssertf(0, "CVideoEditorPlayback - Invalid state in init (%d)", (s32)ms_playbackType);
				}
			}

			ms_bReadyToPlay = isReadyToPlay;
			forcePlaybackButtonVisUpdate = isReadyToPlay;
		}
	}

	// no "else" as we want to drop in from the above block if playback is now ready
	if ( ms_bReadyToPlay )
	{
		if( IsPendingInitStep2() &&
			!ms_ComplexObject[PLAYBACK_OBJECT_PLAYBACK_PANEL].HasInvokesPending() )  // only process Step2 of init if any invokes have been flushed
		{
			InitStep2();
		}

		if( ms_tutorialCountdownMs >= 0 )
		{
			ms_tutorialCountdownMs -= fwTimer::GetNonPausableCamTimeStepInMilliseconds();

			if( ms_tutorialCountdownMs <= 0 )
			{
				CHelpMessage::ClearAll();
			}
		}

		if( ms_cameraShuttersFrameDelay >= 0 && !ms_bKeepCameraShuttersClosed)
		{	
			--ms_cameraShuttersFrameDelay;

			if( ms_cameraShuttersFrameDelay < 0 )
			{
				EndCameraShutterEffect();
			}
		}

#if defined( SHOW_PLAYBACK_BUTTONS ) && SHOW_PLAYBACK_BUTTONS			
		static bool s_onEditMenuLastFrame = false;

		bool const oldKbmValue = ms_bWasKbmLastFrame;
		ms_bWasKbmLastFrame =  
#if RSG_PC
			CControlMgr::GetMainFrontendControl( false ).WasKeyboardMouseLastKnownSource();
#else
			false;
#endif

		bool const kbmChanged = ms_bWasKbmLastFrame != oldKbmValue;
		if( forcePlaybackButtonVisUpdate || kbmChanged || s_onEditMenuLastFrame != IsOnEditMenu() )
		{
			UpdatePlaybackControlVisibility();
		}

		ms_refreshInstructionalButtons = ms_refreshInstructionalButtons || kbmChanged;
		s_onEditMenuLastFrame = IsOnEditMenu();

#endif // defined( SHOW_PLAYBACK_BUTTONS ) && SHOW_PLAYBACK_BUTTONS

		if( ms_refreshInstructionalButtons )
		{
			CVideoEditorInstructionalButtons::Refresh();
			ms_refreshInstructionalButtons = false;
		}

		// Reset override
		if( !playbackController.IsJumping() && CReplayMarkerContext::IsJumpingOverrideTimeSet() )
		{
			CReplayMarkerContext::SetShiftingCurrentMarker( false );
			CReplayMarkerContext::SetJumpingOverrideNonDilatedTimeMs( -1 );
		}

		if( !IsScrubbing() && g_FrontendAudioEntity.IsScrubSoundPlaying() )
		{
			g_FrontendAudioEntity.StopSoundReplayScrubbing();
		}

		UpdateLoadingScreen();
		UpdateWarningText();

		if( !ms_bLoadingScreen && !CVideoEditorInterface::ShouldShowLoadingScreen() && !CVideoEditorInterface::IsPendingCleanup() )
		{
			if( ms_rebuildMarkers )
			{
				ms_rebuildMarkers = !RebuildMarkersOnPlayBar();
			}

			UpdateCameraTargetFocusDependentState();

			float const c_oldCurrentTimeMs = GetEffectiveCurrentClipTimeMs();

            if(ms_bAddNewMarkerOnNextUpdate)
            {
                AddNewEditMarker();
                ms_bAddNewMarkerOnNextUpdate = false;
            }

			UpdateInput();

            bool const c_isPlaying = playbackController.IsPlaying();

            if( !IsScrubbing() && !IsDraggingMarker() &&
                c_isPlaying != ms_bWasPlayingLastFrame )
            {
                ms_refreshInstructionalButtons = true;
                ms_refreshEditMarkerState = true;
            }

            ms_bWasPlayingLastFrame = c_isPlaying;

			float const c_newCurrentTimeMs = GetEffectiveCurrentClipTimeMs();

			if( IsEditing() || IsPreviewingProject() || IsViewingRawClip() )
			{
#if defined(SHOW_PLAYBACK_BUTTONS) && SHOW_PLAYBACK_BUTTONS
				// Always update these buttons as the replay manager may change it's state underneath us
				UpdatePlayPauseButtonState();
				UpdatePlaybackButtonToggleStates();

				if( ms_bUpdateAddMarkerButtonState )
				{
					UpdateAddMarkerButtonState();
				}
#endif

				UpdateTimerText();
				UpdatePlayBar();

				if( !ms_rebuildMarkers  )
				{
					ms_refreshEditMarkerState = IsEditing() && ( ms_refreshEditMarkerState || c_oldCurrentTimeMs != c_newCurrentTimeMs || 
						playbackController.IsFastForwarding() || playbackController.IsPlaying() || playbackController.IsRewinding() );

					if( ms_refreshEditMarkerState )
					{
						// Scrubbing with R-Stick handles focus changing internally
						UpdateFocusedMarker( GetEffectiveCurrentClipTimeMs() );
						CReplayMarkerContext::SetForceCameraShake( false );

						UpdateEditMarkerStates();
						ms_refreshEditMarkerState = false;
					}
				}
			}
			else if( IsBaking() )
			{
				UpdateSpinnerPlaybackProgress();
			}
		}
	}

	UpdatePhotoCreation();
}

void CVideoEditorPlayback::Render()
{
	if( IsActive() )
	{
		if( CVideoEditorInterface::ShouldShowLoadingScreen() )
		{
			GRCDEVICE.Clear(true, Color32(0,0,0,0), false, 0.0f, 0); 
		}
		else if ( !ms_bReadyToPlay || CVideoEditorInterface::IsPendingCleanup() )
		{
			GRCDEVICE.Clear(true, Color32(0,0,0,0), false, 0.0f, 0); 
			CPauseMenu::RenderAnimatedSpinner(-1.0f, true, false);  // render a spinner as we prepare to deal with playback
		}
		else if( ( ms_bPreCaching && !IsBaking() ) || RenderSnapmaticSpinner() )
		{
			CPauseMenu::RenderAnimatedSpinner(-1.0f, true, false);
		}
	}
}

char const * CVideoEditorPlayback::GetMarkerTypeName( u32 const index )
{
	char const * const markerObjectName = !IsEditing() ? "LINE_MARKER" : index == 0 ? "MARKER_LEFT" :
		index == (u32)(GetEditMarkerCount() - 1) ? "MARKER_RIGHT" : "MARKER";

	return markerObjectName;
}

#if RSG_PC
void CVideoEditorPlayback::UpdateMouseEvent(const GFxValue* args)
{
	if ( !IsActive() || IsBaking() || IsWaitingForTutorialToFinish() )  // do not process unless the playback is active
	{
		return;
	}

	if (uiVerifyf(args[2].IsNumber() && args[3].IsNumber(), "MOUSE_EVENT params not compatible: %s %s", sfScaleformManager::GetTypeName(args[2]), sfScaleformManager::GetTypeName(args[3]) ))
	{
		const s32 iButtonType = (s32)args[2].GetNumber();		// button type
		const s32 iEventType = (s32)args[3].GetNumber();		// item type
		char const * const c_objectName = args[4].GetString();	// Object name

		ms_markerXOffset = 0.f;

		switch (iButtonType)
		{
			case BUTTON_TYPE_CLIP_EDIT_TIMELINE:
			{
				if (iEventType == EVENT_TYPE_MOUSE_PRESS)
				{
					ms_bMouseHeldDown[CLICKABLE_ITEM_TIMELINE] = true;

					if( IsOnEditMenu() )
					{
						DisableCurrentMenu();
						ms_focusContext = FOCUS_MARKERS;
					}
				
					ms_refreshInstructionalButtons = true;
				}

				if (iEventType == EVENT_TYPE_MOUSE_RELEASE || iEventType == EVENT_TYPE_MOUSE_RELEASE_OUTSIDE)
				{
					ms_bMouseHeldDown[CLICKABLE_ITEM_TIMELINE] = false;
				}

				break;
			}

			case BUTTON_TYPE_MARKER:
			{
				if( IsEditing() && !IsEditingCamera() && 
					ms_bMouseHeldDown[CLICKABLE_ITEM_TIMELINE] == false )
				{
					s32 const c_actualMarker = GetMarkerIndexForComplexObjectName( c_objectName );

					if( iEventType == EVENT_TYPE_MOUSE_ROLL_OVER &&
						c_actualMarker == ms_currentFocusIndex && ms_mouseHeldMarker == -1 )
					{
						CMousePointer::SetMouseCursorStyle(MOUSE_CURSOR_STYLE_HAND_OPEN);
					}

					if( iEventType == EVENT_TYPE_MOUSE_ROLL_OUT &&
						c_actualMarker == ms_currentFocusIndex && ms_mouseHeldMarker == -1 )
					{
						CMousePointer::SetMouseCursorStyle(MOUSE_CURSOR_STYLE_ARROW);
					}

					if( iEventType == EVENT_TYPE_MOUSE_PRESS )
					{
						ms_markerXOffset = (float)args[5].GetNumber();
						ms_mouseHeldMarker = c_actualMarker;

						if( ms_mouseHeldMarker == 0 )
						{
							CComplexObject* startMarker = GetEditMarkerComplexObject( ms_mouseHeldMarker );
							if( startMarker && startMarker->IsActive() )
							{
								ms_markerXOffset -= startMarker->GetWidth();
							}
						}

						ms_currentFocusIndex = ms_mouseHeldMarker;
						CMousePointer::SetMouseCursorStyle(MOUSE_CURSOR_STYLE_HAND_OPEN);

						JumpToCurrentEditMarker();
						RefreshEditMarkerMenu();
						SetMenuVisible( false );

						ms_refreshEditMarkerState = true;
						ms_refreshInstructionalButtons = true;
					}

					if( iEventType == EVENT_TYPE_MOUSE_RELEASE || iEventType == EVENT_TYPE_MOUSE_RELEASE_OUTSIDE )
					{
						ms_mouseHeldMarker = -1;

						eMOUSE_CURSOR_STYLE const c_cursorStyle = ( iEventType == EVENT_TYPE_MOUSE_RELEASE_OUTSIDE ) ?
							MOUSE_CURSOR_STYLE_ARROW : MOUSE_CURSOR_STYLE_HAND_OPEN;
						
						CMousePointer::SetMouseCursorStyle( c_cursorStyle );
						
						if( IsOnEditMenu() )
						{
							SetMenuVisible( true );
						}

						ms_refreshInstructionalButtons = true;
					}
				}

				break;
			}

#if defined(SHOW_PLAYBACK_BUTTONS) && SHOW_PLAYBACK_BUTTONS

			case BUTTON_TYPE_PLAYBACK_BUTTON:
				{
					if( !IsEditingCamera() && 
						ms_bMouseHeldDown[CLICKABLE_ITEM_TIMELINE] == false && ms_mouseHeldMarker == -1 )
					{
						ePlaybackButtonId const c_targetButton = GetPlaybackButtonId( c_objectName );
						UpdatePlaybackButtonAction( c_targetButton, iEventType );

						if( c_targetButton == PLAYBACK_BUTTON_PLAYPAUSE )
						{
							UpdatePlayPauseButtonState( iEventType );
						}
						else if( c_targetButton == PLAYBACK_BUTTON_REWIND || c_targetButton == PLAYBACK_BUTTON_FASTFORWARD )
						{
							UpdatePlaybackButtonToggleStates( c_targetButton, iEventType );
						}
						else
						{
							UpdatePlaybackButtonStates( c_targetButton, iEventType );
						}
					}

					break;
				}

#endif // defined(SHOW_PLAYBACK_BUTTONS) && SHOW_PLAYBACK_BUTTONS

			case BUTTON_TYPE_MENU:
			{
				if (uiVerifyf(args[6].IsNumber(), "MOUSE_EVENT - 6th param (item index) not compatible: %s", sfScaleformManager::GetTypeName(args[6])))
				{
					s32 const iItemIndex = (s32)args[6].GetNumber();  // item index
					s32 const c_menuFocusThreshold = ms_activeMenuOptions.GetCount() - 1;
					bool const c_isValidIndex = iItemIndex >= 0 && iItemIndex < c_menuFocusThreshold;

					switch( iEventType )
					{
					case EVENT_TYPE_MOUSE_ROLL_OVER:
						{
							if (CMousePointer::HasMouseInputOccurred())  // only if mouse has moved
							{
								if ( c_isValidIndex )
								{
									if( ms_menuFocusIndex != iItemIndex )
									{
										SetItemHovered( iItemIndex );
										UpdateEditMenuState();
									}

									UpdateMenuHelpText( iItemIndex );
								}

								ms_bMouseInEditMenu = true;
							}

							break;
						}

					case EVENT_TYPE_MOUSE_ROLL_OUT:
						{
							if (CMousePointer::HasMouseInputOccurred())  // only if mouse has moved
							{
								ms_bMouseInEditMenu = false;
								UnsetAllItemsHovered();
							}

							ms_pendingMouseToggleDirection = UNDEFINED_INPUT;

							break;
						}

					case EVENT_TYPE_MOUSE_PRESS:
						{
							if ( c_isValidIndex )
							{
								ms_menuFocusIndex = iItemIndex;
								SetItemSelected( ms_menuFocusIndex );
								UpdateMenuHelpText( ms_menuFocusIndex );
								UpdateEditMenuState();

								s32 const arrowClickedIndex = (s32)args[7].GetNumber();
								s32 const c_input = arrowClickedIndex == 0 ? INPUT_FRONTEND_LEFT : 
									arrowClickedIndex == 1 ? INPUT_FRONTEND_RIGHT : UNDEFINED_INPUT;


								ms_hasTriggeredMouseToggleDirection = false;
								ms_mouseToggleDirectionDelayTimeMs = 120.f;
								ms_mouseToggleDirectionPendingTimeMs = fwTimer::GetSystemTimeInMilliseconds();
								ms_pendingMouseToggleDirection = c_input;
							}

							break;
						}

					case EVENT_TYPE_MOUSE_RELEASE:
						{
							if( c_isValidIndex )
							{
								CMousePointer::SetMouseAccept();
							}					

							break;
						}

					default:
						{
							// NOP
						}
					}
				}

				break;
			}

			default:
			{
				// nothing
				break;
			}
		}
	}
}
#endif // #if RSG_PC

void CVideoEditorPlayback::InitStep1()
{
	if( !IsBaking() )
	{
		for (s32 i = 0; i < MAX_PLAYBACK_COMPLEX_OBJECTS; i++)
		{
			ms_ComplexObject[i].Init();
		}

		ms_ComplexObject[PLAYBACK_OBJECT_STAGE_ROOT] = CComplexObject::GetStageRoot(CVideoEditorUi::GetMovieId());

		if (ms_ComplexObject[PLAYBACK_OBJECT_STAGE_ROOT].IsActive())
		{
			ms_ComplexObject[PLAYBACK_OBJECT_PLAYBACK_PANEL] = ms_ComplexObject[PLAYBACK_OBJECT_STAGE_ROOT].AttachMovieClip("PLAYBACK_PANEL", VE_DEPTH_PLAYBACK_PANEL );
			ms_ComplexObject[PLAYBACK_OBJECT_CLIP_EDIT_HELPTEXT] = ms_ComplexObject[PLAYBACK_OBJECT_STAGE_ROOT].AttachMovieClip("HELPTEXT", VE_DEPTH_CLIP_EDIT_HELPTEXT );
			ms_ComplexObject[PLAYBACK_OBJECT_CLIP_EDIT_CAM_OVERLAY] = ms_ComplexObject[PLAYBACK_OBJECT_STAGE_ROOT].AttachMovieClip("CAMERA_ZOOM", VE_DEPTH_CLIP_EDIT_CAMERA_OVERLAY );
			ms_ComplexObject[PLAYBACK_OBJECT_CAM_SHUTTERS] = ms_ComplexObject[PLAYBACK_OBJECT_STAGE_ROOT].AttachMovieClip( "EDITOR_SHUTTER", VE_DEPTH_CLIP_EDIT_CAMERA_SHUTTERS );

			SetCameraOverlayVisible( false );

			if (ms_ComplexObject[PLAYBACK_OBJECT_PLAYBACK_PANEL].IsActive())
			{
				CScaleformMgr::SetComplexObjectDisplayConfig( ms_ComplexObject[PLAYBACK_OBJECT_PLAYBACK_PANEL].GetId(), SF_BASE_CLASS_VIDEO_EDITOR );
				SetupPlaybackButtonState( ms_ComplexObject[PLAYBACK_OBJECT_PLAYBACK_PANEL], IsEditing() );
			}
		}
	}
}

bool CVideoEditorPlayback::IsPendingInitStep2()
{
	// Just check if one of our step 2 objects is initialized yet
	return !ms_ComplexObject[PLAYBACK_OBJECT_BUTTON_PANEL].IsActive();
}

void CVideoEditorPlayback::InitStep2()
{
	if( ms_ComplexObject[PLAYBACK_OBJECT_PLAYBACK_PANEL].IsActive() )
	{
		ms_ComplexObject[PLAYBACK_OBJECT_BUTTON_PANEL] = ms_ComplexObject[PLAYBACK_OBJECT_PLAYBACK_PANEL].GetObject("BUTTONS");
		ms_ComplexObject[PLAYBACK_OBJECT_PLAYBACK_BAR_BG] = ms_ComplexObject[PLAYBACK_OBJECT_PLAYBACK_PANEL].GetObject("PLAYBACK_BAR_BG");
		ms_ComplexObject[PLAYBACK_OBJECT_PLAYBACK_BAR] = ms_ComplexObject[PLAYBACK_OBJECT_PLAYBACK_PANEL].GetObject("PLAYBACK_BAR");
		ms_ComplexObject[PLAYBACK_OBJECT_PLAYBACK_BAR_LIGHT] = ms_ComplexObject[PLAYBACK_OBJECT_PLAYBACK_PANEL].GetObject("PLAYBACK_BAR_LIGHT");
		ms_ComplexObject[PLAYBACK_OBJECT_PLAYBACK_BAR_SCRUBBER] = ms_ComplexObject[PLAYBACK_OBJECT_PLAYBACK_PANEL].AttachMovieClip("PLAYBACK_BAR_SCRUBBER", VE_DEPTH_PLAYBACK_SCRUBBER );
		ms_ComplexObject[PLAYBACK_OBJECT_PLAYBACK_BG] = ms_ComplexObject[PLAYBACK_OBJECT_PLAYBACK_PANEL].GetObject( "PLAYBACK_BACKGROUND" );

		
#if RSG_PC
		ms_ComplexObject[PLAYBACK_OBJECT_CLIP_EDIT_TIMELINE_CLICK_REGION] = ms_ComplexObject[PLAYBACK_OBJECT_PLAYBACK_PANEL].AttachMovieClip("CLIP_EDIT_TIMELINE_CLICK_REGION", VE_DEPTH_CLICKABLE_PLAYBAR );
		ms_ComplexObject[PLAYBACK_OBJECT_CLIP_EDIT_TIMELINE_CLICK_REGION].SetPosition(ms_ComplexObject[PLAYBACK_OBJECT_PLAYBACK_BAR_BG].GetPosition());
		ms_ComplexObject[PLAYBACK_OBJECT_CLIP_EDIT_TIMELINE_CLICK_REGION].SetWidth(ms_ComplexObject[PLAYBACK_OBJECT_PLAYBACK_BAR_BG].GetWidth());

		u32 const c_complexObjectButtonCount( PLAYBACK_OBJECT_BUTTON_LAST - PLAYBACK_OBJECT_BUTTON_FIRST + 1 );

		if( ms_ComplexObject[PLAYBACK_OBJECT_BUTTON_PANEL].IsActive() &&
			uiVerifyf( c_complexObjectButtonCount == PLAYBACK_BUTTON_MAX, 
			"Mismatch in button count (%u) vs. complex objects for buttons (%u)", PLAYBACK_BUTTON_MAX, c_complexObjectButtonCount ) )
		{
			for( u32 buttonIndex = PLAYBACK_BUTTON_FIRST; buttonIndex < PLAYBACK_BUTTON_MAX; ++buttonIndex )
			{
				u32 const c_complexObjectIndex = PLAYBACK_OBJECT_BUTTON_FIRST + buttonIndex;

				char const * buttonName = sc_buttonObjectNames[buttonIndex];
				ms_ComplexObject[c_complexObjectIndex] = ms_ComplexObject[PLAYBACK_OBJECT_BUTTON_PANEL].GetObject( buttonName );
			}
		}
#endif

#if defined(SHOW_PLAYBACK_BUTTONS) && SHOW_PLAYBACK_BUTTONS

		SetPlaybackButtonVisibility( PLAYBACK_BUTTON_PREV_MARKER, IsEditing() );
		SetPlaybackButtonVisibility( PLAYBACK_BUTTON_ADD_MARKER, IsEditing() );
		SetPlaybackButtonVisibility( PLAYBACK_BUTTON_NEXT_MARKER, IsEditing() );

		//! Reset state
		for( s32 index = 0; index < PLAYBACK_BUTTON_MAX; ++index )
		{
			ms_playbackButtonStates[index] = 1;
			SetPlaybackButtonState( index, TOGGLE_BUTTON_NO_FOCUS );
		}

#else

		ms_ComplexObject[PLAYBACK_OBJECT_BUTTON_PANEL].SetVisible( false );

#endif

	}

	ms_refreshEditMarkerState = ms_rebuildMarkers = !IsBaking();

#if RSG_PC

	for (s32 i = 0; i < MAX_PLAYBACK_CLICKABLE_ITEMS; i++)
	{
		ms_bMouseHeldDown[i] = false;
	}

#endif // #if RSG_PC

}

void CVideoEditorPlayback::Shutdown()
{
	if( ms_ComplexObject[PLAYBACK_OBJECT_PLAYBACK_PANEL].IsActive() )
	{
		ms_ComplexObject[PLAYBACK_OBJECT_PLAYBACK_PANEL].SetVisible( false );
	}

	g_FrontendAudioEntity.StopSoundReplayScrubbing();
	CleanupMarkersOnPlayBar();

	// remove all objects:
	for (s32 i = 0; i < MAX_PLAYBACK_COMPLEX_OBJECTS; i++)
	{
		if (ms_ComplexObject[i].IsActive())
		{
			ms_ComplexObject[i].Release();
		}
	}

	ms_bReadyToPlay = false;
	ms_bWasKbmLastFrame = false;
	ms_pendingAction = WARNING_PROMPT_NONE;

	// Turn off game tips
	if(CGameStreamMgr::GetGameStream())
	{
		CGameStreamMgr::GetGameStream()->ForceRenderOff();
		CGameStreamMgr::GetGameStream()->AutoPostGameTipOff();
	}

	CBusySpinner::Off(SPINNER_SOURCE_VIDEO_EDITOR);
	CVideoEditorInstructionalButtons::Refresh();
    CVideoEditorUi::SetFreeReplayBlocksNeeded();
}

void CVideoEditorPlayback::UpdateInput()
{
	if( IsWaitingForTutorialToFinish() || IsTakingSnapmatic() )
	{
		return;
	}

	if( ms_pendingAction != WARNING_PROMPT_NONE )
	{
		UpdateWarningPrompt();
		return;
	}

	if( CVideoEditorMenu::CheckForUserConfirmationScreens() )
	{
		return;
	}

	if( IsEditing() && !IsScrubbing() && !IsDraggingMarker() && !CVideoEditorInterface::ShouldShowLoadingScreen() && !CVideoEditorInterface::IsPendingCleanup() && 
		!CReplayMgr::IsPreCachingScene() )
	{
        // Save functionality
        if( ( ( ms_bWasKbmLastFrame && CControlMgr::GetMainFrontendControl().GetReplaySave().IsReleased() ) || 
            ( !ms_bWasKbmLastFrame && CControlMgr::GetMainFrontendControl().GetFrontendSelect().IsReleased() ) ) && ( HasBeenEdited() || IsEditingCamera() ) )
        {
            u64 const c_saveResult = ms_project->SaveProject( ms_project->GetName(), false );
            CVideoEditorUi::DisplaySaveProjectResultPrompt( c_saveResult );

            bool const c_saveFailed = ( c_saveResult & CReplayEditorParameters::PROJECT_SAVE_RESULT_FAILED ) != 0;
            CVideoEditorUi::SetProjectEdited( c_saveFailed );

            if( !c_saveFailed )
            {
                ClearEditFlags();
                ms_refreshInstructionalButtons = true;
            }

            return;
        }

        if( CControlMgr::GetMainFrontendControl().GetReplaySnapmatic().IsReleased() )
        {
            BeginPhotoCreation();
            return;
        }
	}

#if RSG_PC
	UpdateMouseInput();
#endif // #if RSG_PC

	bool handledBackOut = false;
	CVideoProjectPlaybackController& playbackController = CVideoEditorInterface::GetPlaybackController();

	if( !IsOnEditMenu() )
	{
		// Only render transitions when playing, not scrubbing and not dragging
		bool const c_allowTransitions = playbackController.IsPlaying() && !IsScrubbing() && !IsDraggingMarker();
		CReplayCoordinator::SetShouldRenderTransitionsInEditMode( c_allowTransitions );
	}

	// Can only change playback when we are not loading and not baking
	if( ms_bReadyToPlay && !ms_bLoadingScreen && !IsBaking() )
	{
		bool releasedDragThisFrame = false;

		if( !ms_bWasKbmLastFrame && ( IsPendingMarkerDrag() || IsDraggingMarker() ) )
		{
			u32 const c_currentTimeMs = fwTimer::GetSystemTimeInMilliseconds();
			bool const c_hasBeenDownForThreshold = ( c_currentTimeMs - ms_markerDragPendingTimeMs) > 200;

			if( CanDragMarker() && ( CControlMgr::GetMainFrontendControl().GetFrontendLB().IsDown() || 
				CControlMgr::GetMainFrontendControl().GetFrontendRB().IsDown() ) )
			{
				bool switchToDragging = c_hasBeenDownForThreshold;
				bool didMarkerChangeTime = false;
				ms_refreshInstructionalButtons = ms_refreshInstructionalButtons || switchToDragging;

				float const c_singleMsStep = 0.1f;
				float const c_multiplier = CControlMgr::GetMainFrontendControl().GetFrontendLeft().IsDown() ? -1.f : 
					CControlMgr::GetMainFrontendControl().GetFrontendRight().IsDown() ? 1.f : 0.f;

				float dragAxis = CControlMgr::GetMainFrontendControl().GetFrontendRStickLeftRight().GetNorm();
				dragAxis = dragAxis == 0.f ? c_singleMsStep * c_multiplier : dragAxis;

				if( dragAxis != 0.f  )
				{
					switchToDragging = true;
					didMarkerChangeTime = UpdateRepeatedInputMarkerTime( dragAxis );
					ms_refreshInstructionalButtons = ( didMarkerChangeTime && IsOnTransitionPoint() ) || ms_refreshInstructionalButtons;
				}
				else if( CanSkipMarkerToBeat( ms_currentFocusIndex ) )
				{
					switchToDragging = UpdateMarkerJumpToBeat();
				}

				if( !IsDraggingMarker() && switchToDragging && IsOnEditMenu() )
				{
					SetMenuVisible( false );
				}

				ms_markerDragState = switchToDragging ? MARKER_DRAG_DRAGGING : ms_markerDragState;

				if( dragAxis == 0.f || !didMarkerChangeTime )
				{
					g_FrontendAudioEntity.StopSoundReplayScrubbing();
				}
				else if( ms_markerDragState == MARKER_DRAG_DRAGGING )
				{
					g_FrontendAudioEntity.PlaySoundReplayScrubbing();
				}
			}
			else if( CControlMgr::GetMainFrontendControl().GetFrontendLB().IsReleased() || 
				CControlMgr::GetMainFrontendControl().GetFrontendRB().IsReleased() )
			{
				releasedDragThisFrame = IsPendingMarkerDrag() ? ms_jumpedToMarkerOnDragPending : true;
				ms_markerDragState = MARKER_DRAG_NONE;
				ms_refreshInstructionalButtons = true;
				ms_jumpedToMarkerOnDragPending = false;
				playbackController.Pause();

				g_FrontendAudioEntity.StopSoundReplayScrubbing();

				if( IsOnEditMenu() )
				{
					// Top-level menu, so we need to update the displayed time value
					if( IsEditingEditMarker() )
					{
						s32 menuIndex = -1;
						TryGetMenuOptionAndIndex( MARKER_MENU_OPTION_TIME, menuIndex );

						IReplayMarkerStorage* markerStorage = GetMarkerStorage();
						s32 const c_currentMarkerIndex = GetCurrentEditMarkerIndex();

						if( markerStorage && c_currentMarkerIndex != -1 && menuIndex != -1 )
						{
							UpdateItemTimeValue( menuIndex, (u32)markerStorage->GetMarkerDisplayTimeMs( c_currentMarkerIndex ) );
						}
					}

					SetMenuVisible( true );
				}
			}
		}

		if( !releasedDragThisFrame && ( ( !ms_bWasKbmLastFrame && CControlMgr::GetMainFrontendControl().GetFrontendRS().IsReleased() ) || 
			( ms_bWasKbmLastFrame && CControlMgr::GetMainFrontendControl().GetReplayHideHUD().IsReleased() ) ) )
		{
			ms_shouldRender = !ms_shouldRender;
			g_FrontendAudioEntity.PlaySound( "TOGGLE_ON", "HUD_FRONTEND_DEFAULT_SOUNDSET" );
		}

		if( !IsPendingMarkerDrag() && !IsDraggingMarker() && !releasedDragThisFrame ) 
		{
			bool allowUpdateScrubbing = CanScrub();

			if( !IsEditingCamera() && !IsScrubbing() && CanScrub() )
			{
				if( ( ( !ms_bWasKbmLastFrame && CControlMgr::GetMainFrontendControl().GetFrontendRT().IsPressed() ) ||
						( ms_bWasKbmLastFrame &&  CControlMgr::GetMainFrontendControl().GetReplayPause().IsPressed() ) ) &&
					!playbackController.IsAtEndOfPlayback() && !CReplayMgr::IsPreCachingScene() )
				{
					char const * const audioTrigger = playbackController.IsPaused() ?
						"RESUME" : "PAUSE_FIRST_HALF";

					g_FrontendAudioEntity.PlaySound( audioTrigger, "HUD_FRONTEND_DEFAULT_SOUNDSET" );

					playbackController.TogglePlayOrPause();

					if( IsOnEditMenu() )
					{
						CloseCurrentMenu();
					}

					ms_refreshInstructionalButtons = true;
				}
				
				if( ( ( !ms_bWasKbmLastFrame && CControlMgr::GetMainFrontendControl().GetFrontendLT().IsPressed() ) ||
							( ms_bWasKbmLastFrame && CControlMgr::GetMainFrontendControl().GetReplayMoveToStartPoint().IsPressed() ) ) 
							&& !playbackController.IsAtStartOfPlayback() && !CReplayMgr::IsPreCachingScene() )
				{
					if( IsOnEditMenu() )
					{
						CloseCurrentMenu();
					}

					playbackController.JumpToStart();
					g_FrontendAudioEntity.PlaySound( "SELECT", "HUD_FRONTEND_DEFAULT_SOUNDSET" );

					if( IsEditing() )
					{
						CReplayMarkerContext::SetJumpingOverrideNonDilatedTimeMs( playbackController.GetPlaybackCurrentClipStartNonDilatedTimeMs() );
					}

					ms_refreshEditMarkerState = true;
					ms_refreshInstructionalButtons = true;
				}
				
				if( !IsOnEditMenu() )
				{
					if( ( !ms_bWasKbmLastFrame && CControlMgr::GetMainFrontendControl().GetFrontendLeft().IsDown() ) || 
						 ( ms_bWasKbmLastFrame && CControlMgr::GetMainFrontendControl().GetReplayBackFrame().IsDown() ) )
					{
						UpdateFineScrubbing( GetFineScrubbingIncrement( -1.f ) );
						allowUpdateScrubbing = false;
					}
					else if( ( !ms_bWasKbmLastFrame && CControlMgr::GetMainFrontendControl().GetFrontendRight().IsDown() ) || 
								( ms_bWasKbmLastFrame && CControlMgr::GetMainFrontendControl().GetReplayAdvanceFrame().IsDown() ) )
					{
						UpdateFineScrubbing( GetFineScrubbingIncrement( 1.f ) );
						allowUpdateScrubbing = false;
					}
				}

				if( IsOnEditMenu() &&
					( ( !ms_bWasKbmLastFrame && CControlMgr::GetMainFrontendControl().GetFrontendY().IsReleased() ) || 
					( ms_bWasKbmLastFrame && CControlMgr::GetMainFrontendControl().GetReplayDelete().IsReleased() ) ) )
				{
					if( CanDeleteMarkerAtCurrentTime() )
					{
						DeleteCurrentEditMarker();
					}
					else
					{
						ResetCurrentEditMarker();
					}

					ms_refreshInstructionalButtons = true;
				}

                if( CanCopyMarkerProperties() &&
                    ( ( !ms_bWasKbmLastFrame && CControlMgr::GetMainFrontendControl().GetFrontendLS().IsReleased() ) || 
                    ( ms_bWasKbmLastFrame && CControlMgr::GetMainFrontendControl().GetReplayTimelineDuplicateClip().IsReleased() ) ) )
                {
                    CopyCurrentMarker();
                    g_FrontendAudioEntity.PlaySound( "COPY", "HUD_FRONTEND_DEFAULT_SOUNDSET" );
                    ms_refreshInstructionalButtons = true;
                }

                if( CanPasteMarkerProperties() && CControlMgr::GetMainFrontendControl().GetFrontendAccept().IsReleased() )
                {
                    PasteToCurrentMarker();
                    g_FrontendAudioEntity.PlaySound( "PASTE", "HUD_FRONTEND_DEFAULT_SOUNDSET" );
                    RefreshEditMarkerMenu();
                }
			}

			if( IsPreviewingProject() && !IsScrubbing() && ms_project && ms_project->GetClipCount() > 1 )
			{
				if( ( !ms_bWasKbmLastFrame && CControlMgr::GetMainFrontendControl().GetFrontendLB().IsReleased() ) ||
					( ms_bWasKbmLastFrame && CControlMgr::GetMainFrontendControl().GetReplayCycleMarkerLeft().IsReleased() ) )
				{
					StopScrubbing();

					g_FrontendAudioEntity.PlaySound( "NAV_LEFT_RIGHT", "HUD_FRONTEND_DEFAULT_SOUNDSET" );
					playbackController.JumpToPreviousClip();
					allowUpdateScrubbing = false;
				}
				else if( ( !ms_bWasKbmLastFrame && CControlMgr::GetMainFrontendControl().GetFrontendRB().IsReleased() ) ||
					( ms_bWasKbmLastFrame && CControlMgr::GetMainFrontendControl().GetReplayCycleMarkerRight().IsReleased() ) )
				{
					StopScrubbing();

					g_FrontendAudioEntity.PlaySound( "NAV_LEFT_RIGHT", "HUD_FRONTEND_DEFAULT_SOUNDSET" );
					playbackController.JumpToNextClip();
					allowUpdateScrubbing = false;
				}
			}	

			if( IsEditing() && !IsEditingCamera() && !IsScrubbing() && !IsDraggingMarker() && !releasedDragThisFrame )
			{
				bool jumpedToBeat = false;
                bool failedJump = false;
				float const c_effectiveCurrentTimeMs = GetEffectiveCurrentClipTimeMs();
				char const * audioTrigger = NULL;

				if( !IsOnEditMenu() && CanSkipToBeat() )
				{					
					if( ( ms_bWasKbmLastFrame && CControlMgr::GetMainFrontendControl().GetReplayFastForward().IsReleased() ) ||
						( !ms_bWasKbmLastFrame && CPauseMenu::CheckInput( FRONTEND_INPUT_UP, false, CHECK_INPUT_OVERRIDE_FLAG_IGNORE_ANALOGUE_STICKS ) ) )
					{
						float const nextAudioBeatMs = playbackController.CalculateNextAudioBeatNonDilatedTimeMs();
						if( nextAudioBeatMs > c_effectiveCurrentTimeMs )
						{
							jumpedToBeat = playbackController.JumpToNonDilatedTimeMs( nextAudioBeatMs, CReplayMgr::JO_FreezeRenderPhase );
							CReplayMarkerContext::SetJumpingOverrideNonDilatedTimeMs( jumpedToBeat ? nextAudioBeatMs : -1 );		
						}
                        else
                        {
                            failedJump = true;
                        }
					}
					else if( ( ms_bWasKbmLastFrame && CControlMgr::GetMainFrontendControl().GetReplayRewind().IsReleased() ) ||
						( !ms_bWasKbmLastFrame && CPauseMenu::CheckInput( FRONTEND_INPUT_DOWN, false, CHECK_INPUT_OVERRIDE_FLAG_IGNORE_ANALOGUE_STICKS ) ) )
					{
						float const prevAudioBeatMs = playbackController.CalculatePrevAudioBeatNonDilatedTimeMs();
						if( prevAudioBeatMs >= 0 && prevAudioBeatMs < c_effectiveCurrentTimeMs )
						{
							jumpedToBeat = playbackController.JumpToNonDilatedTimeMs( prevAudioBeatMs, CReplayMgr::JO_FreezeRenderPhase );
							CReplayMarkerContext::SetJumpingOverrideNonDilatedTimeMs( jumpedToBeat ? prevAudioBeatMs : -1 );						
						}
                        else
                        {
                            failedJump = true;
                        }
					}

					if( jumpedToBeat )
					{
						audioTrigger = "NAV_LEFT_RIGHT";
						ms_refreshInstructionalButtons = true;
					}
                    else if( failedJump )
                    {
                        audioTrigger = "ERROR";
                    }
				}

				if( !jumpedToBeat )
				{
					s32 nextFocusMarker = -1;

                    if( !ms_bWasKbmLastFrame && ( CControlMgr::GetMainFrontendControl().GetFrontendLB().IsDown() || 
                        CControlMgr::GetMainFrontendControl().GetFrontendRB().IsDown() ) )
					{

                        if( ( CControlMgr::GetMainFrontendControl().GetFrontendLB().IsPressed() || 
                            CControlMgr::GetMainFrontendControl().GetFrontendRB().IsPressed() ) )
                        {
                            float const c_currentTimeMs = c_effectiveCurrentTimeMs;
                            float const c_currentMarkerTimeMs = GetClosestEditMarkerTimeMs();

                            if( abs( c_currentMarkerTimeMs - c_currentTimeMs ) <= MARKER_MIN_BOUNDARY_MS )
                            {
                                if ( ms_currentFocusIndex < 0 )
                                {
                                    JumpToClosestEditMarker();
                                }
                            }
                            else
                            {
                                nextFocusMarker = GetNearestEditMarkerIndex( CControlMgr::GetMainFrontendControl().GetFrontendRB().IsDown() );
                                ms_jumpedToMarkerOnDragPending = true;
                            }
                        }

						ms_markerDragPendingTimeMs = fwTimer::GetSystemTimeInMilliseconds();
						ms_markerDragState = CanDragMarker() ? MARKER_DRAG_PENDING : ms_markerDragState;
						ms_refreshInstructionalButtons = ms_refreshEditMarkerState = true;
					}
					else
					{
                        if( !ms_jumpedToMarkerOnDragPending )
                        {
                            if( ( !ms_bWasKbmLastFrame && CControlMgr::GetMainFrontendControl().GetFrontendLB().IsReleased() ) ||
                                ( ms_bWasKbmLastFrame && CControlMgr::GetMainFrontendControl().GetReplayCycleMarkerLeft().IsReleased() ) )
                            {
                                nextFocusMarker = GetNearestEditMarkerIndex( false );
                            }
                            else if( ( !ms_bWasKbmLastFrame && CControlMgr::GetMainFrontendControl().GetFrontendRB().IsReleased() ) ||
                                ( ms_bWasKbmLastFrame && CControlMgr::GetMainFrontendControl().GetReplayCycleMarkerRight().IsReleased() ) )
                            {
                                nextFocusMarker = GetNearestEditMarkerIndex( true );
                            }
                        }

						ms_markerDragState = MARKER_DRAG_NONE;
					}

					if( nextFocusMarker >= 0 )
					{
						audioTrigger = "NAV_LEFT_RIGHT";

						ms_currentFocusIndex = nextFocusMarker;
						ms_refreshEditMarkerState = true;

						JumpToCurrentEditMarker();
						RefreshEditMarkerMenu();

						allowUpdateScrubbing = false;
					}
				}

				if( audioTrigger )
				{
					g_FrontendAudioEntity.PlaySound( audioTrigger, "HUD_FRONTEND_DEFAULT_SOUNDSET" );
				}
			}

			if( !IsEditingCamera() && !IsDraggingMarker() && allowUpdateScrubbing )
			{
				UpdateScrubbing();
			}
			else if( IsScrubbing() )
			{
				StopScrubbing();
			}

			if( ms_shouldRender && !IsScrubbing() ) // Only allow these controls if the HUD is visible and we aren't scrubbing
			{
				if( IsEditingCamera() )
				{
					UpdateCameraControls();
				}
				else
				{
					UpdateRepeatedInput();

					if( IsOnEditMenu() && 
						 ( CControlMgr::GetMainFrontendControl().GetFrontendAccept().IsReleased() || CMousePointer::IsMouseAccept() ) )
					{
						UpdateFocusedInput( INPUT_FRONTEND_ACCEPT );
						ms_refreshInstructionalButtons = true;
					}
					else if( IsEditing() && 
						( ( CControlMgr::GetMainFrontendControl().GetFrontendAccept().IsReleased() ) || 
							( ms_bWasKbmLastFrame && CControlMgr::GetMainFrontendControl().GetReplayNewMarker().IsReleased() ) ) )
					{
						if( CanAddMarkerAtCurrentTime() )
						{
                            if( IsSpaceToAddMarker() )
                            {
                                CVideoProjectPlaybackController& playbackController = CVideoEditorInterface::GetPlaybackController();
                                playbackController.Pause();
                                ms_bAddNewMarkerOnNextUpdate = true;
                            }
                            else
                            {
                                ms_pendingAction = WARNING_PROMPT_NO_SPACE_FOR_MARKERS;
                            }                
						}
						else
						{
							ActuateCurrentEditMarker();
						}		

						ms_refreshInstructionalButtons = true;
					}
					else if( !DoesContextSupportTopLevelBackOut() && 
						( CControlMgr::GetMainFrontendControl().GetFrontendCancel().IsReleased() || CMousePointer::IsMouseBack() ) )
					{
						if ( IsOnPostFxMenu() )
						{
							ExitPostFxMenu();
						}
						else if ( IsOnDoFMenu() )
						{
							ExitDoFMenu();
						}
						else if ( IsOnCameraMenu() )
						{
							ExitCamMenu();
						}
						else if( IsOnAudioMenu() )
						{
							ExitAudioMenu();
						}
						else if( IsEditingEditMarker() )
						{
							BackoutEditMarkerMenu();
						}

						handledBackOut = true;
					}
					else if( CPauseMenu::CheckInput( FRONTEND_INPUT_LEFT, false, GetNavInputCheckFlags() ) )
					{
						UpdateFocusedInput( INPUT_FRONTEND_LEFT );
					}
					else if( CPauseMenu::CheckInput( FRONTEND_INPUT_RIGHT, false, GetNavInputCheckFlags() ) )
					{
						UpdateFocusedInput( INPUT_FRONTEND_RIGHT );
					}
					else if( ms_bWasKbmLastFrame && CControlMgr::GetMainFrontendControl().GetReplayCameraUp().IsDown() )
					{
						UpdateFocusedInput( INPUT_REPLAY_CAMERAUP );
					}
					else if( ms_bWasKbmLastFrame && CControlMgr::GetMainFrontendControl().GetReplayCameraDown().IsDown() )
					{
						UpdateFocusedInput( INPUT_REPLAY_CAMERADOWN );
					}
					else if( CPauseMenu::CheckInput( FRONTEND_INPUT_UP ) || ( CMousePointer::IsMouseWheelUp() && ms_bMouseInEditMenu ) )
					{
						UpdateFocusedInput( INPUT_FRONTEND_UP );
					}
					else if (CPauseMenu::CheckInput( FRONTEND_INPUT_DOWN ) || ( CMousePointer::IsMouseWheelDown() && ms_bMouseInEditMenu ) )
					{
						UpdateFocusedInput( INPUT_FRONTEND_DOWN );
					}
				}
			}
		}
	}

	if( !handledBackOut )
    {
        UpdateTopLevelBackoutInput();
    }
}

#if RSG_PC
void CVideoEditorPlayback::UpdateMouseInput()
{
	if( IsWaitingForTutorialToFinish() || !CanDragMarker() )
	{
		return;
	}

	bool const c_leftPressed = ( ioMouse::GetButtons() & ioMouse::MOUSE_LEFT );

	if( IsOnEditMenu() && ms_pendingMouseToggleDirection != UNDEFINED_INPUT )
	{
		if( c_leftPressed )
		{
			u32 const c_currentTimeMs = fwTimer::GetSystemTimeInMilliseconds();
			bool const c_hasBeenDownForThreshold = ( c_currentTimeMs - ms_mouseToggleDirectionPendingTimeMs) > (u32)ms_mouseToggleDirectionDelayTimeMs;

			if( c_hasBeenDownForThreshold )
			{
				ms_hasTriggeredMouseToggleDirection = true;

				UpdateFocusedInput( ms_pendingMouseToggleDirection );
				ms_mouseToggleDirectionPendingTimeMs = c_currentTimeMs;

				ms_mouseToggleDirectionDelayTimeMs -= 4.f;
				ms_mouseToggleDirectionDelayTimeMs = Max( ms_mouseToggleDirectionDelayTimeMs, IsOnAcceleratingMenu() ? 2.f : 120.f );
			}
		}
		else
		{
			if( !ms_hasTriggeredMouseToggleDirection )
			{								
				UpdateFocusedInput( ms_pendingMouseToggleDirection );
			}

			ms_pendingMouseToggleDirection = UNDEFINED_INPUT;
		}	
	}
	else
	{
		ms_mouseHeldMarker = c_leftPressed ? ms_mouseHeldMarker : -1;
		ms_bMouseHeldDown[CLICKABLE_ITEM_TIMELINE] = ms_bMouseHeldDown[CLICKABLE_ITEM_TIMELINE] && c_leftPressed;
		bool const c_mouseMovedThisFrame = ioMouse::GetDX() != 0;
		
		CVideoProjectPlaybackController& playbackController = CVideoEditorInterface::GetPlaybackController();

		float const c_mousePosX = (float)ioMouse::GetX();
		float const c_mousePosY = (float)ioMouse::GetY();

		float mousePosXScaleform = c_mousePosX;
		float mousePosYScaleform = c_mousePosY;

		CScaleformMgr::TranslateWindowCoordinateToScaleformMovieCoordinate( 
			c_mousePosX, c_mousePosY, CVideoEditorUi::GetMovieId(), mousePosXScaleform, mousePosYScaleform );

		mousePosXScaleform -= ms_markerXOffset;

		IReplayMarkerStorage* markerStorage = GetMarkerStorage();
		sReplayMarkerInfo* heldMarker = markerStorage ? markerStorage->TryGetMarker( ms_mouseHeldMarker ) : NULL;
		if( heldMarker && IsEditing() ) // mouse button held down, selecting a marker
		{
			CMousePointer::SetMouseCursorStyle(MOUSE_CURSOR_STYLE_HAND_GRAB);

			if( c_mouseMovedThisFrame )
			{
				float const c_barX = GetPlaybackBarBaseStartPositionX();
				float const c_mouseOffset = mousePosXScaleform - c_barX;

				float const c_barWidth = GetPlaybackBarMaxWidth();
				float const c_clickPercent = ( 1.f / c_barWidth ) * c_mouseOffset;
				float const c_clickPercentClamped = Clamp( c_clickPercent, 0.f, 1.f );

				bool const c_markerTimeChanged = UpdateMarkerDrag( ms_mouseHeldMarker, c_clickPercentClamped );
				if( c_markerTimeChanged )
				{
					float const c_markerNonDilatedTimeMs = heldMarker->GetNonDilatedTimeMs();

					// HACK - B*2053311 - Replay system takes a frame to jump times, so we need to override time given to other systems
					CReplayMarkerContext::SetJumpingOverrideNonDilatedTimeMs( c_markerNonDilatedTimeMs );
					CReplayMarkerContext::SetShiftingCurrentMarker(true);
					ms_refreshInstructionalButtons = playbackController.JumpToNonDilatedTimeMs( c_markerNonDilatedTimeMs, CReplayMgr::JO_None ) || ms_refreshInstructionalButtons;

					SetHasBeenEdited();
					SetAnchorsHasBeenEdited();
					RefreshMarkerPositionsOnPlayBar();
					ms_refreshEditMarkerState = true;

					if( IsOnEditMenu() )
					{
						// Top-level menu, so we need to update the displayed time value
						if( IsEditingEditMarker() )
						{
							s32 menuIndex = -1;
							TryGetMenuOptionAndIndex( MARKER_MENU_OPTION_TIME, menuIndex );

							IReplayMarkerStorage* markerStorage = GetMarkerStorage();

							if( markerStorage && menuIndex != -1 )
							{
								UpdateItemTimeValue( menuIndex, (u32)markerStorage->GetMarkerDisplayTimeMs( ms_mouseHeldMarker ) );
							}
						}
					}

					g_FrontendAudioEntity.PlaySoundReplayScrubbing();
				}
			}
			else
			{
				g_FrontendAudioEntity.StopSoundReplayScrubbing();
				CReplayMarkerContext::SetShiftingCurrentMarker(false);
			}
		}
		else if ( ms_bMouseHeldDown[CLICKABLE_ITEM_TIMELINE] ) // mouse button held down to scrub along playback bar
		{
			float const c_barX = GetPlaybackBarClickableRegionPositionX();
			float const c_mouseOffset = mousePosXScaleform - c_barX;

			float const c_barWidth = GetPlaybackBarClickableRegionWidth();
			float const c_clickPercent = ( 1.f / c_barWidth ) * c_mouseOffset;
			float const c_clickPercentClamped = Clamp( c_clickPercent, 0.f, 1.f );

			if( IsEditing() || IsViewingRawClip() )
			{
				float const c_currentTimeMs = playbackController.GetEffectiveCurrentClipTimeMs();
				float const c_jumpTimeMs = ms_project->ConvertPlaybackPercentageToClipNonDilatedTimeMs( ms_clipIndexToEdit, c_clickPercentClamped );
	
				if( c_currentTimeMs != c_jumpTimeMs )
				{
					playbackController.JumpToNonDilatedTimeMs( c_jumpTimeMs, CReplayMgr::JO_None );
					UpdateFocusedMarker( c_jumpTimeMs );
					g_FrontendAudioEntity.PlaySoundReplayScrubbing();

					ms_bUpdateAddMarkerButtonState = true;
				}
				else
				{
					g_FrontendAudioEntity.StopSoundReplayScrubbing();
				}
			}
			else if( IsPreviewingProject() )
			{
				u32 const c_currentClipIndex = (u32)playbackController.GetCurrentClipIndex();
				u32 clipIndex;
				float jumpTimeMs;

				ms_project->ConvertProjectPlaybackPercentageToClipIndexAndClipNonDilatedTimeMs( c_clickPercentClamped, clipIndex, jumpTimeMs );

				if( clipIndex == c_currentClipIndex )
				{
					float const c_currentTimeMs = playbackController.GetEffectiveCurrentClipTimeMs();
					if( c_currentTimeMs != jumpTimeMs )
					{
						g_FrontendAudioEntity.PlaySoundReplayScrubbing();
						playbackController.JumpToNonDilatedTimeMs( jumpTimeMs, CReplayMgr::JO_None );
					}
					else
					{
						g_FrontendAudioEntity.StopSoundReplayScrubbing();
					}
				}
				else
				{
					playbackController.JumpToClip( clipIndex, jumpTimeMs );
				}
			}

		}
	}
}

#endif // #if RSG_PC

void CVideoEditorPlayback::UpdateWarningPrompt()
{
    if(  ms_pendingAction == WARNING_PROMPT_SNAPMATIC_FULL )
    {
        CWarningScreen::SetMessageWithHeaderAndSubtext(WARNING_MESSAGE_STANDARD, "SG_HDNG", "CELL_CAM_FW_1", "CELL_CAM_FW_2", FE_WARNING_OK );

        eWarningButtonFlags const c_result = CWarningScreen::CheckAllInput(false);
        if( c_result == FE_WARNING_OK )
        {
            // Player has been warned, return to normal
            ms_pendingAction = WARNING_PROMPT_NONE;
        }
    }
    else if ( ms_pendingAction == WARNING_PROMPT_NO_SPACE_FOR_MARKERS )
    {
        CWarningScreen::SetMessageWithHeader( WARNING_MESSAGE_STANDARD, "VEUI_HDR_ALERT", "VE_NO_SPACE_MARKERS", FE_WARNING_OK );

        eWarningButtonFlags const c_result = CWarningScreen::CheckAllInput(false);
        if( c_result == FE_WARNING_OK )
        {
            // Player has been warned, return to normal
            ms_pendingAction = WARNING_PROMPT_NONE;
        }
    }
	else if( ms_pendingAction == WARNING_PROMPT_EDIT_BACK )
	{
		CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, "VEUI_HDR_ALERT", 
			IsViewingRawClip() ? "VE_PREVIEW_CLIP_BACK" : IsPreviewingProject() ? "VE_PROJ_PREVIEW_BACK" : "VE_EDIT_CLIP_BACK", FE_WARNING_YES_NO );

		eWarningButtonFlags const c_result = CWarningScreen::CheckAllInput(false);
		if( c_result == FE_WARNING_YES)
		{
			CVideoEditorInterface::KillPlaybackOrBake( true );
			ms_pendingAction = WARNING_PROMPT_NONE;
			Close();
		}
		else if ( c_result == FE_WARNING_NO )
		{
			// do not commit the action, and return to no prompt
			ms_pendingAction = WARNING_PROMPT_NONE;
		}
	}
	else if( ms_pendingAction == WARNING_PROMPT_EDIT_DISCARD_BACK || ms_pendingAction == WARNING_PROMPT_EDIT_DISCARD_NEXT_CLIP ||
		ms_pendingAction == WARNING_PROMPT_EDIT_DISCARD_PREV_CLIP )
	{
		bool commitAction = false;

		CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, "VEUI_HDR_ALERT", 
			ms_pendingAction == WARNING_PROMPT_EDIT_DISCARD_BACK ? "VE_EDIT_APPLY" : "VE_EDIT_CLIP_CHANGE", FE_WARNING_YES_NO_CANCEL );

		eWarningButtonFlags const c_result = CWarningScreen::CheckAllInput(false);

		if( c_result == FE_WARNING_YES)
		{
			if( ms_project )
			{
				u64 const c_saveResult = ms_project->SaveProject( ms_project->GetName() );
				CVideoEditorUi::DisplaySaveProjectResultPrompt( c_saveResult );

				bool const c_saveFailed = ( c_saveResult & CReplayEditorParameters::PROJECT_SAVE_RESULT_FAILED ) != 0;
				CVideoEditorUi::SetProjectEdited( c_saveFailed );

				if( c_saveFailed )
				{
					commitAction = false;
				}
				else
				{
					ClearEditFlags();
					commitAction = true;
				}
			}
		}
		else if ( c_result == FE_WARNING_NO_ON_X )
		{
			//! Restore the clip to previous state, then move to the next action
			ms_project->RestoreBackupToClip( ms_clipIndexToEdit );
			commitAction = true;
		}
		else if ( c_result == FE_WARNING_CANCEL_ON_B )
		{
			// do not commit the action, and return to no prompt
			ms_pendingAction = WARNING_PROMPT_NONE;
		}

		if( commitAction )
		{
			if( ms_pendingAction == WARNING_PROMPT_EDIT_DISCARD_BACK )
			{
				CVideoEditorInterface::KillPlaybackOrBake( true );
				ms_pendingAction = WARNING_PROMPT_NONE;
				Close();
				return;  // return out as we dont want to end up setting ms_clipIndexToEdit to -1
			}
			else if( ms_pendingAction == WARNING_PROMPT_EDIT_DISCARD_PREV_CLIP )
			{ 
				CVideoEditorInterface::GetPlaybackController().JumpToPreviousClip();
			}
			else if( ms_pendingAction == WARNING_PROMPT_EDIT_DISCARD_NEXT_CLIP )
			{
				CVideoEditorInterface::GetPlaybackController().JumpToNextClip();
			}

			//! Update the edit index, in case we've changed clips
			ms_clipIndexToEdit = CVideoProjectPlaybackController::GetCurrentClipIndex();
			ms_pendingAction = WARNING_PROMPT_NONE;
		}
	}
	else
	{
		ms_pendingAction = WARNING_PROMPT_NONE;
	}
}

bool CVideoEditorPlayback::CanDragMarker()
{
    bool canDrag = true;

    if( ms_bBlockDraggingUntilPrecacheComplete )
    {
        canDrag = false;
        ms_bBlockDraggingUntilPrecacheComplete = CReplayMgr::IsPreCachingScene() || CPauseMenu::GetPauseRenderPhasesStatus();
    }

    return canDrag;
}

void CVideoEditorPlayback::UpdateTopLevelBackoutInput()
{
    // be able to quit at any time, if under the right context
    if( ( CControlMgr::GetMainFrontendControl().GetFrontendCancel().IsReleased() || CMousePointer::IsMouseBack() ) )
    {
        if( DoesContextSupportTopLevelBackOut() )
        {
            HandleTopLevelBackOut();
        }
        else if ( IsLoading() && !ms_bDelayedBackOut )
        {
            CBusySpinner::On( TheText.Get( "VEUI_CLEANING_UP" ), BUSYSPINNER_LOADING, SPINNER_SOURCE_VIDEO_EDITOR );
            ms_bDelayedBackOut = true;
            ms_refreshInstructionalButtons = true;
        }
    }
}

void CVideoEditorPlayback::HandleTopLevelBackOut()
{
    bool shutdown = false;

    if( ms_bDelayedBackOut )
    {
        shutdown = ms_bDelayedBackOut;
    }
    else
    {
        CVideoProjectPlaybackController& playbackController = CVideoEditorInterface::GetPlaybackController();
        g_FrontendAudioEntity.PlaySound( "BACK", "HUD_FRONTEND_DEFAULT_SOUNDSET" );

        if( IsEditing() && HasBeenEdited() )
        {
            ms_pendingAction = WARNING_PROMPT_EDIT_DISCARD_BACK;

            //! Pause, since we've brought up the error prompt
            playbackController.Pause();
        }
        else if ( IsEditing() || IsPreviewingProject() || IsViewingRawClip() )
        {
            ms_pendingAction = WARNING_PROMPT_EDIT_BACK;

            //! Pause, since we've brought up the error prompt
            playbackController.Pause();
        }
        else
        {
            shutdown = true;
        }
    }

    if( shutdown )
    {
        ms_pendingAction = WARNING_PROMPT_NONE;
        CVideoEditorInterface::KillPlaybackOrBake(true);

        if ( IsBaking() )
        {
            CVideoEditorMenu::SetUserConfirmationScreen("VE_BAKE_CAN");  // display warning message after we have returned to menu
			CVideoEditorUi::RefreshVideoViewWhenSafe();
        }

        Close();
    }

    ms_bDelayedBackOut = false;
}

bool CVideoEditorPlayback::IsOnAcceleratingMenu()
{
	MenuOption const * const c_option = TryGetMenuOption(ms_menuFocusIndex);
	
	bool const c_isOnAcceleratingMenu = ( IsOnEditMenu() && c_option ) && ( (IsEditingEditMarker() && c_option->m_option == MARKER_MENU_OPTION_TIME ) || 
		( IsOnPostFxMenu() && ( c_option->m_option == MARKER_MENU_OPTION_FILTER_INTENSITY || c_option->m_option == MARKER_MENU_OPTION_SATURATION ||
		c_option->m_option == MARKER_MENU_OPTION_CONTRAST || c_option->m_option == MARKER_MENU_OPTION_VIGNETTE || 
		c_option->m_option == MARKER_MENU_OPTION_BRIGHTNESS ) ) ||
		( IsOnDoFMenu() && ( c_option->m_option == MARKER_MENU_OPTION_FOCUS_DISTANCE ||c_option->m_option == MARKER_MENU_OPTION_DOF_INTENSITY ) ) );

	return c_isOnAcceleratingMenu;
}

u32 CVideoEditorPlayback::GetNavInputCheckFlags()
{
	u32 const c_flags = IsOnAcceleratingMenu() && !ms_bWasKbmLastFrame ? CHECK_INPUT_OVERRIDE_FLAG_IGNORE_ANALOGUE_STICKS : 0;
	return c_flags;
}

bool CVideoEditorPlayback::CanScrub()
{
	return !IsEditingCamera() && !CVideoEditorInterface::ShouldShowLoadingScreen() && !CVideoEditorInterface::IsPendingCleanup()
#if RSG_PC
		&& ms_mouseHeldMarker == -1;
#else
		;
#endif
}

void CVideoEditorPlayback::SetHasBeenEdited()
{
	ms_hasBeenEdited = true;
	ms_refreshInstructionalButtons = true;
}

void CVideoEditorPlayback::SetAnchorsHasBeenEdited()
{
	ms_hasAnchorsBeenEdited = true;
	ms_refreshInstructionalButtons = true;
}

void CVideoEditorPlayback::ClearEditFlags()
{
	if( IsEditing() )
	{
		ms_project->BackupClipSettings( ms_clipIndexToEdit );
		ms_hasBeenEdited = ms_hasAnchorsBeenEdited = false;
	}
}

void CVideoEditorPlayback::UpdateThumbnail()
{
	if( IsEditing() && ms_project && !ms_project->DoesClipHavePendingBespokeCapture( ms_clipIndexToEdit ) )
	{
		CVideoEditorTimeline::ClearAllStageClips(true);
		CVideoEditorUi::RefreshThumbnail( ms_clipIndexToEdit );
		ms_project->CaptureBespokeThumbnailForClip( ms_clipIndexToEdit );
		SetHasBeenEdited();
		BeginCameraShutterEffect(false);
	}
}

void CVideoEditorPlayback::UpdateTimerText()
{
	SimpleString_256 finalTimeString;

	CVideoProjectPlaybackController& playbackController = CVideoEditorInterface::GetPlaybackController();
	if ( playbackController.CanQueryStartAndEndTimes() && !CVideoEditorInterface::ShouldShowLoadingScreen() && !CVideoEditorInterface::IsPendingCleanup() )
	{
		SimpleString_128 timeString;

		u32 const c_currentTimeMilliseconds		= (u32)playbackController.GetPlaybackCurrentTimeMs();
		u32 const c_currentTimeMillisecondsPart = ( c_currentTimeMilliseconds % 1000 ) / 10;
		u32 const c_currentTimeSecondsPart		= ( c_currentTimeMilliseconds / 1000 ) % 60;
		u32 const c_currentTimeMinutesPart		= ( c_currentTimeMilliseconds / 1000 ) / 60;

		u32 const c_endTimeMilliseconds			= (u32)playbackController.GetPlaybackEndTimeMs();
		u32 const c_endTimeMillisecondsPart		= ( c_endTimeMilliseconds % 1000 ) / 10;
		u32 const c_endTimeSecondsPart			= ( c_endTimeMilliseconds / 1000 ) % 60;
		u32 const c_endTimeMinutesPart			= ( c_endTimeMilliseconds / 1000 ) / 60;

		if( IsEditing() )
		{
			u32 const c_currentProjTimeMilliseconds	= c_currentTimeMilliseconds +
							( ms_project ? (u32)ms_project->GetTrimmedTimeToClipMs( ms_clipIndexToEdit ) : 0 );
			u32 const c_currentProjTimeMillisecondsPart = ( c_currentProjTimeMilliseconds % 1000 ) / 10;
			u32 const c_currentProjTimeSecondsPart		= ( c_currentProjTimeMilliseconds / 1000 ) % 60;
			u32 const c_currentProjTimeMinutesPart		= ( c_currentProjTimeMilliseconds / 1000 ) / 60;

			u32 const c_projTimeMilliseconds		= ms_project ? (u32)ms_project->GetTotalClipTimeMs() : 0;
			u32 const c_projTimeMillisecondsPart	= ( c_projTimeMilliseconds % 1000 ) / 10;
			u32 const c_projTimeSecondsPart			= ( c_projTimeMilliseconds / 1000 ) % 60;
			u32 const c_projTimeMinutesPart			= ( c_projTimeMilliseconds / 1000 ) / 60;

			timeString.sprintf( "%02u:%02u.%02u / %02u:%02u.%02u ~HUD_COLOUR_GREY~%02u:%02u.%02u / %02u:%02u.%02u", c_currentTimeMinutesPart, c_currentTimeSecondsPart, c_currentTimeMillisecondsPart,
				c_endTimeMinutesPart, c_endTimeSecondsPart, c_endTimeMillisecondsPart,
				c_currentProjTimeMinutesPart, c_currentProjTimeSecondsPart, c_currentProjTimeMillisecondsPart,
				c_projTimeMinutesPart, c_projTimeSecondsPart, c_projTimeMillisecondsPart );
		}
		else if( IsPreviewingProject() || IsBaking() || IsViewingRawClip() )
		{
			timeString.sprintf( "%02u:%02u.%02u / %02u:%02u.%02u", c_currentTimeMinutesPart, c_currentTimeSecondsPart, c_currentTimeMillisecondsPart,
				c_endTimeMinutesPart, c_endTimeSecondsPart, c_endTimeMillisecondsPart );
		}
		
		CTextConversion::TextToHtml( timeString.getBuffer(), finalTimeString.getWritableBuffer(), (s32)finalTimeString.GetMaxByteCount() );
	}

	if (CVideoEditorPlayback::ms_ComplexObject[PLAYBACK_OBJECT_PLAYBACK_PANEL].IsActive())
	{
		if( CScaleformMgr::BeginMethodOnComplexObject( ms_ComplexObject[PLAYBACK_OBJECT_PLAYBACK_PANEL].GetId(), 
			SF_BASE_CLASS_VIDEO_EDITOR, "SET_TIMECODE") )
		{
			CScaleformMgr::AddParamString( finalTimeString.getBuffer(), false );
			CScaleformMgr::EndMethod();
		}
	}
}

void CVideoEditorPlayback::UpdatePlayBar()
{
	CVideoProjectPlaybackController& playbackController = CVideoEditorInterface::GetPlaybackController();
	if ( playbackController.CanQueryStartAndEndTimes() && !CVideoEditorInterface::ShouldShowLoadingScreen() && !CVideoEditorInterface::IsPendingCleanup() )
	{
		if( ms_ComplexObject[PLAYBACK_OBJECT_PLAYBACK_PANEL].IsActive() &&
			ms_ComplexObject[PLAYBACK_OBJECT_PLAYBACK_BAR].IsActive() )
		{
			bool const c_editingCamera = IsEditingCamera();
			bool const c_isVisible = !c_editingCamera && !IsDisplayingCameraShutters();
			ms_ComplexObject[PLAYBACK_OBJECT_PLAYBACK_PANEL].SetVisible( c_isVisible );

			if( ms_ComplexObject[PLAYBACK_OBJECT_CLIP_EDIT_HELPTEXT].IsActive() && 
				ms_ComplexObject[PLAYBACK_OBJECT_PLAYBACK_BAR_BG].IsActive() )
			{
				float const c_yPadding = ms_ComplexObject[PLAYBACK_OBJECT_CLIP_EDIT_HELPTEXT].GetHeight() * 2;
				float const c_yPos = ms_ComplexObject[PLAYBACK_OBJECT_PLAYBACK_BAR_BG].GetYPositionInPixels() - c_yPadding;

				float const c_playbackPanelWidth = ms_ComplexObject[PLAYBACK_OBJECT_PLAYBACK_PANEL].GetWidth();
				float const c_xPos = ( c_playbackPanelWidth / 2.f ) - ( ms_ComplexObject[PLAYBACK_OBJECT_CLIP_EDIT_HELPTEXT].GetWidth() / 2.f );
				ms_ComplexObject[PLAYBACK_OBJECT_CLIP_EDIT_HELPTEXT].SetPositionInPixels( Vector2( c_xPos, c_yPos ) );
			}

			if( !c_editingCamera )
			{
				//! First we need to know how long the bar can possibly be
				float const c_barMaxWidth = GetPlaybackBarMaxWidth();

				// Now we grab the end time, and use it to calculate how much of our bar length 1 millisecond is worth
				float const c_rawEndTimeMs = IsEditing() ?
					playbackController.GetPlaybackCurrentClipRawEndTimeMsAsTimeMs() : playbackController.GetPlaybackEndTimeMs();

				float const c_barSegment = c_barMaxWidth / c_rawEndTimeMs;

				float const c_currentTimeMs = playbackController.GetPlaybackCurrentTimeMs();

				// Values we need to populate...
				float barOffsetX = 0.f;
				float availableBarWidth = c_barMaxWidth;

				if( IsEditing() )
				{
					//! Using that, we can grab the actual offset start time and calculate our offset on X
					float const c_startTimeMs = playbackController.GetPlaybackStartTimeMs();
					barOffsetX = c_startTimeMs * c_barSegment;

					// Grab the end time, now we know how long the "available" bar is. Since this is in "real" time, it's effectively the duration
					float const c_endTimeMs = playbackController.GetPlaybackEndTimeMs();
					availableBarWidth = c_endTimeMs * c_barSegment;	
				}

				// Finally, calculate the playback bar width
				float playbackBarWidth = c_currentTimeMs * c_barSegment;

				// This is our position offset, so apply that straight here
				Vector2 playbackBarsStartingPosition( GetPlaybackBarBaseStartPositionX() + barOffsetX, GetPlaybackBarBaseStartPositionY() );

				if (CVideoEditorPlayback::ms_ComplexObject[PLAYBACK_OBJECT_PLAYBACK_BAR].IsActive())
				{
					CVideoEditorPlayback::ms_ComplexObject[PLAYBACK_OBJECT_PLAYBACK_BAR].SetPositionInPixels(playbackBarsStartingPosition);
					CVideoEditorPlayback::ms_ComplexObject[PLAYBACK_OBJECT_PLAYBACK_BAR].SetWidth( availableBarWidth );
				}

#if RSG_PC
				if (CVideoEditorPlayback::ms_ComplexObject[PLAYBACK_OBJECT_CLIP_EDIT_TIMELINE_CLICK_REGION].IsActive())
				{
					CVideoEditorPlayback::ms_ComplexObject[PLAYBACK_OBJECT_CLIP_EDIT_TIMELINE_CLICK_REGION].SetPositionInPixels(playbackBarsStartingPosition);
					CVideoEditorPlayback::ms_ComplexObject[PLAYBACK_OBJECT_CLIP_EDIT_TIMELINE_CLICK_REGION].SetWidth( availableBarWidth );
				}
#endif

				if (CVideoEditorPlayback::ms_ComplexObject[PLAYBACK_OBJECT_PLAYBACK_BAR_LIGHT].IsActive())
				{
					CVideoEditorPlayback::ms_ComplexObject[PLAYBACK_OBJECT_PLAYBACK_BAR_LIGHT].SetPositionInPixels(playbackBarsStartingPosition);
					CVideoEditorPlayback::ms_ComplexObject[PLAYBACK_OBJECT_PLAYBACK_BAR_LIGHT].SetWidth( playbackBarWidth );
				}

				if( CVideoEditorPlayback::ms_ComplexObject[PLAYBACK_OBJECT_PLAYBACK_BAR_SCRUBBER].IsActive() )
				{
					Vector2 scrubStartingPosition( playbackBarsStartingPosition.x + playbackBarWidth, playbackBarsStartingPosition.y );
					CVideoEditorPlayback::ms_ComplexObject[PLAYBACK_OBJECT_PLAYBACK_BAR_SCRUBBER].SetPositionInPixels( scrubStartingPosition );
				}
			}
		}
	}
}

void CVideoEditorPlayback::UpdateLoadingScreen()
{
	if( ms_bLoadingScreen && CVideoEditorInterface::HasClipLoadFailed() )
	{
		uiDisplayf( "CVideoEditorPlayback::UpdateLoadingScreen - clip load failed, abandoning playback or export" );
		CVideoEditorInterface::KillPlaybackOrBake();
		Close();
		CVideoEditorUi::SetErrorState( "CLIP_LOAD_FAIL" );
	}
	else if( CVideoEditorInterface::ShouldShowLoadingScreen() )
	{
        UpdateTopLevelBackoutInput();
		SetupLoadingScreen();
	}
	else if (ms_bLoadingScreen)
	{
		CleanupLoadingScreen();
	}
	else if( CReplayMgr::IsPreCachingScene() || CPauseMenu::GetPauseRenderPhasesStatus() )
	{
		ms_bPreCaching = !CControlMgr::GetMainFrontendControl().GetReplayAdvanceFrame().IsDown() &&
			!CControlMgr::GetMainFrontendControl().GetReplayBackFrame().IsDown();
	}
	else if (ms_bPreCaching)
	{
		ms_bPreCaching = false;
	}
}

void CVideoEditorPlayback::UpdateEditMenuState()
{
	if( IsOnEditMenu() )
	{
		s32 const c_itemCount = (s32)ms_activeMenuOptions.GetCount();
		for( s32 index = 0; index < c_itemCount; ++index )
		{
			MenuOption const& currentOption = ms_activeMenuOptions[ index ];
			if( !currentOption.IsEnabled()  )
			{
				SetItemActiveDisable( index );
			}
		}
	}
}

void CVideoEditorPlayback::UpdatePlaybackControlVisibility()
{
#if defined(SHOW_PLAYBACK_BUTTONS) && SHOW_PLAYBACK_BUTTONS
	if ( !IsBaking() )
	{
		ms_ComplexObject[PLAYBACK_OBJECT_BUTTON_PANEL].SetVisible( ms_bWasKbmLastFrame );
	}
#endif // defined(SHOW_PLAYBACK_BUTTONS) && SHOW_PLAYBACK_BUTTONS
}

void CVideoEditorPlayback::UpdateInstructionalButtonsPlaybackShortcuts( CScaleformMovieWrapper& buttonsMovie )
{
	if( buttonsMovie.IsActive() && DoesContextSupportQuickPlaybackControls() )
	{
		CVideoProjectPlaybackController const& playbackController = CVideoEditorInterface::GetPlaybackController();

		if( !IsOnEditMenu() )
		{
			if( ms_bWasKbmLastFrame )
			{
				CVideoEditorInstructionalButtons::AddButton( INPUTGROUP_CURSOR_SCROLL, TheText.Get("VEUI_SCRUB"));
			}
			else
			{
				CVideoEditorInstructionalButtons::AddButton(INPUT_FRONTEND_RIGHT_AXIS_X, TheText.Get("VEUI_SCRUB"));
			}
		}

		if( !IsScrubbing() )
		{
			if( !IsOnEditMenu() )
			{
				if( ms_bWasKbmLastFrame )
				{
					CVideoEditorInstructionalButtons::AddButton( INPUT_REPLAY_ADVANCE, INPUT_REPLAY_BACK, TheText.Get("VEUI_SCRUB_FINE"));
				}
				else
				{
					CVideoEditorInstructionalButtons::AddButton(INPUTGROUP_FRONTEND_DPAD_LR, TheText.Get("VEUI_SCRUB_FINE"));
				}

				if( IsEditing() && CanSkipToBeat() )
				{				
					if( ms_bWasKbmLastFrame )
					{
						CVideoEditorInstructionalButtons::AddButton( INPUT_REPLAY_FFWD, INPUT_REPLAY_REWIND, TheText.Get("VEUI_BEAT_SKIP") );
					}
					else
					{
						CVideoEditorInstructionalButtons::AddButton( INPUTGROUP_FRONTEND_DPAD_UD, TheText.Get("VEUI_BEAT_SKIP") );
					}
				}
			}

			if( IsPreviewingProject() && !IsScrubbing() )
			{
				if( ms_project && ms_project->GetClipCount() > 1 )
				{
					if( ms_bWasKbmLastFrame )
					{
						CVideoEditorInstructionalButtons::AddButton( INPUT_REPLAY_CYCLEMARKERRIGHT, INPUT_REPLAY_CYCLEMARKERLEFT, TheText.Get("VEUI_CLIP_SKIP"));
					}
					else
					{
						CVideoEditorInstructionalButtons::AddButton( INPUTGROUP_FRONTEND_BUMPERS, TheText.Get("VEUI_CLIP_SKIP") );
					}
				}
			}
			else if( IsEditing() && !IsEditingCamera()  )
			{
				if( ms_bWasKbmLastFrame )
				{
					CVideoEditorInstructionalButtons::AddButton( INPUT_REPLAY_CYCLEMARKERRIGHT, INPUT_REPLAY_CYCLEMARKERLEFT, TheText.Get("VEUI_MARKER_SKIP"));
				}
				else
				{
					if( IsOnEditMenu() )
					{
						CVideoEditorInstructionalButtons::AddButton( INPUTGROUP_FRONTEND_BUMPERS, TheText.Get("VEUI_MARKER_DRAG"));
					}

					CVideoEditorInstructionalButtons::AddButton( INPUTGROUP_FRONTEND_BUMPERS, TheText.Get("VEUI_MARKER_SKIP"));
				}
			}

			if( !playbackController.IsAtEndOfPlayback() && !IsEditingCamera() )
			{
				InputType const playInput = ms_bWasKbmLastFrame ? INPUT_REPLAY_PAUSE : INPUT_FRONTEND_RT;
                bool const c_isPlaying = playbackController.IsPlaying();

				CVideoEditorInstructionalButtons::AddButton( playInput,
					c_isPlaying ? TheText.Get("VEUI_PAUSE") : TheText.Get("VEUI_PLAY"), true );
			}

			if( !playbackController.IsAtStartOfPlayback() && ( IsPreviewingProject() || IsViewingRawClip() ) )
			{
				InputType const jumpToStartInput = ms_bWasKbmLastFrame ? INPUT_REPLAY_STARTPOINT : INPUT_FRONTEND_LT;
				CVideoEditorInstructionalButtons::AddButton( jumpToStartInput, TheText.Get("VEUI_JUMP_START"), true );
			}
		}
	}
}

void CVideoEditorPlayback::UpdateInstructionalButtonsBaking( CScaleformMovieWrapper& buttonsMovie )
{
	CVideoProjectPlaybackController const& c_playbackController = CVideoEditorInterface::GetPlaybackController();
	if( !IsLoading() && !CVideoEditorInterface::ShouldShowLoadingScreen() && !CVideoEditorInterface::IsPendingCleanup() && 
		c_playbackController.IsEnabled() && 
		c_playbackController.CanQueryStartAndEndTimes() )
	{
		if( buttonsMovie.IsActive() )
		{
			char cFullRenderingString[100];
			formatf(cFullRenderingString, "%s 000%%", TheText.Get("VEUI_RENDERING_VID") );
			CVideoEditorInstructionalButtons::AddButton(ICON_SPINNER, cFullRenderingString);

			CVideoEditorInstructionalButtons::AddButton( INPUT_FRONTEND_CANCEL, TheText.Get("IB_CANCEL"), true );
		}

		UpdateSpinnerPlaybackProgress();
	}
}

void CVideoEditorPlayback::UpdateInstructionalButtonsEditing( CScaleformMovieWrapper& buttonsMovie )
{
	if( IsEditing() && buttonsMovie.IsActive() )
	{
#if RSG_PC
		if( CanSkipMarkerToBeat( CVideoEditorPlayback::ms_mouseHeldMarker ) )
		{
			CVideoEditorInstructionalButtons::AddButton( INPUT_REPLAY_CTRL, TheText.Get("VEUI_BEAT_SKIP_KBM") );
		}
		else 
#endif
		if( IsPendingMarkerDrag() || IsDraggingMarker() )
		{
			if( ms_bWasKbmLastFrame )
			{
				CVideoEditorInstructionalButtons::AddButton( INPUT_CURSOR_SCROLL_UP, TheText.Get("VEUI_SCRUB"));
			}
			else
			{
				CVideoEditorInstructionalButtons::AddButton(INPUT_FRONTEND_RIGHT_AXIS_X, TheText.Get("VEUI_SCRUB"));
			}

			if( ms_bWasKbmLastFrame )
			{
				CVideoEditorInstructionalButtons::AddButton( INPUT_REPLAY_ADVANCE, INPUT_REPLAY_BACK, TheText.Get("VEUI_SCRUB_FINE"));
			}
			else
			{
				CVideoEditorInstructionalButtons::AddButton(INPUTGROUP_FRONTEND_DPAD_LR, TheText.Get("VEUI_SCRUB_FINE"));
			}

			if( CanSkipMarkerToBeat( ms_currentFocusIndex ) )
			{
				if( ms_bWasKbmLastFrame )
				{
					CVideoEditorInstructionalButtons::AddButton( INPUT_REPLAY_FFWD, INPUT_REPLAY_REWIND, TheText.Get("VEUI_BEAT_SKIP") );
				}
				else
				{
					CVideoEditorInstructionalButtons::AddButton( INPUTGROUP_FRONTEND_DPAD_UD, TheText.Get("VEUI_BEAT_SKIP") );
				}
			}
		}
		else
		{
			InputType const cancelInput = INPUT_FRONTEND_CANCEL;

			if( IsEditingCamera() )
			{
				CVideoEditorInstructionalButtons::AddButton( cancelInput, TheText.Get("IB_BACK"), true );
				CVideoEditorInstructionalButtons::AddButton(INPUT_SCRIPT_LS, TheText.Get("VEUI_MARKER_CAM_RESET"), true );

				sReplayMarkerInfo const* currentMarker = GetCurrentEditMarker();
				if( currentMarker )
				{
					if( currentMarker->GetCameraType() != RPLYCAM_OVERHEAD )
					{
						CVideoEditorInstructionalButtons::AddButton( INPUTGROUP_SCRIPT_DPAD_LR, TheText.Get("VEUI_MARKER_CAM_ROLL") );
					}

					CVideoEditorInstructionalButtons::AddButton(INPUTGROUP_SCRIPT_TRIGGERS, TheText.Get("VEUI_MARKER_CAM_ZOOM"));

					bool const c_isFreeCam = currentMarker->IsFreeCamera();
					bool const c_hasLookAtTarget = currentMarker->GetLookAtTargetId() != -1;

					if( c_isFreeCam )
					{
						if( c_hasLookAtTarget )
						{
							CVideoEditorInstructionalButtons::AddButton(INPUTGROUP_SCRIPT_BUMPERS, TheText.Get("VEUI_EFC_TILT"));

							CVideoEditorInstructionalButtons::AddButton(INPUTGROUP_SCRIPT_DPAD_UD, TheText.Get("VEUI_MARKER_CAM_LINMOV"));

							if( ms_bWasKbmLastFrame )
							{
								CVideoEditorInstructionalButtons::AddButton( INPUT_SCRIPT_RDOWN, TheText.Get("VEUI_EFC_OFFSET"));
							}
							else
							{
								CVideoEditorInstructionalButtons::AddButton( INPUTGROUP_SCRIPT_RSTICK_ALL, TheText.Get("VEUI_EFC_OFFSET"));
							}

							CVideoEditorInstructionalButtons::AddButton(INPUTGROUP_SCRIPT_LSTICK_ALL, TheText.Get("VEUI_EFC_ORBIT"));
						}
						else
						{
							CVideoEditorInstructionalButtons::AddButton( INPUTGROUP_SCRIPT_BUMPERS, TheText.Get("VEUI_EFC_HEIGHT"));

							if( ms_bWasKbmLastFrame )
							{
								CVideoEditorInstructionalButtons::AddButton( INPUT_SCRIPT_RDOWN, TheText.Get("VEUI_EFC_LOOK"));
							}
							else
							{
								CVideoEditorInstructionalButtons::AddButton( INPUTGROUP_SCRIPT_RSTICK_ALL, TheText.Get("VEUI_EFC_LOOK"));
							}
							
							CVideoEditorInstructionalButtons::AddButton( INPUTGROUP_SCRIPT_LSTICK_ALL, TheText.Get("VEUI_EFC_MOVE"));
						}
					}
					else
					{
						CVideoEditorInstructionalButtons::AddButton(INPUT_FRONTEND_RIGHT_AXIS_Y, TheText.Get("VEUI_MARKER_CAM_LINMOV"));
					}
				}
			}
			else
			{
                if( IsOnEditMenu() )
                {
                    if( CanPasteMarkerProperties() )
                    {
                        char const * const pasteString = IsOnPostFxMenu() ? "IB_PASTE_MARKER_FX" : "IB_PASTE_MARKER_AUDIO";
                        CVideoEditorInstructionalButtons::AddButton( INPUT_FRONTEND_ACCEPT, TheText.Get( pasteString ), true  );
                    }

                    if( CanCopyMarkerProperties() )
                    {
                        InputType const copyInput = ms_bWasKbmLastFrame ? INPUT_REPLAY_TIMELINE_DUPLICATE_CLIP : INPUT_FRONTEND_LS;
                        char const * const copyString = IsOnPostFxMenu() ? "IB_COPY_MARKER_FX" : "IB_COPY_MARKER_AUDIO";
                        CVideoEditorInstructionalButtons::AddButton( copyInput, TheText.Get( copyString ), true  );
                    }
 
                    MenuOption const * const c_option = TryGetMenuOption(ms_menuFocusIndex);
                    if( uiVerifyf( c_option,
                        "CVideoEditorPlayback::UpdateInstructionalButtonsEditing - Updating helptext with invalid focus index %d of %d total options"
                        , ms_menuFocusIndex, ms_activeMenuOptions.GetCount() ) )
                    {
                        if( c_option->IsEnabled() )
                        {
                            if( c_option->m_option == MARKER_MENU_OPTION_CAMERA_MENU || c_option->m_option == MARKER_MENU_OPTION_POSTFX_MENU || 
                                c_option->m_option == MARKER_MENU_OPTION_DOF_MENU || c_option->m_option == MARKER_MENU_OPTION_AUDIO_MENU || 
                                c_option->m_option == MARKER_MENU_OPTION_SET_THUMBNAIL )
                            {
                                CVideoEditorInstructionalButtons::AddButton( INPUT_FRONTEND_ACCEPT, TheText.Get("IB_SELECT"), true  );
                            }
                            else if( c_option->m_option == MARKER_MENU_OPTION_EDIT_CAMERA )
                            {
                                CVideoEditorInstructionalButtons::AddButton( INPUT_FRONTEND_ACCEPT, TheText.Get("VEUI_EDIT_CAM"), true  );
                            }
                        }

                        CVideoEditorInstructionalButtons::AddButton( cancelInput, TheText.Get("IB_BACK"), true  );

                        InputType const clearMarkerInput = ms_bWasKbmLastFrame ? INPUT_REPLAY_MARKER_DELETE : INPUT_FRONTEND_Y;
                        char const * const c_markerClearInputLngKey = CanDeleteMarkerAtCurrentTime() ? "VEUI_ED_MK_DEL" : "VEUI_ED_MK_RES";
                        CVideoEditorInstructionalButtons::AddButton( clearMarkerInput, TheText.Get( c_markerClearInputLngKey ), true );

                        if( c_option->IsEnabled() )
                        {
                            if( c_option->m_option == MARKER_MENU_OPTION_TIME || c_option->m_option == MARKER_MENU_OPTION_FILTER_INTENSITY ||
                                c_option->m_option == MARKER_MENU_OPTION_SATURATION || c_option->m_option == MARKER_MENU_OPTION_CONTRAST ||
                                c_option->m_option == MARKER_MENU_OPTION_VIGNETTE || c_option->m_option == MARKER_MENU_OPTION_BRIGHTNESS )
                            {
                                if( ms_bWasKbmLastFrame )
                                {
                                    CVideoEditorInstructionalButtons::AddButton( INPUT_REPLAY_CAMERADOWN, INPUT_REPLAY_CAMERAUP, TheText.Get("VEUI_TIME_ADJ") );
                                }
                                else
                                {
                                    CVideoEditorInstructionalButtons::AddButton( INPUTGROUP_FRONTEND_DPAD_LR, TheText.Get("VEUI_TIME_ADJF") );		
                                }
                            }
                        }
                    }
                }
				else
				{
					if( !IsScrubbing() )
					{
						if( IsEditing() )
						{
							InputType const markerInput = ms_bWasKbmLastFrame ? INPUT_REPLAY_NEWMARKER : INPUT_FRONTEND_ACCEPT;
							char const * const markerString = CanAddMarkerAtCurrentTime() ? "VEUI_ADD_MARKER" : "VEUI_EDIT_MARKER";
							CVideoEditorInstructionalButtons::AddButton( markerInput, TheText.Get( markerString ), true  );

                            if( !IsScrubbing() && !IsTakingSnapmatic() && !IsDraggingMarker() )
                            {
                                CVideoEditorInstructionalButtons::AddButton( INPUT_REPLAY_SNAPMATIC_PHOTO, TheText.Get("VEUI_SNAPMATIC"), true  );
                            }
						}

						CVideoEditorInstructionalButtons::AddButton( cancelInput, TheText.Get("IB_BACK"), true  );
					}

				}
			}


			if( DoesContextSupportQuickPlaybackControls() )
			{
				UpdateInstructionalButtonsPlaybackShortcuts( buttonsMovie );
			}

            bool const c_canShowSaveButton = !IsScrubbing() && !IsTakingSnapmatic() &&
#if RSG_PC
                ( ( HasBeenEdited() && IsOnEditMenu() ) || IsEditingCamera() );
#else
                ( HasBeenEdited() || IsEditingCamera() );
#endif

			if( c_canShowSaveButton )
			{
                if( ms_bWasKbmLastFrame )
                {
                    CVideoEditorInstructionalButtons::AddButton( INPUT_REPLAY_SAVE, TheText.Get("VEUI_SAVE"), true  );
                }
                else
                {
                    CVideoEditorInstructionalButtons::AddButton( INPUT_FRONTEND_SELECT, TheText.Get("VEUI_SAVE"), true  );
                }
			}
		}
	}
}

void CVideoEditorPlayback::UpdateInstructionalButtonsPreview( CScaleformMovieWrapper& buttonsMovie )
{
	if( buttonsMovie.IsActive() )
	{
		if( !IsScrubbing() )
		{
			CVideoEditorInstructionalButtons::AddButton( INPUT_FRONTEND_CANCEL, TheText.Get("IB_BACK"), true );
		}

		UpdateInstructionalButtonsPlaybackShortcuts( buttonsMovie );
	}
}

void CVideoEditorPlayback::UpdateInstructionalButtonsLoading( CScaleformMovieWrapper& buttonsMovie )
{
    if( buttonsMovie.IsActive() && !ms_bDelayedBackOut )
    {
        char const * const c_lngKey = IsBaking() ? "IB_CANCEL" : "IB_BACK";
        CVideoEditorInstructionalButtons::AddButton( INPUT_FRONTEND_CANCEL, TheText.Get( c_lngKey ), true );
    }
}

void CVideoEditorPlayback::UpdateSpinnerPlaybackProgress()
{
	if( ms_bActive )
	{
		CVideoProjectPlaybackController const& c_playbackController = CVideoEditorInterface::GetPlaybackController();

		char cFullRenderingString[100];

		float const c_endTimeMs = c_playbackController.GetPlaybackEndTimeMs();
		float const c_percentilePerMs = ( 100.0f / c_endTimeMs );

		float const c_currentTimeMs = c_playbackController.GetPlaybackCurrentTimeMs();
		u32 const c_percentage = (u32)Clamp( c_percentilePerMs * c_currentTimeMs, 0.f, 100.f );

		formatf(cFullRenderingString, "%s %u%%", TheText.Get("VEUI_RENDERING_VID"), c_percentage );
		
		CVideoEditorInstructionalButtons::UpdateSpinnerText(cFullRenderingString);  // update the spinner progress text with percentage value
	}
}

void CVideoEditorPlayback::AddNewEditMarker()
{
	if( ms_project && IsEditing() )
	{
		IReplayMarkerStorage* markerStorage = ms_project->GetClipMarkers( ms_clipIndexToEdit );
		if( markerStorage )
		{
			s32 markerIndex = -1;
			if( markerStorage->AddMarker( true, true, markerIndex ) )
			{
				CVideoProjectPlaybackController& playbackController = CVideoEditorInterface::GetPlaybackController();

				float const endTime = playbackController.GetPlaybackCurrentClipRawEndTimeMsAsTimeMs();
				Vector2 sourcePosition( GetPlaybackBarBaseStartPositionX(), GetPlaybackBarBaseStartPositionY() );

				float const markerTime = markerStorage->GetMarkerDisplayTimeMs( markerIndex );
				AddMarkerToPlayBar( "MARKER", markerTime, endTime, sourcePosition, (u32)markerIndex, (u32)markerIndex );

				SetHasBeenEdited();

				ms_focusContext = FOCUS_MARKERS;
				ms_currentFocusIndex = markerIndex;
				ms_refreshEditMarkerState = true;
				ms_refreshInstructionalButtons = true;
				ms_bUpdateAddMarkerButtonState = true;

				ActuateCurrentEditMarker();
			}
		}
	}
}

void CVideoEditorPlayback::DeleteCurrentEditMarker()
{
	if( ms_project && ms_currentFocusIndex >= 0 && IsEditing() )
	{
		IReplayMarkerStorage* markerStorage = ms_project->GetClipMarkers( ms_clipIndexToEdit );
		if( markerStorage && markerStorage->GetMarkerCount() > 2 )
		{
			sReplayMarkerInfo* marker = markerStorage->TryGetMarker( (u32)ms_currentFocusIndex );
			if( marker )
			{
				if( marker->IsAnchor() )
				{
					SetAnchorsHasBeenEdited();
				}

				BackoutEditMarkerMenu();

				ms_currentFocusIndexDepthBeforeFocus = -1;
				markerStorage->RemoveMarker( ms_currentFocusIndex );
				RemoveMarkerOnPlaybar( ms_currentFocusIndex );
				RefreshMarkerPositionsOnPlayBar();

				//! Try and switch focus to the closest marker
				ms_currentFocusIndex = GetNearestEditMarkerIndex( false );
				ms_refreshInstructionalButtons = true;
				ms_refreshEditMarkerState = true;
				SetHasBeenEdited();

#if RSG_PC
				CMousePointer::SetMouseCursorStyle( MOUSE_CURSOR_STYLE_ARROW );
#endif
			}
		}
	}
}

void CVideoEditorPlayback::ResetCurrentEditMarker()
{
	if( ms_project && ms_currentFocusIndex >= 0 && IsEditing() )
	{
		IReplayMarkerStorage* markerStorage = ms_project->GetClipMarkers( ms_clipIndexToEdit );
		if( markerStorage )
		{
			markerStorage->ResetMarkerToDefaultState( (u32)ms_currentFocusIndex );
			markerStorage->RecalculateMarkerPositionsOnSpeedChange( (u32)ms_currentFocusIndex );
			ms_refreshInstructionalButtons = true;
			ms_refreshEditMarkerState = true;
			SetHasBeenEdited();

			RefreshEditMarkerMenu();
			RefreshMarkerPositionsOnPlayBar();

            if( ms_currentFocusIndex == 0 || ms_currentFocusIndex == GetLastMarkerIndex() )
            {
                sReplayMarkerInfo const * const c_currentEditMarker = GetCurrentEditMarker();
                CComplexObject* markerComplexObject = c_currentEditMarker ? GetCurrentEditMarkerComplexObject() : NULL;
                if( markerComplexObject )
                {
                    bool const c_hasTransition = c_currentEditMarker->GetTransitionType() != MARKER_TRANSITIONTYPE_NONE;
                    SetEditMarkerState( *markerComplexObject, EDIT_MARKER_EDITING, false );	
                    SetTransitionElementVisible( *markerComplexObject, c_hasTransition );
                }
            }
		}
	}
}

bool CVideoEditorPlayback::IsEditingCamera()
{
	return IsEditing() && ms_focusContext == FOCUS_EDITING_CAMERA;
}

void CVideoEditorPlayback::UpdateEditMarkerStates()
{
	if( IsEditing() )
	{
		IReplayMarkerStorage const * const c_markerStorage = GetMarkerStorage();

		s32 const c_markerCount = ms_ComplexObjectMarkers.GetCount();
		for( s32 i = 0; i < c_markerCount; ++i )
		{
			CComplexObject* currentMarker = GetEditMarkerComplexObject( i );
			if( currentMarker )
			{
				eEDIT_MARKER_FOCUS const c_focusState = IsFocusedOnEditMarker( i ) ? EDIT_MARKER_FOCUS : 
					( IsEditingEditMarker( i ) ? EDIT_MARKER_EDITING : EDIT_MARKER_NO_FOCUS );

				sReplayMarkerInfo const * const c_currentMarker = c_markerStorage ? c_markerStorage->TryGetMarker( i ) : NULL;
				bool const c_isAnchor = c_currentMarker ? c_currentMarker->IsAnchor() : false;

				SetEditMarkerState( *currentMarker, c_focusState, c_isAnchor );
			}
		}

		// Now update time dilation!
		for( s32 i = 0; i < c_markerCount - 1; ++i )
		{
			sReplayMarkerInfo const * const c_currentMarker = c_markerStorage ? c_markerStorage->TryGetMarker( i ) : NULL;
			bool const c_isCurrentMarkerAnAnchor = c_currentMarker ? c_currentMarker->IsAnchor() : false;
			s32 const c_nextNonAnchorMarkerIndex = c_markerStorage ? c_markerStorage->GetNextMarkerIndex( i, true ) : -1;

			CComplexObject* currentTimeDilation = GetTimeDilationComplexObject( i );
			CComplexObject* nextEditMarker = c_nextNonAnchorMarkerIndex > i ? GetEditMarkerComplexObject( c_nextNonAnchorMarkerIndex ) : NULL;

			if( currentTimeDilation && ( c_isCurrentMarkerAnAnchor || nextEditMarker ) )
			{
				bool const c_isSpedUp = c_currentMarker->GetSpeedValueU32() > 100;
				bool const c_isSlowedDown = c_currentMarker->GetSpeedValueU32() < 100;

				float const c_width = c_isCurrentMarkerAnAnchor ? 0.f : ( nextEditMarker->GetXPositionInPixels() - currentTimeDilation->GetXPositionInPixels() );
				SetTimeDilationState( *currentTimeDilation, c_width, c_isSpedUp, c_isSlowedDown );
			}
		}
	}
}

void CVideoEditorPlayback::SetEditMarkerState( CComplexObject& editMarkerObject, eEDIT_MARKER_FOCUS const focus, bool const isAnchor )
{
	if ( editMarkerObject.IsActive() )
	{
		//! TODO - Incomplete scale form so just using coloration for now
		static eHUD_COLOURS s_editMarkerColours[ EDIT_MARKER_FOCUS_MAX ] = 
		{
			HUD_COLOUR_GREY, HUD_COLOUR_WHITE, HUD_COLOUR_WHITE
		};

		static eHUD_COLOURS s_editAnchorColours[ EDIT_MARKER_FOCUS_MAX ] = 
		{
			HUD_COLOUR_YELLOWDARK, HUD_COLOUR_YELLOWLIGHT, HUD_COLOUR_YELLOWLIGHT 
		};

		eHUD_COLOURS const c_markerColour = isAnchor ? s_editAnchorColours[ focus ] : s_editMarkerColours[ focus ];
		editMarkerObject.SetColour( CHudColour::GetRGBA( c_markerColour ) );

		switch( focus )
		{
		case EDIT_MARKER_EDITING:
			{
				s32 const c_markerDepth = editMarkerObject.GetDepth();
				if( c_markerDepth != VE_DEPTH_EDITING_MARKER && ms_currentFocusIndexDepthBeforeFocus == -1 )
				{
					ms_currentFocusIndexDepthBeforeFocus = c_markerDepth;
					editMarkerObject.SetDepth( VE_DEPTH_EDITING_MARKER );
				}

				editMarkerObject.GotoFrame( 2, true );

				break;
			}
		case EDIT_MARKER_NO_FOCUS:
		case EDIT_MARKER_FOCUS:
		default:
			{
				if( editMarkerObject.GetDepth() == VE_DEPTH_EDITING_MARKER && ms_currentFocusIndexDepthBeforeFocus != -1 )
				{
					editMarkerObject.SetDepth( ms_currentFocusIndexDepthBeforeFocus );
					ms_currentFocusIndexDepthBeforeFocus = -1;
				}

				editMarkerObject.GotoFrame( 1 );

				break;
			}
		}
	}
}

void CVideoEditorPlayback::SetTimeDilationState( CComplexObject& timeDilationObject, float const width, 
																	bool const isSpedUp, bool const isSlowedDown )
{
	if ( timeDilationObject.IsActive() )
	{
		timeDilationObject.SetVisible( isSpedUp || isSlowedDown );

		if( isSpedUp || isSlowedDown )
		{
			if( CScaleformMgr::BeginMethodOnComplexObject( timeDilationObject.GetId(), SF_BASE_CLASS_VIDEO_EDITOR, 
				"INIT_SPEED_INDICATOR" ) )
			{
				CScaleformMgr::AddParamInt( isSpedUp ? 0 : 1 );
				CScaleformMgr::AddParamFloat( width );
				CScaleformMgr::EndMethod();
			}
		}
	}
}

void CVideoEditorPlayback::EnterCamMenu( s32 const startingFocus )
{
	ms_editMenuStackedIndex = ms_cameraMenuStackedIndex == -1 ? ms_menuFocusIndex : ms_editMenuStackedIndex;

	DisableCurrentMenu();
	JumpToCurrentEditMarker();

	sReplayMarkerInfo const* marker = GetCurrentEditMarker();
	if( uiVerifyf( marker, "CVideoEditorPlayback::EnterCamMenu - Entering camera menu with no marker to edit!" ) )
	{
		CVideoEditorPlayback::BeginAddingColumnData( 0, COL_TYPE_LIST, TheText.Get( "VEUI_CAM_TIT" ) );

		if( !marker->IsAnchor() )
		{
			if( CScaleformMgr::BeginMethod(CVideoEditorUi::GetMovieId(), SF_BASE_CLASS_VIDEO_EDITOR, "SET_CLIP_EDIT_HEADER") )
			{
				CScaleformMgr::AddParamString( TheText.Get( "VEUI_CAM_TIT" ), false );
				CScaleformMgr::AddParamString( "", false );
				CScaleformMgr::EndMethod();
			}

			if( CScaleformMgr::BeginMethod(CVideoEditorUi::GetMovieId(), SF_BASE_CLASS_VIDEO_EDITOR, "ADD_COLUMN_ITEM_WITH_OPTIONS") )
			{
				CScaleformMgr::AddParamString( TheText.Get( "VEUI_ED_MK_CAM" ), false );
				CScaleformMgr::AddParamString( TheText.Get( marker->GetCameraTypeLngKey() ), false);
				CScaleformMgr::EndMethod();

				ms_activeMenuOptions.PushAndGrow( MenuOption( MARKER_MENU_OPTION_CAMERA_TYPE, 
					IsOnLastMarker() ? IReplayMarkerStorage::EDIT_RESTRICTION_IS_ENDPOINT : IReplayMarkerStorage::EDIT_RESTRICTION_NONE ), RPLYCAM_MAX );
			}
			
			if( marker->IsTargetCamera() )
			{
				if( CScaleformMgr::BeginMethod(CVideoEditorUi::GetMovieId(), SF_BASE_CLASS_VIDEO_EDITOR, "ADD_COLUMN_ITEM_WITH_OPTIONS") )
				{
					CScaleformMgr::AddParamString( TheText.Get( "VEUI_EDIT_CAM_LOOKAT" ), false );

					s32 const c_lookAtTargetIndex = marker->GetLookAtTargetIndex();
					s32 const c_lookAtTargetCount = camInterface::GetReplayDirector().GetNumValidTargets();

					if( c_lookAtTargetIndex == -1 )
					{
						CScaleformMgr::AddParamString( TheText.Get( "VEUI_TARGET_NONE" ), false);
					}
					else if( c_lookAtTargetIndex == 0 )
					{
						CScaleformMgr::AddParamString( TheText.Get( "VEUI_TARGET_PLYR" ), false);
					}
					else
					{
						SimpleString_32 stringBuffer;
                        stringBuffer.sprintf( "%u", (u32)c_lookAtTargetIndex );
						CScaleformMgr::AddParamString( stringBuffer.getBuffer(), false);
					}

					CScaleformMgr::EndMethod();
					ms_activeMenuOptions.PushAndGrow( MenuOption( MARKER_MENU_OPTION_CAMERA_LOOKAT_TARGET, IReplayMarkerStorage::EDIT_RESTRICTION_NONE, c_lookAtTargetCount ) );
				}

				s32 const c_attachTargetIndex = marker->GetAttachTargetIndex();
				s32 const c_attachTargetCount = camInterface::GetReplayDirector().GetNumValidTargets();

				if( CScaleformMgr::BeginMethod(CVideoEditorUi::GetMovieId(), SF_BASE_CLASS_VIDEO_EDITOR, "ADD_COLUMN_ITEM_WITH_OPTIONS") )
				{
					CScaleformMgr::AddParamString( TheText.Get( "VEUI_EDIT_CAM_ATTACH" ), false );

					if( c_attachTargetIndex == -1 )
					{
						CScaleformMgr::AddParamString( TheText.Get( "VEUI_TARGET_NONE" ), false);
					}
					else if( c_attachTargetIndex == 0 )
					{
						CScaleformMgr::AddParamString( TheText.Get( "VEUI_TARGET_PLYR" ), false);
					}
					else
					{
						SimpleString_32 stringBuffer;
                        stringBuffer.sprintf( "%u", (u32)c_attachTargetIndex );
						CScaleformMgr::AddParamString( stringBuffer.getBuffer(), false);
					}

					CScaleformMgr::EndMethod();
					ms_activeMenuOptions.PushAndGrow( MenuOption( MARKER_MENU_OPTION_CAMERA_ATTACH_TARGET, IReplayMarkerStorage::EDIT_RESTRICTION_NONE, c_attachTargetCount ) );
				}

				if( c_attachTargetIndex >= 0 &&
					CScaleformMgr::BeginMethod(CVideoEditorUi::GetMovieId(), SF_BASE_CLASS_VIDEO_EDITOR, "ADD_COLUMN_ITEM_WITH_OPTIONS") )
				{
					CScaleformMgr::AddParamString( TheText.Get( "VEUI_EDIT_CAM_MOUNTTYPE" ), false );

					char const * const c_lngKey = marker->GetAttachTypeLngKey();
					CScaleformMgr::AddParamString( TheText.Get( c_lngKey ), false);
					CScaleformMgr::EndMethod();

					ms_activeMenuOptions.PushAndGrow( MenuOption( MARKER_MENU_OPTION_CAMERA_ATTACH_TYPE, 
						IReplayMarkerStorage::EDIT_RESTRICTION_NONE, MARKER_CAMERA_ATTACH_MAX ) );
				}

				if( !IsOnLastMarker() &&
					CScaleformMgr::BeginMethod(CVideoEditorUi::GetMovieId(), SF_BASE_CLASS_VIDEO_EDITOR, "ADD_COLUMN_ITEM_WITH_OPTIONS") )
				{
					CScaleformMgr::AddParamString( TheText.Get( "VEUI_EDIT_CAM_BLEND" ), false );
					CScaleformMgr::AddParamString( TheText.Get( marker->GetMarkerBlendLngKey() ), false);
					CScaleformMgr::EndMethod();

					ms_activeMenuOptions.PushAndGrow( MenuOption( MARKER_MENU_OPTION_CAMERA_BLEND, 
						IReplayMarkerStorage::EDIT_RESTRICTION_NONE, MARKER_CAMERA_BLEND_MAX ) );
				}

				sReplayMarkerInfo const* previousMarker = GetPreviousEditMarker();
				if( ( marker->HasBlend() || (previousMarker && previousMarker->HasBlend()) || IsOnLastMarker() ) &&
					CScaleformMgr::BeginMethod(CVideoEditorUi::GetMovieId(), SF_BASE_CLASS_VIDEO_EDITOR, "ADD_COLUMN_ITEM_WITH_OPTIONS") )
				{
					CScaleformMgr::AddParamString( TheText.Get( "VEUI_EDIT_CAM_BLENDES" ), false );
					CScaleformMgr::AddParamString( TheText.Get( marker->GetMarkerBlendEaseLngKey() ), false);
					CScaleformMgr::EndMethod();

					ms_activeMenuOptions.PushAndGrow( MenuOption( MARKER_MENU_OPTION_CAMERA_BLEND_EASING, 
						IReplayMarkerStorage::EDIT_RESTRICTION_NONE, 2 ) );
				}
			}

			if( CScaleformMgr::BeginMethod(CVideoEditorUi::GetMovieId(), SF_BASE_CLASS_VIDEO_EDITOR, "ADD_COLUMN_ITEM_WITH_OPTIONS") )
			{
				CScaleformMgr::AddParamString( TheText.Get( "VEUI_EDIT_CAM_SHAKE" ), false );
				CScaleformMgr::AddParamString( TheText.Get( marker->GetMarkerShakeLngKey() ), false);
				CScaleformMgr::EndMethod();

				ms_activeMenuOptions.PushAndGrow( MenuOption( MARKER_MENU_OPTION_CAMERA_SHAKE, 
					IReplayMarkerStorage::EDIT_RESTRICTION_NONE, MARKER_SHAKE_MAX ) );
			}

			if( marker->HasShakeApplied() )
			{
				if( CScaleformMgr::BeginMethod(CVideoEditorUi::GetMovieId(), SF_BASE_CLASS_VIDEO_EDITOR, "ADD_COLUMN_ITEM_WITH_OPTIONS") )
				{
					CScaleformMgr::AddParamString( TheText.Get( "VEUI_EDIT_CAM_SHAKI" ), false );

                    SimpleString_32 stringBuffer;
                    stringBuffer.sprintf( "%u", marker->GetShakeIntensity() );
					CScaleformMgr::AddParamString( stringBuffer.getBuffer(), false);

					CScaleformMgr::EndMethod();

					ms_activeMenuOptions.PushAndGrow( MenuOption( MARKER_MENU_OPTION_CAMERA_SHAKE_INTENSITY, 
						IReplayMarkerStorage::EDIT_RESTRICTION_NONE ) );
				}

				if( CScaleformMgr::BeginMethod(CVideoEditorUi::GetMovieId(), SF_BASE_CLASS_VIDEO_EDITOR, "ADD_COLUMN_ITEM_WITH_OPTIONS") )
				{
					CScaleformMgr::AddParamString( TheText.Get( "VEUI_EDIT_CAM_SHAKS" ), false );
					CScaleformMgr::AddParamString( TheText.Get( marker->GetMarkerShakeSpeedLngKey() ), false);

					CScaleformMgr::EndMethod();

					ms_activeMenuOptions.PushAndGrow( MenuOption( MARKER_MENU_OPTION_CAMERA_SHAKE_SPEED, 
						IReplayMarkerStorage::EDIT_RESTRICTION_NONE, MARKER_SPEED_MAX ) );
				}
			}

			if( marker->IsEditableCamera() &&
				CScaleformMgr::BeginMethod(CVideoEditorUi::GetMovieId(), SF_BASE_CLASS_VIDEO_EDITOR, "ADD_COLUMN_ITEM") )
			{
				CScaleformMgr::AddParamString( TheText.Get( "VEUI_EDIT_CAM" ), false );
				CScaleformMgr::EndMethod();

				ms_activeMenuOptions.PushAndGrow( MenuOption( MARKER_MENU_OPTION_EDIT_CAMERA, IReplayMarkerStorage::EDIT_RESTRICTION_NONE ) );
			}
		}

		if (CScaleformMgr::BeginMethod(CVideoEditorUi::GetMovieId(), SF_BASE_CLASS_VIDEO_EDITOR, "ADD_COLUMN_HELP_TEXT"))
		{
			CScaleformMgr::AddParamString( "", false );
			CScaleformMgr::EndMethod();

			ms_activeMenuOptions.PushAndGrow( MenuOption( MARKER_MENU_OPTION_HELP_TEXT, IReplayMarkerStorage::EDIT_RESTRICTION_NONE ) );
		}

		CVideoEditorPlayback::EndAddingColumnData( true, true );
		ms_focusContext = FOCUS_CAMERA_MENU;

		s32 const c_menuOptionCount = ms_activeMenuOptions.GetCount() - 1;
		s32 const c_lastValidOptionIndex = c_menuOptionCount >= 1 ? c_menuOptionCount - 1 : 0;

		ms_menuFocusIndex = ms_cameraMenuStackedIndex > -1 ? ms_cameraMenuStackedIndex : startingFocus;
		ms_menuFocusIndex = Clamp( ms_menuFocusIndex, 0, c_lastValidOptionIndex );

		SetCurrentItemIntoCorrectState();
		UpdateEditMenuState();
		UpdateMenuHelpText( ms_menuFocusIndex );
		UpdateItemFocusDependentState();
	}

	ms_cameraMenuStackedIndex = ms_menuFocusIndex;
	ms_refreshInstructionalButtons = true;
}

void CVideoEditorPlayback::ExitCamMenu()
{
	ms_cameraMenuStackedIndex = -1;
	DisableMenu();
	CVideoEditorUi::GarbageCollect();

	g_FrontendAudioEntity.PlaySound( "BACK", "HUD_FRONTEND_DEFAULT_SOUNDSET" );

	ms_focusContext = FOCUS_MARKER_MENU;
	ms_activeMenuOptions.Reset();

	CReplayMarkerContext::SetForceCameraShake( false );
	EnableEditMarkerMenu();
}

void CVideoEditorPlayback::EnterPostFxMenu( s32 const startingFocus )
{
	DisableCurrentMenu();

	sReplayMarkerInfo const* marker = GetCurrentEditMarker();
	if( uiVerifyf( marker, "CVideoEditorPlayback::EnterPostFxMenu - Entering postfx menu with no marker to edit!" ) )
	{
		CVideoEditorPlayback::BeginAddingColumnData( 0, COL_TYPE_LIST, TheText.Get( "VEUI_PFX_TIT" ) );

		if( !marker->IsAnchor() && !IsOnLastMarker() )
		{
			if( CScaleformMgr::BeginMethod(CVideoEditorUi::GetMovieId(), SF_BASE_CLASS_VIDEO_EDITOR, "SET_CLIP_EDIT_HEADER") )
			{
				CScaleformMgr::AddParamString( TheText.Get( "VEUI_PFX_TIT" ), false );
				CScaleformMgr::AddParamString( "", false );
				CScaleformMgr::EndMethod();
			}

			if( CScaleformMgr::BeginMethod(CVideoEditorUi::GetMovieId(), SF_BASE_CLASS_VIDEO_EDITOR, "ADD_COLUMN_ITEM_WITH_OPTIONS") )
			{
				u8 const c_appliedFilter = marker->GetActivePostFx();

				CScaleformMgr::AddParamString( TheText.Get( "VEUI_ED_MK_FILT" ), false );
				CScaleformMgr::AddParamString( TheText.Get( CReplayCoordinator::GetPostFxRegistry().GetEffectUiNameHash( c_appliedFilter ), "" ), false);

				CScaleformMgr::EndMethod();

				ms_activeMenuOptions.PushAndGrow( MenuOption( MARKER_MENU_OPTION_FILTER, IReplayMarkerStorage::EDIT_RESTRICTION_NONE,
					CReplayCoordinator::GetPostFxRegistry().GetEffectCount() ) );


				if( c_appliedFilter != 0 &&
					CScaleformMgr::BeginMethod(CVideoEditorUi::GetMovieId(), SF_BASE_CLASS_VIDEO_EDITOR, "ADD_COLUMN_ITEM_WITH_OPTIONS") )
				{
					CScaleformMgr::AddParamString( TheText.Get( "VEUI_ED_MK_FXIN" ), false );

                    SimpleString_32 stringBuffer;
                    stringBuffer.sprintf( "%3d%%",  marker->GetPostFxIntensityWholePercent() );
					CScaleformMgr::AddParamString( stringBuffer.getBuffer(), false);
					CScaleformMgr::EndMethod();

					ms_activeMenuOptions.PushAndGrow( MenuOption( MARKER_MENU_OPTION_FILTER_INTENSITY, IReplayMarkerStorage::EDIT_RESTRICTION_NONE ) );
				}

				if( CScaleformMgr::BeginMethod(CVideoEditorUi::GetMovieId(), SF_BASE_CLASS_VIDEO_EDITOR, "ADD_COLUMN_ITEM_WITH_OPTIONS") )
				{
					CScaleformMgr::AddParamString( TheText.Get( "VEUI_ED_MK_FXSA" ), false );

                    SimpleString_32 stringBuffer;
                    stringBuffer.sprintf( "%3d",  marker->GetSaturationIntensityWholePercent() );
					CScaleformMgr::AddParamString( stringBuffer.getBuffer(), false);
					CScaleformMgr::EndMethod();

					ms_activeMenuOptions.PushAndGrow( MenuOption( MARKER_MENU_OPTION_SATURATION, IReplayMarkerStorage::EDIT_RESTRICTION_NONE ) );
				}

				if( CScaleformMgr::BeginMethod(CVideoEditorUi::GetMovieId(), SF_BASE_CLASS_VIDEO_EDITOR, "ADD_COLUMN_ITEM_WITH_OPTIONS") )
				{
					CScaleformMgr::AddParamString( TheText.Get( "VEUI_ED_MK_FXCO" ), false );

                    SimpleString_32 stringBuffer;
                    stringBuffer.sprintf( "%3d",  marker->GetContrastIntensityWholePercent() );
					CScaleformMgr::AddParamString( stringBuffer.getBuffer(), false);
					CScaleformMgr::EndMethod();

					ms_activeMenuOptions.PushAndGrow( MenuOption( MARKER_MENU_OPTION_CONTRAST, IReplayMarkerStorage::EDIT_RESTRICTION_NONE ) );
				}

				if( CScaleformMgr::BeginMethod(CVideoEditorUi::GetMovieId(), SF_BASE_CLASS_VIDEO_EDITOR, "ADD_COLUMN_ITEM_WITH_OPTIONS") )
				{
					CScaleformMgr::AddParamString( TheText.Get( "VEUI_ED_MK_FXBR" ), false );

                    SimpleString_32 stringBuffer;
                    stringBuffer.sprintf( "%3d",  marker->GetBrightnessWholePercent() );
					CScaleformMgr::AddParamString( stringBuffer.getBuffer(), false);
					CScaleformMgr::EndMethod();

					ms_activeMenuOptions.PushAndGrow( MenuOption( MARKER_MENU_OPTION_BRIGHTNESS, IReplayMarkerStorage::EDIT_RESTRICTION_NONE ) );
				}

				if( CScaleformMgr::BeginMethod(CVideoEditorUi::GetMovieId(), SF_BASE_CLASS_VIDEO_EDITOR, "ADD_COLUMN_ITEM_WITH_OPTIONS") )
				{
					CScaleformMgr::AddParamString( TheText.Get( "VEUI_ED_MK_FXVG" ), false );

                    SimpleString_32 stringBuffer;
                    stringBuffer.sprintf( "%3d",  marker->GetVignetteIntensityWholePercent() );
					CScaleformMgr::AddParamString( stringBuffer.getBuffer(), false);
					CScaleformMgr::EndMethod();

					ms_activeMenuOptions.PushAndGrow( MenuOption( MARKER_MENU_OPTION_VIGNETTE, IReplayMarkerStorage::EDIT_RESTRICTION_NONE ) );
				}
			}
		}

		if (CScaleformMgr::BeginMethod(CVideoEditorUi::GetMovieId(), SF_BASE_CLASS_VIDEO_EDITOR, "ADD_COLUMN_HELP_TEXT"))
		{
			CScaleformMgr::AddParamString( "", false );
			CScaleformMgr::EndMethod();

			ms_activeMenuOptions.PushAndGrow( MenuOption( MARKER_MENU_OPTION_HELP_TEXT, IReplayMarkerStorage::EDIT_RESTRICTION_NONE ) );
		}

		CVideoEditorPlayback::EndAddingColumnData( true, true );
		ms_focusContext = FOCUS_POSTFX_MENU;

		s32 const c_menuOptionCount = ms_activeMenuOptions.GetCount() - 1;
		s32 const c_lastValidOptionIndex = c_menuOptionCount >= 1 ? c_menuOptionCount - 1 : 0;

		ms_menuFocusIndex = startingFocus;
		ms_menuFocusIndex = Clamp( ms_menuFocusIndex, 0, c_lastValidOptionIndex );

		SetCurrentItemIntoCorrectState();
		UpdateEditMenuState();
		UpdateMenuHelpText( ms_menuFocusIndex );

		PostFX::SetUpdateReplayEffectsWhilePaused( true );
	}

	ms_refreshInstructionalButtons = true;
}

void CVideoEditorPlayback::ExitPostFxMenu()
{
	DisableMenu();
	CVideoEditorUi::GarbageCollect();

	g_FrontendAudioEntity.PlaySound( "BACK", "HUD_FRONTEND_DEFAULT_SOUNDSET" );

	ms_focusContext = FOCUS_MARKER_MENU;
	ms_activeMenuOptions.Reset();

    EnableEditMarkerMenu();
    ms_refreshInstructionalButtons = true;
}

void CVideoEditorPlayback::EnterDoFMenu( s32 const startingFocus )
{
	DisableCurrentMenu();

	sReplayMarkerInfo const* c_marker = GetCurrentEditMarker();
	if( uiVerifyf( c_marker, "CVideoEditorPlayback::EnterDoFMenu - Entering dof menu with no marker to edit!" ) )
	{
		CVideoEditorPlayback::BeginAddingColumnData( 0, COL_TYPE_LIST, TheText.Get( "VEUI_DOF_TIT" ) );

		if( !c_marker->IsAnchor() && !IsOnLastMarker() )
		{
			if( CScaleformMgr::BeginMethod(CVideoEditorUi::GetMovieId(), SF_BASE_CLASS_VIDEO_EDITOR, "SET_CLIP_EDIT_HEADER") )
			{
				CScaleformMgr::AddParamString( TheText.Get( "VEUI_DOF_TIT" ), false );
				CScaleformMgr::AddParamString( "", false );
				CScaleformMgr::EndMethod();
			}

			if( CScaleformMgr::BeginMethod(CVideoEditorUi::GetMovieId(), SF_BASE_CLASS_VIDEO_EDITOR, "ADD_COLUMN_ITEM_WITH_OPTIONS") )
			{
				CScaleformMgr::AddParamString( TheText.Get( "VEUI_ED_MK_DOFL" ), false );
				CScaleformMgr::AddParamString( TheText.Get( c_marker->GetDofModeLngKey() ), false);

				CScaleformMgr::EndMethod();

				ms_activeMenuOptions.PushAndGrow( MenuOption( MARKER_MENU_OPTION_DOF_MODE, IReplayMarkerStorage::EDIT_RESTRICTION_NONE, MARKER_DOF_MODE_MAX ) );

				if( !c_marker->IsDofNone() )
				{
					if( CScaleformMgr::BeginMethod(CVideoEditorUi::GetMovieId(), SF_BASE_CLASS_VIDEO_EDITOR, "ADD_COLUMN_ITEM_WITH_OPTIONS") )
					{
						CScaleformMgr::AddParamString( TheText.Get( "VEUI_ED_MK_DOFI" ), false );

                        SimpleString_32 stringBuffer;
                        stringBuffer.sprintf( "%3d%%",  c_marker->GetDofIntensityWholePercent() );
						CScaleformMgr::AddParamString( stringBuffer.getBuffer(), false);
						CScaleformMgr::EndMethod();

						ms_activeMenuOptions.PushAndGrow( MenuOption( MARKER_MENU_OPTION_DOF_INTENSITY, IReplayMarkerStorage::EDIT_RESTRICTION_NONE ) );
					}

					if( !c_marker->IsDofDefault() )
					{
						if( CScaleformMgr::BeginMethod(CVideoEditorUi::GetMovieId(), SF_BASE_CLASS_VIDEO_EDITOR, "ADD_COLUMN_ITEM_WITH_OPTIONS") )
						{
							CScaleformMgr::AddParamString( TheText.Get( "VEUI_ED_MK_DOFF" ), false );
							CScaleformMgr::AddParamString( TheText.Get( c_marker->GetFocusModeLngKey() ), false);
							CScaleformMgr::EndMethod();

							ms_activeMenuOptions.PushAndGrow( MenuOption( MARKER_MENU_OPTION_FOCUS_MODE, 
								IReplayMarkerStorage::EDIT_RESTRICTION_NONE, MARKER_FOCUS_MAX ) );
						}

						if( c_marker->CanChangeDofFocusDistance() &&
							CScaleformMgr::BeginMethod(CVideoEditorUi::GetMovieId(), SF_BASE_CLASS_VIDEO_EDITOR, "ADD_COLUMN_ITEM_WITH_OPTIONS") )
						{
							CScaleformMgr::AddParamString( TheText.Get( "VEUI_ED_MK_DOFFD" ), false );

                            SimpleString_32 stringBuffer;
                            stringBuffer.sprintf( "%.1fm",  c_marker->GetFocalDistance() );
							CScaleformMgr::AddParamString( stringBuffer.getBuffer(), false);
							CScaleformMgr::EndMethod();

							ms_activeMenuOptions.PushAndGrow( MenuOption( MARKER_MENU_OPTION_FOCUS_DISTANCE, IReplayMarkerStorage::EDIT_RESTRICTION_NONE ) );
						}

						if( c_marker->CanChangeDofFocusTarget() &&
							CScaleformMgr::BeginMethod(CVideoEditorUi::GetMovieId(), SF_BASE_CLASS_VIDEO_EDITOR, "ADD_COLUMN_ITEM_WITH_OPTIONS") )
						{
							CScaleformMgr::AddParamString( TheText.Get( "VEUI_ED_MK_DOFFT" ), false );

							s32 const c_focusTargetIndex = c_marker->GetFocusTargetIndex();
							s32 const c_focusTargetCount = camInterface::GetReplayDirector().GetNumValidTargets();

							if( c_focusTargetIndex == -1 )
							{
								CScaleformMgr::AddParamString( TheText.Get( "VEUI_TARGET_NONE" ), false);
							}
							else if( c_focusTargetIndex == 0 )
							{
								CScaleformMgr::AddParamString( TheText.Get( "VEUI_TARGET_PLYR" ), false);
							}
							else
							{
                                SimpleString_32 stringBuffer;
                                stringBuffer.sprintf( "%u", (u32)c_focusTargetIndex );
								CScaleformMgr::AddParamString( stringBuffer.getBuffer(), false);
							}

							CScaleformMgr::EndMethod();

							ms_activeMenuOptions.PushAndGrow( MenuOption( MARKER_MENU_OPTION_FOCUS_TARGET, 
								IReplayMarkerStorage::EDIT_RESTRICTION_NONE, c_focusTargetCount ) );
						}
					}
				}
			}
		}

		if (CScaleformMgr::BeginMethod(CVideoEditorUi::GetMovieId(), SF_BASE_CLASS_VIDEO_EDITOR, "ADD_COLUMN_HELP_TEXT"))
		{
			CScaleformMgr::AddParamString( "", false );
			CScaleformMgr::EndMethod();

			ms_activeMenuOptions.PushAndGrow( MenuOption( MARKER_MENU_OPTION_HELP_TEXT, IReplayMarkerStorage::EDIT_RESTRICTION_NONE ) );
		}

		CVideoEditorPlayback::EndAddingColumnData( true, true );
		ms_focusContext = FOCUS_DOF_MENU;

		s32 const c_menuOptionCount = ms_activeMenuOptions.GetCount() - 1;
		s32 const c_lastValidOptionIndex = c_menuOptionCount >= 1 ? c_menuOptionCount - 1 : 0;

		ms_menuFocusIndex = startingFocus;
		ms_menuFocusIndex = Clamp( ms_menuFocusIndex, 0, c_lastValidOptionIndex );

		SetCurrentItemIntoCorrectState();
		UpdateEditMenuState();
		UpdateMenuHelpText( ms_menuFocusIndex );
	}

	ms_refreshInstructionalButtons = true;
}

void CVideoEditorPlayback::ExitDoFMenu()
{
	DisableMenu();
	CVideoEditorUi::GarbageCollect();

	g_FrontendAudioEntity.PlaySound( "BACK", "HUD_FRONTEND_DEFAULT_SOUNDSET" );

	ms_focusContext = FOCUS_POSTFX_MENU;
	ms_activeMenuOptions.Reset();

	EnableEditMarkerMenu();
}

void CVideoEditorPlayback::EnterAudioMenu( s32 const startingFocus )
{
	ms_editMenuStackedIndex = ms_menuFocusIndex;

	DisableCurrentMenu();

	IReplayMarkerStorage const* markerStorage = GetMarkerStorage();
	sReplayMarkerInfo const* marker = GetCurrentEditMarker();
	if( uiVerifyf( marker, "CVideoEditorPlayback::EnterAudioMenu - Entering audio menu with no marker to edit!" ) )
	{
		CVideoEditorPlayback::BeginAddingColumnData( 0, COL_TYPE_LIST, TheText.Get( "VEUI_AUD_TIT" ) );

		if( !marker->IsAnchor() && !IsOnLastMarker() )
		{
			if( CScaleformMgr::BeginMethod(CVideoEditorUi::GetMovieId(), SF_BASE_CLASS_VIDEO_EDITOR, "SET_CLIP_EDIT_HEADER") )
			{
				CScaleformMgr::AddParamString( TheText.Get( "VEUI_AUD_TIT" ), false );
				CScaleformMgr::AddParamString( "", false );
				CScaleformMgr::EndMethod();
			}

            if( CScaleformMgr::BeginMethod(CVideoEditorUi::GetMovieId(), SF_BASE_CLASS_VIDEO_EDITOR, "ADD_COLUMN_ITEM_WITH_OPTIONS") )
            {
                CScaleformMgr::AddParamString( TheText.Get( "VEUI_ED_MK_SFX_CAT" ), false );
                CScaleformMgr::AddParamString( TheText.Get( marker->GetSfxCategoryLngKey() ), false );

                CScaleformMgr::EndMethod();

                ms_activeMenuOptions.PushAndGrow( MenuOption( MARKER_MENU_OPTION_SFX_CAT, IReplayMarkerStorage::EDIT_RESTRICTION_NONE ) );
            }

            if( CScaleformMgr::BeginMethod(CVideoEditorUi::GetMovieId(), SF_BASE_CLASS_VIDEO_EDITOR, "ADD_COLUMN_ITEM_WITH_OPTIONS") )
            {
                CScaleformMgr::AddParamString( TheText.Get( "VEUI_ED_MK_SFX_HASH" ), false );

                LocalizationKey stringBuffer;
                marker->GetSfxLngKey( stringBuffer.getBufferRef() );

                CScaleformMgr::AddParamString( TheText.Get( stringBuffer.getBuffer() ) );

                CScaleformMgr::EndMethod();

                ms_activeMenuOptions.PushAndGrow( MenuOption( MARKER_MENU_OPTION_SFX_HASH, IReplayMarkerStorage::EDIT_RESTRICTION_NONE ) );
            }

            if( CScaleformMgr::BeginMethod(CVideoEditorUi::GetMovieId(), SF_BASE_CLASS_VIDEO_EDITOR, "ADD_COLUMN_ITEM_WITH_OPTIONS") )
            {
                CScaleformMgr::AddParamString( TheText.Get( "VEUI_ED_MK_SFXOS" ), false );

                SimpleString_32 stringBuffer;
                stringBuffer.sprintf( "%u", marker->GetSfxOSVolume() );
                CScaleformMgr::AddParamString( stringBuffer.getBuffer(), false);

                CScaleformMgr::EndMethod();

                ms_activeMenuOptions.PushAndGrow( MenuOption( MARKER_MENU_OPTION_SFX_OS_VOL, IReplayMarkerStorage::EDIT_RESTRICTION_NONE ) );
            }

			if( CScaleformMgr::BeginMethod(CVideoEditorUi::GetMovieId(), SF_BASE_CLASS_VIDEO_EDITOR, "ADD_COLUMN_ITEM_WITH_OPTIONS") )
			{
				CScaleformMgr::AddParamString( TheText.Get( "VEUI_ED_MK_SFX" ), false );

                SimpleString_32 stringBuffer;
                stringBuffer.sprintf( "%u", marker->GetSfxVolume() );
				CScaleformMgr::AddParamString( stringBuffer.getBuffer(), false);

				CScaleformMgr::EndMethod();

				ms_activeMenuOptions.PushAndGrow( MenuOption( MARKER_MENU_OPTION_SFX_VOL, markerStorage->IsMarkerControlEditable( MARKER_CONTROL_SFXVOLUME ) ) );
			}

			if( CScaleformMgr::BeginMethod(CVideoEditorUi::GetMovieId(), SF_BASE_CLASS_VIDEO_EDITOR, "ADD_COLUMN_ITEM_WITH_OPTIONS") )
			{
				CScaleformMgr::AddParamString( TheText.Get( "VEUI_ED_MK_DLG" ), false );

                SimpleString_32 stringBuffer;
                stringBuffer.sprintf( "%u", marker->GetDialogVolume() );
				CScaleformMgr::AddParamString( stringBuffer.getBuffer(), false);

				CScaleformMgr::EndMethod();

				ms_activeMenuOptions.PushAndGrow( MenuOption( MARKER_MENU_OPTION_DIALOG_VOL, markerStorage->IsMarkerControlEditable( MARKER_CONTROL_DIALOGVOLUME ) ) );
			}

			if( CScaleformMgr::BeginMethod(CVideoEditorUi::GetMovieId(), SF_BASE_CLASS_VIDEO_EDITOR, "ADD_COLUMN_ITEM_WITH_OPTIONS") )
			{
				CScaleformMgr::AddParamString( TheText.Get( "VEUI_ED_MK_MUS" ), false );

                SimpleString_32 stringBuffer;
                stringBuffer.sprintf( "%u", marker->GetMusicVolume() );
				CScaleformMgr::AddParamString( stringBuffer.getBuffer(), false);

				CScaleformMgr::EndMethod();

				ms_activeMenuOptions.PushAndGrow( MenuOption( MARKER_MENU_OPTION_MUSIC_VOL, markerStorage->IsMarkerControlEditable( MARKER_CONTROL_MUSICVOLUME ) ) );
			}

            if( CScaleformMgr::BeginMethod(CVideoEditorUi::GetMovieId(), SF_BASE_CLASS_VIDEO_EDITOR, "ADD_COLUMN_ITEM_WITH_OPTIONS") )
            {
                CScaleformMgr::AddParamString( TheText.Get( "VEUI_ED_MK_AMB" ), false );

                SimpleString_32 stringBuffer;
                stringBuffer.sprintf( "%u", marker->GetAmbientVolume() );
                CScaleformMgr::AddParamString( stringBuffer.getBuffer(), false);

                CScaleformMgr::EndMethod();

                ms_activeMenuOptions.PushAndGrow( MenuOption( MARKER_MENU_OPTION_AMB_VOL, IReplayMarkerStorage::EDIT_RESTRICTION_NONE ) );
            }

			if( CScaleformMgr::BeginMethod(CVideoEditorUi::GetMovieId(), SF_BASE_CLASS_VIDEO_EDITOR, "ADD_COLUMN_ITEM_WITH_OPTIONS") )
			{
				CScaleformMgr::AddParamString( TheText.Get( "VEUI_ED_MK_SCI" ), false );
				CScaleformMgr::AddParamString( TheText.Get( marker->GetMarkerAudioIntensityLngKey() ), false);

				CScaleformMgr::EndMethod();

				ms_activeMenuOptions.PushAndGrow( MenuOption( MARKER_MENU_OPTION_SCORE_INTENSITY, markerStorage->IsMarkerControlEditable( MARKER_CONTROL_SCORE_INTENSITY ) ) );
			}

			if( CScaleformMgr::BeginMethod(CVideoEditorUi::GetMovieId(), SF_BASE_CLASS_VIDEO_EDITOR, "ADD_COLUMN_ITEM_WITH_OPTIONS") )
			{
				CScaleformMgr::AddParamString( TheText.Get( "VEUI_ED_MK_MIC" ), false );
				CScaleformMgr::AddParamString( TheText.Get( marker->GetMarkerMicrophoneTypeLngKey() ), false);

				CScaleformMgr::EndMethod();

				ms_activeMenuOptions.PushAndGrow( MenuOption( MARKER_MENU_OPTION_MIC_TYPE, 
					IReplayMarkerStorage::EDIT_RESTRICTION_NONE, MARKER_MICROPHONE_MAX ) );
			}
		}

		if (CScaleformMgr::BeginMethod(CVideoEditorUi::GetMovieId(), SF_BASE_CLASS_VIDEO_EDITOR, "ADD_COLUMN_HELP_TEXT"))
		{
			CScaleformMgr::AddParamString( "", false );
			CScaleformMgr::EndMethod();

			ms_activeMenuOptions.PushAndGrow( MenuOption( MARKER_MENU_OPTION_HELP_TEXT, IReplayMarkerStorage::EDIT_RESTRICTION_NONE ) );
		}

		CVideoEditorPlayback::EndAddingColumnData( true, true );
		ms_focusContext = FOCUS_AUDIO_MENU;

		s32 const c_menuOptionCount = ms_activeMenuOptions.GetCount() - 1;
		s32 const c_lastValidOptionIndex = c_menuOptionCount >= 1 ? c_menuOptionCount - 1 : 0;

		ms_menuFocusIndex = startingFocus;
		ms_menuFocusIndex = Clamp( ms_menuFocusIndex, 0, c_lastValidOptionIndex );

		SetCurrentItemIntoCorrectState();
		UpdateEditMenuState();
		UpdateMenuHelpText( ms_menuFocusIndex );
	}

	ms_refreshInstructionalButtons = true;
}

void CVideoEditorPlayback::ExitAudioMenu()
{
	DisableMenu();
	CVideoEditorUi::GarbageCollect();

	g_FrontendAudioEntity.PlaySound( "BACK", "HUD_FRONTEND_DEFAULT_SOUNDSET" );

	ms_focusContext = FOCUS_MARKER_MENU;
	ms_activeMenuOptions.Reset();

	EnableEditMarkerMenu();
    ms_refreshInstructionalButtons = true;
}

void CVideoEditorPlayback::EnterCamEditMode()
{
	if( IsOnCameraMenu() )
	{
		DisableCurrentMenu();
		ms_refreshInstructionalButtons = true;
		ms_focusContext = FOCUS_EDITING_CAMERA;

		// Assume we are edited if entering camera edit mode
		SetHasBeenEdited();

		// Camera edits only work if we are on the correct frame
		JumpToCurrentEditMarker();
		CReplayMarkerContext::SetEditingMarkerIndex( GetCurrentEditMarkerIndex() );

		SetCameraOverlayVisible( true );


		camReplayDirector& replayDirector = camInterface::GetReplayDirector();
		ms_cameraEditFovLastFrame = replayDirector.GetFovOfRenderedFrame();
		ms_bMouseInEditMenu = false;

		UpdateCameraOverlay();
	}
}

void CVideoEditorPlayback::ExitCamEditMode()
{
	if( IsEditingCamera() )
	{
		CReplayMarkerContext::SetEditingMarkerIndex( -1 );
		CVideoEditorUi::GarbageCollect();

		EnterCamMenu();
		ms_refreshInstructionalButtons = true;
		ms_focusContext = FOCUS_CAMERA_MENU;
	}

	SetCameraOverlayVisible( false );
}

eMarkerCopyContext CVideoEditorPlayback::GetCopyContext()
{
    eMarkerCopyContext const c_expectedCopyContext = IsOnPostFxMenu() ? COPY_CONTEXT_EFFECTS_ONLY : 
        IsOnAudioMenu() ? COPY_CONTEXT_AUDIO_ONLY : COPY_CONTEXT_NONE;

    return c_expectedCopyContext;
}

bool CVideoEditorPlayback::CanPasteMarkerProperties()
{
    eMarkerCopyContext const c_expectedCopyContext = GetCopyContext();
    bool const c_canPaste = ms_project && ms_project->HasMarkerOnClipboard( c_expectedCopyContext );
    return c_canPaste;
}

void CVideoEditorPlayback::CopyCurrentMarker()
{
    eMarkerCopyContext const c_copyContext = GetCopyContext();
    s32 const c_currentEditMarker = GetCurrentEditMarkerIndex();
    if( ms_project && c_currentEditMarker >= 0 )
    {
        ms_project->CopyMarker( ms_clipIndexToEdit, (u32)c_currentEditMarker, c_copyContext );
    }
}

void CVideoEditorPlayback::PasteToCurrentMarker()
{
    eMarkerCopyContext const c_pasteContext = GetCopyContext();
    s32 const c_currentEditMarker = GetCurrentEditMarkerIndex();
    if( ms_project && c_currentEditMarker >= 0 )
    {
        ms_project->PasteMarker( ms_clipIndexToEdit, (u32)c_currentEditMarker, c_pasteContext );
        SetHasBeenEdited();
    }
}

CVideoEditorPlayback::MenuOption const* CVideoEditorPlayback::TryGetMenuOption( s32 const index )
{
	s32 const c_itemCount = (s32)ms_activeMenuOptions.GetCount();
	CVideoEditorPlayback::MenuOption const * const c_result = index >= 0 && index < c_itemCount ? 
		&ms_activeMenuOptions[ index ] : NULL;

	return c_result;
}

CVideoEditorPlayback::MenuOption const* CVideoEditorPlayback::TryGetMenuOptionAndIndex( eMARKER_MENU_OPTIONS const option, s32& out_index )
{
	out_index = -1;
	CVideoEditorPlayback::MenuOption const* result = NULL;

	s32 const c_itemCount = (s32)ms_activeMenuOptions.GetCount();
	for( s32 index = 0; result == NULL && index < c_itemCount; ++index )
	{
		CVideoEditorPlayback::MenuOption const& currentOption = ms_activeMenuOptions[ index ];
		out_index = ( currentOption.m_option == option ) ? index : out_index;
		result = ( currentOption.m_option == option ) ? &currentOption : NULL;
	}
	
	return result;
}

bool CVideoEditorPlayback::IsFirstEntryToEditMode()
{
	bool const c_isFirstEntry = IsEditing() && !CVideoEditorInterface::HasSeenTutorial( CVideoEditorInterface::EDITOR_TUTORIAL_MARKER_EXTENTS );
	return c_isFirstEntry;
}

void CVideoEditorPlayback::SetFirstEntryEditModeTutorialSeen()
{
	CVideoEditorInterface::SetTutorialSeen( CVideoEditorInterface::EDITOR_TUTORIAL_MARKER_EXTENTS );
}

bool CVideoEditorPlayback::IsWaitingForTutorialToFinish()
{
	bool const c_isWaiting = ms_tutorialCountdownMs > 0;
	return c_isWaiting;
}

bool CVideoEditorPlayback::RebuildMarkersOnPlayBar()
{
	bool rebuilt = false;

	CleanupMarkersOnPlayBar();

	if( CVideoEditorPlayback::ms_ComplexObject[PLAYBACK_OBJECT_PLAYBACK_BAR].IsActiveAndAttached() )
	{
		CVideoProjectPlaybackController& playbackController = CVideoEditorInterface::GetPlaybackController();

		float const endTime = IsEditing() ? 
			playbackController.GetPlaybackCurrentClipRawEndTimeMsAsTimeMs() : playbackController.GetPlaybackEndTimeMs();

		Vector2 const sourcePosition( GetPlaybackBarBaseStartPositionX(), GetPlaybackBarBaseStartPositionY() );

		// For Edit mode, we use the clip markers...
		if( IsEditing() )
		{
			IReplayMarkerStorage* markerStorage = ms_project ? ms_project->GetClipMarkers( ms_clipIndexToEdit ) : NULL;
			if( markerStorage )
			{
				u32 const c_markerCount = markerStorage->GetMarkerCount();
				for( u32 i = 0; i < c_markerCount; ++i )
				{
					float const markerTime = markerStorage->GetMarkerDisplayTimeMs( i );
					AddMarkerToPlayBar( CVideoEditorPlayback::GetMarkerTypeName( i ), markerTime, endTime, sourcePosition, i, i );
				}
			}
		}
		else // ... otherwise we show the boundaries between clips
		{
			u32 const c_clipCount = ms_project ? ms_project->GetClipCount() : 0;
			for( u32 i = 1; i < c_clipCount; ++i )
			{
				float const clipTime = ms_project->GetTrimmedTimeToClipMs( i );
				AddMarkerToPlayBar( CVideoEditorPlayback::GetMarkerTypeName( i ), clipTime, endTime, sourcePosition, i, i - 1 );
			}
		}

		rebuilt = true;
	}

	return rebuilt;
}

void CVideoEditorPlayback::RefreshMarkerPositionsOnPlayBar()
{
	// Only valid in edit mode
	if( IsEditing() && 
		CVideoEditorPlayback::ms_ComplexObject[PLAYBACK_OBJECT_PLAYBACK_BAR].IsActive() )
	{
		CVideoProjectPlaybackController& playbackController = CVideoEditorInterface::GetPlaybackController();
		Vector2 const sourcePosition( GetPlaybackBarBaseStartPositionX(), GetPlaybackBarBaseStartPositionY() );

		int const c_markerCount = ms_ComplexObjectMarkers.GetCount();
		IReplayMarkerStorage* markerStorage = ms_project ? ms_project->GetClipMarkers( ms_clipIndexToEdit ) : NULL;
		float const endTimeMs = playbackController.GetPlaybackCurrentClipRawEndTimeMsAsTimeMs();

		for( int index = 0; index < c_markerCount && markerStorage; ++index )
		{
			MarkerDisplayObject* markerObject = GetEditMarkerDisplayObject( index );
			if( markerObject )
			{
				float const uMarkerTime = markerStorage->GetMarkerDisplayTimeMs( index );
				UpdateMarkerPositionOnPlayBar( markerObject->m_markerComplexObject, markerObject->m_timeDilationComplexObject, 
					index, uMarkerTime, endTimeMs, sourcePosition );
			}		
		}

		ms_refreshEditMarkerState = true;
	}
}

void CVideoEditorPlayback::CleanupMarkersOnPlayBar()
{
	while( ms_ComplexObjectMarkers.GetCount() )
	{
		RemoveMarkerOnPlaybar(0);
	}

	ms_ComplexObjectMarkers.Reset();
	ms_markerDepthCount = 0;
	ms_timeDilationDepthCount = 0;
}

void CVideoEditorPlayback::AddMarkerToPlayBar( char const * const type, float const clipTime, float const clipEndTime, 
											  Vector2 const& startPosition, u32 const markerIndex, u32 const complexObjectInsertionIndex )
{
	if( type && clipEndTime >= clipTime )
	{	
		if( uiVerifyf( ms_ComplexObjectMarkers.GetCount() < GetMaxMarkerCount() , "CVideoEditorPlayback::AddMarkerToPlayBar - Complex object array is full!" ) )
		{
			s32 const c_newMarkerDepth = ( VE_DEPTH_MARKERS + ms_markerDepthCount );
			CComplexObject newMarkerObject = CVideoEditorPlayback::ms_ComplexObject[PLAYBACK_OBJECT_PLAYBACK_PANEL].AttachMovieClipInstance( 
				type, c_newMarkerDepth );

			++ms_markerDepthCount;

			s32 const c_newTimeDilationDepth = ( VE_DEPTH_TIME_DILATION + ms_timeDilationDepthCount );
			CComplexObject timeDilationObject = CVideoEditorPlayback::ms_ComplexObject[PLAYBACK_OBJECT_PLAYBACK_PANEL].AttachMovieClipInstance( 
				"SPEED_INDICATOR", c_newTimeDilationDepth );

			++ms_timeDilationDepthCount;

			if( ms_ComplexObject[PLAYBACK_OBJECT_PLAYBACK_BG].IsActive() )
			{
				timeDilationObject.SetYPositionInPixels( ms_ComplexObject[PLAYBACK_OBJECT_PLAYBACK_BG].GetYPositionInPixels() );
			}

			if( IsEditing() && ( markerIndex == 0 || markerIndex == (u32)GetLastMarkerIndex() ) )
			{
				IReplayMarkerStorage const* markerStorage = GetMarkerStorage();
				sReplayMarkerInfo const* editMarker = markerStorage ? markerStorage->GetMarker( markerIndex ) : NULL;
				SetTransitionElementVisible( newMarkerObject, editMarker && editMarker->GetTransitionType() != MARKER_TRANSITIONTYPE_NONE );
			}

			MarkerDisplayObject displayObject;
			displayObject.m_markerComplexObject = newMarkerObject;
			displayObject.m_timeDilationComplexObject = timeDilationObject;

			ms_ComplexObjectMarkers.insert( ms_ComplexObjectMarkers.begin() + complexObjectInsertionIndex, displayObject );
			UpdateMarkerPositionOnPlayBar( displayObject.m_markerComplexObject, displayObject.m_timeDilationComplexObject,
				markerIndex, clipTime, clipEndTime, startPosition );
		}
	}
}

s32 CVideoEditorPlayback::GetMarkerIndexForComplexObjectName( char const * const objectName )
{
	return GetMarkerIndexForComplexObjectNameHash( rage::atStringHash( objectName ) );
}

s32 CVideoEditorPlayback::GetMarkerIndexForComplexObjectNameHash( u32 const objectNameHash )
{
	s32 resultIndex = -1;

	s32 const c_markerObjCount = ms_ComplexObjectMarkers.GetCount();
	for( s32 index = 0; resultIndex < 0 && index < c_markerObjCount; ++index )
	{
		CComplexObject* markerObject = GetEditMarkerComplexObject( index );
		resultIndex = ( markerObject && markerObject->GetObjectNameHash() == objectNameHash ) ? index : -1;
	}

	return resultIndex;
}

void CVideoEditorPlayback::RemoveMarkerOnPlaybar( u32 const index )
{
	if( index < CVideoEditorPlayback::ms_ComplexObjectMarkers.size() )
	{
		MarkerDisplayObject* markerObject = GetEditMarkerDisplayObject( index );
		if( markerObject )
		{
			SetEditMarkerState( markerObject->m_markerComplexObject, EDIT_MARKER_NO_FOCUS, false );
			markerObject->m_markerComplexObject.Release();

			CleanupTimeDilationElement( markerObject->m_timeDilationComplexObject );
			markerObject->m_timeDilationComplexObject.Release();
		}
		
		ms_ComplexObjectMarkers.Delete( index );
	}
}

void CVideoEditorPlayback::UpdateMarkerPositionOnPlayBar( CComplexObject& markerObject, CComplexObject& timeDilationObject, 
														 u32 const markerIndex, float const clipTime, float const clipEndTime, Vector2 const& startPosition )
{
	if( markerObject.IsActive() && timeDilationObject.IsActive() )
	{
		float const c_clampedRange = rage::ClampRange( clipTime, 0.f, clipEndTime );
		float const c_fMarkerPos = GetPlaybackBarMaxWidth() * c_clampedRange;
		float const c_markerWidth = markerObject.GetWidth();

		Vector2 finalPosition( startPosition );

		if( IsEditing() )
		{
			finalPosition.x -= markerIndex == 0 ? c_markerWidth : 0.f;
			finalPosition.y -= ( markerIndex == 0 || markerIndex == (u32)GetEditMarkerCount() - 1 ) ?
				(markerObject.GetHeight() / 2.f) :  0.f;
		}
		else
		{
			finalPosition.x -= c_markerWidth / 2.f;
		}
				
		finalPosition.x += c_fMarkerPos;

		markerObject.SetPositionInPixels( finalPosition );
		timeDilationObject.SetXPositionInPixels( finalPosition.x );
	}
}

bool CVideoEditorPlayback::UpdateFocusedMarker( float const currentTimeMs )
{
	s32 const c_initialFocus = ms_currentFocusIndex;

	IReplayMarkerStorage const* markerStorage = GetMarkerStorage();
	if( IsEditing() && markerStorage )
	{
		s32 const c_previousMarkerIndex = markerStorage->GetClosestMarkerIndex( currentTimeMs, CLOSEST_MARKER_LESS_OR_EQUAL );

		sReplayMarkerInfo const * const c_prePreviousMarker = markerStorage->TryGetMarker( c_previousMarkerIndex - 1 );
		sReplayMarkerInfo const * const c_previousMarker = markerStorage->TryGetMarker( c_previousMarkerIndex );
		sReplayMarkerInfo const * const c_nextMarker = markerStorage->TryGetMarker( c_previousMarkerIndex + 1 );

		float const c_prePreviousBoundaryNonDilatedTimeMs = c_prePreviousMarker ? c_prePreviousMarker->GetFrameBoundaryForThisMarker() : 0.f;
		float const c_previousBoundaryNonDilatedTimeMs = c_previousMarker ? c_previousMarker->GetFrameBoundaryForThisMarker() : 0.f;
		float const c_nextBoundaryNonDilatedTimeMs = c_nextMarker ? c_nextMarker->GetFrameBoundaryForThisMarker() : 0.f;

		float const c_previousMarkerNonDilatedTimeMs = c_previousMarker ? c_previousMarker->GetNonDilatedTimeMs() : 0.f;
		float const c_nextMarkerNonDilatedTimeMs = c_nextMarker ? c_nextMarker->GetNonDilatedTimeMs() : ms_project->GetRawEndTimeMs( ms_clipIndexToEdit );

		bool const c_closeToPrevious = c_previousMarker && ( currentTimeMs >= c_previousMarkerNonDilatedTimeMs - c_prePreviousBoundaryNonDilatedTimeMs && 
			currentTimeMs <= c_previousMarkerNonDilatedTimeMs + c_previousBoundaryNonDilatedTimeMs );

		bool const c_closeToNext = c_nextMarker && ( currentTimeMs >= c_nextMarkerNonDilatedTimeMs - c_previousBoundaryNonDilatedTimeMs && 
			currentTimeMs <= c_nextMarkerNonDilatedTimeMs + c_nextBoundaryNonDilatedTimeMs );

		ms_currentFocusIndex = c_closeToPrevious ? c_previousMarkerIndex : c_closeToNext ? c_previousMarkerIndex + 1 : -1;
	}

	bool const c_focusChanged = c_initialFocus != ms_currentFocusIndex;
	ms_refreshInstructionalButtons = c_focusChanged || ms_refreshInstructionalButtons;
	return c_focusChanged;
}

void CVideoEditorPlayback::RefreshEditMarkerMenu()
{
	s32 const c_lastMarkerIndex = GetLastMarkerIndex();
	sReplayMarkerInfo const* newMarker = GetCurrentEditMarker();

	if( IsFocusedOnEditMarker() )
	{
		ActuateCurrentEditMarker();
	}
	else if( IsOnEditMenu() )
	{
		DisableCurrentMenu();
		CReplayMarkerContext::SetJumpingOverrideNonDilatedTimeMs( newMarker->GetNonDilatedTimeMs() );

		bool const c_focusedOnLastMarker = ms_currentFocusIndex == c_lastMarkerIndex;
		bool const c_jumpToTopLevelMenu = !IsEditingEditMarker() && ( newMarker->IsAnchor() || 
			( c_focusedOnLastMarker && ( ( IsOnCameraMenu() &&  !newMarker->IsFreeCamera() ) || !IsOnCameraMenu() ) ) );

		if( IsEditingEditMarker() || c_jumpToTopLevelMenu )
		{
			EnableEditMarkerMenu( ms_menuFocusIndex );
			ms_cameraMenuStackedIndex = -1;
		}
		else if( IsOnPostFxMenu() )
		{
			EnterPostFxMenu( ms_menuFocusIndex );
		}
		else if( IsOnDoFMenu() )
		{
			EnterDoFMenu( ms_menuFocusIndex );
		}
		else if( IsOnCameraMenu() )
		{
			EnterCamMenu( ms_menuFocusIndex );
		}
		else if( IsOnAudioMenu() )
		{
			EnterAudioMenu( ms_menuFocusIndex );
		}
	}

	ms_refreshInstructionalButtons = true;
	ms_refreshEditMarkerState = true;
}

void CVideoEditorPlayback::UpdateFineScrubbing( float const increment )
{
	CVideoProjectPlaybackController& playbackController = CVideoEditorInterface::GetPlaybackController();

	bool const c_forward = increment > 0;
	bool const c_backward = increment < 0; 

	float const c_startTimeMs = playbackController.GetPlaybackCurrentClipStartNonDilatedTimeMs();
	float const c_endTimeMs = playbackController.GetPlaybackCurrentClipEndNonDilatedTimeMs();
	float const c_currentTimeMs = playbackController.GetPlaybackCurrentClipCurrentNonDilatedTimeMs();
	float const c_targetTimeMs = c_currentTimeMs + increment;

	float const c_clampedTimeMs = Clamp( c_targetTimeMs < 0 ? 0 : c_targetTimeMs, c_startTimeMs, c_endTimeMs );
	CReplayMarkerContext::SetJumpingOverrideNonDilatedTimeMs( c_clampedTimeMs );
	playbackController.JumpToNonDilatedTimeMs( c_clampedTimeMs, CReplayMgr::JO_FreezeRenderPhase | CReplayMgr::JO_FineScrubbing );

	if( ( c_forward || c_backward ) && c_clampedTimeMs != c_currentTimeMs )
	{
		playbackController.Pause();

		UpdateFocusedMarker( c_clampedTimeMs );
		g_FrontendAudioEntity.PlaySoundReplayScrubbing();

		if( IsOnEditMenu() )
		{
			CloseCurrentMenu();
		}
	}
	else
	{
		g_FrontendAudioEntity.StopSoundReplayScrubbing();
	}

	ms_refreshEditMarkerState = true;
	ms_refreshInstructionalButtons = true;
}

void CVideoEditorPlayback::UpdateScrubbing()
{
	float const c_scrubAxis = ms_bWasKbmLastFrame ? 0.f : CControlMgr::GetMainFrontendControl().GetFrontendRStickLeftRight().GetNorm();
	bool const c_forward = c_scrubAxis > 0.f;
	bool const c_backward = c_scrubAxis < 0.f; 

	CVideoProjectPlaybackController& playbackController = CVideoEditorInterface::GetPlaybackController();

	s32 const c_initialFocus = ms_currentFocusIndex;

	float const c_startTimeMs = playbackController.GetPlaybackCurrentClipStartNonDilatedTimeMs();
	float const c_endTimeMs = playbackController.GetPlaybackCurrentClipEndNonDilatedTimeMs();
	float currentTime;

	bool const c_wasScrubbing = ms_isScrubbing;

	UpdateScrubbingCommon( c_scrubAxis, c_startTimeMs, c_endTimeMs, ms_isScrubbing, currentTime );

	if( ms_isScrubbing && !c_wasScrubbing )
	{
		g_FrontendAudioEntity.PlaySoundReplayScrubbing();

		if( IsOnEditMenu() )
		{
			CloseCurrentMenu();
		}
	}
	else if( !ms_isScrubbing && c_wasScrubbing )
	{
		g_FrontendAudioEntity.StopSoundReplayScrubbing();
	}

	if( ( c_forward || c_backward ) )
	{
		UpdateFocusedMarker( currentTime );
	}

	bool const c_markerFocusChanged = ( ms_currentFocusIndex != c_initialFocus );

	//! Update visual state if we have changed scrubbing state
	ms_refreshEditMarkerState = ms_refreshEditMarkerState || ( ms_isScrubbing && !c_wasScrubbing ) || c_markerFocusChanged;
	ms_refreshInstructionalButtons = ms_refreshInstructionalButtons || ( ms_isScrubbing != c_wasScrubbing ) || c_markerFocusChanged;
}

void CVideoEditorPlayback::StopScrubbing()
{
	if( ms_isScrubbing )
	{
		CVideoProjectPlaybackController& playbackController = CVideoEditorInterface::GetPlaybackController();
		playbackController.Pause();
	}

	ResetScrubbingParams();
}

void CVideoEditorPlayback::ResetScrubbingParams()
{
	ms_isScrubbing = false;
	g_FrontendAudioEntity.StopSoundReplayScrubbing();
	ms_refreshEditMarkerState = true;
	ms_refreshInstructionalButtons = true;
}

bool CVideoEditorPlayback::UpdateScrubbingCommon( float scrubAxis, float const c_minTimeMs, float const c_maxTimeMs, 
												 bool& inout_isScrubbing, float& out_currentTimeMs )
{
	float const c_scrubAxisThreshold = GetScrubAxisThreshold();

#if RSG_PC
#define FRAMES_TO_KEEP_MOUSE_WHEEL_STATE (10)  // need to keep scrubbing if user is scrolling the mouse wheel over several frames, this covers the frames that the mouse technically says the user isnt scrolling
	static u32 uFrameCounter = 0;

	if( CanMouseScrub() )
	{
		if( scrubAxis == 0.0f )
		{
			if( CMousePointer::IsMouseWheelDown() )
			{
				scrubAxis = 1.0f;
			}

			if( CMousePointer::IsMouseWheelUp() )
			{
				scrubAxis = -1.0f;
			}
		}

		if( CMousePointer::IsMouseWheelUp() || CMousePointer::IsMouseWheelDown() )
		{
			uFrameCounter = FRAMES_TO_KEEP_MOUSE_WHEEL_STATE;
		}
	}
#endif // #if RSG_PC

	CVideoProjectPlaybackController& playbackController = CVideoEditorInterface::GetPlaybackController();
	float const previousTimeMs = playbackController.GetPlaybackCurrentClipCurrentNonDilatedTimeMs();
	out_currentTimeMs = previousTimeMs;

	bool const c_wasScrubbing = inout_isScrubbing;

	if( ( scrubAxis < -c_scrubAxisThreshold && out_currentTimeMs > c_minTimeMs ) || 
		( scrubAxis > c_scrubAxisThreshold && out_currentTimeMs <  c_maxTimeMs ) )
	{
		inout_isScrubbing = true;

		float const c_scrubSpeed = scrubAxis * GetScrubMultiplier();
		float const c_currentScrubSpeed = playbackController.GetCursorSpeed();

		if(!c_wasScrubbing || CReplayMgr::IsSystemPaused() || abs(c_currentScrubSpeed - c_scrubSpeed) >= 0.03f 
#if RSG_PC
			|| ( uFrameCounter == FRAMES_TO_KEEP_MOUSE_WHEEL_STATE && CanMouseScrub() )
#endif // #if RSG_PC
			)
		{
			playbackController.SetCursorSpeed( c_scrubSpeed );
		}
	}
	else
	{
#if RSG_PC
		bool bResetScrub = !CanMouseScrub();
		if( CanMouseScrub() )
		{
			if ( (!CMousePointer::IsMouseWheelUp()) && (!CMousePointer::IsMouseWheelDown()) )
			{
				if (uFrameCounter == 0)
				{
					bResetScrub = true;
				}
				else
				{
					uFrameCounter--;
				}
			}
			else
			{
				bResetScrub = true;
			}
		}

		if (bResetScrub)
#endif // #if RSG_PC
		{
			inout_isScrubbing = false;

			if( c_wasScrubbing )
			{
				playbackController.Pause();
			}
		}
	}

	out_currentTimeMs = playbackController.GetPlaybackCurrentClipCurrentNonDilatedTimeMs();

	if( out_currentTimeMs < c_minTimeMs || out_currentTimeMs > c_maxTimeMs )
	{
		StopScrubbing();
		out_currentTimeMs = Clamp( out_currentTimeMs, c_minTimeMs, c_maxTimeMs );
		playbackController.JumpToNonDilatedTimeMs( out_currentTimeMs, CReplayMgr::JO_FreezeRenderPhase );
	}

	return previousTimeMs != out_currentTimeMs;
}

float CVideoEditorPlayback::GetScrubAxisThreshold()
{
	return SCRUB_AXIS_THRESHOLD;
}

float CVideoEditorPlayback::GetScrubMultiplier()
{
	float const c_baseMultiplier = SCRUB_MULTIPLIER;

    CVideoProjectPlaybackController& playbackController = CVideoEditorInterface::GetPlaybackController();
    float const c_currentTime = playbackController.GetPlaybackCurrentNonDilatedTimeMs();

    u32 const c_currentClipIndex = playbackController.GetCurrentClipIndex();
    IReplayMarkerStorage const * const c_markerStorage = GetMarkerStorage( c_currentClipIndex );
    float const c_markerSpeed = c_markerStorage ? c_markerStorage->GetMarkerFloatSpeedAtCurrentNonDilatedTimeMs( c_currentTime ) : 1.f;
    float const c_finalMultiplier = c_baseMultiplier / c_markerSpeed;

	return c_finalMultiplier;
}

float CVideoEditorPlayback::GetFineScrubbingIncrement( float const multiplier )
{
	float const c_baseIncrement = FINE_SCRUB_INCREMENT * multiplier;

	CVideoProjectPlaybackController& playbackController = CVideoEditorInterface::GetPlaybackController();
	float const c_currentTime = playbackController.GetPlaybackCurrentNonDilatedTimeMs();

    u32 const c_currentClipIndex = playbackController.GetCurrentClipIndex();
    IReplayMarkerStorage const * const c_markerStorage = GetMarkerStorage( c_currentClipIndex );
    float const c_markerSpeed = c_markerStorage ? c_markerStorage->GetMarkerFloatSpeedAtCurrentNonDilatedTimeMs( c_currentTime ) : 1.f;
    float finalIncrement = c_baseIncrement * c_markerSpeed;

    bool const c_forward = finalIncrement > 0.f;
    eClosestMarkerPreference const c_searchPref = c_forward ? CLOSEST_MARKER_MORE : CLOSEST_MARKER_LESS;
    float const c_closestMarkerTimeMs = GetClosestEditMarkerTimeMs( c_currentClipIndex, c_searchPref );

    bool const c_crossingMarkerBoundary =  ( ( c_forward && c_currentTime < c_closestMarkerTimeMs && c_currentTime + finalIncrement > c_closestMarkerTimeMs ) ||
        ( !c_forward && c_currentTime > c_closestMarkerTimeMs && c_currentTime + finalIncrement < c_closestMarkerTimeMs ) );
    
    if( c_crossingMarkerBoundary ) 
    {
        finalIncrement = c_closestMarkerTimeMs - c_currentTime;
    }

	return finalIncrement;
}

bool CVideoEditorPlayback::UpdateMarkerDrag( u32 const markerIndex, float const playbarPercentage )
{
	bool timeChanged = false;

	IReplayMarkerStorage* markerStorage = GetMarkerStorage();
	sReplayMarkerInfo* heldMarker = markerStorage ? markerStorage->TryGetMarker( markerIndex ) : NULL;
	if( heldMarker && ms_project )
	{
		float percentageToJumpTo = playbarPercentage;
		bool shouldJump = true;

		float const c_oldMarkerNonDilatedTimeMs = heldMarker->GetNonDilatedTimeMs();

		bool const c_snapToBeat = CanSkipMarkerToBeat( markerIndex ) && CControlMgr::GetMainFrontendControl().GetReplayCtrl().IsDown();
		if( c_snapToBeat )
		{
			shouldJump = false;

			CVideoProjectPlaybackController& playbackController = CVideoEditorInterface::GetPlaybackController();

			float const c_percentageTimeMs = ms_project->ConvertClipPercentageToClipNonDilatedTimeMs( ms_clipIndexToEdit, playbarPercentage );
            u32 const c_percentageTimeMsRounded = (u32)(c_percentageTimeMs);
			bool const c_draggingForward = c_percentageTimeMs > c_oldMarkerNonDilatedTimeMs;

			float const c_beatSnapTimeMs = playbackController.CalculateClosestAudioBeatNonDilatedTimeMs( c_percentageTimeMs, !c_draggingForward );
            u32 const c_beatSnapTimeMsRounded = (u32)(c_beatSnapTimeMs);

			bool const c_snapToBeat = c_beatSnapTimeMs >= 0.f && ( c_percentageTimeMsRounded != c_beatSnapTimeMsRounded ) &&
				( (c_draggingForward && c_beatSnapTimeMs > c_oldMarkerNonDilatedTimeMs ) || ( !c_draggingForward && c_beatSnapTimeMs < c_oldMarkerNonDilatedTimeMs ) );

			if( c_snapToBeat )
			{	
				percentageToJumpTo = ms_project->ConvertClipNonDilatedTimeMsToClipPercentage( ms_clipIndexToEdit, c_beatSnapTimeMs );
				shouldJump = true;
			}
		}

		if( shouldJump )
		{
			float const c_newMarkerTimeMs = markerStorage->UpdateMarkerNonDilatedTimeForMarkerDragMs( markerIndex, percentageToJumpTo );
			timeChanged = c_oldMarkerNonDilatedTimeMs != c_newMarkerTimeMs;
		}
	}

	return timeChanged;
}

void CVideoEditorPlayback::UpdateRepeatedInput()
{
	float const c_xAxis = CControlMgr::GetMainFrontendControl().GetFrontendLeftRight().GetNorm();

	if( IsOnEditMenu() && !ms_bWasKbmLastFrame )
	{
		UpdateRepeatedInputMarkers( c_xAxis );
	}
}

void CVideoEditorPlayback::UpdateFocusedInput( s32 const c_inputType )
{
	if( IsEditing() && IsOnEditMenu() && !ms_bPreCaching )
	{
		UpdateFocusedInputEditMenu( c_inputType );
	}
}

void CVideoEditorPlayback::UpdateFocusedInputEditMenu( s32 const c_inputType )
{
	char const * audioTrigger = NULL;
	char const * soundSet = "HUD_FRONTEND_DEFAULT_SOUNDSET";

	IReplayMarkerStorage* markerStorage = GetMarkerStorage();
	sReplayMarkerInfo* currentMarker = GetCurrentEditMarker();
	u32 const c_currentMarkerIndex = (u32)GetCurrentEditMarkerIndex();

	MenuOption const * const c_option = TryGetMenuOption(ms_menuFocusIndex);;
	if( markerStorage && currentMarker && c_option )
	{
		s32 newFocus = ms_menuFocusIndex;
		s32 const c_menuFocusThreshold = ms_activeMenuOptions.GetCount() - 1;
		bool const c_optionEnabled = c_option->IsEnabled();

		if( c_inputType == INPUT_FRONTEND_UP )
		{
			--newFocus;

			newFocus = newFocus < 0 ? c_menuFocusThreshold - 1 : newFocus;
			audioTrigger = "NAV_UP_DOWN";
		}
		else if( c_inputType == INPUT_FRONTEND_DOWN )
		{
			++newFocus;
			newFocus = newFocus >= c_menuFocusThreshold ? 0 : newFocus;
			audioTrigger = "NAV_UP_DOWN";
		}
		else if( c_inputType == INPUT_FRONTEND_ACCEPT )
		{
			if( c_optionEnabled )
			{
				switch( c_option->m_option )
				{
				case MARKER_MENU_OPTION_POSTFX_MENU:
					{
						EnterPostFxMenu();

						newFocus = ms_menuFocusIndex;
						audioTrigger = "SELECT";
						break;
					}

				case MARKER_MENU_OPTION_DOF_MENU:
					{
						EnterDoFMenu();

						newFocus = ms_menuFocusIndex;
						audioTrigger = "SELECT";
						break;
					}

				case MARKER_MENU_OPTION_CAMERA_MENU:
					{
						ms_cameraMenuStackedIndex = -1;
						EnterCamMenu();

						newFocus = ms_menuFocusIndex;
						audioTrigger = "SELECT";
						break;
					}

				case MARKER_MENU_OPTION_AUDIO_MENU:
					{
						EnterAudioMenu();
						newFocus = ms_menuFocusIndex;
						audioTrigger = "SELECT";
						break;
					}

				case MARKER_MENU_OPTION_EDIT_CAMERA:
					{
						if( currentMarker->IsEditableCamera() )
						{
							EnterCamEditMode();
							newFocus = ms_menuFocusIndex;
							audioTrigger = "SELECT";
						}
						break;
					}

				case MARKER_MENU_OPTION_SET_THUMBNAIL:
					{
						UpdateThumbnail();
						break;
					}

				default:
					{
						// NOP
						break;
					}
				}
			}
			else
			{
				audioTrigger = "ERROR";
			}
		}
		else if( c_optionEnabled )
		{
			s32 const c_toggleIncrement = c_inputType == INPUT_FRONTEND_LEFT ? -1 : ( c_inputType == INPUT_FRONTEND_RIGHT ? 1 : 0 );
			float const c_percentDelta = c_inputType == INPUT_FRONTEND_LEFT ? -1.f : c_inputType == INPUT_FRONTEND_RIGHT ? 1.f :
				c_inputType == INPUT_REPLAY_CAMERAUP ? 5.f : c_inputType == INPUT_REPLAY_CAMERADOWN ? -5.f : 0.f ;

			switch( c_option->m_option )
			{
			case MARKER_MENU_OPTION_TYPE:
				{
					if( c_toggleIncrement != 0 )
					{
						if( currentMarker->UpdateMarkerType( c_toggleIncrement ) )
						{
							UpdateItemTextValue( ms_menuFocusIndex, TheText.Get( currentMarker->GetMarkerTypeLngKey() ) );
							markerStorage->RecalculateMarkerPositionsOnSpeedChange( c_currentMarkerIndex );

							ms_refreshEditMarkerState = true;
							RefreshMarkerPositionsOnPlayBar();

							// Force menu rebuild
							DisableCurrentMenu();
							EnableEditMarkerMenu();

							SetAnchorsHasBeenEdited();
							SetHasBeenEdited();
							audioTrigger = "NAV_LEFT_RIGHT";
						}
						else
						{
							audioTrigger = "ERROR";
						}
					}

					break;
				}

			case MARKER_MENU_OPTION_TRANSITION:
				{
					if( c_toggleIncrement != 0 )
					{
						if( currentMarker->UpdateTransitionType( c_toggleIncrement, 
							IsOnFirstMarker() ? MARKER_TRANSITIONTYPE_FADEOUT : MARKER_TRANSITIONTYPE_FADEIN ) )
						{

							UpdateItemTextValue( ms_menuFocusIndex, TheText.Get( currentMarker->GetTransitionTypeLngKey() ) );
							ms_refreshEditMarkerState = true;

							CComplexObject* markerComplexObject = GetCurrentEditMarkerComplexObject();
							if( markerComplexObject )
							{
								bool const c_hasTransition = currentMarker->GetTransitionType() != MARKER_TRANSITIONTYPE_NONE;
								SetEditMarkerState( *markerComplexObject, EDIT_MARKER_EDITING, false );	
								SetTransitionElementVisible( *markerComplexObject, c_hasTransition );
							}

							ms_project->ValidateTransitionDuration( ms_clipIndexToEdit );

							SetAnchorsHasBeenEdited();
							SetHasBeenEdited();
							audioTrigger = "NAV_LEFT_RIGHT";
						}
						else
						{
							audioTrigger = "ERROR";
						}
					}

					break;
				}

			case MARKER_MENU_OPTION_TIME:
				{
					float const c_multiplier = c_inputType == INPUT_FRONTEND_LEFT ? -1.f : c_inputType == INPUT_FRONTEND_RIGHT ? 1.f :
						c_inputType == INPUT_REPLAY_CAMERAUP ? 10.f : c_inputType == INPUT_REPLAY_CAMERADOWN ? -10.f : 0.f ;

					float const c_xAxis = GetFineScrubbingIncrement( c_multiplier ) / 100.f;
					if( c_xAxis != 0.f )
					{
						UpdateRepeatedInputMarkers( c_xAxis );
					}

					break;
				}

			case MARKER_MENU_OPTION_FILTER:
				{
					if( c_toggleIncrement != 0 )
					{
						u32 const c_newFx = c_toggleIncrement > 0 ? 
							CReplayCoordinator::GetPostFxRegistry().GetNextFilter( currentMarker->GetActivePostFx() ) : CReplayCoordinator::GetPostFxRegistry().GetPreviousFilter( currentMarker->GetActivePostFx() );

						currentMarker->SetActivePostFx( (u8)c_newFx );

						// Force menu rebuild
						DisableCurrentMenu();
						EnterPostFxMenu();

						SetHasBeenEdited();
						audioTrigger = "NAV_LEFT_RIGHT";
					}

					break;
				}

			case MARKER_MENU_OPTION_FILTER_INTENSITY:
				{
					if( c_percentDelta != 0.f )
					{
						if( currentMarker->UpdatePostFxIntensity( c_percentDelta ) )
						{
                            UpdateItemPercentValue( ms_menuFocusIndex, currentMarker->GetPostFxIntensityWholePercent() );
							SetHasBeenEdited();
							audioTrigger = "NAV_LEFT_RIGHT";
						}
						else
						{
							audioTrigger = "ERROR";
						}
					}

					break;
				}

			case MARKER_MENU_OPTION_SATURATION:
				{
					if( c_percentDelta != 0.f )
					{
						if( currentMarker->UpdateSaturationIntensity( c_percentDelta ) )
						{
                            UpdateItemFxValue( ms_menuFocusIndex, currentMarker->GetSaturationIntensityWholePercent() );
							SetHasBeenEdited();
							audioTrigger = "NAV_LEFT_RIGHT";
						}
						else
						{
							audioTrigger = "ERROR";
						}	
					}

					break;
				}

			case MARKER_MENU_OPTION_CONTRAST:
				{
					if( c_percentDelta != 0.f )
					{
						if( currentMarker->UpdateContrastIntensity( c_percentDelta ) )
						{
							UpdateItemFxValue( ms_menuFocusIndex, currentMarker->GetContrastIntensityWholePercent() );
							SetHasBeenEdited();
							audioTrigger = "NAV_LEFT_RIGHT";
						}
						else
						{
							audioTrigger = "ERROR";
						}
					}

					break;
				}

			case MARKER_MENU_OPTION_VIGNETTE:
				{
					if( c_percentDelta != 0.f )
					{
						if( currentMarker->UpdateVignetteIntensity( c_percentDelta ) )
						{
							UpdateItemFxValue( ms_menuFocusIndex, currentMarker->GetVignetteIntensityWholePercent() );
							SetHasBeenEdited();
							audioTrigger = "NAV_LEFT_RIGHT";
						}
						else
						{
							audioTrigger = "ERROR";
						}
					}

					break;
				}

			case MARKER_MENU_OPTION_BRIGHTNESS:
				{
					if( c_percentDelta != 0.f )
					{
						if( currentMarker->UpdateBrightness( c_percentDelta ) )
						{
							UpdateItemFxValue( ms_menuFocusIndex, currentMarker->GetBrightnessWholePercent() );
							SetHasBeenEdited();
							audioTrigger = "NAV_LEFT_RIGHT";
						}
						else
						{
							audioTrigger = "ERROR";
						}
					}

					break;
				}

			case MARKER_MENU_OPTION_DOF_MODE:
				{
					if( c_toggleIncrement != 0 )
					{
						if( currentMarker->UpdateDofMode( c_toggleIncrement ) )
						{
							// Force menu rebuild
							DisableCurrentMenu();
							EnterDoFMenu();

							SetHasBeenEdited();
							audioTrigger = "NAV_LEFT_RIGHT";
						}
						else
						{
							audioTrigger = "ERROR";
						}
					}

					break;
				}

			case MARKER_MENU_OPTION_DOF_INTENSITY:
				{
					if( c_percentDelta != 0.f )
					{
						if( currentMarker->UpdateDofIntensity( c_percentDelta ) )
						{
							UpdateItemPercentValue( ms_menuFocusIndex, currentMarker->GetDofIntensityWholePercent() );
							SetHasBeenEdited();
							audioTrigger = "NAV_LEFT_RIGHT";
						}
						else
						{
							audioTrigger = "ERROR";
						}
					}

					break;
				}

			case MARKER_MENU_OPTION_FOCUS_MODE:
				{
					if( c_toggleIncrement != 0 )
					{
						if( currentMarker->UpdateFocusMode( c_toggleIncrement ) )
						{
							// Force menu rebuild
							DisableCurrentMenu();
							EnterDoFMenu();

							SetHasBeenEdited();
							audioTrigger = "NAV_LEFT_RIGHT";
						}
						else
						{
							audioTrigger = "ERROR";
						}
					}

					break;
				}

			case MARKER_MENU_OPTION_FOCUS_DISTANCE:
				{
					float const c_focusDistanceDelta = c_inputType == INPUT_FRONTEND_LEFT ? -0.1f : c_inputType == INPUT_FRONTEND_RIGHT ? 0.1f :
						c_inputType == INPUT_REPLAY_CAMERAUP ? 5.0f : c_inputType == INPUT_REPLAY_CAMERADOWN ? -5.0f : 0.f ;

					if( c_focusDistanceDelta != 0.f )
					{
						if( currentMarker->UpdateFocalDistance( c_focusDistanceDelta ) )
						{
							UpdateItemMetresValueFloat( ms_menuFocusIndex, currentMarker->GetFocalDistance() );
							SetHasBeenEdited();
							audioTrigger = "NAV_LEFT_RIGHT";
						}
						else
						{
							audioTrigger = "ERROR";
						}
					}

					break;
				}

			case MARKER_MENU_OPTION_FOCUS_TARGET:
				{
					s32 const c_oldTarget = currentMarker->GetFocusTargetIndex();

					if( c_toggleIncrement > 0 )
					{
						camInterface::GetReplayDirector().IncreaseFocusTargetSelection(c_currentMarkerIndex);
					}
					else if( c_toggleIncrement < 0 )
					{
						camInterface::GetReplayDirector().DecreaseFocusTargetSelection(c_currentMarkerIndex);
					}

					s32 const c_newTarget = currentMarker->GetFocusTargetIndex();
					if( c_oldTarget != c_newTarget )
					{
						if( c_newTarget == -1 )
						{
							UpdateItemTextValue( ms_menuFocusIndex, TheText.Get( "VEUI_TARGET_NONE" ) );
						}
						else if( c_newTarget == 0 )
						{
							UpdateItemTextValue( ms_menuFocusIndex, TheText.Get( "VEUI_TARGET_PLYR" ) );
						}
						else
						{
							UpdateItemNumberValue( ms_menuFocusIndex, (u32)c_newTarget );
						}	

						audioTrigger = "NAV_LEFT_RIGHT";
						SetHasBeenEdited();
					}
					else
					{
						audioTrigger = "ERROR";
					}

					break;
				}

			case MARKER_MENU_OPTION_SPEED:
				{
					if( c_toggleIncrement != 0 )
					{
						sReplayMarkerInfo const * const c_currentMarker = GetCurrentEditMarker();
						markerStorage->IncrememtMarkerSpeed( c_currentMarkerIndex, c_toggleIncrement );
						markerStorage->RecalculateMarkerPositionsOnSpeedChange( c_currentMarkerIndex );
						ms_project->ValidateTransitionDuration( ms_clipIndexToEdit );

						//! We may have been shifted due to the time change, so ensure replay jumps to the accurate new time
						float const c_newMarkerTimeMs = c_currentMarker->GetNonDilatedTimeMs();
						CVideoProjectPlaybackController& playbackController = CVideoEditorInterface::GetPlaybackController();
						playbackController.JumpToNonDilatedTimeMs( c_newMarkerTimeMs, CReplayMgr::JO_FreezeRenderPhase );
						CReplayMarkerContext::SetJumpingOverrideNonDilatedTimeMs( c_newMarkerTimeMs );

						UpdateItemTextValue( ms_menuFocusIndex, TheText.Get( currentMarker->GetMarkerSpeedLngKey() ) );

						s32 menuIndex = -1;
						TryGetMenuOptionAndIndex( MARKER_MENU_OPTION_TIME, menuIndex );

						if( menuIndex != -1 )
						{
							UpdateItemTimeValue( menuIndex, (u32)c_newMarkerTimeMs );
						}

						RefreshMarkerPositionsOnPlayBar();

						SetHasBeenEdited();
						SetAnchorsHasBeenEdited();
						audioTrigger = "NAV_LEFT_RIGHT";
					}

					break;
				}

            case MARKER_MENU_OPTION_SFX_CAT:
                {
                    if( c_toggleIncrement != 0 )
                    {
                        if( currentMarker->UpdateMarkerSfxCategory( c_toggleIncrement ) )
                        {
                            UpdateItemTextValue( ms_menuFocusIndex, TheText.Get( currentMarker->GetSfxCategoryLngKey() ) );

                            s32 menuIndex = -1;
                            TryGetMenuOptionAndIndex( MARKER_MENU_OPTION_SFX_HASH, menuIndex );

                            if( menuIndex != -1 )
                            {
                                LocalizationKey stringBuffer;
                                currentMarker->GetSfxLngKey( stringBuffer.getBufferRef() );

                                UpdateItemTextValue( menuIndex, TheText.Get( stringBuffer.getBuffer() ) );
                            }    

                            SetHasBeenEdited();
                            audioTrigger = "NAV_LEFT_RIGHT";
                        }
                        else
                        {
                            audioTrigger = "ERROR";
                        }
                    }

                    break;
                }

            case MARKER_MENU_OPTION_SFX_HASH:
                {
                    if( c_toggleIncrement != 0 )
                    {
                        if( currentMarker->UpdateMarkerSfxHash( c_toggleIncrement ) )
                        {
                            LocalizationKey stringBuffer;
                            currentMarker->GetSfxLngKey( stringBuffer.getBufferRef() );

                            UpdateItemTextValue( ms_menuFocusIndex, TheText.Get( stringBuffer.getBuffer() ) );
                            SetHasBeenEdited();
							g_ReplayAudioEntity.SetPreviewMarkerSFX(true, currentMarker->GetSfxHash());
                            audioTrigger = "NAV_LEFT_RIGHT";
                        }
                        else
                        {
                            audioTrigger = "ERROR";
                        }
                    }

                    break;
                }

            case MARKER_MENU_OPTION_SFX_OS_VOL:
                {
                    if( c_toggleIncrement != 0 )
                    {
                        if( currentMarker->UpdateSfxOSVolume( c_toggleIncrement ) )
                        {
                            UpdateItemNumberValue( ms_menuFocusIndex, currentMarker->GetSfxOSVolume() );
                            SetHasBeenEdited();
                            audioTrigger = "NAV_LEFT_RIGHT";
                        }
                        else
                        {
                            audioTrigger = "ERROR";
                        }
                    }

                    break;
                }

			case MARKER_MENU_OPTION_SFX_VOL:
				{
					if( c_toggleIncrement != 0 )
					{
						if( currentMarker->UpdateSfxVolume( c_toggleIncrement ) )
						{
							UpdateItemNumberValue( ms_menuFocusIndex, currentMarker->GetSfxVolume() );
							SetHasBeenEdited();
							audioTrigger = "NAV_LEFT_RIGHT";
						}
						else
						{
							audioTrigger = "ERROR";
						}
					}

					break;
				}

			case MARKER_MENU_OPTION_DIALOG_VOL:
				{
					if( c_toggleIncrement != 0 )
					{
						if( currentMarker->UpdateDialogVolume( c_toggleIncrement ) )
						{
							UpdateItemNumberValue( ms_menuFocusIndex, currentMarker->GetDialogVolume() );
							SetHasBeenEdited();
							audioTrigger = "NAV_LEFT_RIGHT";
						}
						else
						{
							audioTrigger = "ERROR";
						}
					}

					break;
				}

			case MARKER_MENU_OPTION_MUSIC_VOL:
				{
					if( c_toggleIncrement != 0 )
					{
						if( currentMarker->UpdateMusicVolume( c_toggleIncrement ) )
						{
							UpdateItemNumberValue( ms_menuFocusIndex, currentMarker->GetMusicVolume() );
							SetHasBeenEdited();
							audioTrigger = "NAV_LEFT_RIGHT";
						}
						else
						{
							audioTrigger = "ERROR";
						}
					}

					break;
				}

            case MARKER_MENU_OPTION_AMB_VOL:
                {
                    if( c_toggleIncrement != 0 )
                    {
                        if( currentMarker->UpdateAmbientVolume( c_toggleIncrement ) )
                        {
                            UpdateItemNumberValue( ms_menuFocusIndex, currentMarker->GetAmbientVolume() );
                            SetHasBeenEdited();
                            audioTrigger = "NAV_LEFT_RIGHT";
                        }
                        else
                        {
                            audioTrigger = "ERROR";
                        }
                    }

                    break;
                }

			case MARKER_MENU_OPTION_SCORE_INTENSITY:
				{
					if( c_toggleIncrement != 0 )
					{
						if( currentMarker->UpdateScoreTrackIntensity( c_toggleIncrement ) )
						{
							UpdateItemTextValue( ms_menuFocusIndex, TheText.Get( currentMarker->GetMarkerAudioIntensityLngKey() ) );
							SetHasBeenEdited();
							audioTrigger = "NAV_LEFT_RIGHT";
						}
						else
						{
							audioTrigger = "ERROR";
						}
					}

					break;
				}

			case MARKER_MENU_OPTION_MIC_TYPE:
				{
					if( c_toggleIncrement != 0 )
					{
						if( currentMarker->UpdateMarkerMicrophoneType( c_toggleIncrement ) )
						{
							UpdateItemTextValue( ms_menuFocusIndex, TheText.Get( currentMarker->GetMarkerMicrophoneTypeLngKey() ) );

							// Force menu rebuild
							DisableCurrentMenu();
							EnterAudioMenu( ms_menuFocusIndex );

							SetHasBeenEdited();
							audioTrigger = "NAV_LEFT_RIGHT";
						}
						else
						{
							audioTrigger = "ERROR";
						}
					}

					break;
				}

			case MARKER_MENU_OPTION_CAMERA_TYPE:
				{
					if( c_toggleIncrement != 0 )
					{
						if( currentMarker->UpdateCameraType( c_toggleIncrement ) )
						{
							UpdateItemTextValue( ms_menuFocusIndex, TheText.Get( currentMarker->GetCameraTypeLngKey() ) );

							// Force menu rebuild
							DisableCurrentMenu();
							EnterCamMenu();

							SetHasBeenEdited();
							audioTrigger = "NAV_LEFT_RIGHT";
						}
						else
						{
							audioTrigger = "ERROR";
						}
					}

					break;
				}

			case MARKER_MENU_OPTION_CAMERA_BLEND:
				{
					if( c_toggleIncrement != 0 )
					{
						if( currentMarker->UpdateBlendType( c_toggleIncrement ) )
						{
							UpdateItemTextValue( ms_menuFocusIndex, TheText.Get( currentMarker->GetMarkerBlendLngKey() ) );

							DisableCurrentMenu();
							EnterCamMenu( ms_menuFocusIndex );

							markerStorage->BlendTypeUpdatedOnMarker( c_currentMarkerIndex );

							SetHasBeenEdited();
							audioTrigger = "NAV_LEFT_RIGHT";
						}
						else
						{
							audioTrigger = "ERROR";
						}
					}

					break;
				}

			case MARKER_MENU_OPTION_CAMERA_BLEND_EASING:
				{
					if( c_toggleIncrement != 0 )
					{
						if( currentMarker->UpdateBlendEaseType( c_toggleIncrement ) )
						{
							UpdateItemTextValue( ms_menuFocusIndex, TheText.Get( currentMarker->GetMarkerBlendEaseLngKey() ) );

							SetHasBeenEdited();
							audioTrigger = "NAV_LEFT_RIGHT";
						}
						else
						{
							audioTrigger = "ERROR";
						}
					}

					break;
				}

			case MARKER_MENU_OPTION_CAMERA_SHAKE:
				{
					if( c_toggleIncrement != 0 )
					{
						if( currentMarker->UpdateShakeType( c_toggleIncrement ) )
						{
							UpdateItemTextValue( ms_menuFocusIndex, TheText.Get( currentMarker->GetMarkerShakeLngKey() ) );

							// Force menu rebuild
							DisableCurrentMenu();
							EnterCamMenu( ms_menuFocusIndex );

							SetHasBeenEdited();
							audioTrigger = "NAV_LEFT_RIGHT";
						}
						else
						{
							audioTrigger = "ERROR";
						}
					}

					break;
				}

			case MARKER_MENU_OPTION_CAMERA_SHAKE_INTENSITY:
				{
					if( c_toggleIncrement != 0 )
					{
						if( currentMarker->UpdateShakeIntensity( c_toggleIncrement ) )
						{
							UpdateItemNumberValue( ms_menuFocusIndex, currentMarker->GetShakeIntensity() );

							SetHasBeenEdited();
							audioTrigger = "NAV_LEFT_RIGHT";
						}
						else
						{
							audioTrigger = "ERROR";
						}
					}

					break;
				}

			case MARKER_MENU_OPTION_CAMERA_SHAKE_SPEED:
				{
					if( c_toggleIncrement != 0 )
					{
						if( currentMarker->UpdateMarkerShakeSpeed( c_toggleIncrement ) )
						{
							UpdateItemTextValue( ms_menuFocusIndex, TheText.Get( currentMarker->GetMarkerShakeSpeedLngKey() ) );

							SetHasBeenEdited();
							SetAnchorsHasBeenEdited();
							audioTrigger = "NAV_LEFT_RIGHT";
						}
						else
						{
							audioTrigger = "ERROR";
						}
					}

					break;
				}

			case MARKER_MENU_OPTION_CAMERA_ATTACH_TARGET:
				{
					s32 const c_oldAttachTarget = currentMarker->GetAttachTargetIndex();

					if( c_toggleIncrement > 0 )
					{
						camInterface::GetReplayDirector().IncreaseAttachTargetSelection(c_currentMarkerIndex);
					}
					else if( c_toggleIncrement < 0 )
					{
						camInterface::GetReplayDirector().DecreaseAttachTargetSelection(c_currentMarkerIndex);
					}

					s32 const c_newAttachTarget = currentMarker->GetAttachTargetIndex();
					if( c_oldAttachTarget != c_newAttachTarget )
					{
						if( c_newAttachTarget == -1 )
						{
							UpdateItemTextValue( ms_menuFocusIndex, TheText.Get( "VEUI_TARGET_NONE" ) );
						}
						else if( c_newAttachTarget == 0 )
						{
							UpdateItemTextValue( ms_menuFocusIndex, TheText.Get( "VEUI_TARGET_PLYR" ) );
						}
						else
						{
							UpdateItemNumberValue( ms_menuFocusIndex, (u32)c_newAttachTarget );
						}	

						// Force menu rebuild
						DisableCurrentMenu();
						EnterCamMenu();

						audioTrigger = "NAV_LEFT_RIGHT";
						SetHasBeenEdited();
					}
					else
					{
						audioTrigger = "ERROR";
					}

					break;
				}

			case MARKER_MENU_OPTION_CAMERA_ATTACH_TYPE:
				{
					if( c_toggleIncrement != 0 )
					{
						if( currentMarker->UpdateAttachType( c_toggleIncrement ) )
						{
							UpdateItemTextValue( ms_menuFocusIndex, TheText.Get( currentMarker->GetAttachTypeLngKey() ) );

							// Force menu rebuild (this may alter look at target selection)
							DisableCurrentMenu();
							EnterCamMenu(); 

							SetHasBeenEdited();
							audioTrigger = "NAV_LEFT_RIGHT";
						}
						else
						{
							audioTrigger = "ERROR";
						}
					}

					break;
				}

			case MARKER_MENU_OPTION_CAMERA_LOOKAT_TARGET:
				{
					s32 const c_oldLookTarget = currentMarker->GetLookAtTargetIndex();
					if( c_toggleIncrement > 0 )
					{
						camInterface::GetReplayDirector().IncreaseLookAtTargetSelection(c_currentMarkerIndex);
					}
					else if( c_toggleIncrement < 0 )
					{
						camInterface::GetReplayDirector().DecreaseLookAtTargetSelection(c_currentMarkerIndex);
					}

					s32 const c_newLookTarget = currentMarker->GetLookAtTargetIndex();
					if( c_oldLookTarget != c_newLookTarget )
					{
						if( c_newLookTarget == -1 )
						{
							UpdateItemTextValue( ms_menuFocusIndex, TheText.Get( "VEUI_TARGET_NONE" ) );
						}
						else if( c_newLookTarget == 0 )
						{
							UpdateItemTextValue( ms_menuFocusIndex, TheText.Get( "VEUI_TARGET_PLYR" ) );
						}
						else
						{
							UpdateItemNumberValue( ms_menuFocusIndex, (u32)c_newLookTarget );
						}	

						// Force menu rebuild (this may alter attach type selection)
						DisableCurrentMenu();
						EnterCamMenu(); 

						audioTrigger = "NAV_LEFT_RIGHT";
						SetHasBeenEdited();
					}
					else
					{
						audioTrigger = "ERROR";
					}

					break;
				}
			default:
				{
					// NOP
					break;
				}
			}
		}

		if( newFocus != ms_menuFocusIndex )
		{
			ms_menuFocusIndex = newFocus;

			MenuOption const * const c_newOption = TryGetMenuOption(ms_menuFocusIndex);
			bool const c_newOptionEnabled = c_newOption ? c_newOption->IsEnabled() : false;

			ms_editMenuStackedIndex = IsEditingEditMarker() ? ms_menuFocusIndex : ms_editMenuStackedIndex;
			ms_cameraMenuStackedIndex = IsOnCameraMenu() ? ms_menuFocusIndex : ms_cameraMenuStackedIndex;

			SetItemSelected( ms_menuFocusIndex, !c_newOptionEnabled );

			UpdateMenuHelpText( ms_menuFocusIndex );
			UpdateEditMenuState();

			ms_refreshInstructionalButtons = true;

			UpdateItemFocusDependentState();
		}
	}

	if( audioTrigger && soundSet )
	{
		g_FrontendAudioEntity.PlaySound( audioTrigger, soundSet );
	}
}

void CVideoEditorPlayback::UpdateRepeatedInputMarkers( float const c_xAxis )
{
#define SOUND_TRIGGER_PERIOD (12)
	char const * audioTrigger = NULL;
	static s32 soundTriggerDelay = 0;
	static bool wasLastSoundError = false;

	IReplayMarkerStorage* markerStorage = GetMarkerStorage();
	sReplayMarkerInfo* currentMarker = GetCurrentEditMarker();
	s32 const c_currentMarkerIndex = GetCurrentEditMarkerIndex();

	if( markerStorage && currentMarker && markerStorage->IsValidMarkerIndex( c_currentMarkerIndex ) )
	{	
		float const c_percentDelta = MARKER_PERCENT_MULTIPLIER * c_xAxis;
		float const c_metresDelta = MARKER_METRES_MULTIPLIER * c_xAxis;

		// No sound by default
		bool triggerUpdateSound = false;
		bool triggerErrorSound = false;

		int const c_menuOptionCount = ms_activeMenuOptions.GetCount();
		if( uiVerifyf( c_menuOptionCount > 0 && ms_menuFocusIndex < c_menuOptionCount,
			"CVideoEditorPlayback::UpdateRepeatedInputMarkers - Updating input with invalid focus index %d of %d total options"
			, ms_menuFocusIndex, c_menuOptionCount ) )
		{
			MenuOption& option = ms_activeMenuOptions[ms_menuFocusIndex];
			if( option.m_option == MARKER_MENU_OPTION_TIME )
			{
				if( UpdateRepeatedInputMarkerTime( c_xAxis ) )
				{
					UpdateMarkerMenuTimeValue( (u32)markerStorage->GetMarkerDisplayTimeMs( c_currentMarkerIndex ) );
					g_FrontendAudioEntity.PlaySoundNavLeftRightContinuous();
				}
			}
			else if( option.m_option == MARKER_MENU_OPTION_FILTER_INTENSITY && c_percentDelta != 0.f )
			{			
				s32 const c_startingIntensity = currentMarker->GetPostFxIntensityWholePercent();

				if( currentMarker->UpdatePostFxIntensity( c_percentDelta ) )
				{
					SetHasBeenEdited();
					s32 const c_newIntensity = currentMarker->GetPostFxIntensityWholePercent();

					if( c_startingIntensity != c_newIntensity )
					{
						UpdateItemPercentValue( ms_menuFocusIndex, c_newIntensity );
						triggerUpdateSound = true;
					}
				}
				else
				{
					triggerErrorSound = true;
				}
			}
			else if( option.m_option == MARKER_MENU_OPTION_SATURATION && c_percentDelta != 0.f )
			{			
				s32 const c_startingSaturation = currentMarker->GetSaturationIntensityWholePercent();

				if( currentMarker->UpdateSaturationIntensity( c_percentDelta ) )
				{
					SetHasBeenEdited();
					s32 const c_newSaturation = currentMarker->GetSaturationIntensityWholePercent();

					if( c_startingSaturation != c_newSaturation )
					{
						UpdateItemFxValue( ms_menuFocusIndex, c_newSaturation );
						triggerUpdateSound = true;
					}
				}
				else
				{
					triggerErrorSound = true;
				}
			}
			else if( option.m_option == MARKER_MENU_OPTION_CONTRAST && c_percentDelta != 0.f )
			{			
				s32 const c_startingContrast = currentMarker->GetContrastIntensityWholePercent();

				if( currentMarker->UpdateContrastIntensity( c_percentDelta ) )
				{
					SetHasBeenEdited();
					s32 const c_newContrast = currentMarker->GetContrastIntensityWholePercent();

					if( c_startingContrast != c_newContrast )
					{
						UpdateItemFxValue( ms_menuFocusIndex, c_newContrast );
						triggerUpdateSound = true;
					}
				}
				else
				{
					triggerErrorSound = true;
				}
			}
			else if( option.m_option == MARKER_MENU_OPTION_VIGNETTE && c_percentDelta != 0.f )
			{			
				s32 const c_startingVignette = currentMarker->GetVignetteIntensityWholePercent();

				if( currentMarker->UpdateVignetteIntensity( c_percentDelta ) )
				{
					SetHasBeenEdited();
					s32 const c_newVignette = currentMarker->GetVignetteIntensityWholePercent();

					if( c_startingVignette != c_newVignette )
					{
						UpdateItemFxValue( ms_menuFocusIndex, c_newVignette );
						triggerUpdateSound = true;
					}
				}
				else
				{
					triggerErrorSound = true;
				}
			}
			else if( option.m_option == MARKER_MENU_OPTION_BRIGHTNESS && c_percentDelta != 0.f )
			{			
				s32 const c_startingBrightness = currentMarker->GetBrightnessWholePercent();

				if( currentMarker->UpdateBrightness( c_percentDelta ) )
				{
					SetHasBeenEdited();
					s32 const c_newBrightness = currentMarker->GetBrightnessWholePercent();

					if( c_startingBrightness != c_newBrightness )
					{
						UpdateItemFxValue( ms_menuFocusIndex, c_newBrightness );
						triggerUpdateSound = true;
					}
				}
				else
				{
					triggerErrorSound = true;
				}
			}
			else if( option.m_option == MARKER_MENU_OPTION_DOF_INTENSITY && c_percentDelta != 0.f )
			{			
				s32 const c_startingIntensity = currentMarker->GetDofIntensityWholePercent();

				if( currentMarker->UpdateDofIntensity( c_percentDelta ) )
				{
					SetHasBeenEdited();
					s32 const c_newIntensity = currentMarker->GetDofIntensityWholePercent();

					if( c_startingIntensity != c_newIntensity )
					{
						UpdateItemPercentValue( ms_menuFocusIndex, c_newIntensity );
						triggerUpdateSound = true;
					}
				}
				else
				{
					triggerErrorSound = true;
				}
			}
			else if( option.m_option == MARKER_MENU_OPTION_FOCUS_DISTANCE && c_metresDelta != 0.f )
			{			
				s32 const c_startingFocalDistance = currentMarker->GetFocalDistanceWholeDecimetre();

				if( currentMarker->UpdateFocalDistance( c_metresDelta ) )
				{
					SetHasBeenEdited();
					s32 const c_newFocalDistance = currentMarker->GetFocalDistanceWholeDecimetre();

					if( c_startingFocalDistance != c_newFocalDistance )
					{
						UpdateItemMetresValueFloat( ms_menuFocusIndex, currentMarker->GetFocalDistance() );
						triggerUpdateSound = true;
					}
				}
				else
				{
					triggerErrorSound = true;
				}
			}
		}

		if( triggerUpdateSound )
		{
			audioTrigger = "NAV_LEFT_RIGHT";
			wasLastSoundError = false;
			soundTriggerDelay = 0;
		}
		else if( triggerErrorSound )
		{
			audioTrigger = "ERROR";
			soundTriggerDelay = !wasLastSoundError ? 0 : soundTriggerDelay > 0 ? soundTriggerDelay : SOUND_TRIGGER_PERIOD;
			wasLastSoundError = true;
		}
	}

	if( soundTriggerDelay > 0 )
	{
		--soundTriggerDelay;
	}

	if( audioTrigger && soundTriggerDelay <= 0 )
	{
		g_FrontendAudioEntity.PlaySound( audioTrigger, "HUD_FRONTEND_DEFAULT_SOUNDSET" );
	}
}

bool CVideoEditorPlayback::UpdateRepeatedInputMarkerTime( float const c_xAxis )
{
	bool hasChanged = false;

	IReplayMarkerStorage* markerStorage = GetMarkerStorage();
	sReplayMarkerInfo* currentMarker = GetCurrentEditMarker();
	s32 const c_currentMarkerIndex = GetCurrentEditMarkerIndex();
	if( c_xAxis != 0.f && markerStorage && currentMarker && markerStorage->IsValidMarkerIndex( c_currentMarkerIndex ) )
	{	
		CVideoProjectPlaybackController& playbackController = CVideoEditorInterface::GetPlaybackController();

		float const c_timeDelta = (float)rage::round( c_xAxis * 100.f );
		float const c_oldTimeMs = currentMarker->GetNonDilatedTimeMs();
		float newTimeMs = c_oldTimeMs;

		bool const c_timeChanged = markerStorage->CanIncrementMarkerNonDilatedTimeMs( c_currentMarkerIndex, c_timeDelta, newTimeMs );
		if( c_timeChanged && c_oldTimeMs != newTimeMs )
		{
			// HACK - B*2053311 - Replay system takes a frame to jump times, so we need to override time given to other systems
			CReplayMarkerContext::SetJumpingOverrideNonDilatedTimeMs( newTimeMs );
			CReplayMarkerContext::SetShiftingCurrentMarker( true );

			markerStorage->SetMarkerNonDilatedTimeMs( c_currentMarkerIndex, newTimeMs );
			playbackController.JumpToNonDilatedTimeMs( newTimeMs, CReplayMgr::JO_LinearJump );

			SetHasBeenEdited();
			SetAnchorsHasBeenEdited();
			RefreshMarkerPositionsOnPlayBar();

			ms_refreshEditMarkerState = true;
			hasChanged = true;
		}
		else
		{
			CReplayMarkerContext::SetShiftingCurrentMarker( false );
		}
	}
	else
	{
		CReplayMarkerContext::SetShiftingCurrentMarker( false );
	}

	return hasChanged;
}

bool CVideoEditorPlayback::UpdateMarkerJumpToBeat()
{
	bool jumpedToBeat = false;
    char const * audioTrigger = NULL;

    CVideoProjectPlaybackController& playbackController = CVideoEditorInterface::GetPlaybackController();

	IReplayMarkerStorage* markerStorage = GetMarkerStorage();
	sReplayMarkerInfo* currentMarker = GetCurrentEditMarker();
	s32 const c_currentMarkerIndex = GetCurrentEditMarkerIndex();

	if( markerStorage && currentMarker && markerStorage->IsValidMarkerIndex( c_currentMarkerIndex ) && CanSkipToBeat() )
	{	
        u32 const c_originalTimeMs = (u32)markerStorage->GetMarker( c_currentMarkerIndex )->GetNonDilatedTimeMs();
        u32 targetTimeMs = c_originalTimeMs;

		float targetJumpTimeFloatMs = -1;

		if( CPauseMenu::CheckInput( FRONTEND_INPUT_UP, false, CHECK_INPUT_OVERRIDE_FLAG_IGNORE_ANALOGUE_STICKS ) )
		{
			targetJumpTimeFloatMs = playbackController.CalculateNextAudioBeatNonDilatedTimeMs();
            targetTimeMs = (u32)targetJumpTimeFloatMs;
		}
		else if( CPauseMenu::CheckInput( FRONTEND_INPUT_DOWN, false, CHECK_INPUT_OVERRIDE_FLAG_IGNORE_ANALOGUE_STICKS ) )
		{
			targetJumpTimeFloatMs = playbackController.CalculatePrevAudioBeatNonDilatedTimeMs();
            targetTimeMs = (u32)targetJumpTimeFloatMs;
		}

		if( c_originalTimeMs != targetTimeMs )
		{
			float minNonDilatedTimeMs = 0;
			float maxNonDilatedTimeMs = 0;
			markerStorage->GetMarkerNonDilatedTimeConstraintsMs( c_currentMarkerIndex, minNonDilatedTimeMs, maxNonDilatedTimeMs );

			if( targetJumpTimeFloatMs >= minNonDilatedTimeMs && targetJumpTimeFloatMs <= maxNonDilatedTimeMs )
			{
				// HACK - B*2053311 - Replay system takes a frame to jump times, so we need to override time given to other systems
				CReplayMarkerContext::SetJumpingOverrideNonDilatedTimeMs( targetJumpTimeFloatMs );
				CReplayMarkerContext::SetShiftingCurrentMarker( true );

				markerStorage->SetMarkerNonDilatedTimeMs( c_currentMarkerIndex, targetJumpTimeFloatMs );

                u32 const c_newTimeMs = (u32)markerStorage->GetMarker( c_currentMarkerIndex )->GetNonDilatedTimeMs();

				jumpedToBeat = playbackController.JumpToNonDilatedTimeMs( targetJumpTimeFloatMs, CReplayMgr::JO_FreezeRenderPhase ) && c_originalTimeMs != c_newTimeMs;

				SetHasBeenEdited();
				SetAnchorsHasBeenEdited();
				RefreshMarkerPositionsOnPlayBar();
            }

            audioTrigger = jumpedToBeat ? "NAV_LEFT_RIGHT" : "ERROR";
		}
	}

	if( audioTrigger )
	{
		g_FrontendAudioEntity.PlaySound( audioTrigger, "HUD_FRONTEND_DEFAULT_SOUNDSET" );
	}

    ms_refreshInstructionalButtons = ms_refreshInstructionalButtons || jumpedToBeat;

	return jumpedToBeat;
}

void CVideoEditorPlayback::UpdateCameraControls()
{
	UpdateCameraOverlay();

#if RSG_PC
	CMousePointer::SetMouseCursorVisible( !CReplayMarkerContext::IsCameraUsingMouse() );
#endif

	if( CControlMgr::GetMainFrontendControl().GetFrontendCancel().IsReleased() ||
		CControlMgr::GetMainFrontendControl().GetReplayRestart().IsReleased() ||
		CMousePointer::IsMouseBack() )
	{
		ExitCamEditMode();
		g_FrontendAudioEntity.PlaySound( "BACK", "HUD_FRONTEND_DEFAULT_SOUNDSET" );

#if RSG_PC
		CMousePointer::SetMouseCursorVisible( true );
#endif

	}
}

void CVideoEditorPlayback::UpdateWarningText()
{
	bool const c_canShowWarnings = !IsBaking() && !IsPreviewingProject();

	ms_warningDisplayTimeRemainingMs -= ms_warningDisplayTimeRemainingMs > 0.f ? 
													fwTimer::GetTimeStep_NonPausedNonScaledClipped() : 0.f;

	bool const c_hasPendingWarning = c_canShowWarnings ? CReplayMarkerContext::HasWarning() : false;
	CReplayMarkerContext::eEditWarnings const c_pendingWarning = c_canShowWarnings ? 
												CReplayMarkerContext::GetWarning() : CReplayMarkerContext::EDIT_WARNING_NONE;

	bool const c_warningsAllowedThisFrame = c_canShowWarnings && !ms_bLoadingScreen && ms_bActive && ms_bReadyToPlay;
	bool const c_hideWarning = !c_warningsAllowedThisFrame || ( HasWarningText() && ms_warningDisplayTimeRemainingMs <= 0.f) || !c_hasPendingWarning;

	if( HasWarningText() && c_hideWarning )
	{
		ClearWarningText();

		if( c_pendingWarning == ms_currentEditWarning )
		{
			CReplayMarkerContext::ResetWarning();
		}

		ms_currentEditWarning = CReplayMarkerContext::EDIT_WARNING_NONE;
	}
	else if( c_warningsAllowedThisFrame )
	{
		bool const c_updateWarning = c_hasPendingWarning && ( !HasWarningText() || c_pendingWarning != ms_currentEditWarning );

		if( c_updateWarning )
		{
			char const * const c_warningTextKey = CReplayMarkerContext::GetWarningLngKey();
			SetWarningText( c_warningTextKey );
			ms_currentEditWarning = c_pendingWarning;

			ms_warningDisplayTimeRemainingMs = WARNING_TEXT_DISPLAY_TIME;
		}
	}
}

bool CVideoEditorPlayback::CheckExportStatus()
{
	bool statusChange = false;

	if( IsBaking() )
	{
		if( CVideoEditorInterface::HasVideoRenderErrored() )
		{
			uiDisplayf( "CVideoEditorPlayback::Update - Video export has errored out, abandoning attempt" );

			char const * const c_errorLngKey = CVideoEditorInterface::GetVideoRenderErrorLngKey();
			CVideoEditorMenu::SetUserConfirmationScreen( c_errorLngKey ? c_errorLngKey : "VE_BAKE_ERROR" );  

			CVideoEditorInterface::KillPlaybackOrBake(true);
			CVideoEditorUi::RefreshVideoViewWhenSafe();
			CBusySpinner::Off( SPINNER_SOURCE_VIDEO_EDITOR );

			ms_bWantDelayedClose = true;
			statusChange = true;
		}
		else if( !CVideoEditorInterface::IsRenderingVideo() && !CVideoEditorInterface::ShouldShowLoadingScreen() &&
			!CVideoEditorInterface::IsPendingVideoFinalization() && !VideoRecording::IsRecording() )  
		{
            CBusySpinner::Off( SPINNER_SOURCE_VIDEO_EDITOR );
            CVideoEditorUi::RefreshVideoViewWhenSafe(true);

			// display "playback finished" when we return to the menus
			char const * outputName = CVideoEditorUi::GetExportName();
			CVideoEditorMenu::SetUserConfirmationScreen( outputName ? "VE_BAKE_FIN_NAMED" : 
				"VE_BAKE_FIN", "VE_SUCCESS", FE_WARNING_OK, false, outputName ? outputName : "" );  

			// register video metadata
			if(outputName)
			{
				CReplayVideoMetadataStore::VideoMetadata metadata;
				metadata.score = CVideoEditorInterface::GetActiveProject()->GetRating();
				metadata.trackId = CVideoEditorInterface::GetActiveProject()->GetLongestTrackId();
				CReplayVideoMetadataStore::GetInstance().RegisterVideo(outputName, metadata);
				
				CProfileSettings& settings = CProfileSettings::GetInstance();
				if(settings.AreSettingsValid())
				{
					s32 videoCount = 0;
					if(settings.Exists(CProfileSettings::REPLAY_VIDEOS_CREATED))
					{
						videoCount = settings.GetInt(CProfileSettings::REPLAY_VIDEOS_CREATED);
					}

					const s32 achievementNoofVideos = 10;
					videoCount++;
					if (videoCount <= achievementNoofVideos)
					{
#if RSG_DURANGO || __STEAM_BUILD
						CLiveManager::GetAchMgr().SetAchievementProgress(player_schema::Achievements::ACHR2, videoCount);
#endif
						settings.Set(CProfileSettings::REPLAY_VIDEOS_CREATED, videoCount);
						settings.Write();
					}

					// keep on trying to achieve awards with >=, in case trophies have been deleted ... this will re-award it B* 2426356
#if RSG_ORBIS || RSG_DURANGO
					// console version of achievement occurs on video creation, not upload like on PC
					if (videoCount >= 1)
					{
						// Unlock ACH H11 - Vinewood Visionaryer
						CLiveManager::GetAchMgr().AwardAchievement(player_schema::Achievements::ACHH11);
					}
#endif
					if (videoCount >= achievementNoofVideos)
					{
						// Unlock ACH R2 - Majestic
						CLiveManager::GetAchMgr().AwardAchievement(player_schema::Achievements::ACHR2);
					}
				}
			}

			ms_bWantDelayedClose = true;
			statusChange = true;
		}	
		else if( CVideoEditorInterface::HasVideoRenderSuspended() )
		{
			uiDisplayf( "CVideoEditorPlayback::Update - Entering suspend mode, cancelling video export" );
			CVideoEditorInterface::KillPlaybackOrBake(true);
			CVideoEditorUi::RefreshVideoViewWhenSafe();
			CBusySpinner::Off( SPINNER_SOURCE_VIDEO_EDITOR );
			CVideoEditorMenu::SetUserConfirmationScreen("VE_BAKE_SUS");  

			ms_bWantDelayedClose = true;
			statusChange = true;
		}
#if !RSG_PC // B*2161840 - Allow video capture to continue despite "focus loss"
		else if( CVideoEditorInterface::HasVideoRenderBeenConstrained() )
		{
			uiDisplayf( "CVideoEditorPlayback::Update - Entering constrained mode, cancelling video export" );

			CVideoEditorInterface::KillPlaybackOrBake(true);
			CVideoEditorUi::RefreshVideoViewWhenSafe();
			CBusySpinner::Off( SPINNER_SOURCE_VIDEO_EDITOR );
			CVideoEditorMenu::SetUserConfirmationScreen("VE_BAKE_CONS");  

			ms_bWantDelayedClose = true;
			statusChange = true;
		}
#endif
	}

	return statusChange;
}

#if defined(SHOW_PLAYBACK_BUTTONS) && SHOW_PLAYBACK_BUTTONS

void CVideoEditorPlayback::SetPlaybackButtonVisibility( s32 const button, bool const isVisible )
{
	if( button >= PLAYBACK_BUTTON_FIRST && button < PLAYBACK_BUTTON_MAX &&
		CVideoEditorPlayback::ms_ComplexObject[PLAYBACK_OBJECT_BUTTON_PANEL].IsActive() )
	{
		char const * buttonName = sc_buttonObjectNames[ button ];
		CComplexObject button = CVideoEditorPlayback::ms_ComplexObject[PLAYBACK_OBJECT_BUTTON_PANEL].GetObject( buttonName );
		if( button.IsActive() )
		{
			button.SetVisible( isVisible );
			button.Release();
		}
	}
}

void CVideoEditorPlayback::UpdatePlayPauseButtonState( s32 const iMouseEventType )
{
	bool const c_isPaused = CVideoEditorInterface::GetPlaybackController().IsPaused();

	bool const c_hasFocus = iMouseEventType == EVENT_TYPE_MOUSE_ROLL_OVER;
	bool const c_lostFocus = iMouseEventType == EVENT_TYPE_MOUSE_ROLL_OUT || iMouseEventType == EVENT_TYPE_MOUSE_RELEASE_OUTSIDE;

	if( c_hasFocus || c_lostFocus )
	{
		s32 const playPauseButtonState =  c_isPaused ?  ( c_hasFocus ? PLAYPAUSE_PLAY_FOCUS : PLAYPAUSE_PLAY_NO_FOCUS ) :
			( c_hasFocus ? PLAYPAUSE_PAUSE_FOCUS : PLAYPAUSE_PAUSE_NO_FOCUS );

		// Store state then do the scaleform update
		ms_playbackButtonStates[ PLAYBACK_BUTTON_PLAYPAUSE ] = playPauseButtonState;
		SetPlaybackButtonState( PLAYBACK_BUTTON_PLAYPAUSE, playPauseButtonState );
	}
}

void CVideoEditorPlayback::UpdatePlayPauseButtonState()
{
	bool const c_isPaused = CVideoEditorInterface::GetPlaybackController().IsPaused();
	bool const c_hasFocus = ms_playbackButtonStates[ PLAYBACK_BUTTON_PLAYPAUSE ] == PLAYPAUSE_PLAY_FOCUS ||
									ms_playbackButtonStates[ PLAYBACK_BUTTON_PLAYPAUSE ] == PLAYPAUSE_PAUSE_FOCUS;

	s32 const playPauseButtonState =  c_isPaused ?  ( c_hasFocus ? PLAYPAUSE_PLAY_FOCUS : PLAYPAUSE_PLAY_NO_FOCUS ) :
													( c_hasFocus ? PLAYPAUSE_PAUSE_FOCUS : PLAYPAUSE_PAUSE_NO_FOCUS );

	SetPlaybackButtonState( PLAYBACK_BUTTON_PLAYPAUSE, playPauseButtonState );
}


void CVideoEditorPlayback::UpdatePlaybackButtonAction( ePlaybackButtonId const playbackButton, s32 const iMouseEventType )
{
	bool const c_doAction = iMouseEventType == EVENT_TYPE_MOUSE_RELEASE;
	bool const c_isPrecaching = CReplayMgr::IsPreCachingScene();
	if( c_doAction && playbackButton > PLAYBACK_BUTTON_INVALID && playbackButton < PLAYBACK_BUTTON_MAX )
	{
		StopScrubbing();

		char const * audioTrigger = NULL;
		bool closeEditMenuIfOpen = false;

		CVideoProjectPlaybackController& playbackController = CVideoEditorInterface::GetPlaybackController();

		switch ( playbackButton )
		{
		case PLAYBACK_BUTTON_SKIPTOSTART:
			{
				if( IsEditing() )
				{
					playbackController.JumpToStart();
				}
				else
				{
					playbackController.JumpToPreviousClip();
				}

				audioTrigger = "SELECT";
				closeEditMenuIfOpen = true;
				break;
			}
		case PLAYBACK_BUTTON_REWIND:
			{
				if( !c_isPrecaching )
				{
					playbackController.ApplyRewind();
					audioTrigger = "SELECT";
					closeEditMenuIfOpen = !playbackController.IsAtStartOfPlayback();
				}

				break;
			}
		case PLAYBACK_BUTTON_PLAYPAUSE:
			{
				bool isAtEndOfPlayback = playbackController.IsAtEndOfPlayback();

				if( !c_isPrecaching && !isAtEndOfPlayback )
				{
					audioTrigger = playbackController.IsPaused() ?
						"RESUME" : "PAUSE_FIRST_HALF";

					playbackController.TogglePlayOrPause();
					closeEditMenuIfOpen = !isAtEndOfPlayback;
				}

				break;
			}
		case PLAYBACK_BUTTON_FASTFORWARD:
			{
				if( !c_isPrecaching )
				{
					playbackController.ApplyFastForward();
					audioTrigger = "SELECT";
					closeEditMenuIfOpen = !playbackController.IsAtEndOfPlayback();
				}

				break;
			}
		case PLAYBACK_BUTTON_SKIPTOEND:
			{
				if( IsEditing() )
				{
					playbackController.JumpToEnd();
				}
				else
				{
					playbackController.JumpToNextClip();
				}

				audioTrigger = "SELECT";
				closeEditMenuIfOpen = true;
				break;
			}
		case PLAYBACK_BUTTON_PREV_MARKER:
			{
				ms_currentFocusIndex = GetNearestEditMarkerIndex( false );
				JumpToCurrentEditMarker();

				if( IsOnEditMenu() )
				{
					OpenCurrentEditMarkerMenu();
				}
				else
				{
					ActuateCurrentEditMarker();
				}

				audioTrigger = "NAV_LEFT_RIGHT";

				break;
			}
		case PLAYBACK_BUTTON_ADD_MARKER:
			{
				if( CanAddMarkerAtCurrentTime() )
				{
					AddNewEditMarker();
				}
				else
				{
					audioTrigger = JumpToCurrentEditMarker() ? "SELECT" : NULL;
					ActuateCurrentEditMarker();
				}

				break;
			}
		case PLAYBACK_BUTTON_NEXT_MARKER:
			{
				ms_currentFocusIndex = GetNearestEditMarkerIndex( true );
				JumpToCurrentEditMarker();

				if( IsOnEditMenu() )
				{
					OpenCurrentEditMarkerMenu();
				}
				else
				{
					ActuateCurrentEditMarker();
				}

				audioTrigger = "NAV_LEFT_RIGHT";

				break;
			}
		default:
			{
				uiAssertf( false, "CVideoEditorPlayback::ActuateCurrentPlaybackButton - Unknown button %d focused", ms_currentFocusIndex );
				break;
			}
		}

		if( audioTrigger )
		{
			g_FrontendAudioEntity.PlaySound( audioTrigger, "HUD_FRONTEND_DEFAULT_SOUNDSET" );
			ms_refreshEditMarkerState = true;
			ms_refreshInstructionalButtons = true;
		}

		if( IsOnEditMenu() && closeEditMenuIfOpen )
		{
			CloseCurrentMenu();
		}
	}
}

void CVideoEditorPlayback::UpdatePlaybackButtonStates( ePlaybackButtonId const playbackButton, s32 const iMouseEventType )
{
	bool const c_hasFocus = iMouseEventType == EVENT_TYPE_MOUSE_ROLL_OVER;
	bool const c_lostFocus = iMouseEventType == EVENT_TYPE_MOUSE_ROLL_OUT || iMouseEventType == EVENT_TYPE_MOUSE_RELEASE_OUTSIDE;

	if( c_hasFocus || c_lostFocus )
	{
		s32 buttonState = c_hasFocus ? TOGGLE_BUTTON_FOCUS : TOGGLE_BUTTON_NO_FOCUS;
		buttonState = IsOnEditMenu() && playbackButton == PLAYBACK_BUTTON_ADD_MARKER ? 
			( c_hasFocus ? TOGGLE_BUTTON_DISABLED_HOVER : TOGGLE_BUTTON_DISABLED ) : buttonState;

		// Store state then do the scaleform update
		ms_playbackButtonStates[ playbackButton ] = buttonState;
		SetPlaybackButtonState( playbackButton, buttonState );
	}
}

void CVideoEditorPlayback::UpdatePlaybackButtonToggleStates( ePlaybackButtonId const playbackButton, s32 const iMouseEventType )
{
	bool const c_hasFocus = iMouseEventType == EVENT_TYPE_MOUSE_ROLL_OVER;
	bool const c_lostFocus = iMouseEventType == EVENT_TYPE_MOUSE_ROLL_OUT || iMouseEventType == EVENT_TYPE_MOUSE_RELEASE_OUTSIDE;

	if( c_hasFocus || c_lostFocus )
	{
		bool const c_actionApplied = playbackButton == PLAYBACK_BUTTON_REWIND ? CVideoEditorInterface::GetPlaybackController().IsRewinding() : 
			CVideoEditorInterface::GetPlaybackController().IsFastForwarding();

		s32 const buttonState = c_hasFocus ? TOGGLE_BUTTON_FOCUS : ( c_actionApplied ? TOGGLE_BUTTON_TOGGLED : TOGGLE_BUTTON_NO_FOCUS );

		// Store state then do the scaleform update
		ms_playbackButtonStates[ playbackButton ] = buttonState;
		SetPlaybackButtonState( playbackButton, buttonState );
	}
}

void CVideoEditorPlayback::UpdatePlaybackButtonToggleStates()
{
	CVideoProjectPlaybackController const& playbackController = CVideoEditorInterface::GetPlaybackController();

	bool const c_isRewinding = playbackController.IsRewinding() && playbackController.IsPlaying();
	bool const c_isFFwding = playbackController.IsFastForwarding() && playbackController.IsPlaying();

	bool const c_rwdHasFocus = ms_playbackButtonStates[ PLAYBACK_BUTTON_REWIND ] == TOGGLE_BUTTON_FOCUS;
	bool const c_ffwdHasFocus = ms_playbackButtonStates[ PLAYBACK_BUTTON_FASTFORWARD ] == TOGGLE_BUTTON_FOCUS;

	s32 const rwdButtonState =  c_rwdHasFocus ? TOGGLE_BUTTON_FOCUS : ( c_isRewinding ? TOGGLE_BUTTON_TOGGLED : TOGGLE_BUTTON_NO_FOCUS );
	s32 const ffwdButtonState =  c_ffwdHasFocus ? TOGGLE_BUTTON_FOCUS : ( c_isFFwding ? TOGGLE_BUTTON_TOGGLED : TOGGLE_BUTTON_NO_FOCUS );

	SetPlaybackButtonState( PLAYBACK_BUTTON_REWIND, rwdButtonState );
	SetPlaybackButtonState( PLAYBACK_BUTTON_FASTFORWARD, ffwdButtonState );
}

void CVideoEditorPlayback::SetPlaybackButtonState( s32 const button, s32 const stateFrame )
{
	if( IsValidPlaybackButton( button ) && CVideoEditorPlayback::ms_ComplexObject[PLAYBACK_OBJECT_BUTTON_PANEL].IsActive() )
	{
		u32 const c_complexObjectIndex = PLAYBACK_OBJECT_BUTTON_FIRST + button;
		if( c_complexObjectIndex >= PLAYBACK_OBJECT_BUTTON_FIRST && c_complexObjectIndex <= PLAYBACK_OBJECT_BUTTON_LAST )
		{
			CComplexObject& button = ms_ComplexObject[ c_complexObjectIndex ];
			if( button.IsActive() )
			{
				button.GotoFrame( stateFrame );
			}
		}
	}
}

ePlaybackButtonId CVideoEditorPlayback::GetPlaybackButtonId( char const * const buttonName )
{
	ePlaybackButtonId result = PLAYBACK_BUTTON_INVALID;

	for( u32 index = 0; buttonName && index < PLAYBACK_BUTTON_MAX; ++index )
	{
		char const * const c_comparisonString = sc_buttonObjectNames[ index ];

		if( strcmp( buttonName, c_comparisonString ) == 0 )
		{
			result = (ePlaybackButtonId)index;
			break;
		}
	}

	return result;
}

#endif // defined(SHOW_PLAYBACK_BUTTONS) && SHOW_PLAYBACK_BUTTONS

void CVideoEditorPlayback::ActuateCurrentEditMarker()
{
	if( ms_currentFocusIndex != -1 )
	{
		OpenCurrentEditMarkerMenu();
		g_FrontendAudioEntity.PlaySound( "SELECT", "HUD_FRONTEND_DEFAULT_SOUNDSET" );
	}
}

void CVideoEditorPlayback::OpenCurrentEditMarkerMenu()
{
	ms_focusContext = FOCUS_MARKER_MENU;
	ms_refreshInstructionalButtons = true;
	ms_refreshEditMarkerState = true;
	ms_editMenuStackedIndex = -1;

	StopScrubbing();
	JumpToCurrentEditMarker();

	DisableCurrentMenu();
	SetMenuVisible( true );
	EnableEditMarkerMenu();
}

bool CVideoEditorPlayback::JumpToClosestEditMarker()
{
	bool timeChanged = false;

	CVideoProjectPlaybackController& playbackController = CVideoEditorInterface::GetPlaybackController();
	playbackController.Pause();

	sReplayMarkerInfo const* editMarker = GetClosestEditMarker();
	if( editMarker )
	{
		float const& targetTime = editMarker->GetNonDilatedTimeMs();

		if( playbackController.GetPlaybackCurrentNonDilatedTimeMs() != targetTime )
		{
			playbackController.JumpToNonDilatedTimeMs( targetTime, CReplayMgr::JO_FreezeRenderPhase );
			CReplayMarkerContext::SetJumpingOverrideNonDilatedTimeMs( targetTime );
			timeChanged = true;
		}
	}

	return timeChanged;
}

bool CVideoEditorPlayback::JumpToCurrentEditMarker()
{
	bool timeChanged = false;

	CVideoProjectPlaybackController& playbackController = CVideoEditorInterface::GetPlaybackController();
	playbackController.Pause();

	sReplayMarkerInfo const* editMarker = GetCurrentEditMarker();
	if( editMarker )
	{
		float const& targetTime = editMarker->GetNonDilatedTimeMs();

		if( playbackController.GetPlaybackCurrentNonDilatedTimeMs() != targetTime )
		{
			playbackController.JumpToNonDilatedTimeMs( targetTime, CReplayMgr::JO_FreezeRenderPhase );
			CReplayMarkerContext::SetJumpingOverrideNonDilatedTimeMs( targetTime );
			timeChanged = true;
		}
	}

    ms_bBlockDraggingUntilPrecacheComplete = ms_bBlockDraggingUntilPrecacheComplete || timeChanged;
	return timeChanged;
}

void CVideoEditorPlayback::EnableEditMarkerMenu( s32 const startingFocus )
{
	IReplayMarkerStorage const* markerStorage = GetMarkerStorage();
	sReplayMarkerInfo const* marker = GetCurrentEditMarker();
	u32 const currentMarkerIndex = GetCurrentEditMarkerIndex();
	u32 const markerCount = GetEditMarkerCount();

	if( uiVerifyf( marker, "CVideoEditorPlayback::EnableEditMarkerMenu - Entering marker editor with no marker to edit!" ) )
	{
		IReplayMarkerStorage::eEditRestrictionReason const c_typeBasedRestriction = 
			IsOnLastMarker() ? IReplayMarkerStorage::EDIT_RESTRICTION_IS_ENDPOINT : marker->IsAnchor() ? IReplayMarkerStorage::EDIT_RESTRICTION_IS_ANCHOR :
			IReplayMarkerStorage::EDIT_RESTRICTION_NONE;

		bool const c_hasMarkerTypeRestriction = c_typeBasedRestriction != IReplayMarkerStorage::EDIT_RESTRICTION_NONE;

		CVideoEditorPlayback::BeginAddingColumnData( 0, COL_TYPE_LIST, TheText.Get( "VEUI_EDIT_MRK" ) );

		if( CScaleformMgr::BeginMethod(CVideoEditorUi::GetMovieId(), SF_BASE_CLASS_VIDEO_EDITOR, "SET_CLIP_EDIT_HEADER") )
		{
			char const * const locKey = currentMarkerIndex == 0 ? 
				"VEUI_EDIT_MRKS" : currentMarkerIndex == markerCount -1 ? "VEUI_EDIT_MRKE" : "VEUI_EDIT_MRK";

			CScaleformMgr::AddParamString( TheText.Get( locKey ), false );

            SimpleString_32 stringBuffer;
            stringBuffer.sprintf( "%u/%u", currentMarkerIndex + 1, markerCount );
			CScaleformMgr::AddParamString( stringBuffer.getBuffer(), false );

			CScaleformMgr::EndMethod();
		}

		if( CScaleformMgr::BeginMethod(CVideoEditorUi::GetMovieId(), SF_BASE_CLASS_VIDEO_EDITOR, "ADD_COLUMN_ITEM" ) )
		{
			CScaleformMgr::AddParamString( TheText.Get( "VEUI_CAM_OPT" ), false );
			CScaleformMgr::EndMethod();

			IReplayMarkerStorage::eEditRestrictionReason const c_baseCamRestriction = markerStorage->IsMarkerControlEditable( MARKER_CONTROL_CAMERA_TYPE );

			bool const c_hasBaseCamRestriction = c_baseCamRestriction != IReplayMarkerStorage::EDIT_RESTRICTION_NONE;
			bool const c_isFreeCamera = marker->IsFreeCamera();

			IReplayMarkerStorage::eEditRestrictionReason const c_camEditRestriction = c_hasBaseCamRestriction ? c_baseCamRestriction :
				IsOnLastMarker() && !c_isFreeCamera ? IReplayMarkerStorage::EDIT_RESTRICTION_REQUIRES_CAMERA_BLEND :
					IsOnLastMarker() && c_isFreeCamera ? IReplayMarkerStorage::EDIT_RESTRICTION_NONE : c_typeBasedRestriction;

			ms_activeMenuOptions.PushAndGrow( MenuOption( MARKER_MENU_OPTION_CAMERA_MENU, c_camEditRestriction ) );
		}

		if( CScaleformMgr::BeginMethod(CVideoEditorUi::GetMovieId(), SF_BASE_CLASS_VIDEO_EDITOR, "ADD_COLUMN_ITEM") )
		{
			CScaleformMgr::AddParamString( TheText.Get( "VEUI_DOF_OPT" ), false );
			CScaleformMgr::EndMethod();

#if RSG_PC
			bool const c_shouldDisableInGameDof = CPauseMenu::GetMenuPreference(PREF_GFX_DOF) == 0 || CPauseMenu::GetMenuPreference(PREF_GFX_POST_FX) < 2;
#else
			bool const c_shouldDisableInGameDof = (CProfileSettings::GetInstance().Exists(CProfileSettings::DISPLAY_DOF) && 
				(CProfileSettings::GetInstance().GetInt(CProfileSettings::DISPLAY_DOF) == 0));
#endif // RSG_PC

			IReplayMarkerStorage::eEditRestrictionReason const c_dofRestriction = c_hasMarkerTypeRestriction ? c_typeBasedRestriction :
				c_shouldDisableInGameDof ? IReplayMarkerStorage::EDIT_RESTRICTION_DOF_DISABLED : markerStorage->IsMarkerControlEditable( MARKER_CONTROL_DOF );

			ms_activeMenuOptions.PushAndGrow( MenuOption( MARKER_MENU_OPTION_DOF_MENU, c_dofRestriction ) );
		}

		if( CScaleformMgr::BeginMethod(CVideoEditorUi::GetMovieId(), SF_BASE_CLASS_VIDEO_EDITOR, "ADD_COLUMN_ITEM") )
		{
			CScaleformMgr::AddParamString( TheText.Get( "VEUI_PFX_OPT" ), false );

			CScaleformMgr::EndMethod();

			ms_activeMenuOptions.PushAndGrow( MenuOption( MARKER_MENU_OPTION_POSTFX_MENU, 
				c_hasMarkerTypeRestriction ? c_typeBasedRestriction : markerStorage->IsMarkerControlEditable( MARKER_CONTROL_EFFECT ) ) 
				);
		}

		if( CScaleformMgr::BeginMethod(CVideoEditorUi::GetMovieId(), SF_BASE_CLASS_VIDEO_EDITOR, "ADD_COLUMN_ITEM") )
		{
			CScaleformMgr::AddParamString( TheText.Get( "VEUI_AUD_OPT" ), false );
			CScaleformMgr::EndMethod();

			ms_activeMenuOptions.PushAndGrow( 
				MenuOption( MARKER_MENU_OPTION_AUDIO_MENU, c_hasMarkerTypeRestriction ? c_typeBasedRestriction : markerStorage->IsMarkerControlEditable( MARKER_CONTROL_SOUND ) ) 
				);
		}

		if( CScaleformMgr::BeginMethod(CVideoEditorUi::GetMovieId(), SF_BASE_CLASS_VIDEO_EDITOR, "ADD_COLUMN_ITEM_WITH_OPTIONS") )
		{
			CScaleformMgr::AddParamString( TheText.Get( "VEUI_ED_MK_SPEED" ), false );
			CScaleformMgr::AddParamString( TheText.Get( marker->GetMarkerSpeedLngKey() ), false);

			CScaleformMgr::EndMethod();

			ms_activeMenuOptions.PushAndGrow( 
				MenuOption( MARKER_MENU_OPTION_SPEED, c_hasMarkerTypeRestriction ? c_typeBasedRestriction : markerStorage->IsMarkerControlEditable( MARKER_CONTROL_SPEED ) ) 
				);
		}	

		if (CScaleformMgr::BeginMethod(CVideoEditorUi::GetMovieId(), SF_BASE_CLASS_VIDEO_EDITOR, "ADD_COLUMN_ITEM_WITH_OPTIONS"))
		{
			CScaleformMgr::AddParamString( TheText.Get( "VEUI_ED_MK_TIME" ), false );

			float const c_currentTimeMilliseconds	= marker ? markerStorage->GetMarkerDisplayTimeMs( currentMarkerIndex ) : 0;

			SimpleString_32 stringBuffer;
			CTextConversion::FormatMsTimeAsString( stringBuffer.getWritableBuffer(), stringBuffer.GetMaxByteCount(), (u32)c_currentTimeMilliseconds, false );
			CScaleformMgr::AddParamString( stringBuffer.getBuffer(), false);
			CScaleformMgr::EndMethod();

			ms_activeMenuOptions.PushAndGrow( MenuOption( MARKER_MENU_OPTION_TIME, IReplayMarkerStorage::EDIT_RESTRICTION_NONE ) );
		}

		if( CScaleformMgr::BeginMethod(CVideoEditorUi::GetMovieId(), SF_BASE_CLASS_VIDEO_EDITOR, "ADD_COLUMN_ITEM") )
		{
			CScaleformMgr::AddParamString( TheText.Get( "VEUI_CAP_THUMB" ), false );
			CScaleformMgr::EndMethod();

			ms_activeMenuOptions.PushAndGrow( MenuOption( MARKER_MENU_OPTION_SET_THUMBNAIL, IReplayMarkerStorage::EDIT_RESTRICTION_NONE ) );
		}

		if (CScaleformMgr::BeginMethod(CVideoEditorUi::GetMovieId(), SF_BASE_CLASS_VIDEO_EDITOR, "ADD_COLUMN_ITEM_WITH_OPTIONS"))
		{
			if( IsOnTransitionPoint() )
			{
				CScaleformMgr::AddParamString( TheText.Get( "VEUI_TRANS_TOGGLE" ), false );
				CScaleformMgr::AddParamString( TheText.Get( marker->GetTransitionTypeLngKey() ), false);
				ms_activeMenuOptions.PushAndGrow( MenuOption( MARKER_MENU_OPTION_TRANSITION, IReplayMarkerStorage::EDIT_RESTRICTION_NONE, 2 ) );
			}
			else
			{
				CScaleformMgr::AddParamString( TheText.Get( "VEUI_ED_MK_ANCHOR" ), false );
				CScaleformMgr::AddParamString( TheText.Get( marker->GetMarkerTypeLngKey() ), false);
				ms_activeMenuOptions.PushAndGrow( MenuOption( MARKER_MENU_OPTION_TYPE, IReplayMarkerStorage::EDIT_RESTRICTION_NONE, 3 ) );
			}

			CScaleformMgr::EndMethod();
		}

		if (CScaleformMgr::BeginMethod(CVideoEditorUi::GetMovieId(), SF_BASE_CLASS_VIDEO_EDITOR, "ADD_COLUMN_HELP_TEXT"))
		{
			CScaleformMgr::AddParamString( "", false );
			CScaleformMgr::EndMethod();

			ms_activeMenuOptions.PushAndGrow( MenuOption( MARKER_MENU_OPTION_HELP_TEXT, IReplayMarkerStorage::EDIT_RESTRICTION_NONE ) );
		}

		CVideoEditorPlayback::EndAddingColumnData( true, true );
		ms_focusContext = FOCUS_MARKER_MENU;

		s32 const c_menuOptionCount = ms_activeMenuOptions.GetCount() - 1;
		s32 const c_lastValidOptionIndex = c_menuOptionCount >= 1 ? c_menuOptionCount - 1 : 0;

		ms_menuFocusIndex = ms_editMenuStackedIndex > -1 ? ms_editMenuStackedIndex : startingFocus;
		ms_menuFocusIndex = Clamp( ms_menuFocusIndex, 0, c_lastValidOptionIndex );
		ms_editMenuStackedIndex = ms_menuFocusIndex;

		CReplayCoordinator::SetShouldRenderTransitionsInEditMode( false );

		SetCurrentItemIntoCorrectState();
		UpdateEditMenuState();
		UpdateMenuHelpText( ms_menuFocusIndex );
		UpdateAddMarkerButtonState();
		UpdateItemFocusDependentState();
	}
}

void CVideoEditorPlayback::UpdateMarkerMenuTimeValue( u32 const timeMs )
{
	UpdateItemTimeValue( ms_menuFocusIndex, timeMs );
}

void CVideoEditorPlayback::UpdateMenuHelpText( s32 const menuIndexForHelpText )
{
	int const c_menuOptionCount = ms_activeMenuOptions.GetCount();
	if( uiVerifyf( c_menuOptionCount > 0 && ms_menuFocusIndex < c_menuOptionCount,
		"CVideoEditorPlayback::UpdateMenuHelpText - Updating helptext with invalid focus index %d of %d total options"
		, ms_menuFocusIndex, c_menuOptionCount ) )
	{
		const char* helpTextKey = NULL;

		MenuOption const * const c_option = TryGetMenuOption( menuIndexForHelpText );
		eMARKER_MENU_OPTIONS const c_menuOption = c_option ? c_option->m_option : MARKER_MENU_OPTION_MAX; 
		IReplayMarkerStorage::eEditRestrictionReason const c_editRestriction = c_option ? 
			c_option->m_editRestrictions : IReplayMarkerStorage::EDIT_RESTRICTION_NONE;

		bool const c_isEnabled = c_option ? c_option->IsEnabled() : false;
		if( c_isEnabled )
		{
			if( c_menuOption == MARKER_MENU_OPTION_TRANSITION )
			{
				sReplayMarkerInfo* currentMarker = GetCurrentEditMarker();
				helpTextKey = currentMarker ? currentMarker->GetTransitionTypeHelpLngKey() : NULL;
			}
			else if( c_menuOption == MARKER_MENU_OPTION_TYPE )
			{
				sReplayMarkerInfo* currentMarker = GetCurrentEditMarker();
				helpTextKey = currentMarker ? currentMarker->GetMarkerTypeHelpLngKey() : NULL;
			}
			else if( c_menuOption == MARKER_MENU_OPTION_CAMERA_TYPE )
			{
				sReplayMarkerInfo* currentMarker = GetCurrentEditMarker();
				helpTextKey = currentMarker ? currentMarker->GetCameraTypeHelpLngKey() : NULL;
			}
			else if( c_menuOption == MARKER_MENU_OPTION_DOF_MODE )
			{
				sReplayMarkerInfo* currentMarker = GetCurrentEditMarker();
				helpTextKey = currentMarker ? currentMarker->GetDofModeHelpLngKey() : NULL;
			}
			else if( c_menuOption == MARKER_MENU_OPTION_MUSIC_VOL )
			{
				bool const c_projectHasMusic = ms_project ? ms_project->GetMusicClipCount() > 0 : false;
				helpTextKey = c_projectHasMusic ? sc_EditMarkerMenuHelpText[ c_menuOption ] : "VEUI_ED_MK_NOMUS_HLP";
			}
            else if( c_menuOption == MARKER_MENU_OPTION_AMB_VOL )
            {
                bool const c_projectHasMusic = ms_project ? ms_project->GetAmbientClipCount() > 0 : false;
                helpTextKey = c_projectHasMusic ? sc_EditMarkerMenuHelpText[ c_menuOption ] : "VEUI_ED_MK_NOAMB_HLP";
            }
			else if( c_menuOption == MARKER_MENU_OPTION_SCORE_INTENSITY )
			{
				bool const c_projectHasMusic = ms_project ? ms_project->GetMusicClipScoreOnlyCount() > 0 : false;
				helpTextKey = c_projectHasMusic ? sc_EditMarkerMenuHelpText[ c_menuOption ] : "VEUI_ED_MK_NOSCI_HLP";
			}
			else if( c_menuOption == MARKER_MENU_OPTION_MIC_TYPE )
			{
				sReplayMarkerInfo* currentMarker = GetCurrentEditMarker();
				helpTextKey = currentMarker ? currentMarker->GetMarkerMicrophoneTypeHelpLngKey() : NULL;
			}
			else if( c_menuOption == MARKER_MENU_OPTION_CAMERA_ATTACH_TYPE )
			{
				sReplayMarkerInfo* currentMarker = GetCurrentEditMarker();
				helpTextKey = currentMarker ? currentMarker->GetAttachTypeHelpLngKey() : NULL;
			}
			else if( c_menuOption == MARKER_MENU_OPTION_FOCUS_MODE )
			{
				sReplayMarkerInfo* currentMarker = GetCurrentEditMarker();
				helpTextKey = currentMarker ? currentMarker->GetFocusModeHelpLngKey() : NULL;
			}
			else
			{
				helpTextKey = sc_EditMarkerMenuHelpText[ c_menuOption ];
			}
		}
		else if( c_menuOption == MARKER_MENU_OPTION_CAMERA_MENU && IsOnLastMarker() )
		{
			helpTextKey = "VEUI_ENDPOINT_CAM_MNU";
		}
		else if( c_menuOption == MARKER_MENU_OPTION_CAMERA_TYPE && IsOnLastMarker() )
		{
			helpTextKey = "VEUI_ENDPOINT_CAM_TYP";
		}
		else
		{
			helpTextKey = IReplayMarkerStorage::GetEditRestrictionLngKey( c_editRestriction );
		}

		if( helpTextKey )
		{	
			char const * const helpText = TheText.Get( helpTextKey );
			if( helpText &&
				CScaleformMgr::BeginMethod(CVideoEditorUi::GetMovieId(), SF_BASE_CLASS_VIDEO_EDITOR, "UPDATE_COLUMN_HELP_TEXT") )
			{
				CScaleformMgr::AddParamString( helpText, false );
				CScaleformMgr::EndMethod();
			}
		}
	}
}

void CVideoEditorPlayback::DisableCurrentMenu()
{
	DisableMenu();
	ms_activeMenuOptions.Reset();
}

void CVideoEditorPlayback::CloseCurrentMenu()
{
	DisableCurrentMenu();

	ms_focusContext = FOCUS_MARKERS;
	ms_refreshInstructionalButtons = true;
	ms_refreshEditMarkerState = true;

	UpdateAddMarkerButtonState();
}

void CVideoEditorPlayback::UpdateItemFocusDependentState()
{
	MenuOption const * const c_focusedOption = TryGetMenuOption(ms_menuFocusIndex);

	bool const c_forceCamShake( c_focusedOption && ( c_focusedOption->m_option == MARKER_MENU_OPTION_CAMERA_SHAKE ||
		c_focusedOption->m_option == MARKER_MENU_OPTION_CAMERA_SHAKE_INTENSITY || c_focusedOption->m_option == MARKER_MENU_OPTION_CAMERA_SHAKE_SPEED ) );
	CReplayMarkerContext::SetForceCameraShake( c_forceCamShake );

	bool const c_renderTransitions( c_focusedOption && ( c_focusedOption->m_option == MARKER_MENU_OPTION_TRANSITION ) );
	CReplayCoordinator::SetShouldRenderTransitionsInEditMode( c_renderTransitions );

	bool const c_previewSFX( c_focusedOption && ( c_focusedOption->m_option == MARKER_MENU_OPTION_SFX_HASH));
	sReplayMarkerInfo* currentMarker = GetCurrentEditMarker();
	g_ReplayAudioEntity.SetPreviewMarkerSFX( c_previewSFX, currentMarker->GetSfxHash() );
}

void CVideoEditorPlayback::UpdateCameraTargetFocusDependentState()
{
	if( IsEditing() )
	{
		MenuOption const * const c_focusedOption = TryGetMenuOption( ms_menuFocusIndex );

		sReplayMarkerInfo const* c_currentMarker = GetCurrentEditMarker();
		if( c_currentMarker && c_focusedOption )
		{
			s32 const c_targetId = ( c_focusedOption->m_option == MARKER_MENU_OPTION_CAMERA_LOOKAT_TARGET ) ? c_currentMarker->GetLookAtTargetId() : 
				( c_focusedOption->m_option == MARKER_MENU_OPTION_CAMERA_ATTACH_TARGET ) ? c_currentMarker->GetAttachTargetId() : 
				( c_focusedOption->m_option == MARKER_MENU_OPTION_FOCUS_TARGET ) ? c_currentMarker->GetFocusTargetId() : -1;

			if( c_targetId != -1 )
			{
				SetupCameraTargetMarkerForEntity( c_targetId );
			}
		}
	}
}

void CVideoEditorPlayback::BackoutEditMarkerMenu()
{
	ms_editMenuStackedIndex = -1;
	DisableCurrentMenu();
	CVideoEditorUi::GarbageCollect();
	ExitCamEditMode();

	g_FrontendAudioEntity.PlaySound( "BACK", "HUD_FRONTEND_DEFAULT_SOUNDSET" );

	ms_focusContext = FOCUS_MARKERS;
	ms_refreshInstructionalButtons = true;
	ms_refreshEditMarkerState = true;

	UpdateAddMarkerButtonState();
}

float CVideoEditorPlayback::GetPlaybackBarBaseStartPositionX()
{
	return CVideoEditorPlayback::ms_ComplexObject[PLAYBACK_OBJECT_PLAYBACK_BAR_BG].IsActive() ? 
		CVideoEditorPlayback::ms_ComplexObject[PLAYBACK_OBJECT_PLAYBACK_BAR_BG].GetXPositionInPixels() : 72.f;
}

float CVideoEditorPlayback::GetPlaybackBarEffectiveStartPositionX()
{
	return CVideoEditorPlayback::ms_ComplexObject[PLAYBACK_OBJECT_PLAYBACK_BAR].IsActive() ? 
		CVideoEditorPlayback::ms_ComplexObject[PLAYBACK_OBJECT_PLAYBACK_BAR].GetXPositionInPixels() : 72.f;
}

float CVideoEditorPlayback::GetPlaybackBarBaseStartPositionY()
{
	return CVideoEditorPlayback::ms_ComplexObject[PLAYBACK_OBJECT_PLAYBACK_BAR_BG].IsActive() ? 
		CVideoEditorPlayback::ms_ComplexObject[PLAYBACK_OBJECT_PLAYBACK_BAR_BG].GetYPositionInPixels() : 44.f;
}

float CVideoEditorPlayback::GetPlaybackBarMaxWidth()
{
	return CVideoEditorPlayback::ms_ComplexObject[PLAYBACK_OBJECT_PLAYBACK_BAR_BG].IsActive() ? 
		CVideoEditorPlayback::ms_ComplexObject[PLAYBACK_OBJECT_PLAYBACK_BAR_BG].GetWidth() : 1138.f;
}

float CVideoEditorPlayback::GetPlaybackBarEffectiveWidth()
{
	return CVideoEditorPlayback::ms_ComplexObject[PLAYBACK_OBJECT_PLAYBACK_BAR].IsActive() ? 
		CVideoEditorPlayback::ms_ComplexObject[PLAYBACK_OBJECT_PLAYBACK_BAR].GetWidth() : 1138.f;
}

float CVideoEditorPlayback::GetPlaybackBarHeight()
{
	return CVideoEditorPlayback::ms_ComplexObject[PLAYBACK_OBJECT_PLAYBACK_BAR_BG].IsActive() ? 
		CVideoEditorPlayback::ms_ComplexObject[PLAYBACK_OBJECT_PLAYBACK_BAR_BG].GetHeight() : 6.f;
}

float CVideoEditorPlayback::GetPlaybackBarClickableRegionPositionX()
{
	return CVideoEditorPlayback::ms_ComplexObject[PLAYBACK_OBJECT_CLIP_EDIT_TIMELINE_CLICK_REGION].IsActive() ? 
		CVideoEditorPlayback::ms_ComplexObject[PLAYBACK_OBJECT_CLIP_EDIT_TIMELINE_CLICK_REGION].GetXPositionInPixels() : 6.f;
}

float CVideoEditorPlayback::GetPlaybackBarClickableRegionWidth()
{
	return CVideoEditorPlayback::ms_ComplexObject[PLAYBACK_OBJECT_CLIP_EDIT_TIMELINE_CLICK_REGION].IsActive() ? 
		CVideoEditorPlayback::ms_ComplexObject[PLAYBACK_OBJECT_CLIP_EDIT_TIMELINE_CLICK_REGION].GetWidth() : 6.f;
}

float CVideoEditorPlayback::GetEffectiveCurrentClipTimeMs()
{
	return CVideoProjectPlaybackController::GetEffectiveCurrentClipTimeMs();
}

bool CVideoEditorPlayback::IsSpaceAvailableToSaveCurrentProject()
{
	bool const c_result = ms_project ? ms_project->IsSpaceAvailableForSaving() : false;
	return c_result;
}

bool CVideoEditorPlayback::IsSpaceToAddMarker()
{
	bool const c_result = ms_project ? ms_project->IsSpaceToAddMarker() : false;
	return c_result;
}

void CVideoEditorPlayback::BeginCameraShutterEffect(bool bKeepClosed)
{
	CComplexObject& shutterObject = ms_ComplexObject[ PLAYBACK_OBJECT_CAM_SHUTTERS ];
	if( shutterObject.IsActiveAndAttached() )
	{
		SetMenuVisible( false );
		ms_cameraShuttersFrameDelay = CAMERA_SHUTTERS_FRAME_DELAY;
		ms_bKeepCameraShuttersClosed = bKeepClosed;
        g_FrontendAudioEntity.PlaySound( "Camera_Shoot", "Phone_SoundSet_Default" );

		if( CScaleformMgr::BeginMethodOnComplexObject( shutterObject.GetId(), SF_BASE_CLASS_VIDEO_EDITOR, bKeepClosed?"CLOSE_SHUTTER":"CLOSE_THEN_OPEN_SHUTTER" ) )
		{
			CScaleformMgr::EndMethod();
		}
	}
}

void CVideoEditorPlayback::EndCameraShutterEffect()
{
	SetMenuVisible( IsOnEditMenu() );
	ms_cameraShuttersFrameDelay = -1;
}

void CVideoEditorPlayback::OpenCameraShutters()
{
	if (ms_bKeepCameraShuttersClosed)
	{
		ms_bKeepCameraShuttersClosed = false;

		CComplexObject& shutterObject = ms_ComplexObject[ PLAYBACK_OBJECT_CAM_SHUTTERS ];
		if( shutterObject.IsActiveAndAttached() )
		{
			if( CScaleformMgr::BeginMethodOnComplexObject( shutterObject.GetId(), SF_BASE_CLASS_VIDEO_EDITOR, "OPEN_SHUTTER" ) )
			{
				CScaleformMgr::EndMethod();
			}
		}
	}
}


// Taking a Snapmatic Photo in the Editor
void CVideoEditorPlayback::BeginPhotoCreation()
{
	if (ms_CreatePhotoProgress == CREATE_PHOTO_PROGRESS_IDLE)
	{
		ms_CreatePhotoProgress = CREATE_PHOTO_PROGRESS_BEGIN_PHOTO_ENUMERATION;
	}
}

void CVideoEditorPlayback::UpdatePhotoCreation()
{
	bool bReset = false;

	switch (ms_CreatePhotoProgress)
	{
		case CREATE_PHOTO_PROGRESS_BEGIN_PHOTO_ENUMERATION :
			{
				if (CPhotoManager::GetCreateSortedListOfPhotosStatus(true, true) != MEM_CARD_BUSY)
				{
					CPhotoManager::RequestCreateSortedListOfPhotos(true, true);
				}

				ms_CreatePhotoProgress = CREATE_PHOTO_PROGRESS_CHECK_PHOTO_ENUMERATION;
			}
			break;

		case CREATE_PHOTO_PROGRESS_CHECK_PHOTO_ENUMERATION :
			{
				switch (CPhotoManager::GetCreateSortedListOfPhotosStatus(true, true))
				{
					case MEM_CARD_COMPLETE :
                        {
                            uiAssertf(CPhotoManager::IsListOfPhotosUpToDate(true), 
                                "CVideoEditorPlayback::UpdatePhotoCreation - CREATE_PHOTO_PROGRESS_CHECK_FOR_FREE_PHOTO_SLOT - CPhotoManager::IsListOfPhotosUpToDate() returned false");

                            if (CPhotoManager::GetNumberOfPhotos(false) < NUMBER_OF_LOCAL_PHOTOS)
                            {
                                BeginCameraShutterEffect(true);

                                ms_CreatePhotoProgress = CPhotoManager::RequestTakeRockstarEditorPhoto() ? CREATE_PHOTO_PROGRESS_CHECK_TAKE_PHOTO : CREATE_PHOTO_PROGRESS_ERROR;
                                uiAssertf( ms_CreatePhotoProgress == CREATE_PHOTO_PROGRESS_CHECK_TAKE_PHOTO, "CVideoEditorPlayback::UpdatePhotoCreation - "
                                        "CREATE_PHOTO_PROGRESS_BEGIN_TAKE_PHOTO - CPhotoManager::RequestTakeRockstarEditorPhoto() returned false");
                                    bReset = ms_CreatePhotoProgress == CREATE_PHOTO_PROGRESS_ERROR;
                            }
                            else
                            {
                                ms_pendingAction = WARNING_PROMPT_SNAPMATIC_FULL;
                                bReset = true;
                            }
                        }
						break;

					case MEM_CARD_BUSY :
						break;

					case MEM_CARD_ERROR :
						uiAssertf(0, "CVideoEditorPlayback::UpdatePhotoCreation - CREATE_PHOTO_PROGRESS_CHECK_PHOTO_ENUMERATION - "
                            "CPhotoManager::GetCreateSortedListOfPhotosStatus returned MEM_CARD_ERROR");
						bReset = true;
                        ms_CreatePhotoProgress = CREATE_PHOTO_PROGRESS_ERROR;

						break;
				}
			}
			break;

		case CREATE_PHOTO_PROGRESS_CHECK_TAKE_PHOTO :
			{
				switch (CPhotoManager::GetTakeRockstarEditorPhotoStatus())
				{
					case MEM_CARD_COMPLETE :
						OpenCameraShutters();
						
                        ms_CreatePhotoProgress =  uiVerifyf(CPhotoManager::HasRockstarEditorPhotoBufferBeenAllocated(), "CVideoEditorPlayback::UpdatePhotoCreation - "
                            "CREATE_PHOTO_PROGRESS_BEGIN_SAVE_PHOTO - photo buffer isn't allocated" ) && uiVerifyf( CPhotoManager::RequestSaveRockstarEditorPhoto(), 
                            "CVideoEditorPlayback::UpdatePhotoCreation - CREATE_PHOTO_PROGRESS_BEGIN_SAVE_PHOTO - CPhotoManager::RequestSaveRockstarEditorPhoto() returned false" ) ? 
                            CREATE_PHOTO_PROGRESS_CHECK_SAVE_PHOTO : CREATE_PHOTO_PROGRESS_ERROR;

                        bReset = ms_CreatePhotoProgress == CREATE_PHOTO_PROGRESS_ERROR;
                     
						break;

					case MEM_CARD_BUSY :
						break;

					case MEM_CARD_ERROR :
						uiAssertf(0, "CVideoEditorPlayback::UpdatePhotoCreation - CREATE_PHOTO_PROGRESS_CHECK_TAKE_PHOTO - CPhotoManager::GetTakeRockstarEditorPhotoStatus() returned MEM_CARD_ERROR");
						bReset = true;
                        ms_CreatePhotoProgress = CREATE_PHOTO_PROGRESS_ERROR;

						break;
				}
			}
			break;

		case CREATE_PHOTO_PROGRESS_CHECK_SAVE_PHOTO :
			{
				switch ( CPhotoManager::GetSaveRockstarEditorPhotoStatus() )
				{
					case MEM_CARD_COMPLETE :
						bReset = true;
						break;

					case MEM_CARD_BUSY :
						break;

					case MEM_CARD_ERROR :
						uiAssertf(0, "CVideoEditorPlayback::UpdatePhotoCreation - CREATE_PHOTO_PROGRESS_CHECK_SAVE_PHOTO - CPhotoManager::GetSaveRockstarEditorPhotoStatus() returned MEM_CARD_ERROR");
						bReset = true;
                        ms_CreatePhotoProgress = CREATE_PHOTO_PROGRESS_ERROR;

						break;
				}
			}
			break;

        default:
            //	Do nothing
            break;
	}

	if (bReset)
	{
		if( CPhotoManager::HasRockstarEditorPhotoBufferBeenAllocated() )
		{
			CPhotoManager::RequestFreeMemoryForHighQualityRockstarEditorPhoto();
		}

		OpenCameraShutters();
		ms_CreatePhotoProgress = CREATE_PHOTO_PROGRESS_IDLE;
	}
}


void CVideoEditorPlayback::SetupCameraTargetMarkerForEntity( s32 const id )
{
	CEntity const * c_targetEntity = CReplayMgr::GetEntity( id );
	if( c_targetEntity )
	{
		Vector3 position;

		float const c_radius = c_targetEntity->GetBoundCentreAndRadius( position );
		position.z += c_radius + CAMERA_TARGET_ARROW_PADDING;

		SetupCameraTargetMarker( position );
	}
}

void CVideoEditorPlayback::SetupCameraTargetMarker( Vector3 const& position )
{
	MarkerInfo_t markerInfo;
	markerInfo.type		= MARKERTYPE_ARROW;
	markerInfo.vPos		= RCC_VEC3V( position );
	markerInfo.vScale	= Vec3V( CAMERA_TARGET_ARROW_SCALE, CAMERA_TARGET_ARROW_SCALE, -CAMERA_TARGET_ARROW_SCALE );
	markerInfo.col		= CHudColour::GetRGBA( HUD_COLOUR_BLUE );
	markerInfo.faceCam	= true;
	markerInfo.clip		= false;

	g_markers.Register(markerInfo);
}

IReplayMarkerStorage* CVideoEditorPlayback::GetMarkerStorage()
{
	IReplayMarkerStorage* result = ms_project && ms_clipIndexToEdit >= 0 ? ms_project->GetClipMarkers( ms_clipIndexToEdit ) : NULL;
	return result;
}

IReplayMarkerStorage* CVideoEditorPlayback::GetMarkerStorage( u32 const clipIndex )
{
    IReplayMarkerStorage* result = ms_project ? ms_project->GetClipMarkers( clipIndex ) : NULL;
    return result;
}

sReplayMarkerInfo* CVideoEditorPlayback::GetPreviousEditMarker()
{
	IReplayMarkerStorage* markerStorage = GetMarkerStorage();
	sReplayMarkerInfo* result = markerStorage ? markerStorage->TryGetMarker( ms_currentFocusIndex - 1 ) : NULL;
	return result;
}

sReplayMarkerInfo* CVideoEditorPlayback::GetCurrentEditMarker()
{
	IReplayMarkerStorage* markerStorage = GetMarkerStorage();
	sReplayMarkerInfo* result = markerStorage ? markerStorage->TryGetMarker( ms_currentFocusIndex ) : NULL;
	return result;
}

sReplayMarkerInfo* CVideoEditorPlayback::GetClosestEditMarker()
{
    IReplayMarkerStorage* markerStorage = GetMarkerStorage();
    sReplayMarkerInfo* result( NULL );

    if( ms_currentFocusIndex >= 0 )
    {
        result = markerStorage ? markerStorage->TryGetMarker( ms_currentFocusIndex ) : NULL;
    }
    else
    {
        float const c_currentEffectiveTimeMs = GetEffectiveCurrentClipTimeMs();
        result = markerStorage->GetClosestMarker( c_currentEffectiveTimeMs, CLOSEST_MARKER_ANY );
    }

    return result;
}

sReplayMarkerInfo* CVideoEditorPlayback::GetClosestEditMarker( u32 const clipIndex, eClosestMarkerPreference const markerPref )
{
    IReplayMarkerStorage* markerStorage = GetMarkerStorage( clipIndex );

    float const c_currentEffectiveTimeMs = GetEffectiveCurrentClipTimeMs();
    sReplayMarkerInfo* result = markerStorage ? markerStorage->GetClosestMarker( c_currentEffectiveTimeMs, markerPref ) : NULL;

    return result;
}

float CVideoEditorPlayback::GetClosestEditMarkerTimeMs()
{
	sReplayMarkerInfo const* closestEditMarker = GetClosestEditMarker();
	return closestEditMarker ? closestEditMarker->GetNonDilatedTimeMs() : 0;
}

float CVideoEditorPlayback::GetClosestEditMarkerTimeMs( u32 const clipIndex, eClosestMarkerPreference const markerPref )
{
    sReplayMarkerInfo const* closestEditMarker = GetClosestEditMarker( clipIndex, markerPref );
    return closestEditMarker ? closestEditMarker->GetNonDilatedTimeMs() : 0;
}

float CVideoEditorPlayback::GetCurrentEditMarkerTimeMs()
{
	sReplayMarkerInfo const* currentEditMarker = GetCurrentEditMarker();
	return currentEditMarker ? currentEditMarker->GetNonDilatedTimeMs() : 0;
}

s32 CVideoEditorPlayback::GetCurrentEditMarkerIndex()
{
	return ms_currentFocusIndex;
}

s32 CVideoEditorPlayback::GetEditMarkerCount()
{
	IReplayMarkerStorage* markerStorage = ms_project ? ms_project->GetClipMarkers( ms_clipIndexToEdit ) : NULL;
	return markerStorage ? markerStorage->GetMarkerCount() : 0;
}

s32 CVideoEditorPlayback::GetLastMarkerIndex()
{
	return GetEditMarkerCount() - 1;
}

bool CVideoEditorPlayback::CanAddMarkerAtCurrentTime()
{
	bool canAddMarker = false;

	IReplayMarkerStorage* markerStorage = GetMarkerStorage();
	if( IsEditing() && markerStorage && !IsAtMaxMarkerCount() )
	{
		float const c_currentTimeMs = GetEffectiveCurrentClipTimeMs();
		canAddMarker = markerStorage && markerStorage->CanPlaceMarkerAtNonDilatedTimeMs( c_currentTimeMs, false );
	}

	return canAddMarker;
}

bool CVideoEditorPlayback::IsAtMaxMarkerCount()
{
	return GetEditMarkerCount() >= GetMaxMarkerCount();
}

bool CVideoEditorPlayback::CanDeleteMarkerAtCurrentTime()
{
	bool const c_canDeleteMarker( IsEditing() && ms_currentFocusIndex > 0 && ms_currentFocusIndex < GetLastMarkerIndex() );
	return c_canDeleteMarker;
}

s32 CVideoEditorPlayback::GetNearestEditMarkerIndex()
{
	s32 nearestIndex = -1;

	if( IsEditing() )
	{
		float const c_currentTimeMs = GetEffectiveCurrentClipTimeMs();

		IReplayMarkerStorage const* markerStorage = GetMarkerStorage();
		nearestIndex = markerStorage->GetClosestMarkerIndex( c_currentTimeMs, CLOSEST_MARKER_ANY );
	}

	return nearestIndex;
}

s32 CVideoEditorPlayback::GetNearestEditMarkerIndex( bool const greaterThan )
{
	s32 nearestIndex = -1;

	if( IsEditing() )
	{
		float const c_currentTimeMs = GetEffectiveCurrentClipTimeMs();

		IReplayMarkerStorage const* markerStorage = GetMarkerStorage();
		sReplayMarkerInfo const* currentMarker = ( IsOnEditMenu() || IsFocusedOnEditMarker() ) ? GetCurrentEditMarker() : NULL;
		eClosestMarkerPreference const markerPref = greaterThan ? CLOSEST_MARKER_MORE_OR_EQUAL : CLOSEST_MARKER_LESS_OR_EQUAL;

		nearestIndex = markerStorage->GetClosestMarkerIndex( c_currentTimeMs, markerPref, NULL, currentMarker );

		// Handle wrap-around cases
		nearestIndex = nearestIndex != -1 ? nearestIndex : greaterThan ? 0 : GetLastMarkerIndex();
	}

	return nearestIndex;
}

u32 CVideoEditorPlayback::GetMaxMarkerCount()
{
	u32 result = 2;

	if( IsEditing() )
	{
		IReplayMarkerStorage* markerStorage = GetMarkerStorage();
		result = markerStorage ? markerStorage->GetMaxMarkerCount() : 2;
	}
	else if( IsPreviewingProject() )
	{
		result = ms_project->GetClipCount();
	}

	return result;
}

CVideoEditorPlayback::MarkerDisplayObject* CVideoEditorPlayback::GetEditMarkerDisplayObject( s32 const index )
{
	MarkerDisplayObject* result = result = ( index >= 0 && index < ms_ComplexObjectMarkers.GetCount() ) ? 
		&ms_ComplexObjectMarkers[ index ] : NULL;

	return result;
}

CComplexObject* CVideoEditorPlayback::GetTimeDilationComplexObject( s32 const index )
{
	MarkerDisplayObject* displayObject = GetEditMarkerDisplayObject( index );

	CComplexObject* result = displayObject ? &displayObject->m_timeDilationComplexObject : NULL;
	return result;
}

CComplexObject* CVideoEditorPlayback::GetEditMarkerComplexObject( s32 const index )
{
	MarkerDisplayObject* displayObject = GetEditMarkerDisplayObject( index );

	CComplexObject* result = displayObject ? &displayObject->m_markerComplexObject : NULL;
	return result;
}

CComplexObject* CVideoEditorPlayback::GetCurrentEditMarkerComplexObject()
{
	s32 const c_markerIndex = GetCurrentEditMarkerIndex();
	return GetEditMarkerComplexObject( c_markerIndex );
}

void CVideoEditorPlayback::BeginAddingColumnData(s32 iColumnId, s32 iColumnType, const char *pHeaderText)
{
	if (CScaleformMgr::BeginMethod(CVideoEditorUi::GetMovieId(), SF_BASE_CLASS_VIDEO_EDITOR, "BEGIN_ADDING_COLUMN_DATA"))
	{
		CScaleformMgr::AddParamInt(iColumnId);
		CScaleformMgr::AddParamInt(iColumnType);
		CScaleformMgr::AddParamString(pHeaderText);
		CScaleformMgr::EndMethod();
	}
}

void CVideoEditorPlayback::EndAddingColumnData(bool bShouldRender, bool bSetActive)
{
	if (CScaleformMgr::BeginMethod(CVideoEditorUi::GetMovieId(), SF_BASE_CLASS_VIDEO_EDITOR, "END_ADDING_COLUMN_DATA"))
	{
		CScaleformMgr::AddParamBool(bShouldRender);
		CScaleformMgr::AddParamBool(bSetActive);
		CScaleformMgr::EndMethod();
	}
}

void CVideoEditorPlayback::RemoveColumn(s32 iColumnToClear)
{
	uiDisplayf("Removing Column %d", iColumnToClear);

	if (CScaleformMgr::BeginMethod(CVideoEditorUi::GetMovieId(), SF_BASE_CLASS_VIDEO_EDITOR, "REMOVE_COLUMN"))
	{
		CScaleformMgr::AddParamInt(iColumnToClear);
		CScaleformMgr::EndMethod();
	}
}

void CVideoEditorPlayback::UnsetAllItemsHovered()
{
	if (CScaleformMgr::BeginMethod(CVideoEditorUi::GetMovieId(), SF_BASE_CLASS_VIDEO_EDITOR, "UNHIGHLIGHT"))
	{
		CScaleformMgr::EndMethod();
	}
}

void CVideoEditorPlayback::SetItemActiveDisable( s32 const index )
{
	if( CScaleformMgr::BeginMethod(CVideoEditorUi::GetMovieId(), SF_BASE_CLASS_VIDEO_EDITOR, "SET_ITEMS_GREYED_OUT") )
	{
		CScaleformMgr::AddParamInt( (s32)index );
		CScaleformMgr::EndMethod();
	}
}

void CVideoEditorPlayback::SetItemSelected( s32 const itemIndex, bool const disableArrows )
{
	UnsetAllItemsHovered();

	if( CScaleformMgr::BeginMethod(CVideoEditorUi::GetMovieId(), SF_BASE_CLASS_VIDEO_EDITOR, "SET_ITEM_SELECTED" ) )
	{
		CScaleformMgr::AddParamInt( itemIndex );
		CScaleformMgr::AddParamBool( disableArrows );
		CScaleformMgr::EndMethod();
	}
}

void CVideoEditorPlayback::SetItemHovered( s32 const itemIndex )
{
	UnsetAllItemsHovered();

	if( CScaleformMgr::BeginMethod(CVideoEditorUi::GetMovieId(), SF_BASE_CLASS_VIDEO_EDITOR, "SET_ITEM_ACTIVE" ) )
	{
		CScaleformMgr::AddParamInt( 0 );
		CScaleformMgr::AddParamInt( itemIndex );
		CScaleformMgr::EndMethod();
	}
}

void CVideoEditorPlayback::SetCurrentItemIntoCorrectState()
{
	MenuOption const * const c_activeOption = TryGetMenuOption( ms_menuFocusIndex );
	bool const c_isDisabled = !c_activeOption || !c_activeOption->IsEnabled() ||  ( c_activeOption->IsToggle() && !c_activeOption->HasMoreThanOneToggleValue() );
	SetItemSelected( ms_menuFocusIndex, c_isDisabled );
}

void CVideoEditorPlayback::UpdateItemTextValue( s32 const itemIndex, char const * const localizedString )
{
	if( CScaleformMgr::BeginMethod(CVideoEditorUi::GetMovieId(), SF_BASE_CLASS_VIDEO_EDITOR, "UPDATE_LIST_ITEM_ELEMENT"))
	{
		CScaleformMgr::AddParamInt( 0 );
		CScaleformMgr::AddParamInt( itemIndex );
		CScaleformMgr::AddParamInt( 1 );
		CScaleformMgr::AddParamString( localizedString ? localizedString : "", false );

		CScaleformMgr::EndMethod();
	}
}

void CVideoEditorPlayback::UpdateItemNumberValue( s32 const itemIndex, u32 const number )
{
    SimpleString_32 stringBuffer;
    stringBuffer.sprintf( "%u", number );
	UpdateItemTextValue( itemIndex, stringBuffer.getBuffer() );
}

void CVideoEditorPlayback::UpdateItemPercentValue( s32 const itemIndex, s32 const percent )
{
    SimpleString_32 stringBuffer;
    stringBuffer.sprintf( "%3d%%", percent );
	UpdateItemTextValue( itemIndex, stringBuffer.getBuffer() );
}

void CVideoEditorPlayback::UpdateItemValueFloat( s32 const itemIndex, float const floatValue )
{
    SimpleString_32 stringBuffer;
    stringBuffer.sprintf( "%1.2f", floatValue );
	UpdateItemTextValue( itemIndex, stringBuffer.getBuffer() );
}

void CVideoEditorPlayback::UpdateItemPercentValueFloat( s32 const itemIndex, float const percent )
{
    SimpleString_32 stringBuffer;
    stringBuffer.sprintf( "%3.0f%%", percent );
	UpdateItemTextValue( itemIndex, stringBuffer.getBuffer() );
}

void CVideoEditorPlayback::UpdateItemFxValue( s32 const itemIndex, s32 const fxVal )
{
    SimpleString_32 stringBuffer;
    stringBuffer.sprintf( "%3d", fxVal );
	UpdateItemTextValue( itemIndex, stringBuffer.getBuffer() );
}

void CVideoEditorPlayback::UpdateItemFxValueFloat( s32 const itemIndex, float const fxVal )
{
    SimpleString_32 stringBuffer;
    stringBuffer.sprintf( "%3.0f", fxVal );
	UpdateItemTextValue( itemIndex, stringBuffer.getBuffer() );
}

void CVideoEditorPlayback::UpdateItemMetresValueFloat( s32 const itemIndex, float const metres )
{
    SimpleString_32 stringBuffer;
    stringBuffer.sprintf( "%.1fm", metres );
	UpdateItemTextValue( itemIndex, stringBuffer.getBuffer() );
}

void CVideoEditorPlayback::UpdateItemTimeValue( s32 const itemIndex, u32 const timeMs )
{
	SimpleString_32 stringBuffer;
	CTextConversion::FormatMsTimeAsString( stringBuffer.getWritableBuffer(), stringBuffer.GetMaxByteCount(), timeMs, false );
	UpdateItemTextValue( itemIndex, stringBuffer.getBuffer() );
}

void CVideoEditorPlayback::DisableMenu()
{
	PostFX::SetUpdateReplayEffectsWhilePaused( false );
	CVideoEditorPlayback::RemoveColumn(0);
}

void CVideoEditorPlayback::SetMenuVisible( bool const visible )
{
	if( CScaleformMgr::BeginMethod(CVideoEditorUi::GetMovieId(), SF_BASE_CLASS_VIDEO_EDITOR, "SET_PLAYBACK_MENU_VISIBLE"))
	{
		CScaleformMgr::AddParamBool( visible );
		CScaleformMgr::EndMethod();
	}
}

void CVideoEditorPlayback::CleanupTimeDilationElement( CComplexObject& dilationObject )
{
	if( dilationObject.IsActive() && 
		CScaleformMgr::BeginMethodOnComplexObject( dilationObject.GetId(), SF_BASE_CLASS_VIDEO_EDITOR, "DISPOSE" ) )
	{
		CScaleformMgr::EndMethod();
	}
}

void CVideoEditorPlayback::SetTransitionElementVisible( CComplexObject& markerObject, bool const isVisible )
{
	if( CScaleformMgr::BeginMethodOnComplexObject( markerObject.GetId(), SF_BASE_CLASS_VIDEO_EDITOR, 
		"SET_FILTER_INDICATOR_VISIBILITY" ) )
	{
		CScaleformMgr::AddParamBool( isVisible );
		CScaleformMgr::EndMethod();
	}
}

void CVideoEditorPlayback::SetupPlaybackButtonState( CComplexObject& playbackPanel, bool const isEditing )
{
	if( playbackPanel.IsActive() && 
		CScaleformMgr::BeginMethodOnComplexObject( playbackPanel.GetId(), SF_BASE_CLASS_VIDEO_EDITOR, "SET_PLAYBACK_BUTTONS_MODE" ) )
	{
		CScaleformMgr::AddParamBool( isEditing );
		CScaleformMgr::EndMethod();
	}
}

void CVideoEditorPlayback::SetWarningText( char const * const lngKey )
{
	if( ms_ComplexObject[PLAYBACK_OBJECT_CLIP_EDIT_HELPTEXT].IsActive() )
	{
		CComplexObject helpText = ms_ComplexObject[PLAYBACK_OBJECT_CLIP_EDIT_HELPTEXT].GetObject("helpTextField");
		if (helpText.IsActive())
		{
			helpText.SetTextField( TheText.Get( lngKey ) );
			helpText.Release();
		}
	}
}

void CVideoEditorPlayback::ClearWarningText()
{
	if( ms_ComplexObject[PLAYBACK_OBJECT_CLIP_EDIT_HELPTEXT].IsActive() )
	{
		CComplexObject helpText = ms_ComplexObject[PLAYBACK_OBJECT_CLIP_EDIT_HELPTEXT].GetObject("helpTextField");
		if (helpText.IsActive())
		{
			helpText.SetTextField( "" );
			helpText.Release();
		}
	}
}

void CVideoEditorPlayback::SetCameraOverlayVisible( bool const visible )
{
	if( ms_ComplexObject[PLAYBACK_OBJECT_CLIP_EDIT_CAM_OVERLAY].IsActive() )
	{
		ms_ComplexObject[PLAYBACK_OBJECT_CLIP_EDIT_CAM_OVERLAY].SetVisible( visible );
	}
}

void CVideoEditorPlayback::UpdateCameraOverlay()
{
	CComplexObject& camOverlay = ms_ComplexObject[PLAYBACK_OBJECT_CLIP_EDIT_CAM_OVERLAY];
	if( camOverlay.IsActive() )
	{
		camOverlay.SetPosition( Vector2( 0.5f, 0.5f ) );
		camReplayDirector& replayDirector = camInterface::GetReplayDirector();

		CComplexObject zoomIndicators = camOverlay.GetObject( "ZOOM_INDICATORS" );
		if( zoomIndicators.IsActive() )
		{
			float defaultFov;
			float minFov;
			float maxFov;
			GetDefaultAndMinAndMaxFovForCurrentCamera( defaultFov, minFov, maxFov );

			float const c_fovRange = maxFov - minFov;
			float const c_fovCurrentActual = replayDirector.GetFovOfRenderedFrame();
			float const c_fovCurrent = c_fovCurrentActual - minFov;

			float const c_fovValuePercent = ( 1.f / c_fovRange ) * c_fovCurrent;
			float const c_scaleValue = 135.f - ( c_fovValuePercent * 70.f ); // range of 65% to 135% scale

			zoomIndicators.SetScale( Vector2( c_scaleValue, c_scaleValue ) );

			bool const c_isZooming = ms_cameraEditFovLastFrame != c_fovCurrent;
			u8 const c_zoomAlphaValue = c_isZooming ? 255 : 77;
			zoomIndicators.SetAlpha( c_zoomAlphaValue );
			ms_cameraEditFovLastFrame = c_fovCurrent;

			zoomIndicators.Release();

			CComplexObject centreBox = camOverlay.GetObject( "CENTRE" );
			if( centreBox.IsActive() )
			{
				centreBox.SetAlpha( c_zoomAlphaValue );
				centreBox.Release();
			}

			CComplexObject textBox = camOverlay.GetObject( "TEXT_BOX" );
			if( textBox.IsActive() )
			{
				float const c_zoomRatio = defaultFov / c_fovCurrentActual;
                SimpleString_32 textLabel;
                textLabel.sprintf( "%1.2fX %s", c_zoomRatio, TheText.Get( "VEUI_CAM_OVERLAY_ZOOM" ) );

				CComplexObject zoomTextField = textBox.GetObject( "ZOOM_TF" );
				if( zoomTextField.IsActive() )
				{	
					zoomTextField.SetTextField( textLabel.getBuffer() );
					zoomTextField.Release();
				}

				CComplexObject zoomShadowTextField = textBox.GetObject( "ZOOM_TF_DROPSHADOW" );
				if( zoomShadowTextField.IsActive() )
				{
					zoomShadowTextField.SetTextField( c_isZooming ? textLabel.getBuffer() : "" );
					zoomShadowTextField.SetVisible( c_isZooming );
					zoomShadowTextField.Release();
				}

				textBox.Release();
			}
		}

		CComplexObject rollOverlay = camOverlay.GetObject( "ROTATION_INDICATOR" );
		if( rollOverlay.IsActive() )
		{
			bool const c_canRoll = CanRollCurrentCamera();
			rollOverlay.SetVisible( c_canRoll );

			// Set the alpha even if invisible so we never get a "pop" of the wrong frame when we enter camera edit mode
			s32 const c_currentRoll = s32( camInterface::GetRoll() * RtoD );

			const CControl& control	= CControlMgr::GetMainFrontendControl(false);

			bool const c_isRollInputHeld = control.GetScriptPadLeft().IsDown() || control.GetScriptPadRight().IsDown();
			bool const c_isRolling = c_canRoll && c_isRollInputHeld;

			u8 const c_rollAlphaValue = c_isRolling ? 255 : 51;
			rollOverlay.SetAlpha( c_rollAlphaValue );

			if( c_canRoll )
			{
				CComplexObject rotationArrow = rollOverlay.GetObject( "ROTATION_ARROW" );
				if( rotationArrow.IsActive() )
				{
					rotationArrow.SetRotation( -(float)c_currentRoll );
					rotationArrow.Release();
				}
			}

			rollOverlay.Release();
		}
	}
}

bool CVideoEditorPlayback::CanRollCurrentCamera()
{
	sReplayMarkerInfo const* currentMarker = GetCurrentEditMarker();
	bool const c_canRoll = IsEditingCamera() && currentMarker && currentMarker->GetCameraType() != RPLYCAM_OVERHEAD;
	return c_canRoll;
}

void CVideoEditorPlayback::GetDefaultAndMinAndMaxFovForCurrentCamera( float& out_defaultFov, float& out_minFov, float& out_maxFov )
{
	const camBaseCamera* dominantRenderedCamera = camInterface::GetDominantRenderedCamera();
	out_minFov = 5.0f; 
	out_defaultFov = 45.f;
	out_maxFov = 100.0f;

	if (dominantRenderedCamera && dominantRenderedCamera->GetIsClassId(camReplayFreeCamera::GetStaticClassId()))
	{
		static_cast<const camReplayFreeCamera*>(dominantRenderedCamera)->GetMinMaxFov( out_minFov, out_maxFov );
		static_cast<const camReplayFreeCamera*>(dominantRenderedCamera)->GetDefaultFov( out_defaultFov );
	}
	else if (dominantRenderedCamera && dominantRenderedCamera->GetIsClassId(camReplayPresetCamera::GetStaticClassId()))
	{
		static_cast<const camReplayPresetCamera*>(dominantRenderedCamera)->GetMinMaxFov( out_minFov, out_maxFov );
		static_cast<const camReplayPresetCamera*>(dominantRenderedCamera)->GetDefaultFov( out_defaultFov );
	}

}

bool CVideoEditorPlayback::CanSkipToBeat()
{
	CVideoProjectPlaybackController const& playbackController = CVideoEditorInterface::GetPlaybackController();
	return playbackController.CanJumpToBeatInCurrentClip();
}

bool CVideoEditorPlayback::CanSkipMarkerToBeat( s32 const markerIndex )
{
    IReplayMarkerStorage const * const c_markerStorage = GetMarkerStorage();
    sReplayMarkerInfo const * const c_marker = c_markerStorage ? c_markerStorage->TryGetMarker( markerIndex ) : NULL;

    CVideoProjectPlaybackController const& playbackController = CVideoEditorInterface::GetPlaybackController();
    float const c_currentMarkerTimeMs = c_marker ? c_marker->GetNonDilatedTimeMs() : -1.f;
    float const c_prevBeatTimeMs = c_marker ? playbackController.CalculatePrevAudioBeatNonDilatedTimeMs( &c_currentMarkerTimeMs ) : -1.f;
    float const c_nextBeatTimeMs = c_marker ? playbackController.CalculateNextAudioBeatNonDilatedTimeMs( &c_currentMarkerTimeMs ) : -1.f;
   
	bool const c_result = markerIndex > 0 && markerIndex < GetLastMarkerIndex() && CanSkipToBeat() && 
        ( ( c_prevBeatTimeMs >= 0.f && c_prevBeatTimeMs < c_currentMarkerTimeMs ) || 
        ( c_nextBeatTimeMs >= 0.f && c_nextBeatTimeMs > c_currentMarkerTimeMs ) );

	return c_result;
}

void CVideoEditorPlayback::SetupLoadingScreen()
{
	if( !ms_bLoadingScreen )
	{
		if( IsBaking() )
		{
			WatermarkRenderer::PauseWatermark();
		}

		CAncillaryReplayRenderer::DisableReplayDefaultEffects();

		ms_bWasScrubbingOnLoad = IsScrubbing() || CControlMgr::GetMainFrontendControl().GetFrontendRStickLeftRight().GetNorm() != 0.f;
		ResetScrubbingParams();
		CVideoEditorInstructionalButtons::Clear();  // clear any instructional buttons

		bool const c_isFinalizing = CReplayCoordinator::IsPendingVideoFinalization();
		char const * const c_lngKey = c_isFinalizing ? "VEUI_VID_FINALIZING" : "VEUI_LOADING_CLIP";
		CBusySpinner::On( TheText.Get( c_lngKey ), BUSYSPINNER_LOADING, SPINNER_SOURCE_VIDEO_EDITOR );
        ms_bLoadingText = !c_isFinalizing;
        ms_refreshInstructionalButtons = true;

		// Turn on game tips
		if(CGameStreamMgr::GetGameStream())
		{
			CGameStreamMgr::GetGameStream()->ForceRenderOn();
			CGameStreamMgr::GetGameStream()->AutoPostGameTipOn(CGameStream::TIP_TYPE_REPLAY);
		}

		ms_bLoadingScreen = true;
		CGtaOldLoadingScreen::ForceLoadingRenderFunction(true);
	}
#if USE_SRLS_IN_REPLAY
	else if( ms_bLoadingText )
	{
		// display "processing scene" spinner with text
		CBusySpinner::On(TheText.Get("VEUI_PROC_CLIP"), BUSYSPINNER_LOADING, SPINNER_SOURCE_VIDEO_EDITOR );  
		ms_bLoadingText = false;
	}
#endif
}

void CVideoEditorPlayback::CleanupLoadingScreen( bool const isExiting )
{
	if( ms_bLoadingScreen )
	{
		CGtaOldLoadingScreen::ForceLoadingRenderFunction(false);
		CAncillaryReplayRenderer::EnableReplayDefaultEffects();

		CBusySpinner::Off(SPINNER_SOURCE_VIDEO_EDITOR);  // turn off the loading spinner

		// Turn off game tips
		if(CGameStreamMgr::GetGameStream())
		{
			CGameStreamMgr::GetGameStream()->ForceRenderOff();
			CGameStreamMgr::GetGameStream()->AutoPostGameTipOff();
		}

		ms_bLoadingScreen = false;
		ms_refreshInstructionalButtons = true;

		if( !isExiting )
		{
			if( IsEditing() )
			{
				ms_refreshEditMarkerState = true;
				ms_currentFocusIndex = 0;

				if( IsFirstEntryToEditMode() )
				{
					ms_tutorialCountdownMs = CVideoEditorInterface::GetTutorialDurationMs( CVideoEditorInterface::EDITOR_TUTORIAL_MARKER_EXTENTS );
					CHelpMessage::SetMessageText( HELP_TEXT_SLOT_STANDARD, TheText.Get( "VEUI_TUT_EXTENTS" ), NULL, 0, NULL, 0, true, false, ms_tutorialCountdownMs );
					SetFirstEntryEditModeTutorialSeen();
				}
				else
				{ 
					OpenCurrentEditMarkerMenu();
				}
			}
			else if( IsBaking() )
			{
				WatermarkRenderer::PlayWatermark();
			}

			if( ms_bWasScrubbingOnLoad )
			{
				CVideoEditorInterface::GetPlaybackController().SetCursorSpeed
					( CControlMgr::GetMainFrontendControl().GetFrontendRStickLeftRight().GetNorm() );		
			}
		}

        if( ms_bDelayedBackOut )
        {
            HandleTopLevelBackOut();
        }
	}
}

void CVideoEditorPlayback::UpdateAddMarkerButtonState()
{

#if defined(SHOW_PLAYBACK_BUTTONS) && SHOW_PLAYBACK_BUTTONS

	bool const c_isHoveringAddMarkerButton = ms_playbackButtonStates[PLAYBACK_BUTTON_ADD_MARKER] == TOGGLE_BUTTON_FOCUS || 
		ms_playbackButtonStates[PLAYBACK_BUTTON_ADD_MARKER] == TOGGLE_BUTTON_DISABLED_HOVER;

	bool const c_isOnEditMenu = IsOnEditMenu();
	bool const c_canAddMarker = !c_isOnEditMenu && CanAddMarkerAtCurrentTime();

	s32 const c_newState = c_isHoveringAddMarkerButton ?
		( c_canAddMarker ? TOGGLE_BUTTON_FOCUS : TOGGLE_BUTTON_DISABLED_HOVER ) :
		(  c_canAddMarker ? TOGGLE_BUTTON_NO_FOCUS : TOGGLE_BUTTON_DISABLED );

	SetPlaybackButtonState( PLAYBACK_BUTTON_ADD_MARKER, c_newState );

#endif // defined(SHOW_PLAYBACK_BUTTONS) && SHOW_PLAYBACK_BUTTONS

}

void CVideoEditorPlayback::UpdateInstructionalButtons( CScaleformMovieWrapper& buttonsMovie )
{
    if ( IsLoading() )
    {
        UpdateInstructionalButtonsLoading( buttonsMovie );
    }
    else if( ms_bReadyToPlay )
	{
		if( IsBaking() )
		{
			UpdateInstructionalButtonsBaking( buttonsMovie );
		}
		else if( IsEditing() )
		{
			UpdateInstructionalButtonsEditing( buttonsMovie );
		}
		else
		{
			UpdateInstructionalButtonsPreview( buttonsMovie );
		}
	}
}

#endif // #if defined( GTA_REPLAY ) && GTA_REPLAY
