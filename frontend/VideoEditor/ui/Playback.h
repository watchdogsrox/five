/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : Playback.h
// PURPOSE : the NG video editor playback ui system
// AUTHOR  : Derek Payne
// STARTED : 27/02/2014
//
/////////////////////////////////////////////////////////////////////////////////
#include "control/replay/ReplaySettings.h"
#include "replaycoordinator/ReplayEditorParameters.h"

#if defined( GTA_REPLAY ) && GTA_REPLAY

#ifndef VIDEO_EDITOR_PLAYBACK_H
#define VIDEO_EDITOR_PLAYBACK_H

#include "control/replay/IReplayMarkerStorage.h"
#include "control/replay/ReplayMarkerContext.h"
#include "frontend/VideoEditor/Core/VideoEditorProject.h"
#include "frontend/VideoEditor/ui/Editor.h"
#include "frontend/Scaleform/ScaleFormMgr.h"
#include "Frontend/Scaleform/ScaleFormComplexObjectMgr.h"

#if RSG_PC
enum  // list of items that are clickable by the mouse in the playback mode
{
	CLICKABLE_ITEM_TIMELINE = 0,
	MAX_PLAYBACK_CLICKABLE_ITEMS
};
#endif  // #if RSG_PC

enum ePLAYBACK_TYPE
{
	PLAYBACK_TYPE_PREVIEW_FULL_PROJECT = 0,
	PLAYBACK_TYPE_BAKE,
	PLAYBACK_TYPE_EDIT_CLIP,
	PLAYBACK_TYPE_RAW_CLIP_PREVIEW,
};

enum ePLAYBACK_STATE
{
	PLAYBACK_STATE_STOP = 0,
	PLAYBACK_STATE_PLAY,
	PLAYBACK_STATE_PAUSE,
	PLAYBACK_STATE_REWIND,
	PLAYBACK_STATE_FASTFORWARD
};

enum
{
	PLAYBACK_OBJECT_STAGE_ROOT = 0,
	PLAYBACK_OBJECT_PLAYBACK_PANEL,
	PLAYBACK_OBJECT_PLAYBACK_BAR_BG,
	PLAYBACK_OBJECT_PLAYBACK_BAR,
	PLAYBACK_OBJECT_PLAYBACK_BAR_LIGHT,
	PLAYBACK_OBJECT_PLAYBACK_BAR_SCRUBBER,
	PLAYBACK_OBJECT_PLAYBACK_BG,

	PLAYBACK_OBJECT_BUTTON_PANEL,

	PLAYBACK_OBJECT_BUTTON_FIRST,

	PLAYBACK_OBJECT_BUTTON_SKIPTOSTART = PLAYBACK_OBJECT_BUTTON_FIRST,
	PLAYBACK_OBJECT_BUTTON_REWIND,
	PLAYBACK_OBJECT_BUTTON_PLAYPAUSE,
	PLAYBACK_OBJECT_BUTTON_FASTFORWARD,
	PLAYBACK_OBJECT_BUTTON_SKIPTOEND,

	PLAYBACK_OBJECT_BUTTON_PREV_MARKER,
	PLAYBACK_OBJECT_BUTTON_ADD_MARKER,
	PLAYBACK_OBJECT_BUTTON_NEXT_MARKER,

	PLAYBACK_OBJECT_BUTTON_LAST = PLAYBACK_OBJECT_BUTTON_NEXT_MARKER, 

	PLAYBACK_OBJECT_CLIP_EDIT_CAM_OVERLAY,
	PLAYBACK_OBJECT_CLIP_EDIT_HELPTEXT,
	PLAYBACK_OBJECT_CLIP_EDIT_TIMELINE_CLICK_REGION,

	PLAYBACK_OBJECT_CAM_SHUTTERS,

	MAX_PLAYBACK_COMPLEX_OBJECTS
};

enum ePlaybackButtonId
{
	PLAYBACK_BUTTON_INVALID = -1,
	PLAYBACK_BUTTON_FIRST = 0,

