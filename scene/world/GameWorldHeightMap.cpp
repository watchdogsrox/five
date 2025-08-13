// ==================================
// scene/world/GameWorldHeightMap.cpp
// (c) 2011 RockstarNorth
// ==================================

#include "file/stream.h"
#include "grcore/debugdraw.h"
#if __PS3
#include "grcore/edgeExtractgeomspu.h"
#endif // __PS3
#include "grcore/image.h"
#include "grcore/indexbuffer.h"
#include "grmodel/geometry.h"
#include "spatialdata/aabb.h"
#include "spatialdata/kdop2d.h" // for GetUniquePoints
#include "string/stringutil.h"
#include "system/memory.h"
#include "system/nelem.h"
#include "vectormath/classes.h"

#include "fwdrawlist/drawlist.h"
#include "fwgeovis/geovis.h"
#include "fwmaths/kDOP18.h"
#include "fwmaths/vectorutil.h"
#include "fwutil/xmacro.h"

#include "camera/CamInterface.h"
#include "camera/debug/DebugDirector.h"
#include "debug/DebugArchetypeProxy.h"
#include "objects/DummyObject.h"
#include "objects/object.h"
#include "peds/ped.h"
#include "physics/gtaMaterialDebug.h"
#include "renderer/Water.h"
#include "scene/world/GameWorldHeightMap.h"
#include "Scene/DataFileMgr.h"
#include "scene/Building.h"
#include "scene/AnimatedBuilding.h"
#include "shaders/CustomShaderEffectCable.h" // for CheckCables
#include "streaming/streaming.h"
#include "system/memops.h"

SCENE_OPTIMISATIONS()

// ================================================================================================

#define GWHM_SAFE_DELETE(p) if (p) { delete[] p; p = NULL; }
#define GWHM_SAFE_DELETE_VP(p) if (p) { delete[] (char*)p; p = NULL; }

PS3_ONLY(namespace rage { extern u32 g_AllowVertexBufferVramLocks; })

typedef u8 CellTypeDefault;
#if __WIN32PC || RSG_DURANGO || RSG_ORBIS
typedef u16 CellTypeHR;
#else
typedef u8 CellTypeHR;
#endif

class CHeightMapBoundsInterface BANK_ONLY(: public datBase)
{
public:
	CHeightMapBoundsInterface(float defaultCellSize, float defaultMinX, float defaultMinY, float defaultMinZ, float defaultMaxX, float defaultMaxY, float defaultMaxZ)
		: m_defaultCellSize(defaultCellSize)
		, m_defaultBoundsMinX(defaultMinX)
		, m_defaultBoundsMinY(defaultMinY)
		, m_defaultBoundsMinZ(defaultMinZ)
		, m_defaultBoundsMaxX(defaultMaxX)
		, m_defaultBoundsMaxY(defaultMaxY)
		, m_defaultBoundsMaxZ(defaultMaxZ)
	{

	}
	void Clear()
	{
		m_cellSizeX  = m_defaultCellSize;
		m_cellSizeY  = m_defaultCellSize;
		m_boundsMinX = m_defaultBoundsMinX;
		m_boundsMinY = m_defaultBoundsMinY;
		m_boundsMinZ = m_defaultBoundsMinZ;
		m_boundsMaxX = m_defaultBoundsMaxX;
		m_boundsMaxY = m_defaultBoundsMaxY;
		m_boundsMaxZ = m_defaultBoundsMaxZ;

		// calculate from bounds
		m_numCols = (int)((m_boundsMaxX - m_boundsMinX)/m_cellSizeX);
		m_numRows = (int)((m_boundsMaxY - m_boundsMinY)/m_cellSizeY);

		sysMemSet(m_path, 0, sizeof(m_path));
	}

#if __BANK

	void AddWidgets(bkBank* pBank)
	{
		datCallback _UpdateCellSize(MFA(CHeightMapBoundsInterface::UpdateCellSize), this);

		const float globalBoundsMinX = m_defaultBoundsMinX;
		const float globalBoundsMinY = m_defaultBoundsMinY;
		const float globalBoundsMaxX = m_defaultBoundsMaxX;
		const float globalBoundsMaxY = m_defaultBoundsMaxY;

		pBank->AddSlider("Num columns" , &m_numCols, 8, 1024, 1, _UpdateCellSize);
		pBank->AddSlider("Num rows"    , &m_numRows, 8, 1024, 1, _UpdateCellSize);
		pBank->AddSlider("Cell size x" , &m_cellSizeX, 1.0f, 1000.0f, 0.0f);
		pBank->AddSlider("Cell size y" , &m_cellSizeY, 1.0f, 1000.0f, 0.0f);
		m_pSliders[0] = pBank->AddSlider("Bounds min x", &m_boundsMinX, globalBoundsMinX, globalBoundsMaxX, 1.0f, _UpdateCellSize);
		m_pSliders[1] = pBank->AddSlider("Bounds min y", &m_boundsMinY, globalBoundsMinY, globalBoundsMaxY, 1.0f, _UpdateCellSize);
		m_pSliders[2] = pBank->AddSlider("Bounds min z", &m_boundsMinZ, -1000.0f, 1000.0f, 1.0f);
		m_pSliders[3] = pBank->AddSlider("Bounds max x", &m_boundsMaxX, globalBoundsMinX, globalBoundsMaxX, 1.0f, _UpdateCellSize);
		m_pSliders[4] = pBank->AddSlider("Bounds max y", &m_boundsMaxY, globalBoundsMinY, globalBoundsMaxY, 1.0f, _UpdateCellSize);
		m_pSliders[5] = pBank->AddSlider("Bounds max z", &m_boundsMaxZ, -1000.0f, 1000.0f, 1.0f);

		datCallback _UpdateSliders(MFA(CHeightMapBoundsInterface::UpdateSliders), this);

		const float minValue = -16000.0f;
		const float maxValue = 16000.0f;
		pBank->AddSlider("Default Cell Size", &m_defaultCellSize, 1.0f, 1000.0f, 0.0f, _UpdateSliders);
		pBank->AddSlider("Default Bounds min X", &m_defaultBoundsMinX, minValue, maxValue, 1.0f, _UpdateSliders);
		pBank->AddSlider("Default Bounds min Y", &m_defaultBoundsMinY, minValue, maxValue, 1.0f, _UpdateSliders);
		pBank->AddSlider("Default Bounds min Z", &m_defaultBoundsMinZ, minValue, maxValue, 1.0f, _UpdateSliders);
		pBank->AddSlider("Default Bounds max X", &m_defaultBoundsMaxX, minValue, maxValue, 1.0f, _UpdateSliders);
		pBank->AddSlider("Default Bounds max Y", &m_defaultBoundsMaxY, minValue, maxValue, 1.0f, _UpdateSliders);
		pBank->AddSlider("Default Bounds max Z", &m_defaultBoundsMaxZ, minValue, maxValue, 1.0f, _UpdateSliders);
	}

	void UpdateFrom(const CHeightMapBoundsInterface& bounds)
	{
		m_numCols		= bounds.m_numCols;
		m_numRows		= bounds.m_numRows;
		m_cellSizeX	= bounds.m_cellSizeX;
		m_cellSizeY	= bounds.m_cellSizeY;
		m_boundsMinX	= bounds.m_boundsMinX;
		m_boundsMinY	= bounds.m_boundsMinY;
		m_boundsMinZ	= bounds.m_boundsMinZ;
		m_boundsMaxX	= bounds.m_boundsMaxX;
		m_boundsMaxY	= bounds.m_boundsMaxY;
		m_boundsMaxZ	= bounds.m_boundsMaxZ;
	}

	template <int cellSize, int resScale> void ResetBoundsToMainLevel_() { ResetBoundsToMainLevel(cellSize, resScale); }

#endif // __BANK

#if HEIGHTMAP_TOOL

	void ResetBoundsToMainLevel(int cellSize, int resScale)
	{
		m_boundsMinX = (float)gv::WORLD_BOUNDS_MIN_X;
		m_boundsMinY = (float)gv::WORLD_BOUNDS_MIN_Y;
		m_boundsMinZ = (float)gv::WORLD_BOUNDS_MIN_Z;
		m_boundsMaxX = (float)gv::WORLD_BOUNDS_MAX_X;
		m_boundsMaxY = (float)gv::WORLD_BOUNDS_MAX_Y;
		m_boundsMaxZ = (float)gv::WORLD_BOUNDS_MAX_Z;

		// calculate from bounds
		m_numCols = resScale*(int)((m_boundsMaxX - m_boundsMinX)/(float)cellSize);
		m_numRows = resScale*(int)((m_boundsMaxY - m_boundsMinY)/(float)cellSize);

		UpdateCellSize();
	}

#endif // HEIGHTMAP_TOOL

	void UpdateCellSize()
	{
		m_cellSizeX = (m_boundsMaxX - m_boundsMinX)/(float)m_numCols;
		m_cellSizeY = (m_boundsMaxY - m_boundsMinY)/(float)m_numRows;
	}
#if __BANK
	void UpdateSliders()
	{
		m_pSliders[0]->SetRange(m_defaultBoundsMinX, m_defaultBoundsMaxX);
		m_pSliders[1]->SetRange(m_defaultBoundsMinY, m_defaultBoundsMaxY);
		m_pSliders[2]->SetRange(m_defaultBoundsMinZ, m_defaultBoundsMaxZ);
		m_pSliders[3]->SetRange(m_defaultBoundsMinX, m_defaultBoundsMaxX);
		m_pSliders[4]->SetRange(m_defaultBoundsMinY, m_defaultBoundsMaxY);
		m_pSliders[5]->SetRange(m_defaultBoundsMinZ, m_defaultBoundsMaxZ);

		UpdateCellSize();
	}
#endif // __BANK

	char m_path[80];

	int   m_numCols;
	int   m_numRows;
	float m_cellSizeX;
	float m_cellSizeY;
	float m_boundsMinX;
	float m_boundsMinY;
	float m_boundsMinZ;
	float m_boundsMaxX;
	float m_boundsMaxY;
	float m_boundsMaxZ;

#if __BANK
	bkSlider* m_pSliders[6];
#endif // __BANK

	float m_defaultCellSize;
	float m_defaultBoundsMinX;
	float m_defaultBoundsMinY;
	float m_defaultBoundsMinZ;
	float m_defaultBoundsMaxX;
	float m_defaultBoundsMaxY;
	float m_defaultBoundsMaxZ;
};

#if __BANK
class CHeightMapInterface : public CHeightMapBoundsInterface
{
public:
	CHeightMapInterface(float defaultCellSize, float defaultMinX, float defaultMinY, float defaultMinZ, float defaultMaxX, float defaultMaxY, float defaultMaxZ)
		: CHeightMapBoundsInterface(defaultCellSize, defaultMinX, defaultMinY, defaultMinZ, defaultMaxX, defaultMaxY, defaultMaxZ)
	{}

	void Clear()
	{
		sysMemSet(m_pathImportExportDDS, 0, sizeof(m_pathImportExportDDS));

		CHeightMapBoundsInterface::Clear();
	}
	
	char m_pathImportExportDDS[80];
};
#endif // __BANK

#if HEIGHTMAP_TOOL
class CHeightMapMask
{
public:
	char m_name[16];
	int  m_numCols;
	int  m_numRows;
	u8*  m_data;
};
#endif // HEIGHTMAP_TOOL

template <typename CellType> static __forceinline bool CellTypeIsFloat() { return false; }

template <> __forceinline bool CellTypeIsFloat<float>() { return true; }

template <typename CellType> static __forceinline CellType GetCellMaxValue(float maxZ);

template <> __forceinline u8    GetCellMaxValue<u8   >(float) { return 255; }
template <> __forceinline u16   GetCellMaxValue<u16  >(float) { return 65535; }
template <> __forceinline float GetCellMaxValue<float>(float maxZ) { return maxZ; }

template <typename CellType> static __forceinline CellType GetCellEmptyValue() { return (CellType)0; }

template <> __forceinline float GetCellEmptyValue<float>() { return -1.0f; }

template <typename CellType> static __forceinline CellType QuantiseCellMin(float value, float boundsMinZ, float boundsMaxZ);
template <typename CellType> static __forceinline CellType QuantiseCellMax(float value, float boundsMinZ, float boundsMaxZ);
template <typename CellType> static __forceinline CellType QuantiseCellAvg(float value, float boundsMinZ, float boundsMaxZ);

template <> __forceinline u8    QuantiseCellMin<u8   >(float value, float boundsMinZ, float boundsMaxZ) { return (u8 )Clamp<float>(floorf(  255.0f*(value - boundsMinZ)/(boundsMaxZ - boundsMinZ)), 0.0f,   255.0f); }
template <> __forceinline u16   QuantiseCellMin<u16  >(float value, float boundsMinZ, float boundsMaxZ) { return (u16)Clamp<float>(floorf(65535.0f*(value - boundsMinZ)/(boundsMaxZ - boundsMinZ)), 0.0f, 65535.0f); }
template <> __forceinline float QuantiseCellMin<float>(float value, float, float) { return value; }

template <> __forceinline u8    QuantiseCellMax<u8   >(float value, float boundsMinZ, float boundsMaxZ) { return (u8 )Clamp<float>(ceilf(  255.0f*(value - boundsMinZ)/(boundsMaxZ - boundsMinZ)), 0.0f,   255.0f); }
template <> __forceinline u16   QuantiseCellMax<u16  >(float value, float boundsMinZ, float boundsMaxZ) { return (u16)Clamp<float>(ceilf(65535.0f*(value - boundsMinZ)/(boundsMaxZ - boundsMinZ)), 0.0f, 65535.0f); }
template <> __forceinline float QuantiseCellMax<float>(float value, float, float) { return value; }

template <> __forceinline u8    QuantiseCellAvg<u8   >(float value, float boundsMinZ, float boundsMaxZ) { return (u8 )Clamp<float>(floorf(0.5f +   255.0f*(value - boundsMinZ)/(boundsMaxZ - boundsMinZ)), 0.0f,   255.0f); }
template <> __forceinline u16   QuantiseCellAvg<u16  >(float value, float boundsMinZ, float boundsMaxZ) { return (u16)Clamp<float>(floorf(0.5f + 65535.0f*(value - boundsMinZ)/(boundsMaxZ - boundsMinZ)), 0.0f, 65535.0f); }
template <> __forceinline float QuantiseCellAvg<float>(float value, float, float) { return value; }

template <typename CellType> static __forceinline float UnquantiseCell(CellType value, float boundsMinZ, float boundsMaxZ);

template <> __forceinline float UnquantiseCell<u8   >(u8    value, float boundsMinZ, float boundsMaxZ) { return boundsMinZ + (boundsMaxZ - boundsMinZ)*((float)value/  255.0f); }
template <> __forceinline float UnquantiseCell<u16  >(u16   value, float boundsMinZ, float boundsMaxZ) { return boundsMinZ + (boundsMaxZ - boundsMinZ)*((float)value/65535.0f); }
template <> __forceinline float UnquantiseCell<float>(float value, float, float) { return value; }

template <typename T> static __forceinline void ByteSwapElement(T& elem)
{
	grcImage::ByteSwapData(&elem, sizeof(T), sizeof(T));
}

#if HEIGHTMAP_TOOL

enum eCreateHeightmapFlags
{
	CREATE_HEIGHTMAP             = BIT(0), // min/max heightmap and world mask
	CREATE_WATERMASK             = BIT(1),
	CREATE_WATERHEIGHT           = BIT(2),
	CREATE_STAIRSMASK            = BIT(3),
	CREATE_ROADSMASK             = BIT(4),
	CREATE_SCRIPTMASK            = BIT(5),
	CREATE_BOUNDMASK             = BIT(6),
	CREATE_BOUNDTYPEMASK         = BIT(7),
	CREATE_WATERMIRRORPORTALMASK = BIT(8),
	CREATE_CABLEMASK             = BIT(9),
	CREATE_CORONAMASK            = BIT(10),
	CREATE_ANIMBLDGMASK          = BIT(11),
	CREATE_INTERIORMASK          = BIT(12),
	CREATE_MATERIALMASK          = BIT(13),

	CREATE_DENSITYMAP_PED        = BIT(14),
	CREATE_DENSITYMAP_MOVER      = BIT(15),
	CREATE_DENSITYMAP_WEAPON     = BIT(16),
	CREATE_DENSITYMAP_LIGHT      = BIT(17),
	CREATE_DENSITYMAP            = BIT(18),
	CREATE_DENSITYMAP_EX         = BIT(19),
	CREATE_BOUND_ID_MAP          = BIT(20),

	CREATE_DENSITYMAP_ALL        = CREATE_DENSITYMAP_PED | CREATE_DENSITYMAP_MOVER | CREATE_DENSITYMAP_WEAPON | CREATE_DENSITYMAP_LIGHT | CREATE_DENSITYMAP,
	CREATE_WORLDMASKONLY         = 0,
};

#define DENSITY_FUNC(x) powf(x, 0.77f)//sqrtf(x)
#define DENSITY_MIN_AREA 0.001f

#endif // HEIGHTMAP_TOOL

template <typename CellType> class CHeightMap : public CHeightMapBoundsInterface
{
private:
	class CHeightMapHeader
	{
	public:
		void ByteSwap()
		{
			ByteSwapElement(m_tag);
			ByteSwapElement(m_flags);
			ByteSwapElement(m_numCols);
			ByteSwapElement(m_numRows);
			ByteSwapElement(m_boundsMinX);
			ByteSwapElement(m_boundsMinY);
			ByteSwapElement(m_boundsMinZ);
			ByteSwapElement(m_boundsMaxX);
			ByteSwapElement(m_boundsMaxY);
			ByteSwapElement(m_boundsMaxZ);
			ByteSwapElement(m_dataSizeInBytes);
		}

		enum
		{
			HMAP_FLAG_RLE_DATA   = BIT(0),
			HMAP_FLAG_WATER_MASK = BIT(1),
		};

		u32   m_tag; // 'HMAP'
		u8    m_version;
		u8    m_cellSizeInBytes; // 1 for u8 data
		u8    m_pad[2];
		u32   m_flags;
		u16   m_numCols;
		u16   m_numRows;
		float m_boundsMinX;
		float m_boundsMinY;
		float m_boundsMinZ;
		float m_boundsMaxX;
		float m_boundsMaxY;
		float m_boundsMaxZ;
		u32   m_dataSizeInBytes; // includes RLE + max + min data buffers, but not water mask
	};

	class RLE
	{
	public:
		__forceinline void Set(int start, int count, int dataOffset)
		{
			m_start      = (u16)start;
			m_count      = (u16)count;
			m_dataOffset = dataOffset;
		}

		void ByteSwap()
		{
			ByteSwapElement(m_start);
			ByteSwapElement(m_count);
			ByteSwapElement(m_dataOffset);
		}

		__forceinline int GetStart() const { return (int)m_start; }
		__forceinline int GetStop() const { return (int)(m_start + m_count) - 1; }
		__forceinline int GetCount() const { return (int)m_count; }
		__forceinline bool GetIsWithinRange(int i) const { return i >= GetStart() && i <= GetStop(); }
		__forceinline int GetDataOffset(int i) const { return m_dataOffset + i; }
		__forceinline int GetDataStart() const { return m_dataOffset + GetStart(); }

	private:
		u16 m_start;      // start column index
		u16 m_count;      // number of cells in row
		int m_dataOffset; // offset into m_data[] for column 0
	};

public:
	CHeightMap();
	~CHeightMap();

	inline bool GetIndexAtCoord(int& i, int& j, float x, float y) const;
	inline bool GetIndexAtBound(int& i0, int& j0, int& i1, int& j1, float x0, float y0, float x1, float y1) const;

	void SetMinHeightAtCoord(float x, float y, float h);
	void SetMaxHeightAtCoord(float x, float y, float h);

	float GetMinHeightAtIndex(int i, int j) const;
	float GetMaxHeightAtIndex(int i, int j) const;
	float GetMinHeightAtCoord(float x, float y) const;
	float GetMaxHeightAtCoord(float x, float y) const;
	float GetMinHeightAtBound(float x0, float y0, float x1, float y1) const;
	float GetMaxHeightAtBound(float x0, float y0, float x1, float y1) const;

	spdAABB GetBoundingBox() const;

	void SetEnabled(bool b) { m_enabled = b; }
	
	bool IsEnabled() const { return m_enabled; } // enabled heightmap might not have valid data, i.e. if it's mask-only
	bool IsValid() const { return m_enabled && m_data != NULL; }
	bool Load(const char* path);
#if HEIGHTMAP_TOOL
	void Finalise(bool bExpandWorldBounds);
	void Save(const char* path, bool bSaveWaterMask) const;
	void ImportDDS(const char* path);
	void ExportDDS(const char* path, int downsampleData = 1, int downsampleMask = 1, bool bAutoImageRange = false, bool bIgnoreWorldMaskForHeightMap = false) const;
	void Reset(int numCols, int numRows, int downsampe, float boundsMinX, float boundsMinY, float boundsMinZ, float boundsMaxX, float boundsMaxY, float boundsMaxZ, const char* waterBoundaryPath, eCreateHeightmapFlags flags);
	void SetAllMinsToMax(int numRows, int numCols);
	void Release();
	void ConvertToNonRLE();
	int  ConvertToRLE(); // returns number of cells
	void RasteriseAtIndex(
		int i,
		int j,
		float zmin,
		float zmax,
		bool bIsCentreFacingCell, // cell centre is inside triangle, and triangle is facing upwards
		u32 flags, // TODO -- move all the bools below this into these flags
		const CHeightMap<CellTypeHR>* ground,
		int materialId, // index into special material list (low 8 bits) and proc tag (high 8 bits)
		u32 propGroupMask,
		u8 pedDensity,
		u8 moverBoundPolyDensity,
		u8 moverBoundPrimDensity,
		u8 weaponBoundPolyDensity,
		u8 weaponBoundPrimDensity,
		float density,
		u32 boundID_rpf,
		u32 boundID,
		int boundPrimitiveType
#if HEIGHTMAP_TOOL_DENSITY_MAP_TEST
		, bool  bIsEdgeCell   = false
		, bool  bIsVertCell   = false
		, float clippedArea2D = 0.0f
		, float area          = 0.0f
		, float normal_z      = 0.0f
#endif // HEIGHTMAP_TOOL_DENSITY_MAP_TEST
		);
	void RasteriseTriangle(
		Vec3V_In p0,
		Vec3V_In p1,
		Vec3V_In p2,
		u32 flags, // TODO -- move all the bools below this into these flags
		const CHeightMap<CellTypeHR>* ground,
		int materialId, // index into special material list (low 8 bits) and proc tag (high 8 bits)
		u32 propGroupMask,
		u8 pedDensity,
		u8 moverBoundPolyDensity,
		u8 moverBoundPrimDensity,
		u8 weaponBoundPolyDensity,
		u8 weaponBoundPrimDensity,
		u32 boundID_rpf,
		u32 boundID,
		int boundPrimitiveType
		);
	void RasteriseGeometryBegin();
	void RasteriseGeometryEnd();
#if __BANK
	void RasteriseGeometryShowStats(const char* modelName, int modelGeomIndex, Vec3V_In modelPos) const;
#endif // __BANK
	void RasteriseWaterBegin();
	void RasteriseWaterEnd();
	bool GetIsWaterCell(int i, int j) const;
	int  CountWaterCells() const;
#if __BANK
	void AddWidgets(bkBank* pBank, const char* groupName);
	void DebugDraw(Vec3V_In camPos, Color32 meshColour);
#endif // __BANK
	CHeightMapBoundsInterface& GetBoundsInterface() { return *this; }
#endif // HEIGHTMAP_TOOL

	void Clear(bool BANK_ONLY(bClearInterface)) // since this method inherits from datBase and has an interface, we don't want to call sysMemSet(this, 0, sizeof(*this)) anymore
	{
		// make sure we're not leaking memory
		Assert(m_data == NULL);
#if HEIGHTMAP_TOOL
		Assert(m_worldMask == NULL);
		Assert(m_waterMask == NULL);
		Assert(m_stairsMask == NULL);
		Assert(m_stairSlopeMask == NULL);
		Assert(m_roadsMask == NULL);
		Assert(m_scriptMask == NULL);
		Assert(m_moverBoundMask == NULL);
		Assert(m_weaponBoundMask == NULL);
		Assert(m_vehicleBoundMask == NULL);
		Assert(m_moverNoVehicleBoundMaskM == NULL);
		Assert(m_moverNoVehicleBoundMaskP == NULL);
		Assert(m_boundTypeMask_BOX == NULL);
		Assert(m_boundTypeMask_CYLINDER == NULL);
		Assert(m_boundTypeMask_CAPSULE == NULL);
		Assert(m_boundTypeMask_SPHERE == NULL);
		Assert(m_boundTypeMask_nonBVHPRIM == NULL);
		Assert(m_interiorMask == NULL);
		Assert(m_exteriorPortalMask == NULL);
		Assert(m_waterCollisionMask == NULL);
		Assert(m_waterOccluderMask == NULL);
		Assert(m_waterDrawableMask == NULL);
		Assert(m_waterOceanMask == NULL);
		Assert(m_waterOceanGeometryMask == NULL);
		Assert(m_waterNoReflectionMask == NULL);
		Assert(m_underwaterMap == NULL);
		Assert(m_waterHeightMap == NULL);
		Assert(m_waterPortalMask == NULL);
		Assert(m_mirrorDrawableMask == NULL);
		Assert(m_mirrorPortalMask == NULL);
		Assert(m_cableMask == NULL);
		Assert(m_coronaMask == NULL);
		Assert(m_animatedBuildingMask == NULL);
		Assert(m_materialsMask == NULL);
		Assert(m_matProcIdMask == NULL);
		Assert(m_matProcMasks == NULL);
		Assert(m_propGroupMaskMap == NULL);
		Assert(m_pedDensityMap == NULL);
		Assert(m_moverBoundPolyDensityMap == NULL);
		Assert(m_moverBoundPrimDensityMap == NULL);
		Assert(m_weaponBoundPolyDensityMap == NULL);
		Assert(m_weaponBoundPrimDensityMap == NULL);
		Assert(m_lightDensityMap == NULL);
		Assert(m_densityMap == NULL);
		Assert(m_boundIDRpfMap == NULL);
		Assert(m_boundIDMaxMap == NULL);
		Assert(m_boundIDXorMap == NULL);
		Assert(m_waterBoundaryFile == NULL);
#endif // HEIGHTMAP_TOOL

		char* ptr_start = (char*)&m_FIRST_MEMBER;
		char* ptr_stop  = (char*)&m_LAST_MEMBER + sizeof(m_LAST_MEMBER);

		sysMemSet(ptr_start, 0, (size_t)(ptr_stop - ptr_start));

		CHeightMapBoundsInterface::Clear();

#if __BANK
		if (bClearInterface)
		{
			m_interface.Clear();
		}
#endif // __BANK
	}

	int       m_FIRST_MEMBER;
	bool      m_enabled;
	float     m_oneOverCellSizeX;
	float     m_oneOverCellSizeY;
	void*     m_data;
	RLE*      m_dataRLE;
	CellType* m_dataMax;
	CellType* m_dataMin;
#if HEIGHTMAP_TOOL
	int       m_downsample;
	bool      m_geomUpdate; // currently rasterising geometry (not ocean water)
	bool      m_waterUpdate; // currently rasterising water quads (ocean)
	u8*       m_worldMask;
	u8*       m_waterMask;
	u8*       m_stairsMask;
	u8*       m_stairSlopeMask;
	u8*       m_roadsMask;
	u8*       m_scriptMask;
	u8*       m_moverBoundMask;
	u8*       m_weaponBoundMask;
	u8*       m_vehicleBoundMask;
	u8*       m_moverNoVehicleBoundMaskM;
	u8*       m_moverNoVehicleBoundMaskP;
	u8*       m_boundTypeMask_BOX;
	u8*       m_boundTypeMask_CYLINDER;
	u8*       m_boundTypeMask_CAPSULE;
	u8*       m_boundTypeMask_SPHERE;
	u8*       m_boundTypeMask_nonBVHPRIM;
	u8*       m_interiorMask;
	u8*       m_exteriorPortalMask;
	u8*       m_waterCollisionMask;
	u8*       m_waterOccluderMask;
	u8*       m_waterDrawableMask;
	u8*       m_waterOceanMask;
	u8*       m_waterOceanGeometryMask;
	u8*       m_waterNoReflectionMask;
	float*    m_underwaterMap; // underwater height map (negative height)
	float*    m_waterHeightMap;
	u8*       m_waterPortalMask;
	u8*       m_mirrorDrawableMask;
	u8*       m_mirrorPortalMask;
	u8*       m_cableMask;
	u8*       m_coronaMask;
	u8*       m_animatedBuildingMask;
	u64*      m_materialsMask;
	u16*      m_matProcIdMask;
	int       m_matProcMaskCount;
	u64**     m_matProcMasks;
	u16*      m_propGroupMaskMap;
	u8*       m_pedDensityMap;
	u8*       m_moverBoundPolyDensityMap;
	u8*       m_moverBoundPrimDensityMap;
	u8*       m_weaponBoundPolyDensityMap;
	u8*       m_weaponBoundPrimDensityMap;
	u8*       m_lightDensityMap;
	float*    m_densityMap;
#if HEIGHTMAP_TOOL_DENSITY_MAP_TEST
	float*    m_densityMapA;
	float*    m_densityMapB;
	float*    m_densityMapC;
	float*    m_densityMapD;
	float*    m_densityMapE;
	float*    m_densityMapF;
	float*    m_densityMapG;
	float*    m_densityMapH;
	float*    m_densityMapI;
#endif // HEIGHTMAP_TOOL_DENSITY_MAP_TEST
	u32*      m_boundIDRpfMap;
	u32*      m_boundIDMaxMap;
	u32*      m_boundIDXorMap;
	fiStream* m_waterBoundaryFile; // postscript file for water boundary (optional)
	int       m_waterBoundaryEdgesCurrent;
#if __BANK
	int       m_stats_geometryCountTotal;
	int       m_stats_triangleCountTotal;
	int       m_stats_triangleCountCurrent;
	int       m_stats_waterEdgeCountTotal;
	int       m_stats_waterEdgeCountCurrent;
	int       m_stats_cellsCovered; // number of cells covered by current geometry
	int       m_stats_cellsUpdated; // number of cells updated by current geometry
#endif // __BANK
#endif // HEIGHTMAP_TOOL
	int       m_LAST_MEMBER;

#if __BANK
	void Interface_Reset()
	{
		Reset(
			m_interface.m_numCols,
			m_interface.m_numRows,
			1,
			m_interface.m_boundsMinX,
			m_interface.m_boundsMinY,
			m_interface.m_boundsMinZ,
			m_interface.m_boundsMaxX,
			m_interface.m_boundsMaxY,
			m_interface.m_boundsMaxZ,
			NULL,
			CREATE_HEIGHTMAP
		);
	}

	void Interface_SetMinToMax()
	{
		SetAllMinsToMax(m_interface.m_numCols,
			m_interface.m_numRows);

	}

	void Interface_Load() { Load(m_interface.m_path); }
	void Interface_Save() { Save(m_interface.m_path, false); }
	void Interface_SaveWithWaterMask() { Save(m_interface.m_path, true); }
	void Interface_ImportDDS() { ImportDDS(m_interface.m_pathImportExportDDS); }
	void Interface_ExportDDS() { ExportDDS(m_interface.m_pathImportExportDDS); }

	CHeightMapInterface m_interface;
#endif // __BANK

#if HEIGHTMAP_TOOL
	atArray<CHeightMapMask> m_masks;
#endif // HEIGHTMAP_TOOL

	float m_playerLocationMapMinZ;
	float m_playerLocationMapMaxZ;
	int m_cellI;
	int m_cellJ;

	bool  m_worldHeightMapDebugDraw;
	float m_worldHeightMapDebugDrawExtent;
	bool  m_worldHeightMapDebugDrawSolidTop;
	bool  m_worldHeightMapDebugDrawSolidBottom;
	bool m_editOnTheFlyMode;
	float m_editOnTheFlyOffset;
	bool m_editOnTheFlyMinHeights;
};

