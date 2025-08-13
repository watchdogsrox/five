/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : WatermarkRenderer.h
// PURPOSE : Renders Scaleform watermark for video exports
// AUTHOR  : James Strain
//
/////////////////////////////////////////////////////////////////////////////////
#include "control/replay/ReplaySettings.h"
#include "video/VideoPlaybackSettings.h"

#ifndef WATERMARK_RENDERER__H
#define WATERMARK_RENDERER__H

#if defined( GTA_REPLAY ) && GTA_REPLAY

// game
#include "frontend/ui_channel.h"
#include "frontend/Scaleform/ScaleFormMgr.h"

class WatermarkRenderer
{
public: // declarations and variables

	enum eWATERMARK_SETUP_STATE
	{
		WATERMARK_SETUP_INVALID = -1,
		WATERMARK_SETUP_FIRST = 0,

		WATERMARK_SETUP_MOVIE_LOADED = WATERMARK_SETUP_FIRST,
		WATERMARK_SETUP_MOVIE_PREPARED,
		WATERMARK_SETUP_MOVIE_PLAYING,
		WATERMARK_SETUP_MOVIE_PAUSED,
		WATERMARK_SETUP_MOVIE_CLEANING_UP,

		WATERMARK_SETUP_MAX
	};

	enum eLOGO_TYPE
	{
		LOGO_TYPE_ROCKSTAR = 0,
		LOGO_TYPE_SOCIAL_CLUB = 1,
	};

public: // methods
	static void Init(unsigned initMode);
	static void Shutdown(unsigned shutdownMode);

	static void Update();
	static void Render();

	static void OpenMovie();

	inline static bool IsMovieIdValid() { return ms_watermarkMovieId != INVALID_MOVIE_ID; }
	inline static bool IsMovieLoaded() { return IsMovieIdValid() && ms_setupState >= WATERMARK_SETUP_MOVIE_LOADED; }
	inline static bool IsMoviePendingPrepare() { return IsMovieIdValid() && ms_setupState == WATERMARK_SETUP_MOVIE_LOADED; }
	inline static bool IsMoviePrepared() { return IsMovieIdValid() && ms_setupState == WATERMARK_SETUP_MOVIE_PREPARED; }
	inline static bool IsMoviePlaying() { return IsMovieIdValid() && ms_setupState == WATERMARK_SETUP_MOVIE_PLAYING; }
	inline static bool IsMoviePaused() { return IsMovieIdValid() && ms_setupState == WATERMARK_SETUP_MOVIE_PAUSED; }

	static void CloseMovie();

	static void PrepareForPlayback( char const * const lineOne, char const * const lineTwo, eLOGO_TYPE const logoType );
	static void CleanupPlayback();

	static void PlayWatermark();
	static void PauseWatermark();

private: // declarations and variables
	static s32						ms_watermarkMovieId;
	static eWATERMARK_SETUP_STATE	ms_setupState;

private: // methods
	
};

#endif // WATERMARK_RENDERER__H

#endif // #if defined( GTA_REPLAY ) && GTA_REPLAY
