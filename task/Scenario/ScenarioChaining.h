//
// task/Scenario/ScenarioChaining.h
//
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#ifndef TASK_SCENARIO_SCENARIO_CHAINING_H
#define TASK_SCENARIO_SCENARIO_CHAINING_H

#include "atl/array.h"
#include "atl/hashstring.h"
#include "atl/ptr.h"
#include "parser/macros.h"
#include "task/Scenario/ScenarioPoint.h"	// RegdScenarioPnt
#include "vectormath/classes.h"
#include "vector/colors.h"
#include "ai/aichannel.h"

class C2dEffect;
class CEntity;
class CScenarioPoint;
class CScenarioPointRegion;
struct CSpawnPoint;

//-----------------------------------------------------------------------------

class CScenarioChainingNode
{
public:
	CScenarioChainingNode();

#if __BANK
	void UpdateFromPoint(const CScenarioPoint& point);
#endif

	Vec3V			m_Position;

	// PURPOSE:	If attached, this is the hash of the entity model name, otherwise it's zero.
	atHashString	m_AttachedToEntityType;

	atHashString	m_ScenarioType;

	// PURPOSE:	True if this node has any incoming edges.
	// NOTES:	Computed by CScenarioChainingGraph::UpdateNodesHaveEdges().
	bool			m_HasIncomingEdges;

	// PURPOSE:	True if this node has any outgoing edges.
	// NOTES:	Computed by CScenarioChainingGraph::UpdateNodesHaveEdges().
	bool			m_HasOutgoingEdges;

	PAR_SIMPLE_PARSABLE;
};

//-----------------------------------------------------------------------------

class CScenarioChainingEdge
{
public:
	enum eAction
	{
		// Please keep up to date with sm_ActionNames.

		kMove,							// Move along the link.
		kMoveIntoVehicleAsPassenger,	// Move along the link leading to a parked vehicle, and enter it as passenger.
		kMoveFollowMaster,				// Move along the link, setting your speed to be the speed of your master.

		kNumActions,					// Leave last, except the one below...
		kActionDefault = kMove
	};

	enum eNavMode
	{
		// Please keep up to date with string  CScenarioEditor::UpdateEditingPointWidgets().

		kDirect,		// Go on a straight line without using navmesh.
		kNavMesh,		// Use navmesh data.
		kRoads,			// Use road network when possible (driving scenarios only).

		kNumNavModes,	// Leave last, except the one below...
		kNavModeDefault = kNavMesh
	};

#if __BANK
	static Color32 GetNavModeColor(eNavMode _mode)
	{
		switch (_mode)
		{
		case kDirect: return Color_yellow;
		case kNavMesh: return Color_blue;
		case kRoads: return Color_green;
		default:
			aiAssertf(false, "Unknown CScenarioChainingEdge::eNavMode %d", _mode);
			break;
		};

		return Color_pink;
	}

	static const char* sm_ActionNames[];
#endif	// __BANK

	enum eNavSpeed
	{
		kSpd5mph,
		kSpd10mph,
		kSpd15mph,
		kSpd25mph,
		kSpd35mph,
		kSpd45mph,
		kSpd55mph,
		kSpd65mph,
		kSpd80mph,
		kSpd100mph,
		kSpd125mph,
		kSpd150mph,
		kSpd200mph,
		kSpdWalk,
		kSpdRun,
		kSpdSprint,

		kNumNavSpeeds,
		kNavSpeedDefault = kSpd15mph
	};

	CScenarioChainingEdge();

	int GetNodeIndexFrom() const		{ return m_NodeIndexFrom; }
	int GetNodeIndexTo() const			{ return m_NodeIndexTo; }
	eAction GetAction() const			{ return (eAction)m_Action; }
	eNavMode GetNavMode() const			{ return (eNavMode)m_NavMode; }
	eNavSpeed GetNavSpeed() const		{ return (eNavSpeed)m_NavSpeed; }

#if __BANK
	void CopyEdgeDataFrom(const CScenarioChainingEdge& src);//This used to copy extra data from one edge to another (ie m_NavMode)
	void CopyFull(const CScenarioChainingEdge& src);		//This used to copy all the data from one edge to another

