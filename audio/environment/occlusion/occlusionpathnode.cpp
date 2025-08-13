// 
// audio/environment/occlusion/occlusionpathnode.cpp
// 
// Copyright (C) 1999-2006 Rockstar Games.  All Rights Reserved.
// 

#include "audio/environment/environmentgroup.h"
#include "audio/environment/occlusion/occlusionmanager.h"
#include "audio/environment/occlusion/occlusionpathnode.h"
#include "audio/environment/occlusion/occlusionportalinfo.h"
#include "audio/northaudioengine.h"

#include "modelinfo/MloModelInfo.h"
#include "scene/portals/InteriorInst.h"

#if __BANK
#include "peds/PlayerInfo.h"
#include "peds/Ped.h"
#include "scene/portals/PortalInst.h"
#endif

AUDIO_ENVIRONMENT_OPTIMISATIONS()

FW_INSTANTIATE_CLASS_POOL(naOcclusionPathNode, CONFIGURED_FROM_FILE, atHashString("OcclusionPathNode",0x4821c9c1));	

audCurve naOcclusionPathNode::sm_DistanceToAngleFactorDamping;

f32 naOcclusionPathNode::sm_AngleFactorToWeightExponent = 5.0f;	// Weights paths with better line of sight to be exponentially greater 
f32 naOcclusionPathNode::sm_BlockedFactorToWeightExponent = 5.0f;	// Weights paths with less blockage (no doors, etc.) to be exponentially greater

#if __BANK
s32 g_ChildDebugNodeIndex[naOcclusionManager::sm_MaxOcclusionPathDepth];

extern bool g_DebugAllEnvironmentGroups;
extern bool g_DebugSelectedEnvironmentGroup;
extern bool g_DrawPortalOcclusionPaths;
extern bool g_DrawPortalOcclusionPathDistances;
extern bool g_DrawPortalOcclusionPathInfo;
extern bool g_DrawChildWeights;
#endif

naOcclusionPathNode::naOcclusionPathNode()
{
}

naOcclusionPathNode::~naOcclusionPathNode()
{
}

void naOcclusionPathNode::InitClass()
{
	sm_DistanceToAngleFactorDamping.Init(ATSTRINGHASH("DISTANCE_TO_ANGLE_FACTOR_DAMPING", 0x0379da97c));

#if __BANK
	for(u8 i = 0; i < naOcclusionManager::sm_MaxOcclusionPathDepth; i++)
	{
		g_ChildDebugNodeIndex[i] = -1;
	}
#endif
}

naOcclusionPathNode* naOcclusionPathNode::Allocate()
{
	if(naVerifyf(naOcclusionPathNode::GetPool()->GetNoOfFreeSpaces() > 0, "naOcclusionPathNode pool is full, increase size"))
	{
		return rage_new naOcclusionPathNode();
	}
	return NULL;
}

void naOcclusionPathNode::ProcessOcclusionPathTree(naOcclusionPathNodeData* currentDataNode, naEnvironmentGroup* environmentGroup, Vector3& simulatedPlayPos)
{
	BuildOcclusionDataTree(currentDataNode);

	ComputePathSegmentEnds(currentDataNode, environmentGroup);

	ComputePathSegmentStarts(currentDataNode);

	ComputeOcclusionFromPathTree(currentDataNode, environmentGroup);

	ComputeSimulatedPlayPos(currentDataNode, currentDataNode->pathSegmentStart, simulatedPlayPos);
}

void naOcclusionPathNode::BuildOcclusionDataTree(naOcclusionPathNodeData* currentDataNode)
{
	s32 numChildren = m_Children.GetCount();
	naAssertf(numChildren < 64, "naOcclusionPathNode::ProcessOcclusionPathTree Processing room with %d portals, Nick W investigate", numChildren);

	// If we don't already have enough children allocated from the last path tree we processed, determine the amount we'll need to grow.
	u32 numChildrenAboveCurrentCapacity = 0;
	if(numChildren > currentDataNode->children.GetCapacity())
	{
		numChildrenAboveCurrentCapacity = numChildren - currentDataNode->children.GetCapacity();
	}

	// We don't want to reallocate any memory, but we want to start at the beginning of the array so just reset the Count
	currentDataNode->children.ResetCount();

	for(s32 childIdx = 0; childIdx < numChildren; childIdx++)
	{
		// Get the corresponding naOcclusionPathNodeData child for the naOcclusionPathNode child.
		// We know how many new instances we need, so pass that into grow, so the next time through the loop we're not allocating any memory or allocating more than we need.
		naOcclusionPathNodeData* childData = &currentDataNode->children.Grow(numChildrenAboveCurrentCapacity);

		// Because we don't always allocate memory we might have old data, and also the naOcclusionPathNodeData constructor is empty, 
		// so whether it's a new allocation or not we still need to reset the data.
		childData->ResetData();

		naOcclusionPathNodeChild* child = GetChild(childIdx);
		if(child && child->portalInfo)
		{
			childData->parent = currentDataNode;

			// Since we're sharing path trees, for each node we have all possible routes from A to B, which means we need to check to make sure we're not doubling back through the same room
			bool hasBeenInRoom = false;
			naOcclusionPathNodeData* pathNodeDataIterator = currentDataNode;
			u32 childDestinationHash = child->portalInfo->GetDestInteriorHash();
			s32 childDestinationRoomIdx = child->portalInfo->GetDestRoomIdx();
			while(pathNodeDataIterator)
			{
				if((pathNodeDataIterator->interiorHash == childDestinationHash && pathNodeDataIterator->roomIdx == childDestinationRoomIdx) ||
					(pathNodeDataIterator->roomIdx == INTLOC_ROOMINDEX_LIMBO && childDestinationRoomIdx == INTLOC_ROOMINDEX_LIMBO))
				{
					hasBeenInRoom = true;
					break;
				}
				pathNodeDataIterator = pathNodeDataIterator->parent;
			}

			childData->portalCorners = child->portalInfo->GetPortalCorners();

			if(!hasBeenInRoom && childData->portalCorners)
			{
				childData->interiorHash = child->portalInfo->GetDestInteriorHash();
				childData->roomIdx = child->portalInfo->GetDestRoomIdx();

				// If this doesn't reach the source, keep going, otherwise mark that we have a valid path
				if(child->pathNode)
				{
					child->pathNode->BuildOcclusionDataTree(childData);
				}
				else
				{
					childData->usePath = true;
				}
			}
		}
	}

	// Filter out paths that do not reach the source
	for(s32 childIdx = 0; childIdx < numChildren; childIdx++)
	{
		if(currentDataNode->children[childIdx].usePath)
		{
			currentDataNode->usePath = true;
			break;
		}
	}
}

