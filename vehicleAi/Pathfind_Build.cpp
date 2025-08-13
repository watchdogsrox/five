#include "Pathfind_Build.h"

//framework headers
#include "fwscene/stores/mapdatastore.h"
#include "fwscene/world/WorldLimits.h"
#include "fwmaths/geomutil.h"
#include "fwsys/fileexts.h"


#include "control/trafficLights.h"
#include "objects/dummyobject.h"
#include "objects/object.h"
#include "Objects/ProcObjects.h"
#include "physics/physics.h"
#include "renderer/water.h"
#include "scene/FileLoader.h"
#include "scene/streamer/SceneStreamerMgr.h"
#include "streaming/streaming.h"
#include "system/fileMgr.h"
#include "vehicles/virtualroad.h"

AI_OPTIMISATIONS()
AI_VEHICLE_OPTIMISATIONS()
VEHICLE_OPTIMISATIONS()

RAGE_DEFINE_CHANNEL(pathbuild)

PARAM(nosnap, "[paths] don't snap nodes to collision during build process.");
PARAM(nocamber, "[paths] don't calculate camber (tilt) values during build process.");
PARAM(nosortnodes, "[paths] don't sort nodes during build process, only for debugging -buildpaths as this will really fuck up in-game algorithms.");
PARAM(generateIsland, "[paths] generating nodes for heist island");

#define MERGEDISTCAR (1.0f)		// Nodes are considered at the same coordinate
#define MERGEDISTPED (1.0f)		// within this distance


CPathFindBuild::CPathFindBuild()
{
	PathNodeIndex = 0;
	ClearTempNodes();
//Not required //Init(INIT_CORE);
	AllocateMemoryForBuildingPaths();
}

CPathFindBuild::~CPathFindBuild()
{
	FreeMemoryForBuildingPaths();
}


void CPathFindBuild::ClearTempNodes()
{
	TempNodes = NULL;
	TempLinks = NULL;
	NumTempNodes = NumTempLinks = 0;
}


void CPathFindBuild::AllocateMemoryForBuildingPaths()
{
	// Create the region array
	// Allocate some memory to play with so that we can build the files as required by the streaming.
	for (s32 Region = 0; Region < PATHFINDREGIONS; Region++)
	{
		//Assert(!apRegions[Region]);
		apRegions[Region] = rage_aligned_new(16) CPathRegion();

		apRegions[Region]->aNodes = (CPathNode *) sysMemAllocator::GetMaster().Allocate(MAXNODESPERREGION * sizeof(CPathNode), 16, MEMTYPE_RESOURCE_VIRTUAL);
		Assert(apRegions[Region]->aNodes);
		memset(apRegions[Region]->aNodes, 0, MAXNODESPERREGION * sizeof(CPathNode));
		for (s32 T = 0; T < MAXNODESPERREGION; T++)
		{
			apRegions[Region]->aNodes[T].m_distanceToTarget = PF_VERYLARGENUMBER;
		}

		apRegions[Region]->aLinks = (CPathNodeLink *) sysMemAllocator::GetMaster().Allocate( (MAXLINKSPERREGION) * sizeof(CPathNodeLink) , 16, MEMTYPE_RESOURCE_VIRTUAL);
		Assert(apRegions[Region]->aLinks);

		// Set all the adjacent nodes to emptyAddr. This is important because the dynamic adjacent nodes(at the end)
		// should be set to emptyAddr so that we know they are not being used yet.
		// Make sure the extra adjacent nodes(for dynamic links)are set to emptyAddr.
		for(s32 regionLinkIndexToClear = 0; regionLinkIndexToClear <(MAXLINKSPERREGION); regionLinkIndexToClear++)
		{
			apRegions[Region]->aLinks[regionLinkIndexToClear].m_OtherNode.SetEmpty();
		}

		apRegions[Region]->aVirtualJunctions = (CPathVirtualJunction *) sysMemAllocator::GetMaster().Allocate(MAXJUNCTIONSPERREGION * sizeof(CPathVirtualJunction), 16, MEMTYPE_RESOURCE_VIRTUAL);
		Assert(apRegions[Region]->aVirtualJunctions);

		apRegions[Region]->aHeightSamples = (u8*) sysMemAllocator::GetMaster().Allocate(MAXHEIGHTSAMPLESPERREGION * sizeof(u8), 16, MEMTYPE_RESOURCE_VIRTUAL);
		Assert(apRegions[Region]->aHeightSamples);

		apRegions[Region]->NumNodes = 0;
		apRegions[Region]->NumNodesCarNodes = 0;
		apRegions[Region]->NumNodesPedNodes = 0;
		apRegions[Region]->NumLinks = 0;
		apRegions[Region]->NumJunctions = 0;
		apRegions[Region]->NumHeightSamples = 0;
	}

	// allocate the TempNode and TempLink arrays
	TempNodes = (CTempNode*)sysMemAllocator::GetMaster().Allocate(sizeof(CTempNode[PF_TEMPNODES]), 16, MEMTYPE_RESOURCE_VIRTUAL);
	Assert(TempNodes);
	memset(TempNodes, 0, PF_TEMPNODES * sizeof(CTempNode));
	TempLinks = (CTempLink*)sysMemAllocator::GetMaster().Allocate(sizeof(CTempLink[PF_TEMPLINKS]), 16, MEMTYPE_RESOURCE_VIRTUAL);
	Assert(TempLinks);
	memset(TempLinks, 0, PF_TEMPLINKS * sizeof(CTempLink));

	NumTempNodes = NumTempLinks = 0;
}

