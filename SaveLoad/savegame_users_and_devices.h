

#ifndef SAVEGAME_USERS_AND_DEVICES_H
#define SAVEGAME_USERS_AND_DEVICES_H

// Rage headers
#include "rline/rlgamerinfo.h"


class CSavegameUsers
{
private:
//	Private data
	static s32		ms_UserID;
	static rlGamerId	ms_UniqueIDOfGamerWhoStartedTheLoad;

public:
//	Public functions
	static void ClearVariablesForNewlySignedInPlayer();

	static void Init(unsigned initMode);

	static bool GetSignedInUser();

	//	Returns the current UserID without checking if the user is signed in
	static int GetUser() { return ms_UserID; }

	static rlGamerId GetUniqueIDOfGamerWhoStartedTheLoad() { return ms_UniqueIDOfGamerWhoStartedTheLoad; }
	static void SetUniqueIDOfGamerWhoStartedTheLoad(rlGamerId GamerID) { ms_UniqueIDOfGamerWhoStartedTheLoad = GamerID; }
	static void ClearUniqueIDOfGamerWhoStartedTheLoad() { ms_UniqueIDOfGamerWhoStartedTheLoad.Clear(); }

	static bool HasPlayerJustSignedOut();
};

class CSavegameDevices
{
private:
//	Private data
	static bool			ms_bDeviceIsValid;
	static bool			ms_bDisplayDeviceHasBeenRemovedMessageNextTime;
	static bool			ms_bAllowDeviceHasBeenRemovedMessageToBeDisplayed;

public:
//	Public functions
	static void Init(unsigned initMode);

	static void SetDeviceIsInvalid(bool bDisplayRemovedMessage);

	static bool IsDeviceValid();

	static bool GetDisplayDeviceHasBeenRemovedMessageNextTime() { return ms_bDisplayDeviceHasBeenRemovedMessageNextTime; }
	static void SetDisplayDeviceHasBeenRemovedMessageNextTime(bool bDisplayMessage) { ms_bDisplayDeviceHasBeenRemovedMessageNextTime = bDisplayMessage; }

	static void SetAllowDeviceHasBeenRemovedMessageToBeDisplayed(bool bAllowMessage) { ms_bAllowDeviceHasBeenRemovedMessageToBeDisplayed = bAllowMessage; }

	static bool HasSelectedDeviceJustBeenRemoved();

#if __XENON
	static void InitNotificationListener();

	static void ShutdownNotificationListener();
#endif
};


#endif	//	SAVEGAME_USERS_AND_DEVICES_H

