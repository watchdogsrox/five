/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : LandingPageArbiter.cpp
// PURPOSE : Intermediary class that abstracts platform specific logic
//           for landing pages (as it differs per platform)
//
// AUTHOR  : james.strain
// STARTED : October 2020
//
/////////////////////////////////////////////////////////////////////////////////
#include "LandingPageArbiter.h"

// rage
#include "system/param.h"

// game
#include "core/UserEntitlementService.h"
#include "frontend/landing_page/LandingPage.h"
#include "frontend/landing_page/LegacyLandingScreen.h"
#include "frontend/page_deck/items/uiCloudSupport.h"
#include "frontend/PauseMenu.h"
#include "network/NetworkInterface.h"
#include "network/SocialClubServices/SocialClubFeedTilesMgr.h"
#include "stats/StatsInterface.h"

#if UI_LANDING_PAGE_ENABLED

GEN9_UI_SIMULATION_ONLY(PARAM(gen9BootFlow, "[startup] Enable Gen9 Boot Flow"));
GEN9_UI_SIMULATION_ONLY(PARAM(simulateGen9BootFlow, "[game] Simulate how the boot flow behaves"));

#if !__FINAL
XPARAM(skipLandingPage);
#endif

NOSTRIP_PC_XPARAM(benchmark);

#if GEN9_LANDING_PAGE_ENABLED && !RSG_FINAL
rage::fwFlags<CLandingPageArbiter::BootflowFlagsRawType> CLandingPageArbiter::ms_simulatedBootFlowState;
bool CLandingPageArbiter::ms_simulatedBootFlowStateInitialized = false;
#endif // GEN9_LANDING_PAGE_ENABLED && !RSG_FINAL

bool CLandingPageArbiter::ShouldShowLandingPageOnBoot()
{
#if RSG_PC
    if(PARAM_benchmark.Get())
    {
        return false;
    }
#endif // RSG_PC

#if !__FINAL
	if (PARAM_skipLandingPage.Get())
	{
		return false;
	}
#endif

    bool const c_result = CLegacyLandingScreen::IsLandingPageEnabled()
		GEN9_LANDING_PAGE_ONLY(|| (Gen9LandingPageEnabled() && CLandingPage::ShouldShowOnBoot()))
		;

    return c_result;
}

bool CLandingPageArbiter::IsSkipLandingPageSettingEnabled()
{
    return CPauseMenu::GetMenuPreference(PREF_LANDING_PAGE) != 1;
}

bool CLandingPageArbiter::IsLandingPageActive()
{
    bool const c_result = CLegacyLandingScreen::IsLandingPageInitialized()
		GEN9_LANDING_PAGE_ONLY(|| (Gen9LandingPageEnabled() && CLandingPage::IsActive()))
		;

    return c_result;
}

bool CLandingPageArbiter::ShouldGameRunLandingPageUpdate()
{
    bool const c_result = IsLandingPageActive()
		GEN9_LANDING_PAGE_ONLY(&& CLandingPage::ShouldGameRunLandingPageUpdate())
		;

    return c_result;
}

bool CLandingPageArbiter::IsStoryModeAvailable()
{
    CUserEntitlementService const& c_entitlementService = SUserEntitlementService::GetInstance();
    bool const c_isEntitlementDataUpToDate = c_entitlementService.AreEntitlementsUpToDate();

    // We are hitting the core game flow FSM. If entitlement data is out of date but required by the caller then we should not have presented the option to the user.
    // If this is an automated flow (e.g. CSavegameAutoload) then it should not attempt to enter storymode prematurely.
    bool hasStoryModeEntitlement = false;
    if( c_isEntitlementDataUpToDate )
    {
        UserEntitlements const * entitlements = nullptr;
        if( c_entitlementService.TryGetEntitlements( entitlements ) )
        {
            hasStoryModeEntitlement = entitlements->storyMode == UserEntitlementStatus::Entitled;
        }
    }

    return hasStoryModeEntitlement;
}

bool CLandingPageArbiter::IsFTUEComplete()
{
    bool result = false;

#if GEN9_LANDING_PAGE_ENABLED && !RSG_FINAL
	result = GetSimulatedBootFlowArg( BootFlowState::CompletedFTUE );
#endif // GEN9_LANDING_PAGE_ENABLED && !RSG_FINAL

    return result;
}

bool CLandingPageArbiter::HasPlayerRecievedWindfall()
{
    bool result = false;

#if GEN9_LANDING_PAGE_ENABLED && !RSG_FINAL
	result = GetSimulatedBootFlowArg( BootFlowState::ReceivedWindfall );
#endif // GEN9_LANDING_PAGE_ENABLED && !RSG_FINAL

    return result;
}

bool CLandingPageArbiter::DoesPlayerHaveCharacterToTransfer()
{
    bool result = false;

#if GEN9_LANDING_PAGE_ENABLED && !RSG_FINAL
	result = GetSimulatedBootFlowArg( BootFlowState::TransferCharacter );
#endif // GEN9_LANDING_PAGE_ENABLED && !RSG_FINAL

    return result;
}

