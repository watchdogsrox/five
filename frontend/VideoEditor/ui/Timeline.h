/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : Timeline.h
// PURPOSE : the NG video editor timeline - header
// AUTHOR  : Derek Payne
// STARTED : 18/02/2014
//
/////////////////////////////////////////////////////////////////////////////////
#include "control/replay/ReplaySettings.h"

#if defined( GTA_REPLAY ) && GTA_REPLAY

#ifndef VIDEO_EDITOR_TIMELINE_H
#define VIDEO_EDITOR_TIMELINE_H

#include "atl/hashstring.h"

#include "Frontend/Scaleform/ScaleFormComplexObjectMgr.h"
#include "frontend/VideoEditor/Core/VideoEditorHelpers.h"
#include "frontend/VideoEditor/ui/Menu.h"
#include "frontend/VideoEditor/ui/TextTemplate.h"
#include "frontend/VideoEditor/ui/ThumbnailManager.h"


enum
{
	TIMELINE_OBJECT_STAGE_ROOT = 0,

	TIMELINE_OBJECT_TIMECODE,
	TIMELINE_OBJECT_TIMELINE_PANEL,
	TIMELINE_OBJECT_TIMELINE_CONTAINER,
	TIMELINE_OBJECT_TIMELINE_BACKGROUND,
	TIMELINE_OBJECT_OVERVIEW_CONTAINER,
	TIMELINE_OBJECT_OVERVIEW_CLIP_CONTAINER,
	TIMELINE_OBJECT_OVERVIEW_SCROLLER,
	TIMELINE_OBJECT_OVERVIEW_SCROLLER_BOUNDARY_LEFT,
	TIMELINE_OBJECT_OVERVIEW_SCROLLER_BOUNDARY_RIGHT,
	TIMELINE_OBJECT_OVERVIEW_BACKGROUND,
	TIMELINE_OBJECT_PLAYHEAD_THUMBNAIL,
	TIMELINE_OBJECT_STAGE_CONTAINER_PREV,
	TIMELINE_OBJECT_STAGE_CONTAINER,
	TIMELINE_OBJECT_STAGE_CONTAINER_NEXT,
	TIMELINE_OBJECT_STAGE_PREV,
	TIMELINE_OBJECT_STAGE,
	TIMELINE_OBJECT_STAGE_NEXT,
	TIMELINE_OBJECT_STAGE_TEXT_LEFT,
	TIMELINE_OBJECT_STAGE_TEXT_RIGHT,
	TIMELINE_OBJECT_LEFT_ARROW,
	TIMELINE_OBJECT_RIGHT_ARROW,
	TIMELINE_OBJECT_PLAYHEAD_MARKER,
	TIMELINE_OBJECT_PLAYHEAD_MARKER_TIMECODE,
	TIMELINE_OBJECT_PLAYHEAD_MARKER_TIMECODE_BACKGROUND,
	TIMELINE_OBJECT_PLAYHEAD_MARKER_TIMECODE_TEXT,
	TIMELINE_OBJECT_PLAYHEAD_MARKER_BACKGROUND,
	TIMELINE_OBJECT_PLAYHEAD_MARKER_LINE,
	TIMELINE_OBJECT_PLAYHEAD_TRIMMING_ICON,

	MAX_TIMELINE_COMPLEX_OBJECTS
};



enum eTIMELINE_CONSTRUCTION
{
	TIMELINE_IDLE = 0,
	TIMELINE_INIT,

	MAX_TIMELINE_CONSTRUCTION
};


enum
{
	STAGE_CLIP_PREV = 0,
	STAGE_CLIP_CENTER,
	STAGE_CLIP_NEXT,
	MAX_STAGE_CLIPS
};


class CVideoEditorTimelineThumbnailManager : public CVideoEditorThumbnailManager
{
	s32 GetNonVisibleBufferMs() const;
	u32 GetActiveListCount() const;
	sTimelineItem *GetActiveListItem(const u32 uIndex) const;
	void SetupThumbnail(const u32 c_projectIndex) const;
	void ReportFailed(const u32 c_projectIndex) const;
	s32 GetLocalIndexFromProjectIndex(const u32 c_projectIndex) const;
	void SetSpinner(const u32 c_projectIndex, const bool bOnOff) const;
	bool CheckForExcludedClip(s32 const c_projectIndex) const;
	s32 GetWindowPosMs() const;
	s32 GetWindowSizeMs() const;
	bool CanMakeRequests() const;
};



