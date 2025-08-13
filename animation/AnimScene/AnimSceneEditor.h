#ifndef _ANIM_SCENE_EDITOR_H_
#define _ANIM_SCENE_EDITOR_H_

#if __BANK

// rage includes
#include "atl/functor.h"
#include "bank/button.h"
#include "bank/combo.h"
#include "data/base.h"
#include "physics/debugPlayback.h" // for TimeLineZoom

// framework includes

// game includes
#include "animation/debug/AnimDebug.h"
#include "AnimSceneScrollbar.h"

class CAnimScene;
class CAnimSceneEntity;
class CAnimSceneEntityInitialiser;
class CAnimSceneEvent;
class CAnimSceneEventInitialiser;
class CAnimSceneSection;
class CGtaPickerInterfaceSettings;
class CAnimSceneEditor; 
class CEventPanelGroup; 

class CScreenExtents{
public:
	CScreenExtents()
		: m_min(0.0f, 0.0f)
		, m_max(0.0f, 0.0f)
	{

	}

	CScreenExtents(const Vector2& min,const Vector2& max) { Set(min, max); }
	CScreenExtents(float xMin, float yMin,float xMax, float yMax) { Set(xMin, yMin, xMax, yMax); }

	float GetMinX() const { return m_min.x; }
	float GetMinY() const { return m_min.y; }
	float GetMaxX() const { return m_max.x; }
	float GetMaxY() const { return m_max.y; }
	float GetWidth() const { return m_max.x - m_min.x; }
	float GetHeight() const { return m_max.y - m_min.y; }

	const Vector2& GetMin() const {return m_min; }
	const Vector2& GetMax() const {return m_max; }

	void Set(const Vector2& min, const Vector2& max) { m_min = min; m_max=max; }
	void Set(float xMin, float yMin,float xMax, float yMax)
	{
		m_min.x=xMin;
		m_min.y=yMin;
		m_max.x=xMax;
		m_max.y=yMax;
	}

	// PURPOSE: Return true if the specified x /y coordinate is within the extents
	bool Contains(float xCoord, float yCoord){ return (xCoord >= m_min.x) && (xCoord <= m_max.x) && (yCoord >= m_min.y) && (yCoord <= m_max.y); }
	bool Contains(const Vector2& coord){ return (coord.x >= m_min.x) && (coord.x <= m_max.x) && (coord.y >= m_min.y) && (coord.y <= m_max.y); }

	// PURPOSE: Draw a simple colored border around the extents
	void DrawBorder(Color32 color);

private:

	Vector2 m_min;
	Vector2 m_max;
};

//////////////////////////////////////////////////////////////////////////
// CSceneInformationDisplay - A re-usable, on-screen information display
//////////////////////////////////////////////////////////////////////////
class CSceneInformationDisplay
{
public:

	enum eColorType{
		kDisplayOutlineColor,			// Colour of the outline around the main display panel
		kDisplayBackgroundColor,		// Colour of the display background
	};

	CSceneInformationDisplay();
	CSceneInformationDisplay(const CScreenExtents& screenExtents);
	~CSceneInformationDisplay();

	// PURPOSE: Call once per frame to update the display
	void Process(float fCurrentTime, float fTotalSceneTime);

	// PURPOSE: Set the screen extents of the display
	void SetScreenExtents(const CScreenExtents& screenExtents);
	const CScreenExtents& GetScreenExtents() {return m_screenExtents; }

	// PURPOSE: Set the text colour to be used by the display
	void SetTextColour(Color32 colour) { m_textColour = colour; }

private:

	// PURPOSE: Gets the colour of a particular element
	Color32 GetColor(eColorType type);

	// PURPOSE: The rendered extents of the display
	CScreenExtents m_screenExtents;

	// PURPOSE: The colour to display the text in
	Color32 m_textColour;
};

//////////////////////////////////////////////////////////////////////////
//CDebug: Video control panel class
//////////////////////////////////////////////////////////////////////////
typedef Functor1<const CScreenExtents&> ButtonDrawCB;
typedef Functor1Ret <bool, u32>ButtonCB;

class COnScreenButton{
public:
	COnScreenButton(); 
	COnScreenButton(const CScreenExtents& screenExtents); 

	enum eButtonState
	{
		kButtonPressed = 0,
		kButtonReleased,
		kButtonHeld 
	};

	void Draw(); 
	void Update(); 

	// PURPOSE: Set the screen extents of the video control
	void SetScreenExtents(const CScreenExtents& screenExtents) { m_screenExtents = screenExtents; }
	const CScreenExtents& GetScreenExtents() {return m_screenExtents; }

	void SetActive(bool active) { m_isActive = active; }
	bool IsActive() { return m_isActive; }
	bool WasPressed() { return m_wasPressed; }
	bool IsHeld() { return m_isHeld; }
	bool IsHoveringOver(); 

	// PURPOSE: Set callback to call when the timeline selector is dragged i.e. time has changed
	void SetButtonDrawCallBack(ButtonDrawCB func) { m_drawButtonCB = func; }
	void SetButtonCallBack(ButtonCB func) { m_buttonCB = func; }

private:
	// PURPOSE: The rendered extents of the editor
	CScreenExtents m_screenExtents;

	// Callback to call when the selector is dragged on the timeline (i.e current time is changed)
	ButtonDrawCB m_drawButtonCB;
	
	ButtonCB m_buttonCB; 
	
	bool m_isActive : 1; 
	bool m_isHeld : 1; 
	bool m_isFocus : 1; 
	bool m_wasPressed : 1; 
};

