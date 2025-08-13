/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : ReviewCharacterPageView.cpp
// PURPOSE : The view that manages the review character pages. They consist of tthe review character item, a tooltip and the buttons
//
// AUTHOR  : Charalampos.Koundourakis
// STARTED : August 2021
//
/////////////////////////////////////////////////////////////////////////////////
#ifndef REVIEW_CHARACTER_PAGE_VIEW_H
#define REVIEW_CHARACTER_PAGE_VIEW_H

#include "frontend/page_deck/uiPageConfig.h"
#if UI_PAGE_DECK_ENABLED

// rage
#include "atl/array.h"

// game
#include "frontend/page_deck/layout/PageGridSimple.h"
#include "frontend/page_deck/views/SingleLayoutView.h"
#include "frontend/career_builder/items/ParallaxImage.h"
#include "frontend/save_migration/items/ReviewCharacterItem.h"

class CReviewCharacterPageView final : public CPopulatablePageView
{
	FWUI_DECLARE_DERIVED_TYPE(CReviewCharacterPageView, CPopulatablePageView);
public:
	CReviewCharacterPageView();
	virtual ~CReviewCharacterPageView();

private: // declarations and variables
	CPageGridSimple         m_topLevelLayout;
	CParallaxImage			m_parallaxImage;
	CReviewCharacterItem    m_reviewCharacter;
	uiPageLink				m_targetPageLink;
	int						m_selectedCharacter;
	bool					m_hasUpdatedAccountDetails;
	atArray<SaveMigrationDefs::MultiplayerCharacterEntry> m_characters;

	PAR_PARSABLE;
	NON_COPYABLE(CReviewCharacterPageView);

private: // methods
	void RecalculateGrids();
	void OnFocusGainedDerived();
	void UpdateDerived(float const deltaMs) final;

	void FillCharacterSlots(SaveMigrationDefs::MultiplayerModeSaveGameEntry profileData);
	void FillEmptyCharacterSlot(int profileSLot);

	void OnFocusLostDerived() final;

	void PopulateDerived(IPageItemProvider& itemProvider) final;
	bool IsPopulatedDerived() const;
	void CleanupDerived() final;

	void GetContentArea(Vec2f_InOut pos, Vec2f_InOut size) const;

	void UpdateInputDerived() final;
	void SetSelectedCharacter(int targetCharacter);
	void UpdateInstructionalButtonsDerived() final;
	fwuiInput::eHandlerResult HandleCharacterSlotSelectedAction(eFRONTEND_INPUT const inputAction, IPageMessageHandler & messageHandler);

#if RSG_BANK
	void DebugRenderDerived() const final;
#endif 
};

#endif // UI_PAGE_DECK_ENABLED

#endif // REVIEW_CHARACTER_PAGE_VIEW_H