void naOcclusionPathNode::ComputePathSegmentEnds(naOcclusionPathNodeData* currentDataNode, naEnvironmentGroup* environmentGroup)
{
	s32 numChildren = m_Children.GetCount();
	for(s32 childIdx = 0; childIdx < numChildren; childIdx++)
	{
		naOcclusionPathNodeChild* child = GetChild(childIdx);
		naOcclusionPathNodeData* childData = &currentDataNode->children[childIdx];
		if(child && childData && childData->usePath)
		{
			naAssert(child->portalInfo);
			naAssert(childData->portalCorners);

			// Set this to where the last node ended for now, and we'll adjust those when computing the path segment starts
			childData->pathSegmentStart = currentDataNode->pathSegmentEnd;

			// Get where we intersect this portal trying to get to the source
			GetPortalIntersectionBetweenPositions(childData->pathSegmentStart, environmentGroup->GetPosition(), childData->pathSegmentEnd, childData->portalCorners);
				
			if(child->pathNode)
			{
				child->pathNode->ComputePathSegmentEnds(childData, environmentGroup);
			}
		}
	}
}

void naOcclusionPathNode::ComputePathSegmentStarts(naOcclusionPathNodeData* currentDataNode)
{
	s32 numChildren = m_Children.GetCount();
	for(s32 childIdx = 0; childIdx < numChildren; childIdx++)
	{
		naOcclusionPathNodeChild* child = GetChild(childIdx);
		naOcclusionPathNodeData* childData = &currentDataNode->children[childIdx];
		if(child && childData && childData->usePath)
		{
			naAssertf(child->portalInfo, "We should have already verified this when building the data tree");
			naAssertf(childData->portalCorners, "We should have set these when building the data tree");

			if(currentDataNode->parent)
			{
				// Recompute the child's pathSegmentStart using the pathSegmentEnd instead of the occlusion group
				GetPortalIntersectionBetweenPositions(currentDataNode->pathSegmentStart, childData->pathSegmentEnd, childData->pathSegmentStart, currentDataNode->portalCorners);
			}

			if(child->pathNode)
			{
				child->pathNode->ComputePathSegmentStarts(childData);
			}
		}
	}
}

