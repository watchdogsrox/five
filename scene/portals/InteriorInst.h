// Title	:	InteriorInst.h
// Author	:	John Whyte
// Started	:	19/10/2005

#ifndef _INTERIOR_INST_H_
#define _INTERIOR_INST_H_

// Framework headers
#include "fwscene/Ipl.h"
#include "fwtl/pool.h"
#include "fwutil/PtrList.h"
#include "fwscene/mapdata/mapdata.h"
#include "fwscene/world/EntityContainer.h"
#include "fwscene/world/InteriorSceneGraphNode.h"
#include "fwscene/world/InteriorLocation.h"

// Game headers
#include "renderer/RenderPhases/RenderPhase.h"
#include "scene/building.h"
#include "scene/FileLoader.h"
#include "scene/portals/interiorProxy.h"
#include "script/script_memory_tracking.h"
#include "streaming/streamingrequest.h"


#define MAX_ROOMS_PER_INTERIOR		(32)
#define MAX_SMOKE_LEVEL_SECTIONS	(6)

#define MAX_SHELL_ENTITIES			(64)

class CPortalInst;
class CPortalTracker;
class CPortalVisTracker;
class CDynamicEntity;
class CMloInstanceDef;

namespace rage {
class fwIsSphereIntersecting;
}

enum II_Lod_Levels{
	IILL_DETAIL				= 0,		// rendering detail objects (inside interior or through a nearby portal)
	IILL_LOD1				= 1,		// rendering MLO as LOD1 (not rendering through any portal)
	IILL_LOD2				= 2,		// rendering MLO as LOD2 (not close enough to render LOD1)
	IILL_NONE				= 3			// rendering nothing (not close enough to render LOD2)
};



class CRoomData
{
private:

	fwRoomSceneGraphNode*		m_roomSceneNode;

public:

	fwPtrListSingleLink	m_activeTrackerList;	//list of trackers currently active in this room

	atArray<int>		m_portalIndices;

	int					m_timeCycleIndex;
	int					m_timeCycleIndexSecondary;

	float				m_smokeGridSize;
	float				m_smokeLevel[MAX_SMOKE_LEVEL_SECTIONS][MAX_SMOKE_LEVEL_SECTIONS];

	float				m_localWaterLevel;

	spdAABB				m_boundingBox;

	bool				m_bHasLocalWaterLevel;

	void SetRoomSceneNode(fwRoomSceneGraphNode* roomSceneNode)	{ m_roomSceneNode = roomSceneNode; }
	fwRoomSceneGraphNode* GetRoomSceneNode() const				{ return m_roomSceneNode; }
};

class CMegaMLOLink
{
public:
	CInteriorInst*	m_pTargetIntInst;
	s32				m_TargetPortalIdx;	
};

#define NUM_RETAINED_ENTITIES (16)

// a retained entity cannot be removed from interior during depopulation - must be kept until repopulation or shutdown
struct sRetainedEntity
{
	CDynamicEntity*			m_pEntity;
};

//
// name:		CInteriorInst
// description:	instance data for a building interior
class CInteriorInst : public CBuilding
{
	friend class CInteriorProxy;
	friend class CLayoutManager;

public:
	FW_REGISTER_CLASS_POOL(CInteriorInst);

	CInteriorInst(const eEntityOwnedBy ownedBy);
	~CInteriorInst();
	void Update();

	inline CMloModelInfo* GetMloModelInfo() const {return (CMloModelInfo*)GetBaseModelInfo();}

	void AddToInterior(fwInteriorLocation)	{ Assertf( false, "An interior cannot be added to an interior." ); }
	void RemoveFromInterior()				{ Assertf( false, "An interior cannot be removed from an interior." ); }

	static void ShutdownLevel(void);

	bool SetInstanceTransform(Vector3& trans, Quaternion& orien);
	void GenerateInstanceData();		// create from streaming code path
	void ShutdownInstanceData();

	static void CheckInteriorIntegrity(u32 modelIdx);
	void CheckForCollisions(void);

