/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    UiGadgetInspector.h
// PURPOSE : a window to display name/value pairs
// CREATED : 8/06/11
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef _DEBUG_UIGADGET_UIGADGETINSPECTOR_H_
#define _DEBUG_UIGADGET_UIGADGETINSPECTOR_H_

#if __BANK

#include "debug/UiGadget/UiGadgetBase.h"
#include "debug/UiGadget/UiGadgetBox.h"
#include "debug/UiGadget/UiGadgetText.h"

// Forward declarations
class CUiColorScheme;

class CUiGadgetInspector : public CUiGadgetBase
{
public:
	typedef void (*SetEntryCB)(CUiGadgetText* pResult, u32 row, u32 col);

	CUiGadgetInspector(float fX, float fY, float fWidth, u32 numRows, u32 numColumns, const float* columnOffsets, const CUiColorScheme& colorScheme);
	virtual ~CUiGadgetInspector();
	virtual void Draw() { }
	virtual void Update();
	virtual fwBox2D GetBounds() const;
	u32 GetNumEntries() const { return m_numEntries; }
	void SetNumEntries(u32 numEntries) { m_numEntries = numEntries; }
	inline void SetUpdateCellCB(SetEntryCB cb) { m_setEntryCB = cb; }

private:
	atArray<CUiGadgetBox> m_aBgBoxes;
	atArray<CUiGadgetText> m_aTextFields;
	float m_fWidth;
	u32 m_numRows;
	u32 m_numColumns;
	u32 m_numEntries;

	SetEntryCB m_setEntryCB;
};

#endif	//__BANK

#endif	//_DEBUG_UIGADGET_UIGADGETINSPECTOR_H_

