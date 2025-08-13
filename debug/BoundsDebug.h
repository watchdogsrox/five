/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    debug/BoundsDebug.h
// PURPOSE : useful debug functions static bounds etc
// AUTHOR :  Ian Kiigan
// CREATED : 27/09/11
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef _DEBUG_BOUNDSDEBUG_H_
#define _DEBUG_BOUNDSDEBUG_H_

#if __BANK

#include "atl/array.h"
#include "bank/bank.h"
#include "bank/bkmgr.h"
#include "streaming/streamingdefs.h"

class CEntity;

class CBoundsDebug
{
public:
	enum
	{
		MODE_NONE,
		MODE_COLLISION_UNDER_MOUSE,
		MODE_COLLISION_FROM_LIST,

		MODE_TOTAL
	};

	static void InitWidgets(bkBank& bank);
	static void Update();

private:
	static void DisplayCollisionUnderMouse();
	static void DisplayCollisionInList();
	static void DrawStaticBounds(strLocalIndex slotIndex, CEntity* pEntity=NULL);
	static s32 CompareNameCB(const s32* pIndex1, const s32* pIndex2);
	static void DisplayWeaponMoverBreakdownSummary();

	static atArray<s32> ms_slotIndices;
	static atArray<const char*> ms_apszDebugSlotNames;
	static s32 ms_selectedBoundsComboIndex;
	static s32 ms_mode;
	static bool ms_bDisplayMoverWeaponBreakdown;
};

#endif	//__BANK

#endif	//_DEBUG_BOUNDSDEBUG_H_