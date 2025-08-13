// Title	:	InteriorProxy.h
// Author	:	John Whyte
// Started	:	30/1/2012

#ifndef _INTERIOR_PROXY_H_
#define _INTERIOR_PROXY_H_

// Framework headers
#include "fwscene/Ipl.h"
#include "fwtl/pool.h"
#include "fwutil/PtrList.h"
#include "fwscene/mapdata/mapdata.h"
#include "fwscene/world/EntityContainer.h"
#include "fwscene/world/InteriorSceneGraphNode.h"
#include "system/memops.h"

// Game headers
#include "scene/building.h"
#include "scene/FileLoader.h"
#include "control/replay/ReplaySettings.h"

namespace rage {
	class fwIsSphereIntersecting;

	struct SInteriorOrderData
	{
		SInteriorOrderData() { Reset();	}

		void Reset()
		{
			m_startFrom = -1;
			m_filePathHash = 0;
			m_proxies.ResetCount();
		}

		bool operator == (const SInteriorOrderData& rhs) const
		{
			return rhs.m_filePathHash == m_filePathHash;
		}

		s32 m_startFrom;
		u32 m_filePathHash;
		atArray<atHashString> m_proxies;

		PAR_SIMPLE_PARSABLE;
	};
}

#define MAX_ENTITY_SETS			(150)

#define PROXY_NAME_LENGTH		(32)

class CMloInstanceDef;
class CInteriorProxy;

class CInteriorProxyCacheEntry 
{
public:
	CInteriorProxyCacheEntry() {}
	CInteriorProxyCacheEntry(CInteriorProxy* pProxy);
	CInteriorProxyCacheEntry(const void* const pEntry) { sysMemCpy(this, pEntry, sizeof(CInteriorProxyCacheEntry)); }

	u32						m_groupId;
	u32						m_floorId;
	u32						m_numExitPortals;
	u32						m_proxyNameHash;

	u32						m_mapDataStoreNameHash;
	float					m_posX, m_posY, m_posZ;
	float					m_quatX, m_quatY, m_quatZ, m_quatW;
	float					m_bboxMinX, m_bboxMinY, m_bboxMinZ, m_bboxMaxX, m_bboxMaxY, m_bboxMaxZ;

	char					m_proxyName[PROXY_NAME_LENGTH];
};

class SEntitySet
{
public:
	SEntitySet(atHashString iName, int iLimit, int tintIdx)	: m_name(iName),	m_limit(iLimit),	m_tintIdx(tintIdx) {};
	SEntitySet(atHashString iName, int iLimit)					: m_name(iName),	m_limit(iLimit),	m_tintIdx(0) {};
	SEntitySet(atHashString iName)								: m_name(iName),	m_limit(MAX_ENTITY_SETS),	m_tintIdx(0) {};
	SEntitySet()												: m_name(),			m_limit(MAX_ENTITY_SETS),	m_tintIdx(0) {};

	bool operator==(const SEntitySet& rhs) const { return m_name==rhs.m_name && m_limit==rhs.m_limit && m_tintIdx==rhs.m_tintIdx; }
	bool operator!=(const SEntitySet& rhs) const { return !(*this == rhs); }

	atHashString m_name;
	int m_limit;
	int m_tintIdx;
};

class CInteriorProxy{
public:
	// State management
	//describe what instancing state the interior is currently in
	enum eProxy_State{
		PS_NONE					= 0,		// no objects in the interior are instanced
		PS_PARTIAL				= 1,		// the interior is partially instanced with portal attached objects and room 0 objects
		PS_FULL					= 2,		// the interior is fully instanced with all objects inside
		PS_FULL_WITH_COLLISIONS	= 3,		// the interior is fully instanced with all objects, all objects models are also loaded
		PS_NUM_STATES			= 4
	};

	// describe the cause of the current instance state of the interior
	enum eRequest_Module{
		RM_NONE					= 0,
		RM_VISIBLE				= 1,		// caused by being within the main frustum
		RM_PT_PROXIMITY			= 2,		// caused by activating tracker proximity to the interior
		RM_GROUP				= 3,		// caused by interior group policies
		RM_SCRIPT				= 4,		// caused by script requesting interior to be loaded
		RM_CUTSCENE				= 5,		// caused by the cutscene requesting the interior to be loaded
		RM_LOADSCENE			= 6,		// caused by a LoadScene call loading assets at a new warp location
		RM_SHORT_TUNNEL			= 7,		// caused by short tunnel activation group policy
		RM_NUM_MODULES			= 8
	};

