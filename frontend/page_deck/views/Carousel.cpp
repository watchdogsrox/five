/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : Carousel.cpp
// PURPOSE : Quick wrapper for the common carousel interface
//
// AUTHOR  : james.strain
// STARTED : March 2021
//
/////////////////////////////////////////////////////////////////////////////////
#include "Carousel.h"
#if UI_PAGE_DECK_ENABLED

// framework
#include "fwutil/xmacro.h"

// game
#include "frontend/page_deck/items/CardItem.h"
#include "frontend/page_deck/items/CloudCardItem.h"
#include "frontend/page_deck/items/uiCloudSupport.h"
#include "frontend/page_deck/PageItemCategoryBase.h"
#include "frontend/Scaleform/ScaleFormMgr.h"
#include "frontend/Scaleform/ScaleFormComplexObjectMgr.h"
#include "frontend/VideoEditor/ui/InstructionalButtons.h"
#include "frontend/ui_channel.h"
#include "system/control_mapping.h"

void CCarousel::AddCategory( CPageItemCategoryBase& category, const eCarouselItemType type)
{
    auto itemFunc = [this, type]( CPageItemBase& item )
    {
        AddItem( item, type );
    };

    category.ForEachItem( itemFunc );
}

void CCarousel::ShutdownDerived()
{
    // We don't own the items, so just clear the array
    m_items.ResetCount();
    m_requiresTextureRefresh.ResetCount();
}

void CCarousel::SetDefaultFocus()
{
    m_focusedIndex = m_items.GetCount() > 0 ? 0 : INDEX_NONE;
    RefreshFocusVisuals();
}

void CCarousel::InvalidateFocus()
{
    m_focusedIndex = INDEX_NONE;
    ClearFocusVisuals();
}

void CCarousel::RefreshFocusVisuals()
{
    CComplexObject& displayObject = GetDisplayObject();
    if( displayObject.IsActive() && m_focusedIndex >= 0 )
    {
        CScaleformMgr::BeginMethodOnComplexObject( displayObject.GetId(), SF_BASE_CLASS_GENERIC, "SET_CAROUSEL_ITEM_STATE" );
        CScaleformMgr::AddParamInt( m_focusedIndex );
        CScaleformMgr::AddParamInt( uiPageDeck::STATE_FOCUSED );
        CScaleformMgr::AddParamBool( true ); // Enabled
        CScaleformMgr::EndMethod();
    }
}

void CCarousel::ClearFocusVisuals()
{
    CComplexObject& displayObject = GetDisplayObject();
    if( displayObject.IsActive() )
    {
        CScaleformMgr::BeginMethodOnComplexObject( displayObject.GetId(), SF_BASE_CLASS_GENERIC, "UNHIGHLIGHT_ALL_CAROUSEL_ITEMS" );
        CScaleformMgr::EndMethod();
    }
}

char const * CCarousel::GetFocusTooltip() const
{
    CPageItemBase const * pageItem = GetFocusedItem();
    char const * const c_tipText = pageItem ? pageItem->GetTooltipText() : nullptr;
    return c_tipText;
}

void CCarousel::ForEachItem(PerItemConstLambda action) const
{
	const int c_itemCount = m_items.GetCount();
	for (int i = 0; i < c_itemCount; ++i)
	{
		const CPageItemBase* entry = m_items[i];
		if (entry != nullptr)
		{
			action(*entry);
		}
	}
}

void CCarousel::SetSticker( int const index, IItemStickerInterface::StickerType const stickerType, char const * const text )
{
	if (uiVerifyf(index >= 0 && index < m_items.GetCount(), "Attempting to set a sticker on invalid carousel index %d", index))
	{
		SetStickerInternal(index, stickerType, text );
	}
}

CPageItemBase const* CCarousel::GetFocusedItem() const
{
    return GetItemByIndex( m_focusedIndex );
}

void CCarousel::UpdateVisuals()
{
    int const c_itemCount = GetItemCount();
    for( int index = 0; index < c_itemCount; ++index )
    {
        RefreshItem( index );
    }
}

void CCarousel::UpdateInstructionalButtons( atHashString const acceptLabelOverride ) const
{
    CPageItemBase const * const c_focusedItem = GetFocusedItem();
    if( c_focusedItem )
    {
        char const * const c_label = uiPageConfig::GetLabel( ATSTRINGHASH( "IB_SELECT", 0xD7ED7F0C ), acceptLabelOverride );
        CVideoEditorInstructionalButtons::AddButton( INPUT_FRONTEND_ACCEPT, c_label, true );
    }
}