	PLAYBACK_BUTTON_SKIPTOSTART = PLAYBACK_BUTTON_FIRST,
	PLAYBACK_BUTTON_REWIND,
	PLAYBACK_BUTTON_PLAYPAUSE,
	PLAYBACK_BUTTON_FASTFORWARD,
	PLAYBACK_BUTTON_SKIPTOEND,

	PLAYBACK_BUTTON_PREV_MARKER,
	PLAYBACK_BUTTON_ADD_MARKER,
	PLAYBACK_BUTTON_NEXT_MARKER,

	PLAYBACK_BUTTON_MAX
};

class CVideoEditorProject;
class IReplayMarkerStorage;
struct sReplayMarkerInfo;

#define SHOW_PLAYBACK_BUTTONS ( RSG_PC )

class CVideoEditorPlayback
{
public: // methods

	static void Open(ePLAYBACK_TYPE playbackType, s32 const clipIndex = -1 );
	static void Close();

	static void Update();
	
	static void Render();

	static bool IsActive()						{ return ms_bActive; }
	static bool IsBaking()						{ return ms_playbackType == PLAYBACK_TYPE_BAKE; }
	static bool IsEditing()						{ return ms_playbackType == PLAYBACK_TYPE_EDIT_CLIP && ms_project != NULL; }
	static bool IsViewingRawClip()				{ return ms_playbackType == PLAYBACK_TYPE_RAW_CLIP_PREVIEW && ms_project != NULL; }
	static bool IsPreviewingProject()			{ return ms_playbackType == PLAYBACK_TYPE_PREVIEW_FULL_PROJECT && ms_project != NULL; }
	static bool IsLoading()						{ return ms_bLoadingScreen; }
	static bool IsScrubbing()					{ return ms_isScrubbing; }
    static bool IsTakingSnapmatic()             { return ms_CreatePhotoProgress != CREATE_PHOTO_PROGRESS_IDLE; }
    static bool RenderSnapmaticSpinner()        { return ms_CreatePhotoProgress == CREATE_PHOTO_PROGRESS_CHECK_SAVE_PHOTO; }
	static bool IsDisplayingCameraShutters()	{ return ms_cameraShuttersFrameDelay > 0; }

	static char const * GetMarkerTypeName( u32 const index );

	static bool ShouldRender() { return ms_shouldRender && !ms_bLoadingScreen; }
    static bool ShouldRenderIBDuringLoading() { return IsLoading(); }

#if RSG_PC
	static void UpdateMouseEvent(const GFxValue* args);
#endif // #if RSG_PC


private: // declarations and variables

	enum eWARNING_PROMPT_ACTION
	{
		WARNING_PROMPT_NONE,
		WARNING_PROMPT_EDIT_BACK,				// Pending backing out without edits
		WARNING_PROMPT_EDIT_DISCARD_BACK,		// Pending an edit discard, moving back to the main editor
		WARNING_PROMPT_EDIT_DISCARD_PREV_CLIP,	// Pending an edit discard, moving to the previous clip
		WARNING_PROMPT_EDIT_DISCARD_NEXT_CLIP,	// Pending an edit discard, moving to the next clip
        WARNING_PROMPT_SNAPMATIC_FULL,          // Snapmatic gallery is full
        WARNING_PROMPT_NO_SPACE_FOR_MARKERS,    // Not enough disk space to save this project if you add a marker
	};

	enum eFOCUS_CONTEXT
	{
		FOCUS_NONE,
		FOCUS_MARKERS,
		FOCUS_MARKER_MENU,
		FOCUS_POSTFX_MENU,
		FOCUS_CAMERA_MENU,
		FOCUS_DOF_MENU,
		FOCUS_AUDIO_MENU,
		FOCUS_EDITING_CAMERA,
	};

	enum eTOGGLE_BUTTON_FRAMES
	{
		TOGGLE_BUTTON_NO_FOCUS = 1,
		TOGGLE_BUTTON_FOCUS = 2,
		TOGGLE_BUTTON_TOGGLED = 3,
		TOGGLE_BUTTON_DISABLED = 4,
		TOGGLE_BUTTON_DISABLED_HOVER = 5
	};

