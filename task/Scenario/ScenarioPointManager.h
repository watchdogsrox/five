#ifndef INC_SCENARIO_POINT_MANAGER_H
#define INC_SCENARIO_POINT_MANAGER_H

// Rage headers
#include "atl/array.h"
#include "atl/binmap.h"
#include "atl/singleton.h"
#include "parser/macros.h"
#include "spatialdata/aabb.h"

// Framework headers

// Game headers
#include "ai/spatialarraynode.h"
#include "scene/loader/MapData_Extensions.h"
#include "task/scenario/ScenarioPoint.h"


// Forward definitions
namespace rage {
	class bkGroup;
	class bkCombo;
	class parElement;
}
class CInteriorInst;
class CScenarioPointManager;
class CScenarioPointRegion;
class CScenarioChainSearchResults;
class CScenarioPointGroup;
class CSpatialArray;

typedef atSingleton<CScenarioPointManager> CScenarioPointManagerSingleton;
#define SCENARIOPOINTMGR CScenarioPointManagerSingleton::InstanceRef()
#define INIT_SCENARIOPOINTMGR											\
	do {																\
		if (!CScenarioPointManagerSingleton::IsInstantiated()){			\
		CScenarioPointManagerSingleton::Instantiate();					\
		}																\
	} while(0)															\
	//END
#define SHUTDOWN_SCENARIOPOINTMGR CScenarioPointManagerSingleton::Destroy()

class CScenarioPointRegionDef
{
public:
	CScenarioPointRegionDef(): m_SlotId(-1) 
#if SCENARIO_DEBUG
							 , m_NonStreamPointer(NULL)
#endif // SCENARIO_DEBUG
	{

	}

	atFinalHashString m_Name;
	spdAABB m_AABB;
	int m_SlotId; //non-pargen data
#if SCENARIO_DEBUG
	CScenarioPointRegion* m_NonStreamPointer;//for editor usage 
#endif // SCENARIO_DEBUG
	PAR_SIMPLE_PARSABLE;
};

class CScenarioPointManifest
{
public:
	CScenarioPointManifest();

	// PURPOSE:	Similar to CScenarioPointRegion::m_VersionNumber, this is a number that keeps increasing
	//			each time the file is saved, used to detect lack of proper PSO conversion.
	int									m_VersionNumber;

	atArray<CScenarioPointRegionDef*>	m_RegionDefs;
	
	atArray<CScenarioPointGroup*>		m_Groups;
	
	// PURPOSE:	Array of interior name hashes, for mapping from the interior index in scenario points.
	atArray<atHashWithStringBank>		m_InteriorNames;

	PAR_SIMPLE_PARSABLE;
};

class CScenarioPointManager : public datBase
{
public:
	static const int kNoGroup = 0;
	static const int kNoInterior = 0;
	static const unsigned kNoCustomModelSet = (MAX_MODELSET_TYPES-1);
	static const u8 kNoRequiredIMap = 0xff;

	// PURPOSE:	Value used in m_RequiredIMapStoreSlots when an IMAP file
	//			was specified, but couldn't be found at all.
	static const u16 kRequiredImapIndexInMapDataStoreNotFound = 0xfffe;

	// PURPOSE:	This constant is not used by CScenarioPointManager itself, but by a few users of
	//			CScenarioPointManager::FindScenarioPointsInArea(). It only affects how
	//			much stack space will be used for temporary arrays, and may have to be increased
	//			if the arrays fill up.
	static const int kMaxPtsInArea = SCENARIO_MAX_POINTS_IN_AREA;

	CScenarioPointManager();
	~CScenarioPointManager();

	// Initialise
	void Init();

	// Reset statics dependent on fwTimer.
	static void InitSession(u32 initMode);

	static void ShutdownSession(u32 shutdownMode);

	// Shutdown
	void Shutdown();

	// Load the data
	void Load();

	void Reset();

	//Append DLC data
	void Append(const char* filename, bool isPso);
	//Remove DLC data
	void Remove(const char* filename, bool isPso);

	bool LoadPso(const char* filename, CScenarioPointManifest& instance);
	bool LoadXml(const char* filename, CScenarioPointManifest& instance);

	void RegisterRegion(CScenarioPointRegionDef* rDef);

	void InvalidateRegion(CScenarioPointRegionDef* rDef);

