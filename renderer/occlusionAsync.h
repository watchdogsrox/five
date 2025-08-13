
#include "fwscene/scan/ScanEntities.h"
#include "softrasterizer/softrasterizer.h"
#include "fwscene/scan/ScanDebug.h"
#if !__SPU
#include "renderer/occlusion.h"
#endif

///////////////////////////////////////////////////////////////////////////////
//  IsAABBOccludedAsync
///////////////////////////////////////////////////////////////////////////////
bool IsAABBOccludedAsync(const spdAABB& aabb, Vec3V_In camPos, int occluderPhase, const fwScanBaseInfo* scanBaseInfo, const Vec4V* hiZBuffer
						 DEV_ONLY(, u32* trivialAcceptActiveFrustum, u32* trivialAcceptMinZ, u32* trivialAcceptVisiblePixel));

///////////////////////////////////////////////////////////////////////////////
//  IsSphereVisibleAsync
///////////////////////////////////////////////////////////////////////////////
bool IsSphereVisibleAsync(const spdSphere& sphere, int occluderPhase, const fwScanBaseInfo* scanBaseInfo, const Vec4V* hiZBuffer,
						  const Vec3V* IcosahedronPts, u32 numIcosahedronPts, const u8* IcosahedronIndices, u32 numIcosahedronIndices);

///////////////////////////////////////////////////////////////////////////////
//  IsConeVisibleAsync
///////////////////////////////////////////////////////////////////////////////
bool IsConeVisibleAsync(const Mat34V& mtx, float radius, float spreadf, int occluderPhase, const fwScanBaseInfo* scanBaseInfo, const Vec4V* hiZBuffer,
						const Vec3V* ConePts, u32 numConePts, const u8* ConeIndices, u32 numConeIndices);
