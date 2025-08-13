/////////////////////////////////////////////////////////////////////////////////
// Title	:	zones.cpp
// Author	:	Graeme, Adam Croston
// Started	:	19.06.00
// Purpose	:	To keep track of game-play areas of the map.
/////////////////////////////////////////////////////////////////////////////////
#include "game/zones.h"

// C headers
#include <stdio.h>
#include <string.h>

#include "spatialdata/aabb.h"

// Game headers
#include "core/game.h"
#include "game/clock.h"
#include "peds/popcycle.h"
#include "scene/world/gameWorld.h"
#include "script/script.h"
#include "system/filemgr.h"


#define CITY_NAME_HASH (4005646697)	//	(atStringHash("city"))
#define COUNTRYSIDE_NAME_HASH (2072609373)	//	(atStringHash("countryside"))


/////////////////////////////////////////////////////////////////////////////////
// Name			:	CMapArea
// Purpose		:	To store a game play area of a map defined by several boxes.
/////////////////////////////////////////////////////////////////////////////////
class CMapArea
{
public:
	CMapArea(u32 NameHash)
		: // Initializer list.
		m_nameHash		(NameHash)
	{
		m_areaBoxes.Reset();
	}

	~CMapArea()
	{
		const s32 numOfAreaBoxes = m_areaBoxes.GetCount();
		for (s32 loop = 0; loop < numOfAreaBoxes; loop++)
		{
			delete m_areaBoxes[loop];
		}
		m_areaBoxes.Reset();
	}

	u32		m_nameHash;

	atArray<spdAABB*> 	m_areaBoxes;
};


/////////////////////////////////////////////////////////////////////////////////
// Name			:	CPopZone
// Purpose		:	TODO
/////////////////////////////////////////////////////////////////////////////////
atArray<CMapArea*>	CMapAreas::sm_mapAreas;

/////////////////////////////////////////////////////////////////////////////////
// Name			:	InitLevel
// Purpose		:	TODO
// Parameters	:	None
// Returns		:	Nothing
/////////////////////////////////////////////////////////////////////////////////
void CMapAreas::Init(unsigned initMode)
{
    if(initMode == INIT_BEFORE_MAP_LOADED)
    {
	    ResetMapAreas();
    }
	else if (initMode == INIT_AFTER_MAP_LOADED)
	{
#if __ASSERT
		CheckForOverlap();
#endif	//	__ASSERT
	}
}


/////////////////////////////////////////////////////////////////////////////////
// Name			:	Update
// Purpose		:	TODO
// Parameters	:	None
// Returns		:	Nothing
/////////////////////////////////////////////////////////////////////////////////
void CMapAreas::Update(void)
{
}


/////////////////////////////////////////////////////////////////////////////////
// Name			:	AddAreaBox
// Purpose		:	For a given area adds a box at the given coordinates.
// Parameters	:	None
// Returns		:	NewX1, NewY1, NewZ1 - box start coords.
//					NewX2, NewY2, NewZ2 - box end coords.
//					areaIndex - which area is this box for.
/////////////////////////////////////////////////////////////////////////////////
void CMapAreas::AddAreaBoxToArea(const Vector3 &boxMin_, const Vector3 &boxMax_, u32 HashOfAreaName)
{
	Assertf( (HashOfAreaName == CITY_NAME_HASH) || (HashOfAreaName == COUNTRYSIDE_NAME_HASH), "CMapAreas::AddAreaBoxToArea - expected all zones in maparea.ipl to be either city or countryside");

	// Try to get the map area.
	CMapArea* pMapArea = GetAreaFromNameHash(HashOfAreaName);
	if(!pMapArea)
	{
		pMapArea = AddAreaWithNameHash(HashOfAreaName);
	}
	Assertf(pMapArea, "CMapAreas::AddAreaBoxToArea - could not get or create map area to add box to.");

	// Make sure the min and max values for the box is in the right order.
	Vec3V boxMin = Min(RCC_VEC3V(boxMin_), RCC_VEC3V(boxMax_));
	Vec3V boxMax = Max(RCC_VEC3V(boxMin_), RCC_VEC3V(boxMax_));

	// Try to add the box to the area.
	spdAABB* pNewArea = rage_new spdAABB();

	if (Verifyf(pNewArea, "CMapAreas::AddAreaBoxToArea - failed to allocate a new spdAABB to hold an area"))
	{
		pNewArea->Set(boxMin, boxMax);
		pMapArea->m_areaBoxes.PushAndGrow(pNewArea);
	}
}


