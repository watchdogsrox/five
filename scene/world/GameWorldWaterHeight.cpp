// ====================================
// scene/world/GameWorldWaterHeight.cpp
// (c) 2012 RockstarNorth
// ====================================

#include "file/stream.h"
#include "grcore/debugdraw.h"
#include "grcore/image.h"
#include "grcore/viewport.h"
#include "system/endian.h"
#include "vectormath/classes.h"

#include "fwdrawlist/drawlist.h"
#include "fwmaths/vectorutil.h"
#include "fwsys/gameskeleton.h"
#include "fwutil/xmacro.h"

#include "camera/viewports/ViewportManager.h"
#include "debug/DebugGeometryUtil.h"
#include "renderer/Debug/Raster.h"
#include "renderer/Water.h"
#include "scene/DataFileMgr.h"
#include "scene/world/GameWorldWaterHeight.h"
#include "system/memops.h"

SCENE_OPTIMISATIONS()

// ================================================================================================

class CWaterMapHeader
{
public:
	void ByteSwap()
	{
		sysEndian::SwapMe(m_dataSizeInBytes);
		sysEndian::SwapMe(m_offsetX);
		sysEndian::SwapMe(m_offsetY);
		sysEndian::SwapMe(m_tileSizeX);
		sysEndian::SwapMe(m_tileSizeY);
		sysEndian::SwapMe(m_numTileCols);
		sysEndian::SwapMe(m_numTileRows);
		sysEndian::SwapMe(m_numTiles);
		sysEndian::SwapMe(m_numRefIDs);
		sysEndian::SwapMe(m_numRiverPoints);
		sysEndian::SwapMe(m_numRivers);
		sysEndian::SwapMe(m_numLakeBoxes);
		sysEndian::SwapMe(m_numLakes);
		sysEndian::SwapMe(m_numPools);
	}

	u32   m_dataSizeInBytes;
	float m_offsetX;
	float m_offsetY;
	float m_tileSizeX;
	float m_tileSizeY;
	u16   m_numTileCols;
	u16   m_numTileRows;
	u32   m_numTiles;
	u32   m_numRefIDs;
	u16   m_numRiverPoints;
	u16   m_numRivers;
	u16   m_numLakeBoxes;
	u16   m_numLakes;
	u16   m_numPools;

	u8    m_sizeofWaterMapHeader;
	u8    m_sizeofWaterMapTileRow;
	u8    m_sizeofWaterMapTile;
	u8    m_sizeofWaterMapRefID;
	u8    m_sizeofWaterMapRiverPoint;
	u8    m_sizeofWaterMapRiver;
	u8    m_sizeofWaterMapLakeBox;
	u8    m_sizeofWaterMapLake;
	u8    m_sizeofWaterMapPool;
};

class CWaterMapTileRow
{
public:
	static bool IsAligned() { return false; }

	void ByteSwap()
	{
		sysEndian::SwapMe(m_first);
		sysEndian::SwapMe(m_count);
		sysEndian::SwapMe(m_start);
	}

	u8  m_first; // column index of first tile
	u8  m_count; // number of tiles
	u16 m_start; // index of first tile in m_tiles
};

class CWaterMapTile
{
public:
	static bool IsAligned() { return false; }

	void ByteSwap()
	{
		sysEndian::SwapMe(m_start);
	}

	u16 m_start; // index of first refID in m_refIDs
};

class CWaterMapRefID
{
public:
	CWaterMapRefID(u16 v) : m_typeIndexSegment(v) {}

	static bool IsAligned() { return false; }

	void ByteSwap()
	{
		sysEndian::SwapMe(m_typeIndexSegment);
	}

	enum Type
	{
		TYPE_OCEAN = 0,
		TYPE_RIVER,
		TYPE_LAKE,
		TYPE_POOL,
	};

	inline bool IsValid() const { return m_typeIndexSegment != 0; }
	inline bool IsLastRefID() const { return (m_typeIndexSegment & 0x8000) != 0; }
	inline Type GetType() const { return (Type)((m_typeIndexSegment >> 13) & 3); }

	inline int GetIndex() const
	{
		if (GetType() == TYPE_POOL)
		{
			return (int)(m_typeIndexSegment & 8191);
		}
		else
		{
			return (int)((m_typeIndexSegment >> 7) & 63);
		}
	}

	inline int GetSegment() const
	{
		if (GetType() == TYPE_POOL)
		{
			return 0;
		}
		else
		{
			return (int)(m_typeIndexSegment & 127);
		}
	}

	u16 m_typeIndexSegment;
};

class CWaterMapRegionBase
{
public:
	static bool IsAligned() { return true; }

	void ByteSwap()
	{
		sysEndian::SwapMe(m_centreAndHeight);
		sysEndian::SwapMe(m_extentAndWeight);
	}

	inline ScalarV_Out GetHeight() const
	{
		return m_centreAndHeight.GetZ();
	}

	inline ScalarV_Out GetDistanceSquared(Vec3V_In camPos) const
	{
		const Vec2V v = Abs((camPos - m_centreAndHeight).GetXY());
		const ScalarV d = MagSquared(Max(Vec2V(V_ZERO), v - m_extentAndWeight.GetXY()));

		return d;
	}

	inline ScalarV_Out GetWeight(Vec3V_In camPos) const
	{
		return Min(ScalarV(V_ONE), GetDistanceSquared(camPos)*m_extentAndWeight.GetZ());
	}

#if __BANK
	void DebugDraw(const Color32& colour = Color32(255,0,0,255)) const
	{
		const Vec3V bmin = m_centreAndHeight - Vec3V(m_extentAndWeight.GetXY(), ScalarV(V_ZERO));
		const Vec3V bmax = m_centreAndHeight + Vec3V(m_extentAndWeight.GetXY(), ScalarV(V_ONE));

		grcDebugDraw::BoxAxisAligned(bmin, bmax, colour, false);
	}
#endif // __BANK

	Vec3V m_centreAndHeight;
	Vec3V m_extentAndWeight;
};

class CWaterMapRiverPoint
{
public:
	static bool IsAligned() { return true; }

	void ByteSwap()
	{
		sysEndian::SwapMe(m_point);
	}

	Vec4V m_point;
};

// TODO -- vectorise
static float DistanceToSegmentSquared(float x, float y, float x0, float y0, float x1, float y1, float* t = NULL)
{
	float nx = x1 - x0;
	float ny = y1 - y0;
	float q = nx*nx + ny*ny;
	float dx = 0.0f;
	float dy = 0.0f;
	float dz = 0.0f;

	if (q < 0.00001f)
	{
		dx = x - (x1 + x0)*0.5f;
		dy = y - (y1 + y0)*0.5f;

		if (t) { *t = 0.5f; }
	}
	else
	{
		const float len = sqrtf(q);

		dz = ((x - x0)*nx + (y - y0)*ny)/len; // projected distance along segment

		if (dz >= len)
		{
			dx = x - x1;
			dy = y - y1;
			dz = 0.0f;

			if (t) { *t = 1.0f; }
		}
		else
		{
			dx = x - x0;
			dy = y - y0;

			if (dz <= 0.0f)
			{
				dz = 0.0f;

				if (t) { *t = 0.0f; }
			}
			else
			{
				if (t) { *t = dz/len; }
			}
		}
	}

	return Max<float>(0.0f, dx*dx + dy*dy - dz*dz);
}

class CWaterMapRiver : public CWaterMapRegionBase
{
public:
	void ByteSwap()
	{
		CWaterMapRegionBase::ByteSwap();

		sysEndian::SwapMe(m_count);
		sysEndian::SwapMe(m_pad);
		sysEndian::SwapMe(m_start);
	}

