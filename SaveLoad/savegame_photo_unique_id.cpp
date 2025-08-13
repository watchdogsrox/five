
#include "SaveLoad/savegame_photo_unique_id.h"

// Rage headers
#include "system/timer.h"

// Framework headers
#include "fwsys/gameskeleton.h"

// Game headers
#include "SaveLoad/savegame_channel.h"
#include "SaveLoad/savegame_photo_local_list.h"


mthRandom CSavegamePhotoUniqueId::sm_Random;

void CSavegamePhotoUniqueId::Init(unsigned initMode)
{
	if(initMode == INIT_CORE)
	{
		sm_Random.SetFullSeed((u64)sysTimer::GetTicks());
	}
}

void CSavegamePhotoUniqueId::GenerateNewUniqueId()
{
	bool bFoundAUniqueValue = false;

	while (!bFoundAUniqueValue)
	{
		m_Value = sm_Random.GetInt();

		photoAssertf(CSavegamePhotoLocalList::GetListHasBeenCreated(), "CSavegamePhotoUniqueId::GenerateNewUniqueId - about to check DoesIdExistInList() but list hasn't been created yet");

		if ( (m_Value != 0) && (!CSavegamePhotoLocalList::DoesIdExistInList(this)) )	//	don't allow 0 as a valid value. Don't allow a value that is already used by an existing local photo
		{
			photoDisplayf("CSavegamePhotoUniqueId::GenerateNewUniqueId - Unique Id is %d", m_Value);
			bFoundAUniqueValue = true;
		}
		else
		{
			photoDisplayf("CSavegamePhotoUniqueId::GenerateNewUniqueId - Unique Id %d is either 0 or already used by another local photo so picking another random number", m_Value);
		}
	}
}

