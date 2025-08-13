/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    debug/DebugDraw/DebugWindow.h
// PURPOSE : displays onscreen windows for displaying a list of results etc
// AUTHOR :  Ian Kiigan
// CREATED : 25/10/10
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef _DEBUG_DEBUGDRAW_DEBUGWINDOW_H_
#define _DEBUG_DEBUGDRAW_DEBUGWINDOW_H_

#define USE_OLD_DEBUGWINDOW (0)

#if USE_OLD_DEBUGWINDOW

// How to use:
//
// CDebugWindow* pWindow = new CDebugWindow(200.0f, 200.0f, 300.0f, 6, GetNumEntriesCB, GetEntryTextCB);
// pWindow->AddColumn("Column1");
// pWindow->AddColumn("Column2");
// pWindow->SetVisible(true);

#if __BANK
#include "atl/array.h"
#include "atl/string.h"
#include "fwmaths/Rect.h"
#include "fwutil/PtrList.h"
#include "vector/color32.h"
#include "vector/vector2.h"

// represents a column in a list of results
class CDebugWindowColumn
{
public:
	CDebugWindowColumn() {}
	CDebugWindowColumn(const char* name, float fOffsetX);

	void        SetName(const char* name) { m_name = name; }
	const char* GetName() const { return m_name.c_str(); }
	void        SetOffsetX(float offsetX) { m_fOffsetX = offsetX; }
	float       GetOffsetX() const { return m_fOffsetX; }

private:
	atString m_name;
	float    m_fOffsetX;
};

// a simple scrollable window suitable for displaying a list of entries
class CDebugWindow
{
private:
	~CDebugWindow();

public:
	enum eDebugWindowColourType
	{
		DEBUGWINDOW_COLOUR_BORDER            ,
		DEBUGWINDOW_COLOUR_TITLE_BG          ,
		DEBUGWINDOW_COLOUR_TITLE_TEXT        ,
		DEBUGWINDOW_COLOUR_TITLE_SHADOW      ,
		DEBUGWINDOW_COLOUR_TITLE_CLOSEBOX_BG ,
		DEBUGWINDOW_COLOUR_TITLE_CLOSEBOX_FG ,
		DEBUGWINDOW_COLOUR_SPACER            ,
		DEBUGWINDOW_COLOUR_LIST_CATEGORY_BG  ,
		DEBUGWINDOW_COLOUR_LIST_CATEGORY_TEXT,
		DEBUGWINDOW_COLOUR_LIST_ENTRY_BG     ,
		DEBUGWINDOW_COLOUR_LIST_ENTRY_TEXT   ,
		DEBUGWINDOW_COLOUR_LIST_SELECTOR     ,
		DEBUGWINDOW_COLOUR_LIST_SCROLLBAR_BG ,
		DEBUGWINDOW_COLOUR_LIST_SCROLLBAR_FG ,

		DEBUGWINDOW_COLOUR_MAX
	};

	enum { DEBUGWINDOWCOLUMN_ENTRY_MAX_CHARS = 256 };

	// an event system?
	typedef int  (*GetNumEntriesCB )(void);
	typedef void (*GetEntryTextCB  )(char*    result, int row, int column);
	typedef bool (*GetEntryColourCB)(Color32* result, int row, int column);
	typedef bool (*GetRowActiveCB  )(                 int row);
	typedef void (*ClickCategoryCB )(                 int row, int column);
	typedef void (*CloseWindowCB   )(void);

	CDebugWindow(float fX, float fY, float fWidth, float fPreviewWidth, int numSlots, GetNumEntriesCB getNumEntriesCB, GetEntryTextCB getEntryTextCB);
	void Release();

	fwBox2D GetWindowBounds () const;
	fwBox2D GetListBounds   () const;
	fwBox2D GetPreviewBounds() const;

	CDebugWindowColumn& AddColumn(const char* name, float fOffsetX) { m_columns.PushAndGrow(CDebugWindowColumn(name, fOffsetX)); return m_columns.back(); }
	CDebugWindowColumn& GetColumn(int columnIndex) { return m_columns[columnIndex]; }

	void DeleteColumns() { m_columns.clear(); }

	const CDebugWindowColumn& GetColumn(int columnIndex) const { return m_columns[columnIndex]; }

