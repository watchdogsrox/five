/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : Editor.h
// PURPOSE : the NG video editor - header
// AUTHOR  : Derek Payne
// STARTED : 17/06/2014
//
/////////////////////////////////////////////////////////////////////////////////
#include "control/replay/ReplaySettings.h"
#include "video/VideoPlaybackSettings.h"

#ifndef VIDEO_EDITOR_UI_H
#define VIDEO_EDITOR_UI_H

#if defined( GTA_REPLAY ) && GTA_REPLAY


// Rage headers
#include "atl/hashstring.h"
#include "atl/string.h"
#include "output/constantlighteffect.h"
#include "parser/macros.h"

// Framework headers
#include "fwlocalisation/templateString.h"
#include "fwnet/netprofanityfilter.h"
#include "video/VideoPlayback.h"

// game headers
#include "control/replay/file/FileData.h"
#include "renderer/sprite2d.h"
#include "replaycoordinator/ReplayEditorParameters.h"
#include "replaycoordinator/Storage/MontageMusicClip.h"
#include "replaycoordinator/Storage/MontageText.h"
#include "frontend/Scaleform/ScaleFormMgr.h"
#include "frontend/VideoEditor/Core/VideoEditorProject.h"
#include "system/control.h"
#include "Text/TextFormat.h"

#include "frontend/VideoEditor/ui/TextTemplate.h"


class CRawClipFileView;
class CVideoProjectFileView;
class CVideoFileView;
class CVideoEditorProject;
class CThumbnailFileView;

#define USE_OLD_TEXT_EDITOR (1)

#define MIN_AMBIENT_TRACK_DURATION_MS ( MIN_AMBIENT_DURATION_MS )  
#define MIN_RADIO_TRACK_DURATION_MS	( MIN_MUSIC_DURATION_MS )  
#define MIN_SCORE_TRACK_DURATION_MS	( MIN_SCORE_DURATION_MS )
#define MIN_TEXT_DURATION_MS	(500)

struct sSelectedAudioProperties
{
	u32 m_soundHash;
	u32 m_durationMs;
	s32 m_startOffsetMs;
	bool m_isScoreTrack;
};

struct sExportProperties
{
	sExportProperties();
	
	s32 m_fps;
	s32 m_quality;
};

enum ePLAYHEAD_TYPE  // must match frame numbers in ActionScript
{
	PLAYHEAD_TYPE_NONE = 0,

	PLAYHEAD_TYPE_VIDEO,
	PLAYHEAD_TYPE_AMBIENT,
	PLAYHEAD_TYPE_AUDIO,
	PLAYHEAD_TYPE_TEXT,

	PLAYHEAD_TYPE_ANCHOR,

	PLAYHEAD_TYPE_OVERVIEW_VIDEO,
	PLAYHEAD_TYPE_OVERVIEW_AMBIENT,
	PLAYHEAD_TYPE_OVERVIEW_AUDIO,
	PLAYHEAD_TYPE_OVERVIEW_TEXT,

	PLAYHEAD_TYPE_GHOST,

	MAX_PLAYHEAD_TYPES
};


enum eICON_TYPE
{
	ICON_TYPE_NONE = 0,
	ICON_TYPE_VIDEO,
	ICON_TYPE_AUDIO,
	ICON_TYPE_TEXT,
	ICON_TYPE_LINKED,
	ICON_TYPE_SCORE,
	ICON_TYPE_OTHER_USER,
	ICON_TYPE_UNSAVED,
	ICON_TYPE_REDLIGHT,
	ICON_TYPE_AMBERLIGHT,
	ICON_TYPE_GREENLIGHT,
	ICON_TYPE_AMBIENT,
	MAX_ICON_TYPES
};

enum eMENU_ITEM_COLOUR
{
	MENU_ITEM_COLOUR_NONE = 0,
	MENU_ITEM_COLOUR_PENDING_DELETE,
	MENU_ITEM_COLOUR_FAVOURITE,

	MAX_IMENU_ITEM_COLOUR
};