	inline ScalarV_Out GetHeight(Vec3V_In camPos, int segment, const CWaterMapRiverPoint* riverPoints) const
	{
		const Vec3V point0 = riverPoints[(int)m_start + segment + 0].m_point.GetXYZ();
		const Vec3V point1 = riverPoints[(int)m_start + segment + 1].m_point.GetXYZ();

		float t = 0.0f;
		DistanceToSegmentSquared(
			VEC2V_ARGS(camPos),
			VEC2V_ARGS(point0),
			VEC2V_ARGS(point1),
			&t
		);

		return (point0 + (point1 - point0)*ScalarV(t)).GetZ();
	}

	inline ScalarV_Out GetDistanceSquared(Vec3V_In camPos, int segment, const CWaterMapRiverPoint* riverPoints) const
	{
		const Vec3V point0 = riverPoints[(int)m_start + segment + 0].m_point.GetXYZ();
		const Vec3V point1 = riverPoints[(int)m_start + segment + 1].m_point.GetXYZ();

		return ScalarV(DistanceToSegmentSquared(VEC2V_ARGS(camPos), VEC2V_ARGS(point0), VEC2V_ARGS(point1)));
	}

	inline ScalarV_Out GetWeight(Vec3V_In camPos, int segment, const CWaterMapRiverPoint* riverPoints) const
	{
		return Min(ScalarV(V_ONE), GetDistanceSquared(camPos, segment, riverPoints)*m_extentAndWeight.GetZ());
	}

#if __BANK
	void DebugDraw(const CWaterMapRiverPoint* riverPoints, u8 alpha, bool) const
	{
		for (int i = 0; i < (int)m_count; i++)
		{
			const Vec3V   point = riverPoints[(int)m_start + i].m_point.GetXYZ();
			const ScalarV width = riverPoints[(int)m_start + i].m_point.GetW();

			const int i0 = Max<int>(i - 1, 0);
			const int i1 = Min<int>(i + 1, (int)m_count - 1);

			const Vec3V v = riverPoints[(int)m_start + i1].m_point.GetXYZ() - riverPoints[(int)m_start + i0].m_point.GetXYZ();
			const Vec3V n = Normalize(CrossZAxis(v));

			const Vec3V p0 = point - n*width*ScalarV(V_HALF);
			const Vec3V p1 = point + n*width*ScalarV(V_HALF);
			const Vec3V p2 = p1 + width*Vec3V(V_Z_AXIS_WZERO);
			const Vec3V p3 = p0 + width*Vec3V(V_Z_AXIS_WZERO);

			grcDebugDraw::Quad(p0, p1, p2, p3, Color32(0,0,255,alpha), true, false);
		}
	}
#endif // __BANK

	u8  m_count; // number of points in this river
	u8  m_pad;
	u16 m_start; // index of first point in m_riverPoints
};

class CWaterMapLakeBox
{
public:
	static bool IsAligned() { return true; }

	void ByteSwap()
	{
		sysEndian::SwapMe(m_box);
	}

	inline Vec2V GetCentre() const { return m_box.GetXY(); }
	inline Vec2V GetExtent() const { return m_box.GetZW(); }

	Vec4V m_box; // centre xy, extent xy
};

class CWaterMapLake : public CWaterMapRegionBase
{
public:
	void ByteSwap()
	{
		CWaterMapRegionBase::ByteSwap();

		sysEndian::SwapMe(m_count);
		sysEndian::SwapMe(m_pad);
		sysEndian::SwapMe(m_start);
	}

	inline ScalarV_Out GetDistanceSquared(Vec3V_In camPos, int segment, const CWaterMapLakeBox* lakeBoxes) const
	{
		const CWaterMapLakeBox& box = lakeBoxes[(int)m_start + segment];
		const Vec2V v = Abs(camPos.GetXY() - box.GetCentre());
		const ScalarV d = MagSquared(Max(Vec2V(V_ZERO), v - box.GetExtent()));
		
		return d;
	}

	inline ScalarV_Out GetWeight(Vec3V_In camPos, int segment, const CWaterMapLakeBox* lakeBoxes) const
	{
		return Min(ScalarV(V_ONE), GetDistanceSquared(camPos, segment, lakeBoxes)*m_extentAndWeight.GetZ());
	}

#if __BANK
	void DebugDraw(const CWaterMapLakeBox* lakeBoxes, u8 alpha, bool) const
	{
		for (int i = 0; i < (int)m_count; i++)
		{
			const CWaterMapLakeBox& box = lakeBoxes[(int)m_start + i];

			const Vec3V bmin = Vec3V(box.GetCentre() - box.GetExtent(), m_centreAndHeight.GetZ());
			const Vec3V bmax = Vec3V(box.GetCentre() + box.GetExtent(), m_centreAndHeight.GetZ() + ScalarV(V_ONE));

			grcDebugDraw::BoxAxisAligned(bmin, bmax, Color32(255,0,255,alpha), false);
		}

		CWaterMapRegionBase::DebugDraw(Color32(255,255,0,alpha));
	}
#endif // __BANK

	u8  m_count; // number of boxes in this lake
	u8  m_pad;
	u16 m_start; // index of first box in m_lakeBoxes
};

class CWaterMapPool : public CWaterMapRegionBase
{
public:
#if __BANK
	void DebugDraw(u8 alpha, bool) const
	{
		CWaterMapRegionBase::DebugDraw(Color32(0,255,0,alpha));
	}
#endif // __BANK
};

#if __BANK

class CWaterMapRegionBaseDebugInfo
{
public:
	static bool IsAligned() { return false; }

	void ByteSwap()
	{
	}

	u8 m_colour[4];
};

class CWaterMapRiverDebugInfo : public CWaterMapRegionBaseDebugInfo
{
public:
};

class CWaterMapLakeDebugInfo : public CWaterMapRegionBaseDebugInfo
{
public:
};

class CWaterMapPoolDebugInfo : public CWaterMapRegionBaseDebugInfo
{
public:
};

#endif // __BANK

template <typename T> static T* CWaterMap_GetArrayPtr(u8* data, int& offset, int count, bool bNeedsByteSwap)
{
	if (T::IsAligned())
	{
		offset = (offset + 15)&~15;
	}
	else
	{
		offset = (offset + 3)&~3;
	}

	T* array = reinterpret_cast<T*>(data + offset);

	if (bNeedsByteSwap)
	{
		for (int i = 0; i < count; i++)
		{
			array[i].ByteSwap();
		}
	}

	offset += count*sizeof(T);
	return array;
}

#if __BANK
static bool g_waterMapSystemEnabled          = true;
static bool g_waterMapUpdateEnabled          = true;
static bool g_waterMapRiversEnabled          = true;
static bool g_waterMapLakesEnabled           = true;
static bool g_waterMapPoolsEnabled           = true;
static bool g_waterMapPoolViewportCulling    = true;
static bool g_waterMapDebugShowInfo          = false;
static bool g_waterMapDebugDrawCurrentRegion = false;
static bool g_waterMapDebugDrawAllRivers     = false;
static bool g_waterMapDebugDrawAllLakes      = false;
static bool g_waterMapDebugDrawAllPools      = false;
static int  g_waterMapDebugPoolIndex         = -1;
#endif // __BANK

// TODO -- this ocean transition hack doesn't really work too well, but we might need to use it if we don't find a better solution
// if we do use it, we'll need to change the ocean height to 30.0f when the camera is near the big lake
#define WATER_MAP_OCEAN_TRANSITION (1 && __DEV)

// here's another solution - count the number of ocean pixels divided by the total number of reflective water pixels
#define WATER_MAP_OCEAN_PIXEL_TEST (1)// && __DEV)

#if WATER_MAP_OCEAN_TRANSITION || WATER_MAP_OCEAN_PIXEL_TEST
static float g_waterMapOceanHeight = 0.0f; // height of ocean (TODO SHIP -- this needs to be the height of the "closest ocean body" to the camera, or something)
#endif // WATER_MAP_OCEAN_TRANSITION || WATER_MAP_OCEAN_PIXEL_TEST

