/////////////////////////////////////////////////////////////////////////////////
// FILE :    racingline.cpp
// PURPOSE : 
// AUTHOR :  
// CREATED : 29/03/10
/////////////////////////////////////////////////////////////////////////////////
#include "vehicleAi/racingline.h"

// Framework headers
#include "fwmaths/Angle.h"

#include "debug/DebugScene.h"
#include "vehicleAi/pathfind.h"

AI_OPTIMISATIONS()



//---------------------------------------------------------------------------
// Racing line base class.
//---------------------------------------------------------------------------
RacingLine::RacingLine()
{
	;
}


RacingLine::~RacingLine()
{
	;
}


void RacingLine::ClearAll(void)
{
	m_roughPathPolyLine.clear();
	m_smoothedPathPolyLine.clear();
}


void RacingLine::ClearStampedData(u32 updateStamp)
{
	while(m_roughPathPolyLine.size() && m_roughPathPolyLine[0].m_second == updateStamp)
	{
		//m_roughPathPolyLine.pop_front();
		m_roughPathPolyLine.Delete(0);
	}
	while(m_smoothedPathPolyLine.size() && m_smoothedPathPolyLine[0].m_second == updateStamp)
	{
		//m_smoothedPathPolyLine.pop_front();
		m_smoothedPathPolyLine.Delete(0);
	}
}


void RacingLine::RoughPathPointAdd(u32 updateStamp, const Vector3& newRoughPathPoint)
{
	const int roughPathPointCount = m_roughPathPolyLine.size();
	if((roughPathPointCount == 0) || ((newRoughPathPoint - m_roughPathPolyLine[roughPathPointCount - 1].m_first).Mag() > 0.01f))
	{
		PointWithUpdateStamp pointWithUpdateStamp(newRoughPathPoint, updateStamp);
		//m_roughPathPolyLine.push_back(pointWithUpdateStamp);
		m_roughPathPolyLine.PushAndGrow(pointWithUpdateStamp);
	}
}


Vector3 RacingLine::RoughPathPointGetLast(void)
{	
	//return m_roughPathPolyLine.get_back().m_first;
	return m_roughPathPolyLine.back().m_first;
}


bool RacingLine::SmoothedPathInit(int numSmoothPathSegsPerRoughSeg)
{
	m_smoothedPathPolyLine.clear();
	m_numSmoothPathSegsPerRoughSeg = numSmoothPathSegsPerRoughSeg;

	return true;
}


void RacingLine::SmoothedPathAdvance(int numSmoothPathSegsPerRoughSeg)
{
	// Check if we need to re-init the smoothed path.
	if(numSmoothPathSegsPerRoughSeg != m_numSmoothPathSegsPerRoughSeg)
	{
		SmoothedPathInit(numSmoothPathSegsPerRoughSeg);
	}
}


void RacingLine::SmoothedPathUpdate(void)
{
	;
}


Vector3 RacingLine::SmoothedPathGetPointPos(int pathPointIndex) const
{
	Assert(pathPointIndex < (int)m_smoothedPathPolyLine.size());
	return m_smoothedPathPolyLine[pathPointIndex].m_first;
}


void RacingLine::SmoothedPathGetPointAngleAndRadiusOfCurvature(int smoothedPathPointIndex, float& angle_out, float& radius_out) const
{
	const int smoothedPathPointCount = m_smoothedPathPolyLine.size();
	Assert(smoothedPathPointIndex <= smoothedPathPointCount);

	// Get or create the 3 associated node positions.
	Vector3 nodeAPos;
	Vector3 nodeBPos;
	Vector3 nodeCPos;
	if(smoothedPathPointIndex == 0)
	{
		nodeBPos = m_smoothedPathPolyLine[smoothedPathPointIndex].m_first;
		nodeCPos = m_smoothedPathPolyLine[smoothedPathPointIndex + 1].m_first;

		const Vector3 nodeBCDelta = nodeCPos - nodeBPos;
		nodeAPos = nodeBPos - nodeBCDelta;
	}
	else if(smoothedPathPointIndex < (smoothedPathPointCount - 1))
	{
		nodeAPos = m_smoothedPathPolyLine[smoothedPathPointIndex - 1].m_first;
		nodeBPos = m_smoothedPathPolyLine[smoothedPathPointIndex].m_first;
		nodeCPos = m_smoothedPathPolyLine[smoothedPathPointIndex + 1].m_first;
	}
	else
	{
		nodeAPos = m_smoothedPathPolyLine[smoothedPathPointIndex - 1].m_first;
		nodeBPos = m_smoothedPathPolyLine[smoothedPathPointIndex].m_first;
		const Vector3 nodeABDelta = nodeBPos - nodeAPos;
		nodeCPos = nodeBPos + nodeABDelta;
	}
	Assert(Vec3IsValid(nodeAPos));
	Assert(Vec3IsValid(nodeBPos));
	Assert(Vec3IsValid(nodeCPos));

	// Get the angle between the nodes.
	angle_out = 0.0f;
	{
		Vector2 springADir((nodeBPos - nodeAPos), Vector2::kXY);
		Assert(Vec2IsValid(springADir));
		springADir.Normalize();
		Assert(Vec2IsValid(springADir));
		Assert(springADir.Mag() > 0.99f);
		Vector2 springBDir((nodeCPos - nodeBPos), Vector2::kXY);
		Assert(Vec2IsValid(springBDir));
		springBDir.Normalize();
		Assert(Vec2IsValid(springBDir));
		Assert(springBDir.Mag() > 0.99f);
		const Vector2 springBSideRightDir(springBDir.y, -springBDir.x);
		Assert(Vec2IsValid(springBSideRightDir));
		const float nextSpringsDirAlignmentForward = springBDir.Dot(springADir);
		const float nextSpringsDirAlignmentRightward = springBSideRightDir.Dot(springADir);
		Assert(!(nextSpringsDirAlignmentForward == 0.0f && nextSpringsDirAlignmentRightward == 0.0f));
		angle_out = rage::Atan2f(-nextSpringsDirAlignmentRightward, nextSpringsDirAlignmentForward);
		angle_out = fwAngle::LimitRadianAngle(angle_out);
	}

	if(	(rage::Abs(angle_out) < 0.0001f) ||
		(rage::Abs(angle_out) > (PI - 0.0001f)))
	{
		radius_out = 10000000.0f;// A really big radius.
	}
	else
	{
		// Formula from http://en.wikipedia.org/wiki/Radius.
		radius_out = rage::Abs((nodeAPos - nodeCPos).Mag() / (2.0f * rage::Sinf(rage::Abs(angle_out))));
	}

	Assert(radius_out >= 0.0f);
}


void RacingLine::SmoothedPathGetPointAngleAndRadiusOfCurvature(int smoothedPathPointIndex, float smoothedPathSegmentPortion, float& angle_out, float& radius_out) const
{
	const int smoothedPathPointCount = m_smoothedPathPolyLine.size();
	Assert(smoothedPathPointIndex <= smoothedPathPointCount);
	Assert((smoothedPathSegmentPortion >= 0.0f) && (smoothedPathSegmentPortion <= 1.0f));

	if(smoothedPathPointIndex == (smoothedPathPointCount - 1))
	{
		SmoothedPathGetPointAngleAndRadiusOfCurvature(smoothedPathPointIndex, angle_out, radius_out);
	}
	else
	{
		float angleA	= 0.0f;
		float radiusA	= 0.0f;
		SmoothedPathGetPointAngleAndRadiusOfCurvature(smoothedPathPointIndex, angleA, radiusA);

		float angleB	= 0.0f;
		float radiusB	= 0.0f;
		SmoothedPathGetPointAngleAndRadiusOfCurvature(smoothedPathPointIndex + 1, angleB, radiusB);

		angle_out = Lerp(smoothedPathSegmentPortion, angleA, angleB);
		radius_out = Lerp(smoothedPathSegmentPortion, radiusA, radiusB);
	}
}


