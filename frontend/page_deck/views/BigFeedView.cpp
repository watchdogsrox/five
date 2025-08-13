/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : BigFeedView.cpp
// PURPOSE : Page view that renders the GTAV big feed.
//
// AUTHOR  : james.strain
// STARTED : June 2021
//
/////////////////////////////////////////////////////////////////////////////////
#include "BigFeedView.h"

#if GEN9_LANDING_PAGE_ENABLED

#include "BigFeedView_parser.h"

// framework
#include "fwutil/xmacro.h"

// game
#include "frontend/ui_channel.h"
#include "frontend/loadingscreens.h"
#include "frontend/NewHud.h"
#include "frontend/page_deck/IPageViewHost.h"
#include "network/SocialClubServices/SocialClubNewsStoryMgr.h"
#include "network/live/NetworkTelemetry.h"

FWUI_DEFINE_TYPE( CBigFeedView, 0x1C4F769A );

CBigFeedView::CBigFeedView()
    : superclass()
    , m_skipAction(nullptr)
    , m_topLevelLayout( CPageGridSimple::GRID_3x6 )
    , m_skipDelayMs( 0.f )
    , m_newsMovieId( INVALID_MOVIE_ID )
    , m_viewStartTimeMs(0)
    , m_skipDelayTotalTimeMs(0)
    , m_currentStoryHasLink( false )
    , m_metricNavReported(false)
{
    
}

CBigFeedView::~CBigFeedView()
{
    delete m_skipAction;
}

void CBigFeedView::RecalculateGrids()
{
    m_topLevelLayout.Reset();

    // Screen area is acceptable as our grids are all using star sizing
    Vec2f const c_screenExtents = uiPageConfig::GetScreenArea(); 
    m_topLevelLayout.RecalculateLayout( Vec2f( Vec2f::ZERO ), c_screenExtents );
    m_topLevelLayout.NotifyOfLayoutUpdate();
}

bool CBigFeedView::IsPriorityFeedMode() const
{
    bool const c_isPriorityFeedMode = SUIContexts::IsActive( BIG_FEED_PRIORITY_FEED_CONTEXT );
    return c_isPriorityFeedMode;
}

bool CBigFeedView::CanActionFeeds() const
{
    bool const c_isPriorityMode = IsPriorityFeedMode();
    return !c_isPriorityMode || m_skipDelayMs <= 0.f;
}

bool CBigFeedView::IsCurrentStoryWithLink() const
{
    CNetworkTransitionNewsController& newsController = CNetworkTransitionNewsController::Get();
    const char* url = newsController.IsDisplayingNews() ? newsController.GetCurrentNewsItemUrl() : nullptr;

    return url != nullptr && url[0] != '\0';
}

void CBigFeedView::RenderDerived() const
{
    CNetworkTransitionNewsController& newsController = CNetworkTransitionNewsController::Get();
    if( newsController.IsDisplayingNews() && newsController.HasValidNewsMovie() )
    {
        CLoadingScreens::RenderLoadingScreenMovie();

        s32 const c_movieId = newsController.GetNewsMovieId();
        CScaleformMgr::RenderMovie( c_movieId, fwTimer::GetSystemTimeStep(), true );

        CVideoEditorInstructionalButtons::Render();
    }
}

void CBigFeedView::OnEnterStartDerived()
{
    CLoadingScreens::LoadNewsMovie( m_newsMovieId );
    CLoadingScreens::ResetButtons();
    CLoadingScreens::SetNewsScreenOrder(false);

    m_currentStoryHasLink = false;
    m_metricNavReported = false;

    CNetworkTransitionNewsController& newsController = CNetworkTransitionNewsController::Get();
    newsController.ClearSeenNewsMask();
}

bool CBigFeedView::OnEnterUpdateDerived( float const UNUSED_PARAM(deltaMs) )
{
    bool complete = false;

    bool const c_movieLoaded = CScaleformMgr::IsMovieActive( m_newsMovieId );
    bool const c_loadingActive = CLoadingScreens::AreActive();
    if( c_movieLoaded && c_loadingActive )
    {
        CNetworkTransitionNewsController& newsController = CNetworkTransitionNewsController::Get();
        if( !newsController.IsActive() )
        {
            CNetworkTransitionNewsController::eMode const c_mode = IsPriorityFeedMode() ? CNetworkTransitionNewsController::MODE_CODE_PRIORITY : CNetworkTransitionNewsController::MODE_CODE_INTERACTIVE;
            newsController.InitControl( m_newsMovieId, c_mode );

            OnNewsMovieFirstLoaded();
        }

        complete = newsController.IsDisplayingNews();
    }

    return complete;
}