	void           SetGetEntryColourCB(GetEntryColourCB cb) { m_getEntryColourCB = cb; }
	void           SetGetRowActiveCB(GetRowActiveCB cb) { m_getRowActiveCB = cb; }
	void           SetClickCategoryCB(ClickCategoryCB cb) { m_clickCategoryCB = cb; }
	void           SetCloseWindowCB(CloseWindowCB cb) { m_closeWindowCB = cb; }
	void           SetTitle(const char* title) { m_title = title; }
	const char*    GetTitle() const { return m_title.c_str(); }
	void           SetTitleHeight(float titleHeight) { m_fTitleHeight = titleHeight; }
	float          GetTitleHeight() const { return m_fTitleHeight; }
	void           SetTitleOffsetX(float titleOffsetX) { m_fTitleOffsetX = titleOffsetX; }
	float          GetTitleOffsetX() const { return m_fTitleOffsetX; }
	void           SetWidth(float fWidth) { m_fWidth = fWidth; }
	float          GetWidth() const { return m_fWidth; }
	void           SetPreviewWidth(float fPreviewWidth) { m_fPreviewWidth = fPreviewWidth; }
	float          GetPreviewWidth() const { return m_fPreviewWidth; }
	void           SetIsActive(bool bIsActive) { m_bIsActive = bIsActive; }
	bool           GetIsActive() const { return m_bIsActive; }
	void           SetIsVisible(bool bIsVisible) { m_bIsVisible = bIsVisible; }
	bool           GetIsVisible() const { return m_bIsVisible; }
	void           SetPos(const Vector2& position) { m_vPosition = position; }
	const Vector2& GetPos() const { return m_vPosition; }
	void           SetNumSlots(int numSlots) { m_numSlots = numSlots; }
	int            GetNumSlots() const { return m_numSlots; }
	void           SetCurrentIndex(int index) { m_selectorEnabled = true; if (m_numEntriesInList > 0) { m_indexInList = Clamp<int>(index, 0, m_numEntriesInList - 1); } }
	int            GetCurrentIndex() const { return m_selectorEnabled ? m_indexInList : -1; }
	void           SetSelectorEnabled(bool bEnabled) { m_selectorEnabled = bEnabled; }
	bool           GetSelectorEnabled() const { return m_selectorEnabled; }
	void           SetBorder(float border) { m_fBorder = border; }
	float          GetBorder() const { return m_fBorder; }
	void           SetSpacing(float spacing) { m_fSpacing = spacing; }
	float          GetSpacing() const { return m_fSpacing; }
	void           SetScale(float scale) { m_fScale = scale; }
	float          GetScale() const { return m_fScale; }
	void           SetOffset(const Vector2& offset) { m_vOffset = offset; }
	const Vector2& GetOffset() const { return m_vOffset; }

	void           SetColour(eDebugWindowColourType e, const Color32& color);
	Color32        GetColour(eDebugWindowColourType e, float opacity) const;

	static float   GetDefaultListEntryHeight();

protected:
	//virtual int GetNumEntries() = 0;
	//virtual void GetEntryText(char* achResult, int row, int column) = 0;

private:
	void Draw();
	void Update();

	atString m_title;
	float    m_fTitleHeight;
	float    m_fTitleOffsetX;
	Vector2  m_vPosition;
	Vector2  m_vOffset; // for debugging
	Vector2  m_vClickDragDelta;
	float    m_fWidth;
	float    m_fPreviewWidth;
	float    m_fBorder;
	float    m_fSpacing;
	float    m_fScale;
	bool     m_bIsMoving;
	bool     m_bIsActive;
	bool     m_bIsVisible;
	Color32  m_colours[DEBUGWINDOW_COLOUR_MAX];
	atArray<CDebugWindowColumn> m_columns;
	int      m_numSlots;           // num of entries displayed onscreen
	bool     m_selectorEnabled;    // whether the selector is enabled, if this is false then GetCurrentIndex() will return -1
	int      m_selectorIndex;      // index of selector from 0 to m_numSlots-1 (should be [0..min(m_numSlots,m_numEntriesInList)-1]?)
	int      m_indexInList;        // index of current selected entry from 0 to m_numEntriesInList-1 (or -1 if m_numEntriesInList is 0)
	int      m_slotZeroEntryIndex; // index of slot 0 in the overall list from 0 to m_numEntriesInList-1 (or -1 if m_numEntriesInList is 0)
	int      m_numEntriesInList;

	GetNumEntriesCB  m_getNumEntriesCB;
	GetEntryTextCB   m_getEntryTextCB;
	GetEntryColourCB m_getEntryColourCB;
	GetRowActiveCB   m_getRowActiveCB;
	ClickCategoryCB  m_clickCategoryCB;
	CloseWindowCB    m_closeWindowCB;

	friend class CDebugWindowMgr;
};

// responsible for updating and drawing a number of registered CDebugWindow objects
class CDebugWindowMgr
{
public:
	CDebugWindowMgr() : m_bDeleteOccurred(false) {}
	~CDebugWindowMgr() { m_windowList.Flush(); }
	void Register(CDebugWindow* pWindow) { m_windowList.Add(pWindow); }
	void Unregister(CDebugWindow* pWindow) { m_windowList.Remove(pWindow); m_bDeleteOccurred = true; }
	void Draw();
	void Update();
	static CDebugWindowMgr& GetMgr() { return ms_wMgr; }

	static void SetLeftMouseButtonHandled(bool bHandled);
	static bool GetLeftMouseButtonHandled();

private:
	static bool ms_bEnabled;
	static int ms_LeftMouseButtonHandledCooldown;
	fwPtrListSingleLink m_windowList;
	static CDebugWindowMgr ms_wMgr;
	bool m_bDeleteOccurred;
};

#endif //__BANK
#endif // USE_OLD_DEBUGWINDOW
#endif //_DEBUG_DEBUGDRAW_DEBUGWINDOW_H_