#define g_WorldHeightMapML g_WorldHeightMap // main level

static CHeightMap<CellTypeDefault> g_WorldHeightMap;
static const int g_NumAuxHeightMaps = 1; // We can add more if we need more heightmaps
static CHeightMap<CellTypeDefault> g_AuxHeightMaps[g_NumAuxHeightMaps];
#if HEIGHTMAP_TOOL
static CHeightMap<CellTypeHR>      g_WorldHeightMapHR; // high-res heightmap
static const atMap<u16,int>*       g_GlobalMaterialMap = NULL;
static int                         g_GlobalMaterialCount = 0;
#endif // HEIGHTMAP_TOOL

const float WorldMinX = (float)gv::WORLD_BOUNDS_MIN_X;
const float WorldMaxX = (float)gv::WORLD_BOUNDS_MAX_X_ISLANDHEIST; // Island is further than WORLD_BOUNDS_MAX_X
const float WorldMinY = (float)gv::WORLD_BOUNDS_MIN_Y_ISLANDHEIST; // Island is further than WORLD_BOUNDS_MIN_Y
const float WorldMaxY = (float)gv::WORLD_BOUNDS_MAX_Y;

template <typename CellType> CHeightMap<CellType>::CHeightMap()
	: CHeightMapBoundsInterface((float)gv::WORLD_CELL_SIZE, WorldMinX, WorldMinY, (float)gv::WORLD_BOUNDS_MIN_Z, WorldMaxX, WorldMaxY, (float)gv::WORLD_BOUNDS_MAX_Z)
	BANK_ONLY(, m_interface((float)gv::WORLD_CELL_SIZE, WorldMinX, WorldMinY, (float)gv::WORLD_BOUNDS_MIN_Z, WorldMaxX, WorldMaxY, (float)gv::WORLD_BOUNDS_MAX_Z))
{
	Clear(true);

	m_enabled = false;

	m_playerLocationMapMinZ = 0.0f;
	m_playerLocationMapMaxZ = 0.0f;
	m_cellI = -1;
	m_cellJ = -1;

	m_worldHeightMapDebugDraw = false;
	m_worldHeightMapDebugDrawExtent = 6.0f;
	m_worldHeightMapDebugDrawSolidTop = false;
	m_worldHeightMapDebugDrawSolidBottom = false;
	m_editOnTheFlyMode = false;
	m_editOnTheFlyOffset = 5.0f;
	m_editOnTheFlyMinHeights = false;
}

template <typename CellType> CHeightMap<CellType>::~CHeightMap()
{
#if HEIGHTMAP_TOOL
	Release(); // we never reload in the final game
#endif // HEIGHTMAP_TOOL
}

template <typename CellType> inline bool CHeightMap<CellType>::GetIndexAtCoord(int& i, int& j, float x, float y) const
{
	i = (int)floorf((x - m_boundsMinX)*m_oneOverCellSizeX);
	j = (int)floorf((y - m_boundsMinY)*m_oneOverCellSizeY);

	return (i >= 0 && j >= 0 && i < m_numCols && j < m_numRows);
}

template <typename CellType> inline bool CHeightMap<CellType>::GetIndexAtBound(int& i0, int& j0, int& i1, int& j1, float x0, float y0, float x1, float y1) const
{
	i0 = Max<int>((int)floorf((x0 - m_boundsMinX)*m_oneOverCellSizeX), 0);
	j0 = Max<int>((int)floorf((y0 - m_boundsMinY)*m_oneOverCellSizeY), 0);
	i1 = Min<int>((int)ceilf ((x1 - m_boundsMinX)*m_oneOverCellSizeX), m_numCols) - 1;
	j1 = Min<int>((int)ceilf ((y1 - m_boundsMinY)*m_oneOverCellSizeY), m_numRows) - 1;

	if (i0 <= i1 && j0 <= j1)
	{
		// TODO -- temp fix for BS#1334511, somehow we're accessing the data out of bounds?
		if (i0 >= 0 && i1 < m_numCols &&
			j0 >= 0 && j1 < m_numRows)
		{
			return true;
		}
		else
		{
			Assertf(0, "GetIndexAtBound: out of bounds! x0=%f, y0=%f, x1=%f, y1=%f, i0=%d, j0=%d, i1=%d, j1=%d", x0, y0, x1, y1, i0, j0, i1, j1);
		}
	}

	return false;
}


template <typename CellType> void CHeightMap<CellType>::SetMinHeightAtCoord(float x, float y, float h)
{
	CellType z = QuantiseCellMin<CellType>(h, m_boundsMinZ, m_boundsMaxZ);

	int i, j;
	if (GetIndexAtCoord(i, j, x, y))
	{
		Assertf(!m_dataRLE, "Can only set heights on non-RLE data");
		if (!m_dataRLE && i >= 0 && i < m_numCols) // no RLE
		{
			m_dataMin[i + j*m_numCols] = z;
		}
	}
}

template <typename CellType> void CHeightMap<CellType>::SetMaxHeightAtCoord(float x, float y, float h)
{
	CellType z = QuantiseCellMax<CellType>(h, m_boundsMinZ, m_boundsMaxZ);

	int i, j;
	if (GetIndexAtCoord(i, j, x, y))
	{
		Assertf(!m_dataRLE, "Can only set heights on non-RLE data");
		if (!m_dataRLE && i >= 0 && i < m_numCols) // no RLE
		{
			m_dataMax[i + j*m_numCols] = z;
		}
	}
}

template <typename CellType> float CHeightMap<CellType>::GetMinHeightAtIndex(int i, int j) const
{
	CellType z = (CellType)0;

	if (j >= 0 && j < m_numRows)
	{
		if (m_dataRLE)
		{
			if (m_dataRLE[j].GetIsWithinRange(i))
			{
				z = m_dataMin[m_dataRLE[j].GetDataOffset(i)];
			}
		}
		else if (i >= 0 && i < m_numCols) // no RLE
		{
			z = m_dataMin[i + j*m_numCols];
		}
	}

	return UnquantiseCell<CellType>(z, m_boundsMinZ, m_boundsMaxZ);
}

template <typename CellType> float CHeightMap<CellType>::GetMaxHeightAtIndex(int i, int j) const
{
	CellType z = (CellType)0;

	if (j >= 0 && j < m_numRows)
	{
		if (m_dataRLE)
		{
			if (m_dataRLE[j].GetIsWithinRange(i))
			{
				z = m_dataMax[m_dataRLE[j].GetDataOffset(i)];
			}
		}
		else if (i >= 0 && i < m_numCols) // no RLE
		{
			z = m_dataMax[i + j*m_numCols];
		}
	}

	return UnquantiseCell<CellType>(z, m_boundsMinZ, m_boundsMaxZ);
}

template <typename CellType> float CHeightMap<CellType>::GetMinHeightAtCoord(float x, float y) const
{
	CellType z = (CellType)0;

	int i, j;
	if (GetIndexAtCoord(i, j, x, y))
	{
		if (m_dataRLE)
		{
			if (m_dataRLE[j].GetIsWithinRange(i))
			{
				z = m_dataMin[m_dataRLE[j].GetDataOffset(i)];
			}
		}
		else // no RLE
		{
			z = m_dataMin[i + j*m_numCols];
		}
	}

	return UnquantiseCell<CellType>(z, m_boundsMinZ, m_boundsMaxZ);
}

template <typename CellType> float CHeightMap<CellType>::GetMaxHeightAtCoord(float x, float y) const
{
	CellType z = (CellType)0;

	int i, j;
	if (GetIndexAtCoord(i, j, x, y))
	{
		if (m_dataRLE)
		{
			if (m_dataRLE[j].GetIsWithinRange(i))
			{
				z = m_dataMax[m_dataRLE[j].GetDataOffset(i)];
			}
		}
		else // no RLE
		{
			z = m_dataMax[i + j*m_numCols];
		}
	}

	return UnquantiseCell<CellType>(z, m_boundsMinZ, m_boundsMaxZ);
}

template <typename CellType> float CHeightMap<CellType>::GetMinHeightAtBound(float x0, float y0, float x1, float y1) const
{
	CellType z = (CellType)0;

	int i0, j0, i1, j1;
	if (GetIndexAtBound(i0, j0, i1, j1, x0, y0, x1, y1))
	{
		bool zvalid = false;
		z = GetCellMaxValue<CellType>(m_boundsMaxZ);

		if (m_dataRLE)
		{
			for (int j = j0; j <= j1; j++)
			{
				const int ii0 = Max<int>(i0, (int)m_dataRLE[j].GetStart());
				const int ii1 = Min<int>(i1, (int)m_dataRLE[j].GetStop());

				const CellType* data = &m_dataMin[m_dataRLE[j].GetDataOffset(ii0)];

				for (int i = ii0; i <= ii1; i++)
				{
					z = Min<CellType>(*(data++), z);
					zvalid = true;
				}
			}
		}
		else // no RLE
		{
			for (int j = j0; j <= j1; j++)
			{
				const CellType* row = &m_dataMin[j*m_numCols];

				for (int i = i0; i <= i1; i++)
				{
					z = Min<CellType>(*(row++), z);
					zvalid = true;
				}
			}
		}

		if (!zvalid)
		{
			z = (CellType)0;
		}
	}

	return UnquantiseCell<CellType>(z, m_boundsMinZ, m_boundsMaxZ);
}

template <typename CellType> float CHeightMap<CellType>::GetMaxHeightAtBound(float x0, float y0, float x1, float y1) const
{
	CellType z = (CellType)0;

	int i0, j0, i1, j1;
	if (GetIndexAtBound(i0, j0, i1, j1, x0, y0, x1, y1))
	{
		if (m_dataRLE)
		{
			for (int j = j0; j <= j1; j++)
			{
				const int ii0 = Max<int>(i0, (int)m_dataRLE[j].GetStart());
				const int ii1 = Min<int>(i1, (int)m_dataRLE[j].GetStop());

				const CellType* data = &m_dataMax[m_dataRLE[j].GetDataOffset(ii0)];

				for (int i = ii0; i <= ii1; i++)
				{
					z = Max<CellType>(*(data++), z);
				}
			}
		}
		else // no RLE
		{
			for (int j = j0; j <= j1; j++)
			{
				const CellType* row = &m_dataMax[j*m_numCols];

				for (int i = i0; i <= i1; i++)
				{
					z = Max<CellType>(*(row++), z);
				}
			}
		}
	}

	return UnquantiseCell<CellType>(z, m_boundsMinZ, m_boundsMaxZ);
}

template <typename CellType> spdAABB CHeightMap<CellType>::GetBoundingBox() const
{
	const Vec3V bmin(m_boundsMinX, m_boundsMinY, m_boundsMinZ);
	const Vec3V bmax(m_boundsMaxX, m_boundsMaxY, m_boundsMaxZ);

	return spdAABB(bmin, bmax);
}

template <typename CellType> bool CHeightMap<CellType>::Load(const char* path)
{
#if __BANK
	strcpy(m_interface.m_path, path);
#endif // __BANK

	fiStream* fd = fiStream::Open(path, true);

	if (fd)
	{
#if HEIGHTMAP_TOOL
		Release(); // we never reload in the final game
#endif // HEIGHTMAP_TOOL

		strcpy(m_path, path);

		CHeightMapHeader header;
		fd->Read(&header, sizeof(header));

		bool bNeedsByteSwap = false;

		if (header.m_tag != 'HMAP')
		{
			u32 swaptag = header.m_tag;
			ByteSwapElement(swaptag);

			if (swaptag == 'HMAP')
			{
				header.ByteSwap();
				bNeedsByteSwap = true;
			}
		}

#if !__FINAL
		bool bSwapMinMax = false;
		bool bLoadZero = false;

		if (header.m_tag != 'HMAP') // old height map data, fixed size, no RLE
		{
			Assertf(0, "height map \"%s\" uses old format", path);

			enum
			{
				WORLD_HTMAP_CELL_SIZE = 50,
				WORLD_HTMAP_MIN_X = -3250,
				WORLD_HTMAP_MIN_Y = -3250,
				WORLD_HTMAP_MIN_Z = 0,
				WORLD_HTMAP_MAX_X = +4000,
				WORLD_HTMAP_MAX_Y = +7250,
				WORLD_HTMAP_MAX_Z = +800,
				WORLD_HTMAP_NUM_SECTORS_X = (WORLD_HTMAP_MAX_X - WORLD_HTMAP_MIN_X)/WORLD_HTMAP_CELL_SIZE,
				WORLD_HTMAP_NUM_SECTORS_Y = (WORLD_HTMAP_MAX_Y - WORLD_HTMAP_MIN_Y)/WORLD_HTMAP_CELL_SIZE,
				WORLD_HTMAP_NUM_SECTORS = WORLD_HTMAP_NUM_SECTORS_X*WORLD_HTMAP_NUM_SECTORS_Y,
			};

			header.m_tag             = 0;
			header.m_version         = 0;
			header.m_cellSizeInBytes = sizeof(u8); // expect u8 data
			header.m_flags           = 0;
			header.m_numCols         = WORLD_HTMAP_NUM_SECTORS_X;
			header.m_numRows         = WORLD_HTMAP_NUM_SECTORS_Y;
			header.m_boundsMinX      = (float)WORLD_HTMAP_MIN_X;
			header.m_boundsMinY      = (float)WORLD_HTMAP_MIN_Y;
			header.m_boundsMinZ      = (float)WORLD_HTMAP_MIN_Z;
			header.m_boundsMaxX      = (float)WORLD_HTMAP_MAX_X;
			header.m_boundsMaxY      = (float)WORLD_HTMAP_MAX_Y;
			header.m_boundsMaxZ      = (float)WORLD_HTMAP_MAX_Z;
			header.m_dataSizeInBytes = WORLD_HTMAP_NUM_SECTORS*sizeof(u8)*2; // expect u8 data

			fd->Seek(0); // rewind

			bSwapMinMax = true;
		}
#endif // !__FINAL

		char* data = NULL;

		if (header.m_cellSizeInBytes == sizeof(CellType))
		{
			data = rage_new char[header.m_dataSizeInBytes];
			fd->Read(data, header.m_dataSizeInBytes);
		}
		else
		{
#if !__FINAL
			bLoadZero = true; // incompatible cell data
#endif // !__FINAL
		}

		m_numCols          = (int)header.m_numCols;
		m_numRows          = (int)header.m_numRows;
		m_cellSizeX        = (header.m_boundsMaxX - header.m_boundsMinX)/(float)header.m_numCols;
		m_cellSizeY        = (header.m_boundsMaxY - header.m_boundsMinY)/(float)header.m_numRows;
		m_boundsMinX       = header.m_boundsMinX;
		m_boundsMinY       = header.m_boundsMinY;
		m_boundsMinZ       = header.m_boundsMinZ;
		m_boundsMaxX       = header.m_boundsMaxX;
		m_boundsMaxY       = header.m_boundsMaxY;
		m_boundsMaxZ       = header.m_boundsMaxZ;
		m_oneOverCellSizeX = ((float)header.m_numCols)/(header.m_boundsMaxX - header.m_boundsMinX);
		m_oneOverCellSizeY = ((float)header.m_numRows)/(header.m_boundsMaxY - header.m_boundsMinY);
#if HEIGHTMAP_TOOL
		m_downsample       = 1;
#endif // HEIGHTMAP_TOOL

#if !__FINAL
		if (bLoadZero) // incompatible, load with zero data (no RLE)
		{
			Assertf(0, "height map \"%s\" uses %d-byte cells, expected %" SIZETFMT "d .. loading as zero data", path, header.m_cellSizeInBytes, sizeof(CellType));

			const int dataSizeInBytes = m_numRows*m_numCols*sizeof(CellType)*2;
			data = rage_new char[dataSizeInBytes];
			sysMemSet(data, 0, dataSizeInBytes);

			m_data    = data;
			m_dataRLE = NULL;
			m_dataMax = (CellType*)data; data += dataSizeInBytes/2;
			m_dataMin = (CellType*)data;
		}
		else
#endif // !__FINAL
		{
			if (header.m_flags & CHeightMapHeader::HMAP_FLAG_RLE_DATA)
			{
				m_data    = data;
				m_dataRLE = (RLE*)data; data += m_numRows*sizeof(RLE);
				m_dataMax = (CellType*)data; data += (header.m_dataSizeInBytes - m_numRows*sizeof(RLE))/2;
				m_dataMin = (CellType*)data;
			}
			else // non-RLE data, load and then convert to RLE if we can
			{
				m_data    = data;
				m_dataRLE = NULL;
				m_dataMax = (CellType*)data; data += header.m_dataSizeInBytes/2;
				m_dataMin = (CellType*)data;
#if !__FINAL
				if (bSwapMinMax) // old format stored min before max .. swap it
				{
					for (int i = 0; i < m_numRows*m_numCols; i++)
					{
						CellType z = m_dataMin[i];
						m_dataMin[i] = m_dataMax[i];
						m_dataMax[i] = z;
					}
				}
#endif // !__FINAL
			}

			if (bNeedsByteSwap)
			{
				int numCells = 0;

				if (m_dataRLE)
				{
					for (int j = 0; j < m_numRows; j++) // .. byte-swap RLE data
					{
						m_dataRLE[j].ByteSwap();

						numCells += m_dataRLE[j].GetCount();
					}
				}
				else
				{
					numCells = m_numRows*m_numCols;
				}

				if (sizeof(CellType) > 1) // only need to byte-swap data if cell type is not u8
				{
					for (int i = 0; i < numCells; i++)
					{
						ByteSwapElement(m_dataMin[i]);
						ByteSwapElement(m_dataMax[i]);
					}
				}
			}

			// check values are sensible (these limits are arbitrary)
			Assert(m_numCols >= 10 && m_numCols <= 1000);
			Assert(m_numRows >= 10 && m_numRows <= 1000);
			Assert(FPIsFinite(m_boundsMinX) && FPIsFinite(m_boundsMaxX));
			Assert(FPIsFinite(m_boundsMinY) && FPIsFinite(m_boundsMaxY));
			Assert(FPIsFinite(m_boundsMinZ) && FPIsFinite(m_boundsMaxZ));
			Assert(-16000.0f <= m_boundsMinX && m_boundsMinX < m_boundsMaxX && m_boundsMaxX <= 16000.0f);
			Assert(-16000.0f <= m_boundsMinY && m_boundsMinY < m_boundsMaxY && m_boundsMaxY <= 16000.0f);
			Assert(-16000.0f <= m_boundsMinZ && m_boundsMinZ < m_boundsMaxZ && m_boundsMaxZ <= 16000.0f);

#if HEIGHTMAP_TOOL
			ConvertToRLE();
#endif // HEIGHTMAP_TOOL
		}

#if __BANK
		m_interface.UpdateFrom(*this);

		// report size
		{
			int numCells = 0;
			
			for (int j = 0; j < m_numRows; j++)
			{
				numCells += m_dataRLE[j].GetCount();
			}

			const int dataSizeInBytes = m_numRows*sizeof(RLE) + numCells*sizeof(CellType)*2;

			Displayf(
				"height map \"%s\" loaded, bounds=[%d..%d][%d..%d], minZ=%d, maxZ=%d, res=%dx%d numCells=%d (%.2fKB)",
				path,
				(int)m_boundsMinX,
				(int)m_boundsMinY,
				(int)m_boundsMinZ,
				(int)m_boundsMaxX,
				(int)m_boundsMaxY,
				(int)m_boundsMaxZ,
				m_numCols,
				m_numRows,
				numCells, 
				(float)dataSizeInBytes/1024.0f
			);
		}
#endif // __BANK

#if HEIGHTMAP_TOOL
		if (0) // we could load the water mask here if we need it .. what could we use this for?
		{
			const int maskSizeInBytes = (m_numRows*m_numCols + 7)/8;

			m_waterMask = rage_new u8[maskSizeInBytes];

			if ((header.m_flags & CHeightMapHeader::HMAP_FLAG_WATER_MASK) != 0 && !bLoadZero)
			{
				fd->Read(m_waterMask, maskSizeInBytes);
			}
			else
			{
				sysMemSet(m_waterMask, 0x00, maskSizeInBytes);
			}
		}
#endif // HEIGHTMAP_TOOL

		fd->Close();

		return true;
	}
	else
	{
		if (strstr(path, "prologue"))
		{
			// don't complain about prologue height map
		}
		else
		{
			Assertf(0, "failed to load height map \"%s\"", path);
		}

		return false;
	}
}

#if HEIGHTMAP_TOOL

template <typename CellType> void CHeightMap<CellType>::Finalise(bool bExpandWorldBounds)
{
	if (!IsEnabled())
	{
		return;
	}

	if (m_moverBoundMask != NULL || m_weaponBoundMask != NULL || m_vehicleBoundMask != NULL) // mover/weapon/vehicle bound masks are exclusive to missing world mask cells
	{
		for (int j = 0; j < m_numRows; j++)
		{
			for (int i = 0; i < m_numCols; i++)
			{
				const int index = i + j*m_numCols;

				if ((m_worldMask[index/8] & BIT(index%8)) == 0)
				{
					if (m_moverBoundMask  ) { m_moverBoundMask  [index/8] |= BIT(index%8); }
					if (m_weaponBoundMask ) { m_weaponBoundMask [index/8] |= BIT(index%8); }
					if (m_vehicleBoundMask) { m_vehicleBoundMask[index/8] |= BIT(index%8); }
				}
			}
		}
	}

	if (bExpandWorldBounds)
	{
		int* worldMinX = rage_new int[m_numRows];
		int* worldMinY = rage_new int[m_numCols];
		int* worldMaxX = rage_new int[m_numRows];
		int* worldMaxY = rage_new int[m_numCols];

		for (int j = 0; j < m_numRows; j++)
		{
			worldMinX[j] = +99999;
			worldMaxX[j] = -99999;
		}

		for (int i = 0; i < m_numCols; i++)
		{
			worldMinY[i] = +99999;
			worldMaxY[i] = -99999;
		}

		for (int j = 0; j < m_numRows; j++)
		{
			for (int i = 0; i < m_numCols; i++)
			{
				const int index = i + j*m_numCols;

				if (m_worldMask[index/8] & BIT(index%8))
				{
					worldMinX[j] = Min<int>(i, worldMinX[j]);
					worldMinY[i] = Min<int>(j, worldMinY[i]);
					worldMaxX[j] = Max<int>(i, worldMaxX[j]);
					worldMaxY[i] = Max<int>(j, worldMaxY[i]);
				}
			}
		}

		for (int j = 0; j < m_numRows; j++)
		{
			for (int i = 0; i < m_numCols; i++)
			{
				if (i < worldMinX[j] || i > worldMaxX[j] ||
					j < worldMinY[i] || j > worldMaxY[i])
				{
					const int index = i + j*m_numCols;

					m_worldMask[index/8] |= BIT(index%8);
				}
			}
		}

		delete[] worldMinX;
		delete[] worldMinY;
		delete[] worldMaxX;
		delete[] worldMaxY;
	}
}

template <typename CellType> void CHeightMap<CellType>::Save(const char* path, bool bSaveWaterMask) const
{
	if (!IsValid())
	{
		return;
	}

	fiStream* fd = fiStream::Create(path);

	if (fd)
	{
		const bool bNeedsByteSwap = !__BE;

		CHeightMapHeader header;
		sysMemSet(&header, 0, sizeof(header));

		header.m_tag             = 'HMAP';
		header.m_version         = 1;
		header.m_cellSizeInBytes = sizeof(CellType);
		header.m_flags           = 0;
		header.m_numCols         = (u16)m_numCols;
		header.m_numRows         = (u16)m_numRows;
		header.m_boundsMinX      = m_boundsMinX;
		header.m_boundsMinY      = m_boundsMinY;
		header.m_boundsMinZ      = m_boundsMinZ;
		header.m_boundsMaxX      = m_boundsMaxX;
		header.m_boundsMaxY      = m_boundsMaxY;
		header.m_boundsMaxZ      = m_boundsMaxZ;

		int numCells = 0;

		if (m_waterMask == NULL)
		{
			bSaveWaterMask = false;
		}

		if (bSaveWaterMask)
		{
			header.m_flags |= CHeightMapHeader::HMAP_FLAG_WATER_MASK;
		}

		if (m_dataRLE)
		{
			for (int j = 0; j < m_numRows; j++)
			{
				numCells += m_dataRLE[j].GetCount();
			}

			header.m_flags |= CHeightMapHeader::HMAP_FLAG_RLE_DATA;
			header.m_dataSizeInBytes = m_numRows*sizeof(RLE) + numCells*sizeof(CellType)*2;

			if (bNeedsByteSwap)
			{
				header.ByteSwap();
				fd->Write(&header, sizeof(header));

				RLE* tempRLE = rage_new RLE[m_numRows];
				sysMemCpy(tempRLE, m_dataRLE, m_numRows*sizeof(RLE));
				for (int j = 0; j < m_numRows; j++) { tempRLE[j].ByteSwap(); }
				fd->Write(tempRLE, m_numRows*sizeof(RLE));
				delete[] tempRLE;
			}
			else
			{
				fd->Write(&header, sizeof(header));
				fd->Write(m_dataRLE, m_numRows*sizeof(RLE));
			}
		}
		else // no RLE
		{
			numCells = m_numRows*m_numCols;

			header.m_dataSizeInBytes = numCells*sizeof(CellType)*2;

			if (bNeedsByteSwap)
			{
				header.ByteSwap();
				fd->Write(&header, sizeof(header));
			}
			else
			{
				fd->Write(&header, sizeof(header));
			}
		}

		if (bNeedsByteSwap && sizeof(CellType) > 1) // only need to byte-swap data if cell type is not u8
		{
			CellType* tempData = rage_new CellType[numCells];

			sysMemCpy(tempData, m_dataMax, numCells*sizeof(CellType));
			for (int i = 0; i < numCells; i++) { ByteSwapElement(tempData[i]); }
			fd->Write(tempData, numCells*sizeof(CellType));

			sysMemCpy(tempData, m_dataMin, numCells*sizeof(CellType));
			for (int i = 0; i < numCells; i++) { ByteSwapElement(tempData[i]); }
			fd->Write(tempData, numCells*sizeof(CellType));

			delete[] tempData;
		}
		else
		{
			fd->Write(m_dataMax, numCells*sizeof(CellType));
			fd->Write(m_dataMin, numCells*sizeof(CellType));
		}

		if (bSaveWaterMask)
		{
			const int maskSizeInBytes = (m_numRows*m_numCols + 7)/8;

			fd->Write(m_waterMask, maskSizeInBytes);
		}

		fd->Close();
	}
	else
	{
		Assertf(0, "failed to save height map \"%s\"", path);
	}
}

