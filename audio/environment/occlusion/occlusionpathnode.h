//
// audio/environment/occlusion/occlusionpathnode.h
//
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#ifndef NAOCCLUSIONPATHNODE_H
#define NAOCCLUSIONPATHNODE_H

#include "audio/debugaudio.h"

#include "audioengine/curve.h"
#include "audioengine/enginedefs.h"
#include "atl/array.h"
#include "fwscene/world/InteriorLocation.h"
#include "fwtl/Pool.h"
#include "vector/vector3.h"

#if __BANK
#include "bank/bank.h"
#endif

class naOcclusionPathNode;
class naOcclusionPortalInfo;
class naEnvironmentGroup;
class CInteriorInst;

namespace rage {
	class fwPortalCorners;
}

struct naOcclusionPathNodeChild
{
	naOcclusionPathNodeChild()
	{
		pathNode = NULL;
		portalInfo = NULL;
	}
	naOcclusionPathNode* pathNode;
	naOcclusionPortalInfo* portalInfo;
};

struct naOcclusionPathNodeData
{
	// Constructor is empty because we use the ResetData() whenever we're using this struct.
	// The reason for this is sometimes we're allocating memory, and sometimes we're not, but we always want to reset the data
	// and there's no sense in resetting it on allocation, and then resetting it again before usage.
	naOcclusionPathNodeData(){}

	void ResetData()
	{
		pathSegmentStart.Zero();
		pathSegmentEnd.Zero();
		normalizedPathSegment.Zero();
		parent = NULL;
		portalCorners = NULL;
		interiorHash = 0;
		roomIdx = INTLOC_INVALID_INDEX;
		angleFactor = 0.0f;
		blockageFactor = 0.0f;
		pathDistance = 0.0f;
		weight = 0.0f;
		pathSegmentDistance = 0.0f;
		usePath = false;

#if __BANK
		debugDrawPos.Zero();
		drawDebug = false;
#endif
	}

	Vector3 pathSegmentStart;
	Vector3 pathSegmentEnd;
	Vector3 normalizedPathSegment;
	naOcclusionPathNodeData* parent;
	const fwPortalCorners* portalCorners;
	u32 interiorHash;
	s32 roomIdx;
	f32 angleFactor;
	f32 blockageFactor;
	f32 pathDistance;
	f32 pathSegmentDistance;
	f32 weight;
	bool usePath;

	atArray<naOcclusionPathNodeData> children;

#if __BANK
	Vector3 debugDrawPos;
	bool drawDebug;
#endif
};	

//*********************************************************************
// naOcclusionPathNode
// An naOcclusionPathNode is a node in the path tree, and contains an array of children (other path nodes and the info on how to reach them - portalinfos) which will eventually reach the final destination
// We build the portal paths via the 'ComputePortalPaths' RAG button in Audio->Occlusion when running a __BANK build and write them to a file.
class naOcclusionPathNode
{
public:

	FW_REGISTER_CLASS_POOL(naOcclusionPathNode);

	naOcclusionPathNode();
	~naOcclusionPathNode();

	// Init curves
	static void InitClass();

	static naOcclusionPathNode* Allocate();

	// Looks at the angles, distance, and portal blockage along the path for the specified key, mixing them down at each portal depth
	void ProcessOcclusionPathTree(naOcclusionPathNodeData* currentDataNode, naEnvironmentGroup* environmentGroup, Vector3& simulatedPlayPos);

	s32 GetNumChildren() const { return m_Children.GetCount(); }

	void ReserveChildArray(u32 numChildren) { m_Children.Reserve(numChildren); }

	naOcclusionPathNodeChild* CreateChild(naOcclusionPathNode* pathNode, naOcclusionPortalInfo* portalInfo);

	u32 GetCapacity() const { return m_Children.GetCapacity(); }
	void ResetChildArray() { m_Children.Reset(); }

	naOcclusionPathNodeChild* GetChild(const s32 childIdx) { return &m_Children[childIdx]; }

#if __BANK
	u32 GetMemoryFootprint();

	u32 GetRoomHash();
	u32 GetDestinationInteriorHash();
	u32 GetDestinationRoomHash();

