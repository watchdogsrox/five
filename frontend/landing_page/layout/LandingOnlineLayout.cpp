/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : LandingOnlineLayout.cpp
// PURPOSE : Specialized layout for the online section, as it wants very curated
//           dynamic layout.
//
// AUTHOR  : james.strain
// STARTED : February 2021
//
/////////////////////////////////////////////////////////////////////////////////
#include "LandingOnlineLayout.h"

#include "frontend/page_deck/uiPageConfig.h"
#if GEN9_LANDING_PAGE_ENABLED
#include "LandingOnlineLayout_parser.h"

// game
#include "frontend/page_deck/layout/PageLayoutItem.h"
#include "frontend/ui_channel.h"

FWUI_DEFINE_TYPE( CLandingOnlineLayout, 0xA6DF5798 );

void CLandingOnlineLayout::ResetDerived()
{
    SetConfiguration( CPageGridSimple::GRID_7x7_INTERLACED );
    superclass::ResetDerived();
}

/*
    We treat the items as follows

    0-3 : These are the 4 "Fixed" tiles 

    3+ : These are the dynamic tiles we expect from the cloud, ordered by priority TOP/LEFT 

    We have a 7x7 grid as follows

    Columns 
    0 - Padding
    1...3 Dynamic content or margins
    4 - Fixed Margin
    5 - Fixed Content
    6 - Padding

    Rows
    0-6 - Dynamic content or margins, except in Column 5 where we interleave fixed content
     _ ____________________ _
    |_|____|__|____|__|____|_|
    |_|____|__|____|__|____|_|
    |_|____|__|____|__|____|_|
    |_|____|__|____|__|____|_|
    |_|____|__|____|__|____|_|
    |_|____|__|____|__|____|_|
    |_|____|__|____|__|____|_|

    The following defines map the cells as strictly specified in the UI Designs
*/
#define LANDING_FIXED_ITEM_COUNT 4

#define LANDING_CONTENT_COL_COUNT 3
#define LANDING_CONTENT_ROW_COUNT 7

#define LANDING_FIRST_CONTENT_COLUMN 1
#define LANDING_SUB_CONTENT_RIGHT_COL 3
#define LANDING_MINI_CONTENT_RIGHT_COL 3
#define LANDING_FIXED_CONTENT_COLUMN 5

#define LANDING_FIRST_CONTENT_ROW 0
#define LANDING_SUB_CONTENT_BOTTOM_ROW 4
#define LANDING_MINI_CONTENT_MIDDLE_BOTTOM_ROW 4
#define LANDING_MINI_CONTENT_BOTTOM_ROW 6
#define LANDING_FIXED_CONTENT_ROW_SPAN 1

#define LANDING_PRIMARY_CONTENT_ROW_SPAN LANDING_CONTENT_ROW_COUNT
#define LANDING_SUB_CONTENT_ROW_SPAN 3
#define LANDING_WIDE_CONTENT_COL_SPAN 3

