#include "UserEntitlementDataProvider_dummy.h"

XPARAM(nostoryModeEntitlement);

CUserEntitlementDataProvider_dummy::CUserEntitlementDataProvider_dummy()
{
	m_dummyRequestStatus.ForceSucceeded();

	m_dummyEntitlements.storyMode = PARAM_nostoryModeEntitlement.Get() ? UserEntitlementStatus::None : UserEntitlementStatus::Entitled;
}

bool CUserEntitlementDataProvider_dummy::AreEntitlementsUpToDate() const
{
	return true;
}

bool CUserEntitlementDataProvider_dummy::RequestEntitlements(const UserEntitlements*& out_entitlements, const rage::netStatus*& out_status)
{
	out_entitlements = &m_dummyEntitlements;
	out_status = &m_dummyRequestStatus;

	return true;
}
