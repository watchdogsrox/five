/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    scene/debug/SceneIsolator.h
// PURPOSE : debug tool for isolating entries in PVS and displaying info etc
// AUTHOR :  Ian Kiigan
// CREATED : 05/07/10
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef _SCENE_DEBUG_SCENEISOLATOR_H_
#define _SCENE_DEBUG_SCENEISOLATOR_H_

#if __BANK

#include "atl/array.h"
#include "spatialdata/aabb.h"
#include "vector/vector2.h"

class CEntity;

enum
{
	SCENEISOLATOR_COST_PHYS = 0,
	SCENEISOLATOR_COST_VIRT,
	SCENEISOLATOR_COST_TOTAL
};

class CSceneIsolator
{
public:
	static void AddWidgets(bkBank* pBank);
	static inline void Init() { ms_vResultsBoxPos.Set(200, 200); }
	static void Start();
	static void Add(CEntity* pEntity);
	static void Stop();
	static void Update();
	static void Draw();
	static inline CEntity* GetCurrentEntity() { return ms_pCurrent; }
	static void SortByName();
	static void SortByCost();
	static void SetSelectedInPicker();
	static int	GetEntityCost(const CEntity* pEntity, s32 eCost);

	static bool			ms_bLocked;
	static bool			ms_bEnabled;
	static bool			ms_bOnlyShowSelected;
	static bool			ms_bShowDescBoundingBox;
	static spdAABB	ms_descAabb;

private:
	static void DrawResultsBox();

	static CEntity*				ms_pCurrent;
	static s32					ms_index;
	static atArray<CEntity*>	ms_entities;
	static s32					ms_nNumSlots;
	static Vector2				ms_vResultsBoxPos;
	static Vector2				ms_vClickDragDelta;
	static bool					ms_bMoveResultsBox;
	static s32					ms_nSelectorIndex;
	static s32					ms_nNumResults;
	static s32					ms_nListOffset;
	static s32					ms_nIndexInList;
};

#endif //__BANK

#endif //_SCENE_DEBUG_SCENEISOLATOR_H_