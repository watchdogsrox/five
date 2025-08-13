/////////////////////////////////////////////////////////////////////////////////
// Title	:	zones.h
// Author	:	Graeme, Adam Croston
// Started	:	19.06.00
// Purpose	:	To keep track of game-play areas of the map.
/////////////////////////////////////////////////////////////////////////////////
#ifndef MAP_AREAS_H_
#define MAP_AREAS_H_

// Rage headers
#include "atl/array.h"		// For atArray
#include "vector/vector3.h"	// For Vector3

// Game headers
#include "text/text.h"		// For char


// Forward declarations.
class CMapArea;


/////////////////////////////////////////////////////////////////////////////////
// Name			:	CMapAreas
// Purpose		:	TODO
/////////////////////////////////////////////////////////////////////////////////
class CMapAreas
{
public:
	static void			Init                        (unsigned initMode);
	static void			Update						(void);
	
	// Map areas.
	static void			AddAreaBoxToArea			(const Vector3 &boxMin, const Vector3 &boxMax, u32 HashOfAreaName);
#if __DEV
	static const char*	GetAreaNameFromPosition		(const Vector3& vecPoint);
#endif // __DEV
	static u32		GetAreaNameHashFromPosition	(const Vector3& vecPoint);
	static int			GetAreaIndexFromPosition	(const Vector3& vecPoint);
	static bool			IsPositionInNamedArea		(const Vector3& vecPoint, u32 areaNameHash);

protected:
	static void			ResetMapAreas				(void);
	static CMapArea*	GetAreaFromNameHash			(u32 areaNameHash);
	static CMapArea*	AddAreaWithNameHash			(u32 AreaNameHash);

#if __ASSERT
	static void			CheckForBoxOverlap			(u32 FirstArea, u32 FirstBox, u32 SecondArea, u32 SecondBox);
	static bool			DoesOverlapInOneDimension(float FirstMin, float FirstMax, float SecondMin, float SecondMax);
	static void			CheckForOverlap				(void);
#endif // __ASSERT

	static atArray<CMapArea*> 	sm_mapAreas;
};


#endif // MAP_AREAS_H_