enum eVIDEO_EDITOR_BUTTON_TYPES
{
	BUTTON_TYPE_INVALID = -1,
	BUTTON_TYPE_MENU = 0,
	BUTTON_TYPE_VIDEO_CLIP,
	BUTTON_TYPE_AUDIO_CLIP,
	BUTTON_TYPE_TEXT_CLIP,
	BUTTON_TYPE_CLIP_EDIT_TIMELINE,
	BUTTON_TYPE_MARKER,
	BUTTON_TYPE_PLAYBACK_BUTTON,
	BUTTON_TYPE_SCORE_CLIP,
	BUTTON_TYPE_STAGE,
	BUTTON_TYPE_TIMELINE_ARROWS,
	BUTTON_TYPE_OVERVIEW_SCROLLER, 
	BUTTON_TYPE_LIST_SCROLL_ARROWS,
	BUTTON_TYPE_OVERVIEW_BACKGROUND,
	BUTTON_TYPE_AMBIENT_CLIP,
};

enum eVIDEO_EDITOR_DEPTHS
{
	VE_DEPTH_TIMECODE = 10,
	VE_DEPTH_TIMELINE_PANEL = 20,
	VE_DEPTH_PLAYBACK_PANEL = 30,
	VE_DEPTH_OVERVIEW_CONTAINER = 40,
	VE_DEPTH_OVERVIEW_SCROLLER = 50,

	VE_DEPTH_TIME_DILATION = 1024,
	VE_DEPTH_MARKERS = VE_DEPTH_TIME_DILATION + CVideoEditorProject::MAX_MARKERS_PER_CLIP,
	VE_DEPTH_EDITING_MARKER = VE_DEPTH_MARKERS + CVideoEditorProject::MAX_MARKERS_PER_CLIP,

	VE_DEPTH_CLIP_EDIT_CAMERA_OVERLAY,
	VE_DEPTH_CLIP_EDIT_HELPTEXT,
	VE_DEPTH_PLAYBACK_SCRUBBER,
	VE_DEPTH_CLIP_EDIT_CAMERA_SHUTTERS,
	VE_DEPTH_CLICKABLE_PLAYBAR,

	// do not enter any global depths below this please
	VE_DEPTH_SAFE_WRAP_START
};


enum eME_EDITING_STATE
{
	ME_EDITING_STATE_IDLE = 0,
	ME_EDITING_STATE_EDITING,
	MAX_ME_EDITING_STATEs
};


enum eME_MOVIE_MODE  // must match same as in Actionscript
{
	ME_MOVIE_MODE_TIMELINE = 0,
	ME_MOVIE_MODE_PLAYBACK,
	ME_MOVIE_MODE_MENU, 
};



enum eME_STATES
{
	ME_STATE_NONE = 0,
	ME_STATE_LOADING,
	ME_STATE_INIT,
	ME_STATE_SETUP,
	ME_STATE_ACTIVE,
	ME_STATE_SHUTDOWN,
	ME_STATE_WAITING_PROJECT_LOAD,
	ME_STATE_WAITING_PROJECT_GENERATION,
	ME_STATE_ACTIVE_WAITING_FOR_MENU_DATA,
	ME_STATE_ACTIVE_WAITING_FOR_FILENAME,
	ME_STATE_ACTIVE_WAITING_FOR_FILENAME_THEN_EXIT,
	ME_STATE_ACTIVE_WAITING_FOR_EXPORT_FILENAME,
	ME_STATE_ACTIVE_WAITING_FOR_EXPORT_FILENAME_PROFANITY_CHECK,
	ME_STATE_ACTIVE_WAITING_FOR_TEXT_INPUT,
	ME_STATE_ACTIVE_WAITING_FOR_VIDEO_UPLOAD_TEXT_INPUT,
	ME_STATE_ACTIVE_TEXT_ADJUSTMENT,
	ME_STATE_ACTIVE_WAITING_FOR_STAGING_CLIP,
    ME_STATE_ACTIVE_WAITING_FOR_STAGING_CLIP_TL,  // todo
    ME_STATE_ACTIVE_WAITING_FOR_DECODER_MEM,

	MAX_ME_STATEs
};


enum eCodeSprites
{
	CODE_SPRITE_LOGO = 0,
	CODE_SPRITE_WALLPAPER,
	MAX_CODE_SPRITES
};

// RM: The SCRIPT_ enums are mirrored in script (commands_replay.sch)
// (minus sentinel values OFS_SCRIPT_FIRST & OFS_SCRIPT_LAST)
enum eOpenedFromSource
{
	OFS_UNKNOWN = 0,
	
