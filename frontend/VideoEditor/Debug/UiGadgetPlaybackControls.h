/////////////////////////////////////////////////////////////////////////////////
//
// FILE	   : UiGadgetPlaybackControls.h
// PURPOSE : A compound gadget used for controlling replay playback.
//
// AUTHOR  : james.strain
//
// Copyright (C) 1999-2013 Rockstar Games.  All Rights Reserved.
//
/////////////////////////////////////////////////////////////////////////////////
#include "control/replay/ReplaySettings.h"

#if defined( GTA_REPLAY ) && GTA_REPLAY

#ifndef UI_GADGET_PLAYBACK_CONTROLS_H_
#define UI_GADGET_PLAYBACK_CONTROLS_H_

#if __BANK

// game
#include "debug/UiGadget/UiGadgetBase.h"
#include "debug/UiGadget/UiGadgetBox.h"
#include "debug/UiGadget/UiGadgetButton.h"

class CUiGadgetPlaybackControls : public CUiGadgetBase
{
public:
	CUiGadgetPlaybackControls( float const fX, float const fY, CUiColorScheme const& colourScheme );
	virtual ~CUiGadgetPlaybackControls();

	virtual void Update();
	virtual void Draw() { }

	virtual void SetActive(bool bActive);

	virtual fwBox2D GetBounds() const;

private: // declarations and variables
	CUiGadgetButton				m_jumpToStartButton;
	CUiGadgetButton				m_rewindButton;
	CUiGadgetButton				m_playPauseButton;
	CUiGadgetButton				m_ffwdButton;
	CUiGadgetButton				m_jumpToEndButton;
	CUiGadgetBox				m_background;

private: // methods

	inline void SetJumpToStartCallback( datCallback const& callback )	{ m_jumpToStartButton.SetReleasedCallback( callback ); }
	inline void SetRewindCallback( datCallback const& callback )		{ m_rewindButton.SetDownCallback( callback ); }
	inline void SetPlayPauseCallback( datCallback const& callback )		{ m_playPauseButton.SetReleasedCallback( callback ); }
	inline void SetFastForwardCallback( datCallback const& callback )	{ m_ffwdButton.SetDownCallback( callback ); }
	inline void SetJumpToEndCallback( datCallback const& callback )		{ m_jumpToEndButton.SetReleasedCallback( callback ); }

	static void onJumpToStartButtonPressed( void* callbackData );
	static void onRewindButtonPressed( void* callbackData );
	static void onPlayPauseButtonPressed( void* callbackData );
	static void onFastForwardButtonPressed( void* callbackData );
	static void onJumpToEndButtonPressed( void* callbackData );
};

#endif //__BANK

#endif //UI_GADGET_PLAYBACK_CONTROLS_H_

#endif // defined( GTA_REPLAY ) && GTA_REPLAY
