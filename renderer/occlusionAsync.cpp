// occlusionAsync
// athomas: 07/03/2013
// Lighter Occlusion test methods for Async Jobs

#define g_auxDmaTag		(0)
#include "occlusionAsync.h"

///////////////////////////////////////////////////////////////////////////////
//  IsAABBOccludedAsync
///////////////////////////////////////////////////////////////////////////////
bool IsAABBOccludedAsync(const spdAABB& aabb, Vec3V_In camPos, int occluderPhase, const fwScanBaseInfo* scanBaseInfo, const Vec4V* hiZBuffer
						 DEV_ONLY(, u32* trivialAcceptActiveFrustum, u32* trivialAcceptMinZ, u32* trivialAcceptVisiblePixel))
{
	bool result = false;

	ScalarV area = rstTestAABBExactEdgeList(false, scanBaseInfo->m_transform[ occluderPhase ], 
		ScalarVFromF32(scanBaseInfo->m_depthRescale[ occluderPhase ]),
		hiZBuffer, camPos, aabb.GetMin(), aabb.GetMax(), ScalarV(V_ZERO)
		, scanBaseInfo->m_minMaxBounds[occluderPhase], scanBaseInfo->m_minZ[occluderPhase]
	DEV_ONLY(, trivialAcceptActiveFrustum, trivialAcceptMinZ, trivialAcceptVisiblePixel)
	);
	result =  IsGreaterThanAll( area, ScalarV(V_ZERO))==0;

	return result;
}

bool IsSphereVisibleAsync(const spdSphere& sphere, int occluderPhase, const fwScanBaseInfo* scanBaseInfo, const Vec4V* hiZBuffer,
						  const Vec3V* IcosahedronPts, u32 numIcosahedronPts, const u8* IcosahedronIndices, u32 numIcosahedronIndices)
{
	// generate a icosahedron
#define MAX_ICOSAHEDRON_INDICES	60
	Assert(numIcosahedronIndices <= MAX_ICOSAHEDRON_INDICES);
	Vec3V pointList[ MAX_ICOSAHEDRON_INDICES ];
	for (u32 i = 0;i< numIcosahedronPts ;i++)
	{
		pointList[i]=AddScaled( sphere.GetCenter(), IcosahedronPts[i], sphere.GetRadius() );
	}
	ScalarV area = rstTestModelExact( scanBaseInfo->m_transform[ occluderPhase ], 
		ScalarVFromF32(scanBaseInfo->m_depthRescale[ occluderPhase ]), 
		hiZBuffer, pointList, IcosahedronIndices,
		numIcosahedronPts, numIcosahedronIndices, ScalarV(V_ZERO));

	return IsTrue( area > ScalarV(V_ZERO));
}

bool IsConeVisibleAsync(const Mat34V& mtx, float radius, float spreadf, int occluderPhase, const fwScanBaseInfo* scanBaseInfo, const Vec4V* hiZBuffer,
					    const Vec3V* ConePts, u32 numConePts, const u8* ConeIndices, u32 numConeIndices)
{
	// generate a icosahedron
#define MAX_CONE_POINTS		8
	Assert(numConePts <= MAX_CONE_POINTS);
	Vec3V pointList[ MAX_CONE_POINTS+1 ];
	const Vec3V a = mtx.a();
	const Vec3V b = mtx.b();
	const Vec3V c = mtx.c();

	// Get location of the cap.
	const ScalarV computedRange = ScalarVFromF32(radius); 

	const ScalarV spread = ScalarVFromF32(spreadf);
	// const ScalarV halfSpread = Scale( spread, ScalarV(V_HALF));
	const Vec3V endcap = AddScaled( mtx.d(), c, -computedRange /* +halfSpread */);

	pointList[0] = mtx.d();

	const Vec3V as = a*spread;
	const Vec3V bs = b* spread;
	const Vec3V cs = c * -spread;
	for (u32 i = 0; i<numConePts; i++)
	{
		pointList[i+1]= AddScaled(AddScaled( AddScaled( endcap, as, ConePts[i].GetY()), bs, ConePts[i].GetZ() ), cs, ConePts[i].GetX());
	}

	ScalarV area = rstTestModelExact( scanBaseInfo->m_transform[ occluderPhase ], 
		ScalarVFromF32(scanBaseInfo->m_depthRescale[ occluderPhase ]), 
		hiZBuffer, pointList, ConeIndices,
		numConePts+1, numConeIndices, ScalarV(V_ZERO));

	return IsTrue( area > ScalarV(V_ZERO));
}