	void SetNodeIndexFrom(int index)	{ Assert(index >= 0 && index <= 0xffff); m_NodeIndexFrom = (u16)index; }
	void SetNodeIndexTo(int index)		{ Assert(index >= 0 && index <= 0xffff); m_NodeIndexTo = (u16)index; }
	void SetAction(eAction act)			{ Assign(m_Action,(int)act); }
	void SetNavMode(eNavMode mode)		{ Assign(m_NavMode,(int)mode); }
	void SetNavSpeed(eNavSpeed spd)		{ Assign(m_NavSpeed,(int)spd); }
#endif	// __BANK

protected:
	u16				m_NodeIndexFrom;
	u16				m_NodeIndexTo;

	u8 /*eAction*/	m_Action;
	u8 /*eNavMode*/	m_NavMode;
	u8 /*eNavSpeed*/m_NavSpeed;

	PAR_SIMPLE_PARSABLE;
};

//-----------------------------------------------------------------------------

class CScenarioPointChainUseInfo 
{
public:
	CScenarioPointChainUseInfo()
		: m_RegionId(-1)
		, m_ChainId(-1)
		, m_NodeIdx(-1)
		, m_UnstreamedRegionId(-1)
		, m_UnstreamedChainId(-1)
		, m_UnstreamedNodeIdx(-1)
		, m_IsDummy(false)
		, m_Ped(NULL)
		, m_Vehicle(NULL)
		, m_RefCount(0)
		, m_NumRefsInSpawnHistory(0)
	{
	}
	  

	~CScenarioPointChainUseInfo()
	{
		Assertf(!m_RefCount, "Something still has a reference to this user data (R) %d, (C) %d, (N) %d 0x%p (R) %d", m_RegionId, m_ChainId, m_NodeIdx, this, m_RefCount);
		Assert(!m_NumRefsInSpawnHistory);
	}
	
	FW_REGISTER_CLASS_POOL(CScenarioPointChainUseInfo);

	bool IsIndexInfoValid() const { return (m_RegionId != -1 && m_ChainId != -1 && m_NodeIdx != -1)? true : false;}

	void SetPedAndVehicle(CPed* pPed, CVehicle* pVehicle)
	{
		Assert(!m_Ped && !m_Vehicle);
		m_Ped = pPed;
		m_Vehicle = pVehicle;
		Validate();
	}

	void ResetPed() { m_Ped = NULL; Validate(); }
	void ResetVehicle() { m_Vehicle = NULL; Validate(); }

	void AddRefInSpawnHistory() { m_NumRefsInSpawnHistory++; Assert(m_NumRefsInSpawnHistory); }
	void RemoveRefInSpawnHistory() { Assert(m_NumRefsInSpawnHistory > 0); m_NumRefsInSpawnHistory--; }
	int GetNumRefsInSpawnHistory() const { return m_NumRefsInSpawnHistory; }

	int GetUnstreamedRegionId() const { return m_UnstreamedRegionId; }
	bool IsInUnstreamedRegion() const { return m_UnstreamedRegionId >= 0; }

	void RestoreIndexInfoFromUnstreamed()
	{
		m_RegionId = m_UnstreamedRegionId;
		m_ChainId = m_UnstreamedChainId;
		m_NodeIdx = m_UnstreamedNodeIdx;
		m_UnstreamedRegionId = m_UnstreamedChainId = m_UnstreamedNodeIdx = -1;
	}

	int GetRegionIndex() const { return m_RegionId; }
	int GetChainIndex() const { return m_ChainId; }

	CPed * GetPed() const { return m_Ped.Get(); }
	CVehicle* GetVehicle() const { return m_Vehicle.Get(); }

