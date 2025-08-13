//
// entity/entitybatch.h : base class for batched entity lists
//
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved.
//

#ifndef SCENE_ENTITY_BATCH_H_
#define SCENE_ENTITY_BATCH_H_

#include "EntityBatch_Def.h"
#include "fwtl/pool.h"
#include "Entity.h"
#include "grcore/effect.h"

#include "system/SettingsManager.h"

#define KEEP_INSTANCELIST_ASSETS_RESIDENT (1)

namespace rage {
	class fwArchetype;
	class fwPropInstanceListDef;
	class fwGrassInstanceListDef;
	class grcVecArrayInstanceBufferList;
	class grcVertexBuffer;
	class bkBank;
} // namespace rage

template <class list_type>
struct CEntityBatchBaseTraits
{
	typedef list_type InstanceList;
	typedef list_type * InstanceListMember_type;
};

template <class list_type>
struct CEntityBatchBaseTraits<const list_type>
{
	typedef list_type InstanceList;
	typedef const list_type * InstanceListMember_type;
};

//Prop Entity Batch
template <class inst_def_type, class batch_traits = CEntityBatchBaseTraits<inst_def_type> >
class CEntityBatchBase : public CEntity
{
public:
	typedef CEntity parent_type;
	typedef batch_traits traits;
	typedef typename batch_traits::InstanceList InstanceList;
	typedef typename batch_traits::InstanceListMember_type InstanceListMember_type;
	
	CEntityBatchBase(const eEntityOwnedBy ownedBy);
	~CEntityBatchBase()	{ }

	virtual void InitVisibilityFromDefinition(const InstanceList *definition, fwArchetype *archetype);
	virtual void InitBatchTransformFromDefinition(const InstanceList *definition, fwArchetype *archetype);

	//Entity Interface
	virtual void SetModelId(fwModelId modelId);

	const InstanceList *GetInstanceList() const	{ return m_InstanceList; }
	s32 GetMapDataDefIndex() const	{ return m_MapDataDefIndex; }

#if __BANK
	virtual s32 GetNumInstances();
#endif

protected:
	InstanceListMember_type m_InstanceList;
	s32 m_MapDataDefIndex;
};

class CEntityBatch : public CEntityBatchBase<const fwPropInstanceListDef> //CEntity
{
public:
	typedef CEntityBatchBase<const fwPropInstanceListDef> parent_type;
	typedef CEntityBatchBase<fwPropInstanceListDef>::InstanceList InstanceList;

	CEntityBatch(const eEntityOwnedBy ownedBy);
	~CEntityBatch();

	FW_REGISTER_CLASS_POOL(CEntityBatch);

	PPUVIRTUAL void InitEntityFromDefinition(fwEntityDef* definition, fwArchetype* archetype, s32 mapDataDefIndex) 
		{parent_type::InitEntityFromDefinition(definition, archetype, mapDataDefIndex);}

	virtual void InitEntityFromDefinition(const fwPropInstanceListDef *definition, fwArchetype *archetype, s32 mapDataDefIndex);

	//Entity Interface
	PPUVIRTUAL fwDrawData *AllocateDrawHandler(rmcDrawable *pDrawable); // Overload CEntity's AllocateDrawHandler so we can allocate the custom batch draw handler.
	virtual void CalculateDynamicAmbientScales();

	u32 UpdateLinkedInstanceVisibility() const;	//returns number of visible instances.
	grcInstanceBuffer *CreateInstanceBuffer() const;
	u32 GetLod() const	{ return m_Lod; }

	s32 GetHdMapDataDefIndex() const	{ return m_HdMapDataDefIndex; }

	static void ResetHdEntityVisibility();

	//Some useful static debug functionality
#if __BANK
	static void RenderDebug();
	static void AddWidgets(bkBank &bank);
#endif

private:
	u32 m_Lod;
	s32 m_HdMapDataDefIndex; //Index of map data containing HD models.
};

class CGrassBatch : public CEntityBatchBase<fwGrassInstanceListDef> //CEntity
{
public:
	typedef CEntityBatchBase<fwGrassInstanceListDef> parent_type;
	typedef CEntityBatchBase<fwGrassInstanceListDef>::InstanceList InstanceList;

	struct DeviceResources
	{
		DeviceResources(fwGrassInstanceListDef &batch);
		~DeviceResources();

#if GRASS_BATCH_CS_CULLING
		grcBufferUAV m_RawInstBuffer;
		NON_COPYABLE(DeviceResources);
#else
		grcVertexBuffer *m_Verts;
#endif
	};

	CGrassBatch(const eEntityOwnedBy ownedBy);
	~CGrassBatch();

	FW_REGISTER_CLASS_POOL(CGrassBatch);

	static void Init(unsigned initMode);
	static void Shutdown();
	static void SetQuality(CSettings::eSettingsLevel quality);
	static bool IsRenderingEnabled();	//From quality settings

	PPUVIRTUAL void InitEntityFromDefinition(fwEntityDef* definition, fwArchetype* archetype, s32 mapDataDefIndex)
		{parent_type::InitEntityFromDefinition(definition, archetype, mapDataDefIndex);}

	virtual void InitEntityFromDefinition(fwGrassInstanceListDef *definition, fwArchetype* archetype, s32 mapDataDefIndex);

	virtual ePrerenderStatus PreRender(const bool bIsVisibleInMainViewport = true);

	//Entity Interface
	PPUVIRTUAL fwDrawData *AllocateDrawHandler(rmcDrawable *pDrawable); // Overload CEntity's AllocateDrawHandler so we can allocate the custom batch draw handler.

	static int GetSharedInstanceCount();
	static bool BindSharedBuffer();
	static float GetShadowLODFactor();

	const DeviceResources *GetDeviceResources() const	{ return GetInstanceList()->GetDeviceResources<DeviceResources>(); }
	float GetDistanceToCamera() const					{ return m_DistToCamera; }
	u32 ComputeLod(float dist) const;

#if GRASS_BATCH_CS_CULLING
	typedef EBStatic::GrassCSParams CSParams;
	void GetCurrentCSParams(CSParams &params) GRASS_BATCH_CS_DYNAMIC_BUFFERS_ONLY(const);
#else
	const grcVertexBuffer *GetInstanceVertexBuffer() const	{ return GetDeviceResources() ? GetDeviceResources()->m_Verts : NULL ; }
#endif //GRASS_BATCH_CS_CULLING

private:
	GRASS_BATCH_CS_CULLING_ONLY(EBStatic::GrassCSBaseParams m_CSParams);

	float m_DistToCamera;
};

#if KEEP_INSTANCELIST_ASSETS_RESIDENT

// loads props used by instance list rendering and keeps them resident
class CInstanceListAssetLoader
{
public:
	static void Init(u32 initMode);
	static void Shutdown(u32 initMode);

private:
	static void RequestAllModels(const char* pszItypName);
	static void ReleaseAllModels(const char* pszItypName);
	static void RequestModel(fwArchetype* pArchetype);
	static void ReleaseModel(fwArchetype* pArchetype);
};

#endif	//KEEP_INSTANCELIST_ASSETS_RESIDENT

#if __DEV && !__SPU
// forward declare so we don't get multiple definitions
template<> void fwPool<CEntityBatch>::PoolFullCallback();
template<> void fwPool<CGrassBatch>::PoolFullCallback();
#endif

#endif //SCENE_ENTITY_BATCH_H_
