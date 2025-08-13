//
//
//    Filename: ModelInfo.h
//     Creator: Adam Fowler
//     $Author: $
//       $Date: $
//   $Revision: $
// Description: Class containing all the lists of models

//
//
#ifndef INC_MODELINFO_H_
#define INC_MODELINFO_H_

#include "data/struct.h"
#include "entity/archetypemanager.h"
#include "entity/entity.h"

// Game headers
#include "debug/Debug.h"
#include "modelinfo/BaseModelInfo.h"
#include "scene/2deffect.h"
#include "templates/StaticStore.h"

#define DEBUGINFO_2DFX 0

class CBaseModelInfo;
class CTimeModelInfo;
class CWeaponModelInfo;
class CVehicleModelInfo;
class CPedModelInfo;
class CMloModelInfo;
class CCompEntityModelInfo;
//struct CMloInstance;
class C2dEffect;
namespace rage { class strStreamingModule; }

class CArchetypeStreamSlot {
public:
	enum{
		UNUSED			=	0,
		REQUESTED		=	1,
		LOADED			=	2,

		RECYCLE_TIME	=	5
	};

	CArchetypeStreamSlot(fwModelId modelId) { m_modelId = modelId; m_timeStamp = ms_timer; }
	~CArchetypeStreamSlot() {}

	FW_REGISTER_CLASS_POOL(CArchetypeStreamSlot); 

	fwModelId GetModelId() { return(m_modelId); }

	// give a request 5 frames before recycling the slot
	bool	IsDueForRecycling()		{ if ((m_state==REQUESTED) && (m_timeStamp == ms_timer)) { return(true); }  return(false);}

	static void	Update(void) { ms_timer = (ms_timer + 1)%RECYCLE_TIME; }

private:
	fwModelId	m_modelId;
	u32			m_state : 16;
	u32			m_timeStamp : 8;

	static u32		ms_timer;	
};

#if __BANK
struct sDebugSize
{
    u32 main;
    u32 vram;
    u32 mainHd;
    u32 vramHd;
    const char* name;
};
#endif // __BANK


//
//   Class Name: CModelInfo
//  Description: Class holding all the lists of model types
//
class CModelInfo
{
public:
	enum{
		INVALID_LOCAL_INDEX		= -1
	};

    static void Init(unsigned initMode);

	static void RegisterStreamingModule();

	static void Shutdown(unsigned shutdownMode);

	static void ShutdownModelInfoExtra(rage::fwArchetype *arch);

	static CBaseModelInfo* GetBaseModelInfo(fwModelId id) { return (CBaseModelInfo *) fwArchetypeManager::GetArchetype(id); }
	static CBaseModelInfo* GetBaseModelInfoFromName(const strStreamingObjectName name, fwModelId* pModelId)	{ u32 index = fwModelId::MI_INVALID; CBaseModelInfo* ret = (CBaseModelInfo *) fwArchetypeManager::GetArchetype(name, &index); if (pModelId) pModelId->SetModelIndex(index); return(ret); }
	static CBaseModelInfo* GetBaseModelInfoFromHashKey(u32 key, fwModelId* pModelId)				{ u32 index = fwModelId::MI_INVALID; CBaseModelInfo* ret = (CBaseModelInfo *) fwArchetypeManager::GetArchetypeFromHashKey(key, &index); if (pModelId) pModelId->SetModelIndex(index); return(ret); }
	static CBaseModelInfo* GetFullBaseModelInfoFromName(const strStreamingObjectName name, fwModelId* pModelId)	{ CBaseModelInfo* ret = (CBaseModelInfo *) fwArchetypeManager::GetArchetype(name, pModelId); return(ret); }

	static fwModelId GetModelIdFromName(const strStreamingObjectName name) { u32 index = fwModelId::MI_INVALID; fwArchetypeManager::GetArchetype(name, &index); return(fwModelId(strLocalIndex(index))); }
#if !__NO_OUTPUT
	static const char* GetBaseModelInfoName(fwModelId id)  { return (fwArchetypeManager::GetArchetypeName(id.GetModelIndex())); }
#endif

	static void CopyOutCurrentMPPedMapping();
	static void ValidateCurrentMPPedMapping();

	static CBaseModelInfo* AddBaseModel(const char* name, u32 *modelIndex = NULL);
	static CWeaponModelInfo* AddWeaponModel(const char* name, bool permanent = true, s32 mapTypeDefIndex = fwFactory::GLOBAL);
	static CVehicleModelInfo* AddVehicleModel(const char* name, bool permanent = true, s32 mapTypeDefIndex = fwFactory::GLOBAL);
	static CPedModelInfo* AddPedModel(const char* name, bool permanent = true, s32 mapTypeDefIndex = fwFactory::GLOBAL);
	static CCompEntityModelInfo* AddCompEntityModel(const char* name);


	static void PostLoad();
	static void PostLoad(fwArchetype* pArchetype);

	static void PrintModelInfoStoreUsage();

	static s32 GetStreamingModuleId()					{return fwArchetypeManager::GetStreamingModuleId();}
	static strStreamingModule* GetStreamingModule()		{return fwArchetypeManager::GetStreamingModule();}