	// for adding / removing various game objects from interior
	void AppendObjectInstance(CEntity* pEntity, fwInteriorLocation interiorLocation);
	void RemoveObjectInstance(CEntity* pEntity);
	void UpdateObjectInstance(CEntity* pEntity);
	void AddObjectInstance(CDynamicEntity* pEntity, fwInteriorLocation interiorLocation);
	void RemoveObjectInstance(CDynamicEntity* pObj);	

	virtual Vector3	GetBoundCentre() const;
	virtual void GetBoundCentre(Vector3& centre) const;	// quicker version
	virtual float GetBoundCentreAndRadius(Vector3& centre) const; // get both in only 1 virtual function and help to simplify SPU code
	virtual void GetBoundSphere(spdSphere& sphere) const;
	virtual float GetBoundRadius() const;
	virtual const Vector3& GetBoundingBoxMin() const;
	virtual const Vector3& GetBoundingBoxMax() const;
	virtual FASTRETURNCHECK(const spdAABB &) GetBoundBox(spdAABB& box) const;

	virtual void PatchUpLODDistances(fwEntity* pLOD);

	void	CalcTransformedBounds(void);

	fwPortalCorners& GetPortal(s32 idx) { Assert(idx < m_aPortals.GetCount()); return(m_aPortals[idx]->GetCorners());}
	const fwPortalCorners& GetPortal(s32 idx) const { Assert(idx < m_aPortals.GetCount()); return(m_aPortals[idx]->GetCorners());}

	bool	IsSubwayMLO(void) const;			// special flag for subway bits
	bool	IsOfficeMLO(void)  const;			// special flag for offices
	bool	IsAllowRunMLO(void)  const;			// player is allowed to run in this MLO
	bool	IsNoWaterReflection(void)  const;	// should water reflection render phase be disabled in this MLO

	bool	IsPopulated(void) const { return(m_bIsPopulated); }
	bool	ArePortalsCreated(void) const { return(m_bArePortalsCreated); }

	u32	GetNumRooms() const;
	u32	GetNumPortals() const;

	u32	GetNumObjInRoom(u32 roomIdx) const;
	s32 GetFloorIdInRoom(u32 roomIdx) const;
	u32	GetNumObjInPortal(u32 portalIdx) const;
	u32	GetNumExitPortals(void) const {return(m_numExitPortals);}
	u32	GetNumExitPortalsInRoom(u32 roomIdx) const;
	u32	GetNumPortalsInRoom(u32 roomIdx) const;

	bool GetIsShuttingDown() const { return(m_bIsShuttingDown); }

	void GetRoomBoundingBox(const u32 roomIdx, spdAABB& boundingBox) const	{ boundingBox = m_roomData[ roomIdx ].m_boundingBox; }
	fwInteriorSceneGraphNode* GetInteriorSceneGraphNode()					{ return m_interiorSceneNode; }
	fwRoomSceneGraphNode* GetRoomSceneGraphNode(const u32 roomIdx) const	{ return m_roomData[ roomIdx ].GetRoomSceneNode(); }
	fwPortalSceneGraphNode* GetPortalSceneGraphNode(const u32 portalIdx)	{ return m_aPortals[ portalIdx ]; }
	const fwPortalSceneGraphNode* GetPortalSceneGraphNode(const u32 portalIdx) const	{ return m_aPortals[ portalIdx ]; }
	
	CEntity*	GetObjInstancePtr(u32 roomIdx, u32 objIdx) const;
	CEntity*	GetPortalObjInstancePtr(s32 roomIdx, s32 portalIdx, u32 slotIdx) const;
	
	s32		GetPortalIdxInRoom(u32 roomIdx, u32 portalIdx) const;

	void	GetPortalRoomIds(u32 roomIdx, u32 portalIdx, u32 &room1Idx, u32 &room2Idx) const;
	bool	IsLinkPortal(u32 roomIdx, u32 portalIdx) const;
	bool	IsMirrorPortal(u32 roomIdx, u32 portalIdx) const;
	