void CBigFeedView::OnEnterCompleteDerived()
{
    // The value depends on what the user has seen so we keep a copy for tracking
    m_skipDelayTotalTimeMs = IsPriorityFeedMode() ? (CNetworkNewsStoryMgr::Get().GetNewsSkipDelaySec() * 1000) : 0;
    m_skipDelayMs = static_cast<float>(m_skipDelayTotalTimeMs);

    m_viewStartTimeMs = sysTimer::GetSystemMsTime();
}

void CBigFeedView::UpdateDerived( float const deltaMs )
{
    UpdateAcceptButton(deltaMs);
    UpdateInput();
}

void CBigFeedView::UpdateInputDerived()
{
    CNetworkTransitionNewsController& newsController = CNetworkTransitionNewsController::Get();
    if( newsController.IsDisplayingNews() && newsController.GetNewsItemCount() > 1 )
    {
        int deltaX = 0;

        if( CPauseMenu::CheckInput( FRONTEND_INPUT_LEFT ) )
        {
            deltaX = -1;
        }
        else if( CPauseMenu::CheckInput( FRONTEND_INPUT_RIGHT ) )
        {
            deltaX = 1;
        }


        if( deltaX != 0 )
        {
            fwuiInput::eHandlerResult result = fwuiInput::ACTION_NOT_HANDLED;

            if( deltaX > 0 && !m_pipCounter.IsOnLastPip() )
            {
                newsController.ReadyNextRequest( true );
                m_pipCounter.IncrementCurrentItem();
                result = fwuiInput::ACTION_HANDLED;
            }
            else if( deltaX < 0 && !m_pipCounter.IsOnFirstPip() )
            {
                newsController.ReadyPreviousRequest( true );
                m_pipCounter.DecrementCurrentItem();
                result = fwuiInput::ACTION_HANDLED;
            }

            TriggerInputAudio( result, UI_AUDIO_TRIGGER_NAV_LEFT_RIGHT, nullptr );
        }
    }

    bool const c_interactive = CanActionFeeds();
    if( c_interactive )
    {
        if( IsCurrentStoryWithLink() && CPauseMenu::CheckInput(FRONTEND_INPUT_Y) )
        {
            IPageViewHost& viewHost = GetViewHost();
            const char* url = newsController.GetCurrentNewsItemUrl();
            const u32 trackingId = newsController.GetCurrentNewsItemTrackingId();
            atHashString metricType;
            uiPageDeckMessageBase*  message = uiCloudSupport::CreateLink(url, &metricType);

            if( message )
            {
                viewHost.HandleMessage(*message);

                fwuiInput::eHandlerResult result(fwuiInput::ACTION_HANDLED);
                TriggerInputAudio(result, UI_AUDIO_TRIGGER_SELECT, nullptr);

                ReportMetricNav(trackingId, metricType);

                delete message;
            }
        }

        if ( m_skipAction != nullptr && CPauseMenu::CheckInput(FRONTEND_INPUT_ACCEPT) )
        {
            IPageViewHost& viewHost = GetViewHost();
            viewHost.HandleMessage(*m_skipAction);

            fwuiInput::eHandlerResult result(fwuiInput::ACTION_HANDLED);
            TriggerInputAudio(result, UI_AUDIO_TRIGGER_SELECT, nullptr);
        }
    }
}

