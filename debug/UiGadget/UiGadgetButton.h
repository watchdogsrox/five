/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : UiGadgetButton.h
// PURPOSE : Simple UI button with text label and background
//
// AUTHOR  : james.strain
//
// Copyright (C) 1999-2013 Rockstar Games.  All Rights Reserved.
//
/////////////////////////////////////////////////////////////////////////////////
#ifndef _DEBUG_UIGADGET_UIGADGETBUTTON_H_
#define _DEBUG_UIGADGET_UIGADGETBUTTON_H_

#if __BANK

// rage
#include "data/callback.h"

// game
#include "debug/UiGadget/UiGadgetBase.h"
#include "debug/UiGadget/UiGadgetBox.h"
#include "debug/UiGadget/UiGadgetText.h"

class CUiColorScheme;

class CUiGadgetButton: public CUiGadgetBase
{
public:
	CUiGadgetButton( float x, float y, float widthPadding, float heightPadding, const CUiColorScheme& colorScheme, char const * const labelText = NULL );
	~CUiGadgetButton() {}

	inline void SetDownCallback( datCallback const& callback ) { m_onDownCallback = callback; }
	inline void SetPressedCallback( datCallback const& callback ) { m_onPressedCallback = callback; }
	inline void SetReleasedCallback( datCallback const& callback ) { m_onReleasedCallback = callback; }

	void SetLabel( char const * const text );
	
	virtual void HandleEvent(const CUiEvent& uiEvent);

	virtual fwBox2D GetBounds() const;

protected:
    virtual void Draw() {}
	virtual void Update();

private:
	Color32			m_normalColour;
	Color32			m_pressedColour;

	CUiGadgetText	m_label;
	CUiGadgetBox	m_shadowBox;
	CUiGadgetBox	m_backgroundBox;
	
	float			m_widthPadding;
	float			m_heightPadding;

	datCallback		m_onDownCallback;
	datCallback		m_onReleasedCallback;
	datCallback		m_onPressedCallback;
};

#endif	//__BANK

#endif	//_DEBUG_UIGADGET_UIGADGETBUTTON_H_
