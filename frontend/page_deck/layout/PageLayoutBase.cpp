/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : PageLayoutBase.cpp
// PURPOSE : Base for scene layout primitives
//
// AUTHOR  : james.strain
// STARTED : January 2021
//
/////////////////////////////////////////////////////////////////////////////////
#include "PageLayoutBase.h"

#include "frontend/page_deck/uiPageConfig.h"
#if UI_PAGE_DECK_ENABLED

#include "PageLayoutBase_parser.h"

// rage
#include "vector/geometry.h"

// framework
#include "fwutil/xmacro.h"

// game
#include "frontend/ui_channel.h"
#include "frontend/page_deck/layout/PageLayoutItem.h"
#include "frontend/page_deck/PageItemCategoryBase.h"
#include "frontend/VideoEditor/ui/InstructionalButtons.h"
#include "system/control_mapping.h"
#include "text/TextFile.h"

FWUI_DEFINE_TYPE( CPageLayoutBase, 0x57E69C05 );

void CPageLayoutBase::RegisterItem( CPageLayoutItemBase& item, CPageLayoutItemParams const& params )
{
    if( uiVerifyf( !IsItemRegistered( item ), "Attempting to register the same item twice?" ) )
    {
        m_registeredItems.PushAndGrow( &item );
        m_registeredItemParams.PushAndGrow( params );
    }
}

bool CPageLayoutBase::IsItemRegistered( CPageLayoutItemBase const& item ) const
{
    return GetIndex(item) >= 0;
}

void CPageLayoutBase::UnregisterItem( CPageLayoutItemBase const& item )
{
    int const c_idx = GetIndex(item);
    if( c_idx != INDEX_NONE )
    {
        m_registeredItems.Delete( c_idx );
        m_registeredItemParams.Delete( c_idx );
    }
}

void CPageLayoutBase::UnregisterAll()
{
    m_registeredItems.ResetCount();
    m_registeredItemParams.ResetCount();
}

void CPageLayoutBase::RecalculateLayout( Vec2f_In position, Vec2f_In extents )
{
    m_position = position;
    RecalculateLayoutInternal( extents );
    NotifyOfLayoutUpdate();
}

void CPageLayoutBase::NotifyOfLayoutUpdate()
{
    int const c_itemCount = m_registeredItems.GetCount();
    for( int index = 0; index < c_itemCount; ++index )
    {
        CPageLayoutItemBase& item = *m_registeredItems[index];
        CPageLayoutItemParams const& c_params = m_registeredItemParams[index];
        PositionItem( item, c_params );
    }
}

CPageLayoutItemParams CPageLayoutBase::GenerateParams( CPageItemBase const& pageItem ) const
{
    return GenerateParamsDerived( pageItem );
}

char const * const CPageLayoutBase::GetSymbol( CPageItemBase const& pageItem ) const
{
    return GetSymbolDerived( pageItem );
}

void CPageLayoutBase::SetDefaultFocus()
{
    int const c_defaultFocusIdx = GetDefaultItemFocusIndex();

    rage::fwuiNavigationResult const c_result = GetNavDetailsForItem( c_defaultFocusIdx );
    if( uiVerifyf( c_result.IsValid(), "Unable to set default focus to item %d", c_defaultFocusIdx ) )
    {
        CPageLayoutItemBase * const focusItem = GetItemByIndex( c_defaultFocusIdx );
        if( uiVerifyf( focusItem, "Unable to find item %d despite earlier calls telling us to focus it", c_defaultFocusIdx ) )
        {
            OnNavigatedToItem( nullptr, focusItem );
        }

        m_navigationContext.AssignFocusItem( c_result );
    }
}

void CPageLayoutBase::InvalidateFocus()
{
    rage::fwuiNavigationResult const c_oldFocus = m_navigationContext.GetFocusedItemDetails();
    m_navigationContext.ClearFocus();
    if( c_oldFocus.IsValid() )
    {
        CPageLayoutItemBase * const oldFocusItem = GetItemByIndex( c_oldFocus.GetItemIndex() );
        OnNavigatedToItem( oldFocusItem, nullptr );
    }
}

