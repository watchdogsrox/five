#include "CareerBuilderPage.h"
#include "CareerBuilderPage_parser.h"

// framework
#include "fwutil/xmacro.h"

// game
#include "frontend/career_builder/items/CareerBuilderCategory.h"
#include "frontend/ui_channel.h"
#include "Peds/CriminalCareer/CriminalCareerService.h"

#if UI_PAGE_DECK_ENABLED

FWUI_DEFINE_TYPE(CCareerBuilderPage, 0x707F7F4D);

CCareerBuilderPage::~CCareerBuilderPage()
{
	ClearCategories();
}

void CCareerBuilderPage::OnEnterStartDerived()
{
	// Retrieve selected career
	const CCriminalCareerService& criminalCareerService = SCriminalCareerService::GetInstance();
	const CCriminalCareer* p_chosenCareer = criminalCareerService.TryGetChosenCareer();

	if (uiVerifyf(p_chosenCareer != nullptr, "Failed to retrieve chosen career"))
	{
		PopulateWithCareer(*p_chosenCareer);
	}
}

void CCareerBuilderPage::PopulateWithCareer(const CCriminalCareer& career)
{
	ClearCategories();

	const CCriminalCareerService& criminalCareerService = SCriminalCareerService::GetInstance();
	SetTitle(career.GetDisplayNameKey());

	// Iterate through Curated Content tabs
	const CCriminalCareer::ItemCategoryCollection& itemCategories = career.GetItemCategories();
	for (CCriminalCareer::ItemCategoryCollection::const_iterator itr = itemCategories.begin(); itr != itemCategories.end(); ++itr)
	{
		CCareerBuilderCategory* p_curatedContentCategory = rage_new CCareerBuilderCategory(CareerBuilderDefs::CategoryType::ShoppingCartTab, *itr);
		const CriminalCareerDefs::ItemCategoryDisplayData* cp_displayData = criminalCareerService.TryGetDisplayDataForCategory(*itr);
		if ( uiVerifyf(cp_displayData != nullptr, "Failed to retrieve display data for ItemCategory " HASHFMT, HASHOUT(*itr)) )
		{
			const CriminalCareerDefs::LocalizedTextKey& c_displayNameKey = cp_displayData->GetDisplayNameKey();
			p_curatedContentCategory->SetTitle(c_displayNameKey);
		}
		m_categories.PushAndGrow(p_curatedContentCategory);
	}

	// Add Summary tab on the end
	CCareerBuilderCategory* p_summaryCategory = rage_new CCareerBuilderCategory(CareerBuilderDefs::CategoryType::ShoppingCartSummary);
	p_summaryCategory->SetTitle("UI_CB_EXEC_SUMMARY");
	m_categories.PushAndGrow(p_summaryCategory);
}

void CCareerBuilderPage::ClearCategories()
{
	for (CareerBuilderDefs::CategoryIterator itr = m_categories.begin(); itr != m_categories.end(); ++itr)
	{
		delete *itr;
	}

	m_categories.ResetCount();
}

size_t CCareerBuilderPage::GetCategoryCount() const
{
	// Retrieve selected career
	const CCriminalCareerService& criminalCareerService = SCriminalCareerService::GetInstance();
	const CCriminalCareer* p_chosenCareer = criminalCareerService.TryGetChosenCareer();

	if (uiVerifyf(p_chosenCareer != nullptr, "Failed to retrieve chosen career"))
	{
		const int c_curatedContentTabCount = p_chosenCareer->GetItemCategories().GetCount();
		return c_curatedContentTabCount + 1; // Include summary tab in count
	}

	return 0u;
}

void CCareerBuilderPage::ForEachCategoryDerived(PerCategoryLambda action)
{
	for (CareerBuilderDefs::CategoryIterator itr = m_categories.begin(); itr != m_categories.end(); ++itr)
	{
		action(**itr);
	}
}

void CCareerBuilderPage::ForEachCategoryDerived(PerCategoryConstLambda action) const
{
	for (CareerBuilderDefs::ConstCategoryIterator itr = m_categories.begin(); itr != m_categories.end(); ++itr)
	{
		action(**itr);
	}
}

#endif // UI_PAGE_DECK_ENABLED