	// Searches
    u32				FindPointsInArea(Vec3V_In centerPos, ScalarV_In radius, CScenarioPoint** destArray, u32 maxInDestArray, ScalarV_In radiusForExtendedRangeScenarios = ScalarV(V_ZERO), bool includeClusteredPoints = true);
	u32				FindPointsOfTypeForEntity(unsigned scenarioType, CEntity* attachedEntity, CScenarioPoint** destArray, u32 maxInDestArray);
	u32				FindPointsInGroup(unsigned groupId, CScenarioPoint **destArray, u32 maxInDestArray);
	CScenarioPoint*	FindClosestPointWithAttachedEntity(CEntity* entity, Vec3V_In position, int& scenarioTypeRealOut, CPed* pPedUser);
	u32				FindUnusedPointsWithAttachedEntity(CEntity* entity, CScenarioPoint** destArray, u32 maxInDestArray);

	// Used by INIT/SHUTDOWN session of scenario manager
	// NOTE: this should only be called by CScenarioManager Init/Shutdown
	//		to properly handle currently loaded points
	void HandleInitOnAddCalls();
	void HandleShutdownOnRemoveCalls();

	// Entity/World Point Management
	// NOTE: If CScenarioPoint->GetEntity() is NULL, AddScenarioPoint will defer to AddWorldPoint
	//		 If CScenarioPoint->GetEntity() is NOT NULL, AddScenarioPoint will defer to AddEntityPoint
	//		 Call the others explicitly if and only if you know which it is beforehand 
	void	AddScenarioPoint(CScenarioPoint& spoint, bool callOnAdd = true);
	void	AddEntityPoint(CScenarioPoint& spoint, bool callOnAdd = true);
	void	AddWorldPoint(CScenarioPoint& spoint, bool callOnAdd = true);

	// NOTE: Same goes for removal, If CScenarioPoint->GetEntity() is NULL, RemoveScenarioPoint will defer to RemoveWorldPoint
	//		 If CScenarioPoint->GetEntity() is NOT NULL, RemoveScenarioPoint will defer to RemoveEntityPoint
	bool	RemoveScenarioPoint(CScenarioPoint& spoint);
	bool	RemoveEntityPoint(CScenarioPoint& spoint);
	bool	RemoveWorldPoint(CScenarioPoint& spoint);

	void	DeletePointsWithAttachedEntity(CEntity* attachedEntity);
	void	DeletePointsWithAttachedEntities(const CEntity* const* attachedEntities, u32 numEntities);
	bool	ApplyOverridesForEntity(CEntity& entity);
	s32     GetNumEntityPoints() const;
	CScenarioPoint& GetEntityPoint(s32 index) { Assert(index < m_EntityPoints.GetCount()); Assert(m_EntityPoints[index]); return *m_EntityPoints[index]; }
	const CScenarioPoint& GetEntityPoint(s32 index) const { Assert(index < m_EntityPoints.GetCount()); Assert(m_EntityPoints[index]); return *m_EntityPoints[index]; }

	// Groups
	const CScenarioPointGroup& GetGroupByIndex(int groupIndex) const;
	int		FindGroupByNameHash(atHashString athash, bool mayNotExist = false) const;
	bool	IsGroupEnabled(int groupIndex) const {	return m_GroupsEnabled[groupIndex];	}
	void	SetGroupEnabled(int groupIndex, bool b);
	int		GetNumGroups() const;
	void	ResetGroupsEnabledToDefaults();
	void	SetExclusivelyEnabledScenarioGroupIndex(int groupIndex) { Assert(groupIndex >= -0x8000&& groupIndex < 0x8000); m_ExclusivelyEnabledScenarioGroupIndex = (s16)groupIndex; }
	int		GetExclusivelyEnabledScenarioGroupIndex() const { return m_ExclusivelyEnabledScenarioGroupIndex; }

	// Chaining
	int		FindChainedScenarioPoints(const CScenarioPoint& pt, CScenarioChainSearchResults* results, const int maxInDestArray) const;
	bool	IsChainedWithIncomingEdges(const CScenarioPoint& pt) const;
#if __ASSERT	// Intentionally __ASSERT-only for now, it's a bit on the slow side so other use is discouraged unless necessary.
	bool	IsChainEndPoint(const CScenarioPoint& pt) const;
#endif	// __ASSERT

	// Extra data
 	CScenarioPointExtraData* FindExtraData(const CScenarioPoint& pt) const;
 	CScenarioPointExtraData* FindOrCreateExtraData(CScenarioPoint& pt);
 	void	DestroyExtraData(CScenarioPoint& pt);

	// Required IMaps
	int FindOrAddRequiredIMapByHash(u32 nameHash);
	u32 GetRequiredIMapHash(unsigned index) const;
	unsigned GetRequiredIMapSlotId(unsigned index) const;

	// Counts
	int GetNumScenarioPointsEntity() const { return m_NumScenarioPointsEntity;	}
	int GetNumScenarioPointsWorld() const {	return m_NumScenarioPointsWorld;	}

