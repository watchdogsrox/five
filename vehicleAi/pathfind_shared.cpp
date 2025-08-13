#include "vehicleai/pathfind.h"

#if !__SPU
#include "fwscene/world/WorldLimits.h"
#endif


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : FindXCoorsForRegion
// PURPOSE :  Give a Region this function returns the coordinate of the left corner
/////////////////////////////////////////////////////////////////////////////////
float CPathFind::FindXCoorsForRegion(s32 Region) const
{
	return WORLDLIMITS_REP_XMIN + Region * PATHFINDREGIONSIZEX;
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : FindYCoorsForRegion
// PURPOSE :  Give a Region this function returns the coordinate of the left corner
/////////////////////////////////////////////////////////////////////////////////
float CPathFind::FindYCoorsForRegion(s32 Region) const
{
	return WORLDLIMITS_REP_YMIN + Region * PATHFINDREGIONSIZEY;
}