	static inline bool	IsValidModelInfo(u32 MI) { return ( (MI != fwModelId::MI_INVALID) && (MI < fwArchetypeManager::GetMaxArchetypeIndex()) );}
	static inline u32  GetMaxModelInfos(void) { return(fwArchetypeManager::GetMaxArchetypeIndex()); }
	static inline u32 GetStartModelInfos(void) { return(fwArchetypeManager::GetStartArchetypeIndex()); }

	static void UpdateVehicleClassInfos();	

#if DEBUGINFO_2DFX
	static void DebugInfo2dEffect();
#endif

#if __DEV
	static void	CheckMissingPedsInPedPersonalityFile();
	static void	CheckMissingVehicleColours();
	static void	UpdateVfxPedInfos();
	static void	UpdateVfxVehicleInfos();
#endif
#if __BANK
	static void CheckVehicleAssetDependencies();
	static void DumpVehicleModelInfos();
	static void DumpAverageVehicleSize();
	static void DumpAveragePedSize();
	static void DebugRecalculateAllVehicleCoverPoints();
	static void UpdateVehicleHandlingInfos();
#endif
	static void ClearAllCountedAsScenarioPedFlags();

private:
	template <class T>
	static fwArchetypeDynamicFactory<T>& GetArchetypeFactory(const int id)	{ return *static_cast< fwArchetypeDynamicFactory<T>* >( fwArchetypeManager::GetArchetypeFactory(id) ); }
	
	template <class T>
	static fwArchetypeExtensionFactory<T>& Get2dEffectFactory(const int id)	{ return *static_cast< fwArchetypeExtensionFactory<T>* >( fwArchetypeManager::Get2dEffectFactory(id) ); }

public:
	static fwArchetypeDynamicFactory<CBaseModelInfo>& GetBaseModelInfoStore();
	static fwArchetypeDynamicFactory<CTimeModelInfo>& GetTimeModelInfoStore();
	static fwArchetypeDynamicFactory<CMloModelInfo>& GetMloModelInfoStore();
	static fwArchetypeDynamicFactory<CCompEntityModelInfo>& GetCompEntityModelInfoStore();

	static fwArchetypeDynamicFactory<CWeaponModelInfo>& GetWeaponModelInfoStore();
	static fwArchetypeDynamicFactory<CVehicleModelInfo>& GetVehicleModelInfoStore();
	static fwArchetypeDynamicFactory<CPedModelInfo>& GetPedModelInfoStore();

	//static fwArchetypeDynamicFactory<CMloInstance>& GetMloInstanceStore();

	// converted to effect factories
	static fwArchetypeExtensionFactory<CSpawnPoint>& GetSpawnPointStore();

	// I think the worldpointStore is only used through explicit loading of old format .ipl files in fileloader.cpp 
	static fwArchetypeExtensionFactory<CWorldPointAttr>& GetWorldPointStore();

	// I thing the world2dEffectArray is _only_ storing ptrs to entries in the SpawnPointStore
	static atArray<C2dEffect*>& GetWorld2dEffectArray() { return (ms_World2dEffectArray); }

	// ---

	// mediated streaming interface
	static CBaseModelInfo* GetModelInfoFromLocalIndex(s32 idx);

	static void UpdateArchetypeStreamSlots();

	// watch out for code paths which assign - if the request is terminated then it _must_ be paired with a ReleaseStreamingIndex to prevent leaks!
	static strLocalIndex	AssignLocalIndexToModelInfo(fwModelId modelId) { return(modelId.ConvertToStreamingIndex()); }
	static strLocalIndex	AssignLocalIndexToModelInfo2(fwModelId modelId);

	static void ReleaseAssignedLocalIndex(fwModelId modelId);

	static strLocalIndex	LookupLocalIndex(fwModelId modelId) { return strLocalIndex(modelId.ConvertToStreamingIndex()); }

	static bool HaveAssetsLoaded(fwModelId modelId);
	static bool HaveAssetsLoaded(atHashString modelId);
	static bool HaveAssetsLoaded(CBaseModelInfo* pModelInfo);
	static bool AreAssetsLoading(fwModelId modelId);
	static bool RequestAssets(fwModelId modelId, u32 streamFlags);
	static bool RequestAssets(CBaseModelInfo* pModelInfo, u32 streamFlags);
	static bool AreAssetsRequested(fwModelId modelId);

	static void RemoveAssets(fwModelId modelId);

	static void SetAssetsAreDeletable(fwModelId modelId);
	static bool GetAssetsAreDeletable(fwModelId modelId);

	static bool GetAssetRequiredFlag(fwModelId modelId);
	static void SetAssetRequiredFlag(fwModelId modelId, u32 streamFlags);
	static void ClearAssetRequiredFlag(fwModelId modelId,u32 streamFlags);

	static u32 GetAssetStreamFlags(fwModelId modelId);

	static void GetObjectAndDependencies(fwModelId modelId, atArray<strIndex>& allDeps, const strIndex* ignoreList, s32 numIgnores);
	static void GetObjectAndDependenciesSizes(fwModelId modelId, u32& virtualSize, u32& physicalSize, const strIndex* ignoreList=NULL, s32 numIgnores=0, bool mayFail = false);

	static void SetGlobalResidentTxd( int txd);

private:
	static atArray<C2dEffect*>				ms_World2dEffectArray;

	static u32		ms_obfMaleMPPlayerModelId;
	static u32		ms_obfFemaleMPPlayerModelId;
};

#endif // INC_MODELINFO_H_
