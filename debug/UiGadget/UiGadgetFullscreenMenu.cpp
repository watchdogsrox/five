/////////////////////////////////////////////////////////////////////////////////
//
// FILE	   : UiGadgetFullscreenMenu.cpp
// PURPOSE : Acts as a fullscreen container for displaying a simple menu
//
// AUTHOR  : james.strain
// STARTED : 16/10/2020
//
/////////////////////////////////////////////////////////////////////////////////
#if __BANK

// rage
#include "grcore/viewport.h"
#include "vector/vector2.h"

// game
#include "UiGadgetFullscreenMenu.h"

FRONTEND_OPTIMISATIONS();

CUiGadgetFullscreenMenu::CUiGadgetFullscreenMenu()
    : superclass()
    , m_background( 0.f, 0.f, 1.f, 1.f, Color32( 0, 0, 0 ) )
    , m_title( 0.f, 0.f,  Color32( 255, 255, 255 ), NULL )
{
    Initialize();
}

CUiGadgetFullscreenMenu::CUiGadgetFullscreenMenu( CUiColorScheme const& colourScheme )
	: superclass()
	, m_background( 0.f, 0.f, 1.f, 1.f, colourScheme.GetColor( CUiColorScheme::COLOR_LIST_ENTRY_TEXT ) )
	, m_title( 0.f, 0.f, colourScheme.GetColor( CUiColorScheme::COLOR_WINDOW_TITLEBAR_TEXT ), NULL )
{
    Initialize();
}

CUiGadgetFullscreenMenu::~CUiGadgetFullscreenMenu()
{
    CleanupMenuItems();
}

void CUiGadgetFullscreenMenu::AddMenuItem( char const * const label, datCallback const& callback )
{
    const float fButtonPaddingX = 5.0f;
    const float fButtonPaddingY = 5.0f;

    CUiColorScheme colorScheme;
    Color32 white(255, 255, 255);
    colorScheme.SetColor(CUiColorScheme::COLOR_LIST_ENTRY_TEXT, white);

    Vector2 const c_pos = m_menuItemContainer.GetPos();
    CUiGadgetButton* button = rage_new CUiGadgetButton( c_pos.x, c_pos.y, fButtonPaddingX, fButtonPaddingY, colorScheme, label );
    button->SetDownCallback( callback );
    m_menuItemContainer.AttachChild( button );

    UpdateLayout();
}

void CUiGadgetFullscreenMenu::RemoveAllMenuItems()
{
    CleanupMenuItems();
}

void CUiGadgetFullscreenMenu::Initialize()
{
    SetPos( Vector2( 0.f, 0.f ) );
    m_menuItemContainer.SetPos( Vector2( 0.f, 0.f ) );
    m_background.SetPos( Vector2( 0.f, 0.f ) );
    m_title.SetPos( Vector2( 0.f, 0.f ) );

    float width = 0.f;
    float height = 0.f;
    grcViewport const * viewport = grcViewport::GetDefaultScreen();
    if( viewport )
    {
        width = (float)viewport->GetWidth();
        height = (float)viewport->GetHeight();
    }

    m_background.SetSize( width, height );
    m_title.SetScale( 2.f );
    UpdateTitleLayout();

    AttachChild( &m_menuItemContainer );
    AttachChild( &m_title );
    AttachChild( &m_background );

    SetActive( false );
}

void CUiGadgetFullscreenMenu::UpdateTitleLayout()
{
    float const c_bgHalfHeight = m_background.GetHeight() / 2.f;
    float const c_textHeight = m_title.GetHeight();
    float const c_textHalfHeight = c_textHeight / 2.f;

    Vector2 newPos( (m_background.GetWidth() / 2.f) - (m_title.GetWidth() / 2.f), 
        c_bgHalfHeight - c_textHalfHeight );
    m_title.SetPos( newPos );

    m_menuItemContainer.SetPos( Vector2( 0.f, c_bgHalfHeight + c_textHeight ), true );
}

void CUiGadgetFullscreenMenu::UpdateMenuItemLayout()
{
    Vector2 const c_pos = m_menuItemContainer.GetPos();
    int const c_childCount = m_menuItemContainer.GetChildCount();
    Vector2 const c_contentArea ( m_background.GetWidth() - c_pos.x, m_background.GetHeight() - c_pos.y );

    if( c_childCount <= 4 )
    {
        // For just a few items we have a simpler divide approach, which looks neater
        UpdateMenuItemLayout_Small( c_childCount, c_pos, c_contentArea );
    }
    else
    {
        // This is for the case if we decide to have long lists of options here
        UpdateMenuItemLayout_Large( c_childCount, c_pos, c_contentArea );
    }
}

void CUiGadgetFullscreenMenu::UpdateMenuItemLayout_Small( int const childCount, Vector2 const& startPos, Vector2 const& contentArea )
{
    float const c_offsetX = (contentArea.x / (childCount * 2 ) );
    float const c_offsetY = startPos.y + (contentArea.y / 2.f);

    float initialX = startPos.x + c_offsetX;

    for( int index = 0; index < childCount; ++index )
    {
        CUiGadgetBase * const menuItem = m_menuItemContainer.TryGetChildByIndex( index );
        FatalAssert(menuItem);

        fwBox2D const c_bounds = menuItem->GetBounds();
        float const c_halfWidth = ( c_bounds.x1 - c_bounds.x0 ) / 2.f;
        float const c_halfHeight = ( c_bounds.y1 - c_bounds.y0 ) / 2.f;

        float const c_posX = initialX - c_halfWidth;
        float const c_posY = c_offsetY - c_halfHeight;
        Vector2 const c_finalPosition(c_posX, c_posY);
        menuItem->SetPos( c_finalPosition, true);

        initialX += c_offsetX * 2;
    }
}

void CUiGadgetFullscreenMenu::UpdateMenuItemLayout_Large( int const childCount, Vector2 const& startPos, Vector2 const& UNUSED_PARAM(contentArea) )
{
    float const c_hozPadding = 20.f;
    float accumulatedWidth = c_hozPadding;
    float accumulatedHeight = 0.f;

    for( int index = 0; index < childCount; ++index )
    {
        CUiGadgetBase * const menuItem = m_menuItemContainer.TryGetChildByIndex( index );
        FatalAssert(menuItem);

        float const c_posX = startPos.x + accumulatedWidth;
        float const c_posY = startPos.y + accumulatedHeight;
        Vector2 const c_finalPosition(c_posX, c_posY);
        menuItem->SetPos( c_finalPosition, true);

        fwBox2D const c_bounds = menuItem->GetBounds();
        accumulatedWidth += ( c_bounds.x1 - c_bounds.x0 ) + c_hozPadding;
    }
}

void CUiGadgetFullscreenMenu::CleanupMenuItems()
{
    while( m_menuItemContainer.GetChildCount() )
    {
        CUiGadgetBase * menuItem = m_menuItemContainer.TryGetChildByIndex( 0 );
        m_menuItemContainer.DetachChild( menuItem );
        delete menuItem;
    }
}

#endif //__BANK
