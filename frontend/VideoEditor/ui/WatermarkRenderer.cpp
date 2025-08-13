/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : WatermarkRenderer.cpp
// PURPOSE : Renders Scaleform watermark for video exports
// AUTHOR  : James Strain
//
/////////////////////////////////////////////////////////////////////////////////
#include "frontend/VideoEditor/ui/WatermarkRenderer.h"

#if defined( GTA_REPLAY ) && GTA_REPLAY

// rage
#include "grcore/allocscope.h"
#include "system/param.h"

// framework
#include "fwsys/gameskeleton.h"
#include "fwsys/timer.h"

// game
#include "renderer/rendertargets.h"
#include "system/system.h"
#include "frontend/Scaleform/ScaleFormMgr.h"
#include "frontend/HudTools.h"

FRONTEND_OPTIMISATIONS();
//OPTIMISATIONS_OFF()

s32 WatermarkRenderer::ms_watermarkMovieId									= INVALID_MOVIE_ID;
WatermarkRenderer::eWATERMARK_SETUP_STATE WatermarkRenderer::ms_setupState	= WatermarkRenderer::WATERMARK_SETUP_INVALID;

#define WATERMARK_FILENAME "watermark"

#if __BANK
PARAM(disableExportWatermark, "Disable rendering of the R* Editor Watermark");
#endif

void WatermarkRenderer::Init( unsigned initMode )
{
	(void)initMode;
	CloseMovie();
}

void WatermarkRenderer::Shutdown( unsigned shutdownMode )
{
	if ( shutdownMode == rage::SHUTDOWN_CORE )
	{
		CloseMovie();
	}
}

void WatermarkRenderer::Update()
{
	// NOP
}

void WatermarkRenderer::Render()
{
#if __BANK
	if( PARAM_disableExportWatermark.Get() )
	{
		return;
	}
#endif

	if (!CSystem::IsThisThreadId(SYS_THREAD_RENDER))  // only on RT
	{
		uiAssertf(0, "WatermarkRenderer::Render() can only be called on the RenderThread!");
		return;
	}

	if( IsMoviePlaying() && CScaleformMgr::IsMovieActive( ms_watermarkMovieId ) )
	{
		GRC_ALLOC_SCOPE_AUTO_PUSH_POP();
		CScaleformMgr::RenderMovie( ms_watermarkMovieId, fwTimer::GetSystemTimeStep(), true );
	}
}

void WatermarkRenderer::OpenMovie()
{
	if( !CScaleformMgr::IsMovieActive( ms_watermarkMovieId ) )
	{
		ms_watermarkMovieId = CScaleformMgr::CreateMovie( WATERMARK_FILENAME, Vector2( 0.f, 0.f ), Vector2( 1.f, 1.f ) );

		ms_setupState = uiVerifyf( ms_watermarkMovieId != INVALID_MOVIE_ID, "CVideoEditorUi - couldnt load '%s' movie!", WATERMARK_FILENAME ) ?
			WATERMARK_SETUP_MOVIE_LOADED : WATERMARK_SETUP_INVALID;
	}
}

void WatermarkRenderer::CloseMovie()
{
	CleanupPlayback();

	if( CScaleformMgr::IsMovieActive( ms_watermarkMovieId ) )
	{
		CScaleformMgr::RequestRemoveMovie(ms_watermarkMovieId);	
	}

	ms_watermarkMovieId = INVALID_MOVIE_ID;
	ms_setupState = WATERMARK_SETUP_INVALID;
}

void WatermarkRenderer::PrepareForPlayback( char const * const lineOne, char const * const lineTwo, eLOGO_TYPE const logoType )
{
	if( IsMoviePendingPrepare() && 
		CScaleformMgr::IsMovieActive( ms_watermarkMovieId ) )
	{
		CScaleformMgr::ForceCollectGarbage( ms_watermarkMovieId );
		CScaleformMgr::SetMovieDisplayConfig( ms_watermarkMovieId, SF_BASE_CLASS_VIDEO_EDITOR );

		if( CScaleformMgr::BeginMethod( ms_watermarkMovieId, SF_BASE_CLASS_VIDEO_EDITOR, "SET_WATERMARK_TYPE" ) )
		{
			CScaleformMgr::AddParamInt( (s32)logoType );
			CScaleformMgr::AddParamFloat( CHudTools::GetUIWidth() );
			CScaleformMgr::AddParamFloat( CHudTools::GetUIHeight() );
			CScaleformMgr::EndMethod();
		}

		if( CScaleformMgr::BeginMethod( ms_watermarkMovieId, SF_BASE_CLASS_VIDEO_EDITOR, "SET_WATERMARK_LABELS" ) )
		{
			CScaleformMgr::AddParamString( lineOne );
			CScaleformMgr::AddParamString( lineTwo );
			CScaleformMgr::EndMethod();
		}

		ms_setupState = WATERMARK_SETUP_MOVIE_PREPARED;
	}
}

void WatermarkRenderer::CleanupPlayback()
{
	ms_setupState = WATERMARK_SETUP_MOVIE_CLEANING_UP;

	if( IsMovieIdValid() && CScaleformMgr::IsMovieActive( ms_watermarkMovieId ) &&
		CScaleformMgr::BeginMethod( ms_watermarkMovieId, SF_BASE_CLASS_VIDEO_EDITOR, "dispose" ) )
	{
		CScaleformMgr::EndMethod();
	}

	ms_setupState = WATERMARK_SETUP_MOVIE_LOADED;
}

void WatermarkRenderer::PlayWatermark()
{
	char const * const c_functionToCall = IsMoviePrepared() ? "START_ANIMATION" : IsMoviePaused() ? "RESUME_ANIMATION" : NULL;
	
	if( c_functionToCall && CScaleformMgr::IsMovieActive( ms_watermarkMovieId ) &&
		 CScaleformMgr::BeginMethod( ms_watermarkMovieId, SF_BASE_CLASS_VIDEO_EDITOR, c_functionToCall ) )
	{
		CScaleformMgr::EndMethod();
		ms_setupState = WATERMARK_SETUP_MOVIE_PLAYING;
	}
}

void WatermarkRenderer::PauseWatermark()
{
	if( IsMoviePlaying() && CScaleformMgr::IsMovieActive( ms_watermarkMovieId ) &&
		CScaleformMgr::BeginMethod( ms_watermarkMovieId, SF_BASE_CLASS_VIDEO_EDITOR, "PAUSE_ANIMATION" ) )
	{
		CScaleformMgr::EndMethod();
		ms_setupState = WATERMARK_SETUP_MOVIE_PAUSED;
	}
}

#endif // defined( GTA_REPLAY ) && GTA_REPLAY
