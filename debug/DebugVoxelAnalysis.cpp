// ============================
// debug/DebugVoxelAnalysis.cpp
// (c) 2013 RockstarNorth
// ============================

#if __BANK

/*
===================================================================================================
TODO
- this system is way too slow, needs optimisation
- automated reports for multiple drawables
	- hook up to AssetAnalysis
	- generate a csv file with reports for all drawables
	- generate one OBJ file per drawable
- we could make a much faster version of this (at least 3x faster, probably more) if we only
  projected onto XY plane, i think this might be good enough
===================================================================================================
*/

#include "atl/string.h"
#include "bank/bkmgr.h"
#include "bank/bank.h"
#include "file/stream.h"
#include "rmcore/lodgroup.h"
#include "system/memory.h"

#include "fwdebug/picker.h"
#include "fwmaths/vectorutil.h"
#include "fwutil/xmacro.h"

#include "scene/Entity.h"
#include "debug/DebugGeometryUtil.h"
#include "debug/DebugVoxelAnalysis.h"

// see BS#1196726

CVoxelAnalysis::CVoxelAnalysis(atArray<const rmcDrawable*>& drawables, atArray<Mat34V>& matrices, int maxResolution, float maxCellsPerMetre, u32 flags)
{
	Init(drawables, matrices, maxResolution, maxCellsPerMetre, flags);
}

CVoxelAnalysis::CVoxelAnalysis(const rmcDrawable* pDrawable, int maxResolution, float maxCellsPerMetre, u32 flags)
{
	atArray<const rmcDrawable*> drawables;
	atArray<Mat34V> matrices;

	drawables.PushAndGrow(pDrawable);
	matrices.PushAndGrow(Mat34V(V_IDENTITY));

	Init(drawables, matrices, maxResolution, maxCellsPerMetre, flags);
}

CVoxelAnalysis::~CVoxelAnalysis()
{
	{
		// Debug Heap
		sysMemAutoUseDebugMemory debug;
		
		if (m_grid)
			delete [] m_grid;

		if (m_regionGrid)
			delete [] m_regionGrid;
	}
}

