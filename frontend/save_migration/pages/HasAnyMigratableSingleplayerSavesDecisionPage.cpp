#include "HasAnyMigratableSingleplayerSavesDecisionPage.h"

#if GEN9_LANDING_PAGE_ENABLED

#include "HasAnyMigratableSingleplayerSavesDecisionPage_parser.h"

// game
#include "frontend/ui_channel.h"
#include "SaveMigration/SaveMigration.h"

FWUI_DEFINE_TYPE(CHasAnyMigratableSingleplayerSavesDecisionPage, 0xD45A9337);

bool CHasAnyMigratableSingleplayerSavesDecisionPage::GetDecisionResult() const
{
	const CSaveMigrationGetAvailableSingleplayerSaves::MigrationRecordCollection& c_migrationRecords = CSaveMigration::GetSingleplayer().GetAvailableSingleplayerSaves();
	const bool c_hasAnySingleplayerSaves = !c_migrationRecords.empty();
	return c_hasAnySingleplayerSaves;
}

#endif // GEN9_LANDING_PAGE_ENABLED