	static void AddWidgets(bkBank& bank);

	// Goes through every portal in the start CInteriorInst, and creates a child if it reaches the room.  If it doesn't reach the room, it looks for
	// paths starting at the next room that reach the final room within the alloted path depth.
	bool BuildPathTree(const CInteriorInst* currentIntInst, const s32 currentRoomIdx, 
						const CInteriorInst* finishIntInst, const s32 finishRoomIdx, 
						const u32 pathDepth, const bool allowOutside, const bool buildInterInteriorPaths);
#endif


private:

	void BuildOcclusionDataTree(naOcclusionPathNodeData* currentDataNode);

	void ComputePathSegmentEnds(naOcclusionPathNodeData* currentDataNode, naEnvironmentGroup* environmentGroup);

	void ComputePathSegmentStarts(naOcclusionPathNodeData* currentDataNode);

#if __BANK
	void ComputeOcclusionFromPathTree(naOcclusionPathNodeData* currentDataNode, naEnvironmentGroup* environmentGroup, f32 currentPathDistance = 0.0f, u32 pathDepth = 0);
#else
	void ComputeOcclusionFromPathTree(naOcclusionPathNodeData* currentDataNode, naEnvironmentGroup* environmentGroup, f32 currentPathDistance = 0.0f);
#endif

	// Creates a final distance, angleFactor, and blockage value based on the child portal weights for the current node
	void InterpolateChildData(naOcclusionPathNodeData* currentDataNode);

	// Weights each possible portal path based on angles, distance, and blockage
	void ComputeChildWeights(naOcclusionPathNodeData* currentNodeData);

	// Determines where the sounds final panning position should be based on the location and weight of the portals
	void ComputeSimulatedPlayPos(naOcclusionPathNodeData* currentDataNode, const Vector3& playerPos, Vector3& simulatedPlayPos);

	// Angle factor is a value between 0 and 1 based on the angle formed where the path travels through the portal.  
	// 0 is a straight line through, 1 is completely doubled back on itself
	f32 GetAngleFactor(const Vector3& pathSegment1, const Vector3& pathSegment2);

	// Determines where the sound would intersect the portal plane between to positions, so basically where the sound would go through the portal to reach you from the source
	void GetPortalIntersectionBetweenPositions(const Vector3& startPos, const Vector3& finishPos, Vector3& portalIntersection, const fwPortalCorners* portalCorners);

	// Gets the 2 closest portal corner indices for a position.
	void GetClosestPortalCornersToPosition(u32& closestCornerIdx, u32& secondClosestCornerIdx, const Vector3& position, const fwPortalCorners* portalCorners);

	// Figures out how far away you are from the portal and the openness and type of door to get a final metric for how much sound is blocked
	f32 GetBlockage(naOcclusionPortalInfo* portalInfo, const Vector3& position, naEnvironmentGroup* environmentGroup, f32 currentPathDistance = 0.0f);

#if __BANK
	void DrawPortalPathLine(const Vector3& point1, const Vector3& point2);
	void DrawPortalPathInfo(const Vector3& portalPos, f32 angle, f32 angleDamping, f32 portalBlockage, s32 childIdx);

	bool FindAndCreateChildNodes(const CInteriorInst* currentIntInst, const s32 currentRoomIdx, const CInteriorInst* finishIntInst, const s32 finishRoomIdx, 
								const u32 pathDepth, const bool allowOutside);
	void FindAllLinkedInteriors(const CInteriorInst* startIntInst, const CInteriorInst* currentIntInst, atArray<const CInteriorInst*>& linkedIntInstList, u32 currentPathDepth = 0);
#endif

public:

#if __BANK
	static const s32 sm_OcclusionBuildPathNodePoolSize = 10000;
#endif

private:

	// If we want to just store indices in the metadata to the other pathnodes and portal info's then we could get rid of this
	atArray<naOcclusionPathNodeChild> m_Children;
 
	static f32 sm_AngleFactorToWeightExponent;
	static f32 sm_BlockedFactorToWeightExponent;

	static audCurve sm_DistanceToAngleFactorDamping;
};

#endif // NAOCCLUSIONPATHNODE_H
