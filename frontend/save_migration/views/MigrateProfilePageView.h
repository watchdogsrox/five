/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : MigrateProfilePageView.h
// PURPOSE : Page view that shows the
//
// AUTHOR  : Charalampos.Koundourakis
// STARTED : August 2021
//
/////////////////////////////////////////////////////////////////////////////////
#ifndef MIGRATE_PROFILE_PAGE_VIEW_H
#define MIGRATE_PROFILE_PAGE_VIEW_H

#include "frontend/page_deck/uiPageConfig.h"
#if UI_PAGE_DECK_ENABLED

// rage
#include "atl/array.h"
#include "rline/savemigration/rlsavemigrationcommon.h"

// game
#include "frontend/page_deck/layout/PageGridSimple.h"
#include "frontend/page_deck/views/PopulatablePageView.h"
#include "frontend/career_builder/items/ParallaxImage.h"
#include "frontend/save_migration/items/MigrateProfileItem.h"

class CMigrateProfilePageView final : public CPopulatablePageView
{
	FWUI_DECLARE_DERIVED_TYPE(CMigrateProfilePageView, CPopulatablePageView);
public:
	CMigrateProfilePageView();
	virtual ~CMigrateProfilePageView();	

private: // declarations and variables
	CPageGridSimple         m_topLevelLayout;	
	CParallaxImage			m_parallaxImage;	
	CMigrateProfileItem     m_migrateProfile;
	uiPageLink				m_targetPageLink;
	int						m_selectedProfile;
	bool					m_hasUpdatedAccountDetails;
	rlSaveMigrationMPAccountsArray m_accounts;
	rage::atArray<SaveMigrationDefs::MultiplayerModeSaveGameEntry> m_saves;
	PAR_PARSABLE;
	NON_COPYABLE(CMigrateProfilePageView);

private: // methods
	void RecalculateGrids();	
	void OnFocusGainedDerived();
	void UpdateDerived(float const deltaMs) final;
	void FillProfileSlots();
	void OnFocusLostDerived() final;
	
	void PopulateDerived(IPageItemProvider& itemProvider) final;
	fwuiInput::eHandlerResult HandleBackoutAction(eFRONTEND_INPUT const inputAction, IPageMessageHandler & messageHandler);
	fwuiInput::eHandlerResult HandleMigrateAction(eFRONTEND_INPUT const inputAction);
	bool IsPopulatedDerived() const;
	void CleanupDerived() final;

	void GetContentArea(Vec2f_InOut pos, Vec2f_InOut size) const;
	
	void UpdateInputDerived() final;	
	fwuiInput::eHandlerResult SwapSelectedProfile(int targetProfile);
	void UpdateInstructionalButtonsDerived() final;

#if RSG_BANK
	void DebugRenderDerived() const final;
#endif 
};

#endif // UI_PAGE_DECK_ENABLED

#endif // MIGRATE_PROFILE_PAGE_VIEW_H
