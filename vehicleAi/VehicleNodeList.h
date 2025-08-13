#ifndef VEHICLE_NODE_LIST_H
#define VEHICLE_NODE_LIST_H

#include "grcore/debugdraw.h"
#include "vehicleAi/pathfind.h"
#include "vehicleAi/VehMission.h"

class CVehicle;

#define BIT_TURN_STRAIGHT_ON	(1<<0)
#define BIT_TURN_RIGHT			(1<<1)
#define BIT_TURN_LEFT			(1<<2)
#define BIT_NOT_SET				(1<<3)


enum { CAR_OLD_PATHNODES_STORED = 2 };			// Old nodes that are kept purely so that the virtual road stuff doesn't jump

// The following are kept outside of CAutoPilot to keep the code readable.
enum
{
	NODE_VERY_OLD = CAR_OLD_PATHNODES_STORED,
	NODE_OLD,
	NODE_NEW,
	NODE_FUTURE
};


enum
{
	LINK_VERY_OLD_TO_OLD = CAR_OLD_PATHNODES_STORED,
	LINK_OLD_TO_NEW,
	LINK_NEW_TO_FUTURE
};


enum
{
	LANE_VERY_OLD = CAR_OLD_PATHNODES_STORED,
	LANE_OLD,
	LANE_NEW,
	LANE_FUTURE
};

//---------------------------------------------------------------
//	CVehicleNodeList
//	A list of road nodes used by the vehicle AI, to determine the
//	vehicle's future direction - and to maintain a short history

class CVehicleNodeList
{
public:
	CVehicleNodeList();
	~CVehicleNodeList();

	static const s32 CAR_OLD_PATHNODES_STORED = 2;
	static const s32 CAR_MAX_NUM_PATHNODES_STORED = 12 + CAR_OLD_PATHNODES_STORED;

	void Clear();

	inline const CNodeAddress & GetPathNodeAddr(const int iIndex) const
	{
		return m_Entries[iIndex].m_Address;
	}
	inline const CPathNode * GetPathNode(const int iIndex) const
	{
		if(!m_Entries[iIndex].m_Address.IsEmpty() && ThePaths.IsRegionLoaded(m_Entries[iIndex].m_Address))
		{
			return ThePaths.FindNodePointer(m_Entries[iIndex].m_Address);
		}
		return NULL;
	}
	inline const s16 GetPathLinkIndex(const int iIndex) const
	{
		return m_Entries[iIndex].m_iLink;
	}
	inline const s8 GetPathLaneIndex(const int iIndex) const
	{
		return (!m_Entries[iIndex].m_Address.IsEmpty()) ? m_Entries[iIndex].m_iLaneIndex : 0;
	}
	inline void Set(const s32 iIndex, const CNodeAddress & iNode, s16 iLink = 0, s8 iLaneIndex = 0)
	{
		m_bDirty = true;
		m_Entries[iIndex].m_Address = iNode;
		m_Entries[iIndex].m_iLink = iLink;
		m_Entries[iIndex].m_iLaneIndex = iLaneIndex;
	}
	inline void SetPathNodeAddr(const s32 iIndex, const CNodeAddress & iNode)
	{
		m_bDirty = true;
		m_Entries[iIndex].m_Address = iNode;
	}
	inline void SetPathLinkIndex(const s32 iIndex, const s16 iLink)
	{
		m_bDirty = true;
		Assert(iLink >= 0 && (int)iLink <= MAXLINKSPERREGION);
		m_Entries[iIndex].m_iLink = iLink;
	}
	inline void SetPathLaneIndex(const s32 iIndex, const s8 iLane)
	{
		m_bDirty = true;
		Assert(iLane >=0 && iLane < 10);
		m_Entries[iIndex].m_iLaneIndex = iLane;
	}

	void ClearPathNodes();
	void ClearFutureNodes();
	void ClearFrom(u32 iFirstNodeToClear);
	void ClearNonEssentialNodes();
	void ClearLanes();
	void ShiftNodesByOne();
	void ShiftNodesByOneWrongWay();
	s32 FindNumPathNodes() const;
	s32 FindLastGoodNode() const;
	s32 FindNodeIndex(const CNodeAddress& iNode) const;
	void FindLinksWithNodes(const s32 iStartNode=0);
	void FindLanesWithNodes(const s32 iStartNode=0, const Vector3* pvStartPos=NULL, const bool bMayRandomizeLanes=true);
	bool GetClosestPosOnPath(const Vector3 & vPos, Vector3 & vClosestPos_out, int & iPathIndexClosest_out, float & iPathSegmentPortion_out, const int iStartNode=0, const int iEndNode=CAR_MAX_NUM_PATHNODES_STORED-1) const;

	bool JoinToRoadSystem(const CVehicle * pVehicle, const float fRejectDist=DEFAULT_REJECT_JOIN_TO_ROAD_DIST);
	bool JoinToRoadSystem(const Vector3& vStartPosition, const Vector3& vStartOrientation, const bool bIncludeWaterNodes, s32 iOldNode, s32 iCurrentNode, const float fRejectDist=DEFAULT_REJECT_JOIN_TO_ROAD_DIST, const bool bIgnoreDirection = false);
	bool ContainsNode(const CNodeAddress & iNode) const;
	bool GivesWayBeforeNextJunction() const;
	bool IsPointWithinBoundariesOfSegment(const Vector3& vPos, const s32 iSegment) const;
	bool IsTargetNodeAfterUTurn() const;

#if __DEV
	void CheckLinks() const;
	void VerifyNodesAreDistinct() const;
#endif

#if __BANK && DEBUG_DRAW
	void DebugDraw(int iStart, int iEnd) const;
#endif

	// Access the current progress
	s32 GetTargetNodeIndex() const { return m_targetNodeIndex; }
	void SetTargetNodeIndex(rage::s32 val) 
	{
		Assert(val >= 0 && val < CAR_MAX_NUM_PATHNODES_STORED); 

		if (val != m_targetNodeIndex)
		{
			//only flip the dirty bit if this changes--it's called every frame
			m_bDirty = true;
		}

		//this case should be ok since we already assert on the range check above
		m_targetNodeIndex = (s16)val; 
	}

private:
	struct CEntry
	{
		CNodeAddress m_Address;
		s16 m_iLink;				// NB: This index is into the entire CPathRegion (it's *not* the index within the node)
		s8 m_iLaneIndex;
		s8 m_iPadding;
	};

	atRangeArray<CEntry, CAR_MAX_NUM_PATHNODES_STORED> m_Entries;
	s16		m_targetNodeIndex;
	mutable bool	m_bDirty;
	mutable bool	m_bContainsUTurn;
}; 


#endif //VEHICLE_NODE_LIST_H