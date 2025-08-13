#include "vehicles/vehiclepopulation.h"

#include "camera/CamInterface.h"
#include "vehicles/vehpopulation_channel.h"

#include "fwscene/world/WorldLimits.h"
#include "fwsys/timer.h"
#include "profile/timebars.h"
#include "game/PopMultiplierAreas.h"

AI_OPTIMISATIONS()
AI_VEHICLE_OPTIMISATIONS()

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : UpdateGenerationLinkData
// PURPOSE :  Makes a list of all path links where both nodes are within the max
//				distance given.  This list is then alter used to generate vehicle
//				placement positions and initial path following data.
// PARAMETERS :	popCtrlCentre - the center about which to base the links within a given
//					radius.
//				maxNodeDist - the maximum distance to collect nodes.
// RETURNS :	false if update has been skipped, true otherwise	
/////////////////////////////////////////////////////////////////////////////////
void CVehiclePopulation::GatherPotentialVehGenLinks(
	const Vector3& popCtrlCentre,
	const Vector2& sidewallFrustumOffset,
	float maxNodeDistBase,
	float maxNodeDist,
	bool bUseKeyholeShape,
	float fKeyhole_InViewInnerRadius,
	float fKeyhole_InViewOuterRadius,
	float fKeyhole_BehindInnerRadius,
	float fKeyhole_BehindOuterRadius,
	const Vector3& popCtrlDir,
	float fKeyhole_CosHalfFOV,
	u32& numGenLinks,
	CVehGenLink genLinks[MAX_GENERATION_LINKS],
	float& averageLinkHeight,
	CVehGenSphere * apOverlappingAreas,
	s32 iNumOverlappingAreas
#if __SPU
	, PopMultiplierArea *PopMultiplierAreaArray,
	s32 iNumTrafficAreas
#endif	//__SPU
	)
{
	PF_AUTO_PUSH_DETAIL("Gather Potential Veh Gen Links");

	//note the number of links we didn't even check this cycle
	BANK_ONLY(ms_iNumExcessDisparateLinks = ms_iNumActiveLinks - rage::Min(ms_iNumActiveLinks, ms_iCurrentActiveLink));

	fKeyhole_InViewInnerRadius *= fKeyhole_InViewInnerRadius;
	fKeyhole_InViewOuterRadius *= fKeyhole_InViewOuterRadius;
	fKeyhole_BehindInnerRadius *= fKeyhole_BehindInnerRadius;
	fKeyhole_BehindOuterRadius *= fKeyhole_BehindOuterRadius;

	// Reset the link count.
	u32 lNumGenLinks = 0;

	averageLinkHeight = 0.0f;
	u32 nodesConsideredForAverageLinkHeight = 0;

	// Store this for clarity and a slight optimization.
	Vector4 maxNodeDistBase2;
	maxNodeDistBase2.Set(maxNodeDistBase * maxNodeDistBase);

	Vector4 maxNodeDist2;
	maxNodeDist2.Set(maxNodeDist * maxNodeDist);

	const float	centerMaxX		= popCtrlCentre.x + maxNodeDist;
	const float	centerMinX		= popCtrlCentre.x - maxNodeDist;
	const float	centerMaxY		= popCtrlCentre.y + maxNodeDist;
	const float	centerMinY		= popCtrlCentre.y - maxNodeDist;

	const Vector3 sidewallFrustumOffsetXYZ(sidewallFrustumOffset.x, sidewallFrustumOffset.y, 0.0f);

	// Go through all valid links in all loaded regions.
	const u32 numRegions = PATHFINDREGIONS;
	for (u32 region = 0; region < numRegions; ++region)
	{
		// Make sure this region is actually loaded in.
		if (!ThePaths.apRegions[region])
		{
			continue;
		}

		CPathRegion* curRegion = ThePaths.apRegions[region];
		if (!curRegion->NumNodes)
		{
			continue;
		}

		// Determine if the region and the generation area possibly intersect.
		// If the interval distance along either axis is negative then the areas intersect
		// along that axis.
		const float regionMinX	= ThePaths.FindXCoorsForRegion(FIND_X_FROM_REGION_INDEX(region));
		const float regionMaxX	= regionMinX + PATHFINDREGIONSIZEX;
		const float regionMinY	= ThePaths.FindYCoorsForRegion(FIND_Y_FROM_REGION_INDEX(region));
		const float regionMaxY	= regionMinY + PATHFINDREGIONSIZEY;
		const float xAxisIntervalDist = (centerMinX<regionMinX)?(regionMinX-centerMaxX):(centerMinX-regionMaxX);
		const float yAxisIntervalDist = (centerMinY<regionMinY)?(regionMinY-centerMaxY):(centerMinY-regionMaxY);

		const bool outOfRange = (xAxisIntervalDist >= 0) || (yAxisIntervalDist >= 0);
		if(outOfRange)
		{
			continue;
		}

		{
			// We don't need to go through all of the node as they are sorted by y-coordinate.
			s32 startNode, endNode;
			ThePaths.FindStartAndEndNodeBasedOnYRange(region, centerMinY, centerMaxY, startNode, endNode);

			//CPathNode* pVeryFirstNode = &curRegion->aNodes[0];
			CPathNode* pFirstNode = &curRegion->aNodes[startNode];
			CPathNode* pLastNode = &curRegion->aNodes[endNode];

			for (CPathNode* pNode = pFirstNode; pNode != pLastNode; ++pNode)
			{
				PrefetchDC(pNode + 1);

				if (!Verifyf(!pNode->m_address.IsEmpty(), "Warning, found empty path node!"))
					continue;

				// Make sure this node isn't too far away.
				bool nodeOutsideMaxDist = false;
				Vector3 pos;
				pNode->GetCoors(pos);
				Vector4 nodeDist2 = (pos - popCtrlCentre).XYMag2V().xyzw;
				if (nodeDist2.IsGreaterThan(maxNodeDist2))
				{
					nodeOutsideMaxDist = true;
				}

				if (nodeDist2.IsLessThanAll(maxNodeDistBase2))
				{
					// Divide by 1000 to avoid precision issues
					averageLinkHeight += pos.GetZ() / 1000.0f;
					++nodesConsideredForAverageLinkHeight;
				}

				// Optionally classify nodes wrt the population keyhole shape.
				// We might use this, in combination with a more frequent update frequency,
				// when the rangeScale is great to enable use to spawn cars at a greater
				// distance from the origin without blowing the MAX_GENERATION_LINKS limit.
				if(bUseKeyholeShape)
				{
					const bool bInOuterBand = nodeDist2.x > fKeyhole_InViewInnerRadius && nodeDist2.x < fKeyhole_InViewOuterRadius;
					bool bInSidewalls = nodeDist2.x > fKeyhole_BehindOuterRadius && nodeDist2.x < fKeyhole_InViewOuterRadius;
					const bool bInInnerBand = nodeDist2.x > fKeyhole_BehindInnerRadius && nodeDist2.x < fKeyhole_BehindOuterRadius;

					if(bInOuterBand || bInInnerBand || bInSidewalls)
					{
						Vector3 vPopCenterToNode = pos - popCtrlCentre;
						vPopCenterToNode.z = 0.0f;
						vPopCenterToNode.NormalizeFast();

						const float fDot = popCtrlDir.Dot(vPopCenterToNode);
						const bool bInFOV = (fDot > fKeyhole_CosHalfFOV);

						bInSidewalls = bInSidewalls && !bInFOV && fDot >= 0.0f;

						if(bInSidewalls)
						{
							Vector3 vOffsetCenterToNode = pos - (popCtrlCentre - sidewallFrustumOffsetXYZ);
							vOffsetCenterToNode.z = 0.0f;
							vOffsetCenterToNode.NormalizeFast();

							const float fOffsetDot = popCtrlDir.Dot(vOffsetCenterToNode);
							bInSidewalls = (fOffsetDot > fKeyhole_CosHalfFOV);
						}
					
						// Is node within the outer band?
						if(bInFOV && bInOuterBand)
						{

						}
						// Is node within the sidewalls?
						else if(bInSidewalls)
						{

						}
						// Is node within the inner band?
						else if(!bInFOV && bInInnerBand)
						{

						}
						// Otherwise node is within neither, and flag as outside view
						else
						{
							continue;
						}
					}
					else
					{
						continue;
					}
				}

				// Never create vehicles on links connected to junction nodes.
				// JB: Vehicle population used to do this, but it causes too many horrible problems
				// especially now that we have some complex light patterns with the junction editor.
				if(pNode->IsJunctionNode() && !pNode->IsFalseJunction())
				{
					continue;
				}
				// Ignore nodes which are switched off
				if(pNode->m_2.m_switchedOff)
				{
					continue;
				}
				// Certain special function nodes should not be considered for vehicle creation.
				if(pNode->HasSpecialFunction_VehicleCreation())
				{
					continue;
				}

				// Check the nodes linked off this one.
				for (u32 iLink = 0; iLink < pNode->NumLinks(); ++iLink)
				{
					const CPathNodeLink* link = &ThePaths.GetNodesLink(pNode, iLink);

#if VEHGEN_LINKS_IN_BOTH_DIRECTIONS
					if(link->m_1.m_LanesToOtherNode == 0)
					{
						continue;
					}
#endif // VEHGEN_LINKS_IN_BOTH_DIRECTIONS

					// Make sure the other nodeIndex in the link is valid.
					const CNodeAddress adjNode = link->m_OtherNode;

					if (!Verifyf(!adjNode.IsEmpty(), "Warning, found empty path node on link!"))
						continue;

					const u32 adjNodeRegion = adjNode.GetRegion();

					if (!ThePaths.apRegions[adjNodeRegion])
					{
						continue;
					}

					CPathRegion* adjRegion = ThePaths.apRegions[adjNodeRegion];

					if (!adjRegion->aNodes)
					{
						continue;
					}

					// Make sure to only consider each pair only once.
					const s32 adjNodeIndex = adjNode.GetIndex();

					const CPathNode* pNeighbourNode = &adjRegion->aNodes[adjNodeIndex];

#if !VEHGEN_LINKS_IN_BOTH_DIRECTIONS
					if(pNode >= pNeighbourNode)
					{
						continue;
					}
#endif // !VEHGEN_LINKS_IN_BOTH_DIRECTIONS

					// Never create cars inside junctions
					if(pNeighbourNode->IsJunctionNode() && !pNeighbourNode->IsFalseJunction())
					{
						continue;
					}

					// Ignore nodes which are switched off
					if(pNeighbourNode->m_2.m_switchedOff)
					{
						continue;
					}

					if (link->m_1.m_bShortCut)
					{
						continue;
					}

					////if the target node has multiple ways to go, but the junction flag
					////isn't set (checked above), don't allow spawning here
					//if (pNeighbourNode->m_2.m_slipJunction)
					//{
					//	continue;
					//}

					// Check if dead end roads are banned.
					if(!ms_allowSpawningFromDeadEndRoads && ((pNode->m_2.m_deadEndness) || (pNeighbourNode->m_2.m_deadEndness)))
					{
						continue; 
					}

					// Don't spawn on ignore nav links
					if (link->m_1.m_bDontUseForNavigation)
					{
						continue;
					}

					// We don't want to place car on a dead end or heading towards the dead end.
					// This had been disabled for Jimmy--re-enabled for V -JM
					if(ms_spawnCareAboutDeadEnds)
					{
						const bool currentDirectionDeadEnds = pNeighbourNode->m_2.m_deadEndness && link->m_1.m_LeadsToDeadEnd;
						const bool reverseDirectionDeadEnds = pNode->m_2.m_deadEndness && link->m_1.m_LeadsFromDeadEnd;

						if(currentDirectionDeadEnds && reverseDirectionDeadEnds)
						{
							continue; 
						}
					}
					// Certain special function nodes should not be considered for vehicle creation.
					if(pNeighbourNode->HasSpecialFunction_VehicleCreation() || pNeighbourNode->IsTrafficLight())
					{
						continue;
					}

					// Make sure both nodes aren't too far away.
					Vector3 adjPos;
					pNeighbourNode->GetCoors(adjPos);
					const Vector4 adjNodeDist2 = (adjPos-popCtrlCentre).XYMag2V().xyzw;
					if(nodeOutsideMaxDist && adjNodeDist2.IsGreaterThan(maxNodeDist2))
					{
						continue;
					}

					s32 iDensity = ( (((int)pNode->m_2.m_density) + ((int)pNeighbourNode->m_2.m_density)) >> 1);
#if __SPU
					// Use the CThePopMultiplierAreas to apply local multipliers to vehicle density
					if(iNumTrafficAreas != 0)
					{
						Vector3 vPos = (pos+adjPos)*0.5f;

						// Re-written CThePopMultiplierAreas::GetTrafficDensityMultiplier:
						float fMultiplier = 1.0f;
						int iNumVisited = 0;
						for(s32 i=0; i < MAX_POP_MULTIPLIER_AREAS; i++)
						{
							if(PopMultiplierAreaArray[i].m_init && PopMultiplierAreaArray[i].m_bCameraGlobalMultiplier == false)
							{
								if (PopMultiplierAreaArray[i].m_isSphere)
								{
									spdSphere sphere(RCC_VEC3V(PopMultiplierAreaArray[i].m_minWS), ScalarV(PopMultiplierAreaArray[i].GetRadius()));
									if(sphere.ContainsPoint(RCC_VEC3V(vPos)))
									{
										fMultiplier *= PopMultiplierAreaArray[i].m_trafficDensityMultiplier;
									}
								}
								else
								{
									spdAABB box(RCC_VEC3V(PopMultiplierAreaArray[i].m_minWS), RCC_VEC3V(PopMultiplierAreaArray[i].m_maxWS));
									if(box.ContainsPoint(RCC_VEC3V(vPos)))
									{
										fMultiplier *= PopMultiplierAreaArray[i].m_trafficDensityMultiplier;
									}
								}
							}	

							if( PopMultiplierAreaArray[i].m_trafficDensityMultiplier != 1.0f )
							{
								iNumVisited++;
								if(iNumVisited == iNumTrafficAreas)
									break;
							}
						}
						// End of CThePopMultiplierAreas::GetTrafficDensityMultiplier.

						iDensity = (s32) ((float)iDensity * fMultiplier);

						iDensity = Clamp(iDensity, 0, 15);
					}
#endif	// __SPU

					if(iDensity == 0)
					{
						continue;
					}

					// See if we can add this node pair and make sure to quit out when we
					// have filled up our max generation links.
					if(lNumGenLinks < MAX_GENERATION_LINKS)
					{
						Assertf(region == pNode->m_address.GetRegion(), "Bad region %d on node, expected %d", pNode->m_address.GetRegion(), region);

#if VEHGEN_CACHE_LINK_NEXT_JUNCTION_NODE
						bool bHaveNodesChanged =  genLinks[lNumGenLinks].m_nodeA.GetIndex() != pNode->m_address.GetIndex() ||
												  genLinks[lNumGenLinks].m_nodeB.GetIndex() != adjNodeIndex;
#endif

						genLinks[lNumGenLinks].m_nodeA.Set(region, pNode->m_address.GetIndex());
						genLinks[lNumGenLinks].m_nodeB.Set(adjNodeRegion, adjNodeIndex);
						genLinks[lNumGenLinks].m_iMinX = ((s16)Min(pos.x, adjPos.x)) - 1;
						genLinks[lNumGenLinks].m_iMinY = ((s16)Min(pos.y, adjPos.y)) - 1;
						genLinks[lNumGenLinks].m_iMinZ = ((s16)Min(pos.z, adjPos.z)) - 1;
						genLinks[lNumGenLinks].m_iMaxX = ((s16)Max(pos.x, adjPos.x)) + 1;
						genLinks[lNumGenLinks].m_iMaxY = ((s16)Max(pos.y, adjPos.y)) + 1;
						genLinks[lNumGenLinks].m_iMaxZ = ((s16)Max(pos.z, adjPos.z)) + 1;
						genLinks[lNumGenLinks].SetNearbyVehiclesCount(0.0f);

						genLinks[lNumGenLinks].m_iInitialDensity = iDensity;
						genLinks[lNumGenLinks].m_iDensity = iDensity;

						// TODO: Why not do a first order cull of which 'overlapping areas' intersect the population keyhole shape,
						// and pass this culled list into the job instead?
						int r;
						for(r=0; r<iNumOverlappingAreas; r++)
						{
							const s32 dX = ((genLinks[lNumGenLinks].m_iMinX + genLinks[lNumGenLinks].m_iMaxX) >> 1) - apOverlappingAreas[r].m_iPosX;
							const s32 dY = ((genLinks[lNumGenLinks].m_iMinY + genLinks[lNumGenLinks].m_iMaxY) >> 1) - apOverlappingAreas[r].m_iPosY;
							const s32 dZ = ((genLinks[lNumGenLinks].m_iMinZ + genLinks[lNumGenLinks].m_iMaxZ) >> 1) - apOverlappingAreas[r].m_iPosZ;

							if((dX*dX) + (dY*dY) + (dZ*dZ) < apOverlappingAreas[r].m_iRadius*apOverlappingAreas[r].m_iRadius)
							{
								break;
							}
						}
						genLinks[lNumGenLinks].m_bOverlappingRoads = (r != iNumOverlappingAreas);
						

#if VEHGEN_STORE_DEBUG_INFO_IN_LINKS
						const s32 regionLinkIndex = pNode->m_startIndexOfLinks + iLink;
						genLinks[lNumGenLinks].m_iLinkRegion = (u16)region;
						genLinks[lNumGenLinks].m_iLinkIndex = (u16)regionLinkIndex;
						genLinks[lNumGenLinks].m_iVehGenLink = (u16)lNumGenLinks;
						genLinks[lNumGenLinks].m_iFromIndex = 0;
						genLinks[lNumGenLinks].m_iToIndex = 0;
#endif

#if __BANK
						genLinks[lNumGenLinks].m_iFailType = Fail_NoSpawnAttemptedYet;
#endif
						Vector3 vLink = adjPos - pos;

						float fLinkLength = vLink.Mag();
						float fLinkGrade = vLink.GetZ() / vLink.XYMag();

						// I've given up asserting about link lengths, let's just clamp at 255 and be done with it.
						fLinkLength = MIN(fLinkLength, 255.0f);

						genLinks[lNumGenLinks].SetLinkLengths( fLinkLength );
						genLinks[lNumGenLinks].SetLinkGrade( fLinkGrade );
						genLinks[lNumGenLinks].SetLinkHeading( fwAngle::LimitRadianAngleFast( -Atan2f(vLink.x, vLink.y)	) );

#if VEHGEN_LINKS_IN_BOTH_DIRECTIONS
						genLinks[lNumGenLinks].m_iLinkLanes = static_cast<u8>(link->m_1.m_LanesToOtherNode);
#else
						genLinks[lNumGenLinks].m_iLinkLanes = static_cast<u8>(link->m_1.m_LanesToOtherNode) + static_cast<u8>(link->m_1.m_LanesFromOtherNode);
#endif
						genLinks[lNumGenLinks].m_bOnPlayersRoad = (pNode->m_1.m_onPlayersRoad || pNeighbourNode->m_1.m_onPlayersRoad);
						genLinks[lNumGenLinks].m_bUpToDate = false;
						genLinks[lNumGenLinks].m_iLastUpdateTime = 0;
						genLinks[lNumGenLinks].m_iCategory = CPopGenShape::GC_Off;
						genLinks[lNumGenLinks].m_fScore = 0.0f;

#if VEHGEN_CACHE_LINK_NEXT_JUNCTION_NODE
						if(bHaveNodesChanged)
						{
							CacheNextJunctionNode(genLinks[lNumGenLinks], link, pNode);
						}				
#endif

						++lNumGenLinks;
					}
					else
					{
						// Assert left here for debugging purposes
						// We shouldn't usually run out of links for general gameplay, and we now have a new method of
						// gathering nearby links for large distances (sniper scope, etc) which culls to the keyhole shape.

						// vehPopDebugf2("lNumGenLinks has reached max (%i)", MAX_GENERATION_LINKS);
					}
				}// END for each link off current node.
			}// END for each node in region.
		}// END of SPU only batching
	}// END for each region.

	if (nodesConsideredForAverageLinkHeight > 0)
	{
		averageLinkHeight = (averageLinkHeight / nodesConsideredForAverageLinkHeight) * 1000.0f;
	}

	numGenLinks = lNumGenLinks;
}

