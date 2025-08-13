/////////////////////////////////////////////////////////////////////////////////
//
// FILE	   : UiGadgetVideoEditorPane.h
// PURPOSE : A compound gadget used for editing a video project.
//
// AUTHOR  : james.strain
//
// Copyright (C) 1999-2013 Rockstar Games.  All Rights Reserved.
//
/////////////////////////////////////////////////////////////////////////////////
#include "control/replay/ReplaySettings.h"

#if defined( GTA_REPLAY ) && GTA_REPLAY

#ifndef UI_GADGET_VIDEO_EDITOR_PANE_H_
#define UI_GADGET_VIDEO_EDITOR_PANE_H_

#if __BANK

// rage
#include "atl/hashstring.h"

// game
#include "debug/UiGadget/UiGadgetBase.h"
#include "debug/UiGadget/UiGadgetBox.h"
#include "debug/UiGadget/UiGadgetButton.h"
#include "debug/UiGadget/UiGadgetImage.h"
#include "debug/UiGadget/UiGadgetText.h"
#include "UiGadgetFileSelector.h"

class CClip;
class CRawClipFileView;
class CVideoEditorProject;

class CUiGadgetVideoEditorPane : public CUiGadgetBase
{
public: // methods
	CUiGadgetVideoEditorPane( float const fX, float const fY, CUiColorScheme const& colourScheme );
	virtual ~CUiGadgetVideoEditorPane();

	void cleanup();
	void PrepareForEditing();

	virtual void Update();
	virtual void Draw() { }

	virtual fwBox2D GetBounds() const;

private: // declarations and variables
	static size_t const sc_randomStringCount = 2;
	static char const * const sc_randomText[sc_randomStringCount];

	enum EditorState
	{
		INVALID,

		EDITING,
		VIEWING_CLIP,
		SELECTING_CLIP,

		MAX
	};

	CUiGadgetImage				m_clipPreviewA;
	CUiGadgetText				m_transitionLabel;
	CUiGadgetImage				m_clipPreviewB;
	CUiGadgetButton				m_clipSelectA;
	CUiGadgetButton				m_playClipA;
	CUiGadgetButton				m_transitionSelect;
	CUiGadgetButton				m_clipSelectB;
	CUiGadgetButton				m_playClipB;
	CUiGadgetFileSelector		m_clipSelector;
	CUiGadgetButton				m_toggleText;
	CUiGadgetButton				m_toggleFx;

	CUiGadgetBox				m_background;
	EditorState					m_state;
	EditorState					m_pendingState;

	CRawClipFileView*			m_clipFileView;
	int							m_clipSelectionIndex;
	
private: // methods

	static void onTransitionSelectPressed( void* callbackData );
	static void onRequestClipSelector( void* callbackData );
	static void onCancelClipSelector( void* callbackData );
	static void onConfirmClipSelector( void* callbackData );
	static void onPreviewClip( void* callbackData );
	static void onToggleText( void* callbackData );
	static void onToggleFx( void* callbackData );

	void onRequestClipSelector( CUiGadgetButton const* button );
	void onPreviewClip( CUiGadgetButton const* button );
	void onToggleText();
	void onToggleFx();

	void updateTextToggle( CVideoEditorProject const & project );
	void updateFxToggle( CVideoEditorProject const & project );
	static void updateClipPreview( CUiGadgetImage& targetImage, CVideoEditorProject& project, u32 const clipIndex );
	static void releaseClipPreview( CUiGadgetImage& targetImage, CVideoEditorProject& project, u32 const clipIndex );

	void updateClipInSlot();
	void setState( EditorState stateToSet );
};

#endif //__BANK

#endif //UI_GADGET_VIDEO_EDITOR_PANE_H_

#endif // defined( GTA_REPLAY ) && GTA_REPLAY