#if __BANK
void naOcclusionPathNode::ComputeOcclusionFromPathTree(naOcclusionPathNodeData* currentDataNode, naEnvironmentGroup* environmentGroup, f32 currentPathDistance, u32 pathDepth)
#else
void naOcclusionPathNode::ComputeOcclusionFromPathTree(naOcclusionPathNodeData* currentDataNode, naEnvironmentGroup* environmentGroup, f32 currentPathDistance)
#endif
{

#if __BANK
	pathDepth++;
#endif

	s32 numChildren = m_Children.GetCount();
	for(s32 childIdx = 0; childIdx < numChildren; childIdx++)
	{
		naOcclusionPathNodeChild* child = GetChild(childIdx);
		naOcclusionPathNodeData* childData = &currentDataNode->children[childIdx];
		if(child && childData && childData->usePath)
		{
			naAssertf(child->portalInfo, "We should have already verified this when building the data tree");
			naAssertf(childData->portalCorners, "We should have set these when building the data tree");

#if __BANK
			if(g_DebugAllEnvironmentGroups || (environmentGroup->GetIsDebugEnvironmentGroup() && g_DebugSelectedEnvironmentGroup))
			{
				if(currentDataNode->drawDebug == true && (g_ChildDebugNodeIndex[pathDepth-1] == childIdx || g_ChildDebugNodeIndex[pathDepth-1] == -1))
				{
					childData->drawDebug = true;
				}
			}
#endif

			childData->blockageFactor = GetBlockage(child->portalInfo, childData->pathSegmentStart, environmentGroup, currentPathDistance);

			Vector3 childNodeStartToChildNodeEnd;
			f32 childNodeStartToChildNodeEndDistance;

			// Need to put in a check here to protect against the off chance that the listener position is at the intersection
			if(childData->pathSegmentStart != childData->pathSegmentEnd)
			{
				childNodeStartToChildNodeEnd = childData->pathSegmentEnd - childData->pathSegmentStart;
				childNodeStartToChildNodeEndDistance = childNodeStartToChildNodeEnd.Mag();
				childData->normalizedPathSegment = childNodeStartToChildNodeEnd;
				childData->normalizedPathSegment.InvScale(childNodeStartToChildNodeEndDistance);
			}
			else
			{
				childNodeStartToChildNodeEnd = environmentGroup->GetPosition() - childData->pathSegmentStart;
				childNodeStartToChildNodeEndDistance = 1.0f;
				childData->normalizedPathSegment = childNodeStartToChildNodeEnd;
				childData->normalizedPathSegment.NormalizeSafe();
			}

			childData->pathDistance = childNodeStartToChildNodeEndDistance;
			childData->pathSegmentDistance = childNodeStartToChildNodeEndDistance;

			if(currentDataNode->parent)
			{
				Vector3 currentNodeStartToChildNodeStartNormalized;
				f32 currentNodeStartToChildNodeStartDistance;

				if(currentDataNode->pathSegmentStart != childData->pathSegmentStart)
				{
					Vector3 currentNodeStartToChildNodeStart = childData->pathSegmentStart - currentDataNode->pathSegmentStart;
					currentNodeStartToChildNodeStartDistance = currentNodeStartToChildNodeStart.Mag();
					currentNodeStartToChildNodeStartNormalized = currentNodeStartToChildNodeStart;
					currentNodeStartToChildNodeStartNormalized.InvScale(currentNodeStartToChildNodeStartDistance);
				}
				else
				{
					Vector3 currentNodeStartToChildNodeStart = environmentGroup->GetPosition() - currentDataNode->pathSegmentStart;
					currentNodeStartToChildNodeStartDistance = 1.0f;
					currentNodeStartToChildNodeStartNormalized = currentNodeStartToChildNodeStart;
					currentNodeStartToChildNodeStartNormalized.NormalizeSafe();
				}

				childData->angleFactor = GetAngleFactor(currentNodeStartToChildNodeStartNormalized, childData->normalizedPathSegment);
				f32 distanceToUseForDamping = currentNodeStartToChildNodeStartDistance;
				f32 currentNodePortalDamping = sm_DistanceToAngleFactorDamping.CalculateValue(distanceToUseForDamping);
				childData->angleFactor *= currentNodePortalDamping;

#if __BANK
				if(childData->drawDebug)
				{
					DrawPortalPathInfo(childData->pathSegmentStart, childData->angleFactor, currentNodePortalDamping, currentDataNode->blockageFactor, childIdx);
				}
#endif
			}

#if __BANK
			if(childData->drawDebug)
			{		
				if(currentDataNode->parent)
				{
					DrawPortalPathLine(childData->pathSegmentStart, childData->pathSegmentEnd);
				}
				else
				{
					CPed* playerPed = FindPlayerPed();
					if(playerPed)
					{
						Vec3V playerPos = playerPed->GetTransform().GetPosition();
						Vector3 drawPos = VEC3V_TO_VECTOR3(playerPos) + Vector3(0.0f, 0.0f, -0.25f);
						DrawPortalPathLine(drawPos, childData->pathSegmentEnd);
					}
				}

				childData->debugDrawPos = childData->pathSegmentEnd;
			}
#endif
			

			// Does this reach the source ( No more portals to go through after this one )
			// If it does, than process all the data associated with the segment from the portal to the occlusion group
			if(!child->pathNode)
			{
				// It reaches the source, so process the distance and angle between the start-intersection and intersection-source vectors
				Vector3 childSegmentEndToSource = environmentGroup->GetPosition() - childData->pathSegmentEnd;
				f32 childSegmentEndToSourceDistance = childSegmentEndToSource.Mag();
				childData->pathDistance += childSegmentEndToSourceDistance;

				f32 angleFactor = 0.0f;
				if(childSegmentEndToSourceDistance != 0.0f)
				{
					Vector3 childSegmentEndToSourceNormalized = childSegmentEndToSource;
					childSegmentEndToSourceNormalized.InvScale(childSegmentEndToSourceDistance);
					angleFactor = GetAngleFactor(childData->normalizedPathSegment, childSegmentEndToSourceNormalized);
				}
				
				// See what's closer, the last portal intersection to this portal intersection, or the portal intersection to the source
				// and dampen by the lesser amount, so if the source is right around the corner or we have 2 portals really close together we dampen the angle factor
				f32 dampingDistanceToUse = Min(childData->pathSegmentDistance, childSegmentEndToSourceDistance);
				f32 angleFactorDamping = sm_DistanceToAngleFactorDamping.CalculateValue(dampingDistanceToUse);
				angleFactor *= angleFactorDamping;

				// We may already have an angleFactor value, so do a linear interpolation to combine them.
				childData->angleFactor = Lerp(angleFactor, childData->angleFactor, 1.0f); 

#if __BANK
				if(childData->drawDebug)
				{
					DrawPortalPathLine(childData->pathSegmentEnd, environmentGroup->GetPosition());
					DrawPortalPathInfo(childData->pathSegmentEnd, childData->angleFactor, angleFactorDamping, childData->blockageFactor, childIdx);
				}
#endif
			}
			// If not, then keep going down this path.  
			else
			{
				f32 currentChildPathDistance = currentPathDistance + childData->pathDistance;
#if __BANK
				child->pathNode->ComputeOcclusionFromPathTree(childData, environmentGroup, currentChildPathDistance, pathDepth);
#else
				child->pathNode->ComputeOcclusionFromPathTree(childData, environmentGroup, currentChildPathDistance);
#endif
			}
		}
	}

	// Calculate the weights and mix down all the child data angleFactor, blockageFactor, and distance
	InterpolateChildData(currentDataNode);
}