void CPageLayoutBase::RefreshFocusVisuals()
{
    rage::fwuiNavigationResult const c_currentFocus = m_navigationContext.GetFocusedItemDetails();
    if( c_currentFocus.IsValid() )
    {
        CPageLayoutItemBase * const currentFocusItem = GetItemByIndex( c_currentFocus.GetItemIndex() );
        OnNavigatedToItem( nullptr, currentFocusItem );
    }
}

void CPageLayoutBase::ClearFocusVisuals()
{
    rage::fwuiNavigationResult const c_currentFocus = m_navigationContext.GetFocusedItemDetails();
    if( c_currentFocus.IsValid() )
    {
        CPageLayoutItemBase * const currentFocusItem = GetItemByIndex( c_currentFocus.GetItemIndex() );
        OnNavigatedToItem( currentFocusItem, nullptr );
    }
}

char const * CPageLayoutBase::GetFocusTooltip() const
{
    char const * result = nullptr;

    rage::fwuiNavigationResult const c_currentFocus = m_navigationContext.GetFocusedItemDetails();
    if( c_currentFocus.IsValid() )
    {
        CPageLayoutItemBase const * const c_currentFocusItem = GetItemByIndex( c_currentFocus.GetItemIndex() );
        result = c_currentFocusItem ? c_currentFocusItem->GetTooltop() : nullptr;
    }

    return result;
}

void CPageLayoutBase::UpdateInstructionalButtons( atHashString const acceptLabelOverride ) const
{
    rage::fwuiNavigationResult const c_currentFocus = m_navigationContext.GetFocusedItemDetails();
    if( c_currentFocus.IsValid() )
    {
        CPageLayoutItemBase const * const c_focusItem = GetItemByIndex( c_currentFocus.GetItemIndex() );
        if( c_focusItem && c_focusItem->IsEnabled() )
        {
            char const * const c_label = uiPageConfig::GetLabel( ATSTRINGHASH( "IB_SELECT", 0xD7ED7F0C ), acceptLabelOverride );
            CVideoEditorInstructionalButtons::AddButton( INPUT_FRONTEND_ACCEPT, c_label, true );
        }
    }
}

void CPageLayoutBase::PlayEnterAnimation()
{
    PlayEnterAnimationDerived();
}

fwuiInput::eHandlerResult CPageLayoutBase::HandleDigitalNavigation( int const deltaX, int const deltaY )
{
    fwuiInput::eHandlerResult result( fwuiInput::ACTION_NOT_HANDLED );

    rage::fwuiNavigationResult const c_oldFocus = m_navigationContext.GetFocusedItemDetails();

    rage::fwuiNavigationConfig const& c_navConfig = m_navigationContext.GetConfig();
    rage::fwuiNavigationParams const c_navParams( c_navConfig, c_oldFocus, deltaX, deltaY );

    rage::fwuiNavigationResult const c_newFocus = HandleDigitalNavigationDerived( c_navParams );
    if( m_navigationContext.AssignFocusItem( c_newFocus ) )
    {
        CPageLayoutItemBase * const oldFocusItem = GetItemByIndex( c_oldFocus.GetItemIndex() );
        CPageLayoutItemBase * const newFocusItem = GetItemByIndex( c_newFocus.GetItemIndex() );
        OnNavigatedToItem( oldFocusItem, newFocusItem );
        result = fwuiInput::ACTION_HANDLED;
    }

    return result;
}

fwuiInput::eHandlerResult CPageLayoutBase::HandleInputAction( eFRONTEND_INPUT const inputAction, IPageMessageHandler& messageHandler )
{
    fwuiInput::eHandlerResult result( fwuiInput::ACTION_NOT_HANDLED );

    rage::fwuiNavigationResult const c_currentFocus = m_navigationContext.GetFocusedItemDetails();
    if( c_currentFocus.IsValid() )
    {
        CPageLayoutItemBase * const currentFocusItem = GetItemByIndex( c_currentFocus.GetItemIndex() );
        if( currentFocusItem )
        {
            result = OnActionItem( *currentFocusItem, inputAction, messageHandler );
        }
    }

    return result;
}

#if RSG_BANK

void CPageLayoutBase::DebugRender() const
{
    DebugRenderDerived();

    int const c_count = m_registeredItems.GetCount();
    for( int index = 0; index < c_count; ++index )
    {
        CPageLayoutItemBase const * const c_item = m_registeredItems[index];
        c_item->DebugRender();
    }
}

