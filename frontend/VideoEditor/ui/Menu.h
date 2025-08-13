/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : Menu.h
// PURPOSE : menu of the video editor
// AUTHOR  : Derek Payne
// STARTED : 01/12/2013
//
/////////////////////////////////////////////////////////////////////////////////
#include "control/replay/ReplaySettings.h"
#include "video/VideoPlaybackSettings.h"

#ifndef VIDEO_EDITOR_MENU_H
#define VIDEO_EDITOR_MENU_H

#if defined( GTA_REPLAY ) && GTA_REPLAY

// Rage headers
#include "atl/hashstring.h"
#include "atl/string.h"
#include "parser/macros.h"

// Framework headers
#include "fwlocalisation/templateString.h"
#include "video/VideoPlayback.h"

// game headers
#include "frontend/VideoEditor/Views/FileViewFilter.h"
#include "frontend/WarningScreen.h"
#include "replaycoordinator/ReplayAudioTrackProvider.h"
#include "replaycoordinator/ReplayEditorParameters.h"
#include "replaycoordinator/Storage/MontageText.h"
#include "frontend/Scaleform/ScaleFormMgr.h"
#include "Text/TextFormat.h"


class CRawClipFileView;
class CVideoProjectFileView;
class CVideoFileView;
class CVideoEditorProject;
class CThumbnailFileView;




enum eME_PROCESS_STATES
{
	ME_PROCESS_STATE_IDLE = 0,
	ME_PROCESS_STATE_SETUP_PAGE,
	ME_PROCESS_STATE_INIT_PANE,
	ME_PROCESS_STATE_JUMP_PANE,
	ME_PROCESS_STATE_GO_BACK_PANE,
	ME_PROCESS_STATE_TRIGGER_ACTION,
	ME_PROCESS_STATE_DEPENDENT_ACTION,

	MAX_ME_PROCESS_STATEs
};

enum eME_INPUT_STATES
{
	ME_INPUT_IDLE = 0,
	ME_INPUT_UP,
	ME_INPUT_DOWN,
	ME_INPUT_LEFT,
	ME_INPUT_RIGHT,
	ME_INPUT_ACCEPT,
	ME_INPUT_BACK,

	MAX_ME_INPUT_STATEs
};

enum eCOL_NUM
{
	COL_1 = 0,
	COL_2,
	COL_3,

	MAX_COL_NUMs
};

enum eCOL_TYPE  // must match ActionScript list in EDITOR.as
{
	COL_TYPE_INVALID = -1,

	COL_TYPE_LIST = 0,
	COL_TYPE_BASIC_PAGE,
	COL_TYPE_LOAD_PROJ_INFO,
	COL_TYPE_IMG_PROJ_INFO,
	COL_TYPE_IMG_ONE,
	COL_TYPE_IMG_FOUR,
	COL_TYPE_IMG_TWELVE,
	COL_TYPE_TEXT_PLACEMENT,
	COL_TYPE_LIST_LONG_AUDIO,
	MAX_COL_TYPE
};


enum eCOL_DATA_TYPE  // type of data which will populate this column
{
	COL_DATA_TYPE_STANDARD = 0,
	COL_DATA_TYPE_LOAD_FILES,
	COL_DATA_TYPE_AUDIO_LIST,

	MAX_COL_DATA_TYPE
};




class CVideoEditorMenuOption
{
public:

	CVideoEditorMenuOption() { UniqueId.Clear(); ToggleValue.Clear(); TriggerAction.Clear(); }

	atHashString cTextId;
	atHashString Context;
	atHashString Block;
	atHashString LinkMenuId;
	atHashString JumpMenuId;
	atHashString TriggerAction;
	atHashString ToggleValue;
	atHashString WarningText;
	atHashString DependentAction;
	atHashString UniqueId;

	PAR_SIMPLE_PARSABLE;
};

class CVideoEditorMenuBasicPage
{
public:

	atHashString Header;
	atHashString Body;
	atHashString Block;

	PAR_SIMPLE_PARSABLE;
};


class CVideoEditorMenuItem
{
public:

	CVideoEditorMenuItem()
	{
		iLastKnownSelectedItem = 0;
		columnDataType = COL_DATA_TYPE_STANDARD;
	}

	~CVideoEditorMenuItem()
	{
		delete m_pContent;
	}

	atHashString MenuId;
	eCOL_NUM columnId;
	eCOL_TYPE columnType;
	eCOL_DATA_TYPE columnDataType;
	atHashString BackMenuId;

