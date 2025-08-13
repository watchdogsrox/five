#include "HasAnyLocalSingleplayerSavesDecisionPage.h"

#if GEN9_LANDING_PAGE_ENABLED

#include "HasAnyLocalSingleplayerSavesDecisionPage_parser.h"

// game
#include "frontend/save_migration/pages/SaveMigrationWaitingForProcessPages.h"
#include "frontend/ui_channel.h"

FWUI_DEFINE_TYPE(CHasAnyLocalSingleplayerSavesDecisionPage, 0x14F89A1E);

#if RSG_BANK
PARAM(disableSPMigration, "Enable SP migration flow if no local saves are available");
#endif

#if !__FINAL
PARAM(forceSPMigration, "Show SP migration flow even if the player has local saves");
#endif // !__FINAL

bool CHasAnyLocalSingleplayerSavesDecisionPage::GetDecisionResult() const
{
#if !__FINAL
	if (PARAM_forceSPMigration.Get())
	{
		uiWarningf("CHasAnyLocalSingleplayerSavesDecisionPage::GetDecisionResult - running with -forceSPMigration so return false");
		return false;
	}
#endif // !__FINAL

	// TODO_UI SAVE_MIGRATION: Lookup local saves
	const int c_localSaveGamesFound = CLoadingLocalSingleplayerSavesPage::GetNumberOfEnumeratedSaves();
	const bool c_hasAnySingleplayerSaves = c_localSaveGamesFound > 0;
	return c_hasAnySingleplayerSaves BANK_ONLY(|| PARAM_disableSPMigration.Get());
}

#endif // GEN9_LANDING_PAGE_ENABLED