void CBigFeedView::UpdateInstructionalButtonsDerived()
{
    superclass::UpdateInstructionalButtonsDerived();

    bool const c_interactive = CanActionFeeds();
    if( c_interactive )
    {
        if( IsPriorityFeedMode() )
        {
            CVideoEditorInstructionalButtons::AddButton(rage::INPUT_FRONTEND_ACCEPT, TheText.Get(ATSTRINGHASH("IB_SKIP", 0x146CE8B7)), true);
        }

        m_currentStoryHasLink = IsCurrentStoryWithLink();
        if( m_currentStoryHasLink )
        {
            CVideoEditorInstructionalButtons::AddButton(rage::INPUT_FRONTEND_Y, TheText.Get(ATSTRINGHASH("IB_SELECT", 0xD7ED7F0C)), true);
        }
    }

    CNetworkTransitionNewsController& newsController = CNetworkTransitionNewsController::Get();
    if( newsController.IsDisplayingNews() && newsController.GetNewsItemCount() > 1 )
    {
        CVideoEditorInstructionalButtons::AddButton( rage::INPUT_FRONTEND_AXIS_X, TheText.Get( ATSTRINGHASH("IB_NAVIGATE", 0xADE7E851) ), true );
    }
}

void CBigFeedView::UpdateVisualsDerived()
{
    if( m_pipCounter.IsInitialized() )
    {
        
    }
}

void CBigFeedView::OnExitCompleteDerived()
{
    if( CScaleformMgr::IsMovieActive( m_newsMovieId ) )
    {
        OnNewsMovieShutdown();
        CLoadingScreens::ShutdownNewsMovie( m_newsMovieId );
    }

    // Report a back out if no click happened
    ReportMetricNav(0, ATSTRINGHASH("back", 0x37418674));

    // NOTE - We don't currently shut-down the loading screens

    CNetworkTransitionNewsController& newsController = CNetworkTransitionNewsController::Get();
    newsController.Reset();
}

void CBigFeedView::UpdateAcceptButton( float const deltaMs )
{
    bool const c_wasInteractive = CanActionFeeds();

    if ( m_skipDelayMs > 0.f )
    {
        m_skipDelayMs -= deltaMs;
    }

    bool const c_interactive = CanActionFeeds();
    if( c_interactive )
    {
        bool const c_withLink = IsCurrentStoryWithLink();

        if( c_withLink != m_currentStoryHasLink || !c_wasInteractive )
        {
            m_currentStoryHasLink = c_withLink;
            CVideoEditorInstructionalButtons::Refresh();
        }
    }
}

void CBigFeedView::OnNewsMovieFirstLoaded()
{
    CLoadingScreens::DisableLoadingScreenMovieProgressText();
    m_newsMovieRoot = CComplexObject::GetStageRoot( m_newsMovieId );
    SetupPipCounter();
}

void CBigFeedView::OnNewsMovieShutdown()
{
    m_pipCounter.Shutdown();
    m_topLevelLayout.UnregisterAll();
    m_newsMovieRoot.Release();
    CLoadingScreens::EnableLoadingScreenMovieProgressText();
}

void CBigFeedView::SetupPipCounter()
{
    if( !m_pipCounter.IsInitialized() )
    {
        m_pipCounter.Initialize( m_newsMovieRoot );

        CNetworkTransitionNewsController& newsController = CNetworkTransitionNewsController::Get();
        int const c_count = newsController.GetNewsItemCount();
        if( c_count > 0 )
        {
            m_pipCounter.SetCount( c_count );
            m_pipCounter.SetCurrentItem( 0 );
        }

        CPageLayoutItemParams const c_pipParams( 1, 3, 1, 1 );
        m_topLevelLayout.RegisterItem( m_pipCounter, c_pipParams );
        RecalculateGrids();
    }
}

void CBigFeedView::ReportMetricNav(const u32 trackingId, const atHashString leftBy)
{
    if (m_metricNavReported)
    {
        return;
    }

    m_metricNavReported = true;

    CNetworkTransitionNewsController& newsController = CNetworkTransitionNewsController::Get();

    bool const c_whatsNew = SUIContexts::IsActive(BIG_FEED_WHATS_NEW_PAGE_CONTEXT);
    u32 const c_timeNow = sysTimer::GetSystemMsTime();
    u32 const c_viewtimeMs = c_timeNow > m_viewStartTimeMs ? c_timeNow - m_viewStartTimeMs : 0;
    u32 const c_hoverTiles = newsController.GetSeenNewsMask();

    CNetworkTelemetry::AppendBigFeedNav(c_hoverTiles, leftBy, trackingId, c_viewtimeMs, m_skipDelayTotalTimeMs, c_whatsNew);
}

#endif //  GEN9_LANDING_PAGE_ENABLED
