/////////////////////////////////////////////////////////////////////////////////
//
// FILE	   : UiGadgetFileList.h
// PURPOSE : Gadget used for listing details about files
//
// AUTHOR  : james.strain
//
// Copyright (C) 1999-2013 Rockstar Games.  All Rights Reserved.
//
/////////////////////////////////////////////////////////////////////////////////
#ifndef UI_GADGET_FILE_LIST_H_
#define UI_GADGET_FILE_LIST_H_

#if __BANK

// game
#include "debug/UiGadget/UiGadgetBase.h"
#include "debug/UiGadget/UiGadgetList.h"

class CFileViewFilter;

class CUiGadgetFileList : public CUiGadgetBase
{
public:
	CUiGadgetFileList( float const fX, float const fY, CUiColorScheme const& colourScheme );
	virtual ~CUiGadgetFileList();

	void resetSelection();
	inline void SetFileView( CFileViewFilter * view ) { m_fileView = view; refreshList(); }
	
	virtual void Update();
	virtual void Draw() {}

	virtual fwBox2D GetBounds() const;

	int GetSelectedIndex() const { return m_selectedIndex; }
	inline float GetWidth() const { return m_fileList.GetWidth(); }

private: // declarations and variables
	CUiGadgetList				m_fileList;
	CFileViewFilter * 			m_fileView;
	int							m_selectedIndex;

private: // methods
	static void cellUpdate( CUiGadgetText* pResult, u32 row, u32 col, void* extraCallbackData );

	void refreshList();
};

#endif //__BANK

#endif //UI_GADGET_FILE_LIST_H_
