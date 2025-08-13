/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : CardItemBase.cpp
// PURPOSE : Base for card items. Common interface and members for txd/texture info
//
// AUTHOR  : james.strain
// STARTED : May 2021
//
/////////////////////////////////////////////////////////////////////////////////
#include "CardItemBase.h"
#if UI_PAGE_DECK_ENABLED
#include "CardItemBase_parser.h"

// framework
#include "fwutil/xmacro.h"

// game
#include "frontend/page_deck/items/uiCloudSupport.h"
#include "frontend/Scaleform/ScaleFormComplexObjectMgr.h"
#include "frontend/Scaleform/ScaleFormMgr.h"
#include "frontend/ui_channel.h"

FWUI_DEFINE_TYPE( CCardItemBase, 0x33EC029F );

void CCardItemBase::UpdateVisualsDerived( CComplexObject& displayObject )
{
    if( !m_isTextureSet )
    {
        bool const c_isTextureAvailable = IsTextureAvailable();
        if( c_isTextureAvailable )
        {
            SetupCardVisuals( displayObject );
        }
    }
}

void CCardItemBase::OnFocusGainedDerived(CComplexObject& displayObject)
{
    CScaleformMgr::BeginMethodOnComplexObject(displayObject.GetId(), SF_BASE_CLASS_GENERIC, "SET_STATE");
    CScaleformMgr::AddParamInt(uiPageDeck::STATE_FOCUSED);
    CScaleformMgr::EndMethod();
}

void CCardItemBase::OnRevealDerived( CComplexObject& displayObject, int const sequenceIndex )
{
    CScaleformMgr::BeginMethodOnComplexObject( displayObject.GetId(), SF_BASE_CLASS_GENERIC, "ANIMATE_IN" );
    CScaleformMgr::AddParamInt( sequenceIndex );
    CScaleformMgr::EndMethod();
}

void CCardItemBase::OnFocusLostDerived( CComplexObject& displayObject )
{
    CScaleformMgr::BeginMethodOnComplexObject( displayObject.GetId(), SF_BASE_CLASS_GENERIC, "SET_STATE" );
    CScaleformMgr::AddParamInt( uiPageDeck::STATE_UNFOCUSED );
    CScaleformMgr::EndMethod();
}

void CCardItemBase::OnHoverGainedDerived( CComplexObject& displayObject )
{
    CScaleformMgr::BeginMethodOnComplexObject( displayObject.GetId(), SF_BASE_CLASS_GENERIC, "SET_STATE" );
    CScaleformMgr::AddParamInt( uiPageDeck::STATE_HOVERED );
    CScaleformMgr::EndMethod();
}

void CCardItemBase::OnHoverLostDerived( CComplexObject& displayObject )
{
    CScaleformMgr::BeginMethodOnComplexObject( displayObject.GetId(), SF_BASE_CLASS_GENERIC, "SET_STATE" );
    CScaleformMgr::AddParamInt( uiPageDeck::STATE_UNFOCUSED );
    CScaleformMgr::EndMethod();
}

void CCardItemBase::OnSelectedDerived( CComplexObject& displayObject )
{
    // We check this here as due to the SimplePageView we can select objects that do not have Scaleform content
    // We can remove this as part of url:bugstar:
    if( displayObject.IsActive() )
    {
        CScaleformMgr::BeginMethodOnComplexObject( displayObject.GetId(), SF_BASE_CLASS_GENERIC, "SET_STATE" );
        CScaleformMgr::AddParamInt( uiPageDeck::STATE_FOCUSED );
        CScaleformMgr::EndMethod();
    }
}

void CCardItemBase::SetDisplayObjectContentDerived( CComplexObject& displayObject )
{
    SetupCardVisuals( displayObject );
}

void CCardItemBase::SetEnabledVisualState(CComplexObject& displayObject)
{
    bool const c_isEnabled = IsEnabled();
    CScaleformMgr::BeginMethodOnComplexObject( displayObject.GetId(), SF_BASE_CLASS_GENERIC, "SET_ENABLED" );
    CScaleformMgr::AddParamBool( c_isEnabled );
    CScaleformMgr::EndMethod();
}

void CCardItemBase::SetupCardVisuals(CComplexObject& displayObject)
{
    char const * const c_title = GetItemTitleText();
    char const * const c_description = GetDescriptionText();
    char const * const c_txdName = GetTxd();
    char const * const c_textureName = GetTexture();

    CScaleformMgr::BeginMethodOnComplexObject(displayObject.GetId(), SF_BASE_CLASS_GENERIC, "SET_GRID_ITEM_DETAILS");
    CScaleformMgr::AddParamString(c_title, false);
    CScaleformMgr::AddParamString(c_description, false);

    if( uiCloudSupport::IsTextureLoaded( c_textureName, c_txdName ) )
    {
        CScaleformMgr::AddParamString(c_txdName, false);
        CScaleformMgr::AddParamString(c_textureName, false);
        m_isTextureSet = true;
    }
    else
    {
        CScaleformMgr::AddParamString("", false);
        CScaleformMgr::AddParamString("", false);
        m_isTextureSet = false;
    }
    CScaleformMgr::EndMethod();

    IItemStickerInterface const * const c_stickerInterface = GetStickerInterface();
    if( c_stickerInterface )
    {
        IItemStickerInterface::StickerType const c_stickerType = c_stickerInterface->GetItemStickerType();
        char const * const c_text = c_stickerInterface->GetStickerText();
        SetStickerInternal( displayObject, c_stickerType, c_text );        
    }

    SetEnabledVisualState( displayObject );
}

void CCardItemBase::ForEachFeatureTextureDerived(PerTextureLambda action)
{
	action(0, m_featuredTxd, m_featuredTexture);
}

void CCardItemBase::SetStickerInternal( CComplexObject& displayObject, IItemStickerInterface::StickerType const stickerType, char const * const text )
{
    if (displayObject.IsActive())
    {
        COMPLEX_OBJECT_ID const c_objId = displayObject.GetId();
        if( stickerType == IItemStickerInterface::StickerType::None )
        {
            CScaleformMgr::BeginMethodOnComplexObject( c_objId, SF_BASE_CLASS_GENERIC, "REMOVE_STICKER");
            CScaleformMgr::EndMethod();
        }
        else
        {
            CScaleformMgr::BeginMethodOnComplexObject( c_objId, SF_BASE_CLASS_GENERIC, "ADD_STICKER");
            CScaleformMgr::AddParamInt(static_cast<int>(stickerType));
            CScaleformMgr::AddParamString(text);
            CScaleformMgr::EndMethod();
        }
    }
}

#endif // UI_PAGE_DECK_ENABLED