	enum eEDIT_MARKER_FOCUS
	{
		EDIT_MARKER_NO_FOCUS = 0,
		EDIT_MARKER_FOCUS,
		EDIT_MARKER_EDITING,

		EDIT_MARKER_FOCUS_MAX,
	};

	enum ePLAYPAUSE_BUTTON_FRAMES
	{
		PLAYPAUSE_PLAY_NO_FOCUS = 1,
		PLAYPAUSE_PLAY_FOCUS = 2,
		PLAYPAUSE_PAUSE_NO_FOCUS = 3,
		PLAYPAUSE_PAUSE_FOCUS = 4,
	};

	enum eMARKER_DRAG_STATE
	{
		MARKER_DRAG_NONE,
		MARKER_DRAG_PENDING,
		MARKER_DRAG_DRAGGING,
	};

	enum eMARKER_MENU_OPTIONS
	{
		MARKER_MENU_OPTION_TYPE,
		MARKER_MENU_OPTION_TRANSITION,
		MARKER_MENU_OPTION_TIME,

		MARKER_MENU_OPTION_POSTFX_MENU,
		MARKER_MENU_OPTION_FILTER,
		MARKER_MENU_OPTION_FILTER_INTENSITY,
		MARKER_MENU_OPTION_SATURATION,
		MARKER_MENU_OPTION_CONTRAST,
		MARKER_MENU_OPTION_VIGNETTE,
		MARKER_MENU_OPTION_BRIGHTNESS,

		MARKER_MENU_OPTION_DOF_MENU,
		MARKER_MENU_OPTION_DOF_MODE,
		MARKER_MENU_OPTION_DOF_INTENSITY,
		MARKER_MENU_OPTION_FOCUS_MODE,
		MARKER_MENU_OPTION_FOCUS_DISTANCE,
		MARKER_MENU_OPTION_FOCUS_TARGET,

		MARKER_MENU_OPTION_CAMERA_MENU,
		MARKER_MENU_OPTION_CAMERA_TYPE,
		MARKER_MENU_OPTION_EDIT_CAMERA,
		MARKER_MENU_OPTION_CAMERA_ATTACH_TARGET,
		MARKER_MENU_OPTION_CAMERA_LOOKAT_TARGET,
		MARKER_MENU_OPTION_CAMERA_BLEND,
		MARKER_MENU_OPTION_CAMERA_SHAKE,
		MARKER_MENU_OPTION_CAMERA_SHAKE_INTENSITY,
		MARKER_MENU_OPTION_CAMERA_SHAKE_SPEED,
		MARKER_MENU_OPTION_CAMERA_ATTACH_TYPE,
		MARKER_MENU_OPTION_CAMERA_BLEND_EASING,

        MARKER_MENU_OPTION_AUDIO_MENU,
        MARKER_MENU_OPTION_SFX_CAT,
        MARKER_MENU_OPTION_SFX_HASH,
        MARKER_MENU_OPTION_SFX_OS_VOL,
		MARKER_MENU_OPTION_SFX_VOL,
		MARKER_MENU_OPTION_DIALOG_VOL,
        MARKER_MENU_OPTION_MUSIC_VOL,
        MARKER_MENU_OPTION_AMB_VOL,
		MARKER_MENU_OPTION_SCORE_INTENSITY,
		MARKER_MENU_OPTION_MIC_TYPE,

		MARKER_MENU_OPTION_SPEED,

		MARKER_MENU_OPTION_SET_THUMBNAIL,
		MARKER_MENU_OPTION_HELP_TEXT,

		MARKER_MENU_OPTION_MAX,
	};

	enum eCREATE_PHOTO_PROGRESS
	{
		CREATE_PHOTO_PROGRESS_IDLE,
		CREATE_PHOTO_PROGRESS_BEGIN_PHOTO_ENUMERATION,
		CREATE_PHOTO_PROGRESS_CHECK_PHOTO_ENUMERATION,
		CREATE_PHOTO_PROGRESS_CHECK_TAKE_PHOTO,
		CREATE_PHOTO_PROGRESS_BEGIN_SAVE_PHOTO,
        CREATE_PHOTO_PROGRESS_CHECK_SAVE_PHOTO,
        CREATE_PHOTO_PROGRESS_ERROR
	};