void CVoxelAnalysis::Init(atArray<const rmcDrawable*>& drawables, atArray<Mat34V>& matrices, int maxResolution, float maxCellsPerMetre, u32 flags)
{
	sysMemSet(this, 0, sizeof(*this));

	if (drawables.GetCount() == 0 || !AssertVerify(drawables.GetCount() == matrices.GetCount()))
	{
		return;
	}

	static Vec3V bmin; bmin = Vec3V(V_FLT_MAX);
	static Vec3V bmax; bmax = -bmin;

	// find geometry bounds
	{
		class AddTriangleForBounds { public: static void func(Vec3V_In p0, Vec3V_In p1, Vec3V_In p2, int, int, int, void*)
		{
			bmin = Min(p0, p1, p2, bmin);
			bmax = Max(p0, p1, p2, bmax);
		}};

		for (int i = 0; i < drawables.GetCount(); i++)
		{
			GeometryUtil::AddTrianglesForDrawable(drawables[i], LOD_HIGH, "", NULL, AddTriangleForBounds::func, matrices[i]);
		}
	}

	static int resX; resX = Min<int>(maxResolution, (int)ceilf((bmax - bmin).GetXf()*maxCellsPerMetre));
	static int resY; resY = Min<int>(maxResolution, (int)ceilf((bmax - bmin).GetYf()*maxCellsPerMetre));
	static int resZ; resZ = Min<int>(maxResolution, (int)ceilf((bmax - bmin).GetZf()*maxCellsPerMetre));

	// equalise cell size
	{
		const float cellSizeMax = Max<float>
		(
			(bmax - bmin).GetXf()/(float)resX,
			(bmax - bmin).GetYf()/(float)resY,
			(bmax - bmin).GetZf()/(float)resZ
		);

		resX = Min<int>((int)ceilf((bmax - bmin).GetXf()/cellSizeMax), resX);
		resY = Min<int>((int)ceilf((bmax - bmin).GetYf()/cellSizeMax), resY);
		resZ = Min<int>((int)ceilf((bmax - bmin).GetZf()/cellSizeMax), resZ);
	}

	static u8* gridXYMinZ;
	static u8* gridXYMaxZ;
	static u8* gridYZMinX;
	static u8* gridYZMaxX;
	static u8* gridZXMinY;
	static u8* gridZXMaxY;

	{
		// Debug Heap
		sysMemAutoUseDebugMemory debug;
		gridXYMinZ = rage_new u8[resX*resY];
		gridXYMaxZ = rage_new u8[resX*resY];
		gridYZMinX = rage_new u8[resY*resZ];
		gridYZMaxX = rage_new u8[resY*resZ];
		gridZXMinY = rage_new u8[resZ*resX];
		gridZXMaxY = rage_new u8[resZ*resX];
	}

	sysMemSet(gridXYMinZ, 0xff, resX*resY);
	sysMemSet(gridXYMaxZ, 0x00, resX*resY);
	sysMemSet(gridYZMinX, 0xff, resY*resZ);
	sysMemSet(gridYZMaxX, 0x00, resY*resZ);
	sysMemSet(gridZXMinY, 0xff, resZ*resX);
	sysMemSet(gridZXMaxY, 0x00, resZ*resX);

	// add triangles
	{
		class AddTriangle { public: static void func(Vec3V_In p0, Vec3V_In p1, Vec3V_In p2, int, int, int, void*)
		{
			const Vec3V pmin = Min(p0, p1, p2);
			const Vec3V pmax = Max(p0, p1, p2);

			const Vec3V tmin = (pmin - bmin)/(bmax - bmin); // [0..1]
			const Vec3V tmax = (pmax - bmin)/(bmax - bmin); // [0..1]

			const int i0 = (int)floorf(tmin.GetXf()*(float)resX); // [0..resX]
			const int j0 = (int)floorf(tmin.GetYf()*(float)resY); // [0..resY]
			const int k0 = (int)floorf(tmin.GetZf()*(float)resZ); // [0..resZ]

			const int i1 = (int)ceilf(tmax.GetXf()*(float)resX) - 1; // [0..resX]
			const int j1 = (int)ceilf(tmax.GetYf()*(float)resY) - 1; // [0..resY]
			const int k1 = (int)ceilf(tmax.GetZf()*(float)resZ) - 1; // [0..resZ]

			// ======================================================================
			// === XY ===============================================================
			// ======================================================================

			if (i0 == i1 &&
				j0 == j1)
			{
				const float t0 = tmin.GetZf(); // [0..1]
				const float t1 = tmax.GetZf(); // [0..1]

				u8& minZ = gridXYMinZ[i0 + j0*resX];
				u8& maxZ = gridXYMaxZ[i0 + j0*resX];

				minZ = Min<u8>((u8)floorf(Max<float>(0.0f, t0*(float)resZ - 0.0f)), minZ);
				maxZ = Max<u8>((u8)ceilf (Max<float>(0.0f, t1*(float)resZ - 1.0f)), maxZ);
			}
			else
			{
				for (int i = i0; i <= i1; i++)
				for (int j = j0; j <= j1; j++)
				{
					const float x0 = bmin.GetXf() + (bmax - bmin).GetXf()*(float)(i + 0)/(float)resX;
					const float y0 = bmin.GetYf() + (bmax - bmin).GetYf()*(float)(j + 0)/(float)resY;
					const float x1 = bmin.GetXf() + (bmax - bmin).GetXf()*(float)(i + 1)/(float)resX;
					const float y1 = bmin.GetYf() + (bmax - bmin).GetYf()*(float)(j + 1)/(float)resY;

					int   count = 3;
					Vec3V temp0[3 + 4] = {p0, p1, p2};
					Vec3V temp1[3 + 4];

					// clip to four planes
					count = PolyClip(temp1, NELEM(temp1), temp0, count, BuildPlane(Vec3V(x0, y0, 0.0f), +Vec3V(V_X_AXIS_WZERO)));
					count = PolyClip(temp0, NELEM(temp0), temp1, count, BuildPlane(Vec3V(x0, y0, 0.0f), +Vec3V(V_Y_AXIS_WZERO)));
					count = PolyClip(temp1, NELEM(temp1), temp0, count, BuildPlane(Vec3V(x1, y1, 0.0f), -Vec3V(V_X_AXIS_WZERO)));
					count = PolyClip(temp0, NELEM(temp0), temp1, count, BuildPlane(Vec3V(x1, y1, 0.0f), -Vec3V(V_Y_AXIS_WZERO)));

					if (count > 0)
					{
						float z0 = temp0[0].GetZf();
						float z1 = z0;

						for (int n = 1; n < count; n++)
						{
							z0 = Min<float>(temp0[n].GetZf(), z0);
							z1 = Max<float>(temp0[n].GetZf(), z1);
						}

						const float t0 = (z0 - bmin.GetZf())/(bmax - bmin).GetZf(); // [0..1]
						const float t1 = (z1 - bmin.GetZf())/(bmax - bmin).GetZf(); // [0..1]

						u8& minZ = gridXYMinZ[i + j*resX];
						u8& maxZ = gridXYMaxZ[i + j*resX];

						minZ = Min<u8>((u8)floorf(Max<float>(0.0f, t0*(float)resZ - 0.0f)), minZ);
						maxZ = Max<u8>((u8)ceilf (Max<float>(0.0f, t1*(float)resZ - 1.0f)), maxZ);
					}
				}
			}

			// ======================================================================
			// === YZ ===============================================================
			// ======================================================================

			if (j0 == j1 &&
				k0 == k1)
			{
				const float t0 = tmin.GetXf(); // [0..1]
				const float t1 = tmax.GetXf(); // [0..1]

				u8& minX = gridYZMinX[j0 + k0*resY];
				u8& maxX = gridYZMaxX[j0 + k0*resY];

				minX = Min<u8>((u8)floorf(Max<float>(0.0f, t0*(float)resX - 0.0f)), minX);
				maxX = Max<u8>((u8)ceilf (Max<float>(0.0f, t1*(float)resX - 1.0f)), maxX);
			}
			else
			{
				for (int j = j0; j <= j1; j++)
				for (int k = k0; k <= k1; k++)
				{
					const float y0 = bmin.GetYf() + (bmax - bmin).GetYf()*(float)(j + 0)/(float)resY;
					const float z0 = bmin.GetZf() + (bmax - bmin).GetZf()*(float)(k + 0)/(float)resZ;
					const float y1 = bmin.GetYf() + (bmax - bmin).GetYf()*(float)(j + 1)/(float)resY;
					const float z1 = bmin.GetZf() + (bmax - bmin).GetZf()*(float)(k + 1)/(float)resZ;

					int   count = 3;
					Vec3V temp0[3 + 4] = {p0, p1, p2};
					Vec3V temp1[3 + 4];

					// clip to four planes
					count = PolyClip(temp1, NELEM(temp1), temp0, count, BuildPlane(Vec3V(0.0f, y0, z0), +Vec3V(V_Y_AXIS_WZERO)));
					count = PolyClip(temp0, NELEM(temp0), temp1, count, BuildPlane(Vec3V(0.0f, y0, z0), +Vec3V(V_Z_AXIS_WZERO)));
					count = PolyClip(temp1, NELEM(temp1), temp0, count, BuildPlane(Vec3V(0.0f, y1, z1), -Vec3V(V_Y_AXIS_WZERO)));
					count = PolyClip(temp0, NELEM(temp0), temp1, count, BuildPlane(Vec3V(0.0f, y1, z1), -Vec3V(V_Z_AXIS_WZERO)));

					if (count > 0)
					{
						float x0 = temp0[0].GetXf();
						float x1 = x0;

						for (int n = 1; n < count; n++)
						{
							x0 = Min<float>(temp0[n].GetXf(), x0);
							x1 = Max<float>(temp0[n].GetXf(), x1);
						}

						const float t0 = (x0 - bmin.GetXf())/(bmax - bmin).GetXf(); // [0..1]
						const float t1 = (x1 - bmin.GetXf())/(bmax - bmin).GetXf(); // [0..1]

						u8& minX = gridYZMinX[j + k*resY];
						u8& maxX = gridYZMaxX[j + k*resY];

						minX = Min<u8>((u8)floorf(Max<float>(0.0f, t0*(float)resX - 0.0f)), minX);
						maxX = Max<u8>((u8)ceilf (Max<float>(0.0f, t1*(float)resX - 1.0f)), maxX);
					}
				}
			}

			// ======================================================================
			// === ZX ===============================================================
			// ======================================================================

			if (k0 == k1 &&
				i0 == i1)
			{
				const float t0 = tmin.GetYf(); // [0..1]
				const float t1 = tmax.GetYf(); // [0..1]

				u8& minY = gridZXMinY[k0 + i0*resZ];
				u8& maxY = gridZXMaxY[k0 + i0*resZ];

				minY = Min<u8>((u8)floorf(Max<float>(0.0f, t0*(float)resY - 0.0f)), minY);
				maxY = Max<u8>((u8)ceilf (Max<float>(0.0f, t1*(float)resY - 1.0f)), maxY);
			}
			else
			{
				for (int k = k0; k <= k1; k++)
				for (int i = i0; i <= i1; i++)
				{
					const float z0 = bmin.GetZf() + (bmax - bmin).GetZf()*(float)(k + 0)/(float)resZ;
					const float x0 = bmin.GetXf() + (bmax - bmin).GetXf()*(float)(i + 0)/(float)resX;
					const float z1 = bmin.GetZf() + (bmax - bmin).GetZf()*(float)(k + 1)/(float)resZ;
					const float x1 = bmin.GetXf() + (bmax - bmin).GetXf()*(float)(i + 1)/(float)resX;

					int   count = 3;
					Vec3V temp0[3 + 4] = {p0, p1, p2};
					Vec3V temp1[3 + 4];

					// clip to four planes
					count = PolyClip(temp1, NELEM(temp1), temp0, count, BuildPlane(Vec3V(x0, 0.0f, z0), +Vec3V(V_Z_AXIS_WZERO)));
					count = PolyClip(temp0, NELEM(temp0), temp1, count, BuildPlane(Vec3V(x0, 0.0f, z0), +Vec3V(V_X_AXIS_WZERO)));
					count = PolyClip(temp1, NELEM(temp1), temp0, count, BuildPlane(Vec3V(x1, 0.0f, z1), -Vec3V(V_Z_AXIS_WZERO)));
					count = PolyClip(temp0, NELEM(temp0), temp1, count, BuildPlane(Vec3V(x1, 0.0f, z1), -Vec3V(V_X_AXIS_WZERO)));

					if (count > 0)
					{
						float y0 = temp0[0].GetYf();
						float y1 = y0;

						for (int n = 1; n < count; n++)
						{
							y0 = Min<float>(temp0[n].GetYf(), y0);
							y1 = Max<float>(temp0[n].GetYf(), y1);
						}

						const float t0 = (y0 - bmin.GetYf())/(bmax - bmin).GetYf(); // [0..1]
						const float t1 = (y1 - bmin.GetYf())/(bmax - bmin).GetYf(); // [0..1]

						u8& minY = gridZXMinY[k + i*resZ];
						u8& maxY = gridZXMaxY[k + i*resZ];

						minY = Min<u8>((u8)floorf(Max<float>(0.0f, t0*(float)resY - 0.0f)), minY);
						maxY = Max<u8>((u8)ceilf (Max<float>(0.0f, t1*(float)resY - 1.0f)), maxY);
					}
				}
			}
		}};

		for (int i = 0; i < drawables.GetCount(); i++)
		{
			GeometryUtil::AddTrianglesForDrawable(drawables[i], LOD_HIGH, "", NULL, AddTriangle::func, matrices[i]);
		}
	}

	if ((flags & PROJECT_ALONG_X) == 0)
	{
		for (int j = 0; j < resY; j++)
		for (int k = 0; k < resZ; k++)
		{
			gridYZMinX[j + k*resY] = 0;
			gridYZMaxX[j + k*resY] = (u8)resX;
		}
	}

	if ((flags & PROJECT_ALONG_Y) == 0)
	{
		for (int k = 0; k < resZ; k++)
		for (int i = 0; i < resX; i++)
		{
			gridZXMinY[k + i*resZ] = 0;
			gridZXMaxY[k + i*resZ] = (u8)resY;
		}
	}

	if ((flags & PROJECT_ALONG_Z) == 0)
	{
		for (int i = 0; i < resX; i++)
		for (int j = 0; j < resY; j++)
		{
			gridXYMinZ[i + j*resX] = 0;
			gridXYMaxZ[i + j*resX] = (u8)resZ;
		}
	}

	int numCellsXY = 0;
	int numCellsYZ = 0;
	int numCellsZX = 0;

	for (int i = 0; i < resX; i++) for (int j = 0; j < resY; j++) { if (gridXYMinZ[i + j*resX] <= gridXYMaxZ[i + j*resX]) { numCellsXY++; } }
	for (int j = 0; j < resY; j++) for (int k = 0; k < resZ; k++) { if (gridYZMinX[j + k*resY] <= gridYZMaxX[j + k*resY]) { numCellsYZ++; } }
	for (int k = 0; k < resZ; k++) for (int i = 0; i < resX; i++) { if (gridZXMinY[k + i*resZ] <= gridZXMaxY[k + i*resZ]) { numCellsZX++; } }

	const int gridSize = (resX*resY*resZ + 7)/8;

	int gridVolume = 0;
	u8* grid;
	
	{
		// Debug Heap
		sysMemAutoUseDebugMemory debug;
		grid = rage_new u8[gridSize];
	}
	
	sysMemSet(grid, 0, gridSize);

	for (int i = 0; i < resX; i++)
	for (int j = 0; j < resY; j++)
	for (int k = resZ - 1; k >= 0; k--)
	{
		if (i >= gridYZMinX[j + k*resY] && i <= gridYZMaxX[j + k*resY] &&
			j >= gridZXMinY[k + i*resZ] && j <= gridZXMaxY[k + i*resZ] &&
			k >= gridXYMinZ[i + j*resX] && k <= gridXYMaxZ[i + j*resX])
		{
			const int index = i + j*resX + k*resX*resY;

			grid[index/8] |= BIT(index%8);
			gridVolume++;

			if (flags & PROJECT_FLAT_BASE)
			{
				while (--k >= 0)
				{
					const int index2 = i + j*resX + k*resX*resY;

					grid[index2/8] |= BIT(index2%8);
					gridVolume++;
				}
			}
		}
	}

	{
		// Debug Heap
		sysMemAutoUseDebugMemory debug;
		delete [] gridXYMinZ;
		delete [] gridXYMaxZ;
		delete [] gridYZMinX;
		delete [] gridYZMaxX;
		delete [] gridZXMinY;
		delete [] gridZXMaxY;
	}

	m_boundsMin = bmin;
	m_boundsMax = bmax;
	m_resX      = resX;
	m_resY      = resY;
	m_resZ      = resZ;
	m_grid      = grid;
	m_areaXY    = 100.0f*(float)numCellsXY/(float)(resX*resY);
	m_areaYZ    = 100.0f*(float)numCellsYZ/(float)(resY*resZ);
	m_areaZX    = 100.0f*(float)numCellsZX/(float)(resZ*resX);
	m_volume    = 100.0f*(float)gridVolume/(float)(resX*resY*resZ);
}

