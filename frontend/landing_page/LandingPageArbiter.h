/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : LandingPageArbiter.h
// PURPOSE : Intermediary class that abstracts platform specific logic
//           for landing pages (as it differs per platform)
//
//           Primarily used for external systems checking landing logic
//           that is common between the two, but implementation differs
//
// AUTHOR  : james.strain
// STARTED : October 2020
//
/////////////////////////////////////////////////////////////////////////////////
#ifndef LANDING_PAGE_ARBITER_H
#define LANDING_PAGE_ARBITER_H

// framework
#include "fwutil/Flags.h"
#include "fwutil/Gen9Settings.h"

// What data we can display on the landing page
enum class eLandingPageOnlineDataSource
{
    None,
    NoMpAccess,
    NoMpCharacter,
    FromDisk,
    FromCloud
};

class CLandingPageArbiter
{
public:
    static bool ShouldShowLandingPageOnBoot();
    static bool IsSkipLandingPageSettingEnabled();
    static bool IsLandingPageActive();
    static bool ShouldGameRunLandingPageUpdate();

    static bool IsStoryModeAvailable();
    static bool IsFTUEComplete();
    static bool HasPlayerRecievedWindfall();
    static bool DoesPlayerHaveCharacterToTransfer();
    static bool IsOnlineTabAvailable();
    static bool DoesPlayerHaveMPCharacter();

    // Returns the data source that best matches the current network state
    static eLandingPageOnlineDataSource GetBestDataSource();

    // If false the current data source should be changed and the page refreshed
    static bool IsDataSourceOk(const eLandingPageOnlineDataSource current, eLandingPageOnlineDataSource& expected);

private: // declarations and variables
#if GEN9_LANDING_PAGE_ENABLED && !RSG_FINAL
    // Boot flow simulation exists here as when we have proper UI hooks for this state, they will live here.
    // So this conveniently allows us to add the hook functions and have them be populated later
    typedef rage::u32 BootflowFlagsRawType;
    enum BootFlowState : BootflowFlagsRawType
    {
        HasNoCharacters,
        TransferCharacter,
        ReceivedWindfall,
        CompletedFTUE,
		BootFlowState_Count
    };

    static fwFlags<BootflowFlagsRawType> ms_simulatedBootFlowState;
	static bool ms_simulatedBootFlowStateInitialized;
#endif // GEN9_LANDING_PAGE_ENABLED && !RSG_FINAL

private: // methods
#if GEN9_LANDING_PAGE_ENABLED && !RSG_FINAL
	static bool GetSimulatedBootFlowArg(BootFlowState bootFlowFlag);
    static void GenerateSimulatedBootFlowArguments();
#endif // GEN9_LANDING_PAGE_ENABLED && !RSG_FINAL
};

#endif // LANDING_PAGE_ARBITER_H