	bool IsDummy() const { return m_IsDummy; }

	void Validate()
	{
#if __ASSERT
		if (!m_Ped.Get() && !m_Vehicle.Get() && !m_IsDummy && !m_NumRefsInSpawnHistory)
		{
			Assertf(m_RefCount <= 0, "A CScenarioPointChainUseInfo object now fulfills the conditions to become deleted, but still has %d references. (R) %d, (C) %d, (N) %d 0x%p", m_RefCount, m_RegionId, m_ChainId, m_NodeIdx, this);
		}
#endif	// __ASSERT
	}

private:
	friend class CScenarioChainingGraph;//For assignment/usage ... 
	friend class CScenarioPointRegion;//For assignment
	friend class CScenarioChain;//For debug display
	friend class atPtr<CScenarioPointChainUseInfo>; //for refcount access

	// Intentionally unimplemented to catch if we accidentally copy these somewhere.
	CScenarioPointChainUseInfo(const CScenarioPointChainUseInfo&);
	const CScenarioPointChainUseInfo& operator=(const CScenarioPointChainUseInfo&);

	void AddRef() const { Assert(m_RefCount < 10); m_RefCount++; }
	void Release() const { Assert(m_RefCount > 0); m_RefCount--; }

	//Used to Reset stuff when we are unregistering a user ... CScenarioChainingGraph::UnregisterPointChainUser
	void ResetIndexInfo(bool permanent) 
	{ 
		if(!permanent)
		{
			m_UnstreamedRegionId = m_RegionId;
			m_UnstreamedChainId = m_ChainId;
			m_UnstreamedNodeIdx = m_NodeIdx;
		}
		else
		{
			m_UnstreamedRegionId = m_UnstreamedChainId = m_UnstreamedNodeIdx = -1;
		}

		m_RegionId = -1;
		m_ChainId = -1;
		m_NodeIdx = -1;

		Validate(); 
	}

	RegdPed m_Ped;
	RegdVeh m_Vehicle;
	s16 m_RegionId;
	s16 m_ChainId;
	s16 m_NodeIdx;
	mutable u16 m_RefCount;

	s16 m_UnstreamedRegionId;
	s16	m_UnstreamedChainId;
	s16	m_UnstreamedNodeIdx;

	// PURPOSE:	When a chaining user info is referenced from the scenario spawn history, this gets increased,
	//			to prevent the user info from being deleted, thus keeping the chain considered in use and
	//			preventing respawning at other nodes in the chain.
	u8 m_NumRefsInSpawnHistory;

	bool m_IsDummy;
};

typedef	atPtr<CScenarioPointChainUseInfo> CScenarioPointChainUseInfoPtr;

//-----------------------------------------------------------------------------

class CScenarioChain
{
public:
	CScenarioChain()
		: m_MaxUsers(1)
	{
	}

	//Users
	u8 GetMaxUsers() const { return m_MaxUsers; }
	int GetNumUsers() const { return m_Users.GetCount(); }
	void AddUser(CScenarioPointChainUseInfo* user, bool allowOverflow = false);
	void RemoveUser(CScenarioPointChainUseInfo* user);
	void ResetUsers(bool permanentlyRemoveUsers = true, bool reclaimMemory = false);
	bool IsFull() const;
	const atArray<CScenarioPointChainUseInfo*>& GetUsers() const { return m_Users; }

	//Edges
	int GetNumEdges() const { return m_EdgeIds.GetCount(); }
	u16 GetEdgeId(const int index) const {	return m_EdgeIds[index]; }
	void AddEdgeId(u16 edgeId); //not __BANK for legacy loading ...

	//PSO
	static void PostPsoPlace(void* data);

#if __BANK
	void Reset();

	//Users
	void SetMaxUsers(u8 _max) { Assert(_max != 0); m_MaxUsers = _max; }