void naOcclusionPathNode::InterpolateChildData(naOcclusionPathNodeData* currentDataNode)
{
	if(naVerifyf(currentDataNode, "naOcclusionPathNode::InterpolateChildData Passed NULL naOcclusionPathNodeData*"))
	{
		// First figure out the weights of all the children
		ComputeChildWeights(currentDataNode);

		f32 interpolatedDistance = 0.0f;
		f32 interpolatedAngleFactor = 0.0f;
		f32 interpolatedBlockageFactor = 0.0f;
		Vector3 interpolatedPathSegmentEnd;
		interpolatedPathSegmentEnd.Zero();

		// Get the interpolated child data by factoring in their weights and values.
		s32 numChildren = currentDataNode->children.GetCount();
		for(s32 i = 0; i < numChildren; i++)
		{
			naOcclusionPathNodeData* childData = &currentDataNode->children[i];
			if(childData && childData->usePath)
			{
				interpolatedDistance += childData->pathDistance * childData->weight;
				interpolatedAngleFactor += childData->angleFactor * childData->weight;
				interpolatedBlockageFactor += childData->blockageFactor * childData->weight;
				Vector3 scaledChildPathSegmentStart = childData->pathSegmentStart;
				scaledChildPathSegmentStart.Scale(childData->weight);
				interpolatedPathSegmentEnd += scaledChildPathSegmentStart;
			}
		}

		// Combine the current node's data with the interpolated child data,
		// so by the time we reach the root node we'll have added all the path data together.
		currentDataNode->pathDistance += interpolatedDistance;
		currentDataNode->angleFactor = Lerp(currentDataNode->angleFactor, interpolatedAngleFactor, 1.0f);
		currentDataNode->blockageFactor = Lerp(currentDataNode->blockageFactor, interpolatedBlockageFactor, 1.0f);
		currentDataNode->pathSegmentEnd = interpolatedPathSegmentEnd;
	}
}

void naOcclusionPathNode::ComputeChildWeights(naOcclusionPathNodeData* currentNodeData)
{
	if(naVerifyf(currentNodeData, "naOcclusionPathNode::ComputeChildWeights Passed NULL naOcclusionPathNodeData*"))
	{
		f32 totalWeight = 0.0f;
		s32 numChildren = currentNodeData->children.GetCount();
		
		naOcclusionPathNodeData* childData = NULL;

		// First determine the distance, angle, and blocked weights separately
		for(s32 i = 0; i < numChildren; i++)
		{
			childData = &currentNodeData->children[i];
			if(childData && childData->usePath)
			{
				// Be safe and clamp the distance so we're not dividing by 0, or if something horrible happens and we end up with a negative distance
				childData->pathDistance = Max(SMALL_FLOAT, childData->pathDistance);

				// Weight the distance linearly.  This works out well because sound pressure is inversely proportional to distance.
				f32 distanceFactorWeight = 1.0f / childData->pathDistance;

				// Skew portals more heavily that are closer to having line of sight than others.
				f32 angleFactorWeight = Powf((1.0f - childData->angleFactor), sm_AngleFactorToWeightExponent);

				// Also skew portals with less blockage more
				f32 blockageFactorWeight = Powf((1.0f - childData->blockageFactor), sm_BlockedFactorToWeightExponent);

				// Get a final weight value from our 3 metrics
				// If we're fully occluded, set the weight as low as we can go, the reason being we might have a volume or occlusion cap on 
				// our environmentGroup, so we still want to find a place to leak the sound from.
				childData->weight = angleFactorWeight * blockageFactorWeight * distanceFactorWeight;
				childData->weight = Max(VERY_SMALL_FLOAT, childData->weight);

				totalWeight += childData->weight;

#if __BANK
				if(g_DrawChildWeights && childData->drawDebug)
				{
					u32 line = 0;
					char buf[255];
					formatf(buf, sizeof(buf), "Dist Source: %4.2f Dist Portal: %4.2f Dist Factor: %f", childData->pathDistance, childData->pathSegmentDistance, distanceFactorWeight);
					grcDebugDraw::Text(childData->debugDrawPos, Color_AliceBlue, 0, line, buf);
					line += 10;
					formatf(buf, sizeof(buf), "AngleFactor: %f AngleFactorWeight: %f", childData->angleFactor, angleFactorWeight);
					grcDebugDraw::Text(childData->debugDrawPos, Color_AliceBlue, 0, line, buf);
					line += 10;
					formatf(buf, sizeof(buf), "BlockageFactor: %f BlockageFactorWeight: %f", childData->blockageFactor, blockageFactorWeight);
					grcDebugDraw::Text(childData->debugDrawPos, Color_AliceBlue, 0, line, buf);
					line += 10;
				}
#endif
			}
		}

		f32 defaultWeight;
		f32 negTotalWeight;
		f32 invTotalWeight;
		if(numChildren)
		{
			defaultWeight = 1.0f / numChildren;
			negTotalWeight = -totalWeight;
			invTotalWeight = 1.0f / totalWeight;
		}
		else
		{
			defaultWeight = negTotalWeight = invTotalWeight = 0.0f;
		}

		// Determine a final weight for the child based on the total weight
		for(s32 i = 0; i < numChildren; i++)
		{
			childData = &currentNodeData->children[i];
			if(childData && childData->usePath)
			{
				childData->weight = Selectf(negTotalWeight, defaultWeight, childData->weight * invTotalWeight);

#if __BANK
				if(g_DrawChildWeights && childData->drawDebug)
				{
					char buf[255];
					formatf(buf, sizeof(buf), "Child %d: Weight: %f", i, childData->weight);
					grcDebugDraw::Text(childData->debugDrawPos, Color_AliceBlue, 0, -10, buf);
				}
#endif
			}
		}
	}
}

