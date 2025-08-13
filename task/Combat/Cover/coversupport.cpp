//
// task/Combat/Cover/coversupport.cpp
//
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved.
//

#include "task/Combat/Cover/coversupport.h"

//-----------------------------------------------------------------------------

namespace CCoverSupport
{

//-----------------------------------------------------------------------------

VecBoolV_Out IsPointInsideSphereAtOriginVectorized(Vec4V_In sphereRadiusSqV, Vec4V_In pointXV, Vec4V_In pointYV, Vec4V_In pointZV)
{
	// Compute squared distance from the point to the origin.
	const Vec4V magSqXV = Scale(pointXV, pointXV);
	const Vec4V magSqXYV = AddScaled(magSqXV, pointYV, pointYV);
	const Vec4V magSqV = AddScaled(magSqXYV, pointZV, pointZV);

	// Compute a mask for if we are within the sphere.
	const VecBoolV withinSphereV = IsLessThanOrEqual(magSqV, sphereRadiusSqV);

	return withinSphereV;
}



VecBoolV_Out IntersectionSphereToInfAxisLineVectorized(Vec4V_In sphereRadiusSqV, Vec4V_In linePosYV, Vec4V_In linePosZV, Vec4V_Ref isectX1Out, Vec4V_Ref isectX2Out)
{
	const Vec4V zeroV(V_ZERO);

	const Vec4V s1V = SubtractScaled(sphereRadiusSqV, linePosYV, linePosYV);
	const Vec4V sV = SubtractScaled(s1V, linePosZV, linePosZV);

	const VecBoolV_Out solutionExistsV = IsGreaterThanOrEqual(sV, zeroV);

	const Vec4V srootPosV = Sqrt(sV);			// Note: sV may contain negative values, in which case we expect garbage values for that component, but shouldn't crash when using vectors.
	const Vec4V srootNegV = Negate(srootPosV);

	isectX1Out = srootPosV;
	isectX2Out = srootNegV;

	return solutionExistsV;
}


VecBoolV_Out IntersectionPlaneToInfAxisLineVectorized(Vec4V_In planeNormalXV, Vec4V_In planeNormalYV, Vec4V_In planeNormalZV,
			Vec4V_In linePosYV, Vec4V_In linePosZV, Vec4V_Ref isectXOut)
{
	const Vec4V zeroV(V_ZERO);

	// Check if the plane normal is orthogonal to the line. In that case, there is no definite intersection.
	const VecBoolV isectExistsV = InvertBits(IsEqual(planeNormalXV, zeroV));

	// Compute the intersection point, if it exists.
	const Vec4V negDotYV = SubtractScaled(zeroV, planeNormalYV, linePosYV);
	const Vec4V negDotV = SubtractScaled(negDotYV, planeNormalZV, linePosZV);
	const Vec4V isectXV = InvScale(negDotV, planeNormalXV);

	// Store the intersection.
	isectXOut = isectXV;

	return isectExistsV;
}


VecBoolV_Out IsAABBEdgeInsideHemisphereVectorized(
		Vec4V_In planeNormalXV, Vec4V_In planeNormalYV, Vec4V_In planeNormalZV, Vec4V_In sphereRadiusSqV,
		Vec4V_In lineMinXV, Vec4V_In lineMaxXV, Vec4V_In lineYV, Vec4V_In lineZV)
{
	// The edge can have two types of valid intersections:
	// A. An intersection with the plane, which is also within the sphere.
	// B. An intersection with the sphere surface, which is also on the right side of the plane.

	// Find the intersection between the plane and the infinite line passing through the edge.
	Vec4V planeIsectXV;
	const VecBoolV planeIsectExistsV = IntersectionPlaneToInfAxisLineVectorized(planeNormalXV, planeNormalYV, planeNormalZV, lineYV, lineZV, planeIsectXV);

	// Find the intersections between the sphere and the infinite line passing through the edge.
	Vec4V sphX1V, sphX2V;
	const VecBoolV sphereIsectsExistV = IntersectionSphereToInfAxisLineVectorized(sphereRadiusSqV, lineYV, lineZV, sphX1V, sphX2V);

	const Vec4V zeroV(V_ZERO);

	// If we have a valid plane intersection, check if it's also within the sphere.
	const VecBoolV planeIsectIfExistsIsWithinSphereV = IsPointInsideSphereAtOriginVectorized(sphereRadiusSqV, planeIsectXV, lineYV, lineZV);

	// If we have a valid plane intersection, check if it's also on the actual edge.
	const VecBoolV planeIsectIfExistsIsOnEdgeV = And(IsGreaterThanOrEqual(planeIsectXV, lineMinXV), IsLessThanOrEqual(planeIsectXV, lineMaxXV));

	// If we have a valid plane intersection, check if it's both within the sphere and on the actual edge.
	const VecBoolV planeIsectIfExistsIsOnEdgeWithinSphereV	= And(planeIsectIfExistsIsOnEdgeV, planeIsectIfExistsIsWithinSphereV);

	// Finally, determine if the plane intersection actually existed, and is also within the sphere and on the edge. If so,
	// we have found a valid intersection with the hemisphere.
	const VecBoolV planeIsectIsOnEdgeWithinSphereV = And(planeIsectExistsV, planeIsectIfExistsIsOnEdgeWithinSphereV);

	// If we have sphere intersections, compute the dot product with the plane normal to determine which
	// side of the plane they are on.
	const Vec4V sphCommonPartsOfDotV = AddScaled(Scale(lineYV, planeNormalYV), lineZV, planeNormalZV);
	const Vec4V sphDot1V = AddScaled(sphCommonPartsOfDotV, sphX1V, planeNormalXV);
	const Vec4V sphDot2V = AddScaled(sphCommonPartsOfDotV, sphX2V, planeNormalXV);

	// If the sphere intersections exist, check if they are located on the edge.
	const VecBoolV sphereIsect1IfExistsIsOnEdgeV = And(IsGreaterThanOrEqual(sphX1V, lineMinXV), IsLessThanOrEqual(sphX1V, lineMaxXV));
	const VecBoolV sphereIsect2IfExistsIsOnEdgeV = And(IsGreaterThanOrEqual(sphX2V, lineMinXV), IsLessThanOrEqual(sphX2V, lineMaxXV));

	// If the sphere intersections exist, check if they are on the right side of the plane.
	const VecBoolV sphereIsect1IfExistsIsOnRightSideOfPlaneV = IsGreaterThan(sphDot1V, zeroV);
	const VecBoolV sphereIsect2IfExistsIsOnRightSideOfPlaneV = IsGreaterThan(sphDot2V, zeroV);

	// Determine if they were both on the right side of the plane and on the edge.
	const VecBoolV sphereIsect1IfExistsIsOnEdgeOnRightSideOfPlaneV = And(sphereIsect1IfExistsIsOnEdgeV, sphereIsect1IfExistsIsOnRightSideOfPlaneV);
	const VecBoolV sphereIsect2IfExistsIsOnEdgeOnRightSideOfPlaneV = And(sphereIsect2IfExistsIsOnEdgeV, sphereIsect2IfExistsIsOnRightSideOfPlaneV);

	// Finally, determine if those sphere intersections actually existed to begin with.
	const VecBoolV sphereIsect1IsOnEdgeOnRightSideOfPlaneV = And(sphereIsectsExistV, sphereIsect1IfExistsIsOnEdgeOnRightSideOfPlaneV);
	const VecBoolV sphereIsect2IsOnEdgeOnRightSideOfPlaneV = And(sphereIsectsExistV, sphereIsect2IfExistsIsOnEdgeOnRightSideOfPlaneV);

	// Determine if we either found a valid sphere intersection or a valid plane intersection, satisfying all the conditions.
	const VecBoolV gotValidSphereIsectV = Or(sphereIsect1IsOnEdgeOnRightSideOfPlaneV, sphereIsect2IsOnEdgeOnRightSideOfPlaneV);
	const VecBoolV gotValidPlaneOrSphereIsect = Or(gotValidSphereIsectV, planeIsectIsOnEdgeWithinSphereV);

	return gotValidPlaneOrSphereIsect;
}


VecBoolV_Out IsPointInsideHemisphereAtOriginVectorized(
		Vec4V_In planeNormalXV, Vec4V_In planeNormalYV, Vec4V_In planeNormalZV, Vec4V_In hemisphereRadiusSqV,
		Vec4V_In pointXV, Vec4V_In pointYV, Vec4V_In pointZV)
{
	const Vec4V zeroV(V_ZERO);

	// Compute squared distance from the point to the origin.
	const Vec4V magSqXV = Scale(pointXV, pointXV);
	const Vec4V magSqXYV = AddScaled(magSqXV, pointYV, pointYV);
	const Vec4V magSqV = AddScaled(magSqXYV, pointZV, pointZV);

	// Compute dot product with the plane normal, to determine which side it's on.
	const Vec4V dotXV = Scale(planeNormalXV, pointXV);
	const Vec4V dotXYV = AddScaled(dotXV, planeNormalYV, pointYV);
	const Vec4V dotV = AddScaled(dotXYV, planeNormalZV, pointZV);

	// Compute masks for if we are within the sphere and on the positive side of the plane.
	const VecBoolV withinSphereV = IsLessThanOrEqual(magSqV, hemisphereRadiusSqV);
	const VecBoolV onPosSideOfPlaneV = IsGreaterThanOrEqual(dotV, zeroV);

	// Compute mask for the result, we are in the hemisphere if we are both within the sphere
	// and on the right side of the plane.
	const VecBoolV withinHemisphereV = And(withinSphereV, onPosSideOfPlaneV);

	return withinHemisphereV;
}


VecBoolV_Out DoesHemisphereTouchBoxFaceVectorized(Vec4V_In planeNormalXV, Vec4V_In planeNormalYV, Vec4V_In planeNormalZV,
		Vec4V_In sphereRadiusSqV,
		Vec4V_In faceMinXV, Vec4V_In faceMaxXV, Vec4V_In faceMinYV, Vec4V_In faceMaxYV, Vec4V_In faceZV)
{
	// Generate some constants that we'll need.
	const Vec4V zeroV(V_ZERO);
	const Vec4V twoV(V_TWO);
	const Vec4V fourV(V_FOUR);

	// Compute some products we will need further down.
	const Vec4V planeNormalSqXV = Scale(planeNormalXV, planeNormalXV);
	const Vec4V planeNormalSqYV = Scale(planeNormalYV, planeNormalYV);
	const Vec4V dotZV = Scale(faceZV, planeNormalZV);
	const Vec4V faceZSqV = Scale(faceZV, faceZV);

	// Find the square of the radius of the intersecting circle between the
	// face plane and the sphere. If this number is less than zero, it means
	// that the sphere doesn't touch the plane, and there can be no hemisphere intersection.
	const Vec4V circleRadiusSqV = Subtract(sphereRadiusSqV, faceZSqV);
	const VecBoolV intersectionCircleExistsV = IsGreaterThanOrEqual(circleRadiusSqV, zeroV);

	// Compute the closest distance between (0, 0) (the center of the circle) on the face's plane,
	// and the box face. If this distance is greater than the radius of the intersecting circle,
	// we know that the sphere doesn't touch the box face at all, and there can be no intersection.
	const Vec4V distFromFaceXV = Max(Max(-faceMaxXV, faceMinXV), zeroV);
	const Vec4V distFromFaceYV = Max(Max(-faceMaxYV, faceMinYV), zeroV);
	const Vec4V distFaceToCircleCenterSqV = AddScaled(Scale(distFromFaceXV, distFromFaceXV), distFromFaceYV, distFromFaceYV);
	const VecBoolV circleIsNotOutsideOfBoxFaceV = IsGreaterThanOrEqual(Subtract(circleRadiusSqV, distFaceToCircleCenterSqV), zeroV);
	const VecBoolV intersectionMayExistV = And(intersectionCircleExistsV, circleIsNotOutsideOfBoxFaceV);

	// Check if (0, 0, faceZ) is a point on the box face, and if the plane is oriented
	// towards it. If so, we have found an intersection.
	const VecBoolV centerWithinFaceXV = And(IsGreaterThanOrEqual(faceMaxXV, zeroV), IsLessThanOrEqual(faceMinXV, zeroV));
	const VecBoolV centerWithinFaceYV = And(IsGreaterThanOrEqual(faceMaxYV, zeroV), IsLessThanOrEqual(faceMinYV, zeroV));
	const VecBoolV normalPointingTowardsCenterV = IsGreaterThanOrEqual(dotZV, zeroV);
	const VecBoolV centerPointIsIntersectionV = And(And(centerWithinFaceXV, centerWithinFaceYV), normalPointingTowardsCenterV);

	// We will look at the intersection points between the intersecting circle,
	// and the plane. Looking at it in two dimensions, we will need to solve something
	// like this:
	//	x^2 + y^2 + R = 0
	//  Nx*x + Ny*y + W = 0
	// where Nx and Ny are the X and Y components of the plane normal. We can do this
	// for example by changing the line equation into y = (-Nx*x - W)/Ny, and then putting
	// that into the circle equation, and solving the quadratic equation for x. However,
	// this only works if Ny is non-zero. Otherwise, if Nx is non-zero, we can do the same
	// for the other variable instead. For numeric robustness, we are probably best off picking
	// the largest normal component. Everything else can be considered a swap of the X and Y dimensions.
	// So, determine if we should swap or not:
	const VecBoolV shouldSwapXYV = IsGreaterThanOrEqual(planeNormalSqXV, planeNormalSqYV);

	// Compute the normals and face min/max points in X and Y, after the potential swap.
	const Vec4V testPlaneNormalXV = SelectFT(shouldSwapXYV, planeNormalXV, planeNormalYV);
	const Vec4V testPlaneNormalYV = SelectFT(shouldSwapXYV, planeNormalYV, planeNormalXV);
	const Vec4V testPlaneNormalYSqV = SelectFT(shouldSwapXYV, planeNormalSqYV, planeNormalSqXV);
	const Vec4V testFaceMinXV = SelectFT(shouldSwapXYV, faceMinXV, faceMinYV);
	const Vec4V testFaceMinYV = SelectFT(shouldSwapXYV, faceMinYV, faceMinXV);
	const Vec4V testFaceMaxXV = SelectFT(shouldSwapXYV, faceMaxXV, faceMaxYV);
	const Vec4V testFaceMaxYV = SelectFT(shouldSwapXYV, faceMaxYV, faceMaxXV);

	// Set up for a quadratic equation a*x^2 + b*x + c = 0, for finding
	// the intersections between the circle and the line (the plane's intersection with the face).
	const Vec4V aV = Add(planeNormalSqXV, planeNormalSqYV);
	const Vec4V bV = Scale(Scale(twoV, testPlaneNormalXV), dotZV);
	const Vec4V cV = SubtractScaled(Scale(dotZV, dotZV), testPlaneNormalYSqV, circleRadiusSqV);

	// Prepare for solving the quadratic equation by making sure that a is not 0,
	// and that we won't use the square root of a negative number.
	const Vec4V sV = SubtractScaled(Scale(bV, bV), fourV, Scale(aV, cV));
	const VecBoolV gotDivV = InvertBits(IsEqual(testPlaneNormalYV, zeroV));
	const VecBoolV gotRootsV = And(IsGreaterThanOrEqual(sV, zeroV), gotDivV);

	// Compute the roots, if they exist.
	const Vec4V aaV = Scale(twoV, aV);
	const Vec4V invAAV = Invert(aaV);
	const Vec4V srootV = Sqrt(sV);
	const Vec4V negBV = Negate(bV);
	const Vec4V x1V = Scale(Add(negBV, srootV), invAAV);
	const Vec4V x2V = Scale(Subtract(negBV, srootV), invAAV);

	// Compute the other dimension's coordinates.
	const Vec4V invDivV = Invert(testPlaneNormalYV);
	const Vec4V negDotZV = Negate(dotZV);
	const Vec4V y1V = Scale(SubtractScaled(negDotZV, testPlaneNormalXV, x1V), invDivV);
	const Vec4V y2V = Scale(SubtractScaled(negDotZV, testPlaneNormalXV, x2V), invDivV);

	// Check if either of the intersection points are actually on the box face.
	const VecBoolV isect1WithinFaceIfExistsXV = And(IsGreaterThanOrEqual(testFaceMaxXV, x1V), IsLessThanOrEqual(testFaceMinXV, x1V));
	const VecBoolV isect1WithinFaceIfExistsYV = And(IsGreaterThanOrEqual(testFaceMaxYV, y1V), IsLessThanOrEqual(testFaceMinYV, y1V));
	const VecBoolV isect2WithinFaceIfExistsXV = And(IsGreaterThanOrEqual(testFaceMaxXV, x2V), IsLessThanOrEqual(testFaceMinXV, x2V));
	const VecBoolV isect2WithinFaceIfExistsYV = And(IsGreaterThanOrEqual(testFaceMaxYV, y2V), IsLessThanOrEqual(testFaceMinYV, y2V));
	const VecBoolV isect1WithinFaceIfExistsV = And(isect1WithinFaceIfExistsXV, isect1WithinFaceIfExistsYV);
	const VecBoolV isect2WithinFaceIfExistsV = And(isect2WithinFaceIfExistsXV, isect2WithinFaceIfExistsYV);
	const VecBoolV isect1WithinFaceV = And(isect1WithinFaceIfExistsV, gotRootsV);
	const VecBoolV isect2WithinFaceV = And(isect2WithinFaceIfExistsV, gotRootsV);
	const VecBoolV gotIsectsOnFaceV = Or(isect1WithinFaceV, isect2WithinFaceV);

	// Finally, figure out if we got any valid intersections on this face.
	const VecBoolV gotIsectsV = And(intersectionMayExistV, Or(centerPointIsIntersectionV, gotIsectsOnFaceV));

	return gotIsectsV;
}



bool HemisphereAtOriginIntersectsAABB(
		ScalarV_In hemisphereRadiusV, Vec3V_In planeNormalV,
		Vec3V_In boxMinV, Vec3V_In boxMaxV)
{
	// Prepare some local vector variables that will be used from a number of places.
	const ScalarV hemisphereRadiusSqV = Scale(hemisphereRadiusV, hemisphereRadiusV);
	const ScalarV zeroV(V_ZERO);

	// As an initial quick rejection, compute the distance between the sphere center
	// (the origin) and the closest point on the box. If that's greater than the
	// radius of the hemisphere, there can be no intersection.
	// Note: this check is not required for the rest of the function to work, it's here
	// purely as a quick way to reject many cases.
	const Vec3V distFromBoxV = Max(Max(-boxMaxV, boxMinV), (Vec3V)zeroV);
	const ScalarV distSqFromBoxV = Dot(distFromBoxV, distFromBoxV);
	if(IsGreaterThanAll(distSqFromBoxV, hemisphereRadiusSqV))
	{
		return false;
	}

	// Check if all corners of the box are on the negative side of the plane, i.e. where
	// there couldn't possibly be any intersection with the hemisphere. Instead of testing
	// eight corners, we do this by flipping the normal around the X, Y, and Z axes so that
	// each component is positive, and at the same time flipping the components of the box
	// bounding box. That essentially gives us the closest corner to the plane, and then
	// we just check that.
	// Note: like the distance rejection above, this test should not be at all required
	// for correctness, it's purely here as an optimization since it may be a common case.
	const VecBoolV flipNormalsToMakePosV = IsLessThan(planeNormalV, (Vec3V)zeroV);
	const Vec3V flippedNormalV = SelectFT(flipNormalsToMakePosV, planeNormalV, Negate(planeNormalV));
	const Vec3V flippedBoxMaxV = SelectFT(flipNormalsToMakePosV, boxMaxV, Negate(boxMinV));
	const ScalarV dotFlippedBoxToNormalV = Dot(flippedNormalV, flippedBoxMaxV);
	if(IsLessThanAll(dotFlippedBoxToNormalV, zeroV))
	{
		return false;
	}

	// Check if the center point of the sphere (i.e. the origin in this function's coordinate system)
	// is within the box. If so, we've found an intersection and can stop.
	const VecBoolV sphereWithinBoxV = And(IsGreaterThanOrEqual(boxMaxV, (Vec3V)zeroV), IsLessThanOrEqual(boxMinV, (Vec3V)zeroV));
	if(IsTrueXYZ(sphereWithinBoxV))
	{
		// The hemisphere's center is inside the box.
		return true;
	}

	// Next, we will test each of the eight corners of the box against the hemisphere.
	// We will do this four at a time using IsPointInsideHemisphereAtOriginVectorized(),
	// so prepare some variables:
	const ScalarV planeNormalXV = ScalarV(planeNormalV.GetX());
	const ScalarV planeNormalYV = ScalarV(planeNormalV.GetY());
	const ScalarV planeNormalZV = ScalarV(planeNormalV.GetZ());
	const ScalarV boxMinXV = ScalarV(boxMinV.GetX());
	const ScalarV boxMaxXV = ScalarV(boxMaxV.GetX());
	const ScalarV boxMinYV = ScalarV(boxMinV.GetY());
	const ScalarV boxMaxYV = ScalarV(boxMaxV.GetY());
	const ScalarV boxMinZV = ScalarV(boxMinV.GetZ());
	const ScalarV boxMaxZV = ScalarV(boxMaxV.GetZ());
	const Vec4V corners12X(boxMinXV, boxMaxXV, boxMinXV, boxMaxXV);
	const Vec4V corners12Y(boxMinYV, boxMinYV, boxMaxYV, boxMaxYV);
	const Vec4V corners1Z(boxMinZV);
	const Vec4V corners2Z(boxMaxZV);

	// Do the tests.
	const VecBoolV corners1InsideV = IsPointInsideHemisphereAtOriginVectorized((Vec4V)planeNormalXV, (Vec4V)planeNormalYV, (Vec4V)planeNormalZV, (Vec4V)hemisphereRadiusSqV, corners12X, corners12Y, corners1Z);
	const VecBoolV corners2InsideV = IsPointInsideHemisphereAtOriginVectorized((Vec4V)planeNormalXV, (Vec4V)planeNormalYV, (Vec4V)planeNormalZV, (Vec4V)hemisphereRadiusSqV, corners12X, corners12Y, corners2Z);

	// Combine the test results. If at least one of the corners was inside, we have found an intersection and can return.
	const VecBoolV cornersInsideV = Or(corners1InsideV, corners2InsideV);
	if(!IsFalseAll(cornersInsideV))
	{
		return true;
	}

	// This is how we would have done the tests above without using the vectorized function:
	//	if(IsPointInsideHemisphereAtOrigin(planeNormalX, planeNormalY, planeNormalZ, hemisphereRadiusSq, boxMinX, boxMinY, boxMinZ)) { return true; }
	//	if(IsPointInsideHemisphereAtOrigin(planeNormalX, planeNormalY, planeNormalZ, hemisphereRadiusSq, boxMaxX, boxMinY, boxMinZ)) { return true; }
	//	if(IsPointInsideHemisphereAtOrigin(planeNormalX, planeNormalY, planeNormalZ, hemisphereRadiusSq, boxMinX, boxMaxY, boxMinZ)) { return true; }
	//	if(IsPointInsideHemisphereAtOrigin(planeNormalX, planeNormalY, planeNormalZ, hemisphereRadiusSq, boxMaxX, boxMaxY, boxMinZ)) { return true; }
	//	if(IsPointInsideHemisphereAtOrigin(planeNormalX, planeNormalY, planeNormalZ, hemisphereRadiusSq, boxMinX, boxMinY, boxMaxZ)) { return true; }
	//	if(IsPointInsideHemisphereAtOrigin(planeNormalX, planeNormalY, planeNormalZ, hemisphereRadiusSq, boxMaxX, boxMinY, boxMaxZ)) { return true; }
	//	if(IsPointInsideHemisphereAtOrigin(planeNormalX, planeNormalY, planeNormalZ, hemisphereRadiusSq, boxMinX, boxMaxY, boxMaxZ)) { return true; }
	//	if(IsPointInsideHemisphereAtOrigin(planeNormalX, planeNormalY, planeNormalZ, hemisphereRadiusSq, boxMaxX, boxMaxY, boxMaxZ)) { return true; }

	// If no corner was inside the hemisphere, we will test the twelve edges of the box
	// and see if any of them intersect the hemisphere.
	// Prepare input parameters:
	const Vec4V lineMin1XV(boxMinXV);
	const Vec4V lineMax1XV(boxMaxXV);
	const Vec4V lineY1V(boxMinYV, boxMaxYV, boxMinYV, boxMaxYV);
	const Vec4V lineZ1V(boxMinZV, boxMinZV, boxMaxZV, boxMaxZV);
	const Vec4V lineMin2XV(boxMinYV);
	const Vec4V lineMax2XV(boxMaxYV);
	const Vec4V lineY2V(boxMinZV, boxMaxZV, boxMinZV, boxMaxZV);
	const Vec4V lineZ2V(boxMinXV, boxMinXV, boxMaxXV, boxMaxXV);
	const Vec4V lineMin3XV(boxMinZV);
	const Vec4V lineMax3XV(boxMaxZV);
	const Vec4V lineY3V(boxMinXV, boxMaxXV, boxMinXV, boxMaxXV);
	const Vec4V lineZ3V(boxMinYV, boxMinYV, boxMaxYV, boxMaxYV);

	// Carry out the tests. Like above, we use a vectorized function that can do four tests
	// in parallel, so these three calls test all the twelve edges.
	const VecBoolV gotIsectsOnEdges1V = IsAABBEdgeInsideHemisphereVectorized((Vec4V)planeNormalXV, (Vec4V)planeNormalYV, (Vec4V)planeNormalZV, (Vec4V)hemisphereRadiusSqV, lineMin1XV, lineMax1XV, lineY1V, lineZ1V);
	if(!IsFalseAll(gotIsectsOnEdges1V))
	{
		return true;
	}
	const VecBoolV gotIsectsOnEdges2V = IsAABBEdgeInsideHemisphereVectorized((Vec4V)planeNormalYV, (Vec4V)planeNormalZV, (Vec4V)planeNormalXV, (Vec4V)hemisphereRadiusSqV, lineMin2XV, lineMax2XV, lineY2V, lineZ2V);
	if(!IsFalseAll(gotIsectsOnEdges2V))
	{
		return true;
	}
	const VecBoolV gotIsectsOnEdges3V = IsAABBEdgeInsideHemisphereVectorized((Vec4V)planeNormalZV, (Vec4V)planeNormalXV, (Vec4V)planeNormalYV, (Vec4V)hemisphereRadiusSqV, lineMin3XV, lineMax3XV, lineY3V, lineZ3V);
	if(!IsFalseAll(gotIsectsOnEdges3V))
	{
		return true;
	}
	
	// Note: we could also have done it like this, if we want to reduce the number of vector comparisons.
	// It's not clear which way is faster.
	//	const VecBoolV gotIsectsOnEdgesV = Or(gotIsectsOnEdges1V, Or(gotIsectsOnEdges2V, gotIsectsOnEdges3V));
	//	if(!IsFalseAll(gotIsectsOnEdgesV))
	//	{
	//		return true;
	//	}

	// The tests above are equivalent to this, if we had not used the vectorized function:
	//	if(IsAABBEdgeInsideHemisphere(planeNormalX, planeNormalY, planeNormalZ, hemisphereRadiusSq, boxMinX, boxMaxX, boxMinY, boxMinZ)) { return true; }
	//	if(IsAABBEdgeInsideHemisphere(planeNormalX, planeNormalY, planeNormalZ, hemisphereRadiusSq, boxMinX, boxMaxX, boxMaxY, boxMinZ)) { return true; }
	//	if(IsAABBEdgeInsideHemisphere(planeNormalX, planeNormalY, planeNormalZ, hemisphereRadiusSq, boxMinX, boxMaxX, boxMinY, boxMaxZ)) { return true; }
	//	if(IsAABBEdgeInsideHemisphere(planeNormalX, planeNormalY, planeNormalZ, hemisphereRadiusSq, boxMinX, boxMaxX, boxMaxY, boxMaxZ)) { return true; }
	//	if(IsAABBEdgeInsideHemisphere(planeNormalY, planeNormalZ, planeNormalX, hemisphereRadiusSq, boxMinY, boxMaxY, boxMinZ, boxMinX)) { return true; }
	//	if(IsAABBEdgeInsideHemisphere(planeNormalY, planeNormalZ, planeNormalX, hemisphereRadiusSq, boxMinY, boxMaxY, boxMaxZ, boxMinX)) { return true; }
	//	if(IsAABBEdgeInsideHemisphere(planeNormalY, planeNormalZ, planeNormalX, hemisphereRadiusSq, boxMinY, boxMaxY, boxMinZ, boxMaxX)) { return true; }
	//	if(IsAABBEdgeInsideHemisphere(planeNormalY, planeNormalZ, planeNormalX, hemisphereRadiusSq, boxMinY, boxMaxY, boxMaxZ, boxMaxX)) { return true; }
	//	if(IsAABBEdgeInsideHemisphere(planeNormalZ, planeNormalX, planeNormalY, hemisphereRadiusSq, boxMinZ, boxMaxZ, boxMinX, boxMinY)) { return true; }
	//	if(IsAABBEdgeInsideHemisphere(planeNormalZ, planeNormalX, planeNormalY, hemisphereRadiusSq, boxMinZ, boxMaxZ, boxMaxX, boxMinY)) { return true; }
	//	if(IsAABBEdgeInsideHemisphere(planeNormalZ, planeNormalX, planeNormalY, hemisphereRadiusSq, boxMinZ, boxMaxZ, boxMinX, boxMaxY)) { return true; }
	//	if(IsAABBEdgeInsideHemisphere(planeNormalZ, planeNormalX, planeNormalY, hemisphereRadiusSq, boxMinZ, boxMaxZ, boxMaxX, boxMaxY)) { return true; }

	// The last case we need to worry about is an intersection between the hemisphere and one
	// of the surfaces of the box. Like before, we use a vectorized function which can test
	// four faces at a time. Since we have six faces, we need two calls (where we only care
	// about two components of the last call).
	// Prepare input vectors:
	const Vec4V planeNormalXYV(planeNormalXV, planeNormalXV, planeNormalYV, planeNormalYV);
	const Vec4V planeNormalYZV(planeNormalYV, planeNormalYV, planeNormalZV, planeNormalZV);
	const Vec4V planeNormalZXV(planeNormalZV, planeNormalZV, planeNormalXV, planeNormalXV);
	const Vec4V boxMinXYV(boxMinXV, boxMinXV, boxMinYV, boxMinYV);
	const Vec4V boxMaxXYV(boxMaxXV, boxMaxXV, boxMaxYV, boxMaxYV);
	const Vec4V boxMinYZV(boxMinYV, boxMinYV, boxMinZV, boxMinZV);
	const Vec4V boxMaxYZV(boxMaxYV, boxMaxYV, boxMaxZV, boxMaxZV);
	const Vec4V boxMinMaxZX(boxMinZV, boxMaxZV, boxMinXV, boxMaxXV);
	const Vec4V boxMinMaxYV(boxMinYV, boxMaxYV, boxMinYV, boxMaxYV);

	// Check the first four faces.
	VecBoolV facesTouchBoxFaceXYV = DoesHemisphereTouchBoxFaceVectorized(planeNormalXYV, planeNormalYZV, planeNormalZXV, (Vec4V)hemisphereRadiusSqV, boxMinXYV, boxMaxXYV, boxMinYZV, boxMaxYZV, boxMinMaxZX);
	if(!IsFalseAll(facesTouchBoxFaceXYV))
	{
		return true;
	}
	// Check the last two faces (repeated twice to fill up the four vector components).
	VecBoolV facesTouchBoxFaceZZV = DoesHemisphereTouchBoxFaceVectorized((Vec4V)planeNormalZV, (Vec4V)planeNormalXV, (Vec4V)planeNormalYV, (Vec4V)hemisphereRadiusSqV, (Vec4V)boxMinZV, (Vec4V)boxMaxZV, (Vec4V)boxMinXV, (Vec4V)boxMaxXV, boxMinMaxYV);
	if(!IsFalseAll(facesTouchBoxFaceZZV))
	{
		return true;
	}

	// Non-vectorized version of these tests:
	//	if(DoesHemisphereTouchBoxFace(planeNormalX, planeNormalY, planeNormalZ, hemisphereRadiusSq, boxMinX, boxMaxX, boxMinY, boxMaxY, boxMinZ)) { return true; }
	//	if(DoesHemisphereTouchBoxFace(planeNormalX, planeNormalY, planeNormalZ, hemisphereRadiusSq, boxMinX, boxMaxX, boxMinY, boxMaxY, boxMaxZ)) { return true; }
	//	if(DoesHemisphereTouchBoxFace(planeNormalY, planeNormalZ, planeNormalX, hemisphereRadiusSq, boxMinY, boxMaxY, boxMinZ, boxMaxZ, boxMinX)) { return true; }
	//	if(DoesHemisphereTouchBoxFace(planeNormalY, planeNormalZ, planeNormalX, hemisphereRadiusSq, boxMinY, boxMaxY, boxMinZ, boxMaxZ, boxMaxX)) { return true; }
	//	if(DoesHemisphereTouchBoxFace(planeNormalZ, planeNormalX, planeNormalY, hemisphereRadiusSq, boxMinZ, boxMaxZ, boxMinX, boxMaxX, boxMinY)) { return true; }
	//	if(DoesHemisphereTouchBoxFace(planeNormalZ, planeNormalX, planeNormalY, hemisphereRadiusSq, boxMinZ, boxMaxZ, boxMinX, boxMaxX, boxMaxY)) { return true; }

	// If none of the tests above found an intersection, it should be impossible for one to exist,
	// so we can return false.
	return false;
}


bool HemisphereIntersectsAABB(Vec3V_In hemisphereCenterV, ScalarV_In hemisphereRadiusV, Vec3V_In planeNormalV, Vec3V_In boxMinWorldV, Vec3V_In boxMaxWorldV)
{
	// Translate the box so that the hemisphere's center is at the origin.
	const Vec3V boxMinV = Subtract(boxMinWorldV, hemisphereCenterV);
	const Vec3V boxMaxV = Subtract(boxMaxWorldV, hemisphereCenterV);
	return HemisphereAtOriginIntersectsAABB(hemisphereRadiusV, planeNormalV, boxMinV, boxMaxV);
}


bool SphereIntersectsAABB(Vec3V_In sphereCenterV, ScalarV_In sphereRadiusV,
		Vec3V_In boxMinWorldV, Vec3V_In boxMaxWorldV)
{
	// Translate the box so that the sphere's center is at the origin.
	const Vec3V boxMinV = Subtract(boxMinWorldV, sphereCenterV);
	const Vec3V boxMaxV = Subtract(boxMaxWorldV, sphereCenterV);

	const ScalarV sphereRadiusSqV = Scale(sphereRadiusV, sphereRadiusV);
	const ScalarV zeroV(V_ZERO);

	// Compute the distance between the sphere center and the closest point on the box.
	// If that's greater than the radius of the sphere, there can be no intersection.
	const Vec3V distFromBoxV = Max(Max(-boxMaxV, boxMinV), (Vec3V)zeroV);
	const ScalarV distSqFromBoxV = Dot(distFromBoxV, distFromBoxV);
	if(IsGreaterThanAll(distSqFromBoxV, sphereRadiusSqV))
	{
		return false;
	}

	return true;
}

//-----------------------------------------------------------------------------

// Various code such as non-vectorized hemisphere tests in different stages, and
// some work towards cone/AABB intersections.
#if 0

int g_DrawShift = 0;

static void sDrawSphere(float posX, float posY, float posZ, Color32 col)
{
	Vec3V posV;
	if(g_DrawShift == 0)
	{
		posV = Vec3V(posX, posY, posZ);
	}
	else if(g_DrawShift == 1)
	{
		posV = Vec3V(posY, posZ, posX);
	}
	else
	{
		posV = Vec3V(posZ, posX, posY);
	}

	posV = Transform(s_ViewMtrx, posV);
	grcDebugDraw::Sphere(posV, 0.2f, col, true);
}

static void sDrawCircle(float posX, float posY, float posZ, float radius)
{
	Vec3V posV;
	Vec3V axis1, axis2;
	if(g_DrawShift == 0)
	{
		posV = Vec3V(posX, posY, posZ);
		axis1 = Vec3V(V_X_AXIS_WZERO);
		axis2 = Vec3V(V_Y_AXIS_WZERO);
	}
	else if(g_DrawShift == 1)
	{
		posV = Vec3V(posY, posZ, posX);
		axis1 = Vec3V(V_Z_AXIS_WZERO);
		axis2 = Vec3V(V_X_AXIS_WZERO);
	}
	else
	{
		posV = Vec3V(posZ, posX, posY);
		axis1 = Vec3V(V_Y_AXIS_WZERO);
		axis2 = Vec3V(V_Z_AXIS_WZERO);
	}

	posV = Transform(s_ViewMtrx, posV);
	axis1 = Transform3x3(s_ViewMtrx, axis1);
	axis2 = Transform3x3(s_ViewMtrx, axis2);
	grcDebugDraw::Circle(posV, radius, Color_purple, axis1, axis2, false, false);
}



int IntersectionConeToInfAxisLine(const float coneDirX, const float coneDirY, const float coneDirZ,
		const float coneCosAlphaSq, const float linePosY, const float linePosZ, float& x1out, float& x2out)
{
	const float a = coneDirX*coneDirX - coneCosAlphaSq;
	const float b = 2.0f*(coneDirY*coneDirX*linePosY + coneDirX*coneDirZ*linePosZ);
	const float c1 = coneDirY*coneDirY*linePosY*linePosY;
	const float c2 = coneDirZ*coneDirZ*linePosZ*linePosZ;
//	const float c3 = 2.0f*coneDirY*coneDirX*linePosY*linePosZ;
	const float c3 = 2.0f*coneDirY*coneDirZ*linePosY*linePosZ;
	const float c4 = -coneCosAlphaSq*linePosY*linePosY;
	const float c5 = -coneCosAlphaSq*linePosZ*linePosZ;
	const float c = c1 + c2 + c3 + c4 + c5;

	// TODO: May need to reject solutions on the negative side

	// a*x^2 + b*x + c = 0
	if(a == 0)
	{

		if(b == 0)
		{
			// No solution, or the whole line? Not sure if we need to do anything else.
			return 0;
		}
		else
		{
			// b*x + c = 0
			const float x = -c/b;
			x1out = x;
			x2out = x;
			return 1;
		}
	}
	else
	{
		const float s = b*b - 4*a*c;
		if(s >= 0.0f)
		{
			const float aa = 2*a;
			const float sroot = sqrtf(s);
			const float x1 = (-b+sroot)/aa;
			const float x2 = (-b-sroot)/aa;
			// Note: currently these can probably be in any order, should we sort them?
			x1out = x1;
			x2out = x2;
			return 2;
		}
		else
		{
			return 0;
		}
	}
}



bool IntersectionSphereToInfAxisLine(const float sphereRadiusSq, const float linePosY, const float linePosZ, float& x1out, float& x2out)
{
	const float s = sphereRadiusSq - linePosY*linePosY - linePosZ*linePosZ;
	if(s >= 0.0f)
	{
		const float sroot = sqrtf(s);
		x1out = sroot;
		x2out = -sroot;
		return true;
	}
	return false;
}


bool IsPointInsideConeAtOrigin(const float coneDirX, const float coneDirY, const float coneDirZ,
		const float coneAngleCosSq, const float coneLengthSq,
		const float pointX, const float pointY, const float pointZ)
{
	const float magSq = pointX*pointX + pointY*pointY + pointZ*pointZ;
	if(magSq >= coneLengthSq)
	{
		// Too far away.
		return false;
	}

	// Note: this could be extended to handle >90 degree angles fairly easily

	const float dot = coneDirX*pointX + coneDirY*pointY + coneDirZ*pointZ;

	if(dot < 0.0f)
	{
		return false;
	}

	const float dotSq = dot*dot;
	if(dotSq < coneAngleCosSq*magSq)
	{
		return false;
	}

	//sDrawSphere(pointX, pointY, pointZ, Color_black);

	return true;
}

bool IsPointInsideInfiniteConeAtOrigin(const float coneDirX, const float coneDirY, const float coneDirZ,
		const float coneAngleCosSq,
		const float pointX, const float pointY, const float pointZ)
{
	const float magSq = pointX*pointX + pointY*pointY + pointZ*pointZ;
	const float dot = coneDirX*pointX + coneDirY*pointY + coneDirZ*pointZ;

	if(dot < 0.0f)
	{
		return false;
	}

	const float dotSq = dot*dot;
	if(dotSq < coneAngleCosSq*magSq)
	{
		return false;
	}

	return true;
}

bool IsPointInsideSphereAtOrigin(const float sphereRadiusSq, const float pointX, const float pointY, const float pointZ)
{
	const float magSq = pointX*pointX + pointY*pointY + pointZ*pointZ;
	if(magSq >= sphereRadiusSq)
	{
		// Too far away.
		return false;
	}
	return true;
}


bool IsAABBEdgeInsideConeYZ(const float coneDirX, const float coneDirY, const float coneDirZ,
		const float coneAngleCosSq, const float coneLengthSq,
		const float lineMinX, const float lineMaxX, const float lineY, const float lineZ)
{
	float x1, x2;

	int numIsects = IntersectionConeToInfAxisLine(coneDirX, coneDirY, coneDirZ, coneAngleCosSq, lineY, lineZ, x1, x2);

	if(numIsects)
	{
		const float dot1 = x1*coneDirX + lineY*coneDirY + lineZ*coneDirZ;
		const float dot2 = x2*coneDirX + lineY*coneDirY + lineZ*coneDirZ;

		// Intersections with infinite line:
		// (x1, lineY, lineZ)
		// (x2, lineY, lineZ)

		// Note: we could possibly take more advantage of knowing that x1..x2 is within the cone,
		// clamp it against the box edge range, and test those points against the sphere as if the
		// intersections had been on the box edge in the first place.

		//if(dot1 >= 0.0f && x1 >= lineMinX && x1 <= lineMaxX)
		//{
		//	if(IsPointInsideSphereAtOrigin(coneLengthSq, x1, lineY, lineZ))
		//	{
		//		sDrawSphere(x1, lineY, lineZ, Color_yellow);
		//	}
		//}
		//if(dot2 >= 0.0f && x2 >= lineMinX && x2 <= lineMaxX)
		//{
		//	if(IsPointInsideSphereAtOrigin(coneLengthSq, x2, lineY, lineZ))
		//	{
		//		sDrawSphere(x2, lineY, lineZ, Color_yellow);
		//	}
		//}

		if(dot1 >= 0.0f && x1 >= lineMinX && x1 <= lineMaxX)
		{
			// Point 1 is on the box edge, is it also inside the sphere?
			if(IsPointInsideSphereAtOrigin(coneLengthSq, x1, lineY, lineZ))
			{
				return true;
			}
		}
		if(dot2 >= 0.0f && x2 >= lineMinX && x2 <= lineMaxX)
		{
			// Point 2 is on the box edge, is it also inside the sphere?
			if(IsPointInsideSphereAtOrigin(coneLengthSq, x2, lineY, lineZ))
			{
				return true;
			}
		}

		if(x1 < lineMinX && x2 < lineMinX)
		{
			// Both cone intersections are outside of the box edge.
			return false;
		}

		if(x1 > lineMaxX && x2 < lineMaxX)
		{
			// Both cone intersections are outside of the box edge.
			return false;
		}

		float sphX1, sphX2;
		bool gotSphereIsects = IntersectionSphereToInfAxisLine(coneLengthSq, lineY, lineZ, sphX1, sphX2);
		if(gotSphereIsects)
		{

			//if(sphX1 >= lineMinX && sphX1 <= lineMaxX)
			//{
			//	// Sphere intersection 1 is on the box edge, is it also inside the infinite cone?
			//	if(IsPointInsideInfiniteConeAtOrigin(coneDirX, coneDirY, coneDirZ, coneAngleCosSq, sphX1, lineY, lineZ))
			//	{
			//		sDrawSphere(sphX1, lineY, lineZ, Color_blue);
			//	}
			//}
			//if(sphX2 >= lineMinX && sphX2 <= lineMaxX)
			//{
			//	// Sphere intersection 2 is on the box edge, is it also inside the infinite cone?
			//	if(IsPointInsideInfiniteConeAtOrigin(coneDirX, coneDirY, coneDirZ, coneAngleCosSq, sphX2, lineY, lineZ))
			//	{
			//		sDrawSphere(sphX2, lineY, lineZ, Color_blue);
			//	}
			//}


			if(sphX1 >= lineMinX && sphX1 <= lineMaxX)
			{
				// Sphere intersection 1 is on the box edge, is it also inside the infinite cone?
				if(IsPointInsideInfiniteConeAtOrigin(coneDirX, coneDirY, coneDirZ, coneAngleCosSq, sphX1, lineY, lineZ))
				{
					return true;
				}
			}
			if(sphX2 >= lineMinX && sphX2 <= lineMaxX)
			{
				// Sphere intersection 2 is on the box edge, is it also inside the infinite cone?
				if(IsPointInsideInfiniteConeAtOrigin(coneDirX, coneDirY, coneDirZ, coneAngleCosSq, sphX2, lineY, lineZ))
				{
					return true;
				}
			}
		}

		return false;
	}
	else
	{
		// If there are no intersections with the infinite cone, there can't be any with the
		// truncated one either.
		// Note: possible exception if the whole line is parallel with the side of the cone or
		// something.
		return false;
	}

	// 

}


bool DoesConeTouchBoxFace(const float coneDirX, const float coneDirY, const float coneDirZ,
		const float coneAngleCos, const float /*coneAngleSin*/, const float coneLengthSq,
		const float faceMinX, float faceMaxX, float faceMinY, float faceMaxY, float faceZ)
{
#if 0
	if(fabsf(faceZ) > coneLengthSq)
	{
		// The plane is too far away, there is no intersection with the sphere.
		return false;
	}
#endif
	const float circleRadiusSq = coneLengthSq - faceZ*faceZ;
	if(circleRadiusSq < 0.0f)
	{
		return false;
	}

	const float coneAngleCosSq = coneAngleCos*coneAngleCos;

	const float faceCenterX = 0.5f*(faceMinX + faceMaxX);
	const float faceCenterY = 0.5f*(faceMinY + faceMaxY);
	const float faceHalfSizeX = 0.5f*(faceMaxX - faceMinX);
	const float faceHalfSizeY = 0.5f*(faceMaxY - faceMinY);
	const float dx = Max(fabsf(faceCenterX) - faceHalfSizeX, 0.0f);
	const float dy = Max(fabsf(faceCenterY) - faceHalfSizeY, 0.0f);

	const float distFaceToCircleSq = square(dx) + square(dy);
	if(distFaceToCircleSq > circleRadiusSq)
	{
		return false;
	}

do
{
	const float circleRadius = sqrtf(circleRadiusSq);
	sDrawCircle(0.0f, 0.0f, faceZ, circleRadius);
} while(0);

	if(coneDirZ != 0.0f)
	{
		const float t = faceZ/coneDirZ;
		if(t >= 0.0f)
		{
			const float coneCenterX = t*coneDirX;
			const float coneCenterY = t*coneDirY;

			if(t*t <= coneLengthSq)
			{
				if(coneCenterX >= faceMinX && coneCenterX <= faceMaxX && coneCenterY >= faceMinY && coneCenterY <= faceMaxY)
				{
					//sDrawSphere(coneCenterX, coneCenterY, faceZ, Color_green);

					return true;
				}
			}

			const float circleCenterToConeCenterX = coneCenterX;
			const float circleCenterToConeCenterY = coneCenterY;
			const float distToConeCenter = sqrtf(circleCenterToConeCenterX*circleCenterToConeCenterX + circleCenterToConeCenterY*circleCenterToConeCenterY);

			if(distToConeCenter > 0.0f)
			{
				const float circleRadius = sqrtf(circleRadiusSq);
				const float scale = Min(distToConeCenter, circleRadius)/distToConeCenter;
				const float posToTestX = circleCenterToConeCenterX*scale;
				const float posToTestY = circleCenterToConeCenterY*scale;
				const float posToTestZ = faceZ;

				if(posToTestX >= faceMinX && posToTestX <= faceMaxX && posToTestY >= faceMinY && posToTestY <= faceMaxY)
				{
					const float dot = posToTestX*coneDirX + posToTestY*coneDirY + posToTestZ*coneDirZ;
					const float magSq = posToTestX*posToTestX + posToTestY*posToTestY + posToTestZ*posToTestZ;

					if(dot*dot >= coneAngleCosSq*magSq)
					{
						//sDrawSphere(posToTestX, posToTestY, faceZ, Color_white);

						return true;
					}
					else
					{
						//sDrawSphere(posToTestX, posToTestY, faceZ, Color_black);
					}
				}
			}
		}

// TODO: Possible test: find closest point on cone section, check if within sphere

	}

#if 0
	if(coneDirZ != 0.0f)
	{
		const float t = faceZ/coneDirZ;
		if(t >= 0.0f && t*t <= coneLengthSq)
		{
			const float coneCenterX = t*coneDirX;
			const float coneCenterY = t*coneDirY;
			if(coneCenterX >= faceMinX && coneCenterX <= faceMaxX && coneCenterY >= faceMinY && coneCenterY <= faceMaxY)
			{
				//sDrawSphere(coneCenterX, coneCenterY, faceZ, Color_green);

				return true;
			}
		}
	}

	const float coneDirSide1X = coneAngleCos*coneDirX - coneAngleSin*coneDirY;
	const float coneDirSide1Y = coneAngleSin*coneDirX + coneAngleCos*coneDirY;
	const float coneDirSide1Z = coneDirZ;

	const float coneDirSide2X = coneAngleCos*coneDirX + coneAngleSin*coneDirY;
	const float coneDirSide2Y = -coneAngleSin*coneDirX + coneAngleCos*coneDirY;
	const float coneDirSide2Z = coneDirZ;

	const float coneDirSide3X = coneDirX;
	const float coneDirSide3Y = coneAngleCos*coneDirY - coneAngleSin*coneDirZ;
	const float coneDirSide3Z = coneAngleSin*coneDirY + coneAngleCos*coneDirZ;

	const float coneDirSide4X = coneDirX;
	const float coneDirSide4Y = coneAngleCos*coneDirY + coneAngleSin*coneDirZ;
	const float coneDirSide4Z = -coneAngleSin*coneDirY + coneAngleCos*coneDirZ;

	float approxRectMinX = faceMinX;
	float approxRectMaxX = faceMaxX;
	float approxRectMinY = faceMinY;
	float approxRectMaxY = faceMaxY;

	//if(coneDirSide1Z != 0.0f)
	//{
	//	const float t = faceZ/coneDirSide1Z;
	//	if(t >= 0.0f)
	//	{
	//		const float x = t*coneDirSide1X;
	//		const float y = t*coneDirSide1Y;
	//		sDrawSphere(x, y, faceZ, Color_grey);
	//	}
	//}
	//if(coneDirSide2Z != 0.0f)
	//{
	//	const float t = faceZ/coneDirSide2Z;
	//	if(t >= 0.0f)
	//	{
	//		const float x = t*coneDirSide2X;
	//		const float y = t*coneDirSide2Y;
	//		sDrawSphere(x, y, faceZ, Color_red);
	//	}
	//}
	//if(coneDirSide3Z != 0.0f)
	//{
	//	const float t = faceZ/coneDirSide3Z;
	//	if(t >= 0.0f)
	//	{
	//		const float x = t*coneDirSide3X;
	//		const float y = t*coneDirSide3Y;
	//		sDrawSphere(x, y, faceZ, Color_white);
	//	}
	//}
	//if(coneDirSide4Z != 0.0f)
	//{
	//	const float t = faceZ/coneDirSide4Z;
	//	if(t >= 0.0f)
	//	{
	//		const float x = t*coneDirSide4X;
	//		const float y = t*coneDirSide4Y;
	//		sDrawSphere(x, y, faceZ, Color_black);
	//	}
	//}
#endif


#if 0
		bool boxOutsideSphere = false;

		const float boxCenterZ = 0.5f*(pGridCell->m_vMins.z + pGridCell->m_vMaxs.z);

		const float boxHalfSizeX = 0.5f*(pGridCell->m_vMaxs.x - pGridCell->m_vMins.x);
		const float boxHalfSizeY = 0.5f*(pGridCell->m_vMaxs.y - pGridCell->m_vMins.y);
		const float boxHalfSizeZ = 0.5f*(pGridCell->m_vMaxs.z - pGridCell->m_vMins.z);

		const float dx = Max(fabsf(vPos.x - boxCenterX) - boxHalfSizeX, 0.0f);
		const float dy = Max(fabsf(vPos.y - boxCenterY) - boxHalfSizeY, 0.0f);
		const float dz = Max(fabsf(vPos.z - boxCenterZ) - boxHalfSizeZ, 0.0f);

		const float distToBoxSq = square(dx) + square(dy) + square(dz);
		if(distToBoxSq >= square(fMaxDistance))
		{
			boxOutsideSphere = true;
		}

#endif




	// In this case, the intersection between the line and the sphere is a circle:
	// defined by
	//	x*x + y*y + faceZ*faceZ - coneLengthSq = 0

//	const float circleRadius = sqrtf(circleRadiusSq);
//	sDrawCircle(0.0f, 0.0f, faceZ, circleRadius);

// Maybe not true:
#if 0
	// This circle must either be completely outside of the face, or the whole circle must be on it
	// (otherwise, we should have found an intersection with an edge).
	// So, we should be able to just test one of the points on the circle, pick y=0 and compute x^2:
	const float xSq = sphereRadiusSq - faceZ*faceZ;
	const float x = sqrtf(xSq);
	if(x < faceMinX || x > faceMaxX)
	{
		// The whole box face must be outside of the sphere.
		return false;
	}
	
//	bool IsPointInsideInfiniteConeAtOrigin(const float coneDirX, const float coneDirY, const float coneDirZ,
//			const float coneAngleCosSq,
//			const float pointX, const float pointY, const float pointZ)
//	{

	// The intersecting circle could still be either completely inside the cone or completely
	// outside it
#endif

	// cone intersection:
	//	(coneDirX*x + coneDirY*y + coneDirZ*z)/sqrtf(x*x + y*y + z*z) <= sqrtf(coneAngleCosSq)
	//	(coneDirX*x + coneDirY*y + coneDirZ*z)^2 <= (coneAngleCosSq)*(x*x + y*y + z*z)
	

	// TODO - solve this?

	return false;
}



float IntersectionSphereToInfAxisLineBranchless(const float sphereRadiusSq, const float linePosY, const float linePosZ, float& x1out, float& x2out)
{
	const float s = sphereRadiusSq - linePosY*linePosY - linePosZ*linePosZ;
	const float solutionExists = Selectf(s, 1.0f, -1.0f);

	const float sroot = sqrtf(Max(s, 0.0f));	// Max() to protect against negative number sqrt - probably not needed for vector pipeline.

	x1out = sroot;
	x2out = -sroot;

	return solutionExists;
}



float IsPointInsideSphereAtOriginBranchless(const float sphereRadiusSq, const float pointX, const float pointY, const float pointZ)
{
	// Compute squared distance from the point to the origin.
	const float magSq = pointX*pointX + pointY*pointY + pointZ*pointZ;

	// Compute a mask for if we are within the sphere.
	const float withinSphere = Selectf(sphereRadiusSq - magSq, 1.0f, -1.0f);

	return withinSphere;
}


// TEMP
bool IsPointInsideHemisphereAtOriginBranchless(const float planeNormalX, const float planeNormalY, const float planeNormalZ,
		const float hemisphereRadiusSq,
		const float pointX, const float pointY, const float pointZ)
{
	// Compute squared distance from the point to the origin.
	const float magSq = pointX*pointX + pointY*pointY + pointZ*pointZ;

	// Compute dot product with the plane normal, to determine which side it's on.
	const float dot = planeNormalX*pointX + planeNormalY*pointY + planeNormalZ*pointZ;

	// Compute masks for if we are within the sphere and on the positive side of the plane.
	const float withinSphere = Selectf(hemisphereRadiusSq - magSq, 1.0f, -1.0f);
	const float onPosSideOfPlane = Selectf(dot, 1.0f, -1.0f);

	// Compute mask for the result, we are in the hemisphere if we are both within the sphere
	// and on the right side of the plane.
	const float withinHemisphere = Selectf(withinSphere, onPosSideOfPlane, -1.0f);

	return withinHemisphere >= 0.0f;
}


float IntersectionPlaneToInfAxisLineBranchless(const float planeNormalX, const float planeNormalY, const float planeNormalZ,
			const float linePosY, const float linePosZ, float& xOut)
{
	// Check if the plane normal is orthogonal to the line. In that case, there is no definite intersection.
	const float isectExistsPos = Selectf(-planeNormalX, -1.0f, 1.0f);
	const float isectExistsNeg = Selectf(planeNormalX, -1.0f, 1.0f);
	const float isectExists = Selectf(isectExistsPos, isectExistsNeg, -1.0f);

	// If there is an intersection, we will compute it by dividing by planeNormalX, otherwise we don't
	// care about the result and can divide by anything non-zero.
	// Note: if converted to use vectors, we can probably skip this.
	const float div = Selectf(isectExists, planeNormalX, 1.0f);

	// Compute the intersection point, if it exists.
	const float isectX = (-planeNormalY*linePosY - planeNormalZ*linePosZ)/div;

	// Store the intersection.
	xOut = isectX;

	return isectExists >= 0.0f;
}


bool IsAABBEdgeInsideHemisphereBranchless(const float planeNormalX, const float planeNormalY, const float planeNormalZ,
		const float sphereRadiusSq,
		const float lineMinX, const float lineMaxX, const float lineY, const float lineZ)
{
	// The edge can have two types of valid intersections:
	// A. An intersection with the plane, which is also within the sphere.
	// B. An intersection with the sphere surface, which is also on the right side of the plane.

	// Find the intersection between the plane and the infinite line passing through the edge.
	float planeIsectX;
	const float planeIsectExists = IntersectionPlaneToInfAxisLineBranchless(planeNormalX, planeNormalY, planeNormalZ, lineY, lineZ, planeIsectX);

	// Find the intersections between the sphere and the infinite line passing through the edge.
	float sphX1, sphX2;
	const float sphereIsectsExist = IntersectionSphereToInfAxisLineBranchless(sphereRadiusSq, lineY, lineZ, sphX1, sphX2);

	// If we have a valid plane intersection, check if it's also within the sphere.
	const float planeIsectIfExistsIsWithinSphere = IsPointInsideSphereAtOriginBranchless(sphereRadiusSq, planeIsectX, lineY, lineZ);

	// If we have a valid plane intersection, check if it's also on the actual edge.
	const float planeIsectIfExistsIsOnEdge = Selectf(planeIsectX - lineMinX, Selectf(lineMaxX - planeIsectX, 1.0f, -1.0f), -1.0f);

	// If we have a valid plane intersection, check if it's both within the sphere and on the actual edge.
	const float planeIsectIfExistsIsOnEdgeWithinSphere	= Selectf(planeIsectIfExistsIsOnEdge, planeIsectIfExistsIsWithinSphere, -1.0f);

	// Finally, determine if the plane intersection actually existed, and is also within the sphere and on the edge. If so,
	// we have found a valid intersection with the hemisphere.
	const float planeIsectIsOnEdgeWithinSphere = Selectf(planeIsectExists, planeIsectIfExistsIsOnEdgeWithinSphere, -1.0f);

	// If we have sphere intersections, compute the dot product with the plane normal to determine which
	// side of the plane they are on.
	const float sphCommonPartsOfDot = lineY*planeNormalY + lineZ*planeNormalZ;
	const float sphDot1 = sphX1*planeNormalX + sphCommonPartsOfDot;
	const float sphDot2 = sphX2*planeNormalX + sphCommonPartsOfDot;

	// If the sphere intersections exist, check if they are located on the edge.
	const float sphereIsect1IfExistsIsOnEdge = Selectf(sphX1 - lineMinX, Selectf(lineMaxX - sphX1, 1.0f, -1.0f), -1.0f);
	const float sphereIsect2IfExistsIsOnEdge = Selectf(sphX2 - lineMinX, Selectf(lineMaxX - sphX2, 1.0f, -1.0f), -1.0f);

	// If the sphere intersections exist, check if they are on the right side of the plane.
	const float sphereIsect1IfExistsIsOnRightSideOfPlane = Selectf(sphDot1, 1.0f, -1.0f);
	const float sphereIsect2IfExistsIsOnRightSideOfPlane = Selectf(sphDot2, 1.0f, -1.0f);

	// Determine if they were both on the right side of the plane and on the edge.
	const float sphereIsect1IfExistsIsOnEdgeOnRightSideOfPlane = Selectf(sphereIsect1IfExistsIsOnEdge, sphereIsect1IfExistsIsOnRightSideOfPlane, -1.0f);
	const float sphereIsect2IfExistsIsOnEdgeOnRightSideOfPlane = Selectf(sphereIsect2IfExistsIsOnEdge, sphereIsect2IfExistsIsOnRightSideOfPlane, -1.0f);

	// Finally, determine if those sphere intersections actually existed to begin with.
	const float sphereIsect1IsOnEdgeOnRightSideOfPlane = Selectf(sphereIsectsExist, sphereIsect1IfExistsIsOnEdgeOnRightSideOfPlane, -1.0f);
	const float sphereIsect2IsOnEdgeOnRightSideOfPlane = Selectf(sphereIsectsExist, sphereIsect2IfExistsIsOnEdgeOnRightSideOfPlane, -1.0f);

	// Determine if we either found a valid sphere intersection or a valid plane intersection, satisfying all the conditions.
	const float gotValidSphereIsect = Selectf(sphereIsect1IsOnEdgeOnRightSideOfPlane, 1.0f, sphereIsect2IsOnEdgeOnRightSideOfPlane);
	const float gotValidPlaneOrSphereIsect = Selectf(gotValidSphereIsect, 1.0f, planeIsectIsOnEdgeWithinSphere);

	return gotValidPlaneOrSphereIsect >= 0.0f;
}


bool IsPointInsideHemisphereAtOrigin(const float planeNormalX, const float planeNormalY, const float planeNormalZ,
		const float hemisphereRadiusSq,
		const float pointX, const float pointY, const float pointZ)
{
	// Compute squared distance from the point to the origin.
	const float magSq = pointX*pointX + pointY*pointY + pointZ*pointZ;
	if(magSq >= hemisphereRadiusSq)
	{
		// Too far away.
		return false;
	}

	// Compute dot product with the plane normal, to determine which side it's on.
	const float dot = planeNormalX*pointX + planeNormalY*pointY + planeNormalZ*pointZ;

	if(dot < 0.0f)
	{
		return false;
	}

	//sDrawSphere(pointX, pointY, pointZ, Color_black);

	return true;
}




bool IntersectionPlaneToInfAxisLine(const float planeNormalX, const float planeNormalY, const float planeNormalZ,
			const float linePosY, const float linePosZ, float& xOut)
{
	// Check if the plane normal is orthogonal to the line. In that case, there is no definite intersection.
	if(planeNormalX == 0.0f)
	{
		return false;
	}
	
	// Compute the intersection point.
	const float x = (-planeNormalY*linePosY - planeNormalZ*linePosZ)/planeNormalX;

	// Store the intersection.
	xOut = x;

	return true;
}




bool IsAABBEdgeInsideHemisphere(const float planeNormalX, const float planeNormalY, const float planeNormalZ,
		const float sphereRadiusSq,
		const float lineMinX, const float lineMaxX, const float lineY, const float lineZ)
{
	float x;

	if(IntersectionPlaneToInfAxisLine(planeNormalX, planeNormalY, planeNormalZ, lineY, lineZ, x))
	{
//		const float dot = x*planeNormalX + lineY*planeNormalY + lineZ*planeNormalZ;

		// Note: we could possibly take more advantage of knowing that x1..x2 is within the cone,
		// clamp it against the box edge range, and test those points against the sphere as if the
		// intersections had been on the box edge in the first place.

		//if(x >= lineMinX && x <= lineMaxX)
		//{
		//	if(IsPointInsideSphereAtOrigin(sphereRadiusSq, x, lineY, lineZ))
		//	{
		//		sDrawSphere(x, lineY, lineZ, Color_yellow);
		//	}
		//}

		if(x >= lineMinX && x <= lineMaxX)
		{
			if(IsPointInsideSphereAtOrigin(sphereRadiusSq, x, lineY, lineZ))
			{
				return true;
			}
		}
	}

	float sphX1, sphX2;
	bool gotSphereIsects = IntersectionSphereToInfAxisLine(sphereRadiusSq, lineY, lineZ, sphX1, sphX2);
	if(gotSphereIsects)
	{
		//if(sphX1 >= lineMinX && sphX1 <= lineMaxX)
		//{
		//	// Sphere intersection 1 is on the box edge, is it also on the right side of the plane?
		//	if(sphX1*planeNormalX + lineY*planeNormalY + lineZ*planeNormalZ >= 0.0f)
		//	{
		//		sDrawSphere(sphX1, lineY, lineZ, Color_blue);
		//	}
		//}
		//if(sphX2 >= lineMinX && sphX2 <= lineMaxX)
		//{
		//	// Sphere intersection 2 is on the box edge, is it also inside the infinite cone?
		//	if(sphX2*planeNormalX + lineY*planeNormalY + lineZ*planeNormalZ >= 0.0f)
		//	{
		//		sDrawSphere(sphX2, lineY, lineZ, Color_blue);
		//	}
		//}


		if(sphX1 >= lineMinX && sphX1 <= lineMaxX)
		{
			// Sphere intersection 1 is on the box edge, is it also on the right side of the plane?
			if(sphX1*planeNormalX + lineY*planeNormalY + lineZ*planeNormalZ >= 0.0f)
			{
				return true;
			}
		}
		if(sphX2 >= lineMinX && sphX2 <= lineMaxX)
		{
			// Sphere intersection 2 is on the box edge, is it also inside the infinite cone?
			if(sphX2*planeNormalX + lineY*planeNormalY + lineZ*planeNormalZ >= 0.0f)
			{
				return true;
			}
		}
	}

	// Note: might be some edge case with parallel lines here

	return false;

}



bool DoesHemisphereTouchBoxFaceBranchless(const float planeNormalX, const float planeNormalY, const float planeNormalZ,
		const float sphereRadiusSq,
		const float faceMinX, float faceMaxX, float faceMinY, float faceMaxY, float faceZ)
{
	// Compute some products we will need further down.
	const float planeNormalSqX = planeNormalX*planeNormalX;
	const float planeNormalSqY = planeNormalY*planeNormalY;
	const float dotZ = faceZ*planeNormalZ;
	const float faceZSq = faceZ*faceZ;

	// Find the square of the radius of the intersecting circle between the
	// face plane and the sphere. If this number is less than zero, it means
	// that the sphere doesn't touch the plane, and there can be no hemisphere intersection.
	const float circleRadiusSq = sphereRadiusSq - faceZSq;
	const float intersectionCircleExists = Selectf(circleRadiusSq, 1.0f, -1.0f);

	// Compute the closest distance between (0, 0) (the center of the circle) on the face's plane,
	// and the box face. If this distance is greater than the radius of the intersecting circle,
	// we know that the sphere doesn't touch the box face at all, and there can be no intersection.
	const float distFromFaceX = Max(Max(-faceMaxX, faceMinX), 0.0f);
	const float distFromFaceY = Max(Max(-faceMaxY, faceMinY), 0.0f);
	const float distFaceToCircleCenterSq = square(distFromFaceX) + square(distFromFaceY);
	const float circleIsNotOutsideOfBoxFace = Selectf(circleRadiusSq - distFaceToCircleCenterSq, 1.0f, -1.0f);
	const float intersectionMayExist = Selectf(intersectionCircleExists, circleIsNotOutsideOfBoxFace, -1.0f);

	// Check if (0, 0, faceZ) is a point on the box face, and if the plane is oriented
	// towards it. If so, we have found an intersection.
	const float centerWithinFaceX = Selectf(faceMaxX, Selectf(-faceMinX, 1.0f, -1.0f), -1.0f);
	const float centerWithinFaceY = Selectf(faceMaxY, Selectf(-faceMinY, 1.0f, -1.0f), -1.0f);
	const float normalPointingTowardsCenter = Selectf(dotZ, 1.0f, -1.0f);
	const float centerPointIsIntersection = Selectf(normalPointingTowardsCenter, Selectf(centerWithinFaceX, centerWithinFaceY, -1.0f), -1.0f);

	// We will look at the intersection points between the intersecting circle,
	// and the plane. Looking at it in two dimensions, we will need to solve something
	// like this:
	//	x^2 + y^2 + R = 0
	//  Nx*x + Ny*y + W = 0
	// where Nx and Ny are the X and Y components of the plane normal. We can do this
	// for example by changing the line equation into y = (-Nx*x - W)/Ny, and then putting
	// that into the circle equation, and solving the quadratic equation for x. However,
	// this only works if Ny is non-zero. Otherwise, if Nx is non-zero, we can do the same
	// for the other variable instead. For numeric robustness, we are probably best off picking
	// the largest normal component. Everything else can be considered a swap of the X and Y dimensions.
	// So, determine if we should swap or not:
	const float shouldSwapXY = Selectf(planeNormalSqX - planeNormalSqY, 1.0f, -1.0f);

	// Compute the normals and face min/max points in X and Y, after the potential swap.
	const float testPlaneNormalX = Selectf(shouldSwapXY, planeNormalY, planeNormalX);
	const float testPlaneNormalY = Selectf(shouldSwapXY, planeNormalX, planeNormalY);
	const float testPlaneNormalYSq = Selectf(shouldSwapXY, planeNormalSqX, planeNormalSqY);
	const float testFaceMinX = Selectf(shouldSwapXY, faceMinY, faceMinX);
	const float testFaceMinY = Selectf(shouldSwapXY, faceMinX, faceMinY);
	const float testFaceMaxX = Selectf(shouldSwapXY, faceMaxY, faceMaxX);
	const float testFaceMaxY = Selectf(shouldSwapXY, faceMaxX, faceMaxY);

	// Set up for a quadratic equation a*x^2 + b*x + c = 0, for finding
	// the intersections between the circle and the line (the plane's intersection with the face).
	const float a = planeNormalSqX + planeNormalSqY;
	const float b = 2.0f*testPlaneNormalX*dotZ;
	const float c = dotZ*dotZ - testPlaneNormalYSq*circleRadiusSq;

	// Prepare for solving the quadratic equation by making sure that a is not 0,
	// and that we won't use the square root of a negative number.
	// gotDiv = testPlaneNormalY != 0.0f:
	const float s = b*b - 4.0f*a*c;
	const float gotDiv = Selectf(-testPlaneNormalY, Selectf(testPlaneNormalY, -1.0f, 1.0f), 1.0f);
	const float gotRoots = Selectf(s, gotDiv, -1.0f);
	const float div = Selectf(gotDiv, testPlaneNormalY, 1.0f);	// Shouldn't be needed when using vector pipeline.
	const float asafe = Selectf(gotDiv, a, 1.0f);				// Shouldn't be needed when using vector pipeline.
	const float sclamped = Max(s, 0.0f);						// Shouldn't be needed when using vector pipeline.

	// Compute the roots, if they exist.
	const float aa = 2.0f*asafe;
	const float invAA = 1.0f/aa;
	const float sroot = sqrtf(sclamped);
	const float x1 = (-b + sroot)*invAA;
	const float x2 = (-b - sroot)*invAA;

	// Compute the other dimension's coordinates.
	const float invDiv = 1.0f/div;
	const float y1 = (-testPlaneNormalX*x1 - dotZ)*invDiv;
	const float y2 = (-testPlaneNormalX*x2 - dotZ)*invDiv;

	// Check if either of the intersection points are actually on the box face.
	const float isect1WithinFaceIfExistsX = Selectf(testFaceMaxX - x1, Selectf(x1 - testFaceMinX, 1.0f, -1.0f), -1.0f);
	const float isect1WithinFaceIfExistsY = Selectf(testFaceMaxY - y1, Selectf(y1 - testFaceMinY, 1.0f, -1.0f), -1.0f);
	const float isect2WithinFaceIfExistsX = Selectf(testFaceMaxX - x2, Selectf(x2 - testFaceMinX, 1.0f, -1.0f), -1.0f);
	const float isect2WithinFaceIfExistsY = Selectf(testFaceMaxY - y2, Selectf(y2 - testFaceMinY, 1.0f, -1.0f), -1.0f);
	const float isect1WithinFaceIfExists = Selectf(isect1WithinFaceIfExistsX, isect1WithinFaceIfExistsY, -1.0f);
	const float isect2WithinFaceIfExists = Selectf(isect2WithinFaceIfExistsX, isect2WithinFaceIfExistsY, -1.0f);
	const float isect1WithinFace = Selectf(isect1WithinFaceIfExists, gotRoots, -1.0f);
	const float isect2WithinFace = Selectf(isect2WithinFaceIfExists, gotRoots, -1.0f);
	const float gotIsectsOnFace = Selectf(isect1WithinFace, 1.0f, isect2WithinFace);

	// Finally, figure out if we got any valid intersections on this face.
	const float gotIsects = Selectf(intersectionMayExist, Selectf(centerPointIsIntersection, 1.0f, gotIsectsOnFace), -1.0f);

	return gotIsects >= 0.0f;
}



bool DoesHemisphereTouchBoxFace(const float planeNormalX, const float planeNormalY, const float planeNormalZ,
		const float sphereRadiusSq,
		const float faceMinX, float faceMaxX, float faceMinY, float faceMaxY, float faceZ)
{
	const float faceZSq = faceZ*faceZ;

	// Find the square of the radius of the intersecting circle between the
	// face plane and the sphere. If this number is less than zero, it means
	// that the sphere doesn't touch the plane, and there can be no hemisphere intersection.
	const float circleRadiusSq = sphereRadiusSq - faceZSq;
	if(circleRadiusSq < 0.0f)
	{
		return false;
	}

	// Compute the closest distance between (0, 0) (the center of the circle) on the face's plane,
	// and the box face. If this distance is greater than the radius of the intersecting circle,
	// we know that the sphere doesn't touch the box face at all.
	const float distFromFaceX = Max(Max(-faceMaxX, faceMinX), 0.0f);
	const float distFromFaceY = Max(Max(-faceMaxY, faceMinY), 0.0f);
	const float distFaceToCircleSq = square(distFromFaceX) + square(distFromFaceY);
	if(distFaceToCircleSq > circleRadiusSq)
	{
		return false;
	}

	// sDrawCircle(0.0f, 0.0f, faceZ, sqrtf(circleRadiusSq));

	// Check if (0, 0, faceZ) is a point on the box face, and if the plane is oriented
	// towards it. If so, we have found an intersection.
	if(faceZ*planeNormalZ >= 0.0f)
	{
		if(faceMinX <= 0.0f && faceMaxX >= 0.0f && faceMinY <= 0.0f && faceMaxY >= 0.0f)
		{
			sDrawSphere(0.0f, 0.0f, faceZ, Color_black);
			return true;
		}
	}

	// Compute the squares of the X and Y components of the plane normal.
	const float planeNormalSqX = planeNormalX*planeNormalX;
	const float planeNormalSqY = planeNormalY*planeNormalY;

	// We will look at the intersection points between the intersecting circle,
	// and the plane. Looking at it in two dimensions, we will need to solve something
	// like this:
	//	x^2 + y^2 + R = 0
	//  Nx*x + Ny*y + W = 0
	// where Nx and Ny are the X and Y components of the plane normal. We can do this
	// for example by changing the line equation into y = (-Nx*x - W)/Ny, and then putting
	// that into the circle equation, and solving the quadratic equation for x. However,
	// this only works if Ny is non-zero. Otherwise, if Nx is non-zero, we can do the same
	// for the other variable instead. For numeric robustness, we are probably best off picking
	// the largest normal component. Everything else can be considered a swap of the X and Y dimensions.
	// So, determine if we should swap or not:
	const float shouldSwapXY = Selectf(planeNormalSqX - planeNormalSqY, 1.0f, -1.0f);

	// Compute the normals and face min/max points in X and Y, after the potential swap.
	const float testPlaneNormalX = Selectf(shouldSwapXY, planeNormalY, planeNormalX);
	const float testPlaneNormalY = Selectf(shouldSwapXY, planeNormalX, planeNormalY);
	const float testPlaneNormalYSq = Selectf(shouldSwapXY, planeNormalSqX, planeNormalSqY);
	const float testFaceMinX = Selectf(shouldSwapXY, faceMinY, faceMinX);
	const float testFaceMinY = Selectf(shouldSwapXY, faceMinX, faceMinY);
	const float testFaceMaxX = Selectf(shouldSwapXY, faceMaxY, faceMaxX);
	const float testFaceMaxY = Selectf(shouldSwapXY, faceMaxX, faceMaxY);

	// Now, check to make sure we have something we can divide with.
	if(testPlaneNormalY != 0.0f)
	{
		// Set up for a quadratic equation a*x^2 + b*x + c = 0, for finding
		// the intersections between the circle and the line (the plane's intersection with the face).
		const float a = planeNormalSqX + planeNormalSqY;
		const float b = 2.0f*testPlaneNormalX*planeNormalZ*faceZ;
		const float c = planeNormalZ*planeNormalZ*faceZSq - testPlaneNormalYSq*circleRadiusSq;

		// Should always have an actual quadratic equation if we got here:
		Assert(a != 0.0f);

		// Check if there is a solution.
		const float s = b*b - 4*a*c;
		if(s >= 0.0f)
		{
			// Compute the roots.
			const float aa = 2.0f*a;
			const float sroot = sqrtf(s);
			const float x1 = (-b+sroot)/aa;
			const float x2 = (-b-sroot)/aa;

			// Compute the other coordinates.
			const float y1 = (-testPlaneNormalX*x1 - planeNormalZ*faceZ)/testPlaneNormalY;
			const float y2 = (-testPlaneNormalX*x2 - planeNormalZ*faceZ)/testPlaneNormalY;

			//if(x1 >= testFaceMinX && x1 <= testFaceMaxX && y1 >= testFaceMinY && y1 <= testFaceMaxY)
			//{
			//	const float drawX = Selectf(shouldSwapXY, y1, x1);
			//	const float drawY = Selectf(shouldSwapXY, x1, y1);
			//	sDrawSphere(drawX, drawY, faceZ, Color_white);
			//}
			//if(x2 >= testFaceMinX && x2 <= testFaceMaxX && y2 >= testFaceMinY && y2 <= testFaceMaxY)
			//{
			//	const float drawX = Selectf(shouldSwapXY, y2, x2);
			//	const float drawY = Selectf(shouldSwapXY, x2, y2);
			//	sDrawSphere(drawX, drawY, faceZ, Color_white);
			//}

			// Check if either of the intersection points are actually on the box face.
			if(x1 >= testFaceMinX && x1 <= testFaceMaxX && y1 >= testFaceMinY && y1 <= testFaceMaxY)
			{
				return true;
			}
			if(x2 >= testFaceMinX && x2 <= testFaceMaxX && y2 >= testFaceMinY && y2 <= testFaceMaxY)
			{
				return true;
			}
		}

	}

	return false;
}



bool ConeAtOriginIntersectsAABB(const float coneDirX, const float coneDirY, const float coneDirZ,
		const float coneAngle, const float coneLength,
		const float boxMinX, const float boxMinY, const float boxMinZ,
		const float boxMaxX, const float boxMaxY, const float boxMaxZ)
{
	// cone dir should be normalized

g_DrawShift = 0;
#if 0
	if(g_DrawShift == 0)
	{
		posV = Vec3V(posX, posY, posZ);
	}
	else if(g_DrawShift == 1)
	{
		posV = Vec3V(posY, posZ, posX);
	}
	else
	{
		posV = Vec3V(posZ, posX, posY);
	}

#endif

	Assert(coneAngle <= PI*0.5f);

bool ret = false;
#define RETURN_TRUE ret = true

	if(boxMinX <= 0.0f && boxMaxX >= 0.0f &&
			boxMinY <= 0.0f && boxMaxY >= 0.0f &&
			boxMinZ <= 0.0f && boxMaxZ >= 0.0f)
	{
		//sDrawSphere(0.0f, 0.0f, 0.0f, Color_blue);

		// Cone apex is inside the box.
		RETURN_TRUE;
	}

	const float coneLengthSq = coneLength*coneLength;

	const float coneAngleSin = sinf(coneAngle);
	const float coneAngleCos = cosf(coneAngle);
	const float coneAngleCosSq = coneAngleCos*coneAngleCos;

	// Test vertices of box against cone

	if(IsPointInsideConeAtOrigin(coneDirX, coneDirY, coneDirZ, coneAngleCosSq, coneLengthSq, boxMinX, boxMinY, boxMinZ)) { RETURN_TRUE; }
	if(IsPointInsideConeAtOrigin(coneDirX, coneDirY, coneDirZ, coneAngleCosSq, coneLengthSq, boxMaxX, boxMinY, boxMinZ)) { RETURN_TRUE; }
	if(IsPointInsideConeAtOrigin(coneDirX, coneDirY, coneDirZ, coneAngleCosSq, coneLengthSq, boxMinX, boxMaxY, boxMinZ)) { RETURN_TRUE; }
	if(IsPointInsideConeAtOrigin(coneDirX, coneDirY, coneDirZ, coneAngleCosSq, coneLengthSq, boxMaxX, boxMaxY, boxMinZ)) { RETURN_TRUE; }
	if(IsPointInsideConeAtOrigin(coneDirX, coneDirY, coneDirZ, coneAngleCosSq, coneLengthSq, boxMinX, boxMinY, boxMaxZ)) { RETURN_TRUE; }
	if(IsPointInsideConeAtOrigin(coneDirX, coneDirY, coneDirZ, coneAngleCosSq, coneLengthSq, boxMaxX, boxMinY, boxMaxZ)) { RETURN_TRUE; }
	if(IsPointInsideConeAtOrigin(coneDirX, coneDirY, coneDirZ, coneAngleCosSq, coneLengthSq, boxMinX, boxMaxY, boxMaxZ)) { RETURN_TRUE; }
	if(IsPointInsideConeAtOrigin(coneDirX, coneDirY, coneDirZ, coneAngleCosSq, coneLengthSq, boxMaxX, boxMaxY, boxMaxZ)) { RETURN_TRUE; }

	// Test edges of box against cone

#if 0
	// - edges in X direction:
g_DrawShift = 2;
	if(IsAABBEdgeInsideConeYZ(coneDirY, coneDirZ, coneDirX, coneAngleCosSq, coneLengthSq, boxMinY, boxMinZ, boxMinX, boxMaxX)) { RETURN_TRUE; }
	if(IsAABBEdgeInsideConeYZ(coneDirY, coneDirZ, coneDirX, coneAngleCosSq, coneLengthSq, boxMaxY, boxMinZ, boxMinX, boxMaxX)) { RETURN_TRUE; }
	if(IsAABBEdgeInsideConeYZ(coneDirY, coneDirZ, coneDirX, coneAngleCosSq, coneLengthSq, boxMinY, boxMaxZ, boxMinX, boxMaxX)) { RETURN_TRUE; }
	if(IsAABBEdgeInsideConeYZ(coneDirY, coneDirZ, coneDirX, coneAngleCosSq, coneLengthSq, boxMaxY, boxMaxZ, boxMinX, boxMaxX)) { RETURN_TRUE; }

	// - edges in Y direction:
g_DrawShift = 1;
	if(IsAABBEdgeInsideConeYZ(coneDirZ, coneDirX, coneDirY, coneAngleCosSq, coneLengthSq, boxMinZ, boxMinX, boxMinY, boxMaxY)) { RETURN_TRUE; }
	if(IsAABBEdgeInsideConeYZ(coneDirZ, coneDirX, coneDirY, coneAngleCosSq, coneLengthSq, boxMaxZ, boxMinX, boxMinY, boxMaxY)) { RETURN_TRUE; }
	if(IsAABBEdgeInsideConeYZ(coneDirZ, coneDirX, coneDirY, coneAngleCosSq, coneLengthSq, boxMinZ, boxMaxX, boxMinY, boxMaxY)) { RETURN_TRUE; }
	if(IsAABBEdgeInsideConeYZ(coneDirZ, coneDirX, coneDirY, coneAngleCosSq, coneLengthSq, boxMaxZ, boxMaxX, boxMinY, boxMaxY)) { RETURN_TRUE; }

	// - edges in Z direction:
g_DrawShift = 0;
	if(IsAABBEdgeInsideConeYZ(coneDirX, coneDirY, coneDirZ, coneAngleCosSq, coneLengthSq, boxMinX, boxMinY, boxMinZ, boxMaxZ)) { RETURN_TRUE; }
	if(IsAABBEdgeInsideConeYZ(coneDirX, coneDirY, coneDirZ, coneAngleCosSq, coneLengthSq, boxMaxX, boxMinY, boxMinZ, boxMaxZ)) { RETURN_TRUE; }
	if(IsAABBEdgeInsideConeYZ(coneDirX, coneDirY, coneDirZ, coneAngleCosSq, coneLengthSq, boxMinX, boxMaxY, boxMinZ, boxMaxZ)) { RETURN_TRUE; }
	if(IsAABBEdgeInsideConeYZ(coneDirX, coneDirY, coneDirZ, coneAngleCosSq, coneLengthSq, boxMaxX, boxMaxY, boxMinZ, boxMaxZ)) { RETURN_TRUE; }

g_DrawShift = 0;
#endif

g_DrawShift = 0;
	if(IsAABBEdgeInsideConeYZ(coneDirX, coneDirY, coneDirZ, coneAngleCosSq, coneLengthSq, boxMinX, boxMaxX, boxMinY, boxMinZ)) { RETURN_TRUE; }
	if(IsAABBEdgeInsideConeYZ(coneDirX, coneDirY, coneDirZ, coneAngleCosSq, coneLengthSq, boxMinX, boxMaxX, boxMaxY, boxMinZ)) { RETURN_TRUE; }
	if(IsAABBEdgeInsideConeYZ(coneDirX, coneDirY, coneDirZ, coneAngleCosSq, coneLengthSq, boxMinX, boxMaxX, boxMinY, boxMaxZ)) { RETURN_TRUE; }
	if(IsAABBEdgeInsideConeYZ(coneDirX, coneDirY, coneDirZ, coneAngleCosSq, coneLengthSq, boxMinX, boxMaxX, boxMaxY, boxMaxZ)) { RETURN_TRUE; }

g_DrawShift = 2;
	if(IsAABBEdgeInsideConeYZ(coneDirY, coneDirZ, coneDirX, coneAngleCosSq, coneLengthSq, boxMinY, boxMaxY, boxMinZ, boxMinX)) { RETURN_TRUE; }
	if(IsAABBEdgeInsideConeYZ(coneDirY, coneDirZ, coneDirX, coneAngleCosSq, coneLengthSq, boxMinY, boxMaxY, boxMaxZ, boxMinX)) { RETURN_TRUE; }
	if(IsAABBEdgeInsideConeYZ(coneDirY, coneDirZ, coneDirX, coneAngleCosSq, coneLengthSq, boxMinY, boxMaxY, boxMinZ, boxMaxX)) { RETURN_TRUE; }
	if(IsAABBEdgeInsideConeYZ(coneDirY, coneDirZ, coneDirX, coneAngleCosSq, coneLengthSq, boxMinY, boxMaxY, boxMaxZ, boxMaxX)) { RETURN_TRUE; }

g_DrawShift = 1;
	if(IsAABBEdgeInsideConeYZ(coneDirZ, coneDirX, coneDirY, coneAngleCosSq, coneLengthSq, boxMinZ, boxMaxZ, boxMinX, boxMinY)) { RETURN_TRUE; }
	if(IsAABBEdgeInsideConeYZ(coneDirZ, coneDirX, coneDirY, coneAngleCosSq, coneLengthSq, boxMinZ, boxMaxZ, boxMaxX, boxMinY)) { RETURN_TRUE; }
	if(IsAABBEdgeInsideConeYZ(coneDirZ, coneDirX, coneDirY, coneAngleCosSq, coneLengthSq, boxMinZ, boxMaxZ, boxMinX, boxMaxY)) { RETURN_TRUE; }
	if(IsAABBEdgeInsideConeYZ(coneDirZ, coneDirX, coneDirY, coneAngleCosSq, coneLengthSq, boxMinZ, boxMaxZ, boxMaxX, boxMaxY)) { RETURN_TRUE; }
g_DrawShift = 0;



	// Test faces of box against cone
	// Note: shouldn't have to test all of them, probably just the ones that face towards the origin.
g_DrawShift = 0;
	if(DoesConeTouchBoxFace(coneDirX, coneDirY, coneDirZ, coneAngleCos, coneAngleSin, coneLengthSq, boxMinX, boxMaxX, boxMinY, boxMaxY, boxMinZ)) { RETURN_TRUE; }
	if(DoesConeTouchBoxFace(coneDirX, coneDirY, coneDirZ, coneAngleCos, coneAngleSin, coneLengthSq, boxMinX, boxMaxX, boxMinY, boxMaxY, boxMaxZ)) { RETURN_TRUE; }
g_DrawShift = 2;
	if(DoesConeTouchBoxFace(coneDirY, coneDirZ, coneDirX, coneAngleCos, coneAngleSin, coneLengthSq, boxMinY, boxMaxY, boxMinZ, boxMaxZ, boxMinX)) { RETURN_TRUE; }
	if(DoesConeTouchBoxFace(coneDirY, coneDirZ, coneDirX, coneAngleCos, coneAngleSin, coneLengthSq, boxMinY, boxMaxY, boxMinZ, boxMaxZ, boxMaxX)) { RETURN_TRUE; }
g_DrawShift = 1;
	if(DoesConeTouchBoxFace(coneDirZ, coneDirX, coneDirY, coneAngleCos, coneAngleSin, coneLengthSq, boxMinZ, boxMaxZ, boxMinX, boxMaxX, boxMinY)) { RETURN_TRUE; }
	if(DoesConeTouchBoxFace(coneDirZ, coneDirX, coneDirY, coneAngleCos, coneAngleSin, coneLengthSq, boxMinZ, boxMaxZ, boxMinX, boxMaxX, boxMaxY)) { RETURN_TRUE; }
g_DrawShift = 0;

	return ret;
//	return false;
}


bool ConeIntersectsAABB(const float conePosX, const float conePosY, const float conePosZ,
		const float coneDirX, const float coneDirY, const float coneDirZ,
		const float coneAngle, const float coneLength,
		const float boxMinX, const float boxMinY, const float boxMinZ,
		const float boxMaxX, const float boxMaxY, const float boxMaxZ)
{
	return ConeAtOriginIntersectsAABB(coneDirX, coneDirY, coneDirZ,
		coneAngle, coneLength,
		boxMinX - conePosX, boxMinY - conePosY, boxMinZ - conePosZ,
		boxMaxX - conePosX, boxMaxY - conePosY, boxMaxZ - conePosZ);
}

#endif	// 0

// Some code for interactively testing the intersection functions:
#if 0

static Vec3V s_BoxCenter(0.0f, 3.0f, 1.0f);
static Vec3V s_BoxDim(V_TWO);
static float s_ConeYawDegrees = 40.0f;
static float s_ConePitchDegrees = 20.0f;
static float s_ConeAngleDegrees = 89.5f;
static float s_ConeLength = 1.0f;

static Vec3V s_ViewCenter(-4.0f, 74.0f, 8.0f);
static Vec3V s_ViewAngleDegrees(V_ZERO);
static Mat34V s_ViewMtrx;
static bool s_UseVectorizedTest = false;
static bool s_DrawSphere;

void TestConeIsect()
{
	static bool s_First = true;
	if(s_First)
	{
		s_First = false;

		bkBank& bk = BANKMGR.CreateBank("Cone Isect");
		bk.AddSlider("Box Center", &RC_VECTOR3(s_BoxCenter), -1000.0f, 1000.0f, 0.1f);
		bk.AddSlider("Box Dimensions", &RC_VECTOR3(s_BoxDim), 0.0f, 1000.0f, 0.1f);
		bk.AddAngle("Cone Yaw", &s_ConeYawDegrees, bkAngleType::DEGREES, -180.0f, 180.0f);
		bk.AddAngle("Cone Pitch", &s_ConePitchDegrees, bkAngleType::DEGREES, -90.0f, 90.0f);
		bk.AddAngle("Cone Angle", &s_ConeAngleDegrees, bkAngleType::DEGREES, 0.0f, 90.0f);
		bk.AddSlider("Cone Length", &s_ConeLength, 0.0f, 1000.0f, 0.1f);
		bk.AddSlider("View Center", &RC_VECTOR3(s_ViewCenter), -10000.0f, 10000.0f, 0.1f);
		bk.AddAngle("View Angle X", &RC_VECTOR3(s_ViewAngleDegrees).x, bkAngleType::DEGREES, -180.0f, 180.0f);
		bk.AddAngle("View Angle Y", &RC_VECTOR3(s_ViewAngleDegrees).y, bkAngleType::DEGREES, -180.0f, 180.0f);
		bk.AddAngle("View Angle Z", &RC_VECTOR3(s_ViewAngleDegrees).z, bkAngleType::DEGREES, -180.0f, 180.0f);
		bk.AddToggle("Draw Sphere", &s_DrawSphere);
		bk.AddToggle("Use Vectorized Test", &s_UseVectorizedTest);
	}

	Vec3V coneDir(V_Y_AXIS_WZERO);
	coneDir = RotateAboutXAxis(coneDir, ScalarV(s_ConePitchDegrees*DtoR));
	coneDir = RotateAboutZAxis(coneDir, ScalarV(s_ConeYawDegrees*DtoR));

	Mat34V mtrx;
	Vec3V viewAngle = Scale(s_ViewAngleDegrees, ScalarV(V_TO_RADIANS));
	Mat34VFromEulersXYZ(mtrx, viewAngle, s_ViewCenter);

	s_ViewMtrx = mtrx;

	const Vec3V boxCenter = s_BoxCenter;
	const Vec3V boxHalfWidths = s_BoxDim*ScalarV(V_HALF);
	const Vec3V boxMinX = Subtract(boxCenter, boxHalfWidths);
	const Vec3V boxMaxX = Add(boxCenter, boxHalfWidths);

	Color32 coneCol(0x00, 0xff, 0x00, 0x80);
	const Color32 boxCol(0xff, 0x80, 0x00, 0x80);

	if(s_ConeAngleDegrees >= 89.0f && s_ConeAngleDegrees <= 91.0f)
	{
		const Vec3V hemisphereCenterV(V_ZERO);
		const ScalarV hemisphereRadiusV(s_ConeLength);
		if(HemisphereIntersectsAABB(hemisphereCenterV,
				hemisphereRadiusV, coneDir,
				boxMinX, boxMaxX))
		{
			coneCol.Set(0xff, 0x00, 0x00, 0x80);
		}
	}
	//else if(ConeIntersectsAABB(0.0f, 0.0f, 0.0f,
	//		coneDir.GetXf(), coneDir.GetYf(), coneDir.GetZf(),
	//		s_ConeAngleDegrees*DtoR, s_ConeLength,
	//		boxMinX.GetXf(), boxMinX.GetYf(), boxMinX.GetZf(),
	//		boxMaxX.GetXf(), boxMaxX.GetYf(), boxMaxX.GetZf()))
	//{
	//	coneCol.Set(0xff, 0x00, 0x00, 0x80);
	//}


	const Vec3V coneStartView = s_ViewCenter;
	const Vec3V coneEnd = coneDir*ScalarV(s_ConeLength*cosf(s_ConeAngleDegrees*DtoR));
	const Vec3V coneEndView = Transform(mtrx, coneEnd);
	const float coneR = s_ConeLength*cosf(s_ConeAngleDegrees*DtoR)*tanf(s_ConeAngleDegrees*DtoR);
	grcDebugDraw::Cone(coneStartView, coneEndView, coneR, Color_black, false, false, 24);
	grcDebugDraw::Cone(coneStartView, coneEndView, coneR, coneCol, false, true, 24);
	if(s_DrawSphere)
	{
		grcDebugDraw::Sphere(coneStartView, s_ConeLength, Color_black, false, 1, 24);
//		grcDebugDraw::Sphere(coneStartView, s_ConeLength, coneCol, true, 1, 24);
	}

	grcDebugDraw::BoxOriented(boxMinX, boxMaxX, mtrx, Color_black, false);
	grcDebugDraw::BoxOriented(boxMinX, boxMaxX, mtrx, boxCol, true);
}

#endif	// 0

//-----------------------------------------------------------------------------

}	// namespace CCoverSupport

//-----------------------------------------------------------------------------

// End of file 'task/Combat/Cover/coversupport.cpp'
