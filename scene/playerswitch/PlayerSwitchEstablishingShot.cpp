/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    scene/playerswitch/PlayerSwitchEstablishingShot.h
// PURPOSE : manage loading and lookup of establishing shot data, which can occur
//			 just before outro of long switches
// AUTHOR :  Ian Kiigan
// CREATED : 14/11/12
//
/////////////////////////////////////////////////////////////////////////////////

#include "scene/playerswitch/PlayerSwitchEstablishingShot.h"
#include "scene/playerswitch/PlayerSwitchEstablishingShotMetadata.h"
#include "parser/manager.h"
#include "parser/macros.h"

CPlayerSwitchEstablishingShotMetadataStore CPlayerSwitchEstablishingShot::ms_store;

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	Load
// PURPOSE:		load the meta data list for all establishing shots
//////////////////////////////////////////////////////////////////////////
void CPlayerSwitchEstablishingShot::Load()
{
	PARSER.LoadObject(PLAYERSWITCH_ESTABLISHINGSHOT_FILE, "meta", ms_store);
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	FindShotMetaData
// PURPOSE:		look up meta data for the specified establishing shot name
//////////////////////////////////////////////////////////////////////////
CPlayerSwitchEstablishingShotMetadata* CPlayerSwitchEstablishingShot::FindShotMetaData(const atHashString name)
{
	// TODO: if there are a lot of shots, it may be worth paying the memory costs of an atMap for lookup
	for (s32 i=0; i<ms_store.m_ShotList.GetCount(); i++)
	{
		CPlayerSwitchEstablishingShotMetadata& shotData = ms_store.m_ShotList[i];
		if (shotData.m_Name == name)
		{
			return &shotData;
		}
	}
	return NULL;;
}