	// Region

	// PURPOSE:	Get the total number of regions, streamed in or not.
	u32							GetNumRegions() const;

	// PURPOSE:	Get the pointer to a region by index, may be NULL if not streamed in.
	CScenarioPointRegion*		GetRegion(u32 idx) const;

	// PURPOSE:	Like GetRegion(), except faster, but assumes the user has already made sure that the region pointers are properly cached.
	CScenarioPointRegion*		GetRegionFast(int idx) const { Assert(m_CachedRegionPtrsValid); return  m_CachedRegionPtrs[idx]; }

	// PURPOSE:	Mark all cached region pointers as invalid, probably because something streamed in or out.
	void						InvalidateCachedRegionPtrs() { m_CachedRegionPtrsValid = false; }

	// PURPOSE:	Update m_CachedActiveRegionPtrs and m_CachedRegionPtrs, and set m_CachedRegionPtrsValid.
	//			Mostly used internally, users of the GetNumRegions()/GetRegion()/GetNumActiveRegions()/GetActiveRegion()
	//			interface usually don't need to call this directly.
	void						UpdateCachedRegionPtrs() const;

	// PURPOSE:	Get the number of regions that are currently streamed in, mostly useful
	//			in combination with GetActiveRegion().
	int							GetNumActiveRegions() const;

	// PURPOSE:	Get a reference to a streamed in region based on its index in m_CachedActiveRegionPtrs.
	//			GetNumActiveRegions() must have been called first to make sure the cached data is up to date.
	CScenarioPointRegion&		GetActiveRegion(int idx) const { Assert(m_CachedRegionPtrsValid); return *m_CachedActiveRegionPtrs[idx]; }

	// Interiors
	unsigned int				FindOrAddInteriorName(u32 nameHash);
	u32							GetInteriorNameHash(unsigned int index) const { return m_Manifest.m_InteriorNames[index].GetHash(); }
	bool						IsInteriorStreamedIn(unsigned int index) const;

	// PURPOSE: go through all the non region points and update the spatial array positions
	void UpdateNonRegionPointSpatialArray();
	
	//////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////

#if SCENARIO_DEBUG
	//Structure to support saving to multiple manifests at once
	struct SaveData
	{
		CScenarioPointManifest manifest;
		atHashString sourceFile;
		bool operator==(const atHashString &in)
		{
			return this->sourceFile == in;
		}
	};

	void BankLoad();


	void RenderPointSummary() const;

	//Editor interface... Clusters
	int	FindPointsClusterIndex(const CScenarioPoint& pt) const;

	//Editor interface... Chaining
	int	FindPointsChainedIndex(const CScenarioPoint& pt) const;

	//Editor interface... Interiors
	static const CInteriorInst*	FindInteriorForPoint(const CScenarioPoint& pt);
	const char*					GetInteriorName(unsigned int index) const { return m_Manifest.m_InteriorNames[index].GetCStr(); }
	bool						UpdateInteriorForPoint(CScenarioPoint& pt);
	bool						UpdateInteriorForRegion(int regionIndex);

	//Editor interface... Entity points
	s32					  GetNumWorldPoints() const;
	const CScenarioPoint& GetWorldPoint(s32 index) const;
	CScenarioPoint&		  GetWorldPoint(s32 index);
	void				  RemoveEntityPoint(s32 index); //Pass through function for RemoveEntityPointAtIndex

	//Editor interface... Regions
	const char*				GetRegionName(u32 idx) const;
	const char*				GetRegionSource(u32 idx) const;
	CScenarioPointRegion*	GetRegionByNameHash(u32 nameHash);
	const spdAABB&			GetRegionAABB(u32 idx) const;
	void					ReloadRegion(u32 idx);
	void					SaveRegion(u32 idx, bool resourceSaved);
	void					LoadSaveAllRegion(const bool resourceSaved);
	void					AddRegion(const char* name);
	void					DeleteRegionByIndex(u32 idx);
	void					RenameRegionByIndex(u32 idx, const char* name);
	void					UpdateRegionAABB(u32 idx);
	void					UpdateRegionAABB(const CScenarioPointRegion& region);
	void					FlagRegionAsEdited(u32 idx);
	bool					RegionIsEdited(u32 idx) const;
	bool					RemoveDuplicateChainingEdges();
	void					ForceLoadRegion(u32 idx);
	void					DeleteLoosePointsWithinRegion(u32 idx);

