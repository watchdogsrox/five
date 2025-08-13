/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : PageGrid.cpp
// PURPOSE : Layout type for organizing items into grid of NxM slots, with
//           parGen adjustable configurations
//
// AUTHOR  : james.strain
// STARTED : January 2021
//
/////////////////////////////////////////////////////////////////////////////////
#include "PageGrid.h"
#include "frontend/page_deck/uiPageConfig.h"

#if UI_PAGE_DECK_ENABLED
#include "PageGrid_parser.h"

#include <stdint.h>

// rage
#include "grcore/im.h"
#include "grcore/quads.h"
#include "math/amath.h"
#include "vector/colors.h"

// framework
#include "fwlocalisation/templateString.h"
#include "fwutil/xmacro.h"

// game
#include "frontend/page_deck/layout/PageLayoutItem.h"
#include "frontend/ui_channel.h"

#define PAGE_GRID_RAYNAV_ANGLE_DEGREES (10)

float PageGridSizingPacket::GetDefaultSize() const
{
    float const c_minSize = GetMinSize();
    float const c_maxSize = GetMaxSize();
    float const c_initSize = IsFixedSize() ? GetSize() : 0.0f;
    float const c_clampedSize = Clamp<float>( c_initSize, c_minSize, c_maxSize );
    return c_clampedSize;
}

CPageGrid::CPageGrid()
    : superclass()
{

}

CPageGrid::~CPageGrid()
{
    
}

void CPageGrid::ResetRows()
{
    m_rows.ResetCount();
}

void CPageGrid::ResetColumns()
{
    m_columns.ResetCount();
}

void CPageGrid::AddRow( uiPageDeck::eGridSizingType const sizingType, float const size, float const minSize, float const maxSize )
{
    m_rows.PushAndGrow( PageGridRow( size, minSize, maxSize, sizingType ) );
}

void CPageGrid::AddCol( uiPageDeck::eGridSizingType const sizingType, float const size, float const minSize, float const maxSize )
{
    m_columns.PushAndGrow( PageGridCol( size, minSize, maxSize, sizingType ) );
}

void CPageGrid::GetCell( Vec2f_InOut out_pos, Vec2f_InOut out_size, CPageLayoutItemParams const& params ) const
{
    GetCell( out_pos, out_size, params.GetX(), params.GetY(), params.GetHorizontalSpan(), params.GetVerticalSpan() );
}

void CPageGrid::GetCell( Vec2f_InOut out_pos, Vec2f_InOut out_size, size_t const col, size_t const row, size_t const hSpan, size_t const vSpan ) const
{
    out_pos = Vec2f( Vec2f::ZERO );
    out_size = Vec2f( Vec2f::ZERO );

    size_t const c_colCount = m_columns.size();
    size_t const c_rowCount = m_rows.size(); 
    if( c_colCount == 0 || c_rowCount == 0 )
    {
        return;
    }
    uiFatalAssertf( col < c_colCount, "%" SIZETFMT "u/%" SIZETFMT "u is out of our column range", col, c_colCount );
    uiFatalAssertf( row < c_rowCount, "%" SIZETFMT "u/%" SIZETFMT "u is out of our row range", row, c_rowCount );

    // Grab the starting position
    PageGridCol const * const c_colBase = TryGetColumnDefinition( col );
    PageGridRow const * const c_rowBase = TryGetRowDefinition( row );

    float const c_x = c_colBase ? c_colBase->GetCalculatedOffset() : 0.f;
    float const c_y = c_rowBase ? c_rowBase->GetCalculatedOffset() : 0.f;
    out_pos.SetX( c_x );
    out_pos.SetY( c_y );

    Vec2f const c_basePos = GetPosition();
    out_pos += c_basePos;

    float width = c_colBase ? c_colBase->GetCalculatedSize() : 0.f;
    float height = c_rowBase ? c_rowBase->GetCalculatedSize() : 0.f;

    size_t const c_lastCol = col + hSpan;
    if( c_lastCol > (col + 1) )
    {
        for( size_t index = col + 1; index < c_colCount; ++index )
        {
            PageGridCol const * const c_currentCol = TryGetColumnDefinition( index );
            width += c_currentCol ? c_currentCol->GetCalculatedSize() : 0.f;

            if( index == c_lastCol )
            {
                break;
            }
        }
    }

    size_t const c_lastRow = row + vSpan;
    if( c_lastRow > (row + 1) )
    {
        for( size_t index = row + 1; index < c_rowCount; ++index )
        {
            PageGridRow const * const c_currentRow = TryGetRowDefinition( index );
            height += c_currentRow ? c_currentRow->GetCalculatedSize() : 0.f;

            if( index == c_lastRow )
            {
                break;
            }
        }
    }

    out_size.SetX( width );
    out_size.SetY( height );
}