void naOcclusionPathNode::ComputeSimulatedPlayPos(naOcclusionPathNodeData* rootNodeData, const Vector3& playerPos, Vector3& simulatedPlayPos)
{
	if(naVerifyf(rootNodeData, "naOcclusionPathNode::ComputeSimulatedPlayPos Passed NULL naOcclusionPathNodeData*"))
	{
		// The normalized path segment is a vector pointing at the point at which the sound would have traveled through the portal to reach us.
		// So that scaled by the total distance the sound has traveled would be the simulated play position for each portal
		// Get that vector, and then weight it thereby combining all the different portals to a final play position
		simulatedPlayPos.Zero();
		s32 numChildren = rootNodeData->children.GetCount();
		for(s32 i = 0; i < numChildren; i++)
		{
			naOcclusionPathNodeData* childData = &rootNodeData->children[i];
			if(childData && childData->usePath)
			{
				if(IsFiniteAll(VECTOR3_TO_VEC3V(childData->pathSegmentEnd)) && IsFiniteAll(VECTOR3_TO_VEC3V(playerPos)))
				{
					Vector3 childPathSegmentNormalized = childData->pathSegmentEnd - playerPos;
					childPathSegmentNormalized.NormalizeSafe();
					Vector3 soundOffsetFromListenerPos = childPathSegmentNormalized;
					soundOffsetFromListenerPos.Scale(childData->pathDistance * childData->weight);

					simulatedPlayPos += soundOffsetFromListenerPos;
				}
			}
		}

		simulatedPlayPos += playerPos;	// Converts the offset to a world position
	}
}

f32 naOcclusionPathNode::GetAngleFactor(const Vector3& pathSegment1, const Vector3& pathSegment2)
{
	f32 angle = pathSegment1.Dot(pathSegment2);

	angle = Clamp(angle, -1.0f, 1.0f);	// We'll sometimes end up with values just outside of this so clamp it.
	f32 scaledAngle = (angle + 1.0f) * 0.5f;	// Convert the value from -1 to 1, to 0 to 1.

	// We want to eventually use this to get an occlusion value, so we want a value of 0 to not be occlusive, and 1 to be the most occlusive.
	// So reverse the scaled angle since 0 right now is doubling back on itself, and 1 is a straight line
	f32 angleFactor = 1.0f - scaledAngle;

	return angleFactor;	
}

// We want to know where the vStartPos-vFinishPos segment passes through the portal, or where it would pass through if there isn't a direct line.
// So we'll either get the portal position by drawing a ray from the start position to the occlusion group to get the planar intersection, and then clamp to the portal,
// or finding the 2 closest portal corners to the start position, and then clamping to the occlusion group position along that segment.
void naOcclusionPathNode::GetPortalIntersectionBetweenPositions(const Vector3& startPos, const Vector3& finishPos, 
																		Vector3& portalIntersection, const fwPortalCorners* portalCorners)
{
	// Get the plane defined by the portal.
	Vector4 portalPlane;
	portalPlane.ComputePlane(portalCorners->GetCorner(0), portalCorners->GetCorner(1), portalCorners->GetCorner(2));

	// Check if the start position and occlusion group are on the same side of the portal by checking if the distances +- sign.
	// We can't guarantee that the environmentGroup is always on the backside of the portal, so we can't use backface culling on the geomSegments::CollideRayPlane call
	f32 startPosDistToPlane = portalPlane.DistanceToPlane(startPos);
	f32 finishPosDistToPlane = portalPlane.DistanceToPlane(finishPos);

	if((startPosDistToPlane < 0.0f && finishPosDistToPlane < 0.0f) || (startPosDistToPlane >= 0.0f && finishPosDistToPlane >= 0.0f))
	{
		// It's parallel or the source is on the same side of portal plane as the listener.  
		// So get the corners closest to the environmentGroup, and then clamp the environmentGroup position to those corners.
		u32 closestCornerIdx = 0;
		u32 secondClosestCornerIdx = 0;
		GetClosestPortalCornersToPosition(closestCornerIdx, secondClosestCornerIdx, finishPos, portalCorners);

		f32 minX = Min(portalCorners->GetCorner(closestCornerIdx).x, portalCorners->GetCorner(secondClosestCornerIdx).x);
		f32 maxX = Max(portalCorners->GetCorner(closestCornerIdx).x, portalCorners->GetCorner(secondClosestCornerIdx).x);
		f32 minY = Min(portalCorners->GetCorner(closestCornerIdx).y, portalCorners->GetCorner(secondClosestCornerIdx).y);
		f32 maxY = Max(portalCorners->GetCorner(closestCornerIdx).y, portalCorners->GetCorner(secondClosestCornerIdx).y);
		f32 minZ = Min(portalCorners->GetCorner(closestCornerIdx).z, portalCorners->GetCorner(secondClosestCornerIdx).z);
		f32 maxZ = Max(portalCorners->GetCorner(closestCornerIdx).z, portalCorners->GetCorner(secondClosestCornerIdx).z);

		// Now clamp to the finish position to the portal segment, which should give us the best intersection position.  We could do more interpolation but it hardly seems worth it.
		portalIntersection.x = Clamp(finishPos.x, minX, maxX);
		portalIntersection.y = Clamp(finishPos.y, minY, maxY);
		portalIntersection.z = Clamp(finishPos.z, minZ, maxZ);
	}
	else
	{
		// The current position and the occlusion group are on opposite sides of the plane, so we can get the planar intersection
		Vector3 startToFinish = finishPos - startPos;
		f32 tValue;

		// Get the planar intersection
		geomSegments::CollideRayPlane(startPos, startToFinish, portalPlane, &tValue);

		// Get the intersection, start at the start position and go up the tValue along the ray from the Start->Finish vector.
		Vector3 intersectionT = startToFinish;	
		intersectionT.Scale(tValue);
		portalIntersection = startPos + intersectionT;

		// Then clamp within the portal bounds since the sound would need to pass through the portal to reach you.
		f32 minX = Min(portalCorners->GetCorner(0).x, portalCorners->GetCorner(1).x, portalCorners->GetCorner(2).x, portalCorners->GetCorner(3).x);
		f32 maxX = Max(portalCorners->GetCorner(0).x, portalCorners->GetCorner(1).x, portalCorners->GetCorner(2).x, portalCorners->GetCorner(3).x);
		f32 minY = Min(portalCorners->GetCorner(0).y, portalCorners->GetCorner(1).y, portalCorners->GetCorner(2).y, portalCorners->GetCorner(3).y);
		f32 maxY = Max(portalCorners->GetCorner(0).y, portalCorners->GetCorner(1).y, portalCorners->GetCorner(2).y, portalCorners->GetCorner(3).y);
		f32 minZ = Min(portalCorners->GetCorner(0).z, portalCorners->GetCorner(1).z, portalCorners->GetCorner(2).z, portalCorners->GetCorner(3).z);
		f32 maxZ = Max(portalCorners->GetCorner(0).z, portalCorners->GetCorner(1).z, portalCorners->GetCorner(2).z, portalCorners->GetCorner(3).z);

		portalIntersection.x = Clamp(portalIntersection.x, minX, maxX);
		portalIntersection.y = Clamp(portalIntersection.y, minY, maxY);
		portalIntersection.z = Clamp(portalIntersection.z, minZ, maxZ);
	}
}

