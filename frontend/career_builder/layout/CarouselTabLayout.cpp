/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : CarouselTabLayout.cpp
// PURPOSE : Collection of items that form a carousel to be used in a tabbed view
//
// AUTHOR  : Charalampos.Koundourakis
// STARTED : May 2021
//
/////////////////////////////////////////////////////////////////////////////////
#include "CarouselTabLayout.h"

#include "frontend/page_deck/uiPageConfig.h"
#if UI_PAGE_DECK_ENABLED

// game
#include "Peds/CriminalCareer/CriminalCareerService.h"
#include "frontend/page_deck/PageItemCategoryBase.h"
#include "frontend/page_deck/layout/PageLayoutBase.h"
#include "frontend/page_deck/layout/PageLayoutDecoratorItem.h"
#include "frontend/VideoEditor/ui/InstructionalButtons.h"
#include "frontend/ui_channel.h"

CCarouselTabLayout::CCarouselTabLayout(CComplexObject& root, CPageItemCategoryBase& category)
	: superclass()
	, m_topLevelLayout(CPageGridSimple::GRID_3x7)
{		
	Initialize(root);
	GenerateItems(category);
}

CCarouselTabLayout::~CCarouselTabLayout()
{
	// We don't own the m_layout pointer so leave it alone
	m_topLevelLayout.UnregisterAll();
	m_carousel.Shutdown();
	m_featuredItem.Shutdown();
	m_featuredImage.Shutdown();	
}

void CCarouselTabLayout::RecalculateGrids()
{
	// Order is important here, as some of the sizing cascades
	// So if you adjust the order TEST IT!
	m_topLevelLayout.Reset();

	// Screen area is acceptable as our grids are all using star sizing
	Vec2f const c_screenExtents = uiPageConfig::GetScreenArea();
	m_topLevelLayout.RecalculateLayout(Vec2f(Vec2f::ZERO), c_screenExtents);
	m_topLevelLayout.NotifyOfLayoutUpdate();
}

void CCarouselTabLayout::ResolveLayoutDerived(Vec2f_In screenspacePosPx, Vec2f_In localPosPx, Vec2f_In sizePx)
{
	CComplexObject& displayObject = GetDisplayObject();
	displayObject.SetPositionInPixels(localPosPx);
	m_topLevelLayout.RecalculateLayout(screenspacePosPx, sizePx);
	m_topLevelLayout.NotifyOfLayoutUpdate();
}

void CCarouselTabLayout::GenerateItems(CPageItemCategoryBase& category)
{
	CComplexObject& pageRoot = GetDisplayObject();
	
	m_featuredImage.Initialize(pageRoot);
	CPageLayoutItemParams const c_featureImageParams(0, 1, 3, 1);
	m_topLevelLayout.RegisterItem(m_featuredImage, c_featureImageParams);
	
	m_featuredItem.Initialize(pageRoot);
	m_featuredItem.SetUseRichText(true);
	m_featuredItem.SetShowCostText(true);
	
	CPageLayoutItemParams const c_featureItemParams(1, 3, 1, 1);
	m_topLevelLayout.RegisterItem(m_featuredItem, c_featureItemParams);
	ClearFeaturedItem();
	
	m_carousel.Initialize(pageRoot);
	CPageLayoutItemParams const c_carouselParams(1, 4, 1, 1);
	m_topLevelLayout.RegisterItem(m_carousel, c_carouselParams);	

	m_carousel.AddCategory(category);
	m_carousel.SetDefaultFocus();
	UpdateFeaturedItem();
	RecalculateGrids();
}

#if RSG_BANK
void CCarouselTabLayout::DebugRenderDerived() const
{
	m_topLevelLayout.DebugRender();
}
#endif

void CCarouselTabLayout::SetDefaultFocus()
{
	m_carousel.SetDefaultFocus();
}

void CCarouselTabLayout::InvalidateFocus()
{
	m_carousel.InvalidateFocus();
}

void CCarouselTabLayout::RefreshFocusVisuals()
{
	m_carousel.RefreshFocusVisuals();
}

