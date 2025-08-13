// ===========================================
// debug/textureviewer/textureviewerwindow.cpp
// (c) 2011 RockstarNorth
// ===========================================

#if __BANK

#include "bank/bank.h" 
#include "bank/bkmgr.h"
#include "grcore/setup.h"
#include "grcore/viewport.h"
#include "grcore/font.h"
#include "grcore/debugdraw.h"
#include "input/mouse.h"

#include "fwutil/xmacro.h"

#include "system/controlmgr.h" // oops .. game dependency
#include "renderer/color.h"

#include "debug/textureviewer/textureviewerwindow.h"

//DEBUG_TEXTURE_VIEWER_OPTIMISATIONS()
RENDER_OPTIMISATIONS()

#define DebugTextureViewerWindow_SCREEN_COORDS(x,y) Vector2((float)(x)/grcViewport::GetDefaultScreen()->GetWidth(), (float)(y)/grcViewport::GetDefaultScreen()->GetHeight())
#define DebugTextureViewerWindow_TITLE_CLOSEBOX_SIZE (10.0f)
#define DebugTextureViewerWindow_ENTRY_H             (12.0f)
#define DebugTextureViewerWindow_SCROLLBAR_AREA_W    (10.0f)
#define DebugTextureViewerWindow_SCROLLBAR_OFFSET_X  ( 2.0f)
#define DebugTextureViewerWindow_TEXTOFFSET_Y        ( 2.0f)
#define DebugTextureViewerWindow_DEFAULTBORDER       ( 2.0f)
#define DebugTextureViewerWindow_DEFAULTSPACING      ( 2.0f)

#define DebugTextureViewerWindow_DEFAULTCOLOUR_BORDER             Color32(255, 255, 255, 200)
#define DebugTextureViewerWindow_DEFAULTCOLOUR_TITLE_BG           Color32(255,   0,   0, 255)
#define DebugTextureViewerWindow_DEFAULTCOLOUR_TITLE_TEXT         Color32(255, 255, 255, 255)
#define DebugTextureViewerWindow_DEFAULTCOLOUR_TITLE_SHADOW       Color32(  0,   0,   0, 128)
#define DebugTextureViewerWindow_DEFAULTCOLOUR_TITLE_CLOSEBOX_BG  Color32(  0,   0,  32, 255)
#define DebugTextureViewerWindow_DEFAULTCOLOUR_TITLE_CLOSEBOX_FG  Color32(255, 255, 255, 255)
#define DebugTextureViewerWindow_DEFAULTCOLOUR_SPACER             Color32(255, 255, 255, 200)
#define DebugTextureViewerWindow_DEFAULTCOLOUR_LIST_CATEGORY_BG   Color32(255, 255, 255, 200)
#define DebugTextureViewerWindow_DEFAULTCOLOUR_LIST_CATEGORY_TEXT Color32(  0,   0, 100, 255)
#define DebugTextureViewerWindow_DEFAULTCOLOUR_LIST_ENTRY_BG      Color32(180, 180, 180, 200)
#define DebugTextureViewerWindow_DEFAULTCOLOUR_LIST_ENTRY_TEXT    Color32(  0,   0,   0, 255)
#define DebugTextureViewerWindow_DEFAULTCOLOUR_LIST_SELECTOR      Color32(  0, 255, 255, 255)
#define DebugTextureViewerWindow_DEFAULTCOLOUR_LIST_SCROLLBAR_BG  Color32(255, 255, 255, 200)
#define DebugTextureViewerWindow_DEFAULTCOLOUR_LIST_SCROLLBAR_FG  Color32(  0,   0,   0, 255)

static void DebugTextureViewerWindow_DrawRect(const fwBox2D& rect, Color32 colour)
{
	grcDebugDraw::RectAxisAligned(DebugTextureViewerWindow_SCREEN_COORDS(rect.x0, rect.y0), DebugTextureViewerWindow_SCREEN_COORDS(rect.x1, rect.y1), colour, true);
}

static void DebugTextureViewerWindow_DrawShapeX(const fwBox2D& rect, Color32 colour)
{
	const Vector2 scale(1.0f/grcViewport::GetDefaultScreen()->GetWidth(), 1.0f/grcViewport::GetDefaultScreen()->GetHeight());
	const float d = 0.5f/8.0f;

	grcDebugDraw::Quad(
		scale*Vector2(rect.x1 - d, rect.y0    ),
		scale*Vector2(rect.x1    , rect.y0 + d),
		scale*Vector2(rect.x0 + d, rect.y1    ),
		scale*Vector2(rect.x0    , rect.y1 - d),
		colour
	);
	grcDebugDraw::Quad(
		scale*Vector2(rect.x0    , rect.y0 + d),
		scale*Vector2(rect.x0 + d, rect.y0    ),
		scale*Vector2(rect.x1    , rect.y1 - d),
		scale*Vector2(rect.x1 - d, rect.y1    ),
		colour
	);
}

static void DebugTextureViewerWindow_DrawShapeB(const fwBox2D& rect, Color32 colour)
{
	const Vector2 scale(1.0f/grcViewport::GetDefaultScreen()->GetWidth(), 1.0f/grcViewport::GetDefaultScreen()->GetHeight());
	const float d = 0.5f/8.0f;

	grcDebugDraw::Poly(
		scale*Vector2(rect.x1 - d - 1.0f, rect.y0 + d + 1.0f),
		scale*Vector2(rect.x0 + d, (rect.y0 + rect.y1)*0.5f),
		scale*Vector2(rect.x1 - d - 1.0f, rect.y1 - d - 1.0f),
		colour
	);
}

static void DebugTextureViewerWindow_DrawText(float x, float y, float scale, Color32 colour, const char* text)
{
	if (0) // testing .. text background rectangle
	{
		const float w = (float)grcDebugDraw::TextFontGet()->GetStringWidth(text, istrlen(text));
		const float h = (float)grcDebugDraw::TextFontGet()->GetHeight();

		DebugTextureViewerWindow_DrawRect(fwBox2D(x, y, x + w, y + h), Color32(0,0,255,128));
	}

	grcDebugDraw::TextFontPush(grcSetup::GetMiniFixedWidthFont());
	grcDebugDraw::Text(Vector2(x, y), DD_ePCS_Pixels, colour, text, false, scale, scale);
	grcDebugDraw::TextFontPop();
}