	//Edges
	void RemoveEdgeId(u16 edgeId);
	void ShiftEdgeIdsForIdRemoval(u16 edgeId);

#if DR_ENABLED
	void RenderUsage(const char* regionName, debugPlayback::OnScreenOutput& screenSpew) const;
	static void RenderUsageSingle(const char* regionName, const CScenarioPointChainUseInfo& user, debugPlayback::OnScreenOutput& screenSpew);
#endif //DR_ENABLED
#endif //__BANK

private:
	//Parser data
	u8 m_MaxUsers;
	atArray<u16> m_EdgeIds;

	//Non-parser data
	atArray<CScenarioPointChainUseInfo*> m_Users;

	PAR_SIMPLE_PARSABLE;
};

//-----------------------------------------------------------------------------

class CScenarioChainSearchResults
{
public:
	CScenarioChainSearchResults()
		: m_Point(NULL)
		, m_Action(CScenarioChainingEdge::kActionDefault)
		, m_NavMode(CScenarioChainingEdge::kNavModeDefault)
		, m_NavSpeed(CScenarioChainingEdge::kNavSpeedDefault)
	{
	}

	CScenarioPoint* m_Point;
	u8 m_Action;
	u8 m_NavMode;
	u8 m_NavSpeed;
};

//-----------------------------------------------------------------------------

class CScenarioChainingGraph 
{
public:
	CScenarioChainingGraph();
	~CScenarioChainingGraph();

#if __BANK
	int FindNode(const CScenarioChainingNode& node) const;
	int FindOrAddNode(const CScenarioChainingNode& node);

	void AddEdge(int nodeIndex1, int nodeIndex2);
	void DeleteEdge(int edgeIndex);

	void DeleteAnyNodesAndEdgesForPoint(const CScenarioPoint& point);
	void DeleteNodeAndConnectedEdges(int chainingNodeIndex);

	void UpdateNodesHaveEdges();

	CScenarioChainingEdge& GetEdge(int edgeIndex)
	{	return m_Edges[edgeIndex];	}
	
	CScenarioChain& GetChain(int chainIndex)
	{	return m_Chains[chainIndex];	}

	int RemoveDuplicateEdges();
	int RemoveLooseNodes();
	int RemoveEmptyChains();
	void DeleteChain(int chainIndex);	// Note: just deletes from m_Chains and updates the users.

	void PreSave();
	void PrepSaveObject(const CScenarioChainingGraph& prepFrom);

#if DR_ENABLED
	void RenderUsage(const char* regionName, debugPlayback::OnScreenOutput& screenSpew) const;
#endif
#endif	// __BANK

	// PURPOSE:	register a user of a chain and return a class which is used to signify when the user of the chain is 
	// no longer a user of the chain.
	static CScenarioPointChainUseInfo* RegisterPointChainUser(const CScenarioPoint& pt, CPed* pPed, CVehicle* pVeh, bool allowOverflow = false);
	static CScenarioPointChainUseInfo* RegisterPointChainUserForSpecificChain(CPed* pPed, CVehicle* pVeh, int regionIndex, int chainIndex, int nodeIndex);
	static CScenarioPointChainUseInfo* RegisterChainUserDummy(const CScenarioPoint& pt, bool allowOverflow = false);
	static void UpdateDummyChainUser(CScenarioPointChainUseInfo& inout, CPed* pPed, CVehicle* pVeh);
	static void UnregisterPointChainUser(CScenarioPointChainUseInfo& chainUser);
	static void UpdateNewlyLoadedScenarioRegions();
	static void ReportLoadedScenarioRegion() { sm_LoadedNewScenarioRegion = true; }
	static bool GetLoadedNewScenarioRegion() { return sm_LoadedNewScenarioRegion; }
	static void UpdatePointChainUserInfo();
	static bool RemoveChainUserIfUnused(CScenarioPointChainUseInfo& user);
	static const atArray<CScenarioPointChainUseInfo*>& GetChainUserArrayContainingUser(CScenarioPointChainUseInfo& chainUser);

