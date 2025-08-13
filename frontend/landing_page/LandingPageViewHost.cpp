/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : LandingPageViewHost.cpp
// PURPOSE : View portion of the landing page
//
// AUTHOR  : james.strain
// STARTED : October 2020
//
/////////////////////////////////////////////////////////////////////////////////
#include "LandingPageViewHost.h"

#if GEN9_LANDING_PAGE_ENABLED

// game
#include "frontend/ui_channel.h"
#include "frontend/page_deck/IPageMessageHandler.h"
#include "frontend/VideoEditor/ui/InstructionalButtons.h"
#include "streaming/streaming.h"
#include "system/controlMgr.h"

#define LANDING_PAGE_SF_MOVIE_NAME  "frontend_landing"
#define LANDING_PAGE_CODE_TXD		ATSTRINGHASH( "FRONTEND_LANDING_BASE", 0x2C19FF35 )

void CLandingPageViewHost::LoadPersistentAssets()
{
    if( uiVerifyf( m_movie.IsFree(), "Attempting to load persistent assets twice. This means a logic error in caller code" ) )
    {
        m_movie.CreateMovieAndWaitForLoad( SF_BASE_CLASS_GENERIC, LANDING_PAGE_SF_MOVIE_NAME, Vector2(0,0), Vector2(1.0f,1.0f), false );
        m_txdRequest.Request( g_TxdStore.FindSlotFromHashKey( LANDING_PAGE_CODE_TXD ), 
            g_TxdStore.GetStreamingModuleId(), STRFLAG_FORCE_LOAD | STRFLAG_PRIORITY_LOAD | STRFLAG_DONTDELETE );

        // Task exists to fix this up
        CVideoEditorInstructionalButtons::Init();
    }
}

bool CLandingPageViewHost::ArePersistentAssetsReady() const
{
    bool const c_movieReady = m_movie.IsActive();

    // Shimming here, as I am not changing engine code
    strRequest& nonConstRequest = const_cast<strRequest&>(m_txdRequest);
    bool const c_txdReady = nonConstRequest.HasLoaded();

    return c_movieReady && c_txdReady;
}

void CLandingPageViewHost::UnloadPersistentAssets()
{
    if( !m_movie.IsFree() )
    {
        // Task exists to fix this up
        CVideoEditorInstructionalButtons::Shutdown();
        ShutdownSceneRoot();
        m_movie.RemoveMovie();
        m_txdRequest.ClearRequiredFlags(STRFLAG_DONTDELETE);
        m_txdRequest.Release();
    }
}

void CLandingPageViewHost::Initialize( IPageMessageHandler& messageHandler )
{
    if( uiVerifyf( !IsInitialized(), "Attempting to double-initialize landing page view. This means a logic error in caller code" ) )
    {
        m_messageHandler = &messageHandler;
    }
}

void CLandingPageViewHost::Shutdown()
{
	UnloadPersistentAssets();
    m_messageHandler = nullptr;
}

bool CLandingPageViewHost::HandleMessage( uiPageDeckMessageBase& msg )
{
    bool result = false;

    if( m_messageHandler )
    {
        result = m_messageHandler->HandleMessage( msg );
    }

    return result;
}

void CLandingPageViewHost::UpdateDerived()
{
    if( ArePersistentAssetsReady() )
    {
        if( !IsSceneRootInitialized() )
        {
            m_movie.SetDisplayConfig(CScaleformMgr::SDC::ForceWidescreen | CScaleformMgr::SDC::UseFakeSafeZoneOnBootup);

            s32 const c_movieId = m_movie.GetMovieID();
            CScaleformMgr::UpdateMovieParams( c_movieId, uiPageConfig::GetScaleModeType() );

            CComplexObject sceneRoot = CComplexObject::GetStageRoot( c_movieId );
            InitializeSceneRoot( sceneRoot );
        }

        // TODO_UI - We may get a possible double-tap here when the replay editor is active
        // Re-visit once we can launch the replay editor from the landing page
        CVideoEditorInstructionalButtons::Update();
    }
    else
    {
        CStreaming::LoadAllRequestedObjects();
    }
}

#endif // GEN9_LANDING_PAGE_ENABLED