#endif // RSG_BANK

CPageLayoutItemBase * CPageLayoutBase::GetItemByIndex( int const index )
{
    CPageLayoutItemBase * const result = index >= 0 && index < m_registeredItems.GetCount() ? m_registeredItems[index] : nullptr;
    return result;
}

CPageLayoutItemBase const * CPageLayoutBase::GetItemByIndex( int const index ) const
{
    CPageLayoutItemBase const * const c_result = index >= 0 && index < m_registeredItems.GetCount() ? m_registeredItems[index] : nullptr;
    return c_result;
}

CPageLayoutItemParams const * CPageLayoutBase::GetItemParamsByIndex( int const index ) const
{
    CPageLayoutItemParams const * const c_result = index >= 0 && index < m_registeredItemParams.GetCount() ? &m_registeredItemParams[index] : nullptr;
    return c_result;
}

int CPageLayoutBase::GetIndex( CPageLayoutItemBase const& item ) const
{
    int result = INDEX_NONE;

    int const c_count = m_registeredItems.GetCount();
    for( int index = 0; result == INDEX_NONE && index < c_count; ++index )
    {
        result = m_registeredItems[index] == &item ? index : result;
    }

    return result;
}

int CPageLayoutBase::GetClosestIntersectingItemIdx( Vec2f_In p0, Vec2f_In p1, Vec2f_In p2, int const indexToSkip ) const
{
    int result = INDEX_NONE;

    float oldAngle = LARGE_FLOAT;
    float oldLength = LARGE_FLOAT;

    int const c_childCount = GetItemCount();
    Vec2f const c_sideAShim = p1 - p0;
    Vector2 const c_sideA( c_sideAShim.GetX(), c_sideAShim.GetY() );
    Vec2f const c_sideBShim = p2 - p0;
    Vector2 const c_sideB( c_sideBShim.GetX(), c_sideBShim.GetY() );

    for( int index = 0; index < c_childCount; ++index )
    {
        // We may be calling this function to figure out child-to-child navigation and will want to skip the child we are starting from
        // (which naturally will always intersect the triangle)
        if( index == indexToSkip )
            continue;

        Vec2f itemPos, itemSize;
        GetItemPositionAndSize( index, itemPos, itemSize );

        float const c_x0 = itemPos.GetX();
        float const c_y0 = itemPos.GetY();
        float const c_x1 = itemPos.GetX() + itemSize.GetX();
        float const c_y1 = itemPos.GetY() + itemSize.GetY();

        bool const c_intersects = geom2D::Test2DTriVsAlignedRect( p0.GetX(), p0.GetY(), p1.GetX(), p1.GetY(), p2.GetX(), p2.GetY(),
            c_x0, c_y0, c_x1, c_y1 );
        if( c_intersects )
        {
            Vec2f const c_childCenter = itemPos + ( itemSize * 0.5f );
            Vec2f const c_diffRay = c_childCenter - p0;

            // Shimming from RDR code, where Vec2f and Vector2 were merged
            Vector2 const c_diffRayShim( c_diffRay.GetX(), c_diffRay.GetY() );
            if( c_diffRayShim.IsNonZero() )
            {
                float const c_length = c_diffRayShim.Mag2();
                float const c_angleA = c_diffRayShim.Angle( c_sideA );
                float const c_angleB = c_diffRayShim.Angle( c_sideB );
                float const c_angle = fabs(c_angleB-c_angleA);

                // Pick the closest length, and favor a centered element
                if( c_length <= oldLength && c_angle <= oldAngle )
                {
                    oldAngle = c_angle;
                    oldLength = c_length;
                    result = index;
                }
            }
            else
            {
                continue;
            }
        }
    }

    return result;
}

void CPageLayoutBase::GetItemPositionAndSize( int const index, Vec2f_InOut out_pos, Vec2f_InOut out_size ) const
{
    GetItemPositionAndSizeDerived( index, out_pos, out_size );
}

char const * const CPageLayoutBase::GetSymbolDerived( CPageItemBase const& pageItem ) const
{
    return pageItem.GetSymbol();
}

