// ==========================
// debug/DebugVoxelAnalysis.h
// (c) 2013 RockstarNorth
// ==========================

#ifndef _DEBUG_DEBUGVOXELANALYSIS_H_
#define _DEBUG_DEBUGVOXELANALYSIS_H_

#if __BANK

#include "atl/array.h"
#include "vectormath/classes.h"

namespace rage { class bkBank; }
namespace rage { class fiStream; }
namespace rage { class rmcDrawable; }

class CDumpGeometryToOBJ;

class CVoxelAnalysis
{
private:
	void Init(atArray<const rmcDrawable*>& drawables, atArray<Mat34V>& matrices, int maxResolution, float maxCellsPerMetre, u32 flags);

public:
	enum
	{
		PROJECT_ALONG_X   = BIT(0),
		PROJECT_ALONG_Y   = BIT(1),
		PROJECT_ALONG_Z   = BIT(2),
		PROJECT_FLAT_BASE = BIT(3),
	};

	CVoxelAnalysis(atArray<const rmcDrawable*>& drawables, atArray<Mat34V>& matrices, int maxResolution, float maxCellsPerMetre, u32 flags);
	CVoxelAnalysis(const rmcDrawable* pDrawable, int maxResolution, float maxCellsPerMetre, u32 flags);
	~CVoxelAnalysis();

	void FindRegions();
	void MakeHollow();

	void DumpGridGeometryToOBJ(CDumpGeometryToOBJ& dump, float radius, int numSides, float scale, int regionIndex = -1) const;

	static void AddWidgets(bkBank& bank);

	Vec3V m_boundsMin;
	Vec3V m_boundsMax;
	int   m_resX;
	int   m_resY;
	int   m_resZ;
	u8*   m_grid;
	float m_areaXY;
	float m_areaYZ;
	float m_areaZX;
	float m_volume;
	u8    m_regionCount;
	u8*   m_regionGrid;
};

class CVoxelAnalysisReport
{
public:
	CVoxelAnalysisReport();

	void Open(const char* reportPath, const char* objDir = NULL);
	void Close();
	void AddDrawable(const rmcDrawable* pDrawable, const char* path);

	fiStream* m_report;
	int       m_maxResolution;
	float     m_maxCellsPerMetre;
	bool      m_flatBase;

	atString  m_objDir;
	bool      m_makeHollow;
	float     m_gridRadius;
	int       m_gridNumSides;
	float     m_gridScale;
};

class CDebugVoxelAnalysisInterface
{
public:
	static void AddWidgets(bkBank& bank);
};

#endif // __BANK
#endif // _DEBUG_DEBUGVOXELANALYSIS_H_