//////////////////////////////////////////////////////////////////////////
// CAuthoringHelperDisplay - A re-usable, panel for interfacing with the authoring helper
//////////////////////////////////////////////////////////////////////////

class CAuthoringHelperDisplay
{
public:
	enum eColorType{
		kVideoControlPanelBackgroundColor = 0, 
		kVideoControlPanelOutlineColor, 
		kVideoControlButtonColor,
		kVideoControlHoverButtonColor,
		kVideoControlActiveButtonColor,
		kVideoControlActiveButtonHighLigtColor,
		kVideoControlActiveButtonBackgroundColor,
		kVideoControlPanelActiveButtonOutlineColor,
		kVideoControlPanelButtonOutlineColor,
		kMaxNumColors
	};

	enum eButtons{
		kTransGbl=0,
		kTransLcl, 
		kRotLcl,
		kRotGbl,
		kMaxNumButtons
	};

	CAuthoringHelperDisplay(); 
	~CAuthoringHelperDisplay(); 

	// PURPOSE: Call once per frame to update the display
	void Process();

	// PURPOSE: Set the screen extents of the display
	void SetScreenExtents(const CScreenExtents& screenExtents);
	const CScreenExtents& GetScreenExtents() {return m_screenExtents; }
	void SetColor(u32 ColorType, Color32 Color32);  

	COnScreenButton* GetButton(const u32& button); 
private:
	void Init(); 

	// PURPOSE: Gets the colour of a particular element
	Color32 GetColor(eColorType type);

	void DrawTransGbl(const CScreenExtents& screenExtents); 
	void DrawTransLcl(const CScreenExtents& screenExtents); 
	void DrawRotLcl(const CScreenExtents& screenExtents); 
	void DrawRotGbl(const CScreenExtents& screenExtents); 

	bool DrawButtonBackground(const CScreenExtents& , const u32& button); 

	void SetCanvasSize(CScreenExtents& screenExtents); 
private:

	// PURPOSE: The rendered extents of the display
	CScreenExtents m_screenExtents;

	COnScreenButton m_buttons[kMaxNumButtons]; 
	Color32 m_colors[kMaxNumColors]; 
};

class COnScreenVideoControl{
public:
	
	enum eColorType{
		kVideoControlBackgroundColor = 0, 
		kVideoControlButtonColor,
		kVideoControlHoverButtonColor,
		kVideoControlActiveButtonColor,
		kVideoControlActiveButtonHighLigtColor,
		kVideoControlActiveButtonBackgroundColor, 
		kVideoControlDisplayOutlineColor, 
		kVideoControlPanelActiveButtonOutlineColor,
		kVideoControlPanelButtonOutlineColor,
		kMaxNumColors
	};

	enum eButtons{
		kStepToStart=0,
		kStepBackwards, 
		kPlayBackwards,
		kStop,
		kPlayForwards,
		kStepForwards,
		kStepToEnd,
		kMaxNumButtons
	};

	COnScreenVideoControl(); 
	~COnScreenVideoControl(); 
	
	// PURPOSE: Set the screen extents of the video control
	void SetScreenExtents(const CScreenExtents& screenExtents);
	const CScreenExtents& GetScreenExtents() {return m_screenExtents; }
	void SetColor(u32 ColorType, Color32 Color32);  

	COnScreenButton* GetButton(const u32& button); 

	void Process();

private:
	// PURPOSE: Gets the colour of a particular element
	Color32 GetColor(eColorType type);
	void SetCanvasSize(CScreenExtents& screenExtents); 

	void DrawStepToStartButton(const CScreenExtents& extents); 
	void DrawStepBackwardsButton(const CScreenExtents& extents); 
	void DrawPlayBackwardsButton(const CScreenExtents& extents); 
	void DrawStopButton(const CScreenExtents& extents); 
	void DrawPlayForwardsButton(const CScreenExtents& extents); 
	void DrawStepForwardsButton(const CScreenExtents& extents); 
	void DrawStepToEndButton(const CScreenExtents& extents); 

	bool DrawButtonBackground(const CScreenExtents& , const u32& button); 

	void Init(); 
	
private:

	// PURPOSE: The rendered extents of the editor
	CScreenExtents m_screenExtents; 

	COnScreenButton m_buttons[kMaxNumButtons]; 

	Color32 m_colors[kMaxNumColors]; 

	bool m_renderSolid : 1; 

};


struct EventInfo
{
	CAnimSceneEvent* event;
	u32 position;
};

class CEventPanel 
{
public:
	CEventPanel(EventInfo info); 
	~CEventPanel();
	// PURPOSE: Set the screen extents of the video control
	void SetScreenExtents(const CScreenExtents& screenExtents);
	const CScreenExtents& GetScreenExtents() {return m_screenExtents; }

	EventInfo& GetEventInfo() { return m_event; }

	void RenderToolsOverlay(CAnimSceneEditor* animSceneEditor, CScreenExtents& groupScreenExtents, CEventPanelGroup* PanelGroup, int colourId); 

	void RenderEventToolsOverlay(CAnimSceneEditor* animSceneEditor, EventInfo& info, CScreenExtents& groupScreenExtents, CEventPanelGroup* PanelGroup, int colourId); 

	void Render(CAnimSceneEditor* animSceneEditor, CScreenExtents& groupScreenExtents, CEventPanelGroup* PanelGroup, int colourId); 

	void RenderEvent(CAnimSceneEditor* animSceneEditor, EventInfo& info, CScreenExtents& groupScreenExtents, CEventPanelGroup* PanelGroup, int colourId); 