void naOcclusionPathNode::GetClosestPortalCornersToPosition(u32& closestCornerIdx, u32& secondClosestCornerIdx, const Vector3& position, const fwPortalCorners* portalCorners)
{
	if(naVerifyf(portalCorners, "naOcclusionPathNode::GetClosestPortalCornersToPosition Passed NULL fwPortalCorners* when trying to determine closest corners to a position"))
	{
		f32 closestCornerDist2 = LARGE_FLOAT;
		f32 secondClosestCornerDist2 = LARGE_FLOAT;

		for(u32 i = 0; i < 4; i++)
		{
			f32 dist2 = (portalCorners->GetCorner(i) - position).Mag2();
			if(dist2 < closestCornerDist2)
			{
				// Move the closest to the second closest
				secondClosestCornerIdx = closestCornerIdx;
				secondClosestCornerDist2 = closestCornerDist2;

				// Set the new values for the closest
				closestCornerIdx = i;
				closestCornerDist2 = dist2;
			}
			else if(dist2 < secondClosestCornerDist2)
			{
				// Set the new values for the second closest
				secondClosestCornerIdx = i;
				secondClosestCornerDist2 = dist2;
			}
		}
	}
}

f32 naOcclusionPathNode::GetBlockage(naOcclusionPortalInfo* portalInfo, const Vector3& vPosition, naEnvironmentGroup* environmentGroup, f32 currentPathDistance)
{
	// Start at 1.0f, so if our interior isn't streamed in yet, we don't let sound leak through it
	f32 blockage = 1.0f;

	if(portalInfo && environmentGroup)
	{
		const CInteriorInst* intInst = portalInfo->GetInteriorInst();
		if(intInst)
		{
			// Only use the global blockage for portals (NonMarkedPortalOcclusion) if we're Inside going Outside, or Outside going Inside
			bool useInteriorSettingsMetrics = false;

			// Only use exterior blockage if we're inside looking outside, so we can control how much outside ambience is going on inside
			f32 exteriorBlockage = 0.0f;

			// Figure out which room and InteriorSettings to use
			const InteriorSettings* interiorSettings = NULL;
			const InteriorRoom* interiorRoom = NULL;

			// Factor in the openness of any doors, is there a door and what have we set on the portal as a base value.
			CInteriorInst* destIntInst;
			s32 destRoomIdx;
			audNorthAudioEngine::GetOcclusionManager()->GetPortalDestinationInfo(intInst, portalInfo->GetRoomIdx(), portalInfo->GetPortalIdx(), &destIntInst, &destRoomIdx);

			// Do we have valid data?  Also make sure that we're not going through a link portal by comparing our current and dest CInteriorInst
			if(destIntInst && destRoomIdx != INTLOC_INVALID_INDEX && intInst == destIntInst)
			{
				// We are Outside looking Inside
				if(portalInfo->GetRoomIdx() == INTLOC_ROOMINDEX_LIMBO)
				{
					audNorthAudioEngine::GetGtaEnvironment()->GetInteriorSettingsForInterior(destIntInst, destRoomIdx, interiorSettings, interiorRoom);
					useInteriorSettingsMetrics = true;
				}
				// We are Inside looking Outside
				else if(destRoomIdx == INTLOC_ROOMINDEX_LIMBO)
				{		
					audNorthAudioEngine::GetGtaEnvironment()->GetInteriorSettingsForInterior(intInst, portalInfo->GetRoomIdx(), interiorSettings, interiorRoom);

					// Only want to do distance based occlusion for ambients, but for guns we want to still hear them at the other side of a room
					if(environmentGroup->GetUseInteriorDistanceMetrics())
					{
						exteriorBlockage = audNorthAudioEngine::GetOcclusionManager()->CalculateExteriorBlockageForPortal(vPosition, interiorSettings, interiorRoom, 
							portalInfo->GetPortalCorners(), currentPathDistance);
					}

					useInteriorSettingsMetrics = true;
				}
			}

			// Ok to pass a NULL InteriorSettings* here.
			f32 entityBlockage = portalInfo->GetEntityBlockage(interiorSettings, interiorRoom, useInteriorSettingsMetrics);

			// Combine the entity blockage and exterior blockage
			blockage = Lerp(entityBlockage, exteriorBlockage, 1.0f); 
		}
	}

	return blockage;
}