#define STATIC_THIS(type) static type* _this; _this = this

void CVoxelAnalysis::FindRegions()
{
	if (m_grid)
	{
		STATIC_THIS(CVoxelAnalysis);

		{
			// Debug Heap
			sysMemAutoUseDebugMemory debug;
			m_regionGrid = rage_new u8[m_resX*m_resY*m_resZ];
		}
		
		sysMemSet(m_regionGrid, 0xff, m_resX*m_resY*m_resZ);

		class Recurse { public: static int func(int i, int j, int k, u8 regionIndex)
		{
			int count = 0;

			if (i >= 0 && i < _this->m_resX &&
				j >= 0 && j < _this->m_resY &&
				k >= 0 && k < _this->m_resZ)
			{
				const int index = i + j*_this->m_resX + k*_this->m_resX*_this->m_resY;

				if ((_this->m_grid[index/8] & BIT(index%8)) != 0 && _this->m_regionGrid[index] == 0xff)
				{
					_this->m_regionGrid[index] = regionIndex;
					count++;

					if (i > 0) { count += func(i - 1, j, k, regionIndex); }
					if (j > 0) { count += func(i, j - 1, k, regionIndex); }
					if (k > 0) { count += func(i, j, k - 1, regionIndex); }

					if (i < _this->m_resX - 1) { count += func(i + 1, j, k, regionIndex); }
					if (j < _this->m_resY - 1) { count += func(i, j + 1, k, regionIndex); }
					if (k < _this->m_resZ - 1) { count += func(i, j, k + 1, regionIndex); }
				}
			}

			return count;
		}};

		for (int i = 0; i < m_resX; i++)
		for (int j = 0; j < m_resY; j++)
		for (int k = 0; k < m_resZ; k++)
		{
			if (Recurse::func(i, j, k, m_regionCount) > 0)
			{
				m_regionCount++;
			}
		}
	}
}