	bool	CanReceiveObjects(void) const { return(m_bIsShellPopulated); }		// only receptive to objects once populated

	s32	TestPointAgainstPortal(s32 portalIdx, Vector3::Vector3Param pos) const;

	const char*		GetRoomName(u32 roomIdx) const;
	u32		GetRoomHashcode(u32 roomIdx) const;
	s32		FindRoomByHashcode(u32 hashcode) const;

	s32		GetDestThruPortalInRoom(u32 roomIdx, u32 portalIdx) const;
	void	GetPortalInRoom(u32 roomIdx, u32 portalIdx, fwPortalCorners& portalCorners) const;

	void	CalcRoomBoundBox(u32 roomIdx, spdAABB& bbox) const;

	void	RemovePortalInstanceRef(CPortalInst* pPortalInst);

	void	AddToActiveTrackerList(CPortalTracker* pT);
	void	RemoveFromActiveTrackerList(CPortalTracker *pT);

	void	AppendActiveObjects(atArray<CEntity*>& entityArray);
	void	AppendPortalActiveObjects(atArray<CEntity*>& entityArray);

	void	AppendObjectsByType(fwPtrListSingleLink* pMloExtractedObjectList, s32 typeFlags);
	template<class _SearchFn>bool ProcessObjectsByType(const _SearchFn& fn, IntersectingCB cb, void* data, s32 typeFlags);

	void	RenderAllTrackedObjects(s32 list,CRenderPhase *pRenderPhase);     
	fwPtrList& GetTrackedObjects(s32 roomIdx) { return(m_roomData[roomIdx].m_activeTrackerList);}
	
	void	RestartEntityInInteriorList(void) { m_currEntListRoom = 0; m_currEntListIdx = -1;}
	bool	GetNextEntityInInteriorList(CEntity**);		// get the next entity in the list

	void	AppendRoomContents(u32 roomIdx, atArray<fwEntity*> *pEntityArray);
	void	RequestObjects(u32 reqFlag, bool bLoadAll);
	void	RequestRoom(u32 roomId, u32 streamingFlags);
	void	ForceFadeIn(void) {m_bTriggerInstantFadeUp = true;  m_currDetailLevel = IILL_DETAIL;}
	void	CutsceneForceFadeIn(bool bSet) {m_bCutsceneFadeUp = bSet; } 

	bool	SetAlphaForRoomZeroModels(u16 alpha);
	void	CapAlphaForContentsEntities();

	void	RequestCollisionMesh(void);

	CPortalInst*	GetMatchingPortalInst(s32 roomIdx, s32 portalIdx) const;	// return the portal inst matching this portal

	static bool	IsValidInteriorIdx(s32 intIdx) {return(CInteriorInst::GetPool()->GetSlot(intIdx) != NULL);}
	u32		GetInteriorPoolIdx(void) const {return(CInteriorInst::GetPool()->GetJustIndex(this));}

	static CInteriorInst* GetInstancedInteriorAtCoords(const Vector3& testPos, u32 posHash = 0, float fTestRadius=0.1f);

	float	GetDaylightFillScale(void) const;

	bool	IsWaterInside(void) const {return(m_bContainsWater);}

	u32		GeneratePositionHash(void) const;

	inline void	GetTimeCycleIndicesForRoom(u32 roomIdx, int& timeCycleIndex, int& secondaryTimeCycleIndex) const
	{
		timeCycleIndex = m_roomData[roomIdx].m_timeCycleIndex;
		secondaryTimeCycleIndex = m_roomData[roomIdx].m_timeCycleIndexSecondary;
	}
	
	// interior smoke effects
	bool	GetSmokeLevelGridIds(s32& gridX, s32& gridY, s32 roomIdx, const Vector3& wldPos) const;
	void	AddSmokeToRoom(s32 roomIdx, const Vector3& wldPos, bool isExplosion=false);
	float	GetRoomSmokeLevel(s32 roomIdx, const Vector3& wldPos, const Vector3& wldFwd) const;