	atArray<CVideoEditorMenuOption> Option;

	CVideoEditorMenuBasicPage *m_pContent;

	s32 iLastKnownSelectedItem;

	PAR_SIMPLE_PARSABLE;
};


class CVideoEditorErrorMessage
{
public:
	atHashString type;
	atHashString header;
	atHashString body;

	PAR_SIMPLE_PARSABLE;
};

class CVideoEditorPositions
{
public:
	atString name;
	float pos_x;
	float pos_y;
	float size_x;
	float size_y;

	PAR_SIMPLE_PARSABLE;
};

class CVideoEditorMenuArray
{
public:
	atArray<CVideoEditorMenuItem> CVideoEditorMenuItems;
	atArray<CVideoEditorErrorMessage> CVideoEditorErrorMessages;
	atArray<CVideoEditorPositions> CVideoEditorPositioning;

	PAR_SIMPLE_PARSABLE;
};





class CVideoEditorPreviewData
{
	public:

	atString m_thumbnailTxdName;
	atString m_thumbnailTextureName;
	atString m_title;
	atString m_date;
    u64 m_fileSize;
    u64 m_ownerId;
	u32 m_audioCategoryIndex;
	u32 m_soundHash;
	u32 m_durationMs;
	s32 m_fileViewIndex;
	bool m_usedInProject;
	bool m_favourite;
    bool m_flaggedForDelete;
	u8 m_trafficLight;

    bool BelongsToCurrentUser() const;
};



class CVideoEditorUserConfirmationData
{
public:
	void Set(atHashString hash, atHashString bodyText, u64 const type = FE_WARNING_YES_NO, char const* literalString = NULL, char const* literalString2 = NULL, bool const allowSpinner = false ) { m_WaitOnUserConfirmationScreen = hash; m_cHeaderText = atStringHash("VEUI_HDR_ALERT"); m_cBodyText = bodyText; m_type = type; m_allowSpinner = allowSpinner; m_literalString = literalString; m_literalString2 = literalString2; }
	void SetWithHeader(atHashString hash, atHashString bodyText, atHashString headerText, u64 const type = FE_WARNING_YES_NO, char const* literalString = NULL, char const* literalString2 = NULL, bool const allowSpinner = false ) { m_WaitOnUserConfirmationScreen = hash; m_cHeaderText = headerText; m_cBodyText = bodyText; m_type = type; m_allowSpinner = allowSpinner; m_literalString = literalString; m_literalString2 = literalString2; }
	
	void Set(atHashString bodyText, u64 const type = FE_WARNING_OK, char const* literalString = NULL, char const* literalString2 = NULL, bool const allowSpinner = false ) { m_WaitOnUserConfirmationScreen.Clear(); m_cBodyText = bodyText; m_cHeaderText = atStringHash("VEUI_HDR_ALERT"); m_type = type; m_allowSpinner = allowSpinner; m_literalString = literalString; m_literalString2 = literalString2; }
	void SetWithHeader(atHashString bodyText, atHashString headerText, u64 const type = FE_WARNING_OK, char const* literalString = NULL, char const* literalString2 = NULL, bool const allowSpinner = false ) { m_WaitOnUserConfirmationScreen.Clear(); m_cBodyText = bodyText; m_cHeaderText = headerText; m_type = type; m_allowSpinner = allowSpinner; m_literalString = literalString; m_literalString2 = literalString2; }

	bool IsActive() { return m_WaitOnUserConfirmationScreen.IsNotNull() || m_cBodyText.IsNotNull(); }
	void Reset() { m_WaitOnUserConfirmationScreen.Clear(); m_cBodyText.Clear(); m_cHeaderText.Clear(); m_type = 0; m_allowSpinner = false; m_literalString = NULL; }
	atHashString GetHash() { return m_WaitOnUserConfirmationScreen; }
	atHashString GetBodyText() { return m_cBodyText; } 
	atHashString GetHeaderText() { return m_cHeaderText; } 
	char const* GetLiteralString() const { return m_literalString; }
	char const* GetLiteralString2() const { return m_literalString2; }
	u64 GetType() const { return m_type; }
	bool GetAllowSpinner() const { return m_allowSpinner; }

private:

	atHashString m_WaitOnUserConfirmationScreen;
	atHashString m_cBodyText;
	atHashString m_cHeaderText;
	char const * m_literalString;
	char const * m_literalString2;
	u64			 m_type;
	bool		 m_allowSpinner;
};