float RacingLine::SmoothedPathGetPointVelPortionMaxPropagated(int smoothedPathPointIndex, float smoothedPathSegmentPortion, float vehVelMax, float vehSideAccMax, float ASSERT_ONLY(vehStartAccMax), float vehStopAccMax)
{
	const int smoothedPathPointCount = m_smoothedPathPolyLine.size();
	Assert(smoothedPathPointIndex <= smoothedPathPointCount);
	Assert((smoothedPathSegmentPortion >= 0.0f) && (smoothedPathSegmentPortion <= 1.0f));
	Assert(vehVelMax >= 0.0f);
	Assert(vehSideAccMax >= 0.0f);
	Assert(vehStartAccMax >= 0.0f);
	Assert(vehStopAccMax >= 0.0f);

	if(smoothedPathPointCount == 0)
	{
		return 0.0f;
	}

	//atArray<float>	maxVelUnclampedAtNodes;
	//atArray<float>	distToNextNodes;
	m_maxVelUnclampedAtNodes.clear();
	m_distToNextNodes.clear();

	for(int i = 0; i < smoothedPathPointCount; ++i)
	{
		float	nodeBAngle	= 0.0f;
		float	nodeBRadius	= 0.0f;
		SmoothedPathGetPointAngleAndRadiusOfCurvature(i, nodeBAngle, nodeBRadius);

		const float nodeBVel2MaxUnclamped = (nodeBRadius * vehSideAccMax);
		Assert(nodeBVel2MaxUnclamped >= 0.0f);
		const float nodeBVelMaxUnclamped = rage::Sqrtf(nodeBVel2MaxUnclamped);
		m_maxVelUnclampedAtNodes.PushAndGrow(nodeBVelMaxUnclamped);

		float distToNextNode = 0.0f;
		if(i < (smoothedPathPointCount - 1))
		{
			distToNextNode = (m_smoothedPathPolyLine[i + 1].m_first - m_smoothedPathPolyLine[i].m_first).Mag();
		}
		else
		{
			// Just use the same value as the last link.
			distToNextNode = (m_smoothedPathPolyLine[smoothedPathPointCount - 1].m_first - m_smoothedPathPolyLine[smoothedPathPointCount - 2].m_first).Mag();
		}
		m_distToNextNodes.PushAndGrow(distToNextNode);
	}

	// Propagate the max decelerations backwards (this assures we are never going too fast to make the upcoming turns).
	for(int i = (smoothedPathPointCount - 1); i > 0; --i)
	{
		// The total braking acceleration over the available distance between the nodes must
		// be taken into account.
		const float currentNodesMaxVel = m_maxVelUnclampedAtNodes[i];
		const float prevNodesMaxVel = m_maxVelUnclampedAtNodes[i - 1];
		const float distBetweenNodes = m_distToNextNodes[i - 1];
		const float prevNodesNewMaxVel2ToMakeNextNode = ((currentNodesMaxVel * currentNodesMaxVel) + (2.0f * vehStopAccMax * distBetweenNodes));
		Assert(prevNodesNewMaxVel2ToMakeNextNode >= 0.0f);
		const float prevNodesNewMaxVelToMakeNextNode = rage::Sqrtf(prevNodesNewMaxVel2ToMakeNextNode);
		if(prevNodesNewMaxVelToMakeNextNode < prevNodesMaxVel)
		{
			m_maxVelUnclampedAtNodes[i - 1] = prevNodesNewMaxVelToMakeNextNode;
		}
	}

	//		// Propagate the max accelerations forwards (this assures we are always going as fast as we can).
	//		for(int i = 0; i < (smoothedPathPointCount - 1); ++i)
	//		{
	//			// The total start acceleration over the available distance between the nodes must
	//			// be taken into account.
	//			const float currentNodesMaxVel = maxVelUnclampedAtNodes[i];
	//			const float nextNodesMaxVel = maxVelUnclampedAtNodes[i + 1];
	//			const float distBetweenNodes = distToNextNodes[i];
	//			const float nextNodesNewMaxVel2 = ((currentNodesMaxVel * currentNodesMaxVel) + (2.0f * vehStartAccMax * distBetweenNodes));
	//			Assert(nextNodesNewMaxVel2 >= 0.0f);
	//			const float nextNodesNewMaxVel = rage::Sqrtf(nextNodesNewMaxVel2);
	//			if(nextNodesNewMaxVel < nextNodesMaxVel)
	//			{
	//				maxVelUnclampedAtNodes[i + 1] = nextNodesNewMaxVel;
	//			}
	//		}

	// Get the vel portion at the requested location along the path.
	float nodeAVelMaxUnclamped = m_maxVelUnclampedAtNodes[smoothedPathPointIndex];
	float nodeAVelMaxClamped = rage::Clamp(nodeAVelMaxUnclamped, 0.0f, vehVelMax);
	float nodeAVelMaxPortion = (nodeAVelMaxClamped / vehVelMax);
	if(smoothedPathPointIndex < (smoothedPathPointCount - 1))
	{
		float nodeBVelMaxUnclamped = m_maxVelUnclampedAtNodes[smoothedPathPointIndex + 1];
		float nodeBVelMaxClamped = rage::Clamp(nodeBVelMaxUnclamped, 0.0f, vehVelMax);
		float nodeBVelMaxPortion = (nodeBVelMaxClamped / vehVelMax);

		float velMaxPortionInterpolated = Lerp(smoothedPathSegmentPortion, nodeAVelMaxPortion, nodeBVelMaxPortion);

		return velMaxPortionInterpolated;
	}
	else
	{
		return nodeAVelMaxPortion;
	}
}


Vector3 RacingLine::SmoothedPathGetPosAlong(int startPathPointIndex, float startPathSegmentPortion, float lookAheadDist) const
{
	const int smoothedPathPointCount = m_smoothedPathPolyLine.size();
	Assert(startPathPointIndex <= smoothedPathPointCount);
	Assert((startPathSegmentPortion >= 0.0f) && (startPathSegmentPortion <= 1.0f));
	Assert(lookAheadDist >= 0.0f);

	if(smoothedPathPointCount == 0)
	{
		return Vector3(0.0f, 0.0f, 0.0f);
	}

	if(startPathPointIndex >= (smoothedPathPointCount - 1))
	{
		return m_smoothedPathPolyLine[smoothedPathPointCount - 1].m_first;
	}

	Vector3	pathPointAPos		= m_smoothedPathPolyLine[startPathPointIndex].m_first;
	Vector3	pathPointBPos		= m_smoothedPathPolyLine[startPathPointIndex + 1].m_first;
	Vector3	pathPointABDelta	= (pathPointBPos - pathPointAPos);
	float	pathSegmentLength	= pathPointABDelta.Mag();

	const float pathSegmentPortionRemaining	= (1.0f - startPathSegmentPortion);
	const float pathSegmentLengthRemaining	= pathSegmentPortionRemaining * pathSegmentLength;

	if(lookAheadDist < pathSegmentLengthRemaining)
	{
		const float pathSegmentLengthUsed	= pathSegmentLengthRemaining - lookAheadDist;
		const float pathSegmentPortionUsed	= pathSegmentLengthUsed / pathSegmentLength;

		const float pathSegmentPortionOfEndPoint = startPathSegmentPortion + pathSegmentPortionUsed;
		const Vector3 posOnPath(pathPointAPos + (pathPointABDelta * pathSegmentPortionOfEndPoint));

		return posOnPath;
	}

	float lengthSoFar = pathSegmentLengthRemaining;
	for(int i = startPathPointIndex + 1; i < smoothedPathPointCount - 1; ++i)
	{
		pathPointAPos		= m_smoothedPathPolyLine[i].m_first;
		pathPointBPos		= m_smoothedPathPolyLine[i + 1].m_first;
		pathPointABDelta	= (pathPointBPos - pathPointAPos);
		pathSegmentLength	= pathPointABDelta.Mag();

		const float lengthToGo = lookAheadDist - lengthSoFar;
		if(lengthToGo <= pathSegmentLength)
		{
			const float pathSegmentPortionOfEndPoint = lengthToGo / pathSegmentLength;
			const Vector3 posOnPath(pathPointAPos + (pathPointABDelta * pathSegmentPortionOfEndPoint));

			return posOnPath;
		}

		lengthSoFar += pathSegmentLength;
	}

	// We've gone to the end of the smoothed path so just return the end point.
	return m_smoothedPathPolyLine[smoothedPathPointCount - 1].m_first;
}


