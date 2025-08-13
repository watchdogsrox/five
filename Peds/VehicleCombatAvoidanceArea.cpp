// File header
#include "Peds/VehicleCombatAvoidanceArea.h"

// Rage headers
#include "ai/aichannel.h"

AI_OPTIMISATIONS()

//-------------------------------------------------------------------------
// CVehicleCombatAvoidanceArea
//-------------------------------------------------------------------------

FW_INSTANTIATE_CLASS_POOL(CVehicleCombatAvoidanceArea, CONFIGURED_FROM_FILE, atHashString("CVehicleCombatAvoidanceArea",0x670737c4));

#if __BANK
bool CVehicleCombatAvoidanceArea::ms_bRender = false;
#endif

CVehicleCombatAvoidanceArea::CVehicleCombatAvoidanceArea()
: CArea()
{
}

CVehicleCombatAvoidanceArea::~CVehicleCombatAvoidanceArea()
{
}

s32 CVehicleCombatAvoidanceArea::Add(Vec3V_In vStart, Vec3V_In vEnd, const float fWidth)
{
	//Grab the pool.
	CVehicleCombatAvoidanceArea::Pool* pPool = CVehicleCombatAvoidanceArea::GetPool();

	//Ensure there is room in the pool for a new area.
	if(!aiVerifyf(pPool->GetNoOfFreeSpaces() != 0, "No room in the pool for another avoidance area."))
	{
		return 0;
	}

	//Create a new area.
	CVehicleCombatAvoidanceArea* pArea = rage_new CVehicleCombatAvoidanceArea();
	pArea->Set(VEC3V_TO_VECTOR3(vStart), VEC3V_TO_VECTOR3(vEnd), fWidth);

	return pPool->GetIndex(pArea);
}

s32 CVehicleCombatAvoidanceArea::AddSphere(Vec3V_In vCenter, const float fRadius)
{
	//Grab the pool.
	CVehicleCombatAvoidanceArea::Pool* pPool = CVehicleCombatAvoidanceArea::GetPool();

	//Ensure there is room in the pool for a new area.
	if(!aiVerifyf(pPool->GetNoOfFreeSpaces() != 0, "No room in the pool for another avoidance area."))
	{
		return 0;
	}

	//Create a new area.
	CVehicleCombatAvoidanceArea* pArea = rage_new CVehicleCombatAvoidanceArea();
	pArea->SetAsSphere(VEC3V_TO_VECTOR3(vCenter), fRadius);

	return pPool->GetIndex(pArea);
}

void CVehicleCombatAvoidanceArea::Clear()
{
	//Grab the pool.
	CVehicleCombatAvoidanceArea::Pool* pPool = CVehicleCombatAvoidanceArea::GetPool();

	//Ensure there is at least one area.
	if(pPool->GetNoOfUsedSpaces() == 0)
	{
		return;
	}
	
	//Iterate over the pool.
	s32 iCount = pPool->GetSize();
	for(s32 i = 0; i < iCount; ++i)
	{
		//Grab the area.
		CVehicleCombatAvoidanceArea* pArea = pPool->GetSlot(i);
		if(!pArea)
		{
			continue;
		}
		
		//Free the area.
		delete pArea;
	}
}

void CVehicleCombatAvoidanceArea::Init(unsigned UNUSED_PARAM(initMode))
{
	//Init the pool.
	CVehicleCombatAvoidanceArea::InitPool(MEMBUCKET_GAMEPLAY);
}

bool CVehicleCombatAvoidanceArea::IsPointInAreas(Vec3V_In vPosition)
{
	//Grab the pool.
	CVehicleCombatAvoidanceArea::Pool* pPool = CVehicleCombatAvoidanceArea::GetPool();

	//Ensure there is at least one area.
	if(pPool->GetNoOfUsedSpaces() == 0)
	{
		return false;
	}

	//Iterate over the pool.
	s32 iCount = pPool->GetSize();
	for(s32 i = 0; i < iCount; ++i)
	{
		//Grab the area.
		CVehicleCombatAvoidanceArea* pArea = pPool->GetSlot(i);
		if(!pArea)
		{
			continue;
		}

		//Check if the point is in the area.
		if(pArea->IsPointInArea(VEC3V_TO_VECTOR3(vPosition)))
		{
			return true;
		}
	}

	return false;
}

void CVehicleCombatAvoidanceArea::Remove(const s32 iIndex)
{
	//Grab the pool.
	CVehicleCombatAvoidanceArea::Pool* pPool = CVehicleCombatAvoidanceArea::GetPool();

	//Ensure the index is valid.
// 	if(!aiVerifyf(iIndex >= 0 && iIndex < pPool->GetSize(), "Index is invalid: %d.", iIndex))
// 	{
// 		return;
// 	}

	//Ensure the area is valid.
	CVehicleCombatAvoidanceArea* pArea = pPool->GetAt(iIndex);
	if(!aiVerifyf(pArea, "Area at index: %d is invalid.", iIndex))
	{
		return;
	}

	//Free the area.
	delete pArea;
}

void CVehicleCombatAvoidanceArea::Shutdown(unsigned UNUSED_PARAM(shutdownMode))
{
	//Shut down the pool.
	CVehicleCombatAvoidanceArea::ShutdownPool();
}

#if __BANK
void CVehicleCombatAvoidanceArea::Debug()
{
	//Check if anything should be rendered.
	if(!ms_bRender)
	{
		return;
	}

	//Grab the pool.
	CVehicleCombatAvoidanceArea::Pool* pPool = CVehicleCombatAvoidanceArea::GetPool();

	//Ensure there is at least one area.
	if(pPool->GetNoOfUsedSpaces() == 0)
	{
		return;
	}

	//Iterate over the pool.
	s32 iCount = pPool->GetSize();
	for(s32 i = 0; i < iCount; ++i)
	{
		//Grab the area.
		CVehicleCombatAvoidanceArea* pArea = pPool->GetSlot(i);
		if(!pArea)
		{
			continue;
		}

		//Ensure the area is active.
		if(!pArea->IsActive())
		{
			continue;
		}

		//Render the area.
		pArea->IsPointInArea(VEC3_ZERO, true);
	}
}
#endif
