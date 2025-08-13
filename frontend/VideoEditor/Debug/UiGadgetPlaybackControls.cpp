/////////////////////////////////////////////////////////////////////////////////
//
// FILE	   : UiGadgetPlaybackControls.cpp
// PURPOSE : A compound gadget used for controlling replay playback.
//
// AUTHOR  : james.strain
//
// Copyright (C) 1999-2013 Rockstar Games.  All Rights Reserved.
//
/////////////////////////////////////////////////////////////////////////////////
#include "UiGadgetPlaybackControls.h"

#if defined( GTA_REPLAY ) && GTA_REPLAY

#if __BANK

// game
#include "control/replay/replay.h"
#include "debug/UiGadget/UiColorScheme.h"
#include "frontend/VideoEditor/VideoEditorInterface.h"

FRONTEND_OPTIMISATIONS();

CUiGadgetPlaybackControls::CUiGadgetPlaybackControls( float const fX, float const fY, CUiColorScheme const& colourScheme )
	: CUiGadgetBase()
	, m_jumpToStartButton( fX, fY, 5.f, 5.f, colourScheme, "<<" )
	, m_rewindButton( m_jumpToStartButton.GetBounds().x1 + 1.f, fY, 5.f, 5.f, colourScheme, "<" )
	, m_playPauseButton( m_rewindButton.GetBounds().x1 + 7.f, fY, 5.f, 5.f, colourScheme, "Play" )
	, m_ffwdButton( m_playPauseButton.GetBounds().x1 + 7.f, fY, 5.f, 5.f, colourScheme, ">" )
	, m_jumpToEndButton( m_ffwdButton.GetBounds().x1 + 1.f, fY, 5.f, 5.f, colourScheme, ">>" )
	, m_background( fX, fY, 120.f, 16.f, colourScheme.GetColor( CUiColorScheme::COLOR_WINDOW_BG ) )
{
	SetJumpToStartCallback( datCallback(onJumpToStartButtonPressed) );
	SetRewindCallback( datCallback(onRewindButtonPressed) );
	SetPlayPauseCallback( datCallback(onPlayPauseButtonPressed) );
	SetFastForwardCallback( datCallback(onFastForwardButtonPressed) );
	SetJumpToEndCallback( datCallback(onJumpToEndButtonPressed) );

	AttachChild( &m_jumpToStartButton );
	AttachChild( &m_rewindButton );
	AttachChild( &m_playPauseButton );
	AttachChild( &m_ffwdButton );
	AttachChild( &m_jumpToEndButton );

	AttachChild( &m_background );
}

CUiGadgetPlaybackControls::~CUiGadgetPlaybackControls()
{

}

void CUiGadgetPlaybackControls::Update()
{
	m_playPauseButton.SetLabel( CVideoEditorInterface::GetPlaybackController().IsPaused() ? "Play" : "Pause" );
}

void CUiGadgetPlaybackControls::SetActive( bool bActive )
{
	CUiGadgetBase::SetActive( bActive );
	SetChildrenActive( bActive );
}

rage::fwBox2D CUiGadgetPlaybackControls::GetBounds() const
{
	return m_background.GetBounds();
}

void CUiGadgetPlaybackControls::onJumpToStartButtonPressed( void* callbackData )
{
	UNUSED_VAR( callbackData );
	CVideoEditorInterface::GetPlaybackController().JumpToStart();
}

void CUiGadgetPlaybackControls::onRewindButtonPressed( void* callbackData )
{
	UNUSED_VAR( callbackData );
	CVideoEditorInterface::GetPlaybackController().ApplyRewind();
}

void CUiGadgetPlaybackControls::onPlayPauseButtonPressed( void* callbackData )
{
	UNUSED_VAR( callbackData );
	CVideoEditorInterface::GetPlaybackController().TogglePlayOrPause();
}

void CUiGadgetPlaybackControls::onFastForwardButtonPressed( void* callbackData )
{
	UNUSED_VAR( callbackData );
	CVideoEditorInterface::GetPlaybackController().ApplyFastForward();
}

void CUiGadgetPlaybackControls::onJumpToEndButtonPressed( void* callbackData )
{
	UNUSED_VAR( callbackData );
	CVideoEditorInterface::GetPlaybackController().JumpToEnd();
}

#endif //__BANK

#endif // defined( GTA_REPLAY ) && GTA_REPLAY