CDebugTextureViewerWindowMgr CDebugTextureViewerWindowMgr::ms_wMgr;
bool CDebugTextureViewerWindowMgr::ms_bEnabled = true;
int CDebugTextureViewerWindowMgr::ms_LeftMouseButtonHandledCooldown = 0;

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	CDebugTextureViewerWindowColumn
// PURPOSE:		ctor, takes an x offset for drawing and a name
//////////////////////////////////////////////////////////////////////////
CDebugTextureViewerWindowColumn::CDebugTextureViewerWindowColumn(const char* name, float fOffsetX) : m_name(name), m_fOffsetX(fOffsetX)
{
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	Draw
// PURPOSE:		draws all registered windows
//////////////////////////////////////////////////////////////////////////
void CDebugTextureViewerWindowMgr::Draw()
{
	if (!ms_bEnabled) { return; }

	fwPtrNodeSingleLink* pNode = m_windowList.GetHeadPtr();

	while (pNode)
	{
		CDebugTextureViewerWindow* pWindow = (CDebugTextureViewerWindow*)pNode->GetPtr();

		if (pWindow)
		{
			pWindow->Draw();
		}

		pNode = (fwPtrNodeSingleLink*)pNode->GetNextPtr();
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	Update
// PURPOSE:		updates all registered windows
//////////////////////////////////////////////////////////////////////////
void CDebugTextureViewerWindowMgr::Update()
{
	if (ms_LeftMouseButtonHandledCooldown > 0)
	{
		ms_LeftMouseButtonHandledCooldown--;
	}

	if (!ms_bEnabled) { return; }

	fwPtrNodeSingleLink* pNode = m_windowList.GetHeadPtr();

	while (pNode)
	{
		CDebugTextureViewerWindow* pWindow = (CDebugTextureViewerWindow*)pNode->GetPtr();

		if (pWindow)
		{
			pWindow->Update();
		}

		// advance to next node, or reset if a window has been unregistered during update
		if (m_bDeleteOccurred)
		{
			m_bDeleteOccurred = false;
			pNode = m_windowList.GetHeadPtr();
		}
		else
		{
			pNode = (fwPtrNodeSingleLink*)pNode->GetNextPtr();
		}
	}
}

void CDebugTextureViewerWindowMgr::SetLeftMouseButtonHandled(bool bHandled)
{
	ms_LeftMouseButtonHandledCooldown = (bHandled ? 3 : 0);
}

bool CDebugTextureViewerWindowMgr::GetLeftMouseButtonHandled()
{
	return ms_LeftMouseButtonHandledCooldown > 0;
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	CDebugTextureViewerWindow
// PURPOSE:		ctor, auto registers with manager
//////////////////////////////////////////////////////////////////////////
CDebugTextureViewerWindow::CDebugTextureViewerWindow(float fX, float fY, float fWidth, float fPreviewWidth, int numSlots, GetNumEntriesCB getNumEntriesCB, GetEntryTextCB getEntryTextCB)
	: m_title             ("Debug Window"            )
	, m_fTitleHeight      (12.0f                     )
	, m_fTitleOffsetX     (0.0f                      )
	, m_vOffset           (0.0f, 0.0f                )
	, m_vClickDragDelta   (0.0f, 0.0f                )
	, m_numSlots          (numSlots                  )
	, m_fWidth            (fWidth                    )
	, m_fPreviewWidth     (fPreviewWidth             )
	, m_fBorder           (DebugTextureViewerWindow_DEFAULTBORDER )
	, m_fSpacing          (DebugTextureViewerWindow_DEFAULTSPACING)
	, m_fScale            (1.0f                      )
	, m_bIsMoving         (false                     )
	, m_bIsActive         (false                     )
	, m_bIsVisible        (false                     )
	, m_selectorEnabled   (true                      )
	, m_selectorIndex     (0                         )
	, m_indexInList       (0                         )
	, m_numEntriesInList  (100                       )
	, m_slotZeroEntryIndex(0                         )
	, m_getNumEntriesCB   (getNumEntriesCB           )
	, m_getEntryTextCB    (getEntryTextCB            )
	, m_getEntryColourCB  (NULL                      )
	, m_getRowActiveCB    (NULL                      )
	, m_clickCategoryCB   (NULL                      )
	, m_clickBackCB       (NULL                      )
	, m_clickBackEnabledCB(NULL                      )
	, m_closeWindowCB     (NULL                      )
{
	SetPos(Vector2(fX, fY));

	SetColour(DebugTextureViewerWindow_COLOUR_BORDER            , DebugTextureViewerWindow_DEFAULTCOLOUR_BORDER            );
	SetColour(DebugTextureViewerWindow_COLOUR_TITLE_BG          , DebugTextureViewerWindow_DEFAULTCOLOUR_TITLE_BG          );
	SetColour(DebugTextureViewerWindow_COLOUR_TITLE_TEXT        , DebugTextureViewerWindow_DEFAULTCOLOUR_TITLE_TEXT        );
	SetColour(DebugTextureViewerWindow_COLOUR_TITLE_SHADOW      , DebugTextureViewerWindow_DEFAULTCOLOUR_TITLE_SHADOW      );
	SetColour(DebugTextureViewerWindow_COLOUR_TITLE_CLOSEBOX_BG , DebugTextureViewerWindow_DEFAULTCOLOUR_TITLE_CLOSEBOX_BG );
	SetColour(DebugTextureViewerWindow_COLOUR_TITLE_CLOSEBOX_FG , DebugTextureViewerWindow_DEFAULTCOLOUR_TITLE_CLOSEBOX_FG );
	SetColour(DebugTextureViewerWindow_COLOUR_SPACER            , DebugTextureViewerWindow_DEFAULTCOLOUR_SPACER            );
	SetColour(DebugTextureViewerWindow_COLOUR_LIST_CATEGORY_BG  , DebugTextureViewerWindow_DEFAULTCOLOUR_LIST_CATEGORY_BG  );
	SetColour(DebugTextureViewerWindow_COLOUR_LIST_CATEGORY_TEXT, DebugTextureViewerWindow_DEFAULTCOLOUR_LIST_CATEGORY_TEXT);
	SetColour(DebugTextureViewerWindow_COLOUR_LIST_ENTRY_BG     , DebugTextureViewerWindow_DEFAULTCOLOUR_LIST_ENTRY_BG     );
	SetColour(DebugTextureViewerWindow_COLOUR_LIST_ENTRY_TEXT   , DebugTextureViewerWindow_DEFAULTCOLOUR_LIST_ENTRY_TEXT   );
	SetColour(DebugTextureViewerWindow_COLOUR_LIST_SELECTOR     , DebugTextureViewerWindow_DEFAULTCOLOUR_LIST_SELECTOR     );
	SetColour(DebugTextureViewerWindow_COLOUR_LIST_SCROLLBAR_BG , DebugTextureViewerWindow_DEFAULTCOLOUR_LIST_SCROLLBAR_BG );
	SetColour(DebugTextureViewerWindow_COLOUR_LIST_SCROLLBAR_FG , DebugTextureViewerWindow_DEFAULTCOLOUR_LIST_SCROLLBAR_FG );

	CDebugTextureViewerWindowMgr::GetMgr().Register(this);
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	~CDebugTextureViewerWindow
// PURPOSE:		dtor, auto unregisters with manager
//////////////////////////////////////////////////////////////////////////
CDebugTextureViewerWindow::~CDebugTextureViewerWindow()
{
	if (m_closeWindowCB)
	{
		m_closeWindowCB();
	}

	CDebugTextureViewerWindowMgr::GetMgr().Unregister(this);
}

void CDebugTextureViewerWindow::Release()
{
	delete this;
}

void CDebugTextureViewerWindow::SetColour(eDebugWindowColourType e, const Color32& color)
{
	Assert((int)e >= 0 && (int)e < DebugTextureViewerWindow_COLOUR_MAX);

	m_colours[e] = color;
}

Color32 CDebugTextureViewerWindow::GetColour(eDebugWindowColourType e, float opacity) const
{
	Assert((int)e >= 0 && (int)e < DebugTextureViewerWindow_COLOUR_MAX);

	Color32 temp = m_colours[e];

	if (opacity != 1.0f)
	{
		temp.SetAlpha((int)Clamp<float>(temp.GetAlphaf()*opacity*255.0f + 0.5f, 0.0f, 255.0f));
	}

	return temp;
}

fwBox2D CDebugTextureViewerWindow::GetWindowBounds() const // includes borders etc.
{
	const float titleSizeY = m_fScale*m_fTitleHeight;

	const float listSizeX = m_fScale*m_fWidth;
	const float listSizeY = m_fScale*(float)(m_numSlots + 1)*DebugTextureViewerWindow_ENTRY_H; // includes category region

	const float previewSizeX = m_fScale*m_fPreviewWidth;

	const float windowSizeX = m_fBorder + listSizeX  + 0*m_fSpacing + previewSizeX + m_fBorder;
	const float windowSizeY = m_fBorder + titleSizeY +   m_fSpacing + listSizeY    + m_fBorder;

	const float windowOriginX = m_vPosition.x + m_vOffset.x - m_fBorder;
	const float windowOriginY = m_vPosition.y + m_vOffset.y - m_fBorder;

	return fwBox2D(
		windowOriginX,
		windowOriginY,
		windowOriginX + windowSizeX,
		windowOriginY + windowSizeY
	);
}

fwBox2D CDebugTextureViewerWindow::GetListBounds() const // does not include category region
{
	const float titleSizeY = m_fScale*m_fTitleHeight;

	const float listSizeX = m_fScale*m_fWidth;
	const float listSizeY = m_fScale*(float)(m_numSlots + 1)*DebugTextureViewerWindow_ENTRY_H; // includes category region

//	const float previewSizeX = m_fScale*m_fPreviewWidth;

//	const float windowSizeX = m_fBorder + listSizeX  + 0*m_fSpacing + previewSizeX + m_fBorder;
	const float windowSizeY = m_fBorder + titleSizeY +   m_fSpacing + listSizeY    + m_fBorder;

	const float windowOriginX = m_vPosition.x + m_vOffset.x - m_fBorder;
	const float windowOriginY = m_vPosition.y + m_vOffset.y - m_fBorder;

	return fwBox2D(
		windowOriginX + m_fBorder,
		windowOriginY + m_fBorder + titleSizeY + m_fSpacing + m_fScale*DebugTextureViewerWindow_ENTRY_H,
		windowOriginX + m_fBorder + listSizeX - DebugTextureViewerWindow_SCROLLBAR_AREA_W,
		windowOriginY + windowSizeY - m_fBorder
	);
}

fwBox2D CDebugTextureViewerWindow::GetPreviewBounds() const
{
	const float titleSizeY = m_fScale*m_fTitleHeight;

	const float listSizeX = m_fScale*m_fWidth;
	const float listSizeY = m_fScale*(float)(m_numSlots + 1)*DebugTextureViewerWindow_ENTRY_H; // includes category region

//	const float listEntrySizeX = listSizeX - DebugTextureViewerWindow_SCROLLBAR_AREA_W;

	const float previewSizeX = m_fScale*m_fPreviewWidth;

	const float windowSizeX = m_fBorder + listSizeX  + 0*m_fSpacing + previewSizeX + m_fBorder;
	const float windowSizeY = m_fBorder + titleSizeY +   m_fSpacing + listSizeY    + m_fBorder;

	const float windowOriginX = m_vPosition.x + m_vOffset.x - m_fBorder;
	const float windowOriginY = m_vPosition.y + m_vOffset.y - m_fBorder;

	return fwBox2D(
		windowOriginX + windowSizeX - m_fBorder - previewSizeX,
		windowOriginY + windowSizeY - m_fBorder - listSizeY,
		windowOriginX + windowSizeX - m_fBorder,
		windowOriginY + windowSizeY - m_fBorder
	);
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	Draw
// PURPOSE:		draws a window 
//////////////////////////////////////////////////////////////////////////
void CDebugTextureViewerWindow::Draw()
{
	if (!m_bIsVisible) { return; }

	const float opacity = m_bIsActive ? 1.0f : 0.7f;

	// would it make sense to implement something like 'sizers' in wxWidgets or layout managers in java?
	// http://biolpc22.york.ac.uk/wx/docs/html/wx/wx_sizeroverview.html
	// http://neume.sourceforge.net/sizerdemo
	// http://download.oracle.com/javase/tutorial/uiswing/layout/using.html
	// 
	// regions:
	// 
	// +------------------------+
	// |title                   |
	// +-----------+------------+
	// |list       |preview     |
	// |           |            |
	// |           |            |
	// |           |            |
	// +-----------+------------+
	// 
	// list region:
	// 
	// +------------------+
	// |cat0 cat1  cat2   |
	// +----+-----+----+--+
	// |item|item |item|sb|
	// +----+-----+----+  |
	// |item|item |item|  |
	// +----+-----+----+  |
	// |item|item |item|  |
	// +----+-----+----+--+

	/*
	+------------------------------------------------------------------------+
	|                                                                    [ ] |
	+--+------------------------------------------------------------------+--+
	|  |title                                                             |  |
	|  |                                                                  |  |
	|  |                                                                  |  |
	|  +------------------------------------------------------------------+  |
	|  +-------------------------------------------+-+--------------------+  |
	|  |list                                       | |preview             |  |
	|  |                                           | |                    |  |
	|  |                                           | |                    |  |
	|  |                                           | |                    |  |
	|  |                                           | |                    |  |
	|  |                                           | |                    |  |
	|  |                                           | |                    |  |
	|  |                                           | |                    |  |
	|  |                                           | |                    |  |
	|  |                                           | |                    |  |
	|  |                                           | |                    |  |
	|  |                                           | |                    |  |
	|  |                                           | |                    |  |
	|  |                                           | |                    |  |
	|  |                                           | |                    |  |
	+--+-------------------------------------------+-+--------------------+--+
	|                                                                        |
	+------------------------------------------------------------------------+
	*/
	const float titleSizeY = m_fScale*m_fTitleHeight;

	const float listSizeX = m_fScale*m_fWidth;
	const float listSizeY = m_fScale*(float)(m_numSlots + 1)*DebugTextureViewerWindow_ENTRY_H; // includes category region

	const float listEntrySizeX = listSizeX - DebugTextureViewerWindow_SCROLLBAR_AREA_W;

	const float previewSizeX = m_fScale*m_fPreviewWidth;

	const float windowSizeX = m_fBorder + listSizeX  + 0*m_fSpacing + previewSizeX + m_fBorder;
	const float windowSizeY = m_fBorder + titleSizeY +   m_fSpacing + listSizeY    + m_fBorder;

	const float windowOriginX = m_vPosition.x + m_vOffset.x - m_fBorder;
	const float windowOriginY = m_vPosition.y + m_vOffset.y - m_fBorder;

	const fwBox2D windowRegion(
		windowOriginX + 0,
		windowOriginY + 0,
		windowOriginX + windowSizeX,
		windowOriginY + windowSizeY
	);
	const fwBox2D windowBorderTop(
		windowOriginX + 0,
		windowOriginY + 0,
		windowOriginX + windowSizeX,
		windowOriginY + m_fBorder
	);
	const fwBox2D windowBorderBottom(
		windowOriginX + 0,
		windowOriginY + windowSizeY - m_fBorder,
		windowOriginX + windowSizeX,
		windowOriginY + windowSizeY
	);
	const fwBox2D windowBorderLeft(
		windowOriginX + 0,
		windowOriginY + m_fBorder,
		windowOriginX + m_fBorder,
		windowOriginY + windowSizeY - m_fBorder
	);
	const fwBox2D windowBorderRight(
		windowOriginX + windowSizeX - m_fBorder,
		windowOriginY + m_fBorder,
		windowOriginX + windowSizeX,
		windowOriginY + windowSizeY - m_fBorder
	);
	const fwBox2D titleRegion(
		windowOriginX + m_fBorder,
		windowOriginY + m_fBorder,
		windowOriginX + windowSizeX - m_fBorder,
		windowOriginY + m_fBorder + titleSizeY
	);
	const fwBox2D contentRegion( // list + preview
		windowOriginX + m_fBorder,
		windowOriginY + m_fBorder + titleSizeY + m_fSpacing,
		windowOriginX + windowSizeX - m_fBorder,
		windowOriginY + windowSizeY - m_fBorder
	);
	const fwBox2D listRegion = GetListBounds();
//	const fwBox2D listRegion(
//		windowOriginX + m_fBorder,
//		windowOriginY + m_fBorder + titleSizeY + m_fSpacing + m_fScale*DebugTextureViewerWindow_ENTRY_H,
//		windowOriginX + m_fBorder + listSizeX  - DebugTextureViewerWindow_SCROLLBAR_AREA_W,
//		windowOriginY + windowSizeY - m_fBorder
//	);
	const fwBox2D previewRegion = GetPreviewBounds();
//	const fwBox2D previewRegion(
//		windowSizeX - m_fBorder - previewSizeX,
//		windowSizeY - m_fBorder - listSizeY,
//		windowSizeX - m_fBorder,
//		windowSizeY - m_fBorder
//	);
	const fwBox2D titleSpacer(
		windowOriginX + m_fBorder,
		windowOriginY + m_fBorder + titleSizeY,
		windowOriginX + windowSizeX - m_fBorder,
		windowOriginY + m_fBorder + titleSizeY + m_fSpacing
	);
	const fwBox2D listSpacer(
		windowOriginX + m_fBorder + listSizeX,
		windowOriginY + m_fBorder + titleSizeY +   m_fSpacing,
		windowOriginX + m_fBorder + listSizeX  + 0*m_fSpacing,
		windowOriginY + windowSizeY - m_fBorder
	);
	const fwBox2D listCategoryRegion(
		windowOriginX + m_fBorder,
		windowOriginY + m_fBorder + titleSizeY + m_fSpacing,
		windowOriginX + m_fBorder + listSizeX,
		windowOriginY + m_fBorder + titleSizeY + m_fSpacing + m_fScale*DebugTextureViewerWindow_ENTRY_H
	);
	const Vector2 listEntryOffset(
		listCategoryRegion.x0,
		listCategoryRegion.y1
	);
	const fwBox2D listScrollRegion(
		windowOriginX + m_fBorder + listSizeX - DebugTextureViewerWindow_SCROLLBAR_AREA_W,
		windowOriginY + m_fBorder + titleSizeY + m_fSpacing + m_fScale*DebugTextureViewerWindow_ENTRY_H,
		windowOriginX + m_fBorder + listSizeX,
		windowOriginY + windowSizeY - m_fBorder
	);

	const Color32 borderColour = GetColour(DebugTextureViewerWindow_COLOUR_BORDER, opacity);
	const Color32 spacerColour = GetColour(DebugTextureViewerWindow_COLOUR_SPACER, opacity);

	DebugTextureViewerWindow_DrawRect(windowBorderTop   , borderColour);
	DebugTextureViewerWindow_DrawRect(windowBorderBottom, borderColour);
	DebugTextureViewerWindow_DrawRect(windowBorderLeft  , borderColour);
	DebugTextureViewerWindow_DrawRect(windowBorderRight , borderColour);
	DebugTextureViewerWindow_DrawRect(titleSpacer       , spacerColour);
	DebugTextureViewerWindow_DrawRect(listSpacer        , spacerColour);
	DebugTextureViewerWindow_DrawRect(listCategoryRegion, GetColour(DebugTextureViewerWindow_COLOUR_LIST_CATEGORY_BG , opacity));
	DebugTextureViewerWindow_DrawRect(listScrollRegion  , GetColour(DebugTextureViewerWindow_COLOUR_LIST_SCROLLBAR_BG, opacity));
	DebugTextureViewerWindow_DrawRect(listRegion        , GetColour(DebugTextureViewerWindow_COLOUR_LIST_ENTRY_BG    , opacity));
	DebugTextureViewerWindow_DrawRect(titleRegion       , GetColour(DebugTextureViewerWindow_COLOUR_TITLE_BG         , opacity));

	const float titleBoxInset  = 1.0f;
	const float titleBoxSize   = m_fScale*DebugTextureViewerWindow_TITLE_CLOSEBOX_SIZE;
	const float titleBoxOffset = (titleRegion.y1 - titleRegion.y0 - titleBoxSize)/2.0f;

	float   titleTextPos__x = titleRegion.x0 + m_fScale*m_fTitleOffsetX;
	float   titleBoxPos__x = titleRegion.x1;
	fwBox2D titleCloseBoxRegion;
	fwBox2D titleBackBoxRegion;

	if (m_clickBackCB) // draw title back box
	{
		const float backBoxOpacity = opacity*((m_clickBackEnabledCB && (*m_clickBackEnabledCB)() == false) ? 0.35f : 1.0f);

		titleBackBoxRegion = fwBox2D(
			titleRegion.x0 + titleBoxOffset,
			titleRegion.y0 + titleBoxOffset,
			titleRegion.x0 + titleBoxOffset + titleBoxSize,
			titleRegion.y0 + titleBoxOffset + titleBoxSize
		);
		const fwBox2D titleBackBoxBG(
			titleBackBoxRegion.x0 + titleBoxInset,
			titleBackBoxRegion.y0 + titleBoxInset,
			titleBackBoxRegion.x1 - titleBoxInset,
			titleBackBoxRegion.y1 - titleBoxInset
		);
		DebugTextureViewerWindow_DrawRect(titleBackBoxBG, GetColour(DebugTextureViewerWindow_COLOUR_TITLE_CLOSEBOX_BG, backBoxOpacity));

		const Color32 titleBackBoxColourFG = GetColour(DebugTextureViewerWindow_COLOUR_TITLE_CLOSEBOX_FG, backBoxOpacity);

		if (titleBoxInset != 0.0f)
		{
			const fwBox2D titleBackBoxFG_Y0(
				titleBackBoxRegion.x0,
				titleBackBoxRegion.y0,
				titleBackBoxRegion.x1,
				titleBackBoxRegion.y0 + titleBoxInset
			);
			const fwBox2D titleBackBoxFG_Y1(
				titleBackBoxRegion.x0,
				titleBackBoxRegion.y1 - titleBoxInset,
				titleBackBoxRegion.x1,
				titleBackBoxRegion.y1
			);
			const fwBox2D titleBackBoxFG_X0(
				titleBackBoxRegion.x0,
				titleBackBoxRegion.y0 + titleBoxInset,
				titleBackBoxRegion.x0 + titleBoxInset,
				titleBackBoxRegion.y1 - titleBoxInset
			);
			const fwBox2D titleBackBoxFG_X1(
				titleBackBoxRegion.x1 - titleBoxInset,
				titleBackBoxRegion.y0 + titleBoxInset,
				titleBackBoxRegion.x1,
				titleBackBoxRegion.y1 - titleBoxInset
			);
			DebugTextureViewerWindow_DrawRect(titleBackBoxFG_Y0, titleBackBoxColourFG);
			DebugTextureViewerWindow_DrawRect(titleBackBoxFG_Y1, titleBackBoxColourFG);
			DebugTextureViewerWindow_DrawRect(titleBackBoxFG_X0, titleBackBoxColourFG);
			DebugTextureViewerWindow_DrawRect(titleBackBoxFG_X1, titleBackBoxColourFG);
		}

		if (1)
		{
			DebugTextureViewerWindow_DrawShapeB(titleBackBoxBG, titleBackBoxColourFG);
		}

		titleTextPos__x = titleBackBoxRegion.x1 + 2.0f;
	}

	// draw title text
	{
		const float fTitleX = titleTextPos__x;
		const float fTitleY = titleRegion.y0 + m_fScale*DebugTextureViewerWindow_TEXTOFFSET_Y;

		DebugTextureViewerWindow_DrawText(fTitleX + 1.0f, fTitleY + 1.0f, m_fScale, GetColour(DebugTextureViewerWindow_COLOUR_TITLE_SHADOW, opacity), GetTitle());
		DebugTextureViewerWindow_DrawText(fTitleX       , fTitleY       , m_fScale, GetColour(DebugTextureViewerWindow_COLOUR_TITLE_TEXT  , opacity), GetTitle());
	}

	// draw title close box
	{
		titleCloseBoxRegion = fwBox2D(
			titleBoxPos__x - titleBoxOffset - titleBoxSize,
			titleRegion.y0 + titleBoxOffset,
			titleBoxPos__x - titleBoxOffset,
			titleRegion.y0 + titleBoxOffset + titleBoxSize
		);
		const fwBox2D titleCloseBoxBG(
			titleCloseBoxRegion.x0 + titleBoxInset,
			titleCloseBoxRegion.y0 + titleBoxInset,
			titleCloseBoxRegion.x1 - titleBoxInset,
			titleCloseBoxRegion.y1 - titleBoxInset
		);
		DebugTextureViewerWindow_DrawRect(titleCloseBoxBG, GetColour(DebugTextureViewerWindow_COLOUR_TITLE_CLOSEBOX_BG, opacity));

		const Color32 titleCloseBoxColourFG = GetColour(DebugTextureViewerWindow_COLOUR_TITLE_CLOSEBOX_FG, opacity);

		if (titleBoxInset != 0.0f)
		{
			const fwBox2D titleCloseBoxFG_Y0(
				titleCloseBoxRegion.x0,
				titleCloseBoxRegion.y0,
				titleCloseBoxRegion.x1,
				titleCloseBoxRegion.y0 + titleBoxInset
			);
			const fwBox2D titleCloseBoxFG_Y1(
				titleCloseBoxRegion.x0,
				titleCloseBoxRegion.y1 - titleBoxInset,
				titleCloseBoxRegion.x1,
				titleCloseBoxRegion.y1
			);
			const fwBox2D titleCloseBoxFG_X0(
				titleCloseBoxRegion.x0,
				titleCloseBoxRegion.y0 + titleBoxInset,
				titleCloseBoxRegion.x0 + titleBoxInset,
				titleCloseBoxRegion.y1 - titleBoxInset
			);
			const fwBox2D titleCloseBoxFG_X1(
				titleCloseBoxRegion.x1 - titleBoxInset,
				titleCloseBoxRegion.y0 + titleBoxInset,
				titleCloseBoxRegion.x1,
				titleCloseBoxRegion.y1 - titleBoxInset
			);
			DebugTextureViewerWindow_DrawRect(titleCloseBoxFG_Y0, titleCloseBoxColourFG);
			DebugTextureViewerWindow_DrawRect(titleCloseBoxFG_Y1, titleCloseBoxColourFG);
			DebugTextureViewerWindow_DrawRect(titleCloseBoxFG_X0, titleCloseBoxColourFG);
			DebugTextureViewerWindow_DrawRect(titleCloseBoxFG_X1, titleCloseBoxColourFG);
		}

		if (1)
		{
			DebugTextureViewerWindow_DrawShapeX(titleCloseBoxBG, titleCloseBoxColourFG);
		}

		titleBoxPos__x = titleCloseBoxRegion.x0;
	}

	/*if (m_clickBackCB) // draw title back box
	{
		titleBackBoxRegion = fwBox2D(
			titleBoxPos__x - titleBoxOffset - titleBoxSize,
			titleRegion.y0 + titleBoxOffset,
			titleBoxPos__x - titleBoxOffset,
			titleRegion.y0 + titleBoxOffset + titleBoxSize
		);
		const fwBox2D titleBackBoxBG(
			titleBackBoxRegion.x0 + titleBoxInset,
			titleBackBoxRegion.y0 + titleBoxInset,
			titleBackBoxRegion.x1 - titleBoxInset,
			titleBackBoxRegion.y1 - titleBoxInset
		);
		DebugTextureViewerWindow_DrawRect(titleBackBoxBG, GetColour(DebugTextureViewerWindow_COLOUR_TITLE_CLOSEBOX_BG, opacity));

		const Color32 titleBackBoxColourFG = GetColour(DebugTextureViewerWindow_COLOUR_TITLE_CLOSEBOX_FG, opacity);

		if (titleBoxInset != 0.0f)
		{
			const fwBox2D titleBackBoxFG_Y0(
				titleBackBoxRegion.x0,
				titleBackBoxRegion.y0,
				titleBackBoxRegion.x1,
				titleBackBoxRegion.y0 + titleBoxInset
			);
			const fwBox2D titleBackBoxFG_Y1(
				titleBackBoxRegion.x0,
				titleBackBoxRegion.y1 - titleBoxInset,
				titleBackBoxRegion.x1,
				titleBackBoxRegion.y1
			);
			const fwBox2D titleBackBoxFG_X0(
				titleBackBoxRegion.x0,
				titleBackBoxRegion.y0 + titleBoxInset,
				titleBackBoxRegion.x0 + titleBoxInset,
				titleBackBoxRegion.y1 - titleBoxInset
			);
			const fwBox2D titleBackBoxFG_X1(
				titleBackBoxRegion.x1 - titleBoxInset,
				titleBackBoxRegion.y0 + titleBoxInset,
				titleBackBoxRegion.x1,
				titleBackBoxRegion.y1 - titleBoxInset
			);
			DebugTextureViewerWindow_DrawRect(titleBackBoxFG_Y0, titleBackBoxColourFG);
			DebugTextureViewerWindow_DrawRect(titleBackBoxFG_Y1, titleBackBoxColourFG);
			DebugTextureViewerWindow_DrawRect(titleBackBoxFG_X0, titleBackBoxColourFG);
			DebugTextureViewerWindow_DrawRect(titleBackBoxFG_X1, titleBackBoxColourFG);
		}

		if (1)
		{
			DebugTextureViewerWindow_DrawShapeB(titleBackBoxBG, titleBackBoxColourFG);
		}
	}*/

	(void)windowRegion;
	(void)contentRegion;
	(void)previewRegion;

	if (m_bIsActive) // draw alternate-colored bars
	{
		for (int i = Max<int>(0, m_slotZeroEntryIndex) & 1; i < m_numSlots; i += 2)
		{
			const fwBox2D r(
				listEntryOffset.x,
				listEntryOffset.y + m_fScale*(float)(i + 0)*DebugTextureViewerWindow_ENTRY_H,
				listEntryOffset.x + listEntrySizeX,
				listEntryOffset.y + m_fScale*(float)(i + 1)*DebugTextureViewerWindow_ENTRY_H
			);
			DebugTextureViewerWindow_DrawRect(r, Color32(0,0,0,16));
		}
	}

	if (0) // debuggery
	{
		const float fTitleX = listCategoryRegion.x0;
		const float fTitleY = listCategoryRegion.y0 + m_fScale*DebugTextureViewerWindow_TEXTOFFSET_Y;

		char temp[256] = "";

		sprintf(temp, "m_numSlots           = %d", m_numSlots                          ); DebugTextureViewerWindow_DrawText(fTitleX - 256, fTitleY + 12*0, 1.0f, CRGBA_Black(), temp);
		sprintf(temp, "m_selectorEnabled    = %s", m_selectorEnabled ? "TRUE" : "FALSE"); DebugTextureViewerWindow_DrawText(fTitleX - 256, fTitleY + 12*1, 1.0f, CRGBA_Black(), temp);
		sprintf(temp, "m_selectorIndex      = %d", m_selectorIndex                     ); DebugTextureViewerWindow_DrawText(fTitleX - 256, fTitleY + 12*2, 1.0f, CRGBA_Black(), temp);
		sprintf(temp, "m_indexInList        = %d", m_indexInList                       ); DebugTextureViewerWindow_DrawText(fTitleX - 256, fTitleY + 12*3, 1.0f, CRGBA_Black(), temp);
		sprintf(temp, "m_slotZeroEntryIndex = %d", m_slotZeroEntryIndex                ); DebugTextureViewerWindow_DrawText(fTitleX - 256, fTitleY + 12*4, 1.0f, CRGBA_Black(), temp);
		sprintf(temp, "m_numEntriesInList   = %d", m_numEntriesInList                  ); DebugTextureViewerWindow_DrawText(fTitleX - 256, fTitleY + 12*5, 1.0f, CRGBA_Black(), temp);
	}

	for (int colIndex = 0; colIndex < m_columns.size(); colIndex++) // draw column titles (categories)
	{
		const CDebugTextureViewerWindowColumn& column = m_columns[colIndex];
		const float fTitleX = listCategoryRegion.x0 + m_fScale*column.GetOffsetX();
		const float fTitleY = listCategoryRegion.y0 + m_fScale*DebugTextureViewerWindow_TEXTOFFSET_Y;

		Color32 colour(0);

		if (m_getEntryColourCB && m_getEntryColourCB(&colour, INDEX_NONE, colIndex))
		{
			colour.SetAlpha((int)Clamp<float>(colour.GetAlphaf()*opacity*255.0f + 0.5f, 0.0f, 255.0f));
		}
		else
		{
			colour = GetColour(DebugTextureViewerWindow_COLOUR_LIST_CATEGORY_TEXT, opacity);
		}

		DebugTextureViewerWindow_DrawText(fTitleX, fTitleY, m_fScale, colour, column.GetName());
	}

	if (m_numEntriesInList > 0)
	{
		if (m_selectorIndex >= 0 && m_selectorEnabled) // draw selector
		{
			const fwBox2D r(
				listEntryOffset.x,
				listEntryOffset.y + m_fScale*(float)(m_selectorIndex + 0)*DebugTextureViewerWindow_ENTRY_H,
				listEntryOffset.x + listEntrySizeX,
				listEntryOffset.y + m_fScale*(float)(m_selectorIndex + 1)*DebugTextureViewerWindow_ENTRY_H
			);
			DebugTextureViewerWindow_DrawRect(r, GetColour(DebugTextureViewerWindow_COLOUR_LIST_SELECTOR, opacity*opacity));
		}

		if (m_numEntriesInList <= m_numSlots) // draw scroll bar
		{
			const fwBox2D r(
				listScrollRegion.x0 + DebugTextureViewerWindow_SCROLLBAR_OFFSET_X,
				listScrollRegion.y0,
				listScrollRegion.x1 - DebugTextureViewerWindow_SCROLLBAR_OFFSET_X,
				listScrollRegion.y1
			);
			DebugTextureViewerWindow_DrawRect(r, GetColour(DebugTextureViewerWindow_COLOUR_LIST_SCROLLBAR_FG, opacity));
		}
		else
		{
			float fScrollBarHeight = (listScrollRegion.y1 - listScrollRegion.y0)*(float)m_numSlots          /(float)m_numEntriesInList;
			float fScrollBarY      = (listScrollRegion.y1 - listScrollRegion.y0)*(float)m_slotZeroEntryIndex/(float)m_numEntriesInList;

			if (fScrollBarHeight < 3.0f) // adjust scroll bar so that it is never < 3 pixels
			{
				const float fScrollBarCentre = fScrollBarY + fScrollBarHeight*0.5f;

				fScrollBarHeight = 3.0f;
				fScrollBarY      = Clamp<float>(fScrollBarCentre - fScrollBarHeight*0.5f, 0.0f, listScrollRegion.y1 - fScrollBarHeight);
			}

			const fwBox2D r(
				listScrollRegion.x0 + DebugTextureViewerWindow_SCROLLBAR_OFFSET_X,
				listScrollRegion.y0 + fScrollBarY,
				listScrollRegion.x1 - DebugTextureViewerWindow_SCROLLBAR_OFFSET_X,
				listScrollRegion.y0 + fScrollBarY + fScrollBarHeight
			);
			DebugTextureViewerWindow_DrawRect(r, GetColour(DebugTextureViewerWindow_COLOUR_LIST_SCROLLBAR_FG, opacity));
		}

		for (int i = 0; i < m_numSlots; i++) // draw entries
		{
			const int rowIndex = m_slotZeroEntryIndex + i;

			if (rowIndex >= 0 && rowIndex < m_numEntriesInList)
			{
				for (int colIndex = 0; colIndex < m_columns.size(); colIndex++)
				{
					const CDebugTextureViewerWindowColumn& column = m_columns[colIndex];
					char achTmp[DEBUGWINDOWCOLUMN_ENTRY_MAX_CHARS];
					m_getEntryTextCB(achTmp, rowIndex, colIndex);

					const float fTextX = listEntryOffset.x + m_fScale*column.GetOffsetX();
					const float fTextY = listEntryOffset.y + m_fScale*(float)i*DebugTextureViewerWindow_ENTRY_H + m_fScale*DebugTextureViewerWindow_TEXTOFFSET_Y;

					Color32 colour(0);

					if (m_getEntryColourCB && m_getEntryColourCB(&colour, rowIndex, colIndex))
					{
						colour.SetAlpha((int)Clamp<float>(colour.GetAlphaf()*opacity*255.0f + 0.5f, 0.0f, 255.0f));
					}
					else
					{
						colour = GetColour(DebugTextureViewerWindow_COLOUR_LIST_ENTRY_TEXT, opacity);
					}

					DebugTextureViewerWindow_DrawText(fTextX, fTextY, m_fScale, colour, achTmp);
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	Update
// PURPOSE:		updates click-and-drag behaviour etc
//////////////////////////////////////////////////////////////////////////
void CDebugTextureViewerWindow::Update()
{
	if (!m_bIsVisible) { return; }

	m_numEntriesInList = m_getNumEntriesCB();

	const float titleSizeY = m_fScale*m_fTitleHeight;

	const float listSizeX = m_fScale*m_fWidth;
	const float listSizeY = m_fScale*(float)(m_numSlots + 1)*DebugTextureViewerWindow_ENTRY_H; // includes category region

	const float listEntrySizeX = listSizeX - DebugTextureViewerWindow_SCROLLBAR_AREA_W;

	const float previewSizeX = m_fScale*m_fPreviewWidth;

	const float windowSizeX = m_fBorder + listSizeX  + 0*m_fSpacing + previewSizeX + m_fBorder;

	const float windowOriginX = m_vPosition.x + m_vOffset.x - m_fBorder;
	const float windowOriginY = m_vPosition.y + m_vOffset.y - m_fBorder;

	const fwBox2D titleRegion(
		windowOriginX + m_fBorder,
		windowOriginY + m_fBorder,
		windowOriginX + windowSizeX - m_fBorder,
		windowOriginY + m_fBorder + titleSizeY
	);

	const float titleBoxSize   = m_fScale*DebugTextureViewerWindow_TITLE_CLOSEBOX_SIZE;
	const float titleBoxOffset = (titleRegion.y1 - titleRegion.y0 - titleBoxSize)/2.0f;

	const fwBox2D titleBackBoxRegion(
		titleRegion.x0 + titleBoxOffset,
		titleRegion.y0 + titleBoxOffset,
		titleRegion.x0 + titleBoxOffset + titleBoxSize,
		titleRegion.y0 + titleBoxOffset + titleBoxSize
	);

	float titleBoxPos__x = titleRegion.x1;
	const fwBox2D titleCloseBoxRegion(
		titleBoxPos__x - titleBoxOffset - titleBoxSize,
		titleRegion.y0 + titleBoxOffset,
		titleBoxPos__x - titleBoxOffset,
		titleRegion.y0 + titleBoxOffset + titleBoxSize
	);
	titleBoxPos__x = titleCloseBoxRegion.x0;
	// .. more boxes

	//const fwBox2D titleBackBoxRegion(
	//	titleBoxPos__x - titleBoxOffset - titleBoxSize,
	//	titleRegion.y0 + titleBoxOffset,
	//	titleBoxPos__x - titleBoxOffset,
	//	titleRegion.y0 + titleBoxOffset + titleBoxSize
	//);

	const fwBox2D listCategoryRegion(
		windowOriginX + m_fBorder,
		windowOriginY + m_fBorder + titleSizeY + m_fSpacing,
		windowOriginX + m_fBorder + listSizeX,
		windowOriginY + m_fBorder + titleSizeY + m_fSpacing + m_fScale*DebugTextureViewerWindow_ENTRY_H
	);

	(void)listSizeY;
	(void)listEntrySizeX;

	int padScroll = 0;

	// joypad
	{
		CPad* pad = CControlMgr::IsDebugPadOn() ? &CControlMgr::GetDebugPad() : CControlMgr::GetPlayerPad();

		if (pad) // L2 enables autoscroll, R2 adjusts scroll speed
		{
			const int  padScrollAmount = pad->GetRightShoulder2() ? Max<int>(1, (int)((float)m_numSlots*(float)pad->GetRightShoulder2()/128.0f)) : 1;
			const bool padScrollRepeat = pad->GetLeftShoulder2() != 0;

			if (pad->DPadDownJustDown() || (padScrollRepeat && pad->GetDPadDown())) { padScroll -= padScrollAmount; }
			if (pad->DPadUpJustDown  () || (padScrollRepeat && pad->GetDPadUp  ())) { padScroll += padScrollAmount; }
		}
	}

	const Vector2 vMousePos = Vector2((float)ioMouse::GetX(), (float)ioMouse::GetY());
	const int mouseWheel = ioMouse::GetDZ() + padScroll;
	const bool bMouseDownLeft    = (ioMouse::GetButtons()        & ioMouse::MOUSE_LEFT) != 0;
	const bool bMousePressedLeft = (ioMouse::GetPressedButtons() & ioMouse::MOUSE_LEFT) != 0;

	if (m_bIsMoving)
	{
		SetPos(vMousePos - m_vClickDragDelta); // already dragging window so update position and state

		if (!bMouseDownLeft)
		{
			m_bIsMoving = false;
		}
	}
	else
	{
		SetPos(m_vPosition); // required in case m_fScale has changed .. we could test for this condition i suppose

		if (bMousePressedLeft && titleRegion.IsInside(vMousePos)) // check for player clicking on title bar to draw window
		{
			if (titleCloseBoxRegion.IsInside(vMousePos))
			{
				CDebugTextureViewerWindowMgr::SetLeftMouseButtonHandled(true);
				Release();
				return;
			}
			else if (titleBackBoxRegion.IsInside(vMousePos))
			{
				if (m_clickBackCB && !(m_clickBackEnabledCB && (*m_clickBackEnabledCB)() == false))
				{
					(*m_clickBackCB)();
				}

				return;
			}
			else
			{
				m_bIsMoving = true;
				m_vClickDragDelta = vMousePos - (m_vPosition + m_vOffset);
			}
		}
	}

	// ===================================================================================================

	if (bMouseDownLeft) // check for activation (which window receives keyboards and mousewheel events?)
	{
		if (GetWindowBounds().IsInside(vMousePos))
		{
			m_bIsActive = true;
		}
	}

	if (m_bIsActive && !m_bIsMoving && bMousePressedLeft && m_clickCategoryCB)
	{
		for (int colIndex = 0; colIndex < m_columns.size(); colIndex++)
		{
			const float x0 =                                     (listCategoryRegion.x0 + m_fScale*m_columns[colIndex + 0].GetOffsetX());
			const float x1 = (colIndex + 1) < m_columns.size() ? (listCategoryRegion.x0 + m_fScale*m_columns[colIndex + 1].GetOffsetX()) : listCategoryRegion.x1;

			const fwBox2D listCategoryBounds(
				x0,
				listCategoryRegion.y0,
				x1,
				listCategoryRegion.y1
			);

			if (listCategoryBounds.IsInside(vMousePos))
			{
				m_clickCategoryCB(INDEX_NONE, colIndex);
			}
		}
	}

	if (m_numEntriesInList > 0) // if window is active, check for input events such as mouse wheel etc
	{
		if (m_bIsActive && !m_bIsMoving)
		{
			int newIndexInList = m_indexInList;

			if (mouseWheel != 0) // mouse wheel movement
			{
				newIndexInList = m_indexInList - mouseWheel;
				m_selectorEnabled = true;
			}
			else if (bMouseDownLeft && GetListBounds().IsInside(vMousePos)) // mouse clicks
			{
				const int possibleEntry = (int)((vMousePos.y - GetListBounds().y0)/(m_fScale*DebugTextureViewerWindow_ENTRY_H));

				if ((m_slotZeroEntryIndex + possibleEntry) < m_numEntriesInList)
				{
					newIndexInList = m_slotZeroEntryIndex + possibleEntry;
				}

				m_selectorEnabled = true;
			}
			else // page up/down etc, keyboard input
			{
				newIndexInList = m_indexInList;

				if      (CControlMgr::GetKeyboard().GetKeyJustDown(KEY_SUBTRACT, KEYBOARD_MODE_DEBUG)) { m_selectorEnabled = true; newIndexInList -= m_numSlots; }
				else if (CControlMgr::GetKeyboard().GetKeyJustDown(KEY_ADD     , KEYBOARD_MODE_DEBUG)) { m_selectorEnabled = true; newIndexInList += m_numSlots; }
				else if (CControlMgr::GetKeyboard().GetKeyJustDown(KEY_UP      , KEYBOARD_MODE_DEBUG))
				{
					m_selectorEnabled = true;
					newIndexInList--;

					if (m_getRowActiveCB)
					{
						while (newIndexInList > 0 && !m_getRowActiveCB(newIndexInList))
						{
							newIndexInList--;
						}
					}
				}
				else if (CControlMgr::GetKeyboard().GetKeyJustDown(KEY_DOWN, KEYBOARD_MODE_DEBUG))
				{
					m_selectorEnabled = true;
					newIndexInList++;

					if (m_getRowActiveCB)
					{
						while (newIndexInList < m_numEntriesInList && !m_getRowActiveCB(newIndexInList))
						{
							newIndexInList++;
						}
					}
				}
			}

			m_indexInList = newIndexInList;
		}

		m_indexInList        = Clamp<int>(m_indexInList       , 0,             m_numEntriesInList - 1          );
		m_slotZeroEntryIndex = Clamp<int>(m_slotZeroEntryIndex, 0, Max<int>(0, m_numEntriesInList - m_numSlots));

		// adjust viewport
		{
			const int minSlotEntryIndex = m_slotZeroEntryIndex;
			const int maxSlotEntryIndex = m_slotZeroEntryIndex + m_numSlots - 1;

			if (m_indexInList < minSlotEntryIndex) // scroll up
			{
				m_slotZeroEntryIndex = m_indexInList;
			}
			else if (m_indexInList > maxSlotEntryIndex) // scroll down
			{
				m_slotZeroEntryIndex += (m_indexInList - maxSlotEntryIndex);
			}
		}

		// adjust selector
		{
			m_selectorIndex = Clamp<int>(m_indexInList - m_slotZeroEntryIndex, 0, Min<int>(m_numSlots, m_numEntriesInList) - 1);
		}
	}
	else // no entries
	{
		m_selectorIndex      = 0;
		m_indexInList        = -1;
		m_slotZeroEntryIndex = -1;
	}
}

float CDebugTextureViewerWindow::GetDefaultListEntryHeight()
{
	return DebugTextureViewerWindow_ENTRY_H;
}

#endif // __BANK