class CVideoEditorTimeline
{
public:
	CVideoEditorTimeline();

	static void Init();
	static void Shutdown();
	static void Update();

	static void UpdateInput();

	static bool IsActive() { return ms_bActive; }

	static void ReleaseThumbnail( u32 const c_projectIndex );
	static void RebuildAnchors();
	static bool RebuildTimeline();

	static void InsertVideoClipIntoTimeline(u32 const c_projectIndex);
	static void InsertAmbientClipIntoTimeline(CAmbientClipHandle const &c_handle);
	static void InsertMusicClipIntoTimeline(CMusicClipHandle const &handle);
	static void InsertTextClipIntoTimeline(CTextOverlayHandle const &handle);

	static void DeleteVideoClipFromTimeline(u32 const c_projectIndex);
	static void DeleteAmbientClipFromTimeline(const u32 c_projectIndex);
	static void DeleteMusicClipFromTimeline(const u32 c_projectIndex);
	static void DeleteTextClipFromTimeline(const u32 c_projectIndex);

	static s32 FindNearestAnchor(const u32 uStartPosMs, const u32 uEndPosMs);
	static void UpdateClipsOutsideProjectTime();
	static void DropPreviewClipAtPosition(u32 positionMs, bool const c_revertedClip);
	static void TriggerOverwriteClip();
	static bool PlaceAmbientOntoTimeline(u32 const c_positionMs);
	static bool PlaceAudioOntoTimeline(u32 const c_positionMs);
	static bool PlaceEditedTextOntoTimeline(const u32 uPositionMs);
	static void SetPlayheadToStartOfProjectClip(ePLAYHEAD_TYPE const c_type, u32 const c_projectIndex, bool const c_readjustTimeline);
	static u32 GetPlayheadPositionMs();
	static u32 GetMousePositionMs();
	static void AdjustPlayheadFromAxis(const float fAxis);

	static s32 GetProjectIndexAtTime(ePLAYHEAD_TYPE const c_type, u32 const c_timeMs);
	static s32 GetProjectIndexBetweenTime(ePLAYHEAD_TYPE const c_type, u32 const c_startTimeMs, u32 const c_endTimeMs);
	static u32 GetProjectIndexFromTimelineIndex(u32 const c_timelineIndex);
	static s32 GetProjectIndexNearestStartPos(ePLAYHEAD_TYPE const c_type, u32 const c_timeMs, s32 const c_dir = 0, s32 const c_indexToIgnore = -1, bool const c_checkHalfwayPosition = false);

	static void ShowPreviewStage(const bool bActive);
	static void SetTimelineSelected(const bool bActive);
	static bool IsTimelineSelected() { return ms_bSelected; }
	
	static bool UpdateStageTexture(s32 const c_stageClip, u32 const c_projectIndex);
	static bool HasStageTextureBeenRequested(s32 const c_stageClip);
	static bool IsRequestedStageTextureLoaded(s32 const c_stageClip);
	static bool IsRequestedStageTextureFailed(s32 const c_stageClip);
	static bool IsStageTextureReady(s32 const c_stageClip);
	static bool ShouldRenderOverlayOverCentreStageClip();

	static void AddVideoToPlayheadPreview( const char *pThumbnailTxdName, const char * pThumbnailFilename );
	static void AddAmbientToPlayheadPreview();
	static void AddAudioToPlayheadPreview();
	static void AddTextToPlayheadPreview();
	static void HidePlayheadPreview();

	static void SetCurrentTextOverlayIndex(s32 const c_index) { sm_currentOverlayTextIndexInUse = c_index; }

	static float GetPosOfObject(const s32 iClipObjectId, const char *pLinkageName);