static void CHeightMap_ImportDDSMask(u8* mask, const char* path, int numCols, int numRows)
{
	const int maskSizeInBytes = (numRows*numCols + 7)/8;

	sysMemSet(mask, 0, maskSizeInBytes);

	grcImage* image = grcImage::LoadDDS(path);

	if (image)
	{
		if (image->GetWidth()  != numCols ||
			image->GetHeight() != numRows ||
			image->GetFormat() != grcImage::L8)
		{
			Assertf(0, "mask \"%s\" (%dx%d %s) is incompatible",
				path,
				image->GetWidth(),
				image->GetHeight(),
				grcImage::GetFormatString(image->GetFormat())
			);
		}
		else
		{
			for (int j = 0; j < numRows; j++)
			{
				const u8* row = (const u8*)image->GetBits() + (numRows - j - 1)*numCols;

				for (int i = 0; i < numCols; i++)
				{
					if (row[i] > 0)
					{
						const int index = i + j*numCols;

						mask[index/8] |= BIT(index%8);
					}
				}
			}
		}

		image->Release();
	}
	else
	{
		Assertf(0, "failed to load mask \"%s\"", path);
	}
}

template <typename CellType> void CHeightMap<CellType>::ImportDDS(const char* path)
{
#if __BANK
	strcpy(m_interface.m_pathImportExportDDS, path);
#endif // __BANK

	const atVarString pathname_dataMin("%s_min.dds", path);
	const atVarString pathname_dataMax("%s_max.dds", path);

	grcImage* image_dataMin = grcImage::LoadDDS(pathname_dataMin);
	grcImage* image_dataMax = grcImage::LoadDDS(pathname_dataMax);

	if (image_dataMin == NULL)
	{
		Assertf(0, "failed to load min heightmap \"%s\"", pathname_dataMin.c_str());

		if (image_dataMax)
		{
			image_dataMax->Release();
		}

		return;
	}

	if (image_dataMax == NULL)
	{
		Assertf(0, "failed to load max heightmap \"%s\"", pathname_dataMax.c_str());

		if (image_dataMin)
		{
			image_dataMin->Release();
		}

		return;
	}

	grcImage::Format imageFormat = grcImage::UNKNOWN;

	switch (sizeof(CellType))
	{
	case 1: imageFormat = grcImage::L8; break;
	case 2: imageFormat = grcImage::L16; break;
	case 4: imageFormat = grcImage::R32F; break;
	}

	if (image_dataMin->GetWidth()  != image_dataMax->GetWidth() ||
		image_dataMin->GetHeight() != image_dataMax->GetHeight() ||
		image_dataMin->GetFormat() != imageFormat ||
		image_dataMax->GetFormat() != imageFormat)
	{
		Assertf(0, "min heightmap \"%s\" (%dx%d %s) and max heightmap \"%s\" (%dx%d %s) are not compatible",
			pathname_dataMin.c_str(),
			image_dataMin->GetWidth(),
			image_dataMin->GetHeight(),
			grcImage::GetFormatString(image_dataMin->GetFormat()),
			pathname_dataMax.c_str(),
			image_dataMax->GetWidth(),
			image_dataMax->GetHeight(),
			grcImage::GetFormatString(image_dataMax->GetFormat())
		);

		image_dataMin->Release();
		image_dataMax->Release();
		return;
	}

	Reset(
		image_dataMin->GetWidth(),
		image_dataMin->GetHeight(),
		1,
		m_boundsMinX,
		m_boundsMinY,
		m_boundsMinZ,
		m_boundsMaxX,
		m_boundsMaxY,
		m_boundsMaxZ,
		NULL, // water boundary path
		(eCreateHeightmapFlags)(CREATE_HEIGHTMAP | CREATE_WATERMASK)
	);

	for (int j = 0; j < m_numRows; j++)
	{
		sysMemCpy(&m_dataMax[j*m_numCols], (const CellType*)image_dataMax->GetBits() + (m_numRows - j - 1)*m_numCols, m_numCols*sizeof(CellType));
		sysMemCpy(&m_dataMin[j*m_numCols], (const CellType*)image_dataMin->GetBits() + (m_numRows - j - 1)*m_numCols, m_numCols*sizeof(CellType));
	}

	if (CellTypeIsFloat<CellType>())
	{
		for (int i = 0; i < m_numRows*m_numCols; i++)
		{
			m_dataMax[i] = (CellType)m_boundsMinZ + m_dataMax[i]*(CellType)(m_boundsMaxZ - m_boundsMinZ);
			m_dataMin[i] = (CellType)m_boundsMinZ + m_dataMin[i]*(CellType)(m_boundsMaxZ - m_boundsMinZ);
		}
	}

	image_dataMin->Release();
	image_dataMax->Release();

	ConvertToRLE();

	if (m_waterMask)
	{
		CHeightMap_ImportDDSMask(m_waterMask, atVarString("%s_water.dds", path).c_str(), m_numCols, m_numRows);
	}
}

template <typename CellType> static void CHeightMap_ExportDDSData(const CellType* data, const char* path, int numCols, int numRows, int downsample, grcImage*& image, const u8* worldMask, float boundsMinZ, float boundsMaxZ, bool bIsDataMin, bool bIsRawData, bool bAutoImageRange)
{
	if (data == NULL)
	{
		return;
	}

	const int wd = numCols/downsample;
	const int hd = numRows/downsample;

	grcImage::Format imageFormat = grcImage::UNKNOWN;

	switch (sizeof(CellType))
	{
	case 1: imageFormat = grcImage::L8; break;
	case 2: imageFormat = grcImage::L16; break;
	case 4: imageFormat = grcImage::R32F; break;
	}

	if (image)
	{
		if (image->GetWidth()  != wd ||
			image->GetHeight() != hd ||
			image->GetFormat() != imageFormat)
		{
			image->Release();
			image = NULL;
		}
	}

	if (image == NULL)
	{
		image = grcImage::Create(wd, hd, 1, imageFormat, grcImage::STANDARD, 0, 0);
	}

	CellType rangeMin = (CellType)0;
	CellType rangeMax = (CellType)0;

	if (bAutoImageRange)
	{
		int numCells = 0;

		rangeMin = data[0];
		rangeMax = data[0];

		for (int j = 0; j < numRows; j++)
		{
			for (int i = 0; i < numCols; i++)
			{
				const int index = i + j*numCols;

				if (worldMask == NULL || (worldMask[index/8] & BIT(index%8)) != 0)
				{
					rangeMin = Min<CellType>(data[i + j*numCols], rangeMin);
					rangeMax = Max<CellType>(data[i + j*numCols], rangeMax);
					numCells++;
				}
			}
		}
	}

	for (int j = 0; j < numRows; j += downsample)
	{
		for (int i = 0; i < numCols; i += downsample)
		{
			const int id = i/downsample;
			const int jd = j/downsample;

			CellType p = (CellType)0;
			float q = 0.0f;
			bool bDataIsEmpty = true;

			for (int jj = 0; jj < downsample; jj++)
			{
				for (int ii = 0; ii < downsample; ii++)
				{
					if (i + ii < numCols &&
						j + jj < numRows)
					{
						const int index = (i + ii) + (j + jj)*numCols;

						if (worldMask == NULL || (worldMask[index/8] & BIT(index%8)))
						{
							if (bDataIsEmpty)
							{
								bDataIsEmpty = false;
								p = data[index];
								q = 1.0f;
							}
							else
							{
								if      (bIsRawData) { p += data[index]; q += 1.0f; }
								else if (bIsDataMin) { p = Min<CellType>(data[index], p); }
								else                 { p = Max<CellType>(data[index], p); }
							}
						}
					}
				}
			}

			if (bAutoImageRange)
			{
				p = QuantiseCellAvg<CellType>((float)(p - rangeMin)/(float)(rangeMax - rangeMin), 0.0f, 1.0f);
			}
			else if (bIsRawData)
			{
				if (q > 0.0f)
				{
					p /= (CellType)q;
				}
			}
			else if (CellTypeIsFloat<CellType>())
			{
				p = (p - (CellType)boundsMinZ)/(CellType)(boundsMaxZ - boundsMinZ);
			}

			if (CellTypeIsFloat<CellType>() && bDataIsEmpty && !bIsRawData)
			{
				p = GetCellEmptyValue<CellType>(); // empty water cell?
			}

			((CellType*)image->GetBits())[id + (hd - jd - 1)*(wd)] = p;
		}
	}

#if !__WIN32PC
	Displayf("exporting \"%s\"", path);
#endif // !__WIN32PC
	image->SaveDDS(path);
}

static void CHeightMap_ExportDDSMask(const u8* mask, const char* path, int numCols, int numRows, int downsample, grcImage*& image, const u8* secondaryMask = NULL)
{
	if (mask == NULL)
	{
		return;
	}

	const int wd = numCols/downsample;
	const int hd = numRows/downsample;

	grcImage::Format imageFormat = grcImage::L8;

	if (downsample > 12) // requires higher precision
	{
		imageFormat = grcImage::L16; 
	}

	if (image)
	{
		if (image->GetWidth()  != wd ||
			image->GetHeight() != hd ||
			image->GetFormat() != imageFormat)
		{
			image->Release();
			image = NULL;
		}
	}

	if (image == NULL)
	{
		image = grcImage::Create(wd, hd, 1, imageFormat, grcImage::STANDARD, 0, 0);
	}

	bool bEmpty = true;

	for (int j = 0; j < numRows; j += downsample)
	{
		for (int i = 0; i < numCols; i += downsample)
		{
			const int id = i/downsample;
			const int jd = j/downsample;

			int p = 0;
			int q = 0;

			int p_secondary = 0;

			for (int jj = 0; jj < downsample; jj++)
			{
				for (int ii = 0; ii < downsample; ii++)
				{
					if (i + ii < numCols &&
						j + jj < numRows)
					{
						const int index = (i + ii) + (j + jj)*numCols;

						if (mask[index/8] & BIT(index%8))
						{
							p++;
							bEmpty = false;
						}

						if (secondaryMask && (secondaryMask[index/8] & BIT(index%8)) != 0)
						{
							p_secondary++;
							bEmpty = false;
						}

						q++;
					}
				}
			}

			float value = 0.0f;

			if (q > 0)
			{
				value = Max<float>((float)p, 0.25f*(float)p_secondary)/(float)q;
			}

			if (imageFormat == grcImage::L8)
			{
				((u8*)image->GetBits())[id + (hd - jd - 1)*(wd)] = (u8)(0.5f + 255.0f*value);
			}
			else if (imageFormat == grcImage::L16)
			{
				((u16*)image->GetBits())[id + (hd - jd - 1)*(wd)] = (u16)(0.5f + 65535.0f*value);
			}
		}
	}

	if (!bEmpty)
	{
#if !__WIN32PC
		Displayf("exporting \"%s\"", path);
#endif // !__WIN32PC
		image->SaveDDS(path);
	}
}

template <typename T> static void CHeightMap_ExportDDSMaskRaw(const T* mask, const char* path, int numCols, int numRows, int downsample, grcImage*& image)
{
	if (mask == NULL)
	{
		return;
	}

	const int wd = numCols/downsample;
	const int hd = numRows/downsample;

	grcImage::Format imageFormat = grcImage::UNKNOWN;
	
	switch (sizeof(T))
	{
	case sizeof(u64): imageFormat = grcImage::A16B16G16R16; break;
	case sizeof(u32): imageFormat = grcImage::A8R8G8B8; break;
	case sizeof(u16): imageFormat = grcImage::L16; break;
	}

	if (image)
	{
		if (image->GetWidth()  != wd ||
			image->GetHeight() != hd ||
			image->GetFormat() != imageFormat)
		{
			image->Release();
			image = NULL;
		}
	}

	if (image == NULL)
	{
		image = grcImage::Create(wd, hd, 1, imageFormat, grcImage::STANDARD, 0, 0);
	}

	bool bEmpty = true;

	for (int j = 0; j < numRows; j += downsample)
	{
		for (int i = 0; i < numCols; i += downsample)
		{
			const int id = i/downsample;
			const int jd = j/downsample;

			T bits = 0;

			for (int jj = 0; jj < downsample; jj++)
			{
				for (int ii = 0; ii < downsample; ii++)
				{
					if (i + ii < numCols &&
						j + jj < numRows)
					{
						bits |= mask[(i + ii) + (j + jj)*numCols];
					}
				}
			}

			((T*)image->GetBits())[id + (hd - jd - 1)*(wd)] = bits;
			
			if (bits)
			{
				bEmpty = false;
			}
		}
	}

	if (!bEmpty)
	{
#if !__WIN32PC
		Displayf("exporting \"%s\"", path);
#endif // !__WIN32PC
		image->SaveDDS(path);
	}
}

template <typename CellType> static void CHeightMap_ExportDDSDensityMap(const float* data, const char* path, int numCols, int numRows, int downsample, grcImage*& image, const u8* worldMask)
{
	if (data == NULL)
	{
		return;
	}

	const int wd = numCols/downsample;
	const int hd = numRows/downsample;

	grcImage::Format imageFormat = grcImage::UNKNOWN;

	switch (sizeof(CellType))
	{
	case 1: imageFormat = grcImage::L8; break;
	case 2: imageFormat = grcImage::L16; break;
	case 4: imageFormat = grcImage::R32F; break;
	}

	if (image)
	{
		if (image->GetWidth()  != wd ||
			image->GetHeight() != hd ||
			image->GetFormat() != imageFormat)
		{
			image->Release();
			image = NULL;
		}
	}

	if (image == NULL)
	{
		image = grcImage::Create(wd, hd, 1, imageFormat, grcImage::STANDARD, 0, 0);
	}

	sysMemSet(image->GetBits(), 0, wd*hd*sizeof(CellType));

	float valueMin = +FLT_MAX;
	float valueMax = -FLT_MAX;

	for (int pass = 0; pass < 2; pass++)
	{
		if (1) // hardcoded density range, works for gta5 map (this is required for tiles)
		{
			if (pass == 0)
			{
				valueMin = -5.0f;
				valueMax = logf(1.0f/DENSITY_FUNC(DENSITY_MIN_AREA));
				continue;
			}
		}
		else if (pass == 1)
		{
			Displayf("density min=%f, max=%f\n", valueMin, valueMax);
		}

		for (int j = 0; j < numRows; j += downsample)
		{
			for (int i = 0; i < numCols; i += downsample)
			{
				const int id = i/downsample;
				const int jd = j/downsample;

				float p = 0.0f;
				float q = 0.0f;

				for (int jj = 0; jj < downsample; jj++)
				{
					for (int ii = 0; ii < downsample; ii++)
					{
						if (i + ii < numCols &&
							j + jj < numRows)
						{
							const int index = (i + ii) + (j + jj)*numCols;

							if ((worldMask[index/8] & BIT(index%8)) != 0 && data[index] > 0.0f)//0.001f)
							{
								p += logf(data[index]);
								q += 1.0f;
							}
						}
					}
				}

				if (q > 0.0f)
				{
					const float value = p/q;

					if (pass == 0)
					{
						valueMin = Min<float>(value, valueMin);
						valueMax = Max<float>(value, valueMax);
					}
					else
					{
						if (valueMin >= valueMax)
						{
							valueMin = 0.0f;
							valueMax = 1.0f;
						}

						const float v = (value - valueMin)/(valueMax - valueMin);

						((CellType*)image->GetBits())[id + (hd - jd - 1)*(wd)] = QuantiseCellAvg<CellType>(v, 0.0f, 1.0f);
					}
				}
			}
		}
	}

	// invert
	{
		if (image->GetFormat() == grcImage::L8)
		{
			u8* data = (u8*)image->GetBits();

			for (int i = 0; i < wd*hd; i++)
			{
				data[i] ^= 0xff;
			}
		}
		else if (image->GetFormat() == grcImage::L16)
		{
			u16* data = (u16*)image->GetBits();

			for (int i = 0; i < wd*hd; i++)
			{
				data[i] ^= 0xffff;
			}
		}
		else if (image->GetFormat() == grcImage::R32F)
		{
			float* data = (float*)image->GetBits();

			for (int i = 0; i < wd*hd; i++)
			{
				data[i] = 1.0f - data[i];
			}
		}
	}

#if !__WIN32PC
	Displayf("exporting \"%s\"", path);
#endif // !__WIN32PC
	image->SaveDDS(path);
}

template <typename CellType> static void CHeightMap_ExportDDSDensityMap(const u8* data, const char* path, int numCols, int numRows, int downsample, grcImage*& image, const u8* worldMask, bool bNormalise)
{
	if (data == NULL)
	{
		return;
	}

	const int wd = numCols/downsample;
	const int hd = numRows/downsample;

	grcImage::Format imageFormat = grcImage::UNKNOWN;

	switch (sizeof(CellType))
	{
	case 1: imageFormat = grcImage::L8; break;
	case 2: imageFormat = grcImage::L16; break;
	case 4: imageFormat = grcImage::R32F; break;
	}

	if (image)
	{
		if (image->GetWidth()  != wd ||
			image->GetHeight() != hd ||
			image->GetFormat() != imageFormat)
		{
			image->Release();
			image = NULL;
		}
	}

	if (image == NULL)
	{
		image = grcImage::Create(wd, hd, 1, imageFormat, grcImage::STANDARD, 0, 0);
	}

	sysMemSet(image->GetBits(), 0, wd*hd*sizeof(CellType));

	float valueMin = +FLT_MAX;
	float valueMax = -FLT_MAX;

	for (int pass = 0; pass < 2; pass++)
	{
		if (pass == 0 && !bNormalise)
		{
			valueMin = 0.0f;
			valueMax = 255.0f;
			continue;
		}

		for (int j = 0; j < numRows; j += downsample)
		{
			for (int i = 0; i < numCols; i += downsample)
			{
				const int id = i/downsample;
				const int jd = j/downsample;

				float p = 0.0f;
				float q = 0.0f;

				for (int jj = 0; jj < downsample; jj++)
				{
					for (int ii = 0; ii < downsample; ii++)
					{
						if (i + ii < numCols &&
							j + jj < numRows)
						{
							const int index = (i + ii) + (j + jj)*numCols;

							if (worldMask[index/8] & BIT(index%8))
							{
								p += (float)data[index];
								q += 1.0f;
							}
						}
					}
				}

				if (q > 0.0f)
				{
					const float value = p/q;

					if (pass == 0)
					{
						valueMin = Min<float>(value, valueMin);
						valueMax = Max<float>(value, valueMax);
					}
					else
					{
						if (valueMin >= valueMax)
						{
							valueMin = 0.0f;
							valueMax = 1.0f;
						}

						const float v = (value - valueMin)/(valueMax - valueMin);

						((CellType*)image->GetBits())[id + (hd - jd - 1)*(wd)] = QuantiseCellAvg<CellType>(v, 0.0f, 1.0f);
					}
				}
			}
		}
	}

	// invert
	{
		if (image->GetFormat() == grcImage::L8)
		{
			u8* data = (u8*)image->GetBits();

			for (int i = 0; i < wd*hd; i++)
			{
				data[i] ^= 0xff;
			}
		}
		else if (image->GetFormat() == grcImage::L16)
		{
			u16* data = (u16*)image->GetBits();

			for (int i = 0; i < wd*hd; i++)
			{
				data[i] ^= 0xffff;
			}
		}
		else if (image->GetFormat() == grcImage::R32F)
		{
			float* data = (float*)image->GetBits();

			for (int i = 0; i < wd*hd; i++)
			{
				data[i] = 1.0f - data[i];
			}
		}
	}

#if !__WIN32PC
	Displayf("exporting \"%s\"", path);
#endif // !__WIN32PC
	image->SaveDDS(path);
}

static void CHeightMap_ExportDDSIDMap(const u32* data, const char* path, int numCols, int numRows, int downsample, grcImage*& image)
{
	if (data == NULL)
	{
		return;
	}

	const int wd = numCols/downsample;
	const int hd = numRows/downsample;

	const grcImage::Format imageFormat = grcImage::A8R8G8B8;

	if (image)
	{
		if (image->GetWidth()  != wd ||
			image->GetHeight() != hd ||
			image->GetFormat() != imageFormat)
		{
			image->Release();
			image = NULL;
		}
	}

	if (image == NULL)
	{
		image = grcImage::Create(wd, hd, 1, imageFormat, grcImage::STANDARD, 0, 0);
	}

	sysMemSet(image->GetBits(), 0, wd*hd*sizeof(u32));

	for (int j = 0; j < numRows; j += downsample)
	{
		for (int i = 0; i < numCols; i += downsample)
		{
			const int id = i/downsample;
			const int jd = j/downsample;

			Vec4V p(V_ZERO);
			float q = 0.0f;

			for (int jj = 0; jj < downsample; jj++)
			{
				for (int ii = 0; ii < downsample; ii++)
				{
					if (i + ii < numCols &&
						j + jj < numRows)
					{
						const int index = (i + ii) + (j + jj)*numCols;

						p += Color32(data[index]).GetRGBA();
						q += 1.0f;
					}
				}
			}

			((Color32*)image->GetBits())[id + (hd - jd - 1)*(wd)] = Color32(p*ScalarV(1.0f/q));
		}
	}

#if !__WIN32PC
	Displayf("exporting \"%s\"", path);
#endif // !__WIN32PC
	image->SaveDDS(path);
}

