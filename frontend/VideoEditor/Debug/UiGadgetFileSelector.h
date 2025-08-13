/////////////////////////////////////////////////////////////////////////////////
//
// FILE	   : UiGadgetFileSelector.h
// PURPOSE : A compound gadget used for selecting files
//
// AUTHOR  : james.strain
//
// Copyright (C) 1999-2013 Rockstar Games.  All Rights Reserved.
//
/////////////////////////////////////////////////////////////////////////////////
#ifndef UI_GADGET_FILE_SELECTOR_H_
#define UI_GADGET_FILE_SELECTOR_H_

#if __BANK

// framework
#include "streaming/streamingmodule.h"

// game
#include "debug/UiGadget/UiGadgetBase.h"
#include "debug/UiGadget/UiGadgetButton.h"
#include "debug/UiGadget/UiGadgetImage.h"
#include "UiGadgetFileList.h"

class CThumbnailFileView;
class CImposedImageHandler;

class CUiGadgetFileSelector : public CUiGadgetBase
{
public:
	CUiGadgetFileSelector( float const fX, float const fY, CImposedImageHandler& imageHandler, CUiColorScheme const& colourScheme, bool allowCancel = false );
	virtual ~CUiGadgetFileSelector();

	void resetSelection();
	inline void SetFileView( CThumbnailFileView * view ) { resetSelection(); m_fileView = view; m_fileList.SetFileView( (CFileViewFilter*)view ); }
	inline void SetClipSelectedCallback( datCallback const& callback ) { m_confirmButton.SetReleasedCallback( callback ); }
	inline void SetCancelCallback( datCallback const& callback ) { m_cancelButton.SetReleasedCallback( callback ); }

	virtual void Update();
	virtual void Draw() { }

	virtual fwBox2D GetBounds() const;

	int GetSelectedIndex() const { return m_fileList.GetSelectedIndex(); }

private: // declarations and variables
	static strStreamingObjectName const sc_previewTextureName;

	CThumbnailFileView *		m_fileView;
	CImposedImageHandler&		m_imageHandler;
	CUiGadgetFileList			m_fileList;
	CUiGadgetImage				m_previewPane;
	CUiGadgetButton				m_confirmButton;
	CUiGadgetButton				m_cancelButton;
	int							m_oldFileIndex;

private: // methods
	void initializePreviewPane();
	void cleanupPreviewPane();
};

#endif //__BANK

#endif //UI_GADGET_FILE_SELECTOR_H_