	float GetEventHeight() { return m_eventHeight; } 

public:
	float m_eventHeight; 
	float m_offscreenArrowMarker; 

private:

	EventInfo m_event;

	// PURPOSE: The rendered extents of the editor
	CScreenExtents m_screenExtents; 

	// PURPOSE: Helper to draw a dashed line (for blend info)
	void DrawDashedLine(const Vector3& v1, const Vector3& v2, const Color32& colour, int numDashes);

	// PURPOSE: Helper to draw a colour blend between events when they have blend in/out
	//Must be 4 points, first two associated with the left colour, second two associated with the right colour
	void DrawTransitionBlock(const Vector3 *points, const Color32& leftColour, const Color32& rightColour);
};

// A panel group are a selection of event panels related to an entity
class CEventPanelGroup
{
public:
	CEventPanelGroup(atHashString name); 
	~CEventPanelGroup();

	void AddEvent(EventInfo EventInfo); 
	void RemoveEvent(EventInfo Event); 
	void RemoveAllEvents(); 

	// PURPOSE: Set the screen extents of the video control
	void SetScreenExtents(const CScreenExtents& screenExtents);
	const CScreenExtents& GetScreenExtents() {return m_screenExtents; }

	atArray<CEventPanel*>& GetEvents() { return m_events; } 

	atHashString& GetName() { return m_name; }

	void Render(CAnimSceneEditor* animSceneEditor); 

	float GetGroupHeight() { return m_GroupHeight; } 
	float GetGroupSpacing()  { return m_GroupSpacing; }

	float GetGroupPanelHeight() { return m_GroupHeight + m_GroupSpacing; } 

	void AddEventInfo(const char* EventInfo); 

	void RenderEventInfoOverlay(CAnimSceneEditor* animSceneEditor); 

	//PURPOSE: Checks whether there are any timing gaps between animations for this group
	bool DoesGroupHaveGaps();

private:

	static int CompareEvents(CEventPanel* const* pEvent1,  CEventPanel* const* pEvent2);

	atHashString m_name; 

	atArray<atString> m_HoveringInfo; 

	float m_GroupHeight; 
	float m_GroupSpacing; 
	// PURPOSE: The rendered extents of the editor
	CScreenExtents m_screenExtents; 
	atArray<CEventPanel*> m_events; 

};


class CEventLayerPanel
{
public:
	CEventLayerPanel(atHashString name); 
	~CEventLayerPanel();

	void AddEventGroup(atHashString name);
	void RemoveEventGroup(u32 i); 

	// PURPOSE: Set the screen extents of the video control
	void SetScreenExtents(const CScreenExtents& screenExtents);
	const CScreenExtents& GetScreenExtents() {return m_screenExtents; }

	atHashString& GetName() { return m_name; }

	atArray<CEventPanelGroup*>& GetEventGroup() { return m_groups; } 

	void RenderPanelInfo(CAnimSceneEditor* animSceneEditor); 

	void Render(CAnimSceneEditor* animSceneEditor); 

	float GetTotalPanelHeight(); 

private:
	float m_basePanelInfoHeight;
	float m_basePanelSpacing; 

	atHashString m_name; 

	// PURPOSE: The rendered extents of the editor
	CScreenExtents m_screenExtents;

	atArray<CEventPanelGroup*> m_groups; 
};


class CEventLayersPane
{
public:
	CEventLayersPane(); 
	~CEventLayersPane();

	void AddEventLayerPanel(atHashString name); 
	void RemoveEventLayerPanel(int i); 

	// PURPOSE: Set the screen extents of the video control
	void SetScreenExtents(const CScreenExtents& screenExtents);
	const CScreenExtents& GetScreenExtents() {return m_screenExtents; }

	void SetEventScreenExtents(const CScreenExtents& m_eventScreenExtents);
	const CScreenExtents& GetEventScreenExtents() {return m_eventScreenExtents; }

	atArray<CEventLayerPanel*>& GetEventLayers() { return m_eventLayerPanels; } 
 
	CEventPanelGroup* GetEventPanelGroup(u32 PanelLayer, u32 Group); 

	void Render(CAnimSceneEditor* animSceneEditor); 

private:
	// PURPOSE: The rendered extents of the editor
	CScreenExtents m_screenExtents;
	CScreenExtents m_eventScreenExtents;

	atArray<CEventLayerPanel*> m_eventLayerPanels; 
};

//////////////////////////////////////////////////////////////////////////
//	Anim scene sectioning and playback list controls
//////////////////////////////////////////////////////////////////////////

class CSectioningPanel
{
public:
	enum flags{
		kSelectedListChanged = BIT(0),
		kDraggingSectionFromMasterList = BIT(1),
		kDraggingSectionFromPlaybackList = BIT(2),
		kDraggingScrollBar = BIT(3)
	};

	static atHashString ms_NoListString;
	static atHashString ms_InvalidSectionId;
	static atHashString ms_InvalidPlaybackListId;

	CSectioningPanel()
		: m_selectedList(ms_NoListString)
		, m_draggedSection(ms_InvalidSectionId)
		, m_dragStartTime(0)
		, m_scrollOffset(0.f)
		, m_scrollbar()
	{

	}; 

	~CSectioningPanel() {};

	// PURPOSE: Set the screen extents of the video control
	void SetScreenExtents(const CScreenExtents& screenExtents) 
	{ 
		m_screenExtents = screenExtents; 
	}
	const CScreenExtents& GetScreenExtents() {return m_screenExtents; }

	void Render(CAnimSceneEditor* animSceneEditor); 