void CVoxelAnalysis::MakeHollow()
{
	if (m_grid)
	{
		const int tempSize = (m_resX*m_resY*m_resZ + 7)/8;
		u8* temp;

		{
			// Debug Heap
			sysMemAutoUseDebugMemory debug;
			temp = rage_new u8[tempSize];
		}
		
		sysMemCpy(temp, m_grid, tempSize);

		for (int i = 1; i < m_resX - 1; i++)
		for (int j = 1; j < m_resY - 1; j++)
		for (int k = 1; k < m_resZ - 1; k++)
		{
			const int dx = 1;
			const int dy = m_resX;
			const int dz = m_resX*m_resY;

			const int index = i*dx + j*dy + k*dz;

			int numNeighbours = 0;

			const int px = index + dx; if (temp[px/8] & BIT(px%8)) { numNeighbours++; }
			const int py = index + dy; if (temp[py/8] & BIT(py%8)) { numNeighbours++; }
			const int pz = index + dz; if (temp[pz/8] & BIT(pz%8)) { numNeighbours++; }
			const int nx = index - dx; if (temp[nx/8] & BIT(nx%8)) { numNeighbours++; }
			const int ny = index - dy; if (temp[ny/8] & BIT(ny%8)) { numNeighbours++; }
			const int nz = index - dz; if (temp[nz/8] & BIT(nz%8)) { numNeighbours++; }

			if (numNeighbours == 6)
			{
				m_grid[index/8] &= ~BIT(index%8);
			}
		}

		{
			// Debug Heap
			sysMemAutoUseDebugMemory debug;
			delete [] temp;
		}
	}
}