	static void AddVideo(const u32 c_projectIndex);
	static void AddAmbient(const u32 c_projectIndex, CAmbientClipHandle const &c_clip);
	static void AddAudio(const u32 c_projectIndex, CMusicClipHandle const &MusicClip);
	static void AddText(const u32 c_projectIndex, CTextOverlayHandle const &OverlayText);
	static void AddAnchor(const u32 c_projectIndex, const u32 uTimeMs);
	static void AddGhostTrack(u32 const c_projectIndex);
	static void UpdateProjectSpaceIndicator();
	static void ClearProjectSpaceIndicator();

	static bool IsTimelineItemStandard(ePLAYHEAD_TYPE const c_type) { return (c_type == PLAYHEAD_TYPE_VIDEO || c_type == PLAYHEAD_TYPE_AMBIENT || c_type == PLAYHEAD_TYPE_AUDIO || c_type == PLAYHEAD_TYPE_TEXT); }
	static bool IsTimelineItemNonVideo(ePLAYHEAD_TYPE const c_type) { return (c_type == PLAYHEAD_TYPE_AMBIENT || c_type == PLAYHEAD_TYPE_AUDIO || c_type == PLAYHEAD_TYPE_TEXT); }
    static bool IsTimelineItemAudioOrAmbient(ePLAYHEAD_TYPE const c_type) { return (c_type == PLAYHEAD_TYPE_AMBIENT || c_type == PLAYHEAD_TYPE_AUDIO); }
    static bool IsTimelineItemAudio(ePLAYHEAD_TYPE const c_type) { return c_type == PLAYHEAD_TYPE_AUDIO; }
	static bool IsPreviewing() { return (IsPreviewingVideo() || IsPreviewingAmbientTrack() || IsPreviewingMusicTrack() || IsPreviewingText()); }
	static bool IsPreviewingNonVideo() { return (IsPreviewingAmbientTrack() || IsPreviewingMusicTrack() || IsPreviewingText()); }
	static bool IsPreviewingAudioTrack() { return (IsPreviewingAmbientTrack() || IsPreviewingMusicTrack()); }

	static bool IsPreviewingVideo() { return ms_PlayheadPreview == PLAYHEAD_TYPE_VIDEO; }
	static bool IsPreviewingAmbientTrack() { return ms_PlayheadPreview == PLAYHEAD_TYPE_AMBIENT; }
	static bool IsPreviewingMusicTrack() { return ms_PlayheadPreview == PLAYHEAD_TYPE_AUDIO; }
	static bool IsPreviewingText() { return ms_PlayheadPreview == PLAYHEAD_TYPE_TEXT; }

	static void ProcessOverlayOverCentreStageClipAtPlayheadPosition();

#if USE_CODE_TEXT
	static void RenderStageText(const char *pText, Vector2 const& c_pos, Vector2 const& c_scale, CRGBA const& c_color, s32 const c_style, Vector2 const& c_stagePos, Vector2 const& c_stageSize);
#endif // #if USE_CODE_TEXT

	static u32 AdjustToNearestDroppablePosition(u32 positionMs);

	static void ClearAllClips();
	static void ClearAllAnchors();

	static CVideoEditorTimelineThumbnailManager ms_TimelineThumbnailMgr;

	static Vector2 GetStagePosition() { return ms_vStagePos; }
	static Vector2 GetStageSize() { return ms_vStageSize; }
	static bool SetupStagePreviewClip(u32 const c_positionMs);
	static bool ReleaseAllStageClipThumbnailsIfNeeded();
	static void ClearAllStageClips(bool const c_releaseClip);
	static void RepositionPlayheadIfNeeded(bool const c_clipIsBeingDeleted);
	static ePLAYHEAD_TYPE GetItemSelected() { return ms_ItemSelected; }

#if RSG_PC
	static void UpdateMouseEvent(const GFxValue* args);
#endif // #if RSG_PC

private:
	static void UpdateProjectMaxIndicator();
	static s32 GetMouseMovementDirInTimelineArea();
	static bool IsMouseWithinTimelineArea();
	static bool IsMouseWithinOverviewArea();
	static bool IsMouseWithinScrollerArea();

	static void SetPlayheadToStartOfNearestValidClipOfAnyType(u32 const c_positionMs);

