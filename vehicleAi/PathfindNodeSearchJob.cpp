// Vehicle population SPU update
// flavius.alecu: 25/10/2011

#include "math/simpleMath.h"

#include "pathfind.h"

#if __SPU
	#define PASS_SPURS_ARGS (1)
	#include "system/task_spu.h"
	#include "system/taskheaderspu.h"
	#include "system/dependency_spu.h"
	#include "system/tinyheap.cpp"
	#include "system/memops.h"
	#include "softrasterizer/tsklib.h"
#endif

#if !__SPU
#include "control/gps.h"
#endif 

#include "BaseTypes.h"

#include "renderer\Renderer.h"

#include "grcore/viewport.h"
#include "grcore/viewport_inline.h"
#include "pathfind.h"
#include "vector/Vector4.h"
#include "scene/world/gameWorld.h"
#include "system/timer.h"
#include "fwscene/world/WorldLimits.h"
#include "fwmaths/Vector.h"
#include "profile/timebars.h"

#include "vector/geometry.h"
#if __SPU
#include "vector/geometry.cpp"
#endif

#if __SPU
#define SPU_PAD_SIZE(x) (((x) + 15) &~15)
#define GET_TEMP_NODE_DMA_TAG(x) (x+1)
#define GET_TEMP_LINK_DMA_TAG(x) (x+1)

#define DEBUG_PATHFIND_NODE_SEARCH_SPU 0

#if DEBUG_PATHFIND_NODE_SEARCH_SPU
#define SPU_DEBUG_ONLY(x) x
#else
#define SPU_DEBUG_ONLY(...)
#endif

	class CPathFindGPSSpu
	{
	public:

		CPathFindGPSSpu()
		{
			m_LocalNodes = NULL;
		}
		// Localized some of the CPathFind Functions to this class for use on the SPU
		float	FindXCoorsForRegion(s32 Region) const { return WORLDLIMITS_REP_XMIN + Region * PATHFINDREGIONSIZEX; }
		float	FindYCoorsForRegion(s32 Region) const { return WORLDLIMITS_REP_YMIN + Region * PATHFINDREGIONSIZEY; }

		s32		FindXRegionForCoors(float X) const;
		s32		FindYRegionForCoors(float Y) const;
		float	CalculateFitnessValueForNode(const Vector3& Pos, const Vector3& Dir, const CPathNode* pNode) const;

		void	FindNodeClosestInRegionSPU(CNodeAddress * pNodeFound, u32 Region, const Vector3 & SearchCoors, float * pClosestDist, void * pData, const float fCutoffDistForNodeSearch, bool bForGps);

		CNodeAddress	FindNodeClosestToCoorsSPU(const Vector3& SearchCoors, void * pData, float CutoffDistForNodeSearch, bool bForGps);

		CPathNodeLink* GetNodesLink(const CPathNode* pNode, const s32 iLink) const;
		bool IsLinkNodeLocal(CPathNode* pNode, s32 iLink) const;
		CPathNode*	FindNodePointerSafe(const CNodeAddress Node) const;
		CPathNode*	FindPPUNodePointer(CPathNode* lsNode);

		void DMALinkNode(const CNodeAddress* Node, u32 linkIndex);
		void DMAAllLinkNodes(CPathNode* pNode, u32 linkIndex);
		void DMANextBatchOfLinks(u32 Region);

		bool FindClosestNodeLinkCB(CPathNode * pNode, void * pData);
		// Keep track of if our current best node is on a one-way link, and it's current fitness
		float m_CurrentBestNodeFitness;
		bool m_CurrentBestNodeIsOneWay;
		Vector3 m_CurrentBestNodeCoors;
		
		s32 FindLocalRegionIndex(s32 region) const;

		void LoadForeignRegion(s32 region);
		void LoadForeignNodes(s32 region);

		void FindRoadBoundaries(s32 lanesTo, s32 lanesFrom, float centralReservationWidth, bool bNarrowRoad, bool bAllLanesThoughCentre, float& roadWidthOnLeft_out, float& roadWidthOnRight_out) const;

		CPathRegion** foreignRegions;
		
#define MAX_LOCAL_REGIONS (64)
		CPathRegion* m_LocalRegions[MAX_LOCAL_REGIONS]; // we DMA in regions to access their member variables
		s16	m_RegionNumbers[PATHFINDREGIONS]; // The Region numbers of the DMA regions
		s32 m_LocalRegionCount; // Current number of regions stored locally
		CPathNode*	m_LocalNodes; // Storage for a single regions nodes, so that we can iterate over them
		s32 m_LocalNodeRegionIndex; // The region number for the locally stored nodes
		
#define LINK_BATCH_NUMBER (2)
		CPathNodeLink* m_LocalLinks[LINK_BATCH_NUMBER]; // storage for links we DMA from the current region, split into batches
		s32 m_LocalLinkOffsets[LINK_BATCH_NUMBER]; // Offset values for the link batches, so we can keep track of what links we have locally
		u32 m_LocalLinkCurrentMax; // current highest link number
		u32 m_CurrentBatchSlot; // The batch slot we're using
		
#define MAX_TEMP_NODES (64)
#define MAX_TEMP_LINKS (512)
		CPathNode* m_TempLinkNodes[LINK_BATCH_NUMBER]; // storage for nodes in other regions attached to links in the current region - we dma these nodes
		CNodeAddress* m_TempLinkNodeAddresses[LINK_BATCH_NUMBER]; // Addresses for these temporary nodes
		u8* m_TempLinkNodeTags[LINK_BATCH_NUMBER]; // DMA tags for the temp nodes
		u32 m_TempLinkNodeCount[LINK_BATCH_NUMBER]; // count of temporary nodes
		u32 m_LocalLinkIndex; // batch number for dma'ing link nodes

		// Our input parameters
		CPathFind::FindNodeClosestToCoorsAsyncParams* m_LocalAsyncParams;
		// Search structure to keep track of some params as well as our output node
		CPathFind::TFindClosestNodeLinkGPS* m_LocalSearchParams;

		// Heap allocator for our scratch space
		sysTinyHeap m_HeapAllocator;

		void IncrementLocalLinkIndex()
		{
			m_LocalLinkIndex++;
			if(m_LocalLinkIndex >= LINK_BATCH_NUMBER)
			{
				m_LocalLinkIndex = 0;
			}
		}

		// Get Next index, doesn't increment actual index
		u32 GetNextLocalLinkIndex()
		{
			u32 tempIndex = m_LocalLinkIndex;
			tempIndex++;
			if(tempIndex >= LINK_BATCH_NUMBER)
			{
				tempIndex = 0;
			}
			return tempIndex;
		}

		u8 m_TempNodeCurrentTag;
		u8 GetNextTempNodeTag()
		{
			m_TempNodeCurrentTag++;
			if(m_TempNodeCurrentTag > LINK_BATCH_NUMBER+8)
				m_TempNodeCurrentTag = LINK_BATCH_NUMBER;

			return m_TempNodeCurrentTag;
		}
	};
#endif

#if __SPU
	#include "vehicleAi/pathfind_shared.cpp"
#endif