	//Editor interface ... groups
	CScenarioPointGroup& GetGroupByIndex(int groupIndex);
	CScenarioPointGroup& AddGroup(const char* name);
	void DeleteGroupByIndex(int index);
	void RenameGroup(u32 oldGroupID, u32 newGroupID);
	void ResetGroupForUsersOf(u32 groupName);
	void RestoreGroupFromUndoData(int groupIndex, const CScenarioPointGroup& undoData);

	// Save the data
	void Save(bool resourceSaved); //saves all data
	void SaveManifest(CScenarioPointManifest& manifest, const char* file); //saves all data
	bool ManifestIsEdited() const { return m_ManifestNeedSave; }
	void SetManifestNeedSave() { m_ManifestNeedSave = true; }

	// Add widgets
	void AddWidgets(bkBank* bank);
	void ClearScenarioGroupWidgets();
	void UpdateScenarioGroupWidgets();
	static void GroupsEnabledChangedCB();
	const CSpatialArray* GetNonRegionSpatialArray() const { return m_SpatialArray; }

	void UpdateScenarioTypeIds(const atArray<u32>& idMapping);
	void VerifyFileVersions();

	static bool ReadVersionNumber(const char* fileName, int& versionNumberOut);
	static void VersionCheckOnBeginCB(parElement& elt, bool isLeaf);
	static void VersionCheckOnEndCB(bool isLeaf);
	static void VersionCheckOnDataCB(char* data, int size, bool dataIncomplete);
	static int sm_VersionCheckVersionNumber;

#endif // SCENARIO_DEBUG

private:

	//Load/Save
	void PostLoad();

	void RemoveEntityPointAtIndex(s32 pointIndex);//Called by DeletePointsWithAttachedEntity
	void RemoveWorldPointAtIndex(s32 pointIndex);

	// Extra data
	int FindExtraDataIndex(const CScenarioPoint& pt) const;

	// PURPOSE:	Get a pointer to a CScenarioPointRegion by index, if streamed in, but don't use
	//			the cached data.
	// NOTES:	Only meant for internal use, GetRegion() is normally preferable.
	CScenarioPointRegion*		FindRegionByIndex(u32 idx) const;

	// PURPOSE:	Initialize the m_CachedActiveRegionPtrs and m_CachedRegionPtrs arrays, as empty
	//			but ready for calls to UpdateCachedRegionPtrs().
	void						InitCachedRegionPtrs();

	// PURPOSE:	Do some initialization of m_Manifest after being loaded (from XML or PSO).
	void						InitManifest();

	u32 AddToSpatialArray( u32 curIndex, const u32 maxSNodes, CScenarioPoint& pt );

#if SCENARIO_DEBUG
	void RenderPointPopularity(const atArray<CScenarioPoint*>& pointArray, u32 numToShow, Color32 color) const;

	struct PopularScenarioType 
	{
		PopularScenarioType() : m_Index(0xFFFFFFFF), m_TimesPresent(0) {}

		//Qsort callbacks ...
		static int SortByCountCB(const PopularScenarioType* p_A, const PopularScenarioType* p_B);
		static int SortByTypeNameNoCaseCB(const PopularScenarioType* p_A, const PopularScenarioType* p_B);

		u32 m_Index;
		u32 m_TimesPresent;
	};
	 
	void GatherPopularUsedScenarioTypes(const atArray<CScenarioPoint*>& pointArray, atArray<PopularScenarioType>* out_sortedArray) const;
#endif // SCENARIO_DEBUG

	//
	// Members
	//
#if SCENARIO_DEBUG
	bkGroup*	m_ScenarioGroupsWidgetGroup;
	bool		m_ManifestNeedSave;
	int			m_ManifestOverrideIndex;
	atArray<const char*> m_manifestSources;
	atArray<bool>		 m_manifestSourceInfos;
	atBinaryMap<atHashString, atFinalHashString> m_regionSourceMap;
	atBinaryMap<atHashString, atHashString> m_groupSourceMap;
	atBinaryMap<atHashString, atHashString> m_interiorSourceMap;
public:
	bkCombo* AddWorkingManifestCombo(bkBank* bank);
	void	 OverrideAdded(){ m_ManifestOverrideIndex=m_manifestSources.GetCount()-1; }
	void	 WorkingManifestChanged();
	void	 PopulateSourceData(const char* source, CScenarioPointManifest& manifest);
	void	 HandleFileSources(const char* fileToLoad,bool isPso);
private:
	static int m_sWorkingManifestIndex;
#endif // SCENARIO_DEBUG


	CScenarioPointManifest m_Manifest;

	// PURPOSE:	Dense array of pointers to all CScenarioPointRegion objects currently streamed in.
	// NOTES:	Currently valid if m_CachedRegionPtrsValid is set.
	mutable atArray<CScenarioPointRegion*>	m_CachedActiveRegionPtrs;

