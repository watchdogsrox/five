/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : Timeline.cpp
// PURPOSE : the NG video editor timeline
// AUTHOR  : Derek Payne
// STARTED : 18/02/2014
// NOTES   : 
//
/////////////////////////////////////////////////////////////////////////////////
#include "frontend/VideoEditor/ui/Timeline.h"

#if defined( GTA_REPLAY ) && GTA_REPLAY

// framework
#include "grcore/stateblock.h"
#include "input/mouse.h"
#include "parser/manager.h"
#include "fwlocalisation/textUtil.h"
#include "fwlocalisation/templateString.h"
#include "math/angmath.h"

// game
#include "audio/frontendaudioentity.h"
#include "audio/radioaudioentity.h"
#include "frontend/hud_colour.h"
#include "frontend/HudTools.h"
#include "frontend/MousePointer.h"
#include "frontend/PauseMenuData.h"
#include "frontend/VideoEditor/Core/VideoProjectAudioTrackProvider.h"
#include "frontend/VideoEditor/ui/Editor.h"
#include "frontend/VideoEditor/ui/Menu.h"
#include "frontend/VideoEditor/ui/InstructionalButtons.h"
#include "frontend/VideoEditor/VideoEditorInterface.h"
#include "frontend/VideoEditor/ui/TextTemplate.h"
#include "frontend/VideoEditor/ui/ThumbnailManager.h"
#include "Frontend/ui_channel.h"
#include "frontend/Scaleform/ScaleFormMgr.h"
#include "frontend/Scaleform/ScaleFormComplexObjectMgr.h"
#include "renderer/PostProcessFXHelper.h"
#include "replaycoordinator/Storage/MontageMusicClip.h"
#include "replaycoordinator/Storage/MontageText.h"
#include "System/controlMgr.h"
#include "System/keyboard.h"
#include "system/system.h"
#include "text/TextFormat.h"
#include "text/TextConversion.h"

FRONTEND_OPTIMISATIONS();
//OPTIMISATIONS_OFF()

#define TIMELINE_POSITION_X (0.161f)  // position of the timeline container based on design from Wiki
#define TIMELINE_POSITION_Y (0.222f)

#define TIMELINE_AXIS_THRESHOLD				( 0.625f )
#define TIMELINE_MAX_INPUT_ACC				(250)
#define	TIMELINE_INPUT_ACC_NO_TEXTURES		(220)
#define TIMELINE_MIN_INPUT_ACC				(10)
#define TIMELINE_MIN_INPUT_DELTA			(10)

#define AMOUNT_OF_MOUSE_MOVEMENT_BUFFER_AT_SIDES (0.15f * ACTIONSCRIPT_STAGE_SIZE_X)
#define AMOUNT_OF_SIDE_VISIBILITY (AMOUNT_OF_MOUSE_MOVEMENT_BUFFER_AT_SIDES * 0.5f)

#define TIMELINE_DEFAULT_DEPTH_GHOST		(VE_DEPTH_SAFE_WRAP_START)
#define TIMELINE_DEFAULT_DEPTH_START		(TIMELINE_DEFAULT_DEPTH_GHOST + 1)
#define TIMELINE_DEFAULT_DEPTH_END			(1000000)  // Recognised Actionscript limit of 1048575 
#define TIMELINE_DEFAULT_DEPTH_CUT_OFF		(TIMELINE_DEFAULT_DEPTH_END + 1)

#define OVERVIEW_BAR_YPOS_VIDEO		(4)
#define OVERVIEW_BAR_YPOS_AMBIENT	(11)
#define OVERVIEW_BAR_YPOS_AUDIO		(18)
#define OVERVIEW_BAR_YPOS_TEXT		(25)

#define TRIMMING_ICON_YPOS_AMBIENT	(-42.0f)
#define TRIMMING_ICON_YPOS_AUDIO	(-15.0f)
#define TRIMMING_ICON_YPOS_TEXT		(12.0f)



CComplexObject CVideoEditorTimeline::ms_ComplexObject[MAX_TIMELINE_COMPLEX_OBJECTS];
atArray<sTimelineItem> CVideoEditorTimeline::ms_ComplexObjectClips;

bool CVideoEditorTimeline::ms_bActive = false;
bool CVideoEditorTimeline::ms_bSelected = false;
ePLAYHEAD_TYPE CVideoEditorTimeline::ms_ItemSelected = PLAYHEAD_TYPE_VIDEO;
bool CVideoEditorTimeline::ms_stagingClipNeedsUpdated = false;
Vector2 CVideoEditorTimeline::ms_vStagePos;
Vector2 CVideoEditorTimeline::ms_vStageSize;
bool CVideoEditorTimeline::ms_bHaveProjectToRevert = false;
bool CVideoEditorTimeline::ms_droppedByMouse = false;
u32 CVideoEditorTimeline::ms_uProjectPositionToRevertMs = 0;
s32 CVideoEditorTimeline::ms_iProjectPositionToRevertOnUpMs = -1;
s32 CVideoEditorTimeline::ms_iProjectPositionToRevertOnDownMs = -1;
u32 CVideoEditorTimeline::ms_maxPreviewAreaMs = 0;
s32 CVideoEditorTimeline::ms_iScrollTimeline = 0;
s32 CVideoEditorTimeline::ms_iStageClipThumbnailInUse[MAX_STAGE_CLIPS];
s32 CVideoEditorTimeline::ms_iStageClipThumbnailRequested[MAX_STAGE_CLIPS];
s32 CVideoEditorTimeline::ms_iStageClipThumbnailToBeReleased[MAX_STAGE_CLIPS];
bool CVideoEditorTimeline::ms_renderOverlayOverCentreStageClip = false;
u32 CVideoEditorTimeline::ms_uPreviousTimelineIndexHovered = MAX_UINT32;
bool CVideoEditorTimeline::ms_heldDownAndScrubbingTimeline = false;
bool CVideoEditorTimeline::ms_heldDownAndScrubbingOverviewBar = false;
bool CVideoEditorTimeline::ms_trimmingClip = false;
bool CVideoEditorTimeline::ms_trimmingClipLeft = false;
bool CVideoEditorTimeline::ms_trimmingClipRight = false;
s32 CVideoEditorTimeline::ms_trimmingClipOriginalPositionMs = -1;
s32 CVideoEditorTimeline::ms_trimmingClipIncrementMultiplier = 0;
s32 CVideoEditorTimeline::ms_previewingAudioTrack = -1;
s32 CVideoEditorTimeline::sm_currentOverlayTextIndexInUse = -1;
u32 CVideoEditorTimeline::ms_stickTimerAcc = TIMELINE_MAX_INPUT_ACC;

ePLAYHEAD_TYPE CVideoEditorTimeline::ms_PlayheadPreview = PLAYHEAD_TYPE_NONE;
u32 CVideoEditorTimeline::ms_uClipAttachDepth = TIMELINE_DEFAULT_DEPTH_START;
bool CVideoEditorTimeline::ms_wantScrubbingSoundThisFrame = false;

#if RSG_PC
u32 CVideoEditorTimeline::ms_heldDownTimer = 0;
bool CVideoEditorTimeline::ms_waitForMouseMovement = false;
s32 CVideoEditorTimeline::ms_mousePressedIndexFromActionscript = -1;
s32 CVideoEditorTimeline::ms_mouseReleasedIndexFromActionscript = -1;
s32 CVideoEditorTimeline::ms_mouseHoveredIndexFromActionscript = -1;
ePLAYHEAD_TYPE CVideoEditorTimeline::ms_mousePressedTypeFromActionscript = PLAYHEAD_TYPE_NONE;
ePLAYHEAD_TYPE CVideoEditorTimeline::ms_mouseReleasedTypeFromActionscript = PLAYHEAD_TYPE_NONE;
#endif // #if RSG_PC

CVideoEditorTimelineThumbnailManager CVideoEditorTimeline::ms_TimelineThumbnailMgr;

static eHUD_COLOURS s_TimelineColours[MAX_PLAYHEAD_TYPES] = 
{
	HUD_COLOUR_INVALID, HUD_COLOUR_VIDEO_EDITOR_VIDEO, HUD_COLOUR_VIDEO_EDITOR_AMBIENT, HUD_COLOUR_VIDEO_EDITOR_AUDIO, HUD_COLOUR_VIDEO_EDITOR_TEXT
};

#define GAP_BETWEEN_BORDERS (2.0f)  // a small gap between each clip so they are easily identifiable

#define ALPHA_100_PERCENT (255)
#define ALPHA_30_PERCENT (77)  // 30% of hud colour

#define MS_TO_PIXEL_SCALE_FACTOR ( 50.f )

#define MOUSE_DEADZONE_AMOUNT_IN_MS (14.f * MS_TO_PIXEL_SCALE_FACTOR)  // 10 pixels deadzone on mouse on timeline

#define ADJUSTMENT_TIMER_SLOW (150)
#define ADJUSTMENT_TIMER_STANDARD (8)

/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorTimeline::CVideoEditorTimeline
// PURPOSE:	init values
/////////////////////////////////////////////////////////////////////////////////////////
CVideoEditorTimeline::CVideoEditorTimeline()
{
	// ensure all pointers are null at init
	for (s32 i = 0; i < MAX_TIMELINE_COMPLEX_OBJECTS; i++)
	{
		ms_ComplexObject[i].Init();
	}

	ms_ComplexObjectClips.Reset();
}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorTimeline::Init
// PURPOSE:	init values as the timeline is started up
/////////////////////////////////////////////////////////////////////////////////////////
void CVideoEditorTimeline::Init()
{
	ms_ComplexObject[TIMELINE_OBJECT_STAGE_ROOT] = CComplexObject::GetStageRoot(CVideoEditorUi::GetMovieId());

	if (ms_ComplexObject[TIMELINE_OBJECT_STAGE_ROOT].IsActive())
	{
		ms_ComplexObject[TIMELINE_OBJECT_TIMECODE] = ms_ComplexObject[TIMELINE_OBJECT_STAGE_ROOT].AttachMovieClip("TIMECODE_NEW", VE_DEPTH_TIMECODE);

		if (ms_ComplexObject[TIMELINE_OBJECT_TIMECODE].IsActive())
		{
			// position timecode on safezone
			Vector2 safeZoneMin, safeZoneMax;
			CHudTools::GetMinSafeZoneForScaleformMovies(safeZoneMin.x, safeZoneMin.y, safeZoneMax.x, safeZoneMax.y);

			ms_ComplexObject[TIMELINE_OBJECT_TIMECODE].SetPosition(Vector2(safeZoneMin.x, safeZoneMax.y-(ms_ComplexObject[TIMELINE_OBJECT_TIMECODE].GetHeight() / ACTIONSCRIPT_STAGE_SIZE_Y)));
		}

		ms_ComplexObject[TIMELINE_OBJECT_TIMELINE_PANEL] = ms_ComplexObject[TIMELINE_OBJECT_STAGE_ROOT].AttachMovieClip("TIMELINE_PANEL", VE_DEPTH_TIMELINE_PANEL);
		
		ms_ComplexObject[TIMELINE_OBJECT_TIMELINE_PANEL].SetPosition(Vector2(TIMELINE_POSITION_X, TIMELINE_POSITION_Y));

		ms_ComplexObject[TIMELINE_OBJECT_TIMELINE_CONTAINER] = ms_ComplexObject[TIMELINE_OBJECT_TIMELINE_PANEL].GetObject("TIMELINE_CONTAINER");
		ms_ComplexObject[TIMELINE_OBJECT_TIMELINE_BACKGROUND] = ms_ComplexObject[TIMELINE_OBJECT_TIMELINE_PANEL].GetObject("TIMELINE_BACKGROUND");

		ms_ComplexObject[TIMELINE_OBJECT_OVERVIEW_BACKGROUND] = ms_ComplexObject[TIMELINE_OBJECT_TIMELINE_BACKGROUND].GetObject("OVERVIEW_BACKGROUND");

		ms_ComplexObject[TIMELINE_OBJECT_OVERVIEW_CONTAINER] = ms_ComplexObject[TIMELINE_OBJECT_TIMELINE_BACKGROUND].CreateEmptyMovieClip("OVERVIEW_CONTAINER", VE_DEPTH_OVERVIEW_CONTAINER);
		ms_ComplexObject[TIMELINE_OBJECT_OVERVIEW_CLIP_CONTAINER] = ms_ComplexObject[TIMELINE_OBJECT_OVERVIEW_CONTAINER].CreateEmptyMovieClip("OVERVIEW_CLIP_CONTAINER", 1);

		if (ms_ComplexObject[TIMELINE_OBJECT_OVERVIEW_BACKGROUND].IsActive())
		{
			ms_ComplexObject[TIMELINE_OBJECT_OVERVIEW_BACKGROUND].SetAlpha(255);

			if (ms_ComplexObject[TIMELINE_OBJECT_OVERVIEW_CONTAINER].IsActive())
			{
				Vector2 const c_OverViewBackgroundPos = ms_ComplexObject[TIMELINE_OBJECT_OVERVIEW_BACKGROUND].GetPosition();
				ms_ComplexObject[TIMELINE_OBJECT_OVERVIEW_CONTAINER].SetPosition(c_OverViewBackgroundPos);		
			}
		}

		ms_ComplexObject[TIMELINE_OBJECT_OVERVIEW_SCROLLER] = ms_ComplexObject[TIMELINE_OBJECT_OVERVIEW_CONTAINER].AttachMovieClip("OVERVIEW_SCROLLER", VE_DEPTH_OVERVIEW_SCROLLER);
		ms_ComplexObject[TIMELINE_OBJECT_OVERVIEW_SCROLLER_BOUNDARY_LEFT] = ms_ComplexObject[TIMELINE_OBJECT_OVERVIEW_CONTAINER].AttachMovieClipInstance("OVERVIEW_SCROLLER_BOUNDARY", VE_DEPTH_OVERVIEW_SCROLLER+1);
		ms_ComplexObject[TIMELINE_OBJECT_OVERVIEW_SCROLLER_BOUNDARY_RIGHT] = ms_ComplexObject[TIMELINE_OBJECT_OVERVIEW_CONTAINER].AttachMovieClipInstance("OVERVIEW_SCROLLER_BOUNDARY", VE_DEPTH_OVERVIEW_SCROLLER+2);

		ms_ComplexObject[TIMELINE_OBJECT_OVERVIEW_SCROLLER_BOUNDARY_LEFT].SetPositionInPixels(ms_ComplexObject[TIMELINE_OBJECT_OVERVIEW_SCROLLER].GetPositionInPixels());
		ms_ComplexObject[TIMELINE_OBJECT_OVERVIEW_SCROLLER_BOUNDARY_RIGHT].SetPositionInPixels(Vector2(ms_ComplexObject[TIMELINE_OBJECT_OVERVIEW_SCROLLER].GetXPositionInPixels()+ms_ComplexObject[TIMELINE_OBJECT_OVERVIEW_SCROLLER].GetWidth(), ms_ComplexObject[TIMELINE_OBJECT_OVERVIEW_SCROLLER].GetYPositionInPixels()));

		ms_ComplexObject[TIMELINE_OBJECT_STAGE_CONTAINER] = ms_ComplexObject[TIMELINE_OBJECT_TIMELINE_PANEL].GetObject("STAGE");
		ms_ComplexObject[TIMELINE_OBJECT_STAGE_CONTAINER_PREV] = ms_ComplexObject[TIMELINE_OBJECT_TIMELINE_PANEL].GetObject("PREV_STAGE");
		ms_ComplexObject[TIMELINE_OBJECT_STAGE_CONTAINER_NEXT] = ms_ComplexObject[TIMELINE_OBJECT_TIMELINE_PANEL].GetObject("NEXT_STAGE");
		ms_ComplexObject[TIMELINE_OBJECT_LEFT_ARROW] = ms_ComplexObject[TIMELINE_OBJECT_TIMELINE_PANEL].GetObject("LEFT_ARROW");
		ms_ComplexObject[TIMELINE_OBJECT_RIGHT_ARROW] = ms_ComplexObject[TIMELINE_OBJECT_TIMELINE_PANEL].GetObject("RIGHT_ARROW");

		if (ms_ComplexObject[TIMELINE_OBJECT_TIMELINE_CONTAINER].IsActive())
		{
			ms_ComplexObject[TIMELINE_OBJECT_TIMELINE_CONTAINER].SetXPositionInPixels(0.0f);
		}

		u8 const c_alpha = RSG_PC ? ALPHA_30_PERCENT : 0;  // arrows only show on PC

		if (ms_ComplexObject[TIMELINE_OBJECT_LEFT_ARROW].IsActive())
		{
			ms_ComplexObject[TIMELINE_OBJECT_LEFT_ARROW].SetAlpha(c_alpha);
		}
		
		if (ms_ComplexObject[TIMELINE_OBJECT_RIGHT_ARROW].IsActive())
		{
			ms_ComplexObject[TIMELINE_OBJECT_RIGHT_ARROW].SetAlpha(c_alpha);
		}

		ms_ComplexObject[TIMELINE_OBJECT_STAGE] = ms_ComplexObject[TIMELINE_OBJECT_STAGE_CONTAINER].GetObject("CLIP");
		ms_ComplexObject[TIMELINE_OBJECT_STAGE_PREV] = ms_ComplexObject[TIMELINE_OBJECT_STAGE_CONTAINER_PREV].GetObject("CLIP");
		ms_ComplexObject[TIMELINE_OBJECT_STAGE_NEXT] = ms_ComplexObject[TIMELINE_OBJECT_STAGE_CONTAINER_NEXT].GetObject("CLIP");
		ms_ComplexObject[TIMELINE_OBJECT_STAGE_TEXT_LEFT] = ms_ComplexObject[TIMELINE_OBJECT_STAGE_CONTAINER].GetObject("MOVIE_TEXT_LEFT");
		ms_ComplexObject[TIMELINE_OBJECT_STAGE_TEXT_RIGHT] = ms_ComplexObject[TIMELINE_OBJECT_STAGE_CONTAINER].GetObject("MOVIE_TEXT_RIGHT");
		ms_ComplexObject[TIMELINE_OBJECT_PLAYHEAD_MARKER] = ms_ComplexObject[TIMELINE_OBJECT_TIMELINE_PANEL].GetObject("PLAYHEAD_MARKER");

		if (ms_ComplexObject[TIMELINE_OBJECT_PLAYHEAD_MARKER].IsActive())
		{
			ms_ComplexObject[TIMELINE_OBJECT_PLAYHEAD_MARKER_TIMECODE] = ms_ComplexObject[TIMELINE_OBJECT_PLAYHEAD_MARKER].GetObject("TEXT_TIMECODE");
			ms_ComplexObject[TIMELINE_OBJECT_PLAYHEAD_TRIMMING_ICON] = ms_ComplexObject[TIMELINE_OBJECT_PLAYHEAD_MARKER_TIMECODE].GetObject("TRIMMING_ICON");
			ms_ComplexObject[TIMELINE_OBJECT_PLAYHEAD_MARKER_TIMECODE_BACKGROUND] = ms_ComplexObject[TIMELINE_OBJECT_PLAYHEAD_MARKER_TIMECODE].GetObject("BACKGROUND");
			ms_ComplexObject[TIMELINE_OBJECT_PLAYHEAD_MARKER_TIMECODE_TEXT] = ms_ComplexObject[TIMELINE_OBJECT_PLAYHEAD_MARKER_TIMECODE].GetObject("TEXT_TIMECODE_TF");

			ms_ComplexObject[TIMELINE_OBJECT_PLAYHEAD_MARKER].SetXPositionInPixels(0.0f);
			ms_ComplexObject[TIMELINE_OBJECT_PLAYHEAD_MARKER].SetVisible(false);

			ms_ComplexObject[TIMELINE_OBJECT_PLAYHEAD_THUMBNAIL] = ms_ComplexObject[TIMELINE_OBJECT_PLAYHEAD_MARKER].GetObject("THUMBNAIL_CLIP");
		}

		if (ms_ComplexObject[TIMELINE_OBJECT_PLAYHEAD_THUMBNAIL].IsActive())
		{
			ms_ComplexObject[TIMELINE_OBJECT_PLAYHEAD_MARKER_BACKGROUND] = ms_ComplexObject[TIMELINE_OBJECT_PLAYHEAD_THUMBNAIL].GetObject("BACKGROUND");
			ms_ComplexObject[TIMELINE_OBJECT_PLAYHEAD_MARKER_LINE] = ms_ComplexObject[TIMELINE_OBJECT_PLAYHEAD_MARKER_TIMECODE_BACKGROUND].GetObject("LINE");
			ms_ComplexObject[TIMELINE_OBJECT_PLAYHEAD_THUMBNAIL].SetVisible(false);
		}

		if (ms_ComplexObject[TIMELINE_OBJECT_STAGE].IsActive())
		{
			ms_ComplexObject[TIMELINE_OBJECT_STAGE].SetVisible(true);
		}
	}

  	if (ms_ComplexObject[TIMELINE_OBJECT_TIMELINE_PANEL].IsActive())
  	{
  		ms_ComplexObject[TIMELINE_OBJECT_TIMELINE_PANEL].SetVisible(false);
  	}

	ms_ComplexObject[TIMELINE_OBJECT_STAGE_CONTAINER_PREV].SetAlpha(0);
	ms_ComplexObject[TIMELINE_OBJECT_STAGE_CONTAINER_NEXT].SetAlpha(0);

	UpdatePlayheadPreview(PLAYHEAD_TYPE_NONE);

	// store these as we need these every frame for rendering the text at the stage scale:
	ms_vStageSize = SetupStageSize();
	ms_vStagePos = SetupStagePosition();

	ms_bHaveProjectToRevert = false;
	ms_droppedByMouse = false;
	ms_uProjectPositionToRevertMs = 0;
	ms_iProjectPositionToRevertOnUpMs = -1;
	ms_iProjectPositionToRevertOnDownMs = -1;
	ms_maxPreviewAreaMs = 0;
	ms_iScrollTimeline = 0;
	ms_heldDownAndScrubbingTimeline = false;
	ms_heldDownAndScrubbingOverviewBar = false;

#if RSG_PC
	ms_waitForMouseMovement = false;
	ms_heldDownTimer = fwTimer::GetSystemTimeInMilliseconds();
#endif

	for (s32 i = 0; i < MAX_STAGE_CLIPS; i++)
	{
		ms_iStageClipThumbnailInUse[i] = -1;
		ms_iStageClipThumbnailRequested[i] = -1;
		ms_iStageClipThumbnailToBeReleased[i] = -1;
	}

	ms_ItemSelected = PLAYHEAD_TYPE_VIDEO;

	ms_TimelineThumbnailMgr.Init();

	ms_bActive = true;

	SetTimelineSelected(false);

	ms_uClipAttachDepth = TIMELINE_DEFAULT_DEPTH_START;

	ClearProjectSpaceIndicator();
}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorTimeline::Shutdown
// PURPOSE:	shuts down the timeline
/////////////////////////////////////////////////////////////////////////////////////////
void CVideoEditorTimeline::Shutdown()
{
	ms_TimelineThumbnailMgr.Shutdown();

	if (GetProject())  // release all clip thumbnails
	{
		GetProject()->ReleaseAllClipThumbnails();
	}

	// remove any clips we added:
	for (s32 i = 0; i < ms_ComplexObjectClips.GetCount(); i++)
	{
		if (ms_ComplexObjectClips[i].object.IsActive())
		{
			ms_ComplexObjectClips[i].object.Release();
		}
	}

	ms_ComplexObjectClips.Reset();

	// remove all other timeline objects:
	for (s32 i = 0; i < MAX_TIMELINE_COMPLEX_OBJECTS; i++)
	{
		if (ms_ComplexObject[i].IsActive())
		{
			ms_ComplexObject[i].Release();
		}
	}

	ms_bActive = false;
}




/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorTimeline::Update
// PURPOSE:	main timeline update - depends whether visible
/////////////////////////////////////////////////////////////////////////////////////////
void CVideoEditorTimeline::Update()
{
	if (!ms_bActive || !GetProject())
	{
		return;
	}

	if (ReleaseAllStageClipThumbnailsIfNeeded())
	{
		return;
	}

	UpdateDroppedRevertedClip();

	ms_TimelineThumbnailMgr.Update();

	UpdateItemsHighlighted();

#if USE_TEXT_CANVAS
	ProcessOverlayOverCentreStageClipAtPlayheadPosition();  // render any existing text on the stage
#endif 

	if (ms_previewingAudioTrack != -1)
	{
		if (g_RadioAudioEntity.IsReplayMusicPreviewPlaying())
		{
			u32 const c_playbackTimeLeftMs = g_RadioAudioEntity.GetReplayMusicPlaybackTimeMs();

			u32 const c_startOffsetMs = GetItemSelected() == PLAYHEAD_TYPE_AMBIENT ? 
                GetProject()->GetAmbientClip(ms_previewingAudioTrack).getStartOffsetMs() : GetProject()->GetMusicClip(ms_previewingAudioTrack).getStartOffsetMs();

			u32 const c_startTimeMs = GetStartTimeOfClipMs( GetItemSelected(), ms_previewingAudioTrack );
			u32 const c_playbackTimeMs = c_startTimeMs - c_playbackTimeLeftMs + c_startOffsetMs;

			SetPlayheadPositionMs(c_playbackTimeMs, true);

            //videoDisplayf( " CVideoEditorTimeline::Update - Audio Entity reporting preview track time left as %u", c_playbackTimeLeftMs );
		}
		else
		{
			ms_previewingAudioTrack = -1;

			SetPlayheadPositionMs(ms_uProjectPositionToRevertMs, true);

			CVideoEditorInstructionalButtons::Refresh();
		}

		return;
	}
}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorTimeline::UpdateOverviewBar
// PURPOSE: scales the overview bar to make the current project size fit neatly
/////////////////////////////////////////////////////////////////////////////////////////
void CVideoEditorTimeline::UpdateOverviewBar()
{
	if (GetProject() && ms_ComplexObject[TIMELINE_OBJECT_OVERVIEW_CONTAINER].IsActive() && ms_ComplexObject[TIMELINE_OBJECT_OVERVIEW_SCROLLER].IsActive())
	{
		const u32 c_totalTimelineTimeMs = GetTotalTimelineTimeMs(true);  // dont need the duration cap for the overview bar

		const float c_scaler = c_totalTimelineTimeMs < 1000 ? (1000.0f / c_totalTimelineTimeMs) : 1.0f;
		float const c_pixelsUnzoomed = ms_ComplexObject[TIMELINE_OBJECT_OVERVIEW_BACKGROUND].GetWidth();
		const float c_scaledPixels = c_totalTimelineTimeMs > 0 ? (c_pixelsUnzoomed / (MsToPixels(c_totalTimelineTimeMs)) * 100.0f) / c_scaler : c_pixelsUnzoomed;

		ms_ComplexObject[TIMELINE_OBJECT_OVERVIEW_CONTAINER].SetScale(Vector2(c_scaledPixels, 100.0f));
		ms_ComplexObject[TIMELINE_OBJECT_OVERVIEW_SCROLLER].SetWidth(c_scaledPixels > 100.0f ? ((100.0f/c_scaledPixels)*c_pixelsUnzoomed) : c_pixelsUnzoomed);

		const float c_scaledVerticalLineWidth = 2.0f * (100 / c_scaledPixels);
		ms_ComplexObject[TIMELINE_OBJECT_OVERVIEW_SCROLLER_BOUNDARY_LEFT].SetWidth(c_scaledVerticalLineWidth);
		ms_ComplexObject[TIMELINE_OBJECT_OVERVIEW_SCROLLER_BOUNDARY_RIGHT].SetWidth(c_scaledVerticalLineWidth);

		// need to go through each overview bar clip and readjust the size
		for (s32 i = 0; i < ms_ComplexObjectClips.GetCount(); i++)  // go through full list
		{
			if (ms_ComplexObjectClips[i].object.IsActive())  // active
			{
				u32 uClipLengthMs = 0;

				if (ms_ComplexObjectClips[i].type == PLAYHEAD_TYPE_OVERVIEW_VIDEO)
				{
					uClipLengthMs = (u32)GetProject()->GetClipDurationTimeMs(ms_ComplexObjectClips[i].uIndexToProject);
				} else if (ms_ComplexObjectClips[i].type == PLAYHEAD_TYPE_OVERVIEW_AMBIENT)
				{
					uClipLengthMs = GetProject()->GetAmbientClip(ms_ComplexObjectClips[i].uIndexToProject).getTrimmedDurationMs();
				} else if (ms_ComplexObjectClips[i].type == PLAYHEAD_TYPE_OVERVIEW_AUDIO)
				{
					uClipLengthMs = GetProject()->GetMusicClip(ms_ComplexObjectClips[i].uIndexToProject).getTrimmedDurationMs();
				} else if (ms_ComplexObjectClips[i].type == PLAYHEAD_TYPE_OVERVIEW_TEXT)
				{
					uClipLengthMs = GetProject()->GetOverlayText(ms_ComplexObjectClips[i].uIndexToProject).getDurationMs();
				}

				if (uClipLengthMs > 0)
				{
					const float c_scaledGap = GAP_BETWEEN_BORDERS * (100 / c_scaledPixels);
					const float c_adjustedWidth = MsToPixels(uClipLengthMs) - ((MsToPixels(uClipLengthMs) > c_scaledGap * 2.0f) ? c_scaledGap : 0.0f);

					ms_ComplexObjectClips[i].object.SetWidth(c_adjustedWidth);

					// highlight the item selected on the timeline
					u8 alpha = ALPHA_30_PERCENT;
					if ((!IsPreviewing() && !ms_trimmingClip && ms_previewingAudioTrack == -1) &&
						((GetItemSelected() == PLAYHEAD_TYPE_VIDEO && ms_ComplexObjectClips[i].type == PLAYHEAD_TYPE_OVERVIEW_VIDEO) ||
						 (GetItemSelected() == PLAYHEAD_TYPE_AMBIENT && ms_ComplexObjectClips[i].type == PLAYHEAD_TYPE_OVERVIEW_AMBIENT) ||
						 (GetItemSelected() == PLAYHEAD_TYPE_AUDIO && ms_ComplexObjectClips[i].type == PLAYHEAD_TYPE_OVERVIEW_AUDIO) ||
						 (GetItemSelected() == PLAYHEAD_TYPE_TEXT && ms_ComplexObjectClips[i].type == PLAYHEAD_TYPE_OVERVIEW_TEXT)))
					{
						u32 const c_playheadPositionMs = GetPlayheadPositionMs();
						s32 const c_projectIndexNearestPlayhead = GetProjectIndexNearestStartPos(GetItemSelected(), c_playheadPositionMs, -1);

						if (c_projectIndexNearestPlayhead != -1)
						{
							if ((u32)c_projectIndexNearestPlayhead == ms_ComplexObjectClips[i].uIndexToProject)
							{
								alpha = ALPHA_100_PERCENT;
							}
						}
					}

					ms_ComplexObjectClips[i].object.SetAlpha(alpha);
				}
			}
		}

		if (GetProject() && ms_ComplexObject[TIMELINE_OBJECT_OVERVIEW_CONTAINER].IsActive() && ms_ComplexObject[TIMELINE_OBJECT_OVERVIEW_SCROLLER].IsActive())
		{
			if (ms_ComplexObject[TIMELINE_OBJECT_TIMELINE_CONTAINER].IsActive())
			{
				float const c_positionInPixels = -ms_ComplexObject[TIMELINE_OBJECT_TIMELINE_CONTAINER].GetXPositionInPixels();
				float const c_widthInPixels = ms_ComplexObject[TIMELINE_OBJECT_OVERVIEW_SCROLLER].GetWidth();

				ms_ComplexObject[TIMELINE_OBJECT_OVERVIEW_SCROLLER].SetXPositionInPixels(c_positionInPixels);
				ms_ComplexObject[TIMELINE_OBJECT_OVERVIEW_SCROLLER_BOUNDARY_LEFT].SetXPositionInPixels(c_positionInPixels - ms_ComplexObject[TIMELINE_OBJECT_OVERVIEW_SCROLLER_BOUNDARY_LEFT].GetWidth());
				ms_ComplexObject[TIMELINE_OBJECT_OVERVIEW_SCROLLER_BOUNDARY_RIGHT].SetXPositionInPixels(c_positionInPixels + c_widthInPixels);
			}
		}
	}
}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorTimeline::GetTotalTimelineTimeMs
// PURPOSE:	returns the time in ms of the total space used up in the timeline
//			video project time + any text/audio that is over the end
/////////////////////////////////////////////////////////////////////////////////////////
u32 CVideoEditorTimeline::GetTotalTimelineTimeMs(const bool c_ignoreCap)
{
	u32 totalTimelineTimeMs = 0;

	if (GetProject())
	{
		totalTimelineTimeMs = (u32)GetProject()->GetTotalClipTimeMs();

		// check for any text over the length of the end of the project:
		for (u32 count = 0; count < (u32)GetProject()->GetOverlayTextCount(); count++)
		{
			u32 const c_startTimeAndDurationMs = GetProject()->GetOverlayText(count).getStartTimeMs() + GetProject()->GetOverlayText(count).getDurationMs();

			if (c_startTimeAndDurationMs > totalTimelineTimeMs)
			{
				totalTimelineTimeMs = c_startTimeAndDurationMs;  // it is higher so use it
			}
		}

		// check for any ambient tracks over the length of the end of the project:
		for (u32 count = 0; count < (u32)GetProject()->GetAmbientClipCount(); count++)
		{
			s32 const c_startTimeAndDurationMs = GetProject()->GetAmbientClip(count).getStartTimeWithOffsetMs() + GetProject()->GetAmbientClip(count).getTrimmedDurationMs();

			if (c_startTimeAndDurationMs > totalTimelineTimeMs)
			{
				totalTimelineTimeMs = c_startTimeAndDurationMs;  // it is higher so use it
			}
		}

		// check for any audio over the length of the end of the project:
		for (u32 count = 0; count < (u32)GetProject()->GetMusicClipCount(); count++)
		{
			s32 const c_startTimeAndDurationMs = GetProject()->GetMusicClip(count).getStartTimeWithOffsetMs() + GetProject()->GetMusicClip(count).getTrimmedDurationMs();

			if (c_startTimeAndDurationMs > totalTimelineTimeMs)
			{
				totalTimelineTimeMs = c_startTimeAndDurationMs;  // it is higher so use it
			}
		}

		// cap at the current picked up clip pos
		if (ms_maxPreviewAreaMs > 0 && IsPreviewingNonVideo())
		{
			if (ms_maxPreviewAreaMs > totalTimelineTimeMs)
			{
				totalTimelineTimeMs = ms_maxPreviewAreaMs;
			}
		
			if (!c_ignoreCap)
			{
				if (totalTimelineTimeMs >= GetProject()->GetTotalClipTimeMs())
				{
					totalTimelineTimeMs = (u32)GetProject()->GetTotalClipTimeMs();
				}
			}
		}
	}

	return totalTimelineTimeMs;
}



bool CVideoEditorTimeline::PlaceAmbientOntoTimeline(u32 const c_positionMs)
{
	s32 const c_offsetPosMs = CVideoEditorUi::GetSelectedAudio()->m_startOffsetMs;
	u32 const c_durationMs = CVideoEditorUi::GetSelectedAudio()->m_durationMs;

	CAmbientClipHandle newClip = GetProject()->AddAmbientClip(CVideoEditorUi::GetSelectedAudio()->m_soundHash, c_positionMs - c_offsetPosMs);
	newClip.setStartOffsetMs(c_offsetPosMs);
	newClip.setTrimmedDurationMs(c_durationMs);

	InsertAmbientClipIntoTimeline(newClip);

	HidePlayheadPreview();

	CVideoEditorUi::GetSelectedAudio()->m_soundHash = 0;

	SetPlayheadToStartOfNearestValidClip();

	return true;
}


bool CVideoEditorTimeline::PlaceAudioOntoTimeline(u32 const c_positionMs)
{
	s32 const c_offsetPosMs = CVideoEditorUi::GetSelectedAudio()->m_startOffsetMs;
	u32 const c_durationMs = CVideoEditorUi::GetSelectedAudio()->m_durationMs;

	CMusicClipHandle newMusicClip = GetProject()->AddMusicClip(CVideoEditorUi::GetSelectedAudio()->m_soundHash, c_positionMs - c_offsetPosMs);
	newMusicClip.setStartOffsetMs(c_offsetPosMs);
	newMusicClip.setTrimmedDurationMs(c_durationMs);

	InsertMusicClipIntoTimeline(newMusicClip);

	HidePlayheadPreview();

	CVideoEditorUi::GetSelectedAudio()->m_soundHash = 0;

	SetPlayheadToStartOfNearestValidClip();

	return true;
}



void CVideoEditorTimeline::DropPreviewClipAtPosition(u32 positionMs, bool const c_revertedClip)
{
	if (IsPreviewingVideo())
	{
		const bool c_isFirstClip = GetProject()->GetClipCount() == 0;

		// get the project index at the passed position
		const s32 c_projectIndex = c_isFirstClip ? 0 : GetProjectIndexNearestStartPos(PLAYHEAD_TYPE_VIDEO, positionMs, 0, -1, true);
		if (c_projectIndex != -1)
		{
			GetProject()->MoveStagingClipToProject(c_projectIndex);

			InsertVideoClipIntoTimeline(c_projectIndex);

			HidePlayheadPreview();

			RebuildAnchors();  // need to refresh the anchors

			SetPlayheadToStartOfNearestValidClip();

			// First clip gets automatically placed without user input, so don't trigger a sound for this
			if(!c_isFirstClip)
			{
				g_FrontendAudioEntity.PlaySound( "SELECT", "HUD_FRONTEND_DEFAULT_SOUNDSET" );
			}
		}
	}
	else if (IsPreviewingAmbientTrack())
	{
		if (c_revertedClip)
		{
			PlaceAmbientOntoTimeline(positionMs);
			g_FrontendAudioEntity.PlaySound( "SELECT", "HUD_FRONTEND_DEFAULT_SOUNDSET" );
		}
		else
		{
			if (positionMs <= GetProject()->GetTotalClipTimeMs())
			{
				if (CanPreviewClipBeDroppedAtPosition(positionMs) || CanPreviewClipBeDroppedAtPosition(positionMs+1))
				{
					positionMs = AdjustToNearestDroppablePosition(positionMs);

					PlaceAmbientOntoTimeline(positionMs);
					g_FrontendAudioEntity.PlaySound( "SELECT", "HUD_FRONTEND_DEFAULT_SOUNDSET" );
				}
				else
				{
					CVideoEditorMenu::SetUserConfirmationScreenWithAction("TRIGGER_OVERWRITE_CLIP", "VE_OVERWRITE_TL_AMB_CLIP_SURE");
				}
			}
			else
			{
				g_FrontendAudioEntity.PlaySound( "ERROR", "HUD_FRONTEND_DEFAULT_SOUNDSET" );
			}
		}
	}
	else if (IsPreviewingMusicTrack())
	{
		if (c_revertedClip)
		{
			PlaceAudioOntoTimeline(positionMs);
			g_FrontendAudioEntity.PlaySound( "SELECT", "HUD_FRONTEND_DEFAULT_SOUNDSET" );
		}
		else
		{
			if (positionMs <= GetProject()->GetTotalClipTimeMs())
			{
				if (CanPreviewClipBeDroppedAtPosition(positionMs) || CanPreviewClipBeDroppedAtPosition(positionMs+1))
				{
					positionMs = AdjustToNearestDroppablePosition(positionMs);

					PlaceAudioOntoTimeline(positionMs);
					g_FrontendAudioEntity.PlaySound( "SELECT", "HUD_FRONTEND_DEFAULT_SOUNDSET" );
				}
				else
				{
					CVideoEditorMenu::SetUserConfirmationScreenWithAction("TRIGGER_OVERWRITE_CLIP", "VE_OVERWRITE_TL_A_CLIP_SURE");
				}
			}
			else
			{
				g_FrontendAudioEntity.PlaySound( "ERROR", "HUD_FRONTEND_DEFAULT_SOUNDSET" );
			}
		}
	}
	else if (IsPreviewingText()) 
	{
		if (c_revertedClip)
		{
			PlaceEditedTextOntoTimeline(positionMs);
			g_FrontendAudioEntity.PlaySound( "SELECT", "HUD_FRONTEND_DEFAULT_SOUNDSET" );
		}
		else
		{
			if (positionMs <= GetProject()->GetTotalClipTimeMs())
			{
				if (CanPreviewClipBeDroppedAtPosition(positionMs) || CanPreviewClipBeDroppedAtPosition(positionMs+1))
				{
					positionMs = AdjustToNearestDroppablePosition(positionMs);
					PlaceEditedTextOntoTimeline(positionMs);
					g_FrontendAudioEntity.PlaySound( "SELECT", "HUD_FRONTEND_DEFAULT_SOUNDSET" );
				}
				else
				{
					CVideoEditorMenu::SetUserConfirmationScreenWithAction("TRIGGER_OVERWRITE_CLIP", "VE_OVERWRITE_TL_T_CLIP_SURE");
				}
			}
			else
			{
				g_FrontendAudioEntity.PlaySound( "ERROR", "HUD_FRONTEND_DEFAULT_SOUNDSET" );
			}
		}
	}

	if (!CVideoEditorMenu::IsUserConfirmationScreenActive())
	{
		// if item is dropped at the end or over of the timeline then, adjust the timeline back to the middle (fixes 2230256)
#define SPACE_TO_AUTOSHIFT_IN_MS (5000)  // if dropped 500ms or less away from the end of the timeline, readjust timeline so it appears in centre
		u32 const c_endOfVisibleProjectOnTimelineMs = (u32)GetProject()->GetTotalClipTimeMs();
		float const c_timelineWidth = ms_ComplexObject[TIMELINE_OBJECT_TIMELINE_BACKGROUND].GetWidth();
		if (positionMs >= c_endOfVisibleProjectOnTimelineMs - SPACE_TO_AUTOSHIFT_IN_MS)  // adjust the playhead marker to the last clip if we have moved
		{
			float const c_diffPixels = ((MsToPixels(positionMs - c_endOfVisibleProjectOnTimelineMs) - c_timelineWidth) * 0.5f);
			AdjustPlayhead(c_diffPixels, false);
			SetPlayheadToStartOfNearestValidClip();
		}
	}
}



void CVideoEditorTimeline::TriggerOverwriteClip()
{
	if (IsPreviewing())
	{
		const u32 c_positionMs = GetPlayheadPositionMs();

		u32 durationToCheckFor = 0;

		if (IsPreviewingAudioTrack())
		{
			durationToCheckFor += CVideoEditorUi::GetSelectedAudio()->m_durationMs;
		}

		if (IsPreviewingText())
		{
			durationToCheckFor += CTextTemplate::GetEditedText().m_durationMs;
		}

		s32 hoveringIndex;

		while ((hoveringIndex = GetProjectIndexBetweenTime(GetItemSelected(), c_positionMs, c_positionMs + durationToCheckFor)) != -1)
		{
			// delete the audio on the timeline at the current pos:
			if (IsPreviewingAmbientTrack())
			{
				DeleteAmbientClipFromTimeline(hoveringIndex);
				GetProject()->DeleteAmbientClip(hoveringIndex);
			}

			if (IsPreviewingMusicTrack())
			{
				DeleteMusicClipFromTimeline(hoveringIndex);
				GetProject()->DeleteMusicClip(hoveringIndex);
			}

			if (IsPreviewingText())
			{
				DeleteTextClipFromTimeline(hoveringIndex);
				GetProject()->DeleteOverlayText(hoveringIndex);
			}
		}

		if (IsPreviewingAmbientTrack())
		{
			PlaceAmbientOntoTimeline(c_positionMs);
		}

		if (IsPreviewingMusicTrack())
		{
			PlaceAudioOntoTimeline(c_positionMs);
		}

		if (IsPreviewingText())
		{
			PlaceEditedTextOntoTimeline(c_positionMs);
		}
	}
}


bool CVideoEditorTimeline::PlaceEditedTextOntoTimeline(u32 const c_positionMs)
{
	sEditedTextProperties &editedText = CTextTemplate::GetEditedText();

	if (GetProject())
	{
		CTextOverlayHandle newTextClip = GetProject()->AddOverlayText( c_positionMs, editedText.m_durationMs );
		newTextClip.SetLine( 0, editedText.m_text, editedText.m_position, Vector2(editedText.m_scale,editedText.m_scale), editedText.m_colorRGBA, editedText.m_style );
		CVideoEditorUi::ActivateStageClipTextOverlay(false);

		HidePlayheadPreview();

		InsertTextClipIntoTimeline(newTextClip);

		CTextTemplate::SetTextIndexEditing(-1);

		SetPlayheadToStartOfNearestValidClip();
						
		return true;
	}

	return false;
}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorTimeline::UpdateInput
// PURPOSE: calls the internal updateinput function and processes the sound effects
//			required
/////////////////////////////////////////////////////////////////////////////////////////
void CVideoEditorTimeline::UpdateInput()
{
	ms_wantScrubbingSoundThisFrame = false;

	static bool s_playingSound = false;
	static u32 s_playheadPositionLastFrame = 0;

	UpdateInputInternal();

	u32 const c_playheadPositionThisFrame = GetPlayheadPositionMs();

	if (ms_wantScrubbingSoundThisFrame && s_playheadPositionLastFrame != c_playheadPositionThisFrame)
	{
		if (!s_playingSound)
		{
			g_FrontendAudioEntity.PlaySoundReplayScrubbing();
			s_playingSound = true;
		}
	}
	else
	{
		if (s_playingSound)
		{
			g_FrontendAudioEntity.StopSoundReplayScrubbing();
			s_playingSound = false;
		}
	}

	s_playheadPositionLastFrame = c_playheadPositionThisFrame;
}



bool CVideoEditorTimeline::IsClipValid(ePLAYHEAD_TYPE const c_type, s32 const c_projIndex)
{
	if (c_type == PLAYHEAD_TYPE_VIDEO)
	{
		if (c_projIndex < GetProject()->GetClipCount())
		{
			return true;
		}
	}

	if (c_type == PLAYHEAD_TYPE_AMBIENT)
	{
		if (c_projIndex < GetProject()->GetAmbientClipCount())
		{
			return GetProject()->GetAmbientClip(c_projIndex).IsValid();
		}
	}

	if (c_type == PLAYHEAD_TYPE_AUDIO)
	{
		if (c_projIndex < GetProject()->GetMusicClipCount())
		{
			return GetProject()->GetMusicClip(c_projIndex).IsValid();
		}
	}

	if (c_type == PLAYHEAD_TYPE_TEXT)
	{
		if (c_projIndex < GetProject()->GetOverlayTextCount())
		{
			return (s32)GetProject()->GetOverlayText(c_projIndex).IsValid();
		}
	}

	return false;
}


s32 CVideoEditorTimeline::GetMaxClipCount(ePLAYHEAD_TYPE const c_type)
{
	if (GetProject())
	{
		return c_type == PLAYHEAD_TYPE_VIDEO ? GetProject()->GetClipCount() : c_type == PLAYHEAD_TYPE_AMBIENT ? GetProject()->GetAmbientClipCount() : c_type == PLAYHEAD_TYPE_AUDIO ? GetProject()->GetMusicClipCount() : GetProject()->GetOverlayTextCount();
	}

	return 0;
}



s32 CVideoEditorTimeline::GetStartTimeOfClipMs(ePLAYHEAD_TYPE const c_type, s32 const c_projIndex)
{
	if (c_type == PLAYHEAD_TYPE_VIDEO)
	{
		if (c_projIndex < GetProject()->GetClipCount())
		{
			return (s32)GetProject()->GetTrimmedTimeToClipMs(c_projIndex);
		}
	}

	if (c_type == PLAYHEAD_TYPE_AMBIENT)
	{
		if (c_projIndex < GetProject()->GetAmbientClipCount())
		{
			return (s32)GetProject()->GetAmbientClip(c_projIndex).getStartTimeWithOffsetMs();
		}
	}

	if (c_type == PLAYHEAD_TYPE_AUDIO)
	{
		if (c_projIndex < GetProject()->GetMusicClipCount())
		{
			return (s32)GetProject()->GetMusicClip(c_projIndex).getStartTimeWithOffsetMs();
		}
	}

	if (c_type == PLAYHEAD_TYPE_TEXT)
	{
		if (c_projIndex < GetProject()->GetOverlayTextCount())
		{
			return (s32)GetProject()->GetOverlayText(c_projIndex).getStartTimeMs();
		}
	}

	return -1;
}


s32 CVideoEditorTimeline::GetEndTimeOfClipMs(ePLAYHEAD_TYPE const c_type, s32 const c_projIndex)
{
	if (c_type == PLAYHEAD_TYPE_VIDEO)
	{
		if (c_projIndex < GetProject()->GetClipCount())
		{
			return (s32)(GetProject()->GetTrimmedTimeToClipMs(c_projIndex) + GetProject()->GetClipDurationTimeMs(c_projIndex));
		}
	}

	if (c_type == PLAYHEAD_TYPE_AMBIENT)
	{
		if (c_projIndex < GetProject()->GetAmbientClipCount())
		{
			return (s32)(GetProject()->GetAmbientClip(c_projIndex).getStartTimeWithOffsetMs() + GetProject()->GetAmbientClip(c_projIndex).getTrimmedDurationMs());
		}
	}

	if (c_type == PLAYHEAD_TYPE_AUDIO)
	{
		if (c_projIndex < GetProject()->GetMusicClipCount())
		{
			return (s32)(GetProject()->GetMusicClip(c_projIndex).getStartTimeWithOffsetMs() + GetProject()->GetMusicClip(c_projIndex).getTrimmedDurationMs());
		}
	}

	if (c_type == PLAYHEAD_TYPE_TEXT)
	{
		if (c_projIndex < GetProject()->GetOverlayTextCount())
		{
			return (s32)(GetProject()->GetOverlayText(c_projIndex).getStartTimeMs() + GetProject()->GetOverlayText(c_projIndex).getDurationMs());
		}
	}

	return -1;
}




u32 CVideoEditorTimeline::GetDurationOfClipMs(ePLAYHEAD_TYPE const c_type, s32 const c_projIndex)
{
	if (c_type == PLAYHEAD_TYPE_VIDEO)
	{
		if (c_projIndex < GetProject()->GetClipCount())
		{
			return (u32)GetProject()->GetClipDurationTimeMs(c_projIndex);
		}
	}

	if (c_type == PLAYHEAD_TYPE_AMBIENT)
	{
		if (c_projIndex < GetProject()->GetAmbientClipCount())
		{
			return GetProject()->GetAmbientClip(c_projIndex).getTrimmedDurationMs();
		}
	}

	if (c_type == PLAYHEAD_TYPE_AUDIO)
	{
		if (c_projIndex < GetProject()->GetMusicClipCount())
		{
			return GetProject()->GetMusicClip(c_projIndex).getTrimmedDurationMs();
		}
	}

	if (c_type == PLAYHEAD_TYPE_TEXT)
	{
		if (c_projIndex < GetProject()->GetOverlayTextCount())
		{
			return GetProject()->GetOverlayText(c_projIndex).getDurationMs();
		}
	}

	return 0;
}



s32 CVideoEditorTimeline::FindNearestAnchor(s32 const c_currentPosMs, s32 const c_dir)
{
	s32 newPositionMs = -1;

	s32 const c_nearestAnchorIndex = GetProject()->GetNearestAnchor( float(c_currentPosMs+c_dir), c_dir > 0 ? GetProject()->GetTotalClipTimeMs() : 0.0f);

	if (c_nearestAnchorIndex != -1)
	{
		newPositionMs = (s32)GetProject()->GetAnchorTimeMs(c_nearestAnchorIndex);
	}

	return newPositionMs;
}



s32 CVideoEditorTimeline::FindNearestSnapPoint(ePLAYHEAD_TYPE const c_type, s32 const c_currentPosMs, s32 const c_dir)
{
	if (c_type == PLAYHEAD_TYPE_ANCHOR)
	{
		return FindNearestAnchor(c_currentPosMs, c_dir);
	}

	s32 newPositionMs = c_currentPosMs;

	// get the start position of the nearest clip
	s32 const c_projectIndexAtPlayhead = GetProjectIndexAtTime(c_type, c_currentPosMs);
	s32 const c_projectIndexNextAlong = GetProjectIndexNearestStartPos(c_type, c_currentPosMs, c_dir, c_projectIndexAtPlayhead);

	bool const c_atOutsideClip = c_projectIndexAtPlayhead == -1;
	bool const c_atStartOfClip = c_projectIndexAtPlayhead != -1 && c_currentPosMs == GetStartTimeOfClipMs(c_type, c_projectIndexAtPlayhead);
	bool const c_atEndOfClip = c_projectIndexAtPlayhead != -1 && c_currentPosMs == GetEndTimeOfClipMs(c_type, c_projectIndexAtPlayhead);
	bool const c_atMiddleOfClip = c_projectIndexAtPlayhead != -1 && !c_atStartOfClip && !c_atEndOfClip;

	if (c_atOutsideClip)  // not on any clip
	{
		if (c_dir < 0)  // moving left
		{
			if (c_projectIndexNextAlong != -1)
			{
				newPositionMs = GetEndTimeOfClipMs(c_type, c_projectIndexNextAlong);  // go to end of next clip to left
			}
		}
		else
		{
			if (c_projectIndexNextAlong != -1)
			{
				newPositionMs = GetStartTimeOfClipMs(c_type, c_projectIndexNextAlong);  // go to start of next clip to right
			}
		}
	}

	if (c_atStartOfClip)
	{
		if (c_dir < 0)  // moving left
		{
			if (c_projectIndexNextAlong != -1)
			{
				newPositionMs = c_type == PLAYHEAD_TYPE_VIDEO ? GetStartTimeOfClipMs(c_type, c_projectIndexNextAlong) : GetEndTimeOfClipMs(c_type, c_projectIndexNextAlong);  // go to end of next clip to left
			}
		}
		else
		{
			if (c_projectIndexAtPlayhead != -1)
			{
				newPositionMs = GetEndTimeOfClipMs(c_type, c_projectIndexAtPlayhead);  // go to end of this clip
			}
		}
	}

	if (c_atEndOfClip)
	{
		if (c_dir < 0)  // moving left
		{
			if (c_projectIndexAtPlayhead != -1)
			{
				newPositionMs = GetStartTimeOfClipMs(c_type, c_projectIndexAtPlayhead);  // go to start of this clip
			}
		}
		else
		{
			if (c_projectIndexNextAlong != -1)
			{
				newPositionMs = GetStartTimeOfClipMs(c_type, c_projectIndexNextAlong);  // go to start of next clip to right
			}
		}
	}

	if (c_atMiddleOfClip)
	{
		if (c_dir < 0)  // moving left
		{
			if (c_projectIndexAtPlayhead != -1)
			{
				newPositionMs = GetStartTimeOfClipMs(c_type, c_projectIndexAtPlayhead);  // go to start of this clip
			}
		}
		else
		{
			if (c_projectIndexAtPlayhead != -1)
			{
				newPositionMs = GetEndTimeOfClipMs(c_type, c_projectIndexAtPlayhead);  // go to end of this clip
			}
		}
	}

	return newPositionMs;
}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorTimeline::AdjustToNearestSnapPoint
// PURPOSE: gets the position of the nearest item tos nap to based on current pos & dir
/////////////////////////////////////////////////////////////////////////////////////////
u32 CVideoEditorTimeline::AdjustToNearestSnapPoint(u32 const c_currentPosMs, s32 const c_dir, bool const c_precision)
{
	if (c_dir == 0)
	{
		return c_currentPosMs;  // not moving in any dir so just return the current pos
	}

	if (c_dir < 0 && c_currentPosMs == 1)  // if 0 is the only place to go, then lets go there!
	{
		return 0;
	}
	
	s32 const c_totalTimelineTimeMs = GetItemSelected() == PLAYHEAD_TYPE_VIDEO ? (s32)GetProject()->GetTotalClipTimeMs() : (s32)GetTotalTimelineTimeMs();

	if (c_dir > 0 && (s32)c_currentPosMs == c_totalTimelineTimeMs-1)  // if the max timeline pos is the only place to go, then lets go there!
	{
		return (u32)c_totalTimelineTimeMs;
	}

	s32 const c_firstClip = GetProjectIndexNearestStartPos(GetItemSelected(), 0);
	s32 const c_lastClip = GetProjectIndexNearestStartPos(GetItemSelected(), c_totalTimelineTimeMs);

	s32 const c_firstClipPosMs = c_firstClip != -1 ? GetStartTimeOfClipMs(GetItemSelected(), c_firstClip) : 0;
	s32 const c_lastClipPosMs = c_lastClip != -1 ? GetEndTimeOfClipMs(GetItemSelected(), c_lastClip) : c_totalTimelineTimeMs;

	s32 newPositionMs = c_dir < 0 ? c_firstClipPosMs : c_lastClipPosMs;

	s32 const c_nearestAnchor = FindNearestSnapPoint(PLAYHEAD_TYPE_ANCHOR, c_currentPosMs, c_dir);
	s32 const c_nearestVideo = FindNearestSnapPoint(PLAYHEAD_TYPE_VIDEO, c_currentPosMs, c_dir);
	s32 const c_nearestAmbient = FindNearestSnapPoint(PLAYHEAD_TYPE_AMBIENT, c_currentPosMs, c_dir);
	s32 const c_nearestAudio = FindNearestSnapPoint(PLAYHEAD_TYPE_AUDIO, c_currentPosMs, c_dir);
	s32 const c_nearestText = FindNearestSnapPoint(PLAYHEAD_TYPE_TEXT, c_currentPosMs, c_dir);

	if (c_precision)
	{
		s32 const c_diffAnchor = c_nearestAnchor != -1 ? (c_dir < 0 ? c_currentPosMs - c_nearestAnchor : c_nearestAnchor - c_currentPosMs) : 0;
		s32 const c_diffVideo = c_nearestVideo != -1 ? (c_dir < 0 ? c_currentPosMs - c_nearestVideo : c_nearestVideo - c_currentPosMs) : 0;
		s32 const c_diffAmbient = c_nearestAmbient != -1 ? (c_dir < 0 ? c_currentPosMs -  c_nearestAmbient : c_nearestAmbient - c_currentPosMs) : 0;
		s32 const c_diffAudio = c_nearestAudio != -1 ? (c_dir < 0 ? c_currentPosMs -  c_nearestAudio : c_nearestAudio - c_currentPosMs) : 0;
		s32 const c_diffText = c_nearestText != -1 ? (c_dir < 0 ? c_currentPosMs - c_nearestText : c_nearestText - c_currentPosMs) : 0;

		s32 nearest = c_diffAnchor;
		newPositionMs = c_nearestAnchor;

		if (c_diffVideo > 0 && (c_diffVideo < nearest || nearest == 0))
		{
			nearest = c_diffVideo;
			newPositionMs = c_nearestVideo;
		}

		if (c_diffAmbient > 0 && (c_diffAmbient < nearest || nearest == 0))
		{
			nearest = c_diffAmbient;
			newPositionMs = c_nearestAmbient;
		}

		if (c_diffAudio > 0 && (c_diffAudio < nearest || nearest == 0))
		{
			nearest = c_diffAudio;
			newPositionMs = c_nearestAudio;
		}

		if (c_diffText > 0 && (c_diffText < nearest || nearest == 0))
		{
			nearest = c_diffText;
			newPositionMs = c_nearestText;
		}
	}
	else
	{
		newPositionMs = GetItemSelected() == PLAYHEAD_TYPE_VIDEO ? c_nearestVideo : GetItemSelected() == PLAYHEAD_TYPE_AMBIENT ? c_nearestAmbient : GetItemSelected() == PLAYHEAD_TYPE_AUDIO ? c_nearestAudio : c_nearestText;

		if (newPositionMs == (s32)c_currentPosMs && !(c_firstClipPosMs == newPositionMs || c_lastClipPosMs == newPositionMs))
		{
			newPositionMs += c_dir;
		}
	}

	return newPositionMs;
}



bool CVideoEditorTimeline::TrimObject(ePLAYHEAD_TYPE c_type, u32 const c_projectIndex, s32 const c_trimDir, bool const c_trimmingStart, bool const c_snap, s32 const c_directMovePosMs)
{
#define TRIM_INCREMENT_MS		(10)

	if (!GetProject())
	{
		return false;
	}

	if (!IsTimelineItemNonVideo(c_type))
	{
		return false;
	}

	s32 const c_trimMultiplier = c_trimDir == 0 ? 0 : c_directMovePosMs == -1 ? c_trimDir > 0 ? 1 : -1 : c_trimDir;
	bool c_isValid = IsClipValid(c_type, c_projectIndex);

	if (c_isValid)
	{
		bool playErrorSound = false;

		s32 const c_totalUsableProjectSpaceMs = (s32)GetProject()->GetTotalClipTimeMs();
		u32 const c_soundHash = GetProject()->GetMusicClip(c_projectIndex).getClipSoundHash();

		s32 minDurationMs = c_type == PLAYHEAD_TYPE_AMBIENT ? MIN_AMBIENT_TRACK_DURATION_MS : c_type == PLAYHEAD_TYPE_AUDIO ? (audRadioTrack::IsScoreTrack(c_soundHash) || audRadioTrack::IsCommercial(c_soundHash) ? MIN_SCORE_TRACK_DURATION_MS : MIN_RADIO_TRACK_DURATION_MS) : MIN_TEXT_DURATION_MS;
		s32 const c_incrementMs = TRIM_INCREMENT_MS * rage::Max(ms_trimmingClipIncrementMultiplier, 1);

		s32 const c_currentOutsideTimeMs = c_type == PLAYHEAD_TYPE_AMBIENT ? GetProject()->GetAmbientClip(c_projectIndex).getStartTimeMs() : c_type == PLAYHEAD_TYPE_AUDIO ? GetProject()->GetMusicClip(c_projectIndex).getStartTimeMs() : 0;
		s32 const c_currentStartOffsetTimeMs = c_type == PLAYHEAD_TYPE_AMBIENT ? GetProject()->GetAmbientClip(c_projectIndex).getStartOffsetMs() : c_type == PLAYHEAD_TYPE_AUDIO ? GetProject()->GetMusicClip(c_projectIndex).getStartOffsetMs() : GetProject()->GetOverlayText(c_projectIndex).getStartTimeMs();
		s32 const c_currentDisplayedTimelinePosMs = c_currentOutsideTimeMs + c_currentStartOffsetTimeMs;
		s32 const c_currentOffsetDuration = GetDurationOfClipMs(c_type, c_projectIndex);

		s32 minStartTimeMs = c_currentOutsideTimeMs > 0 ? c_currentOutsideTimeMs : 0;
		s32 maxDurationMs = c_type == PLAYHEAD_TYPE_AMBIENT ? GetProject()->GetAmbientClip(c_projectIndex).getTrackTotalDurationMs()-c_currentStartOffsetTimeMs : c_type == PLAYHEAD_TYPE_AUDIO ? GetProject()->GetMusicClip(c_projectIndex).getTrackTotalDurationMs()-c_currentStartOffsetTimeMs : (s32)GetProject()->GetTotalClipTimeMs();

		// dont allow any increase in duration if over end of usable timeline area
		if (!c_trimmingStart && c_trimDir > 0 && c_currentOffsetDuration + c_currentDisplayedTimelinePosMs > c_totalUsableProjectSpaceMs)
		{
			maxDurationMs = c_currentOffsetDuration;
		}

		// ensure that the max duration we can use is the start of the next text clip along
		// get the start position of the nearest clip
		s32 const c_projectIndexToLeft = CVideoEditorTimeline::GetProjectIndexNearestStartPos(c_type, c_currentDisplayedTimelinePosMs, -1, c_projectIndex);  // nearest proj index
		s32 const c_projectIndexToRight = CVideoEditorTimeline::GetProjectIndexNearestStartPos(c_type, c_currentDisplayedTimelinePosMs, 1, c_projectIndex);  // nearest proj index

		if (c_projectIndexToLeft != -1)
		{
			if (c_currentDisplayedTimelinePosMs > 0)
			{
				s32 endTimeOfClipToLeft = minStartTimeMs;
				if (c_type == PLAYHEAD_TYPE_AMBIENT)
				{
					endTimeOfClipToLeft = (s32)(GetProject()->GetAmbientClip(c_projectIndexToLeft).getStartTimeWithOffsetMs()+GetProject()->GetAmbientClip(c_projectIndexToLeft).getTrimmedDurationMs());
				}
				else if (c_type == PLAYHEAD_TYPE_AUDIO)
				{
					endTimeOfClipToLeft = (s32)(GetProject()->GetMusicClip(c_projectIndexToLeft).getStartTimeWithOffsetMs()+GetProject()->GetMusicClip(c_projectIndexToLeft).getTrimmedDurationMs());
				}
				else
				{
					endTimeOfClipToLeft = (s32)(GetProject()->GetOverlayText(c_projectIndexToLeft).getStartTimeMs()+GetProject()->GetOverlayText(c_projectIndexToLeft).getDurationMs());
				}

				if (endTimeOfClipToLeft > minStartTimeMs)  // if the minimum time already isnt the end of the other clip, then set the minimum to be the end of the clip + 1ms
				{
					minStartTimeMs = endTimeOfClipToLeft + (c_snap ? 0 : 1);
				}
			}
		}

		if (c_projectIndexToRight != -1)
		{
			s32 startTimeOfClipToRight = 0;

			if (c_type == PLAYHEAD_TYPE_AMBIENT)
			{
				startTimeOfClipToRight = (s32)GetProject()->GetAmbientClip(c_projectIndexToRight).getStartTimeWithOffsetMs();
			}
			else if (c_type == PLAYHEAD_TYPE_AUDIO)
			{
				startTimeOfClipToRight = (s32)GetProject()->GetMusicClip(c_projectIndexToRight).getStartTimeWithOffsetMs();
			}
			else
			{
				startTimeOfClipToRight = (s32)GetProject()->GetOverlayText(c_projectIndexToRight).getStartTimeMs();
			}

			s32 newMaxDurationMs = startTimeOfClipToRight - c_currentDisplayedTimelinePosMs;

			if (newMaxDurationMs < maxDurationMs)
			{
				maxDurationMs = newMaxDurationMs - (c_snap ? 0 : 1);
			}
		}

		if (maxDurationMs < minDurationMs)   // max time always will need to be atleast the minimum size for legals
		{
			maxDurationMs = minDurationMs;
		}

		s32 newAdjustedStartTimeMs = c_currentStartOffsetTimeMs;
		s32 newDuration = c_currentOffsetDuration;

		s32 delta = 0;

		if (c_directMovePosMs != -1)
		{
			if (c_trimmingStart)
			{
				delta = c_trimMultiplier > 0 ? c_directMovePosMs - c_currentDisplayedTimelinePosMs : c_currentDisplayedTimelinePosMs - c_directMovePosMs;
			}
			else
			{
				delta = c_trimMultiplier > 0 ? c_directMovePosMs - (c_currentDisplayedTimelinePosMs+c_currentOffsetDuration) : (c_currentDisplayedTimelinePosMs+c_currentOffsetDuration) - c_directMovePosMs;
			}
		}
		else if (c_snap)
		{
			s32 const c_currentPos = c_trimmingStart ? c_currentDisplayedTimelinePosMs : c_currentDisplayedTimelinePosMs + c_currentOffsetDuration;
			s32 const c_newPositionAfterSnapMs = AdjustToNearestSnapPoint(c_currentPos, c_trimMultiplier, true);

			if (c_newPositionAfterSnapMs != -1)
			{
				delta = c_trimMultiplier > 0 ? c_newPositionAfterSnapMs - c_currentPos : c_currentPos - c_newPositionAfterSnapMs;
			}
		}
		else
		{
			delta = c_incrementMs;
		}

		if (c_trimmingStart)
		{
			bool counterDuration = true;
			newAdjustedStartTimeMs += (c_trimMultiplier * delta);

			if (newAdjustedStartTimeMs < 0)  // cap at any negative value
			{
				const s32 c_outsideBoundsDelta = c_trimMultiplier * newAdjustedStartTimeMs;
				delta -= c_outsideBoundsDelta;

				newAdjustedStartTimeMs = 0;
				playErrorSound = true;
			}

			if (c_currentOutsideTimeMs+newAdjustedStartTimeMs < minStartTimeMs)  // limit to the minimum start time we may have, taking into account the outside time
			{
				newAdjustedStartTimeMs = minStartTimeMs-c_currentOutsideTimeMs;

				if (newAdjustedStartTimeMs != c_currentStartOffsetTimeMs)  // only adjust duration if we have readjusted start time
				{
					const s32 c_outsideBoundsDelta = c_currentStartOffsetTimeMs - newAdjustedStartTimeMs;//minStartTimeMs-(c_currentOutsideTimeMs+newAdjustedStartTimeMs);
					newDuration += c_outsideBoundsDelta;
				}

				counterDuration = false;
				playErrorSound = true;
			}

			if (counterDuration)
			{
				newDuration += (-c_trimMultiplier * delta);  // if trimming the start, need to apply the delta to the duration so it remains the same

				if (c_trimMultiplier * delta < 0)
				{
					maxDurationMs -= (c_trimMultiplier * delta);
				}
			}

			if (newDuration < minDurationMs)  // minimum duration allowed
			{
				const s32 c_outsideBoundsDelta = minDurationMs - newDuration;
				newAdjustedStartTimeMs -= c_outsideBoundsDelta;
				newDuration += c_outsideBoundsDelta;
				playErrorSound = true;
			}

			if (newDuration > maxDurationMs)  // maximum duration allowed
			{
				const s32 c_outsideBoundsDelta = newDuration - maxDurationMs;
				newAdjustedStartTimeMs -= c_outsideBoundsDelta;
				newDuration -= c_outsideBoundsDelta;
				playErrorSound = true;
			}

			if (newAdjustedStartTimeMs > c_currentStartOffsetTimeMs + c_currentOffsetDuration)  // start time is over old starttime+duration
			{
				newAdjustedStartTimeMs = c_currentStartOffsetTimeMs;
				newDuration = c_currentOffsetDuration;
				playErrorSound = true;
			}

			if (newAdjustedStartTimeMs > c_totalUsableProjectSpaceMs - c_currentOutsideTimeMs)  // cannot trim it out of the project space
			{
				const s32 c_outsideBoundsDelta = newAdjustedStartTimeMs - (c_totalUsableProjectSpaceMs - c_currentOutsideTimeMs);
				newAdjustedStartTimeMs -= c_outsideBoundsDelta;
				newDuration += c_outsideBoundsDelta;
				playErrorSound = true;
			}
		}
		else
		{
			newDuration += (c_trimMultiplier * delta);

			if (newDuration < minDurationMs)  // minimum duration allowed
			{
				newDuration = minDurationMs;
				playErrorSound = true;
			}

			if ( newDuration > maxDurationMs)  // minimum duration allowed
			{
				newDuration = maxDurationMs;
				playErrorSound = true;
			}

			// cap so it never goes over the end of usable project space
			if (newDuration > c_totalUsableProjectSpaceMs - c_currentDisplayedTimelinePosMs)
			{
				newDuration = c_totalUsableProjectSpaceMs - c_currentDisplayedTimelinePosMs;
			}
		}

		// final check after all precise adjustments that we never go outside the min and max durations
		if (c_currentOutsideTimeMs+newAdjustedStartTimeMs < minStartTimeMs || newDuration > maxDurationMs || newDuration < minDurationMs)
		{
			newAdjustedStartTimeMs = c_currentStartOffsetTimeMs;
			newDuration = c_currentOffsetDuration;
			playErrorSound = true;
		}

		// need these functions to set an offset and also a duration
		if (c_type == PLAYHEAD_TYPE_AMBIENT)
		{
			u32 const c_validStartOffetMs = GetProject()->GetAmbientClip(c_projectIndex).getStartOffsetMs();
			u32 const c_validTrimmedDurationMs = GetProject()->GetAmbientClip(c_projectIndex).getTrimmedDurationMs();

			GetProject()->GetAmbientClip(c_projectIndex).setStartOffsetMs(newAdjustedStartTimeMs);
			GetProject()->GetAmbientClip(c_projectIndex).setTrimmedDurationMs(newDuration);

			if (!GetProject()->GetAmbientClip(c_projectIndex).VerifyTimingValues())  // if we sent through invalid values, revert them back
			{
				GetProject()->GetAmbientClip(c_projectIndex).setStartOffsetMs(c_validStartOffetMs);
				GetProject()->GetAmbientClip(c_projectIndex).setTrimmedDurationMs(c_validTrimmedDurationMs);
			}
		}
		else if (c_type == PLAYHEAD_TYPE_AUDIO)
		{
			u32 const c_validStartOffetMs = GetProject()->GetMusicClip(c_projectIndex).getStartOffsetMs();
			u32 const c_validTrimmedDurationMs = GetProject()->GetMusicClip(c_projectIndex).getTrimmedDurationMs();

			GetProject()->GetMusicClip(c_projectIndex).setStartOffsetMs(newAdjustedStartTimeMs);
			GetProject()->GetMusicClip(c_projectIndex).setTrimmedDurationMs(newDuration);

			if (!GetProject()->GetMusicClip(c_projectIndex).VerifyTimingValues())  // if we sent through invalid values, revert them back
			{
				GetProject()->GetMusicClip(c_projectIndex).setStartOffsetMs(c_validStartOffetMs);
				GetProject()->GetMusicClip(c_projectIndex).setTrimmedDurationMs(c_validTrimmedDurationMs);
			}
		}
		else
		{
			GetProject()->GetOverlayText(c_projectIndex).setStartTimeMs(newAdjustedStartTimeMs);  // just apply the offset
			GetProject()->GetOverlayText(c_projectIndex).setDurationMs(newDuration);
		}

		s32 const c_timelineIndex = GetTimelineIndexFromProjectIndex(c_type, c_projectIndex);
		s32 const c_timelineOverviewIndex = GetTimelineIndexFromProjectIndex(c_type == PLAYHEAD_TYPE_AMBIENT ? PLAYHEAD_TYPE_OVERVIEW_AMBIENT : c_type == PLAYHEAD_TYPE_AUDIO ? PLAYHEAD_TYPE_OVERVIEW_AUDIO : PLAYHEAD_TYPE_OVERVIEW_TEXT, c_projectIndex);

		if (c_timelineIndex != -1)
		{
			sTimelineItem *pTimelineClipObject = &ms_ComplexObjectClips[c_timelineIndex];
			sTimelineItem *pTimelineOverviewBarObject = &ms_ComplexObjectClips[c_timelineOverviewIndex];

			if (pTimelineClipObject && pTimelineOverviewBarObject)
			{
				if (c_type == PLAYHEAD_TYPE_AUDIO)
				{
					UpdateAdjustableAudioProperties(pTimelineClipObject, pTimelineOverviewBarObject, GetProject()->GetMusicClip(c_projectIndex));
				}
				else if (c_type == PLAYHEAD_TYPE_AMBIENT)
				{
					UpdateAdjustableAmbientProperties(pTimelineClipObject, pTimelineOverviewBarObject, GetProject()->GetAmbientClip(c_projectIndex));
				}
				else
				{
					UpdateAdjustableTextProperties(pTimelineClipObject, pTimelineOverviewBarObject, GetProject()->GetOverlayText(c_projectIndex));
				}

				static bool s_canPlaySound = true;
				bool playedSoundThisFrame = false;
				s32 const c_newPosition = c_trimmingStart ? c_currentOutsideTimeMs + newAdjustedStartTimeMs : c_currentOutsideTimeMs + newAdjustedStartTimeMs + newDuration;

				if (playErrorSound)
				{
					if (s_canPlaySound)
					{
						g_FrontendAudioEntity.PlaySound("ERROR","HUD_FRONTEND_DEFAULT_SOUNDSET");
						s_canPlaySound = false;
						playedSoundThisFrame = true;
					}
				}

				if (c_newPosition >= 0)
				{
					if (GetPlayheadPositionMs() != (u32)c_newPosition)
					{
						RepositionPlayheadIfNeeded(false);

						SetPlayheadPositionMs(c_newPosition > 0 ? c_newPosition : 0, false);
						ms_trimmingClipOriginalPositionMs = -1;

						if (!playErrorSound && !playedSoundThisFrame)
						{
							s_canPlaySound = true;
						}

						CVideoEditorUi::SetProjectEdited(true);  // flag as edited

						UpdateOverviewBar();
					}
				}

				return true;
			}
		}
	}

	return false;
}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorTimeline::GetMouseMovementDirInTimelineArea
// PURPOSE: returns what size of the timeline the mouse is on for direction
/////////////////////////////////////////////////////////////////////////////////////////
s32 CVideoEditorTimeline::GetMouseMovementDirInTimelineArea()
{
#if RSG_PC
	float const c_currentContainerPos = ms_ComplexObject[TIMELINE_OBJECT_TIMELINE_CONTAINER].GetXPositionInPixels();
	u32 const c_lastClipEndPosFromTimelinePanelMs = GetTotalTimelineTimeMs();
	float const c_lastClipEndPosFromTimelinePanel = MsToPixels(c_lastClipEndPosFromTimelinePanelMs) - (ms_ComplexObject[TIMELINE_OBJECT_TIMELINE_BACKGROUND].GetWidth());

	float const c_mousePosX = (float)ioMouse::GetX();
		
	float mousePosXScaleform = c_mousePosX;
	float mousePosYScaleform = 0.0f;

	CScaleformMgr::TranslateWindowCoordinateToScaleformMovieCoordinate( 
		c_mousePosX, mousePosYScaleform, CVideoEditorUi::GetMovieId(), mousePosXScaleform, mousePosYScaleform );

	float const c_leftTimelineArea = (TIMELINE_POSITION_X * ACTIONSCRIPT_STAGE_SIZE_X) + ms_ComplexObject[TIMELINE_OBJECT_TIMELINE_BACKGROUND].GetXPositionInPixels();
	float const c_rightTimelineArea = c_leftTimelineArea + ms_ComplexObject[TIMELINE_OBJECT_TIMELINE_BACKGROUND].GetWidth();


	if (-c_currentContainerPos < c_lastClipEndPosFromTimelinePanel && mousePosXScaleform > c_rightTimelineArea-AMOUNT_OF_MOUSE_MOVEMENT_BUFFER_AT_SIDES)
	{
		return PixelsToMs(mousePosXScaleform-(c_rightTimelineArea-AMOUNT_OF_MOUSE_MOVEMENT_BUFFER_AT_SIDES));
	}

	if (c_currentContainerPos < 0.0f && mousePosXScaleform < c_leftTimelineArea+AMOUNT_OF_MOUSE_MOVEMENT_BUFFER_AT_SIDES)
	{
		return -PixelsToMs((c_leftTimelineArea+AMOUNT_OF_MOUSE_MOVEMENT_BUFFER_AT_SIDES)-mousePosXScaleform);
	}
#endif  // RSG_PC

	return 0;
}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorTimeline::IsMouseWithinTimelineArea
// PURPOSE: whether the mouse is in the timeline area on screen
/////////////////////////////////////////////////////////////////////////////////////////
bool CVideoEditorTimeline::IsMouseWithinTimelineArea()
{
#if RSG_PC
	float const c_mousePosX = (float)ioMouse::GetX();
	float const c_mousePosY = (float)ioMouse::GetY();

	float mousePosXScaleform = c_mousePosX;
	float mousePosYScaleform = c_mousePosY;

	CScaleformMgr::TranslateWindowCoordinateToScaleformMovieCoordinate( 
		c_mousePosX, c_mousePosY, CVideoEditorUi::GetMovieId(), mousePosXScaleform, mousePosYScaleform );

	float const c_leftTimelineArea = (TIMELINE_POSITION_X * ACTIONSCRIPT_STAGE_SIZE_X) + ms_ComplexObject[TIMELINE_OBJECT_TIMELINE_BACKGROUND].GetXPositionInPixels();
	float const c_rightTimelineArea = c_leftTimelineArea + ms_ComplexObject[TIMELINE_OBJECT_TIMELINE_BACKGROUND].GetWidth();
	float const c_topTimelineArea = (TIMELINE_POSITION_Y * ACTIONSCRIPT_STAGE_SIZE_Y) + ms_ComplexObject[TIMELINE_OBJECT_TIMELINE_CONTAINER].GetYPositionInPixels();
	float const c_bottomTimelineArea = c_topTimelineArea + ms_ComplexObject[TIMELINE_OBJECT_TIMELINE_CONTAINER].GetHeight();

	if (mousePosXScaleform >= c_leftTimelineArea && mousePosXScaleform <= c_rightTimelineArea && mousePosYScaleform >= c_topTimelineArea && mousePosYScaleform <= c_bottomTimelineArea)
	{
		return true;
	}
#endif  // RSG_PC

	return false;
}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorTimeline::IsMouseWithinOverviewArea
// PURPOSE: whether the mouse is in the overview area on screen
/////////////////////////////////////////////////////////////////////////////////////////
bool CVideoEditorTimeline::IsMouseWithinOverviewArea()
{
#if RSG_PC
	float const c_mousePosX = (float)ioMouse::GetX();
	float const c_mousePosY = (float)ioMouse::GetY();

	float mousePosXScaleform = c_mousePosX;
	float mousePosYScaleform = c_mousePosY;

	CScaleformMgr::TranslateWindowCoordinateToScaleformMovieCoordinate( 
		c_mousePosX, c_mousePosY, CVideoEditorUi::GetMovieId(), mousePosXScaleform, mousePosYScaleform );

	float const c_leftOverviewArea = (TIMELINE_POSITION_X * ACTIONSCRIPT_STAGE_SIZE_X) + ms_ComplexObject[TIMELINE_OBJECT_TIMELINE_BACKGROUND].GetXPositionInPixels();
	float const c_rightOverviewArea = c_leftOverviewArea + ms_ComplexObject[TIMELINE_OBJECT_TIMELINE_BACKGROUND].GetWidth();
	float const c_topOverviewArea = (TIMELINE_POSITION_Y * ACTIONSCRIPT_STAGE_SIZE_Y) + ms_ComplexObject[TIMELINE_OBJECT_TIMELINE_CONTAINER].GetYPositionInPixels() + ms_ComplexObject[TIMELINE_OBJECT_OVERVIEW_BACKGROUND].GetYPositionInPixels();
	float const c_bottomOverviewArea = c_topOverviewArea + ms_ComplexObject[TIMELINE_OBJECT_OVERVIEW_BACKGROUND].GetHeight();
	float const c_additionalMouseBoundary = 4.0f;  // little bit extra to allow for accidental movement outside when dragging

	if (mousePosXScaleform >= c_leftOverviewArea && mousePosXScaleform <= c_rightOverviewArea && mousePosYScaleform >= c_topOverviewArea - c_additionalMouseBoundary && mousePosYScaleform <= c_bottomOverviewArea + c_additionalMouseBoundary)
	{
		return true;
	}
#endif  // RSG_PC

	return false;
}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorTimeline::IsMouseWithinScrollerArea
// PURPOSE: whether the mouse is in the scroller area on screen
/////////////////////////////////////////////////////////////////////////////////////////
bool CVideoEditorTimeline::IsMouseWithinScrollerArea()
{
#if RSG_PC
	float const c_mousePosX = (float)ioMouse::GetX();
	float const c_mousePosY = (float)ioMouse::GetY();

	float mousePosXScaleform = 0.0f;
	float mousePosYScaleform = 0.0f;

	CScaleformMgr::TranslateWindowCoordinateToScaleformMovieCoordinate( c_mousePosX, c_mousePosY, CVideoEditorUi::GetMovieId(), mousePosXScaleform, mousePosYScaleform );

	float const c_leftScroller = (TIMELINE_POSITION_X * ACTIONSCRIPT_STAGE_SIZE_X) + ms_ComplexObject[TIMELINE_OBJECT_OVERVIEW_SCROLLER_BOUNDARY_LEFT].GetXPositionInPixels() * ms_ComplexObject[TIMELINE_OBJECT_OVERVIEW_CONTAINER].GetScale().x / 100.0f;
	float const c_rightScroller = (TIMELINE_POSITION_X * ACTIONSCRIPT_STAGE_SIZE_X) + ms_ComplexObject[TIMELINE_OBJECT_OVERVIEW_SCROLLER_BOUNDARY_RIGHT].GetXPositionInPixels() * ms_ComplexObject[TIMELINE_OBJECT_OVERVIEW_CONTAINER].GetScale().x / 100.0f;
	float const c_topScroller = (TIMELINE_POSITION_Y * ACTIONSCRIPT_STAGE_SIZE_Y) + ms_ComplexObject[TIMELINE_OBJECT_TIMELINE_CONTAINER].GetYPositionInPixels() + ms_ComplexObject[TIMELINE_OBJECT_OVERVIEW_BACKGROUND].GetYPositionInPixels();
	float const c_bottomScroller = (c_topScroller) + ms_ComplexObject[TIMELINE_OBJECT_OVERVIEW_SCROLLER].GetHeight();

	if ( mousePosXScaleform >= c_leftScroller &&
		 mousePosXScaleform <= c_rightScroller &&
		 mousePosYScaleform >= c_topScroller &&
		 mousePosYScaleform <= c_bottomScroller )
	{
		return true;
	}
#endif  // RSG_PC

	return false;
}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorTimeline::UpdateInputInternal
// PURPOSE: checks for input on the timeline
/////////////////////////////////////////////////////////////////////////////////////////
void CVideoEditorTimeline::UpdateInputInternal()
{
	if (!GetProject())
	{
		return;
	}

	if (ms_previewingAudioTrack != -1)  // only RT works when we are previewing Audio Track
	{
		if (CControlMgr::GetMainFrontendControl().GetReplayPreviewAudio().IsReleased() ||
			CControlMgr::GetMainFrontendControl().GetReplayToggleTimeline().IsReleased() ||
			CPauseMenu::CheckInput(FRONTEND_INPUT_BACK) ||
			CMousePointer::IsMouseBack() ||
			CControlMgr::GetMainFrontendControl().GetFrontendLeftRight().GetNorm() != 0.0f)
		{
			g_RadioAudioEntity.StopReplayTrackPreview();
			ms_previewingAudioTrack = -1;

			SetPlayheadPositionMs(ms_uProjectPositionToRevertMs, true);

			CVideoEditorInstructionalButtons::Refresh();
		}

		return;
	}

#if RSG_PC
	bool const c_mousePointerChanged = UpdateMouseInput();

	if (IsMouseWithinScrollerArea() && !ms_heldDownAndScrubbingTimeline && !ms_heldDownAndScrubbingOverviewBar && !IsPreviewing())
	{
		CMousePointer::SetMouseCursorStyle(MOUSE_CURSOR_STYLE_HAND_OPEN);
	}
	else if (!c_mousePointerChanged)
	{
		CMousePointer::SetMouseCursorStyle(MOUSE_CURSOR_STYLE_ARROW);
	}
#endif // #if RSG_PC

	bool const c_wasKbmLastFrame =  CVideoEditorInstructionalButtons::LatestInstructionsAreKeyboardMouse();

	if (c_wasKbmLastFrame && CControlMgr::GetMainFrontendControl().GetReplayTimelineSave().IsReleased())
	{
		CVideoEditorUi::TriggerSave(false);
	}

	if (CPauseMenu::CheckInput(FRONTEND_INPUT_ACCEPT) ||
		( c_wasKbmLastFrame && CControlMgr::GetMainFrontendControl().GetReplayCtrl().IsDown() && CControlMgr::GetMainFrontendControl().GetReplayTimelinePlaceClip().IsReleased() ) )
	{
		if (IsPreviewing())
		{
			u32 const c_playheadPos = GetPlayheadPositionMs();
			DropPreviewClipAtPosition(c_playheadPos, false);
			g_FrontendAudioEntity.PlaySound( "SELECT", "HUD_FRONTEND_DEFAULT_SOUNDSET" );

			return;
		}
	}

	if( ( c_wasKbmLastFrame && CControlMgr::GetMainFrontendControl().GetReplayCtrl().IsDown() && CControlMgr::GetMainFrontendControl().GetReplayTimelinePickupClip().IsReleased() ) ||
		( !c_wasKbmLastFrame && CPauseMenu::CheckInput(FRONTEND_INPUT_X ) ) )
	{
		if (!IsPreviewing() && !ms_trimmingClip)
		{
			ms_iProjectPositionToRevertOnUpMs = ms_iProjectPositionToRevertOnDownMs = -1;  // we moved so reset

			u32 const c_playheadPosMs = GetPlayheadPositionMs();

			if ( GetProject()->GetClipCount() > 1 || GetItemSelected() != PLAYHEAD_TYPE_VIDEO)
			{
				s32 const c_projectIndex = GetProjectIndexAtTime(GetItemSelected(), c_playheadPosMs);

				if (c_projectIndex != -1)
				{
					ms_bHaveProjectToRevert = true;
					ms_uProjectPositionToRevertMs = GetStartTimeOfClipMs(GetItemSelected(), c_projectIndex);

					CVideoEditorUi::PickupSelectedClip((u32)c_projectIndex, true);
				}
			}

			CVideoEditorInstructionalButtons::Refresh();

			return;
		}
	}

	if (IsPreviewing())
	{
		if (CPauseMenu::CheckInput(FRONTEND_INPUT_BACK) || CMousePointer::IsMouseBack())
		{
			g_FrontendAudioEntity.PlaySound( "BACK", "HUD_FRONTEND_DEFAULT_SOUNDSET" );

			ms_iProjectPositionToRevertOnUpMs = ms_iProjectPositionToRevertOnDownMs = -1;  // we moved so reset

			if (ms_bHaveProjectToRevert)
			{
				DropPreviewClipAtPosition(ms_uProjectPositionToRevertMs, true);
			}
			else
			{
				GetProject()->InvalidateStagingClip();

				SetPlayheadToStartOfNearestValidClip();
			}

			HidePlayheadPreview();

			RefreshStagingArea();

			return;
		}
	}
	else if (!ms_trimmingClip)
	{
		if (CControlMgr::GetMainFrontendControl().GetReplayToggleTimeline().IsReleased() ||
			CPauseMenu::CheckInput(FRONTEND_INPUT_BACK) ||
			CMousePointer::IsMouseBack())
		{
			if (CVideoEditorUi::ShowOptionsMenu())
			{
				ms_iProjectPositionToRevertOnUpMs = ms_iProjectPositionToRevertOnDownMs = -1;  // we moved so reset

				if (!CMousePointer::HasMouseInputOccurred())
				{
					CVideoEditorMenu::SetItemActive(0);
					CVideoEditorMenu::UpdatePane();
				}
		
				g_FrontendAudioEntity.PlaySound( "BACK", "HUD_FRONTEND_DEFAULT_SOUNDSET" );

				return;
			}
		}	
	}

	if (!CVideoEditorUi::IsProjectPopulated())
	{
		return;
	}

	bool ignoreShoulderButtonsThisFrame = false;
	float const c_xAxisL = CControlMgr::GetMainFrontendControl().GetFrontendLeftRight().GetNorm();
	float const c_xAxisR = CControlMgr::GetMainFrontendControl().GetFrontendRStickLeftRight().GetNorm();

#if RSG_PC
	if (c_xAxisL != 0.0f || c_xAxisR != 0.0f)  // if we move either pad axis, then clear any mouse input and hide the mouse pointer (fixes 2286679)
	{
		CMousePointer::SetKeyPressed();
	}
#endif // RSG_PC

	if (!IsPreviewing() && IsTimelineItemNonVideo(GetItemSelected()))
	{
		u32 const c_playheadPositionMs = GetPlayheadPositionMs();

		static s32 projectIndexCurrentlyTrimming = -1;

		bool const c_ButtonPressedLB = (CControlMgr::GetMainFrontendControl().GetFrontendLB().IsPressed());
		bool const c_ButtonPressedRB = (CControlMgr::GetMainFrontendControl().GetFrontendRB().IsPressed());
		bool const c_ButtonReleasedLB = (CControlMgr::GetMainFrontendControl().GetFrontendLB().IsReleased());
		bool const c_ButtonReleasedRB = (CControlMgr::GetMainFrontendControl().GetFrontendRB().IsReleased());

		const bool c_mouseHeldThisFrame = RSG_PC && ((ioMouse::GetButtons() & ioMouse::MOUSE_LEFT) != 0);
		const bool c_mousePressedThisFrame = RSG_PC && ((ioMouse::GetPressedButtons() & ioMouse::MOUSE_LEFT) != 0);
		const bool c_mouseReleasedThisFrame = RSG_PC && ((ioMouse::GetReleasedButtons() & ioMouse::MOUSE_LEFT) != 0);

		bool hoveringOverLeft = false;
		bool hoveringOverRight = false;

		s32 const c_projectIndex = projectIndexCurrentlyTrimming == -1 ? GetProjectIndexNearestStartPos(GetItemSelected(), c_playheadPositionMs, -1) : projectIndexCurrentlyTrimming;

#if RSG_PC
		if (IsMouseWithinTimelineArea() && CMousePointer::HasMouseInputOccurred())
		{
			if (c_projectIndex != -1)
			{
				u32 const c_mousePosMs = GetMousePositionMs();  // get the current mouse pos in ms

				if (GetItemSelected() == PLAYHEAD_TYPE_TEXT)
				{
					if (c_mousePosMs > GetProject()->GetOverlayText(c_projectIndex).getStartTimeMs() - MOUSE_DEADZONE_AMOUNT_IN_MS && c_mousePosMs < GetProject()->GetOverlayText(c_projectIndex).getStartTimeMs() + MOUSE_DEADZONE_AMOUNT_IN_MS)
					{
						hoveringOverLeft = true;
					}
					else if (c_mousePosMs > GetProject()->GetOverlayText(c_projectIndex).getStartTimeMs() + GetProject()->GetOverlayText(c_projectIndex).getDurationMs() - MOUSE_DEADZONE_AMOUNT_IN_MS && c_mousePosMs < GetProject()->GetOverlayText(c_projectIndex).getStartTimeMs() + GetProject()->GetOverlayText(c_projectIndex).getDurationMs() + MOUSE_DEADZONE_AMOUNT_IN_MS)
					{
						hoveringOverRight = true;
					}
				}
				else if (GetItemSelected() == PLAYHEAD_TYPE_AMBIENT)
				{
					if (c_mousePosMs > GetProject()->GetAmbientClip(c_projectIndex).getStartTimeWithOffsetMs() - MOUSE_DEADZONE_AMOUNT_IN_MS && c_mousePosMs < GetProject()->GetAmbientClip(c_projectIndex).getStartTimeWithOffsetMs() + MOUSE_DEADZONE_AMOUNT_IN_MS)
					{
						hoveringOverLeft = true;
					}
					else if (c_mousePosMs > GetProject()->GetAmbientClip(c_projectIndex).getStartTimeWithOffsetMs() + GetProject()->GetAmbientClip(c_projectIndex).getTrimmedDurationMs() - MOUSE_DEADZONE_AMOUNT_IN_MS && c_mousePosMs < GetProject()->GetAmbientClip(c_projectIndex).getStartTimeWithOffsetMs() + GetProject()->GetAmbientClip(c_projectIndex).getTrimmedDurationMs() + MOUSE_DEADZONE_AMOUNT_IN_MS)
					{
						hoveringOverRight = true;
					}
				}
				else if (GetItemSelected() == PLAYHEAD_TYPE_AUDIO)
				{
					if (c_mousePosMs > GetProject()->GetMusicClip(c_projectIndex).getStartTimeWithOffsetMs() - MOUSE_DEADZONE_AMOUNT_IN_MS && c_mousePosMs < GetProject()->GetMusicClip(c_projectIndex).getStartTimeWithOffsetMs() + MOUSE_DEADZONE_AMOUNT_IN_MS)
					{
						hoveringOverLeft = true;
					}
					else if (c_mousePosMs > GetProject()->GetMusicClip(c_projectIndex).getStartTimeWithOffsetMs() + GetProject()->GetMusicClip(c_projectIndex).getTrimmedDurationMs() - MOUSE_DEADZONE_AMOUNT_IN_MS && c_mousePosMs < GetProject()->GetMusicClip(c_projectIndex).getStartTimeWithOffsetMs() + GetProject()->GetMusicClip(c_projectIndex).getTrimmedDurationMs() + MOUSE_DEADZONE_AMOUNT_IN_MS)
					{
						hoveringOverRight = true;
					}
				}
			}
		}
#endif

		if ( (c_mousePressedThisFrame || c_ButtonPressedLB || c_ButtonPressedRB) && (projectIndexCurrentlyTrimming == -1) )
		{
			s32 const c_projectIndex = GetProjectIndexNearestStartPos(GetItemSelected(), c_playheadPositionMs, -1);

			if (c_projectIndex != -1)
			{
				projectIndexCurrentlyTrimming = c_projectIndex;

#if RSG_PC
				if (ms_trimmingClip || IsMouseWithinTimelineArea())
				{
					if (projectIndexCurrentlyTrimming != -1)
					{
						ms_trimmingClipLeft = hoveringOverLeft || c_ButtonPressedLB;
						ms_trimmingClipRight = hoveringOverRight || c_ButtonPressedRB;
					}
				}
#endif

				if (c_ButtonPressedLB || c_ButtonPressedRB)
				{
					ms_trimmingClipLeft = c_ButtonPressedLB;
					ms_trimmingClipRight = c_ButtonPressedRB;
				}
			}
		}

		if (c_projectIndex != -1)
		{
			if (ms_ComplexObject[TIMELINE_OBJECT_PLAYHEAD_TRIMMING_ICON].IsActive())
			{
				ms_ComplexObject[TIMELINE_OBJECT_PLAYHEAD_TRIMMING_ICON].SetAlpha(0);
				ms_ComplexObject[TIMELINE_OBJECT_PLAYHEAD_TRIMMING_ICON].SetYPositionInPixels(GetItemSelected() == PLAYHEAD_TYPE_AMBIENT ? TRIMMING_ICON_YPOS_AMBIENT : GetItemSelected() == PLAYHEAD_TYPE_AUDIO ? TRIMMING_ICON_YPOS_AUDIO : TRIMMING_ICON_YPOS_TEXT);

				if ((hoveringOverLeft && !ms_trimmingClipRight) || (ms_trimmingClip && ms_trimmingClipLeft))
				{
					if ((s32)c_playheadPositionMs == GetEndTimeOfClipMs(GetItemSelected(), c_projectIndex))
					{
						const u32 c_durationMs = GetDurationOfClipMs(GetItemSelected(), c_projectIndex);
						ms_ComplexObject[TIMELINE_OBJECT_PLAYHEAD_TRIMMING_ICON].SetXPositionInPixels(-MsToPixels(c_durationMs));
					}
					else
					{
						ms_ComplexObject[TIMELINE_OBJECT_PLAYHEAD_TRIMMING_ICON].SetXPositionInPixels(0);
					}

					ms_ComplexObject[TIMELINE_OBJECT_PLAYHEAD_TRIMMING_ICON].SetAlpha(255);
				}
				else if ((hoveringOverRight && !ms_trimmingClipLeft) || (ms_trimmingClip && ms_trimmingClipRight))
				{
					if ((s32)c_playheadPositionMs == GetStartTimeOfClipMs(GetItemSelected(), c_projectIndex))
					{
						const u32 c_durationMs = GetDurationOfClipMs(GetItemSelected(), c_projectIndex);
						ms_ComplexObject[TIMELINE_OBJECT_PLAYHEAD_TRIMMING_ICON].SetXPositionInPixels(MsToPixels(c_durationMs));
					}
					else
					{
						ms_ComplexObject[TIMELINE_OBJECT_PLAYHEAD_TRIMMING_ICON].SetXPositionInPixels(0);
					}

					ms_ComplexObject[TIMELINE_OBJECT_PLAYHEAD_TRIMMING_ICON].SetAlpha(255);
				}
			}
		}

#if RSG_PC
		if (ms_trimmingClipLeft && ms_trimmingClipRight)
		{
			ms_mousePressedIndexFromActionscript = -1;
			ms_mouseReleasedIndexFromActionscript = -1;
		}
#endif // RSG_PC

		s32 projectIndexWasTrimmingThisFrame = -1;
		bool wasTrimmingClipLeftThisFrame = false;
		if ( (c_mouseReleasedThisFrame || c_ButtonReleasedLB || c_ButtonReleasedRB) && (projectIndexCurrentlyTrimming != -1) )
		{
			projectIndexWasTrimmingThisFrame = projectIndexCurrentlyTrimming;
			projectIndexCurrentlyTrimming = -1;

			wasTrimmingClipLeftThisFrame = ms_trimmingClipLeft;

			ms_trimmingClipLeft = false;
			ms_trimmingClipRight = false;
		}

		//
		// trimming
		//
		s32 directMoveMs = -1;
		s32 trimDir = 0;
		bool snap = false;
		static u32 s_trimButtonTimer = fwTimer::GetSystemTimeInMilliseconds();
		static u32 s_trimTimer = fwTimer::GetSystemTimeInMilliseconds();

#if RSG_PC
		u32 const c_mousePosMs = GetMousePositionMs();  // get the current mouse pos in ms
		static u32 s_oldMousePosMs = c_mousePosMs+1;
#endif // #if RSG_PC

		if (ms_trimmingClipLeft || ms_trimmingClipRight)
		{
			if (fwTimer::GetSystemTimeInMilliseconds() > s_trimButtonTimer + 500)
			{
				if (!ms_trimmingClip)
				{
					if (IsTimelineItemAudioOrAmbient(GetItemSelected()))
					{
						ms_ComplexObject[TIMELINE_OBJECT_PLAYHEAD_MARKER_LINE].GotoFrame(2);  // dotted line on audio
					}

					ms_trimmingClipOriginalPositionMs = (s32)c_playheadPositionMs;
					ms_trimmingClip = true;
					CVideoEditorInstructionalButtons::Refresh();

					if (projectIndexCurrentlyTrimming != -1)
					{
						if (IsTimelineItemAudioOrAmbient(GetItemSelected()))
						{
							AddGhostTrack(projectIndexCurrentlyTrimming);
						}
					}

					HighlightClip(GetTimelineIndexFromProjectIndex(GetItemSelected(), projectIndexCurrentlyTrimming), true);
				}

				s32 timerIteration = ms_trimmingClipIncrementMultiplier > 4 ? ADJUSTMENT_TIMER_STANDARD : ADJUSTMENT_TIMER_SLOW;
				if (c_mouseHeldThisFrame || fwTimer::GetSystemTimeInMilliseconds() > s_trimTimer + timerIteration)
				{
					s_trimTimer = fwTimer::GetSystemTimeInMilliseconds();

					if (!c_mouseHeldThisFrame)
					{
						if (c_xAxisR > TIMELINE_AXIS_THRESHOLD)
						{
							trimDir = 1;
						}
						else if (c_xAxisR < -TIMELINE_AXIS_THRESHOLD)
						{
							trimDir = -1;
						}
						else
						{
							ms_trimmingClipIncrementMultiplier = 0;
						}

						if (trimDir != 0 && ms_trimmingClipIncrementMultiplier < 50)  // 500ms is the biggest increment
						{
							ms_trimmingClipIncrementMultiplier++;
						}
					}
#if RSG_PC
					else // otherwise, on PC check the mouse pos
					{
						if (!ms_waitForMouseMovement || c_mousePosMs != s_oldMousePosMs)
						{
							ms_waitForMouseMovement = false;

							u32 const c_projectTotalTimeMs = GetTotalTimelineTimeMs();
							s32 const c_dir = GetMouseMovementDirInTimelineArea();
							s32 const c_timerLimit = ADJUSTMENT_TIMER_STANDARD;
							static u32 s_trimmingMovementTimer = fwTimer::GetSystemTimeInMilliseconds();

							if (!CPauseMenu::CheckInput(FRONTEND_INPUT_LEFT) && !CPauseMenu::CheckInput(FRONTEND_INPUT_RIGHT))
							{
								if (c_dir != 0)
								{
									if (c_mousePosMs < 0 && c_mousePosMs > c_projectTotalTimeMs || fwTimer::GetSystemTimeInMilliseconds() > s_trimmingMovementTimer + c_timerLimit)  // slower timer when previewing video
									{
										s32 newPlayheadPos = GetPlayheadPositionMs() + c_dir;

										if (newPlayheadPos < 0)
										{
											newPlayheadPos = 0;
										}

										if (newPlayheadPos > c_projectTotalTimeMs)
										{
											newPlayheadPos = c_projectTotalTimeMs;
										}

										trimDir = c_mousePosMs > s_oldMousePosMs ? c_dir : c_dir;
										directMoveMs = -1;

										s_trimmingMovementTimer = fwTimer::GetSystemTimeInMilliseconds();
										ms_trimmingClipIncrementMultiplier = 1;
									}
								}
								else if (c_mousePosMs >= 0 && c_mousePosMs <= c_projectTotalTimeMs)
								{
									trimDir = c_mousePosMs > s_oldMousePosMs ? 1 : -1;
									directMoveMs = c_mousePosMs;
									ms_trimmingClipIncrementMultiplier = 1;
								}
							}
						}
					}
#endif // #if RSG_PC
				}
				
				if (c_xAxisR >= -TIMELINE_AXIS_THRESHOLD && c_xAxisR <= TIMELINE_AXIS_THRESHOLD)
				{
					s_trimTimer = fwTimer::GetSystemTimeInMilliseconds();

					if (CPauseMenu::CheckInput(FRONTEND_INPUT_LEFT, false, CHECK_INPUT_OVERRIDE_FLAG_IGNORE_ANALOGUE_STICKS) || CMousePointer::IsMouseWheelUp())
					{
						trimDir = -1;
						directMoveMs = -1;
						snap = true;
#if RSG_PC
						ms_waitForMouseMovement = true;
#endif
					}
					else if (CPauseMenu::CheckInput(FRONTEND_INPUT_RIGHT, false, CHECK_INPUT_OVERRIDE_FLAG_IGNORE_ANALOGUE_STICKS) || CMousePointer::IsMouseWheelDown())
					{
						trimDir = 1;
						directMoveMs = -1;
						snap = true;
#if RSG_PC
						ms_waitForMouseMovement = true;
#endif
					}
				}
			}
			else
			{
				if (ms_ComplexObject[TIMELINE_OBJECT_PLAYHEAD_TRIMMING_ICON].IsActive())
				{
					ms_ComplexObject[TIMELINE_OBJECT_PLAYHEAD_TRIMMING_ICON].SetAlpha(0);
				}
			}
		}
		else
		{
			if (ms_trimmingClip)
			{
				ms_ComplexObject[TIMELINE_OBJECT_PLAYHEAD_MARKER_LINE].GotoFrame(1);

				ms_trimmingClip = false;
				ms_trimmingClipIncrementMultiplier = 0;

#if RSG_PC
				ms_mousePressedIndexFromActionscript = -1;
				ms_mouseReleasedIndexFromActionscript = -1;
#endif
				ms_heldDownAndScrubbingOverviewBar = ms_heldDownAndScrubbingTimeline = false;

				if (projectIndexWasTrimmingThisFrame != -1)
				{
					TrimObject(GetItemSelected(), projectIndexWasTrimmingThisFrame, 0, wasTrimmingClipLeftThisFrame, false, false);  // last update to reapply any text/icon

					if (IsTimelineItemAudioOrAmbient(GetItemSelected()))
					{
						const s32 iTimelineIndexOfGhostClip = CVideoEditorTimeline::GetTimelineIndexFromProjectIndex(PLAYHEAD_TYPE_GHOST, projectIndexWasTrimmingThisFrame);

						if (iTimelineIndexOfGhostClip != -1)
						{
							DeleteClip(iTimelineIndexOfGhostClip);
						}
					}

					if (ms_trimmingClipOriginalPositionMs != -1)  // if we have an original pos stored then revert to it when we finished trimming
					{
						SetPlayheadPositionMs(ms_trimmingClipOriginalPositionMs, true);
						ms_trimmingClipOriginalPositionMs = -1;
					}
					else
					{
						RepositionPlayheadIfNeeded(false);

						if (wasTrimmingClipLeftThisFrame)
						{
							SetPlayheadPositionMs(GetStartTimeOfClipMs(GetItemSelected(), projectIndexWasTrimmingThisFrame), true);
						}
						else
						{
							SetPlayheadPositionMs(GetEndTimeOfClipMs(GetItemSelected(), projectIndexWasTrimmingThisFrame), true);
						}

						UpdateOverviewBar();
					}
				}

				ignoreShoulderButtonsThisFrame = true;
				CVideoEditorInstructionalButtons::Refresh();

				if (ms_ComplexObject[TIMELINE_OBJECT_PLAYHEAD_TRIMMING_ICON].IsActive())
				{
					ms_ComplexObject[TIMELINE_OBJECT_PLAYHEAD_TRIMMING_ICON].SetXPositionInPixels(0);
					ms_ComplexObject[TIMELINE_OBJECT_PLAYHEAD_TRIMMING_ICON].SetAlpha(0);
				}
			}

			s_trimButtonTimer = fwTimer::GetSystemTimeInMilliseconds();
		}

		if (ms_trimmingClip)
		{
			if (projectIndexCurrentlyTrimming != -1)
			{
				TrimObject(GetItemSelected(), projectIndexCurrentlyTrimming, trimDir, ms_trimmingClipLeft, snap, directMoveMs);

				if (c_playheadPositionMs != GetPlayheadPositionMs())  // playhead has moved, so play the sound effect
				{
					ms_wantScrubbingSoundThisFrame |= true;

#if RSG_PC
					s_oldMousePosMs = GetMousePositionMs();  // store the new mouse position for the following frame check
#endif
				}
			}
		}

	}

	if (ms_trimmingClip)
	{
		return;  // no more input if we are trimming
	}

	if (IsPreviewingNonVideo())
	{
		if (c_xAxisL != 0.0f)
		{
			AdjustPlayheadFromAxis(c_xAxisL);

			ms_wantScrubbingSoundThisFrame |= true;
		}
	}

	s32 inputDirection = 0;

	if (IsPreviewingVideo() || !IsPreviewing())
	{
		static u32 s_StickTimer = 0;

		if (c_xAxisL != 0.0f)
		{
			if (ms_stickTimerAcc == TIMELINE_MAX_INPUT_ACC || fwTimer::GetSystemTimeInMilliseconds() > s_StickTimer + ms_stickTimerAcc)
			{
				if (c_xAxisL < -TIMELINE_AXIS_THRESHOLD)
				{
					inputDirection = -1;
				}

				if (c_xAxisL > TIMELINE_AXIS_THRESHOLD)
				{
					inputDirection = 1;
				}

				if (inputDirection != 0)
				{
					s_StickTimer = fwTimer::GetSystemTimeInMilliseconds();

					if (ms_stickTimerAcc > TIMELINE_MIN_INPUT_ACC)
					{
						ms_stickTimerAcc -= TIMELINE_MIN_INPUT_DELTA;

						if (ms_stickTimerAcc < TIMELINE_MIN_INPUT_ACC)
						{
							ms_stickTimerAcc = TIMELINE_MIN_INPUT_ACC;
						}
					}
				}
			}
		}
		else
		{
			s_StickTimer = fwTimer::GetSystemTimeInMilliseconds();
			ms_stickTimerAcc = TIMELINE_MAX_INPUT_ACC;
		}
	}

	if ( (CPauseMenu::CheckInput(FRONTEND_INPUT_LEFT, false, CHECK_INPUT_OVERRIDE_FLAG_IGNORE_ANALOGUE_STICKS) || ms_iScrollTimeline < 0) ||
		 (CControlMgr::GetMainFrontendControl().GetFrontendLB().IsReleased() && !ignoreShoulderButtonsThisFrame) ||
		 (CMousePointer::IsMouseWheelUp()) ||
		 (inputDirection < 0) )
	{
		RepositionPlayheadIfNeeded(false);  // reposition before we snap (fixes 2375028)

#if RSG_PC
		ms_waitForMouseMovement = true;
#endif

		// reset some flags:
		ms_iScrollTimeline = 0;
		ms_iProjectPositionToRevertOnUpMs = ms_iProjectPositionToRevertOnDownMs = -1;

		bool const c_precisionSnap = IsPreviewingNonVideo() && (!CControlMgr::GetMainFrontendControl().GetFrontendLB().IsReleased());
		u32 const c_playheadPositionMs = GetPlayheadPositionMs();  // current playhead position
		u32 const c_newPlayheadPositionMs = AdjustToNearestSnapPoint(c_playheadPositionMs, -1, c_precisionSnap);  // intended/new playhead position

		if (c_newPlayheadPositionMs < c_playheadPositionMs)  // final check to ensure we are moving in the correct direction
		{
			SetPlayheadPositionMs(c_newPlayheadPositionMs, true);  // set the new position
			g_FrontendAudioEntity.PlaySound( "NAV_LEFT_RIGHT", "HUD_FRONTEND_DEFAULT_SOUNDSET" );  // play sound effect
		}

		return;
	}

	if ( (CPauseMenu::CheckInput(FRONTEND_INPUT_RIGHT, false, CHECK_INPUT_OVERRIDE_FLAG_IGNORE_ANALOGUE_STICKS) || ms_iScrollTimeline > 0) ||
		 (CControlMgr::GetMainFrontendControl().GetFrontendRB().IsReleased() && !ignoreShoulderButtonsThisFrame) ||
		 (CMousePointer::IsMouseWheelDown()) ||
		 (inputDirection > 0) )
	{
		RepositionPlayheadIfNeeded(false);  // reposition before we snap (fixes 2375028)

#if RSG_PC
		ms_waitForMouseMovement = true;
#endif

		// reset some flags:
		ms_iScrollTimeline = 0;
		ms_iProjectPositionToRevertOnUpMs = ms_iProjectPositionToRevertOnDownMs = -1;

		bool const c_precisionSnap = IsPreviewingNonVideo() && (!CControlMgr::GetMainFrontendControl().GetFrontendRB().IsReleased());
		u32 const c_playheadPositionMs = GetPlayheadPositionMs();  // current playhead position
		u32 const c_newPlayheadPositionMs = AdjustToNearestSnapPoint(c_playheadPositionMs, 1, c_precisionSnap);  // intended/new playhead position

		if (c_newPlayheadPositionMs > c_playheadPositionMs)  // final check to ensure we are moving in the correct direction
		{
			SetPlayheadPositionMs(c_newPlayheadPositionMs, true);  // set the new position
			g_FrontendAudioEntity.PlaySound( "NAV_LEFT_RIGHT", "HUD_FRONTEND_DEFAULT_SOUNDSET" );  // play sound effect
		}

		return;
	}

	if (!IsPreviewing())
	{
		const u32 uPlayheadPos = GetPlayheadPositionMs();
		ePLAYHEAD_TYPE const c_itemSelected = GetItemSelected();
		CVideoEditorProject* project = GetProject();

		if (project && !IsTimelineItemAudioOrAmbient(c_itemSelected))  // cannot edit audio
		{
			if (c_itemSelected != PLAYHEAD_TYPE_TEXT || project->GetClipCount() > 0)  // can only edit text if we have some video clips
			{
				if (CPauseMenu::CheckInput(FRONTEND_INPUT_ACCEPT))
				{
					ms_iProjectPositionToRevertOnUpMs = ms_iProjectPositionToRevertOnDownMs = -1;  // we moved so reset

					bool canEditItem = true;
					char const * errorState = NULL;
					bool const c_overEndOfProjectTime = uPlayheadPos > GetProject()->GetTotalClipTimeMs();

					if( c_itemSelected == PLAYHEAD_TYPE_VIDEO )
					{
						const u32 c_projectIndex = GetProjectIndexAtTime(PLAYHEAD_TYPE_VIDEO, uPlayheadPos);
						canEditItem = project->IsClipReferenceValid( c_projectIndex );
					
						if( !canEditItem )
						{
							errorState = "CLIP_DELETED_FROM_DISK";
							ClearAllClips();

							project->DeleteClip( c_projectIndex );
						}
					}

					if( canEditItem && !c_overEndOfProjectTime)
					{
						g_FrontendAudioEntity.PlaySound( "SELECT", "HUD_FRONTEND_DEFAULT_SOUNDSET" );

						CVideoEditorMenu::SetUserConfirmationScreenWithAction(
							"TRIGGER_EDIT_CLIP", c_itemSelected == PLAYHEAD_TYPE_TEXT ? "VE_EDIT_TEXT_SURE" : "VE_EDIT_CLIP_SURE");
					}
					else if( errorState )
					{
						g_FrontendAudioEntity.PlaySound("ERROR","HUD_FRONTEND_DEFAULT_SOUNDSET");

						CVideoEditorUi::SetErrorState( errorState );
						CVideoEditorUi::RebuildTimeline();
					}

					return;
				}
			}
		}

		if (ms_previewingAudioTrack == -1)
		{
			if ( CControlMgr::GetMainFrontendControl().GetReplayPreviewAudio().IsReleased())
			{
				if (project && IsTimelineItemAudioOrAmbient(c_itemSelected))
				{
					s32 const c_projectIndex = GetProjectIndexAtTime(c_itemSelected, uPlayheadPos);

					if (c_projectIndex != -1)
					{
						// play the track from the offset and for the duration, no rockout jump and no fades
						u32 const c_soundHash = c_itemSelected == PLAYHEAD_TYPE_AMBIENT ? project->GetAmbientClip(c_projectIndex).getClipSoundHash() : project->GetMusicClip(c_projectIndex).getClipSoundHash();
						u32 const c_startOffset = c_itemSelected == PLAYHEAD_TYPE_AMBIENT ? project->GetAmbientClip(c_projectIndex).getStartOffsetMs() : project->GetMusicClip(c_projectIndex).getStartOffsetMs();
						u32 const c_duration = GetDurationOfClipMs(c_itemSelected, c_projectIndex);

						ms_uProjectPositionToRevertMs = GetPlayheadPositionMs();

						g_RadioAudioEntity.StartReplayTrackPreview(c_soundHash, c_startOffset, c_duration, false, false);

						ms_previewingAudioTrack = c_projectIndex;

						CVideoEditorInstructionalButtons::Refresh();

						return;
					}
				}
			}
		}
		
		if( ( c_wasKbmLastFrame && CControlMgr::GetMainFrontendControl().GetReplayCtrl().IsDown() && CControlMgr::GetMainFrontendControl().GetReplayTimelineDuplicateClip().IsReleased() ) ||
			( !c_wasKbmLastFrame && CPauseMenu::CheckInput(FRONTEND_INPUT_L3 ) ) )
		{
			if (!IsPreviewing() && !ms_trimmingClip)
			{
				ms_iProjectPositionToRevertOnUpMs = ms_iProjectPositionToRevertOnDownMs = -1;  // we moved so reset
				bool reposition = false;

				switch (GetItemSelected())
				{
					case PLAYHEAD_TYPE_VIDEO:
						{
							if( GetProject()->CanAddClip() )
							{
								const s32 c_projectIndex = GetProjectIndexAtTime(PLAYHEAD_TYPE_VIDEO, uPlayheadPos);

								if (c_projectIndex != -1)
								{
									CVideoEditorUi::PickupSelectedClip(c_projectIndex, false);
									reposition = true;
								}
							}
							break;
						}

					case PLAYHEAD_TYPE_AMBIENT:
						{
							if (GetProject()->CanAddAmbient())  // can only duplicate if it is a score track (2206982)
							{
								const s32 c_projectIndex = GetProjectIndexAtTime(PLAYHEAD_TYPE_AMBIENT, uPlayheadPos);

								if (c_projectIndex != -1)
								{
									CVideoEditorUi::PickupSelectedClip(c_projectIndex, false);
									reposition = true;
								}
							}

							break;
						}

					case PLAYHEAD_TYPE_AUDIO:
						{
							bool const c_isScoreTrack = IsPlayheadAudioScore(uPlayheadPos, PLAYHEAD_TYPE_AUDIO);
							bool const c_isCommercialTrack = IsPlayheadCommercial(uPlayheadPos, PLAYHEAD_TYPE_AUDIO);

							if( (c_isScoreTrack || c_isCommercialTrack) && GetProject()->CanAddMusic() )  // can only duplicate if it is a score track (2206982)
							{
								const s32 c_projectIndex = GetProjectIndexAtTime(PLAYHEAD_TYPE_AUDIO, uPlayheadPos);

								if (c_projectIndex != -1)
								{
									CVideoEditorUi::PickupSelectedClip(c_projectIndex, false);
									reposition = true;
								}
							}

							break;
						}

					case PLAYHEAD_TYPE_TEXT:
						{
							if( GetProject()->CanAddText() )
							{
								const s32 c_projectIndex = GetProjectIndexAtTime(PLAYHEAD_TYPE_TEXT, uPlayheadPos);

								if (c_projectIndex != -1)
								{
									CVideoEditorUi::PickupSelectedClip(c_projectIndex, false);
									reposition = true;
								}
							}

							break;
						}

					default:
					{
						uiAssertf(0, "CVideoEditorTimeline::UpdateInput() - invalid type passed (%d)", (s32)GetItemSelected());
					}
				}

				if( reposition )
				{
					RepositionPlayheadIfNeeded(false);
				}

				return;
			}
		}

		if( ( c_wasKbmLastFrame && CControlMgr::GetMainFrontendControl().GetReplayClipDelete().IsReleased() ) ||
			( !c_wasKbmLastFrame && CPauseMenu::CheckInput(FRONTEND_INPUT_Y ) ) )
		{
			g_FrontendAudioEntity.PlaySound( "SELECT", "HUD_FRONTEND_DEFAULT_SOUNDSET" );

			ms_iProjectPositionToRevertOnUpMs = ms_iProjectPositionToRevertOnDownMs = -1;  // we moved so reset

			CVideoEditorMenu::SetUserConfirmationScreenWithAction("TRIGGER_DELETE_CLIP_FROM_TIMELINE",
																	GetItemSelected() == PLAYHEAD_TYPE_TEXT ? "VE_DELETE_TEXT_SURE" :
																	GetItemSelected() == PLAYHEAD_TYPE_AUDIO ? "VE_DELETE_AUD_SURE" :
																	GetItemSelected() == PLAYHEAD_TYPE_AMBIENT ? "VE_DELETE_AMB_SURE" :
																	"VE_DELETE_CLIP_SURE");

			return;
		}

		if ( (!CPauseMenu::CheckInput(FRONTEND_INPUT_LEFT)) && (!CPauseMenu::CheckInput(FRONTEND_INPUT_RIGHT)) )  // do not check up/down unless left right are not held
		{
			if (CPauseMenu::CheckInput(FRONTEND_INPUT_UP))
			{
				if (GetItemSelected() != PLAYHEAD_TYPE_VIDEO)
				{
					const ePLAYHEAD_TYPE newItem = (ePLAYHEAD_TYPE)((s32)(GetItemSelected()) - 1);

					if (SetItemSelected(newItem))
					{
						g_FrontendAudioEntity.PlaySound( "NAV_UP_DOWN", "HUD_FRONTEND_DEFAULT_SOUNDSET" );
					}

					const u32 uPlayheadPositionMs = GetPlayheadPositionMs();

					if (ms_iProjectPositionToRevertOnDownMs != -1)
					{
						SetPlayheadPositionMs((u32)ms_iProjectPositionToRevertOnDownMs, false); // revert back to old position if we havnt moved left/right since previous move
						ms_iProjectPositionToRevertOnDownMs = -1;
					}

					ms_iProjectPositionToRevertOnUpMs = uPlayheadPositionMs;

					UpdatePlayheadPreview(PLAYHEAD_TYPE_NONE);

					const s32 iProjectIndex = GetProjectIndexNearestStartPos(GetItemSelected(), uPlayheadPositionMs);
					SetPlayheadToStartOfProjectClip(GetItemSelected(), (iProjectIndex != -1) ? (u32)iProjectIndex : 0, true);  // snap to nearest clip

					RefreshStagingArea();
				}
			}

			if (CPauseMenu::CheckInput(FRONTEND_INPUT_DOWN))
			{
				if (GetItemSelected() != PLAYHEAD_TYPE_TEXT)
				{
					const ePLAYHEAD_TYPE newItem = (ePLAYHEAD_TYPE)((s32)(GetItemSelected()) + 1);

					if (SetItemSelected(newItem))
					{
						g_FrontendAudioEntity.PlaySound( "NAV_UP_DOWN", "HUD_FRONTEND_DEFAULT_SOUNDSET" );
					}

					const u32 uPlayheadPositionMs = GetPlayheadPositionMs();

					if (ms_iProjectPositionToRevertOnUpMs != -1)
					{
						SetPlayheadPositionMs((u32)ms_iProjectPositionToRevertOnUpMs, false); // revert back to old position if we havnt moved left/right since previous move
						ms_iProjectPositionToRevertOnUpMs = -1;
					}

					ms_iProjectPositionToRevertOnDownMs = uPlayheadPositionMs;

					UpdatePlayheadPreview(PLAYHEAD_TYPE_NONE);  // set the playhead to be the updated type, but not previewing

					const s32 iProjectIndex = GetProjectIndexNearestStartPos(GetItemSelected(), uPlayheadPositionMs);
					SetPlayheadToStartOfProjectClip(GetItemSelected(), (iProjectIndex != -1) ? (u32)iProjectIndex : 0, true);  // snap to nearest clip

					RefreshStagingArea();
				}
			}
		}
	}
}



#if RSG_PC
/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorTimeline::UpdateMouseInput
// PURPOSE:	updates mouse input for navigating/scrolling the timeline
/////////////////////////////////////////////////////////////////////////////////////////
bool CVideoEditorTimeline::UpdateMouseInput()
{
//	CMousePointer::SetMouseCursorVisible(!ms_trimmingClip);  // mouse pointer only visible in video editor timeline when we are not trimming

	bool mousePointerChanged = false;

	bool const c_withinOverviewBarYLimits = IsMouseWithinOverviewArea();

	if (!CVideoEditorUi::IsProjectPopulated())
	{
		return mousePointerChanged;
	}

	if (!CMousePointer::HasMouseInputOccurred())
	{
		return mousePointerChanged;
	}

	if (!IsPreviewing())
	{
		if (CMousePointer::IsMouseWheelUp())
		{
			ms_iScrollTimeline = -1;
		}

		if (CMousePointer::IsMouseWheelDown())
		{
			ms_iScrollTimeline = 1;
		}
	}

	if (ms_ComplexObject[TIMELINE_OBJECT_TIMELINE_CONTAINER].IsActive() && ms_ComplexObject[TIMELINE_OBJECT_TIMELINE_PANEL].IsActive())
	{
		u32 const c_mousePosMs = GetMousePositionMs();  // get the current mouse pos in ms

		static bool s_mousePressedLastFrame = false;

		if (ms_mouseReleasedTypeFromActionscript != PLAYHEAD_TYPE_NONE && ms_mouseReleasedIndexFromActionscript >= 0)
		{
			if (ms_mouseReleasedTypeFromActionscript != PLAYHEAD_TYPE_OVERVIEW_VIDEO)
			{
				SetItemSelected(ms_mouseReleasedTypeFromActionscript);
				g_FrontendAudioEntity.PlaySound( "NAV_LEFT_RIGHT", "HUD_FRONTEND_DEFAULT_SOUNDSET" );  // play sound effect
			}

			UpdatePlayheadPreview(PLAYHEAD_TYPE_NONE);

			u32 const c_projectIndex = GetProjectIndexFromTimelineIndex(ms_mouseReleasedIndexFromActionscript);
			SetPlayheadToStartOfProjectClip(GetItemSelected(), c_projectIndex, true);

			// reset
			ms_mousePressedTypeFromActionscript = PLAYHEAD_TYPE_NONE;
			ms_mouseReleasedTypeFromActionscript = PLAYHEAD_TYPE_NONE;
			ms_mousePressedIndexFromActionscript = -1;
			ms_mouseReleasedIndexFromActionscript = -1;
			ms_mouseHoveredIndexFromActionscript = -1;

			s_mousePressedLastFrame = false;

			CMousePointer::SetMouseCursorStyle(MOUSE_CURSOR_STYLE_ARROW);

			return mousePointerChanged;
		}

		u32 const c_currentProjectIndexAtPlayhead = GetProjectIndexAtTime(GetItemSelected(), GetPlayheadPositionMs());

		const bool c_mousePressedThisFrame = ( (ioMouse::GetButtons() & ioMouse::MOUSE_LEFT) != 0 || (!ms_heldDownAndScrubbingTimeline && ms_mousePressedIndexFromActionscript >= 0 ) );

		//
		// check if the mouse is being held down
		//
		if (c_mousePressedThisFrame && s_mousePressedLastFrame)  // is pressed this frame AND was pressed last time, means we are held
		{
			if (ms_mousePressedTypeFromActionscript == PLAYHEAD_TYPE_OVERVIEW_VIDEO)
			{
				if (c_withinOverviewBarYLimits)
				{
					if (!ms_heldDownAndScrubbingOverviewBar)	
					{
						ms_heldDownAndScrubbingOverviewBar = true;
					}
				}
			}
		}
		else if (c_mousePressedThisFrame && !s_mousePressedLastFrame)  // is pressed this frame but wasnt pressed last frame, means its the 1st time we have pressed
		{
			// so start the timer:
			ms_heldDownTimer = fwTimer::GetSystemTimeInMilliseconds();
		}

		//
		// check states when we are not scrubbing
		//
		if (!IsPreviewing() && !ms_heldDownAndScrubbingTimeline && !ms_heldDownAndScrubbingOverviewBar && !ms_trimmingClip)
		{
			// we are held down
			//
			// if we have held down for a while, then pick it up
			//
			if (ms_mousePressedIndexFromActionscript >= 0)  // if its a different clip, then set the playhead to it
			{
				u32 const c_projectIndex = GetProjectIndexFromTimelineIndex(ms_mousePressedIndexFromActionscript);

				if (c_projectIndex == c_currentProjectIndexAtPlayhead)  // if its a different clip, then set the playhead to it
				{
#define TIME_HELD_BEFORE_PICKUP_MS (200)
					if (fwTimer::GetSystemTimeInMilliseconds() > ms_heldDownTimer + TIME_HELD_BEFORE_PICKUP_MS)
					{
						if ( GetProject()->GetClipCount() > 1 || GetItemSelected() != PLAYHEAD_TYPE_VIDEO)
						{
							ms_bHaveProjectToRevert = true;
							
							ms_uProjectPositionToRevertMs = GetStartTimeOfClipMs(GetItemSelected(), c_projectIndex);

							CVideoEditorUi::PickupSelectedClip(c_projectIndex, true);

							ms_heldDownAndScrubbingTimeline = true;

							ms_mousePressedIndexFromActionscript = -1;
							ms_mouseReleasedIndexFromActionscript = -1;
						}
					}
				}
			}

			if (ms_mouseReleasedIndexFromActionscript >= 0)
			{
				u32 const c_projectIndex = GetProjectIndexFromTimelineIndex(ms_mouseReleasedIndexFromActionscript);

				if (c_projectIndex == c_currentProjectIndexAtPlayhead)  // if its a different clip, then set the playhead to it
				{
					if (fwTimer::GetSystemTimeInMilliseconds() < ms_heldDownTimer + TIME_HELD_BEFORE_PICKUP_MS)
					{
						if (!IsTimelineItemAudioOrAmbient(GetItemSelected()))  // cannot edit audio
						{
							if (GetItemSelected() != PLAYHEAD_TYPE_TEXT || GetProject()->GetClipCount() > 0) 
							{
								ms_iProjectPositionToRevertOnUpMs = ms_iProjectPositionToRevertOnDownMs = -1;
								CVideoEditorMenu::SetUserConfirmationScreenWithAction("TRIGGER_EDIT_CLIP", GetItemSelected() == PLAYHEAD_TYPE_TEXT ? "VE_EDIT_TEXT_SURE" : "VE_EDIT_CLIP_SURE");

								ms_mousePressedIndexFromActionscript = -1;
								ms_mouseReleasedIndexFromActionscript = -1;
							}
						}
					}
					else
					{
						ms_mousePressedIndexFromActionscript = -1;
						ms_mouseReleasedIndexFromActionscript = -1;
					}
				}
				else
				{
					bool highlightStartOfClip = true;
					u32 clipEndTimeMs = 0;

					if (IsTimelineItemNonVideo(GetItemSelected()))
					{
						// highlight the new clicked item - either from the start, or at the very end if clicked at the very end point of the clip (ready to trim)
						clipEndTimeMs = GetEndTimeOfClipMs(GetItemSelected(), c_projectIndex);

						if (IsMouseWithinTimelineArea())
						{
							if (c_mousePosMs > clipEndTimeMs - MOUSE_DEADZONE_AMOUNT_IN_MS)
							{
								highlightStartOfClip = false;
							}
						}
					}

					if (highlightStartOfClip)
					{
						SetPlayheadToStartOfProjectClip(GetItemSelected(), c_projectIndex, true);
					}
					else
					{
						SetPlayheadPositionMs(clipEndTimeMs, true);
					}

					ms_mousePressedIndexFromActionscript = -1;
					ms_mouseReleasedIndexFromActionscript = -1;
				}
			}
		}
		
		bool mouseReleasedThisFrame = false;
		//
		// check for button release:
		//
		if (s_mousePressedLastFrame && !c_mousePressedThisFrame)
		{
			u32 const c_playheadPosMs = GetPlayheadPositionMs();

			// we have released
			s_mousePressedLastFrame = false;
			mouseReleasedThisFrame = true;

			//
			// if we are scrubbing then drop the clip wherever we release the button
			//
			if (ms_heldDownAndScrubbingOverviewBar)
			{
				ms_heldDownAndScrubbingOverviewBar = false;

				SetPlayheadToStartOfNearestValidClipOfAnyType(c_playheadPosMs);
			}

			if (ms_heldDownAndScrubbingTimeline || IsPreviewing())
			{
				ms_droppedByMouse = ms_heldDownAndScrubbingTimeline;

				DropPreviewClipAtPosition(c_playheadPosMs, false);
				g_FrontendAudioEntity.PlaySound( "SELECT", "HUD_FRONTEND_DEFAULT_SOUNDSET" );
			}

			// reset all now as we have actioned them
			ms_mousePressedIndexFromActionscript = -1;
			ms_mouseReleasedIndexFromActionscript = -1;

			ms_heldDownAndScrubbingOverviewBar = ms_heldDownAndScrubbingTimeline = false;
			RepositionPlayheadIfNeeded(false);
		}

		s_mousePressedLastFrame = c_mousePressedThisFrame;  // store so we can check state change next frame

		ms_wantScrubbingSoundThisFrame |= ms_heldDownAndScrubbingOverviewBar || ms_heldDownAndScrubbingTimeline;

		if (ms_heldDownAndScrubbingOverviewBar)
		{
			CMousePointer::SetMouseCursorStyle(MOUSE_CURSOR_STYLE_HAND_GRAB);
			mousePointerChanged = true;

			const u32 c_totalTimelineTimeMs = GetTotalTimelineTimeMs(true);
			float const c_pixelsUnzoomed = ms_ComplexObject[TIMELINE_OBJECT_OVERVIEW_BACKGROUND].GetWidth();

			const float c_scaledPixels = c_totalTimelineTimeMs > 0 ? (c_pixelsUnzoomed / (MsToPixels(c_totalTimelineTimeMs)) * 100.0f) : c_pixelsUnzoomed;

			float const c_scaledHalf = (c_scaledPixels / ACTIONSCRIPT_STAGE_SIZE_X) * 0.5f;
			float const c_offsetPosition = TIMELINE_POSITION_X + c_scaledHalf;

			float const c_mousePosX = (float)ioMouse::GetX();
			float const c_mousePosY = (float)ioMouse::GetY();

			float mousePosXScaleform = c_mousePosX;
			float mousePosYScaleform = c_mousePosY;

			CScaleformMgr::TranslateWindowCoordinateToScaleformMovieCoordinate( 
				c_mousePosX, c_mousePosY, CVideoEditorUi::GetMovieId(), mousePosXScaleform, mousePosYScaleform );

			float const c_mousePos = (((mousePosXScaleform - (c_offsetPosition * ACTIONSCRIPT_STAGE_SIZE_X))) / c_pixelsUnzoomed);

			u32 const c_mousePosMs = (s32)rage::round(c_mousePos * c_totalTimelineTimeMs);

			if (c_mousePosMs >= 0 && c_mousePosMs <= c_totalTimelineTimeMs)
			{
				SetPlayheadPositionMs(c_mousePosMs, true);
			}
		}

		//
		// update position of the playhead if we are scrubbing:
		//
		if (ms_heldDownAndScrubbingTimeline || IsPreviewing())
		{
			static u32 s_oldMousePosMs = c_mousePosMs+1;
			u32 const c_projectTotalTimeMs = IsPreviewingVideo() ? (u32)GetProject()->GetTotalClipTimeMs() : GetTotalTimelineTimeMs();
			static u32 s_scrubbingMovementTimer = fwTimer::GetSystemTimeInMilliseconds();
			s32 const c_dir = GetMouseMovementDirInTimelineArea();
			s32 const c_timerLimit = ADJUSTMENT_TIMER_STANDARD;

			if (!ms_waitForMouseMovement || c_mousePosMs != s_oldMousePosMs)
			{
				ms_waitForMouseMovement = false;

				if (c_dir != 0)
				{
					if (fwTimer::GetSystemTimeInMilliseconds() > s_scrubbingMovementTimer + c_timerLimit)  // slower timer when previewing video
					{
						s32 newPlayheadPos = GetPlayheadPositionMs() + c_dir;

						if (newPlayheadPos < 0)
						{
							newPlayheadPos = 0;
						}
						
						if (newPlayheadPos > c_projectTotalTimeMs)
						{
							newPlayheadPos = c_projectTotalTimeMs;
						}

						SetPlayheadPositionMs(newPlayheadPos, true);

						s_scrubbingMovementTimer = fwTimer::GetSystemTimeInMilliseconds();
					}
				}
				else
				{
					s32 newPlayheadPos = c_mousePosMs;

					if (newPlayheadPos < 0)
					{
						newPlayheadPos = 0;
					}

					if (newPlayheadPos > c_projectTotalTimeMs)
					{
						newPlayheadPos = c_projectTotalTimeMs;
					}

					SetPlayheadPositionMs(newPlayheadPos, true);
				}
			}

			s_oldMousePosMs = c_mousePosMs;  // store the new mouse position for the following frame check
		}
	}

	return mousePointerChanged;
}
#endif // #if RSG_PC



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorTimeline::SetItemSelected
// PURPOSE:	sets the item selected on the timeline and deals with whether it can
//			be selected (if there are no clips of that type, it cannot be selected)
/////////////////////////////////////////////////////////////////////////////////////////
bool CVideoEditorTimeline::SetItemSelected(const ePLAYHEAD_TYPE newItemSelected)
{
	const s32 iCurrentItemSelected = (s32)ms_ItemSelected;

	ms_ItemSelected = newItemSelected;

	if ((s32)newItemSelected != iCurrentItemSelected)
	{
		if ((s32)newItemSelected > iCurrentItemSelected)
		{
			if (ms_ItemSelected == PLAYHEAD_TYPE_AMBIENT)
			{
				if (GetProject()->GetAmbientClipCount() == 0)
				{
					ms_ItemSelected = PLAYHEAD_TYPE_AUDIO;
				}
			}

			if (ms_ItemSelected == PLAYHEAD_TYPE_AUDIO)
			{
				if (GetProject()->GetMusicClipCount() == 0)
				{
					ms_ItemSelected = PLAYHEAD_TYPE_TEXT;
				}
			}

			if (ms_ItemSelected == PLAYHEAD_TYPE_TEXT)
			{
				if (GetProject()->GetOverlayTextCount() == 0)
				{
					ms_ItemSelected = (ePLAYHEAD_TYPE)iCurrentItemSelected;  // we didnt find anything below, so return back to original value
				}
			}
		}
		else
		{
			if (ms_ItemSelected == PLAYHEAD_TYPE_AUDIO)
			{
				if (GetProject()->GetMusicClipCount() == 0)
				{
					ms_ItemSelected = PLAYHEAD_TYPE_AMBIENT;
				}
			}

			if (ms_ItemSelected == PLAYHEAD_TYPE_AMBIENT)
			{
				if (GetProject()->GetAmbientClipCount() == 0)
				{
					ms_ItemSelected = PLAYHEAD_TYPE_VIDEO;
				}
			}

			if (ms_ItemSelected == PLAYHEAD_TYPE_VIDEO)
			{
				if (GetProject()->GetClipCount() == 0)
				{
					ms_ItemSelected = (ePLAYHEAD_TYPE)iCurrentItemSelected;  // we didnt find anything above, so return back to original value
				}
			}
		}
	}

	u32 const c_playheadPos = GetPlayheadPositionMs();
	s32 const c_projIndex = GetProjectIndexNearestStartPos(ms_ItemSelected, c_playheadPos);

	if (c_projIndex != -1)
	{
		SetPlayheadToStartOfProjectClip(ms_ItemSelected, c_projIndex, true);
	}

	if (ms_ItemSelected != iCurrentItemSelected)
	{
		if (ms_ComplexObject[TIMELINE_OBJECT_PLAYHEAD_TRIMMING_ICON].IsActive())
		{
			ms_ComplexObject[TIMELINE_OBJECT_PLAYHEAD_TRIMMING_ICON].SetXPositionInPixels(0);
			ms_ComplexObject[TIMELINE_OBJECT_PLAYHEAD_TRIMMING_ICON].SetAlpha(0);
		}

		RefreshStagingArea();
		CVideoEditorInstructionalButtons::Refresh();
		
		return true;
	}
	else
	{
		return false;
	}
}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorTimeline::SetupStagePosition
// PURPOSE:	returns the title position of the stage area (large thumbnail)
/////////////////////////////////////////////////////////////////////////////////////////
Vector2 CVideoEditorTimeline::SetupStagePosition()
{
	Vector2 vReturnPos(0.0f, 0.0f);

	if (ms_ComplexObject[TIMELINE_OBJECT_TIMELINE_PANEL].IsActive())
	{
		vReturnPos = ms_ComplexObject[TIMELINE_OBJECT_TIMELINE_PANEL].GetPosition();

		if (ms_ComplexObject[TIMELINE_OBJECT_STAGE].IsActive())
		{
			vReturnPos += Vector2(ms_ComplexObject[TIMELINE_OBJECT_STAGE].GetPosition());
			// Vector2 const c_stageSize = Vector2(ms_ComplexObject[TIMELINE_OBJECT_STAGE].GetSize());

			float const c_aspectRatio = CHudTools::GetAspectRatio();
			float const c_SIXTEEN_BY_NINE = 16.0f/9.0f;
			float const c_difference = (c_SIXTEEN_BY_NINE / c_aspectRatio);
			float const c_finalX = 0.5f - ((0.5f - vReturnPos.x) * c_difference);

			vReturnPos.x = c_finalX;
		}
	}

	return Vector2(vReturnPos.x, vReturnPos.y);
}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorTimeline::SetupStageSize
// PURPOSE:	returns the size of the stage area (large thumbnail)
/////////////////////////////////////////////////////////////////////////////////////////
Vector2 CVideoEditorTimeline::SetupStageSize()
{
	Vector2 vReturnPos(0.0f, 0.0f);

	if (ms_ComplexObject[TIMELINE_OBJECT_STAGE].IsActive())
	{
		vReturnPos = Vector2(ms_ComplexObject[TIMELINE_OBJECT_STAGE].GetSize());
		
		float const c_aspectRatio = CHudTools::GetAspectRatio();
		float const c_SIXTEEN_BY_NINE = 16.0f/9.0f;
		float const c_difference = (c_SIXTEEN_BY_NINE / c_aspectRatio);

		if(!CHudTools::IsSuperWideScreen())
			vReturnPos.x *= c_difference;
	}

	return vReturnPos;
}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorTimeline::GetProjectIndexAtTime
// PURPOSE:	returns the project index that is exactly at the passed time
/////////////////////////////////////////////////////////////////////////////////////////
s32 CVideoEditorTimeline::GetProjectIndexAtTime(ePLAYHEAD_TYPE const c_type, u32 const c_timeMs)
{
	return GetProjectIndexBetweenTime(c_type, c_timeMs, c_timeMs);
}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorTimeline::GetProjectIndexBetweenTime
// PURPOSE:	returns the project index that is within the passed time
/////////////////////////////////////////////////////////////////////////////////////////
s32 CVideoEditorTimeline::GetProjectIndexBetweenTime(ePLAYHEAD_TYPE const c_type, u32 const c_startTimeMs, u32 const c_endTimeMs)
{
	if (CVideoEditorUi::IsProjectPopulated())
	{
		if (IsTimelineItemStandard(c_type))
		{
			s32 const c_maxCount = GetMaxClipCount(c_type);

			for (s32 index = 0; index < c_maxCount; index++)
			{
				u32 const c_clipStartTimeMs = GetStartTimeOfClipMs(c_type, index);
				u32 const c_clipEndTimeMs = GetEndTimeOfClipMs(c_type, index);

				s32 const c_additionalRange = c_type == PLAYHEAD_TYPE_VIDEO && (index < c_maxCount - 1) ? 0 : 1;  // need to take into account last pos of clip if we can move to it (last video clip or any audio/text clip)

				if (c_startTimeMs >= c_clipStartTimeMs && c_startTimeMs < c_clipEndTimeMs + c_additionalRange)
				{
					return index;
				}

				if (c_endTimeMs >= c_clipStartTimeMs && c_endTimeMs < c_clipEndTimeMs + c_additionalRange)
				{
					return index;
				}

				if (c_startTimeMs >= c_clipStartTimeMs && c_endTimeMs < c_clipEndTimeMs + c_additionalRange)
				{
					return index;
				}

				if (c_clipStartTimeMs >= c_startTimeMs && c_clipEndTimeMs + c_additionalRange < c_endTimeMs)
				{
					return index;
				}
			}
		}
	}

	return -1;
}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorTimeline::GetProjectIndexFromTimelineIndex
// PURPOSE:	
/////////////////////////////////////////////////////////////////////////////////////////
u32 CVideoEditorTimeline::GetProjectIndexFromTimelineIndex(u32 const c_timelineIndex)
{
	if (uiVerifyf(c_timelineIndex < ms_ComplexObjectClips.GetCount(), "CVideoEditorTimeline::GetProjectIndexFromTimelineIndex() couldnt find timeline index %u in the project", c_timelineIndex))
	{
		return (ms_ComplexObjectClips[c_timelineIndex].uIndexToProject);
	}

	return 0;
}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorTimeline::GetTimelineIndexFromProjectIndex
// PURPOSE:	
/////////////////////////////////////////////////////////////////////////////////////////
s32 CVideoEditorTimeline::GetTimelineIndexFromProjectIndex(ePLAYHEAD_TYPE const c_type, u32 const c_projectIndex)
{
	for (s32 timelineIndexCount = 0; timelineIndexCount < ms_ComplexObjectClips.GetCount(); timelineIndexCount++)
	{
		if (ms_ComplexObjectClips[timelineIndexCount].type == c_type)
		{
			if (ms_ComplexObjectClips[timelineIndexCount].uIndexToProject == c_projectIndex)
			{
				return timelineIndexCount;
			}
		}
	}
	
	return -1;
}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorTimeline::GetProjectIndexHoveredOverNearestStartPos
// PURPOSE:	returns the clip where the playhead is nearest to the start of, either the
//			current clip or the next clip.
/////////////////////////////////////////////////////////////////////////////////////////
s32 CVideoEditorTimeline::GetProjectIndexNearestStartPos(ePLAYHEAD_TYPE const c_type, u32 const c_timeMs, s32 const c_dir, s32 const c_indexToIgnore, bool const c_checkHalfwayPosition)
{
	if (!CVideoEditorUi::IsProjectPopulated())
		return -1;

	if (IsTimelineItemStandard(c_type))
	{
		s32 const c_maxCount = GetMaxClipCount(c_type);

		u32 const c_totalClipTimeMs = (u32)GetProject()->GetTotalClipTimeMs();
		u32 biggestDifferenceFound = GetTotalTimelineTimeMs();
		s32 nearestItemIndex = -1;// c_dir > 0 ? c_maxCount - 1 : 0;

		for (s32 i = 0; i < c_maxCount; i++)
		{
			if (i == c_indexToIgnore)
				continue;

			u32 const c_clipStart = GetStartTimeOfClipMs(c_type, i);
			u32 const c_clipEnd = GetEndTimeOfClipMs(c_type, i) - 1;
			bool const c_atOrOverEndOfProjectSpace = c_type == PLAYHEAD_TYPE_VIDEO && c_timeMs >= c_totalClipTimeMs;

 			if (c_timeMs >= c_clipStart && (c_timeMs <= c_clipEnd || c_atOrOverEndOfProjectSpace))
 			{
 				nearestItemIndex = c_atOrOverEndOfProjectSpace ? c_maxCount - 1 : i;

				break;
 			}

			if ( (c_dir == 0) || (c_dir > 0 && c_clipEnd >= c_timeMs) || (c_dir < 0 && c_clipStart <= c_timeMs)  )
			{
				u32 const c_diffRight = c_dir > 0 ? c_clipEnd - c_timeMs : 0;
				u32 const c_diffLeft = c_dir <= 0 ? c_clipStart > c_timeMs ? c_clipStart - c_timeMs : c_timeMs - c_clipStart : 0;

				if ( c_dir > 0 && (c_diffRight < biggestDifferenceFound || (nearestItemIndex == -1 && c_diffRight == biggestDifferenceFound)) )
				{
					biggestDifferenceFound = c_diffRight;
					nearestItemIndex = i;
				}

				if ( c_dir <= 0 && c_diffLeft > 0 && (c_diffLeft < biggestDifferenceFound || (nearestItemIndex == -1 && c_diffLeft == biggestDifferenceFound)) )
				{
					biggestDifferenceFound = c_diffLeft;
					nearestItemIndex = i;
				}
			}
		}

		if (IsPreviewingVideo() && c_checkHalfwayPosition)
		{
			u32 const c_clipStart = GetStartTimeOfClipMs(c_type, nearestItemIndex);
			u32 const c_clipEnd = GetEndTimeOfClipMs(c_type, nearestItemIndex) - 1;

			u32 const c_clipMidPoint = c_clipStart + (u32)((c_clipEnd - c_clipStart) * 0.5f);

			if (c_timeMs >= c_clipMidPoint)  // if over mid point, then use the next index along
			{
				nearestItemIndex ++;
			}
		}

		return nearestItemIndex;  // return the index of the nearest
	}

	uiAssertf(0, "CVideoEditorTimeline::GetProjectIndexNearestStartPos() - Didnt have valid ms_ItemSelected (%d)", (s32)c_type);

	return -1;
}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorTimeline::SetPlayheadToStartOfNearestValidClipOfAnyType
// PURPOSE:	sets the playhead to the start of any clip that  is nearest the passed pos
/////////////////////////////////////////////////////////////////////////////////////////
void CVideoEditorTimeline::SetPlayheadToStartOfNearestValidClipOfAnyType(u32 const c_positionMs)
{
	u32 const c_totalTimelineTime =  GetTotalTimelineTimeMs();
	s32 const c_nearestVideoProjectIndex = GetProjectIndexNearestStartPos(PLAYHEAD_TYPE_VIDEO, c_positionMs);
	s32 const c_nearestAmbientProjectIndex = GetProjectIndexNearestStartPos(PLAYHEAD_TYPE_AMBIENT, c_positionMs);
	s32 const c_nearestAudioProjectIndex = GetProjectIndexNearestStartPos(PLAYHEAD_TYPE_AUDIO, c_positionMs);
	s32 const c_nearestTextProjectIndex = GetProjectIndexNearestStartPos(PLAYHEAD_TYPE_TEXT, c_positionMs);

	s32 const c_nearestVideoPos = c_nearestVideoProjectIndex != -1 ? (u32)GetProject()->GetTrimmedTimeToClipMs(c_nearestVideoProjectIndex) : 0;
	s32 const c_nearestAmbientPos = c_nearestAmbientProjectIndex != -1 ? GetProject()->GetAmbientClip(c_nearestAmbientProjectIndex).getStartTimeWithOffsetMs() : 0;
	s32 const c_nearestAudioPos = c_nearestAudioProjectIndex != -1 ? GetProject()->GetMusicClip(c_nearestAudioProjectIndex).getStartTimeWithOffsetMs() : 0;
	s32 const c_nearestTextPos = c_nearestTextProjectIndex != -1 ? GetProject()->GetOverlayText(c_nearestTextProjectIndex).getStartTimeMs() : 0;

	s32 const c_diffVideo = c_nearestVideoProjectIndex != -1 ? (c_positionMs > c_nearestVideoPos ? c_positionMs-c_nearestVideoPos : c_nearestVideoPos-c_positionMs) : c_totalTimelineTime;
	s32 const c_diffAmbient = c_nearestAmbientProjectIndex != -1 ? (c_positionMs > c_nearestAmbientPos ? c_positionMs-c_nearestAmbientPos : c_nearestAmbientPos-c_positionMs) : c_totalTimelineTime;
	s32 const c_diffAudio = c_nearestAudioProjectIndex != -1 ? (c_positionMs > c_nearestAudioPos ? c_positionMs-c_nearestAudioPos : c_nearestAudioPos-c_positionMs) : c_totalTimelineTime;
	s32 const c_diffText = c_nearestTextProjectIndex != -1 ? (c_positionMs > c_nearestTextPos ? c_positionMs-c_nearestTextPos : c_nearestTextPos-c_positionMs) : c_totalTimelineTime;

	if (c_diffVideo < c_diffAudio && c_diffVideo < c_diffAmbient && c_diffVideo < c_diffText)  // set to nearest video
	{
		SetItemSelected(PLAYHEAD_TYPE_VIDEO);
		UpdatePlayheadPreview(PLAYHEAD_TYPE_NONE);
		SetPlayheadToStartOfProjectClip(PLAYHEAD_TYPE_VIDEO, c_nearestVideoProjectIndex, true);
		return;
	}

	if (c_diffAmbient < c_diffVideo && c_diffAmbient < c_diffAudio && c_diffAmbient < c_diffText)  // set to nearest ambient
	{
		SetItemSelected(PLAYHEAD_TYPE_AMBIENT);
		UpdatePlayheadPreview(PLAYHEAD_TYPE_NONE);
		SetPlayheadToStartOfProjectClip(PLAYHEAD_TYPE_AMBIENT, c_nearestAmbientProjectIndex, true);
		return;
	}

	if (c_diffAudio < c_diffVideo && c_diffAudio < c_diffAmbient && c_diffAudio < c_diffText)  // set to nearest audio
	{
		SetItemSelected(PLAYHEAD_TYPE_AUDIO);
		UpdatePlayheadPreview(PLAYHEAD_TYPE_NONE);
		SetPlayheadToStartOfProjectClip(PLAYHEAD_TYPE_AUDIO, c_nearestAudioProjectIndex, true);
		return;
	}

	if (c_diffText < c_diffVideo && c_diffText < c_diffAudio && c_diffText < c_diffAmbient)  // set to nearest text
	{
		SetItemSelected(PLAYHEAD_TYPE_TEXT);
		UpdatePlayheadPreview(PLAYHEAD_TYPE_NONE);
		SetPlayheadToStartOfProjectClip(PLAYHEAD_TYPE_TEXT, c_nearestTextProjectIndex, true);
		return;
	}

	SetPlayheadToStartOfNearestValidClip();  // set to nearest of current type
}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorTimeline::SetPlayheadToStartOfNearestValidClip
// PURPOSE:	goes to the start of the nearest clip that is still valid
/////////////////////////////////////////////////////////////////////////////////////////
void CVideoEditorTimeline::SetPlayheadToStartOfNearestValidClip()
{
	ePLAYHEAD_TYPE const c_currentPlayheadType = GetItemSelected();

	ePLAYHEAD_TYPE type = c_currentPlayheadType;

	if (CVideoEditorUi::IsProjectPopulated())
	{
		if (c_currentPlayheadType == PLAYHEAD_TYPE_VIDEO)
		{
			if (GetProject()->GetClipCount() > 0)
			{
				type = PLAYHEAD_TYPE_VIDEO;
			}
			else if (GetProject()->GetAmbientClipCount() > 0)
			{
				type = PLAYHEAD_TYPE_AMBIENT;
			}
			else if (GetProject()->GetMusicClipCount() > 0)
			{
				type = PLAYHEAD_TYPE_AUDIO;
			}
			else
			{
				type = PLAYHEAD_TYPE_TEXT;
			}
		}
		else if (c_currentPlayheadType == PLAYHEAD_TYPE_AMBIENT)
		{
			if (GetProject()->GetAmbientClipCount() > 0)
			{
				type = PLAYHEAD_TYPE_AMBIENT;
			}
			else if (GetProject()->GetClipCount() > 0)
			{
				type = PLAYHEAD_TYPE_VIDEO;
			}
			else if (GetProject()->GetMusicClipCount() > 0)
			{
				type = PLAYHEAD_TYPE_AUDIO;
			}
			else
			{
				type = PLAYHEAD_TYPE_TEXT;
			}
		}
		else if (c_currentPlayheadType == PLAYHEAD_TYPE_AUDIO)
		{
			if (GetProject()->GetMusicClipCount() > 0)
			{
				type = PLAYHEAD_TYPE_AUDIO;
			}
			else if (GetProject()->GetAmbientClipCount() > 0)
			{
				type = PLAYHEAD_TYPE_AMBIENT;
			}
			else if (GetProject()->GetClipCount() > 0)
			{
				type = PLAYHEAD_TYPE_VIDEO;
			}
			else
			{
				type = PLAYHEAD_TYPE_TEXT;
			}
		}
		else
		{
			if (GetProject()->GetOverlayTextCount() > 0)
			{
				type = PLAYHEAD_TYPE_TEXT;
			}
			else if (GetProject()->GetClipCount() > 0)
			{
				type = PLAYHEAD_TYPE_VIDEO;
			}
			else if (GetProject()->GetAmbientClipCount() > 0)
			{
				type = PLAYHEAD_TYPE_AMBIENT;
			}
			else
			{
				type = PLAYHEAD_TYPE_AUDIO;
			}
		}
	}

	// ensure we highlight the start of the nearest video clip:
	s32 const c_nearestProjectIndex = GetProjectIndexNearestStartPos(type, GetPlayheadPositionMs());
	if (c_nearestProjectIndex != -1)
	{
		SetItemSelected(type);
		UpdatePlayheadPreview(PLAYHEAD_TYPE_NONE);
		SetPlayheadToStartOfProjectClip(type, (u32)c_nearestProjectIndex, true);
	}
}

/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorTimeline::GetNumberedIndexOfClipTypeOnTimeline
// PURPOSE:	returns the the nth clip on the timeline of the type passed
/////////////////////////////////////////////////////////////////////////////////////////
u32 CVideoEditorTimeline::GetNumberedIndexOfClipTypeOnTimeline(const u32 uNumber, const ePLAYHEAD_TYPE findType)
{
	u32 uCount = 0;

	for (u32 i = 0; i < ms_ComplexObjectClips.GetCount(); i++)
	{
		if (ms_ComplexObjectClips[i].object.IsActive())
		{
			if (ms_ComplexObjectClips[i].type == findType)
			{
				if (uCount == uNumber)
				{
					return i;
				}

				uCount++;  // increment the counter
			}
		}
	}

	uiAssertf(0, "CVideoEditorTimeline::GetNumberedIndexOfClipTypeOnTimeline() couldnt number %u of clip type %d", uNumber, (s32)findType);	

	return 0;
}





/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorTimeline::GetSelectedClipObject
// PURPOSE:	returns the actual video clip object we are selected on
/////////////////////////////////////////////////////////////////////////////////////////
u32 CVideoEditorTimeline::GetSelectedClipObject(const u32 c_projectIndex, const ePLAYHEAD_TYPE type)
{
	for (u32 i = 0; i < ms_ComplexObjectClips.GetCount(); i++)
	{
		if (ms_ComplexObjectClips[i].object.IsActive())
		{
			if (ms_ComplexObjectClips[i].type == type)
			{
				if (ms_ComplexObjectClips[i].uIndexToProject == c_projectIndex)
				{
					return i;
				}
			}
		}
	}

	uiAssertf(0, "CVideoEditorTimeline::GetSelectedClipObject() Cannot find project index %u in timeline array", c_projectIndex);

	return 0;
}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorTimeline::SetTimelineSelected
// PURPOSE:	sets that the timeline is active or not
/////////////////////////////////////////////////////////////////////////////////////////
void CVideoEditorTimeline::SetTimelineSelected(const bool bActive)
{
	if (bActive)
	{
		// if setting timeline to visible and we have no clips, clear anything on the stage and display 'add clip' message
		if (GetProject() && GetProject()->GetClipCount() == 0)
		{
			ClearAllStageClips(false);
		}
	}
	else
	{
		SetCurrentTextOverlayIndex(-1);
	}

	SetTimelineHighlighting(bActive);

	if (ms_ComplexObject[TIMELINE_OBJECT_TIMELINE_PANEL].IsActive())
	{
		ms_ComplexObject[TIMELINE_OBJECT_TIMELINE_PANEL].SetVisible(bActive);
	}

	SetTimeOnPlayhead(bActive);

	if (bActive)
	{
		UpdateProjectMaxIndicator();
		UpdateProjectSpaceIndicator();
		UpdateOverviewBar();
	}

	if (g_RadioAudioEntity.IsReplayMusicPreviewPlaying())
	{
		g_RadioAudioEntity.StopReplayTrackPreview();
	}
	
#if RSG_PC
	CMousePointer::SetMouseCursorStyle(MOUSE_CURSOR_STYLE_ARROW);  // reset the mouse cursor here to the standard arrow
#endif // #if RSG_PC

	ms_bSelected = bActive;

	CTextTemplate::UpdateTemplateWindow();
}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorTimeline::ShowPreviewStage
// PURPOSE:	shows just the preview stage area (no full timeline)
/////////////////////////////////////////////////////////////////////////////////////////
void CVideoEditorTimeline::ShowPreviewStage(const bool bActive)
{
	if (ms_ComplexObject[TIMELINE_OBJECT_TIMELINE_PANEL].IsActive())
	{
		ms_ComplexObject[TIMELINE_OBJECT_TIMELINE_PANEL].SetVisible(bActive);
	}
}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorTimeline::SetTimelineHighlighting
// PURPOSE:	highlights the timeline to be on or off
/////////////////////////////////////////////////////////////////////////////////////////
void CVideoEditorTimeline::SetTimelineHighlighting(const bool bActive)
{
	if (GetItemSelected() == PLAYHEAD_TYPE_VIDEO)
	{
		if (ms_ComplexObjectClips.GetCount() > 0)
		{
			const s32 iProjectIndexAtPlayhead = GetProjectIndexAtTime(PLAYHEAD_TYPE_VIDEO, GetPlayheadPositionMs());

			if (iProjectIndexAtPlayhead != -1)
			{
				HighlightClip(GetTimelineIndexFromProjectIndex(PLAYHEAD_TYPE_VIDEO, iProjectIndexAtPlayhead), bActive);
			}
		}
	}
}



bool CVideoEditorTimeline::IsScrollingFast()
{
	if (ms_heldDownAndScrubbingOverviewBar)  // we are scrubbing = no requests
	{
		return true;
	}

	if (ms_stickTimerAcc == TIMELINE_MIN_INPUT_ACC)  // fastest stick acceleration = no requests
	{
		return true;
	}

	return false;
}


bool CVideoEditorTimeline::IsScrollingMediumFast()
{
	if (ms_heldDownAndScrubbingOverviewBar)  // we are scrubbing = no requests
	{
		return true;
	}

	if (ms_stickTimerAcc < TIMELINE_INPUT_ACC_NO_TEXTURES)  // fastest stick acceleration = no requests
	{
		return true;
	}

	return false;
}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorTimeline::SetPlayheadToStartOfProjectClip
// PURPOSE:	sets the playhead to the start of the passed clip index
/////////////////////////////////////////////////////////////////////////////////////////
void CVideoEditorTimeline::SetPlayheadToStartOfProjectClip(ePLAYHEAD_TYPE const c_type, u32 const c_projectIndex, bool const c_readjustTimeline)
{
	s32 const c_newTimeMs = GetStartTimeOfClipMs(c_type, c_projectIndex);

	if (c_newTimeMs >= 0)
	{
		SetPlayheadPositionMs(c_newTimeMs, c_readjustTimeline);
	}
}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorTimeline::GetPlayheadPositionMs
// PURPOSE:	returns the playhead position in the project timespace in milliseconds
/////////////////////////////////////////////////////////////////////////////////////////
u32 CVideoEditorTimeline::GetPlayheadPositionMs()
{
	if (ms_ComplexObject[TIMELINE_OBJECT_PLAYHEAD_MARKER].IsActive())
	{
		s32 const c_TimelineContainerPosMs = -PixelsToMs(ms_ComplexObject[TIMELINE_OBJECT_TIMELINE_CONTAINER].GetXPositionInPixels());
		s32 const c_PlayheadMarkerPosMs = PixelsToMs(ms_ComplexObject[TIMELINE_OBJECT_PLAYHEAD_MARKER].GetXPositionInPixels());

		s32 const c_PlayheadPosMs = c_TimelineContainerPosMs + c_PlayheadMarkerPosMs;

		if (c_PlayheadPosMs > 0)
		{
			return c_PlayheadPosMs;
		}
	}

	return 0;
}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorTimeline::GetMousePositionMs
// PURPOSE:	returns the position in ms of the mouse position, valid on consoles but
//			returns 0
/////////////////////////////////////////////////////////////////////////////////////////
u32 CVideoEditorTimeline::GetMousePositionMs()
{
#if RSG_PC
	float const c_mousePosX = (float)ioMouse::GetX();
	float const c_mousePosY = (float)ioMouse::GetY();

	float mousePosXScaleform = c_mousePosX;
	float mousePosYScaleform = c_mousePosY;

	CScaleformMgr::TranslateWindowCoordinateToScaleformMovieCoordinate( 
		c_mousePosX, c_mousePosY, CVideoEditorUi::GetMovieId(), mousePosXScaleform, mousePosYScaleform );

	float const c_currentContainerPos = ms_ComplexObject[TIMELINE_OBJECT_TIMELINE_CONTAINER].GetXPositionInPixels();

	float const c_mousePos = ((mousePosXScaleform - (TIMELINE_POSITION_X*ACTIONSCRIPT_STAGE_SIZE_X))) - c_currentContainerPos;
	u32 const c_mousePosMs = c_mousePos > 0.0f ? (u32)PixelsToMs(c_mousePos) : 0;

	return c_mousePosMs;
#else
	return 0;
#endif
}


/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorTimeline::HighlightClip
// PURPOSE:	highlights or unhighlights the passed clip
/////////////////////////////////////////////////////////////////////////////////////////
void CVideoEditorTimeline::HighlightClip(u32 const c_timelineIndex, bool highlight)
{
	if (c_timelineIndex == MAX_UINT32)
	{
		return;
	}

	if (uiVerifyf(c_timelineIndex < ms_ComplexObjectClips.GetCount(), "timeline index %u is not within timeline range (%u)", c_timelineIndex, ms_ComplexObjectClips.GetCount()))
	{
		ePLAYHEAD_TYPE const c_playheadType = ms_ComplexObjectClips[c_timelineIndex].type;

		if (IsTimelineItemStandard(c_playheadType))
		{
			if (IsPreviewing() || ms_previewingAudioTrack != -1)  // no highlighting when we are previewing
			{
				highlight = false;
			}

			const u32 c_positionOfClipMs = PixelsToMs(ms_ComplexObjectClips[c_timelineIndex].object.GetXPositionInPixels());

			CComplexObject backgroundObject = ms_ComplexObjectClips[c_timelineIndex].object.GetObject("BACKGROUND");

			if (backgroundObject.IsActive())
			{
				if (highlight)
				{
					if (backgroundObject.IsActive())
					{
						backgroundObject.SetAlpha(ALPHA_100_PERCENT);
					}
				}
				else
				{
					if (backgroundObject.IsActive())
					{
						backgroundObject.SetAlpha(ALPHA_30_PERCENT);
					}
				}

				if (c_playheadType != PLAYHEAD_TYPE_VIDEO)
				{
					CComplexObject textFadeOutObject = ms_ComplexObjectClips[c_timelineIndex].object.GetObject("TEXT_FADE_OUT");
					if (textFadeOutObject.IsActive())
					{
						Color32 colorToPass;
						bool const c_isScoreTrack = IsPlayheadAudioScore(c_positionOfClipMs, c_playheadType);

						if (highlight)
						{
							if (c_playheadType == PLAYHEAD_TYPE_AMBIENT)
							{
								colorToPass = CHudColour::GetRGB(HUD_COLOUR_VIDEO_EDITOR_AMBIENT, ALPHA_100_PERCENT);
							}
							else if (c_playheadType == PLAYHEAD_TYPE_AUDIO)
							{
								colorToPass = CHudColour::GetRGB(c_isScoreTrack ? HUD_COLOUR_VIDEO_EDITOR_SCORE : HUD_COLOUR_VIDEO_EDITOR_AUDIO, ALPHA_100_PERCENT);
							}
							else
							{
								colorToPass = CHudColour::GetRGB(HUD_COLOUR_VIDEO_EDITOR_TEXT, ALPHA_100_PERCENT);
							}
						}
						else
						{
							if (c_playheadType == PLAYHEAD_TYPE_AMBIENT)
							{
								colorToPass = CHudColour::GetRGB(HUD_COLOUR_VIDEO_EDITOR_AMBIENT_FADEOUT, ALPHA_100_PERCENT);
							}
							else if (c_playheadType == PLAYHEAD_TYPE_AUDIO)
							{
								colorToPass = CHudColour::GetRGB(c_isScoreTrack ? HUD_COLOUR_VIDEO_EDITOR_SCORE_FADEOUT : HUD_COLOUR_VIDEO_EDITOR_AUDIO_FADEOUT, ALPHA_100_PERCENT);
							}
							else
							{
								colorToPass = CHudColour::GetRGB(HUD_COLOUR_VIDEO_EDITOR_TEXT_FADEOUT, ALPHA_100_PERCENT);
							}
						}

						textFadeOutObject.SetColour(colorToPass);
						textFadeOutObject.Release();
					}
				}

				backgroundObject.Release();
			}

			if (highlight)
			{
				if (ms_uPreviousTimelineIndexHovered != MAX_UINT32 && ms_uPreviousTimelineIndexHovered != c_timelineIndex && ms_uPreviousTimelineIndexHovered < ms_ComplexObjectClips.GetCount())  // ensure its still a valid index
				{
					HighlightClip(ms_uPreviousTimelineIndexHovered, false);
				}

				ms_uPreviousTimelineIndexHovered = c_timelineIndex;  // store the latest highlighted item

				UpdateProjectSpaceIndicator();
				UpdateOverviewBar();

				CVideoEditorInstructionalButtons::Refresh();
			}
		}
	}
}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorTimeline::AdjustPlayheadFromAxis
// PURPOSE: takes an axis value from the pad and after applying a multiplier, movies it
//			by the amount of pixels
/////////////////////////////////////////////////////////////////////////////////////////
void CVideoEditorTimeline::AdjustPlayheadFromAxis(const float fAxis)
{
#define ADJUST_MULTIPLIER (8.0f)

	AdjustPlayhead(fAxis * ADJUST_MULTIPLIER, true);  // move the playhead by the passed value
}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorTimeline::RepositionPlayheadIfNeeded
// PURPOSE: ensure the playhead and also timeline positioning are positioned correctly
//			for the number of clips shown and amount scrolled
/////////////////////////////////////////////////////////////////////////////////////////
void CVideoEditorTimeline::RepositionPlayheadIfNeeded(bool const c_clipIsBeingDeleted)
{
	if (!GetProject())
	{
		SetTimeOnPlayhead(false);

		return;
	}

	if (!CVideoEditorUi::IsProjectPopulated())  // do not move anything if we have no clips
	{
		// if 0 clips, then ensure playhead marker is reset to start
		ms_ComplexObject[TIMELINE_OBJECT_TIMELINE_CONTAINER].SetXPositionInPixels(0.0f);
		ms_ComplexObject[TIMELINE_OBJECT_PLAYHEAD_MARKER].SetXPositionInPixels(0.0f);

		ClearAllStageClips(true);
		RefreshStagingArea();

		return;
	}
	
	if (ms_ComplexObject[TIMELINE_OBJECT_TIMELINE_CONTAINER].IsActive() && ms_ComplexObject[TIMELINE_OBJECT_TIMELINE_BACKGROUND].IsActive() && ms_ComplexObject[TIMELINE_OBJECT_PLAYHEAD_MARKER].IsActive())
	{
		u32 const c_endOfVisibleProjectOnTimelineMs = GetTotalTimelineTimeMs();
		
		u32 const c_lastClipEndPosFromTimelinePanelMs = c_endOfVisibleProjectOnTimelineMs;
		float const c_lastClipEndPosFromTimelinePanel = MsToPixels(c_lastClipEndPosFromTimelinePanelMs);

		float const c_timelineWidth = ms_ComplexObject[TIMELINE_OBJECT_TIMELINE_BACKGROUND].GetWidth();

		if (ms_ComplexObject[TIMELINE_OBJECT_TIMELINE_CONTAINER].GetXPositionInPixels() < 0.0f && c_lastClipEndPosFromTimelinePanel-(abs(ms_ComplexObject[TIMELINE_OBJECT_TIMELINE_CONTAINER].GetXPositionInPixels())) < c_timelineWidth)  // if the last clip is less than the width of the timeline (ie gap at the right hand side), shift everything along
		{
			const float fDiff = c_lastClipEndPosFromTimelinePanel-(abs(ms_ComplexObject[TIMELINE_OBJECT_TIMELINE_CONTAINER].GetXPositionInPixels())) - c_timelineWidth;  // this is the amount we need to shift the container back along by
			ms_ComplexObject[TIMELINE_OBJECT_TIMELINE_CONTAINER].AdjustXPositionInPixels(-fDiff);  // adjust container by the diff
		}

		if (ms_ComplexObject[TIMELINE_OBJECT_TIMELINE_CONTAINER].GetXPositionInPixels() > 0.0f || c_lastClipEndPosFromTimelinePanel < c_timelineWidth)  // if we are scrolled along to the left but the amount of clips now fit inside the timeline window fully
		{
			ms_ComplexObject[TIMELINE_OBJECT_TIMELINE_CONTAINER].SetXPositionInPixels(0.0f);  // reset it back to position 0
		}

		if (GetPlayheadPositionMs() > c_endOfVisibleProjectOnTimelineMs)  // adjust the playhead marker to the last clip if we have moved
		{
			u32 const c_diffMs = GetPlayheadPositionMs() - c_endOfVisibleProjectOnTimelineMs;
			AdjustPlayhead(-MsToPixels(c_diffMs), false);
		}

		if (ms_ComplexObject[TIMELINE_OBJECT_PLAYHEAD_MARKER].GetXPositionInPixels() < 0.0f)
		{
			ms_ComplexObject[TIMELINE_OBJECT_PLAYHEAD_MARKER].SetXPositionInPixels(0.0f);
		}

		if (ms_ComplexObject[TIMELINE_OBJECT_PLAYHEAD_MARKER].GetXPositionInPixels() > c_timelineWidth)
		{
			ms_ComplexObject[TIMELINE_OBJECT_PLAYHEAD_MARKER].SetXPositionInPixels(c_timelineWidth);
		}
	}

	if (!IsPreviewing())
	{
		// if repositioning with text or audio and text no longer exists, set it back to the clip above, up to video
		if (GetItemSelected() == PLAYHEAD_TYPE_TEXT)
		{
			if (GetProject()->GetOverlayTextCount() == 0)
			{
				SetItemSelected(PLAYHEAD_TYPE_AUDIO);
				UpdatePlayheadPreview(PLAYHEAD_TYPE_NONE);
			}
		}

		if (GetItemSelected() == PLAYHEAD_TYPE_AUDIO)
		{
			if (GetProject()->GetMusicClipCount() == 0)
			{
				SetItemSelected(PLAYHEAD_TYPE_AMBIENT);
				UpdatePlayheadPreview(PLAYHEAD_TYPE_NONE);
			}
		}

		if (GetItemSelected() == PLAYHEAD_TYPE_AMBIENT)
		{
			if (GetProject()->GetAmbientClipCount() == 0)
			{
				SetItemSelected(PLAYHEAD_TYPE_VIDEO);
				UpdatePlayheadPreview(PLAYHEAD_TYPE_NONE);
			}
		}
	}
	else
	{
		SetTimeOnPlayhead();
	}

	// update the time onto the playhead
	RefreshStagingArea();

	if (c_clipIsBeingDeleted)
	{
		if ( (GetItemSelected() == PLAYHEAD_TYPE_VIDEO) && (!IsPreviewing()) )
		{
			// move cursor to nearest start position, as we may actually need to re adjust it again if adjusted and then we need to offset the cursor to make it appear at the start or end of the clip based on the new position
			u32 projectIndexHoveringOver = GetProjectIndexNearestStartPos(PLAYHEAD_TYPE_VIDEO, GetPlayheadPositionMs());

			if (projectIndexHoveringOver == (u32)GetProject()->GetClipCount())  // if the clip has been deleted, then move back if we are on the last clip id
 			{
 				projectIndexHoveringOver = GetProject()->GetClipCount()-1;
 			}

			SetPlayheadToStartOfProjectClip(PLAYHEAD_TYPE_VIDEO, projectIndexHoveringOver, false);
		}
	}

	UpdateProjectSpaceIndicator();
	UpdateOverviewBar();
}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorTimeline::SetPlayheadPositionMs
// PURPOSE: Sets the playhead cursor at a set position on the timeline (in Ms)
/////////////////////////////////////////////////////////////////////////////////////////
void CVideoEditorTimeline::SetPlayheadPositionMs(const u32 uNewPlayheadPosMs, const bool bReadjustTimeline)
{
	u32 const c_currentPlayheadPosMs = GetPlayheadPositionMs();
	if (uNewPlayheadPosMs != c_currentPlayheadPosMs)
	{
		// if going to or from a position over the end of the final clip then clear stage clips ready for the special case of displaying just one at the left
		if (c_currentPlayheadPosMs > GetProject()->GetTotalClipTimeMs() || uNewPlayheadPosMs > GetProject()->GetTotalClipTimeMs())
		{
			ClearAllStageClips(false);
		}

		s32 const c_dir = uNewPlayheadPosMs > c_currentPlayheadPosMs ? 1 : -1;
		s32 const c_adjustmentMs = c_dir > 0 ? uNewPlayheadPosMs - c_currentPlayheadPosMs : c_currentPlayheadPosMs - uNewPlayheadPosMs;
		float const c_scaledAdjustmentPixels = MsToPixels(c_adjustmentMs);
		AdjustPlayhead(c_scaledAdjustmentPixels * c_dir, bReadjustTimeline);
	}
}



void CVideoEditorTimeline::AdjustPlayhead(float deltaPixels, bool const c_readjustTimeline)
{
	if (!GetProject())
	{
		SetTimeOnPlayhead(false);

		return;
	}

	if (!CVideoEditorUi::IsProjectPopulated())  // do not move anything if we have no clips
	{
		// if 0 clips, then ensure playhead marker is reset to start
		ms_ComplexObject[TIMELINE_OBJECT_TIMELINE_CONTAINER].SetXPositionInPixels(0.0f);
		ms_ComplexObject[TIMELINE_OBJECT_PLAYHEAD_MARKER].SetXPositionInPixels(0.0f);

		ClearAllStageClips(true);
	}
	else if (deltaPixels != 0.0f)
	{
		s32 const c_currentPlayheadPosMs = (s32)GetPlayheadPositionMs();
		s32 const c_deltaMs = PixelsToMs(deltaPixels);

		if (IsPreviewing() && !ms_trimmingClip)
		{
			// if over end of visible project space, then snap to end of the clip, so we find a new delta
			if (c_currentPlayheadPosMs + c_deltaMs > GetProject()->GetTotalClipTimeMs())
			{
				s32 const c_newSnapPointMs = FindNearestSnapPoint(GetItemSelected(), c_currentPlayheadPosMs, 1);

				if (c_newSnapPointMs != -1)
				{
					if (c_currentPlayheadPosMs + c_deltaMs > c_newSnapPointMs)
					{
						s32 const c_dir = c_newSnapPointMs > c_currentPlayheadPosMs ? 1 : -1;
						s32 const c_adjustmentMs = c_dir > 0 ? c_newSnapPointMs - c_currentPlayheadPosMs : c_currentPlayheadPosMs - c_newSnapPointMs;
						float const c_scaledAdjustmentPixels = MsToPixels(c_adjustmentMs);
						deltaPixels = c_scaledAdjustmentPixels * c_dir;
					}
				}
				else
				{
					return;  // if we were over but for some reason unable to find a clip, we just return and not reposition
				}
			}
		}

		if (ms_ComplexObject[TIMELINE_OBJECT_TIMELINE_CONTAINER].IsActive() && ms_ComplexObject[TIMELINE_OBJECT_TIMELINE_BACKGROUND].IsActive() && ms_ComplexObject[TIMELINE_OBJECT_PLAYHEAD_MARKER].IsActive())
		{
			u32 const c_endOfVisibleProjectOnTimelineMs = GetTotalTimelineTimeMs();

			u32 const c_lastClipEndPosFromTimelinePanelMs = c_endOfVisibleProjectOnTimelineMs;
			float const c_lastClipEndPosFromTimelinePanel = MsToPixels(c_lastClipEndPosFromTimelinePanelMs) - (ms_ComplexObject[TIMELINE_OBJECT_TIMELINE_BACKGROUND].GetWidth());

			float const c_timelineWidth = ms_ComplexObject[TIMELINE_OBJECT_TIMELINE_BACKGROUND].GetWidth();
			float timelineMid = c_timelineWidth * 0.5f;

			float const c_currentContainerPos = ms_ComplexObject[TIMELINE_OBJECT_TIMELINE_CONTAINER].GetXPositionInPixels();
			float const c_currentPlayheadPos = ms_ComplexObject[TIMELINE_OBJECT_PLAYHEAD_MARKER].GetXPositionInPixels();
			float newPlayheadPos = c_currentPlayheadPos + deltaPixels;

#if RSG_PC
			// on PC, when scrubbing or previewing we keep the centre focus of the timeline on the current mouse position, otherwise lock to centre as normal
			bool const c_lastInputMouse = CControlMgr::GetPlayerMappingControl().WasKeyboardMouseLastKnownSource();
			if (c_lastInputMouse && (IsPreviewing() || ms_trimmingClip))
			{
				float const c_mousePosX = (float)ioMouse::GetX();
				float const c_mousePosY = (float)ioMouse::GetY();

				float mousePosXScaleform = c_mousePosX;
				float mousePosYScaleform = c_mousePosY;

				CScaleformMgr::TranslateWindowCoordinateToScaleformMovieCoordinate( 
					c_mousePosX, mousePosYScaleform, CVideoEditorUi::GetMovieId(), mousePosXScaleform, mousePosYScaleform );

				float const c_leftTimelineArea = (TIMELINE_POSITION_X * ACTIONSCRIPT_STAGE_SIZE_X) + ms_ComplexObject[TIMELINE_OBJECT_TIMELINE_BACKGROUND].GetXPositionInPixels();
				mousePosXScaleform -= c_leftTimelineArea;

				if (mousePosXScaleform < AMOUNT_OF_SIDE_VISIBILITY)
				{
					mousePosXScaleform = AMOUNT_OF_SIDE_VISIBILITY;
				}

				if (mousePosXScaleform > c_timelineWidth-AMOUNT_OF_SIDE_VISIBILITY)
				{
					mousePosXScaleform = c_timelineWidth-AMOUNT_OF_SIDE_VISIBILITY;
				}

				timelineMid = mousePosXScaleform;
				newPlayheadPos = c_currentPlayheadPos + deltaPixels;

				// need to round these if we are at ends of timeline to ensure mouse adjustment is smooth
				if (c_currentContainerPos <= -c_lastClipEndPosFromTimelinePanel || c_currentContainerPos >= 0.0f)
				{
					if (!ms_waitForMouseMovement)  // only do the rounding when we are not waiting for mouse movement
					{
						timelineMid = (float)rage::round(timelineMid);
						newPlayheadPos = (float)rage::round(newPlayheadPos);
					}
				}
			}
#endif // RSG_PC
			float const c_diff = newPlayheadPos - timelineMid;

			if (deltaPixels > 0.0f)
			{
				if (-c_currentContainerPos < c_lastClipEndPosFromTimelinePanel)
				{
					if (newPlayheadPos > timelineMid)
					{
						newPlayheadPos -= c_diff;  // adjust by the diff
						ms_ComplexObject[TIMELINE_OBJECT_TIMELINE_CONTAINER].AdjustXPositionInPixels(-c_diff);  // adjust container by the diff

						// cap at the end
						if (ms_ComplexObject[TIMELINE_OBJECT_TIMELINE_CONTAINER].GetXPositionInPixels() < -c_lastClipEndPosFromTimelinePanel)
						{
							newPlayheadPos += (-c_lastClipEndPosFromTimelinePanel-ms_ComplexObject[TIMELINE_OBJECT_TIMELINE_CONTAINER].GetXPositionInPixels());

							ms_ComplexObject[TIMELINE_OBJECT_TIMELINE_CONTAINER].SetXPositionInPixels(-c_lastClipEndPosFromTimelinePanel);
						}
					}
				}
			}

			if (deltaPixels < 0.0f)
			{
				if (c_currentContainerPos < 0.0f)
				{
					if (newPlayheadPos < timelineMid)
					{
						newPlayheadPos -= c_diff;  // adjust by the diff
						ms_ComplexObject[TIMELINE_OBJECT_TIMELINE_CONTAINER].AdjustXPositionInPixels(-c_diff);  // adjust container by the diff

						// cap at the start
						if (ms_ComplexObject[TIMELINE_OBJECT_TIMELINE_CONTAINER].GetXPositionInPixels() > 0.0f)
						{
							newPlayheadPos -= (ms_ComplexObject[TIMELINE_OBJECT_TIMELINE_CONTAINER].GetXPositionInPixels());

							ms_ComplexObject[TIMELINE_OBJECT_TIMELINE_CONTAINER].SetXPositionInPixels(0.0f);
						}
					}
				}
			}

			if (ms_trimmingClip || newPlayheadPos <= MsToPixels(c_lastClipEndPosFromTimelinePanelMs+1))
			{
				ms_ComplexObject[TIMELINE_OBJECT_PLAYHEAD_MARKER].SetXPositionInPixels(newPlayheadPos);
				ms_ComplexObject[TIMELINE_OBJECT_PLAYHEAD_TRIMMING_ICON].SetXPositionInPixels(0);
			}
		}
	}

	if (c_readjustTimeline)
	{
		RepositionPlayheadIfNeeded(false);
	}
}




/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorTimeline::SetTimeOnPlayhead
// PURPOSE:	works out time from milliseconds and places it on the text on the playhead
/////////////////////////////////////////////////////////////////////////////////////////
void CVideoEditorTimeline::SetTimeOnPlayhead(const bool bShow)
{
	u32 uPlayheadPosMs = GetPlayheadPositionMs();

	const s32 c_projectIndexAtPlayhead = GetProjectIndexAtTime(GetItemSelected(), uPlayheadPosMs);
	const s32 c_projectIndexAtPlayheadPlusOne = GetProjectIndexAtTime(GetItemSelected(), uPlayheadPosMs+1);

	if (c_projectIndexAtPlayheadPlusOne != -1 && c_projectIndexAtPlayhead != c_projectIndexAtPlayheadPlusOne)  // if at the very end of a clip, we show the time + 1ms as this will be the duration of the clip we are on, since we overlap items on timeline
	{
		uPlayheadPosMs++;
	}

	if (ms_ComplexObject[TIMELINE_OBJECT_TIMECODE].IsActive() && ms_ComplexObject[TIMELINE_OBJECT_PLAYHEAD_MARKER].IsActive())
	{
		static u32 uPreviousPlayheadPosMs = MAX_UINT32;
		static u32 uPreviousMaxTimeMs = MAX_UINT32;

		if ( IsTimelineSelected() && bShow && CVideoEditorUi::IsProjectPopulated())
		{
			const u32 uProjectTotalTimeMs = (u32)GetProject()->GetTotalClipTimeMs();

			if ( uPlayheadPosMs != uPreviousPlayheadPosMs || uProjectTotalTimeMs != uPreviousMaxTimeMs )
			{
				char cTimecodeText[50];

				// if we have a project, update and make it visible with current values:
				SimpleString_32 CurrentTimeBuffer, MaxTimeBuffer;

				CTextConversion::FormatMsTimeAsString( CurrentTimeBuffer.getWritableBuffer(), CurrentTimeBuffer.GetMaxByteCount(), uPlayheadPosMs );
				CTextConversion::FormatMsTimeAsString( MaxTimeBuffer.getWritableBuffer(), MaxTimeBuffer.GetMaxByteCount(), uProjectTotalTimeMs );

				ms_ComplexObject[TIMELINE_OBJECT_PLAYHEAD_MARKER_TIMECODE_TEXT].SetTextField( CurrentTimeBuffer.getBuffer() );

				if (!ms_ComplexObject[TIMELINE_OBJECT_TIMECODE].IsVisible())
				{
					ms_ComplexObject[TIMELINE_OBJECT_TIMECODE].SetVisible(true);
				}

				ms_ComplexObject[TIMELINE_OBJECT_PLAYHEAD_MARKER].SetVisible(true);

				char colorText[50];
				
				if (uPlayheadPosMs >= (u32)GetProject()->GetMaxDurationMs())
				{
					formatf(colorText, "~HUD_COLOUR_RED~", NELEM(colorText));
				}
				else
				{
					colorText[0] = '\0';
				}

				formatf(cTimecodeText, "%s / %s%s", CurrentTimeBuffer.getBuffer(), colorText, MaxTimeBuffer.getBuffer(), NELEM(cTimecodeText));

				if (CScaleformMgr::BeginMethodOnComplexObject(ms_ComplexObject[TIMELINE_OBJECT_TIMECODE].GetId(), SF_BASE_CLASS_VIDEO_EDITOR, "SET_TIMECODE"))
				{
					CScaleformMgr::AddParamString(cTimecodeText);
					CScaleformMgr::EndMethod();
				}

				uPreviousPlayheadPosMs = uPlayheadPosMs;
				uPreviousMaxTimeMs = uProjectTotalTimeMs;
			}
		}
		else
		{
			if (ms_ComplexObject[TIMELINE_OBJECT_TIMECODE].IsVisible())
			{
				ms_ComplexObject[TIMELINE_OBJECT_TIMECODE].SetVisible(false);
			}

			if (ms_ComplexObject[TIMELINE_OBJECT_PLAYHEAD_MARKER].IsVisible())
			{
				ms_ComplexObject[TIMELINE_OBJECT_PLAYHEAD_MARKER].SetVisible(false);
			}

			uPreviousPlayheadPosMs = MAX_UINT32;
			uPreviousMaxTimeMs = MAX_UINT32;
		}
	}

	// update the colour of the playhead if previewing (it may need to go red if you cant overwrite an item)
	if (ms_ComplexObject[TIMELINE_OBJECT_PLAYHEAD_MARKER_TIMECODE_BACKGROUND].IsActive() &&
		ms_ComplexObject[TIMELINE_OBJECT_PLAYHEAD_MARKER_BACKGROUND].IsActive())
	{
		Color32 const c_color = GetColourForPlayheadType(GetPlayheadPositionMs(), GetItemSelected());
		ms_ComplexObject[TIMELINE_OBJECT_PLAYHEAD_MARKER_TIMECODE_BACKGROUND].SetColour(c_color);
		ms_ComplexObject[TIMELINE_OBJECT_PLAYHEAD_MARKER_BACKGROUND].SetColour(c_color);
	}
}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorTimeline::UpdateDroppedRevertedClip
// PURPOSE: if user has dropped a clip with the mouse then we revert the clip
/////////////////////////////////////////////////////////////////////////////////////////
void CVideoEditorTimeline::UpdateDroppedRevertedClip()
{
	if (!CVideoEditorMenu::IsUserConfirmationScreenActive())
	{
		if (ms_droppedByMouse)
		{
			ms_iProjectPositionToRevertOnUpMs = ms_iProjectPositionToRevertOnDownMs = -1;  // we moved so reset

			if (ms_bHaveProjectToRevert)
			{
				DropPreviewClipAtPosition(ms_uProjectPositionToRevertMs, true);
			}
			else
			{
				GetProject()->InvalidateStagingClip();

				SetPlayheadToStartOfNearestValidClip();
			}

			HidePlayheadPreview();

			RefreshStagingArea();

			ms_droppedByMouse = false;
		}
	}
}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorTimeline::UpdateItemsHighlighted
// PURPOSE:	highlight (and unhighlight previous) clips
/////////////////////////////////////////////////////////////////////////////////////////
void CVideoEditorTimeline::UpdateItemsHighlighted()
{
	if (!ms_stagingClipNeedsUpdated)
	{
		return;
	}

	if (!CVideoEditorUi::IsProjectPopulated())
		return;

	u32 const c_playheadPosMs = GetPlayheadPositionMs();

	
	if (c_playheadPosMs > GetTotalTimelineTimeMs())
	{
		return;
	}

	if (ms_ComplexObjectClips.GetCount() > 0)  // need atleast 1 clip to be able to highlight anything
	{
		s32 const c_projectClipIndexHovered = GetProjectIndexNearestStartPos(GetItemSelected(), c_playheadPosMs, -1);
		s32 const c_timelineClipIndexHovered = ms_trimmingClip ? -1 : GetTimelineIndexFromProjectIndex(GetItemSelected(), c_projectClipIndexHovered);

		if (IsScrollingMediumFast())  // fastest stick acceleration = no requests
		{
			ClearAllStageClips(false);  // hide anything on the 3 larger thumbs as we scroll very fast and do not allow requests at this time as it gets too spammy for the texture requests

			if (c_timelineClipIndexHovered != -1)
			{
				HighlightClip(c_timelineClipIndexHovered, true);
			}

			ms_ComplexObject[TIMELINE_OBJECT_STAGE_CONTAINER].SetAlpha(255);

			if (!CVideoEditorTimeline::IsPreviewing())
			{
				ms_ComplexObject[TIMELINE_OBJECT_STAGE_CONTAINER_PREV].SetAlpha(255);
				ms_ComplexObject[TIMELINE_OBJECT_STAGE_CONTAINER_NEXT].SetAlpha(255);
			}

			SetTimeOnPlayhead();

			return;
		}

		if (c_timelineClipIndexHovered != -1)
		{
			HighlightClip(c_timelineClipIndexHovered, true);
		}

		if (ms_stagingClipNeedsUpdated)
		{
			ms_stagingClipNeedsUpdated = !SetupStagePreviewClip(c_playheadPosMs);
		}
		else
		{
			if (GetPlayheadPositionMs() > GetProject()->GetTotalClipTimeMs())
			{
				ms_renderOverlayOverCentreStageClip = false;
			}
		}
	}

	SetTimeOnPlayhead();
}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorTimeline::ProcessOverlayOverCentreStageClipAtPlayheadPosition
// PURPOSE: renders/sets up template for text at the current position on the playhead
/////////////////////////////////////////////////////////////////////////////////////////
void CVideoEditorTimeline::ProcessOverlayOverCentreStageClipAtPlayheadPosition()
{
	if (!GetProject())
	{
		return;
	}

	u32 const c_playheadPos = GetPlayheadPositionMs();
	s32 textIndex = GetProjectIndexBetweenTime(PLAYHEAD_TYPE_TEXT, c_playheadPos, c_playheadPos);

	if (textIndex == -1 && c_playheadPos > 0)
	{
		textIndex = GetProjectIndexBetweenTime(PLAYHEAD_TYPE_TEXT, c_playheadPos-1, c_playheadPos-1);
	}

	// early out if conditions are not correct to render the overlay over the centre stage clip:
	if (textIndex == -1 ||
		!ShouldRenderOverlayOverCentreStageClip() ||
		!CVideoEditorTimeline::IsTimelineSelected() ||
		IsPreviewing() ||
		GetProject()->GetOverlayTextCount() == 0)
	{
#if USE_TEXT_CANVAS
		if (sm_currentOverlayTextIndexInUse != -1)
		{
			CTextTemplate::UnloadTemplate();
			sm_currentOverlayTextIndexInUse = -1;
		}
#endif // #if USE_TEXT_CANVAS

		return;
	}
	
	//
	// we can render, so set up the template based on the current text the playhead is over:
	//
	CTextOverlayHandle const &text = GetProject()->GetOverlayText(textIndex);

	if (text.IsValid())
	{
#if USE_TEXT_CANVAS
		if (textIndex != sm_currentOverlayTextIndexInUse)
		{
			sEditedTextProperties canvasText;

			strcpy(canvasText.m_text, text.getText(0));
			canvasText.m_position = text.getTextPosition(0);
			canvasText.m_scale = text.getTextScale(0).y;
			CRGBA colorRGB = text.getTextColor(0);
			colorRGB.SetAlpha(255);
			canvasText.m_hudColor = CHudColour::GetColourFromRGBA(colorRGB);
			canvasText.m_colorRGBA = text.getTextColor(0);
			canvasText.m_style = text.getTextStyle(0);
			canvasText.m_alpha = text.getTextColor(0).GetAlphaf();

			CTextTemplate::SetupTemplate(canvasText);
		}
#else
#if USE_CODE_TEXT
		RenderStageText(text.getText(0), text.getTextPosition(0), text.getTextScale(0), text.getTextColor(0), text.getTextStyle(0), ms_vStagePos, ms_vStageSize);
#endif // #if USE_CODE_TEXT
#endif // #if USE_TEXT_CANVAS
	}

	sm_currentOverlayTextIndexInUse = textIndex;
}


#if USE_CODE_TEXT
/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorTimeline::RenderStageText
// PURPOSE: Renders the CMontageText that is passed in at the scale of the preview stage
/////////////////////////////////////////////////////////////////////////////////////////
void CVideoEditorTimeline::RenderStageText(const char *pText, Vector2 const& c_pos, Vector2 const& c_scale, CRGBA const& c_color, s32 c_style, Vector2 const& c_stagePos, Vector2 const& c_stageSize)
{
	// scale down from full screen, to the coordinates of the large thumbnail (stage)
	Vector2 const c_newPos = Vector2((c_pos.x * c_stageSize.x) + c_stagePos.x, (c_pos.y * c_stageSize.y) + c_stagePos.y);
	Vector2 const c_newScale = Vector2(c_scale.x * c_stageSize.x, c_scale.y * c_stageSize.y);

	float const c_wrapPercentage = (c_stageSize.x * 0.2f);  // 80% title safe area

	OverlayTextRenderer::RenderText(pText, c_newPos, c_newScale, c_color, c_style, Vector2(c_stagePos.x + c_wrapPercentage, c_stagePos.x+c_stageSize.x), c_stagePos, c_stageSize);
}
#endif // #if USE_CODE_TEXT

void CVideoEditorTimeline::ReleaseThumbnail( u32 const c_projectIndex )
{
	ms_TimelineThumbnailMgr.ReleaseThumbnail( c_projectIndex );
}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorTimeline::MsToPixels
// PURPOSE:	converts ms value into timeline pixel value
/////////////////////////////////////////////////////////////////////////////////////////
float CVideoEditorTimeline::MsToPixels(const u32 uTimeInMs)
{
	return ((float)(uTimeInMs) / MS_TO_PIXEL_SCALE_FACTOR); // every 1 pixel represents 10 milliseconds
}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorTimeline::PixelsToMs
// PURPOSE:	converts timeline pixel value to a time in ms value
/////////////////////////////////////////////////////////////////////////////////////////
s32 CVideoEditorTimeline::PixelsToMs(const float fPixels)
{
	return s32( rage::round(fPixels * MS_TO_PIXEL_SCALE_FACTOR) ); // every 1 pixel represents 10 milliseconds
}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorTimeline::RebuildAnchors
// PURPOSE:	rebuilds all the anchors on the timeline
/////////////////////////////////////////////////////////////////////////////////////////
void CVideoEditorTimeline::RebuildAnchors()
{
	ClearAllAnchors();  // remove any existing anchors if we are to rebuild

	const s32 c_timelineIndexOfFirstClip = GetTimelineIndexFromProjectIndex(PLAYHEAD_TYPE_VIDEO, 0);

	if (GetProject()->GetClipCount() > 0 && c_timelineIndexOfFirstClip != -1)
	{
		for (u32 projectIndex = 0; projectIndex < GetProject()->GetAnchorCount(); projectIndex++)
		{
			u32 const c_anchorTimeMs = (u32)GetProject()->GetAnchorTimeMs(projectIndex);

			if (c_anchorTimeMs < GetProject()->GetTotalClipTimeMs())  // only add the anchor if its within the project time
			{
				AddAnchor(projectIndex, c_anchorTimeMs);
			}
		}
	}

	CVideoEditorUi::GarbageCollect();
}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorTimeline::RebuildTimeline
// PURPOSE:	completly clear the timeline and rebuild it based on current project data
/////////////////////////////////////////////////////////////////////////////////////////
bool CVideoEditorTimeline::RebuildTimeline()
{
	bool success = false;

	CVideoEditorProject* project = GetProject();

	if( project )
	{
		s32 projectIndex;

		for (projectIndex = 0; projectIndex < project->GetClipCount(); projectIndex++)
		{
			AddVideo(projectIndex);
		}

		for (projectIndex = 0; projectIndex < project->GetAmbientClipCount(); projectIndex++)
		{
			AddAmbient(projectIndex, project->GetAmbientClip(projectIndex));
		}

		for (projectIndex = 0; projectIndex < project->GetMusicClipCount(); projectIndex++)
		{
			AddAudio(projectIndex, project->GetMusicClip(projectIndex));
		}

		for (projectIndex = 0; projectIndex < project->GetOverlayTextCount(); projectIndex++)
		{
			AddText(projectIndex, project->GetOverlayText(projectIndex));
		}

		RebuildAnchors();

		UpdateClipsOutsideProjectTime();

		SetPlayheadToStartOfNearestValidClip();

		success = true;
	}

	CVideoEditorUi::GarbageCollect();
	return success;
}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorTimeline::InsertVideoClipIntoTimeline
// PURPOSE:	inserts a videoclip into the middle of the timeline without having to
//			rebuild it all
/////////////////////////////////////////////////////////////////////////////////////////
void CVideoEditorTimeline::InsertVideoClipIntoTimeline(u32 const c_projectIndex)
{
	// if this is the 1st clip added but there is music or text still in the project, then rebuild the timeline
	if ( (GetProject()->GetClipCount() == 1) && (GetProject()->GetMusicClipCount() != 0 || GetProject()->GetOverlayTextCount() != 0) )
	{
		ClearAllClips();
		RebuildTimeline();

		return;
	}

	u32 const c_startTimeMs = (u32)GetProject()->GetTrimmedTimeToClipMs(c_projectIndex);
	float const c_durationPixels = MsToPixels((u32)GetProject()->GetClipDurationTimeMs(c_projectIndex));

	for (s32 i = 0; i < ms_ComplexObjectClips.GetCount(); i++)  // go through full list
	{
		if (ms_ComplexObjectClips[i].object.IsActive())  // active
		{
			if (ms_ComplexObjectClips[i].type == PLAYHEAD_TYPE_VIDEO || ms_ComplexObjectClips[i].type == PLAYHEAD_TYPE_OVERVIEW_VIDEO)  // its a video clip
			{
				// now adjust values:
				u32 const c_startTimeOfThisClipMs = (u32)GetProject()->GetTrimmedTimeToClipMs(ms_ComplexObjectClips[i].uIndexToProject);

				if (c_startTimeOfThisClipMs >= c_startTimeMs)  // its clip id is right of the one we are inserting
				{
					ms_ComplexObjectClips[i].uIndexToProject++;

					ms_ComplexObjectClips[i].object.AdjustXPositionInPixels(c_durationPixels);
				}
			}
		}
	}

	// add the new clip:
	AddVideo(c_projectIndex);
	ClearAllStageClips(true);

	if (c_projectIndex > 0 && GetProject()->GetClipCount()-1 == (s32)c_projectIndex)
	{
		SetPlayheadPositionMs((u32)GetProject()->GetTotalClipTimeMs(), true);
	}
	else
	{
		SetPlayheadToStartOfProjectClip(PLAYHEAD_TYPE_VIDEO, c_projectIndex, true);  // set playhead to start of this newly added clip
	}

	UpdateClipsOutsideProjectTime();

#if __BANK
	//OutputTimelineDebug(c_projectIndex);
#endif // __BANK

	CVideoEditorUi::GarbageCollect();
}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorTimeline::InsertAmbientClipIntoTimeline
// PURPOSE:	inserts a audio clip into the middle of the timeline without having to
//			rebuild it all
/////////////////////////////////////////////////////////////////////////////////////////
void CVideoEditorTimeline::InsertAmbientClipIntoTimeline(CAmbientClipHandle const &c_handle)
{
	u32 const c_projectIndex = (u32)GetProject()->GetAmbientClipCount()-1;
	AddAmbient(c_projectIndex, c_handle);

	SetPlayheadToStartOfProjectClip(PLAYHEAD_TYPE_AMBIENT, c_projectIndex, true);  // ensure playhead is always at start of new clip

	RepositionPlayheadIfNeeded(false);
}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorTimeline::InsertMusicClipIntoTimeline
// PURPOSE:	inserts a audio clip into the middle of the timeline without having to
//			rebuild it all
/////////////////////////////////////////////////////////////////////////////////////////
void CVideoEditorTimeline::InsertMusicClipIntoTimeline(CMusicClipHandle const &handle)
{
	u32 const c_projectIndex = (u32)GetProject()->GetMusicClipCount()-1;
	AddAudio(c_projectIndex, handle);

	SetPlayheadToStartOfProjectClip(PLAYHEAD_TYPE_AUDIO, c_projectIndex, true);  // ensure playhead is always at start of new clip

	RepositionPlayheadIfNeeded(false);
}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorTimeline::InsertTextClipIntoTimeline
// PURPOSE:	inserts a text clip into the middle of the timeline without having to
//			rebuild it all
/////////////////////////////////////////////////////////////////////////////////////////
void CVideoEditorTimeline::InsertTextClipIntoTimeline(CTextOverlayHandle const &handle)
{
	u32 const c_projectIndex = (u32)GetProject()->GetOverlayTextCount()-1;
	AddText(c_projectIndex, handle);
	
	SetPlayheadToStartOfProjectClip(PLAYHEAD_TYPE_TEXT, c_projectIndex, true);  // ensure playhead is always at start of new clip

	RepositionPlayheadIfNeeded(false);
}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorTimeline::DeleteVideoClipFromTimeline
// PURPOSE:	deletes one video clip from the timeline
/////////////////////////////////////////////////////////////////////////////////////////
void CVideoEditorTimeline::DeleteVideoClipFromTimeline(u32 const c_projectIndex)
{
	u32 const c_timelineIndex = GetTimelineIndexFromProjectIndex(PLAYHEAD_TYPE_VIDEO, c_projectIndex);
	u32 const c_startTimeMs = (u32)GetProject()->GetTrimmedTimeToClipMs(c_projectIndex);
	float const c_durationPixels = MsToPixels((u32)GetProject()->GetClipDurationTimeMs(c_projectIndex));

	if (c_timelineIndex != -1)
	{
		HighlightClip(c_timelineIndex, false);  // ensure clip we are removing is unhighlighted
	}

	s32 overViewBarIndex = -1;

	if (ms_ComplexObject[TIMELINE_OBJECT_TIMELINE_CONTAINER].IsActive())
	{
		for (s32 i = 0; i < ms_ComplexObjectClips.GetCount(); i++)  // go through full list
		{
			if (ms_ComplexObjectClips[i].object.IsActive())  // active
			{
				if (ms_ComplexObjectClips[i].type == PLAYHEAD_TYPE_VIDEO || ms_ComplexObjectClips[i].type == PLAYHEAD_TYPE_OVERVIEW_VIDEO)  // its a video clip
				{
					// delete the overview object associated with this item
					if (ms_ComplexObjectClips[i].type == PLAYHEAD_TYPE_OVERVIEW_VIDEO && ms_ComplexObjectClips[i].uIndexToProject == c_projectIndex)
					{
						overViewBarIndex = i;
					}
					else
					{
						// now adjust values:
						u32 const c_startTimeOfThisClipMs = (u32)GetProject()->GetTrimmedTimeToClipMs(ms_ComplexObjectClips[i].uIndexToProject);
						if (c_startTimeOfThisClipMs > c_startTimeMs)
						{
							if (ms_ComplexObjectClips[i].uIndexToProject > 0)
							{
								ms_ComplexObjectClips[i].uIndexToProject--;
							}

							ms_ComplexObjectClips[i].object.AdjustXPositionInPixels(-c_durationPixels);
						}
					}
				}
			}
		}
	}

	ClearAllStageClips(true);
	ReleaseAllStageClipThumbnailsIfNeeded();


	// delete the overview bar clip:
	if (overViewBarIndex != -1)
	{
		DeleteClip(overViewBarIndex);
	}

	// delete the timeline clip:
	if (c_timelineIndex != -1)
	{
		DeleteClip(c_timelineIndex);
	}

	UpdateClipsOutsideProjectTime();

	RefreshStagingArea();

	UpdateProjectSpaceIndicator();
	UpdateOverviewBar();

#if __BANK
	//OutputTimelineDebug(c_projectIndex);
#endif // __BANK
}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorTimeline::DeleteAmbientClipFromTimeline
// PURPOSE:	deletes one ambient clip from the timeline
/////////////////////////////////////////////////////////////////////////////////////////
void CVideoEditorTimeline::DeleteAmbientClipFromTimeline(const u32 c_projectIndex)
{
	u32 const c_timelineIndex = GetTimelineIndexFromProjectIndex(PLAYHEAD_TYPE_AMBIENT, c_projectIndex);
	s32 overViewBarIndex = -1;

	if (c_timelineIndex != -1)
	{
		HighlightClip(c_timelineIndex, false);  // ensure clip we are removing is unhighlighted
	}

	// first of all, shift everything after c_projectIndex along by one:
	if (ms_ComplexObject[TIMELINE_OBJECT_TIMELINE_CONTAINER].IsActive())
	{
		for (s32 i = 0; i < ms_ComplexObjectClips.GetCount(); i++)  // go through full list
		{
			if (ms_ComplexObjectClips[i].object.IsActive())  // active
			{
				if (ms_ComplexObjectClips[i].type == PLAYHEAD_TYPE_AMBIENT || ms_ComplexObjectClips[i].type == PLAYHEAD_TYPE_OVERVIEW_AMBIENT)  // its a audio clip
				{
					// delete the overview object associated with this item
					if (ms_ComplexObjectClips[i].type == PLAYHEAD_TYPE_OVERVIEW_AMBIENT && ms_ComplexObjectClips[i].uIndexToProject == c_projectIndex)
					{
						overViewBarIndex = i;
					}
					else
					{
						// now adjust values:
						if (ms_ComplexObjectClips[i].uIndexToProject > c_projectIndex) 
						{
							if (ms_ComplexObjectClips[i].uIndexToProject > 0)
							{
								ms_ComplexObjectClips[i].uIndexToProject--;
							}
						}
					}
				}
			}
		}
	}

	// delete the overview bar clip:
	if (overViewBarIndex != -1)
	{
		DeleteClip(overViewBarIndex);
	}

	// delete the timeline clip:
	if (c_timelineIndex != -1)
	{
		DeleteClip(c_timelineIndex);
	}

	UpdateProjectSpaceIndicator();
	UpdateOverviewBar();

#if __BANK
	//OutputTimelineDebug(c_projectIndex);
#endif // __BANK
}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorTimeline::DeleteMusicClipFromTimeline
// PURPOSE:	deletes one audio clip from the timeline
/////////////////////////////////////////////////////////////////////////////////////////
void CVideoEditorTimeline::DeleteMusicClipFromTimeline(const u32 c_projectIndex)
{
	u32 const c_timelineIndex = GetTimelineIndexFromProjectIndex(PLAYHEAD_TYPE_AUDIO, c_projectIndex);
	s32 overViewBarIndex = -1;

	if (c_timelineIndex != -1)
	{
		HighlightClip(c_timelineIndex, false);  // ensure clip we are removing is unhighlighted
	}

	// first of all, shift everything after c_projectIndex along by one:
	if (ms_ComplexObject[TIMELINE_OBJECT_TIMELINE_CONTAINER].IsActive())
	{
		for (s32 i = 0; i < ms_ComplexObjectClips.GetCount(); i++)  // go through full list
		{
			if (ms_ComplexObjectClips[i].object.IsActive())  // active
			{
				if (ms_ComplexObjectClips[i].type == PLAYHEAD_TYPE_AUDIO || ms_ComplexObjectClips[i].type == PLAYHEAD_TYPE_OVERVIEW_AUDIO)  // its a audio clip
				{
					// delete the overview object associated with this item
					if (ms_ComplexObjectClips[i].type == PLAYHEAD_TYPE_OVERVIEW_AUDIO && ms_ComplexObjectClips[i].uIndexToProject == c_projectIndex)
					{
						overViewBarIndex = i;
					}
					else
					{
						// now adjust values:
						if (ms_ComplexObjectClips[i].uIndexToProject > c_projectIndex) 
						{
							if (ms_ComplexObjectClips[i].uIndexToProject > 0)
							{
								ms_ComplexObjectClips[i].uIndexToProject--;
							}
						}
					}
				}
			}
		}
	}

	// delete the overview bar clip:
	if (overViewBarIndex != -1)
	{
		DeleteClip(overViewBarIndex);
	}

	// delete the timeline clip:
	if (c_timelineIndex != -1)
	{
		DeleteClip(c_timelineIndex);
	}

	UpdateProjectSpaceIndicator();
	UpdateOverviewBar();

#if __BANK
	//OutputTimelineDebug(c_projectIndex);
#endif // __BANK
}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorTimeline::DeleteTextClipFromTimeline
// PURPOSE:	deletes one text clip from the timeline
/////////////////////////////////////////////////////////////////////////////////////////
void CVideoEditorTimeline::DeleteTextClipFromTimeline(const u32 c_projectIndex)
{
	s32 const c_timelineIndex = GetTimelineIndexFromProjectIndex(PLAYHEAD_TYPE_TEXT, c_projectIndex);

	if (c_timelineIndex != -1)
	{
		HighlightClip(c_timelineIndex, false);  // ensure clip we are removing is unhighlighted
	}

	s32 overViewBarIndex = -1;

	// first of all, shift everything after c_projectIndex along by one:
	if (ms_ComplexObject[TIMELINE_OBJECT_TIMELINE_CONTAINER].IsActive())
	{
		for (s32 i = 0; i < ms_ComplexObjectClips.GetCount(); i++)  // go through full list
		{
			if (ms_ComplexObjectClips[i].object.IsActive())  // active
			{
				if (ms_ComplexObjectClips[i].type == PLAYHEAD_TYPE_TEXT || ms_ComplexObjectClips[i].type == PLAYHEAD_TYPE_OVERVIEW_TEXT)  // its a text clip
				{
					// delete the overview object associated with this item
					if (ms_ComplexObjectClips[i].type == PLAYHEAD_TYPE_OVERVIEW_TEXT && ms_ComplexObjectClips[i].uIndexToProject == c_projectIndex)
					{
						overViewBarIndex = i;
					}
					else
					{
						// now adjust values:
						if (ms_ComplexObjectClips[i].uIndexToProject > c_projectIndex) 
						{
							if (ms_ComplexObjectClips[i].uIndexToProject > 0)
							{
								ms_ComplexObjectClips[i].uIndexToProject--;
							}
						}
					}
				}
			}
		}
	}

	// delete the overview bar clip:
	if (overViewBarIndex != -1)
	{
		DeleteClip(overViewBarIndex);
	}

	// delete the timeline clip:
	if (c_timelineIndex != -1)
	{
		DeleteClip(c_timelineIndex);
	}

	UpdateProjectSpaceIndicator();
	UpdateOverviewBar();

#if __BANK
	//OutputTimelineDebug(c_projectIndex);
#endif // __BANK
}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorTimeline::CanPreviewClipBeDroppedAtPosition
// PURPOSE:	can the previewed clip (audio or text) be dropped here?
/////////////////////////////////////////////////////////////////////////////////////////
bool CVideoEditorTimeline::CanPreviewClipBeDroppedAtPosition(u32 const c_positionMs, s32 const c_indexToIgnore)
{
	if (!CVideoEditorUi::IsProjectPopulated())
	{
		return false;
	}

	if (IsPreviewingVideo())  // we can always drop video
	{
		return true;
	}

	if (c_positionMs > GetProject()->GetTotalClipTimeMs())  // do not allow anything to be dropped over end of the 'clip space'
	{
		return false;
	}

	u32 durationToCheckFor = 0;

	if (IsPreviewingAudioTrack())
	{
		durationToCheckFor += CVideoEditorUi::GetSelectedAudio()->m_durationMs-1;
	}

	if (IsPreviewingText())
	{
		durationToCheckFor += CTextTemplate::GetEditedText().m_durationMs-1;
	}

	s32 const c_hoveringIndex = GetProjectIndexBetweenTime(GetItemSelected(), c_positionMs, c_positionMs + durationToCheckFor);

	return (c_hoveringIndex == -1 || c_hoveringIndex == c_indexToIgnore);
}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorTimeline::GetColourForPlayheadType
// PURPOSE:	returns the hud colour to use for the passed type.  We can change the
//			colour used here based on context
/////////////////////////////////////////////////////////////////////////////////////////
Color32 CVideoEditorTimeline::GetColourForPlayheadType(u32 const c_positionMs, ePLAYHEAD_TYPE const c_type, u8 const c_alpha, bool const c_forceColor)
{
	Color32 color;

	if (c_type != PLAYHEAD_TYPE_NONE)
	{
		color = CHudColour::GetRGB(s_TimelineColours[(s32)c_type], c_alpha);  // use the basic colour based on the passed type
	
		if (!CanPreviewClipBeDroppedAtPosition(c_positionMs) && !CanPreviewClipBeDroppedAtPosition(c_positionMs+1))
		{
			// if previewing audio or text, then turn the playhead RED if it is hovering over something (to indicate it cannot be overwritten without deleting)
			if (!c_forceColor && IsPreviewingNonVideo())
			{
				return CHudColour::GetRGB(HUD_COLOUR_RED, c_alpha);
			}
		}

		//
		// override the colour based on context:
		//
		if (IsPlayheadAudioScore(c_positionMs, c_type))
		{
			color = CHudColour::GetRGB(HUD_COLOUR_VIDEO_EDITOR_SCORE, c_alpha);
		}
	}
	else
	{
		uiAssertf(0, "CVideoEditorTimeline::GetColourForPlayheadType() - Cannot get colour from PLAYHEAD_TYPE_NONE");
		color = CHudColour::GetRGB(HUD_COLOUR_PURE_WHITE, c_alpha);
	}

	return color;
}


bool CVideoEditorTimeline::IsPlayheadAudioScore(u32 const c_positionMs, ePLAYHEAD_TYPE const c_type)
{
	if (c_type == PLAYHEAD_TYPE_AUDIO)  // if type is audio track, has a soundhash and is a score track, then use the 'score' colour, but retain audio 'type'
	{
		s32 const c_hoveringIndex = GetProjectIndexNearestStartPos(c_type, c_positionMs, -1);

		if (c_hoveringIndex != -1 || IsPreviewing())
		{
			u32 soundHash = 0;

			if (!IsPreviewing())
			{
				CMusicClipHandle const &c_MusicClip = GetProject()->GetMusicClip(c_hoveringIndex);

				if (c_MusicClip.IsValid())
				{
					soundHash = c_MusicClip.getClipSoundHash();
				}
			}
			else
			{
				soundHash = CVideoEditorUi::GetSelectedAudio()->m_soundHash;
			}

			if (soundHash != 0)
			{
				if (audRadioTrack::IsScoreTrack(soundHash))
				{
					return true;
				}
			}
		}
	}

	return false;
}


bool CVideoEditorTimeline::IsPlayheadCommercial(u32 const c_positionMs, ePLAYHEAD_TYPE const c_type)
{
	if (c_type == PLAYHEAD_TYPE_AUDIO)  // if type is audio track, has a soundhash and is a score track, then use the 'score' colour, but retain audio 'type'
	{
		s32 const c_hoveringIndex = GetProjectIndexNearestStartPos(c_type, c_positionMs, -1);

		if (c_hoveringIndex != -1 || IsPreviewing())
		{
			u32 soundHash = 0;

			if ( (!IsPreviewing()) || (!CanPreviewClipBeDroppedAtPosition(c_positionMs)) )
			{
				CMusicClipHandle const &c_MusicClip = GetProject()->GetMusicClip(c_hoveringIndex);

				if (c_MusicClip.IsValid())
				{
					soundHash = c_MusicClip.getClipSoundHash();
				}
			}
			else
			{
				soundHash = CVideoEditorUi::GetSelectedAudio()->m_soundHash;
			}

			if (soundHash != 0)
			{
				if (audRadioTrack::IsCommercial(soundHash))
				{
					return true;
				}
			}
		}
	}

	return false;
}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorTimeline::UpdatePlayheadPreview
// PURPOSE:	update the playhead preview icon, thumbnail, and colour
/////////////////////////////////////////////////////////////////////////////////////////
void CVideoEditorTimeline::UpdatePlayheadPreview(const ePLAYHEAD_TYPE type)
{
	ms_PlayheadPreview = type;

	if ( ms_ComplexObject[TIMELINE_OBJECT_PLAYHEAD_THUMBNAIL].IsActive() &&
		 ms_ComplexObject[TIMELINE_OBJECT_PLAYHEAD_MARKER_TIMECODE_BACKGROUND].IsActive() &&
		 ms_ComplexObject[TIMELINE_OBJECT_PLAYHEAD_MARKER_BACKGROUND].IsActive() )
	{
		Color32 color;

		u32 const uPlayheadPosMs = GetPlayheadPositionMs();

		if (type != PLAYHEAD_TYPE_NONE)
		{
			if (IsPreviewing())
			{
				ms_ItemSelected = type;  // just set it here without checks as we may actually have no clips yet
			}

			s32 frame = (s32)type;

			if (IsPlayheadAudioScore(uPlayheadPosMs, type))
			{
#define PLAYHEAD_TYPE_SCORE (PLAYHEAD_TYPE_TEXT + 1)  // score is always after the text type in the actionscript

				frame = PLAYHEAD_TYPE_SCORE;  // 'SCORE' icon
			}

			ms_ComplexObject[TIMELINE_OBJECT_PLAYHEAD_THUMBNAIL].GotoFrame(frame);
			ms_ComplexObject[TIMELINE_OBJECT_PLAYHEAD_MARKER_TIMECODE_BACKGROUND].GotoFrame(frame);
			
/*			if (type == PLAYHEAD_TYPE_VIDEO)
			{
				if (!CHudTools::GetWideScreen())
				{
					// scale and reposition the thumb if in 4:3 (fixes 2167322)
					ms_ComplexObject[TIMELINE_OBJECT_PLAYHEAD_THUMBNAIL].SetScale(Vector2(80.0f,80.0f));
					ms_ComplexObject[TIMELINE_OBJECT_PLAYHEAD_THUMBNAIL].SetYPositionInPixels(-52.0f);
				}
				else
				{
					ms_ComplexObject[TIMELINE_OBJECT_PLAYHEAD_THUMBNAIL].SetScale(Vector2(100.0f,100.0f));
					ms_ComplexObject[TIMELINE_OBJECT_PLAYHEAD_THUMBNAIL].SetYPositionInPixels(-66.0f);
				}
			}
*/

			ms_ComplexObject[TIMELINE_OBJECT_PLAYHEAD_THUMBNAIL].SetVisible(true);
			color = GetColourForPlayheadType(uPlayheadPosMs, type);  // use type colour
		}
		else
		{
			ms_maxPreviewAreaMs = 0;
			ms_ComplexObject[TIMELINE_OBJECT_PLAYHEAD_THUMBNAIL].GotoFrame((s32)GetItemSelected());
			ms_ComplexObject[TIMELINE_OBJECT_PLAYHEAD_MARKER_TIMECODE_BACKGROUND].GotoFrame((s32)GetItemSelected());

			ms_ComplexObject[TIMELINE_OBJECT_PLAYHEAD_THUMBNAIL].SetVisible(false);
			color = GetColourForPlayheadType(uPlayheadPosMs, GetItemSelected());
		}

		ms_ComplexObject[TIMELINE_OBJECT_PLAYHEAD_MARKER_TIMECODE_BACKGROUND].SetColour(color);
		ms_ComplexObject[TIMELINE_OBJECT_PLAYHEAD_MARKER_BACKGROUND].SetColour(color);
	}

	if ( IsPreviewing() || CVideoEditorUi::IsProjectPopulated() )
	{
		ms_ComplexObject[TIMELINE_OBJECT_PLAYHEAD_MARKER].SetVisible(true);
	}
	else
	{
		ms_ComplexObject[TIMELINE_OBJECT_PLAYHEAD_MARKER].SetVisible(false);
	}

	if (ms_ComplexObject[TIMELINE_OBJECT_PLAYHEAD_TRIMMING_ICON].IsActive())
	{
		ms_ComplexObject[TIMELINE_OBJECT_PLAYHEAD_TRIMMING_ICON].SetXPositionInPixels(0);
		ms_ComplexObject[TIMELINE_OBJECT_PLAYHEAD_TRIMMING_ICON].SetAlpha(0);
	}

	if (type != PLAYHEAD_TYPE_NONE)
	{
		CVideoEditorUi::ShowTimeline();
	}

	ClearAllStageClips(false);
	RepositionPlayheadIfNeeded(false);
	RefreshStagingArea();

	CVideoEditorInstructionalButtons::Refresh();
}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorTimeline::AddVideoToPlayheadPreview
// PURPOSE:	sets the playhead preview to be a video clip and adds the thumnail image
/////////////////////////////////////////////////////////////////////////////////////////
void CVideoEditorTimeline::AddVideoToPlayheadPreview( const char *pThumbnailTxdName, const char * pThumbnailFilename)
{
	if (GetItemSelected() != PLAYHEAD_TYPE_VIDEO)  // move playhead to start of the nearest video clip: (fixes 2458201)
	{
		s32 const c_nearestProjectIndex = GetProjectIndexNearestStartPos(PLAYHEAD_TYPE_VIDEO, GetPlayheadPositionMs());
		if (c_nearestProjectIndex != -1)
		{
			SetPlayheadToStartOfProjectClip(PLAYHEAD_TYPE_VIDEO, (u32)c_nearestProjectIndex, true);
		}
	}

	UpdatePlayheadPreview(PLAYHEAD_TYPE_VIDEO);

	if (CScaleformMgr::BeginMethodOnComplexObject(ms_ComplexObject[TIMELINE_OBJECT_PLAYHEAD_THUMBNAIL].GetId(), SF_BASE_CLASS_VIDEO_EDITOR, "unloadComponent"))
	{
		CScaleformMgr::EndMethod();
	}

	if (CScaleformMgr::BeginMethodOnComplexObject(ms_ComplexObject[TIMELINE_OBJECT_PLAYHEAD_THUMBNAIL].GetId(), SF_BASE_CLASS_VIDEO_EDITOR, "loadComponent"))
	{
		CScaleformMgr::AddParamString(pThumbnailTxdName, false);
		CScaleformMgr::AddParamString(pThumbnailFilename, false);
		CScaleformMgr::EndMethod();
	}
}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorTimeline::AddAmbientToPlayheadPreview
// PURPOSE:	sets the playhead preview to be an audio clip
/////////////////////////////////////////////////////////////////////////////////////////
void CVideoEditorTimeline::AddAmbientToPlayheadPreview()
{
	UpdatePlayheadPreview(PLAYHEAD_TYPE_AMBIENT);
}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorTimeline::AddAudioToPlayheadThumbnail
// PURPOSE:	sets the playhead preview to be an audio clip
/////////////////////////////////////////////////////////////////////////////////////////
void CVideoEditorTimeline::AddAudioToPlayheadPreview()
{
	UpdatePlayheadPreview(PLAYHEAD_TYPE_AUDIO);
}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorTimeline::AddTextToPlayheadPreview
// PURPOSE:	sets the playhead preview to be an text clip
/////////////////////////////////////////////////////////////////////////////////////////
void CVideoEditorTimeline::AddTextToPlayheadPreview()
{
	UpdatePlayheadPreview(PLAYHEAD_TYPE_TEXT);
}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorTimeline::HidePlayheadPreview
// PURPOSE:	sets the playhead preview off
/////////////////////////////////////////////////////////////////////////////////////////
void CVideoEditorTimeline::HidePlayheadPreview()
{
	if (CScaleformMgr::BeginMethodOnComplexObject(ms_ComplexObject[TIMELINE_OBJECT_PLAYHEAD_THUMBNAIL].GetId(), SF_BASE_CLASS_VIDEO_EDITOR, "unloadComponent"))
	{
		CScaleformMgr::EndMethod();
	}

	if (ms_ComplexObject[TIMELINE_OBJECT_PLAYHEAD_THUMBNAIL].IsActive())
	{
		ms_ComplexObject[TIMELINE_OBJECT_PLAYHEAD_THUMBNAIL].SetVisible(false);
	}

	UpdatePlayheadPreview(PLAYHEAD_TYPE_NONE);

	CVideoEditorInstructionalButtons::Refresh();

	// reset the revert values
	ms_uProjectPositionToRevertMs = 0;
	ms_bHaveProjectToRevert = false;
}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorTimeline::AdjustPlayheadToNearestDroppablePosition
// PURPOSE:	sets the playhead to the next nearest position that a previewing item can
//			be dropped onto the timeline.   Will give up when it reaches the end
//			and wont adjust it if it fails to find any space
/////////////////////////////////////////////////////////////////////////////////////////
u32 CVideoEditorTimeline::AdjustToNearestDroppablePosition(u32 positionMs)
{
	if (IsPreviewingNonVideo())
	{
		u32 const c_totalTimelineTimeMs = GetTotalTimelineTimeMs();

		while (!CanPreviewClipBeDroppedAtPosition(positionMs) && positionMs < c_totalTimelineTimeMs)
		{
			positionMs++;
		}
	}

	return positionMs;
}



bool CVideoEditorTimeline::ReleaseAllStageClipThumbnailsIfNeeded()
{
	bool bReleasedThisFrame = false;

	for (s32 stageClip = 0; stageClip < MAX_STAGE_CLIPS; stageClip++)
	{
		if (ms_iStageClipThumbnailToBeReleased[stageClip] != -1)
		{
			GetProject()->ReleaseClipThumbnail((u32)ms_iStageClipThumbnailToBeReleased[stageClip]);
			ms_iStageClipThumbnailToBeReleased[stageClip] = -1;  // if its released then it cant be requested or in use, so reset these (fixes 2456794)

			bReleasedThisFrame = true;
		}
	}

	return bReleasedThisFrame;
}




void CVideoEditorTimeline::ClearAllStageClips(bool const c_releaseClip)
{
	for (s32 i = 0; i < MAX_STAGE_CLIPS; i++)
	{
		ClearStageClip(i, c_releaseClip);
	}
}



void CVideoEditorTimeline::ClearStageClip(s32 const c_stageClip, bool const c_releaseClip)
{
	if (uiVerifyf(c_stageClip >= 0 && c_stageClip < MAX_STAGE_CLIPS, "Invalid stageClip type passed (%d)", c_stageClip))
	{
		if (c_releaseClip && GetProject())
		{
			if (IsStageTextureReady(c_stageClip))
			{
				const s32 iTimelineIndex = GetTimelineIndexFromProjectIndex(PLAYHEAD_TYPE_VIDEO, (u32)ms_iStageClipThumbnailInUse[c_stageClip]);

				if (iTimelineIndex != -1)
				{
					if (!ms_ComplexObjectClips[iTimelineIndex].bUsesProjectTexture)
					{
						if ( !GetProject()->DoesClipHavePendingBespokeCapture( (u32)ms_iStageClipThumbnailInUse[c_stageClip] ) )
						{
							if (ms_iStageClipThumbnailToBeReleased[c_stageClip] != -1)
							{
								GetProject()->ReleaseClipThumbnail((u32)ms_iStageClipThumbnailToBeReleased[c_stageClip]);
							}

							ms_iStageClipThumbnailToBeReleased[c_stageClip] = ms_iStageClipThumbnailInUse[c_stageClip];
						}
					}
				}
			}

			if (HasStageTextureBeenRequested(c_stageClip))
			{
				const s32 iTimelineIndex = GetTimelineIndexFromProjectIndex(PLAYHEAD_TYPE_VIDEO, (u32)ms_iStageClipThumbnailRequested[c_stageClip]);

				if (iTimelineIndex != -1)
				{
					if (!ms_ComplexObjectClips[iTimelineIndex].bUsesProjectTexture)
					{
						if ( !GetProject()->DoesClipHavePendingBespokeCapture( (u32)ms_iStageClipThumbnailRequested[c_stageClip] ) )
						{
							if (ms_iStageClipThumbnailToBeReleased[c_stageClip] != -1)
							{
								GetProject()->ReleaseClipThumbnail((u32)ms_iStageClipThumbnailToBeReleased[c_stageClip]);
							}

							ms_iStageClipThumbnailToBeReleased[c_stageClip] = ms_iStageClipThumbnailRequested[c_stageClip];
						}
					}
				}
			}
		}


		ms_iStageClipThumbnailInUse[c_stageClip] = -1;
		ms_iStageClipThumbnailRequested[c_stageClip] = -1;

		s32 objectToUse = TIMELINE_OBJECT_STAGE;

		if (c_stageClip == STAGE_CLIP_NEXT)
		{
			objectToUse = TIMELINE_OBJECT_STAGE_NEXT;
		}
		else if (c_stageClip == STAGE_CLIP_PREV)
		{
			objectToUse = TIMELINE_OBJECT_STAGE_PREV;
		}

		if (ms_ComplexObject[objectToUse].IsActive())
		{
			if (CScaleformMgr::BeginMethodOnComplexObject( ms_ComplexObject[objectToUse].GetId(), SF_BASE_CLASS_VIDEO_EDITOR, "unloadComponent"))
			{
				CScaleformMgr::EndMethod();
			}
		}

		ms_renderOverlayOverCentreStageClip = false;

		// clear the text fields:
		if (c_stageClip == STAGE_CLIP_CENTER)
		{
			if (ms_ComplexObject[TIMELINE_OBJECT_STAGE_TEXT_LEFT].IsActive())
			{
				ms_ComplexObject[TIMELINE_OBJECT_STAGE_TEXT_LEFT].SetTextField(NULL);
			}

			if (ms_ComplexObject[TIMELINE_OBJECT_STAGE_TEXT_RIGHT].IsActive())
			{
				ms_ComplexObject[TIMELINE_OBJECT_STAGE_TEXT_RIGHT].SetTextField(NULL);
			}

			SetupStageHelpText();  // clear stage help text if we are not previewing (or update it if we are)
		}
	}
}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorTimeline::ClearAllClips
// PURPOSE:	removes all clips from the timeline
/////////////////////////////////////////////////////////////////////////////////////////
void CVideoEditorTimeline::ClearAllClips()
{
	UpdateClipsOutsideProjectTime();

	ms_TimelineThumbnailMgr.ReleaseAll();

	if (GetProject() && GetProject()->GetClipCount() > 0)
	{
		ClearAllStageClips(true);
		ReleaseAllStageClipThumbnailsIfNeeded();
	}
	
	for (s32 i = 0; i < ms_ComplexObjectClips.GetCount(); i++)
	{
		if (ms_ComplexObjectClips[i].object.IsActive())
		{
			ms_ComplexObjectClips[i].object.SetVisible(false);  // hacky fix for 2467314

			if (ms_ComplexObjectClips[i].type == PLAYHEAD_TYPE_VIDEO)
			{
				if (CScaleformMgr::BeginMethodOnComplexObject(ms_ComplexObjectClips[i].object.GetId(), SF_BASE_CLASS_VIDEO_EDITOR, "unloadComponent"))
				{
					CScaleformMgr::EndMethod();
				}
			}

			ms_ComplexObjectClips[i].object.Release();
		}
	}

	ms_ComplexObjectClips.Reset();

	SetTimeOnPlayhead(false);

	ClearProjectSpaceIndicator();
}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorTimeline::ClearAllAnchors
// PURPOSE:	removes all anchors from the timeline
/////////////////////////////////////////////////////////////////////////////////////////
void CVideoEditorTimeline::ClearAllAnchors()
{
	s32 i = 0;

	while (i < ms_ComplexObjectClips.GetCount())
	{
		if (ms_ComplexObjectClips[i].type == PLAYHEAD_TYPE_ANCHOR)
		{
			if (ms_ComplexObjectClips[i].object.IsActive())
			{
				ms_ComplexObjectClips[i].object.Release();
			}

			ms_ComplexObjectClips.Delete(i);
		}
		else
		{
			i++;  // only increment through array if we havnt deleted an index
		}
	}
}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorTimeline::SetupStageHelpText
// PURPOSE:	displays the relevant 'centre help' text on the stage
/////////////////////////////////////////////////////////////////////////////////////////
bool CVideoEditorTimeline::SetupStageHelpText()
{
	bool bTextApplied = false;

	if (CScaleformMgr::BeginMethodOnComplexObject( ms_ComplexObject[TIMELINE_OBJECT_STAGE_CONTAINER].GetId(), SF_BASE_CLASS_VIDEO_EDITOR, "SET_STAGE_TEXT"))
	{
		if (IsPreviewing())
		{
			CScaleformMgr::AddParamString(TheText.Get(IsPreviewingText() ? "VEUI_PLACE_ANOTHER_T" :  // text
														IsPreviewingAmbientTrack() ? "VEUI_PLACE_ANOTHER_AMB" : // audio
														IsPreviewingMusicTrack() ? "VEUI_PLACE_ANOTHER_A" : // audio
														"VEUI_PLACE_ANOTHER"), false);  // video (or failsafe)

			bTextApplied = true;
		}
		else
		{
			CScaleformMgr::AddParamString("", false);
		}

		CScaleformMgr::EndMethod();
	}

	return bTextApplied;
}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorTimeline::SetupStagePreviewClip
// PURPOSE:	sets up the preview clip onto the main preview stage area
/////////////////////////////////////////////////////////////////////////////////////////
bool CVideoEditorTimeline::SetupStagePreviewClip(u32 const c_positionMs)
{
	// default them all to off
	s32 projectIndex = -1;
	s32 projectIndexPrev = -1;
	s32 projectIndexNext = -1;

	u32 const c_totalClipTimeMs = (u32)GetProject()->GetTotalClipTimeMs();
	bool const c_overEndOfProjectTime = c_positionMs > c_totalClipTimeMs;
	// if over end of the project then we want the 'previous' stage clip to be the last one in the project and the centre one blank
	if (c_overEndOfProjectTime)
	{
		projectIndexPrev = GetProjectIndexNearestStartPos(PLAYHEAD_TYPE_VIDEO, c_totalClipTimeMs, -1);

		if (projectIndexPrev != -1 && ms_iStageClipThumbnailInUse[STAGE_CLIP_PREV] == projectIndexPrev)
		{
			return true;  // project indexes have not changed, so return true as success
		}
	}
	else
	{
		projectIndex = GetProjectIndexNearestStartPos(PLAYHEAD_TYPE_VIDEO, c_positionMs, -1);
		projectIndexPrev = (!IsPreviewing() && projectIndex > 0) ? (s32)projectIndex-1 : -1;
		projectIndexNext = (!IsPreviewing() && projectIndex < (u32)GetProject()->GetClipCount()-1) ? (s32)projectIndex+1 : -1;
	}

	bool success = false;

	if (c_overEndOfProjectTime)
	{
		success = true;
	}
	else
	{
		success = SetupIndividualStagePreviewClip(STAGE_CLIP_PREV, projectIndexPrev);
		success = success && SetupIndividualStagePreviewClip(STAGE_CLIP_CENTER, projectIndex);
		success = success && SetupIndividualStagePreviewClip(STAGE_CLIP_NEXT, projectIndexNext);
	}

	return (success);
}



bool CVideoEditorTimeline::SetupIndividualStagePreviewClip(s32 const c_stageClip, u32 const c_projectIndex)
{
	if (c_projectIndex == -1)
	{
		if (c_stageClip == STAGE_CLIP_CENTER)
		{
			ms_ComplexObject[TIMELINE_OBJECT_STAGE_CONTAINER_PREV].SetAlpha(0);
		}
		else if (c_stageClip == STAGE_CLIP_PREV)
		{
			ms_ComplexObject[TIMELINE_OBJECT_STAGE_CONTAINER_PREV].SetAlpha(0);
		}
		else if (c_stageClip == STAGE_CLIP_NEXT)
		{
			ms_ComplexObject[TIMELINE_OBJECT_STAGE_CONTAINER_NEXT].SetAlpha(0);
		}

		return true;
	}

	bool bSuccess = false;

	if (UpdateStageTexture(c_stageClip, c_projectIndex))
	{
		s32 objectToUse = TIMELINE_OBJECT_STAGE;

		if (c_stageClip == STAGE_CLIP_NEXT)
		{
			objectToUse = TIMELINE_OBJECT_STAGE_NEXT;
		}
		else if (c_stageClip == STAGE_CLIP_PREV)
		{
			objectToUse = TIMELINE_OBJECT_STAGE_PREV;
		}
		
		if (ms_ComplexObject[objectToUse].IsActive())
		{
			if ( (!IsPreviewing()) && ms_ComplexObjectClips.GetCount() > 0)  // only set up once we have some valid clips on the stage
			{
				if (IsStageTextureReady(c_stageClip))  // has it now loaded?
				{
					// it has loaded, set it up:

					atString nameString;
					GetProject()->getClipTextureName(c_projectIndex, nameString);


					if (CScaleformMgr::BeginMethodOnComplexObject(ms_ComplexObject[objectToUse].GetId(), SF_BASE_CLASS_VIDEO_EDITOR, "loadComponent"))
					{
						CScaleformMgr::AddParamString(nameString.c_str(), false);
						CScaleformMgr::AddParamString(nameString.c_str(), false);

						CScaleformMgr::EndMethod();
					}

					if (c_stageClip == STAGE_CLIP_CENTER)  // display text on the centre clip
					{
						nameString = GetProject()->GetClipName( c_projectIndex );
						if (ms_ComplexObject[TIMELINE_OBJECT_STAGE_TEXT_LEFT].IsActive())
						{
							ms_ComplexObject[TIMELINE_OBJECT_STAGE_TEXT_LEFT].SetTextField(nameString);
						}

						if (ms_ComplexObject[TIMELINE_OBJECT_STAGE_TEXT_RIGHT].IsActive())
						{
							const u32 uDuration = (u32)GetProject()->GetClipDurationTimeMs(c_projectIndex);
							SimpleString_32 TimeBuffer;

							CTextConversion::FormatMsTimeAsString( TimeBuffer.getWritableBuffer(), TimeBuffer.GetMaxByteCount(), uDuration );

							ms_ComplexObject[TIMELINE_OBJECT_STAGE_TEXT_RIGHT].SetTextField(TimeBuffer.getBuffer());
						}
					}

					bSuccess = true;
				}

				if (c_stageClip == STAGE_CLIP_CENTER)
				{
#if __BANK
					if (!bSuccess && !IsScrollingFast())
					{
						//
						// something went wrong, so display some helpful on-screen debug
						//
						const s32 iTimelineIndex = GetTimelineIndexFromProjectIndex(PLAYHEAD_TYPE_VIDEO, c_projectIndex);

						char cDurationText[128];

						if (iTimelineIndex != -1 && iTimelineIndex < ms_ComplexObjectClips.GetCount())
						{
							formatf(cDurationText,  "FAIL: State: %d (t=%d, p=%u, s=%d, l=%s, f=%s)",	(s32)ms_ComplexObjectClips[iTimelineIndex].m_state,
																												iTimelineIndex,
																												c_projectIndex,
																												ms_iStageClipThumbnailInUse[c_stageClip],
																												GetProject()->IsClipThumbnailLoaded( c_projectIndex ) ? "TRUE" : "FALSE",
																												IsRequestedStageTextureFailed(c_stageClip) ? "TRUE" : "FALSE",
																												NELEM(cDurationText));
						}
						else
						{
							formatf(cDurationText,  "FAIL: Nothing (t=%d, p=%u)", iTimelineIndex, c_projectIndex, NELEM(cDurationText));
						}
			
						if (ms_ComplexObject[TIMELINE_OBJECT_STAGE_TEXT_RIGHT].IsActive())
						{
							ms_ComplexObject[TIMELINE_OBJECT_STAGE_TEXT_RIGHT].SetTextField(cDurationText);
						}

						if (ms_ComplexObject[TIMELINE_OBJECT_STAGE_TEXT_LEFT].IsActive())
						{
							ms_ComplexObject[TIMELINE_OBJECT_STAGE_TEXT_LEFT].SetTextField(NULL);
						}
					}
#endif // __BANK
				}
			}
			else
			{
				if (c_stageClip == STAGE_CLIP_CENTER)
				{
					bSuccess = SetupStageHelpText();
				}
				else
				{
					bSuccess = true;
				}
			}
		}

		if (c_stageClip == STAGE_CLIP_CENTER)
		{
			if (bSuccess)
			{
				ms_ComplexObject[TIMELINE_OBJECT_STAGE_CONTAINER].SetAlpha(255);
			}

			ms_renderOverlayOverCentreStageClip = bSuccess;

#if __BANK
			if (!bSuccess && !IsScrollingFast())  // set alpha to 255 to ensure we always see debug info
			{
				ms_ComplexObject[TIMELINE_OBJECT_STAGE_CONTAINER].SetAlpha(255);
			}
#endif

		}

		else if (c_stageClip == STAGE_CLIP_PREV)
		{
			if (bSuccess)
			{
				ms_ComplexObject[TIMELINE_OBJECT_STAGE_CONTAINER_PREV].SetAlpha(255);
			}
		}
		else if (c_stageClip == STAGE_CLIP_NEXT)
		{
			if (bSuccess)
			{
				ms_ComplexObject[TIMELINE_OBJECT_STAGE_CONTAINER_NEXT].SetAlpha(255);
			}
		}
	}

	return bSuccess;
}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorTimeline::ReportTextureFailedToVideoClip
// PURPOSE:	informs scaleform that this texture failed
/////////////////////////////////////////////////////////////////////////////////////////
void CVideoEditorTimeline::ReportTextureFailedToVideoClip(const u32 c_projectIndex)
{
	const s32 iTimelineIndex = GetTimelineIndexFromProjectIndex(PLAYHEAD_TYPE_VIDEO, c_projectIndex);

	if (iTimelineIndex != -1)
	{
		SetTimelineObjectSpinner((u32)iTimelineIndex, false);

		if (CScaleformMgr::BeginMethodOnComplexObject(ms_ComplexObjectClips[iTimelineIndex].object.GetId(), SF_BASE_CLASS_VIDEO_EDITOR, "loadComponent"))
		{
			CScaleformMgr::AddParamString(VIDEO_EDITOR_UI_FILENAME, false);
			CScaleformMgr::AddParamString(VIDEO_EDITOR_LOAD_ERROR_ICON, false);
			CScaleformMgr::EndMethod();
		}
	}
};



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorTimeline::AddTextureToVideoClip
// PURPOSE:	applies the texture linked to the passed index into the videoclip object
/////////////////////////////////////////////////////////////////////////////////////////
void CVideoEditorTimeline::AddTextureToVideoClip(const u32 c_projectIndex)
{
	if (ms_TimelineThumbnailMgr.SetupTextureOnComplexObject(c_projectIndex))
	{
		const s32 iTimelineIndex = GetTimelineIndexFromProjectIndex(PLAYHEAD_TYPE_VIDEO, c_projectIndex);

		if (iTimelineIndex != 1)
		{
			SetTimelineObjectSpinner((u32)iTimelineIndex, false);
		}

		if (!ms_stagingClipNeedsUpdated)
		{
			ms_stagingClipNeedsUpdated = !SetupStagePreviewClip(GetPlayheadPositionMs());
		}

		if (c_projectIndex == 0)  // if 1st clip then auto-highlight the large preview
		{
			if (ms_ComplexObject[TIMELINE_OBJECT_STAGE].IsActive())
			{
				ms_ComplexObject[TIMELINE_OBJECT_STAGE].SetVisible(true);
			}
		}
	}
};



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorTimeline::SetTimelineObjectSpinner
// PURPOSE:	turns on/off a spinner on the timeline video object
/////////////////////////////////////////////////////////////////////////////////////////
void CVideoEditorTimeline::SetTimelineObjectSpinner(const u32 uTimelineIndex, const bool bOnOff)
{
	if (GetProject() && ms_ComplexObjectClips[uTimelineIndex].object.IsActive())
	{
		if (ms_ComplexObjectClips[uTimelineIndex].type == PLAYHEAD_TYPE_VIDEO)
		{
			CComplexObject spinnerObject = ms_ComplexObjectClips[uTimelineIndex].object.GetObject("SPINNER");

			if (spinnerObject.IsActive())
			{
				if (bOnOff)
				{
					const u32 c_projectIndex = GetProjectIndexFromTimelineIndex(uTimelineIndex);

					if (c_projectIndex < GetProject()->GetClipCount())
					{
#define WIDTH_OF_SPINNER_IN_PIXELS (24)  // use this instead of "getting" it from the scaleform object since it will have to attach it early each time if so
						bool const c_clipLongEnough = MsToPixels((u32)(GetProject()->GetClipDurationTimeMs(c_projectIndex))) > WIDTH_OF_SPINNER_IN_PIXELS;
						spinnerObject.SetVisible(c_clipLongEnough);

						if (c_clipLongEnough)
						{
							const float fPixelPos = MsToPixels((u32)(GetProject()->GetClipDurationTimeMs(c_projectIndex) * 0.5f));
							spinnerObject.SetXPositionInPixels(fPixelPos);
						}
					}
				}
				else
				{
					spinnerObject.SetVisible(false);
				}

				spinnerObject.Release();
			}
		}
	}
}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorTimeline::GetPosOfObject
// PURPOSE:	returns the X position of the passed object
/////////////////////////////////////////////////////////////////////////////////////////
float CVideoEditorTimeline::GetPosOfObject(const s32 iClipObjectId, const char *pLinkageName)
{
	float fPos = 0.0f;

	if (ms_ComplexObject[iClipObjectId].IsActive())
	{
		CComplexObject object = ms_ComplexObject[iClipObjectId].GetObject(pLinkageName);

		if (object.IsActive())
		{
			fPos =  object.GetPositionInPixels().y;

			object.Release();
		}
	}

	return fPos;
}



bool CVideoEditorTimeline::UpdateStageTexture(s32 const c_stageClip, u32 const c_projectIndex)
{
	if (uiVerifyf(c_stageClip >= 0 && c_stageClip < MAX_STAGE_CLIPS, "Invalid stageClip type passed (%d)", c_stageClip))
	{
		// clear the exising clip if its already been set up
		if (IsStageTextureReady(c_stageClip))
		{
			if (ms_iStageClipThumbnailInUse[c_stageClip] != (s32)c_projectIndex)
			{
				ClearStageClip(c_stageClip, true);

				return false;
			}
		}

		// clear any requested clip if it has changed
		if (HasStageTextureBeenRequested(c_stageClip))
		{
			if (ms_iStageClipThumbnailRequested[c_stageClip] != (s32)c_projectIndex)
			{
				ClearStageClip(c_stageClip, true);

				return false;
			}
		}
		
		if (HasStageTextureBeenRequested(c_stageClip))  // has it been requested already?
		{
			if (IsRequestedStageTextureLoaded(c_stageClip))  // has it loaded?
			{
				ms_iStageClipThumbnailInUse[c_stageClip] = c_projectIndex;  // done - use the index
				ms_iStageClipThumbnailRequested[c_stageClip] = -1;  // no longer requested

				return true;
			}

			if (IsRequestedStageTextureFailed(c_stageClip))  // has it failed to load?
			{
				ClearStageClip(c_stageClip, true);

				ms_iStageClipThumbnailInUse[c_stageClip] = -1;
				ms_iStageClipThumbnailRequested[c_stageClip] = -1;

				return true;
			}
		}
		else  // not requested yet
		{
			const s32 iTimelineIndex = GetTimelineIndexFromProjectIndex(PLAYHEAD_TYPE_VIDEO, c_projectIndex);

			if (iTimelineIndex != -1)
			{
				if (!ms_ComplexObjectClips[iTimelineIndex].bUsesProjectTexture)
				{
					if ( !GetProject()->DoesClipHavePendingBespokeCapture( c_projectIndex ) )
					{
						if (GetProject()->IsClipThumbnailLoaded(c_projectIndex))
						{
							ms_iStageClipThumbnailInUse[c_stageClip] = c_projectIndex;  // done - use the index
							ms_iStageClipThumbnailRequested[c_stageClip] = -1;
						}
						else if (GetProject()->IsClipThumbnailRequested(c_projectIndex))
						{
							ms_iStageClipThumbnailInUse[c_stageClip] = -1;
							ms_iStageClipThumbnailRequested[c_stageClip] = c_projectIndex;
						}
						else
						{
							if ( GetProject()->RequestClipThumbnail(c_projectIndex) )  // otherwise try to request it
							{
								ms_iStageClipThumbnailInUse[c_stageClip] = -1;
								ms_iStageClipThumbnailRequested[c_stageClip] = c_projectIndex;  // if it succeeded to request, store it
							}
						}
					}
				}
				else
				{
					ms_iStageClipThumbnailInUse[c_stageClip] = c_projectIndex;  // done - use the index
					ms_iStageClipThumbnailRequested[c_stageClip] = -1;
				}

				return true;
			}
		}
	}

	return false;
}



bool CVideoEditorTimeline::HasStageTextureBeenRequested(s32 const c_stageClip)
{
	if (uiVerifyf(c_stageClip >= 0 && c_stageClip < MAX_STAGE_CLIPS, "Invalid stageClip type passed (%d)", c_stageClip))
	{
		if (ms_iStageClipThumbnailRequested[c_stageClip] == -1)
		{
			return false;
		}

		return (GetProject()->IsClipThumbnailRequested((u32)ms_iStageClipThumbnailRequested[c_stageClip]));
	}

	return false;
}


bool CVideoEditorTimeline::IsRequestedStageTextureLoaded(s32 const c_stageClip)
{
	if (uiVerifyf(c_stageClip >= 0 && c_stageClip < MAX_STAGE_CLIPS, "Invalid stageClip type passed (%d)", c_stageClip))
	{
		if (ms_iStageClipThumbnailRequested[c_stageClip] == -1)
		{
			return false;
		}

		return (GetProject()->IsClipThumbnailLoaded((u32)ms_iStageClipThumbnailRequested[c_stageClip]));
	}

	return false;
}



bool CVideoEditorTimeline::IsStageTextureReady(s32 const c_stageClip)
{
	if (uiVerifyf(c_stageClip >= 0 && c_stageClip < MAX_STAGE_CLIPS, "Invalid stageClip type passed (%d)", c_stageClip))
	{
		if (ms_iStageClipThumbnailInUse[c_stageClip] == -1)
		{
			return false;
		}

		return (GetProject()->IsClipThumbnailLoaded((u32)ms_iStageClipThumbnailInUse[c_stageClip]));
	}

	return false;
}



bool CVideoEditorTimeline::IsRequestedStageTextureFailed(s32 const c_stageClip)
{
	if (uiVerifyf(c_stageClip >= 0 && c_stageClip < MAX_STAGE_CLIPS, "Invalid stageClip type passed (%d)", c_stageClip))
	{
		if (ms_iStageClipThumbnailRequested[c_stageClip] == -1)
		{
			return false;
		}

		return (GetProject()->HasClipThumbnailLoadFailed((u32)ms_iStageClipThumbnailRequested[c_stageClip]));
	}

	return false;
}


bool CVideoEditorTimeline::ShouldRenderOverlayOverCentreStageClip()
{
	return IsTimelineSelected() && ms_renderOverlayOverCentreStageClip;
}


/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorTimeline::AddVideo
// PURPOSE:	adds a video clip to the timeline and sets its position, background size,
//			mask etc
/////////////////////////////////////////////////////////////////////////////////////////
void CVideoEditorTimeline::AddVideo(const u32 c_projectIndex)
{
	if (ms_ComplexObject[TIMELINE_OBJECT_TIMELINE_CONTAINER].IsActive())
	{
		UpdateProjectSpaceIndicator();

		sTimelineItem *pNewTimelineObject = &ms_ComplexObjectClips.Grow();
		sTimelineItem *pNewOverviewBarObject = &ms_ComplexObjectClips.Grow();

		const u32 uClipLengthMs = (u32)GetProject()->GetClipDurationTimeMs(c_projectIndex);

		ms_ComplexObject[TIMELINE_OBJECT_PLAYHEAD_MARKER].SetVisible(true);

		// main timeline:
		pNewTimelineObject->uIndexToProject = c_projectIndex;
		pNewTimelineObject->type = PLAYHEAD_TYPE_VIDEO;
		pNewTimelineObject->object = (ms_ComplexObject[TIMELINE_OBJECT_TIMELINE_CONTAINER].AttachMovieClipInstance("VIDEO_CLIP", GetUniqueClipDepth()));
		pNewTimelineObject->m_state = THUMBNAIL_STATE_NEEDED;
		pNewTimelineObject->bUsesProjectTexture = (uClipLengthMs > (MIN_CLIP_SIZE_FOR_TEXTURE*MS_TO_PIXEL_SCALE_FACTOR));

		// overview:
		pNewOverviewBarObject->uIndexToProject = c_projectIndex;
		pNewOverviewBarObject->type = PLAYHEAD_TYPE_OVERVIEW_VIDEO;
		pNewOverviewBarObject->object = (ms_ComplexObject[TIMELINE_OBJECT_OVERVIEW_CLIP_CONTAINER].AttachMovieClipInstance("OVERVIEW_BAR", GetUniqueClipDepth()));
		pNewOverviewBarObject->bUsesProjectTexture = false;
		
		if (pNewTimelineObject->object.IsActive() && pNewOverviewBarObject->object.IsActive())
		{
			const u32 uStartTimeMs = (u32)GetProject()->GetTrimmedTimeToClipMs(c_projectIndex);
			Color32 const c_bgColour = GetColourForPlayheadType(uStartTimeMs, PLAYHEAD_TYPE_VIDEO, ALPHA_30_PERCENT, true );

			pNewTimelineObject->object.SetPositionInPixels(Vector2(MsToPixels(uStartTimeMs), GetPosOfObject(TIMELINE_OBJECT_TIMELINE_CONTAINER, "VIDEO_CLIP_PLACEHOLDER")));
			pNewOverviewBarObject->object.SetPositionInPixels(Vector2(MsToPixels(uStartTimeMs), OVERVIEW_BAR_YPOS_VIDEO));

			CComplexObject backgroundObject = pNewTimelineObject->object.GetObject("BACKGROUND");
			if (backgroundObject.IsActive())
			{
				backgroundObject.SetColour( c_bgColour );
				backgroundObject.SetWidth( MsToPixels(uClipLengthMs) - GAP_BETWEEN_BORDERS );
				backgroundObject.Release();
			}

			pNewOverviewBarObject->object.SetColour(c_bgColour);
			pNewOverviewBarObject->object.SetWidth( MsToPixels(uClipLengthMs) );

			CComplexObject imageObject = pNewTimelineObject->object.GetObject("imgContainer");

			float fThumbSize = 0.0f;
			if (imageObject.IsActive())
			{
				fThumbSize = imageObject.GetWidth();
				imageObject.Release();
			}

#define BORDER_WIDTH (8.0f)  // width of the border
			const float fTotalBorderSizeUsed = (GAP_BETWEEN_BORDERS + (BORDER_WIDTH * 2.0f));
			const float fFinalWidth = MsToPixels(uClipLengthMs) - fTotalBorderSizeUsed;  // both borders + the gap

			if (fFinalWidth < fThumbSize)
			{
				CComplexObject maskObject = pNewTimelineObject->object.GetObject("VIDEO_CLIP_MASK");
				if (maskObject.IsActive())
				{
					if ( MsToPixels(uClipLengthMs) < fTotalBorderSizeUsed )  // if too small, then hide completely
					{
						maskObject.SetWidth(0.0f);  // but doesnt for some reason, so set mask to 0.0f
					}
					else
					{
						maskObject.SetWidth(fFinalWidth);
					}

					maskObject.Release();
				}
			}

			AddTextureToVideoClip(c_projectIndex);
		}

		UpdateOverviewBar();
	}
}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorTimeline::UpdateProjectSpaceIndicator
// PURPOSE:	updates the project space indicator
/////////////////////////////////////////////////////////////////////////////////////////
void CVideoEditorTimeline::UpdateProjectSpaceIndicator()
{
	if (GetProject())
	{
		if (ms_ComplexObject[TIMELINE_OBJECT_TIMELINE_CONTAINER].IsActive())
		{
			CComplexObject cutOffPointObject = ms_ComplexObject[TIMELINE_OBJECT_TIMELINE_CONTAINER].GetObject("CUT_OFF_POINT");
			if (cutOffPointObject.IsActive())
			{
				u32 const c_totalTimelineTimeMs = (u32)GetProject()->GetTotalClipTimeMs();
				cutOffPointObject.SetXPositionInPixels(MsToPixels(c_totalTimelineTimeMs));
				cutOffPointObject.SetDepth(TIMELINE_DEFAULT_DEPTH_CUT_OFF);  // stick this cut off point at the default depth
				cutOffPointObject.SetVisible(c_totalTimelineTimeMs > 0);

				cutOffPointObject.Release();
			}
		}
	}
}


/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorTimeline::UpdateProjectMaxIndicator
// PURPOSE: red line at the 30 min max
/////////////////////////////////////////////////////////////////////////////////////////
void CVideoEditorTimeline::UpdateProjectMaxIndicator()
{
	if (GetProject())
	{
		if (ms_ComplexObject[TIMELINE_OBJECT_TIMELINE_CONTAINER].IsActive())
		{
			CComplexObject maxProjectIndicator = ms_ComplexObject[TIMELINE_OBJECT_TIMELINE_CONTAINER].GetObject("MAX_PROJECT_LENGTH");
			if (maxProjectIndicator.IsActive())
			{
				u32 const c_totalProjectTimeMs = (u32)GetProject()->GetMaxDurationMs();
				maxProjectIndicator.SetXPositionInPixels(MsToPixels(c_totalProjectTimeMs));
				maxProjectIndicator.SetColour(CHudColour::GetRGB(HUD_COLOUR_RED, ALPHA_100_PERCENT));
				maxProjectIndicator.SetDepth(TIMELINE_DEFAULT_DEPTH_CUT_OFF+1);  // stick this cut off point at the default depth
				maxProjectIndicator.SetVisible(true);

				maxProjectIndicator.Release();
			}
		}
	}
}


void CVideoEditorTimeline::ClearProjectSpaceIndicator()
{
	if (GetProject())
	{
		if (ms_ComplexObject[TIMELINE_OBJECT_TIMELINE_CONTAINER].IsActive())
		{
			CComplexObject cutOffPointObject = ms_ComplexObject[TIMELINE_OBJECT_TIMELINE_CONTAINER].GetObject("CUT_OFF_POINT");
			if (cutOffPointObject.IsActive())
			{
				cutOffPointObject.SetDepth(TIMELINE_DEFAULT_DEPTH_CUT_OFF);  // stick this cut off point at the default depth
				cutOffPointObject.SetVisible(false);

				cutOffPointObject.Release();
			}
		}
	}
}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorTimeline::AddGhostTrack
// PURPOSE:	adds the full start and duration of an ambient/audio track, which is used for a
//			'ghost track' as we are trimming
/////////////////////////////////////////////////////////////////////////////////////////
void CVideoEditorTimeline::AddGhostTrack(u32 const c_projectIndex)
{
	if (ms_ComplexObject[TIMELINE_OBJECT_TIMELINE_CONTAINER].IsActive())
	{
		sTimelineItem *pNewTimelineObject = &ms_ComplexObjectClips.Grow();

		pNewTimelineObject->uIndexToProject = c_projectIndex;
		pNewTimelineObject->type = PLAYHEAD_TYPE_GHOST;
		pNewTimelineObject->object = (ms_ComplexObject[TIMELINE_OBJECT_TIMELINE_CONTAINER].AttachMovieClipInstance(GetItemSelected() == PLAYHEAD_TYPE_AMBIENT ? "AMBIENT_CLIP" : "AUDIO_CLIP", TIMELINE_DEFAULT_DEPTH_GHOST));
		pNewTimelineObject->bUsesProjectTexture = false;

		if (pNewTimelineObject->object.IsActive())
		{
			ePLAYHEAD_TYPE const c_type = GetItemSelected();
#define ALPHA_GHOST_TRACK (40)

			s32 const c_currentOutsideTimeMs = GetItemSelected() == PLAYHEAD_TYPE_AMBIENT ? GetProject()->GetAmbientClip(c_projectIndex).getStartTimeMs() : GetProject()->GetMusicClip(c_projectIndex).getStartTimeMs();

			s32 minStartTimeMs = c_currentOutsideTimeMs;
			s32 maxDurationMs = GetItemSelected() == PLAYHEAD_TYPE_AMBIENT ? GetProject()->GetAmbientClip(c_projectIndex).getTrackTotalDurationMs() : GetProject()->GetMusicClip(c_projectIndex).getTrackTotalDurationMs();

			if (c_currentOutsideTimeMs < 0)  // cap time at 0 so adjust the max duration by the difference if below
			{
				minStartTimeMs = 0;
				maxDurationMs += c_currentOutsideTimeMs;
			}

			pNewTimelineObject->object.SetPositionInPixels(Vector2(MsToPixels(minStartTimeMs), GetPosOfObject(TIMELINE_OBJECT_TIMELINE_CONTAINER, c_type == PLAYHEAD_TYPE_AMBIENT ? "AMBIENT_CLIP_PLACEHOLDER" : "AUDIO_CLIP_PLACEHOLDER")));
			pNewTimelineObject->object.SetWidth(MsToPixels(maxDurationMs));
			pNewTimelineObject->object.SetColour(GetColourForPlayheadType(GetStartTimeOfClipMs(c_type, c_projectIndex), c_type, ALPHA_GHOST_TRACK, true));

			CComplexObject textFadeOutObject = pNewTimelineObject->object.GetObject("TEXT_FADE_OUT");

			if (textFadeOutObject.IsActive())
			{
				textFadeOutObject.SetColour(CHudColour::GetRGB(c_type == PLAYHEAD_TYPE_AMBIENT ? HUD_COLOUR_VIDEO_EDITOR_AMBIENT_FADEOUT : HUD_COLOUR_VIDEO_EDITOR_AUDIO_FADEOUT, ALPHA_100_PERCENT));
				textFadeOutObject.Release();
			}

			if (CScaleformMgr::BeginMethodOnComplexObject(pNewTimelineObject->object.GetId(), SF_BASE_CLASS_VIDEO_EDITOR, "SET_TEXT_WITH_WIDTH"))
			{
				CScaleformMgr::AddParamString("\0", false);

				CScaleformMgr::AddParamFloat(MsToPixels(maxDurationMs));

				CScaleformMgr::AddParamBool(false);

				CScaleformMgr::EndMethod();
			}

			CComplexObject backgroundObject = pNewTimelineObject->object.GetObject("BACKGROUND");

			if (backgroundObject.IsActive())
			{
				backgroundObject.SetVisible(false);
				backgroundObject.Release();
			}

			CComplexObject iconObject = pNewTimelineObject->object.GetObject("ICON");

			if (iconObject.IsActive())
			{
				iconObject.SetVisible(false);
				iconObject.Release();
			}
		}
	}
}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorTimeline::AddAmbient
// PURPOSE:	adds a audio clip to the timeline and sets its position, background size,
//			mask etc
/////////////////////////////////////////////////////////////////////////////////////////
void CVideoEditorTimeline::AddAmbient(const u32 c_projectIndex, CAmbientClipHandle const &c_clip)
{
	if (!c_clip.IsValid()) 
		return;

	if (ms_ComplexObject[TIMELINE_OBJECT_TIMELINE_CONTAINER].IsActive())
	{
		UpdateProjectSpaceIndicator();

		sTimelineItem *pNewTimelineObject = &ms_ComplexObjectClips.Grow();
		sTimelineItem *pNewOverviewBarObject = &ms_ComplexObjectClips.Grow();

		pNewTimelineObject->uIndexToProject = c_projectIndex;
		pNewTimelineObject->type = PLAYHEAD_TYPE_AMBIENT;
		pNewTimelineObject->object = (ms_ComplexObject[TIMELINE_OBJECT_TIMELINE_CONTAINER].AttachMovieClipInstance("AMBIENT_CLIP", GetUniqueClipDepth() ));
		pNewTimelineObject->bUsesProjectTexture = false;

		// overview:
		pNewOverviewBarObject->uIndexToProject = c_projectIndex;
		pNewOverviewBarObject->type = PLAYHEAD_TYPE_OVERVIEW_AMBIENT;
		pNewOverviewBarObject->object = (ms_ComplexObject[TIMELINE_OBJECT_OVERVIEW_CLIP_CONTAINER].AttachMovieClipInstance("OVERVIEW_BAR", GetUniqueClipDepth()));
		pNewOverviewBarObject->bUsesProjectTexture = false;

		CComplexObject backgroundHashObject = pNewTimelineObject->object.GetObject("BACKGROUND_HASH");  // ensure hash is invisible
		if (backgroundHashObject.IsActive())
		{
			backgroundHashObject.SetVisible(false);
			backgroundHashObject.Release();
		}

		if (pNewTimelineObject->object.IsActive())
		{
			Color32 const c_color = CHudColour::GetRGB(HUD_COLOUR_VIDEO_EDITOR_AMBIENT, ALPHA_30_PERCENT);

			const u32 uTrackStartTimeMs = c_clip.getStartTimeWithOffsetMs();
			const u32 uTrackLengthMs = c_clip.getTrimmedDurationMs();

			pNewTimelineObject->object.SetPositionInPixels(Vector2(MsToPixels(uTrackStartTimeMs), GetPosOfObject(TIMELINE_OBJECT_TIMELINE_CONTAINER, "AMBIENT_CLIP_PLACEHOLDER")));
			pNewOverviewBarObject->object.SetPositionInPixels(Vector2(MsToPixels(uTrackStartTimeMs), OVERVIEW_BAR_YPOS_AMBIENT));

			CComplexObject backgroundObject = pNewTimelineObject->object.GetObject("BACKGROUND");

			if (backgroundObject.IsActive())
			{
				backgroundObject.SetColour(c_color);
				backgroundObject.SetWidth(MsToPixels(uTrackLengthMs)-GAP_BETWEEN_BORDERS);
				backgroundObject.Release();
			}

			CComplexObject textFadeOutObject = pNewTimelineObject->object.GetObject("TEXT_FADE_OUT");

			if (textFadeOutObject.IsActive())
			{
				textFadeOutObject.SetColour(CHudColour::GetRGB(HUD_COLOUR_VIDEO_EDITOR_AMBIENT_FADEOUT, ALPHA_100_PERCENT));
				textFadeOutObject.Release();
			}

			CComplexObject iconObject = pNewTimelineObject->object.GetObject("ICON");

			if (iconObject.IsActive())
			{
				iconObject.SetVisible(c_clip.getTrimmedDurationMs() >= 3000);
				iconObject.Release();
			}

			pNewOverviewBarObject->object.SetColour(c_color);
			pNewOverviewBarObject->object.SetWidth( MsToPixels(uTrackLengthMs));

			LocalizationKey locKey;

			CVideoProjectAudioTrackProvider::GetMusicTrackNameKey( REPLAY_AMBIENT_TRACK, c_clip.getClipSoundHash(), locKey.getBufferRef() );

			if (CScaleformMgr::BeginMethodOnComplexObject(pNewTimelineObject->object.GetId(), SF_BASE_CLASS_VIDEO_EDITOR, "SET_TEXT_WITH_WIDTH"))
			{
				CScaleformMgr::AddParamString(TheText.Get(locKey.getBuffer()), false);

				CScaleformMgr::AddParamFloat(MsToPixels(uTrackLengthMs)-GAP_BETWEEN_BORDERS);

				CScaleformMgr::AddParamBool(true);

				CScaleformMgr::EndMethod();
			}

			UpdateOverviewBar();
		}
	}
}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorTimeline::AddAudio
// PURPOSE:	adds a audio clip to the timeline and sets its position, background size,
//			mask etc
/////////////////////////////////////////////////////////////////////////////////////////
void CVideoEditorTimeline::AddAudio(const u32 c_projectIndex, CMusicClipHandle const &MusicClip)
{
	if (!MusicClip.IsValid()) 
		return;

	if (ms_ComplexObject[TIMELINE_OBJECT_TIMELINE_CONTAINER].IsActive())
	{
		UpdateProjectSpaceIndicator();

		sTimelineItem *pNewTimelineObject = &ms_ComplexObjectClips.Grow();
		sTimelineItem *pNewOverviewBarObject = &ms_ComplexObjectClips.Grow();

		bool const c_isScoreTrack = audRadioTrack::IsScoreTrack(MusicClip.getClipSoundHash());

		pNewTimelineObject->uIndexToProject = c_projectIndex;
		pNewTimelineObject->type = PLAYHEAD_TYPE_AUDIO;
		pNewTimelineObject->object = (ms_ComplexObject[TIMELINE_OBJECT_TIMELINE_CONTAINER].AttachMovieClipInstance( (c_isScoreTrack ? "SCORE_CLIP" : "AUDIO_CLIP"), GetUniqueClipDepth() ));
		pNewTimelineObject->bUsesProjectTexture = false;

		// overview:
		pNewOverviewBarObject->uIndexToProject = c_projectIndex;
		pNewOverviewBarObject->type = PLAYHEAD_TYPE_OVERVIEW_AUDIO;
		pNewOverviewBarObject->object = (ms_ComplexObject[TIMELINE_OBJECT_OVERVIEW_CLIP_CONTAINER].AttachMovieClipInstance("OVERVIEW_BAR", GetUniqueClipDepth()));
		pNewOverviewBarObject->bUsesProjectTexture = false;

		CComplexObject backgroundHashObject = pNewTimelineObject->object.GetObject("BACKGROUND_HASH");  // ensure hash is invisible
		if (backgroundHashObject.IsActive())
		{
			backgroundHashObject.SetVisible(false);
			backgroundHashObject.Release();
		}

		if (pNewTimelineObject->object.IsActive())
		{
			Color32 const c_color = CHudColour::GetRGB(c_isScoreTrack ? HUD_COLOUR_VIDEO_EDITOR_SCORE : HUD_COLOUR_VIDEO_EDITOR_AUDIO, ALPHA_30_PERCENT);

			const u32 uTrackStartTimeMs = MusicClip.getStartTimeWithOffsetMs();
			const u32 uTrackLengthMs = MusicClip.getTrimmedDurationMs();

			pNewTimelineObject->object.SetPositionInPixels(Vector2(MsToPixels(uTrackStartTimeMs), GetPosOfObject(TIMELINE_OBJECT_TIMELINE_CONTAINER, "AUDIO_CLIP_PLACEHOLDER")));
			pNewOverviewBarObject->object.SetPositionInPixels(Vector2(MsToPixels(uTrackStartTimeMs), OVERVIEW_BAR_YPOS_AUDIO));

			CComplexObject backgroundObject = pNewTimelineObject->object.GetObject("BACKGROUND");

			if (backgroundObject.IsActive())
			{
				backgroundObject.SetColour(c_color);
				backgroundObject.SetWidth(MsToPixels(uTrackLengthMs)-GAP_BETWEEN_BORDERS);
				backgroundObject.Release();
			}

			CComplexObject textFadeOutObject = pNewTimelineObject->object.GetObject("TEXT_FADE_OUT");

			if (textFadeOutObject.IsActive())
			{
				textFadeOutObject.SetColour(CHudColour::GetRGB(c_isScoreTrack ? HUD_COLOUR_VIDEO_EDITOR_SCORE_FADEOUT : HUD_COLOUR_VIDEO_EDITOR_AUDIO_FADEOUT, ALPHA_100_PERCENT));
				textFadeOutObject.Release();
			}

			CComplexObject iconObject = pNewTimelineObject->object.GetObject("ICON");

			if (iconObject.IsActive())
			{
				iconObject.SetVisible(MusicClip.getTrimmedDurationMs() >= 3000);
				iconObject.Release();
			}

			pNewOverviewBarObject->object.SetColour(c_color);
			pNewOverviewBarObject->object.SetWidth( MsToPixels(uTrackLengthMs));

			LocalizationKey locKey;
			CVideoProjectAudioTrackProvider::GetMusicTrackNameKey( c_isScoreTrack ? REPLAY_SCORE_MUSIC : REPLAY_RADIO_MUSIC, MusicClip.getClipSoundHash(), locKey.getBufferRef() );

			if (CScaleformMgr::BeginMethodOnComplexObject(pNewTimelineObject->object.GetId(), SF_BASE_CLASS_VIDEO_EDITOR, "SET_TEXT_WITH_WIDTH"))
			{
				CScaleformMgr::AddParamString(TheText.Get(locKey.getBuffer()), false);

				CScaleformMgr::AddParamFloat(MsToPixels(uTrackLengthMs)-GAP_BETWEEN_BORDERS);

				CScaleformMgr::AddParamBool(true);

				CScaleformMgr::EndMethod();
			}

			UpdateOverviewBar();
		}
	}
}

bool CVideoEditorTimeline::UpdateAdjustableTextProperties(sTimelineItem *pTimelineObject, sTimelineItem *pOverviewBarObject, CTextOverlayHandle const &OverlayText)
{
	if (pTimelineObject && pOverviewBarObject)
	{
		u32 const startTimeMs = OverlayText.getStartTimeMs();

		pTimelineObject->object.SetPositionInPixels(Vector2(MsToPixels(startTimeMs), GetPosOfObject(TIMELINE_OBJECT_TIMELINE_CONTAINER, "TEXT_CLIP_PLACEHOLDER")));
		pOverviewBarObject->object.SetXPositionInPixels(MsToPixels(startTimeMs));

		CComplexObject backgroundObject = pTimelineObject->object.GetObject("BACKGROUND");

		if (backgroundObject.IsActive())
		{
			backgroundObject.SetWidth(MsToPixels(OverlayText.getDurationMs()));

			CComplexObject iconObject = pTimelineObject->object.GetObject("ICON");

			if (iconObject.IsActive())
			{
				iconObject.SetVisible(!ms_trimmingClip && OverlayText.getDurationMs() >= 3000);
				iconObject.Release();
			}

			if (CScaleformMgr::BeginMethodOnComplexObject(pTimelineObject->object.GetId(), SF_BASE_CLASS_VIDEO_EDITOR, "SET_TEXT_WITH_WIDTH"))
			{
				CScaleformMgr::AddParamString(!ms_trimmingClip ? OverlayText.getText(0) : "\0", false);
				CScaleformMgr::AddParamFloat(MsToPixels(OverlayText.getDurationMs())-GAP_BETWEEN_BORDERS);

				CScaleformMgr::AddParamBool(!ms_trimmingClip);

				CScaleformMgr::EndMethod();
			}

			pOverviewBarObject->object.SetWidth(MsToPixels(OverlayText.getDurationMs()));

			backgroundObject.Release();

			return true;
		}
	}

	return false;
}



bool CVideoEditorTimeline::UpdateAdjustableAmbientProperties(sTimelineItem *pTimelineObject, sTimelineItem *pOverviewBarObject, CAmbientClipHandle const &clip)
{
	if (pTimelineObject && pOverviewBarObject)
	{
		u32 const startTimeMs = clip.getStartTimeWithOffsetMs();

		pTimelineObject->object.SetPositionInPixels(Vector2(MsToPixels(startTimeMs), GetPosOfObject(TIMELINE_OBJECT_TIMELINE_CONTAINER, "AMBIENT_CLIP_PLACEHOLDER")));
		pOverviewBarObject->object.SetXPositionInPixels(MsToPixels(startTimeMs));

		CComplexObject backgroundObject = pTimelineObject->object.GetObject("BACKGROUND");

		if (backgroundObject.IsActive())
		{
			backgroundObject.SetWidth(MsToPixels(clip.getTrimmedDurationMs())-GAP_BETWEEN_BORDERS);

			LocalizationKey locKey;
			CVideoProjectAudioTrackProvider::GetMusicTrackNameKey( REPLAY_AMBIENT_TRACK, clip.getClipSoundHash(), locKey.getBufferRef() );

			CComplexObject iconObject = pTimelineObject->object.GetObject("ICON");

			if (iconObject.IsActive())
			{
				iconObject.SetVisible(!ms_trimmingClip && clip.getTrimmedDurationMs() >= 3000);
				iconObject.Release();
			}

			if (CScaleformMgr::BeginMethodOnComplexObject(pTimelineObject->object.GetId(), SF_BASE_CLASS_VIDEO_EDITOR, "SET_TEXT_WITH_WIDTH"))
			{
				CScaleformMgr::AddParamString(!ms_trimmingClip ? TheText.Get(locKey.getBuffer()) : "\0", false);

				const u32 uTrackLengthMs = clip.getTrimmedDurationMs();
				CScaleformMgr::AddParamFloat(MsToPixels(uTrackLengthMs)-GAP_BETWEEN_BORDERS);

				CScaleformMgr::AddParamBool(!ms_trimmingClip);

				CScaleformMgr::EndMethod();
			}

			backgroundObject.Release();

			return true;
		}
	}

	return false;
}



bool CVideoEditorTimeline::UpdateAdjustableAudioProperties(sTimelineItem *pTimelineObject, sTimelineItem *pOverviewBarObject, CMusicClipHandle const &music)
{
	if (pTimelineObject && pOverviewBarObject)
	{
		u32 const startTimeMs = music.getStartTimeWithOffsetMs();

		pTimelineObject->object.SetPositionInPixels(Vector2(MsToPixels(startTimeMs), GetPosOfObject(TIMELINE_OBJECT_TIMELINE_CONTAINER, "AUDIO_CLIP_PLACEHOLDER")));
		pOverviewBarObject->object.SetXPositionInPixels(MsToPixels(startTimeMs));

		CComplexObject backgroundObject = pTimelineObject->object.GetObject("BACKGROUND");

		if (backgroundObject.IsActive())
		{
			backgroundObject.SetWidth(MsToPixels(music.getTrimmedDurationMs())-GAP_BETWEEN_BORDERS);

			bool const c_isScoreTrack = audRadioTrack::IsScoreTrack(music.getClipSoundHash());

			LocalizationKey locKey;
			CVideoProjectAudioTrackProvider::GetMusicTrackNameKey( c_isScoreTrack ? REPLAY_SCORE_MUSIC : REPLAY_RADIO_MUSIC, music.getClipSoundHash(), locKey.getBufferRef() );

			CComplexObject iconObject = pTimelineObject->object.GetObject("ICON");

			if (iconObject.IsActive())
			{
				iconObject.SetVisible(!ms_trimmingClip && music.getTrimmedDurationMs() >= 3000);
				iconObject.Release();
			}

			if (CScaleformMgr::BeginMethodOnComplexObject(pTimelineObject->object.GetId(), SF_BASE_CLASS_VIDEO_EDITOR, "SET_TEXT_WITH_WIDTH"))
			{
				CScaleformMgr::AddParamString(!ms_trimmingClip ? TheText.Get(locKey.getBuffer()) : "\0", false);

				const u32 uTrackLengthMs = music.getTrimmedDurationMs();
				CScaleformMgr::AddParamFloat(MsToPixels(uTrackLengthMs)-GAP_BETWEEN_BORDERS);

				CScaleformMgr::AddParamBool(!ms_trimmingClip);

				CScaleformMgr::EndMethod();
			}

			backgroundObject.Release();

			return true;
		}
	}

	return false;
}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorTimeline::AddText
// PURPOSE:	adds a text clip to the timeline and sets its position, background size,
//			mask etc
/////////////////////////////////////////////////////////////////////////////////////////
void CVideoEditorTimeline::AddText(const u32 c_projectIndex, CTextOverlayHandle const &OverlayText)
{
	if (!OverlayText.IsValid())
		return;
	
	if (ms_ComplexObject[TIMELINE_OBJECT_TIMELINE_CONTAINER].IsActive())
	{
		UpdateProjectSpaceIndicator();

		sTimelineItem *pNewTimelineObject = &ms_ComplexObjectClips.Grow();
		sTimelineItem *pNewOverviewBarObject = &ms_ComplexObjectClips.Grow();

		pNewTimelineObject->uIndexToProject = c_projectIndex;
		pNewTimelineObject->type = PLAYHEAD_TYPE_TEXT;
		pNewTimelineObject->object = (ms_ComplexObject[TIMELINE_OBJECT_TIMELINE_CONTAINER].AttachMovieClipInstance("TEXT_CLIP", GetUniqueClipDepth()));
		pNewTimelineObject->bUsesProjectTexture = false;

		// overview:
		pNewOverviewBarObject->uIndexToProject = c_projectIndex;
		pNewOverviewBarObject->type = PLAYHEAD_TYPE_OVERVIEW_TEXT;
		pNewOverviewBarObject->object = (ms_ComplexObject[TIMELINE_OBJECT_OVERVIEW_CLIP_CONTAINER].AttachMovieClipInstance("OVERVIEW_BAR", GetUniqueClipDepth()));
		pNewOverviewBarObject->bUsesProjectTexture = false;

		if (pNewTimelineObject->object.IsActive())
		{
			u32 const startTimeMs = OverlayText.getStartTimeMs();

			pNewTimelineObject->object.SetPositionInPixels(Vector2(MsToPixels(startTimeMs), GetPosOfObject(TIMELINE_OBJECT_TIMELINE_CONTAINER, "TEXT_CLIP_PLACEHOLDER")));
			pNewOverviewBarObject->object.SetPositionInPixels(Vector2(MsToPixels(startTimeMs), OVERVIEW_BAR_YPOS_TEXT));

			CComplexObject backgroundObject = pNewTimelineObject->object.GetObject("BACKGROUND");

			if (backgroundObject.IsActive())
			{
				backgroundObject.SetColour(GetColourForPlayheadType(startTimeMs, PLAYHEAD_TYPE_TEXT, ALPHA_30_PERCENT, true));
				backgroundObject.SetWidth(MsToPixels(OverlayText.getDurationMs()));

				CComplexObject iconObject = pNewTimelineObject->object.GetObject("ICON");

				if (iconObject.IsActive())
				{
					iconObject.SetVisible(OverlayText.getDurationMs() >= 3000);
					iconObject.Release();
				}

				if (CScaleformMgr::BeginMethodOnComplexObject(pNewTimelineObject->object.GetId(), SF_BASE_CLASS_VIDEO_EDITOR, "SET_TEXT_WITH_WIDTH"))
				{
					CScaleformMgr::AddParamString(OverlayText.getText(0), false);
					CScaleformMgr::AddParamFloat(MsToPixels(OverlayText.getDurationMs())-GAP_BETWEEN_BORDERS);

					CScaleformMgr::AddParamBool(true);

					CScaleformMgr::EndMethod();
				}

				pNewOverviewBarObject->object.SetColour(GetColourForPlayheadType(startTimeMs, PLAYHEAD_TYPE_TEXT, ALPHA_30_PERCENT, true));
				pNewOverviewBarObject->object.SetWidth(MsToPixels(OverlayText.getDurationMs())-GAP_BETWEEN_BORDERS);

				backgroundObject.Release();
			}

			CComplexObject textFadeOutObject = pNewTimelineObject->object.GetObject("TEXT_FADE_OUT");

			if (textFadeOutObject.IsActive())
			{
				textFadeOutObject.SetColour(CHudColour::GetRGB(HUD_COLOUR_VIDEO_EDITOR_TEXT_FADEOUT, ALPHA_100_PERCENT));
				textFadeOutObject.Release();
			}
		}

		UpdateOverviewBar();
	}
}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorTimeline::AddAnchor
// PURPOSE:	adds one anchor to the timeline
/////////////////////////////////////////////////////////////////////////////////////////
void CVideoEditorTimeline::AddAnchor(const u32 c_projectIndex, const u32 uTimeMs)
{
	if (ms_ComplexObject[TIMELINE_OBJECT_TIMELINE_CONTAINER].IsActive())
	{
		sTimelineItem *pNewClipObject = &ms_ComplexObjectClips.Grow();

		pNewClipObject->uIndexToProject = c_projectIndex;
		pNewClipObject->type = PLAYHEAD_TYPE_ANCHOR;
		pNewClipObject->object = (ms_ComplexObject[TIMELINE_OBJECT_TIMELINE_CONTAINER].AttachMovieClipInstance("ANCHOR", GetUniqueClipDepth()));
		pNewClipObject->bUsesProjectTexture = false;

		if (pNewClipObject->object.IsActive())
		{
			pNewClipObject->object.SetXPositionInPixels(MsToPixels(uTimeMs));

			pNewClipObject->object.SetColour(CHudColour::GetRGBA(HUD_COLOUR_YELLOW));

			// get the 1st video clip (this should always exist if we are building markers)
			const s32 iTimelineIndexOfFirstClip = CVideoEditorTimeline::GetTimelineIndexFromProjectIndex(PLAYHEAD_TYPE_VIDEO, 0);

			if (uiVerifyf(iTimelineIndexOfFirstClip != -1, "Could not find any video clip on the timeline!"))
			{
				if (ms_ComplexObjectClips[iTimelineIndexOfFirstClip].object.IsActive())
				{
					const float fHeight = ms_ComplexObjectClips[iTimelineIndexOfFirstClip].object.GetHeight();
					pNewClipObject->object.SetHeight(fHeight);
				}
			}
		}
	}
}



//////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorTimeline::DeleteClip
// PURPOSE:	deletes a clip and its associated complex object from the timeline array
//////////////////////////////////////////////////////////////////////////////////////////
void CVideoEditorTimeline::DeleteClip(const u32 uTimelineIndex)
{
	if (uTimelineIndex < ms_ComplexObjectClips.GetCount())
	{
		if (ms_ComplexObjectClips[uTimelineIndex].object.IsActive())
		{
			ms_ComplexObjectClips[uTimelineIndex].object.SetVisible(false);

			if (ms_ComplexObjectClips[uTimelineIndex].type == PLAYHEAD_TYPE_VIDEO && ms_ComplexObjectClips[uTimelineIndex].m_state == THUMBNAIL_STATE_IN_USE)
			{
				if (CScaleformMgr::BeginMethodOnComplexObject(ms_ComplexObjectClips[uTimelineIndex].object.GetId(), SF_BASE_CLASS_VIDEO_EDITOR, "unloadComponent"))
				{
					CScaleformMgr::EndMethod();
				}
			}

			ms_ComplexObjectClips[uTimelineIndex].object.Release();
		}

		ms_ComplexObjectClips.Delete(uTimelineIndex);
	}

	SetTimeOnPlayhead(CVideoEditorUi::IsProjectPopulated());
}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorTimeline::UpdateClipsOutsideProjectTime
// PURPOSE:	updates anything we want to cull outside of the video project time
/////////////////////////////////////////////////////////////////////////////////////////
void CVideoEditorTimeline::UpdateClipsOutsideProjectTime()
{
	if (GetProject())
	{
		if (ms_ComplexObjectClips.GetCount() > 0)  // only process if we have a valid project and have any clips aready setup on the timeline
		{
			RebuildAnchors();
		}

		if (IsTimelineSelected())
		{
			SetTimeOnPlayhead(CVideoEditorUi::IsProjectPopulated());
		}
	}
}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorTimeline::GetUniqueClipDepth
// PURPOSE:	gets a unique depth to use as the new clip attached to the timeline
/////////////////////////////////////////////////////////////////////////////////////////
u32 CVideoEditorTimeline::GetUniqueClipDepth()
{
	ms_uClipAttachDepth++;

	if (ms_uClipAttachDepth > TIMELINE_DEFAULT_DEPTH_END)  // ensure we reset this eventually to keep the number reasonable
		ms_uClipAttachDepth = TIMELINE_DEFAULT_DEPTH_START;

	 return ms_uClipAttachDepth;
}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorTimeline::UpdateInstructionalButtons
// PURPOSE:	updates the instructional buttons
/////////////////////////////////////////////////////////////////////////////////////////
void CVideoEditorTimeline::UpdateInstructionalButtons( CScaleformMovieWrapper& buttonsMovie )
{
	if (!GetProject())
	{
		return;
	}

	if( buttonsMovie.IsActive() )
	{
		const bool c_mouseHeldThisFrame = RSG_PC && ((ioMouse::GetButtons() & ioMouse::MOUSE_LEFT) != 0);
		bool const c_wasKbmLastFrame =  CVideoEditorInstructionalButtons::LatestInstructionsAreKeyboardMouse();

		if (ms_previewingAudioTrack != -1)
		{
			CVideoEditorInstructionalButtons::AddButton(INPUT_REPLAY_PREVIEW_AUDIO, TheText.Get("IB_STOP_TRACK"), true);

			return;
		}

		if (ms_trimmingClip)
		{
			if (!c_mouseHeldThisFrame)
			{
				CVideoEditorInstructionalButtons::AddButton(INPUT_FRONTEND_RIGHT_AXIS_X, TheText.Get("IB_MANUAL_TRIM"), true);
			}

			CVideoEditorInstructionalButtons::AddButton(INPUTGROUP_FRONTEND_DPAD_LR, TheText.Get("IB_SNAP_TRIM"), true);

			return;
		}

		if (CControlMgr::GetVirtualKeyboardState() == ioVirtualKeyboard::kKBState_PENDING)
		{
			CVideoEditorInstructionalButtons::AddButton(INPUT_FRONTEND_ACCEPT, TheText.Get("IB_CONFIRM"), true);
			CVideoEditorInstructionalButtons::AddButton(INPUT_FRONTEND_CANCEL, TheText.Get("IB_CANCEL"), true);
		}
		else
		{
			bool const c_overEndOfProjectTime = GetPlayheadPositionMs() > GetProject()->GetTotalClipTimeMs();
			bool bHaveAtleastOneClip = false;
			bool bHaveMultipleClips = false;
			bool bCanDuplicate = false;
		
			switch (GetItemSelected())
			{
				case PLAYHEAD_TYPE_VIDEO:
				{
					bHaveAtleastOneClip = (GetProject()->GetClipCount() > 0);
					bHaveMultipleClips = (GetProject()->GetClipCount() > 1);
					bCanDuplicate = GetProject()->CanAddClip();
					break;
				}

				case PLAYHEAD_TYPE_AMBIENT:
				{
					bHaveAtleastOneClip = bHaveMultipleClips = (GetProject()->GetAmbientClipCount() > 0);
					bCanDuplicate = GetProject()->CanAddAmbient();

					break;
				}

				case PLAYHEAD_TYPE_AUDIO:
				{
					bHaveAtleastOneClip = bHaveMultipleClips = (GetProject()->GetMusicClipCount() > 0);
					bCanDuplicate = IsPlayheadAudioScore(GetPlayheadPositionMs(), PLAYHEAD_TYPE_AUDIO) && GetProject()->CanAddMusic();

					break;
				}

				case PLAYHEAD_TYPE_TEXT:
				{
					bHaveAtleastOneClip = bHaveMultipleClips = (GetProject()->GetOverlayTextCount() > 0);
					bCanDuplicate = GetProject()->CanAddText();
					break;
				}

				default:
				{
					uiAssertf(0, "CVideoEditorTimeline::UpdateInstructionalButtons() - invalid type (%d)", (s32)GetItemSelected());
				}
			}

			if (IsPreviewing())
			{
				if (c_wasKbmLastFrame)
				{
					CVideoEditorInstructionalButtons::AddButtonCombo(INPUT_REPLAY_CTRL, INPUT_REPLAY_TIMELINE_PLACE_CLIP, TheText.Get("IB_PLACE"));
				}
				else
				{
					CVideoEditorInstructionalButtons::AddButton(INPUT_FRONTEND_ACCEPT, TheText.Get("IB_PLACE"), false);
				}

				CVideoEditorInstructionalButtons::AddButton(INPUT_FRONTEND_CANCEL, TheText.Get("IB_CANCEL"), false);

				if (c_wasKbmLastFrame)
				{
					CVideoEditorInstructionalButtons::AddButton(INPUTGROUP_CURSOR_SCROLL, TheText.Get("IB_PLAYHEAD"), false);
				}
				else
				{
					CVideoEditorInstructionalButtons::AddButton(INPUT_FRONTEND_AXIS_X, TheText.Get("IB_PLAYHEAD"), false);
				}

				if (IsPreviewingNonVideo())
				{
					CVideoEditorInstructionalButtons::AddButton(INPUTGROUP_FRONTEND_DPAD_LR, TheText.Get("IB_SNAP"), false);
				}
			}
			else
			{
				if (!c_overEndOfProjectTime && bHaveAtleastOneClip)
				{
					if (!IsTimelineItemAudioOrAmbient(GetItemSelected()))  // no 'edit' when audio is selected
					{
						if (GetItemSelected() != PLAYHEAD_TYPE_TEXT || GetProject()->GetClipCount() > 0) 
						{
							CVideoEditorInstructionalButtons::AddButton(INPUT_FRONTEND_ACCEPT, TheText.Get("IB_EDIT"), true);
						}
					}
				}

				InputType const toggleInput = c_wasKbmLastFrame ? INPUT_REPLAY_TOGGLE_TIMELINE : INPUT_FRONTEND_CANCEL;
				CVideoEditorInstructionalButtons::AddButton(toggleInput, TheText.Get("IB_PROJ_MENU"), true);

				if (bHaveAtleastOneClip)
				{
					InputType const deleteInput = c_wasKbmLastFrame ? INPUT_REPLAY_CLIP_DELETE : INPUT_FRONTEND_Y;
					CVideoEditorInstructionalButtons::AddButton( deleteInput, TheText.Get("IB_DELETE"), true);

					if (IsTimelineItemNonVideo(GetItemSelected()))
					{
						CVideoEditorInstructionalButtons::AddButton( INPUT_FRONTEND_RB, INPUT_FRONTEND_LB, TheText.Get("IB_TRIM_HOLD"));
					}

					if (IsTimelineItemAudioOrAmbient(GetItemSelected()) && ms_previewingAudioTrack == -1)
					{
						CVideoEditorInstructionalButtons::AddButton(INPUT_REPLAY_PREVIEW_AUDIO, TheText.Get("IB_PLAY_TRACK"), true);
					}
				}

				//! NOTE! Don't move this to the above if block. We need to ensure this comes after the back button due to
				//! ordering on the instructional buttons
				if( bHaveAtleastOneClip && bCanDuplicate )
				{
					if (c_wasKbmLastFrame)
					{
						CVideoEditorInstructionalButtons::AddButtonCombo(INPUT_REPLAY_CTRL, INPUT_REPLAY_TIMELINE_DUPLICATE_CLIP, TheText.Get("IB_DUPLICATE"));
					}
					else
					{
						CVideoEditorInstructionalButtons::AddButton(INPUT_FRONTEND_LS, TheText.Get("IB_DUPLICATE"), true);
					}
				}

				if (bHaveMultipleClips)
				{
					if (c_wasKbmLastFrame)
					{
						CVideoEditorInstructionalButtons::AddButtonCombo(INPUT_REPLAY_CTRL, INPUT_REPLAY_TIMELINE_PICKUP_CLIP, TheText.Get("IB_PICKUP"));
					}
					else
					{
						CVideoEditorInstructionalButtons::AddButton(INPUT_FRONTEND_X, TheText.Get("IB_PICKUP"), true);
					}
				}

				if (bHaveMultipleClips)
				{
					if (c_wasKbmLastFrame)
					{
						CVideoEditorInstructionalButtons::AddButton(INPUTGROUP_CURSOR_SCROLL, TheText.Get("IB_PLAYHEAD"), false);
					}
					else
					{
						CVideoEditorInstructionalButtons::AddButton(INPUT_FRONTEND_AXIS_X, TheText.Get("IB_PLAYHEAD"), false);
					}
				}
			}
		}
	}
}



s32 CVideoEditorTimeline::FindTimelineIndexFromHashName(const ePLAYHEAD_TYPE type, atHashString nameHash)
{
	for (u32 i = 0; i < ms_ComplexObjectClips.GetCount(); i++)  // go through full list
	{
		if (ms_ComplexObjectClips[i].object.IsActive())  // active
		{
			if (ms_ComplexObjectClips[i].type == type)
			{
				if (nameHash == ms_ComplexObjectClips[i].object.GetObjectNameHash())
				{
					return i;
				}
			}
		}
	}

	return -1;
}



#if RSG_PC
void CVideoEditorTimeline::UpdateMouseEvent(const GFxValue* args)
{
	if (ms_previewingAudioTrack != -1)  // mo mouse events during audio preview playback
	{
		return;
	}

	if (uiVerifyf(args[2].IsNumber() && args[3].IsNumber() && args[4].IsString(), "MOUSE_EVENT params not compatible: %s %s %s", sfScaleformManager::GetTypeName(args[2]), sfScaleformManager::GetTypeName(args[3]), sfScaleformManager::GetTypeName(args[4])))
	{
		const s32 c_buttonType = (s32)args[2].GetNumber();  // button type
		const s32 c_eventType = (s32)args[3].GetNumber();  // item type
		const atHashString c_nameHash = atHashString(args[4].GetString());  // movieclip instance name

		//
		// check for click (press) on the stage area:
		//
		if (c_buttonType == BUTTON_TYPE_STAGE)
		{
			if (!IsPreviewing() && c_eventType == EVENT_TYPE_MOUSE_PRESS)
			{
				if (GetItemSelected() != PLAYHEAD_TYPE_VIDEO)  // go back to video selection if we click on stage clips
				{
					SetItemSelected(PLAYHEAD_TYPE_VIDEO);
					UpdatePlayheadPreview(PLAYHEAD_TYPE_NONE);
				}

				// move to previous clip on timeline:
				if (c_nameHash == ms_ComplexObject[TIMELINE_OBJECT_STAGE_CONTAINER_PREV].GetObjectNameHash())
				{
					ms_iScrollTimeline = -1;
				}

				// move to next clip on timeline:
				if (c_nameHash == ms_ComplexObject[TIMELINE_OBJECT_STAGE_CONTAINER_NEXT].GetObjectNameHash())
				{
					ms_iScrollTimeline = 1;
				}

				// edit the current clip on the timeline:
				if (c_nameHash == ms_ComplexObject[TIMELINE_OBJECT_STAGE_CONTAINER].GetObjectNameHash())
				{
					const s32 iProjectIndexAtPlayhead = GetProjectIndexAtTime(PLAYHEAD_TYPE_VIDEO, GetPlayheadPositionMs());

					if (iProjectIndexAtPlayhead != -1)
					{
						SetItemSelected(PLAYHEAD_TYPE_VIDEO);
						UpdatePlayheadPreview(PLAYHEAD_TYPE_NONE);

						// edit video clip:
						ms_iProjectPositionToRevertOnUpMs = ms_iProjectPositionToRevertOnDownMs = -1;  // we moved so reset
						CVideoEditorMenu::SetUserConfirmationScreenWithAction("TRIGGER_EDIT_CLIP", "VE_EDIT_CLIP_SURE");
					}

				}
			}

			return;
		}


		//
		// check for click (press) on the stage area:
		//
		if (c_buttonType == BUTTON_TYPE_TIMELINE_ARROWS)
		{
			if (!IsPreviewing())
			{
				if (c_eventType == EVENT_TYPE_MOUSE_ROLL_OVER)
				{
					if (c_nameHash == ms_ComplexObject[TIMELINE_OBJECT_LEFT_ARROW].GetObjectNameHash())
					{
						ms_ComplexObject[TIMELINE_OBJECT_LEFT_ARROW].SetAlpha(ALPHA_100_PERCENT);
					}

					if (c_nameHash == ms_ComplexObject[TIMELINE_OBJECT_RIGHT_ARROW].GetObjectNameHash())
					{
						ms_ComplexObject[TIMELINE_OBJECT_RIGHT_ARROW].SetAlpha(ALPHA_100_PERCENT);
					}
				}

				if (c_eventType == EVENT_TYPE_MOUSE_ROLL_OUT)
				{
					if (c_nameHash == ms_ComplexObject[TIMELINE_OBJECT_LEFT_ARROW].GetObjectNameHash())
					{
						ms_ComplexObject[TIMELINE_OBJECT_LEFT_ARROW].SetAlpha(ALPHA_30_PERCENT);
					}

					if (c_nameHash == ms_ComplexObject[TIMELINE_OBJECT_RIGHT_ARROW].GetObjectNameHash())
					{
						ms_ComplexObject[TIMELINE_OBJECT_RIGHT_ARROW].SetAlpha(ALPHA_30_PERCENT);
					}
				}

				if (c_eventType == EVENT_TYPE_MOUSE_RELEASE)
				{
					// move to previous clip on timeline:
					if (c_nameHash == ms_ComplexObject[TIMELINE_OBJECT_LEFT_ARROW].GetObjectNameHash())
					{
						ms_iScrollTimeline = -1;
					}

					// move to next clip on timeline:
					if (c_nameHash == ms_ComplexObject[TIMELINE_OBJECT_RIGHT_ARROW].GetObjectNameHash())
					{
						ms_iScrollTimeline = 1;
					}
				}
			}

			return;
		}



		//
		// check for click on the timeline area, and process the click/release here:
		//
		ePLAYHEAD_TYPE type;
		switch (c_buttonType)
		{
			case BUTTON_TYPE_OVERVIEW_SCROLLER:
			case BUTTON_TYPE_OVERVIEW_BACKGROUND:
			{
				type = PLAYHEAD_TYPE_OVERVIEW_VIDEO;
				break;
			}
			case BUTTON_TYPE_VIDEO_CLIP:
			{
				type = PLAYHEAD_TYPE_VIDEO;
				break;
			}

			case BUTTON_TYPE_AUDIO_CLIP:
			case BUTTON_TYPE_SCORE_CLIP:
			{
				type = PLAYHEAD_TYPE_AUDIO;
				break;
			}

			case BUTTON_TYPE_AMBIENT_CLIP:
			{
				type = PLAYHEAD_TYPE_AMBIENT;
				break;
			}

			case BUTTON_TYPE_TEXT_CLIP:
			{
				type = PLAYHEAD_TYPE_TEXT;
				break;
			}

			default:
			{
				type = PLAYHEAD_TYPE_VIDEO;
				uiAssertf(0, "CVideoEditorTimeline::UpdateMouseEvent() - invalid button type passed (%d)", c_buttonType);
			}
		}

		if (c_eventType == EVENT_TYPE_MOUSE_ROLL_OVER)
		{
			const s32 c_timelineIndexHovered = FindTimelineIndexFromHashName(type, c_nameHash);

			ms_mouseHoveredIndexFromActionscript = c_timelineIndexHovered;
		}

		if (c_eventType == EVENT_TYPE_MOUSE_ROLL_OUT)
		{
			if (!ms_trimmingClip)
			{
				if (c_buttonType == BUTTON_TYPE_OVERVIEW_SCROLLER || c_buttonType == BUTTON_TYPE_OVERVIEW_BACKGROUND)
				{
					if (ms_mousePressedTypeFromActionscript == PLAYHEAD_TYPE_OVERVIEW_VIDEO)
					{
						ms_mousePressedTypeFromActionscript = PLAYHEAD_TYPE_NONE;
					}
				}

				ms_mouseHoveredIndexFromActionscript = -1;
			
				if (ms_mousePressedIndexFromActionscript >= 0)  // if we roll out then apply the movement (dummy release)
				{
					const s32 c_timelineIndexReleased = FindTimelineIndexFromHashName(type, c_nameHash);

					if (c_timelineIndexReleased != -1)
					{
						ms_mouseReleasedIndexFromActionscript = c_timelineIndexReleased;
					}
					else
					{
						ms_mouseReleasedIndexFromActionscript = -2;
					}

					ms_mousePressedIndexFromActionscript = -1;
				}
			}
		}

		if (c_eventType == EVENT_TYPE_MOUSE_PRESS)
		{
			if (ms_mousePressedTypeFromActionscript != PLAYHEAD_TYPE_OVERVIEW_VIDEO && !ms_trimmingClipLeft && !ms_trimmingClipRight)
			{
				ms_mousePressedIndexFromActionscript = -1;
				ms_mouseReleasedIndexFromActionscript = -1;
				ms_mousePressedTypeFromActionscript = type;
				ms_mouseReleasedTypeFromActionscript = PLAYHEAD_TYPE_NONE;

				if (c_buttonType == BUTTON_TYPE_OVERVIEW_SCROLLER || c_buttonType == BUTTON_TYPE_OVERVIEW_BACKGROUND)
				{
					ms_mousePressedTypeFromActionscript = PLAYHEAD_TYPE_OVERVIEW_VIDEO;
				}
				else
				{
					const s32 c_timelineIndexPressed = FindTimelineIndexFromHashName(type, c_nameHash);

					if (c_timelineIndexPressed != -1)
					{
						ms_mousePressedIndexFromActionscript = c_timelineIndexPressed;
					}
				}
			}
		}

		if (c_eventType == EVENT_TYPE_MOUSE_RELEASE)
		{
			if (!ms_heldDownAndScrubbingOverviewBar)
			{
				ms_mousePressedTypeFromActionscript = PLAYHEAD_TYPE_NONE;
				ms_mouseReleasedTypeFromActionscript = PLAYHEAD_TYPE_NONE;
				//
				// check if the item selected has changed from a release:
				//
				if (!IsPreviewing())
				{
					if (type != PLAYHEAD_TYPE_NONE)
					{
						if (type != GetItemSelected())
						{
							ms_mouseReleasedTypeFromActionscript = type;
						}
					}
				}

				ms_mousePressedIndexFromActionscript = -1;

				const s32 c_timelineIndexReleased = FindTimelineIndexFromHashName(type, c_nameHash);

				if (c_timelineIndexReleased != -1)
				{
					ms_mouseReleasedIndexFromActionscript = c_timelineIndexReleased;
				}
				else
				{
					ms_mouseReleasedIndexFromActionscript = -2;
				}
			}
		}
	}
}
#endif // #if RSG_PC



#if __BANK
/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorTimeline::OutputTimelineDebug
// PURPOSE:	outputs useful debug output showing the current state of the timeline
/////////////////////////////////////////////////////////////////////////////////////////
void CVideoEditorTimeline::OutputTimelineDebug(const u32 uCallingIndex)
{
	// display a list of currently ordered clips:
	if (ms_ComplexObject[TIMELINE_OBJECT_TIMELINE_CONTAINER].IsActive())
	{
		uiDisplayf("Current Items on visual timeline:  (Calling index: %u)", uCallingIndex);
		uiDisplayf("{VIDEO_BEGIN}");

		for (s32 i = 0; i < ms_ComplexObjectClips.GetCount(); i++)  // go through full list
		{
			if (ms_ComplexObjectClips[i].object.IsActive())  // active
			{
				if (ms_ComplexObjectClips[i].type == PLAYHEAD_TYPE_VIDEO)  // its a video clip
				{
					uiDisplayf("   Array index: %u -  complex_obj_id: %d - pos: %0.2f    - uIndexToProject: %u", i, (s32)ms_ComplexObjectClips[i].object.GetId(),  ms_ComplexObjectClips[i].object.GetXPositionInPixels(), ms_ComplexObjectClips[i].uIndexToProject);
				}
			}
		}

		uiDisplayf("{END}");

		uiDisplayf("{AMBIENT_BEGIN}");

		for (s32 i = 0; i < ms_ComplexObjectClips.GetCount(); i++)  // go through full list
		{
			if (ms_ComplexObjectClips[i].object.IsActive())  // active
			{
				if (ms_ComplexObjectClips[i].type == PLAYHEAD_TYPE_AMBIENT)
				{
					uiDisplayf("   Array index: %u -  complex_obj_id: %d - pos: %0.2f    - uIndexToProject: %u", i, (s32)ms_ComplexObjectClips[i].object.GetId(),  ms_ComplexObjectClips[i].object.GetXPositionInPixels(), ms_ComplexObjectClips[i].uIndexToProject);
				}
			}
		}

		uiDisplayf("{END}");

		uiDisplayf("{AUDIO_BEGIN}");

		for (s32 i = 0; i < ms_ComplexObjectClips.GetCount(); i++)  // go through full list
		{
			if (ms_ComplexObjectClips[i].object.IsActive())  // active
			{
				if (ms_ComplexObjectClips[i].type == PLAYHEAD_TYPE_AUDIO)
				{
					uiDisplayf("   Array index: %u -  complex_obj_id: %d - pos: %0.2f    - uIndexToProject: %u", i, (s32)ms_ComplexObjectClips[i].object.GetId(),  ms_ComplexObjectClips[i].object.GetXPositionInPixels(), ms_ComplexObjectClips[i].uIndexToProject);
				}
			}
		}

		uiDisplayf("{END}");


		uiDisplayf("{TEXT_BEGIN}");

		for (s32 i = 0; i < ms_ComplexObjectClips.GetCount(); i++)  // go through full list
		{
			if (ms_ComplexObjectClips[i].object.IsActive())  // active
			{
				if (ms_ComplexObjectClips[i].type == PLAYHEAD_TYPE_TEXT)
				{
					uiDisplayf("   Array index: %u -  complex_obj_id: %d - pos: %0.2f    - uIndexToProject: %u", i, (s32)ms_ComplexObjectClips[i].object.GetId(),  ms_ComplexObjectClips[i].object.GetXPositionInPixels(), ms_ComplexObjectClips[i].uIndexToProject);
				}
			}
		}

		uiDisplayf("{END}");

	}
}
#endif // __BANK




//
// CVideoEditorTimelineThumbnailManager - timeline specifics:
//

u32 CVideoEditorTimelineThumbnailManager::GetActiveListCount() const
{
	return CVideoEditorTimeline::ms_ComplexObjectClips.GetCount();
}

sTimelineItem *CVideoEditorTimelineThumbnailManager::GetActiveListItem(const u32 uIndex) const
{
	if (uIndex < CVideoEditorTimeline::ms_ComplexObjectClips.GetCount())
	{
		if (CVideoEditorTimeline::ms_ComplexObjectClips[uIndex].type == PLAYHEAD_TYPE_VIDEO)  // only active list item if its video
		{
			return &CVideoEditorTimeline::ms_ComplexObjectClips[uIndex];
		}
	}

	return NULL;
}


void CVideoEditorTimelineThumbnailManager::SetupThumbnail(const u32 c_projectIndex) const
{
	CVideoEditorTimeline::AddTextureToVideoClip(c_projectIndex);
}

void CVideoEditorTimelineThumbnailManager::ReportFailed(const u32 c_projectIndex) const
{
	CVideoEditorTimeline::ReportTextureFailedToVideoClip(c_projectIndex);
}

s32 CVideoEditorTimelineThumbnailManager::GetLocalIndexFromProjectIndex(const u32 c_projectIndex) const
{
	return CVideoEditorTimeline::GetTimelineIndexFromProjectIndex(PLAYHEAD_TYPE_VIDEO, c_projectIndex);
}

void CVideoEditorTimelineThumbnailManager::SetSpinner(const u32 c_projectIndex, const bool bOnOff) const
{
	CVideoEditorTimeline::SetTimelineObjectSpinner(CVideoEditorTimeline::GetTimelineIndexFromProjectIndex(PLAYHEAD_TYPE_VIDEO, c_projectIndex), bOnOff);
}

bool CVideoEditorTimelineThumbnailManager::CheckForExcludedClip(s32 const c_projectIndex) const
{
	bool projectIndexIsInExcludedList = false;

	u32 const c_playheadPosMs = CVideoEditorTimeline::GetPlayheadPositionMs();
	s32 const c_projectIndexAtPlayhead = CVideoEditorTimeline::GetProjectIndexAtTime(CVideoEditorTimeline::GetItemSelected(), c_playheadPosMs);

	if (c_projectIndexAtPlayhead != -1)
	{
		if (c_projectIndex == c_projectIndexAtPlayhead)
		{
			projectIndexIsInExcludedList = true;
		}

		if (c_projectIndexAtPlayhead > 0 && c_projectIndex == c_projectIndexAtPlayhead - 1)
		{
			projectIndexIsInExcludedList = true;
		}

		if (c_projectIndex == c_projectIndexAtPlayhead + 1)
		{
			projectIndexIsInExcludedList = true;
		}
	}

	return projectIndexIsInExcludedList;
}



bool CVideoEditorTimelineThumbnailManager::CanMakeRequests() const
{
	if (CVideoEditorTimeline::IsScrollingFast())
	{
		// every so often we allow requests to happen, if the project index is the same as last check
		u32 const c_playheadPosMs = CVideoEditorTimeline::GetPlayheadPositionMs();
		s32 const c_projectClipIndexHovered = CVideoEditorTimeline::GetProjectIndexAtTime(CVideoEditorTimeline::GetItemSelected(), c_playheadPosMs);

		static s32 s_previousProjectClipIndexHovered = -1;
		static u32 s_forceThumbnailTimer = fwTimer::GetSystemTimeInMilliseconds();

#define TIME_TO_FORCE_THUMBNAIL_UPDATE_MS (100)
		if (fwTimer::GetSystemTimeInMilliseconds() > s_forceThumbnailTimer + TIME_TO_FORCE_THUMBNAIL_UPDATE_MS)
		{
			s_forceThumbnailTimer = fwTimer::GetSystemTimeInMilliseconds();

			if (c_projectClipIndexHovered == s_previousProjectClipIndexHovered)
			{
				return true;
			}

			s_previousProjectClipIndexHovered = c_projectClipIndexHovered;
		}

		return false;
	}

	return true;
}



s32 CVideoEditorTimelineThumbnailManager::GetNonVisibleBufferMs() const
{
	const s32 PIXELS_OUTSIDE_OF_VISIBLE_TIMELINE = 0;
	return PIXELS_OUTSIDE_OF_VISIBLE_TIMELINE;
}

s32 CVideoEditorTimelineThumbnailManager::GetWindowPosMs() const
{
	return (CVideoEditorTimeline::PixelsToMs(-CVideoEditorTimeline::ms_ComplexObject[TIMELINE_OBJECT_TIMELINE_CONTAINER].GetXPositionInPixels()));
};

s32 CVideoEditorTimelineThumbnailManager::GetWindowSizeMs() const
{
	return CVideoEditorTimeline::PixelsToMs(CVideoEditorTimeline::ms_ComplexObject[TIMELINE_OBJECT_TIMELINE_BACKGROUND].GetWidth());
}

#endif // #if defined( GTA_REPLAY ) && GTA_REPLAY

// eof