#if __SPU
// Spu job entry point
void PathfindNodeSearchSPU(::rage::sysTaskParameters &taskParams, CellSpursJobContext2* UNUSED_PARAM(jobContext), CellSpursJob256 *UNUSED_PARAM(job))
 {	
	CPathFindGPSSpu ThePathsGPSSpu;	
	// Initialize heap allocator with scratch space pointer. We only use this allocator from here on out.
	ThePathsGPSSpu.m_HeapAllocator.Init(taskParams.Scratch.Data, taskParams.Scratch.Size);
	// Pointers to the regions in PPU memory
	ThePathsGPSSpu.foreignRegions = (CPathRegion**)taskParams.ReadOnly[0].Data;

	// set up region storage
	for(s32 i = 0; i < MAX_LOCAL_REGIONS; i++)
	{		
		ThePathsGPSSpu.m_LocalRegions[i] = (CPathRegion*)ThePathsGPSSpu.m_HeapAllocator.Allocate(SPU_PAD_SIZE(static_cast<size_t>(sizeof(CPathRegion))));		
	}
	for(s32 i = 0; i < PATHFINDREGIONS; i++)
	{
		ThePathsGPSSpu.m_RegionNumbers[i] = -1;
	}
	ThePathsGPSSpu.m_LocalRegionCount = 0;
	ThePathsGPSSpu.m_LocalNodeRegionIndex = -1; // no nodes are currently loaded

	// Set up temporary link storage
	for(int i = 0; i < LINK_BATCH_NUMBER; i++)
	{
		ThePathsGPSSpu.m_TempLinkNodes[i] = (CPathNode*)ThePathsGPSSpu.m_HeapAllocator.Allocate(static_cast<size_t>(MAX_TEMP_NODES*sizeof(CPathNode)));
		ThePathsGPSSpu.m_TempLinkNodeAddresses[i] = (CNodeAddress*)ThePathsGPSSpu.m_HeapAllocator.Allocate(static_cast<size_t>(MAX_TEMP_NODES*sizeof(CNodeAddress)));
		ThePathsGPSSpu.m_TempLinkNodeTags[i] = (u8*)ThePathsGPSSpu.m_HeapAllocator.Allocate(static_cast<size_t>(MAX_TEMP_NODES));
		memset(ThePathsGPSSpu.m_TempLinkNodeTags[i], 0, MAX_TEMP_NODES);
		ThePathsGPSSpu.m_TempLinkNodeCount[i] = 0;
		ThePathsGPSSpu.m_LocalLinks[i] = (CPathNodeLink*)ThePathsGPSSpu.m_HeapAllocator.Allocate(static_cast<size_t>(MAX_TEMP_LINKS*sizeof(CPathNodeLink)));
		ThePathsGPSSpu.m_LocalLinkOffsets[i] = -1;
		
	}
	ThePathsGPSSpu.m_LocalLinkCurrentMax = 0;
	ThePathsGPSSpu.m_LocalLinkIndex = 0;
	ThePathsGPSSpu.m_CurrentBatchSlot = 0;
	ThePathsGPSSpu.m_TempNodeCurrentTag = LINK_BATCH_NUMBER;
	ThePathsGPSSpu.m_CurrentBestNodeIsOneWay = false;
	ThePathsGPSSpu.m_CurrentBestNodeFitness = -1.f;
	ThePathsGPSSpu.m_CurrentBestNodeCoors = VEC3_ZERO;

	ThePathsGPSSpu.m_LocalAsyncParams = (CPathFind::FindNodeClosestToCoorsAsyncParams*)(taskParams.Input.Data);
	SPU_DEBUG_ONLY(spu_printf("Starting New Search for slot %u\n", (u32)ThePathsGPSSpu.m_LocalAsyncParams->m_CallingGPSSlot));
	// Get Search data structure
	ThePathsGPSSpu.m_LocalSearchParams = (CPathFind::TFindClosestNodeLinkGPS*)taskParams.ReadOnly[1].Data;
	
	// Do the search
	ThePathsGPSSpu.FindNodeClosestToCoorsSPU(ThePathsGPSSpu.m_LocalAsyncParams->m_SearchCoors, (void*)ThePathsGPSSpu.m_LocalSearchParams, ThePathsGPSSpu.m_LocalAsyncParams->m_CutoffDistance, ThePathsGPSSpu.m_LocalAsyncParams->m_bForGPS);

	// store our best node so far
	CPathNode * pSourceNode = (CPathNode*)alloca(sizeof(CPathNode));
	sysMemCpy(pSourceNode, ThePathsGPSSpu.m_LocalSearchParams->pBestNodeSoFar, sizeof(CPathNode));
	if (pSourceNode)
	{ 		
		// Test for finding one-way nodes
		// if the node was one-way, we should have a current best fitness that is not -1.f
		if (ThePathsGPSSpu.m_CurrentBestNodeIsOneWay && ThePathsGPSSpu.m_CurrentBestNodeFitness < ThePathsGPSSpu.m_LocalAsyncParams->m_fFitnessThreshold)
		{
			float FitnessFirstNode = ThePathsGPSSpu.m_CurrentBestNodeFitness; 
			Vector3 vSourceNode = ThePathsGPSSpu.m_CurrentBestNodeCoors;
			CPathFind::TFindClosestNodeLinkGPS ignoreOneWayData;

			ignoreOneWayData.Reset();
			ignoreOneWayData.vSearchOrigin = ThePathsGPSSpu.m_LocalSearchParams->vSearchOrigin;
			ignoreOneWayData.vSearchDir = ThePathsGPSSpu.m_LocalSearchParams->vSearchDir;
			ignoreOneWayData.fDistZPenalty = ThePathsGPSSpu.m_LocalSearchParams->fDistZPenalty;
			ignoreOneWayData.bIgnoreHeading = ThePathsGPSSpu.m_LocalSearchParams->bIgnoreHeading;
			ignoreOneWayData.fHeadingPenalty = ThePathsGPSSpu.m_LocalSearchParams->fHeadingPenalty;
			ignoreOneWayData.bIgnoreOneWay = true;

			ThePathsGPSSpu.m_CurrentBestNodeCoors = VEC3_ZERO;
			
			// Search again, ignoring one way nodes
			ThePathsGPSSpu.FindNodeClosestToCoorsSPU(ThePathsGPSSpu.m_LocalAsyncParams->m_SearchCoors, &ignoreOneWayData, ThePathsGPSSpu.m_LocalAsyncParams->m_CutoffDistance, ThePathsGPSSpu.m_LocalAsyncParams->m_bForGPS);			
			
			CPathNode * pSourceNodeIgnoreOneWay = ignoreOneWayData.pBestNodeSoFar;
			if (pSourceNodeIgnoreOneWay)
			{
				// when we run the search again, the fitness will be updated
				float FitnessSecondNode = ThePathsGPSSpu.m_CurrentBestNodeFitness;
				if (FitnessFirstNode > FitnessSecondNode * ThePathsGPSSpu.m_LocalAsyncParams->m_fTwoLinkBonusFitness)
				{
					Vector3 vSourceNodeIgnoreOneWay = ThePathsGPSSpu.m_CurrentBestNodeCoors;					

					pSourceNode =
						(vSourceNode - vSourceNodeIgnoreOneWay).XYMag2() < ThePathsGPSSpu.m_LocalAsyncParams->m_fPreferBothWayProximitySqrXY &&
						IsClose(vSourceNode.z, vSourceNodeIgnoreOneWay.z, ThePathsGPSSpu.m_LocalAsyncParams->m_fPreferBothWayMaxZ) ?
	pSourceNodeIgnoreOneWay : pSourceNode;

					if (pSourceNode == pSourceNodeIgnoreOneWay)
					{
						// we need to use the ignore one way data
						ThePathsGPSSpu.m_LocalSearchParams->vClosestPointOnNearbyLinks = ignoreOneWayData.vClosestPointOnNearbyLinks;
						ThePathsGPSSpu.m_LocalSearchParams->pBestNodeSoFar = pSourceNode;
						ThePathsGPSSpu.m_LocalSearchParams->resultNodeAddress = ignoreOneWayData.resultNodeAddress;
					}
				}				
			}
		}
	}

	// Copy output struct
	sysMemCpy(taskParams.Output.Data, ThePathsGPSSpu.m_LocalSearchParams, sizeof(CPathFind::TFindClosestNodeLinkGPS));
	SPU_DEBUG_ONLY(spu_printf("Ending Search for slot %u - %x\n", (u32)ThePathsGPSSpu.m_LocalAsyncParams->m_CallingGPSSlot, (u32)ThePathsGPSSpu.m_LocalSearchParams->pBestNodeSoFar));
	SPU_DEBUG_ONLY(spu_printf("Output size: %u %u\n", (u32)taskParams.Output.Size, (u32)sizeof(CPathFind::TFindClosestNodeLinkGPS)));
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : FindNodeClosestToCoorsSPU - SPU version of FindNodeClosestToCoors
// PURPOSE :  Goes through the nodes on the map and finds the one closest to the
//			  specified coordinate. Cars or peds depending on GraphType.
/////////////////////////////////////////////////////////////////////////////////
CNodeAddress CPathFindGPSSpu::FindNodeClosestToCoorsSPU(const Vector3& SearchCoors, void * pData, float CutoffDistForNodeSearch, bool bForGps)
{
	s32			CenterRegionX, CenterRegionY;//, RegionX, RegionY;
	//s32			Area;
	CNodeAddress	ClosestNode;
	float			ClosestDist = CutoffDistForNodeSearch, DistToBorderOfRegion;
	u16			Region;

	ClosestNode.SetEmpty();

	// To optimise this 
	// We start with the Region that the Searchcoors are within.
	// If we don't find a node or there is a possibility of finding an even
	// closer node we continue to look in ever expanding areas.

	// Find the X and Y of the Region we're in.
	CenterRegionX = FindXRegionForCoors(SearchCoors.x);
	CenterRegionY = FindYRegionForCoors(SearchCoors.y);
	Region = (u16)(FIND_REGION_INDEX(CenterRegionX, CenterRegionY));

	// Get our region
	SPU_DEBUG_ONLY(spu_printf("Our Point: %.2f %.2f\n", SearchCoors.x, SearchCoors.y));
	
	// Load the region into local storage
	LoadForeignRegion(Region);
	

	///////////////////////////////

	DistToBorderOfRegion = SearchCoors.x - FindXCoorsForRegion(CenterRegionX);
	DistToBorderOfRegion = rage::Min(DistToBorderOfRegion, FindXCoorsForRegion(CenterRegionX+1) - SearchCoors.x);
	DistToBorderOfRegion = rage::Min(DistToBorderOfRegion, SearchCoors.y - FindYCoorsForRegion(CenterRegionY));
	DistToBorderOfRegion = rage::Min(DistToBorderOfRegion, FindYCoorsForRegion(CenterRegionY+1) - SearchCoors.y);

	FindNodeClosestInRegionSPU(&ClosestNode, Region, SearchCoors, &ClosestDist, pData, CutoffDistForNodeSearch, bForGps);

	
	// This first function only checks for nodes in the relevant cells assuming we have found a node
	// The second one expand the search one border of a square each search, we don't need to search the full border if we assume we found something!

	//TUNE_GROUP_INT(GPS_OPTIMISATION, FasterFindNodeToCoord, 1, 0, 1, 1);
	if (ClosestDist < rage::Max(PATHFINDREGIONSIZEX, PATHFINDREGIONSIZEY))
	{
		CenterRegionX = FindXRegionForCoors(SearchCoors.x);
		CenterRegionY = FindYRegionForCoors(SearchCoors.y);

		// We might be near a border of a region so we must extend the search as we might not be able to DMA load links between regions
		const float fMinRegionClosestDist = rage::Max(ClosestDist, 25.f);
		s32 RegionToRight = FindXRegionForCoors(SearchCoors.x + fMinRegionClosestDist);
		s32 RegionToLeft = FindXRegionForCoors(SearchCoors.x - fMinRegionClosestDist);
		s32 RegionToTop = FindYRegionForCoors(SearchCoors.y - fMinRegionClosestDist);
		s32 RegionToBottom = FindYRegionForCoors(SearchCoors.y + fMinRegionClosestDist);

		for (int y = RegionToTop; y <= RegionToBottom; ++y)
		{
			for (int x = RegionToLeft; x <= RegionToRight; ++x)
			{
				if (CenterRegionX != x || CenterRegionY != y)
				{						
					LoadForeignRegion(FIND_REGION_INDEX(x, y));
					FindNodeClosestInRegionSPU(&ClosestNode, (u16)(FIND_REGION_INDEX(x, y)), SearchCoors, &ClosestDist, pData, CutoffDistForNodeSearch, bForGps);
				}
			}
		}
	}
	
	if (ClosestNode.IsEmpty()/* || ClosestDist > DistToBorderOfRegion*/) // If we cannot possibly find a closer node we stop here.
	{
		s32			Area, RegionX, RegionY;
		// Expand the search to a wider area. (The border of a square)
		for (Area = 1; Area < 5; Area++)
		{
			RegionX = CenterRegionX - Area;
			if (RegionX >= 0 && RegionX < PATHFINDMAPSPLIT)
			{
				for (RegionY = CenterRegionY - Area; RegionY <= CenterRegionY + Area; RegionY++)
				{
					if (RegionY >= 0 && RegionY < PATHFINDMAPSPLIT)
					{
						LoadForeignRegion(FIND_REGION_INDEX(RegionX, RegionY));
						FindNodeClosestInRegionSPU(&ClosestNode, (u16)(FIND_REGION_INDEX(RegionX, RegionY)), SearchCoors, &ClosestDist, pData, CutoffDistForNodeSearch, bForGps);
					}
				}
			}
			RegionX = CenterRegionX + Area;
			if (RegionX >= 0 && RegionX < PATHFINDMAPSPLIT)
			{
				for (RegionY = CenterRegionY - Area; RegionY <= CenterRegionY + Area; RegionY++)
				{
					if (RegionY >= 0 && RegionY < PATHFINDMAPSPLIT)
					{
						LoadForeignRegion(FIND_REGION_INDEX(RegionX, RegionY));
						FindNodeClosestInRegionSPU(&ClosestNode, (u16)(FIND_REGION_INDEX(RegionX, RegionY)), SearchCoors, &ClosestDist, pData, CutoffDistForNodeSearch, bForGps);
					}
				}
			}

			RegionY = CenterRegionY - Area;
			if (RegionY >= 0 && RegionY < PATHFINDMAPSPLIT)
			{
				for (RegionX = CenterRegionX - Area + 1; RegionX < CenterRegionX + Area; RegionX++)
				{
					if (RegionX >= 0 && RegionX < PATHFINDMAPSPLIT)
					{
						LoadForeignRegion(FIND_REGION_INDEX(RegionX, RegionY));
						FindNodeClosestInRegionSPU(&ClosestNode, (u16)(FIND_REGION_INDEX(RegionX, RegionY)), SearchCoors, &ClosestDist, pData, CutoffDistForNodeSearch, bForGps);
					}
				}
			}
			RegionY = CenterRegionY + Area;
			if (RegionY >= 0 && RegionY < PATHFINDMAPSPLIT)
			{
				for (RegionX = CenterRegionX - Area + 1; RegionX < CenterRegionX + Area; RegionX++)
				{
					if (RegionX >= 0 && RegionX < PATHFINDMAPSPLIT)
					{
						LoadForeignRegion(FIND_REGION_INDEX(RegionX, RegionY));
						FindNodeClosestInRegionSPU(&ClosestNode, (u16)(FIND_REGION_INDEX(RegionX, RegionY)), SearchCoors, &ClosestDist,pData, CutoffDistForNodeSearch, bForGps);
					}
				}
			}

			DistToBorderOfRegion += rage::Min(PATHFINDREGIONSIZEX, PATHFINDREGIONSIZEY);
			// If we cannot possibly find a closer node we stop here.
			if (ClosestDist < DistToBorderOfRegion) break;
		}
	}
#if DEBUG_PATHFIND_NODE_SEARCH_SPU
	CPathFind::TFindClosestNodeLinkGPS * pFindClosest = (CPathFind::TFindClosestNodeLinkGPS*)pData;
	if(!ClosestNode.IsEmpty())
	{		
		spu_printf("Closest Node %u %u %x\n", ClosestNode.GetRegion(), ClosestNode.GetIndex(), pFindClosest ? (u32)(void*)pFindClosest->pBestNodeSoFar : 0);
	}
	else
	{
		spu_printf("Didn't find a closest node\n");
	}
#endif
	return(ClosestNode);
}



/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : FindXRegionForCoors
// PURPOSE :  Given a X and Y coordinate this function works out in what Region of the map this node
//			  is located.
/////////////////////////////////////////////////////////////////////////////////
s32 CPathFindGPSSpu::FindXRegionForCoors(float X) const
{
	s32	ResultX = (s32)((X - WORLDLIMITS_REP_XMIN) / PATHFINDREGIONSIZEX);
	ResultX = rage::Max(ResultX, 0);
	ResultX = rage::Min(ResultX, PATHFINDMAPSPLIT-1);
	return ResultX;
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : FindYRegionForCoors
// PURPOSE :  Given a X and Y coordinate this function works out in what Region of the map this node
//			  is located.
/////////////////////////////////////////////////////////////////////////////////
s32 CPathFindGPSSpu::FindYRegionForCoors(float Y) const
{
	s32	ResultY = (s32)((Y - WORLDLIMITS_REP_YMIN) / PATHFINDREGIONSIZEY);
	ResultY = rage::Max(ResultY, 0);
	ResultY = rage::Min(ResultY, PATHFINDMAPSPLIT-1);
	return ResultY;
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : CalculateFitnessValueForNode
// PURPOSE : Should be used to determine which node is best suited to connect to
/////////////////////////////////////////////////////////////////////////////////
float CPathFindGPSSpu::CalculateFitnessValueForNode(const Vector3& Pos, const Vector3& Dir, const CPathNode* pNode) const
{
	Assertf(pNode, "pNode is NULL, we're going to crash");
	const Vector3 NodePos = pNode->GetPos();
	float BestDist = Max(Pos.Dist(NodePos) * 10.f, 100.f);	// Bad fitness per default

	u32 nNodeLinks = pNode->NumLinks();
	for (u32 i = 0; i < nNodeLinks; ++i)
	{
		CPathNodeLink* NodeLink = GetNodesLink(pNode, i);
		const CPathNode* pOtherNode = NodeLink ? FindNodePointerSafe(NodeLink->m_OtherNode) : NULL;
		if (pOtherNode)
		{
			const Vector3 OtherNodePos = pOtherNode->GetPos();
			float DistToLink = geomDistances::DistanceSegToPoint(NodePos, OtherNodePos, Pos);

			Vector3 LinkDir;
			bool bIgnoreDirCheck = false;
			if (NodeLink->m_1.m_LanesToOtherNode > 0 && NodeLink->m_1.m_LanesFromOtherNode > 0)
				bIgnoreDirCheck = true;
			else if (NodeLink->m_1.m_LanesToOtherNode > 0)
				LinkDir = OtherNodePos - NodePos;
			else
				LinkDir = NodePos - OtherNodePos;

			// Test direction if it is only in one direction
			if (DistToLink < BestDist && (bIgnoreDirCheck || LinkDir.Dot(Dir) > 0.f))
				BestDist = DistToLink;
		}
	}

	return BestDist;
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : FindNodeClosestInRegion
// PURPOSE :  Goes through the nodes on the map and finds the one closest to the
//			  specified coordinate. Cars or peds depending on GraphType.
/////////////////////////////////////////////////////////////////////////////////
void CPathFindGPSSpu::FindNodeClosestInRegionSPU(CNodeAddress * pNodeFound, u32 Region, const Vector3 & SearchCoors, float * pClosestDist, void * pData, const float fCutoffDistForNodeSearch, bool bForGps)
{	
	// Find the region index
	s32 localRegionIndex = FindLocalRegionIndex(Region);

	if(localRegionIndex == -1)
	{
		SPU_DEBUG_ONLY(spu_printf("Didn't find region %u\n", (u32)Region));
		// Didn't find the region? WTF?
		return;
	}

	// check if the region is the current region
	if(m_LocalNodeRegionIndex != localRegionIndex)
	{
		// load the nodes into local storage so we can access them
		LoadForeignNodes(Region);
	}
	// Reset our link counter
	m_LocalLinkCurrentMax = 0;
	m_CurrentBatchSlot = 0;
	// Preload all of the links we can
	for(u32 i = 0; i < LINK_BATCH_NUMBER; i++)
	{
		DMANextBatchOfLinks(Region);
	}
	

	s32	StartNode, EndNode, Node;
	
	StartNode = 0;
	EndNode = m_LocalRegions[localRegionIndex]->NumNodesCarNodes;

	float closestDist2 = *pClosestDist * *pClosestDist;
#if DEBUG_PATHFIND_NODE_SEARCH_SPU
	u32 blocked = 0;
	u32 callbackFail = 0;
	u32 tooFar = 0;
#endif
	if(StartNode < EndNode)
	{
		// get the first node's link nodes
		DMAAllLinkNodes(&(m_LocalNodes[StartNode]), m_LocalLinkIndex); 
	}

	for (Node = StartNode; Node < EndNode; Node++)
	{
		CPathNode	*pNode = &(m_LocalNodes[Node]);
		
		// Start grabbing the next node's link node
		if(Node+1 < EndNode)
		{
			DMAAllLinkNodes(&(m_LocalNodes[Node+1]), GetNextLocalLinkIndex());
		}
		
		if(pNode->m_startIndexOfLinks > (m_LocalLinkOffsets[m_CurrentBatchSlot] + MAX_TEMP_LINKS)) // test if we're done with the current batch slot
			DMANextBatchOfLinks(Region); // start the DMA for the next batch of links

		for(int i = 0; i < CPathFind::MAX_GPS_DISABLED_ZONES; ++i)
		{
			if(bForGps && m_LocalAsyncParams->m_gpsDisabledZones[i].m_iGpsBlockedByScriptID)
			{
				if( pNode->m_coorsX < m_LocalAsyncParams->m_gpsDisabledZones[i].m_iGpsBlockingRegionMinX || 
					pNode->m_coorsX > m_LocalAsyncParams->m_gpsDisabledZones[i].m_iGpsBlockingRegionMaxX ||
					pNode->m_coorsY < m_LocalAsyncParams->m_gpsDisabledZones[i].m_iGpsBlockingRegionMinY || 
					pNode->m_coorsY > m_LocalAsyncParams->m_gpsDisabledZones[i].m_iGpsBlockingRegionMaxY )
				{				
					// no intersection
				}
				else
				{
					SPU_DEBUG_ONLY(blocked++);
					continue;
				}
			}
		}

		// Run our callback - actually does most of the search's work
		if(!FindClosestNodeLinkCB(pNode, (void*)pData))
		{
			SPU_DEBUG_ONLY(callbackFail++);
			continue;
		}

		Vector3	nodeCrs;
		pNode->GetCoors(nodeCrs);
		float Dist2ToNode = (nodeCrs - SearchCoors).Mag2();

		if( Dist2ToNode > closestDist2)
		{
			SPU_DEBUG_ONLY(tooFar++);
			continue;
		}

		closestDist2 = Dist2ToNode;
		pNodeFound->Set(Region, Node);

		// Move on to the next node
		IncrementLocalLinkIndex();
	}

	SPU_DEBUG_ONLY(spu_printf("Region: %u Blocked: %u Callback: %u TooFar: %u\n", Region, blocked, callbackFail, tooFar));

	*pClosestDist = closestDist2 > 0.0f ? Sqrtf(closestDist2) : 0.0f;
}


//////////////////////////////////////////////////////////////////////////
// Custom callback function for the GPS SPU job. Should match CGps::FindClosestNodeLinkCB
bool CPathFindGPSSpu::FindClosestNodeLinkCB(CPathNode * pNode, void * pData)
{
	CPathFind::TFindClosestNodeLinkGPS * pFindClosest = (CPathFind::TFindClosestNodeLinkGPS*)pData;

	if(pNode->IsPedNode())
		return false;	
	if(pNode->m_2.m_noGps && !pFindClosest->bIgnoreNoGps)	
		return false;
	if(pNode->m_2.m_waterNode && pFindClosest->bIgnoreWater)
		return false;
	if(pFindClosest->bIgnoreJunctions && pNode->IsJunctionNode())	
		return false;

	bool oneWayNode = false;
	if(pNode->NumLinks()==2 && GetNodesLink(pNode, 0) && GetNodesLink(pNode, 0)->IsOneWay() && GetNodesLink(pNode, 1) && GetNodesLink(pNode, 1)->IsOneWay())
	{
		oneWayNode = true;
	}
	if(pFindClosest->bIgnoreOneWay && oneWayNode)
		return false;

	Vector3 vNode, vOtherNode, vToOther, vClosest;
	pNode->GetCoors(vNode);
	
	if(pFindClosest->pBestNodeSoFar && (pFindClosest->fDistToClosestPointOnNearbyLinks*pFindClosest->fDistToClosestPointOnNearbyLinks*2) < (pFindClosest->vSearchOrigin - vNode).XYMag2())
	{
		// Skip this node, it's really far away - chances are it's links won't be good enough to justify testing it at all
		return true;
	}

	for(u32 l=0; l<pNode->NumLinks(); l++)
	{
		CPathNodeLink* link = GetNodesLink(pNode, l);
		if(link && link->m_1.m_bDontUseForNavigation && pFindClosest->bIgnoreNoNav)
			continue;

		const CPathNode * pOtherNode = link ? FindNodePointerSafe(link->m_OtherNode) : NULL;
		if(pOtherNode)
		{
			if(pFindClosest->bIgnoreJunctions && pOtherNode->IsJunctionNode())
				continue;

			pOtherNode->GetCoors(vOtherNode);
			vToOther = vOtherNode - vNode;
			if(vToOther.Mag2() > SMALL_FLOAT)
			{
				float fDot;
				if(pFindClosest->bIgnoreHeading)
				{
					fDot = 1.0f;
				}
				else
				{					
					Vector3 vToOtherUnit = link->m_1.m_LanesToOtherNode == 0 ? -vToOther : vToOther;

					vToOtherUnit.NormalizeFast();
					fDot = DotProduct(pFindClosest->vSearchDir, vToOtherUnit);
				}

				//	if(fDot >= 0.0f)
				{
					const float T = rage::geomTValues::FindTValueSegToPoint(vNode, vToOther, pFindClosest->vSearchOrigin);
					vClosest = vNode + (vToOther * T);
					Vector3 vDiff = (vClosest - pFindClosest->vSearchOrigin);
					float fOnLaneWeight = 1.f;
					float fMinDist = 1.25f;	// Can never be closer to a link than x meter unless... A min value makes the weights more sane for links crossing our path

					// We test here if we are on the actual lane for the link and if so we get a better fitnessvalue for this node
					if(fDot >= 0.25f)
					{
						fMinDist = 0.1f;	// 

						// Get the road widths.
						float drivableWidthOnLeft = 0.0f;
						float drivableWidthOnRight = 0.0f;
						float centralReservationWidth = static_cast<float>(link->m_1.m_Width);
						bool bAllLanesThoughCentre = (link->m_1.m_Width == ALL_LANES_THROUGH_CENTRE_FLAG_VAL);
						FindRoadBoundaries(link->m_1.m_LanesToOtherNode, link->m_1.m_LanesFromOtherNode, centralReservationWidth, link->m_1.m_NarrowRoad, bAllLanesThoughCentre, drivableWidthOnLeft, drivableWidthOnRight);

						Vector3 LinkRightVec = vToOther;
						LinkRightVec.Cross(ZAXIS);

						float fDistToLane = vDiff.XYMag();	// Yes sqrt here, think we save some cycles this way
						float fMinLaneDist = link->InitialLaneCenterOffset() - link->GetLaneWidth() * 0.5f;						
						float fMaxLaneDist = ((vDiff.Dot(LinkRightVec) > 0.f) ? drivableWidthOnRight : drivableWidthOnLeft);
						if (fMaxLaneDist > fDistToLane && fMinLaneDist < fDistToLane)
						{
							static dev_float DistEps = 0.6f;							
							fOnLaneWeight = DistEps;
						}
					}

					// Only consider this ZPenalty if the point is fairly close to us,
					dev_float fDistToConsider = 35.f;
					vDiff.z = Min(abs(vDiff.z), fDistToConsider);
					vDiff.z *= pFindClosest->fDistZPenalty;

					float fDist = Max(vDiff.Mag(), fMinDist);
					float fDistXY = Max(vDiff.XYMag(), fMinDist);

					// Factor in the distance from the actual node to the position on the link
					static dev_float fPenaltyAlongSeg = 0.25f;
					fDist += (vClosest - vNode).Mag() * fPenaltyAlongSeg;

					float fHeadingPenalty = (pNode->IsHighway() ? pFindClosest->fHeadingPenalty * 0.5f : pFindClosest->fHeadingPenalty);

					// Apply penalty for heading
					if(fDot >= 0.25f)
						fDist += (1.0f - fDot) * fHeadingPenalty;
					else
						fDist += fHeadingPenalty * 2.f;

					static dev_float fOffNodeWeight = 0.75f;	
					float fOffPenaltyParent = ((pNode->m_2.m_switchedOff || pOtherNode->m_2.m_switchedOff) ? fOffNodeWeight : 0.f);

					fDist *= fOnLaneWeight;
					fDist += fDistXY * fOffPenaltyParent;

					if(fDist < (pFindClosest->fDistToClosestPointOnNearbyLinks))
					{
						pFindClosest->fDistToClosestPointOnNearbyLinks = fDist;
						pFindClosest->vClosestPointOnNearbyLinks = vClosest;
						pFindClosest->pBestNodeSoFar = FindPPUNodePointer(pNode);
						pFindClosest->resultNodeAddress = pNode->m_address;
						pNode->GetCoors(m_CurrentBestNodeCoors);

						// if this is a one-way node, store it's fitness
						// or if we're ignoring one-way nodes, as we might need this fitness
						m_CurrentBestNodeIsOneWay = oneWayNode; // store if this node is One-way
						if(oneWayNode || pFindClosest->bIgnoreOneWay)
						{
							m_CurrentBestNodeFitness = CalculateFitnessValueForNode(pFindClosest->vSearchOrigin, pFindClosest->vSearchDir, pNode);							
						}
						else
						{
							m_CurrentBestNodeFitness = -1.f; // invalid fitness
						}
						
					}
				}
			}
		}
	}

	return true;
}

// Get the link for the given node - can return null if the link isn't loaded
CPathNodeLink* CPathFindGPSSpu::GetNodesLink(const CPathNode* pNode, const s32 iLink) const
{
	Assertf(iLink >= 0 && iLink < (s32)pNode->NumLinks(), "Link out of range iLink: %u numLinks: %u", (u32)iLink, pNode->NumLinks());

	for(u32 i = 0; i < LINK_BATCH_NUMBER; i++)
	{
		if(m_LocalLinkOffsets[i] != -1 && (pNode->m_startIndexOfLinks + iLink) >= m_LocalLinkOffsets[i] && (pNode->m_startIndexOfLinks - m_LocalLinkOffsets[i] + iLink) < MAX_TEMP_LINKS) // link is in this batch
		{
			// Make sure DMA is done
			//sysDmaWait(1 << GET_TEMP_LINK_DMA_TAG(i));
			return &m_LocalLinks[i][(pNode->m_startIndexOfLinks - m_LocalLinkOffsets[i] + iLink)];
		}
	}
#if DEBUG_PATHFIND_NODE_SEARCH_SPU
	spu_printf("Didn't have the links loaded for the given node %u %u - looking for index %u + %u = %u\n", pNode->GetAddrRegion(), pNode->GetAddrIndex(), (u32)pNode->m_startIndexOfLinks, (u32)iLink, (u32)pNode->m_startIndexOfLinks+iLink);
	for(u32 i = 0; i < LINK_BATCH_NUMBER; i++)
	{
		spu_printf("Offset %u: %u\n", i, (u32)m_LocalLinkOffsets[i]);
	}
#endif
	return NULL;
}


// Test to see if the link is to a node we have in local storage already
bool CPathFindGPSSpu::IsLinkNodeLocal(CPathNode* pNode, s32 iLink) const
{
	CPathNodeLink* link = GetNodesLink(pNode, iLink);
	return m_LocalNodeRegionIndex != -1 && link && FindLocalRegionIndex(link->m_OtherNode.GetRegion()) == m_LocalNodeRegionIndex;
}

// Get a pointer to a node in local storage. Will find the node in our temporary node buffer as well
CPathNode* CPathFindGPSSpu::FindNodePointerSafe(const CNodeAddress Node) const
{
	if(Node.IsEmpty())
	{
		return NULL;
	}
	
	s32 localRegionIndex = FindLocalRegionIndex(Node.GetRegion());
	if(localRegionIndex == -1)
		return NULL; // region not loaded

	 if(m_LocalNodeRegionIndex != localRegionIndex) // if we're trying to access nodes we don't have loaded
	 {
		 SPU_DEBUG_ONLY(spu_printf("temp link count: %u\n",m_TempLinkNodeCount[m_LocalLinkIndex]));
		 for(u32 i = 0; i < m_TempLinkNodeCount[m_LocalLinkIndex]; i++)
		 {
			if(m_TempLinkNodeAddresses[m_LocalLinkIndex][i] == Node)
			{
				SPU_DEBUG_ONLY(spu_printf("Waiting on DMA\n"));
				sysDmaWait(1 << m_TempLinkNodeTags[m_LocalLinkIndex][i]); // wait on the DMA
#if DEBUG_PATHFIND_NODE_SEARCH_SPU
				Vector3 v;
				m_TempLinkNodes[m_LocalLinkIndex][i].GetCoors(v);
				spu_printf("DMA Complete - our node: %u %u X:%.1f Y:%.1f Z:%.1f\n", Node.GetRegion(), Node.GetIndex(), v.GetX(), v.GetY(), v.GetZ());
#endif
				
				return &m_TempLinkNodes[m_LocalLinkIndex][i];
			}
		 }
		 return NULL; // not found
	 }
	 else
	 {
		return &m_LocalNodes[Node.GetIndex()];
	 }
}

// Takes in a SPU local storage pointer and finds the appropriate ppu memory pointer
CPathNode* CPathFindGPSSpu::FindPPUNodePointer(CPathNode* lsNode)
{
	u32 region = lsNode->GetAddrRegion();
	u32 index = lsNode->GetAddrIndex();

	s32 localRegionIndex = FindLocalRegionIndex(region);
	if(localRegionIndex == -1)
		return NULL; // trying to convert a pointer for a region that isn't loaded

	return &m_LocalRegions[localRegionIndex]->aNodes[index];
}

// DMA the next set of links for the given region. We DMA the links in MAX_TEMP_LINKS chunks
void CPathFindGPSSpu::DMANextBatchOfLinks(u32 Region)
{
	u32 batchSlotToLoad = m_CurrentBatchSlot;
	s32 localRegionIndex = FindLocalRegionIndex(Region);
	if(localRegionIndex == -1)
	{		
		return; // region isn't loaded
	}

	// compute new link interval
	u32 linkStartIndex = m_LocalLinkCurrentMax;
	
	if(m_LocalRegions[localRegionIndex]->NumLinks >= 0 && linkStartIndex < (u32)m_LocalRegions[localRegionIndex]->NumLinks)
	{
		SPU_DEBUG_ONLY(spu_printf("Loading more links start index: %u slot: %u\n", linkStartIndex, batchSlotToLoad));
		u64 foreignLink = (u64)(m_LocalRegions[localRegionIndex]->aLinks + linkStartIndex);
		SPU_DEBUG_ONLY(spu_printf("Link DMA: ls: %x ppu: %x size: %u\n", (u32)(void*)m_LocalLinks[batchSlotToLoad], (u32)foreignLink, (u32)(sizeof(CPathNodeLink) * MAX_TEMP_LINKS)));
		sysDmaLargeGetAndWait(m_LocalLinks[batchSlotToLoad], foreignLink, (u32)(sizeof(CPathNodeLink) * MAX_TEMP_LINKS), GET_TEMP_LINK_DMA_TAG(batchSlotToLoad));
		m_LocalLinkCurrentMax += MAX_TEMP_LINKS;
		m_LocalLinkOffsets[batchSlotToLoad] = linkStartIndex;
	}
	else
	{
		SPU_DEBUG_ONLY(spu_printf("No more links to load\n"));
		m_LocalLinkOffsets[batchSlotToLoad] = -1;
	}

	m_CurrentBatchSlot++;
	if(m_CurrentBatchSlot == LINK_BATCH_NUMBER)
		m_CurrentBatchSlot = 0;
}

// DMA all of the nodes on the links from the given node (if they are not already in local storage)
// Uses the linkIndex paramater to allow multiple nodes to DMA their link nodes concurrently
void CPathFindGPSSpu::DMAAllLinkNodes(CPathNode* pNode, u32 linkIndex)
{
	m_TempLinkNodeCount[linkIndex] = 0;

	s32 localRegionIndex = FindLocalRegionIndex(pNode->GetAddrRegion());
	if(localRegionIndex == -1 || m_LocalNodeRegionIndex != localRegionIndex) // if we're trying to load the links for a different region
		return;	
	u32 linkCount = pNode->NumLinks();
		
	for(u32 i =0; i < linkCount; i++)
	{
		CPathNodeLink* link = GetNodesLink(pNode,i);
		if(!IsLinkNodeLocal(pNode, i))
			DMALinkNode(link ? &link->m_OtherNode : NULL,linkIndex);
	}
}


// Grab a given link's node
void CPathFindGPSSpu::DMALinkNode(const CNodeAddress* Node, u32 linkIndex)
{
	if(!Node)
		return;
	// test for bogus node
	if(Node->GetRegion() > PATHFINDREGIONS)
	{
		SPU_DEBUG_ONLY(spu_printf("Throwing out bogus Node %u %u", Node->GetRegion(), Node->GetIndex()));
		return;
	}
	// check to see if we've already loaded it
	for(u32 i = 0; i < m_TempLinkNodeCount[linkIndex]; i++)
	{
		if(m_TempLinkNodeAddresses[linkIndex][i] == *Node)
		{
			return; // we already have this node in temporary storage
		}
	}
	s32 localRegionIndex = FindLocalRegionIndex(Node->GetRegion());
	if(localRegionIndex == -1) // region isn't loaded
	{
		SPU_DEBUG_ONLY(spu_printf("Loading a region because we want a node from it - Region %u Node: %u\n", Node->GetRegion(), Node->GetIndex()));
		LoadForeignRegion(Node->GetRegion());
	}
	localRegionIndex = FindLocalRegionIndex(Node->GetRegion());
	if(localRegionIndex == -1)
		return;
	// DMA the node
	u8 tag = GetNextTempNodeTag();
	 
	sysDmaGet((void*)&m_TempLinkNodes[linkIndex][m_TempLinkNodeCount[linkIndex]], (u64)&m_LocalRegions[localRegionIndex]->aNodes[Node->GetIndex()], (u32)sizeof(CPathNode), tag);
	m_TempLinkNodeAddresses[linkIndex][m_TempLinkNodeCount[linkIndex]] = *Node;
	m_TempLinkNodeTags[linkIndex][m_TempLinkNodeCount[linkIndex]] = tag;
	m_TempLinkNodeCount[linkIndex]++;	
	Assertf(m_TempLinkNodeCount[linkIndex] < MAX_TEMP_NODES, "Too many temp nodes, increase MAX_TEMP_NODES");
	
}

// Find the local index of a given region in local storage
s32 CPathFindGPSSpu::FindLocalRegionIndex(s32 region) const
{
	Assertf(region < PATHFINDREGIONS, "Trying to find the index for region %u, which is greater than PATHFINDREGIONS (%u)", (u32)region, PATHFINDREGIONS);
	
	if(region < PATHFINDREGIONS)
	{
		return m_RegionNumbers[region]; // direct access here - will be -1 if the region hasn't been loaded yet
	}
	else
	{
		return -1; // Trying to use a bogus region number - return -1 here to not crash
	}
	
}

// Load a region from PPU memory into local storage
void CPathFindGPSSpu::LoadForeignRegion(s32 region)
{
	if(region == -1)
		return;
	if(FindLocalRegionIndex(region)!=-1)
		return; // already loaded

	// Get our region
	SPU_DEBUG_ONLY(spu_printf("Region %u we care about: %u %x\n", m_LocalRegionCount, region, (u32)(void*)foreignRegions[region]));
	if(!foreignRegions[region])
	{
		return;
	}
	m_RegionNumbers[region] = m_LocalRegionCount; // store the local index in the region array
	sysDmaGetAndWait(m_LocalRegions[m_LocalRegionCount], (u64)foreignRegions[region], SPU_PAD_SIZE(sizeof(CPathRegion)), 0);
	SPU_DEBUG_ONLY(spu_printf("Region %u we care about: %u (%x) Car Nodes: %u (%x) Links: %u (%x)\n", m_LocalRegionCount, region, (u32)(void*)m_LocalRegions[m_LocalRegionCount], m_LocalRegions[m_LocalRegionCount]->NumNodesCarNodes,(u32)(void*)m_LocalRegions[m_LocalRegionCount]->aNodes, m_LocalRegions[m_LocalRegionCount]->NumLinks, (u32)(void*)m_LocalRegions[m_LocalRegionCount]->aLinks));
	m_LocalRegionCount++;
}

// Load nodes from PPU memory into local storage
void CPathFindGPSSpu::LoadForeignNodes(s32 region)
{
	SPU_DEBUG_ONLY(spu_printf("Loading nodes for region %u\n", region));
	if(m_LocalNodes) // we only keep one batch of nodes in local storage at a time
	{
		m_HeapAllocator.Free(m_LocalNodes);
		m_LocalNodes = NULL;
		m_LocalNodeRegionIndex = -1;
	}
	s32 localRegionIndex = FindLocalRegionIndex(region);
	if(localRegionIndex == -1)
	{
		SPU_DEBUG_ONLY(spu_printf("Local region index not found for Region %u", (u32)region));
		return;
	}
	// Load it

	m_LocalNodes = (CPathNode*)m_HeapAllocator.Allocate(static_cast<size_t>(SPU_PAD_SIZE(sizeof(CPathNode)*m_LocalRegions[localRegionIndex]->NumNodesCarNodes)));
	SPU_DEBUG_ONLY(spu_printf("Node Allocation of %u bytes to %x - Available: %u Used: %u\n", static_cast<u32>(SPU_PAD_SIZE(sizeof(CPathNode)*m_LocalRegions[localRegionIndex]->NumNodesCarNodes)), (u32)(void*)m_LocalNodes, (u32)m_HeapAllocator.GetMemoryAvailable(), (u32)m_HeapAllocator.GetMemoryUsed()));
	// DMA the region we care about
	SPU_DEBUG_ONLY(spu_printf("Nodes DMA: ls: %x fs: %x size: %u\n",(u32)(void*)m_LocalNodes, (u32)(void*)m_LocalRegions[localRegionIndex]->aNodes,  SPU_PAD_SIZE(static_cast<u32>(sizeof(CPathNode) * m_LocalRegions[localRegionIndex]->NumNodesCarNodes))));
	Assertf(m_LocalNodes, "Allocation failed, we're going to crash");
	sysDmaLargeGetAndWait((void*)m_LocalNodes, (u64)m_LocalRegions[localRegionIndex]->aNodes, SPU_PAD_SIZE(sizeof(CPathNode) * m_LocalRegions[localRegionIndex]->NumNodesCarNodes), 0);
	m_LocalNodeRegionIndex = localRegionIndex;
}

// Copy of CPathFind::FindRoadBoundaries
void CPathFindGPSSpu::FindRoadBoundaries(s32 lanesTo, s32 lanesFrom, float centralReservationWidth, bool bNarrowRoad, bool bAllLanesThoughCentre, float& roadWidthOnLeft_out, float& roadWidthOnRight_out) const
{
	float laneWidth = LANEWIDTH;
	if(bNarrowRoad)
	{ 
		laneWidth = LANEWIDTH_NARROW;
	}

	if(bAllLanesThoughCentre)
	{
		roadWidthOnLeft_out = rage::Max(lanesFrom * laneWidth, lanesTo * laneWidth) * 0.5f;
		roadWidthOnRight_out = roadWidthOnLeft_out;
	}
	else if(lanesTo == 0)
	{
		roadWidthOnLeft_out = (lanesFrom * laneWidth) * 0.5f;
		roadWidthOnRight_out = roadWidthOnLeft_out;
	}
	else if(lanesFrom == 0)
	{
		roadWidthOnLeft_out = (lanesTo * laneWidth) * 0.5f;
		roadWidthOnRight_out = roadWidthOnLeft_out;
	}
	else
	{
		roadWidthOnLeft_out = lanesFrom * laneWidth + (centralReservationWidth * 0.5f);
		roadWidthOnRight_out = lanesTo * laneWidth + (centralReservationWidth * 0.5f);
	}
}



#elif !__PPU  // Not __SPU either

void PathfindNodeSearchSPU(::rage::sysTaskParameters& taskParams)
{
	CPathFind::FindNodeClosestToCoorsAsyncParams* params = (CPathFind::FindNodeClosestToCoorsAsyncParams*)taskParams.Input.Data;

	ThePaths.FindNodeClosestToCoors(params->m_SearchCoors, params->m_CallbackFunction, params->m_SearchData, params->m_CutoffDistance, params->m_bForGPS);

	CPathFind::TFindClosestNodeLinkGPS* srcClosestData = static_cast<CPathFind::TFindClosestNodeLinkGPS*>(params->m_SearchData);
	CPathNode * pSourceNode = srcClosestData->pBestNodeSoFar;
	if (!pSourceNode)
	{ 
		return;
	}	

	// Test for finding one-way nodes
	if (pSourceNode->NumLinks()==2 && ThePaths.GetNodesLink(pSourceNode, 0).IsOneWay() && ThePaths.GetNodesLink(pSourceNode, 1).IsOneWay())
	{	
		// calculate node fitness
		float FitnessFirstNode = ThePaths.CalculateFitnessValueForNode(srcClosestData->vSearchOrigin, srcClosestData->vSearchDir, pSourceNode);
		if (FitnessFirstNode < params->m_fFitnessThreshold )
		{
			// test to see if there is a better node on a non-one way street

			CPathFind::TFindClosestNodeLinkGPS ignoreOneWayData;

			ignoreOneWayData.Reset();
			ignoreOneWayData.vSearchOrigin = srcClosestData->vSearchOrigin;
			ignoreOneWayData.vSearchDir = srcClosestData->vSearchDir;
			ignoreOneWayData.fDistZPenalty = srcClosestData->fDistZPenalty;
			ignoreOneWayData.bIgnoreHeading = srcClosestData->bIgnoreHeading;
			ignoreOneWayData.fHeadingPenalty = srcClosestData->fHeadingPenalty;
			ignoreOneWayData.bIgnoreOneWay = true;
		
			ThePaths.FindNodeClosestToCoors(params->m_SearchCoors, params->m_CallbackFunction, &ignoreOneWayData, params->m_CutoffDistance, params->m_bForGPS);
			CPathNode * pSourceNodeIgnoreOneWay = ignoreOneWayData.pBestNodeSoFar;
			if (pSourceNodeIgnoreOneWay)
			{			
				float FitnessSecondNode = ThePaths.CalculateFitnessValueForNode(srcClosestData->vSearchOrigin,  srcClosestData->vSearchDir, pSourceNodeIgnoreOneWay);
				if( FitnessFirstNode > FitnessSecondNode * params->m_fTwoLinkBonusFitness)
				{
					Vector3 vSourceNode, vSourceNodeIgnoreOneWay;
					pSourceNode->GetCoors(vSourceNode);
					pSourceNodeIgnoreOneWay->GetCoors(vSourceNodeIgnoreOneWay);

					pSourceNode =
						(vSourceNode - vSourceNodeIgnoreOneWay).XYMag2() < params->m_fPreferBothWayProximitySqrXY &&
						IsClose(vSourceNode.z, vSourceNodeIgnoreOneWay.z, params->m_fPreferBothWayMaxZ) ?
	pSourceNodeIgnoreOneWay : pSourceNode;

					if (pSourceNode == pSourceNodeIgnoreOneWay)
						srcClosestData->vClosestPointOnNearbyLinks = ignoreOneWayData.vClosestPointOnNearbyLinks;

					srcClosestData->pBestNodeSoFar = pSourceNode;
				}
			}
		}
	}

}

#endif // __SPU
