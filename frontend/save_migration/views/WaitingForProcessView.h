/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : WaitingForProcessView.h
// PURPOSE : Page view that renders a loading spinner.
//
// AUTHOR  : stephen.phillips
// STARTED : August 2021
//
/////////////////////////////////////////////////////////////////////////////////
#ifndef WAITING_FOR_PROCESS_VIEW
#define WAITING_FOR_PROCESS_VIEW

#include "frontend/landing_page/LandingPageConfig.h"

#if GEN9_LANDING_PAGE_ENABLED

// game
#include "frontend/page_deck/layout/PageGridSimple.h"
#include "frontend/page_deck/views/PopulatablePageView.h"

class CWaitingForProcessView final : public CPopulatablePageView
{
	FWUI_DECLARE_DERIVED_TYPE(CWaitingForProcessView, CPopulatablePageView);
public:
	CWaitingForProcessView();
	virtual ~CWaitingForProcessView();

private: // declarations and variables
	CPageGridSimple m_layout;
	bool m_isPopulated;

	PAR_PARSABLE;
	NON_COPYABLE(CWaitingForProcessView);

private: // methods
	void OnFocusGainedDerived() final;
    void OnFocusLostDerived() final;
	void PopulateDerived(IPageItemProvider& itemProvider) final;
	bool IsPopulatedDerived() const final;
	void CleanupDerived() final;
};

#endif // GEN9_LANDING_PAGE_ENABLED

#endif // WAITING_FOR_PROCESS_VIEW