#if WATER_MAP_OCEAN_TRANSITION
static bool  g_waterMapOceanTransitionEnabled   = false;
static float g_waterMapOceanTransitionHeightMin = 40.0f; // at this height above the water, start interpolating water height to the ocean height ..
static float g_waterMapOceanTransitionHeightMax = 100.0f;
#endif // WATER_MAP_OCEAN_TRANSITION

#if WATER_MAP_OCEAN_PIXEL_TEST
static bool  g_waterMapOceanPixelTestEnabled             = true;
static float g_waterMapOceanPixelTestMinThresh           = 0.0f; // minimum threshold for ocean pixel coverage to switch to ocean height
static float g_waterMapOceanPixelTestMinRatio            = 0.8f; // minimum ratio for ocean/river pixel coverage to switch to ocean height
static u32   g_waterMapOceanPixelTestOceanZeroMultiplier = 1;    // pixel count multiplier for ocean height 0
#endif // WATER_MAP_OCEAN_PIXEL_TEST

static int g_currentWaterTileIndices[2] = {-1, -1};

class CWaterMap
{
public:
	CWaterMap()
	{
		sysMemSet(this, 0, sizeof(*this));
	}

	void Load(const char* path)
	{
		fiStream* f = fiStream::Open(path, true);

		if (!Verifyf(f, "water height file \"%s\" not found", path))
		{
			return;
		}

		bool bNeedsByteSwap = false;

		u32 tag = 0;
		f->Read(&tag, sizeof(tag));

		if (tag != 'WMAP')
		{
			sysEndian::SwapMe(tag);

			if (tag != 'WMAP')
			{
				Assertf(0, "water height file \"%s\" is corrupted", path);
				return;
			}
			else
			{
				bNeedsByteSwap = true;
			}
		}

		const u32 expectedVersion = 100;
		u32 version = 0;
		f->Read(&version, sizeof(version));

		if (bNeedsByteSwap)
		{
			sysEndian::SwapMe(version);
		}

		if (version/100 != expectedVersion/100)
		{
			Assertf(0, "water height file \"%s\" version %d.%02d is incorrect, expected version %d", path, version/100, version%100, expectedVersion/100);
			return;
		}

		f->Read(&m_header, sizeof(m_header));

		if (bNeedsByteSwap)
		{
			m_header.ByteSwap();
		}

		Assert(m_header.m_sizeofWaterMapHeader     == sizeof(CWaterMapHeader    ));
		Assert(m_header.m_sizeofWaterMapTileRow    == sizeof(CWaterMapTileRow   ));
		Assert(m_header.m_sizeofWaterMapTile       == sizeof(CWaterMapTile      ));
		Assert(m_header.m_sizeofWaterMapRefID      == sizeof(CWaterMapRefID     ));
		Assert(m_header.m_sizeofWaterMapRiverPoint == sizeof(CWaterMapRiverPoint));
		Assert(m_header.m_sizeofWaterMapRiver      == sizeof(CWaterMapRiver     ));
		Assert(m_header.m_sizeofWaterMapLakeBox    == sizeof(CWaterMapLakeBox   ));
		Assert(m_header.m_sizeofWaterMapLake       == sizeof(CWaterMapLake      ));
		Assert(m_header.m_sizeofWaterMapPool       == sizeof(CWaterMapPool      ));

		m_data = rage_aligned_new(16) u8[m_header.m_dataSizeInBytes];

		f->Read(m_data, m_header.m_dataSizeInBytes);

		int offset = 0;

		m_tileRows    = CWaterMap_GetArrayPtr<CWaterMapTileRow   >(m_data, offset, (int)m_header.m_numTileRows   , bNeedsByteSwap);
		m_tiles       = CWaterMap_GetArrayPtr<CWaterMapTile      >(m_data, offset, (int)m_header.m_numTiles      , bNeedsByteSwap);
		m_refIDs      = CWaterMap_GetArrayPtr<CWaterMapRefID     >(m_data, offset, (int)m_header.m_numRefIDs     , bNeedsByteSwap);
		m_riverPoints = CWaterMap_GetArrayPtr<CWaterMapRiverPoint>(m_data, offset, (int)m_header.m_numRiverPoints, bNeedsByteSwap);
		m_rivers      = CWaterMap_GetArrayPtr<CWaterMapRiver     >(m_data, offset, (int)m_header.m_numRivers     , bNeedsByteSwap);
		m_lakeBoxes   = CWaterMap_GetArrayPtr<CWaterMapLakeBox   >(m_data, offset, (int)m_header.m_numLakeBoxes  , bNeedsByteSwap);
		m_lakes       = CWaterMap_GetArrayPtr<CWaterMapLake      >(m_data, offset, (int)m_header.m_numLakes      , bNeedsByteSwap);
		m_pools       = CWaterMap_GetArrayPtr<CWaterMapPool      >(m_data, offset, (int)m_header.m_numPools      , bNeedsByteSwap);

#if __BANK
		m_riverDebugInfo = rage_new CWaterMapRiverDebugInfo[m_header.m_numRivers];
		f->Read(m_riverDebugInfo, (int)m_header.m_numRivers*sizeof(CWaterMapRiverDebugInfo));

		m_lakeDebugInfo = rage_new CWaterMapLakeDebugInfo[m_header.m_numLakes];
		f->Read(m_lakeDebugInfo, (int)m_header.m_numLakes*sizeof(CWaterMapLakeDebugInfo));

		m_poolDebugInfo = rage_new CWaterMapPoolDebugInfo[m_header.m_numPools];
		f->Read(m_poolDebugInfo, (int)m_header.m_numPools*sizeof(CWaterMapPoolDebugInfo));

		if (bNeedsByteSwap)
		{
			for (int i = 0; i < (int)m_header.m_numRivers; i++)
			{
				m_riverDebugInfo[i].ByteSwap();
				Assert(m_riverDebugInfo[i].m_colour[3] == 255);
			}

			for (int i = 0; i < (int)m_header.m_numLakes; i++)
			{
				m_lakeDebugInfo[i].ByteSwap();
				Assert(m_lakeDebugInfo[i].m_colour[3] == 255);
			}

			for (int i = 0; i < (int)m_header.m_numPools; i++)
			{
				m_poolDebugInfo[i].ByteSwap();
				Assert(m_poolDebugInfo[i].m_colour[3] == 255);
			}
		}
#endif // __BANK

		f->Close();
	}

