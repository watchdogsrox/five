/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : WaitingForProcessPage.h
// PURPOSE : Base Page for waiting on an external process to complete
//
// AUTHOR  : stephen.phillips
// STARTED : August 2021
//
/////////////////////////////////////////////////////////////////////////////////
#ifndef WAITING_FOR_PROCESS_PAGE
#define WAITING_FOR_PROCESS_PAGE

#include "frontend/landing_page/LandingPageConfig.h"

#if GEN9_LANDING_PAGE_ENABLED

// game
#include "frontend/page_deck/PageBase.h"

class uiPageDeckMessageBase;

class CWaitingForProcessPage : public CCategorylessPageBase
{
	FWUI_DECLARE_DERIVED_TYPE(CWaitingForProcessPage, CCategorylessPageBase);
public:
	CWaitingForProcessPage();
	virtual ~CWaitingForProcessPage() {}

protected:
	virtual bool HasProcessCompletedDerived() const = 0;
	void UpdateDerived(float const deltaMs) override;

private: // declarations and variables
	uiPageDeckMessageBase* m_onCompleteAction;

	PAR_PARSABLE;
	NON_COPYABLE(CWaitingForProcessPage);

private: // methods
	bool HasProcessCompleted() const;

	bool IsTransitoryPageDerived() const final { return true; }
};

#endif // GEN9_LANDING_PAGE_ENABLED

#endif // WAITING_FOR_PROCESS_PAGE