bool CLandingPageArbiter::IsOnlineTabAvailable()
{
    //For now let's do the more restrictive option. That should be closer to what
    //we need and highlight more issues but can be changed back to IsOnline
    //if it turns out to work better.
    return NetworkInterface::HasValidRosCredentials();
}

bool CLandingPageArbiter::DoesPlayerHaveMPCharacter()
{
    bool result = true;

#if GEN9_LANDING_PAGE_ENABLED && !RSG_FINAL
	result = !GetSimulatedBootFlowArg( BootFlowState::HasNoCharacters );
#endif // GEN9_LANDING_PAGE_ENABLED && !RSG_FINAL

    return result;
}

eLandingPageOnlineDataSource CLandingPageArbiter::GetBestDataSource()
{
#if GEN9_LANDING_PAGE_ENABLED
    const eNetworkAccessCode accessCode = NetworkInterface::CheckNetworkAccess(eNetworkAccessArea::AccessArea_Landing);

    if (accessCode != eNetworkAccessCode::Access_Granted)
    {
        return eLandingPageOnlineDataSource::NoMpAccess;
    }

    if (!DoesPlayerHaveMPCharacter())
    {
        return eLandingPageOnlineDataSource::NoMpCharacter;
    }

    CSocialClubFeedTilesRow* row = uiCloudSupport::GetSource(ScFeedUIArare::LANDING_FEED);
    const bool hasOnlineData = row != nullptr && row->GetMetaDownloadState() == TilesMetaDownloadState::MetaReady;

    if (hasOnlineData)
    {
        return eLandingPageOnlineDataSource::FromCloud;
    }
#endif

    return eLandingPageOnlineDataSource::FromDisk;
}

bool CLandingPageArbiter::IsDataSourceOk(const eLandingPageOnlineDataSource current, eLandingPageOnlineDataSource& expected)
{
    const eLandingPageOnlineDataSource best = GetBestDataSource();

    expected = best;
    // We could have cases where the current is not the best but it's still fine.
    // For now we try this simple solution.
    return current == best;
}

#if GEN9_LANDING_PAGE_ENABLED && !RSG_FINAL

bool CLandingPageArbiter::GetSimulatedBootFlowArg(BootFlowState bootFlowFlag)
{
	if (ms_simulatedBootFlowStateInitialized == false)
	{
		GenerateSimulatedBootFlowArguments();
		ms_simulatedBootFlowStateInitialized = true;
	}

	return ms_simulatedBootFlowState.IsFlagSet(BIT(bootFlowFlag));
}

void CLandingPageArbiter::GenerateSimulatedBootFlowArguments()
{
	ms_simulatedBootFlowState.ClearAllFlags();

	const int c_argBufferLength = 32 * BootFlowState_Count;
	const char* args[BootFlowState_Count];
	char buffer[c_argBufferLength];

	int argc = PARAM_simulateGen9BootFlow.GetArray(args, BootFlowState_Count, buffer, c_argBufferLength);
	for (int i = 0; i < argc; ++i)
	{
		if (stricmp(args[i], "HasNoCharacters") == 0)
		{
			ms_simulatedBootFlowState.SetFlag( BIT(BootFlowState::HasNoCharacters) );
		}
		else if (stricmp(args[i], "TransferCharacter") == 0)
		{
			ms_simulatedBootFlowState.SetFlag( BIT(BootFlowState::TransferCharacter) );
		}
		else if (stricmp(args[i], "ReceivedWindfall") == 0)
		{
			ms_simulatedBootFlowState.SetFlag( BIT(BootFlowState::ReceivedWindfall) );
		}
		else if (stricmp(args[i], "CompletedFTUE") == 0)
		{
			ms_simulatedBootFlowState.SetFlag( BIT(BootFlowState::CompletedFTUE) );
		}
    }
}

#endif // GEN9_LANDING_PAGE_ENABLED && !RSG_FINAL

#else // UI_LANDING_PAGE_ENABLED

bool CLandingPageArbiter::ShouldShowLandingPageOnBoot()
{
    return false;
}

bool CLandingPageArbiter::IsSkipLandingPageSettingEnabled()
{
    return false;
}

bool CLandingPageArbiter::IsLandingPageActive()
{
    return false;
}

bool CLandingPageArbiter::IsStoryModeAvailable()
{
    return true;
}

bool CLandingPageArbiter::IsFTUEComplete()
{
    return true;
}

bool CLandingPageArbiter::HasPlayerRecievedWindfall()
{
    return true;
}

bool CLandingPageArbiter::DoesPlayerHaveCharacterToTransfer()
{
    return false;
}

eLandingPageOnlineDataSource CLandingPageArbiter::GetBestDataSource()
{
    return eLandingPageOnlineDataSource::FromDisk;
}

bool CLandingPageArbiter::IsDataSourceOk(const eLandingPageOnlineDataSource current, eLandingPageOnlineDataSource& expected)
{
    expected = current;
    return true;
}

#endif // UI_LANDING_PAGE_ENABLED