class CVideoEditorMenu
{
	enum eGoToRebuildBehaviour
	{
		GOTO_REBUILD_NONE,					// Don't rebuild the column at all
		GOTO_REBUILD_AUTO,					// Rebuild what is necessary (may be nothing)
		GOTO_REBUILD_PREVIEW_PANE,			// Rebuild at least the preview pane
	};

public:

	static void Init(unsigned initMode);
	static void Shutdown(unsigned shutdownMode);
	static bool Update();
	static void Render();

	static bool UpdateInput();

	static void Open();
    static bool IsOpen();
	static void Close();

#if __BANK
	static void InitWidgets();
	static void ShutdownWidgets();
#endif // __BANK

	static void UpdatePane();

	static void SetErrorState( const char *pHashString, char const * const literalString = NULL, const char* literalString2 = NULL );
	static void SetUserConfirmationScreen( const char *pTextLabel, const char *pTitle = NULL, u32 const type = FE_WARNING_OK, bool const allowSpinner = false, char const * const literalString = NULL, char const * const literalString2 = NULL );
	static void SetUserConfirmationScreenWithAction(const char *pAction, const char *pTextLabel, const char *pTitle = NULL, u32 const type = FE_WARNING_YES_NO, bool const allowSpinner = false, char const * const literalString = NULL, char const * const literalString2 = NULL );
	static void RebuildTimeline();

	static void GoBackOnePane(  s32 const itemIndex = -1 );
	static bool JumpToMenu(const atHashString menuHash);
	static void ExitToMainMenu();

	static bool CheckForUserConfirmationScreens();

	static bool IsGalleryMenu( s32 const menuId );
	static bool IsGalleryFilteringMenu( s32 const menuId );
	static bool IsOnGalleryFilteringMenu();
	static bool IsOnGalleryMenu();
	static bool IsOnAudioMenu();
	static bool IsOnTrackListMenu();

	static void RebuildMenu();
	static void ClearPreviewData( const s32 iColumn, bool const releaseThumbnail, bool const wipeFocus );

	static void SetupStartingPanel(const atHashString startingMenu);

	static void TurnOffMenu(const bool bClearContents);
	static void RefreshProjectHeading();

	static Vector2 GetStagePosition() { return ms_vStagePos; }
	static Vector2 GetStageSize() { return ms_vStageSize; }

	static void GreyOutCurrentColumn(const bool bGreyOut);
	static void SetItemActive( s32 const c_itemIndex );
	static void SetItemActive( s32 const c_menuId, s32 const c_colIndex, s32 const c_itemIndex );
	static void SetItemHovered( s32 const c_colIndex, s32 const c_itemIndex );
	static void UnsetAllItemsHovered();
	static void SetItemSelected( s32 const c_itemIndex );

	static void SetColumnActive(const s32 iIndex);

	static s32 GetCurrentColumn() { return ms_iCurrentColumn; }

	static void JumpPaneOnAction();

#if RSG_PC
	static void UpdateMouseEvent(const GFxValue* args);
#endif // #if RSG_PC

	static bool IsPendingDelete() { return ms_pendingDeleteView != NULL; }

	static void ForceUploadUpdate(bool expectingOnline = false) { ms_forceUpdatingUpload = true; ms_isUpdatingUpload = expectingOnline; }

private:
	static bool IsMenuSelected();

	static bool DisplayClipRestrictions( s32 const iColumn, s32 const iIndex );
	static bool SetUploadValues();
	static void UpdateUploadValues();
	static void ShowAnimatedAudioIcon(bool const c_show);
	static bool ShouldUpdatePaneOnItemChange();
	static void RebuildColumn(const s32 c_column = -1);
	static bool IsItemSelectable( s32 const iMenuId,  s32 const iColumn, s32 const iIndex );
	static bool IsItemEnabled( s32 const iMenuId,  s32 const iColumn, s32 const iIndex );
	static bool CanItemBeActioned( s32 const iMenuId,  s32 const iColumn, s32 const iIndex );
	static bool CanHighlightItem( s32 const iMenuId,  s32 const iColumn, s32 const iIndex);

	static Vector2 SetupStagePosition();
	static Vector2 SetupStageSize();

	static s32 GetFileViewIndexForDataIndex( const s32 iFileDataIndex );
	static s32 GetFileViewIndexForItem( const s32 iColumn, const s32 iItemIndex );
	static s32 GetItemIndexForFileViewIndex( const s32 iColumn, const s32 viewIndex );

	static void RequestThumbnailOfItem( const s32 iColumn, const s32 iIndex);
	static bool IsLoadingThumbnail();
	static bool IsThumbnailLoadedOrFailed();
	static void ReleaseThumbnailOfItem();