	float GetHeight(bool bGlobal, Vec3V_In camPos, const grcViewport* vp, Vec4V* out_closestPoolSphere, float* out_closestRiverDistance, float& out_weight BANK_PARAM(CWaterMapRefID& out_refID)) const
	{
#if __BANK
		bool bShowInfo = bGlobal && g_waterMapDebugShowInfo;

		if (bGlobal)
		{
#if RASTER
			if (CRasterInterface::GetControlRef().m_displayOpacity > 0.0f)
			{
				bShowInfo = true;
			}
#endif // RASTER

			if (g_waterMapDebugPoolIndex >= 0 &&
				g_waterMapDebugPoolIndex < (int)m_header.m_numPools)
			{
				const CWaterMapPool& pool = m_pools[g_waterMapDebugPoolIndex];

				const Vec2V v = Abs((camPos - pool.m_centreAndHeight).GetXY());
				const float d1 = MagSquared(Max(Vec2V(V_ZERO), v - pool.m_extentAndWeight.GetXY())).Getf();
				const float d2 = pool.m_extentAndWeight.GetZf();

				grcDebugDraw::AddDebugOutput("debug pool[%d] weight = Min(1, %f*%f)", g_waterMapDebugPoolIndex, d1, d2);
			}
		}
#endif // __BANK

		const int i = (int)floorf(+(camPos.GetXf() - m_header.m_offsetX)/m_header.m_tileSizeX);
		const int j = (int)floorf(-(camPos.GetYf() - m_header.m_offsetY)/m_header.m_tileSizeY); // flipped

		if (bGlobal)
		{
			g_currentWaterTileIndices[0] = i;
			g_currentWaterTileIndices[1] = j;
		}

		if (i >= 0 && i < (int)m_header.m_numTileCols &&
			j >= 0 && j < (int)m_header.m_numTileRows)
		{
			const CWaterMapTileRow& row = m_tileRows[j];
			const int i2 = i - row.m_first;

			if (i2 >= 0 && i2 < row.m_count)
			{
				const CWaterMapTile& tile = m_tiles[(int)row.m_start + i2];

				if (tile.m_start != 0xffff)
				{
					const CWaterMapPool* closestPool = NULL;
					const CWaterMapRiver* closestRiver = NULL;
					int closestRiverSegment = 0;

					ScalarV height(V_ZERO);
					ScalarV weight(V_ONE); // it's faster to track "1 - weight^2" than actual weight

					for (int k = 0; ; k++)
					{
						const CWaterMapRefID& refID = m_refIDs[(int)tile.m_start + k];

						switch (refID.GetType())
						{
						case CWaterMapRefID::TYPE_OCEAN:
							{
								break;
							}
						case CWaterMapRefID::TYPE_RIVER:
							{
								BANK_ONLY(if (g_waterMapRiversEnabled))
								{
									const int segment = refID.GetSegment();
									const CWaterMapRiver& river = m_rivers[refID.GetIndex()];
									const ScalarV w = river.GetWeight(camPos, segment, m_riverPoints);
									
									if (IsGreaterThanAll(weight, w))
									{
										weight = w;
										height = river.GetHeight(camPos, segment, m_riverPoints);
										closestPool = NULL;
										closestRiver = &river;
										closestRiverSegment = segment;
										BANK_ONLY(out_refID = refID);
									}

									BANK_ONLY(if (bShowInfo)
									{
										grcDebugDraw::AddDebugOutput("watermap: k=%d, height=%f, weight=%f, dist=%f (RIVER[%d] segment=%d)", k, river.GetHeight(camPos, segment, m_riverPoints).Getf(), w.Getf(), sqrtf(river.GetDistanceSquared(camPos, segment, m_riverPoints).Getf()), refID.GetIndex(), segment);
									})
								}
								break;
							}
						case CWaterMapRefID::TYPE_LAKE:
							{
								BANK_ONLY(if (g_waterMapLakesEnabled))
								{
									const int segment = refID.GetSegment();
									const CWaterMapLake& lake = m_lakes[refID.GetIndex()];
									const ScalarV w = lake.GetWeight(camPos, segment, m_lakeBoxes);

									if (IsGreaterThanAll(weight, w))
									{
										weight = w;
										height = lake.GetHeight();
										closestPool = NULL;
										closestRiver = NULL;
										BANK_ONLY(out_refID = refID);
									}

									BANK_ONLY(if (bShowInfo)
									{
										grcDebugDraw::AddDebugOutput("watermap: k=%d, height=%f, weight=%f (LAKE[%d] segment=%d)", k, lake.GetHeight().Getf(), w.Getf(), refID.GetIndex(), segment);
									})
								}
								break;
							}
						case CWaterMapRefID::TYPE_POOL:
							{
								BANK_ONLY(if (g_waterMapPoolsEnabled))
								{
									const CWaterMapPool& pool = m_pools[refID.GetIndex()];
									const ScalarV w = pool.GetWeight(camPos);

									if (vp BANK_ONLY(&& g_waterMapPoolViewportCulling))
									{
										// BS#1569428, prevent nearby pools from affecting water height if they are above the camera
										if (IsGreaterThanAll(pool.m_centreAndHeight.GetZ(), camPos.GetZ() + ScalarV(V_FLT_SMALL_1)) && Likely(!Water::IsCameraUnderwater()))
										{
											break;
										}

										const Vec3V bmin = pool.m_centreAndHeight - Vec3V(pool.m_extentAndWeight.GetXY(), ScalarV(V_ZERO));
										const Vec3V bmax = pool.m_centreAndHeight + Vec3V(pool.m_extentAndWeight.GetXY(), ScalarV(V_ONE));

										if (!vp->IsAABBVisible(bmin.GetIntrin128(), bmax.GetIntrin128(), vp->GetFrustumLRTB()))
										{
											break;
										}
									}

									if (IsGreaterThanAll(weight, w))
									{
										weight = w;
										height = pool.GetHeight();
										closestPool = &pool;
										closestRiver = NULL;
										BANK_ONLY(out_refID = refID);
									}

									BANK_ONLY(if (bShowInfo)
									{
										grcDebugDraw::AddDebugOutput("watermap: k=%d, height=%f, weight=%f (POOL[%d])", k, pool.GetHeight().Getf(), w.Getf(), refID.GetIndex());
									})
								}
								break;
							}
						}

						if (refID.IsLastRefID())
						{
							break;
						}
					}

					if (out_closestPoolSphere)
					{
						if (closestPool)
						{
							*out_closestPoolSphere = Vec4V(closestPool->m_centreAndHeight, MagFast(closestPool->m_extentAndWeight.GetXY()));
						}
						else
						{
							*out_closestPoolSphere = Vec4V(V_ZERO);
						}

						BANK_ONLY(if (bShowInfo)
						{
							grcDebugDraw::AddDebugOutput("closest pool: %f,%f,%f,%f", VEC4V_ARGS(*out_closestPoolSphere));
						})
					}

					if (out_closestRiverDistance)
					{
						if (closestRiver)
						{
							const Vec3V point0 = m_riverPoints[(int)closestRiver->m_start + closestRiverSegment + 0].m_point.GetXYZ();
							const Vec3V point1 = m_riverPoints[(int)closestRiver->m_start + closestRiverSegment + 1].m_point.GetXYZ();

							*out_closestRiverDistance = sqrtf(DistanceToSegmentSquared(
								VEC2V_ARGS(camPos),
								VEC2V_ARGS(point0),
								VEC2V_ARGS(point1)
							));

							BANK_ONLY(if (bShowInfo)
							{
								grcDebugDraw::AddDebugOutput("closest river dist: %f", *out_closestRiverDistance);
							})
						}
						else
						{
							*out_closestRiverDistance = FLT_MAX;
						}
					}

					out_weight = Max<float>(0.0f, 1.0f - sqrtf(weight.Getf()));
					return height.Getf();
				}
				BANK_ONLY(else if (bShowInfo)
				{
					grcDebugDraw::AddDebugOutput("watermap: tile.m_start=0x%04x", tile.m_start);
				})
			}
			BANK_ONLY(else if (bShowInfo)
			{
				grcDebugDraw::AddDebugOutput("watermap: i,j=%d,%d, row defined for [%d..%d]", i, j, row.m_first, row.m_first + row.m_count - 1);
			})
		}
		BANK_ONLY(else if (bShowInfo)
		{
			grcDebugDraw::AddDebugOutput("watermap: i,j=%d,%d", i, j);
		})

		if (out_closestPoolSphere)
		{
			*out_closestPoolSphere = Vec4V(V_ZERO);
		}

		if (out_closestRiverDistance)
		{
			*out_closestRiverDistance = FLT_MAX;
		}

		out_weight = 0.0f;
		return 0.0f;
	}

