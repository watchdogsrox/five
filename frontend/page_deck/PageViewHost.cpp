/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : PageViewHost.cpp
// PURPOSE : Host class for all visual pages. Has initial setup logic for 
//           scaleform and debug views
//
// AUTHOR  : james.strain
// STARTED : October 2020
//
/////////////////////////////////////////////////////////////////////////////////
#include "PageViewHost.h"

#if UI_PAGE_DECK_ENABLED

// framework
#include "fwsys/timer.h"
#include "fwutil/xmacro.h"

// game
#include "frontend/page_deck/IPageView.h"
#include "frontend/Scaleform/ScaleFormMgr.h"
#include "frontend/ui_channel.h"
#include "frontend/VideoEditor/ui/InstructionalButtons.h"
#include "renderer/color.h"

void CPageViewHost::Update()
{
    UpdateDerived();
}

void CPageViewHost::UpdateInstructionalButtons()
{
    // Add buttons for the current page
    int const c_count = m_activePages.GetCount();
    if( c_count > 0 )
    {
        IPageView * const currentPage = m_activePages.Top();
        if( currentPage )
        {
            currentPage->UpdateInstructionalButtons();
        }
    }

    // Then allow for any global requirements from the view host
    UpdateInstructionalButtonsDerived();
}

void CPageViewHost::Render()
{
    GRCDEVICE.Clear(true, CRGBA(0,0,0,255), false, 0.0f, 0);

    if( m_sceneRoot.IsActive() )
    {
        s32 const c_movieIdx = m_sceneRoot.GetMovieId();
        CScaleformMgr::RenderMovie( c_movieIdx, fwTimer::GetSystemTimeStep(), true );
    }

#if RSG_BANK
    grcStateBlock::SetRasterizerState( grcStateBlock::RS_NoBackfaceCull );
    grcStateBlock::SetDepthStencilState( grcStateBlock::DSS_IgnoreDepth );
    grcStateBlock::SetBlendState( grcStateBlock::BS_Default );

    PUSH_DEFAULT_SCREEN();

    static bool s_debugRender = false;
    if( s_debugRender )
    {
        DebugRender();
    }
    POP_DEFAULT_SCREEN();

    grcStateBlock::SetRasterizerState( grcStateBlock::RS_Default );
    grcStateBlock::SetDepthStencilState( grcStateBlock::DSS_Default );
#endif // RSG_BANK

    CVideoEditorInstructionalButtons::Render();

    int const c_count = m_activePages.GetCount();
    for( int index = 0; index < c_count; ++index )
    {
        IPageView* const currentPage = m_activePages[index];
        if( currentPage && currentPage->HasCustomRender() )
        {
            currentPage->Render();
        }
    }
}

void CPageViewHost::RegisterActivePage( IPageView& pageView )
{
    if( uiVerifyf( !IsRegistered( pageView ), "Attempting to double-register page. This is indicative of a caller logic error" ) )
    {
        m_activePages.PushAndGrow( &pageView );
    }
}

bool CPageViewHost::IsRegistered( IPageView const& pageView ) const
{
    return m_activePages.Find( const_cast<IPageView*>(&pageView) ) >= 0;
}

void CPageViewHost::UnregisterActivePage( IPageView& pageView )
{
    int const c_index = m_activePages.Find( &pageView );
    if( c_index > INDEX_NONE )
    {
        m_activePages.Delete( c_index );
    }
}

void CPageViewHost::InitializeSceneRoot( CComplexObject& sceneRoot )
{
    if( uiVerifyf( !IsSceneRootInitialized(), "Double initialzing scene. This implies caller logic errors" ) )
    {
        m_sceneRoot = sceneRoot;
    }
}

bool CPageViewHost::IsSceneRootInitialized() const
{
    return m_sceneRoot.IsActive();
}

void CPageViewHost::ShutdownSceneRoot()
{
    m_activePages.ResetCount();
    m_sceneRoot.Release();
}

#if RSG_BANK

void CPageViewHost::DebugRender()
{
    IPageView const * const c_top = m_activePages.GetCount() > 0 ? m_activePages.Top() : nullptr;
    if( c_top )
    {
        c_top->DebugRender();
    }
}

#endif // UI_PAGE_DECK_ENABLED

#endif // RSG_BANK