	static void AdjustToggleValue(const s32 iDirection, const atHashString ToggleHash);
	static const char *GetToggleString(atHashString const c_toggleHash, atHashString const c_UniqueId);
	static void CopyCurrentUpdateBufferToRenderState();
	static s32 BuildMenu(const s32 iIndex, const bool bBuildBranches = true, const s32 iStartingColumn = -1);
	static s32 FindMenuIndex(const atHashString theMenuId);
	static s32 FindMenuIndex(const u32 theMenuId);

	static void ReleaseDataAndViews();
	static void JumpPane();

	static s32 GetIndexToUseForAction(const s32 iIndex);
	static bool TriggerAction(const s32 iIndex);

	static bool IsProjectValid();

	static s32 ms_iMenuIdForColumn[MAX_COL_NUMs];
	static s32 ms_iCurrentItem[MAX_COL_NUMs];
	static s32 ms_iCurrentScrollOffset[MAX_COL_NUMs];
	static s32 ms_iCurrentColumn;
	static s32 ms_iColumnHoveredOver;
	static s32 ms_iClipThumbnailInUse;
	static u32 ms_lastSoundHashPlayed;
	
	static Vector2 ms_vStagePos;
	static Vector2 ms_vStageSize;
	
	static s32 ms_previewDataId[MAX_COL_NUMs];
	static s32 ms_previewDataSort[MAX_COL_NUMs];
    static s32 ms_previewDataFilter[MAX_COL_NUMs];
    static bool ms_previewDataInversion[MAX_COL_NUMs];
	static atArray<CVideoEditorPreviewData> ms_PreviewData[MAX_COL_NUMs];
	static u32 ms_inProjectToDeleteCount;
	static u32 ms_otherUserToDeleteCount;

	static bool ms_isUpdatingUpload;
	static bool ms_forceUpdatingUpload;
	static bool ms_isHighlightedVideoUploading;
	static bool ms_clipFavouritesNeedingSaved;

#if RSG_PC
	static s32 ms_mousePressedForToggle;
#endif //#if RSG_PC

#if RSG_ORBIS
	static s32 ms_userLastShownUGCMessage;
#endif

	// interface

	static void BeginAddingColumnData(const s32 iColumnId, const s32 iColumnType);
	static void EndAddingColumnData(const bool bShouldRender, const bool bUsable);
	static void UnHighlight();
	static s32 GetNumberOfItemsInColumn( s32 const menuId, s32 const c_column );
	static void GoToPreviousItem();
	static void GoToNextItem();
	static void GoToItem( s32 const c_itemIndex, eGoToRebuildBehaviour const rebuildBehaviour );
	static void GoToItem( s32 itemIndex, const s32 iColumn, eGoToRebuildBehaviour const rebuildBehaviour );

	static void AddSizeIndicator();
	static void SetupSizeIndicator( char const * const titleLngKey, float const startingValue );
	static void UpdateSizeIndicator( float const sizeValue );
	static void RepositionSizeIndicator();
	static float CalculateActiveProjectSize();
	
	static void MouseScrollList( s32 const delta );
	static void GoToPreviousColumn();
	static void GoToNextColumn();

	static void RemoveColumn(const s32 iColumnToClear);

	static void UpdateItemColouration( s32 const columnId, s32 const listIndex, s32 const itemIndex );

	static bool SortAndFilterCurrentFileView( s32 const menuId, s32 const sort, s32 const filter );
	static bool ExecuteDependentAction( s32 const menuId, s32 const action );
	static bool CheckForDependentData(const s32 iMenuId, const s32 iIndex, const s32 iColumn );
	static bool DoesDependentDataNeedRebuilt( const s32 iColumn, const s32 iMenuId, const s32 actionHash );
	static void RefreshDependentDataBuildFlags( const s32 iColumn, const s32 iMenuId );
	static bool FillInLoadProjectData(const s32 iColumn);
	static bool FillInAudioData(s32 const c_column, eReplayMusicType const c_musicType);
	static bool FillInClipData(const s32 iColumn);

	
#if defined(VIDEO_PLAYBACK_ENABLED) && VIDEO_PLAYBACK_ENABLED
	static bool FillInGalleryData(const s32 iColumn);
#endif

	static bool HasUserContentPrivileges();

	static void ActionInputUp();
	static void ActionInputDown();
	static void ActionInputLeft(const bool bStick);
	static void ActionInputRight(const bool bStick);