	void PostLoad(CScenarioPointRegion& reg);
	static void PostPsoPlace(void* data);

	int FindChainingNodeIndexForScenarioPoint(const CScenarioPoint& pt) const;
	int FindChainedScenarioPointsFromScenarioPoint(const CScenarioPoint& pt, CScenarioChainSearchResults* results, const int maxInDestArray) const;
	int FindChainedScenarioPointsFromNodeIndex(const int nodeIndex, CScenarioChainSearchResults* results, const int maxInDestArray) const;
	
	void UpdateNodeToScenarioPointMapping(CScenarioPointRegion& reg, bool force = false);
	void UpdateNodeToScenarioMappingForAddedPoint(CScenarioPoint& pt);
	void UpdateNodeToChainIndexMapping();
	int	 GetNodesChainIndex(const int nodeindex) const;
	void AddChainUser(CScenarioPointChainUseInfo* user, bool allowOverflow = false);
	void RemoveChainUser(CScenarioPointChainUseInfo* user);

	CScenarioPoint* GetChainingNodeScenarioPoint(int chainingNodeIndex) const
	{
		Assert(m_NodeToScenarioPointMappingUpToDate);
		return m_NodeToScenarioPointMapping[chainingNodeIndex];
	}

	int GetNumNodes() const
	{	return m_Nodes.GetCount();	}

	int GetNumEdges() const
	{	return m_Edges.GetCount();	}
	
	int GetNumChains() const
	{	return m_Chains.GetCount();	}

	CScenarioChainingNode& GetNode(int nodeIndex)
	{	return m_Nodes[nodeIndex];	}

	const CScenarioChainingNode& GetNode(int nodeIndex) const
	{	return m_Nodes[nodeIndex];	}

	const CScenarioChainingEdge& GetEdge(int edgeIndex) const
	{	return m_Edges[edgeIndex];	}
	
	const CScenarioChain& GetChain(int chainIndex) const
	{	return m_Chains[chainIndex];	}

	static float sm_PointSearchRadius;//MAGIC

	void ResetChainUsers(bool permanentlyRemoveUsers = true, bool reclaimMemory = false);

	void RemoveFromStore();
protected:

	int LegacyUpdateNodeToChainIndexMapping(atArray<int>& outNodeToChainIndexMapping); //Legacy convert
	void UpdateChainFullStatus(const CScenarioChain& chain, const bool full) const;

#if __BANK
	CScenarioChain& GetNewChain();

	void DeleteNodeInternal(u32 nodeIndex);

	void AddEdgeToChain(u16 edgeId);
	void RemoveEdgeFromChains(u16 edgeId);
	void DeleteEdgeInternal(u16 edgeId);
#endif

	// PURPOSE:	Check if a given chaining node matches the given CScenarioPoint.
	static bool CheckNodeMatchesScenarioPoint(const CScenarioChainingNode& node, const CScenarioPoint& scenarioPoint);

	atArray<CScenarioChainingNode>		m_Nodes;
	atArray<CScenarioChainingEdge>		m_Edges; 
	atArray<CScenarioChain>				m_Chains;

	atArray<int> m_NodeToChainIndexMapping; // Mapping of node index to the chain that the node belongs too ... Filled by UpdateNodeToChainIndexMapping
	
	// PURPOSE:	Parallel array to m_Nodes, for how the nodes map to
	//			CScenarioPoint objects.
	atArray<RegdScenarioPnt>			m_NodeToScenarioPointMapping;

	// PURPOSE:	Keep track of if m_NodeToScenarioPointMapping is expected to be
	//			up to date with m_Nodes, mostly just to catch errors.
	bool	m_NodeToScenarioPointMappingUpToDate;

	static bool sm_LoadedNewScenarioRegion;

	PAR_SIMPLE_PARSABLE;
};

//-----------------------------------------------------------------------------

#endif	// TASK_SCENARIO_SCENARIO_CHAINING_H

// End of file 'task/Scenario/ScenarioChaining.h'
