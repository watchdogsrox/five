/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : UserEntitlementDefs.h
// PURPOSE : Common definitions for handling user entitlements
//
// AUTHOR  : stephen.phillips
// STARTED : February 2021
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef CORE_USER_ENTITLEMENT_DEFS
#define CORE_USER_ENTITLEMENT_DEFS

enum class UserEntitlementStatus : rage::u32
{
	Unknown,
	None,
	Entitled
};

struct UserEntitlements
{
	UserEntitlementStatus storyMode;
};

#endif // CORE_USER_ENTITLEMENT_DEFS
