#ifndef STEAM_VERIFICATION_H
#define STEAM_VERIFICATION_H

#include "system/SteamVerification.h"
#include "rline/steam/rlsteam.h"

#if RSG_PC
#if __STEAM_BUILD

#include "system/ipc.h"
#include "system/criticalsection.h"


class CSteamVerification
{
	static bool sm_initialized;

	static bool sm_hasCompleted;

	static sysCriticalSectionToken sm_Cs;

	static rage::netStatus sm_asyncStatus;
public:
	// Called once to set up
	static void Init();

	// Called again to re-check
	static void Reset();

	// Called each frame to update on main thread
	static void Update();

	// Check if an entitlement check is in progress
	static bool IsPending();

	// Check to determine if it's failed
	static bool IsFailed(){return sm_asyncStatus.Failed();}

	// Check to determine if it's initialized
	static bool IsInitialized(){return sm_initialized;}

	// Return FailureType
	static rlSteam::SteamFailureType GetFailureType(){return sm_FailureType;}

	// Return FailureCode
	static int GetFailureCode(){return sm_FailureCode;}
private:

	// Indicating the type of failure that's possible
	static rlSteam::SteamFailureType sm_FailureType;

	// Integer representing the error code
	static int sm_FailureCode;
};

#endif // __STEAM_BUILD
#endif // RSG_PC
#endif