	float GetClosestWaterDistance(Vec3V_In pos, bool bOnlyRivers) const
	{
		const int i = (int)floorf(+(pos.GetXf() - m_header.m_offsetX)/m_header.m_tileSizeX);
		const int j = (int)floorf(-(pos.GetYf() - m_header.m_offsetY)/m_header.m_tileSizeY); // flipped

		if (i >= 0 && i < (int)m_header.m_numTileCols &&
			j >= 0 && j < (int)m_header.m_numTileRows)
		{
			const CWaterMapTileRow& row = m_tileRows[j];
			const int i2 = i - row.m_first;

			if (i2 >= 0 && i2 < row.m_count)
			{
				const CWaterMapTile& tile = m_tiles[(int)row.m_start + i2];

				if (tile.m_start != 0xffff)
				{
					ScalarV closestDistanceSquared(V_FLT_MAX);
					bool    closestDistanceIsRiver = false;

					for (int k = 0; ; k++)
					{
						const CWaterMapRefID& refID = m_refIDs[(int)tile.m_start + k];

						switch (refID.GetType())
						{
						case CWaterMapRefID::TYPE_OCEAN:
							{
								break;
							}
						case CWaterMapRefID::TYPE_RIVER:
							{
								BANK_ONLY(if (g_waterMapRiversEnabled))
								{
									const int segment = refID.GetSegment();
									const CWaterMapRiver& river = m_rivers[refID.GetIndex()];
									const ScalarV d = river.GetDistanceSquared(pos, segment, m_riverPoints);

									if (IsGreaterThanAll(closestDistanceSquared, d))
									{
										closestDistanceSquared = d;
										closestDistanceIsRiver = true;
									}
								}
								break;
							}
						case CWaterMapRefID::TYPE_LAKE:
							{
								BANK_ONLY(if (g_waterMapLakesEnabled))
								{
									const int segment = refID.GetSegment();
									const CWaterMapLake& lake = m_lakes[refID.GetIndex()];
									const ScalarV d = lake.GetDistanceSquared(pos, segment, m_lakeBoxes);

									if (IsGreaterThanAll(closestDistanceSquared, d))
									{
										closestDistanceSquared = d;
										closestDistanceIsRiver = false;
									}
								}
								break;
							}
						case CWaterMapRefID::TYPE_POOL:
							{
								BANK_ONLY(if (g_waterMapPoolsEnabled))
								{
									const CWaterMapPool& pool = m_pools[refID.GetIndex()];
									const ScalarV d = pool.GetDistanceSquared(pos);

									if (IsGreaterThanAll(closestDistanceSquared, d))
									{
										closestDistanceSquared = d;
										closestDistanceIsRiver = false;
									}
								}
								break;
							}
						}

						if (refID.IsLastRefID())
						{
							break;
						}
					}

					if (closestDistanceIsRiver || !bOnlyRivers)
					{
						return sqrtf(closestDistanceSquared.Getf());
					}
				}
			}
		}

		return FLT_MAX;
	}

#if __BANK
	void DebugDraw(Vec3V_In, const CWaterMapRefID& currentRefID) const
	{
		u8 alpha = 255;

		if (g_waterMapDebugDrawCurrentRegion)
		{
			switch (currentRefID.GetType())
			{
			case CWaterMapRefID::TYPE_OCEAN : break;
			case CWaterMapRefID::TYPE_RIVER : m_rivers[currentRefID.GetIndex()].DebugDraw(m_riverPoints, 255, true); break;
			case CWaterMapRefID::TYPE_LAKE  : m_lakes [currentRefID.GetIndex()].DebugDraw(m_lakeBoxes, 255, true); break;
			case CWaterMapRefID::TYPE_POOL  : m_pools [currentRefID.GetIndex()].DebugDraw(255, true); break;
			}

			alpha = 64;
		}

		if (g_waterMapDebugDrawAllRivers)
		{
			for (int i = 0; i < (int)m_header.m_numRivers; i++)
			{
				m_rivers[i].DebugDraw(m_riverPoints, alpha, false);
			}
		}

		if (g_waterMapDebugDrawAllLakes)
		{
			for (int i = 0; i < (int)m_header.m_numLakes; i++)
			{
				m_lakes[i].DebugDraw(m_lakeBoxes, alpha, false);
			}
		}

		if (g_waterMapDebugDrawAllPools)
		{
			for (int i = 0; i < (int)m_header.m_numPools; i++)
			{
				m_pools[i].DebugDraw(alpha, false);
			}
		}
	}
#endif // __BANK

	CWaterMapHeader      m_header;
	u8*                  m_data;
	CWaterMapTileRow*    m_tileRows;
	CWaterMapTile*       m_tiles;
	CWaterMapRefID*      m_refIDs;
	CWaterMapRiverPoint* m_riverPoints;
	CWaterMapRiver*      m_rivers;
	CWaterMapLakeBox*    m_lakeBoxes;
	CWaterMapLake*       m_lakes;
	CWaterMapPool*       m_pools;

#if __BANK
	CWaterMapRiverDebugInfo* m_riverDebugInfo;
	CWaterMapLakeDebugInfo*  m_lakeDebugInfo;
	CWaterMapPoolDebugInfo*  m_poolDebugInfo;
#endif // __BANK
};

static CWaterMap g_waterMap;

static float g_currentWaterHeight = 0.0f;
static float g_currentWaterWeight = 0.0f;
static Vec4V g_currentClosestPoolSphere = Vec4V(V_ZERO);
static float g_currentClosestRiverDistance = FLT_MAX;

#if __BANK
static CWaterMapRefID g_currentWaterRefID(0);
#endif // __BANK

// ================================================================================================

PARAM(waterheightpath, "");

__COMMENT(static) void CGameWorldWaterHeight::Init(unsigned initMode)
{
	if (initMode == INIT_AFTER_MAP_LOADED)
	{
		const char* path = NULL;

		if (PARAM_waterheightpath.Get(path))
		{
			Displayf("loading %s", path);
			g_waterMap.Load(path);
			return;
		}

		const CDataFileMgr::DataFile* pData = DATAFILEMGR.GetLastFile(CDataFileMgr::WORLD_WATERHEIGHT_FILE);
		Assertf(!DATAFILEMGR.IsValid(DATAFILEMGR.GetPrevFile(pData)), "More than 1 world water height data file was defined! Only loaded the last file that was defined: %s", pData->m_filename);

		if (DATAFILEMGR.IsValid(pData))
		{
			g_waterMap.Load(pData->m_filename);
		}
	}
}

__COMMENT(static) void CGameWorldWaterHeight::Shutdown(unsigned)
{
}

#if __BANK

class CWaterMapTest
{
public:
	static CWaterMapTest& Get()
	{
		static CWaterMapTest s;
		return s;
	}