	struct MenuOption
	{
		eMARKER_MENU_OPTIONS m_option;
		IReplayMarkerStorage::eEditRestrictionReason m_editRestrictions;
		s32 m_toggleOptionCount;

		MenuOption()
			: m_option( MARKER_MENU_OPTION_MAX )
			, m_editRestrictions( IReplayMarkerStorage::EDIT_RESTRICTION_NONE )
			, m_toggleOptionCount( -1 )
		{

		}

		MenuOption( eMARKER_MENU_OPTIONS const option, IReplayMarkerStorage::eEditRestrictionReason const restriction, s32 const toggleCount = -1 )
			: m_option( option )
			, m_editRestrictions( restriction )
			, m_toggleOptionCount( toggleCount )
		{

		}

		inline bool IsEnabled() const { return m_editRestrictions == IReplayMarkerStorage::EDIT_RESTRICTION_NONE; }
		inline bool IsToggle() const { return m_toggleOptionCount > 0; }
		inline bool HasMoreThanOneToggleValue() const { return m_toggleOptionCount > 1; }
	};

	class MarkerDisplayObject
	{
	public:
		CComplexObject m_markerComplexObject;
		CComplexObject m_timeDilationComplexObject;

		MarkerDisplayObject()
		{

		}

		~MarkerDisplayObject()
		{

		}
	};

	static CComplexObject																ms_ComplexObject[MAX_PLAYBACK_COMPLEX_OBJECTS];
	static atFixedArray<MarkerDisplayObject, CVideoEditorProject::MAX_MARKERS_PER_CLIP>	ms_ComplexObjectMarkers;
	static atArray<MenuOption>															ms_activeMenuOptions;

	static CVideoEditorProject*					ms_project;
	static s32									ms_clipIndexToEdit;

	static ePLAYBACK_TYPE						ms_playbackType;
	static eWARNING_PROMPT_ACTION				ms_pendingAction;

	static eFOCUS_CONTEXT						ms_focusContext;
	static s32									ms_currentFocusIndex;
	static s32									ms_currentFocusIndexDepthBeforeFocus;
	static s32									ms_menuFocusIndex;

	static s32									ms_editMenuStackedIndex;
	static s32									ms_cameraMenuStackedIndex;
	static u32									ms_markerDepthCount;
	static u32									ms_timeDilationDepthCount;

	static s32									ms_pendingMouseToggleDirection;
	static bool									ms_hasTriggeredMouseToggleDirection;
	static u32									ms_mouseToggleDirectionPendingTimeMs;
	static float								ms_mouseToggleDirectionDelayTimeMs;

	static u32									ms_markerDragPendingTimeMs;
	static eMARKER_DRAG_STATE					ms_markerDragState;
	static CReplayMarkerContext::eEditWarnings	ms_currentEditWarning;
	static float								ms_warningDisplayTimeRemainingMs;
	static float								ms_cameraEditFovLastFrame;

	static s32									ms_cameraShuttersFrameDelay;
	static bool									ms_bKeepCameraShuttersClosed;
	static s32									ms_tutorialCountdownMs;

	static bool									ms_jumpedToMarkerOnDragPending;
	static bool									ms_isScrubbing;

	static bool ms_hasBeenEdited;
	static bool ms_hasAnchorsBeenEdited;
	static bool ms_bActive;
	static bool ms_bReadyToPlay;
	static bool ms_bWantDelayedClose;
	static bool ms_refreshInstructionalButtons;
	static bool ms_rebuildMarkers;
	static bool ms_refreshEditMarkerState;
	static bool ms_shouldRender;
	static bool ms_bLoadingScreen;
	static bool ms_bLoadingText;
	static bool ms_bPreCaching;
	static bool ms_bWasScrubbingOnLoad;
	static bool ms_bWasKbmLastFrame;
	static bool ms_bMouseInEditMenu;
	static bool ms_bUpdateAddMarkerButtonState;
    static bool ms_bAddNewMarkerOnNextUpdate;
    static bool ms_bWasPlayingLastFrame;
    static bool ms_bDelayedBackOut;
    static bool ms_bBlockDraggingUntilPrecacheComplete;