template <typename CellType> void CHeightMap<CellType>::ExportDDS(const char* path, int downsampleData, int downsampleMask, bool bAutoImageRange, bool bIgnoreWorldMaskForHeightMap) const
{
	if (!Verifyf(m_dataRLE == NULL, "can't export RLE data as DDS"))
	{
		return;
	}

	grcImage* image = NULL;

	if (m_downsample > 1)
	{
		downsampleData *= m_downsample;
		downsampleMask *= m_downsample;
	}

	if (downsampleData > 0)
	{
		const u8* worldMask = bIgnoreWorldMaskForHeightMap ? NULL : m_worldMask;
		bool bIsRawData = false;

		CHeightMap_ExportDDSData(m_dataMin, atVarString("%s_min.dds", path).c_str(), m_numCols, m_numRows, downsampleData, image, worldMask, m_boundsMinZ, m_boundsMaxZ, true, bIsRawData, bAutoImageRange);
		CHeightMap_ExportDDSData(m_dataMax, atVarString("%s_max.dds", path).c_str(), m_numCols, m_numRows, downsampleData, image, worldMask, m_boundsMinZ, m_boundsMaxZ, false, bIsRawData, bAutoImageRange);
	}

	if (downsampleMask > 0)
	{
		CHeightMap_ExportDDSMask(m_worldMask, atVarString("%s_wld.dds", path).c_str(), m_numCols, m_numRows, downsampleMask, image);

		// masks
		{
			CHeightMap_ExportDDSMask(m_waterMask,                atVarString("%s_water.dds",           path).c_str(), m_numCols, m_numRows, downsampleMask, image);
			CHeightMap_ExportDDSMask(m_stairsMask,               atVarString("%s_stairs.dds",          path).c_str(), m_numCols, m_numRows, downsampleMask, image);
			CHeightMap_ExportDDSMask(m_stairSlopeMask,           atVarString("%s_stairslope.dds",      path).c_str(), m_numCols, m_numRows, downsampleMask, image);
			CHeightMap_ExportDDSMask(m_roadsMask,                atVarString("%s_roads.dds",           path).c_str(), m_numCols, m_numRows, downsampleMask, image);
			CHeightMap_ExportDDSMask(m_scriptMask,               atVarString("%s_script.dds",          path).c_str(), m_numCols, m_numRows, downsampleMask, image);
			CHeightMap_ExportDDSMask(m_moverBoundMask,           atVarString("%s_mov.dds",             path).c_str(), m_numCols, m_numRows, downsampleMask, image);
			CHeightMap_ExportDDSMask(m_weaponBoundMask,          atVarString("%s_wpn.dds",             path).c_str(), m_numCols, m_numRows, downsampleMask, image);
			CHeightMap_ExportDDSMask(m_vehicleBoundMask,         atVarString("%s_veh.dds",             path).c_str(), m_numCols, m_numRows, downsampleMask, image);
			CHeightMap_ExportDDSMask(m_moverNoVehicleBoundMaskM, atVarString("%s_movnoveh_m.dds",      path).c_str(), m_numCols, m_numRows, downsampleMask, image);
			CHeightMap_ExportDDSMask(m_moverNoVehicleBoundMaskP, atVarString("%s_movnoveh_p.dds",      path).c_str(), m_numCols, m_numRows, downsampleMask, image);
			CHeightMap_ExportDDSMask(m_boundTypeMask_BOX,        atVarString("%s_type_box.dds",        path).c_str(), m_numCols, m_numRows, downsampleMask, image);
			CHeightMap_ExportDDSMask(m_boundTypeMask_CYLINDER,   atVarString("%s_type_cyl.dds",        path).c_str(), m_numCols, m_numRows, downsampleMask, image);
			CHeightMap_ExportDDSMask(m_boundTypeMask_CAPSULE,    atVarString("%s_type_cap.dds",        path).c_str(), m_numCols, m_numRows, downsampleMask, image);
			CHeightMap_ExportDDSMask(m_boundTypeMask_SPHERE,     atVarString("%s_type_sph.dds",        path).c_str(), m_numCols, m_numRows, downsampleMask, image);
			CHeightMap_ExportDDSMask(m_boundTypeMask_nonBVHPRIM, atVarString("%s_type_nonbvhprim.dds", path).c_str(), m_numCols, m_numRows, downsampleMask, image);
			CHeightMap_ExportDDSMask(m_exteriorPortalMask,       atVarString("%s_interiors.dds",       path).c_str(), m_numCols, m_numRows, downsampleMask, image, m_interiorMask);
			CHeightMap_ExportDDSMask(m_waterCollisionMask,       atVarString("%s_watercoll.dds",       path).c_str(), m_numCols, m_numRows, downsampleMask, image);
			CHeightMap_ExportDDSMask(m_waterOccluderMask,        atVarString("%s_wateroccl.dds",       path).c_str(), m_numCols, m_numRows, downsampleMask, image);
			CHeightMap_ExportDDSMask(m_waterDrawableMask,        atVarString("%s_waterdraw.dds",       path).c_str(), m_numCols, m_numRows, downsampleMask, image);
			CHeightMap_ExportDDSMask(m_waterOceanMask,           atVarString("%s_waterocean.dds",      path).c_str(), m_numCols, m_numRows, downsampleMask, image, m_waterOceanGeometryMask);
			CHeightMap_ExportDDSMask(m_waterNoReflectionMask,    atVarString("%s_waternoreflect.dds",  path).c_str(), m_numCols, m_numRows, downsampleMask, image);
			CHeightMap_ExportDDSData(m_underwaterMap,            atVarString("%s_maxneg.dds",          path).c_str(), m_numCols, m_numRows, downsampleMask, image, NULL, m_boundsMinZ, m_boundsMaxZ, false, false, false);
			CHeightMap_ExportDDSData(m_waterHeightMap,           atVarString("%s_waterheight.dds",     path).c_str(), m_numCols, m_numRows, downsampleData, image, NULL, m_boundsMinZ, m_boundsMaxZ, false, true, false);
			CHeightMap_ExportDDSMask(m_waterPortalMask,          atVarString("%s_waterportal.dds",     path).c_str(), m_numCols, m_numRows, downsampleMask, image);
			CHeightMap_ExportDDSMask(m_mirrorDrawableMask,       atVarString("%s_mirrordraw.dds",      path).c_str(), m_numCols, m_numRows, downsampleMask, image);
			CHeightMap_ExportDDSMask(m_mirrorPortalMask,         atVarString("%s_mirrorportal.dds",    path).c_str(), m_numCols, m_numRows, downsampleMask, image);
			CHeightMap_ExportDDSMask(m_cableMask,                atVarString("%s_cables.dds",          path).c_str(), m_numCols, m_numRows, downsampleMask, image);
			CHeightMap_ExportDDSMask(m_coronaMask,               atVarString("%s_coronas.dds",         path).c_str(), m_numCols, m_numRows, downsampleMask, image);
			CHeightMap_ExportDDSMask(m_animatedBuildingMask,     atVarString("%s_animbldg.dds",        path).c_str(), m_numCols, m_numRows, downsampleMask, image);

			CHeightMap_ExportDDSMaskRaw<u64>(m_materialsMask, atVarString("%s_materials.dds", path).c_str(), m_numCols, m_numRows, downsampleMask, image);
			CHeightMap_ExportDDSMaskRaw<u16>(m_matProcIdMask, atVarString("%s_matproc.dds",   path).c_str(), m_numCols, m_numRows, downsampleMask, image);

			for (int i = 0; i < m_matProcMaskCount; i++)
			{
				CHeightMap_ExportDDSMaskRaw<u64>(m_matProcMasks[i], atVarString("%s_materials_%04d.dds", path, i).c_str(), m_numCols, m_numRows, downsampleMask, image);
			}

			CHeightMap_ExportDDSMaskRaw<u16>(m_propGroupMaskMap, atVarString("%s_propgroupmask.dds", path).c_str(), m_numCols, m_numRows, downsampleMask, image);

			CHeightMap_ExportDDSDensityMap<u8> (m_pedDensityMap,             atVarString("%s_den_ped.dds",      path).c_str(), m_numCols, m_numRows, downsampleMask, image, m_worldMask, false);
			CHeightMap_ExportDDSDensityMap<u8> (m_moverBoundPolyDensityMap,  atVarString("%s_den_mov_poly.dds", path).c_str(), m_numCols, m_numRows, downsampleMask, image, m_worldMask, false);
			CHeightMap_ExportDDSDensityMap<u8> (m_moverBoundPrimDensityMap,  atVarString("%s_den_mov_prim.dds", path).c_str(), m_numCols, m_numRows, downsampleMask, image, m_worldMask, false);
			CHeightMap_ExportDDSDensityMap<u8> (m_weaponBoundPolyDensityMap, atVarString("%s_den_wpn_poly.dds", path).c_str(), m_numCols, m_numRows, downsampleMask, image, m_worldMask, false);
			CHeightMap_ExportDDSDensityMap<u8> (m_weaponBoundPrimDensityMap, atVarString("%s_den_wpn_prim.dds", path).c_str(), m_numCols, m_numRows, downsampleMask, image, m_worldMask, false);
			CHeightMap_ExportDDSDensityMap<u8> (m_lightDensityMap,           atVarString("%s_den_light.dds",    path).c_str(), m_numCols, m_numRows, downsampleMask, image, m_worldMask, false);
			CHeightMap_ExportDDSDensityMap<u16>(m_densityMap,                atVarString("%s_den.dds",          path).c_str(), m_numCols, m_numRows, downsampleMask, image, m_worldMask);

#if HEIGHTMAP_TOOL_DENSITY_MAP_TEST
			CHeightMap_ExportDDSData<float>(m_densityMapA, atVarString("%s_denA.dds", path).c_str(), m_numCols, m_numRows, 1, image, m_worldMask, 0.0f, 1.0f, false, true, false);
			CHeightMap_ExportDDSData<float>(m_densityMapB, atVarString("%s_denB.dds", path).c_str(), m_numCols, m_numRows, 1, image, m_worldMask, 0.0f, 1.0f, false, true, false);
			CHeightMap_ExportDDSData<float>(m_densityMapC, atVarString("%s_denC.dds", path).c_str(), m_numCols, m_numRows, 1, image, m_worldMask, 0.0f, 1.0f, false, true, false);
			CHeightMap_ExportDDSData<float>(m_densityMapD, atVarString("%s_denD.dds", path).c_str(), m_numCols, m_numRows, 1, image, m_worldMask, 0.0f, 1.0f, false, true, false);
			CHeightMap_ExportDDSData<float>(m_densityMapE, atVarString("%s_denE.dds", path).c_str(), m_numCols, m_numRows, 1, image, m_worldMask, 0.0f, 1.0f, false, true, false);
			CHeightMap_ExportDDSData<float>(m_densityMapF, atVarString("%s_denF.dds", path).c_str(), m_numCols, m_numRows, 1, image, m_worldMask, 0.0f, 1.0f, false, true, false);
			CHeightMap_ExportDDSData<float>(m_densityMapG, atVarString("%s_denG.dds", path).c_str(), m_numCols, m_numRows, 1, image, m_worldMask, 0.0f, 1.0f, false, true, false);
			CHeightMap_ExportDDSData<float>(m_densityMapH, atVarString("%s_denH.dds", path).c_str(), m_numCols, m_numRows, 1, image, m_worldMask, 0.0f, 1.0f, false, true, false);
			CHeightMap_ExportDDSData<float>(m_densityMapI, atVarString("%s_denI.dds", path).c_str(), m_numCols, m_numRows, 1, image, m_worldMask, 0.0f, 1.0f, false, true, false);
#endif // HEIGHTMAP_TOOL_DENSITY_MAP_TEST

			CHeightMap_ExportDDSIDMap(m_boundIDRpfMap, atVarString("%s_regionID.dds",    path).c_str(), m_numCols, m_numRows, downsampleMask, image);
			CHeightMap_ExportDDSIDMap(m_boundIDMaxMap, atVarString("%s_boundID.dds",     path).c_str(), m_numCols, m_numRows, downsampleMask, image);
			CHeightMap_ExportDDSIDMap(m_boundIDXorMap, atVarString("%s_boundID_xor.dds", path).c_str(), m_numCols, m_numRows, downsampleMask, image);
		}
	}

	if (image)
	{
		image->Release();
		image = NULL;
	}
}

template <typename CellType> void CHeightMap<CellType>::Reset(int numCols, int numRows, int downsample, float boundsMinX, float boundsMinY, float boundsMinZ, float boundsMaxX, float boundsMaxY, float boundsMaxZ, const char* waterBoundaryPath, eCreateHeightmapFlags flags)
{
	//USE_DEBUG_MEMORY();

	Release();

	if (numCols == -1)
	{
		// calculate from bounds
		numCols = (int)((boundsMaxX - boundsMinX)/(float)gv::WORLD_CELL_SIZE);
		numRows = (int)((boundsMaxY - boundsMinY)/(float)gv::WORLD_CELL_SIZE);
	}
	else if (numCols == 0)
	{
		return; // we probably haven't loaded any height map yet but we called Reset() for some reason
	}

	m_enabled          = true;
	m_numCols          = numCols;
	m_numRows          = numRows;
	m_downsample       = downsample;
	m_cellSizeX        = (boundsMaxX - boundsMinX)/(float)numCols;
	m_cellSizeY        = (boundsMaxY - boundsMinY)/(float)numRows;
	m_boundsMinX       = boundsMinX;
	m_boundsMinY       = boundsMinY;
	m_boundsMinZ       = boundsMinZ;
	m_boundsMaxX       = boundsMaxX;
	m_boundsMaxY       = boundsMaxY;
	m_boundsMaxZ       = boundsMaxZ;
	m_oneOverCellSizeX = ((float)numCols)/(boundsMaxX - boundsMinX);
	m_oneOverCellSizeY = ((float)numRows)/(boundsMaxY - boundsMinY);

	if (flags & CREATE_HEIGHTMAP)
	{
		const int dataSizeInBytes = m_numRows*m_numCols*sizeof(CellType)*2;
		char*     data            = rage_new char[dataSizeInBytes];

		if (data)
		{
			sysMemSet(data, 0, dataSizeInBytes);
		}

		m_data    = data;
		m_dataRLE = NULL;
		m_dataMax = (CellType*)data; data += dataSizeInBytes/2;
		m_dataMin = (CellType*)data;
	}

	const int maskSizeInBytes = (m_numRows*m_numCols + 7)/8;

#define CREATE_MASK(name) { name = rage_new u8[maskSizeInBytes]; sysMemSet(name, 0x00, maskSizeInBytes); }

	if (1) // always create a worldmask
	{
		CREATE_MASK(m_worldMask);
	}

	if (flags & CREATE_WATERMASK)
	{
		CREATE_MASK(m_waterMask);
	}

	if (flags & CREATE_WATERHEIGHT)
	{
		CREATE_MASK(m_waterCollisionMask    );
		CREATE_MASK(m_waterOccluderMask     );
		CREATE_MASK(m_waterDrawableMask     );
		CREATE_MASK(m_waterOceanMask        );
		CREATE_MASK(m_waterOceanGeometryMask);
		CREATE_MASK(m_waterNoReflectionMask );

		m_waterHeightMap = rage_new float[m_numRows*m_numCols];
		m_underwaterMap  = rage_new float[m_numRows*m_numCols];

		for (int i = 0; i < m_numRows*m_numCols; i++)
		{
			m_waterHeightMap[i] = -1000.0f;
			m_underwaterMap[i] = 0.0f;
		}
	}

	if (flags & CREATE_STAIRSMASK)
	{
		CREATE_MASK(m_stairsMask);
		CREATE_MASK(m_stairSlopeMask);
	}

	if (flags & CREATE_ROADSMASK)
	{
		CREATE_MASK(m_roadsMask);
	}

	if (flags & CREATE_SCRIPTMASK)
	{
		CREATE_MASK(m_scriptMask);
	}

	if (flags & CREATE_BOUNDMASK)
	{
		CREATE_MASK(m_moverBoundMask);
		CREATE_MASK(m_weaponBoundMask);
		CREATE_MASK(m_vehicleBoundMask);
		CREATE_MASK(m_moverNoVehicleBoundMaskM);
		CREATE_MASK(m_moverNoVehicleBoundMaskP);
	}

	if (flags & CREATE_INTERIORMASK)
	{
		CREATE_MASK(m_interiorMask      );
		CREATE_MASK(m_exteriorPortalMask);
	}

	if (flags & CREATE_BOUNDTYPEMASK)
	{
		CREATE_MASK(m_boundTypeMask_BOX       );
		CREATE_MASK(m_boundTypeMask_CYLINDER  );
		CREATE_MASK(m_boundTypeMask_CAPSULE   );
		CREATE_MASK(m_boundTypeMask_SPHERE    );
		CREATE_MASK(m_boundTypeMask_nonBVHPRIM);
	}

	if (flags & CREATE_WATERMIRRORPORTALMASK)
	{
		CREATE_MASK(m_waterPortalMask   );
		CREATE_MASK(m_mirrorDrawableMask); // to be compared against mirror portal mask
		CREATE_MASK(m_mirrorPortalMask  );
	}

	if (flags & CREATE_CABLEMASK)
	{
		CREATE_MASK(m_cableMask);
	}

	if (flags & CREATE_CORONAMASK)
	{
		CREATE_MASK(m_coronaMask);
	}

	if (flags & CREATE_ANIMBLDGMASK)
	{
		CREATE_MASK(m_animatedBuildingMask);
	}

	if (flags & CREATE_MATERIALMASK)
	{
		m_materialsMask = rage_new u64[m_numRows*m_numCols];
		m_matProcIdMask = rage_new u16[m_numRows*m_numCols];
		sysMemSet(m_materialsMask, 0x00, m_numRows*m_numCols*sizeof(u64));
		sysMemSet(m_matProcIdMask, 0x00, m_numRows*m_numCols*sizeof(u16));

		if (g_GlobalMaterialMap)
		{
			m_matProcMaskCount = (g_GlobalMaterialCount + 63)/64;
			m_matProcMasks = rage_new u64*[m_matProcMaskCount];

			for (int i = 0; i < m_matProcMaskCount; i++)
			{
				m_matProcMasks[i] = rage_new u64[m_numRows*m_numCols];
				sysMemSet(m_matProcMasks[i], 0x00, m_numRows*m_numCols*sizeof(u64));
			}
		}

		m_propGroupMaskMap = rage_new u16[m_numRows*m_numCols];
		sysMemSet(m_propGroupMaskMap, 0x00, m_numRows*m_numCols*sizeof(u16));
	}

	if (flags & CREATE_DENSITYMAP_PED)
	{
		m_pedDensityMap = rage_new u8[m_numRows*m_numCols];
		sysMemSet(m_pedDensityMap, 0x00, m_numRows*m_numCols*sizeof(u8));
	}

	if (flags & CREATE_DENSITYMAP_MOVER)
	{
		m_moverBoundPolyDensityMap = rage_new u8[m_numRows*m_numCols];
		m_moverBoundPrimDensityMap = rage_new u8[m_numRows*m_numCols];
		sysMemSet(m_moverBoundPolyDensityMap, 0x00, m_numRows*m_numCols*sizeof(u8));
		sysMemSet(m_moverBoundPrimDensityMap, 0x00, m_numRows*m_numCols*sizeof(u8));
	}

	if (flags & CREATE_DENSITYMAP_WEAPON)
	{
		m_weaponBoundPolyDensityMap = rage_new u8[m_numRows*m_numCols];
		m_weaponBoundPrimDensityMap = rage_new u8[m_numRows*m_numCols];
		sysMemSet(m_weaponBoundPolyDensityMap, 0x00, m_numRows*m_numCols*sizeof(u8));
		sysMemSet(m_weaponBoundPrimDensityMap, 0x00, m_numRows*m_numCols*sizeof(u8));
	}

	if (flags & CREATE_DENSITYMAP_LIGHT)
	{
		m_lightDensityMap = rage_new u8[m_numRows*m_numCols];
		sysMemSet(m_lightDensityMap, 0x00, m_numRows*m_numCols*sizeof(u8));
	}

	if (flags & CREATE_DENSITYMAP)
	{
		m_densityMap = rage_new float[m_numRows*m_numCols];
		sysMemSet(m_densityMap, 0x00, m_numRows*m_numCols*sizeof(float));
	}

	if (flags & CREATE_DENSITYMAP_EX)
	{
#if HEIGHTMAP_TOOL_DENSITY_MAP_TEST
		if (1) { m_densityMapA = rage_new float[m_numRows*m_numCols]; sysMemSet(m_densityMapA, 0x00, m_numRows*m_numCols*sizeof(float)); }
		if (1) { m_densityMapB = rage_new float[m_numRows*m_numCols]; sysMemSet(m_densityMapB, 0x00, m_numRows*m_numCols*sizeof(float)); }
		if (1) { m_densityMapC = rage_new float[m_numRows*m_numCols]; sysMemSet(m_densityMapC, 0x00, m_numRows*m_numCols*sizeof(float)); }
		if (1) { m_densityMapD = rage_new float[m_numRows*m_numCols]; sysMemSet(m_densityMapD, 0x00, m_numRows*m_numCols*sizeof(float)); }
		if (1) { m_densityMapE = rage_new float[m_numRows*m_numCols]; sysMemSet(m_densityMapE, 0x00, m_numRows*m_numCols*sizeof(float)); }
		if (1) { m_densityMapF = rage_new float[m_numRows*m_numCols]; sysMemSet(m_densityMapF, 0x00, m_numRows*m_numCols*sizeof(float)); }
		if (1) { m_densityMapG = rage_new float[m_numRows*m_numCols]; sysMemSet(m_densityMapG, 0x00, m_numRows*m_numCols*sizeof(float)); }
		if (1) { m_densityMapH = rage_new float[m_numRows*m_numCols]; sysMemSet(m_densityMapH, 0x00, m_numRows*m_numCols*sizeof(float)); }
		if (1) { m_densityMapI = rage_new float[m_numRows*m_numCols]; sysMemSet(m_densityMapI, 0x00, m_numRows*m_numCols*sizeof(float)); }
#endif // HEIGHTMAP_TOOL_DENSITY_MAP_TEST
	}

	if (flags & CREATE_BOUND_ID_MAP)
	{
		m_boundIDRpfMap = rage_new u32[m_numRows*m_numCols];
		m_boundIDMaxMap = rage_new u32[m_numRows*m_numCols];
	//	m_boundIDXorMap = rage_new u32[m_numRows*m_numCols];
		sysMemSet(m_boundIDRpfMap, 0x00, m_numRows*m_numCols*sizeof(u32));
		sysMemSet(m_boundIDMaxMap, 0x00, m_numRows*m_numCols*sizeof(u32));
	//	sysMemSet(m_boundIDXorMap, 0x00, m_numRows*m_numCols*sizeof(u32));
	}

#undef CREATE_MASK

	if (waterBoundaryPath && waterBoundaryPath[0] != '\0')
	{
		fiStream* eps = fiStream::Create(waterBoundaryPath);

		if (Verifyf(eps, "failed to create water boundary file \"%s\"", waterBoundaryPath))
		{
			fprintf(eps, "%%!PS-Adobe EPSF-3.0\n");
			fprintf(eps, "%%%%HiResBoundingBox: 0 0 700 700\n");
			fprintf(eps, "%%%%BoundingBox: 0 0 700 700\n");
			fprintf(eps, "\n");
			fprintf(eps, "/Times-Roman findfont\n");
			fprintf(eps, "12 scalefont\n");
			fprintf(eps, "setfont\n");
			fprintf(eps, "25 12 moveto\n");
			fprintf(eps, "0 0 0 setrgbcolor\n");
			fprintf(eps, "(water boundary edges) show\n");
			fprintf(eps, "\n");
			fprintf(eps, "0.0 0.0 0.0 setrgbcolor\n");
			fprintf(eps, "0.2 setlinewidth\n");
			fprintf(eps, "\n");

			// bounds
			{
				const float scale = 512.0f/(float)Min<int>(gv::WORLD_BOUNDS_MAX_X - gv::WORLD_BOUNDS_MIN_X, gv::WORLD_BOUNDS_MAX_Y - gv::WORLD_BOUNDS_MIN_Y);
				const float offset = 40.0f;

				const float x0 = offset;
				const float y0 = offset;
				const float x1 = offset + scale*(float)(gv::WORLD_BOUNDS_MAX_X - gv::WORLD_BOUNDS_MIN_X);
				const float y1 = offset + scale*(float)(gv::WORLD_BOUNDS_MAX_Y - gv::WORLD_BOUNDS_MIN_Y);

				fprintf(eps, "%f %f moveto %f %f lineto stroke\n", x0, y0, x1, y0);
				fprintf(eps, "%f %f moveto %f %f lineto stroke\n", x1, y0, x1, y1);
				fprintf(eps, "%f %f moveto %f %f lineto stroke\n", x1, y1, x0, y1);
				fprintf(eps, "%f %f moveto %f %f lineto stroke\n", x0, y1, x0, y0);
				fprintf(eps, "\n");
			}
		}

		m_waterBoundaryFile = eps;
	}

#if __BANK
	m_interface.UpdateFrom(*this);
#endif // __BANK
}

template <typename CellType> void CHeightMap<CellType>::SetAllMinsToMax(int numRows, int numCols)
{
	if(!m_dataRLE)
	{
		int numCells = numRows*numCols;

		for(int i = 0; i < numCells; ++i)
		{
			m_dataMin[i] = m_dataMax[i];
		}
	}
}

template <typename CellType> void CHeightMap<CellType>::Release()
{
	GWHM_SAFE_DELETE_VP(m_data); // this includes memory used by m_dataRLE, m_dataMax, m_dataMin

	GWHM_SAFE_DELETE(m_worldMask);
	GWHM_SAFE_DELETE(m_waterMask);
	GWHM_SAFE_DELETE(m_stairsMask);
	GWHM_SAFE_DELETE(m_stairSlopeMask);
	GWHM_SAFE_DELETE(m_roadsMask);
	GWHM_SAFE_DELETE(m_scriptMask);
	GWHM_SAFE_DELETE(m_moverBoundMask);
	GWHM_SAFE_DELETE(m_weaponBoundMask);
	GWHM_SAFE_DELETE(m_vehicleBoundMask);
	GWHM_SAFE_DELETE(m_moverNoVehicleBoundMaskM);
	GWHM_SAFE_DELETE(m_moverNoVehicleBoundMaskP);
	GWHM_SAFE_DELETE(m_boundTypeMask_BOX);
	GWHM_SAFE_DELETE(m_boundTypeMask_CYLINDER);
	GWHM_SAFE_DELETE(m_boundTypeMask_CAPSULE);
	GWHM_SAFE_DELETE(m_boundTypeMask_SPHERE);
	GWHM_SAFE_DELETE(m_boundTypeMask_nonBVHPRIM);
	GWHM_SAFE_DELETE(m_interiorMask);
	GWHM_SAFE_DELETE(m_exteriorPortalMask);
	GWHM_SAFE_DELETE(m_waterCollisionMask);
	GWHM_SAFE_DELETE(m_waterOccluderMask);
	GWHM_SAFE_DELETE(m_waterDrawableMask);
	GWHM_SAFE_DELETE(m_waterOceanMask);
	GWHM_SAFE_DELETE(m_waterOceanGeometryMask);
	GWHM_SAFE_DELETE(m_waterNoReflectionMask);
	GWHM_SAFE_DELETE(m_underwaterMap);
	GWHM_SAFE_DELETE(m_waterHeightMap);
	GWHM_SAFE_DELETE(m_waterPortalMask);
	GWHM_SAFE_DELETE(m_mirrorDrawableMask);
	GWHM_SAFE_DELETE(m_mirrorPortalMask);
	GWHM_SAFE_DELETE(m_cableMask);
	GWHM_SAFE_DELETE(m_coronaMask);
	GWHM_SAFE_DELETE(m_animatedBuildingMask);
	GWHM_SAFE_DELETE(m_materialsMask);
	GWHM_SAFE_DELETE(m_matProcIdMask);
	for (int i = 0; i < m_matProcMaskCount; i++) { GWHM_SAFE_DELETE(m_matProcMasks[i]); }
	GWHM_SAFE_DELETE(m_matProcMasks);
	m_matProcMaskCount = 0;
	GWHM_SAFE_DELETE(m_propGroupMaskMap);
	GWHM_SAFE_DELETE(m_pedDensityMap);
	GWHM_SAFE_DELETE(m_moverBoundPolyDensityMap);
	GWHM_SAFE_DELETE(m_moverBoundPrimDensityMap);
	GWHM_SAFE_DELETE(m_weaponBoundPolyDensityMap);
	GWHM_SAFE_DELETE(m_weaponBoundPrimDensityMap);
	GWHM_SAFE_DELETE(m_lightDensityMap);
	GWHM_SAFE_DELETE(m_densityMap);
#if HEIGHTMAP_TOOL_DENSITY_MAP_TEST
	GWHM_SAFE_DELETE(m_densityMapA);
	GWHM_SAFE_DELETE(m_densityMapB);
	GWHM_SAFE_DELETE(m_densityMapC);
	GWHM_SAFE_DELETE(m_densityMapD);
	GWHM_SAFE_DELETE(m_densityMapE);
	GWHM_SAFE_DELETE(m_densityMapF);
	GWHM_SAFE_DELETE(m_densityMapG);
	GWHM_SAFE_DELETE(m_densityMapH);
	GWHM_SAFE_DELETE(m_densityMapI);
#endif // HEIGHTMAP_TOOL_DENSITY_MAP_TEST
	GWHM_SAFE_DELETE(m_boundIDRpfMap);
	GWHM_SAFE_DELETE(m_boundIDMaxMap);
	GWHM_SAFE_DELETE(m_boundIDXorMap);

	if (m_waterBoundaryFile)
	{
		m_waterBoundaryFile->Close();
		m_waterBoundaryFile = NULL;
	}

	Clear(false);
}

template <typename CellType> void CHeightMap<CellType>::ConvertToNonRLE()
{
	if (!IsValid())
	{
		return;
	}

	if (m_dataRLE)
	{
		int dataSizeInBytes = m_numRows*m_numCols*sizeof(CellType)*2;
		char* data = rage_new char[dataSizeInBytes];
		sysMemSet(data, 0, dataSizeInBytes);

		CellType* dataMax = (CellType*)data;
		CellType* dataMin = (CellType*)(data + (dataSizeInBytes/2));

#if __ASSERT
		int numCells = 0;

		for (int j = 0; j < m_numRows; j++)
		{
			numCells += m_dataRLE[j].GetCount();
		}
#endif // __ASSERT

		for (int j = 0; j < m_numRows; j++)
		{
			const int dstOffset = m_dataRLE[j].GetStart() + j*m_numCols;
			const int srcOffset = m_dataRLE[j].GetDataStart();
			const int count     = m_dataRLE[j].GetCount();

			Assert(dstOffset >= 0 && dstOffset < m_numRows*m_numCols);
			Assert(srcOffset >= 0 && srcOffset < numCells);

			sysMemCpy(&dataMax[dstOffset], &m_dataMax[srcOffset], count*sizeof(CellType));
			sysMemCpy(&dataMin[dstOffset], &m_dataMin[srcOffset], count*sizeof(CellType));
		}

		delete[] (char*) m_data;

		m_data    = data;
		m_dataRLE = NULL;
		m_dataMax = dataMax;
		m_dataMin = dataMin;
	}
#if __WIN32PC
	else
	{
		return; // get out of here .. otherwise the code below will create potentially unwanted images
	}
#endif // __WIN32PC

	const int maskSizeInBytes = (m_numRows*m_numCols + 7)/8;

	if (m_worldMask == NULL)
	{
		m_worldMask = rage_new u8[maskSizeInBytes];
		sysMemSet(m_worldMask, 0x00, maskSizeInBytes);
	}

	if (m_waterMask == NULL)
	{
		m_waterMask = rage_new u8[maskSizeInBytes];
		sysMemSet(m_waterMask, 0x00, maskSizeInBytes);
	}

#if __WIN32PC // only set up roads mask for the heightmap tool
	if (m_roadsMask == NULL)
	{
		m_roadsMask = rage_new u8[maskSizeInBytes];
		sysMemSet(m_roadsMask, 0x00, maskSizeInBytes);
	}
#endif // __WIN32PC
}

template <typename CellType> int CHeightMap<CellType>::ConvertToRLE()
{
	if (!IsValid())
	{
		return 0;
	}

	int numCells = 0;

	if (m_dataRLE) // already has RLE data, just count the cells
	{
		for (int j = 0; j < m_numRows; j++)
		{
			numCells += m_dataRLE[j].GetCount();
		}
	}
	else // create RLE data
	{
		RLE* dataRLE = rage_new RLE[m_numRows];
		sysMemSet(dataRLE, 0, m_numRows*sizeof(RLE));

		for (int j = 0; j < m_numRows; j++)
		{
			const CellType* rowMin = &m_dataMin[j*m_numCols];
			const CellType* rowMax = &m_dataMax[j*m_numCols];
			int start = 0;
			int stop = m_numCols - 1;

			while (start < m_numCols && rowMax[start] == 0 && rowMin[start] == 0)
			{
				start++;
			}

			while (stop >= 0 && rowMax[stop] == 0 && rowMin[stop] == 0)
			{
				stop--;
			}

			if (start <= stop)
			{
				const int count = stop - start + 1;

				dataRLE[j].Set(start, count, numCells - start);

				numCells += count;
			}
		}

		const int dataSizeInBytes = m_numRows*sizeof(RLE) + numCells*sizeof(CellType)*2;
		char* data = rage_new char[dataSizeInBytes];
		sysMemSet(data, 0, dataSizeInBytes);

		char* dst = data;

		// copy RLE data
		{
			sysMemCpy(dst, dataRLE, m_numRows*sizeof(RLE));
			dst += m_numRows*sizeof(RLE);
		}

		// copy max data
		for (int j = 0; j < m_numRows; j++)
		{
			const int count = dataRLE[j].GetCount();
			sysMemCpy(dst, &m_dataMax[dataRLE[j].GetStart() + j*m_numCols], count*sizeof(CellType));
			dst += count*sizeof(CellType);
		}

		// copy min data
		for (int j = 0; j < m_numRows; j++)
		{
			const int count = dataRLE[j].GetCount();
			sysMemCpy(dst, &m_dataMin[dataRLE[j].GetStart() + j*m_numCols], count*sizeof(CellType));
			dst += count*sizeof(CellType);
		}

		Assert(dst == data + dataSizeInBytes);

		delete[] dataRLE;
		delete[] (char*) m_data;

		m_data    = data;
		m_dataRLE = (RLE*)data; data += m_numRows*sizeof(RLE);
		m_dataMax = (CellType*)data; data += numCells*sizeof(CellType);
		m_dataMin = (CellType*)data;
	}

	return numCells;
}

