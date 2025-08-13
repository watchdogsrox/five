/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : CarouselTabLayout.h
// PURPOSE : Collection of items that form a carousel to be used in a tabbed view
//
// AUTHOR  : Charalampos.Koundourakis
// STARTED : May 2021
//
/////////////////////////////////////////////////////////////////////////////////
#ifndef CAROUSEL_CART_TAB_LAYOUT_H
#define CAROUSEL_CART_TAB_LAYOUT_H

#include "frontend/page_deck/uiPageConfig.h"
#if UI_PAGE_DECK_ENABLED

// rage
#include "atl/array.h"

// framework
#include "fwui/Input/fwuiInputEnums.h"

// game
#include "frontend/input/uiInputEnums.h"
#include "frontend/page_deck/layout/PageLayoutItem.h"
#include "frontend/page_deck/views/Carousel.h"
#include "frontend/page_deck/views/Tooltip.h"
#include "frontend/page_deck/views/FeaturedImage.h"
#include "frontend/page_deck/views/FeaturedItem.h"

class CPageItemCategoryBase;
class CPageLayoutBase;
class CPageLayoutDecoratorItem;
class IPageMessageHandler;

// Inherits from layout item so we can easily cascade layout resolves
class CCarouselTabLayout final : public CPageLayoutItem
{
	typedef CPageLayoutItem superclass;
public:
	CCarouselTabLayout(CComplexObject& root, CPageItemCategoryBase& category);
	virtual ~CCarouselTabLayout();	
	void AddCategory(CPageItemCategoryBase& category) { GenerateItems(category); }

	void SetDefaultFocus();
	void InvalidateFocus();
	void RefreshFocusVisuals();
	void ClearFocusVisuals();
	bool UpdateInput();

	char const * GetFocusTooltip() const;
	void UpdateInstructionalButtons() const;

	CPageItemBase const * GetFocusedItem() const;
	void UpdateFeaturedItem();
	void ClearFeaturedItem();

	fwuiInput::eHandlerResult HandleDigitalNavigation(int const deltaX, int const deltaY);
	fwuiInput::eHandlerResult HandleCarouselDigitalNavigation(int const deltaX);
	fwuiInput::eHandlerResult HandleInputAction(eFRONTEND_INPUT const inputAction, IPageMessageHandler& messageHandler);

private: // declarations and variables	
	CPageGridSimple						m_topLevelLayout;
	CCarousel							m_carousel;
	CFeaturedImage						m_featuredImage;
	CFeaturedItem						m_featuredItem;

	NON_COPYABLE(CCarouselTabLayout);

private: // methods
	void RecalculateGrids();


	void GenerateItems(CPageItemCategoryBase& category);

	void ResolveLayoutDerived(Vec2f_In screenspacePosPx, Vec2f_In localPosPx, Vec2f_In sizePx) final;
#if RSG_BANK
	void DebugRenderDerived() const final;
#endif 
};

#endif // UI_PAGE_DECK_ENABLED

#endif // CAROUSEL_CART_TAB_LAYOUT_H