rage::fwuiInput::eHandlerResult CCarousel::HandleDigitalNavigation( int const deltaX )
{
    fwuiInput::eHandlerResult result( fwuiInput::ACTION_NOT_HANDLED );

    if( m_items.GetCount() > 0 )
    {
        if( UpdateFocus( deltaX ) )
        {
            result = fwuiInput::ACTION_HANDLED;
        }
        else
        {
            result = fwuiInput::ACTION_IGNORED;
        }
    }

    return result;
}

rage::fwuiInput::eHandlerResult CCarousel::HandleInputAction( eFRONTEND_INPUT const inputAction, IPageMessageHandler& messageHandler )
{
    fwuiInput::eHandlerResult result( fwuiInput::ACTION_NOT_HANDLED );

    if( inputAction == FRONTEND_INPUT_ACCEPT )
    {
        CPageItemBase * const focusedItem = GetFocusedItemInternal();
        if( focusedItem )
        {
            focusedItem->OnSelected( GetDisplayObject(), messageHandler );
            result = fwuiInput::ACTION_HANDLED;
        }
    }

    return result;
}

void CCarousel::PlayRevealAnimation( int const UNUSED_PARAM(sequenceIndex) )
{
    CComplexObject& displayObject = GetDisplayObject();
    if( displayObject.IsActive() )
    {
        CScaleformMgr::BeginMethodOnComplexObject( displayObject.GetId(), SF_BASE_CLASS_GENERIC, "ANIMATE_IN" );
        CScaleformMgr::EndMethod();
    }
}

void CCarousel::AddItem( CPageItemBase& item, const eCarouselItemType type)
{
    int const c_itemIndex = m_items.GetCount();
    m_items.PushAndGrow( &item );

    bool requiresTextureRefresh = true;

    CComplexObject& displayObject = GetDisplayObject();
    if( displayObject.IsActive() )
    {
        IItemTextureInterface const * const c_textureInterface = item.GetTextureInterface();
        char const * txd = c_textureInterface ? c_textureInterface->GetTxd() : "";
        char const * texture = c_textureInterface ? c_textureInterface->GetTexture() : "";

        requiresTextureRefresh = !uiCloudSupport::IsTextureLoaded( texture, txd );
        if( requiresTextureRefresh )
        {
            // If the texture is not yet available, use the default variant
            txd = "";
            texture = "";
        }

        CScaleformMgr::BeginMethodOnComplexObject( displayObject.GetId(), SF_BASE_CLASS_GENERIC, "ADD_CAROUSEL_ITEM" );
        CScaleformMgr::AddParamString( item.GetItemTitleText(), true ); // price/label
        CScaleformMgr::AddParamString( txd, false );                    // mainImgTxd
        CScaleformMgr::AddParamString( texture, false );                // mainImgTexture
        CScaleformMgr::AddParamInt(type);                                   // itemType
        CScaleformMgr::AddParamBool(true);                              // isRichText
        CScaleformMgr::EndMethod();

        IItemStickerInterface const * const c_stickerInterface = item.GetStickerInterface();
        if( c_stickerInterface )
        {
            char const * const c_text = c_stickerInterface->GetStickerText();
            IItemStickerInterface::StickerType const c_type = c_stickerInterface->GetItemStickerType();
            SetSticker( c_itemIndex, c_type, c_text );
        }
    }

    m_requiresTextureRefresh.PushAndGrow( requiresTextureRefresh );
}

void CCarousel::RefreshItem( int const index )
{
    bool const c_requiresRefresh = m_requiresTextureRefresh[ index ];
    if( c_requiresRefresh )
    {
        CComplexObject& displayObject = GetDisplayObject();
        if( displayObject.IsActive() )
        {
            CPageItemBase const * const c_item = GetItemByIndex( index );
            IItemTextureInterface const * const c_textureInterface = c_item ? c_item->GetTextureInterface() : nullptr;
            if( c_textureInterface)
            {
                char const * const c_txd = c_textureInterface->GetTxd();
                char const * const c_texture =c_textureInterface->GetTexture();

                // We only refresh the card if the textures are now available
                if( uiCloudSupport::IsTextureLoaded( c_texture, c_txd ) )
                {
                    CScaleformMgr::BeginMethodOnComplexObject( displayObject.GetId(), SF_BASE_CLASS_GENERIC, "UPDATE_ITEM_TEXTURE" );
                    CScaleformMgr::AddParamInt( index );
                    CScaleformMgr::AddParamString( c_txd, false );
                    CScaleformMgr::AddParamString( c_texture, false );
                    CScaleformMgr::EndMethod();
                    m_requiresTextureRefresh[ index ] = false;
                }
            }

            RefreshEnabledState( index, displayObject );
        }
    }
}