	bool SelectedListChangedThisFrame() { return m_flags.IsFlagSet(kSelectedListChanged);}
	atHashString GetSelectedPlaybackList() { return m_selectedList==ms_NoListString ? ms_InvalidPlaybackListId : m_selectedList; }
	void SetSelectedPlaybackList(atHashString id) 
	{ 
		if (id.GetHash()==ms_InvalidPlaybackListId.GetHash())
			m_selectedList=ms_NoListString;
		else
			m_selectedList = id;
	}

	atHashString GetSelectedSection() { return m_selectedSection; }
	void SetSelectedSection(atHashString id) 
	{ 
		m_selectedSection = id;
	}

	fwFlags8& GetFlags() { return m_flags; }

	bool IsDraggingSection() { return m_flags.IsFlagSet(kDraggingSectionFromMasterList | kDraggingSectionFromPlaybackList); }

	bool IsDraggingScrollbar() { return m_flags.IsFlagSet(kDraggingScrollBar); }

private:

	// PURPOSE: The rendered extents of the editor
	CScreenExtents m_screenExtents;
	
	// PURPOSE: The currently selected playback list
	atHashString m_selectedList;

	// PURPOSE: The section currently selected in the master list
	atHashString m_selectedSection;

	// PURPOSE: The section being dragged
	atHashString m_draggedSection;

	// PURPOSE: Track the time we started dragging
	u32 m_dragStartTime;

	// PURPOSE: Control flags
	fwFlags8 m_flags;

	/************************************************************************/
	/* Scroll bar control                                                   */
	/************************************************************************/

	// PURPOSE: The offset of the section group (controlled by the scrollbar)
	float m_scrollOffset;

	void ScrollRight()
	{
		m_scrollOffset = Clamp(m_scrollOffset -= 10.f,-m_scrollbar.GetMaxValue(),0.f);
	}

	void ScrollLeft()
	{
		m_scrollOffset = Clamp(m_scrollOffset += 10.f,-m_scrollbar.GetMaxValue(),0.f);
	}

	CAnimSceneScrollbar m_scrollbar;

};


//////////////////////////////////////////////////////////////////////////
// CDebugTimeline - A re-usable, mouse controllable on screen timeline
// control for debug systems
//////////////////////////////////////////////////////////////////////////
typedef Functor1<float> TimelineSelectorDraggedCB; // New time

class COnScreenTimeline{
public:

	enum eColorType{
		kTimelineSelectorColor = 0,				// Colour of the selector/side arrows when mouse is not hovering over them.
		kTimelineSelectorHoverColor,			// Colour of the selector/side arrows when mouse is hovering over them.
		kTimelineSelectorDraggingColor,			// Colour of the selector when the mouse is dragging it.

		kTimelineSelectorTimeTextColor,			// Colour of the text for the current time of the selector.
		kTimelineSelectorTimeBackgroundColor,	// Colour of the background behind the selector time text.

		kTimelineOutlineColor,					// Colour of the outline around the main timeline
		kTimelineBackgroundInRangeColor,		// Colour of the timeline background behind the selectable range.
		kTimelineBackgroundOutOfRangeColor,		// Colour of the timeline background outside of the selectable range.

		kTimelineTimeMarkerLabelColor,			// Colour of the frame/time labels across the timeline.

		kTimelineTimeMarker1FrameColor,			// Colour of the single frame time markers.
		kTimelineTimeMarker5FrameColor,			// Colour of the single frame time markers.
		kTimelineTimeMarker30FrameColor,			// Colour of the single frame time markers.

		kTimelineColor_MaxNum
	};

	struct TimeLineZoom
	{
		TimeLineZoom()
		{
			Reset(); 
		}

		void CalculateInitialZoom(float xMin, float xMax, float fRangeStart, float fRangeEnd);

		void UpdateZoom(float xMinX, float xMaxX, float fMouseX, COnScreenTimeline* owningTimeline, bool bZoomOnly = false);
		void Reset()
		{
			m_LastMouseTrackFrame = 0;
			m_fLastMouseX = 0.0f;
			m_fZoom = 1.0f;
			m_fTimeLineOffset = 0.0f;
		}
		u32 m_LastMouseTrackFrame;
		float m_fLastMouseX;
		float m_fZoom;
		float m_fTimeLineOffset;
	};

	enum eFlags{
		kUserControlDisabled = BIT(0),
		kDraggingSelector = BIT(1),
		kDraggingTimeline = BIT(2),
	};

	enum eTimeDisplayMode
	{
		kDisplayTimes = 0,
		kDisplayFrames
	};

	COnScreenTimeline();
	COnScreenTimeline(const CScreenExtents& screenExtents);
	~COnScreenTimeline();
	
	// PURPOSE: Call once per frame to update the timeline
	void Process();

	// PURPOSE: Set the screen extents of the timeline control
	void SetScreenExtents(const CScreenExtents& screenExtents);
	const CScreenExtents& GetScreenExtents() {return m_screenExtents; }

	// PURPOSE: Set and get the current zoom level on the control
	void SetZoom(float zoom) { m_zoomController.m_fZoom = zoom; }
	float GetZoom() { return m_zoomController.m_fZoom; }
	void CalculateInitialZoom(float fRangeStart, float floatRangeEnd);
	
	// PURPOSE: Set and get the current time of the selector
	void SetCurrentTime(float time) { m_currentTime = Clamp(time, m_minSelectableTime, m_maxSelectableTime); }
	float GetCurrentTime() { return m_currentTime; }

	// PURPOSE: Gets the selectors position on the timeline visually.
	// 0.0 = at the start, 1.0 = at the start.
	// Out of that range, means the selector is outside of the visible timeline range
	float GetSelectorVisualPosition();