/////////////////////////////////////////////////////////////////////////////////
// Name			:	ResetMapAreas
// Purpose		:	To clean the sm_mapAreas array out.
// Parameters	:	None
// Returns		:	Nothing
/////////////////////////////////////////////////////////////////////////////////
void CMapAreas::ResetMapAreas(void)
{
	const int mapAreaCount = sm_mapAreas.GetCount();
	for(int mapAreaIndex = 0; mapAreaIndex < mapAreaCount; ++mapAreaIndex)
	{
		delete sm_mapAreas[mapAreaIndex];
	}
	sm_mapAreas.Reset();
}


/////////////////////////////////////////////////////////////////////////////////
// Name			:	GetAreaFromNameHash
// Purpose		:	To find map areas by name.
// Parameters	:	areaNameHash - the hashed name of the area we're interested in.
// Returns		:	The map area or null if it couldn't e found.
/////////////////////////////////////////////////////////////////////////////////
CMapArea* CMapAreas::GetAreaFromNameHash(u32 areaNameHash)
{
	// Try to get the map area.
	for(int mapAreaIndex = 0; mapAreaIndex < sm_mapAreas.GetCount(); ++mapAreaIndex)
	{
		if(sm_mapAreas[mapAreaIndex]->m_nameHash == areaNameHash)
		{
			return sm_mapAreas[mapAreaIndex];
		}
	}

	return NULL;
}


/////////////////////////////////////////////////////////////////////////////////
// Name			:	AddAreaWithNameHash
// Purpose		:	To create a map area and add it to the list of map areas.
// Parameters	:	AreaNameHash - the hash of the name of the area to create.
// Returns		:	The new map area.
/////////////////////////////////////////////////////////////////////////////////
CMapArea* CMapAreas::AddAreaWithNameHash(u32 AreaNameHash)
{
	CMapArea* pMapArea = rage_new CMapArea(AreaNameHash);
	sm_mapAreas.PushAndGrow(pMapArea);

	return pMapArea;
}


#if __ASSERT
/////////////////////////////////////////////////////////////////////////////////
// Name			:	CheckForOverlap
// Purpose		:	TODO
// Parameters	:	None
// Returns		:	Nothing
/////////////////////////////////////////////////////////////////////////////////
void CMapAreas::CheckForOverlap(void)
{
	for(int FirstAreaIndex = 0; FirstAreaIndex < sm_mapAreas.GetCount(); ++FirstAreaIndex)
	{
		const u32 numAreaBoxesInFirstArea = sm_mapAreas[FirstAreaIndex]->m_areaBoxes.GetCount();
		for(u32 FirstBoxIndex = 0; FirstBoxIndex < numAreaBoxesInFirstArea; ++FirstBoxIndex)
		{
			for(int SecondAreaIndex = 0; SecondAreaIndex < sm_mapAreas.GetCount(); ++SecondAreaIndex)
			{
				const u32 numAreaBoxesInSecondArea = sm_mapAreas[SecondAreaIndex]->m_areaBoxes.GetCount();
				for(u32 SecondBoxIndex = 0; SecondBoxIndex < numAreaBoxesInSecondArea; ++SecondBoxIndex)
				{
					if((FirstAreaIndex == SecondAreaIndex) && (FirstBoxIndex == SecondBoxIndex))
					{	//	don't check a box against itself
					}
					else
					{
						CheckForBoxOverlap(FirstAreaIndex, FirstBoxIndex, SecondAreaIndex, SecondBoxIndex);
					}
				}
			}
		}
	}
}


/////////////////////////////////////////////////////////////////////////////////
// Name			:	CheckForBoxOverlap
// Purpose		:	TODO
// Parameters	:	TODO
// Returns		:	Nothing
/////////////////////////////////////////////////////////////////////////////////
void CMapAreas::CheckForBoxOverlap(u32 FirstArea, u32 FirstBox, u32 SecondArea, u32 SecondBox)
{
	spdAABB *pFirstBox = sm_mapAreas[FirstArea]->m_areaBoxes[FirstBox];
	spdAABB *pSecondBox = sm_mapAreas[SecondArea]->m_areaBoxes[SecondBox];

	if (pFirstBox->IntersectsAABB(*pSecondBox))
	{
		Assertf(0, "CMapAreas::CheckForBoxOverlap - Map area with Min <<%f,%f,%f>> Max <<%f,%f,%f>> overlaps with map area with Min <<%f,%f,%f>> Max <<%f,%f,%f>>", 
			VEC3V_ARGS(pFirstBox->GetMin()),
			VEC3V_ARGS(pFirstBox->GetMax()),
			VEC3V_ARGS(pSecondBox->GetMin()),
			VEC3V_ARGS(pSecondBox->GetMax()));
	}
}

