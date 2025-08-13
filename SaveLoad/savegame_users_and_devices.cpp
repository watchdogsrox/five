
#if __XENON
	#include "system/xtl.h"
#endif


// Rage headers
#include "file/savegame.h"

// Game headers
#include "Network/NetworkInterface.h"
#include "SaveLoad/GenericGameStorage.h"
#include "SaveLoad/savegame_users_and_devices.h"

#if __WIN32PC || RSG_DURANGO
#include "rline/rlpc.h"
PARAM(showProfileBypass, "[save] Shows Asserts whenever the profile/social club is bypassed.");
#endif // __WIN32PC || RSG_DURANGO

SAVEGAME_OPTIMISATIONS()

#if defined(MASTER_NO_SCUI)
namespace rage
{
	NOSTRIP_XPARAM(noSocialClub);
}
#elif !__FINAL
namespace rage
{
	XPARAM(noSocialClub);
}
#endif

#if __XENON
HANDLE		s_hNotification;
#endif

s32		CSavegameUsers::ms_UserID;
rlGamerId	CSavegameUsers::ms_UniqueIDOfGamerWhoStartedTheLoad;

bool		CSavegameDevices::ms_bDeviceIsValid;
bool		CSavegameDevices::ms_bDisplayDeviceHasBeenRemovedMessageNextTime;
bool		CSavegameDevices::ms_bAllowDeviceHasBeenRemovedMessageToBeDisplayed;


#if __XENON
void CSavegameDevices::InitNotificationListener()
{
   // Register a notification listener so that we can listen to device removal and player sign-out
    s_hNotification = XNotifyCreateListener( XNOTIFY_SYSTEM );
    if(s_hNotification == NULL || s_hNotification == INVALID_HANDLE_VALUE)
    { 
	    Assertf(0, "CSavegameDevices::InitNotificationListener - Unable to create the listener for device removal");
    }
}

void CSavegameDevices::ShutdownNotificationListener()
{
	CloseHandle( s_hNotification );
}
#endif	//	__XENON


void CSavegameUsers::Init(unsigned initMode)
{
    if(initMode == INIT_CORE)
    {
#if __WIN32PC || RSG_DURANGO
		// We do not yet support user accounts/live/social club etc. so we are faking it for now.
		Assertf(!PARAM_showProfileBypass.Get(), "User Profiles is not yet supported on PC. This WILL need update once Social Club is added! For now we will mimic a valid user id!");
		ms_UserID = 0;
#else
		//	This will be cleared if the player signs out but shouldn't clear it at the start of each session
		ms_UserID = -1;
#endif // __WIN32PC

		//	Need this for the rare case where a player signs out and signs in using a different profile
		//	between the end of GenericLoad and the start of Deserialize
		ms_UniqueIDOfGamerWhoStartedTheLoad.Clear();

    }
    else if(initMode == INIT_AFTER_MAP_LOADED)
    {
	    if (GetSignedInUser())
	    {
		    //	Kevin Baca added code that restarted the session if the player is signed into a profile and another player on a different Xbox signs in to the same profile.
		    //	It was possible for this to happen while the storage device was in use so I've added the code below as a safeguard.
		    //	The particular case in Bug 173335 shouldn't happen any more as Kevin has changed the code so that only network sessions are affected by Xbox Live disconnects.
			Assertf( (SAVEGAME.GetState(CSavegameUsers::GetUser()) == fiSaveGameState::IDLE) || (SAVEGAME.GetState(CSavegameUsers::GetUser()) == fiSaveGameState::HAD_ERROR), "CGenericGameStorage::Init - expected save game state to be IDLE or HAD_ERROR but it's %d", (s32) SAVEGAME.GetState(CSavegameUsers::GetUser()));
		    SAVEGAME.SetStateToIdle(CSavegameUsers::GetUser());
	    }
	}
}

bool CSavegameUsers::GetSignedInUser()
{
#if __WIN32PC
	if (g_rlPc.IsInitialized() &&
		g_rlPc.GetProfileManager()->IsSignedIn())
	{
		return true;
	}
	else
	{
#if  !__FINAL || defined(MASTER_NO_SCUI)
		return PARAM_noSocialClub.Get();
#else
		return false;
#endif
	}
#else
	bool ret = false;

	ms_UserID = NetworkInterface::GetLocalGamerIndex();

	if (ms_UserID >= 0)
	{
		ret = true;

#if __XENON
		if (XUserGetSigninState(ms_UserID) == eXUserSigninState_NotSignedIn)
		{//player has just signed out but MainGamerInfo hasn't been updated yet
			ms_UserID = -1;
			ret = false;
		}
#endif // __XENON
	}

	return ret;
#endif // __WIN32PC
}

