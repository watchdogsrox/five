/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : ParallaxCardItem.cpp
// PURPOSE : Card specialization for parallax visuals
//
// AUTHOR  : james.strain
// STARTED : July 2021
//
/////////////////////////////////////////////////////////////////////////////////
#include "ParallaxCardItem.h"
#if UI_PAGE_DECK_ENABLED
#include "ParallaxCardItem_parser.h"

// framework
#include "fwutil/xmacro.h"

// game
#include "frontend/ui_channel.h"
#include "frontend/landing_page/LandingPageArbiter.h"

FWUI_DEFINE_TYPE(CParallaxCardItem, 0x3744BE0D);

void CParallaxCardItem::UpdateVisualsDerived(CComplexObject& displayObject)
{
    if( !IsTextureSet() )
    {
        // url:bugstar: 7271948
        // There is no "RemoveParallax" on the Flash side, so we can't call this multiple times once set
        // but also we don't _need_ to reset it yet. So skip it for now
        // SetupCardTextures();
        MarkTextureSet( true );
    }

    if( !AreDetailsSet() )
    {
        SetupCardDetails(displayObject);
    }
}

void CParallaxCardItem::SetupCardVisuals(CComplexObject& displayObject)
{
    SetupCardDetails(displayObject);
    SetupCardTextures(displayObject);    
}

void CParallaxCardItem::SetupCardDetails(CComplexObject& displayObject)
{
    char const * const c_title = GetItemTitleText();
    char const * const c_description = GetDescriptionText();

    CScaleformMgr::BeginMethodOnComplexObject(displayObject.GetId(), SF_BASE_CLASS_GENERIC, "SET_GRID_ITEM_DETAILS");
    CScaleformMgr::AddParamString(c_title, false);
    CScaleformMgr::AddParamString(c_description, false);
    CScaleformMgr::EndMethod();

    MarkDetailsSet( true );
}

void CParallaxCardItem::SetupCardTextures(CComplexObject& displayObject)
{
    IItemTextureInterface const * const c_textureInterface = GetTextureInterface();
    if( c_textureInterface )
    {
        COMPLEX_OBJECT_ID const c_objectId = displayObject.GetId();

        char const * const c_txd = c_textureInterface->GetTxd();
        auto textureFunc = [c_objectId,c_txd]( u32 const UNUSED_PARAM(index), char const * const backgroundTexture, char const * const foregroundTexture, int const parallaxType )
        {
            CScaleformMgr::BeginMethodOnComplexObject(c_objectId, SF_BASE_CLASS_GENERIC, "ADD_PARALLAX_ITEM");
            CScaleformMgr::AddParamString(c_txd, false);
            CScaleformMgr::AddParamString(backgroundTexture, false);
            CScaleformMgr::AddParamString(c_txd, false);
            CScaleformMgr::AddParamString(foregroundTexture, false);
            CScaleformMgr::AddParamInt( parallaxType );
            CScaleformMgr::EndMethod();
        };

        ForEachTextureInfo( textureFunc ); 

        CScaleformMgr::BeginMethodOnComplexObject( c_objectId, SF_BASE_CLASS_GENERIC, "PLAY_ANIMATION");
        CScaleformMgr::EndMethod();

        MarkTextureSet( true );
    }
}

void CParallaxCardItem::ForEachTextureInfo(ParallaxInfoLambda action)
{
    int const c_count = GetTexturePairCount();
    for( int index = 0; index < c_count; ++index )
    {
        ParallaxTextureInfo const * const c_info = GetTextureInfo( index );
        if( c_info )
        {
            action( index, c_info->m_backgroundTexture, c_info->m_foregroundTexture, c_info->m_parallaxType );
        }
    }
}

int const CParallaxCardItem::GetTexturePairCount() const
{
    int const c_count = m_textureInfoCollection.GetCount();
    return c_count;
}

CParallaxCardItem::ParallaxTextureInfo const * CParallaxCardItem::GetTextureInfo(int const index) const
{
    ParallaxTextureInfo const * const c_info = index >= 0 && index < m_textureInfoCollection.GetCount() ? &m_textureInfoCollection[index] : nullptr;
    return c_info;
}

#endif // UI_PAGE_DECK_ENABLED
