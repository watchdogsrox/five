#ifndef INC_LIGHTCULLING_H_
#define INC_LIGHTCULLING_H_ 

#include "grcore/viewport.h"
#include "../../renderer/Lights/LightCommon.h"
#include "../../renderer/Lights/LightSource.h"
#include "../../renderer/occlusionAsync.h"
#if !__SPU
#include "debug/Rendering/DebugLights.h"
#endif

namespace rage {
	struct fwScanBaseInfo;
}

// ----------------------------------------------------------------------------------------------- //

class fwBoxFromCone : public spdAABB
{
public:
	fwBoxFromCone(const Vector3& posn, float radius, const Vector3& dir, float angle)
	{
		const ScalarV angleV   = ScalarV(angle);
		const ScalarV radiusV  = ScalarV(radius);
		const Vec3V	  dirV	   = Clamp(RCC_VEC3V(dir), Vec3V(V_NEGONE), Vec3V(V_ONE));
		const Vec3V   angleXYZ = ArccosFast(dirV);
		const Vec3V   bbMin    = AddScaled(RCC_VEC3V(posn), Cos(Clamp(angleXYZ + Vec3V(angleV), Vec3V(V_PI_OVER_TWO), Vec3V(V_PI         ))), Vec3V(radiusV));
		const Vec3V   bbMax    = AddScaled(RCC_VEC3V(posn), Cos(Clamp(angleXYZ - Vec3V(angleV), Vec3V(V_ZERO       ), Vec3V(V_PI_OVER_TWO))), Vec3V(radiusV));

		Set(bbMin, bbMax);
	}
};

// ----------------------------------------------------------------------------------------------- //

#if __SPU
inline bool IsLightVisible( const CLightSource& lgt, Vec3V_In vCameraPos, const fwScanBaseInfo* scanBaseInfo, const Vec4V* hiZBuffer,
	const Vec3V* icosahedronPoints, u32 numIcosahedronPoints, const u8* icosahedronIndices, u32 numIcosahedronIndices,
	const Vec3V* conePoints, u32 numConePoints, const u8* coneIndices, u32 numConeIndices	
	DEV_ONLY(,u32* pNumOcclusionTests, u32* pNumObjectsOccluded, u32* trivialAcceptActiveFrustum, u32* trivialAcceptMinZ, u32* trivialAcceptVisiblePixel) )
#else
inline bool IsLightVisible( const CLightSource& lgt )
#endif
{
	bool result;
	if (lgt.GetType() == LIGHT_TYPE_SPOT && lgt.GetConeCosOuterAngle() >= 0.01f)
	{
		Mat34V mtx;
		float threshold=0.0001f;
		Vec3V dir=VECTOR3_TO_VEC3V(lgt.GetDirection());
		if ( MagSquared(dir).Getf()<threshold )// invalid light so can't be visible
		{
			Assertf(true,"Light is invalid, has zero direction vector");
			return false;
		}
		LookDown(mtx, -dir, (Abs(dir.GetYf())>.95f) ? Vec3V(V_X_AXIS_WZERO):Vec3V(V_Y_AXIS_WZERO));
		mtx.Setd(VECTOR3_TO_VEC3V(lgt.GetPosition()));

#if __SPU
		result = IsConeVisibleAsync(mtx, lgt.GetConeHeight(),lgt.GetConeBaseRadius(), SCAN_OCCLUSION_STORAGE_PRIMARY, scanBaseInfo, hiZBuffer,
			conePoints, numConePoints, coneIndices, numConeIndices);
#else
		result = COcclusion::IsConeVisible( mtx, lgt.GetConeHeight(),lgt.GetConeBaseRadius() );	
#endif
	}
	else if (lgt.GetType() == LIGHT_TYPE_CAPSULE && lgt.GetCapsuleExtent() > 0.0)
	{
		spdAABB bound;
		const Vector3 pointA = lgt.GetPosition() + (lgt.GetDirection() * (lgt.GetCapsuleExtent() * 0.5f));
		const Vector3 pointB = lgt.GetPosition() - (lgt.GetDirection() * (lgt.GetCapsuleExtent() * 0.5f));

		ScalarV radius = ScalarV(lgt.GetRadius());
		const spdSphere capSphereA = spdSphere(RCC_VEC3V(pointA), radius);
		const spdSphere capSphereB = spdSphere(RCC_VEC3V(pointB), radius);

		bound.Invalidate();
		bound.GrowSphere(capSphereA);
		bound.GrowSphere(capSphereB);
#if __SPU
		result = !IsAABBOccludedAsync(bound, vCameraPos, SCAN_OCCLUSION_STORAGE_PRIMARY, scanBaseInfo, hiZBuffer
			DEV_ONLY(, trivialAcceptActiveFrustum, trivialAcceptMinZ, trivialAcceptVisiblePixel));
#else
		result = COcclusion::IsBoxVisible( bound);
#endif
	}
	else
	{
#if __SPU
		result = IsSphereVisibleAsync(spdSphere(VECTOR3_TO_VEC3V(lgt.GetPosition()), ScalarV(lgt.GetRadius())), SCAN_OCCLUSION_STORAGE_PRIMARY, scanBaseInfo, hiZBuffer,
			icosahedronPoints, numIcosahedronPoints, icosahedronIndices, numIcosahedronIndices);
#else
		result = COcclusion::IsSphereVisible(spdSphere(VECTOR3_TO_VEC3V(lgt.GetPosition()), ScalarV(lgt.GetRadius())));
#endif
	}

#if __DEV
#if !__SPU
	u32 *pNumObjectsOccluded = &COcclusion::m_NumOfObjectsOccluded[SCAN_OCCLUSION_STORAGE_COUNT];
	u32 *pNumOcclusionTests = &COcclusion::m_NumOfOcclusionTests[SCAN_OCCLUSION_STORAGE_COUNT];
#endif

	if (!result)
		sysInterlockedIncrement(pNumObjectsOccluded);
	sysInterlockedIncrement(pNumOcclusionTests);
#endif

	return result;
}

#endif // !INC_LIGHTCULLING_H_