	static eCREATE_PHOTO_PROGRESS				ms_CreatePhotoProgress;

#if RSG_PC
	static bool							ms_bMouseHeldDown[MAX_PLAYBACK_CLICKABLE_ITEMS];
	static s32							ms_mouseHeldMarker;
	static float						ms_markerXOffset;
#endif // #if RSG_PC

#if defined(SHOW_PLAYBACK_BUTTONS) && SHOW_PLAYBACK_BUTTONS

	static s32 ms_playbackButtonStates[ PLAYBACK_BUTTON_MAX ];

#endif

private:
	static char const * const sc_EditMarkerMenuHelpText[ MARKER_MENU_OPTION_MAX ];

	static void InitStep1();

	static bool IsPendingInitStep2();
	static void InitStep2();


	static void Shutdown();
	static void UpdateInput();
#if RSG_PC
	static void UpdateMouseInput();
#endif //#if RSG_PC
	static void UpdateWarningPrompt();

	static bool IsPendingWarningPrompt() { return ms_pendingAction != WARNING_PROMPT_NONE; }
	static bool IsValidPlaybackButton( s32 const button ) { return button >= PLAYBACK_BUTTON_FIRST && button < PLAYBACK_BUTTON_MAX; }
	static bool IsDraggingMarker() { return ms_markerDragState == MARKER_DRAG_DRAGGING; }
	static bool IsPendingMarkerDrag() { return ms_markerDragState == MARKER_DRAG_PENDING; }
    static bool CanDragMarker();

	static bool DoesContextSupportQuickPlaybackControls() { return !IsDraggingMarker() && !IsPendingMarkerDrag() && !IsEditingCamera(); }

	static bool DoesContextSupportTopLevelBackOut() { return ( ms_focusContext == FOCUS_MARKERS || ms_focusContext == FOCUS_NONE ) 
		&& !IsEditingCamera() && !IsScrubbing() && !IsLoading() && !IsDraggingMarker() && !IsPendingMarkerDrag(); }

    static void UpdateTopLevelBackoutInput();
    static void HandleTopLevelBackOut();

	static bool IsOnAcceleratingMenu();
	static u32 GetNavInputCheckFlags();

	static bool CanScrub();
	static bool CanMouseScrub() { return !ms_bMouseInEditMenu && CanScrub(); }

	static bool HasBeenEdited()			{ return IsEditing() && (ms_hasBeenEdited || ms_hasAnchorsBeenEdited); }
	static bool HasAnchorsBeenEdited()	{ return IsEditing() && ms_hasAnchorsBeenEdited; }
	static void SetHasBeenEdited();
	static void SetAnchorsHasBeenEdited();
	static void ClearEditFlags();
	static bool CanUpdateThumbnail() { return IsEditing() && !IsScrubbing() && !IsPendingWarningPrompt() && !IsEditingCamera(); }
	static void UpdateThumbnail();

	static void UpdateTimerText();
	static void UpdatePlayBar();
	static void UpdateLoadingScreen();
	static void UpdateEditMenuState();
	static void UpdatePlaybackControlVisibility();

	static void UpdateInstructionalButtonsPlaybackShortcuts( CScaleformMovieWrapper& buttonsMovie );
	static void UpdateInstructionalButtonsBaking( CScaleformMovieWrapper& buttonsMovie );
	static void UpdateInstructionalButtonsEditing( CScaleformMovieWrapper& buttonsMovie );
    static void UpdateInstructionalButtonsPreview( CScaleformMovieWrapper& buttonsMovie );
    static void UpdateInstructionalButtonsLoading( CScaleformMovieWrapper& buttonsMovie );

	static void UpdateSpinnerPlaybackProgress();

	// Edit Markers
	static void AddNewEditMarker();
	static void DeleteCurrentEditMarker();
	static void ResetCurrentEditMarker();

