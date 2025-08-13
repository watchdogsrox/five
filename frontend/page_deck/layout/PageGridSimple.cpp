/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : PageGridSimple.cpp
// PURPOSE : Extension of the base grid that exposes some pre-defined grid configs
//           that are conveniently available via an enum.
//
// AUTHOR  : james.strain
// STARTED : January 2021
//
/////////////////////////////////////////////////////////////////////////////////
#include "PageGridSimple.h"

#include "frontend/page_deck/uiPageConfig.h"
#if UI_PAGE_DECK_ENABLED
#include "PageGridSimple_parser.h"

// rage
#include "math/amath.h"

// game
#include "frontend/ui_channel.h"

void CPageGridSimple::SetLayoutConfig( eType const newConfig )
{
    if( m_configuration != newConfig )
    {
        Reset();
    }
}

void CPageGridSimple::ResetDerived()
{
    ResetRows();
    ResetColumns();

    switch( m_configuration )
    {
    case GRID_1x1:
        {
            ResetTo1x1();
            break;
        }
    case GRID_1x1_ALIGNED:
        {
            // Default/Simple variant
            AddRow( uiPageDeck::SIZING_TYPE_STAR, 1.f );

            AddCol(uiPageDeck::SIZING_TYPE_STAR, 96.f);
            AddCol(uiPageDeck::SIZING_TYPE_STAR, 1728.f);
            AddCol(uiPageDeck::SIZING_TYPE_STAR, 96.f);
            break;
        }
    case GRID_3x1_INTERLACED_ALIGNED:
        {
            // Example use - Creator section of landing page
            // 1920x1080 units, but as "star size" it will scale
            AddRow( uiPageDeck::SIZING_TYPE_STAR, 1.f );

            AddCol( uiPageDeck::SIZING_TYPE_STAR, 96.f );
            AddCol( uiPageDeck::SIZING_TYPE_STAR, 855.f );
            AddCol( uiPageDeck::SIZING_TYPE_STAR, 20.f );
            AddCol( uiPageDeck::SIZING_TYPE_STAR, 855.f );
            AddCol( uiPageDeck::SIZING_TYPE_STAR, 96.f );
            break;
        }
    case GRID_3x5:
        {
            // This is used for framing main content at a top level primarily
            // Hence using the 1920x1080 "reference" to match UI Designs
            // 1920x1080 units, but as "star size" it will scale
            AddRow( uiPageDeck::SIZING_TYPE_STAR, 54.f );
            AddRow( uiPageDeck::SIZING_TYPE_STAR, 70.f );
            AddRow( uiPageDeck::SIZING_TYPE_STAR, 826.f );
            AddRow( uiPageDeck::SIZING_TYPE_STAR, 41.f );
            AddRow( uiPageDeck::SIZING_TYPE_STAR, 26.f );
            AddRow( uiPageDeck::SIZING_TYPE_STAR, 57.f );

            AddCol( uiPageDeck::SIZING_TYPE_STAR, 96.f );
            AddCol( uiPageDeck::SIZING_TYPE_STAR, 1728.f );
            AddCol( uiPageDeck::SIZING_TYPE_STAR, 96.f );
            break;
        }
    case GRID_3x6:
        {
            // This is used for framing content on the news pages
            // Hence using the 1920x1080 "reference" to match UI Designs
            // 1920x1080 units, but as "star size" it will scale
            AddRow( uiPageDeck::SIZING_TYPE_STAR, 54.f );
            AddRow( uiPageDeck::SIZING_TYPE_STAR, 70.f );
            AddRow( uiPageDeck::SIZING_TYPE_STAR, 756.f );
            AddRow( uiPageDeck::SIZING_TYPE_STAR, 70.f );
            AddRow( uiPageDeck::SIZING_TYPE_STAR, 41.f );
            AddRow( uiPageDeck::SIZING_TYPE_STAR, 26.f );
            AddRow( uiPageDeck::SIZING_TYPE_STAR, 57.f );

            AddCol( uiPageDeck::SIZING_TYPE_STAR, 96.f );
            AddCol( uiPageDeck::SIZING_TYPE_STAR, 1728.f );
            AddCol( uiPageDeck::SIZING_TYPE_STAR, 96.f );
            break;
        }
	case GRID_3x7:
	{		
		AddRow(uiPageDeck::SIZING_TYPE_STAR, 54.f);	
		AddRow(uiPageDeck::SIZING_TYPE_STAR, 70.f);
		AddRow(uiPageDeck::SIZING_TYPE_STAR, 10.f);
		AddRow(uiPageDeck::SIZING_TYPE_STAR, 565.f);
		AddRow(uiPageDeck::SIZING_TYPE_STAR, 238.f);
		AddRow(uiPageDeck::SIZING_TYPE_STAR, 26.f);
		AddRow(uiPageDeck::SIZING_TYPE_STAR, 26.f);

		AddCol(uiPageDeck::SIZING_TYPE_STAR, 96.f);
		AddCol(uiPageDeck::SIZING_TYPE_STAR, 1728.f);
		AddCol(uiPageDeck::SIZING_TYPE_STAR, 96.f);
		break;
	}
    case GRID_3x8:
    {
        // This is used for framing carousel content at a top level primarily
        // Hence using the 1920x1080 "reference" to match UI Designs
        // 1920x1080 units, but as "star size" it will scale
        AddRow( uiPageDeck::SIZING_TYPE_STAR, 54.f );
        AddRow( uiPageDeck::SIZING_TYPE_STAR, 40.f );
        AddRow( uiPageDeck::SIZING_TYPE_STAR, 30.f );
        AddRow( uiPageDeck::SIZING_TYPE_STAR, 70.f );
        AddRow( uiPageDeck::SIZING_TYPE_STAR, 585.f );
        AddRow( uiPageDeck::SIZING_TYPE_STAR, 218.f );
        AddRow( uiPageDeck::SIZING_TYPE_STAR, 26.f );
        AddRow( uiPageDeck::SIZING_TYPE_STAR, 57.f );

        AddCol(uiPageDeck::SIZING_TYPE_STAR, 96.f);
        AddCol(uiPageDeck::SIZING_TYPE_STAR, 1728.f);
        AddCol(uiPageDeck::SIZING_TYPE_STAR, 96.f);
        break;
    }
    case GRID_3x13:
    {	
        // This grid is used for the summary screen
        // It is under the nav tab so it only has 1920 x 906 to fill
        AddRow(uiPageDeck::SIZING_TYPE_STAR, 35.f);
        AddRow(uiPageDeck::SIZING_TYPE_STAR, 40.f);           
        AddRow(uiPageDeck::SIZING_TYPE_STAR, 40.f);         
        AddRow(uiPageDeck::SIZING_TYPE_STAR, 40.f);         
        AddRow(uiPageDeck::SIZING_TYPE_STAR, 30.f);
        AddRow(uiPageDeck::SIZING_TYPE_STAR, 20.f);
        AddRow(uiPageDeck::SIZING_TYPE_STAR, 20.f);		
        AddRow(uiPageDeck::SIZING_TYPE_STAR, 20.f);
        AddRow(uiPageDeck::SIZING_TYPE_STAR, 20.f);

        AddRow(uiPageDeck::SIZING_TYPE_STAR, 10.f);
        AddRow(uiPageDeck::SIZING_TYPE_STAR, 45.f);
            
        AddRow(uiPageDeck::SIZING_TYPE_STAR, 66.f);
        AddRow(uiPageDeck::SIZING_TYPE_STAR, 26.f);	

        AddCol(uiPageDeck::SIZING_TYPE_STAR, 96.f);
        AddCol(uiPageDeck::SIZING_TYPE_STAR, 1728.f);
        AddCol(uiPageDeck::SIZING_TYPE_STAR, 96.f);
        break;
    }
    case GRID_7x7_INTERLACED:
        {
            // 1920x800 units, as it fills in the content
            // span in certain screens
            AddRow( uiPageDeck::SIZING_TYPE_STAR, 185.f );
            AddRow( uiPageDeck::SIZING_TYPE_STAR, 20.f );
            AddRow( uiPageDeck::SIZING_TYPE_STAR, 185.f );
            AddRow( uiPageDeck::SIZING_TYPE_STAR, 20.f );
            AddRow( uiPageDeck::SIZING_TYPE_STAR, 185.f );
            AddRow( uiPageDeck::SIZING_TYPE_STAR, 20.f );
            AddRow( uiPageDeck::SIZING_TYPE_STAR, 185.f );

            AddCol( uiPageDeck::SIZING_TYPE_STAR, 96.f );
            AddCol( uiPageDeck::SIZING_TYPE_STAR, 563.f );
            AddCol( uiPageDeck::SIZING_TYPE_STAR, 20.f );
            AddCol( uiPageDeck::SIZING_TYPE_STAR, 563.f );
            AddCol( uiPageDeck::SIZING_TYPE_STAR, 20.f );
            AddCol( uiPageDeck::SIZING_TYPE_STAR, 562.f );
            AddCol( uiPageDeck::SIZING_TYPE_STAR, 96.f );
            break;
        }
    case GRID_9x1:
        {
            // 1920x800 units, as it fills in the content
            // span in certain screens
            AddRow( uiPageDeck::SIZING_TYPE_STAR, 1.f );

            AddCol( uiPageDeck::SIZING_TYPE_STAR, 96.f );
            AddCol( uiPageDeck::SIZING_TYPE_STAR, 417.f );
            AddCol( uiPageDeck::SIZING_TYPE_STAR, 20.f );
            AddCol( uiPageDeck::SIZING_TYPE_STAR, 417.f );
            AddCol( uiPageDeck::SIZING_TYPE_STAR, 20.f );
            AddCol( uiPageDeck::SIZING_TYPE_STAR, 417.f );
            AddCol( uiPageDeck::SIZING_TYPE_STAR, 20.f );
            AddCol( uiPageDeck::SIZING_TYPE_STAR, 417.f );
            AddCol( uiPageDeck::SIZING_TYPE_STAR, 96.f );
            break;
        }	
    default:
        {
            uiAssertf( false, "Unknown grid mode %u specified", m_configuration );
            superclass::ResetDerived();
            break;
        }
    }
}