	static bool IsClipValid(ePLAYHEAD_TYPE const c_type, s32 const c_projIndex);
	static s32 GetMaxClipCount(ePLAYHEAD_TYPE const c_type);
	static s32 GetStartTimeOfClipMs(ePLAYHEAD_TYPE const c_type, s32 const c_projIndex);
	static s32 GetEndTimeOfClipMs(ePLAYHEAD_TYPE const c_type, s32 const c_projIndex);
	static u32 GetDurationOfClipMs(ePLAYHEAD_TYPE const c_type, s32 const c_projIndex);
	static s32 FindNearestAnchor(s32 const c_currentPosMs, s32 const c_dir);
	static s32 FindNearestSnapPoint(ePLAYHEAD_TYPE const c_type, s32 const c_currentPosMs, s32 const c_dir);
	static u32 AdjustToNearestSnapPoint(u32 const c_currentPosMs, s32 const c_dir, bool const c_precision);
	static void UpdateInputInternal();
	static void UpdateOverviewBar();
	static bool TrimObject(ePLAYHEAD_TYPE c_type, u32 const c_projectIndex, s32 const c_trimDir, bool const c_trimmingStart, bool const c_snap, s32 const c_directMovePosMs);
	static bool UpdateAdjustableTextProperties(sTimelineItem *pTimelineObject, sTimelineItem *pOverviewBarObject, CTextOverlayHandle const &OverlayText);
	static bool UpdateAdjustableAmbientProperties(sTimelineItem *pTimelineObject, sTimelineItem *pOverviewBarObject, CAmbientClipHandle const &clip);
	static bool UpdateAdjustableAudioProperties(sTimelineItem *pTimelineObject, sTimelineItem *pOverviewBarObject, CMusicClipHandle const &music);

	static bool SetupIndividualStagePreviewClip(s32 const c_stageClip, u32 const c_projectIndex);
	static void ClearStageClip(s32 const c_stageClip, bool const c_releaseClip);

	static CVideoEditorProject *GetProject() { return CVideoEditorUi::GetProject(); }
	static bool SetupStageHelpText();
	static u32 GetTotalTimelineTimeMs(const bool c_ignoreCap = false);

	inline static float MsToPixels(const u32 uTimeInMs);
	inline static s32 PixelsToMs(const float fPixels);

	static bool SetItemSelected(ePLAYHEAD_TYPE newItemSelected);
	static void RefreshStagingArea() { ms_stagingClipNeedsUpdated = true; }

	static Vector2 SetupStagePosition();
	static Vector2 SetupStageSize();
	static void SetTimelineHighlighting(const bool bActive);
	static void HighlightClip(u32 const c_timelineIndex, bool highlight);
	static bool CanPreviewClipBeDroppedAtPosition(u32 const c_positionMs, s32 const c_indexToIgnore = -1);
	static Color32 GetColourForPlayheadType(u32 const c_positionMs, ePLAYHEAD_TYPE const c_type, u8 const c_alpha = 255, bool const c_forceColor = false);
	static bool IsPlayheadAudioScore(u32 const c_positionMs, ePLAYHEAD_TYPE const c_type);
	static bool IsPlayheadCommercial(u32 const c_positionMs, ePLAYHEAD_TYPE const c_type);
	static void UpdatePlayheadPreview(const ePLAYHEAD_TYPE type);
	static void UpdateDroppedRevertedClip();
	static void UpdateItemsHighlighted();
	static void AdjustPlayhead(float deltaPixels, bool const c_readjustTimeline);
	static u32 GetNumberedIndexOfClipTypeOnTimeline(const u32 uNumber, const ePLAYHEAD_TYPE findType);
	static u32 GetSelectedClipObject(const u32 c_projectIndex, const ePLAYHEAD_TYPE type);
	static u32 GetUniqueClipDepth();
	static void DeleteClip(const u32 uTimelineIndex);

#if RSG_PC
	static bool UpdateMouseInput();
#endif // #if RSG_PC


	static bool SetupTextureOnComplexObject(const u32 c_projectIndex);
	static bool RemoveTextureOnComplexObject(const u32 c_projectIndex);
	static void UnRequestThumbnail(const u32 uThumbnailIndex);
	static s32 GetFirstThumbnailAwaitingSetup();
	static s32 GetFirstThumbnailNotNeeded();
	static void RequestAndUnrequestThumbnails();
	static void CullTexturesNotInVisibleWindow();