	static void Create_button()
	{
		const int w = (int)floorf((float)(sm_boundsMaxX - sm_boundsMinX)*sm_cellsPerMetre);
		const int h = (int)floorf((float)(sm_boundsMaxY - sm_boundsMinY)*sm_cellsPerMetre);

		if (!AssertVerify(w >= 1 && h >= 1))
		{
			return;
		}

		if (sm_saveHeightImage)
		{
			float zmin = +FLT_MAX;
			float zmax = -FLT_MAX;

			for (int j = 0; j < h; j++)
			{
				for (int i = 0; i < w; i++)
				{
					float weight = 0.0f;
					CWaterMapRefID refID(0);

					const float x = (float)sm_boundsMinX + (0.5f + (float)i)/sm_cellsPerMetre;
					const float y = (float)sm_boundsMaxY - (0.5f + (float)j)/sm_cellsPerMetre;
					const float z = g_waterMap.GetHeight(false, Vec3V(x, y, 0.0f), NULL, NULL, NULL, weight, refID);

					if (refID.IsValid())
					{
						zmin = Min<float>(z, zmin);
						zmax = Max<float>(z, zmax);
					}
				}
			}

			if (!AssertVerify(zmin < zmax))
			{
				return;
			}

			grcImage* pImage = grcImage::Create(w, h, 1, grcImage::L8, grcImage::STANDARD, 0, 0);

			if (AssertVerify(pImage))
			{
				u8* data = pImage->GetBits();
				sysMemSet(data, 0x00, w*h*sizeof(u8));

				for (int j = 0; j < h; j++)
				{
					for (int i = 0; i < w; i++)
					{
						float weight = 0.0f;
						CWaterMapRefID refID(0);

						const float x = (float)sm_boundsMinX + (0.5f + (float)i)/sm_cellsPerMetre;
						const float y = (float)sm_boundsMaxY - (0.5f + (float)j)/sm_cellsPerMetre;
						const float z = g_waterMap.GetHeight(false, Vec3V(x, y, 0.0f), NULL, NULL, NULL, weight, refID);

						if (refID.IsValid())
						{
							data[i + j*w] = (u8)(0.5f + 255.0f*(z - zmin)/(zmax - zmin));
						}
					}
				}

				pImage->SaveDDS(sm_saveHeightImagePath);
				pImage->Release();
			}
		}

		if (sm_saveWeightImage)
		{
			grcImage* pImage = grcImage::Create(w, h, 1, grcImage::L8, grcImage::STANDARD, 0, 0);

			if (AssertVerify(pImage))
			{
				u8* data = pImage->GetBits();
				sysMemSet(data, 0x00, w*h*sizeof(u8));

				for (int j = 0; j < h; j++)
				{
					for (int i = 0; i < w; i++)
					{
						float weight = 0.0f;
						CWaterMapRefID refID(0);

						const float x = (float)sm_boundsMinX + (0.5f + (float)i)/sm_cellsPerMetre;
						const float y = (float)sm_boundsMaxY - (0.5f + (float)j)/sm_cellsPerMetre;

						g_waterMap.GetHeight(false, Vec3V(x, y, 0.0f),  NULL, NULL, NULL, weight, refID);

						if (refID.IsValid())
						{
							data[i + j*w] = (u8)(0.5f + 255.0f*weight);
						}
					}
				}

				pImage->SaveDDS(sm_saveWeightImagePath);
				pImage->Release();
			}
		}

		if (sm_saveColourImage)
		{
			grcImage* pImage = grcImage::Create(w, h, 1, grcImage::A8R8G8B8, grcImage::STANDARD, 0, 0);

			if (AssertVerify(pImage))
			{
				Color32* data = (Color32*)pImage->GetBits();

				for (int j = 0; j < h; j++)
				{
					for (int i = 0; i < w; i++)
					{
						const float x = (float)sm_boundsMinX + (0.5f + (float)i)/sm_cellsPerMetre;
						const float y = (float)sm_boundsMaxY - (0.5f + (float)j)/sm_cellsPerMetre;
						const u8 colourWhite[] = {255, 255, 255, 255};
						const u8* colour = colourWhite;

						float weight = 0.0f;
						CWaterMapRefID refID(0);

						g_waterMap.GetHeight(false, Vec3V(x, y, 0.0f), NULL, NULL, NULL, weight, refID);

						if (refID.IsValid())
						{
							switch (refID.GetType())
							{
							case CWaterMapRefID::TYPE_OCEAN : break;
							case CWaterMapRefID::TYPE_RIVER : colour = g_waterMap.m_riverDebugInfo[refID.GetIndex()].m_colour; break;
							case CWaterMapRefID::TYPE_LAKE  : colour = g_waterMap.m_lakeDebugInfo [refID.GetIndex()].m_colour; break;
							case CWaterMapRefID::TYPE_POOL  : colour = g_waterMap.m_poolDebugInfo [refID.GetIndex()].m_colour; break;
							}

							data[i + j*w] = Color32(colour[0], colour[1], colour[2], 255);
						}
						else
						{
							data[i + j*w] = Color32(255,255,255,255);
						}
					}
				}

				pImage->SaveDDS(sm_saveColourImagePath);
				pImage->Release();
			}
		}
	}

	static void CreateOBJ_button()
	{
		CDumpGeometryToOBJ dump(sm_OBJPath);

		Displayf("watermap:");
		Displayf("  dataSizeInBytes = %d", g_waterMap.m_header.m_dataSizeInBytes);
		Displayf("  offsetX = %f"        , g_waterMap.m_header.m_offsetX        );
		Displayf("  offsetY = %f"        , g_waterMap.m_header.m_offsetY        );
		Displayf("  tileSizeX = %f"      , g_waterMap.m_header.m_tileSizeX      );
		Displayf("  tileSizeY = %f"      , g_waterMap.m_header.m_tileSizeY      );
		Displayf("  numTileCols = %d"    , g_waterMap.m_header.m_numTileCols    );
		Displayf("  numTileRows = %d"    , g_waterMap.m_header.m_numTileRows    );
		Displayf("  numTiles = %d"       , g_waterMap.m_header.m_numTiles       );
		Displayf("  numRefIDs = %d"      , g_waterMap.m_header.m_numRefIDs      );
		Displayf("  numRiverPoints = %d" , g_waterMap.m_header.m_numRiverPoints );
		Displayf("  numRivers = %d"      , g_waterMap.m_header.m_numRivers      );
		Displayf("  numLakeBoxes = %d"   , g_waterMap.m_header.m_numLakeBoxes   );
		Displayf("  numLakes = %d"       , g_waterMap.m_header.m_numLakes       );
		Displayf("  numPools = %d"       , g_waterMap.m_header.m_numPools       );

		for (int i = 0; i < (int)g_waterMap.m_header.m_numRivers; i++)
		{
			const CWaterMapRiver& river = g_waterMap.m_rivers[i];
			Vec3V point0(V_ZERO);

			for (int j = 0; j < (int)river.m_count; j++)
			{
				const Vec3V point1 = g_waterMap.m_riverPoints[(int)river.m_start + j].m_point.GetXYZ();

				if (i > 0)
				{
					dump.AddCylinder(point0, point1, ScalarV(0.1f), 6);
				}

				point0 = point1;
			}
		}

		for (int i = 0; i < (int)g_waterMap.m_header.m_numLakes; i++)
		{
			const CWaterMapLake& lake = g_waterMap.m_lakes[i];

			for (int j = 0; j < (int)lake.m_count; j++)
			{
				const CWaterMapLakeBox& box = g_waterMap.m_lakeBoxes[(int)lake.m_start + j];

				const Vec3V bmin = Vec3V(box.GetCentre() - box.GetExtent(), lake.m_centreAndHeight.GetZ());
				const Vec3V bmax = Vec3V(box.GetCentre() + box.GetExtent(), lake.m_centreAndHeight.GetZ() + ScalarV(V_ONE));

				dump.AddBoxEdges(bmin, bmax, 0.1f, 6);
			}
		}

		for (int i = 0; i < (int)g_waterMap.m_header.m_numPools; i++)
		{
			const CWaterMapPool& pool = g_waterMap.m_pools[i];

			const Vec3V bmin = pool.m_centreAndHeight - Vec3V(pool.m_extentAndWeight.GetXY(), ScalarV(V_ZERO));
			const Vec3V bmax = pool.m_centreAndHeight + Vec3V(pool.m_extentAndWeight.GetXY(), ScalarV(V_ONE));

			dump.AddBox(bmin, bmax);
		}

		dump.Close();
	}