	bool	GetLocalWaterLevel(s32 roomIdx, float& localWaterLevel) const;

	bool	TestPortalState(bool bIsClosed, bool bIsLocked, s32 globalPortalIdx) const;			// check state of doors for this portal
	bool	TestPortalState(bool bIsClosed, bool bIsLocked, s32 roomIdx, s32 portalIdx) const;

#if __BANK
	void	GetLODState(char* pString);

	u32		GetRetainListCount(void) const { return(m_pProxy->GetRetainList().CountElements()); }
#endif //__BANK

#if __SCRIPT_MEM_CALC
	void	AddInteriorContents(s32 roomIdx, bool bAllRooms, bool bPortalAttached, atArray<u32>& modelIdxArray);
	void	AddRoomContents(u32 roomIdx, atArray<u32>& modelIdxArray);
#endif	//	__SCRIPT_MEM_CALC

	bool	m_bInUse     : 1;		// interior contains some active game objects - don't convert to dummy!
	bool	m_bIsDummied : 1;		// contents have been dummified

	bool	m_bIsPopulated			 : 1; // has been filled with the objects specified in the .ide file?
	bool	m_bArePortalsCreated	 : 1;
	bool	m_bInstanceDataGenerated : 1;

	bool	m_bResetObjFadeBecauseOfLowLOD	: 1;
	bool	m_bTriggerInstantFadeUp			: 1;
	bool	m_bCutsceneFadeUp				: 1;
	bool	m_bForceLowLODOn				: 1;	
	bool	m_bNeedsFullTransform			: 1;

	bool	m_bIsShellPopulated				: 1;
	bool	m_bIsContentsPopulated			: 1;
	bool    m_bAddedToClosedInteriorList	: 1;
	bool    m_bInterfacePortalInstsCreated	: 1;
	bool    m_bIsLayoutPopulated			: 1;
	bool	m_bIgnoreInstancePriority		: 1;

	u16		m_bIsAddedToLodProcessListForPhase;
	s16     m_openPortalsCount;

	fwIplInstance*	AddAppendObject();

	void	PortalObjectsConvertToReal(void);
	void	PortalObjectsConvertToDummy(void);
	void	PortalObjectsDestroy(void);
	void    IncrementmOpenPortalsCount(s32 interiorProxyIndex);
	void    DecrementmOpenPortalsCount(s32 interiorProxyIndex);

	void	CapturePedsAndVehicles(void);

	bool	GetIsDoNotRenderDetail(void) const { return(m_bDoNotRenderDetail);}

	static	void FindIntersectingMLOs(const fwIsSphereIntersecting &testSphere, fwPtrListSingleLink&	scanList);

	static void HoldRoomZeroModels(CInteriorInst* pIntInst);

	// ---  new state code  ----

private:

	// new state functions
	bool	PopulateInteriorShell(void);
	bool	PopulateInteriorContents(void);
	bool	DepopulateInterior(void);

	bool	HasShellLoaded(void);

	void	CreateEntity(fwEntityDef* pEntityDef, fwInteriorLocation interiorLocation, bool bIntersectsModelSwap, bool bAddToShellRequests, u8 tintIdx);
	void	CreateInteriorEntities(CMloModelInfo* pMI, bool bSelectShell, bool bIntersectsModelSwap);

	void AddRoomZeroModels(strRequestArray<INTLOC_MAX_ENTITIES_IN_ROOM_0>& RoomZeroRequestArray);

	// --- code to access retain list for this instance
	fwPtrListSingleLink&		GetRetainList(void)		{ return(m_pProxy->GetRetainList()); }

public:
	void	DeleteDetailDrawables(void);


	void SetGroupId(const u32 groupId)		{ m_groupId = groupId; }
	u32 GetGroupId() const					{ return m_groupId; }
	void SetFloorId(const s16 floorId)		{ m_floorId = floorId; }
	s16 GetFloorId() const					{ return m_floorId; }

	void SetMLOFlags(const u32 flags)		{ m_MLOFlags = flags; }
	u32  GetMLOFlags() const				{ return m_MLOFlags; }

