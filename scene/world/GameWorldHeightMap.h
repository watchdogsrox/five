// ================================
// scene/world/GameWorldHeightMap.h
// (c) 2011 RockstarNorth
// ================================

#ifndef _SCENE_WORLD_GAMEWORLDHEIGHTMAP_H_
#define _SCENE_WORLD_GAMEWORLDHEIGHTMAP_H_

#include "atl/map.h"
#include "phcore/materialmgr.h" // for phMaterialMgr::Id
#include "system/bit.h"
#include "vectormath/vec3v.h"

#include "fwgeovis/geovis.h"

namespace rage { class spdAABB; }
namespace rage { class bkBank; }

class CEntity;

#define HEIGHTMAP_TOOL (__BANK || (__WIN32PC /*&& !__D3D11*/ && !__FINAL))

#define HEIGHTMAP_TOOL_DENSITY_MAP_TEST (0 && HEIGHTMAP_TOOL && __WIN32PC)
#define HEIGHTMAP_TOOL_DEBUG_OUTPUT_DIR "assets:/non_final/heightmap"

#if HEIGHTMAP_TOOL
enum
{
	GWHM_FLAG_WATER_COLLISION         = BIT(0),
	GWHM_FLAG_WATER_OCCLUDER          = BIT(1),
	GWHM_FLAG_WATER_DRAWABLE          = BIT(2),
	GWHM_FLAG_STAIRS                  = BIT(3),
	GWHM_FLAG_STAIRSLOPE              = BIT(4),
	GWHM_FLAG_ROAD                    = BIT(5),
	GWHM_FLAG_PROP                    = BIT(6),
	GWHM_FLAG_SCRIPT                  = BIT(7),
	GWHM_FLAG_MOVER_BOUND             = BIT(8),
	GWHM_FLAG_WEAPON_BOUND            = BIT(9),
	GWHM_FLAG_VEHICLE_BOUND           = BIT(10),
	GWHM_FLAG_CONTAINS_MOVER_BOUNDS   = BIT(11),
	GWHM_FLAG_CONTAINS_WEAPON_BOUNDS  = BIT(12),
	GWHM_FLAG_CONTAINS_VEHICLE_BOUNDS = BIT(13),
	GWHM_FLAG_INTERIOR                = BIT(14),
	GWHM_FLAG_EXTERIOR_PORTAL         = BIT(15),
	GWHM_FLAG_NON_BVH_PRIMITIVE       = BIT(16),
	GWHM_FLAG_MIRROR_SURFACE_PORTAL   = BIT(17),
	GWHM_FLAG_MIRROR_DRAWABLE         = BIT(18),
	GWHM_FLAG_WATER_SURFACE_PORTAL    = BIT(19),
	GWHM_FLAG_WATER_NO_REFLECTION     = BIT(20), // timecycle modifier boxes (TCVAR_WATER_REFLECTION_FAR_CLIP = 0)
	GWHM_FLAG_CABLE                   = BIT(21),
	GWHM_FLAG_LIGHT                   = BIT(22),
	GWHM_FLAG_CORONA_QUAD             = BIT(23),
	GWHM_FLAG_ANIMATED_BUILDING       = BIT(24),
	GWHM_FLAG_MASK_ONLY               = BIT(25),
};
#endif // HEIGHTMAP_TOOL

class CHeightMapBoundsInterface;

class CGameWorldHeightMap
{
public:
	static void	Init(unsigned initMode);
	static void	Shutdown(unsigned shutdownMode);

	static void LoadExtraFile(char const* filename);
	static void EnableHeightmap(char const* heightmapname, bool enable);

	static void SetEnableHeightmap(CHeightMapBoundsInterface* heightmap);
#if __BANK
	static void AddWidgets(bkBank* pBank);
	static void DebugDraw(Vec3V_In camPos);
#endif // __BANK

	static float GetMinHeightFromWorldHeightMap(float x, float y);
	static float GetMinHeightFromWorldHeightMap(float x0, float y0, float x1, float y1);
	static float GetMaxHeightFromWorldHeightMap(float x, float y);
	static float GetMaxHeightFromWorldHeightMap(float x0, float y0, float x1, float y1);

	static float GetMinIntervalHeightFromWorldHeightMap(float x0, float y0, float x1, float y1, float maxStep = 20.0f);
	static float GetMaxIntervalHeightFromWorldHeightMap(float x0, float y0, float x1, float y1, float maxStep = 20.0f);

	static float GetMinInterpolatedHeightFromWorldHeightMap(float x, float y, bool bUseMinFilter);
	static float GetMaxInterpolatedHeightFromWorldHeightMap(float x, float y, bool bUseMaxFilter);

	static void GetWorldHeightMapIndex(int& i, int& j, float x, float y); // used by cascade shadows to determine when to scan height map

#if HEIGHTMAP_TOOL

	static void ResetWorldHeightMaps(const char* waterBoundaryPath = NULL, const Vec3V* boundsMin = NULL, const Vec3V* boundsMax = NULL, int resolution = 1, int downsample = 1, bool bMasksOnly = false, bool bCreateDensityMap = false);
	static void ResetWorldHeightMapTiles(int tileX, int tileY, int tileW, int tileH, int numSectorsPerTile, int resolution, int downsample, bool bMasksOnly, bool bCreateDensityMap, bool bCreateExperimentalDensityMaps, bool bCreateWaterOnly, const float* worldBoundsMinZOverride = NULL, const atMap<u16,int>* materialMap = NULL, int materialCount = 0);

	static void AddGeometryBegin();
	static void AddGeometryEnd();

	static void AddTriangleToWorldHeightMap(
		Vec3V_In p0,
		Vec3V_In p1,
		Vec3V_In p2,
		u32 flags,
		int materialId = -1, // index into special material list (low 8 bits) and proc tag (high 8 bits)
		u32 propGroupMask = 0,
		u8 pedDensity = 0,
		u8 moverBoundPolyDensity = 0,
		u8 moverBoundPrimDensity = 0,
		u8 weaponBoundPolyDensity = 0,
		u8 weaponBoundPrimDensity = 0,
		u32 boundID_rpf = 0,
		u32 boundId = 0,
		int boundPrimitiveType = 0 // 0=PRIM_TYPE_POLYGON
		);

#if __BANK
	static void PostCreateDrawableForEntity(CEntity* entity);
#endif // __BANK

	static void AddWaterGeometryBegin();
	static void AddWaterGeometryEnd();

	static void FinaliseWorldHeightMaps(bool bExpandWorldBounds);

	static void SaveWorldHeightMaps(bool bAutoImageRange = false, bool bIsUnderwater = false);
	static void SaveWorldHeightMapTile(char* dir, int tileX, int tileY, int numSectorsPerTile, int* _numEmptyWorldCells = NULL);

#if __WIN32PC
	static int   GetHRNumCols();
	static int   GetHRNumRows();
	static float GetHROneOverCellSizeX();
	static float GetHROneOverCellSizeY();
	static u16*  GetHRHeightMin();
	static u16*  GetHRHeightMax();
	static u8*   GetHRWorldMask();
	static float GetHRCellHeight(u16 cellZ);
#endif // __WIN32PC

	static const char** GetMaterialMaskList();
	static int GetMaterialMaskIndex(phMaterialMgr::Id materialId);
	static const char** GetProceduralList();

#endif // HEIGHTMAP_TOOL
};

#endif // _SCENE_WORLD_GAMEWORLDHEIGHTMAP_H_