	void AddWidgets(bkBank* pBank)
	{
		pBank->PushGroup("Water Map Test", false);
		{
			pBank->AddToggle("Save Height Image"     , &sm_saveHeightImage);
			pBank->AddText  ("Save Height Image Path", &sm_saveHeightImagePath[0], sizeof(sm_saveHeightImagePath), false);
			pBank->AddToggle("Save Weight Image"     , &sm_saveWeightImage);
			pBank->AddText  ("Save Weight Image Path", &sm_saveWeightImagePath[0], sizeof(sm_saveWeightImagePath), false);
			pBank->AddToggle("Save Colour Image"     , &sm_saveColourImage);
			pBank->AddText  ("Save Colour Image Path", &sm_saveColourImagePath[0], sizeof(sm_saveColourImagePath), false);
			pBank->AddSlider("Bounds Min X"          , &sm_boundsMinX, -10000, 10000, 1);
			pBank->AddSlider("Bounds Min Y"          , &sm_boundsMinY, -10000, 10000, 1);
			pBank->AddSlider("Bounds Max X"          , &sm_boundsMaxX, -10000, 10000, 1);
			pBank->AddSlider("Bounds Max Y"          , &sm_boundsMaxY, -10000, 10000, 1);
			pBank->AddSlider("Cells Per Metre"       , &sm_cellsPerMetre, 1.0f/8.0f, 16.0f, 1.0f/64.0f);
			pBank->AddButton("Create Image"          , Create_button);

			pBank->AddText  ("OBJ Path"              , &sm_OBJPath[0], sizeof(sm_OBJPath), false);
			pBank->AddButton("Create OBJ"            , CreateOBJ_button);
		}
		pBank->PopGroup();
	}

	static bool  sm_saveHeightImage;
	static char  sm_saveHeightImagePath[80];
	static bool  sm_saveWeightImage;
	static char  sm_saveWeightImagePath[80];
	static bool  sm_saveColourImage;
	static char  sm_saveColourImagePath[80];
	static int   sm_boundsMinX;
	static int   sm_boundsMinY;
	static int   sm_boundsMaxX;
	static int   sm_boundsMaxY;
	static float sm_cellsPerMetre;
	static char  sm_OBJPath[80];
};

__COMMENT(static) bool  CWaterMapTest::sm_saveHeightImage = true;
__COMMENT(static) char  CWaterMapTest::sm_saveHeightImagePath[] = "assets:/non_final/watermaptest_height.dds";
__COMMENT(static) bool  CWaterMapTest::sm_saveWeightImage = true;
__COMMENT(static) char  CWaterMapTest::sm_saveWeightImagePath[] = "assets:/non_final/watermaptest_weight.dds";
__COMMENT(static) bool  CWaterMapTest::sm_saveColourImage = true;
__COMMENT(static) char  CWaterMapTest::sm_saveColourImagePath[] = "assets:/non_final/watermaptest_colour.dds";
__COMMENT(static) int   CWaterMapTest::sm_boundsMinX = -2550; // swimming pool area
__COMMENT(static) int   CWaterMapTest::sm_boundsMinY = -1950;
__COMMENT(static) int   CWaterMapTest::sm_boundsMaxX = 450;
__COMMENT(static) int   CWaterMapTest::sm_boundsMaxY = 1050;
__COMMENT(static) float CWaterMapTest::sm_cellsPerMetre = 0.25f;
__COMMENT(static) char  CWaterMapTest::sm_OBJPath[] = "assets:/non_final/watermaptest.obj";

__COMMENT(static) void CGameWorldWaterHeight::AddWidgets(bkBank* pBank)
{
	pBank->AddToggle("System Enabled"       , &g_waterMapSystemEnabled);
	pBank->AddToggle("Update Enabled"       , &g_waterMapUpdateEnabled);
	pBank->AddToggle("Rivers Enabled"       , &g_waterMapRiversEnabled);
	pBank->AddToggle("Lakes Enabled"        , &g_waterMapLakesEnabled);
	pBank->AddToggle("Pools Enabled"        , &g_waterMapPoolsEnabled);
	pBank->AddToggle("Pool Viewport Culling", &g_waterMapPoolViewportCulling);

#if WATER_MAP_OCEAN_TRANSITION || WATER_MAP_OCEAN_PIXEL_TEST
	pBank->AddToggle("Ocean Height", &g_waterMapOceanHeight);
#endif // WATER_MAP_OCEAN_TRANSITION || WATER_MAP_OCEAN_PIXEL_TEST

#if WATER_MAP_OCEAN_TRANSITION
	pBank->AddToggle("Ocean Transition Enabled"   , &g_waterMapOceanTransitionEnabled);
	pBank->AddSlider("Ocean Transition Height Min", &g_waterMapOceanTransitionHeightMin, 0.0f, 1000.0f, 1.0f/4.0f);
	pBank->AddSlider("Ocean Transition Height Max", &g_waterMapOceanTransitionHeightMax, 0.0f, 1000.0f, 1.0f/4.0f);
#endif // WATER_MAP_OCEAN_TRANSITION

#if WATER_MAP_OCEAN_PIXEL_TEST
	pBank->AddToggle("Ocean Pixel Test Enabled"   , &g_waterMapOceanPixelTestEnabled);
	pBank->AddSlider("Ocean Pixel Test Min Thresh", &g_waterMapOceanPixelTestMinThresh, 0.0f, 1.0f, 0.005f);
	pBank->AddSlider("Ocean Pixel Test Min Ratio" , &g_waterMapOceanPixelTestMinRatio, 0.0f, 1.0f, 0.005f);
	pBank->AddSlider("Ocean Zero Multiplier"      , &g_waterMapOceanPixelTestOceanZeroMultiplier, 1, 20, 1);
#endif // WATER_MAP_OCEAN_PIXEL_TEST

	pBank->AddToggle("Debug Show Info"          , &g_waterMapDebugShowInfo);
	pBank->AddToggle("Debug Draw Current Region", &g_waterMapDebugDrawCurrentRegion);
	pBank->AddToggle("Debug Draw All Rivers"    , &g_waterMapDebugDrawAllRivers);
	pBank->AddToggle("Debug Draw All Lakes"     , &g_waterMapDebugDrawAllLakes);
	pBank->AddToggle("Debug Draw All Pools"     , &g_waterMapDebugDrawAllPools);
	pBank->AddSlider("Debug Pool Index"         , &g_waterMapDebugPoolIndex, -1, 1000, 1);

	// dump
	{
		class DumpWaterMap { public: static void func()
		{
			fiStream* f = fiStream::Create("assets:/non_final/dumpwatermap.txt");

			if (f)
			{
				fprintf(f, "g_waterMap.m_header.m_dataSizeInBytes=%d\n", g_waterMap.m_header.m_dataSizeInBytes);
				fprintf(f, "g_waterMap.m_header.m_offsetX=%f\n"        , g_waterMap.m_header.m_offsetX        );
				fprintf(f, "g_waterMap.m_header.m_offsetY=%f\n"        , g_waterMap.m_header.m_offsetY        );
				fprintf(f, "g_waterMap.m_header.m_tileSizeX=%f\n"      , g_waterMap.m_header.m_tileSizeX      );
				fprintf(f, "g_waterMap.m_header.m_tileSizeY=%f\n"      , g_waterMap.m_header.m_tileSizeY      );
				fprintf(f, "g_waterMap.m_header.m_numTileCols=%d\n"    , g_waterMap.m_header.m_numTileCols    );
				fprintf(f, "g_waterMap.m_header.m_numTileRows=%d\n"    , g_waterMap.m_header.m_numTileRows    );
				fprintf(f, "g_waterMap.m_header.m_numTiles=%d\n"       , g_waterMap.m_header.m_numTiles       );
				fprintf(f, "g_waterMap.m_header.m_numRefIDs=%d\n"      , g_waterMap.m_header.m_numRefIDs      );
				fprintf(f, "g_waterMap.m_header.m_numRiverPoints=%d\n" , g_waterMap.m_header.m_numRiverPoints );
				fprintf(f, "g_waterMap.m_header.m_numRivers=%d\n"      , g_waterMap.m_header.m_numRivers      );
				fprintf(f, "g_waterMap.m_header.m_numLakeBoxes=%d\n"   , g_waterMap.m_header.m_numLakeBoxes   );
				fprintf(f, "g_waterMap.m_header.m_numLakes=%d\n"       , g_waterMap.m_header.m_numLakes       );
				fprintf(f, "g_waterMap.m_header.m_numPools=%d\n"       , g_waterMap.m_header.m_numPools       );
				fprintf(f, "\n");
				fprintf(f, "g_waterMap.m_header.m_sizeofWaterMapHeader=%d\n"    , g_waterMap.m_header.m_sizeofWaterMapHeader    );
				fprintf(f, "g_waterMap.m_header.m_sizeofWaterMapTileRow=%d\n"   , g_waterMap.m_header.m_sizeofWaterMapTileRow   );
				fprintf(f, "g_waterMap.m_header.m_sizeofWaterMapTile=%d\n"      , g_waterMap.m_header.m_sizeofWaterMapTile      );
				fprintf(f, "g_waterMap.m_header.m_sizeofWaterMapRefID=%d\n"     , g_waterMap.m_header.m_sizeofWaterMapRefID     );
				fprintf(f, "g_waterMap.m_header.m_sizeofWaterMapRiverPoint=%d\n", g_waterMap.m_header.m_sizeofWaterMapRiverPoint);
				fprintf(f, "g_waterMap.m_header.m_sizeofWaterMapRiver=%d\n"     , g_waterMap.m_header.m_sizeofWaterMapRiver     );
				fprintf(f, "g_waterMap.m_header.m_sizeofWaterMapLakeBox=%d\n"   , g_waterMap.m_header.m_sizeofWaterMapLakeBox   );
				fprintf(f, "g_waterMap.m_header.m_sizeofWaterMapLake=%d\n"      , g_waterMap.m_header.m_sizeofWaterMapLake      );
				fprintf(f, "g_waterMap.m_header.m_sizeofWaterMapPool=%d\n"      , g_waterMap.m_header.m_sizeofWaterMapPool      );
				fprintf(f, "\n");

				for (int i = 0; i < (int)g_waterMap.m_header.m_numRiverPoints; i++)
				{
					fprintf(f, "g_watermap.m_riverPoints[%d]: %f,%f,%f,%f\n",
						i,
						VEC4V_ARGS(g_waterMap.m_riverPoints[i].m_point));
				}

				fprintf(f, "\n");

				for (int i = 0; i < (int)g_waterMap.m_header.m_numRivers; i++)
				{
					fprintf(f, "g_watermap.m_numRivers[%d]: centre=%f,%f, height=%f, extent=%f,%f, weight=%f, count=%d, start=%d\n",
						i,
						VEC3V_ARGS(g_waterMap.m_rivers[i].m_centreAndHeight),
						VEC3V_ARGS(g_waterMap.m_rivers[i].m_extentAndWeight),
						g_waterMap.m_rivers[i].m_count,
						g_waterMap.m_rivers[i].m_start);
				}

				// TODO -- i could dump lakes and pools too, but i just needed this to debug rivers
				f->Close();
			}
		}};

		pBank->AddButton("Dump", DumpWaterMap::func);
	}

	CWaterMapTest::Get().AddWidgets(pBank);
}

