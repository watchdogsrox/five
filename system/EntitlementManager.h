#ifndef ENTITLEMENT_MANAGER_H_
#define ENTITLEMENT_MANAGER_H_

//#if defined(MASTER_NO_ENTITLEMENTS)
//#define RSG_ENTITLEMENT_ENABLED 0
//#else
//#define RSG_ENTITLEMENT_ENABLED (1 && RSG_PC && !PRELOADED_SOCIAL_CLUB)
//#endif

#if RSG_ENTITLEMENT_ENABLED

#include "rline/ros/rlros.h"
#include "system/ipc.h"
#include "system/criticalsection.h"

#define SUPPORT_OLD_MAIN_SKU 0

#ifndef SUPPORT_OFFLINE_ENTITLEMENT 
#define SUPPORT_OFFLINE_ENTITLEMENT 1
#endif

class CEntitlementManager
{
	static bool sm_initialized;
	static bool sm_codeRedemptionPending;

	static bool sm_IsActivationUiShowing;
	static bool sm_bEntitlementWorkComplete;
	static bool sm_bFirstTimeShowSignIn;
	static u32 sm_codeAskStartTime;

	static u32 sm_EntitlementCheckStartTime;
	static u32 sm_EntitlementMsgStartTime;
	static u32 sm_CheckStartTime;
	static bool sm_EntitlementError;

	static u32 sm_PreorderBonus;

	static sysCriticalSectionToken sm_Cs;

	static const int SYSTEM_UI_WAIT_TIME_MS = 1000;

public:
	// Called once to set up
	static void Init(unsigned);

	// PURPOSE
	//	Resets the entitlement manager after a signout
	static void Reset();

	// Called each frame to update on main thread
	static void Update();

	// Check if an entitlement check is in progress
	static bool IsPending();

	// Begin a check
	static void StartCheck();

	// Called once to shut down
	static void Shutdown(unsigned);

	// Called to determine ownership of preorder bonus
	static bool IsEntitledToSpPreorderBonus();

	// Called during shutdown to see if we're able to
	static bool IsReadyToShutdown();

	// Called when a Ros Event is fired
	static void OnRosEvent(const rlRosEvent& evt);

private:
	 
	// Thread entry point
	static void ThreadEntry(void*);

	// Update functions
	static void UpdateOnlineState();
	static void UpdateWaitingForOnlineState();

	// Show activation UI
	static void ShowActivationUi();

	// Callback function from the activation interface
	static void ActivationCallback(const int contentId, const char* activationCode, const rgsc::IActivationV1::ActivationAction action);


	// Pre-Populate the Redemption Key
	static bool PrePopulateKey();
	static char sm_PrePopulateKey[MAX_PATH];

#if !__NO_OUTPUT
	static bool sm_bPassedLegals;
	static bool sm_bPassedInitialization;
	static bool sm_bPassedUIAcceptingCommands;
#endif
};

#endif //RSG_ENTITLEMENT_ENABLED

#endif //!ENTITLEMENT_MANAGER_H_