void CCarouselTabLayout::ClearFocusVisuals()
{
	m_carousel.ClearFocusVisuals();
}

fwuiInput::eHandlerResult CCarouselTabLayout::HandleDigitalNavigation(int const deltaX, int const deltaY)
{
	return m_topLevelLayout.HandleDigitalNavigation(deltaX, deltaY);
}

fwuiInput::eHandlerResult CCarouselTabLayout::HandleCarouselDigitalNavigation(int const deltaX)
{
	return m_carousel.HandleDigitalNavigation(deltaX);
}

bool CCarouselTabLayout::UpdateInput()
{
	return m_featuredImage.UpdateInput();
}

rage::fwuiInput::eHandlerResult CCarouselTabLayout::HandleInputAction(eFRONTEND_INPUT const inputAction, IPageMessageHandler& messageHandler)
{
	fwuiInput::eHandlerResult result(fwuiInput::ACTION_NOT_HANDLED);
	result = m_carousel.HandleInputAction(inputAction, messageHandler);

	// Update elements when input is handled
	if (result == fwuiInput::ACTION_HANDLED)
	{
		const CCriminalCareerService& careerService = SCriminalCareerService::GetInstance();
		int index = 0;
		auto action = [this, &careerService, &index](const CPageItemBase& item)
		{
			// Update sticker to state if item is in shopping cart
			IItemStickerInterface const * const c_stickerInterface = item.GetStickerInterface();
			if (c_stickerInterface)
			{
				char const * const c_text = c_stickerInterface->GetStickerText();
				IItemStickerInterface::StickerType const c_type = c_stickerInterface->GetItemStickerType();
				m_carousel.SetSticker(index, c_type, c_text);			
			}
			++index;
		};

		CPageItemBase const * const c_focusedItem = GetFocusedItem();
		m_featuredItem.SetSticker(c_focusedItem);
		m_carousel.ForEachItem(action);
	}

	return result;
}

char const * CCarouselTabLayout::GetFocusTooltip() const
{
	return m_carousel.GetFocusTooltip();
}

void CCarouselTabLayout::UpdateInstructionalButtons() const
{
	m_featuredImage.UpdateInstructionalButtons();

	CPageItemBase const* c_focusedItem = GetFocusedItem();
	if (c_focusedItem && c_focusedItem->GetIsClass<CCareerBuilderShoppingCartItem>())
	{
		const CCriminalCareerService& c_careerService = SCriminalCareerService::GetInstance();
		const CCareerBuilderShoppingCartItem& c_shoppingCartItem = static_cast<const CCareerBuilderShoppingCartItem&>(*c_focusedItem);
		const CriminalCareerDefs::ItemIdentifier& c_itemId = c_shoppingCartItem.GetItemIdentifier();
		const bool c_isAlwaysInShoppingCart = c_careerService.IsFlagSetOnItem(CriminalCareerDefs::AlwaysInShoppingCart, c_itemId);

		// Only enable toggling item if it does not have AlwaysInShoppingCart set
		if (!c_isAlwaysInShoppingCart)
		{
			const bool c_isInShoppingCart = c_careerService.IsItemInShoppingCart(c_shoppingCartItem.GetItemIdentifier());
			const atHashString c_label = c_isInShoppingCart ? ATSTRINGHASH("IB_REMOVE", 0xE69FF992) : ATSTRINGHASH("IB_ADD", 0xBC497101);
			m_carousel.UpdateInstructionalButtons(c_label);
		}
	}
}

CPageItemBase const* CCarouselTabLayout::GetFocusedItem() const
{	
	return m_carousel.GetFocusedItem();
}

void CCarouselTabLayout::UpdateFeaturedItem()
{
	ClearFeaturedItem();
	CPageItemBase const * const c_focusedItem = GetFocusedItem();
	m_featuredItem.SetDetails(c_focusedItem);
	m_featuredImage.Set(c_focusedItem);
}

void CCarouselTabLayout::ClearFeaturedItem()
{
	m_featuredItem.SetDetails(nullptr);
	m_featuredImage.Reset();
}

#endif // UI_PAGE_DECK_ENABLED
