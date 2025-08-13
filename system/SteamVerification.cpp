#if RSG_PC
#if __STEAM_BUILD

#include <comdef.h>
#include <WbemIdl.h>

#include "diag/seh.h"
#include "rline/steam/rlsteam.h"
#include "SteamVerification.h"
#include "system/timer.h"

#include "wolfssl/wolfcrypt/sha.h"

sysCriticalSectionToken CSteamVerification::sm_Cs;
bool CSteamVerification::sm_initialized = false;
bool CSteamVerification::sm_hasCompleted = false;
rlSteam::SteamFailureType  CSteamVerification::sm_FailureType = rlSteam::SteamFailureType::STEAM_FAILURE_NONE;
int CSteamVerification::sm_FailureCode = 0;

rage::netStatus CSteamVerification::sm_asyncStatus;

RAGE_DEFINE_SUBCHANNEL(net, steamverification, DIAG_SEVERITY_DEBUG3)
#undef __net_channel
#define __net_channel net_steamverification

// Not much to this implementation, but it's available in case people need to add stuff
void CSteamVerification::Init()
{
	gnetDebug1("Initializing Steam Verification");	
}

// Providing a means to reset, accordingly
void CSteamVerification::Reset()
{
	SYS_CS_SYNC(sm_Cs);
	sm_initialized = false;
	sm_hasCompleted = false;
	sm_FailureType = rlSteam::STEAM_FAILURE_NONE;
}


bool CSteamVerification::IsPending()
{
	return sm_asyncStatus.Pending();
}


void CSteamVerification::Update()
{
	// This is really the only usage for the sm_hasComplted.
	// I want to be able to detect if the process has finished,
	// and if it has, not grind on the steps, so this 
	// effectively becomes an empty function call.
	if(!sm_hasCompleted)
	{
#if !__FINAL
		if (!g_rlPc.IsInitialized())
			return;
#endif
		switch(sm_asyncStatus.GetStatus())
		{
			case netStatus::NET_STATUS_NONE:
				sm_initialized = true;
				if(!rlSteam::VerifyOwnership(0, &sm_asyncStatus, &sm_FailureType, &sm_FailureCode))
				{
					sm_hasCompleted = true;
					break;
				}
				break;
			case netStatus::NET_STATUS_PENDING:
				break;
			case netStatus::NET_STATUS_SUCCEEDED:
				sm_hasCompleted = true;
				break;
			case netStatus::NET_STATUS_FAILED:
			case netStatus::NET_STATUS_CANCELED:
				sm_hasCompleted = true;
				break;
			case netStatus::NET_STATUS_COUNT:
				break;
		}
		SYS_CS_SYNC(sm_Cs);
	}
}

#endif // __STEAM_BUILD
#endif // RSG_PC