Vector3 RacingLine::SmoothedPathGetClosestPos(const Vector3& pos, int& pathPointClosest_out, float& pathSegmentPortion_out) const
{
	const int smoothedPathPointCount = m_smoothedPathPolyLine.size();
	float pathPointSegClosestDist = 100000.0f;// Large sentinel value.
	Vector3 closestPos(0.0f, 0.0f, 0.0f);
	bool pointFound = false;
	for(int i = 0; i < (smoothedPathPointCount - 1); ++i)
	{
		Vector3 pathPointAPos = m_smoothedPathPolyLine[i].m_first;
		Vector3 pathPointBPos = m_smoothedPathPolyLine[i + 1].m_first;

		// Find the closest position to this pathPoint and portion along it's segment to the next pathPoint.
		float segmentPortion = 0.0f;
		Vector3 closestPointOnSegmentFlat = GetClosestPointOnSegmentFlat(pathPointAPos, pathPointBPos, pos, segmentPortion);

		// Check if this is the closest one so far.
		Vector3 posFlat = pos;
		posFlat.z = 0.0f;
		const float distToSeg = (posFlat - closestPointOnSegmentFlat).Mag();
		if(distToSeg < pathPointSegClosestDist)
		{
			pointFound = true;

			pathPointClosest_out = i;
			pathSegmentPortion_out = segmentPortion;

			pathPointSegClosestDist = distToSeg;
			closestPos = pathPointAPos + ((pathPointBPos - pathPointAPos) * segmentPortion);
		}
	}

	//if we haven't found something on the smoothed path, try the rough
	if(pointFound == false)
	{
		const int roughPathPointCount = m_roughPathPolyLine.size();
		if(roughPathPointCount > 2)
		{
			for(int i = 0; i < (roughPathPointCount - 1); ++i)
			{
				Vector3 pathPointAPos = m_roughPathPolyLine[i].m_first;
				Vector3 pathPointBPos = m_roughPathPolyLine[i + 1].m_first;

				// Find the closest position to this pathPoint and portion along it's segment to the next pathPoint.
				float segmentPortion = 0.0f;
				Vector3 closestPointOnSegmentFlat = GetClosestPointOnSegmentFlat(pathPointAPos, pathPointBPos, pos, segmentPortion);

				// Check if this is the closest one so far.
				Vector3 posFlat = pos;
				posFlat.z = 0.0f;
				const float distToSeg = (posFlat - closestPointOnSegmentFlat).Mag();
				if(distToSeg < pathPointSegClosestDist)
				{
					pathPointClosest_out = i;
					pathSegmentPortion_out = segmentPortion;

					pathPointSegClosestDist = distToSeg;
					closestPos = pathPointAPos + ((pathPointBPos - pathPointAPos) * segmentPortion);
				}
			}
		}
		else if(roughPathPointCount > 0)
		{
			closestPos = m_roughPathPolyLine[roughPathPointCount-1].m_first;//just aim for the furthest node we have, it'll be ok
		}
	}

	return closestPos;
}

void RacingLine::Draw(	float DEV_ONLY(drawVertOffset),
	Color32 DEV_ONLY(roughPathPointsColour),
	Color32 DEV_ONLY(smoothPathColour),
	bool DEV_ONLY(drawPathAsHeath),
	float DEV_ONLY(vehVelMax),
	float DEV_ONLY(vehSideAccMax),
	float DEV_ONLY(vehStartAccMax),
	float DEV_ONLY(vehStopAccMax)
	) const
{
#if __DEV
	RoughPathPointsDraw(drawVertOffset, roughPathPointsColour);
	if(drawPathAsHeath)
	{
		SmoothedPathDrawHeat(drawVertOffset, vehVelMax, vehSideAccMax, vehStartAccMax, vehStopAccMax);
	}
	else
	{
		SmoothedPathDraw(drawVertOffset, smoothPathColour);
	}
#endif // __DEV
}


#if __DEV
void RacingLine::RoughPathPointsDraw(float drawVertOffset, Color32 roughPathPointsColour) const
{
	// Draw the rough path points.
	const int roughPathPointCount = m_roughPathPolyLine.size();
	Vec3V drawVertOffsetV(0.0f, 0.0f, drawVertOffset);

	for(int i = 0; i < roughPathPointCount; ++i)
	{
		Vec3V pos = RCC_VEC3V(m_roughPathPolyLine[i].m_first);
		pos += drawVertOffsetV;
		grcDebugDraw::Cross(pos, 0.25f, roughPathPointsColour);
	}
}


void RacingLine::SmoothedPathDraw(float drawVertOffset, Color32 smoothPathColour) const
{
	// Draw the smoothed path.
	const int smoothedPathPointCount = m_smoothedPathPolyLine.size();
	for(int i = 0; i < (smoothedPathPointCount - 1); ++i)
	{
		Vector3 a = m_smoothedPathPolyLine[i].m_first;
		a.z += drawVertOffset;
		Vector3 b = m_smoothedPathPolyLine[i + 1].m_first;
		b.z += drawVertOffset;
		grcDebugDraw::Line(a, b, smoothPathColour);
	}
}


// Draw the racing line.
void RacingLine::SmoothedPathDrawHeat(	float drawVertOffset,
	float vehVelMax,
	float vehSideAccMax,
	float UNUSED_PARAM(vehStartAccMax),
	float vehStopAccMax
	) const
{
	const int smoothedPathPointCount = m_smoothedPathPolyLine.size();
	if(smoothedPathPointCount == 0)
	{
		return;
	}

	Vec3V vDrawVertOffsetZ = Vec3VFromF32(drawVertOffset);
	vDrawVertOffsetZ = And(vDrawVertOffsetZ, Vec3V(V_MASKZ));

	TUNE_GROUP_BOOL(ACK, propagateMaxVels, true);
	if(propagateMaxVels)
	{
		atArray<float>	maxVelUnclampedAtNodes;
		atArray<float>	distToNextNodes;
		for(int i = 0; i < smoothedPathPointCount; ++i)
		{
			float	nodeBAngle	= 0.0f;
			float	nodeBRadius	= 0.0f;
			SmoothedPathGetPointAngleAndRadiusOfCurvature(i, nodeBAngle, nodeBRadius);

			const float nodeBVel2MaxUnclamped = nodeBRadius * vehSideAccMax;
			Assert(nodeBVel2MaxUnclamped >= 0.0f);
			const float nodeBVelMaxUnclamped = rage::Sqrtf(nodeBVel2MaxUnclamped);
			maxVelUnclampedAtNodes.PushAndGrow(nodeBVelMaxUnclamped);

			float distToNextNode = 0.0f;
			if(i < (smoothedPathPointCount - 1))
			{
				distToNextNode = (m_smoothedPathPolyLine[i + 1].m_first - m_smoothedPathPolyLine[i].m_first).Mag();
			}
			else
			{
				// Just use the same value as the last link.
				distToNextNode = (m_smoothedPathPolyLine[smoothedPathPointCount - 1].m_first - m_smoothedPathPolyLine[smoothedPathPointCount - 2].m_first).Mag();
			}
			distToNextNodes.PushAndGrow(distToNextNode);
		}

		// Propagate the max decelerations backwards (this assures we are never going too fast to make the upcoming turns).
		for(int i = (smoothedPathPointCount - 1); i > 0; --i)
		{
			// The total braking acceleration over the available distance between the nodes must
			// be taken into account.
			const float currentNodesMaxVel = maxVelUnclampedAtNodes[i];
			const float prevNodesMaxVel = maxVelUnclampedAtNodes[i - 1];
			const float distBetweenNodes = distToNextNodes[i - 1];

			const float prevNodesNewMaxVel2ToMakeNextNode = (currentNodesMaxVel * currentNodesMaxVel) + (2.0f * vehStopAccMax * distBetweenNodes);
			Assert(prevNodesNewMaxVel2ToMakeNextNode >= 0.0f);
			const float prevNodesNewMaxVelToMakeNextNode = rage::Sqrtf(prevNodesNewMaxVel2ToMakeNextNode);
			if(prevNodesNewMaxVelToMakeNextNode < prevNodesMaxVel)
			{
				maxVelUnclampedAtNodes[i - 1] = prevNodesNewMaxVelToMakeNextNode;
			}
		}

		//			// Propagate the max accelerations forwards (this assures we are always going as fast as we can).
		//			for(int i = 0; i < (smoothedPathPointCount - 1); ++i)
		//			{
		//				// The total start acceleration over the available distance between the nodes must
		//				// be taken into account.
		//				const float currentNodesMaxVel = maxVelUnclampedAtNodes[i];
		//				const float nextNodesMaxVel = maxVelUnclampedAtNodes[i + 1];
		//				const float distBetweenNodes = distToNextNodes[i];
		//				const float nextNodesNewMaxVel2 = (currentNodesMaxVel * currentNodesMaxVel) + (2.0f * vehStartAccMax * distBetweenNodes);
		//				Assert(nextNodesNewMaxVel2 >= 0.0f);
		//				const float nextNodesNewMaxVel = rage::Sqrtf(nextNodesNewMaxVel2);
		//				if(nextNodesNewMaxVel < nextNodesMaxVel)
		//				{
		//					maxVelUnclampedAtNodes[i + 1] = nextNodesNewMaxVel;
		//				}
		//			}

		Vec3V nodeAPos(RCC_VEC3V(m_smoothedPathPolyLine[0].m_first));
		nodeAPos += vDrawVertOffsetZ;
		Color32 nodeACurvatureBasedColour(0.0f, 1.0f, 0.0f, 1.0f);
		for(int i = 0; i < smoothedPathPointCount; ++i)
		{
			Vec3V nodeBPos	= VECTOR3_TO_VEC3V(SmoothedPathGetPointPos(i));
			nodeBPos += vDrawVertOffsetZ;

			float nodeBVelMaxUnclamped = maxVelUnclampedAtNodes[i];
			float nodeBVelMaxClamped = rage::Clamp(nodeBVelMaxUnclamped, 0.0f, vehVelMax);
			float nodeBVelMaxPortion = (nodeBVelMaxClamped / vehVelMax);

			// Calculate the color to use.
			const float		nodeBCurvePortion = 1.0f - nodeBVelMaxPortion;
			const float		nodeBRedPortion = rage::Clamp(2.0f * nodeBCurvePortion, 0.0f, 1.0f);
			const float		nodeBGreenPortion = rage::Clamp(2.0f - (2.0f * nodeBCurvePortion), 0.0f, 1.0f);
			Color32 nodeBCurvatureBasedColour(nodeBRedPortion, nodeBGreenPortion, 0.0f, 1.0f);
			TUNE_BOOL(FORCE_RED, false);
			TUNE_FLOAT(radius, 0.1f, 0.0f, 1.0f, 0.01f);
			if(FORCE_RED){nodeBCurvatureBasedColour = Color32(1.0f, 0.0f, 0.0f, 1.0f);}

			grcDebugDraw::Cylinder(nodeAPos, nodeBPos, radius, nodeACurvatureBasedColour, nodeBCurvatureBasedColour, false, true, 5);

			// Update our rolling history.
			nodeAPos = nodeBPos;
			nodeACurvatureBasedColour = nodeBCurvatureBasedColour;
		}
	}
	else
	{
		Vec3V nodeAPos(RCC_VEC3V(m_smoothedPathPolyLine[0].m_first));
		nodeAPos += vDrawVertOffsetZ;
		Color32 nodeACurvatureBasedColour(0.0f, 1.0f, 0.0f, 1.0f);
		for(int i = 0; i < smoothedPathPointCount; ++i)
		{
			Vec3V nodeBPos	= VECTOR3_TO_VEC3V(SmoothedPathGetPointPos(i));
			nodeBPos += vDrawVertOffsetZ;
			float	nodeBAngle	= 0.0f;
			float	nodeBRadius	= 0.0f;
			SmoothedPathGetPointAngleAndRadiusOfCurvature(i, nodeBAngle, nodeBRadius);

			const float nodeBVel2MaxUnclamped = (nodeBRadius * vehSideAccMax);
			Assert(nodeBVel2MaxUnclamped >= 0.0f);
			const float nodeBVelMaxUnclamped = rage::Sqrtf(nodeBVel2MaxUnclamped);
			const float nodeBVelMaxClamped = rage::Clamp(nodeBVelMaxUnclamped, 0.0f, vehVelMax);
			const float nodeBVelMaxPortion = (rage::Abs(nodeBAngle) < 0.001f)?1.0f:(nodeBVelMaxClamped / vehVelMax);

			// Calculate the color to use.
			const float		nodeBCurvePortion = 1.0f - nodeBVelMaxPortion;
			const float		nodeBRedPortion = rage::Clamp(2.0f * nodeBCurvePortion, 0.0f, 1.0f);
			const float		nodeBGreenPortion = rage::Clamp(2.0f - (2.0f * nodeBCurvePortion), 0.0f, 1.0f);
			Color32 nodeBCurvatureBasedColour(nodeBRedPortion, nodeBGreenPortion, 0.0f, 1.0f);
			TUNE_BOOL(FORCE_RED, false);
			TUNE_FLOAT(radius, 0.1f, 0.0f, 1.0f, 0.01f);
			if(FORCE_RED){nodeBCurvatureBasedColour = Color32(1.0f, 0.0f, 0.0f, 1.0f);}

			grcDebugDraw::Cylinder(nodeAPos, nodeBPos, radius, nodeACurvatureBasedColour, nodeBCurvatureBasedColour, false, true, 5);

			// Update our rolling history.
			nodeAPos = nodeBPos;
			nodeACurvatureBasedColour = nodeBCurvatureBasedColour;
		}
	}
}
#endif // __DEV