template <typename CellType> void CHeightMap<CellType>::RasteriseAtIndex(
	int i,
	int j,
	float zmin,
	float zmax,
	bool bIsCentreFacingCell,
	u32 flags,
	const CHeightMap<CellTypeHR>* ,//ground,
	int materialId, // index into special material list (low 8 bits) and proc tag (high 8 bits)
	u32 propGroupMask,
	u8 pedDensity,
	u8 moverBoundPolyDensity,
	u8 moverBoundPrimDensity,
	u8 weaponBoundPolyDensity,
	u8 weaponBoundPrimDensity,
	float density,
	u32 boundID_rpf,
	u32 boundID,
	int boundPrimitiveType
#if HEIGHTMAP_TOOL_DENSITY_MAP_TEST
	, bool bIsEdgeCell
	, bool bIsVertCell
	, float clippedArea2D
	, float area
	, float normal_z
#endif // HEIGHTMAP_TOOL_DENSITY_MAP_TEST
	)
{
	if (!IsEnabled())
	{
		return;
	}

#if __ASSERT
	if (!AssertVerify(m_waterUpdate != m_geomUpdate)) { return; }
	if (!AssertVerify(m_dataRLE == NULL))             { return; }

	if (!AssertVerify(i >= 0 && i < m_numCols))       { return; }
	if (!AssertVerify(j >= 0 && j < m_numRows))       { return; }
#endif // __ASSERT

	const int index = i + j*m_numCols;

	if (m_stairsMask            && (flags & GWHM_FLAG_STAIRS               ) != 0) { m_stairsMask           [index/8] |= BIT(index%8); }
	if (m_stairSlopeMask        && (flags & GWHM_FLAG_STAIRSLOPE           ) != 0) { m_stairSlopeMask       [index/8] |= BIT(index%8); }
	if (m_roadsMask             && (flags & GWHM_FLAG_ROAD                 ) != 0) { m_roadsMask            [index/8] |= BIT(index%8); }
	if (m_scriptMask            && (flags & GWHM_FLAG_SCRIPT               ) != 0) { m_scriptMask           [index/8] |= BIT(index%8); }
	if (m_moverBoundMask        && (flags & GWHM_FLAG_MOVER_BOUND          ) != 0) { m_moverBoundMask       [index/8] |= BIT(index%8); }
	if (m_weaponBoundMask       && (flags & GWHM_FLAG_WEAPON_BOUND         ) != 0) { m_weaponBoundMask      [index/8] |= BIT(index%8); }
	if (m_vehicleBoundMask      && (flags & GWHM_FLAG_VEHICLE_BOUND        ) != 0) { m_vehicleBoundMask     [index/8] |= BIT(index%8); }
	if (m_interiorMask          && (flags & GWHM_FLAG_INTERIOR             ) != 0) { m_interiorMask         [index/8] |= BIT(index%8); }
	if (m_exteriorPortalMask    && (flags & GWHM_FLAG_EXTERIOR_PORTAL      ) != 0) { m_exteriorPortalMask   [index/8] |= BIT(index%8); }
	if (m_waterCollisionMask    && (flags & GWHM_FLAG_WATER_COLLISION      ) != 0) { m_waterCollisionMask   [index/8] |= BIT(index%8); }
	if (m_waterOccluderMask     && (flags & GWHM_FLAG_WATER_OCCLUDER       ) != 0) { m_waterOccluderMask    [index/8] |= BIT(index%8); }
	if (m_waterDrawableMask     && (flags & GWHM_FLAG_WATER_DRAWABLE       ) != 0) { m_waterDrawableMask    [index/8] |= BIT(index%8); }
	if (m_waterPortalMask       && (flags & GWHM_FLAG_WATER_SURFACE_PORTAL ) != 0) { m_waterPortalMask      [index/8] |= BIT(index%8); }
	if (m_waterNoReflectionMask && (flags & GWHM_FLAG_WATER_NO_REFLECTION  ) != 0) { m_waterNoReflectionMask[index/8] |= BIT(index%8); }
	if (m_mirrorDrawableMask    && (flags & GWHM_FLAG_MIRROR_DRAWABLE      ) != 0) { m_mirrorDrawableMask   [index/8] |= BIT(index%8); }
	if (m_mirrorPortalMask      && (flags & GWHM_FLAG_MIRROR_SURFACE_PORTAL) != 0) { m_mirrorPortalMask     [index/8] |= BIT(index%8); }
	if (m_cableMask             && (flags & GWHM_FLAG_CABLE                ) != 0) { m_cableMask            [index/8] |= BIT(index%8); }
	if (m_coronaMask            && (flags & GWHM_FLAG_CORONA_QUAD          ) != 0) { m_coronaMask           [index/8] |= BIT(index%8); }
	if (m_animatedBuildingMask  && (flags & GWHM_FLAG_ANIMATED_BUILDING    ) != 0) { m_animatedBuildingMask [index/8] |= BIT(index%8); }

	if (flags & GWHM_FLAG_PROP)
	{
		if (m_moverNoVehicleBoundMaskP)
		{
			if ((flags & GWHM_FLAG_CONTAINS_MOVER_BOUNDS) != 0 &&
				(flags & GWHM_FLAG_CONTAINS_VEHICLE_BOUNDS) == 0)
			{
				m_moverNoVehicleBoundMaskP[index/8] |= BIT(index%8);
			}
		}
	}
	else // not a prop
	{
		if (m_moverNoVehicleBoundMaskM)
		{
			if ((flags & GWHM_FLAG_CONTAINS_MOVER_BOUNDS) != 0 &&
				(flags & GWHM_FLAG_CONTAINS_VEHICLE_BOUNDS) == 0)
			{
				m_moverNoVehicleBoundMaskM[index/8] |= BIT(index%8);
			}
		}
	}

	if (m_propGroupMaskMap)
	{
		m_propGroupMaskMap[index] |= propGroupMask;
	}

	if (m_lightDensityMap && (flags & GWHM_FLAG_LIGHT) != 0 && bIsCentreFacingCell)
	{
		if (m_lightDensityMap[index] < 255) // clamp at 255
		{
			m_lightDensityMap[index]++;
		}
	}

	if (flags & (GWHM_FLAG_WATER_NO_REFLECTION | GWHM_FLAG_CABLE | GWHM_FLAG_LIGHT | GWHM_FLAG_CORONA_QUAD | GWHM_FLAG_ANIMATED_BUILDING | GWHM_FLAG_MASK_ONLY))
	{
		return;
	}

	if (materialId != -1)
	{
		if (m_materialsMask)
		{
			m_materialsMask[index] |= BIT64(materialId & 0x003f);
		}

		if (m_matProcMasks && g_GlobalMaterialMap)
		{
			const int* mid = g_GlobalMaterialMap->Access((u16)materialId);

			if (mid && (*mid)/64 < m_matProcMaskCount)
			{
				u64* matProcMask = m_matProcMasks[(*mid)/64];
				matProcMask[index] |= BIT64((*mid)%64);
			}

			mid = g_GlobalMaterialMap->Access((u16)(materialId | 0x003f)); // all materials for this procId

			if (mid && (*mid)/64 < m_matProcMaskCount)
			{
				u64* matProcMask = m_matProcMasks[(*mid)/64];
				matProcMask[index] |= BIT64((*mid)%64);
			}
		}
	}

	if (!m_waterUpdate && boundPrimitiveType != -1)
	{
		if (m_boundTypeMask_BOX        && boundPrimitiveType == PRIM_TYPE_BOX       ) { m_boundTypeMask_BOX       [index/8] |= BIT(index%8); }
		if (m_boundTypeMask_CYLINDER   && boundPrimitiveType == PRIM_TYPE_CYLINDER  ) { m_boundTypeMask_CYLINDER  [index/8] |= BIT(index%8); }
		if (m_boundTypeMask_CAPSULE    && boundPrimitiveType == PRIM_TYPE_CAPSULE   ) { m_boundTypeMask_CAPSULE   [index/8] |= BIT(index%8); }
		if (m_boundTypeMask_SPHERE     && boundPrimitiveType == PRIM_TYPE_SPHERE    ) { m_boundTypeMask_SPHERE    [index/8] |= BIT(index%8); }
		if (m_boundTypeMask_nonBVHPRIM && (flags & GWHM_FLAG_NON_BVH_PRIMITIVE) != 0) { m_boundTypeMask_nonBVHPRIM[index/8] |= BIT(index%8); }
	}

	const float densityScale = (boundPrimitiveType == PRIM_TYPE_POLYGON || boundPrimitiveType == PRIM_TYPE_BOX) ? 1.0f : 0.0f;

	if (m_pedDensityMap            ) { m_pedDensityMap            [index] = Max<u8>   (m_pedDensityMap            [index], pedDensity            ); }
	if (m_moverBoundPolyDensityMap ) { m_moverBoundPolyDensityMap [index] = Max<u8>   (m_moverBoundPolyDensityMap [index], moverBoundPolyDensity ); }
	if (m_moverBoundPrimDensityMap ) { m_moverBoundPrimDensityMap [index] = Max<u8>   (m_moverBoundPrimDensityMap [index], moverBoundPrimDensity ); }
	if (m_weaponBoundPolyDensityMap) { m_weaponBoundPolyDensityMap[index] = Max<u8>   (m_weaponBoundPolyDensityMap[index], weaponBoundPolyDensity); }
	if (m_weaponBoundPrimDensityMap) { m_weaponBoundPrimDensityMap[index] = Max<u8>   (m_weaponBoundPrimDensityMap[index], weaponBoundPrimDensity); }
	if (m_densityMap               ) { m_densityMap               [index] = Max<float>(m_densityMap               [index], density*densityScale  ); }

#if HEIGHTMAP_TOOL_DENSITY_MAP_TEST
	if (!m_waterUpdate && boundPrimitiveType != -1)
	{
		const float densityA = densityScale/Max<float>(0.001f, area);
		const float densityB = area;
		const float densityC = Abs<float>(normal_z);
		const float densityD = 1.0f - Abs<float>(normal_z);

		if (m_densityMapA) { m_densityMapA[index] = Max<float>(densityA, m_densityMapA[index]); }
		if (m_densityMapB) { m_densityMapB[index] = Max<float>(densityB, m_densityMapB[index]); }
		if (m_densityMapC) { m_densityMapC[index] = Max<float>(densityC, m_densityMapC[index]); }
		if (m_densityMapD) { m_densityMapD[index] = Max<float>(densityD, m_densityMapD[index]); }

		if (bIsCentreFacingCell)
		{
			const float densityE = clippedArea2D;
			const float densityF = clippedArea2D*densityA;
			const float densityG = 1.0f;
			const float densityH = bIsEdgeCell ? 1.0f : 0.0f;
			const float densityI = bIsVertCell ? 1.0f : 0.0f;

			if (m_densityMapE) { m_densityMapE[index] += densityE; }
			if (m_densityMapF) { m_densityMapF[index] += densityF; }
			if (m_densityMapG) { m_densityMapG[index] += densityG; }
			if (m_densityMapH) { m_densityMapH[index] += densityH; }
			if (m_densityMapI) { m_densityMapI[index] += densityI; }
		}
	}
#endif // HEIGHTMAP_TOOL_DENSITY_MAP_TEST

	CellType z0 = QuantiseCellMin<CellType>(zmin, m_boundsMinZ, m_boundsMaxZ);
	CellType z1 = QuantiseCellMax<CellType>(zmax, m_boundsMinZ, m_boundsMaxZ);

	CellType& dataMin = m_dataMin[index];
	CellType& dataMax = m_dataMax[index];

	if (m_matProcIdMask && materialId != -1 && dataMax < z1)
	{
		m_matProcIdMask[index] = (u16)materialId;
	}

	const bool bIsWaterCollision = (flags & GWHM_FLAG_WATER_COLLISION) != 0;
	const bool bIsWaterOccluder  = (flags & GWHM_FLAG_WATER_OCCLUDER ) != 0;
	const bool bIsWaterDrawable  = (flags & GWHM_FLAG_WATER_DRAWABLE ) != 0;
	const bool bIsNonOceanWater  = bIsWaterCollision || bIsWaterOccluder || bIsWaterDrawable;
	const bool bIsOceanWater     = m_waterUpdate && dataMax <= z1;

	if (m_waterMask && (bIsNonOceanWater || bIsOceanWater))
	{
		m_waterMask[index/8] |= BIT(index%8);
	}

	if (m_waterOceanMask && bIsOceanWater)
	{
		m_waterOceanMask[index/8] |= BIT(index%8);
	}

	if (m_waterOceanGeometryMask && m_waterUpdate)
	{
		m_waterOceanGeometryMask[index/8] |= BIT(index%8);
	}

	if (m_waterHeightMap && bIsWaterDrawable)
	{
		m_waterHeightMap[index] = Max<float>(zmax, m_waterHeightMap[index]);
	}

#if __BANK
	m_stats_cellsCovered++;
#endif // __BANK

	const bool bIsInterior       = (flags & GWHM_FLAG_INTERIOR       ) != 0;
	const bool bIsExteriorPortal = (flags & GWHM_FLAG_EXTERIOR_PORTAL) != 0;

	if (!m_waterUpdate && boundPrimitiveType != -1)
	{
		if (m_underwaterMap)
		{
			m_underwaterMap[index] = Max<float>(-zmax, m_underwaterMap[index]);
		}

		if (boundID_rpf != 0)
		{
			if (m_boundIDRpfMap && z1 >= dataMax)
			{
				m_boundIDRpfMap[index] = boundID_rpf;
			}
		}

		if (boundID != 0)
		{
			if (m_boundIDMaxMap && z1 >= dataMax)
			{
				m_boundIDMaxMap[index] = boundID;
			}

			if (m_boundIDXorMap && bIsCentreFacingCell)
			{
				m_boundIDXorMap[index] ^= boundID;
			}
		}
	}

	if (!m_waterUpdate && !bIsInterior && (bIsExteriorPortal || boundPrimitiveType != -1))
	{
		if ((m_worldMask[index/8] & BIT(index%8)) == 0)
		{
			m_worldMask[index/8] |= BIT(index%8);

			dataMin = z0;
			dataMax = z1;
#if __BANK
			m_stats_cellsUpdated++;
#endif // __BANK

			return;
		}

#if __BANK
		const CellType dataMin_prev = dataMin;
		const CellType dataMax_prev = dataMax;
#endif // __BANK

		if (dataMin > z0 && !m_waterUpdate)
		{
			dataMin = z0;
		}

		if (dataMax < z1)
		{
			dataMax = z1;
		}

#if __BANK
		if (dataMin != dataMin_prev ||
			dataMax != dataMax_prev)
		{
			m_stats_cellsUpdated++;
		}
#endif // __BANK
	}
}

class CHeightMapRiverBoundary
{
public:
	class CRiverTri
	{
	public:
		CRiverTri() {}
		CRiverTri(Vec3V_In p0, Vec3V_In p1, Vec3V_In p2) { m_p[0] = p0; m_p[1] = p1; m_p[2] = p2; }

		Vec3V m_p[3];
	};

	class CRiverEdge
	{
	public:
		CRiverEdge() {}
		CRiverEdge(Vec2V_In p0, Vec2V_In p1) { m_edge = Vec4V(p0, p1); }

		Vec2V_Out GetP0() const { return m_edge.GetXY(); }
		Vec2V_Out GetP1() const { return m_edge.GetZW(); }

		Vec4V m_edge;
	};

	void AddTriangle(Vec3V_In p0, Vec3V_In p1, Vec3V_In p2)
	{
		m_tris.PushAndGrow(CRiverTri(p0, p1, p2));
	}

	void GetEdges(atArray<CRiverEdge>& edges) const
	{
		for (int i = 0; i < m_tris.GetCount(); i++)
		{
			for (int i_side = 0; i_side < 3; i_side++)
			{
				bool bConnected = false;

				const Vec3V i_p0 = m_tris[i].m_p[(i_side + 0)];
				const Vec3V i_p1 = m_tris[i].m_p[(i_side + 1)%3];

				for (int j = i + 1; j < m_tris.GetCount(); j++)
				{
					for (int j_side = 0; j_side < 3; j_side++)
					{
						const Vec3V j_p0 = m_tris[j].m_p[(j_side + 0)];
						const Vec3V j_p1 = m_tris[j].m_p[(j_side + 1)%3];

						if (IsEqualAll(i_p0, j_p1) &
							IsEqualAll(i_p1, j_p0))
						{
							bConnected = true;
							break;
						}
					}

					if (bConnected)
					{
						break;
					}
				}

				if (!bConnected)
				{
					edges.PushAndGrow(CRiverEdge(i_p0.GetXY(), i_p1.GetXY()));
				}
			}
		}
	}

	atArray<CRiverTri> m_tris;
};

static CHeightMapRiverBoundary g_WorldHeightMapRiverBoundary;

/*static*/ Vec2V_Out CalcTriangleHeightAndAspect(Vec3V_In p0, Vec3V_In p1, Vec3V_In p2)
{
	ScalarV a0 = (MagSquared(p2 - p0) - square(Dot(p2 - p0, Normalize(p0 - p1))));
	ScalarV a1 = (MagSquared(p0 - p1) - square(Dot(p0 - p1, Normalize(p1 - p2))));
	ScalarV a2 = (MagSquared(p1 - p2) - square(Dot(p1 - p2, Normalize(p2 - p0))));

	const ScalarV height = Min(a0, a1, a2);

	a0 = MagSquared(p0 - p1)/a0;
	a1 = MagSquared(p1 - p2)/a1;
	a2 = MagSquared(p2 - p0)/a2;

	const ScalarV aspect = Max(a0, a1, a2);

	return Vec2V(height, aspect);
}

/*static*/ bool TriangleContainsPoint(Vec2V_In p0, Vec2V_In p1, Vec2V_In p2, Vec2V_In point)
{
	const float c10 = Dot(p1 - p0, (point - p0).Get<Vec::Y,Vec::X>()*Vec2V(-1.0f, 1.0f)).Getf();
	const float c21 = Dot(p2 - p1, (point - p1).Get<Vec::Y,Vec::X>()*Vec2V(-1.0f, 1.0f)).Getf();
	const float c02 = Dot(p0 - p2, (point - p2).Get<Vec::Y,Vec::X>()*Vec2V(-1.0f, 1.0f)).Getf();

	if ((c10 < 0.0f && c21 < 0.0f && c02 < 0.0f) ||
		(c10 > 0.0f && c21 > 0.0f && c02 > 0.0f))
	{
		return true;
	}

	return false;
}

template <typename CellType> void CHeightMap<CellType>::RasteriseTriangle(
	Vec3V_In p0,
	Vec3V_In p1,
	Vec3V_In p2,
	u32 flags,
	const CHeightMap<CellTypeHR>* ground,
	int materialId, // index into special material list (low 8 bits) and proc tag (high 8 bits)
	u32 propGroupMask,
	u8 pedDensity,
	u8 moverBoundPolyDensity,
	u8 moverBoundPrimDensity,
	u8 weaponBoundPolyDensity,
	u8 weaponBoundPrimDensity,
	u32 boundID_rpf,
	u32 boundID,
	int boundPrimitiveType
	)
{
	if (!IsEnabled())
	{
		return;
	}

#if __ASSERT
	if (!AssertVerify(m_waterUpdate != m_geomUpdate)) { return; }
	if (!AssertVerify(m_dataRLE == NULL))             { return; }
#endif // __ASSERT

	if (IsEqualAll(p0 - p1, Vec3V(V_ZERO)) |
		IsEqualAll(p1 - p2, Vec3V(V_ZERO)) |
		IsEqualAll(p2 - p0, Vec3V(V_ZERO)))
	{
		if ((flags & GWHM_FLAG_CABLE) == 0)
		{
			// triangle is degenerate - technically we could still rasterise it, but let's not bother
			return;
		}
	}

	const Vec3V pmin = Min(p0, p1, p2);
	const Vec3V pmax = Max(p0, p1, p2);

	const float x0 = pmin.GetXf();
	const float y0 = pmin.GetYf();
	const float x1 = pmax.GetXf();
	const float y1 = pmax.GetYf();

	int i0, j0, i1, j1;
	if (GetIndexAtBound(i0, j0, i1, j1, x0, y0, x1, y1))
	{
		const float area = CalculateTriangleArea(p0, p1, p2).Getf();
		const float normal_z = -Normalize(Cross(p1 - p0, p2 - p0)).GetZf();
#if HEIGHTMAP_TOOL_DENSITY_MAP_TEST
		//const Vec2V triHeightAndAspect = CalcTriangleHeightAndAspect(p0, p1, p2);
#endif // HEIGHTMAP_TOOL_DENSITY_MAP_TEST
		float density = 0.0f;

		if (m_densityMap)
		{
			density = 1.0f/DENSITY_FUNC(Max<float>(DENSITY_MIN_AREA, area));
		}

		if (i0 == i1 && j0 == j1) // triangle fits into a single grid cell
		{
			const float cellX = m_boundsMinX + m_cellSizeX*(0.5f + (float)i0);
			const float cellY = m_boundsMinY + m_cellSizeY*(0.5f + (float)j0);

			RasteriseAtIndex(
				i0,
				j0,
				pmin.GetZf(),
				pmax.GetZf(),
				normal_z > 0.0f && TriangleContainsPoint(p0.GetXY(), p1.GetXY(), p2.GetXY(), Vec2V(cellX, cellY)),
				flags,
				ground,
				materialId,
				propGroupMask,
				pedDensity,
				moverBoundPolyDensity,
				moverBoundPrimDensity,
				weaponBoundPolyDensity,
				weaponBoundPrimDensity,
				density,
				boundID_rpf,
				boundID,
				boundPrimitiveType
#if HEIGHTMAP_TOOL_DENSITY_MAP_TEST
				, true
				, true
				, CalculateTriangleArea(p0.GetXY(), p1.GetXY(), p2.GetXY()).Getf() // clippedArea2D
				, area 
				, normal_z
#endif // HEIGHTMAP_TOOL_DENSITY_MAP_TEST
			);
		}
		else // rasterise triangle by clipping to each grid cell (slow!)
		{
			for (int j = j0; j <= j1; j++)
			{
				for (int i = i0; i <= i1; i++)
				{
					const float cell_x0 = m_boundsMinX + m_cellSizeX*(float)(i + 0);
					const float cell_y0 = m_boundsMinY + m_cellSizeY*(float)(j + 0);
					const float cell_x1 = m_boundsMinX + m_cellSizeX*(float)(i + 1);
					const float cell_y1 = m_boundsMinY + m_cellSizeY*(float)(j + 1);

#if HEIGHTMAP_TOOL_DENSITY_MAP_TEST
					bool bIsEdgeCell = false;
					bool bIsVertCell = false;

					const Vec3V p[3] = {p0, p1, p2};

					for (int k = 0; k < 3; k++)
					{
						int   count = 2;
						Vec3V temp0[2 + 4] = {p[k], p[(k + 1)%3]};
						Vec3V temp1[2 + 4];

						// clip to four planes
						count = PolyClip(temp1, NELEM(temp1), temp0, count, BuildPlane(Vec3V(cell_x0, cell_y0, 0.0f), +Vec3V(V_X_AXIS_WZERO)));
						count = PolyClip(temp0, NELEM(temp0), temp1, count, BuildPlane(Vec3V(cell_x0, cell_y0, 0.0f), +Vec3V(V_Y_AXIS_WZERO)));
						count = PolyClip(temp1, NELEM(temp1), temp0, count, BuildPlane(Vec3V(cell_x1, cell_y1, 0.0f), -Vec3V(V_X_AXIS_WZERO)));
						count = PolyClip(temp0, NELEM(temp0), temp1, count, BuildPlane(Vec3V(cell_x1, cell_y1, 0.0f), -Vec3V(V_Y_AXIS_WZERO)));

						if (count > 0)
						{
							bIsEdgeCell = true;
							break;
						}
					}

					for (int k = 0; k < 3; k++)
					{
						if (p[k].GetXf() >= cell_x0 && p[k].GetXf() <= cell_x1 &&
							p[k].GetYf() >= cell_y0 && p[k].GetYf() <= cell_y1)
						{
							bIsVertCell = true;
							break;
						}	
					}
#endif // HEIGHTMAP_TOOL_DENSITY_MAP_TEST

					int   count = 3;
					Vec3V temp0[3 + 4] = {p0, p1, p2};
					Vec3V temp1[3 + 4];

					// clip to four planes
					count = PolyClip(temp1, NELEM(temp1), temp0, count, BuildPlane(Vec3V(cell_x0, cell_y0, 0.0f), +Vec3V(V_X_AXIS_WZERO)));
					count = PolyClip(temp0, NELEM(temp0), temp1, count, BuildPlane(Vec3V(cell_x0, cell_y0, 0.0f), +Vec3V(V_Y_AXIS_WZERO)));
					count = PolyClip(temp1, NELEM(temp1), temp0, count, BuildPlane(Vec3V(cell_x1, cell_y1, 0.0f), -Vec3V(V_X_AXIS_WZERO)));
					count = PolyClip(temp0, NELEM(temp0), temp1, count, BuildPlane(Vec3V(cell_x1, cell_y1, 0.0f), -Vec3V(V_Y_AXIS_WZERO)));

					if (count > 0)
					{
						ScalarV zmin = temp0[0].GetZ();
						ScalarV zmax = zmin;

						for (int k = 1; k < count; k++)
						{
							const ScalarV z = temp0[k].GetZ();

							zmin = Min(z, zmin);
							zmax = Max(z, zmax);
						}

						zmin = Clamp(zmin, pmin.GetZ(), pmax.GetZ()); // clip to triangle bounds just to be safe
						zmax = Clamp(zmax, pmin.GetZ(), pmax.GetZ()); // clip to triangle bounds just to be safe

#if HEIGHTMAP_TOOL_DENSITY_MAP_TEST
						float clippedArea2D = 0.0f;

						for (int k = 2; k < count; k++)
						{
							clippedArea2D += CalculateTriangleArea(temp0[0].GetXY(), temp0[k - 1].GetXY(), temp0[k].GetXY()).Getf();
						}
#endif // HEIGHTMAP_TOOL_DENSITY_MAP_TEST

						const float cellX = m_boundsMinX + m_cellSizeX*(0.5f + (float)i);
						const float cellY = m_boundsMinY + m_cellSizeY*(0.5f + (float)j);

						RasteriseAtIndex(
							i,
							j,
							zmin.Getf(),
							zmax.Getf(),
							normal_z > 0.0f && TriangleContainsPoint(p0.GetXY(), p1.GetXY(), p2.GetXY(), Vec2V(cellX, cellY)),
							flags,
							ground,
							materialId,
							propGroupMask,
							pedDensity,
							moverBoundPolyDensity,
							moverBoundPrimDensity,
							weaponBoundPolyDensity,
							weaponBoundPrimDensity,
							density,
							boundID_rpf,
							boundID,
							boundPrimitiveType
#if HEIGHTMAP_TOOL_DENSITY_MAP_TEST
							, bIsEdgeCell
							, bIsVertCell
							, clippedArea2D
							, area
							, normal_z
#endif // HEIGHTMAP_TOOL_DENSITY_MAP_TEST
						);
					}
				}
			}
		}

		const bool bIsWaterCollision = (flags & GWHM_FLAG_WATER_COLLISION) != 0;

		if (!m_waterUpdate && !bIsWaterCollision && !ground)
		{
#if USE_WATER_REGIONS
			extern ScalarV_Out GetWaterRegionZ(const spdRect& bounds);
			const ScalarV waterZ = GetWaterRegionZ(spdRect(spdAABB(pmin, pmax)));
			const Vec4V waterPlane = Vec4V(Vec3V(V_Z_AXIS_WZERO), -waterZ);
#else
			const Vec4V waterPlane = Vec4V(V_Z_AXIS_WZERO);
#endif
			Vec3V tri[3] = {p0, p1, p2};
			Vec3V edge[2];

			if (PolyClipEdge(edge, tri, NELEM(tri), waterPlane))
			{
				const float scale = 512.0f/(float)Min<int>(gv::WORLD_BOUNDS_MAX_X - gv::WORLD_BOUNDS_MIN_X, gv::WORLD_BOUNDS_MAX_Y - gv::WORLD_BOUNDS_MIN_Y);
				const float offset = 40.0f;

				const float x0 = (edge[0].GetXf() - (float)gv::WORLD_BOUNDS_MIN_X)*scale + offset;
				const float y0 = (edge[0].GetYf() - (float)gv::WORLD_BOUNDS_MIN_Y)*scale + offset;
				const float x1 = (edge[1].GetXf() - (float)gv::WORLD_BOUNDS_MIN_X)*scale + offset;
				const float y1 = (edge[1].GetYf() - (float)gv::WORLD_BOUNDS_MIN_Y)*scale + offset;
#if __BANK
				m_stats_waterEdgeCountCurrent++;
#endif // __BANK
				m_waterBoundaryEdgesCurrent++;

				if (m_waterBoundaryFile)
				{
					fprintf(m_waterBoundaryFile, "%f %f moveto %f %f lineto ", x0, y0, x1, y1);

					if (m_waterBoundaryEdgesCurrent%64 == 0)
					{
						fprintf(m_waterBoundaryFile, "\n ");
					}
				}
			}
#if __BANK
			m_stats_triangleCountCurrent++;
#endif // __BANK
		}

		if (bIsWaterCollision && m_waterBoundaryFile)
		{
			g_WorldHeightMapRiverBoundary.AddTriangle(p0, p1, p2);
		}
	}
}

