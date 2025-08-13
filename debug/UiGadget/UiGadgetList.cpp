/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    debug/UiGadget/UiGadgetList.cpp
// PURPOSE : a scrolly list of text gadgets
// AUTHOR :  Ian Kiigan
// CREATED : 22/11/10
//
/////////////////////////////////////////////////////////////////////////////////

#include "debug/UiGadget/UiGadgetList.h"

#if __BANK

// TODO make these configure per-instance (probably taking the padding values from some kind of layout scheme?
#define UIGADGETLIST_ENTRY_H			(12.0f)
#define UIGADGETLIST_TEXT_OFFSET_X		(2.0f)
#define UIGADGETLIST_TEXT_OFFSET_Y		(3.0f)
#define UIGADGETLIST_SCROLLBAR_OFFSET_X	(2.0f)
#define UIGADGETLIST_SCROLLBAR_W		(10.0f)
#define UIGADGETLIST_MAX_KEYBOARD_DELTA	(20)

float CUiGadgetSimpleListAndWindow::ms_afDefaultColumnOffsets[] = { 0.0f };

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	CUiGadgetScrollBar
// PURPOSE:		ctor
//////////////////////////////////////////////////////////////////////////
CUiGadgetScrollBar::CUiGadgetScrollBar(float fX, float fY, float fWidth, float fHeight, const CUiColorScheme& colorScheme)
:	CUiGadgetBox(fX, fY, fWidth, fHeight, colorScheme.GetColor(CUiColorScheme::COLOR_SCROLLBAR_BG)), 
	m_box(fX, fY, fWidth, fHeight, colorScheme.GetColor(CUiColorScheme::COLOR_SCROLLBAR_FG)),
	m_state(STATE_NORMAL)
{
	SetCanHandleEventsOfType(UIEVENT_TYPE_MOUSEL_PRESSED);
	SetCanHandleEventsOfType(UIEVENT_TYPE_MOUSEL_RELEASED);
	m_box.AttachToParent(this);
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	SetState
// PURPOSE:
//////////////////////////////////////////////////////////////////////////
void CUiGadgetScrollBar::SetState(eState newState)
{
	switch (newState)
	{
	case STATE_BEINGDRAGGED:
		m_vClickDragDelta = g_UiEventMgr.GetCurrentMousePos() - m_box.GetPos();
		break;
	case STATE_NORMAL:
		break;
	default:
		break;
	}
	m_state = newState;
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	Update
// PURPOSE:		updates position of scrollbar when dragging etc
//////////////////////////////////////////////////////////////////////////
void CUiGadgetScrollBar::Update()
{
	switch (m_state)
	{
	case STATE_BEINGDRAGGED:
		{
			float fHeight = m_box.GetHeight();
			Vector2 vMousePos = g_UiEventMgr.GetCurrentMousePos();
			float fNewY = (vMousePos - m_vClickDragDelta).y;
			if (fNewY < m_fMinY) { fNewY = m_fMinY; }
			else if ((fNewY+fHeight)>m_fMaxY) { fNewY = m_fMaxY-fHeight; }
			m_box.SetPos(Vector2(m_vPos.x, fNewY));
		}
		break;
	case STATE_NORMAL:
		break;
	default:
		break;
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	HandleEvent
// PURPOSE:		check for mouse events, used for dragging window etc
//////////////////////////////////////////////////////////////////////////
void CUiGadgetScrollBar::HandleEvent(const CUiEvent& uiEvent)
{
	switch (m_state)
	{
	case STATE_BEINGDRAGGED:
		if (uiEvent.IsSet(UIEVENT_TYPE_MOUSEL_RELEASED))
		{
			SetState(STATE_NORMAL);
		}
		break;
	case STATE_NORMAL:
		if (uiEvent.IsSet(UIEVENT_TYPE_MOUSEL_PRESSED) && m_box.ContainsMouse())
		{
			SetState(STATE_BEINGDRAGGED);
		}
		break;
	default:
		break;
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	CUiGadgetList
// PURPOSE:		ctor
//////////////////////////////////////////////////////////////////////////
CUiGadgetList::CUiGadgetList(float fX, float fY, float fWidth, u32 numRows, u32 numColumns, float* columnOffsets, const CUiColorScheme& colorScheme, const char** apszColumnNames)
:	m_fWidth(fWidth), m_numRows(numRows), m_numColumns(numColumns),
	m_selectorBox(fX, fY, fWidth, UIGADGETLIST_ENTRY_H, colorScheme.GetColor(CUiColorScheme::COLOR_LIST_SELECTOR)),
	m_scrollBar(fX+fWidth+UIGADGETLIST_SCROLLBAR_OFFSET_X, fY, UIGADGETLIST_SCROLLBAR_W, numRows*UIGADGETLIST_ENTRY_H, colorScheme),
	m_setEntryCB(NULL), m_extraCallbackData( NULL), m_keyboardDelta(0), m_bClickedNewEntry(false), m_bSelectorEnabled(true), m_bRequestSetViewportToSelected(false), m_selectBehaviourToggles( true )
{
	SetPos(Vector2(fX, fY));
	Color32 bg1 = colorScheme.GetColor(CUiColorScheme::COLOR_LIST_BG1);
	Color32 bg2 = colorScheme.GetColor(CUiColorScheme::COLOR_LIST_BG2);

	// create parent widgets (reverse order for now, because ptrlist always adds to head)
	m_textRoot.AttachToParent(this);
	m_selectorRoot.AttachToParent(this);
	m_bgRoot.AttachToParent(this);

	// create background boxes
	for (u32 i=0; i<numRows; i++)
	{
		m_aBgBoxes.PushAndGrow(CUiGadgetBox(fX, fY+i*UIGADGETLIST_ENTRY_H, fWidth, UIGADGETLIST_ENTRY_H, ((i&1) ? bg1 : bg2)));
	}
	for (u32 i=0; i<m_aBgBoxes.size(); i++)
	{
		m_aBgBoxes[i].AttachToParent(&m_bgRoot);
	}

	// create text fields
	for (u32 i=0; i<numRows*numColumns; i++)
	{
		u32 col = i%numColumns;
		u32 row = i/numColumns;
		float fTextX = UIGADGETLIST_TEXT_OFFSET_X + fX + columnOffsets[col];
		float fTextY = UIGADGETLIST_TEXT_OFFSET_Y + fY + row*UIGADGETLIST_ENTRY_H;
		m_aTextFields.PushAndGrow(CUiGadgetText(fTextX, fTextY, colorScheme.GetColor(CUiColorScheme::COLOR_LIST_ENTRY_TEXT), ""));
	}
	for (u32 i=0; i<m_aTextFields.size(); i++)
	{
		m_aTextFields[i].AttachToParent(&m_textRoot);
	}

	// create column titles if required
	if (apszColumnNames)
	{
		for (u32 i=0; i<numColumns; i++)
		{
			float fColumnTitleX = UIGADGETLIST_TEXT_OFFSET_X + fX + columnOffsets[i];
			float fColumnTitleY = UIGADGETLIST_TEXT_OFFSET_Y + fY - UIGADGETLIST_ENTRY_H;
			m_aColumnNames.PushAndGrow(CUiGadgetText(fColumnTitleX, fColumnTitleY, colorScheme.GetColor(CUiColorScheme::COLOR_LIST_ENTRY_TEXT), apszColumnNames[i]));
		}
	}
	for (u32 i=0; i<m_aColumnNames.size(); i++)
	{
		m_aColumnNames[i].AttachToParent(&m_textRoot);
	}

	m_selectorBox.AttachToParent(&m_selectorRoot);
	m_scrollBar.AttachToParent(this);
	m_scrollBar.SetMinAndMax(fY, fY+numRows*UIGADGETLIST_ENTRY_H);

	SetHasFocus(false);
	SetNumEntries(0);

	SetCanHandleEventsOfType(UIEVENT_TYPE_MOUSEL_PRESSED);
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	~CUiGadgetList
// PURPOSE:		dtor (TODO - how should gadget trees be shut down and destroyed?)
//////////////////////////////////////////////////////////////////////////
CUiGadgetList::~CUiGadgetList()
{
	DetachAllChildren();
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	GetBounds
// PURPOSE:
//////////////////////////////////////////////////////////////////////////
fwBox2D CUiGadgetList::GetBounds() const
{
	Vector2 vPos = GetPos();
	return fwBox2D(vPos.x, vPos.y, vPos.x+m_fWidth, vPos.y+m_numRows*UIGADGETLIST_ENTRY_H);
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	SetNumEntries
// PURPOSE:		sets the total number of list entries and updates viewport
//////////////////////////////////////////////////////////////////////////
void CUiGadgetList::SetNumEntries(u32 numEntries)
{
	m_numEntries = numEntries;

	if (numEntries>0)
	{
		if (HasFocus())
		{
			m_indexInList = 0;
		}
		else
		{
			m_indexInList = -1;
		}
		m_viewportIndex = 0;
	}
	else
	{
		m_indexInList = -1;
		m_viewportIndex = -1;
	}

	// update scrollbar
	m_scrollBar.SetPosToTop();
	if (m_numEntries<=m_numRows)
	{
		m_scrollBar.GetDraggableBox().SetSize(UIGADGETLIST_SCROLLBAR_W, m_numRows*UIGADGETLIST_ENTRY_H);
	}
	else
	{
		const float kMinScrollBarSize = 10.0f; // scroll bars less than some minimum are too small to see/manipulate

		float fMaxHeight = m_numRows*UIGADGETLIST_ENTRY_H;
		float fNewHeight = Max<float>(kMinScrollBarSize, fMaxHeight*m_numRows/m_numEntries);
		m_scrollBar.GetDraggableBox().SetSize(UIGADGETLIST_SCROLLBAR_W, fNewHeight);
	}
}

//////////////////////////////////////////////////////////////////////////
// PURPOSE:		HandleEvent
// PURPOSE:		processes mouse click events
//////////////////////////////////////////////////////////////////////////
void CUiGadgetList::HandleEvent(const CUiEvent& uiEvent)
{
	if (uiEvent.IsSet(UIEVENT_TYPE_MOUSEL_PRESSED))
	{
		Vector2 vMousePos = g_UiEventMgr.GetCurrentMousePos();

		if (GetBounds().IsInside(vMousePos) && m_numEntries && IsActive())
		{
			s32 oldSelectedIndex = GetCurrentIndex();
		
			fwBox2D bounds = GetBounds();
			const int possibleEntry = (int) ((vMousePos.y - bounds.y0) / UIGADGETLIST_ENTRY_H);
			m_indexInList = Clamp<int>(m_viewportIndex+possibleEntry, 0, m_numEntries-1);

			if ( oldSelectedIndex == m_indexInList )
			{
				m_indexInList = m_selectBehaviourToggles ? -1 : m_indexInList;
			}
			else
			{
				m_bClickedNewEntry = true;
			}
			
			CUiGadgetBase* pParent = GetParent();
			if (pParent)
			{
				pParent->SetNewChildFocus(this);
			}
		}
		else
		{
			SetHasFocus(false);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	Update
// PURPOSE:		updates state of list, scroll bar position etc
//////////////////////////////////////////////////////////////////////////
void CUiGadgetList::Update()
{
	// keyboard
	if (g_UiEventMgr.GetCurrentKeyUp())
	{
		m_keyboardDelta = Max(m_keyboardDelta-1, -UIGADGETLIST_MAX_KEYBOARD_DELTA);
	}
	else if (g_UiEventMgr.GetCurrentKeyDown())
	{
		m_keyboardDelta = Min(m_keyboardDelta+1, UIGADGETLIST_MAX_KEYBOARD_DELTA);
	}
	else
	{
		m_keyboardDelta = 0;
	}

	Vector2 vPos = GetPos();
	/// fwBox2D bounds = GetBounds();

	// update scroll bar limits
	m_scrollBar.SetMinAndMax(vPos.y, vPos.y+m_numRows*UIGADGETLIST_ENTRY_H);

	// update scrollbar's effect on current view of list
	s32 oldIndex = m_viewportIndex;
	if (m_numEntries && m_numEntries>m_numRows && m_scrollBar.IsScrolling())
	{
		float fMinPos = 0.0f;
		float fMaxPos = m_scrollBar.GetHeight() - m_scrollBar.GetDraggableBox().GetHeight();
		float fCurPos = m_scrollBar.GetDraggableBox().GetPos().y - vPos.y;
		s32 minIndex = 0;
		s32 maxIndex = m_numEntries - m_numRows;
		s32 newIndex = (s32) ( (float)minIndex + (fCurPos - fMinPos)/(fMaxPos - fMinPos)*(float)(maxIndex - minIndex + 1) );
		m_viewportIndex = Clamp<s32>(newIndex, 0, maxIndex);
	}
	
	// force viewport to show selected index, if requested
	if (m_bRequestSetViewportToSelected && m_indexInList>=0)
	{
		if (m_numRows>m_numEntries)
		{
			m_viewportIndex = 0;
		}
		else
		{
			s32 newIndex = m_indexInList;
			s32 maxIndex = m_numEntries - m_numRows;
			m_viewportIndex = Clamp<s32>(newIndex, 0, maxIndex);
		}
		m_bRequestSetViewportToSelected = false;
	}

	// no movement from scroll bar, so check mouse wheel and keyboard
	if (m_bSelectorEnabled && HasFocus() && oldIndex==m_viewportIndex)
	{
		s32 mouseWheel = -1*g_UiEventMgr.GetCurrentMouseWheel();
		bool bIndexChanged = false;
		s32 oldIndexInList = m_indexInList;
		if (mouseWheel != 0)
		{
			m_indexInList = Clamp<s32>( m_indexInList+mouseWheel, 0, m_numEntries-1);
			bIndexChanged = true;
		}
		else if (m_keyboardDelta != 0)
		{
			m_indexInList = Clamp<s32>( m_indexInList+m_keyboardDelta, 0, m_numEntries-1);
			bIndexChanged = true;
		}

		if (bIndexChanged)
		{
			if (m_indexInList < m_viewportIndex) { m_viewportIndex = m_indexInList; }
			else if (m_indexInList >= (m_viewportIndex+m_numRows)) { m_viewportIndex = m_indexInList-m_numRows+1; }

			if (m_indexInList!=oldIndexInList)
			{
				m_bClickedNewEntry = true;
			}
		}
	}

	// update scrollbar position
	if (oldIndex!=m_viewportIndex)
	{
		int minIndex = 0;
		int maxIndex = m_numEntries - m_numRows;
		int curIndex = m_viewportIndex;
		float fMinPos = 0.0f;
		float fMaxPos = m_scrollBar.GetHeight() - m_scrollBar.GetDraggableBox().GetHeight();
		float fCurPos = fMinPos + (fMaxPos - fMinPos)*(float)(curIndex - minIndex)/(float)(maxIndex - minIndex);
		m_scrollBar.GetDraggableBox().SetPos(Vector2(m_scrollBar.GetPos().x, m_scrollBar.GetPos().y + fCurPos));
	}

	// update selector position and visibility
	if (m_bSelectorEnabled && m_numEntries>0 && CanSeeSelector())
	{
		m_selectorBox.SetFaded(HasFocus()==false);
		u32 selectorIndex = (m_indexInList-m_viewportIndex);
		m_selectorBox.SetPos(Vector2(vPos.x, vPos.y+selectorIndex*UIGADGETLIST_ENTRY_H));
		m_selectorBox.SetActive(true);
	}
	else
	{
		m_selectorBox.SetActive(false);
	}

	// update value of cells
	if (m_setEntryCB!=NULL)
	{
		for (u32 row=0; row<m_numRows; row++)
		{
			for (u32 col=0; col<m_numColumns; col++)
			{
				u32 listIndex = m_viewportIndex+row;
				if (listIndex<m_numEntries)
				{
					m_setEntryCB(&m_aTextFields[col+row*m_numColumns], listIndex, col, m_extraCallbackData );
				}
				else
				{
					m_aTextFields[col+row*m_numColumns].SetString("");
				}
			}
		}	
	}
}

#endif	//__BANK