__COMMENT(static) void CGameWorldWaterHeight::DebugDraw(Vec3V_In camPos)
{
	g_waterMap.DebugDraw(camPos, g_currentWaterRefID);

	if (g_waterMapDebugDrawCurrentRegion)
	{
		grcDebugDraw::AddDebugOutput(
			"water height = %f @ [%d,%d], type = %d, segment = %d, index = %d",
			GetWaterHeight(),
			g_currentWaterTileIndices[0],
			g_currentWaterTileIndices[1],
			g_currentWaterRefID.GetType(),
			g_currentWaterRefID.GetSegment(),
			g_currentWaterRefID.GetIndex()
		);
	}
}

#endif // __BANK

__COMMENT(static) bool CGameWorldWaterHeight::IsEnabled()
{
	return BANK_SWITCH(g_waterMapSystemEnabled, true);
}

__COMMENT(static) void CGameWorldWaterHeight::Update()
{
	BANK_ONLY(if (g_waterMapUpdateEnabled))
	{
		const grcViewport* vp = gVpMan.GetUpdateGameGrcViewport();

		if (vp)
		{
			const Vec3V camPos = vp->GetCameraPosition();

			g_currentWaterHeight = g_waterMap.GetHeight(true, camPos, vp, &g_currentClosestPoolSphere, &g_currentClosestRiverDistance, g_currentWaterWeight BANK_PARAM(g_currentWaterRefID));

#if WATER_MAP_OCEAN_TRANSITION
			if (g_waterMapOceanTransitionEnabled) // transition to ocean height as camera goes above water
			{
				const float camHeightAboveWater = camPos.GetZf() - g_currentWaterHeight;

				if (camHeightAboveWater >= g_waterMapOceanTransitionHeightMax)
				{
					g_currentWaterHeight = g_waterMapOceanHeight;
				}
				else if (camHeightAboveWater > g_waterMapOceanTransitionHeightMin)
				{
					const float t = (camHeightAboveWater - g_waterMapOceanTransitionHeightMin)/(g_waterMapOceanTransitionHeightMax - g_waterMapOceanTransitionHeightMin);

					g_currentWaterHeight += (g_waterMapOceanHeight - g_currentWaterHeight)*t;
				}
			}
#endif // WATER_MAP_OCEAN_TRANSITION

#if WATER_MAP_OCEAN_PIXEL_TEST
			if (g_waterMapOceanPixelTestEnabled)
			{
				const int numOceanPixels = Water::GetWaterPixelsRenderedOcean();
				const int numWaterPixels = Water::GetWaterPixelsRenderedPlanarReflective(true);
				const int numScreenPixels = GRCDEVICE.GetWidth()*GRCDEVICE.GetHeight();

				const float ocean = (float)numOceanPixels/(float)numScreenPixels;
				const float ratio = (float)numOceanPixels/(float)numWaterPixels;

				if (ocean > g_waterMapOceanPixelTestMinThresh &&
					ratio > g_waterMapOceanPixelTestMinRatio)
				{
					// find the height of the ocean body which is covering the most pixels
					int oceanQueryType = -1;
					u32 oceanNumPixels = 0;

					for (int i = Water::query_oceanstart; i <= Water::query_oceanend; i++)
					{
						const u32 numPixels = Water::GetWaterPixelsRendered(i, true)*(i == Water::query_ocean0 ? g_waterMapOceanPixelTestOceanZeroMultiplier : 1);

						if (oceanNumPixels < numPixels)
						{
							oceanNumPixels = numPixels;
							oceanQueryType = i;
						}
					}

					if (AssertVerify(oceanQueryType != -1))
					{
						g_waterMapOceanHeight = Water::GetOceanHeight(oceanQueryType);
					}

					g_currentWaterHeight = g_waterMapOceanHeight;
				}
			}
#endif // WATER_MAP_OCEAN_PIXEL_TEST
		}
	}
}

__COMMENT(static) float CGameWorldWaterHeight::GetWaterHeight()
{
	return g_currentWaterHeight;
}

__COMMENT(static) float CGameWorldWaterHeight::GetWaterHeightAtPos(Vec3V_In pos)
{
	float weight = 0.0f;
	BANK_ONLY(CWaterMapRefID dummyi(0));
	float height = g_waterMap.GetHeight(false, pos, NULL, NULL, NULL, weight BANK_PARAM(dummyi));
	if(weight == 1.0f)
	{
		return height;
	}

	return 0.0f;
}

__COMMENT(static) Vec4V_Out CGameWorldWaterHeight::GetClosestPoolSphere()
{
	return g_currentClosestPoolSphere;
}

__COMMENT(static) float CGameWorldWaterHeight::GetDistanceToClosestRiver()
{
	return g_currentClosestRiverDistance;
}

__COMMENT(static) float CGameWorldWaterHeight::GetDistanceToClosestRiver(Vec3V_In pos)
{
	return g_waterMap.GetClosestWaterDistance(pos, true);
}

__COMMENT(static) float CGameWorldWaterHeight::GetDistanceToClosestWater(Vec3V_In pos)
{
	return g_waterMap.GetClosestWaterDistance(pos, false);
}