char const * const CLandingOnlineLayout::GetSymbolDerived( CPageItemBase const& pageItem ) const
{
    char const * result = superclass::GetSymbolDerived( pageItem );

    int itemCount = GetItemCount();
    if( itemCount >= LANDING_FIXED_ITEM_COUNT )
    {
        itemCount -= LANDING_FIXED_ITEM_COUNT;

        switch( m_layoutType )
        {
        case LAYOUT_ONE_PRIMARY:
            {
                result = "gridItem2x4";
                break;
            }
        case LAYOUT_TWO_PRIMARY:
            {
                result = "gridItem1x4";
                break;
            }
        case LAYOUT_ONE_PRIMARY_TWO_SUB:
            {
                result = ( itemCount == 0 ) ? "gridItem1x4" : "gridItem1x2";
                break;
            }
        case LAYOUT_ONE_WIDE_TWO_SUB:
            {
                result = ( itemCount == 0 ) ? "gridItem2x2" : "gridItem1x2";
                break;
            }
        case LAYOUT_ONE_WIDE_ONE_SUB_TWO_MINI:
            {
                if( itemCount == 0 )
                {
                    result = "gridItem2x2";
                }
                else if( itemCount == 1 )
                {
                    result = "gridItem1x2";
                }
                else
                {
                    result = "gridItem1x1";
                }
                break;
            }
		case LAYOUT_ONE_WIDE_TWO_MINI_ONE_SUB:
		{
			if (itemCount == 0)
			{
				result = "gridItem2x2";
			}
			else if (itemCount == 1 || itemCount == 2)
			{
				result = "gridItem1x1";
			}
			else
			{
				result = "gridItem1x2"; 
			}
			break;
		}
        case LAYOUT_FOUR_SUB:
            {
                result = "gridItem1x2";
                break;
            }
        default:
            {
                // NOP
            }
        }
    }
    else if ( m_layoutType == LAYOUT_FULLSCREEN )
    {
        result = result == nullptr ? "gridItem3x4" : result;
    }

    result = result == nullptr ? "gridItem1x1" : result;
    return result;
}

CPageLayoutItemParams CLandingOnlineLayout::GenerateParamsDerived( CPageItemBase const& UNUSED_PARAM(pageItem) ) const
{
    u32 x = LANDING_FIRST_CONTENT_COLUMN;
    u32 y = LANDING_FIRST_CONTENT_ROW;
    u32 hSpan = 1;
    u32 vSpan = 1;

    if( m_layoutType == LAYOUT_FULLSCREEN )
    {
        hSpan = LANDING_CONTENT_COL_COUNT;
        vSpan = LANDING_CONTENT_ROW_COUNT;
    }
    else
    {
        int itemCount = GetItemCount();
        if( itemCount < LANDING_FIXED_ITEM_COUNT )
        {
            x = LANDING_FIXED_CONTENT_COLUMN;
            y = LANDING_FIRST_CONTENT_ROW + (itemCount*2); // Single width item + margin means * 2
        }
        else
        {
            itemCount -= LANDING_FIXED_ITEM_COUNT;

            switch( m_layoutType )
            {
            case LAYOUT_ONE_PRIMARY:
                {
                    hSpan = LANDING_CONTENT_COL_COUNT;
                    vSpan = LANDING_PRIMARY_CONTENT_ROW_SPAN;
                    break;
                }
            case LAYOUT_TWO_PRIMARY:
                {
                    x = LANDING_FIRST_CONTENT_COLUMN + (itemCount*2); // Single width item + margin means * 2
                    vSpan = LANDING_PRIMARY_CONTENT_ROW_SPAN;
                    break;
                }
            case LAYOUT_ONE_PRIMARY_TWO_SUB:
                {
                    if( itemCount == 0 )
                    {
                        vSpan = LANDING_PRIMARY_CONTENT_ROW_SPAN;
                    }
                    else 
                    {
                        x = LANDING_FIRST_CONTENT_COLUMN + 2; // Single width item, so add item + margin
                        vSpan = LANDING_SUB_CONTENT_ROW_SPAN;

                        if( itemCount > 1 )
                        {
                            y = LANDING_SUB_CONTENT_BOTTOM_ROW;
                        }
                    }

                    break;
                }
            case LAYOUT_ONE_WIDE_TWO_SUB:
                {
                    vSpan = LANDING_SUB_CONTENT_ROW_SPAN;

                    if( itemCount == 0 )
                    {
                        hSpan = LANDING_WIDE_CONTENT_COL_SPAN;
                    }
                    else 
                    {
                        y = LANDING_SUB_CONTENT_BOTTOM_ROW;
                        x = ( itemCount == 1 ) ? LANDING_FIRST_CONTENT_COLUMN : LANDING_SUB_CONTENT_RIGHT_COL;
                    }

                    break;
                }
            case LAYOUT_ONE_WIDE_ONE_SUB_TWO_MINI:
                {
                    vSpan = LANDING_SUB_CONTENT_ROW_SPAN;

                    if( itemCount == 0 )
                    {
                        hSpan = LANDING_WIDE_CONTENT_COL_SPAN;
                    }
                    else 
                    {
                        if( itemCount == 1 )
                        {
                            y = LANDING_SUB_CONTENT_BOTTOM_ROW;
                        }
                        else
                        {
                            x = LANDING_MINI_CONTENT_RIGHT_COL;
                            vSpan = 1; // Mini item is one wide
                            if( itemCount == 2 )
                            {
                                y = LANDING_MINI_CONTENT_MIDDLE_BOTTOM_ROW;
                            }
                            else
                            {
                                y = LANDING_MINI_CONTENT_BOTTOM_ROW;
                            }
                        }
                    }
                    break;
                }
            case LAYOUT_ONE_WIDE_TWO_MINI_ONE_SUB:
                {
                    vSpan = LANDING_SUB_CONTENT_ROW_SPAN;

                    if (itemCount == 0)
                    {
                        hSpan = LANDING_WIDE_CONTENT_COL_SPAN;
                    }
                    else
                    {
                        if (itemCount == 1 || itemCount == 2 )
                        {
                            vSpan = 1; // Mini item is one wide
                            if (itemCount == 1)
                            {
                                y = LANDING_MINI_CONTENT_MIDDLE_BOTTOM_ROW;
                            }
                            else
                            {
                                y = LANDING_MINI_CONTENT_BOTTOM_ROW;
                            }
                        }
                        else
                        {
                            x = LANDING_MINI_CONTENT_RIGHT_COL;
                            y = LANDING_SUB_CONTENT_BOTTOM_ROW;
                        }
                    }
                    break;
                }
            case LAYOUT_FOUR_SUB:
                {
                    x = (itemCount & 1) != 0 ? LANDING_SUB_CONTENT_RIGHT_COL : LANDING_FIRST_CONTENT_COLUMN; 
                    y = itemCount < 2 ? LANDING_FIRST_CONTENT_ROW : LANDING_SUB_CONTENT_BOTTOM_ROW;
                    vSpan = LANDING_SUB_CONTENT_ROW_SPAN;
                    break;
                }
            default:
                {
                    // NOP
                }
            }
        }
    }

    return CPageLayoutItemParams( x, y, hSpan, vSpan );
}

