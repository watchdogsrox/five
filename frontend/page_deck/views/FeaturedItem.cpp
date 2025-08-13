/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : FeaturedItem.cpp
// PURPOSE : Quick wrapper for the common featured item interface
//
// AUTHOR  : james.strain
// STARTED : March 2021
//
/////////////////////////////////////////////////////////////////////////////////
#include "FeaturedItem.h"
#if UI_PAGE_DECK_ENABLED

// framework
#include "fwutil/xmacro.h"

// game
#include "frontend/page_deck/PageItemBase.h"
#include "frontend/page_deck/items/CardItem.h"
#include "frontend/page_deck/items/CloudCardItem.h"
#include "frontend/Scaleform/ScaleFormMgr.h"
#include "frontend/Scaleform/ScaleFormComplexObjectMgr.h"
#include "frontend/ui_channel.h"

void CFeaturedItem::SetUseRichText( bool useRichText )
{
	m_useRichText = useRichText;
}

void CFeaturedItem::SetShowCostText(bool showCostText)
{
	m_showCostText = showCostText;
}

void CFeaturedItem::SetDetails( CPageItemBase const * const pageItem )
{
    char const * title = "";
    char const * description = "";
    char const * tagTxd = "";
    char const * tagTexture = "";
	char const * cost = "";

	if (pageItem)
	{
		title = pageItem->GetPageTitleText();
		description = pageItem->GetDescriptionText();
		cost = pageItem->GetItemTitleText();

		IItemTextureInterface const * const c_textureInterface = pageItem->GetTextureInterface();
		tagTxd = c_textureInterface ? c_textureInterface->GetTagTxd() : "";
		tagTexture = c_textureInterface ? c_textureInterface->GetTagTexture() : "";

		IItemStatsInterface const * c_statsInterface = pageItem->GetStatsInterface();
		if (c_statsInterface != nullptr)
		{
			c_statsInterface->AssignStats(*this);
		}

		SetSticker(pageItem);
	}
	else
	{
		ClearStats();
    }

    SetDetailsLiteral( title, description, tagTxd, tagTexture, cost);
}

void CFeaturedItem::SetDetails( atHashString const title, atHashString const description, char const * const tagTxd, char const * const tagTexture, char const * const cost)
{
    char const * const c_titleLiteral = TheText.Get( title );
    char const * const c_descriptionLiteral = TheText.Get( description );
    SetDetailsLiteral( c_titleLiteral, c_descriptionLiteral, tagTxd, tagTexture, cost);
}

void CFeaturedItem::SetSticker(CPageItemBase const * const pageItem)
{
	IItemStickerInterface const * const c_stickerInterface = pageItem->GetStickerInterface();
	if (c_stickerInterface)
	{
		IItemStickerInterface::StickerType const c_stickerType = c_stickerInterface->GetPageStickerType();
		char const * const c_text = c_stickerInterface->GetStickerText();
		SetStickerInternal(c_stickerType, c_text);
	}
}

void CFeaturedItem::SetDetailsLiteral( char const * const title, char const * const description, char const * const tagTxd, char const * const tagTexture, char const * const cost)
{
    CComplexObject& displayObject = GetDisplayObject();
    if( displayObject.IsActive() )
    {
		const char* const c_methodName = m_useRichText ? "SET_FEATURE_ITEM_DETAILS_RICHTEXT" : "SET_FEATURE_ITEM_DETAILS";
        CScaleformMgr::BeginMethodOnComplexObject( displayObject.GetId(), SF_BASE_CLASS_GENERIC, c_methodName);
        CScaleformMgr::AddParamString( title, false);
        CScaleformMgr::AddParamString( description, false );
        CScaleformMgr::AddParamString( tagTxd, false );
        CScaleformMgr::AddParamString( tagTexture, false );
		if (m_showCostText)
		{
			CScaleformMgr::AddParamString(cost, false);
		}
        CScaleformMgr::EndMethod();
    }
}

void CFeaturedItem::SetStats(atHashString const stat0Label, int const stat0, atHashString const stat1Label, int const stat1, atHashString const stat2Label, int const stat2, atHashString const stat3Label, int const stat3, atHashString const stat4Label , int const stat4)
{
    char const * const c_stat0String = TheText.Get( stat0Label );
    char const * const c_stat1String = TheText.Get( stat1Label );
    char const * const c_stat2String = TheText.Get( stat2Label );
    char const * const c_stat3String = TheText.Get( stat3Label );
    char const * const c_stat4String = stat4Label.IsNotNull() ? TheText.Get( stat4Label ) : "";
    SetStatsLiteral( c_stat0String, stat0, c_stat1String, stat1, c_stat2String, stat2, c_stat3String, stat3, c_stat4String, stat4 );
}

void CFeaturedItem::SetStatsLiteral(char const * const stat0Label, int const stat0, char const * const stat1Label, int const stat1, char const * const stat2Label, int const stat2, char const * const stat3Label, int const stat3, char const * const stat4Label, int const stat4)
{
    CComplexObject& displayObject = GetDisplayObject();
    if( displayObject.IsActive() )
    {
        CScaleformMgr::BeginMethodOnComplexObject( displayObject.GetId(), SF_BASE_CLASS_GENERIC, "SET_FEATURE_ITEM_STATS" );
        CScaleformMgr::AddParamString( stat0Label, false );
        CScaleformMgr::AddParamString( stat1Label, false );
        CScaleformMgr::AddParamString( stat2Label, false );
        CScaleformMgr::AddParamString( stat3Label, false );
        CScaleformMgr::AddParamString( stat4Label, false );
        CScaleformMgr::AddParamInt( stat0 );
        CScaleformMgr::AddParamInt( stat1 );
        CScaleformMgr::AddParamInt( stat2 );
        CScaleformMgr::AddParamInt( stat3 );
        CScaleformMgr::AddParamInt( stat4 );
        CScaleformMgr::EndMethod();
    }
}

void CFeaturedItem::ClearStats()
{
    CComplexObject& displayObject = GetDisplayObject();
    if( displayObject.IsActive() )
    {
        CScaleformMgr::BeginMethodOnComplexObject( displayObject.GetId(), SF_BASE_CLASS_GENERIC, "CLEAR_STATS" );
        CScaleformMgr::EndMethod();
    }
}

void CFeaturedItem::SetStickerInternal(IItemStickerInterface::StickerType const stickerType, char const * const text)
{
	CComplexObject& displayObject = GetDisplayObject();
	if (displayObject.IsActive())
	{
		COMPLEX_OBJECT_ID const c_objId = displayObject.GetId();
		if (stickerType == IItemStickerInterface::StickerType::None)
		{
			CScaleformMgr::BeginMethodOnComplexObject(c_objId, SF_BASE_CLASS_GENERIC, "REMOVE_STICKER");
			CScaleformMgr::EndMethod();
		}
		else
		{
			CScaleformMgr::BeginMethodOnComplexObject(c_objId, SF_BASE_CLASS_GENERIC, "ADD_STICKER");
			CScaleformMgr::AddParamInt(static_cast<int>(stickerType));
			CScaleformMgr::AddParamString(text);
			CScaleformMgr::EndMethod();
		}
	}
}

#endif // UI_PAGE_DECK_ENABLED