	// PURPOSE: Moves the visible timeline range so that the marker is at a specific position on it visually.
	// 0.0 = at the start of the visible timeline.
	// 1.0 = at the end of the visible timeline.
	void MoveMarkerToVisibleTimelinePosition(float fPositionOnVisibleTimeline);

	// PURPOSE: Enable or disable user mouse control of the timeline
	void EnableUserControl() { m_flags.ClearFlag(kUserControlDisabled); }
	void DisableUserControl() { m_flags.SetFlag(kUserControlDisabled); }
	bool IsUserControlEnabled() { return m_flags.IsFlagSet(kUserControlDisabled); }

	// PURPOSE: Render times as times or frame numbers?
	void SetTimeDisplayMode(eTimeDisplayMode mode) { m_timeMode = mode; }

	// PURPOSE: Should the timeline marker snap to frames?
	void SetMarkerSnapToFrames(bool bSnap) { m_bMarkerSnapToFrames = bSnap; }

	// PURPOSE: Set callback to call when the timeline selector is dragged i.e. time has changed
	void SetTimelineSelectorDraggedCallback(TimelineSelectorDraggedCB func) { m_timelineSelectorDraggedCB = func; }

	// PURPOSE: Return the screen x coordinate represented by the passed
	//			in time, given the position and zoom level on the control.
	// PARAMS:	time: The time to get the screen coord for
	// RETURNS: The screen x coordinate for the given time (note: may be out of the range of the timeline control).
	float GetScreenX(float time);

	// PURPOSE: Returns the time on the timeline for the screen X coordinate
	float GetTimeForScreenX(float x);

	// PURPOSE: Returns the minimum time currently visible on the timeline
	float GetMinVisibleTime();

	// PURPOSE: Returns the maximum time currently visible on the timeline
	float GetMaxVisibleTime();

	// PURPOSE: Returns the minimum selectable time
	float GetMinSelectableTime() { return m_minSelectableTime; };

	// PURPOSE: Returns the maximum selectable time
	float GetMaxSelectableTime() { return m_maxSelectableTime; };

	// PURPOSE: Return the timeline scale modifier (the inverse of the zoom value)
	float GetScale() { return 1.0f / m_zoomController.m_fZoom; }

	// PURPOSE: Returns true if the user is currently dragging the timeline controls
	bool IsDragging() { return m_flags.IsFlagSet(kDraggingSelector | kDraggingTimeline); }

	// PURPOSE: Is the selector being dragged
	bool IsDraggingSelector() { return m_flags.IsFlagSet(kDraggingSelector); }

	// PURPOSE: Is the time line being dragged
	bool IsDraggingTimeline() { return m_flags.IsFlagSet(kDraggingTimeline); }

	// PURPOSE: Set the minimum and maximum allowed selectable time on the time line
	void SetSelectableRange(float minTime, float maxTime) { m_minSelectableTime = minTime; m_maxSelectableTime = maxTime; }

	// PURPOSE: Set the colour for a particular timeline element
	void SetColor(u32 ColorType, Color32 Color32);

private:

	// PURPOSE: Gets the colour of a particular element
	Color32 GetColor(eColorType type);

	// PURPOSE: Clamp offset to range
	void ClampTimelineOffset();

	// Helper function to check if the mouse pointer is hovering over the time selector
	bool IsHoveringSelector();

	// Customisable colours for the timeline
	Color32 m_colors[kTimelineColor_MaxNum];

	// PURPOSE: The current time selected by the timeline indicator
	float m_currentTime;
	float m_minSelectableTime;
	float m_maxSelectableTime;

	float m_lastMouseX;

	// PURPOSE: Helper zoom controller for handling mouse control
	TimeLineZoom m_zoomController;

	// PURPOSE: The rendered extents of the editor
	CScreenExtents m_screenExtents;

	// Display text in times or frames?
	eTimeDisplayMode m_timeMode;

	// Should the marker snap to frame boundaries?
	bool m_bMarkerSnapToFrames;

	// Callback to call when the selector is dragged on the timeline (i.e current time is changed)
	TimelineSelectorDraggedCB m_timelineSelectorDraggedCB;

	fwFlags32 m_flags;
};

/////////////////////////////////////////////////////////////////////////////////
// CAnimSceneEditor - A combined on screen and RAG editing suite for anim scenes
/////////////////////////////////////////////////////////////////////////////////

class CAnimSceneEditor : datBase {

public:
	
	enum eFocusPanel{
		kFocusEntity = 0,
		kFocusEvent,
		kFocusPlaybackList,
		kFocusSection
	};

	enum eControlFlags{
		kDraggingEvent = BIT(0),
		kDraggingEventStart = BIT(1),
		kDraggingEventEnd = BIT(2),
		kDraggingEventPreLoad = BIT(3),
		kRegenerateSelectionWidgets = BIT(4),
		kRegenerateNewEventWidgets = BIT(5),
		kCloseCurrentScene = BIT(6),
		kMadeEntitySelectionThisFrame = BIT(7),
		kInitialTimelineZoomSet = BIT(8),
		kMadeEventSelectionThisClick = BIT(9),
		kRegenerateEventTypeWidgets = BIT(10)
	};