char const * const CPageGridSimple::GetSymbolDerived( CPageItemBase const& pageItem ) const 
{
    char const * result = m_defaultSymbol;
    if( result == nullptr || result[0] == rage::TEXT_CHAR_TERMINATOR )
    {
        result = superclass::GetSymbolDerived( pageItem );
    }
    return result;
}

CPageLayoutItemParams CPageGridSimple::GenerateParamsDerived( CPageItemBase const& UNUSED_PARAM(pageItem) ) const 
{
    u32 x = 0;
    u32 y = 0;
    u32 hSpan = 1;
    u32 vSpan = 1;

    int const c_itemCount = GetItemCount();
    
    switch( m_configuration )
    {
    case GRID_1x1_ALIGNED:
        {
            x = 1;
            break;
        }
    case GRID_3x1_INTERLACED_ALIGNED:
        {
            // We only expect two items, so just switch based on the count
            x = ( c_itemCount & 1 ) != 0 ? 3 : 1;
            break;
        }
    case GRID_7x7_INTERLACED:
        {
            // 3 Columns of content available, with padding in-between each
            u32 const c_xModulo = c_itemCount % 3;
            x = c_xModulo*2;
            
            // 4 rows of content available, with padding in-between each
            y = (c_itemCount / 3) * 2;
            break;
        }
    case GRID_9x1:
        {
            // We expect 4 items at most
            u32 const c_xModulo = c_itemCount % 4;

            // 4 cols of content available, with padding in-between each
            x = (c_xModulo * 2) + 1;
            break;
        }
    default:
        {
            // NOP
        }
    }

    return CPageLayoutItemParams( x, y, hSpan, vSpan );
}

#endif // UI_PAGE_DECK_ENABLED
