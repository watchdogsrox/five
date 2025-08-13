/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : StoryModeSaveMigrationView.h
// PURPOSE : Page view for displaying the Story Mode save migration process.
//
// AUTHOR  : stephen.phillips
// STARTED : August 2021
//
/////////////////////////////////////////////////////////////////////////////////
#ifndef STORY_MODE_SAVE_MIGRATION_VIEW
#define STORY_MODE_SAVE_MIGRATION_VIEW

#include "frontend/landing_page/LandingPageConfig.h"

#if GEN9_LANDING_PAGE_ENABLED

// rage
#include "atl/array.h"
#include "atl/vector.h"

// framework
#include "fwutil/Gen9Settings.h"

// game
#include "frontend/page_deck/layout/PageGridSimple.h"
#include "frontend/page_deck/views/PopulatablePageView.h"
#include "frontend/career_builder/items/ParallaxImage.h"
#include "frontend/save_migration/items/StoryModeSaveMigrationSaveGameItem.h"

class CStoryModeSaveMigrationView final : public CPopulatablePageView
{
	FWUI_DECLARE_DERIVED_TYPE(CStoryModeSaveMigrationView, CPopulatablePageView);
public:
	CStoryModeSaveMigrationView();
	virtual ~CStoryModeSaveMigrationView() { }

private: // declarations and variables
	CPageGridSimple m_layout;
	CParallaxImage m_parallaxImage;
	CStoryModeSaveMigrationSaveGameItem m_saveGameItem;

	uiPageDeckMessageBase* m_onSelectMessage;

	PAR_PARSABLE;
	NON_COPYABLE(CStoryModeSaveMigrationView);

private: // methods
	void RecalculateGrids();

	void UpdateDerived(float const deltaMs) final;

	void PopulateDerived(IPageItemProvider& itemProvider) final;
	bool IsPopulatedDerived() const;
	void CleanupDerived() final;

	void UpdateInputDerived() final;
	void UpdateInstructionalButtonsDerived() final;
	fwuiInput::eHandlerResult HandleSaveSlotSelectedAction(IPageMessageHandler& messageHandler);

	void FillSaveSlots();

#if RSG_BANK
	void DebugRenderDerived() const final;
#endif 
};

#endif // GEN9_LANDING_PAGE_ENABLED

#endif // STORY_MODE_SAVE_MIGRATION_VIEW