	static bool IsFocusedOnEditMarker() { return IsEditing() && ms_focusContext == FOCUS_MARKERS && ms_currentFocusIndex >= 0; }
	static bool IsFocusedOnEditMarker( s32 const markerIndex ) { return IsFocusedOnEditMarker() && ms_currentFocusIndex == markerIndex; }

	static bool IsOnFirstMarker() { return IsEditing() && ms_currentFocusIndex == 0; }
	static bool IsOnLastMarker() { return IsEditing() && ms_currentFocusIndex == GetLastMarkerIndex(); }
	static bool IsOnTransitionPoint() { return IsOnFirstMarker() || IsOnLastMarker(); }

	static bool IsEditingEditMarker() { return IsEditing() && ms_focusContext == FOCUS_MARKER_MENU; }
	static bool IsEditingEditMarker( s32 const markerIndex ) { return IsOnEditMenu() && ms_currentFocusIndex == markerIndex; }
	static bool IsEditingCamera();

	static void UpdateEditMarkerStates();
	static void SetEditMarkerState( CComplexObject& editMarkerObject, eEDIT_MARKER_FOCUS const focus, bool const isAnchor );
	static void SetTimeDilationState( CComplexObject& timeDilationObject, float const width, 
													bool const isSpedUp, bool const isSlowedDown );

	static void EnterCamMenu( s32 const startingFocus = 0 );
	static bool IsOnCameraMenu() { return ms_focusContext == FOCUS_CAMERA_MENU; }
	static void ExitCamMenu();

	static void EnterCamEditMode();
	static void ExitCamEditMode();

	static void EnterPostFxMenu( s32 const startingFocus = 0 );
	static bool IsOnPostFxMenu() { return ms_focusContext == FOCUS_POSTFX_MENU; }
	static void ExitPostFxMenu();

	static void EnterDoFMenu( s32 const startingFocus = 0 );
	static bool IsOnDoFMenu() { return ms_focusContext == FOCUS_DOF_MENU; }
	static void ExitDoFMenu();

	static void EnterAudioMenu( s32 const startingFocus = 0 );
	static bool IsOnAudioMenu() { return ms_focusContext == FOCUS_AUDIO_MENU; }
	static void ExitAudioMenu();

	static bool IsOnEditMenu() { return IsEditingEditMarker() || IsOnCameraMenu() || IsOnPostFxMenu() || IsOnDoFMenu() || IsOnAudioMenu(); }

    static bool CanCopyMarkerProperties() { return IsOnPostFxMenu() || IsOnAudioMenu(); }
    static eMarkerCopyContext GetCopyContext();
    static bool CanPasteMarkerProperties();
    static void CopyCurrentMarker();
    static void PasteToCurrentMarker();

	static MenuOption const* TryGetMenuOption( s32 const index );
	static MenuOption const* TryGetMenuOptionAndIndex( eMARKER_MENU_OPTIONS const option, s32& out_index );

	static bool IsFirstEntryToEditMode();
	static void SetFirstEntryEditModeTutorialSeen();
	static bool IsWaitingForTutorialToFinish();

	// Marker Common
	static bool RebuildMarkersOnPlayBar();
	static void RefreshMarkerPositionsOnPlayBar();
	static void CleanupMarkersOnPlayBar();
	static void AddMarkerToPlayBar( char const * const type, float const clipTime, float const clipEndTime, 
		Vector2 const& startPosition, u32 const markerIndex, u32 const complexObjectInsertionIndex );

	static s32 GetMarkerIndexForComplexObjectName( char const * const objectName );
	static s32 GetMarkerIndexForComplexObjectNameHash( u32 const objectNameHash );

	static void RemoveMarkerOnPlaybar( u32 const index );
	static void UpdateMarkerPositionOnPlayBar( CComplexObject& markerObject, CComplexObject& timeDilationObject, u32 const markerIndex, float const clipTime, float const clipEndTime, Vector2 const& startPosition );

	static bool UpdateFocusedMarker( float const currentTimeMs );
	static void RefreshEditMarkerMenu();

