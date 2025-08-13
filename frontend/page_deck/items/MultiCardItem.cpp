/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : MultiCardItem.cpp
// PURPOSE : Extension of CardItem to provide multiple feature elements
//
// AUTHOR  : james.strain
// STARTED : May 2021
//
/////////////////////////////////////////////////////////////////////////////////
#include "MultiCardItem.h"
#if UI_PAGE_DECK_ENABLED
#include "MultiCardItem_parser.h"

// framework
#include "fwutil/xmacro.h"

// game
#include "frontend/ui_channel.h"

FWUI_DEFINE_TYPE( CMultiCardItem, 0xC708F0B1 );

void CMultiCardItem::ForEachFeatureTextureDerived(PerTextureLambda action)
{
    rage::u32 const c_count = GetFeatureItemCount();
    for( rage::u32 index = 0; index < c_count; ++index )
    {
        char const * const c_txd = GetFeatureTxd( index );
        char const * const c_texture = GetFeatureTexture( index );

        action( index, c_txd, c_texture );
    }
}

u32 const CMultiCardItem::GetFeatureItemCount() const
{
    // Factor for bad hand-editing and take the smaller array
    rage::u32 const c_count = rage::Min( m_featuredTxdCollection.GetCount(), m_featuredTextureCollection.GetCount() );
    return c_count;
}

char const * CMultiCardItem::GetFeatureTxd(rage::u32 const index) const 
{
    char const * const c_result = index < (rage::u32)m_featuredTxdCollection.GetCount() ? m_featuredTxdCollection[ index ].c_str() : nullptr;
    return c_result;
}

char const * CMultiCardItem::GetFeatureTexture(rage::u32 const index) const 
{
    char const * const c_result = index < (rage::u32)m_featuredTextureCollection.GetCount() ? m_featuredTextureCollection[ index ].c_str() : nullptr;
    return c_result;
}

#endif // UI_PAGE_DECK_ENABLED
