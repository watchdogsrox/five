#include "HasAnyMigratableAccountsDecisionPage.h"

#if GEN9_LANDING_PAGE_ENABLED

#include "HasAnyMigratableAccountsDecisionPage_parser.h"

// game
#include "SaveMigration/SaveMigration.h"

FWUI_DEFINE_TYPE(CHasAnyMigratableAccountsDecisionPage, 0xBDF87A39);

bool CHasAnyMigratableAccountsDecisionPage::GetDecisionResult() const
{
	return CSaveMigration::GetMultiplayer().GetAccounts().GetCount() > 0;
}
#endif // GEN9_LANDING_PAGE_ENABLED