	enum {
		STATE_COOLDOWN					= 15			// state cooldown transitions have a 15 frame between states
	};

public:
	FW_REGISTER_CLASS_POOL(CInteriorProxy);

	CInteriorProxy(void);
	virtual ~CInteriorProxy(void);

	static void RegisterMountInterface();

	static u32 ms_currExecutingCS;

	static CInteriorProxy* Add(CMloInstanceDef* pMLODef, strLocalIndex mapDataSlotIndex, spdAABB& bbox);
	static CInteriorProxy* Overwrite(CMloInstanceDef* pMLODef, strLocalIndex mapDataSlotIndex, spdAABB& bbox);

	static void AddProxySortOrder(const char* filePath);
	static void RemoveProxySortOrder(const char* filePath);

	static int SortProxiesByNameHash(const CInteriorProxy *a, const CInteriorProxy *b);
	static void	Sort();

	void	SetInteriorInst(CInteriorInst* pIntInst);
	CInteriorInst* GetInteriorInst(void) const { return(m_pIntInst); }

	static void				FindActiveIntersecting(const fwIsSphereIntersecting &testSphere, fwPtrListSingleLink&	scanList, bool activeOnly = true);
	static CInteriorProxy*	FindProxy(s32 mapDataDefIndex);
	static CInteriorProxy*	FindProxy(const u32 modelHash, const Vector3& vPositionToCheck);

	static void				AddAllDummyBounds(u32 changeSetSource);

	static u32 GetPoolUsage(void)						{ return(GetPool()->GetNoOfUsedSpaces());}

	static CInteriorProxy*		GetInteriorProxyFromCollisionData(CEntity* pIntersectionEntity, Vec3V const* pVec);
	
	void GetBoundSphere(spdSphere& sphere)		const	{ sphere = m_boundSphere; }
	void GetBoundBox(spdAABB& box)				const	{ box = m_boundBox; }
	void GetInitialBoundBox(spdAABB& box)		const	{ box = m_initialBoundBox; }
	void GetQuaternion(QuatV_InOut quat)		const	{ quat = m_quat; }
	void GetPosition(Vec3V_InOut pos)			const	{ pos = m_pos; }
	float GetHeading();		
	inline u32 GetNameHash()					const	{ return m_modelName.GetHash();}
	inline strStreamingObjectName	GetName()	const	{ return m_modelName; }
	u32 GetGroupId()							const	{ return m_groupId; }
	u32 GetFloorId()							const	{ return(0); }

#if !defined(_MSC_VER) || !defined(_M_X64)
	void SetNumExitPortals(u32 num)						{ m_numExitPortals = (u16) num; }
#else
	void SetNumExitPortals(u32 num)						{ m_numExitPortals = num; }
#endif

	u32	GetNumExitPortals(void)					const	{ return(m_numExitPortals);}

	float GetPopulationDistance(bool bScaled) const;

	static CInteriorProxy* GetFromProxyIndex(s32 idx);
	u32 GetInteriorProxyPoolIdx(void) 
	{
#if __BANK
		Assertf(m_IsSorted == true, "CInteriorProxy::GetInteriorProxyPoolIdx() called before sorting!");
#endif
		return(CInteriorProxy::GetPool()->GetJustIndex(this));
	}

	static u32 GetChecksum();
	static u32 GetCheckSumCount();
	static u32 GetCount(void);

#if !__NO_OUTPUT
	inline const char* GetModelName()			const	{ return m_modelName.TryGetCStr();}
	const char* GetContainerName() const;
#endif

	Mat34V_Out	GetMatrix()						const;

	void RequestContainingImapFile();
	void RequestStaticBoundsFile();
	void ReleaseStaticBoundsFile();

	strLocalIndex GetAudioOcclusionMetadataFileIndex()	const	{ return m_audioOcclusionMetadataFileIndex; }


	// retain list functionality
public:
	fwPtrListSingleLink&	GetRetainList(void) { return(m_retainList); }
	void					MoveToRetainList(CEntity* pEntity);		
	void					RemoveFromRetainList(CEntity* pEntity);		
	static bool				ShouldEntityFreezeAndRetain(CEntity *pEntity);
	void					ExtractRetainedEntities();	

