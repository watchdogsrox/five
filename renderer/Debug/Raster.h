// =======================
// renderer/Debug/Raster.h
// (c) 2012 RockstarNorth
// =======================

#ifndef _RENDERER_DEBUG_RASTER_H_
#define _RENDERER_DEBUG_RASTER_H_

#define RASTER             (1 && __DEV)
#define RASTER_GEOM_BUFFER (0 && RASTER)

#if RASTER

#if RASTER_GEOM_BUFFER
	#define RASTER_GEOM_BUFFER_ONLY(x) x
#else
	#define RASTER_GEOM_BUFFER_ONLY(x)
#endif

#include "vectormath/vec3v.h"

namespace rage { class grcViewport; }

class CRasterControl
{
public:
	bool  m_useCoverage;        // rasterise coverage instead of pixel centres
	bool  m_useCorners;         // rasterise min/max corner depth instead of pixel centres
	bool  m_useLinearZ;         // convert sample depth to linear depth before rasterisation
#if __BANK
	bool  m_perfectClip;        // clip triangles to pixels perfectly, this is really slow so we would only want to do it for one frame
	float m_displayOpacity;     // opacity for debug draw
	bool  m_displayStencilOnly; // display stencil only (not depth)
	float m_displayLinearZ0;    // depth values <= this in linear space will be displayed with max shade
	float m_displayLinearZ1;    // depth values >= this in linear space will be displayed with min shade
#endif // __BANK
};

#if __BANK
class CRasterStatistics
{
public:
	int m_numPixelsPassed;  // number of pixels passed the depth test
	int m_numPixels;        // number of pixels rasterised (passed or failed)
	int m_numPixelsBounds;  // number of pixels if we rasterised triangles as boxes (e.g. softrasterizer)
	int m_numPolysVisible;  // number of polys with at least one pixel passed
	int m_numPolys;         // number of polys which were in the viewport and not backfacing
	int m_numPolysScene;    // number of polys total in the scene (all models and boxes in the viewport)
	int m_numModelsVisible;
	int m_numModels;
	int m_numModelsScene;
	int m_numBoxesVisible;
	int m_numBoxes;
	int m_numBoxesScene;
};
#endif // __BANK

class CRasterInterface
{
public:
	static CRasterControl& GetControlRef();

	static bool IsInited();
	static void Init(const grcViewport* vp, int w, int h, int maxOccludeVerts = 0, int maxStencilVerts = 3200);
	static void Clear(const grcViewport* vp);
	static void RasteriseOccludePoly(const Vec3V* points, int numPoints, bool bClipToFrustum = true);
	static void RasteriseStencilPoly(const Vec3V* points, int numPoints, bool bClipToFrustum = true);
#if RASTER_GEOM_BUFFER
	static void RegisterOccludePoly(const Vec3V* points, int numPoints);
	static void RegisterStencilPoly(const Vec3V* points, int numPoints);
	static void RasteriseOccludeGeometry();
	static void RasteriseStencilGeometry();
#endif // RASTER_GEOM_BUFFER
	static void RasteriseOccludersAndWater(bool bBoxes, bool bMeshes, bool bWater);
	static int GetWidth();
	static int GetHeight();
	static const float* GetDepthData();
	static const u64* GetStencilData();
#if __BANK
	static CRasterStatistics& GetOccludeStatsRef();
	static CRasterStatistics& GetStencilStatsRef();
	static void PerfectClip();
	static void DebugDraw();
	static void DebugDrawOccludersAndWater(bool bBoxes, bool bMeshes, bool bWater, float opacity);
	static void DumpOccludersToOBJ_Begin(const char* path);
	static void DumpOccludersToOBJ_End();
	static void DumpOccludersToOBJ_Update(bool bWaterOccluders);
#endif // __BANK
};

#endif // RASTER
#endif // _RENDERER_DEBUG_RASTER_H_