	enum eColorType{
		kMainTextColor = 0,
		kSelectedTextColor,
		kSelectedTextOutlineColor,
		kFocusTextOutlineColor,
		kTextBackgroundColor,
		kSelectedTextBackGroundColor,
		kTextOutlineColor,
		kTextHoverColor, 
		kTextWarningColor, 
		kSelectedTextHoverColor,
		kDefaultEventColor,
		kVfxEventColor,
		kPlayAnimEventColor,
		kPlayAnimEventColorAlt,
		kEventMarkerColor,
		kEventOutlineColor,
		kEventBackgroundColor,
		kSelectedEventColor,
		kSelectedEventMarkerColor,
		kSelectedEventOutlineColor,
		kSelectedEventDraggingColor,
		kEventPreloadLineColor,
		kEventCurrentTimeLineColor,
		kPlayControlColor,
		kPlayControlHoveredColor,
		kDisabledTextColor,
		kDisabledBackgroundColor,
		kDisabledOutlineColor,
		kBlendInMarkerColor,
		kBlendOutMarkerColor,
		kRegionOutlineColor,
		kScrollBarBackgroundColour,
		kScrollBarActiveBackgroundColour,
		kScrollBarHoverBackgroundColour,
		kSectionDividerColor,
		kMaxNumColors
	};

	class Selection{
	public:
		Selection()
			: m_pEvent(NULL)
			, m_pEntity(NULL)
		{

		}

		CAnimSceneEntity* GetEntity() { return m_pEntity; }
		CAnimSceneEvent* GetEvent() { return m_pEvent; }
		void SelectEntity( CAnimSceneEntity* pEnt);
		void SelectEvent( CAnimSceneEvent* pEnt);
	private:
		CAnimSceneEvent* m_pEvent;
		CAnimSceneEntity* m_pEntity;
	};

	// PURPOSE: Used to handle stacking of events based on type and sub type
	struct EventLayer
	{
		struct SubGroup{

			SubGroup()
			{

			}

			SubGroup(atHashString name)
				: m_name(name)
			{

			}
			atHashString m_name;
			atArray<EventInfo> m_events;
		};

		EventLayer()
		{

		}
		EventLayer(atHashString name)
			: m_name(name)
		{

		}

		atHashString m_name;
		atArray<SubGroup> m_subGroups;

		static float ms_layerHeight;
	};

	struct Widgets{

		Widgets()
		{
			Init();
		}
		~Widgets()
		{
			Shutdown();
		}

		void Init();

		void Shutdown();

		bkGroup* m_pSelectionGroup;
		
		char      m_saveStatusTextArray[32];
		bkText*   m_pSaveStatus;
		bkButton* m_pSaveButton;
		bkButton* m_pCloseButton;

		bkGroup* m_pSceneGroup;
		
		bkGroup* m_pAddEntityGroup;

		bkCombo* m_pEntityTypeCombo;
		s32 m_selectedEntityType;
		atHashString m_newEntityId;
		atArray<const char *> m_entityNameList;

		bkGroup* m_pAddSectionGroup;
		atHashString m_newSectionId;
		CDebugClipSetSelector m_playbackListLeadInClipSetSelector;

		bkGroup* m_pAddEventGroup;

		bkCombo* m_pEventTypeCombo;
		s32 m_selectedEventType;
		atArray<const char *> m_eventNameList;
		
 		CDebugAnimSceneSectionSelector m_addEventSectionSelector;
	};

	CAnimSceneEditor();
	CAnimSceneEditor(CAnimScene* pScene, const CScreenExtents& screenExtents);
	~CAnimSceneEditor();

	// PURPOSE: init the scene editing
	void Init(CAnimScene* pScene, const CScreenExtents& screenExtents);

	// PURPOSE: Shutdown the scene editing
	void Shutdown();

	// PURPOSE: Returns true if editing is currently active
	bool IsActive() { return m_pScene!=NULL; }

	// PURPOSE: Call once per frame to update the editor (from the render thread)
	void Process_RenderThread();

	// PURPOSE: Call once per frame to update the editor (from the main thread)
	void Process_MainThread();

	// PURPOSE: Display the mini-map this frame?
	bool GetDisplayMiniMapThisUpdate();

	// PURPOSE: Callback called when the timeline selector is dragged, which updates the time.
	void TimelineSelectorDragged(float fNewTime);

	// PURPOSE: Set the screen extents of the editor
	void SetScreenExtents(const CScreenExtents& screenExtents);
	
	// PURPOSE: Set the anim scene currently being edited
	void SetAnimScene(CAnimScene* pScene);

	// PURPOSE: HAndle rendering and update for a single event
	void UpdateEventLayerInfo(CAnimSceneEvent* evt);

	void RenderEvent(CAnimSceneEvent* evt, float yMin); 

	// PURPOSE: Add the supporting editing widgets.
	void AddWidgets(bkGroup* pGroup);
	bool HasWidgets()  {return m_widgets.m_pSelectionGroup!=NULL; }

	// PURPOSE: Add the editor settings widgets
	void AddEditorWidgets(bkGroup* pGroup);

	// PURPOSE: Select the given event in the editor
	void SelectEvent(CAnimSceneEvent* pEvent, bool setFocus = true);

	// PURPOSE: Select the given entity in the editor
	void SelectEntity(CAnimSceneEntity* pEnt, bool setFocus = true);

	// PURPOSE: Save the scene currently being edited
	void SaveCurrentScene();

	// PURPOSE: Close the scene currently being edited
	void CloseCurrentScene();

	CAnimScene* GetScene() { return m_pScene; }

	// PURPOSE: editor flag control
	bool IsFlagSet(eControlFlags flag) const { return m_flags.IsFlagSet(flag); }
	void SetFlag(eControlFlags flag){ m_flags.SetFlag(flag); }
	void ClearFlag(eControlFlags flag){ m_flags.ClearFlag(flag); }

	// PURPOSE: Called from the RAG widgets. Flags the current scene for destruction on the next update
	void TriggerCloseCurrentScene();

