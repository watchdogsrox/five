/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    scene/playerswitch/PlayerSwitchEstablishingShot.h
// PURPOSE : manage loading and lookup of establishing shot data, which can occur
//			 just before outro of long switches
// AUTHOR :  Ian Kiigan
// CREATED : 14/11/12
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef _SCENE_PLAYERSWITCH_PLAYERSWITCHESTABLISHINGSHOT_H_
#define _SCENE_PLAYERSWITCH_PLAYERSWITCHESTABLISHINGSHOT_H_

#include "scene/playerswitch/PlayerSwitchEstablishingShotMetadata.h"
#include "atl/hashstring.h"

#define PLAYERSWITCH_ESTABLISHINGSHOT_FILE ("common:/data/playerswitchestablishingshots")

class CPlayerSwitchEstablishingShot
{
public:
	static void Load();
	static CPlayerSwitchEstablishingShotMetadata* FindShotMetaData(const atHashString name);

private:
	static CPlayerSwitchEstablishingShotMetadataStore ms_store;

#if __BANK
	friend class CPlayerSwitchDbg;
#endif	//__BANK
};

#endif	//_SCENE_PLAYERSWITCH_PLAYERSWITCHESTABLISHINGSHOT_H_