template <typename CellType> void CHeightMap<CellType>::RasteriseGeometryBegin()
{
	if (!IsEnabled())
	{
		return;
	}

	ConvertToNonRLE(); // in case we've loaded an RLE height map and we want to build "on top of it"

#if __ASSERT
	if (!AssertVerify(!m_waterUpdate && !m_geomUpdate)) { return; }
	if (!AssertVerify(m_dataRLE == NULL))               { return; }
#endif // __ASSERT

	m_geomUpdate = true;
}

template <typename CellType> void CHeightMap<CellType>::RasteriseGeometryEnd()
{
	if (!IsEnabled())
	{
		return;
	}

#if __ASSERT
	if (!AssertVerify(!m_waterUpdate && m_geomUpdate)) { return; }
	if (!AssertVerify(m_dataRLE == NULL))              { return; }
#endif // __ASSERT

	if (m_waterBoundaryFile)
	{
		if (m_waterBoundaryEdgesCurrent > 0)
		{
			fprintf(m_waterBoundaryFile, "stroke\n");
			m_waterBoundaryFile->Flush();
			m_waterBoundaryEdgesCurrent = 0;
		}

		// river boundary
		{
			atArray<CHeightMapRiverBoundary::CRiverEdge> edges;

			g_WorldHeightMapRiverBoundary.GetEdges(edges);

			if (edges.GetCount() > 0)
			{
				for (int i = 0; i < edges.GetCount(); i++)
				{
					const Vec2V p0 = edges[i].GetP0();
					const Vec2V p1 = edges[i].GetP1();

					const float scale = 512.0f/(float)Min<int>(gv::WORLD_BOUNDS_MAX_X - gv::WORLD_BOUNDS_MIN_X, gv::WORLD_BOUNDS_MAX_Y - gv::WORLD_BOUNDS_MIN_Y);
					const float offset = 40.0f;

					const float x0 = (p0.GetXf() - (float)gv::WORLD_BOUNDS_MIN_X)*scale + offset;
					const float y0 = (p0.GetYf() - (float)gv::WORLD_BOUNDS_MIN_Y)*scale + offset;
					const float x1 = (p1.GetXf() - (float)gv::WORLD_BOUNDS_MIN_X)*scale + offset;
					const float y1 = (p1.GetYf() - (float)gv::WORLD_BOUNDS_MIN_Y)*scale + offset;

					fprintf(m_waterBoundaryFile, "%f %f moveto %f %f lineto ", x0, y0, x1, y1);
				}

				fprintf(m_waterBoundaryFile, "stroke\n");
			}

			g_WorldHeightMapRiverBoundary.m_tris.Reset();
		}
	}

	m_geomUpdate = false;

#if __BANK
	m_stats_geometryCountTotal++;
	m_stats_triangleCountTotal += m_stats_triangleCountCurrent;
	m_stats_triangleCountCurrent = 0;
	m_stats_waterEdgeCountTotal += m_stats_waterEdgeCountCurrent;
	m_stats_waterEdgeCountCurrent = 0;
	m_stats_cellsCovered = 0;
	m_stats_cellsUpdated = 0;
#endif // __BANK
}

#if __BANK
template <typename CellType> void CHeightMap<CellType>::RasteriseGeometryShowStats(const char* modelName, int modelGeomIndex, Vec3V_In modelPos) const
{
	if (!IsEnabled())
	{
		return;
	}

	if (m_stats_waterEdgeCountCurrent > 0)
	{
		Displayf(
			"> %04d tris and %03d water edges%s covered %05d cells, %05d updated - %s/%d at %.2f,%.2f,%.2f",
			m_stats_triangleCountCurrent,
			m_stats_waterEdgeCountCurrent,
			m_waterBoundaryFile ? "" : " (NO FILE)",
			m_stats_cellsCovered,
			m_stats_cellsUpdated,
			modelName,
			modelGeomIndex,
			VEC3V_ARGS(modelPos)
		);
	}
	else
	{
		Displayf(
			"> %04d tris covered %05d cells, %05d updated - %s/%d at %.2f,%.2f,%.2f",
			m_stats_triangleCountCurrent,
			m_stats_cellsCovered,
			m_stats_cellsUpdated,
			modelName,
			modelGeomIndex,
			VEC3V_ARGS(modelPos)
		);
	}
}
#endif // __BANK

template <typename CellType> void CHeightMap<CellType>::RasteriseWaterBegin()
{
	if (!IsEnabled())
	{
		return;
	}

	ConvertToNonRLE(); // in case we've loaded an RLE height map and we want to build "on top of it"

#if __ASSERT
	if (!AssertVerify(!m_waterUpdate && !m_geomUpdate)) { return; }
	if (!AssertVerify(m_dataRLE == NULL))               { return; }
#endif // __ASSERT

	m_waterUpdate = true;
}

template <typename CellType> void CHeightMap<CellType>::RasteriseWaterEnd()
{
	if (!IsEnabled())
	{
		return;
	}

#if __ASSERT
	if (!AssertVerify(m_waterUpdate && !m_geomUpdate)) { return; }
	if (!AssertVerify(m_dataRLE == NULL))              { return; }
#endif // __ASSERT

	m_waterUpdate = false;

	if (m_dataMax && m_waterMask)
	{
		for (int j = 0; j < m_numRows; j++)
		{
			for (int i = 0; i < m_numCols; i++)
			{
				const int index = i + j*m_numCols;

				if (m_dataMax[index] == (CellType)0)
				{
					m_waterMask[index/8] |= BIT(index%8);
				}
			}
		}
	}
}

template <typename CellType> bool CHeightMap<CellType>::GetIsWaterCell(int i, int j) const
{
	Assert(i >= 0 && i < m_numCols);
	Assert(j >= 0 && j < m_numRows);

	if (m_waterMask)
	{
		const int index = i + j*m_numCols;

		if (m_waterMask[index/8] & BIT(index%8))
		{
			return true;
		}
	}

	return false;
}

template <typename CellType> int CHeightMap<CellType>::CountWaterCells() const
{
	int count = 0;

	if (m_waterMask)
	{
		for (int j = 0; j < m_numRows; j++)
		{
			for (int i = 0; i < m_numCols; i++)
			{
				if (GetIsWaterCell(i, j))
				{
					count++;
				}
			}
		}
	}

	return count;
}

#if __BANK

template <typename CellType> void CHeightMap<CellType>::AddWidgets(bkBank* pBank, const char* groupName)
{
	const int cellSizeML = gv::WORLD_CELL_SIZE;
	const int resScaleML = 1;
	const int cellSizeHR = gv::WORLD_CELL_SIZE;
	const int resScaleHR = 3;

	if (groupName)
	{
		pBank->PushGroup(groupName, false);
	}

	m_interface.AddWidgets(pBank);

	pBank->AddToggle("Enabled", &m_enabled, datCallback(CFA1(CGameWorldHeightMap::SetEnableHeightmap), (CallbackData)this));

	pBank->AddToggle("Show height map"             , &m_worldHeightMapDebugDraw);
	pBank->AddSlider("Show height map extent"      , &m_worldHeightMapDebugDrawExtent, 1.0f, 16.0f, 1.0f);
	pBank->AddToggle("Show height map solid top"   , &m_worldHeightMapDebugDrawSolidTop);
	pBank->AddToggle("Show height map solid bottom", &m_worldHeightMapDebugDrawSolidBottom);

	pBank->AddButton("Reset", datCallback(MFA(CHeightMap<CellType>::Interface_Reset), this));

#define ResetML CHeightMapBoundsInterface::ResetBoundsToMainLevel_<cellSizeML,resScaleML>
#define ResetHR CHeightMapBoundsInterface::ResetBoundsToMainLevel_<cellSizeHR,resScaleHR>

	if      (this == (void*)&g_WorldHeightMapML) { pBank->AddButton("Reset bounds", datCallback(MFA(ResetML), &m_interface)); }
	else if (this == (void*)&g_WorldHeightMapHR) { pBank->AddButton("Reset bounds", datCallback(MFA(ResetHR), &m_interface)); }

#undef ResetML
#undef ResetHR

	pBank->AddSlider("MinZ @ Player Cell", &m_playerLocationMapMinZ, gv::WORLD_BOUNDS_MIN_Z, gv::WORLD_BOUNDS_MAX_Z, 1.0f);
	pBank->AddSlider("MaxZ @ Player Cell", &m_playerLocationMapMaxZ, gv::WORLD_BOUNDS_MIN_Z, gv::WORLD_BOUNDS_MAX_Z, 1.0f);
	pBank->AddButton("Convert to RLE"      , datCallback(MFA(CHeightMap<CellType>::ConvertToRLE), this));
	pBank->AddButton("Convert to non-RLE"  , datCallback(MFA(CHeightMap<CellType>::ConvertToNonRLE), this));
	pBank->AddToggle("Edit on the fly",		&m_editOnTheFlyMode);	// Allow the game to set the heightmap value based off the player position
	pBank->AddToggle("Edit Min Heights",	&m_editOnTheFlyMinHeights);
	pBank->AddSlider("Edit on the fly offset", &m_editOnTheFlyOffset, 1.0f, 20.0f, 1.0f);	// Height offset from the player z
	pBank->AddButton("Set All Min to Max", datCallback(MFA(CHeightMap<CellType>::Interface_SetMinToMax), this));
	pBank->AddText  ("Load/save path"      , &m_interface.m_path[0], sizeof(m_interface.m_path), false);
	pBank->AddButton("Load"                , datCallback(MFA(CHeightMap<CellType>::Interface_Load), this));
	pBank->AddButton("Save"                , datCallback(MFA(CHeightMap<CellType>::Interface_Save), this));
	pBank->AddButton("Save with water mask", datCallback(MFA(CHeightMap<CellType>::Interface_SaveWithWaterMask), this));
	pBank->AddText  ("Import/export path"  , &m_interface.m_pathImportExportDDS[0], sizeof(m_interface.m_pathImportExportDDS), false);
	pBank->AddButton("Import from DDS"     , datCallback(MFA(CHeightMap<CellType>::Interface_ImportDDS), this));
	pBank->AddButton("Export to DDS"       , datCallback(MFA(CHeightMap<CellType>::Interface_ExportDDS), this));

	if (groupName)
	{
		pBank->PopGroup();
	}
}

#if __BANK
static bool g_WorldHeightMapDebugDrawTestSamples = false;
static bool g_WorldHeightMapDebugDrawTestSamplesUpdate = false;
#endif // __BANK

template <typename CellType> void CHeightMap<CellType>::DebugDraw(Vec3V_In camPos, Color32 meshColour)
{
	if(!m_worldHeightMapDebugDraw)
		return;

	float extent = m_worldHeightMapDebugDrawExtent;
	bool bSolidTop = m_worldHeightMapDebugDrawSolidTop;
	bool bSolidBottom = m_worldHeightMapDebugDrawSolidBottom;

	const float x0 = camPos.GetXf() - m_cellSizeX*extent;
	const float y0 = camPos.GetYf() - m_cellSizeX*extent;
	const float x1 = camPos.GetXf() + m_cellSizeX*extent;
	const float y1 = camPos.GetYf() + m_cellSizeX*extent;

	int i0, j0, i1, j1;
	if (GetIndexAtBound(i0, j0, i1, j1, x0, y0, x1, y1))
	{
		const Color32 pointColour = Color32(255,255,128,255);
		const Color32 waterColour = Color32(0,64,255,255);
		const Color32 mesh0Colour = Color32(0,0,255,255);
		const Color32 mesh1Colour = m_enabled ? meshColour : Color32(meshColour.GetRed(), meshColour.GetGreen(), meshColour.GetBlue(), 125);

		const float dx = 0.5f*m_cellSizeX;
		const float dy = 0.5f*m_cellSizeY;

		for (int j = j0; j <= j1; j++)
		{
			for (int i = i0; i <= i1; i++)
			{
				const float x = m_boundsMinX + m_cellSizeX*(0.5f + (float)i);
				const float y = m_boundsMinY + m_cellSizeY*(0.5f + (float)j);

				const float z0 = GetMinHeightAtIndex(i, j);
				const float z1 = GetMaxHeightAtIndex(i, j);

				const bool bIsWaterCell = GetIsWaterCell(i, j);

				const Vec3V p000 = Vec3V(x - dx, y - dy, z0);
				const Vec3V p100 = Vec3V(x + dx, y - dy, z0);
				const Vec3V p010 = Vec3V(x - dx, y + dy, z0);
				const Vec3V p110 = Vec3V(x + dx, y + dy, z0);
				const Vec3V p001 = Vec3V(x - dx, y - dy, z1);
				const Vec3V p101 = Vec3V(x + dx, y - dy, z1);
				const Vec3V p011 = Vec3V(x - dx, y + dy, z1);
				const Vec3V p111 = Vec3V(x + dx, y + dy, z1);

				if (bSolidBottom)
				{
					grcDebugDraw::Quad(p000, p100, p110, p010, mesh0Colour, true, true);
				}
				else
				{
					grcDebugDraw::Line(p000, p100, mesh0Colour);
					grcDebugDraw::Line(p100, p110, mesh0Colour);
					grcDebugDraw::Line(p110, p010, mesh0Colour);
					grcDebugDraw::Line(p010, p000, mesh0Colour);

					grcDebugDraw::Cross(Vec3V(x, y, z0), 0.25f*m_cellSizeX, bIsWaterCell ? waterColour : pointColour);
				}

				if (bSolidTop)
				{
					grcDebugDraw::Quad(p001, p101, p111, p011, mesh1Colour, true, true);
				}
				else
				{
					grcDebugDraw::Line(p001, p101, mesh1Colour);
					grcDebugDraw::Line(p101, p111, mesh1Colour);
					grcDebugDraw::Line(p111, p011, mesh1Colour);
					grcDebugDraw::Line(p011, p001, mesh1Colour);

					grcDebugDraw::Cross(Vec3V(x, y, z1), 0.25f*m_cellSizeX, bIsWaterCell ? waterColour : pointColour);
				}
			}
		}

		for (int j = j0; j <= j1 + 1; j++)
		{
			for (int i = i0; i <= i1 + 1; i++)
			{
				const float x = m_boundsMinX + m_cellSizeX*(float)i;
				const float y = m_boundsMinY + m_cellSizeY*(float)j;

				float z0min = (float)GetCellMaxValue<CellType>(0.0f);
				float z0max = 0.0f;
				float z1min = (float)GetCellMaxValue<CellType>(0.0f);
				float z1max = 0.0f;

				for (int jj = j - 1; jj <= j; jj++)
				{
					for (int ii = i - 1; ii <= i; ii++)
					{
						if (ii >= 0 && ii < m_numCols &&
							jj >= 0 && jj < m_numRows)
						{
							const float z0 = GetMinHeightAtIndex(ii, jj);
							const float z1 = GetMaxHeightAtIndex(ii, jj);

							if (z0min > z0max)
							{
								z0min = z0max = z0;
							}
							else
							{
								z0min = Min<float>(z0, z0min);
								z0max = Max<float>(z0, z0max);
							}

							if (z1min > z1max)
							{
								z1min = z1max = z1;
							}
							else
							{
								z1min = Min<float>(z1, z1min);
								z1max = Max<float>(z1, z1max);
							}
						}
					}
				}

				if (z1min < z1max)
				{
					grcDebugDraw::Line(Vec3V(x, y, z0min), Vec3V(x, y, Min<float>(z0max, z1min)), mesh0Colour);
					grcDebugDraw::Line(Vec3V(x, y, z1max), Vec3V(x, y, Max<float>(z1min, z0max)), mesh1Colour);

					if (z0max > z1min)
					{
						grcDebugDraw::Line(Vec3V(x, y, z1min), Vec3V(x, y, z0max), mesh1Colour, mesh0Colour);
					}
				}
			}
		}
	}

	if (g_WorldHeightMapDebugDrawTestSamples)
	{
		static Vec3V* samples = NULL;

		if (samples == NULL)
		{
			samples = rage_new Vec3V[(10*2 + 1)*(10*2 + 1)];
		}

		if (g_WorldHeightMapDebugDrawTestSamplesUpdate)
		{
			for (int j = -10; j <= 10; j++)
			{
				for (int i = -10; i <= 10; i++)
				{
					Vec3V samplePos = camPos + Vec3V((float)(i*2), (float)(j*2), 0.0f);
					const float sampleZ = GetMaxHeightAtCoord(samplePos.GetXf(), samplePos.GetYf());
					samplePos.SetZf(sampleZ);
					samples[(i + 10) + (j + 10)*(10*2 + 1)] = samplePos;
				}
			}
		}

		for (int i = -10; i <= 10; i++)
		{
			for (int j = -10; j < 10; j++)
			{
				const Vec3V p0 = samples[(i + 10) + (j + 10 + 0)*(10*2 + 1)];
				const Vec3V p1 = samples[(i + 10) + (j + 10 + 1)*(10*2 + 1)];
				grcDebugDraw::Line(p0, p1, Color32(255,255,0,255));
			}
		}

		for (int j = -10; j <= 10; j++)
		{
			for (int i = -10; i < 10; i++)
			{
				const Vec3V p0 = samples[(i + 10 + 0) + (j + 10)*(10*2 + 1)];
				const Vec3V p1 = samples[(i + 10 + 1) + (j + 10)*(10*2 + 1)];
				grcDebugDraw::Line(p0, p1, Color32(255,255,0,255));
			}
		}
	}


	if(m_dataMin && m_dataMin && !m_dataRLE)
	{
		if(m_editOnTheFlyMode)
		{
			Vec3V playerPos = FindPlayerPed()->GetMatrix().d();
			float playerHeight = playerPos.GetZf();
			if(m_editOnTheFlyMinHeights)
			{
				float mapHeight = GetMinHeightAtCoord(playerPos.GetXf(), playerPos.GetYf());

				if((playerHeight - m_editOnTheFlyOffset) < mapHeight)
				{
					SetMinHeightAtCoord(playerPos.GetXf(), playerPos.GetYf(), playerHeight - m_editOnTheFlyOffset);
					m_playerLocationMapMaxZ = GetMaxHeightAtCoord(playerPos.GetXf(), playerPos.GetYf());
				}
			}
			else
			{
				float mapHeight = GetMaxHeightAtCoord(playerPos.GetXf(), playerPos.GetYf());

				if((playerHeight + m_editOnTheFlyOffset) > mapHeight)
				{
					SetMaxHeightAtCoord(playerPos.GetXf(), playerPos.GetYf(), playerHeight + m_editOnTheFlyOffset);
					m_playerLocationMapMinZ = GetMinHeightAtCoord(playerPos.GetXf(), playerPos.GetYf());
				}
			}
		}
 		else
 		{
 			int currentCellI = -1;
 			int currentCellJ = -1;
 
 			Vec3V location = FindPlayerPed()->GetMatrix().d();
 			GetIndexAtCoord(currentCellI, currentCellJ, location.GetXf(), location.GetYf());
 
 			if(currentCellI != m_cellI || currentCellJ != m_cellJ)
 			{
 				m_cellI = currentCellI;
 				m_cellJ = currentCellJ;
 
 				m_playerLocationMapMinZ = GetMinHeightAtCoord(location.GetXf(), location.GetYf());
 				m_playerLocationMapMaxZ = GetMaxHeightAtCoord(location.GetXf(), location.GetYf());
 			}
 
 			static float playerLocationMapMinZ = 0.0f;
 			playerLocationMapMinZ = GetMinHeightAtCoord(location.GetXf(), location.GetYf());
 			static float playerLocationMapMaxZ = 0.0f;
 			playerLocationMapMaxZ = GetMaxHeightAtCoord(location.GetXf(), location.GetYf());
 
 			if(playerLocationMapMinZ != m_playerLocationMapMinZ)
 			{
 				SetMinHeightAtCoord(location.GetXf(), location.GetYf(), m_playerLocationMapMinZ);
 				m_playerLocationMapMinZ = GetMinHeightAtCoord(location.GetXf(), location.GetYf());
 			}
 
 			if(playerLocationMapMaxZ != m_playerLocationMapMaxZ)
 			{
 				SetMaxHeightAtCoord(location.GetXf(), location.GetYf(), m_playerLocationMapMaxZ);
 				m_playerLocationMapMaxZ = GetMaxHeightAtCoord(location.GetXf(), location.GetYf());
 			}
 		}
	}
	else
	{
		m_editOnTheFlyMode = false;
	}
}

#endif // __BANK
#endif // HEIGHTMAP_TOOL

// ================================================================================================

#if __BANK

static bool  g_WorldHeightMapDebugDrawHR = false;
//static char  g_WorldHeightMapWaterBoundaryPath[80] = HEIGHTMAP_TOOL_DEBUG_OUTPUT_DIR"/waterboundary.eps";

#endif // __BANK

void CGameWorldHeightMap::Init(unsigned initMode)
{
	if (initMode == INIT_AFTER_MAP_LOADED)
	{
		//Get the heightmap data file path
		const CDataFileMgr::DataFile* pData = DATAFILEMGR.GetLastFile(CDataFileMgr::WORLD_HEIGHTMAP_FILE);

		if(DATAFILEMGR.IsValid(pData))
		{
			g_WorldHeightMap.Load(pData->m_filename);
			g_WorldHeightMap.SetEnabled(true); // Main heightmap is always enabled
		}

		//Probably shouldn't have more than 1 heightmap datafile.. Looks like code will just overwrite heightmap with each file loaded.
		// Edit: we have an extra heightmap for the heistisland dlc
		//Assertf(!DATAFILEMGR.IsValid(DATAFILEMGR.GetPrevFile(pData)), "More than 1 world height map data file was defined! Only loaded the last file that was defined: %s %s", pData->m_filename, DATAFILEMGR.GetPrevFile(pData)->m_filename);

#if __BANK
		strcpy(g_WorldHeightMapML.m_interface.m_pathImportExportDDS, HEIGHTMAP_TOOL_DEBUG_OUTPUT_DIR"/heightmap_level");
		strcpy(g_WorldHeightMapHR.m_interface.m_pathImportExportDDS, HEIGHTMAP_TOOL_DEBUG_OUTPUT_DIR"/heightmap_highres");
		strcpy(g_WorldHeightMapHR.m_interface.m_path,                HEIGHTMAP_TOOL_DEBUG_OUTPUT_DIR"/heightmap_highres.dat");
#endif // __BANK
	}
}

void CGameWorldHeightMap::LoadExtraFile(char const* filename)
{
	g_AuxHeightMaps[0].Load(filename);
}

void CGameWorldHeightMap::EnableHeightmap(char const* heightmapname, bool enable)
{
	for(int i = 0; i < NELEM(g_AuxHeightMaps); ++i)
	{
		if((g_AuxHeightMaps[i].IsEnabled() != enable) && (strstr(g_AuxHeightMaps[i].m_path, heightmapname) != nullptr))
		{
			g_AuxHeightMaps[i].SetEnabled(enable);
			return;
		}
	}
}


void CGameWorldHeightMap::SetEnableHeightmap(CHeightMapBoundsInterface* heightmap)
{
	CHeightMap<CellTypeDefault>* pHm = static_cast<CHeightMap<CellTypeDefault>*>(heightmap);
	bool toEnable = pHm->IsEnabled();
	pHm->SetEnabled(!toEnable); // Do a crappy toggle back so the check in EnableHeightmap will work

	char* lastSlash = strrchr(pHm->m_path, '/');
	++lastSlash;

	char heightmapname[64] = {0};
	strcpy_s(heightmapname, sizeof(heightmapname), lastSlash);
	char* dot = strrchr(heightmapname, '.');
	if(dot)
		*dot = 0;

	EnableHeightmap(heightmapname, toEnable);
}

void CGameWorldHeightMap::Shutdown(unsigned UNUSED_PARAM(shutdownMode))
{
}

#if __BANK

static int s_WarpToLocationImageSizeX = 0;
static int s_WarpToLocationImageSizeY = 0;
static int s_WarpToLocationImageScale = 1;
static int s_WarpToLocationPosX = 0;
static int s_WarpToLocationPosY = 0;

static void WarpToLocationAboveHeightmap_button()
{
	float z = 0.0f;

	if (camInterface::GetDebugDirector().IsFreeCamActive())
	{
		z = camInterface::GetDebugDirector().GetFreeCamFrameNonConst().GetPosition().z;
	}
	else
	{
		camInterface::GetDebugDirector().ActivateFreeCam();
		z = 500.0f;
	}

	const float worldMinX = (float)gv::WORLD_BOUNDS_MIN_X;
	const float worldMinY = (float)gv::WORLD_BOUNDS_MIN_Y;
	const float worldMaxX = (float)gv::WORLD_BOUNDS_MAX_X;
	const float worldMaxY = (float)gv::WORLD_BOUNDS_MAX_Y;

	const float x = worldMinX + (worldMaxX - worldMinX)*(float)s_WarpToLocationPosX/(float)(s_WarpToLocationImageSizeX*s_WarpToLocationImageScale);
	const float y = worldMaxY + (worldMinY - worldMaxY)*(float)s_WarpToLocationPosY/(float)(s_WarpToLocationImageSizeY*s_WarpToLocationImageScale); // flip in y

	camInterface::GetDebugDirector().GetFreeCamFrameNonConst().SetPosition(Vector3(x, y, z));

	// figure out what tri file this would be
	const int triFileX = gv::WORLD_CELLS_PER_TILE*(int)floorf((x - (float)gv::NAVMESH_BOUNDS_MIN_X)/(float)gv::WORLD_TILE_SIZE);
	const int triFileY = gv::WORLD_CELLS_PER_TILE*(int)floorf((y - (float)gv::NAVMESH_BOUNDS_MIN_Y)/(float)gv::WORLD_TILE_SIZE);

	Displayf("warped to %f,%f: navmesh[%d][%d].tri", x, y, triFileX, triFileY);
}

static float g_testCoordX = 0.0f;
static float g_testCoordY = 0.0f;
static float g_testCoordR = 15.0f; // radius