bool CMapAreas::DoesOverlapInOneDimension(float FirstMin, float FirstMax, float SecondMin, float SecondMax)
{
	float LeftValue = Max(FirstMin, SecondMin);
	float RightValue = Min(FirstMax, SecondMax);

	float Difference = RightValue - LeftValue;

	if (Difference > 0.0f)
	{
		return true;
	}

	return false;
}
#endif // __ASSERT


/////////////////////////////////////////////////////////////////////////////////
// Name			:	GetAreaIndexFromPosition
// Purpose		:	TODO
// Parameters	:	TODO
// Returns		:	TODO
/////////////////////////////////////////////////////////////////////////////////
u32 CMapAreas::GetAreaNameHashFromPosition(const Vector3& vecPoint)
{
	s32 areaIndex = GetAreaIndexFromPosition(vecPoint);
	if (areaIndex >= 0)
	{
		return sm_mapAreas[areaIndex]->m_nameHash;
	}

	return COUNTRYSIDE_NAME_HASH;
}


/////////////////////////////////////////////////////////////////////////////////
// Name			:	GetAreaIndexFromPosition
// Purpose		:	TODO
// Parameters	:	TODO
// Returns		:	TODO
/////////////////////////////////////////////////////////////////////////////////
int CMapAreas::GetAreaIndexFromPosition(const Vector3& vecPoint)
{
	for(int areaIndex = 0; areaIndex < sm_mapAreas.GetCount(); ++areaIndex)
	{
		const u32 numOfAreaBoxes = sm_mapAreas[areaIndex]->m_areaBoxes.GetCount();
		for(u32 boxIndex = 0; boxIndex < numOfAreaBoxes; ++boxIndex)
		{
			if(sm_mapAreas[areaIndex]->m_areaBoxes[boxIndex]->ContainsPointFlat(RCC_VEC3V(vecPoint)))
			{
				return areaIndex;
			}
		}
	}

	return -1;
}


/////////////////////////////////////////////////////////////////////////////////
// Name			:	IsPositionInNamedArea
// Purpose		:	TODO
// Parameters	:	TODO
// Returns		:	TODO
/////////////////////////////////////////////////////////////////////////////////
bool CMapAreas::IsPositionInNamedArea(const Vector3& vecPoint, u32 areaNameHash)
{
	// Try to get the map area.
	CMapArea* pMapArea = GetAreaFromNameHash(areaNameHash);
	if(!pMapArea)
	{
		Assertf(false,"Unknown map area name");
		return false;
	}

	// Check if the point is in any of the areas boxes.
	const u32 numOfAreaBoxes = pMapArea->m_areaBoxes.GetCount();
	for(u32 boxIndex = 0; boxIndex < numOfAreaBoxes; ++boxIndex)
	{
		if(pMapArea->m_areaBoxes[boxIndex]->ContainsPointFlat(RCC_VEC3V(vecPoint)))
		{
			return true;
		}
	}

	return false;
}


#if __DEV
/////////////////////////////////////////////////////////////////////////////////
// Name			:	GetAreaNameFromPosition
// Purpose		:	TODO
// Parameters	:	TODO
// Returns		:	TODO
/////////////////////////////////////////////////////////////////////////////////
const char* CMapAreas::GetAreaNameFromPosition(const Vector3& vecPoint)
{
	const u32 MaxLengthOfMapAreaName = 48;
	static char MapAreaName[MaxLengthOfMapAreaName];

	for(int areaIndex = 0; areaIndex < sm_mapAreas.GetCount(); ++areaIndex)
	{
		const u32 numOfAreaBoxes = sm_mapAreas[areaIndex]->m_areaBoxes.GetCount();
		for(u32 boxIndex = 0; boxIndex < numOfAreaBoxes; ++boxIndex)
		{
			if(sm_mapAreas[areaIndex]->m_areaBoxes[boxIndex]->ContainsPointFlat(RCC_VEC3V(vecPoint)))
			{
				switch (sm_mapAreas[areaIndex]->m_nameHash)
				{
					case CITY_NAME_HASH :
						safecpy(MapAreaName, "city", MaxLengthOfMapAreaName);
						break;

					case COUNTRYSIDE_NAME_HASH :
						safecpy(MapAreaName, "countryside", MaxLengthOfMapAreaName);
						break;

					default :
						sprintf(MapAreaName, "MapAreaHash:%u", sm_mapAreas[areaIndex]->m_nameHash);
						break;
				}

				return MapAreaName;
			}
		}
	}

	return "DEFAULT MAP AREA";
}
#endif // __DEV