void CVoxelAnalysis::DumpGridGeometryToOBJ(CDumpGeometryToOBJ& dump, float radius, int numSides, float scale, int regionIndex) const
{
	if (m_grid)
	{
		for (int i = 0; i < m_resX; i++)
		for (int j = 0; j < m_resY; j++)
		for (int k = 0; k < m_resZ; k++)
		{
			const int index = i + j*m_resX + k*m_resX*m_resY;

			if (m_grid[index/8] & BIT(index%8))
			{
				if (regionIndex != INDEX_NONE && m_regionGrid && m_regionGrid[index] != (u8)regionIndex)
				{
					continue;
				}

				Vec3V bmin = m_boundsMin + (m_boundsMax - m_boundsMin)*Vec3V((float)(i + 0)/(float)(m_resX), (float)(j + 0)/(float)(m_resY), (float)(k + 0)/(float)(m_resZ));
				Vec3V bmax = m_boundsMin + (m_boundsMax - m_boundsMin)*Vec3V((float)(i + 1)/(float)(m_resX), (float)(j + 1)/(float)(m_resY), (float)(k + 1)/(float)(m_resZ));

				if (scale != 1.0f)
				{
					const Vec3V centre = (bmax + bmin)*ScalarV(V_HALF);

					bmin = centre + (bmin - centre)*ScalarV(scale);
					bmax = centre + (bmax - centre)*ScalarV(scale);
				}

				dump.AddBoxEdges(bmin, bmax, radius, numSides);
			}
		}
	}
}