	// PURPOSE:	Array of pointers to CScenarioPointRegion, parallel to m_RegionDefs.
	mutable atArray<CScenarioPointRegion*>	m_CachedRegionPtrs;

	// PURPOSE:	True if m_CachedActiveRegionPtrs and m_CachedRegionPtrs are currently up to date.
	mutable bool							m_CachedRegionPtrsValid;

	bool									m_dirtyStreamerTrees;

	// PURPOSE:	Parallel array to CScenarioPointRegion::m_Groups, keeping track
	//			of which scenario point groups are currently enabled.
	// NOTES:	By keeping this separate, the data we need at runtime when searching for
	//			scenarios is more compact in memory than if we just made this a member
	//			of CScenarioPointGroup. So, it's done like this intentionally for performance
	//			reasons, and also, it's nice to keep runtime data out of the classes
	//			we load using PARSER.
	atArray<bool>						m_GroupsEnabled;
	atArray<CScenarioPoint*>			m_EntityPoints;
	atArray<CScenarioPoint*>			m_WorldPoints;

	// PURPOSE: store the required imap hash data for lookup purposes
	atArray<u32> m_RequiredIMapHashes;
	// PURPOSE: store found imap index data so we dont have to keep looking it up
	atArray<u16> m_RequiredIMapStoreSlots;

	// PURPOSE:	Sorted array of the indices of active CScenarioPoints that have "extra data" associated with them.
	atArray<u16>						m_ExtraScenarioDataPoints;

	// PURPOSE:	Parallel array to m_ExtraScenarioDataPoints, storing pointers to the extra scenario point data objects.
	atArray<CScenarioPointExtraData*>	m_ExtraScenarioData;

	// PURPOSE:	Current number of scenario points attached to entities.
	int						m_NumScenarioPointsEntity;

	// PURPOSE:	Current number of loose scenario points placed in the world.
	int						m_NumScenarioPointsWorld;

	// PURPOSE:	If >=0, this is the only scenario group that should be considered enabled, all the others (incl. ungrouped points) act as if disabled.
	s16						m_ExclusivelyEnabledScenarioGroupIndex;

	// PURPOSE: spatial array for all non region points to help speed up searches.
	class CSpatialArrayNode_SP : public CSpatialArrayNode
	{
	public:
		CSpatialArrayNode_SP() : CSpatialArrayNode(), m_Point (NULL){}

		enum
		{
			kSpatialArrayTypeFlagExtendedRange = BIT0,
		};

		static CScenarioPoint* GetScenarioPointFromSpatialNode(CSpatialArrayNode &node) 
		{
			CSpatialArrayNode_SP* node_sp = (CSpatialArrayNode_SP*)&node;
			return node_sp->m_Point;
		}

		//This should only be used in the UpdateNonRegionPointSpatialArray function.
		// PROBABLY dont use this function anywhere else as CSpatialArray relies on
		// the m_Offs value to find nodes.
		void ResetOffs() { m_Offs = kOffsInvalid; }

		RegdScenarioPnt m_Point;
	};

	CSpatialArray* m_SpatialArray;
	atArray<CSpatialArrayNode_SP> m_SpatialNodeList;
	u32 m_LastNonRegionPointUpdate;
};

class CScenarioPointPriorityManager
{
public:
	static CScenarioPointPriorityManager& GetInstance() { return sm_Instance; }

	// Called by script to change the priority of a given scenario point group.
	void ForceScenarioPointGroupPriority(u16 iGroupIndex, bool bIsHighNow);
	
	// Check the scenario point's group against the forced group priorities.
	bool CheckPriority(const CScenarioPoint& rPoint);
	
	// Restore all scenario point groups to their original priority values.
	void RestoreGroupsToOriginalPriorities();

private:
	
	CScenarioPointPriorityManager() : m_aAlteredGroups(), m_aForcedGroupPriorities() {};
	~CScenarioPointPriorityManager() {};

	//Static member variables
	static const int kMaxAlteredPriorityPoints = 8;		//lowballing this for now
	static CScenarioPointPriorityManager sm_Instance;

	// Internal helper function to check a scenario point's priority value.
	bool CheckScenarioPointGroupPriority(const CScenarioPoint& rPoint) const;

	//Member variables
	// Parallel arrays for the altered scenario points and their new priorities.
	atFixedArray<u16, kMaxAlteredPriorityPoints> m_aAlteredGroups;
	atFixedArray<bool, kMaxAlteredPriorityPoints> m_aForcedGroupPriorities;
};

#endif // INC_SCENARIO_POINT_MANAGER_H