void CSavegameUsers::ClearVariablesForNewlySignedInPlayer()
{
	if (GetSignedInUser())
	{
		CSavegameDevices::SetAllowDeviceHasBeenRemovedMessageToBeDisplayed(false);
		CSavegameDevices::SetDeviceIsInvalid(false);

		//	Kevin Baca added code that restarted the session if the player is signed into a profile and another player on a different Xbox signs in to the same profile.
		//	It was possible for this to happen while the storage device was in use so I've added the code below as a safeguard.
		//	The particular case in Bug 173335 shouldn't happen any more as Kevin has changed the code so that only network sessions are affected by Xbox Live disconnects.
		Assertf( (SAVEGAME.GetState(ms_UserID) == fiSaveGameState::IDLE) || (SAVEGAME.GetState(ms_UserID) == fiSaveGameState::HAD_ERROR), "CSavegameUsers::ClearVariablesForNewlySignedInPlayer - expected save game state to be IDLE or HAD_ERROR but it's %d", (s32) SAVEGAME.GetState(ms_UserID));
		SAVEGAME.SetStateToIdle(ms_UserID);
	}
	else
	{
		Assertf(0, "CSavegameUsers::ClearVariablesForNewlySignedInPlayer - player is not signed in");
	}
}

bool CSavegameUsers::HasPlayerJustSignedOut()
{
#if __XENON
	DWORD dwNotificationId = 0;
	ULONG_PTR ulParam;

	if (XNotifyGetNext( s_hNotification, XN_SYS_SIGNINCHANGED, &dwNotificationId, &ulParam )!=0)
	{
		if (ms_UserID != -1)
		{
			if (!GetSignedInUser())
			{
				return true;
			}
		}
	}

	return false;
#else
	return false;
#endif
}

void CSavegameDevices::Init(unsigned initMode)
{
    if(initMode == INIT_CORE)
    {
		//	Same as with CSavegameUsers::Init - the device should still be valid in the new session
		//	The only thing that would invalidate it is if the player removes the MU
		ms_bDeviceIsValid = false;
		ms_bDisplayDeviceHasBeenRemovedMessageNextTime = false;
		ms_bAllowDeviceHasBeenRemovedMessageToBeDisplayed = false;
	}
    else if(initMode == INIT_AFTER_MAP_LOADED)
    {

	}
}

bool CSavegameDevices::IsDeviceValid()
{
	if (SAVEGAME.IsCurrentDeviceValid(CSavegameUsers::GetUser()))
	{
		ms_bDeviceIsValid = true;
		ms_bAllowDeviceHasBeenRemovedMessageToBeDisplayed = true;
		return true;
	}

	SetDeviceIsInvalid(true);
	return false;
}

void CSavegameDevices::SetDeviceIsInvalid(bool bDisplayRemovedMessage)
{
	SAVEGAME.SetSelectedDeviceToAny(CSavegameUsers::GetUser());
	if (ms_bAllowDeviceHasBeenRemovedMessageToBeDisplayed)
	{
		ms_bDisplayDeviceHasBeenRemovedMessageNextTime = bDisplayRemovedMessage;
	}
	else
	{
		ms_bDisplayDeviceHasBeenRemovedMessageNextTime = false;
	}

	ms_bDeviceIsValid = false;
	CGenericGameStorage::SetFlagForAutosaveOverwriteMessage(true);
}

bool CSavegameDevices::HasSelectedDeviceJustBeenRemoved()
{
#if __XENON
	DWORD dwNotificationId = 0;
	ULONG_PTR ulParam;

	if (XNotifyGetNext( s_hNotification, XN_SYS_STORAGEDEVICESCHANGED, &dwNotificationId, &ulParam )!=0)
	{
		if (CSavegameUsers::GetSignedInUser())
		{
			if (ms_bDeviceIsValid)
			{	//	if device was valid in previous frame
				if (!IsDeviceValid())
				{	//	and not valid now
//					SAVEGAME.SetSelectedDeviceToAny(ms_UserID);	// this is done inside IsDeviceValid()
					ms_bDisplayDeviceHasBeenRemovedMessageNextTime = true;
					CGenericGameStorage::SetFlagForAutosaveOverwriteMessage(true);
					return true;
				}
			}
		}
	}

	return false;
#else
	return false;
#endif
}