	static void				CleanupRetainedEntity(CEntity* pEntity);
	// ---

	// --- fwInteriorLocation code
public:
	static CInteriorProxy*		GetFromLocation(const fwInteriorLocation& intLoc);
	static fwInteriorLocation	CreateLocation(const CInteriorProxy* pIntProxy, const s32 roomIndex);
	static void ValidateLocation(fwInteriorLocation loc);
	static u64 GenerateUniqueHashFromLocation(const fwInteriorLocation& intLoc);
	// ---

	// --- proxy state management ---
public:
	// setting the desired state of an interior & which module is making the state request
	void	SetRequestedState(eRequest_Module module, eProxy_State instState, bool doStateCooldown = false);
	u8		GetRequestedState(u32 module)					{ return m_statesRequestedByModules[module]; }
	void	CleanupInteriorImmediately(void);
	u32		GetCurrentState(void)					const	{ return(m_currentState); }
	u32		GetRequestingModule(void)				const	{ return(m_requestingModule); }
	void	UpdateImapPinState(void);
	void	SetRequiredByLoadScene(bool bRequired)			{ m_bRequiredByLoadScene = bRequired; }
	bool	IsRequiredByLoadScene() const					{ return m_bRequiredByLoadScene; }
	
	void	SkipShellAssetBlocking(bool val)				{ m_bSkipShellAssetBlocking = val; }
	bool	GetSkipShellAssetBlocking()						{ return(m_bSkipShellAssetBlocking); }

	static void ImmediateStateUpdate(fwPtrListSingleLink* pUpdateList = NULL);	// go through the list of interiors requiring state management & process accordingly
	static void SetIsDisabledDLC(strLocalIndex mapDataSlotIndex, bool disable);
	static bool GetIsDisabledForSlot(strLocalIndex mapDataSlotIndex);

	// high level control
	void SetIsDisabledDLC(bool disable);
	void	SetIsDisabled(bool val);
	void	SetIsCappedAtPartial(bool val);
	bool	GetIsDisabled(void)								{ return(m_bIsDisabled || m_bDisabledByCode); }
	bool	GetIsCappedAtPartial(void)						{ return(m_bIsCappedAtPartial); }
	void	SetIsDisabledByCode(bool val)					{ m_bDisabledByCode = val; }
	void SetChangeSetSource(u32 value) { m_sourceChangeSet = value; }
	u32 GetChangeSetSource() const { return m_sourceChangeSet; }

	bool	GetIsRegenLightsOnAdd(void)						{ return(m_bRegenLightsOnAdd); }

	static void	DisableMetro(bool val);							

	// --- entity sets ---
	void ActivateEntitySet(atHashString setName);
	void DeactivateEntitySet(atHashString setName);
	bool IsEntitySetActive(atHashString setName)			{ return(FindEntitySetByName(setName)!=-1); }

	int SetEntitySetLimit(atHashString setName, int iLimit)				{ int i = FindEntitySetByName(setName); if(i != -1) return m_activeEntitySets[i].m_limit = iLimit; return -1;}
	int GetEntitySetLimit(atHashString setName)	const					{ int i = FindEntitySetByName(setName); if(i != -1) return m_activeEntitySets[i].m_limit; return -1; }
	int SetEntitySetTintIndex(atHashString setName, UInt32 tintIdx)		{ int i = FindEntitySetByName(setName); if(i != -1) return m_activeEntitySets[i].m_tintIdx = tintIdx; return -1; }
	int GetEntitySetTintIndex(atHashString setName)	const				{ int i = FindEntitySetByName(setName); if(i != -1) return m_activeEntitySets[i].m_tintIdx; return -1; }

	int FindEntitySetByName(atHashString setName) const;
	void DeleteEntitySet(atHashString setName);
	void RefreshInterior();

	void ValidateActiveSets();

	strLocalIndex	GetMapDataSlotIndex()						const	{ return(m_mapDataSlotIndex); }
	strLocalIndex GetStaticBoundsSlotIndex()							{ return(m_staticBoundsSlotIndex); }
	bool HasStaticBounds()									{ return(m_staticBoundsSlotIndex != -1); }

	bool AreStaticCollisionsLoaded();				