	bool IsDragging() { return IsDraggingEvent() || m_timeline.IsDragging() || m_sectionPanel.IsDraggingSection() || IsDraggingMatrixHelper() || m_eventsScrollbar.IsDragging(); }
	bool IsDraggingEvent() { return m_flags.IsFlagSet(kDraggingEvent | kDraggingEventEnd | kDraggingEventPreLoad | kDraggingEventStart); }

	// PURPOSE: Return true if a matrix helper is currently being dragged
	bool IsDraggingMatrixHelper();

	// PURPOSE: Return the currently selected event
	CAnimSceneEvent* GetSelectedEvent() { return m_selected.GetEvent(); }

	// PURPOSE: Return the currently selected entity
	CAnimSceneEntity* GetSelectedEntity() { return m_selected.GetEntity(); }
	atHashString GetSelectedEntityId();

	// PURPOSE: Return the color to be used for the given type of drawing
	Color32 GetColor(eColorType type);
	void SetColor(u32 ColorType, Color32 color); 

	COnScreenTimeline& GetTimeLine() { return m_timeline; }

	// PURPOSE: Set or get the panel that currently has focus
	void SetFocus(eFocusPanel panel) { m_currentFocusPanel = panel; }
	eFocusPanel GetFocus() const { return m_currentFocusPanel; }

	// PURPOSE: Return the currently selected time on the timeline.
	float GetCurrentTime() { return m_timeline.GetCurrentTime(); }

	// PURPOSE: Returns the internal widget structure (useful for getting values from selectors etc when initing events and entities).
	Widgets& GetWidgets() { return m_widgets; }

	// PURPOSE: Sets the picker focus entity to be the currently selected anim scene entities' entity. 
	void SyncPickerFromAnimSceneEntity();

	// PURPOSE: Sets the currently selected anim scene to be the pickers selected entity. 
	void SyncAnimSceneEntityFromPicker(); 

	//check that the player is not using the picker to select a new game entity for the anim scene entity
	bool IsSelectingADifferentGameEntity(); 

	//render the possible new registered game entity
	void RenderSelectedAnimScenesPossibleNewGameEntity(); 

	//reset and start the scne when a new enity is registered
	void OnGameEntityRegister(); 

	bool PlaySceneForwardsCB(u32 buttonState); 
	
	bool PlaySceneBackwardsCB(u32 buttonState); 

	bool StepForwardCB(u32 buttonState); 

	bool StepBackwardCB(u32 buttonState); 

	bool StepToStartCB(u32 buttonState);

	bool StepToEndCB(u32 buttonState);

	bool StopCB(u32 buttonState); 
	
	bool SetAuthoringHelperTransGbl(u32 buttonState);

	bool SetAuthoringHelperTransLcl(u32 buttonState);

	bool SetAuthoringHelperRotGbl(u32 buttonState);

	bool SetAuthoringHelperRotLcl(u32 buttonState);

	float GetEventLayersYScreenOffset() { return m_EventLayersYScreenOffset;}

	static bool GetUseSmoothFont() { return m_editorSettingUseSmoothFont; }

	bool GetEditEventPreloads() { return m_editorSettingEditEventPreloads; }

	void RefreshScreenExtents() { SetScreenExtents(m_screenExtents);}

	bool CanDragEvent(CAnimSceneEvent* pEvent) { return pEvent==m_pSelectedEventOnLastRelease; }

	// PURPOSE: Fix invalid id.
	bool FixInvalidId(CAnimSceneEntity::Id& id);

	//PURPOSE: creates new scene entity and adds it to the editor
	CAnimSceneEvent*  CreateEventAndAddToCurrentScene(const char * eventTypeName, CAnimSceneEventInitialiser* pInitialiser, atHashString sectionId);

	static float ms_editorSettingBackgroundAlpha;
	static bool ms_editorSettingUseEventColourForMarker;

private:

	void InitAuthoringHelperPanel(); 

	void InitVideoControlPanel();

	void SetAuthoringHelperColors();

	void InitScenePicker(); 

	void SetEditorAndComponentColors();

	void SetVideoControlPanelColors();

	void SetTimelinePanelColors();

	void SetAnimSceneEditorColors(); 

	// PURPOSE: Adds a new entity to the scene of the selected id and type
	void AddNewEntity();

	//PURPOSE: creates new scene entity and adds it to the editor
	CAnimSceneEntity*  CreateEntityAndAddToSceneEditor(CAnimSceneEntity::Id newID, CAnimSceneEntityInitialiser* pInitialiser, parStructure* pStruct); 

	// PURPOSE: Create a new event with the given type name from the pargen definition
	CAnimSceneEvent* CreateNewEventOfType(const char * typeName);

	// PURPOSE: Adds a new event of the selected type to the selected entity
	void AddNewEvent();

	// PURPOSE: Add a new section to the current scene
	void AddNewSection();

	// PURPOSE: Add a new playback list to the current scene
	void AddNewPlaybackList();

	// PURPOSE: A new lead in has been selected from the combo box
	void LeadInSelected();

	// PURPOSE: Add a new section to the current scene
	void DeleteSection();

	// PURPOSE: Adds the widgets to support adding new events to the given entity
	void AddNewEventWidgetsForEntity(CAnimSceneEntity* pEntity, bkGroup* pGroup);

	// PURPOSE: Regenerate the entity type specific widgets for adding new entities
	void RegenEntityTypeWidgets();

	// PURPOSE: Regenerate the event type specific widgets for adding new events
	void RegenEventTypeWidgets();