void CCarousel::RefreshEnabledState( int const index )
{
    CComplexObject& displayObject = GetDisplayObject();
    if( displayObject.IsActive() )
    {
        RefreshEnabledState( index, displayObject );
    }
}

void CCarousel::RefreshEnabledState(int const index, CComplexObject& displayObject)
{
    CPageItemBase const * const c_item = GetItemByIndex( index );
    bool const c_isEnabled = c_item && c_item->IsEnabled();

    CScaleformMgr::BeginMethodOnComplexObject( displayObject.GetId(), SF_BASE_CLASS_GENERIC, "SET_CAROUSEL_ITEM_ENABLED" );
    CScaleformMgr::AddParamInt( index );
    CScaleformMgr::AddParamBool( c_isEnabled );
    CScaleformMgr::EndMethod();
}

CPageItemBase* CCarousel::GetFocusedItemInternal()
{
    return GetItemByIndex( m_focusedIndex );
}

CPageItemBase* CCarousel::GetItemByIndex( int const index )
{
    CPageItemBase* const result = index >= 0 && index < m_items.GetCount() ?
        m_items[index] : nullptr;

    return result;
}

CPageItemBase const* CCarousel::GetItemByIndex( int const index ) const
{
    CPageItemBase const * const c_result = index >= 0 && index < m_items.GetCount() ?
        m_items[index] : nullptr;

    return c_result;
}

bool CCarousel::IsItemEnabled( int const index ) const
{
    // We don't have item disabling requirements yet, but the AS interface supports it
    // Fill this in later as required
    UNUSED_VAR(index);
    return true;
}

bool CCarousel::UpdateFocus( int const delta )
{
    int const c_oldFocusIndex = m_focusedIndex;

    char const * invokeName = nullptr;
    if( delta > 0 )
    {
        invokeName = "NAV_RIGHT";
        ++m_focusedIndex;
    }
    else if( delta < 0 )
    {
        invokeName = "NAV_LEFT";
        --m_focusedIndex;
    }

    if( invokeName )
    {
        int const c_itemCount = GetItemCount();
        if( m_focusedIndex < 0 )
        {
            m_focusedIndex = 0;
        }
        else if( m_focusedIndex >= c_itemCount )
        {
            m_focusedIndex = c_itemCount - 1;
        }
        else
        {
            CComplexObject& displayObject = GetDisplayObject();
            CScaleformMgr::BeginMethodOnComplexObject( displayObject.GetId(), SF_BASE_CLASS_GENERIC, invokeName );
            CScaleformMgr::EndMethod();
        }
    }
    
    return c_oldFocusIndex != m_focusedIndex;
}

void CCarousel::SetStickerInternal( int const index, IItemStickerInterface::StickerType const stickerType, char const * const text  )
{
	CComplexObject& displayObject = GetDisplayObject();
	if (displayObject.IsActive() && index >= 0)
    {
        COMPLEX_OBJECT_ID const c_objId = displayObject.GetId();
		if(stickerType == IItemStickerInterface::StickerType::None )
		{
			CScaleformMgr::BeginMethodOnComplexObject(c_objId, SF_BASE_CLASS_GENERIC, "REMOVE_STICKER");
			CScaleformMgr::AddParamInt(index);
			CScaleformMgr::EndMethod();
		}
		else
		{
			CScaleformMgr::BeginMethodOnComplexObject(c_objId, SF_BASE_CLASS_GENERIC, "ADD_STICKER");
            CScaleformMgr::AddParamInt(index);
            CScaleformMgr::AddParamInt(static_cast<int>(stickerType));
			CScaleformMgr::AddParamString(text);
			CScaleformMgr::EndMethod();
		}
	}
}

#endif // UI_PAGE_DECK_ENABLED