Vector3 RacingLine::GetClosestPointOnSegmentFlat(const Vector3& segmentAPos, const Vector3& segmentBPos, const Vector3& pos, float& segmentPortion) const
{
	//const Vector3 pathPointABDelta = segmentBPos - segmentAPos;
	Vector3 pathPointABDeltaFlat = segmentBPos - segmentAPos; pathPointABDeltaFlat.z = 0.0f;
	Vector3 diffVehPosFlat(pos - segmentAPos); diffVehPosFlat.z = 0.0f;
	float dotPrFlat = diffVehPosFlat.Dot(pathPointABDeltaFlat);
	Vector3 closestPointOnSegmentFlat = segmentAPos;
	segmentPortion = 0.0f;
	if(dotPrFlat > 0.0f)
	{
		const float length2 = pathPointABDeltaFlat.Mag2();
		dotPrFlat /= length2;
		if (dotPrFlat >= 1.0f)
		{
			closestPointOnSegmentFlat = segmentBPos;
			segmentPortion = 1.0f;
		}
		else
		{
			closestPointOnSegmentFlat = segmentAPos + pathPointABDeltaFlat * dotPrFlat;
			segmentPortion = dotPrFlat;
		}
	}
	closestPointOnSegmentFlat.z = 0.0f;

	return closestPointOnSegmentFlat;
}


//---------------------------------------------------------------------------
// RacingLineCubic
//---------------------------------------------------------------------------
RacingLineCubic::RacingLineCubic()
{
	;
}


RacingLineCubic::~RacingLineCubic()
{
	;
}


bool RacingLineCubic::SmoothedPathInit(int numSmoothPathSegsPerRoughSeg)
{
	RacingLine::SmoothedPathInit(numSmoothPathSegsPerRoughSeg);
	if(m_roughPathPolyLine.size() >= 4)
	{
		MakeSmoothedPathCubic(m_numSmoothPathSegsPerRoughSeg);
		return true;
	}
	return false;
}


void RacingLineCubic::SmoothedPathAdvance(int numSmoothPathSegsPerRoughSeg)
{
	// Possible re-init the smoothed path.
	RacingLine::SmoothedPathAdvance(numSmoothPathSegsPerRoughSeg);

	// Just remake the entire smooth path as the changes can propagate
	// to several nodes away.
	SmoothedPathInit(m_numSmoothPathSegsPerRoughSeg);
}