	OFS_SCRIPT_FIRST,
	OFS_SCRIPT_DEFAULT = OFS_SCRIPT_FIRST,
	OFS_SCRIPT_LANDING_PAGE,
	OFS_SCRIPT_DIRECTOR,
	OFS_SCRIPT_LAST = OFS_SCRIPT_DIRECTOR,

	OFS_CODE_FIRST,
	OFS_CODE_DEFAULT = OFS_CODE_FIRST,
	OFS_CODE_PAUSE_MENU,
	OFS_CODE_LANDING_PAGE,
	OFS_CODE_LAST = OFS_CODE_LANDING_PAGE
};

#define VIDEO_EDITOR_UI_FILENAME				"new_editor"
#define VIDEO_EDITOR_CODE_TXD					"replay_textures"
#define VIDEO_EDITOR_LOAD_ERROR_ICON			"image_load_error"
#define VIDEO_EDITOR_DEFAULT_ICON				"video_default_image"



class CVideoEditorUi
{
public:
    typedef fwTemplateString<MAX_FILEDATA_INPUT_FILENAME_BUFFER_SIZE> FilenameInputBuffer;

	static void Init(unsigned initMode);
	static void Shutdown(unsigned shutdownMode);
	static void Update();
	static void Render();
	static void RenderLoadingScreenOverlay();

	static void Open(eOpenedFromSource source = OFS_CODE_DEFAULT);
	static void Close(const bool fadeOut = true);
	static void OnSignOut( );

	static bool IsActive() { return ms_iState != ME_STATE_NONE; }
	static bool IsInitialising() { return ms_iState == ME_STATE_INIT || ms_iState == ME_STATE_SETUP || ms_iState == ME_STATE_LOADING; }
	static bool IsStateWaitingForData() { return ms_iState == ME_STATE_ACTIVE_WAITING_FOR_MENU_DATA; }

	static void SetActiveState() { ms_iState = ME_STATE_ACTIVE; }
	static void SetStateWaitingForData() { ms_iState = ME_STATE_ACTIVE_WAITING_FOR_MENU_DATA; }

	static void SetEditingText() { ms_iState = ME_STATE_ACTIVE_TEXT_ADJUSTMENT; }

	static void RefreshThumbnail( u32 const clipIndex );
	static void RebuildTimeline();
	static void RebuildAnchors();

	static bool StageTextAdjustment();

	static void GarbageCollect();
	static void ShowTimeline();
	static bool ShowMenu(const atHashString menuToShow);
	static bool ShowMainMenu();
	static bool ShowOptionsMenu();
	static bool ShowExportMenu();

	static void ShowPlayback();

	static void SetActionEventsOnGameDestruction();
	static void ActionEventsOnGameDestruction();

	static void SetupFilenameEntry();
	static void SetupExportFilenameEntry();
	static void SetupTextEntry(const char* title = NULL, bool clearKeyboard = false);

    static bool CheckExportDuration();
	static void CheckExportLimits();
	
	static void SetProjectEdited(const bool bEdit);
	static bool HasProjectBeenEdited() { return ms_bProjectHasBeenEdited; }

	static bool HasDirectorModeBeenLaunched() { return ms_directorModeLaunched; }
	static void ClearDirectorMode() { ms_directorModeLaunched = false; }

	static void SetUserWarnedOfGameDestroy(const bool bTrueFalse) { ms_bGameDestroyWarned = bTrueFalse; }
	static bool HasUserBeenWarnedOfGameDestory() { return ms_bGameDestroyWarned; }

    static void SetFreeReplayBlocksNeeded();

#if defined(VIDEO_PLAYBACK_ENABLED) && VIDEO_PLAYBACK_ENABLED
#if !USE_ORBIS_UPLOAD
	static void SetupVideoUploadTextEntry(const char* title, u32 maxStringLength = 32, const char* initialValue = NULL);
#endif
	static void BeginFullscreenVideoPlayback(const s32 iIndex);
	static bool IsPlaybackPaused();
#endif // defined(VIDEO_PLAYBACK_ENABLED) && VIDEO_PLAYBACK_ENABLED

