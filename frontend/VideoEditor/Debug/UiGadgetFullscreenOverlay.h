/////////////////////////////////////////////////////////////////////////////////
//
// FILE	   : UiGadgetFullscreenOverlay.h
// PURPOSE : A gadget used for overlaying the entire screen temporarily (e.g. debug loading).
//
// AUTHOR  : james.strain
//
// Copyright (C) 1999-2014 Rockstar Games.  All Rights Reserved.
//
/////////////////////////////////////////////////////////////////////////////////
#ifndef UI_GADGET_FULLSCREEN_OVERLAY_H_
#define UI_GADGET_FULLSCREEN_OVERLAY_H_

#if __BANK

// game
#include "debug/UiGadget/UiGadgetBase.h"
#include "debug/UiGadget/UiGadgetBox.h"
#include "debug/UiGadget/UiColorScheme.h"
#include "debug/UiGadget/UiGadgetText.h"

class CUiGadgetFullscreenOverlay : public CUiGadgetBase
{
public:
	CUiGadgetFullscreenOverlay( CUiColorScheme const& colourScheme );
	virtual ~CUiGadgetFullscreenOverlay();

	void SetText( char const * const label ) { m_text.SetString( label ); }

	virtual void Update() {};
	virtual void Draw() {};

	virtual fwBox2D GetBounds() const;

private: // declarations and variables
	CUiGadgetBox				m_background;
	CUiGadgetText				m_text;

private: // methods
};

#endif //__BANK

#endif //UI_GADGET_FULLSCREEN_OVERLAY_H_