void CGameWorldHeightMap::AddWidgets(bkBank* pBank)
{
	s_WarpToLocationImageSizeX = ((gv::WORLD_BOUNDS_MAX_X - gv::WORLD_BOUNDS_MIN_X)/gv::WORLD_CELL_SIZE)*gv::WORLD_CELL_RESOLUTION;
	s_WarpToLocationImageSizeY = ((gv::WORLD_BOUNDS_MAX_Y - gv::WORLD_BOUNDS_MIN_Y)/gv::WORLD_CELL_SIZE)*gv::WORLD_CELL_RESOLUTION;

	pBank->AddSlider("Warp image width", &s_WarpToLocationImageSizeX, 1, 50000, 1);
	pBank->AddSlider("Warp image height", &s_WarpToLocationImageSizeY, 1, 50000, 1);
	pBank->AddSlider("Warp image scale", &s_WarpToLocationImageScale, 1, 32, 1);
	pBank->AddSlider("Warp pos x", &s_WarpToLocationPosX, 0, 50000, 1);
	pBank->AddSlider("Warp pos y", &s_WarpToLocationPosY, 0, 50000, 1);
	pBank->AddButton("Warp to location above height map", WarpToLocationAboveHeightmap_button);
	pBank->AddButton("Toggle find collision holes mode", phMaterialDebug::ToggleRenderFindCollisionHoles);
	pBank->AddButton("Toggle find mover bounds holes mode", phMaterialDebug::ToggleRenderFindCollisionHolesMoverMapBounds);
	pBank->AddButton("Toggle find weapon bounds holes mode", phMaterialDebug::ToggleRenderFindCollisionHolesWeaponMapBounds);

	pBank->AddSeparator();

 	pBank->AddToggle("Show height map (high-res)"  , &g_WorldHeightMapDebugDrawHR);
	pBank->AddToggle("Test samples"                , &g_WorldHeightMapDebugDrawTestSamples);
	pBank->AddToggle("Test samples update"         , &g_WorldHeightMapDebugDrawTestSamplesUpdate);

	g_WorldHeightMapML.AddWidgets(pBank, "Main level height map");
	pBank->PushGroup("Aux heightmaps");
	for(int i = 0; i < NELEM(g_AuxHeightMaps); ++i)
	{
		g_AuxHeightMaps[i].AddWidgets(pBank, g_AuxHeightMaps[i].m_interface.m_path);
	}
	pBank->PopGroup();
	g_WorldHeightMapHR.AddWidgets(pBank, "High-res height map");

	pBank->AddSeparator();

	// getting a single heightmap sample min/max value
	{
		class GetHeightMapValues_button { public: static void func()
		{
			if (g_testCoordR == 0.0f)
			{
				const float z0 = GetMinHeightFromWorldHeightMap(g_testCoordX, g_testCoordY);
				const float z1 = GetMaxHeightFromWorldHeightMap(g_testCoordX, g_testCoordY);

				Displayf("heightmap @ (%f,%f) min = %f, max = %f", g_testCoordX, g_testCoordY, z0, z1);
			}
			else
			{
				const float z0 = GetMinHeightFromWorldHeightMap(g_testCoordX - g_testCoordR, g_testCoordY - g_testCoordR, g_testCoordX + g_testCoordR, g_testCoordY + g_testCoordR);
				const float z1 = GetMaxHeightFromWorldHeightMap(g_testCoordX - g_testCoordR, g_testCoordY - g_testCoordR, g_testCoordX + g_testCoordR, g_testCoordY + g_testCoordR);

				Displayf("heightmap @ (%f,%f,r=%f) min = %f, max = %f", g_testCoordX, g_testCoordY, g_testCoordR, z0, z1);
			}
		}};

		pBank->AddSlider("Get height map values x", &g_testCoordX, -9999.0f, 9999.0f, 0.1f);
		pBank->AddSlider("Get height map values y", &g_testCoordY, -9999.0f, 9999.0f, 0.1f);
		pBank->AddSlider("Get height map values r", &g_testCoordR, -9999.0f, 9999.0f, 0.1f);
		pBank->AddButton("Get height map values", GetHeightMapValues_button::func);
	}

	// dumping heightmap values to text file
	{
		static char s_dumpPath[80] = "assets:/non_final/heightmapdump.txt";

		class DumpHeightMapValuesToFile_button { public: static void func()
		{
			fiStream* fp = fiStream::Create(s_dumpPath);

			for (int minmax = 0; minmax < 2; minmax++)
			{
				fprintf(fp, "%s heightmap values:\n", minmax ? "max" : "min");

				for (int i = 0, y = gv::WORLD_BOUNDS_MIN_Y - gv::WORLD_CELL_SIZE/2; y <= gv::WORLD_BOUNDS_MAX_Y + gv::WORLD_CELL_SIZE/2; y += gv::WORLD_CELL_SIZE, i++)
				{
					char line[2048] = "";

					for (int x = gv::WORLD_BOUNDS_MIN_X - gv::WORLD_CELL_SIZE/2; x <= gv::WORLD_BOUNDS_MAX_X + gv::WORLD_CELL_SIZE/2; x += gv::WORLD_CELL_SIZE)
					{
						char temp[6];
						sprintf(temp, "%.1f", minmax ? GetMaxHeightFromWorldHeightMap((float)x, (float)y) : GetMinHeightFromWorldHeightMap((float)x, (float)y));

						while (strlen(temp) < NELEM(temp) - 1)
						{
							strcpy(temp, atVarString(" %s", temp).c_str());
						}

						if (line[0] != '\0')
						{
							strcat(line, " ");
						}

						strcat(line, temp);
					}

					fprintf(fp, "line %03d: %s\n", i, line);
				}

				fprintf(fp, "\n");
			}

			fp->Close();
		}};

		pBank->AddText  ("Dump height map values path", &s_dumpPath[0], sizeof(s_dumpPath), false);
		pBank->AddButton("Dump height map values", DumpHeightMapValuesToFile_button::func);
	}
}

void CGameWorldHeightMap::DebugDraw(Vec3V_In camPos)

{
	{
		if (g_WorldHeightMapHR.m_data && g_WorldHeightMapDebugDrawHR)
		{
			g_WorldHeightMapHR.DebugDraw(camPos, Color32(255,0,0,255));
		}
		else
		{
			g_WorldHeightMap.DebugDraw(camPos, Color32(255,0,0,255));

			g_AuxHeightMaps[0].DebugDraw(camPos, Color32(0,255,0,255));
		}
	}
}

#endif // __BANK


CHeightMap<CellTypeDefault>* GetHeightMap(float x, float y)
{
	int i0 = 0;
	int j0 = 0;
	if(g_WorldHeightMap.GetIndexAtCoord(i0, j0, x, y))
	{
		return &g_WorldHeightMap;
	}

	for(int i = 0; i < NELEM(g_AuxHeightMaps); ++i)
	{
		CHeightMap<CellTypeDefault>& heightmap = g_AuxHeightMaps[i];
		if(heightmap.IsEnabled() && heightmap.GetIndexAtCoord(i0, j0, x, y))
		{
			return &heightmap;
		}
	}

	// To preserve old behaviour where this fn is called we return the world heightmap as a last resort if it's valid
	return g_WorldHeightMap.IsValid() ? &g_WorldHeightMap : nullptr;
}

CHeightMap<CellTypeDefault>* GetHeightMap(float x0, float y0, float x1, float y1)
{
	int i0, j0, i1, j1;
	if(g_WorldHeightMap.GetIndexAtBound(i0, j0, i1, j1, x0, y0, x1, y1))
	{
		return &g_WorldHeightMap;
	}

	for(int i = 0; i < NELEM(g_AuxHeightMaps); ++i)
	{
		CHeightMap<CellTypeDefault>& heightmap = g_AuxHeightMaps[i];
		if(heightmap.IsEnabled() && heightmap.GetIndexAtBound(i0, j0, i1, j1, x0, y0, x1, y1))
		{
			return &heightmap;
		}
	}

	// To preserve old behaviour where this fn is called we return the world heightmap as a last resort if it's valid
	return g_WorldHeightMap.IsValid() ? &g_WorldHeightMap : nullptr;
}

// TODO -- need to output sensible values for prologue area, i.e. if the camera is _not_ within the main map bounds

float CGameWorldHeightMap::GetMinHeightFromWorldHeightMap(float x, float y)
{
	CHeightMap<CellTypeDefault> const* pHeightmap = GetHeightMap(x, y);
	if (pHeightmap && pHeightmap->IsValid())
	{
		Assertf(FPIsFinite(x), "x is not finite (%f)", x);
		Assertf(FPIsFinite(y), "y is not finite (%f)", y);

		return pHeightmap->GetMinHeightAtCoord(x, y);
	}

	return 0.0f;
}

float CGameWorldHeightMap::GetMaxHeightFromWorldHeightMap(float x, float y)
{
	CHeightMap<CellTypeDefault> const* pHeightmap = GetHeightMap(x, y);
	if (pHeightmap && pHeightmap->IsValid())
	{
		Assertf(FPIsFinite(x), "x is not finite (%f)", x);
		Assertf(FPIsFinite(y), "y is not finite (%f)", y);

		return pHeightmap->GetMaxHeightAtCoord(x, y);
	}

	return (float)gv::WORLD_BOUNDS_MAX_Z;
}

float CGameWorldHeightMap::GetMinHeightFromWorldHeightMap(float x0, float y0, float x1, float y1)
{
	if(Verifyf(FPIsFinite(x0) && FPIsFinite(y0) && FPIsFinite(x1) && FPIsFinite(y1), "One or more coordinates is not finite %f, %f, %f, %f", x0, y0, x1, y1))
	{
		CHeightMap<CellTypeDefault> const* pHeightmap = GetHeightMap(x0, y0, x1, y1);
		if (pHeightmap && pHeightmap->IsValid())
		{
			return pHeightmap->GetMinHeightAtBound(x0, y0, x1, y1);
		}
	}

	return 0.0f;
}

float CGameWorldHeightMap::GetMaxHeightFromWorldHeightMap(float x0, float y0, float x1, float y1)
{
	CHeightMap<CellTypeDefault> const* pHeightmap = GetHeightMap(x0, y0, x1, y1);
	if (pHeightmap && pHeightmap->IsValid())
	{
		Assertf(FPIsFinite(x0), "x0 is not finite (%f)", x0);
		Assertf(FPIsFinite(y0), "y0 is not finite (%f)", y0);
		Assertf(FPIsFinite(x1), "x1 is not finite (%f)", x1);
		Assertf(FPIsFinite(y1), "y1 is not finite (%f)", y1);

		return pHeightmap->GetMaxHeightAtBound(x0, y0, x1, y1);
	}

	return (float)gv::WORLD_BOUNDS_MAX_Z;
}

float CGameWorldHeightMap::GetMinIntervalHeightFromWorldHeightMap(float x0, float y0, float x1, float y1, float maxStep)
{
	const float dist = sqrtf((x1 - x0)*(x1 - x0) + (y1 - y0)*(y1 - y0));
	const int numSegments = (int)ceilf(dist/maxStep);
	float z = (float)gv::WORLD_BOUNDS_MAX_Z;

	if (numSegments > 0.0f)
	{
		const float dx = (x1 - x0)/(float)numSegments;
		const float dy = (y1 - y0)/(float)numSegments;

		float x = x0;
		float y = y0;

		for (int i = 0; i <= numSegments; i++)
		{
			z = Min<float>(GetMinHeightFromWorldHeightMap(x, y), z);

			x += dx;
			y += dy;
		}
	}
	else
	{
		z = GetMinHeightFromWorldHeightMap(x0, y0);
	}

	return z;
}

float CGameWorldHeightMap::GetMaxIntervalHeightFromWorldHeightMap(float x0, float y0, float x1, float y1, float maxStep)
{
	const float dist = sqrtf((x1 - x0)*(x1 - x0) + (y1 - y0)*(y1 - y0));
	const int numSegments = (int)ceilf(dist/maxStep);
	float z = 0.0f;

	if (numSegments > 0)
	{
		const float dx = (x1 - x0)/(float)numSegments;
		const float dy = (y1 - y0)/(float)numSegments;

		float x = x0;
		float y = y0;

		for (int i = 0; i <= numSegments; i++)
		{
			z = Max<float>(GetMaxHeightFromWorldHeightMap(x, y), z);

			x += dx;
			y += dy;
		}
	}
	else
	{
		z = GetMaxHeightFromWorldHeightMap(x0, y0);
	}

	return z;
}

float CGameWorldHeightMap::GetMinInterpolatedHeightFromWorldHeightMap(float x, float y, bool bUseMinFilter)
{
	float z = 0.0f;

	CHeightMap<CellTypeDefault> const* pHeightmap = GetHeightMap(x, y);
	if (pHeightmap && pHeightmap->IsValid())
	{
		x -= 0.5f*(float)gv::WORLD_CELL_SIZE;
		y -= 0.5f*(float)gv::WORLD_CELL_SIZE;

		int i = 0;
		int j = 0;
		GetWorldHeightMapIndex(i, j, x, y);

		const float xf = (x - pHeightmap->m_boundsMinX)*pHeightmap->m_oneOverCellSizeX - (float)i; // [0..1]
		const float yf = (y - pHeightmap->m_boundsMinY)*pHeightmap->m_oneOverCellSizeY - (float)j; // [0..1]

		float h[2][2];

		if (bUseMinFilter)
		{
			// sample 4x4 grid
			float g[4][4];

			for (int jj = 0; jj < 4; jj++)
			{
				for (int ii = 0; ii < 4; ii++)
				{
					g[ii][jj] = g_WorldHeightMap.GetMinHeightAtIndex(i + ii - 1, j + jj - 1);
				}
			}

			for (int jj = 0; jj < 2; jj++)
			{
				for (int ii = 0; ii < 2; ii++)
				{
					// compute min of 3x3 neighbourhood
					h[ii][jj] = FLT_MAX;

					for (int jj2 = 0; jj2 < 3; jj2++)
					{
						for (int ii2 = 0; ii2 < 3; ii2++)
						{
							h[ii][jj] = Min<float>(g[ii + ii2][jj + jj2], h[ii][jj]);
						}
					}
				}
			}
		}
		else
		{
			h[0][0] = pHeightmap->GetMinHeightAtIndex(i + 0, j + 0);
			h[1][0] = pHeightmap->GetMinHeightAtIndex(i + 1, j + 0);
			h[0][1] = pHeightmap->GetMinHeightAtIndex(i + 0, j + 1);
			h[1][1] = pHeightmap->GetMinHeightAtIndex(i + 1, j + 1);
		}

		const float h0 = h[0][0] + (h[0][1] - h[0][0])*yf;
		const float h1 = h[1][0] + (h[1][1] - h[1][0])*yf;

		z = h0 + (h1 - h0)*xf;
	}

	return z;
}

float CGameWorldHeightMap::GetMaxInterpolatedHeightFromWorldHeightMap(float x, float y, bool bUseMaxFilter)
{
	float z = 0.0f;

	CHeightMap<CellTypeDefault> const* pHeightmap = GetHeightMap(x, y);
	if (pHeightmap && pHeightmap->IsValid())
	{
		x -= 0.5f*(float)gv::WORLD_CELL_SIZE;
		y -= 0.5f*(float)gv::WORLD_CELL_SIZE;

		int i = 0;
		int j = 0;
		GetWorldHeightMapIndex(i, j, x, y);

		const float xf = (x - pHeightmap->m_boundsMinX)*pHeightmap->m_oneOverCellSizeX - (float)i; // [0..1]
		const float yf = (y - pHeightmap->m_boundsMinY)*pHeightmap->m_oneOverCellSizeY - (float)j; // [0..1]

		float h[2][2];

		if (bUseMaxFilter)
		{
			// sample 4x4 grid
			float g[4][4];

			for (int jj = 0; jj < 4; jj++)
			{
				for (int ii = 0; ii < 4; ii++)
				{
					g[ii][jj] = pHeightmap->GetMaxHeightAtIndex(i + ii - 1, j + jj - 1);
				}
			}

			for (int jj = 0; jj < 2; jj++)
			{
				for (int ii = 0; ii < 2; ii++)
				{
					// compute max of 3x3 neighbourhood
					h[ii][jj] = 0.0f;

					for (int jj2 = 0; jj2 < 3; jj2++)
					{
						for (int ii2 = 0; ii2 < 3; ii2++)
						{
							h[ii][jj] = Max<float>(g[ii + ii2][jj + jj2], h[ii][jj]);
						}
					}
				}
			}
		}
		else
		{
			h[0][0] = pHeightmap->GetMaxHeightAtIndex(i + 0, j + 0);
			h[1][0] = pHeightmap->GetMaxHeightAtIndex(i + 1, j + 0);
			h[0][1] = pHeightmap->GetMaxHeightAtIndex(i + 0, j + 1);
			h[1][1] = pHeightmap->GetMaxHeightAtIndex(i + 1, j + 1);
		}

		const float h0 = h[0][0] + (h[0][1] - h[0][0])*yf;
		const float h1 = h[1][0] + (h[1][1] - h[1][0])*yf;

		z = h0 + (h1 - h0)*xf;
	}

	return z;
}

void CGameWorldHeightMap::GetWorldHeightMapIndex(int& i, int& j, float x, float y)
{
	CHeightMap<CellTypeDefault> const* pHeightmap = GetHeightMap(x, y);
	if (pHeightmap && pHeightmap->IsValid())
	{
		if (pHeightmap->GetIndexAtCoord(i, j, x, y))
		{
			Assertf(FPIsFinite(x), "x is not finite (%f)", x);
			Assertf(FPIsFinite(y), "y is not finite (%f)", y);

			// ignore result, index is valid even if out of range
		}
	}
}

#if HEIGHTMAP_TOOL

void CGameWorldHeightMap::ResetWorldHeightMaps(const char* waterBoundaryPath, const Vec3V* boundsMin, const Vec3V* boundsMax, int resolution, int downsample, bool bMasksOnly, bool bCreateDensityMap)
{
	if (boundsMin && boundsMax)
	{
		g_WorldHeightMap.m_boundsMinX = boundsMin->GetXf();
		g_WorldHeightMap.m_boundsMinY = boundsMin->GetYf();
		g_WorldHeightMap.m_boundsMinZ = boundsMin->GetZf();
		g_WorldHeightMap.m_boundsMaxX = boundsMax->GetXf();
		g_WorldHeightMap.m_boundsMaxY = boundsMax->GetYf();
		g_WorldHeightMap.m_boundsMaxZ = boundsMax->GetZf();

		// calculate from bounds
		g_WorldHeightMap.m_numCols = (int)((g_WorldHeightMap.m_boundsMaxX - g_WorldHeightMap.m_boundsMinX)/(float)gv::WORLD_CELL_SIZE);
		g_WorldHeightMap.m_numRows = (int)((g_WorldHeightMap.m_boundsMaxY - g_WorldHeightMap.m_boundsMinY)/(float)gv::WORLD_CELL_SIZE);
	}
	else
	{
		g_WorldHeightMap.ResetBoundsToMainLevel(gv::WORLD_CELL_SIZE, 1);
	}

	if (boundsMin && boundsMax)
	{
		g_WorldHeightMapHR.GetBoundsInterface() = g_WorldHeightMap.GetBoundsInterface();
		g_WorldHeightMapHR.m_numCols *= gv::WORLD_CELL_RESOLUTION;
		g_WorldHeightMapHR.m_numRows *= gv::WORLD_CELL_RESOLUTION;
	}
	else
	{
		g_WorldHeightMapHR.ResetBoundsToMainLevel(gv::WORLD_CELL_SIZE, gv::WORLD_CELL_RESOLUTION);
	}

	if (!bMasksOnly) // main heightmap
	{
		g_WorldHeightMap.Reset(
			downsample*resolution*g_WorldHeightMap.m_numCols,
			downsample*resolution*g_WorldHeightMap.m_numRows,
			downsample,
			g_WorldHeightMap.m_boundsMinX,
			g_WorldHeightMap.m_boundsMinY,
			g_WorldHeightMap.m_boundsMinZ,
			g_WorldHeightMap.m_boundsMaxX,
			g_WorldHeightMap.m_boundsMaxY,
			g_WorldHeightMap.m_boundsMaxZ,
			waterBoundaryPath,
			CREATE_HEIGHTMAP
		);
	}
	else
	{
		g_WorldHeightMap.Release();
	}

	if (1) // high-res heightmap
	{
		int flags = 0;

		if (bMasksOnly)
		{
			flags = CREATE_WORLDMASKONLY;
		}
		else
		{
			flags = CREATE_HEIGHTMAP | CREATE_WATERMASK WIN32PC_ONLY(| CREATE_ROADSMASK | CREATE_BOUNDMASK);
		}

		if (bCreateDensityMap)
		{
			flags |= CREATE_DENSITYMAP_ALL;// | CREATE_DENSITYMAP_EX;
		}

		g_WorldHeightMapHR.Reset(
			downsample*resolution*g_WorldHeightMapHR.m_numCols,
			downsample*resolution*g_WorldHeightMapHR.m_numRows,
			downsample,
			g_WorldHeightMapHR.m_boundsMinX,
			g_WorldHeightMapHR.m_boundsMinY,
			g_WorldHeightMapHR.m_boundsMinZ,
			g_WorldHeightMapHR.m_boundsMaxX,
			g_WorldHeightMapHR.m_boundsMaxY,
			g_WorldHeightMapHR.m_boundsMaxZ,
			NULL,
			(eCreateHeightmapFlags)flags
		);
	}
	else
	{
		g_WorldHeightMapHR.Release();
	}
}

void CGameWorldHeightMap::ResetWorldHeightMapTiles(int tileX, int tileY, int tileW, int tileH, int numSectorsPerTile, int resolution, int downsample, bool bMasksOnly, bool bCreateDensityMap, bool bCreateExperimentalDensityMaps, bool bCreateWaterOnly, const float* worldBoundsMinZOverride, const atMap<u16,int>* materialMap, int materialCount)
{
	const int sectorStartX = gv::NAVMESH_BOUNDS_MIN_X;
	const int sectorStartY = gv::NAVMESH_BOUNDS_MIN_Y;
	const int sectorSize   = gv::WORLD_CELL_SIZE;

	g_WorldHeightMapHR.m_boundsMinX = (float)(sectorStartX + sectorSize*gv::WORLD_CELLS_PER_TILE*(tileX));
	g_WorldHeightMapHR.m_boundsMinY = (float)(sectorStartY + sectorSize*gv::WORLD_CELLS_PER_TILE*(tileY));
	g_WorldHeightMapHR.m_boundsMinZ = (float)(gv::WORLD_BOUNDS_MIN_Z);
	g_WorldHeightMapHR.m_boundsMaxX = (float)(sectorStartX + sectorSize*gv::WORLD_CELLS_PER_TILE*(tileX + tileW));
	g_WorldHeightMapHR.m_boundsMaxY = (float)(sectorStartY + sectorSize*gv::WORLD_CELLS_PER_TILE*(tileY + tileH));
	g_WorldHeightMapHR.m_boundsMaxZ = (float)(gv::WORLD_BOUNDS_MAX_Z);

	g_WorldHeightMapHR.m_numCols = gv::WORLD_CELL_RESOLUTION*numSectorsPerTile*tileW;
	g_WorldHeightMapHR.m_numRows = gv::WORLD_CELL_RESOLUTION*numSectorsPerTile*tileH;

	if (worldBoundsMinZOverride)
	{
		g_WorldHeightMapHR.m_boundsMinZ = *worldBoundsMinZOverride;
	}

	int flags = 0;

	if (bMasksOnly)
	{
		flags = CREATE_WORLDMASKONLY;
	}
	else if (bCreateWaterOnly)
	{
		flags = CREATE_HEIGHTMAP | CREATE_WATERMASK | CREATE_WATERHEIGHT | CREATE_WATERMIRRORPORTALMASK;
	}
	else
	{
		flags = CREATE_HEIGHTMAP | CREATE_WATERMASK | CREATE_WATERHEIGHT | CREATE_WATERMIRRORPORTALMASK | CREATE_BOUNDMASK | CREATE_BOUNDTYPEMASK | CREATE_INTERIORMASK | CREATE_CABLEMASK | CREATE_CORONAMASK | CREATE_ANIMBLDGMASK | CREATE_MATERIALMASK | CREATE_STAIRSMASK | CREATE_SCRIPTMASK;

		if (bCreateDensityMap)
		{
			flags |= CREATE_DENSITYMAP_ALL | CREATE_BOUND_ID_MAP;
		}

		if (bCreateExperimentalDensityMaps)
		{
			flags |= CREATE_DENSITYMAP_EX;
		}
	}

	g_GlobalMaterialMap = (materialMap && materialCount > 0) ? materialMap : NULL;
	g_GlobalMaterialCount = materialCount;

	g_WorldHeightMapHR.Reset(
		downsample*resolution*g_WorldHeightMapHR.m_numCols,
		downsample*resolution*g_WorldHeightMapHR.m_numRows,
		downsample,
		g_WorldHeightMapHR.m_boundsMinX,
		g_WorldHeightMapHR.m_boundsMinY,
		g_WorldHeightMapHR.m_boundsMinZ,
		g_WorldHeightMapHR.m_boundsMaxX,
		g_WorldHeightMapHR.m_boundsMaxY,
		g_WorldHeightMapHR.m_boundsMaxZ,
		NULL,
		(eCreateHeightmapFlags)flags
	);
}

void CGameWorldHeightMap::AddGeometryBegin()
{
	g_WorldHeightMapML.RasteriseGeometryBegin();
	g_WorldHeightMapHR.RasteriseGeometryBegin();
}

void CGameWorldHeightMap::AddGeometryEnd()
{
	g_WorldHeightMapML.RasteriseGeometryEnd();
	g_WorldHeightMapHR.RasteriseGeometryEnd();
}

void CGameWorldHeightMap::AddTriangleToWorldHeightMap(
	Vec3V_In p0,
	Vec3V_In p1,
	Vec3V_In p2,
	u32 flags,
	int materialId, // index into special material list (low 8 bits) and proc tag (high 8 bits)
	u32 propGroupMask,
	u8 pedDensity,
	u8 moverBoundPolyDensity,
	u8 moverBoundPrimDensity,
	u8 weaponBoundPolyDensity,
	u8 weaponBoundPrimDensity,
	u32 boundID_rpf,
	u32 boundID,
	int boundPrimitiveType
	)
{
	if (boundPrimitiveType != PRIM_TYPE_POLYGON)
	{
		flags &= ~GWHM_FLAG_NON_BVH_PRIMITIVE;
	}

	g_WorldHeightMapML.RasteriseTriangle(p0, p1, p2, flags, NULL, materialId, propGroupMask, pedDensity, moverBoundPolyDensity, moverBoundPrimDensity, weaponBoundPolyDensity, weaponBoundPrimDensity, boundID_rpf, boundID, boundPrimitiveType);
	g_WorldHeightMapHR.RasteriseTriangle(p0, p1, p2, flags, NULL, materialId, propGroupMask, pedDensity, moverBoundPolyDensity, moverBoundPrimDensity, weaponBoundPolyDensity, weaponBoundPrimDensity, boundID_rpf, boundID, boundPrimitiveType);
}

#if __BANK

#if USE_DEBUG_ARCHETYPE_PROXIES
XPARAM(CheckDynArch);
#endif // USE_DEBUG_ARCHETYPE_PROXIES

#if __DEV
PARAM(CheckCables,"");
PARAM(CheckCablesAuto,"");
PARAM(CheckkDOPs,"");
#endif // __DEV

void CGameWorldHeightMap::PostCreateDrawableForEntity(CEntity* entity)
{
#if USE_DEBUG_ARCHETYPE_PROXIES
	if (PARAM_CheckDynArch.Get())
	{
		CDebugArchetype::CreateDebugArchetypeProxies();
		const CBaseModelInfo* pModelInfo = entity->GetBaseModelInfo();

		if (pModelInfo)
		{
			CDebugArchetype::CheckDebugArchetypeProxy(pModelInfo);
		}
	}
#endif // USE_DEBUG_ARCHETYPE_PROXIES

#if __DEV
	if (PARAM_CheckCables.Get())
	{
		CCustomShaderEffectCable::CheckDrawable(entity->GetDrawable(), entity->GetModelName());
	}

	if (PARAM_CheckkDOPs.Get()) // automatic construction of kDOPs for all building entities - useful for testing kDOP code
	{
		if (entity->GetIsTypeBuilding())
		{
			const fwkDOP18* kDOP = fwkDOP18::GetCachedFromDrawable(entity, entity->GetDrawable());

			if (kDOP)
			{
				Vec3V verts[96];
				kDOP->BuildVertices(verts);
				Vec3V unique[96];
				const int numUniquePoints = GetUniquePoints<Vec3V>(unique, verts, 96, ScalarV(V_FLT_SMALL_5));
				static int maxNumUniquePoints = 0;

				if (maxNumUniquePoints < numUniquePoints)
				{
					maxNumUniquePoints = numUniquePoints;
					Displayf("### max unique points in kDOP18 = %d", numUniquePoints);
				}
			}
		}
	}
#endif // __DEV
}

#endif // __BANK

void CGameWorldHeightMap::AddWaterGeometryBegin()
{
	// NOTE -- there is no water in prologue
	g_WorldHeightMapML.RasteriseWaterBegin();
	g_WorldHeightMapHR.RasteriseWaterBegin();
}

void CGameWorldHeightMap::AddWaterGeometryEnd()
{
	// NOTE -- there is no water in prologue
	g_WorldHeightMapML.RasteriseWaterEnd();
	g_WorldHeightMapHR.RasteriseWaterEnd();
}

void CGameWorldHeightMap::FinaliseWorldHeightMaps(bool bExpandWorldBounds)
{
	g_WorldHeightMapML.Finalise(bExpandWorldBounds);
	g_WorldHeightMapHR.Finalise(bExpandWorldBounds);
}

