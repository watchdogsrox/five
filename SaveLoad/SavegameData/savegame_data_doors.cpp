
#include "savegame_data_doors.h"
#include "savegame_data_doors_parser.h"


// Game headers
#include "Objects/Door.h"
#include "SaveLoad/GenericGameStorage.h"



void CSaveDoors::PreSave()
{
	if (!CGenericGameStorage::AllowPreSaveAndPostLoad("CSaveDoors::PreSave"))
	{
		return;
	}

	for (CDoorSystem::DoorSystemMap::Iterator doorIterator = CDoorSystem::GetDoorMap().CreateIterator(); !doorIterator.AtEnd(); doorIterator.Next())
	{
		const CDoorSystemData &dsd = *doorIterator.GetData();
		const Vector3 &position = dsd.GetPosition();		

		u32 posHash   = CDoorSystem::GenerateHash32(position);
		u32 enumHash  = u32(dsd.GetEnumHash());
		u32 modelHash =  u32(dsd.GetModelInfoHash());

		// When a door is added through the SetLocked interface it builds an enum hash using CDoorSystem::GenerateHash32()
		// We don't want to save these doors out.
		if (posHash == enumHash)
		{
			continue;
		}
		DoorState &ds = m_savedDoorStates.Grow();
		ds.m_doorEnumHash  = enumHash;
		ds.m_modelInfoHash = modelHash;
		ds.m_doorState     = dsd.GetState();
		ds.m_postion       = position;
	}
}

void CSaveDoors::PostLoad()
{
	if (!CGenericGameStorage::AllowPreSaveAndPostLoad("CSaveDoors::PostLoad"))
	{
		return;
	}

	for (int i = 0; i < m_savedDoorStates.GetCount(); ++i)
	{
		DoorState &ds = m_savedDoorStates[i];
		CDoorSystem::AddSavedDoor(ds.m_doorEnumHash, ds.m_modelInfoHash, (DoorScriptState)ds.m_doorState, ds.m_postion);
	}

}