void CPathFindBuild::FreeMemoryForBuildingPaths()
{
	// Free the memory we have allocated so that the normal streaming stuff can now take over.
	for (s32 Region = 0; Region < PATHFINDREGIONS; Region++)
	{
		apRegions[Region]->Unload();
		delete apRegions[Region];
		apRegions[Region] = NULL;
	}

	sysMemAllocator::GetMaster().Free(TempNodes);
	sysMemAllocator::GetMaster().Free(TempLinks);
	ClearTempNodes();
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : SavePathFindData
// PURPOSE :  Save out the data in pathfind struct
/////////////////////////////////////////////////////////////////////////////////

void CPathFindBuild::SavePathFindData(void)
{
#if !__FINAL

	Displayf("Buildpaths:SavePathFindData\n");

	for (s32 Region = 0; Region < PATHFINDREGIONS; Region++)
	{
		s32 iRegion = Region;

		if (PARAM_generateIsland.Get())
		{
			iRegion += ThePaths.sm_iHeistsSize;
		}

		char tmp[50];
		CFileMgr::SetDir("common:/data/paths/");
		sprintf(tmp, "nodes%d.ind", iRegion);
		apRegions[Region]->SaveXML(tmp);

		CFileMgr::SetDir("");
	}

#endif
}

void CPathFindBuild::IdentifySlipLaneNodes()
{
	for (s32 Region = 0; Region < PATHFINDREGIONS; Region++)
	{
		if(!apRegions[Region] || !apRegions[Region]->NumNodes)
			continue;

		for (s32 Node = 0; Node < apRegions[Region]->NumNodes; Node++)
		{
			Vector3 vNodePos;			
			CPathNode * pNode = &((apRegions[Region]->aNodes)[Node]);
			pNode->GetCoors(vNodePos);

			for(u32 iLink = 0; iLink < pNode->NumLinks(); iLink++)
			{
				//CPathNodeLink link = GetNodesLink(pNode, iLink);
				ASSERT_ONLY(CNodeAddress linkNodeAddr = GetNodesLinkedNodeAddr(pNode, iLink);)
				Assert(IsRegionLoaded(linkNodeAddr));
				CPathNode* pLinkedNode = GetNodesLinkedNode(pNode, iLink);

				s32 iSlipLane = CPathNodeRouteSearchHelper::GetSlipLaneNodeLinkIndex(pLinkedNode, pNode->m_address, *this);
				if(iSlipLane != -1)
				{
					// We've identified the slip node
					CPathNode * pSlipNode = GetNodesLinkedNode(pLinkedNode, iSlipLane);
					pSlipNode->m_1.m_slipLane = true;

					// Now iterate along the links towards the junction node, marking all nodes between also as sliplanes
					// If we find 8 nodes before hitting the junction, then bail out
					CPathNode* pLastNode = pSlipNode;
					CPathNode* pNextNode = pLastNode;
					s32 iMaxSlipNodes = 16;
					do
					{
						if (FindNumberNonShortcutLinksForNode(pLastNode) != 2)
							break;

						CPathNode* pCurrentNode = pNextNode;
						//pNextNode = (GetNodesLinkedNode(pLastNode, 0) == pLastNode) ? GetNodesLinkedNode(pLastNode, 1) : GetNodesLinkedNode(pLastNode, 0);
						for (int i = 0; i < pCurrentNode->NumLinks(); i++)
						{
							//don't consider shortcut links
							if (GetNodesLink(pCurrentNode, i).IsShortCut())
							{
								continue;
							}

							if (GetNodesLinkedNode(pCurrentNode, i) == pLastNode)
							{
								continue;
							}

							//prevent backtracking to the slipj
							if (GetNodesLinkedNode(pCurrentNode, i) == pLinkedNode)
							{
								continue;
							}
							
							pNextNode = GetNodesLinkedNode(pCurrentNode, i);
							Assert(pNextNode);
							break;
						}

						if(pNextNode->IsJunctionNode() || FindNumberNonShortcutLinksForNode(pNextNode) != 2 || pNextNode->m_1.m_slipLane)
						{
							break;
						}

						pNextNode->m_1.m_slipLane = true;
						pLastNode = pCurrentNode;
						
					} while(iMaxSlipNodes-- > 0);
				}
			}
		}
	}
}

// FUNCTION : PreprocessJunctions
// PURPOSE : Perform preprocessing for junctions:
//	1) set SPECIAL_TRAFFIC_LIGHT special-function on all nodes which are entrances to templated junctions

void CPathFindBuild::PreprocessJunctions()
{ 
	Vector3 vNodePos;

	for(int j=0; j<CJunctions::GetNumJunctionTemplates(); j++)
	{
		CJunctionTemplate & temp = CJunctions::GetJunctionTemplate(j);

		const bool bEmpty = (temp.m_iFlags & CJunctionTemplate::Flag_NonEmpty)==0;
		const bool bRailwayCrossing = (temp.m_iFlags & CJunctionTemplate::Flag_RailwayCrossing)!=0;

		if( !bEmpty )
		{
			//----------------------------------------------------------------------------------------------------
			// For all junction nodes in templated junctions, ensure that the "m_qualifiesAsJunction" flag is set

			for(int n=0; n<temp.m_iNumJunctionNodes; n++)
			{
				CPathNode * pClosestJunctionNode = NULL;
				float fClosestDistXY = FLT_MAX;

				for (s32 Region = 0; (Region < PATHFINDREGIONS); Region++)
				{
					if(!apRegions[Region] || !apRegions[Region]->NumNodes)
						continue;

					for (s32 Node = 0; (Node < apRegions[Region]->NumNodes); Node++)
					{
						CPathNode * pNode = &((apRegions[Region]->aNodes)[Node]);
						if(pNode->HasSpecialFunction())
							continue;
						if(pNode->NumLinks() <= 2 && !bRailwayCrossing )
							continue;

						pNode->GetCoors(vNodePos);

						float fDistXY = (vNodePos - temp.m_vJunctionNodePositions[n]).XYMag();
						if( vNodePos.IsClose( temp.m_vJunctionNodePositions[n], 2.0f) && fDistXY < fClosestDistXY )
						{
							fClosestDistXY = fDistXY;
							pClosestJunctionNode = pNode;
							break;
						}
					}
				}

				if(pClosestJunctionNode)
					pClosestJunctionNode->m_2.m_qualifiesAsJunction = true;
			}

			for(int e=0; e<temp.m_iNumEntrances; e++)
			{
				// For each entrance to this junction, find which pathnode corresponds to it
				CJunctionTemplate::CEntrance & entrance = temp.m_Entrances[e];
				bool bNodeFound = false;

				for (s32 Region = 0; (Region < PATHFINDREGIONS) && !bNodeFound; Region++)
				{
					if(!apRegions[Region] || !apRegions[Region]->NumNodes)
						continue;

					for (s32 Node = 0; (Node < apRegions[Region]->NumNodes) && !bNodeFound; Node++)
					{
						CPathNode * pNode = &((apRegions[Region]->aNodes)[Node]);
						if(pNode->HasSpecialFunction())
							continue;
						//if(pNode->NumLinks()!=2)
						//	continue;

						pNode->GetCoors(vNodePos);

						float fEntranceNodeDistXZ = (vNodePos - entrance.m_vNodePosition).XYMag();
						if(fEntranceNodeDistXZ < CJunctions::ms_fEntranceNodePosEps &&
							IsClose(vNodePos.z, entrance.m_vNodePosition.z, 3.0f))
						{
							pNode->m_1.m_specialFunction = SPECIAL_TRAFFIC_LIGHT;
							bNodeFound = true;
							break;
						}
					}
				}
			}
		}
	}
}


bool SortByCarThenYCoord(const CTempNode& a, const CTempNode& b)
{
	if (a.IsPedNode() == b.IsPedNode())
	{
		return a.m_Coors.y < b.m_Coors.y; // both ped or both car, sort by Y val
	}
	else
	{
		return !a.IsPedNode() && b.IsPedNode(); // a "less than" b iff a is a car, b is a ped
	}
}


int QSortByCarThenYCoord(const void* a, const void* b)
{
	const CTempNode* aa = static_cast<const CTempNode*>(a);
	const CTempNode* bb = static_cast<const CTempNode*>(b);
	if(SortByCarThenYCoord(*aa, *bb))
	{
		return -1;
	}
	else if(SortByCarThenYCoord(*bb, *aa))
	{
		return 1;
	}
	else
	{
		return 0;
	}
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : SortPathNodes
// PURPOSE :  Sort the nodes so that the car nodes come first and then the ped nodes.
/////////////////////////////////////////////////////////////////////////////////
void CPathFindBuild::SortPathNodes(	CTempNode *TempNodeList, CTempLink *UNUSED_PARAM(TempLinkList),
							  s32 NumTempNodes, s32 UNUSED_PARAM(NumTempLinks))
{
	Displayf("Buildpaths:SortPathNodes\n");

	// For some reason, this didn't output the right order, the array would mostly be sorted,
	// but maybe 10% or so of the nodes would be in a slightly incorrect order relative to their neighbors:
	//	std::sort(&TempNodeList[0], &TempNodeList[NumTempNodes], SortByCarThenYCoord);
	// qsort() seems to work without any problems, so I guess we'll use that as a workaround:
	qsort((void*)TempNodeList, NumTempNodes, sizeof(CTempNode), QSortByCarThenYCoord);
}


void CPathFindBuild::PreparePathData(void)
{
	USE_MEMBUCKET(MEMBUCKET_GAMEPLAY);
	Displayf("PreparePathData\n");

	{		
		// File wasn't here. This means we have to build the (streaming) files from the map data.
		// We can only do this if there actually is data read in for the
		// paths. If there is no data there is no point in preparing it.
		if (TempNodes && TempLinks)
		{

			// Add the nodes automatically generated by open areas.
			// The areas are specified in max as a circular chain of nodes. (Not necessarily convex)
			AddOpenSpaceNodes();

			// We process the cars and pedestrian graphs in the same list.
			// We have to sort the nodes first so that the car nodes come first and the ped nodes after.
			if(!PARAM_nosortnodes.Get())
			{
				SortPathNodes(TempNodes, TempLinks, NumTempNodes, NumTempLinks);
			}

			PreparePathDataForType(TempNodes, TempLinks, NumTempNodes, NumTempLinks, MERGEDISTCAR);
			// Total number of car nodes

			CountFloodFillGroups();

			bool doSnap = true;
			bool doCamber = true;
			bool doJunctions = true;
#if !__FINAL
			if(PARAM_nosnap.Get())
			{
				doSnap = false;
			}
			if(PARAM_nocamber.Get())
			{
				doCamber = false;
			}
#endif // !__FINAL
			if(doSnap || doCamber || doJunctions)
			{
				SnapToGround(doSnap, doCamber, doJunctions);
			}

#ifdef DEBUG
#if __WIN32PC	// Don't do this on PS2 (slow)
			CheckGrid();
#endif
#endif

#if __DEV

			// Build the distantlights also, since it uses the same data
			m_DistantLights.BuildData(TempNodes, TempLinks, NumTempLinks);
			m_DistantLights2.BuildData(TempNodes, TempLinks, NumTempLinks);
			
#endif	//__DEV

		}

#if __DEV
		s32 Region;
		// Do a check to make sure nodes point 2 ways.
		for (Region = 0; Region < PATHFINDREGIONS; Region++)
		{
			CheckPathNodeIntegrityForZone(Region);
		}
#endif	//__DEV

		// Save file out now
		SavePathFindData();
	}

	Displayf("Done with PreparePathData\n");
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : QualifiesAsJunction
// PURPOSE :  Works out whether this node should be considered a junction.
/////////////////////////////////////////////////////////////////////////////////

bool CPathFindBuild::QualifiesAsJunction(const CPathNode* pNode)
{
	Assert(pNode);
	Assert(IsRegionLoaded(pNode->m_address));
	Assert(!pNode->m_address.IsEmpty());

	if (!pNode->m_2.m_qualifiesAsJunction)
	{
		return false;
	}

	//if this junction isn't on the highway, early out
	if (!pNode->IsHighway())
	{
		return true;
	}

	//otherwise, all the junction entrances need to be on the highway for this to fail
	for (int i = 0; i < pNode->NumLinks(); i++)
	{
		CPathNode* pOtherNode = GetNodesLinkedNode(pNode, i);
		if (pOtherNode && !pOtherNode->IsHighway())
		{
			return true;
		}
	}

	return false;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : PreparePathDataForType
// PURPOSE :  Will go through all the blocks in the map and extract the node
//			  data from them. The links between the nodes will then be
//			  established so that route finding can be done.
/////////////////////////////////////////////////////////////////////////////////
void CPathFindBuild::PreparePathDataForType(	CTempNode *TempNodeList, CTempLink *TempLinkList,
											   s32 NumTempNodes, s32 NumTempLinks,
											   float MergeDist)
{

#define	fMaxLinkLengthSqr (200.0f*200.0f)

	Displayf("Buildpaths:Prepare path data\n");

	Displayf("Buildpaths:Prepare path data - check duplicate nodes\n");

	s32 Region;

	// Test whether we have any double nodes.
#if __ASSERT
	const float fDoubleNodeEpsSqr = 0.1f*0.1f;
	bool bDoubleNodes = false;
	for (s32 i = 0; i < NumTempNodes-1; i++)
	{
		for (s32 ii = i+1; ii < NumTempNodes; ii++)
		{
			float	distSqr = (TempNodeList[i].m_Coors - TempNodeList[ii].m_Coors).Mag2();

			if (distSqr < fDoubleNodeEpsSqr)
			{
				Displayf("Double node at: %f %f %f\n", TempNodeList[i].m_Coors.x, TempNodeList[i].m_Coors.y, TempNodeList[i].m_Coors.z);
				ASSERT_ONLY(bDoubleNodes = true);
			}
		}
	}
	Assertf((bDoubleNodes==false), "Double nodes (nodes at identical nodes) found. Check debug output");
#endif


	Displayf("Buildpaths:Prepare path data - resolve links\n");

	// The links point to nodes in the same file. We have to change the Node1 and Node2 fields
	// in the links to point to the nodes within the full array as opposed to within the file.
	for (s32 Link = 0; Link < NumTempLinks; Link++)
	{
		for (s32 Node = 0; Node < NumTempNodes; Node++)
		{
			if (TempNodeList[Node].m_FileId == TempLinkList[Link].m_FileId)
			{
				if (TempLinkList[Link].m_Node1 == TempNodeList[Node].m_NodeIndex)
				{
					TempLinkList[Link].m_Node1 = Node | (1<<31);
				}
				if (TempLinkList[Link].m_Node2 == TempNodeList[Node].m_NodeIndex)
				{
					TempLinkList[Link].m_Node2 = Node | (1<<31);
				}
			}
		}
	}
	for (s32 Link = 0; Link < NumTempLinks; Link++)
	{
		Assert(TempLinkList[Link].m_Node1 & (1<<31));
		Assert(TempLinkList[Link].m_Node2 & (1<<31));
		TempLinkList[Link].m_Node1 &= (~(1<<31));
		TempLinkList[Link].m_Node2 &= (~(1<<31));
	}


	// Make sure there are no links of ridiculous length

	for (s32 l = 0; l < NumTempLinks; l++)
	{
		const Vector3 & C1 = TempNodeList[TempLinkList[l].m_Node1].m_Coors;
		const Vector3 & C2 = TempNodeList[TempLinkList[l].m_Node2].m_Coors;
		float fLinkLength = (C1 - C2).Mag2();
		Assertf(fLinkLength < fMaxLinkLengthSqr, "Massively huge link detected #3 from (%.1f, %.1f, %.1f) to (%.1f, %.1f, %.1f).. That's %.1fm long!", C1.x, C1.y, C1.z, C2.x, C2.y, C2.z, fLinkLength);
		if(fLinkLength >= fMaxLinkLengthSqr)
		{
			Errorf("Massively huge link detected #3 from (%.1f, %.1f, %.1f) to (%.1f, %.1f, %.1f).. That's %.1fm long!", C1.x, C1.y, C1.z, C2.x, C2.y, C2.z, fLinkLength);
		}
	}

	Displayf("Buildpaths:Prepare path data - check link semantics\n");


	// Make sure there are no ped nodes connected to non ped nodes
	for (s32 l = 0; l < NumTempLinks; l++)
	{
		s32 special1 = TempNodeList[TempLinkList[l].m_Node1].m_SpecialFunction;
		s32 special2 = TempNodeList[TempLinkList[l].m_Node2].m_SpecialFunction;
		const bool bCrossingNodesOk = (special1==SPECIAL_PED_CROSSING)==(special2==SPECIAL_PED_CROSSING);
		const bool bDrivewayCrossingNodesOk = (special1==SPECIAL_PED_DRIVEWAY_CROSSING)==(special2==SPECIAL_PED_DRIVEWAY_CROSSING);
		const bool bSpecialNodesOk = (special1==SPECIAL_PED_ASSISTED_MOVEMENT)==(special2==SPECIAL_PED_ASSISTED_MOVEMENT);

		Assertf( bCrossingNodesOk, "Ped crossing node connected to non ped crossing node %f %f", TempNodeList[TempLinkList[l].m_Node1].m_Coors.x, TempNodeList[TempLinkList[l].m_Node1].m_Coors.y);
		Assertf( bDrivewayCrossingNodesOk, "Ped driveway crossing node connected to non ped driveway crossing node %f %f", TempNodeList[TempLinkList[l].m_Node1].m_Coors.x, TempNodeList[TempLinkList[l].m_Node1].m_Coors.y);
		Assertf( bSpecialNodesOk, "Ped assisted movement node connected to non assisted movement crossing node %f %f", TempNodeList[TempLinkList[l].m_Node1].m_Coors.x, TempNodeList[TempLinkList[l].m_Node1].m_Coors.y);

		if(!bCrossingNodesOk)
		{
			Errorf("Ped crossing node connected to non ped crossing node %f %f", TempNodeList[TempLinkList[l].m_Node1].m_Coors.x, TempNodeList[TempLinkList[l].m_Node1].m_Coors.y);
		}
		if(!bDrivewayCrossingNodesOk)
		{
			Errorf("Ped driveway crossing node connected to non ped driveway crossing node %f %f", TempNodeList[TempLinkList[l].m_Node1].m_Coors.x, TempNodeList[TempLinkList[l].m_Node1].m_Coors.y);
		}
		if(!bSpecialNodesOk)
		{
			Errorf("Ped assisted movement node connected to non assisted movement crossing node %f %f", TempNodeList[TempLinkList[l].m_Node1].m_Coors.x, TempNodeList[TempLinkList[l].m_Node1].m_Coors.y);
		}
	}


	Displayf("Buildpaths:Merge nodes\n");

	const float fMergeDistSqr = MergeDist*MergeDist;

	// Nodes that are near each other and from different files have to be merged.
	// Note: The links pointing to the node that was removed have to be modified also.
	for (s32 Node1 = 0; Node1 < NumTempNodes-1; Node1++)
	{
		for (s32 Node2 = Node1+1; Node2 < NumTempNodes; Node2++)
		{
			if (TempNodeList[Node1].m_FileId != TempNodeList[Node2].m_FileId)
			{
				if ((TempNodeList[Node1].m_Coors - TempNodeList[Node2].m_Coors).Mag2() < fMergeDistSqr)
				{
					// These 2 nodes have to be merged. Node2 gets removed. (Node2 is greater than Node1)
					for (s32 NodeX = Node2; NodeX < NumTempNodes-1; NodeX++)
					{
						TempNodeList[NodeX] = TempNodeList[NodeX+1];
					}

					// link pointing to Node2 now have to point to Node1
					// link pointing to nodes after Node2 have to point one node lower as the nodes have shifted.
					for (s32 link = 0; link < NumTempLinks; link++)
					{
						if (TempLinkList[link].m_Node1 == Node2)
						{
							TempLinkList[link].m_Node1 = Node1;
						}
						else if (TempLinkList[link].m_Node1 > Node2)
						{
							TempLinkList[link].m_Node1--;
						}

						if (TempLinkList[link].m_Node2 == Node2)
						{
							TempLinkList[link].m_Node2 = Node1;
						}
						else if (TempLinkList[link].m_Node2 > Node2)
						{
							TempLinkList[link].m_Node2--;
						}
					}
					// Make sure our loop still works.
					Node2--;
					NumTempNodes--;
				}
			}
		}
	}

	// Make sure there are no links of ridiculous length
#if __ASSERT
	for (s32 l = 0; l < NumTempLinks; l++)
	{
		const Vector3 & C1 = TempNodeList[TempLinkList[l].m_Node1].m_Coors;
		const Vector3 & C2 = TempNodeList[TempLinkList[l].m_Node2].m_Coors;
		float fLinkLength = (C1 - C2).Mag2();
		Assertf(fLinkLength < fMaxLinkLengthSqr, "Massively huge link detected #3 from (%.1f, %.1f, %.1f) to (%.1f, %.1f, %.1f).. That's %.1fm long!", C1.x, C1.y, C1.z, C2.x, C2.y, C2.z, fLinkLength);
	}
#endif

	Displayf("Buildpaths:Remove isolated nodes\n");

	// Remove nodes that have no neighbours. This should save the artists a lot of work and avoid nasty
	// crashes and cars getting confused.
	for (s32 Node = 0; Node < NumTempNodes; Node++)
	{
		s32 Link;
		for (Link = 0; Link < NumTempLinks; Link++)
		{
			if (TempLinkList[Link].m_Node1 == Node || TempLinkList[Link].m_Node2 == Node)
			{
				break;
			}
		}
		if (Link == NumTempLinks)
		{	
			// This node has no links using it and should be removed. (Unless it is a network restart point. They can be isolated)
			{
				for (s32 NodeX = Node; NodeX < NumTempNodes-1; NodeX++)
				{
					TempNodeList[NodeX] = TempNodeList[NodeX+1];
				}

				// link pointing to nodes after Node2 have to point one node lower as the nodes have shifted.
				for (s32 link = 0; link < NumTempLinks; link++)
				{
					if (TempLinkList[link].m_Node1 > Node)
					{
						TempLinkList[link].m_Node1--;
					}

					if (TempLinkList[link].m_Node2 > Node)
					{
						TempLinkList[link].m_Node2--;
					}
				}

				// Make sure our loop still works.
				Node--;
				NumTempNodes--;
			}
		}
	}

	// Make sure there are no links of ridiculous length
#if __ASSERT
	for (s32 l = 0; l < NumTempLinks; l++)
	{

		const Vector3 & C1 = TempNodeList[TempLinkList[l].m_Node1].m_Coors;
		const Vector3 & C2 = TempNodeList[TempLinkList[l].m_Node2].m_Coors;
		float fLinkLength = (C1 - C2).Mag2();
		Assertf(fLinkLength < fMaxLinkLengthSqr, "Massively huge link detected #3 from (%.1f, %.1f, %.1f) to (%.1f, %.1f, %.1f).. That's %.1fm long!", C1.x, C1.y, C1.z, C2.x, C2.y, C2.z, fLinkLength);
	}
#endif

	Displayf("Buildpaths:Connect open space nodes to roads\n");

	// Artist placed nodes form connections with open-space nodes so that cars can travel
	// from roads onto open planes and vice versa.
	for (s32 Node = 0; Node < NumTempNodes; Node++)
	{
		if (!TempNodeList[Node].m_SpecialFunction == SPECIAL_USE_NONE)
		{
			for (s32 Node2 = 0; Node2 < NumTempNodes; Node2++)
			{
				if (TempNodeList[Node].m_SpecialFunction == SPECIAL_OPEN_SPACE)
				{
					if ( (TempNodeList[Node].m_Coors - TempNodeList[Node2].m_Coors).XYMag() < OPEN_SPACE_SPACING)
					{	
						// These 2 are close together and should form a link.
						TempLinks[NumTempLinks].m_FileId = 0;
						TempLinks[NumTempLinks].m_Node1 = Node;
						TempLinks[NumTempLinks].m_Node2 = Node2;
						TempLinks[NumTempLinks].m_Lanes1To2 = 1;
						TempLinks[NumTempLinks].m_Lanes2To1 = 1;
						TempLinks[NumTempLinks].m_Width = 0;
						TempLinks[NumTempLinks].m_NarrowRoad = true;
						TempLinks[NumTempLinks].m_bGpsCanGoBothWays = true;
						TempLinks[NumTempLinks].m_bDontUseForNavigation = false;
						TempLinks[NumTempLinks].m_bBlockIfNoLanes = false;

						NumTempLinks++;

						PathBuildFatalAssertf(NumTempLinks < PF_TEMPLINKS,"");
					}
				}
			}
		}
	}

	Displayf("Buildpaths:Assign indices\n");

	// Assign each node an index within its block of the world (CNodeAddress)
	// and store the node.
	for (s32 Node = 0; Node < NumTempNodes; Node++)
	{
		Region = FindRegionForCoors(TempNodeList[Node].m_Coors.x, TempNodeList[Node].m_Coors.y);
		Assert(apRegions[Region]);

		CPathNode *pNode = &apRegions[Region]->aNodes[apRegions[Region]->NumNodes];



		pNode->SetCoors(TempNodeList[Node].m_Coors);
		Assertf(Sign(TempNodeList[Node].m_Coors.x) == Sign(pNode->m_coorsX) || pNode->m_coorsX==0, "\nOh dear, pathnode (%.1f, %.1f, %.1f) outside of X range", TempNodeList[Node].m_Coors.x, TempNodeList[Node].m_Coors.y, TempNodeList[Node].m_Coors.z);
		Assertf(Sign(TempNodeList[Node].m_Coors.y) == Sign(pNode->m_coorsY) || pNode->m_coorsY==0, "\nOh dear, pathnode (%.1f, %.1f, %.1f) outside of Y range", TempNodeList[Node].m_Coors.x, TempNodeList[Node].m_Coors.y, TempNodeList[Node].m_Coors.z);

		pNode->m_address.Set(Region, apRegions[Region]->NumNodes);
		pNode->m_streetNameHash = TempNodeList[Node].m_StreetNameHash;
		pNode->m_1.m_group = 0;				// Filled in later
		pNode->m_2.m_numLinks = 0;		// Filled in later
		pNode->m_2.m_slipJunction = false;
		pNode->m_startIndexOfLinks = 0;	// Filled in later
		pNode->m_2.m_deadEndness = 0;			// Filled in later
		pNode->m_2.m_switchedOff = TempNodeList[Node].m_bSwitchedOff;
		pNode->m_2.m_switchedOffOriginal = TempNodeList[Node].m_bSwitchedOff;
		pNode->m_2.m_noGps = TempNodeList[Node].m_bNoGps;
		pNode->m_2.m_waterNode = TempNodeList[Node].m_bWaterNode;
		pNode->m_2.m_alreadyFound = false;
		pNode->m_2.m_highwayOrLowBridge = TempNodeList[Node].m_bHighwayOrLowBridge;
		pNode->m_2.m_speed = TempNodeList[Node].m_Speed;
		pNode->m_2.m_density = TempNodeList[Node].m_Density;
		pNode->m_1.m_specialFunction = TempNodeList[Node].m_SpecialFunction;
		pNode->m_2.m_inTunnel = TempNodeList[Node].m_bAdditionalTunnelFlag;
		pNode->m_1.m_cannotGoLeft = TempNodeList[Node].m_bNoLeftTurns;
		pNode->m_2.m_leftOnly = TempNodeList[Node].m_bLeftOnly;
		pNode->m_1.m_cannotGoRight = TempNodeList[Node].m_bNoRightTurns;
		pNode->m_1.m_Offroad = TempNodeList[Node].m_bOffroad;
		pNode->m_1.m_noBigVehicles = TempNodeList[Node].m_bNoBigVehicles;
		pNode->m_1.m_indicateKeepLeft = TempNodeList[Node].m_bIndicateKeepLeft;
		pNode->m_1.m_indicateKeepRight = TempNodeList[Node].m_bIndicateKeepRight;
		pNode->m_1.m_slipLane = TempNodeList[Node].m_bSlipNode;

// 		float	waterz;
// 		if (Water::GetWaterLevel(TempNodeList[Node].m_Coors, &waterz, true, POOL_DEPTH, REJECTIONABOVEWATER, NULL) &&
// 			waterz > TempNodeList[Node].m_Coors.z + 2.0f)
// 		{
// 			pNode->m_2.m_inTunnel = true;
// 		}

		TempNodeList[Node].m_NodeAddress.Set(Region, apRegions[Region]->NumNodes);

		apRegions[Region]->NumNodes++;

		PathBuildFatalAssertf(apRegions[Region]->NumNodes < MAXNODESPERREGION, "Too Many Nodes in this Region");
	}

#define DISTMULT_SPEED_0	(2.0f)
#define DISTMULT_SPEED_2	(0.9f)
#define DISTMULT_OPEN_SPACE	(0.5f)

	Displayf("Buildpaths:Store links\n");

	// Store the links that connect to this node.
	for (s32 Node = 0; Node < NumTempNodes; Node++)
	{
		Region = TempNodeList[Node].m_NodeAddress.GetRegion();
		s32	IndexWithinRegion = TempNodeList[Node].m_NodeAddress.GetIndex();

		CPathNode *pNode = &apRegions[Region]->aNodes[IndexWithinRegion];

		// Maximum number storable in m_startIndexOfLinks
		// TODO: We can/should change this to a u16 really
		PathBuildFatalAssertf(apRegions[Region]->NumLinks <= 32767, "Too Many Links in this Region");	

		pNode->m_startIndexOfLinks = (s16)(apRegions[Region]->NumLinks);

		// Find the links that connect this node.
		for (s32 Link = 0; Link < NumTempLinks; Link++)
		{
			s32	NumLinksForThisNode = 0;

			if (TempLinkList[Link].m_Node1 == Node)
			{
				s32 OtherNode = TempLinkList[Link].m_Node2;

				// Calculate a distance multiplier based on the speed of the nodes. This way the path finding can be
				// discouraged to take routes that have low speeds (back alleys, petrol stations, the limo garage near the start point)
				float	distMultiplier = 1.0f;
				if (TempNodeList[Node].m_Speed == 0 && TempNodeList[OtherNode].m_Speed == 0)
				{
					distMultiplier = DISTMULT_SPEED_0;
				}
				else if (TempNodeList[Node].m_Speed >= 2 && TempNodeList[OtherNode].m_Speed >= 2)
				{
					distMultiplier = DISTMULT_SPEED_2;
				}
				if (TempNodeList[Node].m_bOpenSpace || TempNodeList[OtherNode].m_bOpenSpace)
				{
					distMultiplier *= DISTMULT_OPEN_SPACE;
				}

				apRegions[Region]->aLinks[apRegions[Region]->NumLinks].m_OtherNode = TempNodeList[OtherNode].m_NodeAddress;
				apRegions[Region]->aLinks[apRegions[Region]->NumLinks].m_1.m_Distance = Clamp((u8)(distMultiplier*(TempNodeList[Node].m_Coors - TempNodeList[OtherNode].m_Coors).Mag()), (u8)1, (u8)255);
				apRegions[Region]->aLinks[apRegions[Region]->NumLinks].m_1.m_Width = TempLinkList[Link].m_Width;
				apRegions[Region]->aLinks[apRegions[Region]->NumLinks].m_1.m_LanesToOtherNode = TempLinkList[Link].m_Lanes1To2;
				apRegions[Region]->aLinks[apRegions[Region]->NumLinks].m_1.m_LanesFromOtherNode = TempLinkList[Link].m_Lanes2To1;
				apRegions[Region]->aLinks[apRegions[Region]->NumLinks].m_1.m_NarrowRoad = TempLinkList[Link].m_NarrowRoad;
				apRegions[Region]->aLinks[apRegions[Region]->NumLinks].m_1.m_bGpsCanGoBothWays = TempLinkList[Link].m_bGpsCanGoBothWays;
				apRegions[Region]->aLinks[apRegions[Region]->NumLinks].m_1.m_bShortCut = TempLinkList[Link].m_bShortCut;
				apRegions[Region]->aLinks[apRegions[Region]->NumLinks].m_1.m_bDontUseForNavigation = TempLinkList[Link].m_bDontUseForNavigation;
				apRegions[Region]->aLinks[apRegions[Region]->NumLinks].m_1.m_bBlockIfNoLanes = TempLinkList[Link].m_bBlockIfNoLanes;

				NumLinksForThisNode++;

				PathBuildFatalAssertf(NumLinksForThisNode < PF_MAXLINKSPERNODE, "");
				PathBuildFatalAssertf(pNode->m_2.m_numLinks < 31,"");
				PathBuildFatalAssertf(apRegions[Region]->NumLinks < MAXLINKSPERREGION,"");

				pNode->m_2.m_numLinks++;
				apRegions[Region]->NumLinks++;
			}
			else if (TempLinkList[Link].m_Node2 == Node)
			{
				s32	OtherNode = TempLinkList[Link].m_Node1;

				// Calculate a distance multiplier based on the speed of the nodes. This way the path finding can be
				// discouraged to take routes that have low speeds (back alleys, petrol stations, the limo garage near the start point)
				float	distMultiplier = 1.0f;
				if (TempNodeList[Node].m_Speed == 0 && TempNodeList[OtherNode].m_Speed == 0)
				{
					distMultiplier = DISTMULT_SPEED_0;
				}
				else if (TempNodeList[Node].m_Speed >= 2 && TempNodeList[OtherNode].m_Speed >= 2)
				{
					distMultiplier = DISTMULT_SPEED_2;
				}
				if (TempNodeList[Node].m_bOpenSpace || TempNodeList[OtherNode].m_bOpenSpace)
				{
					distMultiplier *= DISTMULT_OPEN_SPACE;
				}

				apRegions[Region]->aLinks[apRegions[Region]->NumLinks].m_OtherNode = TempNodeList[OtherNode].m_NodeAddress;
				apRegions[Region]->aLinks[apRegions[Region]->NumLinks].m_1.m_Distance = Clamp((u8)(distMultiplier*(TempNodeList[Node].m_Coors - TempNodeList[OtherNode].m_Coors).Mag()), (u8)1, (u8)255);
				apRegions[Region]->aLinks[apRegions[Region]->NumLinks].m_1.m_Width = TempLinkList[Link].m_Width;
				apRegions[Region]->aLinks[apRegions[Region]->NumLinks].m_1.m_LanesToOtherNode = TempLinkList[Link].m_Lanes2To1;
				apRegions[Region]->aLinks[apRegions[Region]->NumLinks].m_1.m_LanesFromOtherNode = TempLinkList[Link].m_Lanes1To2;
				apRegions[Region]->aLinks[apRegions[Region]->NumLinks].m_1.m_NarrowRoad = TempLinkList[Link].m_NarrowRoad;
				apRegions[Region]->aLinks[apRegions[Region]->NumLinks].m_1.m_bGpsCanGoBothWays = TempLinkList[Link].m_bGpsCanGoBothWays;
				apRegions[Region]->aLinks[apRegions[Region]->NumLinks].m_1.m_bShortCut = TempLinkList[Link].m_bShortCut;
				apRegions[Region]->aLinks[apRegions[Region]->NumLinks].m_1.m_bDontUseForNavigation = TempLinkList[Link].m_bDontUseForNavigation;
				apRegions[Region]->aLinks[apRegions[Region]->NumLinks].m_1.m_bBlockIfNoLanes = TempLinkList[Link].m_bBlockIfNoLanes;

				NumLinksForThisNode++;

				PathBuildFatalAssertf(NumLinksForThisNode < PF_MAXLINKSPERNODE,"");
				PathBuildFatalAssertf(pNode->m_2.m_numLinks < 31,"");
				PathBuildFatalAssertf(apRegions[Region]->NumLinks < MAXLINKSPERREGION,"");

				pNode->m_2.m_numLinks++;
				apRegions[Region]->NumLinks++;
			}
		}
	}

#if __DEV
	// Do a check to make sure nodes point 2 ways.
	for (Region = 0; Region < PATHFINDREGIONS; Region++)
	{
		CheckPathNodeIntegrityForZone(Region);
	}
#endif	//DEV

	Displayf("Buildpaths:Update dead ends\n");

	// For the roads we detect the ones on a dead end so we can get the computer controlled
	// cars to avoid these.
	bool	ChangeHasHappened = true;
	u32	LiveLinks, DeadEndLinks, HighestDeadEndness;		

	// Mark the nodes next to a dead end as dead end as well.
	while (ChangeHasHappened)
	{
		ChangeHasHappened = false;
		for (Region = 0; Region < PATHFINDREGIONS; Region++)
		{
			if(!apRegions[Region] || !apRegions[Region]->NumNodes)
			{
				continue;
			}

			for (s32 Node = 0; Node < apRegions[Region]->NumNodes; Node++)
			{
				CPathNode		*pNode = &((apRegions[Region]->aNodes)[Node]);
				if (!pNode->m_2.m_deadEndness)
				{
					// Count the number of non dead end neighbours.
					LiveLinks = 0;
					DeadEndLinks = 0;
					HighestDeadEndness = 0;
					for(u32 linkNum = 0; linkNum < pNode->NumLinks(); linkNum++)
					{
						ASSERT_ONLY(CNodeAddress linkNodeAddr = GetNodesLinkedNodeAddr(pNode, linkNum);)
						Assert(IsRegionLoaded(linkNodeAddr));
						CPathNode* pLinkedNode = GetNodesLinkedNode(pNode, linkNum);

						bool bSpecialFunctionIsThroughNode = false;
						switch(pLinkedNode->m_1.m_specialFunction)
						{
						case SPECIAL_USE_NONE:
						case SPECIAL_DROPOFF_GOODS:
						case SPECIAL_DRIVE_THROUGH:
						case SPECIAL_DRIVE_THROUGH_WINDOW:
						case SPECIAL_DROPOFF_GOODS_UNLOAD:
						case SPECIAL_SMALL_WORK_VEHICLES:
						case SPECIAL_PETROL_STATION:
						case SPECIAL_DROPOFF_PASSENGERS:
						case SPECIAL_DROPOFF_PASSENGERS_UNLOAD:
						case SPECIAL_TRAFFIC_LIGHT:
						case SPECIAL_GIVE_WAY:
						case SPECIAL_FORCE_JUNCTION:
						case SPECIAL_RESTRICTED_AREA:
							bSpecialFunctionIsThroughNode = true;
							break;
						}

						if ( bSpecialFunctionIsThroughNode &&
							(!pLinkedNode->m_2.m_switchedOffOriginal || pNode->m_2.m_switchedOffOriginal))
						{
							if(pLinkedNode->m_2.m_deadEndness)
							{
								DeadEndLinks++;
								HighestDeadEndness = MAX(pLinkedNode->m_2.m_deadEndness, HighestDeadEndness);
							}
							else
							{
								LiveLinks++;
							}
						}
					}
					if (LiveLinks < 2)
					{
						if (DeadEndLinks >= 2)		// We have more than 1 deadend neighbours. We're at a junction.
						{
							Assert(HighestDeadEndness <= 6);
							pNode->m_2.m_deadEndness = MIN(HighestDeadEndness + 1, 7);
						}
						else
						{
							pNode->m_2.m_deadEndness = MAX(1, HighestDeadEndness);
						}
						ChangeHasHappened = true;
					}
				}
			}
		}		
	}

	// Change the deadendness so that the higher number is further from the main road (so that cars will always drive towards lower deadendness.
	for (Region = 0; Region < PATHFINDREGIONS; Region++)
	{
		if(!apRegions[Region] || !apRegions[Region]->NumNodes)
		{
			continue;
		}

		for (s32 Node = 0; Node < apRegions[Region]->NumNodes; Node++)
		{
			CPathNode *pNode = &((apRegions[Region]->aNodes)[Node]);

			if (pNode->m_2.m_deadEndness)
			{
				pNode->m_2.m_deadEndness = 8 - pNode->m_2.m_deadEndness;
			}
		}
	}

	// Preprocess "CPathFind::ThisNodeWillLeadIntoADeadEnd()"
	// This now has to be stored in the link between two nodes, because the code can't be SPU'd (without a lot of work)
	for (Region = 0; Region < PATHFINDREGIONS; Region++)
	{
		if(!apRegions[Region] || !apRegions[Region]->NumNodes)
			continue;

		for (s32 Node = 0; Node < apRegions[Region]->NumNodes; Node++)
		{
			CPathNode *pNode = &((apRegions[Region]->aNodes)[Node]);

			for(int l=0; l<pNode->NumLinks(); l++)
			{
				CPathNodeLink & link = GetNodesLink(pNode, l);

				// to start with, set both to be false
				link.m_1.m_LeadsToDeadEnd = false;
				link.m_1.m_LeadsFromDeadEnd = false;

				CPathNode * pOtherNode = FindNodePointerSafe(link.m_OtherNode);
				Assert(pOtherNode);

				if(pOtherNode)
				{
					link.m_1.m_LeadsToDeadEnd = ThisNodeWillLeadIntoADeadEnd(pOtherNode, pNode);
				}
			}
		}
	}

	// Now iterate though all nodes & links again, and set the 'm_LeadsFromDeadEnd' for the
	// opposite direction in each link.
	for (Region = 0; Region < PATHFINDREGIONS; Region++)
	{
		if(!apRegions[Region] || !apRegions[Region]->NumNodes)
			continue;

		for (s32 Node = 0; Node < apRegions[Region]->NumNodes; Node++)
		{
			CPathNode *pNode = &((apRegions[Region]->aNodes)[Node]);

			for(int l=0; l<pNode->NumLinks(); l++)
			{
				CPathNodeLink & link = GetNodesLink(pNode, l);
				CPathNode * pOtherNode = FindNodePointerSafe(link.m_OtherNode);
				Assert(pOtherNode);

				if(pOtherNode)
				{
					s16 iLinkBack;
					if( FindNodesLinkIndexBetween2Nodes(pOtherNode->m_address, pNode->m_address, iLinkBack) )
					{
						const CPathNodeLink & linkBack = GetNodesLink( pOtherNode, iLinkBack );
						link.m_1.m_LeadsFromDeadEnd = linkBack.m_1.m_LeadsToDeadEnd;
					}
				}
			}
		}
	}

	Displayf("Buildpaths:shift nodes at junction (almost there John)\n");

	// Single direction nodes can get merged into a road with more lanes. If this is the case
	// the lanes need to be shifted at the junction so that traffic doesn't infringe upon traffic
	// it is supposed to carry on alongside with.
	// Goes through all the nodes going into this junction and identify neighbouring ones that are
	// one way and that are within a certain angle of eachother.
	for (Region = 0; Region < PATHFINDREGIONS; Region++)
	{
		if(!apRegions[Region] || !apRegions[Region]->NumNodes)
		{
			continue;
		}

		for (s32 Node = 0; Node < apRegions[Region]->NumNodes; Node++)
		{
			CPathNode		*pNode = &((apRegions[Region]->aNodes)[Node]);

			if(pNode->NumLinks() > 2)
			{
#define MAX_N (32)
				PathBuildFatalAssertf(pNode->NumLinks() < MAX_N,"");
				s32	num = 0;

				struct sDir{
					Vector2				diff;
					float				orientation;
					CPathNodeLink	*pLink;
					CPathNodeLink	*pOppositeLink;
					bool				bCanBeMerged;
					bool				bSwitchedOff;
				};
				sDir	aDirs[MAX_N];

				for(u32 n = 0; n < pNode->NumLinks(); n++)
				{
					CPathNodeLink *pLink = &GetNodesLink(pNode, n);

					if (pLink->m_1.m_LanesFromOtherNode == 0 || pLink->m_1.m_LanesToOtherNode == 0)
					{	
						// One way. Add link to be considered.
						CNodeAddress otherNodeAddr = pLink->m_OtherNode;
						CPathNode *pOtherNode = FindNodePointer(otherNodeAddr);
						Vector2	ourCoors, otherCoors;
						pNode->GetCoors2(ourCoors);
						pOtherNode->GetCoors2(otherCoors);
						aDirs[num].diff = otherCoors - ourCoors;
						aDirs[num].diff.Normalize();
						aDirs[num].bCanBeMerged = true;
						aDirs[num].bSwitchedOff = pOtherNode->m_2.m_switchedOffOriginal;
						aDirs[num].orientation = rage::Atan2f(-aDirs[num].diff.x, aDirs[num].diff.y);
						aDirs[num].pLink = pLink;

						s16 iLink = -1;
						const bool bLinkFound = FindNodesLinkIndexBetween2Nodes(otherNodeAddr, pNode->m_address, iLink);
						if(!bLinkFound)//can't find the link so just continue
						{
							continue;
						}

						aDirs[num].pOppositeLink = &GetNodesLink(pOtherNode, iLink);
						num++;
					}
				}

				if (num >= 2)
				{
					// Sort the connecting nodes in clockwise order
					bool bChange = true;
					while (bChange)
					{
						bChange = false;
						for (s32 e = 0; e < num-1; e++)
						{
							if (aDirs[e].orientation > aDirs[e+1].orientation)
							{
								sDir	temp = aDirs[e];
								aDirs[e] = aDirs[e+1];
								aDirs[e+1] = temp;

								bChange = true;
							}
						}
					} 
				}

				// Now go through and find the closest pair of nodes.
				s32	bestCandidate = 0;
				while (bestCandidate >= 0)
				{
					bestCandidate = -1;
					float	smallestOrientationDiff = PI * 0.35f;		// Cut off for roads considered to be merging.
					float	averageOrientation = 0.0f;
					for (s32 e = 0; e < num; e++)
					{
						if (aDirs[e].bCanBeMerged && aDirs[(e+1)%num].bCanBeMerged && (aDirs[e].bSwitchedOff == aDirs[(e+1)%num].bSwitchedOff) )
						{
							float	orientDiffSigned = aDirs[ (e+1)%num ].orientation - aDirs[e].orientation;
							orientDiffSigned = fwAngle::LimitRadianAngle(orientDiffSigned);
							float orientDiff = rage::Abs(orientDiffSigned);

							if (orientDiff < smallestOrientationDiff)
							{
								smallestOrientationDiff = orientDiff;
								bestCandidate = e;
								averageOrientation = aDirs[e].orientation + orientDiffSigned * 0.5f;
							}
						}
					}
					if (bestCandidate >= 0)
					{
						// Found the 2 nodes that we consider being part of the same road.
						s32 n1 = bestCandidate;
						s32 n2 = (n1+1) % num;

						// Before we do the shifting business we make sure there are links
						// on the other side of the junction that can accomodate all these lanes.
						// If this is not the case we don't shift as it causes cars to deviate from
						// their path unnecessarily.
						//s32	lanesWeSupplyToTheJunction = aDirs[n1].pLink->m_1.m_LanesFromOtherNode + aDirs[n2].pLink->m_1.m_LanesFromOtherNode;
						//s32	lanesWeTakeFromTheJunction = aDirs[n1].pLink->m_1.m_LanesToOtherNode + aDirs[n2].pLink->m_1.m_LanesToOtherNode;
						s32	lanesThatCanBeTakenFromTheJunction = 0;
						s32	lanesThatCanBeSuppliedToTheJunction = 0;

						for(u32 n = 0; n < pNode->NumLinks(); n++)
						{
							CPathNodeLink *pLink = &GetNodesLink(pNode, n);

							if (pLink != aDirs[n1].pLink && pLink != aDirs[n2].pLink)
							{
								CNodeAddress otherNodeAddr = pLink->m_OtherNode;
								CPathNode *pOtherNode = FindNodePointer(otherNodeAddr);
								Vector2	ourCoors, otherCoors;
								pNode->GetCoors2(ourCoors);
								pOtherNode->GetCoors2(otherCoors);
								Vector2 diff = otherCoors - ourCoors;
								diff.Normalize();
								float orientation = rage::Atan2f(-diff.x, diff.y);

								float angleFromExactlyOpposite = averageOrientation - orientation + PI;
								angleFromExactlyOpposite = fwAngle::LimitRadianAngle(angleFromExactlyOpposite);
								if (rage::Abs(angleFromExactlyOpposite) < PI * 0.5f)
								{
									lanesThatCanBeSuppliedToTheJunction += pLink->m_1.m_LanesFromOtherNode;
									lanesThatCanBeTakenFromTheJunction += pLink->m_1.m_LanesToOtherNode;
								}
							}
						}	

						// Only merge the lanes if on the other side the road is wide enough.
// 						if (lanesThatCanBeSuppliedToTheJunction >= lanesWeTakeFromTheJunction &&
// 							lanesThatCanBeTakenFromTheJunction >= lanesWeSupplyToTheJunction)
// 						{
// 							float	orientDiff = aDirs[n1].orientation - aDirs[n2].orientation;
// 							orientDiff = fwAngle::LimitRadianAngle(orientDiff);
// 
// 							if (orientDiff < 0.0f)
// 							{
// 								aDirs[n1].pLink->m_1.m_LanesShiftedToRightAtOriginNode = true;
// 								aDirs[n1].pOppositeLink->m_1.m_LanesShiftedToLeftAtOtherNode = true;
// 								aDirs[n2].pLink->m_1.m_LanesShiftedToLeftAtOriginNode = true;
// 								aDirs[n2].pOppositeLink->m_1.m_LanesShiftedToRightAtOtherNode = true;
// 							}
// 							else
// 							{
// 								aDirs[n1].pLink->m_1.m_LanesShiftedToLeftAtOriginNode = true;
// 								aDirs[n1].pOppositeLink->m_1.m_LanesShiftedToRightAtOtherNode = true;
// 								aDirs[n2].pLink->m_1.m_LanesShiftedToRightAtOriginNode = true;
// 								aDirs[n2].pOppositeLink->m_1.m_LanesShiftedToLeftAtOtherNode = true;
// 							}
// 						}
						aDirs[n1].bCanBeMerged = false;		// Done with these.
						aDirs[n2].bCanBeMerged = false;
					}
				}
			}
		}
	}


	Displayf("Buildpaths:do street names\n");

	// Extend the street names until a junction is hit.
	// This way John doesn't have to set the street name for each and every node.
	bool bChangedMade = true;
	while (bChangedMade)
	{
		bChangedMade = false;
		for (Region = 0; Region < PATHFINDREGIONS; Region++)
		{
			if(!apRegions[Region] || !apRegions[Region]->NumNodes)
			{
				continue;
			}

			for (s32 Node = 0; Node < apRegions[Region]->NumNodes; Node++)
			{
				CPathNode		*pNode = &((apRegions[Region]->aNodes)[Node]);
				if(pNode->m_streetNameHash==0 && pNode->NumLinks() <= 2)
				{
					// Where the streets have no name,
					// we try to copy a name from a neighbouring node.
					for(u32 Link = 0; Link < pNode->NumLinks(); Link++)
					{
						ASSERT_ONLY(CNodeAddress linkNodeAddr = GetNodesLinkedNodeAddr(pNode, Link);)
						Assert(IsRegionLoaded(linkNodeAddr));
						CPathNode* pLinkedNode = GetNodesLinkedNode(pNode, Link);

						if(pLinkedNode->m_streetNameHash != 0)
						{
							pNode->m_streetNameHash = pLinkedNode->m_streetNameHash;

							bChangedMade = true;
						}
					}
				}
			}
		}
	}

	Displayf("Buildpaths: extend low bridge\n");

	// Extend the low bridge until junction is hit.
	// This way John doesn't have to set the low bridge for each and every node.
	bChangedMade = true;
	while (bChangedMade)
	{
		bChangedMade = false;
		for (Region = 0; Region < PATHFINDREGIONS; Region++)
		{
			if(!apRegions[Region] || !apRegions[Region]->NumNodes)
			{
				continue;
			}

			for (s32 Node = 0; Node < apRegions[Region]->NumNodes; Node++)
			{
				CPathNode		*pNode = &((apRegions[Region]->aNodes)[Node]);
				if(pNode->m_2.m_waterNode && !pNode->m_2.m_highwayOrLowBridge && pNode->NumLinks() <= 2)
				{
					for(u32 Link = 0; Link < pNode->NumLinks(); Link++)
					{
						ASSERT_ONLY(CNodeAddress linkNodeAddr = GetNodesLinkedNodeAddr(pNode, Link);)
						PathBuildFatalAssertf(IsRegionLoaded(linkNodeAddr), "");

						CPathNode* pLinkedNode = GetNodesLinkedNode(pNode, Link);

						if(pLinkedNode->m_2.m_waterNode && pLinkedNode->m_2.m_highwayOrLowBridge)
						{
							pNode->m_2.m_highwayOrLowBridge = true;
							bChangedMade = true;
						}
					}
				}
			}
		}
	}

	Vector3 vDirToValidNeighbourNode[32];
	const float fSameDotEps = CPathFind::sm_fSlipLaneDotThreshold;

	Displayf("Buildpaths:Find junctions\n");

	// Work out for each node whether it would be a junction. More than 2 nodes that aren't special.
	for (Region = 0; Region < PATHFINDREGIONS; Region++)
	{
		if(!apRegions[Region] || !apRegions[Region]->NumNodes)
		{
			continue;
		}

		for (s32 Node = 0; Node < apRegions[Region]->NumNodes; Node++)
		{
			Vector3 vNodePos;			
			CPathNode * pNode = &((apRegions[Region]->aNodes)[Node]);
			pNode->GetCoors(vNodePos);

			s32	validNeigbours = 0;

			for(u32 Link = 0; Link < pNode->NumLinks(); Link++)
			{
				CPathNodeLink pathNodeLink = GetNodesLink(pNode, Link);
				ASSERT_ONLY(CNodeAddress linkNodeAddr = GetNodesLinkedNodeAddr(pNode, Link);)
				Assert(IsRegionLoaded(linkNodeAddr));
				CPathNode* pLinkedNode = GetNodesLinkedNode(pNode, Link);

				if(pNode->m_1.m_specialFunction != SPECIAL_OPEN_SPACE 
					&& pLinkedNode->m_1.m_specialFunction != SPECIAL_PARKING_PARALLEL 
					&& pLinkedNode->m_1.m_specialFunction != SPECIAL_PARKING_PERPENDICULAR 
					&& !pathNodeLink.m_1.m_bShortCut)
				{
					//let's just create a junction regardless of whether or not
					//any nodes are switched off--we hvae a lot of places where we
					//want cars entering the road from switched-off parking lots
					//to do junction logic
					//if(pNode->m_2.m_switchedOff || !pLinkedNode->m_2.m_switchedOff)
					{
 						Vector3 vNeighbour;
 						pLinkedNode->GetCoors(vNeighbour);
 						vDirToValidNeighbourNode[validNeigbours] = vNeighbour - vNodePos;
 						vDirToValidNeighbourNode[validNeigbours].z = 0.0f;
 						vDirToValidNeighbourNode[validNeigbours].Normalize();
 
 						validNeigbours++;
					}
				}
			}
			if (validNeigbours > 2 && pNode->m_1.m_specialFunction != SPECIAL_OPEN_SPACE)
			{
				bool bQualifiesAsJunction = true;

				// Identify slip-lanes; these will have 3 links, 2 of which will be almost exactly the same direction.
				// Never mark these up as Junction nodes - or we will have cars waiting wherever they exist
 				if(validNeigbours == 3)
 				{
 					if(DotProduct(vDirToValidNeighbourNode[0], vDirToValidNeighbourNode[1]) > fSameDotEps ||
 						DotProduct(vDirToValidNeighbourNode[1], vDirToValidNeighbourNode[2]) > fSameDotEps ||
 						DotProduct(vDirToValidNeighbourNode[2], vDirToValidNeighbourNode[0]) > fSameDotEps)
 					{
 						bQualifiesAsJunction = false;
 					}
 				}

				pNode->m_2.m_qualifiesAsJunction = bQualifiesAsJunction;

				if (pNode->m_2.m_qualifiesAsJunction && !QualifiesAsJunction(pNode))
				{
					pNode->m_2.m_qualifiesAsJunction = false;
				}

				//look for junctions that only have one inbound lane and filter them out
				if (pNode->m_2.m_qualifiesAsJunction)
				{
					int iNumInboundLinks = 0;
					for (int Link=0; Link < pNode->NumLinks(); Link++)
					{
						CPathNodeLink pathNodeLink = GetNodesLink(pNode, Link);
						ASSERT_ONLY(CNodeAddress linkNodeAddr = GetNodesLinkedNodeAddr(pNode, Link);)
						Assert(IsRegionLoaded(linkNodeAddr));
						CPathNode* pLinkedNode = GetNodesLinkedNode(pNode, Link);

						if (pathNodeLink.IsShortCut())
						{
							continue;
						}

						//traffic lights or give ways should be a signal that we actually
						//want to do junction logic here
						if (pLinkedNode->IsGiveWay() || pLinkedNode->IsTrafficLight())
						{
							iNumInboundLinks = 64;	//arbitrary >1 sentinal value
							break;
						}

						if (pathNodeLink.m_1.m_LanesFromOtherNode > 0)
						{
							++iNumInboundLinks;
						}
					}

					//if we have 1 or fewer inbound links, it's probably a fork
					//in the road, in which case we don't need to do the junction logic
					if (iNumInboundLinks < 2)
					{
						pNode->m_2.m_qualifiesAsJunction = false;
					}
				}

				//iterate through each entry-exit pair and see if any of them have multiple valid ways out
				//it's not a junction if none of them do
				//unless they signal they want it to be a junction with traffic lights or give-ways
				if (pNode->m_2.m_qualifiesAsJunction)
				{
					//int iNumEntriesWithMultipleValidExits = 0;
					bool bFoundEntryWMultipleValidExits = false;

					for (int iEntryLink=0; iEntryLink < pNode->NumLinks(); iEntryLink++)
					{
						int iNumValidWaysOut = 0;

						CPathNodeLink EntryLink = GetNodesLink(pNode, iEntryLink);
						ASSERT_ONLY(CNodeAddress EntrylinkNodeAddr = GetNodesLinkedNodeAddr(pNode, iEntryLink);)
						Assert(IsRegionLoaded(EntrylinkNodeAddr));
						CPathNode* pEntryNode = GetNodesLinkedNode(pNode, iEntryLink);

						if (pEntryNode->IsGiveWay() || pEntryNode->IsTrafficLight())
						{
							//early out if we find a traffic light or give-way
							//iNumEntriesWithMultipleValidExits = 64;
							bFoundEntryWMultipleValidExits = true;
							break;
						}

						if (EntryLink.IsShortCut())
						{
							//ignore shortcuts
							continue;
						}
						if (EntryLink.m_1.m_LanesFromOtherNode < 1)
						{
							continue;
						}

						for (int iExitLink=0; iExitLink < pNode->NumLinks(); iExitLink++)
						{
							CPathNodeLink ExitLink = GetNodesLink(pNode, iExitLink);
							ASSERT_ONLY(CNodeAddress ExitLinkNodeAddr = GetNodesLinkedNodeAddr(pNode, iExitLink);)
							Assert(IsRegionLoaded(ExitLinkNodeAddr));
							CPathNode* pExitNode = GetNodesLinkedNode(pNode, iExitLink);

							if (pExitNode->IsGiveWay() || pExitNode->IsTrafficLight())
							{
								//early out if we find a traffic light or give-way
								iNumValidWaysOut = 64;	//random >1 sentinal value
								//iNumEntriesWithMultipleValidExits = 64;
								bFoundEntryWMultipleValidExits = true;
								break;
							}

							if (ExitLink.IsShortCut())
							{
								//ignore shortcuts
								continue;
							}

							if (pExitNode->m_address == pEntryNode->m_address)
							{
								//u-turning is not a valid way out
								continue;
							}

							if (ExitLink.m_1.m_LanesToOtherNode < 1)
							{
								continue;
							}

							//now try and calculate  the direction
							float fDotProduct = 0.0f, fLeftness = 0.0f;
							bool bSharpTurn = false;
							CPathNodeRouteSearchHelper::FindJunctionTurnDirection(EmptyNodeAddress, pEntryNode->m_address, pNode->m_address, pExitNode->m_address
								, &bSharpTurn, &fLeftness, CPathNodeRouteSearchHelper::sm_fDefaultDotThresholdForRoadStraightAhead, &fDotProduct, *this);

							if (!bSharpTurn)
							{
								iNumValidWaysOut++;
							}
						}

						if (iNumValidWaysOut > 1)
						{
							bFoundEntryWMultipleValidExits = true;
						}
// 						if (iNumValidWaysOut > 2)
// 						{
// 							//iNumEntriesWithMultipleValidExits = 64;
// 							bFoundEntryWMultipleValidExits = true;
// 							break;
// 						}
// 						else if (iNumValidWaysOut > 1)
// 						{
// 							iNumEntriesWithMultipleValidExits++;
// 						}
					}

// 					if (iNumEntriesWithMultipleValidExits < 2)
// 					{
// 						pNode->m_2.m_qualifiesAsJunction = false;
// 					}
					if (!bFoundEntryWMultipleValidExits)
					{
						pNode->m_2.m_qualifiesAsJunction = false;
					}
				}
			}
			else
			{
				pNode->m_2.m_qualifiesAsJunction = false;
			}

			//STUPID STUPID HACK GET RID OF THIS
#if HACK_GTA4
			if (pNode->GetPos().Dist2(Vector3(717.0f, -1010.5f, 29.0f)) < 1.0f * 1.0f)
			{
				pNode->m_2.m_qualifiesAsJunction = false;
			}
#endif //HACK_GTA4

			//let the "force junction" special function override whatever we calculated above
			if (pNode->m_1.m_specialFunction == SPECIAL_FORCE_JUNCTION)
			{
				pNode->m_2.m_qualifiesAsJunction = true;
			}

			if (validNeigbours > 2)
			{
				pNode->m_2.m_slipJunction = true;
			}
			else
			{
				pNode->m_2.m_slipJunction = false;
			}
		}

		for (s32 Node = 0; Node < apRegions[Region]->NumNodes; Node++)
		{
			CPathNode * pNode = &((apRegions[Region]->aNodes)[Node]);
			if (pNode->NumLinks() > 2)
			{
				Vector3 vNodeCoords;
				pNode->GetCoors(vNodeCoords);

				//iterate through each of our links, looking for two that are collinear
				//if we find them, check that one looks like a sliplane and the other doesn't
				//this used to tag up the non-sliplane node as "cannotGoLeft", but I found enough
				//instances of cases where we want the non-sliplane to be allowed to go left in the world
				//that the default is now that you can go left, and in places where we don't want that to happen
				//it will have to be marked up in the world
				for(u32 iLink = 0; iLink < pNode->NumLinks(); iLink++)
				{
					ASSERT_ONLY(CNodeAddress iLinkNodeAddr = GetNodesLinkedNodeAddr(pNode, iLink);)
					Assert(IsRegionLoaded(iLinkNodeAddr));
					CPathNode* piLinkedNode = GetNodesLinkedNode(pNode, iLink);
					CPathNodeLink& riLink = GetNodesLink(pNode, iLink);

					Vector3 viNodeCoords;
					piLinkedNode->GetCoors(viNodeCoords);

					Vector3 vBaseToI = viNodeCoords - vNodeCoords;
					vBaseToI.z = 0.0f;
					vBaseToI.NormalizeSafe();

					for (u32 jLink = 0; jLink < pNode->NumLinks(); jLink++)
					{
						if (iLink == jLink)
						{
							continue;
						}

						ASSERT_ONLY(CNodeAddress jLinkNodeAddr = GetNodesLinkedNodeAddr(pNode, jLink);)
						PathBuildFatalAssertf(IsRegionLoaded(jLinkNodeAddr),"");
						CPathNode* pjLinkedNode = GetNodesLinkedNode(pNode, jLink);
						const CPathNodeLink & rjLink = GetNodesLink(pNode, jLink);

						{
							//test links
							if (riLink.m_1.m_LanesToOtherNode == 0 &&
								riLink.m_1.m_LanesFromOtherNode == 1 &&
								riLink.m_1.m_Width == 0 &&
								rjLink.m_1.m_Width > 0)
							{
								Vector3 vjNodeCoords;
								pjLinkedNode->GetCoors(vjNodeCoords);

								Vector3 vBaseToJ = vjNodeCoords - vNodeCoords;
								vBaseToJ.z = 0.0f;
								vBaseToJ.NormalizeSafe();

								//test for colinearity
								if (vBaseToI.Dot(vBaseToJ) > CPathFind::sm_fSlipLaneDotThreshold)
								{
									//i is the sliplane link
									piLinkedNode->m_2.m_leftOnly = true;
								}
							}
							else if (rjLink.m_1.m_LanesToOtherNode == 0 &&
								rjLink.m_1.m_LanesFromOtherNode == 1 &&
								rjLink.m_1.m_Width == 0 &&
								riLink.m_1.m_Width > 0)
							{
								Vector3 vjNodeCoords;
								pjLinkedNode->GetCoors(vjNodeCoords);

								Vector3 vBaseToJ = vjNodeCoords - vNodeCoords;
								vBaseToJ.z = 0.0f;
								vBaseToJ.NormalizeSafe();

								//test for colinearity
								if (vBaseToI.Dot(vBaseToJ) > CPathFind::sm_fSlipLaneDotThreshold)
								{
									//j is the sliplane link
									pjLinkedNode->m_2.m_leftOnly = true;
								}
							}
						}
					}
				}
			}
		}
	}

	// Flag which nodes are slip lanes, using CPathNodeRouteSearchHelper::GetSlipLaneNodeLinkIndex()
	Displayf("Buildpaths:Identify Sliplanes\n");
	IdentifySlipLaneNodes();

	// For all nodes which are entrances to templated junctions, set the SPECIAL_TRAFFIC_LIGHT attribute
	Displayf("Buildpaths:Preprocess junctions\n");
	PreprocessJunctions();

	// Count the nodes of each type in each Region.
	for (Region = 0; Region < PATHFINDREGIONS; Region++)
	{
		if(!apRegions[Region])
		{
			continue;
		}

		apRegions[Region]->NumNodesCarNodes = 0;
		while (apRegions[Region]->NumNodesCarNodes < apRegions[Region]->NumNodes && !apRegions[Region]->aNodes[apRegions[Region]->NumNodesCarNodes].IsPedNode() )
		{
			apRegions[Region]->NumNodesCarNodes++;
		}
		apRegions[Region]->NumNodesPedNodes = apRegions[Region]->NumNodes - apRegions[Region]->NumNodesCarNodes;
	}

	// Now set the m_2.m_distanceHash value. This is a value that represents a distance in meters to the next node but
	// carries on from node to node. Used by the car placement code.
	// Count the nodes of each type in each Region.
	for (Region = 0; Region < PATHFINDREGIONS; Region++)
	{
		if(!apRegions[Region])
		{
			continue;
		}

		for (s32 node = 0; node < apRegions[Region]->NumNodesCarNodes; node++)
		{
			CPathNode *pNode = &apRegions[Region]->aNodes[node];
			if (pNode->m_2.m_distanceHash == 0)
			{
				pNode->m_2.m_distanceHash = 1;
				FloodFillDistanceHashes();
			}
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : ThisNodeWillLeadIntoADeadEnd
// PURPOSE :  This little helper function will test whether a node will eventually lead
//			  to a dead end.
/////////////////////////////////////////////////////////////////////////////////
bool CPathFindBuild::ThisNodeWillLeadIntoADeadEnd(const CPathNode *pArgToNode, const CPathNode *pArgFromNode) const
{
	const CPathNode	*pSuccessFullCandidate;
	const CPathNode	*pToNode = pArgToNode;
	const CPathNode	*pFromNode = pArgFromNode;

	u32	currentDeadEndNess = pToNode->m_2.m_deadEndness;
	if (currentDeadEndNess == 0)
	{
		return false;
	}
	if(pArgToNode->IsPedNode() || pArgFromNode->IsPedNode())
	{
		return false;
	}
	while (1)
	{
		// Go through the neighbouring nodes and count the viable ones.
		pSuccessFullCandidate = NULL;
		s32	numSuccessFullNodes = 0;
		for(u32 Link = 0; Link < pToNode->NumLinks(); Link++)
		{
			CNodeAddress Candidate = GetNodesLinkedNodeAddr(pToNode, Link);
			if(!IsRegionLoaded(Candidate))
			{
				continue;
			}

			const CPathNode *pCandidate = FindNodePointer(Candidate);
			if(!pCandidate || (pCandidate == pFromNode))
			{
				continue;
			}

			if(pCandidate->HasSpecialFunction_Driving())
			{
				continue;
			}

			if (pCandidate->m_2.m_deadEndness < currentDeadEndNess)
			{
				return false;		// We found a node that leads back to the main network.
			}
			// Only traverse this link if it has the same dead-endness as this node, if its more we know its taking us away from the network.
			else if (pCandidate->m_2.m_deadEndness == currentDeadEndNess
				&& (!pCandidate->m_2.m_switchedOffOriginal || pToNode->m_2.m_switchedOffOriginal))
			{
				numSuccessFullNodes++;
				pSuccessFullCandidate = pCandidate;
			}
		}

		if (numSuccessFullNodes >= 2)
		{		// We've just hit a junction with more than 2 new nodes neither of which lead back to the node network.
			return true;
		}

		if (numSuccessFullNodes == 0)
		{		// We've just hit a node without suitable neighbours. This is the dead end itself. Call it a day.
			return true;
		}

		// We have found the 1 suitable other node. Continue looking down this path.

		pFromNode = pToNode;
		pToNode = pSuccessFullCandidate;

		// The following line will catch the case where the nodes are in an isolated circle.
		// The code would otherwise go round forever.
		if (pToNode == pArgToNode)
		{
			return false;
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : SetDistanceHashRecursively
// PURPOSE :  Goes through neighbours of this node to set the distance hash
/////////////////////////////////////////////////////////////////////////////////
void	CPathFindBuild::FloodFillDistanceHashes()
{
	bool	bChange = true;

	while (bChange)
	{
		bChange = false;
		for (s32 Region = 0; Region < PATHFINDREGIONS; Region++)
		{
			if(!apRegions[Region])
			{
				continue;
			}

			for (s32 node = 0; node < apRegions[Region]->NumNodesCarNodes; node++)
			{
				CPathNode *pNode = &apRegions[Region]->aNodes[node];
				if (pNode->m_2.m_distanceHash == 0)
				{
					for(u32 n = 0; n < pNode->NumLinks(); n++)
					{
						CPathNodeLink* pLink = &GetNodesLink(pNode, n);

						CPathNode *pNeighbourNode = FindNodePointer(pLink->m_OtherNode);

						if (pNeighbourNode->m_2.m_distanceHash)
						{
							Vector3 crs1, crs2;
							pNode->GetCoors(crs1);
							pNeighbourNode->GetCoors(crs2);
							s32 dist = MAX(1, ((s32)(crs1-crs2).Mag()) );
							pNode->m_2.m_distanceHash = pNeighbourNode->m_2.m_distanceHash + dist;
							if (pNode->m_2.m_distanceHash == 0) pNode->m_2.m_distanceHash = 1;
							bChange = true;
						}
					}
				}
			}
		}
	}
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : AddOpenSpaceNodes
// PURPOSE :  Goes through the open space that have been set up in max and adds the nodes
//			  to fill them up.
/////////////////////////////////////////////////////////////////////////////////
//float	openspaceX[OPEN_SPACE_POINTS] = { 2444.0f, 2532.0f, 2437.0f, 2850.0f, 2755.0f, 2626.0f, 2434.0f, 2206.0f, 1916.0f, 1848.0f, 1688.0f, 1591.0f, 1496.0f, 1414.0f, 1328.0f, 1623.0f, 1852.0f, 2377.0f };
//float	openspaceY[OPEN_SPACE_POINTS] = { 728.0f,   501.0f,  295.0f,  105.0f,  -63.0f,  -78.0f, -140.0f, -413.0f, -322.0f,  -35.0f, 18.0f,   -116.0f, -45.0f,   105.0f,  576.0f,  891.0f, 1238.0f, 1238.0f };
#define OPEN_SPACE_NUM_X	((int)((WORLDLIMITS_REP_XMAX - WORLDLIMITS_REP_XMIN) / OPEN_SPACE_SPACING))
#define OPEN_SPACE_NUM_Y	((int)((WORLDLIMITS_REP_YMAX - WORLDLIMITS_REP_YMIN) / OPEN_SPACE_SPACING))
void CPathFindBuild::RemoveNode(int nodeIndex)
{
	// Any links that still point to this node will have to be set to -1.
	for (int link = 0; link < NumTempLinks; link++)
	{
		if (TempLinks[link].m_FileId == TempNodes[nodeIndex].m_FileId)
		{
			if (TempLinks[link].m_Node1 == TempNodes[nodeIndex].m_NodeIndex)
			{
				TempLinks[link].m_Node1 = -1;
			}
			//			else if (TempLinks[link].m_Node1 > node)
			//			{
			//				TempLinks[link].m_Node1--;
			//			}

			if (TempLinks[link].m_Node2 == TempNodes[nodeIndex].m_NodeIndex)
			{
				TempLinks[link].m_Node2 = -1;
			}
			//			else if (TempLinks[link].m_Node2 > node)
			//			{
			//				TempLinks[link].m_Node2--;
			//			}
		}
	}

	for (s32 i = nodeIndex; i < NumTempNodes-1; i++)
	{
		TempNodes[i] = TempNodes[i+1];
	}
	NumTempNodes--;
}


void CPathFindBuild::RemoveLink(int link)
{
	for (s32 i = link; i < NumTempLinks-1; i++)
	{
		TempLinks[i] = TempLinks[i+1];
	}
	NumTempLinks--;
}


bool CPathFindBuild::FindNextNodeAndRemoveLinkAndNode(int nodeIndex, int &nextNodeIndex)
{
	if(NumTempLinks == 0) return false;	// should never happen.

	Assert(nodeIndex < NumTempNodes);
	int	link = 0;
	while (link < NumTempLinks && (TempLinks[link].m_FileId != TempNodes[nodeIndex].m_FileId || (TempLinks[link].m_Node1 != TempNodes[nodeIndex].m_NodeIndex && TempLinks[link].m_Node2 != TempNodes[nodeIndex].m_NodeIndex) ) )
	{
		link++;
	}
	Assertf(link < NumTempLinks, "Could not find link to connect to open space boundary node. Badly set up open space\n");

	if (link >= NumTempLinks) return false;	// should never happen.


	int nextNodeId;
	if (TempLinks[link].m_Node1 == TempNodes[nodeIndex].m_NodeIndex)
	{
		nextNodeId = TempLinks[link].m_Node2;
	}
	else
	{
		nextNodeId = TempLinks[link].m_Node1;
	}

	RemoveNode(nodeIndex);
	RemoveLink(link);

	if (nextNodeId < 0)	// We have gone round and found the initial node (that was removed)
	{
		return false;
	}

	nextNodeIndex = 0;
	while (nextNodeIndex < NumTempNodes && TempNodes[nextNodeIndex].m_NodeIndex != nextNodeId)
	{
		nextNodeIndex++;
	}

	Assert(nextNodeIndex < NumTempNodes);
	//	Assert(chainNextNodeIndex != currentNodeIndex);


	//	if (nextNode >= node)
	//	{
	//		nextNode--;		// because we removed a node.
	//	}
	return true;
};


void	CPathFindBuild::FindOpenSpacePos(int nodeX, int nodeY, float &coorsX, float &coorsY)
{
	float stepX = (WORLDLIMITS_REP_XMAX - WORLDLIMITS_REP_XMIN) / OPEN_SPACE_NUM_X;
	float stepY = (WORLDLIMITS_REP_YMAX - WORLDLIMITS_REP_YMIN) / OPEN_SPACE_NUM_Y;

	coorsX = WORLDLIMITS_REP_XMIN + stepX * (0.5f + nodeX);
	coorsY = WORLDLIMITS_REP_YMIN + stepY * (0.5f + nodeY);

	Assert(coorsX >= WORLDLIMITS_REP_XMIN && coorsX <= WORLDLIMITS_REP_XMAX);
	Assert(coorsY >= WORLDLIMITS_REP_YMIN && coorsY <= WORLDLIMITS_REP_YMAX);
}


#define MAX_OPEN_SPACE_BOUNDARY_POINTS (1000)
void	CPathFindBuild::AddOpenSpaceNodes()
{
	Assert(TempNodes);
	Assert(TempLinks);
	if(NumTempNodes == 0)
	{
		return;
	}

	Displayf("Buildpaths:AddOpenSpaceNodes\n");

	// Allocate some (large) temporary storage for the open space nodes.
	bool	*pNodes = (bool *)sysMemAllocator::GetMaster().Allocate(sizeof(bool) * OPEN_SPACE_NUM_X * OPEN_SPACE_NUM_Y, 16, MEMTYPE_RESOURCE_VIRTUAL);
	Assert(pNodes);
	int		*pNodesIndex = (int *)sysMemAllocator::GetMaster().Allocate(sizeof(int) * OPEN_SPACE_NUM_X * OPEN_SPACE_NUM_Y, 16, MEMTYPE_RESOURCE_VIRTUAL);
	Assert(pNodesIndex);
	float	*pNodesZ = (float *)sysMemAllocator::GetMaster().Allocate(sizeof(float) * OPEN_SPACE_NUM_X * OPEN_SPACE_NUM_Y, 16, MEMTYPE_RESOURCE_VIRTUAL);
	Assert(pNodesZ);

	// Clear the grid of potential open space nodes.
	for (s32 iX = 0; iX < OPEN_SPACE_NUM_X; iX++)
	{
		for (s32 iY = 0; iY < OPEN_SPACE_NUM_Y; iY++)
		{
			pNodes[iX+iY*OPEN_SPACE_NUM_X] = false;
			pNodesIndex[iX+iY*OPEN_SPACE_NUM_X] = 0;
			pNodesZ[iX+iY*OPEN_SPACE_NUM_X] = 0.0f;
		}
	}

	float	openSpaceX[MAX_OPEN_SPACE_BOUNDARY_POINTS];
	float	openSpaceY[MAX_OPEN_SPACE_BOUNDARY_POINTS];
	float	openSpaceZ[MAX_OPEN_SPACE_BOUNDARY_POINTS];
	bool	bNewOpenSpaceSequence[MAX_OPEN_SPACE_BOUNDARY_POINTS];
	s32	numOpenSpacePoints = 0;

	// First we need to identify the chains of open space boundary nodes and store them in temporary storage.
	// We also need to remove the nodes and links that are part of the chain.
	s32 currentNodeIndex = 0;
	while(numOpenSpacePoints < MAX_OPEN_SPACE_BOUNDARY_POINTS) // Make sure the number of open space points isn't exceeded.
	{
		// Find the next open space node in the temp nodes.
		while (currentNodeIndex < NumTempNodes && !TempNodes[currentNodeIndex].m_bOpenSpace )
		{
			++currentNodeIndex;
		}
		if(currentNodeIndex == NumTempNodes)
		{
			break;
		}

		// Add the open space point.
		bNewOpenSpaceSequence[numOpenSpacePoints] = true;
		openSpaceX[numOpenSpacePoints] = TempNodes[currentNodeIndex].m_Coors.x;
		openSpaceY[numOpenSpacePoints] = TempNodes[currentNodeIndex].m_Coors.y;
		openSpaceZ[numOpenSpacePoints] = TempNodes[currentNodeIndex].m_Coors.z;
		++numOpenSpacePoints;

		// Make sure the number of open space points isn't exceeded.
		if(numOpenSpacePoints == MAX_OPEN_SPACE_BOUNDARY_POINTS)
		{
			break;
		}

		// Add the open space points along this chain and remove the links
		// and nodes of the chain as we move along.
		int chainCurrentNodeIndex = currentNodeIndex;
		int chainNextNodeIndex = 0;
		while(	(numOpenSpacePoints < MAX_OPEN_SPACE_BOUNDARY_POINTS) &&
			FindNextNodeAndRemoveLinkAndNode(chainCurrentNodeIndex, chainNextNodeIndex))
		{
			// Add the open space point of the chain link.
			bNewOpenSpaceSequence[numOpenSpacePoints] = false;
			openSpaceX[numOpenSpacePoints] = TempNodes[chainNextNodeIndex].m_Coors.x;
			openSpaceY[numOpenSpacePoints] = TempNodes[chainNextNodeIndex].m_Coors.y;
			openSpaceZ[numOpenSpacePoints] = TempNodes[chainNextNodeIndex].m_Coors.z;
			++numOpenSpacePoints;

			// Advance along the chain.
			chainCurrentNodeIndex = chainNextNodeIndex;
		}

		// Go to the next node.
		++currentNodeIndex;
	}

	// Now that the OpenSpace nodes have been removed from the list we can use them to create
	// new nodes that actually fill in the area.
	int numOpenSpacePointsVisited = 0;

	while (numOpenSpacePointsVisited < numOpenSpacePoints)
	{
		int numPointsInSequence = 1;

		while (numOpenSpacePointsVisited+numPointsInSequence < numOpenSpacePoints && bNewOpenSpaceSequence[numOpenSpacePointsVisited+numPointsInSequence] == false)
		{
			numPointsInSequence++;
		}

		float	openSpaceMinX, openSpaceMinY, openSpaceMaxX, openSpaceMaxY;
		openSpaceMinX = openSpaceMaxX = openSpaceX[numOpenSpacePointsVisited];
		openSpaceMinY = openSpaceMaxY = openSpaceY[numOpenSpacePointsVisited];
		for (int n = 1; n < numPointsInSequence; n++)
		{
			openSpaceMinX = rage::Min(openSpaceMinX, openSpaceX[numOpenSpacePointsVisited+n]);
			openSpaceMaxX = rage::Max(openSpaceMaxX, openSpaceX[numOpenSpacePointsVisited+n]);
			openSpaceMinY = rage::Min(openSpaceMinY, openSpaceY[numOpenSpacePointsVisited+n]);
			openSpaceMaxY = rage::Max(openSpaceMaxY, openSpaceY[numOpenSpacePointsVisited+n]);
		}

		// Go through all of the potential nodes for all of the open spaces defined and test whether they are inside.
		for (s32 iX = 0; iX < OPEN_SPACE_NUM_X; iX++)
		{
			for (s32 iY = 0; iY < OPEN_SPACE_NUM_Y; iY++)
			{
				float	posX, posY;
				FindOpenSpacePos(iX, iY, posX, posY);

				if (posX >= openSpaceMinX && posX <= openSpaceMaxX && posY >= openSpaceMinY && posY <= openSpaceMaxY)
				{
					// Find out whether this point lies within the area specified by calculating the number of intersections with
					// the line segments.
					int	numIntersections = 0;
					for (int l = 0; l < numPointsInSequence; l++)
					{
						float point1X = openSpaceX[numOpenSpacePointsVisited+l];
						float point1Y = openSpaceY[numOpenSpacePointsVisited+l];
						float point2X = openSpaceX[numOpenSpacePointsVisited + ((l+1)%numPointsInSequence)];
						float point2Y = openSpaceY[numOpenSpacePointsVisited + ((l+1)%numPointsInSequence)];

						float	alongLine = (posX - point1X) / (point2X - point1X);
						if (alongLine >= 0.0f && alongLine <= 1.0f)
						{
							float	intersectY = point1Y + alongLine * (point2Y - point1Y);
							if (posY < intersectY)
							{
								numIntersections++;
							}
						}
					}
					if (numIntersections & 1)
					{
						pNodes[iX+iY*OPEN_SPACE_NUM_X] = true;

						// To calculate a height value we calculate a contribution from each line segment
						float	totalContribution = 0.0f, totalHeight = 0.0f;

						for (int l = 0; l < numPointsInSequence; l++)
						{
							float point1X = openSpaceX[numOpenSpacePointsVisited+l];
							float point1Y = openSpaceY[numOpenSpacePointsVisited+l];
							float point1Z = openSpaceZ[numOpenSpacePointsVisited+l];
							float point2X = openSpaceX[numOpenSpacePointsVisited + ((l+1)%numPointsInSequence)];
							float point2Y = openSpaceY[numOpenSpacePointsVisited + ((l+1)%numPointsInSequence)];
							float point2Z = openSpaceZ[numOpenSpacePointsVisited + ((l+1)%numPointsInSequence)];

							Assertf(point1Z >= -100.0f && point1Z <= 500.0f, "Map generated open space node outwidth reasonable z range");
							Assertf(point2Z >= -100.0f && point2Z <= 500.0f, "Map generated open space node outwidth reasonable z range");


							float dist = fwGeom::DistToLine(Vector3(point1X, point1Y, 0.0f), Vector3(point2X, point2Y, 0.0f), Vector3(posX, posY, 0.0f) );

							float	alongLine = (posX - point1X) / (point2X - point1X);
							alongLine = rage::Max(0.0f, alongLine);
							alongLine = rage::Min(1.0f, alongLine);
							float	height = (1.0f - alongLine) * point1Z + alongLine * point2Z;
							float	weight = 10.0f / (dist + 0.1f);
							totalHeight += height * weight;
							totalContribution += weight;
						}

						pNodesZ[iX+iY*OPEN_SPACE_NUM_X] = totalHeight / totalContribution;
						Assertf(pNodesZ[iX+iY*OPEN_SPACE_NUM_X] >= -100.0f && pNodesZ[iX+iY*OPEN_SPACE_NUM_X] <= 500.0f, "Calculated open space node outwidth reasonable z range");
					}
				}
			}
		}
		numOpenSpacePointsVisited += numPointsInSequence;
	}

	// Now go through all the nodes that are marked as within an open space.
	// Add these nodes to the list and also add the connections to neighbours.
	int	nodesAdded = 0;
	int	linksAdded = 0;
	for (s32 iX = 0; iX < OPEN_SPACE_NUM_X; iX++)
	{
		for (s32 iY = 0; iY < OPEN_SPACE_NUM_Y; iY++)
		{
			if (!pNodes[iX+iY*OPEN_SPACE_NUM_X])
			{
				continue;
			}

			float	posX, posY;
			FindOpenSpacePos(iX, iY, posX, posY);

			TempNodes[NumTempNodes].m_bAdditionalTunnelFlag = false;

			// Register the new node.
			StoreVehicleNode("open_space", nodesAdded, posX, posY, pNodesZ[iX+iY*OPEN_SPACE_NUM_X],
				true, false, false, //waternode
				SPECIAL_OPEN_SPACE, 1, 1.0f, 0, false, true, false, true, false, false, false, false, false, false, false, false, false);

			pNodesIndex[iX+iY*OPEN_SPACE_NUM_X] = nodesAdded;

			nodesAdded++;
		}
	}

	// Now we need to go through and register the links between nearby nodes.
	for (s32 iX = 0; iX < OPEN_SPACE_NUM_X; iX++)
	{
		for (s32 iY = 0; iY < OPEN_SPACE_NUM_Y; iY++)
		{
			if (!pNodes[iX+iY*OPEN_SPACE_NUM_X])
			{
				continue;
			}

			// Register the connections with neighbours.
			static int neighbourX[16] = { -1,  0, 1, 0, -1, -1,  1, 1,   2, 1, -1, -2, -2, -1,  1, 2 };
			static int neighbourY[16] = {  0, -1, 0, 1, -1,  1, -1, 1,   1, 2,  2,  1, -1, -2, -2, -1 };

			for (int link = 0; link < 16; link++)
			{
				int	neighbouriX = iX + neighbourX[link];
				int	neighbouriY = iY + neighbourY[link];

				if (neighbouriX < 0 || neighbouriX >= OPEN_SPACE_NUM_X ||
					neighbouriY < 0 || neighbouriY >= OPEN_SPACE_NUM_Y)
				{
					continue;
				}

				if (!pNodes[neighbouriX+neighbouriY*OPEN_SPACE_NUM_X])
				{
					continue;
				}

				if (pNodesIndex[iX+iY*OPEN_SPACE_NUM_X] >= pNodesIndex[neighbouriX+neighbouriY*OPEN_SPACE_NUM_X])
				{
					continue;
				}

				StoreVehicleLink("open_space", pNodesIndex[iX+iY*OPEN_SPACE_NUM_X], pNodesIndex[neighbouriX+neighbouriY*OPEN_SPACE_NUM_X],
					1, 1, 0.0f, true, true, false, false, false);

				linksAdded++;
			}
		}
	}

	// Free the (large) temporary storage for the open space nodes.
	sysMemAllocator::GetMaster().Free(pNodes);
	sysMemAllocator::GetMaster().Free(pNodesIndex);
	sysMemAllocator::GetMaster().Free(pNodesZ);

	Displayf("AddOpenSpaceNodes added %d nodes and %d links\n", nodesAdded, linksAdded);
}


/////////////////////////////////////////////////////////////////////////////////
//
// FUNCTION : SnapToGround
// PURPOSE :  Loads the collision on the map and snaps the nodes to the correct height and also
//			  samples the tilt. The tilt is used to improve the transition from virtual road surface to
//			  real road surface.
//
/////////////////////////////////////////////////////////////////////////////////

int CPathFindBuild::QuantizeTiltValue(float angle)
{
	float	smallestDiff = rage::Abs(saTiltValues[0] - angle);
	int		smallestDiffIndex = 0;

	for (int loop = 1; loop < NUM_NODE_TILT_VALUES; loop++)
	{
		float diff = rage::Abs(saTiltValues[loop] - angle);
		if (diff < smallestDiff)
		{
			smallestDiffIndex = loop;
			smallestDiff = diff;
		}
	}
	return smallestDiffIndex;
}


int CPathFindBuild::GetNextLargerTilt(int i)
{
	CompileTimeAssert(NUM_NODE_TILT_VALUES==29);
	if(i<0)			return -1;
	else if(i<14)	return i+1;
	else if(i==14)	return -1;
	else if(i==15)	return 0;
	else if(i<=28)	return i-1;
	else			return -1;
}


void CPathFindBuild::SnapToGround(const bool bDoSnap, const bool bDoCamber, const bool bDoJunctions)
{
	Displayf("Buildpaths: SnapToGround\n");

	g_procObjMan.Init();

	if(bDoSnap)
	{
		for (int regionX = 0; regionX < PATHFINDMAPSPLIT; regionX++)
		{
			for (int regionY = 0; regionY < PATHFINDMAPSPLIT; regionY++)
			{
				int Region = FIND_REGION_INDEX(regionX, regionY);

				Displayf("Buildpaths: SnapToGround Region:%i (%i,%i)\n", Region, regionX, regionY);

				if(!apRegions[Region] || !apRegions[Region]->NumNodes)
				{
					continue;
				}

				for (s32 node = 0; node < apRegions[Region]->NumNodes; node++)
				{
					CPathNode *pNode = &apRegions[Region]->aNodes[node];

					Vector3 posn;
					pNode->GetCoors(posn);

					// LoadScene crashes, so let's not do it
					if (false && !INSTANCE_STORE.GetBoxStreamer().HasLoadedAboutPos(RCC_VEC3V(posn), fwBoxStreamerAsset::MASK_MAPDATA))
					{
						g_SceneStreamerMgr.LoadScene(posn);
					}

					CStreaming::LoadAllRequestedObjects();
					CPhysics::LoadAboutPosition(RCC_VEC3V(posn));
					CStreaming::LoadAllRequestedObjects();
					
					float	maxSnap = 1.0f;
					if (pNode->m_1.m_specialFunction == SPECIAL_OPEN_SPACE)
					{
						maxSnap = 10.0f;
					}

					u32 roomId = 0;
					float nearestHeight = FindNearestGroundHeightForBuildPaths(posn, maxSnap, &roomId);

					if (rage::Abs(nearestHeight - posn.z) < maxSnap)
					{
						posn.z = nearestHeight;
					}
					pNode->SetCoors(posn);
					pNode->m_2.m_inTunnel = (roomId == 0 ? 0 : 1);

					Assertf(Sign(posn.x) == Sign(pNode->m_coorsX), "Oh dear, pathnode (%.1f, %.1f, %.1f) outside of X range", posn.x, posn.y, posn.z);
					Assertf(Sign(posn.y) == Sign(pNode->m_coorsY), "Oh dear, pathnode (%.1f, %.1f, %.1f) outside of Y range", posn.x, posn.y, posn.z);
				}	//end node iterator
			}	//end regionY iterator
		}
	}

	if (bDoJunctions)
	{
		for (int regionX = 0; regionX < PATHFINDMAPSPLIT; regionX++)
		{
			for (int regionY = 0; regionY < PATHFINDMAPSPLIT; regionY++)
			{
				int Region = FIND_REGION_INDEX(regionX, regionY);

				Displayf("Buildpaths: Virtual Junctions Region:%i (%i,%i)\n", Region, regionX, regionY);

				if(!apRegions[Region] || !apRegions[Region]->NumNodes)
				{
					continue;
				}

				for (s32 node = 0; node < apRegions[Region]->NumNodes; node++)
				{
					CPathNode *pNode = &apRegions[Region]->aNodes[node];
					//also create a virtual junction now if this is a junction node
					if (pNode->IsJunctionNode())
					{
						//create a cjunction and deploy it
						CJunction junction;
						junction.Clear();
						junction.Deploy(pNode->m_address, true, *this);

						//const Vector3 posn = pNode->GetPos();
						const Vector3 vJunctionMin = junction.GetJunctionMin();
						const Vector3 vJunctionMax = junction.GetJunctionMax();
						CStreaming::LoadAllRequestedObjects();
						CPhysics::LoadAboutPosition(RCC_VEC3V(vJunctionMin));
						CPhysics::LoadAboutPosition(RCC_VEC3V(vJunctionMax));
						CStreaming::LoadAllRequestedObjects();

						CPathVirtualJunction* pThisJunction = &apRegions[Region]->aVirtualJunctions[apRegions[Region]->NumJunctions];
						const int iStartIndexOfHeightSamples = apRegions[Region]->NumHeightSamples;
						CVirtualJunctionInterface junctionInterface(pThisJunction, apRegions[Region]);
						junctionInterface.InitAndSample(&junction, 2.0f, iStartIndexOfHeightSamples, *this);

						//add this junction into the binary map
						//region index -> junction index
						apRegions[Region]->JunctionMap.JunctionMap.Insert(pNode->m_address.RegionAndIndex(), apRegions[Region]->NumJunctions);

						apRegions[Region]->NumJunctions++;
					}
				}

				//sort the binary map for this region
				apRegions[Region]->JunctionMap.JunctionMap.FinishInsertion();
			}
		}
	}

	//--------------------------------------------------------------------------------------------------
	// Now go through the links and store the angle of the road surface to be used by the transition
	// to virtual surface code.

	if(bDoCamber)
	{
		for (int regionX = 0; regionX < PATHFINDMAPSPLIT; regionX++)
		{
			for (int regionY = 0; regionY < PATHFINDMAPSPLIT; regionY++)
			{
				int Region = FIND_REGION_INDEX(regionX, regionY);
				if(!apRegions[Region] || !apRegions[Region]->NumNodes)
				{
					continue;
				}

				Displayf("Buildpaths: Camber (Angle Links) Region:%i (%i,%i)\n", Region, regionX, regionY);

				for (s32 node = 0; node < apRegions[Region]->NumNodes; node++)
				{
					CPathNode *pNode = &apRegions[Region]->aNodes[node];

					Vector3 posn;
					pNode->GetCoors(posn);

					u32 roomId = 0;
					FindNearestGroundHeightForBuildPaths(posn,10.0f, &roomId);
					pNode->m_2.m_inTunnel = (roomId == 0) ? 0 : 1;

					for(u32 iLink = 0; iLink < pNode->NumLinks(); iLink++)
					{
						CPathNodeLink *pLink = &GetNodesLink(pNode, iLink);

						CPathNode *pOtherNode = GetNodesLinkedNode(pNode, iLink);
						if(!pOtherNode)
						{
							continue;
						}

						Vector3 posn2;
						pOtherNode->GetCoors(posn2);

						Vector3 vLinkCenter = (posn + posn2) * 0.5f;

						// Ignore certain nodes when it comes to analyzing tilt calculations
						bool bIgnoreSpecialLinkTiltErrors = false;
						bIgnoreSpecialLinkTiltErrors = bIgnoreSpecialLinkTiltErrors || (pNode->m_1.m_specialFunction == SPECIAL_PED_ASSISTED_MOVEMENT) || (pOtherNode->m_1.m_specialFunction == SPECIAL_PED_ASSISTED_MOVEMENT);
						bIgnoreSpecialLinkTiltErrors = bIgnoreSpecialLinkTiltErrors || (pNode->m_1.m_specialFunction == SPECIAL_PED_DRIVEWAY_CROSSING) || (pOtherNode->m_1.m_specialFunction == SPECIAL_PED_DRIVEWAY_CROSSING);
						bIgnoreSpecialLinkTiltErrors = bIgnoreSpecialLinkTiltErrors || (pNode->m_1.m_specialFunction == SPECIAL_PED_CROSSING) || (pOtherNode->m_1.m_specialFunction == SPECIAL_PED_CROSSING);
						bIgnoreSpecialLinkTiltErrors = bIgnoreSpecialLinkTiltErrors || pNode->m_2.m_waterNode || pOtherNode->m_2.m_waterNode;
						static bool bIgnoreNoNav = true;
						bIgnoreSpecialLinkTiltErrors = bIgnoreSpecialLinkTiltErrors || (bIgnoreNoNav && pLink->m_1.m_bDontUseForNavigation);

						// Other links should be considered "irregular" in the sense that they are likely to go off normal roads (i.e. shortcuts)
						bool bLinkIsIrregularType = pLink->m_1.m_bShortCut;

						Vector3 vPerpDir = Vector3(posn2.y - posn.y, posn.x - posn2.x, 0.0f);
						vPerpDir.Normalize();

						// Width of link for camber determination
						float fCamberProbesHorizontalOffset;

						float fRoadWidthLeft, fRoadWidthRight;
						const bool bAllLanesThoughCentre = (pLink->m_1.m_Width == ALL_LANES_THROUGH_CENTRE_FLAG_VAL);
						FindRoadBoundaries(
							pLink->m_1.m_LanesToOtherNode,
							pLink->m_1.m_LanesFromOtherNode,
							static_cast<float>(pLink->m_1.m_Width),
							pLink->m_1.m_NarrowRoad,
							bAllLanesThoughCentre,
							fRoadWidthLeft, fRoadWidthRight);

						fCamberProbesHorizontalOffset = Max(fRoadWidthRight-ROAD_EDGE_BUFFER_FOR_CAMBER_CALCULATION,0.1f);

						// Find the tilt
						static bool bUseComplex = true;
						int iTilt = 0, iFalloff = 0;
						if(!bUseComplex)
						{
							FindTiltForLink_Simple(vLinkCenter,vPerpDir,fCamberProbesHorizontalOffset,iTilt);
						}
						else
						{
							float fCentreMedianWidth = bAllLanesThoughCentre ? 0.0f : ((float)(pLink->m_1.m_Width)/2.0f);
							FindTiltForLink_Complex(vLinkCenter,vPerpDir,fCamberProbesHorizontalOffset,fCentreMedianWidth,!bIgnoreSpecialLinkTiltErrors,bLinkIsIrregularType,iTilt,iFalloff);
						}

						pLink->m_1.m_Tilt = iTilt;
						pLink->m_1.m_TiltFalloff = iFalloff;
					}
				}
			}
		}
	}

	g_procObjMan.Shutdown();

}


void CPathFindBuild::FindTiltForLink_Simple(const Vector3 & vLinkCenter, const Vector3 & vLinkPerp, float fWidth, int & iTiltOut)
{
	CStreaming::LoadAllRequestedObjects();
	CPhysics::LoadAboutPosition(RCC_VEC3V(vLinkCenter));
	CStreaming::LoadAllRequestedObjects();

	// Find heights
	const Vector3 vSample1 = vLinkCenter + fWidth * vLinkPerp;
	const Vector3 vSample2 = vLinkCenter;
	float fHeight1 = FindNearestGroundHeightForBuildPaths(vSample1, 10.0f, NULL);
	float fHeight2 = FindNearestGroundHeightForBuildPaths(vSample2, 10.0f, NULL);
	float fAngle = rage::Atan2f( (fHeight1 - fHeight2) , fWidth );
	iTiltOut = QuantizeTiltValue(fAngle);
}


float CPathFindBuild::CalcSumErrorSqForLine(int iNumSamples, float * fZSamples, float fWidth, float fSlope)
{
	float fSumErrorSq = 0.0f;
	for(int iSample=0; iSample<iNumSamples; iSample++)
	{
		const float fX = (fWidth * iSample)/(iNumSamples-1);
		const float fZFromLine = fX * fSlope;
		const float fError = fZFromLine - fZSamples[iSample];
		fSumErrorSq += square(fError);
	}
	return sqrtf(fSumErrorSq);
}

float CPathFindBuild::ZForLineWithFallof(float fX, float fWidth, float fSlope, float fFalloffWidth, float fFalloffHeight)
{
	const float fZFromLine = fX * fSlope;
	const float fXDistFromEdge = fWidth - fX;
	const float fZFromFalloff = - fFalloffHeight * Max( 1.0f - fXDistFromEdge / fFalloffWidth, 0.0f);
	return fZFromLine + fZFromFalloff;
}


float CPathFindBuild::CalcSumErrorSqForLineWithFalloff(int iNumSamples, float * fZSamples, float fWidth, float fSlope, float fFalloffWidth, float fFalloffHeight)
{
	float fSumErrorSq = 0.0f;
	for(int iSample=0; iSample<iNumSamples; iSample++)
	{
		const float fX = (fWidth * iSample)/(iNumSamples-1);
		const float fZ = ZForLineWithFallof(fX,fWidth,fSlope,fFalloffWidth,fFalloffHeight);
		const float fError = fZ - fZSamples[iSample];
		fSumErrorSq += square(fError);
	}
	return sqrtf(fSumErrorSq);
}


void CPathFindBuild::CalcMaxAndAverageErrorForLineWithFalloff(int iNumSamples, float * fZSamples, float fWidth, float fSlope, float fFalloffWidth, float fFalloffHeight, float & fErrorMaxOut, float & fErrorAveOut)
{
	float fErrorSum = 0.0f;
	fErrorMaxOut = 0.0f;
	for(int iSample=0; iSample<iNumSamples; iSample++)
	{
		const float fX = (fWidth * iSample)/(iNumSamples-1);
		const float fZ = ZForLineWithFallof(fX,fWidth,fSlope,fFalloffWidth,fFalloffHeight);
		const float fErrorAbs = fabsf(fZ - fZSamples[iSample]);
		fErrorSum += fErrorAbs;
		if(fErrorAbs > fErrorMaxOut)
		{
			fErrorMaxOut = fErrorAbs;
		}
	}

	fErrorAveOut = fErrorSum / iNumSamples;
}

#if !__NO_OUTPUT
void CPathFindBuild::PrintMaterialsOnLink(int kNumSamples, phMaterialMgr::Id * pMaterialIds)
{
	Printf("  - Materials: ");
	for(int i=0; i<kNumSamples; i++)
	{
		const char * pMtlName = PGTAMATERIALMGR->GetMaterial(pMaterialIds[i]).GetName();
		u32 mtlNameHashed = atStringHash(pMtlName);
		bool bIsIrregularMaterial = IsMaterialIrregularSurface(pMaterialIds[i]);
		Printf("(%s,0x%x,%d) ",pMtlName,mtlNameHashed,bIsIrregularMaterial);
	}
	Displayf("");
}
#endif

bool CPathFindBuild::IsMaterialIrregularSurface(phMaterialMgr::Id mtlId)
{
	static u32 mtlsWithIrregularSurface[] = 
	{
		0x09424a9f, // "dirt_track"
		0x72623394, // "grass"
		0x79c6b603, // "gravel_small"
		0xd5fa0b8b, // "mud_hard"
		0x8eea76dd, // "sand_compact"
		0x97fc89ba, // "sand_loose"
		0xb6b0644e, // "sand_track"
	};
	int kNumMaterialsToIgnore = NELEM(mtlsWithIrregularSurface);
	const char * pMtlName = PGTAMATERIALMGR->GetMaterial(mtlId).GetName();
	u32 mtlNameHashed = atStringHash(pMtlName);
	for(int i=0; i<kNumMaterialsToIgnore; i++)
	{
		if(mtlNameHashed == mtlsWithIrregularSurface[i])
		{
			return true;
		}
	}
	return false;
}


bool CPathFindBuild::IsLinkAllIrregularMaterials(int kNumSamples, phMaterialMgr::Id * pMaterialIds)
{
	bool bAllIrregularMaterials = true;
	for(int i=0; i<kNumSamples; i++)
	{
		bAllIrregularMaterials = bAllIrregularMaterials && IsMaterialIrregularSurface(pMaterialIds[i]);
	}
	return bAllIrregularMaterials;
}


void CPathFindBuild::FindTiltForLink_Complex(const Vector3 & vLinkCenter, const Vector3 & vLinkPerp, float fTotalWidth, float fCentreWidth, bool bReportIfInaccurate, bool bIsIrregularLinkType, int & iTiltOut, int & iFalloffOut)
{
	CStreaming::LoadAllRequestedObjects();
	CPhysics::LoadAboutPosition(RCC_VEC3V(vLinkCenter));
	CStreaming::LoadAllRequestedObjects();

	// Sample height at several points along the perpendicular
	Vector3 vStart = vLinkCenter;
	Vector3 vEnd = vLinkCenter + fTotalWidth * vLinkPerp;
	const int kNumSamples = 5;
	Assert(kNumSamples>1);
	float fZ[kNumSamples];
	phMaterialMgr::Id materialHits[kNumSamples];

	// Possibly skip probes on the central median.  If there is a median and the road is wider than the indicated median, then
	// consider samples within the median to be at the height of the link center.  Avoids spurious samples at the height of the median.
	static bool bEnableMedianSkipping = true;
	const float fNonMedianBuffer = 1.5f;	// Amount the width must exceed the central median width in order to trigger skipping probes on the central median.
	bool bSkipSamplesWithinMedian = bEnableMedianSkipping && (fTotalWidth - fCentreWidth > fNonMedianBuffer);

	for(int iSample=0; iSample<kNumSamples; iSample++)
	{
		Vector3 vSample;
		const float fT = ((float)iSample)/(kNumSamples-1);
		vSample.Lerp(fT,vStart,vEnd);
		float fDist = fT * fTotalWidth;
		if(bSkipSamplesWithinMedian && fDist < fCentreWidth)
		{
			fZ[iSample] = 0.0f;
		}
		else
		{
			static float fRange = 5.0f;
			fZ[iSample] = FindNearestGroundHeightForBuildPaths(vSample,fRange,NULL,materialHits+iSample);
			fZ[iSample] -= vLinkCenter.z;
		}
	}

	// Calc the best quantized angle from best floating-point angle
	int iTiltBestLine = -1;
	float fErrorSqBestLine = FLT_MAX;
	{
		// Simplified version of slope/intercept regression.  In this case the intercept is fixed (to be the link point).
		float fSumXX = 0.0f;
		float fSumXZ = 0.0f;
		for(int iSample=0; iSample<kNumSamples; iSample++)
		{
			const float fX = fTotalWidth * ((float)iSample)/(kNumSamples-1);
			fSumXX += square(fX);
			const float fXZ = fX * fZ[iSample];
			fSumXZ += fXZ;
		}
		const float fSlope = fSumXZ / fSumXX;
		const float fAngle = atanf(fSlope);
		iTiltBestLine = QuantizeTiltValue(fAngle);
		fErrorSqBestLine = CalcSumErrorSqForLine(kNumSamples,fZ,fTotalWidth,fSlope);
	}

	iTiltOut = iTiltBestLine;
	iFalloffOut = 0;

	const float fFalloffWidth = LINK_TILT_FALLOFF_WIDTH;
	const float fFalloffHeight = LINK_TILT_FALLOFF_HEIGHT;

	// Check higher tilts with a falloff
	int iTiltBestLineWithFalloff = iTiltBestLine;
	float fErrorSqBestLineWithFalloff = FLT_MAX;
	{
		const int kNumHigherTiltsToTest = 2;
		for(int i=0, iTilt=iTiltBestLine; i<kNumHigherTiltsToTest && iTilt>=0; i++)
		{
			float fSlope = tanf(saTiltValues[iTilt]);
			float fErrorSq = CalcSumErrorSqForLineWithFalloff(kNumSamples,fZ,fTotalWidth,fSlope,fFalloffWidth,fFalloffHeight);
			if(fErrorSq < fErrorSqBestLineWithFalloff)
			{
				iTiltBestLineWithFalloff = iTilt;
				fErrorSqBestLineWithFalloff = fErrorSq;
			}
			iTilt = GetNextLargerTilt(iTilt);
		}
	}

	static bool bUseFalloff = true;
	if(bUseFalloff && fErrorSqBestLine > fErrorSqBestLineWithFalloff)
	{
		//Displayf("(falloff line better) %d %d, at %0.2f, %0.2f",iTiltBestLine,iTiltBestLineWithFalloff);
		//for(int iSample=0; iSample<kNumSamples; iSample++)
		//{
		//	const float fX = fWidth * ((float)iSample)/(kNumSamples-1);
		//	Displayf("%d %0.3f %0.3f",iSample,fX,fZ[iSample]);
		//}
		iTiltOut = iTiltBestLineWithFalloff;
		iFalloffOut = 1;
	}

	if(bReportIfInaccurate)
	{
		float fSlope = tanf(saTiltValues[iTiltOut]);
		float fErrorMax, fErrorAve;
		CalcMaxAndAverageErrorForLineWithFalloff(kNumSamples,fZ,fTotalWidth,fSlope,fFalloffWidth,fFalloffHeight,fErrorMax,fErrorAve);

		// If a link exceeds these error thresholds, then report it.
		// For now, start with large error values and only decrease these after those bugs are fixed.  Later these could be command-line parameters.
		static float fMaxErrorThresholdNormal = 0.50f;
		static float fAverageErrorThresholdNormal = 0.25f;
		static float fMaxErrorThresholdIrregular = 0.50f;
		static float fAverageErrorThresholdIrregular = 0.25f;

		// "Irregular" links are shortcuts (and other types of links?) that are likely to travel off of normal roads, or
		// links that are entirely on off-road materials (like rock, dirt, etc.).  Links that are partially on off-road materials
		// are not considered irregular since it is possible they are too wide and going from on-road to off-road.

		bool bIsIrregular = bIsIrregularLinkType || IsLinkAllIrregularMaterials(kNumSamples,materialHits);
	
		float fMaxErrorThreshold = bIsIrregular ? fMaxErrorThresholdIrregular : fMaxErrorThresholdNormal;
		float fAverageErrorThreshold = bIsIrregular ? fAverageErrorThresholdIrregular : fAverageErrorThresholdNormal;
		if(fErrorMax > fMaxErrorThreshold || fErrorAve > fAverageErrorThreshold)
		{
			Printf("Link at (%0.2f %0.2f %0.2f) with high error - %0.2f max, %0.2f average. ",vLinkCenter.x,vLinkCenter.y,vLinkCenter.z,fErrorMax,fErrorAve);
			OUTPUT_ONLY(PrintMaterialsOnLink(kNumSamples,materialHits);)
		}
	}
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : CountFloodFillGroups
// PURPOSE :  The idea here is to do a flood fill and count the number of groups
//			  we encounter this way. Hopefully we would get one group. If we get
//			  more than that the grid is not connected and the artists have made
//			  a mistake. We can then display a little coloured flag for each
//			  group so that we can find the bit where the connection is broken.
/////////////////////////////////////////////////////////////////////////////////
void CPathFindBuild::CountFloodFillGroups()
{
	Displayf("Buildpaths:CountFloodFillGroups\n");

	CPathNode		*pCurrNode, *pCandidateNode;
	CPathNode		*pToDoList, *pSearchNode;
	u32			link;
	s32			StartNode=0, EndNode=0, Node;
	CNodeAddress	CandidateNode;
	u16			CurrentColour;

	for (s32 Region = 0; Region < PATHFINDREGIONS; Region++)
	{
		if(!apRegions[Region] || !apRegions[Region]->NumNodesCarNodes)
		{
			continue;
		}

		StartNode = 0;
		EndNode = apRegions[Region]->NumNodesCarNodes;

		// Clear the nodes
		for (Node = StartNode; Node < EndNode; Node++)
		{
			(apRegions[Region]->aNodes)[Node].m_1.m_group = 0;
		}
	}

	CurrentColour = 0;

	while (1)			// Keep going through new groups
	{
		CurrentColour++;				// New m_1.m_group

		if (CurrentColour > 15000)		// Too many groups. Something's seriously wrong
		{
			Assertf(0, "Too many groups (network restart points?)");
		}

		// Find a node to begin with (the first not coloured node)
		s32 SearchRegion = 0;
		s32 SearchNode;

		while (1)
		{		
			if(apRegions[SearchRegion])
			{

				StartNode = 0;
				EndNode = apRegions[SearchRegion]->NumNodesCarNodes;

				for (SearchNode = StartNode; SearchNode < EndNode; SearchNode++)
				{
					pSearchNode = &(apRegions[SearchRegion]->aNodes)[SearchNode];

					if (pSearchNode->m_1.m_group == 0 &&
						pSearchNode->m_2.m_noGps == 0 &&		// Uncomment if we got no space left in m_group
						pSearchNode->m_2.m_waterNode == 0 &&	// Only used by AI and there aint lots of these nodes anyway
						pSearchNode->m_1.m_specialFunction != SPECIAL_HIDING_NODE &&
						pSearchNode->m_1.m_specialFunction != SPECIAL_PED_CROSSING &&
						pSearchNode->m_1.m_specialFunction != SPECIAL_PED_DRIVEWAY_CROSSING &&
						pSearchNode->m_1.m_specialFunction != SPECIAL_PED_ASSISTED_MOVEMENT)
					{
						// We found a non coloured node. Use this to flood fill the area.
						Vector3 nodePos(0.0f, 0.0f, 0.0f);
						pSearchNode->GetCoors(nodePos);
						Displayf("New island search from node: %f %f %f", nodePos.x, nodePos.y, nodePos.z);

						goto ___FoundANodeToGrowFrom;
					}			
				}
			}

			SearchRegion++;
			if (SearchRegion >= PATHFINDREGIONS)
			{
				Displayf("Number of groups:%d\n", CurrentColour);
				return;		// No non-coloured nodes left. Our work is done here.
			}
		}

___FoundANodeToGrowFrom:
		// Put the start node on the list
		pToDoList = pSearchNode;
		pToDoList->SetNext(NULL);
		pSearchNode->m_1.m_group = (u8)CurrentColour;

		if(pToDoList->NumLinks() == 0)
		{		// Print out isolated nodes
#if __DEV
			Vector3 nodePos(0.0f, 0.0f, 0.0f);
			pToDoList->GetCoors(nodePos);
			Displayf("Single car node: %f %f %f\n", nodePos.x, nodePos.y, nodePos.z);
#endif // __DEV
		}

		while (pToDoList)	// while still nodes left
		{
			// Go through the list for this Hash value.
			pCurrNode = pToDoList;
			pToDoList = pToDoList->GetNext();

			// Process the neighbours
			for(link = 0; link < pCurrNode->NumLinks(); link++)
			{
				CandidateNode = GetNodesLinkedNodeAddr(pCurrNode, link);

				pCandidateNode = FindNodePointer(CandidateNode);
				if (pCandidateNode->m_1.m_group == 0 && !pCandidateNode->m_2.m_noGps)
				{
					pCandidateNode->m_1.m_group =(u8)CurrentColour;		// laneCol this bloke in
					// Since group is only 5 bits; 32 turns into 0 locking up the algorithm. This is bad.
					if (!pCandidateNode->m_1.m_group) pCandidateNode->m_1.m_group = 7;	// Fudge this for now (It's only a test anyway)
					pCandidateNode->SetNext(pToDoList);
					pToDoList = pCandidateNode;
				}
			}
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : CheckGrid
// PURPOSE :  Any check we do to make sure the grid is allright should be put here.
//			  Among other things we look for isolated ped nodes.
/////////////////////////////////////////////////////////////////////////////////
void CPathFindBuild::CheckGrid(void)
{
	Displayf("Buildpaths:CheckGrid\n");

	s32	IsolatedNodes, Node, NumLinks, Node1, Node2, Region;
	bool	bAssertAtEnd = false;

	// Go through the pedestrian nodes and count the ones without neighbours.
	// This should not happen.
	IsolatedNodes = 0;

	for (Region = 0; Region < PATHFINDREGIONS; Region++)
	{
		if(!apRegions[Region])
		{
			continue;
		}

		for (Node = 0; Node < apRegions[Region]->NumNodesCarNodes; Node++)
		{
			NumLinks = apRegions[Region]->aNodes[Node].NumLinks();

			if (NumLinks == 0)
			{
				IsolatedNodes++;
				// Also place a marker
#if __DEV
				Vector3 nodePos(0.0f, 0.0f, 0.0f);
				apRegions[Region]->aNodes[Node].GetCoors(nodePos);
				Displayf("Isolated node: %f %f %f\n", nodePos.x, nodePos.y, nodePos.z );
#endif // __DEV
				//Vector3 Temp = apRegions[Region]->aNodes[Node].GetCoors();
			}
		}
	}
	if (IsolatedNodes)
	{
		Displayf("***************** BAD\n");
		Displayf("***************** BAD\n");
		Displayf("***************** There are %d isolated path nodes.\n", IsolatedNodes);
	}

	// We don't want any nodes that have a lanes coming in but no lanes going out.
	// They will create pile ups and are usually a sign of badly placed nodes.
	for (Region = 0; Region < PATHFINDREGIONS; Region++)
	{
		if(!apRegions[Region])
		{
			continue;
		}

		for (Node = 0; Node < apRegions[Region]->NumNodesCarNodes; Node++)
		{
			CNodeAddress	TestNode;
			TestNode.Set(Region, Node);
			NumLinks = apRegions[Region]->aNodes[Node].NumLinks();

			if (NumLinks != 0)
			{
				if (apRegions[Region]->aNodes[Node].m_1.m_specialFunction != SPECIAL_PARKING_PARALLEL &&
					apRegions[Region]->aNodes[Node].m_1.m_specialFunction != SPECIAL_PARKING_PERPENDICULAR)
				{
					s32	nodeStartIndexOfLinks = apRegions[Region]->aNodes[Node].m_startIndexOfLinks;
					bool	bLanesIn = false;
					bool	bLanesOut = false;

					for (s32 N = 0; N < NumLinks; N++)
					{
						CPathNodeLink *pAdjLink = &apRegions[Region]->aLinks[nodeStartIndexOfLinks+N];
						if (pAdjLink->m_1.m_LanesFromOtherNode) bLanesIn = true;
						if (pAdjLink->m_1.m_LanesToOtherNode) bLanesOut = true;

					}
					if (bLanesIn && !bLanesOut)
					{
						Vector3 nodePos(0.0f, 0.0f, 0.0f);
						apRegions[Region]->aNodes[Node].GetCoors(nodePos);
						Displayf("Node with only lanes coming in: %f %f %f\n", nodePos.x, nodePos.y, nodePos.z );
					}
				}
			}
		}
	}

	// We will go through the nodes and make sure they're all different.
	// Nodes with identical coordinates are obviously a mistake.
	for (Region = 0; Region < PATHFINDREGIONS; Region++)
	{
		if(!apRegions[Region])
		{
			continue;
		}

		for (Node1 = 0; Node1 < (apRegions[Region]->NumNodesCarNodes-1); Node1++)
		{
			for (Node2 = Node1+1; Node2 < apRegions[Region]->NumNodesCarNodes; Node2++)
			{
				Vector3 node1Coors;
				Vector3 node2Coors;
				apRegions[Region]->aNodes[Node1].GetCoors(node1Coors);
				apRegions[Region]->aNodes[Node2].GetCoors(node2Coors);
				if (node1Coors == node2Coors/*apRegions[Region]->aNodes[Node1].GetCoorsX() == apRegions[Region]->aNodes[Node2].GetCoorsX() && apRegions[Region]->aNodes[Node1].GetCoorsY() == apRegions[Region]->aNodes[Node2].GetCoorsY() && apRegions[Region]->aNodes[Node1].GetCoors().z == apRegions[Region]->aNodes[Node2].GetCoors().z*/)
				{
					Displayf("ALERT: 2 car nodes have identical coordinates\n");
					Displayf("Coordinates:%f %f %f.\n", node1Coors.x, node1Coors.y, node1Coors.z);
					bAssertAtEnd = true;
				}
			}	
		}
	}
	// Also for ped nodes
	for (Region = 0; Region < PATHFINDREGIONS; Region++)
	{
		if(!apRegions[Region] || !apRegions[Region]->NumNodes)
		{
			continue;
		}

		for (Node1 = apRegions[Region]->NumNodesCarNodes; Node1 < (apRegions[Region]->NumNodes-1); Node1++)
		{
			for (Node2 = Node1+1; Node2 < apRegions[Region]->NumNodes; Node2++)
			{
				Vector3 node1Coors;
				Vector3 node2Coors;
				apRegions[Region]->aNodes[Node1].GetCoors(node1Coors);
				apRegions[Region]->aNodes[Node2].GetCoors(node2Coors);
				if (node1Coors == node2Coors/*apRegions[Region]->aNodes[Node1].GetCoors().x == apRegions[Region]->aNodes[Node2].GetCoors().x && apRegions[Region]->aNodes[Node1].GetCoors().y == apRegions[Region]->aNodes[Node2].GetCoors().y && apRegions[Region]->aNodes[Node1].GetCoors().z == apRegions[Region]->aNodes[Node2].GetCoors().z*/)
				{
					Displayf("ALERT: 2 ped nodes have identical coordinates\n");

#if __DEV
					Vector3 nodePos(0.0f, 0.0f, 0.0f);
					apRegions[Region]->aNodes[Node1].GetCoors(nodePos);
					Displayf("Coordinates:%f %f %f.\n", nodePos.x, nodePos.y, nodePos.z );
#endif // __DEV			
					bAssertAtEnd = true;
				}
			}	
		}
	}

	if (bAssertAtEnd) { Assert(0); }
}



/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : CalcRoadDensity
// PURPOSE :  Calculates the average density of roads in a certain area. The idea
//			  is that areas with more roads in them get more cars created. This
//			  way a country lane doesn't get flooded with cars and a motorway
//			  would get a reasonable number of cars.
//			  The result should be about 1.0f for an average road density
/////////////////////////////////////////////////////////////////////////////////
float CPathFindBuild::CalcRoadDensity(float TestX, float TestY)
{
	float			DensityCount = 0;
	s32			Node;
	u32			Link;
	s32			Region;
	s32			DbgNodesInArea = 0;
	float			Length, DeltaX, DeltaY;
	CPathNode		*pNode, *pOtherNode;
	CNodeAddress	OtherNode;
#define AREA_OF_INTEREST (80.0f)	// in meters in +X, -X, +Y and -Y direction.
#define AVERAGE_VALUE 	 (2500.0f)		// What is an average value that we could expect

	// Go through all the nodes in the world and count the ones that are within our
	// area of interest.
	for (Region = 0; Region < PATHFINDREGIONS; Region++)
	{
		if(!apRegions[Region])
		{
			continue;
		}

		if (apRegions[Region] && apRegions[Region]->aNodes)	// Make sure this Region is loaded
		{
			for (Node = 0; Node < apRegions[Region]->NumNodesCarNodes; Node++)
			{
				pNode = &apRegions[Region]->aNodes[Node];
				Vector2 nodePos(0.0f, 0.0f);
				pNode->GetCoors2(nodePos);
				if ( rage::Abs(nodePos.x - TestX) < AREA_OF_INTEREST &&
					rage::Abs(nodePos.y - TestY) < AREA_OF_INTEREST)
				{	// This node needs to be counted.
					DbgNodesInArea++;
					if(pNode->NumLinks())		// No nodes without mates please(bad level)
					{
						// Go through the links (to count the number of lanes
						for(Link = 0; Link < pNode->NumLinks(); Link++)
						{
							CPathNodeLink *pLink = &GetNodesLink(pNode, Link);
							OtherNode = pLink->m_OtherNode;
							if (IsRegionLoaded(OtherNode))
							{
								pOtherNode = FindNodePointer(OtherNode);
								Vector2 otherNodePos(0.0f, 0.0f);
								pOtherNode->GetCoors2(otherNodePos);

								// Calculate the length of the link.
								DeltaX = nodePos.x - otherNodePos.x;
								DeltaY = nodePos.y - otherNodePos.y;
								Length = rage::Sqrtf(DeltaX*DeltaX + DeltaY*DeltaY);

								DensityCount += Length * pLink->m_1.m_LanesFromOtherNode;
								DensityCount += Length * pLink->m_1.m_LanesToOtherNode;

							}
						}	
					}
				}
			}
		}
	}
	return(DensityCount / AVERAGE_VALUE);
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : StoreVehicleNode
// PURPOSE :  Stores one node to be used by vehicles.
//			  pFileName String of the name of the file it is read from.
//			  NodeIndex is the index of the node within the file.
//			  X, Y, Z coordinates in world space in meters.
//			  bSwitchedOff. If this is true cars shouldn't go here.
//			  bRoadBlock. This is a good place for a roadblock.
//			  bWaterNode. This node is for boats and stuff.
//			  bUnderBridge. This node is under a bridge so big boats shouldn't go here.
//			  SpecialFunction. Toll booth, parking place etc
//			  Speed (0 = slow, 1 = normal, 2 = fast)
//			  Width. Size of the space between opposite lanes in meters
/////////////////////////////////////////////////////////////////////////////////
void CPathFindBuild::StoreVehicleNode(const char *pFileName, s32 NodeIndex, float X, float Y, float Z,
								 bool bSwitchedOff, bool bLowBridge, bool bWaterNode,
								 u32 SpecialFunction, s32 Speed, float Density, u32 StreetNameHash, 
								 bool bHighway, bool bNoGps, bool bAdditionalTunnelFlag, bool bOpenSpace, 
								 bool UNUSED_PARAM(usesHighResCoors), bool bLeftOnly, bool bNoLeftTurns, 
								 bool bNoRightTurns, bool bOffroad, bool bNoBigVehicles,
								 bool bIndicateKeepLeft, bool bIndicateKeepRight, bool bIsSlipNode)
{
	if (!TempNodes)
	{		// If we're not looking to rebuild the path data we don't have to store this
		return;
	}

	Assert(SpecialFunction < SPECIAL_USE_LAST);
	PathBuildFatalAssertf(NumTempNodes < PF_TEMPNODES, "");

	TempNodes[NumTempNodes].m_Coors.x = X;
	TempNodes[NumTempNodes].m_Coors.y = Y;
	TempNodes[NumTempNodes].m_Coors.z = Z;
	TempNodes[NumTempNodes].m_NodeIndex = NodeIndex;
	TempNodes[NumTempNodes].m_FileId = atStringHash(pFileName);
	TempNodes[NumTempNodes].m_StreetNameHash = StreetNameHash;
	TempNodes[NumTempNodes].m_bSwitchedOff = bSwitchedOff;
	TempNodes[NumTempNodes].m_SpecialFunction = (u8)(SpecialFunction);
	TempNodes[NumTempNodes].m_Speed = (u8)(Speed);
	TempNodes[NumTempNodes].m_bNoGps = bNoGps;
	TempNodes[NumTempNodes].m_bAdditionalTunnelFlag = bAdditionalTunnelFlag;
	TempNodes[NumTempNodes].m_bWaterNode = bWaterNode;
	TempNodes[NumTempNodes].m_bOpenSpace = bOpenSpace;
	TempNodes[NumTempNodes].m_bLeftOnly = bLeftOnly;
	TempNodes[NumTempNodes].m_bNoLeftTurns = bNoLeftTurns;
	TempNodes[NumTempNodes].m_bNoRightTurns = bNoRightTurns;
	TempNodes[NumTempNodes].m_bOffroad = bOffroad;
	TempNodes[NumTempNodes].m_bNoBigVehicles = bNoBigVehicles;
	TempNodes[NumTempNodes].m_bIndicateKeepLeft = bIndicateKeepLeft;
	TempNodes[NumTempNodes].m_bIndicateKeepRight = bIndicateKeepRight;
	TempNodes[NumTempNodes].m_bSlipNode	= bIsSlipNode;

	if (bWaterNode)
	{
		TempNodes[NumTempNodes].m_bHighwayOrLowBridge = bLowBridge;
	}
	else
	{
		TempNodes[NumTempNodes].m_bHighwayOrLowBridge = bHighway;
	}

	TempNodes[NumTempNodes].m_Density = (u8)(rage::round(rage::Min(Density, 1.0f) * 15.0f));

	NumTempNodes++;
	PathBuildFatalAssertf(NumTempNodes < PF_TEMPNODES,"");
}



/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : StoreVehicleLink
// PURPOSE :  Stores a link between two vehicle nodes.
//			  pFileName String of the name of the file it is read from.
//            Node1 and Node2 are the 2 nodes that the link connects.
//			  Lanes1To2 The number of lanes that go from Node1 to Node2
//			  Lanes2To1 The number of lanes that go the other way
//			  Width. Size of the space between opposite lanes in meters
/////////////////////////////////////////////////////////////////////////////////
void CPathFindBuild::StoreVehicleLink(	const char *pFileName, s32 Node1, s32 Node2,
										s32 Lanes1To2, s32 Lanes2To1, float Width,
										bool bNarrowRoad, bool bGpsCanGoBothWays, bool bShortcut, 
										bool bIgnoreForNav, bool bBlockIfNoLanes )
{
	Assertf(Node1 <= 100000 && Node2 <= 100000, "link with corrupt values (%d, %d) in %s", Node1, Node2, pFileName);

	// Find the nodes that this link connects.

	// The following is just debug code to detect links between the same node.
#if __DEV
	if (Node1 == Node2)
	{
		// Try to find the node that is the problem (hopefully it is already read in)
		u32	FileId = atStringHash(pFileName);
		s32	TestNode = 0;
		s32	NodeToFind = Node1;
		char debugText[100];

		sprintf(debugText, "There is a link set up between 1 node and itself (%s)", pFileName);

		while (TestNode < NumTempNodes && NodeToFind >= 0)
		{
			if (FileId == TempNodes[TestNode].m_FileId)
			{
				NodeToFind--;
				if (NodeToFind < 0)
				{		// We found our node.
					sprintf(debugText, " %f %f %f", TempNodes[NumTempNodes].m_Coors.x, TempNodes[NumTempNodes].m_Coors.y, TempNodes[NumTempNodes].m_Coors.z);
				}
			}

			TestNode++;
		}
		Assertf(0, "%s", debugText);
	}
#endif 


	if (!TempLinks)
	{		// If we're not looking to rebuild the path data we don't have to store this
		return;
	}

	PathBuildFatalAssertf(NumTempLinks < PF_TEMPLINKS, "");

	// This can be a link between car nodes or ped nodes.
	TempLinks[NumTempLinks].m_FileId = atStringHash(pFileName);
	TempLinks[NumTempLinks].m_Node1 = Node1;
	TempLinks[NumTempLinks].m_Node2 = Node2;
	TempLinks[NumTempLinks].m_Lanes1To2 = (u8)(Lanes1To2);
	TempLinks[NumTempLinks].m_Lanes2To1 = (u8)(Lanes2To1);
	TempLinks[NumTempLinks].m_Width = (u8)rage::Min(Width, 14.0f);
	if (Width < 0.0f)
	{
		TempLinks[NumTempLinks].m_Width = ALL_LANES_THROUGH_CENTRE_FLAG_VAL;
	}

	TempLinks[NumTempLinks].m_NarrowRoad = bNarrowRoad;
	TempLinks[NumTempLinks].m_bGpsCanGoBothWays = bGpsCanGoBothWays;
	TempLinks[NumTempLinks].m_bShortCut = bShortcut;
	TempLinks[NumTempLinks].m_bDontUseForNavigation = bIgnoreForNav;
	TempLinks[NumTempLinks].m_bBlockIfNoLanes = bBlockIfNoLanes;

	NumTempLinks++;
}

/*
// JAY, not sure if this is required.
void CPathFindBuild::CreateRegionsForPathBuild()
{

//	CFileMgr::SetDir("platform:/data/paths/");

	{
		// Allocate some memory to play with so that we can build the files as required by the streaming.
		for (s32 Region = 0; Region < PATHFINDREGIONS; Region++)
		{
			// DEBUG!! -AC, MEMLEAK: fix?
			Assert(!apRegions[Region]);
			apRegions[Region] = rage_new CPathRegion();
			Assert(apRegions[Region]);
			// END DEBUG!!

			//apRegions[Region]->aNodes = (CPathNode *) GtaMalloc(MAXNODESPERREGION * sizeof(CPathNode));
			apRegions[Region]->aNodes = (CPathNode *) sysMemAllocator::GetMaster().Allocate(MAXNODESPERREGION * sizeof(CPathNode), 16, MEMTYPE_RESOURCE_VIRTUAL);
			Assert(apRegions[Region]->aNodes);
			memset(apRegions[Region]->aNodes, 0, MAXNODESPERREGION * sizeof(CPathNode));
			for (s32 T = 0; T < MAXNODESPERREGION; T++)
			{
				apRegions[Region]->aNodes[T].m_distanceToTarget = PF_VERYLARGENUMBER;
			}

			//apRegions[Region]->aLinks = (CPathNodeLink *) GtaMalloc((MAXLINKSPREREGION + (MAX_NUM_DYNAMIC_LINKS * PF_MAXNUMNODESPEROBJECT)) * sizeof(CPathNodeLink));
			apRegions[Region]->aLinks = (CPathNodeLink *) sysMemAllocator::GetMaster().Allocate( (MAXLINKSPREREGION) * sizeof(CPathNodeLink) , 16, MEMTYPE_RESOURCE_VIRTUAL);
			Assert(apRegions[Region]->aLinks);
			// Set all the adjacent nodes to emptyAddr. This is important because the dynamic adjacent nodes(at the end)
			// should be set to emptyAddr so that we know they are not being used yet.
			// Make sure the extra adjacent nodes(for dynamic links)are set to emptyAddr.
			for(s32 regionLinkIndexToClear = 0; regionLinkIndexToClear <(MAXLINKSPREREGION); regionLinkIndexToClear++)
			{
				apRegions[Region]->aLinks[regionLinkIndexToClear].m_OtherNode.SetEmpty();
			}

			apRegions[Region]->NumNodes = 0;
			apRegions[Region]->NumNodesCarNodes = 0;
			apRegions[Region]->NumNodesPedNodes = 0;
			apRegions[Region]->NumLinks = 0;
		}
	}
//	CFileMgr::SetDir("");
}
*/

// JAY, robbed from here:-
//        name: CFileLoader::LoadScene
// description: Load Object instance information from IPL file
//          in: pLevelName = pointer to level name string e.g "INDUSTRIAL"
bool CPathFindBuild::ReadPathInfoFromIPL(const char* pFileName)
{
#define FILELOADER_COMMENT_CHAR '#'

	enum IPLLoadingStatus {
		LOADING_NOTHING,
		LOADING_IGNORE,
		LOADING_PATHNODES,
		LOADING_PATHLINKS,
	};

	char binaryIplName[RAGE_MAX_PATH];
	strcpy(binaryIplName, pFileName);
	char* pDotIndex;
	pDotIndex = strrchr(binaryIplName,'.');
	*pDotIndex = '\0';

	if(ASSET.Exists(binaryIplName, PLACEMENTS_FILE_EXT))
		return false;

	char* pLine;

	IPLLoadingStatus status = LOADING_NOTHING;

	FileHandle fid;
	fid = CFileMgr::OpenFile(pFileName, "rb");
	Assertf( CFileMgr::IsValidFileHandle(fid), "Can not open '%s'", pFileName);
	if(!CFileMgr::IsValidFileHandle(fid))
		return false;

	// first, strip out all commas and escape sequence chars, and replace them with spaces
	while ((pLine = CFileMgr::ReadLine(fid)) != NULL)
	{
		// if beginning of line is the end it is empty
		if(*pLine == '\0')
			continue;

		// ignore lines starting with # they are comments
		if(*pLine == FILELOADER_COMMENT_CHAR)
			continue;

		if(status == LOADING_NOTHING)
		{
			// 'vnod' tag indicates start of path nodes
			if(*pLine == 'v' && *(pLine+1) == 'n' && *(pLine+2) == 'o' && *(pLine+3) =='d')
			{
				status = LOADING_PATHNODES;
				PathNodeIndex = 0;
			}
			// 'link' tag indicates start of path nodes
			else if(*pLine == 'l' && *(pLine+1) == 'i' && *(pLine+2) == 'n' && *(pLine+3) =='k')
				status = LOADING_PATHLINKS;
		}
		else
		{
			// 'end' tag indicates end of current section
			if(*pLine == 'e' && *(pLine+1) == 'n' && *(pLine+2) == 'd')
			{
				status = LOADING_NOTHING;
			}
			else
			{
				switch(status)
				{
				case LOADING_PATHNODES:
					{
						float	x, y, z;
						u32	SwitchedOff, WaterNode, LowBridge, Speed, SpecialFunction, StreetNameHash = 0, Highway = 0, NoGps = 0, AdditionalTunnelFlag = 0, OpenSpaceBoundary = 0, LeftOnly = 0, NoLeftTurns = 0, NoRightTurns = 0, Offroad = 0, NoBigVehicles = 0, IndicateKeepLeft = 0, IndicateKeepRight = 0, IsSlipNode = 0;
						float	Density = 1.0f;


						ASSERT_ONLY(s32 numArgs =) sscanf(pLine, "%f %f %f %d %d %d %d %d %f %d %d %d %d %d %d %d %d %d %d %d %d %d",
							&x, &y, &z, &SwitchedOff, &WaterNode, &LowBridge, &Speed, &SpecialFunction, &Density, &StreetNameHash
							, &Highway, &NoGps, &AdditionalTunnelFlag, &OpenSpaceBoundary, &NoLeftTurns, &LeftOnly, &Offroad
							, &NoRightTurns, &NoBigVehicles, &IndicateKeepLeft, &IndicateKeepRight, &IsSlipNode);
						Assert(numArgs >= 8);
				
						//void	StoreVehicleNode(const char *pFileName, s32 NodeIndex, float X, float Y, float Z,
						//	bool bSwitchedOff, bool bLowBridge, bool bWaterNode,
						//	u32 SpecialFunction, s32 Speed, float Density, u32 StreetNameHash,
						//	bool bHighway, bool bNoGps, bool bAdditionalTunnelFlag, bool bOpenSpace, bool usesHighResCoors);
						StoreVehicleNode(pFileName, PathNodeIndex, x, y, z, SwitchedOff != 0, LowBridge != 0, WaterNode != 0, SpecialFunction, 
							Speed, Density, StreetNameHash, Highway != 0, NoGps != 0, AdditionalTunnelFlag != 0, OpenSpaceBoundary != 0, false, 
							LeftOnly != 0, NoLeftTurns != 0, NoRightTurns != 0, Offroad != 0, NoBigVehicles != 0, 
							IndicateKeepLeft != 0, IndicateKeepRight != 0, IsSlipNode != 0);
						PathNodeIndex++;
					}
					break;
				case LOADING_PATHLINKS:
					{
						#define FLAG_VEHLINK_NARROW         (1<<0)
						#define FLAG_VEHLINK_GPSBOTHWAYS    (1<<1)
						#define FLAG_VEHLINK_BLOCKIFNOLANES (1<<2)
						#define FLAG_VEHLINK_SHORTCUT		(1<<3)
						#define FLAG_VEHLINK_IGNOREFORNAV	(1<<4)

						u32	Node1, Node2, Lanes1To2, Lanes2To1;
						float	Width;
						s32	flags = 0;

						ASSERT_ONLY(s32 numArgs =) sscanf(pLine, "%d %d %f %d %d %d",
							&Node1, &Node2, &Width, &Lanes1To2, &Lanes2To1, &flags);
						Assert(numArgs >= 6);

						// void	StoreVehicleLink(const char *pFileName, s32 Node1, s32 Node2,
						//	s32 Lanes1To2, s32 Lanes2To1, float Width, bool bNarrowRoad, bool bGpsCanGoBothWays);
						const bool bNarrow = (flags&FLAG_VEHLINK_NARROW) != 0;
						const bool bGpsBothWays = (flags&FLAG_VEHLINK_GPSBOTHWAYS) != 0;
						const bool bShortcut = (flags&FLAG_VEHLINK_SHORTCUT) != 0;
						const bool bIgnoreForNav = (flags&FLAG_VEHLINK_IGNOREFORNAV) != 0;
						const bool bBlockIfNoLanes = (flags&FLAG_VEHLINK_BLOCKIFNOLANES) != 0;

						StoreVehicleLink( pFileName, Node1, Node2, Lanes1To2, Lanes2To1, Width,
											bNarrow, bGpsBothWays, bShortcut, bIgnoreForNav, bBlockIfNoLanes);
					}
					break;

				default:
					break;
				}
			}

			// If status is still IGNORE then found a line so jump out of reading
			// this file
			if(status == LOADING_IGNORE)
			{
				break;
			}
		}
	}

	CFileMgr::CloseFile(fid);
	return true;
}

/* TEST - JAY - this may need to go back in if path data is sored in IPL files that aren't paths.IPL
bool CPathFindBuild::LoadIplFiles()
{
	USE_MEMBUCKET(MEMBUCKET_WORLD);

	const CDataFileMgr::DataFile* pData = DATAFILEMGR.GetFirstFile(CDataFileMgr::IPL_FILE);
	atArray<const fiDevice*> m_iplDeviceArray;
	atArray<u32> m_filenameHashArray;

	m_iplDeviceArray.Reserve(100);
	m_filenameHashArray.Reserve(100);

	while(DATAFILEMGR.IsValid(pData))
	{
		char basename[32];
		ASSET.BaseName(basename, sizeof(basename), ASSET.FileName(pData->m_filename));
		const fiDevice* pDevice = fiDevice::GetDevice(pData->m_filename);
		u32 filenameHash = atStringHash(basename);

		// search for file in list with different device. If we find one then don't load this one
		s32 i;
		for(i=0; i<m_iplDeviceArray.GetCount(); i++)
		{
			if(m_iplDeviceArray[i] != pDevice &&
				m_filenameHashArray[i] == filenameHash)
				break;
		}

		// If a version of this file hasn't been loaded already then load it
		if(i == m_iplDeviceArray.GetCount())
		{
			ReadPathInfoFromIPL(pData->m_filename); // DW - IPLs are loaded here
		}
		else 
		{
			Displayf("Ignoring %s as we have loaded a version of this already", pData->m_filename);
		}

		// add to lists
		m_iplDeviceArray.PushAndGrow(pDevice);
		m_filenameHashArray.PushAndGrow(filenameHash);

		pData = DATAFILEMGR.GetNextFile(pData);
	}

	return true;
}
*/