	static bool IsOnMainMenu();
	static bool IsOnOptionsMenu();

	static bool IsClipFilteringMenu( s32 const menuId );
	static bool IsAddClipMenu( s32 const menuId );
	static bool IsAddClipMenuOrAddClipFilterList( s32 const menuId );
	static bool IsOnClipFilteringMenu();
	static bool IsOnAddClipMenu();
	static bool IsOnAddClipMenuOrAddClipFilterList();
	static bool IsClipManagementMenu( s32 const menuId );
	static bool IsOnClipManagementMenu();
	static bool IsClipMenuClipList(const s32 iMenuId);

	static bool IsProjectFilteringMenu( s32 const menuId );
	static bool IsProjectMenu( s32 const menuId );
	static bool IsOnProjectFilteringMenu();
	static bool IsOnProjectMenu();

	static bool IsAudioTrackList( s32 const menuId );
	static bool IsOnAudioTrackList();
	static bool IsAudioCategoryList( s32 const menuId );
	static bool IsOnAudioCategoryList();

	static bool IsOnMouseScrollableMenu() 
	{
		return IsOnProjectMenu() || IsOnAddClipMenu() || IsOnClipManagementMenu() || IsOnGalleryMenu() || IsOnTrackListMenu();
	}

	static bool IsFileMenu( s32 const menuId ) 
	{
		return IsAddClipMenu( menuId ) || IsClipManagementMenu( menuId ) || IsGalleryMenu( menuId ) || IsProjectMenu( menuId );
	}

	static bool IsFileFilteringMenu( s32 const menuId ) 
	{
		return IsClipFilteringMenu( menuId ) || IsGalleryFilteringMenu( menuId ) || IsProjectFilteringMenu( menuId );
	}

    static bool CanActionFile( s32 const iColumn, s32 const iIndex );
    static bool CanActionCurrentFile();
	static s32 GenerateMenuId( s32 const menuId, s32 const startingId );

	static void GenerateListFooterText( SimpleString_32& out_buffer, s32 const c_columnId );
	static void UpdateFooterText();

	static CFileViewFilter* GetFileViewFilterForMenu( s32 const menuId );
	static CThumbnailFileView* GetThumbnailViewForMenu( s32 const menuId );
	static CThumbnailFileView* GetCurrentThumbnailView();
	static CThumbnailFileView* GetThumbnailViewForPreviewData( s32 const previewId );

	static void ActionSortInverseRequest();
	static void SetCurrentMenuSortInverse( bool const inverse );
	static void ToggleCurrentMenuSortInverse();
	static bool IsCurrentMenuSortInverse();

	static bool ToggleClipAsFavourite( s32 const columnIndex, s32 const listIndex, s32 const itemIndex );
	static void SaveClipFavourites();

	static bool ToggleClipAsDelete( s32 const columnIndex, s32 const listIndex, s32 const itemIndex );

	static bool IsOnMarkedForDeleteFilter();
	static bool IsMultiDelete();

	static bool CheckForClips();
	static bool CheckForProjects();
	static bool CheckForVideos();
	static bool LaunchTutorialsUrl();

#if RSG_PC
	static bool ToggleOptionWithMouseViaArrows();
#endif // RSG_PC

	static CVideoEditorUserConfirmationData		ms_UserConfirmationScreen;
	static CThumbnailFileView const*			ms_pendingDeleteView;
	static bool									ms_multiDeleteLastUsed;


protected:
	friend class CVideoEditorInstructionalButtons;
	friend class CTextTemplate;

	static void UpdateInstructionalButtons( CScaleformMovieWrapper& buttonsMovie );
    static CVideoEditorPreviewData const* GetPreviewData( s32 const iColumn, s32 const itemIndex );
	static void ClearPreviewDataInternal(const s32 iColumn, bool const wipeFocus);

    static void InvertPreviewDataInternal( const s32 iColumn );

	static s32 GetMenuId( s32 const columnIndex ) { return ms_iMenuIdForColumn[ columnIndex ]; }
	static s32 GetCurrentMenuId() { return GetMenuId( ms_iCurrentColumn ); }

	friend class CVideoEditorUi;

	static CVideoEditorMenuArray ms_MenuArray;

	friend class CVideoEditorTimeline;
public:
	static bool IsUserConfirmationScreenActive() { return ms_UserConfirmationScreen.IsActive(); }
};

#endif // VIDEO_EDITOR_MENU_H

#endif // #if defined( GTA_REPLAY ) && GTA_REPLAY
// eof