	const atArray<CPortalInst*>& GetRoomZeroPortalInsts() const { return m_aRoomZeroPortalInsts; }

	static CPortalInst*		ms_pLinkingPortal;	// stores result of the search CB for a linking portal

	void SetMLODistance(float dist) { m_MLODist = dist; }

	void SetClosestExternPortalDist(float dist) { m_closestExternPortalDist = dist; }

	static bool RequestInteriorAtLocation(Vector3& pos, CInteriorProxy* &foundInteriorProxy, s32& probedRoomIdx);

	void SetProxy(CInteriorProxy* pProxy) { m_pProxy = pProxy; }
	CInteriorProxy* GetProxy(void) const { FastAssert(m_pProxy); return(m_pProxy); }		// an interior should _always_ have a proxy

	const CEntity* GetInternalLOD() { return(m_pInternalLOD); }
	void SuppressInternalLOD();
	void SuppressExternalLOD();


private:

	void MergePortalCorners(s32 srcPortalIdx, CPortalInst* pPortalInst);
	CPortalInst* FindExistingLinkPortal(u32 portalIdx);
	
	CEntity* GetCurrEntityFromPTNode(fwPtrNode* pNode);

	void InitializeSubSceneGraph();
	void ShutdownSubSceneGraph();
	void ProcessCloseablePortals();
	
	void PopulateMloShell();
	void PopulateMloContents();

	void CreateInterfacePortalInsts();

	void SetAudioInteriorData();

	void SelectInternalModels(bool bShowDetail);

	atArray<fwPortalSceneGraphNode*>	m_aPortals;	// the instanced portals in world co-ordinates & attached objects

	// bounding sphere info
	// [SPHERE-OPTIMISE] should be spdSphere
	float				m_boundSphereRadius;
	Vector3				m_boundSphereCentre;

	// bounding box info
	spdAABB				m_boundBox;

	// capture flag - when the interior instances, it will attempt to capture any peds & vehicles in the world which should be in the interior
	bool				m_bHasCapturedFlag;

	// for all entities in every room, need ptrs to the entities to toggle rendering
	atArray<CRoomData>		m_roomData;
	atArray<CPortalInst*>	m_aRoomZeroPortalInsts;
	
	// for accessing entities in interiors as a list of entities
	s32		m_currEntListRoom;
	s32		m_currEntListIdx;

	II_Lod_Levels	m_currDetailLevel;
	CEntity*		m_pInternalLOD;

	float			m_closestLinkedPortalDist;
	float			m_closestExternPortalDist;
	float			m_MLODist;
	u32				m_numExitPortals;	

	u32			m_bDoNotRenderLowLODs	: 1;
	u32			m_bDoNotRenderDetail	: 1;
	u32			m_bContainsWater		: 1;
	u32			m_bIsShuttingDown		: 1;

	fwInteriorSceneGraphNode*	m_interiorSceneNode;

	u32			m_groupId;
	s16			m_floorId;
	
	static	CInteriorInst*	ms_pLockedForDeletion;		// interior which is locked for deletion

	CInteriorProxy*		m_pProxy;

	u32			m_MLOFlags;

	strRequestArray<MAX_SHELL_ENTITIES>*						m_pShellRequests;

	static strRequestArray<INTLOC_MAX_ENTITIES_IN_ROOM_0>		ms_roomZeroRequests;
	static CInteriorInst*										ms_pHoldRoomZeroInterior;

	// --- interior location utility functions ---

public:
	static fwInteriorLocation CreateLocation(const CInteriorInst* interiorInst, const s32 roomIndex);
	static CInteriorInst* GetInteriorForLocation(const fwInteriorLocation& intLoc);

	PPUVIRTUAL void InitEntityFromDefinition(fwEntityDef* definition, fwArchetype* archetype, s32 mapDataDefIndex);

};

inline float	CInteriorInst::GetBoundRadius() const
{
	return m_boundSphereRadius;
}

#endif