// Only call if there are 4 rough path points points(m_roughPathPointCount > 3)...
void RacingLineCubic::MakeSmoothedPathCubic(int numSmoothPathSegsPerRoughSeg)
{
	const int roughPathPointCount = m_roughPathPolyLine.size();
	Assert(roughPathPointCount > 3);

	// Make the points.
	for(int i = 0; i < roughPathPointCount; ++i)
	{
		Vector3 roughPathPointPrev;
		if(i > 0)
		{
			roughPathPointPrev = m_roughPathPolyLine[i - 1].m_first;
		}
		else
		{
			roughPathPointPrev = (m_roughPathPolyLine[0].m_first * 2.0f) - m_roughPathPolyLine[1].m_first;
		}

		Vector3 roughPathPointCurr = m_roughPathPolyLine[i].m_first;

		Vector3 roughPathPointNext;
		if(i < (roughPathPointCount - 1))
		{
			roughPathPointNext = m_roughPathPolyLine[i + 1].m_first;
		}
		else // i == (roughPathPointCount - 1)
		{
			roughPathPointNext = (m_roughPathPolyLine[roughPathPointCount - 1].m_first * 2.0f) - m_roughPathPolyLine[roughPathPointCount - 2].m_first;
		}

		Vector3 roughPathPointFutr;
		if(i < (roughPathPointCount - 2))
		{
			roughPathPointFutr = m_roughPathPolyLine[i + 2].m_first;
		}
		else if(i == (roughPathPointCount - 2))
		{
			roughPathPointFutr = (m_roughPathPolyLine[roughPathPointCount - 1].m_first * 2.0f) - m_roughPathPolyLine[roughPathPointCount - 2].m_first;
		}
		else // i == (roughPathPointCount - 1)
		{
			roughPathPointFutr = (m_roughPathPolyLine[roughPathPointCount - 1].m_first * 3.0f) - (m_roughPathPolyLine[roughPathPointCount - 2].m_first * 2.0f);
		}

		float a[4];
		a[0] =(-roughPathPointPrev.x + 3.0f * roughPathPointCurr.x - 3.0f * roughPathPointNext.x + roughPathPointFutr.x) / 6.0f;
		a[1] =(3.0f * roughPathPointPrev.x - 6.0f * roughPathPointCurr.x + 3.0f * roughPathPointNext.x) / 6.0f;
		a[2] =(-3.0f * roughPathPointPrev.x + 3.0f * roughPathPointNext.x) / 6.0f;
		a[3] =(roughPathPointPrev.x + 4.0f * roughPathPointCurr.x + roughPathPointNext.x) / 6.0f;

		float b[4];
		b[0] =(-roughPathPointPrev.y + 3.0f * roughPathPointCurr.y - 3.0f * roughPathPointNext.y + roughPathPointFutr.y) / 6.0f;
		b[1] =(3.0f * roughPathPointPrev.y - 6.0f * roughPathPointCurr.y + 3.0f * roughPathPointNext.y) / 6.0f;
		b[2] =(-3.0f * roughPathPointPrev.y + 3.0f * roughPathPointNext.y) / 6.0f;
		b[3] =(roughPathPointPrev.y + 4.0f * roughPathPointCurr.y + roughPathPointNext.y) / 6.0f;

		float c[4];
		c[0] =(-roughPathPointPrev.z + 3.0f * roughPathPointCurr.z - 3.0f * roughPathPointNext.z + roughPathPointFutr.z) / 6.0f;
		c[1] =(3.0f * roughPathPointPrev.z - 6.0f * roughPathPointCurr.z + 3.0f * roughPathPointNext.z) / 6.0f;
		c[2] =(-3.0f * roughPathPointPrev.z + 3.0f * roughPathPointNext.z) / 6.0f;
		c[3] =(roughPathPointPrev.z + 4.0f * roughPathPointCurr.z + roughPathPointNext.z) / 6.0f;

		// Determine if we should add the last point or not.
		const bool addLastPointAlongSegment = (i == (roughPathPointCount - 1));

		// Add the smooth points.
		const int numPoints = addLastPointAlongSegment?numSmoothPathSegsPerRoughSeg:numSmoothPathSegsPerRoughSeg - 1;
		for(int j = 0; j < numPoints; ++j)
		{
			const float t = static_cast<float>(j) / static_cast<float>(numSmoothPathSegsPerRoughSeg - 1);

			const float x =	a[3] + t *(a[2] + t *(a[1] + t * a[0]));
			const float y =	b[3] + t *(b[2] + t *(b[1] + t * b[0]));
			const float z =	c[3] + t *(c[2] + t *(c[1] + t * c[0]));

			const Vector3 newPoint(x, y, z);

#if __ASSERT
			const int smoothedPathPointCount = m_smoothedPathPolyLine.size();
			if(smoothedPathPointCount > 0)
			{
				const Vector3 lastAddedPoint = m_smoothedPathPolyLine[smoothedPathPointCount - 1].m_first;
				Assert(newPoint != lastAddedPoint);
			}
#endif // __ASSERT

			PointWithUpdateStamp pointWithUpdateStamp(newPoint, m_roughPathPolyLine[i].m_second);
			//m_smoothedPathPolyLine.push_back(pointWithUpdateStamp);
			m_smoothedPathPolyLine.PushAndGrow(pointWithUpdateStamp);
		}
	}
}


//---------------------------------------------------------------------------
// RacingLineCardinal
//---------------------------------------------------------------------------
RacingLineCardinal::RacingLineCardinal()
{
	;
}


RacingLineCardinal::~RacingLineCardinal()
{
	;
}

//returns whether we were able to smooth the path
bool RacingLineCardinal::SmoothedPathInit(int numSmoothPathSegsPerRoughSeg)
{
	RacingLine::SmoothedPathInit(numSmoothPathSegsPerRoughSeg);
	if(m_roughPathPolyLine.size() >= 4)
	{
		MakeSmoothedPathCardinal(m_numSmoothPathSegsPerRoughSeg);
		return true;
	}
	return false;
}

void RacingLineCardinal::SmoothedPathAdvance(int numSmoothPathSegsPerRoughSeg)
{
	// Possible re-init the smoothed path.
	RacingLine::SmoothedPathAdvance(numSmoothPathSegsPerRoughSeg);

	// Just remake the entire smooth path as the changes can propagate
	// to several nodes away.
	SmoothedPathInit(m_numSmoothPathSegsPerRoughSeg);
}


// CardinalPath is responsible for connecting
// a set of points with curves.
void RacingLineCardinal::MakeSmoothedPathCardinal(int numSmoothPathSegsPerRoughSeg)
{
	const int roughPathPointCount = m_roughPathPolyLine.size();
	Assert(roughPathPointCount > 3);

	// Set up the path output.
	for(int i = 0; i < roughPathPointCount; ++i)
	{
		float t = 0.0f;
		float deltaTPerPathStep = 1.0f / static_cast<float>(numSmoothPathSegsPerRoughSeg - 1);

		// Determine if we should add the last point or not.
		const bool addLastPointAlongSegment = (i == (roughPathPointCount - 1));

		// Add the smooth points.
		const int numPoints = addLastPointAlongSegment?numSmoothPathSegsPerRoughSeg:numSmoothPathSegsPerRoughSeg - 1;
		for(int j = 0; j < numPoints; ++j)
		{
			Vector3 roughPathPointPrev;
			if(i > 0)
			{
				roughPathPointPrev = m_roughPathPolyLine[i - 1].m_first;
			}
			else
			{
				roughPathPointPrev = (m_roughPathPolyLine[0].m_first * 2.0f) - m_roughPathPolyLine[1].m_first;
			}

			Vector3 roughPathPointCurr = m_roughPathPolyLine[i].m_first;

			Vector3 roughPathPointNext;
			if(i < (roughPathPointCount - 1))
			{
				roughPathPointNext = m_roughPathPolyLine[i + 1].m_first;
			}
			else // i == (roughPathPointCount - 1)
			{
				roughPathPointNext = (m_roughPathPolyLine[roughPathPointCount - 1].m_first * 2.0f) - m_roughPathPolyLine[roughPathPointCount - 2].m_first;
			}

			Vector3 roughPathPointFutr;
			if(i < (roughPathPointCount - 2))
			{
				roughPathPointFutr = m_roughPathPolyLine[i + 2].m_first;
			}
			else if(i == (roughPathPointCount - 2))
			{
				roughPathPointFutr = (m_roughPathPolyLine[roughPathPointCount - 1].m_first * 2.0f) - m_roughPathPolyLine[roughPathPointCount - 2].m_first;
			}
			else // i == (roughPathPointCount - 1)
			{
				roughPathPointFutr = (m_roughPathPolyLine[roughPathPointCount - 1].m_first * 3.0f) - (m_roughPathPolyLine[roughPathPointCount - 2].m_first * 2.0f);
			}

			const float t1 = 1.0f - t;
			const float t12 = t1 * t1;
			const float t2 = t * t;
			const float cp0 = t1 * t12;
			const float cp1 = 3.0f * t* t12;
			const float cp2 = 3.0f * t2* t1;
			const float cp3 = t * t2;
			t += deltaTPerPathStep;

			const float x =	(roughPathPointCurr.x * cp0) +
				(roughPathPointCurr.x +(roughPathPointNext.x - roughPathPointPrev.x) / 6.0f) * cp1 + 
				(roughPathPointNext.x -(roughPathPointFutr.x - roughPathPointCurr.x) / 6.0f) * cp2 +
				(roughPathPointNext.x * cp3);
			const float y =	(roughPathPointCurr.y * cp0) +
				(roughPathPointCurr.y +( roughPathPointNext.y - roughPathPointPrev.y) / 6.0f) * cp1 +
				(roughPathPointNext.y -(roughPathPointFutr.y - roughPathPointCurr.y) / 6.0f) * cp2 +
				(roughPathPointNext.y * cp3);
			const float z =	(roughPathPointCurr.z * cp0) +
				(roughPathPointCurr.z +( roughPathPointNext.z - roughPathPointPrev.z) / 6.0f) * cp1 +
				(roughPathPointNext.z -(roughPathPointFutr.z - roughPathPointCurr.z) / 6.0f) * cp2 +
				(roughPathPointNext.z * cp3);

			const Vector3 newPoint(x, y, z);

#if __ASSERT
			const int smoothedPathPointCount = m_smoothedPathPolyLine.size();
			if(smoothedPathPointCount > 0)
			{
				const Vector3 lastAddedPoint = m_smoothedPathPolyLine[smoothedPathPointCount - 1].m_first;
				Assert(newPoint != lastAddedPoint);
			}
#endif // __ASSERT

			PointWithUpdateStamp pointWithUpdateStamp(newPoint, m_roughPathPolyLine[i].m_second);
			//m_smoothedPathPolyLine.push_back(pointWithUpdateStamp);
			m_smoothedPathPolyLine.PushAndGrow(pointWithUpdateStamp);
		}
	}
}


