#ifndef SAVEGAME_DATA_DOORS_H
#define SAVEGAME_DATA_DOORS_H

// Rage headers
#include "atl/array.h"
#include "parser/macros.h"
#include "vector/vector3.h"


class CSaveDoors
{
public:
	class DoorState
	{
	public:
		Vector3 m_postion;
		int m_doorEnumHash;
		int m_modelInfoHash;
		int m_doorState;
		PAR_SIMPLE_PARSABLE
	};

	atArray<DoorState> m_savedDoorStates;

	void PreSave();
	void PostLoad();

	PAR_SIMPLE_PARSABLE
};

#endif // SAVEGAME_DATA_DOORS_H

