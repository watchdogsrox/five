/////////////////////////////////////////////////////////////////////////////////
//
// FILE	   : UiGadgetFullscreenOverlay.cpp
// PURPOSE : A gadget used for overlaying the entire screen temporarily (e.g. debug loading).
//
// AUTHOR  : james.strain
//
// Copyright (C) 1999-2014 Rockstar Games.  All Rights Reserved.
//
/////////////////////////////////////////////////////////////////////////////////
#if __BANK

// rage
#include "grcore/viewport.h"
#include "vector/vector2.h"

// game
#include "UiGadgetFullscreenOverlay.h"

FRONTEND_OPTIMISATIONS();

CUiGadgetFullscreenOverlay::CUiGadgetFullscreenOverlay( CUiColorScheme const& colourScheme )
	: CUiGadgetBase()
	, m_background( 0.f, 0.f, 1.f, 1.f, colourScheme.GetColor( CUiColorScheme::COLOR_LIST_ENTRY_TEXT ) )
	, m_text( 0.f, 0.f, colourScheme.GetColor( CUiColorScheme::COLOR_WINDOW_TITLEBAR_TEXT ), NULL )
{
    SetPos( Vector2( 0.f, 0.f ) );
	grcViewport const * viewport = grcViewport::GetDefaultScreen();
	if( viewport )
	{
		m_background.SetSize( (float)viewport->GetWidth(), (float)viewport->GetHeight() );
	}

	m_text.SetScale( 4.f );
	Vector2 newPos( (m_background.GetWidth() / 2.f) - (m_text.GetWidth() / 2.f), 
		(m_background.GetHeight() / 2.f) - (m_text.GetHeight() / 2.f) );
	m_text.SetPos( newPos );

    AttachChild( &m_text );
    AttachChild( &m_background );

	SetActive( false );
}

CUiGadgetFullscreenOverlay::~CUiGadgetFullscreenOverlay()
{
	
}

rage::fwBox2D CUiGadgetFullscreenOverlay::GetBounds() const
{
	return m_background.GetBounds();
}

#endif //__BANK
