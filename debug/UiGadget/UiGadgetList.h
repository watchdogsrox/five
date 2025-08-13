/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    debug/UiGadget/UiGadgetList.h
// PURPOSE : a scrolly list of text gadgets
// AUTHOR :  Ian Kiigan
// CREATED : 22/11/10
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef _DEBUG_UIGADGET_UIGADGETLIST_H_
#define _DEBUG_UIGADGET_UIGADGETLIST_H_

#if __BANK

#include "debug/UiGadget/UiColorScheme.h"
#include "debug/UiGadget/UiGadgetBase.h"
#include "debug/UiGadget/UiGadgetBox.h"
#include "debug/UiGadget/UiGadgetText.h"
#include "debug/UiGadget/UiGadgetWindow.h"
#include "vector/color32.h"
#include "atl/array.h"
#include "atl/string.h"

// a draggable scroll bar
class CUiGadgetScrollBar : public CUiGadgetBox
{
public:
	CUiGadgetScrollBar(float fX, float fY, float fWidth, float fHeight, const CUiColorScheme& colorScheme);
	virtual ~CUiGadgetScrollBar() {}
	inline void SetPosToTop() { m_box.SetPos(Vector2(m_vPos.x, m_fMinY)); }
	inline CUiGadgetBox& GetDraggableBox() { return m_box; }
	virtual void Update();
	virtual void HandleEvent(const CUiEvent& uiEvent);
	inline void SetMinAndMax(float fMinY, float fMaxY) { m_fMinY = fMinY; m_fMaxY = fMaxY; }
	inline bool IsScrolling() { return m_state==STATE_BEINGDRAGGED; }

private:
	enum eState
	{
		STATE_NORMAL,
		STATE_BEINGDRAGGED		
	};
	void SetState(eState newState);
	eState m_state;
	Vector2 m_vClickDragDelta;
	float m_fMinY, m_fMaxY;
	CUiGadgetBox m_box;
};

// simple scrolly list
class CUiGadgetList : public CUiGadgetBase
{
public:
	typedef void (*SetEntryCB)(CUiGadgetText* pResult, u32 row, u32 col, void* extraCallbackData );

	CUiGadgetList(float fX, float fY, float fWidth, u32 numRows, u32 numColumns, float* columnOffsets, const CUiColorScheme& colorScheme, const char** apszColumnNames=NULL);
	virtual ~CUiGadgetList();
	virtual void Draw() { }
	virtual void Update();
	virtual void HandleEvent(const CUiEvent& uiEvent);
	virtual fwBox2D GetBounds() const;
	u32 GetNumEntries() const { return m_numEntries; }
	void SetNumEntries(u32 numEntries);
	inline void SetCell(u32 col, u32 row, const char* pszText) { m_aTextFields[col+row*m_numColumns].SetString(pszText); SetCellVisible( col, row, true ); }
	inline void SetCellVisible(u32 col, u32 row, bool visible ) 
	{ 
		m_aBgBoxes[col+row*m_numColumns].SetActive( visible ); 
		m_aTextFields[col+row*m_numColumns].SetActive( visible ); 
	}
	inline s32 GetCurrentIndex() const { return m_indexInList; }
	inline void SetCurrentIndex(s32 idx) { m_indexInList = idx; }
	inline void SetUpdateCellCB(SetEntryCB cb, void* extraCallbackData = NULL) { m_setEntryCB = cb; m_extraCallbackData = extraCallbackData; }
	inline void SetSelectorEnabled(bool bEnabled) { m_bSelectorEnabled = bEnabled; }
	inline bool UserProcessClick() { if (m_bClickedNewEntry) { m_bClickedNewEntry = false; return true; } return false; }
	inline void RequestSetViewportToShowSelected() { m_bRequestSetViewportToSelected = true; }
	inline float GetWidth() const { return m_fWidth; }
	inline void SetScrollbarVisibility( bool visible ) { m_scrollBar.SetActive( visible ); }
	inline void SetSelectBehaviourToggles( bool selectBehaviourToggles ) { m_selectBehaviourToggles = selectBehaviourToggles; }

private:
	bool CanSeeSelector() const { return (m_indexInList>=m_viewportIndex && m_indexInList<m_viewportIndex+(s32)m_numRows); }

	atArray<CUiGadgetBox> m_aBgBoxes;
	atArray<CUiGadgetText> m_aTextFields;
	atArray<CUiGadgetText> m_aColumnNames;
	float m_fWidth;
	u32 m_numRows;
	u32 m_numColumns;
	u32 m_numEntries;
	CUiGadgetDummy m_bgRoot;
	CUiGadgetDummy m_selectorRoot;
	CUiGadgetDummy m_textRoot;
	CUiGadgetBox m_selectorBox;
	CUiGadgetScrollBar m_scrollBar;
	s32 m_indexInList;       // index of current selected entry from 0 to m_numEntries-1 (or -1 if m_numEntriesInList is 0)
	s32 m_viewportIndex;	 // index of row 0 in the overall list from 0 to m_numEntries-1 (or -1 if m_numEntries is 0)
	s32 m_keyboardDelta;
	bool m_bClickedNewEntry;
	bool m_bSelectorEnabled;
	bool m_bRequestSetViewportToSelected;
	bool m_selectBehaviourToggles;

	SetEntryCB  m_setEntryCB;
	void*		m_extraCallbackData;
};

// just a simple, single scrolly list in a window. use it by creating it
class CUiGadgetSimpleListAndWindow : public CUiGadgetList
{
public:
	CUiGadgetSimpleListAndWindow(const char* pszTitle, float fX, float fY, float fWidth, u32 numRows=DEFAULT_NUM_ROWS) :
	  m_window(fX, fY, fWidth, 3.0f*PADDING+(numRows+1)*ENTRY_HEIGHT, CUiColorScheme()),
	  CUiGadgetList(fX+PADDING, fY+ENTRY_HEIGHT+2*PADDING, fWidth-SCROLLBAR_WIDTH, numRows, 1, ms_afDefaultColumnOffsets, CUiColorScheme())
	{
		m_window.SetTitle(pszTitle);
		g_UiGadgetRoot.AttachChild(&m_window);
		m_window.AttachChild(this);
		m_window.SetActive(true);
	}
	~CUiGadgetSimpleListAndWindow()
	{
		m_window.DetachFromParent();
	}

	void UpdateTitle(const char* pszTitle) { m_window.SetTitle(pszTitle); }

	void SetDisplay(bool bDisplay)
	{
		m_window.SetActive(bDisplay);
	}

	void SetPos(const Vector2& vNewPos, bool bMoveChildren=false)
	{
		CUiGadgetList::SetPos(vNewPos);
		m_window.SetPos(vNewPos, bMoveChildren);
	}

private:
	CUiGadgetWindow m_window;
	enum
	{
		PADDING				= 2,
		ENTRY_HEIGHT		= 12,
		SCROLLBAR_WIDTH		= 16,
		DEFAULT_NUM_ROWS	= 12
	};
	static float ms_afDefaultColumnOffsets[1];
};

#endif	//__BANK

#endif	//_DEBUG_UIGADGET_UIGADGETLIST_H_
