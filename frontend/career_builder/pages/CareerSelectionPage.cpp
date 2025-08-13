#include "CareerSelectionPage.h"
#include "CareerSelectionPage_parser.h"

// framework
#include "fwutil/xmacro.h"

// game
#include "frontend/career_builder/items/CareerBuilderCategory.h"
#include "frontend/ui_channel.h"
#include "Peds/CriminalCareer/CriminalCareerService.h"

#if UI_PAGE_DECK_ENABLED

FWUI_DEFINE_TYPE(CCareerSelectionPage, 0x56C13C8B);

CCareerSelectionPage::~CCareerSelectionPage()
{
	ClearCategories();
}

void CCareerSelectionPage::OnEnterStartDerived()
{
	PopulateCategories();
}

void CCareerSelectionPage::PopulateCategories()
{
	ClearCategories();

	// We only have one category - the list of careers
	CCareerBuilderCategory* p_careerSelectionCategory = rage_new CCareerBuilderCategory(CareerBuilderDefs::CategoryType::CareerSelection, m_careerShopPageId);
	m_categories.PushAndGrow(p_careerSelectionCategory);
}

void CCareerSelectionPage::ClearCategories()
{
	for (CareerBuilderDefs::CategoryIterator itr = m_categories.begin(); itr != m_categories.end(); ++itr)
	{
		delete *itr;
	}

	m_categories.ResetCount();
}

size_t CCareerSelectionPage::GetCategoryCount() const
{
	// Retrieve count from CriminalCareerService directly instead of relying on our virtualised list which may not be up to date.
	const CCriminalCareerService& criminalCareerService = SCriminalCareerService::GetInstance();
	const int c_careerCount = criminalCareerService.GetAllCareers().GetCount();
	return c_careerCount;
}

void CCareerSelectionPage::ForEachCategoryDerived(PerCategoryLambda action)
{
	for (CareerBuilderDefs::CategoryIterator itr = m_categories.begin(); itr != m_categories.end(); ++itr)
	{
		action(**itr);
	}
}

void CCareerSelectionPage::ForEachCategoryDerived(PerCategoryConstLambda action) const
{
	for (CareerBuilderDefs::ConstCategoryIterator itr = m_categories.begin(); itr != m_categories.end(); ++itr)
	{
		action(**itr);
	}
}

#endif // UI_PAGE_DECK_ENABLED
