/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    UiGadgetInspector.cpp
// PURPOSE : a window to display name/value pairs
// CREATED : 8/06/11
//
/////////////////////////////////////////////////////////////////////////////////

#include "debug/UiGadget/UiGadgetInspector.h"

#include "debug/UiGadget/UiColorScheme.h"

#if __BANK

#define UIGADGETINSPECTOR_ENTRY_H			(12.0f)
#define UIGADGETINSPECTOR_TEXT_OFFSET_X		(2.0f)
#define UIGADGETINSPECTOR_TEXT_OFFSET_Y		(3.0f)


//////////////////////////////////////////////////////////////////////////
// FUNCTION:	CUiGadgetInspector
// PURPOSE:		ctor
//////////////////////////////////////////////////////////////////////////
CUiGadgetInspector::CUiGadgetInspector(float fX, float fY, float fWidth, u32 numRows, u32 numColumns, const float* columnOffsets, const CUiColorScheme& colorScheme)
:	m_fWidth(fWidth), m_numRows(numRows), m_numColumns(numColumns), m_setEntryCB(NULL)
{
	SetPos(Vector2(fX, fY));
	Color32 bg1 = colorScheme.GetColor(CUiColorScheme::COLOR_LIST_BG1);
	Color32 bg2 = colorScheme.GetColor(CUiColorScheme::COLOR_LIST_BG2);

	// create text fields
	for (u32 i=0; i<numRows*numColumns; i++)
	{
		u32 col = i%numColumns;
		u32 row = i/numColumns;
		float fTextX = UIGADGETINSPECTOR_TEXT_OFFSET_X + fX + columnOffsets[col];
		float fTextY = UIGADGETINSPECTOR_TEXT_OFFSET_Y + fY + row*UIGADGETINSPECTOR_ENTRY_H;
		m_aTextFields.PushAndGrow(CUiGadgetText(fTextX, fTextY, colorScheme.GetColor(CUiColorScheme::COLOR_LIST_ENTRY_TEXT), ""));
	}
	for (u32 i=0; i<m_aTextFields.size(); i++)
	{
		m_aTextFields[i].AttachToParent(this);
	}


	// create background boxes
	for (u32 i=0; i<numRows; i++)
	{
		m_aBgBoxes.PushAndGrow(CUiGadgetBox(fX, fY+i*UIGADGETINSPECTOR_ENTRY_H, fWidth, UIGADGETINSPECTOR_ENTRY_H, ((i&1) ? bg1 : bg2)));
	}
	for (u32 i=0; i<m_aBgBoxes.size(); i++)
	{
		m_aBgBoxes[i].AttachToParent(this);
	}

	SetHasFocus(false);
	SetNumEntries(0);
}


//////////////////////////////////////////////////////////////////////////
// FUNCTION:	~CUiGadgetInspector
// PURPOSE:		dtor (TODO - how should gadget trees be shut down and destroyed?)
//////////////////////////////////////////////////////////////////////////
CUiGadgetInspector::~CUiGadgetInspector()
{
	DetachAllChildren();
}


//////////////////////////////////////////////////////////////////////////
// FUNCTION:	GetBounds
// PURPOSE:
//////////////////////////////////////////////////////////////////////////
fwBox2D CUiGadgetInspector::GetBounds() const
{
	Vector2 vPos = GetPos();
	return fwBox2D(vPos.x, vPos.y, vPos.x+m_fWidth, vPos.y+m_numRows*UIGADGETINSPECTOR_ENTRY_H);
}


//////////////////////////////////////////////////////////////////////////
// FUNCTION:	Update
// PURPOSE:		updates contents of inspector
//////////////////////////////////////////////////////////////////////////
void CUiGadgetInspector::Update()
{
	// update value of cells
	if (m_setEntryCB!=NULL)
	{
		for (u32 row=0; row<m_numRows; row++)
		{
			for (u32 col=0; col<m_numColumns; col++)
			{
				if (row<m_numEntries)
				{
					m_setEntryCB(&m_aTextFields[col+row*m_numColumns], row, col);
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

