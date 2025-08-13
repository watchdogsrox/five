/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : UserEntitlementService.h
// PURPOSE : Provides access to platform entitlement information
//
// AUTHOR  : stephen.phillips
// STARTED : February 2021
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef CORE_USER_ENTITLEMENT_SERVICE_H
#define CORE_USER_ENTITLEMENT_SERVICE_H

#include "Core/UserEntitlementDefs.h"
#include "atl/singleton.h"

class IUserEntitlementDataProvider;

namespace rage
{
	class netStatus;
}

class CUserEntitlementService
{
public:
	~CUserEntitlementService();

	//PURPOSE: Check if it is necessary to request updated entitlements,
	//         e.g. the player has signed in since we last queried entitlements.
	//RETURN: Do we have up to date entitlements to retrieve with TryGetEntitlements.
	//        If not, they should be refreshed using RequestEntitlements.
	bool AreEntitlementsUpToDate() const;

	//PURPOSE: Create a request for the latest user entitlements.
	//RETURN: Was the request successfully created or are entitlements already up to date.
	bool RequestEntitlements();

	//PURPOSE: Attempt to populate entitlements data or retrieve it if not currently available.
	//PARAMS: out_entitlements - entitlements data to be populated when available.
	//RETURN: Was the entitlements data successfully populated.
	bool TryGetEntitlements(const UserEntitlements*& out_entitlements) const;

protected:
	CUserEntitlementService();

private:
	IUserEntitlementDataProvider* mp_dataProvider;
	const UserEntitlements* mp_latestEntitlements;
	const rage::netStatus* mp_latestRequestStatus;
};

typedef atSingleton<CUserEntitlementService> SUserEntitlementService;

// wrapper class needed to interface with game skeleton code
class CUserEntitlementServiceWrapper
{
public:
	static void Init(unsigned initMode);
	static void Shutdown(unsigned shutdownMode);
};

#endif // CORE_USER_ENTITLEMENT_SERVICE_H