void CPageGrid::ResetTo1x1()
{
    AddRow( uiPageDeck::SIZING_TYPE_STAR, 1.f );
    AddCol( uiPageDeck::SIZING_TYPE_STAR, 1.f );
}

char const * const CPageGrid::GetSymbolDerived( CPageItemBase const& pageItem ) const 
{
    char const * result = superclass::GetSymbolDerived( pageItem );
    return result ? result : "gridItem1x1";
}

CPageLayoutItemBase* CPageGrid::GetItemByCell( int const x, int const y, int const searchStartIndex, bool const useSpan  )
{
    CPageGrid const * const c_this = const_cast<CPageGrid const*>(this);
    CPageLayoutItemBase const * const c_result = c_this->GetItemByCell( x, y, searchStartIndex, useSpan );
    return const_cast<CPageLayoutItemBase*>(c_result);
}

CPageLayoutItemBase const * CPageGrid::GetItemByCell( int const x, int const y, int const searchStartIndex, bool const useSpan ) const
{
    CPageLayoutItemBase const * result( nullptr );

    int const c_itemCount = GetItemCount();
    for( int index = searchStartIndex; result == nullptr && index < c_itemCount; ++index )
    {
        CPageLayoutItemBase const * const c_currentCandidate = GetItemByIndex( index );
        CPageLayoutItemParams const * const c_params = GetItemParamsByIndex( index );

        int const c_column = c_params ? c_params->GetX() : -1;
        int const c_row = c_params ? c_params->GetY() : -1;

        if (c_column >= 0 && c_row >= 0)
        {
            if (!useSpan)
            {
                result = c_column == x && c_row == y ? c_currentCandidate : nullptr;
            }
            else
            {
                int const c_columnSpan = c_params->GetHorizontalSpan();
                int const c_rowSpan = c_params->GetVerticalSpan();
                result = (InRange<int>(x, c_column, c_column + c_columnSpan - 1) && InRange<int>(y, c_row, c_row + c_rowSpan - 1)) ? c_currentCandidate : nullptr;
            }
        }
    }

    return result;
}

CPageLayoutItemBase* CPageGrid::GetItemByColumn( int const x, int const ySuggested )
{
    CPageGrid const * const c_this = const_cast<CPageGrid const*>(this);
    CPageLayoutItemBase const * const c_result = c_this->GetItemByColumn( x, ySuggested );
    return const_cast<CPageLayoutItemBase*>(c_result);
}

CPageLayoutItemBase const* CPageGrid::GetItemByColumn( int const x, int const ySuggested ) const
{
    CPageLayoutItemBase const * result( nullptr );
    CPageLayoutItemBase const * closest( nullptr );
    int closestNodeY = INT32_MAX;

    int const c_itemCount = GetItemCount();
    for( int index = 0; result == nullptr && index < c_itemCount; ++index )
    {
        CPageLayoutItemBase const * const c_currentCandidate = GetItemByIndex( index );
        CPageLayoutItemParams const * const c_params = GetItemParamsByIndex( index );

        int const c_column = c_params ? c_params->GetX() : -1;
        int const c_row = c_params ? c_params->GetY() : -1;

        if( c_column == x )
        {
            // If we found the exact Y, take that
            result = c_row == ySuggested ? c_currentCandidate : nullptr;

            // Otherwise store the closest one we found
            int const c_diff = abs( ySuggested - c_row);
            if( c_diff < closestNodeY )
            {
                closest = c_currentCandidate;
                closestNodeY = c_diff;
            }
        }
    }

    return result ? result : closest;
}

CPageLayoutItemBase* CPageGrid::GetItemByRow( int const xSuggested, int const y )
{
    CPageGrid const * const c_this = const_cast<CPageGrid const*>(this);
    CPageLayoutItemBase const * const c_result = c_this->GetItemByRow( xSuggested, y );
    return const_cast<CPageLayoutItemBase*>(c_result);
}