CVoxelAnalysisReport::CVoxelAnalysisReport()
{
	m_report           = NULL;
	m_maxResolution    = 32;
	m_maxCellsPerMetre = 10.0f;
	m_flatBase         = true;

	m_objDir           = "";
	m_makeHollow       = true;
	m_gridRadius       = 0.05f;
	m_gridNumSides     = 6;
	m_gridScale        = 0.8f;
}

void CVoxelAnalysisReport::Open(const char* reportPath, const char* objDir)
{
	if (reportPath && reportPath[0] != '\0')
	{
		m_report = fiStream::Create(reportPath);
	}

	if (objDir && objDir[0] != '\0')
	{
		m_objDir = objDir;
	}
}

void CVoxelAnalysisReport::Close()
{
	if (m_report)
	{
		m_report->Close();
		m_report = NULL;
	}
}

void CVoxelAnalysisReport::AddDrawable(const rmcDrawable* pDrawable, const char* path)
{
	const char* name = strrchr(path, '/');

	if (name)
	{
		name++;
	}
	else
	{
		name = path; // ?
	}

	u32 flags = 0;

	if (m_flatBase)
	{
		flags |= CVoxelAnalysis::PROJECT_FLAT_BASE;
	}

	CVoxelAnalysis vox(pDrawable, m_maxResolution, m_maxCellsPerMetre, flags);

	if (m_report)
	{
		fprintf(
			m_report,
			"%s,%s,%f,%f,%f,%dx%dx%d,%f,%f,%f,%f,%d\n",
			path,
			name,
			(vox.m_boundsMax - vox.m_boundsMin).GetXf(),
			(vox.m_boundsMax - vox.m_boundsMin).GetYf(),
			(vox.m_boundsMax - vox.m_boundsMin).GetZf(),
			vox.m_resX,
			vox.m_resY,
			vox.m_resZ,
			vox.m_areaXY,
			vox.m_areaYZ,
			vox.m_areaZX,
			vox.m_volume,
			vox.m_regionCount
		);
	}

	if (m_objDir.c_str() && m_objDir.c_str()[0] != '\0')
	{
		vox.FindRegions();

		if (m_makeHollow)
		{
			vox.MakeHollow();
		}

		CDumpGeometryToOBJ dump(atVarString("%s/%s.obj", m_objDir.c_str(), name).c_str(), "materials.mtl");
		static CDumpGeometryToOBJ* s_dump; s_dump = &dump;

		dump.MaterialBegin("white");

		class AddTriangle { public: static void func(Vec3V_In v0, Vec3V_In v1, Vec3V_In v2, int, int, int, void*)
		{
			s_dump->AddTriangle(v2, v1, v0);
		}};

		GeometryUtil::AddTrianglesForDrawable(pDrawable, LOD_HIGH, name, NULL, AddTriangle::func);

		dump.MaterialEnd();

		for (u8 regionIndex = 0; regionIndex < vox.m_regionCount; regionIndex++)
		{
			const char* materialNames[] =
			{
				"green",
				"blue",
				"cyan",
				"magenta",
				"yellow",
			};

			dump.MaterialBegin(materialNames[regionIndex%NELEM(materialNames)]);
			vox.DumpGridGeometryToOBJ(dump, m_gridRadius, m_gridNumSides, m_gridScale, (int)regionIndex);
			dump.MaterialEnd();
		}

		dump.MaterialBegin("red");
		dump.AddBoxEdges(vox.m_boundsMin, vox.m_boundsMax, m_gridRadius, m_gridNumSides);
		dump.MaterialEnd();

		dump.Close();
	}
}