//---------------------------------------------------------------------------
// CRacingLineMass
// The racing line is approximated by a series of points (masses) connected
// by vectors (springs). This is the representation of one of the masses.
//---------------------------------------------------------------------------
CRacingLineMass::CRacingLineMass()
	:
	m_updateStamp	(0),
	m_mass			(0.0f),
	m_pos			(0.0f, 0.0f, 0.0f),
	m_vel			(0.0f, 0.0f),
	m_force			(0.0f, 0.0f)
{
	;
}


void CRacingLineMass::Set(	const u32 updateStamp,
	const float mass,
	const Vector3& pos)
{
	Assert(mass > 0.0f);
	m_updateStamp = updateStamp;
	m_mass	= mass;
	m_pos	= pos;
	m_vel.Set(0.0f, 0.0f);
	m_force.Set(0.0f, 0.0f);
}


//---------------------------------------------------------------------------
// CRacingLineSpring
// The racing line is approximated by a series of points (masses) connected
// by vectors (springs). This is the representation of one of the springs.
//---------------------------------------------------------------------------
CRacingLineSpring::CRacingLineSpring()
	:
	m_updateStamp	(0),
	m_massIndex0	(-1),
	m_massIndex1	(-1),
	m_restLength	(0.0f),
	m_stiffness		(0.0f)
{
	;
}


void CRacingLineSpring::Set(	const u32 updateStamp,
	const float restLength,
	const float stiffness,
	const int massIndex0,
	const int massIndex1)
{
	Assert(restLength > 0.0f);
	Assert(stiffness > 0.0f);
	Assert(massIndex0 >= 0);
	Assert(massIndex1 >= 0);

	m_updateStamp	= updateStamp;
	m_massIndex0	= massIndex0;
	m_massIndex1	= massIndex1;
	m_restLength	= restLength;
	m_stiffness		= stiffness;
}


//---------------------------------------------------------------------------
// RacingLineOptimizing
//---------------------------------------------------------------------------
RacingLineOptimizing::RacingLineOptimizing()
	: // Initializer list.
	m_numRaceLineMassesPerRoughSeg	(0)
{
	;
}


RacingLineOptimizing::~RacingLineOptimizing()
{
	;
}


void RacingLineOptimizing::ClearAll(void)
{
	RacingLine::ClearAll();

	m_masses.clear();
	m_springs.clear();
	m_boundPolyLines.clear();
}


void RacingLineOptimizing::ClearStampedData(u32 updateStamp)
{
	RacingLine::ClearStampedData(updateStamp);

	while(m_masses.size() && m_masses[0].m_updateStamp == updateStamp)
	{
		m_masses.pop_front();
	}
	while(m_springs.size() && m_springs[0].m_updateStamp == updateStamp)
	{
		m_springs.pop_front();
	}
	const int boundPolyLineCount = m_boundPolyLines.GetCount();
	for(int i = 0; i < boundPolyLineCount; ++i)
	{	
		while(m_boundPolyLines[i].m_first.size() && m_boundPolyLines[i].m_first[0].m_second == updateStamp)
		{
			m_boundPolyLines[i].m_first.pop_front();
		}
	}
}


void RacingLineOptimizing::BoundPolyLinesAddPoint(u32 boundPolyLineId, u32 updateStamp, const Vector3& boundPolyLineNewPoint)
{
	bool boundPolyLineIdFound = false;
	const int boundPolyLineCount = m_boundPolyLines.GetCount();
	for(int i = 0; i < boundPolyLineCount; ++i)
	{	
		if(m_boundPolyLines[i].m_second == boundPolyLineId)
		{
			boundPolyLineIdFound = true;

			PointWithUpdateStamp pointWithUpdateStamp(boundPolyLineNewPoint, updateStamp);
			m_boundPolyLines[i].m_first.push_back(pointWithUpdateStamp);
		}
	}

	if(!boundPolyLineIdFound)
	{
		BoundPolyLineWithId temp;
		temp.m_second = boundPolyLineId;
		m_boundPolyLines.PushAndGrow(temp);

		PointWithUpdateStamp pointWithUpdateStamp(boundPolyLineNewPoint, updateStamp);
		m_boundPolyLines[m_boundPolyLines.GetCount() - 1].m_first.push_back(pointWithUpdateStamp);
	}
}


bool RacingLineOptimizing::SmoothedPathInit(int numSmoothPathSegsPerRoughSeg)
{
	RacingLine::SmoothedPathInit(numSmoothPathSegsPerRoughSeg);
	RaceLineInit(m_numSmoothPathSegsPerRoughSeg);
	TUNE_GROUP_INT(PATH_RACING_LINE, relaxIterationsToDoOnInit, 300, 1, 1000, 1);
	for(int i  = 0; i < relaxIterationsToDoOnInit; ++i)
	{
		RaceLineUpdate();
	}
	SmoothedPathMakeFromRacingLine();

	return true;
}


void RacingLineOptimizing::SmoothedPathAdvance(int numSmoothPathSegsPerRoughSeg)
{
	// Possible re-init the smoothed path.
	RacingLine::SmoothedPathAdvance(numSmoothPathSegsPerRoughSeg);

	// Just remake the entire smooth path as the changes can propagate
	// to several nodes away.
	RaceLineAdvance(m_numSmoothPathSegsPerRoughSeg);
	TUNE_GROUP_INT(PATH_RACING_LINE, relaxIterationsToDoOnAdvance, 20, 1, 1000, 1);
	for(int i  = 0; i < relaxIterationsToDoOnAdvance; ++i)
	{
		RaceLineUpdate();
	}
	SmoothedPathMakeFromRacingLine();
}


void RacingLineOptimizing::SmoothedPathUpdate(void)
{
	TUNE_GROUP_INT(PATH_RACING_LINE, relaxIterationsToDoOnUpdate, 10, 1, 1000, 1);
	for(int i  = 0; i < relaxIterationsToDoOnUpdate; ++i)
	{
		RaceLineUpdate();
	}
	SmoothedPathMakeFromRacingLine();
}


void RacingLineOptimizing::Draw(	float DEV_ONLY(drawVertOffset),
									Color32 DEV_ONLY(roughPathPointsColour),
									Color32 DEV_ONLY(smoothPathColour),
									bool DEV_ONLY(drawPathAsHeath),
									float DEV_ONLY(vehVelMax),
									float DEV_ONLY(vehSideAccMax),
									float DEV_ONLY(vehStartAccMax),
									float DEV_ONLY(vehStopAccMax)
									) const
{
#if __DEV
	RoughPathPointsDraw(drawVertOffset, roughPathPointsColour);
	if(drawPathAsHeath)
	{
		SmoothedPathDrawHeat(drawVertOffset, vehVelMax, vehSideAccMax, vehStartAccMax, vehStopAccMax);
	}
	else
	{
		SmoothedPathDraw(drawVertOffset, smoothPathColour);
	}

	// Draw the track
	TUNE_GROUP_BOOL(PATH_RACING_TRACK, boundsDraw, true);
	TUNE_GROUP_COLOR(PATH_RACING_TRACK, boundsColour, Color32(0.0f, 0.0f, 1.0f, 1.0f));
	if(boundsDraw)
	{
		const int boundPolyLineCount = m_boundPolyLines.GetCount();
		for(int i = 0; i < boundPolyLineCount; ++i)
		{	
			for(int j = 0; j < m_boundPolyLines[i].m_first.size() - 1; ++j)
			{
				Vector3 boundSegA(m_boundPolyLines[i].m_first[j].m_first);
				Vector3 boundSegB(m_boundPolyLines[i].m_first[j + 1].m_first);
				Vector3 boundSegAHigh(boundSegA.x, boundSegA.y, boundSegA.z + (2.0f * drawVertOffset));
				Vector3 boundSegBHigh(boundSegB.x, boundSegB.y, boundSegB.z + (2.0f * drawVertOffset));
				grcDebugDraw::Poly(RCC_VEC3V(boundSegA), RCC_VEC3V(boundSegB), RCC_VEC3V(boundSegAHigh), boundsColour, true);
				grcDebugDraw::Poly(RCC_VEC3V(boundSegAHigh), RCC_VEC3V(boundSegB), RCC_VEC3V(boundSegBHigh), boundsColour, true);
			}
		}
	}
#endif // __DEV
}


void RacingLineOptimizing::SmoothedPathMakeFromRacingLine(void)
{
	m_smoothedPathPolyLine.clear();

	const int massCount = m_masses.size();
	for(int i = 0; i < massCount; ++i)
	{
		PointWithUpdateStamp pointWithUpdateStamp(m_masses[i].m_pos, m_masses[i].m_updateStamp);
		//m_smoothedPathPolyLine.push_back(pointWithUpdateStamp);
		m_smoothedPathPolyLine.PushAndGrow(pointWithUpdateStamp);
	}
}