CPageLayoutItemBase const* CPageGrid::GetItemByRow( int const xSuggested, int const y ) const
{
    CPageLayoutItemBase const * result( nullptr );
    CPageLayoutItemBase const * closest( nullptr );
    int closestNodeX = INT32_MAX;

    int const c_itemCount = GetItemCount();
    for( int index = 0; result == nullptr && index < c_itemCount; ++index )
    {
        CPageLayoutItemBase const * const c_currentCandidate = GetItemByIndex( index );
        CPageLayoutItemParams const * const c_params = GetItemParamsByIndex( index );

        int const c_column = c_params ? c_params->GetX() : -1;
        int const c_row = c_params ? c_params->GetY() : -1;

        if( c_row == y )
        {
            // If we found the exact Y, take that
            result = (size_t)c_column == xSuggested ? c_currentCandidate : nullptr;

            // Otherwise store the closest one we found
            int const c_diff = abs( (int)xSuggested - c_column );
            if( c_diff < closestNodeX )
            {
                closest = c_currentCandidate;
                closestNodeX = c_diff;
            }
        }
    }

    return result ? result : closest;
}

void CPageGrid::GetItemPositionAndSizeDerived( int const index, Vec2f_InOut out_pos, Vec2f_InOut out_size ) const 
{
    CPageLayoutItemParams const * const c_params = GetItemParamsByIndex( index );
    if( c_params )
    {
        GetCell( out_pos, out_size, *c_params );
    }
}

void CPageGrid::ForEachCol( PerColLambda action )
{
    for( ColumnIterator itr = m_columns.begin(); itr != m_columns.end(); ++itr )
    {
        action( *itr );
    }
}

void CPageGrid::ForEachCol( PerColConstLambda action ) const
{
    for( ConstColumnIterator itr = m_columns.begin(); itr != m_columns.end(); ++itr )
    {
        action( *itr );
    }
}

void CPageGrid::ForEachRow( PerRowLambda action )
{
    for( RowIterator itr = m_rows.begin(); itr != m_rows.end(); ++itr )
    {
        action( *itr );
    }
}

void CPageGrid::ForEachRow( PerRowConstLambda action ) const
{
    for( ConstRowIterator itr = m_rows.begin(); itr != m_rows.end(); ++itr )
    {
        action( *itr );
    }
}

rage::Vec2f CPageGrid::GetExtentsDerived() const 
{
    Vec2f pos = Vec2f( Vec2f::ZERO );
    Vec2f size = Vec2f( Vec2f::ZERO );

    GetCell( pos, size, 0, 0, GetColumnCount(), GetRowCount() );

    return pos + size;
}

void CPageGrid::RecalculateLayoutInternal( Vec2f_In extents )
{
    // Per col/row set, we calculate the exact sizes
    //
    // Scoping to avoid variable leakage
    // First pass we tot up the fixed sizes, and track our total star segments
    // Second pass we fill in the blanks for star sizes
    {
        float const c_totalWidth = extents.GetX();
        float fixedSizeWidth = 0.f;
        float totalStarWidths = 0.f;
        auto colFirstPassLambda = [&fixedSizeWidth, &totalStarWidths]( PageGridCol& currentColumn )
        {
            float const c_colSize = currentColumn.GetSize();
            if( currentColumn.IsFixedSize() )
            {
                currentColumn.SetCalculatedSizeClamped( c_colSize );
                fixedSizeWidth += currentColumn.GetCalculatedSize();
            }
            else
            {
                totalStarWidths += c_colSize;
                currentColumn.SetCalculatedSize( 0.f );
            }
        };
        ForEachCol( colFirstPassLambda );

        float const c_widthRemainder = rage::Max( c_totalWidth - fixedSizeWidth, 0.f );
        float const c_widthPerStar = totalStarWidths > 0.f ? c_widthRemainder / totalStarWidths : 0.f;
        float offsetPos = 0.f;
        auto colSecondPassLambda = [&c_widthPerStar,&offsetPos]( PageGridCol& currentColumn )
        {
            if( currentColumn.IsStarSize() )
            {
                float const c_calculatedSize = c_widthPerStar * currentColumn.GetSize();
                currentColumn.SetCalculatedSizeClamped( c_calculatedSize );
            }

            currentColumn.SetCalculatedOffset( offsetPos );
            offsetPos += currentColumn.GetCalculatedSize();
        };
        ForEachCol( colSecondPassLambda );
    }

    // Dupe of above block effectively.
    // I'm sure there is a clever template way to do it, but I'm tired :)
    {
        float const c_totalHeight = extents.GetY();
        float fixedSizeHeight = 0.f;
        float totalStarHeights = 0.f;
        auto rowFirstPassLambda = [&fixedSizeHeight, &totalStarHeights]( PageGridRow& currentRow )
        {
            float const c_rowSize = currentRow.GetSize();
            if( currentRow.IsFixedSize() )
            {
                currentRow.SetCalculatedSizeClamped( c_rowSize );
                fixedSizeHeight += currentRow.GetCalculatedSize();
            }
            else
            {
                totalStarHeights += c_rowSize;
                currentRow.SetCalculatedSize( 0.f );
            }
        };
        ForEachRow( rowFirstPassLambda );

        float const c_heightRemainder = rage::Max( c_totalHeight - fixedSizeHeight, 0.f );
        float const c_heightPerStar = totalStarHeights > 0.f ? c_heightRemainder / totalStarHeights : 0.f;
        float offsetPos = 0.f;
        auto rowSecondPassLambda = [&c_heightPerStar,&offsetPos]( PageGridRow& currentRow )
        {
            if( currentRow.IsStarSize() )
            {
                float const c_calculatedSize = c_heightPerStar * currentRow.GetSize();
                currentRow.SetCalculatedSizeClamped( c_calculatedSize );
            }

            currentRow.SetCalculatedOffset( offsetPos );
            offsetPos += currentRow.GetCalculatedSize();
        };
        ForEachRow( rowSecondPassLambda );
    }
}

