#include "TaskNavBase.h"

// Rage headers
#include "Math/AngMath.h"

// Game headers
#if !__FINAL
#include "Camera/CamInterface.h"
#include "Camera/Debug/DebugDirector.h"
#include "Camera/Helpers/Frame.h"
#endif
#include "Event/EventMovement.h"
#include "Peds/Ped.h"
#include "Peds/PedCapsule.h"
#include "Peds/PedDebugVisualiser.h"
#include "Peds/PedIntelligence.h"
#include "Peds/PedMoveBlend/PedMoveBlendInWater.h"
#include "Peds/PedMoveBlend/PedMoveBlendOnFoot.h"
#include "Renderer/Water.h"
#include "Task/Motion/Locomotion/TaskHumanLocomotion.h"
#include "Task/Movement/TaskGetUp.h"
#include "Task/Movement/TaskGoTo.h"
#include "Task/Movement/TaskGoToPoint.h"
#include "Task/Movement/TaskSlideToCoord.h"
#include "Task/Movement/Climbing/TaskLadderClimb.h"
#include "Task/Movement/Jumping/TaskInAir.h"
#include "Task/General/TaskBasic.h"
#include "Task/Motion/Locomotion/TaskMotionPed.h"
#include "Task/Motion/Locomotion/TaskQuadLocomotion.h"
#include "Task/Scenario/Types/TaskUseScenario.h"
#include "Task/Vehicle/TaskCarUtils.h"
#include "Task/Vehicle/TaskExitVehicle.h"
#include "Task/Vehicle/TaskGoToVehicleDoor.h"

FW_INSTANTIATE_CLASS_POOL_SPILLOVER(CNavMeshRoute, CONFIGURED_FROM_FILE, 0.45f, atHashString("NavMeshRoute",0xf66ca574));

AI_NAVIGATION_OPTIMISATIONS()
AI_OPTIMISATIONS()

// Always use a look-ahead along the route (not just for spline sections)
#define __USE_LOOKAHEAD_ON_ROUTE						1
// Allows walkstops to operate over multiple linear segments (otherwise will only occur if the last segment has enough length)
#define __MAY_WALKSTOP_OVER_MULTIPLE_ROUTE_SEGMENTS		1
// Creates a follow point route task with the upcoming navmesh route, for use when moving whilst waiting for a path
#define __USE_POINTROUTE_WHEN_WAITING_FOR_PATH			0
// This will allow the navigation tasks to request a new route whilst continuing to follow their current route until the new one is ready
#define __REQUEST_ROUTE_WHILST_FOLLOWING_PATH			1
// This enables a more optimized version of CNavMeshRoute::ExpandRoute()
#define __USE_OPTIMIZED_EXPAND_ROUTE					1
// Attempt to use the CTaskDropDown and dropdown detector, instead of just walking off drop edges
#define __USE_DROPDOWN_TASK								0

static const float g_fFixupRunHeadingRadiusSqr = 0.5f * 0.5f;

// NAME: ExpandRoute
// PURPOSE : Dump out route as a series of linear segments, returning the number of points generated
// Using this format simplifies some operations, but allows us to use a compacted representation to store curves.
// NB: pRoutePoints must point to an array of MAX_NUM_ROUTE_ELEMENTS * RUNTIME_PATH_SPLINE_MAX_SEGMENTS in size
// NB: pRouteLength is an optional parameter to retrieve the length of the entire route

#if !__USE_OPTIMIZED_EXPAND_ROUTE

int CNavMeshRoute::ExpandRoute(CNavMeshExpandedRouteVert * RESTRICT pRoutePoints, float * RESTRICT pRouteLength, int startSegmentIndex, int stopSegmentIndex, const float& distanceToExpand) const
{
	if(pRouteLength)
		*pRouteLength = 0.0f;

	if(!m_iRouteSize)
	{
		return 0;
	}

	if(startSegmentIndex >= m_iRouteSize || stopSegmentIndex <= startSegmentIndex)
	{
		return 0;
	}

	//
	// if we're on a spline then we need to expand the previous point
	//
	static bool s_EnableSplineCheck = true;
	if ( s_EnableSplineCheck && GetSplineEnum(startSegmentIndex) != -1 )
	{
		startSegmentIndex = Max(0, startSegmentIndex-1);
	}

	pRoutePoints[0].SetPosAndSplineT(RCC_VEC3V(m_routePoints[startSegmentIndex]), ScalarV(V_ZERO));
	pRoutePoints[0].m_iProgress = startSegmentIndex;
	pRoutePoints[0].m_iWptFlag = m_routeWayPoints[startSegmentIndex];

	Vector3 vLast, vCurrent, vNext, vLastToCurrent, vCurrentToNext, vCtrlPt1, vCtrlPt2, vCtrlPt3, vCtrlPt4;
	float fCtrlPt2TVal, t;
	int iNum = 1;
	int i, s;

	for(i=startSegmentIndex+1; i<stopSegmentIndex; i++)
	{
		if(m_routeSplineEnums[i]==-1)
		{
			pRoutePoints[iNum].SetPosAndSplineT(RCC_VEC3V(m_routePoints[i]), ScalarV(V_ZERO));
			pRoutePoints[iNum].m_iProgress = i;
			pRoutePoints[iNum].m_iWptFlag = m_routeWayPoints[i];

			if(pRouteLength)
				*pRouteLength += (VEC3V_TO_VECTOR3(pRoutePoints[iNum].GetPos()) - VEC3V_TO_VECTOR3(pRoutePoints[iNum-1].GetPos())).Mag();

			iNum++;
			Assert(iNum <= CTaskNavBase::ms_iMaxExpandedVerts);
		}
		else
		{
			vLast = m_routePoints[i-1];
			vCurrent = m_routePoints[i];
			vNext = m_routePoints[i+1];
			vLastToCurrent = vCurrent - vLast;
			vCurrentToNext = vNext - vCurrent;

			fCtrlPt2TVal = GetSplineCtrlPt2TVal(i);
			CalcPathSplineCtrlPts(m_routeSplineEnums[i], fCtrlPt2TVal, vLast, vNext, vLastToCurrent, vCurrentToNext, vCtrlPt1, vCtrlPt2, vCtrlPt3, vCtrlPt4);

			t = 0.0f;
			s = RUNTIME_PATH_SPLINE_MAX_SEGMENTS;
			while(s >= 0)
			{
				pRoutePoints[iNum].m_vPosAndSplineT.SetIntrin128(VECTOR3_TO_VEC3V(PathSpline(vCtrlPt1, vCtrlPt2, vCtrlPt3, vCtrlPt4, t)).GetIntrin128());
				pRoutePoints[iNum].m_iProgress = (t < 0.5) ? i-1 : i;
				pRoutePoints[iNum].m_vPosAndSplineT.SetWf(t);
				pRoutePoints[iNum].m_iWptFlag.Clear();

				if(pRouteLength)
					*pRouteLength += (VEC3V_TO_VECTOR3(pRoutePoints[iNum].GetPos()) - VEC3V_TO_VECTOR3(pRoutePoints[iNum-1].GetPos())).Mag();

				iNum++;
				Assert(iNum <= CTaskNavBase::ms_iMaxExpandedVerts);

				t += RUNTIME_PATH_SPLINE_STEPVAL;
				s--;
			}
		}

		// stop if we have expanded past our desired distance 
		if(pRouteLength)
		{
			if ( *pRouteLength >= distanceToExpand )
			{
				break;
			}
		}
	}
	Assert(iNum <= CTaskNavBase::ms_iMaxExpandedVerts);
	return iNum;
}

#else	// !__USE_OPTIMIZED_EXPAND_ROUTE

int CNavMeshRoute::ExpandRoute(CNavMeshExpandedRouteVert * RESTRICT pRoutePoints, float * RESTRICT pRouteLength, int startSegmentIndex, int stopSegmentIndex, const float& distanceToExpand) const
{
	if(!m_iRouteSize
		|| startSegmentIndex >= m_iRouteSize
		|| stopSegmentIndex <= startSegmentIndex)
	{
		if(pRouteLength)
		{
			*pRouteLength = 0.0f;
		}

		return 0;
	}

	const bool shouldComputeDistances = (pRouteLength != NULL);	// || distanceToExpand < 1000.0f ??

	const ScalarV distanceToExpandV = LoadScalar32IntoScalarV(distanceToExpand);

	//
	// if we're on a spline then we need to expand the previous point
	//
	if (GetSplineEnum(startSegmentIndex) != -1 )
	{
		startSegmentIndex = Max(0, startSegmentIndex-1);
	}

	Vec3V prevExpandedV = VECTOR3_TO_VEC3V(m_routePoints[startSegmentIndex]);
	pRoutePoints[0].SetPosAndSplineT(prevExpandedV, ScalarV(V_ZERO));
	pRoutePoints[0].m_iProgress = startSegmentIndex;
	pRoutePoints[0].m_iWptFlag = m_routeWayPoints[startSegmentIndex];

	int iNum = 1;

	ScalarV routeLengthV(V_ZERO);

	for(int i=startSegmentIndex+1; i<stopSegmentIndex; i++)
	{
		if(m_routeSplineEnums[i]==-1)
		{
			const Vec3V currentExpandedV = VECTOR3_TO_VEC3V(m_routePoints[i]);
			pRoutePoints[iNum].SetPosAndSplineT(currentExpandedV, ScalarV(V_ZERO));
			pRoutePoints[iNum].m_iProgress = i;
			pRoutePoints[iNum].m_iWptFlag = m_routeWayPoints[i];

			if(shouldComputeDistances)
			{
				// Could go back to Dist() if DistFast() isn't accurate enough:
				//	routeLengthV = rage::Add(routeLengthV, Dist(currentExpandedV, prevExpandedV));
				routeLengthV = rage::Add(routeLengthV, DistFast(currentExpandedV, prevExpandedV));
			}

			prevExpandedV = currentExpandedV;

			iNum++;
			Assert(iNum <= CTaskNavBase::ms_iMaxExpandedVerts);
		}
		else
		{
			// Grab the route points we will build the spline for.
			const Vec3V vLast = RCC_VEC3V(m_routePoints[i-1]);
			const Vec3V vCurrent = RCC_VEC3V(m_routePoints[i]);
			const Vec3V vNext = RCC_VEC3V(m_routePoints[i+1]);
			const Vec3V vLastToCurrent = Subtract(vCurrent, vLast);
			const Vec3V vCurrentToNext = Subtract(vNext, vCurrent);

			// Compute the control points of the spline.
			const ScalarV ctrlPt2TValV = GetSplineCtrlPt2TValV(i);
			Vec3V ctrlPt0V, ctrlPt1V, ctrlPt2V, ctrlPt3V;
			CalcPathSplineCtrlPtsV(m_routeSplineEnums[i], ctrlPt2TValV, vLast, vNext, vLastToCurrent, vCurrentToNext, ctrlPt0V, ctrlPt1V, ctrlPt2V, ctrlPt3V);

			// A lot of what follows is assuming that RUNTIME_PATH_SPLINE_MAX_SEGMENTS is 8, so we can
			// fit the coordinates, T values, etc, of eight points in two 4D vectors. Extending it to
			// 12 or 16 should be fairly straightforward, but then we may want to de-unroll the code
			// and set it up as a loop. Using something that's not an even 4 would be slightly trickier.
			CompileTimeAssert(RUNTIME_PATH_SPLINE_MAX_SEGMENTS == 8);

			// We will keep T values of four points in a vector. These are the first four:
			const Vec4V tV(Vec::V4VConstant<
					FLOAT_TO_INT(0.0f/8.0f),	// 8.0f == RUNTIME_PATH_SPLINE_MAX_SEGMENTS
					FLOAT_TO_INT(1.0f/8.0f),
					FLOAT_TO_INT(2.0f/8.0f),
					FLOAT_TO_INT(3.0f/8.0f)
					>());

			// This is the amount that the T values increase by for the next four points.
			// const Vec4V tStepV(ScalarV(4.0f/(float)RUNTIME_PATH_SPLINE_MAX_SEGMENTS));
			const Vec4V tStepV(V_HALF);

			// Compute the square and cube of all T values for eight points.
			const Vec4V tAV = tV;
			const Vec4V t2AV = Scale(tAV, tAV);
			const Vec4V t3AV = Scale(t2AV, tAV);
			const Vec4V tBV = rage::Add(tV, tStepV);
			const Vec4V t2BV = Scale(tBV, tBV);
			const Vec4V t3BV = Scale(t2BV, tBV);

			// Next, we will compute coefficients for computing the spline points
			// from control points. This was adapted from PathSpline():
			//	const Vector3 vTargetAfterSlide =
			//		vCtrlPt1 * (-0.5f*u*u*u + u*u - 0.5f*u) +
			//		vCtrlPt2 * (1.5f*u*u*u + -2.5f*u*u + 1.0f) +
			//		vCtrlPt3 * (-1.5f*u*u*u + 2.0f*u*u + 0.5f*u) +
			//		vCtrlPt4 * (0.5f*u*u*u - 0.5f*u*u);
			// through something like this:
			//		const Vec4V k0V = ScalarV(-0.5f)*t3V + t2V - ScalarV(0.5f)*tV;
			//		const Vec4V k1V = ScalarV(1.5f)*t3V - ScalarV(2.5f)*t2V + Vec4V(ScalarV(1.0f));
			//		const Vec4V k2V = ScalarV(-1.5f)*t3V + ScalarV(2.0f)*t2V + ScalarV(0.5f)*tV;
			//		const Vec4V k3V = ScalarV(0.5f)*t3V - ScalarV(0.5f)*t2V;

			// First, we will need some constants:
			const ScalarV halfV(V_HALF);
			const ScalarV oneV(V_ONE);
			const ScalarV twoV(V_TWO);
			const ScalarV negOnePointFiveV = Subtract(halfV, twoV);
			const ScalarV twoPointFiveV = rage::Add(twoV, halfV);

			// Compute the coefficients to use for the eight points on the spline,
			// for the four control points. k0V are the coefficients for four spline points
			// for control point 0, etc.

			const Vec4V k0V = SubtractScaled(SubtractScaled(t2AV, tAV, halfV), t3AV, halfV);
			const Vec4V k1V = SubtractScaled(SubtractScaled(Vec4V(oneV), t2AV, twoPointFiveV), t3AV, negOnePointFiveV);
			const Vec4V k2V = AddScaled(AddScaled(Scale(tAV, halfV), t2AV, twoV), t3AV, negOnePointFiveV);
			const Vec4V k3V = SubtractScaled(Scale(t3AV, halfV), t2AV, halfV);

			const Vec4V k4V = SubtractScaled(SubtractScaled(t2BV, tBV, halfV), t3BV, halfV);
			const Vec4V k5V = SubtractScaled(SubtractScaled(Vec4V(oneV), t2BV, twoPointFiveV), t3BV, negOnePointFiveV);
			const Vec4V k6V = AddScaled(AddScaled(Scale(tBV, halfV), t2BV, twoV), t3BV, negOnePointFiveV);
			const Vec4V k7V = SubtractScaled(Scale(t3BV, halfV), t2BV, halfV);

			// Multiply control point 0 for all the points.
			Vec4V xAV = Scale(k0V, ctrlPt0V.GetX());
			Vec4V yAV = Scale(k0V, ctrlPt0V.GetY());
			Vec4V zAV = Scale(k0V, ctrlPt0V.GetZ());
			Vec4V xBV = Scale(k4V, ctrlPt0V.GetX());
			Vec4V yBV = Scale(k4V, ctrlPt0V.GetY());
			Vec4V zBV = Scale(k4V, ctrlPt0V.GetZ());

			// Multiply control point 1 for all the points.
			xAV = AddScaled(xAV, k1V, ctrlPt1V.GetX());
			yAV = AddScaled(yAV, k1V, ctrlPt1V.GetY());
			zAV = AddScaled(zAV, k1V, ctrlPt1V.GetZ());
			xBV = AddScaled(xBV, k5V, ctrlPt1V.GetX());
			yBV = AddScaled(yBV, k5V, ctrlPt1V.GetY());
			zBV = AddScaled(zBV, k5V, ctrlPt1V.GetZ());

			// Multiply control point 2 for all the points.
			xAV = AddScaled(xAV, k2V, ctrlPt2V.GetX());
			yAV = AddScaled(yAV, k2V, ctrlPt2V.GetY());
			zAV = AddScaled(zAV, k2V, ctrlPt2V.GetZ());
			xBV = AddScaled(xBV, k6V, ctrlPt2V.GetX());
			yBV = AddScaled(yBV, k6V, ctrlPt2V.GetY());
			zBV = AddScaled(zBV, k6V, ctrlPt2V.GetZ());

			// Multiply control point 3 for all the points.
			xAV = AddScaled(xAV, k3V, ctrlPt3V.GetX());
			yAV = AddScaled(yAV, k3V, ctrlPt3V.GetY());
			zAV = AddScaled(zAV, k3V, ctrlPt3V.GetZ());
			xBV = AddScaled(xBV, k7V, ctrlPt3V.GetX());
			yBV = AddScaled(yBV, k7V, ctrlPt3V.GetY());
			zBV = AddScaled(zBV, k7V, ctrlPt3V.GetZ());

			// As at least in some cases, we will need distances along the path,
			// we will compute those here as well.

			// First, compute X, Y, and Z vectors shifted over by one point.
			// For example, in shiftedYAV, the first component is the Y coordinate of the previous
			// point (before the spline), the second component is the Y coordinate of the first point
			// on the spline, and so on.
			const Vec4V shiftedXAV = GetFromTwo<Vec::W1, Vec::X2, Vec::Y2, Vec::Z2>(Vec4V(prevExpandedV.GetX()), xAV);
			const Vec4V shiftedYAV = GetFromTwo<Vec::W1, Vec::X2, Vec::Y2, Vec::Z2>(Vec4V(prevExpandedV.GetY()), yAV);
			const Vec4V shiftedZAV = GetFromTwo<Vec::W1, Vec::X2, Vec::Y2, Vec::Z2>(Vec4V(prevExpandedV.GetZ()), zAV);
			const Vec4V shiftedXBV = GetFromTwo<Vec::W1, Vec::X2, Vec::Y2, Vec::Z2>(xAV, xBV);
			const Vec4V shiftedYBV = GetFromTwo<Vec::W1, Vec::X2, Vec::Y2, Vec::Z2>(yAV, yBV);
			const Vec4V shiftedZBV = GetFromTwo<Vec::W1, Vec::X2, Vec::Y2, Vec::Z2>(zAV, zBV);

			// Compute the deltas between these points and the previous points, in X, Y, and Z.
			const Vec4V deltaXAV = Subtract(xAV, shiftedXAV);
			const Vec4V deltaYAV = Subtract(yAV, shiftedYAV);
			const Vec4V deltaZAV = Subtract(zAV, shiftedZAV);
			const Vec4V deltaXBV = Subtract(xBV, shiftedXBV);
			const Vec4V deltaYBV = Subtract(yBV, shiftedYBV);
			const Vec4V deltaZBV = Subtract(zBV, shiftedZBV);

			// Compute the squared sum of the deltas.
			const Vec4V deltaXSqAV = Scale(deltaXAV, deltaXAV);
			const Vec4V deltaXSqBV = Scale(deltaXBV, deltaXBV);
			const Vec4V deltaXYSqAV = AddScaled(deltaXSqAV, deltaYAV, deltaYAV);
			const Vec4V deltaXYSqBV = AddScaled(deltaXSqBV, deltaYBV, deltaYBV);
			const Vec4V deltaSqAV = AddScaled(deltaXYSqAV, deltaZAV, deltaZAV);
			const Vec4V deltaSqBV = AddScaled(deltaXYSqBV, deltaZBV, deltaZBV);

			// Compute the square roots of the sum of squares, to get the distance.
			// Note: could go for Sqrt() rather than SqrtFast() if the latter is not accurate enough:
			//	const Vec4V distancesAV = Sqrt(deltaSqAV);
			//	const Vec4V distancesBV = Sqrt(deltaSqBV);
			const Vec4V distancesAV = SqrtFast(deltaSqAV);
			const Vec4V distancesBV = SqrtFast(deltaSqBV);

			// Add together the distances for the two vectors. We still need
			// to sum up the components, but we do that later.
			const Vec4V distSumFourV = rage::Add(distancesAV, distancesBV);

			// Next, we have to transpose the points from SoA to AoS format, similar to how
			// it would be done for a matrix multiplication.
			const Vec4V temp0AV = MergeXY(xAV, zAV);
			const Vec4V temp1AV = MergeXY(yAV, tAV);
			const Vec4V temp2AV = MergeZW(xAV, zAV);
			const Vec4V temp3AV = MergeZW(yAV, tAV);
			const Vec4V temp0BV = MergeXY(xBV, zBV);
			const Vec4V temp1BV = MergeXY(yBV, tBV);
			const Vec4V temp2BV = MergeZW(xBV, zBV);
			const Vec4V temp3BV = MergeZW(yBV, tBV);
			const Vec4V xyzt0V = MergeXY(temp0AV, temp1AV);
			const Vec4V xyzt1V = MergeZW(temp0AV, temp1AV);
			const Vec4V xyzt2V = MergeXY(temp2AV, temp3AV);
			const Vec4V xyzt3V = MergeZW(temp2AV, temp3AV);
			const Vec4V xyzt4V = MergeXY(temp0BV, temp1BV);
			const Vec4V xyzt5V = MergeZW(temp0BV, temp1BV);
			const Vec4V xyzt6V = MergeXY(temp2BV, temp3BV);
			const Vec4V xyzt7V = MergeZW(temp2BV, temp3BV);

			// Store the points:
			pRoutePoints[iNum    ].SetPosAndSplineT(xyzt0V);
			pRoutePoints[iNum + 1].SetPosAndSplineT(xyzt1V);
			pRoutePoints[iNum + 2].SetPosAndSplineT(xyzt2V);
			pRoutePoints[iNum + 3].SetPosAndSplineT(xyzt3V);
			pRoutePoints[iNum + 4].SetPosAndSplineT(xyzt4V);
			pRoutePoints[iNum + 5].SetPosAndSplineT(xyzt5V);
			pRoutePoints[iNum + 6].SetPosAndSplineT(xyzt6V);
			pRoutePoints[iNum + 7].SetPosAndSplineT(xyzt7V);

			// We actually need to add 9 points if RUNTIME_PATH_SPLINE_MAX_SEGMENTS is 8.
			// The last point is the same as the third control point, so this is easy:
			pRoutePoints[iNum + 8].SetPosAndSplineT(ctrlPt2V, ScalarV(V_ONE));

			// Keep track of the last point, for measuring the distance for the next segment.
			prevExpandedV = ctrlPt2V;

			// Compute the distance for the last segment we added. Unfortunately this is somewhat
			// costly since the square root here is for just one segment, not for like what we do
			// for the rest of the spline.
			if(shouldComputeDistances)
			{
				const Vec4V distTemp1V = distSumFourV.Get<Vec::Z, Vec::W, Vec::X, Vec::Y>();
				const Vec4V distTemp2V = rage::Add(distSumFourV, distTemp1V);					// x+z y+w z+x w+y
				const Vec4V distTemp3V = distTemp2V.Get<Vec::Y, Vec::Z, Vec::W, Vec::X>();		// y+w z+x w+y x+z
				ScalarV distSumV = rage::Add(distTemp2V, distTemp3V).GetX();
				distSumV = rage::Add(distSumV, DistFast(ctrlPt2V, xyzt7V.GetXYZ()));		// Note: using DistFast() as an approximation of Dist().
				routeLengthV = rage::Add(routeLengthV, distSumV);
			}

			// Set the progress of the points: this is equivalent to t < 0.5.
			const int prevI = i - 1;
			pRoutePoints[iNum    ].m_iProgress = prevI;
			pRoutePoints[iNum + 1].m_iProgress = prevI;
			pRoutePoints[iNum + 2].m_iProgress = prevI;
			pRoutePoints[iNum + 3].m_iProgress = prevI;
			pRoutePoints[iNum + 4].m_iProgress = i;
			pRoutePoints[iNum + 5].m_iProgress = i;
			pRoutePoints[iNum + 6].m_iProgress = i;
			pRoutePoints[iNum + 7].m_iProgress = i;
			pRoutePoints[iNum + 8].m_iProgress = i;

			// Clear the flags. Note: we could probably do this faster if we made some assumptions
			// about us always wanting everything to be zero, etc.
			pRoutePoints[iNum    ].m_iWptFlag.Clear();
			pRoutePoints[iNum + 1].m_iWptFlag.Clear();
			pRoutePoints[iNum + 2].m_iWptFlag.Clear();
			pRoutePoints[iNum + 3].m_iWptFlag.Clear();
			pRoutePoints[iNum + 4].m_iWptFlag.Clear();
			pRoutePoints[iNum + 5].m_iWptFlag.Clear();
			pRoutePoints[iNum + 6].m_iWptFlag.Clear();
			pRoutePoints[iNum + 7].m_iWptFlag.Clear();
			pRoutePoints[iNum + 8].m_iWptFlag.Clear();

			iNum += RUNTIME_PATH_SPLINE_MAX_SEGMENTS + 1;
			Assert(iNum <= CTaskNavBase::ms_iMaxExpandedVerts);
		}

		// stop if we have expanded past our desired distance 
		// Note: there is potentially a bit of a bug here: it seems like calling
		// code expects us to break out even if we pass in NULL as pRouteLength.
		// If we want to change the behavior of the code to work like that,
		// we will need to make sure we compute the distances everywhere, since we
		// currently skip some of the math in that case.
		if(shouldComputeDistances)
		{
			if(IsGreaterThanAll(routeLengthV, distanceToExpandV))
			{
				break;
			}
		}
	}

	if(pRouteLength)
	{
		StoreScalar32FromScalarV(*pRouteLength, routeLengthV);
	}

	Assert(iNum <= CTaskNavBase::ms_iMaxExpandedVerts);
	return iNum;
}

#endif	// !__USE_OPTIMIZED_EXPAND_ROUTE

bool CNavMeshRoute::GetPositionAheadOnSpline(const Vector3 & vPedPos, const int iInitialProgress, float fInitialSplineTVal, const float fInitialDistanceAhead, Vector3 & vOutPos) const
{
	Vector3 vSegmentStart = vPedPos;
	int iProgress = iInitialProgress;
	float fSplineTVal = fInitialSplineTVal;
	float fDistanceAhead = fInitialDistanceAhead;

	while(fDistanceAhead >= 0.0f && iProgress < m_iRouteSize)
	{
		const int iSplineEnum = (iProgress < m_iRouteSize) ? GetSplineEnum(iProgress) : -1;
		if(iSplineEnum == -1)
		{
			Vector3 vDiff = m_routePoints[iProgress] - vSegmentStart;
			const float fMag2 = vDiff.Mag2();

			if( fMag2 > fDistanceAhead*fDistanceAhead )
			{
				vDiff.Normalize();
				vOutPos = vSegmentStart + (vDiff * fDistanceAhead);
				return true;
			}

			fDistanceAhead -= Sqrtf(fMag2);

			vSegmentStart = m_routePoints[iProgress];
			iProgress++;
			fSplineTVal = RUNTIME_PATH_SPLINE_STEPVAL;
			fInitialSplineTVal = 0.0f;
		}
		else
		{
			const Vector3 & vLast = GetPosition(iProgress-1);
			const Vector3 & vCurrent = GetPosition(iProgress);
			const Vector3 & vNext = GetPosition(iProgress+1);
			const Vector3 vLastToCurrent = vCurrent - vLast;
			const Vector3 vCurrentToNext = vNext - vCurrent;

			const float fCtrlPt2TVal = GetSplineCtrlPt2TVal(iProgress);
			Vector3 vCtrlPt1, vCtrlPt2, vCtrlPt3, vCtrlPt4;
			CalcPathSplineCtrlPts(iSplineEnum, fCtrlPt2TVal, vLast, vNext, vLastToCurrent, vCurrentToNext, vCtrlPt1, vCtrlPt2, vCtrlPt3, vCtrlPt4);

			// Check for position along straight segment before start of spline
			if(fInitialSplineTVal == 0.0f)
			{
				const Vector3 vSegStartToCtrlPt1 = vCtrlPt1 - vSegmentStart;
				Vector3 vDiff = vCtrlPt2 - vSegmentStart;
				const float fMag2 = vDiff.Mag2();
				vDiff.Normalize();
				if(DotProduct(vDiff, vSegStartToCtrlPt1) < 0.0f)
				{
					if( fMag2 > fDistanceAhead*fDistanceAhead )
					{
						vOutPos = vSegmentStart + (vDiff * fDistanceAhead);
						return true;
					}
				}

				vSegmentStart = vCtrlPt2;
				fDistanceAhead -= Sqrtf(fMag2);
				fSplineTVal = RUNTIME_PATH_SPLINE_STEPVAL;
				fInitialSplineTVal = 0.0f;
			}

			if(fInitialSplineTVal == 0.0f)
				vSegmentStart = vCtrlPt2;

			while(fSplineTVal < 1.0f)
			{
				const Vector3 vSplinePt = PathSpline(vCtrlPt1, vCtrlPt2, vCtrlPt3, vCtrlPt4, fSplineTVal);
				Vector3 vDiff = vSplinePt - vSegmentStart;
				const float fMag2 = vDiff.Mag2();

				if( fMag2 > fDistanceAhead*fDistanceAhead )
				{
					vDiff.Normalize();
					vOutPos = vSegmentStart + (vDiff * fDistanceAhead);
					return true;
				}

				fDistanceAhead -= Sqrtf(fMag2);

				fSplineTVal += RUNTIME_PATH_SPLINE_STEPVAL;
				vSegmentStart = vSplinePt;
			}

			vSegmentStart = vCtrlPt3;
			iProgress++;
			fSplineTVal = RUNTIME_PATH_SPLINE_STEPVAL;
			fInitialSplineTVal = 0.0f;
		}		
	}

	return false;
}

bank_float CTaskNavBase::ms_fCorneringLookAheadTime = 1.0f; //0.75f;
bank_float CTaskNavBase::ms_fGotoTargetLookAheadDist = 1.5f;
const float CTaskNavBase::ms_fTargetRadiusForWaypoints	= 0.5f;
const float CTaskNavBase::ms_fHeightAboveReturnedPointsForRouteVerts = 0.0f;
const float CTaskNavBase::ms_fTargetEqualEpsilon = 0.5f;
const float CTaskNavBase::ms_fWaypointSlowdownProximitySqr = 1.5f*1.5f;
const s32 CTaskNavBase::ms_iAttemptToSwimStraightToTargetFreqMs = 4000;
const float CTaskNavBase::ms_DefaultPathDistanceToExpand = 10.0f;
bank_bool CTaskNavBase::ms_bScanForObstructions = true;
bank_s32 CTaskNavBase::ms_iScanForObstructionsFreq_Walk = 4000;
bank_s32 CTaskNavBase::ms_iScanForObstructionsFreq_Run = 4000;
bank_s32 CTaskNavBase::ms_iScanForObstructionsFreq_Sprint = 3000;
bank_float CTaskNavBase::ms_fScanForObstructionsDistance = 30.0f;
bank_bool CTaskNavBase::ms_bUseExandRouteOptimisations = true;
bank_float CTaskNavBase::ms_fDistanceFromRouteEndToRequestNextSection = 6.0f;
bank_float CTaskNavBase::ms_fDefaultDistanceAheadForSetNewTarget = 0.0f;
bank_float CTaskNavBase::ms_fDistanceAheadForSetNewTargetObstruction = 4.0f;
bank_float CTaskNavBase::ms_fDistanceFromPathToTurnOnNavMeshAvoidance = 0.5f;
bank_float CTaskNavBase::ms_fDistanceFromPathToTurnOnObjectAvoidance = 0.5f;
bank_float CTaskNavBase::ms_fDistanceFromPathToTurnOnVehicleAvoidance = 0.5f;



#if __BANK
float CTaskNavBase::ms_fOverrideRadius = PATHSERVER_PED_RADIUS;
bank_bool CTaskNavBase::ms_bDebugRequests = false;
#endif

CNavMeshExpandedRouteVert CTaskNavBase::ms_vExpandedRouteVerts[CTaskNavBase::ms_iMaxExpandedVerts];
s32 CTaskNavBase::ms_iNumExpandedRouteVerts = 0;
Vector3 CTaskNavBase::ms_vClosestSegmentOnRoute = VEC3_ZERO;

u64 CNavBaseConfigFlags::GetAsPathServerFlags() const
{
	u64 iFlags = 0;

	if(IsSet(NB_ScriptedRoute))
		iFlags |= PATH_FLAG_SCRIPTED_ROUTE;
	if(IsSet(NB_HighPrioRoute))
		iFlags |= PATH_FLAG_HIGH_PRIO_ROUTE;
	if(IsSet(NB_UseLargerSearchExtents))
		iFlags |= PATH_FLAG_USE_LARGER_SEARCH_EXTENTS;
	if(!IsSet(NB_AvoidObjects))
		iFlags |= PATH_FLAG_DONT_AVOID_DYNAMIC_OBJECTS;
	if(IsSet(NB_UseDirectionalCover))
		iFlags |= PATH_FLAG_USE_DIRECTIONAL_COVER;
	if(IsSet(NB_DontUseLadders))
		iFlags |= PATH_FLAG_NEVER_USE_LADDERS;
	if(IsSet(NB_DontUseClimbs))
		iFlags |= PATH_FLAG_NEVER_CLIMB_OVER_STUFF;
	if(IsSet(NB_DontUseDrops))
		iFlags |= PATH_FLAG_NEVER_DROP_FROM_HEIGHT;
	if(IsSet(NB_SmoothPathCorners))
		iFlags |= PATH_FLAG_CUT_SHARP_CORNERS;
	if(IsSet(NB_AllowToNavigateUpSteepPolygons))
		iFlags |= PATH_FLAG_ALLOW_TO_NAVIGATE_UP_STEEP_POLYGONS;
	if(IsSet(NB_AllowToPushVehicleDoorsClosed))
		iFlags |= PATH_FLAG_ALLOW_TO_PUSH_VEHICLE_DOORS_CLOSED;
	if(IsSet(NB_NeverLeaveWater))
		iFlags |= PATH_FLAG_NEVER_LEAVE_WATER;
//	if(!IsSet(NB_DontAdjustTargetPosition))
//		iFlags |= PATH_FLAG_ENSURE_LOS_BEFORE_ENDING;
	if(IsSet(NB_ExpandStartEndTessellationRadius))
		iFlags |= PATH_FLAG_EXPAND_START_END_TESSELLATION_RADIUS;
	if(IsSet(NB_PullFromEdgeExtra))
		iFlags |= PATH_FLAG_PULL_FROM_EDGE_EXTRA;
	if(IsSet(NB_SofterFleeHeuristics))
		iFlags |= PATH_FLAG_SOFTER_FLEE_HEURISTICS;
	if(IsSet(NB_NeverLeaveDeepWater))
		iFlags |= PATH_FLAG_NEVER_LEAVE_DEEP_WATER;

	return iFlags;
}
void CNavBaseInternalFlags::ResetTemporaryFlags()
{
	// Save off the flags we need to persist
	bool bInitialised = IsSet(NB_HasBeenInitialised);
	bool bAbortedForDoorCollision = IsSet(NB_AbortedForDoorCollision);
	bool bAbortedForCollisionEvent = IsSet(NB_AbortedForCollisionEvent);
	bool bExceededVerticalDistanceFromPath = IsSet(NB_ExceededVerticalDistanceFromPath);

	ResetAll();

	// Restore saved flags
	Set(NB_HasBeenInitialised, bInitialised);
	Set(NB_AbortedForDoorCollision, bAbortedForDoorCollision);
	Set(NB_AbortedForCollisionEvent, bAbortedForCollisionEvent);
	Set(NB_ExceededVerticalDistanceFromPath, bExceededVerticalDistanceFromPath);
}

CTaskNavBase::CTaskNavBase(const float fMoveBlendRatio, const Vector3 & vTarget) :
	CTaskMove(fMoveBlendRatio),
	m_fInitialMoveBlendRatio(fMoveBlendRatio),
	m_vTarget(vTarget),
	m_vNewTarget(vTarget),
	m_pOnEntityRouteData(NULL),
	m_pRoute(NULL),
	m_pNextRouteSection(NULL),
	m_iPathHandle(PATH_HANDLE_NULL),
	m_iLastPathHandle(PATH_HANDLE_NULL),
	m_pLadderClipRequestHelper(NULL),
	m_fTargetRadius(0.0f),
	m_fCurrentTargetRadius(0.0f),
	m_fTargetStopHeading(DEFAULT_NAVMESH_FINAL_HEADING),
	m_fSlowDownDistance(0.0f),
	m_fDistanceToMoveWhileWaitingForPath(4.0f),
	m_fMaxSlopeNavigable(0.0f),
	m_fSlideToCoordHeading(0.0f),
	m_fSlideToCoordSpeed(0.0f),
	m_fSlideToCoordHeadingChangeSpeed(0.0f),
	m_fAnimatedStoppingDistance(0.0f),
	m_iWarpTimeMs(-1),
	m_iTimeSpentOnCurrentRouteSegmentMs(0),
	m_iTimeShouldSpendOnCurrentRouteSegmentMs(0),
	m_iRecalcPathTimerMs(0),
	m_iRecalcPathFreqMs(0),
	m_iTimeOfLastSetTargetCall(0),
	m_iProgress(0),
	m_iClampMinProgress(0),
	m_iClampMaxSearchDistance(0),
	m_fSplineTVal(0.0f),
	m_fCurrentLookAheadDistance(0.0f),
	m_iFreeSpaceReferenceValue(0),
	m_iNavMeshRouteMethod(eRouteMethod_Normal),
	m_iNumRouteAttempts(0),
	m_iLastRecoveryMode(0)
{
	Assertf(fMoveBlendRatio >= MOVEBLENDRATIO_STILL && fMoveBlendRatio <= MOVEBLENDRATIO_SPRINT, "Move blend ratio must be between MOVEBLENDRATIO_STILL and MOVEBLENDRATIO_SPRINT inclusive. The value passed in was %.1f", fMoveBlendRatio);
	ASSERT_ONLY(const float fLargeValue = 10000.0f;)
	Assertf(vTarget.x > -fLargeValue && vTarget.y > -fLargeValue && vTarget.z > -fLargeValue && vTarget.x < fLargeValue && vTarget.y < fLargeValue && vTarget.z < fLargeValue, "vTarget (%.2f, %.2f, %.2f) is outside of the world", vTarget.x, vTarget.y, vTarget.z);

	m_vAdjustedTarget[0] = m_vAdjustedTarget[1] = m_vAdjustedTarget[2] = 0.0f;
	m_vDirectionalCoverThreatPos[0] = m_vDirectionalCoverThreatPos[1] = 0.0f;

	m_NavBaseConfigFlags.ResetAll();
	m_NavBaseInternalFlags.ResetAll();

	// Set default flags
	m_NavBaseConfigFlags.Set(CNavBaseConfigFlags::NB_AvoidObjects);
	m_NavBaseConfigFlags.Set(CNavBaseConfigFlags::NB_AvoidEntityTypeObject);
	m_NavBaseConfigFlags.Set(CNavBaseConfigFlags::NB_AvoidEntityTypeVehicle);
	m_NavBaseConfigFlags.Set(CNavBaseConfigFlags::NB_AvoidPeds);
	m_NavBaseConfigFlags.Set(CNavBaseConfigFlags::NB_TryToGetOntoMainNavMeshIfStranded);
	m_NavBaseConfigFlags.Set(CNavBaseConfigFlags::NB_ScanForObstructions);
	m_NavBaseConfigFlags.Set(CNavBaseConfigFlags::NB_SetAllowNavmeshAvoidance);

	SetMaxDistanceMayAdjustPathStartPosition(TRequestPathStruct::ms_fDefaultMaxDistanceToAdjustPathEndPoints);
	SetMaxDistanceMayAdjustPathEndPosition(TRequestPathStruct::ms_fDefaultMaxDistanceToAdjustPathEndPoints);

#if __ALWAYS_STOP_EXACTLY
	m_NavBaseConfigFlags.Set(CNavBaseConfigFlags::NB_StopExactly);
#endif

	TUNE_GROUP_BOOL(PATH_FOLLOWING, ms_bUseExandRouteOptimisations, true);
}

CTaskNavBase::~CTaskNavBase()
{
	if(m_iPathHandle)
	{
		CPathServer::CancelRequest(m_iPathHandle);
	}
	if(m_pRoute)
	{
		delete m_pRoute;
	}
	if(m_pNextRouteSection)
	{
		delete m_pNextRouteSection;
	}
	if(m_pOnEntityRouteData)
	{
		delete m_pOnEntityRouteData;
	}
	if(m_pLadderClipRequestHelper)
	{
		CLadderClipSetRequestHelperPool::ReleaseClipSetRequestHelper(m_pLadderClipRequestHelper);
		m_pLadderClipRequestHelper = NULL;	// Will the destructor ever be called twice anyway!?
	}
}

s32 CTaskNavBase::GetDefaultStateAfterAbort() const
{
	return NavBaseState_Initial;
}

void CTaskNavBase::Restart()
{
	if(GetState()==NavBaseState_FollowingPath)
	{
		if(m_iPathHandle)
		{
			CPathServer::CancelRequest(m_iPathHandle);
			m_iLastPathHandle = m_iPathHandle;
			m_iPathHandle = PATH_HANDLE_NULL;
		}
		m_NavBaseInternalFlags.Set(CNavBaseInternalFlags::NB_RestartNextFrame, true);
	}
}

void CTaskNavBase::ResetForRestart()
{
	// Cancel any pending path request
	if(m_iPathHandle)
	{
		CPathServer::CancelRequest(m_iPathHandle);
		m_iLastPathHandle = m_iPathHandle;
		m_iPathHandle = PATH_HANDLE_NULL;
	}
	// Reset all base flags.
	// Any derived classes which implement this, must also call this base class function.

	m_NavBaseInternalFlags.ResetTemporaryFlags();
}

void CTaskNavBase::CleanUp()
{
	if(m_iPathHandle)
	{
		CPathServer::CancelRequest(m_iPathHandle);
		m_iLastPathHandle = m_iPathHandle;
		m_iPathHandle = PATH_HANDLE_NULL;
	}
}

void CTaskNavBase::CopySettings(CTaskNavBase * pClone) const
{
	pClone->m_NavBaseConfigFlags = m_NavBaseConfigFlags;
	pClone->m_NavBaseInternalFlags = m_NavBaseInternalFlags;

	pClone->SetMaxSlopeNavigable(GetMaxSlopeNavigable());
	pClone->SetDontUseLadders(GetDontUseLadders());
	pClone->SetDontUseClimbs(GetDontUseClimbs());
	pClone->SetDontUseDrops(GetDontUseDrops());
	pClone->SetUseLargerSearchExtents(GetUseLargerSearchExtents());
	pClone->SetIsScriptedRoute(GetIsScriptedRoute());
	pClone->SetSlideToCoordAtEnd(GetSlideToCoordAtEnd(), m_fSlideToCoordHeading, m_fSlideToCoordSpeed, m_fSlideToCoordHeadingChangeSpeed);
	Vector3 vThreatPos(m_vDirectionalCoverThreatPos[0], m_vDirectionalCoverThreatPos[0], 0.0f);
	pClone->SetUseDirectionalCover(m_NavBaseConfigFlags.IsSet(CNavBaseConfigFlags::NB_UseDirectionalCover), &vThreatPos);
	pClone->SetUseFreeSpaceReferenceValue(m_NavBaseConfigFlags.IsSet(CNavBaseConfigFlags::NB_UseFreeSpaceReferenceValue), m_iFreeSpaceReferenceValue, m_NavBaseConfigFlags.IsSet(CNavBaseConfigFlags::NB_FavourGreaterThanFreeSpaceReferenceValue));
	pClone->SetSmoothPathCorners(GetSmoothPathCorners());
	pClone->SetStartAtFirstRoutePoint(GetStartAtFirstRoutePoint());
	pClone->SetAllowToNavigateUpSteepPolygons(GetAllowToNavigateUpSteepPolygons());
	pClone->SetAllowToPushVehicleDoorsClosed(GetAllowToPushVehicleDoorsClosed());
	pClone->SetRecalcPathFrequency(m_iRecalcPathFreqMs, m_iRecalcPathTimerMs);
	pClone->SetHighPrioRoute(GetHighPrioRoute());
	pClone->SetConsiderRunStartForPathLookAhead(GetConsiderRunStartForPathLookAhead());
	pClone->SetScanForObstructions(GetScanForObstructions());
	pClone->m_ScanForObstructionsTimer.ChangeDuration(m_ScanForObstructionsTimer.GetDuration());
	pClone->SetDontStopForClimbs(GetDontStopForClimbs());
	pClone->SetClampMaxSearchDistance(GetClampMaxSearchDistance());
	pClone->m_iMaxDistanceToMovePathStart = m_iMaxDistanceToMovePathStart;
	pClone->m_iMaxDistanceToMovePathEnd = m_iMaxDistanceToMovePathEnd;
}


bool CTaskNavBase::ShouldAbort(const AbortPriority iPriority, const aiEvent* pEvent)
{
	return CTask::ShouldAbort(iPriority, pEvent);
}

// Handle aborting the task - this will be called in addition to CleanUp when we are aborting
void CTaskNavBase::DoAbort(const AbortPriority UNUSED_PARAM(iPriority), const aiEvent* pEvent)
{
	if(pEvent)
	{
		switch(pEvent->GetEventType())
		{
			case EVENT_OBJECT_COLLISION:
			{
				CEventObjectCollision * pObjectCollision = (CEventObjectCollision*)pEvent;
				if(pObjectCollision->GetCObject() && pObjectCollision->GetCObject()->IsADoor())
				{
					m_NavBaseInternalFlags.Set(CNavBaseInternalFlags::NB_AbortedForDoorCollision);
				}
				else
				{
					m_NavBaseInternalFlags.Set(CNavBaseInternalFlags::NB_AbortedForCollisionEvent);
				}
				break;
			}
			case EVENT_VEHICLE_COLLISION:
			{
				m_NavBaseInternalFlags.Set(CNavBaseInternalFlags::NB_AbortedForCollisionEvent);
				break;
			}
		}
	}
}

void CTaskNavBase::SwitchToNextRoute()
{
	Assert(m_pRoute);
	Assert(m_pNextRouteSection);

#if __ASSERT
	m_pRoute->CheckValid();
	m_pNextRouteSection->CheckValid();
#endif

	CNavMeshRoute * pNewRoute = rage_new CNavMeshRoute();

	s32 p;

	if(m_NavBaseInternalFlags.IsSet(CNavBaseInternalFlags::NB_OnTheFlyPathStartedAtRouteEnd))
	{
		// Add the remainder of the previous route to the beginning of the new route
		// We start from m_iProgress-1 so that we include the current segment (and spline)
		// which we are on; and we continue to GetSize()-1, because the final point will
		// be duplicated as the starting point of m_pNextRouteSection.

		p = Max(m_iProgress-1, 0);	// Make sure we dont' pass in a negative value. B* 1199441
		for( ; p<m_pRoute->GetSize()-1; p++)
		{
			pNewRoute->Add(
				m_pRoute->GetPosition(p),
				m_pRoute->GetWaypoint(p),
				m_pRoute->GetSplineEnum(p),
				m_pRoute->GetSplineCtrlPt2TVal(p),
				m_pRoute->GetFreeSpaceApproachingWaypoint(p)
				);
		}
	}
	else
	{
		// Find which expanded route section the start of the next section is along
		float fTVelTmp;
		Vector3 vClosestPosTmp;
		Vector3 vSegmentTmp;
		s32 iClosestSegToSectionStart = (s16) CalculateClosestSegmentOnExpandedRoute(ms_vExpandedRouteVerts, ms_iNumExpandedRouteVerts, m_pNextRouteSection->GetPosition(0), fTVelTmp, &vClosestPosTmp, &vSegmentTmp);
		Assert(iClosestSegToSectionStart != -1);

		// Add the expanded route segments up until the start of the next section
		// Since we already specified stopping at climbs/drops when we called GetPositionAheadOnRoute in InitPathRequestStruct()
		// we shouldn't have to worry about hittinga any of these.
		TNavMeshWaypointFlag wptEmpty;
		wptEmpty.Clear();

		const s32 iStartSeg =
			(m_iClosestSegOnExpandedRoute > 0 && IsSpecialActionClimbOrDrop(ms_vExpandedRouteVerts[m_iClosestSegOnExpandedRoute-1].m_iWptFlag.GetSpecialActionFlags())==0)
				? m_iClosestSegOnExpandedRoute - 1 : m_iClosestSegOnExpandedRoute;

		for(s32 s=iStartSeg; s<=iClosestSegToSectionStart; s++)
		{
			pNewRoute->Add( RCC_VECTOR3(ms_vExpandedRouteVerts[s].GetPosRef()), wptEmpty );
		}
	}

	// Append the next route to the new route up until capacity
	for(p=0; p<m_pNextRouteSection->GetSize() && pNewRoute->GetSize() < CNavMeshRoute::MAX_NUM_ROUTE_ELEMENTS; p++)
	{
		pNewRoute->Add(
			m_pNextRouteSection->GetPosition(p),
			m_pNextRouteSection->GetWaypoint(p),
			m_pNextRouteSection->GetSplineEnum(p),
			m_pNextRouteSection->GetSplineCtrlPt2TVal(p),
			m_pNextRouteSection->GetFreeSpaceApproachingWaypoint(p)
		);
	}

	// Ensure that we didn't end on a climb/drop waypoint, we require there always to be
	// space for a normal waypoint to exist in the route after these.
	// Also ensure that there is no spline enum set up for the final point
	if(pNewRoute->GetSize()==CNavMeshRoute::MAX_NUM_ROUTE_ELEMENTS)
	{
		TNavMeshWaypointFlag wpt = pNewRoute->GetWaypoint(pNewRoute->GetSize()-1);
		if(wpt.IsClimb() || wpt.IsDrop() || wpt.IsLadderClimb())
		{
			pNewRoute->SetSize( pNewRoute->GetSize()-1 );
		}
#if __ASSERT
		// Test again; should always be false
		wpt = pNewRoute->GetWaypoint(pNewRoute->GetSize()-1);
		Assert(!(wpt.IsClimb() || wpt.IsDrop() || wpt.IsLadderClimb()));
#endif

		// Clear spline enum on last point
		pNewRoute->Set(pNewRoute->GetSize()-1, pNewRoute->GetPosition(pNewRoute->GetSize()-1), pNewRoute->GetWaypoint(pNewRoute->GetSize()-1));
	}

	pNewRoute->SetRouteFlags( m_pNextRouteSection->GetRouteFlags() );

	delete m_pRoute;
	delete m_pNextRouteSection;

	m_pRoute = pNewRoute;
	m_pNextRouteSection = NULL;

#if __ASSERT
	m_pRoute->CheckValid();
#endif
}

CTask::FSM_Return CTaskNavBase::ProcessPreFSM()
{
	ms_iNumExpandedRouteVerts = 0;
	ms_vClosestSegmentOnRoute.Zero();
	m_iClosestSegOnExpandedRoute = 0;

	//*************************************************************************

	CPed *pPed = GetPed(); // Get the ped ptr.

	if(m_pOnEntityRouteData && m_pOnEntityRouteData->m_pDynamicEntityRouteIsOn)
	{
		// Supress timeslicing when navigating on top of a dynamic entity
		pPed->GetPedAiLod().SetForceNoTimesliceIntelligenceUpdate(true);

		// We need to favour maneuverability over aesthetics when navigating around on top of a dynamic entity
		pPed->SetPedResetFlag(CPED_RESET_FLAG_UseTighterTurnSettings, true);
	}

	//*************************************************************************
	// If this path is on a dynamic navmesh, then we transform it every frame

	RetransformRouteOnDynamicNavmesh(pPed);


	//****************************************************************************
	// Handle aborting the task & warping ped to the destination, due to time-out

	if(m_WarpTimer.IsSet() && m_WarpTimer.IsOutOfTime())
	{
		if(!GetSubTask() || GetSubTask()->MakeAbortable( CTask::ABORT_PRIORITY_URGENT, 0))
		{
			// Find the actual ground position below the target point
			bool bStupidUnusedVariable = true;
			Vector3 vGroundNormal(VEC3_ZERO);
			Vector3 vTeleportPos = m_vTarget;
			// Raise up by 1 meter in case the position was on/intersecting the ground
			vTeleportPos.z += 1.0f;

			float fGroundZ =  WorldProbe::FindGroundZFor3DCoord(TOP_SURFACE, m_vTarget, &bStupidUnusedVariable, &vGroundNormal);

			// If ground Z was relatively close to the specified target Z, we accept it
			if(Abs(vTeleportPos.z - fGroundZ) < 4.0f)
			{
				// Add on the height from the ground to the ped's root bone, to place them correctly on the ground surface
				vTeleportPos.z = fGroundZ + Abs(pPed->GetCapsuleInfo()->GetGroundToRootOffset());
			}
			else
			{
				// If position was greatly different, revert to the specified target to be on the safe side
				vTeleportPos = m_vTarget;
			}

			float fTeleportHeading = (m_fTargetStopHeading == DEFAULT_NAVMESH_FINAL_HEADING) ? pPed->GetDesiredHeading() : m_fTargetStopHeading;

			pPed->GetMotionData()->SetDesiredMoveBlendRatio(0.0f,0.0f);
			pPed->Teleport(vTeleportPos, fTeleportHeading, true);
			return FSM_Quit;
		}
	}

	if(GetHasNewMoveSpeed())
	{
		PropagateNewMoveSpeed(m_fMoveBlendRatio);
	}

	if(m_pRoute)
	{
		if(ms_bUseExandRouteOptimisations)
		{
			ms_iNumExpandedRouteVerts = m_pRoute->ExpandRoute(ms_vExpandedRouteVerts, NULL, Max(0, m_iProgress - 1), m_pRoute->GetSize(), ms_DefaultPathDistanceToExpand);
		}
		else
		{
			ms_iNumExpandedRouteVerts = m_pRoute->ExpandRoute(ms_vExpandedRouteVerts, NULL, 0, m_pRoute->GetSize(), 9999.0f);
		}

#if __DEV && DEBUG_DRAW
		static dev_bool bDrawExpandedRoute = false; 
		if(bDrawExpandedRoute)
		{
			for(int i=0; i<ms_iNumExpandedRouteVerts-1; i++)
			{
				grcDebugDraw::Line(VEC3V_TO_VECTOR3(ms_vExpandedRouteVerts[i].GetPos()) + ZAXIS, VEC3V_TO_VECTOR3(ms_vExpandedRouteVerts[i+1].GetPos()) + ZAXIS, Color_cyan, Color_cyan, 2);
			}
		}
#endif

		const Vector3 vPedPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());

		// Get the ped's progress along the linear sections stored in 'ms_vExpandedRouteVerts'
		// Don't allow ever progress along the route to go backwards
		if(ms_iNumExpandedRouteVerts)
		{
			m_iClosestSegOnExpandedRoute = (s16) CalculateClosestSegmentOnExpandedRoute(ms_vExpandedRouteVerts, ms_iNumExpandedRouteVerts, vPedPos, m_fSplineTVal, &m_vClosestPosOnRoute, &ms_vClosestSegmentOnRoute);

			// Advance to minimum 'm_iClampMinProgress'
			while(ms_vExpandedRouteVerts[m_iClosestSegOnExpandedRoute].m_iProgress < m_iClampMinProgress && m_iClosestSegOnExpandedRoute < ms_iNumExpandedRouteVerts-1)
				m_iClosestSegOnExpandedRoute++;
		}
		else
		{
			m_iClosestSegOnExpandedRoute = 0;
		}
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskNavBase::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	FSM_Begin

		FSM_State(NavBaseState_Initial)
			FSM_OnEnter
				return StateInitial_OnEnter(pPed);
			FSM_OnUpdate
				return StateInitial_OnUpdate(pPed);

		FSM_State(NavBaseState_WaitingInVehiclePriorToRequest)
			FSM_OnUpdate
				return StateWaitingInVehiclePriorToRequest_OnUpdate(pPed);

		FSM_State(NavBaseState_RequestPath)
			FSM_OnEnter
				return StateRequestPath_OnEnter(pPed);
			FSM_OnUpdate
				return StateRequestPath_OnUpdate(pPed);

		FSM_State(NavBaseState_MovingWhilstWaitingForPath)
			FSM_OnEnter
				return StateMovingWhilstWaitingForPath_OnEnter(pPed);
			FSM_OnUpdate
				return StateMovingWhilstWaitingForPath_OnUpdate(pPed);

		FSM_State(NavBaseState_StationaryWhilstWaitingForPath)
			FSM_OnEnter
				return StateStationaryWhilstWaitingForPath_OnEnter(pPed);
			FSM_OnUpdate
				return StateStationaryWhilstWaitingForPath_OnUpdate(pPed);

		FSM_State(NavBaseState_WaitingToLeaveVehicle)
			FSM_OnUpdate
				return StateWaitingToLeaveVehicle_OnUpdate(pPed);

		FSM_State(NavBaseState_WaitingAfterFailureToObtainPath)
				FSM_OnEnter
				return StateWaitingAfterFailureToObtainPath_OnEnter(pPed);
			FSM_OnUpdate
				return StateWaitingAfterFailureToObtainPath_OnUpdate(pPed);
			

		FSM_State(NavBaseState_FollowingPath)
			FSM_OnEnter
				return StateFollowingPath_OnEnter(pPed);
			FSM_OnUpdate
				return StateFollowingPath_OnUpdate(pPed);

		FSM_State(NavBaseState_SlideToCoord)
			FSM_OnEnter
				return StateSlideToCoord_OnEnter(pPed);
			FSM_OnUpdate
				return StateSlideToCoord_OnUpdate(pPed);

		FSM_State(NavBaseState_StoppingAtTarget)
			FSM_OnEnter
				return StateStoppingAtTarget_OnEnter(pPed);
			FSM_OnUpdate
				return StateStoppingAtTarget_OnUpdate(pPed);

		FSM_State(NavBaseState_WaitingForWaypointActionEvent)
			FSM_OnEnter
				return StateWaitingForWaypointActionEvent_OnEnter(pPed);
			FSM_OnUpdate
				return StateWaitingForWaypointActionEvent_OnUpdate(pPed);

		FSM_State(NavBaseState_WaitingAfterFailedWaypointAction)
			FSM_OnEnter
				return StateWaitingAfterFailedWaypointAction_OnEnter(pPed);
			FSM_OnUpdate
				return StateWaitingAfterFailedWaypointAction_OnUpdate(pPed);

		FSM_State(NavBaseState_GetOntoMainNavMesh)
			FSM_OnEnter
				return StateGetOntoMainNavMesh_OnEnter(pPed);
			FSM_OnUpdate
				return StateGetOntoMainNavMesh_OnUpdate(pPed);

		FSM_State(NavBaseState_AchieveHeading)
			FSM_OnEnter
				return StateAchieveHeading_OnEnter(pPed);
			FSM_OnUpdate
				return StateAchieveHeading_OnUpdate(pPed);

		FSM_State(NavBaseState_UsingLadder)
			FSM_OnEnter
				return StateUsingLadder_OnEnter(pPed);
			FSM_OnUpdate
				return StateUsingLadder_OnUpdate(pPed);

	FSM_End
}

CTask::FSM_Return CTaskNavBase::ProcessPostFSM()
{
//	m_NavBaseInternalFlags.Reset(CNavBaseInternalFlags::NB_WasAborted);

	ms_iNumExpandedRouteVerts = 0;
	return FSM_Continue;
}

CTask::FSM_Return CTaskNavBase::StateInitial_OnEnter(CPed * UNUSED_PARAM(pPed))
{
	if(!m_pRoute)
		m_pRoute = rage_new CNavMeshRoute();

	return FSM_Continue;
}

CTask::FSM_Return CTaskNavBase::StateInitial_OnUpdate(CPed * UNUSED_PARAM(pPed))
{
	//Check if we should wait in the vehicle.
	if(ShouldWaitInVehicle())
	{
		//Wait in the vehicle.
		SetState(NavBaseState_WaitingInVehiclePriorToRequest);
	}
	else
	{
		//Request a path.
		SetState(NavBaseState_RequestPath);
	}

	// Set the initial look ahead distance.
	m_fCurrentLookAheadDistance = GetInitialGotoTargetLookAheadDistance();

	// Look the default max navigable slope for the ped type, unless previously set.
	if (!m_NavBaseConfigFlags.IsSet(CNavBaseInternalFlags::NB_UsingMaxSlopeNavigableOverride))
	{
		m_fMaxSlopeNavigable = GetPed()->GetPedIntelligence()->GetNavCapabilities().GetDefaultMaxSlopeNavigable();
	}

	return FSM_Continue;
}

bool CTaskNavBase::ForceVehicleDoorOpenForNavigation(CPed * pPed)
{
	if(pPed->GetVehiclePedInside())
	{
		//Check if the ped is exiting the vehicle.
		//Force the vehicle's door to be considered opened for navigation.
		CTaskExitVehicle* pTask = static_cast<CTaskExitVehicle*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_EXIT_VEHICLE));
		if(pTask && pTask->IsRunningFlagSet(CVehicleEnterExitFlags::IsFleeExit))
		{
			CCarDoor * pDoor = pTask->GetDoor();
			if(pDoor)
			{
				//Ensure the vehicle is up-to-date (especially the doors).
				pDoor->SetForceOpenForNavigation(true);

				CVehicle* pVehicle = pPed->GetMyVehicle();
				if(pVehicle->WasUnableToAddToPathServer())
				{
					CPathServerGta::MaybeAddDynamicObject(pVehicle);
				}
				if(pVehicle->IsInPathServer())
				{
					CPathServerGta::UpdateDynamicObject(pVehicle, true, true);

					// This may cause doors to be updated twice but no matter
					// Its important we call this to ensure doors get added to pathfinder if not already
					pVehicle->UpdateDoorsForNavigation();
				}

				return true;
			}
		}
	}
	return false;
}

CTask::FSM_Return CTaskNavBase::StateWaitingInVehiclePriorToRequest_OnUpdate(CPed * pPed)
{
	ForceVehicleDoorOpenForNavigation(pPed);

	//Check if we should not wait in the vehicle.
	if(!ShouldWaitInVehicle())
	{
		//Ensure the vehicle is up-to-date (especially the doors).
		/*
		CVehicle* pVehicle = pPed->GetMyVehicle();
		if(pVehicle && pVehicle->IsInPathServer())
		{
			CPathServerGta::UpdateDynamicObject(pVehicle, true);
		}
		else if(pVehicle && pVehicle->WasUnableToAddToPathServer())
		{
			CPathServerGta::MaybeAddDynamicObject(pVehicle);
		}
		*/

		//Request a path.
		SetState(NavBaseState_RequestPath);
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskNavBase::StateRequestPath_OnEnter(CPed * pPed)
{
	if(!m_pRoute)
		m_pRoute = rage_new CNavMeshRoute;

	ResetForRestart();

	float fMinLookAhead = GetInitialGotoTargetLookAheadDistance();
	m_fCurrentLookAheadDistance = Max(m_fCurrentLookAheadDistance, fMinLookAhead);

	bool bOnDynamicEntity =  false;
	if(!m_NavBaseInternalFlags.IsSet(CNavBaseInternalFlags::NB_HasBeenInitialised))
	{
		//***************************************************************************************
		// If this route is on a dynamic entity, then initialise our m_pOnEntityRouteData member

		if(pPed->GetGroundPhysical() &&
			pPed->GetGroundPhysical()->GetType()==ENTITY_TYPE_VEHICLE &&
			((CVehicle*)pPed->GetGroundPhysical())->m_nVehicleFlags.bHasDynamicNavMesh)
		{
			bOnDynamicEntity = true;
			if(!m_pOnEntityRouteData)
				m_pOnEntityRouteData = rage_new COnEntityRouteData();
			m_pOnEntityRouteData->m_pDynamicEntityRouteIsOn = pPed->GetGroundPhysical();

			Matrix34 mInvMat;
			mInvMat.Inverse(MAT34V_TO_MATRIX34(((CVehicle*)pPed->GetGroundPhysical())->GetMatrix()));
			mInvMat.Transform(m_vTarget, m_pOnEntityRouteData->m_vLocalSpaceTarget);
		}
	}

	if(!bOnDynamicEntity && m_pOnEntityRouteData)
	{
		delete m_pOnEntityRouteData;
		m_pOnEntityRouteData = NULL;
	}

	//***************************************************************************
	// Do anything which needs to be done on the first running of this task only

	const bool bForceTarget = !m_NavBaseInternalFlags.IsSet(CNavBaseInternalFlags::NB_HasBeenInitialised);

	if(!m_NavBaseInternalFlags.IsSet(CNavBaseInternalFlags::NB_HasBeenInitialised))
	{
		m_NavBaseInternalFlags.Set(CNavBaseInternalFlags::NB_HasBeenInitialised);

		if(m_iWarpTimeMs > 0)
			m_WarpTimer.Set(fwTimer::GetTimeInMilliseconds(), m_iWarpTimeMs);

		//***************************************************************
		// Record if we are in a sequence of move tasks before/after us

		bool bGotoTaskBefore, bGotoTaskAfter;
		GetIsTaskSequencedBefore(pPed, bGotoTaskBefore);
		GetIsTaskSequencedAfter(pPed, bGotoTaskAfter);
		m_NavBaseInternalFlags.Set(CNavBaseInternalFlags::NB_GotoTaskSequencedBefore, bGotoTaskBefore);
		m_NavBaseInternalFlags.Set(CNavBaseInternalFlags::NB_GotoTaskSequencedAfter, bGotoTaskAfter);
	}

	//******************************************
	// Request the route in from the pathfinder

	SetTarget(pPed, VECTOR3_TO_VEC3V(m_vNewTarget), bForceTarget);
	m_NavBaseInternalFlags.Reset(CNavBaseInternalFlags::NB_HadNewTarget);

	SetTargetRadius(m_fTargetRadius);
	SetSlowDownDistance(m_fSlowDownDistance);

	BANK_ONLY( if(ms_bDebugRequests) Displayf("%u) RequestPath() : Ped 0x%p due to StateRequestPath_OnEnter", fwTimer::GetFrameCount(), pPed); )

	RequestPath(pPed);

	return FSM_Continue;
}
CTask::FSM_Return CTaskNavBase::StateRequestPath_OnUpdate(CPed * pPed)
{
	if(m_iPathHandle==PATH_HANDLE_NULL)
	{
		m_NavBaseInternalFlags.Reset(CNavBaseInternalFlags::NB_AbortedForDoorCollision);
		m_NavBaseInternalFlags.Reset(CNavBaseInternalFlags::NB_AbortedForCollisionEvent);
		m_NavBaseInternalFlags.Reset(CNavBaseInternalFlags::NB_ExceededVerticalDistanceFromPath);

		SetState(NavBaseState_WaitingAfterFailureToObtainPath);
		return FSM_Continue;
	}

	bool bMayUsePreviousTarget = true;
	m_NavBaseInternalFlags.Set(CNavBaseInternalFlags::NB_MayUsePreviousTargetInMovingWaitingForPath, true);

	// Normal processing
	if(pPed->GetVehiclePedInside()==NULL && ShouldKeepMovingWhilstWaitingForPath(pPed, bMayUsePreviousTarget))
	{
		m_NavBaseInternalFlags.Reset(CNavBaseInternalFlags::NB_AbortedForDoorCollision);
		m_NavBaseInternalFlags.Reset(CNavBaseInternalFlags::NB_AbortedForCollisionEvent);
		m_NavBaseInternalFlags.Reset(CNavBaseInternalFlags::NB_ExceededVerticalDistanceFromPath);

		m_NavBaseInternalFlags.Set(CNavBaseInternalFlags::NB_MayUsePreviousTargetInMovingWaitingForPath, bMayUsePreviousTarget);

		SetState(NavBaseState_MovingWhilstWaitingForPath);
		return FSM_Continue;
	}
	else
	{
		m_NavBaseInternalFlags.Reset(CNavBaseInternalFlags::NB_AbortedForDoorCollision);
		m_NavBaseInternalFlags.Reset(CNavBaseInternalFlags::NB_AbortedForCollisionEvent);
		m_NavBaseInternalFlags.Reset(CNavBaseInternalFlags::NB_ExceededVerticalDistanceFromPath);

		BANK_ONLY( if(ms_bDebugRequests) Displayf("%u) Ped 0x%p entering NavBaseState_StationaryWhilstWaitingForPath, from StateRequestPath_OnUpdate", fwTimer::GetFrameCount(), pPed); )

		SetState(NavBaseState_StationaryWhilstWaitingForPath);
		return FSM_Continue;
	}
}
CTask::FSM_Return CTaskNavBase::StateMovingWhilstWaitingForPath_OnEnter(CPed * pPed)
{
	m_NavBaseInternalFlags.Reset(CNavBaseInternalFlags::NB_CalculatingPathOnTheFly);
	m_NavBaseInternalFlags.Reset(CNavBaseInternalFlags::NB_OnTheFlyPathStartedAtRouteEnd);
	m_NavBaseInternalFlags.Reset(CNavBaseInternalFlags::NB_StartedFromInVehicle);

	if(m_iPathHandle==PATH_HANDLE_NULL)
	{	
		return FSM_Continue;
	}

	Vector3 vTempTarget;
	bool bMoveTargetDecided = false;
	if( GetSubTask() && m_NavBaseInternalFlags.IsSet(CNavBaseInternalFlags::NB_MayUsePreviousTargetInMovingWaitingForPath) &&
		(GetSubTask()->GetTaskType()==CTaskTypes::TASK_MOVE_GO_TO_POINT_ON_ROUTE ||
		GetSubTask()->GetTaskType()==CTaskTypes::TASK_MOVE_GO_TO_POINT_AND_STAND_STILL ||

			(GetSubTask()->GetTaskType()==CTaskTypes::TASK_MOVE_GO_TO_POINT &&
			m_iProgress==CNavMeshRoute::MAX_NUM_ROUTE_ELEMENTS &&
			GetPreviousState()==NavBaseState_FollowingPath) ||

				(GetSubTask()->GetTaskType()==CTaskTypes::TASK_MOVE_GO_TO_POINT &&
				GetPreviousState()==NavBaseState_FollowingPath &&
				m_pRoute && m_iProgress==m_pRoute->GetSize()-1) ) )

	{
		Vector3 vTarget = ((CTaskMove*)GetSubTask())->GetTarget();
		Vector3 vVec = vTarget - VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
		if (vVec.Mag2() > 0.2f * 0.2f || m_fInitialMoveBlendRatio < MBR_RUN_BOUNDARY)
		{
			vVec.Normalize();
			float fDotReq =  (m_fInitialMoveBlendRatio < MBR_RUN_BOUNDARY ? 0.f : 0.866f);
			if (vVec.Dot(VEC3V_TO_VECTOR3(pPed->GetTransform().GetForward())) > fDotReq)
			{
				vVec.Scale(m_fDistanceToMoveWhileWaitingForPath);
				vTempTarget = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()) + vVec;
				bMoveTargetDecided = true;
			}
		}
	}

	if (!bMoveTargetDecided)
	{
		// If strafing then move in the strafe direction
		if(pPed->IsStrafing() && pPed->GetMotionData()->GetCurrentMoveBlendRatio().Mag2() > 0.1f)
		{
			Vector3 vTargetOffset( pPed->GetMotionData()->GetCurrentMoveBlendRatio(), Vector2::kXY);
			vTargetOffset.Normalize();
			vTargetOffset.Scale(m_fDistanceToMoveWhileWaitingForPath);
			// moveblend ratio is in local space, so rotate by ped's heading
			Matrix34 rotMat;
			rotMat.Identity();
			rotMat.RotateZ(pPed->GetCurrentHeading());
			rotMat.Transform3x3(vTargetOffset);
			vTempTarget = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()) + vTargetOffset;
		}
		// Otherwise move in the ped's heading
		else
		{
			Vector3 vTargetOffset(-rage::Sinf(pPed->GetCurrentHeading()), rage::Cosf(pPed->GetCurrentHeading()), 0.0f);
			vTargetOffset.Scale(m_fDistanceToMoveWhileWaitingForPath);
			vTempTarget = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()) + vTargetOffset;
		}
	}

	const float fTargetRadius =
		(IsNavigatingUnderwater(pPed) || pPed->GetIsSkiing()) ?
			2.0f : CTaskMoveGoToPoint::ms_fTargetRadius;

	CTaskMoveGoToPoint * pMove = rage_new CTaskMoveGoToPoint(m_fInitialMoveBlendRatio, vTempTarget, fTargetRadius);
	pMove->SetAllowNavmeshAvoidance(m_NavBaseConfigFlags.IsSet(CNavBaseConfigFlags::NB_SetAllowNavmeshAvoidance));
	// turn on all avoidance since we don't have a path
	pMove->SetDistanceFromPathToEnableNavMeshAvoidance(-1.0f); 
	pMove->SetDistanceFromPathToEnableObstacleAvoidance(-1.0f);
	pMove->SetDistanceFromPathToEnableVehicleAvoidance(-1.0f);
	SetNewTask(pMove);

	m_NavBaseInternalFlags.Reset(CNavBaseInternalFlags::NB_MayUsePreviousTargetInMovingWaitingForPath);

	if(m_pRoute)
		m_pRoute->Clear();

	return FSM_Continue;
}
CTask::FSM_Return CTaskNavBase::StateMovingWhilstWaitingForPath_OnUpdate(CPed * pPed)
{
	pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_LowPhysicsLodMayPlaceOnNavMesh, true);

	// In this state we have to get an update every frame, or run the risk of penetrating collision
	if( CPedAILodManager::IsTimeslicingEnabled() )
	{
		pPed->GetPedAiLod().SetForceNoTimesliceIntelligenceUpdate(true);
	}

	// If we are a lod physics LOD ped, and have moved off the navmesh - then immediately go into
	// state NavBaseState_StationaryWhilstWaitingForPath.  Peds may otherwise run through collision.
	if(pPed->GetPedAiLod().IsLodFlagSet(CPedAILod::AL_LodPhysics) && !pPed->GetNavMeshTracker().GetIsValid())
	{
		BANK_ONLY( if(ms_bDebugRequests) Displayf("%u) Ped 0x%p entering NavBaseState_StationaryWhilstWaitingForPath, from StateMovingWhilstWaitingForPath_OnUpdate (tracker invalid)", fwTimer::GetFrameCount(), pPed); )

		pPed->GetMotionData()->SetDesiredMoveBlendRatio(0.0f, 0.0f);
		SetState(NavBaseState_StationaryWhilstWaitingForPath);
		return FSM_Continue;
	}

	// If we are running directly into some obstruction then go immediately into stationary state
	if(IsMovingIntoObstacle(pPed, true, true))
	{
		BANK_ONLY( if(ms_bDebugRequests) Displayf("%u) Ped 0x%p entering NavBaseState_StationaryWhilstWaitingForPath, from StateMovingWhilstWaitingForPath_OnUpdate (moving into obstacle)", fwTimer::GetFrameCount(), pPed); )

		pPed->GetMotionData()->SetDesiredMoveBlendRatio(0.0f, 0.0f);
		SetState(NavBaseState_StationaryWhilstWaitingForPath);
		return FSM_Continue;
	}

	if(m_iPathHandle==PATH_HANDLE_NULL)
	{
		SetState(NavBaseState_WaitingAfterFailureToObtainPath);
		return FSM_Continue;
	}

	Assert(GetSubTask() && (GetSubTask()->GetTaskType()==CTaskTypes::TASK_MOVE_GO_TO_POINT || GetSubTask()->GetTaskType()==CTaskTypes::TASK_MOVE_FOLLOW_POINT_ROUTE));

	EPathServerRequestResult ret = CPathServer::IsRequestResultReady(m_iPathHandle, EPath);

	if(ret == ERequest_Ready)
	{
		TPathResultInfo pathResultInfo;

		if(!RetrievePath(pathResultInfo, pPed, m_pRoute))
		{
			return OnFailedRoute(pPed, pathResultInfo);
		}

		if(OnSuccessfulRoute(pPed))
		{
			SetState(NavBaseState_FollowingPath);
		}
		else
		{
			BANK_ONLY( if(ms_bDebugRequests) Displayf("%u) Ped 0x%p entering NavBaseState_StationaryWhilstWaitingForPath, from StateMovingWhilstWaitingForPath_OnUpdate (unsuccessful route)", fwTimer::GetFrameCount(), pPed); )

			SetState(NavBaseState_StationaryWhilstWaitingForPath);
		}

		return FSM_Continue;
	}
	else if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		BANK_ONLY( if(ms_bDebugRequests) Displayf("%u) Ped 0x%p entering NavBaseState_StationaryWhilstWaitingForPath, from StateMovingWhilstWaitingForPath_OnUpdate (move subtask is finished)", fwTimer::GetFrameCount(), pPed); )

		Assert(m_iPathHandle!=PATH_HANDLE_NULL);
		SetState(NavBaseState_StationaryWhilstWaitingForPath);
	}
	return FSM_Continue;
}
CTask::FSM_Return CTaskNavBase::StateStationaryWhilstWaitingForPath_OnEnter(CPed * pPed)
{
	m_NavBaseInternalFlags.Reset(CNavBaseInternalFlags::NB_MayUsePreviousTargetInMovingWaitingForPath);
	m_NavBaseInternalFlags.Reset(CNavBaseInternalFlags::NB_CalculatingPathOnTheFly);
	m_NavBaseInternalFlags.Reset(CNavBaseInternalFlags::NB_OnTheFlyPathStartedAtRouteEnd);

	m_NavBaseInternalFlags.Set(CNavBaseInternalFlags::NB_StartedFromInVehicle, pPed->GetVehiclePedInside()!=NULL );

	CTaskMoveStandStill * pTaskStill = rage_new CTaskMoveStandStill(1);
	SetNewTask(pTaskStill);

	if(m_pRoute)
		m_pRoute->Clear();

	return FSM_Continue;
}
CTask::FSM_Return CTaskNavBase::StateStationaryWhilstWaitingForPath_OnUpdate(CPed * pPed)
{
	if(pPed->GetVehiclePedInside()==NULL)
	{
		pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_LowPhysicsLodMayPlaceOnNavMesh, true);
	}
	else
	{
		ForceVehicleDoorOpenForNavigation(pPed);
	}

	if(m_iPathHandle == PATH_HANDLE_NULL)
	{
		SetState(NavBaseState_WaitingAfterFailureToObtainPath);
		return FSM_Continue;
	}

	Assert(!GetSubTask() || GetSubTask()->GetTaskType()==CTaskTypes::TASK_MOVE_STAND_STILL);

	EPathServerRequestResult ret = CPathServer::IsRequestResultReady(m_iPathHandle, EPath);

	if(ret == ERequest_Ready)
	{
		TPathResultInfo pathResultInfo;

		if(!RetrievePath(pathResultInfo, pPed, m_pRoute))
		{
			return OnFailedRoute(pPed, pathResultInfo);
		}

		if( OnSuccessfulRoute(pPed) )
		{
			m_fCurrentLookAheadDistance = GetInitialGotoTargetLookAheadDistance();

			if(pPed->GetVehiclePedInside())
			{
				SetState(NavBaseState_WaitingToLeaveVehicle);
			}
			else
			{
				SetState(NavBaseState_FollowingPath);
			}
		}

		return FSM_Continue;
	}
	else if(GetIsFlagSet(aiTaskFlags::SubTaskFinished) || (!GetSubTask()) || ret == ERequest_NotFound)
	{
		if(ret == ERequest_NotFound)
			m_iPathHandle = PATH_HANDLE_NULL;

		//*************************************************
		// Handle a new target being assigned to this task

		s32 iStateBefore = GetState();
		if(HandleNewTarget(pPed))
		{
			if(iStateBefore != GetState())
			{
				return FSM_Continue;
			}
			else
			{
				SetFlag(aiTaskFlags::RestartCurrentState);
				return FSM_Continue;
			}
		}
		else
		{
			CTaskMoveStandStill * pTaskStill = rage_new CTaskMoveStandStill(1);
			SetNewTask(pTaskStill);
			return FSM_Continue;
		}
	}
	return FSM_Continue;
}

CTask::FSM_Return CTaskNavBase::StateWaitingToLeaveVehicle_OnUpdate(CPed * pPed)
{
	if(pPed->GetVehiclePedInside())
	{
		return FSM_Continue;
	}
	SetState(NavBaseState_FollowingPath);
	return FSM_Continue;
}

CTask::FSM_Return CTaskNavBase::StateWaitingAfterFailureToObtainPath_OnEnter(CPed * UNUSED_PARAM(pPed))
{
	CTaskMoveStandStill * pTaskStill = rage_new CTaskMoveStandStill(500);
	SetNewTask(pTaskStill);
	return FSM_Continue;
}

CTask::FSM_Return CTaskNavBase::StateWaitingAfterFailureToObtainPath_OnUpdate(CPed * UNUSED_PARAM(pPed))
{
	if(!GetSubTask() || GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		SetState(NavBaseState_Initial);
	}
	return FSM_Continue;
}

CTask::FSM_Return CTaskNavBase::StateFollowingPath_OnEnter(CPed * pPed)
{
	Assert(pPed->GetVehiclePedInside()==NULL);

	CreateRouteFollowingTask(pPed);

	CalcTimeNeededToCompleteRouteSegment(pPed, m_iProgress);

	// Perform the initial scan only 1 second after we start following this route;
	// helps with cases where the route has been calculated early (fleeing on foot, fleeing on vehicle) whilst some
	// animation has been played out, but something has moved in our path in the meantime.
	// Subsequent scans will be at ms_iScanForObstructionsFreq.
	if( ms_bScanForObstructions && GetScanForObstructions() )
	{
		const u32 iTime = GetInitialScanForObstructionsTime();
		m_ScanForObstructionsTimer.Set( fwTimer::GetTimeInMilliseconds(), iTime );	
	}

	m_NavBaseInternalFlags.Reset(CNavBaseInternalFlags::NB_StartedFromInVehicle);

	// Check the union of the special action flags on the route, before doing the full loop over the route points.
	if(m_pRoute->GetPossiblySetSpecialActionFlags() & (WAYPOINT_FLAG_CLIMB_LADDER|WAYPOINT_FLAG_DESCEND_LADDER))
	{
		for (s32 p = 0; p < m_pRoute->GetSize(); ++p)
		{
			u32 SpecialActionFlags = m_pRoute->GetWaypoint(p).GetSpecialActionFlags();
			if (SpecialActionFlags & (WAYPOINT_FLAG_CLIMB_LADDER|WAYPOINT_FLAG_DESCEND_LADDER))
			{

				// Stream the bloody ladder
				if (!m_pLadderClipRequestHelper)
					m_pLadderClipRequestHelper = CLadderClipSetRequestHelperPool::GetNextFreeHelper(pPed);

				taskAssertf(NetworkInterface::IsGameInProgress() || m_pLadderClipRequestHelper, "CLadderClipSetRequestHelperPool::GetNextFreeHelper() Gave us NULL!? in CTaskNavBase::StateFollowingPath_OnEnter()");

				if (m_pLadderClipRequestHelper)
				{
					int iStreamStates = CTaskClimbLadder::NEXT_STATE_CLIMBING;
					if (SpecialActionFlags & WAYPOINT_FLAG_IS_ON_WATER_SURFACE)
						iStreamStates |= CTaskClimbLadder::NEXT_STATE_WATERMOUNT;

					CTaskClimbLadder::StreamReqResourcesIn(m_pLadderClipRequestHelper, pPed, iStreamStates);
				}
				break;
			}
		}
	}

	return FSM_Continue;
}

/*
PURPOSE
	Helper function that takes two vectors, flattens them onto the XY plane, measures the angle between them,
	and checks if it's less than some threshold. The threshold angle has to be less than 90 degrees, and is
	specified as the square of the sine of the angle. The input vectors don't have to be normalized. It's safe
	to call even if the length of the vectors in the XY plane is zero, though the results are not well defined.
	The function effectively computes the angle using a cross product rather than a dot product, which is done
	to make it more accurate for very acute angles.
PARAMS
	inputVecA			- Input vector A.
	inputVecB			- Input vector B.
	squareSineAngleV	- The square of the sine of the acute threshold angle.
RETURNS
	True if the two vectors are within the specified angle.
NOTES
	This function should basically be equivalent to this, but very much faster:
		const float angA = fwAngle::GetRadianAngleBetweenPoints(inputVecA.GetXf(), inputVecA.GetYf(), 0.0f, 0.0f);
		const float angB = fwAngle::GetRadianAngleBetweenPoints(inputVecB.GetXf(), inputVecB.GetYf(), 0.0f, 0.0f);
		const float absDelta = Abs(SubtractAngleShorter(angA, angB));
		return absDelta <= angle;
*/
static bool sIsAngleBetweenFlattenedVectorsLessThan(Vec3V_In inputVecA, Vec3V_In inputVecB, ScalarV_In squareSineAngleV)
{
	// Treat the input as 4D vectors so we can build other 4D vectors from them.
	const Vec4V inputVecA4V(inputVecA.GetIntrin128());
	const Vec4V inputVecB4V(inputVecB.GetIntrin128());

	// Build a 4D vectors with the X and Y elements from the both input vectors.
	const Vec4V inputVecBXAXBYAYV = GetFromTwo<Vec::X1, Vec::X2, Vec::Y1, Vec::Y2>(inputVecB4V, inputVecA4V);

	// Multiply each element, rotate, and add, so that lenProductsSumV contains the square
	// of the lengths of the two vectors.
	const Vec4V lenProductsV = Scale(inputVecBXAXBYAYV, inputVecBXAXBYAYV);
	const Vec4V lenProductsRotV = lenProductsV.Get<Vec::Z, Vec::W, Vec::X, Vec::Y>();
	const Vec4V lenProductsSumV = Add(lenProductsV, lenProductsRotV);

	// Build vectors to use for computing the dot product between the two vectors, and between one input vector
	// and the orthonormal vector of the other.
	const Vec4V dotTermsAV = GetFromTwo<Vec::X1, Vec::Y1, Vec::Y1, Vec::X2>(inputVecA4V, Negate(inputVecA4V));
	const Vec4V dotTermsBV = inputVecB4V.Get<Vec::X, Vec::X, Vec::Y, Vec::Y>();

	// Multiply the elements, rotate, and add together.
	const Vec4V dotProductsV = Scale(dotTermsBV, dotTermsAV);
	const Vec4V dotProductsRotV = dotProductsV.Get<Vec::Z, Vec::W, Vec::X, Vec::Y>();
	const Vec4V dotProductSumsV = Add(dotProductsV, dotProductsRotV);

	// Now, we have all the dot products and lengths that we need.
	const ScalarV fwdDotV = dotProductSumsV.GetX();
	const ScalarV orthDotV = dotProductSumsV.GetY();
	const ScalarV inputLenASqV = lenProductsSumV.GetY();
	const ScalarV inputLenBSqV = lenProductsSumV.GetX();

	// For the two vectors to be within the angle threshold, the dot product with the orthonormal
	// (basically the Z component of the cross product of the flattened vectors in 3D space) must
	// have an absolute value less than the sine of the angle, after dividing by the length of the
	// input vectors in 2D. We multiply by the lengths and then square both sides of the inequality
	// to avoid both division and square roots. In addition to this condition, the dot product between
	// the two vectors also has to be positive.
	const ScalarV orthDotSqV = Scale(orthDotV, orthDotV);
	const ScalarV thresholdV = Scale(squareSineAngleV, Scale(inputLenASqV, inputLenBSqV));
	const BoolV sineWithinThresholdV = IsLessThanOrEqual(orthDotSqV, thresholdV);
	const BoolV cosRightSignV = IsGreaterThanOrEqual(fwdDotV, ScalarV(V_ZERO));
	return IsTrue(And(sineWithinThresholdV, cosRightSignV));
}


// NAME : OnSuccessfulRoute
// PURPOSE : Base implementation of this, which must be called from within the derived version to set up various data for route-following

bool CTaskNavBase::OnSuccessfulRoute(CPed * pPed)
{
	float fRouteLength;
	m_iClosestSegOnExpandedRoute = 0;
	m_iClampMinProgress = 0;
	ms_iNumExpandedRouteVerts = m_pRoute->ExpandRoute(ms_vExpandedRouteVerts, &fRouteLength, 0, m_pRoute->GetSize(), 9999.0f);
	Assert(ms_iNumExpandedRouteVerts);

	const Vector3 vPedPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());

	if(ms_iNumExpandedRouteVerts)
	{
		m_iClosestSegOnExpandedRoute = (s16) CalculateClosestSegmentOnExpandedRoute(ms_vExpandedRouteVerts, ms_iNumExpandedRouteVerts, vPedPos, m_fSplineTVal, &m_vClosestPosOnRoute, &ms_vClosestSegmentOnRoute);

		// Advance to minimum 'm_iClampMinProgress'
		while(ms_vExpandedRouteVerts[m_iClosestSegOnExpandedRoute].m_iProgress < m_iClampMinProgress && m_iClosestSegOnExpandedRoute < ms_iNumExpandedRouteVerts-1)
			m_iClosestSegOnExpandedRoute++;
	}
	else
	{
		m_iClosestSegOnExpandedRoute = 0;
		m_iProgress = 0;
		return false;
	}

	s32 iProgress = Min(ms_vExpandedRouteVerts[m_iClosestSegOnExpandedRoute].m_iProgress + 1, m_pRoute->GetSize() - 1);
	Assert(iProgress >= 0 && iProgress < 127);
	m_iProgress = (s8) iProgress;

	if (!GetNeverSlowDownForPathLength())
	{
		m_fMoveBlendRatio = AdjustMoveBlendRatioForRouteLength(m_fInitialMoveBlendRatio, fRouteLength);
	}

	return true;
}


CTask::FSM_Return CTaskNavBase::StateFollowingPath_OnUpdate(CPed * pPed)
{
	Assert(ms_iNumExpandedRouteVerts != 0);

	pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_LowPhysicsLodMayPlaceOnNavMesh, true);
	
	if (pPed->GetIsSwimming())
		pPed->SetPedResetFlag(CPED_RESET_FLAG_UseTighterTurnSettings, true);

	m_NavBaseInternalFlags.Set(CNavBaseInternalFlags::NB_HasPathed, true);

	// Keep try to stream in those anims if any
	if (m_pLadderClipRequestHelper)
		m_pLadderClipRequestHelper->RequestAllClipSets();

	if(m_NavBaseInternalFlags.IsSet(CNavBaseInternalFlags::NB_RestartNextFrame))
	{
		SetState(NavBaseState_Initial);
		return FSM_Continue;
	}

	if(m_NavBaseInternalFlags.IsSet(CNavBaseInternalFlags::NB_ForceRestartFollowPathState))
	{
		m_NavBaseInternalFlags.Set(CNavBaseInternalFlags::NB_ForceRestartFollowPathState, false);
		SetFlag(aiTaskFlags::RestartCurrentState);
		return FSM_Continue;
	}

#if __REQUEST_ROUTE_WHILST_FOLLOWING_PATH

	//----------------------------------------------------------------------------
	// Handle retrieving a path which has been pending whilst we follow the route
	// TODO: Most of this copied from StateMovingWhilstWaitingForPath_OnUpdate()
	// and as such should be copied into a shared function.

	if(m_NavBaseInternalFlags.IsSet(CNavBaseInternalFlags::NB_CalculatingPathOnTheFly) && m_iPathHandle != PATH_HANDLE_NULL)
	{
		EPathServerRequestResult ret = CPathServer::IsRequestResultReady(m_iPathHandle, EPath);

		if(ret == ERequest_Ready)
		{
			m_NavBaseInternalFlags.Reset(CNavBaseInternalFlags::NB_CalculatingPathOnTheFly);

			TPathResultInfo pathResultInfo;

			Assert(!m_pNextRouteSection);

			if(m_pNextRouteSection)
				m_pNextRouteSection->Clear();
			else
				m_pNextRouteSection = rage_new CNavMeshRoute();

			if(!RetrievePath(pathResultInfo, pPed, m_pNextRouteSection))
			{
				delete m_pNextRouteSection;
				m_pNextRouteSection = NULL;

				return OnFailedRoute(pPed, pathResultInfo);
			}
		}
		else if(ret == ERequest_NotFound)
		{
			AI_OPTIMISATIONS_OFF_ONLY( Assert(false); )
			BANK_ONLY( if(ms_bDebugRequests) Displayf("%u) Ped 0x%p entering NavBaseState_StationaryWhilstWaitingForPath, from StateFollowingPath_OnUpdate (path request not found)", fwTimer::GetFrameCount(), pPed); )

			m_NavBaseInternalFlags.Reset(CNavBaseInternalFlags::NB_CalculatingPathOnTheFly);
			SetState(NavBaseState_StationaryWhilstWaitingForPath);
			return FSM_Continue;
		}
	}

	//--------------------------------------------------------
	// Determine if we are ready to switch to our new route?

	if(m_pNextRouteSection)
	{
		SwitchToNextRoute();
		if(OnSuccessfulRoute(pPed))
		{
			SetFlag(aiTaskFlags::RestartCurrentState);
			return FSM_Continue;
		}
		else
		{
			SetState(NavBaseState_WaitingAfterFailureToObtainPath);
			return FSM_Continue;
		}
	}


#endif // __REQUEST_ROUTE_WHILST_FOLLOWING_PATH

	if(m_pRoute && IsSpecialActionClimbOrDrop(m_pRoute->GetPossiblySetSpecialActionFlags()))
	{
		// TODO: We should probably look at each route just once and see if it even contains
		// any of these points, so we don't have to do it for each update, in most cases.

		const int routeSize = m_pRoute->GetSize();
		for(s32 i=m_iProgress; i<routeSize; i++)
		{
			if( IsSpecialActionClimbOrDrop( m_pRoute->GetWaypoint(i).GetSpecialActionFlags() ) )
			{
				// Prevent ped entering low-lod physics if we are approaching a waypoint which prohibits it
				pPed->GetPedAiLod().SetBlockedLodFlag( CPedAILod::AL_LodPhysics );
				break;
			}
		}

		// Check for the possibility of having missed a climb point, and being stuck running into geometry
		// Note: an assumption that is made here is that ScanForMissedClimb() can't return true
		// unless IsSpecialActionClimbOrDrop() returns true for any of the points on the path.
		if( ScanForMissedClimb(pPed) )
		{
			return FSM_Continue;
		}
	}

	CTask* pSubTask = GetSubTask();
	if(!pSubTask)
	{
#if NAVMESH_OPTIMISATIONS_OFF
		Vector3 pos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
		Assertf(false, "ped 0x%p at (%.1f, %.1f, %.1f) was left in navmesh state FollowingPath, but with no subtask (task will restart)", pPed, pos.x, pos.y, pos.z);
#endif
		m_NavBaseConfigFlags.Reset(CNavBaseConfigFlags::NB_KeepMovingWhilstWaitingForPath);
		SetState(NavBaseState_Initial);
		return FSM_Continue;
	}

	const Vec3V pedPosV = pPed->GetMatrixRef().GetCol3();

	//---------------------------------------------------------------------------------
	// Detect if the ped has fallen off something and is now underneath their route

	static dev_float fFallenThresholdZ = 3.0f;
	if(!pPed->GetIsSwimming() && !pPed->GetCapsuleInfo()->IsBird() && !pPed->GetCapsuleInfo()->IsFish())
	{
		const float fDeltaZ = m_vClosestPosOnRoute.z - pedPosV.GetZf();
		if(fDeltaZ > fFallenThresholdZ)
		{
			// We set this flag so that ShouldKeepMovingWhilstWaitingForPath() will may return true
			m_NavBaseInternalFlags.Set(CNavBaseInternalFlags::NB_ExceededVerticalDistanceFromPath, true);

			SetState(NavBaseState_Initial);
			return FSM_Continue;
		}
	}

	bool bForceNextWptStep = false;
	bool bForceProceedToNextWaypoint = false;
	bool bForceCompleteRoute = false;
	const s32 iPreviousProgress = m_iProgress;
	

	//************************************************************************************************************
	// Check whether target radius from target has been reached
	// This needs to be done even if we are not on the last leg of the route, or our final point isn't our target

	if(m_NavBaseInternalFlags.IsSet(CNavBaseInternalFlags::NB_TargetIsDestination))	// Only if our target position is our destination (ie. not for fleeing or wandering paths)
	{
		// Code before optimization:
		//	const float fTargetHeightDelta = pPed->GetIsSwimming() ? 4.0f : 1.5f;//CTaskMoveGoToPoint::ms_fTargetHeightDelta;
		//	const Vector3 vDiff = m_vTarget - VEC3V_TO_VECTOR3(pedPosV);
		//	if(vDiff.XYMag2() < (m_fTargetRadius*m_fTargetRadius) && Abs(vDiff.z) < fTargetHeightDelta)

		const Vec4V constantsV(Vec::V4VConstant<
				FLOAT_TO_INT(4.0f*4.0f),		// Square of swimming target height delta
				FLOAT_TO_INT(1.5f*1.5f),		// Square of regular target height delta
				FLOAT_TO_INT(0.5f*0.5f),		// Underwater.
				FLOAT_TO_INT(0.0f)>());			// Unused

		ScalarV targetHeightDeltaV;
		if(pPed->GetIsSwimming() )
		{
			//! Special case for navigating to vehicle entry point. Don't assume we are there if we are 4m under ground!
			if(IsNavigatingUnderwater(pPed) && 
				pPed->GetPedResetFlag(CPED_RESET_FLAG_IsEnteringVehicle) && 
				pedPosV.GetZf() < m_vTarget.z)
			{
				targetHeightDeltaV = constantsV.GetZ();
			}
			else
			{
				targetHeightDeltaV = constantsV.GetX();
			}
		}
		else
		{
			targetHeightDeltaV = constantsV.GetY();
		}

		const Vec3V diffV = Subtract(VECTOR3_TO_VEC3V(m_vTarget), pedPosV);
		const Vec3V diffSqV = Scale(diffV, diffV);

		const ScalarV diffSqXYV = Add(diffSqV.GetX(), diffSqV.GetY());

		const ScalarV targetRadiusV = LoadScalar32IntoScalarV(m_fTargetRadius);
		const ScalarV targetRadiusSqV = Scale(targetRadiusV, targetRadiusV);

		const BoolV withinTargetXYV = IsLessThan(diffSqXYV, targetRadiusSqV);
		const BoolV withinTargetZV = IsLessThan(diffSqV.GetZ(), targetHeightDeltaV);	// Use the square we already computed rather than an Abs().
		const BoolV withinTargetV = And(withinTargetXYV, withinTargetZV);

		if(IsTrue(withinTargetV))
		{
			// Force entire navigation task to complete
			Assign(m_iProgress, m_pRoute->GetSize()-1);
			bForceCompleteRoute = true;
			m_pRoute->SetRouteFlags( m_pRoute->GetRouteFlags() | CNavMeshRoute::FLAG_LAST_ROUTE_POINT_IS_TARGET );
			//m_NavBaseInternalFlags.Set(CNavBaseInternalFlags::NB_LastRoutePointIsTarget, true);
		}
	}

	//***************************************
	// Update ped's progress along the route

	if(!bForceCompleteRoute)
	{
		const s32 iProgress = ms_vExpandedRouteVerts[m_iClosestSegOnExpandedRoute].m_iProgress + 1;

		// Never allow progress to go back to a progress which has a waypoint we've already completed
		if(iProgress < m_iProgress) // && IsSpecialActionClimbOrDrop(m_pRoute->GetWaypoint(iProgress).GetSpecialActionFlags()))
		{

		}
		else if( iProgress != m_iProgress )
		{
			if( iProgress > m_iProgress )
			{
				CalcTimeNeededToCompleteRouteSegment(pPed, iProgress);

				m_fCurrentTargetRadius = GetTargetRadiusForWaypoint(m_iProgress);
				((CTaskMove*)pSubTask)->SetTargetRadius(m_fCurrentTargetRadius);
			}

			Assert(iProgress >= 0 && iProgress < 127);
			m_iProgress = (s8) iProgress;
			bForceProceedToNextWaypoint = true;
		}

		// If we are approaching a drop-down then start scanning for a drop-down so that we may have the chance
		// to use the proper CTaskDropDown, instead of just running off the edge
#if __USE_DROPDOWN_TASK
		if( !bForceProceedToNextWaypoint && m_iClosestSegOnExpandedRoute+1 < ms_iNumExpandedRouteVerts &&
			ms_vExpandedRouteVerts[m_iClosestSegOnExpandedRoute+1].m_iWptFlag.IsDrop() )
		{
			const float fDistXY = MagXY(ms_vExpandedRouteVerts[m_iClosestSegOnExpandedRoute+1].GetPos() - pedPosV).Getf();
			static dev_float fDropDownDist = 1.0f;
			if( fDistXY < fDropDownDist )
			{
				pPed->SetPedResetFlag(CPED_RESET_FLAG_SearchingForDropDown, true);

				const u8 nFlags = CDropDownDetector::DSF_AsyncTests | CDropDownDetector::DSF_TestForDive;

				pPed->GetPedIntelligence()->GetDropDownDetector().Process(NULL, nFlags);

				if(pPed->GetPedIntelligence()->GetDropDownDetector().HasDetectedDropDown())
				{
					bForceProceedToNextWaypoint = true;
				}
			}
		}
#endif
	}

	//************************
	// Process path following

	// This will be set later
	bool bWithinApproxStoppingDist = false;
	bool bApproachingWaypoint = false;
	bool distToStopWithinStoppingDist = false;
	u32 iRetFlags = 0;
	ScalarV distanceRemainingV(V_FLT_MAX);

	if(!bForceProceedToNextWaypoint && !bForceCompleteRoute)
	{
		//*****************************************************************************
		// Handle case of ped swimming into geometry whilst trying to get out of water.
		// Detect this case and automatically create a climb event

		static dev_s32 iSwimClimbOutStaticCount = 5;

		// Ped is in water (at least partially), and their next waypoint requires for them to be out of water
		if( pPed->GetPedIntelligence()->GetEventScanner()->GetStaticMovementScanner().GetStaticCounter() >= iSwimClimbOutStaticCount &&
			(pPed->GetIsSwimming() || pPed->m_Buoyancy.GetStatus()!=NOT_IN_WATER) &&
			//(m_iProgress+1 < m_pRoute->GetSize()) &&
			((m_pRoute->GetWaypoint(m_iProgress).GetSpecialActionFlags()&WAYPOINT_FLAG_IS_ON_WATER_SURFACE)==0))
		{
			if( CreateClimbOutOfWaterEvent(pPed, RCC_VECTOR3(pedPosV)) )
			{
				SetState(NavBaseState_WaitingForWaypointActionEvent);
				return FSM_Continue;
			}
		}

		//**************************************************************************************
		// If we are stuck running into something, then (optionally?) generate a breakout event
		// TODO: Allow this behaviour to be switchable via a flag

		if(pPed->GetPedIntelligence()->GetEventScanner()->GetStaticMovementScanner().GetStaticCounter() >= CStaticMovementScanner::ms_iStaticCounterStuckLimit)
		{
			m_NavBaseConfigFlags.Reset(CNavBaseConfigFlags::NB_KeepMovingWhilstWaitingForPath);
			CEventStaticCountReachedMax staticEvent(GetTaskType());
			pPed->GetPedIntelligence()->AddEvent(staticEvent);
		}

		//****************************************************************************
		// If we are adaptively scaling the SetTarget() epsilon, then see how long it
		// has been since we rejected the last target.  If it is over some delta value
		// then we should check for a target update using the normal means.

		if(m_NavBaseConfigFlags.IsSet(CNavBaseConfigFlags::NB_UseAdaptiveSetTargetEpsilon) && m_NavBaseInternalFlags.IsSet(CNavBaseInternalFlags::NB_LastSetTargetWasNotUsed))
		{
			static const u32 iMaxNewTargetTimeDelta = 5000;
			const u32 iDeltaMs = fwTimer::GetTimeInMilliseconds() - m_iTimeOfLastSetTargetCall;
			if(iDeltaMs >= iMaxNewTargetTimeDelta)
			{
				m_NavBaseConfigFlags.Reset(CNavBaseConfigFlags::NB_UseAdaptiveSetTargetEpsilon);
				SetTarget(pPed, VECTOR3_TO_VEC3V(m_vNewTarget), false);
				m_NavBaseConfigFlags.Set(CNavBaseConfigFlags::NB_UseAdaptiveSetTargetEpsilon);
			}
		}

		//*******************************************************************************************************
		// If the task is set up to recalculate the path at regular intervals, then see if the timer has elapsed

		if(m_iRecalcPathFreqMs > 0)
		{
			m_iRecalcPathTimerMs -= (s16)GetTimeStepInMilliseconds();
			if(m_iRecalcPathTimerMs <= 0)
			{
				m_iRecalcPathTimerMs = m_iRecalcPathFreqMs;

				//*****************************************************************************
				// Setting this flag forces a target update, which in turn requests a new path

				m_NavBaseInternalFlags.Set(CNavBaseInternalFlags::NB_HadNewTarget, true);
			}
		}

		//*************************************************
		// Handle a new target being assigned to this task

		s32 iStateBefore = GetState();
		if(HandleNewTarget(pPed))
		{
			m_NavBaseInternalFlags.Reset(CNavBaseInternalFlags::NB_CheckedStoppingDistance);

			// Ensure that we break out of any stopping behaviour in the subtask, or we might end
			// up sliding the rest of the way to the new target.. (url:bugstar:1246800)
			if(pSubTask && pSubTask->GetTaskType()==CTaskTypes::TASK_MOVE_GO_TO_POINT_AND_STAND_STILL)
			{
				CTaskMoveGoToPointAndStandStill * pGotoTask = (CTaskMoveGoToPointAndStandStill*)pSubTask;
				if(pGotoTask->GetState() != CTaskMoveGoToPointAndStandStill::EGoingToPoint)
				{
					pGotoTask->RestartNextFrame();
				}
			}

			// Did HandleNewTarget() cause a state transition?
			if(iStateBefore != GetState())
			{
				return FSM_Continue;
			}
		}

//#if __REQUEST_ROUTE_WHILST_FOLLOWING_PATH
//			return FSM_Continue;
//#endif

		//********************************************************************************************
		// Process the timer which handles whether we've spent too long on the current route segment

		if(m_iTimeShouldSpendOnCurrentRouteSegmentMs != 0)
		{
			// values are uint16s, so we want to avoid overflow
			int iTimeSoFar = m_iTimeSpentOnCurrentRouteSegmentMs + GetTimeStepInMilliseconds();
			if(iTimeSoFar > 65535)
				iTimeSoFar = 65535;
			m_iTimeSpentOnCurrentRouteSegmentMs = (u16)iTimeSoFar;

			if(m_iTimeSpentOnCurrentRouteSegmentMs >= m_iTimeShouldSpendOnCurrentRouteSegmentMs)
			{
				if(pSubTask->MakeAbortable( ABORT_PRIORITY_URGENT, NULL))
				{
					SetState(NavBaseState_Initial);
					return FSM_Continue;
				}
			}
		}

		if( ms_bScanForObstructions && m_NavBaseConfigFlags.IsSet(CNavBaseConfigFlags::NB_ScanForObstructions) )
		{
			if( m_NavBaseConfigFlags.IsSet(CNavBaseConfigFlags::NB_AvoidObjects) )
			{
				if( m_ScanForObstructionsTimer.IsOutOfTime() )
				{
					m_ScanForObstructionsTimer.Set( fwTimer::GetTimeInMilliseconds(), CalcScanForObstructionsFreq() );

					// Take this opportunity to detect whether we've left proximity of any plane we started our route inside
					if( m_NavBaseInternalFlags.IsSet(CNavBaseInternalFlags::NB_PathStartedInsidePlaneVehicle) )
					{
						ScanForMovedOutsidePlaneBounds(pPed);
					}

					// Scan ahead along the route for obstructions
					float fDistanceToObstruction;
					if( ScanForObstructions(pPed, ms_fScanForObstructionsDistance, fDistanceToObstruction ) )
					{
#if __BANK && AI_OPTIMISATIONS_OFF
						static dev_bool bNotifyOnRepath = false;
						if(bNotifyOnRepath)
						{
							const Vector3 vPedPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
							Displayf("+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+\n");
							Displayf("Ped 0x%x at (%.1f, %.1f, %.1f) has repathed round obstacle\n", pPed, vPedPos.x, vPedPos.y, vPedPos.z);
							Displayf("+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+\n");
						}
#endif

						// If we are currently in the middle of calculating a new path on-the-fly, then
						// don't bother responding to this obstruction: it is most likely that the new path
						// will already deal with it when it is ready.
						if(m_iPathHandle == PATH_HANDLE_NULL
							&& !m_pNextRouteSection
							&& !m_NavBaseInternalFlags.IsSet(CNavBaseInternalFlags::NB_CalculatingPathOnTheFly))
						{
							// If the distance to the obstruction is sufficiently far away, calculate new route whilst following this one

							if(fDistanceToObstruction > ms_fDistanceAheadForSetNewTargetObstruction)
							{
								BANK_ONLY( if(ms_bDebugRequests) Displayf("%u) RequestPath() : Ped 0x%p on-the-fly, due to ScanForObstruction()", fwTimer::GetFrameCount(), pPed); )

								m_NavBaseInternalFlags.Set(CNavBaseInternalFlags::NB_OnTheFlyPathStartedAtRouteEnd, true);
								RequestPath(pPed, Max(fDistanceToObstruction - 1.0f, 0.0f));
							}
							// Otherwise we must stop and search for a new path
							else
							{
								BANK_ONLY( if(ms_bDebugRequests) Displayf("%u) RequestPath() : Ped 0x%p stationary, due to ScanForObstruction()", fwTimer::GetFrameCount(), pPed); )
								BANK_ONLY( if(ms_bDebugRequests) Displayf("%u) Ped 0x%p entering NavBaseState_StationaryWhilstWaitingForPath, from StateFollowingPath_OnUpdate (impending obstruction)", fwTimer::GetFrameCount(), pPed); )

								RequestPath(pPed);
								SetState(NavBaseState_StationaryWhilstWaitingForPath);
								return FSM_Continue;
							}
						}
					}
				}
			}
		}

		//***************************************************************************************************************
		// Determine if we are reaching the end of entire route, or are approaching a waypoint which must be stopped for
		// by comparing the ped's animated stopping distance with the distance left to this event.

		const ScalarV stoppingDistV(V_TEN /* 10.0f */);	// some value which is longer than any actual move-stop animation
		const ScalarV noEdgeAvoidanceDistV(V_TWO);

#if !__MAY_WALKSTOP_OVER_MULTIPLE_ROUTE_SEGMENTS
		ScalarV lengthOfFinalSegmentV;
		ScalarV* lengthOfFinalSegmentPtr = &lengthOfFinalSegmentV;
#else
		ScalarV* lengthOfFinalSegmentPtr = NULL;
#endif
		ScalarV distRemainingToClimbWptV(V_FLT_MAX);
		ScalarV distRemainingToDropWptV(V_FLT_MAX);

		bool bHaltAtClimbs = m_NavBaseConfigFlags.IsSet(CNavBaseConfigFlags::NB_DontStopForClimbs) ? false : true;
		if (pPed->GetWaterLevelOnPed() > 0.33f)	// Yeah.. Some peds are having issues exiting pools
			bHaltAtClimbs = true;

		Vec3V stopAtPosV;	// Note: should always be getting a value by GetPositionAheadOnRoute().

		Assert(ms_iNumExpandedRouteVerts != 0);

		// See whether we are in range of the end of route, or a waypoint which requires stopping for
		iRetFlags = GetPositionAheadOnRoute(ms_vExpandedRouteVerts, ms_iNumExpandedRouteVerts, 
			pedPosV, stoppingDistV, stopAtPosV, 
			distanceRemainingV, lengthOfFinalSegmentPtr, bHaltAtClimbs, false, &distRemainingToClimbWptV, &distRemainingToDropWptV);

		bWithinApproxStoppingDist = ((iRetFlags&(GPAFlag_HitWaypointFlag | GPAFlag_HitRouteEnd)) != 0);
		bApproachingWaypoint = ((iRetFlags&GPAFlag_HitWaypointFlag) != 0);

		if (iRetFlags & GPAFlag_HitWaypointFlagPullOut)
		{
			pPed->SetPedResetFlag(CPED_RESET_FLAG_UseTighterTurnSettings, true);
		}


		const Vec2V velV(VECTOR3_TO_INTRIN(pPed->GetVelocity()));
		const ScalarV XYSpeedV = Mag(velV);

		if(IsGreaterThanAll(XYSpeedV, ScalarV(V_ZERO)))
		{
			const ScalarV TValueV = InvScale(Min(distRemainingToClimbWptV, distanceRemainingV), XYSpeedV);
			const ScalarV maxAvoidanceTValueV = InvScale(LoadScalar32IntoScalarV(CTaskMoveGoToPoint::ms_fAvoidanceScanAheadDistance), XYSpeedV);
			if(pSubTask->GetTaskType()==CTaskTypes::TASK_MOVE_GO_TO_POINT_AND_STAND_STILL)
			{
				CTaskMoveGoToPointAndStandStill * pGotoTask = (CTaskMoveGoToPointAndStandStill*)pSubTask;
				pGotoTask->SetMaxAvoidanceTValue(Min(maxAvoidanceTValueV, TValueV));
			}
			else if ( pSubTask->GetTaskType()==CTaskTypes::TASK_MOVE_GO_TO_POINT )
			{
				CTaskMoveGoToPoint * pGotoTask = (CTaskMoveGoToPoint*)pSubTask;
				pGotoTask->SetMaxAvoidanceTValue(Min(maxAvoidanceTValueV, TValueV));
			}
		}

		// If we are close approaching a climb/drop then turn off navmesh edge avoidance, since we'll need to leave the navmesh
		if( ((iRetFlags & GPAFlag_UpcomingClimbOrDrop)!=0) && (IsLessThanAll(distRemainingToClimbWptV, noEdgeAvoidanceDistV) || IsLessThanAll(distRemainingToDropWptV, noEdgeAvoidanceDistV)) )
		{
			pPed->SetPedResetFlag(CPED_RESET_FLAG_DisableSteeringAroundNavMeshEdges, true);
		}

		const Vec3V pedPosToStopAtPosV = Subtract(stopAtPosV, pedPosV);
		const ScalarV flatDistToStopAtPosSqV(MagSquared(Vec2V(pedPosToStopAtPosV.GetX(), pedPosToStopAtPosV.GetY())));

		// So we are close to that waypoint then we must try to nail it or we might miss it as we don't got swimstops
		// We also do this for ladders for more "seemless/quicker" get ons

		// Also, be less sensitive to stopping at waypoints whilst in water
		if (((pPed->GetIsSwimming() || (iRetFlags&GPSFlag_HitWaypointFlagLadder) /* = bCloseToLadderNode*/)) ||
			((iRetFlags & GPAFlag_HitWaypointFlag) && pPed->GetWaterLevelOnPed() > 0.33f))
		{
			if(IsLessThanAll(flatDistToStopAtPosSqV, ScalarV(0.3f)))
			{
				bForceProceedToNextWaypoint = true;
				bForceNextWptStep = true;
			}
		}

		const ScalarV currentTargetRadiusV = LoadScalar32IntoScalarV(m_fCurrentTargetRadius);
		const ScalarV currentTargetRadiusSqV = Scale(currentTargetRadiusV, currentTargetRadiusV);

		// Incase we got a huge radius we must make sure we are not stopping for a waypoint, if so, we would slide all the way to it
		distToStopWithinStoppingDist = !bApproachingWaypoint && bWithinApproxStoppingDist && IsLessThanAll(flatDistToStopAtPosSqV, currentTargetRadiusSqV);

		// If our subtask is CTaskMoveGoToPointAndStandStill, then we must tell it how much distance is left before we need to stop
		// Also if we are unable to play a stop animation we should instruct the subtask to slow approaching the target instead.
		if(pSubTask->GetTaskType()==CTaskTypes::TASK_MOVE_GO_TO_POINT_AND_STAND_STILL)
		{
			CTaskMoveGoToPointAndStandStill * pGotoTask = (CTaskMoveGoToPointAndStandStill*)pSubTask;

			// If we're stopping & are within target radius, then we're as good as done
			if(distToStopWithinStoppingDist)
			{
				const ScalarV distRemainingZeroThresholdV(0.05f);
				distanceRemainingV = Sqrt(flatDistToStopAtPosSqV);
				distanceRemainingV = SelectFT(IsLessThan(distanceRemainingV, distRemainingZeroThresholdV), distanceRemainingV, ScalarV(V_ZERO));
				//	if(fDistanceRemaining < 0.05f)
				//		fDistanceRemaining = 0.0f;
			}

#if !__MAY_WALKSTOP_OVER_MULTIPLE_ROUTE_SEGMENTS

			if(bWithinApproxStoppingDist && m_NavBaseConfigFlags.IsSet(CNavBaseConfigFlags::NB_StopExactly) && pGotoTask->GetStopExactly()
				&& !m_NavBaseInternalFlags.IsSet(CNavBaseInternalFlags::NB_CheckedStoppingDistance))
			{
				FastAssert(pPed->GetCurrentMotionTask());
				m_fAnimatedStoppingDistance = pPed->GetCurrentMotionTask()->GetStoppingDistance();
				if(m_fAnimatedStoppingDistance > lengthOfFinalSegmentV.Getf())
				{
					pGotoTask->SetUseRunstopInsteadOfSlowingToWalk(false);
					pGotoTask->SetSlowDownDistance(m_fSlowDownDistance > 0.0f ? m_fSlowDownDistance : CTaskMoveGoToPointAndStandStill::ms_fSlowDownDistance);
					pGotoTask->SetStopExactly(false);
				}
				m_NavBaseInternalFlags.Set(CNavBaseInternalFlags::NB_CheckedStoppingDistance, true);
			}

#endif // !__MAY_WALKSTOP_OVER_MULTIPLE_ROUTE_SEGMENTS

			pGotoTask->SetDistanceRemaining(distanceRemainingV);
		}

#if __MAY_WALKSTOP_OVER_MULTIPLE_ROUTE_SEGMENTS
		// Obtain the exact distance which this ped will take to stop
		if(bWithinApproxStoppingDist && m_NavBaseConfigFlags.IsSet(CNavBaseConfigFlags::NB_StopExactly) &&
			!m_NavBaseInternalFlags.IsSet(CNavBaseInternalFlags::NB_CheckedStoppingDistance))
		{
			float fStopHeading = m_fTargetStopHeading != DEFAULT_NAVMESH_FINAL_HEADING ? m_fTargetStopHeading : pPed->GetCurrentHeading();
			m_fAnimatedStoppingDistance = pPed->GetCurrentMotionTask()->GetStoppingDistance(fStopHeading);
			m_NavBaseInternalFlags.Set(CNavBaseInternalFlags::NB_CheckedStoppingDistance, true);
		}
#endif

		// When reaching the end of our route it is essential that we have an update every frame, or we will
		// get peds running into/through collision geometry.
		if( CPedAILodManager::IsTimeslicingEnabled() )
		{
			const ScalarV animatedStoppingDistanceV = LoadScalar32IntoScalarV(m_fAnimatedStoppingDistance);

			// m_fAnimatedStoppingDistance will always be 0.0 for ped running low LOD motion tasks..
			// Old float version:
			//	if( m_NavBaseInternalFlags.IsSet(CNavBaseInternalFlags::NB_CheckedStoppingDistance) &&
			//		fDistanceRemaining < m_fAnimatedStoppingDistance)
			if( m_NavBaseInternalFlags.IsSet(CNavBaseInternalFlags::NB_CheckedStoppingDistance) &&
				IsLessThanAll(distanceRemainingV, animatedStoppingDistanceV))
			{
				pPed->GetPedAiLod().SetForceNoTimesliceIntelligenceUpdate(true);
			}
			// ..in which case we will use some arbitrary distance which we hope will be enough to trigger despite the low frequency task updates
			// Old float version:
			//	else if( m_fAnimatedStoppingDistance==0.0f && fDistanceRemaining < 2.0f )
			else if( IsTrue(And(IsEqual(animatedStoppingDistanceV, ScalarV(V_ZERO)), IsLessThan(distanceRemainingV, ScalarV(V_TWO)))))
			{
				pPed->GetPedAiLod().SetForceNoTimesliceIntelligenceUpdate(true);
			}
			// We also have to suppress timeslicing when we are approaching the end of a route, and we
			// intend to calculate the next route section 'on-the-fly'; if timeslicing is enabled here then
			// we will miss the opportunity to issue the path request in enough time, resulting in the ped
			// transitioning to stationary.
			else if( !GetLastRoutePointIsTarget() && (!m_NavBaseInternalFlags.IsSet(CNavBaseInternalFlags::NB_CalculatingPathOnTheFly)) &&
				IsLessThan(distanceRemainingV, ScalarV(ms_fDistanceFromRouteEndToRequestNextSection) + ScalarV(V_TWO)).Getb() )
			{
				pPed->GetPedAiLod().SetForceNoTimesliceIntelligenceUpdate(true);
			}
		}

		//*************************************************************************
		// Calculate distances and times to look ahead from current route position

#if !__MAY_WALKSTOP_OVER_MULTIPLE_ROUTE_SEGMENTS
		static dev_float fMinAngleDeltaForLos = 2.0f * DtoR;

		Vector3 vGotoPointTarget;
		ProcessLookAhead(pPed, bWithinStoppingDist, vGotoPointTarget);
		((CTaskMove*)pSubTask)->SetTarget(pPed, vGotoPointTarget);

		if( pPed->GetPedAiLod().IsLodFlagSet( CPedAILod::AL_LodPhysics ) == false && !pPed->IsStrafing() )
		{
			const float fAngleToGotoTarget = fwAngle::GetRadianAngleBetweenPoints(vGotoPointTarget.x, vGotoPointTarget.y, vPedPos.x, vPedPos.y);
			const float fPedHeading = pPed->GetCurrentHeading();
			const float fAbsDelta = Abs( SubtractAngleShorter(fAngleToGotoTarget, fPedHeading) );
			if( fAbsDelta > fMinAngleDeltaForLos )
			{
				pPed->GetNavMeshTracker().SetPerformLosAheadLineTest(true);
			}
		}

#else	// !__MAY_WALKSTOP_OVER_MULTIPLE_ROUTE_SEGMENTS

		const bool bSubtaskIsStopping =
			pSubTask->GetTaskType()==CTaskTypes::TASK_MOVE_GO_TO_POINT_AND_STAND_STILL &&
			((CTaskMoveGoToPointAndStandStill*)pSubTask)->GetIsStopping();

		if(!bSubtaskIsStopping)
		{
			Vector3 vGotoPointTarget, vLookAheadTarget;
			ProcessLookAhead(pPed, bWithinApproxStoppingDist, vGotoPointTarget, vLookAheadTarget);
			((CTaskMove*)pSubTask)->SetTarget(pPed, VECTOR3_TO_VEC3V(vGotoPointTarget));
			((CTaskMove*)pSubTask)->SetLookAheadTarget(pPed, vLookAheadTarget);

			if( pPed->GetPedAiLod().IsLodFlagSet( CPedAILod::AL_LodPhysics ) == false && !pPed->IsStrafing() )
			{
				if(iRetFlags & GPAFlag_HitWaypointFlagPullOut)
				{
					pPed->GetNavMeshTracker().SetPerformLosAheadLineTest(true);
				}
				else
				{
					// This used to be
					//	static dev_float fMinAngleDeltaForLos = 2.0f * DtoR;
					//	const float fAngleToGotoTarget = fwAngle::GetRadianAngleBetweenPoints(vLookAheadTarget.x, vLookAheadTarget.y, pedPosV.GetXf(), pedPosV.GetYf());
					//	const float fPedHeading = pPed->GetCurrentHeading();
					//	const float fAbsDelta = Abs( SubtractAngleShorter(fAngleToGotoTarget, fPedHeading) );
					//	if(fAbsDelta > fMinAngleDeltaForLos)
					// The new code should be equivalent, but much faster (no atan2(), etc):
					const Vec3V pedFwdV = pPed->GetMatrixRef().GetCol1();
					const Vec3V toTargetV = Subtract(VECTOR3_TO_VEC3V(vLookAheadTarget), pedPosV);
					const ScalarV squareSineAngleV(0.00121797487f);	// square(sinf(fMinAngleDeltaForLos))	fMinAngleDeltaForLos = 2.0f * DtoR;
					if(!sIsAngleBetweenFlattenedVectorsLessThan(pedFwdV, toTargetV, squareSineAngleV))
					{
						pPed->GetNavMeshTracker().SetPerformLosAheadLineTest(true);
					}
				}
			}

			//It is important to get frequent updates when we are running and deviate from our path, to avoid wobbling and over-correction.
			//However, this really only matters for on-screen peds.
			if(CPedAILodManager::IsTimeslicingEnabled() && !pPed->GetPedAiLod().IsBlockedFlagSet(CPedAILod::AL_LodTimesliceIntelligenceUpdate) &&
				!pPed->GetPedAiLod().GetForceNoTimesliceIntelligenceUpdate() && !pPed->GetMotionData()->GetIsLessThanRun() &&
				pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_VisibleOnScreen))
			{
				//After a lot of testing, it seems we can really only allow time slicing
				//in this situation when moving in the correct direction on a straight path.

				Vec3V vPedToGotoPointTarget = Subtract(VECTOR3_TO_VEC3V(vGotoPointTarget), pPed->GetTransform().GetPosition());
				Vec3V vPedToGotoPointTargetXY = vPedToGotoPointTarget;
				vPedToGotoPointTargetXY.SetZ(ScalarV(V_ZERO));
				Vec3V vPedToGotoPointTargetXYDirection = NormalizeFastSafe(vPedToGotoPointTargetXY, Vec3V(V_ZERO));

				Vec3V vPedForward = pPed->GetTransform().GetForward();
				Vec3V vPedForwardXY = vPedForward;
				vPedForwardXY.SetZ(ScalarV(V_ZERO));

				Vec3V vPedVelocity = VECTOR3_TO_VEC3V(pPed->GetVelocity());
				Vec3V vPedVelocityXY = vPedVelocity;
				vPedVelocityXY.SetZ(ScalarV(V_ZERO));
				Vec3V vPedXYDirection = NormalizeFastSafe(vPedVelocityXY, vPedForwardXY);

				ScalarV scDot = Dot(vPedXYDirection, vPedToGotoPointTargetXYDirection);
				static dev_float s_fMinDotForFacing = 0.98f;
				ScalarV scMinDot = ScalarVFromF32(s_fMinDotForFacing);
				if(IsLessThanAll(scDot, scMinDot))
				{
					pPed->GetPedAiLod().SetForceNoTimesliceIntelligenceUpdate(true);
				}
				else
				{
					Vec3V vGotoPointTargetToLookAheadTarget = Subtract(VECTOR3_TO_VEC3V(vLookAheadTarget), VECTOR3_TO_VEC3V(vGotoPointTarget));
					Vec3V vGotoPointTargetToLookAheadTargetXY = vGotoPointTargetToLookAheadTarget;
					vGotoPointTargetToLookAheadTargetXY.SetZ(ScalarV(V_ZERO));
					Vec3V vGotoPointTargetToLookAheadTargetXYDirection = NormalizeFastSafe(vGotoPointTargetToLookAheadTargetXY, Vec3V(V_ZERO));

					scDot = Dot(vPedToGotoPointTargetXYDirection, vGotoPointTargetToLookAheadTargetXYDirection);
					static dev_float s_fMinDotForLookAhead = 0.98f;
					scMinDot = ScalarVFromF32(s_fMinDotForLookAhead);
					if(IsLessThanAll(scDot, scMinDot))
					{
						pPed->GetPedAiLod().SetForceNoTimesliceIntelligenceUpdate(true);
					}
				}
			}
		}


		if (bSubtaskIsStopping)
		{
	//		m_fCurrentLookAheadDistance = 0.5f;

			// This is a hack to make the destination for points in shallow water more sane, and fixing odd sliding on the spot issues
			s32 iSeg = m_iClosestSegOnExpandedRoute;
			if ((ms_vExpandedRouteVerts[iSeg+1].m_iWptFlag.GetSpecialActionFlags() & WAYPOINT_FLAG_IS_ON_WATER_SURFACE) && pPed->GetWaterLevelOnPed() < 0.33f)
			{
				const Vec3V vecToWptV = Subtract(ms_vExpandedRouteVerts[iSeg+1].GetPos(), pedPosV);
				const ScalarV distToWptSqV = MagXYSquared(vecToWptV);
				const ScalarV minDistToWptSqV(0.36f);
				if (IsLessThanAll(distToWptSqV, minDistToWptSqV))
				{
					ScalarV newZV = ms_vExpandedRouteVerts[iSeg+1].m_vPosAndSplineT.GetZ() + ScalarV(pPed->GetCapsuleInfo()->GetGroundToRootOffset());
					ms_vExpandedRouteVerts[iSeg+1].m_vPosAndSplineT.SetZ(newZV);
					ms_vExpandedRouteVerts[iSeg+1].m_iWptFlag.m_iSpecialActionFlags = ms_vExpandedRouteVerts[iSeg+1].m_iWptFlag.m_iSpecialActionFlags & (~WAYPOINT_FLAG_IS_ON_WATER_SURFACE);
				}
			}

			Vector3 vGotoPointTarget, vLookAheadTarget;
			ProcessLookAhead(pPed, bWithinApproxStoppingDist, vGotoPointTarget, vLookAheadTarget);
			((CTaskMove*)pSubTask)->SetTarget(pPed, VECTOR3_TO_VEC3V(vGotoPointTarget));
			((CTaskMove*)pSubTask)->SetLookAheadTarget(pPed, vLookAheadTarget);

			bool bLocomotionTaskIsStopping = false;
			// Only do this if stopping
			const CTaskMotionBase* pTask = pPed->GetCurrentMotionTask();
			
			if (pTask)
			{
				if (pTask->GetTaskType() == CTaskTypes::TASK_HUMAN_LOCOMOTION)
				{
					const CTaskHumanLocomotion* pHumanLocomotion = static_cast<const CTaskHumanLocomotion*>(pTask);
					bLocomotionTaskIsStopping = pHumanLocomotion->IsFullyInStop() || pHumanLocomotion->IsTryingToStop() || pTask->GetState() == CTaskHumanLocomotion::State_Stop;
				}
				else if (pTask->GetTaskType() == CTaskTypes::TASK_ON_FOOT_QUAD)
				{
					bLocomotionTaskIsStopping = pTask->GetState() == CTaskQuadLocomotion::State_StopLocomotion;
				}
			}


			if(bLocomotionTaskIsStopping)	// Start rotate as we blend also
			{	
				const bool bSubtaskIsRunStop =
					pSubTask->GetTaskType()==CTaskTypes::TASK_MOVE_GO_TO_POINT_AND_STAND_STILL &&
					((CTaskMoveGoToPointAndStandStill*)pSubTask)->GetIsRunStop();

				
				if (CPedMotionData::GetIsWalking(m_fMoveBlendRatio) && !bSubtaskIsRunStop)
				{
				}
				else
				{
					Vector3 vDist = vGotoPointTarget - VEC3V_TO_VECTOR3(pedPosV);


					// fix for heading issues, taken from CL 3047955					
					// Current heading is not interpolated towards desired whilst ped is stopping
					// so we manually achieve this by applying extra heading change

					const float fCurrentHeading = pPed->GetMotionData()->GetCurrentHeading();

					if (vDist.XYMag2() > 0.4f * 0.4f)
					{
						static dev_float fHeadingSpeed = 6.0f;
						// static dev_float fHeadingSpeedFleeFixup = 2.0f;
						float fModHeadingSpeed = fHeadingSpeed;
						const float fTargetHeading = fwAngle::LimitRadianAngleFast( fwAngle::GetRadianAngleBetweenPoints( vGotoPointTarget.x, vGotoPointTarget.y, pedPosV.GetXf(), pedPosV.GetYf() ) );
						const float fHeadingDelta = SubtractAngleShorter( fTargetHeading, fCurrentHeading );
						pPed->GetMotionData()->SetDesiredHeading( fTargetHeading );
						pPed->GetMotionData()->SetExtraHeadingChangeThisFrame( fHeadingDelta * fModHeadingSpeed * fwTimer::GetTimeStep() );
					}
					else
					{
						static dev_float fHeadingSpeed = 4.0f;
						const float fTargetHeading = pPed->GetDesiredHeading();
						float fHeadingDelta = SubtractAngleShorter( fTargetHeading, fCurrentHeading ) / fwTimer::GetTimeStep();
						if (fHeadingDelta > 0.f)
							fHeadingDelta = Min(fHeadingSpeed, fHeadingDelta);
						else
							fHeadingDelta = Max(-fHeadingSpeed, fHeadingDelta);
						
						pPed->GetMotionData()->SetExtraHeadingChangeThisFrame( fHeadingDelta * fwTimer::GetTimeStep() );
					}
				}
			}
		}

#endif // !__MAY_WALKSTOP_OVER_MULTIPLE_ROUTE_SEGMENTS

		//***************************************************
		// Set the bFollowingRoute reset flag if appropriate

		pPed->SetPedResetFlag( CPED_RESET_FLAG_FollowingRoute, true );

		//*********************************************************************************
		// If we have overshot the route end, identify this case and increment the progress
		// This can happen because we cannot slide the goto target past the route end, and
		// thus risk circling the target position if we don't achieve the target radius.

		const int routeSize = m_pRoute->GetSize();
		const int expandedSize = ms_iNumExpandedRouteVerts;
		if(	m_iProgress+1 >= routeSize && routeSize >= 2 &&
			m_iClosestSegOnExpandedRoute + 2 >= expandedSize && expandedSize >= 2 && 
			IsLessThanAll(distanceRemainingV, ScalarV(ms_DefaultPathDistanceToExpand)))
		{
			// Old Vector3/float version of the code:
			//	const Vector3 & v1 = m_pRoute->GetPosition(routeSize-2);
			//	const Vector3 & v2 = m_pRoute->GetPosition(routeSize-1);
			//	Vector3 vSeg = v2 - v1;
			//	vSeg.Normalize();
			//	const float fD = - DotProduct( vSeg, v2 );
			//	const float fPlanarDist = DotProduct(VEC3V_TO_VECTOR3(pedPosV), vSeg) + fD;
			//	if( -fPlanarDist < m_fCurrentTargetRadius )
			// 
			// The new version basically has the math rearranged a bit, allowing us to
			// eliminate the square root and division in the normalization, by basically
			// squaring on both sides of the inequality, taking into account that -fPlanarDist
			// can be negative.

			const Vec3V seg1V = ms_vExpandedRouteVerts[expandedSize-2].GetPos();
			const Vec3V seg2V = ms_vExpandedRouteVerts[expandedSize-1].GetPos();

			const Vec3V	segDeltaV = Subtract(seg2V, seg1V);
			const Vec3V pedPosToSeg2V = Subtract(seg2V, pedPosV);
			const ScalarV dotV = Dot(segDeltaV, pedPosToSeg2V);
			const ScalarV zeroV(V_ZERO);
			const ScalarV dotSqV = Scale(dotV, dotV);
			const ScalarV currentTargetRadiusSqV = Scale(currentTargetRadiusV, currentTargetRadiusV);
			const ScalarV segLenSqV = MagSquared(segDeltaV);
			const ScalarV thresholdSqV = Scale(currentTargetRadiusSqV, segLenSqV);
			const BoolV dotLessThanZeroV = IsLessThan(dotV, zeroV);
			const BoolV dotLessThanThresholdV = IsLessThan(dotSqV, thresholdSqV);
			const BoolV planarDistWithinTargetV = Or(dotLessThanZeroV, dotLessThanThresholdV);

			if(IsTrue(planarDistWithinTargetV))
			{
				if(pSubTask->GetTaskType()==CTaskTypes::TASK_MOVE_GO_TO_POINT_AND_STAND_STILL)
				{
					CTaskMoveGoToPointAndStandStill * pGotoTask = (CTaskMoveGoToPointAndStandStill*)pSubTask;
					if (m_NavBaseConfigFlags.IsSet(CNavBaseConfigFlags::NB_StopExactly))
					{
						pGotoTask->ResetDistanceRemaining(pPed); // So we can slide that last bit
					}
					else
					{
						pGotoTask->SetDistanceRemaining(0.0f);
					}
				}

				bForceCompleteRoute = true;
			}
		}
	}

	if(!distToStopWithinStoppingDist)
	{
		if(pSubTask->GetTaskType()==CTaskTypes::TASK_MOVE_GO_TO_POINT_AND_STAND_STILL)
		{
			((CTaskMoveGoToPointAndStandStill*)pSubTask)->SetDontCompleteThisFrame();
		}
		else if(pSubTask->GetTaskType()==CTaskTypes::TASK_MOVE_GO_TO_POINT)
		{
			((CTaskMoveGoToPoint*)pSubTask)->SetDontCompleteThisFrame();
		}
		else
		{
			Assert(false);
		}
	}



	//*******************************************************************************************
	// Save state information about the closest point on the current route.  This used to be set
	// directly into the running CTaskMoveGoToPoint task, but it was problematic to set this if
	// a new task was set this frame (especially in the case of CTaskMoveGoToPointAndStandStill,
	// which wont even have a CTaskMoveGoToPoint subtask until it is run.

	pPed->GetPedIntelligence()->SetClosestPosOnRoute(m_vClosestPosOnRoute, ms_vClosestSegmentOnRoute);

	//***********************************************************************************************************
	// Process when the goto subtask has finished.
	// NB: The previous logic may have terminated the subtask, so don't enclose this as in 'else..' statement

	const bool bSubtaskFinished = GetIsFlagSet(aiTaskFlags::SubTaskFinished);
	if(bSubtaskFinished || bForceProceedToNextWaypoint || bForceCompleteRoute)
	{
		//*****************************************
		// Otherwise increment progress as normal

		if(!bForceCompleteRoute)
		{
			if( bSubtaskFinished && !bForceProceedToNextWaypoint )
			{
				m_iProgress++;
				((CTaskMove*)pSubTask)->SetTargetRadius(GetTargetRadiusForWaypoint(m_iProgress));
			}

			const s32 iNewProgress = m_iProgress;

			//************************************************************************************
			// Process special waypoints which may involve climbing, dropping, using ladders etc.
			// These are usually handled by way of an event.  We need to iterate from the last
			// progress to the current to avoid missing any actions in between.

			s32 iMaxProgress = Min((s32)m_iProgress, m_pRoute->GetSize());
			if (bForceNextWptStep && iPreviousProgress == iMaxProgress)
				iMaxProgress = Min(iMaxProgress + 2, m_pRoute->GetSize());

			for(s32 p=iPreviousProgress; p<iMaxProgress; p++)
			{
				const u32 iSpecialAction = m_pRoute->GetWaypoint(p).GetSpecialActionFlags();

				if(IsSpecialActionClimbOrDrop(iSpecialAction))
				{					
					// Prevent ped entering low-lod physics if we are approaching a waypoint which prohibits it
					pPed->GetPedAiLod().SetBlockedLodFlag( CPedAILod::AL_LodPhysics );

					Assert(p >= 0 && p < 127);
					m_iProgress = (s8) p;

					ASSERT_ONLY(bool bOk;)

					const EWaypointActionResult eWptActionResult = CreateWaypointActionEvent(pPed, p);

					switch(eWptActionResult)
					{
						case WaypointAction_EventCreated:
							SetState(NavBaseState_WaitingForWaypointActionEvent);
							return FSM_Continue;

						case WaypointAction_ActionFailed:
							SetState(NavBaseState_WaitingAfterFailedWaypointAction);
							return FSM_Continue;

						case WaypointAction_EventNotNeeded:
						case WaypointAction_ActionNotFound:
							Assert(iNewProgress >= 0 && iNewProgress < 127);
							m_iProgress = (s8) iNewProgress;
							ASSERT_ONLY(bOk =) CreateRouteFollowingTask(pPed);
							Assert(bOk);
							return FSM_Continue;

						case WaypointAction_StateTransitioned:
							return FSM_Continue;
					}
				}
			}
		}
		else
		{
			// Force route completion
			s32 iProgress = m_pRoute->GetSize();
			Assert(iProgress >= 0 && iProgress < 127);
			m_iProgress = (s8) iProgress;
		}

		if( m_iProgress >= m_pRoute->GetSize() || (bSubtaskFinished && bWithinApproxStoppingDist) )
		{
			//if(m_NavBaseInternalFlags.IsSet(CNavBaseInternalFlags::NB_LastRoutePointIsTarget))
			if(GetLastRoutePointIsTarget())
			{
#if __REQUEST_ROUTE_WHILST_FOLLOWING_PATH
				// We may have reached out subtask's destination at the same time as having received a
				// new target; if we are calculating our path on the fly then we need to restart.
				if(m_NavBaseInternalFlags.IsSet(CNavBaseInternalFlags::NB_CalculatingPathOnTheFly))
				{
					BANK_ONLY( if(ms_bDebugRequests) Displayf("%u) Ped 0x%p entering NavBaseState_StationaryWhilstWaitingForPath, from StateFollowingPath_OnUpdate (reached end of route whilst repathing on-the-fly)", fwTimer::GetFrameCount(), pPed); )

					Assert(!m_pNextRouteSection);
					if(m_pNextRouteSection)
						delete m_pNextRouteSection;
					m_pNextRouteSection = NULL;

					Assert(m_iPathHandle != PATH_HANDLE_NULL);
					//SetState(NavBaseState_StationaryWhilstWaitingForPath);
					SetState(NavBaseState_MovingWhilstWaitingForPath);
					return FSM_Continue;
				}
#endif
				if(m_NavBaseConfigFlags.IsSet(CNavBaseConfigFlags::NB_SlideToCoordAtEnd))
				{
					SetState(NavBaseState_SlideToCoord);
					return FSM_Continue;
				}
				else
				{
					if (pSubTask && pSubTask->GetTaskType()==CTaskTypes::TASK_MOVE_GO_TO_POINT_AND_STAND_STILL)
					{
						CTaskMoveGoToPointAndStandStill* pTask = static_cast<CTaskMoveGoToPointAndStandStill*>(pSubTask);
						if (pTask->IsStoppedAtTargetPoint())
						{
							//pPed->GetMotionData()->SetDesiredHeading( CalculateFinalHeading(pPed) );

							return FSM_Quit;
						}
						else
						{
							return DecideStoppingState(pPed);
						}
					}
					else
					{
						return FSM_Quit;
					}
				}
			}
			else
			{
				if(m_pNextRouteSection)
				{
					SwitchToNextRoute();
					if(OnSuccessfulRoute(pPed))
					{
						SetFlag(aiTaskFlags::RestartCurrentState);
						return FSM_Continue;
					}
					else
					{
						SetState(NavBaseState_WaitingAfterFailureToObtainPath);
						return FSM_Continue;
					}
				}
				else
				{
					BANK_ONLY( if(ms_bDebugRequests) Displayf("%u) RequestPath() : Ped 0x%p stationary, due to reaching route end with no m_pNextRouteSection", fwTimer::GetFrameCount(), pPed); )

					RequestPath(pPed);

					bool bMayUsePreviousTarget = true;
					m_NavBaseInternalFlags.Set(CNavBaseInternalFlags::NB_MayUsePreviousTargetInMovingWaitingForPath, true);

					if(ShouldKeepMovingWhilstWaitingForPath(pPed, bMayUsePreviousTarget))
					{
						m_NavBaseInternalFlags.Set(CNavBaseInternalFlags::NB_MayUsePreviousTargetInMovingWaitingForPath, bMayUsePreviousTarget);

						SetState(NavBaseState_MovingWhilstWaitingForPath);
						return FSM_Continue;
					}
					else
					{
						BANK_ONLY( if(ms_bDebugRequests) Displayf("%u) Ped 0x%p entering NavBaseState_StationaryWhilstWaitingForPath, from StateFollowingPath_OnUpdate (reached end of route but we were not on-the-fly repathing && ShouldKeepMovingWhilstWaitingForPath returned false)", fwTimer::GetFrameCount(), pPed); )

						SetState(NavBaseState_StationaryWhilstWaitingForPath);
						return FSM_Continue;
					}
				}
			}
		}
	}

#if __REQUEST_ROUTE_WHILST_FOLLOWING_PATH

	//-------------------------------------------------------------------------------------------------
	// If we are within 'ms_fDistanceFromRouteEndToRequestNextSection' from the end of our route, and
	// the current route does not take us to our target, then request another path 'on-the-fly'

	const bool bWithinEndRequestDistance =
		(distanceRemainingV.Getf() < GetRouteDistanceFromEndForNextRequest()) ||
			(!GetLastRoutePointIsTarget() && distanceRemainingV.Getf() < ms_fDistanceFromRouteEndToRequestNextSection);

	if(m_iPathHandle == PATH_HANDLE_NULL
		&& !m_pNextRouteSection
		&& !m_NavBaseInternalFlags.IsSet(CNavBaseInternalFlags::NB_CalculatingPathOnTheFly)
		&& !m_NavBaseConfigFlags.IsSet(CNavBaseConfigFlags::NB_DisableCalculatePathOnTheFly)
		&& bWithinEndRequestDistance
		&& !bApproachingWaypoint )
	{
		BANK_ONLY( if(ms_bDebugRequests) Displayf("%u) RequestPath() : Ped 0x%p on-the-fly following path, due to approaching end of current route section", fwTimer::GetFrameCount(), pPed); )

		m_NavBaseInternalFlags.Set(CNavBaseInternalFlags::NB_OnTheFlyPathStartedAtRouteEnd, true);
		RequestPath(pPed, 9999.0f);
	}
#endif

	// Gradually increase the look-ahead distance up to the maximum
	if(!pPed->IsStrafing())
	{
		const bool bSubtaskIsStopping =
			pSubTask->GetTaskType()==CTaskTypes::TASK_MOVE_GO_TO_POINT_AND_STAND_STILL &&
			((CTaskMoveGoToPointAndStandStill*)pSubTask)->GetIsStopping();

		const float fMinLookAheadDistance = (bSubtaskIsStopping ? 0.5f : GetInitialGotoTargetLookAheadDistance());
		const float fMaxLookAheadDistance = GetGotoTargetLookAheadDistance();
		const float fLookAheadInc = GetLookAheadDistanceIncrement();

		// JB: Turned this off again as it creates ugly swerving around objects (eg. url:bugstar:1085569)
		TUNE_GROUP_BOOL(PATH_FOLLOWING, bPerformObjectLookAheadLOS, false);

		// Only perform a dynamic object LOS if we have a clear navmesh LOS; to save processing time
		const bool bObjectLOS =
			(bPerformObjectLookAheadLOS && pPed->GetNavMeshTracker().GetWasLosAheadTestClear()) ? TestObjectLookAheadLOS(pPed) : true;

		float timeStep = GetTimeStep();
		// Peds party submerged in water might fail their LOS tests as they are below the navmesh
		if(!bSubtaskIsStopping && ((pPed->GetNavMeshTracker().GetWasLosAheadTestClear() && bObjectLOS) || pPed->GetWaterLevelOnPed() > 0.5f))
		{
			const float newUnclampedDist = m_fCurrentLookAheadDistance + fLookAheadInc * timeStep;
			m_fCurrentLookAheadDistance = Min(newUnclampedDist, fMaxLookAheadDistance);
		}
		else
		{
			const float newUnclampedDist = m_fCurrentLookAheadDistance - fLookAheadInc * timeStep;
			m_fCurrentLookAheadDistance = Max(newUnclampedDist, fMinLookAheadDistance);
		}
	}
	else
	{
		static dev_float fDistStrafing = 0.6f;
		static dev_float fDistStrafingTight = 0.3f;

		m_fCurrentLookAheadDistance = (iRetFlags & GPAFlag_HitWaypointFlagPullOut) ? fDistStrafingTight : fDistStrafing;
	}

	if (pSubTask && pSubTask->GetTaskType()==CTaskTypes::TASK_MOVE_GO_TO_POINT_AND_STAND_STILL)
	{
		if (((CTaskMoveGoToPointAndStandStill*)pSubTask)->GetIsStopDueToInsideRadius())
		{
			return DecideStoppingState(pPed);
		}
	}

	return FSM_Continue;
}


float CTaskNavBase::CalculateFinalHeading(CPed * pPed) const
{
	int iPt1, iPt2;
	if(m_iClosestSegOnExpandedRoute < ms_iNumExpandedRouteVerts)
	{
		iPt2 = m_iClosestSegOnExpandedRoute;
		iPt1 = m_iClosestSegOnExpandedRoute-1;
	}
	else
	{
		iPt2 = ms_iNumExpandedRouteVerts-1;
		iPt1 = ms_iNumExpandedRouteVerts-2;
	}
	if(iPt1 >= 0 && iPt2 >= 0)
	{
		Vector3 v1 = VEC3V_TO_VECTOR3(ms_vExpandedRouteVerts[ iPt1 ].GetPos());
		Vector3 v2 = VEC3V_TO_VECTOR3(ms_vExpandedRouteVerts[ iPt2 ].GetPos());
		const float fFinalHdg = fwAngle::GetRadianAngleBetweenPoints(v2.x, v2.y, v1.x, v1.y);
		return fFinalHdg;
	}
	return pPed->GetMotionData()->GetCurrentHeading();
}


void CTaskNavBase::ProcessLookAhead(CPed * pPed, const bool bWithinSlowDownDist, Vector3 & vOut_GotoPointTarget, Vector3 & vOut_LookAheadTarget)
{
	const Vec3V pedPosV = pPed->GetTransform().GetPosition();

	Assert( rage::FPIsFinite(pedPosV.GetXf()) && rage::FPIsFinite(pedPosV.GetYf()) && rage::FPIsFinite(pedPosV.GetZf()) );

	//*************************************************************************
	// Calculate distances and times to look ahead from current route position

	const float fCorneringLookAheadTime = ms_fCorneringLookAheadTime;
	const ScalarV goToPointLookAheadDistV = LoadScalar32IntoScalarV(m_fCurrentLookAheadDistance);
	const ScalarV lookAheadPointDistanceV = Add(goToPointLookAheadDistV, ScalarVFromF32(GetLookAheadExtra()));	// TODO: Would be good if GetLookAheadExtra() returned a ScalarV.

	//**********************************************
	// Find the target to use for our goto subtask

	Assert(ms_iNumExpandedRouteVerts != 0);

	const bool bHaltAtWaypoint = bWithinSlowDownDist && (m_iProgress < m_pRoute->GetSize() && ShouldSlowDownForWaypoint(m_pRoute->GetWaypoint(m_iProgress)));
	ScalarV distanceRemainingV;			// Note: unused
	GetPositionAheadOnRoute(ms_vExpandedRouteVerts, ms_iNumExpandedRouteVerts, pedPosV, goToPointLookAheadDistV, RC_VEC3V(vOut_GotoPointTarget), distanceRemainingV, NULL, bHaltAtWaypoint, false);
	GetPositionAheadOnRoute(ms_vExpandedRouteVerts, ms_iNumExpandedRouteVerts, pedPosV, lookAheadPointDistanceV, RC_VEC3V(vOut_LookAheadTarget), distanceRemainingV, NULL, bHaltAtWaypoint, false);

	pPed->SetPedResetFlag( CPED_RESET_FLAG_TakingRouteSplineCorner, (m_fSplineTVal > 0.0f && m_fSplineTVal < 1.0f) );

	//*************************************************************************
	// Update the subtask's target to implement following curved spline paths

	CTask * pSubTask = GetSubTask();
	if(pSubTask)
	{
		const int iSubTaskType = pSubTask->GetTaskType();
		if(iSubTaskType==CTaskTypes::TASK_MOVE_GO_TO_POINT ||
			iSubTaskType==CTaskTypes::TASK_MOVE_GO_TO_POINT_ON_ROUTE ||
			iSubTaskType==CTaskTypes::TASK_MOVE_GO_TO_POINT_AND_STAND_STILL)
		{
			//**************************************************************************************
			// Look ahead to find a position ahead along the path to use for slowing for corners

			const float fCurrMbr = Clamp(pPed->GetMotionData()->GetCurrentMbrY(), MOVEBLENDRATIO_STILL, MOVEBLENDRATIO_SPRINT);

			if( !pPed->IsStrafing() && !pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_InStrafeTransition) )
			{
				CMoveBlendRatioSpeeds moveSpeeds;
				CMoveBlendRatioTurnRates turnRates;

				CTaskMotionBase* pMotionTask = pPed->GetCurrentMotionTask();

				pMotionTask->GetMoveSpeeds(moveSpeeds);
				pMotionTask->GetTurnRates(turnRates);
			
				const float fMoveSpeed = CTaskMotionBase::GetActualMoveSpeed(moveSpeeds.GetSpeedsArray(), fCurrMbr);
				const float fCorneringLookAheadDist = fMoveSpeed * fCorneringLookAheadTime;
				ScalarV distanceRemainingV;		// Note: unused

				Assert(ms_iNumExpandedRouteVerts != 0);

				Vec3V corneringTargetV;
				GetPositionAheadOnRoute(ms_vExpandedRouteVerts, ms_iNumExpandedRouteVerts, pedPosV, ScalarVFromF32(fCorneringLookAheadDist), corneringTargetV, distanceRemainingV, NULL, bHaltAtWaypoint, false);

#if __BANK && defined(DEBUG_DRAW)
				if(CPedDebugVisualiser::GetDebugDisplay()==CPedDebugVisualiser::eTasksFullDebugging || CPedDebugVisualiser::GetDebugDisplay()==CPedDebugVisualiser::eTasksNoText)
					grcDebugDraw::Cross(corneringTargetV, 0.25f, Color_orange);
#endif

				//*************************************************************************
				// Slow this ped when taking sharp corners, to ensure we stay on our route

				ProcessCornering(pPed, corneringTargetV, fCorneringLookAheadTime, moveSpeeds, turnRates);
			}
			//*********************************************************************************************
			// Ensure we reset the moveblendratio to this task's MBR if we didn't call ProcessCornering()

			else
			{
				((CTaskMove*)pSubTask)->SetMoveBlendRatio(m_fMoveBlendRatio);
			}

			pPed->SetPedResetFlag( CPED_RESET_FLAG_HasProcessedCornering, true );
		}
	}

	if (GetFollowNavmeshAtCurrentHeight())
	{
		vOut_GotoPointTarget.z = pedPosV.GetZf();
	}

	Assert( rage::FPIsFinite(vOut_GotoPointTarget.x) && rage::FPIsFinite(vOut_GotoPointTarget.y) && rage::FPIsFinite(vOut_GotoPointTarget.z) );
	Assert( rage::FPIsFinite(vOut_LookAheadTarget.x) && rage::FPIsFinite(vOut_LookAheadTarget.y) && rage::FPIsFinite(vOut_LookAheadTarget.z) );
}

float CTaskNavBase::GetLookAheadExtra() const
{
	const float fFrames = 4.f;
	return GetPed()->GetVelocity().Mag() * fwTimer::GetTimeStep() * fFrames;
}

float CTaskNavBase::GetLookAheadDistanceIncrement() const
{
	return GetPed()->GetPedIntelligence()->GetNavCapabilities().GetGotoTargetLookAheadDistanceIncrement();
}

float CTaskNavBase::GetInitialGotoTargetLookAheadDistance() const
{
	const float fMinInitialLookAhead = (GetPed()->GetIsSwimming() ? 0.5f : 0.0f);	// Shit maybe but swimming peds got issues as their path is way below or above them
	return Max(GetPed()->GetPedIntelligence()->GetNavCapabilities().GetGotoTargetInitialLookAheadDistance(), fMinInitialLookAhead);
}

float CTaskNavBase::GetGotoTargetLookAheadDistance() const
{
	return GetPed()->GetPedIntelligence()->GetNavCapabilities().GetGotoTargetLookAheadDistance();
}

CTask::FSM_Return CTaskNavBase::StateSlideToCoord_OnEnter(CPed * pPed)
{
	Assert(m_NavBaseConfigFlags.IsSet(CNavBaseConfigFlags::NB_SlideToCoordAtEnd));
	const float fHdg = m_NavBaseConfigFlags.IsSet(CNavBaseConfigFlags::NB_SlideToCoordUsingCurrentHeading) ? pPed->GetCurrentHeading() : m_fSlideToCoordHeading;
	CTaskMoveSlideToCoord * pSlideTask = rage_new CTaskMoveSlideToCoord(m_vTarget, fHdg, m_fSlideToCoordSpeed, m_fSlideToCoordHeadingChangeSpeed, 2000);
	SetNewTask(pSlideTask);
	return FSM_Continue;
}
CTask::FSM_Return CTaskNavBase::StateSlideToCoord_OnUpdate(CPed * UNUSED_PARAM(pPed))
{
	if(!GetSubTask() || GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		return FSM_Quit;
	}
	return FSM_Continue;
}

CTask::FSM_Return CTaskNavBase::StateStoppingAtTarget_OnEnter(CPed * pPed)
{
	// Make sure we set the end as target or we might in some circumstances when we just look ahead a bit, not stop where we should B* 1235560, 1270405
	if (GetSubTask())
	{
		// If we nailed the radius we just stop at a good point inside it
		if (GetSubTask()->GetTaskType()==CTaskTypes::TASK_MOVE_GO_TO_POINT_AND_STAND_STILL/* &&
			((CTaskMoveGoToPointAndStandStill*)GetSubTask())->GetIsStopDueToInsideRadius()*/)
		{
			float fStopHeading = m_fTargetStopHeading != DEFAULT_NAVMESH_FINAL_HEADING ? m_fTargetStopHeading : pPed->GetCurrentHeading();
			m_fCurrentLookAheadDistance = pPed->GetCurrentMotionTask()->GetStoppingDistance(fStopHeading);

			Vector3 vGotoPointTarget, vLookAheadTarget;
			ProcessLookAhead(pPed, true, vGotoPointTarget, vLookAheadTarget);
			((CTaskMove*)GetSubTask())->SetTarget(pPed, VECTOR3_TO_VEC3V(vGotoPointTarget));
		}
		else if (m_pRoute && m_pRoute->GetSize() > 0)
		{
			((CTaskMove*)GetSubTask())->SetTarget(pPed, VECTOR3_TO_VEC3V(m_pRoute->GetPosition(m_pRoute->GetSize() - 1)));
		}
		else
		{
			((CTaskMove*)GetSubTask())->SetTarget(pPed, VECTOR3_TO_VEC3V(m_vTarget));
		}
	}

	return FSM_Continue;
}
CTask::FSM_Return CTaskNavBase::StateStoppingAtTarget_OnUpdate(CPed * pPed)
{
	pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_LowPhysicsLodMayPlaceOnNavMesh, true);

	// Handle the case where the task was assigned a new target whilst stopping
	if(HandleNewTarget(pPed))
	{
		SetState(NavBaseState_StationaryWhilstWaitingForPath);
		return FSM_Continue;
	}

	Assert(!GetSubTask() || GetSubTask()->GetTaskType()==CTaskTypes::TASK_MOVE_GO_TO_POINT_AND_STAND_STILL);
	if(!GetSubTask() || GetSubTask()->GetTaskType()!=CTaskTypes::TASK_MOVE_GO_TO_POINT_AND_STAND_STILL)
		return FSM_Quit;

	CTaskMoveGoToPointAndStandStill* pTask = static_cast<CTaskMoveGoToPointAndStandStill*>(GetSubTask());

	if (m_NavBaseConfigFlags.IsSet(CNavBaseConfigFlags::NB_StopExactly))
		((CTaskMoveGoToPointAndStandStill*)GetSubTask())->ResetDistanceRemaining(pPed);
	else
		((CTaskMoveGoToPointAndStandStill*)GetSubTask())->SetDistanceRemaining(0.0f);

	if (m_NavBaseConfigFlags.IsSet(CNavBaseConfigFlags::NB_StopExactly))
	{
		// So runstop nail desired heading code
		Vector3 vDist = pTask->GetTarget() - VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
		if (CPedMotionData::GetIsWalking(m_fMoveBlendRatio) && !pTask->GetIsRunStop())
		{
		}
		else if(vDist.XYMag2() <= g_fFixupRunHeadingRadiusSqr)	//
		{
			static dev_float fHeadingSpeed = 4.0f;
			const float fTargetHeading = pPed->GetDesiredHeading();
			const float fCurrentHeading = pPed->GetMotionData()->GetCurrentHeading();
			float fHeadingDelta = SubtractAngleShorter( fTargetHeading, fCurrentHeading ) / fwTimer::GetTimeStep();
			if (fHeadingDelta > 0.f)
				fHeadingDelta = Min(fHeadingSpeed, fHeadingDelta);
			else
				fHeadingDelta = Max(-fHeadingSpeed, fHeadingDelta);
			
			pPed->GetMotionData()->SetExtraHeadingChangeThisFrame( fHeadingDelta * fwTimer::GetTimeStep() );
		}
	}

	if (pTask->IsStoppedAtTargetPoint())
	{
		return FSM_Quit;
	}

	//--------------------------------------------------------------------------------
	// Handle ped becoming stuck whilst sliding to target (see url:bugstar:1388664)

	if(pPed->GetPedIntelligence()->GetEventScanner()->GetStaticMovementScanner().GetStaticCounter() >= CStaticMovementScanner::ms_iStaticCounterStuckLimit)
	{
		m_NavBaseConfigFlags.Reset(CNavBaseConfigFlags::NB_KeepMovingWhilstWaitingForPath);
		CEventStaticCountReachedMax staticEvent(GetTaskType());
		pPed->GetPedIntelligence()->AddEvent(staticEvent);
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskNavBase::StateWaitingForWaypointActionEvent_OnEnter(CPed * UNUSED_PARAM(pPed))
{
	//Assert(m_pRoute && m_iProgress >= 0 && m_iProgress < m_pRoute->GetSize()-1);

	if(m_iPathHandle!=PATH_HANDLE_NULL)
	{
		CPathServer::CancelRequest(m_iPathHandle);
		m_iPathHandle = PATH_HANDLE_NULL;
	}

	CTaskMoveStandStill * pTaskStandStill = rage_new CTaskMoveStandStill(2000);
	SetNewTask(pTaskStandStill);

	return FSM_Continue;
}

CTask::FSM_Return CTaskNavBase::StateWaitingForWaypointActionEvent_OnUpdate(CPed * UNUSED_PARAM(pPed))
{
	Assert(GetSubTask() && GetSubTask()->GetTaskType()==CTaskTypes::TASK_MOVE_STAND_STILL);

	// If the stand-still task has completed without the special-action event having occured
	// then we will restart the task.
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		AI_OPTIMISATIONS_OFF_ONLY( Assertf(false, "StateWaitingForWaypointActionEvent_OnUpdate - never received the waypoint event."); )

		SetState(NavBaseState_Initial);
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskNavBase::StateWaitingAfterFailedWaypointAction_OnEnter(CPed * UNUSED_PARAM(pPed))
{
	CTaskMoveStandStill * pTaskStandStill = rage_new CTaskMoveStandStill(2000);
	SetNewTask(pTaskStandStill);

	return FSM_Continue;
}

CTask::FSM_Return CTaskNavBase::StateWaitingAfterFailedWaypointAction_OnUpdate(CPed * UNUSED_PARAM(pPed))
{
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		// Restart the task
		SetState(NavBaseState_Initial);
	}
	return FSM_Continue;
}

CTask::FSM_Return CTaskNavBase::StateGetOntoMainNavMesh_OnEnter(CPed * ASSERT_ONLY(pPed))
{
#if NAVMESH_OPTIMISATIONS_OFF && __ASSERT
	Displayf("PED 0x%p at (%.1f, %.1f, %.1f) about to run CTaskMoveGetOntoMainNavMesh", pPed, pPed->GetTransform().GetPosition().GetXf(), pPed->GetTransform().GetPosition().GetYf(), pPed->GetTransform().GetPosition().GetZf());
#endif
	Assert(pPed->GetIsTypePed()); // just to silence the compiler when the above statement is #defined out

	CTaskMoveGetOntoMainNavMesh * pRecoveryTask = rage_new CTaskMoveGetOntoMainNavMesh(m_fInitialMoveBlendRatio, m_vTarget, m_iLastRecoveryMode);
	SetNewTask(pRecoveryTask);

	m_iLastRecoveryMode++;
	if(m_iLastRecoveryMode == CTaskMoveGetOntoMainNavMesh::RecoveryMode_NumModes)
		m_iLastRecoveryMode = 0;

	return FSM_Continue;
}

CTask::FSM_Return CTaskNavBase::StateGetOntoMainNavMesh_OnUpdate(CPed * UNUSED_PARAM(pPed))
{
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		// Restart the task
		SetState(NavBaseState_Initial);
	}
	return FSM_Continue;
}

CTask::FSM_Return CTaskNavBase::StateAchieveHeading_OnEnter(CPed * UNUSED_PARAM(pPed))
{
	Assert(m_fTargetStopHeading < DEFAULT_NAVMESH_FINAL_HEADING);
	CTaskMoveAchieveHeading* pTask = rage_new CTaskMoveAchieveHeading(m_fTargetStopHeading);

	SetNewTask(pTask);
	return FSM_Continue;
}

CTask::FSM_Return CTaskNavBase::StateAchieveHeading_OnUpdate(CPed * UNUSED_PARAM(pPed))
{
	if (!GetSubTask() || GetIsFlagSet(aiTaskFlags::SubTaskFinished))
		return FSM_Quit;

	return FSM_Continue;
}

CTask::FSM_Return CTaskNavBase::StateUsingLadder_OnEnter(CPed * UNUSED_PARAM(pPed))
{
	return FSM_Continue;
}

CTask::FSM_Return CTaskNavBase::StateUsingLadder_OnUpdate(CPed * pPed)
{
	// Wait until CTaskComplexControlMovement task has completed ladder climb subtask, before returning to path following
	CTaskComplexControlMovement * pCtrlMove = (CTaskComplexControlMovement*) pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_COMPLEX_CONTROL_MOVEMENT);
	CTask* pRuningMovementTask = pCtrlMove ? pCtrlMove->GetRunningMovementTask() : NULL;
	if( pRuningMovementTask == this || (pRuningMovementTask && pRuningMovementTask->FindSubTaskOfType(GetTaskType()) == this) )
	{
		if(!pCtrlMove->GetIsClimbingLadder())
		{
			if (!pCtrlMove->HasLastClimbingLadderFailed())
			{
				m_iProgress = m_iClampMinProgress;
				SetState(NavBaseState_FollowingPath);
				return FSM_Continue;
			}
			else
			{
				return FSM_Quit;
			}
		}
	}
	else
	{
		Assert(false);
		SetState(NavBaseState_FollowingPath);
		return FSM_Continue;
	}

	return FSM_Continue;
}

void CTaskNavBase::ModifyPathFlagsForPedLod(CPed * pPed, u64 & iFlags) const
{
	if( pPed->GetPedAiLod().IsLodFlagSet( CPedAILod::AL_LodPhysics ) )
	{
		iFlags |= PATH_FLAG_NEVER_CLIMB_OVER_STUFF;
		iFlags |= PATH_FLAG_NEVER_DROP_FROM_HEIGHT;
		iFlags |= PATH_FLAG_NEVER_USE_LADDERS;
		iFlags &= ~PATH_FLAG_IF_NOT_ON_PAVEMENT_ALLOW_DROPS_AND_CLIMBS;
	}
}

//***********************************************************************************************
// Create an event which will perform a special action at this waypoint.  For example climbing /
// descending a ladder, climbing up an edge, dropping down over an edge.  This function is
// virtual so that derived CTaskNavBase::classes can implement custom waypoint actions.

CTaskNavBase::EWaypointActionResult CTaskNavBase::CreateWaypointActionEvent(CPed * pPed, const s32 iProgress)
{
	if(iProgress+1 >= m_pRoute->GetSize())
	{
#if AI_OPTIMISATIONS_OFF
		Assertf( iProgress+1 >= m_pRoute->GetSize(), "waypoint at route end (%i/%i)", iProgress, m_pRoute->GetSize() );
#endif
		// Error - special action waypoints should never be created at the end of a route.
		// We need to have a waypoint after them in order to know where our destination is
		// (eg. top of ladder, climb target, etc).
		// In this case return 'WaypointAction_ActionFailed' which will cause a re-path
		// NB: This error probably has occurred due to some kind of path post-processing,
		// such as inserting points to preserve the Z height of the terrain
		return WaypointAction_ActionFailed;
	}

	const TNavMeshWaypointFlag & iWptFlags = m_pRoute->GetWaypoint(iProgress);

	const Vector3 vPedPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
	const Vector3 vWptPos = m_pRoute->GetPosition(iProgress);
	const Vector3 vDiff = vWptPos - vPedPos;
	const float fInPositionSqr = 2.0f*2.0f;
#if AI_OPTIMISATIONS_OFF
	Assertf(vDiff.XYMag2() < fInPositionSqr, "Ped is not in position for waypoint.\nPed 0x%p at (%.1f,%.1f,%.1f) is %.1fm away from waypoint type %u", pPed, vPedPos.x, vPedPos.y, vPedPos.z, vDiff.Mag(), iWptFlags.GetSpecialAction());
#endif
	if(vDiff.XYMag2() > fInPositionSqr)
		return WaypointAction_ActionFailed;


	//**********
	// Ladders

	if(iWptFlags.m_iSpecialActionFlags & (WAYPOINT_FLAG_CLIMB_LADDER|WAYPOINT_FLAG_DESCEND_LADDER))
	{
		Vector3 vBase, vTop;
		const int iDir = (iWptFlags.m_iSpecialActionFlags & WAYPOINT_FLAG_CLIMB_LADDER) ? 1 : -1;

		if(iWptFlags.m_iSpecialActionFlags & WAYPOINT_FLAG_CLIMB_LADDER)
		{
			vBase = m_pRoute->GetPosition(iProgress);
			vTop = m_pRoute->GetPosition(iProgress+1);
		}
		else
		{
			vTop = m_pRoute->GetPosition(iProgress);
			vBase = m_pRoute->GetPosition(iProgress+1);
		}

		const float fHeading = iWptFlags.GetHeading();

		// Poll any controlling CTaskComplexControlMovement task to see whether it is able to run a ladder-climbing task as its subtask.
		// If we signal it to climb the ladder, and transition to state NavBaseState_UsingLadder whilst it does so.
		CTaskComplexControlMovement * pCtrlMove = (CTaskComplexControlMovement*) pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_COMPLEX_CONTROL_MOVEMENT);

		if(pCtrlMove && pCtrlMove->MayClimbLadderAsSubtask())
		{
			CTask* pRuningMovementTask = pCtrlMove->GetRunningMovementTask();
			if( pRuningMovementTask == this || (pRuningMovementTask && pRuningMovementTask->FindSubTaskOfType(GetTaskType()) == this) )
			{
				Assert(!pCtrlMove->GetIsClimbingLadder());

				pCtrlMove->SetIsClimbingLadder(iDir == 1 ? vBase : vTop, fHeading, m_fInitialMoveBlendRatio);
				SetState(NavBaseState_UsingLadder);

				m_iClampMinProgress = (u8)(iProgress+1);

				return WaypointAction_StateTransitioned;
			}
		}

		// Otherwise we use the default behaviour for all ladders/climbs/drops, which is to create
		// an event which handles the waypoint action as a response task.
		CEventClimbLadderOnRoute ladderEvent(vBase, vTop, iDir, fHeading, m_fInitialMoveBlendRatio);
		pPed->GetPedIntelligence()->AddEvent(ladderEvent);

		return WaypointAction_EventCreated;
	}

	//**********************
	// Climbs & drop-downs

	else if(iWptFlags.m_iSpecialActionFlags & (WAYPOINT_FLAG_CLIMB_LOW|WAYPOINT_FLAG_CLIMB_HIGH|WAYPOINT_FLAG_DROPDOWN|WAYPOINT_FLAG_CLIMB_OBJECT))
	{
		//*******************************************************************************************
		// If this route is taking place on a dynamic navmesh/entity, then the points we give to the
		// CEventClimbNavMeshOnRoute event must be local, and we'll include an entity pointer whose
		// matrix we must transform them by.

		CNavMeshRoute * pRoute;
		if(m_pOnEntityRouteData && m_pOnEntityRouteData->m_pDynamicEntityRouteIsOn)
		{
			Assert(m_pOnEntityRouteData->m_pLocalSpaceRoute);
			pRoute = m_pOnEntityRouteData->m_pLocalSpaceRoute;
		}
		else
		{
			pRoute = m_pRoute;
		}

		const Vector3 & vCurrentTarget = pRoute->GetPosition(iProgress);
		const Vector3 & vNextTarget = pRoute->GetPosition(iProgress+1);

		Vector3 vClimbVec = vNextTarget - vCurrentTarget;
		vClimbVec.z = 0.0f;
		vClimbVec.Normalize();
		const float fHeading = fwAngle::GetRadianAngleBetweenPoints(vClimbVec.x, vClimbVec.y, 0.0f, 0.0f);
		bool bSpecialActionForceJump = false;

		//**************************************************************************************
		// If this is a climb of some sort, use the CInAirHelper class to determine whether
		// we can actually climb here.  If not, then we will need to find an alternative route.

		if(iWptFlags.m_iSpecialActionFlags & (WAYPOINT_FLAG_CLIMB_LOW|WAYPOINT_FLAG_CLIMB_HIGH|WAYPOINT_FLAG_CLIMB_OBJECT))
		{
			if(!CheckIfClimbIsPossible(vCurrentTarget, fHeading, pPed))
			{
				//*******************************************************************************
				// No valid climb was found...
				// If this was only a "low climb" then make sure that we really do need to do a
				// jump/climb here.  If the target is vertically very close to us, and there is
				// no obstruction - then we can probably just walk there.

				if((iWptFlags.m_iSpecialActionFlags & WAYPOINT_FLAG_CLIMB_LOW) && !pPed->GetIsSwimming())
				{
					const Vector3 vPedPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
					if(CTaskNavBase::TestClearance(vPedPosition, vNextTarget, pPed, false))
					{
						return WaypointAction_EventNotNeeded;
					}

					// In this situation we did not manage to climb but if this test is true we could jump here instead
					// This should happen when the step is too low to allow a climb but too high to walk over
					if(CTaskNavBase::TestClearance(vPedPosition, vNextTarget, pPed, true))
					{
						bSpecialActionForceJump = true;
					}
				}

				if (!bSpecialActionForceJump)
				{
					//***************************************************************************
					// This tags the climb as failed.  After a certain number of failed attempts
					// in the same location for this ped, the climb will actually be disabled
					// within the navmesh itself - thus excluding it from future path searches.

					pPed->GetPedIntelligence()->RecordFailedClimbOnRoute(vCurrentTarget);

					return WaypointAction_ActionFailed;
				}
			}
		}

		const s32 iTimeLeft = m_WarpTimer.IsSet() ? m_WarpTimer.GetTimeLeft() : -1;

		CEventClimbNavMeshOnRoute climbNavMeshEvent(
			vCurrentTarget,
			fHeading,
			vNextTarget,
			m_fInitialMoveBlendRatio,
			iWptFlags.m_iSpecialActionFlags,
			m_pOnEntityRouteData ? m_pOnEntityRouteData->m_pDynamicEntityRouteIsOn.Get() : NULL,
			iTimeLeft,
			m_vTarget,
			bSpecialActionForceJump
		);

		pPed->GetPedIntelligence()->AddEvent(climbNavMeshEvent);

		return WaypointAction_EventCreated;
	}

	return WaypointAction_ActionNotFound;
}

bool CTaskNavBase::TestClearance(const Vector3 & vStartIn, const Vector3 & vEndIn, const CPed * pPed, bool bStartTestAboveKnee)
{
#if __DEV && DEBUG_DRAW
	static dev_bool bDrawCapsules = false;
#endif

	const CBaseCapsuleInfo * pPedCapsuleInfo = pPed->GetCapsuleInfo();

	const float fHeadHeight = pPedCapsuleInfo->GetMaxSolidHeight();
	const float fKneeHeight = pPedCapsuleInfo->GetMinSolidHeight();
	const float fDeltaHeight = fHeadHeight - fKneeHeight;
	const float fPedRadius = pPedCapsuleInfo->GetHalfWidth();
	const float fTestInc = ((fPedRadius*2.0f) * 0.75f);
	const int iNumTests = (int) (fDeltaHeight / fTestInc) + 1;

	Vector3 vStart = vStartIn;
	vStart.z += fKneeHeight + fPedRadius * 0.5f;
	Vector3 vEnd = vEndIn;
	vEnd.z += fKneeHeight + fPedRadius * 0.5f;

	int i = 0;
	if(bStartTestAboveKnee)
	{
		i = 1;
		vStart.z += fTestInc;
		vEnd.z += fTestInc;
	}

	for( ; i<iNumTests; i++)
	{
#if __DEV && DEBUG_DRAW
		if(bDrawCapsules && (CPedDebugVisualiser::GetDebugDisplay()==CPedDebugVisualiser::eTasksFullDebugging ||
			CPedDebugVisualiser::GetDebugDisplay()==CPedDebugVisualiser::eTasksNoText))
			grcDebugDraw::Line(vEnd, vStart, Color_white, Color_green);
#endif

		const u32 iTestFlags = ArchetypeFlags::GTA_MAP_TYPE_MOVER | ArchetypeFlags::GTA_GLASS_TYPE | ArchetypeFlags::GTA_VEHICLE_TYPE | ArchetypeFlags::GTA_BOX_VEHICLE_TYPE | ArchetypeFlags::GTA_OBJECT_TYPE;
		WorldProbe::CShapeTestCapsuleDesc capsuleDesc;
		//const Vector3 vPedPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
		capsuleDesc.SetCapsule(vStart, vEnd, fPedRadius);
		capsuleDesc.SetIncludeFlags(iTestFlags);
		capsuleDesc.SetContext(WorldProbe::LOS_GeneralAI);
		WorldProbe::CShapeTestFixedResults<> capsuleResult;
		capsuleDesc.SetResultsStructure(&capsuleResult);

		bool bClearOfObstacles = !WorldProbe::GetShapeTestManager()->SubmitTest(capsuleDesc);
		if (!bClearOfObstacles)
		{
			u32 i = 0;
			for ( ; i < capsuleResult.GetNumHits(); ++i)
			{
				// Not sure if we should allow null here or not, maybe just assert on null?
				const CEntity* pEntity = CPhysics::GetEntityFromInst(capsuleResult[i].GetHitInst());
				taskAssert(pEntity);
				if (!pEntity->GetIsTypeObject())
					break;

				if (CPathServerGta::ShouldAvoidObject(pEntity))
					break;
			}

			if (i == capsuleResult.GetNumHits())
				bClearOfObstacles = true;
		}

#if __DEV && DEBUG_DRAW
		if(bDrawCapsules)
			CTask::ms_debugDraw.AddCapsule(RCC_VEC3V(vStart), RCC_VEC3V(vEnd), fPedRadius, bClearOfObstacles ? Color_green : Color_red, 1000, 0, false);
#endif

		if(!bClearOfObstacles)
			return false;

		vStart.z += fTestInc;
		vEnd.z += fTestInc;
	}
	return true;
}

Vector3 CTaskNavBase::PathRequest_DistanceAheadCalculation(const float fDistanceAhead, CPed * pPed)
{
	TRequestPathStruct tempReqStruct;
	InitPathRequest_DistanceAheadCalculation(fDistanceAhead, pPed, tempReqStruct);

	return tempReqStruct.m_vPathStart;
}

bool CTaskNavBase::CreateClimbOutOfWaterEvent(CPed * pPed, const Vector3 & vPedPos)
{
	const CCollisionRecord * colRec = pPed->GetFrameCollisionHistory()->GetMostSignificantCollisionRecord();
	if( colRec && colRec->m_pRegdCollisionEntity && colRec->m_pRegdCollisionEntity->GetIsTypeBuilding() )
	{
		Vector3 vNext;
		if (m_iProgress + 1 < m_pRoute->GetSize())
			vNext = m_pRoute->GetPosition(m_iProgress+1);
		else
			vNext = m_pRoute->GetPosition(m_iProgress);


		const float fHeading = fwAngle::GetRadianAngleBetweenPoints(vNext.x, vNext.y, vPedPos.x, vPedPos.y);

		if(CheckIfClimbIsPossible(vPedPos, fHeading, pPed))
		{
			const s32 iTimeLeft = m_WarpTimer.IsSet() ? m_WarpTimer.GetTimeLeft() : -1;

			Vector3 vToNext = vNext - vPedPos;
			vToNext.Normalize();
			const Vector3 vTargetAfterClimb = vPedPos + (vToNext * 1.0f);

			CEventClimbNavMeshOnRoute climbNavMeshEvent(
				vPedPos,
				fHeading,
				vNext,
				m_fInitialMoveBlendRatio,
				WAYPOINT_FLAG_CLIMB_LOW,
				m_pOnEntityRouteData ? m_pOnEntityRouteData->m_pDynamicEntityRouteIsOn.Get() : NULL,
				iTimeLeft,
				vTargetAfterClimb);
			pPed->GetPedIntelligence()->AddEvent(climbNavMeshEvent);

			return true;
		}
	}
	return false;
}

bool CTaskNavBase::CheckIfClimbIsPossible(const Vector3 & vClimbPos, float fClimbHeading, CPed * pPed) const
{
	// Try firstly with the position of the climb as returned from the navmesh
	Matrix34 mClimbMat;
	mClimbMat.MakeRotateZ(fClimbHeading);
	mClimbMat.d = vClimbPos;

	CClimbHandHoldDetected handHold;
	pPed->SetPedResetFlag(CPED_RESET_FLAG_SearchingForClimb, true);
	pPed->GetPedIntelligence()->GetClimbDetector().Process(&mClimbMat);
	if(pPed->GetPedIntelligence()->GetClimbDetector().GetDetectedHandHold(handHold))
		return true;

	// Try again with the ped's current position
	mClimbMat.d = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());

	pPed->SetPedResetFlag(CPED_RESET_FLAG_SearchingForClimb, true);
	pPed->GetPedIntelligence()->GetClimbDetector().Process(&mClimbMat);
	if(pPed->GetPedIntelligence()->GetClimbDetector().GetDetectedHandHold(handHold))
		return true;

	return false;
}

bool CTaskNavBase::ShouldSlowDownForWaypoint(const TNavMeshWaypointFlag & wptFlag) const
{
	if(TNavMeshWaypointFlag::IsClimb(wptFlag.m_iSpecialActionFlags))
		return true;

	return false;
}

float CTaskNavBase::GetTargetRadiusForWaypoint(const int iProgress) const
{
	if(iProgress < m_pRoute->GetSize())
	{
		const u32 iWptAction = m_pRoute->GetWaypoint(iProgress).GetSpecialActionFlags();

		const u32 iNextWptFlags =
			(iProgress < m_pRoute->GetSize()-1 && (m_pRoute->GetPosition(iProgress) - m_pRoute->GetPosition(iProgress+1)).XYMag2() < ms_fWaypointSlowdownProximitySqr)
				? m_pRoute->GetWaypoint(iProgress+1).GetSpecialActionFlags() : 0;

		if((iWptAction & (WAYPOINT_FLAG_CLIMB_LADDER|WAYPOINT_FLAG_DESCEND_LADDER)) ||
			(iNextWptFlags & (WAYPOINT_FLAG_CLIMB_LADDER|WAYPOINT_FLAG_DESCEND_LADDER)))
		{
			return 0.5f;
		}
		else if(IsSpecialActionClimbOrDrop(iWptAction))
		{
			return 0.1f;
		}
		else
		{
			return m_fTargetRadius;
		}
	}
	return m_fTargetRadius;
}

float CTaskNavBase::GetPathToleranceForWaypoint(const int iProgress) const
{
	// first and last segments have min tolerance
	if ( iProgress > 0 && iProgress < m_pRoute->GetSize()-1)
	{
		const u32 iWptAction = m_pRoute->GetWaypoint(iProgress).GetSpecialActionFlags();

		const u32 iNextWptFlags = (iProgress < m_pRoute->GetSize()-1 && (m_pRoute->GetPosition(iProgress) - m_pRoute->GetPosition(iProgress+1)).XYMag2() < ms_fWaypointSlowdownProximitySqr)
			? m_pRoute->GetWaypoint(iProgress+1).GetSpecialActionFlags() : 0;

		if( (iWptAction & (WAYPOINT_FLAG_IS_NARROW_IN|WAYPOINT_FLAG_IS_NARROW_OUT))
			|| (iNextWptFlags & (WAYPOINT_FLAG_IS_NARROW_IN|WAYPOINT_FLAG_IS_NARROW_OUT)) )
		{
			
			return 0.0f;
		}
		return 1.0f;
	}
	return 0.0f;
}

CTask::FSM_Return CTaskNavBase::DecideStoppingState(CPed * pPed)
{
	float fStopHeading = m_fTargetStopHeading != DEFAULT_NAVMESH_FINAL_HEADING ? m_fTargetStopHeading : pPed->GetCurrentHeading();
	if (pPed->GetVelocity().Mag2() <= 0.01f || pPed->GetCurrentMotionTask()->GetStoppingDistance(fStopHeading) <= 0.0f)
	{
		static dev_float s_fMaxExactHeadingDiff = 0.025f;
		float fHeadingDiff = (m_fTargetStopHeading < DEFAULT_NAVMESH_FINAL_HEADING ? Abs(SubtractAngleShorter(m_fTargetStopHeading, pPed->GetCurrentHeading())) : DEFAULT_NAVMESH_FINAL_HEADING);
		if (fHeadingDiff > s_fMaxExactHeadingDiff && fHeadingDiff < DEFAULT_NAVMESH_FINAL_HEADING)
		{
			SetState(NavBaseState_AchieveHeading);
			return FSM_Continue;
		}
		else
		{
			return FSM_Quit;
		}
	}

	SetFlag(aiTaskFlags::KeepCurrentSubtaskAcrossTransition);
	SetState(NavBaseState_StoppingAtTarget);
	return FSM_Continue;
}

bool CTaskNavBase::IsNavigatingUnderwater(const CPed * pPed) const
{
	if (pPed->GetCurrentMotionTask()->IsUnderWater())
	{
		return true;
	}

	return false;
}

void CTaskNavBase::SetMoveBlendRatio(const float fMoveBlendRatio, const bool bFlagAsChanged)
{
	//Call the base class version.
	CTaskMove::SetMoveBlendRatio(fMoveBlendRatio, bFlagAsChanged);
	
	//Set the initial move blend ratio.
	m_fInitialMoveBlendRatio = fMoveBlendRatio;
}

void CTaskNavBase::SetTarget(const CPed * pPed, Vec3V_In vTarget1, const bool bForce)
{
	const Vector3 vTarget = VEC3V_TO_VECTOR3(vTarget1);

	// Ensure that the backed up movement task in CTaskComplexControlMovement is up to date
	// TODO: We really need to make this an automatic operation, perhaps in the CTaskMoveBase::SetTarget() base
	CTaskMove * pBackup = GetControlMovementBackup(pPed);
	if(pBackup && pBackup->InheritsFromTaskNavBase())
		((CTaskNavBase*)pBackup)->m_vTarget = vTarget;

	Assertf(rage::FPIsFinite(vTarget.x) && rage::FPIsFinite(vTarget.y) && rage::FPIsFinite(vTarget.z), "vTarget is not numerically valid.");
	ASSERT_ONLY(const float fLargeValue = 10000.0f;)
	Assertf(vTarget.x > -fLargeValue && vTarget.y > -fLargeValue && vTarget.z > -fLargeValue && vTarget.x < fLargeValue && vTarget.y < fLargeValue && vTarget.z < fLargeValue, "vTarget (%.2f, %.2f, %.2f) is outside of the world", vTarget.x, vTarget.y, vTarget.z);

	//****************************************************************************************************************
	// the "m_bAdaptiveSetTargetEpsilon" feature allows this task to have an adaptive epsilon calculated for testing
	// whether to recalculated the route or not.  If this is set, then the further the ped is from their target -
	// the greater the difference between the current target & new target must be before an update occurs.

	static const float fMaxAdaptiveTargetEpsilon	= 5.0f;
	static const float fMaxAdaptiveTargetDist		= 50.0f;
	static const float fMinAdaptiveTargetDist		= 10.0f;
	static const float fRange						= fMaxAdaptiveTargetDist - fMinAdaptiveTargetDist;

	float fTargetEqualEpsilon;

	if(m_NavBaseConfigFlags.IsSet(CNavBaseConfigFlags::NB_UseAdaptiveSetTargetEpsilon))
	{
		const Vector3 vPedToNewTarget = vTarget - VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
		const float fDist = vPedToNewTarget.Mag();

		if(fDist < fMinAdaptiveTargetDist)
		{
			fTargetEqualEpsilon = ms_fTargetEqualEpsilon;
		}
		else
		{
			fTargetEqualEpsilon = (fDist-fMinAdaptiveTargetDist) / fRange;
			fTargetEqualEpsilon = Clamp(fTargetEqualEpsilon, 0.0f, 1.0f);
			fTargetEqualEpsilon *= fMaxAdaptiveTargetEpsilon;
			fTargetEqualEpsilon = Max(fTargetEqualEpsilon, ms_fTargetEqualEpsilon);
		}
	}
	else
	{
		fTargetEqualEpsilon = ms_fTargetEqualEpsilon;
	}

	bool bTargetPosEqual = m_vTarget.IsClose(vTarget, fTargetEqualEpsilon);

	m_vNewTarget = vTarget;

	if(bTargetPosEqual && !bForce)
	{
		m_NavBaseInternalFlags.Set(CNavBaseInternalFlags::NB_LastSetTargetWasNotUsed);
	}
	else
	{
		m_NavBaseInternalFlags.Set(CNavBaseInternalFlags::NB_HadNewTarget);
		m_NavBaseInternalFlags.Reset(CNavBaseInternalFlags::NB_LastSetTargetWasNotUsed);

		m_iTimeOfLastSetTargetCall = fwTimer::GetTimeInMilliseconds();
	}
}
void CTaskNavBase::SetTargetRadius(const float fTargetRadius)
{
	m_fTargetRadius = fTargetRadius;
}
bool CTaskNavBase::HandleNewTarget(CPed * pPed)
{
	Assertf(GetState() < NavBaseState_NumStates, "State %i should be handled in the derived class", GetState());

	if(!m_NavBaseInternalFlags.IsSet(CNavBaseInternalFlags::NB_HadNewTarget) || m_iPathHandle != PATH_HANDLE_NULL)
	{
		return false;
	}

	Vector3 vOldTarget = m_vTarget;
	m_NavBaseInternalFlags.Reset(CNavBaseInternalFlags::NB_HadNewTarget);
	m_vTarget = m_vNewTarget;

	if(GetState()==NavBaseState_FollowingPath)
	{
		m_NavBaseInternalFlags.Set(CNavBaseInternalFlags::NB_HadPreviousGotoTarget);

		bool bMayUsePreviousTarget = true;
		m_NavBaseInternalFlags.Set(CNavBaseInternalFlags::NB_MayUsePreviousTargetInMovingWaitingForPath, true);

		if(ShouldKeepMovingWhilstWaitingForPath(pPed, bMayUsePreviousTarget))
		{
#if __REQUEST_ROUTE_WHILST_FOLLOWING_PATH
			BANK_ONLY( if(ms_bDebugRequests) Displayf("%u) RequestPath() : Ped 0x%p on-the-fly, inside HandleNewTarget (no state change)", fwTimer::GetFrameCount(), pPed); )

			if( RequestPath(pPed, GetDistanceAheadForSetNewTarget() ) )
			{
				m_NavBaseInternalFlags.Set(CNavBaseInternalFlags::NB_CalculatingPathOnTheFly, true);
				m_NavBaseInternalFlags.Set(CNavBaseInternalFlags::NB_OnTheFlyPathStartedAtRouteEnd, false);
				return true;
			}
#else
			BANK_ONLY( if(ms_bDebugRequests) Displayf("%u) RequestPath() : Ped 0x%p moving-whilst-waiting-for-path, inside HandleNewTarget", fwTimer::GetFrameCount(), pPed); )

			if( RequestPath(pPed) )
			{
				m_NavBaseInternalFlags.Set(CNavBaseInternalFlags::NB_MayUsePreviousTargetInMovingWaitingForPath, bMayUsePreviousTarget);
				SetState(NavBaseState_MovingWhilstWaitingForPath);
				return true;
			}
#endif
			
		}
		else
		{
			BANK_ONLY( if(ms_bDebugRequests) Displayf("%u) RequestPath() : Ped 0x%p stationary, inside HandleNewTarget", fwTimer::GetFrameCount(), pPed); )

			if( RequestPath(pPed) )
			{
				BANK_ONLY( if(ms_bDebugRequests) Displayf("%u) Ped 0x%p entering NavBaseState_StationaryWhilstWaitingForPath, from HandleNewTarget (ShouldKeepMovingWhilstWaitingForPath returned false)", fwTimer::GetFrameCount(), pPed); )

				SetState(NavBaseState_StationaryWhilstWaitingForPath);
				return true;
			}
		}		
	}
	else
	{
		BANK_ONLY( if(ms_bDebugRequests) Displayf("%u) RequestPath() : Ped 0x%p inside HandleNewTarget, no state change from %i", fwTimer::GetFrameCount(), pPed, GetState()); )

		if( RequestPath(pPed) )
		{
			return true;
		}
	}

	// If we get here, then we failed to request a path
	// This will be due to the pathserver not having any free slots for the request
	// Reset target, and NB_HadNewTarget flag for trying again next frame.

	m_NavBaseInternalFlags.Set(CNavBaseInternalFlags::NB_HadNewTarget);
	m_vTarget = vOldTarget;

	return false;
}

bool CTaskNavBase::RequestPath(CPed * pPed, const float fDistanceAheadOfPed)
{
	//Assert(!m_NavBaseInternalFlags.IsSet(CNavBaseInternalFlags::NB_CalculatingPathOnTheFly));	// this flag will be set in InitPathRequestStruct()

	//----------------------------------------------------------------------
	// Handle the case where we were already calculating a path on the fly

	if(m_NavBaseInternalFlags.IsSet(CNavBaseInternalFlags::NB_CalculatingPathOnTheFly))
	{
		m_NavBaseInternalFlags.Reset(CNavBaseInternalFlags::NB_CalculatingPathOnTheFly);
		if(m_pNextRouteSection)
		{
			delete m_pNextRouteSection;
			m_pNextRouteSection = NULL;
		}
		if(m_iPathHandle != PATH_HANDLE_NULL)
		{
			BANK_ONLY( if(ms_bDebugRequests) Displayf("%u) Ped 0x%p cancelling path handle 0x%x", fwTimer::GetFrameCount(), pPed, m_iPathHandle); )

			CPathServer::CancelRequest(m_iPathHandle);
			m_iPathHandle = PATH_HANDLE_NULL;
		}
	}

	//----------------
	// Request path

	if(m_iPathHandle == PATH_HANDLE_NULL)
	{
		TRequestPathStruct pathReq;

		InitPathRequestPathStruct(pPed, pathReq, fDistanceAheadOfPed);

		ModifyPathFlagsForPedLod(pPed, pathReq.m_iFlags);

		m_iPathHandle = CPathServer::RequestPath(pathReq);
	}

	if(m_iPathHandle == PATH_HANDLE_NULL)
	{
		m_NavBaseInternalFlags.Reset(CNavBaseInternalFlags::NB_CalculatingPathOnTheFly);
	}

	BANK_ONLY( if(ms_bDebugRequests) Displayf("%u) Ped 0x%p path handle was 0x%x", fwTimer::GetFrameCount(), pPed, m_iPathHandle); )

	return (m_iPathHandle != PATH_HANDLE_NULL);
}

void CTaskNavBase::InitPathRequest_DistanceAheadCalculation(const float fDistanceAhead, CPed * pPed, TRequestPathStruct & reqPathStruct)
{
	const Vector3 vPedPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());

	bool bPositionAheadFound = false;

	if(m_pRoute && m_pRoute->GetSize() > 0)
	{
		// We must expand the current route in total to calculate the position ahead, and a progress for it
		ms_iNumExpandedRouteVerts = m_pRoute->ExpandRoute(ms_vExpandedRouteVerts, NULL, 0, m_pRoute->GetSize(), 9999.0f);

		if( ms_iNumExpandedRouteVerts )
		{
			// We must recalculate our 'm_iClosestSegOnExpandedRoute' since we have changed the values in 'ms_vExpandedRouteVerts'
			m_iClosestSegOnExpandedRoute = (s16) CalculateClosestSegmentOnExpandedRoute(ms_vExpandedRouteVerts, ms_iNumExpandedRouteVerts, vPedPosition, m_fSplineTVal, &m_vClosestPosOnRoute, &ms_vClosestSegmentOnRoute);

			// Advance to minimum 'm_iClampMinProgress'
			while(ms_vExpandedRouteVerts[m_iClosestSegOnExpandedRoute].m_iProgress < m_iClampMinProgress && m_iClosestSegOnExpandedRoute < ms_iNumExpandedRouteVerts-1)
				m_iClosestSegOnExpandedRoute++;

			Assert(ms_iNumExpandedRouteVerts != 0);

			// Determine the target position ahead along the route, which will be the start of our new path request
			ScalarV distanceRemainingV;
			GetPositionAheadOnRoute(ms_vExpandedRouteVerts, ms_iNumExpandedRouteVerts, VECTOR3_TO_VEC3V(vPedPosition), ScalarV(fDistanceAhead), RC_VEC3V(reqPathStruct.m_vPathStart), distanceRemainingV, NULL, true, true);

			// Flag as calculating this path on the fly
			m_NavBaseInternalFlags.Set(CNavBaseInternalFlags::NB_CalculatingPathOnTheFly, true);

			bPositionAheadFound = true;
		}
	}

	// If we failed to calculate a position ahead, we use the current position
	if(!bPositionAheadFound)
	{
		// Search start is set the the ped's current position
		reqPathStruct.m_vPathStart = vPedPosition;
		m_NavBaseInternalFlags.Set(CNavBaseInternalFlags::NB_CalculatingPathOnTheFly, false);
	}
}

bool CTaskNavBase::RetrievePath(TPathResultInfo & pathResultInfo, CPed * pPed, CNavMeshRoute * pTargetRoute)
{
	Assert(m_iPathHandle != PATH_HANDLE_NULL);
	Assert(pTargetRoute);

	int iNumPts = 0;
	Vector3 * pPoints = NULL;
	TNavMeshWaypointFlag * pWaypointFlags = NULL;

	const int iRetVal = CPathServer::LockPathResult(m_iPathHandle, &iNumPts, pPoints, pWaypointFlags, &pathResultInfo);
	Assert(iRetVal != PATH_STILL_PENDING);	// We should have already ensured that the path is ready

	if(iRetVal == PATH_FOUND)
	{
		pTargetRoute->Clear();

		//************************************************************
		// Record whether the final point in the path is our target

		if((!pathResultInfo.m_bLastRoutePointIsTarget) || pathResultInfo.m_bWentFarAsPossibleTowardsTarget)
		{
			pTargetRoute->SetRouteFlags( m_pRoute->GetRouteFlags() &~ CNavMeshRoute::FLAG_LAST_ROUTE_POINT_IS_TARGET );
		}
		else
		{
			pTargetRoute->SetRouteFlags( CNavMeshRoute::FLAG_LAST_ROUTE_POINT_IS_TARGET );
		}

		//**************************************************
		// Record whether this path segment includes water

		if(pathResultInfo.m_bPathIncludesWater)
			m_NavBaseInternalFlags.Set(CNavBaseInternalFlags::NB_PathIncludesWater);
		else
			m_NavBaseInternalFlags.Reset(CNavBaseInternalFlags::NB_PathIncludesWater);

		//************************************************
		// See how many of the returned points we can use

		const int iNumPtsUsable = Min(iNumPts, (int)CNavMeshRoute::MAX_NUM_ROUTE_ELEMENTS);

		//*********************************************************************
		// If the returned route was empty then just go directly to the target

		if(!iNumPtsUsable)
		{
			TNavMeshWaypointFlag wptFlag;
			wptFlag.Clear();
			pTargetRoute->Add(m_vTarget, wptFlag);
		}

		else
		{
			//-----------------------------------------------------------------------------------
			// Was the final waypoint adjusted from the required position ?
			// For example, if it was inside the bounds of an entity it may have been pushed out.

			const TNavMeshWaypointFlag wptFlag = pTargetRoute->GetWaypoint(iNumPtsUsable-1);

			// If the path reached the target and we specified NB_DontAdjustTargetPosition, ensure that final point is exact target position
			if(pathResultInfo.m_bLastRoutePointIsTarget && m_NavBaseConfigFlags.IsSet(CNavBaseConfigFlags::NB_DontAdjustTargetPosition))
			{
				pPoints[iNumPtsUsable-1] = m_vTarget;
			}

			if((wptFlag.m_iSpecialActionFlags & WAYPOINT_FLAG_ADJUSTED_TARGET_POS)!=0) // (this also implies 'pathResultInfo.m_bLastRoutePointIsTarget')
			{
				if(m_NavBaseConfigFlags.IsSet(CNavBaseConfigFlags::NB_DontAdjustTargetPosition))
				{
					m_NavBaseInternalFlags.Reset(CNavBaseInternalFlags::NB_TargetPositionWasAdjusted);
					pPoints[iNumPtsUsable-1] = m_vTarget;
				}
				else
				{
					m_NavBaseInternalFlags.Set(CNavBaseInternalFlags::NB_TargetPositionWasAdjusted);
					m_vAdjustedTarget[0] = pPoints[iNumPtsUsable-1].x;
					m_vAdjustedTarget[1] = pPoints[iNumPtsUsable-1].y;
					m_vAdjustedTarget[2] = pPoints[iNumPtsUsable-1].z;
				}
			}

			//------------------
			// Create the route

			for(int p=0; p<iNumPtsUsable; p++)
			{
				Vector3 vPoint = pPoints[p];

				Assert( rage::FPIsFinite(vPoint.x) && rage::FPIsFinite(vPoint.y) && rage::FPIsFinite(vPoint.z) );
	
				//------------------------------------------------------------------------------------
				// Points returned by navmesh searches are at the same height as the navmesh surface
				// We raise them up to the root height of the ped here.
				// This is important so that low LOD peds with their physics switched off are
				// able to interpolate along their paths at the correct Z height.

				const bool bShouldNotOffset = (pWaypointFlags[p].GetSpecialActionFlags() & WAYPOINT_FLAG_IS_ON_WATER_SURFACE) != 0;

				if(!bShouldNotOffset)
				{
					vPoint.z += pPed->GetCapsuleInfo()->GetGroundToRootOffset();
				}

				pTargetRoute->Add(
					vPoint,
					pWaypointFlags[p],
					pathResultInfo.m_iPathSplineDistances[p],
					pathResultInfo.m_fPathCtrlPt2TVals[p],
					pathResultInfo.m_iMinApproxFreeSpaceApproachingEachWaypoint[p]
				);
			}
		}

		//************************************************************************************
		// If this was a route on top of a dynamic navmesh, make a backup copy of the route
		// in the local-space of the navmesh.  We retransform it into world space by the
		// entity's matrix every frame.

		if(m_pOnEntityRouteData && m_pOnEntityRouteData->m_pDynamicEntityRouteIsOn)
		{
			// If for some reason the dynamic navmesh instance wasn't found, then
			// restart/quit the task?  This is an error situation.
			if(!pathResultInfo.m_bFoundDynamicEntityPathIsOn)
			{
				CPathServer::UnlockPathResultAndClear(m_iPathHandle);
				m_iLastPathHandle = m_iPathHandle;
				m_iPathHandle = PATH_HANDLE_NULL;

				m_NavBaseConfigFlags.Reset(CNavBaseConfigFlags::NB_KeepMovingWhilstWaitingForPath);
				SetState(NavBaseState_Initial);

				return false;
			}

			if(!m_pOnEntityRouteData->m_pLocalSpaceRoute)
				m_pOnEntityRouteData->m_pLocalSpaceRoute = rage_new CNavMeshRoute;
			else
				m_pOnEntityRouteData->m_pLocalSpaceRoute->Clear();

			Matrix34 mInvMat;
			mInvMat.Inverse(pathResultInfo.m_MatrixOfEntityAtPathCreationTime);
			for(int r=0; r<pTargetRoute->GetSize(); r++)
			{
				Vector3 vLocalPos;
				mInvMat.Transform( pTargetRoute->GetPosition(r), vLocalPos );
				m_pOnEntityRouteData->m_pLocalSpaceRoute->Add( vLocalPos, pTargetRoute->GetWaypoint(r) );
			}

			m_iProgress = 0;
		}

		//******************************************************************************************
		// Test whether the path start was inside a VEHICLE_TYPE_PLANE; if so then we enable
		// a special-case avoidance whereby CEventVehicleCollision may create a response task of
		// CTaskMoveWalkRoundVehicle vehicle.  This is because we cannot simply resolve the start
		// position directly outside of the closest edge, due to the plane's wings, propeller, etc.

		if( pathResultInfo.m_bStartPositionWasInsideAnObject && m_NavBaseConfigFlags.IsSet(CNavBaseConfigFlags::NB_AvoidObjects) )
		{
			CEntityScannerIterator vehiclesList = pPed->GetPedIntelligence()->GetNearbyVehicles();
			for( const CVehicle* pVeh = (const CVehicle*)vehiclesList.GetFirst(); pVeh; pVeh = (const CVehicle*)vehiclesList.GetNext() )
			{
				if(pVeh->GetVehicleType()==VEHICLE_TYPE_PLANE)
				{
					CEntityBoundAI vehicleBound;
					vehicleBound.Set(*pVeh, pVeh->GetTransform().GetPosition().GetZf(), pPed->GetCapsuleInfo()->GetHalfWidth(), true, NULL);
					Vector3 vPedPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
					if(vPedPos.z >= vehicleBound.GetBottomZ() && vPedPos.z < vehicleBound.GetTopZ() && vehicleBound.LiesInside(vPedPos))
					{
						m_NavBaseInternalFlags.Set(CNavBaseInternalFlags::NB_PathStartedInsidePlaneVehicle, true);
						break;
					}
				}
			}			
		}

		CPathServer::UnlockPathResultAndClear(m_iPathHandle);
		m_iLastPathHandle = m_iPathHandle;
		m_iPathHandle = PATH_HANDLE_NULL;

		return pTargetRoute->GetSize()!=0;
	}
	else
	{
		CPathServer::UnlockPathResultAndClear(m_iPathHandle);
		m_iLastPathHandle = m_iPathHandle;
		m_iPathHandle = PATH_HANDLE_NULL;
		return false;
	}
}


bool CTaskNavBase::CanSwimStraightToNextTarget(CPed * pPed) const
{
	if (!pPed->GetCurrentMotionTask()->IsUnderWater())
	{
		return false;
	}

	if(GetState()!=NavBaseState_FollowingPath || !m_pRoute || m_pRoute->GetSize() <= 2 || m_iProgress >= m_pRoute->GetSize()-1)
	{
		return false;
	}

	const Vector3 vPedPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
	const Vector3 vNextWaypointPos = m_pRoute->GetPosition(m_iProgress+1);

	float fStartWaterLevel, fEndWaterLevel;

	if(Water::GetWaterLevelNoWaves(vPedPos, &fStartWaterLevel, true, REJECTIONABOVEWATER, NULL))
	{
		if(Water::GetWaterLevelNoWaves(vNextWaypointPos, &fEndWaterLevel, true, REJECTIONABOVEWATER, NULL))
		{
			if(vPedPos.z < fStartWaterLevel && vNextWaypointPos.z < fEndWaterLevel)
			{
				WorldProbe::CShapeTestProbeDesc probeDesc;
				probeDesc.SetStartAndEnd(vPedPos, vNextWaypointPos);
				probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES|ArchetypeFlags::GTA_VEHICLE_TYPE|ArchetypeFlags::GTA_OBJECT_TYPE);
				const bool bClearLos = !WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc);

				return bClearLos;
			}
		}
	}

	return false;
}

void CTaskNavBase::ScanForMovedOutsidePlaneBounds(CPed * pPed)
{
	Assert(m_NavBaseInternalFlags.IsSet(CNavBaseInternalFlags::NB_PathStartedInsidePlaneVehicle));

	CEntityScannerIterator vehiclesList = pPed->GetPedIntelligence()->GetNearbyVehicles();
	for( const CVehicle* pVeh = (const CVehicle*)vehiclesList.GetFirst(); pVeh; pVeh = (const CVehicle*)vehiclesList.GetNext() )
	{
		if(pVeh->GetVehicleType()==VEHICLE_TYPE_PLANE)
		{
			CEntityBoundAI vehicleBound;
			vehicleBound.Set(*pVeh, pVeh->GetTransform().GetPosition().GetZf(), pPed->GetCapsuleInfo()->GetHalfWidth(), true, NULL);
			Vector3 vPedPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
			if(vPedPos.z >= vehicleBound.GetBottomZ() && vPedPos.z < vehicleBound.GetTopZ() && vehicleBound.LiesInside(vPedPos))
			{
				return;
			}
		}
	}			

	// Reset the flag to false
	m_NavBaseInternalFlags.Set(CNavBaseInternalFlags::NB_PathStartedInsidePlaneVehicle, false);
}

bool CTaskNavBase::CreateRouteFollowingTask(CPed * pPed)
{
	Assert(m_pRoute && m_pRoute->GetSize()>0);
	Assert(m_iProgress >= 0 && m_iProgress < m_pRoute->GetSize());
	Assertf(m_fMoveBlendRatio >= MOVEBLENDRATIO_STILL && m_fMoveBlendRatio <= MOVEBLENDRATIO_SPRINT, "Move blend ratio was : %.1f", m_fMoveBlendRatio);

	m_fMoveBlendRatio = Clamp(m_fMoveBlendRatio, MOVEBLENDRATIO_STILL, MOVEBLENDRATIO_SPRINT);

	const int iExpandedRouteIndex = Min(m_iClosestSegOnExpandedRoute + 1, ms_iNumExpandedRouteVerts - 1);
	const Vector3 & vInitialTarget = RCC_VECTOR3(ms_vExpandedRouteVerts[ iExpandedRouteIndex ].GetPosRef());

	Assert( rage::FPIsFinite(vInitialTarget.x) && rage::FPIsFinite(vInitialTarget.y) && rage::FPIsFinite(vInitialTarget.z) );

	m_fCurrentTargetRadius = GetTargetRadiusForWaypoint(m_iProgress);

	CalcTimeNeededToCompleteRouteSegment(pPed, m_iProgress);

	if (GetConsiderRunStartForPathLookAhead())
	{
		m_fCurrentLookAheadDistance = Max(m_fCurrentLookAheadDistance, Max(m_fMoveBlendRatio * 0.5f, 0.5f));
	}
	else if (m_fMoveBlendRatio >= MBR_RUN_BOUNDARY)
	{
		// We could check the angle between the next segments and if they are more narrow than X we might need a lower look ahead distance
		// For this range we assume that ped radius is 0.35f
		m_fCurrentLookAheadDistance = Max(m_fCurrentLookAheadDistance, 0.707f);	// 1.0f
	}

	m_NavBaseInternalFlags.Reset(CNavBaseInternalFlags::NB_HasSlidTargetAlongSpline);
	m_NavBaseInternalFlags.Reset(CNavBaseInternalFlags::NB_CheckedStoppingDistance);

	bool bApproachingWaypoint = false;
	float fWaypointHeading = 0.0f;
	for(s32 p=0; p<m_pRoute->GetSize(); p++)
	{
		if( IsSpecialActionClimbOrDrop( m_pRoute->GetWaypoint(p).GetSpecialActionFlags() ) )
		{
			bApproachingWaypoint = true;
			fWaypointHeading = m_pRoute->GetWaypoint(p).GetHeading();
			break;
		}
	}

	bool bStopAtEnd = bApproachingWaypoint ||
		(GetLastRoutePointIsTarget() &&
		//(m_NavBaseInternalFlags.IsSet(CNavBaseInternalFlags::NB_LastRoutePointIsTarget) &&
		!m_NavBaseConfigFlags.IsSet(CNavBaseConfigFlags::NB_DontStandStillAtEnd) &&
		!m_NavBaseInternalFlags.IsSet(CNavBaseInternalFlags::NB_GotoTaskSequencedAfter));

	if(bStopAtEnd)
	{
		if(bApproachingWaypoint)
		{
			const float fBoundary = (MOVEBLENDRATIO_WALK + MBR_RUN_BOUNDARY) * 0.5f; // err on the side of caution
			const float fHeading = (m_fMoveBlendRatio <= fBoundary) ? fWaypointHeading : DEFAULT_NAVMESH_FINAL_HEADING;

			CTaskMoveGoToPointAndStandStill * pGotoTask = rage_new CTaskMoveGoToPointAndStandStill(
				m_fMoveBlendRatio,
				vInitialTarget,
				m_fCurrentTargetRadius,
				0.0f,
				false,
				true,
				fHeading);

			SetNewTask(pGotoTask);

			pGotoTask->SetStandStillDuration(0);
			pGotoTask->SetDontCompleteThisFrame();
			pGotoTask->SetAllowNavmeshAvoidance(m_NavBaseConfigFlags.IsSet(CNavBaseConfigFlags::NB_SetAllowNavmeshAvoidance));
			pGotoTask->SetDistanceFromPathToEnableNavMeshAvoidance(ms_fDistanceFromPathToTurnOnNavMeshAvoidance); 
			pGotoTask->SetDistanceFromPathToEnableObstacleAvoidance(ms_fDistanceFromPathToTurnOnObjectAvoidance);
			pGotoTask->SetDistanceFromPathToEnableVehicleAvoidance(ms_fDistanceFromPathToTurnOnVehicleAvoidance);
			pGotoTask->SetDistanceFromPathTolerance(GetPathToleranceForWaypoint(m_iProgress));

			pGotoTask->SetAvoidPeds(m_NavBaseConfigFlags.IsSet(CNavBaseConfigFlags::NB_AvoidPeds));
			pGotoTask->SetLenientAchieveHeadingForExactStop(GetLenientAchieveHeadingForExactStop());

			Vector3 vGotoPointTarget, vLookAheadTarget;
			ProcessLookAhead(pPed, false, vGotoPointTarget, vLookAheadTarget);

			pGotoTask->SetTarget(pPed, VECTOR3_TO_VEC3V(vGotoPointTarget));
			pGotoTask->SetLookAheadTarget(pPed, vGotoPointTarget);	// Should this be using vLookAheadTarget?

			Assert( rage::FPIsFinite(vGotoPointTarget.x) && rage::FPIsFinite(vGotoPointTarget.y) && rage::FPIsFinite(vGotoPointTarget.z) );
		}
		else
		{
			const bool bStopExactly = m_NavBaseConfigFlags.IsSet(CNavBaseConfigFlags::NB_StopExactly) || m_fTargetStopHeading < DEFAULT_NAVMESH_FINAL_HEADING;

			CTaskMoveGoToPointAndStandStill * pGotoTask = rage_new CTaskMoveGoToPointAndStandStill(
				m_fMoveBlendRatio,
				vInitialTarget,
				m_fCurrentTargetRadius,
				bStopExactly ? 0.0f : m_fSlowDownDistance,
				false,
				bStopExactly,
				m_fTargetStopHeading);

			SetNewTask(pGotoTask);

			pGotoTask->SetStandStillDuration(0);
			pGotoTask->SetDontCompleteThisFrame();
			pGotoTask->SetAllowNavmeshAvoidance(m_NavBaseConfigFlags.IsSet(CNavBaseConfigFlags::NB_SetAllowNavmeshAvoidance));
			pGotoTask->SetDistanceFromPathToEnableNavMeshAvoidance(ms_fDistanceFromPathToTurnOnNavMeshAvoidance); 
			pGotoTask->SetDistanceFromPathToEnableObstacleAvoidance(ms_fDistanceFromPathToTurnOnObjectAvoidance);
			pGotoTask->SetDistanceFromPathToEnableVehicleAvoidance(ms_fDistanceFromPathToTurnOnVehicleAvoidance);
			pGotoTask->SetDistanceFromPathTolerance(GetPathToleranceForWaypoint(m_iProgress));
				

			pGotoTask->SetAvoidPeds(m_NavBaseConfigFlags.IsSet(CNavBaseConfigFlags::NB_AvoidPeds));
			pGotoTask->SetLenientAchieveHeadingForExactStop(GetLenientAchieveHeadingForExactStop());
			pGotoTask->SetUseRunstopInsteadOfSlowingToWalk(!m_NavBaseConfigFlags.IsSet(CNavBaseConfigFlags::NB_SlowDownInsteadOfUsingRunStops));

			Vector3 vGotoPointTarget, vLookAheadTarget;
			ProcessLookAhead(pPed, false, vGotoPointTarget, vLookAheadTarget);
			pGotoTask->SetTarget(pPed, VECTOR3_TO_VEC3V(vGotoPointTarget));
			pGotoTask->SetLookAheadTarget(pPed, vLookAheadTarget);

			Assert( rage::FPIsFinite(vGotoPointTarget.x) && rage::FPIsFinite(vGotoPointTarget.y) && rage::FPIsFinite(vGotoPointTarget.z) );
			Assert( rage::FPIsFinite(vLookAheadTarget.x) && rage::FPIsFinite(vLookAheadTarget.y) && rage::FPIsFinite(vLookAheadTarget.z) );
		}
	}
	else
	{
		CTaskMoveGoToPoint * pGotoTask = rage_new CTaskMoveGoToPoint(
			m_fMoveBlendRatio,
			vInitialTarget,
			m_fCurrentTargetRadius,
			false);

		SetNewTask(pGotoTask);

		pGotoTask->SetDontCompleteThisFrame();
		pGotoTask->SetAllowNavmeshAvoidance(m_NavBaseConfigFlags.IsSet(CNavBaseConfigFlags::NB_SetAllowNavmeshAvoidance));
		pGotoTask->SetDistanceFromPathToEnableNavMeshAvoidance(ms_fDistanceFromPathToTurnOnNavMeshAvoidance); 
		pGotoTask->SetDistanceFromPathToEnableObstacleAvoidance(ms_fDistanceFromPathToTurnOnObjectAvoidance);
		pGotoTask->SetDistanceFromPathToEnableVehicleAvoidance(ms_fDistanceFromPathToTurnOnVehicleAvoidance);
		pGotoTask->SetDistanceFromPathTolerance(GetPathToleranceForWaypoint(m_iProgress));

		pGotoTask->SetAvoidPeds(m_NavBaseConfigFlags.IsSet(CNavBaseConfigFlags::NB_AvoidPeds));

		Vector3 vGotoPointTarget, vLookAheadTarget;
		ProcessLookAhead(pPed, false, vGotoPointTarget, vLookAheadTarget);
		pGotoTask->SetTarget(pPed, VECTOR3_TO_VEC3V(vGotoPointTarget));
		pGotoTask->SetLookAheadTarget(pPed, vLookAheadTarget);

		Assert( rage::FPIsFinite(vGotoPointTarget.x) && rage::FPIsFinite(vGotoPointTarget.y) && rage::FPIsFinite(vGotoPointTarget.z) );
		Assert( rage::FPIsFinite(vLookAheadTarget.x) && rage::FPIsFinite(vLookAheadTarget.y) && rage::FPIsFinite(vLookAheadTarget.z) );
	}

	return true;
}
void CTaskNavBase::CheckForInitialDoorObstruction(CPed * pPed, const Vector3 & vTarget)
{
	if(!pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_JustLeftCarNotCheckedForDoors ) || !pPed->GetMyVehicle() || !pPed->GetMyVehicle()->GetCurrentPhysicsInst())
		return;

	pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_JustLeftCarNotCheckedForDoors, false );

	WorldProbe::CShapeTestCapsuleDesc capsuleDesc;
	WorldProbe::CShapeTestFixedResults<> shapeTestResult;
	capsuleDesc.SetResultsStructure(&shapeTestResult);

	const Vector3 vPedPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());

	capsuleDesc.SetCapsule(vPedPosition, vTarget, pPed->GetCapsuleInfo()->GetHalfWidth());
	capsuleDesc.SetDomainForTest(WorldProbe::TEST_AGAINST_INDIVIDUAL_OBJECTS);
	capsuleDesc.SetIncludeEntity(pPed->GetMyVehicle());
	capsuleDesc.SetDoInitialSphereCheck(true);
	capsuleDesc.SetIsDirected(true);

	if(!WorldProbe::GetShapeTestManager()->SubmitTest(capsuleDesc))
		return;

	const CCarDoor* pDoor = CCarEnterExit::GetCollidedDoorFromCollisionComponent(pPed->GetMyVehicle(), shapeTestResult[0].GetHitComponent());
	if(!pDoor)
		return;

	// Our path hits a car door which is sufficiently open to be a pain in the ass.
	if(pDoor && pDoor->GetDoorRatio() > 0.1f)
	{
		// Generate a vehicle collision event which will force the ped to walk around this door.
		Vector3 vColPos = shapeTestResult[0].GetHitPosition();
		vColPos.z = vPedPosition.z;
		Vector3 vToPed = vPedPosition - vColPos;
		vToPed.NormalizeSafe();

		static const float fImpulseMag = 1.0f;
		CEventVehicleCollision eventCollision(pPed->GetMyVehicle(), vToPed, vColPos, fImpulseMag, shapeTestResult[0].GetHitComponent(), m_fMoveBlendRatio);
		eventCollision.SetIsForcedDoorCollision();
		pPed->GetPedIntelligence()->AddEvent(eventCollision);
	}
}

// NAME : GetNearbyEntitiesForLineOfSightTest
// PURPOSE : Retrieve an array of nearby entities (objects and vehicles) which would pose obstacles given the current pathfinding settings
// Returns populated CEntityBoundsAI array, and pEntityArray; note that entries in the latter will be NULL for vehicle doors

s32 CTaskNavBase::GetNearbyEntitiesForLineOfSightTest(CPed * pPed, CEntityBoundAI * pEntityBoundsArray, const CEntity ** pEntityArray, const s32 iNumClosest, const s32 iMaxNum, const float fPedRadius, const float fPedZ) const
{
#if __BANK
	static dev_bool bDebugDraw = false;
#endif

	CEntity * pEntityPathingOn =
		(pPed->GetGroundPhysical() && pPed->GetGroundPhysical()->GetIsTypeVehicle() && ((CVehicle*)pPed->GetGroundPhysical())->m_nVehicleFlags.bHasDynamicNavMesh)
			? pPed->GetGroundPhysical() : NULL;

	s32 iNumEntities = 0;

	if( m_NavBaseConfigFlags.IsSet(CNavBaseConfigFlags::NB_AvoidEntityTypeObject) )
	{
		// If entity scanning is off, we must force a scan or be unaware of nearby objects
		if(pPed->GetPedAiLod().IsLodFlagSet(CPedAILod::AL_LodEntityScanning))
		{
			pPed->GetPedIntelligence()->GetObjectScanner()->ScanForEntitiesInRange(*static_cast<CEntity*>(pPed), true);
		}

		s32 iNumObjects = 0;
		const CEntityScannerIterator objectsList = pPed->GetPedIntelligence()->GetNearbyObjects();
		for( const CObject* pObj = (const CObject*)objectsList.GetFirst(); pObj; pObj = (const CObject*)objectsList.GetNext() )
		{
			// Obviously, don't recalculate our path for anything which is not an obstacle in the navmesh
			// This also prevents us having to do a million tests here for "isPickUp", "isPedProp", etc.
			if( !pObj->IsInPathServer() )
				continue;

			if( pObj == pEntityPathingOn )
				continue;

			// Always ignore unlocked doors
			if( pObj->IsADoor() && !pObj->GetIsAnyFixedFlagSet() )
				continue;

			// Always ignore anything which is marked as "no avoid"
			if( pObj->GetBaseModelInfo()->GetNotAvoidedByPeds() )
				continue;

			// Create the CEntityBoundAI here - because we need it to perform out final 'GetIsClimbableByAI' test immediately after
			pEntityBoundsArray[iNumEntities].Set( *pObj, fPedZ, (pObj->IsReducedBBForPathfind() ? 0.f : fPedRadius), true, NULL );

			// If we specified not to adjust the target position, then ignore any entities which are intersecting it
			if( m_NavBaseConfigFlags.IsSet(CNavBaseConfigFlags::NB_DontAdjustTargetPosition) &&
				pEntityBoundsArray[iNumEntities].LiesInside(m_vTarget))
			{
				continue;
			}

			// Ignore any objects which are marked as climbable by AI, if we are climbing them.
			// How do we decide whether we're climbing them?
			// If we have a climb waypoint in the close vicinity to the object, I suppose.
			if( pObj->GetBaseModelInfo()->GetIsClimbableByAI() ) // && !pObj->m_nObjectFlags.bHasBeenUprooted )
			{
				// If the route has any object climbs
				if( (m_pRoute->GetPossiblySetSpecialActionFlags() & WAYPOINT_FLAG_CLIMB_OBJECT) != 0)
				{
					bool bShouldIgnore = false;
					for(s32 p=m_iProgress; p<m_pRoute->GetSize(); p++)
					{
						if( (m_pRoute->GetWaypoint(p).GetSpecialActionFlags() & WAYPOINT_FLAG_CLIMB_OBJECT) != 0)
						{
							if( pEntityBoundsArray[iNumEntities].LiesInside(m_pRoute->GetPosition(p), 1.0f) )
							{
								bShouldIgnore = true;
								break;
							}
						}
					}
					if(bShouldIgnore)
						continue;
				}
			}

			pEntityArray[iNumEntities] = pObj;

#if __BANK
			if(bDebugDraw)
				pEntityBoundsArray[iNumEntities].fwDebugDraw(Color_cyan);
#endif

			iNumEntities++;
			if(iNumEntities == iMaxNum)
				return iMaxNum;
			iNumObjects++;
			if(iNumObjects == iNumClosest)
				break;
		}
	}

	int iIgnoreComponents[32];	// See TDynObjBoundsFuncs::CalcBoundsForEntity() for the origins of this code

	if( m_NavBaseConfigFlags.IsSet(CNavBaseConfigFlags::NB_AvoidEntityTypeVehicle) )
	{
		CPhysical * pVehicleEntering = NULL;
		CTaskMoveGoToVehicleDoor * pGotoDoor = (CTaskMoveGoToVehicleDoor*) pPed->GetPedIntelligence()->FindTaskActiveMovementByType(CTaskTypes::TASK_MOVE_GO_TO_VEHICLE_DOOR);
		if(pGotoDoor)
			pVehicleEntering = pGotoDoor->GetTargetVehicle();

		// If entity scanning is off, we must force a scan or be unaware of nearby vehicles
		if(pPed->GetPedAiLod().IsLodFlagSet(CPedAILod::AL_LodEntityScanning))
		{
			pPed->GetPedIntelligence()->GetVehicleScanner()->ScanForEntitiesInRange(*static_cast<CEntity*>(pPed), true);
		}

		s32 iNumVehicles = 0;
		const CEntityScannerIterator vehiclesList = pPed->GetPedIntelligence()->GetNearbyVehicles();
		for( const CVehicle* pVehicle = (const CVehicle*)vehiclesList.GetFirst(); pVehicle; pVehicle = (const CVehicle*)vehiclesList.GetNext() )
		{
			if( !pVehicle->IsInPathServer() )
				continue;

			if( pVehicle->InheritsFromBike() && pVehicle->IsOnItsSide() )
				continue;

			if( pVehicle == pEntityPathingOn )
				continue;

			if( pVehicle == pVehicleEntering )
				continue;

			if( !pVehicle->GetActiveForPedNavitation() )
				continue;

			int iNumIgnoreComponents = TDynObjBoundsFuncs::GetIgnoreComponentsForEntity(pVehicle, iIgnoreComponents, 32);
			Assertf(iNumIgnoreComponents <= 32, "This is very bad, memory overwrite!");

			pEntityBoundsArray[iNumEntities].Set( *pVehicle, fPedZ, (pVehicle->IsReducedBBForPathfind() ? 0.f : fPedRadius), true, NULL, iNumIgnoreComponents, iIgnoreComponents );
			pEntityArray[iNumEntities] = pVehicle;

#if __BANK
			if(bDebugDraw)
				pEntityBoundsArray[iNumEntities].fwDebugDraw(Color_cyan);
#endif

			iNumEntities++;
			if(iNumEntities == iMaxNum)
				return iMaxNum;

			// Add the doors for this vehicle
			// TODO: The code ignores doors if the path was set up to allow them being pushed closed.
			// This is *not* correct behaviour; to do this correctly we'll need to logic into the CEntityBoundAI
			// so that it is aware of which direction a door can be pushed/ignored.
			// (Same as is done in the pathserver object LOS - see __ALLOW_TO_PUSH_VEHICLE_DOORS_CLOSED for reference)

			if(!GetAllowToPushVehicleDoorsClosed())
			{
				for(s32 d=0; d<pVehicle->GetNumDoors(); d++)
				{
					const CCarDoor * pDoor = pVehicle->GetDoor(d);
					if(pDoor->GetDoorRatio() > SMALL_FLOAT)
					{
						if( pEntityBoundsArray[iNumEntities].Set(*pDoor, pVehicle, fPedZ, fPedRadius, true) )
						{
							pEntityArray[iNumEntities] = NULL;	// Can't set CCarDoor in here, since it doesn't derive from CEntity
#if __BANK
							if(bDebugDraw)
								pEntityBoundsArray[iNumEntities].fwDebugDraw(Color_cyan);
#endif

							iNumEntities++;
							if(iNumEntities == iMaxNum)
								return iMaxNum;
						}
					}
				}
			}

			// Iterate to next vehicle
			iNumVehicles++;
			if(iNumVehicles == iNumClosest)
				break;
		}
	}

	// Add the nearby fires
	if(pPed->GetPedIntelligence()->GetNavCapabilities().IsFlagSet(CPedNavCapabilityInfo::FLAG_AVOID_FIRE))
	{
		// If entity scanning is off, we must force a scan or be unaware of nearby fires
		if(pPed->GetPedAiLod().IsLodFlagSet(CPedAILod::AL_LodEventScanning))
		{
			pPed->GetPedIntelligence()->GetEventScanner()->GetFireScanner().Scan(*pPed, true);
		}

		Matrix34 fireMat;

		const Vector3 * pNearbyFires = pPed->GetPedIntelligence()->GetEventScanner()->GetFireScanner().GetNearbyFires();
		for(s32 f=0; f<CNearbyFireScanner::NUM_NEARBY_FIRES; f++)
		{
			const Vector3 & vFirePosAndRadius = pNearbyFires[f];
			if(vFirePosAndRadius.IsZero())
				break;

			Vector3 vFireMin( -vFirePosAndRadius.w, -vFirePosAndRadius.w, -1.0f );
			Vector3 vFireMax( vFirePosAndRadius.w, vFirePosAndRadius.w, 1.5f );
			fireMat.MakeRotateZ( fwRandom::GetRandomNumberInRange(-PI, PI) );
			fireMat.d = vFirePosAndRadius;

			pEntityBoundsArray[iNumEntities].Set(vFireMin, vFireMax, fireMat, fPedZ, fPedRadius, true);

			pEntityArray[iNumEntities] = NULL;	// We have no entity pointer to set in here
#if __BANK
			if(bDebugDraw)
				pEntityBoundsArray[iNumEntities].fwDebugDraw(Color_cyan);
#endif
			iNumEntities++;
			if(iNumEntities == iMaxNum)
				return iMaxNum;
		}
	}

	return iNumEntities;
}

void CTaskNavBase::SetScanForObstructionsFrequency(u32 iFrequencyMS)
{
	m_ScanForObstructionsTimer.ChangeDuration(iFrequencyMS);
}

void CTaskNavBase::SetScanForObstructionsNextFrame()
{
	m_ScanForObstructionsTimer.SubtractTime( m_ScanForObstructionsTimer.GetDuration() );
}

u32 CTaskNavBase::GetTimeSinceLastScanForObstructions()
{
	if(m_ScanForObstructionsTimer.IsSet())
	{
		return m_ScanForObstructionsTimer.GetTimeElapsed();
	}
	// If timer is not set, then we'll return the maximum time deltad
	else
	{
		return CalcScanForObstructionsFreq();
	}
}

u32 CTaskNavBase::CalcScanForObstructionsFreq() const
{
	if(m_fInitialMoveBlendRatio >= MBR_SPRINT_BOUNDARY)
	{
		return ms_iScanForObstructionsFreq_Sprint;
	}
	else if(m_fInitialMoveBlendRatio >= MBR_RUN_BOUNDARY)
	{
		return ms_iScanForObstructionsFreq_Run;
	}
	else
	{
		return ms_iScanForObstructionsFreq_Walk;
	}
}


u32 CTaskNavBase::GetInitialScanForObstructionsTime()
{
	static dev_u32 iDefaultTime = 1000;
	return iDefaultTime;
}

// NAME : ScanForObstructions
// PURPOSE : Scan ahead of the ped for object/vehicle obstructions on path segments
// Obstacles may have moved into position after the route was planned, or may have
// converted from dummy to real in the meantime.
// RETURNS: true if there is an obstruction on the route
//
// TODO: We need to factor in the logic currently in CTaskMoveFollowNavMesh, concerning
// route modes such as objects' bounding-boxes being reduced.  Otherwise we will falsely
// identify object collisions.

bool CTaskNavBase::ScanForObstructions(CPed * pPed, const float fMaxDistanceAheadToScan, float & fOut_DistanceToObstruction) const
{
	const Vector3 vPedPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
	const float fPedRadius =
		(m_iNavMeshRouteMethod==eRouteMethod_ReducedObjectBoundingBoxes) ?
			0.0f : PATHSERVER_PED_RADIUS - 0.1f;	// TODO: Should use capsule info here

	fOut_DistanceToObstruction = 0.0f;

#if __BANK
	static dev_s32 iDbgFrames = 10000;
	static dev_bool bDebugDraw = false;
#endif

	// NB: I am not going to special-case "eRouteMethod_OnlyAvoidNonMovableObjects" in here, in the understanding
	// that ScanForObstructions() will be run only periodically and that it will be advantageous to kick the
	// ped out of "eRouteMethod_OnlyAvoidNonMovableObjects" mode when possible.

	// What we are doing here is really this:
	//	CEntityBoundAI entityBounds[CEntityScanner::MAX_NUM_ENTITIES*2];
	// but we put them in a vector-aligned buffer without calling the constructor. This is done
	// to reduce performance overhead, we are always going to initialize the ones we use separately anyway,
	// and while CEntityBoundAI's constructor doesn't currently initialize anything anyway, it contains
	// a spdSphere which has a default constructor that writes to memory, so we avoid doing a bunch of
	// extra memory accesses and possibly function calls by not calling it.
	// The reason for using Vec4V rather than an aligned array of chars or something is that the alignment
	// declarations tend to be a bit fragile across platforms.
	Vec4V entityBoundsBuffer[CEntityScanner::MAX_NUM_ENTITIES*2*((sizeof(CEntityBoundAI) + sizeof(Vec4V) - 1)/sizeof(Vec4V))];
	CompileTimeAssert(sizeof(entityBoundsBuffer) >= sizeof(CEntityBoundAI)*CEntityScanner::MAX_NUM_ENTITIES*2);
	CompileTimeAssert(__alignof(CEntityBoundAI) <= sizeof(Vec4V));
	CEntityBoundAI* entityBounds = reinterpret_cast<CEntityBoundAI*>(entityBoundsBuffer);

	const CEntity * entities[CEntityScanner::MAX_NUM_ENTITIES*2];

	const int iNumEntities = GetNearbyEntitiesForLineOfSightTest( pPed, entityBounds, entities, CEntityScanner::MAX_NUM_ENTITIES*2, CEntityScanner::MAX_NUM_ENTITIES*2, fPedRadius, vPedPos.z );

	if( !iNumEntities )
	{
		return false;
	}

	ScalarV distToObstructionV(V_ZERO);

	const ScalarV maxDistanceAheadToScanV(fMaxDistanceAheadToScan);

	const int iStartPoint = m_iClosestSegOnExpandedRoute+1;
	Vec3V endV;

	// Start from 'm_vClosestPosOnRoute'
	if(iStartPoint < ms_iNumExpandedRouteVerts)
	{
		endV = RCC_VEC3V(m_vClosestPosOnRoute);
	}
	for( int v=iStartPoint; v<ms_iNumExpandedRouteVerts && IsLessThanAll(distToObstructionV, maxDistanceAheadToScanV); v++)
	{
		const Vec3V startV = endV;		// This segment starts where the previous one ended.
		endV = ms_vExpandedRouteVerts[v].GetPos();

		// Check for special action flags;
		// We will not scan beyond a special action link, unless this is the first line-segment which we
		// are testing - which would indicate that the peds has already passed that action.
		if( IsSpecialActionClimbOrDrop(ms_vExpandedRouteVerts[v-1].m_iWptFlag.GetSpecialActionFlags() ) )
		{
			if( v == iStartPoint )
			{
				// If prev waypoint was a climb, then ignore the segment to the next one
				distToObstructionV = Add(distToObstructionV, DistFast(startV, endV));
				continue;
			}
			else
			{
				// If we have reached a special action waypoint, then abort scanning
				return false;
			}
		}

		// Put the start and end Z coordinates into a single vector, for faster comparisons with entity bound Z limits within the loop.
		const Vec4V startEndZ4V = GetFromTwo<Vec::Z1, Vec::Z1, Vec::Z2, Vec::Z2>(startV, endV);

		for( s32 e=0; e<iNumEntities; e++)
		{
			// Test whether this entity is above or below this route
			// NB: This is not even nearly accurate enough, and simply throws out any entity which is above/below
			// either the route start or route end.  Ideally we should clip start/end same as in TestLineOfSight(), below.

			const Vec4V topZ4V(entityBounds[e].GetTopZV());
			const Vec4V bottomZ4V(entityBounds[e].GetBottomZV());
			const VecBoolV isAboveV = IsGreaterThan(startEndZ4V, topZ4V);
			const VecBoolV isBelowV = IsLessThan(startEndZ4V, bottomZ4V);
			const VecBoolV isAboveOrBelowV = Or(isAboveV, isBelowV);
			if(!IsFalseAll(isAboveOrBelowV))
			{
				Assert(startV.GetZf() > entityBounds[e].GetTopZ() || endV.GetZf() > entityBounds[e].GetTopZ() || startV.GetZf() < entityBounds[e].GetBottomZ() || endV.GetZf() < entityBounds[e].GetBottomZ());
				continue;
			}
			Assert(!(startV.GetZf() > entityBounds[e].GetTopZ() || endV.GetZf() > entityBounds[e].GetTopZ() || startV.GetZf() < entityBounds[e].GetBottomZ() || endV.GetZf() < entityBounds[e].GetBottomZ()));

			ScalarV distToIsectV;
			if ( !entityBounds[e].TestLineOfSightReturnDistance( startV, endV, distToIsectV ) )
			{
#if __BANK
				if( bDebugDraw )
				{
					grcDebugDraw::Line(VEC3V_TO_VECTOR3(startV) - (ZAXIS*0.5f), VEC3V_TO_VECTOR3(endV) - (ZAXIS*0.5f), Color_red, Color_red, iDbgFrames);

					Vector3 vHit1, vHit2;
					if(entityBounds[e].ComputeCrossingPoints(VEC3V_TO_VECTOR3(startV), VEC3V_TO_VECTOR3(endV), vHit1, vHit2))
					{
						Vector3 vHit = (vHit1-VEC3V_TO_VECTOR3(startV)).Mag2() < (vHit2-VEC3V_TO_VECTOR3(startV)).Mag2() ? vHit1 : vHit2;
						grcDebugDraw::Sphere(vHit - (ZAXIS*0.5f), 0.3f, Color_red, false, iDbgFrames);
					}
				}
#endif
				distToObstructionV = Add(distToObstructionV, distToIsectV);

				StoreScalar32FromScalarV(fOut_DistanceToObstruction, distToObstructionV);
				return true;
			}
		}
#if __BANK
		if( bDebugDraw )
		{
			grcDebugDraw::Line(VEC3V_TO_VECTOR3(startV) - (ZAXIS*0.5f), VEC3V_TO_VECTOR3(endV) - (ZAXIS*0.5f), Color_orange, Color_orange, 50);
		}
#endif
		distToObstructionV = Add(distToObstructionV, DistFast(startV, endV));
	}

	// Note: we currently don't bother writing out the distance in this case, though we could if there
	// was any benefit in the caller knowing the length of the path.

	return false;
}


// NAME : IsMovingIntoObstacle
// PURPOSE : Determine whether ped is moving into specified entity types, based upon examining this frame's collision records

bool CTaskNavBase::IsMovingIntoObstacle(const CPed * pPed, bool bVehicles, bool bObjects) const
{
	const CCollisionHistory* pCollisionHistory = pPed->GetFrameCollisionHistory();
	if(pCollisionHistory->GetNumCollidedEntities() == 0)
		return false;

	Vector3 vMoveDirLocal(pPed->GetMotionData()->GetCurrentMbrX(), pPed->GetMotionData()->GetCurrentMbrY(), 0.0f);
	if(vMoveDirLocal.Mag2() < MBR_WALK_BOUNDARY)
		return false;
	vMoveDirLocal.Normalize();

	static dev_float fIgnoreImpulseMag = 0.0f;
	Vector3 vLocalNormal;
	const CCollisionRecord* pColRecord;

	//-----------------------------------
	// Examine vehicle collision records

	if(bVehicles)
	{
		for(pColRecord = pCollisionHistory->GetFirstVehicleCollisionRecord(); pColRecord != NULL ; pColRecord = pColRecord->GetNext())
		{
			// Maybe rule out vehicles we are entering/leaving ?
			//CVehicle * pVehicle = static_cast<CVehicle*>(pColRecord->m_pRegdCollisionEntity.Get());

			if(pColRecord->m_fCollisionImpulseMag < fIgnoreImpulseMag)
				continue;

			vLocalNormal = VEC3V_TO_VECTOR3(pPed->GetTransform().UnTransform3x3(RCC_VEC3V(pColRecord->m_MyCollisionNormal)));
			if(DotProduct(vMoveDirLocal, vLocalNormal) < -0.707f)
			{
				return true;
			}
		}
	}

	//----------------------------------
	// Examine object collision records

	if(bObjects)
	{
		for(pColRecord = pCollisionHistory->GetFirstObjectCollisionRecord(); pColRecord != NULL ; pColRecord = pColRecord->GetNext())
		{
			if(pColRecord->m_fCollisionImpulseMag < fIgnoreImpulseMag)
				continue;

			vLocalNormal = VEC3V_TO_VECTOR3(pPed->GetTransform().UnTransform3x3(RCC_VEC3V(pColRecord->m_MyCollisionNormal)));
			if(DotProduct(vMoveDirLocal, vLocalNormal) < -0.707f)
			{
				return true;
			}
		}
	}

	// Shall we do the same for building collisions?

	return false;
}

bool CTaskNavBase::TestObjectLookAheadLOS(CPed * pPed)
{
	if( ((CTaskMove*)GetSubTask())->HasTarget()==false )
		return true;

	const Vector3 vPedPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
	const float fPedRadius = 
		(m_iNavMeshRouteMethod==eRouteMethod_ReducedObjectBoundingBoxes) ?
		0.0f : PATHSERVER_PED_RADIUS - 0.1f;	// TODO: Should use capsule info here

	CEntityBoundAI entityBounds[CEntityScanner::MAX_NUM_ENTITIES*2];
	const CEntity * entities[CEntityScanner::MAX_NUM_ENTITIES*2];

	static dev_s32 iNumOfEachType = 2;
	s32 iNumEntities = GetNearbyEntitiesForLineOfSightTest( pPed, entityBounds, entities, iNumOfEachType, CEntityScanner::MAX_NUM_ENTITIES*2, fPedRadius, vPedPos.z );

	if( !iNumEntities )
	{
		return true;
	}

	const Vector3 vMoveTarget = ((CTaskMove*)GetSubTask())->GetTarget();

	for( s32 e=0; e<iNumEntities; e++)
	{
		if( vPedPos.z > entityBounds[e].GetTopZ() || vMoveTarget.z > entityBounds[e].GetTopZ() || vPedPos.z < entityBounds[e].GetBottomZ() || vMoveTarget.z < entityBounds[e].GetBottomZ() )
		{
			continue;
		}

		if ( entityBounds[e].TestLineOfSight( VECTOR3_TO_VEC3V(vPedPos), VECTOR3_TO_VEC3V(vMoveTarget) ) == false )
		{
			return false;
		}
	}

	return true;
}

// NAME : ScanForMissedClimb
// PURPOSE : It is possible to miss a climb waypoint, if the climb point on the navmesh is some distance away from the climb -
// in which case the climb may fail & the task continue.  Alternatively the initial progress on the route may be calcaulted to
// be some distance beyond a climb, in which case it will also be missed.
// As a remedy for this, if our previous progress was a climb - then we should perform tests to determine whether the ped is
// stuck at climbable geometry, and perform the climb belatedly if so.
bool CTaskNavBase::ScanForMissedClimb(CPed * pPed)
{
	Assert(GetState()==NavBaseState_FollowingPath);

	dev_s32 iStaticThreshold = 4;

	if(m_pRoute && m_iProgress > 0)
	{
		// Note: from StateFollowingPath_OnUpdate(), we now make the assumption that this function can't
		// return true unless IsSpecialActionClimbOrDrop() returns true for one of the waypoints. This includes
		// WAYPOINT_FLAG_CLIMB_HIGH and WAYPOINT_FLAG_CLIMB_LOW, but we may have to be a bit careful if making changes.

		const u32 iWpt = m_pRoute->GetWaypoint(m_iProgress-1).GetSpecialAction();
		if(iWpt == WAYPOINT_FLAG_CLIMB_HIGH || iWpt == WAYPOINT_FLAG_CLIMB_LOW)
		{
			if(pPed->GetPedIntelligence()->GetEventScanner()->GetStaticMovementScanner().GetStaticCounter() > iStaticThreshold)
			{
				const EWaypointActionResult eWptActionResult = CreateWaypointActionEvent(pPed, m_iProgress-1);
				if(eWptActionResult == WaypointAction_EventCreated)
				{
					SetState(NavBaseState_WaitingForWaypointActionEvent);
					return true;
				}
				else
				{
					return false;
				}
			}
		}
	}
	return false;
}

bool CTaskNavBase::GetIsTaskSequencedBefore(const CPed * pPed, bool & bIsGotoTask)
{
	return CTaskNavBase::GetIsTaskSequencedAdjacent(pPed, this, bIsGotoTask, -1);
}
bool CTaskNavBase::GetIsTaskSequencedAfter(const CPed * pPed, bool & bIsGotoTask)
{
	return CTaskNavBase::GetIsTaskSequencedAdjacent(pPed, this, bIsGotoTask, 1);
}
bool CTaskNavBase::GetIsTaskSequencedAdjacent(const CPed * pPed, const CTaskMove * UNUSED_PARAM(pThisTask), bool & bIsGotoTask, const int iDirection)
{
	Assert(iDirection==-1 || iDirection==1);

	bIsGotoTask = false;

	CPedIntelligence * pPedAi = pPed->GetPedIntelligence();
	CTask * pActiveSeqTask = pPedAi->FindTaskActiveByType(CTaskTypes::TASK_USE_SEQUENCE);
	if(!pActiveSeqTask)
		return false;

	CTaskUseSequence * pTaskUseSequence = (CTaskUseSequence*)pActiveSeqTask;

	const CTaskSequenceList * pTaskSequence = pTaskUseSequence->GetStaticTaskSequenceList();

	if (!pTaskSequence)
		return false;

	// Check that the task at the current position in the sequence is a navmesh route task
	const int iProgress = pTaskUseSequence->GetPrimarySequenceProgress();
	const aiTask * pSeqTask = pTaskSequence->GetTask(iProgress);
	if(pSeqTask->GetTaskType() != CTaskTypes::TASK_COMPLEX_CONTROL_MOVEMENT)
		return false;

	if(iProgress + iDirection >= 0 && iProgress + iDirection < CTaskList::MAX_LIST_SIZE)
	{
		const aiTask * pTask = pTaskSequence->GetTask(iProgress + iDirection);

		// There is a task sequenced after this task
		if(pTask)
		{
			if(pTask->GetTaskType()==CTaskTypes::TASK_COMPLEX_CONTROL_MOVEMENT)
			{
				CTaskTypes::eTaskType iNextTaskType = (CTaskTypes::eTaskType) ((CTaskComplexControlMovement*)pTask)->GetMoveTaskType();

				if(iNextTaskType == CTaskTypes::TASK_MOVE_GO_TO_POINT ||
					iNextTaskType == CTaskTypes::TASK_MOVE_GO_TO_POINT_ON_ROUTE ||
					iNextTaskType == CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH ||
					iNextTaskType == CTaskTypes::TASK_MOVE_FOLLOW_POINT_ROUTE)
				{
					bIsGotoTask = true;
				}
			}
			// Hack
			if(pTask->GetTaskType()==CTaskTypes::TASK_COMPLEX_USE_MOBILE_PHONE)
			{
				bIsGotoTask = true;
			}
			return true;
		}
	}
	return false;
}
bool CTaskNavBase::TransformRouteOnDynamicNavmesh(const CPed * pPed)
{
	Assert(m_pOnEntityRouteData && m_pOnEntityRouteData->m_pDynamicEntityRouteIsOn);
	Assert(m_pOnEntityRouteData && m_pOnEntityRouteData->m_pLocalSpaceRoute);
	Assert(m_pRoute);

	if(!m_pRoute || !m_pOnEntityRouteData || !m_pOnEntityRouteData->m_pLocalSpaceRoute || !m_pOnEntityRouteData->m_pDynamicEntityRouteIsOn)
		return false;

	AI_OPTIMISATIONS_OFF_ONLY( Assert(m_pRoute->GetSize() == m_pOnEntityRouteData->m_pLocalSpaceRoute->GetSize()) );

	if(m_pRoute->GetSize() != m_pOnEntityRouteData->m_pLocalSpaceRoute->GetSize())
		return false;

	// Transform all the route elements in m_pPointRoute
	const Matrix34 mat = MAT34V_TO_MATRIX34(m_pOnEntityRouteData->m_pDynamicEntityRouteIsOn->GetMatrix());

	int p;
	for(p=0; p<m_pOnEntityRouteData->m_pLocalSpaceRoute->GetSize(); p++)
	{
		Vector3 vTransformed;
		mat.Transform(m_pOnEntityRouteData->m_pLocalSpaceRoute->GetPosition(p), vTransformed);
		m_pRoute->Set(p, vTransformed, m_pOnEntityRouteData->m_pLocalSpaceRoute->GetWaypoint(p) );
	}

	if(GetState() != NavBaseState_FollowingPath)
		return false;

	Assert(m_iProgress >= 0 && m_iProgress < m_pRoute->GetSize());

	const Vector3 & vGoToTarget = m_pRoute->GetPosition(m_iProgress);

	if(GetSubTask())
	{
		Assert(GetSubTask()->IsMoveTask());

		CTaskMove * pMoveTask = (CTaskMove*)GetSubTask();
		pMoveTask->SetTarget(pPed, VECTOR3_TO_VEC3V(vGoToTarget));
	}
	return true;
}

// NAME : AdjustMoveBlendRatioForRouteLength
// PURPOSE : Determine whether the MBR needs reducing for small route lengths
// This is to avoid peds performing long run start/stop anims for small routes (and thus overshooting)
float CTaskNavBase::AdjustMoveBlendRatioForRouteLength(const float fMBR, const float fRouteLength) const
{
	float fMaxMbr;
	if(fRouteLength < 3.0f)
		fMaxMbr = 1.0f;
	else if(fRouteLength < 6.0f)
		fMaxMbr = 2.0;
	else fMaxMbr = 3.0f;

	return Min(fMBR, fMaxMbr);
}

float CTaskNavBase::GetDistanceLeftOnCurrentRouteSection(const CPed * pPed) const
{
	if(!m_pRoute || !GetSubTask() || m_iProgress >= m_pRoute->GetSize())
		return 0.0f;

	Assert(m_iProgress >= 0 && m_iProgress < m_pRoute->GetSize());

	int iProgressToUse;

	if(GetSubTask()->GetTaskType()==CTaskTypes::TASK_MOVE_GO_TO_POINT_ON_ROUTE &&
		m_NavBaseInternalFlags.IsSet(CNavBaseInternalFlags::NB_HasSlidTargetAlongSpline) &&
		m_fSplineTVal > 0.5f)
	{
		Assert(m_iProgress+1 < m_pRoute->GetSize());
		iProgressToUse = m_iProgress+1;
	}
	else
	{
		iProgressToUse = m_iProgress;
	}

	float fDist = 0.0f;
	const Vector3 & vCurrTarget = m_pRoute->GetPosition(iProgressToUse);
	const Vector3 vToTarget = vCurrTarget - VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
	const float fMagToTarget2 = vToTarget.XYMag2();
	if(fMagToTarget2 > 0.001f)
		fDist += Sqrtf(fMagToTarget2);

	for(int p=iProgressToUse+1; p<m_pRoute->GetSize(); p++)
	{
		const Vector3 vToNext = m_pRoute->GetPosition(p) - m_pRoute->GetPosition(p-1);
		const float fMagToNext2 = vToNext.XYMag2();
		if(fMagToNext2 > 0.001f)
			fDist += Sqrtf(fMagToNext2);
	}
	return fDist;
}

float CTaskNavBase::GetTotalRouteLength() const
{
	Assert(m_pRoute);
	return m_pRoute->GetLength();
}

float CTaskNavBase::GetTotalRouteLengthSq() const
{
	Assert(m_pRoute);
	return m_pRoute->GetLengthSqr();
}

bool CTaskNavBase::IsTotalRouteLongerThan(float fLength) const
{
	Assert(m_pRoute);
	return m_pRoute->IsLongerThan(fLength);
}

TNavMeshWaypointFlag CTaskNavBase::GetWaypointFlag(const int i) const
{
	Assert(m_pRoute && m_pRoute->GetSize());
	Assert(i >= 0 && i < m_pRoute->GetSize());
	return m_pRoute->GetWaypoint(i);
}

/*
PURPOSE
	Helper function which is essentially the same as geomTValues::FindTValueSegToPoint(),
	but it operates on up to four segments and four target points
	in parallel, passed in as struct of array-like vectors. point1XV
	contains the X coordinates of the point1 variable for the four segments, etc.
NOTES
	This shouldn't really be more expensive than the regular FindTValueSegToPoint(),
	and the math should be pretty much the same as the vectorized single segment version
	of that function so hopefully there won't be any significant differences in the output.
*/
static Vec4V_Out sFindTValueSegToPoint(
		Vec4V_In point1XV, Vec4V_In point1YV, Vec4V_In point1ZV,
		Vec4V_In deltaXV, Vec4V_In deltaYV, Vec4V_In deltaZV,
		Vec4V_In targetPointXV, Vec4V_In targetPointYV, Vec4V_In targetPointZV)
{
	const Vec4V ptXV = Subtract(targetPointXV, point1XV);
	const Vec4V ptYV = Subtract(targetPointYV, point1YV);
	const Vec4V ptZV = Subtract(targetPointZV, point1ZV);

	const Vec4V oneDotXV = Scale(deltaXV, ptXV);
	const Vec4V oneDotXYV = AddScaled(oneDotXV, deltaYV, ptYV);
	const Vec4V oneDotV = AddScaled(oneDotXYV, deltaZV, ptZV);

	const Vec4V bothDotXV = Scale(deltaXV, deltaXV);
	const Vec4V bothDotXYV = AddScaled(bothDotXV, deltaYV, deltaYV);
	const Vec4V bothDotV = AddScaled(bothDotXYV, deltaZV, deltaZV);

	const Vec4V zeroV(V_ZERO);
	const Vec4V oneV(V_ONE);

	// Note: InvScaleFast() here is an approximation, but it should be the same as
	// the  __vmulfp(oneDotV,__vrefp(bothDotV)) intrinsics in the single segment version,
	// i.e. good enough.
	const Vec4V tOnInfLineV = InvScaleFast(oneDotV, bothDotV);
	const VecBoolV tMaxMaskV = IsGreaterThanOrEqual(tOnInfLineV, oneV);
	const Vec4V tClampedMaxV = SelectFT(tMaxMaskV, tOnInfLineV, oneV);
	const VecBoolV tMinMaskV = IsGreaterThan(oneDotV, zeroV);	// Note: this was probably IsGreaterThanOrEqual() in the original function.
	const Vec4V tClampedV = And(tClampedMaxV, Vec4V(tMinMaskV));

	return tClampedV;
}


int CTaskNavBase::CalculateClosestSegmentOnExpandedRoute(CNavMeshExpandedRouteVert * pExpandedRouteVerts, const int iNumVerts, const Vector3 & vTargetPos, float & fOut_SplineTVal, Vector3 * v_OutClosestPosOnRoute, Vector3 * v_OutClosestSegment)
{
	if(iNumVerts < 2)
	{
		fOut_SplineTVal = 0.0f;
		if(iNumVerts > 0 && v_OutClosestPosOnRoute)
			*v_OutClosestPosOnRoute = VEC3V_TO_VECTOR3(pExpandedRouteVerts[0].GetPos());

		return 0;
	}

	const int iCount = iNumVerts-1;

	const Vec4V zeroV(V_ZERO);

	// Initialize vectors for keeping track of the closest point found.
	Vec4V closestDistSqV(V_FLT_MAX);
	Vec4V closestTValV = zeroV;
	Vec4V closestSegIntV(V_80000000);
	Vec4V closestSegFloatV(V_NEGONE);

	// Get the target position.
	const Vec3V targetPosV = RCC_VEC3V(vTargetPos);

	// We will do up to four segment tests in one operation. Each of the variables below
	// store the X, Y, or Z coordinate of all four start or end points of these segments.
	Vec4V points1XV = zeroV;
	Vec4V points1YV = zeroV;
	Vec4V points1ZV = zeroV;
	Vec4V points2XV = zeroV;
	Vec4V points2YV = zeroV;
	Vec4V points2ZV = zeroV;

	// We keep track of the current segment index both as integers and floats in vector
	// registers, and these variables are related to that:
	Vec4V segsIntV(V_80000000);
	Vec4V segsFloatV(V_NEGONE);
	Vec4V segIntV = zeroV;
	Vec4V segFloatV = zeroV;
	const Vec4V segAddIntV(V_INT_1);
	const Vec4V segAddFloatV(V_ONE);

	// Load the first point of the first segment to prepare for the loop.
	Vec3V currV = pExpandedRouteVerts[0].GetPos();

	// We will load up four segments into vector registers, and numLoadedSegs keeps track
	// of how many we currently have loaded.
	int numLoadedSegs = 0;

	for(int j = 1; j < iNumVerts; j++)
	{
		// Load the XYZ coordinates of the second point of this segment.
		const Vec3V nextV = pExpandedRouteVerts[j].GetPos();

		// Next, we take all of our vectors for four segment tests, and shift them one step,
		// shifting in this new segment into the W component.
		points1XV = GetFromTwo<Vec::Y1, Vec::Z1, Vec::W1, Vec::X2>(points1XV, Vec4V(currV));
		points1YV = GetFromTwo<Vec::Y1, Vec::Z1, Vec::W1, Vec::Y2>(points1YV, Vec4V(currV));
		points1ZV = GetFromTwo<Vec::Y1, Vec::Z1, Vec::W1, Vec::Z2>(points1ZV, Vec4V(currV));
		points2XV = GetFromTwo<Vec::Y1, Vec::Z1, Vec::W1, Vec::X2>(points2XV, Vec4V(nextV));
		points2YV = GetFromTwo<Vec::Y1, Vec::Z1, Vec::W1, Vec::Y2>(points2YV, Vec4V(nextV));
		points2ZV = GetFromTwo<Vec::Y1, Vec::Z1, Vec::W1, Vec::Z2>(points2ZV, Vec4V(nextV));
		segsIntV = GetFromTwo<Vec::Y1, Vec::Z1, Vec::W1, Vec::W2>(segsIntV, segIntV);
		segsFloatV = GetFromTwo<Vec::Y1, Vec::Z1, Vec::W1, Vec::W2>(segsFloatV, segFloatV);

		// Prepare for the next iteration.
		currV = nextV;
		segIntV = AddInt(segIntV, segAddIntV);
		segFloatV = Add(segFloatV, segAddFloatV);

		// If we don't have four segments yet, and this isn't the very last iteration through
		// the loop, we can continue with the next segment.
		if(++numLoadedSegs < 4 && j != iCount)
		{
			continue;
		}

		// Compute the delta vectors for the (up to) four segments we are about to test.
		const Vec4V deltaXV = Subtract(points2XV, points1XV);
		const Vec4V deltaYV = Subtract(points2YV, points1YV);
		const Vec4V deltaZV = Subtract(points2ZV, points1ZV);

		// Find the T values of the closest points to targetPointV, on these four segments.
		const Vec4V tV = sFindTValueSegToPoint(points1XV, points1YV, points1ZV, deltaXV, deltaYV, deltaZV, Vec4V(targetPosV.GetX()), Vec4V(targetPosV.GetY()), Vec4V(targetPosV.GetZ()));

		// Compute the position of the closets point on each segment.
		const Vec4V posOnSegXV = AddScaled(points1XV, deltaXV, tV);
		const Vec4V posOnSegYV = AddScaled(points1YV, deltaYV, tV);
		const Vec4V posOnSegZV = AddScaled(points1ZV, deltaZV, tV);

		// Compute the vector from the target point to the closest point.
		const Vec4V deltaToClosestPosXV = Subtract(posOnSegXV, Vec4V(targetPosV.GetX()));
		const Vec4V deltaToClosestPosYV = Subtract(posOnSegYV, Vec4V(targetPosV.GetY()));
		const Vec4V deltaToClosestPosZV = Subtract(posOnSegZV, Vec4V(targetPosV.GetZ()));

		// Compute the squared distance to each of these four closest points.
		const Vec4V distToClosestPosSqXV = Scale(deltaToClosestPosXV, deltaToClosestPosXV);
		const Vec4V distToClosestPosSqXYV = AddScaled(distToClosestPosSqXV, deltaToClosestPosYV, deltaToClosestPosYV);
		const Vec4V distToClosestPosSqV = AddScaled(distToClosestPosSqXYV, deltaToClosestPosZV, deltaToClosestPosZV);

		// At the end of the array, we may not actually have had four segments loaded up.
		// Here, we create a mask representing the elements of the vectors that are currently valid.
		const VecBoolV validMaskV = IsGreaterThanOrEqual(segsFloatV, zeroV);

		// Create a mask that's true if the squared distance is closer than the closest previously find
		// square distance (for each of the four vector components). We and with validMaskV we computed
		// above, so that only the valid segments can cause a new minimum to be found.
		const VecBoolV lessThanClosestDistMaskV = And(IsLessThan(distToClosestPosSqV, closestDistSqV), validMaskV);

		// Use vector select operations to either keep the old value, or switch to use the new
		// values if we found a new best segment.
		closestDistSqV =	SelectFT(lessThanClosestDistMaskV,	closestDistSqV, distToClosestPosSqV);
		closestTValV =		SelectFT(lessThanClosestDistMaskV,	closestTValV,	tV);
		closestSegIntV =	SelectFT(lessThanClosestDistMaskV,	closestSegIntV,	segsIntV);
		closestSegFloatV =	SelectFT(lessThanClosestDistMaskV,	closestSegFloatV, segsFloatV);

		// Clear out the segment information in the buffer, so we can keep track of which ones
		// are valid in future iterations. We could clear out the rest of the buffer too (points1XV, etc),
		// but that really shouldn't be necessary since we won't use the results of those anyway.
		segsIntV = Vec4V(V_80000000);
		segsFloatV = Vec4V(V_NEGONE);
	}

	// At this point, we have a vector containing the closest squared distance for every four
	// segments. That is, the X component contains the closest of segments 0, 4, 8, etc, Y is
	// the closest of segments 1, 5, 9, etc. We can use MinElement() to find the smallest across
	// all the components.
	const ScalarV closestDistOfAllSqV = MinElement(closestDistSqV);

	// Next, we can compare this value with each of the components, to create a mask which
	// tells us which of the values was the smallest.
	const VecBoolV maskClosestOfAllV = IsEqual(closestDistSqV, Vec4V(closestDistOfAllSqV));

	// Now, potentially there could be more than one that had this smallest value. This would be
	// a problem since we need a single T value, etc. We have the indices of each one stored
	// in a separate vector, so we can use that to arbitrate. First, we use the mask we computed
	// above to come up with a vector where only the winner(s) have valid values, the rest get set to FLT_MAX.
	const Vec4V closestSegFloatMaskedV = SelectFT(maskClosestOfAllV, Vec4V(V_FLT_MAX), closestSegFloatV);

	// Now, we can find the smallest of these elements, and create a new mask in which only one
	// element can be true, corresponding to the final winner.
	// Note: this would have been less awkward if we had a Min operation on integer vectors, since
	// then we could get rid of the float segment vector. I think that was removed from the 360/PS3
	// instruction sets to make space for other things. I tried comparing integers as if they were floats,
	// but didn't have much success with that, even if they were all positive, probably because some integers
	// correspond to denormalized or otherwise invalid vectors.
	const ScalarV closestSmallestIndexFloatV = MinElement(closestSegFloatMaskedV);
	const VecBoolV maskSmallestIndexV = IsEqual(closestSegFloatMaskedV, Vec4V(closestSmallestIndexFloatV));

	// Now, we use the mask we computed to And the integer index vector.
	const Vec4V maskedClosestSmallestIndexV = And(Vec4V(maskSmallestIndexV), closestSegIntV);

	// Again, it would have been nice to use a Max() operation here, but since we don't have that for
	// vectors, we can get the right value by adding up the components - all but the one that we want
	// should be zero due to the And operation above.
	const ScalarV closestSmallestIndexV = AddInt(AddInt(maskedClosestSmallestIndexV.GetX(), maskedClosestSmallestIndexV.GetY()),
			AddInt(maskedClosestSmallestIndexV.GetZ(), maskedClosestSmallestIndexV.GetW()));

	// Finally, we can extract the T value of the closest segment.
	const ScalarV finalClosestTValV = MaxElement(And(closestTValV, Vec4V(maskSmallestIndexV)));

	// At this point, we need the index of the closest segment in an integer register.
	const int iClosestSeg = closestSmallestIndexV.Geti();
	Assert(iClosestSeg >= 0 && iClosestSeg < iCount);

	// Get the two end points of the segment containing the closest point.
	const Vec3V closestSegPt1V = pExpandedRouteVerts[iClosestSeg].GetPos();
	const Vec3V closestSegPt2V = pExpandedRouteVerts[iClosestSeg + 1].GetPos();

	// Compute the closest point by using the T value to lerp on this segment.
	const Vec3V closestPtV = Lerp(finalClosestTValV, closestSegPt1V, closestSegPt2V);

	// Load the m_fSplineT values into vector registers.
	const ScalarV closestSegSplineT1V = pExpandedRouteVerts[iClosestSeg].GetSplineTV();
	const ScalarV closestSegSplineT2V = pExpandedRouteVerts[iClosestSeg + 1].GetSplineTV();

	// Now, there used to be logic like this:
	//	if( (pExpandedRouteVerts[iClosestSeg].m_fSplineT == 0.0f && pExpandedRouteVerts[iClosestSeg+1].m_fSplineT == 1.0f) ||
	//		(pExpandedRouteVerts[iClosestSeg].m_fSplineT == 1.0f && pExpandedRouteVerts[iClosestSeg+1].m_fSplineT == 0.0f) )
	//	{
	//		fOut_SplineTVal = 0.0f;
	//	}
	//	else
	//	{
	//		fOut_SplineTVal = Lerp(fClosestTVal, pExpandedRouteVerts[iClosestSeg].m_fSplineT, pExpandedRouteVerts[iClosestSeg+1].m_fSplineT);
	//	}
	// To save on float comparisons, etc, this has been rewritten in a vectorized form. We can do the four equality checks
	// by building two vectors, basically (t1, t2, t1, t2) and (0, 1, 1, 0).
	const Vec4V closestSegSplineTV(closestSegSplineT1V, closestSegSplineT2V, closestSegSplineT1V, closestSegSplineT2V);
	const Vec4V closestSegSplineTCmpV = And(Vec4V(V_MASKYZ), Vec4V(V_ONE));
	
	// Do the comparison.
	const VecBoolV closestSegSplineTMaskV = IsEqual(closestSegSplineTV, closestSegSplineTCmpV);

	// Compute a single boolean value for the expression ((t1 == 0) && (t2 == 1)) || ((t1 == 1) && (t2 == 0)).
	const BoolV closestSegSplineTShouldBeZeroV = Or(And(closestSegSplineTMaskV.GetX(), closestSegSplineTMaskV.GetY()), And(closestSegSplineTMaskV.GetZ(), closestSegSplineTMaskV.GetW()));

	// Compute the lerped spline T value, if we are going to use it.
	const ScalarV lerpedSplineTValV = Lerp(finalClosestTValV, closestSegSplineT1V, closestSegSplineT2V);

	// Finally, compute the spline T value, masked to zero if the boolean expression above evaluated to true.
	ScalarV closestSegSplineTShouldNotBeZeroV;
	closestSegSplineTShouldNotBeZeroV.SetIntrin128(InvertBits(closestSegSplineTShouldBeZeroV).GetIntrin128());
	const ScalarV splineTValV = And(closestSegSplineTShouldNotBeZeroV, lerpedSplineTValV);

	// Closest point and spline T value, and return the segment index.
	StoreScalar32FromScalarV(fOut_SplineTVal, splineTValV);
	if(v_OutClosestPosOnRoute)
		*v_OutClosestPosOnRoute = VEC3V_TO_VECTOR3(closestPtV);
	if(v_OutClosestSegment)
		*v_OutClosestSegment = VEC3V_TO_VECTOR3(closestSegPt2V - closestSegPt1V);

	Assert(iClosestSeg >= 0);
	return iClosestSeg;
}

#if 0	// Unoptimized version, for reference.

int CTaskNavBase::CalculateClosestSegmentOnExpandedRoute(CNavMeshExpandedRouteVert * pExpandedRouteVerts, const int iNumVerts, const Vector3 & vTargetPos, float & fOut_SplineTVal)
{
	if(iNumVerts < 2)
	{
		fOut_SplineTVal = 0.0f;
		return 0;
	}

	const int iCount = iNumVerts-1;

	float fClosestDistSqr = FLT_MAX;
	float fClosestTVal = 0.0f;
	s32 iClosestSeg = -1;

	for(s32 i=0; i<iCount; i++)
	{
		const Vector3 & vCurr = pExpandedRouteVerts[i].m_vPos;
		const Vector3 & vNext = pExpandedRouteVerts[i+1].m_vPos;
		const Vector3 vVec = vNext-vCurr;

		const float t = (vVec.Mag2() > 0.0f) ? geomTValues::FindTValueSegToPoint(vCurr, vVec, vTargetPos) : 0.0f;
		const Vector3 vPosOnSeg = (t==0.0f) ? vCurr : (t==1.0f) ? vNext : vCurr + (vVec * t);
		const float fDistSqr = (vPosOnSeg-vTargetPos).Mag2();

		if(fDistSqr < fClosestDistSqr)
		{
			fClosestDistSqr = fDistSqr;
			fClosestTVal = t;
			iClosestSeg = i;
		}
	}

	m_vClosestPosOnRoute =
		pExpandedRouteVerts[iClosestSeg].m_vPos +
			((pExpandedRouteVerts[iClosestSeg+1].m_vPos - pExpandedRouteVerts[iClosestSeg].m_vPos) * fClosestTVal);

	if( (pExpandedRouteVerts[iClosestSeg].m_fSplineT == 0.0f && pExpandedRouteVerts[iClosestSeg+1].m_fSplineT == 1.0f) ||
		(pExpandedRouteVerts[iClosestSeg].m_fSplineT == 1.0f && pExpandedRouteVerts[iClosestSeg+1].m_fSplineT == 0.0f) )
	{
		fOut_SplineTVal = 0.0f;
	}
	else
	{
		fOut_SplineTVal = Lerp(fClosestTVal, pExpandedRouteVerts[iClosestSeg].m_fSplineT, pExpandedRouteVerts[iClosestSeg+1].m_fSplineT);
	}

	return iClosestSeg;
}

#endif	// 0 - Unoptimized version, for reference.


bool CTaskNavBase::GetThisWaypointLine(const CPed * UNUSED_PARAM(pPed), Vector3 & vOutStartPos, Vector3 & vOutEndPos, s32 iProgressAhead )
{
	if(GetState()==NavBaseState_FollowingPath && m_pRoute && !m_NavBaseInternalFlags.IsSet(CNavBaseInternalFlags::NB_JustDoneClimbResponse))
	{
		const s32 iProgress = m_iProgress + iProgressAhead;

		if(iProgress > 0 && iProgress < m_pRoute->GetSize())
		{
			vOutStartPos = m_pRoute->GetPosition(iProgress-1);
			vOutEndPos = m_pRoute->GetPosition(iProgress);
			return true;
		}
		else if(iProgress+1 < m_pRoute->GetSize())
		{
			vOutStartPos = m_pRoute->GetPosition(iProgress);
			vOutEndPos = m_pRoute->GetPosition(iProgress+1);
			return true;
		}
	}
	return false;
}

bool CTaskNavBase::GetPreviousWaypoint(Vector3 & vPos) const
{
	if(GetState()==NavBaseState_FollowingPath && m_pRoute && !m_NavBaseInternalFlags.IsSet(CNavBaseInternalFlags::NB_JustDoneClimbResponse))
	{
		if(m_iProgress > 0 && m_iProgress < m_pRoute->GetSize())
		{
			vPos = m_pRoute->GetPosition(m_iProgress - 1);
			return true;
		}
	}
	return false;
}

bool CTaskNavBase::GetCurrentWaypoint(Vector3 & vPos) const
{
	if(GetState()==NavBaseState_FollowingPath && m_pRoute && !m_NavBaseInternalFlags.IsSet(CNavBaseInternalFlags::NB_JustDoneClimbResponse))
	{
		if(m_iProgress >= 0 && m_iProgress < m_pRoute->GetSize())
		{
			vPos = m_pRoute->GetPosition(m_iProgress);
			return true;
		}
	}
	return false;
}

// NAME : GetPathIntersectsRegion
// PUPOSE : If we have a route, returns whether it intersects the AABB
// Disregards spline corners - should this be modified to test the expanded route?
bool CTaskNavBase::GetPathIntersectsRegion(const Vector3 & vMin, const Vector3 & vMax)
{
	if(m_pRoute)
	{
		int i;
		for(i=1; i<m_pRoute->GetSize(); i++)
		{
			const Vector3 & v1 = m_pRoute->GetPosition(i-1);
			const Vector3 & v2 = m_pRoute->GetPosition(i);

			// Either endpoint inside ?
			if( v1.IsGreaterThan(vMin) && vMax.IsGreaterThan(v1) )
				return true;
			if( v2.IsGreaterThan(vMin) && vMax.IsGreaterThan(v2) )
				return true;

			// Segment intersects ?
			int iHits = geomBoxes::TestSegmentToBoxMinMax(v1, v2, vMin, vMax);
			if(iHits > 0)
				return true;
		}
	}
	return false;
}

// NAME : TestSpheresIntersectingRoute
// PURPOSE : Test an array of up to 32 spheres for intersection with the route

u32 CTaskNavBase::TestSpheresIntersectingRoute( s32 iNumSpheres, const spdSphere * ppSpheres, const bool bOnlyAheadOfCurrentPosition ) const
{
	Assertf(iNumSpheres < 32, "Maximum num spheres is 32");
	iNumSpheres = Min(iNumSpheres, 32);

	s32 i,s;

	ScalarV radiiSqr[32];
	for(s=0; s<iNumSpheres; s++)
		radiiSqr[s] = ppSpheres[s].GetRadiusSquared();

	u32 iResults = 0;
	s32 iNumIntersections = 0;

	if(m_pRoute && m_pRoute->GetSize())
	{
		ms_iNumExpandedRouteVerts = m_pRoute->ExpandRoute(ms_vExpandedRouteVerts, NULL, 0, m_pRoute->GetSize());		
		if(m_iClosestSegOnExpandedRoute >= ms_iNumExpandedRouteVerts - 1)
		{
			return iResults;
		}

		const s32 iStartSeg = bOnlyAheadOfCurrentPosition ? m_iClosestSegOnExpandedRoute : 0;
		ScalarV fPedT;

		if(bOnlyAheadOfCurrentPosition)
		{
			Vec3V vVec = ms_vExpandedRouteVerts[m_iClosestSegOnExpandedRoute+1].GetPos() - ms_vExpandedRouteVerts[m_iClosestSegOnExpandedRoute].GetPos();
			fPedT = geomTValues::FindTValueSegToPoint( ms_vExpandedRouteVerts[m_iClosestSegOnExpandedRoute].GetPos(), vVec, GetPed()->GetTransform().GetPosition() );
		}

		const s32 iMaxVerts=ms_iNumExpandedRouteVerts-1;
		for(i=iStartSeg; i<iMaxVerts && iNumIntersections<iNumSpheres; i++)
		{
			if( !IsSpecialActionClimbOrDrop(ms_vExpandedRouteVerts[i].m_iWptFlag.GetSpecialActionFlags()) )
			{
				Vec3V vVec = ms_vExpandedRouteVerts[i+1].GetPos() - ms_vExpandedRouteVerts[i].GetPos();
				for(s=0; s<iNumSpheres; s++)
				{
					if( (iResults & (1<<s))==0 )	// Only if we've not already established intersection for this sphere
					{
						ScalarV T = geomTValues::FindTValueSegToPoint( ms_vExpandedRouteVerts[i].GetPos(), vVec, ppSpheres[s].GetCenter() );
						Vec3V vPos = ms_vExpandedRouteVerts[i].GetPos() + Scale(vVec, T);
						Vec3V vDiff = vPos - ppSpheres[s].GetCenter();
						if( (MagSquared( vDiff ) < radiiSqr[s]).Getb() )
						{
							// If only checking ahead on the route, and this is the ped's segment, then compare T vals
							if( bOnlyAheadOfCurrentPosition && (i==iStartSeg) && (T < fPedT).Getb() )
								continue;

							iResults |= (1<<s);
							iNumIntersections++;
						}
					}
				}
			}
		}
	}

	return iResults;
}

bool CTaskNavBase::GetPositionAlongRouteWithDistanceFromTarget(Vec3V_InOut vReturnPosition, Vec3V_In vTargetPosition, float &fDesiredDistanceFromTarget)
{
	if(m_pRoute && m_pRoute->GetSize())
	{
		ms_iNumExpandedRouteVerts = m_pRoute->ExpandRoute(ms_vExpandedRouteVerts, NULL, 0, m_pRoute->GetSize());	

		ScalarV scDesiredDistSq = ScalarVFromF32(square(fDesiredDistanceFromTarget));

		for(int i = 0; i < ms_iNumExpandedRouteVerts; i++)
		{
			// Check if this points position is far enough away from the target position
			Vec3V vPoint2 = ms_vExpandedRouteVerts[i].GetPos();
			ScalarV scDistToTargetSq = DistSquared(vPoint2, vTargetPosition);
			if(IsGreaterThanAll(scDistToTargetSq, scDesiredDistSq))
			{
				// If this is the first point then we don't have a line to lerp on, so just return this point's position
				if(i == 0)
				{
					vReturnPosition = ms_vExpandedRouteVerts[i].GetPos();
				}
				else
				{
					// Otherwise, we need to find the point along the segment that is the desired distance away from the target
					Vec3V vPoint1 = ms_vExpandedRouteVerts[i - 1].GetPos();
					Vec3V vToTarget = vTargetPosition - vPoint1;
					Vec3V vSegmentDirection = vPoint2 - vPoint1;
					ScalarV vT = Dot(vToTarget, vSegmentDirection) / Dot(vSegmentDirection, vSegmentDirection);
					Vec3V vClosestPointOnSegment = vPoint1 + (vT * vSegmentDirection);
					Vec3V vTargetToClosestPoint = vClosestPointOnSegment - vTargetPosition;
					ScalarV vClosestToDesiredLength = SqrtFast(Dot(vTargetToClosestPoint, vTargetToClosestPoint) + scDesiredDistSq);
					ScalarV vLengthT = vClosestToDesiredLength / Mag(vSegmentDirection);
					vReturnPosition = vPoint1 + (vSegmentDirection * (vT + vLengthT));
				}

				return true;
			}
		}
	}

	return false;
}

bool CTaskNavBase::GetPositionAheadOnRoute(const CPed * pPed, const float fDistanceAhead, Vector3 & vOutPos, const bool bHaltAtClimbsOrDrops)
{
	if(m_pRoute && m_pRoute->GetSize())
	{
		const Vector3 vPedPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());

		ms_iNumExpandedRouteVerts = m_pRoute->ExpandRoute(ms_vExpandedRouteVerts, NULL, Max(0, m_iProgress - 1), m_pRoute->GetSize(), fDistanceAhead);

		// Since we've expanded the route again we need to find the closest segment
		if(ms_iNumExpandedRouteVerts)
			m_iClosestSegOnExpandedRoute = (s16) CalculateClosestSegmentOnExpandedRoute(ms_vExpandedRouteVerts, ms_iNumExpandedRouteVerts, vPedPos, m_fSplineTVal, NULL, NULL);
		else
			m_iClosestSegOnExpandedRoute = 0;

		// Advance to minimum 'm_iClampMinProgress'
		while(ms_vExpandedRouteVerts[m_iClosestSegOnExpandedRoute].m_iProgress < m_iClampMinProgress && m_iClosestSegOnExpandedRoute < ms_iNumExpandedRouteVerts-1)
			m_iClosestSegOnExpandedRoute++;

		if (m_iClosestSegOnExpandedRoute >= ms_iNumExpandedRouteVerts-1)
			return false;

		ScalarV distanceRemainingV;
		GetPositionAheadOnRoute(ms_vExpandedRouteVerts, ms_iNumExpandedRouteVerts, VECTOR3_TO_VEC3V(vPedPos), ScalarV(fDistanceAhead), RC_VEC3V(vOutPos), distanceRemainingV, NULL, bHaltAtClimbsOrDrops, bHaltAtClimbsOrDrops);

		return true;
	}

	return false;
}


u32 CTaskNavBase::GetPositionAheadOnRoute(const CNavMeshExpandedRouteVert * pPoints, const int iNumPts, Vec3V_In inputPos, ScalarV_In distanceAhead,
		Vec3V_Ref outPos, ScalarV_Ref outDistanceRemainingV, ScalarV* outLengthOfFinalSegment, const bool bHaltAtClimbs, const bool bHaltAtDrops, ScalarV* pOutDistToClimb, ScalarV* pOutDistToDrop) const
{
	Assert(iNumPts);
	Assertf(m_iClosestSegOnExpandedRoute < iNumPts, "m_iClosestSegOnExpandedRoute = %i, iNumPts = %i", m_iClosestSegOnExpandedRoute, iNumPts);
	Assert(rage::FPIsFinite(inputPos.GetXf()) && rage::FPIsFinite(inputPos.GetYf()) && rage::FPIsFinite(inputPos.GetZf()));

	const ScalarV zeroV(V_ZERO);

	if(IsEqualAll(distanceAhead, zeroV))
	{
		outDistanceRemainingV = zeroV;
		outPos = inputPos;

		Assert(rage::FPIsFinite(outPos.GetXf()) && rage::FPIsFinite(outPos.GetYf()) && rage::FPIsFinite(outPos.GetZf()));

		return 0;
	}

	const s32 iLastIndex = iNumPts-1;

	s32 iSeg = m_iClosestSegOnExpandedRoute;

	// Find the position of the input position along the route
	Vec3V currV = pPoints[iSeg].GetPos();
	Vec3V nextV = pPoints[iSeg+1].GetPos();
	Vec3V vecV = Subtract(nextV, currV);
	const ScalarV tV = geomTValues::FindTValueSegToPoint(currV, vecV, inputPos);
	
	Assert(rage::FPIsFinite(tV.Getf()));

	currV = AddScaled(currV, vecV, tV);
	vecV = Subtract(nextV, currV);
	bool bFoundClimbWpt = false;
	bool bFoundDropWpt = false;

	ScalarV distLeftV = distanceAhead;
	ScalarV distToCheckForNarrow = LoadScalar32IntoScalarV(GetGotoTargetLookAheadDistance() + GetLookAheadExtra());

	ScalarV distanceRemainingV = zeroV;

	u32 iRouteFlags = 0;

	while(IsGreaterThanAll(distLeftV, zeroV))
	{
		const ScalarV segLenV = Mag(vecV);
		Assert(rage::FPIsFinite(segLenV.Getf()));

		if(IsGreaterThanOrEqualAll(segLenV, distLeftV))
		{
			// Note: segLenV must be >0, because we know distLeftV > 0 and segLenV >= distLeftV,
			// so should be OK to divide.
			// Note 2: we use InvScaleFast() here, which is a slight approximation, but when comparing
			// the output computed with the output compared with a full division, the difference is really
			// small, didn't see a 0.01 m tolerance assert fail during testing.
			const ScalarV lerpTApproxV = InvScaleFast(distLeftV, segLenV);
			const Vec3V outPosV = AddScaled(currV, vecV, lerpTApproxV);

			Assert(rage::FPIsFinite(lerpTApproxV.Getf()));

			distanceRemainingV = Add(distanceRemainingV, distLeftV);

			outPos = outPosV;
			outDistanceRemainingV = distanceRemainingV;

			Assert(rage::FPIsFinite(outPos.GetXf()) && rage::FPIsFinite(outPos.GetYf()) && rage::FPIsFinite(outPos.GetZf()));

			return iRouteFlags;
		}

		distLeftV = Subtract(distLeftV, segLenV);
		distanceRemainingV = Add(distanceRemainingV, segLenV);
		iSeg++;

		u32 iRetFlags = 0;
		if(iSeg == iLastIndex)
			iRetFlags |= GPAFlag_HitRouteEnd;

		const CNavMeshExpandedRouteVert& pt = pPoints[iSeg];

		const bool bIsClimb = pt.m_iWptFlag.IsClimb();
		const bool bIsDrop = pt.m_iWptFlag.IsDrop();
		const bool bIsLadder = pt.m_iWptFlag.IsLadderClimb();

		if (bIsClimb || bIsDrop || bIsLadder)
		{
			if(bIsLadder)	// We always consider ladder climbs for now
			{
				iRetFlags |= GPAFlag_HitWaypointFlag;
				iRetFlags |= GPSFlag_HitWaypointFlagLadder;	// For special stops
			}
			else if( bHaltAtClimbs && bIsClimb )
				iRetFlags |= GPAFlag_HitWaypointFlag;
			else if( bHaltAtDrops && bIsDrop )
				iRetFlags |= GPAFlag_HitWaypointFlag;

			// So we can turn off navmesh edge avoidance
			// Ladders are always reported even if this function is not to halt at them
			// Edge avoidance is turned off for approaching ladders elsewhere anyway
			if(bIsClimb || bIsDrop)
				iRouteFlags |= GPAFlag_UpcomingClimbOrDrop;	

			if((bIsClimb || bIsLadder) && !bFoundClimbWpt && pOutDistToClimb)
			{
				*pOutDistToClimb = distanceRemainingV;
				bFoundClimbWpt = true;
			}
			if(bIsDrop && !bFoundDropWpt && pOutDistToDrop)
			{
				*pOutDistToDrop = distanceRemainingV;
				bFoundDropWpt = true;
			}
		}

		if (pt.m_iWptFlag.IsNarrow() && IsGreaterThanOrEqualAll(distToCheckForNarrow, distanceRemainingV))
			iRouteFlags = GPAFlag_HitWaypointFlagPullOut;

		if((iRetFlags & GPAFlag_MaskForRouteAhead) != 0)
		{

			// We've reached the end of the route, or encountered a waypoint which we can't progress past

			if(iSeg == m_iClosestSegOnExpandedRoute + 1)
			{
				outPos = nextV;
				if(outLengthOfFinalSegment)		// Note: in active code, nothing actually uses this - if that remains the case we should remove this code.
				{
					const ScalarV lengthOfFinalSegmentV = Dist(pPoints[iSeg-1].GetPos(), nextV);
					*outLengthOfFinalSegment = lengthOfFinalSegmentV;
				}
			}
			else
			{
				outPos = nextV;
				if(outLengthOfFinalSegment)
				{
					*outLengthOfFinalSegment = segLenV;
				}
			}
			outDistanceRemainingV = distanceRemainingV;

			return iRetFlags | iRouteFlags;
		}

		currV = nextV;
		nextV = pPoints[iSeg+1].GetPos();
		vecV = Subtract(nextV, currV);
	}

	outPos = pPoints[iLastIndex].GetPos();
	outDistanceRemainingV = distanceRemainingV;

	Assert(rage::FPIsFinite(outPos.GetXf()) && rage::FPIsFinite(outPos.GetYf()) && rage::FPIsFinite(outPos.GetZf()));

	return GPAFlag_HitRouteEnd | iRouteFlags;
}

bool CTaskNavBase::ProcessCornering(CPed * pPed, Vec3V_In vPosAhead, const float fLookAheadTime, const CMoveBlendRatioSpeeds& moveSpeeds, const CMoveBlendRatioTurnRates& turnRates)
{
	// Where this function is called from, we have already tested this:
	Assert(!pPed->IsStrafing());

	CTask* pSubTask = GetSubTask();
	if(!pSubTask || !m_pRoute || pPed->GetPedResetFlag( CPED_RESET_FLAG_SuppressSlowingForCorners ) )
		return false;

	// Shouldn't have to check this, I think, I don't think it's really valid for a move task to have
	// anything else than a move task as a subtask, is it?
	Assert(pSubTask->IsMoveTask());

	float fNewBMR = CTaskMoveGoToPoint::CalculateMoveBlendRatioForCorner(pPed, vPosAhead, fLookAheadTime, pPed->GetMotionData()->GetCurrentMbrY(), m_fMoveBlendRatio, CTaskMoveGoToPoint::ms_fAccelMBR, GetTimeStep(), moveSpeeds, turnRates);
	
	if (pPed->GetPedResetFlag(CPED_RESET_FLAG_MoveBlend_bFleeTaskRunning) && m_fMoveBlendRatio >= MBR_RUN_BOUNDARY)
		fNewBMR = Max(fNewBMR, MBR_RUN_BOUNDARY);	// Enforce running for fleeing peds even in very sharp corners

	pSubTask->PropagateNewMoveSpeed(fNewBMR);

	return true;
}

bool CTaskNavBase::RetransformRouteOnDynamicNavmesh(const CPed * pPed)
{
	if(!m_pOnEntityRouteData || !m_pOnEntityRouteData->m_pDynamicEntityRouteIsOn || !m_pRoute || !m_pOnEntityRouteData->m_pLocalSpaceRoute)
		return false;

	AI_OPTIMISATIONS_OFF_ONLY( (m_pRoute->GetSize() == m_pOnEntityRouteData->m_pLocalSpaceRoute->GetSize()); )
	if(m_pRoute->GetSize() != m_pOnEntityRouteData->m_pLocalSpaceRoute->GetSize())
		return false;

	// Transform all the route elements in m_pPointRoute
	const Matrix34 mat = MAT34V_TO_MATRIX34(m_pOnEntityRouteData->m_pDynamicEntityRouteIsOn->GetMatrix());
	for(int p=0; p<m_pOnEntityRouteData->m_pLocalSpaceRoute->GetSize(); p++)
	{
		Vector3 vTransformed;
		mat.Transform(m_pOnEntityRouteData->m_pLocalSpaceRoute->GetPosition(p), vTransformed);
		m_pRoute->Set(p, vTransformed, m_pOnEntityRouteData->m_pLocalSpaceRoute->GetWaypoint(p) );
	}

	if(GetState()!=NavBaseState_FollowingPath)
		return false;

	Assert(m_iProgress >= 0 && m_iProgress < m_pRoute->GetSize());
	const Vector3 & vGoToTarget = m_pRoute->GetPosition(m_iProgress);

	if(GetSubTask())
	{
		Assert(GetSubTask()->IsMoveTask());
		((CTaskMove*)GetSubTask())->SetTarget(pPed, VECTOR3_TO_VEC3V(vGoToTarget));
	}

	return true;
}

bool CTaskNavBase::IsExitingScenario(Vec3V_InOut vExitPosition) const
{
	//Grab the ped.
	const CPed* pPed = GetPed();

	//Ensure the ped is exiting the vehicle.
	CTaskUseScenario* pTask = static_cast<CTaskUseScenario *>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_USE_SCENARIO));
	if(!pTask || !pTask->IsExitingAndHasANewPosition())
	{
		return false;
	}

	//Set the exit position.
	pTask->GetExitPosition(RC_VECTOR3(vExitPosition));

	return true;
}


bool CTaskNavBase::IsExitingVehicle(Vec3V_InOut vExitPosition) const
{
	//Grab the ped.
	const CPed* pPed = GetPed();

	//Ensure the ped is exiting the vehicle.
	CTaskExitVehicle* pTask = static_cast<CTaskExitVehicle *>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_EXIT_VEHICLE));
	if(!pTask || !pTask->HasValidExitPosition())
	{
		return false;
	}

	//Set the exit position.
	vExitPosition = pTask->GetExitPosition();

	// If this is a plane, then push out the exit position to the edge of the bounding box in the +/- X direction
	// If we don't do this, then the path start will be resolved outside of the bbox of the vehicle by the pathfinder,
	// in the closest direction to any edge (and this might be directly forwards, through the propellers)
	if( pTask->GetVehicle() && pTask->GetVehicle()->InheritsFromPlane() )
	{
		spdAABB tempBox;
		const spdAABB &bbox = pTask->GetVehicle()->GetBoundBox(tempBox);
		const float fBoxHalfWidth = (bbox.GetMax().GetXf() - bbox.GetMin().GetXf()) * 0.5f;
		Matrix34 mat = MAT34V_TO_MATRIX34(pTask->GetVehicle()->GetTransform().GetMatrix());
		float fDistX = DotProduct( mat.a, VEC3V_TO_VECTOR3(vExitPosition) - mat.d );
		float fDelta = fBoxHalfWidth - Abs(fDistX);
		if( fDelta > 0.0f )
		{
			vExitPosition += VECTOR3_TO_VEC3V(mat.a * Sign(fDistX) * fDelta);
		}
	}

	return true;
}

bool CTaskNavBase::ShouldWaitInVehicle() const
{
	//Grab the ped.
	const CPed* pPed = GetPed();

	//Ensure the ped is in a vehicle.
	const CVehicle* pVehicle = pPed->GetVehiclePedInside();
	if(!pVehicle)
	{
		return false;
	}

	//Check if the ped is exiting the vehicle.
	CTaskExitVehicle* pTask = static_cast<CTaskExitVehicle *>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_EXIT_VEHICLE));
	if(pTask)
	{
		//Debug: allow ped to leave vehicle earlier, as soon as we have chosen an exit door
		static dev_bool bForceDoorsOpenForNavigation = true;

		if(bForceDoorsOpenForNavigation)
		{
			return false;
		}

		//Check if the door is (mostly) open.
		static float s_fMinRatio = 0.8f;
		if(pTask->IsDoorOpen(s_fMinRatio))
		{
			return false;
		}
	}

	return true;
}

void CTaskNavBase::CalcTimeNeededToCompleteRouteSegment(const CPed * pPed, const s32 iProgress)
{
	//************************************************************************
	// Set a maximum time that the ped should spend getting to their target

	CMoveBlendRatioSpeeds moveSpeeds;
	pPed->GetCurrentMotionTask()->GetMoveSpeeds(moveSpeeds);
	const float fMoveSpeed = CTaskMotionBase::GetActualMoveSpeed(moveSpeeds.GetSpeedsArray(), Clamp(m_fInitialMoveBlendRatio, MOVEBLENDRATIO_STILL, MOVEBLENDRATIO_SPRINT));

	static const float fTimeToGetToWaypointMult = 1.5f;		// Multiplier to the actual time to get to this waypoint
	static const int iMinRouteSegTimeMs = 8000;				// be lenient & allow 8secs for all route-segments before retrying

	m_iTimeSpentOnCurrentRouteSegmentMs = 0;

	if(iProgress < m_pRoute->GetSize())
	{
		const float fDistToTarget = (m_pRoute->GetPosition(iProgress) - VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition())).Mag();
		const float fTimeToTarget = fDistToTarget / fMoveSpeed;
		m_iTimeShouldSpendOnCurrentRouteSegmentMs = (u16) MIN(65535, (int)(fTimeToTarget * fTimeToGetToWaypointMult * 1000.0f));
		m_iTimeShouldSpendOnCurrentRouteSegmentMs = MAX(m_iTimeShouldSpendOnCurrentRouteSegmentMs, iMinRouteSegTimeMs);
	}
	else
	{
		m_iTimeShouldSpendOnCurrentRouteSegmentMs = iMinRouteSegTimeMs;
	}
}

CTask::FSM_Return CTaskNavBase::OnFailedRoute(CPed * UNUSED_PARAM(pPed), const TPathResultInfo & UNUSED_PARAM(pathResultInfo))
{
	return FSM_Quit;
}

bool CTaskNavBase::IsPedMoving(CPed * pPed) const
{
	if (pPed->GetMotionData()->GetCurrentMbrY() > MBR_WALK_BOUNDARY)
		return true;

	dev_float KeepMovingSpeedThresholdSqr = square(0.25f);
	if (pPed->GetVelocity().XYMag2() > KeepMovingSpeedThresholdSqr)
		return true;

	switch(pPed->GetMotionData()->GetForcedMotionStateThisFrame())
	{
		case CPedMotionStates::MotionState_Walk:
		case CPedMotionStates::MotionState_Run:
		case CPedMotionStates::MotionState_Sprint:
		case CPedMotionStates::MotionState_Crouch_Walk:
		case CPedMotionStates::MotionState_Crouch_Run:
		case CPedMotionStates::MotionState_Stealth_Walk:
		case CPedMotionStates::MotionState_Stealth_Run:
		case CPedMotionStates::MotionState_ActionMode_Walk:
		case CPedMotionStates::MotionState_ActionMode_Run:
			return true;
	}

	return false;
}

bool CTaskNavBase::ShouldKeepMovingWhilstWaitingForPath(CPed * pPed, bool & bMayUsePreviousTarget) const
{
	bMayUsePreviousTarget =
		m_NavBaseInternalFlags.IsSet(CNavBaseInternalFlags::NB_AbortedForDoorCollision)==false &&
		m_NavBaseInternalFlags.IsSet(CNavBaseInternalFlags::NB_AbortedForCollisionEvent)==false &&
		m_NavBaseInternalFlags.IsSet(CNavBaseInternalFlags::NB_ExceededVerticalDistanceFromPath)==false;

	if (!IsPedMoving(pPed))
		return false;

	if(m_NavBaseInternalFlags.IsSet(CNavBaseInternalFlags::NB_AbortedForDoorCollision) ||
		m_NavBaseInternalFlags.IsSet(CNavBaseInternalFlags::NB_AbortedForCollisionEvent) ||
		 m_NavBaseInternalFlags.IsSet(CNavBaseInternalFlags::NB_ExceededVerticalDistanceFromPath))
		return true;

	// If we have a periodic recalc frequency, and it has just expired
	// and this is not our first path - then continue moving whilst we calculate it
	if(m_iRecalcPathFreqMs != 0 && m_iRecalcPathTimerMs==m_iRecalcPathFreqMs && m_iLastPathHandle!=PATH_HANDLE_NULL)
		return true;

	return(m_NavBaseConfigFlags.IsSet(CNavBaseConfigFlags::NB_KeepMovingWhilstWaitingForPath));
}

#if !__FINAL
const char * CTaskNavBase::GetBaseStateName(s32 iState)
{
	switch(iState)
	{
		case NavBaseState_Initial: return "Initial";
		case NavBaseState_WaitingInVehiclePriorToRequest: return "WaitingInVehiclePriorToRequest";
		case NavBaseState_RequestPath: return "RequestPath";
		case NavBaseState_MovingWhilstWaitingForPath: return "MovingWhilstWaitingForPath";
		case NavBaseState_StationaryWhilstWaitingForPath: return "StationaryWhilstWaitingForPath";
		case NavBaseState_WaitingToLeaveVehicle: return "WaitingToLeaveVehicle";
		case NavBaseState_WaitingAfterFailureToObtainPath : return "WaitingAfterFailureToObtainPath";
		case NavBaseState_FollowingPath: return "FollowingPath";
		case NavBaseState_SlideToCoord: return "SlideToCoord";
		case NavBaseState_StoppingAtTarget: return "StoppingAtTarget";
		case NavBaseState_WaitingForWaypointActionEvent: return "WaitingForWaypointActionEvent";
		case NavBaseState_WaitingAfterFailedWaypointAction: return "WaitingAfterFailedWaypointAction";
		case NavBaseState_GetOntoMainNavMesh: return "GetOntoMainNavMesh";
		case NavBaseState_AchieveHeading: return "AchieveHeading";
		case NavBaseState_UsingLadder: return "UsingLadder";
		default: return NULL;
	}
}
#endif //!__FINAL

#if !__FINAL && DEBUG_DRAW
const char * CTaskNavBase::GetInfoForWaypoint(const TNavMeshWaypointFlag & wpt, Color32 & col) const
{
	const char * pDesc = NULL;

	u16 iFlags = wpt.m_iSpecialActionFlags;
	static const u16 iMask = (WAYPOINT_FLAG_CLIMB_HIGH|WAYPOINT_FLAG_CLIMB_LOW|WAYPOINT_FLAG_DROPDOWN|WAYPOINT_FLAG_OPEN_DOOR|WAYPOINT_FLAG_CLIMB_LADDER|WAYPOINT_FLAG_DESCEND_LADDER|WAYPOINT_FLAG_CLIMB_OBJECT|WAYPOINT_FLAG_ADJUSTED_TARGET_POS|WAYPOINT_FLAG_HIERARCHICAL_PATHFIND);
	iFlags &= iMask;

	if(!iFlags)
	{
		col = Color_grey;
		pDesc = NULL;	// NULL string indicates a regular wpt
	}
	else if(iFlags&WAYPOINT_FLAG_CLIMB_HIGH)
	{
		col = Color_SpringGreen1;
		pDesc = "HighClimb";
	}
	else if(iFlags&WAYPOINT_FLAG_CLIMB_LOW)
	{
		col = Color_SpringGreen2;
		pDesc = "LowClimb";
	}
	else if(iFlags&WAYPOINT_FLAG_DROPDOWN)
	{
		col = Color_SpringGreen3;
		pDesc = "DropDown";
	}
	else if(iFlags&WAYPOINT_FLAG_OPEN_DOOR)
	{
		col = Color_yellow;
		pDesc = "OpenDoor";
	}
	else if(iFlags&WAYPOINT_FLAG_CLIMB_LADDER)
	{
		col = Color_OrangeRed1;
		pDesc = "ClimbLadder";
	}
	else if(iFlags&WAYPOINT_FLAG_DESCEND_LADDER)
	{
		col = Color_OrangeRed2;
		pDesc = "DescendLadder";
	}
	else if(iFlags&WAYPOINT_FLAG_CLIMB_OBJECT)
	{
		col = Color_OrangeRed3;
		pDesc = "ClimbObject";
	}
	else if(iFlags&WAYPOINT_FLAG_ADJUSTED_TARGET_POS)
	{
		col = Color_grey50;
		pDesc = "MovedTarget";
	}
	else if(iFlags&WAYPOINT_FLAG_HIERARCHICAL_PATHFIND)
	{
		col = Color_MediumBlue;
		pDesc = "Hierarchical";
	}

	return pDesc;
}

void CTaskNavBase::DrawPedText(const CPed * pPed, const Vector3 & vWorldCoords, const Color32 iTxtCol, int & iOffsetY) const
{
	char taskText[256];

	//****************
	// Path handle

	sprintf(taskText, "hPath: 0x%x", m_iPathHandle);
	grcDebugDraw::Text( vWorldCoords, iTxtCol, 0, iOffsetY, taskText);
	iOffsetY+=grcDebugDraw::GetScreenSpaceTextHeight();

	sprintf(taskText, "hLastPath: 0x%x", m_iLastPathHandle);
	grcDebugDraw::Text( vWorldCoords, iTxtCol, 0, iOffsetY, taskText);
	iOffsetY+=grcDebugDraw::GetScreenSpaceTextHeight();

	//***************************
	// Progress

	if(m_pRoute && m_pRoute->GetSize())
	{
		sprintf(taskText, "Progress: %i/%i", m_iProgress, m_pRoute->GetSize());
		grcDebugDraw::Text( vWorldCoords, iTxtCol, 0, iOffsetY, taskText);
		iOffsetY+=grcDebugDraw::GetScreenSpaceTextHeight();

		sprintf(taskText, "fSplineProgress: %f", m_fSplineTVal);
		grcDebugDraw::Text( vWorldCoords, iTxtCol, 0, iOffsetY, taskText);
		iOffsetY+=grcDebugDraw::GetScreenSpaceTextHeight();
	}

	//**********************************
	// Time spent on this route segment

	sprintf(taskText, "Time: %i/%i", m_iTimeSpentOnCurrentRouteSegmentMs, m_iTimeShouldSpendOnCurrentRouteSegmentMs);
	grcDebugDraw::Text( vWorldCoords, iTxtCol, 0, iOffsetY, taskText);
	iOffsetY+=grcDebugDraw::GetScreenSpaceTextHeight();

	sprintf(taskText, "Free space metric: %.2f", pPed->GetNavMeshTracker().GetFreeSpaceMetric());
	grcDebugDraw::Text( vWorldCoords, iTxtCol, 0, iOffsetY, taskText);
	iOffsetY+=grcDebugDraw::GetScreenSpaceTextHeight();

	//grcDebugDraw::Text( vWorldCoords, iTxtCol, 0, iOffsetY, taskText);

	//***************************
	// CTaskNavBase Flags

	//draw flags here
}

void CTaskNavBase::DebugVisualiserDisplay(const CPed * pPed, const Vector3 & vWorldCoords, s32 iOffsetY) const
{
	int p;
	static const Vector3 vMarkerTop(0.0f,0.0f,2.0f);
	static const float fMarkerTopRadius = 0.1f;
	static const float fWptTxtRange = 30.0f;
	static const float fTxtRange = 30.0f;

#if __DEV
	camDebugDirector& debugDirector = camInterface::GetDebugDirector();
	Vector3 vOrigin = debugDirector.IsFreeCamActive() ? debugDirector.GetFreeCamFrame().GetPosition() : VEC3V_TO_VECTOR3(FindPlayerPed()->GetTransform().GetPosition());
#else
	Vector3 vOrigin = VEC3V_TO_VECTOR3(FindPlayerPed()->GetTransform().GetPosition());
#endif


	//*********************************************************************************
	// Draw the ped's route points.  We can draw the route markers for each point,
	// with some colour-coded info (or text) to describe the waypoint type.
	if(m_pRoute)
	{
		for(p=0; p<m_pRoute->GetSize(); p++)
		{
			Vector3 vRoutePt = m_pRoute->GetPosition(p);
			Vector3 vRoutePtTop = vRoutePt + vMarkerTop;

			// Remove the extra height which we'd added on when the path was created
			vRoutePt.z -= ms_fHeightAboveReturnedPointsForRouteVerts;

			const Vector3 vDiff = vRoutePt - vOrigin;
			const float fDist = vDiff.Mag();
			const float fScale = 1.0f - (fDist / fWptTxtRange);
			const u8 iAlpha = (u8)Clamp((int)(255.0f * fScale), 0, 255);

			const TNavMeshWaypointFlag & iWpt = m_pRoute->GetWaypoint(p);

			Color32 iWptCol;
			const char * pWptDesc = GetInfoForWaypoint(iWpt, iWptCol);
			if(pWptDesc)
			{
				grcDebugDraw::Line(vRoutePt, vRoutePtTop, Color_LightCyan);
				grcDebugDraw::Sphere(vRoutePtTop, fMarkerTopRadius, iWptCol, true);

				Vector3 vTextPos = vRoutePtTop;

				if(fDist < fWptTxtRange)
				{
					if(pWptDesc)
					{
						iWptCol.SetAlpha(iAlpha);
						grcDebugDraw::Text( vTextPos, iWptCol, pWptDesc);
					}

					vTextPos.y += grcDebugDraw::GetScreenSpaceTextHeight();

					int iSpace = m_pRoute->GetFreeSpaceApproachingWaypoint(p);
					if(iSpace > 0)
					{
						iWptCol = Color_OrangeRed3;		iWptCol.SetAlpha(iAlpha);
						static char tmp[32];
						sprintf(tmp, "space approaching : %i", iSpace);
						grcDebugDraw::Text(vTextPos, iWptCol, tmp);
					}
				}
			}
		}
	}

	//***********************************************************************
	//	Draw a ped-width strip following the route at the base height.

	Color32 iRouteCol = Color_PaleGreen;
	// If this route is being used to return to another route, then draw in amber
	if(GetParent() && GetParent()->GetTaskType()==CTaskTypes::TASK_MOVE_RETURN_TO_ROUTE)
		iRouteCol = Color_OrangeRed1;

	if(m_pRoute)
	{
		float fPedRadius = pPed->GetCapsuleInfo()->GetHalfWidth();
#if __BANK
		if(ms_fOverrideRadius != PATHSERVER_PED_RADIUS)
			fPedRadius = ms_fOverrideRadius;
#endif

		Vector3 vTriVerts[3];

		//-----------------------------------------------
		// Render non-splined linear route in low alpha

		iRouteCol.SetAlpha(64);

		for(p=0; p<m_pRoute->GetSize()-1; p++)
		{
			Vector3 vRoutePt = m_pRoute->GetPosition(p);
			vRoutePt.z -= ms_fHeightAboveReturnedPointsForRouteVerts;
			Vector3 vNextRoutePt = m_pRoute->GetPosition(p+1);
			vNextRoutePt.z -= ms_fHeightAboveReturnedPointsForRouteVerts;

			Vector3 vVecToNext = vNextRoutePt - vRoutePt;
			vVecToNext.Normalize();
			Vector3 vRight = CrossProduct(vVecToNext, Vector3(0.0f,0.0f,1.0f));

			vTriVerts[0] = vRoutePt + (vRight * fPedRadius);
			vTriVerts[1] = vRoutePt - (vRight * fPedRadius);
			vTriVerts[2] = vNextRoutePt + (vRight * fPedRadius);
			grcDebugDraw::Poly(RCC_VEC3V(vTriVerts[2]), RCC_VEC3V(vTriVerts[1]), RCC_VEC3V(vTriVerts[0]), iRouteCol);

			vTriVerts[0] = vRoutePt - (vRight * fPedRadius);
			vTriVerts[1] = vNextRoutePt - (vRight * fPedRadius);
			vTriVerts[2] = vNextRoutePt + (vRight * fPedRadius);
			grcDebugDraw::Poly(RCC_VEC3V(vTriVerts[2]), RCC_VEC3V(vTriVerts[1]), RCC_VEC3V(vTriVerts[0]), iRouteCol);

			// If there's another point after vNextRoutePt, then see if we can make some
			// bevel tris so that the path looks a bit nicer.
			if(p < m_pRoute->GetSize()-2)
			{
				Vector3 vNextNextRoutePt = m_pRoute->GetPosition(p+2);
				vNextNextRoutePt.z -= ms_fHeightAboveReturnedPointsForRouteVerts;

				Vector3 vVecToNextNext = vNextNextRoutePt - vNextRoutePt;
				vVecToNextNext.Normalize();
				Vector3 vRightNext = CrossProduct(vVecToNextNext, Vector3(0.0f,0.0f,1.0f));

				vTriVerts[0] = vNextRoutePt + (vRight * fPedRadius);
				vTriVerts[1] = vNextRoutePt;
				vTriVerts[2] = vNextRoutePt + (vRightNext * fPedRadius);
				grcDebugDraw::Poly(RCC_VEC3V(vTriVerts[2]), RCC_VEC3V(vTriVerts[1]), RCC_VEC3V(vTriVerts[0]), iRouteCol);

				vTriVerts[0] = vNextRoutePt - (vRight * fPedRadius);
				vTriVerts[1] = vNextRoutePt;
				vTriVerts[2] = vNextRoutePt - (vRightNext * fPedRadius);
				grcDebugDraw::Poly(RCC_VEC3V(vTriVerts[2]), RCC_VEC3V(vTriVerts[1]), RCC_VEC3V(vTriVerts[0]), iRouteCol);
			}
		}

		//----------------------------------------------------------
		// Render the expanded route using splines, in full opacity

		iRouteCol.SetAlpha(255);

		static bool s_StartAtCurrentProgress = false;
		int startSegment = 0;
		if ( s_StartAtCurrentProgress )
		{
			startSegment = Max(0, m_iProgress-1);
		}
		ms_iNumExpandedRouteVerts = m_pRoute->ExpandRoute(ms_vExpandedRouteVerts, NULL, startSegment, m_pRoute->GetSize());

		for(p=0; p<ms_iNumExpandedRouteVerts-1; p++)
		{
			Vector3 vRoutePt = VEC3V_TO_VECTOR3(ms_vExpandedRouteVerts[p].GetPos());
			vRoutePt.z -= ms_fHeightAboveReturnedPointsForRouteVerts;
			Vector3 vNextRoutePt = VEC3V_TO_VECTOR3(ms_vExpandedRouteVerts[p+1].GetPos());
			vNextRoutePt.z -= ms_fHeightAboveReturnedPointsForRouteVerts;

			Vector3 vVecToNext = vNextRoutePt - vRoutePt;
			vVecToNext.Normalize();
			Vector3 vRight = CrossProduct(vVecToNext, Vector3(0.0f,0.0f,1.0f));

			vTriVerts[0] = vRoutePt + (vRight * fPedRadius);
			vTriVerts[1] = vRoutePt - (vRight * fPedRadius);
			vTriVerts[2] = vNextRoutePt + (vRight * fPedRadius);
			grcDebugDraw::Poly(RCC_VEC3V(vTriVerts[2]), RCC_VEC3V(vTriVerts[1]), RCC_VEC3V(vTriVerts[0]), iRouteCol);

			vTriVerts[0] = vRoutePt - (vRight * fPedRadius);
			vTriVerts[1] = vNextRoutePt - (vRight * fPedRadius);
			vTriVerts[2] = vNextRoutePt + (vRight * fPedRadius);
			grcDebugDraw::Poly(RCC_VEC3V(vTriVerts[2]), RCC_VEC3V(vTriVerts[1]), RCC_VEC3V(vTriVerts[0]), iRouteCol);

			// If there's another point after vNextRoutePt, then see if we can make some
			// bevel tris so that the path looks a bit nicer.
			if(p < ms_iNumExpandedRouteVerts-2)
			{
				Vector3 vNextNextRoutePt = VEC3V_TO_VECTOR3(ms_vExpandedRouteVerts[p+2].GetPos());
				vNextNextRoutePt.z -= ms_fHeightAboveReturnedPointsForRouteVerts;

				Vector3 vVecToNextNext = vNextNextRoutePt - vNextRoutePt;
				vVecToNextNext.Normalize();
				Vector3 vRightNext = CrossProduct(vVecToNextNext, Vector3(0.0f,0.0f,1.0f));

				vTriVerts[0] = vNextRoutePt + (vRight * fPedRadius);
				vTriVerts[1] = vNextRoutePt;
				vTriVerts[2] = vNextRoutePt + (vRightNext * fPedRadius);
				grcDebugDraw::Poly(RCC_VEC3V(vTriVerts[2]), RCC_VEC3V(vTriVerts[1]), RCC_VEC3V(vTriVerts[0]), iRouteCol);

				vTriVerts[0] = vNextRoutePt - (vRight * fPedRadius);
				vTriVerts[1] = vNextRoutePt;
				vTriVerts[2] = vNextRoutePt - (vRightNext * fPedRadius);
				grcDebugDraw::Poly(RCC_VEC3V(vTriVerts[2]), RCC_VEC3V(vTriVerts[1]), RCC_VEC3V(vTriVerts[0]), iRouteCol);
			}
		}
	}

	

	//***********************************************************************
	//	Draw a line between the startpos & targetpos

	//****************************************************************************
	//	Draw a line from the ped to the closest point on the current route segment

	//*****************************************************************************
	//	Draw some text output which allows us to investigate the state of the task

	int iTextOffset = iOffsetY;

	Vector3 vDiff = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()) - vOrigin;
	float fDist = vDiff.Mag();
	if(fDist < fTxtRange)
	{
		//*********************
		// Set up font

		float fScale = 1.0f - (fDist / fTxtRange);
		fScale = Max(fScale, 0.5f);
		u8 iAlpha = (u8)Clamp((int)(255.0f * fScale), 0, 255);
		Color32 iTxtCol = Color_chartreuse;
		iTxtCol.SetAlpha(iAlpha);

		DrawPedText(pPed, vWorldCoords, iTxtCol, iTextOffset);
	}
}
#endif









//----------------------------------------------------------------------------------------------------------


//********************************************************************************
//	CTaskMoveGetOntoMainNavMesh
//	Sometimes ped can get themselves trapped on small sections of navmesh with no
//	connectivity to the main mesh.  In these circumstances the ped will be unable
//	to find a path & just stand still.  This task is created as a subtask to the
//	CTaskComplexMoveFollowNavMesg task, and is designed to identify if this has
//	happened and attempt to fix the situation.

const float CTaskMoveGetOntoMainNavMesh::ms_fTargetRadius = 0.25f;
const float CTaskMoveGetOntoMainNavMesh::ms_fMaxAreaToAttempt = 64.0f;

CTaskMoveGetOntoMainNavMesh::CTaskMoveGetOntoMainNavMesh(const float fMoveBlendRatio, const Vector3 & vTarget, const int iRecoveryMode, const int iAttemptTime) :
	CTaskMove(fMoveBlendRatio),
	m_vNavMeshTarget(vTarget),
	m_vTarget(vTarget),
	m_iAttemptTime(iAttemptTime),
	m_hPathHandle(PATH_HANDLE_NULL),
	m_hCalcAreaUnderfootHandle(PATH_HANDLE_NULL),
	m_bIsOnSmallAreaOfNavMesh(false),
	m_iRecoveryMode(iRecoveryMode)
{
	SetInternalTaskType(CTaskTypes::TASK_MOVE_GET_ONTO_MAIN_NAVMESH);
}

CTaskMoveGetOntoMainNavMesh::~CTaskMoveGetOntoMainNavMesh()
{
	if(m_hPathHandle)
	{
		CPathServer::CancelRequest(m_hPathHandle);
		m_hPathHandle = PATH_HANDLE_NULL;
	}
	if(m_hCalcAreaUnderfootHandle)
	{
		CPathServer::CancelRequest(m_hCalcAreaUnderfootHandle);
		m_hCalcAreaUnderfootHandle = PATH_HANDLE_NULL;
	}
}


#if !__FINAL
void CTaskMoveGetOntoMainNavMesh::Debug() const
{
	if(GetSubTask())
		GetSubTask()->Debug();
}
#endif

CTask::FSM_Return CTaskMoveGetOntoMainNavMesh::ProcessPreFSM()
{
	CPed * pPed = GetPed();
	if(pPed->GetNavMeshTracker().GetIsValid() && !pPed->GetNavMeshTracker().GetNavPolyData().m_bIsolated)
		return FSM_Quit;

	// Prevent ped entering low-lod physics whilst performing recovery task
	pPed->GetPedAiLod().SetBlockedLodFlag( CPedAILod::AL_LodPhysics );

	return FSM_Continue;
}

CTask::FSM_Return
CTaskMoveGetOntoMainNavMesh::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	FSM_Begin
		FSM_State(State_Initial)
			FSM_OnUpdate
				return Initial_OnUpdate(pPed);
		FSM_State(State_CalculatingAreaUnderfoot)
			FSM_OnUpdate
				return CalculatingAreaUnderfoot_OnUpdate(pPed);
		FSM_State(State_FindingReversePath)
			FSM_OnUpdate
				return FindingReversePath_OnUpdate(pPed);
		FSM_State(State_MovingTowardsTarget)
			FSM_OnUpdate
				return MovingTowardsTarget_OnUpdate(pPed);
		FSM_State(State_MovingToLastNonIsolatedPosition)
			FSM_OnUpdate
				return MovingToLastNonIsolatedPosition_OnUpdate(pPed);
	FSM_End
}

CTask::FSM_Return
CTaskMoveGetOntoMainNavMesh::Initial_OnUpdate(CPed * pPed)
{
	//*******************************************************************************
	// Request a route in reverse, from the target position to the pPed's position
	// with a fairly large completion radius.  If this succeeds, then try just doing
	// a goto point towards this target.  The idea is that this will get us onto
	// the navmesh at least, at which point a regular route can be followed.

	Assert(m_hPathHandle==PATH_HANDLE_NULL);
	if(m_hPathHandle)
	{
		CPathServer::CancelRequest(m_hPathHandle);
		m_hPathHandle = PATH_HANDLE_NULL;
	}
	Assert(m_hCalcAreaUnderfootHandle==PATH_HANDLE_NULL);
	if(m_hCalcAreaUnderfootHandle)
	{
		CPathServer::CancelRequest(m_hCalcAreaUnderfootHandle);
		m_hCalcAreaUnderfootHandle = PATH_HANDLE_NULL;
	}

	// We can only attempt to goto our last safe position, if we have a valid position
	// and if we're still a reasonable distance from it (lets say 10m)
	Vector3 vPedPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
	Vector3 vLastGoodPos = pPed->GetPedIntelligence()->GetLastMainNavMeshPosition();

	static dev_float fMaxRecoveryDelta = 10.0f;

	if( m_iRecoveryMode==RecoveryMode_GoToLastNonIsolatedPosition &&
		(!vLastGoodPos.IsClose(VEC3_ZERO, 0.001f)) &&
		(vPedPos - vLastGoodPos).Mag2() < fMaxRecoveryDelta*fMaxRecoveryDelta)
	{
		m_iRecoveryMode = RecoveryMode_ReversePathFind;
	}

	// Attempt to find a reverse path back to our position, and then move in that direction
	if( m_iRecoveryMode == RecoveryMode_ReversePathFind )
	{

		m_hCalcAreaUnderfootHandle = CPathServer::RequestCalcAreaUnderfootSearch(VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()), 30.0f, (void*)pPed);

		SetState(State_CalculatingAreaUnderfoot);
		SetNewTask( CreateSubTask(CTaskTypes::TASK_MOVE_STAND_STILL, pPed) );
		SetFlag(aiTaskFlags::KeepCurrentSubtaskAcrossTransition);
	}
	// Move towards our last known non-isolated position
	else
	{
		m_AttemptTimer.Set(fwTimer::GetTimeInMilliseconds(), m_iAttemptTime);
		CTaskMoveGoToPointAndStandStill * pGotoTask = rage_new CTaskMoveGoToPointAndStandStill(m_fMoveBlendRatio, pPed->GetPedIntelligence()->GetLastMainNavMeshPosition(), ms_fTargetRadius);
		// We don't care about the vertical height of the goto target
		pGotoTask->SetTargetHeightDelta(1000.0f);
		SetNewTask( pGotoTask );

		SetState(State_MovingToLastNonIsolatedPosition);
		SetFlag(aiTaskFlags::KeepCurrentSubtaskAcrossTransition);
	}

	return FSM_Continue;
}

CTask::FSM_Return
CTaskMoveGetOntoMainNavMesh::CalculatingAreaUnderfoot_OnUpdate(CPed * pPed)
{
	Assert(GetSubTask() && GetSubTask()->GetTaskType()==CTaskTypes::TASK_MOVE_STAND_STILL);

	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		if(m_hCalcAreaUnderfootHandle==PATH_HANDLE_NULL)
			return FSM_Quit;

		const eReqResult ret = GetAreaRequestResult(pPed);
		if(ret==EContinue && m_bIsOnSmallAreaOfNavMesh)
		{
			Assert(m_hPathHandle==PATH_HANDLE_NULL);
			const u32 iFlags = PATH_FLAG_REDUCE_OBJECT_BBOXES;
			m_hPathHandle = CPathServer::RequestPath(m_vNavMeshTarget, VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()), iFlags, 8.0f, pPed);

			SetState(State_FindingReversePath);
			SetFlag(aiTaskFlags::KeepCurrentSubtaskAcrossTransition);
			SetNewTask( CreateSubTask(CTaskTypes::TASK_MOVE_STAND_STILL, pPed) );
		}
		else if(ret==EStillWaiting)
		{
			SetNewTask( CreateSubTask(CTaskTypes::TASK_MOVE_STAND_STILL, pPed) );
		}
		else
		{
			return FSM_Quit;
		}
	}

	return FSM_Continue;
}

CTask::FSM_Return
CTaskMoveGetOntoMainNavMesh::FindingReversePath_OnUpdate(CPed * pPed)
{
	Assert(GetSubTask() && GetSubTask()->GetTaskType()==CTaskTypes::TASK_MOVE_STAND_STILL);

	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		Assert(m_hPathHandle!=PATH_HANDLE_NULL);
		Assert(m_bIsOnSmallAreaOfNavMesh);
		if(m_hPathHandle==PATH_HANDLE_NULL)
			return FSM_Quit;

		const eReqResult ret = GetPathRequestResult(pPed);
		if(ret==EContinue)
		{
			SetState(State_MovingTowardsTarget);
			SetFlag(aiTaskFlags::KeepCurrentSubtaskAcrossTransition);
			SetNewTask( CreateSubTask(CTaskTypes::TASK_MOVE_GO_TO_POINT_AND_STAND_STILL, pPed) );
		}
		else if(ret==EStillWaiting)
		{
			SetNewTask( CreateSubTask(CTaskTypes::TASK_MOVE_STAND_STILL, pPed) );
		}
		else
		{
			return FSM_Quit;
		}
	}
	return FSM_Continue;
}

CTask::FSM_Return CTaskMoveGetOntoMainNavMesh::MovingTowardsTarget_OnUpdate(CPed * UNUSED_PARAM(pPed))
{
	Assert(GetSubTask() && GetSubTask()->GetTaskType()==CTaskTypes::TASK_MOVE_GO_TO_POINT_AND_STAND_STILL);

	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		return FSM_Quit;
	}
	if(m_AttemptTimer.IsSet() && m_AttemptTimer.IsOutOfTime() && GetSubTask()->MakeAbortable( ABORT_PRIORITY_URGENT, NULL))
	{
		return FSM_Quit;
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskMoveGetOntoMainNavMesh::MovingToLastNonIsolatedPosition_OnUpdate(CPed * UNUSED_PARAM(pPed))
{
	Assert(GetSubTask() && GetSubTask()->GetTaskType()==CTaskTypes::TASK_MOVE_GO_TO_POINT_AND_STAND_STILL);

	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		return FSM_Quit;
	}
	if(m_AttemptTimer.IsSet() && m_AttemptTimer.IsOutOfTime() && GetSubTask()->MakeAbortable( ABORT_PRIORITY_URGENT, NULL))
	{
		return FSM_Quit;
	}

	return FSM_Continue;
}

void CTaskMoveGetOntoMainNavMesh::DoAbort(const AbortPriority iPriority, const aiEvent* pEvent)
{
	if(m_hPathHandle)
	{
		CPathServer::CancelRequest(m_hPathHandle);
		m_hPathHandle = PATH_HANDLE_NULL;
	}
	if(m_hCalcAreaUnderfootHandle)
	{
		CPathServer::CancelRequest(m_hCalcAreaUnderfootHandle);
		m_hCalcAreaUnderfootHandle = PATH_HANDLE_NULL;
	}

	// Base class
	CTaskMove::DoAbort(iPriority, pEvent);
}

CTaskMoveGetOntoMainNavMesh::eReqResult
CTaskMoveGetOntoMainNavMesh::GetPathRequestResult(const CPed * pPed)
{
	Assert(m_hPathHandle!=PATH_HANDLE_NULL);

	EPathServerRequestResult ret = CPathServer::IsRequestResultReady(m_hPathHandle);
	if(ret == ERequest_Ready)
	{
		int iNumPts = 0;
		Vector3 * pPts = NULL;
		TNavMeshWaypointFlag * pWpts = NULL;
		TPathResultInfo resultInfo;
		EPathServerErrorCode errCode = CPathServer::LockPathResult(m_hPathHandle, &iNumPts, pPts, pWpts, &resultInfo);
		if(errCode != PATH_NO_ERROR)
		{
			CPathServer::UnlockPathResultAndClear(m_hPathHandle);
			m_hPathHandle = PATH_HANDLE_NULL;
			return EQuit;
		}

		if(iNumPts > 1)
		{
			static const float fGoToDist = 2.0f;
			Vector3 vMoveDir = pPts[iNumPts-2] - pPts[iNumPts-1];
			vMoveDir.Normalize();
			vMoveDir *= fGoToDist;
			m_vTarget = pPts[ iNumPts-1 ] + vMoveDir;
		}
		else
		{
			Assert(iNumPts!=0);
			if(iNumPts!=0)
			{
				m_vTarget = pPts[ iNumPts-1 ];
			}
			else
			{
				// Error, shouldn't happen, something's fucked up..
				m_vTarget = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition() + pPed->GetTransform().GetB());
			}
		}

		CPathServer::UnlockPathResultAndClear(m_hPathHandle);
		m_hPathHandle = PATH_HANDLE_NULL;

		return EContinue;
	}
	else if(ret == ERequest_NotFound)
	{
		m_hPathHandle = PATH_HANDLE_NULL;
		return EQuit;
	}

	return EStillWaiting;
}

CTaskMoveGetOntoMainNavMesh::eReqResult
CTaskMoveGetOntoMainNavMesh::GetAreaRequestResult(const CPed * UNUSED_PARAM(pPed))
{
	Assert(m_hCalcAreaUnderfootHandle!=PATH_HANDLE_NULL);

	EPathServerRequestResult ret = CPathServer::IsRequestResultReady(m_hCalcAreaUnderfootHandle);
	if(ret == ERequest_Ready)
	{
		float fArea;
		EPathServerErrorCode errCode = CPathServer::GetAreaUnderfootSearchResultAndClear(m_hCalcAreaUnderfootHandle, fArea);
		if(errCode != PATH_NO_ERROR)
		{
			CPathServer::UnlockPathResultAndClear(m_hCalcAreaUnderfootHandle);
			m_hCalcAreaUnderfootHandle = PATH_HANDLE_NULL;
			return EQuit;
		}
		
		CPathServer::UnlockPathResultAndClear(m_hCalcAreaUnderfootHandle);
		m_hCalcAreaUnderfootHandle = PATH_HANDLE_NULL;

		m_bIsOnSmallAreaOfNavMesh = (fArea <= ms_fMaxAreaToAttempt);

		return EContinue;
	}
	else if(ret == ERequest_NotFound)
	{
		m_hCalcAreaUnderfootHandle = PATH_HANDLE_NULL;
		return EQuit;
	}

	return EStillWaiting;
}

CTask * CTaskMoveGetOntoMainNavMesh::CreateSubTask(const int iTaskType, CPed* UNUSED_PARAM(pPed))
{
	switch(iTaskType)
	{
		case CTaskTypes::TASK_MOVE_STAND_STILL:
		{
			return rage_new CTaskMoveStandStill(1000);
		}
		case CTaskTypes::TASK_MOVE_GO_TO_POINT_AND_STAND_STILL:
		{
			m_AttemptTimer.Set(fwTimer::GetTimeInMilliseconds(), m_iAttemptTime);
			CTaskMoveGoToPointAndStandStill * pGotoTask = rage_new CTaskMoveGoToPointAndStandStill(m_fMoveBlendRatio, m_vTarget, ms_fTargetRadius);
			// We don't care about the vertical height of the goto target
			pGotoTask->SetTargetHeightDelta(1000.0f);
			return pGotoTask;
		}
	}
	Assert(false);
	return NULL;
}