void RacingLineOptimizing::RaceLineReset(void)
{
	m_numRaceLineMassesPerRoughSeg	= 0;
	m_masses.clear();
	m_springs.clear();
}


// This function uses the existing track to set the initial approximation
// to its racing line to lie along the track's centre line.
void RacingLineOptimizing::RaceLineInit(int numRaceLineMassesPerRoughSeg)
{
	Assert(numRaceLineMassesPerRoughSeg > 0);
	const int roughPathNodeCount = m_roughPathPolyLine.size();
	Assert(roughPathNodeCount > 1);

	RaceLineReset();
	m_numRaceLineMassesPerRoughSeg = numRaceLineMassesPerRoughSeg;

	// Make the racing line initially follow the center of the track.
	// For looped tracks, create an extra spring at the end connecting the
	// first and last masses (though we don't do this right now).
	TUNE_GROUP_FLOAT(PATH_RACING_LINE, springLengthScaleInit, 0.9f, 0.0f, 1.0f, 0.001f);
	TUNE_GROUP_FLOAT(PATH_RACING_LINE, springStiffnessInit, 1.0f, 0.0f, 10.0f, 0.01f);
	TUNE_GROUP_FLOAT(PATH_RACING_LINE, nodeMassInit, 1.0f, 0.0f, 10.0f, 0.01f);
	const int massCount = roughPathNodeCount + ((roughPathNodeCount - 1) * (m_numRaceLineMassesPerRoughSeg - 1));
	for(int massIndex = 0; massIndex < massCount; ++massIndex)
	{
		const u32 updateStamp = m_roughPathPolyLine[massIndex / m_numRaceLineMassesPerRoughSeg].m_second;

		// Add a new point to the racing line which lies at the
		// centre of the track at the start of the new segment.
		// Connect the new point to the last one that was created
		// using a spring so that the distance between the points
		// remains roughly fixed as the racing line is optimized.
		Vector3 mass1InitialPos = RaceLineMassGetInitialPosFromRoughPath(massIndex);
		CRacingLineMass tempMass;
		tempMass.Set(updateStamp, nodeMassInit, mass1InitialPos);
		m_masses.push_back(tempMass);

		if(massIndex > 0)
		{
			Vector3 mass0InitialPos = RaceLineMassGetInitialPosFromRoughPath(massIndex - 1);

			const Vector2 mass0InitialPos2(mass0InitialPos, Vector2::kXY);
			const Vector2 mass1InitialPos2(mass1InitialPos, Vector2::kXY);
			const float	initialLength = (mass1InitialPos2 - mass0InitialPos2).Mag();
			Assert(initialLength > 0.0f);
			const float naturalLengthOfSpring = initialLength * springLengthScaleInit;
			CRacingLineSpring tempSpring;
			tempSpring.Set(updateStamp, naturalLengthOfSpring, springStiffnessInit, (massIndex - 1), massIndex);
			m_springs.push_back(tempSpring);
		}
	}
}


void RacingLineOptimizing::RaceLineAdvance(int numRaceLineMassesPerRoughSeg)
{
	Assert(numRaceLineMassesPerRoughSeg > 0);
	const int roughPathNodeCount = m_roughPathPolyLine.size();
	Assert(roughPathNodeCount > 1);

	// Check if we need to re-init the race line.
	if(numRaceLineMassesPerRoughSeg != m_numRaceLineMassesPerRoughSeg)
	{
		RaceLineInit(numRaceLineMassesPerRoughSeg);
		return;
	}

	// Make the racing line initially follow the center of the track.
	// For looped tracks, create an extra spring at the end connecting the
	// first and last masses (though we don't do this right now).
	TUNE_GROUP_FLOAT(PATH_RACING_LINE, springLengthScaleAdvance, 0.9f, 0.0f, 1.0f, 0.001f);
	TUNE_GROUP_FLOAT(PATH_RACING_LINE, springStiffnessAdvance, 1.0f, 0.0f, 10.0f, 0.01f);
	TUNE_GROUP_FLOAT(PATH_RACING_LINE, nodeMassAdvance, 1.0f, 0.0f, 10.0f, 0.01f);
	const u32 updateStamp = m_roughPathPolyLine[roughPathNodeCount - 1].m_second;
	for(int i = 0; i < m_numRaceLineMassesPerRoughSeg; ++i)
	{
		const int massIndex = (((roughPathNodeCount - 2) * m_numRaceLineMassesPerRoughSeg) + i) + 1;

		// Add a new point to the racing line which lies at the
		// centre of the track at the start of the new segment.
		// Connect the new point to the last one that was created
		// using a spring so that the distance between the points
		// remains roughly fixed as the racing line is optimized.
		Vector3 mass1InitialPos = RaceLineMassGetInitialPosFromRoughPath(massIndex);
		CRacingLineMass tempMass;
		tempMass.Set(updateStamp, nodeMassAdvance, mass1InitialPos);
		m_masses.push_back(tempMass);

		if(massIndex > 0)
		{
			Vector3 mass0InitialPos = RaceLineMassGetInitialPosFromRoughPath(massIndex - 1);

			const Vector2 mass0InitialPos2(mass0InitialPos, Vector2::kXY);
			const Vector2 mass1InitialPos2(mass1InitialPos, Vector2::kXY);
			const float	initialLength = (mass1InitialPos2 - mass0InitialPos2).Mag();
			Assert(initialLength > 0.0f);
			const float naturalLengthOfSpring = initialLength * springLengthScaleAdvance;
			CRacingLineSpring tempSpring;
			tempSpring.Set(updateStamp, naturalLengthOfSpring, springStiffnessAdvance, (massIndex - 1), massIndex);
			m_springs.push_back(tempSpring);
		}
	}
}


// Since we can have multiple masses per track element this function
// makes getting the center position easier.
Vector3 RacingLineOptimizing::RaceLineMassGetInitialPosFromRoughPath(int massIndex) const
{
	Assert(massIndex >= 0);
	Assert(m_roughPathPolyLine.size() > 1);

	const int roughPathNodeIndexA = (massIndex / m_numRaceLineMassesPerRoughSeg);
	Assert(roughPathNodeIndexA < m_roughPathPolyLine.size());

	// Check if we are on top of a rough path node or need to use an
	// interpolated position.
	const bool onRoughPathNode = ((massIndex % m_numRaceLineMassesPerRoughSeg) == 0);
	if(onRoughPathNode)
	{
		const Vector3 pos(m_roughPathPolyLine[roughPathNodeIndexA].m_first);
		Assert(Vec3IsValid(pos));

		return pos;
	}
	else
	{
		const int roughPathNodeIndexB = roughPathNodeIndexA + 1;
		const float lerpFactorAB = static_cast<float>(massIndex % m_numRaceLineMassesPerRoughSeg) / static_cast<float>(m_numRaceLineMassesPerRoughSeg);

		const Vector3 roughPathNodePosA = m_roughPathPolyLine[roughPathNodeIndexA].m_first;
		const Vector3 roughPathNodePosB = m_roughPathPolyLine[roughPathNodeIndexB].m_first;
		const Vector3 roughPathNodeDeltaAB = roughPathNodePosB - roughPathNodePosA;
		Assert(roughPathNodeDeltaAB.Mag() > 0.0f);

		const Vector3 pos(roughPathNodePosA + (roughPathNodeDeltaAB * lerpFactorAB));
		Assert(Vec3IsValid(pos));

		return pos;
	}
}


// This is the main function that causes the racing line to smooth out.  It
// should be called repeatedly with the game update loop.
// In an actual game it would do the smoothing all in one function call when
// the path is set up.
void RacingLineOptimizing::RaceLineUpdate(void) 
{
	// Update the positions of the points in the racing line
	// approximation.

	// This function computes the forces that need to be applied to
	// the points in the racing line in order to improve the
	// approximation.
	UpdateForces();

	// Update the velocities of the masses in the racing line given
	// the forces applied to them.
	UpdateVelocities();

	// Update the positions of the masses in the racing line given
	// their velocities.
	UpdatePositions();
}


