/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : IUserEntitlementDataProvider.h
// PURPOSE : Interface for per-platform user entitlement requests
//
// AUTHOR  : stephen.phillips
// STARTED : February 2021
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef CORE_USER_ENTITLEMENT_DATA_PROVIDER_DUMMY
#define CORE_USER_ENTITLEMENT_DATA_PROVIDER_DUMMY

#include "Core/IUserEntitlementDataProvider.h"
#include "net/status.h"

class CUserEntitlementDataProvider_dummy final : public IUserEntitlementDataProvider
{
public:
	CUserEntitlementDataProvider_dummy();

	bool AreEntitlementsUpToDate() const final;
	bool RequestEntitlements(const UserEntitlements*& out_entitlements, const rage::netStatus*& out_status) final;

private:
	UserEntitlements m_dummyEntitlements;
	rage::netStatus m_dummyRequestStatus;
};

#endif // CORE_USER_ENTITLEMENT_DATA_PROVIDER_DUMMY