void CGameWorldHeightMap::SaveWorldHeightMaps(bool bAutoImageRange, bool bIsUnderwater)
{
	if (bIsUnderwater)
	{
		g_WorldHeightMapHR.ExportDDS(HEIGHTMAP_TOOL_DEBUG_OUTPUT_DIR"/heightmap_underwater", 1, 1, bAutoImageRange);
	}
	else
	{
		const bool bSaveWaterMask = true; // takes up a tiny amount of disk space, but we won't load it unless we need it

		g_WorldHeightMapML.ExportDDS(HEIGHTMAP_TOOL_DEBUG_OUTPUT_DIR"/heightmap_level", 1, 1);
		g_WorldHeightMapHR.ExportDDS(HEIGHTMAP_TOOL_DEBUG_OUTPUT_DIR"/heightmap_highres", 1, 1, bAutoImageRange);

		g_WorldHeightMapML.ConvertToRLE();

		g_WorldHeightMapML.Save(HEIGHTMAP_TOOL_DEBUG_OUTPUT_DIR"/heightmap_level.dat", bSaveWaterMask);

#if __WIN32PC
		if (g_WorldHeightMapHR.m_densityMap)
		{
			const bool bUseLogDensity = true;

			const int    densityMapNumCells = g_WorldHeightMapHR.m_numRows*g_WorldHeightMapHR.m_numCols;
			const float* densityMap         = g_WorldHeightMapHR.m_densityMap;
			const u8*    worldMask          = g_WorldHeightMapHR.m_worldMask;

			/*class CompareFloats { public: static int func(const void* pVoidA, const void* pVoidB)
			{
				const float* pA = (const float*)pVoidA;
				const float* pB = (const float*)pVoidB;
				if ((*pA) > (*pB)) { return +1; }
				if ((*pA) < (*pB)) { return -1; }
				return 0;
			}};
			qsort(densityMap, densityMapNumCells, sizeof(float), CompareFloats::func);*/

			float densityMin = +FLT_MAX;
			float densityMax = -FLT_MAX;

			for (int i = 0; i < densityMapNumCells; i++)
			{
				if (worldMask[i/8] & BIT(i%8))
				{
					densityMin = Min<float>(densityMap[i], densityMin);
					densityMax = Max<float>(densityMap[i], densityMax);
				}
			}

			if (bUseLogDensity)
			{
				densityMin = logf(densityMin);
				densityMax = logf(densityMax);
			}

			const int numBuckets = 2048;
			int* histogram = rage_new int[numBuckets];

			sysMemSet(histogram, 0, numBuckets*sizeof(int));

			for (int i = 0; i < densityMapNumCells; i++)
			{
				if (worldMask[i/8] & BIT(i%8))
				{
					float density = densityMap[i];

					if (bUseLogDensity)
					{
						density = logf(density);
					}

					const float f = (density - densityMin)/(densityMax - densityMin); // [0..1]
					const int bucket = Clamp<int>((int)floorf(f*(float)numBuckets), 0, numBuckets - 1);

					histogram[bucket]++;
				}
			}

			fiStream* fp = fiStream::Create(HEIGHTMAP_TOOL_DEBUG_OUTPUT_DIR"/heightmap_density.array");

			if (fp)
			{
				fwrite(&densityMin, sizeof(float), 1, fp);
				fwrite(&densityMax, sizeof(float), 1, fp);
				fwrite(&numBuckets, sizeof(int), 1, fp);
				fwrite(histogram, sizeof(int), numBuckets, fp);
				fp->Close();
			}

			delete[] histogram;
		}
#endif // __WIN32PC
	}
}

void CGameWorldHeightMap::SaveWorldHeightMapTile(char* dir, int tileX, int tileY, int numSectorsPerTile, int* _numEmptyWorldCells)
{
	int numEmptyWorldCells = 0;

	if (_numEmptyWorldCells && g_WorldHeightMapHR.m_worldMask)
	{
		for (int j = 0; j < g_WorldHeightMapHR.m_numRows; j++)
		{
			for (int i = 0; i < g_WorldHeightMapHR.m_numCols; i++)
			{
				const int index = i + j*g_WorldHeightMapHR.m_numCols;

				if ((g_WorldHeightMapHR.m_worldMask[index/8] & BIT(index%8)) == 0)
				{
					numEmptyWorldCells++;
				}
			}
		}
	}

	const int sectorX = numSectorsPerTile*tileX;
	const int sectorY = numSectorsPerTile*tileY;

	g_WorldHeightMapHR.ExportDDS(atVarString(HEIGHTMAP_TOOL_DEBUG_OUTPUT_DIR"/%s/heightmap_tile_%s", dir, gv::GetTileNameFromCoords(sectorX, sectorY)).c_str(), 1, 1, false);

	if (_numEmptyWorldCells)
	{
		*_numEmptyWorldCells = numEmptyWorldCells;
	}
}

#if __WIN32PC
int   CGameWorldHeightMap::GetHRNumCols()             { return g_WorldHeightMapHR.m_numCols; }
int   CGameWorldHeightMap::GetHRNumRows()             { return g_WorldHeightMapHR.m_numRows; }
float CGameWorldHeightMap::GetHROneOverCellSizeX()    { return g_WorldHeightMapHR.m_oneOverCellSizeX; }
float CGameWorldHeightMap::GetHROneOverCellSizeY()    { return g_WorldHeightMapHR.m_oneOverCellSizeY; }
u16*  CGameWorldHeightMap::GetHRHeightMin()           { return g_WorldHeightMapHR.m_dataRLE ? NULL : g_WorldHeightMapHR.m_dataMin; }
u16*  CGameWorldHeightMap::GetHRHeightMax()           { return g_WorldHeightMapHR.m_dataRLE ? NULL : g_WorldHeightMapHR.m_dataMax; }
u8*   CGameWorldHeightMap::GetHRWorldMask()           { return g_WorldHeightMapHR.m_worldMask; }
float CGameWorldHeightMap::GetHRCellHeight(u16 cellZ) { return UnquantiseCell<u16>(cellZ, g_WorldHeightMapHR.m_boundsMinZ, g_WorldHeightMapHR.m_boundsMaxZ); }
#endif // __WIN32PC

__COMMENT(static) const char** CGameWorldHeightMap::GetMaterialMaskList()
{
	// see BS#1156382
	static const char* materialMaskList[] =
	{
		"BRICK", // added for rdr3
		"BUSHES",
		"CLAY_HARD",
		"CLAY_SOFT",
		"DIRT_TRACK",
		"GRASS",
		"GRASS_LONG",
		"GRASS_MOWN",
		"GRASS_SHORT",
		"GRAVEL_LARGE",
		"GRAVEL_SMALL",
		"HAY",
		"LEAVES",
		"MUD_DEEP",
		"MUD_HARD",
		"MUD_POTHOLE",
		"MUD_SOFT",
		"MUD_UNDERWATER",
		"MARSH",
		"MARSH_DEEP",
		"ROCK",
		"ROCK_MOSSY",
		"SAND_COMPACT",
		"SAND_DRY_DEEP",
		"SAND_LOOSE",
		"SAND_TRACK",
		"SAND_UNDERWATER",
		"SAND_WET",
		"SAND_WET_DEEP",
		"SNOW_COMPACT", // added for rdr3
		"SOIL",
		"STONE",
		// =========
		"TARMAC",
		"TREE_BARK",
		"TWIGS",
		"WOODCHIPS",
		"unused_36",
		"unused_37",
		"unused_38",
		"unused_39",
		"unused_40",
		"unused_41",
		"unused_42",
		"unused_43",
		"unused_44",
		"unused_45",
		"unused_46",
		"unused_47",
		"unused_48",
		"unused_49",
		"unused_50",
		"unused_51",
		"unused_52",
		"unused_53",
		"unused_54",
		"unused_55",
		"unused_56",
		"unused_57",
		"unused_58",
		"unused_59",
		"unused_60",
		"unused_61",
		"OTHER",
		"ALL", // must be at index 0x003f
	};
	CompileTimeAssert(NELEM(materialMaskList) == 64); // we're going to store these masks in a 64-bit image

	return materialMaskList;
}

__COMMENT(static) int CGameWorldHeightMap::GetMaterialMaskIndex(phMaterialMgr::Id materialId)
{
	if (materialId != phMaterialMgr::MATERIAL_NOT_FOUND &&
		materialId != phMaterialMgr::DEFAULT_MATERIAL_ID)
	{
		char materialName[64] = "";
		PGTAMATERIALMGR->GetMaterialName(materialId, materialName, sizeof(materialName));
		strrchr(materialName, '_')[0] = '\0';
		const char** maskList = GetMaterialMaskList();

		for (int i = 0; i < 64; i++)
		{
			if (stricmp(maskList[i], materialName) == 0)
			{
				return i;
			}
		}

		return 62; // OTHER
	}

	return -1;
}

__COMMENT(static) const char** CGameWorldHeightMap::GetProceduralList()
{
	static char** s_proceduralList = NULL;
	static bool   s_proceduralListLoaded = false;
	static int    s_proceduralListCount = 0;

	if (!s_proceduralListLoaded)
	{
		fiStream* proceduralMetaFile = fiStream::Open("common:/data/materials/procedural.meta");

		if (proceduralMetaFile)
		{
			atArray<atString> names;
			bool bParsingProcNames = false;
			char line[1024] = "";

			while (fgetline(line, sizeof(line), proceduralMetaFile))
			{
				if (strstr(line, "<procTagTable>"))
				{
					bParsingProcNames = true;
				}
				else if (strstr(line, "</procTagTable>"))
				{
					break;
				}
				else if (bParsingProcNames)
				{
					char* s = strstr(line, "<name>");

					if (s)
					{
						char* name = s + strlen("<name>");

						s = strstr(name, "</name>");

						if (s)
						{
							*s = '\0';
							names.PushAndGrow(atString(name));
						}
					}
				}
			}

			proceduralMetaFile->Close();

			if (names.GetCount() > 0)
			{
				s_proceduralList = rage_new char*[names.GetCount() + 1];

				for (int i = 0; i < names.GetCount(); i++)
				{
					s_proceduralList[i] = rage_new char[names[i].length() + 1];
					strcpy(s_proceduralList[i], names[i].c_str());
				}

				s_proceduralList[names.GetCount()] = NULL;
				s_proceduralListCount = names.GetCount();
			}
		}

		s_proceduralListLoaded = true;
	}

	return const_cast<const char**>(s_proceduralList);
}

#endif // HEIGHTMAP_TOOL

// ================================================================================================

#if __BANK && __PS3

class CWaterBoundary
{
private:
	class CWaterBoundaryCounter
	{
	public:
		CWaterBoundaryCounter()
		{
			Reset();
		}

		void Reset()
		{
			m_edges = 0;
			m_geometries = 0;
			m_triangles = 0;
		}

		int m_edges;      // number of edges which contribute to water boundary
		int m_geometries; // number of geometries processed
		int m_triangles;  // number of triangles (total) for all geometries processed
	};

	bool m_bIsEnabled;
	atMap<u32,bool> m_entityInstances;
	fiStream* m_eps_water;
	fiStream* m_eps_waterLOD;
	fiStream* m_eps_river;
	fiStream* m_eps_riverLOD;
	CWaterBoundaryCounter m_counters_water;
	CWaterBoundaryCounter m_counters_waterLOD;
	CWaterBoundaryCounter m_counters_river;
	CWaterBoundaryCounter m_counters_riverLOD;

public:
	CWaterBoundary()
	{
		m_bIsEnabled   = false;
		m_eps_water    = NULL;
		m_eps_waterLOD = NULL;
		m_eps_river    = NULL;
		m_eps_riverLOD = NULL;
	}

	void AddWidgets(bkBank* pBank)
	{
		pBank->AddToggle("Enabled", &m_bIsEnabled);

		pBank->AddSlider("Edge count (water)"    , &m_counters_water.m_edges        , 0, 9999999, 0);
		pBank->AddSlider("- geometries"          , &m_counters_water.m_geometries   , 0, 999999, 0);
		pBank->AddSlider("- triangles"           , &m_counters_water.m_triangles    , 0, 99999999, 0);
		pBank->AddSlider("Edge count (water LOD)", &m_counters_waterLOD.m_edges     , 0, 9999999, 0);
		pBank->AddSlider("- geometries"          , &m_counters_waterLOD.m_geometries, 0, 999999, 0);
		pBank->AddSlider("- triangles"           , &m_counters_waterLOD.m_triangles , 0, 99999999, 0);
		pBank->AddSlider("Edge count (river)"    , &m_counters_river.m_edges        , 0, 9999999, 0);
		pBank->AddSlider("- geometries"          , &m_counters_river.m_geometries   , 0, 999999, 0);
		pBank->AddSlider("- triangles"           , &m_counters_river.m_triangles    , 0, 99999999, 0);
		pBank->AddSlider("Edge count (river LOD)", &m_counters_riverLOD.m_edges     , 0, 9999999, 0);
		pBank->AddSlider("- geometries"          , &m_counters_riverLOD.m_geometries, 0, 999999, 0);
		pBank->AddSlider("- triangles"           , &m_counters_riverLOD.m_triangles , 0, 99999999, 0);

		extern void WaterBoundary_DumpToEPS();
		pBank->AddButton("Dump to EPS file", WaterBoundary_DumpToEPS);
	}

	bool GetIsEnabled() const
	{
		return m_bIsEnabled;
	}

	bool HasEntityBeenAdded(const CEntity* pEntity, bool bAddEntityInstance)
	{
		const Mat34V matrix = pEntity->GetTransform().GetMatrix();

		const Vec3V entityPosition   = matrix.GetCol3();
		const u32   entityModelIndex = pEntity->GetModelIndex();

		u32 entityInstanceHash;

		entityInstanceHash = atDataHash((const char*)&entityPosition, 3*sizeof(float));
		entityInstanceHash = atDataHash((const char*)&entityModelIndex, sizeof(u32), entityInstanceHash);

		if (m_entityInstances.Access(entityInstanceHash))
		{
			return true;
		}
		else if (bAddEntityInstance)
		{
			m_entityInstances[entityInstanceHash] = true;
		}

		return false;
	}

	void AddEntity(const CEntity* pEntity)
	{
		USE_DEBUG_MEMORY();

		if (m_eps_water == NULL)
		{
			DumpToEPS_Begin(m_eps_water   , "assets:/waterboundary_ocean.eps");
			DumpToEPS_Begin(m_eps_waterLOD, "assets:/waterboundary_oceanLOD.eps");
			DumpToEPS_Begin(m_eps_river   , "assets:/waterboundary_river.eps");
			DumpToEPS_Begin(m_eps_riverLOD, "assets:/waterboundary_riverLOD.eps");
		}

		const rmcDrawable* pDrawable = pEntity->GetDrawable();

		if (pDrawable)
		{
			if ((pEntity->GetBaseFlags() & fwEntity::HAS_WATER) == 0)
			{
				spdAABB bounds = pEntity->GetBoundBox();
#if USE_WATER_REGIONS
				extern ScalarV_Out GetWaterRegionMaxZ(const spdRect& bounds);
				const ScalarV maxZ = GetWaterRegionMaxZ(spdRect(bounds));
#else
				const ScalarV maxZ = ScalarV(V_ZERO);
#endif
				if (bounds.GetMin().GetZf() > maxZ.Getf() ||
					bounds.GetMax().GetZf() < 0.0f)
				{
					return; // non-water entity doesn't intersect the water plane
				}
			}

			if (!HasEntityBeenAdded(pEntity, true))
			{
				const rmcLodGroup& lodGroup = pDrawable->GetLodGroup();

				if (lodGroup.ContainsLod(LOD_HIGH))
				{
					const rmcLod& lod = lodGroup.GetLod(LOD_HIGH);

					for (int lodModelIndex = 0; lodModelIndex < lod.GetCount(); lodModelIndex++)
					{
						const grmModel* pModel = lod.GetModel(lodModelIndex);

						if (pModel)
						{
							const grmModel& model = *pModel;
							const Mat34V matrix = pEntity->GetTransform().GetMatrix();

							for (int geomIndex = 0; geomIndex < model.GetGeometryCount(); geomIndex++)
							{
								const int shaderIndex = model.GetShaderIndex(geomIndex);
								const grmShader& shader = pDrawable->GetShaderGroup().GetShader(shaderIndex);
								const bool bIsWaterCollision = (shader.GetDrawBucket() == CRenderer::RB_WATER) || (strstr(shader.GetName(), "water") != NULL); // "water" shader implies river
								const bool bIsDecalShader = (shader.GetDrawBucket() == CRenderer::RB_DECAL);

								if (bIsDecalShader)
								{
									continue;
								}

								grmGeometry& geom = model.GetGeometry(geomIndex);

								if (geom.GetType() == grmGeometry::GEOMETRYEDGE)
								{
									grmGeometryEdge *geomEdge = reinterpret_cast<grmGeometryEdge*>(&geom);

#if HACK_GTA4_MODELINFOIDX_ON_SPU && USE_EDGE
									CGta4DbgSpuInfoStruct gtaSpuInfoStruct;
									gtaSpuInfoStruct.gta4RenderPhaseID = 0x02; // called by Object
									gtaSpuInfoStruct.gta4ModelInfoIdx  = pEntity->GetModelIndex();
									gtaSpuInfoStruct.gta4ModelInfoType = pEntity->GetBaseModelInfo()->GetModelType();
#endif // HACK_GTA4_MODELINFOIDX_ON_SPU && USE_EDGE

									const int WATER_BOUNDARY_EXTRACT_MAX_INDICES  = 16*1024*3;
									const int WATER_BOUNDARY_EXTRACT_MAX_VERTICES = 16*1024;

									// check up front how many verts are in processed geometry and assert if too many
									int totalI = 0;
									int totalV = 0;

									for (int i = 0; i < geomEdge->GetEdgeGeomPpuConfigInfoCount(); i++)
									{
										totalI += geomEdge->GetEdgeGeomPpuConfigInfos()[i].spuConfigInfo.numIndexes;
										totalV += geomEdge->GetEdgeGeomPpuConfigInfos()[i].spuConfigInfo.numVertexes;
									}

									if (totalI > WATER_BOUNDARY_EXTRACT_MAX_INDICES)
									{
										Assertf(0, "%s: index buffer has more indices (%d) than system can handle (%d)", pEntity->GetModelName(), totalI, WATER_BOUNDARY_EXTRACT_MAX_INDICES);
										return;
									}

									if (totalV > WATER_BOUNDARY_EXTRACT_MAX_VERTICES)
									{
										Assertf(0, "%s: vertex buffer has more verts (%d) than system can handle (%d)", pEntity->GetModelName(), totalV, WATER_BOUNDARY_EXTRACT_MAX_VERTICES);
										return;
									}

									static Vec4V* extractVertStreams[CExtractGeomParams::obvIdxMax] ;
									static Vec4V* extractVerts = NULL;
									static u16*   extractIndices = NULL;

									if (extractVerts   == NULL) { extractVerts   = rage_aligned_new(16) Vec4V[WATER_BOUNDARY_EXTRACT_MAX_VERTICES*1]; } // pos-only
									if (extractIndices == NULL) { extractIndices = rage_aligned_new(16) u16[WATER_BOUNDARY_EXTRACT_MAX_INDICES]; }

									sysMemSet(&extractVertStreams[0], 0, sizeof(extractVertStreams));

									int numVerts = 0;

									const int numIndices = geomEdge->GetVertexAndIndex(
										(Vector4*)extractVerts,
										WATER_BOUNDARY_EXTRACT_MAX_VERTICES,
										(Vector4**)extractVertStreams,
										extractIndices,
										WATER_BOUNDARY_EXTRACT_MAX_INDICES,
										NULL,//BoneIndexesAndWeights,
										0,//sizeof(BoneIndexesAndWeights),
										NULL,//&BoneIndexOffset,
										NULL,//&BoneIndexStride,
										NULL,//&BoneOffset1,
										NULL,//&BoneOffset2,
										NULL,//&BoneOffsetPoint,
										(u32*)&numVerts,
#if HACK_GTA4_MODELINFOIDX_ON_SPU
										&gtaSpuInfoStruct,
#endif // HACK_GTA4_MODELINFOIDX_ON_SPU
										NULL,
										CExtractGeomParams::extractPos
									);

#if !USE_WATER_REGIONS
									const Vec4V waterPlane = Vec4V(V_Z_AXIS_WZERO);
#endif // !USE_WATER_REGIONS
									int numEdges = 0;

									fiStream* eps = NULL;
									int* edgeCounter = NULL;
									int* geometryCounter = NULL;
									int* triangleCounter = NULL;

									if (bIsWaterCollision)
									{
										eps             = pEntity->GetLodData().IsHighDetail() ? m_eps_river : m_eps_riverLOD;
										edgeCounter     = pEntity->GetLodData().IsHighDetail() ? &m_counters_river.m_edges      : &m_counters_riverLOD.m_edges;
										geometryCounter = pEntity->GetLodData().IsHighDetail() ? &m_counters_river.m_geometries : &m_counters_riverLOD.m_geometries;
										triangleCounter = pEntity->GetLodData().IsHighDetail() ? &m_counters_river.m_triangles  : &m_counters_riverLOD.m_triangles;
									}
									else
									{
										eps             = pEntity->GetLodData().IsHighDetail() ? m_eps_water : m_eps_waterLOD;
										edgeCounter     = pEntity->GetLodData().IsHighDetail() ? &m_counters_water.m_edges      : &m_counters_waterLOD.m_edges;
										geometryCounter = pEntity->GetLodData().IsHighDetail() ? &m_counters_water.m_geometries : &m_counters_waterLOD.m_geometries;
										triangleCounter = pEntity->GetLodData().IsHighDetail() ? &m_counters_water.m_triangles  : &m_counters_waterLOD.m_triangles;
									}

									for (int i = 0; i < numIndices; i += 3)
									{
										const float scale = 512.0f/(float)Min<int>(gv::WORLD_BOUNDS_MAX_X - gv::WORLD_BOUNDS_MIN_X, gv::WORLD_BOUNDS_MAX_Y - gv::WORLD_BOUNDS_MIN_Y);
										const float offset = 40.0f;

										if (bIsWaterCollision)
										{
											for (int side = 0; side < 3; side++)
											{
												const u16 indexA = extractIndices[i + (side + 0)];
												const u16 indexB = extractIndices[i + (side + 1)%3];
												bool bConnected = false;

												for (int i2 = 0; i2 < numIndices; i2 += 3)
												{
													if (i2 != i)
													{
														for (int side2 = 0; side2 < 3; side2++)
														{
															const u16 indexA2 = extractIndices[i2 + (side2 + 0)];
															const u16 indexB2 = extractIndices[i2 + (side2 + 1)%3];

															if (indexA2 == indexB &&
																indexB2 == indexA)
															{
																bConnected = true;
																break;
															}
														}
													}
												}

												if (!bConnected)
												{
													if (eps)
													{
														Vec3V edge[2];

														edge[0] = Transform(matrix, extractVerts[indexA].GetXYZ());
														edge[1] = Transform(matrix, extractVerts[indexB].GetXYZ());

														const float x0 = (edge[0].GetXf() - (float)gv::WORLD_BOUNDS_MIN_X)*scale + offset;
														const float y0 = (edge[0].GetYf() - (float)gv::WORLD_BOUNDS_MIN_Y)*scale + offset;
														const float x1 = (edge[1].GetXf() - (float)gv::WORLD_BOUNDS_MIN_X)*scale + offset;
														const float y1 = (edge[1].GetYf() - (float)gv::WORLD_BOUNDS_MIN_Y)*scale + offset;

														fprintf(eps, "%f %f moveto %f %f lineto ", x0, y0, x1, y1);
														numEdges++;
													}

													(*edgeCounter)++;
												}
											}
										}
										else
										{
											Vec3V edge[2];
											Vec3V tri[3];

											tri[0] = Transform(matrix, extractVerts[extractIndices[i + 0]].GetXYZ());
											tri[1] = Transform(matrix, extractVerts[extractIndices[i + 1]].GetXYZ());
											tri[2] = Transform(matrix, extractVerts[extractIndices[i + 2]].GetXYZ());
#if USE_WATER_REGIONS
											spdRect triBounds;

											triBounds.Invalidate();
											triBounds.GrowPoint(tri[0].GetXY());
											triBounds.GrowPoint(tri[1].GetXY());
											triBounds.GrowPoint(tri[2].GetXY());

											extern ScalarV_Out GetWaterRegionZ(const spdRect& bounds);
											const ScalarV waterZ = GetWaterRegionZ(triBounds);
											const Vec4V waterPlane = Vec4V(Vec3V(V_Z_AXIS_WZERO), -waterZ);
#endif // USE_WATER_REGIONS
											if (PolyClipEdge(edge, tri, NELEM(tri), waterPlane))
											{
												if (eps)
												{
													const float x0 = (edge[0].GetXf() - (float)gv::WORLD_BOUNDS_MIN_X)*scale + offset;
													const float y0 = (edge[0].GetYf() - (float)gv::WORLD_BOUNDS_MIN_Y)*scale + offset;
													const float x1 = (edge[1].GetXf() - (float)gv::WORLD_BOUNDS_MIN_X)*scale + offset;
													const float y1 = (edge[1].GetYf() - (float)gv::WORLD_BOUNDS_MIN_Y)*scale + offset;

													fprintf(eps, "%f %f moveto %f %f lineto ", x0, y0, x1, y1);
													numEdges++;
												}

												(*edgeCounter)++;
											}
										}

										(*triangleCounter++);
									}

									(*geometryCounter)++;

									if (eps && numEdges > 0)
									{
										fprintf(eps, "stroke\n");
										eps->Flush();
									}
								}
							}
						}
					}
				}
			}
		}
	}

	static void DumpToEPS_Begin(fiStream*& eps, const char* path)
	{
		if (eps == NULL)
		{
			eps = fiStream::Create(path);

			if (AssertVerify(eps))
			{
				fprintf(eps, "%%!PS-Adobe EPSF-3.0\n");
				fprintf(eps, "%%%%HiResBoundingBox: 0 0 700 700\n");
				fprintf(eps, "%%%%BoundingBox: 0 0 700 700\n");
				fprintf(eps, "\n");
				fprintf(eps, "/Times-Roman findfont\n");
				fprintf(eps, "12 scalefont\n");
				fprintf(eps, "setfont\n");
				fprintf(eps, "25 12 moveto\n");
				fprintf(eps, "0 0 0 setrgbcolor\n");
				fprintf(eps, "(water boundary edges) show\n");
				fprintf(eps, "\n");
				fprintf(eps, "0.0 0.0 0.0 setrgbcolor\n");
				fprintf(eps, "0.2 setlinewidth\n");
				fprintf(eps, "\n");
			}
		}
	}

	void DumpToEPS()
	{
		if (m_eps_water)
		{
			fprintf(m_eps_water, "\n");
			fprintf(m_eps_water, "showpage\n");

			m_eps_water->Close();
			m_eps_water = NULL;
		}

		if (m_eps_waterLOD)
		{
			fprintf(m_eps_waterLOD, "\n");
			fprintf(m_eps_waterLOD, "showpage\n");

			m_eps_waterLOD->Close();
			m_eps_waterLOD = NULL;
		}

		if (m_eps_river)
		{
			fprintf(m_eps_river, "\n");
			fprintf(m_eps_river, "showpage\n");

			m_eps_river->Close();
			m_eps_river = NULL;
		}

		if (m_eps_riverLOD)
		{
			fprintf(m_eps_riverLOD, "\n");
			fprintf(m_eps_riverLOD, "showpage\n");

			m_eps_riverLOD->Close();
			m_eps_riverLOD = NULL;
		}

		m_entityInstances.Reset();

		m_counters_water.Reset();
		m_counters_waterLOD.Reset();
		m_counters_river.Reset();
		m_counters_riverLOD.Reset();

		m_bIsEnabled = false;
	}
};

static CWaterBoundary g_waterBoundary;

bool WaterBoundary_IsEnabled()
{
	return g_waterBoundary.GetIsEnabled();
}

void WaterBoundary_AddWidgets(bkBank* pBank)
{
	g_waterBoundary.AddWidgets(pBank);
}

bool WaterBoundary_HasEntityBeenAdded(const CEntity* pEntity)
{
	return g_waterBoundary.HasEntityBeenAdded(pEntity, false);
}

void WaterBoundary_AddEntity(const CEntity* pEntity, float)
{
	if (pEntity->GetIsTypeBuilding() ||
		pEntity->GetIsTypeObject() ||
		pEntity->GetIsTypeDummyObject())
	{
		g_waterBoundary.AddEntity(pEntity);
	}
}

void WaterBoundary_DumpToEPS()
{
	g_waterBoundary.DumpToEPS();
}

#endif // __BANK && __PS3