naOcclusionPathNodeChild* naOcclusionPathNode::CreateChild(naOcclusionPathNode* pathNode, naOcclusionPortalInfo* portalInfo)
{
	naOcclusionPathNodeChild* newChild = &m_Children.Grow();
	newChild->pathNode = pathNode;
	newChild->portalInfo = portalInfo;
	
	return newChild;
}

#if __BANK
void naOcclusionPathNode::DrawPortalPathLine(const Vector3& point1, const Vector3& point2)
{
	if(g_DrawPortalOcclusionPaths)
	{
		grcDebugDraw::Line(point1, point2, Color_OrangeRed2, Color_OrangeRed2);
	}
	if(g_DrawPortalOcclusionPathDistances)
	{
		char buf[255];
		formatf(buf, sizeof(buf), "distance: %.02f", (point1 - point2).Mag());
		grcDebugDraw::Text(point2, Color_DarkSeaGreen, buf);
	}
}

void naOcclusionPathNode::DrawPortalPathInfo(const Vector3& portalPos, f32 angleFactor, f32 angleDamping, f32 portalBlockage, s32 childIdx)
{
	if(g_DrawPortalOcclusionPathInfo)
	{
		char buf[255];
		formatf(buf, sizeof(buf), "CHILD %d: AF: %.02f AFD: %.02f BLK: %.02f", childIdx, angleFactor, angleDamping, portalBlockage);
		grcDebugDraw::Text(portalPos, Color_pink, 0, 10*childIdx, buf);
	}
}

u32 naOcclusionPathNode::GetMemoryFootprint()
{
	u32 size = (u32)sizeof(naOcclusionPathNode*);
	size +=(u32)(sizeof(naOcclusionPathNodeChild) * GetNumChildren());
	return size;
}

void naOcclusionPathNode::AddWidgets(bkBank& bank)
{
	bank.AddSlider("AngleFactorToWeightExponent", &sm_AngleFactorToWeightExponent, 1.0f, 10.0f, 0.1f);
	bank.AddSlider("BlockedFactorToWeightExponent", &sm_BlockedFactorToWeightExponent, 1.0f, 10.0f, 0.1f);
	for(u32 i = 0; i < naOcclusionManager::sm_MaxOcclusionPathDepth; i++)
	{
		char buf[255];
		formatf(buf, "ChildNodeDebugIdx %u", i);
		bank.AddSlider(buf, &g_ChildDebugNodeIndex[i], -1, 64, 1);
	}
}
#endif

#if __BANK
bool naOcclusionPathNode::BuildPathTree(const CInteriorInst* startIntInst, const s32 startRoomIdx, 
										const CInteriorInst* finishIntInst, const s32 finishRoomIdx, 
										const u32 pathDepth, const bool allowOutside, const bool buildInterInteriorPaths)
{
	naAssertf(PARAM_audOcclusionBuildEnabled.Get(), "Calling occlusion build function when occlusion build is not enabled.");
	bool hasFoundFinishRoom = false;
	if(FindAndCreateChildNodes(startIntInst, startRoomIdx, finishIntInst, finishRoomIdx, pathDepth, allowOutside))
	{
		hasFoundFinishRoom = true;
	}

	// If we're going outside to inside, and we want to build inter-interior paths, 
	// then see if we can go from outside->linkedInterior->finishIntInst
	if(startRoomIdx == INTLOC_ROOMINDEX_LIMBO && buildInterInteriorPaths)
	{
		// Get an array of all interiors linked to the startIntInst, and interior linked to those interiors
		atArray<const CInteriorInst*> linkedIntInstList;
		FindAllLinkedInteriors(startIntInst, startIntInst, linkedIntInstList);

		// Go through each portal that leads from outside->linkedInterior.  Room INTLOC_ROOMINDEX_LIMBO will contain all the portals that lead outside.
		// From there we'll see if we can then go from linkedInterior->finishIntInst with the remaining path depth.
		for(s32 i = 0; i < linkedIntInstList.GetCount(); i++)
		{
			if(naVerifyf(linkedIntInstList[i], "naOcclusionPathNode::BuildPathTree NULL CInteriorInst* in the linkedIntInstList") && 
				FindAndCreateChildNodes(linkedIntInstList[i], INTLOC_ROOMINDEX_LIMBO, finishIntInst, finishRoomIdx, pathDepth, allowOutside))
			{
				hasFoundFinishRoom = true;
			}
		}
	}
			
	return hasFoundFinishRoom;
}

