// Title	:	Building.h
// Author	:	Richard Jobling
// Started	:	22/06/99

#ifndef _BUILDING_H_
#define _BUILDING_H_

#include "fwtl/pool.h"
#include "scene/Entity.h"

#define NUM_MULTI_MODELINDICES	200

//
// name:		CBuilding
// description:	Describes a static object in the map
class CBuilding : public CEntity
{
public:
	CBuilding(const eEntityOwnedBy ownedBy);
	~CBuilding();

	virtual void InitEntityFromDefinition(fwEntityDef* definition, fwArchetype* archetype, s32 mapDataDefIndex);
	virtual bool CreateDrawable();

	FW_REGISTER_CLASS_POOL(CBuilding);	// was 40000

	#if __BANK
		static void RegenDLAlphas(void);
	#endif //__BANK
};

#ifdef DEBUG
void AssertBuildingPointerValid(CBuilding *pBuilding);
#else
#define AssertBuildingPointerValid( A )
#endif

bool IsBuildingPointerValid(CBuilding *pBuilding);

#if __BANK && !__SPU
// forward declare so we don't get multiple definitions
template<> void fwPool<CBuilding>::PoolFullCallback();
#endif

#endif