#if VEHGEN_CACHE_LINK_NEXT_JUNCTION_NODE
//this is too expensive to use - idealy we'd like to cache this data when building data offline
void CVehiclePopulation::CacheNextJunctionNode(CVehGenLink& genLink, const CPathNodeLink* nodeLink, const CPathNode* pOriginNode)
{
	genLink.m_junctionNode.SetEmpty();

	const CPathNode* pCurrentNode = ThePaths.FindNodePointerSafe(nodeLink->m_OtherNode);
	const CPathNode* pPreviousNode = pOriginNode;
	if(pCurrentNode)
	{
		u8 iNumAttempts = 0;
		static u8 s_iLinksToCheck = 20;
		while(iNumAttempts < s_iLinksToCheck && pCurrentNode != NULL)
		{
			if(pCurrentNode->IsTrafficLight())
			{
				if(iNumAttempts != 1)
				{
					genLink.m_junctionNode.Set(pCurrentNode->GetAddrRegion(), pCurrentNode->m_address.GetIndex());
				}			
				break;
			}

			const CPathNode* pThisNode = pCurrentNode;
			pCurrentNode = NULL;
			for (u32 iLink = 0; iLink < pThisNode->NumLinks(); ++iLink)
			{
				const CPathNodeLink* link = &ThePaths.GetNodesLink(pThisNode, iLink);

				if (link->m_1.m_bShortCut || link->m_1.m_bDontUseForNavigation
#if VEHGEN_LINKS_IN_BOTH_DIRECTIONS
					|| link->m_1.m_LanesToOtherNode == 0
#endif
					)
				{
					continue;
				}

				const CPathNode * pNeighbourNode = ThePaths.FindNodePointerSafe(link->m_OtherNode);
				if( !pNeighbourNode || pNeighbourNode == pPreviousNode )
				{
					continue;
				}

				//found what we want
				if( pNeighbourNode->IsTrafficLight() )
				{
					pCurrentNode = pNeighbourNode;
					break;
				}

				if(pNeighbourNode->m_2.m_switchedOff || pNeighbourNode->m_2.m_deadEndness)
				{
					continue;
				}

				if(pCurrentNode == NULL)
				{
					//accept the first good link as our next one
					pPreviousNode = pThisNode;
					pCurrentNode = pNeighbourNode;
				}		
			}

			++iNumAttempts;
		}
	}
}
#endif