	static void UpdateFineScrubbing( float const increment );
	static void UpdateScrubbing();
	static void StopScrubbing();
	static void ResetScrubbingParams();
	static bool UpdateScrubbingCommon( float scrubAxis, float const c_minTimeMs, float const c_maxTimeMs, 
		bool& inout_isScrubbing, float& out_currentTime );
	static float GetScrubAxisThreshold();
	static float GetScrubMultiplier();
	static float GetFineScrubbingIncrement( float const multiplier );

	static bool UpdateMarkerDrag( u32 const markerIndex, float const playbarPercentage );

	static void UpdateRepeatedInput();
	static void UpdateFocusedInput( s32 const c_inputType );
	static void UpdateFocusedInputEditMenu( s32 const c_inputType );

	static void UpdateRepeatedInputMarkers( float const c_xAxis );
	static bool UpdateRepeatedInputMarkerTime( float const c_xAxis );
	static bool UpdateMarkerJumpToBeat();
	static void UpdateCameraControls();

	static void UpdateWarningText();
	static inline bool HasWarningText() { return ms_currentEditWarning != CReplayMarkerContext::EDIT_WARNING_NONE; }

	static bool CheckExportStatus();

#if defined(SHOW_PLAYBACK_BUTTONS) && SHOW_PLAYBACK_BUTTONS

	static void SetPlaybackButtonVisibility( s32 const button, bool const isVisible );

	// Playback button control
	static void UpdatePlayPauseButtonState( s32 const iMouseEventType );
	static void UpdatePlayPauseButtonState(); // non-mouse version

	static void UpdatePlaybackButtonAction( ePlaybackButtonId const playbackButton, s32 const iMouseEventType );
	static void UpdatePlaybackButtonStates( ePlaybackButtonId const playbackButton, s32 const iMouseEventType );
	static void UpdatePlaybackButtonToggleStates( ePlaybackButtonId const playbackButton, s32 const iMouseEventType );
	static void UpdatePlaybackButtonToggleStates();

	static void SetPlaybackButtonState( s32 const button, s32 const stateFrame );

	static ePlaybackButtonId GetPlaybackButtonId( char const * const buttonName );

#endif // defined(SHOW_PLAYBACK_BUTTONS) && SHOW_PLAYBACK_BUTTONS

	static void ActuateCurrentEditMarker();
	static void OpenCurrentEditMarkerMenu();
	static bool JumpToClosestEditMarker();
	static bool JumpToCurrentEditMarker();

	static void EnableEditMarkerMenu( s32 const startingFocus = 0 );
	static void UpdateMarkerMenuTimeValue( u32 const timeMs );
	static void UpdateMenuHelpText( s32 const menuIndexForHelpText );
	static void DisableCurrentMenu();
	static void CloseCurrentMenu();

	static void UpdateItemFocusDependentState();
	static void UpdateCameraTargetFocusDependentState();

	static void BackoutEditMarkerMenu();

	// Playback Bar
	static float GetPlaybackBarBaseStartPositionX();
	static float GetPlaybackBarEffectiveStartPositionX();
	static float GetPlaybackBarBaseStartPositionY();
	static float GetPlaybackBarMaxWidth();
	static float GetPlaybackBarEffectiveWidth();
	static float GetPlaybackBarHeight();

	static float GetPlaybackBarClickableRegionPositionX();
	static float GetPlaybackBarClickableRegionWidth();

	static float GetEffectiveCurrentClipTimeMs();

	static bool IsSpaceAvailableToSaveCurrentProject(); 
	static bool IsSpaceToAddMarker(); 

	// Camera Shutters
	static void BeginCameraShutterEffect(bool bKeepClosed);
	static void EndCameraShutterEffect();
	static void OpenCameraShutters();

	// Taking a Snapmatic Photo in the Editor
	static void BeginPhotoCreation();
	static void UpdatePhotoCreation();

	// Camera Target Marker
	static void SetupCameraTargetMarkerForEntity( s32 const id );
	static void SetupCameraTargetMarker( Vector3 const& position );

