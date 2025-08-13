#ifndef SAVEGAME_DATA_MINI_MAP_H
#define SAVEGAME_DATA_MINI_MAP_H

// Rage headers
#include "atl/array.h"
#include "parser/macros.h"
#include "vector/vector2.h"


class CMiniMapSaveStructure
{	
public:	 

	class CPointOfInterestStruct
	{
	public:
		bool m_bIsSet;
		Vector2 m_vPos;

		PAR_SIMPLE_PARSABLE
	};

	atArray<CPointOfInterestStruct> m_PointsOfInterestList;

	void PreSave();
	void PostLoad();

	PAR_SIMPLE_PARSABLE
};

#endif // SAVEGAME_DATA_MINI_MAP_H