__COMMENT(static) void CDebugVoxelAnalysisInterface::AddWidgets(bkBank& bank)
{
	static char  s_objPath[80]      = "assets:/non_final/dumpgeom.obj";
	static int   s_maxResolution    = 32;
	static float s_maxCellsPerMetre = 10.0f;
	static u8    s_projectFlags     = CVoxelAnalysis::PROJECT_ALONG_X | CVoxelAnalysis::PROJECT_ALONG_Y | CVoxelAnalysis::PROJECT_ALONG_Z | CVoxelAnalysis::PROJECT_FLAT_BASE;
	static bool  s_findRegions      = false; // TODO -- this blows the stack ..
	static bool  s_makeHollow       = true;
	static float s_gridRadius       = 0.1f;
	static int   s_gridNumSides     = 6;
	static float s_gridScale        = 0.9f;

	class GenerateVoxelAnalysisForSelectedEntities_button { public: static void func()
	{
		const int numEntities = g_PickerManager.GetNumberOfEntities();

		if (numEntities > 0)
		{
			atArray<const rmcDrawable*> drawables;
			atArray<Mat34V> matrices;
			const char* firstEntityName = NULL;

			CDumpGeometryToOBJ dump(s_objPath, "materials.mtl");
			static CDumpGeometryToOBJ* s_dump; s_dump = &dump;

			dump.MaterialBegin("white");

			for (int i = 0; i < numEntities; i++)
			{
				const CEntity* pEntity = static_cast<const CEntity*>(g_PickerManager.GetEntity(i));

				if (pEntity)
				{
					class AddTriangle { public: static void func(Vec3V_In v0, Vec3V_In v1, Vec3V_In v2, int, int, int, void*)
					{
						s_dump->AddTriangle(v2, v1, v0);
					}};

					GeometryUtil::AddTrianglesForEntity(pEntity, LOD_HIGH, NULL, AddTriangle::func);

					drawables.PushAndGrow(pEntity->GetDrawable());
					matrices.PushAndGrow(pEntity->GetMatrix());

					if (firstEntityName == NULL)
					{
						firstEntityName = pEntity->GetModelName();
					}
				}
			}

			dump.MaterialEnd();

			CVoxelAnalysis vox(drawables, matrices, s_maxResolution, s_maxCellsPerMetre, s_projectFlags);

			if (s_findRegions)
			{
				vox.FindRegions();
			}

			if (s_makeHollow)
			{
				vox.MakeHollow();
			}

			if (vox.m_regionCount > 1)
			{
				for (u8 regionIndex = 0; regionIndex < vox.m_regionCount; regionIndex++)
				{
					const char* materialNames[] =
					{
						"green",
						"blue",
						"cyan",
						"magenta",
						"yellow",
					};

					dump.MaterialBegin(materialNames[regionIndex%NELEM(materialNames)]);
					vox.DumpGridGeometryToOBJ(dump, s_gridRadius, s_gridNumSides, s_gridScale, (int)regionIndex);
					dump.MaterialEnd();
				}
			}
			else
			{
				dump.MaterialBegin("green");
				vox.DumpGridGeometryToOBJ(dump, s_gridRadius, s_gridNumSides, s_gridScale, INDEX_NONE);
				dump.MaterialEnd();
			}

			dump.MaterialBegin("red");
			dump.AddBoxEdges(vox.m_boundsMin, vox.m_boundsMax, s_gridRadius, s_gridNumSides);
			dump.MaterialEnd();

			dump.Close();

			if (drawables.GetCount() == 1)
			{
				Displayf("Voxel analysis (%s):", firstEntityName);
			}
			else
			{
				Displayf("Voxel analysis (%d models):", drawables.GetCount());
			}

			Displayf("  resX = %d"       , vox.m_resX);
			Displayf("  resY = %d"       , vox.m_resY);
			Displayf("  resZ = %d"       , vox.m_resZ);
			Displayf("  areaXY = %.2f%%" , vox.m_areaXY);
			Displayf("  areaYZ = %.2f%%" , vox.m_areaYZ);
			Displayf("  areaZX = %.2f%%" , vox.m_areaZX);
			Displayf("  volume = %.2f%%" , vox.m_volume);
			Displayf("  regionCount = %d", vox.m_regionCount);
		}
	}};

	bank.PushGroup("Voxel Analysis", false);
	{
		bank.AddText  ("Path"                   , &s_objPath[0], sizeof(s_objPath), false);
		bank.AddSlider("Max Resolution"         , &s_maxResolution, 2, 256, 1);
		bank.AddSlider("Max Cells/Metre"        , &s_maxCellsPerMetre, 0.1f, 100.0f, 0.01f);
		bank.AddToggle("Project Along X"        , &s_projectFlags, CVoxelAnalysis::PROJECT_ALONG_X);
		bank.AddToggle("Project Along Y"        , &s_projectFlags, CVoxelAnalysis::PROJECT_ALONG_Y);
		bank.AddToggle("Project Along Z"        , &s_projectFlags, CVoxelAnalysis::PROJECT_ALONG_Z);
		bank.AddToggle("Project Flat Base"      , &s_projectFlags, CVoxelAnalysis::PROJECT_FLAT_BASE);
		bank.AddToggle("Find Regions"           , &s_findRegions);
		bank.AddToggle("Make Hollow"            , &s_makeHollow);
		bank.AddSlider("Grid Cylinder Radius"   , &s_gridRadius, 0.01f, 1.0f, 1.0f/64.0f);
		bank.AddSlider("Grid Cylinder Num Sides", &s_gridNumSides, 3, 16, 1);
		bank.AddSlider("Grid Cylinder Scale"    , &s_gridScale, 0.0f, 1.0f, 1.0f/64.0f);
		bank.AddButton("Generate"               , GenerateVoxelAnalysisForSelectedEntities_button::func);
	}
	bank.PopGroup();
}

#endif // __BANK
