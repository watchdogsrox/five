
#include "savegame_data_stunt_jumps.h"
#include "savegame_data_stunt_jumps_parser.h"


// Game headers
#include "control/stuntjump.h"
#include "SaveLoad/GenericGameStorage.h"


void CStuntJumpSaveStructure::PreSave()
{
	if (!CGenericGameStorage::AllowPreSaveAndPostLoad("CStuntJumpSaveStructure::PreSave"))
	{
		return;
	}

	m_bActive = SStuntJumpManager::GetInstance().IsActive();
}

void CStuntJumpSaveStructure::PostLoad()
{
	if (!CGenericGameStorage::AllowPreSaveAndPostLoad("CStuntJumpSaveStructure::PostLoad"))
	{
		return;
	}

	//	Check that the following gets done by InitSession for stuntjumps
	//	SStuntJumpManager::GetInstance().Clear();
	SStuntJumpManager::GetInstance().SetActive(m_bActive);
}