	static u32 GetFirstClipVisibility(const u32 uStartingProjectIndex, const bool bSearchForVisible);
	static void CheckForVisibility(u32 *pStart, u32 *pEnd);

	static s32 FindTimelineIndexFromHashName(const ePLAYHEAD_TYPE type, const atHashString nameHash);

#if __BANK
	static void OutputTimelineDebug(const u32 iCallingIndex);
#endif // __BANK

#if RSG_PC
	static u32 ms_heldDownTimer;
	static bool ms_waitForMouseMovement;
	static s32 ms_mousePressedIndexFromActionscript;
	static s32 ms_mouseReleasedIndexFromActionscript;
	static s32 ms_mouseHoveredIndexFromActionscript;
	static ePLAYHEAD_TYPE ms_mousePressedTypeFromActionscript;
	static ePLAYHEAD_TYPE ms_mouseReleasedTypeFromActionscript;
#endif // #if RSG_PC

	static u32 ms_uPreviousTimelineIndexHovered;
	static u32 ms_uClipAttachDepth;
	static bool ms_bActive;
	static bool ms_bSelected;
	static ePLAYHEAD_TYPE ms_ItemSelected;
	static bool ms_stagingClipNeedsUpdated;
	static Vector2 ms_vStagePos;
	static Vector2 ms_vStageSize;
	static ePLAYHEAD_TYPE ms_PlayheadPreview;
	static bool ms_bHaveProjectToRevert;
	static bool ms_droppedByMouse;
	static u32 ms_uProjectPositionToRevertMs;
	static s32 ms_iProjectPositionToRevertOnUpMs;
	static s32 ms_iProjectPositionToRevertOnDownMs;
	static u32 ms_maxPreviewAreaMs;
	static s32 ms_iScrollTimeline;
	static s32 ms_iStageClipThumbnailInUse[MAX_STAGE_CLIPS];
	static s32 ms_iStageClipThumbnailRequested[MAX_STAGE_CLIPS];
	static s32 ms_iStageClipThumbnailToBeReleased[MAX_STAGE_CLIPS];
	static bool ms_renderOverlayOverCentreStageClip;
	static bool ms_heldDownAndScrubbingTimeline;
	static bool ms_heldDownAndScrubbingOverviewBar;
	static u32 ms_stickTimerAcc;
	static bool ms_wantScrubbingSoundThisFrame;
	static bool ms_trimmingClip;
	static bool ms_trimmingClipLeft;
	static bool ms_trimmingClipRight;
	static s32 ms_trimmingClipOriginalPositionMs;
	static s32 ms_trimmingClipIncrementMultiplier;
	static s32 ms_previewingAudioTrack;
	static s32 sm_currentOverlayTextIndexInUse;

protected:
	friend class CVideoEditorInstructionalButtons;
	static void UpdateInstructionalButtons( CScaleformMovieWrapper& buttonsMovie );

	friend class CVideoEditorTimelineThumbnailManager;
	friend class CVideoEditorMenu;
	friend class CVideoEditorUi;

	static void SetMaxPreviewAreaMs(s32 const c_posMs) { c_posMs > 0 ? ms_maxPreviewAreaMs = c_posMs : 0; }
	static void SetPlayheadToStartOfNearestValidClip();
	static bool IsScrollingFast();
	static bool IsScrollingMediumFast();
	static void SetPlayheadPositionMs(const u32 uNewPlayheadPosMs, const bool bReadjustTimeline);
	static void SetTimeOnPlayhead(const bool bShow = true);
	static atArray<sTimelineItem> ms_ComplexObjectClips;
	static s32 GetTimelineIndexFromProjectIndex(ePLAYHEAD_TYPE const c_type, u32 const c_projectIndex);
	static void SetTimelineObjectSpinner(const u32 uTimelineIndex, const bool bOnOff);
	static CComplexObject ms_ComplexObject[MAX_TIMELINE_COMPLEX_OBJECTS];
	static void AddTextureToVideoClip(const u32 iProjectIndex);
	static void ReportTextureFailedToVideoClip(const u32 c_projectIndex);
};




#endif // VIDEO_EDITOR_TIMELINE_H

#endif // #if defined( GTA_REPLAY ) && GTA_REPLAY

// eof


