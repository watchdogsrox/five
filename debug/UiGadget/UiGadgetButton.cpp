/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : UiGadgetButton.cpp
// PURPOSE : Simple UI button with text label and background
//
// AUTHOR  : james.strain
//
// Copyright (C) 1999-2013 Rockstar Games.  All Rights Reserved.
//
/////////////////////////////////////////////////////////////////////////////////
#include "UiGadgetButton.h"

#if __BANK

// rage
#include "grcore/debugdraw.h"

// game
#include "UiColorScheme.h"

CUiGadgetButton::CUiGadgetButton( float x, float y, float widthPadding, float heightPadding, 
								 const CUiColorScheme& colorScheme, char const * const labelText )
	: m_normalColour( colorScheme.GetColor( CUiColorScheme::COLOR_LIST_BG1 ) )
	, m_pressedColour( colorScheme.GetColor( CUiColorScheme::COLOR_LIST_SELECTOR ) )
	, m_label( x + widthPadding, y + heightPadding, colorScheme.GetColor( CUiColorScheme::COLOR_LIST_ENTRY_TEXT ), labelText )
	, m_shadowBox( x, y, widthPadding + m_label.GetWidth() + widthPadding, heightPadding + m_label.GetHeight() + heightPadding, colorScheme.GetColor( CUiColorScheme::COLOR_LIST_ENTRY_TEXT ) )
	, m_backgroundBox( x + 1.f, y + 1.f, m_shadowBox.GetWidth() - 2.f, m_shadowBox.GetHeight() - 2.f, m_normalColour )
	, m_widthPadding( widthPadding )
	, m_heightPadding( heightPadding )
	, m_onDownCallback( NullCB )
	, m_onPressedCallback( NullCB )
	, m_onReleasedCallback( NullCB )
{
    SetPos( Vector2( x, y ) );
	m_backgroundBox.SetFaded( true );

    AttachChild( &m_label );
    AttachChild( &m_backgroundBox );

	SetCanHandleEventsOfType(UIEVENT_TYPE_MOUSEL_PRESSED);
	SetCanHandleEventsOfType(UIEVENT_TYPE_MOUSEL_RELEASED);
	SetCanHandleEventsOfType(UIEVENT_TYPE_MOUSEL_SINGLECLICK);
}

void CUiGadgetButton::SetLabel( char const * const text )
{
	m_label.SetString( text );
	m_shadowBox.SetSize(  m_widthPadding + m_label.GetWidth() + m_widthPadding, m_heightPadding + m_label.GetHeight() + m_heightPadding );
	m_backgroundBox.SetSize( m_shadowBox.GetWidth() - 2.f, m_shadowBox.GetHeight() - 2.f );
}

void CUiGadgetButton::HandleEvent( const CUiEvent& uiEvent )
{
	if( !ContainsMouse() || !IsActive() )
		return;

	if( uiEvent.IsSet( UIEVENT_TYPE_MOUSEL_PRESSED ) )
	{
		m_backgroundBox.SetColor( m_pressedColour );
		m_onDownCallback.Call( this );
	}

	if( uiEvent.IsSet( UIEVENT_TYPE_MOUSEL_RELEASED ) )
	{
		m_backgroundBox.SetColor( m_normalColour );
		m_onReleasedCallback.Call( this );
	}

	if( uiEvent.IsSet( UIEVENT_TYPE_MOUSEL_SINGLECLICK ) )
	{
		m_onPressedCallback.Call( this );
	}
}

fwBox2D CUiGadgetButton::GetBounds() const
{
	return m_backgroundBox.GetBounds();
}


void CUiGadgetButton::Update()
{
	m_backgroundBox.SetFaded( !ContainsMouse() );
}

#endif	//__BANK