	static void TriggerNewText();
	static bool TriggerNewBlank();
	static bool TriggerLoad(const u32 uProjectIndex);
	static void TriggerSaveAs(const bool bExit);
	static void TriggerSave(const bool bExit);
	static void TriggerAdditionalExportPrompt();
	static void TriggerOverwriteUnavailablePrompt( char const * const filename );
	static void TriggerExport();
	static bool TriggerAutomatedMovie();
	static void TriggerAddClipToTimeline(const u32 uProjectIndex);
	static void TriggerAddAmbientToTimeline();
	static void TriggerAddAudioToTimeline();
	static void TriggerAddTextToTimeline();
	static void AttemptToReplaceEditedTextWithNewText();
	static void PickupSelectedClip(const u32 uProjectIndex, const bool bDeleteOriginal);
	static void DeleteSelectedClip();
	static void TriggerDirectorMode();
	static void TriggerFullScreenPreview();
	static bool TriggerPreviewClip( u32 const clipIndex );
	static void TriggerEditClip();
	static void SaveProject( bool const showSpinnerWithText );
	static void CheckAndTriggerExportPrompts( char const * const filename );

	static void SetSelectedAudio( u32 const c_soundHash, const s32 c_startOffsetMs, u32 const c_durationMs, bool const c_isScoreTrack);

	static void EditLengthSelection(s32 const c_direction);
	static void EditTextDuration(s32 const c_direction);
	static void EditTemplatePosition(s32 const c_direction);
	static void EditTextTemplate(s32 const c_direction);
	static void EditExportFps(s32 const c_direction);
	static void EditExportQuality(s32 const c_direction);
	static s32 GetAutomatedLengthSelection() { return ms_iAutomatedLengthSelection; }
	static sSelectedAudioProperties *GetSelectedAudio() { return &ms_SelectedAudio; }
	static const sExportProperties &GetExportProperties() { return ms_ExportProperties; }

	static void LeaveEditingMode();

	static bool FindProjectNameInUse(const char *pName);

	static void SetErrorState( const char *pHashString, const char* literalString = NULL, const char* literalString2 = NULL );

#if RSG_PC
	static void UpdateMouseEvent(const GFxValue* args);
#endif // #if RSG_PC

#if __BANK
	static void InitWidgets();
	static void ShutdownWidgets();
#endif // __BANK

#if VIDEO_PLAYBACK_ENABLED
	static CVideoFileView *GetVideoFileView() { return ms_VideoFileView; }
	static VideoInstanceHandle GetActivePlaybackHandle() { return ms_activePlaybackHandle; }
#endif // defined(VIDEO_PLAYBACK_ENABLED) && VIDEO_PLAYBACK_ENABLED

	static bool IsAdjustingStageText() { return ms_iState == ME_STATE_ACTIVE_TEXT_ADJUSTMENT; }

	static char const * GetExportName() { return ms_exportNameBuffer.getBuffer(); }
	static char const * GetLiteralStringBuffer() { return ms_literalBuffer.getBuffer(); }
	static char const * GetLiteralStringBuffer2() { return ms_literalBuffer2.getBuffer(); }
	static void UpdateLiteralStringAsDuration();
	static void UpdateLiteralStringAsSize( size_t const sizeNeeded, size_t const sizeAvailable );
	static void UpdateLiteralStringAsNumber( u32 const c_number );

	static bool CanDuplicateClips();

    static bool HasMinDecoderMemory();

private:
	static void CloseInternal();
	static CReplayEditorParameters::eAutoGenerationDuration GetAutomatedMovieLength() { return (CReplayEditorParameters::eAutoGenerationDuration)ms_iAutomatedLengthSelection; }
	static s32 ms_iAutomatedLengthSelection;

	static bool CheckForInviteInterruption();
	static void UpdateMouseInput();
	static void SetMovieMode(eME_MOVIE_MODE mode);
	static bool CreateFileViews();
	static void ReleaseFileViews();
	static void RefreshFileViews();
	static void RefreshVideoViewWhenSafe(bool completedExport = false);
	static void ReleaseProject();

	static void ToggleVideoPlaybackPaused();
	static void SeekToVideoPlaybackStart();
	static void EndFullscreenVideoPlayback();

	static bool CheckForUserConfirmationScreens();

