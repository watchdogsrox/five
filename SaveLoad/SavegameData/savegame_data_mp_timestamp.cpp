
#include "savegame_data_mp_timestamp.h"
#include "savegame_data_mp_timestamp_parser.h"


// Rage headers
#include "rline/rl.h"

// Game headers
#include "SaveLoad/GenericGameStorage.h"
#include "SaveLoad/savegame_channel.h"
#include "SaveLoad/savegame_defines.h"



CPosixTimeStampForMultiplayerSaves::CPosixTimeStampForMultiplayerSaves()
{
	m_PosixHigh = 0;
	m_PosixLow = 0;
}

void CPosixTimeStampForMultiplayerSaves::PreSave()
{
	if (!CGenericGameStorage::AllowPreSaveAndPostLoad("CPosixTimeStampForMultiplayerSaves::PreSave"))
	{
		savegameAssertf(0, "CPosixTimeStampForMultiplayerSaves::PreSave - didn't expect this MP function to get called during the import/export of an SP savegame");
		return;
	}

	u64 currentPosixTime = rlGetPosixTime();

	m_PosixHigh = currentPosixTime >> 32;
	m_PosixLow = currentPosixTime & 0xffffffff;
}

void CPosixTimeStampForMultiplayerSaves::PostLoad()
{
	if (!CGenericGameStorage::AllowPreSaveAndPostLoad("CPosixTimeStampForMultiplayerSaves::PostLoad"))
	{
		savegameAssertf(0, "CPosixTimeStampForMultiplayerSaves::PostLoad - didn't expect this MP function to get called during the import/export of an SP savegame");
		return;
	}

#if __ALLOW_LOCAL_MP_STATS_SAVES
	u64 timeStamp = ((u64) m_PosixHigh) << 32;
	timeStamp |= m_PosixLow;

	CGenericGameStorage::SetTimestampReadFromMpSave(timeStamp);
#endif	//	__ALLOW_LOCAL_MP_STATS_SAVES
}