	// Marker Helpers
	static IReplayMarkerStorage* GetMarkerStorage();
    static IReplayMarkerStorage* GetMarkerStorage( u32 const clipIndex );
    static sReplayMarkerInfo* GetPreviousEditMarker();
	static sReplayMarkerInfo* GetCurrentEditMarker();
    static sReplayMarkerInfo* GetClosestEditMarker();
    static sReplayMarkerInfo* GetClosestEditMarker( u32 const clipIndex, eClosestMarkerPreference const markerPref );
    static float GetClosestEditMarkerTimeMs();
    static float GetClosestEditMarkerTimeMs( u32 const clipIndex, eClosestMarkerPreference const markerPref );
	static float GetCurrentEditMarkerTimeMs();
	static s32 GetCurrentEditMarkerIndex();
	static s32 GetEditMarkerCount();
	static s32 GetLastMarkerIndex();
	static bool CanAddMarkerAtCurrentTime();
	static bool IsAtMaxMarkerCount();
	static bool CanDeleteMarkerAtCurrentTime();
	static s32 GetNearestEditMarkerIndex();
	static s32 GetNearestEditMarkerIndex( bool const greaterThan );
	static u32 GetMaxMarkerCount();

	static MarkerDisplayObject* GetEditMarkerDisplayObject( s32 const index );
	static CComplexObject* GetTimeDilationComplexObject( s32 const index );
	static CComplexObject* GetEditMarkerComplexObject( s32 const index );
	static CComplexObject* GetCurrentEditMarkerComplexObject();

	static void BeginAddingColumnData(s32 iColumnId, s32 iColumnType, const char *pHeaderText);
	static void EndAddingColumnData(bool bShouldRender, bool bSetActive);
	static void RemoveColumn(s32 iColumnToClear);

	static void UnsetAllItemsHovered();
	static void SetItemActiveDisable( s32 const index );
	static void SetItemSelected( s32 const itemIndex, bool const disableArrows = false );
	static void SetItemHovered( s32 const itemIndex );
	static void SetCurrentItemIntoCorrectState();

	static void UpdateItemTextValue( s32 const itemIndex, char const * const localizedString );
	static void UpdateItemNumberValue( s32 const itemIndex, u32 const number );
	static void UpdateItemPercentValue( s32 const itemIndex, s32 const percent );
	static void UpdateItemValueFloat( s32 const itemIndex, float const floatValue );
	static void UpdateItemPercentValueFloat( s32 const itemIndex, float const percent );
	static void UpdateItemFxValue( s32 const itemIndex, s32 const fxVal );
	static void UpdateItemFxValueFloat( s32 const itemIndex, float const fxVal );
	static void UpdateItemMetresValueFloat( s32 const itemIndex, float const metres );
	static void UpdateItemTimeValue( s32 const itemIndex, u32 const timeMs );

	static void DisableMenu();
	static void SetMenuVisible( bool const visible );

	static void CleanupTimeDilationElement( CComplexObject& dilationObject );
	static void SetTransitionElementVisible(CComplexObject& markerObject, bool const isVisible );
	static void SetupPlaybackButtonState( CComplexObject& playbackPanel, bool const isEditing );
	static void SetWarningText( char const * const lngKey );
	static void ClearWarningText();

	static void SetCameraOverlayVisible( bool const visible );
	static void UpdateCameraOverlay();
	static bool CanRollCurrentCamera();
	static void GetDefaultAndMinAndMaxFovForCurrentCamera( float& out_defaultFov, float& out_minFov, float& out_maxFov );

	static bool CanSkipToBeat();
	static bool CanSkipMarkerToBeat( s32 const markerIndex );

	static void SetupLoadingScreen();
	static void CleanupLoadingScreen( bool const isExiting = false );

	static void UpdateAddMarkerButtonState();
protected:
	friend class CVideoEditorInstructionalButtons;

	static void UpdateInstructionalButtons( CScaleformMovieWrapper& buttonsMovie );
};

#endif // VIDEO_EDITOR_PLAYBACK_H

#endif //#if defined( GTA_REPLAY ) && GTA_REPLAY

// eof
