
// Rage headers
#include "script/wrapper.h"
// Game headers
#include "Core/game.h"
#include "frontend/landing_page/LandingPage.h"
#include "frontend/landing_page/LandingPageArbiter.h"
#include "Stats/StatsInterface.h"

namespace landingpage_commands
{
	bool CommandIsLandingPageActive()
	{
#if GEN9_LANDING_PAGE_ENABLED
		return CLandingPageArbiter::IsLandingPageActive();
#else // GEN9_LANDING_PAGE_ENABLED
		return false;
#endif // GEN9_LANDING_PAGE_ENABLED
	}

	void CommandSetShouldLaunchLandingPage(int entrypointIdValue)
	{
#if GEN9_LANDING_PAGE_ENABLED
		if (SCRIPT_VERIFY(!CLandingPage::IsActive(), "Attempting to launch Landing Page when already active"))
		{
			scriptDebugf1("%s :: SET_SHOULD_LAUNCH_LANDING_PAGE - eEntryPoint='%s'.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), LandingPageConfigEntryPointToName(entrypointIdValue));
			LandingPageConfig::eEntryPoint entrypointId = (LandingPageConfig::eEntryPoint)entrypointIdValue;
			CLandingPage::SetShouldLaunch( entrypointId );

            if( CPauseMenu::IsActive() && !CPauseMenu::IsClosingDown() )
            {
				scriptAssertf( false, "%s :: SET_SHOULD_LAUNCH_LANDING_PAGE - Pause menu was left active when requesting landing page, so we are forcing it closed. This is safe to skip, but not ideal.", CTheScripts::GetCurrentScriptNameAndProgramCounter() );
				CPauseMenu::Close();
            }
		}
#else
		(void)entrypointIdValue;
#endif // GEN9_LANDING_PAGE_ENABLED
	}

	void CommandSetShouldDismissLandingPage()
	{
#if GEN9_LANDING_PAGE_ENABLED
		if (SCRIPT_VERIFY(CLandingPage::IsActive(), "Attempting to dismiss Landing Page when it is not active"))
		{
			scriptDebugf1("%s :: SET_SHOULD_DISMISS_LANDING_PAGE.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
			CLandingPage::SetShouldDismiss();
		}
#endif // GEN9_LANDING_PAGE_ENABLED
	}

	int CommandGetLandingPageSelectedCharacterSlot()
	{
#if GEN9_LANDING_PAGE_ENABLED
		return StatsInterface::GetLandingPageCharacterSlotSelection();
#else
		return -1;
#endif // GEN9_LANDING_PAGE_ENABLED
	}

	void SetupScriptCommands()
	{
#if GEN9_LANDING_PAGE_ENABLED
		SCR_REGISTER_UNUSED(IS_LANDING_PAGE_ACTIVE, 0xdcc443647e72d3a9, CommandIsLandingPageActive);
		SCR_REGISTER_UNUSED(SET_SHOULD_LAUNCH_LANDING_PAGE, 0x84170bb1a4049cce, CommandSetShouldLaunchLandingPage);
		SCR_REGISTER_UNUSED(SET_SHOULD_DISMISS_LANDING_PAGE, 0x586d714afa1a4068, CommandSetShouldDismissLandingPage);
		SCR_REGISTER_UNUSED(GET_LANDING_PAGE_SELECTED_CHARACTER_SLOT, 0xfb9d4edbe2e05b3f, CommandGetLandingPageSelectedCharacterSlot);
#endif
	}
} // landingpage_commands