bool naOcclusionPathNode::FindAndCreateChildNodes(const CInteriorInst* currentIntInst, const s32 currentRoomIdx,
												const CInteriorInst* finishIntInst, const s32 finishRoomIdx, 
												const u32 pathDepth, const bool allowOutside)
{
	naAssertf(PARAM_audOcclusionBuildEnabled.Get(), "Calling occlusion build function when occlusion build is not enabled.");
	bool hasFoundFinishRoom = false;

	const u32 numPortalsInRoom = currentIntInst->GetNumPortalsInRoom(currentRoomIdx);
	for(u32 portalIdx = 0; portalIdx < numPortalsInRoom; portalIdx++)
	{
		// Get destination to where this portal leads
		CInteriorInst* destIntInst;
		s32 destRoomIdx;
		audNorthAudioEngine::GetOcclusionManager()->GetPortalDestinationInfo(currentIntInst, currentRoomIdx, portalIdx, &destIntInst, &destRoomIdx);

		if(destIntInst && destRoomIdx != INTLOC_INVALID_INDEX)
		{
			bool isDestinationInDoNotUseList = audNorthAudioEngine::GetOcclusionManager()->IsInteriorInDoNotUseList(audNorthAudioEngine::GetOcclusionManager()->GetUniqueProxyHashkey(destIntInst->GetProxy()));

			if(!isDestinationInDoNotUseList)
			{
				// Get the portal info associated with this information.
				const u32 uniqueProxyHashkey = audNorthAudioEngine::GetOcclusionManager()->GetUniqueProxyHashkey(currentIntInst->GetProxy());

				// This will go through the pool and find the portal info that matches.  This is especially good because all naOcclusionInteriorInfo's store their portalinfo's in that pool, 
				// so if we're building an inter-interior path then we'll already have that portal info with all the correct entity info loaded 
				naOcclusionPortalInfo* portalInfo = audNorthAudioEngine::GetOcclusionManager()->GetPortalInfoFromParameters(uniqueProxyHashkey, currentRoomIdx, portalIdx);

				// We may not have a portal info if we're looking to come in from a linked interior that's really far away, so we can safely ignore those
				naAssertf(portalInfo || (!portalInfo && !audNorthAudioEngine::GetOcclusionManager()->AreOcclusionPathsLoadedForInterior(currentIntInst->GetProxy())),
					"We don't have a portal info already loaded, and we should");

				if(portalInfo)
				{
					// Did we find a match?  Either we'll have the same interior and room, or if we're looking to go outside,
					// then we have a match if the roomIdx's are both INTLOC_ROOMINDEX_LIMBO, since a 0 roomIdx is outside for all interiors.
					if((destIntInst == finishIntInst && destRoomIdx == finishRoomIdx) ||
						(destRoomIdx == INTLOC_ROOMINDEX_LIMBO && finishRoomIdx == INTLOC_ROOMINDEX_LIMBO))
					{
						// This is the end of the line, so pass in a NULL naOcclusionPathNode*.  That way we know later that this path ends here.
						CreateChild(NULL, portalInfo);
						hasFoundFinishRoom = true;
					}
					// As we build up paths starting at a depth of 1, if we still have pathDepthRemaining, then we should already have the paths built for the dest->finish
					else if(pathDepth > 1)
					{
						// Only go inside->outside->inside if we specifically allow it.  If destIntInst != currentIntInst then it's a link portal
						if(destRoomIdx != INTLOC_ROOMINDEX_LIMBO || destIntInst != currentIntInst || allowOutside)
						{
							const u32 pathTreeKey = audNorthAudioEngine::GetOcclusionManager()->GenerateOcclusionPathKey(destIntInst, destRoomIdx, finishIntInst, finishRoomIdx, pathDepth-1);

							naOcclusionPathNode* pathTree = audNorthAudioEngine::GetOcclusionManager()->GetPathNodeFromKey(pathTreeKey);
							if(pathTree)
							{
								CreateChild(pathTree, portalInfo);
								hasFoundFinishRoom = true;
							}
						}
					}			
				}
			}
		}
	}

	return hasFoundFinishRoom;
}

void naOcclusionPathNode::FindAllLinkedInteriors(const CInteriorInst* startIntInst, const CInteriorInst* currentIntInst, atArray<const CInteriorInst*>& linkedIntInstList, u32 currentPathDepth)
{
	naAssertf(PARAM_audOcclusionBuildEnabled.Get(), "Calling occlusion build function when occlusion build is not enabled.");
	currentPathDepth++;

	if(currentPathDepth <= audNorthAudioEngine::GetOcclusionManager()->GetMaxOcclusionPathDepth()
		&& naVerifyf(startIntInst, "Passed NULL start CinteriorInst* in naOcclusionPathNode::FindAllLinkedInteriors") 
		&& naVerifyf(currentIntInst, "Passed NULL current CInteriorInst* in naOcclusionPathNode::FindAllLinkedInteriors"))
	{
		const u32 numPortalsInRoom = currentIntInst->GetNumPortalsInRoom(INTLOC_ROOMINDEX_LIMBO);
		for(u32 portalIdx = 0; portalIdx < numPortalsInRoom; portalIdx++)
		{
			CMloModelInfo* currentModelInfo = currentIntInst->GetMloModelInfo();
			if(naVerifyf(currentModelInfo, "naEnvironmentGroupManager::GetPortalDestinationInfo Unable to get CMloModelInfo in audio occlusion update"))
			{
				CPortalFlags portalFlags;
				u32 roomIdx1, roomIdx2;

				s32 globalPortalIdx = currentIntInst->GetPortalIdxInRoom(INTLOC_ROOMINDEX_LIMBO, portalIdx);
				currentModelInfo->GetPortalData(globalPortalIdx, roomIdx1, roomIdx2, portalFlags);

				if(!portalFlags.GetIsMirrorPortal())
				{
					// If it's a link portal, go through and get the room and CInteriorInst* info
					if(portalFlags.GetIsLinkPortal())
					{
						CPortalInst* portalInst = currentIntInst->GetMatchingPortalInst(INTLOC_ROOMINDEX_LIMBO, portalIdx);
						if(portalInst && portalInst->IsLinkPortal())
						{
							const CInteriorInst* linkedIntInst = currentIntInst == portalInst->GetPrimaryInterior() ? portalInst->GetLinkedInterior() : portalInst->GetPrimaryInterior();
							if(linkedIntInst)
							{
								// Check if it's already in the linkedIntInst list
								bool addInteriorToList = true;
								for(s32 i = 0; i < linkedIntInstList.GetCount(); i++)
								{
									if(linkedIntInst == linkedIntInstList[i] || linkedIntInst == startIntInst)
									{
										addInteriorToList = false;
										break;
									}
								}

								if(addInteriorToList)
								{
									linkedIntInstList.PushAndGrow(linkedIntInst);
									FindAllLinkedInteriors(startIntInst, linkedIntInst, linkedIntInstList, currentPathDepth);
								}
							}
						}
					}
				}
			}
		}
	}
}
#endif // __BANK