	static bool ShouldRenderOverlayDuringTextEdit() { return ms_bRenderStageText; }
	static bool RenderWallpaper();
#if USE_CODE_TEXT
	static void RenderOverlayText();
#endif // #if USE_CODE_TEXT
	static bool UpdateInput();

	static bool IsInBlockingOperation( bool* opt_needsSpinner = NULL );
	static bool IsInHideEditorOperation( bool* out_showInstructionalButtons = NULL );
	static void InitCodeTextures();
	static void ShutdownCodeTextures();

	static void getExportTimeAsString( char (&out_buffer)[ 128 ] );
	static void DisplaySaveProjectResultPrompt( u64 const resultFlags );

	static CSprite2d ms_codeSprite[MAX_CODE_SPRITES];

	static s32 ms_iMovieId;
	static eME_STATES ms_iState;
	static bool ms_bRebuildTimeline;
	static bool ms_bRebuildAnchors;
	static bool ms_bProjectHasBeenEdited;
	static bool ms_bGameDestroyWarned;
    static bool ms_refreshVideoViewOnMemBlockFree;

	static CVideoEditorProject *ms_project;
	static CVideoProjectFileView *ms_ProjectFileView;
#if VIDEO_PLAYBACK_ENABLED
	static CVideoFileView *ms_VideoFileView;
#endif // defined(VIDEO_PLAYBACK_ENABLED) && VIDEO_PLAYBACK_ENABLED
	static CRawClipFileView *ms_clipFileView;

	static grcBlendStateHandle ms_StandardSpriteBlendStateHandle;

#if VIDEO_PLAYBACK_ENABLED
	static VideoInstanceHandle ms_activePlaybackHandle;
#endif // defined(VIDEO_PLAYBACK_ENABLED) && VIDEO_PLAYBACK_ENABLED

	static sExportProperties ms_ExportProperties;
	static sSelectedAudioProperties ms_SelectedAudio;
	static netProfanityFilter::CheckTextToken ms_profanityToken;
	static fwTemplateString<CMontageText::MAX_MONTAGE_TEXT_BYTES>	ms_textBuffer;

	static FilenameInputBuffer		                                ms_exportNameBuffer;
	static SimpleString_128											ms_literalBuffer;
	static SimpleString_128											ms_literalBuffer2;
	static bool ms_bRenderStageText;

	static bool ms_directorModeLaunched;
	static bool ms_hasStoppedSounds;

	static bool ms_actionEventsOnGameDestruction;

	static float const sm_minMemeTextSize;
	static float const sm_maxMemeTextSize;
	static float const sm_minMemeTextSizeIncrement;

	static float const sm_triggerThreshold;
	static float const sm_thumbstickThreshold;
	static float const sm_thumbstickMultiplier;
	
	static eOpenedFromSource sm_openedFromSource;

protected:
	friend class CVideoEditorMenu;
	friend class CTextTemplate;
	friend class CVideoEditorTimeline;
	friend class CVideoEditorPlayback;
	friend class CVideoEditorPlaybackGUI;
	friend class CVideoEditorThumbnailManager;

	static s32 GetMovieId() { return ms_iMovieId; }

	static CVideoEditorProject *GetProject() { return ms_project; }
	static bool IsProjectPopulated();

	static void ReleaseFileViewThumbnails();

	static CVideoProjectFileView *GetProjectFileView() { return ms_ProjectFileView; }
	static CRawClipFileView *GetClipFileView() { return ms_clipFileView; }

	static CThumbnailFileView *GetThumbnailViewFromClipFileView() { return (CThumbnailFileView*)ms_clipFileView; }
	static CThumbnailFileView *GetThumbnailViewFromProjectFileView() { return (CThumbnailFileView*)ms_ProjectFileView; }

	static void LoadAudioTrackText();
	static bool IsAudioTrackTextLoaded();

	static void ActivateStageClipTextOverlay(bool const c_activate);

private:
#if __BANK
	static void DebugCreateVideoEditorBankWidgets();
#endif  // __BANK

#if LIGHT_EFFECTS_SUPPORT
	static const ioConstantLightEffect sm_LightEffect;
#endif // LIGHT_EFFECTS_SUPPORT

};

#endif // VIDEO_EDITOR_UI_H

#endif // #if defined( GTA_REPLAY ) && GTA_REPLAY
// eof
