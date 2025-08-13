#include "VerifyDownloadedSaveDecisionPage.h"

#if GEN9_LANDING_PAGE_ENABLED

#include "VerifyDownloadedSaveDecisionPage_parser.h"

// game
#include "frontend/ui_channel.h"

FWUI_DEFINE_TYPE(CVerifyDownloadedSaveDecisionPage, 0xFE25FB1);

bool CVerifyDownloadedSaveDecisionPage::GetDecisionResult() const
{
	// TODO_UI SAVE_MIGRATION: Verify downloaded save data here
	const bool c_isSaveValid = true;
	return c_isSaveValid;
}

#endif // GEN9_LANDING_PAGE_ENABLED