	bool IsContainingImapActive() const;

	//////////////////////////////////////////////////////////////////////////
	// CACHELOADER related
public:
	static void InitCacheLoaderModule();
	static bool ReadFromCacheFile(const void* const pLine, fiStream* pDebugTextFileToWriteTo);
	static void AddToCacheFile(fiStream* pDebugTextFileToWriteTo);

#if !__FINAL
	static void EnableAndUncapAllInteriors();
	static void DumpInteriorProxyInfo();
#endif

#if __BANK
	static void AddWidgets(bkBank& bank);
	static void DumpPositionsAndOrientation();
	static const atArray<atHashString>& GetUnorderedProxies() { return m_unorderedProxies; }
	static void AddUnorderedProxy(atHashString value) { if (m_unorderedProxies.Find(value) == -1) { m_unorderedProxies.PushAndGrow(value); } }
	static void RemoveUnorderedProxy(atHashString value) { m_unorderedProxies.DeleteMatches(value); }
#endif	//__BANK

#if __BANK
	bool GetIsDisabledByDLC() const { return m_bIsDisabled && m_disabledByDLC; }
#endif

#if !__FINAL
#endif

#if GTA_REPLAY
	atArray<SEntitySet>	&GetEntitySets()	{ return m_activeEntitySets; }
#endif	//GTA_REPLAY

private:
	void SetCurrentState(eProxy_State instState) { m_currentState = instState; }
	void ResetStateArray(void) { for(u32 i=0;i< RM_NUM_MODULES; i++) m_statesRequestedByModules[i] = (1<< PS_NONE);}

	void GetMoverBoundsName(char* boundsName, u32 index);
	void GetWeaponBoundsName(char* boundsName, u32 index);
	void DisableAllCollisions(bool bVal);

	strLocalIndex			m_mapDataSlotIndex;			// source .imap index 
	strLocalIndex			m_staticBoundsSlotIndex;	// .bvh for this interior instance

	strLocalIndex			m_audioOcclusionMetadataFileIndex;
	u16						m_numExitPortals				: 9;
	u16						m_bIsPinned						: 1;
	u16						m_bRequiredByLoadScene			: 1;
	u16						m_bIsStaticBoundsReffed			: 1;
	u16						m_bSkipShellAssetBlocking		: 1;

	u16						m_bIsDisabled					: 1;
	u16						m_bIsCappedAtPartial			: 1;

	u16						m_bDisabledByCode				: 1;

	u8					m_statesRequestedByModules[RM_NUM_MODULES];

	u32					m_currentState		: 8;
	u32					m_stateCooldown		: 8;
	u32					m_requestingModule	: 8;
	u32					m_groupId			: 8;

	u32					m_bRegenLightsOnAdd	: 1;
	u32					m_Pad				: 31;

	CInteriorInst*			m_pIntInst;					// ptr to the transient interior instance object
	fwPtrListSingleLink		m_retainList;				// retain list required for this interior	
	u32 m_sourceChangeSet;

	QuatV					m_quat;
	Vec3V					m_pos;						// <- this whole class is packed nicely! There is 4 bytes in the .w here though!
	spdAABB					m_initialBoundBox;
	spdAABB					m_boundBox;
	spdSphere				m_boundSphere;

	atArray<SEntitySet>		m_activeEntitySets;
	atHashString			m_weaponBoundsHash;
	strStreamingObjectName	m_modelName;
	static atArray<SInteriorOrderData> m_orderData;

	static fwPtrListSingleLink ms_immediateStateUpdateList;		// list of interior proxies undergoing state transitions		
#if __BANK
	static	bool			m_IsSorted;			// A catchall for accesses before the pool is sorted.
	static atArray<atHashString> m_unorderedProxies;
#endif	// __BANK

#if __BANK
	bool m_disabledByDLC;
#endif

	void					InitFromMLOInstanceDef(CMloInstanceDef* pMLODef, s32 mapDataSlotIndex, spdAABB& bbox);

	void					SetMapDataSlotIndex(s32 idx) { m_mapDataSlotIndex = static_cast<s16>(idx); }
	void					SetStaticBoundsSlotIndex(s32 idx) { m_staticBoundsSlotIndex = static_cast<s16>(idx); }
};

#endif //_INTERIOR_PROXY_H_
