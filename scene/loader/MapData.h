//
// scene/loader/mapdata.h
//
// Copyright (C) 1999-2011 Rockstar Games. All Rights Reserved.
//

#ifndef SCENE_LOADER_MAPDATA_H
#define SCENE_LOADER_MAPDATA_H

// fw includes
#include "entity/archetypemanager.h"
#include "fwscene/mapdata/mapdata.h"

// game includes
#include "parser/macros.h"
#include "data/base.h"
#include "atl/array.h"
#include "scene/loader/mapdata_interiors.h"
#include "scene/loader/MapData_Misc.h"
#include "fwscene/stores/mapdatastore.h"
#include "softrasterizer/occludeModel.h"
#include "vfx/misc/LODLightManager.h"

class CMapData;
class CInteriorProxy;

class CMapDataContents : public fwMapDataContents
{
public:
	virtual void Load(fwMapDataDef* pDef, fwMapData* pMapData, strLocalIndex mapDataSlotIndex);
	virtual void Remove(strLocalIndex mapDataSlotIndex);

	virtual void ConstructInteriorProxies(fwMapDataDef* pDef, fwMapData* pMapData, strLocalIndex mapDataSlotIndex);

protected:
	void Entities_PreAdd();
	void Entities_PostAdd(s32 mapDataSlotIndex, bool bIsHighDetail);
	void Entities_PostRemove();
	void CarGens_Create(CMapData* pMapData, s32 mapDataSlotIndex);
	void CarGens_Destroy(s32 mapDataSlotIndex);
	void TimeCycs_Create(CMapData* pMapData, s32 mapDataSlotIndex);
	void TimeCycs_Destroy(s32 mapDataSlotIndex);
	void Occluders_Create(CMapData* pMapData, s32 mapDataSlotIndex);
	void Occluders_Destroy(s32 mapDataSlotIndex);

	void LODLights_Create(CMapData* pMapData, s32 mapDataSlotIndex);
	void LODLights_Destroy(s32 mapDataSlotIndex);
	void DistantLODLights_Create(CMapData* pMapData, s32 mapDataSlotIndex);
	void DistantLODLights_Destroy(s32 mapDataSlotIndex);

#if __ASSERT
	void DebugPostLoadValidation();
#endif	//__ASSERT

#if __BANK
	static bool	bEkgEnabled;
	static void ToggleEkgActive(void);

public:
	virtual void LoadDebug(fwMapDataDef* pDef, fwMapData* pMapData);
	static void AddBankWidgets(bkBank* pBank);
#endif //__BANK
};

class CBlockDesc
{
public:
	CBlockDesc() { m_name = NULL; }
	u32 m_version;
	u32 m_flags;
	atString m_name;
	atString m_exportedBy;
	atString m_owner;
	atString m_time;

	PAR_SIMPLE_PARSABLE;
};

//
// CMapdata is the class that is at the root of the map metadata files.
//
class CMapData : public fwMapData
{
public:
	virtual ~CMapData();

	virtual fwMapDataContents* CreateMapDataContents() { return rage_new CMapDataContents(); }
		
	virtual void DestroyOccluders();

	atArray <CTimeCycleModifier>	m_timeCycleModifiers;
	atArray <CCarGen>				m_carGenerators;
	CLODLight						m_LODLightsSOA;
	CDistantLODLight				m_DistantLODLightsSOA;
	CBlockDesc						m_block;

private:
	PAR_PARSABLE;
};

#if __BANK
struct CMapDataGameInterface : public fwMapDataGameInterface
{
	virtual void CreateBlockInfoFromCache(fwMapDataDef* pDef, fwMapDataCacheBlockInfo* pBlockInfo);
	virtual void CreateCacheFromBlockInfo(fwMapDataCacheBlockInfo& output, s32 blockIndex);
};

extern CMapDataGameInterface g_MapDataGameInterface;
#endif	//__BANK
 
#endif // SCENE_LOADER_MAPDATA_H
