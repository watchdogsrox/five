#include "UserEntitlementService.h"
#include "Core/IUserEntitlementDataProvider.h"
#include "fwsys/gameskeleton.h"
#include "net/status.h"
#include "system/param.h"

// TODO: Include correct data provider based on platform
#include "Core/UserEntitlementDataProvider_dummy.h"

PARAM(nostoryModeEntitlement, "Simulate story mode being unavailable without checking user entitlements");

CUserEntitlementService::CUserEntitlementService() :
	mp_latestEntitlements(nullptr),
	mp_latestRequestStatus(nullptr)
{
	// TODO: Resolve data provider based on platform
	mp_dataProvider = rage_new CUserEntitlementDataProvider_dummy();
}

CUserEntitlementService::~CUserEntitlementService()
{
	delete mp_dataProvider;
}

bool CUserEntitlementService::AreEntitlementsUpToDate() const
{
	const bool success = mp_dataProvider && mp_dataProvider->AreEntitlementsUpToDate();
	return success;
}

bool CUserEntitlementService::RequestEntitlements()
{
	const bool success = mp_dataProvider && mp_dataProvider->RequestEntitlements(mp_latestEntitlements, mp_latestRequestStatus);
	return success;
}

bool CUserEntitlementService::TryGetEntitlements(const UserEntitlements*& out_entitlements) const
{
	out_entitlements = mp_latestEntitlements;
	const bool success = mp_latestRequestStatus && mp_latestRequestStatus->Succeeded();
	return success;
}

void CUserEntitlementServiceWrapper::Init(unsigned int initMode)
{
	if (initMode == INIT_CORE)
	{
		SUserEntitlementService::Instantiate();
		// TODO: Requesting immediately here to populate from dummy data
		//       Should hook into platform sign in event
		SUserEntitlementService::GetInstance().RequestEntitlements();
	}
}

void CUserEntitlementServiceWrapper::Shutdown(unsigned int shutdownMode)
{
	if (shutdownMode == SHUTDOWN_CORE)
	{
		SUserEntitlementService::Destroy();
	}
}