void CPageLayoutBase::ForEachLayoutItemDerived(PerLayoutItemLambda action)
{
    int const c_itemCount = GetItemCount();
    for( int index = 0; index < c_itemCount; ++index )
    {
        CPageLayoutItemBase * const layoutItem = m_registeredItems[index];
        if( layoutItem )
        {
            action( *layoutItem );
        }
    }
}

void CPageLayoutBase::ForEachLayoutItemDerived(PerLayoutItemConstLambda action) const
{
    int const c_itemCount = GetItemCount();
    for( int index = 0; index < c_itemCount; ++index )
    {
        CPageLayoutItemBase const * const c_layoutItem = m_registeredItems[index];
        if( c_layoutItem )
        {
            action( *c_layoutItem );
        }
    }
}

void CPageLayoutBase::PlayEnterAnimationDerived()
{
    int itemIndex = 0;
    auto itemFunc = [&itemIndex]( CPageLayoutItemBase& layoutItem )
    {
        layoutItem.PlayRevealAnimation( itemIndex );
        ++itemIndex;
    };

    ForEachLayoutItemDerived( itemFunc );
}

int CPageLayoutBase::GetDefaultItemFocusIndex() const
{
    int result = INDEX_NONE;

    int const c_count = m_registeredItems.GetCount();
    for( int index = 0; result == INDEX_NONE && index < c_count; ++index )
    {
        result = m_registeredItems[index]->IsInteractive() ? index : result;
    }

    return result;
}

rage::fwuiNavigationResult CPageLayoutBase::GetNavDetailsForItem( int const index ) const
{
    int const c_itemCount = GetItemCount();
    CPageLayoutItemParams const * const c_params = c_itemCount > 0 && index < c_itemCount ? GetItemParamsByIndex( index ) : nullptr;
    if( c_params )
    {
        return rage::fwuiNavigationResult( c_params->GetX(), c_params->GetY(), index );
    }
    else
    {
        return rage::fwuiNavigationResult();
    }
}

CPageLayoutItemParams CPageLayoutBase::GenerateParamsDerived( CPageItemBase const& UNUSED_PARAM(pageItem) ) const
{
    return CPageLayoutItemParams();
}

void CPageLayoutBase::OnNavigatedToItem( CPageLayoutItemBase* const oldItem, CPageLayoutItemBase * const newItem )
{
    if( oldItem )
    {
        oldItem->OnFocusLost();
    }

    if( newItem )
    {
        newItem->OnFocusGained();
    }
}

fwuiInput::eHandlerResult CPageLayoutBase::OnActionItem( CPageLayoutItemBase& item, eFRONTEND_INPUT const inputAction, IPageMessageHandler& messageHandler )
{
    fwuiInput::eHandlerResult result( fwuiInput::ACTION_NOT_HANDLED );

    if( inputAction == FRONTEND_INPUT_ACCEPT )
    {
        if( item.IsEnabled() )
        {
            item.OnSelected( messageHandler );
            result = fwuiInput::ACTION_HANDLED;
        }
        else
        {
            result = fwuiInput::ACTION_IGNORED;
        }
    }

    return result;
}

rage::fwuiNavigationResult CPageLayoutBase::HandleDigitalNavigationDerived( rage::fwuiNavigationParams const& navParams )
{
    int const c_delta = navParams.GetDigitalDeltaY();
    bool const c_isWrapping = navParams.GetNavConfig().IsWrappingY();
    int const c_existingIndex = navParams.GetPreviousResult().GetItemIndex();

    int const c_newIndex = HandleDigitalNavigationSimple( c_delta, c_isWrapping, c_existingIndex );
    return rage::fwuiNavigationResult( INDEX_NONE, c_newIndex, c_newIndex );
}

int CPageLayoutBase::HandleDigitalNavigationSimple( int const delta, bool const isWrapping, int const existingIndex )
{
    int result = INDEX_NONE;

    int const c_itemCount = GetItemCount();
    if( c_itemCount > 0 )
    {
        result = existingIndex + delta;

        if( result < 0 )
        {
            result = isWrapping ? c_itemCount - 1 : 0;
        }

        if( result >= c_itemCount )
        {
            result = isWrapping ? 0 : c_itemCount - 1;
        }
    }

    return result;
}

#endif // UI_PAGE_DECK_ENABLED