// Calculate all the forces that will operate on the racing line masses.
void RacingLineOptimizing::UpdateForces(void)
{
	// reset all forces to get ready to calculate new ones.
	const int massCount = m_masses.size();
	for(int i = 0; i < massCount; ++i)
	{
		m_masses[i].m_force.Set(0.0f, 0.0f);
	}

	// Calculate the restoring forces that the springs apply to the
	// masses in the racing line. These forces help to keep the masses
	// evenly distributed along the initialLength of the racing line.
	TUNE_GROUP_BOOL(PATH_RACING_LINE, doLengthAdjustment, true);
	if(doLengthAdjustment)
	{
		const int springCount = m_springs.size();
		for(int i = 0; i < springCount; ++i)
		{
			CRacingLineSpring& spring = m_springs[i];
			const int springMassIndex0 = spring.m_massIndex0;
#if __ASSERT
			const int springMassIndex1 = spring.m_massIndex1;
#endif // __ASSERT
			Assert((springMassIndex0 >= 0) && (springMassIndex0 < m_masses.size()));
			Assert((springMassIndex1 >= 0) && (springMassIndex1 < m_masses.size()));
			TDequeNode<CRacingLineMass>* pSpringMass0Node = m_masses.GetNode(springMassIndex0);
			CRacingLineMass& springMass0 = pSpringMass0Node->m_data;//m_masses[springMassIndex0];
			Assert(Vec3IsValid(springMass0.m_pos));
			TDequeNode<CRacingLineMass>* pSpringMass1Node = pSpringMass0Node->m_pNext;
			CRacingLineMass& springMass1 = pSpringMass1Node->m_data;//m_masses[springMassIndex1];
			Assert(Vec3IsValid(springMass1.m_pos));

			const Vector2	mass0InitialPos(springMass0.m_pos, Vector2::kXY);
			const Vector2	mass1InitialPos(springMass1.m_pos, Vector2::kXY);
			const Vector2	springDelta0to1			= mass1InitialPos - mass0InitialPos;
			Assert(Vec2IsValid(springDelta0to1));
			Vector2			springDir				= springDelta0to1;
			springDir.Normalize();
			Assert(Vec2IsValid(springDir));
			Assert(springDir.Mag() > 0.99f);
			const float		lengthRestoringForceScale	= spring.m_stiffness * ((springDelta0to1.Mag() - spring.m_restLength) / spring.m_restLength);
			const Vector2	lengthRestoringForce		= springDir * lengthRestoringForceScale;

			if(i != 0)
			{
				springMass0.m_force += lengthRestoringForce;
			}
			if(i != springCount - 1)
			{
				springMass1.m_force -= lengthRestoringForce;
			}

			Assert(Vec2IsValid(springMass0.m_force));
			Assert(Vec2IsValid(springMass1.m_force));
		}
	}
}


// Calculate all the velocities that will operate on the racing line masses.
void RacingLineOptimizing::UpdateVelocities(void)
{
	// Update the velocities of the masses in the racing line according
	// to the forces applied to them.

	// Scale the forces according to the largest of them to produce a
	// good trade of force between speed and stability.
	float largestForce = 0.0f;
	const int massCount = m_masses.size();
	for(int i = 0; i < massCount; ++i)
	{
		const float forceMag = m_masses[i].m_force.Mag();
		if(forceMag > largestForce)
		{
			largestForce = forceMag;
		}
	}

	// Update the velocities according to the scaled forces.
	for(int i = 0; i < massCount; ++i)
	{
		const float forceScale = (0.1f * largestForce) / (1.0f + m_masses[i].m_mass);
		const Vector2 scaledForce = (m_masses[i].m_force * forceScale);
		m_masses[i].m_vel += scaledForce;
		Assert(Vec2IsValid(m_masses[i].m_vel));
	}

	// Reduce the velocities a little. This helps to keep the racing line
	// approximation stable.
	TUNE_GROUP_FLOAT(PATH_RACING_LINE, nodeVelocityDamping, 0.99f, 0.0f, 1.0f, 0.001f);
	for(int i = 0; i < massCount; ++i)
	{
		m_masses[i].m_vel *= nodeVelocityDamping;
		Assert(Vec2IsValid(m_masses[i].m_vel));
	}
}


// Calculate all the new positions for the racing line masses.
void RacingLineOptimizing::UpdatePositions(void)
{
	// Update the positions of the masses that form the racing line
	// according to their velocities, making sure that they stay
	// within the bounds of the track.

	// This loop does not process the position of the first or last point
	// of the racing line and hence creates a racing line that always starts
	// and ends at the centre of the track.
	const int massCount = m_masses.size();
	for(int i = 1; i < massCount - 1; ++i)
	{
		const Vector3 currMassOldPos = m_masses[i].m_pos;
		const Vector3 currMassNewPos = currMassOldPos + Vector3(m_masses[i].m_vel.x, m_masses[i].m_vel.y, 0.0f);
		Vector3 earliestCollisionPos(currMassOldPos);
		const bool massCollided = CheckIfSegmentCollidesWithBounds(currMassOldPos, currMassNewPos, earliestCollisionPos);
		const Vector3 prevMassNewPos = m_masses[i - 1].m_pos;
		const bool segmentPrevCollided = CheckIfSegmentCollidesWithBounds(prevMassNewPos, currMassNewPos, earliestCollisionPos);
		const Vector3 nextMassOldPos = m_masses[i + 1].m_pos;
		const bool segmentNextCollided = CheckIfSegmentCollidesWithBounds(currMassNewPos, nextMassOldPos, earliestCollisionPos);
		if(massCollided && !segmentPrevCollided && !segmentNextCollided)
		{
			m_masses[i].m_pos = earliestCollisionPos;
			m_masses[i].m_vel.Zero();
		}
		else if(segmentPrevCollided || segmentNextCollided)
		{
			m_masses[i].m_vel.Zero();
			m_masses[i].m_pos = currMassOldPos;
		}
		else
		{
			m_masses[i].m_pos = currMassNewPos;
		}
		Assert(Vec3IsValid(m_masses[i].m_pos));
	}
}


bool RacingLineOptimizing::CheckIfSegmentCollidesWithBounds(const Vector3& oldPos, const Vector3& newPos, Vector3& collisionPosEarliest_out)
{
	bool	collidedAnySegment = false;
	float	collisionMovePortionEarliest = 0.0f;
	Vector3	collisionPosEarliest(oldPos);

	const int boundPolyLineCount = m_boundPolyLines.GetCount();
	for(int i = 0; i < boundPolyLineCount; ++i)
	{	
		for(int j = 0; j < m_boundPolyLines[i].m_first.size() - 1; ++j)
		{
			Vector3 boundSegA(m_boundPolyLines[i].m_first[j].m_first);
			Vector3 boundSegB(m_boundPolyLines[i].m_first[j + 1].m_first);

			float collisionMovePortion = 0.0f;
			Vector3 collisionPos(oldPos);
			const IntersectResult result = Test2DSegmentSegment(oldPos, newPos, boundSegA, boundSegB, collisionMovePortion, collisionPos);
			if(result == COINCIDENT || result == INTERESECTING)
			{
				collidedAnySegment = true;
				if(collisionMovePortion < collisionMovePortionEarliest)
				{
					collisionMovePortionEarliest = collisionMovePortion;
					collisionPosEarliest = collisionPos;
				}
			}
		}
	}

	if(collidedAnySegment)
	{
		collisionPosEarliest_out = collisionPosEarliest;
		return true;
	}
	else
	{
		return false;
	}
}


// Implementation of the theory provided by Paul Bourke.
RacingLineOptimizing::IntersectResult RacingLineOptimizing::Test2DSegmentSegment(const Vector3& seg0A, const Vector3& seg0B, const Vector3& seg1A, const Vector3& seg1B, float& seg0Portion_out, Vector3& collisionPos_out)
{
	const float denominator =	(((seg1B.y - seg1A.y)*(seg0B.x - seg0A.x)) - ((seg1B.x - seg1A.x)*(seg0B.y - seg0A.y)));
	const float numeratorA =	(((seg1B.x - seg1A.x)*(seg0A.y - seg1A.y)) - ((seg1B.y - seg1A.y)*(seg0A.x - seg1A.x)));
	const float numeratorB =	(((seg0B.x - seg0A.x)*(seg0A.y - seg1A.y)) - ((seg0B.y - seg0A.y)*(seg0A.x - seg1A.x)));
	if(denominator == 0.0f)
	{
		if(numeratorA == 0.0f && numeratorB == 0.0f)
		{
			return RacingLineOptimizing::COINCIDENT;
		}
		return RacingLineOptimizing::PARALLEL;
	}

	const float seg0Portion = numeratorA / denominator;
	const float seg1Portion = numeratorB / denominator;
	if(seg0Portion >= 0.0f && seg0Portion <= 1.0f && seg1Portion >= 0.0f && seg1Portion <= 1.0f)
	{
		// Get the intersection point.
		seg0Portion_out = seg0Portion;
		collisionPos_out.x = seg0A.x + seg0Portion_out*(seg0B.x - seg0A.x);
		collisionPos_out.y = seg0A.y + seg0Portion_out*(seg0B.y - seg0A.y);
		collisionPos_out.y = seg0A.z + seg0Portion_out*(seg0B.z - seg0A.z);

		return RacingLineOptimizing::INTERESECTING;
	}

	return RacingLineOptimizing::NOT_INTERESECTING;
}