int CLandingOnlineLayout::GetDefaultItemFocusIndex() const 
{
    // Other than fullscreen, our data set is the 4 static items THEN our content.
    // So we want to start our focus at item 4
    return m_layoutType == LAYOUT_FULLSCREEN ? 0 : LANDING_FIXED_ITEM_COUNT;
}

void CLandingOnlineLayout::PlayEnterAnimationDerived()
{
    int const c_itemCount = GetItemCount();
    if( c_itemCount < LANDING_FIXED_ITEM_COUNT )
    {
        // Simple version
        int itemIndex = 0;
        auto itemFunc = [&itemIndex]( CPageLayoutItemBase& layoutItem )
        {
            layoutItem.PlayRevealAnimation( itemIndex );
            ++itemIndex;
        };
        ForEachLayoutItemDerived( itemFunc );
    }
    else
    {
        // Due to our fixed item schema being first, we need to make some adjustments
        int itemIndex = 0;
        int const c_fixedItemOffset = c_itemCount - 1 - LANDING_FIXED_ITEM_COUNT;

        auto itemFunc = [&itemIndex,c_fixedItemOffset]( CPageLayoutItemBase& layoutItem )
        {
            int sequenceIndex = itemIndex - LANDING_FIXED_ITEM_COUNT;
            if( itemIndex < LANDING_FIXED_ITEM_COUNT )
            {
                sequenceIndex = itemIndex + c_fixedItemOffset;
            }

            layoutItem.PlayRevealAnimation( sequenceIndex );
            ++itemIndex;
        };
        ForEachLayoutItemDerived( itemFunc );
    }
}

#endif // GEN9_LANDING_PAGE_ENABLED
