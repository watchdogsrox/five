
#include "savegame_data_camera.h"
#include "savegame_data_camera_parser.h"


// Game headers
#include "camera/helpers/ControlHelper.h"
#include "SaveLoad/GenericGameStorage.h"


void CCameraSaveStructure::PreSave()
{
	if (!CGenericGameStorage::AllowPreSaveAndPostLoad("CCameraSaveStructure::PreSave"))
	{
		return;
	}

	m_ContextViewModeMap.Reset();

	camControlHelper::StoreContextViewModesInMap(m_ContextViewModeMap);
}

void CCameraSaveStructure::PostLoad()
{
	if (!CGenericGameStorage::AllowPreSaveAndPostLoad("CCameraSaveStructure::PostLoad"))
	{
		return;
	}

	camControlHelper::RestoreContextViewModesFromMap(m_ContextViewModeMap);
}


#if __ALLOW_IMPORT_OF_SP_SAVEGAMES
void CCameraSaveStructure::Initialize(CCameraSaveStructure_Migration &copyFrom)
{
	m_ContextViewModeMap.Reset();
	m_ContextViewModeMap.Reserve(copyFrom.m_ContextViewModeMap.GetCount());

	atBinaryMap<u32, u32>::Iterator viewModeIterator = copyFrom.m_ContextViewModeMap.Begin();
	while (viewModeIterator != copyFrom.m_ContextViewModeMap.End())
	{
		u32 keyAsU32 = viewModeIterator.GetKey();
		atHashValue keyAsHashValue(keyAsU32);
		u32 valueAsU32 = *viewModeIterator;
		atHashValue valueToInsert(valueAsU32);
		m_ContextViewModeMap.Insert(keyAsHashValue, valueToInsert);

		viewModeIterator++;
	}
	m_ContextViewModeMap.FinishInsertion();
}
#endif // __ALLOW_IMPORT_OF_SP_SAVEGAMES


//	#if __ALLOW_EXPORT_OF_SP_SAVEGAMES
void CCameraSaveStructure_Migration::Initialize(CCameraSaveStructure &copyFrom)
{
	m_ContextViewModeMap.Reset();
	m_ContextViewModeMap.Reserve(copyFrom.m_ContextViewModeMap.GetCount());

	atBinaryMap<atHashValue, atHashValue>::Iterator viewModeIterator = copyFrom.m_ContextViewModeMap.Begin();
	while (viewModeIterator != copyFrom.m_ContextViewModeMap.End())
	{
		u32 keyAsU32 = viewModeIterator.GetKey().GetHash();
		u32 valueToInsert = (*viewModeIterator).GetHash();
		m_ContextViewModeMap.Insert(keyAsU32, valueToInsert);

		viewModeIterator++;
	}
	m_ContextViewModeMap.FinishInsertion();
}

void CCameraSaveStructure_Migration::PreSave()
{
	if (!CGenericGameStorage::AllowPreSaveAndPostLoad("CCameraSaveStructure_Migration::PreSave"))
	{
		return;
	}
}

void CCameraSaveStructure_Migration::PostLoad()
{
	if (!CGenericGameStorage::AllowPreSaveAndPostLoad("CCameraSaveStructure_Migration::PostLoad"))
	{
		return;
	}
}

//	#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES
