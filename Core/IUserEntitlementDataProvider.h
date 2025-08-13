/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : IUserEntitlementDataProvider.h
// PURPOSE : Interface for per-platform user entitlement requests
//
// AUTHOR  : stephen.phillips
// STARTED : February 2021
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef CORE_I_USER_ENTITLEMENT_DATA_PROVIDER
#define CORE_I_USER_ENTITLEMENT_DATA_PROVIDER

#include "Core/UserEntitlementDefs.h"

namespace rage
{
	class netStatus;
}

class IUserEntitlementDataProvider
{
public:
	virtual ~IUserEntitlementDataProvider() { }

	//PURPOSE: Check if it is necessary to request updated entitlements,
	//         e.g. the player has signed in since we last queried entitlements.
	virtual bool AreEntitlementsUpToDate() const = 0;

	//PURPOSE: Create network request for entitlements if not already up to date.
	//PARAMS: out_entitlements - entitlements data to be populated when the data request is complete or up to date.
	//		  out_status - data request status to determine when the request is complete.
	//RETURN: Was the data request created successfully or already up to date.
	virtual bool RequestEntitlements(const UserEntitlements*& out_entitlements, const rage::netStatus*& out_status) = 0;
};

#endif // CORE_I_USER_ENTITLEMENT_DATA_PROVIDER
