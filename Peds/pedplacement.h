
/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    pedplacement.cpp
// PURPOSE : One or two handy functions to deal with the placement of
//			 peds. It seemed handy to put this in a separate file since
//			 it was needed in so many different bits of the code.
//			 (player leaving the car, ped leaving a car, cops generated
//			 for a roadblock etc)
// AUTHOR :  Obbe.
// CREATED : 15/08/00
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef _PEDPLACEMENT_H_
#define _PEDPLACEMENT_H_

#include "physics/gtaArchetype.h"
#include "vector/vector3.h"

class CPed;
class CEntity;
class CInteriorInst;

/////////////////////////////////////////////////////////////////////////////////
// A class to contain the functions.
/////////////////////////////////////////////////////////////////////////////////

class CPedPlacement
{
public:
	static bool	IsPositionClearForPed(const Vector3& vPos, float fRange=-1, bool useBoundBoxes = true, int MaxNum=-1, CEntity** pKindaCollidingObjects=0, bool considerVehicles=true, bool considerPeds=true, bool considerObjects=true);
	static CEntity *IsPositionClearOfCars(const Vector3 *pCoors);
	static CEntity *IsPositionClearOfCars(const CPed* pPed);

	static bool FindZCoorForPed(const float fSecondSurfaceInterp, Vector3 *pCoors, const CEntity* pException, s32* piRoomId = NULL, CInteriorInst** ppInteriorInst = NULL, float fZAmountAbove = 0.5f, float fZAmountBelow = 99.5f, const u32 iFlags = ArchetypeFlags::GTA_ALL_MAP_TYPES|ArchetypeFlags::GTA_VEHICLE_TYPE, bool* pbOnDynamicCollision = NULL, bool bIgnoreGroundOffset = false );

};

#endif // _PEDPLACEMENT_H_