void CPageGrid::PositionItem( CPageLayoutItemBase& item, CPageLayoutItemParams const& params ) const
{
    int const c_x = params.GetX();
    int const c_y = params.GetY();
    int const c_hSpan = params.GetHorizontalSpan();
    int const c_vSpan = params.GetVerticalSpan();

    Vec2f const c_selfPosition = GetPosition();
    Vec2f position, size;
    GetCell( position, size, (size_t)c_x, (size_t)c_y, (size_t)c_hSpan, (size_t)c_vSpan );
    item.ResolveLayout( position, position - c_selfPosition, size );
}

rage::fwuiNavigationResult CPageGrid::HandleDigitalNavigationDerived( rage::fwuiNavigationParams const& navParams )
{
    fwuiNavigationConfig const& navConfig = navParams.GetNavConfig();
    if( navConfig.ShouldForceRaycast() )
    {
        return HandleDigitalNavigationByRaycast( navParams );
    }

    return HandleDigitalNavigationByIndices( navParams );
}

rage::fwuiNavigationResult CPageGrid::HandleDigitalNavigationByRaycast( rage::fwuiNavigationParams const& navParams )
{
    int linearIndex = INDEX_NONE;
    int xResult = INDEX_NONE;
    int yResult = INDEX_NONE;

    if( GetItemCount() > 1 )
{

        // This code is proudly sponsored by RDR2, where it was written and tested before
        // being stolen to be used here :)
        int const c_existingIndex = navParams.GetPreviousResult().GetItemIndex();
        CPageLayoutItemParams const * const c_existingParams = GetItemParamsByIndex( c_existingIndex );
        if( c_existingParams )
        {
            fwuiNavigationConfig const& navConfig = navParams.GetNavConfig();

            CPageLayoutItemBase * const existingFocus = GetItemByIndex( c_existingIndex );

            Vec2f selfPos, selfSize;
            GetCell( selfPos, selfSize, 0, 0, GetColumnCount(), GetRowCount() );

            int const c_deltaX = navParams.GetDigitalDeltaX();
            int const c_deltaY = navParams.GetDigitalDeltaY();
            Vec2f delta( (float)c_deltaX, (float)c_deltaY );

            Vec2f existingItemPos, existingItemSize;
            GetCell( existingItemPos, existingItemSize, *c_existingParams );

            // We want a minor sweep to catch non-immediate focus targets (think non-uniform grids where the focused item
            // is 2 rows tall but the items to the right are 1 item tall each. A raycast directly right in a linear form
            // would go between the two and find no focus).
            Vec2f const c_p0 = existingItemPos + (existingItemSize * 0.5f );

            // Shimming from RDR code, where Vec2f and Vector2 were merged
            Vector2 delta1( delta.GetX(), delta.GetY() );
            delta1.Rotate( DtoR * PAGE_GRID_RAYNAV_ANGLE_DEGREES );
            Vec2f const c_delta1Shim( delta1.x, delta1.y );
            Vec2f const c_p1 = c_p0 + ( c_delta1Shim * selfSize );

            // Shimming from RDR code, where Vec2f and Vector2 were merged
            Vector2 delta2( delta.GetX(), delta.GetY() );
            delta2.Rotate( DtoR * -PAGE_GRID_RAYNAV_ANGLE_DEGREES );
            Vec2f const c_delta2Shim( delta2.x, delta2.y );
            Vec2f const c_p2 = c_p0 + (c_delta2Shim * selfSize);

            int const c_candidateIndex = GetClosestIntersectingItemIdx( c_p0, c_p1, c_p2, c_existingIndex );
            CPageLayoutItemBase * const candidateObject = GetItemByIndex( c_candidateIndex );
            if( candidateObject && candidateObject->IsInteractive() )
            {
                linearIndex = c_candidateIndex;
                CPageLayoutItemParams const * const c_candidateParams = GetItemParamsByIndex( linearIndex );
                xResult = c_candidateParams->GetX();
                yResult = c_candidateParams->GetY();
            }
            else // Ray-Casting failed, so do it the boring index way
            {
                CPageLayoutItemBase * newFocusedObject = nullptr;

                int const c_currentX = c_existingParams->GetX();
                int const c_currentY = c_existingParams->GetY();

                {
                    int const c_xDelta = navParams.GetDigitalDeltaX();
                    int const c_columnCount = (int)GetColumnCount();
                    bool const c_wrappingX = navConfig.IsWrappingX();

                    if( c_xDelta != 0 )
                    {
                        int iterations = c_wrappingX ? c_columnCount : c_xDelta < 0 ? c_currentX - 1 : c_columnCount - c_currentX;
                        for( int index = c_currentX + c_xDelta; iterations > 0; --iterations, index += c_xDelta )
                        {
                            // TODO_UI - Technically grids support multiple children at the same index, so we'd want to pick the correct one
                            CPageLayoutItemBase const * const c_candidateObject = GetItemByColumn( index, c_currentY );
                            if( c_candidateObject )
                            {
                                if( candidateObject == existingFocus && existingFocus != nullptr )
                                {
                                    break;
                                }
                                else if( c_candidateObject && c_candidateObject->IsInteractive() )
                                {
                                    linearIndex = GetIndex( *c_candidateObject );
                                    xResult = index;
                                    yResult = c_currentY;
                                    break;
                                }
                            }
                        }
                    }
                }

                if( newFocusedObject == nullptr )
                {
                    int const c_yDelta = navParams.GetDigitalDeltaY();
                    int const c_rowCount = (int)GetRowCount();
                    bool const c_wrappingY = navConfig.IsWrappingY();

                    if( c_yDelta != 0 )
                    {
                        int iterations = c_wrappingY ? c_rowCount : c_yDelta < 0 ? c_currentY - 1 : c_rowCount - c_currentY;
                        for( int index = c_currentY + c_yDelta; iterations > 0; --iterations, index += c_yDelta )
                        {
                            // TODO_UI - Technically grids support multiple children at the same index, so we'd want to pick the correct one
                            CPageLayoutItemBase * const candidateObject = GetItemByRow( c_currentX, index );
                            if( candidateObject )
                            {
                                if( candidateObject == existingFocus && existingFocus != nullptr )
                                {
                                    break;
                                }
                                else if( candidateObject && candidateObject->IsInteractive() )
                                {
                                    linearIndex = GetIndex( *candidateObject );
                                    xResult = c_currentX;
                                    yResult = index;
                                    break;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    return fwuiNavigationResult( xResult, yResult, linearIndex ); 
}


rage::fwuiNavigationResult CPageGrid::HandleDigitalNavigationByIndices( rage::fwuiNavigationParams const& navParams )
{
    fwuiNavigationResult result;

    if( GetItemCount() > 1 )
    {
        // This code is proudly sponsored by RDR2, where it was written and tested before
        // being stolen to be used here :)
        int const c_existingIndex = navParams.GetPreviousResult().GetItemIndex();
        CPageLayoutItemParams const * const c_existingParams = GetItemParamsByIndex( c_existingIndex );
        if( c_existingParams )
        {
            fwuiNavigationConfig const& navConfig = navParams.GetNavConfig();
            int const c_yDelta = navParams.GetDigitalDeltaY();
            int const c_xDelta = navParams.GetDigitalDeltaX();
            int const c_currentX = c_existingParams->GetX();
            int const c_currentY = c_existingParams->GetY();
            int const c_rowCount = (int)GetRowCount();
            int const c_columnCount = (int)GetColumnCount();
            bool const c_wrappingX = navConfig.IsWrappingX();
            bool const c_wrappingY = navConfig.IsWrappingY();
            int candidateX = c_currentX;
            int candidateY = c_currentY;

            CPageLayoutItemBase const * const c_existingFocus = GetItemByIndex( c_existingIndex );
            CPageLayoutItemBase const* candidateTarget = nullptr;
            bool hasWrapped = false;
            int searchStartIndex = 0;
            int maxIterations = c_columnCount * c_rowCount;

            while( !candidateTarget && InRange( candidateX, 0, c_columnCount ) && InRange( candidateY, 0, c_rowCount ) )
            {
                if( candidateTarget == nullptr && searchStartIndex == 0 )
                {
                    candidateX += c_xDelta;
                    candidateY += c_yDelta;

                    // Allow wrapping once while iterating. This would come into play with items that span multiple rows/columns, are on the edge of the wrapping behavior.
                    // We won't wrap on the first N checks, which would return the existing focus, but would want to still wrap once we're past its span.
                    if( !hasWrapped && ( !InRange( candidateX, 0, c_columnCount ) || !InRange( candidateY, 0, c_rowCount ) ) )
                    {
                        hasWrapped = true;
                        if( c_wrappingX )
                        {
                            candidateX = ( candidateX + c_columnCount ) % c_columnCount;
                        }
                        if( c_wrappingY )
                        {
                            candidateY = ( candidateY + c_rowCount ) % c_rowCount;
                        }
                    }
                }

                candidateTarget = GetItemByCell( candidateX, candidateY, searchStartIndex, true );
                searchStartIndex = 0;

                if( candidateTarget )
                {
                    // Filter out non-interactives, or re-focusing the currently focused object _if it spans multiple rows/columns, you'll never get to the next item).
                    int const c_candidateIdx = GetIndex( *candidateTarget );
                    bool const c_refocusedExisting = candidateTarget == c_existingFocus;
                    bool const c_targetFocusable = candidateTarget->IsInteractive();

                    if( !c_refocusedExisting && c_targetFocusable )
                    {
                        result = fwuiNavigationResult( candidateX, candidateY, c_candidateIdx );
                    }
                    else
                    {
                        // If we found a non-focusable child, search next time past the current 
                        searchStartIndex = c_targetFocusable ? 0 : c_candidateIdx + 1;
                        candidateTarget = nullptr;
                    }
                }

                // Escape hatch for if we have some garbage setup
                if( --maxIterations == 0 )
                {
                    uiFatalAssertf( false, "Abandoning grid navigation due to too many iterations" );
                    break;
                }
            }
        }
    }

    return result;
}

#if RSG_BANK

void CPageGrid::DebugRenderDerived() const 
{
    grcBindTexture(NULL);
    RenderGridCells();
}

void CPageGrid::RenderGridCells() const
{
    static const Color32 sc_gridColors[] =
    {
        Color_blue,
        Color_cyan,
        Color_azure,
        Color_grey,
        Color_DarkOrange,
        Color_SeaGreen,
        Color_PeachPuff,
        Color_VioletRed
    };
    static const size_t sc_colorCount = NELEM(sc_gridColors);

    static bool s_renderCells = true;
    if( s_renderCells )
    {
        size_t const c_columnCount = GetColumnCount();
        size_t const c_rowCount = GetRowCount();
        size_t currentColor = 0;
        for( size_t colIndex = 0; colIndex < c_columnCount; ++colIndex )
        {
            for( size_t rowIndex = 0; rowIndex < c_rowCount; ++rowIndex )
            {
                Vec2f pos, size;
                GetCell( pos, size, colIndex, rowIndex, 1, 1 );

                float const c_x0 = pos.GetX();
                float const c_y0 = pos.GetY();
                float const c_x1 = c_x0 + size.GetX();
                float const c_y1 = c_y0 + size.GetY();

                float const c_innerx0 = c_x0 + 1.f;
                float const c_innery0 = c_y0 + 1.f;
                float const c_innerx1 = c_x1 - 1.f;
                float const c_innery1 = c_y1 - 1.f;

                // Draw the row/column border, then the cell
                grcDrawSingleQuadf( c_x0, c_y0, c_x1, c_y1, 0.0f, 0.f, 0.f, 0.f, 0.f, Color_LimeGreen );
                grcDrawSingleQuadf( c_innerx0, c_innery0, c_innerx1, c_innery1, 0.0f, 0.f, 0.f, 0.f, 0.f, sc_gridColors[currentColor] );
                ++currentColor;
                currentColor = currentColor >= sc_colorCount ? 0 : currentColor;
            } 
        }
    }
}

#endif // RSG_BANK

#endif // UI_PAGE_DECK_ENABLED