	// PURPOSE: Helper function to return the y position of the given event on screen
	void ComputeEventLayerInfo(CAnimSceneEvent* evt);

	// PURPOSE: Destroys the widgets for the currently selected entity / event
	void ClearSelectionWidgets();
	
	// PURPOSE: Event layer management
	void InitEventLayers();
	void CullEmptyEventLayers();
	//void RenderEventLayers();
	void AddEventLayerIfMissing(atHashString name);

	// Rag callbacks
	void TimelineDisplayModeChangedCB();
	void TimelineSnapChangedCB();

	// PURPOSE: Create the entity initialiser templates used to create new entities
	void InitEntityInitialisers();
	// PURPOSE: Return the entity initialiser for the entity class with the given name
	CAnimSceneEntityInitialiser* GetEntityInitialiser(atHashString entityClassName);
	// PURPOSE: Return the entity initialiser for the entity class with the given name
	CAnimSceneEntityInitialiser* GetEntityInitialiserFromFriendlyName(atHashString friendlyName);
	// PURPOSE: Clean up widgets on any entity initialisers that require it
	void CleanupEntityInitialiserWidgets();

	// PURPOSE: Create the event initialiser templates used to create new events
	void InitEventInitialisers();
	// PURPOSE: Return the event initialiser for the event class with the given name
	CAnimSceneEventInitialiser* GetEventInitialiser(atHashString eventClassName);
	// PURPOSE: Return the event initialiser for the event class with the given name
	CAnimSceneEventInitialiser* GetEventInitialiserFromFriendlyName(atHashString friendlyName);
	// PURPOSE: Clean up widgets on any event initialisers that require it
	void CleanupEventInitialiserWidgets();
	// PURPOSE: Initialise the scrollbar at the correct size and position on the screen
	void InitEventScrollbar();

	// PURPOSE: Detect and handle editing an older anim scene than the current version
	void CheckAnimSceneVersion();
	//PURPOSE: Render a vertical scrollbar on the left side of the screen
	void RenderEventsScrollbar();
	// PURPOSE: Call the validate function on the scene
	bool ValidateScene();
	// PURPOSE: Ensure the editor and the scene have the same values for overriding holding on the last frame
	void SynchroniseHoldAtEndFlag();
	// PURPOSE: Callback on looping toggle. If looping is enabled, de-select holding
	void LoopEditorToggleCB();


	CAnimScene* m_pScene;
	CScreenExtents m_entityPaneExtents;
	CScreenExtents m_screenExtents;
	COnScreenTimeline m_timeline;
	CSceneInformationDisplay m_sceneInfoDisplay;
	COnScreenVideoControl m_videoPlayBackControl;  
	CAuthoringHelperDisplay m_authoringDisplay;
	CSectioningPanel m_sectionPanel;

	CGtaPickerInterfaceSettings* m_pickerSettings; 

	eFocusPanel m_currentFocusPanel;

	Selection m_selected;
	CAnimSceneEvent* m_pSelectedEventOnLastRelease;

	fwFlags32 m_flags;

	//atArray<EventLayer> m_eventLayers;
	CEventLayersPane m_EventLayersWindow; 
	CAnimSceneScrollbar m_eventsScrollbar;

	Widgets m_widgets;

	atArray<CAnimSceneEntityInitialiser*> m_entityInitialisers;

	atArray<CAnimSceneEventInitialiser*> m_eventInitialisers;	

	Color32 m_colors[kMaxNumColors];

	// Editor settings
	enum eTimelinePlaybackMode
	{
		kTimelinePlaybackMode_MaintainSelectorPosition = 0,	// Keeps the selector in the same visual position on the timeline when the play button was pressed
		kTimelinePlaybackMode_SelectorInCentre,				// Keeps the selector in the centre of the timeline
		kTimelinePlaybackMode_Static,						// Do not adjust the timeline.  Keep it static.

		kTimelinePlaybackMode_Num
	};

	enum eTimelineDisplayMode
	{
		kTimelineDisplayMode_Frames = 0,					// Display as frames
		kTimelineDisplayMode_Seconds,						// Display as seconds

		kTimelineDisplayMode_Num
	};

	// TODO - add these to a pargen structure and make it save / loadable?

	bool m_editorSettingPauseSceneAtFirstStart;
	bool m_editorSettingAutoRegisterPlayer;

	bool m_editorSettingResetSceneOnPlaybackListChange;

	int m_editorSettingTimelinePlaybackMode;
	int m_editorSettingTimelineDisplayMode;
	bool m_editorSettingTimelineSnapToFrames;

	bool m_editorSettingEventEditorSnapToFrames;

	bool m_editorSettingEditEventPreloads;

	float m_fSelectorTimelinePositionUponPlayback;			// This is the position of the selector on the timeline when we hit play
	float m_EventLayersYScreenOffset;

	static bool m_editorSettingUseSmoothFont;
	static bool m_editorSettingLoopPlayback;
	static bool m_editorSettingHoldAtLastFrame;
	static bool m_editorSettingHideEvents;
	bool m_bAllowWidgetAutoRegen;
	bool m_firstPlayback;

	struct MouseState{
		
		MouseState();

		Vector2 m_mouseCoords;
		Vector2 m_lastMouseCoords;
		bool m_leftMouseDown;
		bool m_leftMousePressed;
		bool m_leftMouseReleased;
		bool m_rightMouseDown;
		bool m_rightMousePressed;
		bool m_rightMouseReleased;

		void Update();
	};

	MouseState m_mouseState;

};

#endif //__BANK

#endif //_ANIM_SCENE_EDITOR_H_
