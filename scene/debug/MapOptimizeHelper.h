/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    scene/debug/MapOptimizeHelper.h
// PURPOSE : widget to guide map optimisation effort, tracking txd costs etc
// AUTHOR :  Ian Kiigan
// CREATED : 21/01/11
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef _SCENE_DEBUG_MAPOPTIMIZEHELPER_H_
#define _SCENE_DEBUG_MAPOPTIMIZEHELPER_H_

#if __BANK

#include "atl/array.h"
#include "bank/bank.h"
#include "debug/UiGadget/UiGadgetList.h"
#include "debug/UiGadget/UiGadgetWindow.h"
#include "renderer/PostScan.h"
#include "scene/Entity.h"
#include "scene/RegdRefTypes.h"
#include "vector/vector3.h"

class CTxdRef;

class CMapOptimizeHelper
{
public:
	static void AddWidgets(bkBank& bank);
	static void Update();
	static bool ShouldDraw(CEntity* pEntity);
	static bool IsolateEnabled() { return ms_bIsolateSelected; }
	static void UpdateFromPvs(CGtaPvsEntry* pPvsStart, CGtaPvsEntry* pPvsStop);
	
	enum
	{
		MODE_TXD = 0,
		MODE_ORPHANENTITY,
		MODE_LODDIST,

		MODE_TOTAL
	};
	enum
	{
		TXDLIST_COLUMN_NAME = 0,
		TXDLIST_COLUMN_SIZE
	};
	enum
	{
		ENTITYLIST_COLUMN_NAME = 0,
		ENTITYLIST_COLUMN_DIST
	};
	enum
	{
		ORPHANLIST_COLUMN_NAME = 0,
		ORPHANLIST_COLUMN_LODDIST,
		ORPHANLIST_COLUMN_COST
	};
	enum
	{
		BADLODDISTLIST_COLUMN_NAME = 0,
		BADLODDISTLIST_COLUMN_LODDIST,
		BADLODDISTLIST_COLUMN_FAULT
	};
	enum eLodDistFault
	{
		BADLODDIST_FAULT_NONE = 0,
		BADLODDIST_FAULT_TOOSMALL,
		BADLODDIST_FAULT_DOESNTMATCHSIBLINGS,
		BADLODDIST_FAULT_GREATERTHANPARENT,
	};
	enum
	{
		MAPDATA_TYPE_MAPONLY = 0,
		MAPDATA_TYPE_PROPSONLY,
		MAPDATA_TYPE_MAPANDPROPS,
		MAPDATA_TYPE_ALL, // including peds and vehicles

		MAPDATA_TYPE_TOTAL
	};

	enum
	{
		INSPECTOR_STAT_ENTITYTYPE = 0,
		INSPECTOR_STAT_LODTYPE,
		INSPECTOR_STAT_LODDIST,
		INSPECTOR_STAT_ALPHA,
		INSPECTOR_STAT_BLOCK,
		INSPECTOR_STAT_POLYCOUNT,

		INSPECTOR_STAT_TOTAL
	};

private:
	static inline void RequestNewScanCB() { ms_bNewScanRequired = true; }
	static inline void RequestNewPvsScanCB() { ms_bNewPvsScanRequired = true; }
	static inline void LogWriteRequired() { ms_bWriteToLog = true; }
	static inline void LogWriteUnusedTextures() { ms_bWriteUnusedTexturesToLog = true; }
	static void PerformNewScan(const Vector3& vPos, bool bPerformScan, bool bFiltered);
	static bool AppendToListCB(CEntity* pEntity, void* pData);
	static void CreateGadgets();
	static void DestroyGadgets();
	static s32 CompareTxdRefSizeCB(const CTxdRef* pTxdRef1, const CTxdRef* pTxdRef2);
	static s32 CompareNameCB(const u32* pIndex1, const u32* pIndex2);
	static s32 CompareLodDistCB(const u32* pIndex1, const u32* pIndex2);
	static void SwitchModeCB();
	static void EnabledCB();
	static void IsolateCB();
	static void TextureViewerCB();
	static void UpdateCellForAsset(CUiGadgetText* pResult, u32 row, u32 col, void * extraCallbackData );
	static void UpdateCellForEntity(CUiGadgetText* pResult, u32 row, u32 col, void * extraCallbackData );
	static void UpdateCellForOrphanEntity(CUiGadgetText* pResult, u32 row, u32 col, void * extraCallbackData );
	static void UpdateCellForBadLodDistEntity(CUiGadgetText* pResult, u32 row, u32 col, void * extraCallbackData );
	static void RegenSortedEntityList();
	static void WriteList();
	static void SelectLogFile();
	static void WriteListToFile();
	static void WriteUnusedTexturesToFile();
	static void UpdateInspector(CEntity* pEntity);
	static void FilterAssetsCB();
	static u32	FindLodDistFault(CEntity* pEntity);
	
	static bool ms_bEnabled;
	static s32 ms_currentMode;
	static bool ms_bShowResultsInTextureViewer;
	static bool ms_bNewScanRequired;
	static bool ms_bNewPvsScanRequired;
	static bool ms_bIsolateSelected;
	static bool ms_bWriteToLog;
	static bool ms_bWriteUnusedTexturesToLog;
	static bool ms_bWriteUnusedTexturesOnlyUsed;
	static bool ms_bWriteUniqueTextures;
	static bool ms_bWriteUniqueDuplicatesOnly;
	static bool ms_bWriteUniqueExcludePed;
	static char ms_achLogFileName[256];
	static char ms_achFilterName[256];
	static CUiGadgetWindow* ms_pWindow;
	static CUiGadgetList* ms_pList1;
	static CUiGadgetList* ms_pList2;
	static CUiGadgetList* ms_pList3;
	static CUiGadgetList* ms_pList4;
	static CUiGadgetBox* ms_pInspectorBg;
	static s32 ms_backupAssetIndex;
	static s32 ms_backupEntityIndex;
	static s32 ms_mapDataType;
	static s32 ms_radius;
	static atArray<RegdEnt> ms_entities;
	static atArray<CTxdRef> ms_sortedTxdRefList;
	static atArray<u32> ms_sortedEntityIndices;
	static atArray<u32> ms_sortedOrphanEntityIndices;
	static atArray<u32> ms_sortedBadLodDistEntityIndices;
	static CUiGadgetText* ms_apStatNames[INSPECTOR_STAT_TOTAL];
	static CUiGadgetText* ms_apStatValues[INSPECTOR_STAT_TOTAL];
};

#endif	//__BANK

#endif	//_SCENE_DEBUG_MAPOPTIMIZEHELPER_H_